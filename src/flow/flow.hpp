#pragma once
// PhyriadFG flow layer (STEP 4a of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). The FLOW factories/structs: the CPU IRLS affine
// global-motion fit (gme_fit_affine), the VK_NV_optical_flow provider (NvofaProvider +
// create/alloc/write_set/destroy + the nvofa_run TEMPLATE), the temporal-MV EMA pipe
// (MvSmoothPipe), the gme-gpu affine fit on device B (GmePipe + gme_record), and the MV
// vector-median consensus (MedianPipe). The struct defs + the nvofa_run template live here;
// the non-template factory bodies live in flow.cpp. Factories take the SPIR-V as a
// std::vector<uint32_t>& (main.cpp builds the vectors from the *_spv.hpp constants), so this
// layer pulls NO shader headers.
#include "core/device.hpp"        // VDev (every factory takes VDev&)
#include "core/vk_util.hpp"        // Img/HBuf + img_barrier (nvofa_run template) + dbuf/hbuf helpers
#include "core/globals.hpp"        // vk_live (used inside the nvofa_run template body)
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include "core/fg_context.hpp"     // STEP 5.3: FgContext (run_flow's shared main()-locals as refs)

// kGenRing (F->P interp generation-ring depth) relocated from main.cpp (STEP 4a): GmePipe's
// dset[kGenRing][2] + gme_create's dis_fwd/dis_bwd[kGenRing] need it at the declaration site.
// STAGE-39c: F→P interp GENERATION ring depth. Was 2, which assumed P's presentation
// window = 1 source period — but R1 paces a span-S set over S×T while F (capture-paced)
// builds a set per period: at span≥2, F wrapped the ring and OVERWROTE the generation P
// was still presenting (duck-tracker forensics: presented buffers 4-6 of a span-3 set
// held the NEXT-NEXT set's early phases — checksum-exact). Depth 3 covers the common
// overload spans; the F-side guard on P's active set covers the rest.
static constexpr int kGenRing=3;

// STAGE-61: the object-holon constants. Fixed at v1 (the marker prints them verbatim — see the
// FG-print object_buf). kObjSlots = the temporal-identity table width (16 tracked objects max,
// the kickoff's k:16). kObjMinMass = a cluster is ignored below 6 member blocks (min:6, kills
// noise specks). kObjInhMin = the inheritance arms only on objects whose |mv_obj| ≥ 2px (inh:2px
// — the moving-silhouette precondition). kObjStaticMax = a member block is "near-static" (the
// cancellation signature) when its own |mv| ≤ 1px. kObjMatchRadius = the centroid-match gate in
// BLOCK units (~6 blocks). kObjMaxMiss = a slot retires after 4 consecutive unmatched pairs.
// kObjPersistShield = the HUD shield: persist[] ≥ 128 exempts a block from the repair (shield:per
// — long static history wins over membership; a long degenerate phase fades the repair after ~8
// pairs at +16/pair, the documented trade that protects every real HUD). kObjMvEma = the slot
// mv smoothing (0.5) for advection stability.
static constexpr int   kObjSlots        = 16;
static constexpr int   kObjMinMass      = 6;
static constexpr float kObjInhMin       = 2.0f;   // px — object must move this fast to arm inheritance
static constexpr float kObjStaticMax    = 1.0f;   // px — block counts as near-static below this
static constexpr float kObjMatchRadius  = 6.0f;   // block units — centroid match gate
static constexpr int   kObjMaxMiss      = 4;      // retire a slot after this many misses
static constexpr int   kObjPersistShield= 128;    // persist[] ≥ this exempts a block (the HUD shield)
static constexpr float kObjMvEma        = 0.5f;   // slot mv EMA weight
// STAGE-65: the stigmergy-expiration constants (the operator's principle: derived memories EXPIRE on
// CONTRADICTION instead of decaying through it — EMA for noise, expiration for contradiction; the same
// asymmetry STAGE-57's inertia already uses, generalized to the content-decision EMAs).
static constexpr float  kObjMvExpirePx  = 4.0f;   // slot-MV innovation (px) beyond which the memory expires
static constexpr double kMassErrExpire  = 1.0;    // mass-feedback |err−ema| innovation that expires the EMA
// STAGE-66: the scene-holon's silhouette-memory constant. The persistent CUR-anchored silhouette
// prior (a byte array in the SAME 16·r quantization the dissidence masks use) is the holon's belief;
// the STAGE-65 expiration rule is its belief-update protocol (built before the memory it protects).
// kPriorDecay = the UNCONDITIONAL per-pair decay floor an UNCONFIRMED memory suffers: prior ·= 0.55
// each pair (STAGE-67: was 0.75), so a silhouette that stops being refreshed dies in ~⌈log_0.55(thr/255)⌉
// ≈ 3 pairs at the matte cutoff (0.55³ ≈ 0.166; a mid-byte ~128 falls under a thr=0.25 cutoff (64) inside
// ~2 pairs). STAGE-67 shortens the unconfirmed-memory WAKE tail (~5→~3 pairs): the STAGE-67 matte fix
// makes the remembered wake HARMLESS for the OBJECT layer anyway (the traveling silhouette no longer
// paints object into the swept band), so this only trims the crescent-evidence tail. The armed-rim
// confirmation path is UNAFFECTED — a moving rim re-confirms its memory each pair via the max(fresh,·).
// The decay is the floor; the expiration rule (fresh-low AND |mv|>kObjStaticMax ⇒ instant 0) is the kill.
static constexpr double kPriorDecay     = 0.55;   // STAGE-66/67: unconfirmed silhouette-prior per-pair decay

