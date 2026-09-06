// PhyriadFG capture layer. Bodies of the CAPTURE-side D3D11/DXGI interop + iGPU convert/unpack
// factories declared in capture/capture.hpp. The find-window internals (WndFind / enum_wnd_cb)
// stay `static`; the main-called factories get external linkage. No *_spv.hpp includes (factories
// take the SPIR-V as a param).
#include "capture/capture.hpp"
#include "ingest/ingest.hpp"   // R6 step 1: STAGE 2 (the convert + publish, and the --ingest-async worker)
#include "ingest/frames.hpp"   // R7: RawRing::publish -- the acquire side of the 1->2 ring
#include <d3d11_4.h>   // ID3D11Multithread (ring: shared immediate-context protection)
#include <cstring>     // std::strstr (find_window_by_substr / enum_wnd_cb)
#include "core/fg_context.hpp"   // FgContext (the C-thread's shared main()-locals as refs)
#include <cstdio>                 // std::printf (C-thread body)
#include <cmath>                  // std::sqrt (igpu-field-verify CPU oracle)
#include <chrono>                 // std::chrono::milliseconds (the convert-worker wait_for timeout)

// want_dup=false: skip DuplicateOutput (WGC path); returns true if D3D11 device created.
bool d3d_init(D3D& d,int ci,bool want_dup,int gpu_thread_prio){
    d.cap_ci=ci;   // persist the chosen output index for dda_rearm()
    D3D_FEATURE_LEVEL fl;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d.dev,&fl,&d.ctx))) return false;
    // Enable D3D11 multithread protection so callback (CopyResource) and main loop
    // (Map/Unmap) can share the immediate context safely (WGC = MSVC only).
#ifdef _MSC_VER
    { ID3D11Multithread* _mt=nullptr; if(SUCCEEDED(d.ctx->QueryInterface(__uuidof(ID3D11Multithread),(void**)&_mt))&&_mt){ _mt->SetMultithreadProtected(TRUE); _mt->Release(); } }
#endif
    IDXGIDevice* dxgi=nullptr; d.dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);
    // --gpu-priority LEVER 3: raise the CAPTURE device's GPU thread priority (IDXGIDevice range
    // [-7,+7]) so the WGC staging CopyResource (measured 3-8ms INVISIBLE under a saturated game)
    // schedules ahead of the game's queue. Honest print either way; failure is never fatal.
    if(gpu_thread_prio && dxgi){
        const HRESULT phr=dxgi->SetGPUThreadPriority(gpu_thread_prio);
        if(SUCCEEDED(phr)) std::printf("[ra] --gpu-priority: capture device SetGPUThreadPriority(%+d) OK (lever 3 ACTIVE)\n",gpu_thread_prio);
        else               std::printf("[ra] --gpu-priority: capture device SetGPUThreadPriority(%+d) FAILED (hr=0x%08lX) — continuing (lever 3 inactive)\n",gpu_thread_prio,(unsigned long)phr);
    }
    IDXGIAdapter* ad=nullptr; dxgi->GetAdapter(&ad); DXGI_ADAPTER_DESC adesc{}; ad->GetDesc(&adesc); d.luid=adesc.AdapterLuid;
    WideCharToMultiByte(CP_ACP,0,adesc.Description,-1,d.adapter,sizeof(d.adapter),nullptr,nullptr);
    for(UINT i=0;;++i){ IDXGIOutput* o=nullptr; if(ad->EnumOutputs(i,&o)!=S_OK)break; DXGI_OUTPUT_DESC od{}; o->GetDesc(&od);
        OutInfo info; WideCharToMultiByte(CP_ACP,0,od.DeviceName,-1,info.name,sizeof(info.name),nullptr,nullptr);
        info.coords=od.DesktopCoordinates; info.attached=(od.AttachedToDesktop!=0); info.hmon=od.Monitor; info.rot=(int)od.Rotation;
        // Refresh rate: the visibility ceiling of any present on this output (MAILBOX shows at most hz fps).
        { DEVMODEA dm{}; dm.dmSize=sizeof(dm); if(EnumDisplaySettingsExA(info.name,ENUM_CURRENT_SETTINGS,&dm,0)) info.hz=(int)dm.dmDisplayFrequency; }
        d.outputs.push_back(info);
        if((int)i==ci){
            d.cap_hmon=od.Monitor;
            if(want_dup){
                IDXGIOutput1* o1=nullptr; o->QueryInterface(__uuidof(IDXGIOutput1),(void**)&o1);
                if(o1&&o1->DuplicateOutput(d.dev,&d.dup)==S_OK){DXGI_OUTDUPL_DESC dd{}; d.dup->GetDesc(&dd); d.w=dd.ModeDesc.Width; d.h=dd.ModeDesc.Height; d.fmt=dd.ModeDesc.Format; d.cap_rot=(int)dd.Rotation;}
                rel(o1);
            } else {
                // WGC path: get dimensions from output rect; WGC delivers BGRA8
                d.w=(uint32_t)(od.DesktopCoordinates.right-od.DesktopCoordinates.left);
                d.h=(uint32_t)(od.DesktopCoordinates.bottom-od.DesktopCoordinates.top);
                d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM;
                d.cap_rot=(int)od.Rotation;
            }
        }
        rel(o); }
    rel(ad); rel(dxgi); return want_dup ? (d.dup!=nullptr) : (d.dev!=nullptr);
}

// Re-arm the DXGI output duplication after it dies. On Win11 (esp. 24H2) a fullscreen transition, a
// display-mode change, or an MPO plane re-plan returns DXGI_ERROR_ACCESS_LOST from AcquireNextFrame and
// INVALIDATES the IDXGIOutputDuplication; without re-arming, the capture thread would spin forever on the
// corpse and the FG freezes mid-game. We release the dead dup and re-DuplicateOutput on the SAME (persisted)
// output index, refreshing dims/format in case the mode changed. Does NOT recreate the D3D11 device — a
// genuine DEVICE_REMOVED (TDR) needs a full restart (rare; caller logs + keeps retrying).
bool dda_rearm(D3D& d){
    rel(d.dup);                                   // drop the dead duplication (rel() is null-safe)
    if(!d.dev || d.cap_ci<0) return false;
    IDXGIDevice* dxgi=nullptr;
    if(FAILED(d.dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi))||!dxgi) return false;
    IDXGIAdapter* ad=nullptr; dxgi->GetAdapter(&ad); rel(dxgi);
    if(!ad) return false;
    bool ok=false; IDXGIOutput* o=nullptr;
    if(ad->EnumOutputs((UINT)d.cap_ci,&o)==S_OK && o){
        IDXGIOutput1* o1=nullptr; o->QueryInterface(__uuidof(IDXGIOutput1),(void**)&o1);
        if(o1 && o1->DuplicateOutput(d.dev,&d.dup)==S_OK && d.dup){
            DXGI_OUTDUPL_DESC dd{}; d.dup->GetDesc(&dd);
            d.w=dd.ModeDesc.Width; d.h=dd.ModeDesc.Height; d.fmt=dd.ModeDesc.Format; d.cap_rot=(int)dd.Rotation;
            ok=true;
        }
        rel(o1); rel(o);
    }
    rel(ad);
    return ok;
}

