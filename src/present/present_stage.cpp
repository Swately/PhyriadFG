// PhyriadFG — src/present/present_stage.cpp : STAGE 6 — PRESENT, the bodies (R4/X12).
//
// Every method body below is a stretch of the pre-R4 src/present/present.cpp, MOVED by scratchpad/r4_patch.py
// (which extracts the lines by anchor from the real file and inserts them here — never retyped). The only
// edits inside a moved body are the ones listed at its head as TRANSFORMS (a local renamed to a Tick field, a
// lambda call renamed to the method, `return` → `return false`); scratchpad/verbatim_r4.py measures the rest
// as byte-identical and lists every non-matching line for records/R4_GATE.md §2.
//
// The alias lines at the top of each method (`const Config& cfg = cfg_;` …) exist so the moved text keeps its
// legacy spellings (`cfg.`, `A.dev`, `ra_surface`, `bridge_w`) — the same E1 preamble idiom the rest of the
// tree uses.
// Made with my soul - Swately <3
#include "present/present_stage.hpp"
#include "control/cli.hpp"                // Config (cfg.async_present / pace_hard / shallow_queue / present_* …)
#include "core/globals.hpp"           // g_quit / g_quit_threads / g_device_lost / vk_live
#include "core/compat_reason.hpp"     // ra::compat::emit (the named-reason present-init bail)
#include <phyriad/hal/CpuWait.hpp>    // phyriad::hal::cpu_wait_for_ns / spin_hint
#include <chrono>
#include <cstdio>
#include <thread>

