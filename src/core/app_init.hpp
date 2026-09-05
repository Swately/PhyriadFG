#pragma once
// PhyriadFG — the init-seq ownership structs (E1 of docs/planning/RESTRUCTURE_PLAN.md).
//
// These structs OWN the storage that used to be main()'s hoisted declaration block
// (main.cpp 502–844 pre-E1): every resource variable declared BEFORE any `goto done`
// so no jump crosses an initializer (the C++ goto rule), zero-initialized so the
// `done:` cleanup can null-check every member regardless of how far init got.
// main() instantiates them (zero-init `{}`), rebinds every original name with an
// `auto&`/`T&` alias, and the init sections / main loop / FgContext aggregate /
// teardown all stay textually unchanged.
//
// Grouping rule (CR2 of RESTRUCTURE_RISK_REGISTER.md): a struct groups the fields its
// init_* function FILLS; later functions receive the earlier structs as parameters, so
// a cross-section dependency is a visible parameter instead of a shared scope.
// All structs live HERE (not in the module headers) because fg_context.hpp already
// includes capture/warp_blend headers — a module header including flow/present types
// back would cycle. Same centralization precedent as fg_context.hpp itself.
//
// The comments on the fields are the original declaration-block comments, moved with
// their declarations.

#include <vulkan/vulkan.h>
#include <d3d11.h>
#include "core/device.hpp"          // VDev
#include "core/vk_util.hpp"         // Img / HBuf
#include "core/fg_context.hpp"      // kRawSlots (+ the capture/wgc/warp_blend/cli types)
#include "flow/flow.hpp"            // kGenRing, MvSmoothPipe, NvofaProvider, GmePipe, MedianPipe, OpticalFlowPipeline (transitively)
#include "present/present.hpp"      // UpPipe

// Ring-buffered real slots; 2 interp generations × up to kMaxInterp interps. The capture ring
// lets F consume captures IN ORDER (span 1) instead of jumping to the newest — jumping turns a
// long F iteration into a source skip → span-2 pair → 2× displacement → a walking tremble. The
// depth keeps spare overwrite margin beyond the in-order read window. The F→P interp-generation
// ring stays at 2, independent of the capture ring.
static constexpr int kCapSlots=32;   // compile-time MAX (the static handle-array size, ~negligible memory); the ACTIVE ring depth is the RUNTIME `cap_slots` (auto-sized at init from the source/flow/resolution regime, NOT the CPU). A too-shallow ring LAPS the reals at high source (prev_slot_safe = cur_c-prev_cseq < cap_slots-1 fails → freeze-at-cur every tick); a deep ring widens the real-validity window (the present's read-side gates real_valid/prev_slot_safe use cap_slots-1) so WAP interpolates B's pairs instead of degrading to show-reals+repeat. The present still selects the freshest pair.
// The in-order INGEST backlog cap is DECOUPLED from the ring depth. It governs LATENCY (how far behind newest F drains in order before jumping), NOT real-persistence — keep it small so latency stays low while the deep ring keeps the reals valid.
static constexpr int kIngestBacklog=3;
static constexpr int kCapSlotsMin=4;   // the minimum viable ring (kGenRing=3 + 1) AND the torn-read floor.
static_assert(kCapSlotsMin >= kIngestBacklog + 1, "torn-read safety: the in-order ingest write head and the present read slot must never collide; the non-collision margin (cap_slots - kIngestBacklog) >= 1 is guaranteed by cap_slots >= kCapSlotsMin >= kIngestBacklog+1.");

// ── Capture source (filled by init_capture_source — the pre-goto init head) ───────────
struct CaptureSrcInit {
    D3D d{};
    std::vector<OutInfo> pres_outputs;   // present candidates from EVERY adapter (DXGI enumeration)
#ifdef _MSC_VER
    WgcCtx* wgc_ctx=nullptr;             // WGC capture context (created by init_wgc_backend)
    HWND    wgc_target_hwnd=nullptr;     // set when --window is used (window-capture mode)
#endif
    bool want_dd=false; bool IS_HDR=false;
    VkFormat nat_vkfmt=VK_FORMAT_UNDEFINED; uint32_t nat_bpp=0; Route route=RT_NONE; const char* rdesc="";
    uint32_t NAT_W=0,NAT_H=0,WW=0,WH=0,UP_W=0,UP_H=0;
    uint32_t flow_div=1,WW_flow=0,WH_flow=0,warp_div=1,WW_warp=0,WH_warp=0;
    int cap_mon_hz=0; const char* cap_mon_src="";   // the ingest-scaling base (capture ring + WGC MinUpdateInterval)
};

// ── Vulkan instance + physical-device pick (filled by init_vk_pick) ─────────────────
struct VkPickInit {
    VkInstance inst=VK_NULL_HANDLE;
    VkPhysicalDevice pA=VK_NULL_HANDLE,pB=VK_NULL_HANDLE,pG=VK_NULL_HANDLE;
    // deviceLUIDs for the per-adapter GPU%% stat (PDH instance-name match) + the A/B device names
    // for the NVML utilization match (the PDH GPU-Engine LUID space is not the DXGI LUID on some rigs).
    LUID luidA{},luidB{},luidG{}; bool luidA_ok=false,luidB_ok=false,luidG_ok=false;
    std::string nvNameA,nvNameB;
    bool single_gpu=false;   // --force-single-gpu OR no 2nd discrete GPU (pB null)
};

