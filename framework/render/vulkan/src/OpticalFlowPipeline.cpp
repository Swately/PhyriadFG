// framework/render/vulkan/src/OpticalFlowPipeline.cpp
// Implementation — see header for the contract.
//
// STAGE-15: the motion estimator is now HIERARCHICAL (pyramid coarse-to-fine), promoted from
// bench/stage14_hierarchical (measured: D=16/32/64 tracked at ~38–41 dB, beating the flat search on
// BOTH quality and cost, and lifting the old R≤32 cap). The public API (init / record_optical_flow /
// motion_image) is unchanged — this is an internal algorithm swap. The warp pass (corrected, 9b509d7)
// is untouched.
//
// STAGE-20: confidence gate promoted from bench/stage19_confidence into the pillar, replacing the
// STAGE-18 per-tile static lock (static_lock_thresh absolute threshold). Changes:
//   optical_flow_hier_match.comp: +binding 4 (rg16f SAD field: R=sad_best, G=sad_zero); push constant
//     reverts 12→8 bytes (static_thresh removed); sad_zero always computed; best_sad tracked without
//     lambda bias; static-lock block removed.
//   optical_flow_warp.comp: +binding 3 (sampler2D SAD field); output C moves to binding 4; +8-byte
//     push constants {residual_ceil, improvement_frac}; per-tile WARP/BLEND confidence gate.
//   init() API: static_lock_thresh → residual_ceil=32.0f + improvement_frac=0.5f.
//   New sad_field image (RG16F, mv_w×mv_h, STORAGE|SAMPLED), destroyed in shutdown().
//
// STAGE-24: post-warp source-agreement gate promoted from bench/stage23_agreement, extending the
// warp shader. Changes:
//   optical_flow_warp.comp: push constant 8→12 bytes (+float agreement_threshold); added shared float
//     s_d[64] + unconditional per-tile d-reduction with barrier(); warp samples A_samp/B_samp fetched
//     unconditionally (barrier must be in uniform control flow); WARP only when confidence AND
//     d_tile < agreement_threshold; BLEND otherwise.
//   init() API: +agreement_thresh=0.20f (default, optimal from bench sweep).
//   Measured (bench/stage23_agreement, 256×256, GTX 1080 Ti): per-tile thresh=0.20 →
//     hud_fine +39 dB vs texelFetch conf_only, blob_edge/large_uni/two_vel delta=0;
//     warp-only cost +12% vs conf_only (barrier + unconditional warp-sample fetch).
//
// STAGE-29: Nx present pacing — warp generalised from hardcoded t=0.5 to parametric t∈(0,1).
//   optical_flow_warp.comp: push constant 12→16 bytes (+float t); half_mv_uv split into
//     t_mv_uv (= mv*t / out_size) and inv_t_mv_uv (= mv*(1−t) / out_size); result changed
//     from 0.5*(A+B) to (1−t)*A + t*B for both WARP and BLEND paths.
//   record_optical_flow(): +float t=0.5f parameter; stores last_slot_ for record_warp_only.
//   record_warp_only(): new — re-dispatches the warp at a different t, reusing the MV/SAD
//     from the last record_optical_flow call; enables N-1 intermediate presents per source
//     frame (Nx pacing in apps/render_assistant/) without re-running the block-match.
//
// STAGE-42: temporal MV prior (dual-centre coarsest search) — opt-in, default OFF, every existing
//   consumer byte-identical with it off. set_temporal_prior(bool) arms it. When armed and a previous
//   record has run, record_optical_flow copies the previous pair's coarsest MV (mvl_img_[coarsest])
//   into the predictor image (renamed zero_mv_ → prior_mv_) at record START — before the matcher
//   overwrites that source — and dispatches the coarsest match with pred_scale=1 + dual_centre=1.
//   optical_flow_hier_match.comp: push 8→12 bytes (+int dual_centre); the coarsest level evaluates the
//     ±R window around BOTH (0,0) AND round(prior) and keeps the lower-cost winner (self-healing on
//     scene cuts — the zero centre wins a poisoned prior by construction; no detector). dual_centre=0
//     (finer levels + the whole prior-off path) is byte-identical to the pre-STAGE-42 single-centre body.
//   First armed pair (no seed yet) and frames with a single-level pyramid fall back to the zero-clear.
//
// STAGE-77: candidate (second-best / runner-up) field — opt-in via init(emit_second_best=true), default
//   OFF, every existing consumer byte-identical with it off. A second RGBA16F image (cand_img_, same size
//   as mv_image_) is created when armed; the FINEST match level emits, per tile, the runner-up MV (.xy,
//   pixel units) + its pure SAD (.z, .w=0). The matcher (optical_flow_hier_match.comp) gains binding 5
//   (rgba16f, writeonly) and a 4th push int emit_second (push 12→16 bytes). The binding is ALWAYS in the
//   layout — when off (and at non-finest levels) a 1×1 RGBA16F placeholder (cand_ph_*) is bound and
//   emit_second=0, so the shader writes NOTHING there → MV/SAD outputs byte-identical. Purpose: ambiguity
//   awareness for periodic textures (striped flags, concentric tunnels) where the matcher resolves SAD
//   ties arbitrarily — the runner-up is the other plausible vector ~one texture period away.
//
// ARC-A LEVER-1 (per-tile PARAMETRIC MV refinement — `--mv-affine`, opt-in via init(mv_affine=true),
//   default OFF, every existing consumer byte-identical with it off): the block-match emits ONE
//   translational MV per 8x8 tile; on NON-translational motion (zoom/rotation/scale) a single MV cannot
//   represent the intra-tile divergence/curl → wrong MVs → the warp samples wrong content → the
//   crossfade the operator sees on real combat (proven held-out: zoom flowdsc 0.599, orbit 0.834, mixed
//   1.211 — step-INVARIANT, NOT reducible by search radius). The fix adds a per-tile LINEAR model
//   WITHOUT bloating the hot match kernel: a NEW post-pass dispatch (optical_flow_affine_fit.comp) runs
//   AFTER the finest match level, READS the full-res MV field, fits — per tile — the local 2x2 linear
//   part M over the 3x3 neighbourhood of best_mv samples (fp32 separable LS), and EMITS M into a
//   COMPANION per-tile affine field (aff_img_, RGBA16F, same size as mv_image_). The full per-pixel flow
//   a consumer reconstructs is flow(p) = best_mv(tile) + M·(p − tile_centre) [p in tile units]. On pure
//   translation M≈0 → flow(p)=best_mv exactly = byte-identical. Ill-conditioned (flat interior / aperture
//   / grid border / outlier-slope) gates M→0 via the SAME confidence test the warp uses (sad_best vs
//   residual_ceil, sad_zero·(1−improvement_frac)>sad_best) — a tile the warp would BLEND never gets a
//   fabricated gradient. When OFF: aff_img_ is NOT created, the post-pass is NOT recorded, the layout/
//   bindings of the existing passes are UNCHANGED → MV/SAD outputs byte-identical. When ON, kCoarseR is
//   also raised 6→8 (the cheap large-motion/fast-pan reach rider, gated on the same arm). Phase 1 (this):
//   matcher emits M; the in-pillar warp does NOT yet consume it (phase 2 = the output/PSNR win) — the
//   scorer measures whether M reduces the flow discrepancy BEFORE warp integration.
//
#include <phyriad/render/vulkan/OpticalFlowPipeline.hpp>

#include "optical_flow_pyr_down_spv.hpp"
#include "optical_flow_hier_match_spv.hpp"
#include "optical_flow_hier_match_fg_spv.hpp"  // CANON #12 fork: FG-variant matcher (runner-up tracking out)
#include "optical_flow_warp_spv.hpp"
#include "optical_flow_affine_fit_spv.hpp"   // ARC-A LEVER-1: per-tile affine companion-field post-pass

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace phyriad::render::vulkan {

namespace {

void log_vk(const char* op, VkResult r) noexcept {
    std::fprintf(stderr, "[OpticalFlowPipeline] %s -> VkResult=%d\n",
                 op, static_cast<int>(r));
}

[[nodiscard]] uint32_t find_memory_type(VkPhysicalDevice phys,
                                        uint32_t          type_bits,
                                        VkMemoryPropertyFlags want) noexcept
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0u; i < mp.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & want) == want)
            return i;
    }
    return UINT32_MAX;
}

