// PhyriadFG present layer.
// Bodies of the present-side upscale factory declared in present/present.hpp; they have
// external linkage here (main calls them).
#include "present/present.hpp"
#include "control/layer_config.hpp"   // R3: FgPush, layer_arm_mask, the contract hash
#include "clock/phase_clock.hpp"   // STAGE 4: the content clock (R1/X14)
#include "present/present_stage.hpp"   // STAGE 6: the present stage (R4/X12)
#include "tdr_hang_spv.hpp"           // kTdrHangSpv (--tdr-test, G-R4)
#include "seam/seam_graph.hpp"     // STAGE 5 output barriers, derived (R2b/X15)
#include "core/compat_reason.hpp"    // ra::compat::emit / ReasonCode (named-reason present-init bail)
#include "core/app_init.hpp"         // wgc_recheck_window_monitor (I-8: the 1 Hz captured-monitor re-check)
#include "flow/flow.hpp"             // MedianPipe (the P-thread medPipe member access)
#include <phyriad/hal/CpuWait.hpp>   // phyriad::hal::cpu_wait_for_ns (paced spin-finish)
#include "core/globals.hpp"          // g_quit / vk_live / g_ov_in/g_ov_out / g_gpu_a_util / g_device_lost (true globals the P body names)
#include "core/telemetry_csv.hpp"    // phyriadfg::TelemetryCsv (the P-thread-local tcsv)
#include "instrument/instrument.hpp" // dump_bmp / dump_rgba (P-thread diagnostic dumps)
#include <phyriad/render/present/PresentSurface.hpp>  // pp::PresentSurface (the present pillar)
#include "overlay_fps_spv.hpp"       // kOverlayFpsSpv (the --fps-overlay compute module)
#include <timeapi.h>                  // timeBeginPeriod/timeEndPeriod (paced sleeps)
#include <pdh.h>                       // per-adapter GPU% perf counter
#include <pdhmsg.h>                    // PDH_MORE_DATA / PDH_CSTATUS_VALID_DATA
#include <dwmapi.h>                    // DwmGetCompositionTimingInfo (--pace-vblank)
#include <d3d11_4.h>                   // ID3D11Multithread / keyed-mutex bridge
#include <dxgi1_2.h>                   // IDXGIKeyedMutex (bridge_km)
#include <vulkan/vulkan_win32.h>       // Win32 external-memory import (bridge_img)
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

bool up_create(VDev& d,Img& src,Img& dst,const std::vector<uint32_t>& spv,UpPipe& p){
    VkSamplerCreateInfo s{}; s.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; s.magFilter=VK_FILTER_LINEAR; s.minFilter=VK_FILTER_LINEAR; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; vkCreateSampler(d.dev,&s,nullptr,&p.samp);
    const VkDescriptorSetLayoutBinding bd[2]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,&p.samp},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=2; dl.pBindings=bd; vkCreateDescriptorSetLayout(d.dev,&dl,nullptr,&p.dsl);
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1; pl.pSetLayouts=&p.dsl; vkCreatePipelineLayout(d.dev,&pl,nullptr,&p.layout);
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data(); VkShaderModule mod; vkCreateShaderModule(d.dev,&mci,nullptr,&mod);
    VkPipelineShaderStageCreateInfo stg{}; stg.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stg.stage=VK_SHADER_STAGE_COMPUTE_BIT; stg.module=mod; stg.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=stg; cp.layout=p.layout; vkCreateComputePipelines(d.dev,VK_NULL_HANDLE,1,&cp,nullptr,&p.pipe); vkDestroyShaderModule(d.dev,mod,nullptr);
    const VkDescriptorPoolSize psz[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}};
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1; pi.poolSizeCount=2; pi.pPoolSizes=psz; vkCreateDescriptorPool(d.dev,&pi,nullptr,&p.pool);
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=p.pool; dai.descriptorSetCount=1; dai.pSetLayouts=&p.dsl; vkAllocateDescriptorSets(d.dev,&dai,&p.set);
    VkDescriptorImageInfo si{}; si.sampler=p.samp; si.imageView=src.view; si.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo di{}; di.imageView=dst.view; di.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    const VkWriteDescriptorSet wds[2]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&si,nullptr,nullptr},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,p.set,1,0,1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,&di,nullptr,nullptr}};
    vkUpdateDescriptorSets(d.dev,2,wds,0,nullptr); return p.pipe!=VK_NULL_HANDLE;
}
void up_destroy(VDev& d,UpPipe& p){ if(p.pool) vkDestroyDescriptorPool(d.dev,p.pool,nullptr); if(p.pipe) vkDestroyPipeline(d.dev,p.pipe,nullptr); if(p.layout) vkDestroyPipelineLayout(d.dev,p.layout,nullptr); if(p.dsl) vkDestroyDescriptorSetLayout(d.dev,p.dsl,nullptr); if(p.samp) vkDestroySampler(d.dev,p.samp,nullptr); p=UpPipe{}; }