// Find the first visible window whose title contains substr (EnumWindows helper).
// WGC-only (--window): MSVC path. Guarded so the mingw DD-only build stays warning-clean.
#ifdef _MSC_VER
struct WndFind { const char* substr; HWND found; };
static BOOL CALLBACK enum_wnd_cb(HWND h,LPARAM lp){
    WndFind* f=reinterpret_cast<WndFind*>(lp);
    char title[256]={}; GetWindowTextA(h,title,sizeof(title));
    if(IsWindowVisible(h)&&title[0]&&std::strstr(title,f->substr)){f->found=h;return FALSE;}
    return TRUE;
}
HWND find_window_by_substr(const char* substr){
    WndFind f{substr,nullptr}; EnumWindows(enum_wnd_cb,reinterpret_cast<LPARAM>(&f)); return f.found;
}
// Map an HMONITOR to the DXGI output INDEX on the capture (primary) adapter (for --window → DDA-on-its-monitor).
// Creates a throwaway D3D11 device just to enumerate the primary adapter's outputs; matches by HMONITOR.
int d3d_output_index_for_monitor(HMONITOR hm){
    if(!hm) return -1;
    ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr; D3D_FEATURE_LEVEL fl;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,&fl,&ctx))) return -1;
    IDXGIDevice* dxgi=nullptr; dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);
    IDXGIAdapter* ad=nullptr; if(dxgi) dxgi->GetAdapter(&ad);
    int idx=-1;
    if(ad){ for(UINT i=0;;++i){ IDXGIOutput* o=nullptr; if(ad->EnumOutputs(i,&o)!=S_OK) break;
        DXGI_OUTPUT_DESC od{}; o->GetDesc(&od); rel(o); if(od.Monitor==hm){ idx=(int)i; break; } } }
    rel(ad); rel(dxgi); rel(ctx); rel(dev);
    return idx;
}
#endif // _MSC_VER
void d3d_shutdown(D3D& d){rel(d.dup);rel(d.ctx);rel(d.dev);}
ID3D11Texture2D* d3d_staging(D3D& d,uint32_t w,uint32_t h){ D3D11_TEXTURE2D_DESC td{}; td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.Format=d.fmt;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ; ID3D11Texture2D* t=nullptr; d.dev->CreateTexture2D(&td,nullptr,&t); return t; }
// --copy-device: same staging texture, but created on an EXPLICIT device + format (the WGC ring on
// the 2nd copy device). fmt is d.fmt (WGC BGRA8) — passed so this helper does not depend on the D3D struct.
ID3D11Texture2D* d3d_staging_on(ID3D11Device* dev,DXGI_FORMAT fmt,uint32_t w,uint32_t h){ D3D11_TEXTURE2D_DESC td{}; td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.Format=fmt;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ; ID3D11Texture2D* t=nullptr; dev->CreateTexture2D(&td,nullptr,&t); return t; }

// iGPU convert+pack pipeline (3 SSBO bindings: src=Astage, dst=hostRP, rgba=hostR).
// rgba buf (binding 2): iGPU writes RGBA8 to hostR for A-present + no-upscale path.
bool cpipe_create(VDev& d,VkBuffer src,VkDeviceSize src_b,VkBuffer dst,VkDeviceSize dst_b,VkBuffer rgba,VkDeviceSize rgba_b,const std::vector<uint32_t>& spv,ConvPackPipe& p){
    const VkDescriptorSetLayoutBinding bd[3]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=3; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=24; // {groups,is_bgra,px,is_hdr,exposure,rot180}
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[1]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=1; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorBufferInfo bi0{}; bi0.buffer=src;  bi0.offset=0; bi0.range=src_b;
    VkDescriptorBufferInfo bi1{}; bi1.buffer=dst;  bi1.offset=0; bi1.range=dst_b;
    VkDescriptorBufferInfo bi2{}; bi2.buffer=rgba; bi2.offset=0; bi2.range=rgba_b;
    const VkWriteDescriptorSet wds[3]={
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bi0,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bi1,nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,2,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bi2,nullptr}};
    vkUpdateDescriptorSets(d.dev,3,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void cpipe_destroy(VDev& d,ConvPackPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); p=ConvPackPipe{}; }

// packed → RGBA8 unpack pipeline (1 SSBO src + up to 2 storage image dsts).
// On B: 2 sets (one per Bframe slot). On G: 1 set (Gsrc for upscale real-frame path).
bool unpipe_create(VDev& d,VkBuffer src,VkDeviceSize src_b,VkImageView* dst_views,uint32_t n,const std::vector<uint32_t>& spv,UnpackPipe& p){
    if(n>2) n=2;
    const VkDescriptorSetLayoutBinding bd[2]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=2; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.size=8; // {W,H}
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; pl.pushConstantRangeCount=1; pl.pPushConstantRanges=&pcr; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,n},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,n}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=n; pi.poolSizeCount=2; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetLayout dsls[2]={p.dsl,p.dsl};
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=n; dai.pSetLayouts=dsls; vkAllocateDescriptorSets(d.dev,&dai,p.sets);
    p.nsets=n;
    VkDescriptorBufferInfo si{}; si.buffer=src; si.offset=0; si.range=src_b;
    for(uint32_t i=0;i<n;++i){
        VkDescriptorImageInfo ii{}; ii.imageView=dst_views[i]; ii.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        const VkWriteDescriptorSet wds[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.sets[i],0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&si,nullptr},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.sets[i],1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&ii,nullptr,nullptr}};
        vkUpdateDescriptorSets(d.dev,2,wds,0,nullptr);
    }
    return p.pipe!=VK_NULL_HANDLE;
}
void unpipe_destroy(VDev& d,UnpackPipe& p){ if(p.pool)vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe)vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout)vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl)vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); p=UnpackPipe{}; }

// CAPTURE-DEDUP (--dedup): hash MUESTREADO (FNV-1a sobre 1 de cada 64 bytes) del buffer del frame ya
// en CPU. POR QUÉ: DDA captura el escritorio a la tasa de COMPOSICIÓN del DWM (= refresh del monitor,
// p.ej. 240Hz), pero el juego solo produce ~150 frames ÚNICOS/s → ~90 capturas/s son DUPLICADOS de
// contenido (el DWM re-compuso el mismo frame del juego). Frames idénticos → hash idéntico
// (detección de duplicado-exacto). El stride ≤64 cubre cambios de ≤16px en una fila; un cambio
// diminuto entre muestras podría falso-descartar — aceptable para FG. Coste ~0.1-0.2ms a 1080p,
// despreciable a ~240/s. El offset basis de FNV es no-cero y se multiplica por el primo, así que el
// hash de un buffer válido nunca es 0 → el centinela prev_hash==0 (primer frame) es seguro.
static uint64_t frame_sample_hash(const uint8_t* p, size_t n){
    uint64_t h=1469598103934665603ull;                 // FNV-1a offset basis (64-bit)
    for(size_t i=0;i<n;i+=64){ h^=(uint64_t)p[i]; h*=1099511628211ull; }   // XOR-byte, *FNV-prime
    return h;
}