[[nodiscard]] VkResult create_module(VkDevice device,
                                     const uint32_t* spv, std::size_t n,
                                     VkShaderModule& out) noexcept
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = n * sizeof(uint32_t);
    ci.pCode    = spv;
    return vkCreateShaderModule(device, &ci, nullptr, &out);
}

// Create a DEVICE_LOCAL 2D image + view. Fills (img, view, mem); returns false on any failure.
[[nodiscard]] bool create_image_2d(VkPhysicalDevice phys, VkDevice device,
                                   uint32_t w, uint32_t h, VkFormat fmt,
                                   VkImageUsageFlags usage,
                                   VkImage& img, VkImageView& view, VkDeviceMemory& mem) noexcept
{
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = fmt;
    ci.extent        = {w, h, 1u};
    ci.mipLevels     = 1u;
    ci.arrayLayers   = 1u;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = usage;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &ci, nullptr, &img) != VK_SUCCESS) return false;

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(device, img, &mr);
    const uint32_t mt = find_memory_type(phys, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mt == UINT32_MAX) return false;

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = mt;
    if (vkAllocateMemory(device, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindImageMemory(device, img, mem, 0u);

    VkImageViewCreateInfo vi{};
    vi.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image                       = img;
    vi.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    vi.format                      = fmt;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1u;
    vi.subresourceRange.layerCount = 1u;
    return vkCreateImageView(device, &vi, nullptr, &view) == VK_SUCCESS;
}

void image_barrier(VkCommandBuffer cmd, VkImage img,
                   VkImageLayout o, VkImageLayout n,
                   VkAccessFlags sa, VkAccessFlags da,
                   VkPipelineStageFlags ss, VkPipelineStageFlags ds) noexcept
{
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = o; b.newLayout = n; b.srcAccessMask = sa; b.dstAccessMask = da;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    vkCmdPipelineBarrier(cmd, ss, ds, 0u, 0u, nullptr, 0u, nullptr, 1u, &b);
}

void compute_membar(VkCommandBuffer cmd) noexcept {   // shader-write → shader-read between dispatches
    VkMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0u, 1u, &b, 0u, nullptr, 0u, nullptr);
}

constexpr uint32_t kBlockSize = 8u;   // matches the shaders' hard-coded value
constexpr int32_t  kCoarseR   = 6;    // coarsest-level radius (±6 at scale 2^(n-1) full-res → ±48 px
                                      //   at the 4090-spare operating size, covers D≤48). STAGE-17
                                      //   measured Rc6/Rf2 as near-lossless at 0.41× the Rc10/Rf3 cost.
constexpr int32_t  kCoarseRAffine = 8; // ARC-A LEVER-1b rider (DECOUPLED): raise the coarsest radius 6→8
                                      //   (±8·2^(n-1) full-res reach) for the secondary large-motion/
                                      //   fast-pan reach mode. Behind its OWN default-OFF switch
                                      //   (coarse_wide_) — it NO LONGER rides on --mv-affine, which the
                                      //   supervisor measured as net-negative (47 worse / 8 better). When
                                      //   coarse_wide_ is OFF the coarsest radius keeps kCoarseR=6
                                      //   (byte-identical). The name is kept for the decoupled rider.
constexpr int32_t  kRefineR   = 2;    // intermediate-level refinement radius (STAGE-17: Rf2)
constexpr uint32_t kMinCoarse = 32u;  // coarsest level keeps ≥ this many px/axis (small objects survive)

} // anonymous

// ─────────────────────────────────────────────────────────────────────────────
// create_mv_image  (LEVEL-0 motion-vector image, RG16F — the public field)
// create_sad_field_image  (same size, RG16F — written by block-match, read by warp)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_mv_image(VkPhysicalDevice phys_dev,
                                          VkDevice         device,
                                          uint32_t         w,
                                          uint32_t         h) noexcept
{
    mv_w_ = (w + kBlockSize - 1u) / kBlockSize;
    mv_h_ = (h + kBlockSize - 1u) / kBlockSize;
    return create_image_2d(phys_dev, device, mv_w_, mv_h_, VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        mv_image_, mv_view_, mv_memory_);
}

bool OpticalFlowPipeline::create_sad_field_image(VkPhysicalDevice phys_dev,
                                                 VkDevice         device) noexcept
{
    // Same dimensions as the level-0 MV image.  STORAGE (written by hier_match) +
    // SAMPLED (read by confidence warp).  TRANSFER_SRC: the field is now copy-able to a host
    // bridge so a downstream presenter can re-warp at an arbitrary phase off-device (STAGE-41
    // warp-at-presenter ships MV+SAD instead of pre-warped frames); mirrors the MV image's
    // existing TRANSFER_SRC. Additive — no behavioural change to the in-pillar warp path.
    return create_image_2d(phys_dev, device, mv_w_, mv_h_, VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        sad_img_, sad_view_, sad_memory_);
}

// STAGE-77 candidate (second-best) field: same dimensions as the level-0 MV/SAD images, RGBA16F.
// STORAGE (written by the finest hier_match level), SAMPLED (a downstream warp samples it), TRANSFER_SRC
// (copy-able to a host bridge, mirroring mv_image_/sad_img_ — the warp-at-presenter ships it off-device).
bool OpticalFlowPipeline::create_cand_image(VkPhysicalDevice phys_dev,
                                            VkDevice         device) noexcept
{
    return create_image_2d(phys_dev, device, mv_w_, mv_h_, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        cand_img_, cand_view_, cand_memory_);
}

// STAGE-77 1×1 RGBA16F STORAGE placeholder bound at match binding 5 whenever a level must NOT write the
// candidate field (every non-finest level, and every level when emit_second_best_ is OFF). The shader's
// emit_second push is 0 in those cases → no imageStore touches this image; it exists only to keep the
// descriptor layout valid (the established placeholder discipline). Always created (negligible — one texel).
bool OpticalFlowPipeline::create_cand_placeholder(VkPhysicalDevice phys_dev,
                                                  VkDevice         device) noexcept
{
    return create_image_2d(phys_dev, device, 1u, 1u, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        cand_ph_img_, cand_ph_view_, cand_ph_memory_);
}

