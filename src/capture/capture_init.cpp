// PhyriadFG — capture-side init (E1 of docs/planning/RESTRUCTURE_PLAN.md).
// Init-seq sections moved verbatim out of main.cpp behind ownership-struct binding preambles;
// bodies byte-identical to the pre-E1 sections except `goto done` -> `return false`.
#include "capture/wgc_ctx.hpp"   // FIRST: defines NOMINMAX before <windows.h> (winrt include order matters — same as main.cpp)
#include <algorithm>
#include <cstdio>
#include "core/app_init.hpp"
#include "core/compat_reason.hpp"   // ra::compat::emit (named compatibility-floor reasons)
#include "core/globals.hpp"          // g_quit — the source-lifecycle callbacks latch the clean-exit flag
#include "hdr_convert_spv.hpp"
#include "igpu_convert_pack_spv.hpp"
#include "unpack_packed_spv.hpp"
#include "igpu_field_spv.hpp"

// ── Convert pipeline (A: native→RGBA8 work) (E1: moved VERBATIM from main.cpp).
bool init_convert_pipe(DevicesInit& o_dev, ImagesInit& o_img, ConvertInit& o_cv){
    VDev& A=o_dev.A;
    auto& Anative=o_img.Anative; auto& Awork=o_img.Awork;
    auto& cvSamp=o_cv.cvSamp; auto& cvDsl=o_cv.cvDsl; auto& cvLayout=o_cv.cvLayout;
    auto& cvPipe=o_cv.cvPipe; auto& cvPool=o_cv.cvPool; auto& cvSet=o_cv.cvSet;
    // ── Convert pipeline (A: native→RGBA8 work) ───────────────────────────────
    {
        VkSamplerCreateInfo s{}; s.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; s.magFilter=VK_FILTER_LINEAR; s.minFilter=VK_FILTER_LINEAR; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(A.dev,&s,nullptr,&cvSamp);
        const VkDescriptorSetLayoutBinding bd[2]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&cvSamp},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=2; dl.pBindings=bd; vkCreateDescriptorSetLayout(A.dev,&dl,nullptr,&cvDsl);
        VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=12;   // {is_hdr,exposure,rot180}
        VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&cvDsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(A.dev,&pl,nullptr,&cvLayout);
        VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=kHdrConvertSpv.size()*sizeof(uint32_t); mci.pCode=kHdrConvertSpv.data(); VkShaderModule mod; vkCreateShaderModule(A.dev,&mci,nullptr,&mod);
        VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
        VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=cvLayout; vkCreateComputePipelines(A.dev,VK_NULL_HANDLE,1,&cp,nullptr,&cvPipe); vkDestroyShaderModule(A.dev,mod,nullptr);
        const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
        VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=2; pi.pPoolSizes=psz; vkCreateDescriptorPool(A.dev,&pi,nullptr,&cvPool);
        VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=cvPool; dai.descriptorSetCount=1; dai.pSetLayouts=&cvDsl; vkAllocateDescriptorSets(A.dev,&dai,&cvSet);
        VkDescriptorImageInfo si{}; si.sampler=cvSamp; si.imageView=Anative.view; si.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo di{}; di.imageView=Awork.view; di.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        const VkWriteDescriptorSet wds[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,cvSet,0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&si,nullptr,nullptr},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,cvSet,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&di,nullptr,nullptr}};
        vkUpdateDescriptorSets(A.dev,2,wds,0,nullptr);
        if(!cvPipe){ std::printf("[ra] convert pipeline failed\n"); return false; }
    }
    return true;
}

// ── iGPU convert+pack + B/G unpack pipelines — × kCapSlots (E1: moved VERBATIM from main.cpp).
bool init_igpu_pipes(Config& cfg, uint32_t NAT_W, uint32_t NAT_H, uint32_t nat_bpp,
                     uint32_t WW, uint32_t WH, DevicesInit& o_dev, HostBridgeInit& o_host,
                     ImagesInit& o_img, FlowPipesInit& o_flow, IgpuPipesInit& o_igpu){
    VDev& A=o_dev.A; VDev& B=o_dev.B; VDev& G=o_dev.G;
    auto& use_igpu_convert=o_dev.use_igpu_convert;
    auto& cap_slots=o_host.cap_slots; auto& Astage_g=o_host.Astage_g;
    auto& hRP_g=o_host.hRP_g; auto& hR_g=o_host.hR_g; auto& hRP_b_dev=o_host.hRP_b_dev;
    auto& hFIELD_g=o_host.hFIELD_g;
    auto& Bframe=o_img.Bframe; auto& Gsrc=o_img.Gsrc;
    auto& pfg_enabled=o_flow.pfg_enabled;
    auto& cpPipe=o_igpu.cpPipe; auto& fpipe=o_igpu.fpipe; auto& ubPipe=o_igpu.ubPipe; auto& ugPipe=o_igpu.ugPipe;
    // ── iGPU convert+pack + B/G unpack pipelines — × kCapSlots ─
    if(use_igpu_convert){
        const VkDeviceSize ab_g=VkDeviceSize(NAT_W)*NAT_H*nat_bpp;
        const VkDeviceSize rp_sz=VkDeviceSize(WW)*WH*3u;
        const VkDeviceSize hR_sz=VkDeviceSize(WW)*WH*4u;
        const std::vector<uint32_t> spvcp(kIgpuConvertPackSpv.begin(),kIgpuConvertPackSpv.end());
        const std::vector<uint32_t> spvup(kUnpackPackedSpv.begin(),kUnpackPackedSpv.end());
        VkImageView bviews[2]={Bframe[0].view,Bframe[1].view};
        VkImageView gv[1]={Gsrc.view};
        for(int _s=0;_s<cap_slots;++_s){
            if(!cpipe_create(G,Astage_g.buf,ab_g,hRP_g[_s].buf,rp_sz,hR_g[_s].buf,hR_sz,spvcp,cpPipe[_s]))
                { std::printf("[ra] igpu convpack[%d] failed\n",_s); return false; }
            if(!unpipe_create(B,hRP_b_dev[_s].buf,rp_sz,bviews,2,spvup,ubPipe[_s]))
                { std::printf("[ra] B unpack[%d] failed\n",_s); return false; }
            if(!unpipe_create(G,hRP_g[_s].buf,rp_sz,gv,1,spvup,ugPipe[_s]))
                { std::printf("[ra] G unpack[%d] failed\n",_s); return false; }
            if(cfg.igpu_field){   // the contour-field pipe (src=hR_g RGBA8, dst=hFIELD_g)
                const std::vector<uint32_t> spvfld(kIgpuFieldSpv.begin(),kIgpuFieldSpv.end());
                if(!fpipe_create(G,hR_g[_s].buf,hR_sz,hFIELD_g[_s].buf,hR_sz,spvfld,fpipe[_s]))
                    { std::printf("[ra] igpu field[%d] failed\n",_s); return false; }
            }
        }
        // Keep primary-FG when the user EXPLICITLY asked for it (--fg-gpu primary) AND device A has a real
        // 2nd queue (A.q2!=A.q, forced same-family via prefer_same_family_q2). F then routes its flow
        // submit to A.q2 (submit_wait_q2) → lock-free, no race with P on A.q; convert stays on the iGPU
        // (this block). Otherwise disable primary-FG.
        if(cfg.fg_gpu==FG_PRIMARY && A.q2!=A.q){
            std::printf("[ra] igpu-convert + --fg-gpu primary: primary-FG KEPT on A, flow routed to A.q2 (lock-free same-family partition; convert stays on iGPU)\n");
        } else {
            pfg_enabled=false;
            std::printf("[ra] igpu-convert: primary-FG disabled (4090 freed to capture+present only)\n");
        }
    }
    return true;
}

