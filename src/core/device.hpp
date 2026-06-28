#pragma once
// PhyriadFG core/device layer: the per-GPU Vulkan device wrapper (VDev), the memory-type picker
// (pick_mem), and the device create/destroy free functions. The bodies of vdev_create/vdev_destroy
// live in device.cpp; pick_mem is a tiny inline shared by vk_util's image/buffer factories + main.cpp.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>   // PFN_vkGetMemoryWin32HandlePropertiesKHR (VDev field)
#include <cstdint>

// ─── Vulkan helpers ────────────────────────────────────────────────────────────
inline uint32_t pick_mem(const VkPhysicalDeviceMemoryProperties& mp,uint32_t bits,VkMemoryPropertyFlags want){ for(uint32_t i=0;i<mp.memoryTypeCount;++i) if((bits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&want)==want) return i; return UINT32_MAX; }
struct VDev { VkPhysicalDevice phys=VK_NULL_HANDLE; VkDevice dev=VK_NULL_HANDLE; VkQueue q=VK_NULL_HANDLE; uint32_t qfam=UINT32_MAX; VkCommandPool pool=VK_NULL_HANDLE; VkPhysicalDeviceMemoryProperties mp{}; char name[256]={}; VkPhysicalDeviceType type=VK_PHYSICAL_DEVICE_TYPE_OTHER; bool has_emh=false; uint64_t host_align=1; PFN_vkGetMemoryHostPointerPropertiesEXT pfnHostPtr=nullptr;
    // Second queue for G's convert/present split. q2==q when shared-fallback.
    // pool2 is FAMILY-bound to qfam2 (a cmd buffer from `pool` cannot submit to a different-family q2).
    VkQueue q2=VK_NULL_HANDLE; uint32_t qfam2=UINT32_MAX; VkCommandPool pool2=VK_NULL_HANDLE;
    // --upload-xfer: a TRANSFER/async-compute queue (the 4090's parallel DMA engine) for the per-pair WAP
    // upload, so the copies overlap the graphics engine instead of serializing on A.q. qfamT is a
    // COMPUTE&&!GRAPHICS family (distinct from qfam/qfam2; can run the in-upload median COMPUTE dispatch),
    // fallback transfer-only, else UINT32_MAX → the feature force-disables. Two TIMELINE semaphores order
    // upload(A.qT)→warp(A.q) (semUpTL) and warp→upload (semWarpTL, the WAR back-edge) with NO CPU mutex.
    // Created ONLY when want_xfer_q; A.q stays present-exclusive (the partition holds).
    VkQueue qT=VK_NULL_HANDLE; uint32_t qfamT=UINT32_MAX; VkCommandPool poolT=VK_NULL_HANDLE;
    VkSemaphore semUpTL=VK_NULL_HANDLE, semWarpTL=VK_NULL_HANDLE;
    // --nvofa: the NVIDIA hardware Optical Flow Accelerator queue (VK_QUEUE_OPTICAL_FLOW_BIT_NV — its OWN
    // family on the 4090: TRANSFER+OPTICAL_FLOW, count 1). Created ONLY when want_ofa AND the device exposes
    // VK_NV_optical_flow; ofaQueue stays null otherwise (the feature force-disables to the classical OFP).
    // Uploads/layout transitions stay on the graphics/compute family (q); ofaQueue runs only
    // vkCmdOpticalFlowExecuteNV. The OF PFNs are device-loaded here so the app never touches the extension
    // entry points on the no-OFA path.
    bool has_optical_flow=false;            // the device EXPOSES VK_NV_optical_flow
    VkQueue ofaQueue=VK_NULL_HANDLE; uint32_t ofaQfam=UINT32_MAX; VkCommandPool ofaPool=VK_NULL_HANDLE;
    // Format support (BGRA8 input, R16G16_SFIXED5_NV flow, R8_UINT cost) is bench-verified on the 4090;
    // the format-query PFN is instance-level so we skip it here (session-create surfaces any unsupported
    // format loudly → auto-disable to the classical OFP).
    PFN_vkCreateOpticalFlowSessionNV   pfnCreateOFSession=nullptr;
    PFN_vkDestroyOpticalFlowSessionNV  pfnDestroyOFSession=nullptr;
    PFN_vkBindOpticalFlowSessionImageNV pfnBindOFImage=nullptr;
    PFN_vkCmdOpticalFlowExecuteNV      pfnCmdOFExecute=nullptr;
    // The VK→D3D11 bridge for --present-surface. has_* = the device EXPOSES the ext;
    // extmem_win32_enabled = it was actually requested+enabled (want_extmem_win32). pfnGetMemWin32
    // imports the D3D11 shared-texture NT handle; keyed-mutex sync is chained on the bridge-blit submit
    // (VkWin32KeyedMutexAcquireReleaseInfoKHR — no PFN, core struct).
    bool has_extmem_win32=false, has_keyed_mutex=false, extmem_win32_enabled=false;
    PFN_vkGetMemoryWin32HandlePropertiesKHR pfnGetMemWin32=nullptr;
    // Vendor-NAMED capability fields on the FG's app-local VDev (this app does NOT link framework/gpu — it
    // consumes Vulkan + topology by source inclusion; break_even_decide's offload=false is const-inlined for
    // the FG's AI≈2.5). Populated first-hand from the device in vdev_create
    // (VkPhysicalDeviceShaderFloat16Int8Features / VkPhysicalDeviceShaderIntegerDotProductFeatures,
    // query-only — never enabled — so the device-creation path is unchanged). The FG routes A/B/G by FIXED
    // device roles (primary/assist/iGPU), so the A/B/G path ignores these fields.
    // has_fp16  = VkPhysicalDeviceShaderFloat16Int8Features.shaderFloat16 (the portable fp16-packed-math primitive).
    // has_dp4a  = VkPhysicalDeviceShaderIntegerDotProductFeatures.shaderIntegerDotProduct (the cross-vendor
    //             int8-dot accelerator: NVIDIA Pascal+, AMD, Intel).
    // fp16_caps = the storage flag (shaderFloat16 implies arithmetic; storage is the gating sub-feature).
    bool has_fp16=false;            // shaderFloat16 supported (the portable fp16-packed-math key)
    bool has_dp4a=false;            // shaderIntegerDotProduct supported (the DP4a cross-vendor int8 key)
    bool fp16_storage=false;        // the 16-bit-storage sub-feature (storageBuffer16BitAccess)
    float fp16_gflops=0.0f; };      // measured fp16 packed-math throughput; 0 == not characterized
bool vdev_create(VkPhysicalDevice phys,VDev& d,bool want_swap,bool want_extmem_win32=false,bool prefer_same_family_q2=false,bool want_xfer_q=false,bool want_ofa=false);
void vdev_destroy(VDev& d);
