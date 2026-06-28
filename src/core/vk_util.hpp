#pragma once
// PhyriadFG core/vk_util layer (STEP 3 of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). Holds the timing helper (now_ms), the hal f16 forwarders,
// the DXGI→VK format router (route_for), the Img/HBuf wrappers + their create/destroy factories,
// and the command-buffer helpers (img_barrier / oneshot / submit_wait / submit_wait_q2 / full_bic).
// Tiny/hot helpers stay `inline` in the header (now_ms, the f16 forwarders, img_barrier, full_bic)
// and oneshot is a template; the larger factory bodies live in vk_util.cpp.
#include "core/device.hpp"          // VDev + pick_mem (the factories take VDev& and call pick_mem)
#include <vulkan/vulkan.h>
#include <dxgiformat.h>             // DXGI_FORMAT (route_for input)
#include <phyriad/hal/Simd.hpp>     // f16_to_f32 / f32_to_f16 (the f16 forwarders)
#include <chrono>
#include <cstdint>

// ─── Timing ──────────────────────────────────────────────────────────────────
inline double now_ms() { using namespace std::chrono; return duration<double,std::milli>(steady_clock::now().time_since_epoch()).count(); }

// STAGE-86b (the dogfooding fix): thin forwarders to the hal binary16 codec (phyriad::hal, FR-HAL-1,
// framework/hal/include/phyriad/hal/Simd.hpp) — PROVEN byte-identical to the old local bit-math. The
// names are kept so the ~dozens of call sites are unchanged. For the BATCH F16C decode-once (gme_fit),
// see ra::decode_f16 (ra_simd.cpp — the one flagged TU).
inline float half_to_float(uint16_t h){ return phyriad::hal::f16_to_f32(h); }
inline uint16_t float_to_half(float f){ return phyriad::hal::f32_to_f16(f); }

// ─── COM release helper ────────────────────────────────────────────────────────
// STEP 4b: relocated from src/core/main.cpp (was a file-static template at the dump-helper
// site). A header template (general util) so every module sees it — capture's COM objects
// (d3d_init/d3d_shutdown/staging) + main's DXGI/WGC interop both use it. Generic on T (only
// requires T::Release()); pulls no extra includes.
template<class T> void rel(T*& p){ if(p){p->Release();p=nullptr;} }

// ─── Format routing ───────────────────────────────────────────────────────────
enum Route { RT_PASS,RT_HDR,RT_10,RT_NONE };
bool route_for(DXGI_FORMAT f,VkFormat& vk,uint32_t& bpp,Route& rt,const char*& desc);

// ─── Vulkan image / buffer wrappers ─────────────────────────────────────────────
struct Img { VkImage img=VK_NULL_HANDLE; VkImageView view=VK_NULL_HANDLE; VkDeviceMemory mem=VK_NULL_HANDLE; };
// STAGE-106 (--upload-xfer S2): optional concurrent_fams/n_fams (default nullptr/0 → EXCLUSIVE, byte-
// identical to every existing caller). When non-null the image is created VK_SHARING_MODE_CONCURRENT over
// the given families so it can be WRITTEN on A.qT (upload) and READ on A.q (warp) with NO queue-family
// ownership transfer (the dogma-faithful no-QFOT path; mirrors hbuf_import's q2_shared buffer precedent).
// Used ONLY for the wap-input images and ONLY when xfer_on (CR6/CR9). Note: CONCURRENT can cost lossless
// compression on some HW — acceptable here (transient per-pair upload targets), not claimed free.
bool img_create(VDev& d,uint32_t w,uint32_t h,VkFormat fmt,VkImageUsageFlags usage,Img& out,bool want_view=true,
                const uint32_t* concurrent_fams=nullptr,uint32_t n_fams=0);
void img_destroy(VDev& d,Img& i);

struct HBuf { VkBuffer buf=VK_NULL_HANDLE; VkDeviceMemory mem=VK_NULL_HANDLE; void* mapped=nullptr; };
// (hbuf_create was removed in STAGE-26c: every host buffer is now an _aligned_malloc +
//  hbuf_import, so the same CPU pointer is importable on multiple devices — incl. G as SSBO.)
bool hbuf_import(VDev& d,void* ptr,VkDeviceSize bytes,HBuf& out,
                 VkBufferUsageFlags usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 bool q2_shared=false);
void hbuf_destroy(VDev& d,HBuf& b);
// STAGE-26c-3: device-local buffer (no host import — VRAM only).
// Used for hRP_b_dev: DMA destination on B so unpack reads from VRAM, not over x4.
bool dbuf_create(VDev& d,VkDeviceSize bytes,VkBufferUsageFlags usage,HBuf& out);

// ─── Command-buffer helpers ──────────────────────────────────────────────────────
// STAGE-106 (--upload-xfer S2-fix): src/dstQueueFamilyIndex were zero-init to 0 (a REAL family),
// not VK_QUEUE_FAMILY_IGNORED — benign under EXCLUSIVE single-family (a no-op transfer), but a
// malformed queue-family ownership transfer the moment a CONCURRENT image is barriered on A.qT.
// IGNORED is the correct "no ownership transfer" form for BOTH sharing modes (behaviour-preserving).
inline void img_barrier(VkCommandBuffer c,VkImage im,VkImageLayout f,VkImageLayout t,VkAccessFlags sa,VkAccessFlags da){ VkImageMemoryBarrier b{}; b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; b.oldLayout=f; b.newLayout=t; b.image=im; b.srcAccessMask=sa; b.dstAccessMask=da; b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}; vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b); }
template<class R> void oneshot(VDev& d,R rec){ VkCommandBufferAllocateInfo cbai{}; cbai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cbai.commandPool=d.pool; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=1; VkCommandBuffer cmd; vkAllocateCommandBuffers(d.dev,&cbai,&cmd); VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cmd,&bi); rec(cmd); vkEndCommandBuffer(cmd); VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmd; VkFenceCreateInfo fci{}; fci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; VkFence f; vkCreateFence(d.dev,&fci,nullptr,&f); vkQueueSubmit(d.q,1,&si,f); vkWaitForFences(d.dev,1,&f,VK_TRUE,UINT64_MAX); vkDestroyFence(d.dev,f,nullptr); vkFreeCommandBuffers(d.dev,d.pool,1,&cmd); }
void submit_wait(VDev& d,VkCommandBuffer cmd,VkFence f);   // P0: device-lost via the shared submit helpers (covers their G/B/setup callers)
// CRASH-FIX: identical to submit_wait but targets d.q2 — for non-present A-work (F's flow, C's convert)
// so A.q stays exclusive to P (present). Caller must guarantee a SINGLE submitting thread per queue
// handle (Vulkan external sync is per-handle): on device A, d.q2 is a same-family split-queue (q2mode=1)
// so A.pool cmd buffers submit with no family trap. Safe to call only when d.q2!=d.q (a real 2nd queue);
// otherwise it would submit to d.q and race P — callers gate on (A.q2!=A.q) before invoking.
void submit_wait_q2(VDev& d,VkCommandBuffer cmd,VkFence f);   // P0: device-lost via the q2 submit helper
inline VkBufferImageCopy full_bic(uint32_t w,uint32_t h){ VkBufferImageCopy c{}; c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; c.imageExtent={w,h,1}; return c; }
