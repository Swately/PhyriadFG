// PhyriadFG — src/flow/holons.cpp : the holon leaves' bodies, moved by anchor from run_flow (R5 step 3a; see holons.hpp).
// The text below is the lambdas' bodies VERBATIM (the pre-extraction flow.cpp is the reference; the verbatim measure
// is in records/R5_GATE.md §3a). Nothing here was retyped. Made with my soul - Swately <3
#include "flow/holons.hpp"
#include "flow/flow.hpp"          // kObj* / kPriorDecay constants
#include "cli/cli.hpp"            // Config (cfg.matte_thresh, cfg.shapefield, cfg.obj_fill_rim, cfg.expire, ...)
#include "core/vk_util.hpp"       // half_to_float / float_to_half
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <climits>
#include <vector>

namespace pfg::flow {

// ── object_repair — moved by anchor from run_flow (flow.cpp:885–1245 pre-extraction); the body is verbatim, the captured
//    scratch is bound below by reference, the captured cfg / grid size are parameters. ──
void object_repair(pfg::flow::HolonScratch& S, const Config& cfg, uint32_t mvw_f, uint32_t mvh_f, void* mv_field, uint8_t* dis_mask, const float model6[6], std::vector<ObjSlot>& slots, const uint8_t* persist_p, uint8_t* persist_mut, uint64_t span_local, double adv_sign, uint32_t* out_live, uint32_t* out_rep, uint32_t* out_infill, WakeRec* wake_out, int* wake_n_out) {
    auto& obj_label = S.obj_label;
    auto& obj_bfs = S.obj_bfs;
    auto& obj_clusters = S.obj_clusters;
    auto& obj_used = S.obj_used;
    auto& obj_rowmin = S.obj_rowmin;
    auto& obj_rowmax = S.obj_rowmax;
    auto& obj_chamf = S.obj_chamf;
    auto& obj_feat = S.obj_feat;
                const uint32_t MW=mvw_f, MH=mvh_f;
                const float thr_byte=cfg.matte_thresh*255.0f;   // the EXACT shader OBJECT cutoff (mirror of matte_mass_count)
                uint16_t* hmv=(uint16_t*)mv_field;              // interleaved R(mv_x),G(mv_y) per block
                // (1) connected components — 4-conn BFS over OBJECT blocks (dis>thr_byte). No heap in the
                // loop: obj_label / obj_bfs / obj_clusters are the fixed scratch. Reset labels to -1 each pair.
                std::fill(obj_label.begin(),obj_label.end(),-1);
                obj_clusters.clear();
                for(uint32_t gy=0; gy<MH; ++gy){
                    for(uint32_t gx=0; gx<MW; ++gx){
                        const size_t i=(size_t)gy*MW+gx;
                        if(obj_label[i]!=-1) continue;                       // already labelled
                        if((float)dis_mask[i] <= thr_byte) continue;        // not an OBJECT block
                        // new component — BFS flood, accumulating mass / centroid / bbox / member-MV mean.
                        const int32_t cid=(int32_t)obj_clusters.size();
                        ObjCluster cl{0,0,0,0,0,(int)gx,(int)gx,(int)gy,(int)gy,0,0};
                        size_t head=0,tail=0;
                        obj_bfs[tail++]=(uint32_t)i; obj_label[i]=cid;
                        while(head<tail){
                            const uint32_t bi=obj_bfs[head++];
                            const uint32_t by=bi/MW, bx=bi%MW;
                            const float mvx=half_to_float(hmv[(size_t)bi*2u+0u]);
                            const float mvy=half_to_float(hmv[(size_t)bi*2u+1u]);
                            ++cl.mass; cl.sx+=bx; cl.sy+=by; cl.mvx+=mvx; cl.mvy+=mvy;
                            if((int)bx<cl.minx)cl.minx=(int)bx; if((int)bx>cl.maxx)cl.maxx=(int)bx;
                            if((int)by<cl.miny)cl.miny=(int)by; if((int)by>cl.maxy)cl.maxy=(int)by;
                            // 4-connected neighbours
                            if(bx>0){      const size_t n=bi-1;  if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                            if(bx+1<MW){   const size_t n=bi+1;  if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                            if(by>0){      const size_t n=bi-MW; if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                            if(by+1<MH){   const size_t n=bi+MW; if(obj_label[n]==-1 && (float)dis_mask[n]>thr_byte){ obj_label[n]=cid; obj_bfs[tail++]=(uint32_t)n; } }
                        }
                        if(cl.mass>0){ cl.cx=cl.sx/(double)cl.mass; cl.cy=cl.sy/(double)cl.mass; cl.mvx/=(double)cl.mass; cl.mvy/=(double)cl.mass; }
                        obj_clusters.push_back(cl);
                    }
                }
                // (1b) keep the top-K=kObjSlots clusters by mass with mass ≥ kObjMinMass. We don't sort the
                // whole vector (could be large) — we select the K heaviest indices by repeated max-scan into
                // a fixed-size index array (K is 16; K passes over N clusters, cheap vs the BFS).
                int keep_idx[kObjSlots]; int keep_n=0;
                {
                    // mark-and-pick into the fixed obj_used scratch (no heap in the loop). For K≤16 and the
                    // typically small cluster count, K linear scans are negligible vs the BFS.
                    if(!obj_clusters.empty()) std::fill(obj_used.begin(),obj_used.begin()+(std::ptrdiff_t)obj_clusters.size(),(uint8_t)0);
                    for(int k=0;k<kObjSlots;++k){
                        int best=-1; uint32_t bestmass=0;
                        for(size_t c=0;c<obj_clusters.size();++c){
                            if(obj_used[c]) continue;
                            if(obj_clusters[c].mass<(uint32_t)kObjMinMass) continue;
                            if(obj_clusters[c].mass>bestmass){ bestmass=obj_clusters[c].mass; best=(int)c; }
                        }
                        if(best<0) break;
                        obj_used[best]=1; keep_idx[keep_n++]=best;
                    }
                }
                // (2) temporal identity — advect each slot's centroid by its mv over this pair's span
                // (prev centroid + prev mv·span/8.0 in block units; mv is px, the grid is 8px/block, so
                // px/8 = block units), then match each kept cluster to the nearest unmatched slot within
                // kObjMatchRadius. Unmatched cluster → new slot (evict the oldest-missed). Unmatched slot
                // → miss++, retire after kObjMaxMiss. EMA the matched slot's mv (kObjMvEma) for stability.
                const double adv=adv_sign*(double)span_local/8.0;   // block-units advection per px of mv (signed per anchor)
                double pred_cx[kObjSlots], pred_cy[kObjSlots]; int slot_taken[kObjSlots];
                for(int s2=0;s2<kObjSlots;++s2){
                    slot_taken[s2]=0;
                    pred_cx[s2]=slots[s2].cx + slots[s2].mvx*adv;
                    pred_cy[s2]=slots[s2].cy + slots[s2].mvy*adv;
                }
                int cl_slot[kObjSlots];               // matched slot per kept cluster (-1 = none)
                for(int k=0;k<keep_n;++k){
                    const ObjCluster& cl=obj_clusters[keep_idx[k]];
                    int best=-1; double bestd=kObjMatchRadius*kObjMatchRadius;
                    for(int s2=0;s2<kObjSlots;++s2){
                        if(!slots[s2].active || slot_taken[s2]) continue;
                        const double dx=cl.cx-pred_cx[s2], dy=cl.cy-pred_cy[s2];
                        const double d2=dx*dx+dy*dy;
                        if(d2<bestd){ bestd=d2; best=s2; }
                    }
                    cl_slot[k]=best;
                    if(best>=0) slot_taken[best]=1;
                }
                // apply matches / spawns. cl_slot[k] is UPDATED to the owner slot for every kept cluster
                // (matched or freshly spawned) so the repair loop below reads it directly — no float-
                // equality refind.
                for(int k=0;k<keep_n;++k){
                    const ObjCluster& cl=obj_clusters[keep_idx[k]];
                    int s2=cl_slot[k];
                    if(s2<0){
                        // spawn — find a free slot, else evict the active slot with the highest miss count.
                        s2=-1; for(int t=0;t<kObjSlots;++t){ if(!slots[t].active){ s2=t; break; } }
                        if(s2<0){ int worst=0; for(int t=1;t<kObjSlots;++t) if(slots[t].miss>slots[worst].miss) worst=t; s2=worst; }
                        slots[s2].active=1; slots[s2].cx=cl.cx; slots[s2].cy=cl.cy;
                        slots[s2].mvx=cl.mvx; slots[s2].mvy=cl.mvy; slots[s2].age=0; slots[s2].miss=0;
                        slots[s2].mass=cl.mass; slot_taken[s2]=1;
                        cl_slot[k]=s2;          // record the spawned owner
                    } else {
                        slots[s2].cx=cl.cx; slots[s2].cy=cl.cy;
                        // the slot MV memory EXPIRES on CONTRADICTION instead of decaying through it (the plain
                        // EMA inherits a STALE direction for ~1-2 pairs after a bounce). Innovation gate: a
                        // direction REVERSAL (negative dot) or a large delta (> kObjMvExpirePx) means the old
                        // evidence is INVALID, not noisy → adopt the fresh measurement outright. Otherwise the
                        // EMA keeps smoothing measurement noise. The rule: EMA for noise, expiration for
                        // contradiction. --no-expire reverts to the pure EMA.
                        const double exp_dot = slots[s2].mvx*cl.mvx + slots[s2].mvy*cl.mvy;
                        const double exp_dx  = cl.mvx-slots[s2].mvx, exp_dy = cl.mvy-slots[s2].mvy;
                        const bool contradicted = (exp_dot < 0.0)
                            || (exp_dx*exp_dx+exp_dy*exp_dy > (double)kObjMvExpirePx*(double)kObjMvExpirePx);
                        if(cfg.expire && contradicted){ slots[s2].mvx=cl.mvx; slots[s2].mvy=cl.mvy; }
                        else { slots[s2].mvx=slots[s2].mvx*(1.0-kObjMvEma)+cl.mvx*kObjMvEma;
                               slots[s2].mvy=slots[s2].mvy*(1.0-kObjMvEma)+cl.mvy*kObjMvEma; }
                        slots[s2].mass=cl.mass; slots[s2].miss=0; ++slots[s2].age;
                    }
                }
                // unmatched slots → miss++, retire after kObjMaxMiss
                uint32_t live=0;
                for(int s2=0;s2<kObjSlots;++s2){
                    if(!slots[s2].active) continue;
                    if(!slot_taken[s2]){ if(++slots[s2].miss>=kObjMaxMiss){ slots[s2].active=0; continue; } }
                    ++live;
                }
                // (3) the INHERITANCE repair — per kept cluster whose MATCHED slot moves ≥ kObjInhMin (rigid
                // path) OR whose rim MV field is alive (shape-field path), scanline-fill the silhouette (per-row
                // min..max member column within the bbox) and overwrite near-static interior blocks. The repair
                // uses the SLOT mv (the EMA-smoothed, temporally-stable object velocity), not the raw cluster
                // mean. Only blocks BELONGING to this cluster's label form the silhouette (the scanline runs over
                // label==cid blocks per row, the vessel filled row-wise).
                // cfg.shapefield (the default) replaces the rigid single-MV write with the contour
                // distance+feature transform + rim-sector inheritance; --no-shapefield reverts to the rigid
                // single-MV path (the else-arm below).
                const bool shapefield_on=cfg.shapefield;
                uint32_t infill_total=0, rep_total=0;
                int wake_nrec=0;   // count of armed clusters recorded into wake_out this pass (bwd only)
                for(int k=0;k<keep_n;++k){
                    const int owner=cl_slot[k];   // owner slot recorded in the apply loop (match or spawn)
                    if(owner<0) continue;
                    const double omvx=slots[owner].mvx, omvy=slots[owner].mvy;
                    const double omv=std::sqrt(omvx*omvx+omvy*omvy);
                    const int32_t cid=keep_idx[k];
                    const ObjCluster& cl=obj_clusters[keep_idx[k]];
                  if(!shapefield_on){
                    // ── RIGID PATH (the --no-shapefield else-arm) ──
                    if(omv<(double)kObjInhMin) continue;                 // object not moving → no inheritance
                    // armed (rigid arm = omv≥kObjInhMin here) → record for the wake evaporation.
                    if(wake_out && wake_nrec<kObjSlots){ wake_out[wake_nrec++]=WakeRec{cid,cl.minx,cl.maxx,cl.miny,cl.maxy,omvx,omvy}; }
                    // scanline fill bounds over the bbox rows: min/max member COLUMN (label==cid) per row.
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){ obj_rowmin[ry]=INT32_MAX; obj_rowmax[ry]=INT32_MIN; }
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        const size_t base=(size_t)ry*MW;
                        for(int rx=cl.minx; rx<=cl.maxx; ++rx){
                            if(obj_label[base+rx]==cid){ if(rx<obj_rowmin[ry])obj_rowmin[ry]=rx; if(rx>obj_rowmax[ry])obj_rowmax[ry]=rx; }
                        }
                    }
                    const uint16_t mvx_h=float_to_half((float)omvx), mvy_h=float_to_half((float)omvy);
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;       // no member on this row
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            ++infill_total;                               // a silhouette-interior (filled) block
                            // MEMBERSHIP BEATS INERTIA — inside this ARMED silhouette the block is a mover's
                            // interior, not a HUD (the flat-interior illusion); reset its persistence so the
                            // shield below never blocks it and the shader's inertia gates release.
                            if(persist_mut) persist_mut[bi]=0;
                            // HUD shield: a long-static-history block is exempt — UNLESS just reset above
                            // (armed membership wins; the shield still guards the bwd call + --no-persist-reset).
                            if(persist_p && persist_p[bi]>=(uint8_t)kObjPersistShield) continue;
                            const float bmvx=half_to_float(hmv[bi*2u+0u]);
                            const float bmvy=half_to_float(hmv[bi*2u+1u]);
                            // --obj-fill-rim (rigid arm): OFF -> the near-static gate (leave non-static blocks).
                            // ON -> the whole rigid silhouette inherits the slot MV (this arm writes mvx_h/mvy_h
                            // everywhere it does not continue).
                            if(!cfg.obj_fill_rim && std::sqrt(bmvx*bmvx+bmvy*bmvy) > (double)kObjStaticMax) continue;  // not near-static → leave it
                            // the cancellation signature: static interior inside a moving silhouette → inherit.
                            hmv[bi*2u+0u]=mvx_h; hmv[bi*2u+1u]=mvy_h;
                            // (4) consistency re-walk: recompute THIS block's dissidence byte against the SAME
                            // affine model (r = |mv_rep − model(gx,gy)|, byte = min(255, round(16·r))) so the
                            // mask the matte/mass-count read stays consistent with the field we just wrote.
                            const double gx=(double)rx, gy=(double)ry;
                            const double rxr=omvx-((double)model6[0]+model6[1]*gx+model6[2]*gy);
                            const double ryr=omvy-((double)model6[3]+model6[4]*gx+model6[5]*gy);
                            const double r=std::sqrt(rxr*rxr+ryr*ryr);
                            int q=(int)(16.0*r+0.5); if(q>255) q=255;
                            dis_mask[bi]=(uint8_t)q;
                            ++rep_total;
                        }
                    }
                    continue;
                  }
                    // ── SHAPE-FIELD PATH (default) ────────────────────────────────────────────────────────
                    // (3a) scanline fill bounds over the bbox rows: min/max member COLUMN (label==cid) per row.
                    // This defines the FILLED silhouette (the chamfer domain) exactly as the rigid path fills.
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){ obj_rowmin[ry]=INT32_MAX; obj_rowmax[ry]=INT32_MIN; }
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        const size_t base=(size_t)ry*MW;
                        for(int rx=cl.minx; rx<=cl.maxx; ++rx){
                            if(obj_label[base+rx]==cid){ if(rx<obj_rowmin[ry])obj_rowmin[ry]=rx; if(rx>obj_rowmax[ry])obj_rowmax[ry]=rx; }
                        }
                    }
                    // in-silhouette test (a block is in the filled vessel iff its column ∈ its row's fill bounds).
                    auto in_sil=[&](int rx,int ry)->bool{
                        if(ry<cl.miny||ry>cl.maxy) return false;
                        if(obj_rowmin[ry]>obj_rowmax[ry]) return false;
                        return rx>=obj_rowmin[ry] && rx<=obj_rowmax[ry];
                    };
                    // (3b) rim extraction + rim MV SPREAD. A member (label==cid) block is RIM iff any 4-neighbour
                    // is non-member (label!=cid, OR off-grid → the grid edge counts as non-member). Initialize the
                    // chamfer/feature scratch over the bbox window: rim → dist 0, feat=self; other silhouette →
                    // INT32_MAX. We also SUBSAMPLE up to kObjRimSpreadSamples rim blocks (stride) to measure the
                    // rim MV spread = max pairwise |mv_rim_i − mv_rim_j| (bounded O(32²)) for the generalized arm.
                    float rim_mx[kObjRimSpreadSamples], rim_my[kObjRimSpreadSamples]; int rim_ns=0;
                    uint32_t rim_count=0;
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            obj_chamf[bi]=INT32_MAX;                       // default: silhouette but not yet reached
                            if(obj_label[bi]!=cid) continue;              // only label-member blocks can be rim
                            const bool wEdge=(rx==0)             || obj_label[bi-1]!=cid;
                            const bool eEdge=((uint32_t)rx+1>=MW) || obj_label[bi+1]!=cid;
                            const bool nEdge=(ry==0)             || obj_label[bi-MW]!=cid;
                            const bool sEdge=((uint32_t)ry+1>=MH) || obj_label[bi+MW]!=cid;
                            if(wEdge||eEdge||nEdge||sEdge){
                                obj_chamf[bi]=0; obj_feat[bi]=(uint32_t)bi;   // rim: distance 0, feature = self
                                ++rim_count;
                            }
                        }
                    }
                    // subsample the rim MVs for the spread scan (deterministic stride over the bbox-scan order).
                    {
                        const uint32_t stride = rim_count>(uint32_t)kObjRimSpreadSamples
                                              ? (rim_count/(uint32_t)kObjRimSpreadSamples) : 1u;
                        uint32_t seen=0;
                        for(int ry=cl.miny; ry<=cl.maxy && rim_ns<kObjRimSpreadSamples; ++ry){
                            if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                            const size_t base=(size_t)ry*MW;
                            for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry] && rim_ns<kObjRimSpreadSamples; ++rx){
                                const size_t bi=base+rx;
                                if(obj_chamf[bi]!=0) continue;            // not a rim block
                                if((seen++ % stride)!=0u) continue;       // stride subsample
                                rim_mx[rim_ns]=half_to_float(hmv[bi*2u+0u]);
                                rim_my[rim_ns]=half_to_float(hmv[bi*2u+1u]);
                                ++rim_ns;
                            }
                        }
                    }
                    double rim_spread=0.0;
                    for(int a=0;a<rim_ns;++a) for(int b=a+1;b<rim_ns;++b){
                        const double dx=(double)rim_mx[a]-rim_mx[b], dy=(double)rim_my[a]-rim_my[b];
                        const double d=std::sqrt(dx*dx+dy*dy); if(d>rim_spread) rim_spread=d;
                    }
                    // (3c) GENERALIZED ARMING: a rigid mean ≥ kObjInhMin OR a live rim field (spread ≥ min) — a
                    // scaling/rotating object can have mean mv ≈ 0 yet a live rim.
                    const bool armed = (omv>=(double)kObjInhMin) || (rim_spread>=(double)kObjRimSpreadMin);
                    if(!armed) continue;
                    // --obj-fill-rim: a RIGID cluster (rim field already AGREES: rim_spread<min) stamps its
                    // single slot MV across the WHOLE silhouette (incl. the protected rim band + the >static
                    // annulus) so the disc warps as one surface. Off -> zero work. Stands down on a
                    // scaling/rotating/articulated rim (the graded chamfer blend rules there).
                    const bool rigid_fill = cfg.obj_fill_rim && (rim_spread < (double)kObjRimSpreadMin);
                    // armed → record for the wake evaporation (the trailing sweep in mem_refresh uses the slot
                    // mv; a rim-only-armed cluster with omv≈0 yields a ~1-block sweep — harmless).
                    if(wake_out && wake_nrec<kObjSlots){ wake_out[wake_nrec++]=WakeRec{cid,cl.minx,cl.maxx,cl.miny,cl.maxy,omvx,omvy}; }
                    // (3d) CHAMFER distance + FEATURE transform (2-pass, bbox-bounded) over the filled silhouette.
                    // Integer chamfer weights 3 (orthogonal) / 4 (diagonal). Forward pass TL→BR relaxes from the
                    // already-swept W/N/NW/NE neighbours; backward pass BR→TL from E/S/SE/SW. When a neighbour
                    // relaxes the distance we COPY its nearest-rim feature (the feature transform). Only
                    // in-silhouette neighbours with a finite distance contribute.
                    auto relax=[&](size_t bi,int nrx,int nry,int w){
                        if(!in_sil(nrx,nry)) return;
                        const size_t ni=(size_t)nry*MW+nrx;
                        const int32_t nd=obj_chamf[ni];
                        if(nd==INT32_MAX) return;
                        const int32_t cand=nd+w;
                        if(cand<obj_chamf[bi]){ obj_chamf[bi]=cand; obj_feat[bi]=obj_feat[ni]; }
                    };
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){               // forward pass TL→BR
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            if(obj_chamf[bi]==0) continue;                // rim seed — never relaxed
                            relax(bi,rx-1,ry,  kChamfOrtho);             // W
                            relax(bi,rx,  ry-1,kChamfOrtho);             // N
                            relax(bi,rx-1,ry-1,kChamfDiag);              // NW
                            relax(bi,rx+1,ry-1,kChamfDiag);              // NE
                        }
                    }
                    for(int ry=cl.maxy; ry>=cl.miny; --ry){              // backward pass BR→TL
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmax[ry]; rx>=obj_rowmin[ry]; --rx){
                            const size_t bi=base+rx;
                            if(obj_chamf[bi]==0) continue;                // rim seed — never relaxed
                            relax(bi,rx+1,ry,  kChamfOrtho);             // E
                            relax(bi,rx,  ry+1,kChamfOrtho);             // S
                            relax(bi,rx+1,ry+1,kChamfDiag);              // SE
                            relax(bi,rx-1,ry+1,kChamfDiag);              // SW
                        }
                    }
                    // (3e) RIM-SECTOR INHERITANCE fill with the DEPTH GATE. Only strictly-interior blocks
                    // (chamfer dist ≥ kObjShapeDepthMin) are eligible — the rim band itself is NEVER rewritten
                    // (the stable contour is the instrument). The inherited MV is
                    // mix(nearest-rim MV, slot MV, w), w = clamp(dist/kChamfWMax, 0, 1): near the contour the
                    // nearest rim sector wins (rotation/scaling correct), deep interior relaxes to the slot mean.
                    for(int ry=cl.miny; ry<=cl.maxy; ++ry){
                        if(obj_rowmin[ry]>obj_rowmax[ry]) continue;       // no member on this row
                        const size_t base=(size_t)ry*MW;
                        for(int rx=obj_rowmin[ry]; rx<=obj_rowmax[ry]; ++rx){
                            const size_t bi=base+rx;
                            ++infill_total;                               // a silhouette-interior (filled) block
                            const int32_t d=obj_chamf[bi];
                            // --obj-fill-rim: under rigid_fill the rim band + unreached blocks DO inherit the
                            // slot MV (they are part of the one rigid surface). Off -> the gates apply.
                            if(!rigid_fill && d==INT32_MAX) continue;     // unreached silhouette block — cannot inherit
                            if(!rigid_fill && d<kObjShapeDepthMin) continue; // DEPTH GATE: rim band protected, never rewritten
                            // MEMBERSHIP BEATS INERTIA — reset the mover-interior persistence (see the rigid
                            // arm + the lambda doc; the flat-interior illusion is not a HUD).
                            if(persist_mut) persist_mut[bi]=0;
                            // HUD shield: exempt — UNLESS just reset above (armed membership wins).
                            if(persist_p && persist_p[bi]>=(uint8_t)kObjPersistShield) continue;
                            const float bmvx=half_to_float(hmv[bi*2u+0u]);
                            const float bmvy=half_to_float(hmv[bi*2u+1u]);
                            // --obj-fill-rim: under rigid_fill a >static "spurious" block (aperture-problem
                            // flow) is REWRITTEN to the slot MV instead of left as garbage. Off -> the gate.
                            if(!rigid_fill && std::sqrt(bmvx*bmvx+bmvy*bmvy) > (double)kObjStaticMax) continue;  // not near-static → leave it
                            // rim-sector inheritance: mix(nearest-rim MV, slot MV, w). The nearest-rim MV is the
                            // MEASURED rim instrument read straight from the field (feature transform payload).
                            double imvx, imvy;
                            if(rigid_fill){ imvx=omvx; imvy=omvy; }       // stamp the SINGLE slot MV (no obj_feat
                                                                          // read -> safe for unreached/rim blocks
                                                                          // AND corrects a wrong rim block).
                            else {                                        // original graded rim-sector inheritance:
                                const uint32_t fb=obj_feat[bi];
                                const double rmvx=half_to_float(hmv[(size_t)fb*2u+0u]);
                                const double rmvy=half_to_float(hmv[(size_t)fb*2u+1u]);
                                double w=(double)d/(double)kChamfWMax; if(w<0.0)w=0.0; else if(w>1.0)w=1.0;
                                imvx=rmvx*(1.0-w)+omvx*w;                 // near rim → rim sector; deep → slot mean
                                imvy=rmvy*(1.0-w)+omvy*w;
                            }
                            hmv[bi*2u+0u]=float_to_half((float)imvx); hmv[bi*2u+1u]=float_to_half((float)imvy);
                            // (4) consistency re-walk: recompute THIS block's dissidence byte against the SAME
                            // affine model with the INHERITED (per-block) MV so mask+field ship consistent.
                            const double gx=(double)rx, gy=(double)ry;
                            const double rxr=imvx-((double)model6[0]+model6[1]*gx+model6[2]*gy);
                            const double ryr=imvy-((double)model6[3]+model6[4]*gx+model6[5]*gy);
                            const double r=std::sqrt(rxr*rxr+ryr*ryr);
                            int q=(int)(16.0*r+0.5); if(q>255) q=255;
                            dis_mask[bi]=(uint8_t)q;
                            ++rep_total;
                        }
                    }
                }
                if(out_live)   *out_live=live;
                if(out_rep)    *out_rep=rep_total;
                if(out_infill) *out_infill=infill_total;
                if(wake_n_out) *wake_n_out=wake_nrec;   // armed-cluster count (bwd pass only; 0 when wake_out null)
}