// ── WGC backend init (E1: moved VERBATIM from main.cpp). MSVC-only (C++/WinRT); the MinGW
// build never calls it (the parse guard already refused CA_WGC there).
#ifdef _MSC_VER
bool init_wgc_backend(Config& cfg, D3D& d, uint32_t NAT_W, uint32_t NAT_H, int cap_mon_hz,
                      HWND wgc_target_hwnd, ImagesInit& o_img, WgcCtx*& wgc_ctx){
    auto& dxgi_stage=o_img.dxgi_stage;
    // ── WGC backend init ─────────────────────────────────────────────────────
#ifdef _MSC_VER
    if(cfg.capture_api==CA_WGC){
        // Pre-flight WGC-availability probe. GraphicsCaptureSession::IsSupported() is false on builds <
        // 1803 (and in some locked-down policy states). There is no auto-descent ladder, so on an
        // unsupported OS we surface the NAMED reason and exit cleanly via the `goto done` path instead of
        // throwing deep in WGC init. Best-effort: any projection/throw here is treated as "could not
        // confirm support" and also surfaced (never silent).
        {
            bool wgc_supported=false;
            try { wgc_supported = wgc::GraphicsCaptureSession::IsSupported(); } catch(...) { wgc_supported=false; }
            if(!wgc_supported){
                ra::compat::emit(ra::compat::ReasonCode::WGC_UNSUPPORTED_OS);
                return false;   // clean cold-path exit (no descent ladder)
            }
        }
        // --copy-device: optionally create a SEPARATE D3D11 device on the SAME adapter as d.dev for the
        // WGC copy queue. cap_dev/cap_ctx alias d.dev/d.ctx when OFF (every WGC-path device reference below
        // is then the d.dev/d.ctx code → byte-identical-off). When ON, they point at the 2nd device so the
        // CopyResource runs on its OWN command queue, decoupled from the game's saturated d.dev queue.
        // SAME-adapter is REQUIRED (a cross-adapter copy of the WGC surface would need shared-resource
        // keyed-mutex machinery — out of scope, crash-class); we create on d.luid's adapter.
        ID3D11Device*        cap_dev=d.dev;
        ID3D11DeviceContext* cap_ctx=d.ctx;
        if(cfg.copy_device){
            IDXGIFactory1* _cfac=nullptr; IDXGIAdapter1* _cad=nullptr;
            if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&_cfac)) && _cfac){
                for(UINT _i=0;_cfac->EnumAdapters1(_i,&_cad)==S_OK;++_i){
                    DXGI_ADAPTER_DESC1 _ad{}; _cad->GetDesc1(&_ad);
                    if(_ad.AdapterLuid.LowPart==d.luid.LowPart && _ad.AdapterLuid.HighPart==d.luid.HighPart) break;
                    rel(_cad); _cad=nullptr;
                }
            }
            ID3D11Device* _cdev=nullptr; ID3D11DeviceContext* _cctx=nullptr; D3D_FEATURE_LEVEL _cfl{};
            // DRIVER_TYPE_UNKNOWN when an explicit adapter is passed (D3D11CreateDevice contract); BGRA_SUPPORT
            // to match d3d_init (WGC delivers BGRA8). Same-adapter guarantees CopyResource(ring,tex) is intra-device.
            HRESULT _chr = D3D11CreateDevice(_cad, _cad?D3D_DRIVER_TYPE_UNKNOWN:D3D_DRIVER_TYPE_HARDWARE,
                nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &_cdev, &_cfl, &_cctx);
            rel(_cad); rel(_cfac);
            if(SUCCEEDED(_chr) && _cdev && _cctx){
                // Enable D3D11Multithread so the FrameArrived callback (CopyResource/Signal) and the
                // C-thread (Map/Unmap) can share this immediate context safely.
                { ID3D11Multithread* _mt=nullptr; if(SUCCEEDED(_cctx->QueryInterface(__uuidof(ID3D11Multithread),(void**)&_mt))&&_mt){ _mt->SetMultithreadProtected(TRUE); _mt->Release(); } }
                cap_dev=_cdev; cap_ctx=_cctx; // recorded on wgc_ctx (cdev/cctx) after the new WgcCtx() below

                std::printf("[ra] --copy-device: separate D3D11 device ARMED on the capture adapter — the WGC CopyResource queue is decoupled from d.dev.\n");
            } else {
                if(_cctx){ _cctx->Release(); } if(_cdev){ _cdev->Release(); }
                cfg.copy_device=false;   // FORCED OFF → cap_dev/cap_ctx stay d.dev/d.ctx → byte-identical
                std::printf("[ra] --copy-device: 2nd D3D11 device on the capture adapter unavailable (hr=0x%08lX) — FORCED OFF, the copy runs on d.dev (byte-identical).\n",(unsigned long)_chr);
            }
        }
        winrt::com_ptr<IDXGIDevice> dxgi_dev_wgc;
        cap_dev->QueryInterface(dxgi_dev_wgc.put());
        winrt::com_ptr<IInspectable> d3d_insp;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_dev_wgc.get(),d3d_insp.put()));
        auto d3d_winrt=d3d_insp.as<wd3d::IDirect3DDevice>();

        auto interop=winrt::get_activation_factory<wgc::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
        wgc::GraphicsCaptureItem cap_item{nullptr};
        // CreateForWindow/CreateForMonitor throw winrt::hresult_error on failure (e.g. a window destroyed
        // between match and create, a monitor that vanished, or an OS-policy refusal). main() has no
        // top-level catch, so an escape here would abort with a generic crash — a silent failure with NO
        // named reason. Catch it on this cold path, surface CAPTURE_INIT_FAILED, and exit cleanly via the
        // `goto done` cleanup (forward jump out of this scope runs the locals' destructors).
        try {
            if(wgc_target_hwnd){   // RESOLVED, not merely requested -- --window-pid/--hwnd bind with no title
                winrt::check_hresult(interop->CreateForWindow(wgc_target_hwnd,
                    winrt::guid_of<wgc::GraphicsCaptureItem>(),winrt::put_abi(cap_item)));
                {   // Report the window we RESOLVED, not the argument we were given: with
                    // --window-pid the title argument is a tie-break and may not describe it at all.
                    wchar_t _wt[256]={}; GetWindowTextW(wgc_target_hwnd,_wt,256);
                    char _t8[512]={}; WideCharToMultiByte(CP_UTF8,0,_wt,-1,_t8,(int)sizeof(_t8)-1,nullptr,nullptr);
                    DWORD _tp=0; GetWindowThreadProcessId(wgc_target_hwnd,&_tp);
                    std::printf("[ra] WGC: capturing window '%s' (pid %lu)\n",_t8,(unsigned long)_tp);
                }
                if(cfg.dpi_probe){ auto _sz=cap_item.Size(); std::printf("[ra] --dpi-probe: cap_item.Size()=%dx%d (WGC's OWN physical window size — the DPI-independent source of truth)\n",_sz.Width,_sz.Height); }
            } else {
                winrt::check_hresult(interop->CreateForMonitor(d.cap_hmon,
                    winrt::guid_of<wgc::GraphicsCaptureItem>(),winrt::put_abi(cap_item)));
                std::printf("[ra] WGC: capturing monitor %d\n",cfg.cap_mon);
            }
        } catch(const winrt::hresult_error& e){
            std::printf("[ra] WGC GraphicsCaptureItem create FAILED (hr=0x%08lX)\n",(unsigned long)e.code().value);
            ra::compat::emit(ra::compat::ReasonCode::CAPTURE_INIT_FAILED);
            return false;
        }

        wgc_ctx=new WgcCtx();
        // --copy-device: record the 2nd device on the ctx (null when off). cap_dev/cap_ctx already alias
        // d.dev/d.ctx when off → the ring-alloc + teardown below are byte-identical in the off case.
        if(cfg.copy_device){ wgc_ctx->cdev=cap_dev; wgc_ctx->cctx=cap_ctx; }
        // Fill the staging ring; ring[0] reuses the already-created dxgi_stage (off path). With --copy-device
        // ON the ring lives on the 2nd device (CopyResource must be intra-device with the WGC surface that
        // device owns), so ALL slots — including ring[0] — are fresh d3d_staging_on(cap_dev) textures;
        // dxgi_stage (on d.dev) stays allocated but unused by WGC (released at teardown). The off path is
        // unchanged.
        if(cfg.copy_device){
            for(uint32_t _i=0;_i<WgcCtx::RING_N;++_i) wgc_ctx->ring[_i]=d3d_staging_on(cap_dev,d.fmt,NAT_W,NAT_H);
        } else {
            wgc_ctx->ring[0]=dxgi_stage;
            for(uint32_t _i=1;_i<WgcCtx::RING_N;++_i) wgc_ctx->ring[_i]=d3d_staging(d,NAT_W,NAT_H);
        }
        winrt::Windows::Graphics::SizeInt32 pool_sz{(int32_t)NAT_W,(int32_t)NAT_H};
        // bufferCount 6. Under a saturated 4090, WGC's GPU copy takes ~25 ms; with only 2 pool slots and
        // frames every ~11 ms, slot #3 finds the pool full → WGC drops it silently (FrameArrived never
        // fires) → arr ceiling ~57/90 fps. 6 slots absorb up to ~66 ms copy backlog before any WGC-level
        // drop — lifting the ceiling to the source fps. (When that still binds, --copy-device gives the
        // CopyResource its own queue, separate from WGC's internal copies.)
        wgc_ctx->pool=wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            d3d_winrt,wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized,6,pool_sz);

        // --copy-fence init: the D3D11.4 capability probe. Query ID3D11Device5 (CreateFence) +
        // ID3D11DeviceContext4 (Signal) + an auto-reset event from the CAPTURE device/context (cap_dev/cap_ctx
        // — == d.dev/d.ctx when --copy-device off, the 2nd device when on; the fence MUST be created+signaled
        // on the SAME device that issues the CopyResource). If ANY step fails → release partials and FORCE
        // cfg.copy_fence=false → the C-thread + callback both take the byte-identical Map-retry path.
        if(cfg.copy_fence){
            bool cf_ok=false;
            ID3D11Device5* dev5=nullptr;
            if(SUCCEEDED(cap_dev->QueryInterface(__uuidof(ID3D11Device5),(void**)&dev5)) && dev5){
                if(SUCCEEDED(dev5->CreateFence(0,D3D11_FENCE_FLAG_NONE,__uuidof(ID3D11Fence),(void**)&wgc_ctx->copyFence)) && wgc_ctx->copyFence){
                    if(SUCCEEDED(cap_ctx->QueryInterface(__uuidof(ID3D11DeviceContext4),(void**)&wgc_ctx->ctx4)) && wgc_ctx->ctx4){
                        wgc_ctx->copyEvt=CreateEventEx(nullptr,nullptr,0,EVENT_ALL_ACCESS); // auto-reset (no MANUAL_RESET flag)
                        if(wgc_ctx->copyEvt) cf_ok=true;
                    }
                }
            }
            dev5 && (dev5->Release(),true);   // release the temporary dev5 query ref (the fence keeps the device alive)
            if(cf_ok){
                std::printf("[ra] --copy-fence: event-driven WGC copy-completion pickup ARMED (D3D11.4 fence+event).\n");
            } else {
                // Fallback: release whatever was created, force OFF → byte-identical Map-retry path.
                if(wgc_ctx->copyEvt){ CloseHandle(wgc_ctx->copyEvt); wgc_ctx->copyEvt=nullptr; }
                if(wgc_ctx->ctx4){ wgc_ctx->ctx4->Release(); wgc_ctx->ctx4=nullptr; }
                if(wgc_ctx->copyFence){ wgc_ctx->copyFence->Release(); wgc_ctx->copyFence=nullptr; }
                cfg.copy_fence=false;
                std::printf("[ra] --copy-fence: D3D11.4 (ID3D11Device5/Context4) or event unavailable — FORCED OFF, the Map-retry busy-poll path runs (byte-identical).\n");
            }
        }

        // FrameArrived callback — CopyResource to next ring slot only. Map/memcpy run in the main loop;
        // D3D11Multithread (d3d_init) makes both sides safe. Captures pointers by value to survive teardown
        // ordering.
        // --copy-device: raw_ctx is the CAPTURE context — cap_ctx (== d.ctx when off, the 2nd device when
        // on). The callback's CopyResource + the C-thread Map (below) both go through the SAME device, so the
        // 2nd device's copy queue is self-consistent. When off this is exactly d.ctx → byte-identical callback.
        ID3D11DeviceContext* raw_ctx=cap_ctx;
        WgcCtx* raw_wctx=wgc_ctx;
        const bool lt_on=cfg.latency_trace;   // captured once → byte-identical callback when off
        const bool cf_on=cfg.copy_fence;      // captured once (post-probe) → zero work in the callback when off
        const bool dp_on=cfg.dpi_probe;       // --dpi-probe: one-shot first-frame ContentSize log (default off → dead in the callback → byte-identical)
        wgc_ctx->pool.FrameArrived([raw_wctx,raw_ctx,lt_on,cf_on,dp_on](auto& p,auto&){
            if(!raw_wctx->running.load()) return;
            auto frame=p.TryGetNextFrame(); if(!frame) return;
            if(dp_on){ static bool _dp_once=false; if(!_dp_once){ _dp_once=true; auto _cs=frame.ContentSize(); std::printf("[ra] --dpi-probe: first frame.ContentSize()=%dx%d (what WGC ACTUALLY delivers — the non-circular truth vs the pool size)\n",_cs.Width,_cs.Height); } }
            // ── I-3 / B-1 / E-4 — MID-RUN SOURCE-RESIZE DETECT (detect-and-quit; MINIMAL fix) ──────
            // The whole pipeline is sized ONCE at init: NAT_W/NAT_H (capture_init.cpp:424, const) feed
            // the staging ring (:188/:191), the WGC pool (:193/:199-200), the Vulkan images and every
            // flow/warp extent. WGC frames are always POOL-sized, so a grown window is CROPPED and a
            // shrunk one is parked top-left with stale margins — wrong content, no crash, no log line,
            // for the rest of the run. There is no re-init path (see the deferred full fix), so the
            // honest small fix is to detect the change and exit cleanly with a named reason.
            //
            // WHY THE BASELINE IS THE FIRST FRAME AND NOT pool_sz: WGC's own idea of the source size can
            // legitimately differ from the GetClientRect that sized the pool — that gap IS the known
            // startup high-DPI crop this file's own --dpi-probe (:166, :418) exists to expose. Comparing
            // against pool_sz would fire on frame 1 and refuse to run at all on those rigs. Comparing
            // against the first DELIVERED frame detects the CHANGE, which is the actual defect.
            // Cost: one ContentSize() property read per delivered frame (<=240/s) — a getter on an
            // already-materialized frame; there is no cheaper way to see the change.
            {
                const auto _sz=frame.ContentSize();
                const int32_t _bw=raw_wctx->base_w.load();
                if(_bw==0){
                    raw_wctx->base_w.store(_sz.Width); raw_wctx->base_h.store(_sz.Height);
                } else if(_sz.Width>_bw || _sz.Height>raw_wctx->base_h.load()){
                    // EXCEEDS the geometry everything downstream was sized for. WGC clamps content to
                    // the pool, so the extra pixels are not delivered at all: every remaining frame is
                    // CROPPED, silently. That is the defect, and there is no re-init path — exit.
                    raw_wctx->size_changed.store(true);
                    if(!raw_wctx->bail_said.exchange(true)){
                        std::printf("[ra] captured source GREW %dx%d -> %dx%d — beyond the size the pipeline was built for; every further frame would be silently cropped. Exiting cleanly; restart at the new size.\n",
                                    _bw,raw_wctx->base_h.load(),_sz.Width,_sz.Height);
                        ra::compat::emit(ra::compat::ReasonCode::SOURCE_RESIZED);
                    }
                    g_quit=true;
                    return;   // do NOT copy a frame whose content no longer matches the ring geometry
                } else if(_sz.Width!=_bw || _sz.Height!=raw_wctx->base_h.load()){
                    // SMALLER than the baseline, in one or both axes. Nothing is cropped and nothing
                    // reads out of bounds — the valid content simply occupies less of a pool-sized
                    // texture, leaving a stale margin. Killing the run over this was wrong, and it is
                    // not a theoretical case: the operator lost a session to 1920x1080 -> 1920x1079,
                    // a ONE-PIXEL flutter Chrome produces on its own while relaying out. Say it once
                    // and keep generating frames.
                    if(!raw_wctx->shrink_said.exchange(true)){
                        std::printf("[ra] captured source is now %dx%d (was %dx%d) — smaller than the size the pipeline was built for, so a %dx%d margin may hold stale pixels. Continuing; restart to resize the pipeline.\n",
                                    _sz.Width,_sz.Height,_bw,raw_wctx->base_h.load(),
                                    _bw-_sz.Width,raw_wctx->base_h.load()-_sz.Height);
                    }
                }
            }
            auto surface=frame.Surface();
            auto acc=surface.try_as<IDirect3DDxgiInterfaceAccess>(); if(!acc) return;
            winrt::com_ptr<ID3D11Texture2D> tex;
            acc->GetInterface(IID_PPV_ARGS(tex.put())); if(!tex) return;
            // Plain seq_cst atomics throughout (≤240 ops/s — ordering cost is nil; explicit
            // memory_order_* lives in framework/hal/ only, per the lint_hal rule).
            const uint32_t w=raw_wctx->ring_write.load();
            const uint32_t r=raw_wctx->ring_read.load();
            if(w-r>=WgcCtx::RING_N){ ++raw_wctx->arrived; ++raw_wctx->ringfull; return; }  // ring full; drop frame (ringfull counts the SILENT drop arrived++ masks)
            raw_ctx->CopyResource(raw_wctx->ring[w%WgcCtx::RING_N],tex.get());
            ++raw_wctx->ring_write;
            // --copy-fence: enqueue a GPU-timeline Signal AFTER the CopyResource for slot w%N. The value is
            // w+1 (== ring_write post-increment) so the C-thread, seeing ring_write==W, can wait fence>=W for
            // the newest slot (W-1)%N whose copy was signaled w+1==W. One fire-and-return enqueue on the
            // already-multithread-protected context (≤125/s) — NEVER blocks the winrt thread.
            // + Flush(): SUBMIT the copy+signal now. Sin él, ambos quedan en el command buffer del
            // runtime sin enviar (nada más lo envía — el render es Vulkan) → el fence NUNCA alcanza
            // el target antes del primer Map; el flush implícito del Map FALLIDO era quien lo enviaba
            // → mapmiss == arr, un miss por frame (medido). El Flush es un submit no-bloqueante,
            // ≤source-fps/s, seguro en el hilo winrt. Gated en cf_on → path copy-fence-off byte-idéntico.
            if(cf_on && raw_wctx->ctx4 && raw_wctx->copyFence){
                raw_wctx->ctx4->Signal(raw_wctx->copyFence,(UINT64)(w+1u));
                raw_ctx->Flush();
            }
            ++raw_wctx->arrived;
            // --latency-trace: stamp the WGC-ring slot just written (w%RING_N). submit = steady now (C
            // computes copy-exec = tcap−submit); compose = the QPC delta WGC-compose→this-callback (both
            // QPC → clock-safe). All in DOUBLE ms (no int overflow / no __int128). If the QPC↔
            // SystemRelativeTime epochs do NOT match, the delta is out-of-band → stored 0 (= invalid, the
            // stats then show compose=0, flagging the assumption failed). Off (lt_on=false) → zero work.
            if(lt_on){
                raw_wctx->ring_submit_us[w%WgcCtx::RING_N].store((uint64_t)(now_ms()*1000.0));
                static LARGE_INTEGER s_qf=[]{ LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
                LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
                const double qpc_ms = s_qf.QuadPart>0 ? (double)qpc.QuadPart*1000.0/(double)s_qf.QuadPart : 0.0;
                const double srt_ms = (double)frame.SystemRelativeTime().count()/10000.0;   // 100ns → ms
                const double dcm = qpc_ms - srt_ms;
                raw_wctx->ring_compose_us[w%WgcCtx::RING_N].store((dcm>0.0&&dcm<200.0)?(uint64_t)(dcm*1000.0):0);
            }
            // (PLL units fix) arr_delta_us ya NO se escribe aquí por-ENTREGA: el delta se mide en la
            // INGESTA post-dedup (capture.cpp, cola compartida) para que un diluvio de duplicados
            // (MinUpdateInterval bajo) no envenene la EMA de cadencia del PLL.
            raw_wctx->frame_ready.store(true);
        });

        // ── B-6 — SOURCE-DEATH DETECTOR for WGC (covers the MONITOR path the watchdog cannot) ────
        // present.cpp:1950's window-death watchdog is gated on `if(wgc_target_hwnd)`, and
        // present_stage.cpp:45 confirms that handle is NULL in monitor mode — so when a captured
        // DISPLAY is removed, frames stop, f_seq stalls and P re-presents the stale pair forever with
        // unbounded latency, which is precisely the failure the window watchdog was written to fix.
        // GraphicsCaptureItem::Closed fires for BOTH shapes (window destroyed / display removed), so
        // ONE registration beside the FrameArrived one closes the gap without touching the window path.
        // The item is stored on the ctx first: the local cap_item dies with this function, and the
        // registration must outlive it. Teardown sets running=false BEFORE session.Close()
        // (main.cpp:1113-1114), so a Closed fired by our own teardown is correctly ignored.
        wgc_ctx->item=cap_item;
        {
            WgcCtx* raw_wctx_c=wgc_ctx;
            wgc_ctx->item.Closed([raw_wctx_c](auto&,auto&){
                if(!raw_wctx_c->running.load()) return;   // our own teardown closes it — not a death
                raw_wctx_c->source_closed.store(true);
                if(!raw_wctx_c->bail_said.exchange(true)){
                    std::printf("[ra] capture source CLOSED (window destroyed or captured display removed) — exiting cleanly\n");
                    ra::compat::emit(ra::compat::ReasonCode::SOURCE_CLOSED);
                }
                g_quit=true;
            });
        }
        wgc_ctx->session=wgc_ctx->pool.CreateCaptureSession(cap_item);
        try { wgc_ctx->session.IsBorderRequired(false); } catch(...) {}  // Win11 22621+ only
        // Do NOT bake the cursor into captured frames — a captured (delayed) cursor would float
        // mid-screen even while the game hides the real one. With capture off, the REAL hardware cursor
        // draws above the overlay and shows/hides exactly as the game commands.
        try { wgc_ctx->session.IsCursorCaptureEnabled(false); } catch(...) {}
        // WGC's default MinUpdateInterval (~16.6 ms) caps delivery at ~60/s; Win11 24H2+ exposes the
        // knob. A low interval makes WGC deliver DUPLICATE frames in micro-bursts up to the compose
        // rate. Históricamente eso ENVENENABA la EMA de cadencia (los deltas de ráfaga colapsaban
        // T_ema → presents back-to-back → MAILBOX se quedaba solo con el real) y por eso el default
        // era un cap fijo de 8 ms (~125/s). Ese veneno está CURADO: el PLL se alimenta de deltas
        // medidos en la INGESTA post-dedup (capture.cpp), así que con --dedup armado los duplicados
        // se descartan y el delta cuenta solo únicos → el intervalo se DERIVA del panel capturado:
        // MEDIO período de composición (cap_mon_hz, detectado arriba — margen anti-cuantización para
        // que el floor nunca estrangule la entrega a tasa de composición). 240Hz→~2.1ms (entrega
        // 240/s), 500Hz→1ms (500/s), 1000Hz→0.5ms floor. NUNCA un hardcode — escala con el monitor
        // del usuario. Clamp [0.5ms, 8ms]: el techo = el viejo default (paneles ≤62Hz, continuidad).
        // SIN --dedup el diluvio SÍ entraría al pipeline (pares de movimiento-cero) → se conserva 8ms.
        try {
            // --cap-fps N throttles the source to N fps (MinUpdateInterval = 1/N, in 100ns units);
            // default (cap_fps==0): medio período del panel capturado con --dedup; 8ms sin él.
            const long long half_period_100ns = 10000000LL / (2LL*(long long)(cap_mon_hz>0?cap_mon_hz:60));
            const long long mui_dedup = half_period_100ns<5000LL?5000LL:(half_period_100ns>80000LL?80000LL:half_period_100ns);
            const long long mui_100ns = (cfg.cap_fps > 0) ? (10000000LL / (long long)cfg.cap_fps)
                                       : (cfg.dedup ? mui_dedup : 80000LL);
            wgc_ctx->session.MinUpdateInterval(winrt::Windows::Foundation::TimeSpan{mui_100ns});
            wgc_ctx->mui_100ns.store(mui_100ns);   // I-8: the LIVE cadence base, so the 1 Hz monitor re-check re-applies only a CHANGED value
            if (cfg.cap_fps > 0)
                std::printf("[ra] WGC MinUpdateInterval: %.1f ms (--cap-fps %d) — source throttled so the FG interpolates more frames/pair (crescent easier to eyeball)\n", (double)mui_100ns/10000.0, cfg.cap_fps);
            else if (cfg.dedup)
                std::printf("[ra] WGC MinUpdateInterval: %.2f ms (half the %d Hz capture-monitor period) — delivery up to the compose rate; --dedup filters the duplicates, the PLL is fed post-dedup (units-consistent)\n", (double)mui_100ns/10000.0, cap_mon_hz);
            else
                std::printf("[ra] WGC MinUpdateInterval: 8 ms requested (~125/s cap; anti-duplicate-flood — use --dedup to unlock compose-rate delivery)\n");
        } catch(...) { std::printf("[ra] WGC MinUpdateInterval: unavailable (pre-24H2) — default ~60/s cap\n"); }
        wgc_ctx->session.StartCapture();
        std::printf("[ra] WGC session started — frames arriving via free-threaded callback\n");
        // ── I-6 — FIRST-FRAME TIMEOUT ────────────────────────────────────────────────────────────
        // Before this, EVERY no-frames cause left the process idling forever with no named reason: the
        // C-thread's spin (capture.cpp:418-419) is bounded but its enclosing loop is not, and
        // present.cpp:1953's death watchdog needs !IsWindow, which stays TRUE for a live-but-silent
        // source. So the run printed 'cap 0/s' every stat line and never terminated. Wait BOUNDED for
        // the first FrameArrived — the callback bumps `arrived` before any drop path can swallow the
        // frame — and quit with a named reason if none comes. This catches the iconic case the guard
        // above cannot see (a window restored between the IsIconic check and StartCapture), an OS
        // capture-policy refusal, and a source on a GPU this session cannot be fed from.
        // Healthy path: WGC delivers on session start, so this costs one 5 ms sleep at most and is silent.
        {
            const int kFirstFrameMs=5000;   // generous: a busy compositor, or a --cap-fps 1 throttle
            int _waited=0;
            while(wgc_ctx->arrived.load()==0 && !g_quit && _waited<kFirstFrameMs){ Sleep(5); _waited+=5; }
            if(wgc_ctx->arrived.load()==0 && !g_quit){
                std::printf("[ra] WGC delivered NO frame in %d ms — the source is not producing (minimized, capture-policy blocked, or on a GPU WGC will not deliver from)\n",kFirstFrameMs);
                ra::compat::emit(ra::compat::ReasonCode::NO_FRAMES);
                return false;   // clean cold-path exit — main.cpp:519 takes `goto done`, which tears the ctx down
            }
            if(_waited>=250) std::printf("[ra] WGC first frame took %d ms\n",_waited);
        }
    }
