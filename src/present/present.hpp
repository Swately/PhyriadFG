#pragma once
// PhyriadFG present layer (STEP 4b of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). The PRESENT-side upscale pipeline: UpPipe (a 2-binding
// compute pass — sampler2D src -> rgba8 storage dst) + up_create/up_destroy. The bilinear and
// lanczos variants share this same struct/layout and differ only by the SPIR-V passed in (the
// upscale_bilinear / upscale_lanczos modules), so this layer pulls NO shader headers — main.cpp
// builds the spv vector from the *_spv.hpp constant and passes it to up_create.
#include "core/device.hpp"     // VDev (up_create takes VDev&)
#include "core/vk_util.hpp"    // Img (up_create takes Img& src, Img& dst)
#include "core/fg_context.hpp" // STEP 5.4: FgContext (run_present's shared main()-locals as refs)
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// Upscale pipeline (2-binding compute: sampler2D src → rgba8 storage dst).
struct UpPipe { VkSampler samp=VK_NULL_HANDLE; VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set=VK_NULL_HANDLE; };
bool up_create(VDev& d,Img& src,Img& dst,const std::vector<uint32_t>& spv,UpPipe& p);
void up_destroy(VDev& d,UpPipe& p);

// FPS-OVERLAY (--fps-overlay): relocated from main.cpp file scope (STEP 5.4) so the present thread
// body (run_present, present.cpp) AND main() both see them. OverlayFpsPC = the overlay push-constants
// (MUST match overlay_fps.comp's PushConsts block byte-for-byte, 16 bytes). kOverlayW/kOverlayH = the
// overlay dispatch rect (px), top-left-anchored (multiples of 8 = the workgroup; the shader bounds-
// guards every pixel against Apresent's extent + the real glyph count).
struct OverlayFpsPC { uint32_t in_fps; uint32_t out_fps; uint32_t width; uint32_t height; };
static constexpr uint32_t kOverlayW = 264u;
static constexpr uint32_t kOverlayH = 56u;

// ── Thread P — run_present (STEP 5.4: VERBATIM relocation of main()'s thr_p_fn lambda; no logic
//    change). main() builds FgContext ctx and launches std::thread thr_p(run_present, std::ref(ctx)).
void run_present(FgContext& ctx);