// STAGE-63: the contour shape-field constants. The closed contour casts a distance + feature
// transform inward over the FILLED silhouette (chamfer integer weights 3 orthogonal / 4 diagonal,
// 2-pass bbox-bounded), so every interior block knows its distance to the rim AND its nearest rim
// block (whose MEASURED MV is the contour's instrument). kChamfOrtho/kChamfDiag = the chamfer step
// weights (3-4 chamfer ≈ Euclidean to ~2%). kObjShapeDepthMin = the depth gate: only blocks at
// chamfer dist ≥ this (≥1 block off the rim, since the nearest orthogonal step is 3) get repaired —
// the rim band is the operator's stable contour and is NEVER rewritten. kChamfWMax = the
// inheritance blend normalizer (3·4=12): w = clamp(dist/kChamfWMax, 0, 1) grades the mix from the
// nearest rim sector (near the contour, rotation/scaling correct) to the slot mean (deep interior,
// the stable EMA). kObjRimSpreadMin = the generalized arming threshold: a scaling/rotating object
// can have mean mv ≈ 0 yet a live rim field, so arm iff |mv_obj_slot| ≥ kObjInhMin OR the rim MV
// spread ≥ this. kObjRimSpreadSamples = the cap on rim blocks scanned for that spread (32 — the
// pairwise scan is O(n²) so it is bounded; the rim is stride-subsampled when larger).
static constexpr int   kChamfOrtho      = 3;      // chamfer orthogonal step weight
static constexpr int   kChamfDiag       = 4;      // chamfer diagonal step weight
static constexpr int   kObjShapeDepthMin= 3;      // chamfer dist ≥ this → strictly interior (rim band protected)
static constexpr float kChamfWMax       = 12.0f;  // 3·4 — the rim→slot blend normalizer
static constexpr int   kObjRimSpreadSamples = 32; // cap on rim blocks scanned for the spread (bounded O(n²))

// gme_fit_affine — CPU IRLS affine global-motion fit (decode-once + 6-param fit + dissidence).
double gme_fit_affine(const void* mv_raw, const void* sad_raw, uint32_t mvw, uint32_t mvh,
                      float out6[6], uint8_t* dis_out, bool sub2, int irls_iters=3);