#endif
    return true;
}

// ── I-8 — slow-cadence re-check of the CAPTURED WINDOW'S MONITOR (WGC window path only) ────────
// THE HONEST SPLIT between the two halves of this finding:
//
//   CHEAP — implemented here. WGC's CreateForWindow follows the window across monitors BY ITSELF, so
//   the CONTENT stays correct; the only thing that goes stale is the delivery-cadence base, because
//   MinUpdateInterval was derived ONCE (capture_init.cpp:312-316) from the panel the window started on
//   and cap_mon_hz (:474-478) is never revisited. Re-deriving it costs a MonitorFromWindow +
//   GetMonitorInfo + EnumDisplaySettingsEx and a LIVE property set on the session that already exists.
//   Nothing is reallocated, nothing is re-created, and while the window stays put the whole call is one
//   MonitorFromWindow that returns the cached HMONITOR and bails.
//
//   NOT CHEAP — deliberately NOT implemented, and this is the honest half. Following the window on the
//   DDA path means releasing the duplication, re-DuplicateOutput on a new output index (d.cap_ci is the
//   index dda_rearm persists), and accepting that the new panel may have a DIFFERENT RESOLUTION — which
//   walks straight into the same wall as I-3/B-1/B-4: NAT_W/NAT_H, the staging ring, the Vulkan images
//   and every flow/warp extent are frozen at init and there is no re-init path. Doing it "cheaply"
//   would produce exactly the silent CopyResource-size-mismatch stall B-4 documents. DDA is also behind
//   the non-default --capture-api dd and is monitor-scoped by construction (cli.cpp:784). So the DDA
//   half stays deferred, named here rather than pretended away.
//
// Called at ~1 Hz from the present loop. io_hmon/io_hz are the caller's remembered state.
void wgc_recheck_window_monitor(Config& cfg, HWND hwnd, WgcCtx* ctx, HMONITOR& io_hmon, int& io_hz){
    if(!hwnd || !ctx || cfg.capture_api!=CA_WGC) return;
    const HMONITOR hm=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
    if(!hm || hm==io_hmon) return;              // the window has not changed panel — this is the whole cost
    io_hmon=hm;
    MONITORINFOEXA mi{}; mi.cbSize=sizeof(mi);
    int hz=0;
    if(GetMonitorInfoA(hm,(LPMONITORINFO)&mi)){
        DEVMODEA dm{}; dm.dmSize=sizeof(dm);
        if(EnumDisplaySettingsExA(mi.szDevice,ENUM_CURRENT_SETTINGS,&dm,0)) hz=(int)dm.dmDisplayFrequency;
    }
    if(hz<=0) return;                           // could not read the new panel — leave the cadence alone
    io_hz=hz;
    // The SAME derivation as the init site (capture_init.cpp:312-316) — one formula, kept identical:
    // --cap-fps wins; else HALF the captured panel's period with --dedup, clamped [0.5ms, 8ms]; else the
    // historical 8 ms floor. With --cap-fps set or --dedup off the value does not depend on the panel,
    // so the compare below correctly makes this a no-op instead of a misleading re-apply.
    const long long half_period_100ns = 10000000LL / (2LL*(long long)hz);
    const long long mui_dedup = half_period_100ns<5000LL?5000LL:(half_period_100ns>80000LL?80000LL:half_period_100ns);
    const long long mui_100ns = (cfg.cap_fps > 0) ? (10000000LL / (long long)cfg.cap_fps)
                               : (cfg.dedup ? mui_dedup : 80000LL);
    if(mui_100ns==ctx->mui_100ns.load()){
        std::printf("[ra] captured window moved to a %d Hz panel — the WGC cadence base is unchanged (%.2f ms; --cap-fps / no --dedup pin it)\n",
                    hz,(double)mui_100ns/10000.0);
        return;
    }
    try {
        ctx->session.MinUpdateInterval(winrt::Windows::Foundation::TimeSpan{mui_100ns});
        ctx->mui_100ns.store(mui_100ns);
        std::printf("[ra] captured window moved to a %d Hz panel — WGC MinUpdateInterval re-applied: %.2f ms (delivery cadence follows the window)\n",
                    hz,(double)mui_100ns/10000.0);
    } catch(...) {
        std::printf("[ra] captured window moved to a %d Hz panel — MinUpdateInterval unavailable (pre-24H2); delivery cadence unchanged\n",hz);
    }
}
#endif
// ── Capture source init (E1: moved VERBATIM from main.cpp's pre-goto init head).
// DDA/WGC route selection, dimension derivation (NAT/WW/flow/warp), the captured-monitor
// refresh probe and the FG-marker banner. Returns -1 to continue; >=0 is main()'s exit code
// (the original section's early `return 0/1` paths, verbatim).
int init_capture_source(Config& cfg, CaptureSrcInit& o_cap){
#ifdef _MSC_VER
    auto& wgc_target_hwnd=o_cap.wgc_target_hwnd;
#endif
    D3D& d=o_cap.d;
    auto& pres_outputs=o_cap.pres_outputs;
    // ── WGC state ─────────────────────────────────────────────────────────────
    // (wgc_ctx / wgc_target_hwnd now live in CaptureSrcInit — bound by the alias preamble.)
#ifdef _MSC_VER
#endif

    // ── Capture init ─────────────────────────────────────────────────────────
    // (D3D d — now o_cap.d, bound by the preamble above.)
#ifdef _MSC_VER
    // Ruta DDA explícita (--capture-api dd, ya no el default): --window captura el MONITOR completo
    // donde está la ventana, vía DDA (llega al refresh del panel; el default WGC con --dedup también
    // llega — MinUpdateInterval derivado del panel capturado; sin --dedup WGC conserva su cap de 8ms).
    if (cfg.capture_api==CA_DD && wants_window_target(cfg)) {
        // --present-own-window con DDA: persistimos el HWND del juego en wgc_target_hwnd para que
        // present.cpp lo pase como psd.game_hwnd → el yield del plano OwnWindow lo reconoce como "frente
        // válido" y NO se esconde cuando el juego está al frente. Da el pid correcto al CSV. El watchdog de
        // muerte-de-ventana es seguro en DDA (la captura del escritorio no se estanca al cerrar la ventana).
        wgc_target_hwnd=find_window_by_substr(cfg.window_substr, (DWORD)cfg.window_pid, (HWND)(uintptr_t)cfg.window_hwnd);
        if (wgc_target_hwnd) {
            int _oi=d3d_output_index_for_monitor(MonitorFromWindow(wgc_target_hwnd,MONITOR_DEFAULTTONEAREST));
            if (_oi>=0) { cfg.cap_mon=_oi; std::printf("[ra] --window '%s' -> DDA en su monitor (salida %d)\n",cfg.window_substr,_oi); }
            else std::printf("[ra] --window '%s': no mapea a una salida DXGI del adaptador de captura (¿otra GPU?) — usando --monitor %d (DDA)\n",cfg.window_substr,cfg.cap_mon);
        } else {
            // L-3 / E-5: this was a bare printf that fell through, so a stale --window title silently captured
            // --monitor 0 -- with tgt_pid 0 in the CSV (present.cpp gates GetWindowThreadProcessId on this HWND)
            // and the window-death watchdog disabled (present.cpp gates it on `if(wgc_target_hwnd)`), while the
            // WGC branch below emits WINDOW_NOT_FOUND and bails on the identical condition. Emit the named reason
            // ALWAYS so the launcher's compat surface and the operator see the target was never bound, and bail
            // like WGC -- EXCEPT when --monitor was explicit, which is the one case where whole-monitor DDA
            // capture of a named output is what was asked for rather than a degradation.
            std::printf("[ra] --window '%s': ninguna ventana visible coincide (DDA)\n",cfg.window_substr);
            ra::compat::emit(ra::compat::ReasonCode::WINDOW_NOT_FOUND);
            if(!cfg.cap_mon_explicit){ d3d_shutdown(d); return 1; }
            std::printf("[ra] --monitor %d was explicit: continuing as WHOLE-MONITOR DDA capture with NO window binding (no target pid in the CSV, no window-death watchdog)\n",cfg.cap_mon);
        }
    }
#endif
    const bool want_dd = (cfg.capture_api == CA_DD);
    // --gpu-priority LEVER 3: +7 GPU thread priority on the D3D11 CAPTURE device (the WGC copy
    // path — the INVISIBLE staging CopyResource measured 3-8ms behind a saturated game queue).
    if (!d3d_init(d, cfg.cap_mon, want_dd, /*gpu_thread_prio=*/cfg.gpu_priority ? 7 : 0)) {
        std::printf("[ra] capture output %d unavailable (api=%s)\n",cfg.cap_mon,want_dd?"dd":"wgc");
        // The generic capture-init failure carries a NAMED reason. On a Microsoft-Hybrid laptop dGPU the
        // DD path fails by design (DuplicateOutput → UNSUPPORTED); surface that floor specifically so the
        // user knows to switch to WGC, else the generic reason.
        ra::compat::emit(want_dd && d.dev /*device made, duplication failed*/
                         ? ra::compat::ReasonCode::HYBRID_DD_WRONG_GPU
                         : ra::compat::ReasonCode::CAPTURE_INIT_FAILED);
        d3d_shutdown(d); return 1;
    }
    // Enumerate ALL DXGI adapters to build pres_outputs (present candidates from every GPU).
    // (pres_outputs — now o_cap.pres_outputs, bound by the preamble above.)
    { IDXGIFactory1* fac=nullptr;
      if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&fac))){
        for(UINT _ai=0;;++_ai){ IDXGIAdapter* _oad=nullptr; if(fac->EnumAdapters(_ai,&_oad)!=S_OK) break;
            DXGI_ADAPTER_DESC _ad2{}; _oad->GetDesc(&_ad2);
            char _aname[128]={}; WideCharToMultiByte(CP_ACP,0,_ad2.Description,-1,_aname,sizeof(_aname),nullptr,nullptr);
            for(UINT _oi=0;;++_oi){ IDXGIOutput* _o=nullptr; if(_oad->EnumOutputs(_oi,&_o)!=S_OK) break;
                DXGI_OUTPUT_DESC _od{}; _o->GetDesc(&_od); OutInfo _info;
                WideCharToMultiByte(CP_ACP,0,_od.DeviceName,-1,_info.name,sizeof(_info.name),nullptr,nullptr);
                _info.coords=_od.DesktopCoordinates; _info.attached=(_od.AttachedToDesktop!=0); _info.hmon=_od.Monitor;
                std::snprintf(_info.adapter_name,sizeof(_info.adapter_name),"%s",_aname);
                { DEVMODEA _dm{}; _dm.dmSize=sizeof(_dm); if(EnumDisplaySettingsExA(_info.name,ENUM_CURRENT_SETTINGS,&_dm,0)) _info.hz=(int)_dm.dmDisplayFrequency; }
                pres_outputs.push_back(_info); rel(_o); }
            rel(_oad); }
        rel(fac); }
      if(pres_outputs.empty()) pres_outputs=d.outputs; }
    std::printf("[render_assistant] adapter: %s | present candidates:\n",d.adapter);
    for (size_t i=0;i<pres_outputs.size();++i)
        std::printf("  [%zu] %-14s %4ldx%-4ld @ (%5ld,%4ld) %dHz  [%s]%s\n",i,pres_outputs[i].name,
            pres_outputs[i].coords.right-pres_outputs[i].coords.left,pres_outputs[i].coords.bottom-pres_outputs[i].coords.top,
            pres_outputs[i].coords.left,pres_outputs[i].coords.top,pres_outputs[i].hz,
            pres_outputs[i].adapter_name,pres_outputs[i].attached?"":" [detached]");
    if (cfg.list_only) { d3d_shutdown(d); return 0; }
    // ── B-7: pres_outputs is indexed UNGUARDED downstream (main.cpp: `pres_outputs[cfg.pres_mon].coords`,
    // std::vector::operator[]), and nothing upstream range-tested the index it inherited. Three guards:
    // an empty enumeration bails with a named reason; an out-of-range --present-monitor is REPORTED and
    // demoted to "unset" instead of silently becoming something else; an out-of-range --monitor is
    // reported here, where d.outputs is finally known (d3d_init cannot reject it on the WGC path --
    // capture.cpp returns true on device-created alone, and capture_init force-sets d.fmt below, which
    // bypasses route_for, the only guard that used to catch it).
    if (pres_outputs.empty()) {
        std::printf("[ra] no present candidates enumerated (no attached DXGI output on any adapter)\n");
        ra::compat::emit(ra::compat::ReasonCode::CAPTURE_INIT_FAILED); d3d_shutdown(d); return 1;
    }
    if (cfg.pres_mon>=(int)pres_outputs.size()) {
        std::printf("[ra] --present-monitor %d out of range (%zu present candidates) — resolving from the capture/game monitor instead\n",cfg.pres_mon,pres_outputs.size());
        cfg.pres_mon=-1;
    }
    if (cfg.cap_mon<0 || cfg.cap_mon>=(int)d.outputs.size())
        std::printf("[ra] WARNING: --monitor %d is not an output index on the capture adapter (%zu outputs) — capture dimensions and the present-monitor derivation fall back\n",cfg.cap_mon,d.outputs.size());
    // pres_mon itself is resolved AFTER the --window block below: on the WGC path the target window's
    // HMONITOR is not known until then, and the resolution keys on that HMONITOR, never on an index
    // copied across index spaces (see the block after #endif).
    // WDA_EXCLUDEFROMCAPTURE on the pillar's HWND makes capture+present on one output
    // feedback-free unconditionally.

    // WGC window capture: adjust d.w/d.h from target window's client rect before route_for.
