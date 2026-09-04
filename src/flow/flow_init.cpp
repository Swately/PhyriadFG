// PhyriadFG — flow-side init (E1 of docs/planning/RESTRUCTURE_PLAN.md).
// Init-seq sections moved verbatim out of main.cpp behind ownership-struct binding preambles;
// bodies byte-identical to the pre-E1 sections except `goto done` -> `return false`.
#include "capture/wgc_ctx.hpp"   // FIRST: defines NOMINMAX before <windows.h> (winrt include order matters — same as main.cpp)
#include <algorithm>
#include <cstdio>
#include "core/app_init.hpp"
#include "nvofa_convert_spv.hpp"
#include "mv_smooth_spv.hpp"
#include "gme_reduce_spv.hpp"
#include "gme_solve_spv.hpp"
#include "gme_dissidence_spv.hpp"

// ── OpticalFlowPipeline (B) + NVOFA + MV-smooth + the primary-FG OFP (A) (E1: VERBATIM).
bool init_flow_pipes(Config& cfg, bool single_gpu, bool want_pfg, uint32_t WW, uint32_t WH,
                     uint32_t WW_flow, uint32_t WH_flow, uint32_t flow_div,
                     DevicesInit& o_dev, VDev& FD, ImagesInit& o_img, FlowPipesInit& o_flow){
    VDev& A=o_dev.A;
    auto& use_ambig=o_dev.use_ambig; auto& use_bidir=o_dev.use_bidir;
    auto& Bframe=o_img.Bframe; auto& Bflow=o_img.Bflow; auto& Cinterp=o_img.Cinterp;
    auto& ofp=o_flow.ofp; auto& nvofa=o_flow.nvofa; auto& use_nvofa=o_flow.use_nvofa;
    auto& mvsm=o_flow.mvsm; auto& mv_prev=o_flow.mv_prev; auto& use_mv_smooth=o_flow.use_mv_smooth;
    auto& ofpA=o_flow.ofpA; auto& AframeA=o_flow.AframeA; auto& pfg_enabled=o_flow.pfg_enabled;
    // ── OpticalFlowPipeline (B) ───────────────────────────────────────────────
    // emit_second_best (10th arg) = use_ambig — when ON the pipeline creates cand_image() (RGBA16F, same
    // mvw×mvh as motion_image()) and the finest match level writes the runner-up MV+SAD. When OFF (default)
    // cand_image() is VK_NULL_HANDLE and nothing extra is written (byte-identical).
    // Init the B-path flow at the (down)sampled work size WW_flow×WH_flow → motion_width()/motion_height()
    // = (WW_flow+7)/8 × (WH_flow+7)/8, so the MV grid (and the whole host-bridge / WAP / CPU-tail chain
    // that sizes off ofp.motion_width()) shrinks ~flow_div² with NO change to those sites. At flow_div==1,
    // WW_flow==WW → byte-identical init.
    // FD-route the flow pipeline (B→A under single_gpu). Under single_gpu ofp lives on A (the 4090); its
    // compute dispatches run on A.q2 (the F-thread's lane), serialized vs C's convert by a_q2_mtx.
    // fg_variant=true opts into the FG-LOCAL matcher with the runner-up tracking compiled out. INERT BY
    // DEFAULT — a --no-ambig artifact: the pipeline AND-folds !emit_second_best internally, and the FG
    // DEFAULT runs ambig=ON (cfg.ambig default true ⇒ emit_second_best = use_ambig = 1), so the variant
    // AUTO-FALLS-BACK to the canonical matcher (which produces the candidate field the ambig warp reads). It
    // short-circuits the runner-up tracking ONLY under --no-ambig (the matcher's raw pick), where the
    // runner-up was tracked unconditionally with no consumer. Kept: correct + byte-identical-off.
    if(!ofp.init(FD.phys,FD.dev,WW_flow,WH_flow,2u,2,cfg.res_ceil,cfg.conf_improv,cfg.agreement,use_ambig,/*mv_affine*/false,/*mv_subpel*/cfg.mv_subpel,/*coarse_wide*/false,/*mv_candsel*/cfg.mv_candsel,/*fg_variant*/true))
        { std::printf("[ra] OpticalFlowPipeline init failed\n"); return false; }
    // Smoke print: the live MV grid after init. At flow_div>1 this confirms the halved/quartered
    // grid (== (WW_flow+7)/8 × (WH_flow+7)/8). Printed once at startup.
    std::printf("[ra] flow-scale %u: MV grid %ux%u (work %ux%u, flow %ux%u)%s\n",
                flow_div, ofp.motion_width(), ofp.motion_height(), WW, WH, WW_flow, WH_flow,
                flow_div>1u?" — DRS active (~flow_div^2 fewer tiles + CPU work/pair)":" (byte-identical default)");
    // The pre-init host-bridge grid ((WW_flow+7)/8) MUST equal the accessor the post-init sites
    // read (ofp.motion_width()). They are derived the same way, but a divergence (e.g. a future change to
    // create_mv_image's rounding) would corrupt the copy-out — assert it here, cold path, once at startup.
    if(ofp.motion_width()!=(WW_flow+7u)/8u || ofp.motion_height()!=(WH_flow+7u)/8u){
        std::printf("[ra] FATAL: MV-grid mismatch — ofp.motion_width()=%u expected=%u (host bridges would over/under-read). Aborting init.\n",
                    ofp.motion_width(),(WW_flow+7u)/8u);
        return false;
    }
    if(cfg.mv_prior) ofp.set_temporal_prior(true);   // arm the dual-centre temporal prior (B-path)
    // --fg-prebake: pre-bake the two ping-pong descriptor collections so the F-thread's per-pair
    // record_optical_flow SKIPS the vkUpdateDescriptorSets burst. The level-0 inputs the F-thread feeds
    // are FIXED: at flow_div==1 the matcher reads the two Bframe views in BOTH orderings (fwd = (prv,cur),
    // bwd = (cur,prv), and prv/cur ping-pong over {0,1}); at flow_div>1 it always reads the two Bflow
    // scratch views (the downsample blits into Bflow[0]/Bflow[1] regardless of parity). c_view is Cinterp
    // every record. INERT BY DEFAULT — a --no-ambig artifact. The FG DEFAULT runs ambig=ON (the
    // periodic-texture killer, which USES the runner-up tracking to arbitrate SAD ties), so
    // prebake_fg_descriptors — which requires match_fg_active() (the variant matcher, itself gated OFF
    // whenever emit_second_best/ambig is on) AND no affine — returns FALSE on the default, the per-record
    // update path stays armed (byte-identical), and the print below reads INERT. It ELIMINATES the burst
    // ONLY under --no-ambig (the matcher's raw pick). Cold call (no GPU work).
    if(cfg.fg_prebake){
        const VkImageView pa0 = (flow_div>1u) ? Bflow[0].view : Bframe[0].view;
        const VkImageView pb0 = (flow_div>1u) ? Bflow[1].view : Bframe[1].view;
        const VkImageView pa1 = (flow_div>1u) ? Bflow[0].view : Bframe[1].view;   // swapped ordering (bwd / odd parity)
        const VkImageView pb1 = (flow_div>1u) ? Bflow[1].view : Bframe[0].view;
        const bool baked = ofp.prebake_fg_descriptors(pa0,pb0,pa1,pb1,Cinterp.view);
        std::printf("[ra] --fg-prebake: %s (fg_variant_active=%d, use_ambig=%d) — per-pair vkUpdateDescriptorSets burst %s.\n",
                    baked?"ARMED":"INERT (eligibility not met)", (int)ofp.match_fg_active(), (int)use_ambig,
                    baked?"ELIMINATED (host-CPU relief; read the call-count delta at F teardown)":"unchanged");
    }
    // ── --nvofa: build the hardware OFA flow provider ─────────────
    // Gated on --nvofa AND single_gpu (the OFA is 4090-only — FD==A; NVOFA is a single-GPU lever) AND A
    // actually armed the OFA queue (vdev_create auto-disabled if the device lacked VK_NV_optical_flow). On
    // ANY failure → print + fall back to the classical ofp (use_nvofa stays false → byte-identical). The
    // session is sized so its 4x4 grid == ofp.motion_width()/height() exactly (in_w=mvw*4). use_bidir picks
    // BOTH_DIRECTIONS. The convert lands the OFP contract (RG16F MV in WW_flow px, cost->sad_best, a
    // SEPARATE |A-B|->sad_zero), so every downstream consumer reads ofp.motion_image()/sad_field_image()
    // UNCHANGED.
    if(cfg.nvofa){
        if(!single_gpu){
            std::printf("[ra] --nvofa: multi-GPU rig — the flow rides device B (1080 Ti, no OFA); NVOFA is a single-GPU lever (use --force-single-gpu). Falling back to the classical OFP (byte-identical).\n");
        } else if(flow_div>1u){
            // LIMITATION: at flow_div>1 the classical OFP produces MV in WW_flow pixel units (it searches
            // the Bflow downscale); reconciling those units with the OFA path (which would need the Bflow
            // source, produced mid-cmdF) is unsupported. At flow_div==1 (the operating point — 1080p)
            // WW_flow==WW so mv_scale=WW/in_w is exact. Refuse nvofa under --flow-scale>1 (classical OFP).
            std::printf("[ra] --nvofa: --flow-scale %u (flow_div>1) not supported by the v1 OFA path (unit reconciliation is a follow-up) — falling back to the classical OFP (byte-identical).\n",flow_div);
        } else if(A.ofaQueue==VK_NULL_HANDLE || !A.has_optical_flow){
            std::printf("[ra] --nvofa: device A lacks VK_NV_optical_flow (or no OFA queue) — falling back to the classical OFP (byte-identical).\n");
        } else {
            const std::vector<uint32_t> spvnv(kNvofaConvertSpv.begin(),kNvofaConvertSpv.end());
            const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();
            if(nvofa_create(A,nvofa,mvw,mvh,WW_flow,use_bidir,cfg.nvofa_cost_scale,cfg.nvofa_sadz_scale,spvnv)
               && nvofa_alloc_cmds(A,nvofa)){
                VkDescriptorSetAllocateInfo dsa{}; dsa.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dsa.descriptorPool=nvofa.dp; dsa.descriptorSetCount=1; dsa.pSetLayouts=&nvofa.dsl;
                bool sok = (vkAllocateDescriptorSets(A.dev,&dsa,&nvofa.setF)==VK_SUCCESS);
                if(sok && use_bidir) sok = (vkAllocateDescriptorSets(A.dev,&dsa,&nvofa.setB)==VK_SUCCESS);
                if(sok){
                    // fwd set: OFA fwd flow/cost + inputs + ofp's OWN MV/SAD images.
                    nvofa_write_set(A,nvofa,nvofa.setF,nvofa.mvFiv,nvofa.costFuv,nvofa.in0cv,nvofa.in1cv,ofp.motion_view(),ofp.sad_field_view());
                    if(use_bidir) nvofa_write_set(A,nvofa,nvofa.setB,nvofa.mvBiv,nvofa.costBuv,nvofa.in0cv,nvofa.in1cv,ofp.motion_view(),ofp.sad_field_view());
                    use_nvofa=true;
                    // Residual (VUID-vkCmdDraw-None-09600, 2 occ at startup): on the OFA path ofp.execute()
                    // never runs, so an OFP-adjacent image consumed by a GPU command is reported UNDEFINED for
                    // ~the first 2 frames (deferred submit-time layout check). Benign — content is don't-care at
                    // startup, the path runs correctly (cap recovers, frz 0, slice drops, byte-identical off).
                    // Not motion/sad (a one-shot RO seed of those does NOT clear it) nor cand (null without --ambig).
                    std::printf("[ra] --nvofa: ACTIVE — HW OFA flow provider (in %ux%u, grid %ux%u, mv_scale %.3f, bidir=%d, cost_scale %.3f, sadz_scale %.3f). Replaces the classical block-match. sad_best=cost remap + sad_zero=separate |A-B| pass (BOTH need eye-calibration).\n",
                                nvofa.in_w,nvofa.in_h,mvw,mvh,nvofa.mv_scale,(int)use_bidir,cfg.nvofa_cost_scale,cfg.nvofa_sadz_scale);
                } else { std::printf("[ra] --nvofa: descriptor-set alloc failed — falling back to the classical OFP\n"); nvofa_destroy(A,nvofa); }
            } else { std::printf("[ra] --nvofa: provider/cmd init failed — falling back to the classical OFP\n"); nvofa_destroy(A,nvofa); }
        }
    }
    // ── Temporal MV smoothing resources (B-path, opt-in) ─────────
    if(cfg.mv_smooth>0.f){
        const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();
        if(img_create(FD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,mv_prev)){   // FD-route mv_prev (B→A)
            // Zero-seed prev (the cut-bypass self-seeds it on the first real-motion pair) → GENERAL.
            oneshot(FD,[&](VkCommandBuffer c){   // FD-route the zero-seed oneshot
                img_barrier(c,mv_prev.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                VkClearColorValue z{}; VkImageSubresourceRange r{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                vkCmdClearColorImage(c,mv_prev.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&z,1,&r);
                img_barrier(c,mv_prev.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
            });
            const std::vector<uint32_t> spvmv(kMvSmoothSpv.begin(),kMvSmoothSpv.end());
            use_mv_smooth=mvsm_create(FD,ofp.motion_image(),mv_prev.view,spvmv,mvsm);   // FD-route the MV-smooth pipeline (B→A)
        }
        if(use_mv_smooth) std::printf("[ra] mv-smooth: ACTIVE (alpha=%.2f, B-path) — temporal MV EMA\n",cfg.mv_smooth);
        else std::printf("[ra] mv-smooth: requested but resource init failed — disabled\n");
    }
    // ── OpticalFlowPipeline (A) — primary-FG path ────────────────
    // OFP on A reads AframeA[prv/cur] (already on A's VRAM) → zero PCIe transfer.
    if(want_pfg&&AframeA[0].img){
        // The primary-FG OFP always runs with emit_second=false (it never feeds the ambig warp) → it is a
        // pure default-path consumer of the runner-up-free FG-variant matcher.
        pfg_enabled=ofpA.init(A.phys,A.dev,WW,WH,2u,2,cfg.res_ceil,cfg.conf_improv,cfg.agreement,/*emit_second*/false,/*mv_affine*/false,/*mv_subpel*/cfg.mv_subpel,/*coarse_wide*/false,/*mv_candsel*/cfg.mv_candsel,/*fg_variant*/true);
        if(!pfg_enabled) std::printf("[ra] ofpA (primary-FG) init failed — assist path only\n");
        else if(cfg.mv_prior) ofpA.set_temporal_prior(true);   // arm the prior on the A (primary-FG) OFP too
    }
    return true;
}
// ── gme-gpu pipeline build + the use_* re-finalization (E1: moved VERBATIM from main.cpp).
// Re-derives use_gme/use_ambig/use_matte/use_objects/use_memory from the POST-alloc cfg state
// (bridge/image failures above may have cleared flags) and builds the device-B gme pipeline set.
void init_gme_finalize(Config& cfg, DevicesInit& o_dev, FlowPipesInit& o_flow, GmeGpuInit& o_gme){
    VDev& B=o_dev.B;
    auto& use_wap=o_dev.use_wap; auto& use_gme=o_dev.use_gme; auto& use_ambig=o_dev.use_ambig;
    auto& use_matte=o_dev.use_matte; auto& use_objects=o_dev.use_objects; auto& use_memory=o_dev.use_memory;
    auto& use_bidir=o_dev.use_bidir; auto& use_rescue=o_dev.use_rescue; auto& use_fill_div=o_dev.use_fill_div;
    auto& use_mv_guided=o_dev.use_mv_guided; auto& use_inertia=o_dev.use_inertia;
    auto& ofp=o_flow.ofp;
    auto& gmePipe=o_gme.gmePipe; auto& use_gme_gpu=o_gme.use_gme_gpu;
    auto& hDIS_b=o_gme.hDIS_b; auto& hDISB_b=o_gme.hDISB_b;
    // Re-finalize use_gme — cfg.gme may have been cleared by a DIS bridge/image failure above, and use_wap
    // by a WAP pipeline failure. The fit + mask + warp consumption all require both live.
    use_gme=cfg.gme&&use_wap;
    if(use_gme) std::printf("[ra] gme: ACTIVE — global affine model fitted per pair (F, IRLS 2-iter); dissidence mask → wapDISA; model rescue%s + fill-div assist%s\n",
                            use_rescue?"":" (off: needs --rescue)", use_fill_div?"":" (off: needs --fill-div)");
    // ── gme-gpu: build the device-B affine-fit pipeline set. Requires the FINAL use_gme (the dis bridges
    // + model readback only exist when gme is live) AND cfg.gme_gpu (the B-side bridges were imported
    // above only then). The OFP MV/SAD grid is ofp.motion_width()/height(). want_bwd = use_bidir (the bwd
    // anchor needs its own dis sets). On any create failure we clear use_gme_gpu → the CPU gme_fit_affine
    // path runs (byte-identical). Off = zero pipeline allocated.
    use_gme_gpu=use_gme&&cfg.gme_gpu;
    // gme-gpu is DEFAULT ON, so guard the device-B precondition explicitly: the B-side pipelines + dis
    // bridges + model readback all live on device B (the 1080 Ti). On a topology WITHOUT a usable device B
    // (B.dev null — the upstream A+B requirement at the LUID/vdev_create gate normally exits first, this is
    // the belt-and-suspenders guard so default-ON can never crash here), or if cfg.gme_gpu was already
    // cleared by a B-side bridge-import failure above, route to the CPU gme_fit_affine (byte-identical)
    // instead of building the B pipelines.
    if(use_gme&&cfg.gme_gpu&&B.dev==VK_NULL_HANDLE){
        std::printf("[ra] --gme-gpu: device B unavailable -> CPU gme fallback\n");
        use_gme_gpu=false; cfg.gme_gpu=false; cfg.gme_gpu_verify=false;
    }
    if(use_gme_gpu){
        const uint32_t gmvw=ofp.motion_width(), gmvh=ofp.motion_height();
        VkBuffer disF[kGenRing]={}, disB[kGenRing]={};
        for(int _g=0;_g<kGenRing;++_g){ disF[_g]=hDIS_b[_g].buf; disB[_g]=hDISB_b[_g].buf; }
        const std::vector<uint32_t> rspv(kGmeReduceSpv.begin(),kGmeReduceSpv.end());
        const std::vector<uint32_t> sspv(kGmeSolveSpv.begin(),kGmeSolveSpv.end());
        const std::vector<uint32_t> dspv(kGmeDissidenceSpv.begin(),kGmeDissidenceSpv.end());
        if(!gme_create(B,ofp.motion_image(),ofp.sad_field_image(),disF,use_bidir?disB:nullptr,
                       (VkDeviceSize)gmvw*gmvh,use_bidir,rspv,sspv,dspv,gmePipe)){
            std::printf("[ra] gme-gpu: pipeline create failed — falling back to CPU gme\n");
            gme_destroy(B,gmePipe); use_gme_gpu=false;
        } else {
            std::printf("[ra] gme-gpu: ACTIVE — gme_fit_affine offloaded onto device B (reduce->solve x3 + dissidence, ONE B cmd buffer); only the 6-float model reads back%s\n",
                        cfg.gme_gpu_verify?" (+ --gme-gpu-verify: CPU cross-check per pair)":"");
        }
    }
    // Re-finalize use_ambig — it requires the FINAL use_gme (the referee is gme_model_mv). If gme was
    // cleared above (a DIS bridge/image failure) the candidate field has no model to arbitrate against,
    // so ambiguity falls to OFF (byte-identical — the push flag is 0, the binding stays a valid placeholder).
    // use_ambig already folded its own bridge/image-alloc failures; ANDing the final use_gme closes the gap.
    use_ambig=use_ambig&&use_gme;
    if(use_ambig) std::printf("[ra] ambig: ACTIVE — B emits the second-best candidate per pair (cand_image → wapC2A binding 10); the warp arbitrates SAD ties on background/texture interiors vs the gme model referee\n");
    // Re-finalize use_matte — the matte consumes wapDISA (the object layer) + gme_model_mv() (the
    // background layer), both gated by use_gme. If gme was cleared above (DIS-image failure) the matte
    // falls back to OFF (byte-identical) with a printed note — never a half-built matte.
    use_matte=cfg.matte&&use_gme;
    if(cfg.matte&&!use_gme) std::printf("[ra] --matte disabled — its layers come from gme (the dissidence mask + the global model), which is not active\n");
    if(use_matte) std::printf("[ra] matte: ACTIVE — boundary-first two-layer compositing; advected-dissidence binary matte (thr=%.2f, r>%.1fpx); BACKGROUND → %s model layer%s\n",
                            cfg.matte_thresh, cfg.matte_thresh*255.0f/16.0f, cfg.crescent?"crescent-weighted":"pure", use_mv_guided?" (+color cross-check)":"");
    // Re-finalize use_objects — it clusters the gme dissidence mask + reuses the gme decode, so a gme
    // clear above (DIS bridge/image failure) takes objects with it (the repair has nothing to act on).
    // When live it prints its parameters; the shield note flags the inertia coupling.
    use_objects=cfg.objects&&use_gme;
    if(cfg.objects&&!use_gme) std::printf("[ra] --objects disabled — the inheritance repair acts on the gme dissidence mask, which is not active\n");
    if(use_objects) std::printf("[ra] objects: ACTIVE — connected-component clustering (k=%d, min-mass=%d) + 16-slot temporal identity + %s repair (inh≥%.0fpx, static≤%.0fpx)%s\n",
                            kObjSlots,kObjMinMass,cfg.shapefield?"contour shape-field motion-inheritance":"rigid single-MV motion-inheritance",kObjInhMin,kObjStaticMax,use_inertia?" + HUD shield (persist≥128)":" (no HUD shield — inertia off)");
    // Re-finalize use_memory — the scene-holon silhouette memory rides the object-holon (it advects +
    // merges the dissidence prior INTO the masks the repair then clusters), so a gme/objects clear above
    // takes the memory with it (nothing to advect, no holon to keep alive). When live it prints its decay
    // constant; --no-memory leaves the masks fresh-only.
    use_memory = use_objects && cfg.scene_memory;   // re-finalized after the gme/objects clears above
    if(cfg.scene_memory&&!use_objects) std::printf("[ra] --memory disabled — the silhouette memory rides the object-holon, which is not active\n");
    if(use_memory) std::printf("[ra] memory: ACTIVE — persistent CUR-anchored silhouette prior advected by the fwd MV field; merged max(fresh, %.2f·prior) before clustering; expiration guards confident background\n",kPriorDecay);
}

// Made with my soul - Swately <3