// ── STAGE-115 (--nvofa): the NVIDIA hardware Optical Flow Accelerator FLOW PROVIDER ─────────────────
// A drop-in replacement for ofp.record_optical_flow that produces the OFP CONTRACT (writes the OFP's OWN
// motion_image()/sad_field_image() in place), so every downstream consumer (warp/gme/bg-snap/matte/host
// bridges) reads it UNCHANGED. Only ever constructed + used when --nvofa armed AND device A exposes
// VK_NV_optical_flow AND single_gpu (FD==A — the OFA is 4090-only; §3.3). The default path never touches it.
//
// Per pair (run()): (1) downscale-blit the captured pair into the OFA input images (mvw*4 x mvh*4, BGRA8,
// GENERAL); (2) vkCmdOpticalFlowExecuteNV on the OFA queue (its own family) → S10.5 flow + R8 cost at the
// mvw x mvh grid; (3) a convert compute pass (FD.q2) lands flow->RG16F MV (WW_flow px), cost->sad_best, and
// a SEPARATE |A-B| reduction -> sad_zero into the OFP images, leaving them SHADER_READ_ONLY_OPTIMAL (the
// exact post-record_optical_flow layout). STAGE-115b: the prep/OFA/convert stages chain on the GPU via two
// BINARY semaphores (semPrep: prep→OFA; semOfa: OFA→convert) so the F-thread issues 3 submits and blocks ONCE
// (on fConv) instead of three — removing the v1 preflow spike (2 CPU round-trips + inter-submit GPU idle) and
// letting the OFA(own queue)+convert overlap the warp on A.q. The OFA(~1.1ms 1080p)+converts are far under the
// 7.5ms classical flow it replaces.
struct NvofaProvider {
    bool ok=false, bidir=false;
    uint32_t mvw=0, mvh=0;       // the MV grid (== OFP motion_width/height)
    uint32_t in_w=0, in_h=0;     // OFA input size (mvw*4 x mvh*4)
    float mv_scale=1.f, cost_scale=0.5f, sadz_scale=4.f;
    int   blk=4;                 // input texels per MV tile (= in_w/mvw)
    // OFA images (each chains VkOpticalFlowImageFormatInfoNV; live in GENERAL). in0/in1 CONCURRENT over
    // {qfam,ofaQfam}; mvF/costF/mvB/costB CONCURRENT over {ofaQfam,qfam} (OFA writes, convert reads).
    VkImage in0=VK_NULL_HANDLE, in1=VK_NULL_HANDLE; VkDeviceMemory in0m=VK_NULL_HANDLE,in1m=VK_NULL_HANDLE; VkImageView in0v=VK_NULL_HANDLE,in1v=VK_NULL_HANDLE,in0cv=VK_NULL_HANDLE,in1cv=VK_NULL_HANDLE;
    VkImage mvF=VK_NULL_HANDLE, costF=VK_NULL_HANDLE, mvB=VK_NULL_HANDLE, costB=VK_NULL_HANDLE;
    VkDeviceMemory mvFm=VK_NULL_HANDLE,costFm=VK_NULL_HANDLE,mvBm=VK_NULL_HANDLE,costBm=VK_NULL_HANDLE;
    VkImageView mvFofv=VK_NULL_HANDLE,costFofv=VK_NULL_HANDLE,mvBofv=VK_NULL_HANDLE,costBofv=VK_NULL_HANDLE;   // SFIXED5/R8 OF-bind views
    VkImageView mvFiv=VK_NULL_HANDLE,mvBiv=VK_NULL_HANDLE;     // rg16i MUTABLE views (convert reads raw int16)
    VkImageView costFuv=VK_NULL_HANDLE,costBuv=VK_NULL_HANDLE; // r8ui views (convert reads cost)
    VkOpticalFlowSessionNV sess=VK_NULL_HANDLE;
    VkSampler samp=VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE;
    VkDescriptorPool dp=VK_NULL_HANDLE; VkDescriptorSet setF=VK_NULL_HANDLE, setB=VK_NULL_HANDLE;
    VkCommandBuffer cmdPrep=VK_NULL_HANDLE, cmdOfa=VK_NULL_HANDLE, cmdConv=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;   // STAGE-115 fix: F-thread-private pool for cmdPrep/cmdConv (NOT d.pool = P's present pool — the MultipleThreads-Write race)
    VkFence fPrep=VK_NULL_HANDLE, fOfa=VK_NULL_HANDLE, fConv=VK_NULL_HANDLE;
    // STAGE-115b (semaphore chain): two BINARY semaphores so the 3 data-dependent submits (prep→OFA→convert)
    // chain on the GPU with ONE CPU fence wait (fConv) instead of three. semPrep: prep(A.q2)→OFA(ofaQueue);
    // semOfa: OFA(ofaQueue)→convert(A.q2). Reusable each pair: the F-thread blocks on fConv at the end of
    // nvofa_run, so all 3 prior submits have retired and both semaphores are back unsignaled before the next pair.
    VkSemaphore semPrep=VK_NULL_HANDLE, semOfa=VK_NULL_HANDLE;
};