namespace pfg::present {

PresentStage::PresentStage(VDev& A, const Config& cfg, uint32_t& bridge_w, uint32_t& bridge_h, bool& bridge_use_km,
                           void*& bridge_nt, void*& hostMassPtr, std::atomic<uint64_t>& total_frames, std::atomic<bool>& g_quit_threads)
    : A_(A), cfg_(cfg), bridge_w_(bridge_w), bridge_h_(bridge_h), bridge_use_km_(bridge_use_km),
      bridge_nt_(bridge_nt), hostMassPtr_(hostMassPtr), total_frames_(total_frames), g_quit_threads_(g_quit_threads) {}

PresentStage::~PresentStage() { tdr_destroy(); }

// ── init: the PresentSurface, created ON THIS THREAD (legacy present.cpp:406-447) ────────────────────────
// TRANSFORMS: the two `return;` of the enclosing thread body → `return false;` (the caller returns).
bool PresentStage::init(void* wgc_target_hwnd) {
    const Config& cfg = cfg_; pp::PresentSurface& ra_surface = surface; std::atomic<bool>& g_quit_threads = g_quit_threads_;
    pp::PresentSurfaceDesc psd{};
    psd.monitor_index=cfg.pres_mon; psd.width=0; psd.height=0;  // full present-monitor extent
    // The EXACT binding: cfg.pres_hmon is pres_outputs[pres_mon].hmon, resolved in capture_init. It wins
    // over monitor_index inside the pillar, so the DXGI present-candidate index the FG printed and the
    // panel the surface opens on can no longer disagree (pick_monitor's ordering is bypassed). nullptr
    // (a detached candidate with no HMONITOR) falls back to monitor_index exactly as before.
    psd.monitor_handle=cfg.pres_hmon;
    psd.waitable=cfg.present_waitable; psd.sync_interval=(uint8_t)cfg.present_sync;  // B (default-off byte-identical)
    psd.present_colorspace=(uint8_t)cfg.present_colorspace;  // (default-off byte-identical)
    psd.present_format=(uint8_t)cfg.present_format;           // (default-off byte-identical) request the FP16 scRGB swapchain (the bridge above was built FP16 iff cfg.present_format==1)
    if(cfg.present_own_window){
        // the opaque own-window displayed flip plane (Style::OwnWindow) + the
        // captured game's HWND as the foreground-yield reference. Default-off
        // → psd.style stays DcompCt + game_hwnd stays null → byte-identical to the overlay.
        psd.style=pp::Style::OwnWindow;
        psd.game_hwnd=(void*)wgc_target_hwnd;   // null in monitor-capture mode → the plane is UNBOUND
        // State the binding once, at init: it decides whether the plane yields at all, and the yield
        // transition log downstream is meaningless without it (I-5).
        std::printf(psd.game_hwnd
            ? "[ra] own-window: plane BOUND to the captured window — it yields the panel while another app is in front.\n"
            : "[ra] own-window: plane UNBOUND (monitor capture — no --window target) — it never yields; quit PhyriadFG to release the panel.\n");
    }
    auto cr=pp::PresentSurface::create(psd);
    if(!cr){
        // fault containment: there is no in-thread fallback path — degrade by
        // signalling a clean quit with the precise failing reason (never a crash).
        std::printf("[ra] present-surface: PresentSurface::create FAILED (ErrorCode=%u) — no fallback path; quitting cleanly\n",
            (unsigned)cr.error().code);
        ra::compat::emit(ra::compat::ReasonCode::PRESENT_INIT_FAILED);   // named reason on the present-init bail
        g_quit_threads.store(true); g_quit=true; return false;
    }
    ra_surface=std::move(*cr); surface_ready=true;
    // Print the REAL style (the old line said "dcomp-ct+WDA" hardcoded — under
    // --present-own-window it misreported the own flip plane as the overlay). The yield clause is
    // read from psd.game_hwnd rather than hardcoded: since I-5 an UNBOUND plane never yields, and
    // the old fixed wording contradicted the own-window line printed immediately above it.
    const char* style_txt =
        !cfg.present_own_window ? "dcomp-ct+WDA"
        : psd.game_hwnd         ? "OWN-WINDOW flip plane (displayed only while the game/our window is FOREGROUND — watch the yield lines)"
                                : "OWN-WINDOW flip plane (UNBOUND — displayed for the whole run; it never yields)";
    std::printf("[ra] present: PresentSurface %s — click_through=%s capture_excluded=%s\n",
        style_txt, ra_surface.is_click_through()?"yes":"no", ra_surface.capture_excluded()?"yes":"no");
    // The producer bridge texture is FP16 iff cfg.present_format==1 (above), and the swapchain
    // was requested FP16 with the SAME flag. If the surface's soft fallback dropped to BGRA8 on
    // this rig (FP16 composition refused) while the bridge is FP16, the formats DISAGREE → the
    // CopyResource(BGRA8 backbuffer, FP16 bridge) would be a format mismatch → corrupt/black
    // present. The bridge cannot be rebuilt on this thread (it is created before the device/
    // threads split), so degrade by a NAMED clean quit rather than present garbage. Inert unless
    // --present-fp16/--hdr is set AND the rig refuses FP16.
    if(cfg.present_format==1 && !ra_surface.present_is_fp16()){
        std::printf("[ra] present-surface: FP16 swapchain REFUSED on this rig (fell back to BGRA8) but the bridge is FP16 — format disagreement; no in-thread rebuild path. Quitting cleanly. Run without --present-fp16/--hdr for the 8-bit present.\n");
        ra::compat::emit(ra::compat::ReasonCode::PRESENT_INIT_FAILED);
        g_quit_threads.store(true); g_quit=true; return false;
    }
    if(ra_surface.present_is_fp16())
        std::printf("[ra] present-surface: FP16 scRGB present ACTIVE (R16G16B16A16_FLOAT + G10_NONE_P709). HDR is inert without an HDR display + HDR content (coverage, not a default-path win).\n");
    return true;
}

// ── account: the submit-result accounting (legacy ps_account, present.cpp:459-470) ─────────────────────
// TRANSFORMS: none.
void PresentStage::account(const std::expected<void, phyriad::Error>& r) {
    std::atomic<uint64_t>& total_frames = total_frames_;
    if(r){ ++ps_ok; total_frames.fetch_add(1); return; }
    const auto code=r.error().code;
    if(code==phyriad::ErrorCode::Timeout){ ++ps_timeout; return; }
    if(code==phyriad::ErrorCode::ShuttingDown){
        if(!g_device_lost.exchange(true))
            std::printf("[ra] present-surface device loss (DXGI) -- graceful exit (the game keeps running; PhyriadFG is an external overlay)\n");
        g_quit=true; ++ps_err; return;
    }
    ++ps_err;

}

// ── present_front: the --pace-hard pin + the slot-0 submit (legacy bridge_present, present.cpp:485-525) ──
// TRANSFORMS: `ps_account(` → `account(`.
void PresentStage::present_front() {
    const Config& cfg = cfg_; pp::PresentSurface& ra_surface = surface;
    uint32_t& bridge_w = bridge_w_; uint32_t& bridge_h = bridge_h_; void*& bridge_nt = bridge_nt_;
    if(!surface_ready) return;
    // --pace-hard: the HARD present-target pin. This is the single
    // chokepoint right before Present, reached AFTER all the variable per-tick work W (upscale +
    // copy + blit + the warp fence wait) — so the present here naturally lands at ph_tgt + W with W
    // jittering (the ~4.28ms present-MASD). The pin holds the present back to a FIXED phase
    // ph_tgt + budget so successive presents are tick_period_ms apart (MASD→0) when W<=budget.
    // Reached only when --pace-hard + own-window AND ph_tgt was set this tick → flag-off it is dead
    // (ph_tgt stays 0.0) → byte-identical.
    if(cfg.pace_hard && cfg.present_own_window && ph_tgt>0.0){
        const double W = now_ms() - ph_tgt;                  // the variable work this tick took past the grid target
        if(W>0.0 && W<100.0) ph_w_ema = ph_w_ema>0.0 ? 0.9*ph_w_ema + 0.1*W : W;   // EMA(W), skip hitches (scene cut / device stall)
        const double ph_period_ms = 1000.0/(double)cfg.refresh_hz;   // one vblank period (tick_period_ms is out of scope here; cfg.refresh_hz is the same source)
        double budget = ph_w_ema + cfg.ph_spin_ms;          // EMA(work) + margin (reuse ph_spin_ms as the present-target margin)
        const double bud_cap = ph_period_ms - 0.001;        // FREEZE-FLOOR: never target more than ~1 vblank of latency
        if(budget > bud_cap) budget = bud_cap;
        const double present_target = ph_tgt + budget;
        const double rem = present_target - now_ms();
        // FREEZE-FLOOR: a target already past (rem<=0 → work overran the budget) or absurd
        // (rem>one period → bad clock/target) → present NOW, NEVER block; count the degrade. Else a
        // bounded sleep-then-spin (the paced_wait_P shape) coarse sleep to present_target−ph_spin_ms,
        // then the TSC spin-finish. The total wait is hard-capped <1 vblank by the budget clamp +
        // this rem>period guard, so the present thread can never be held past a vblank while displayed.
        if(rem<=0.0 || rem>ph_period_ms){
            ++ph_overshoot;
        } else {
            if(rem > cfg.ph_spin_ms + 0.5){
                std::this_thread::sleep_until(std::chrono::steady_clock::now()
                    + std::chrono::duration<double,std::milli>(rem - cfg.ph_spin_ms));
            }
            const double spin_ms = present_target - now_ms();
            if(spin_ms>0.0){
                double sm=spin_ms; if(sm>cfg.ph_spin_ms) sm=cfg.ph_spin_ms;   // bound the busy-wait (efficiency mandate)
                phyriad::hal::cpu_wait_for_ns((uint64_t)(sm*1e6));
            }
            ++ph_held;
        }
        ph_tgt = 0.0;   // consume this tick's target (a second bridge_present in the same tick won't re-pin)
    }
    pp::SharedFrameHandle h{ bridge_nt, /*key*/0, bridge_w, bridge_h };
    account(ra_surface.submit(h));

}

// ── poll_inflight: the async preamble (legacy present.cpp:979-986; the SAME text at :1683-1686 in rfp_present
//    minus the mass read — one body, two callers; rfp passes presented_out = nullptr) ──────────────────────
// TRANSFORMS: none.
void PresentStage::poll_inflight(uint32_t* presented_out) {
    const Config& cfg = cfg_; VDev& A = A_; void*& hostMassPtr = hostMassPtr_;
    const bool ap = cfg.async_present;
    const int i0 = async_inflight;   // R4b: a promotion below clears it — that is a FRESH frame for the next present
    struct Promoted { PresentStage& s; int i0; ~Promoted(){ if(i0 >= 0 && s.async_inflight < 0) s.on_promoted(i0); } } _p{ *this, i0 };
    if(ap && async_inflight>=0){
        const VkResult fs=vkGetFenceStatus(A.dev, bslot[async_inflight].fence); vk_live(fs);   // DEVICE_LOST -> g_quit (else !=VK_SUCCESS is read as "not ready" -> spins forever, never exits)
        if(fs==VK_SUCCESS){
            if(presented_out) *presented_out = hostMassPtr ? *(uint32_t*)hostMassPtr : 0u;
            async_front = async_inflight; async_inflight = -1;
        }
    }

}

// ── begin: the slot decision for this tick (legacy present.cpp:988-1002; rfp :1690-1697) ───────────────
// TRANSFORMS: `do_warp` = (d == Warp); the rdrop count gated on count_drop (rfp never counted it);
// `VkCommandBuffer cmdBridge = …` / `VkFence fBridge = …` / `Img bridge_img = …` / `VkDeviceMemory bridge_mem = …`
// → the Tick fields (the RHS verbatim); the decision recorded.
Tick PresentStage::begin(Decision d, bool count_drop) {
    const Config& cfg = cfg_;
    const bool ap = cfg.async_present;
    const bool do_warp = (d == Decision::Warp);
    Tick tk;
    int back = 0;
    if(ap) back = (async_front==0) ? 1 : 0;
    const bool record_this_tick = (!ap || (async_inflight<0)) && do_warp;   // --fdrop: do_warp=false on an exact-dup drop → skip the warp record/submit (zero 4090 cost); the async fence-poll above STILL promotes async_front, and the present tail re-shows the completed front → state machine stays consistent
    // (observabilidad) rdrop = ticks async donde el warp SIGUE en vuelo → este tick re-presenta
    // el front VIEJO. uniq es CIEGO a esto (cuenta la SELECCIÓN pair/phase, no lo entregado):
    // un rdrop crónico entrega la mitad de las posiciones calculadas y la telemetría se ve sana.
    if(count_drop && ap && do_warp && async_inflight>=0) ++rdrop_ticks;
    // Slot shadows: on the off path these alias slot-0 = the today-resources (byte-identical); on the
    // async path they point at the chosen back slot. (Shadowing the captured names keeps the large
    // record body below textually unchanged — &cmdBridge etc. resolve to these locals.)
    tk.cmd = ap ? bslot[back].cmd  : bslot[0].cmd;
    tk.fence = ap ? bslot[back].fence: bslot[0].fence;
    tk.img = ap ? bslot[back].img : bslot[0].img;
    tk.mem = ap ? bslot[back].mem  : bslot[0].mem;
    tk.ap = ap; tk.back = back; tk.record = record_this_tick;
    tk.decision = (ap && do_warp && async_inflight>=0) ? Decision::Drop : d;
    last_decision_ = tk.decision;
    return tk;
}

// ── submit: sync = submit + the TDR-catching wait; async = submit + mark the slot in flight (legacy :1403-1407) ──
// TRANSFORMS: `ap` → `tk.ap`, `fBridge` → `tk.fence`, `back` → `tk.back`.
void PresentStage::submit(const Tick& tk, VkSubmitInfo& si) {
    VDev& A = A_;
    if(timing_) submit_ms_[tk.back] = now_ms();   // R4b: the host clock at submit (the latency's start)
    struct SyncRead { PresentStage& s; const Tick& tk; ~SyncRead(){ if(!tk.ap && s.timing_) s.timing_read(tk.back); } } _r{ *this, tk };   // sync: the fence completed inside
    if(!tk.ap){
        vkQueueSubmit(A.q,1,&si,tk.fence); vk_wait_live(A.dev,tk.fence);   // catch a TDR on the saturated 4090 present/warp -> g_quit -> graceful exit
    } else {
        // submit non-blocking; mark this slot in flight. The present thread polls it on a
        // LATER tick (the preamble above). No vkWaitForFences here — that wait IS the latency we shed.
        vkQueueSubmit(A.q,1,&si,tk.fence); async_inflight=tk.back;
    }

}

// ── submit_sync: the grid path's shared cmd/fence submit (legacy bridge_present_src, present.cpp:710) ───
// TRANSFORMS: `fBridge` → `fence`.
void PresentStage::submit_sync(VkFence fence, VkSubmitInfo& si) {
    VDev& A = A_;
    vkQueueSubmit(A.q,1,&si,fence); vk_wait_live(A.dev,fence);   // catch a TDR on the saturated 4090 present/warp -> g_quit -> graceful exit
}

// ── shallow_queue: the bounded early promote of THIS tick's warp (legacy present.cpp:1635-1650) ─────────
// TRANSFORMS: `ap` → `tk.ap`, `record_this_tick` → `tk.record`, `back` → `tk.back`.
void PresentStage::shallow_queue(const Tick& tk, uint32_t* presented_out) {
    const Config& cfg = cfg_; VDev& A = A_; void*& hostMassPtr = hostMassPtr_;
    const int i0 = async_inflight;   // R4b: an early promote below is a FRESH frame for this tick's present
    struct Promoted { PresentStage& s; int i0; ~Promoted(){ if(i0 >= 0 && s.async_inflight < 0) s.on_promoted(i0); } } _p{ *this, i0 };
    if(tk.ap && cfg.shallow_queue && cfg.sq_budget_us>0 && tk.record && async_inflight==tk.back && tk.back>=0){
        const double sq_cap_ms=(double)cfg.sq_budget_us/1000.0;
        const double sq_t0=now_ms();
        bool sq_done=false;
        do {
            const VkResult fs=vkGetFenceStatus(A.dev, bslot[tk.back].fence);                       // non-blocking poll
            if(!vk_live(fs)) break;                                                              // DEVICE_LOST -> g_quit -> exit the spin
            if(fs==VK_SUCCESS){ sq_done=true; break; }
            phyriad::hal::spin_hint();
        } while((now_ms()-sq_t0) < sq_cap_ms);
        if(sq_done){
            if(presented_out) *presented_out = hostMassPtr ? *(uint32_t*)hostMassPtr : 0u;   // mirror the preamble mass-read
            async_front = tk.back; async_inflight = -1; ++sq_hits;
        } else { ++sq_misses; }
    }

}

// ── present_tick: sync → the slot-0 present; async → the completed front slot (legacy :1651-1660; rfp :1715-1719) ──
// TRANSFORMS: `ap` → `tk.ap`, `bridge_present()` → `present_front()`, `ps_account(` → `account(`.
void PresentStage::present_tick(const Tick& tk) {
    pp::PresentSurface& ra_surface = surface; uint32_t& bridge_w = bridge_w_; uint32_t& bridge_h = bridge_h_;
    // R4b: FRESH = the slot presented now holds a warp completed since the previous present.
    //   async: a promotion happened since the last present (front_seq_ moved); sync: this tick recorded one.
    last_fresh_ = tk.ap ? (async_front >= 0 && front_seq_ != last_presented_seq_) : tk.record;
    last_presented_seq_ = front_seq_;
    if(last_fresh_){ ++fresh_ticks; presented_gpu_ms_ = last_gpu_ms_; presented_lat_ms_ = last_lat_ms_; }
    else { presented_gpu_ms_ = -1.0; presented_lat_ms_ = -1.0; }
    if(!tk.ap){
        present_front();   // presents slot-0 (bridge_nt)
    } else if(async_front>=0){
        // present the freshest COMPLETED slot (mirrors the bridge_present lambda body,
        // but with the front slot's NT handle). On startup (nothing completed yet) present nothing.
        if(surface_ready){
            pp::SharedFrameHandle h{ bslot[async_front].nt, /*key*/0, bridge_w, bridge_h };
            account(ra_surface.submit(h));   // ShuttingDown → device-loss exit
        }
    }

}

// ── flip_stats: the feedback edge 6 → 4 (the CSV row's MsBetweenDisplayChange reads it; PhaseClock may later) ──
FlipStats PresentStage::flip_stats() const {
    FlipStats fs;
    fs.ps_ok = ps_ok; fs.ps_timeout = ps_timeout; fs.ps_err = ps_err; fs.device_lost = g_device_lost.load();
    if(surface_ready){
        if(auto fst=surface.last_flip_qpc()){ fs.have_flip = true; fs.sync_qpc = fst->sync_qpc; fs.present_count = fst->present_count; }
    }
    return fs;
}

// ── R4b: the fresh / re-show bookkeeping + --warp-timing (INSTRUMENT plane; nothing here touches a pixel) ──
void PresentStage::on_promoted(int slot) {
    ++front_seq_;
    if(timing_) timing_read(slot);
}

bool PresentStage::timing_arm() {
    VDev& A = A_;
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(A.phys, &props);
    if(props.limits.timestampComputeAndGraphics == VK_FALSE || props.limits.timestampPeriod <= 0.0f) return false;
    ts_period_ns_ = (double)props.limits.timestampPeriod;
    VkQueryPoolCreateInfo qi{}; qi.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO; qi.queryType=VK_QUERY_TYPE_TIMESTAMP; qi.queryCount=4;   // 2 per slot
    if(vkCreateQueryPool(A.dev,&qi,nullptr,&ts_pool_)!=VK_SUCCESS) return false;
    timing_ = true;
    std::printf("[ra] --warp-timing: GPU timestamps around the warp batch armed (timestampPeriod %.3f ns) -- warp_gpu_ms / warp_lat_ms per fresh present in the CSV, EMAs in the stats line\n", ts_period_ns_);
    return true;
}

void PresentStage::timing_begin(VkCommandBuffer cmd, int slot) {
    if(!timing_) return;
    vkCmdResetQueryPool(cmd, ts_pool_, (uint32_t)slot * 2u, 2u);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ts_pool_, (uint32_t)slot * 2u);
}