#ifdef _MSC_VER
    if(cfg.capture_api==CA_WGC && wants_window_target(cfg)){
        wgc_target_hwnd=find_window_by_substr(cfg.window_substr, (DWORD)cfg.window_pid, (HWND)(uintptr_t)cfg.window_hwnd);
        if(!wgc_target_hwnd){std::printf("[ra] --window: no visible window matching '%s'\n",cfg.window_substr);ra::compat::emit(ra::compat::ReasonCode::WINDOW_NOT_FOUND);d3d_shutdown(d);return 1;}   // named reason on the no-match bail
        // ── I-6 / B-3 / E-4b — a MINIMIZED target is FATAL, not "keep the monitor size" ──────────
        // The finder's only filter is IsWindowVisible (capture.cpp:94), and WS_VISIBLE survives
        // minimize — so an iconic window MATCHES. Its client rect is empty, and the guard below used to
        // have NO else: d.w/d.h silently kept the whole-monitor values d3d_init wrote (capture.cpp:50-51),
        // sizing the staging ring, the WGC pool and every downstream extent to the monitor and parking
        // the window's content in the top-left corner (the same corner-zoom --dpi-probe names). Bail with
        // a named reason instead, in exactly the shape of the null-HWND bail on the line above.
        if(IsIconic(wgc_target_hwnd)){
            std::printf("[ra] --window '%s': the matched window is MINIMIZED — restore it and start again\n",cfg.window_substr);
            ra::compat::emit(ra::compat::ReasonCode::SOURCE_MINIMIZED); d3d_shutdown(d); return 1;
        }
        RECT cr{}; GetClientRect(wgc_target_hwnd,&cr);
        if(cr.right>cr.left&&cr.bottom>cr.top){d.w=(uint32_t)(cr.right-cr.left);d.h=(uint32_t)(cr.bottom-cr.top);}
        else {
            // Degenerate rect from any cause (iconic-but-not-IsIconic, a shell window, a failed call):
            // inheriting the monitor size here is what makes the corner-zoom look like a healthy run.
            std::printf("[ra] --window '%s': GetClientRect is empty (%ldx%ld) — refusing to inherit the monitor size\n",
                        cfg.window_substr,cr.right-cr.left,cr.bottom-cr.top);
            ra::compat::emit(ra::compat::ReasonCode::SOURCE_MINIMIZED); d3d_shutdown(d); return 1;
        }
        d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM; // WGC always delivers BGRA8
        if(cfg.dpi_probe){ UINT _dpi=GetDpiForWindow(wgc_target_hwnd); std::printf("[ra] --dpi-probe: GetClientRect=%ldx%ld  GetDpiForWindow=%u (scale %.2fx) — this feeds NAT_W/NAT_H + the WGC pool today\n",cr.right-cr.left,cr.bottom-cr.top,_dpi,_dpi/96.0); }
    }
