// PhyriadFG — src/ingest/ingest.cpp : STAGE 2 (INGEST), moved by anchor from run_capture (R6 step 1; see
// ingest.hpp). Both bodies below are the pre-extraction text VERBATIM — nothing was retyped; the reference is
// r6/capture_pre_extract.cpp and the measure is in records/R6_GATE.md §1. Made with my soul - Swately <3
#include "ingest/ingest.hpp"
#include "capture/capture.hpp"
#include "capture/wgc_ctx.hpp"
#include "core/fg_context.hpp"
#include "core/vk_util.hpp"
#include "core/globals.hpp"
#include "cli/cli.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <vector>

namespace pfg::ingest {

// ── convert_and_publish — the serial path's tail (capture.cpp:703-793 pre-extraction). TRANSFORMS: none; the
//    captured names are rebound below by the same lines run_capture uses, and the two locals are parameters. ──
void convert_and_publish(FgContext& ctx, uint32_t cap_rot180, int s) {
    auto& cfg = ctx.cfg;
    auto& c_seq = ctx.c_seq;
    auto& d = ctx.d;
    auto& NAT_W = ctx.NAT_W;
    auto& NAT_H = ctx.NAT_H;
    auto& Astage = ctx.Astage;
    auto& c_slots = ctx.c_slots;
    auto& use_igpu_convert = ctx.use_igpu_convert;
    auto& cmdA = ctx.cmdA;
    auto& Anative = ctx.Anative;
    auto& Awork = ctx.Awork;
    auto& cvPipe = ctx.cvPipe;
    auto& cvLayout = ctx.cvLayout;
    auto& cvSet = ctx.cvSet;
    auto& IS_HDR = ctx.IS_HDR;
    auto& WW = ctx.WW;
    auto& WH = ctx.WH;
    auto& hR_a = ctx.hR_a;
    auto& A = ctx.A;
    auto& single_gpu = ctx.single_gpu;
    auto& a_q2_mtx = ctx.a_q2_mtx;
    auto& fA = ctx.fA;
    auto& c_conv_us = ctx.c_conv_us;
    auto& cmdG = ctx.cmdG;
    auto& cpPipe = ctx.cpPipe;
    auto& fpipe = ctx.fpipe;
    auto& G = ctx.G;
    auto& fG = ctx.fG;
    auto& g_q_mtx = ctx.g_q_mtx;
    auto& hostFIELD = ctx.hostFIELD;
    auto& hostR = ctx.hostR;
    auto& c_cv = ctx.c_cv;
                if(!use_igpu_convert){
                    vkResetCommandBuffer(cmdA,0);
                    VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdA,&bi);
                    img_barrier(cmdA,Anative.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
                    { VkBufferImageCopy cp=full_bic(NAT_W,NAT_H); vkCmdCopyBufferToImage(cmdA,Astage.buf,Anative.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }
                    img_barrier(cmdA,Anative.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
                    img_barrier(cmdA,Awork.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
                    vkCmdBindPipeline(cmdA,VK_PIPELINE_BIND_POINT_COMPUTE,cvPipe); vkCmdBindDescriptorSets(cmdA,VK_PIPELINE_BIND_POINT_COMPUTE,cvLayout,0,1,&cvSet,0,nullptr);
                    struct{uint32_t is_hdr;float exposure;uint32_t rot180;}pcv{IS_HDR?1u:0u,1.f,cap_rot180}; vkCmdPushConstants(cmdA,cvLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcv),&pcv);
                    vkCmdDispatch(cmdA,(WW+7)/8,(WH+7)/8,1);
                    img_barrier(cmdA,Awork.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
                    { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(cmdA,Awork.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hR_a[s].buf,1,&cp); }
                    // Crash safety: when convert runs on the PRIMARY (--convert-gpu primary → use_igpu_convert
                    // false, so we are in THIS branch) route C's convert submit off A.q (P-exclusive) to A.q2
                    // (same-family, lock-free; cmdA is A.pool-bound). Default (--convert-gpu igpu) never enters
                    // this branch. A.q2!=A.q gates the q2 path (same-family split-queue forced on A).
                    vkEndCommandBuffer(cmdA);
                    const double tcv0=now_ms();   // time the A-convert (mirrors the iGPU branch's c_conv_us) so [lat-trace] conv is NON-ZERO on the single-GPU (A-convert) topology — the load-bearing FG-slice measurement. Measurement-only.
                    // Under single_gpu the F-thread ALSO submits flow to A.q2, so serialize C's convert submit
                    // behind a_q2_mtx (the only crash-safe lane for two threads on one VkQueue handle). The
                    // default --convert-gpu primary on a multi-GPU rig keeps flow on B.q → no race → no lock (byte-identical).
                    if(cfg.convert_gpu==CG_PRIMARY && A.q2!=A.q){
                        if(single_gpu){ std::lock_guard<std::mutex> lk(a_q2_mtx); submit_wait_q2(A,cmdA,fA); }
                        else submit_wait_q2(A,cmdA,fA);
                    } else submit_wait(A,cmdA,fA);
                    { const double dt=now_ms()-tcv0;
                      const uint64_t prev=c_conv_us.load();
                      c_conv_us.store(prev?(uint64_t)((double)prev*0.8+dt*1000.0*0.2):(uint64_t)(dt*1000.0)); }
                } else {
                    vkResetCommandBuffer(cmdG,0);
                    VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdG,&bi);
                    vkCmdBindPipeline(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,cpPipe[s].pipe);
                    vkCmdBindDescriptorSets(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,cpPipe[s].layout,0,1,&cpPipe[s].set,0,nullptr);
                    const bool is_bgra=(d.fmt==DXGI_FORMAT_B8G8R8A8_UNORM);
                    // is_hdr selects the FP16 scRGB tone-map branch (8 bytes/px src);
                    // exposure mirrors the A-path hdr_convert (nominal 1.0).
                    struct{uint32_t groups;uint32_t is_bgra;uint32_t px;uint32_t is_hdr;float exposure;uint32_t rot180;}
                        pcg{(WW*WH+3u)/4u,(uint32_t)is_bgra,WW*WH,IS_HDR?1u:0u,1.f,cap_rot180};
                    vkCmdPushConstants(cmdG,cpPipe[s].layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcg),&pcg);
                    vkCmdDispatch(cmdG,(pcg.groups+63)/64,1,1);
                    if(cfg.igpu_field){   // 2nd G.q2 dispatch — contour field over hR_g into hFIELD_g
                        VkMemoryBarrier mb{}; mb.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER; mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; mb.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                        vkCmdPipelineBarrier(cmdG,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&mb,0,nullptr,0,nullptr);
                        vkCmdBindPipeline(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,fpipe[s].pipe);
                        vkCmdBindDescriptorSets(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,fpipe[s].layout,0,1,&fpipe[s].set,0,nullptr);
                        struct{uint32_t w,h,edge_thr,pad;}pcf{(uint32_t)WW,(uint32_t)WH,cfg.igpu_field_thr,0u};
                        vkCmdPushConstants(cmdG,fpipe[s].layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcf),&pcf);
                        vkCmdDispatch(cmdG,(WW*WH+63)/64,1,1);
                    }
                    vkEndCommandBuffer(cmdG);
                    vkResetFences(G.dev,1,&fG);
                    const double tcv0=now_ms();   // C's G-queue round-trip (contention probe)
                    { VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
                      si.commandBufferCount=1; si.pCommandBuffers=&cmdG;
                      // Submit to G.q2. When it's a real second queue C is its ONLY submitter →
                      // externally synchronized by construction, no g_q_mtx. Only the shared fallback
                      // (q2==q) still needs the lock (P also submits to that queue).
                      if(G.q2!=G.q){ vkQueueSubmit(G.q2,1,&si,fG); }
                      else { std::lock_guard<std::mutex> lk(g_q_mtx); vkQueueSubmit(G.q,1,&si,fG); } }
                    vk_wait_live(G.dev,fG);
                    { const double dt=now_ms()-tcv0;
                      const uint64_t prev=c_conv_us.load();
                      c_conv_us.store(prev?(uint64_t)((double)prev*0.8+dt*1000.0*0.2):(uint64_t)(dt*1000.0)); }
                    // --igpu-field-verify: CPU re-derive the Sobel field + compare to the GPU field (byte oracle).
                    // El Sobel CPU se MUESTREA con paso 16 (~8K px a 1080p) cada frame → coste sub-ms constante,
                    // sin pico (un full-frame O(W×H) amortizado dejaría un pico periódico ~20-40ms). El compute
                    // corre cada frame; sólo el printf se limita a ~1/120 (vfy_n) para no inundar el log. El
                    // conteo de diferencias es sobre los píxeles MUESTREADOS. Oráculo debug default-off.
                    static uint64_t vfy_n=0;
                    if(cfg.igpu_field && cfg.igpu_field_verify && hostFIELD[s]){
                        const uint32_t* src=(const uint32_t*)hostR[s];
                        const uint32_t* gpu=(const uint32_t*)hostFIELD[s];
                        auto lum=[&](int x,int y)->float{ const uint32_t px=src[(size_t)y*WW+x]; return 0.299f*(float)(px&0xffu)+0.587f*(float)((px>>8)&0xffu)+0.114f*(float)((px>>16)&0xffu); };
                        uint64_t npx=0,ndiff=0; uint32_t dmax=0;
                        for(int y=0;y<(int)WH;y+=16) for(int x=0;x<(int)WW;x+=16){
                            const int xm=x>0?x-1:0, xp=x+1<(int)WW?x+1:(int)WW-1, ym=y>0?y-1:0, yp=y+1<(int)WH?y+1:(int)WH-1;
                            const float gx=(lum(xp,ym)+2.f*lum(xp,y)+lum(xp,yp))-(lum(xm,ym)+2.f*lum(xm,y)+lum(xm,yp));
                            const float gy=(lum(xm,yp)+2.f*lum(x,yp)+lum(xp,yp))-(lum(xm,ym)+2.f*lum(x,ym)+lum(xp,ym));
                            const float mag=std::sqrt(gx*gx+gy*gy)*0.25f;
                            const uint32_t dist=(uint32_t)(mag<0.f?0.f:(mag>255.f?255.f:mag));
                            const uint32_t cpu=dist | (((dist>=cfg.igpu_field_thr)?1u:0u)<<8);
                            const uint32_t g=gpu[(size_t)y*WW+x]&0xffffu;
                            const uint32_t diff=(cpu>g)?cpu-g:g-cpu; if(diff){++ndiff; if(diff>dmax)dmax=diff;} ++npx;
                        }
                        if((vfy_n++%120u)==0u)
                            std::printf("[ra] igpu-field-verify[slot %d]: %llu/%llu px differ (muestreo step-16), max|d|=%u (CPU Sobel vs GPU)\n",s,(unsigned long long)ndiff,(unsigned long long)npx,dmax);
                    }
                }
                if(cfg.latency_trace) c_slots[s].t_pub_ms=now_ms();   // stamp publish instant; the seq_cst fetch_add below orders it for F (publish→consume wake = F's now − this)
                c_seq.fetch_add(1);
                c_cv.notify_all();
}

}  // namespace pfg::ingest