// ARC-A LEVER-1 affine companion field: same dimensions as the level-0 MV/SAD images, RGBA16F.
// STORAGE (written by the affine-fit post-pass), SAMPLED (a downstream warp samples it — phase 2),
// TRANSFER_SRC (copy-able to a host bridge, mirroring mv_image_/sad_img_). Created ONLY when armed.
// Holds the per-tile 2x2 linear part M = (a,b,c,d) of the local affine; the translational part is the
// tile's own best_mv in mv_image_.
bool OpticalFlowPipeline::create_affine_image(VkPhysicalDevice phys_dev,
                                              VkDevice         device) noexcept
{
    return create_image_2d(phys_dev, device, mv_w_, mv_h_, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        aff_img_, aff_view_, aff_memory_);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_pyramid_resources  (A/B frame levels 1.., MV levels 1.., zero predictor)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_pyramid_resources(VkPhysicalDevice phys_dev,
                                                   VkDevice         device) noexcept
{
    for (uint32_t i = 1u; i < n_levels_; ++i) {
        if (!create_image_2d(phys_dev, device, lvl_w_[i], lvl_h_[i], VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                pyr_a_img_[i], pyr_a_view_[i], pyr_a_mem_[i])) return false;
        if (!create_image_2d(phys_dev, device, lvl_w_[i], lvl_h_[i], VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                pyr_b_img_[i], pyr_b_view_[i], pyr_b_mem_[i])) return false;
        // +TRANSFER_SRC (STAGE-42): the coarsest level's MV is copied into the predictor image at
        // the start of the NEXT record when the temporal prior is armed. Additive — the flag does
        // not change the prior-off path (no copy is recorded). Applied to all intermediate levels
        // for symmetry; only n_levels_-1 is ever a copy source.
        if (!create_image_2d(phys_dev, device, mvl_w_[i], mvl_h_[i], VK_FORMAT_R16G16_SFLOAT,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                mvl_img_[i], mvl_view_[i], mvl_mem_[i])) return false;
    }
    // Coarsest-level predictor (STAGE-42, renamed from zero_mv_): cleared to (0,0) when the prior is
    // off or on the first armed pair (pred_scale=0 nullifies it = old zero predictor); else the prior
    // pair's coarse MV is copied in (TRANSFER_DST) then sampled (dual_centre=1, pred_scale=1).
    return create_image_2d(phys_dev, device, mvl_w_[n_levels_ - 1u], mvl_h_[n_levels_ - 1u],
        VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        prior_mv_img_, prior_mv_view_, prior_mv_mem_);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_downsample_pipeline  (binding 0 = src sampler, 1 = dst storage)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_downsample_pipeline(VkDevice device) noexcept
{
    const VkSampler immutable = sampler_;
    const VkDescriptorSetLayoutBinding bindings[2] = {
        { 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable },
        { 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };
    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 2u; dsl_ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &down_dsl_) != VK_SUCCESS) return false;

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 1u; pl_ci.pSetLayouts = &down_dsl_;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &down_layout_) != VK_SUCCESS) return false;

    VkShaderModule mod = VK_NULL_HANDLE;
    {
        const VkResult r = create_module(device, kOpticalFlowPyrDownSpv.data(),
                                         kOpticalFlowPyrDownSpv.size(), mod);
        if (r != VK_SUCCESS) { log_vk("vkCreateShaderModule(pyr_down)", r); return false; }
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; stage.module = mod; stage.pName = "main";
    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage = stage; cp.layout = down_layout_;
    const VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &cp, nullptr, &down_pipeline_);
    vkDestroyShaderModule(device, mod, nullptr);
    if (r != VK_SUCCESS) { log_vk("vkCreateComputePipelines(pyr_down)", r); return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// create_block_match_pipeline  (HIERARCHICAL matcher: A, B, predictor, MV out, SAD field out)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_block_match_pipeline(VkDevice device) noexcept
{
    const VkSampler immutable[3] = { sampler_, sampler_, sampler_ };
    // STAGE-77: +binding 5 (rgba16f storage — the second-best/candidate field). Always present in the
    // layout (the placeholder discipline) — the matcher's emit_second push gates the actual write.
    const VkDescriptorSetLayoutBinding bindings[6] = {
        { 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[0] },
        { 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[1] },
        { 2u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[2] },
        { 3u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 4u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 5u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };
    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 6u; dsl_ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &match_dsl_) != VK_SUCCESS) return false;

    // Push constants (24 bytes): { int search_radius; float pred_scale; int dual_centre; int emit_second; int subpel; int candsel; }
    // static_thresh removed (STAGE-20 — confidence gate in warp replaces the static lock).
    // +int dual_centre (STAGE-42 — temporal MV prior; 0 = single-centre, byte-identical default).
    // +int emit_second (STAGE-77 — 1 at the finest level when armed, 0 everywhere else / when off).
    // +int subpel (ARC-A LEVER-1b — 1 at the finest level when mv_subpel armed, 0 everywhere else / off).
    // +int candsel (ARC-A holonic candidate-selection — 1 at the finest level when mv_candsel armed, 0 else).
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.size       = 5u * sizeof(int32_t) + sizeof(float);

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 1u; pl_ci.pSetLayouts = &match_dsl_;
    pl_ci.pushConstantRangeCount = 1u; pl_ci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &match_pipeline_layout_) != VK_SUCCESS) return false;

    VkShaderModule mod = VK_NULL_HANDLE;
    {
        const VkResult r = create_module(device, kOpticalFlowHierMatchSpv.data(),
                                         kOpticalFlowHierMatchSpv.size(), mod);
        if (r != VK_SUCCESS) { log_vk("vkCreateShaderModule(hier_match)", r); return false; }
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; stage.module = mod; stage.pName = "main";
    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage = stage; cp.layout = match_pipeline_layout_;
    const VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &cp, nullptr, &match_pipeline_);
    vkDestroyShaderModule(device, mod, nullptr);
    if (r != VK_SUCCESS) { log_vk("vkCreateComputePipelines(hier_match)", r); return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// create_block_match_fg_pipeline  (CANON #12 APP-LOCAL FORK: the FG-variant matcher with the runner-up /
//   STAGE-77 second-best tracking compiled out — optical_flow_hier_match_fg.comp). It REUSES the canonical
//   matcher's descriptor-set layout (match_dsl_) and pipeline layout (match_pipeline_layout_), because the
//   variant shader declares the SAME 6 bindings and the SAME 24-byte push block — only the SPV module
//   differs. So this builds ONLY a second VkPipeline; the descriptor sets allocated for the canonical are
//   bound to it unchanged. Called by init() ONLY when fg_variant_ is armed AND emit_second_best_ is OFF,
//   AFTER create_block_match_pipeline() (which builds match_pipeline_layout_ this depends on).
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_block_match_fg_pipeline(VkDevice device) noexcept
{
    VkShaderModule mod = VK_NULL_HANDLE;
    {
        const VkResult r = create_module(device, kOpticalFlowHierMatchFgSpv.data(),
                                         kOpticalFlowHierMatchFgSpv.size(), mod);
        if (r != VK_SUCCESS) { log_vk("vkCreateShaderModule(hier_match_fg)", r); return false; }
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; stage.module = mod; stage.pName = "main";
    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage = stage; cp.layout = match_pipeline_layout_;
    const VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &cp, nullptr, &match_pipeline_fg_);
    vkDestroyShaderModule(device, mod, nullptr);
    if (r != VK_SUCCESS) { log_vk("vkCreateComputePipelines(hier_match_fg)", r); return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// create_warp_pipeline  (confidence warp — STAGE-20: +SAD field sampler, 8-byte push)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_warp_pipeline(VkDevice device) noexcept
{
    const VkSampler immutable[4] = { sampler_, sampler_, sampler_, sampler_ };
    const VkDescriptorSetLayoutBinding bindings[5] = {
        { 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[0] },
        { 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[1] },
        { 2u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[2] },
        { 3u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[3] },
        { 4u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };
    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 5u; dsl_ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &warp_dsl_) != VK_SUCCESS) return false;

    // Push constants (16 bytes): { float residual_ceil; float improvement_frac; float agreement_threshold; float t; }
    VkPushConstantRange warp_pc{};
    warp_pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    warp_pc.size       = 4u * sizeof(float);

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 1u; pl_ci.pSetLayouts = &warp_dsl_;
    pl_ci.pushConstantRangeCount = 1u; pl_ci.pPushConstantRanges = &warp_pc;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &warp_pipeline_layout_) != VK_SUCCESS) return false;

    VkShaderModule mod = VK_NULL_HANDLE;
    {
        const VkResult r = create_module(device, kOpticalFlowWarpSpv.data(),
                                         kOpticalFlowWarpSpv.size(), mod);
        if (r != VK_SUCCESS) { log_vk("vkCreateShaderModule(warp)", r); return false; }
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; stage.module = mod; stage.pName = "main";
    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage = stage; cp.layout = warp_pipeline_layout_;
    const VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &cp, nullptr, &warp_pipeline_);
    vkDestroyShaderModule(device, mod, nullptr);
    if (r != VK_SUCCESS) { log_vk("vkCreateComputePipelines(warp)", r); return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// create_affine_pipeline  (ARC-A LEVER-1: per-tile affine-fit post-pass)
//   bindings: 0 = MV field sampler, 1 = affine-params storage out, 2 = SAD field sampler (the gate)
//   8-byte push { float residual_ceil; float improvement_frac; }
//   Created ONLY when armed (mv_affine_); the whole post-pass is opt-in.
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::create_affine_pipeline(VkDevice device) noexcept
{
    const VkSampler immutable[2] = { sampler_, sampler_ };
    const VkDescriptorSetLayoutBinding bindings[3] = {
        { 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[0] },
        { 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 2u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, &immutable[1] },
    };
    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 3u; dsl_ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &affine_dsl_) != VK_SUCCESS) return false;

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.size       = 2u * sizeof(float);

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 1u; pl_ci.pSetLayouts = &affine_dsl_;
    pl_ci.pushConstantRangeCount = 1u; pl_ci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &affine_pipeline_layout_) != VK_SUCCESS) return false;

    VkShaderModule mod = VK_NULL_HANDLE;
    {
        const VkResult r = create_module(device, kOpticalFlowAffineFitSpv.data(),
                                         kOpticalFlowAffineFitSpv.size(), mod);
        if (r != VK_SUCCESS) { log_vk("vkCreateShaderModule(affine_fit)", r); return false; }
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; stage.module = mod; stage.pName = "main";
    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage = stage; cp.layout = affine_pipeline_layout_;
    const VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &cp, nullptr, &affine_pipeline_);
    vkDestroyShaderModule(device, mod, nullptr);
    if (r != VK_SUCCESS) { log_vk("vkCreateComputePipelines(affine_fit)", r); return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// allocate_descriptor_sets
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::allocate_descriptor_sets(VkDevice device,
                                                   uint32_t max_in_flight) noexcept
{
    // Per slot:
    //   downsample: 2·(kLevels-1) sets  (1 CIS + 1 STO each)
    //   match:      kLevels sets         (3 CIS + 3 STO each — +1 STO sad_field STAGE-20, +1 STO cand STAGE-77)
    //   warp:       1 set               (4 CIS + 1 STO     — +1 CIS for sad_field, STAGE-20)
    // ARC-A LEVER-1: +1 set/slot for the affine post-pass when armed (2 CIS + 1 STO each).
    const uint32_t aff_sets   = mv_affine_ ? 1u : 0u;
    const uint32_t down_sets  = 2u * (n_levels_ - 1u);
    const uint32_t per_slot   = down_sets + n_levels_ + 1u + aff_sets;
    const uint32_t samplers   = (down_sets * 1u) + (n_levels_ * 3u) + 4u + (aff_sets * 2u);
    const uint32_t storages   = (down_sets * 1u) + (n_levels_ * 3u) + 1u + (aff_sets * 1u);   // STAGE-77: 3 STO per match set
    const VkDescriptorPoolSize sizes[2] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplers * max_in_flight },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          storages * max_in_flight },
    };
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = per_slot * max_in_flight;
    pi.poolSizeCount = 2u; pi.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(device, &pi, nullptr, &desc_pool_) != VK_SUCCESS) return false;

    auto alloc_one = [&](VkDescriptorSetLayout layout, VkDescriptorSet& out) -> bool {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = desc_pool_; ai.descriptorSetCount = 1u; ai.pSetLayouts = &layout;
        return vkAllocateDescriptorSets(device, &ai, &out) == VK_SUCCESS;
    };
    for (uint32_t s = 0u; s < max_in_flight; ++s) {
        for (uint32_t i = 1u; i < n_levels_; ++i) {
            if (!alloc_one(down_dsl_, down_a_set_[s][i])) return false;
            if (!alloc_one(down_dsl_, down_b_set_[s][i])) return false;
        }
        for (uint32_t i = 0u; i < n_levels_; ++i)
            if (!alloc_one(match_dsl_, match_set_[s][i])) return false;
        if (!alloc_one(warp_dsl_, warp_sets_[s])) return false;
        if (mv_affine_ && !alloc_one(affine_dsl_, affine_set_[s])) return false;   // ARC-A LEVER-1
    }
    n_sets_ = max_in_flight;
    next_set_ = 0u;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// init
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::init(VkPhysicalDevice phys_dev,
                               VkDevice         device,
                               uint32_t         width,
                               uint32_t         height,
                               uint32_t         max_in_flight,
                               int              search_radius,
                               float            residual_ceil,
                               float            improvement_frac,
                               float            agreement_thresh,
                               bool             emit_second_best,
                               bool             mv_affine,
                               bool             mv_subpel,
                               bool             coarse_wide,
                               bool             mv_candsel,
                               bool             fg_variant) noexcept
{
    if (initialized()) return true;
    if (device == VK_NULL_HANDLE || phys_dev == VK_NULL_HANDLE) return false;
    if (width == 0u || height == 0u) return false;
    if (max_in_flight == 0u || max_in_flight > kMaxInFlight)   return false;
    // The former R≤32 cap is LIFTED: large displacement is handled by the pyramid, not this radius.
    // search_radius is now the FINEST-level refinement radius; 32 is plenty (and bounds the loop).
    if (search_radius < 1 || search_radius > 32)               return false;

    device_            = device;
    search_radius_     = search_radius;
    residual_ceil_     = residual_ceil;
    improvement_frac_  = improvement_frac;
    agreement_thresh_  = agreement_thresh;
    emit_second_best_  = emit_second_best;   // STAGE-77 candidate field arm switch
    mv_affine_         = mv_affine;          // ARC-A LEVER-1 affine post-pass arm switch
    mv_subpel_         = mv_subpel;          // ARC-A LEVER-1b sub-pixel-MV arm switch (finest level only)
    coarse_wide_       = coarse_wide;        // ARC-A LEVER-1b decoupled coarse-radius rider (6→8)
    mv_candsel_        = mv_candsel;         // ARC-A holonic candidate-selection arm switch (finest level only)
    // CANON #12 app-local fork: the FG-variant matcher applies ONLY on the default path (emit_second_best
    // OFF). When emit_second_best is ON the runner-up field IS consumed → the canonical matcher must run; we
    // fold that here so match_fg_active() reflects the actual binding decision, not just the request.
    fg_variant_        = fg_variant && !emit_second_best;
    frame_w_           = width;
    frame_h_           = height;

    // Choose pyramid depth so the COARSEST level keeps ≥ kMinCoarse px/axis — over-downsampling
    // makes small objects vanish (the STAGE-14 caveat) and the coarse estimate becomes garbage that
    // the propagation amplifies. Big frames get the full kLevels (large-D reach); tiny frames fewer.
    n_levels_ = 1u;
    while (n_levels_ < kLevels &&
           (width  >> n_levels_) >= kMinCoarse &&
           (height >> n_levels_) >= kMinCoarse) {
        ++n_levels_;
    }

    // Per-level dimensions (image halves each level; MV grid = level/8, rounded up).
    for (uint32_t i = 0u; i < kLevels; ++i) {
        lvl_w_[i] = std::max(1u, width  >> i);
        lvl_h_[i] = std::max(1u, height >> i);
        mvl_w_[i] = (lvl_w_[i] + kBlockSize - 1u) / kBlockSize;
        mvl_h_[i] = (lvl_h_[i] + kBlockSize - 1u) / kBlockSize;
    }

    // Sampler (linear, clamp-to-edge — matches the shaders' assumptions).
    {
        VkSamplerCreateInfo s{};
        s.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        s.magFilter    = VK_FILTER_LINEAR;
        s.minFilter    = VK_FILTER_LINEAR;
        s.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.maxLod       = 0.0f;
        if (vkCreateSampler(device, &s, nullptr, &sampler_) != VK_SUCCESS) { shutdown(device); return false; }
    }

    if (!create_mv_image(phys_dev, device, width, height))   { shutdown(device); return false; }
    if (!create_sad_field_image(phys_dev, device))           { shutdown(device); return false; }
    // STAGE-77: always create the 1×1 placeholder (the off-path / non-finest-level binding); create the
    // full-size candidate field only when armed.
    if (!create_cand_placeholder(phys_dev, device))          { shutdown(device); return false; }
    if (emit_second_best_ && !create_cand_image(phys_dev, device)) { shutdown(device); return false; }
    // ARC-A LEVER-1: the affine companion field + its fit pipeline exist only when armed.
    if (mv_affine_ && !create_affine_image(phys_dev, device)) { shutdown(device); return false; }
    if (!create_pyramid_resources(phys_dev, device))         { shutdown(device); return false; }
    if (!create_downsample_pipeline(device))                 { shutdown(device); return false; }
    if (!create_block_match_pipeline(device))                { shutdown(device); return false; }
    // CANON #12 app-local fork: build the FG-variant matcher (runner-up tracking compiled out) ONLY when
    // armed for the default path (fg_variant_ already AND-folds !emit_second_best above). Reuses the
    // canonical's match_pipeline_layout_, so it MUST follow create_block_match_pipeline. When off, no extra
    // pipeline exists → byte-identical to the canonical-only matcher.
    if (fg_variant_ && !create_block_match_fg_pipeline(device)) { shutdown(device); return false; }
    if (!create_warp_pipeline(device))                       { shutdown(device); return false; }
    if (mv_affine_ && !create_affine_pipeline(device))       { shutdown(device); return false; }
    if (!allocate_descriptor_sets(device, max_in_flight))    { shutdown(device); return false; }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// shutdown
// ─────────────────────────────────────────────────────────────────────────────
void OpticalFlowPipeline::shutdown(VkDevice device) noexcept {
    if (device == VK_NULL_HANDLE) device = device_;
    if (device == VK_NULL_HANDLE) return;

    if (desc_pool_ != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, desc_pool_, nullptr); desc_pool_ = VK_NULL_HANDLE; }

    // ARC-A LEVER-1 affine post-pass (created only when armed; guards are no-ops when off).
    if (affine_pipeline_        != VK_NULL_HANDLE) { vkDestroyPipeline(device, affine_pipeline_, nullptr); affine_pipeline_ = VK_NULL_HANDLE; }
    if (affine_pipeline_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, affine_pipeline_layout_, nullptr); affine_pipeline_layout_ = VK_NULL_HANDLE; }
    if (affine_dsl_             != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, affine_dsl_, nullptr); affine_dsl_ = VK_NULL_HANDLE; }
    if (warp_pipeline_         != VK_NULL_HANDLE) { vkDestroyPipeline(device, warp_pipeline_, nullptr); warp_pipeline_ = VK_NULL_HANDLE; }
    if (warp_pipeline_layout_  != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, warp_pipeline_layout_, nullptr); warp_pipeline_layout_ = VK_NULL_HANDLE; }
    if (warp_dsl_              != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, warp_dsl_, nullptr); warp_dsl_ = VK_NULL_HANDLE; }
    // CANON #12 app-local fork: the FG-variant matcher pipeline (shares match_pipeline_layout_ — destroyed
    // separately below; this destroys only the variant VkPipeline). No-op when the fork was off.
    if (match_pipeline_fg_     != VK_NULL_HANDLE) { vkDestroyPipeline(device, match_pipeline_fg_, nullptr); match_pipeline_fg_ = VK_NULL_HANDLE; }
    if (match_pipeline_        != VK_NULL_HANDLE) { vkDestroyPipeline(device, match_pipeline_, nullptr); match_pipeline_ = VK_NULL_HANDLE; }
    if (match_pipeline_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, match_pipeline_layout_, nullptr); match_pipeline_layout_ = VK_NULL_HANDLE; }
    if (match_dsl_            != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, match_dsl_, nullptr); match_dsl_ = VK_NULL_HANDLE; }
    if (down_pipeline_         != VK_NULL_HANDLE) { vkDestroyPipeline(device, down_pipeline_, nullptr); down_pipeline_ = VK_NULL_HANDLE; }
    if (down_layout_           != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, down_layout_, nullptr); down_layout_ = VK_NULL_HANDLE; }
    if (down_dsl_             != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, down_dsl_, nullptr); down_dsl_ = VK_NULL_HANDLE; }
    if (sampler_              != VK_NULL_HANDLE) { vkDestroySampler(device, sampler_, nullptr); sampler_ = VK_NULL_HANDLE; }

    // Level-0 MV (the public field) and the SAD field (STAGE-20).
    if (mv_view_    != VK_NULL_HANDLE) { vkDestroyImageView(device, mv_view_,   nullptr); mv_view_    = VK_NULL_HANDLE; }
    if (mv_image_   != VK_NULL_HANDLE) { vkDestroyImage(device, mv_image_,      nullptr); mv_image_   = VK_NULL_HANDLE; }
    if (mv_memory_  != VK_NULL_HANDLE) { vkFreeMemory(device, mv_memory_,       nullptr); mv_memory_  = VK_NULL_HANDLE; }
    if (sad_view_   != VK_NULL_HANDLE) { vkDestroyImageView(device, sad_view_,  nullptr); sad_view_   = VK_NULL_HANDLE; }
    if (sad_img_    != VK_NULL_HANDLE) { vkDestroyImage(device, sad_img_,       nullptr); sad_img_    = VK_NULL_HANDLE; }
    if (sad_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device, sad_memory_,      nullptr); sad_memory_ = VK_NULL_HANDLE; }
    // STAGE-77 candidate field + its placeholder.
    if (cand_view_     != VK_NULL_HANDLE) { vkDestroyImageView(device, cand_view_,    nullptr); cand_view_     = VK_NULL_HANDLE; }
    if (cand_img_      != VK_NULL_HANDLE) { vkDestroyImage(device, cand_img_,         nullptr); cand_img_      = VK_NULL_HANDLE; }
    if (cand_memory_   != VK_NULL_HANDLE) { vkFreeMemory(device, cand_memory_,        nullptr); cand_memory_   = VK_NULL_HANDLE; }
    if (cand_ph_view_  != VK_NULL_HANDLE) { vkDestroyImageView(device, cand_ph_view_, nullptr); cand_ph_view_  = VK_NULL_HANDLE; }
    if (cand_ph_img_   != VK_NULL_HANDLE) { vkDestroyImage(device, cand_ph_img_,      nullptr); cand_ph_img_   = VK_NULL_HANDLE; }
    if (cand_ph_memory_!= VK_NULL_HANDLE) { vkFreeMemory(device, cand_ph_memory_,     nullptr); cand_ph_memory_= VK_NULL_HANDLE; }
    // ARC-A LEVER-1 affine companion field.
    if (aff_view_   != VK_NULL_HANDLE) { vkDestroyImageView(device, aff_view_,  nullptr); aff_view_   = VK_NULL_HANDLE; }
    if (aff_img_    != VK_NULL_HANDLE) { vkDestroyImage(device, aff_img_,       nullptr); aff_img_    = VK_NULL_HANDLE; }
    if (aff_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device, aff_memory_,      nullptr); aff_memory_ = VK_NULL_HANDLE; }
    // Pyramid levels 1..kLevels-1 + intermediate MV.
    for (uint32_t i = 1u; i < kLevels; ++i) {
        if (pyr_a_view_[i]) { vkDestroyImageView(device, pyr_a_view_[i], nullptr); pyr_a_view_[i] = VK_NULL_HANDLE; }
        if (pyr_a_img_[i])  { vkDestroyImage(device, pyr_a_img_[i], nullptr);      pyr_a_img_[i]  = VK_NULL_HANDLE; }
        if (pyr_a_mem_[i])  { vkFreeMemory(device, pyr_a_mem_[i], nullptr);        pyr_a_mem_[i]  = VK_NULL_HANDLE; }
        if (pyr_b_view_[i]) { vkDestroyImageView(device, pyr_b_view_[i], nullptr); pyr_b_view_[i] = VK_NULL_HANDLE; }
        if (pyr_b_img_[i])  { vkDestroyImage(device, pyr_b_img_[i], nullptr);      pyr_b_img_[i]  = VK_NULL_HANDLE; }
        if (pyr_b_mem_[i])  { vkFreeMemory(device, pyr_b_mem_[i], nullptr);        pyr_b_mem_[i]  = VK_NULL_HANDLE; }
        if (mvl_view_[i])   { vkDestroyImageView(device, mvl_view_[i], nullptr);   mvl_view_[i]   = VK_NULL_HANDLE; }
        if (mvl_img_[i])    { vkDestroyImage(device, mvl_img_[i], nullptr);        mvl_img_[i]    = VK_NULL_HANDLE; }
        if (mvl_mem_[i])    { vkFreeMemory(device, mvl_mem_[i], nullptr);          mvl_mem_[i]    = VK_NULL_HANDLE; }
    }
    if (prior_mv_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device, prior_mv_view_, nullptr); prior_mv_view_ = VK_NULL_HANDLE; }
    if (prior_mv_img_  != VK_NULL_HANDLE) { vkDestroyImage(device, prior_mv_img_, nullptr); prior_mv_img_ = VK_NULL_HANDLE; }
    if (prior_mv_mem_  != VK_NULL_HANDLE) { vkFreeMemory(device, prior_mv_mem_, nullptr); prior_mv_mem_ = VK_NULL_HANDLE; }

    for (auto& row : down_a_set_) for (auto& s : row) s = VK_NULL_HANDLE;
    for (auto& row : down_b_set_) for (auto& s : row) s = VK_NULL_HANDLE;
    for (auto& row : match_set_)  for (auto& s : row) s = VK_NULL_HANDLE;
    for (auto& s : warp_sets_)    s = VK_NULL_HANDLE;
    for (auto& s : affine_set_)   s = VK_NULL_HANDLE;   // ARC-A LEVER-1
    n_sets_ = 0u; next_set_ = 0u; n_levels_ = 0u;
    mv_w_ = mv_h_ = frame_w_ = frame_h_ = 0u;
    have_prev_mv_ = false;   // STAGE-42: a re-armed pipeline must re-seed (no stale prior). temporal_prior_ kept (the arm switch).
    fg_variant_   = false;    // CANON #12 fork: a re-init re-decides the variant from its own args (match_pipeline_fg_ already freed above).
    // CANON #12 fork (descriptor prebake): a re-init must re-bake from its own inputs — clear the arm + the
    // baked parity views (the sets they pointed at are invalidated with the pool above). desc_update_calls_
    // is NOT reset here — it is a lifetime instrument; a test/bench reads it across a record, not across init.
    fg_prebake_ = false;
    fg_pb_a_[0] = fg_pb_b_[0] = fg_pb_a_[1] = fg_pb_b_[1] = fg_pb_c_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