#endif
    // ── The present monitor, resolved by HMONITOR (I-4 / B-8 / B-7) ───────────────────────────────
    // THREE index spaces meet here and an integer must never cross between them:
    //   (1) cfg.cap_mon  indexes d.outputs      — the CAPTURE adapter's EnumOutputs order;
    //   (2) cfg.pres_mon indexes pres_outputs   — EnumAdapters x EnumOutputs over EVERY adapter;
    //   (3) PresentSurface::pick_monitor(n)     — EnumDisplayMonitors order with index 0 FORCED to the
    //       primary monitor (PresentSurface.cpp mon_enum).
    // The old `cfg.pres_mon=cfg.cap_mon` copied (1) into (2) and then handed the result to (3), and the
    // DDA branch's window->monitor derivation was never mirrored on the DEFAULT WGC path, so "present on
    // the game's monitor" was true only on a single-monitor rig. Resolve on the HMONITOR instead: the
    // --window target's monitor when one is bound and --monitor was not explicit, else the captured
    // output's monitor. cfg.pres_hmon then carries the exact handle to the presenter, which bypasses (3).
    {
        HMONITOR _want=nullptr; const char* _src="captured output";
#ifdef _MSC_VER
        if(wgc_target_hwnd && !cfg.cap_mon_explicit){ _want=MonitorFromWindow(wgc_target_hwnd,MONITOR_DEFAULTTONEAREST); _src="--window target's monitor"; }
#endif
        if(!_want){ _want=d.cap_hmon; _src="captured output"; }
        if(cfg.pres_mon>=0) _src="--present-monitor (explicit)";
        else {
            cfg.pres_mon=0;   // floor: pres_outputs is non-empty (guarded above), so [0] is always valid
            bool _hit=false;
            if(_want) for(size_t _i=0;_i<pres_outputs.size();++_i) if(pres_outputs[_i].hmon==_want){ cfg.pres_mon=(int)_i; _hit=true; break; }
            if(!_hit) _src="fallback [0] — no present candidate matches that HMONITOR";
        }
        cfg.pres_hmon=(void*)pres_outputs[cfg.pres_mon].hmon;
        std::printf("[ra] present monitor: [%d] %s %dHz  <- %s\n",cfg.pres_mon,pres_outputs[cfg.pres_mon].name,pres_outputs[cfg.pres_mon].hz,_src);
    }
    VkFormat nat_vkfmt=VK_FORMAT_UNDEFINED; uint32_t nat_bpp=0; Route route=RT_NONE; const char* rdesc="";
    if (!route_for(d.fmt,nat_vkfmt,nat_bpp,route,rdesc)) { std::printf("[ra] unsupported capture format DXGI=%d\n",(int)d.fmt); ra::compat::emit(ra::compat::ReasonCode::UNSUPPORTED_FORMAT); d3d_shutdown(d); return 1; }   // named reason on the unsupported-format bail
    const bool IS_HDR=(route==RT_HDR);
    const uint32_t NAT_W=d.w, NAT_H=d.h;
    const uint32_t WW=std::min(NAT_W,1920u), WH=std::min(NAT_H,1080u);
    const uint32_t UP_W=WW*2, UP_H=WH*2;
    // The flow-resolution DRS divisor. The OFP pyramid keeps the coarsest level ≥ kMinCoarse=32px
    // by halving (n_levels capped at kLevels=4 → coarsest ≈ WW_flow/8), so WW_flow ≥ 256 keeps a full
    // 4-level pyramid valid. Clamp the divisor DOWN (toward 1) if WW/N or WH/N would fall below 256
    // on a small capture (e.g. a 640×360 window) — the dial degrades, never breaks the matcher. At
    // flow_scale==1 (default) flow_div==1 and WW_flow==WW (byte-identical).
    // --flow-scale auto (the adaptive quality<->latency knob): pick the LOWEST flow_div (best quality) whose flow-work
    // pixels fit the --flow-scale-target-mp budget. Resolution-adaptive (the flow cost is ~pixel-count-bound). Picked at
    // STARTUP (the flow pipeline is init-sized → no runtime re-init). WAP-only (the non-WAP/primary-FG paths consume
    // full-res). The clamp below still applies (under-feeding the OFP pyramid). OFF (flow_scale_auto false) → byte-identical.
    if(cfg.flow_scale_auto && cfg.warp_at_presenter){
        const double tgt=(double)cfg.flow_scale_target_mp*1.0e6;
        uint32_t d=4u; for(uint32_t cand:{1u,2u,4u}){ if((double)(WW/cand)*(double)(WH/cand)<=tgt){ d=cand; break; } }
        cfg.flow_scale=(int)d;
        std::printf("[ra] --flow-scale auto -> %u (target %.2f MP; work %ux%u -> flow %ux%u). Adaptive quality<->latency; override --flow-scale N.\n",d,cfg.flow_scale_target_mp,WW,WH,WW/d,WH/d);
    } else if(cfg.flow_scale_auto){
        std::printf("[ra] --flow-scale auto ignored — requires warp-at-presenter (the default). Reverting to flow-scale 1.\n");
        cfg.flow_scale=1;
    }
    uint32_t flow_div=(uint32_t)cfg.flow_scale;
    while(flow_div>1u && (WW/flow_div<256u || WH/flow_div<256u)) flow_div>>=1u;   // 4→2→1
    if(flow_div!=(uint32_t)cfg.flow_scale)
        std::printf("[ra] --flow-scale %d clamped to %u — WW/N or WH/N < 256 would under-feed the OFP pyramid (capture %ux%u, work %ux%u)\n",cfg.flow_scale,flow_div,NAT_W,NAT_H,WW,WH);
    const uint32_t WW_flow=WW/flow_div, WH_flow=WH/flow_div;   // the flow's (down)sampled work size
    // The warp-output divisor (fixed at init, like flow_div). Threads into the wapOutA EXTENT, the
    // warp dispatch dims, the --afill in-place dispatch, the wapOutA→bridge upscale-blit src, the
    // --ts-smooth history (wapPrevOutA + its copy extent), and the --outdump/--qdump readback. At
    // warp_scale==1 (default) warp_div==1 ⇒ WW_warp==WW, WH_warp==WH ⇒ every site byte-identical.
    // Clamp DOWN (toward 1) if WW/N or WH/N would be degenerate (<8 — below one 8×8 workgroup tile the
    // dispatch+blit lose meaning), mirroring the flow_div pyramid-floor clamp above. WAP-only (the
    // parse-time guard already reverted N>1 when WAP is off).
    uint32_t warp_div=(uint32_t)cfg.warp_scale;
    while(warp_div>1u && (WW/warp_div<8u || WH/warp_div<8u)) warp_div>>=1u;   // 4→2→1
    if(warp_div!=(uint32_t)cfg.warp_scale)
        std::printf("[ra] --warp-scale %d clamped to %u — WW/N or WH/N < 8 (below one 8x8 workgroup) on a tiny capture (work %ux%u)\n",cfg.warp_scale,warp_div,WW,WH);
    const uint32_t WW_warp=WW/warp_div, WH_warp=WH/warp_div;   // OUR warp's (down)scaled output/dispatch size

    std::printf("[ra] capture [%d]: %ux%u DXGI=%d → %s → work %ux%u\n",cfg.cap_mon,NAT_W,NAT_H,(int)d.fmt,rdesc,WW,WH);

    // ── Output clock: DERIVED from the present monitor, not hard-coded (C-5) ──────────────────────
    // cli.hpp's refresh_hz=240 was the only value the output clock ever used (its single assignment was
    // --refresh-hz), while present.cpp printed "present cadence = the panel" -- a claim nothing measured.
    // docs/research/ongoing/STAGE39_OUTPUT_CLOCK_DESIGN.md:252 specified the else-branch that was never
    // implemented: read the PRESENT monitor's refresh from the STAGE-33 enumeration unless --refresh-hz
    // overrides. EnumDisplaySettingsEx reports an INTEGER rate (a 143.97Hz mode reports 143), so
    // --refresh-hz remains the exact-grid override and the source is printed rather than asserted.
    if(!cfg.refresh_hz_set){
        const int _phz=pres_outputs[cfg.pres_mon].hz;
        if(_phz>=20){ cfg.refresh_hz=_phz; cfg.refresh_hz_from_panel=true; }
        else std::printf("[ra] present monitor [%d] reports no usable refresh (%d Hz) — output clock stays at the %d Hz built-in default (--refresh-hz overrides)\n",cfg.pres_mon,_phz,cfg.refresh_hz);
    }
    // '--target-output-fps auto' resolved against the PROVISIONAL parse-time refresh_hz; re-resolve it now
    // that the real one is known, else the cap would silently sit at a rate the clock no longer runs at.
    if(cfg.target_output_fps_auto && cfg.target_output_fps!=(float)cfg.refresh_hz){
        std::printf("[ra] --target-output-fps auto: re-resolved %.0f -> %d (the derived output clock)\n",cfg.target_output_fps,cfg.refresh_hz);
        cfg.target_output_fps=(float)cfg.refresh_hz;
    }

    // ── Refresh del monitor CAPTURADO — la base de escalado de la ingesta ─────
    // El techo físico de entrega de cualquier captura por composición (WGC/DDA) = la tasa de
    // composición del panel capturado. DERIVADO de la enumeración ya hecha (OutInfo.hz vía
    // EnumDisplaySettingsEx), nunca un hardcode — escala solo a paneles 240/360/500Hz+.
    // Preferencia: el monitor de la VENTANA objetivo (--window) > el output DDA elegido >
    // cfg.refresh_hz (el flag --refresh-hz). Consumido por el ring de captura y por el
    // MinUpdateInterval de WGC. (Declarado AQUÍ, antes de todo goto de teardown — C2362.)
    int cap_mon_hz = 0; const char* cap_mon_src = "--refresh-hz fallback";