// ── Devices + the derived feature gates (filled by the Devices section) ─────────────────────
struct DevicesInit {
    VDev A{},B{},G{};
    // use_upscale stays declared (the upscale pipes are still COMPILED) but is forced false at runtime
    // by the output-clock guard — the present is ALWAYS the surface bridge, and the bridge blit
    // (FILTER_LINEAR, work→monitor extent) already scales. The pipes are deliberately not deleted.
    bool have_igpu=false, use_upscale=false, use_igpu_convert=false;
    bool use_fwd_prestage=false; // --fwd-prestage: derived gate (cfg.fwd_prestage && use_wap && use_igpu_convert); assigned after use_wap. Declared here (non-const, NOT skipped by goto done) so its init is never bypassed.
    bool use_wap=false;          // warp-at-presenter active (re-warps on A, the bridge owner)
    bool use_bidir=false;        // bidirectional flow + occlusion classification (WAP-only; needs use_wap)
    bool use_fill_div=false;     // divergence-directed disocclusion pick (bidir-only; needs use_bidir)
    bool use_rescue=false;       // candidate-rescue (WAP-only; needs use_wap + commit armed)
    bool use_mv_median=false;    // 3x3 MV vector-median in wap_upload (WAP-only; needs use_wap)
    bool use_mv_guided=false;    // color-guided MV (consensus + bilateral fetch; WAP-only; needs use_wap)
    bool use_gme=false;          // frame-holon global affine model (fit on F + rescue/fill-div in warp; WAP-only)
    bool use_matte=false;        // fluid-matte boundary-first compositing (WAP-only; needs use_gme → wapDISA + model)
    bool use_inertia=false;      // inertia prior (F-side persistence counter + 3 WAP warp gates; WAP-only; needs use_wap)
    bool use_objects=false;      // object-holon clustering + motion-inheritance repair (F-side; needs use_gme for the dissidence mask)
    bool use_ambig=false;        // ambiguity channel — second-best candidate arbitration of SAD ties (WAP warp; needs use_gme for the model referee)
    bool use_commit_default=false; // the commit-default flip — WARP IS THE DEFAULT in the warp-vs-blend selection (WAP-level, matte-independent; needs use_wap)
    bool use_onepos=false;       // the one-position collapse — warp_result is single-position where the two samples disagree (WAP-level, matte-independent; needs use_wap)
    bool use_memory=false;       // scene-holon silhouette memory (advect+merge+refresh the dissidence prior; needs use_objects)
    VkImageUsageFlags bframe_use=0, gsrc_use=0;   // assigned after use_igpu_convert is known (the Images section)
};

