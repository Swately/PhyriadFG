// PhyriadFG — present-side init (E1 of docs/planning/RESTRUCTURE_PLAN.md).
// The init-seq sections that build the present-path producer state, moved verbatim out of
// main.cpp behind ownership-struct binding preambles. The bodies are byte-identical to the
// pre-E1 main.cpp sections except `goto done` -> `return false` (main() converts back).
#include "capture/wgc_ctx.hpp"   // FIRST: defines NOMINMAX before <windows.h> (winrt include order matters — same as main.cpp)
#include <algorithm>
#include <cstdio>
#include "core/app_init.hpp"
#include "upscale_bilinear_spv.hpp"
#include "upscale_lanczos_spv.hpp"

// ── Upscale pipeline (G) (E1: moved VERBATIM from main.cpp; forced-off path, kept compiled).
bool init_upscale(Config& cfg, DevicesInit& o_dev, ImagesInit& o_img, IgpuPipesInit& o_igpu){
    VDev& G=o_dev.G;
    auto& use_upscale=o_dev.use_upscale;
    auto& Gsrc=o_img.Gsrc; auto& Gdst=o_img.Gdst;
    auto& upPipe=o_igpu.upPipe;
    // ── Upscale pipeline (G) ──────────────────────────────────────────────────
    if(use_upscale){
        // .data()/.size() (raw uint32_t*, a common type) — NOT .begin()/.end(): the two SPV arrays
        // differ in size, so their std::array iterators are distinct class types that MSVC refuses to
        // unify in a ternary (mingw/gcc decayed them to a pointer; MSVC does not).
        const uint32_t* spv_ptr = cfg.lanczos ? kUpscaleLanczosSpv.data() : kUpscaleBilinearSpv.data();
        const size_t    spv_len = cfg.lanczos ? kUpscaleLanczosSpv.size() : kUpscaleBilinearSpv.size();
        const std::vector<uint32_t> spv(spv_ptr, spv_ptr + spv_len);
        if(!up_create(G,Gsrc,Gdst,spv,upPipe)){ std::printf("[ra] upscale pipeline failed\n"); return false; }
    }
    return true;
}
// ── Producer-side bridge init (E1: moved VERBATIM from main.cpp's init-seq — RESTRUCTURE_PLAN §3-E1).
// Builds bridge slot-0, the --async-present slot-1 (graceful degrade, never fatal) and slot-0's
// dedicated async cmd/fence, then applies the --rfp/--motion-fallback co-arm guards. Returns false
// where the original section did `goto done` (main() then jumps to its cleanup).
bool init_bridge_slots(Config& cfg, D3D& d, const RECT& pc, DevicesInit& o_dev, BridgeInit& o_br){
    VDev& A=o_dev.A;
    auto& use_upscale=o_dev.use_upscale; auto& use_gme=o_dev.use_gme;
    auto& bridge_w=o_br.bridge_w; auto& bridge_h=o_br.bridge_h;
    auto& bridge_tex=o_br.bridge_tex; auto& bridge_km_d3d=o_br.bridge_km_d3d;
    auto& bridge_nt=o_br.bridge_nt; auto& bridge_img=o_br.bridge_img; auto& bridge_mem=o_br.bridge_mem;
    auto& bridge_use_km=o_br.bridge_use_km;
    auto& bridge_tex1=o_br.bridge_tex1; auto& bridge_km_d3d1=o_br.bridge_km_d3d1;
    auto& bridge_nt1=o_br.bridge_nt1; auto& bridge_img1=o_br.bridge_img1; auto& bridge_mem1=o_br.bridge_mem1;
    auto& cmdBridge1=o_br.cmdBridge1; auto& fBridge1=o_br.fBridge1;
    auto& cmdBridgeA0=o_br.cmdBridgeA0; auto& fBridgeA0=o_br.fBridgeA0;
        // ── Producer side: D3D11 shared bridge texture + VK-A import ──────────
        // One BGRA8 shared texture at the present-monitor extent (= the surface backbuffer), on d.dev (the
        // capture D3D11 device — LUID-matched to A / the 4090, the panel owner). The bridge VK image is
        // B8G8R8A8 so the present-source blit (rgba8→bgra8 channel reinterpretation) lands byte-identically,
        // and the pillar CopyResource (BGRA8→BGRA8) is a valid same-format full-resource copy.
        bridge_w=(uint32_t)(pc.right-pc.left); bridge_h=(uint32_t)(pc.bottom-pc.top);
        // Keyed-mutex path iff A's VK device exposes VK_KHR_win32_keyed_mutex (verified at device
        // create). If absent: a no-KM shared texture + CPU-fence ordering (P waits fBridge before
        // submit() — same thread, ordering holds). The pillar skips AcquireSync when the producer
        // texture carries no keyed mutex (ensure_imported leaves keyed==null).
        bridge_use_km = A.has_keyed_mutex && A.extmem_win32_enabled;
        // The SINGLE bridge-format decision driven by cfg.present_format. When the present is FP16
        // (--present-fp16/--hdr), the producer's shared bridge texture MUST be FP16 too — else the pillar's
        // one CopyResource(FP16 backbuffer, this texture) is a D3D11 format-mismatch (no-op/validation error
        // → black/garbage present). The wapOutA->bridge step is a vkCmdBlitImage (VK_FILTER_LINEAR, below),
        // which CONVERTS rgba8->fp16, so widening the bridge is sufficient (no FP16 warp math → the Pascal
        // 1/64 floor is honored; the warp output is sampled and the blit writes FP16 texels). present_format
        // ==0 → the literal BGRA8 line → BYTE-IDENTICAL. A post-create disagreement (FP16 swapchain refused
        // on this rig but the bridge built FP16) is caught at the surface create and degrades to a named
        // clean quit.
        const bool bridge_fp16 = (cfg.present_format==1);
        const DXGI_FORMAT bridge_dxgi_fmt = bridge_fp16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
        const VkFormat    bridge_vk_fmt   = bridge_fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT  : VK_FORMAT_B8G8R8A8_UNORM;
        D3D11_TEXTURE2D_DESC btd{}; btd.Width=bridge_w; btd.Height=bridge_h; btd.MipLevels=1; btd.ArraySize=1;
        btd.Format=bridge_dxgi_fmt; btd.SampleDesc.Count=1; btd.Usage=D3D11_USAGE_DEFAULT;
        btd.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        btd.MiscFlags=(UINT)D3D11_RESOURCE_MISC_SHARED_NTHANDLE|(bridge_use_km?(UINT)D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX:0u);
        if(FAILED(d.dev->CreateTexture2D(&btd,nullptr,&bridge_tex))||!bridge_tex){ std::printf("[ra] bridge: CreateTexture2D failed\n"); return false; }
        { IDXGIResource1* dr1=nullptr;
          if(SUCCEEDED(bridge_tex->QueryInterface(__uuidof(IDXGIResource1),(void**)&dr1))&&dr1){
              dr1->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&bridge_nt); dr1->Release(); }
          if(!bridge_nt){ std::printf("[ra] bridge: CreateSharedHandle failed\n"); return false; } }
        if(bridge_use_km) bridge_tex->QueryInterface(__uuidof(IDXGIKeyedMutex),(void**)&bridge_km_d3d);
        // Import the D3D11 texture into VK-A as a B8G8R8A8 image (dedicated alloc; the win32 NT
        // handle import). vkGetImageMemoryRequirements after the external image gives the size; the
        // win32 handle-properties query gives the valid memory-type bits.
        {
            VkExternalMemoryImageCreateInfo emi{}; emi.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
            emi.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
            VkImageCreateInfo ici{}; ici.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ici.pNext=&emi;
            ici.imageType=VK_IMAGE_TYPE_2D; ici.format=bridge_vk_fmt; ici.extent={bridge_w,bridge_h,1};   // match the D3D11 bridge format (FP16 when --present-fp16; BGRA8 default = byte-identical)
            ici.mipLevels=1; ici.arrayLayers=1; ici.samples=VK_SAMPLE_COUNT_1_BIT; ici.tiling=VK_IMAGE_TILING_OPTIMAL;
            ici.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT; ici.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            if(vkCreateImage(A.dev,&ici,nullptr,&bridge_img.img)!=VK_SUCCESS){ std::printf("[ra] bridge: vkCreateImage failed\n"); return false; }
            VkMemoryRequirements mr; vkGetImageMemoryRequirements(A.dev,bridge_img.img,&mr);
            VkMemoryWin32HandlePropertiesKHR wp{}; wp.sType=VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
            if(!A.pfnGetMemWin32||A.pfnGetMemWin32(A.dev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,bridge_nt,&wp)!=VK_SUCCESS)
                { std::printf("[ra] bridge: vkGetMemoryWin32HandleProperties failed\n"); return false; }
            const uint32_t mt=pick_mem(A.mp,mr.memoryTypeBits&wp.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if(mt==UINT32_MAX){ std::printf("[ra] bridge: no device-local memory type for the import\n"); return false; }
            VkImportMemoryWin32HandleInfoKHR imp{}; imp.sType=VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
            imp.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT; imp.handle=bridge_nt;
            VkMemoryDedicatedAllocateInfo ded{}; ded.sType=VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO; ded.image=bridge_img.img; imp.pNext=&ded;
            VkMemoryAllocateInfo mai{}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.pNext=&imp; mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
            if(vkAllocateMemory(A.dev,&mai,nullptr,&bridge_mem)!=VK_SUCCESS){ std::printf("[ra] bridge: vkAllocateMemory(import) failed\n"); return false; }
            vkBindImageMemory(A.dev,bridge_img.img,bridge_mem,0);
        }
        std::printf("[ra] present-surface: bridge %ux%u %s on A (4090), keyed-mutex sync=%s\n",
            bridge_w,bridge_h,bridge_fp16?"FP16(R16G16B16A16_FLOAT, HDR)":"BGRA8",bridge_use_km?"VK_KHR_win32_keyed_mutex":"CPU-fence (no KM)");
        // ── --async-present: the SECOND bridge slot ──────────────────────────
        // Mirror slot-0 EXACTLY (same bridge_w/bridge_h, same D3D11_TEXTURE2D_DESC, same CreateSharedHandle,
        // same VK external-image import). Alternating the two NT handles makes PresentSurface re-import each
        // present (a µs-scale OpenSharedResource1 — ensure_imported re-imports only on handle change) which is
        // fine on the async path. Allocate the slot-1 cmd buffer + fence here too. On ANY failure: print,
        // release whatever was partially built, and FORCE cfg.async_present=false (the off path runs
        // byte-identically) — never goto done, never crash.
        if(cfg.async_present){
            bool ok1=true;
            D3D11_TEXTURE2D_DESC btd1{}; btd1.Width=bridge_w; btd1.Height=bridge_h; btd1.MipLevels=1; btd1.ArraySize=1;
            btd1.Format=bridge_dxgi_fmt; btd1.SampleDesc.Count=1; btd1.Usage=D3D11_USAGE_DEFAULT;   // slot-1 mirrors slot-0's format (FP16 when --present-fp16; BGRA8 default = byte-identical)
            btd1.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
            btd1.MiscFlags=(UINT)D3D11_RESOURCE_MISC_SHARED_NTHANDLE|(bridge_use_km?(UINT)D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX:0u);
            if(FAILED(d.dev->CreateTexture2D(&btd1,nullptr,&bridge_tex1))||!bridge_tex1){ std::printf("[ra] bridge[1]: CreateTexture2D failed — falling back to sync present\n"); ok1=false; }
            if(ok1){ IDXGIResource1* dr1=nullptr;
                if(SUCCEEDED(bridge_tex1->QueryInterface(__uuidof(IDXGIResource1),(void**)&dr1))&&dr1){
                    dr1->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&bridge_nt1); dr1->Release(); }
                if(!bridge_nt1){ std::printf("[ra] bridge[1]: CreateSharedHandle failed — falling back to sync present\n"); ok1=false; } }
            if(ok1&&bridge_use_km) bridge_tex1->QueryInterface(__uuidof(IDXGIKeyedMutex),(void**)&bridge_km_d3d1);
            if(ok1){
                VkExternalMemoryImageCreateInfo emi1{}; emi1.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
                emi1.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
                VkImageCreateInfo ici1{}; ici1.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ici1.pNext=&emi1;
                ici1.imageType=VK_IMAGE_TYPE_2D; ici1.format=bridge_vk_fmt; ici1.extent={bridge_w,bridge_h,1};   // slot-1 mirrors slot-0's VK format
                ici1.mipLevels=1; ici1.arrayLayers=1; ici1.samples=VK_SAMPLE_COUNT_1_BIT; ici1.tiling=VK_IMAGE_TILING_OPTIMAL;
                ici1.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT; ici1.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
                if(vkCreateImage(A.dev,&ici1,nullptr,&bridge_img1.img)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkCreateImage failed — falling back to sync present\n"); ok1=false; }
                else {
                    VkMemoryRequirements mr1; vkGetImageMemoryRequirements(A.dev,bridge_img1.img,&mr1);
                    VkMemoryWin32HandlePropertiesKHR wp1{}; wp1.sType=VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
                    if(!A.pfnGetMemWin32||A.pfnGetMemWin32(A.dev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,bridge_nt1,&wp1)!=VK_SUCCESS)
                        { std::printf("[ra] bridge[1]: vkGetMemoryWin32HandleProperties failed — falling back to sync present\n"); ok1=false; }
                    else {
                        const uint32_t mt1=pick_mem(A.mp,mr1.memoryTypeBits&wp1.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                        if(mt1==UINT32_MAX){ std::printf("[ra] bridge[1]: no device-local memory type — falling back to sync present\n"); ok1=false; }
                        else {
                            VkImportMemoryWin32HandleInfoKHR imp1{}; imp1.sType=VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
                            imp1.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT; imp1.handle=bridge_nt1;
                            VkMemoryDedicatedAllocateInfo ded1{}; ded1.sType=VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO; ded1.image=bridge_img1.img; imp1.pNext=&ded1;
                            VkMemoryAllocateInfo mai1{}; mai1.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai1.pNext=&imp1; mai1.allocationSize=mr1.size; mai1.memoryTypeIndex=mt1;
                            if(vkAllocateMemory(A.dev,&mai1,nullptr,&bridge_mem1)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkAllocateMemory(import) failed — falling back to sync present\n"); ok1=false; }
                            else vkBindImageMemory(A.dev,bridge_img1.img,bridge_mem1,0);
                        }
                    }
                }
            }
            // slot-1 cmd buffer + fence on A.
            if(ok1){
                VkCommandBufferAllocateInfo cbi1{}; cbi1.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cbi1.commandPool=A.pool; cbi1.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbi1.commandBufferCount=1;
                if(vkAllocateCommandBuffers(A.dev,&cbi1,&cmdBridge1)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkAllocateCommandBuffers failed — falling back to sync present\n"); ok1=false; }
            }
            if(ok1){
                VkFenceCreateInfo fi1{}; fi1.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                if(vkCreateFence(A.dev,&fi1,nullptr,&fBridge1)!=VK_SUCCESS){ std::printf("[ra] bridge[1]: vkCreateFence failed — falling back to sync present\n"); ok1=false; }
            }
            // slot-0's DEDICATED async cmd buffer + fence (A.pool / A.dev). Without these the async warp
            // would record into the shared cmdBridge/fBridge that wap_upload resets per-pair → in-flight
            // reset → GPU fault. With them, the async warp slots {A0, 1} are disjoint from wap_upload's
            // cmdBridge/fBridge.
            if(ok1){
                VkCommandBufferAllocateInfo cbiA{}; cbiA.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cbiA.commandPool=A.pool; cbiA.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbiA.commandBufferCount=1;
                if(vkAllocateCommandBuffers(A.dev,&cbiA,&cmdBridgeA0)!=VK_SUCCESS){ std::printf("[ra] bridge[A0]: vkAllocateCommandBuffers failed — falling back to sync present\n"); ok1=false; }
            }
            if(ok1){
                VkFenceCreateInfo fiA{}; fiA.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                if(vkCreateFence(A.dev,&fiA,nullptr,&fBridgeA0)!=VK_SUCCESS){ std::printf("[ra] bridge[A0]: vkCreateFence failed — falling back to sync present\n"); ok1=false; }
            }
            if(!ok1){
                // Tear down whatever slot-1 partially built; the teardown guards below also cover these, but
                // releasing now keeps the off-path state clean and avoids a stale half-slot living to teardown.
                if(bridge_img1.img){ vkDestroyImage(A.dev,bridge_img1.img,nullptr); bridge_img1.img=VK_NULL_HANDLE; }
                if(bridge_mem1){ vkFreeMemory(A.dev,bridge_mem1,nullptr); bridge_mem1=VK_NULL_HANDLE; }
                if(fBridge1){ vkDestroyFence(A.dev,fBridge1,nullptr); fBridge1=VK_NULL_HANDLE; }
                if(fBridgeA0){ vkDestroyFence(A.dev,fBridgeA0,nullptr); fBridgeA0=VK_NULL_HANDLE; }  // (cmdBridgeA0 is pool-freed)
                if(bridge_km_d3d1){ bridge_km_d3d1->Release(); bridge_km_d3d1=nullptr; }
                if(bridge_tex1){ bridge_tex1->Release(); bridge_tex1=nullptr; }
                if(bridge_nt1){ CloseHandle(bridge_nt1); bridge_nt1=nullptr; }
                cfg.async_present=false;   // force the off path → byte-identical to today
            } else {
                std::printf("[ra] present-surface: async-present slot-1 ready (2 bridge slots; non-blocking present + drop-interpolated)\n");
            }
        }
        // ── --real-fast-path co-arm guard ────────────────────────────────
        // --rfp presents the real through the DEDICATED async bslot[] path (the bslot[1] slot + cmdBridgeA0/
        // fBridgeA0), which exist ONLY when cfg.async_present is live. If async-present's slot-1 creation
        // FAILED above (cfg.async_present force-OFF at the failure path), the dedicated slot is gone and the
        // only present helper left is the SYNCHRONOUS do_present_P → resetting the SHARED cmdBridge/fBridge
        // would be a use-after-reset / device-lost. So REFUSE to arm --rfp when async is not live (mirror the
        // use_wap-requires disable pattern). The parse auto-enables async, so this only fires when the async
        // CREATE itself failed — a clean, printed refusal, never a crash.
        if(cfg.real_fast_path && !cfg.async_present){
            cfg.real_fast_path=false;
            std::printf("[ra] --real-fast-path: async-present is NOT live (slot-1 create failed) → DISABLING --rfp (the dedicated non-blocking bslot path is required; the synchronous present would risk a use-after-reset on the shared bridge buffers). Running the standard D-anchored present.\n");
        }
        // --motion-fallback co-arm guards — mirror the --rfp pattern: the fast-motion real fallback uses the
        // SAME dedicated non-blocking bslot path, so it needs async-present live (auto-enabled by the flag;
        // this only fires if the async CREATE failed) and the gme dispersion signal, and (like --rfp) it
        // cannot present an upscaled real on the bslot path. Disabling here is a clean printed refusal, never
        // a crash.
        if(cfg.motion_fallback && !cfg.async_present){
            cfg.motion_fallback=false;
            std::printf("[ra] --motion-fallback: async-present is NOT live (slot-1 create failed) -> DISABLING (the fast-motion real fallback needs the dedicated non-blocking bslot path).\n");
        }
        if(cfg.motion_fallback && !use_gme){
            cfg.motion_fallback=false;
            std::printf("[ra] --motion-fallback: needs the gme dispersion signal but --no-gme is set -> DISABLING.\n");
        }
        if(cfg.motion_fallback && use_upscale){
            cfg.motion_fallback=false;
            std::printf("[ra] --motion-fallback: --upscale is active -> DISABLING (the bslot real present is not upscaled, mirrors --rfp).\n");
        }
    return true;
}

// Made with my soul - Swately <3