#ifdef _MSC_VER
    if(wgc_target_hwnd){
        const HMONITOR _hm=MonitorFromWindow(wgc_target_hwnd,MONITOR_DEFAULTTONEAREST);
        for(const auto& _oi:d.outputs) if(_oi.hmon==_hm && _oi.hz>0){ cap_mon_hz=_oi.hz; cap_mon_src="window monitor"; break; }
    }
#endif
    if(cap_mon_hz<=0 && d.cap_ci>=0 && d.cap_ci<(int)d.outputs.size() && d.outputs[d.cap_ci].hz>0){
        cap_mon_hz=d.outputs[d.cap_ci].hz; cap_mon_src="DDA output";
    }
    if(cap_mon_hz<=0) cap_mon_hz=cfg.refresh_hz;
    std::printf("[ra] capture-monitor refresh: %d Hz (%s) — the ingest-scaling base (capture ring + WGC MinUpdateInterval)\n",cap_mon_hz,cap_mon_src);

    // The commit marker gains the appearance band when appearance is live (commit governs):
    //   commit:0.080(real appear:0.10) / commit:0.080(real) / commit:0.080(appear:0.10) / commit:0.080
    char commit_buf[56]={};
    if(cfg.commit_thresh>0.f){
        char appear_sub[24]={};   // "appear:0.10" prefixed with a space iff (real) also present
        if(cfg.appearance) std::snprintf(appear_sub,sizeof(appear_sub),"%sappear:%.2f",cfg.commit_real?" ":"",cfg.appear_band);
        const bool paren = cfg.commit_real || cfg.appearance;   // (real appear:0.10) / (real) / (appear:0.10)
        std::snprintf(commit_buf,sizeof(commit_buf)," commit:%.3f%s%s%s%s",
                      cfg.commit_thresh,
                      paren?"(":"", cfg.commit_real?"real":"", appear_sub, paren?")":"");
    }
    char bidir_buf[40]={}; if(cfg.bidir) std::snprintf(bidir_buf,sizeof(bidir_buf)," bidir(occl:%.2fpx%s)",cfg.occl_thresh,cfg.phase_anchor?"+pa":"");   // +pa = phase-anchored primary MV
    char filldiv_buf[40]={}; if(cfg.fill_div) std::snprintf(filldiv_buf,sizeof(filldiv_buf)," fill-div(eps:%.3f)",cfg.div_eps);
    char mvguided_buf[40]={}; if(cfg.mv_guided) std::snprintf(mvguided_buf,sizeof(mvguided_buf)," mv-guided(sim:%.2f)",cfg.mv_sim);
    char mesnap_buf[56]={}; if(cfg.mv_edge_snap) std::snprintf(mesnap_buf,sizeof(mesnap_buf)," mv-edge-snap(%s sim:%.2f)",cfg.mv_edge_snap==1?"G1-dis":"G2-col",cfg.mv_edge_snap_sim>0.f?cfg.mv_edge_snap_sim:cfg.mv_sim);
    char strack_buf[24]={}; if(cfg.single_track) std::snprintf(strack_buf,sizeof(strack_buf),cfg.st_no_stasis?" single-track(v3.0)":" single-track(v3.2)");   // v3 pre-store B-track override (v3.2 = +per-pixel screen-static evidence mix, the shipping default; v3.0 = pure blend-solo-2 mirror)
    char bgrec_buf[32]={}; if(cfg.bg_reclaim>0.f) std::snprintf(bgrec_buf,sizeof(bgrec_buf)," bg-reclaim(str:%.2f)",cfg.bg_reclaim);   // tile-level gravity fix (marker ⇔ armed; carries the strength)
    char matte_buf[112]={}; if(cfg.matte) std::snprintf(matte_buf,sizeof(matte_buf)," matte(thr:%.2f occ-lerp mass-k:%.2f%s%s%s%s%s)",cfg.matte_thresh,cfg.mass_k,cfg.travel?" travel":"",cfg.crescent?" crescent":"",cfg.contour?" contour":"",cfg.obj_crescent?" objcres":"",cfg.member_commit?" mbb":"");
    char object_buf[80]={}; if(cfg.objects) std::snprintf(object_buf,sizeof(object_buf)," objects(k:%d min:%d inh:%.0fpx shield:%s%s%s)",kObjSlots,kObjMinMass,kObjInhMin,cfg.persist_reset?"hud-only":"per",cfg.shapefield?" shape":"",cfg.scene_memory?" mem:0.55":"");
    char stasis_buf[40]={}; if(cfg.stasis) std::snprintf(stasis_buf,sizeof(stasis_buf)," stasis(thr:%.2f)",cfg.stasis_thresh);
    char inertia_buf[40]={}; if(cfg.inertia) std::snprintf(inertia_buf,sizeof(inertia_buf)," inertia(thr:%.2f)",cfg.inertia_thresh);
    // --mv-guided supersedes --mv-median's blind pass, so suppress the mv-median marker when guided
    // is on (the consensus pass runs color-weighted, not blind — one marker, no contradiction).
    char gme_buf[32]={}; if(cfg.gme) std::snprintf(gme_buf,sizeof(gme_buf)," gme%s%s",cfg.change_gate?"+chg":"",cfg.ambig?"+ambig":"");   // +chg = the change gate, +ambig = the second-best SAD-tie arbitration (gme child)
    // The commit-default marker ` cdef` rides NEXT TO the commit marker (the warp-vs-blend selection
    // rule, sibling to the commit's averaging-vs-commit). WARP-LEVEL (matte-independent), gated on
    // wap → printed when cfg.commit_default (` cdef` present ⇔ the flip is live this run).
    // The one-position marker ` 1pos` rides NEXT TO ` cdef` (sibling warp-vs-blend cure — the
    // warp_result composition collapse). WARP-LEVEL (matte-independent), gated on wap → printed when
    // cfg.onepos (` 1pos` present ⇔ the collapse is live this run).
    // The 1pos marker carries the band when ≠1 (the dial visible in every log).
    char onepos_buf[24]={}; if(cfg.onepos){ if(cfg.onepos_band!=1.0f) std::snprintf(onepos_buf,sizeof(onepos_buf)," 1pos:%.2f",cfg.onepos_band); else std::snprintf(onepos_buf,sizeof(onepos_buf)," 1pos"); }
    std::printf("[ra] FG: res_ceil=%.1f conf_improv=%.2f agreement=%.2f%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",cfg.res_ceil,cfg.conf_improv,cfg.agreement,cfg.warp_at_presenter?" wap":"",cfg.soft_gate?" soft":"",cfg.mv_prior?" mv-prior":"",commit_buf,cfg.commit_default?" cdef":"",onepos_buf,bidir_buf,filldiv_buf,cfg.rescue?" rescue":"",(cfg.mv_median&&!cfg.mv_guided)?" mv-median":"",mvguided_buf,mesnap_buf,strack_buf,bgrec_buf,gme_buf,matte_buf,object_buf,stasis_buf,inertia_buf,cfg.expire?" expire":"");   // FG markers
    // E1 export epilogue — the section's formerly-main-local derived values, published for the
    // init chain, the threads (via the FgContext aggregate) and the teardown.
    o_cap.want_dd=want_dd; o_cap.IS_HDR=IS_HDR;
    o_cap.nat_vkfmt=nat_vkfmt; o_cap.nat_bpp=nat_bpp; o_cap.route=route; o_cap.rdesc=rdesc;
    o_cap.NAT_W=NAT_W; o_cap.NAT_H=NAT_H; o_cap.WW=WW; o_cap.WH=WH; o_cap.UP_W=UP_W; o_cap.UP_H=UP_H;
    o_cap.flow_div=flow_div; o_cap.WW_flow=WW_flow; o_cap.WH_flow=WH_flow;
    o_cap.warp_div=warp_div; o_cap.WW_warp=WW_warp; o_cap.WH_warp=WH_warp;
    o_cap.cap_mon_hz=cap_mon_hz; o_cap.cap_mon_src=cap_mon_src;
    return -1;
}
// Made with my soul - Swately <3
