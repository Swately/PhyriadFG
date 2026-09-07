// PhyriadFG — core-side init (E1 of docs/planning/RESTRUCTURE_PLAN.md).
// Init-seq sections moved verbatim out of main.cpp behind ownership-struct binding preambles;
// bodies byte-identical to the pre-E1 sections except `goto done` -> `return false`.
#include "capture/wgc_ctx.hpp"   // FIRST: defines NOMINMAX before <windows.h> (winrt include order matters — same as main.cpp)
#include <algorithm>
#include <cstdio>
#include "core/app_init.hpp"

// ── Vulkan instance + physical-device selection (E1: moved VERBATIM from main.cpp).
// Returns -1 to continue; >=0 is main()'s exit code (the original early `return 1` paths).
int init_vk_pick(Config& cfg, D3D& d, VkPickInit& o_pick){
    auto& inst=o_pick.inst;
    auto& pA=o_pick.pA; auto& pB=o_pick.pB; auto& pG=o_pick.pG;
    auto& luidA=o_pick.luidA; auto& luidB=o_pick.luidB; auto& luidG=o_pick.luidG;
    auto& luidA_ok=o_pick.luidA_ok; auto& luidB_ok=o_pick.luidB_ok; auto& luidG_ok=o_pick.luidG_ok;
    auto& nvNameA=o_pick.nvNameA; auto& nvNameB=o_pick.nvNameB;
    // ── Vulkan instance ───────────────────────────────────────────────────────
    // The WSI surface extensions are still requested (harmless, no swapchain is created — the bridge
    // presents through PresentSurface's DComp path, not a VkSwapchainKHR).
    // The device ext VK_KHR_format_feature_flags2 (a prereq of VK_NV_optical_flow) itself depends on
    // the INSTANCE ext VK_KHR_get_physical_device_properties2. Under --nvofa we enable it here so the
    // dependency chain is satisfied. Default path: 2 exts (byte-identical).
    const char* inst_exts[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME,VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    // R2 (the SEAM): request Vulkan 1.3 when the LOADER offers it — vkCmdPipelineBarrier2 (the seam
    // engine's only Vulkan call) is core 1.3. Queried, never assumed: vkEnumerateInstanceVersion is a
    // 1.1+ global; if it is missing or reports < 1.3, or --no-sync2 is passed, we stay on 1.2 exactly as
    // before and the graph path is simply not taken. The apiVersion is the only difference on that path.
    uint32_t loader_ver=VK_API_VERSION_1_0;
    if(auto pEnum=(PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr,"vkEnumerateInstanceVersion")) pEnum(&loader_ver);
    const bool want_13 = !cfg.no_sync2 && (VK_API_VERSION_MAJOR(loader_ver)>1 ||
                         (VK_API_VERSION_MAJOR(loader_ver)==1 && VK_API_VERSION_MINOR(loader_ver)>=3));
    VkApplicationInfo ai{}; ai.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO; ai.pApplicationName="render_assistant"; ai.apiVersion=want_13?VK_API_VERSION_1_3:VK_API_VERSION_1_2;
    std::printf("[ra] vulkan: loader %u.%u.%u -> instance %s%s\n",
        VK_API_VERSION_MAJOR(loader_ver),VK_API_VERSION_MINOR(loader_ver),VK_API_VERSION_PATCH(loader_ver),
        want_13?"1.3 (synchronization2 available)":"1.2 (hand-written barriers)", cfg.no_sync2?" [--no-sync2]":"");
    // --validation (DIAGNOSTIC): the KHRONOS layer + VK_EXT_debug_utils so the messenger can print.
    // Off -> enabledLayerCount 0 and the ext list is untouched (byte-identical).
    const char* inst_exts_val[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME,VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    const char* val_layers[]={"VK_LAYER_KHRONOS_validation"};
    VkInstanceCreateInfo ici2{}; ici2.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici2.pApplicationInfo=&ai; ici2.enabledExtensionCount=cfg.nvofa?3u:2u; ici2.ppEnabledExtensionNames=inst_exts;
    if(cfg.validation){ ici2.enabledExtensionCount=4u; ici2.ppEnabledExtensionNames=inst_exts_val; ici2.enabledLayerCount=1u; ici2.ppEnabledLayerNames=val_layers; }
    inst=VK_NULL_HANDLE; if (vkCreateInstance(&ici2,nullptr,&inst)!=VK_SUCCESS) { std::printf("[ra] instance failed\n"); d3d_shutdown(d); return 1; }
    if(cfg.validation){
        // Print EVERY validation message the layer emits (warnings + errors). The gate reads this stream;
        // "clean" means zero VALIDATION lines over the run, quoted in the record.
        auto cb=[](VkDebugUtilsMessageSeverityFlagBitsEXT sev,VkDebugUtilsMessageTypeFlagsEXT,
                   const VkDebugUtilsMessengerCallbackDataEXT* d2,void*)->VkBool32{
            std::printf("[vk-%s] %s\n",(sev&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)?"ERROR":"WARN",
                        d2&&d2->pMessage?d2->pMessage:"(null)"); std::fflush(stdout); return VK_FALSE; };
        VkDebugUtilsMessengerCreateInfoEXT mci{}; mci.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        mci.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        mci.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        mci.pfnUserCallback=cb;
        if(auto pCreate=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst,"vkCreateDebugUtilsMessengerEXT")){
            VkDebugUtilsMessengerEXT msgr=VK_NULL_HANDLE;   // stored in g_dbg_messenger below so teardown can destroy it
            if(pCreate(inst,&mci,nullptr,&msgr)==VK_SUCCESS){ g_dbg_messenger=msgr; std::printf("[ra] --validation: messenger armed\n"); }
            else std::printf("[ra] --validation: messenger create FAILED -- the layer still validates, messages go to the debug output\n");
        } else std::printf("[ra] --validation: vkCreateDebugUtilsMessengerEXT unavailable\n");
    }
    // ── Physical device selection ─────────────────────────────────────────────
    uint32_t nd=0; vkEnumeratePhysicalDevices(inst,&nd,nullptr); std::vector<VkPhysicalDevice> pds(nd); vkEnumeratePhysicalDevices(inst,&nd,pds.data());
    // Capture each selected device's deviceLUID for the per-adapter GPU% stat. The PDH
    // "\GPU Engine(*)\Utilization Percentage" instance names embed luid_0x{High}_0x{Low}; we format
    // these LUIDs the same way and sum each engine's utilization into the matching adapter.
    // deviceLUID is the 8-byte little-endian image of a Windows LUID (LowPart[0..3], HighPart[4..7]).
                                   // space does NOT equal the Vulkan/DXGI deviceLUID on some rigs, so
                                   // A/B utilization comes from NVML — nvidia-smi's own source — by name.
    std::string afrag(cfg.assist_gpu); for(auto& c:afrag) c=(char)std::tolower((unsigned char)c);
    for(auto pd:pds){ VkPhysicalDeviceIDProperties idp{}; idp.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES; VkPhysicalDeviceProperties2 p2{}; p2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2; p2.pNext=&idp; vkGetPhysicalDeviceProperties2(pd,&p2);
        const bool primary=idp.deviceLUIDValid&&std::memcmp(idp.deviceLUID,&d.luid,sizeof(LUID))==0;
        if(primary){pA=pd; if(idp.deviceLUIDValid){std::memcpy(&luidA,idp.deviceLUID,sizeof(LUID)); luidA_ok=true;} continue;}
        if(p2.properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU){pG=pd; if(idp.deviceLUIDValid){std::memcpy(&luidG,idp.deviceLUID,sizeof(LUID)); luidG_ok=true;} continue;}
        if(p2.properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){ if(!afrag.empty()){ std::string nm(p2.properties.deviceName); for(auto& c:nm) c=(char)std::tolower((unsigned char)c); if(nm.find(afrag)!=std::string::npos&&!pB){ pB=pd; if(idp.deviceLUIDValid){std::memcpy(&luidB,idp.deviceLUID,sizeof(LUID)); luidB_ok=true;} } } else if(!pB){ pB=pd; if(idp.deviceLUIDValid){std::memcpy(&luidB,idp.deviceLUID,sizeof(LUID)); luidB_ok=true;} } }
    }
    // Capture A/B device names for the NVML utilization match (re-query, no selection-logic change).
    if(pA){ VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pA,&pr); nvNameA=pr.deviceName; }
    if(pB){ VkPhysicalDeviceProperties pr{}; vkGetPhysicalDeviceProperties(pB,&pr); nvNameB=pr.deviceName; }
    // Require ONLY device A. single_gpu derives true with --force-single-gpu OR when no 2nd discrete
    // GPU physically exists (pB null). Under --force-single-gpu null out pB AND pG so the degenerate
    // path is DRIVEN (no aliasing) — every device-B/G creation gate below sees a null phys handle and
    // routes onto A (FD) instead.
    if(!pA){ std::printf("[ra] need primary GPU (LUID). A=%s\n",pA?"ok":"MISSING"); vkDestroyInstance(inst,nullptr); d3d_shutdown(d); return 1; }
    {   // The honest VRR/G-Sync vendor statement at startup. A non-hooking external overlay cannot
        // drive the captured game's VRR; we NEVER hook the game swapchain (= injection = ban).
        // Informational (always-on, no behavior change). The is_nv/is_amd substrings below select
        // only WHICH sentence to PRINT — they gate NO FG behaviour.
        std::string vlo=nvNameA; for(auto& c2:vlo) c2=(char)std::tolower((unsigned char)c2);
        const bool is_nv=(vlo.find("nvidia")!=std::string::npos||vlo.find("geforce")!=std::string::npos);
        const bool is_amd=(vlo.find("amd")!=std::string::npos||vlo.find("radeon")!=std::string::npos);
        if(is_nv)      std::printf("[ra] F2/VRR (honest): NVIDIA — G-Sync stays OFF for the captured game while PhyriadFG presents (a non-hooking overlay can't drive the game's VRR; a permanent class floor shared with LSFG). Windowed-G-Sync = the partial mitigation. We never hook the game swapchain.\n");
        else if(is_amd)std::printf("[ra] F2/VRR (honest): AMD — FreeSync MAY persist with the overlay (verify the monitor OSD); on NVIDIA it would not. We never degrade the baseline + never hook the game swapchain.\n");
        else           std::printf("[ra] F2/VRR (honest): a non-hooking external overlay cannot drive the captured game's VRR (G-Sync off on NVIDIA; AMD/Intel FreeSync may persist — verify the OSD). We never hook the game swapchain.\n");
    }
    const bool single_gpu = cfg.force_single_gpu || (pA && !pB);   // the single-GPU switch
    if(cfg.force_single_gpu){ pB=VK_NULL_HANDLE; pG=VK_NULL_HANDLE; nvNameB.clear(); luidB_ok=false; }   // drive the TRUE degenerate (no B/G aliasing)
    if(!single_gpu && !pB){ std::printf("[ra] need primary GPU (LUID) + assist discrete (or --force-single-gpu). B=MISSING\n"); vkDestroyInstance(inst,nullptr); d3d_shutdown(d); return 1; }

    o_pick.single_gpu = single_gpu;   // E1 export (was a const main-local derived here)
    return -1;
}

