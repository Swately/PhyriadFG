// PhyriadFG — warp_blend-side init (E1 of docs/planning/RESTRUCTURE_PLAN.md).
// The WAP init section moved verbatim out of main.cpp behind an ownership-struct binding
// preamble; the body is byte-identical to the pre-E1 main.cpp section.
#include "capture/wgc_ctx.hpp"   // FIRST: defines NOMINMAX before <windows.h> (winrt include order matters — same as main.cpp)
#include <algorithm>
#include <cstdio>
#include "core/app_init.hpp"
#include "wap_warp_spv.hpp"
#include "mv_median_spv.hpp"
#include "wap_fill_spv.hpp"

// ── warp-at-presenter pipeline build (E1: moved VERBATIM from main.cpp's init-seq).
// Builds the A-side WAP pipeline + its sampled inputs, the --upload-xfer machinery, the mass
// counter, the dump readbacks and the mv-median/afill companion pipes. Failures degrade
// (use_wap/use_* cleared) exactly as before — never fatal, so the function returns void.
void init_wap(Config& cfg, uint32_t WW, uint32_t WH, uint32_t WW_warp, uint32_t WH_warp,
              DevicesInit& o_dev, FlowPipesInit& o_flow, WapInit& o_wap){
    VDev& A=o_dev.A;
    auto& use_wap=o_dev.use_wap; auto& use_bidir=o_dev.use_bidir; auto& use_ambig=o_dev.use_ambig;
    auto& use_mv_median=o_dev.use_mv_median; auto& use_mv_guided=o_dev.use_mv_guided;
    auto& ofp=o_flow.ofp;
    auto& xfer_on=o_wap.xfer_on; auto& xfer_fams=o_wap.xfer_fams; auto& cmdUpload=o_wap.cmdUpload;
    auto& wapPipeA=o_wap.wapPipeA; auto& fillPipeA=o_wap.fillPipeA;
    auto& wapPrevA=o_wap.wapPrevA; auto& wapCurA=o_wap.wapCurA; auto& wapMVA=o_wap.wapMVA;
    auto& wapSADA=o_wap.wapSADA; auto& wapOutA=o_wap.wapOutA;
    auto& wapFIELDA=o_wap.wapFIELDA; auto& wapFIELDph=o_wap.wapFIELDph;
    auto& wapMVBA=o_wap.wapMVBA; auto& wapC2A=o_wap.wapC2A; auto& wapMVTA=o_wap.wapMVTA;
    auto& wapPrevOutA=o_wap.wapPrevOutA;
    auto& medPipe=o_wap.medPipe; auto& wapMVScratchA=o_wap.wapMVScratchA;
    auto& wapDISA=o_wap.wapDISA; auto& wapDISBA=o_wap.wapDISBA; auto& wapPERA=o_wap.wapPERA;
    auto& hostMassPtr=o_wap.hostMassPtr; auto& hMass_a=o_wap.hMass_a; auto& devMass=o_wap.devMass;
    auto& hostOutD=o_wap.hostOutD; auto& hOutD_a=o_wap.hOutD_a;
    auto& hostPrevD=o_wap.hostPrevD; auto& hPrevD_a=o_wap.hPrevD_a;
    auto& hostCurD=o_wap.hostCurD; auto& hCurD_a=o_wap.hCurD_a;
    // ── warp-at-presenter pipeline (A, the bridge owner) ──────────
    // Presenter-local sampled inputs (two pair reals WW×WH RGBA8, MV+SAD grids RG16F) + the rgba8 warp
    // output, re-uploaded per pair-advance and re-warped per tick at the exact phase. The warper is A
    // (the bridge owner); the WD/WP aliases name the A objects (wapPipeA etc.).
    if(use_wap){
        VDev& WD = A;
        WapPipe& WP = wapPipeA;
        // --upload-xfer: the SINGLE runtime gate. ON only when the flag is set AND the device actually
        // armed a transfer queue (qT non-null ⇒ qfamT!=UINT32_MAX). OFF → EVERYTHING below stays exactly
        // as is: EXCLUSIVE images, the upload on A.q with its sync vkWaitForFences, no A.qT submit, no
        // timeline semaphores touched (byte-identical-off). qfamT is a COMPUTE-capable family on the 4090
        // (fam2, flags 0xE) so the in-upload median dispatch is valid on A.qT. The CONCURRENT family pair
        // {qfam,qfamT} is built once here for the wap-input images.
        xfer_on = cfg.upload_xfer && WD.qT != VK_NULL_HANDLE;
        xfer_fams[0] = WD.qfam; xfer_fams[1] = WD.qfamT;
        // The in-upload median pass (use_mv_median/use_mv_guided) is a COMPUTE dispatch that runs on A.qT
        // — so qfamT MUST carry the COMPUTE bit. The transfer-queue pick prefers a COMPUTE&&!GRAPHICS
        // family, but can fall back to a transfer-only family when none exists; running a dispatch there is
        // invalid → device-lost. Rather than fork the median back to A.q (a cross-queue cmd-buffer split),
        // force xfer OFF if qfamT lacks COMPUTE (degrade to the byte-identical A.q sync upload — never a
        // crash). On the 4090 fam2 is 0xE (has COMPUTE).
        if(xfer_on){
            uint32_t qfc=0; vkGetPhysicalDeviceQueueFamilyProperties(WD.phys,&qfc,nullptr);
            std::vector<VkQueueFamilyProperties> qfs(qfc); vkGetPhysicalDeviceQueueFamilyProperties(WD.phys,&qfc,qfs.data());
            if(WD.qfamT>=qfc || !(qfs[WD.qfamT].queueFlags&VK_QUEUE_COMPUTE_BIT)){
                std::printf("[ra] --upload-xfer: qfamT=%u lacks the COMPUTE bit — the in-upload median dispatch cannot run on A.qT; forcing the upload back onto A.q (byte-identical)\n", WD.qfamT);
                xfer_on=false;
            }
        }
        // --upload-xfer: allocate the PING-PONG upload command buffers from A.poolT ONCE here (init,
        // zero-alloc hot path). A.poolT is RESET_COMMAND_BUFFER_BIT + family-bound to qfamT, so a
        // cmdUpload[] buffer submits to A.qT with no family trap. On a failure we force xfer_on OFF
        // (degrade to the byte-identical A.q sync upload — never a crash). Both buffers must allocate or
        // neither is used.
        if(xfer_on){
            VkCommandBufferAllocateInfo cbiU{}; cbiU.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbiU.commandPool=WD.poolT; cbiU.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbiU.commandBufferCount=2;
            if(vkAllocateCommandBuffers(WD.dev,&cbiU,cmdUpload)!=VK_SUCCESS){
                std::printf("[ra] --upload-xfer: ping-pong upload cmd buffer alloc on A.poolT FAILED — forcing the upload back onto A.q (byte-identical)\n");
                cmdUpload[0]=cmdUpload[1]=VK_NULL_HANDLE; xfer_on=false;
            }
        }
        if(xfer_on) std::printf("[ra] --upload-xfer: ACTIVE — wap-input images CONCURRENT {qfam=%u,qfamT=%u}, upload→A.qT with two timeline semaphores + ping-pong cmd buffers (no host wait, no QFOT, no CPU mutex)\n", WD.qfam, WD.qfamT);
        Img& wPrev = wapPrevA;
        Img& wCur  = wapCurA;
        Img& wMV   = wapMVA;
        Img& wSAD  = wapSADA;
        Img& wOut  = wapOutA;
        Img& wMVB  = wapMVBA;   // backward MV (binding 5) — created unconditionally so the descriptor set
                                // is complete; uploaded + read only when use_bidir.
        Img& wC2   = wapC2A;    // second-best candidate (binding 10) — created only with use_ambig
                                // (RGBA16F); when off the binding is placeholder-bound to wMV.view (never sampled).
        // The WAP sampled MV/SAD/MVB grids are sized off ofp.motion_width()/height() (the single source of
        // truth, valid here — after ofp.init). At flow_div>1 they shrink with the OFP grid; the pair-real
        // images (wPrev/wCur) stay full-res WW×WH (the warp samples the captured frames full-res, upsampling
        // the smaller MV field via textureSize() in the shader). At flow_div==1 == (WW+7)/8.
        const uint32_t mvw=ofp.motion_width(), mvh=ofp.motion_height();
        // wPrev/wCur/wMV/wSAD/wMVB are UPLOAD-WRITTEN (A.qT) + WARP-READ (A.q) → CONCURRENT {qfam,qfamT}
        // when xfer_on (no QFOT); EXCLUSIVE otherwise (byte-identical). wOut is the warp OUTPUT (written +
        // blitted both on A.q, same queue) → stays EXCLUSIVE (no cross-queue).
        if(!img_create(WD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wPrev,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wCur,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wMV,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wSAD,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wMVB,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)||
           !img_create(WD,WW_warp,WH_warp,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,wOut)){   // wOut(=wapOutA) at WW/N×WH/N (the descriptor binds this scaled view at wap_create; the shader's imageSize(u_output) drives the per-invocation work). warp_div==1 ⇒ WW_warp==WW (byte-identical). OURS only — the pair-reals wPrev/wCur above stay full-res (no game cap).
            std::printf("[ra] WAP image allocation failed — disabling warp-at-presenter\n"); use_wap=false;
        } else {
            // The inertia persistence image (R8, mvw×mvh) — created UNCONDITIONALLY with WAP (the SAME
            // completeness discipline wMVB uses for binding 5): binding 8 must hold a valid R8 view
            // regardless of --inertia, so wap_create's descriptor set is always complete. Uploaded per pair
            // from hPER_a ONLY when use_inertia (the upload transitions DST→RO); off-inertia it stays the
            // initial RO image and the shader never samples it (inertia_thresh=0 gates every read). On a
            // creation failure we clear cfg.inertia and bind the MV view as a harmless placeholder below.
            if(!img_create(WD,mvw,mvh,VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapPERA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read → CONCURRENT under xfer_on
                std::printf("[ra] WAP PER image failed — disabling inertia\n"); cfg.inertia=false;
            }
            // The A-side iGPU contour field image (R32_UINT, full-res WW×WH). STORAGE (the fill's
            // imageLoad/Store via fillPipeA AND the warp's binding-11 imageLoad) | TRANSFER_DST (the per-pair
            // upload from hFIELD_a). Created when EITHER --afill (the visualizer) OR --bg-snap (the warp
            // consumer) owns the field; on failure we clear BOTH (graceful degrade — the WAP path keeps
            // running without the field, not a crash). When NEITHER owns it, a 1×1 r32ui PLACEHOLDER
            // (wapFIELDph) is bound to binding 11 instead: binding 11 is statically used in the SPIR-V so it
            // must always hold a valid r32ui storage view, and the candidate-binding-10 float-view placeholder
            // trick does NOT work for an integer storage image (format must match) — a real 1×1 r32ui image is
            // the cheap placeholder (never read: bg_snap_on=0 when bg_snap is off → byte-identical).
            if(cfg.d.field_to_warp){   // ALL field consumers, not just afill||bg_snap
                if(!img_create(WD,WW,WH,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapFIELDA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 11) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP FIELD image failed — disabling all field consumers (afill/bg-snap/band-xfade/disoccl-hardpick)\n"); cfg.afill=false; cfg.bg_snap=false; cfg.band_xfade=0.f; cfg.disoccl_hardpick=0.f; cfg.d.field_to_warp=false;
                }
            }
            if(!wapFIELDA.img){   // binding-11 r32ui placeholder (1×1, GENERAL, never sampled when bg_snap off)
                if(!img_create(WD,1,1,VK_FORMAT_R32_UINT,VK_IMAGE_USAGE_STORAGE_BIT,wapFIELDph)){
                    std::printf("[ra] WAP FIELD placeholder failed — disabling warp-at-presenter\n"); use_wap=false;
                }
            }
            // The second-best candidate image (RGBA16F, mvw×mvh) — created only with use_ambig. Bound as
            // warp binding 10 (u_candidates); uploaded per pair from hC2_a (DST→RO). On a creation failure
            // we clear use_ambig (the warp falls to byte-identical off) and bind wMV.view as a harmless
            // placeholder below (the same completeness trick wMVB/wapDISA use). When use_ambig was already
            // off (e.g. --no-ambig or the bridge alloc failed above) the image is simply not created.
            if(use_ambig){
                if(!img_create(WD,mvw,mvh,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wC2,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 10) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP candidate image failed — disabling ambiguity\n"); use_ambig=false;
                }
            }
            // --vblend: the NEXT-pair forward-MV TARGET image (RG16F, mvw×mvh — the SAME format/size as
            // wapMVA, mirrored exactly). Bound as warp binding 12 (u_mv_target); uploaded per pair from
            // hMV_a[target_gen] (the next-fresher published generation). Created only with cfg.vblend; on a
            // creation failure we clear cfg.vblend (the warp falls to byte-identical off — vblend_on=0 push,
            // the placeholder binding) and bind wMV.view as a harmless unread placeholder below (the same
            // completeness trick wMVB/wapC2A use). When vblend is off the image is simply not created.
            if(cfg.vblend){
                if(!img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapMVTA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 12) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP MV-target image failed — disabling vblend\n"); cfg.vblend=false;
                }
            }
            // --ts-smooth: the PREVIOUS final-output history image (RGBA8, full-res WW×WH — the SAME format/
            // size as wOut, mirrored exactly). Bound as warp binding 13 (u_prev_out, sampled); the copy-back
            // (after the per-tick blit) writes wOut → wapPrevOutA, so the next tick's warp samples THIS tick's
            // output. SAMPLED (the warp's binding-13 sample) | TRANSFER_DST (the per-tick copy target).
            // Created only with cfg.ts_smooth>0; on a creation failure we clear cfg.ts_smooth (the warp falls
            // to byte-identical off — ts_smooth=0 push, the placeholder binding) and bind wCur.view as a
            // harmless unread placeholder below (the same completeness trick wMVB/wapC2A/wapMVTA use).
            if(cfg.ts_smooth>0.0f){
                // The ts-smooth history MUST match wapOutA's extent (the per-tick wapOutA→wapPrevOutA copy
                // uses that extent, and the warp samples binding 13 over the same domain). Size it WW/N×WH/N
                // too. warp_div==1 ⇒ WW_warp==WW (byte-identical).
                if(!img_create(WD,WW_warp,WH_warp,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapPrevOutA)){
                    std::printf("[ra] WAP prev-output image failed — disabling ts-smooth\n"); cfg.ts_smooth=0.f;
                }
            }
            // The dissidence-mask image (R8, mvw×mvh) — uploaded per pair from hDIS_a, held RO. Bound as
            // binding 6, so it must exist BEFORE wap_create writes the descriptor set. Created only with
            // --gme (the matte's two layers both come from gme; the parse gate already forbids --matte
            // without --gme). On any DIS-image failure we clear cfg.gme (no mask → no upload, no matte) and
            // bind the MV view as a harmless placeholder.
            if(cfg.gme){
                if(!img_create(WD,mvw,mvh,VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapDISA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 6) → CONCURRENT under xfer_on
                    std::printf("[ra] WAP DIS image failed — disabling gme\n"); cfg.gme=false;
                }
                // The backward (cur-anchored) dissidence image — same R8 mvw×mvh as wapDISA. Created only
                // when gme survived AND bidir is live (the bwd field exists). On failure we clear cfg.gme too
                // (no bwd mask → the dual-anchored matte can't run) — same degrade path.
                if(cfg.gme && use_bidir){
                    if(!img_create(WD,mvw,mvh,VK_FORMAT_R8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,wapDISBA,true,xfer_on?xfer_fams:nullptr,xfer_on?2u:0u)){   // upload-written + warp-read (binding 7) → CONCURRENT under xfer_on
                        std::printf("[ra] WAP DIS_bwd image failed — disabling gme\n"); cfg.gme=false;
                    }
                }
            }
            const std::vector<uint32_t> spvw(kWapWarpSpv.begin(),kWapWarpSpv.end());
            // binding 6 = wapDISA when gme is live, else the forward MV view as an unread placeholder (the
            // same completeness trick binding 5 uses for MV_bwd off-bidir). matte_on=0 off-matte → the
            // shader never samples binding 6, so the placeholder is harmless.
            const VkImageView dis_view = cfg.gme ? wapDISA.view : wMV.view;
            // binding 7 = wapDISBA when both gme AND bidir are live (the bwd mask exists), else a
            // placeholder. Prefer wapDISA.view (so binding 7's R8 format matches binding 6 — the sampler
            // is shared) when gme is on but bidir is off; else wMV.view (the binding-5/6 trick). matte
            // requires --bidir, so when matte is on this is always the real wapDISBA; the placeholder only
            // matters for gme-without-bidir (matte off → binding 7 is never sampled).
            const VkImageView disb_view = (cfg.gme && use_bidir) ? wapDISBA.view
                                        : (cfg.gme ? wapDISA.view : wMV.view);
            // binding 8 = wapPERA (R8) when it was created (the normal case — it is created unconditionally
            // with WAP above), else the MV view as a harmless unread placeholder (the binding-5/6/7 trick).
            // The shader reads it only when inertia_thresh>0.5 (a uniform push), and inertia_thresh is 0
            // whenever cfg.inertia is false (incl. the create-failure clear) — so the placeholder is never
            // sampled. wapPERA.view==VK_NULL_HANDLE only on a create failure (which already cleared
            // cfg.inertia); the ternary keeps the descriptor write valid in that case.
            const VkImageView per_view = wapPERA.view ? wapPERA.view : wMV.view;
            // binding 10 = wapC2A (RGBA16F second-best candidate) when it was created (use_ambig), else the
            // MV view as a harmless unread placeholder (the binding-5/6/7/8 completeness trick). The shader
            // reads it only when ambig_on>0.5 (a uniform push), and ambig_on is 0 whenever use_ambig is
            // false (incl. any create/bridge-failure clear above) — so the placeholder is never sampled.
            const VkImageView cand_view = wapC2A.view ? wapC2A.view : wMV.view;
            // binding 11 = wapFIELDA (R32_UINT contour field) when --afill/--bg-snap created it, else the
            // 1×1 r32ui placeholder (wapFIELDph). The shader reads it only when bg_snap_on>0.5 (a uniform
            // push), which the host sets only under cfg.bg_snap (and wapFIELDA then exists) — the placeholder
            // is never sampled. An r32ui storage binding needs a format-matching view, so (unlike binding
            // 10's float placeholder) the placeholder is a real 1×1 r32ui image.
            const VkImageView field_view = wapFIELDA.view ? wapFIELDA.view : wapFIELDph.view;
            // --vblend: binding 12 = wapMVTA (RG16F next-pair MV target) when it was created (cfg.vblend),
            // else the forward MV view as a harmless unread placeholder (the binding-5/6/7/8/10 completeness
            // trick). The shader reads it only when vblend_on>0.5 (a uniform push), and vblend_on is 0
            // whenever cfg.vblend is false (incl. any create-failure clear above) — so the placeholder (an
            // rg16f view, format-matching binding 12) is never sampled.
            const VkImageView mvt_view = wapMVTA.view ? wapMVTA.view : wMV.view;
            // --ts-smooth: binding 13 = wapPrevOutA (RGBA8 prev-output history) when it was created
            // (cfg.ts_smooth>0), else wCur.view as a harmless unread placeholder (the binding-5/10/12
            // completeness trick; an RGBA8 sampled view, format-matching binding 13). The shader reads it
            // only when ts_smooth>0 (a uniform push), and ts_smooth is 0 whenever cfg.ts_smooth is off (incl.
            // any create-failure clear above) — so the placeholder is never sampled.
            const VkImageView prevout_view = wapPrevOutA.view ? wapPrevOutA.view : wCur.view;
            // The mass counter SSBO — a 4-byte host-coherent counter imported on the WAP device (WD == A)
            // as the binding-9 STORAGE buffer. hfb discipline: the EMH import needs the POINTER aligned to
            // minImportedHostPointerAlignment AND the size a multiple of it (hbuf_import passes both raw to
            // vkCreateBuffer/vkAllocateMemory) — so the alloc is host_align-aligned and host_align-rounded
            // (4096 on NV; a 4-byte/64-align alloc fails the import). Only the first 4 bytes are ever
            // read/written. Failure → disable WAP (binding 9 must be a valid buffer for a complete descriptor
            // set, exactly like the image bindings above). The pointer doubles as the CPU read/reset window.
            const uint64_t mass_al=std::max<uint64_t>(WD.host_align,1u);
            // The warp-output readback buffer (--outdump N). hfb-rounded; failure just disables the dump
            // (diagnostic only, never kills WAP). --qdump ALSO needs this buffer (it reads back the live
            // wapOutA), so allocate it when EITHER flag is set.
            if(cfg.outdump_n>0 || cfg.qdump_n>0){
                const VkDeviceSize ob=(VkDeviceSize)WW*WH*4u;
                const VkDeviceSize obr=(ob+mass_al-1)/mass_al*mass_al;
                hostOutD=_aligned_malloc((size_t)obr,(size_t)mass_al);
                if(!hostOutD||!hbuf_import(WD,hostOutD,obr,hOutD_a,VK_BUFFER_USAGE_TRANSFER_DST_BIT)){
                    std::printf("[ra] outdump/qdump: live-output readback alloc/import failed — disabling\n");
                    cfg.outdump_n=0; cfg.qdump_n=0; }
                CreateDirectoryA("frames",nullptr);
            }
            // The TWO anchor readback buffers (wapPrevA=real N, wapCurA=real N+2). Allocated ONLY when
            // --qdump is set (zero cost when off). Same hfb-rounded alloc + import as hOutD_a; failure
            // disables qdump (diagnostic only, never kills WAP). The live-output buffer (hOutD_a) was
            // allocated above; if that failed it already cleared qdump_n, so this block is skipped too.
            if(cfg.qdump_n>0){
                const VkDeviceSize ob=(VkDeviceSize)WW*WH*4u;
                const VkDeviceSize obr=(ob+mass_al-1)/mass_al*mass_al;
                hostPrevD=_aligned_malloc((size_t)obr,(size_t)mass_al);
                hostCurD =_aligned_malloc((size_t)obr,(size_t)mass_al);
                if(!hostPrevD||!hbuf_import(WD,hostPrevD,obr,hPrevD_a,VK_BUFFER_USAGE_TRANSFER_DST_BIT)||
                   !hostCurD ||!hbuf_import(WD,hostCurD ,obr,hCurD_a ,VK_BUFFER_USAGE_TRANSFER_DST_BIT)){
                    std::printf("[ra] qdump: anchor readback alloc/import failed — qdump disabled\n"); cfg.qdump_n=0; }
                if(cfg.qdump_n>0) CreateDirectoryA(cfg.qdump_dir,nullptr);
            }
            const VkDeviceSize mass_sz=(sizeof(uint32_t)+mass_al-1)/mass_al*mass_al;
            hostMassPtr=_aligned_malloc((size_t)mass_sz,(size_t)mass_al);
            if(hostMassPtr){ *(uint32_t*)hostMassPtr=0u;
                // hMass_a is the host-coherent COPY DESTINATION (TRANSFER_DST), and devMass is the
                // DEVICE-LOCAL SSBO the shader atomicAdds into (STORAGE + TRANSFER_SRC for the per-dispatch
                // copy + TRANSFER_DST for the on-GPU fill reset). Both must succeed; either failure disables
                // WAP (binding 9 must be valid).
                if(!hbuf_import(WD,hostMassPtr,mass_sz,hMass_a,VK_BUFFER_USAGE_TRANSFER_DST_BIT)){
                    std::printf("[ra] WAP mass-counter copy-dest import failed — disabling warp-at-presenter\n"); use_wap=false; }
                else if(!dbuf_create(WD,mass_sz,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,devMass)){
                    std::printf("[ra] WAP mass-counter device-local alloc failed — disabling warp-at-presenter\n"); use_wap=false; }
            } else { std::printf("[ra] WAP mass-counter alloc failed — disabling warp-at-presenter\n"); use_wap=false; }
            if(use_wap && !wap_create(WD,wPrev.view,wCur.view,wMV.view,wSAD.view,wOut.view,wMVB.view,dis_view,disb_view,per_view,devMass.buf,cand_view,field_view,mvt_view,prevout_view,spvw,WP)){
                std::printf("[ra] WAP pipeline failed — disabling warp-at-presenter\n"); use_wap=false;
            } else if(use_wap){
                // Initial layouts: sampled inputs → SHADER_READ_ONLY (the per-pair upload
                // transitions DST→RO each time), output → GENERAL (the warp dispatch target).
                oneshot(WD,[&](VkCommandBuffer c){
                    img_barrier(c,wPrev.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wCur.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wMV.img,  VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wSAD.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wMVB → RO too. When bidir is off it stays cleared/unread (occl_thresh=0 gates the
                    // shader); when on, wap_upload transitions it DST→RO per pair like wMV.
                    img_barrier(c,wMVB.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(c,wOut.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                    // wapDISA → RO (the wap_upload transitions DST→RO per pair when gme is on; off-gme it is
                    // never created — this barrier only fires when the image exists).
                    if(cfg.gme) img_barrier(c,wapDISA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapDISBA → RO (same per-pair DST→RO upload contract; created only when gme AND bidir,
                    // so this barrier only fires when the bwd dissidence image exists).
                    if(cfg.gme&&use_bidir) img_barrier(c,wapDISBA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapPERA → RO. Created unconditionally with WAP (binding-8 completeness), so this fires
                    // whenever the image exists. The per-pair upload transitions DST→RO when use_inertia;
                    // off-inertia it stays this initial RO state and is never sampled.
                    if(wapPERA.img) img_barrier(c,wapPERA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapC2A → RO. Created only with use_ambig; the per-pair upload transitions DST→RO when
                    // use_ambig. When ambig is off it is never created — this fires only when the candidate
                    // image exists (and the binding is then placeholder-bound to wMV.view).
                    if(wapC2A.img) img_barrier(c,wapC2A.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapMVTA → RO. Created only with cfg.vblend; the per-pair upload transitions DST→RO when
                    // cfg.vblend. When vblend is off it is never created — this fires only when the MV-target
                    // image exists (and the binding is then placeholder-bound to wMV.view).
                    if(wapMVTA.img) img_barrier(c,wapMVTA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapPrevOutA → RO (the warp samples binding 13 in SHADER_READ_ONLY). Created only with
                    // cfg.ts_smooth>0; the per-tick copy-back transitions it RO→TRANSFER_DST→RO each tick.
                    // When ts-smooth is off it is never created — this fires only when the prev-output history
                    // image exists (and the binding is then placeholder-bound to wCur.view). The first tick
                    // samples this UNDEFINED-content-but-RO image (garbage history) — harmless: the copy-back
                    // populates it from tick 1's output, and ts_smooth-gated garbage masking is perceptual anyway.
                    if(wapPrevOutA.img) img_barrier(c,wapPrevOutA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
                    // wapFIELDA → GENERAL (the fill's AND the warp's imageLoad target; a STORAGE r32ui image
                    // read in GENERAL). The per-pair upload transitions DST→GENERAL each pair; this fires only
                    // when --afill/--bg-snap created the image. When neither did, the 1×1 r32ui placeholder (the
                    // bound binding-11 view) takes the GENERAL transition so the statically-used storage
                    // descriptor has a valid layout at dispatch (never sampled: bg_snap_on=0).
                    if(wapFIELDA.img)        img_barrier(c,wapFIELDA.img, VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_READ_BIT);
                    else if(wapFIELDph.img)  img_barrier(c,wapFIELDph.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_READ_BIT);
                });
                std::printf("[ra] warp-at-presenter: ACTIVE — F ships MV+SAD; A re-warps per tick at exact phase (warper=A)\n");
                // The field VISUALIZER pipeline (2 STORAGE images: wOut read-write in-place,
                // wapFIELDA readonly). Created after the warp pipeline; on failure clear cfg.afill (the per-pair
                // field upload + the fill dispatch both gate on it — graceful degrade, the WAP path keeps running
                // without the tint). cfg.afill is true here only if wapFIELDA + hFIELD_a both succeeded above.
                if(cfg.afill){
                    const std::vector<uint32_t> spvfill(kWapFillSpv.begin(),kWapFillSpv.end());
                    if(!fillpipe_create(WD,wOut.view,wapFIELDA.view,wMV.view,spvfill,fillPipeA)){
                        std::printf("[ra] WAP fill (--afill) pipeline failed — disabling the field visualizer\n");
                        fillpipe_destroy(WD,fillPipeA); cfg.afill=false;
                    } else {
                        std::printf("[ra] --afill: ACTIVE — A tints the iGPU contour band onto the present (strength=%.2f, edge_norm=%.3f, mv-gate=%.1fpx still-pixel)\n",cfg.afill_strength,cfg.afill_edge_norm,cfg.afill_mv_gate);
                    }
                }
                // One-time note — bidir doubles B's per-pair match+pyramid work (a second backward
                // record_optical_flow). At ≤60fps source the span machinery absorbs it; the fg-auto
                // per-pair EMA (t_fuse_ema) includes both passes (reported when --fg-factor auto).
                if(use_bidir) std::printf("[ra] bidir: ACTIVE — F records TWO flows/pair (fwd + bwd); ships MV_fwd+SAD+MV_bwd; occl=%.2f px (B pair match ~2x)\n",cfg.occl_thresh);
                // ── MV vector-median scratch + pipeline (A) ─────────────────────
                // ONE RG16F scratch (mvw×mvh), STORAGE (median write) | TRANSFER_SRC (copy back into
                // the WAP MV image after each pass). The pipeline samples a WAP MV image (RO) into the
                // scratch (GENERAL); two descriptor sets (fwd + bwd) share the one scratch. Disable on
                // any failure (the WAP path keeps running unfiltered — a graceful degrade, not a crash).
                // The pass runs for --mv-median OR --mv-guided (run_median_pass). The pipeline also
                // samples cur_real (wCur.view, binding 2) and carries a sim_thresh push: 0 = blind
                // median, >0 = color-weighted consensus. On any failure, BOTH flags fall back to
                // unfiltered (graceful degrade, not a crash).
                if(use_mv_median||use_mv_guided){
                    const std::vector<uint32_t> spvmd(kMvMedianSpv.begin(),kMvMedianSpv.end());
                    if(!img_create(WD,mvw,mvh,VK_FORMAT_R16G16_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,wapMVScratchA)
                       || !med_create(WD,wMV.view,wMVB.view,wCur.view,wapMVScratchA.view,/*with_mvb=*/use_bidir,spvmd,medPipe)){
                        std::printf("[ra] mv-median/guided pipeline failed — disabling the consensus pass\n");
                        med_destroy(WD,medPipe); img_destroy(WD,wapMVScratchA); use_mv_median=false; use_mv_guided=false;
                    } else {
                        // Scratch → GENERAL once (the median dispatch's storage target; the per-pass
                        // chain returns it to GENERAL after the copy-back read, mirroring the upload barriers).
                        oneshot(WD,[&](VkCommandBuffer c){
                            img_barrier(c,wapMVScratchA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                        });
                        if(use_mv_guided) std::printf("[ra] mv-guided: ACTIVE — color-weighted 3x3 consensus on the WAP MV field%s (sim=%.2f, contour-membership) + bilateral primary fetch in warp\n",use_bidir?"(s incl. bwd)":"",cfg.mv_sim);
                        else              std::printf("[ra] mv-median: ACTIVE — 3x3 component-wise vector-median on the WAP MV field%s (flat-content consensus)\n",use_bidir?"(s incl. bwd)":"");
                    }
                }
                // --dump is the pre-generated-grid BMP diagnostic; WAP synthesises on the fly with no
                // hostI buffers to dump → unsupported.
                if(cfg.dump_n>0){ std::printf("[ra] dump unsupported in warp-at-presenter v1\n"); cfg.dump_n=0; }
            }
        }
    }
}

// Made with my soul - Swately <3