// nvofa provider factory declarations (bodies in flow.cpp; nvofa_img is flow.cpp-internal).
bool nvofa_create(VDev& d,NvofaProvider& n,uint32_t mvw,uint32_t mvh,uint32_t WW_flow,bool bidir,
                  float cost_scale,float sadz_scale,const std::vector<uint32_t>& spv);
void nvofa_write_set(VDev& d,NvofaProvider& n,VkDescriptorSet set,VkImageView flowiv,VkImageView costuv,VkImageView inA,VkImageView inB,VkImageView mvOut,VkImageView sadOut);
void nvofa_destroy(VDev& d,NvofaProvider& n);
bool nvofa_alloc_cmds(VDev& d,NvofaProvider& n);

// One pair (a forward OR a backward direction). src_a/src_b = the captured pair source images (full-res
// or Bflow); src_w/src_h = their resolution. mvOut/sadOut/mvOutView/sadOutView = the OFP images to land on
// (ofp.motion_image()/sad_field_image() for fwd; or the SAME for bwd — the caller copies between). use_bwd
// selects the OFA backward outputs.
//
// STAGE-115b (semaphore chain): the three data-dependent stages (prep→OFA→convert) chain on the GPU with a
// pair of BINARY semaphores so the F-thread issues 3 submits and blocks ONCE (on fConv at the end) instead of
// three times. (1) cmdPrep → A.q2 (under a_q2_mtx, SUBMIT ONLY), SIGNAL semPrep, no fence. (2) cmdOfa →
// d.ofaQueue (LOCK-FREE — its own family), WAIT semPrep, SIGNAL semOfa, no CPU wait. (3) cmdConv → A.q2 (under
// a_q2_mtx, SUBMIT ONLY), WAIT semOfa, FENCE fConv. Then one vkWaitForFences(fConv). The GPU wall-clock stays
// ~prep+OFA+convert (they ARE serial — convert needs OFA needs prep), but the 2 CPU round-trips + the
// inter-submit GPU idle (the preflow spike) are gone, and the OFA(own queue)+convert overlap the warp on A.q.
// Leaves mvOut/sadOut in SHADER_READ_ONLY_OPTIMAL.
//
// q2_submit: a caller callable that submits ONE cmd to A.q2 UNDER a_q2_mtx (shared with C's convert) with
// {waitSem, waitStage, signalSem, fence} — submit only, NO wait under the lock. The OFA-queue submit + the
// final fConv wait happen here inside nvofa_run (the OFA queue is lock-free; the fConv wait is outside the lock).
template<class Q2Submit>
void nvofa_run(VDev& d,NvofaProvider& n,VkImage src_a,VkImage src_b,uint32_t src_w,uint32_t src_h,
                      bool use_bwd,VkImage mvOut,VkImageView mvOutView,VkImage sadOut,VkImageView sadOutView,
                      VkImageLayout src_layout_in,Q2Submit q2_submit){
    // ── (1) prep: downscale-blit the pair into the OFA inputs (→ GENERAL). src images are
    // SHADER_READ_ONLY_OPTIMAL on entry (the F-thread's contract) — transition to TRANSFER_SRC and back.
    vkResetCommandBuffer(n.cmdPrep,0);
    VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(n.cmdPrep,&bi);
    auto blit_in=[&](VkImage src,VkImage dst){
        img_barrier(n.cmdPrep,src,src_layout_in,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        img_barrier(n.cmdPrep,dst,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageBlit bl{}; bl.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; bl.dstSubresource=bl.srcSubresource;
        bl.srcOffsets[1]={(int)src_w,(int)src_h,1}; bl.dstOffsets[1]={(int)n.in_w,(int)n.in_h,1};
        vkCmdBlitImage(n.cmdPrep,src,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dst,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bl,VK_FILTER_LINEAR);
        img_barrier(n.cmdPrep,src,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,src_layout_in,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
        img_barrier(n.cmdPrep,dst,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    };
    // INPUT binding = frame at t0 (prev), REFERENCE = frame at t1 (cur). For a fwd pass src_a=prev,src_b=cur;
    // the bwd pass passes them swapped by the caller, so this stays direction-agnostic.
    blit_in(src_a,n.in0); blit_in(src_b,n.in1);
    // The OFA output images need their initial UNDEFINED→GENERAL transition before the OFA writes them. Done
    // on this prep buffer (FD.q2) — CONCURRENT images, no QFOT needed. Idempotent (GENERAL→GENERAL after).
    VkImage ofMv = use_bwd? n.mvB : n.mvF; VkImage ofCost = use_bwd? n.costB : n.costF;
    img_barrier(n.cmdPrep,ofMv,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
    img_barrier(n.cmdPrep,ofCost,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
    vkEndCommandBuffer(n.cmdPrep);
    // (1) prep → A.q2 (under a_q2_mtx, submit only), SIGNAL semPrep, no fence/wait.
    q2_submit(n.cmdPrep,VK_NULL_HANDLE,(VkPipelineStageFlags)0,n.semPrep,VK_NULL_HANDLE);
    // ── (2) OFA execute on the OFA queue (its own family; CONCURRENT images → no QFOT). LOCK-FREE: WAIT
    // semPrep (the prep blits must land first), SIGNAL semOfa (the convert reads the OFA output). No CPU wait. ──
    vkResetCommandBuffer(n.cmdOfa,0);
    VkCommandBufferBeginInfo bo{}; bo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(n.cmdOfa,&bo);
    VkOpticalFlowExecuteInfoNV ei{}; ei.sType=VK_STRUCTURE_TYPE_OPTICAL_FLOW_EXECUTE_INFO_NV; ei.regionCount=0; ei.pRegions=nullptr; ei.flags=0;
    d.pfnCmdOFExecute(n.cmdOfa,n.sess,&ei);
    vkEndCommandBuffer(n.cmdOfa);
    { const VkPipelineStageFlags ofaWaitStage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
      si.waitSemaphoreCount=1; si.pWaitSemaphores=&n.semPrep; si.pWaitDstStageMask=&ofaWaitStage;
      si.commandBufferCount=1; si.pCommandBuffers=&n.cmdOfa; si.signalSemaphoreCount=1; si.pSignalSemaphores=&n.semOfa;
      vkQueueSubmit(d.ofaQueue,1,&si,VK_NULL_HANDLE); }   // no fence: the chain ends at fConv
    // ── (3) convert on FD.q2: OFA flow/cost + the input pair → the OFP MV/SAD images, leave them RO. ──
    VkDescriptorSet set = use_bwd? n.setB : n.setF;
    vkResetCommandBuffer(n.cmdConv,0);
    VkCommandBufferBeginInfo bc{}; bc.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(n.cmdConv,&bc);
    // the OFP mv/sad outputs: bring to GENERAL for the storage write (they enter UNDEFINED on the FIRST pair,
    // SHADER_READ_ONLY_OPTIMAL afterwards — use the broad ALL→GENERAL barrier with srcAccess 0/SHADER_READ).
    img_barrier(n.cmdConv,mvOut,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
    img_barrier(n.cmdConv,sadOut,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(n.cmdConv,VK_PIPELINE_BIND_POINT_COMPUTE,n.pipe);
    vkCmdBindDescriptorSets(n.cmdConv,VK_PIPELINE_BIND_POINT_COMPUTE,n.layout,0,1,&set,0,nullptr);
    struct{ float mv_scale,cost_scale,sadz_scale; int blk,mvw,mvh; } pc{ n.mv_scale,n.cost_scale,n.sadz_scale,n.blk,(int)n.mvw,(int)n.mvh };
    vkCmdPushConstants(n.cmdConv,n.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pc),&pc);
    vkCmdDispatch(n.cmdConv,(n.mvw+7)/8,(n.mvh+7)/8,1);
    img_barrier(n.cmdConv,mvOut,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    img_barrier(n.cmdConv,sadOut,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    vkEndCommandBuffer(n.cmdConv);
    // (3) convert → A.q2 (under a_q2_mtx, submit only), WAIT semOfa (COMPUTE stage — the dispatch reads the OFA
    // output), SIGNAL nothing, FENCE fConv. The q2_submit resets fConv before the submit.
    q2_submit(n.cmdConv,n.semOfa,(VkPipelineStageFlags)VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_NULL_HANDLE,n.fConv);
    // The F-thread blocks ONCE here: fConv signals only after the whole prep→OFA→convert chain has retired, so
    // both binary semaphores are back unsignaled and the per-pair cmd buffers are free to reset next pair.
    vk_live(vkWaitForFences(d.dev,1,&n.fConv,VK_TRUE,UINT64_MAX));
    (void)mvOutView; (void)sadOutView;
}

// STAGE-35-R2: temporal MV smoothing pipeline — 2 storage-image bindings (mv in/out, prev in/out),
// 16-byte push {alpha,cut,w,h}. mv_view is a STORAGE view aliasing OFP's motion_image() (which is
// created STORAGE|SAMPLED|TRANSFER_SRC — a storage view is legal); prev is an app-owned RG16F image.
struct MvSmoothPipe { VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set=VK_NULL_HANDLE; VkImageView mv_store=VK_NULL_HANDLE; };
bool mvsm_create(VDev& d,VkImage mv_img,VkImageView prev_view,const std::vector<uint32_t>& spv,MvSmoothPipe& p);
void mvsm_destroy(VDev& d,MvSmoothPipe& p);

// ── IDEA-1 (gme-gpu): the global-motion affine-fit pipeline set, run ON DEVICE B. ──────────────────
// Three chained compute passes (reduce → solve → dissidence) replace the CPU gme_fit_affine. They
// share two SMALL device-local SSBOs (Accum: 12 floats; Model: 6 floats) and read the OFP MV/SAD as
// STORAGE views (the OFP images are STORAGE|SAMPLED|TRANSFER_SRC — storage views are legal, the
// mv_smooth precedent). The dissidence pass writes the R8 mask DIRECTLY into the host-imported hostDIS
// bridge (imported on B as a STORAGE buffer; packed 4 bytes/uint via atomicOr after a host fill-0) —
// no device-local mask image, no copy-out: B writes straight into the host-coherent buffer F reads and
// A uploads. R8_UNORM storage IMAGES are not mandatory (portability) — the packed-uint SSBO avoids that.
//   reduce      set: {0 storage_image MV (rg16f), 1 ssbo Accum, 2 ssbo Model}  push{mvw,mvh,step,iter}
//   solve       set: {0 ssbo Accum, 1 ssbo Model}                              (no push)
//   dissidence  set: {0 storage_image MV, 1 storage_image SAD, 2 ssbo DisMask (per gen×anchor),
//                     3 ssbo Model}                                            push{mvw,mvh,use_gate,sz}
// The dis-OUT buffer is the per-gen hostDIS bridge (kGenRing gens × fwd/bwd anchor) → dissidence sets
// are allocated per (gen × anchor) and bound once at init; reduce/solve sets are gen/anchor-independent
// (MV/Accum/Model are shared) → one each. The Model SSBO is read back per pair (the 6 floats for
// object_repair). The fwd model is copied to the host BEFORE the bwd record reuses Model (serial WAP).
struct GmePipe {
    VkDescriptorSetLayout rdsl=VK_NULL_HANDLE, sdsl=VK_NULL_HANDLE, ddsl=VK_NULL_HANDLE;
    VkPipelineLayout rlay=VK_NULL_HANDLE, slay=VK_NULL_HANDLE, dlay=VK_NULL_HANDLE;
    VkPipeline rpipe=VK_NULL_HANDLE, spipe=VK_NULL_HANDLE, dpipe=VK_NULL_HANDLE;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    VkDescriptorSet rset=VK_NULL_HANDLE, sset=VK_NULL_HANDLE;
    VkDescriptorSet dset[kGenRing][2]={};   // dissidence sets per (gen, anchor: 0=fwd,1=bwd)
    VkImageView mv_store=VK_NULL_HANDLE, sad_store=VK_NULL_HANDLE;      // OFP MV/SAD storage views
    HBuf accum{};    // device-local 12-float accumulator SSBO
    HBuf model{};    // device-local 6-float model SSBO (read back per pair)
};
bool gme_create(VDev& d,VkImage mv_img,VkImage sad_img,
                VkBuffer dis_fwd[kGenRing],VkBuffer dis_bwd[kGenRing],VkDeviceSize dis_bytes,
                bool want_bwd,
                const std::vector<uint32_t>& rspv,const std::vector<uint32_t>& sspv,
                const std::vector<uint32_t>& dspv,GmePipe& g);
void gme_destroy(VDev& d,GmePipe& g);
void gme_record(VkCommandBuffer cmd,GmePipe& g,VkImage mv_img,VkImage sad_img,
                uint32_t mvw,uint32_t mvh,uint32_t step,bool use_gate,
                int gen,int anchor,VkBuffer dis_buf,VkDeviceSize dis_bytes,
                VkBuffer model_dst,int irls_iters);

// STAGE-49b/c: MV vector-median / color-guided consensus pipeline (A) — 1 combined-image-sampler
// (MV field, sampled RO) + 1 RG16F storage out (the scratch) + (STAGE-49c) 1 combined-image-
// sampler for cur_real (binding 2, the color-membership reference) and a 4-byte push {sim_thresh}.
// ONE pipeline + TWO descriptor sets: set[0] reads wapMVA, set[1] reads wapMVBA (bidir) — the out
// view (scratch) and the cur_real view are the SAME for both. The dispatch ping-pongs scratch back
// into the MV image host-side (copy), so a single scratch + a per-source set is correct and minimal.
// sim_thresh push = 0 → the blind McGuire median (49b, byte-identical); > 0 → color-weighted
// consensus (49c). cur_real (binding 2) is bound regardless (a valid image); unread when sim==0.
struct MedianPipe { VkSampler samp=VK_NULL_HANDLE; VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set_mv=VK_NULL_HANDLE; VkDescriptorSet set_mvb=VK_NULL_HANDLE; };
bool med_create(VDev& d,VkImageView mv_v,VkImageView mvb_v,VkImageView cur_v,VkImageView out_v,bool with_mvb,const std::vector<uint32_t>& spv,MedianPipe& p);
void med_destroy(VDev& d,MedianPipe& p);

// ── Thread F — run_flow (STEP 5.3: VERBATIM relocation of main()'s thr_f_fn lambda; no logic
//    change). main() builds FgContext ctx and launches std::thread thr_f(run_flow, std::ref(ctx)).
void run_flow(FgContext& ctx);