// ── Host bridge (filled by the Host-bridge section: the aligned host allocations + every
//    per-device EMH import, incl. the WAP MV/SAD/candidate/dissidence/persistence bridges and
//    the --ingest-async raw ring) ──────────────────────────────────────────────────────────────
// Host bridge — three disjoint aligned allocations (hostR/hostI de-aliased so the
// real frame survives the interp FG write):
//   hostR (WW×WH×4)      : the REAL converted frame. A writes it (hR_a, cmdA);
//                          B reads it (hR_b) to upload Bframe[cur]; G reads it
//                          (hR_g) to upscale the real present; A reads it (hR_a,
//                          no-upscale path) to present the real frame.
//   hostI (WW×WH×4)      : the INTERP frame. B writes Cinterp into it (hI_b);
//                          G reads it (hI_g) to upscale an interp present; A reads
//                          it (hI_a, no-upscale path) to present an interp frame.
//   hostG (UP_W×UP_H×4)  : G writes upscale output (hGout); A reads (hApres) for present.
// iGPU-convert additions:
//   hostA (NAT_W×NAT_H×bpp): Astage capture buffer, aligned for EMH import on G.
//   hostRP (WW×WH×3)     : packed RGB8 output. G writes (hRP_g) after convert+pack;
//                          B reads (hRP_b) to ingest packed frame; G also reads (hRP_g)
//                          for real-frame unpack before upscale.
struct HostBridgeInit {
    int cap_slots=kCapSlotsMin;   // the ACTIVE ring depth (<= kCapSlots max). Auto-sized at init (just before the host-bridge alloc) from the source/flow/resolution regime, or --cap-slots N override. All ring LOOPS + INDEX MATH + validity gates use this runtime value; the static arrays are sized to kCapSlots (the max).
    void* hostR[kCapSlots]={}; void* hostI[kGenRing][kMaxInterp]={}; void* hostG=nullptr;
    void* hostA=nullptr; void* hostRP[kCapSlots]={};          // hostRP × kCapSlots
    void* hostFIELD[kCapSlots]={};                       // iGPU contour-field host bridge (CPU-readable for the verify gate)
    HBuf hR_a[kCapSlots]{},hR_b[kCapSlots]{},hR_g[kCapSlots]{}; // real-slot imports (A/B/G)
    HBuf hI_a[kGenRing][kMaxInterp]{},hI_b[kGenRing][kMaxInterp]{},hI_g[kGenRing][kMaxInterp]{};  // interp gen×k imports (A/B/G)
    HBuf hGout{},hApres{};
    HBuf hRP_g[kCapSlots]{},hRP_b[kCapSlots]{};   // packed (× kCapSlots)
    HBuf hFIELD_g[kCapSlots]{};                   // iGPU contour-field import (G writer)
    HBuf hFIELD_a[kCapSlots]{};                   // iGPU contour-field import (A reader — uploads it to wapFIELDA; --afill only)
    HBuf hRP_b_dev[kCapSlots]{};          // device-local packed copy (× kCapSlots)
    HBuf Astage_g{};              // Astage on G as SSBO
    HBuf Astage{};                // the A-side EMH import of hostA (the capture staging buffer)
    // INGEST-ASYNC (--ingest-async, default OFF): the RAW host-buffer ring between the acquire thread
    // and the convert worker. kRawSlots clones of the Astage host buffer, each imported on A
    // (TRANSFER_SRC = the A-path convert src) AND, when use_igpu_convert, on G (STORAGE = the iGPU-path
    // convert src). Allocated ONLY when cfg.ingest_async; null/inert otherwise (byte-identical off).
    void* raw_host[kRawSlots]={};
    HBuf  raw_astage_a[kRawSlots]{};   // A imports (TRANSFER_SRC)
    HBuf  raw_astage_g[kRawSlots]{};   // G imports (STORAGE) — only when use_igpu_convert
    double raw_tcap[kRawSlots]={};     // per-slot capture timestamp (ms), carried to c_slots[s].t_cap_ms
    // WGC async lat-trace carry (R6, WGC_INGEST_ASYNC_PLAN.md): the per-slot submit/compose stamps
    // ride the raw ring to the worker (which folds them into lt_copy_us/lt_compose_us). Always 0 on
    // DDA (its callback has no such stamps) → the worker's >0 guards make them inert there.
    double raw_lt_submit[kRawSlots]={};   // now_ms at the WGC callback CopyResource submit (ms)
    double raw_lt_compose[kRawSlots]={};  // WGC compose→callback delta (µs)
    // Warp-at-presenter host bridges — per generation, F copies B's MV+SAD field out to these
    // (~130KB each at 1080p, RG16F at WW/8×WH/8); A imports them (hMV_a/hSAD_a) to upload
    // into its sampled MV/SAD images per pair-advance. Only allocated when WAP is active.
    void* hostMV[kGenRing]={}, *hostSAD[kGenRing]={};
    HBuf hMV_b[kGenRing]{},hSAD_b[kGenRing]{};
    // The second-best CANDIDATE field host bridge (RGBA16F = 8 bytes/texel, same mvw×mvh as hostMV —
    // NOT 4: the candidate carries xy=runner-up MV, z=runner-up SAD, w=0). Only allocated when
    // use_ambig (cfg.ambig && use_gme). F copies B's cand_image() out into hC2_b (TRANSFER_DST, same
    // FWD-pass copy as hMV_b/hSAD_b); A uploads it into wapC2A (binding 10) per pair. Same hfb
    // align-rounded EMH discipline as hostMV (POINTER and SIZE host_align-rounded), but the per-texel
    // size is 8 not 4 so its own field byte-count (cfb) is mvw×mvh×8.
    void* hostC2[kGenRing]={};
    HBuf hC2_b[kGenRing]{};
    // Backward-flow MV host bridges (RG16F, same mvw×mvh as hostMV). Only allocated when --bidir is
    // active (use_bidir). F copies B's backward MV into hMVB_b (TRANSFER_DST); A uploads it into
    // wapMVBA (TRANSFER_SRC). SAD-bwd is not shipped.
    void* hostMVB[kGenRing]={};
    HBuf hMVB_b[kGenRing]{};
    // The frame-holon's per-block dissidence mask (uint8 = min(255, round(16·r)) where r = |mv_block
    // − model(x,y)| in px). Computed on the CPU in F (single-threaded, the fit owns it), so the bridge
    // is a plain _aligned_malloc host buffer (NO B-side GPU write — F writes it via memcpy semantics)
    // imported on A for the per-pair upload into wapDISA. mvw×mvh bytes, hfb align-rounded (the EMH
    // discipline). Only allocated when use_gme.
    void* hostDIS[kGenRing]={};
    HBuf hDIS_a[kGenRing]{};
    // The BACKWARD (cur-anchored) dissidence mask — the SAME uint8 = min(255, round(16·r)) quantization,
    // but r = |mv_bwd − model_bwd| computed by the SAME gme_fit_affine run on the bwd MV field (hostMVB).
    // It carries the cur-anchored object silhouette; advected backward in the warp it covers the LEADING
    // edge the prev-anchored mask arrives late at. Same dfb discipline as hostDIS: a plain _aligned_malloc
    // host buffer (F writes it on the CPU after the bwd fit), imported on A for the per-pair upload into
    // wapDISBA. Only allocated when use_gme (the matte gate already forbids --matte without --bidir, so
    // the bwd field is always present when gme is on with matte).
    void* hostDISB[kGenRing]={};
    HBuf hDISB_a[kGenRing]{};
    // The inertia prior's per-block persistence bridge — R8 (1 byte/MV-block, the uint8 counter mapped
    // straight to the [0,255] R8 store). The SOURCE state is a SINGLE F-local array accumulated across
    // pairs (NOT per-gen — it integrates motion history over time); F memcpy's that continuous array into
    // hostPER[f_gen] right after the persistence update, and A imports it (read-side, TRANSFER_SRC upload
    // into wapPERA). Same R8 dfb discipline + A-side-only import as hostDIS (F writes it on the CPU; no
    // B-side GPU write). Only allocated when cfg.inertia. The per-gen bridge is the upload vehicle; the
    // continuity lives in the F-local persist[] array (declared in run_flow).
    void* hostPER[kGenRing]={};
    HBuf hPER_a[kGenRing]{};
    HBuf hMV_a[kGenRing]{}, hSAD_a[kGenRing]{};   // A-side imports of hostMV/hostSAD (surface WAP)
    HBuf hMVB_a[kGenRing]{};                       // A-side import of hostMVB (backward MV)
    HBuf hC2_a[kGenRing]{};                        // A-side import of hostC2 (second-best candidate, RGBA16F)
    uint32_t pres_w=0, pres_h=0;     // =WW,=WH at main() (they referenced pre-block locals); finalised once use_upscale is known (host bridge section)
};