// ── mem_advect — moved by anchor from run_flow (flow.cpp:1259–1274 pre-extraction); the body is verbatim, the captured
//    scratch is bound below by reference, the captured cfg / grid size are parameters. ──
void mem_advect(pfg::flow::HolonScratch& S, uint32_t mvw_f, uint32_t mvh_f, const void* fwd_field) {
    auto& mem_prior = S.mem_prior;
    auto& mem_adv = S.mem_adv;
                const uint32_t MW=mvw_f, MH=mvh_f;
                const uint16_t* hmv=(const uint16_t*)fwd_field;   // fwd field: R(mv_x),G(mv_y) px, prev→cur
                for(uint32_t by=0; by<MH; ++by){
                    for(uint32_t bx=0; bx<MW; ++bx){
                        const size_t bi=(size_t)by*MW+bx;
                        const float mvx=half_to_float(hmv[bi*2u+0u]);
                        const float mvy=half_to_float(hmv[bi*2u+1u]);
                        // src block = where THIS cur block came FROM in prev space (gather, dst-MV).
                        const long sx=std::lround((double)bx-(double)mvx/8.0);
                        const long sy=std::lround((double)by-(double)mvy/8.0);
                        if(sx<0||sy<0||(uint32_t)sx>=MW||(uint32_t)sy>=MH){ mem_adv[bi]=0; continue; }
                        mem_adv[bi]=mem_prior[(size_t)sy*MW+(size_t)sx];   // unmapped src defaults to its 0 byte
                    }
                }
}