// ─────────────────────────────────────────────────────────────────────────────
// write_slot_descriptors  (the per-slot descriptor-update block, shared by the per-record path AND the
//   CANON #12 prebake). Writes EVERY binding of this slot's down/match/warp sets from the level-0 (a,b)
//   input views + the pipeline's own constant images. Each vkUpdateDescriptorSets is counted in
//   desc_update_calls_ (the G2 instrument). This is the BURST the prebake fork moves off the F-thread.
// ─────────────────────────────────────────────────────────────────────────────
void OpticalFlowPipeline::write_slot_descriptors(uint32_t    slot,
                                                 VkImageView a_view,
                                                 VkImageView b_view,
                                                 VkImageView c_view,
                                                 bool        emit_cand) noexcept
{
    constexpr VkImageLayout RO  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    constexpr VkImageLayout GEN = VK_IMAGE_LAYOUT_GENERAL;

    auto write_sampler = [&](VkDescriptorSet set, uint32_t bind, VkImageView v, VkImageLayout lay) {
        VkDescriptorImageInfo ii{}; ii.imageView = v; ii.imageLayout = lay;
        VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, bind, 0u, 1u,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ii, nullptr, nullptr };
        vkUpdateDescriptorSets(device_, 1u, &w, 0u, nullptr);
        ++desc_update_calls_;
    };
    auto write_storage = [&](VkDescriptorSet set, uint32_t bind, VkImageView v) {
        VkDescriptorImageInfo ii{}; ii.imageView = v; ii.imageLayout = GEN;
        VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, bind, 0u, 1u,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ii, nullptr, nullptr };
        vkUpdateDescriptorSets(device_, 1u, &w, 0u, nullptr);
        ++desc_update_calls_;
    };
    auto a_level = [&](uint32_t i) { return i == 0u ? a_view : pyr_a_view_[i]; };
    auto b_level = [&](uint32_t i) { return i == 0u ? b_view : pyr_b_view_[i]; };
    auto mv_level = [&](uint32_t i) { return i == 0u ? mv_view_ : mvl_view_[i]; };

    for (uint32_t i = 1u; i < n_levels_; ++i) {
        write_sampler(down_a_set_[slot][i], 0u, a_level(i - 1u), i == 1u ? RO : GEN);
        write_storage(down_a_set_[slot][i], 1u, pyr_a_view_[i]);
        write_sampler(down_b_set_[slot][i], 0u, b_level(i - 1u), i == 1u ? RO : GEN);
        write_storage(down_b_set_[slot][i], 1u, pyr_b_view_[i]);
    }
    for (uint32_t i = 0u; i < n_levels_; ++i) {
        const VkImageLayout abLay = (i == 0u) ? RO : GEN;
        write_sampler(match_set_[slot][i], 0u, a_level(i), abLay);
        write_sampler(match_set_[slot][i], 1u, b_level(i), abLay);
        const VkImageView pred = (i == n_levels_ - 1u) ? prior_mv_view_ : mv_level(i + 1u);
        write_sampler(match_set_[slot][i], 2u, pred, GEN);
        write_storage(match_set_[slot][i], 3u, mv_level(i));
        write_storage(match_set_[slot][i], 4u, sad_view_);   // SAD field output (finest level wins)
        const VkImageView cand_v = (emit_cand && i == 0u) ? cand_view_ : cand_ph_view_;
        write_storage(match_set_[slot][i], 5u, cand_v);      // STAGE-77 candidate field (finest level only)
    }
    write_sampler(warp_sets_[slot], 0u, a_view,    RO);
    write_sampler(warp_sets_[slot], 1u, b_view,    RO);
    write_sampler(warp_sets_[slot], 2u, mv_view_,  RO);
    write_sampler(warp_sets_[slot], 3u, sad_view_,  RO);  // sad_field in RO after barrier below
    write_storage(warp_sets_[slot], 4u, c_view);
}

