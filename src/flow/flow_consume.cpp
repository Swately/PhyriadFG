// PhyriadFG — src/flow/flow_consume.cpp : the FLOW stage's per-pair CPU tail, moved by anchor from run_flow
// (R5 step 3c; see flow_consume.hpp). The function body below is the lambda's text VERBATIM — nothing was retyped;
// the reference is the pre-extraction flow.cpp (r5s3/flow_pre_consume.cpp) and the measure is in
// records/R5_GATE.md §3c. Made with my soul - Swately <3
#include "flow/flow_consume.hpp"
#include "flow/flow.hpp"
#include "core/fg_context.hpp"
#include "core/vk_util.hpp"
#include "core/globals.hpp"
#include "control/cli.hpp"
#include "control/layer_config.hpp"
#include "instrument/instrument.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

namespace pfg::flow {

void consume_wap(FgContext& ctx, ConsumeState& S, const FwdPend& pc, bool allow_bwd) {
// ── the captured names, rebound (see the header). The 444 lines below are the lambda body, verbatim. ──
    auto& cfg = ctx.cfg;
    auto& c_slots = ctx.c_slots;
    auto& WW = ctx.WW;
    auto& WH = ctx.WH;
    auto& A = ctx.A;
    auto& FD = ctx.FD;
    auto& mvw_f = ctx.mvw_f;
    auto& mvh_f = ctx.mvh_f;
    auto& flow_div = ctx.flow_div;
    auto& use_inertia = ctx.use_inertia;
    auto& use_memory = ctx.use_memory;
    auto& use_bidir = ctx.use_bidir;
    auto& use_gme = ctx.use_gme;
    auto& use_gme_gpu = ctx.use_gme_gpu;
    auto& use_nvofa = ctx.use_nvofa;
    auto& ofp = ctx.ofp;
    auto& Cinterp = ctx.Cinterp;
    auto& nvofa = ctx.nvofa;
    auto& gmePipe = ctx.gmePipe;
    auto& Bframe = ctx.Bframe;
    auto& cmdB_bwd = ctx.cmdB_bwd;
    auto& fB2 = ctx.fB2;
    auto& hostMV = ctx.hostMV;
    auto& hostSAD = ctx.hostSAD;
    auto& hostPER = ctx.hostPER;
    auto& hostDIS = ctx.hostDIS;
    auto& hostDISB = ctx.hostDISB;
    auto& hostMVB = ctx.hostMVB;
    auto& hostGmeM = ctx.hostGmeM;
    auto& hostGmeMB = ctx.hostGmeMB;
    auto& hMVB_b = ctx.hMVB_b;
    auto& hDISB_b = ctx.hDISB_b;
    auto& hGmeMB_b = ctx.hGmeMB_b;
    auto& f_pair_cseq_a = ctx.f_pair_cseq_a;
    auto& f_pair_slot_a = ctx.f_pair_slot_a;
    auto& f_pair_tcap_a = ctx.f_pair_tcap_a;
    auto& f_pair_span_a = ctx.f_pair_span_a;
    auto& f_pair_n_a = ctx.f_pair_n_a;
    auto& f_pair_gme_a = ctx.f_pair_gme_a;
    auto& f_pair_gme_valid_a = ctx.f_pair_gme_valid_a;
    auto& f_pair_gme_bwd_a = ctx.f_pair_gme_bwd_a;
    auto& f_pair_mfwd_a = ctx.f_pair_mfwd_a;
    auto& f_pair_mbwd_a = ctx.f_pair_mbwd_a;
    auto& f_pair_disp_a = ctx.f_pair_disp_a;
    auto& f_pair_bwd_valid_a = ctx.f_pair_bwd_valid_a;
    auto& f_seq = ctx.f_seq;
    auto& f_cv = ctx.f_cv;
    auto& src_interval_us = ctx.src_interval_us;
    auto& present_cost_us = ctx.present_cost_us;
    auto& stat_tier = ctx.stat_tier;
    auto& stat_bwd_skips = ctx.stat_bwd_skips;
    auto& stat_cons = ctx.stat_cons;
    auto& stat_flow_us = ctx.stat_flow_us;
    auto& stat_pair_us = ctx.stat_pair_us;
    auto& gme_fit_us = ctx.gme_fit_us;
    auto& gme_dis_x100 = ctx.gme_dis_x100;
    auto& obj_live = ctx.obj_live;
    auto& obj_rep_x10 = ctx.obj_rep_x10;
    auto& live_n_atomic = ctx.live_n_atomic;
    auto& lt_fpub_us = ctx.lt_fpub_us;
    auto& hs = S.hs;
    auto& persist = S.persist;
    auto& gme_fit_ema = S.gme_fit_ema;
    auto& gme_fits = S.gme_fits;
    auto& gme_sub2 = S.gme_sub2;
    auto& gme_fit_printed = S.gme_fit_printed;
    auto& gme_vfy_dis = S.gme_vfy_dis;
    auto& gme_vfy_n = S.gme_vfy_n;
    auto& obj_cost_ema = S.obj_cost_ema;
    auto& obj_pairs = S.obj_pairs;
    auto& obj_settle_printed = S.obj_settle_printed;
    auto& t_pair_ema = S.t_pair_ema;
    auto& bwd_skipping = S.bwd_skipping;
    auto& t_flow_ema = S.t_flow_ema;
    auto& t_fuse_ema = S.t_fuse_ema;
    auto& t_warp_ema = S.t_warp_ema;
    auto& span_ema = S.span_ema;
    auto& pressure_tier = S.pressure_tier;
    auto& holon_pair_ctr = S.holon_pair_ctr;
    auto& tier4_dwell = S.tier4_dwell;
    auto& live_n_f = S.live_n_f;
    auto& mv_audit_left = S.mv_audit_left;
    auto& up_streak = S.up_streak;
    auto& deg_streak = S.deg_streak;
    auto& dwell_sets = S.dwell_sets;
    auto& kTier4DwellPairs = S.kTier4DwellPairs;
    auto& objdump_left = S.objdump_left;
    auto& objdump_idx = S.objdump_idx;
    auto& flow_submit_nowait = S.flow_submit_nowait;
    auto& flow_submit_q2_chain = S.flow_submit_q2_chain;
    auto& flow_downsample = S.flow_downsample;
    auto& mv_audit_stat = S.mv_audit_stat;
    auto& objdump_grid = S.objdump_grid;
    // the leaf holons (R5 step 3a, flow/holons.cpp) — the same wrappers run_flow declares, so the calls below are unchanged
    auto object_repair=[&](void* mv_field, uint8_t* dis_mask, const float model6[6],
                           std::vector<pfg::flow::ObjSlot>& slots, const uint8_t* persist_p, uint8_t* persist_mut,
                           uint64_t span_local, double adv_sign,
                           uint32_t* out_live, uint32_t* out_rep, uint32_t* out_infill,
                           pfg::flow::WakeRec* wake_out, int* wake_n_out){
        pfg::flow::object_repair(hs, cfg, mvw_f, mvh_f, mv_field, dis_mask, model6, slots, persist_p, persist_mut, span_local, adv_sign, out_live, out_rep, out_infill, wake_out, wake_n_out);
    };
    auto mem_advect=[&](const void* fwd_field){ pfg::flow::mem_advect(hs, mvw_f, mvh_f, fwd_field); };
    auto mem_merge=[&](uint8_t* dis_mask, const void* anchor_field, const uint8_t* prior_src){ pfg::flow::mem_merge(cfg, mvw_f, mvh_f, dis_mask, anchor_field, prior_src); };
    auto mem_refresh=[&](const uint8_t* dis_b_post, bool bwd_ran, const pfg::flow::WakeRec* wake_recs, int wake_count){ pfg::flow::mem_refresh(hs, mvw_f, mvh_f, dis_b_post, bwd_ran, wake_recs, wake_count); };
    // the scratch names the body reads through their run_flow aliases
    auto& mem_prior = hs.mem_prior; auto& mem_adv = hs.mem_adv; auto& wake_rec = hs.wake_rec; auto& wake_n = hs.wake_n;
    auto& obj_slots_fwd = hs.obj_slots_fwd; auto& obj_slots_bwd = hs.obj_slots_bwd;
    using ObjSlot = pfg::flow::ObjSlot; using WakeRec = pfg::flow::WakeRec;
                const int f_gen=pc.f_gen; const uint64_t cur_c=pc.cur_c; const int s=pc.s;
                const uint64_t span=pc.span; const int N_use=pc.N_use; const int prv_f=pc.prv_f;
                const int cur_f=pc.cur_f; const bool have_prev_f=pc.have_prev_f; const double tp0=pc.tp0;
                const uint32_t mvw=mvw_f, mvh=mvh_f;   // (WW+7)/8 × (WH+7)/8 — the WAP MV grid (constants)
                (void)prv_f;(void)cur_f;   // referenced only by the bwd record (allow_bwd path)
                // tf0 anchors the GPU-wait leg (t_flow) AND the fuse EMA. SERIAL (fwd_fence==NULL): the caller
                // already ran submit_wait and passes pc.tf0 = the pre-submit_wait timestamp, so t_flow =
                // now−pc.tf0 == the submit_wait duration and t_fuse = now−pc.tf0 == submit_wait+CPU.
                // PIPELINED: tf0 is set HERE right before the deferred fence wait, so t_flow = the fence-wait
                // leg of the overlapped pair and t_fuse = wait+CPU. Either way t_flow is the blocking-GPU-leg
                // measure --fsub prints.
                const double tf0 = (pc.fwd_fence!=VK_NULL_HANDLE) ? now_ms() : pc.tf0;
                if(pc.fwd_fence!=VK_NULL_HANDLE) vk_wait_live(FD.dev,pc.fwd_fence);   // the consume-side wait — FD.dev (B.dev null under single_gpu would crash)
                { const double tflow=now_ms()-tf0; t_flow_ema=t_flow_ema>0.0?t_flow_ema*0.8+tflow*0.2:tflow; }
                if(use_inertia && have_prev_f){
                    const uint16_t* hmv =(const uint16_t*)hostMV[f_gen];
                    const uint16_t* hsad=(const uint16_t*)hostSAD[f_gen];
                    const size_t nblk=(size_t)mvw*(size_t)mvh;
                    for(size_t i=0;i<nblk;++i){
                        const float mvx=half_to_float(hmv[i*2u+0u]);
                        const float mvy=half_to_float(hmv[i*2u+1u]);
                        const float szero=half_to_float(hsad[i*2u+1u]);
                        const bool static_now=(std::sqrt(mvx*mvx+mvy*mvy)<=1.0f) && (szero<=8.0f);
                        const int next = static_now ? ((int)persist[i]+16) : 0;
                        persist[i]=(uint8_t)(next>255?255:next);
                    }
                    std::memcpy(hostPER[f_gen], persist.data(), nblk);
                } else if(use_inertia){
                    std::memcpy(hostPER[f_gen], persist.data(), (size_t)mvw*(size_t)mvh);
                }
                const double pair_budget_ms=(double)src_interval_us.load()/1000.0;
                if(t_pair_ema>0.0){
                    if(!bwd_skipping){ if(t_pair_ema>0.95*pair_budget_ms) bwd_skipping=true; }
                    else            { if(t_pair_ema<0.80*pair_budget_ms) bwd_skipping=false; }
                }
                if(cfg.tiers){
                    if(!bwd_skipping){ pressure_tier=0; }
                    else if(t_pair_ema>0.0){
                        if(pressure_tier<2){ if(t_pair_ema>1.10*pair_budget_ms) pressure_tier=2; else pressure_tier=1; }
                        else if(pressure_tier==2){
                            if(t_pair_ema>1.50*pair_budget_ms) pressure_tier=3;
                            else if(t_pair_ema<0.95*pair_budget_ms) pressure_tier=1;
                        } else if(pressure_tier==3){
                            if(t_pair_ema<1.20*pair_budget_ms) pressure_tier=2;
                        }
                        // (pressure_tier==4 is held/released by the deficit override below.)
                        //
                        // The deficit shed-tier is a budget-relative OVERRIDE, not a rung climbed through the
                        // sub-ladder. ENGAGE when the pair sustains over budget (the cons<arr deficit, >1.10×);
                        // HOLD until the SHED pair (~0.8×budget) drops below 0.65×budget — i.e. only when the
                        // source eases enough that full quality refits. The [0.65,1.10]×budget dead-band
                        // brackets the shed pair, so tier-4 STICKS instead of flapping. Gated on --deficit-tier
                        // (default OFF) → tier 4 stays unreachable when the flag is off.
                        if(cfg.deficit_tier){
                            // PROACTIVE + DWELLED shed. ENGAGE EARLY (>0.92×budget — shed before the deficit
                            // bites, leaving jitter headroom) and HOLD with a dwell (≥kTier4DwellPairs after
                            // engaging) so a momentary calm cannot re-trigger the flap; RELEASE only when truly
                            // eased (<0.55×budget AND dwell expired). --deficit-tier-gated (default OFF).
                            if(pressure_tier<4){ if(t_pair_ema>0.92*pair_budget_ms){ pressure_tier=4; tier4_dwell=kTier4DwellPairs; } }
                            else { if(tier4_dwell>0) --tier4_dwell;
                                   else if(t_pair_ema<0.55*pair_budget_ms) pressure_tier=3; }
                        }
                        // The util-driven tier FLOOR is computed+published by the PRESENT thread (g_gov_floor)
                        // and applied BELOW, AFTER this whole if(cfg.tiers) block — DECOUPLED from
                        // bwd_skipping. One decode owner: P.
                    } else { pressure_tier=1; }
                } else { pressure_tier = bwd_skipping?1:0; }
                // apply the PRESENT-published governor floor as a MAX over the CPU ladder, DECOUPLED from
                // bwd_skipping. g_gov_floor is 0 unless --load-governor + a hot 4090. Gated on cfg.tiers to
                // preserve --no-tiers semantics.
                if(cfg.tiers && cfg.load_governor){ const int gf=g_gov_floor.load(); if(gf>pressure_tier) pressure_tier=gf; }
                ++holon_pair_ctr;
                // tier-4 sheds the holon REFINEMENT (object_repair + scene-memory) on EVERY pair, not
                // every-Nth — the deficit recovery. gme-fit + matte + warp + inertia still run (the raw-flow
                // WAP). Only reachable under --deficit-tier.
                const bool holon_skip_pair = pfg::layers::holon_skip_for(pressure_tier, cfg.tiers, holon_pair_ctr);   // R7: == shed_holon || (cfg.tiers && holon_period>1 && ctr%period!=0), verbatim
                stat_tier.store((uint64_t)pressure_tier);
                // --load-governor: tier-5 is the DEEP-shed, a strict SUPERSET of tier-4 — shed_holon above is
                // already true at tier≥4 (object_repair + scene-memory off every pair), and tier-5 ADDS: (a)
                // the BACKWARD optical-flow leg forced OFF (the bwd pyramid + bwd gme — see the do_bwd AND
                // below), and (b) the cheapest single-pass gme-fit (1 IRLS iter, the gme_iters override below).
                // Forward-flow + gme-fit + matte + warp + inertia (raw-flow WAP) still run, so F STILL
                // generates while the CPU tail drops enough to keep up under combat. tier5_active is only ever
                // true when pressure_tier>=5, reachable ONLY under --load-governor. The per-pair IRLS iteration
                // count — single pass at tier-5 (cheapest), else 2/3 (irls2/default).
                const bool tier5_active = (pressure_tier>=5);
                const uint32_t gme_iters = tier5_active ? 1u : (cfg.gme_irls2?2u:3u);
                // R5 step 3b: the FLOW rows decide. ArmInputs from this pair's CONTROL facts → the arm mask; a row acts
                // when it is effectively ON (cfg.layers.eff / avail, resolved at init against the cascades) AND armed.
                // R7: the former hand conditions are gone from here. They were the second oracle of R5 step 3b and
                // they counted 0 disagreements over 58,853 decisions across two pressured runs; the ARM half of what
                // they proved is now enumerated in tests/layers/test_arm_parity.cpp (every state, not the sampled
                // ones), the EFF half is layer_flow_resolve's loud exit 3 at every startup. The rows are the code.
                pfg::layers::ArmInputs fin{}; fin.has_prev=have_prev_f; fin.tier=pressure_tier; fin.holon_skip=holon_skip_pair; fin.pipelined=!allow_bwd; fin.bwd_skipping=bwd_skipping;
                const uint32_t feff=cfg.layers.eff, favail=cfg.layers.avail;
                auto rbit=[](pfg::layers::LayerId id){ return 1u<<(unsigned)id; };
                uint32_t farm=pfg::layers::layer_arm_mask(fin);
                const bool row_bidir = (feff & rbit(pfg::layers::LayerId::BIDIR)) && (farm & rbit(pfg::layers::LayerId::BIDIR));
                fin.bwd_ok=row_bidir; farm=pfg::layers::layer_arm_mask(fin);   // the backward legs' arm (BWD) reads the bidir row's decision
                auto row_on=[&](pfg::layers::LayerId id){ return (feff & rbit(id)) && (farm & rbit(id)); };
                const bool row_gme_ran    = (favail & rbit(pfg::layers::LayerId::GME)) && (farm & rbit(pfg::layers::LayerId::GME));   // the model exists this pair (either variant)
                const bool row_gme_gpu    = (feff & rbit(pfg::layers::LayerId::GME_GPU)) != 0u;
                const bool row_mem_fwd    = row_on(pfg::layers::LayerId::MEM_FWD);
                const bool row_objects    = row_on(pfg::layers::LayerId::OBJECTS);
                const bool row_gme_bwd    = row_on(pfg::layers::LayerId::GME_BWD);
                const bool row_mem_bwd    = row_on(pfg::layers::LayerId::MEM_BWD);
                const bool row_objects_bwd= row_on(pfg::layers::LayerId::OBJECTS_BWD);
                const bool row_mem_refresh= row_on(pfg::layers::LayerId::MEM_REFRESH);
                // allow_bwd folds in the pipeline's bwd-off rule (single ofp / 2 Bframe slots). tier-5 ALSO
                // forces it off (skip the bwd pyramid + bwd gme entirely).
                const bool do_bwd = row_bidir;
                if(do_bwd){
                    // --nvofa: the bwd direction (cur→prv). Run the OFA provider FIRST (it writes
                    // ofp.motion_image()=bwd MV, RO, via its own submits), THEN cmdB_bwd records the copy-out +
                    // gme reading that RO image — same shape as the classical record. The OFA reuses ofp's
                    // mv/sad images; the fwd match already consumed its fwd MV (copied out on cmdF before this)
                    // so overwriting is safe. nvofa is flow_div==1 → feed Bframe at WW (cur,prv swapped for bwd).
                    if(use_nvofa){
                        nvofa_run(A,nvofa,Bframe[cur_f].img,Bframe[prv_f].img,WW,WH,/*use_bwd=*/true,
                                  ofp.motion_image(),ofp.motion_view(),ofp.sad_field_image(),ofp.sad_field_view(),
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  flow_submit_q2_chain);   // the A.q2 prep/convert submits chain on semPrep/semOfa (1 CPU wait inside nvofa_run)
                    }
                    vkResetCommandBuffer(cmdB_bwd,0);
                    VkCommandBufferBeginInfo bib{}; bib.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdB_bwd,&bib);
                    // bwd direction is cur→prv. At flow_div>1 downsample (Bflow reused — the bwd record runs on
                    // its OWN cmd buffer cmdB_bwd, submitted+fenced (fB2) independently of the fwd cmdF, so
                    // reusing Bflow[0/1] is safe: the fwd match already consumed the fwd Bflow contents on cmdF
                    // before this bwd blit overwrites them). At flow_div==1 feed Bframe.
                    VkImageView ba=Bframe[cur_f].view, bb=Bframe[prv_f].view;
                    if(!use_nvofa){
                        if(flow_div>1u) flow_downsample(cmdB_bwd,cur_f,prv_f,ba,bb);
                        (void)ofp.record_optical_flow(cmdB_bwd,ba,bb,Cinterp.view,0.5f);
                    } else { (void)ba;(void)bb; }
                    img_barrier(cmdB_bwd,ofp.motion_image(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    { VkBufferImageCopy cp=full_bic(mvw,mvh); vkCmdCopyImageToBuffer(cmdB_bwd,ofp.motion_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hMVB_b[f_gen].buf,1,&cp); }
                    img_barrier(cmdB_bwd,ofp.motion_image(),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                    // ── gme-gpu: the BWD affine fit on B, in cmdB_bwd, against the bwd MV (just copied out,
                    // but the MV image still holds it) + the LIVE (bwd) SAD. NOTE the change-gate SAD source:
                    // the CPU bwd fit (gme_fit_affine call below) gates on the FWD sad (hostSAD); the GPU here
                    // gates on the bwd match's own sad_zero (the live ofp.sad_field_image()). For a TRUE block
                    // (sad_zero≈0 either way) this agrees; the small divergence is bwd-mask-only (the matte
                    // leading edge), what --gme-gpu-verify surfaces on the bwd anchor. Writes hostDISB[f_gen]
                    // (B import) + reuses hGmeM_b[f_gen] for the bwd model (read back into mb6 below). Recorded
                    // after the MV copy-out so MV stays RO.
                    if(use_gme_gpu){
                        gme_record(cmdB_bwd,gmePipe,ofp.motion_image(),ofp.sad_field_image(),mvw,mvh,
                                   /*step=*/1u,cfg.change_gate,f_gen,/*anchor=*/1,
                                   hDISB_b[f_gen].buf,(VkDeviceSize)mvw*mvh,hGmeMB_b[f_gen].buf,
                                   cfg.gme_irls2?2:3);
                    }
                    vkEndCommandBuffer(cmdB_bwd);
                    // the bidir bwd-flow submit. Routes to A.q2 under single_gpu (no-wait, fenced by fB2).
                    flow_submit_nowait(cmdB_bwd, fB2);
                }
                double gme_fit_total_ms=0.0; bool gme_did_fit=false; bool gme_did_bwd=false; double gme_dis_pct_fwd=0.0; float gme_m6_fwd[6]={};
                double obj_cost_ms=0.0; uint32_t obj_live_pair=0; uint32_t obj_rep_pair=0; uint32_t obj_infill_pair=0;
                if(row_gme_ran){
                    const double g0=now_ms();
                    float m6[6]={};
                    double dis_pct;
                    if(row_gme_gpu){
                        // ── gme-gpu: the GPU produced the model (hostGmeM) + dis-mask (hostDIS) in cmdF,
                        // already waited (fF). Read the 6 floats; dis% derives from the mask (a stat-only
                        // approximation, see gme_dispct_from_mask). NO CPU gme_fit_affine.
                        std::memcpy(m6, hostGmeM[f_gen], 6u*sizeof(float));
                        dis_pct = gme_dispct_from_mask((const uint8_t*)hostDIS[f_gen],mvw,mvh);
                        if(cfg.gme_gpu_verify){
                            // Cross-check: run the CPU fit into scratch + compare model rel-diff + mask flips.
                            const size_t nblk=(size_t)mvw*mvh;
                            if(gme_vfy_dis.size()<nblk) gme_vfy_dis.resize(nblk);
                            float c6[6]={};
                            const double cdis=gme_fit_affine(hostMV[f_gen],cfg.change_gate?hostSAD[f_gen]:nullptr,
                                                             mvw,mvh,c6,gme_vfy_dis.data(),/*sub2=*/false,(int)gme_iters);   // mirror tier-5's iter count for an honest GPU/CPU cross-check
                            double num=0.0,den=0.0; for(int _p=0;_p<6;++_p){ const double dd=(double)m6[_p]-(double)c6[_p]; num+=dd*dd; den+=(double)c6[_p]*(double)c6[_p]; }
                            const double reldiff = den>0.0 ? std::sqrt(num/den) : std::sqrt(num);
                            uint64_t flips=0; const uint8_t* gm=(const uint8_t*)hostDIS[f_gen];
                            for(size_t i=0;i<nblk;++i) if(gm[i]!=gme_vfy_dis[i]) ++flips;
                            ++gme_vfy_n;
                            // dis% cross-check under the SAME definition as the GPU stat (post-change-gate
                            // mask fraction) — the gme_fit_affine RETURN is the raw PRE-gate residual count
                            // (its `dissident` counter increments before the gate zeroes the mask byte), a
                            // DIFFERENT stat: printing them side-by-side read as a 2x GPU/CPU mismatch while
                            // flips=0 (measured: GPU~11% vs CPU 21.7%). Keep the raw one, labeled honestly —
                            // it quantifies how much dissidence the change gate rejects.
                            const double cdis_mask = gme_dispct_from_mask(gme_vfy_dis.data(),mvw,mvh);
                            if(gme_vfy_n<=5 || (gme_vfy_n%120u)==0u)
                                std::printf("[ra] gme-gpu-verify[%llu]: model rel-diff=%.3e dis-mask flips=%llu/%zu | GPU t=(%.2f,%.2f) CPU t=(%.2f,%.2f) | dis GPU~%.1f%% CPU~%.1f%% raw(pre-gate)%.1f%%\n",
                                    (unsigned long long)gme_vfy_n,reldiff,(unsigned long long)flips,nblk,
                                    m6[0],m6[3],c6[0],c6[3],dis_pct,cdis_mask,cdis);
                        }
                    } else {
                        // --load-governor: tier-5 uses the cheapest single-pass CPU fit (gme_iters = 1 at
                        // tier≥5, else 2/3). gme_iters equals cfg.gme_irls2?2:3 when load_governor is off
                        // (tier-5 unreachable).
                        gme_fit_affine(hostMV[f_gen],
                                       cfg.change_gate?hostSAD[f_gen]:nullptr,
                                       mvw,mvh,m6,
                                       (uint8_t*)hostDIS[f_gen],gme_sub2,(int)gme_iters);
                        // dis_pct = the POST-change-gate mask fraction — the SAME definition as the gme-gpu
                        // path above, so the dis:NN% telemetry, the CSV column and the --motion-fallback
                        // threshold keep ONE meaning whichever path ran (the raw gme_fit_affine return is
                        // the pre-gate residual count, ~2x larger scene-dependently). hostDIS is still
                        // pristine here (mem_merge/object_repair mutate it below).
                        dis_pct = gme_dispct_from_mask((const uint8_t*)hostDIS[f_gen],mvw,mvh);
                    }
                    const double gdt=now_ms()-g0;
                    gme_fit_total_ms+=gdt; gme_did_fit=true; gme_dis_pct_fwd=dis_pct;
                    for(int _p=0;_p<6;++_p){ f_pair_gme_a[f_gen][_p]=m6[_p]; gme_m6_fwd[_p]=m6[_p]; }
                    f_pair_gme_valid_a[f_gen]=1;
                    f_pair_disp_a[f_gen]=(float)gme_dis_pct_fwd;   // publish the per-pair gme dispersion to P (F-write-before-fetch_add ordering)
                    gme_dis_x100.store((uint64_t)(dis_pct*100.0+0.5));
                    if(row_mem_fwd){
                        const double mc0=now_ms();
                        mem_advect(hostMV[f_gen]);
                        mem_merge((uint8_t*)hostDIS[f_gen],hostMV[f_gen],mem_prior.data());
                        obj_cost_ms+=now_ms()-mc0;
                    }
                    // --mv-audit tap R: RAW matcher output (subpel/candsel per flags), pre-object_repair,
                    // over the OBJECT tiles the gme dis-mask just marked. hostDIS is pristine here (repair
                    // mutates it below). span-normalize to px/source-frame so the number is comparable to
                    // the ball's per-frame truth regardless of pair span.
                    double auR_mean=0.0; float auR_max=0.f; uint32_t auR_n=0;
                    const float au_thr=cfg.matte_thresh*255.0f;
                    const double au_span=(double)(span?span:1);
                    if(mv_audit_left>0){ mv_audit_stat(hostMV[f_gen],(const uint8_t*)hostDIS[f_gen],au_thr,&auR_mean,&auR_max,&auR_n); }
                    if(row_objects){
                        const double o0=now_ms();
                        uint32_t live=0,rep=0,infill=0;
                        object_repair(hostMV[f_gen],(uint8_t*)hostDIS[f_gen],m6,obj_slots_fwd,
                                      use_inertia?persist.data():nullptr,
                                      (use_inertia&&cfg.persist_reset)?persist.data():nullptr,
                                      span,+1.0,&live,&rep,&infill,
                                      nullptr,nullptr);
                        obj_cost_ms+=now_ms()-o0;
                        obj_live_pair=live; obj_rep_pair+=rep; obj_infill_pair+=infill;
                    }
                    // --mv-audit tap O: POST-object_repair (the field uploaded to the presenter). The dis-mask
                    // may have been rewritten by repair; use it as-is (the object footprint after repair).
                    if(mv_audit_left>0){
                        double auO_mean=0.0; float auO_max=0.f; uint32_t auO_n=0;
                        mv_audit_stat(hostMV[f_gen],(const uint8_t*)hostDIS[f_gen],au_thr,&auO_mean,&auO_max,&auO_n);
                        // tap C-sim: CPU replica of the present-side color-weighted 3x3 consensus (mv_median.comp
                        // guided path, DEFAULT ON). For a single uniform mover the color cohort ≈ the OBJECT
                        // tiles (dis>thr): each object tile takes the marginal .x-median over its object-tile 3x3
                        // neighbours (self always in; <3 object-neighbours → keep own value, mirroring the shader's
                        // "no cohort → fallback", which for the ball degenerates to self since the blind-9 median
                        // over a mostly-object window is still ~ball). Reports mean|mvx| over object tiles AFTER
                        // this consensus — the magnitude the WARP actually samples on the default path.
                        double auC_mean=0.0; float auC_max=0.f; uint32_t auC_n=0;
                        {
                            const uint16_t* hmv=(const uint16_t*)hostMV[f_gen];
                            const uint8_t*  dis=(const uint8_t*)hostDIS[f_gen];
                            const uint32_t MW=mvw_f, MH=mvh_f;
                            double sx=0.0; float mx=0.f; uint32_t nn=0;
                            for(uint32_t gy=0; gy<MH; ++gy) for(uint32_t gx=0; gx<MW; ++gx){
                                const size_t ci=(size_t)gy*MW+gx;
                                if((float)dis[ci]<=au_thr) continue;             // object tiles only
                                float xs[9]; int nc=0;
                                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx){
                                    const int nx=(int)gx+dx, ny=(int)gy+dy;
                                    if(nx<0||ny<0||nx>=(int)MW||ny>=(int)MH) continue;
                                    const size_t ni=(size_t)ny*MW+nx;
                                    const bool self=(dx==0&&dy==0);
                                    if(self || (float)dis[ni]>au_thr)            // cohort = self + object neighbours
                                        xs[nc++]=half_to_float(hmv[ni*2u+0u]);
                                }
                                float cmv;
                                if(nc>=4){ for(int a=1;a<nc;++a){ float k=xs[a]; int b=a-1; while(b>=0&&xs[b]>k){xs[b+1]=xs[b];--b;} xs[b+1]=k; } cmv=xs[nc/2]; }
                                else cmv=half_to_float(hmv[ci*2u+0u]);           // no cohort → keep own
                                const float ax=std::fabs(cmv); sx+=ax; if(ax>mx)mx=ax; ++nn;
                            }
                            auC_mean = nn?sx/(double)nn:0.0; auC_max=mx; auC_n=nn;
                        }
                        // INTERIOR-only |mv.x| (object tiles whose 4-neighbours are ALL object tiles): isolates
                        // the ball CORE from the silhouette edge tiles. If interior ≈ truth while the full mean
                        // is short, the gap is edge-dilution of the block grid, not a magnitude defect.
                        double auI_mean=0.0; uint32_t auI_n=0;
                        {
                            const uint16_t* hmv=(const uint16_t*)hostMV[f_gen];
                            const uint8_t*  dis=(const uint8_t*)hostDIS[f_gen];
                            const uint32_t MW=mvw_f, MH=mvh_f; double sx=0.0; uint32_t nn=0;
                            for(uint32_t gy=1; gy+1<MH; ++gy) for(uint32_t gx=1; gx+1<MW; ++gx){
                                const size_t ci=(size_t)gy*MW+gx;
                                if((float)dis[ci]<=au_thr) continue;
                                if((float)dis[ci-1]<=au_thr||(float)dis[ci+1]<=au_thr||(float)dis[ci-MW]<=au_thr||(float)dis[ci+MW]<=au_thr) continue;
                                sx+=std::fabs(half_to_float(hmv[ci*2u+0u])); ++nn;
                            }
                            auI_mean=nn?sx/(double)nn:0.0; auI_n=nn;
                        }
                        std::printf("[ra] mv-audit pair=%llu span=%.0f | R(raw): mean|mvx|=%.2f max=%.2f n=%u | O: mean=%.2f n=%u | C-sim: mean=%.2f n=%u | INTERIOR: mean=%.2f n=%u | R/span=%.2f INT/span=%.2f\n",
                            (unsigned long long)cur_c, au_span,
                            auR_mean, (double)auR_max, auR_n,
                            auO_mean, auO_n,
                            auC_mean, auC_n,
                            auI_mean, auI_n,
                            auR_mean/au_span, auI_mean/au_span);
                        --mv_audit_left;
                    }
                    f_pair_mfwd_a[f_gen]=(float)matte_mass_count((const uint8_t*)hostDIS[f_gen],mvw,mvh,cfg.matte_thresh);
                    if(objdump_left>0){
                        objdump_grid("dis",[&](size_t bi){ return ((const uint8_t*)hostDIS[f_gen])[bi]; });
                        objdump_grid("mv", [&](size_t bi){ const uint16_t* hh=(const uint16_t*)hostMV[f_gen];
                            const float mx=half_to_float(hh[bi*2u]); const float my=half_to_float(hh[bi*2u+1u]);
                            const float m=std::sqrt(mx*mx+my*my)*16.f; return (uint8_t)(m>255.f?255u:(uint8_t)m); });
                        objdump_grid("per",[&](size_t bi){ return use_inertia?persist[bi]:(uint8_t)0; });
                        ++objdump_idx; --objdump_left;
                        if(objdump_left==0) std::printf("[ra] objdump: complete — %llu pair grids in frames\\\n",(unsigned long long)objdump_idx);
                    }
                    f_pair_mbwd_a[f_gen]=0.f;
                } else if(use_gme){
                    f_pair_gme_valid_a[f_gen]=0;
                    f_pair_mfwd_a[f_gen]=0.f; f_pair_mbwd_a[f_gen]=0.f;
                    f_pair_disp_a[f_gen]=0.f;   // no fit this pair -> no dispersion signal (the gate also checks gme_valid)
                }
                f_pair_bwd_valid_a[f_gen] = do_bwd ? 1 : 0;
                if(use_bidir && !do_bwd && have_prev_f) stat_bwd_skips.fetch_add(1);
                if(do_bwd){
                    vk_wait_live(FD.dev,fB2);   // catch a TDR on the bwd flow — the consume-side wait, FD.dev (B.dev null under single_gpu would crash)
                    if(row_gme_bwd){
                        const double gb0=now_ms();
                        float mb6[6]={};
                        if(row_gme_gpu){
                            // ── gme-gpu: the bwd model + mask were produced in cmdB_bwd on B (waited via fB2
                            // just above). Read the 6 floats; the bwd mask is already in hostDISB. No CPU bwd fit.
                            std::memcpy(mb6, hostGmeMB[f_gen], 6u*sizeof(float));
                        } else {
                            const double dis_pct_b=gme_fit_affine(hostMVB[f_gen],
                                                                  cfg.change_gate?hostSAD[f_gen]:nullptr,
                                                                  mvw,mvh,mb6,
                                                                  (uint8_t*)hostDISB[f_gen],gme_sub2,cfg.gme_irls2?2:3);
                            (void)dis_pct_b;
                        }
                        gme_fit_total_ms+=now_ms()-gb0; gme_did_bwd=true;
                        if(row_mem_bwd){
                            const double mc0=now_ms();
                            mem_merge((uint8_t*)hostDISB[f_gen],hostMVB[f_gen],mem_adv.data());
                            obj_cost_ms+=now_ms()-mc0;
                        }
                        if(row_objects_bwd){
                            const double o0=now_ms();
                            uint32_t live_b=0,rep_b=0,infill_b=0;
                            wake_n=0;
                            object_repair(hostMVB[f_gen],(uint8_t*)hostDISB[f_gen],mb6,obj_slots_bwd,
                                          use_inertia?persist.data():nullptr,
                                          nullptr,
                                          span,-1.0,&live_b,&rep_b,&infill_b,
                                          use_memory?wake_rec.data():nullptr, use_memory?&wake_n:nullptr);
                            obj_cost_ms+=now_ms()-o0;
                            obj_rep_pair+=rep_b; obj_infill_pair+=infill_b;
                            (void)live_b;
                        }
                        f_pair_mbwd_a[f_gen]=(float)matte_mass_count((const uint8_t*)hostDISB[f_gen],mvw,mvh,cfg.matte_thresh);
                        for(int _p=0;_p<6;++_p) f_pair_gme_bwd_a[f_gen][_p]=mb6[_p];
                    }
                }
                if(row_mem_refresh){
                    const double mr0=now_ms();
                    mem_refresh(gme_did_bwd?(const uint8_t*)hostDISB[f_gen]:nullptr, gme_did_bwd,
                                wake_rec.data(), wake_n);
                    obj_cost_ms+=now_ms()-mr0;
                }
                if(use_gme&&gme_did_fit){
                    gme_fit_ema=gme_fit_ema>0.0?gme_fit_ema*0.8+gme_fit_total_ms*0.2:gme_fit_total_ms;
                    gme_fit_us.store((uint64_t)(gme_fit_ema*1000.0));
                    if(!gme_sub2&&gme_fit_ema>2.0) gme_sub2=true;
                    ++gme_fits;
                    if(gme_fits<=3)
                        std::printf("[ra] gme: t=(%.2f,%.2f)px b=%.4f c=%.4f e=%.4f f=%.4f dis:%.0f%% fit=%.2fms%s%s\n",
                            gme_m6_fwd[0],gme_m6_fwd[3],gme_m6_fwd[1],gme_m6_fwd[2],gme_m6_fwd[4],gme_m6_fwd[5],gme_dis_pct_fwd,gme_fit_total_ms,gme_did_bwd?" (fwd+bwd)":"",gme_sub2?" (sub2)":"");
                    if(!gme_fit_printed&&gme_fits>=1){ gme_fit_printed=true;
                        if(gme_fit_ema>1.0) std::printf("[ra] gme: fit cost EMA %.2fms%s (>1ms — shown in stats)\n",gme_fit_ema,gme_did_bwd?" (fwd+bwd)":""); }
                }
                if(row_objects){
                    obj_live.store((uint64_t)obj_live_pair);
                    const double rep_pct=obj_infill_pair?100.0*(double)obj_rep_pair/(double)obj_infill_pair:0.0;
                    obj_rep_x10.store((uint64_t)(rep_pct*10.0+0.5));
                    obj_cost_ema=obj_cost_ema>0.0?obj_cost_ema*0.8+obj_cost_ms*0.2:obj_cost_ms;
                    ++obj_pairs;
                    if(!obj_settle_printed&&obj_pairs>=1){ obj_settle_printed=true;
                        if(obj_cost_ema>0.3) std::printf("[ra] objects: F-side cost EMA %.2fms (>0.3ms — cluster+identity+repair, both anchors)\n",obj_cost_ema); }
                }
                if(cfg.fg_auto){ const double dt=now_ms()-tf0; t_fuse_ema=t_fuse_ema>0.0?t_fuse_ema*0.8+dt*0.2:dt; }
                // the per-pair F-COST that drives the bwd-skip + tier latches must be F's BUSY time, not
                // wall-clock-since-WAP-entry. SERIAL (fwd_fence==NULL): consume runs in the same iteration, so
                // now−tp0 == record+submit_wait+CPU = the busy time. PIPELINE (fwd_fence!=NULL): the deferred
                // consume runs inside the NEXT pair's iteration, so now−tp0 would wrongly fold in that pair's
                // ingest-wait + record. The correct pipeline cost is now−tf0 = fence-wait + CPU ≈ max(GPU-leg,
                // CPU) — the real F-throughput cost (GPU hidden when fully overlapped → ≈CPU; GPU-bound →
                // ≈GPU). The ingest-wait (F idle, only present when NOT in deficit) is correctly excluded.
                { const double dtp=(pc.fwd_fence!=VK_NULL_HANDLE)?(now_ms()-tf0):(now_ms()-tp0);
                  t_pair_ema=t_pair_ema>0.0?t_pair_ema*0.8+dtp*0.2:dtp; }
                stat_flow_us.store((uint64_t)(t_flow_ema*1000.0));
                stat_pair_us.store((uint64_t)(t_pair_ema*1000.0));
                // ── publish (the shared-tail logic for WAP, so the pipeline can DEFER it one pair).
                // f_pair_*[f_gen] written BEFORE the f_seq.fetch_add (the seq_cst pair orders them for P).
                f_pair_cseq_a[f_gen]=cur_c; f_pair_slot_a[f_gen]=s; f_pair_tcap_a[f_gen]=c_slots[s].t_cap_ms;
                f_pair_span_a[f_gen]=span; f_pair_n_a[f_gen]=N_use;
                stat_cons.fetch_add(1);
                f_seq.fetch_add(1);
                f_cv.notify_all();
                if(cfg.latency_trace){ const double tc=f_pair_tcap_a[f_gen];   // tcap→publish = the full F-side freshage contribution (pickup+ingest+convert+build); detect derived in stats
                    if(tc>0.0){ const double fp=(now_ms()-tc)*1000.0; if(fp>0.0&&fp<2000000.0){
                        const uint64_t pv=lt_fpub_us.load(); lt_fpub_us.store(pv?(uint64_t)((double)pv*0.8+fp*0.2):(uint64_t)fp); } } }
                if(cfg.fg_auto){
                    const double Tsrc=(double)src_interval_us.load()/1000.0;
                    const double pres=(double)present_cost_us.load()/1000.0;
                    span_ema=span_ema*0.9+(double)(span>1?span-1:0)*0.1;
                    if(dwell_sets>0) --dwell_sets;
                    if(span_ema>0.25&&live_n_f>1&&dwell_sets==0){
                        live_n_f-=1; span_ema=0.0; up_streak=0; deg_streak=0; dwell_sets=90;
                    } else if(dwell_sets==0){
                        const double set_live=t_fuse_ema+(double)std::max(0,live_n_f-2)*t_warp_ema;
                        const double pres_live=(double)live_n_f*pres;
                        if(live_n_f>1&&(set_live>0.90*Tsrc||(pres>0.0&&pres_live>1.2*Tsrc))){
                            up_streak=0;
                            if(++deg_streak>=3){ live_n_f-=1; deg_streak=0; dwell_sets=90; }
                        } else {
                            deg_streak=0;
                            if(live_n_f<cfg.fg_factor){
                                const int n=live_n_f+1;
                                const double set_t=t_fuse_ema+(double)std::max(0,n-2)*t_warp_ema;
                                const double pres_t=(double)n*pres;
                                if(set_t<=0.65*Tsrc&&(pres<=0.0||pres_t<=Tsrc)){
                                    if(++up_streak>=45){ live_n_f=n; up_streak=0; dwell_sets=90; }
                                } else up_streak=0;
                            }
                        }
                    }
                    live_n_atomic.store(live_n_f);
                }
}

}  // namespace pfg::flow