// ── gme-gpu: the device-B side of the dissidence bridges + the model readback ────────────────
// When --gme-gpu the dis-mask is GPU-PRODUCED on B (gme_dissidence.comp atomicOrs into the host
// bridge directly) instead of CPU-written. So hostDIS/hostDISB get a B-side STORAGE import (hDIS_b/
// hDISB_b) — the dissidence shader's binding-2 target. The A-side imports (hDIS_a/hDISB_a) stay (A
// still uploads the mask into wapDISA/wapDISBA). hostGmeM[gen] is the 6-float model readback: B
// copies the device-local Model SSBO into it (TRANSFER_DST) after the solve passes; F reads it in
// consume_wap. All only allocated when use_gme_gpu.
// NOTE fwd vs bwd model use SEPARATE readback buffers: cmdB_bwd is submitted async (fB2) BEFORE the CPU
// reads the fwd model, so a shared buffer would race (bwd model could clobber the fwd before F reads it).
struct GmeGpuInit {
    GmePipe gmePipe{};
    bool use_gme_gpu=false;
    HBuf hDIS_b[kGenRing]{}, hDISB_b[kGenRing]{};       // B-side STORAGE imports of hostDIS/hostDISB
    void* hostGmeM[kGenRing]={}, *hostGmeMB[kGenRing]={};// 6-float model readback (host-coherent): fwd / bwd
    HBuf hGmeM_b[kGenRing]{}, hGmeMB_b[kGenRing]{};      // B imports (vkCmdCopyBuffer dest): fwd / bwd
};

// ── Images (filled by the Images section; dxgi_stage2 by the --ingest-async block) ───────────
struct ImagesInit {
    ID3D11Texture2D* dxgi_stage=nullptr;
    ID3D11Texture2D* dxgi_stage2=nullptr;   // INGEST-ASYNC: 2nd DDA staging texture (readback double-buffer); created only when cfg.ingest_async
    Img Anative{},Awork{},Bframe[2]{},Cinterp{},Gsrc{},Gdst{};
    // Per-pair flow-input downscale scratch (only allocated when flow_div>1). Two WW_flow×WH_flow
    // RGBA8 DEVICE_LOCAL images (SAMPLED so the OFP can read them as a_view/b_view; TRANSFER_DST as the
    // vkCmdBlitImage destination). The F thread blits Bframe[prv]/Bframe[cur] (full-res) into these with a
    // LINEAR filter before record_optical_flow, so the level-0 match input matches the smaller MV grid.
    Img Bflow[2]{};
    Img Apresent{};                      // present source on A: pres_w×pres_h RGBA8
    // Per-interp B-VRAM staging buffers. The set build copies Cinterp→sbI[k] VRAM→VRAM on B.q
    // (fast, ~0.5ms), then the slow x4 leg sbI[k]→hI_b[gen][k] runs on B.q2 (the second queue) with
    // its OWN fence tfb[k], NOT waited inline — overlapped with the next warp. CPU fence-submit
    // ordering (q2 submit AFTER the B.q fB wait) makes sbI[k] visible to q2 without semaphores. Only
    // active when B.q2 is a real second queue; B.q2==B.q falls back to the serial Cinterp→hI_b path.
    HBuf sbI[kMaxInterp]{};
    bool b_q2_split=false;
};

// ── Convert pipeline (A: native→RGBA8 work) ──────────────────────────────────────────────────
struct ConvertInit {
    VkSampler cvSamp=VK_NULL_HANDLE; VkDescriptorSetLayout cvDsl=VK_NULL_HANDLE;
    VkPipelineLayout cvLayout=VK_NULL_HANDLE; VkPipeline cvPipe=VK_NULL_HANDLE;
    VkDescriptorPool cvPool=VK_NULL_HANDLE; VkDescriptorSet cvSet=VK_NULL_HANDLE;
};

