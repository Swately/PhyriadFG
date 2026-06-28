#pragma once
// PhyriadFG capture layer (STEP 4b of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). The CAPTURE-side D3D11/DXGI interop + the iGPU
// convert/unpack pipelines:
//   - OutInfo / D3D     : the D3D11 device + DXGI output-duplication wrapper (struct state).
//   - d3d_init          : create the D3D11 device, enumerate outputs, optionally DuplicateOutput.
//   - find_window_by_substr : the find-window-by-title helper (WGC --window; MSVC only).
//   - d3d_shutdown / d3d_staging / d3d_staging_on : teardown + CPU-readback staging textures.
//   - ConvPackPipe      : the iGPU fused convert+pack pipeline (cpipe_create/cpipe_destroy).
//   - UnpackPipe        : the packed→RGBA8 unpack pipeline (unpipe_create/unpipe_destroy).
// The compute factories take the SPIR-V as a std::vector<uint32_t>& (main.cpp builds the vectors
// from the *_spv.hpp constants), so this layer pulls NO shader headers.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>                  // ID3D11Device/Context/Texture2D + D3D11CreateDevice (D3D struct + d3d_init)
#include <dxgi1_2.h>               // IDXGIOutputDuplication / IDXGIOutput1 (DuplicateOutput) + DXGI_FORMAT
#include "core/device.hpp"         // VDev (the iGPU pipe factories)
#include "core/vk_util.hpp"        // rel<T> (COM release, used by d3d_init/d3d_shutdown bodies)
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

// ─── D3D11/DXGI ──────────────────────────────────────────────────────────────
struct OutInfo { char name[40]={}; RECT coords{}; bool attached=false; HMONITOR hmon=nullptr; int hz=0; char adapter_name[128]={}; };
struct D3D {
    ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr;
    IDXGIOutputDuplication* dup=nullptr; LUID luid{};
    char adapter[128]={}; uint32_t w=0,h=0; DXGI_FORMAT fmt=DXGI_FORMAT_UNKNOWN;
    HMONITOR cap_hmon=nullptr;
    int cap_ci=-1;             // H1: chosen output index, persisted so dda_rearm() can re-DuplicateOutput after ACCESS_LOST
    std::vector<OutInfo> outputs;
};
// want_dup=false: skip DuplicateOutput (WGC path); returns true if D3D11 device created.
bool d3d_init(D3D& d,int ci,bool want_dup=true);
// H1 (DDA hardening): re-arm the output duplication after DXGI_ERROR_ACCESS_LOST (a fullscreen/mode/MPO
// change silently kills it). Releases the dead dup + re-DuplicateOutput on the persisted output index. true=ok.
bool dda_rearm(D3D& d);
#ifdef _MSC_VER
// Find the first visible window whose title contains substr (EnumWindows helper).
HWND find_window_by_substr(const char* substr);
// Map an HMONITOR to the DXGI output INDEX on the capture (primary) adapter — for --window → DDA-on-its-monitor.
// Returns -1 if no match (e.g. the window is on a non-primary GPU's display). Startup-only (throwaway device).
int d3d_output_index_for_monitor(HMONITOR hm);
#endif // _MSC_VER
void d3d_shutdown(D3D& d);
ID3D11Texture2D* d3d_staging(D3D& d,uint32_t w,uint32_t h);
// STAGE-121 (--copy-device): same staging texture, but created on an EXPLICIT device + format (the WGC ring on
// the 2nd copy device). fmt is d.fmt (WGC BGRA8) — passed so this helper does not depend on the D3D struct.
ID3D11Texture2D* d3d_staging_on(ID3D11Device* dev,DXGI_FORMAT fmt,uint32_t w,uint32_t h);

// STAGE-26c-1/26c-3: iGPU convert+pack pipeline (3 SSBO bindings: src=Astage, dst=hostRP, rgba=hostR).
// rgba buf (binding 2) added in 26c-3: iGPU writes RGBA8 to hostR for A-present + no-upscale path.
struct ConvPackPipe { VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet set=VK_NULL_HANDLE; };
bool cpipe_create(VDev& d,VkBuffer src,VkDeviceSize src_b,VkBuffer dst,VkDeviceSize dst_b,VkBuffer rgba,VkDeviceSize rgba_b,const std::vector<uint32_t>& spv,ConvPackPipe& p);
void cpipe_destroy(VDev& d,ConvPackPipe& p);

// STAGE-26c-1: packed → RGBA8 unpack pipeline (1 SSBO src + up to 2 storage image dsts).
// On B: 2 sets (one per Bframe slot). On G: 1 set (Gsrc for upscale real-frame path).
struct UnpackPipe { VkDescriptorSetLayout dsl=VK_NULL_HANDLE; VkPipelineLayout layout=VK_NULL_HANDLE; VkPipeline pipe=VK_NULL_HANDLE; VkDescriptorPool pool=VK_NULL_HANDLE; VkDescriptorSet sets[2]={}; uint32_t nsets=0; };
bool unpipe_create(VDev& d,VkBuffer src,VkDeviceSize src_b,VkImageView* dst_views,uint32_t n,const std::vector<uint32_t>& spv,UnpackPipe& p);
void unpipe_destroy(VDev& d,UnpackPipe& p);

// -- Thread C entry (STEP 5.2): the relocated capture-thread body, defined in capture.cpp as a
//    free function taking the shared FgContext (refs to main()'s locals). FgContext is
//    forward-declared (reference param only) to avoid a capture.hpp <-> fg_context.hpp include
//    cycle; capture.cpp includes the full core/fg_context.hpp.
struct FgContext;
void run_capture(FgContext& ctx);
// --ingest-async (default OFF): the convert WORKER thread. Launched (only when cfg.ingest_async) next
// to thr_c in main(); it OWNS the convert state (cmdA/cmdG/fA/fG/Anative/Awork/cpPipe) and DROP-TO-
// NEWEST converts the freshest raw slot, publishing c_seq promptly on each convert (no STAGE-85
// deferral). Joined BEFORE any convert/Vulkan teardown. Never spawned when --ingest-async is off.
void run_convert_worker(FgContext& ctx);