// ── mem_merge — moved by anchor from run_flow (flow.cpp:1290–1310 pre-extraction); the body is verbatim, the captured
//    scratch is bound below by reference, the captured cfg / grid size are parameters. ──
void mem_merge(const Config& cfg, uint32_t mvw_f, uint32_t mvh_f, uint8_t* dis_mask, const void* anchor_field, const uint8_t* prior_src) {
                const uint32_t MW=mvw_f, MH=mvh_f;
                const uint16_t* hmv=(const uint16_t*)anchor_field;
                const float thr_byte=cfg.matte_thresh*255.0f;       // the EXACT cluster/shader OBJECT cutoff
                const size_t nblk=(size_t)MW*(size_t)MH;
                for(size_t bi=0; bi<nblk; ++bi){
                    uint32_t p=(uint32_t)prior_src[bi];
                    if(p==0u) continue;                              // no belief here → mask unchanged (fast path)
                    const float f=(float)dis_mask[bi];
                    const bool fresh_low = (f<=thr_byte);            // fresh evidence does NOT call this OBJECT
                    if(fresh_low){
                        const float mvx=half_to_float(hmv[bi*2u+0u]);
                        const float mvy=half_to_float(hmv[bi*2u+1u]);
                        // confident BACKGROUND (moves with the model) contradicts the memory → evaporate now.
                        if(std::sqrt((double)mvx*mvx+(double)mvy*mvy) > (double)kObjStaticMax){ continue; }
                        // else: fresh low + near-static = cancellation-compatible → keep the prior, decayed.
                    }
                    uint32_t pd=(uint32_t)((double)p*kPriorDecay);   // unconditional decay floor
                    if((float)pd>f) dis_mask[bi]=(uint8_t)pd;        // max(fresh, decayed prior)
                }
}