// ── --ingest-async: the convert WORKER thread ───────────────────────────────────────────────────
// Only spawned when cfg.ingest_async (DDA acquire branch OR WGC pickup branch — both deposit into
// the same raw ring; WGC_INGEST_ASYNC_PLAN.md). The capture thread's async branch has
// already deposited raw frames into the raw ring + published raw_seq; this worker DROP-TO-NEWEST
// converts the freshest published raw slot — the EXACT convert tail of run_capture, but reading the
// convert SRC from the chosen raw slot (A-path: the buffer handle in vkCmdCopyBufferToImage; iGPU
// path: one once-per-frame vkUpdateDescriptorSets re-pointing cpPipe[s] binding-0). It OWNS the
// convert state (cmdA/cmdG/fA/fG/Anative/Awork/cpPipe), so no command/fence duplication is needed.
// PROMPT publish: total_real + c_seq are bumped the instant the convert fence signals → the freshest
// converted frame reaches F/P promptly. The worker is joined in main() BEFORE any convert/Vulkan teardown.
void run_convert_worker(FgContext& ctx){
    auto& cfg = ctx.cfg;
    // (issue #1) misma corrección ROTATE180 que el path serial (ver run_capture) — el worker
    // empuja los MISMOS push-constants a los MISMOS pipelines de convert.
    const uint32_t cap_rot180 = (cfg.capture_api==CA_DD && ctx.d.cap_rot==(int)DXGI_MODE_ROTATION_ROTATE180) ? 1u : 0u;
    auto& raw_seq = ctx.raw_seq;
    auto& raw_busy = ctx.raw_busy;
    auto& raw_cv = ctx.raw_cv;
    auto& raw_mtx = ctx.raw_mtx;
    auto& raw_astage_a = ctx.raw_astage_a;
    auto& raw_astage_g = ctx.raw_astage_g;
    auto& raw_tcap = ctx.raw_tcap;
    auto& raw_lt_submit = ctx.raw_lt_submit;    // (R6) WGC-async lat-trace carry (0 on DDA → inert)
    auto& raw_lt_compose = ctx.raw_lt_compose;
    auto& lt_copy_us = ctx.lt_copy_us;
    auto& lt_compose_us = ctx.lt_compose_us;
    auto& c_seq = ctx.c_seq;
    auto& cap_slots = ctx.cap_slots;
    auto& c_slots = ctx.c_slots;
    auto& total_real = ctx.total_real;
    auto& c_cv = ctx.c_cv;
    auto& c_conv_us = ctx.c_conv_us;
    auto& use_igpu_convert = ctx.use_igpu_convert;
    auto& g_quit_threads = ctx.g_quit_threads;
    auto& cmdA = ctx.cmdA;
    auto& Anative = ctx.Anative;
    auto& Awork = ctx.Awork;
    auto& cvPipe = ctx.cvPipe;
    auto& cvLayout = ctx.cvLayout;
    auto& cvSet = ctx.cvSet;
    auto& IS_HDR = ctx.IS_HDR;
    auto& WW = ctx.WW;
    auto& WH = ctx.WH;
    auto& hR_a = ctx.hR_a;
    auto& A = ctx.A;
    auto& single_gpu = ctx.single_gpu;
    auto& a_q2_mtx = ctx.a_q2_mtx;
    auto& fA = ctx.fA;
    auto& cmdG = ctx.cmdG;
    auto& cpPipe = ctx.cpPipe;
    auto& fpipe = ctx.fpipe;
    auto& G = ctx.G;
    auto& fG = ctx.fG;
    auto& g_q_mtx = ctx.g_q_mtx;
    auto& hostFIELD = ctx.hostFIELD;
    auto& hostR = ctx.hostR;
    auto& d = ctx.d;
    auto& NAT_W = ctx.NAT_W;
    auto& NAT_H = ctx.NAT_H;
    auto& nat_bpp = ctx.nat_bpp;

    const VkDeviceSize ab_g=VkDeviceSize(NAT_W)*NAT_H*nat_bpp;   // the iGPU convert SRC range (cpipe_create's src_b)
    uint64_t last_converted=0;
    while(!g_quit&&!g_quit_threads.load()){
        {   // wait for a newer raw frame (or quit). The 5ms timeout is a quit safety-net: main sets
            // g_quit_threads + notifies WITHOUT raw_mtx, so a quit notify could be lost — the timeout
            // re-checks the loop condition within 5ms. In steady streaming raw_seq has already advanced
            // (newer frames published during the convert) so the predicate is true and wait returns at once.
            std::unique_lock<std::mutex> lk(raw_mtx);
            raw_cv.wait_for(lk,std::chrono::milliseconds(5),[&]{ return g_quit||g_quit_threads.load()||raw_seq.load()>last_converted; });
        }
        if(g_quit||g_quit_threads.load()) break;
        const uint64_t newest=raw_seq.load();
        if(newest<=last_converted) continue;   // spurious wake
        last_converted=newest;                 // DROP-TO-NEWEST: discard the backlog, convert only the freshest
        const int rk=(int)((newest-1u)%(uint64_t)kRawSlots);
        raw_busy.store(rk);                     // torn-read guard: the acquire will not overwrite rk until we clear it
        const int s=(int)(c_seq.load()%(uint64_t)cap_slots);   // output slot (same convention as the serial loop)
        c_slots[s].t_cap_ms=raw_tcap[rk];       // freshage anchor carried from acquire (parity with serial's t_cap)
        // (R6) WGC-async lat-trace carry: fold the ridden submit/compose stamps into the same EMAs the
        // serial tail feeds ([lat-trace] INVISIBLE copy/compose stay truthful in async mode). The >0
        // guards make this inert on DDA (its stamps are never written → 0).
        if(cfg.latency_trace){
            const double tc=raw_tcap[rk], sub=raw_lt_submit[rk], cmp=raw_lt_compose[rk];
            if(sub>0.0 && tc>sub){ const double cp=(tc-sub)*1000.0;
                const uint64_t pv=lt_copy_us.load(); lt_copy_us.store(pv?(uint64_t)((double)pv*0.8+cp*0.2):(uint64_t)cp); }
            if(cmp>0.0){ const uint64_t pv=lt_compose_us.load();
                lt_compose_us.store(pv?(uint64_t)((double)pv*0.8+cmp*0.2):(uint64_t)cmp); }
        }
        if(!use_igpu_convert){
            vkResetCommandBuffer(cmdA,0);
            VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdA,&bi);
            img_barrier(cmdA,Anative.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
            { VkBufferImageCopy cp=full_bic(NAT_W,NAT_H); vkCmdCopyBufferToImage(cmdA,raw_astage_a[rk].buf,Anative.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp); }   // SRC = the chosen raw slot (vs the single Astage)
            img_barrier(cmdA,Anative.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
            img_barrier(cmdA,Awork.img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_WRITE_BIT);
            vkCmdBindPipeline(cmdA,VK_PIPELINE_BIND_POINT_COMPUTE,cvPipe); vkCmdBindDescriptorSets(cmdA,VK_PIPELINE_BIND_POINT_COMPUTE,cvLayout,0,1,&cvSet,0,nullptr);
            struct{uint32_t is_hdr;float exposure;uint32_t rot180;}pcv{IS_HDR?1u:0u,1.f,cap_rot180}; vkCmdPushConstants(cmdA,cvLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcv),&pcv);
            vkCmdDispatch(cmdA,(WW+7)/8,(WH+7)/8,1);
            img_barrier(cmdA,Awork.img,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
            { VkBufferImageCopy cp=full_bic(WW,WH); vkCmdCopyImageToBuffer(cmdA,Awork.img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,hR_a[s].buf,1,&cp); }
            vkEndCommandBuffer(cmdA);
            const double tcv0=now_ms();
            if(cfg.convert_gpu==CG_PRIMARY && A.q2!=A.q){
                if(single_gpu){ std::lock_guard<std::mutex> lk(a_q2_mtx); submit_wait_q2(A,cmdA,fA); }
                else submit_wait_q2(A,cmdA,fA);
            } else submit_wait(A,cmdA,fA);
            { const double dt=now_ms()-tcv0;
              const uint64_t prev=c_conv_us.load();
              c_conv_us.store(prev?(uint64_t)((double)prev*0.8+dt*1000.0*0.2):(uint64_t)(dt*1000.0)); }
        } else {
            // iGPU path: re-point cpPipe[s] binding-0 (the convert SRC) to the chosen raw slot's G-import
            // (once per frame, NOT per pixel; the set is free — the previous convert's fence was waited).
            { VkDescriptorBufferInfo bi0{}; bi0.buffer=raw_astage_g[rk].buf; bi0.offset=0; bi0.range=ab_g;
              VkWriteDescriptorSet w0{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,cpPipe[s].set,0,0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,nullptr,&bi0,nullptr};
              vkUpdateDescriptorSets(G.dev,1,&w0,0,nullptr); }
            vkResetCommandBuffer(cmdG,0);
            VkCommandBufferBeginInfo bi{}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; vkBeginCommandBuffer(cmdG,&bi);
            vkCmdBindPipeline(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,cpPipe[s].pipe);
            vkCmdBindDescriptorSets(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,cpPipe[s].layout,0,1,&cpPipe[s].set,0,nullptr);
            const bool is_bgra=(d.fmt==DXGI_FORMAT_B8G8R8A8_UNORM);
            struct{uint32_t groups;uint32_t is_bgra;uint32_t px;uint32_t is_hdr;float exposure;uint32_t rot180;}
                pcg{(WW*WH+3u)/4u,(uint32_t)is_bgra,WW*WH,IS_HDR?1u:0u,1.f,cap_rot180};
            vkCmdPushConstants(cmdG,cpPipe[s].layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcg),&pcg);
            vkCmdDispatch(cmdG,(pcg.groups+63)/64,1,1);
            if(cfg.igpu_field){
                VkMemoryBarrier mb{}; mb.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER; mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; mb.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cmdG,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&mb,0,nullptr,0,nullptr);
                vkCmdBindPipeline(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,fpipe[s].pipe);
                vkCmdBindDescriptorSets(cmdG,VK_PIPELINE_BIND_POINT_COMPUTE,fpipe[s].layout,0,1,&fpipe[s].set,0,nullptr);
                struct{uint32_t w,h,edge_thr,pad;}pcf{(uint32_t)WW,(uint32_t)WH,cfg.igpu_field_thr,0u};
                vkCmdPushConstants(cmdG,fpipe[s].layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(pcf),&pcf);
                vkCmdDispatch(cmdG,(WW*WH+63)/64,1,1);
            }
            vkEndCommandBuffer(cmdG);
            vkResetFences(G.dev,1,&fG);
            const double tcv0=now_ms();
            { VkSubmitInfo si{}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
              si.commandBufferCount=1; si.pCommandBuffers=&cmdG;
              if(G.q2!=G.q){ vkQueueSubmit(G.q2,1,&si,fG); }
              else { std::lock_guard<std::mutex> lk(g_q_mtx); vkQueueSubmit(G.q,1,&si,fG); } }
            vk_wait_live(G.dev,fG);
            { const double dt=now_ms()-tcv0;
              const uint64_t prev=c_conv_us.load();
              c_conv_us.store(prev?(uint64_t)((double)prev*0.8+dt*1000.0*0.2):(uint64_t)(dt*1000.0)); }
            // El Sobel CPU se MUESTREA con paso 16 (~8K px a 1080p) cada frame → coste sub-ms constante,
            // sin pico (un full-frame O(W×H) amortizado dejaría un pico periódico ~20-40ms). El compute corre
            // cada frame; sólo el printf se limita a ~1/120 (vfy_n).
            static uint64_t vfy_n=0;
            if(cfg.igpu_field && cfg.igpu_field_verify && hostFIELD[s]){
                const uint32_t* src=(const uint32_t*)hostR[s];
                const uint32_t* gpu=(const uint32_t*)hostFIELD[s];
                auto lum=[&](int x,int y)->float{ const uint32_t px=src[(size_t)y*WW+x]; return 0.299f*(float)(px&0xffu)+0.587f*(float)((px>>8)&0xffu)+0.114f*(float)((px>>16)&0xffu); };
                uint64_t npx=0,ndiff=0; uint32_t dmax=0;
                for(int y=0;y<(int)WH;y+=16) for(int x=0;x<(int)WW;x+=16){
                    const int xm=x>0?x-1:0, xp=x+1<(int)WW?x+1:(int)WW-1, ym=y>0?y-1:0, yp=y+1<(int)WH?y+1:(int)WH-1;
                    const float gx=(lum(xp,ym)+2.f*lum(xp,y)+lum(xp,yp))-(lum(xm,ym)+2.f*lum(xm,y)+lum(xm,yp));
                    const float gy=(lum(xm,yp)+2.f*lum(x,yp)+lum(xp,yp))-(lum(xm,ym)+2.f*lum(x,ym)+lum(xp,ym));
                    const float mag=std::sqrt(gx*gx+gy*gy)*0.25f;
                    const uint32_t dist=(uint32_t)(mag<0.f?0.f:(mag>255.f?255.f:mag));
                    const uint32_t cpu=dist | (((dist>=cfg.igpu_field_thr)?1u:0u)<<8);
                    const uint32_t g=gpu[(size_t)y*WW+x]&0xffffu;
                    const uint32_t diff=(cpu>g)?cpu-g:g-cpu; if(diff){++ndiff; if(diff>dmax)dmax=diff;} ++npx;
                }
                if((vfy_n++%120u)==0u)
                    std::printf("[ra] igpu-field-verify[slot %d]: %llu/%llu px differ (muestreo step-16), max|d|=%u (CPU Sobel vs GPU)\n",s,(unsigned long long)ndiff,(unsigned long long)npx,dmax);
            }
        }
        raw_busy.store(-1);             // convert done — release the slot (the acquire may reuse it)
        if(cfg.latency_trace) c_slots[s].t_pub_ms=now_ms();   // stamp publish instant; the seq_cst fetch_add below orders it for F (parity with the serial publish)
        total_real.fetch_add(1);        // PROMPT publish
        c_seq.fetch_add(1);
        c_cv.notify_all();
    }
}