// ── Devices + the derived feature gates (E1: moved VERBATIM from main.cpp).
bool init_devices(Config& cfg, VkPhysicalDevice pA, VkPhysicalDevice pB, VkPhysicalDevice pG,
                  bool single_gpu, bool want_pfg, bool IS_HDR, uint32_t NAT_W, uint32_t NAT_H,
                  uint32_t WW, uint32_t WH, Route route, DevicesInit& o_dev){
    VDev& A=o_dev.A; VDev& B=o_dev.B; VDev& G=o_dev.G;
    auto& have_igpu=o_dev.have_igpu; auto& use_upscale=o_dev.use_upscale;
    auto& use_igpu_convert=o_dev.use_igpu_convert; auto& use_fwd_prestage=o_dev.use_fwd_prestage;
    auto& use_wap=o_dev.use_wap; auto& use_bidir=o_dev.use_bidir; auto& use_fill_div=o_dev.use_fill_div;
    auto& use_rescue=o_dev.use_rescue; auto& use_mv_median=o_dev.use_mv_median;
    auto& use_mv_guided=o_dev.use_mv_guided; auto& use_gme=o_dev.use_gme;
    auto& use_inertia=o_dev.use_inertia; auto& use_objects=o_dev.use_objects;
    auto& use_ambig=o_dev.use_ambig; auto& use_commit_default=o_dev.use_commit_default;
    auto& use_onepos=o_dev.use_onepos;
    // ── Devices ───────────────────────────────────────────────────────────────
    // The present is ALWAYS the A-side bridge → PresentSurface (no VkSwapchain on any device). G never
    // presents — its only niche is igpu-convert (fused convert+pack, CPU-proximity). A enables
    // VK_KHR_external_memory_win32 + VK_KHR_win32_keyed_mutex for the bridge unconditionally.
    // Skip B's vdev_create under single_gpu (B stays a null VDev{}); the flow/gme producer rides A (FD).
    // A still needs EMH for the host bridges (unconditional). Gate the B half.
    // --nvofa: request the OFA queue on device A (the 4090 — the only OFA-capable device; the 1080 Ti has
    // no OFA). want_ofa=cfg.nvofa; vdev_create auto-disables (ofaQueue stays null) if A lacks
    // VK_NV_optical_flow. The runtime use is FURTHER gated on single_gpu (FD==A) below — under multi-GPU the
    // flow rides B (no OFA) so NVOFA cannot apply (NVOFA is a single-GPU lever).
    // --gpu-priority LEVER 2: VK_EXT/KHR_global_priority on device A's queues ONLY (the warp/present
    // GPU — the one contended by the game); B/G keep default priority (their work is off the game's GPU).
    if(!vdev_create(pA,A,true,/*want_extmem_win32=*/true,/*prefer_same_family_q2=*/true,/*want_xfer_q=*/cfg.upload_xfer,/*want_ofa=*/cfg.nvofa,/*global_priority=*/cfg.gpu_priority)
       || (!single_gpu && !vdev_create(pB,B,false))){ std::printf("[ra] device creation failed\n"); return false; }
    // NOTA: el 4090 soporta OFA por HW pero en multi-GPU el flow corre en el 1080 Ti (sin OFA),
    // así que --nvofa no aplica aquí. Avisar la opción (NO auto-activar: la salida OFA está sin calibrar y
    // usarla obliga a single-GPU, perdiendo el offload de flow del 1080 Ti).
    if(A.has_optical_flow && !single_gpu && !cfg.nvofa)
        std::printf("[ra] NVOFA: el 4090 soporta optical-flow por HW; para usarlo --nvofa --force-single-gpu "
                    "(el flow vuelve al 4090; se pierde el offload de flow del 1080 Ti — medir antes de adoptar).\n");
    // On a single-graphics-queue GPU A.q2==A.q (q2mode falls back to shared). The flow submit would then
    // land on A.q and RACE the present thread P (external-sync violation → device-lost). The 4090 has
    // queueCount>1 so A.q2!=A.q and this passes. Hard-refuse otherwise (the safe first cut; the CPU-mutex-
    // serialize-on-A.q degraded mode is not shipped) — NEVER a silent A.q2→A.q fallthrough.
    if(single_gpu && A.q2==A.q){
        std::printf("[ra] --force-single-gpu: device A has a single graphics queue (A.q2==A.q) — the flow submit "
                    "would race the present thread on A.q (device-lost). REFUSING (the safe first cut). This GPU "
                    "needs the serialize-on-A.q degraded mode (not yet shipped). Use a GPU with >1 graphics queue.\n");
        return false;   // the standard cleanup (A created; nothing else allocated yet) — same as the hard-exit's intent
    }
    if(pG) have_igpu=vdev_create(pG,G,/*want_swap=*/false);  // G is convert-only (never presents); pG is null under --force-single-gpu so this is skipped
    // Force convert onto A (CG_PRIMARY) so the C-thread's convert runs on A.q2, serialized vs the
    // F-thread flow via a_q2_mtx (below). pG was nulled → have_igpu is false → the iGPU convert path is
    // unavailable anyway; this makes the intent explicit.
    if(single_gpu) cfg.convert_gpu=CG_PRIMARY;
    if(have_igpu){   // report G's queue split (convert on q2)
        const char* qmode=(G.q2==G.q)?"shared":(G.qfam2==G.qfam?"split-queue":"split-family");
        const uint32_t cqi=(G.q2==G.q)?0u:(G.qfam2==G.qfam?1u:0u);
        std::printf("[ra] G queues: convert fam=%u q=%u (%s)\n",G.qfam2,cqi,qmode);
    }
    if(single_gpu){   // single-GPU: the flow/gme producer rides A.q2 (the same-family split-queue), serialized vs C's convert by a_q2_mtx.
        std::printf("[ra] SINGLE-GPU: PhyriadFG collapsed onto device A alone (flow+gme+convert on A.q2, present on A.q). B/G suppressed.\n");
    } else {   // report B's queue split — the interp copy-outs overlap on B.q2 when split.
        const char* bqmode=(B.q2==B.q)?"shared (serial copy-out)":(B.qfam2==B.qfam?"split-queue (overlap)":"split-family (overlap)");
        std::printf("[ra] B queues: interp copy-out fam=%u (%s)\n",B.qfam2,bqmode);
    }
    // --cap-route-probe: a break-even-style CAPABILITY routing decision over the FG's app-local VDevs +
    // a vendor-NAMED capability manifest. MEASUREMENT-ONLY (no behaviour change → byte-identical off AND
    // on; it only std::printf's). INERT by design: the FG's A/B/G roles are FIXED by architecture
    // (A=present/warp, B=flow/gme, G=convert) and the FG's arithmetic intensity (~2.5 FLOP/byte) is far
    // below the break-even crossover (~596 on the author's link), so a capability-driven offload decision
    // returns offload=false ALWAYS — the const-inlined invariant this app encodes. This probe does NOT
    // route; it reports what such a decision WOULD say, so the routing is verifiable rather than hidden.
    // The arithmetic mirrors framework/gpu break_even_decide — replicated inline because this app does
    // NOT link phyriad_gpu. Speculative + inert-by-default on NVIDIA / fixed-role FG.
    if(cfg.cap_route_probe){
        // The vendor-NAMED capability manifest — vendor strings confined HERE (named messaging, not a
        // decision path). No vendor_id/name gates ANY behaviour: the routing decision below keys ONLY on
        // capability fields.
        auto vendor_name=[&](const VDev& v)->const char*{
            std::string lo(v.name); for(auto& ch:lo) ch=(char)std::tolower((unsigned char)ch);
            if(lo.find("nvidia")!=std::string::npos||lo.find("geforce")!=std::string::npos) return "NVIDIA";
            if(lo.find("amd")!=std::string::npos||lo.find("radeon")!=std::string::npos)     return "AMD";
            if(lo.find("intel")!=std::string::npos||lo.find("arc")!=std::string::npos)      return "Intel";
            return "other";   // messaging only, not a decision path
        };
        auto cap_line=[&](const char* role,const VDev& v){
            if(v.dev==VK_NULL_HANDLE){ std::printf("[ra] cap-probe %s: (absent)\n",role); return; }
            std::printf("[ra] cap-probe %s: vendor=%s '%s' has_fp16=%d has_dp4a=%d fp16_storage=%d fp16_gflops=%.1f(0=uncharacterized)\n",
                        role,vendor_name(v),v.name,(int)v.has_fp16,(int)v.has_dp4a,(int)v.fp16_storage,v.fp16_gflops);
        };
        cap_line("A(present/warp)",A);
        if(!single_gpu) cap_line("B(flow/gme)",B);
        if(have_igpu)   cap_line("G(convert)",G);
        // The break-even-style decision (mirrors framework/gpu break_even_decide). Inputs are the MEASURED
        // capability fields when characterized, else degenerate → the honest decline. FG AI ≈ 2.5 FLOP/byte.
        // fp16_gflops==0 (not yet characterized — we do not measure it on this path) makes the inputs
        // degenerate ⇒ offload=false by the same guard the pillar uses. Even with characterized FLOPS the FG's
        // AI ≪ crossover keeps offload=false; the FG's A/B/G roles are not chosen by this number anyway.
        auto break_even_offload=[&](double sup_flops,double ass_flops,double link_bps,double ai,double thresh=0.02)->bool{
            if(sup_flops<=0.0||ass_flops<=0.0||link_bps<=0.0||ai<=0.0) return false;   // not-characterized → honest decline
            const double invs=1.0/sup_flops+1.0/ass_flops+1.0/(ai*link_bps);
            const double f=(1.0/sup_flops)/invs; const double speedup=1.0/(1.0-f);
            return (speedup-1.0)>thresh;
        };
        const double fg_ai=2.5;   // FG's arithmetic intensity (FLOP per result byte) — the const this app encodes
        const double sup=(double)A.fp16_gflops*1.0e9;                 // A's measured fp16 FLOP/s (0 ⇒ degenerate)
        const double ass=single_gpu?0.0:(double)B.fp16_gflops*1.0e9;  // B's measured fp16 FLOP/s (0 ⇒ degenerate)
        const double link=0.0;   // cross-GPU link BW not measured on this path (0 ⇒ degenerate → honest decline)
        const bool off=break_even_offload(sup,ass,link,fg_ai);
        std::printf("[ra] cap-probe DECISION: offload=%s (FG AI~%.1f; link/fp16-gflops uncharacterized on this path => degenerate => honest decline). INERT BY DESIGN: the FG's A/B/G roles are architecture-fixed; this decision ROUTES NOTHING -- it reports what a capability-driven gate WOULD say. Coverage/speculative, not a default win.\n", off?"true":"false", fg_ai);
    }
    // Upscale machinery stays COMPILED but is forced off — the output clock is always the timer, and the
    // per-tick upscale path is not supported under it. The bridge blit (FILTER_LINEAR, work→monitor)
    // already scales. So use_upscale is structurally false at runtime.
    use_upscale=(!cfg.no_upscale&&have_igpu);
    if(use_upscale){
        use_upscale=false;
        std::printf("[ra] upscale: unsupported under the surface present path (v1) — forcing --no-upscale (the bridge blit scales)\n");
    }
    // --real-fast-path — upscale-parity refusal. The real-fast-path blits hR_a[rs] (WW×WH) into the
    // bridge via the bslot record; if use_upscale were live, the present source would be the UP_W×UP_H
    // upscaled buffer and the rfp record would NOT run do_upscale_real_P → a wrong-size/garbage present.
    // Rather than replicate the upscale pre-pass on the fast path, REFUSE --rfp under use_upscale.
    // use_upscale is structurally false here (forced off above), so this is belt-and-suspenders —
    // correct-by-construction if upscale is ever re-enabled through the bridge.
    if(cfg.real_fast_path && use_upscale){
        cfg.real_fast_path=false;
        std::printf("[ra] --real-fast-path: --upscale is active → DISABLING --rfp (the fast path presents the native-res real; an upscaled present needs the do_upscale_real_P pre-pass not replicated on this path).\n");
    }
    // use_igpu_convert: iGPU does fused convert+pack in sysRAM (zero PCIe).
    // Requires: iGPU + SDR 8bpc (RT_PASS) or HDR (RT_HDR) + no scaling (NAT==WW).
    // When active: 4090 freed of ALL compute except capture+present; FG stays on B; primary-FG disabled.
    // HDR (FP16 scRGB, DD on an HDR desktop) is an iGPU-convert format — the pack shader has the is_hdr
    // tone-map. Still 1:1 (NAT==WW), so a >1080p HDR desktop falls back to primary.
    // --fg-gpu primary + --convert-gpu primary would make F's flow AND C's convert both want A.q2 (only
    // ONE non-present submitter per queue handle is race-free; A has exactly 2 queues, P owns A.q).
    // Resolve in favour of FG: force convert back to the iGPU. This runs BEFORE the use_igpu_convert=
    // line so the iGPU path re-evaluates correctly (and the "unavailable -> primary" note still fires if
    // the iGPU/format conditions do not hold — in which case primary-FG is disabled later anyway).
    // 2026-09-06 FIX (the operator's fast-death): this override is a MULTI-GPU resolution and must not
    // fire under single_gpu. Line 149 already set CG_PRIMARY there ON PURPOSE — under single-GPU the
    // convert rides A.q2 serialized against F by a_q2_mtx (ingest.cpp:79-82), and the primary-FG path
    // this override protects does not even exist (main.cpp:339 forces want_pfg=false once single_gpu is
    // derived). Flipping convert back to CG_IGPU there re-enabled the iGPU-convert path, which is wired
    // to device B — a zero-initialised VDev{} under single-GPU. Two deaths followed: the host bridge
    // failing at core_init.cpp:500 (hbuf_import on B) when NAT==WW, or, when NAT!=WW, cfg.convert_gpu
    // staying CG_IGPU so ingest.cpp:79's crash guard missed and the convert submitted on A.q against the
    // present thread -> DEVICE_LOST. Gate on !single_gpu; say plainly that the flag is inert instead.
    // NOT the failure path, for whoever reads this next: core_init.cpp:96's `B=MISSING` guard is
    // UNREACHABLE DEAD CODE — line 82 already returns on !pA, so past it !pB implies single_gpu true and
    // that branch can never be entered. It is left in place deliberately (removing it is a cleanup, not
    // a fix); do not mistake it for where the single-GPU `--fg-gpu primary` run died.
    if(cfg.fg_gpu==FG_PRIMARY && cfg.convert_gpu==CG_PRIMARY && !single_gpu){
        std::printf("[ra] --fg-gpu primary + --convert-gpu primary both target A.q2 (only one non-present submitter per queue) — forcing convert to iGPU; flow keeps A.q2.\n");
        cfg.convert_gpu=CG_IGPU;
    } else if(cfg.fg_gpu==FG_PRIMARY && single_gpu){
        std::printf("[ra] --fg-gpu primary: INERT on the single-GPU topology (primary-FG is forced off once single_gpu is derived). Convert stays on A.q2 (serialized vs the flow by a_q2_mtx); the iGPU-convert path needs device B and is not available here.\n");
    }
    // The iGPU-convert path is STRUCTURALLY device-B dependent (hRP_b / hRP_b_dev at core_init.cpp:500-501,
    // unpipe_create(B,...) at capture_init.cpp:66, Bframe ingest), and B is never created under single_gpu
    // (core_init.cpp:128 skips vdev_create(pB,B)). Making that a term here means no future edit can reach
    // the path with a null B — belt-and-suspenders over the gate above, not a substitute for it.
    use_igpu_convert=(cfg.convert_gpu==CG_IGPU&&!single_gpu&&have_igpu&&NAT_W==WW&&NAT_H==WH&&(route==RT_PASS||route==RT_HDR));
    if(cfg.convert_gpu==CG_IGPU&&!use_igpu_convert)
        std::printf("[ra] igpu-convert: unavailable (no iGPU or NAT!=WW) -> primary\n");
    if(use_igpu_convert)
        std::printf("[ra] igpu-convert: ACTIVE — iGPU fused convert+pack%s, B ingests packed\n",
            IS_HDR?" (HDR tone-map)":"");
    // WAP rides the surface path on A (the bridge owner) — always available.
    use_wap=cfg.warp_at_presenter;
    // --fwd-prestage: the prestage only has a copy to collapse on the iGPU-convert path (the only one
    // with the inline hRP_b[s]->hRP_b_dev[s] copy at the F-build top) AND only matters on the WAP path
    // (the serial WAP build is the one whose blocking flow submit fronts the copy). Force-OFF otherwise
    // so the flag is a clean no-op and the alloc/runtime/destroy all key on this single derived bool. The
    // serial-vs-pipeline split is decided at the per-pair use site (the fwd_pipeline path already overlaps
    // the same copy, so the prestage stands down there). Off → byte-identical (the copy stays inline).
    use_fwd_prestage = cfg.fwd_prestage && use_wap && use_igpu_convert;
    if(cfg.fwd_prestage && !use_fwd_prestage)
        std::printf("[ra] --fwd-prestage: requires --warp-at-presenter + the iGPU-convert ingest path (the only build with the inline hRP_b->dev copy) — disabled (no-op, byte-identical)\n");
    if(use_fwd_prestage)
        std::printf("[ra] --fwd-prestage: ACTIVE — the hRP_b->hRP_b_dev device copy splits to the prestage ping-pong (A.q2 no-wait, overlaps the flow record); cmdF keeps the hRP_b_dev barrier (same-queue cross-submission edge)\n");
    // Bidir is WAP-path only (the classification lives in A's warp). Without --warp-at-presenter there
    // is no MV field shipped to classify against, so --bidir without WAP is a no-op with a note.
    use_bidir=cfg.bidir&&use_wap;
    if(cfg.bidir&&!use_wap) std::printf("[ra] --bidir requires --warp-at-presenter (no WAP MV field to classify) — bidir disabled\n");
    // Fill-div directs the bidir neither-class sliver — alive only when bidir is (parse_args already
    // errored out on --fill-div without --bidir; this also covers --bidir disabled by no-WAP).
    use_fill_div=cfg.fill_div&&use_bidir;
    if(cfg.fill_div&&!use_bidir) std::printf("[ra] --fill-div needs an active bidir classification (disabled with bidir) — fill-div disabled\n");
    // Candidate-rescue runs in A's warp on the d_ab commit trigger — WAP-only (no commit path off WAP)
    // and only when commit is armed (parse_args already errored on --rescue without --commit-warp; this
    // also covers WAP disabled). The shader gate is rescue_on>0.5 AND commit_thresh>0.
    use_rescue=cfg.rescue&&use_wap&&cfg.commit_thresh>0.f;
    if(cfg.rescue&&!use_wap) std::printf("[ra] --rescue requires --warp-at-presenter (the commit/rescue path is the WAP warp) — rescue disabled\n");
    // The MV vector-median filters the WAP MV image(s) in wap_upload — WAP-only (no WAP MV image off
    // the WAP path). Independent of commit/bidir (it's a field-consensus prefilter); the bidir field
    // (wapMVBA) is median-filtered too when bidir is active.
    use_mv_median=cfg.mv_median&&use_wap;
    if(cfg.mv_median&&!use_wap) std::printf("[ra] --mv-median requires --warp-at-presenter (no WAP MV field to filter) — mv-median disabled\n");
    // Color-guided MV — WAP-only (the consensus pass filters the WAP MV image; the bilateral fetch is
    // in the WAP warp). parse_args already HARD-errored on --mv-guided without --warp-at-presenter, so
    // this is belt-and-suspenders for a non-flag WAP disable.
    use_mv_guided=cfg.mv_guided&&use_wap;
    if(cfg.mv_guided&&!use_wap) std::printf("[ra] --mv-guided requires --warp-at-presenter (no WAP MV field to guide) — mv-guided disabled\n");
    // --mv-edge-snap is the WAP-warp's cross-bilateral primary-MV fetch — same WAP requirement as mv-guided.
    if(cfg.mv_edge_snap&&!use_wap){ std::printf("[ra] --mv-edge-snap requires --warp-at-presenter (no WAP MV field to snap) — mv-edge-snap disabled\n"); cfg.mv_edge_snap=0; }
    // --single-track lives at the WAP warp's composition — WAP-only.
    if(cfg.single_track&&!use_wap){ std::printf("[ra] --single-track requires --warp-at-presenter (no WAP warp to base on the B-track) — single-track disabled\n"); cfg.single_track=false; }
    // --bg-reclaim damps the fringe MV toward the gme model — needs WAP AND gme (the runtime push also
    // gates on gme_push, so it is inert without a valid model; here we disable early + warn for the banner).
    if(cfg.bg_reclaim>0.f&&!use_wap){ std::printf("[ra] --bg-reclaim requires --warp-at-presenter (no WAP MV to reclaim) — bg-reclaim disabled\n"); cfg.bg_reclaim=0.f; }
    if(cfg.bg_reclaim>0.f&&!cfg.gme){ std::printf("[ra] --bg-reclaim requires --gme (the background model) — bg-reclaim disabled\n"); cfg.bg_reclaim=0.f; }
    // The frame-holon (global affine fit + dissidence mask + model rescue + fill-div assist) is WAP-only
    // — the fit reads the shipped fwd MV grid and the model is consumed in A's warp. parse_args already
    // HARD-errored on --gme without --warp-at-presenter; this is belt-and-suspenders for a non-flag WAP
    // disable (e.g. WAP bridge/image alloc failure flipped use_wap off above).
    use_gme=cfg.gme&&use_wap;
    if(cfg.gme&&!use_wap) std::printf("[ra] --gme requires --warp-at-presenter (no WAP MV field to fit) — gme disabled\n");
    // The ambiguity channel — second-best candidate arbitration of SAD ties. Needs use_gme (the warp
    // referee is the gme background model gme_model_mv AND the dissidence masks gate the OBJECT
    // exemption; gme already requires WAP). Resolved HERE (before ofp.init) so emit_second_best can be
    // armed on B's pipeline. The cascades already cleared cfg.ambig with cfg.gme; this is the live gate.
    use_ambig=cfg.ambig&&use_gme;
    if(cfg.ambig&&!use_gme) std::printf("[ra] --ambig requires gme (the referee is the gme background model) — ambiguity disabled\n");
    // The inertia prior — F-side per-block persistence counter + the three WAP-warp motion-restriction
    // gates. WAP-only (the persistence rides the shipped MV grid; the gates live in A's warp). parse_args
    // already HARD-errored on --inertia without --warp-at-presenter; this is belt-and-suspenders for a
    // non-flag WAP disable (e.g. a WAP bridge/image alloc failure flipped use_wap off above).
    use_inertia=cfg.inertia&&use_wap;
    if(cfg.inertia&&!use_wap) std::printf("[ra] --inertia requires --warp-at-presenter (no WAP MV field to track) — inertia disabled\n");
    // The commit-default flip is a WARP-LEVEL rule — gated ONLY on use_wap, MATTE-INDEPENDENT (it floors
    // the warp-vs-blend selection for ANY low-confidence warp pixel, inside or outside a matte; the
    // fringe/sub-mask-text lives outside the silhouette). Belt-and-suspenders for a non-flag WAP disable
    // (a WAP image-alloc failure that flipped use_wap off after parse_args).
    use_commit_default=cfg.commit_default&&use_wap;
    if(cfg.commit_default&&!use_wap) std::printf("[ra] --commit-default requires --warp-at-presenter (no warp-vs-blend selection without WAP) — commit-default disabled\n");
    // The one-position collapse is a WARP-LEVEL rule — gated ONLY on use_wap, MATTE-INDEPENDENT (it
    // collapses the warp_result composition for ANY warp pixel whose two displaced samples disagree —
    // the nameplate TEXT, mv=0, lives outside any matte). Belt-and-suspenders for a non-flag WAP disable
    // (a WAP image-alloc failure that flipped use_wap off after parse_args).
    use_onepos=cfg.onepos&&use_wap;
    if(cfg.onepos&&!use_wap) std::printf("[ra] --onepos requires --warp-at-presenter (no warp_result composition without WAP) — one-position disabled\n");
    // The object-holon — F-side connected-component clustering of the gme dissidence mask, a 16-slot
    // temporal-identity table, and the conservative motion-inheritance repair. Needs use_gme (it clusters
    // hostDIS/hostDISB, the gme masks, and reuses the gme half-float decode). The HUD shield reads
    // persist[] (the inertia array) — only populated under use_inertia; when inertia is off the shield
    // simply does not fire (persist treated as 0), which is acceptable (a degenerate phase still fades by
    // the dissidence recompute, and the shield is an exemption, not a requirement). parse_args cascades
    // --no-gme/--no-wap onto objects; this is belt-and-suspenders for a non-flag gme disable (e.g. a WAP
    // image-alloc failure flipped use_wap → use_gme off above).
    use_objects=cfg.objects&&use_gme;
    if(cfg.objects&&!use_gme) std::printf("[ra] --objects requires --gme (no dissidence mask to cluster) — object-holon disabled\n");
    // --mv-guided SUPERSEDES --mv-median's blind pass. The same median compute pass runs (it reads
    // cur_real + a sim_thresh push), but in CONSENSUS mode (sim_thresh>0): color-membership-weighted
    // votes instead of the blind component-wise median. --mv-median alone (no guided) keeps sim_thresh=0
    // → the blind McGuire path. So the pass is enabled for EITHER flag; guided just changes the mode.
    if(use_mv_guided&&use_mv_median) std::printf("[ra] --mv-guided supersedes --mv-median (the consensus pass runs color-weighted, not blind)\n");
    // NOTE: the pipeline is CREATED when (use_mv_median||use_mv_guided) and the per-pair dispatch
    // re-checks the SAME live bools (see wap_upload). No named local is introduced here — the
    // goto done/fail pattern forbids crossing a non-trivial initializer with the jumps above.
    std::printf("[ra] A(primary): %-28s | B(FG): %s%s\n",A.name,B.name,
        use_igpu_convert?" | G(convert)":(!have_igpu&&!cfg.no_upscale?" (no iGPU)":""));
    std::printf("[ra] fg-gpu: %s (want-primary=%d)\n",
        cfg.fg_gpu==FG_AUTO?"auto":cfg.fg_gpu==FG_PRIMARY?"primary":"assist",
        (int)want_pfg);
    return true;
}
// ── Host bridge (E1: moved VERBATIM from main.cpp) — the aligned host allocations + every
// per-device EMH import, the capture-ring auto-size, the --ingest-async raw ring and the WAP/
// gme/inertia host bridges.
bool init_host_bridge(Config& cfg, D3D& d, bool single_gpu, bool want_pfg,
                      uint32_t NAT_W, uint32_t NAT_H, uint32_t nat_bpp,
                      uint32_t WW, uint32_t WH, uint32_t UP_W, uint32_t UP_H,
                      uint32_t WW_flow, uint32_t WH_flow, int cap_mon_hz,
                      DevicesInit& o_dev, VDev& FD, GmeGpuInit& o_gme, ImagesInit& o_img,
                      HostBridgeInit& o_host){
    VDev& A=o_dev.A; VDev& B=o_dev.B; VDev& G=o_dev.G;
    auto& have_igpu=o_dev.have_igpu; auto& use_upscale=o_dev.use_upscale;
    auto& use_igpu_convert=o_dev.use_igpu_convert;
    auto& use_wap=o_dev.use_wap; auto& use_bidir=o_dev.use_bidir; auto& use_ambig=o_dev.use_ambig;
    auto& dxgi_stage2=o_img.dxgi_stage2;
    auto& hDIS_b=o_gme.hDIS_b; auto& hDISB_b=o_gme.hDISB_b;
    auto& hostGmeM=o_gme.hostGmeM; auto& hostGmeMB=o_gme.hostGmeMB;
    auto& hGmeM_b=o_gme.hGmeM_b; auto& hGmeMB_b=o_gme.hGmeMB_b;
    auto& cap_slots=o_host.cap_slots;
    auto& hostR=o_host.hostR; auto& hostI=o_host.hostI; auto& hostG=o_host.hostG;
    auto& hostA=o_host.hostA; auto& hostRP=o_host.hostRP; auto& hostFIELD=o_host.hostFIELD;
    auto& hR_a=o_host.hR_a; auto& hR_b=o_host.hR_b; auto& hR_g=o_host.hR_g;
    auto& hI_a=o_host.hI_a; auto& hI_b=o_host.hI_b; auto& hI_g=o_host.hI_g;
    auto& hGout=o_host.hGout; auto& hApres=o_host.hApres;
    auto& hRP_g=o_host.hRP_g; auto& hRP_b=o_host.hRP_b;
    auto& hFIELD_g=o_host.hFIELD_g; auto& hFIELD_a=o_host.hFIELD_a;
    auto& hRP_b_dev=o_host.hRP_b_dev; auto& Astage_g=o_host.Astage_g; auto& Astage=o_host.Astage;
    auto& raw_host=o_host.raw_host; auto& raw_astage_a=o_host.raw_astage_a; auto& raw_astage_g=o_host.raw_astage_g;
    auto& hostMV=o_host.hostMV; auto& hostSAD=o_host.hostSAD;
    auto& hMV_b=o_host.hMV_b; auto& hSAD_b=o_host.hSAD_b;
    auto& hostC2=o_host.hostC2; auto& hC2_b=o_host.hC2_b;
    auto& hostMVB=o_host.hostMVB; auto& hMVB_b=o_host.hMVB_b;
    auto& hostDIS=o_host.hostDIS; auto& hDIS_a=o_host.hDIS_a;
    auto& hostDISB=o_host.hostDISB; auto& hDISB_a=o_host.hDISB_a;
    auto& hostPER=o_host.hostPER; auto& hPER_a=o_host.hPER_a;
    auto& hMV_a=o_host.hMV_a; auto& hSAD_a=o_host.hSAD_a;
    auto& hMVB_a=o_host.hMVB_a; auto& hC2_a=o_host.hC2_a;
    auto& pres_w=o_host.pres_w; auto& pres_h=o_host.pres_h;
    // ── Host bridge ───────────────────────────────────────────────────────────
    // Use one alignment granule that satisfies every active device so both
    // allocations can be imported by any combination of A / B / G.
    {
        const uint64_t al=std::max<uint64_t>({A.host_align,B.host_align,have_igpu?G.host_align:1u,1u});
        const VkDeviceSize wwork=VkDeviceSize(WW)*WH*4u;
        const VkDeviceSize hwork=(wwork+al-1)/al*al;
        // ── Auto-size the capture ring from the RUNTIME regime (NOT the CPU) ──────────────
        // The ring is RAM frame-slots; the depth must cover how many reals advance while B flows the pair
        // + P consumes it = the source/flow gap. The source ceiling = --cap-fps, or the CAPTURED
        // monitor's compose rate (cap_mon_hz, derived above — uniques can never exceed it; the old
        // 125 hardcode was the retired WGC anti-flood cap). Base of 4, then clamped to
        // [4, kCapSlots(max)] AND to a host-memory budget (~768MB; each slot ~ W*H*4 * 2.7 for R +
        // packed + field). --cap-slots N overrides (0 = auto). Deliberately NOT a function of CPU
        // cores/topology: that pillar (HardwareTopology / Scheduler) governs THREAD-core placement, a
        // separate concern; the ring depth is source/flow/res-bound. (This is an init-time heuristic
        // from the cap CEILING, not a measured per-run optimum; the override + the frz/uniq stats are
        // the tuning path.)
        {
            const int src_ceiling = (cfg.cap_fps>0) ? cfg.cap_fps : cap_mon_hz;
            int want = (cfg.cap_slots>0) ? cfg.cap_slots : (src_ceiling/10 + 4);
            const double slot_bytes = (double)WW*(double)WH*4.0*2.7;            // R + packed + field, approx
            const int mem_cap = (int)(768.0*1024.0*1024.0 / slot_bytes);
            if(want>mem_cap) want=mem_cap;
            if(want<kCapSlotsMin) want=kCapSlotsMin;                            // floor = the torn-read invariant (>= kIngestBacklog+1, static_asserted)
            if(want>kCapSlots) want=kCapSlots;                                  // the compile-time MAX (handle-array size)
            cap_slots = want;
            std::printf("[ra] capture-ring: %d slots (%s; source ceiling %d fps; ~%.0f MB) — auto-sized to the source/flow/res gap, NOT CPU topology\n",
                        cap_slots, cfg.cap_slots>0?"--cap-slots override":"auto", src_ceiling, (double)cap_slots*slot_bytes/1048576.0);
        }
        // kCapSlots real slots; each slot has independent R / RP host bridges.
        // hostR[s]: REAL frame slot s — A writes (hR_a[s]), B reads (hR_b[s]);
        // G reads/writes (hR_g[s]) when upscale / igpu_convert.
        {
            const VkBufferUsageFlags hRg_use=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                            |(use_igpu_convert?(VkBufferUsageFlags)VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:0u);
            for(int _s=0;_s<cap_slots;++_s){
                hostR[_s]=_aligned_malloc((size_t)hwork,(size_t)al);
                if(!hostR[_s]||!hbuf_import(A,hostR[_s],hwork,hR_a[_s])||!hbuf_import(FD,hostR[_s],hwork,hR_b[_s])||   // FD-route (B→A under single_gpu) so the on-A copy-out writes a VALID hR_b handle (not a silent early-return)
                   ((use_upscale||use_igpu_convert)&&!hbuf_import(G,hostR[_s],hwork,hR_g[_s],hRg_use,/*q2_shared=*/use_igpu_convert)))
                    { std::printf("[ra] host bridge R[%d] failed\n",_s); return false; }
            }
            // iGPU contour-field host bridge. G writes hFIELD_g via SSBO; the CPU reads hostFIELD for the
            // --igpu-field-verify byte gate. Size = hwork (W*H*4 = 1 uint/px). Needs the iGPU convert path
            // (the field reads hR_g, the convert's RGBA output).
            if(cfg.igpu_field && !use_igpu_convert){
                // No iGPU convert path → the field can't be produced, so the field AND EVERY consumer must
                // go off. Set igpu_field=false then RE-RESOLVE via the central apply_cascades (cli.cpp) so
                // afill/bg-snap/band-xfade/disoccl-hardpick cascade off AND cfg.d.field_to_warp is re-derived
                // in ONE place. announce=false: the cascade detail is suppressed; the one-line summary below
                // covers it.
                std::printf("[ra] --igpu-field ignored: needs the iGPU convert path (no iGPU or NAT!=WW); cascading off the field consumers (afill/bg-snap/band-xfade/disoccl-hardpick).\n");
                cfg.igpu_field=false;
                apply_cascades(cfg, /*announce=*/false);
            }
            if(cfg.igpu_field){
                for(int _s=0;_s<cap_slots;++_s){
                    hostFIELD[_s]=_aligned_malloc((size_t)hwork,(size_t)al);
                    if(!hostFIELD[_s]||!hbuf_import(G,hostFIELD[_s],hwork,hFIELD_g[_s],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,/*q2_shared=*/true))
                        { std::printf("[ra] host bridge FIELD[%d] failed\n",_s); return false; }
                    // A imports the SAME hostFIELD pointer as TRANSFER_SRC — A uploads the field (which G
                    // wrote) into wapFIELDA each pair; the fill OR the warp reads it (--afill OR --bg-snap).
                    if(cfg.d.field_to_warp && !hbuf_import(A,hostFIELD[_s],hwork,hFIELD_a[_s],VK_BUFFER_USAGE_TRANSFER_SRC_BIT))   // field_to_warp = ALL consumers (afill||bg_snap||band_xfade||disoccl_hardpick||mc_on), not just afill||bg_snap
                        { std::printf("[ra] host bridge FIELD_a[%d] failed\n",_s); return false; }
                }
            }
        }
        // 2 interp generations × NI interps per generation.
        // hostI[g][k]: B writes (hI_b[g][k]); A reads (hI_a[g][k]) for the grid present (the only
        // present path — the surface bridge reads hostI). G reads (hI_g[g][k]) only on the (forced-off)
        // upscale path.
        {
            const int NI_alloc=std::min(std::max(1,cfg.fg_factor-1),kMaxInterp);  // belt vs overrun
            for(int _g=0;_g<kGenRing;++_g) for(int _k=0;_k<NI_alloc;++_k){
                hostI[_g][_k]=_aligned_malloc((size_t)hwork,(size_t)al);
                if(!hostI[_g][_k]||!hbuf_import(FD,hostI[_g][_k],hwork,hI_b[_g][_k])||   // FD-route hI_b (B→A under single_gpu); the warp copy-out targets it
                   (use_upscale ? !hbuf_import(G,hostI[_g][_k],hwork,hI_g[_g][_k])
                                : !hbuf_import(A,hostI[_g][_k],hwork,hI_a[_g][_k])))
                    { std::printf("[ra] host bridge I[%d][%d] failed\n",_g,_k); return false; }
                if(want_pfg&&!hI_a[_g][_k].buf&&!hbuf_import(A,hostI[_g][_k],hwork,hI_a[_g][_k]))
                    { std::printf("[ra] host bridge I(pfg)[%d][%d] failed\n",_g,_k); return false; }
            }
        }
        // hostG: UP_W×UP_H×4 — G writes upscale output (hGout); A reads for present (hApres).
        if(use_upscale){
            const VkDeviceSize wg=VkDeviceSize(UP_W)*UP_H*4u;
            const VkDeviceSize hg=(wg+al-1)/al*al;
            hostG=_aligned_malloc((size_t)hg,(size_t)al);
            if(!hostG||!hbuf_import(G,hostG,hg,hGout)||!hbuf_import(A,hostG,hg,hApres))
                { std::printf("[ra] host bridge G failed\n"); return false; }
            pres_w=UP_W; pres_h=UP_H;
        }
        // hostRP[s] = packed RGB8 per real slot (× kCapSlots).
        if(use_igpu_convert){
            const VkDeviceSize wrp=VkDeviceSize(WW)*WH*3u;
            const VkDeviceSize hrp=(wrp+al-1)/al*al;
            for(int _s=0;_s<cap_slots;++_s){
                hostRP[_s]=_aligned_malloc((size_t)hrp,(size_t)al);
                if(!hostRP[_s]
                   ||!hbuf_import(G,hostRP[_s],hrp,hRP_g[_s],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,/*q2_shared=*/true)
                   ||!hbuf_import(B,hostRP[_s],hrp,hRP_b[_s],VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                   ||!dbuf_create(B,wrp,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,hRP_b_dev[_s]))
                    { std::printf("[ra] host bridge RP[%d] failed\n",_s); return false; }
            }
        }
    }
    // Astage host buffer — aligned for EMH import on both A and G (same CPU pointer, importable as
    // SSBO on G).
    {
        const uint64_t al=std::max<uint64_t>({A.host_align,use_igpu_convert?G.host_align:1u,1u});
        const VkDeviceSize ab=VkDeviceSize(NAT_W)*NAT_H*nat_bpp;
        const VkDeviceSize hab=(ab+al-1)/al*al;
        hostA=_aligned_malloc((size_t)hab,(size_t)al);
        // hab (alignment-rounded), NOT ab: EMH vkAllocateMemory needs the size to be a multiple of
        // minImportedHostPointerAlignment. 1920x1080x4 happens to be 4096-aligned; odd window sizes
        // (1264x681) are not. Every hbuf_import site rounds the same way.
        if(!hostA
           ||!hbuf_import(A,hostA,hab,Astage,VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
           ||(use_igpu_convert&&!hbuf_import(G,hostA,hab,Astage_g,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)))
            { std::printf("[ra] Astage import failed\n"); return false; }
        // ── --ingest-async: the RAW host-buffer ring (+ the 2nd DDA staging texture) ─────────
        // BOTH APIs (WGC_INGEST_ASYNC_PLAN.md): the raw ring + worker decouple the convert from the
        // pickup/acquire loop. Only the readback double-buffer (dxgi_stage2) is DDA-specific — WGC
        // ingests via its own callback staging ring and never touches dxgi_stage/dxgi_stage2. On any
        // raw-ring alloc failure we free what we got and fall back to serial (never abort an opt-in
        // perf flag) — the fallback covers both APIs (R4).
        if(cfg.ingest_async){
            bool ok=true;
            for(int _k=0;_k<kRawSlots && ok;++_k){
                raw_host[_k]=_aligned_malloc((size_t)hab,(size_t)al);   // same hab/al as Astage → same importability on A+G
                ok = raw_host[_k]
                   && hbuf_import(A,raw_host[_k],hab,raw_astage_a[_k],VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                   && (!use_igpu_convert || hbuf_import(G,raw_host[_k],hab,raw_astage_g[_k],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
            }
            // The readback double-buffer: a 2nd DDA staging texture (slot-1; dxgi_stage is slot-0). CA_DD only.
            if(ok && cfg.capture_api==CA_DD){ dxgi_stage2=d3d_staging(d,NAT_W,NAT_H); ok=(dxgi_stage2!=nullptr); }
            if(!ok){
                std::printf("[ra] --ingest-async: raw-ring alloc failed — falling back to the serial capture path\n");
                cfg.ingest_async=false;
                for(int _k=0;_k<kRawSlots;++_k){ hbuf_destroy(A,raw_astage_a[_k]); if(use_igpu_convert) hbuf_destroy(G,raw_astage_g[_k]); if(raw_host[_k]){ _aligned_free(raw_host[_k]); raw_host[_k]=nullptr; } }
                if(dxgi_stage2){ rel(dxgi_stage2); dxgi_stage2=nullptr; }
            } else {
                std::printf("[ra] --ingest-async: ARMED (%s) — %d-slot raw ring%s; convert worker thread will spawn\n",
                    cfg.capture_api==CA_DD?"DDA":"WGC",kRawSlots,cfg.capture_api==CA_DD?" + readback double-buffer":"");
            }
        }
    }
    // Warp-at-presenter MV+SAD host bridges (per generation). The MV/SAD grid is (WW/8)×(WH/8) RG16F
    // (=4 bytes/texel; matches OFP's kBlockSize=8). F copies B's MV+SAD images into hMV_b/hSAD_b
    // (TRANSFER_DST); the presenter uploads them into its sampled images from hMV_*/hSAD_* (TRANSFER_SRC).
    // hbuf_import's default usage (TRANSFER_SRC|DST) covers both. The presenter is A (the bridge owner),
    // so the upload-side import is on A (hMV_a/hSAD_a) — taken with the SAME align-rounded size hfb (EMH
    // vkAllocateMemory needs size %% minImportedHostPointerAlignment == 0).
    if(use_wap){
        const uint64_t al=std::max<uint64_t>({B.host_align,A.host_align,1u});
        // This is the ONLY MV-grid site that runs BEFORE ofp.init (the host bridges are allocated here),
        // so it cannot read ofp.motion_width() yet. It derives the grid from WW_flow/WH_flow — THE inputs
        // about to be passed to ofp.init — so it equals ofp.motion_width() by construction
        // (create_mv_image: mv_w_=(width+7)/8). The post-init sites below read the accessor directly; a
        // runtime assert after init confirms the two never diverge. At flow_div==1, WW_flow==WW → (WW+7)/8.
        const uint32_t mvw=(WW_flow+7u)/8u, mvh=(WH_flow+7u)/8u;
        const VkDeviceSize fb=VkDeviceSize(mvw)*mvh*4u;     // RG16F
        const VkDeviceSize hfb=(fb+al-1)/al*al;
        for(int _g=0;_g<kGenRing;++_g){
            hostMV[_g]=_aligned_malloc((size_t)hfb,(size_t)al);
            hostSAD[_g]=_aligned_malloc((size_t)hfb,(size_t)al);
            // hfb (alignment-rounded), NOT fb: the EMH import's vkAllocateMemory requires the
            // size to be a multiple of minImportedHostPointerAlignment (the gate-found failure).
            bool ok = hostMV[_g] && hostSAD[_g]
                && hbuf_import(FD,hostMV[_g],hfb,hMV_b[_g]) && hbuf_import(FD,hostSAD[_g],hfb,hSAD_b[_g])   // FD-route the write-side MV/SAD bridges (B→A under single_gpu); the on-A copy-out writes them, the CPU gme fit + A warp read the same host pointer
                && hbuf_import(A,hostMV[_g],hfb,hMV_a[_g]) && hbuf_import(A,hostSAD[_g],hfb,hSAD_a[_g]);
            if(ok && use_bidir){
                // The backward-MV bridge — same RG16F dims/hfb discipline. Imported on B (write-side,
                // TRANSFER_DST for the copy-out) and A (read-side, TRANSFER_SRC upload).
                hostMVB[_g]=_aligned_malloc((size_t)hfb,(size_t)al);
                ok = hostMVB[_g]
                  && hbuf_import(FD,hostMVB[_g],hfb,hMVB_b[_g])   // FD-route so single_gpu keeps bidir ON (not silently disabled by a null-B early-return)
                  && hbuf_import(A,hostMVB[_g],hfb,hMVB_a[_g]);
                if(!ok){ std::printf("[ra] WAP MV_bwd bridge[%d] failed — disabling bidir\n",_g); use_bidir=false; ok=true; }
            }
            // The second-best CANDIDATE bridge — RGBA16F (8 bytes/texel, NOT 4), so its OWN
            // align-rounded size cfb (mvw×mvh×8 rounded UP to al, the same EMH discipline hostMV uses —
            // POINTER aligned to al via _aligned_malloc AND SIZE a multiple of al via the round). Imported
            // on B (write-side, TRANSFER_DST for the FWD-pass copy-out from cand_image()) AND A (read-side,
            // TRANSFER_SRC upload into wapC2A). Only allocated under use_ambig (cfg.ambig && use_gme). On
            // failure we clear use_ambig (the warp falls to byte-identical off — push 0, placeholder binding)
            // and keep WAP alive (the candidate is additive). cand_image() was already created by ofp.init
            // (emit_second_best=use_ambig); if use_ambig were off here the whole block is skipped.
            if(ok && use_ambig){
                const VkDeviceSize cfb_raw=VkDeviceSize(mvw)*mvh*8u;   // RGBA16F: 8 bytes/texel
                const VkDeviceSize cfb=(cfb_raw+al-1)/al*al;
                hostC2[_g]=_aligned_malloc((size_t)cfb,(size_t)al);
                bool okc = hostC2[_g]
                  && hbuf_import(FD,hostC2[_g],cfb,hC2_b[_g])   // FD-route so single_gpu keeps ambiguity ON
                  && hbuf_import(A,hostC2[_g],cfb,hC2_a[_g]);
                if(!okc){ std::printf("[ra] WAP candidate bridge[%d] failed — disabling ambiguity\n",_g); use_ambig=false; }
            }
            // The dissidence mask bridge — R8 (1 byte/texel), so its own align-rounded size dfb (NOT hfb,
            // which is the RG16F 4-byte size). F fills hostDIS[_g] by CPU memcpy after the fit; A imports
            // it (read-side, TRANSFER_SRC upload into wapDISA). No B-side import (F writes it on the CPU).
            // Disabling gme on failure keeps WAP alive (the mask is additive).
            if(ok && cfg.gme){
                const VkDeviceSize db=VkDeviceSize(mvw)*mvh;       // R8: 1 byte/texel
                const VkDeviceSize dfb=(db+al-1)/al*al;
                hostDIS[_g]=_aligned_malloc((size_t)dfb,(size_t)al);
                bool okd = hostDIS[_g] && hbuf_import(A,hostDIS[_g],dfb,hDIS_a[_g]);
                // The backward (cur-anchored) dissidence bridge — SAME R8 dfb discipline as hostDIS, same
                // A-side-only import (F writes it on the CPU after the bwd fit, A uploads into wapDISBA).
                // Allocated whenever gme is on AND bidir is active (the bwd field exists). The --matte gate
                // already requires --bidir, so for the matte case this is always taken.
                if(okd && use_bidir){
                    hostDISB[_g]=_aligned_malloc((size_t)dfb,(size_t)al);
                    okd = hostDISB[_g] && hbuf_import(A,hostDISB[_g],dfb,hDISB_a[_g]);
                }
                // ── gme-gpu: the B-side of the dissidence bridges + the model readback. The GPU dissidence
                // pass WRITES the mask into hostDIS (atomicOr) — so it needs a B import as a STORAGE buffer
                // (the dissidence set's binding-2 target). And the 6-float model SSBO is copied into a
                // host-coherent readback buffer (hostGmeM[_g]) that F reads in consume_wap. Only allocated
                // when cfg.gme_gpu; any failure clears cfg.gme_gpu (falls back to the CPU gme_fit_affine —
                // the dis bridges + A-side imports are identical either way). hostDIS is imported on A above
                // (read-side); here we ADD the B write-side + bwd + model. Under single_gpu the GPU gme pass
                // is forced OFF at the B.dev==null guard (→ CPU gme_fit_affine, which reads/writes the host
                // pointers hostDIS/hostMV directly — no device handle needed). So SKIP these imports under
                // single_gpu (gating them, NOT routing dead-but-valid aliases): cfg.gme_gpu stays set here
                // and the guard routes to CPU with its own LOUD "device B unavailable -> CPU gme fallback"
                // message. gme itself (the CPU fit) stays ON (the stat prints gme(dis:NN%)).
                if(okd && cfg.gme_gpu && !single_gpu){
                    // STORAGE (the dissidence atomicOr target) + TRANSFER_DST (the per-pass vkCmdFillBuffer
                    // 0-clear before the dissidence dispatch — non-dissident blocks stay 0, matching the CPU).
                    const VkBufferUsageFlags du=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                    bool okg = hbuf_import(B,hostDIS[_g],dfb,hDIS_b[_g],du);
                    if(okg && use_bidir) okg = hbuf_import(B,hostDISB[_g],dfb,hDISB_b[_g],du);
                    if(okg){
                        const VkDeviceSize mb_raw=6u*sizeof(float);
                        const VkDeviceSize mb=(mb_raw+al-1)/al*al;
                        hostGmeM[_g]=_aligned_malloc((size_t)mb,(size_t)al);
                        okg = hostGmeM[_g] && hbuf_import(B,hostGmeM[_g],mb,hGmeM_b[_g],VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                        if(okg && use_bidir){
                            hostGmeMB[_g]=_aligned_malloc((size_t)mb,(size_t)al);
                            okg = hostGmeMB[_g] && hbuf_import(B,hostGmeMB[_g],mb,hGmeMB_b[_g],VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                        }
                    }
                    if(!okg){ std::printf("[ra] WAP gme-gpu bridge[%d] failed — falling back to CPU gme\n",_g); cfg.gme_gpu=false; }
                }
                if(!okd){ std::printf("[ra] WAP DIS bridge[%d] failed — disabling gme\n",_g); cfg.gme=false; }
            }
            // The inertia persistence bridge — SAME R8 dfb discipline as hostDIS (1 byte/MV-block), SAME
            // A-side-only import (F memcpy's its continuous persist[] into hostPER[_g], A uploads into
            // wapPERA). Independent of gme — gated only on cfg.inertia. On failure we clear cfg.inertia (the
            // gates fall to byte-identical-off) and keep WAP alive (the prior is additive).
            if(ok && cfg.inertia){
                const VkDeviceSize db=VkDeviceSize(mvw)*mvh;       // R8: 1 byte/MV-block
                const VkDeviceSize dfb=(db+al-1)/al*al;
                hostPER[_g]=_aligned_malloc((size_t)dfb,(size_t)al);
                bool okp = hostPER[_g] && hbuf_import(A,hostPER[_g],dfb,hPER_a[_g]);
                if(!okp){ std::printf("[ra] WAP PER bridge[%d] failed — disabling inertia\n",_g); cfg.inertia=false; }
            }
            if(!ok){ std::printf("[ra] WAP MV/SAD bridge[%d] failed — disabling warp-at-presenter\n",_g); use_wap=false; break; }
        }
    }
    return true;
}

// ── Images (E1: moved VERBATIM from main.cpp).
bool init_images(Config& cfg, D3D& d, uint32_t NAT_W, uint32_t NAT_H, VkFormat nat_vkfmt,
                 uint32_t WW, uint32_t WH, uint32_t WW_flow, uint32_t WH_flow, uint32_t flow_div,
                 uint32_t UP_W, uint32_t UP_H, bool want_pfg,
                 DevicesInit& o_dev, VDev& FD, HostBridgeInit& o_host, FlowPipesInit& o_flow, ImagesInit& o_img){
    VDev& A=o_dev.A; VDev& B=o_dev.B; VDev& G=o_dev.G;
    auto& use_upscale=o_dev.use_upscale; auto& use_igpu_convert=o_dev.use_igpu_convert;
    auto& bframe_use=o_dev.bframe_use; auto& gsrc_use=o_dev.gsrc_use;
    auto& pres_w=o_host.pres_w; auto& pres_h=o_host.pres_h;
    auto& AframeA=o_flow.AframeA; auto& CinterpA=o_flow.CinterpA;
    auto& dxgi_stage=o_img.dxgi_stage;
    auto& Anative=o_img.Anative; auto& Awork=o_img.Awork; auto& Bframe=o_img.Bframe;
    auto& Cinterp=o_img.Cinterp; auto& Gsrc=o_img.Gsrc; auto& Gdst=o_img.Gdst;
    auto& Bflow=o_img.Bflow; auto& Apresent=o_img.Apresent;
    auto& sbI=o_img.sbI; auto& b_q2_split=o_img.b_q2_split;
    // ── Images ────────────────────────────────────────────────────────────────
    // Astage is now hostA (aligned _aligned_malloc) imported via hbuf_import above.
    // dxgi_stage is still needed for D3D11 staging texture; Astage.buf already created.
    dxgi_stage=d3d_staging(d,NAT_W,NAT_H);
    // Bframe needs STORAGE_BIT for packed unpack output (compute write).
    // Gsrc needs STORAGE_BIT for iGPU real-frame unpack (compute write before upscale).
    // Assigned (not declared-const) here: earlier goto-done error paths would otherwise
    // jump across the initialization — gcc rejects that; the vars live with the top block.
    bframe_use=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT
               |(use_igpu_convert?(VkImageUsageFlags)VK_IMAGE_USAGE_STORAGE_BIT:0u)
               |(cfg.nvofa?(VkImageUsageFlags)VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0u)   // the OFA blits Bframe -> its downscaled input, so Bframe needs TRANSFER_SRC (flow_div==1 doesn't add it)
               // At flow_div>1 the F thread blits Bframe → Bflow (downscale) before the flow, so Bframe
               // must be a valid blit SOURCE. Added ONLY when downscaling → bytes unchanged at N=1.
               |((flow_div>1u)?(VkImageUsageFlags)VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0u);
    gsrc_use  =VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT
               |(use_igpu_convert?(VkImageUsageFlags)VK_IMAGE_USAGE_STORAGE_BIT:0u);
    if(!dxgi_stage||
       !img_create(A,NAT_W,NAT_H,nat_vkfmt,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,Anative)||
       !img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,Awork)||
       // --fps-overlay: add STORAGE + a view ONLY when the overlay is enabled, so the overlay compute
       // pass can RMW Apresent (rgba8). DEFAULT OFF → created as TRANSFER_DST|TRANSFER_SRC, no view →
       // byte-identical-off.
       !img_create(A,pres_w,pres_h,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|(cfg.fps_overlay?(VkImageUsageFlags)VK_IMAGE_USAGE_STORAGE_BIT:0u),Apresent,/*want_view=*/cfg.fps_overlay)||
       !img_create(FD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,bframe_use,Bframe[0])||   // FD-route Bframe/Cinterp/Bflow (B→A under single_gpu) — the flow producer's frame buffers
       !img_create(FD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,bframe_use,Bframe[1])||
       !img_create(FD,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,Cinterp)||
       // The two flow-input downscale targets (WW_flow×WH_flow). SAMPLED (OFP reads them) +
       // TRANSFER_DST (blit dest). Only when downscaling — zero extra VRAM at flow_div==1.
       (flow_div>1u&&!img_create(FD,WW_flow,WH_flow,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,Bflow[0]))||
       (flow_div>1u&&!img_create(FD,WW_flow,WH_flow,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,Bflow[1]))||
       ((use_upscale||use_igpu_convert)&&!img_create(G,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,gsrc_use,Gsrc))||
       (use_upscale&&!img_create(G,UP_W,UP_H,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,Gdst)))
        { std::printf("[ra] image allocation failed\n"); return false; }

    oneshot(FD,[&](VkCommandBuffer c){ for(int i=0;i<2;++i) img_barrier(c,Bframe[i].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT); img_barrier(c,Cinterp.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT); });   // FD-route the layout-seed oneshot (uses FD.q/FD.pool internally)
    // Seed the downscale scratch to SHADER_READ_ONLY_OPTIMAL so the first per-pair blit's
    // RO→TRANSFER_DST barrier (in the F record sites) is a valid transition (matches Bframe's seed layout).
    if(flow_div>1u) oneshot(FD,[&](VkCommandBuffer c){ for(int i=0;i<2;++i) img_barrier(c,Bflow[i].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT); });   // FD-route
    // B-VRAM staging buffers for the overlapped interp copy-outs. Only when B.q2 is a real second
    // queue (split-family/split-queue); shared fallback keeps serial.
    b_q2_split = (B.q2!=B.q);
    if(b_q2_split){
        const VkDeviceSize ibytes=VkDeviceSize(WW)*WH*4u;   // R8G8B8A8 interp frame ≈ 8.3MB
        for(int _k=0;_k<kMaxInterp;++_k){
            if(!dbuf_create(B,ibytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,sbI[_k]))
                { std::printf("[ra] sbI[%d] alloc failed — reverting to serial copy-out\n",_k); b_q2_split=false; break; }
        }
    }
    if(use_upscale) oneshot(G,[&](VkCommandBuffer c){ img_barrier(c,Gdst.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT); });
    // Primary-FG images on A.
    if(want_pfg){
        if(!img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,AframeA[0])||
           !img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,AframeA[1])||
           !img_create(A,WW,WH,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,CinterpA))
            { std::printf("[ra] pfg image allocation failed — primary-FG disabled\n"); }
        else oneshot(A,[&](VkCommandBuffer c){
            for(int i=0;i<2;++i) img_barrier(c,AframeA[i].img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT);
            img_barrier(c,CinterpA.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
        });
    }
    return true;
}
// ── Command buffers + fences + semaphores (E1: moved VERBATIM from main.cpp).
void init_cmd_sync(Config& cfg, bool single_gpu, DevicesInit& o_dev, VDev& FD, ImagesInit& o_img,
                   FlowPipesInit& o_flow, CmdSyncInit& o_cs, BridgeInit& o_br){
    VDev& A=o_dev.A; VDev& B=o_dev.B; VDev& G=o_dev.G;
    auto& use_upscale=o_dev.use_upscale; auto& use_igpu_convert=o_dev.use_igpu_convert;
    auto& use_bidir=o_dev.use_bidir; auto& use_wap=o_dev.use_wap; auto& use_fwd_prestage=o_dev.use_fwd_prestage;
    auto& pfg_enabled=o_flow.pfg_enabled;
    auto& b_q2_split=o_img.b_q2_split;
    auto& cmdA=o_cs.cmdA; auto& cmdB=o_cs.cmdB; auto& cmdG=o_cs.cmdG;
    auto& a_cpool_sg=o_cs.a_cpool_sg; auto& a_fpool_sg=o_cs.a_fpool_sg;
    auto& cmdGP=o_cs.cmdGP; auto& cmdA_fg=o_cs.cmdA_fg; auto& cmdB_bwd=o_cs.cmdB_bwd;
    auto& cmdB_fwd=o_cs.cmdB_fwd; auto& fB_fwd=o_cs.fB_fwd;
    auto& cmdF_pre=o_cs.cmdF_pre; auto& fF_pre=o_cs.fF_pre;
    auto& cmdB2=o_cs.cmdB2; auto& tfb=o_cs.tfb;
    auto& fA=o_cs.fA; auto& fB=o_cs.fB; auto& fG=o_cs.fG; auto& fGP=o_cs.fGP;
    auto& fA_fg=o_cs.fA_fg; auto& fB2=o_cs.fB2;
    auto& cmdBridge=o_br.cmdBridge; auto& fBridge=o_br.fBridge;
    // ── Command buffers + fences + semaphores ─────────────────────────────────
    {
        VkCommandBufferAllocateInfo ci2{}; ci2.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ci2.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ci2.commandBufferCount=1;
        // single-GPU: command pools are EXTERNALLY-SYNCHRONIZED (one thread records at a time). Under
        // single_gpu the 3 threads P(present)/C(convert)/F(flow) would all record from A.pool → pool race
        // + UB. Give C and F their OWN A-side pools (A.qfam = A.q2's family, the q2mode=1 guarantee); P
        // keeps A.pool. The a_q2_mtx still serializes the SHARED A.q2 submits (a queue and a pool are
        // distinct objects). OFF (single_gpu false) → pools stay null → byte-identical.
        if(single_gpu){ VkCommandPoolCreateInfo cpsg{}; cpsg.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; cpsg.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; cpsg.queueFamilyIndex=A.qfam; vkCreateCommandPool(A.dev,&cpsg,nullptr,&a_cpool_sg); vkCreateCommandPool(A.dev,&cpsg,nullptr,&a_fpool_sg); }
        ci2.commandPool=single_gpu?a_cpool_sg:A.pool; vkAllocateCommandBuffers(A.dev,&ci2,&cmdA);
        // FD-route cmdB (B→A under single_gpu). Under single_gpu it submits to A.q2 — same FAMILY as A.q
        // (q2mode=1, the guaranteed split-queue), so FD.pool (=A.pool, family qfam) is the correct pool.
        ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; vkAllocateCommandBuffers(FD.dev,&ci2,&cmdB);   // F's own A-pool under single_gpu (pool-race fix)
        // Second B cmd buffer for the bwd pass (overlap: submit bwd, fit fwd on CPU, wait bwd). B.pool/B.q
        // (same family as cmdB — F is B.q's only submitter, externally synchronized). Allocated only when
        // bidir is live (the sole bwd consumer).
        if(use_bidir){ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; vkAllocateCommandBuffers(FD.dev,&ci2,&cmdB_bwd);}   // FD-route + pool-race fix
        // The fwd-pass ping-pong cmd buffers (opt-in, WAP only). B.pool/B.q (same family as cmdB — F is
        // B.q's only submitter). Allocated only on the opt-in path; the serial path is unchanged.
        if(use_wap&&cfg.fwd_pipeline){ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; for(int _k=0;_k<2;++_k) vkAllocateCommandBuffers(FD.dev,&ci2,&cmdB_fwd[_k]);}   // FD-route + pool-race fix
        // --fwd-prestage: the prestage ping-pong cmd buffers — SAME F flow pool as cmdB (a_fpool_sg under
        // single_gpu, else FD.pool), allocated ONLY when use_fwd_prestage. Idle otherwise (serial path
        // records the copy inline in cmdF) → byte-unchanged.
        if(use_fwd_prestage){ci2.commandPool=single_gpu?a_fpool_sg:FD.pool; for(int _k=0;_k<2;++_k) vkAllocateCommandBuffers(FD.dev,&ci2,&cmdF_pre[_k]);}   // FD-route + pool-race fix (mirrors cmdB_fwd)
        // cmdG (thread C's convert) is recorded from G.pool2 and submits to G.q2 — family-bound to qfam2
        // (=G.pool/G.q in the shared fallback). Separates C from P's G.pool.
        // Only the (forced-off) upscale or the igpu-convert path needs G cmd buffers.
        if(use_upscale||use_igpu_convert){ci2.commandPool=G.pool2; vkAllocateCommandBuffers(G.dev,&ci2,&cmdG);}
        // cmdGP is P-thread's G cmd buffer — used only by the (forced-off) upscale path now.
        if(use_upscale){ci2.commandPool=G.pool; vkAllocateCommandBuffers(G.dev,&ci2,&cmdGP);}
        // F-thread pfg OFP on A (separate pool entry so C's cmdA is uncontested).
        if(pfg_enabled){ci2.commandPool=A.pool; vkAllocateCommandBuffers(A.dev,&ci2,&cmdA_fg);}
        // F's overlapped interp copy-outs run on B.q2 — one cmd buffer per interp slot, recorded from
        // B.pool2 (family-bound to qfam2; the family trap).
        if(b_q2_split) for(int _k=0;_k<kMaxInterp;++_k){ci2.commandPool=B.pool2; vkAllocateCommandBuffers(B.dev,&ci2,&cmdB2[_k]);}
        // P-thread's A-side bridge cmd buffer (blit/warp → bridge_img) — the present path. A.pool (P
        // submits on A.q — the present-source build, single-threaded on P).
        {ci2.commandPool=A.pool; vkAllocateCommandBuffers(A.dev,&ci2,&cmdBridge);}
    }
    {
        VkFenceCreateInfo fi{}; fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(A.dev,&fi,nullptr,&fA);
        vkCreateFence(FD.dev,&fi,nullptr,&fB); if(use_upscale||use_igpu_convert) vkCreateFence(G.dev,&fi,nullptr,&fG);   // FD-route fB (B→A)
        if(use_bidir) vkCreateFence(FD.dev,&fi,nullptr,&fB2);   // bwd-submit fence (FD-route)
        if(use_wap&&cfg.fwd_pipeline) for(int _k=0;_k<2;++_k) vkCreateFence(FD.dev,&fi,nullptr,&fB_fwd[_k]);  // fwd-pipeline ping-pong fences (FD-route)
        if(use_fwd_prestage) for(int _k=0;_k<2;++_k) vkCreateFence(FD.dev,&fi,nullptr,&fF_pre[_k]);  // fwd-prestage ping-pong fences (FD-route, mirrors fB_fwd)
        if(use_upscale) vkCreateFence(G.dev,&fi,nullptr,&fGP);  // upscale-only
        if(pfg_enabled) vkCreateFence(A.dev,&fi,nullptr,&fA_fg);      // F-thread pfg
        if(b_q2_split) for(int _k=0;_k<kMaxInterp;++_k) vkCreateFence(B.dev,&fi,nullptr,&tfb[_k]);  // overlapped interp copy-out fences
        vkCreateFence(A.dev,&fi,nullptr,&fBridge);  // the present-path fence
    }
}

// Made with my soul - Swately <3