// ── mem_refresh — moved by anchor from run_flow (flow.cpp:1324–1380 pre-extraction); the body is verbatim, the captured
//    scratch is bound below by reference, the captured cfg / grid size are parameters. ──
void mem_refresh(pfg::flow::HolonScratch& S, uint32_t mvw_f, uint32_t mvh_f, const uint8_t* dis_b_post, bool bwd_ran, const WakeRec* wake_recs, int wake_count) {
    auto& obj_label = S.obj_label;
    auto& obj_rowmin = S.obj_rowmin;
    auto& obj_rowmax = S.obj_rowmax;
    auto& mem_prior = S.mem_prior;
    auto& mem_adv = S.mem_adv;
                const uint32_t MW=mvw_f, MH=mvh_f;
                const size_t nblk=(size_t)MW*(size_t)MH;
                if(bwd_ran && dis_b_post){
                    for(size_t bi=0; bi<nblk; ++bi){
                        const uint32_t pd=(uint32_t)((double)mem_adv[bi]*kPriorDecay);
                        const uint32_t db=(uint32_t)dis_b_post[bi];
                        mem_prior[bi]=(uint8_t)(db>pd?db:pd);        // max(decay(advected prior), confirmed mask)
                    }
                } else {
                    for(size_t bi=0; bi<nblk; ++bi)
                        mem_prior[bi]=(uint8_t)((double)mem_adv[bi]*kPriorDecay);   // pure decay (no confirmation)
                }
                // ── holonic wake evaporation (cur-anchored, AFTER the max above). For each armed bwd cluster:
                // zero mem_prior blocks that are (prior>0) AND NOT inside the cluster's scanline-filled
                // silhouette AND inside the cluster bbox EXPANDED opposite to the slot MV by ceil(|mv|/8)+1
                // blocks (the trailing sweep — where a remembered static wake hides behind a moving object).
                // The fill bounds are RE-DERIVED from obj_label==cid; obj_rowmin/obj_rowmax are free scratch
                // here (the repair finished with them). The sweep is the ONLY region touched, so a remembered
                // silhouette AHEAD of / on the object is untouched.
                if(bwd_ran && wake_recs){
                    for(int w=0; w<wake_count; ++w){
                        const WakeRec& r=wake_recs[w];
                        if(r.cid<0) continue;
                        if(r.miny>r.maxy||r.minx>r.maxx) continue;
                        // re-derive the per-row fill bounds (min/max member column) from obj_label==cid.
                        for(int ry=r.miny; ry<=r.maxy; ++ry){ obj_rowmin[ry]=INT32_MAX; obj_rowmax[ry]=INT32_MIN; }
                        for(int ry=r.miny; ry<=r.maxy; ++ry){
                            const size_t base=(size_t)ry*MW;
                            for(int rx=r.minx; rx<=r.maxx; ++rx){
                                if(obj_label[base+rx]==r.cid){ if(rx<obj_rowmin[ry])obj_rowmin[ry]=rx; if(rx>obj_rowmax[ry])obj_rowmax[ry]=rx; }
                            }
                        }
                        // trailing-sweep bbox: expand OPPOSITE to the slot MV by e=ceil(|mv|/8)+1 blocks.
                        const double omv=std::sqrt(r.mvx*r.mvx+r.mvy*r.mvy);
                        const int e=(int)std::ceil(omv/8.0)+1;        // sweep depth in block units (px→block /8)
                        int sminx=r.minx, smaxx=r.maxx, sminy=r.miny, smaxy=r.maxy;
                        if(r.mvx>0.0) sminx-=e; else if(r.mvx<0.0) smaxx+=e;   // trailing side = behind the motion
                        if(r.mvy>0.0) sminy-=e; else if(r.mvy<0.0) smaxy+=e;
                        if(sminx<0)sminx=0; if(sminy<0)sminy=0;
                        if(smaxx>(int)MW-1)smaxx=(int)MW-1; if(smaxy>(int)MH-1)smaxy=(int)MH-1;
                        for(int ry=sminy; ry<=smaxy; ++ry){
                            const size_t base=(size_t)ry*MW;
                            const bool row_in_bbox=(ry>=r.miny && ry<=r.maxy);
                            for(int rx=sminx; rx<=smaxx; ++rx){
                                const size_t bi=base+rx;
                                if(mem_prior[bi]==0) continue;        // nothing to expire here
                                // inside the filled silhouette? (only within the cluster bbox rows can it be).
                                const bool in_sil = row_in_bbox && obj_rowmin[ry]<=obj_rowmax[ry]
                                                  && rx>=obj_rowmin[ry] && rx<=obj_rowmax[ry];
                                if(!in_sil) mem_prior[bi]=0;          // trailing-wake block outside the body → evaporate
                            }
                        }
                    }
                }
}

}  // namespace pfg::flow