// ── The flow pipelines (OFP-B + NVOFA + MV-smooth + the primary-FG OFP-A) ────────────────────
struct FlowPipesInit {
    // Temporal MV smoothing (B-path only; app-local EMA on ofp's MV field).
    MvSmoothPipe mvsm{}; Img mv_prev{}; bool use_mv_smooth=false;
    // OpticalFlowPipeline (B).
    phyriad::render::vulkan::OpticalFlowPipeline ofp{};
    // --nvofa: the hardware OFA flow provider (replaces ofp.record_optical_flow's match when armed).
    // use_nvofa is set TRUE only after a SUCCESSFUL nvofa_create (gated on --nvofa && single_gpu &&
    // A.ofaQueue). When false EVERYTHING below uses the classical ofp path (byte-identical).
    NvofaProvider nvofa{};
    bool use_nvofa=false;
    // Primary-FG path: optional OFP on A when A has spare cycles.
    // Zero PCIe transfer cost: Awork is already on A's VRAM after the convert pass.
    phyriad::render::vulkan::OpticalFlowPipeline ofpA{};
    Img AframeA[2]{}, CinterpA{};   // ping-pong frames + interp output on A
    bool pfg_enabled=false;         // true once ofpA.init() succeeds
};

// ── iGPU-convert pipelines (one set per real slot) + the (forced-off) upscale pipe ───────────
struct IgpuPipesInit {
    // Upscale pipeline (G) — kept compiled but forced off (the bridge blit scales).
    UpPipe upPipe{};
    ConvPackPipe cpPipe[kCapSlots]{};  // G: igpu_convert_pack per real slot (Astage_g → hRP_g[s])
    FieldPipe    fpipe[kCapSlots]{};    // iGPU contour-field pipe per real slot (hR_g → hFIELD_g)
    UnpackPipe   ubPipe[kCapSlots]{};  // B: unpack_packed per real slot (hRP_b[s] → Bframe[0/1])
    UnpackPipe   ugPipe[kCapSlots]{};  // G: unpack_packed per real slot (hRP_g[s] → Gsrc)
};

