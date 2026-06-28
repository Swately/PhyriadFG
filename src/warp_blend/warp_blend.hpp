#pragma once
// PhyriadFG warp_blend layer. The WARP/BLEND/QUALITY factories + the CPU stat helpers:
//   - WapPipe   : the warp-at-presenter compute pipeline (wap_create/wap_destroy) — the core
//                 frame-gen warp; one shared layout/SPIR-V for the G-legacy and A-surface instances.
//   - FieldPipe : the iGPU contour-field producer (fpipe_create/fpipe_destroy).
//   - FillPipe  : the A-side field VISUALIZER (fillpipe_create/fillpipe_destroy).
//   - matte_mass_count / gme_dispct_from_mask : CPU stat helpers over the dissidence mask.
// The factories take the SPIR-V as a std::vector<uint32_t>& (main.cpp builds the vectors from the
// *_spv.hpp constants), so this layer pulls no shader headers.
#include "core/device.hpp"     // VDev (every factory takes VDev&)
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// ── CPU stat helpers over the dissidence mask ──────────────────────────────────
// Anchored matte mass — count of dissidence-mask blocks the shader would classify OBJECT
// (byte > matte_thresh·255). Mirrors the shader's float comparison exactly on the CPU.
uint32_t matte_mass_count(const uint8_t* mask, uint32_t mvw, uint32_t mvh, float matte_thresh);
// Derive the dis% stat from a GPU-produced mask (byte>64 ≈ r>4px). A stat print only — NOT
// load-bearing for the warp or object_repair, which read the mask bytes directly.
double gme_dispct_from_mask(const uint8_t* mask, uint32_t mvw, uint32_t mvh);

// iGPU contour-field pipeline (2 SSBO bindings: src=hostR RGBA8, dst=hostFIELD). The dispatch runs
// on G.q2 after igpu_convert_pack. Push {w,h,edge_thr,pad}.
struct FieldPipe { VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set=VK_NULL_HANDLE; };
bool fpipe_create(VDev& d,VkBuffer src,VkDeviceSize src_b,VkBuffer dst,VkDeviceSize dst_b,const std::vector<uint32_t>& spv,FieldPipe& p);
void fpipe_destroy(VDev& d,FieldPipe& p);

// A-side field VISUALIZER pipeline (3 bindings: 0=u_warp rgba8 wapOutA read-write in-place [STORAGE],
// 1=u_field r32ui wapFIELDA [STORAGE], 2=u_mv rg16f wapMVA [SAMPLED] — for advecting the field to the
// present phase). Image-bound; the dispatch runs between the warp dispatch and the present blit on the
// same A.q submit. Push {w,h,strength,edge_norm,t,pad}.
struct FillPipe { VkSampler samp=VK_NULL_HANDLE; VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set=VK_NULL_HANDLE; };
bool fillpipe_create(VDev& d,VkImageView warp_v,VkImageView field_v,VkImageView mv_v,const std::vector<uint32_t>& spv,FillPipe& p);
void fillpipe_destroy(VDev& d,FillPipe& p);

// warp-at-presenter pipeline — 4 combined-image-samplers (prevReal, curReal, MV, SAD) + 1 rgba8
// storage out + a push block. Both WapPipe instances (G legacy + A surface) share this layout and
// the same SPIR-V. The image VIEWS bound here are fixed; only their CONTENTS change per pair
// (re-uploaded from the host bridges), so the descriptor set is written once at create. The full
// 14-binding + 212-byte push layout is documented at the wap_create body in warp_blend.cpp.
struct WapPipe { VkSampler samp=VK_NULL_HANDLE; VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set=VK_NULL_HANDLE; };
bool wap_create(VDev& d,VkImageView prev_v,VkImageView cur_v,VkImageView mv_v,VkImageView sad_v,VkImageView out_v,VkImageView mvb_v,VkImageView dis_v,VkImageView disb_v,VkImageView per_v,VkBuffer mass_buf,VkImageView c2_v,VkImageView field_v,VkImageView mvt_v,VkImageView prev_out_v,const std::vector<uint32_t>& spv,WapPipe& p);
void wap_destroy(VDev& d,WapPipe& p);