void PresentStage::timing_end(VkCommandBuffer cmd, int slot) {
    if(!timing_) return;
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ts_pool_, (uint32_t)slot * 2u + 1u);
}

void PresentStage::timing_read(int slot) {
    VDev& A = A_;
    uint64_t ts[2] = { 0, 0 };
    const VkResult r = vkGetQueryPoolResults(A.dev, ts_pool_, (uint32_t)slot * 2u, 2u, sizeof ts, ts, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if(r == VK_SUCCESS && ts[1] >= ts[0]){
        last_gpu_ms_ = (double)(ts[1] - ts[0]) * ts_period_ns_ / 1.0e6;
        last_lat_ms_ = now_ms() - submit_ms_[slot];
        gpu_ema_ = gpu_ema_ > 0.0 ? gpu_ema_ * 0.9 + last_gpu_ms_ * 0.1 : last_gpu_ms_;
        lat_ema_ = lat_ema_ > 0.0 ? lat_ema_ * 0.9 + last_lat_ms_ * 0.1 : last_lat_ms_;
    } else { last_gpu_ms_ = -1.0; last_lat_ms_ = -1.0; }   // NOT_READY / DEVICE_LOST → NA (vk_live is the fence poll's business)
}

void PresentStage::timing_line(char* buf, size_t n) const {
    if(!timing_ || n == 0){ if(n) buf[0] = 0; return; }
    std::snprintf(buf, n, " gpu %.2fms sub2fence %.2fms", gpu_ema_, lat_ema_);
}

// ── --tdr-test N: the forced GPU hang (shaders/tdr_hang.comp), armed here, fired once when due ──────────
bool PresentStage::tdr_arm(const std::vector<uint32_t>& spv, int seconds) {
    VDev& A = A_;
    if(seconds <= 0) return false;
    const VkDescriptorSetLayoutBinding b0{ 0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    VkDescriptorSetLayoutCreateInfo dl{}; dl.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; dl.bindingCount=1u; dl.pBindings=&b0;
    if(vkCreateDescriptorSetLayout(A.dev,&dl,nullptr,&tdr_dsl_)!=VK_SUCCESS) return false;
    const VkPushConstantRange pcr{ VK_SHADER_STAGE_COMPUTE_BIT, 0u, 4u };
    VkPipelineLayoutCreateInfo pl{}; pl.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; pl.setLayoutCount=1u; pl.pSetLayouts=&tdr_dsl_; pl.pushConstantRangeCount=1u; pl.pPushConstantRanges=&pcr;
    if(vkCreatePipelineLayout(A.dev,&pl,nullptr,&tdr_pl_)!=VK_SUCCESS) return false;
    VkShaderModuleCreateInfo mci{}; mci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; mci.codeSize=spv.size()*sizeof(uint32_t); mci.pCode=spv.data();
    VkShaderModule mod=VK_NULL_HANDLE; if(vkCreateShaderModule(A.dev,&mci,nullptr,&mod)!=VK_SUCCESS) return false;
    VkPipelineShaderStageCreateInfo st{}; st.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; st.stage=VK_SHADER_STAGE_COMPUTE_BIT; st.module=mod; st.pName="main";
    VkComputePipelineCreateInfo cp{}; cp.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; cp.stage=st; cp.layout=tdr_pl_;
    const VkResult pr=vkCreateComputePipelines(A.dev,VK_NULL_HANDLE,1u,&cp,nullptr,&tdr_pipe_); vkDestroyShaderModule(A.dev,mod,nullptr);
    if(pr!=VK_SUCCESS) return false;
    if(!dbuf_create(A,4096,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,tdr_sink_)) return false;
    const VkDescriptorPoolSize psz{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u };
    VkDescriptorPoolCreateInfo pi{}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pi.maxSets=1u; pi.poolSizeCount=1u; pi.pPoolSizes=&psz;
    if(vkCreateDescriptorPool(A.dev,&pi,nullptr,&tdr_pool_)!=VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo dai{}; dai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; dai.descriptorPool=tdr_pool_; dai.descriptorSetCount=1u; dai.pSetLayouts=&tdr_dsl_;
    if(vkAllocateDescriptorSets(A.dev,&dai,&tdr_set_)!=VK_SUCCESS) return false;
    VkDescriptorBufferInfo bi{}; bi.buffer=tdr_sink_.buf; bi.offset=0; bi.range=VK_WHOLE_SIZE;
    const VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, tdr_set_, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bi, nullptr };
    vkUpdateDescriptorSets(A.dev,1u,&w,0,nullptr);
    tdr_seconds_ = seconds; tdr_t0_ = now_ms(); tdr_fired_ = false;
    std::printf("[ra] --tdr-test %d: ARMED -- a never-terminating compute dispatch will be recorded into the present-stage command buffer at T+%d s to force a GPU timeout (TDR) and prove the device-loss exit path\n", seconds, seconds);
    return true;
}

void PresentStage::tdr_maybe(VkCommandBuffer cmd) {
    if(tdr_pipe_==VK_NULL_HANDLE || tdr_fired_) return;
    if(now_ms() - tdr_t0_ < (double)tdr_seconds_ * 1000.0) return;
    tdr_fired_ = true;
    std::printf("[ra] --tdr-test: dispatching the GPU hang NOW (expect VK_ERROR_DEVICE_LOST within the TDR window, then the clean g_quit exit)\n");
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,tdr_pipe_);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,tdr_pl_,0,1,&tdr_set_,0,nullptr);
    const uint32_t seed = 1u;
    vkCmdPushConstants(cmd,tdr_pl_,VK_SHADER_STAGE_COMPUTE_BIT,0,4u,&seed);
    vkCmdDispatch(cmd,1,1,1);
}

void PresentStage::tdr_destroy() {
    VDev& A = A_;
    if(ts_pool_){ vkDestroyQueryPool(A.dev,ts_pool_,nullptr); ts_pool_=VK_NULL_HANDLE; timing_=false; }   // R4b: the --warp-timing pool goes with the stage
    if(tdr_pool_) vkDestroyDescriptorPool(A.dev,tdr_pool_,nullptr);
    if(tdr_pipe_) vkDestroyPipeline(A.dev,tdr_pipe_,nullptr);
    if(tdr_pl_)   vkDestroyPipelineLayout(A.dev,tdr_pl_,nullptr);
    if(tdr_dsl_)  vkDestroyDescriptorSetLayout(A.dev,tdr_dsl_,nullptr);
    hbuf_destroy(A,tdr_sink_);
    tdr_pool_=VK_NULL_HANDLE; tdr_pipe_=VK_NULL_HANDLE; tdr_pl_=VK_NULL_HANDLE; tdr_dsl_=VK_NULL_HANDLE;
}

}  // namespace pfg::present