// ─────────────────────────────────────────────────────────────────────────────
// prebake_fg_descriptors  (CANON #12 APP-LOCAL FORK — render_assistant; see header contract). Cold-bakes
//   the two ping-pong parities into ring slots 0/1 so the per-record vkUpdateDescriptorSets burst is gone.
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::prebake_fg_descriptors(VkImageView a0, VkImageView b0,
                                                 VkImageView a1, VkImageView b1,
                                                 VkImageView c_view) noexcept
{
    // ELIGIBILITY: the fork only applies on the FG DEFAULT path, where NO binding varies per record.
    //   - match_fg_active() ⇒ fg_variant_ armed AND emit_second_best_ OFF (so binding 5 is ALWAYS the
    //     placeholder — the baked pin is correct). emit_second_best_ ON would make binding 5 the real cand
    //     image at the finest level → it could not be pre-baked statically. mv_affine_ adds a per-record
    //     affine set write → also non-prebakeable. Both are excluded here.
    //   - need ≥ 2 ring slots (the two parities). The render_assistant inits with max_in_flight=2u.
    if (!initialized() || !match_fg_active() || emit_second_best_ || mv_affine_ || n_sets_ < 2u)
        return false;
    if (a0 == VK_NULL_HANDLE || b0 == VK_NULL_HANDLE || a1 == VK_NULL_HANDLE || b1 == VK_NULL_HANDLE ||
        c_view == VK_NULL_HANDLE)
        return false;

    // emit_cand is FALSE on this path (emit_second_best_ off) → binding 5 baked to the placeholder, matching
    // the default record exactly. c_view (warp binding 4) is Cinterp — CONSTANT across the FG run; baked into
    // both warp sets here. Bake parity 0 → slot 0, parity 1 → slot 1 (one cold update burst per slot).
    // NOTE on the flow_div>1 case: the app passes the SAME (Bflow[0],Bflow[1]) for both parities, so fwd AND
    // bwd records both select slot 0 — they bind the SAME read-only baked set from two in-flight cmd buffers.
    // That is legal (a descriptor set is externally-synchronized for UPDATE, not for bind/read; prebake never
    // updates after init), and the GPU output is identical to the per-record path (same views, same dispatch;
    // image hazards are governed by the unchanged image barriers, not by which set object is bound).
    write_slot_descriptors(0u, a0, b0, c_view, /*emit_cand*/ false);
    write_slot_descriptors(1u, a1, b1, c_view, /*emit_cand*/ false);
    fg_pb_a_[0] = a0; fg_pb_b_[0] = b0;
    fg_pb_a_[1] = a1; fg_pb_b_[1] = b1;
    fg_pb_c_    = c_view;
    fg_prebake_ = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// record_optical_flow  (hierarchical: pyramid build → coarse-to-fine match → warp)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::record_optical_flow(VkCommandBuffer cmd,
                                              VkImageView     a_view,
                                              VkImageView     b_view,
                                              VkImageView     c_view,
                                              float           t) noexcept
{
    if (!initialized() || cmd == VK_NULL_HANDLE ||
        a_view == VK_NULL_HANDLE || b_view == VK_NULL_HANDLE || c_view == VK_NULL_HANDLE)
        return false;
    if (n_sets_ == 0u) return false;

    // CANON #12 app-local fork (descriptor prebake): when armed, SELECT the ring slot whose pre-baked
    // parity matches the passed (a,b,c) inputs and SKIP the per-record vkUpdateDescriptorSets burst entirely
    // (the host-CPU relief — survey `widpumzly`). The match is by VkImageView handle equality against the two
    // baked parities + the baked c_view; if NOTHING matches (an unexpected input, or c_view changed), fall
    // back to the canonical per-record update path on the round-robin slot (safety — never bind a stale set).
    // When NOT armed (default), fg_prebake_ is false → the canonical path runs EXACTLY as before (the burst
    // executes, byte-identical). next_set_ is advanced ONLY on the per-record path, so the prebaked slots 0/1
    // are never clobbered by a fallback record between prebaked records (a fallback uses next_set_'s slot).
    int prebaked_parity = -1;
    if (fg_prebake_ && c_view == fg_pb_c_) {
        if (a_view == fg_pb_a_[0] && b_view == fg_pb_b_[0]) prebaked_parity = 0;
        else if (a_view == fg_pb_a_[1] && b_view == fg_pb_b_[1]) prebaked_parity = 1;
    }
    const bool use_prebake = (prebaked_parity >= 0);

    const uint32_t slot = use_prebake ? static_cast<uint32_t>(prebaked_parity) : next_set_;
    if (!use_prebake) next_set_ = (next_set_ + 1u) % n_sets_;
    last_slot_ = slot;   // record_warp_only reuses this slot's warp descriptor set
    constexpr VkImageLayout RO  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    constexpr VkImageLayout GEN = VK_IMAGE_LAYOUT_GENERAL;

    // STAGE-42 temporal MV prior: active for THIS record only when armed AND a previous record has
    // run (so the coarsest MV image holds a real seed). On the first armed pair, prior_active is
    // false → the predictor is zero-cleared exactly like the prior-off path (no copy recorded).
    // Requires coarsest >= 1: the prior source is the INTERMEDIATE mvl_img_[coarsest], which at record
    // start is in GENERAL (the previous matcher left it there). The degenerate single-level pyramid
    // (n_levels_==1, frames < 64 px — never the render-assistant case) leaves the source as mv_image_
    // in SHADER_READ_ONLY; rather than special-case that layout, the prior simply does not engage there
    // (zero-clear fallback, identical to off) — honest and safe.
    const uint32_t coarsest = n_levels_ - 1u;
    const bool     prior_active = temporal_prior_ && have_prev_mv_ && (coarsest >= 1u);
    const VkImage  coarse_mv_src = mvl_img_[coarsest];   // intermediate level (coarsest>=1 when used)

    // STAGE-77: the finest level (i==0) is the only one that emits the candidate field. When armed, bind
    // the real cand image there; everywhere else (and when off) bind the 1×1 placeholder. The shader's
    // emit_second push is set 1 only at the finest level when armed, so only that store touches cand_img_.
    // (On the prebake path emit_second_best_ is OFF by eligibility → emit_cand is false, matching the baked
    // binding-5 placeholder. emit_cand is still needed below for the cand-image layout barrier guards.)
    const bool emit_cand = emit_second_best_ && (cand_img_ != VK_NULL_HANDLE);

    // ── descriptor writes (all sets for this slot) ───────────────────────────
    // PREBAKE: skip the entire per-record vkUpdateDescriptorSets burst — the selected slot's collection was
    //   fully populated at init (cold). PER-RECORD (default + fallback): write every binding now, exactly as
    //   before (the shared write_slot_descriptors carries the identical logic + counts each call).
    if (!use_prebake)
        write_slot_descriptors(slot, a_view, b_view, c_view, emit_cand);

    // ── STAGE-42 prior copy (BEFORE the discard loop, while the prior coarse MV is still intact). ──
    // When prior_active, copy the previous pair's coarsest MV (coarse_mv_src, still GENERAL from the
    // last record's storage write) into prior_mv_img_, BEFORE the discard loop transitions that source
    // image UNDEFINED→GENERAL (which would discard it). The copy reads the source as TRANSFER_SRC; the
    // matcher's later write of the SAME image is hazard-ordered by the explicit TRANSFER_SRC→GENERAL
    // barrier below (src=TRANSFER_READ → dst=SHADER_WRITE), and the discard loop SKIPS this index so it
    // is not double-transitioned. The copy's TRANSFER_WRITE → the matcher's predictor SHADER_READ is
    // ordered by the prior_mv_img_ TRANSFER_DST→GENERAL barrier.
    if (prior_active) {
        image_barrier(cmd, coarse_mv_src, GEN, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        image_barrier(cmd, prior_mv_img_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageCopy cp{};
        cp.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        cp.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        cp.extent = {mvl_w_[coarsest], mvl_h_[coarsest], 1u};
        vkCmdCopyImage(cmd, coarse_mv_src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       prior_mv_img_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &cp);
        image_barrier(cmd, prior_mv_img_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, GEN,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        // Source image back to GENERAL for the matcher's coarsest write (ordered after the copy read).
        image_barrier(cmd, coarse_mv_src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, GEN,
                      VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    // ── layout setup: owned pyramid + MV images → GENERAL (UNDEFINED discards prior; rewritten). ──
    for (uint32_t i = 1u; i < n_levels_; ++i) {
        image_barrier(cmd, pyr_a_img_[i], VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        image_barrier(cmd, pyr_b_img_[i], VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        // Skip mvl_img_[coarsest] when prior_active — the copy above already left it in GENERAL,
        // hazard-ordered. (For finer levels and the prior-off path, the discard is unchanged.)
        if (!(prior_active && i == coarsest))
            image_barrier(cmd, mvl_img_[i], VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
    image_barrier(cmd, mv_image_, VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    image_barrier(cmd, sad_img_, VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    // STAGE-77: the candidate field + the placeholder must be in GENERAL for the storage binding
    // (validation checks the bound image's layout because the matcher's imageStore is statically used,
    // even though emit_second gates the actual write). UNDEFINED→GENERAL discards any prior content
    // (rewritten this record when armed; the placeholder is never written). The bound image is
    // cand_img_ at the finest level when armed, else the placeholder — both are transitioned here so
    // whichever is bound is GENERAL. cand_img_ goes back to SHADER_READ_ONLY after the match (below).
    if (emit_cand)
        image_barrier(cmd, cand_img_, VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    image_barrier(cmd, cand_ph_img_, VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    // Coarsest-level predictor: when the prior is NOT active (off, or first armed pair), zero-clear it
    // exactly as before (TRANSFER_DST → sampler-readable) — byte-identical to the pre-STAGE-42 path.
    // When prior_active the copy above already populated it in GENERAL; skip the clear.
    if (!prior_active) {
        image_barrier(cmd, prior_mv_img_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        { VkClearColorValue z{}; VkImageSubresourceRange r{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
          vkCmdClearColorImage(cmd, prior_mv_img_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &z, 1u, &r); }
        image_barrier(cmd, prior_mv_img_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, GEN,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    // ── 1. build the pyramid (downsample level i from level i-1, for A and B) ──
    for (uint32_t i = 1u; i < n_levels_; ++i) {
        const uint32_t gx = (lvl_w_[i] + 7u) / 8u, gy = (lvl_h_[i] + 7u) / 8u;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, down_pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, down_layout_, 0u, 1u, &down_a_set_[slot][i], 0u, nullptr);
        vkCmdDispatch(cmd, gx, gy, 1u);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, down_layout_, 0u, 1u, &down_b_set_[slot][i], 0u, nullptr);
        vkCmdDispatch(cmd, gx, gy, 1u);
        compute_membar(cmd);   // level i+1 reads level i
    }

    // ── 2. coarse-to-fine match (coarsest → finest) ──
    // push constants (12 bytes): { int search_radius; float pred_scale; int dual_centre } (STAGE-42).
    // static_thresh removed — confidence gate in the warp handles static tiles (STAGE-20).
    // Coarsest level, prior OFF/first pair: scale=0 (predictor nullified → centre (0,0)),
    //   dual_centre=0 → a single ±R window around zero, byte-identical to the pre-STAGE-42 path.
    // Coarsest level, prior_active: the predictor image holds last pair's coarse MV in THIS level's
    //   own pixel units → scale=1.0 (no rescale), dual_centre=1 → search BOTH (0,0) and round(prior).
    // Finer levels (unchanged): scale=2.0 (units double per level), dual_centre=0.
    for (int i = static_cast<int>(n_levels_) - 1; i >= 0; --i) {
        const uint32_t ui = static_cast<uint32_t>(i);
        const bool coarsest_lvl = (ui == n_levels_ - 1u);
        // STAGE-77: +int emit_second. 1 ONLY at the finest level (ui==0) when armed — the candidate field
        // is the full-res/8 runner-up; coarser levels never write it. 0 everywhere else and on the whole
        // feature-off path. ARC-A LEVER-1b: +int subpel (20-byte push). 1 ONLY at the finest level (ui==0)
        // when mv_subpel_ is armed — exactly mirroring emit_second; 0 at every other level and when off →
        // byte-identical to the pre-LEVER-1b 16-byte push (the store path is the exact integer best_mv).
        // ARC-A holonic candidate-selection: +int candsel. 1 ONLY at the finest level (ui==0) when
        // mv_candsel_ is armed — mirroring emit_second/subpel; 0 at every other level and when off →
        // byte-identical to the pre-candsel 20-byte push (the OFF store is the exact integer best_mv).
        struct { int32_t r; float scale; int32_t dual; int32_t emit; int32_t subpel; int32_t candsel; } pc;
        // ARC-A LEVER-1b reach rider DECOUPLED: coarse_wide_ (its own default-OFF switch) widens the
        // coarsest radius (6→8). The affine arm (mv_affine_) NO LONGER influences coarse_r — the rider was
        // measured net-negative (47 worse / 8 better) so it must not ride silently on --mv-affine.
        const int32_t coarse_r = coarse_wide_ ? kCoarseRAffine : kCoarseR;
        pc.r       = coarsest_lvl ? coarse_r : (ui == 0u ? search_radius_ : kRefineR);
        pc.scale   = coarsest_lvl ? (prior_active ? 1.0f : 0.0f) : 2.0f;
        pc.dual    = (coarsest_lvl && prior_active) ? 1 : 0;
        pc.emit    = (emit_cand && ui == 0u) ? 1 : 0;
        pc.subpel  = (mv_subpel_ && ui == 0u) ? 1 : 0;
        pc.candsel = (mv_candsel_ && ui == 0u) ? 1 : 0;
        // CANON #12 app-local fork: bind the FG-variant matcher (runner-up tracking compiled out) when it was
        // built (fg_variant_ armed AND emit_second_best_ OFF → match_pipeline_fg_ != null). It shares
        // match_pipeline_layout_ + the same descriptor set, and writes byte-identical MV/SAD on this path
        // (emit_cand is false, so pc.emit is 0 every level). When the fork is off, match_pipeline_fg_ is null
        // → the canonical match_pipeline_ is bound exactly as before (byte-identical).
        const VkPipeline match_pipe = (match_pipeline_fg_ != VK_NULL_HANDLE) ? match_pipeline_fg_ : match_pipeline_;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, match_pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, match_pipeline_layout_, 0u, 1u, &match_set_[slot][ui], 0u, nullptr);
        vkCmdPushConstants(cmd, match_pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(pc), &pc);
        const uint32_t gx = (mvl_w_[ui] + 7u) / 8u, gy = (mvl_h_[ui] + 7u) / 8u;
        vkCmdDispatch(cmd, gx, gy, 1u);
        compute_membar(cmd);   // MV(i) and SAD field writes visible to next level / warp
    }

    // ── 3. MV + SAD field: GENERAL → SHADER_READ_ONLY for the warp samplers. ──
    image_barrier(cmd, mv_image_, GEN, RO, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    image_barrier(cmd, sad_img_, GEN, RO, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    // STAGE-77: the candidate field → SHADER_READ_ONLY for a downstream sampler / a copy-out (only when
    // armed; the field was written by the finest level). Mirrors the mv/sad transitions exactly.
    if (emit_cand)
        image_barrier(cmd, cand_img_, GEN, RO, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // ── 3b. ARC-A LEVER-1 affine-fit post-pass (opt-in, armed only). Reads the finest MV field + the
    //   SAD field (both now RO samplers) and fits the per-tile 2x2 linear part M over each tile's 3x3
    //   MV neighbourhood, emitting M into aff_img_. The translational part stays in mv_image_. Runs
    //   ONLY when mv_affine_ is armed; when off this block is skipped entirely (no aff_img_ exists, the
    //   bindings/layouts of every other pass are untouched → MV/SAD outputs byte-identical). ──
    if (mv_affine_ && aff_img_ != VK_NULL_HANDLE) {
        // The affine post-pass set is written PER-RECORD (it is never part of the CANON #12 prebake — that
        // path excludes mv_affine_ by eligibility). Inline the 3 updates here (counted in the instrument).
        auto aff_write = [&](uint32_t bind, VkImageView v, VkDescriptorType ty, VkImageLayout lay) {
            VkDescriptorImageInfo ii{}; ii.imageView = v; ii.imageLayout = lay;
            VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, affine_set_[slot], bind,
                0u, 1u, ty, &ii, nullptr, nullptr };
            vkUpdateDescriptorSets(device_, 1u, &w, 0u, nullptr);
            ++desc_update_calls_;
        };
        aff_write(0u, mv_view_,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RO);
        aff_write(1u, aff_view_, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          GEN);
        aff_write(2u, sad_view_, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RO);
        // aff_img_ → GENERAL for the storage write (UNDEFINED discards prior; rewritten this record).
        image_barrier(cmd, aff_img_, VK_IMAGE_LAYOUT_UNDEFINED, GEN, 0u, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        struct { float residual_ceil; float improvement_frac; } aff_pc;
        aff_pc.residual_ceil    = residual_ceil_;
        aff_pc.improvement_frac = improvement_frac_;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, affine_pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, affine_pipeline_layout_, 0u, 1u, &affine_set_[slot], 0u, nullptr);
        vkCmdPushConstants(cmd, affine_pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(aff_pc), &aff_pc);
        const uint32_t agx = (mv_w_ + 7u) / 8u, agy = (mv_h_ + 7u) / 8u;
        vkCmdDispatch(cmd, agx, agy, 1u);
        // aff_img_ → SHADER_READ_ONLY for a downstream sampler / copy-out (the scorer copies it out).
        image_barrier(cmd, aff_img_, GEN, RO, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    // ── 4. confidence+agreement warp dispatch — STAGE-20/STAGE-24/STAGE-29 gates. ──
    // Push constants: { float residual_ceil; float improvement_frac; float agreement_threshold; float t; } (16 bytes).
    struct { float residual_ceil; float improvement_frac; float agreement_threshold; float t; } warp_pc;
    warp_pc.residual_ceil        = residual_ceil_;
    warp_pc.improvement_frac     = improvement_frac_;
    warp_pc.agreement_threshold  = agreement_thresh_;
    warp_pc.t                    = t;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, warp_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, warp_pipeline_layout_, 0u, 1u, &warp_sets_[slot], 0u, nullptr);
    vkCmdPushConstants(cmd, warp_pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(warp_pc), &warp_pc);
    vkCmdDispatch(cmd, (frame_w_ + 7u) / 8u, (frame_h_ + 7u) / 8u, 1u);
    // STAGE-42: the coarsest MV (mvl_img_[coarsest], left in GENERAL) is now a valid seed for the
    // NEXT pair. Set unconditionally — so arming the prior mid-stream picks up the most recent pair's
    // MV on its first armed record (a single pair's startup delay, not a stale/undefined read).
    have_prev_mv_ = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// record_warp_only  (STAGE-29: re-dispatch warp at a different temporal phase,
//   reusing MV + SAD from the most recent record_optical_flow call)
// ─────────────────────────────────────────────────────────────────────────────
bool OpticalFlowPipeline::record_warp_only(VkCommandBuffer cmd, float t) noexcept
{
    if (!initialized() || cmd == VK_NULL_HANDLE) return false;
    if (n_sets_ == 0u) return false;
    // Precondition: warp_sets_[last_slot_] was written by record_optical_flow for
    // the current frame pair. MV + SAD are in SHADER_READ_ONLY_OPTIMAL. c_view (Cinterp)
    // is in GENERAL (left by the copy-back barrier at the end of the previous cmdB).
    struct { float residual_ceil; float improvement_frac; float agreement_threshold; float t; } warp_pc;
    warp_pc.residual_ceil       = residual_ceil_;
    warp_pc.improvement_frac    = improvement_frac_;
    warp_pc.agreement_threshold = agreement_thresh_;
    warp_pc.t                   = t;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, warp_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, warp_pipeline_layout_,
                            0u, 1u, &warp_sets_[last_slot_], 0u, nullptr);
    vkCmdPushConstants(cmd, warp_pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT,
                       0u, sizeof(warp_pc), &warp_pc);
    vkCmdDispatch(cmd, (frame_w_ + 7u) / 8u, (frame_h_ + 7u) / 8u, 1u);
    return true;
}

} // namespace phyriad::render::vulkan
// Made with my soul - Swately <3