// -- Thread C - run_capture. The alias preamble rebinds FgContext's references/array-pointers to
//    the exact local NAMES the body uses, so the body reads against plain locals. --
void run_capture(FgContext& ctx){
    auto& cfg = ctx.cfg;
    auto& ra_core_c = ctx.ra_core_c;
    auto& c_seq = ctx.c_seq;
    auto& cap_slots = ctx.cap_slots;
    auto& g_quit_threads = ctx.g_quit_threads;
#ifdef _MSC_VER
    auto& wgc_ctx = ctx.wgc_ctx;
#endif
    auto& d = ctx.d;
    auto& stat_mapfb = ctx.stat_mapfb;
    auto& stat_mapmiss = ctx.stat_mapmiss;
    auto& NAT_W = ctx.NAT_W;
    auto& NAT_H = ctx.NAT_H;
    auto& nat_bpp = ctx.nat_bpp;
    auto& Astage = ctx.Astage;
    auto& lt_copy_us = ctx.lt_copy_us;
    auto& lt_compose_us = ctx.lt_compose_us;
    auto& dd_timeouts = ctx.dd_timeouts;
    auto& dxgi_stage = ctx.dxgi_stage;
    auto& dd_arr_delta_us = ctx.dd_arr_delta_us;
    auto& dd_arrived = ctx.dd_arrived;
    auto& dd_lost = ctx.dd_lost;
    auto& dd_present = ctx.dd_present;
    auto& c_slots = ctx.c_slots;
    auto& total_real = ctx.total_real;
    // The acquire-side decoupling state (touched only on the cfg.ingest_async branch below).
    auto& dd_acq = ctx.dd_acq;
    auto& dd_uniq = ctx.dd_uniq;   // frames ÚNICOS reales (escrito en el camino de acquire, serial + async)
    auto& raw_busy = ctx.raw_busy;
    auto& raw_cv = ctx.raw_cv;
    auto& raw_mtx = ctx.raw_mtx;
    auto& raw_astage_a = ctx.raw_astage_a;
    auto& raw_tcap = ctx.raw_tcap;
    auto& raw_lt_submit = ctx.raw_lt_submit;
    auto& raw_lt_compose = ctx.raw_lt_compose;
    auto& dxgi_stage2 = ctx.dxgi_stage2;

            // ── Corrección de rotación de pantalla (issue #1) ────────────────────────────
            // DDA entrega los frames en la orientación NATIVA del panel; con el display rotado en
            // Windows (Landscape-flipped = ROTATE180, típico en monitores montados invertidos) el
            // contenido salía de cabeza. Reproducido de primera mano (qdump bajo 180: escritorio
            // invertido). El convert corrige 180 vía push-constant en AMBOS paths (iGPU pack + A).
            // WGC entrega ya-rotado a orientación lógica (MEDIDO en este rig) → gateado a CA_DD.
            // 90/270 exigirían swap W/H por todo el pipeline → no soportado, warning honesto.
            const uint32_t cap_rot180 = (cfg.capture_api==CA_DD && d.cap_rot==(int)DXGI_MODE_ROTATION_ROTATE180) ? 1u : 0u;
            if(cap_rot180)
                std::printf("[ra] capture-rotation: Landscape-flipped (ROTATE180) display detected — DDA delivers the panel's native orientation; correcting in the convert pass.\n");
            else if(cfg.capture_api==CA_DD && (d.cap_rot==(int)DXGI_MODE_ROTATION_ROTATE90 || d.cap_rot==(int)DXGI_MODE_ROTATION_ROTATE270))
                std::printf("[ra] WARNING: the capture display is PORTRAIT-rotated (90/270) — unsupported on the DDA path (would need a W/H swap through the whole pipeline). Content will appear rotated; WGC (the default API) delivers correctly-rotated content.\n");
            // --pin-test 5: MMCSS token held thread-local → RAII AvRevert at thread exit.
            // Default-constructed = inactive (no AVRT call) for modes 0-4 / no --pin (byte-identical-off).
            phyriad::hw::MmcssToken mmcss_c;
            // Pin the CAPTURE thread to a dedicated P-core (gated on cfg.pin_threads; skip UINT32_MAX —
            // unavailable id). On failure log + continue (do NOT crash). --pin-test selects the mode
            // (read only after cfg.pin_threads short-circuits): pin C only when the active mode includes
            // affinity (0=FULL, 1=NO-FLOW-RT, 3=AFFINITY-ONLY); 2/4 skip it.
            if(cfg.pin_threads && (cfg.pin_test==0||cfg.pin_test==1||cfg.pin_test==3)
               && ra_core_c!=UINT32_MAX && !phyriad::hw::pin_current_thread(ra_core_c))
                std::printf("[ra] WARN: capture-thread pin to core %u failed (continuing unpinned)\n", ra_core_c);
            // Mode 5 = MMCSS-COMPOSITE: C gets SOFT affinity (ideal-processor hint, migratable — NOT a
            // hard pin) + MMCSS 'Capture'/NORMAL, with elevate_thread_rt(false)=HIGHEST as the fallback
            // when MMCSS is inactive.
            if(cfg.pin_threads && cfg.pin_test==5){
                if(ra_core_c!=UINT32_MAX
                   && !phyriad::hw::set_thread_ideal_processor(GetCurrentThreadId(), ra_core_c).has_value())
                    std::printf("[ra] WARN: capture-thread soft-affinity (ideal proc %u) failed (continuing unhinted)\n", ra_core_c);
                mmcss_c = phyriad::hw::join_mmcss_task(phyriad::hw::MmcssTask::Capture, phyriad::hw::AvrtPriority::Normal);
                if(mmcss_c.active())
                    std::printf("[ra] MMCSS-composite: C joined 'Capture'/NORMAL (idx=%lu) + soft-affinity core %u\n", mmcss_c.task_index(), ra_core_c);
                else
                    std::printf("[ra] MMCSS-composite: C 'Capture' join INACTIVE -> fallback elevate_thread_rt(HIGHEST)%s + soft-affinity core %u\n", phyriad::hw::elevate_thread_rt(false)?"":" FAILED", ra_core_c);
            }
            int warmup_c=0;
            double last_dd_arr_ms=0.0;   // (async DD) arrival-ts del último frame PUBLICADO post-dedup (el serial usa last_ing_arr_ms)
            // ── --ingest-async (DDA branch): the ACQUIRE-ONLY loop ──────────────────────────────────────
            // This thread does ONLY AcquireNextFrame → CopyResource+Flush → Map(DO_NOT_WAIT the PREVIOUS
            // frame's copy = the readback overlap, removing the blocking-Map stall) → memcpy into the RAW
            // host ring → publish raw_seq. The convert WORKER (run_convert_worker) does the convert + the
            // c_seq publish. No convert here; DROP-TO-NEWEST happens on the worker side (it always converts
            // the freshest published raw slot). When cfg.ingest_async is false this whole block is skipped
            // and the serial loop below runs byte-identically. CA_DD-GATED (WGC_INGEST_ASYNC_PLAN.md): this
            // loop drives d.dup (NULL on WGC — deref = crash); the WGC async deposit lives INSIDE the main
            // loop's WGC pickup branch below, which feeds the same raw ring + worker.
            if(cfg.ingest_async && cfg.capture_api==CA_DD){
                ID3D11Texture2D* ddst[2]={dxgi_stage,dxgi_stage2};
                double db_arr_ms[2]={0.0,0.0};   // (PLL units) arrival-ts por slot del double-buffer (el frame publica 1 iteración después)
                uint64_t acq=0;            // monotone frames acquired (drives the double-buffer parity + the raw slot)
                bool have_prev=false;      // a previous frame's copy is in flight (ready to Map next iteration)
                // MEDICIÓN: desglose por-operación del lazo de acquire (acquire / copy+flush / map+memcpy).
                // EMAs locales 0.9/0.1, impresas ~1/s. Sin efecto en el pipeline.
                double _ema_acq_ms=0.0,_ema_cp_ms=0.0,_ema_mp_ms=0.0; uint64_t _dbg_n=0;
                uint64_t prev_hash=0;   // hash del último frame PUBLICADO (persistente entre iteraciones)
                while(!g_quit&&!g_quit_threads.load()){
                    DXGI_OUTDUPL_FRAME_INFO fi{}; IDXGIResource* res=nullptr;
                    const double _t_acq0=now_ms();
                    HRESULT _ahr=d.dup->AcquireNextFrame(33,&fi,&res);
                    const double _acq_ms=now_ms()-_t_acq0;   // MEDICIÓN: grande = esperando frames (vblank/DDA-bound)
                    if(_ahr==DXGI_ERROR_WAIT_TIMEOUT){ dd_timeouts.fetch_add(1); continue; }
                    if(FAILED(_ahr)){
                        // ACCESS_LOST / mode change — re-arm exactly as the serial branch (park in 10ms backoff).
                        dd_lost.fetch_add(1);
                        if(res){ rel(res); res=nullptr; }
                        bool re=false;
                        while(!(re=dda_rearm(d)) && !g_quit && !g_quit_threads.load()) Sleep(10);
                        if(re && (d.w!=NAT_W || d.h!=NAT_H)){
                            static bool warned=false;
                            if(!warned){ warned=true; std::printf("[ra] WARN: DDA re-armed at %ux%u != pipeline %ux%u (resolution change mid-run needs a restart; capture may show stale frames)\n",d.w,d.h,(unsigned)NAT_W,(unsigned)NAT_H); }
                        }
                        have_prev=false;   // the previous copy (if any) is on the dead duplication — drop it
                        continue;
                    }
                    dd_present.fetch_add((uint64_t)fi.AccumulatedFrames);   // counter; readout uses dd_acq
                    ID3D11Texture2D* cap=nullptr; res->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&cap); rel(res);
                    if(!cap){ d.dup->ReleaseFrame(); continue; }
                    const uint32_t cur_db=(uint32_t)(acq&1u);
                    // Submit THIS frame's copy; the PREVIOUS frame's copy (the other slot) has had ~1 frame to
                    // complete, so its DO_NOT_WAIT Map below almost always hits.
                    const double _t_cp0=now_ms();
                    d.ctx->CopyResource(ddst[cur_db],cap); d.ctx->Flush(); rel(cap);
                    d.dup->ReleaseFrame();
                    const double _cp_ms=now_ms()-_t_cp0;   // MEDICIÓN: CopyResource+Flush (GPU copy bajo saturación)
                    dd_acq.fetch_add(1);   // TELEMETRY: a successful acquire
                    {   // arrival stamp (per double-buffer slot) + dd_arrived. El delta del PLL se almacena
                        // en el punto de PUBLICACIÓN (post-dedup) abajo — un delta por-ENTREGA aquí
                        // corrompería T_robust bajo --dedup (fix de unidades).
                        db_arr_ms[cur_db]=now_ms();
                        dd_arrived.fetch_add(1);
                    }
                    // READBACK OVERLAP: map the PREVIOUS frame's completed copy into the raw ring (DO_NOT_WAIT).
                    double _mp_ms=-1.0;   // MEDICIÓN: -1 = no se mapeó esta iteración (sin prev / slot busy / map-miss)
                    if(have_prev){
                        const uint64_t pframe=acq-1u;
                        const uint32_t prev_db=(uint32_t)(pframe&1u);
                        const int rk=(int)(pframe%(uint64_t)kRawSlots);
                        // Torn-read guard: never overwrite the slot the worker is mid-converting (drop instead).
                        if(rk==raw_busy.load()){ stat_mapmiss.fetch_add(1); }
                        else {
                            D3D11_MAPPED_SUBRESOURCE mr{};
                            const double _t_mp0=now_ms();
                            HRESULT mhr=d.ctx->Map(ddst[prev_db],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mr);
                            if(mhr==S_OK){
                                uint8_t* dst=(uint8_t*)raw_astage_a[rk].mapped;
                                const size_t nat_row=size_t(NAT_W)*nat_bpp;
                                if(mr.RowPitch==nat_row) std::memcpy(dst,mr.pData,size_t(NAT_H)*nat_row);
                                else for(uint32_t y=0;y<NAT_H;++y)
                                    std::memcpy(dst+size_t(y)*nat_row,(const uint8_t*)mr.pData+size_t(y)*mr.RowPitch,nat_row);
                                d.ctx->Unmap(ddst[prev_db],0);
                                _mp_ms=now_ms()-_t_mp0;          // MEDICIÓN: Map+memcpy+Unmap (el readback CPU)
                                // El frame prev ya está en CPU (dst). dd_uniq cuenta únicos reales SIEMPRE
                                // (para el readout uniq=, incluso con --dedup OFF). Con cfg.dedup ON un
                                // DUPLICADO de contenido NO se publica (ni raw_seq ni notify) → el worker
                                // interpola entre únicos verdaderos en vez de un par de movimiento-cero. El
                                // descarte es LIMPIO: el Unmap ya se hizo y el slot rk queda reusable;
                                // have_prev/acq se actualizan abajo igual que en el camino normal.
                                const uint64_t _h=frame_sample_hash(dst,size_t(NAT_H)*nat_row);
                                const bool _dup=(prev_hash!=0 && _h==prev_hash); prev_hash=_h;
                                if(!_dup) dd_uniq.fetch_add(1);
                                if(!(cfg.dedup && _dup)){
                                    // (PLL units) delta entre frames PUBLICADOS (únicos), sellado con la
                                    // llegada PROPIA de cada frame (db_arr_ms[prev_db] — exacto, sin el
                                    // corrimiento de 1 frame del pipeline). Mismo stream que consume el worker.
                                    const double t_arr_pub=db_arr_ms[prev_db];
                                    if(t_arr_pub>0.0){
                                        if(last_dd_arr_ms>0.0){
                                            const double iv=t_arr_pub-last_dd_arr_ms;
                                            if(iv>0.5&&iv<500.0) dd_arr_delta_us.store((uint64_t)(iv*1000.0));
                                        }
                                        last_dd_arr_ms=t_arr_pub;
                                    }
                                    raw_tcap[rk]=now_ms();           // freshage anchor = host-resident instant (≈ serial's about-to-convert tcap)
                                    ctx.raws.publish(pframe+1u, raw_mtx, raw_cv);   // R7: the publish rule (store under the lock, notify outside) lives in RawRing
                                }
                            } else {
                                // Map miss = the in-flight copy still needs ~ms; drop this prev frame (drop-to-newest).
                                stat_mapmiss.fetch_add(1);
                            }
                        }
                    }
                    // MEDICIÓN: EMAs por-operación + impresión ~1/s (cada 120 acquires).
                    // acquire grande = esperando frames (vblank/DDA-bound); map+memcpy grande = procesando
                    // (CPU-bound); copy+flush grande = GPU saturado.
                    _ema_acq_ms = _ema_acq_ms? _ema_acq_ms*0.9+_acq_ms*0.1 : _acq_ms;
                    _ema_cp_ms  = _ema_cp_ms ? _ema_cp_ms*0.9 +_cp_ms*0.1  : _cp_ms;
                    if(_mp_ms>=0.0) _ema_mp_ms = _ema_mp_ms? _ema_mp_ms*0.9+_mp_ms*0.1 : _mp_ms;
                    if((++_dbg_n % 120u)==0u){ const double _loop=_ema_acq_ms+_ema_cp_ms+_ema_mp_ms;
                        std::printf("[ra-acq] acquire=%.2fms copy+flush=%.2fms map+memcpy=%.2fms | loop~%.2fms (~%.0ffps)\n",
                            _ema_acq_ms,_ema_cp_ms,_ema_mp_ms,_loop,_loop>0.0?1000.0/_loop:0.0); }
                    have_prev=true; ++acq;   // THIS frame becomes the prev for the next iteration
                }
                return;   // async acquire-only path complete; the worker handles convert + c_seq publish
            }
            // MEDICIÓN serial (etiqueta "[ra-acq] (serial)"): el lazo serial (async NO armado).
            // Desglose 4-way del readback.
            double _sema_acq=0.0,_sema_cp=0.0,_sema_map=0.0,_sema_mc=0.0; uint64_t _sdbg=0;
            // MEDICIÓN WGC (etiqueta "[ra-acq] (wgc)"): desglose 4-way del pickup WGC, gated en
            // --latency-trace (cero syscalls de timing con el gate OFF; a diferencia del serial de arriba
            // el pickup WGC es el hot path por defecto, así que su medición NO es always-on). staging =
            // spin W1 esperando la entrega del callback | copy = espera del copy-fence event + Map-retry,
            // INCLUYENDO los ciclos de miss (mapmiss): el runtime D3D11 puede fallar el DO_NOT_WAIT aun con
            // el fence pasado (medido mapmiss ≈ in-rate) → el ciclo fence-wait+Map+Sleep de cada miss se
            // acarrea a la figura copy de la siguiente iteración exitosa (sin el acarreo el continue del
            // miss DESCARTABA el término dominante y copy leía ~0)
            // | memcpy = readback CPU puro | hash = el dedup-sample-hash del tail compartido.
            double _wema_st=0.0,_wema_cp=0.0,_wema_mc=0.0,_wema_hs=0.0; uint64_t _wdbg=0;
            double _w_cp_carry=0.0;   // coste de iteraciones de miss acarreado al próximo copy exitoso
            // Acumulador+print [ra-acq] (wgc) — UNA copia compartida entre el tail serial y la rama
            // async (R7, WGC_INGEST_ASYNC_PLAN.md). EMAs 0.9/0.1 (las del serial), print ~1/120 frames.
            auto _wacq_acc=[&](double st,double cp,double mc,double hs){
                if(st>=0.0) _wema_st = _wema_st? _wema_st*0.9+st*0.1 : st;
                if(cp>=0.0) _wema_cp = _wema_cp? _wema_cp*0.9+cp*0.1 : cp;
                if(mc>=0.0) _wema_mc = _wema_mc? _wema_mc*0.9+mc*0.1 : mc;
                if(hs>=0.0) _wema_hs = _wema_hs? _wema_hs*0.9+hs*0.1 : hs;
                if((++_wdbg % 120u)==0u){ const double _loop=_wema_st+_wema_cp+_wema_mc+_wema_hs;
                    std::printf("[ra-acq] (wgc) staging=%.2fms copy=%.2fms memcpy=%.2fms hash=%.2fms | loop~%.2fms (~%.0ffps)\n",
                        _wema_st,_wema_cp,_wema_mc,_wema_hs,_loop,_loop>0.0?1000.0/_loop:0.0); } };
            uint64_t wpub=0;   // (WGC async) contador monotónico de raws PUBLICADOS: rk=wpub%kRawSlots, raw_seq=wpub+1 (analogía de pframe del acquire DDA)
            uint64_t prev_hash=0;   // hash del último frame INGESTADO (persistente entre iteraciones)
            double last_ing_arr_ms=0.0;   // (PLL units) arrival-ts del último frame INGESTADO (post-dedup)
            while(!g_quit&&!g_quit_threads.load()){
                const int s=(int)(c_seq.load()%(uint64_t)cap_slots);
                double lt_wgc_submit_ms=0.0, lt_wgc_compose_us=0.0;   // --latency-trace: carried from the consumed WGC slot
                double _wst_ms=-1.0,_wcp_ms=-1.0,_wmc_ms=-1.0;   // [ra-acq] (wgc): per-frame staging/copy/memcpy (set in the WGC branch; -1 = not measured this iter)
                double arr_ts=0.0;   // this frame's arrival timestamp (branch-set; consumed post-dedup below)
#ifdef _MSC_VER
                if(cfg.capture_api==CA_WGC){
                    // --copy-device: Map the ring on the SAME device the callback copied it on
                    // (wgc_ctx->cctx == the 2nd device when armed, null/d.ctx when off). cctx is non-null ONLY
                    // when --copy-device armed → byte-identical d.ctx path when off.
                    ID3D11DeviceContext* cap_ctx = wgc_ctx->cctx ? wgc_ctx->cctx : d.ctx;
                    const double _w_t_st0 = cfg.latency_trace ? now_ms() : 0.0;   // [ra-acq] (wgc) staging-wait: the W1 spin (only clocked when --latency-trace)
                    for(int spin=0;wgc_ctx->ring_write.load()==wgc_ctx->ring_read.load()&&!g_quit;++spin){
                        if(spin>=33) break; Sleep(1); }
                    if(cfg.latency_trace) _wst_ms=now_ms()-_w_t_st0;
                    const uint32_t w=wgc_ctx->ring_write.load();
                    const uint32_t r=wgc_ctx->ring_read.load();
                    if(w==r) continue;
                    const double _w_t_cp0 = cfg.latency_trace ? now_ms() : 0.0;   // [ra-acq] (wgc) copy-wait: the fence-event wait (or Map-retry) until the copy is mappable
                    // --copy-fence: event-driven wait for the newest slot's copy. The callback signaled the
                    // fence to (w_local+1) after copying slot (w_local)%N; the newest filled slot is (w-1)%N
                    // (w_local==w-1) → its copy was signaled to value w. So wait fence>=w. We wait on the EVENT
                    // (NOT a blocking Map / NOT the context lock) so the FrameArrived callback keeps doing
                    // CopyResource/Signal concurrently (no capture freeze). Bounded 33ms: on timeout we fall
                    // straight into the existing Map(DO_NOT_WAIT)+older-slot+Sleep(1) path below. If the fence
                    // is already >= w, SetEventOnCompletion fires immediately (no spurious wait).
                    bool fence_ok=false;   // the validated wait CONFIRMED completion (fence>=w) — arms the post-fence Map retry below
                    if(cfg.copy_fence && wgc_ctx->copyFence && wgc_ctx->ctx4 && wgc_ctx->copyEvt){
                        // Espera VALIDADA y anti-stale. El copyEvt es auto-reset: una SetEventOnCompletion
                        // de un frame ANTERIOR que disparó sin waiter deja el evento señalado → el
                        // WaitForSingleObject de ESTE frame despertaría al instante con el copy sin
                        // completar (falso wake → Map miss por frame, medido). Por eso: (1) ResetEvent
                        // ANTES de registrar (limpia el stale sin lost-wake: si el fence ya >= tgt al
                        // registrar, el evento se re-señala fresco); (2) el lazo RE-VERIFICA
                        // GetCompletedValue tras cada wake (absorbe cualquier wake espurio); (3) el
                        // presupuesto total 33ms conserva el fallback timeout→Map-retry intacto.
                        const uint64_t tgt=(uint64_t)w;
                        const double t0=now_ms();
                        while(wgc_ctx->copyFence->GetCompletedValue()<tgt){
                            ResetEvent(wgc_ctx->copyEvt);
                            wgc_ctx->copyFence->SetEventOnCompletion(tgt,wgc_ctx->copyEvt);
                            const double el=now_ms()-t0;
                            if(el>=33.0) break;
                            if(WaitForSingleObject(wgc_ctx->copyEvt,(DWORD)(33.0-el))==WAIT_TIMEOUT) break;
                        }
                        fence_ok = wgc_ctx->copyFence->GetCompletedValue()>=tgt;   // passed vs timed-out (one driver read/frame)
                    }
                    // Saturated-primary capture resilience. The callback only SUBMITS the CopyResource — the
                    // 4090 executes it behind a saturated game, so the NEWEST slot's Map(DO_NOT_WAIT) often
                    // fails. Fall back to the next-older UNREAD slot: its copy was submitted a frame earlier
                    // and is almost surely complete. Costs one frame of capture age in exactly the moments the
                    // stall would have cost 1-33ms; the newest slot stays unread and is retried next cycle.
                    uint32_t use_cnt=w;   // consume-up-to count (mapped slot = (use_cnt-1)%RING_N)
                    D3D11_MAPPED_SUBRESOURCE mr{};
                    HRESULT mhr=cap_ctx->Map(wgc_ctx->ring[(w-1u)%WgcCtx::RING_N],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mr);
                    // (A blocking post-fence Map was MEASURED and REJECTED here: holding the multithread
                    // context lock while the runtime syncs starves the FrameArrived callback's CopyResource —
                    // arr fell 241→179/s, in 241→112-128/s. DO_NOT_WAIT + the bounded retry below wins.)
                    // Post-fence bounded retry (no Sleep). fence_ok ⇒ the slot's copy COMPLETED on the GPU
                    // timeline, so a miss here is only the runtime's Map(DO_NOT_WAIT) bookkeeping lagging the
                    // fence (medido: un miss+Sleep(1) POR FRAME = ~1.5-1.9ms/frame de tax fijo en el pickup).
                    // Reintenta el MISMO slot más nuevo en un lazo acotado sin dormir: SwitchToThread cede el
                    // core sin el piso ~1ms del Sleep; 64×(50 pause + yield + Map) es µs-class y está acotado
                    // por CONTEO de iteraciones (sin constante de ms afinada a este rig). Si aún falla tras el
                    // presupuesto → cae al fallback Sleep(1) de abajo (intacto, y el ÚNICO sitio que cuenta
                    // stat_mapmiss: mapmiss = "el pickup pagó un ciclo de 1ms", ahora ~0 esperado). El path
                    // sin fence (probe fallido / timeout) es byte-idéntico: fence_ok=false salta este lazo.
                    if(mhr!=S_OK && fence_ok){
                        for(int rt=0; rt<64 && mhr!=S_OK; ++rt){
                            for(int yp=0; yp<50; ++yp) YieldProcessor();
                            SwitchToThread();
                            mhr=cap_ctx->Map(wgc_ctx->ring[(w-1u)%WgcCtx::RING_N],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mr);
                        }
                    }
                    if(mhr!=S_OK&&(w-r)>=2u){
                        mhr=cap_ctx->Map(wgc_ctx->ring[(w-2u)%WgcCtx::RING_N],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mr);
                        if(mhr==S_OK){ use_cnt=w-1u; stat_mapfb.fetch_add(1); }
                    }
                    // Miss (post-retry) = the copy genuinely is NOT consumable yet — the fence timed out (GPU
                    // behind a saturated primary: the in-flight copy needs ~ms anyway) or the runtime refused
                    // past the retry budget. Sleeping frees C's core. The whole miss cycle (fence-wait + Map
                    // attempts + Sleep) is REAL pickup cost — carried into the NEXT successful iteration's
                    // copy figure instead of discarded at the continue.
                    if(mhr!=S_OK){ stat_mapmiss.fetch_add(1); Sleep(1);
                        if(cfg.latency_trace) _w_cp_carry+=now_ms()-_w_t_cp0; continue; }
                    if(cfg.latency_trace){ _wcp_ms=(now_ms()-_w_t_cp0)+_w_cp_carry; _w_cp_carry=0.0; }
                    // ── --ingest-async (WGC): deposit the raw frame + publish to the convert worker.
                    // INVARIANT (R3, WGC_INGEST_ASYNC_PLAN.md): with --ingest-async armed the WORKER is the
                    // single owner of the convert state (cmdA/cmdG/fA/fG/Anative/Awork/cpPipe) — every path
                    // in this block ends in `continue`, so the shared serial tail below (t_cap/lt-EMA/dedup/
                    // PLL/convert/c_seq publish) is UNREACHABLE in async mode. Mirrors the DDA acquire branch.
                    if(cfg.ingest_async){
                        const int rk=(int)(wpub%(uint64_t)kRawSlots);
                        // Torn-read guard (R1): never overwrite the slot the worker is mid-converting — drop
                        // this frame (the staging slot IS consumed: Unmap+advance). Counted in stat_mapmiss
                        // (the DDA-async busy-drop convention, ver el acquire DDA).
                        if(rk==raw_busy.load()){
                            stat_mapmiss.fetch_add(1);
                            cap_ctx->Unmap(wgc_ctx->ring[(use_cnt-1u)%WgcCtx::RING_N],0); wgc_ctx->ring_read.store(use_cnt);
                            continue;
                        }
                        const size_t nat_row=size_t(NAT_W)*nat_bpp;
                        const double _w_t_mc0 = cfg.latency_trace ? now_ms() : 0.0;
                        { uint8_t* dst=(uint8_t*)raw_astage_a[rk].mapped;
                          if(mr.RowPitch==nat_row) std::memcpy(dst,mr.pData,size_t(NAT_H)*nat_row);
                          else for(uint32_t y=0;y<NAT_H;++y)
                              std::memcpy(dst+size_t(y)*nat_row,(const uint8_t*)mr.pData+size_t(y)*mr.RowPitch,nat_row); }
                        if(cfg.latency_trace) _wmc_ms=now_ms()-_w_t_mc0;
                        cap_ctx->Unmap(wgc_ctx->ring[(use_cnt-1u)%WgcCtx::RING_N],0); wgc_ctx->ring_read.store(use_cnt);
                        arr_ts=now_ms();   // WGC consume instant (la referencia PLL — mismo instante que el serial)
                        if(cfg.latency_trace){ const uint32_t cs=(use_cnt-1u)%WgcCtx::RING_N;
                            lt_wgc_submit_ms =(double)wgc_ctx->ring_submit_us[cs].load()/1000.0;
                            lt_wgc_compose_us=(double)wgc_ctx->ring_compose_us[cs].load(); }
                        dd_acq.fetch_add(1);   // acq= (tasa de pickup, pre-dedup — paridad con el tail serial)
                        // Dedup ANTES de publicar (R5): el worker sólo ve únicos; un duplicado NO publica y
                        // el slot rk se reutiliza en el próximo frame (wpub no avanza). dd_uniq SIEMPRE.
                        { double _whs_ms=-1.0;
                          const double _w_t_hs0 = cfg.latency_trace ? now_ms() : 0.0;
                          const uint64_t _h=frame_sample_hash((const uint8_t*)raw_astage_a[rk].mapped,size_t(NAT_H)*nat_row);
                          if(cfg.latency_trace){ _whs_ms=now_ms()-_w_t_hs0; _wacq_acc(_wst_ms,_wcp_ms,_wmc_ms,_whs_ms); }
                          const bool _dup=(prev_hash!=0 && _h==prev_hash); prev_hash=_h;
                          if(!_dup) dd_uniq.fetch_add(1);
                          if(cfg.dedup && _dup) continue; }
                        // (PLL units, R5) delta entre frames PUBLICADOS (post-dedup), sellado con arr_ts — el
                        // MISMO stream/instante que el serial; al contador WGC del PLL (wgc_ctx->arr_delta_us).
                        if(last_ing_arr_ms>0.0){
                            const double iv=arr_ts-last_ing_arr_ms;
                            if(iv>0.5&&iv<500.0) wgc_ctx->arr_delta_us.store((uint64_t)(iv*1000.0));
                        }
                        last_ing_arr_ms=arr_ts;
                        raw_tcap[rk]=now_ms();                // freshage anchor (≈ el t_cap del serial; el worker lo lleva a c_slots[s])
                        raw_lt_submit[rk]=lt_wgc_submit_ms;   // (R6) lat-trace carry al worker (0 con trace off → inerte)
                        raw_lt_compose[rk]=lt_wgc_compose_us;
                        ctx.raws.publish(wpub+1u, raw_mtx, raw_cv);   // R7: la MISMA publicación que el acquire DDA, desde RawRing
                        ++wpub;
                        continue;   // R3: el tail serial (convert + c_seq publish) NO se alcanza en async
                    }
                    const size_t nat_row=size_t(NAT_W)*nat_bpp;
                    const double _w_t_mc0 = cfg.latency_trace ? now_ms() : 0.0;   // [ra-acq] (wgc) memcpy: pure CPU readback of the mapped slot
                    if(mr.RowPitch==nat_row) std::memcpy(Astage.mapped,mr.pData,size_t(NAT_H)*nat_row);
                    else for(uint32_t y=0;y<NAT_H;++y)
                        std::memcpy((uint8_t*)Astage.mapped+size_t(y)*nat_row,(const uint8_t*)mr.pData+size_t(y)*mr.RowPitch,nat_row);
                    if(cfg.latency_trace) _wmc_ms=now_ms()-_w_t_mc0;
                    cap_ctx->Unmap(wgc_ctx->ring[(use_cnt-1u)%WgcCtx::RING_N],0); wgc_ctx->ring_read.store(use_cnt);
                    arr_ts=now_ms();   // WGC consume instant (≈ delivery + copy; el jitter lo absorbe la EMA+banda)
                    if(cfg.latency_trace){ const uint32_t cs=(use_cnt-1u)%WgcCtx::RING_N;
                        lt_wgc_submit_ms =(double)wgc_ctx->ring_submit_us[cs].load()/1000.0;
                        lt_wgc_compose_us=(double)wgc_ctx->ring_compose_us[cs].load(); }
                } else
#endif
                {
                    DXGI_OUTDUPL_FRAME_INFO fi{}; IDXGIResource* res=nullptr;
                    double _sacq_ms=0.0;
                    {
                        const double _s_t_acq0=now_ms();
                        HRESULT _ahr=d.dup->AcquireNextFrame(33,&fi,&res);
                        _sacq_ms=now_ms()-_s_t_acq0;
                        if(_ahr==DXGI_ERROR_WAIT_TIMEOUT){ dd_timeouts.fetch_add(1); continue; }   // no new desktop frame in 33ms — normal idle
                        if(FAILED(_ahr)){
                            // ACCESS_LOST (fullscreen transition / mode change / MPO re-plan on 24H2) or any
                            // other failure => the duplication is dead. Re-arm it instead of a bare `continue`
                            // (which would spin on the corpse and FREEZE capture mid-game). Park in 10ms backoff
                            // until re-armed or quitting — never pegs the core, always exits on quit, and
                            // guarantees d.dup is valid before the next AcquireNextFrame (no null-deref).
                            dd_lost.fetch_add(1);
                            if(res){ rel(res); res=nullptr; }
                            bool re=false;
                            while(!(re=dda_rearm(d)) && !g_quit && !g_quit_threads.load()) Sleep(10);
                            if(re && (d.w!=NAT_W || d.h!=NAT_H)){
                                static bool warned=false;
                                if(!warned){ warned=true; std::printf("[ra] WARN: DDA re-armed at %ux%u != pipeline %ux%u (resolution change mid-run needs a restart; capture may show stale frames)\n",d.w,d.h,(unsigned)NAT_W,(unsigned)NAT_H); }
                            }
                            continue;
                        }
                    }   // WAIT_TIMEOUT counted (capture cross-check) + re-arm on ACCESS_LOST
                    const bool had_upd=(fi.LastPresentTime.QuadPart!=0||fi.AccumulatedFrames!=0);
                    // CAPTURA REAL: presents que el escritorio acumuló desde nuestro último Acquire.
                    // AccumulatedFrames>1 = DDA compuso varios y solo vimos el último → los descartamos
                    // por ir lentos (cuello B). Sumarlo = tasa entregada verdadera (distinta de dd_arrived,
                    // que va en lockstep con la ingesta y no revela los frames coalescidos).
                    dd_present.fetch_add((uint64_t)fi.AccumulatedFrames);
                    ID3D11Texture2D* cap=nullptr; res->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&cap); rel(res);
                    if(!cap){d.dup->ReleaseFrame();continue;}
                    const double _s_t_cp0=now_ms();
                    d.ctx->CopyResource(dxgi_stage,cap); d.ctx->Flush(); rel(cap);
                    const double _scp_ms=now_ms()-_s_t_cp0;   // submit del CopyResource+Flush (no espera a que ejecute)
                    D3D11_MAPPED_SUBRESOURCE mr{}; bool mapped=false; double _smap_ms=-1.0,_smc_ms=-1.0;
                    const double _s_t_map0=now_ms();
                    HRESULT _smhr=d.ctx->Map(dxgi_stage,0,D3D11_MAP_READ,0,&mr);   // BLOQUEANTE: espera a que el CopyResource termine en el 4090
                    _smap_ms=now_ms()-_s_t_map0;
                    if(_smhr==S_OK){
                        const double _s_t_mc0=now_ms();
                        for(uint32_t y=0;y<NAT_H;++y)
                            std::memcpy((uint8_t*)Astage.mapped+size_t(y)*NAT_W*nat_bpp,(const uint8_t*)mr.pData+size_t(y)*mr.RowPitch,size_t(NAT_W)*nat_bpp);
                        _smc_ms=now_ms()-_s_t_mc0;
                        d.ctx->Unmap(dxgi_stage,0); mapped=true; }
                    d.dup->ReleaseFrame(); if(!mapped) continue;
                    // MEDICIÓN serial (etiqueta "(serial)" = async NO armado). 4-way: acquire(espera del frame) |
                    // copy=submit del CopyResource | mapwait=Map BLOQUEANTE (≈ ejecución del copy en el 4090 saturado) |
                    // memcpy=readback CPU puro. mapwait grande → GPU/copy (interop/DMA); memcpy grande → CPU readback.
                    _sema_acq = _sema_acq? _sema_acq*0.9+_sacq_ms*0.1 : _sacq_ms;
                    _sema_cp  = _sema_cp ? _sema_cp*0.9 +_scp_ms*0.1  : _scp_ms;
                    if(_smap_ms>=0.0) _sema_map = _sema_map? _sema_map*0.9+_smap_ms*0.1 : _smap_ms;
                    if(_smc_ms>=0.0)  _sema_mc  = _sema_mc ? _sema_mc*0.9 +_smc_ms*0.1  : _smc_ms;
                    if((++_sdbg % 120u)==0u){ const double _loop=_sema_acq+_sema_cp+_sema_map+_sema_mc;
                        std::printf("[ra-acq] (serial) acquire=%.2fms copy=%.2fms mapwait=%.2fms memcpy=%.2fms | loop~%.2fms (~%.0ffps)\n",
                            _sema_acq,_sema_cp,_sema_map,_sema_mc,_loop,_loop>0.0?1000.0/_loop:0.0); }
                    if(had_upd)++warmup_c; if(warmup_c<3) continue;
                    // DD "arrival" = a delivered frame in hand. Timestamp it (the PLL delta is stored
                    // POST-dedup in the shared tail below — a per-DELIVERY delta here would corrupt
                    // T_robust by the delivered/unique ratio under --dedup) and count the delivery.
                    arr_ts=now_ms();
                    dd_arrived.fetch_add(1);
                }
                c_slots[s].t_cap_ms=now_ms();
                // --latency-trace: the two PRE-tcap (INVISIBLE-to-freshage) deltas. copy = the
                // CopyResource-execution-under-saturation gap (tcap−submit, both steady); compose = the WGC
                // compose→callback latency (carried QPC delta). EMAs (0.8/0.2), µs. Off → no work.
                if(cfg.latency_trace){
                    const double tc=c_slots[s].t_cap_ms;
                    if(lt_wgc_submit_ms>0.0 && tc>lt_wgc_submit_ms){ const double cp=(tc-lt_wgc_submit_ms)*1000.0;
                        const uint64_t pv=lt_copy_us.load(); lt_copy_us.store(pv?(uint64_t)((double)pv*0.8+cp*0.2):(uint64_t)cp); }
                    if(lt_wgc_compose_us>0.0){ const uint64_t pv=lt_compose_us.load();
                        lt_compose_us.store(pv?(uint64_t)((double)pv*0.8+lt_wgc_compose_us*0.2):(uint64_t)lt_wgc_compose_us); }
                }
                dd_acq.fetch_add(1);   // TELEMETRY: acq cuenta TODAS las adquisiciones (incl. duplicados de composite ≈ refresh); readout fiable (dd_present lee 0 en NVIDIA)
                // El frame ya está en CPU (Astage.mapped). dd_uniq cuenta únicos reales SIEMPRE (para el
                // readout uniq=, incluso con --dedup OFF; aquí solo se computa el hash + el contador, sin
                // saltar nada → pipeline byte-idéntico). Con cfg.dedup ON un DUPLICADO de contenido se
                // DESCARTA (continue: salta total_real + el convert + el c_seq.fetch_add) → el FG nunca ve
                // un par de movimiento-cero, e in= cae a la tasa única. El Unmap/ReleaseFrame ya ocurrieron
                // arriba, así que el continue es LIMPIO (sin fuga; igual que los otros del lazo).
                double _whs_ms=-1.0;   // [ra-acq] (wgc) hash: dedup-sample-hash time this iter (-1 = not clocked)
                {
                    const double _w_t_hs0 = cfg.latency_trace ? now_ms() : 0.0;
                    const uint64_t _h=frame_sample_hash((const uint8_t*)Astage.mapped,size_t(NAT_H)*size_t(NAT_W)*nat_bpp);
                    if(cfg.latency_trace) _whs_ms=now_ms()-_w_t_hs0;
                    // MEDICIÓN WGC 4-way: vía _wacq_acc (una copia; también la llama la rama async). Sólo
                    // WGC + --latency-trace; en DDA-serial estos vars son -1 → no acumula (la rama serial
                    // imprime su propia línea). Se acumula ANTES del posible dedup-continue para que un
                    // frame descartado por --dedup también cuente en la EMA del pickup.
#ifdef _MSC_VER
                    if(cfg.capture_api==CA_WGC && cfg.latency_trace) _wacq_acc(_wst_ms,_wcp_ms,_wmc_ms,_whs_ms);
#endif
                    const bool _dup=(prev_hash!=0 && _h==prev_hash); prev_hash=_h;
                    if(!_dup) dd_uniq.fetch_add(1);
                    if(cfg.dedup && _dup) continue;
                }
                total_real.fetch_add(1);   // in = frames realmente INGESTADOS (cae a la tasa única con --dedup ON; = acq con OFF)
                // (PLL units) el delta de llegada se mide entre frames INGESTADOS — el MISMO stream de
                // eventos que cuenta c_seq (la referencia de fase del reloj de contenido). Medirlo sobre
                // entregas crudas corrompe T_robust por el factor entregado/único bajo --dedup (k≈11
                // medido: fuente 21fps en escritorio DDA 240Hz → la biestabilidad asiento-profundo /
                // vibración). WGC y DD-serial convergen aquí; cada run usa UNA API → stream homogéneo.
                if(arr_ts>0.0){
                    if(last_ing_arr_ms>0.0){
                        const double iv=arr_ts-last_ing_arr_ms;
                        if(iv>0.5&&iv<500.0){
                            const uint64_t us=(uint64_t)(iv*1000.0);
#ifdef _MSC_VER
                            if(cfg.capture_api==CA_WGC&&wgc_ctx) wgc_ctx->arr_delta_us.store(us);
                            else
#endif
                            dd_arr_delta_us.store(us);
                        }
                    }
                    last_ing_arr_ms=arr_ts;
                }
                pfg::ingest::convert_and_publish(ctx, cap_rot180, s);   // R6 step 1: STAGE 2 — the convert + publish tail moved to ingest/ingest.cpp (verbatim)
            }
}
// ── --ingest-async: the convert WORKER thread moved to ingest/ingest.cpp (R6 step 1, stage 2) ──
// Made with my soul - Swately <3