// ── Command buffers + fences (filled by the Cmd/sync section) ────────────────────────────────
struct CmdSyncInit {
    // Command buffers + fences. No present semaphores (no swapchain present).
    VkCommandBuffer cmdA=VK_NULL_HANDLE,cmdB=VK_NULL_HANDLE,cmdG=VK_NULL_HANDLE;
    VkCommandPool a_cpool_sg=VK_NULL_HANDLE,a_fpool_sg=VK_NULL_HANDLE;   // single-GPU: dedicated C(convert)/F(flow) command pools on A — command pools are externally-synchronized (one recording thread each); avoids the validation MultipleThreads-Write/segfault.
    VkCommandBuffer cmdGP=VK_NULL_HANDLE;    // P-thread G cmd buffer (upscale path — forced off)
    VkCommandBuffer cmdA_fg=VK_NULL_HANDLE;  // F-thread pfg OFP on A
    // A SECOND B cmd buffer for the bwd pass — the overlap submits bwd (cmdB_bwd/fB2) and then runs
    // the fwd CPU fit while it matches on the GPU. cmdB cannot be re-recorded while its fwd submit is
    // conceptually still the "owner" of fB; a distinct buffer+fence lets the bwd record proceed in
    // flight without resetting an in-use buffer. Allocated from B.pool when bidir is live (the only
    // consumer); shares B.q (externally synchronized — F is its only submitter).
    VkCommandBuffer cmdB_bwd=VK_NULL_HANDLE;  // F-thread bwd-pass cmd buffer (B.pool)
    // The FORWARD-pass ping-pong. Two cmd buffers + two fences (depth-2 frames-in-flight) let pair N's
    // fwd flow be submitted NO-WAIT to cmdB_fwd[N&1]/fB_fwd[N&1] while pair N-1's fwd (cmdB_fwd[(N-1)&1]/
    // fB_fwd[(N-1)&1]) is consumed by the CPU tail — a distinct buffer+fence per parity so neither resets
    // the other while it is conceptually in flight. Allocated from B.pool (F is B.q's only submitter,
    // externally synchronized) ONLY when use_wap && cfg.fwd_pipeline (the opt-in path; idle otherwise →
    // the serial path uses cmdB/fB, byte-unchanged).
    VkCommandBuffer cmdB_fwd[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    VkFence         fB_fwd[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    // --fwd-prestage: the prestage ping-pong. Two cmd buffers + two fences (depth-2 frames-in-flight)
    // hold ONLY the hRP_b[s]->hRP_b_dev[s] device copy, submitted NO-WAIT on the F flow lane (A.q2)
    // BEFORE cmdF so the copy overlaps the flow record + the ring-guard spin instead of sitting in
    // front of the blocking flow submit. Distinct buffer+fence per g_seq parity so pair N's prestage
    // does not reset the buffer/fence pair N-1's prestage may still be draining. Allocated from the
    // SAME F flow pool as cmdB (a_fpool_sg under single_gpu, else FD.pool) ONLY when cfg.fwd_prestage
    // && use_wap && use_igpu_convert (the opt-in path; idle otherwise → the serial path records the
    // copy INLINE in cmdF, byte-unchanged).
    VkCommandBuffer cmdF_pre[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    VkFence         fF_pre[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    VkCommandBuffer cmdB2[kMaxInterp]={};   // F's overlapped interp copy-outs on B.q2 (recorded from B.pool2)
    VkFence tfb[kMaxInterp]={};             // their per-slot fences
    VkFence fA=VK_NULL_HANDLE,fB=VK_NULL_HANDLE,fG=VK_NULL_HANDLE;
    VkFence fGP=VK_NULL_HANDLE;              // upscale fence (forced-off path)
    VkFence fA_fg=VK_NULL_HANDLE;            // F-thread pfg fence
    VkFence fB2=VK_NULL_HANDLE;             // bwd-submit fence (must not reset fB in flight)
};

// ── PresentSurface bridge — the producer side (D3D11 shared texture + VK-A import), the
//    --async-present second slot, and slot-0's dedicated async cmd/fence ─────────────────────
// THE present path. The present pillar owns the panel (dcomp-ct + WDA + its message pump). The app
// produces each frame into a D3D11 SHARED texture (BGRA8, present-monitor extent) on adapter A —
// the panel owner / 4090 — and hands the NT handle to surface.submit() (one CopyResource +
// Present(0)). VK-A imports that texture as a B8G8R8A8 image (the present-source blit dst → a channel
// reinterpretation). Sync: VK_KHR_win32_keyed_mutex if A exposes it (NVIDIA does), else a CPU fence +
// no-KM texture. The surface itself is created IN P's setup (the threading contract: create()+submit()
// on the SAME thread). bridge_* are the producer side (D3D11+VK), allocated before goto.
struct BridgeInit {
    uint32_t bridge_w=0, bridge_h=0;             // present-monitor extent (= surface backbuffer)
    ID3D11Texture2D* bridge_tex=nullptr;         // D3D11 shared producer texture (on d.dev = A/4090)
    IDXGIKeyedMutex* bridge_km_d3d=nullptr;      // its keyed mutex (D3D11 side; null if no-KM path)
    HANDLE           bridge_nt=nullptr;          // CreateSharedHandle NT handle → surface.submit()
    Img   bridge_img{};                          // VK-A image aliasing bridge_tex (B8G8R8A8)
    VkDeviceMemory bridge_mem=VK_NULL_HANDLE;    // imported memory backing bridge_img
    VkCommandBuffer cmdBridge=VK_NULL_HANDLE;    // A's bridge-blit/warp cmd buffer (P thread)
    VkFence fBridge=VK_NULL_HANDLE;              // bridge-blit fence (CPU ordering before submit())
    bool   bridge_use_km=false;                  // keyed-mutex sync path shipped (vs CPU fence)
    // --async-present: the SECOND bridge slot. Mirrors slot-0 EXACTLY (same extent / desc / shared NT
    // handle / VK import / cmd buffer / fence). Allocated ONLY when cfg.async_present; these members
    // init to null/VK_NULL_HANDLE so they always exist for the teardown guards regardless of the flag.
    // With async off they stay null and are never touched → byte-identical. bridge_w/bridge_h are
    // shared (the second slot is the SAME present-monitor extent).
    ID3D11Texture2D* bridge_tex1=nullptr;        // slot-1 D3D11 shared producer texture (on d.dev = A/4090)
    IDXGIKeyedMutex* bridge_km_d3d1=nullptr;     // slot-1 keyed mutex (null if no-KM path)
    HANDLE           bridge_nt1=nullptr;         // slot-1 CreateSharedHandle NT handle → surface.submit()
    Img   bridge_img1{};                         // slot-1 VK-A image aliasing bridge_tex1 (B8G8R8A8)
    VkDeviceMemory bridge_mem1=VK_NULL_HANDLE;   // slot-1 imported memory backing bridge_img1
    VkCommandBuffer cmdBridge1=VK_NULL_HANDLE;   // slot-1 A's bridge-blit/warp cmd buffer (P thread)
    VkFence fBridge1=VK_NULL_HANDLE;             // slot-1 bridge-blit fence (non-blocking poll target)
    // Slot-0's async warp needs its OWN cmd buffer + fence — it must NOT record into the shared
    // cmdBridge/fBridge, because the per-pair wap_upload resets+resubmits THOSE, and the async
    // (non-blocking) warp submit leaves them in flight → resetting an in-flight cmd buffer/fence = a
    // GPU fault. Slot 0 keeps the shared present TEXTURE (bridge_img/nt/mem); only its cmd+fence become
    // dedicated.
    VkCommandBuffer cmdBridgeA0=VK_NULL_HANDLE;  // slot-0 DEDICATED async cmd buffer (allocated only when async on)
    VkFence fBridgeA0=VK_NULL_HANDLE;            // slot-0 DEDICATED async fence (non-blocking poll target)
};

// ── WAP-on-A (filled by the warp-at-presenter section: the A-local WAP pipeline, its sampled
//    inputs, the upload-xfer machinery, the mass counter and the dump readbacks) ──────────────
struct WapInit {
    // --upload-xfer: PING-PONG upload command buffers on A.poolT (the transfer queue's pool) + the two
    // P-thread-local monotonic timeline counters. cmdUpload[2] lets a new pair-advance upload record into
    // the FREE slot while the prior upload may still execute on A.qT (never reset in flight). uslot =
    // round-robin index; uslot_val[s] = the semUpTL value signaled when slot s was last submitted (0 =
    // never used) — the non-blocking reuse guard polls semUpTL against it. xfer_U/xfer_W are the upload /
    // warp counters: each upload SIGNALS semUpTL=++xfer_U, each warp SIGNALS semWarpTL=++xfer_W; the warp
    // WAITS semUpTL>=xfer_U (RAW) and the next upload WAITS semWarpTL>=xfer_W (WAR back-edge). Both waits
    // are BACKWARD-looking (prior submits) → no cycle → no deadlock. Allocated once at init from A.poolT
    // (zero-alloc hot path); never touched when xfer_on is false (byte-identical).
    VkCommandBuffer cmdUpload[2]={VK_NULL_HANDLE,VK_NULL_HANDLE};
    uint64_t uslot_val[2]={0,0}; int uslot=0;
    uint64_t xfer_U=0, xfer_W=0;
    // --upload-xfer: the SINGLE runtime gate + the CONCURRENT family pair, at this shared scope so they
    // are visible to BOTH the wap-image creation block AND the wap_upload / wap_warp_present lambdas in
    // the present loop. ASSIGNED once device A exists (its A.qT/A.poolT come from vdev_create). OFF here
    // → all the xfer paths are inert and the code is byte-identical.
    bool xfer_on=false; uint32_t xfer_fams[2]={0,0};
    // WAP-on-A: the warp runs on A. A-local WAP pipeline + its sampled inputs (two pair reals, MV, SAD)
    // and the rgba8 warp output, plus A-side MV/SAD host bridges (EMH align-rounded size). Warp writes
    // wapOutA; a blit (rgba8→bgra8 channel reinterpretation) lands it in bridge_img.
    WapPipe wapPipeA{};
    FillPipe fillPipeA{};   // the field VISUALIZER pipe (wapOutA + wapFIELDA storage images; --afill only)
    Img wapPrevA{},wapCurA{},wapMVA{},wapSADA{},wapOutA{};
    Img wapFIELDA{};        // A-side iGPU contour field image (R32_UINT, full-res; --afill OR --bg-snap)
    Img wapFIELDph{};       // 1×1 r32ui binding-11 placeholder when neither --afill nor --bg-snap owns wapFIELDA (never sampled)
    Img wapMVBA{};                                // A-side backward-MV sampled image (binding 5)
    Img wapC2A{};                                 // A-side second-best CANDIDATE sampled image (RGBA16F,
                                                  // mvw×mvh). Created with use_ambig; uploaded from hC2_a per pair;
                                                  // bound as warp binding 10 (u_candidates). Placeholder-bound to
                                                  // wMV.view when ambig is off (the completeness trick) — never sampled.
    Img wapMVTA{};                                // --vblend: A-side NEXT-pair forward-MV TARGET image
                                                  // (RG16F, mvw×mvh — SAME format/size as wapMVA). Created with
                                                  // cfg.vblend; uploaded from hMV_a[target_gen] (the next-fresher
                                                  // generation) per pair; bound as warp binding 12 (u_mv_target).
                                                  // Placeholder-bound to wMV.view when vblend is off (the binding-5/10
                                                  // completeness trick) — never sampled (vblend_on=0 gates the read).
    Img wapPrevOutA{};                            // --ts-smooth: A-side PREVIOUS final-output history image
                                                  // (RGBA8, full-res WW×WH — SAME format/size as wapOutA, mirrored).
                                                  // Created with cfg.ts_smooth>0; the host copies wapOutA → wapPrevOutA
                                                  // after each tick's blit; bound as warp binding 13 (u_prev_out).
                                                  // Placeholder-bound to wCur.view when ts_smooth is off (the binding-
                                                  // 5/10/12 completeness trick) — never sampled (ts_smooth=0 gates the read).
    MedianPipe medPipe{};                          // 3x3 vector-median on the WAP MV field(s)
    Img wapMVScratchA{};                           // ONE RG16F scratch (mvw×mvh) — median dst
    Img wapDISA{};                                  // A-side R8 dissidence mask (mvw×mvh). Uploaded
                                                    // from hDIS_a per pair; bound as warp binding 6.
    Img wapDISBA{};                                 // A-side R8 BACKWARD (cur-anchored) dissidence
                                                    // mask (mvw×mvh). Uploaded from hDISB_a per pair; bound as
                                                    // warp binding 7 (u_dissidence_bwd) for the dual-anchored matte.
    Img wapPERA{};                                  // A-side R8 inertia persistence mask (mvw×mvh).
                                                    // Created unconditionally with WAP (cheap; binding 8 must
                                                    // always have a valid R8 view); uploaded from hPER_a per
                                                    // pair only when use_inertia. Bound as warp binding 8
                                                    // (u_persistence); read only when inertia_thresh>0.
    // The presented-matte-mass counter — a host-visible 4-byte STORAGE buffer bound as warp binding 9.
    // The warp atomicAdds the per-workgroup OBJECT-pixel count into it (gated on matte_on>0.5). The SSBO
    // the shader atomicAdds into is a DEVICE-LOCAL buffer (devMass, VRAM) so the GPU atomic does not
    // cross PCIe per workgroup. The per-dispatch result is reset on-GPU (vkCmdFillBuffer) and copied to
    // the host-imported buffer (vkCmdCopyBuffer) inside cmdBridge after the dispatch, so P reads
    // hostMassPtr AFTER the fBridge fence wait — no extra submit, no stall. hMass_a is the COPY DEST.
    void* hostMassPtr=nullptr;                       // the aligned CPU alloc (the host-coherent COPY dest)
    HBuf  hMass_a{};                                 // the A-device host import (vkCmdCopyBuffer destination)
    HBuf  devMass{};                                 // the device-local SSBO (binding 9; VRAM atomicAdd target)
    // The warp-output readback buffer (--outdump N). hfb discipline.
    void* hostOutD=nullptr; HBuf hOutD_a{};
    // --qdump <dir> N: TWO MORE readback buffers — the two warp anchors (wapPrevA=real N, wapCurA=real
    // N+2). The live wapOutA reuses the --outdump buffers (hostOutD/hOutD_a, allocated when EITHER
    // --outdump OR --qdump is set). All gated on qdump_n>0 → zero cost (no alloc) when OFF.
    void* hostPrevD=nullptr; HBuf hPrevD_a{};
    void* hostCurD =nullptr; HBuf hCurD_a{};
    // --qdump+ (2026-09-04): the POST-consensus MV readback. The 3x3 vector-median pass rewrites
    // wapMVA on the GPU after the host field was uploaded, so hostMV is the field BEFORE it; this
    // is the field the warp actually sampled. mvw*mvh*4 (RG16F), allocated only under --qdump.
    void* hostMV1 =nullptr; HBuf hMV1_a{};
};
// ── The init functions (E1) — each moved verbatim from main.cpp's init-seq into its module;
//    each returns false where its section did `goto done` (main() converts back to the jump).
struct Config;   // cli/cli.hpp (already included transitively)
bool init_bridge_slots(Config& cfg, D3D& d, const RECT& pc, DevicesInit& o_dev, BridgeInit& o_br);   // present/present_init.cpp
void init_gme_finalize(Config& cfg, DevicesInit& o_dev, FlowPipesInit& o_flow, GmeGpuInit& o_gme);   // flow/flow_init.cpp
bool init_convert_pipe(DevicesInit& o_dev, ImagesInit& o_img, ConvertInit& o_cv);                    // capture/capture_init.cpp
int  init_capture_source(Config& cfg, CaptureSrcInit& o_cap);                                       // capture/capture_init.cpp (-1 = continue; >=0 = exit code)
int  init_vk_pick(Config& cfg, D3D& d, VkPickInit& o_pick);                                          // core/core_init.cpp (-1 = continue; >=0 = exit code)
bool init_devices(Config& cfg, VkPhysicalDevice pA, VkPhysicalDevice pB, VkPhysicalDevice pG,
                  bool single_gpu, bool want_pfg, bool IS_HDR, uint32_t NAT_W, uint32_t NAT_H,
                  uint32_t WW, uint32_t WH, Route route, DevicesInit& o_dev);                        // core/core_init.cpp
bool init_host_bridge(Config& cfg, D3D& d, bool single_gpu, bool want_pfg,
                      uint32_t NAT_W, uint32_t NAT_H, uint32_t nat_bpp,
                      uint32_t WW, uint32_t WH, uint32_t UP_W, uint32_t UP_H,
                      uint32_t WW_flow, uint32_t WH_flow, int cap_mon_hz,
                      DevicesInit& o_dev, VDev& FD, GmeGpuInit& o_gme, ImagesInit& o_img,
                      HostBridgeInit& o_host);                                                      // core/core_init.cpp
bool init_images(Config& cfg, D3D& d, uint32_t NAT_W, uint32_t NAT_H, VkFormat nat_vkfmt,
                 uint32_t WW, uint32_t WH, uint32_t WW_flow, uint32_t WH_flow, uint32_t flow_div,
                 uint32_t UP_W, uint32_t UP_H, bool want_pfg,
                 DevicesInit& o_dev, VDev& FD, HostBridgeInit& o_host, FlowPipesInit& o_flow, ImagesInit& o_img);   // core/core_init.cpp
#ifdef _MSC_VER
bool init_wgc_backend(Config& cfg, D3D& d, uint32_t NAT_W, uint32_t NAT_H, int cap_mon_hz,
                      HWND wgc_target_hwnd, ImagesInit& o_img, WgcCtx*& wgc_ctx);                    // capture/capture_init.cpp (MSVC/WGC)
#endif
bool init_igpu_pipes(Config& cfg, uint32_t NAT_W, uint32_t NAT_H, uint32_t nat_bpp,
                     uint32_t WW, uint32_t WH, DevicesInit& o_dev, HostBridgeInit& o_host,
                     ImagesInit& o_img, FlowPipesInit& o_flow, IgpuPipesInit& o_igpu);               // capture/capture_init.cpp
bool init_flow_pipes(Config& cfg, bool single_gpu, bool want_pfg, uint32_t WW, uint32_t WH,
                     uint32_t WW_flow, uint32_t WH_flow, uint32_t flow_div,
                     DevicesInit& o_dev, VDev& FD, ImagesInit& o_img, FlowPipesInit& o_flow);       // flow/flow_init.cpp
bool init_upscale(Config& cfg, DevicesInit& o_dev, ImagesInit& o_img, IgpuPipesInit& o_igpu);        // present/present_init.cpp
void init_wap(Config& cfg, uint32_t WW, uint32_t WH, uint32_t WW_warp, uint32_t WH_warp,
              DevicesInit& o_dev, FlowPipesInit& o_flow, WapInit& o_wap);                            // warp_blend/warp_blend_init.cpp
void init_cmd_sync(Config& cfg, bool single_gpu, DevicesInit& o_dev, VDev& FD, ImagesInit& o_img,
                   FlowPipesInit& o_flow, CmdSyncInit& o_cs, BridgeInit& o_br);                      // core/core_init.cpp

// Made with my soul - Swately <3