// ── Thread P — run_present. The alias preamble below rebinds FgContext's references and
//    array-pointers to short local names the thread body uses.
void run_present(FgContext& ctx){
    namespace pp = phyriad::render::present;
    static constexpr int NS=kGenRing;   // F→P interp generation ring (kGenRing via flow/flow.hpp)
    auto& cfg = ctx.cfg;
    auto& c_seq = ctx.c_seq;
    auto& cap_slots = ctx.cap_slots;
    auto& g_quit_threads = ctx.g_quit_threads;
    auto& wgc_ctx = ctx.wgc_ctx;
    auto& d = ctx.d;
    auto& stat_mapfb = ctx.stat_mapfb;
    auto& stat_mapmiss = ctx.stat_mapmiss;
    auto& lt_copy_us = ctx.lt_copy_us;
    auto& lt_compose_us = ctx.lt_compose_us;
    auto& dd_timeouts = ctx.dd_timeouts;
    auto& dd_arr_delta_us = ctx.dd_arr_delta_us;
    auto& dd_arrived = ctx.dd_arrived;
    auto& dd_lost = ctx.dd_lost;
    auto& dd_acq = ctx.dd_acq;   // ACQUIREs/s = the `acq=` readout (dd_present reads 0 on NVIDIA)
    auto& dd_uniq = ctx.dd_uniq; // unique real frames/s = the `uniq=` readout (matches the game's overlay)
    auto& c_slots = ctx.c_slots;
    auto& total_real = ctx.total_real;
    auto& use_igpu_convert = ctx.use_igpu_convert;
    auto& WW = ctx.WW;
    auto& WH = ctx.WH;
    auto& hR_a = ctx.hR_a;
    auto& A = ctx.A;
    auto& c_conv_us = ctx.c_conv_us;
    auto& cmdG = ctx.cmdG;
    auto& G = ctx.G;
    auto& fG = ctx.fG;
    auto& g_q_mtx = ctx.g_q_mtx;
    auto& hostR = ctx.hostR;
    auto& B = ctx.B;
    auto& WW_flow = ctx.WW_flow;
    auto& flow_div = ctx.flow_div;
    auto& use_inertia = ctx.use_inertia;
    auto& use_objects = ctx.use_objects;
    auto& use_bidir = ctx.use_bidir;
    auto& use_gme = ctx.use_gme;
    auto& use_wap = ctx.use_wap;
    auto& use_ambig = ctx.use_ambig;
    auto& ofp = ctx.ofp;
    auto& hostI = ctx.hostI;
    auto& hostMV = ctx.hostMV;    // --qdump+ (S2.T1): the per-generation MV plane (RG16F, W/8)
    auto& hostSAD = ctx.hostSAD;  // --qdump+ (S2.T1): the per-generation SAD plane (RG16F, W/8)
    auto& hostMVB = ctx.hostMVB;  // --qdump+ (S2.T1c): the BACKWARD MV plane (RG16F, W/8) - binding 5
    auto& hostC2  = ctx.hostC2;   // --qdump+ (S2.T1c): the second-best SAD candidate (RGBA16F) - binding 10
    auto& hostDIS = ctx.hostDIS;  // --qdump+ (S2.T1c): the prev-anchored dissidence mask (R8) - binding 6
    auto& hostDISB= ctx.hostDISB; // --qdump+ (S2.T1c): the cur-anchored dissidence mask (R8) - binding 7
    auto& hostPER = ctx.hostPER;  // --qdump+ (S2.T1c): the inertia persistence field (R8) - binding 8
    auto& hI_a = ctx.hI_a;
    auto& f_pair_cseq_a = ctx.f_pair_cseq_a;
    auto& f_pair_slot_a = ctx.f_pair_slot_a;
    auto& f_pair_tcap_a = ctx.f_pair_tcap_a;
    auto& f_pair_span_a = ctx.f_pair_span_a;
    auto& f_pair_n_a = ctx.f_pair_n_a;
    auto& f_pair_gme_a = ctx.f_pair_gme_a;
    auto& f_pair_gme_valid_a = ctx.f_pair_gme_valid_a;
    auto& f_pair_mfwd_a = ctx.f_pair_mfwd_a;
    auto& f_pair_mbwd_a = ctx.f_pair_mbwd_a;
    auto& f_pair_disp_a = ctx.f_pair_disp_a;
    auto& f_pair_bwd_valid_a = ctx.f_pair_bwd_valid_a;
    auto& f_seq = ctx.f_seq;
    auto& p_presenting = ctx.p_presenting;
    auto& src_interval_us = ctx.src_interval_us;
    auto& present_cost_us = ctx.present_cost_us;
    auto& stat_tier = ctx.stat_tier;
    auto& stat_bwd_skips = ctx.stat_bwd_skips;
    auto& stat_skips = ctx.stat_skips;
    auto& stat_lap = ctx.stat_lap;
    auto& stat_cons = ctx.stat_cons;
    auto& stat_flow_us = ctx.stat_flow_us;
    auto& stat_pair_us = ctx.stat_pair_us;
    auto& gme_fit_us = ctx.gme_fit_us;
    auto& gme_dis_x100 = ctx.gme_dis_x100;
    auto& obj_live = ctx.obj_live;
    auto& obj_rep_x10 = ctx.obj_rep_x10;
    auto& live_n_atomic = ctx.live_n_atomic;
    auto& lt_pickup_us = ctx.lt_pickup_us;
    auto& lt_fwake_us = ctx.lt_fwake_us;
    auto& lt_preflow_us = ctx.lt_preflow_us;
    auto& lt_spin_us = ctx.lt_spin_us;
    auto& Apresent = ctx.Apresent;
    auto& Gdst = ctx.Gdst;
    auto& Gsrc = ctx.Gsrc;
    auto& UP_H = ctx.UP_H;
    auto& UP_W = ctx.UP_W;
    auto& WH_warp = ctx.WH_warp;
    auto& WW_warp = ctx.WW_warp;
    auto& bridge_h = ctx.bridge_h;
    auto& bridge_img1 = ctx.bridge_img1;
    auto& bridge_km_d3d = ctx.bridge_km_d3d;
    auto& bridge_km_d3d1 = ctx.bridge_km_d3d1;
    auto& bridge_mem1 = ctx.bridge_mem1;
    auto& bridge_nt = ctx.bridge_nt;
    auto& bridge_nt1 = ctx.bridge_nt1;
    auto& bridge_tex = ctx.bridge_tex;
    auto& bridge_tex1 = ctx.bridge_tex1;
    auto& bridge_use_km = ctx.bridge_use_km;
    auto& bridge_w = ctx.bridge_w;
    auto& cmdBridge1 = ctx.cmdBridge1;
    auto& cmdBridgeA0 = ctx.cmdBridgeA0;
    auto& cmdGP = ctx.cmdGP;
    auto& cmdUpload = ctx.cmdUpload;
    auto& devMass = ctx.devMass;
    auto& fBridge1 = ctx.fBridge1;
    auto& fBridgeA0 = ctx.fBridgeA0;
    auto& fGP = ctx.fGP;
    auto& fillPipeA = ctx.fillPipeA;
    auto& hC2_a = ctx.hC2_a;
    auto& hCurD_a = ctx.hCurD_a;
    auto& hDISB_a = ctx.hDISB_a;
    auto& hDIS_a = ctx.hDIS_a;
    auto& hFIELD_a = ctx.hFIELD_a;
    auto& hGout = ctx.hGout;
    auto& hMVB_a = ctx.hMVB_a;
    auto& hMV_a = ctx.hMV_a;
    auto& hMass_a = ctx.hMass_a;
    auto& hOutD_a = ctx.hOutD_a;
    auto& hMV1_a = ctx.hMV1_a;     // --qdump+ post-consensus MV readback
    auto& hMVB1_a = ctx.hMVB1_a;   // ... and the backward field
    auto& hPER_a = ctx.hPER_a;
    auto& hPrevD_a = ctx.hPrevD_a;
    auto& hR_g = ctx.hR_g;
    auto& hSAD_a = ctx.hSAD_a;
    auto& hostCurD = ctx.hostCurD;
    auto& hostMassPtr = ctx.hostMassPtr;
    auto& hostOutD = ctx.hostOutD;
    auto& hostMV1 = ctx.hostMV1;   // --qdump+ post-consensus MV readback
    auto& hostMVB1 = ctx.hostMVB1; // ... and the backward field
    auto& hostPrevD = ctx.hostPrevD;
    auto& luidA = ctx.luidA;
    auto& luidA_ok = ctx.luidA_ok;
    auto& luidB = ctx.luidB;
    auto& luidB_ok = ctx.luidB_ok;
    auto& luidG = ctx.luidG;
    auto& luidG_ok = ctx.luidG_ok;
    auto& medPipe = ctx.medPipe;
    auto& nvNameA = ctx.nvNameA;
    auto& nvNameB = ctx.nvNameB;
    auto& pres_h = ctx.pres_h;
    auto& pres_w = ctx.pres_w;
    auto& ra_core_p = ctx.ra_core_p;
    auto& total_frames = ctx.total_frames;
    auto& ugPipe = ctx.ugPipe;
    auto& upPipe = ctx.upPipe;
    auto& use_commit_default = ctx.use_commit_default;
    auto& use_fill_div = ctx.use_fill_div;
    auto& use_matte = ctx.use_matte;
    auto& use_mv_guided = ctx.use_mv_guided;
    auto& use_mv_median = ctx.use_mv_median;
    auto& use_onepos = ctx.use_onepos;
    auto& use_rescue = ctx.use_rescue;
    auto& use_upscale = ctx.use_upscale;
    auto& uslot = ctx.uslot;
    auto& uslot_val = ctx.uslot_val;
    auto& wapC2A = ctx.wapC2A;
    auto& wapCurA = ctx.wapCurA;
    auto& wapDISA = ctx.wapDISA;
    auto& wapDISBA = ctx.wapDISBA;
    auto& wapFIELDA = ctx.wapFIELDA;
    auto& wapMVA = ctx.wapMVA;
    auto& wapMVBA = ctx.wapMVBA;
    auto& wapMVScratchA = ctx.wapMVScratchA;
    auto& wapMVTA = ctx.wapMVTA;
    auto& wapOutA = ctx.wapOutA;
    auto& wapPERA = ctx.wapPERA;
    auto& wapPipeA = ctx.wapPipeA;
    auto& fgPipeA = ctx.fgPipeA;   // R3: the fg_core.comp pipeline
    auto& abPipeA = ctx.abPipeA;   // R3: the --fg-core-ab byte-diff pass
    auto& devAb = ctx.devAb;       // R3: its stats (VRAM) + host copy
    auto& hAb_a = ctx.hAb_a;
    auto& hostAb = ctx.hostAb;
    auto& wapPrevA = ctx.wapPrevA;
    auto& wapPrevOutA = ctx.wapPrevOutA;
    auto& wapSADA = ctx.wapSADA;
    auto& warp_div = ctx.warp_div;
    auto& wgc_target_hwnd = ctx.wgc_target_hwnd;
    auto& xfer_U = ctx.xfer_U;
    auto& xfer_W = ctx.xfer_W;
    auto& xfer_on = ctx.xfer_on;
    auto& bridge_img = ctx.bridge_img;
    auto& bridge_mem = ctx.bridge_mem;
    auto& cmdBridge = ctx.cmdBridge;
    auto& fBridge = ctx.fBridge;
    auto& ra_fgprotect_demote_p = ctx.ra_fgprotect_demote_p;
    auto& lt_fpub_us = ctx.lt_fpub_us;
    // locked G-queue submit helper for the P-thread (upscale + G-present): submit under
    // g_q_mtx (G's shared queue) and block on the fence.
    auto submit_wait_G_P = [&](VkCommandBuffer cb, VkFence f){
        vkResetFences(G.dev,1,&f);
        { std::lock_guard<std::mutex> lk(g_q_mtx);
          VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
          si.commandBufferCount=1; si.pCommandBuffers=&cb;
          vkQueueSubmit(G.q,1,&si,f); }
        vk_wait_live(G.dev,f);
    };
    // ── PRESENT thread body (incl. all nested lambdas) ──
            // --pin-test 5: MMCSS token held thread-local → RAII AvRevert at thread exit.
            // Inactive (no AVRT call) for modes 0-4 / no --pin (byte-identical-off).
            phyriad::hw::MmcssToken mmcss_p;
            // FIX #1: the PRESENT thread is the real-time one — pin to the SPSC consumer core
            // AND elevate to RT priority (gated on cfg.pin_threads; skip UINT32_MAX for the pin).
            // Both [[nodiscard]] bools are consumed; on failure log + continue (do NOT crash).
            if(cfg.pin_threads){
                // ablation (--pin-test) affinity in modes 0/1/3; P TIME_CRITICAL-elevate in modes 0/1/2.
                const bool s0_pin_p   = (cfg.pin_test==0||cfg.pin_test==1||cfg.pin_test==3);
                const bool s0_elev_p  = (cfg.pin_test==0||cfg.pin_test==1||cfg.pin_test==2);
                if(s0_pin_p && ra_core_p!=UINT32_MAX && !phyriad::hw::pin_current_thread(ra_core_p))
                    std::printf("[ra] WARN: present-thread pin to core %u failed (continuing unpinned)\n", ra_core_p);
                if(s0_elev_p && !phyriad::hw::elevate_thread_rt(true))
                    std::printf("[ra] WARN: present-thread RT elevation failed (continuing at normal priority)\n");
                // mode 5 = MMCSS-COMPOSITE: P is the ONE HARD-pinned thread (the present cadence/floor:
                // PIN-FULL's present-floor) + the strongest MMCSS task 'Pro Audio'/CRITICAL, with
                // elevate_thread_rt(true)=TIME_CRITICAL as the fallback when MMCSS is inactive. This
                // composes PIN-FULL's present-floor with PRIO-ONLY's B/latency (the data-refined policy).
                if(cfg.pin_test==5){
                    // THREAD_PROTECTION GAME_FLOOR: under --fg-protect on a small CPU (<= 4 P-cores) the
                    // hard-pin count would breach the game's >= half-the-P-cores reservation -> demote P to a
                    // SOFT ideal-processor hint (migratable, the game can reclaim the core) instead of a hard pin.
                    // Big CPUs (and bare `--pin --pin-test 5`, where ra_fgprotect_demote_p stays false) keep the
                    // hard pin -> byte-identical when off's mode-5.
                    if(cfg.fg_protect && ra_fgprotect_demote_p){
                        if(ra_core_p!=UINT32_MAX
                           && !phyriad::hw::set_thread_ideal_processor(GetCurrentThreadId(), ra_core_p).has_value())
                            std::printf("[ra] WARN: present-thread soft-affinity (ideal proc %u) failed (continuing unhinted)\n", ra_core_p);
                        else
                            std::printf("[ra] --fg-protect GAME_FLOOR: present-thread DEMOTED to soft ideal-processor core %u (small-CPU game-reservation)\n", ra_core_p);
                    } else if(ra_core_p!=UINT32_MAX && !phyriad::hw::pin_current_thread(ra_core_p))
                        std::printf("[ra] WARN: present-thread hard-pin to core %u failed (continuing unpinned)\n", ra_core_p);
                    mmcss_p = phyriad::hw::join_mmcss_task(phyriad::hw::MmcssTask::ProAudio, phyriad::hw::AvrtPriority::Critical);
                    if(mmcss_p.active())
                        std::printf("[ra] MMCSS-composite: P joined 'Pro Audio'/CRITICAL (idx=%lu) + hard-pin core %u\n", mmcss_p.task_index(), ra_core_p);
                    else
                        std::printf("[ra] MMCSS-composite: P 'Pro Audio' join INACTIVE -> fallback elevate_thread_rt(TIME_CRITICAL)%s + hard-pin core %u\n", phyriad::hw::elevate_thread_rt(true)?"":" FAILED", ra_core_p);
                }
            }
            double src_interval_ema_ms=16.67;
            double lat_ema_ms=0.0; bool lat_valid=false;
            double slip_sum=0.0,slip_max=0.0; uint64_t slip_n=0;
            uint64_t last_stat_presents=0; uint64_t last_arrived=0;
            double sum_iter=0,worst=0; double stat_t=now_ms();
            (void)last_arrived;
            // ── per-adapter GPU% via PDH "\GPU Engine(*)\Utilization Percentage" ──
            // One wildcard query, primed once, refreshed at 1Hz. Each engine instance name embeds the
            // adapter as luid_0x{High}_0x{Low}; we sum every engine's utilization into the matching
            // adapter (A/B/G) by string-matching that fragment against the Vulkan deviceLUIDs captured
            // above. The summed-per-adapter total can exceed 100 (overlapping engine queues), which is
            // the normal Task-Manager semantics for these counters. Failure → gpu() segment omitted
            // (the line is otherwise unchanged); the query is closed at P's exit.
            PDH_HQUERY  pdhQuery=nullptr;
            PDH_HCOUNTER pdhGpuEng=nullptr;
            bool pdh_ok=false; double pdh_last_collect=now_ms();
            double gpuA_pct=0.0,gpuB_pct=0.0,gpuG_pct=0.0; bool gpu_have=false;
            char luidStrA[40]={},luidStrB[40]={},luidStrG[40]={};
            if(luidA_ok) std::snprintf(luidStrA,sizeof(luidStrA),"luid_0x%08lX_0x%08lX",(unsigned long)luidA.HighPart,(unsigned long)luidA.LowPart);
            if(luidB_ok) std::snprintf(luidStrB,sizeof(luidStrB),"luid_0x%08lX_0x%08lX",(unsigned long)luidB.HighPart,(unsigned long)luidB.LowPart);
            if(luidG_ok) std::snprintf(luidStrG,sizeof(luidStrG),"luid_0x%08lX_0x%08lX",(unsigned long)luidG.HighPart,(unsigned long)luidG.LowPart);
            // PDH match is case-insensitive vs the counter instance casing, so lowercase the needles.
            for(char* s:{luidStrA,luidStrB,luidStrG}) for(char* p=s;*p;++p) *p=(char)std::tolower((unsigned char)*p);
            if(PdhOpenQueryW(nullptr,0,&pdhQuery)==ERROR_SUCCESS){
                if(PdhAddEnglishCounterW(pdhQuery,L"\\GPU Engine(*)\\Utilization Percentage",0,&pdhGpuEng)==ERROR_SUCCESS
                   && PdhCollectQueryData(pdhQuery)==ERROR_SUCCESS){
                    pdh_ok=true;   // primed; the first formatted read happens after the 1s interval below
                    std::printf("[ra] gpu%%: PDH GPU-Engine counter armed (A=%s B=%s G=%s)\n",
                        luidA_ok?luidStrA:"n/a",luidB_ok?luidStrB:"n/a",luidG_ok?luidStrG:"n/a");
                } else { std::printf("[ra] gpu%%: PDH GPU-Engine counter unavailable — gpu() segment off\n"); PdhCloseQuery(pdhQuery); pdhQuery=nullptr; }
            } else { std::printf("[ra] gpu%%: PdhOpenQuery failed — gpu() segment off\n"); }

            // ── NVML — TRUE NVIDIA per-GPU utilization for A and B ──────────────────────────────
            // The PDH "\GPU Engine(*)" instance LUID space does NOT equal the Vulkan/DXGI deviceLUID on this
            // rig (measured: PDH reports luids {0x102e8,0x12001,0x12030,0x12e98} while Vulkan deviceLUIDs are
            // {0x1038d,0x1229f,0x13051} — no overlap), so the LUID string-match above reads ~0 for A/B (and is
            // unreliable in general). NVML is the very source nvidia-smi reads, so A (primary) and B (assist
            // discrete) get their real utilization from NVML, matched BY DEVICE NAME. G (the AMD iGPU) is not
            // an NVIDIA device → it stays on the PDH path. Dynamically loaded (no link dep); absent nvml.dll →
            // silent fallback to the PDH values. nvml.dll ships with the NVIDIA driver (System32 or the NVSMI dir).
            struct NvmlUtil { unsigned int gpu, mem; };
            typedef int (*PFN_nvmlInit)();
            typedef int (*PFN_nvmlShutdown)();
            typedef int (*PFN_nvmlCount)(unsigned int*);
            typedef int (*PFN_nvmlByIdx)(unsigned int, void**);
            typedef int (*PFN_nvmlName)(void*, char*, unsigned int);
            typedef int (*PFN_nvmlUtil)(void*, NvmlUtil*);
            HMODULE nvmlLib=LoadLibraryW(L"nvml.dll");
            if(!nvmlLib) nvmlLib=LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
            PFN_nvmlUtil nvmlUtilFn=nullptr; PFN_nvmlShutdown nvmlShutFn=nullptr;
            void* nvmlDevA=nullptr; void* nvmlDevB=nullptr; bool nvml_ok=false;
            if(nvmlLib){
                auto fInit =(PFN_nvmlInit) GetProcAddress(nvmlLib,"nvmlInit_v2");
                auto fCount=(PFN_nvmlCount)GetProcAddress(nvmlLib,"nvmlDeviceGetCount_v2");
                auto fByIdx=(PFN_nvmlByIdx)GetProcAddress(nvmlLib,"nvmlDeviceGetHandleByIndex_v2");
                auto fName =(PFN_nvmlName) GetProcAddress(nvmlLib,"nvmlDeviceGetName");
                nvmlUtilFn =(PFN_nvmlUtil) GetProcAddress(nvmlLib,"nvmlDeviceGetUtilizationRates");
                nvmlShutFn =(PFN_nvmlShutdown)GetProcAddress(nvmlLib,"nvmlShutdown");
                if(fInit&&fCount&&fByIdx&&fName&&nvmlUtilFn&&fInit()==0){
                    std::string la=nvNameA,lb=nvNameB;
                    for(auto&c:la)c=(char)std::tolower((unsigned char)c);
                    for(auto&c:lb)c=(char)std::tolower((unsigned char)c);
                    unsigned int nN=0; fCount(&nN);
                    for(unsigned int i=0;i<nN;++i){
                        void* h=nullptr; if(fByIdx(i,&h)!=0||!h) continue;
                        char nm[96]={}; if(fName(h,nm,(unsigned int)sizeof(nm))!=0) continue;
                        std::string ln=nm; for(auto&c:ln)c=(char)std::tolower((unsigned char)c);
                        if(!nvmlDevA && !la.empty() && (la.find(ln)!=std::string::npos||ln.find(la)!=std::string::npos)) nvmlDevA=h;
                        else if(!nvmlDevB && !lb.empty() && (lb.find(ln)!=std::string::npos||ln.find(lb)!=std::string::npos)) nvmlDevB=h;
                    }
                    nvml_ok=(nvmlDevA||nvmlDevB);
                    if(nvml_ok) std::printf("[ra] gpu%%: NVML armed (A=%s B=%s) — NVIDIA util via NVML (matches nvidia-smi); G via PDH\n",
                                            nvmlDevA?"ok":"miss", nvmlDevB?"ok":"miss");
                    else { if(nvmlShutFn) nvmlShutFn(); FreeLibrary(nvmlLib); nvmlLib=nullptr; }
                } else { FreeLibrary(nvmlLib); nvmlLib=nullptr; }
            }

            // ── comprehensive per-present telemetry CSV export (--csv) ────────────
            // Producer = this present thread (push a POD row per tick, lock-free); consumer = the
            // component's own low-priority drain thread (NVML/PDH + CSV I/O off the hot path).
            phyriadfg::TelemetryCsv tcsv;
            if(!cfg.csv_path.empty()){
                unsigned long tgt_pid=0; if(wgc_target_hwnd) GetWindowThreadProcessId(wgc_target_hwnd,&tgt_pid);
                // The CSV's Application column: a pid-bound run has no title argument, so name it by
                // the identity that was actually used rather than mislabelling the row "monitor".
                const std::string appname = cfg.window_substr[0] ? std::string(cfg.window_substr)
                    : cfg.window_pid  ? ("pid:"  + std::to_string(cfg.window_pid))
                    : cfg.window_hwnd ? ("hwnd:" + std::to_string(cfg.window_hwnd))
                    : std::string("monitor");
                tcsv.start(cfg.csv_path, appname, (unsigned int)tgt_pid, nvNameA, nvNameB);
            }
            // per-tick row counter + forward-fill of the per-SECOND rates (computed at the
            // 1Hz stat boundary, held here so every per-present row carries the latest known value).
            uint64_t csv_fi=0, csv_last_frz=0;
            double csv_src=-1.0, csv_cons=-1.0, csv_uniq=-1.0, csv_lat=-1.0, csv_slip=-1.0, csv_dis=-1.0;
            // ── (fluidity PACING axis — measurement-only) the prev DISPLAY-FLIP sample for
            // MsBetweenDisplayChange. The own-window flip swapchain's GetFrameStatistics gives the
            // SyncQPCTime (display-flip instant) + PresentCount; differencing SyncQPCTime across
            // presents where PresentCount advanced yields the binding display-time pacing interval
            // (NOT the present-CALL time --pace-hard controls). P-thread-owned (the present thread is
            // the sole reader/writer). The GetFrameStatistics READ is gated under if(tcsv.active())
            // at every push site below → never runs without --csv → byte-identical-off. csv_flip_qfreq
            // = QPC ticks/sec, queried once (cold setup; inert one-time local init, like csv_fi above).
            uint64_t csv_prev_flip_qpc=0; uint32_t csv_prev_flip_count=0; bool csv_have_flip=false;
            double csv_flip_qfreq=0.0; { LARGE_INTEGER f{}; if(QueryPerformanceFrequency(&f)) csv_flip_qfreq=(double)f.QuadPart; }

            // ── create the PresentSurface ON THIS THREAD ──────
            // The pillar's hard threading contract: create() and submit() on the SAME thread (the
            // window message queue belongs to the creating thread; the ghosting-pump inside submit()
            // drains nothing from any other thread). So the surface is a P-local, created here in P's
            // setup. Defaults = DcompCt + ExcludeFromCapture + Immediate (the measured trilemma fix).
            // ── STAGE 6 (PRESENT) — R4/X12: the stage object, created ON THIS THREAD (the surface's threading contract).
            //    The legacy locals below are ALIASES to its fields, so every read site (stats, CSV, --rfp-fresh,
            //    --motion-fallback) stays byte-identical; the bodies moved to src/present/present_stage.cpp.
            pfg::present::PresentStage pres(A,cfg,bridge_w,bridge_h,bridge_use_km,bridge_nt,hostMassPtr,total_frames,g_quit_threads);
            pp::PresentSurface& ra_surface=pres.surface;
            bool& surface_ready=pres.surface_ready;
            uint64_t& ps_ok=pres.ps_ok; uint64_t& ps_timeout=pres.ps_timeout; uint64_t& ps_err=pres.ps_err; uint64_t last_ps_ok=0;  // submit ok/timeout/err counters (stage-owned; the stats window the deltas)
            uint64_t& rdrop_ticks=pres.rdrop_ticks; uint64_t last_rdrop=0;   // async re-present drops (stage-owned; counted by PresentStage::begin)
            uint64_t& fresh_ticks=pres.fresh_ticks; uint64_t last_fresh=0;   // R4b: presents that carried a NEW warp (stage-owned; the stats window the delta)
            // (--load-governor, self-keyed floor) the FG's OWN-slice distress latch: TRUE when freshage or
            // the warp fence says WE are starved (not merely "the GPU is busy"). Declared BEFORE the warp
            // lambda so warp_light reads the SAME latched decision the F-thread floor uses (one decision,
            // two arms). Updated once/util-refresh in the publish block below; false unless load_governor.
            bool gov_self_distress=false;   // debounced FG-own-slice distress (freshage OR warp inflation)
            if(!pres.init((void*)wgc_target_hwnd)) return;   // R4 (stage 6): the PresentSurface block, moved verbatim to PresentStage::init (the two bails return false)
            if(cfg.warp_timing && !pres.timing_arm()) std::printf("[ra] --warp-timing: timestamps unavailable on this device -- running without\n");
            if(cfg.tdr_test_s>0){ const std::vector<uint32_t> spvt(kTdrHangSpv.begin(),kTdrHangSpv.end());
                if(!pres.tdr_arm(spvt,cfg.tdr_test_s)) std::printf("[ra] --tdr-test: arming FAILED (pipeline/buffer) -- the run proceeds without it\n"); }
            // device-loss bridge: the OwnWindow present site now OWNS the displayed
            // DXGI present, so a terminal device loss (DEVICE_REMOVED/RESET) surfaces from submit() as
            // ErrorCode::ShuttingDown (the pillar's dxgi_live twin of vk_live). Map it to the SAME terminal
            // exit the Vulkan-side device loss drives — g_device_lost + g_quit → the proven Ctrl-C unwind →
            // teardown (guarded `&& !g_device_lost`) → exit. Exit IS passthrough (the game keeps rendering
            // behind us). Centralized so all three present sites share one accounting + one exit path.
            auto ps_account=[&](const std::expected<void,phyriad::Error>& r){ pres.account(r); };   // R4 (stage 6): the body moved to PresentStage::account
            // --pace-hard: the HARD present-target pacer state, hoisted HERE so the
            // bridge_present() lambda (the present chokepoint, just below) captures it by reference. ph_tgt = the current
            // tick's grid target (set at the tick boundary when --pace-hard, consumed + zeroed by the pin in
            // bridge_present so a double-call cannot re-pin). ph_w_ema = EMA of the variable per-tick work-time W
            // (=now_ms()−ph_tgt at present time) → the present fires at ph_tgt + EMA(W) + margin (fixed phase vs the
            // grid → present-MASD→0 when W<=budget). ph_held / ph_overshoot = real-pin / degrade counters (DIAG ph).
            // ALL inert with the flag off: ph_tgt is never set → the pin branch in bridge_present is never entered →
            // byte-identical. Thread-P-local (both lambdas + the tick loop run on thread P only).
            double&   ph_tgt=pres.ph_tgt;   // R4: the --pace-hard pin state is stage-owned (the loop sets ph_tgt per tick; ph_w_ema lives in the stage)
            uint64_t& ph_held=pres.ph_held; uint64_t last_ph_held=0; uint64_t& ph_overshoot=pres.ph_overshoot; uint64_t last_ph_overshoot=0;
            // hand the freshly-blitted/warped bridge image to the pillar. The keyed-mutex
            // path passes the key (the pillar AcquireSync→CopyResource→ReleaseSync); the no-KM path
            // relies on the inline fBridge wait already done by the caller (CPU-fence ordering, same
            // thread). submit() drains the window pump + CopyResource + Present(0). Timeout = a
            // skipped present (counted), never a block.
            auto bridge_present=[&](){ pres.present_front(); };   // R4 (stage 6): the --pace-hard pin + the slot-0 submit moved to PresentStage::present_front

            // precision pacing: a bare sleep_until carries Windows' scheduler quantum
            // (1.4-15.6ms) into every present, which shows up as slip. Instead, coarse-sleep
            // to tgt−2ms (timeBeginPeriod(1) makes that honest), then hal::cpu_wait_for_ns
            // spin-finishes (TPAUSE on Intel, bounded PAUSE on AMD). Cost: ≤2ms of one
            // dedicated core per present — P's core is already ours (performance over power).
            auto paced_wait_P=[&](double tgt){
                constexpr double kSpinMs=2.0;
                const double tn=now_ms();
                if(tgt>tn+kSpinMs+0.5) std::this_thread::sleep_until(
                    std::chrono::steady_clock::now()+std::chrono::duration<double,std::milli>(tgt-tn-kSpinMs));
                const double rem=tgt-now_ms();
                if(rem>0.0) phyriad::hal::cpu_wait_for_ns((uint64_t)(rem*1e6));
                const double slip=now_ms()-tgt;
                if(slip>0){slip_sum+=slip;if(slip>slip_max)slip_max=slip;} ++slip_n;
            };

            // P upscale: uses cmdGP/fGP (C uses cmdG/fG for igpu_convert — separate).
            auto do_upscale_P=[&](const HBuf& src){
                if(!use_upscale) return;
                vkResetCommandBuffer(cmdGP,0);
                VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdGP,&bi);
                img_barrier(cmdGP,Gsrc.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyBufferToImage(cmdGP,src.buf,Gsrc.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                img_barrier(cmdGP,Gsrc.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                vkCmdBindPipeline(cmdGP,VK_PIPELINE_BIND_POINT_COMPUTE,upPipe.pipe);
                vkCmdBindDescriptorSets(cmdGP,VK_PIPELINE_BIND_POINT_COMPUTE,upPipe.layout,0,1,&upPipe.set,0,nullptr);
                vkCmdDispatch(cmdGP,(UP_W+7)/8,(UP_H+7)/8,1);
                img_barrier(cmdGP,Gdst.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                { VkBufferImageCopy cp=full_bic(UP_W,UP_H); vkCmdCopyImageToBuffer(cmdGP,Gdst.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hGout.buf,1,&cp); }
                img_barrier(cmdGP,Gdst.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                vkEndCommandBuffer(cmdGP); submit_wait_G_P(cmdGP,fGP);
            };

            // P real-frame upscale: igpu_convert path unpacks hRP_g[s] → Gsrc then upscales.
            auto do_upscale_real_P=[&](int s){
                if(!use_upscale) return;
                if(!use_igpu_convert){ do_upscale_P(hR_g[s]); return; }
                vkResetCommandBuffer(cmdGP,0);
                VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdGP,&bi);
                img_barrier(cmdGP,Gsrc.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                vkCmdBindPipeline(cmdGP,VK_PIPELINE_BIND_POINT_COMPUTE,ugPipe[s].pipe);
                vkCmdBindDescriptorSets(cmdGP,VK_PIPELINE_BIND_POINT_COMPUTE,ugPipe[s].layout,0,1,&ugPipe[s].sets[0],0,nullptr);
                struct{uint32_t W;uint32_t H;}pcug{WW,WH};
                vkCmdPushConstants(cmdGP,ugPipe[s].layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcug),&pcug);
                vkCmdDispatch(cmdGP,(WW+7)/8,(WH+7)/8,1);
                img_barrier(cmdGP,Gsrc.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                vkCmdBindPipeline(cmdGP,VK_PIPELINE_BIND_POINT_COMPUTE,upPipe.pipe);
                vkCmdBindDescriptorSets(cmdGP,VK_PIPELINE_BIND_POINT_COMPUTE,upPipe.layout,0,1,&upPipe.set,0,nullptr);
                vkCmdDispatch(cmdGP,(UP_W+7)/8,(UP_H+7)/8,1);
                img_barrier(cmdGP,Gdst.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                { VkBufferImageCopy cp=full_bic(UP_W,UP_H); vkCmdCopyImageToBuffer(cmdGP,Gdst.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hGout.buf,1,&cp); }
                img_barrier(cmdGP,Gdst.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                vkEndCommandBuffer(cmdGP); submit_wait_G_P(cmdGP,fGP);
            };

            // ── A blits src_work → bridge image, then surface.submit() ───────────
            // src_work (WW×WH host RGBA8) → Apresent (RGBA8) → blit → bridge_img (BGRA8, monitor
            // extent; the blit does the rgba8→bgra8 channel reinterpretation + scale).
            // Keyed-mutex acquire(0)/release(0) chained on THIS submit so the pillar's AcquireSync(0)
            // sees the texture released; no-KM path relies on the inline fBridge wait (CPU ordering).
            // UX: a small teal corner mark shows that PhyriadFG is active and where.
            // Host-imported buffer of BGRA8 badge pixels, vkCmdCopyBufferToImage'd into bridge_img's
            // top-left each present (after the content blit, with a WAW barrier). Default-on (--no-indicator
            // disables). Reclaimed at process exit — the FG is force-killed in practice; a graceful exit
            // leaves a benign buffer the OS reclaims (acceptable for 0.1.0-experimental).
            HBuf badge_buf{}; void* badge_ptr=nullptr;
            constexpr uint32_t badge_dim=28, badge_margin=16;
            if(cfg.indicator){
                const size_t bb=(size_t)badge_dim*badge_dim*4;
                badge_ptr=_aligned_malloc(bb,64);
                if(badge_ptr){
                    uint8_t* bpx=(uint8_t*)badge_ptr;           // BGRA8
                    const uint8_t bg[4]={0x17,0x11,0x0d,0xff};  // #0D1117 dark
                    const uint8_t tl[4]={0xbf,0xd4,0x2d,0xff};  // #2DD4BF teal
                    const float ctr=(badge_dim-1)*0.5f, rad=badge_dim*0.34f;
                    for(uint32_t y=0;y<badge_dim;++y) for(uint32_t x=0;x<badge_dim;++x){
                        float dx=(float)x-ctr, dy=(float)y-ctr; if(dx<0)dx=-dx; if(dy<0)dy=-dy;
                        const bool border=(x<2||y<2||x>=badge_dim-2||y>=badge_dim-2);
                        const uint8_t* col=(border||(dx+dy)<=rad)?tl:bg;
                        uint8_t* p=bpx+((size_t)y*badge_dim+x)*4; p[0]=col[0];p[1]=col[1];p[2]=col[2];p[3]=col[3];
                    }
                    if(!hbuf_import(A,badge_ptr,bb,badge_buf,VK_BUFFER_USAGE_TRANSFER_SRC_BIT)){ _aligned_free(badge_ptr); badge_ptr=nullptr; }
                }
                std::printf(badge_buf.buf?"[ra] indicator: PhyriadFG-active corner mark ON (top-left; --no-indicator to disable)\n"
                                         :"[ra] --indicator: badge unavailable (no host-ptr import on A) -- running without the on-screen mark\n");
            }
            // ── FPS-OVERLAY (--fps-overlay) the LSFG-style "in->out" fps overlay pipeline, built ONCE on the
            // PRESENT thread (Apresent.view is finalised at init, before this thread runs). Binding 0 = Apresent
            // STORAGE (rgba8, GENERAL) + a 16-byte push (OverlayFpsPC). Mirrors apps/minimal_fg's overlay setup.
            // Gated on cfg.fps_overlay → when OFF, NOTHING is created (Apresent has no view/STORAGE either) and
            // the dispatch below is never recorded → byte-identical-off. ov_handles are P-thread locals,
            // destroyed at the present-loop exit (below). The crash-class WAP warp path is NOT touched.
            VkDescriptorSetLayout ov_dsl=VK_NULL_HANDLE; VkPipelineLayout ov_pl_layout=VK_NULL_HANDLE;
            VkPipeline ov_pipeline=VK_NULL_HANDLE; VkDescriptorPool ov_pool=VK_NULL_HANDLE; VkDescriptorSet ov_set=VK_NULL_HANDLE;
            // (re-home, gap identificado por Oleksi/chlmateus) segundo set apuntando a wapOutA para que el
            // overlay TAMBIÉN dibuje en el path WAP/async default (antes: solo el path grid síncrono → verlo
            // exigía degradar el producto entero con --no-warp-at-presenter). wapOutA es storage por
            // construcción (el warp le hace imageStore) — el patrón seguro es el de --afill (compute in-place
            // sobre wapOutA); el DEVICE_LOST histórico era el stamp sobre la imagen bridge COMPARTIDA, no esto.
            VkDescriptorSet ov_set_wap=VK_NULL_HANDLE;
            bool ov_ready=false, ov_ready_wap=false;
            if(cfg.fps_overlay && Apresent.view){
                bool ov_ok=true; VkShaderModule ov_mod=VK_NULL_HANDLE;
                { VkShaderModuleCreateInfo smci{}; smci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                  smci.codeSize=kOverlayFpsSpv.size()*sizeof(uint32_t); smci.pCode=kOverlayFpsSpv.data();
                  ov_ok=(vkCreateShaderModule(A.dev,&smci,nullptr,&ov_mod)==VK_SUCCESS); }
                const VkDescriptorSetLayoutBinding ov_b0{ 0u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
                if(ov_ok){ VkDescriptorSetLayoutCreateInfo d{}; d.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    d.bindingCount=1u; d.pBindings=&ov_b0; ov_ok=(vkCreateDescriptorSetLayout(A.dev,&d,nullptr,&ov_dsl)==VK_SUCCESS); }
                const VkPushConstantRange ov_pcr{ VK_SHADER_STAGE_COMPUTE_BIT, 0u, (uint32_t)sizeof(OverlayFpsPC) };
                if(ov_ok){ VkPipelineLayoutCreateInfo p{}; p.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                    p.setLayoutCount=1u; p.pSetLayouts=&ov_dsl; p.pushConstantRangeCount=1u; p.pPushConstantRanges=&ov_pcr;
                    ov_ok=(vkCreatePipelineLayout(A.dev,&p,nullptr,&ov_pl_layout)==VK_SUCCESS); }
                if(ov_ok){ VkPipelineShaderStageCreateInfo st{}; st.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    st.stage=VK_SHADER_STAGE_COMPUTE_BIT; st.module=ov_mod; st.pName="main";
                    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=st; cp.layout=ov_pl_layout;
                    ov_ok=(vkCreateComputePipelines(A.dev,VK_NULL_HANDLE,1u,&cp,nullptr,&ov_pipeline)==VK_SUCCESS); }
                if(ov_mod) vkDestroyShaderModule(A.dev,ov_mod,nullptr);
                if(ov_ok){ const VkDescriptorPoolSize ov_sz{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2u };
                    VkDescriptorPoolCreateInfo dp{}; dp.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                    dp.maxSets=2u; dp.poolSizeCount=1u; dp.pPoolSizes=&ov_sz; ov_ok=(vkCreateDescriptorPool(A.dev,&dp,nullptr,&ov_pool)==VK_SUCCESS); }
                if(ov_ok){ VkDescriptorSetAllocateInfo da{}; da.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    da.descriptorPool=ov_pool; da.descriptorSetCount=1u; da.pSetLayouts=&ov_dsl; ov_ok=(vkAllocateDescriptorSets(A.dev,&da,&ov_set)==VK_SUCCESS); }
                if(ov_ok){ VkDescriptorImageInfo ii{}; ii.imageView=Apresent.view; ii.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
                    VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ov_set, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ii, nullptr, nullptr };
                    vkUpdateDescriptorSets(A.dev,1u,&w,0u,nullptr); }
                ov_ready=ov_ok;
                // (re-home) el set del path WAP → wapOutA (mismo layout/pipeline, otra imagen destino).
                // Guardado en su propio bool: si wapOutA no existe (WAP off) el overlay WAP queda inerte
                // y el path grid conserva su set de siempre — cada path dibuja sobre SU imagen.
                if(ov_ok && wapOutA.view){
                    VkDescriptorSetAllocateInfo da{}; da.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    da.descriptorPool=ov_pool; da.descriptorSetCount=1u; da.pSetLayouts=&ov_dsl;
                    if(vkAllocateDescriptorSets(A.dev,&da,&ov_set_wap)==VK_SUCCESS){
                        VkDescriptorImageInfo ii{}; ii.imageView=wapOutA.view; ii.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
                        VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ov_set_wap, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ii, nullptr, nullptr };
                        vkUpdateDescriptorSets(A.dev,1u,&w,0u,nullptr);
                        ov_ready_wap=true;
                    }
                }
                std::printf("[ra] --fps-overlay: %s (rect %ux%u @top-left; baked 5x7 font; paths: sync=%s wap/async=%s)\n",
                            (ov_ready||ov_ready_wap)?"ready":"FAILED -- presenting without the overlay",
                            (unsigned)kOverlayW,(unsigned)kOverlayH,
                            ov_ready?"yes":"no", ov_ready_wap?"yes":"no");
            }
            auto bridge_present_src=[&](const HBuf& src_work){
                if(!surface_ready) return;
                vkResetCommandBuffer(cmdBridge,0);
                VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdBridge,&bi);
                img_barrier(cmdBridge,Apresent.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                { VkBufferImageCopy cp=full_bic(pres_w,pres_h); vkCmdCopyBufferToImage(cmdBridge,src_work.buf,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                // FPS-OVERLAY (--fps-overlay) RMW the "in->out" overlay onto Apresent (RGBA8 == the shader's rgba8
                // qualifier) BETWEEN the copy-in and the blit-out. Apresent TRANSFER_DST -> GENERAL, dispatch over the
                // overlay rect (the shader bounds-guards every pixel), then GENERAL -> TRANSFER_SRC for the blit. The
                // overlay SCALES with the blit just like minimal_fg. When OFF, the ELSE arm records the EXACT today
                // single TRANSFER_DST->TRANSFER_SRC barrier → byte-identical-off. This is the SAFE present path; the
                // crash-class WAP warp dispatch is NOT touched.
                if(cfg.fps_overlay && ov_ready){
                    img_barrier(cmdBridge,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
                    vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,ov_pipeline);
                    vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,ov_pl_layout,0,1,&ov_set,0,nullptr);
                    OverlayFpsPC ovpc{ g_ov_in.load(), g_ov_out.load(), pres_w, pres_h };
                    vkCmdPushConstants(cmdBridge,ov_pl_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,(uint32_t)sizeof(ovpc),&ovpc);
                    vkCmdDispatch(cmdBridge,(kOverlayW+7u)/8u,(kOverlayH+7u)/8u,1u);
                    img_barrier(cmdBridge,Apresent.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                } else {
                    img_barrier(cmdBridge,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                }
                img_barrier(cmdBridge,bridge_img.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                { VkImageBlit bl{}; bl.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; bl.dstSubresource=bl.srcSubresource; bl.srcOffsets[1]={(int)pres_w,(int)pres_h,1}; bl.dstOffsets[1]={(int)bridge_w,(int)bridge_h,1}; vkCmdBlitImage(cmdBridge,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,bridge_img.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bl,VK_FILTER_LINEAR); }
                // the badge is a BGRA8 host buffer stamped via vkCmdCopyBufferToImage — a
                // raw byte COPY (not a converting blit), so it is only valid into a BGRA8 bridge. Skip it when
                // the bridge is FP16 (a BGRA8 copy into an R16G16B16A16_FLOAT image would corrupt the texels).
                // The badge is a default-off diagnostic; this drops only the on-screen mark under --present-fp16.
                if(cfg.indicator&&badge_buf.buf&&cfg.present_format!=1){ img_barrier(cmdBridge,bridge_img.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT); VkBufferImageCopy ic{}; ic.bufferRowLength=badge_dim; ic.bufferImageHeight=badge_dim; ic.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; ic.imageOffset={(int)badge_margin,(int)badge_margin,0}; ic.imageExtent={badge_dim,badge_dim,1}; vkCmdCopyBufferToImage(cmdBridge,badge_buf.buf,bridge_img.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&ic); }   // UX: stamp the corner mark on top of the blit (WAW barrier so it lands over the content)
                // bridge_img returns to UNDEFINED next frame (re-acquired). Leave it TRANSFER_DST.
                vkEndCommandBuffer(cmdBridge); vkResetFences(A.dev,1,&fBridge);
                VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmdBridge;
                const uint64_t key0=0; const uint32_t kt=8;
                VkWin32KeyedMutexAcquireReleaseInfoKHR km{}; km.sType=VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR;
                if(bridge_use_km){ km.acquireCount=1; km.pAcquireSyncs=&bridge_mem; km.pAcquireKeys=&key0; km.pAcquireTimeouts=&kt;
                                   km.releaseCount=1; km.pReleaseSyncs=&bridge_mem; km.pReleaseKeys=&key0; si.pNext=&km; }
                pres.submit_sync(fBridge,si);   // R4 (stage 6): the shared cmd/fence submit + the TDR-catching wait
                pres.present_front();   // pillar: AcquireSync(0) → CopyResource → ReleaseSync(0) → Present(0)
            };

            // the present path — A blits src_work → the bridge image → surface.submit().
            auto do_present_P=[&](const HBuf& src_work){ bridge_present_src(src_work); };

            // ── warp-at-presenter helpers (G) ──────────────────────
            // Per pair-advance: upload the two pair reals + MV + SAD from the host bridges into
            // G's sampled images (one G submit). hR_g[*] reads are native sysRAM on the iGPU
            // (≈free, 73 GB/s); MV/SAD are ~130KB. All under g_q_mtx (G's shared queue).
            // the P-side upload grid — the per-pair vkCmdCopyBufferToImage reads the host bridges
            // (sized at the pre-init site off WW_flow) into the WAP sampled MV/SAD images (sized off
            // ofp.motion_width() at the post-init site). All three MUST agree; ofp.motion_width()/height()
            // is the single source of truth (valid here — after init). At flow_div==1 == (WW+7)/8.
            const uint32_t wap_mvw=ofp.motion_width(), wap_mvh=ofp.motion_height();
            // --qdump+ (S2.T1): the generation whose MV/SAD/gme the CURRENT upload published, recorded at
            // the wap_upload() call site and NEVER recomputed — the sidecars must describe exactly the
            // fields the warp read. -1 until the first upload. Declared here (before the warp lambda) so
            // the dump block inside it can capture it; inert unless --qdump is on.
            int qd_gen=-1;
            int qd_tgen=-1;   // --qdump+ (S2.T1c): the u_mv_target generation of the same upload
            // --qdump+ (S2.T6 diagnosis): a COPY of hostMV[gen] taken at the wap_upload call, i.e. the bytes
            // that upload handed the GPU. The ordinary mv= plane is read LATE in the tick from the same host
            // buffer, which F mutates in place; differencing the two settles whether the record's MV is the MV
            // the shader sampled. Allocated once, only under --qdump.
            std::vector<unsigned char> qd_mv0; bool qd_mv0_ok=false;
            // --qdump+ (S2.T6 residual, 2026-09-04): the same snapshot for the TARGET plane. P warps generation
            // g with target g-1; F produces g+1 then g+2, and with kGenRing=3 slot g+2 IS slot g-1 -- so the
            // target plane sits in the one slot F is about to overwrite while P still holds the pair. The mv=
            // plane was proven faithful by exactly this comparison; mvt= never was, and the argument that made
            // mv safe does not cover it.
            std::vector<unsigned char> qd_mvt0; bool qd_mvt0_ok=false;
            if(cfg.qdump_n>0) qd_mvt0.resize((size_t)wap_mvw*wap_mvh*4u);
            if(cfg.qdump_n>0) qd_mv0.resize((size_t)wap_mvw*wap_mvh*4u);
            // WAP runs on A (the bridge owner): the per-pair upload reads the A-side host bridges
            // (hR_a/hMV_a/hSAD_a) into A's WAP sampled images, and the upload, the warp dispatch, and
            // the bridge blit ALL run on A.q. cmdBridge is A.pool/qfam-bound, so a same-family A.q2
            // (split-queue) would be valid but a split-FAMILY A.q2 is the family trap (a qfam cmd
            // buffer can't submit to qfam2) AND would need an image ownership transfer for the WAP
            // images — not worth it for a ~30/s upload. A.q only.
            auto wap_upload=[&](int prev_slot,int cur_slot,int gen,int target_gen){
                // --upload-xfer: when xfer_on, record into a PING-PONG cmdUpload[] buffer and
                // submit to A.qT (the transfer/async-compute engine) with NO host wait — the copies overlap the
                // graphics engine. The body below is textually UNCHANGED: `cmdBridge` is shadowed by `ucmd` (the
                // chosen buffer), exactly the technique the warp lambda uses for its bslot[] swap. When xfer_on
                // is false, ucmd==cmdBridge and the submit at the tail is the EXACT today A.q + vkWaitForFences
                // path (byte-identical, D1). uslot picks the free ping-pong slot; before reusing it we poll its
                // prior upload's completion (semUpTL>=its signaled value) non-blocking and wait ONLY if still in
                // flight (never reset an in-flight buffer; rare with N=2 + pair-rate uploads << warp ticks).
                const bool up_xfer = xfer_on;
                const int us = up_xfer ? uslot : 0;
                VkCommandBuffer ucmd = up_xfer ? cmdUpload[us] : cmdBridge;
                if(up_xfer){
                    uslot ^= 1;   // advance the round-robin for the NEXT upload
                    if(uslot_val[us]!=0){
                        uint64_t cur=0; vkGetSemaphoreCounterValue(A.dev,A.semUpTL,&cur);   // non-blocking poll
                        if(cur < uslot_val[us]){   // this slot's prior upload is STILL executing on A.qT → wait (rare)
                            VkSemaphoreWaitInfo wi{}; wi.sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO; wi.semaphoreCount=1; wi.pSemaphores=&A.semUpTL; wi.pValues=&uslot_val[us];
                            vk_wait_sem_live(A.dev,wi);
                        }
                    }
                }
                VkCommandBuffer cmdBridge = ucmd;   // shadow: keep the large record body below textually unchanged
                vkResetCommandBuffer(cmdBridge,0);
                VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdBridge,&bi);
                auto up_imgA=[&](Img& dst,VkBuffer src,uint32_t w,uint32_t h){
                    img_barrier(cmdBridge,dst.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkBufferImageCopy cp=full_bic(w,h); vkCmdCopyBufferToImage(cmdBridge,src,dst.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                    img_barrier(cmdBridge,dst.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                };
                // R5 step 4: the per-channel transport is gated by the declared ROWS (cfg.layers.eff), not by the
                // hand flags. R7 retired the second oracle that sat beside each site: 49,014 transport decisions, 0
                // disagreements, over two pressured runs. These six sites read an eff bit and nothing else, so what
                // they rested on is the eff/cascade parity — layer_flow_resolve (exit 3 at startup) and the 34
                // layer_parity_* ctest cases; the one composite, eff(GME)||eff(GME_GPU), is check 3 of pfg_arm_test.
                const uint32_t up_eff = cfg.layers.eff;
                auto up_row = [&](pfg::layers::LayerId id){ return (up_eff & (1u << (unsigned)id)) != 0u; };
                const bool up_bidir  = up_row(pfg::layers::LayerId::BIDIR);
                const bool up_cand   = up_row(pfg::layers::LayerId::CANDIDATES);
                const bool up_gme    = up_row(pfg::layers::LayerId::GME) || up_row(pfg::layers::LayerId::GME_GPU);   // the model exists (either variant)
                const bool up_gmebwd = up_row(pfg::layers::LayerId::GME_BWD);
                const bool up_per    = up_row(pfg::layers::LayerId::PERSISTENCE);
                const bool up_vbl    = up_row(pfg::layers::LayerId::VBLEND);
                up_imgA(wapPrevA,hR_a[prev_slot].buf,WW,WH);
                up_imgA(wapCurA, hR_a[cur_slot].buf, WW,WH);
                // upload the iGPU contour field (cur_slot — the SAME real slot
                // as wapCurA / hR_a[cur_slot]) into wapFIELDA. Distinct from up_imgA: the imageLoad readers (the
                // fill AND the warp's binding 11) read the field as a STORAGE image in GENERAL, so it ends in
                // GENERAL (not RO). GENERAL→DST copy→GENERAL; the init oneshot left it GENERAL, and every prior pair
                // returns it to GENERAL. --afill OR --bg-snap (the warp reads it BEFORE the fill, same GENERAL state).
                if(cfg.d.field_to_warp){   // gated for ALL field consumers (afill, bg_snap, band_xfade, disoccl_hardpick, mc all read binding 11)
                    img_barrier(cmdBridge,wapFIELDA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyBufferToImage(cmdBridge,hFIELD_a[cur_slot].buf,wapFIELDA.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                    img_barrier(cmdBridge,wapFIELDA.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                }
                up_imgA(wapMVA,  hMV_a[gen].buf,  wap_mvw,wap_mvh);
                // --vblend: upload the NEXT pair's forward MV grid into wapMVTA (binding 12) — the
                // velocity-continuity TARGET. Sourced from the SAME hMV_a forward-MV bridge as wapMVA, just at
                // target_gen (the next-fresher published generation, computed at the call site; == gen when
                // there is no fresher pair → self-target → the shader mix toward self is a no-op, graceful).
                // Gated on cfg.vblend (the live flag, cleared on a create failure) so off → no upload, wapMVTA
                // stays in its initial RO state, and the shader never samples it (vblend_on=0).
                if(up_vbl) up_imgA(wapMVTA, hMV_a[target_gen].buf, wap_mvw,wap_mvh);
                up_imgA(wapSADA, hSAD_a[gen].buf, wap_mvw,wap_mvh);
                if(up_bidir) up_imgA(wapMVBA, hMVB_a[gen].buf, wap_mvw,wap_mvh);   // backward MV
                if(up_cand) up_imgA(wapC2A,  hC2_a[gen].buf,  wap_mvw,wap_mvh);   // second-best candidate (RGBA16F, binding 10)
                // upload the dissidence mask (R8, mvw×mvh) into wapDISA. The warp
                // samples it for the matte. use_gme is the live flag (cleared on any gme alloc failure
                // above) so wapDISA is guaranteed created when this fires.
                if(up_gme) up_imgA(wapDISA, hDIS_a[gen].buf, wap_mvw,wap_mvh);
                // upload the BACKWARD (cur-anchored) dissidence mask into wapDISBA (binding 7).
                // Same R8 mvw×mvh upload; gated on use_gme AND use_bidir (the bwd field/mask exist only
                // with --bidir). The dual-anchored matte unions it with wapDISA in the warp.
                if(up_gmebwd) up_imgA(wapDISBA, hDISB_a[gen].buf, wap_mvw,wap_mvh);
                // upload the inertia persistence mask (R8, mvw×mvh) into wapPERA (binding 8). F
                // wrote hPER_a[gen] from its continuous persist[] array after the persistence update. Gated
                // on use_inertia; off-inertia wapPERA stays in its initial RO state and is never sampled
                // (inertia_thresh=0). Same DST→RO per-pair upload contract as the dissidence masks.
                if(up_per) up_imgA(wapPERA, hPER_a[gen].buf, wap_mvw,wap_mvh);
                // ── in-cmdBridge 3x3 consensus on the just-uploaded MV image(s) ─────
                // The MV image is RO after up_imgA (the pass samples it); cur_real (wapCurA) was uploaded
                // above too (RO) — the consensus pass reads it for color membership. Dispatch
                // pass→scratch (GENERAL), then copy scratch→MV image and leave it RO (the state
                // wap_warp_present expects). All in THIS cmd buffer, ordered by ALL_COMMANDS img_barriers
                // (the same hazard model the uploads use) → no extra fence/submit. The scratch returns to
                // GENERAL so the bwd pass (and the next pair) finds it ready. Filters wapMVBA too under
                // bidir. median_sim>0 (mv-guided) → color-weighted consensus; 0 (mv-median
                // alone) → the blind McGuire median, byte-identical when off.
                // The gate + sim use the LIVE bools (NOT the create-time run_median_pass const) so a
                // pipe-create failure — which clears BOTH bools and destroys medPipe — correctly skips
                // the dispatch (the const would be stale-true → use-after-destroy). median_sim is the
                // SAME live value (cfg.mv_sim under guided, else 0).
                if(cfg.mv_consensus && (use_mv_median||use_mv_guided)){   // R5: the consensus row's own switch (--no-mv-consensus); default ON = the former condition
                    const float sim_push = use_mv_guided ? cfg.mv_sim : 0.f;
                    auto median_filter=[&](VkDescriptorSet set,Img& mv){
                        vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,medPipe.pipe);
                        vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,medPipe.layout,0,1,&set,0,nullptr);
                        struct{float sim;}pcm{sim_push}; vkCmdPushConstants(cmdBridge,medPipe.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcm),&pcm);
                        vkCmdDispatch(cmdBridge,(wap_mvw+7)/8,(wap_mvh+7)/8,1);
                        // scratch: shader-write → transfer-read; mv: RO(sampled) → transfer-write.
                        img_barrier(cmdBridge,wapMVScratchA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                        img_barrier(cmdBridge,mv.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
                        { VkImageCopy ic{}; ic.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; ic.dstSubresource=ic.srcSubresource; ic.extent={wap_mvw,wap_mvh,1};
                          vkCmdCopyImage(cmdBridge,wapMVScratchA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,mv.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&ic); }
                        // mv: transfer-write → RO (the wap_warp sampled state); scratch: transfer-read → GENERAL.
                        img_barrier(cmdBridge,mv.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                        img_barrier(cmdBridge,wapMVScratchA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                    };
                    median_filter(medPipe.set_mv,wapMVA);
                    if(use_bidir) median_filter(medPipe.set_mvb,wapMVBA);
                }
                vkEndCommandBuffer(cmdBridge);
                if(up_xfer){
                    // --upload-xfer: submit to A.qT — WAIT semWarpTL>=xfer_W (the WAR back-edge:
                    // every warp submitted so far has finished READING the in-place images before we overwrite
                    // them) and SIGNAL semUpTL=++xfer_U (the RAW edge the warp waits on). NO host wait
                    // the cross-queue order is GPU-side. The wait value xfer_W is BACKWARD-looking (prior
                    // warps), so it is already (or will be) signaled regardless of this upload → no deadlock.
                    // A.q is NOT touched here (present stays exclusive to P) — the submit goes to A.qT only.
                    ++xfer_U;
                    const uint64_t waitVal = xfer_W, sigVal = xfer_U;
                    VkTimelineSemaphoreSubmitInfo tssi{}; tssi.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                    tssi.waitSemaphoreValueCount=1; tssi.pWaitSemaphoreValues=&waitVal;
                    tssi.signalSemaphoreValueCount=1; tssi.pSignalSemaphoreValues=&sigVal;
                    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;   // the upload copies (transfer) + the in-upload median dispatch (compute) must not run until the prior warp's reads completed
                    VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.pNext=&tssi;
                    si.waitSemaphoreCount=1; si.pWaitSemaphores=&A.semWarpTL; si.pWaitDstStageMask=&waitStage;
                    si.signalSemaphoreCount=1; si.pSignalSemaphores=&A.semUpTL;
                    si.commandBufferCount=1; si.pCommandBuffers=&cmdBridge;
                    vkQueueSubmit(A.qT,1,&si,VK_NULL_HANDLE);
                    uslot_val[us] = xfer_U;   // remember this slot's signaled value for the next reuse guard
                } else {
                    vkResetFences(A.dev,1,&fBridge);
                    VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmdBridge;
                    vkQueueSubmit(A.q,1,&si,fBridge); vk_wait_live(A.dev,fBridge);   // catch a TDR on the saturated 4090 present/warp -> g_quit -> graceful exit
                }
            };
            // Per tick (WAP-on-A) warp at the exact phase t into wapOutA, blit → bridge
            // (rgba8→bgra8 + scale, FILTER_LINEAR), submit. The warp + blit run in ONE A.q submit;
            // the keyed mutex (key 0) is chained so the pillar's AcquireSync(0) sees the release.
            // No host bounce, no G in the present path (lowest latency).
            int outdump_left=cfg.outdump_n, outdump_idx=0;   // instrument (declared before the lambda that uses them)
            // layer 1 (--qdump <dir> N) the data-tap state, declared before the lambda that captures it.
            // qdump_left counts DOWN the remaining triples; qdump_idx names the file index. qdump_man_open gates
            // the one-time `size W H` manifest header (written on the FIRST dump, after the dir exists).
            //
            // SAMPLING (S2.T1b, 2026-09-03) - a COVERAGE sampler, not a stride. The record exists to be R3's
            // M4 replay set, and a set whose ticks all sit at one phase tests the core at one phase. The former
            // fixed stride (11, "coprime with the phase steps") was MEASURED not to spread: over 16 triples the
            // phase landed in two eighth-of-a-pair bins and one ring slot. A stride CANNOT spread here, because
            // the dump stalls its own tick (three full-frame copies + a fence), the content clock recovers from
            // that stall the same way every time, and so the phase N ticks later is a deterministic function of
            // the stall rather than of N - the sampler synchronises with its own perturbation.
            //
            // Instead: dump on a tick whose phase bin is the LEAST-COVERED bin the ladder has actually produced
            // (seen[] gates the minimum, so we can never wait forever for a bin this refresh ratio never emits),
            // and whose generation ring slot is likewise least-covered - the slot condition is dropped after
            // kQdumpStarve skipped candidates so the two conditions can never deadlock each other. Dumping a bin
            // makes it ineligible until the others catch up, which is exactly what pinning cannot survive.
            // kQdumpMinGap keeps the next candidate away from the stall we just caused, so the phase we record
            // is a SETTLED phase and not the clock's recovery transient.
            int qdump_left=cfg.qdump_n, qdump_idx=0; uint32_t qdump_tick=0; bool qdump_man_open=false;
            static constexpr int      kQdBins      = 8;    // eighth-of-a-pair phase buckets (check_qdump_plus.py bins the same way)
            static constexpr uint32_t kQdumpMinGap = 8u;   // ticks before the next candidate: let the clock re-lock after the dump stall
            static constexpr uint32_t kQdumpStarve = 64u;  // skipped candidates after which the ring-slot condition is dropped
            uint32_t qd_bin_hits[kQdBins]={}, qd_gen_hits[kGenRing]={};
            bool     qd_bin_seen[kQdBins]={}, qd_gen_seen[kGenRing]={};
            uint32_t qd_skip=0, qd_last_tick=0; bool qd_any=false;
            // --wsub sub-timings of the per-tick warp lambda. These split the `warp` cost EMA
            // (= the wap_warp_present end-to-end) into: rec = CPU cmd record (reset→begin→bind→push→
            // dispatch→barriers→blit→end), gpu = submit + the BLOCKING vkWaitForFences (the warp dispatch
            // + blit run on A.q here — the GPU shader stack lives in this segment), prs = bridge_present()
            // (the present pillar / dcomp path). The mass-read after the fence is folded into prs (it is a
            // single host SSBO read, sub-µs). `up` is timed separately at the wap_upload CALL SITE below
            // (the pair-advance upload is a DISTINCT submit, outside this lambda's pw window) — it is the
            // "(a) optional wap_upload when it fires" of the mission. EMAs (0.8/0.2, matching wap_warp_ema)
            // so the wsub print is per-second-stable. Declared before the lambda so its capture sees them.
            double w_rec_ema=0.0,w_gpu_ema=0.0,w_prs_ema=0.0,w_up_ema=0.0;
            // ── --async-present: the slot view + present-thread state ─────────────────
            // A unified view over the two bridge slots so the warp lambda is one body for both paths. Slot 0 is
            // ALWAYS the today-resources (cmdBridge/fBridge/bridge_img/bridge_mem + the D3D11 tex/nt/km). Slot 1
            // is the second slot, valid only when cfg.async_present (else it ALIASES slot 0, so any
            // accidental slot-1 reference on the off path is harmless). The off path uses ONLY cmdBridge/fBridge/
            // bridge_img.img/bridge_mem directly — byte-identical when off.
            using BridgeSlot = pfg::present::BridgeSlot;   // R4: the slot type lives in the stage; the objects below are BOUND into it
            BridgeSlot (&bslot)[2] = pres.bslot;
            // FIX: when async is on, slot 0's WARP uses the DEDICATED cmdBridgeA0/fBridgeA0 (not the
            // shared cmdBridge/fBridge that wap_upload resets per-pair). It still presents through the shared
            // bridge texture (bridge_img/nt/mem). When async is off, bslot[0] aliases cmdBridge/fBridge so the
            // off path (which uses bslot[0]) stays byte-identical when off.
            bslot[0] = BridgeSlot{ bridge_tex, bridge_nt, bridge_km_d3d, bridge_img.img, bridge_mem,
                                   cfg.async_present ? cmdBridgeA0 : cmdBridge,
                                   cfg.async_present ? fBridgeA0   : fBridge };
            bslot[1] = cfg.async_present ? BridgeSlot{ bridge_tex1, bridge_nt1, bridge_km_d3d1, bridge_img1.img, bridge_mem1, cmdBridge1, fBridge1 } : bslot[0];
            int& async_front=pres.async_front;     // R4: stage-owned — the slot holding the last COMPLETED warp (presentable); -1 = none yet
            // (async_inflight — the slot with a warp in flight — is stage-owned and no longer read here; see PresentStage)
            uint64_t& sq_hits=pres.sq_hits; uint64_t& sq_misses=pres.sq_misses; uint64_t last_sqh_p=0, last_sqm_p=0;   // R4: the --shallow-queue counters are stage-owned; the stats window their deltas here
            // ── STAGE 5 (the seam): the output path's barriers, DERIVED (R2b). Declared lazily on the
            // first recording tick (wapOutA / bridge_img exist by then) and re-executed every tick;
            // execute() patches handles and allocates nothing (graft G5). --sg-barriers off -> never
            // built, never executed, and the hand-written img_barrier path below runs byte-identically.
            pfg::seam::SeamGraph  sg5;
            pfg::seam::Compiled   sg5_c;
            pfg::seam::ResId      sg5_out=0, sg5_bridge=0;
            bool sg5_ready=false, sg5_failed=false, sg5_dumped=false;
            const bool sg5_want = cfg.sg_barriers && A.has_sync2;
            if(cfg.sg_barriers && !A.has_sync2)
                std::printf("[ra] --sg-barriers: synchronization2 is NOT enabled on device A -- falling back to the hand-written barriers\n");
            auto wap_warp_present=[&](float t,float extrap,const float* gme6,bool bwd_ok,float thr_eff,uint32_t* presented_out,bool do_warp=true){
                // --qdump+ (S2.T1): a byte copy of the push block AS SUBMITTED. `pcw` lives in a nested
                // block that closes before the dump tap below, and copying at the submit site is also the
                // truer record: these are exactly the bytes the GPU received this tick. Inert unless
                // --qdump is on (qd_push_sz stays 0 and nothing is written).
                unsigned char qd_push[512]; size_t qd_push_sz=0;
                const double wsub_rec0 = cfg.wsub ? now_ms() : 0.0;
                double wsub_gpu0 = 0.0;   // hoisted out of the record block (assigned inside it) so the
                                          // --wsub `gpu` segment timing below still resolves on a dropped tick.
                // ── --async-present: non-blocking preamble ────────────────────────────
                // ap==false → this whole block is inert: back=0, record_this_tick=true, and the cb/fb/bimg/bmem
                // shadows below alias the ORIGINAL slot-0 resources → the body runs byte-identically to today.
                const bool ap = cfg.async_present;
                // (--load-governor, warp-light) under sustained 4090 saturation, shed the warp's
                // HEAVIEST optional passes (the bg-snap disocclusion band, the vblend velocity-tilt, band-xfade,
                // the multicand medoid) to shrink the per-tick 4090 GPU slice = the make-space lever (the 4090-side
                // arm of the governor, complementing the F-thread tier shed). Gated on the P-thread's OWN published
                // 4090 util (g_gpu_a_util, ~1Hz, one-tick-stale — fine for a sustained signal) crossing gov_util;
                // reachable ONLY under --load-governor → byte-identical when off (warp_light==false reverts every
                // gate exactly). The CORE warp (MV sample + blend) NEVER sheds (keep-generating). g_gpu_a_util<0
                // (NVML not armed) → warp_light=false. Lock-free (an advisory atomic load, no fence).
                const int  gpuA_wl = g_gpu_a_util.load();
                // control-word: derive warp_light from the SHARED governor decode (floor>=5 ⟺
                // util>=gov_util) so present + flow read ONE mapping, never two (the load_governor desync).
                // SELF-KEYED default: the deep shed engages only when util says band-5 AND our OWN slice is
                // distressed (gov_self_distress, latched in the publish block below) — util-high alone with a
                // healthy FG no longer sheds (the finding-#1 fix). --gov-util-floor restores the pure-util
                // gate for A/B. Both arms (this warp shed + the F-thread tier floor) read the SAME latch.
                const bool util_band5   = governor_floor_for_util(gpuA_wl, cfg.gov_util) >= 5;
                const bool warp_light   = cfg.load_governor && util_band5
                                        && (cfg.gov_util_floor ? true : gov_self_distress);
                // Poll the warp submitted on a PRIOR tick. If complete, it is now the freshest presentable frame,
                // and the mass read (the matte feedback) moves HERE (post-completion) — the data is ready.
                // ── STAGE 6 (R4/X12): the async preamble + the slot decision, stage-owned. The DECISION is an input:
                // Warp, or Dup on an exact-duplicate --fdrop tick (do_warp=false → the completed front is re-shown);
                // the stage returns Drop when a warp is still in flight (the async re-present drop, rdrop_ticks).
                pres.poll_inflight(presented_out);
                const pfg::present::Tick tk = pres.begin(do_warp ? pfg::present::Decision::Warp : pfg::present::Decision::Dup, /*count_drop=*/true);
                const bool record_this_tick = tk.record;
                VkCommandBuffer cmdBridge  = tk.cmd;
                VkFence         fBridge    = tk.fence;
                Img             bridge_img = Img{ tk.img, VK_NULL_HANDLE, VK_NULL_HANDLE };
                VkDeviceMemory  bridge_mem = tk.mem;
                if(record_this_tick){
                vkResetCommandBuffer(cmdBridge,0);
                VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdBridge,&bi);
                pres.timing_begin(cmdBridge,tk.back);   // --warp-timing (R4b): the GPU timestamp at the top of the warp batch; no-op unless armed
                // reset the DEVICE-LOCAL mass counter ON-GPU (vkCmdFillBuffer→0), then barrier
                // TRANSFER_WRITE→SHADER_WRITE so the dispatch's atomicAdds observe the cleared value.
                // The counter SSBO lives in VRAM (not a host-coherent buffer), so the per-workgroup
                // atomicAdd pays no PCIe round-trip. The copy back to the host buffer happens after the
                // dispatch.
                vkCmdFillBuffer(cmdBridge,devMass.buf,0,VK_WHOLE_SIZE,0u);
                { VkBufferMemoryBarrier bb{}; bb.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                  bb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; bb.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
                  bb.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; bb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                  bb.buffer=devMass.buf; bb.offset=0; bb.size=VK_WHOLE_SIZE;
                  vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&bb,0,nullptr); }
                // R3: under --fg-core the fg_core.comp kernel IS the product dispatch (below, after the push block is assembled
                // for the --qdump+ record); the legacy bind / push / dispatch are skipped. fg_on also covers --fg-core-ab.
                const bool fg_on=(cfg.fg_core||cfg.fg_core_ab) && fgPipeA.pipe!=VK_NULL_HANDLE;
                const bool fg_product=cfg.fg_core && fgPipeA.pipe!=VK_NULL_HANDLE;
                if(!fg_product){
                vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,wapPipeA.pipe);
                vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,wapPipeA.layout,0,1,&wapPipeA.set,0,nullptr);
                }
                // occl_thresh = the bidir gate. 0 when bidir is off → the shader's whole
                // classification block is skipped (byte-identical to off), even though the
                // MV_bwd binding is bound. Only nonzero when use_bidir (which requires --bidir + WAP).
                // div_eps = the fill-div gate inside the bidir neither-class. 0 unless
                // use_fill_div (which requires --fill-div + active bidir) → byte-identical when off.
                // rescue_on = the candidate-rescue gate. 0 unless use_rescue (which requires
                // --rescue + WAP + commit armed) → byte-identical when off (the shader gate is
                // rescue_on>0.5 AND commit_thresh>0, both uniform push values).
                // mv_guided = the bilateral-fetch gate, ENCODING the membership band. 0 unless
                // use_mv_guided → byte-identical LINEAR fetch. When on, = 1.0 + cfg.mv_sim (>0.5 gates ON;
                // the shader recovers sim = mv_guided − 1.0, the --mv-sim value the consensus pass uses).
                // gme_on gates the frame-holon's model rescue candidate + fill-div assist, and
                // {ga..gf} carry the 6 affine params (mv = (ga+gb·gx+gc·gy, gd+ge·gx+gf·gy), gx,gy the
                // mv-grid coords). gme_on>0.5 only when use_gme AND this generation's model is valid
                // (gme6 non-null); 0 → the shader reads NONE of the gme block, byte-identical.
                // matte_on gates the fluid-matte. matte_on>0.5 ONLY when use_matte AND the model
                // is valid for THIS generation (gme_push) — the BACKGROUND override evaluates gme_model_mv,
                // so an unfitted generation must never matte (it would have no valid background layer).
                // matte_thresh = thr_eff (R8-normalized cutoff). 0 → the shader's whole matte block is
                // skipped, byte-identical. thr_eff is the P-side mass-feedback-tuned threshold
                // (= cfg.matte_thresh when mass_k=0 → the clean occupancy lerp). Push grows 72→80B.
                const bool gme_push = (gme6!=nullptr);
                // bwd_ok=false (F skipped this pair's bwd pass under pressure) → push
                // occl_thresh=0 (the bidir round-trip classification block is skipped, byte-identical
                // to the off-bidir warp) AND matte_on=0 (the dual-anchored matte has no valid bwd mask
                // for this generation). Per-generation graceful degrade — the fwd warp + the
                // safety net still run. bwd_push folds bwd_ok into the existing bidir/matte gates.
                const bool bwd_push = (use_bidir && bwd_ok);
                const bool matte_push = (use_matte && gme_push && bwd_ok);
                // phase_anchor_on = the phase-anchored primary MV gate. 1.0 iff bwd_push
                // (use_bidir AND the bwd field is valid this generation = bwd_ok) AND cfg.phase_anchor.
                // The cur-anchored re-anchor reads binding 5 (the bwd MV field), so it needs exactly what
                // the bidir round-trip needs: an active, this-generation-valid bwd field — bwd_push folds
                // both. The shader additionally guards on occl_thresh>0.0 (== bwd_push here) so a stale push
                // can never read an unbuilt bwd field. 0 → mv = the prev-anchored forward field, byte-
                // identical. Push grows 112→116B (29 floats); pcr.size=116; budget 128B.
                const bool phase_push = (bwd_push && cfg.phase_anchor);
                // stasis_thresh = the stasis-layer gate. 0 unless cfg.stasis → byte-identical
                // (the shader's `stasis_thresh>0.0` short-circuits the override). No flag dependency: the
                // stz tap is the SAD field already bound for Gate 1, so stasis works on ANY stack (plain
                // WAP through full matte). Push grows 80→84B (21 floats); pcr.size=84; budget 128B.
                // inertia_thresh = the inertia-prior gate. 0 unless use_inertia → byte-identical
                // (the shader's `inertia_thresh>0.0` short-circuits all three gates). When on, the per-block
                // persistence (binding 8) restricts the rescue/bilateral/gme paths for static-history pixels.
                // Push grows 84→88B (22 floats); pcr.size=88; budget 128B.
                // crescent_on = the crescent-directed background-fetch gate. 1.0 iff matte_push
                // (use_matte AND a valid model for this generation) AND cfg.crescent → matte governs (no
                // matte ⇒ no masks ⇒ no crescent; the --no-matte/--no-gme/--no-bidir/--no-wap cascades take
                // it down via matte_push). 0 → the shader's matte BACKGROUND blend stays the (1−t,t) form,
                // byte-identical. Push grows 88→92B (23 floats); pcr.size=92; budget 128B.
                const bool crescent_push = (matte_push && cfg.crescent);
                // travel_on = the traveling-silhouette occupancy gate. 1.0 iff matte_push
                // (use_matte AND a valid model for this generation) AND cfg.travel → matte governs (same
                // as crescent: no matte ⇒ no masks ⇒ no travel; the --no-matte/--no-gme/--no-bidir/--no-wap
                // cascades take it down via matte_push). 0 → the shader's matte OCCUPANCY stays the
                // full-testimony lerp, byte-identical. The crescent's dis_fwd/dis_bwd are UNAFFECTED (still
                // the full max(self,gather)); only the occupancy decision fades the selves. Push grows
                // 100→104B (26 floats); pcr.size=104; budget 128B.
                const bool travel_push = (matte_push && cfg.travel);
                // contour_on = the contour-marriage gate. 1.0 iff matte_push (use_matte AND a
                // valid model for this generation) AND cfg.contour → matte governs (the band needs the
                // masks; the --no-matte/--no-gme/--no-bidir/--no-wap cascades take it down via matte_push,
                // same as crescent/travel). 0 → the shader's matte composition stays the binary
                // !matte_object decision, byte-identical. Push grows 104→108B (27 floats); pcr.size=108;
                // budget 128B.
                const bool contour_push = (matte_push && cfg.contour);
                // obj_crescent_on = the object-crescent gate. 1.0 iff matte_push (use_matte AND a
                // valid model for this generation) AND cfg.obj_crescent → matte governs (the claims come from
                // the dual-anchored dissidence masks; no matte ⇒ no masks ⇒ no claims; the --no-matte/--no-gme/
                // --no-bidir/--no-wap cascades take it down via matte_push, same as crescent/travel/contour). 0
                // → the shader's OBJECT A/B blend stays the (1−t,t) form AND the commit side pick stays
                // time-nearest, byte-identical. Push grows 108→112B (28 floats); pcr.size=112; budget 128B.
                const bool objcres_push = (matte_push && cfg.obj_crescent);
                // appear_on = the appearance-band re-blend gate. 1.0 iff commit is LIVE
                // (commit_thresh>0 → there ARE committed pixels) AND cfg.appearance → commit governs
                // (--no-commit takes appearance down via cascade; a bare --commit-thresh 0 still gates
                // off here). 0 → the shader's committed value passes through unchanged, byte-identical.
                // Push grows 92→100B (25 floats).
                const bool appear_push = (cfg.commit_thresh > 0.f && cfg.appearance);
                // ambig_on = the ambiguity-channel gate. 1.0 iff use_ambig (the candidate field is
                // shipped THIS generation — it is shipped every pair when use_ambig, no per-generation degrade
                // like bwd_ok, since the FWD copy-out is unconditional under use_ambig). 0 → the shader's
                // primary MV is the matcher's raw pick, byte-identical (the binding-10 placeholder is never
                // sampled). The shader additionally gates the rule on gme_on>0.5 (the referee is gme_model_mv);
                // use_ambig already required the FINAL use_gme above, so gme_on is always set when ambig_on is.
                // Push grows 116→120B (30 floats); pcr.size=120; budget 128B.
                const bool ambig_push = use_ambig;
                // member_commit_on = the membership-beats-the-blend gate. 1.0 iff matte_push
                // (use_matte AND a valid model for this generation) AND cfg.member_commit → matte governs
                // (the self-mask reads the dual-anchored dissidence masks; no matte ⇒ no masks ⇒ no
                // self-mask; the --no-matte/--no-gme/--no-bidir/--no-wap cascades take it down via
                // matte_push, same as crescent/travel/contour/obj-crescent). 0 → the shader's warp-vs-blend
                // selection is the EXACT off path (member_strength=0 → the soft mix and the hard
                // branch unchanged), byte-identical. Push grows 120→124B (31 floats); pcr.size=124; budget 128B.
                const bool member_push = (matte_push && cfg.member_commit);
                // commit_default_on = the commit-default flip gate. 1.0 iff use_commit_default
                // (= cfg.commit_default && use_wap — a WARP-LEVEL rule, MATTE-INDEPENDENT, so NOT folded
                // into matte_push like member/crescent/etc.). 0 → the shader's warp-vs-blend selection keeps
                // the photometric fallback (the fixed cross-fade), byte-identical. Push grows 124→128B (32
                // floats); pcr.size=128 — AT the spec-minimum push budget (Vulkan guarantees
                // maxPushConstantsSize ≥ 128B for every conformant implementation; both target GPUs report
                // 256B). Legal, exactly met — not exceeded.
                const bool commit_default_push = use_commit_default;
                // onepos_on = the one-position-collapse gate. 1.0 iff use_onepos (= cfg.onepos &&
                // use_wap — a WARP-LEVEL rule, MATTE-INDEPENDENT, so NOT folded into matte_push, exactly like
                // commit_default). 0 → the shader's warp_result stays the two-sample blend, byte-identical. Push
                // grows 128→132B (33 floats); pcr.size=132. The rig reports maxPushConstantsSize=256B (both
                // target GPUs — the budget comment), so 132B is within the device limit; 128B is the
                // Vulkan spec-min guarantee, not a project cap.
                const bool onepos_push = use_onepos;
                // disoccl_commit_on = the occlusion-aware one-sided-commit gate. 1.0 iff matte_push
                // (use_matte AND a valid model AND bwd ok) AND cfg.disoccl_commit — matte governs because the
                // commit's evidence is the dissidence masks (dis_fwd/dis_bwd), valid only under matte. 0 → the
                // two `w_sum<1e-4` blend fallbacks stand exactly → byte-identical. Cascades via matte_push like
                // crescent/obj_crescent (same dissidence dependency).
                const bool disoccl_commit_push = (matte_push && cfg.disoccl_commit);
                // bg_snap_on = the image-field background-snap gate. 1.0 iff cfg.bg_snap (which
                // survived all create/import degrades → the field is uploaded each pair AND binding 11 holds the
                // real wapFIELDA). The shader ADDITIONALLY gates the rule on gme_on>0.5 (gme_model_mv must be the
                // valid background model — gme is default-ON; if it degraded, the snap is simply inert, no crash).
                // WARP-LEVEL (NOT folded into matte_push — the side is photometric d0, matte-free, exactly because
                // the dissidence is corrupted in the gravity band). 0 → mv untouched → byte-identical.
                const bool bg_snap_push = cfg.bg_snap && !warp_light;   // shed the disocclusion band under 4090 saturation
                // --vblend: vblend_on = the velocity-continuity gate. 1.0 iff cfg.vblend (which
                // survived the create degrade → wapMVTA exists AND binding 12 holds the real MV-target image;
                // the per-pair upload fills it from hMV_a[target_gen]). WARP-LEVEL (NOT folded into matte_push —
                // it tilts the sample-offset MV, matte-free). 0 → the shader's sample offsets use the raw mv →
                // byte-identical (u_mv_target never sampled).
                const bool vblend_push = cfg.vblend && !warp_light;   // shed the velocity-tilt under 4090 saturation
                // onepos_band — the collapse-onset scale (the dial; 1.0 =
                // exactly). Push 132→136B (34 floats); within the 256B device limit. Pushed 1.0 when off.
                // + bg_snap_on/strength/norm + extrap appended LAST → 39 floats/156B; pcr.size=156 (well within
                // the 256B device limit both target GPUs report; 128B is the Vulkan spec-min, not a project cap).
                // + vblend_on/t0/strength appended LAST → 42 floats/168B; pcr.size=168 (still well within 256B).
                // + vblend_exact appended LAST → 44 floats/176B; pcr.size=176 (still well within 256B).
                // + ts_smooth appended LAST → 45 floats/180B; pcr.size=180 (still well within 256B).
                // + mc_on/mc_nperturb/mc_perturb/mc_disp/mc_edge appended LAST → 50 floats/200B; pcr.size=200 (still well within 256B).
                // + cam_lead(clx,cly) + disoccl_hardpick → 53 floats/212B; + predict_p2 appended LAST → 54 floats/216B; pcr.size=216 (still well within 256B). Keep pcr.size in warp_blend.cpp in sync.
                // --camera-twarp: clx carries the PER-PIXEL lead scalar `amt`; the shader leads each pixel by its OWN
                // MV (× amt) — NOT a global gme shift (which deformed the non-uniform scene = the "jelly"). No gme dependency now:
                // clx = cfg.camera_twarp ? amt: 0 (OFF → 0 → the shader's uv==uv0 → BYTE-IDENTICAL). cly unused (the lead vector is
                // per-pixel, from the MV field, computed shader-side). SIGN: +mv (forward); eye-tunable — negate clx if lag worsens.
                const float clx = cfg.camera_twarp ? cfg.camera_twarp_amt : 0.f;
                const float cly = 0.f;
                // --mv-edge-snap encoding. variant + sim_band: variant 1 (G1 dissidence) requires the
                // dissidence mask valid THIS generation (gme_push, same gate that uploads wapDISA);
                // when it is not, auto-fall-back to G2 (color, always available from cur_real) so the
                // fix still runs instead of guiding on a stale/placeholder mask. sim band = --mes-sim
                // override else --mv-sim. 0 when off or WAP-invalid (use_mv_guided implies WAP) →
                // byte-identical. Recovered shader-side as use_dis=(v<2.0), sim=fract(v).
                float mes_push = 0.f;
                if (cfg.mv_edge_snap != 0) {
                    const int   es_var  = (cfg.mv_edge_snap == 1 && !gme_push) ? 2 : cfg.mv_edge_snap;   // G1→G2 when no valid mask
                    const float es_simb = (cfg.mv_edge_snap_sim > 0.f) ? cfg.mv_edge_snap_sim : cfg.mv_sim;
                    mes_push = (float)es_var + es_simb;   // e.g. 1.10 = G1@0.10, 2.10 = G2@0.10
                }
                // --single-track (offset 224): v3 — the pre-store B-track override (the exact
                // --blend-solo 2 site + value: B_samp) + staged re-admissions. ENCODING: 0 = OFF
                // (byte-identical); 1.0 = v3.1 (stasis re-admitted at the site — proven-static pixels
                // present crisp cur[uv]; THE SHIPPING CANDIDATE, the --single-track default); 2.0 =
                // v3.0 (--st-no-stasis: pure B_samp, byte-equal to --blend-solo 2 — the
                // indistinguishability eye-gate). Both values (>0.5) also arm the upstream v1/v2
                // gates, now shadowed by the final override (harmless; they save dead work).
                const float sto_push = (cfg.single_track && use_wap) ? (cfg.st_no_stasis ? 2.f : 1.f) : 0.f;
                // --bg-reclaim (offset 228): the gravity fix. Carries the STRENGTH (>0.001 = ON). Needs
                // gme_model_mv valid THIS generation (gme_push, same gate that arms the gme block); when
                // the model is stale push 0 so the reclaim is inert (byte-identical) rather than damping
                // toward a placeholder model. 0 → mv untouched → byte-identical.
                const float bgr_push = (cfg.bg_reclaim > 0.f && gme_push && use_wap) ? cfg.bg_reclaim : 0.f;
                struct{float rc;float ci;float ag;float t;float sg;float ct;float cr;float ol;float de;float re;float mg;
                       float gon;float ga;float gb;float gc;float gd;float ge;float gf;float mon;float mth;float sth;float sti;float cre;float aon;float aba;float tr;float co;float oc;float pa;float am;float mc;float cd;float op;float ob;float dc;float bs;float bss;float bsn;float ex;float vbon;float vbt0;float vbst;float bxf;float vbe;float tss;float mco;float mcn;float mcp;float mcd;float mce;float clx;float cly;float dhp;float pp2;float bso;float mes;float sto;float bgr;}   // +sto (--single-track B-track base) +bgr (--bg-reclaim gravity-fix strength) appended LAST → 58 floats/232B (within the 256B device limit; 8B headroom)
                  pcw{cfg.res_ceil,cfg.conf_improv,cfg.agreement,t,cfg.soft_gate?1.f:0.f,cfg.commit_thresh,cfg.commit_real?1.f:0.f,bwd_push?cfg.occl_thresh:0.f,use_fill_div?cfg.div_eps:0.f,use_rescue?1.f:0.f,use_mv_guided?1.0f+cfg.mv_sim:0.f,
                      gme_push?1.f:0.f,
                      gme_push?gme6[0]:0.f,gme_push?gme6[1]:0.f,gme_push?gme6[2]:0.f,
                      gme_push?gme6[3]:0.f,gme_push?gme6[4]:0.f,gme_push?gme6[5]:0.f,
                      matte_push?1.f:0.f,matte_push?thr_eff:0.f,
                      cfg.stasis?cfg.stasis_thresh:0.f,
                      use_inertia?cfg.inertia_thresh:0.f,
                      crescent_push?1.f:0.f,
                      appear_push?1.f:0.f,appear_push?cfg.appear_band:0.f,
                      travel_push?1.f:0.f,
                      contour_push?1.f:0.f,
                      objcres_push?1.f:0.f,
                      phase_push?1.f:0.f,
                      ambig_push?1.f:0.f,
                      member_push?1.f:0.f,
                      commit_default_push?1.f:0.f,
                      onepos_push?1.f:0.f,
                      onepos_push?cfg.onepos_band:1.f,
                      disoccl_commit_push?1.f:0.f,
                      bg_snap_push?1.f:0.f,bg_snap_push?cfg.bg_snap_strength:0.f,bg_snap_push?cfg.bg_snap_norm:0.f,
                      extrap,   // (ASW) the forward-extrapolation amount (phase units past 1); 0 when --asw off or no overshoot
                      vblend_push?1.f:0.f,vblend_push?cfg.vblend_t0:0.f,vblend_push?cfg.vblend_strength:0.f,   // --vblend: velocity-continuity gate + ramp-onset phase + max tilt weight; vblend_on=0 → byte-identical
                      warp_light?0.f:cfg.band_xfade,   // --band-xfade: the gravity-cancellation crossfade strength (0 = OFF → bx_w=0 → byte-identical); warp-light forces 0 under 4090 saturation
                      (cfg.vblend_exact?1.f:0.f),   // --vblend-exact: 0 = PREDICT (default, byte-identical: shader does 2·mv−mv_ut); >0.5 = EXACT (u_mv_target holds the REAL next-pair MV)
                      cfg.ts_smooth,   // --ts-smooth: adaptive temporal-smoothing strength (0 = OFF → no blend → byte-identical; >0 = blend toward prev output on garbage/low-conf pixels)
                      ((cfg.mc_on && !warp_light)?1.f:0.f),(float)cfg.mc_nperturb,cfg.mc_perturb,cfg.mc_disp,cfg.mc_edge,   // --multicand: medoid-select gate + #perturbed candidates + perturb-frac + dispersion-threshold + Sobel edge-threshold; mc_on=0 → the shader skips the whole branch → byte-identical; warp-light sheds the medoid under 4090 saturation
                      clx,cly,   // --camera-twarp: the global sample-UV lead (gme affine translation/out_size·amt); (0,0) when --camera-twarp off or gme invalid → the shader's uv+=(0,0) is a no-op → byte-identical. SIGN eye-tunable (negate to flip).
                      cfg.disoccl_hardpick,   // --disoccl-hardpick: edge-gated hard-pick threshold at the bidir reveal band; 0 → the shader branch is never entered → byte-identical. (offset 208), after cam_lead, so no prior field shifts.
                      (cfg.predict && cfg.predict_p2)?1.f:0.f,   // --predict-p2: 1.0 = the extrap tap uses mv_target_s (2*mv - mv_prev, const-accel); 0 = P1 (raw mv, const-velocity) = today's ASW tap. Appended (offset 212). Needs vblend armed (default ON) for a valid mv_target_s; gated on cfg.predict so it is 0 without --predict → byte-identical to today's ASW.
                      (float)cfg.blend_solo,   // --blend-solo DIAGNOSTIC (0=off,1=A-track,2=B-track): (offset 216). 0 → the shader's pre-store override branch is never entered → byte-identical. Measurement-only.
                      mes_push,   // --mv-edge-snap: cross-bilateral (edge-aware sub-block) primary MV fetch, packed variant+sim (offset 220). 0 → the primary MV keeps its guided/LINEAR fetch → byte-identical.
                      sto_push,   // --single-track: the composite base becomes the B-track (offset 224). 0 → wa_eff==wa → byte-identical. Quality layers stay active on the base; A re-admitted only where the occlusion machinery owns it.
                      bgr_push};  // --bg-reclaim: tile-level gravity fix — damp gme-nonconform bg-fringe MV toward the model (offset 228). Carries strength (>0.001=ON). 0 → mv untouched → byte-identical.
                if(!fg_product) vkCmdPushConstants(cmdBridge,wapPipeA.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcw),&pcw);
                if(cfg.qdump_n>0){ static_assert(sizeof(pcw)<=sizeof(qd_push),"qd_push too small for the warp push block");
                    qd_push_sz=sizeof(pcw); std::memcpy(qd_push,&pcw,qd_push_sz); }   // --qdump+ record
                if(!fg_product) vkCmdDispatch(cmdBridge,(WW_warp+7)/8,(WH_warp+7)/8,1);   // dispatch the warp over the SCALED wapOutA extent (one 8×8 workgroup per scaled output tile; the shader's imageSize(u_output) valid-test bounds it). warp_div==1 ⇒ == (WW+7)/8,(WH+7)/8 (byte-identical).
                // ── R3: the fg_core.comp kernel. --fg-core: the product dispatch (wapOutA is its output; the legacy
                // bind/push/dispatch above were skipped). --fg-core-ab: it runs BESIDE the legacy warp into fgOutA from the
                // SAME descriptor inputs, then fg_ab_diff counts the differing pixels (the M4 instrument; running totals).
                // The push = CorePush (the six contract params + t + arm_mask) + the gme gen-scalars. arm_mask is derived
                // HERE, once per tick, from the same per-generation validity the legacy gates use (XR5): gme_push = the
                // model is valid this generation; bwd_push = the bwd field is valid this generation.
                if(fg_on){
                    static uint64_t ab_light_skips=0; static uint32_t ab_ticks=0;
                    const pfg::layers::FgPush fgp{ { cfg.res_ceil, cfg.conf_improv, cfg.agreement, t, pfg::layers::layer_arm_mask(pfg::layers::ArmInputs{ gme_push, bwd_push, matte_push, appear_push, cfg.commit_thresh > 0.f }) },
                                                   { gme_push?gme6[0]:0.f, gme_push?gme6[1]:0.f, gme_push?gme6[2]:0.f, gme_push?gme6[3]:0.f, gme_push?gme6[4]:0.f, gme_push?gme6[5]:0.f } };
                    vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,fgPipeA.pipe);
                    vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,fgPipeA.layout,0,1,&fgPipeA.set,0,nullptr);
                    vkCmdPushConstants(cmdBridge,fgPipeA.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(fgp),&fgp);
                    vkCmdDispatch(cmdBridge,(WW_warp+7)/8,(WH_warp+7)/8,1);
                    if(cfg.fg_core_ab && abPipeA.pipe!=VK_NULL_HANDLE){
                        if(warp_light){ ++ab_light_skips; }   // the governor shed vblend/band-xfade in the legacy push this tick; fg_core cannot shed a spec constant -> not compared (declared deviation)
                        else {
                            // both outputs written (SHADER_WRITE, GENERAL) -> the diff pass reads them (imageLoad).
                            VkMemoryBarrier mba{}; mba.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                            mba.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; mba.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
                            vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&mba,0,nullptr,0,nullptr);
                            vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,abPipeA.pipe);
                            vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,abPipeA.layout,0,1,&abPipeA.set,0,nullptr);
                            vkCmdDispatch(cmdBridge,(WW_warp+7)/8,(WH_warp+7)/8,1);
                            // the running totals -> the host copy (the devMass -> hMass_a pattern).
                            VkBufferMemoryBarrier bb{}; bb.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                            bb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; bb.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                            bb.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; bb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                            bb.buffer=devAb.buf; bb.offset=0; bb.size=VK_WHOLE_SIZE;
                            vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&bb,0,nullptr);
                            VkBufferCopy bc{}; bc.srcOffset=0; bc.dstOffset=0; bc.size=kAbStatsBytes; vkCmdCopyBuffer(cmdBridge,devAb.buf,hAb_a.buf,1,&bc);
                            bb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; bb.dstAccessMask=VK_ACCESS_HOST_READ_BIT; bb.buffer=hAb_a.buf;
                            vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&bb,0,nullptr);
                        }
                        if(hostAb && (++ab_ticks % 240u)==0u){ const uint32_t* s=(const uint32_t*)hostAb;   // ~1 Hz at 240 ticks/s; totals lag <= one tick
                            std::printf("[fg-core-ab] compared=%u diff_px=%u max_delta=%u sum_delta=%u light_skips=%llu\n",s[0],s[1],s[2],s[3],(unsigned long long)ab_light_skips);
                            // the evidence list: print the entries that appeared since the last print (position, both rgba, tick)
                            static uint32_t ab_printed=0; const uint32_t n=s[4]<256u?s[4]:256u;
                            for(uint32_t i=ab_printed;i<n;++i){ const uint32_t* e=s+5+i*4;
                                std::printf("[fg-core-ab]   px(%u,%u) legacy=%02X%02X%02X%02X fg_core=%02X%02X%02X%02X tick=%u\n",e[0]&0xFFFFu,e[0]>>16,
                                            e[1]&0xFFu,(e[1]>>8)&0xFFu,(e[1]>>16)&0xFFu,e[1]>>24,e[2]&0xFFu,(e[2]>>8)&0xFFu,(e[2]>>16)&0xFFu,e[2]>>24,e[3]); }
                            ab_printed=n; }
                    }
                }
                // the field VISUALIZER pass — A reads the iGPU contour field (wapFIELDA,
                // GENERAL, uploaded this pair in wap_upload) and tints the boundary band onto wapOutA IN-PLACE
                // (per-pixel, race-free — no warp-neighbour read). Lives in THIS cmdBridge / A.q submit, AFTER
                // the warp dispatch (wapOutA in GENERAL) and BEFORE the GENERAL→TRANSFER_SRC blit barrier — no
                // host bounce, no extra submit. The COMPUTE→COMPUTE VkMemoryBarrier makes the warp's wapOutA
                // SHADER_WRITE visible to the fill's SHADER_READ|SHADER_WRITE (and wapFIELDA readable). --afill
                // only; off → byte-identical (no barrier, no bind, no dispatch). fill_strength==0 / interior
                // pixels also no-op inside the shader (the off-proof), but the gate here keeps the off path clean.
                if(cfg.afill){
                    VkMemoryBarrier mbf{}; mbf.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    mbf.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; mbf.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
                    vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&mbf,0,nullptr,0,nullptr);
                    vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,fillPipeA.pipe);
                    vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,fillPipeA.layout,0,1,&fillPipeA.set,0,nullptr);
                    // --afill writes IN-PLACE onto wapOutA, so its pcf.w/h AND dispatch
                    // dims MUST use the SCALED extent (the same domain the warp just wrote). warp_div==1 ⇒
                    // == WW,WH / (WW+7)/8,(WH+7)/8 (byte-identical). --afill is itself default-off.
                    // HONEST LIMIT (bounded, default-off×default-off) the fill shader samples the contour
                    // field (wapFIELDA, kept FULL-RES WW×WH — it is an iGPU-uploaded game-derived field, NOT
                    // scaled, per the no-game-cap rule) by an INTEGER coord in pc.w/pc.h units.
                    // At --warp-scale N>1 that reads only the top-left WW/N×WH/N corner of the field → a
                    // MISALIGNED tint. --afill is a diagnostic VISUALIZER; scaling/uv-normalizing the field is
                    // a follow-up if the two flags are ever stacked. The synthesis path (the warp itself) is
                    // unaffected — this caveat is the --afill overlay only.
                    struct{uint32_t w,h;float fs,en,t,mv_gate;}pcf{(uint32_t)WW_warp,(uint32_t)WH_warp,cfg.afill_strength,cfg.afill_edge_norm,t,cfg.afill_mv_gate};
                    vkCmdPushConstants(cmdBridge,fillPipeA.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcf),&pcf);
                    vkCmdDispatch(cmdBridge,(WW_warp+7)/8,(WH_warp+7)/8,1);
                }
                // FPS-OVERLAY en el path WAP (re-home; el gap lo identificó Oleksi/chlmateus): RMW del
                // contador "in->out" sobre wapOutA IN-PLACE — el MISMO patrón in-place seguro de --afill
                // (el DEVICE_LOST histórico era el stamp sobre la imagen bridge COMPARTIDA, no un compute
                // sobre wapOutA). DESPUÉS del warp (+afill), ANTES del barrier GENERAL→TRANSFER_SRC del
                // blit — el blit existente acarrea el overlay al bridge gratis y lo escala con él. En los
                // ticks fdrop/async-drop no se graba: el front re-mostrado conserva el último contador
                // (staleness ≤ una ventana de stats, aceptable). OFF → cero barrier/bind/dispatch.
                if(cfg.fps_overlay && ov_ready_wap){
                    VkMemoryBarrier mbo{}; mbo.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    mbo.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; mbo.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
                    vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&mbo,0,nullptr,0,nullptr);
                    vkCmdBindPipeline(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,ov_pipeline);
                    vkCmdBindDescriptorSets(cmdBridge,VK_PIPELINE_BIND_POINT_COMPUTE,ov_pl_layout,0,1,&ov_set_wap,0,nullptr);
                    OverlayFpsPC ovpc{ g_ov_in.load(), g_ov_out.load(), (uint32_t)WW_warp, (uint32_t)WH_warp };
                    vkCmdPushConstants(cmdBridge,ov_pl_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,(uint32_t)sizeof(ovpc),&ovpc);
                    vkCmdDispatch(cmdBridge,(kOverlayW+7u)/8u,(kOverlayH+7u)/8u,1u);
                }
                // copy the device-local mass counter → the host-coherent buffer so P
                // reads the result from hostMassPtr after the fence (the SAME read site as before). Barrier
                // the shader's atomicAdd writes (SHADER_WRITE) → the copy read (TRANSFER_READ), then the copy
                // write → HOST_READ on the imported host buffer. One tiny 4-byte DMA per tick — the PCIe
                // traffic is now ONE coalesced transfer, not one atomic per workgroup (the regression's cause).
                { VkBufferMemoryBarrier bb{}; bb.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                  bb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; bb.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                  bb.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; bb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                  bb.buffer=devMass.buf; bb.offset=0; bb.size=VK_WHOLE_SIZE;
                  vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&bb,0,nullptr); }
                { VkBufferCopy bc{}; bc.srcOffset=0; bc.dstOffset=0; bc.size=sizeof(uint32_t);
                  vkCmdCopyBuffer(cmdBridge,devMass.buf,hMass_a.buf,1,&bc); }
                { VkBufferMemoryBarrier bb{}; bb.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                  bb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; bb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                  bb.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; bb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                  bb.buffer=hMass_a.buf; bb.offset=0; bb.size=VK_WHOLE_SIZE;
                  vkCmdPipelineBarrier(cmdBridge,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&bb,0,nullptr); }
                // ── STAGE 5 output: warp -> blit -> bridge. Two ways to record the SAME three barriers.
                // The seam derives them (proven field-identical in tests/seam check [10]); the hand-written
                // path below is the reference and the fallback. --sg-barriers picks the derived one.
                if(sg5_want && !sg5_failed){
                    if(!sg5_ready){
                        // wapOutA LIVES in GENERAL (the next tick's warp writes it there) -> a declared resting
                        // layout. bridge_img is imported and its contents are DISCARDABLE (the blit overwrites
                        // the whole image) -> UNDEFINED as its import layout.
                        sg5_out    = sg5.declare_image("wapOutA");
                        sg5_bridge = sg5.declare_image("bridge_img", true, VK_IMAGE_LAYOUT_UNDEFINED);
                        sg5.set_resting(sg5_out, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
                        sg5.add_pass("warp", {}, { pfg::seam::Access{ sg5_out, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL } });
                        sg5.add_pass_dominating("blit",
                            { pfg::seam::Access{ sg5_out,    VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL } },
                            { pfg::seam::Access{ sg5_bridge, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL } },
                        "the blit overwrites the WHOLE bridge image every tick: its prior contents are discardable (that is why the import layout is UNDEFINED)",
                            [&](VkCommandBuffer c){ VkImageBlit bl{}; bl.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; bl.dstSubresource=bl.srcSubresource;
                                bl.srcOffsets[1]={(int)WW_warp,(int)WH_warp,1}; bl.dstOffsets[1]={(int)bridge_w,(int)bridge_h,1};
                                vkCmdBlitImage(c,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,bridge_img.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bl,VK_FILTER_LINEAR); });
                        sg5.mark_output(sg5_bridge);
                        sg5_c = sg5.compile();
                        if(!sg5_c.errors.empty()){
                            for(const auto& e : sg5_c.errors) std::printf("[ra] SG-ERROR: %s\n", e.c_str());
                            sg5_failed=true;
                        } else { sg5_ready=true; }
                    }
                    if(sg5_ready){
                        sg5.bind_image(sg5_out, wapOutA.img); sg5.bind_image(sg5_bridge, bridge_img.img);
                        if(cfg.sg_dump && !sg5_dumped){ sg5_dumped=true;
                            std::printf("---- BEGIN --sg-dump (stage 5) ----\n%s%s---- END --sg-dump (%zu epilogue) ----\n",
                                sg5.dump(sg5_c).c_str(), sg5.dump_warnings(sg5_c).c_str(), sg5_c.epilogue.size()); }
                        sg5.execute(cmdBridge, sg5_c);
                    }
                }
                if(!sg5_want || sg5_failed || !sg5_ready){
                    img_barrier(cmdBridge,wapOutA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    img_barrier(cmdBridge,bridge_img.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkImageBlit bl{}; bl.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; bl.dstSubresource=bl.srcSubresource; bl.srcOffsets[1]={(int)WW_warp,(int)WH_warp,1}; bl.dstOffsets[1]={(int)bridge_w,(int)bridge_h,1}; vkCmdBlitImage(cmdBridge,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,bridge_img.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bl,VK_FILTER_LINEAR); }   // src = the SCALED wapOutA (WW/N×WH/N); the EXISTING linear filter upsamples it to the bridge/present extent (no new pass). warp_div==1 ⇒ srcOffsets {WW,WH} (byte-identical). The dst (game-facing present extent) is UNCHANGED — no game downscale.
                    img_barrier(cmdBridge,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                }
                // --ts-smooth: copy THIS tick's warp output (wapOutA) → the prev-output history
                // (wapPrevOutA) so NEXT tick's warp samples it (binding 13). This runs AFTER the blend (which is
                // inside THIS tick's warp dispatch above — it already read the OLD wapPrevOutA, populated by the
                // PREVIOUS tick's copy) and AFTER the blit (which read wapOutA), so the ordering is correct: the
                // warp read last tick's prev, then we refresh prev to this tick's output. Layouts mirror the
                // outdump copy: wapOutA GENERAL→TRANSFER_SRC→GENERAL; wapPrevOutA SHADER_READ_ONLY→
                // TRANSFER_DST→SHADER_READ_ONLY. Gated on cfg.ts_smooth>0 → off = no copy, wapPrevOutA not created
                // → BYTE-IDENTICAL. One full-res RGBA8 device-local image copy per tick (no host bounce).
                if(cfg.ts_smooth>0.0f && wapPrevOutA.img){
                    img_barrier(cmdBridge,wapOutA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    img_barrier(cmdBridge,wapPrevOutA.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkImageCopy ic{}; ic.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; ic.dstSubresource=ic.srcSubresource; ic.extent={(uint32_t)WW_warp,(uint32_t)WH_warp,1};   // copy the SCALED wapOutA → the (same-scaled) wapPrevOutA. warp_div==1 ⇒ {WW,WH} (byte-identical).
                      vkCmdCopyImage(cmdBridge,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,wapPrevOutA.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&ic); }
                    img_barrier(cmdBridge,wapPrevOutA.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(cmdBridge,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                }
                pres.timing_end(cmdBridge,tk.back);   // --warp-timing (R4b): the GPU timestamp after the blit (bottom of pipe); no-op unless armed
                pres.tdr_maybe(cmdBridge);   // --tdr-test (R4/G-R4): the forced GPU hang, recorded once when armed and due; a no-op otherwise
                vkEndCommandBuffer(cmdBridge); vkResetFences(A.dev,1,&fBridge);
                // rec = the CPU cmd-record window just closed (reset→…→end). gpu starts at submit.
                wsub_gpu0 = cfg.wsub ? now_ms() : 0.0;
                if(cfg.wsub){ const double rec=wsub_gpu0-wsub_rec0; w_rec_ema=w_rec_ema>0.0?w_rec_ema*0.8+rec*0.2:rec; }
                VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&cmdBridge;
                const uint64_t key0=0; const uint32_t kt=8;
                VkWin32KeyedMutexAcquireReleaseInfoKHR km{}; km.sType=VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR;
                if(bridge_use_km){ km.acquireCount=1; km.pAcquireSyncs=&bridge_mem; km.pAcquireKeys=&key0; km.pAcquireTimeouts=&kt;
                                   km.releaseCount=1; km.pReleaseSyncs=&bridge_mem; km.pReleaseKeys=&key0; si.pNext=&km; }
                // --upload-xfer: when xfer_on, the warp's A.q submit WAITS semUpTL>=xfer_U (the
                // last-uploaded value — the warp never samples a half-written image) at the COMPUTE_SHADER
                // stage (the wap warp is a compute dispatch that samples wapPrevA/etc.) and SIGNALS semWarpTL=
                // ++xfer_W (the WAR back-edge the NEXT upload waits on). The timeline struct is CHAINED into
                // si.pNext (preserving the keyed-mutex struct if present — both coexist in the chain). The wait
                // value xfer_U is BACKWARD-looking (a prior upload, already submitted), so it cannot block forever
                // This runs ONLY in the record_this_tick branch; on a --fdrop/async DROP no warp submits,
                // xfer_W is NOT advanced, and the next upload's WAR wait lands on the last REAL warp value.
                uint64_t warpWait=0, warpSig=0; VkPipelineStageFlags warpWaitStage=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                VkTimelineSemaphoreSubmitInfo wtssi{}; wtssi.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                if(xfer_on){
                    ++xfer_W; warpWait=xfer_U; warpSig=xfer_W;
                    wtssi.waitSemaphoreValueCount=1; wtssi.pWaitSemaphoreValues=&warpWait;
                    wtssi.signalSemaphoreValueCount=1; wtssi.pSignalSemaphoreValues=&warpSig;
                    wtssi.pNext=si.pNext; si.pNext=&wtssi;   // chain ahead of the keyed-mutex struct (both stay in the chain)
                    si.waitSemaphoreCount=1; si.pWaitSemaphores=&A.semUpTL; si.pWaitDstStageMask=&warpWaitStage;
                    si.signalSemaphoreCount=1; si.pSignalSemaphores=&A.semWarpTL;
                }
                pres.submit(tk,si);   // R4 (stage 6): sync = submit + TDR-catching wait; async = submit + mark the slot in flight
                } // end if(record_this_tick)
                // gpu = submit + the BLOCKING fence wait (the warp dispatch + blit ran on A.q in
                // this segment — if (c) dominates, the shader stack is the cost; bisect with --no-*).
                const double wsub_prs0 = cfg.wsub ? now_ms() : 0.0;
                // gate on record_this_tick: wsub_gpu0 is stamped ONLY inside the record block — on a
                // skip tick (async warp in flight / rfp / fdrop) it holds 0.0 and gpu = now−0 =
                // absolute uptime, poisoning the EMA to millions of ms (measured: wsub gpu:7654321).
                if(cfg.wsub && record_this_tick){ const double gpu=wsub_prs0-wsub_gpu0; w_gpu_ema=w_gpu_ema>0.0?w_gpu_ema*0.8+gpu*0.2:gpu; }
                // read the presented matte mass AFTER the fence — the dispatch + the
                // device→host copy are both complete, so the host-coherent buffer reflects the per-dispatch
                // device-local count (reset on-GPU by the fill, accumulated by the shader, copied back here).
                // No extra GPU sync, no stall (we already wait fBridge for the present). When matte was off
                // this dispatch the shader added nothing → the fill-reset 0 is copied → presented mass 0
                // (feedback inert, as intended). bridge_present runs AFTER the read; order is irrelevant (the
                // read touches only the host copy buffer, the present touches only the bridge image).
                // on the async path this read already happened in the preamble (at fence completion);
                // doing it here would overwrite *presented_out with possibly-incomplete data → gate on !ap.
                if(!ap){ if(presented_out) *presented_out = hostMassPtr ? *(uint32_t*)hostMassPtr : 0u; }
                // instrument: read back THIS tick's warp output (the synthesis plane) — which
                // pixels/layers paint the artifacts, at which phase. Diagnostic-only (--outdump N); the
                // oneshot stalls the tick it runs on (irrelevant for a dump run). wapOutA is GENERAL here.
                // the outdump/qdump taps read wapOutA AFTER the fence; on the async path there is no
                // fence wait here and the warp may be incomplete, so gate both diagnostics on !ap (they are
                // default-off; an async run that wants a dump uses the sync path).
                if(!ap && outdump_left>0 && hostOutD){
                    oneshot(A,[&](VkCommandBuffer c){
                        img_barrier(c,wapOutA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                        VkBufferImageCopy cp=full_bic(WW_warp,WH_warp); vkCmdCopyImageToBuffer(c,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hOutD_a.buf,1,&cp);   // read back the SCALED wapOutA (full_bic(WW,WH) would over-read it). warp_div==1 ⇒ full_bic(WW,WH). Diagnostic, default-off.
                        img_barrier(c,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                    });
                    char dp[160]; std::snprintf(dp,sizeof(dp),"frames\\out_%02d_t%03d.bmp",outdump_idx,(int)(t*100.f+0.5f));
                    dump_bmp(dp,(const uint8_t*)hostOutD,WW_warp,WH_warp);
                    ++outdump_idx; --outdump_left;
                    if(outdump_left==0) std::printf("[ra] outdump: complete — %d frames\n",outdump_idx);
                }
                // layer 1 (--qdump <dir> N) the FG-quality TEST-FIELD data tap. GATED on qdump_left>0
                // (and all the allocs above were qdump_n-gated) → when --qdump is absent this WHOLE block plus
                // its buffers do not exist → byte-identical when off (the default-off discipline).
                // SAMPLING: increment qdump_tick every tick, dump only every kQdumpStride-th → the N triples
                // SPAN the run instead of clustering in the first N ticks. At the dump tick all three warp
                // images are LIVE: wapPrevA (real N) + wapCurA (real N+1) are in SHADER_READ_ONLY_OPTIMAL
                // (span CORRECTED 2026-09-04: MEASURED N+1, not N+2, by decoding the motion-truth
                //  barcode from these very planes -- step 1 on 12 of 12 triples, records/S2_T3_GATE.md.
                //  "N+2" survived from the HELD-OUT design, where the middle real is withheld as truth;
                //  this tap is truth-less and holds nothing out, so the pair is P's own working pair.)
                // (the wap_upload upload state); wapOutA (the live FG output for THIS
                // phase t) is GENERAL (just synthesised). One oneshot per image MIRRORS --outdump's proven
                // barrier+CopyImageToBuffer EXACTLY — only the OUT image uses GENERAL endpoints; the two
                // anchors use SHADER_READ_ONLY_OPTIMAL endpoints (preserving the layout the next warp/upload
                // expects). The oneshot stalls this tick (irrelevant for a dump run, like --outdump).
                if(!ap && qdump_left>0 && hostOutD && hostPrevD && hostCurD){
                    // classify THIS tick, then decide. seen[] is updated on every tick (dumped or not) so the
                    // minimum below ranges over the bins/slots this configuration actually reaches.
                    int qd_b=(int)(t*(float)kQdBins); if(qd_b<0) qd_b=0; if(qd_b>=kQdBins) qd_b=kQdBins-1;
                    const int qd_g=(qd_gen>=0&&qd_gen<kGenRing)?qd_gen:0;
                    qd_bin_seen[qd_b]=true; qd_gen_seen[qd_g]=true;
                    uint32_t qd_bmin=0xFFFFFFFFu, qd_gmin=0xFFFFFFFFu;
                    for(int _i=0;_i<kQdBins;++_i)  if(qd_bin_seen[_i]&&qd_bin_hits[_i]<qd_bmin) qd_bmin=qd_bin_hits[_i];
                    for(int _i=0;_i<kGenRing;++_i) if(qd_gen_seen[_i]&&qd_gen_hits[_i]<qd_gmin) qd_gmin=qd_gen_hits[_i];
                    const bool qd_gap_ok = !qd_any || (qdump_tick-qd_last_tick)>=kQdumpMinGap;
                    const bool qd_want   = qd_gap_ok && qd_bin_hits[qd_b]==qd_bmin
                                           && (qd_gen_hits[qd_g]==qd_gmin || qd_skip>=kQdumpStarve);
                    if(qd_want){
                        oneshot(A,[&](VkCommandBuffer c){
                            // wapOutA: GENERAL → TRANSFER_SRC → GENERAL (identical to --outdump).
                            img_barrier(c,wapOutA.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                            { VkBufferImageCopy cp=full_bic(WW_warp,WH_warp); vkCmdCopyImageToBuffer(c,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hOutD_a.buf,1,&cp); }   // the OUT image is the SCALED wapOutA; the pair-real anchors below stay full_bic(WW,WH) (they are NOT scaled — no game cap). warp_div==1 ⇒ full_bic(WW,WH).
                            img_barrier(c,wapOutA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);
                            // wapPrevA / wapCurA: SHADER_READ_ONLY_OPTIMAL → TRANSFER_SRC → SHADER_READ_ONLY_OPTIMAL
                            // (these anchors are sampled by the warp, never GENERAL — preserve the RO state).
                            img_barrier(c,wapPrevA.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                            { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(c,wapPrevA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hPrevD_a.buf,1,&cp); }
                            img_barrier(c,wapPrevA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                            img_barrier(c,wapCurA.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                            { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(c,wapCurA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hCurD_a.buf,1,&cp); }
                            img_barrier(c,wapCurA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                            // wapMVA: the MV the warp SAMPLED -- after the consensus pass rewrote it on the GPU.
                            // Same RO->TRANSFER_SRC->RO pair as the anchors (it rests RO after the pass, :833).
                            if(hostMV1){
                                img_barrier(c,wapMVA.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                                { VkBufferImageCopy cp=full_bic(wap_mvw,wap_mvh); vkCmdCopyImageToBuffer(c,wapMVA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hMV1_a.buf,1,&cp); }
                                img_barrier(c,wapMVA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                            }
                            // wapMVBA: the BACKWARD field, also rewritten by the pass (present.cpp median_filter on
                            // set_mvb when bidir). Only bound/uploaded under use_bidir; same barrier pair.
                            if(hostMVB1 && use_bidir){
                                img_barrier(c,wapMVBA.img,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                                { VkBufferImageCopy cp=full_bic(wap_mvw,wap_mvh); vkCmdCopyImageToBuffer(c,wapMVBA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hMVB1_a.buf,1,&cp); }
                                img_barrier(c,wapMVBA.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
                            }
                        });
                        // Write the three RAW RGBA8 planes (NO channel swap — the source is RGBA8; see dump_rgba).
                        char qp[320];
                        std::snprintf(qp,sizeof(qp),"%s\\q%06d_prev.rgba",cfg.qdump_dir,qdump_idx); dump_rgba(qp,(const uint8_t*)hostPrevD,WW,WH);
                        std::snprintf(qp,sizeof(qp),"%s\\q%06d_live.rgba",cfg.qdump_dir,qdump_idx); dump_rgba(qp,(const uint8_t*)hostOutD ,WW_warp,WH_warp);   // the live FG output is the SCALED wapOutA; prev/next (pair-reals) stay WW×WH. warp_div==1 ⇒ WW,WH.
                        std::snprintf(qp,sizeof(qp),"%s\\q%06d_next.rgba",cfg.qdump_dir,qdump_idx); dump_rgba(qp,(const uint8_t*)hostCurD ,WW,WH);
                        // Append a TRUTH-LESS manifest line (NO mid= — live FG has no held-out ground truth; the
                        // fg_quality_scorer's truth-less mode scores crossfade-only). Write the `size W H` header
                        // ONCE, on the first dump (manifest opened in "w" then switched to "a" via the flag).
                        // --qdump+ (S2.T1) the REPLAY RECORD: the triple alone cannot be replayed. Dump the fields the
                        // warp actually consumed for THIS tick — the MV and SAD planes of the uploaded generation, and
                        // the push block exactly as submitted. Host pointers, already mapped; no new synchronisation:
                        // this block runs after the tick's fence on the sync path. RING SAFETY: we read generation
                        // qd_gen while F may be writing qd_gen+1; kGenRing=3 makes that safe by construction.
                        size_t qd_pushsz=0; uint32_t qd_mvw=0, qd_mvh=0; int qd_gv=0; const float* qd_gme=nullptr;
                        bool qd_has_mvb=false,qd_has_c2=false,qd_has_dis=false,qd_has_disb=false,qd_has_per=false,qd_has_mvt=false; bool qd_has_mv0=false; bool qd_has_mvt0=false; bool qd_has_mv1=false; bool qd_has_mvb1=false;
                        if(qd_gen>=0){
                            qd_mvw=wap_mvw; qd_mvh=wap_mvh;
                            const size_t qd_plane=(size_t)qd_mvw*qd_mvh*4u;   // RG16F = 4 bytes/texel
                            if(hostMV[qd_gen]){ std::snprintf(qp,sizeof(qp),"%s\\q%06d_mv.rg16f",cfg.qdump_dir,qdump_idx);
                                if(FILE* f=std::fopen(qp,"wb")){ std::fwrite(hostMV[qd_gen],1,qd_plane,f); std::fclose(f); } }
                            if(hostSAD[qd_gen]){ std::snprintf(qp,sizeof(qp),"%s\\q%06d_sad.rg16f",cfg.qdump_dir,qdump_idx);
                                if(FILE* f=std::fopen(qp,"wb")){ std::fwrite(hostSAD[qd_gen],1,qd_plane,f); std::fclose(f); } }
                            qd_gv=f_pair_gme_valid_a[qd_gen]; qd_gme=f_pair_gme_a[qd_gen];
                            // S2.T1c: the rest of what the DEFAULT warp reads. qd_plane is the RG16F size; the R8 masks are a
                            // quarter of it and the candidate field is double. A null pointer means the feature is off for this
                            // run, and the manifest then names the plane '-' instead of a file - the record states its own scope.
                            const size_t qd_r8=(size_t)qd_mvw*qd_mvh, qd_c2b=qd_r8*8u;
                            auto qd_write=[&](const char* suffix,const void* src,size_t n)->bool{
                                if(!src) return false;
                                std::snprintf(qp,sizeof(qp),"%s\\q%06d_%s",cfg.qdump_dir,qdump_idx,suffix);
                                if(FILE* f=std::fopen(qp,"wb")){ std::fwrite(src,1,n,f); std::fclose(f); return true; }
                                return false; };
                            qd_has_mvb =qd_write("mvb.rg16f",   hostMVB[qd_gen], qd_plane);
                            qd_has_c2  =qd_write("c2.rgba16f",  hostC2[qd_gen],  qd_c2b);
                            qd_has_dis =qd_write("dis.r8",      hostDIS[qd_gen], qd_r8);
                            qd_has_disb=qd_write("disb.r8",     hostDISB[qd_gen],qd_r8);
                            qd_has_per =qd_write("per.r8",      hostPER[qd_gen], qd_r8);
                            // u_mv_target is the MV of ANOTHER generation of the same ring (target_gen == qd_gen means the
                            // warp self-targets, and vblend's tilt is then a no-op); dump it under its own name either way so
                            // the replay never has to infer which plane binding 12 held.
                            if(qd_tgen>=0&&qd_tgen<kGenRing) qd_has_mvt=qd_write("mvt.rg16f",hostMV[qd_tgen],qd_plane);
                            qd_has_mv0=qd_mv0_ok && qd_write("mv0.rg16f",qd_mv0.data(),qd_plane);   // the upload-time snapshot
                            qd_has_mvt0=qd_mvt0_ok && qd_write("mvt0.rg16f",qd_mvt0.data(),qd_plane);   // the upload-time TARGET snapshot
                            qd_has_mv1=(hostMV1!=nullptr) && qd_write("mv1.rg16f",hostMV1,qd_plane);   // the POST-consensus MV the warp sampled
                            qd_has_mvb1=(hostMVB1!=nullptr) && use_bidir && qd_write("mvb1.rg16f",hostMVB1,qd_plane);   // ... and the backward one
                        }
                        std::snprintf(qp,sizeof(qp),"%s\\q%06d_push.bin",cfg.qdump_dir,qdump_idx);
                        if(qd_push_sz){ if(FILE* f=std::fopen(qp,"wb")){ qd_pushsz=qd_push_sz; std::fwrite(qd_push,1,qd_pushsz,f); std::fclose(f); } }
                        // A written plane is named by its file; an absent one is named '-'. Six small buffers so every name
                        // is alive for the single fprintf below (snprintf into qp would be overwritten by the next call).
                        char qb0[48],qb1[48],qb2[48],qb3[48],qb4[48],qb5[48],qb6[48],qb7[48],qb8[48],qb9[48],qbab[64];
                        if(cfg.fg_core_ab&&hostAb){ const uint32_t* s=(const uint32_t*)hostAb; std::snprintf(qbab,sizeof(qbab),"%u/%u/%u",s[0],s[1],s[2]); } else std::snprintf(qbab,sizeof(qbab),"-");   // compared/diff_px/max_delta
                        auto qd_nm=[&](bool have,char* buf,const char* suffix)->const char*{
                            if(!have){ buf[0]='-'; buf[1]='\0'; return buf; }
                            std::snprintf(buf,48,"q%06d_%s",qdump_idx,suffix); return buf; };
                        std::snprintf(qp,sizeof(qp),"%s\\manifest.txt",cfg.qdump_dir);
                        FILE* mf=std::fopen(qp,qdump_man_open?"ab":"wb");
                        if(mf){
                            if(!qdump_man_open){
                                std::fprintf(mf,"# qdump (--qdump) HSR FG-quality test-field — TRUTH-LESS held-out triples (live FG, no mid=)\n");
                                std::fprintf(mf,"# sampler: coverage (S2.T1b) - least-covered phase bin of %d, ring slot of %d;"
                                                " min gap %u ticks, slot condition dropped after %u skips\n",
                                                kQdBins,kGenRing,kQdumpMinGap,kQdumpStarve);
                                std::fprintf(mf,"size %u %u\n",WW,WH);
                                // at --warp-scale N>1 the `live` plane is the SCALED warp output
                                // (WW/N×WH/N), while prev/next (pair-reals) stay WW×WH. Annotate the divisor so the
                                // scorer knows to upscale `live` before comparison. warp_div==1 ⇒ live_div 1
                                // (no line emitted) → byte-identical manifest.
                                if(warp_div>1u) std::fprintf(mf,"live_div %u %u %u\n",warp_div,WW_warp,WH_warp);
                                qdump_man_open=true;
                            }
                            // --qdump+ : the sidecar tokens are APPENDED. The scorer's manifest parser ignores trailing
                            // tokens (its README states so; re-verified by running Mode T on a + manifest), so an older
                            // consumer still reads the triple exactly as before.
                            std::fprintf(mf,"triple q%06d prev=q%06d_prev.rgba next=q%06d_next.rgba live=q%06d_live.rgba t=%.4f"
                                            " gen=%d mvw=%u mvh=%u mv=q%06d_mv.rg16f sad=q%06d_sad.rg16f push=q%06d_push.bin pushsz=%zu"
                                            " gme_valid=%d gme=%.9g,%.9g,%.9g,%.9g,%.9g,%.9g"
                                            " tgen=%d mvb=%s c2=%s dis=%s disb=%s per=%s mvt=%s mv0=%s mvt0=%s mv1=%s mvb1=%s core=%s contract=0x%016llX ab=%s\n",
                                qdump_idx,qdump_idx,qdump_idx,qdump_idx,t,
                                qd_gen,qd_mvw,qd_mvh,qdump_idx,qdump_idx,qdump_idx,qd_pushsz,
                                qd_gv,
                                qd_gme?(double)qd_gme[0]:0.0,qd_gme?(double)qd_gme[1]:0.0,qd_gme?(double)qd_gme[2]:0.0,
                                qd_gme?(double)qd_gme[3]:0.0,qd_gme?(double)qd_gme[4]:0.0,qd_gme?(double)qd_gme[5]:0.0,
                                qd_tgen,
                                qd_nm(qd_has_mvb,qb0,"mvb.rg16f"),  qd_nm(qd_has_c2,qb1,"c2.rgba16f"),
                                qd_nm(qd_has_dis,qb2,"dis.r8"),     qd_nm(qd_has_disb,qb3,"disb.r8"),
                                qd_nm(qd_has_per,qb4,"per.r8"),     qd_nm(qd_has_mvt,qb5,"mvt.rg16f"),
                                qd_nm(qd_has_mv0,qb6,"mv0.rg16f"),
                                qd_nm(qd_has_mvt0,qb7,"mvt0.rg16f"),
                                qd_nm(qd_has_mv1,qb8,"mv1.rg16f"),
                                qd_nm(qd_has_mvb1,qb9,"mvb1.rg16f"),
                                // R3: which kernel produced `live`, the layer contract hash, and the --fg-core-ab totals so far
                                (cfg.fg_core&&fgPipeA.pipe!=VK_NULL_HANDLE)?"fg_core":"wap_warp",(unsigned long long)pfg::layers::layer_contract_hash(cfg),qbab);
                            std::fclose(mf);
                        }
                        ++qd_bin_hits[qd_b]; ++qd_gen_hits[qd_g];   // this bin/slot is now covered - ineligible until the rest catch up
                        qd_last_tick=qdump_tick; qd_any=true; qd_skip=0;
                        ++qdump_idx; --qdump_left;
                        if(qdump_left==0) std::printf("[ra] qdump: wrote %d triples to %s\n",qdump_idx,cfg.qdump_dir);
                    }
                    else ++qd_skip;
                    ++qdump_tick;
                }
                // ── --shallow-queue: BOUNDED early-promote of THIS tick's warp ──────────────────
                // Off → cfg.shallow_queue false → short-circuit, ZERO new work → the present tail below shows the
                // OLD async_front (byte-identical one-behind). On a HEADROOM tick poll the just-submitted fence with
                // a HARD wall-clock cap (sq_budget_us); if it completes in budget, promote it to async_front for THIS
                // tick's present (depth ~1→~0). NEVER vkWaitForFences — vkGetFenceStatus + a capped spin (the HAL
                // spin_hint, not a raw PAUSE) → it can NOT become the stall. Guard `record_this_tick &&
                // async_inflight==back`: only ever this tick's freshly-submitted slot (a --fdrop drop tick records
                // nothing → async_inflight keeps a prior value → the guard is false → no foreign-slot touch). On a
                // hit the mass-read + async_front swap MIRROR the preamble (the fence-poll promote) one tick early,
                // and clearing async_inflight=-1 forbids the preamble re-promoting it next tick (no double-present).
                // The content-order key (last_pres_*) is maintained by the SAME post-lambda bookkeeping for hit and
                // miss alike — the warp CONTENT is identical, only its present TIMING moves — so no extra key write.
                pres.shallow_queue(tk,presented_out);   // R4 (stage 6): --shallow-queue early promote, stage-owned
                pres.present_tick(tk);   // R4 (stage 6): sync -> present_front (slot 0); async -> the completed front slot
                // prs = the present pillar (bridge_present / dcomp) + the sub-µs host mass-read +
                // the (off unless --outdump) instrument. If (e)/prs dominates, the suspect is the present
                // path, not the shader. Measured from the fence wait's end to here.
                if(cfg.wsub){ const double prs=now_ms()-wsub_prs0; w_prs_ema=w_prs_ema>0.0?w_prs_ema*0.8+prs*0.2:prs; }
            };

            // ── --real-fast-path / --rfp: present a captured REAL via the DEDICATED async slot ──
            // The CRASH-CLASS core. This is the async tail of
            // wap_warp_present, but it records a pure REAL blit (hR_a[s] → Apresent → bslot[back].img) instead of
            // the warp output. It NEVER calls do_present_P/bridge_present_src (which reset the SHARED cmdBridge/
            // fBridge → use-after-reset / device-lost with an in-flight async warp = the exact reason the
            // buffers were split). Only ever reached under cfg.real_fast_path, which IMPLIES cfg.async_present (the
            // parse auto-enable + the co-arm guard), so ap is always true here; the !ap shadow path is kept for
            // textual symmetry with wap_warp_present and is byte-inert (real_fast_path never arms without async).
            // The record reads ONLY the C→P capture ring (hR_a) + Apresent + the bslot image — none of these are
            // touched by the --upload-xfer transfer engine, so it needs NO timeline-semaphore WAR sync (it cannot
            // race the upload), and it must NOT advance xfer_W (adjacent: the next upload's WAR wait stays on
            // the last REAL warp). Lock-free: only the existing A.q submit + a non-blocking fence, no CPU mutex.
            auto rfp_present=[&](int s){
                if(!surface_ready) return;
                // Promote a completed in-flight slot exactly as the warp preamble does (the matte mass-read is
                // irrelevant to a real present → skip it).
                pres.poll_inflight(nullptr);   // R4 (stage 6): the async preamble, stage-owned (no mass read on the real path)
                // Record the real blit ONLY when a slot is free (no warp/real still in flight). If one is still
                // running we DROP this real's record (NEVER reset an in-flight buffer —) and just re-present
                // the completed front — the state machine stays consistent (mirrors the warp's record_this_tick
                // guard). With ≤1 in flight the free slot is the non-front slot.
                const pfg::present::Tick tk = pres.begin(pfg::present::Decision::Warp, /*count_drop=*/false);   // R4 (stage 6): the slot decision; a real-frame present never counts as a warp drop
                const bool record_this = tk.record;
                if(record_this){
                    VkCommandBuffer rcmd  = tk.cmd;
                    VkFence         rfen  = tk.fence;
                    VkImage         rimg  = tk.img;
                    VkDeviceMemory  rmem  = tk.mem;
                    vkResetCommandBuffer(rcmd,0);
                    VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(rcmd,&bi);
                    img_barrier(rcmd,Apresent.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkBufferImageCopy cp=full_bic(pres_w,pres_h); vkCmdCopyBufferToImage(rcmd,hR_a[s].buf,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                    img_barrier(rcmd,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    img_barrier(rcmd,rimg,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkImageBlit bl{}; bl.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; bl.dstSubresource=bl.srcSubresource; bl.srcOffsets[1]={(int)pres_w,(int)pres_h,1}; bl.dstOffsets[1]={(int)bridge_w,(int)bridge_h,1}; vkCmdBlitImage(rcmd,Apresent.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,rimg,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bl,VK_FILTER_LINEAR); }
                    vkEndCommandBuffer(rcmd); vkResetFences(A.dev,1,&rfen);
                    VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount=1; si.pCommandBuffers=&rcmd;
                    const uint64_t key0=0; const uint32_t kt=8;
                    VkWin32KeyedMutexAcquireReleaseInfoKHR km{}; km.sType=VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR;
                    if(bridge_use_km){ km.acquireCount=1; km.pAcquireSyncs=&rmem; km.pAcquireKeys=&key0; km.pAcquireTimeouts=&kt;
                                       km.releaseCount=1; km.pReleaseSyncs=&rmem; km.pReleaseKeys=&key0; si.pNext=&km; }
                    pres.submit(tk,si);   // R4 (stage 6): sync = submit + wait (dead when rfp armed: rfp implies async); async = submit + in flight
                }
                // Present the freshest COMPLETED slot (the present tail — increments total_frames; no manual bump).
                pres.present_tick(tk);   // R4 (stage 6): async -> the completed front slot (sync would be present_front; dead: rfp implies async)
            };

            // ── Thread P: paced present loop ─────────────────────────────────
            double pcost_ema=0.0;          // measured per-present work (ms) → F budget
            uint64_t last_cons_p=0,last_skips_p=0,last_cap_p=0;
            uint64_t last_bwd_skips_p=0;   // bwd-skip windowed-% denominator tracker
            uint64_t last_lap_p=0;         // F lap-escape windowed-rate tracker (lap:N/s)
            // ── the laser mass-feedback state (P-local) ───────────────────────────────────────
            // mass_err_ema = EMA (0.9) of err = (presented−expected)/max(expected,1), the mass-conservation
            // signal. thr_eff = matte_thresh·(1 + mass_k·err_ema), clamped [0.5,2]·matte_thresh. mass low
            // (presented < expected, the lerp thinned the vessel) → err<0 → thr_eff DROPS → more pixels
            // classify object → the vessel refills; mass high (spill) → thr_eff rises. mass_k=0 → thr_eff ==
            // matte_thresh always (feedback off, the clean lerp). The stat window accumulates the signed err
            // mean for the `mass:±NN%` print (the gme segment). One present per tick → these update per tick.
            double mass_err_ema=0.0; bool mass_err_init=false;
            double mass_err_sum_win=0.0; uint64_t mass_err_n_win=0;   // windowed mean for the stats print
            int dump_left=cfg.dump_n, dump_idx=0;       // diagnostic dump
            int pairdump_left=cfg.pairdump_n, pairdump_idx=0;   // instrument (frames/ created at startup if needed)
            if(cfg.pairdump_n>0) CreateDirectoryA("frames",nullptr);
            // instrument: the phase-ladder probe. For the next phaselog_left pairs, every
            // presented WAP tick's t_use lands in plog_buf (capped at kPlogCap); on pair-advance the
            // closing pair's ladder is printed and the buffer resets for the next. P-local, fixed
            // buffer, zero heap. plog_have/plog_c track the pair currently accumulating. Collection
            // begins only after kPlogWarmupMs of wall time so the D-anchor's startup settle (lat
            // 225→30ms) is excluded — the logged ladders are STEADY-STATE, not the transient.
            static constexpr int kPlogCap=40;
            static constexpr double kPlogWarmupMs=20000.0;
            const double plog_t0=now_ms();
            int   phaselog_left=cfg.phaselog_n;
            float plog_buf[kPlogCap]; int plog_n=0;     // t_use samples STORED for the pair in progress (≤kPlogCap)
            int   plog_total=0;                         // TRUE tick count of the pair (may exceed kPlogCap → ladder truncated)
            uint64_t plog_c=0; uint64_t plog_span=0; bool plog_have=false;

            // ── OUTPUT-CLOCK loop (timer — the only present loop) ───
            // The present cadence is the PANEL's: P ticks once per refresh and presents EXACTLY ONE
            // frame — the nearest pre-generated phase due at that tick (zero new GPU), or (WAP)
            // re-warps the pair at the tick's exact phase. The multiplier is a derived fraction and
            // the beat |R_panel−N·src| is structurally impossible.
            {
                const double tick_period_ms=1000.0/(double)cfg.refresh_hz;
                // C-5: name the SOURCE. This line used to assert "present cadence = the panel" for a number
                // that was hard-coded at 240 Hz and had never been read off any panel.
                const char* _hz_src = cfg.refresh_hz_set        ? "--refresh-hz override"
                                    : cfg.refresh_hz_from_panel ? "derived from the present monitor"
                                                                : "built-in default — the present monitor reported no usable refresh";
                std::printf("[ra] output-clock: timer @ %d Hz (tick %.3f ms) — %s\n",
                    cfg.refresh_hz,tick_period_ms,_hz_src);
                // ── --target-output-fps DECIMATION (v1: exact panel-rate divisors only) ──────────
                // TRUE tick decimation: the pacer keeps the panel vblank cadence; on non-selected
                // slots the tick SKIPS selection+warp+present entirely (the flip chain persists the
                // previous frame) → the FG's GPU cost scales with the OUTPUT rate. (The old s2
                // content-quantizer only re-presented duplicates while still ticking+warping at
                // panel rate — measured to return NOTHING to the game; SATURATION_PLAN.md §9.)
                // FIXED-PANEL ALIGNMENT: an arbitrary cap on a fixed-Hz panel yields uneven vblank
                // patterns (160 on 240 = 2:1:2 judder — the known non-integer-ratio frontier), so
                // v1 snaps the request to the NEAREST divisor of refresh_hz (ties prefer the LOWER
                // rate — the flag is a cost lever). VRR panels could take any rate — out of scope
                // v1. dec_every==1 (cap>=panel / off / --no-decimate) → the gate is never taken →
                // byte-identical.
                int dec_every=1;
                if(cfg.target_output_fps>0.f && cfg.decimate){
                    int bestN=1; double bestErr=1e18;
                    for(int N=16;N>=1;--N){   // descending: on an exact tie the LARGER N (lower fps) wins
                        const double err=std::fabs((double)cfg.refresh_hz/(double)N-(double)cfg.target_output_fps);
                        if(err<bestErr-1e-9){ bestErr=err; bestN=N; }
                    }
                    dec_every=bestN;
                    if(dec_every>1)
                        std::printf("[ra] decimation: requested %.0f -> snapped to %.0f (%d/%d) — present every %d-th vblank slot; the other %d skip warp+present entirely\n",
                            (double)cfg.target_output_fps,(double)cfg.refresh_hz/(double)dec_every,cfg.refresh_hz,dec_every,dec_every,dec_every-1);
                    else
                        std::printf("[ra] decimation: requested %.0f >= the nearest divisor is the panel rate itself (%d/1) — no slots skipped (cap inert)\n",
                            (double)cfg.target_output_fps,cfg.refresh_hz);
                }
                // The OLD s2 content-quantizer is active ONLY on the --no-decimate A/B path — exactly
                // one cap mechanism at a time (they'd fight: the quantizer would learn the decimated
                // present interval as the "achievable rate" and double-throttle the phase grid).
                const bool s2_quant = (cfg.target_output_fps>0.f) && !cfg.decimate;
                // Phase-selection state (P-local; reuses the f_pair_* timeline F publishes).
                // the tick maps to t_display = now − D on the CAPTURE timeline
                // (tcap shares now_ms()'s steady clock). D is the auto-calibrated pipeline
                // delay so the set whose window [tcap−span·T, tcap] contains t_display is
                // ALWAYS fully published when its ticks come due — the clock no longer chases
                // the freshest set, drains it, freezes, then jumps (the 45/45/10 defect).
                // --phase-norm: per-pair displayed-tick index for the normalized-N ladder. pe_pair =
                // the pair we are counting ticks for; pe_j = the displayed-tick index within it (0,1,2,…), placed
                // at the even grid (pe_j+0.5)/N. Reset on pair-advance. P-local, single-threaded (present thread).
                uint64_t pe_pair=0; int pe_j=0; bool pe_have=false;
                // --cphase: cross-pair carry of the velocity-magnitude proxy. TWO variables so the ratio
                // (prev-pair / this-pair speed) stays STABLE across ALL ticks of a pair (a single var would self-
                // overwrite on tick 0 and collapse the ease to one tick). P-thread-local doubles → lock-free.
                uint64_t cph_pair=0; double cph_prev_vmag=0.0, cph_cur_vmag=0.0; bool cph_have=false;
                // (own-window) yield-transition telemetry state: submit() returns SUCCESS while
                // yielded (present-nothing passthrough, no-lock-out), así que sin estas líneas una
                // corrida own-window con el juego nunca-en-foco se ve sana mostrando NADA.
                bool own_yld_prev=false, own_yld_init=false;
                // ── the CONTENT CLOCK (NCO + 2nd-order PLL), the D calibration, the set selection, the phase
                // and the content-order guard are STAGE 4: src/clock/phase_clock.{hpp,cpp} (R1/X14). The state
                // and the loop gains (--sc-freq-alpha / --sc-phase-gain / --sc-reseat) live in PhaseClock; the
                // rationale for each is in that header. What stays here is the CALLER's own gate below:
                // tr_last_cseq applies the captured arrival delta ONCE per ingest (stage-1/2 data, not the clock).
                uint64_t tr_last_cseq=0;    // (PLL units) c_seq en la última aplicación del delta a la EMA — gate una-vez-por-ingesta
                // Loop gains — named with rationale, NOT magic numbers:
                // kScFreqAlpha: EMA weight for the FREQUENCY (T_robust) per outlier-rejected arrival delta.
                // 0.05 = a ~20-sample memory: slow enough that a single jittery arrival barely moves the
                // estimate (the whole point — reject the source's per-frame jitter), fast enough to follow a
                // real rate change (e.g. the source app dropping from 30→21 fps) within ~1s at 21fps.
                // kScPhaseGain: proportional PHASE-correction fraction applied to content_clock per NEW-pair
                // arrival. 0.10 = correct 10% of the phase error each arrival → the clock locks to the
                // source over ~7-10 arrivals (a gentle slew that the eye cannot resolve), NEVER the per-pair
                // JUMP the snapping path produced. This is the classic 2nd-order-loop damping knob: small
                // enough to filter arrival jitter, large enough to stay locked under modest rate drift.
                // kScReseatErr: if the phase error exceeds this many SOURCE-FRAMES the loop has lost lock
                // (a scene cut, a long stall, or startup) — a slow slew would take seconds to recover, so we
                // RE-SEAT the clock to the expected position outright. This is the loop's acquisition path,
                // distinct from the steady-state tracking slew; it is rare and bounded, not the hot path.
                // ── STAGE 4 (CLOCK) — the unit this loop now calls (docs/planning/STAGE_CONTRACT.md).
                // Constructed once from the resolved config; the phase LAYERS (--phase-norm / s2 / --cphase)
                // still reshape its t_use below, behind their own flags.
                pfg::clock::PhaseClock clk({ tick_period_ms, NS, cap_slots, cfg.fg_factor,
                    cfg.sync_clock, cfg.sc_select, cfg.phasefix, cfg.low_d, cfg.vblend_exact, cfg.predict, cfg.asw,
                    cfg.lowd_span_frac, cfg.lowd_span_cap, (double)cfg.predict_e, (double)cfg.asw_max,
                    cfg.sc_freq_alpha, cfg.sc_phase_gain, cfg.sc_reseat_err });
                // --arrival-log (R1/X14): the clock replay oracle. Opened once, written per WAP tick,
                // closed at loop exit. Empty path -> alog==nullptr -> every write is skipped (byte-identical off).
                std::FILE* alog=nullptr; uint64_t alog_n=0;
                if(cfg.arrival_log[0]){
                    alog=std::fopen(cfg.arrival_log,"wb");
                    if(alog) std::fprintf(alog,"# phyriadfg arrival-log v1 (hex-float; the PhaseClock replay oracle)\n"
                                               "# cfg tick_period_ms=%a refresh_hz=%d NS=%d cap_slots=%d fg_factor=%d sync_clock=%d sc_select=%d phasefix=%d low_d=%d vblend_exact=%d predict=%d predict_e=%a asw=%d asw_max=%a lowd_span_frac=%a lowd_span_cap=%a freq_alpha=%a phase_gain=%a reseat_err=%a\n"
                                               "# columns: tick now_b now_d cur_c fs sc_delta_ms T_src async_ready | ring[NS]: tcap cseq span n slot | D t_display content_clock T_robust f_gen gen_back found pair_c span N_set rs tcap_r phase_global extrap t_use cand_k backwards backstep committed commit_k\n",
                        tick_period_ms,cfg.refresh_hz,NS,cap_slots,cfg.fg_factor,(int)cfg.sync_clock,(int)cfg.sc_select,(int)cfg.phasefix,(int)cfg.low_d,
                        (int)cfg.vblend_exact,(int)cfg.predict,(double)cfg.predict_e,(int)cfg.asw,(double)cfg.asw_max,(double)cfg.lowd_span_frac,(double)cfg.lowd_span_cap,
                        clk.cfg().sc_freq_alpha,clk.cfg().sc_phase_gain,clk.cfg().sc_reseat_err);
                    else std::printf("[ra] --arrival-log: cannot open %s -- continuing without it\n",cfg.arrival_log);
                }
                double tick_t0=now_ms(); uint64_t tick_k=0;     // timer schedule
                // --pace-vblank: the vblank phase-lock state, thread-P-local.
                // pv_qpc_freq = QPC ticks/sec (queried ONCE, cold). pv_last_query_ms = the steady-clock-ms instant of the
                // last DwmGetCompositionTimingInfo query → the cold cadence gate (query at most ~1/sec, NOT per tick: the
                // DWM call is an API/syscall, forbidden on the hot path). pv_locks/pv_lock_fails = DIAG counters (the
                // query-success vs unavailable rate). ALL inert with the flag off: the whole HOOK-C block below is gated on
                // cfg.pace_vblank, so neither the DWM query nor the tick_t0 phase-slew ever runs → byte-identical.
                double pv_qpc_freq=0.0; { LARGE_INTEGER f{}; if(QueryPerformanceFrequency(&f)) pv_qpc_freq=(double)f.QuadPart; }
                double pv_last_query_ms=0.0; uint64_t pv_locks=0, pv_lock_fails=0;
                uint64_t uniq_ticks=0,last_uniq=0;              // in-app uniqueness proxy
                uint64_t stat_ticks=0;                          // ticks this stat window
                double dbgD_sum=0.0,dbgPh_sum=0.0,dbgGB_sum=0.0,dbgTsrc_sum=0.0,dbgSpan_sum=0.0; uint64_t dbgN=0; // TEMP diag
                // WAP state: the pair (cseq) + gen currently uploaded to A; re-upload
                // only on pair-advance. wap_freeze/wap_warp_ema = per-tick stats (prev-slot stale
                // freezes; the warp-dispatch cost EMA reported in the startup print + stats).
                uint64_t wap_pair_c_up=0; bool wap_have_up=false;
                uint64_t wap_freeze=0,last_wap_freeze=0; double wap_warp_ema=0.0;
                uint64_t vblend_hit=0,last_vblend_hit=0;   // --vblend: per-pair lookahead-fired counter (next-fresher pair was available); printed as vbhit=N/s in the DIAG line.
                // --real-fast-path: P-local fast-path state. last_rfp_c = the last source frame (cur_c)
                // we injected a real for → fire at most ONE real per source frame (bounds the cadence cost,
                // ). rfp_presents = a diagnostic counter for the injection rate (rfp:N/s in the DIAG line). Both
                // read/written ONLY inside the guarded fast-path branch → dead off-path (byte-identical when off).
                uint64_t last_rfp_c=0; uint64_t rfp_presents=0,last_rfp_presents=0;
                uint64_t mf_presents=0,last_mf_presents=0;   // --motion-fallback: fire-rate counter (mf:N/s stat)
                // FIX 1 (--motion-fallback hysteresis) present-thread-local smoothing + Schmitt latch state. The
                // RAW per-pair dispersion (f_pair_disp_a) crossing mf_disp tick-to-tick made the engage decision
                // FLAP near the threshold (real<->warp alternation = judder). mf_disp_ema = EMA(alpha 0.2) of the
                // dispersion; mf_engaged = the latched engage state; mf_dwell = the persistence counter for the
                // Schmitt flip. All P-local (no F ordering touched); dead off (only updated inside if(cfg.motion_fallback)).
                double mf_disp_ema=0.0; bool mf_disp_ema_init=false; bool mf_engaged=false; int mf_dwell=0;
                uint64_t last_mapfb_lt=0,last_mapmiss_lt=0;   // windowed rate of the +11.6ms older-slot fallback (mapfb) + the Sleep-retry (mapmiss) — the copy-fence value gate
                // FIX (window-death) detect the captured window dying so the FG
                // exits instead of re-presenting the stale pair forever (the infinite-retry bug). Track
                // the last time F published a new generation (f_seq advance); if it stalls AND the window
                // handle is gone, the source died → quit. wd_last_ms seeds to the loop start.
                uint64_t wd_last_fseq=0; double wd_last_ms=tick_t0;
                // --pace-variance: the FSR3 variance-aware pacer state — thread-P-local
                // SMA<10> of realized present deltas + running sum/sum-sq (no concurrency). DEFAULT off → unused → byte-identical.
                double pv_ring[10]={0}; int pv_n=0, pv_idx=0; double pv_sum=0.0, pv_sumsq=0.0, pv_last=0.0;
                // --target-output-fps: the fractional output-rate controller state (thread-P-local, lock-free).
                double s2_pres_ema=0.0, s2_last_pres=0.0, s2_mult=0.0;   // realized achievable-rate EMA (ms) / last present time / held realized_mult
                uint64_t s2_opdrops=0, s2_last_opdrops=0;                // over-production drop counter (the gate-fires confirmation)
                // --pace-present: the metronomic, drift-corrected present pacer state —
                // thread-P-local. An FSR3-style SMA<10> of realized present-to-present deltas + online sum/sum-sq
                // gives a variance-DAMPED estimate of the achievable cadence, driving a SLOW, bounded slew of the
                // absolute grid anchor tick_t0 (the metronome is KEPT; only its phase drifts toward what the warp
                // can deliver). DEFAULT off → the whole block is dead → byte-identical.
                double pp_ring[10]={0}; int pp_n=0, pp_idx=0; double pp_sum=0.0, pp_sumsq=0.0, pp_last=0.0;
                // --pace-hard: the pacer state (ph_tgt / ph_w_ema / ph_held / ph_overshoot
                // last_ph_*) is hoisted to the enclosing scope ABOVE the bridge_present lambda (so the present
                // chokepoint captures it) — see its declaration there. The tick boundary below only sets ph_tgt = tgt.
                // (FG bounded-run) the run-start anchor, captured ONCE just before the present loop. Read only
                // by the default-off deadline guard near the csv push (cfg.run_max_ms != 0); when unbounded it is an
                // inert local with no effect on any presented pixel → byte-identical-off. (.)
                const double t_run_start = now_ms();
                while(!g_quit&&!g_quit_threads.load()){
                    // (FG bounded-run) the deadline guard, HOISTED here (R4b) so every path honours it — its former
                    // home inside the WAP branch left grid mode (--no-warp-at-presenter) unbounded (R4_GATE.md §4.4).
                    // Same g_quit, same counters; default-off (run_max_ms/run_max_frames = 0) → byte-identical-off.
                    if(cfg.run_max_ms && now_ms()-t_run_start >= cfg.run_max_ms) g_quit=true;
                    if(cfg.run_max_frames && total_frames.load() >= cfg.run_max_frames) g_quit=true;
                    // (own-window) log yield/re-assert TRANSITIONS — the only visible truth of whether
                    // our plane is on the panel (ps-ok counts even while yielded). Cold: prints only on change.
                    if(cfg.present_own_window && surface_ready){
                        const bool _y=ra_surface.is_yielded();
                        if(!own_yld_init || _y!=own_yld_prev){
                            own_yld_init=true; own_yld_prev=_y;
                            std::printf(_y?"[ra] own-window: plane YIELDED (foreground is neither the game nor us) -> passthrough, presents are no-ops. Focus the CAPTURED window to display.\n"
                                          :"[ra] own-window: plane DISPLAYED (game/our window in front) -> we own the panel.\n");
                        }
                    }
                    // ── 1. tick boundary ──────────────────────────────────────
                    // timer: paced_wait_P spins to the next k·tick_period target (the clock).
                    {
                        ++tick_k;
                        // RESYNC when systematically behind (per-tick work > period — the
                        // tier-2 upscale-per-tick run accumulated slip 149→8262ms because the
                        // schedule never re-seated). >4 periods late → re-anchor at now; the
                        // tick RATE honestly degrades instead of the slip stat growing forever.
                        const double tn=now_ms();
                        double tgt;
                        if(cfg.pace_variance && pv_n>=10){
                            // variance-damped moving-average target,
                            // relative to the LAST present (FSR3 form). target = SMA(present deltas) − varFactor·stddev
                            // − safetyMargin: the minus-variance term shrinks the target under jitter so a saturated/
                            // erratic GPU can't lock a slow interval then overshoot → smooths the present-interval CoV.
                            const double mean=pv_sum/(double)pv_n;
                            const double var =pv_sumsq/(double)pv_n - mean*mean;
                            const double sd  =var>0.0 ? std::sqrt(var) : 0.0;
                            double ti=mean - cfg.pv_var_factor*sd - cfg.pv_safety_ms;
                            // FLOOR = the PANEL TICK, not an absolute 0.5ms. The SMA samples this pacer's OWN
                            // tick deltas (self-referential), so the −safety/−var bias compounds window over
                            // window: with a 0.5ms floor it SPIRALED to ~1856 presents/s on a 240Hz clock
                            // (measured). The panel tick as floor kills the descent: pace-variance may stretch
                            // the interval ABOVE the tick under jitter (its documented job), never below it.
                            if(ti<tick_period_ms) ti=tick_period_ms;
                            tgt=pv_last+ti;
                            if(tn-tgt>4.0*tick_period_ms) tgt=tn;               // hitch guard (the relative anchor self-corrects)
                        } else {
                            tgt=tick_t0+(double)tick_k*tick_period_ms;          // the default fixed refresh_hz grid (byte-identical off / pace-variance warmup)
                            if(tn-tgt>4.0*tick_period_ms){ tick_t0=tn; tick_k=0; tgt=tn; if(cfg.pace_present){ pp_n=0; pp_idx=0; pp_sum=0.0; pp_sumsq=0.0; pp_last=0.0; } }   // a re-seat also resets the SMA window (old cadence gone); guarded → byte-identical off
                        }
                        paced_wait_P(tgt);   // coarse sleep + hal::cpu_wait_for_ns spin-finish
                        if(cfg.pace_present){
                            // metronomic drift correction. The else-branch grid above IS the
                            // LSFG-style metronome (successive tgt's exactly tick_period_ms apart → present MASD→0
                            // WHEN HIT). We absorb the drift between tick_period_ms and the cadence the warp can
                            // sustain by SLEWING the grid ANCHOR tick_t0 — slowly, bounded — toward where the present
                            // actually lands (this slews the anchor, never replaces the grid).
                            const double now2=now_ms();
                            if(pp_last>0.0){
                                const double d=now2-pp_last;
                                if(d>100.0){ pp_n=0; pp_idx=0; pp_sum=0.0; pp_sumsq=0.0; }   // scene-cut/hitch reset (FSR3)
                                else { if(pp_n==10){ const double e=pp_ring[pp_idx]; pp_sum-=e; pp_sumsq-=e*e; } else ++pp_n;
                                       pp_ring[pp_idx]=d; pp_sum+=d; pp_sumsq+=d*d; pp_idx=(pp_idx+1)%10; }
                                if(pp_n>=10){
                                    const double mean=pp_sum/(double)pp_n;
                                    const double var =pp_sumsq/(double)pp_n - mean*mean;
                                    const double sd  =var>0.0 ? std::sqrt(var) : 0.0;
                                    double ti_ach=mean - cfg.pp_var_factor*sd - cfg.pp_safety_ms;   // variance-damped achievable interval
                                    if(ti_ach<tick_period_ms) ti_ach=tick_period_ms;               // NEVER below the panel tick → no over-production windup
                                    double phase_err=now2 - tgt;                                   // >0 = presented late (grid ran ahead of the achievable cadence)
                                    const double fwd_cap=ti_ach - tick_period_ms;                  // only chase CONFIRMED sustainable lateness (jitter beyond this is not drift)
                                    if(phase_err> fwd_cap) phase_err= fwd_cap;
                                    const double back_cap=0.5*tick_period_ms;
                                    if(phase_err<-back_cap) phase_err=-back_cap;                   // bound a single early outlier (no snap)
                                    tick_t0 += cfg.pp_slew_gain * phase_err;                        // slew the ANCHOR (composes with the grid; period stays tick_period_ms)
                                }
                            }
                            pp_last=now2;
                        }
                        if(cfg.pace_hard && cfg.present_own_window){
                            // PUBLISH this tick's grid target for the present-side pin.
                            // The pin itself is NOT here — pinning at the tick boundary is a no-op, because
                            // paced_wait_P(tgt) above ALREADY spins to tgt (its own cpu_wait_for_ns), so by this point
                            // now_ms()>=tgt. The present-MASD jitter (~4.28ms) is NOT born at the tick boundary; it is
                            // born in the VARIABLE per-tick work W (upscale + copy + blit + the warp fence wait) that
                            // runs BETWEEN this boundary and the actual Present — so the real present lands at tgt+W
                            // with W jittering. The pin therefore lives inside bridge_present() (the single chokepoint
                            // right before ra_surface.submit→Present) it waits to tgt + budget, where budget = EMA(W) +
                            // a margin, collapsing the present-MASD when W<=budget. Here we only hand it ph_tgt = tgt.
                            ph_tgt = tgt;
                        }
                        // --pace-vblank: align the present-target grid to the ACTUAL
                        // display vblank. --pace-hard pins each frame to ph_tgt+budget, but the grid (tick_t0 + k·tick_period_ms)
                        // free-runs on steady_clock with NO phase reference to the panel scanout — correct in PERIOD, arbitrary
                        // in PHASE vs the vblank. This slews tick_t0 (the grid ORIGIN) toward the true vblank phase, so future
                        // ph_tgt land AT the panel cadence. It refines the grid PHASE only — it does NOT touch the --pace-hard
                        // pin, its budget, or its freeze-floor (those live in bridge_present()).
                        //
                        // the DwmGetCompositionTimingInfo query is an API call → COLD/periodic (~1/sec), NEVER per tick.
                        // The per-tick cost when the gate is open but the cadence has not elapsed is one steady_clock read +
                        // a compare. When the flag is OFF the whole block is dead (gated on cfg.pace_vblank) → byte-identical.
                        if(cfg.pace_vblank && cfg.pace_hard && cfg.present_own_window && pv_qpc_freq>0.0){
                            const double nowq=now_ms();
                            if(pv_last_query_ms==0.0 || nowq-pv_last_query_ms>=1000.0){   // cold cadence: at most ~once/sec
                                pv_last_query_ms=nowq;
                                DWM_TIMING_INFO ti{}; ti.cbSize=sizeof(ti);
                                if(SUCCEEDED(DwmGetCompositionTimingInfo(nullptr,&ti)) && ti.qpcRefreshPeriod>0){
                                    // EXPLICIT QPC↔steady_clock bridge: sample BOTH clocks together, ONCE, right here — never
                                    // mix epochs silently (the --pace-hard lesson). qpcVBlank is a QueryPerformanceCounter
                                    // value; now_ms() is steady_clock-ms. Convert the QPC delta (now-paired-sample to qpcVBlank)
                                    // to ms and add it to the paired steady-ms sample → vblank_ms in the GRID's own clock domain.
                                    LARGE_INTEGER qnow{}; QueryPerformanceCounter(&qnow);
                                    const double pair_ms=now_ms();                                  // the steady-ms half of the paired sample
                                    const double qpc_to_ms=1000.0/pv_qpc_freq;                      // QPC ticks → ms
                                    const double vblank_ms = pair_ms + ((double)((long long)ti.qpcVBlank - qnow.QuadPart))*qpc_to_ms;   // last vblank, in steady-clock-ms
                                    // The grid lands at tick_t0 + k·tick_period_ms. To put a grid tick AT the vblank, tick_t0 must
                                    // be ≡ vblank_ms (mod tick_period_ms). Phase error = the signed residual in (−period/2, +period/2].
                                    double phase = std::fmod(vblank_ms - tick_t0, tick_period_ms);
                                    if(phase<0.0) phase += tick_period_ms;                          // → [0, period)
                                    if(phase >  0.5*tick_period_ms) phase -= tick_period_ms;        // → (−period/2, +period/2], the nearest vblank
                                    // BOUNDED phase-lock slew: shift tick_t0 by a small fraction of the error so no single query
                                    // jumps the grid (which could trip the >4·period re-seat or the freeze-floor). The shift is
                                    // additionally hard-bounded to ±half a tick. The slew only moves the grid PHASE; the period
                                    // (tick_period_ms) is untouched, so paced_wait_P / ph_tgt are unaffected except in phase.
                                    double slew = cfg.pv_lock_gain * phase;
                                    const double slew_cap=0.5*tick_period_ms;
                                    if(slew >  slew_cap) slew =  slew_cap;
                                    if(slew < -slew_cap) slew = -slew_cap;
                                    tick_t0 += slew;                                                // takes effect from the NEXT tick's tgt = tick_t0 + k·period (the low-pass family of --pp-slew)
                                    ++pv_locks;
                                } else {
                                    // Query failed / vblank phase unavailable (no DWM composition, RDP, etc.) → leave tick_t0
                                    // UNCHANGED → degrade to plain --pace-hard. NEVER block, never adjust on a bad target.
                                    ++pv_lock_fails;
                                }
                            }
                        }
                        if(cfg.pace_variance){
                            // record the realized present delta into the SMA<10> (post-pace = the present instant).
                            const double now2=now_ms();
                            if(pv_last>0.0){
                                const double d=now2-pv_last;
                                if(d>100.0){ pv_n=0; pv_idx=0; pv_sum=0.0; pv_sumsq=0.0; }   // scene-cut/hitch reset
                                else { if(pv_n==10){ const double e=pv_ring[pv_idx]; pv_sum-=e; pv_sumsq-=e*e; } else ++pv_n;
                                       pv_ring[pv_idx]=d; pv_sum+=d; pv_sumsq+=d*d; pv_idx=(pv_idx+1)%10; }
                            }
                            pv_last=now2;
                        }
                        if(s2_quant){   // s2 path only (--no-decimate); under decimation the controller is fully inert
                            // STEP2: EMA the realized present interval = the MEASURED achievable rate (the warp/tick ceiling under saturation) → the sustain term. Slow EMA (0.05), skip hitches.
                            const double now2=now_ms();
                            if(s2_last_pres>0.0){ const double d=now2-s2_last_pres; if(d>0.5&&d<100.0) s2_pres_ema=(s2_pres_ema>0.0)?s2_pres_ema*0.95+d*0.05:d; }
                            s2_last_pres=now2;
                        }
                    }
                    if(g_quit||g_quit_threads.load()) break;
                    const double t0_p=now_ms();
                    ++stat_ticks;

                    // ── FIX (window-death) exit cleanly when the captured WINDOW dies ──────────────
                    // Bug: if the window being frame-genned is destroyed, F stops publishing
                    // (no new source) but P kept re-presenting the stale pair FOREVER (lat grows unbounded).
                    // Detect: f_seq has not advanced for >2s AND the window handle is no longer valid →
                    // the source DIED → quit. The !IsWindow gate distinguishes DEATH (exit) from a merely
                    // minimized/occluded window (IsWindow stays true → keep presenting + resume on frames).
                    // Window-capture mode only (wgc_target_hwnd set by --window); monitor capture has no hwnd.
                    if(wgc_target_hwnd){
                        const uint64_t fs_now=f_seq.load();
                        if(fs_now!=wd_last_fseq){ wd_last_fseq=fs_now; wd_last_ms=t0_p; }
                        else if((t0_p-wd_last_ms)>2000.0 && !IsWindow(wgc_target_hwnd)){
                            std::printf("[ra] captured window gone (no new frames >2s + IsWindow=false) — exiting cleanly\n");
                            g_quit_threads.store(true); g_quit=true; break;
                        }
                    }

                    // ── I-8 — 1 Hz re-check of the captured window's MONITOR (WGC window path) ─────
                    // WGC follows the window across panels by itself, so the PICTURE stays right; what
                    // goes stale is the delivery-cadence base — MinUpdateInterval was derived once from
                    // the STARTING panel's Hz and never revisited. The check is one MonitorFromWindow per
                    // second while the window stays put. The DDA half of this defect is NOT covered; the
                    // honest reason lives at wgc_recheck_window_monitor (capture/capture_init.cpp).
#ifdef _MSC_VER
                    if(wgc_target_hwnd && cfg.capture_api==CA_WGC && wgc_ctx){
                        static HMONITOR mon_cur=nullptr; static int mon_hz=0; static double mon_last=0.0;
                        if(!mon_cur) mon_cur=MonitorFromWindow(wgc_target_hwnd,MONITOR_DEFAULTTONEAREST);   // seed: no re-apply on the first tick
                        if((t0_p-mon_last)>=1000.0){ mon_last=t0_p; wgc_recheck_window_monitor(cfg,wgc_target_hwnd,wgc_ctx,mon_cur,mon_hz); }
                    }
#endif

                    // ── No async-hang watchdog here. A naive age check on async_inflight false-positives:
                    // async_inflight is read BEFORE the in-lambda preamble that clears it, so it ~always reads
                    // >=0 on a healthy --async-present run. A correct warp-wedge watchdog would need a warp-
                    // COMPLETION counter advanced in the preamble. The real failure modes ARE covered: device
                    // loss by vk_live, source death by the window watchdog above; a wedged-not-lost GPU that
                    // never recovers is rare and externally observable (the user kills it).

                    // ── 1Hz PDH refresh — sum each GPU engine's utilization per adapter ──
                    // Independent of the stat-print cadence (which is faster than 1Hz at 240/s). On each 1s
                    // boundary: collect, enumerate the wildcard instances, and accumulate each engine's value
                    // into A/B/G by matching the luid fragment embedded in the instance name. The latest sums
                    // are held in gpu{A,B,G}_pct for the stat print. PdhGetFormattedCounterArrayW sizes itself
                    // (PDH_MORE_DATA), then fills. Sub-µs amortized (once/sec); zero work the other ticks.
                    if((pdh_ok||nvml_ok) && (t0_p-pdh_last_collect)>=1000.0){
                        pdh_last_collect=t0_p;
                        if(pdh_ok && PdhCollectQueryData(pdhQuery)==ERROR_SUCCESS){
                            DWORD bufSz=0,itemCount=0;
                            PDH_STATUS st=PdhGetFormattedCounterArrayW(pdhGpuEng,PDH_FMT_DOUBLE,&bufSz,&itemCount,nullptr);
                            if(st==(PDH_STATUS)PDH_MORE_DATA && bufSz>0){
                                std::vector<unsigned char> buf(bufSz);
                                auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
                                if(PdhGetFormattedCounterArrayW(pdhGpuEng,PDH_FMT_DOUBLE,&bufSz,&itemCount,items)==ERROR_SUCCESS){
                                    double aSum=0.0,bSum=0.0,gSum=0.0;
                                    for(DWORD i=0;i<itemCount;++i){
                                        if(items[i].FmtValue.CStatus!=PDH_CSTATUS_VALID_DATA && items[i].FmtValue.CStatus!=PDH_CSTATUS_NEW_DATA) continue;
                                        const double v=items[i].FmtValue.doubleValue;
                                        // instance name → narrow lowercase for the luid-fragment match.
                                        char nm[256]; int k=0; const wchar_t* w=items[i].szName;
                                        for(;w&&*w&&k<255;++w) nm[k++]=(char)std::tolower((unsigned char)(*w<128?*w:'?'));
                                        nm[k]='\0';
                                        if(luidA_ok && std::strstr(nm,luidStrA)) aSum+=v;
                                        else if(luidB_ok && std::strstr(nm,luidStrB)) bSum+=v;
                                        else if(luidG_ok && std::strstr(nm,luidStrG)) gSum+=v;
                                    }
                                    gpuA_pct=aSum; gpuB_pct=bSum; gpuG_pct=gSum; gpu_have=true;
                                }
                            }
                        }
                        // NVML override — true NVIDIA per-GPU utilization for A and B (the PDH
                        // LUID-match above is unreliable on this rig; NVML is nvidia-smi's own source). G
                        // keeps its PDH value. When NVML is armed this is the authoritative A/B reading.
                        if(nvml_ok && nvmlUtilFn){
                            NvmlUtil u{};
                            if(nvmlDevA && nvmlUtilFn(nvmlDevA,&u)==0) gpuA_pct=(double)u.gpu;
                            if(nvmlDevB && nvmlUtilFn(nvmlDevB,&u)==0) gpuB_pct=(double)u.gpu;
                            gpu_have=true;
                            // --load-governor: publish the authoritative measured 4090 (device A)
                            // util to the F-thread's tier-5 escalation. Relaxed store — the value is advisory
                            // (a tier hysteresis input, not a synchronisation point). Only the NVML override
                            // path publishes (the PDH LUID-match above is unreliable on this rig); when NVML
                            // is not armed g_gpu_a_util stays -1 and the F-thread falls back to t_pair_ema.
                            g_gpu_a_util.store((int)gpuA_pct);
                            // P owns the governor util→tier floor — the single decode + the dwell hysteresis —
                            // and publishes g_gov_floor for the F-thread (one shared mapping, so present and flow
                            // never disagree). Raise fast, lower on dwell. load_governor off / NVML cool → floor
                            // 0 → F's max(...,0) is a no-op.
                            {
                                static int gov_floor_p = 0; static int gov_dwell_p = 0;
                                constexpr int kGovDwell = 2;   // util updates (~1Hz) held before lowering the floor
                                // util supplies the floor's DEPTH (which band); self-distress supplies the GATE.
                                const int util_floor = governor_floor_for_util((int)gpuA_pct, cfg.gov_util);
                                // ── FG-OWN-SLICE distress (the finding-#1 re-keying) ──────────────────────
                                // (a) freshage: our set-detect delay EMA trending above K_fresh × the source
                                //     period. freshage_ema_ms is one-tick-stale here (updated in step 3 below),
                                //     fine for a slow EMA. Guarded on delay_init (seeded) + T_src>0.
                                // (b) warp: the per-tick warp λ cost inflated vs its OWN calm baseline
                                //     (warp_base, a slow 0.98/0.02 EMA). Seeded + a 0.2ms noise floor.
                                // util is a CORROBORATOR only: util-high alone (healthy FG) → NO floor.
                                static double warp_base = 0.0;   // calm-cost baseline (slow EMA of wap_warp_ema)
                                static int    dist_dwell = 0;    // de-bounce the distress latch (kDistDwell)
                                constexpr int    kDistDwell = 3; // util updates held before releasing distress
                                constexpr double kFresh = 3.0;   // freshage > 3× source-period ⇒ genuinely behind
                                constexpr double kWarp  = 1.6;   // warp λ > 1.6× calm baseline ⇒ our slice squeezed
                                if(wap_warp_ema > 0.0)
                                    warp_base = (warp_base > 0.0) ? warp_base*0.98 + wap_warp_ema*0.02 : wap_warp_ema;
                                const double Tsrc_now  = src_interval_ema_ms;   // live source period (ms)
                                const bool fresh_dist  = clk.delay_init_ok() && Tsrc_now > 0.0
                                                       && clk.freshage_ema() > kFresh * Tsrc_now;
                                const bool warp_dist   = warp_base > 0.2 && wap_warp_ema > kWarp * warp_base;
                                const bool dist_now    = fresh_dist || warp_dist;
                                // latch with dwell: raise instantly, hold for kDistDwell util updates before release
                                if(dist_now)               { gov_self_distress = true;  dist_dwell = kDistDwell; }
                                else if(gov_self_distress) { if(dist_dwell>0) --dist_dwell; else gov_self_distress = false; }
                                // The floor's target: OFF-path (--gov-util-floor) = pure util (today's behavior,
                                // the A/B baseline). Default = util band GATED by our own-slice distress.
                                int target = 0;
                                if(cfg.load_governor)
                                    target = cfg.gov_util_floor ? util_floor
                                                                : (gov_self_distress ? util_floor : 0);
                                const int prev_floor = gov_floor_p;
                                if(target > gov_floor_p)      { gov_floor_p = target; gov_dwell_p = kGovDwell; }
                                else if(target < gov_floor_p) { if(gov_dwell_p>0) --gov_dwell_p; else gov_floor_p = target; }
                                g_gov_floor.store(gov_floor_p);
                                // Honest prints: name WHICH signal fired on each engage/release edge (the
                                // operator reads these). Suppressed on the --gov-util-floor A/B path (no distress).
                                if(cfg.load_governor && !cfg.gov_util_floor && gov_floor_p != prev_floor){
                                    if(gov_floor_p > prev_floor)
                                        std::printf("[ra] gov-floor ENGAGE tier:%d (%s%s%sutil %d%% band:%d)\n",
                                            gov_floor_p,
                                            fresh_dist ? "freshage " : "", warp_dist ? "warp " : "",
                                            (fresh_dist||warp_dist) ? "distress; " : "",
                                            (int)gpuA_pct, util_floor);
                                    else
                                        std::printf("[ra] gov-floor RELEASE tier:%d (freshage %.1fms/%.1fx Tsrc %.1fms; warp %.2fms/base %.2fms; util %d%%)\n",
                                            gov_floor_p, clk.freshage_ema(),
                                            (Tsrc_now>0.0?clk.freshage_ema()/Tsrc_now:0.0), Tsrc_now,
                                            wap_warp_ema, warp_base, (int)gpuA_pct);
                                }
                            }
                        }
                    }

                    // ── 2. keep src_interval_ema_ms honest (F reads src_interval_us) ──
                    // Same arrival-delta EMA as the OFF path — never the processed cadence.
                    // sc_delta_ms carries the OUTLIER-REJECTED arrival delta (ms) of THIS tick
                    // to the sync-clock frequency loop below — the SAME sane-band guard (500..500000us DD /
                    // 1000..500000us WGC) is the PLL's outlier rejection (duplicate/late frames that would
                    // poison the period estimate are dropped). 0.0 = no fresh valid delta this tick.
                    double sc_delta_ms=0.0;   // outlier-rejected arrival delta this tick (ms); 0 = none
                    // (PLL units fix) aplicar el delta atrapado UNA vez por nueva INGESTA (avance de
                    // c_seq), no cada tick: el atomic de último-valor re-entraría a la EMA cada tick
                    // hasta la siguiente llegada — inflando el alpha efectivo ~(ticks/llegada)× y
                    // sesgando por duración hacia los deltas cortos. cur_c se lee AQUÍ (una sola
                    // snapshot por tick, la misma que consume el paso 3 abajo).
                    const uint64_t cur_c=c_seq.load();
                    if(cur_c!=tr_last_cseq){
                        tr_last_cseq=cur_c;
#ifdef _MSC_VER
                        if(cfg.capture_api==CA_WGC&&wgc_ctx){
                            const uint64_t d_us=wgc_ctx->arr_delta_us.load();
                            if(d_us>1000&&d_us<500000){
                                src_interval_ema_ms=src_interval_ema_ms*0.9+((double)d_us/1000.0)*0.1;
                                sc_delta_ms=(double)d_us/1000.0;
                            }
                        } else
#endif
                        if(cfg.capture_api==CA_DD){
                            const uint64_t d_us=dd_arr_delta_us.load();
                            if(d_us>500&&d_us<500000){
                                src_interval_ema_ms=src_interval_ema_ms*0.9+((double)d_us/1000.0)*0.1;
                                sc_delta_ms=(double)d_us/1000.0;
                            }
                        }
                    }
                    src_interval_us.store((uint64_t)(src_interval_ema_ms*1000.0));
                    const double T_src=src_interval_ema_ms;
                    // ── STAGE 4 (CLOCK): the PLL/NCO advance + the D calibration + the per-arrival phase lock.
                    // Moved VERBATIM to src/clock/phase_clock.cpp (R1/X14). Runs EVERY tick, before the decimation
                    // gate — it is the panel-unit timebase (the PLL-units rule the gate's comment states).
                    const double now_b=now_ms();   // the set-detect clock read (one per tick; recorded by --arrival-log)
                    // (T5) ONE consistent view of the F->P publish ring per tick: the clock reads this snapshot and
                    // --arrival-log records it, so a replay reproduces select() exactly. F publishes fields BEFORE the
                    // seq_cst f_seq bump, so a snapshot taken right after reading fs is at least as consistent as the
                    // per-access reads it replaces.
                    const uint64_t _fs_now=f_seq.load();
                    double _rt[NS]; uint64_t _rc[NS], _rsp[NS]; int _rn[NS], _rsl[NS];
                    for(int _g=0;_g<NS;++_g){ _rt[_g]=f_pair_tcap_a[_g]; _rc[_g]=f_pair_cseq_a[_g]; _rsp[_g]=f_pair_span_a[_g];
                                              _rn[_g]=f_pair_n_a[_g];   _rsl[_g]=f_pair_slot_a[_g]; }
                    const pfg::clock::PairRing _clk_ring{ _rt, _rc, _rsp, _rn, _rsl };
                    const pfg::clock::AdvanceOut _adv = clk.advance({ now_b, T_src, sc_delta_ms, cur_c, _fs_now, _clk_ring });
                    const uint64_t fs = _adv.fs;
                    const bool have_interp = _adv.have_interp;
                    const int  f_gen_new   = _adv.f_gen_new;
                    const double D         = _adv.D;
                    // ── DECIMATION GATE (--target-output-fps, the §9 fix): skip this vblank slot ──
                    // Everything ABOVE ticks at panel rate EVERY tick — the pacer grid, the own-window
                    // yield log, the window-death watchdog, the util/governor publish, the PLL frequency
                    // (T_robust) + NCO advance + phase slew, and the set-detect/freshage EMAs: the
                    // panel-unit timebases (the PLL-units bug class — decimation must NOT alter the
                    // clock's timebase). Everything BELOW — selection, phase, upload, warp, present,
                    // per-present bookkeeping (uniq/lat/CSV/stats) — runs ONLY on the selected slot; on
                    // skipped slots the flip chain persists the previous frame (NOTHING is dispatched,
                    // no half-work). Presented phases advance naturally in output-period steps because
                    // t_display is TIME-driven (now−D), not tick-count-driven. p_presenting (the gen-
                    // ring guard) holds the LAST PRESENTED set across skipped slots — the correct hold
                    // (that set is still on the panel). tick_k is the same counter the pacer grid uses,
                    // so a grid re-seat (tick_k=0) re-aligns the pattern harmlessly. dec_every==1 →
                    // never taken → byte-identical.
                    // STAGE 6 decision = Decimated (STAGE_CONTRACT Phase.decision, R4): this vblank slot gets NO present call —
                    // the gate is the declaration; Warp / Dup / Drop are handed to PresentStage::begin below.
                    if(dec_every>1 && (tick_k % (uint64_t)dec_every)!=0) continue;
                    // ── STAGE 4 (CLOCK): t_display + the published-set selection + the phase within its window
                    // (incl. the sync-clock phase override and the ASW overshoot). Moved VERBATIM (R1/X14).
                    const double now_d=now_ms();
                    const pfg::clock::Selection _sel = clk.select({ now_d, D, T_src, cur_c, have_interp, f_gen_new, _clk_ring,
                        [](const void* c,int s)->double{ return ((const RealSlot*)c)[s].t_cap_ms; }, (const void*)c_slots });
                    const double   t_display  = _sel.t_display;
                    const int      f_gen      = _sel.f_gen;
                    const int      gen_back   = _sel.gen_back;
                    const bool     found      = _sel.found;
                    const int      N_set      = _sel.N_set;
                    const uint64_t span       = _sel.span;
                    const uint64_t pair_c     = _sel.pair_c;
                    const int      rs         = _sel.rs;
                    const double   tcap_r     = _sel.tcap_r;
                    const bool     real_valid = _sel.real_valid;
                    // (T3) the ring-guard publish: the generation P actually holds, so F stalls before overwriting it.
                    if(have_interp) p_presenting.store(fs-(uint64_t)gen_back);
                    // read the selected generation's affine model from the F→P publish array
                    // (the SAME channel N_set/span/pair_c travel on). Valid only when gme is active, an
                    // interp set is selected, AND F marked this generation's model valid (a fit ran for
                    // it). gme_ptr non-null → wap_warp_present pushes gme_on=1 + the 6 params; null →
                    // gme_on=0 (byte-identical). No model from another pair is ever pushed (the validity
                    // flag is set per-generation in the same iteration that publishes the set).
                    float gme6[6]={}; const float* gme_ptr=nullptr;
                    if(use_gme&&have_interp&&f_pair_gme_valid_a[f_gen]){
                        for(int _p=0;_p<6;++_p) gme6[_p]=f_pair_gme_a[f_gen][_p];
                        gme_ptr=gme6;
                    }
                    // this generation's backward validity. F sets it 0 when it SKIPPED the
                    // bwd pass for this pair under pressure (or no bwd ran). When 0, P pushes
                    // occl_thresh=0 (the bidir classification block is skipped) AND matte_on=0 (no
                    // dual-anchored matte) for THIS generation only — the fwd warp + the net
                    // still run. Off-bidir the array stays 1 (no effect; the push is gated on use_bidir).
                    const bool bwd_ok = !have_interp || f_pair_bwd_valid_a[f_gen]!=0;
                    // this generation's anchored matte masses (block-grid counts) for the laser
                    // expected-mass lerp. Read from the SAME F→P publish channel (valid only when an interp
                    // set is selected and matte runs for it; F zeroes them on no-fit/no-bwd, so a stale pair
                    // never leaks). m_fwd/m_bwd in BLOCK units → ×64 (8×8 px/MV-block) to compare against the
                    // GPU counter's PIXEL-space presented mass (the ratio err is dimensionless regardless).
                    const float m_fwd_g = have_interp ? f_pair_mfwd_a[f_gen] : 0.f;
                    const float m_bwd_g = have_interp ? f_pair_mbwd_a[f_gen] : 0.f;

                    // (STAGE 4, cont.) the phase outputs of the same select() call.
                    const double span_ms      = _sel.span_ms;
                    const double phase_global = _sel.phase_global;
                    const double extrap_amt   = _sel.extrap_amt;
                    dbgD_sum+=D; dbgPh_sum+=phase_global; dbgGB_sum+=(double)gen_back; dbgTsrc_sum+=T_src; dbgSpan_sum+=(double)span; ++dbgN; // TEMP diag

                    // ── WAP: synthesise the EXACT phase on G per tick ──
                    // No nearest-grid rounding: G re-warps the pair at phase_global directly.
                    // Per pair-advance: derive the prev real's slot from
                    // pair_c−span through the capture ring (cap_slots staleness), upload the two
                    // reals + MV + SAD once. Per tick: one warp dispatch at the exact phase.
                    if(use_wap){
                        // ── --rfp-fresh: OVERRIDE the D-selected pair with the freshest captured REAL on a
                        // tick that already lands at it ─────────────────────────────────────────────────────────
                        // When the tick is a real-present (the freshest published pair, gen_back==0, swept to
                        // phase≈1), the warp output would already ≈ the cur real, so the warp is pure latency
                        // tax. A real needs no interpolation pair → no pair-publish wait → it tracks the input.
                        // The interp ticks (gen_back>0, or mid-pair phase) fall through UNCHANGED.
                        // GATE (valid-real + cadence-bound): the armed flag; have_interp + real_valid + cur_c>=1
                        // (the capture slot rs is published & not lapped — the SAME safe-read window the live real
                        // present at do_present_P(hR_a[rs]) relies on); cur_c!=last_rfp_c (at most ONE real per
                        // source frame); gen_back==0 (the freshest pair — pair_c is the cur real's content index);
                        // phase_global>=1-rfp_window (the tick already ≈ the cur real, so no distinct mid-pair
                        // interp is displaced); `found` (NOT the !found deficit-fallback path, which forges
                        // gen_back=0/phase=1.0). Off → cfg.real_fast_path / cfg.rfp_fresh false → the &&
                        // short-circuits, ZERO new work, the warp path below runs byte-identically. The co-arm
                        // guards force --rfp off unless async-present is live and upscale is off, so the present
                        // here is always the dedicated non-blocking bslot path.
                        if(cfg.real_fast_path && cfg.rfp_fresh && found && have_interp && real_valid && cur_c>=1
                           && cur_c!=last_rfp_c && gen_back==0
                           && phase_global >= 1.0 - (double)cfg.rfp_window){
                            // --rfp-fresh: present the FRESHEST captured real ((cur_c-1)%cap_slots — the
                            // safe no-interp read, one frame younger than cur_c so it can never be lapped) and
                            // measure latency vs ITS OWN tcap. Off (rfp_fresh false) → rfp_slot/rfp_tcap collapse
                            // to rs/tcap_r → byte-identical.
                            const int    rfp_slot = cfg.rfp_fresh ? (int)((cur_c-1)%(uint64_t)cap_slots) : rs;
                            const double rfp_tcap = cfg.rfp_fresh ? c_slots[rfp_slot].t_cap_ms : tcap_r;
                            rfp_present(rfp_slot);   // present the chosen real via the DEDICATED async slot
                            const double t_rfp=now_ms();
                            if(rfp_tcap>0.0){ const double lat=t_rfp-rfp_tcap; lat_ema_ms=lat_valid?lat_ema_ms*0.9+lat*0.1:lat; lat_valid=true; }   // the win is visible in lat
                            // monotonicity: set the content-order key to the cur PAIR's TRUE phase-1 key —
                            // KEPT at pair_c even under --rfp-fresh (NOT the capture index cur_c-1: content_clock/
                            // pair_c trail capture by freshage, so anchoring at a capture index forces a hard
                            // reseat + a structural freeze). The displayed PIXELS are the fresh real, but the
                            // ladder bookkeeping continues at pair_c so the next interp resumes correctly (the
                            // cost is the accepted content sawtooth, fresh real → stale interp). NOT INT_MAX
                            // (self-trips the backwards guard → freeze).
                            clk.commit(pair_c,(int)(span_ms/0.1+0.5));
                            last_rfp_c=cur_c; ++rfp_presents; ++uniq_ticks;   // the real is a distinct delivered frame
                            // NO manual ++total_frames — rfp_present's surface submit already incremented it.
                            // CSV (optional) a REAL-tagged row so the fast-path latency is directly measurable.
                            if(tcsv.active()){
                                LARGE_INTEGER qpc_now; QueryPerformanceCounter(&qpc_now);
                                phyriadfg::CsvRow row; row.frame_index=csv_fi++; row.qpc_present=qpc_now.QuadPart;
                                row.frame_type=0; row.frz=0; row.fdrop=0; row.warp_ms=0.0; row.ms_in_present_api=0.0;
                                row.iter_ms=now_ms()-t0_p; row.added_lat_ms=(rfp_tcap>0.0)?(t_rfp-rfp_tcap):-1.0;
                                row.slip_ms=csv_slip; row.cap_slots=cap_slots; row.gme_dis_pct=csv_dis;
                                row.source_fps=csv_src; row.cons_per_s=csv_cons; row.uniq_per_s=csv_uniq;
                                // (FLUIDITY) a fast-path tick presents a REAL at phase 1.0 of its pair; the depicted
                                // content moment is pair_c (the same content-order anchor the ladder bookkeeping continues at,
                                // line ~8930) → disp_src = (pair_c−span)+1·span = pair_c. Keeps the placement trajectory
                                // continuous when --rfp + --csv are both on (a niche default-off latency-experiment combo).
                                row.disp_phase=1.0; row.disp_src=(double)pair_c;
                                // (fluidity PACING axis) same own-window DISPLAY-FLIP read as the main push
                                // site — keeps the prev-flip sample advancing on this (default-off --rfp/--motion-
                                // fallback) fast-path tick so a mixed rfp+csv run still scores pacing on display-flip
                                // time. nullopt/soft-fail → field stays NA → scorer falls back to MsBetweenPresents.
                                // Entirely inside if(tcsv.active()) → byte-identical-off; GetFrameStatistics is read-only.
                                if(const pfg::present::FlipStats fst=pres.flip_stats(); fst.have_flip){   // R4: the stage-6 -> stage-4 feedback edge, consumed here
                                    if(csv_have_flip && fst.present_count!=csv_prev_flip_count && csv_flip_qfreq>0.0)
                                        row.ms_between_display_change = ((double)fst.sync_qpc - (double)csv_prev_flip_qpc) / csv_flip_qfreq * 1000.0;
                                    csv_prev_flip_qpc=fst.sync_qpc; csv_prev_flip_count=fst.present_count; csv_have_flip=true;
                                }
                                row.route_device=0; tcsv.push(row);
                            }
                            continue;   // real-fast-path tick complete — skip the warp/selection bookkeeping below
                        }
                        // ── --motion-fallback: FRAME-LEVEL fast-motion fallback ───────────────────────────────
                        // When THIS pair's gme dispersion (dis% = % of MV blocks the global affine fails on) exceeds
                        // mf_disp, the motion is too fast/incoherent/disoccluding for the warp — the per-pixel garbage
                        // gates cannot save a WHOLE-FRAME breakdown, so the warp would strobe. Present the FRESHEST
                        // captured real via the SAME dedicated -safe bslot path (rfp_present), tracking the freshest
                        // real through the burst (AFMF2 Fast-Motion-Response). Off -> cfg.motion_fallback false -> the
                        // && short-circuits, ZERO new work, the warp below runs byte-identically. Armed only with
                        // --async-present (auto-enabled by the flag; the co-arm guard forces off otherwise ->).
                        // No gen_back==0 / phase gate (unlike --rfp) a breakdown at ANY pair/phase warrants the real.
                        // ── FIX 1 (hysteresis) update the P-local dispersion EMA + Schmitt latch BEFORE the fire gate ──
                        // The shipped gate was an INSTANTANEOUS single-threshold test on the RAW unsmoothed dispersion
                        // (f_pair_disp_a > mf_disp) → it alternated real<->warp tick-to-tick near the threshold = judder.
                        // Smooth the signal (EMA alpha 0.2), then ENGAGE only after the smoothed value holds above
                        // mf_disp for >=kMfDwell ticks and DISENGAGE only after it holds below mf_disp-kMfHyst for
                        // >=kMfDwell ticks (a deadband + dwell = no flapping). Units are dispersion-PERCENT [0,100]
                        // (same as mf_disp; gme_dispct_from_mask returns 100·dis/n), so kMfHyst is in percent. Updated
                        // ONLY inside if(cfg.motion_fallback) → dead off → byte-identical. The HELD-real behavior once
                        // engaged is UNCHANGED (by-design AFMF2 source-rate fallback); only the ENGAGE/DISENGAGE flips.
                        if(cfg.motion_fallback && have_interp && f_pair_gme_valid_a[f_gen]){
                            constexpr double kMfHyst=12.0;   // dispersion-percent deadband (DISENGAGE below mf_disp-kMfHyst)
                            constexpr int    kMfDwell=4;     // ticks the condition must persist before the latch flips
                            const double disp=(double)f_pair_disp_a[f_gen];
                            mf_disp_ema = mf_disp_ema_init ? (mf_disp_ema*0.8 + disp*0.2) : disp;
                            mf_disp_ema_init=true;
                            if(!mf_engaged){
                                if(mf_disp_ema > cfg.mf_disp){ if(++mf_dwell>=kMfDwell){ mf_engaged=true;  mf_dwell=0; } }
                                else mf_dwell=0;
                            } else {
                                if(mf_disp_ema < cfg.mf_disp - kMfHyst){ if(++mf_dwell>=kMfDwell){ mf_engaged=false; mf_dwell=0; } }
                                else mf_dwell=0;
                            }
                        }
                        // Fire gated on the LATCHED mf_engaged (NOT the raw value); the dwell/deadband above kill the flap.
                        if(cfg.motion_fallback && mf_engaged && have_interp && real_valid && cur_c>=1
                           && f_pair_gme_valid_a[f_gen]){
                            const int    mf_slot = (int)((cur_c-1)%(uint64_t)cap_slots);   // freshest safe real (one frame younger than cur_c; never lapped)
                            const double mf_tcap = c_slots[mf_slot].t_cap_ms;
                            rfp_present(mf_slot);                                            // present the freshest real via the dedicated async slot
                            const double t_mf=now_ms();
                            if(mf_tcap>0.0){ const double lat=t_mf-mf_tcap; lat_ema_ms=lat_valid?lat_ema_ms*0.9+lat*0.1:lat; lat_valid=true; }
                            // content-order key continues at pair_c (the held-real bookkeeping, exactly the --rfp pattern
                            // -> the next interp resumes correctly; the cost is the accepted content sawtooth).
                            clk.commit(pair_c,(int)(span_ms/0.1+0.5));
                            ++mf_presents;   // fire-rate stat (mf:N/s) — confirms firing (e.g. on the zoo at a low --mf-disp / on a real scene-cut)
                            // rfp_present's surface submit already did ++total_frames. Held repeats are not uniq.
                            continue;   // motion-fallback tick complete — skip the warp/selection bookkeeping below
                        }
                        // ── STAGE 4 (CLOCK): the content-order key + the backwards guard + the base t_use.
                        // Moved VERBATIM (R1/X14). The phase LAYERS below (--phase-norm / s2 / --cphase) reshape t_use
                        // afterwards; they are NOT the clock.
                        const pfg::clock::Order _ord = clk.order(phase_global, extrap_amt, span_ms, pair_c,
                                                                  cfg.async_present && async_front>=0);
                        double t_use = _ord.t_use;
                        int    cand_k = _ord.cand_k;
                        const bool backwards = _ord.backwards;
                        bool backstep_freeze = _ord.backstep_freeze;
                        const double _clk_t_use = _ord.t_use;   // the CLOCK's own phase, before the phase LAYERS reshape it
                        const int    _clk_cand_k = _ord.cand_k;
                        // ── --phase-norm: the NORMALIZED-N frame ladder ──────────────────
                        // Override the (jittery) content-clock phase with the EVEN grid (pe_j+0.5)/N, N =
                        // predicted ticks-this-pair = span·T_robust/tick_period. The clock already SELECTED
                        // pair_c (temporal tracking); this only makes the intra-pair sweep UNIFORM. Monotone by
                        // construction (pe_j only rises within a pair; resets on pair-advance → pair_c advances
                        // so the monotonicity guard never trips). cand_k is recomputed from the reshaped t_use so
                        // fdrop + the ladder + last_pres bookkeeping all agree. Skipped on a `backwards` tick
                        // (rare anti-rewind — defer to the clamp). Default-off → t_use unchanged, byte-identical.
                        // ── the fractional output-rate controller — realized_mult governor ──
                        // base_fps = source rate (the existing PLL); sustain = the MEASURED achievable present rate
                        // (s2_pres_ema); target_eff = min(target, refresh, kSustainFrac·sustain) — DELIBERATELY below the
                        // measured ceiling so the warp HOLDS it steady ("a steady 180 beats an erratic 191"). realized_mult
                        // = clamp(target_eff/base, 1, 8) with a 0.15 deadband (no pumping; T_robust is the integrator).
                        // N_target = realized_mult·span LOWERS the commanded phase count under saturation → the
                        // over-production drop refuses to over-command the warp (anti-windup; the --pace-variance race fix).
                        double s2_N_over=0.0;
                        if(s2_quant){   // s2 path only (--no-decimate)
                            const double base_fps    = (clk.T_robust_ms()>1e-3) ? 1000.0/clk.T_robust_ms() : 0.0;
                            const double sustain_fps = (s2_pres_ema>1e-3) ? 1000.0/s2_pres_ema : (double)cfg.refresh_hz;
                            double target_eff = (double)cfg.refresh_hz;
                            if((double)cfg.target_output_fps < target_eff) target_eff=(double)cfg.target_output_fps;
                            const double ceil_fps = (double)cfg.s2_sustain_frac * sustain_fps;     // the sustainable ceiling
                            if(ceil_fps>0.0 && ceil_fps<target_eff) target_eff=ceil_fps;
                            double mult_raw = (base_fps>1e-3) ? target_eff/base_fps : 1.0;
                            if(mult_raw<1.0) mult_raw=1.0; if(mult_raw>8.0) mult_raw=8.0;
                            if(s2_mult<=0.0) s2_mult=mult_raw;                              // cold start
                            else if(std::fabs(mult_raw-s2_mult)>0.15) s2_mult=mult_raw;     // deadband — no per-frame pumping
                            s2_N_over = s2_mult * (double)span;
                            if(s2_N_over<1.0) s2_N_over=1.0;
                        }
                        if((cfg.phase_norm || s2_quant) && !backwards){     // forces the even-grid ON (s2 path only — decimation keeps the passive t_display phase)
                            if(!pe_have || pair_c!=pe_pair){ pe_pair=pair_c; pe_j=0; pe_have=true; }
                            double N = (s2_quant && s2_N_over>0.0) ? s2_N_over   // sustainable count
                                     : ((tick_period_ms>1e-6) ? ((double)span * clk.T_robust_ms() / tick_period_ms) : 1.0);   // the passive count (byte-identical off)
                            if(N<1.0) N=1.0;
                            double te = ((double)pe_j + 0.5) / N;
                            if(te<0.0) te=0.0; if(te>1.0) te=1.0;
                            t_use = te;
                            cand_k = (int)((t_use*span_ms)/0.1 + 0.5);   // re-quantise so fdrop/ladder/monotonicity use the reshaped phase
                            ++pe_j;
                        }
                        // ── --cphase: C1 (velocity-continuous) intra-pair phase-rate reshape ──────────
                        // Bend t_use with a monotone cubic g (g(0)=0,g(1)=1 EXACT) whose OPENING slope matches the
                        // prev pair's closing displayed speed: g'(0)=s0 with s0·|mv_this| ≈ |mv_prev| → the velocity
                        // is continuous across the seam (no first-derivative jump = the residual pulse). NO closing
                        // ease (the next pair's speed is unpublished at close → it would distort the wrong boundary;
                        // the seam is carried one-sidedly by the NEXT pair's opening, since this pair closes linear
                        // g'(1)=1). mv is NEVER touched → A/B converge → TEXT SAFE. Endpoints exact → ZERO latency.
                        // g is a cubic Hermite [(0,0)|s0 → (W,W)|1], monotone for s0∈[0.5,2] (FC bound s0²+1≤9).
                        // Runs AFTER --phase-norm (reshapes whatever t_use it produced) and gated !backwards.
                        if(cfg.cphase && !backwards){
                            // velocity-magnitude proxy: the gme AFFINE TRANSLATION (gme6[0]=ga, gme6[3]=gd) — the
                            // dominant camera/global motion over the pair (NOT gme6[2]/[5], which are deformation-
                            // Jacobian terms). Fall back to span (always ≥1) when no valid gme model for this pair.
                            const double vmag_cur = (use_gme && gme_ptr)
                                ? std::sqrt((double)gme6[0]*gme6[0] + (double)gme6[3]*gme6[3]) + 1e-3
                                : (double)span;
                            // On pair-advance: the just-closed pair's vmag becomes "prev"; record this pair's vmag.
                            // The ratio then stays CONSTANT for every tick of this pair (two-var carry, no collapse).
                            if(!cph_have || pair_c!=cph_pair){
                                cph_prev_vmag = cph_have ? cph_cur_vmag : vmag_cur;   // first pair ever → prev=cur → ratio 1 → linear
                                cph_cur_vmag  = vmag_cur; cph_pair = pair_c; cph_have = true;
                            }
                            double ratio = (cph_prev_vmag>1e-6 && cph_cur_vmag>1e-6) ? cph_prev_vmag/cph_cur_vmag : 1.0;
                            if(ratio<0.5) ratio=0.5; if(ratio>2.0) ratio=2.0;        // bound a regime change (cut) — no whip
                            const double G = (double)cfg.cphase_gain;                 // [0,1] at parse
                            const double s0 = 1.0 + G*(ratio-1.0);                    // opening slope; ∈[0.5,2] for G∈[0,1] → FC-monotone
                            const double W = (double)cfg.cphase_ease;                 // [0.05,0.5] at parse
                            double gt = t_use;
                            if(W>1e-3 && t_use < W){
                                const double u  = t_use/W;                            // [0,1) across the opening ease
                                // cubic Hermite from (0,0,slope s0) to (W,W,slope 1), in g-space (scaled by W):
                                // g = W·[ s0·(u³−2u²+u) + (−u³+2u²) ]; g'(0)=s0, g'(W)=1, monotone for s0∈[0.5,2].
                                gt = W * ( s0*(u*u*u - 2.0*u*u + u) + (-u*u*u + 2.0*u*u) );
                            }
                            if(gt<0.0) gt=0.0; if(gt>1.0) gt=1.0;                     // endpoints already exact; guard FP
                            const int gk=(int)((gt*span_ms)/0.1+0.5);
                            // monotone-key guard (belt + suspenders; g is monotone by construction) never step the
                            // content-order key backwards within the pair → never trip the backwards guard.
                            if(!(clk.has_last() && pair_c==clk.last_cseq() && gk<clk.last_k())){ t_use=gt; cand_k=gk; }
                        }
                        // ── --fdrop: exact-duplicate present-side drop decision ──────────
                        // cand_k/pair_c are now FINAL (post backwards-clamp). On a tick whose (pair,cand_k)
                        // == the last DELIVERED frame's (fires exactly when the content-clock did NOT advance
                        // to a new pair/phase, where the warp would otherwise re-emit a byte-identical frame),
                        // DROP it: skip the warp (do_warp=false at the call) and let the present tail re-show
                        // the completed front slot. Pure P-local scalar
                        // compare, lock-free. Guards: --fdrop + async_present (the drop route) + a completed
                        // front to re-show (async_front>=0, else the startup black frame). Byte-identical off.
                        bool fdrop_this=false;
                        if(cfg.fdrop && clk.has_last() && cfg.async_present && async_front>=0)
                            fdrop_this = (pair_c==clk.last_cseq() && cand_k==clk.last_k());
                        // (freeze-guard) el backstep de par-viejo congela por el MISMO path que fdrop:
                        // do_warp=false → re-show del front completado; el guard !fdrop_this de abajo evita
                        // rebobinar last_pres_* → el regreso al par nuevo no dispara el guard de nuevo.
                        if(backstep_freeze) fdrop_this=true;
                        // ── MANDATORY over-production drop — the anti-windup emit gate ──
                        // When the controller is active, drop a tick whose (pair,cand_k) did NOT advance past the last
                        // DELIVERED frame (the N_target even-grid slot did not advance) — re-warping it would over-command
                        // the warp AND re-present a duplicate (the --pace-variance 395fps race). do_warp=false → the async
                        // tail re-shows the completed front. Generalizes --fdrop, made MANDATORY under target>0; the
                        // free-actuator clamp (async_inflight<0) is already in record_this_tick. Counts op_drops.
                        if(s2_quant && clk.has_last() && cfg.async_present && async_front>=0
                           && pair_c==clk.last_cseq() && cand_k==clk.last_k()){
                            if(!fdrop_this) ++s2_opdrops;
                            fdrop_this=true;
                        }
                        // ── instrument: the phase ladder ─────────────────────────────────
                        // t_use is now the FINAL presented phase for this tick (post-backwards-clamp).
                        // On pair-advance (pair_c changed since the pair we were accumulating) flush the
                        // CLOSING pair's ladder, then start a fresh buffer for the new pair. Captures the
                        // exact phase sequence the panel actually showed per pair — the coverage probe.
                        if(phaselog_left>0 && (now_ms()-plog_t0)>=kPlogWarmupMs){
                            if(plog_have && pair_c!=plog_c){
                                std::printf("[ra] phase pair_c=%llu span=%llu n=%d D=%.1f lat=%.1f t=[",
                                    (unsigned long long)plog_c,(unsigned long long)plog_span,plog_total,D,lat_ema_ms);
                                for(int _i=0;_i<plog_n;++_i) std::printf(_i?" %.2f":"%.2f",(double)plog_buf[_i]);
                                if(plog_total>plog_n) std::printf(" ...(+%d)",plog_total-plog_n);
                                std::printf("]\n");
                                --phaselog_left;
                                plog_n=0; plog_total=0; plog_have=false;
                                if(phaselog_left==0) std::printf("[ra] phaselog: complete\n");
                            }
                            if(phaselog_left>0){
                                if(!plog_have){ plog_c=pair_c; plog_span=span; plog_have=true; plog_n=0; plog_total=0; }
                                ++plog_total;
                                if(plog_n<kPlogCap) plog_buf[plog_n++]=(float)t_use;
                            }
                        }
                        // prev real's slot: the pair's previous source = cur (pair_c) − span. Its
                        // ring slot is (prev_cseq−1)%cap_slots. Safe only if C has not lapped it
                        // (age < cap_slots−1) AND the cur real is itself still valid. If unsafe →
                        // freeze: warp from cur as BOTH sources (t collapses to a static present).
                        const uint64_t prev_cseq=(pair_c>span)?(pair_c-span):0u;
                        const bool prev_slot_safe = have_interp && prev_cseq>0
                            && ((cur_c-prev_cseq)<(uint64_t)(cap_slots-1)) && real_valid;
                        int prev_slot = prev_slot_safe ? (int)((prev_cseq-1)%(uint64_t)cap_slots) : rs;
                        if(!prev_slot_safe){ ++wap_freeze; }   // prev real lapped → freeze-at-cur
                        // Upload the pair to G on advance (pair_c changed or first time, or the
                        // prev-slot safety flipped — the inputs differ). One G submit per pair.
                        if(!wap_have_up || pair_c!=wap_pair_c_up){
                            // wsub (a) up — the per-pair pair-advance upload (a DISTINCT G submit
                            // outside the warp lambda's pw window). Timed only on the ticks it FIRES (pair
                            // advance), so its EMA reflects the upload's own cost, not amortized-per-tick.
                            const double wsub_up0 = cfg.wsub ? now_ms() : 0.0;
                            // --vblend (PREDICT): the velocity-continuity target = the PREDICTED next-pair
                            // velocity, formed in the shader as 2·mv_cur − mv_prev (constant-acceleration extrapolation).
                            // We upload the PREV pair's MV field (one generation OLDER than the selected f_gen) as
                            // u_mv_target. The NEXT-fresher pair is NOT in the ring — the present's lead is canceled
                            // by F's ~1-pair lag behind capture, so content_clock lands on the FRESHEST published
                            // pair (gen_back==0) and the "next" pair is unbuilt. Exact lookahead would cost ~1 pair
                            // of latency; PREDICTION is the no-latency path (the prev pair IS in the ring at
                            // gen_back==0: prev = gen_back+1). prev_gen valid when gen_back+1 ≤ NS-1 AND the slot is
                            // published (cseq≠0, older than pair_c); else self-target (==f_gen → mv_prev==mv →
                            // 2·mv−mv==mv → mix toward self = no-op, graceful at startup / ring edge).
                            // --vblend-exact (default-OFF) trades this no-latency PREDICT for the REAL next pair:
                            // the host adds +1 pair of present lead (the expected −1.0 above) so gen_back>=1 and the
                            // next-fresher published pair (gen_back−1) IS in the ring → upload IT as u_mv_target (the
                            // shader then uses it directly, mv_target_s = mv_ut, no extrapolation error). OFF → predict.
                            int target_gen = f_gen;
                            if(cfg.vblend_exact){
                                // EXACT: the real NEXT pair (one fresher than the selected) — available because the
                                // +1 present lead put gen_back>=1. The shader then uses this MV directly (no predict).
                                if(gen_back >= 1){
                                    const int ng = (int)((f_gen_new - (gen_back-1) + NS*NS) % NS);
                                    if(f_pair_cseq_a[ng]!=0 && f_pair_cseq_a[ng] > pair_c) target_gen = ng;   // the published NEXT pair
                                }
                            } else {
                                // PREDICT (the existing default) the PREV pair (one older) → shader forms 2·mv − mv_prev.
                                if(gen_back+1 <= NS-1){
                                    const int pg = (int)((f_gen_new - (gen_back+1) + NS*NS) % NS);
                                    if(f_pair_cseq_a[pg]!=0 && f_pair_cseq_a[pg] < pair_c) target_gen = pg;   // the published PREV pair (for the extrapolation)
                                }
                            }
                            if(cfg.vblend && target_gen!=f_gen) ++vblend_hit;   // a valid target pair was available (≈cons/s confirms the tilt fires every pair)
                            wap_upload(prev_slot,rs,f_gen,target_gen);
                            qd_gen=f_gen;   // --qdump+ (S2.T1): the generation now uploaded to A
                            qd_tgen=target_gen;   // --qdump+ (S2.T1c): the generation bound as u_mv_target (vblend)
                            if(!qd_mv0.empty() && hostMV[f_gen]){ std::memcpy(qd_mv0.data(),hostMV[f_gen],qd_mv0.size()); qd_mv0_ok=true; }
                            if(!qd_mvt0.empty() && target_gen>=0 && target_gen<kGenRing && hostMV[target_gen]){ std::memcpy(qd_mvt0.data(),hostMV[target_gen],qd_mvt0.size()); qd_mvt0_ok=true; }
                            if(cfg.wsub){ const double up=now_ms()-wsub_up0; w_up_ema=w_up_ema>0.0?w_up_ema*0.8+up*0.2:up; }
                            wap_pair_c_up=pair_c; wap_have_up=true;
                            // instrument: dump the warp's actual input pair (the two full-res frames the
                            // synthesis blends) + the pair metadata. hostR is BGRA8 (the BMP's R/B swap is
                            // irrelevant to geometry).
                            if(pairdump_left>0){
                                std::printf("[ra] pairdump %d: pair_c=%llu prev_cseq=%llu span=%llu cur_c=%llu prev_slot=%d cur_slot=%d safe=%d\n",
                                    pairdump_idx,(unsigned long long)pair_c,(unsigned long long)prev_cseq,(unsigned long long)span,
                                    (unsigned long long)cur_c,prev_slot,rs,prev_slot_safe?1:0);
                                char dp[160];
                                std::snprintf(dp,sizeof(dp),"frames\\pair_%02d_a_prev_c%llu.bmp",pairdump_idx,(unsigned long long)prev_cseq);
                                dump_bmp(dp,(const uint8_t*)hostR[prev_slot],WW,WH);
                                std::snprintf(dp,sizeof(dp),"frames\\pair_%02d_b_cur_c%llu.bmp",pairdump_idx,(unsigned long long)pair_c);
                                dump_bmp(dp,(const uint8_t*)hostR[rs],WW,WH);
                                ++pairdump_idx; --pairdump_left;
                                if(pairdump_left==0) std::printf("[ra] pairdump: complete\n");
                            }
                        }
                        // ── the laser mass-feedback threshold for THIS tick ───────────────────
                        // matte_active mirrors the lambda's matte_push (use_matte AND a valid model AND bwd
                        // ok) — only then does the shader count + does the feedback have meaning. thr_eff
                        // uses the CURRENT err_ema (causal: this tick's threshold reflects prior measured
                        // mass), clamped [0.5,2]·matte_thresh. mass_k=0 → thr_eff == matte_thresh exactly.
                        const bool matte_active = (use_matte && gme_ptr!=nullptr && bwd_ok);
                        const double thr_base = cfg.matte_thresh;
                        double thr_eff_d = thr_base * (1.0 + (double)cfg.mass_k * mass_err_ema);
                        if(thr_eff_d < 0.5*thr_base) thr_eff_d = 0.5*thr_base;
                        if(thr_eff_d > 2.0*thr_base) thr_eff_d = 2.0*thr_base;
                        // ── present exactly one frame: warp at t_use, blit, present ──
                        double t_present_ret=0.0;
                        uint32_t presented_mass=0u;
                        if(!g_quit&&!g_quit_threads.load()){
                            const double pw0=now_ms();
                            wap_warp_present((float)t_use,(float)extrap_amt,gme_ptr,bwd_ok,(float)thr_eff_d,&presented_mass,/*do_warp=*/!fdrop_this);   // on an exact-dup drop, skip the warp → pw≈0, re-show the front
                            t_present_ret=now_ms();
                            const double pw=t_present_ret-pw0;
                            wap_warp_ema=wap_warp_ema>0.0?wap_warp_ema*0.8+pw*0.2:pw;
                            if(cfg.fg_auto){ pcost_ema=pcost_ema>0.0?pcost_ema*0.8+pw*0.2:pw; present_cost_us.store((uint64_t)(pcost_ema*1000.0)); }
                            if(tcap_r>0.0){ const double lat=t_present_ret-tcap_r; lat_ema_ms=lat_valid?lat_ema_ms*0.9+lat*0.1:lat; lat_valid=true; }
                            // ── --csv: push a per-present telemetry row (lock-free; ZERO I/O here) ──
                            if(tcsv.active()){
                                LARGE_INTEGER qpc_now; QueryPerformanceCounter(&qpc_now);
                                const uint64_t fz_now=wap_freeze; const bool froze=(fz_now!=csv_last_frz); csv_last_frz=fz_now;
                                phyriadfg::CsvRow row;
                                row.frame_index = csv_fi++;
                                row.qpc_present = qpc_now.QuadPart;
                                row.frame_type  = (fdrop_this||froze)?1:0;   // Repeated on a freeze re-show OR a --fdrop exact-dup drop
                                row.frz         = froze?1:0;                  // real lap-freeze ONLY (a --fdrop drop is a DISTINCT cause → row.fdrop, never conflated into freeze_count)
                                row.fdrop       = fdrop_this?1:0;             // this present was an exact-duplicate discriminator drop (warp_ms≈0)
                                row.warp_ms     = pw;                 // this tick's warp/present lambda cost (≈0 on a drop — the slot cost nothing, honest)
                                row.ms_in_present_api = pw;
                                row.iter_ms     = now_ms()-t0_p;
                                row.added_lat_ms= (tcap_r>0.0)? (t_present_ret-tcap_r) : -1.0;
                                row.slip_ms     = csv_slip;
                                row.cap_slots   = cap_slots;
                                row.gme_dis_pct = csv_dis;
                                row.source_fps  = csv_src;
                                row.cons_per_s  = csv_cons;
                                row.uniq_per_s  = csv_uniq;
                                row.route_device= 0;                  // 4090 warp/synthesis
                                // ── (FLUIDITY / motion-placement axis) the displayed intra-pair phase + the
                                // content-SOURCE-TIME this present depicts. t_use is the FINAL presented phase for
                                // this tick (post backwards-clamp / --phase-norm / --gravity-ease, line ~9097); pair_c
                                // + span are the selected pair's B content-index and its source-frame span (line 8770,
                                // 8800-8811). (pair_c−span)+t_use·span = the source moment depicted (source-frame units)
                                // the Animation-Error animation-time analogue the deterministic source makes valid
                                // for GENERATED frames. These two assignments are INSIDE `if(tcsv.active())`, which is only
                                // true when --csv armed tcsv.start(); with no --csv they NEVER execute → byte-identical-off.
                                row.fresh       = pres.last_present_fresh()?1:0;   // R4b: 1 = the panel got a NEW warp this tick, 0 = a re-show of the previous image
                                row.warp_gpu_ms = pres.presented_warp_gpu_ms();    // R4b (--warp-timing): the presented warp batch's GPU time (NA on a re-show / off)
                                row.warp_lat_ms = pres.presented_warp_lat_ms();    // R4b (--warp-timing): its submit->completion latency as the host saw it
                                row.disp_phase  = t_use;
                                row.disp_src    = ((double)pair_c - (double)span) + t_use * (double)span;
                                // ── (fluidity PACING axis) the DISPLAY-FLIP interval for MsBetweenDisplayChange ──
                                // Read the own-window flip swapchain's last-flip stats (SyncQPCTime + PresentCount). When
                                // PresentCount advanced since our last sample, the SyncQPCTime delta is a real inter-flip
                                // interval → score pacing on DISPLAY-FLIP time, NOT present-CALL time (which --pace-hard
                                // directly controls, making a pacing A/B on MsBetweenPresents partly circular). nullopt on
                                // the composed/non-own-window path OR a GetFrameStatistics soft-fail → leave the field at
                                // its <0 sentinel (NA) → the scorer falls back to the MsBetweenPresents proxy (flagged).
                                // This entire read is INSIDE if(tcsv.active()) (the --csv gate) → never runs without --csv
                                // → byte-identical-off; GetFrameStatistics is read-only (no present-path change).
                                if(const pfg::present::FlipStats fst=pres.flip_stats(); fst.have_flip){   // R4: the stage-6 -> stage-4 feedback edge, consumed here
                                    if(csv_have_flip && fst.present_count!=csv_prev_flip_count && csv_flip_qfreq>0.0)
                                        row.ms_between_display_change = ((double)fst.sync_qpc - (double)csv_prev_flip_qpc) / csv_flip_qfreq * 1000.0;
                                    csv_prev_flip_qpc=fst.sync_qpc; csv_prev_flip_count=fst.present_count; csv_have_flip=true;
                                }
                                tcsv.push(row);
                            }
                            // ── (FG bounded-run) the deadline guard — default-off, byte-identical-off ────────
                            // The sweep orchestrator bounds a per-config run by wall-clock (--duration/--exit-after) or
                            // by present count (--max-frames). BOTH default to 0 (unbounded), so each `&&` short-circuits
                            // before any clock/counter read → this block is UNENTERED at the defaults → byte-identical.
                            // On a bound we set the SAME g_quit the console Ctrl-C handler + vk_live set (
                            // 3039) the present loop exits at its next `while(!g_quit...)` check, the main pump joins the
                            // workers, and the existing done: teardown runs — NO new exit machinery (the documented
                            // design intent). tcsv's drain/flush + the -stats.csv write happen at
                            // tcsv scope-exit on this thread, so the bounded exit finalizes the CSV like any clean quit.
                            // total_frames is the canonical present counter (the same atomic the done summary prints).
                            // → (reuse the proven g_quit unwind, no present-thread race), (byte-identical-off).
                            // (the deadline guard that lived here moved to the loop top — R4b; see there)
                            // ── update the mass-conservation feedback from the measured mass ──
                            // expected = lerp(m_fwd, m_bwd, t)·64 (block→pixel). err = (presented−expected)/
                            // max(expected,1). err_ema EMA 0.9. Only when matte_active AND mass_k>0 (the
                            // feedback is armed) otherwise leave err_ema/thr_eff inert (mass_k=0 ⇒ thr_eff
                            // is matte_thresh regardless, so the lerp-only path is byte-clean). The windowed
                            // mean feeds `mass:±NN%` in the stats line.
                            if(matte_active && cfg.mass_k>0.0f){
                                const double expected = ((1.0-(double)t_use)*(double)m_fwd_g + (double)t_use*(double)m_bwd_g) * 64.0;
                                const double err = ((double)presented_mass - expected) / (expected>1.0?expected:1.0);
                                // (stigmergy expiration) a large innovation (|err − ema| > kMassErrExpire)
                                // means the REGIME changed (an object entered/left the scene) — the stale EMA must
                                // not steer thr_eff through the transition for ~10 ticks. Adopt the fresh error
                                // outright; the EMA keeps smoothing within-regime noise. --no-expire = pure EMA.
                                const bool m_contra = mass_err_init && std::fabs(err - mass_err_ema) > kMassErrExpire;
                                mass_err_ema = (!mass_err_init || (cfg.expire && m_contra))
                                             ? err : (mass_err_ema*0.9 + err*0.1);
                                mass_err_init = true;
                                mass_err_sum_win += err; ++mass_err_n_win;
                            }
                        }
                        // uniqueness proxy + monotonicity bookkeeping (distinct (pair,0.1ms-phase))
                        // --fdrop: on a drop, DON'T advance last_pres_* / uniq_ticks — the next
                        // distinct candidate must compare against the last truly-DELIVERED frame. For an
                        // EXACT dup the guarded increment is already false and the assigns are no-ops, so this
                        // is byte-identical when --fdrop is off (fdrop_this always false); the wrap is the
                        // forward-correct form for Stage B's soft (non-exact) drop.
                        if(!fdrop_this){
                            if(clk.has_last()){ if(!(pair_c==clk.last_cseq()&&cand_k==clk.last_k())) ++uniq_ticks; }
                            clk.commit(pair_c,cand_k);
                        }
                        // ── --arrival-log (R1/X14): the CLOCK's own inputs+outputs + the COMMIT outcome ──
                        // _clk_t_use/_clk_cand_k are the pure clock result (pre phase-norm/cphase/s2); the trailing
                        // two columns are `committed` and the cand_k actually committed (the layers reshape it).
                        if(alog){
                            std::fprintf(alog,"%llu %a %a %llu %llu %a %a %d ",
                                (unsigned long long)alog_n,now_b,now_d,(unsigned long long)cur_c,(unsigned long long)fs,sc_delta_ms,T_src,(int)(cfg.async_present && async_front>=0));
                            for(int _g=0;_g<NS;++_g) std::fprintf(alog,"%a %llu %llu %d %d ",_rt[_g],(unsigned long long)_rc[_g],
                                                                 (unsigned long long)_rsp[_g],_rn[_g],_rsl[_g]);
                            std::fprintf(alog,"%a %a %a %a %d %d %d %llu %llu %d %d %a %a %a %a %d %d %d %d %d\n",
                                D,t_display,clk.content_clock(),clk.T_robust_ms(),f_gen,gen_back,(int)found,
                                (unsigned long long)pair_c,(unsigned long long)span,N_set,rs,tcap_r,phase_global,extrap_amt,_clk_t_use,
                                _clk_cand_k,(int)backwards,(int)backstep_freeze,(int)(!fdrop_this),cand_k);
                            ++alog_n;
                        }
                        // per-second stats (WAP marker; uniq counts distinct (pair,0.1ms-phase))
                        const double iter_p=now_ms()-t0_p; sum_iter+=iter_p; if(iter_p>worst)worst=iter_p;
                        if(total_frames.load()-last_stat_presents>=90){
                            const double dt=(now_ms()-stat_t)/1e3;
                            const uint64_t dpres=total_frames.load()-last_stat_presents;
                            const double fps=dt>0?(double)dpres/dt:0;
                            const double src_fps=dt>0?(double)stat_ticks/dt:0;
                            const uint64_t cons_now=stat_cons.load();
                            const uint64_t cons_delta=cons_now-last_cons_p;   // pairs consumed this window (bwd-skip denominator)
                            const double cons_fps=dt>0?(double)cons_delta/dt:0; last_cons_p=cons_now;
                            char arr_buf[48]="";
#ifdef _MSC_VER
                            if(cfg.capture_api==CA_WGC&&wgc_ctx){
                                const uint64_t a_now=wgc_ctx->arrived.load();
                                const double arr_fps2=dt>0?(double)(a_now-last_arrived)/dt:0; last_arrived=a_now;
                                std::snprintf(arr_buf,sizeof(arr_buf)," (arr %.0f)",arr_fps2);
                            } else
#endif
                            if(cfg.capture_api==CA_DD){
                                const uint64_t a_now=dd_arrived.load();
                                const double arr_fps2=dt>0?(double)(a_now-last_arrived)/dt:0; last_arrived=a_now;
                                std::snprintf(arr_buf,sizeof(arr_buf)," (arr %.0f)",arr_fps2);
                            }
                            const uint64_t cap_now=total_real.load();
                            const double cap_fps=dt>0?(double)(cap_now-last_cap_p)/dt:0; last_cap_p=cap_now;
                            // CAPTURE cross-check vs minimal_fg: in=cap_fps (real frames ingested/s = mfg 'in'); arrived=dd_arrived/s; dd_timeouts/s (DDA WAIT_TIMEOUT, 33ms granularity).
                            char rf_buf[32]="";   // WGC staging-ring-full drop rate (empty on DDA — 0 there by construction, like arr_buf)
#ifdef _MSC_VER
                            if(cfg.capture_api==CA_WGC&&wgc_ctx){ static uint64_t _lrf1=0; const uint64_t _rf=wgc_ctx->ringfull.load();
                                std::snprintf(rf_buf,sizeof(rf_buf)," ringfull=%.0f/s",dt>0?(double)(_rf-_lrf1)/dt:0); _lrf1=_rf; }
#endif
                            { static uint64_t _lda1=0,_ldt1=0,_ldu1=0; const uint64_t _da=dd_acq.load(),_dto=dd_timeouts.load(),_du=dd_uniq.load();
                              const double _af=dt>0?(double)(_da-_lda1)/dt:0,_tf=dt>0?(double)(_dto-_ldt1)/dt:0,_uf=dt>0?(double)(_du-_ldu1)/dt:0; _lda1=_da; _ldt1=_dto; _ldu1=_du;
                              std::printf("[ra-cap] in=%.0f/s | acq=%.0f/s uniq=%.0f/s dd_timeouts=%.0f/s dd_lost=%llu%s\n",cap_fps,_af,_uf,_tf,(unsigned long long)dd_lost.load(),rf_buf); }
                            // FPS-OVERLAY (--fps-overlay) publish in=cap_fps (real-captured) / out=fps (presented) for the overlay (rounded to uint).
                            g_ov_in.store((uint32_t)(cap_fps+0.5)); g_ov_out.store((uint32_t)(fps+0.5));
                            const double uniq_fps=dt>0?(double)(uniq_ticks-last_uniq)/dt:0; last_uniq=uniq_ticks;
                            // rdrop = async re-present drops/s (warp in-flight → tick re-shows the stale front;
                            // uniq NO los ve). Crónico ≈ tick-rate/2 = la mitad de las posiciones calculadas
                            // nunca llega al panel. Impreso solo cuando ocurre (calma = línea intacta).
                            const double rdrop_fps=dt>0?(double)(rdrop_ticks-last_rdrop)/dt:0; last_rdrop=rdrop_ticks;
                            char rdrop_buf[64]="";
                            const double fresh_fps=dt>0?(double)(fresh_ticks-last_fresh)/dt:0; last_fresh=fresh_ticks;   // R4b: NEW warps that reached the panel
                            if(rdrop_fps>0.5) std::snprintf(rdrop_buf,sizeof(rdrop_buf)," fresh:%.0f/s rdrop:%.0f/s",fresh_fps,rdrop_fps);
                            else              std::snprintf(rdrop_buf,sizeof(rdrop_buf)," fresh:%.0f/s",fresh_fps);
                            char tim_buf[48]=""; pres.timing_line(tim_buf,sizeof(tim_buf));   // R4b (--warp-timing): " gpu X.XXms sub2fence Y.YYms" or empty
                            const double frz_fps=dt>0?(double)(wap_freeze-last_wap_freeze)/dt:0; last_wap_freeze=wap_freeze;
                            const double slip_avg=slip_n?slip_sum/(double)slip_n:0.0, slip_mx=slip_max;
                            // --csv: forward-fill the per-second rates for the per-present rows.
                            if(tcsv.active()){ csv_src=cap_fps; csv_cons=cons_fps; csv_uniq=uniq_fps; csv_lat=lat_ema_ms; csv_slip=slip_avg; csv_dis=use_gme?(double)gme_dis_x100.load()/100.0:-1.0; }
                            // ps marker — PresentSurface submit ok/timeout/err per second.
                            char ps_buf[64]="";
                            { const double psok=dt>0?(double)(ps_ok-last_ps_ok)/dt:0; last_ps_ok=ps_ok;
                              std::snprintf(ps_buf,sizeof(ps_buf)," | ps %.0f/s ok=%llu to=%llu er=%llu",psok,
                                  (unsigned long long)ps_ok,(unsigned long long)ps_timeout,(unsigned long long)ps_err); }
                            // gme marker — dis:NN% (% blocks with r>4px from the last fit); the
                            // fit-cost EMA is appended only when it exceeds 1ms (the kickoff bar). Printed
                            // only when gme is active (the F-published dis value rides gme_dis_x100).
                            char gme_buf[80]="";
                            if(use_gme){ const double dis=(double)gme_dis_x100.load()/100.0;
                                const double gme_fit_ms_p=(double)gme_fit_us.load()/1000.0;
                                if(gme_fit_ms_p>1.0) std::snprintf(gme_buf,sizeof(gme_buf)," | gme(dis:%.0f%% fit:%.2fms)",dis,gme_fit_ms_p);
                                else                 std::snprintf(gme_buf,sizeof(gme_buf)," | gme(dis:%.0f%%)",dis); }
                            // bwd-skip:NN% — the % of CONSUMED pairs this window for which F
                            // skipped the bwd record+fit under pressure (the hysteresis throttle). Printed
                            // only when nonzero (calm = 0 → omitted, line unchanged). Windowed delta /
                            // cons_delta, the same plumbing dis% rides. Appended into the gme segment.
                            char bwdsk_buf[24]="";
                            if(use_bidir){ const uint64_t bsk_now=stat_bwd_skips.load();
                                const uint64_t bsk_delta=bsk_now-last_bwd_skips_p; last_bwd_skips_p=bsk_now;
                                if(bsk_delta>0&&cons_delta>0)
                                    std::snprintf(bwdsk_buf,sizeof(bwdsk_buf)," bwd-skip:%.0f%%",100.0*(double)bsk_delta/(double)cons_delta); }
                            // lap:N/s — F spin-guard escapes this window (P was FROZEN pinned on the
                            // generation F was about to overwrite → F broke the bounded wait to stay live).
                            // Windowed rate; printed ONLY when nonzero (the death-prevention firing is visible;
                            // a calm run never shows it → line unchanged).
                            char lap_buf[24]="";
                            { const uint64_t lap_now=stat_lap.load();
                              const uint64_t lap_delta=lap_now-last_lap_p; last_lap_p=lap_now;
                              if(lap_delta>0) std::snprintf(lap_buf,sizeof(lap_buf)," lap:%.0f/s",dt>0?(double)lap_delta/dt:0.0); }
                            // tier:N — F's current pressure-tier escalation (1=bwd-skip, 2=holon
                            // every-2nd, 3=holon every-4th). Printed only when >0 (tier 0 = calm → omitted, line
                            // unchanged). Single-scalar F publish (stat_tier), the dis% plumbing.
                            char tier_buf[16]="";
                            { const uint64_t tr=stat_tier.load();
                              if(tr>0) std::snprintf(tier_buf,sizeof(tier_buf)," tier:%llu",(unsigned long long)tr); }
                            // mass:±NN% — the windowed-mean signed mass error (presented vs the
                            // t-lerp of the anchored masses). Printed only when the feedback is armed
                            // (mass_k>0) AND it actually measured this window (matte ran) → calm/off → omitted,
                            // line unchanged. The sign indicates the direction (− = thinning/biting
                            // → thr drops; + = spilling → thr rises). Appended into the gme segment.
                            char mass_buf2[24]="";
                            if(cfg.mass_k>0.0f && mass_err_n_win>0){
                                const double m_pct=100.0*mass_err_sum_win/(double)mass_err_n_win;
                                std::snprintf(mass_buf2,sizeof(mass_buf2)," mass:%+.0f%%",m_pct); }
                            mass_err_sum_win=0.0; mass_err_n_win=0;
                            // obj:K rep:N% — live tracked objects (K, the fwd anchor count F
                            // published) + the % of in-fill (silhouette-interior) blocks the inheritance
                            // repair rewrote this pair. F-published via obj_live/obj_rep_x10 (the same
                            // single-scalar plumbing dis% rides). Omitted when no objects are tracked
                            // (obj_live==0 → calm/off → the line is unchanged). Appended into the gme segment.
                            char obj_buf[28]="";
                            if(use_objects){ const uint64_t ol=obj_live.load();
                                if(ol>0){ const double repp=(double)obj_rep_x10.load()/10.0;
                                    std::snprintf(obj_buf,sizeof(obj_buf)," obj:%llu rep:%.0f%%",(unsigned long long)ol,repp); } }
                            // wsub(up:F rec:F gpu:F prs:F) — the warp-tick cost breakdown, gated on
                            // --wsub. up = per-pair upload (fires on advance only), rec = CPU cmd record, gpu =
                            // submit + the blocking fence wait (the shader stack lives here), prs = the present
                            // pillar (bridge_present) + the sub-µs host mass-read. These split the `warp` EMA so
                            // the regression's true segment is visible. Omitted unless --wsub (line unchanged).
                            char wsub_buf[64]="";
                            if(cfg.wsub) std::snprintf(wsub_buf,sizeof(wsub_buf)," wsub(up:%.2f rec:%.2f gpu:%.2f prs:%.2f)",
                                w_up_ema,w_rec_ema,w_gpu_ema,w_prs_ema);
                            // gpu(A:NN% B:NN% G:NN%) — per-adapter summed GPU-engine
                            // utilization (1Hz PDH). Printed once a refresh has landed (gpu_have); omitted
                            // until the first 1s sample arrives (line unchanged). A is the panel-owner 4090
                            // (capture+warp+present), B the assist discrete, G the iGPU (n/a if absent).
                            char gpu_buf[48]="";
                            if(gpu_have) std::snprintf(gpu_buf,sizeof(gpu_buf)," gpu(A:%.0f%% B:%.0f%% G:%.0f%%)",gpuA_pct,gpuB_pct,gpuG_pct);
                            // instrument (--fsub) the F per-pair split. flow = the blocking fwd
                            // fence wait (1080 Ti GPU leg); pair = the full F-pair; cpu = pair − flow = the
                            // serial CPU tail forward-pipelining would hide under flow. Printed only
                            // under --fsub (off → line byte-unchanged). Reads the F-published µs atomics.
                            char fsub_buf[48]="";
                            if(cfg.fsub){ const double fl=(double)stat_flow_us.load()/1000.0;
                                const double pr=(double)stat_pair_us.load()/1000.0;
                                const double cp=pr>fl?pr-fl:0.0;
                                std::snprintf(fsub_buf,sizeof(fsub_buf)," fsub(flow:%.1f pair:%.1f cpu:%.1f)",fl,pr,cp); }
                            // --vblend: per-second lookahead-fired rate — ≈cons/s (≥1/pair) confirms the
                            // next-fresher pair was available in the F→P ring (the holonic lead → FREE lookahead), so
                            // the velocity tilt is NOT a no-op (target≠self). Off → empty buffer (line byte-unchanged).
                            char vbhit_buf[40]="";
                            if(cfg.vblend){ const double vbh=dt>0?(double)(vblend_hit-last_vblend_hit)/dt:0.0; last_vblend_hit=vblend_hit;
                                std::snprintf(vbhit_buf,sizeof(vbhit_buf)," vbhit:%.0f/s",vbh); }
                            // --real-fast-path: per-second real-fast-path injection rate — how many ticks
                            // this window presented the freshest real via the dedicated async slot (the responsiveness
                            // lever's fire rate; confirms it does NOT collapse the interp ladder). Off →
                            // empty buffer (line byte-unchanged).
                            char rfp_buf[36]="";
                            if(cfg.real_fast_path){ const double rfh=dt>0?(double)(rfp_presents-last_rfp_presents)/dt:0.0; last_rfp_presents=rfp_presents;
                                std::snprintf(rfp_buf,sizeof(rfp_buf)," rfp:%.0f/s",rfh); }
                            // --shallow-queue: per-second early-promote hit/miss rate (sq:H/M). The
                            // depth-collapse measure: high H = the warp finished within budget = present depth ~0
                            // (mid-motion); near-all-M = the 4090 overran the tick (combat) → the lever is inert
                            // there. Off → empty buffer (line byte-unchanged).
                            char sq_buf[40]="";
                            if(cfg.shallow_queue){ const double sqh=dt>0?(double)(sq_hits-last_sqh_p)/dt:0.0; const double sqm=dt>0?(double)(sq_misses-last_sqm_p)/dt:0.0;
                                last_sqh_p=sq_hits; last_sqm_p=sq_misses;
                                std::snprintf(sq_buf,sizeof(sq_buf)," sq:%.0fH/%.0fM",sqh,sqm); }
                            // --motion-fallback: per-second fire rate (mf:N/s) — ticks that presented the freshest
                            // real instead of the warp because the gme dispersion exceeded --mf-disp. 0 in calm/coherent
                            // motion (the conservative default). Off → empty buffer (line byte-unchanged).
                            char mf_buf[28]="";
                            if(cfg.motion_fallback){ const double mfh=dt>0?(double)(mf_presents-last_mf_presents)/dt:0.0; last_mf_presents=mf_presents;
                                std::snprintf(mf_buf,sizeof(mf_buf)," mf:%.0f/s",mfh); }
                            // --pace-hard: ph:H/s/O/s — the per-second HARD-pin fire rate
                            // (H = the bounded spin pinned this frame) and the OVERSHOOT/degrade rate (O = the target was
                            // past/un-derivable/over-cap → degraded to the soft path, NEVER blocked). O is the freeze-floor
                            // evidence: it must self-cap, never wedge. Off → empty buffer (line byte-unchanged).
                            char ph_buf[72]="";
                            if(cfg.pace_hard && cfg.present_own_window){ const double phh=dt>0?(double)(ph_held-last_ph_held)/dt:0.0; const double pho=dt>0?(double)(ph_overshoot-last_ph_overshoot)/dt:0.0;
                                last_ph_held=ph_held; last_ph_overshoot=ph_overshoot;
                                int n=std::snprintf(ph_buf,sizeof(ph_buf)," ph:%.0fH/%.0fO/s",phh,pho);
                                // --pace-vblank: vbl:LOCKS/FAILS — the cumulative vblank-query
                                // success vs unavailable count (cold ~1/sec; FAILS>0 = no stable phase, degraded to plain
                                // --pace-hard, the per-config evidence). Appended to ph_buf; OFF → unchanged (byte-identical).
                                if(cfg.pace_vblank && n>0 && (size_t)n<sizeof(ph_buf))
                                    std::snprintf(ph_buf+n,sizeof(ph_buf)-(size_t)n," vbl:%lluL/%lluF",(unsigned long long)pv_locks,(unsigned long long)pv_lock_fails); }
                            std::printf("[ra] %.1f fps (present) | wap tick %.0f/s%s | cap %.0f/s | cons %.0f/s | uniq %.0f/s%s | frz %.1f/s | warp %.2fms | iter %.2f/worst %.2fms | lat %.1fms | slip %.2f/max %.2fms%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
                                fps,src_fps,arr_buf,cap_fps,cons_fps,uniq_fps,rdrop_buf,frz_fps,wap_warp_ema,
                                sum_iter/(double)(stat_ticks>0?stat_ticks:1),worst,lat_ema_ms,
                                slip_avg,slip_mx,gme_buf,bwdsk_buf,lap_buf,tier_buf,mass_buf2,obj_buf,ps_buf,wsub_buf,gpu_buf,fsub_buf,vbhit_buf,rfp_buf,sq_buf,mf_buf,ph_buf,tim_buf);
                            // --latency-trace: the full pipeline latency decomposition. INVISIBLE =
                            // pre-tcap (compose+copy, NOT in freshage/lat — perceived but uncounted). freshage =
                            // pickup(⊇convert) + build + detect(derived = freshage−fpub). Total ≈ invisible+freshage
                            // (the present tail, not yet traced). compose=0 ⇒ the QPC↔SystemRelativeTime epoch
                            // mismatch guard fired (the WGC compose time is unavailable on this clock).
                            if(cfg.latency_trace){
                                const double comp=(double)lt_compose_us.load()/1000.0, copy=(double)lt_copy_us.load()/1000.0;
                                const double conv=(double)c_conv_us.load()/1000.0, pick=(double)lt_pickup_us.load()/1000.0;
                                const double fwake=(double)lt_fwake_us.load()/1000.0;   // publish→consume wake (sub-component of pickup)
                                const double fpub=(double)lt_fpub_us.load()/1000.0, fresh=clk.freshage_ema();
                                const double pre=(double)lt_preflow_us.load()/1000.0, spin=(double)lt_spin_us.load()/1000.0;
                                const double bld=fpub>pick?fpub-pick:0.0, det=fresh>fpub?fresh-fpub:0.0;
                                const double cmp=bld>pre?bld-pre:0.0;   // F-compute = build − pre_flow (≈ fsub flow+cpu)
                                const double mfb=dt>0?(double)(stat_mapfb.load()-last_mapfb_lt)/dt:0.0;     // the +11.6ms older-slot fallback rate (copy-fence gate)
                                const double mms=dt>0?(double)(stat_mapmiss.load()-last_mapmiss_lt)/dt:0.0; // the Sleep-retry rate
                                last_mapfb_lt=stat_mapfb.load(); last_mapmiss_lt=stat_mapmiss.load();
                                std::printf("[lat-trace] INVISIBLE(compose:%.1f copy:%.1f) | freshage=%.1f [pickup:%.1f(conv:%.1f fwake:%.1f) build:%.1f(preflow:%.1f[spin:%.1f] compute:%.1f) detect~%.1f] | game->screen~%.1f | mapfb:%.0f/s mapmiss:%.0f/s\n",
                                    comp,copy,fresh,pick,conv,fwake,bld,pre,spin,cmp,det,comp+copy+fresh,mfb,mms);
                            }
                            sum_iter=0; worst=0; slip_sum=slip_max=0; slip_n=0;
                            stat_ticks=0;
                            stat_t=now_ms(); last_stat_presents=total_frames.load();
                        }
                        continue;   // WAP tick complete — skip the grid path below
                    }

                    // ── 5. nearest pre-generated phase ─────────────────
                    // Interps live at phases (k+1)/N for k=0..NI-1 (buffers hI_*[f_gen][k]);
                    // the pair's CUR real is phase 1 (hR_*[rs]); the prev real (phase 0) is
                    // the previous set's cur — not separately buffered — so clamp k* to
                    // [1,N]: interior → interp, k*==N → the real. k*<1 would need the prev
                    // real; we hold the freshest interior phase instead (never steps back).
                    int kstar = (int)(phase_global*(double)N_set+0.5);   // round-to-nearest
                    if(kstar<1) kstar=1; if(kstar>N_set) kstar=N_set;
                    bool present_real = (kstar>=N_set) || !have_interp;
                    int  interp_k = kstar-1;   // index into hI_*[f_gen][·] when interior
                    // If the pair's real slot was lapped by C, fall back to the freshest
                    // interior interp (index N_set-2) rather than presenting a stale real —
                    // but only when there IS an interior phase (N_set>=2).
                    if(present_real&&!real_valid&&have_interp&&N_set>=2){ present_real=false; interp_k=N_set-2; }

                    // ── 6. monotonicity (spirit) — never step content backwards ──
                    // Content order key = (pair_c, k). A candidate that is strictly behind
                    // the last shown is dropped to a freeze of the last (present the freshest
                    // real of the held pair). Within-pair: higher k = later phase.
                    int cand_k = present_real ? N_set : kstar;
                    bool backwards=false;
                    if(clk.has_last()){
                        if(pair_c<clk.last_cseq()) backwards=true;
                        else if(pair_c==clk.last_cseq() && cand_k<clk.last_k()) backwards=true;
                    }
                    if(backwards){
                        // hold: re-present the newest real we are entitled to (phase-aligned
                        // freeze, no rewind). Keeps the panel ticking without a beat.
                        present_real=true; cand_k=N_set;
                        if(!real_valid&&have_interp&&N_set>=2){ present_real=false; interp_k=N_set-2; cand_k=interp_k+1; }
                    }

                    // ── 7. present exactly one frame ───────────────────────────
                    double t_present_ret=0.0;
                    if(!g_quit&&!g_quit_threads.load()){
                        const double pw0=now_ms();
                        if(present_real){
                            // the present is the A-side bridge (do_present_P). The upscale
                            // pre-pass (do_upscale_real_P — unpacks the packed real then upscales for
                            // the igpu-convert case) stays compiled but is a no-op (use_upscale off).
                            do_upscale_real_P(rs); do_present_P(hR_a[rs]);
                            if(dump_left>0&&total_frames.load()>400){
                                char dp[160]; std::snprintf(dp,sizeof(dp),"frames/d%03d_real_c%llu.bmp",dump_idx,(unsigned long long)pair_c);
                                dump_bmp(dp,(const uint8_t*)hostR[rs],WW,WH); ++dump_idx; --dump_left;
                                if(dump_left==0) std::printf("[ra] dump: complete — %d frames in frames\\\n",dump_idx);
                            }
                        } else {
                            // interior interp index kk ∈ [0, N_set-2] (NI=N_set-1 buffers).
                            const int kk=std::clamp(interp_k,0,std::max(0,N_set-2));
                            do_upscale_P(hI_a[f_gen][kk]); do_present_P(hI_a[f_gen][kk]);
                            if(dump_left>0&&total_frames.load()>400){
                                char dp[160]; std::snprintf(dp,sizeof(dp),"frames/d%03d_i%dof%d_c%llu.bmp",dump_idx,kk+1,N_set,(unsigned long long)pair_c);
                                dump_bmp(dp,(const uint8_t*)hostI[f_gen][kk],WW,WH); ++dump_idx; --dump_left;
                                if(dump_left==0) std::printf("[ra] dump: complete — %d frames in frames\\\n",dump_idx);
                            }
                        }
                        t_present_ret=now_ms();
                        if(cfg.fg_auto){ const double pw=t_present_ret-pw0; pcost_ema=pcost_ema>0.0?pcost_ema*0.8+pw*0.2:pw; present_cost_us.store((uint64_t)(pcost_ema*1000.0)); }
                        // lat measures against the presented content's capture wall-time.
                        if(tcap_r>0.0){ const double lat=t_present_ret-tcap_r; lat_ema_ms=lat_valid?lat_ema_ms*0.9+lat*0.1:lat; lat_valid=true; }
                    }
                    // ── 8. monotonicity bookkeeping + uniqueness proxy ─────────
                    if(clk.has_last()){ if(!(pair_c==clk.last_cseq()&&cand_k==clk.last_k())) ++uniq_ticks; }
                    clk.commit(pair_c,cand_k);

                    // ── 9. per-second stats ───────────────────────────────────
                    const double iter_p=now_ms()-t0_p; sum_iter+=iter_p; if(iter_p>worst)worst=iter_p;
                    if(total_frames.load()-last_stat_presents>=90){
                        const double dt=(now_ms()-stat_t)/1e3;
                        const uint64_t dpres=total_frames.load()-last_stat_presents;
                        const double fps=dt>0?(double)dpres/dt:0;
                        const double src_fps=dt>0?(double)stat_ticks/dt:0;   // ticks/s in this mode
                        const uint64_t cons_now=stat_cons.load();
                        const double cons_fps=dt>0?(double)(cons_now-last_cons_p)/dt:0; last_cons_p=cons_now;
                        char arr_buf[48]="";
#ifdef _MSC_VER
                        if(cfg.capture_api==CA_WGC&&wgc_ctx){
                            const uint64_t a_now=wgc_ctx->arrived.load();
                            const double arr_fps2=dt>0?(double)(a_now-last_arrived)/dt:0; last_arrived=a_now;
                            std::snprintf(arr_buf,sizeof(arr_buf)," (arr %.0f)",arr_fps2);
                        } else
#endif
                        if(cfg.capture_api==CA_DD){
                            const uint64_t a_now=dd_arrived.load();
                            const double arr_fps2=dt>0?(double)(a_now-last_arrived)/dt:0; last_arrived=a_now;
                            std::snprintf(arr_buf,sizeof(arr_buf)," (arr %.0f)",arr_fps2);
                        }
                        char fgn_buf[20]=""; if(cfg.fg_auto) std::snprintf(fgn_buf,sizeof(fgn_buf)," | fg auto:%d",live_n_atomic.load());
                        const uint64_t skips_now=stat_skips.load();
                        const double skip_fps=dt>0?(double)(skips_now-last_skips_p)/dt:0; last_skips_p=skips_now;
                        const uint64_t cap_now=total_real.load();
                        const double cap_fps=dt>0?(double)(cap_now-last_cap_p)/dt:0; last_cap_p=cap_now;
                        // CAPTURE cross-check vs minimal_fg: in=cap_fps (real frames ingested/s = mfg 'in'); arrived=dd_arrived/s; dd_timeouts/s (DDA WAIT_TIMEOUT, 33ms granularity).
                        char rf_buf[32]="";   // WGC staging-ring-full drop rate (empty on DDA — 0 there by construction, like arr_buf)
#ifdef _MSC_VER
                        if(cfg.capture_api==CA_WGC&&wgc_ctx){ static uint64_t _lrf2=0; const uint64_t _rf=wgc_ctx->ringfull.load();
                            std::snprintf(rf_buf,sizeof(rf_buf)," ringfull=%.0f/s",dt>0?(double)(_rf-_lrf2)/dt:0); _lrf2=_rf; }
#endif
                        { static uint64_t _lda2=0,_ldt2=0,_ldu2=0; const uint64_t _da=dd_acq.load(),_dto=dd_timeouts.load(),_du=dd_uniq.load();
                          const double _af=dt>0?(double)(_da-_lda2)/dt:0,_tf=dt>0?(double)(_dto-_ldt2)/dt:0,_uf=dt>0?(double)(_du-_ldu2)/dt:0; _lda2=_da; _ldt2=_dto; _ldu2=_du;
                          std::printf("[ra-cap] in=%.0f/s | acq=%.0f/s uniq=%.0f/s dd_timeouts=%.0f/s dd_lost=%llu%s\n",cap_fps,_af,_uf,_tf,(unsigned long long)dd_lost.load(),rf_buf); }
                        // FPS-OVERLAY (--fps-overlay) publish in=cap_fps (real-captured) / out=fps (presented) for the overlay (rounded to uint).
                        g_ov_in.store((uint32_t)(cap_fps+0.5)); g_ov_out.store((uint32_t)(fps+0.5));
                        const double uniq_fps=dt>0?(double)(uniq_ticks-last_uniq)/dt:0; last_uniq=uniq_ticks;
                        // ps marker — PresentSurface submit counters (non-WAP grid path).
                        char ps_buf[64]="";
                        { const double psok=dt>0?(double)(ps_ok-last_ps_ok)/dt:0; last_ps_ok=ps_ok;
                          std::snprintf(ps_buf,sizeof(ps_buf)," | ps %.0f/s ok=%llu to=%llu er=%llu",psok,
                              (unsigned long long)ps_ok,(unsigned long long)ps_timeout,(unsigned long long)ps_err); }
                        // slip: paced_wait_P slip vs the tick (avg/max ms, target ~0).
                        const double slip_avg=slip_n?slip_sum/(double)slip_n:0.0, slip_mx=slip_max;
                        std::printf("[ra] %.1f fps (present) | tick %.0f/s%s | cap %.0f/s | cons %.0f/s | skip %.1f/s | uniq %.0f/s | iter %.2f/worst %.2fms | lat %.1fms | slip %.2f/max %.2fms%s%s\n",
                            fps,src_fps,arr_buf,cap_fps,cons_fps,skip_fps,uniq_fps,
                            sum_iter/(double)(stat_ticks>0?stat_ticks:1),worst,lat_ema_ms,
                            slip_avg,slip_mx,fgn_buf,ps_buf);
                        // --vblend: per-second lookahead-fired rate (≥1/pair confirms the next-fresher pair was available).
                        const double vbhit_fps=dt>0?(double)(vblend_hit-last_vblend_hit)/dt:0; last_vblend_hit=vblend_hit;
                        if(dbgN){ std::printf("[ra]   DIAG D=%.1fms phase=%.3f gen_back=%.2f T_src=%.2fms span=%.2f vbhit=%.0f/s\n",
                            dbgD_sum/(double)dbgN,dbgPh_sum/(double)dbgN,dbgGB_sum/(double)dbgN,dbgTsrc_sum/(double)dbgN,dbgSpan_sum/(double)dbgN,vbhit_fps); }
                        dbgD_sum=dbgPh_sum=dbgGB_sum=dbgTsrc_sum=dbgSpan_sum=0.0; dbgN=0;
                        sum_iter=0; worst=0; slip_sum=slip_max=0; slip_n=0;
                        stat_ticks=0;
                        stat_t=now_ms(); last_stat_presents=total_frames.load();
                    }
                }
                if(alog){ std::fclose(alog); std::printf("[ra] --arrival-log: %llu tick lines -> %s\n",(unsigned long long)alog_n,cfg.arrival_log); }   // (R1) the replay oracle
                if(pdhQuery) PdhCloseQuery(pdhQuery);   // release the PDH query on P exit
                // FPS-OVERLAY (--fps-overlay) tear down the overlay pipeline on P exit (the loop drained, the last
                // present's fBridge was waited synchronously → no in-flight use). Null-safe (all VK_NULL_HANDLE when off).
                if(ov_pipeline) vkDestroyPipeline(A.dev,ov_pipeline,nullptr);
                if(ov_pl_layout) vkDestroyPipelineLayout(A.dev,ov_pl_layout,nullptr);
                if(ov_pool) vkDestroyDescriptorPool(A.dev,ov_pool,nullptr);   // frees ov_set
                if(ov_dsl) vkDestroyDescriptorSetLayout(A.dev,ov_dsl,nullptr);
                return;   // the output-clock (timer) path is the only present loop
            }
            // The output-clock loop above (the panel-cadence present loop) always returns, so this
            // point is unreachable; the PDH query is already closed at that return.
}
// Made with my soul - Swately <3
