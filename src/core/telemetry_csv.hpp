// telemetry_csv.hpp — STAGE-100: comprehensive per-frame telemetry CSV export for PhyriadFG.
//
// Build contract: docs/planning/FG_TELEMETRY_PRIOR_ART.md (SOTA-grounded, FDP-conform).
// Emits TWO files (mirroring PresentMon's pmcap, F12): a RAW per-present CSV and a
// companion `-stats.csv` with the derived frame-pacing metrics. Three column groups:
//   (A) PresentMon CONSOLE-frontend names verbatim (F1/F2) for the subset we genuinely
//       compute; the columns we CANNOT compute from external capture (display scan-out,
//       input-to-photon, InstrumentedLatency) are emitted EMPTY (NA) — never a proxy (F5/F15).
//   (B) architecture-native columns no external tool can see (F13) — the value-add.
//   (C) per-device system telemetry under PresentMon names + `_<dev>` suffix (F7/F8),
//       from NVML (the two NVIDIA GPUs). iGPU/CPU telemetry = NA in this phase (honest gap).
//
// Real-time isolation (F9 + Phyriad efficiency mandate): the present loop ONLY appends a
// POD row to a lock-free SPSC ring (no NVML, no I/O). A dedicated LOW-PRIORITY drain thread
// samples telemetry at ~40 Hz, writes rows, and computes the stats file at stop().
//
// Made with my soul - Swately <3  (signature lives at the END of the file per §9)
#pragma once
#include <windows.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace phyriadfg {

// ── The per-present record the present loop fills (POD; values it already has in-register).
//    Any field left at its <0 sentinel is written as NA (empty cell).
struct CsvRow {
    uint64_t  frame_index = 0;
    long long qpc_present = 0;     // QueryPerformanceCounter at present (master clock)
    int       frame_type  = 0;     // 0 = Synthesized, 1 = Repeated (a freeze re-show)
    int       frz         = 0;     // 1 if this present was a freeze (real lap-freeze)
    int       fdrop       = 0;     // STAGE-105: 1 if this present was a --fdrop exact-duplicate discriminator drop (distinct from frz)
    int       cap_slots   = -1;    // active capture-ring depth this tick
    int       route_device = 0;    // 0=4090 1=1080Ti 2=iGPU 3=CPU (primary synthesis device)
    double    ms_in_present_api = -1.0;
    // W4 (TB-C9 fluidity PACING axis): the DISPLAY-FLIP interval (ms) for the own-window flip swapchain,
    //   computed by the present site from IDXGISwapChain::GetFrameStatistics (SyncQPCTime delta across
    //   presents where PresentCount advanced). This is the SOTA's binding pacing time — the display-flip
    //   instant, NOT the present-CALL time (MsBetweenPresents, which --pace-hard directly controls).
    //   <0 sentinel → NA (composed/non-own-window path or a GetFrameStatistics soft-fail), so the
    //   fluidity scorer (scripts/fg_testbench_fluidity.py) falls back to the MsBetweenPresents proxy (flagged).
    double    ms_between_display_change = -1.0;
    double    warp_ms     = -1.0;  // 4090 synthesis time (also written to MsGPUTime, approx)
    double    iter_ms     = -1.0;
    double    slip_ms     = -1.0;
    double    added_lat_ms = -1.0; // present_ts - capture_ts (our 'lat ms'); the non-PresentMon proxy
    double    pair_age_ms = -1.0;
    double    ring_occ    = -1.0;  // 0..1
    double    gme_dis_pct = -1.0;
    double    source_fps  = -1.0;  // real captured-frame rate (the honest input rate)
    double    output_fps  = -1.0;  // presented rate incl. synthesized
    double    cons_per_s  = -1.0;
    double    uniq_per_s  = -1.0;
    // ── TB-C9 (FLUIDITY / motion-placement axis): the per-present temporal-placement signal ──
    // disp_phase = the FINAL presented intra-pair interpolation phase t_use ∈ [0,1] (the position
    //   the warp synthesised WITHIN the selected source pair this tick; the displayed phase).
    // disp_src   = the displayed CONTENT-SOURCE-TIME in SOURCE-FRAME units = (pair_c − span) + t_use·span
    //   — i.e. WHICH source moment the presented (interpolated) frame depicts. This is the
    //   animation-time the field's "Animation Error" metric otherwise lacks for generated frames
    //   (PresentMon/GamersNexus: it cannot score generated frames because they carry no engine
    //   animation timestamp); the DETERMINISTIC TB-C1 source supplies it analytically, so this is
    //   a valid ground-truth for GENERATED frames too. The fluidity analyser differences disp_src
    //   against the display interval to get the placement error (see scripts/fg_testbench_fluidity.py).
    // Both stay at the <0 sentinel (→ NA) unless the present site populates them (only on the --csv path).
    double    disp_phase  = -1.0;  // displayed interpolation phase t_use ∈ [0,1]
    double    disp_src    = -1.0;  // displayed content-source-time (source-frame units)
};

class TelemetryCsv {
public:
    // path = the raw per-present CSV path; the stats file is <path-without-.csv>-stats.csv.
    bool start(const std::string& path, const std::string& app, unsigned int pid,
               const std::string& nvNameA, const std::string& nvNameB);
    bool active() const { return active_.load(); }

    // PRODUCER (present thread): lock-free, never blocks, never does I/O. Drops on overflow.
    void push(const CsvRow& r) {
        if (!active_.load()) return;
        const uint64_t w  = w_idx_.load();
        const uint64_t rd = r_idx_.load();
        if (w - rd >= CAP) { drops_.fetch_add(1); return; }
        ring_[w & MASK] = r;
        w_idx_.store(w + 1);
    }

    void stop();   // signal + join + flush + write the stats file (idempotent)
    ~TelemetryCsv() { stop(); }   // RAII: never let a joinable drain thread outlive scope (would std::terminate)

private:
    static constexpr uint64_t CAP = 8192, MASK = CAP - 1;
    std::vector<CsvRow> ring_;   // heap (CAP*~150B ≈ 1.2MB — must NOT live on the stack)
    std::atomic<uint64_t> w_idx_{0}, r_idx_{0}, drops_{0};
    std::atomic<bool>     active_{false}, stop_{false};
    std::thread           drain_;
    std::string           path_, stats_path_, app_;
    unsigned int          pid_ = 0;
    std::string           nvNameA_, nvNameB_;
    double                qfreq_ = 0.0;     // QPC ticks per second
    long long             qpc0_  = 0;       // recording start (for CPUStartTime)
    FILE*                 fp_    = nullptr;

    // ── session accumulation for the stats file (drain thread only) ─────────────
    std::vector<double>   ft_;              // MsBetweenPresents per present
    std::vector<double>   slice_ms_;        // STAGE-104: per-tick FG 4090 slice (warp+present wall, r.warp_ms>0)
    double                src_fps_sum_ = 0.0; uint64_t src_fps_n_ = 0;
    uint64_t              n_present_ = 0, n_freeze_ = 0, n_fdrop_ = 0;   // STAGE-105: n_fdrop_ = exact-dup discriminator drops (distinct from freezes)
    long long             qpc_first_ = 0, qpc_last_ = 0;

    // ── STAGE-103 WATCHDOG: present-thread stall/freeze/hang detection (drain-thread-side) ──
    // The present thread is the CSV PRODUCER, so a present-thread hang can never be logged by
    // the present thread itself — a resumed hang shows only as one buried huge-frametime row, a
    // TERMINAL hang shows as nothing (the gap until exit). The drain thread runs INDEPENDENTLY,
    // so it sees w_idx stop advancing and flags the stall (incl. terminal), with the last GPU
    // telemetry = the WHY. kStallMs = the freeze threshold (~25 ticks @240Hz; >> normal jitter).
    static constexpr double kStallMs = 100.0;
    double                max_stall_ms_ = 0.0, total_stall_ms_ = 0.0;
    uint64_t              stall_count_ = 0;

    // ── per-device telemetry snapshot (sampled on the drain thread) ─────────────
    struct Tele { double util=-1, power_w=-1, temp_c=-1, clk_mhz=-1, memclk_mhz=-1, vram_mib=-1; };
    Tele teleA_{}, teleB_{};   // A = 4090, B = 1080 Ti

    // ── NVML (drain-thread-owned; dynamically loaded) ───────────────────────────
    HMODULE nvmlLib_ = nullptr;
    void*   nvA_ = nullptr; void* nvB_ = nullptr;
    struct NvmlMem  { unsigned long long total, free, used; };
    struct NvmlUtil { unsigned int gpu, mem; };
    typedef int (*PFN_i)();
    typedef int (*PFN_cnt)(unsigned int*);
    typedef int (*PFN_idx)(unsigned int, void**);
    typedef int (*PFN_name)(void*, char*, unsigned int);
    typedef int (*PFN_util)(void*, NvmlUtil*);
    typedef int (*PFN_pow)(void*, unsigned int*);
    typedef int (*PFN_clk)(void*, int, unsigned int*);
    typedef int (*PFN_temp)(void*, int, unsigned int*);
    typedef int (*PFN_mem)(void*, NvmlMem*);
    PFN_i nvShut_=nullptr; PFN_util nvUtil_=nullptr; PFN_pow nvPow_=nullptr;
    PFN_clk nvClk_=nullptr; PFN_temp nvTemp_=nullptr; PFN_mem nvMem_=nullptr;

    void drain_loop();
    void nvml_init();
    void nvml_sample(void* dev, Tele& t);
    void write_header();
    void write_row(const CsvRow& r, long long& prev_qpc, bool& have_prev);
    void write_stats();
    double now_ms() const { LARGE_INTEGER q; QueryPerformanceCounter(&q); return (double)(q.QuadPart - qpc0_) / qfreq_ * 1000.0; }
};

// ── derived-metric helpers (FG_TELEMETRY_PRIOR_ART §4; CapFrameX formulas, F10/F11) ──
namespace tele_detail {
    // 1% low INTEGRAL (the recommended default): sort desc, accumulate frametimes until the
    // cumulative sum reaches frac*total_time, report 1000/boundary_frametime.
    inline double low_integral(std::vector<double> ft, double frac) {
        if (ft.empty()) return -1.0;
        double T = 0.0; for (double f : ft) T += f;
        std::sort(ft.begin(), ft.end(), std::greater<double>());
        double acc = 0.0; for (double f : ft) { acc += f; if (acc >= frac * T) return 1000.0 / f; }
        return 1000.0 / ft.back();
    }
    // x% low AVERAGE: mean of the worst (largest) ceil(frac*N) frametimes -> FPS.
    inline double low_average(std::vector<double> ft, double frac) {
        if (ft.empty()) return -1.0;
        std::sort(ft.begin(), ft.end(), std::greater<double>());
        size_t k = (size_t)std::ceil(frac * (double)ft.size()); if (k < 1) k = 1;
        double s = 0.0; for (size_t i = 0; i < k; ++i) s += ft[i];
        return 1000.0 / (s / (double)k);
    }
    // Frametime percentile (ascending): value for which x% of values are smaller.
    inline double percentile_ft(std::vector<double> ft, double pct) {
        if (ft.empty()) return -1.0;
        std::sort(ft.begin(), ft.end());
        size_t idx = (size_t)std::lround((pct / 100.0) * (double)(ft.size() - 1));
        return ft[idx];
    }
    // Stutter vs a TIME-BASED trailing moving average (window ~1000 ms of frametime, documented).
    // Returns {count_pct, time_pct, adaptive_stdev}. factor default 2.5 (CapFrameX).
    struct StutterOut { double count_pct=-1, time_pct=-1, adaptive_stdev=-1; };
    inline StutterOut stutter(const std::vector<double>& ft, double factor, double win_ms) {
        StutterOut o; const size_t N = ft.size(); if (N < 2) return o;
        size_t lo = 0; double winsum = 0.0;
        size_t stut_cnt = 0; double stut_time = 0.0, T = 0.0;
        double resid2 = 0.0;
        for (size_t i = 0; i < N; ++i) {
            winsum += ft[i]; T += ft[i];
            while (winsum - ft[lo] > win_ms && lo < i) { winsum -= ft[lo]; ++lo; }
            const double movavg = winsum / (double)(i - lo + 1);
            if (ft[i] > factor * movavg) { ++stut_cnt; stut_time += ft[i]; }
            const double d = ft[i] - movavg; resid2 += d * d;
        }
        o.count_pct = 100.0 * (double)stut_cnt / (double)N;
        o.time_pct  = 100.0 * stut_time / T;
        o.adaptive_stdev = std::sqrt(resid2 / (double)(N - 1));
        return o;
    }
}

// ── implementation ──────────────────────────────────────────────────────────────
inline bool TelemetryCsv::start(const std::string& path, const std::string& app, unsigned int pid,
                                const std::string& nvNameA, const std::string& nvNameB) {
    path_ = path; app_ = app; pid_ = pid; nvNameA_ = nvNameA; nvNameB_ = nvNameB;
    // stats path: strip a trailing .csv, append -stats.csv
    stats_path_ = path_;
    if (stats_path_.size() >= 4 && stats_path_.substr(stats_path_.size() - 4) == ".csv")
        stats_path_ = stats_path_.substr(0, stats_path_.size() - 4);
    stats_path_ += "-stats.csv";
    LARGE_INTEGER fq; QueryPerformanceFrequency(&fq); qfreq_ = (double)fq.QuadPart;
    LARGE_INTEGER q0; QueryPerformanceCounter(&q0); qpc0_ = q0.QuadPart;
    fp_ = std::fopen(path_.c_str(), "wb");
    if (!fp_) { std::printf("[ra] --csv: cannot open '%s' for writing\n", path_.c_str()); return false; }
    ring_.resize(CAP);                 // heap-allocate the SPSC ring before the producer can run
    ft_.reserve(1u << 18);
    slice_ms_.reserve(1u << 18);       // STAGE-104: FG 4090-slice accumulator (mirrors ft_)
    active_.store(true);
    drain_ = std::thread([this]{ this->drain_loop(); });
    std::printf("[ra] --csv: telemetry export armed -> %s (+ %s); drain thread up\n", path_.c_str(), stats_path_.c_str());
    return true;
}

inline void TelemetryCsv::stop() {
    if (!active_.load()) return;
    stop_.store(true);
    if (drain_.joinable()) drain_.join();
    active_.store(false);
}

inline void TelemetryCsv::nvml_init() {
    nvmlLib_ = LoadLibraryW(L"nvml.dll");
    if (!nvmlLib_) nvmlLib_ = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
    if (!nvmlLib_) return;
    auto fInit  = (PFN_i)  GetProcAddress(nvmlLib_, "nvmlInit_v2");
    auto fCount = (PFN_cnt)GetProcAddress(nvmlLib_, "nvmlDeviceGetCount_v2");
    auto fByIdx = (PFN_idx)GetProcAddress(nvmlLib_, "nvmlDeviceGetHandleByIndex_v2");
    auto fName  = (PFN_name)GetProcAddress(nvmlLib_, "nvmlDeviceGetName");
    nvShut_ = (PFN_i)   GetProcAddress(nvmlLib_, "nvmlShutdown");
    nvUtil_ = (PFN_util)GetProcAddress(nvmlLib_, "nvmlDeviceGetUtilizationRates");
    nvPow_  = (PFN_pow) GetProcAddress(nvmlLib_, "nvmlDeviceGetPowerUsage");
    nvClk_  = (PFN_clk) GetProcAddress(nvmlLib_, "nvmlDeviceGetClockInfo");
    nvTemp_ = (PFN_temp)GetProcAddress(nvmlLib_, "nvmlDeviceGetTemperature");
    nvMem_  = (PFN_mem) GetProcAddress(nvmlLib_, "nvmlDeviceGetMemoryInfo");
    if (!fInit || !fCount || !fByIdx || !fName || fInit() != 0) { FreeLibrary(nvmlLib_); nvmlLib_ = nullptr; return; }
    std::string la = nvNameA_, lb = nvNameB_;
    for (auto& c : la) c = (char)std::tolower((unsigned char)c);
    for (auto& c : lb) c = (char)std::tolower((unsigned char)c);
    unsigned int n = 0; fCount(&n);
    for (unsigned int i = 0; i < n; ++i) {
        void* h = nullptr; if (fByIdx(i, &h) != 0 || !h) continue;
        char nm[96] = {}; if (fName(h, nm, (unsigned int)sizeof(nm)) != 0) continue;
        std::string ln = nm; for (auto& c : ln) c = (char)std::tolower((unsigned char)c);
        if (!nvA_ && !la.empty() && (la.find(ln) != std::string::npos || ln.find(la) != std::string::npos)) nvA_ = h;
        else if (!nvB_ && !lb.empty() && (lb.find(ln) != std::string::npos || ln.find(lb) != std::string::npos)) nvB_ = h;
    }
}

inline void TelemetryCsv::nvml_sample(void* dev, Tele& t) {
    if (!dev) { t = Tele{}; return; }
    NvmlUtil u{}; if (nvUtil_ && nvUtil_(dev, &u) == 0) { t.util = (double)u.gpu; }
    unsigned int p = 0; if (nvPow_ && nvPow_(dev, &p) == 0) t.power_w = (double)p / 1000.0;          // mW -> W (F8)
    unsigned int c = 0; if (nvClk_ && nvClk_(dev, 0 /*NVML_CLOCK_GRAPHICS*/, &c) == 0) t.clk_mhz = (double)c;
    unsigned int m = 0; if (nvClk_ && nvClk_(dev, 2 /*NVML_CLOCK_MEM*/, &m) == 0) t.memclk_mhz = (double)m;
    unsigned int tm = 0; if (nvTemp_ && nvTemp_(dev, 0 /*NVML_TEMPERATURE_GPU*/, &tm) == 0) t.temp_c = (double)tm;
    NvmlMem mem{}; if (nvMem_ && nvMem_(dev, &mem) == 0) t.vram_mib = (double)mem.used / 1048576.0;  // bytes -> MiB (F8)
}

inline void TelemetryCsv::write_header() {
    // Group A (PresentMon console names, F1/F2/F5) | Group B (architecture-native, F13) |
    // Group C (PresentMon telemetry names + _<dev> suffix, F7/F8). NA columns kept for schema fidelity.
    std::fprintf(fp_,
        "Application,ProcessID,FrameType,CPUStartTime,MsBetweenPresents,MsInPresentAPI,"
        "MsUntilDisplayed,MsBetweenDisplayChange,DisplayedTime,MsRenderPresentLatency,"
        "MsGPUTime,InstrumentedLatency,MsClickToPhotonLatency,"
        "phyriadfg_frame_index,qpc_present,warp_ms,iter_ms,slip_ms,MsAddedLatency,pair_age_ms,"
        "ring_occ,cap_slots,frz,source_fps,output_fps,cons_per_s,uniq_per_s,gme_dissidence_pct,route_device,"
        "GPUUtilization_4090,GPUPower_4090,GPUTemperature_4090,GPUFrequency_4090,GPUMemoryFrequency_4090,GPUMemorySizeUsed_4090,"
        "GPUUtilization_1080Ti,GPUPower_1080Ti,GPUTemperature_1080Ti,GPUFrequency_1080Ti,GPUMemoryFrequency_1080Ti,GPUMemorySizeUsed_1080Ti,"
        "GPUUtilization_iGPU,CPUUtilization,"
        "phyriadfg_disp_phase,phyriadfg_disp_src_frames\n");   // TB-C9 fluidity axis (appended; positions of all prior columns unchanged)
    // a one-line schema note so a consumer knows the vocabulary (console-style, F2)
    std::fprintf(fp_, "# schema=PhyriadFG/STAGE-100+TB-C9; frametime=MsBetweenPresents(console-style); FrameType: Synthesized|Repeated; NA cells are empty; per-device GPU* suffixed; iGPU/CPU telemetry NA this phase; phyriadfg_disp_phase=intra-pair t_use[0,1], phyriadfg_disp_src_frames=content-source-time(source-frame units, the Animation-Error analogue)\n");
}

inline void TelemetryCsv::write_row(const CsvRow& r, long long& prev_qpc, bool& have_prev) {
    auto D  = [this](FILE* f, double v){ if (v < 0) std::fprintf(f, ","); else std::fprintf(f, ",%.4f", v); };
    const char* ft = (r.frame_type == 1) ? "Repeated" : "Synthesized";
    const double mbp = have_prev ? (double)(r.qpc_present - prev_qpc) / qfreq_ * 1000.0 : -1.0;
    const double cpu_start_ms = (double)(r.qpc_present - qpc0_) / qfreq_ * 1000.0;
    // Group A
    std::fprintf(fp_, "%s,%u,%s,%.4f", app_.c_str(), pid_, ft, cpu_start_ms);
    D(fp_, mbp); D(fp_, r.ms_in_present_api);
    // MsUntilDisplayed = NA (external capture). W4: MsBetweenDisplayChange = the own-window DISPLAY-FLIP
    // interval (GetFrameStatistics SyncQPCTime delta) when the present site populated it; else NA (composed/
    // non-own-window or a GetFrameStatistics soft-fail → the scorer falls back to MsBetweenPresents, flagged).
    // DisplayedTime, MsRenderPresentLatency = NA (external capture). NA path writes the SAME 4 empty fields
    // as the prior ",,,," → byte-identical when ms_between_display_change stays at its <0 sentinel.
    std::fprintf(fp_, ",");                    // MsUntilDisplayed = NA
    D(fp_, r.ms_between_display_change);        // MsBetweenDisplayChange (W4; NA when <0)
    std::fprintf(fp_, ",,");                   // DisplayedTime, MsRenderPresentLatency = NA
    D(fp_, r.warp_ms);                         // MsGPUTime ~= 4090 synth (approx; documented)
    std::fprintf(fp_, ",,");                    // InstrumentedLatency,MsClickToPhotonLatency = NA
    // Group B
    std::fprintf(fp_, ",%llu,%lld", (unsigned long long)r.frame_index, r.qpc_present);
    D(fp_, r.warp_ms); D(fp_, r.iter_ms); D(fp_, r.slip_ms); D(fp_, r.added_lat_ms); D(fp_, r.pair_age_ms);
    D(fp_, r.ring_occ);
    if (r.cap_slots < 0) std::fprintf(fp_, ","); else std::fprintf(fp_, ",%d", r.cap_slots);
    std::fprintf(fp_, ",%d", r.frz);
    D(fp_, r.source_fps); D(fp_, r.output_fps); D(fp_, r.cons_per_s); D(fp_, r.uniq_per_s); D(fp_, r.gme_dis_pct);
    const char* rd = r.route_device==0?"4090":r.route_device==1?"1080Ti":r.route_device==2?"iGPU":"CPU";
    std::fprintf(fp_, ",%s", rd);
    // Group C (telemetry snapshot, forward-filled)
    D(fp_, teleA_.util); D(fp_, teleA_.power_w); D(fp_, teleA_.temp_c); D(fp_, teleA_.clk_mhz); D(fp_, teleA_.memclk_mhz); D(fp_, teleA_.vram_mib);
    D(fp_, teleB_.util); D(fp_, teleB_.power_w); D(fp_, teleB_.temp_c); D(fp_, teleB_.clk_mhz); D(fp_, teleB_.memclk_mhz); D(fp_, teleB_.vram_mib);
    std::fprintf(fp_, ",,");                    // GPUUtilization_iGPU, CPUUtilization = NA this phase
    // ── TB-C9 (FLUIDITY): the displayed intra-pair phase + the content-source-time this present
    //    depicts. <0 sentinel → NA (the D helper), so a row from a path that didn't populate them
    //    (e.g. a not-yet-instrumented push site) stays honest rather than logging a fake 0.
    D(fp_, r.disp_phase); D(fp_, r.disp_src);
    std::fprintf(fp_, "\n");
    // accumulate for stats
    if (mbp > 0) ft_.push_back(mbp);
    if (r.warp_ms > 0) slice_ms_.push_back(r.warp_ms);   // STAGE-104: per-tick FG 4090 slice (drops ≈0 → correctly lowers the make-space occupancy)
    if (r.source_fps > 0) { src_fps_sum_ += r.source_fps; ++src_fps_n_; }
    if (r.frz) ++n_freeze_;
    if (r.fdrop) ++n_fdrop_;   // STAGE-105: count discriminator drops distinctly
    ++n_present_;
    if (qpc_first_ == 0) qpc_first_ = r.qpc_present;
    qpc_last_ = r.qpc_present;
    prev_qpc = r.qpc_present; have_prev = true;
}

inline void TelemetryCsv::drain_loop() {
    // low priority so the export never competes with the present thread
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    nvml_init();
    write_header();
    long long prev_qpc = 0; bool have_prev = false;
    double last_sample = -1e9;
    // STAGE-103 watchdog state (drain-local): when did the present thread last push a row?
    uint64_t last_w = 0; double last_w_change_ms = now_ms();
    bool in_stall = false; double stall_start_ms = 0.0, last_ongoing_ms = 0.0;
    double last_flush_ms = -1e9;   // STAGE-103: periodic fflush so a HARD crash leaves the pre-crash tail on disk
    for (;;) {
        const bool stopping = stop_.load();
        const double t = now_ms();
        if (t - last_sample >= 25.0) {   // ~40 Hz telemetry (F9: NVML off the hot path)
            nvml_sample(nvA_, teleA_); nvml_sample(nvB_, teleB_);
            last_sample = t;
        }
        uint64_t w  = w_idx_.load();
        // ── STAGE-103 WATCHDOG: detect present-thread stalls (freeze / sync-fence block / hang) ──
        // w_idx advances once per present tick; if it stops the present is stalled. Markers are
        // flushed to disk IMMEDIATELY so a force-kill mid-hang still leaves the evidence + the WHY.
        // Gated on (w>0 || last_w>0) so the pre-first-present startup is never a false stall; and
        // on !stopping so the shutdown drain is not flagged.
        if (!stopping && (w > 0 || last_w > 0)) {
            if (w != last_w) {
                if (in_stall) {
                    const double dur = t - stall_start_ms;
                    if (dur > max_stall_ms_) max_stall_ms_ = dur;
                    total_stall_ms_ += dur; ++stall_count_;
                    if (fp_) { std::fprintf(fp_, "# STALL_END dur_ms=%.1f at_ms=%.1f present_resumed 4090_util=%.0f 4090_clk=%.0f\n",
                                            dur, t, teleA_.util, teleA_.clk_mhz); std::fflush(fp_); }
                    in_stall = false;
                }
                last_w = w; last_w_change_ms = t;
            } else if (t - last_w_change_ms > kStallMs) {
                if (!in_stall) {
                    in_stall = true; stall_start_ms = last_w_change_ms; last_ongoing_ms = t;
                    if (fp_) { std::fprintf(fp_, "# STALL_BEGIN gap_ms=%.1f at_ms=%.1f present_not_advancing 4090_util=%.0f 4090_pow_w=%.0f 4090_clk=%.0f 4090_temp=%.0f 1080Ti_util=%.0f\n",
                                            t - last_w_change_ms, t, teleA_.util, teleA_.power_w, teleA_.clk_mhz, teleA_.temp_c, teleB_.util); std::fflush(fp_); }
                } else if (t - last_ongoing_ms > 1000.0) {   // terminal-hang forensics: GPU state every ~1s while hung
                    last_ongoing_ms = t;
                    if (fp_) { std::fprintf(fp_, "# STALL_ONGOING for_ms=%.1f at_ms=%.1f 4090_util=%.0f 4090_clk=%.0f 4090_temp=%.0f\n",
                                            t - stall_start_ms, t, teleA_.util, teleA_.clk_mhz, teleA_.temp_c); std::fflush(fp_); }
                }
            }
        }
        uint64_t rd = r_idx_.load();
        while (rd < w) { write_row(ring_[rd & MASK], prev_qpc, have_prev); ++rd; }
        r_idx_.store(rd);
        // STAGE-103: bound the lost-tail on a HARD crash — flush at ~4 Hz so the last ≤250 ms of
        // rows (the pre-crash GPU state) are on disk even if the process dies with no clean stop().
        if (fp_ && t - last_flush_ms > 250.0) { std::fflush(fp_); last_flush_ms = t; }
        if (stopping && rd == w_idx_.load()) break;
        Sleep(2);
    }
    // STAGE-103: if we broke out while still stalled (stop() arrived during a hang), record it.
    if (in_stall) { const double dur = now_ms() - stall_start_ms; if (dur > max_stall_ms_) max_stall_ms_ = dur; total_stall_ms_ += dur; ++stall_count_; }
    if (fp_) { std::fflush(fp_); std::fclose(fp_); fp_ = nullptr; }
    write_stats();
    if (nvmlLib_) { if (nvShut_) nvShut_(); FreeLibrary(nvmlLib_); nvmlLib_ = nullptr; }
}

inline void TelemetryCsv::write_stats() {
    FILE* sf = std::fopen(stats_path_.c_str(), "wb");
    if (!sf) return;
    const double dur_s = (qfreq_ > 0 && qpc_last_ > qpc_first_) ? (double)(qpc_last_ - qpc_first_) / qfreq_ : 0.0;
    const double present_fps = dur_s > 0 ? (double)n_present_ / dur_s : -1.0;
    const double real_fps    = src_fps_n_ ? src_fps_sum_ / (double)src_fps_n_ : -1.0;
    double avg_ft = -1, min_ft = -1, max_ft = -1;
    if (!ft_.empty()) { double s=0; min_ft=ft_[0]; max_ft=ft_[0]; for(double f:ft_){s+=f; if(f<min_ft)min_ft=f; if(f>max_ft)max_ft=f;} avg_ft=s/(double)ft_.size(); }
    const double avg_fps = avg_ft>0?1000.0/avg_ft:-1, min_fps = max_ft>0?1000.0/max_ft:-1, max_fps = min_ft>0?1000.0/min_ft:-1;
    using namespace tele_detail;
    const double low1_int  = low_integral(ft_, 0.01),  low1_avg  = low_average(ft_, 0.01);
    const double low01_int = (ft_.size()>=300)? low_integral(ft_, 0.001) : -1.0;
    const double low01_avg = (ft_.size()>=300)? low_average(ft_, 0.001) : -1.0;
    const double p99 = percentile_ft(ft_, 99), p95 = percentile_ft(ft_, 95), p90 = percentile_ft(ft_, 90);
    StutterOut st = stutter(ft_, 2.5, 1000.0);   // factor 2.5 (CapFrameX), 1000ms trailing window (documented choice)
    auto F = [](double v){ return v<0 ? std::string("NA") : ([&]{ char b[64]; std::snprintf(b,sizeof(b),"%.4f",v); return std::string(b); }()); };
    std::fprintf(sf, "metric,value,method/note\n");
    std::fprintf(sf, "duration_s,%s,\n", F(dur_s).c_str());
    std::fprintf(sf, "frame_count,%llu,presented frames\n", (unsigned long long)n_present_);
    std::fprintf(sf, "freeze_count,%llu,real lap-freezes (re-shows; distinct from fdrop)\n", (unsigned long long)n_freeze_);
    std::fprintf(sf, "fdrop_count,%llu,STAGE-105 --fdrop exact-duplicate discriminator drops (elided redundant warps; make-space density)\n", (unsigned long long)n_fdrop_);
    std::fprintf(sf, "dropped_rows,%llu,ring overflow (telemetry only; not presents)\n", (unsigned long long)drops_.load());
    std::fprintf(sf, "max_stall_ms,%s,STAGE-103 watchdog: longest present-thread stall (gap>100ms = freeze/hang)\n", F(max_stall_ms_).c_str());
    std::fprintf(sf, "stall_count,%llu,present stalls/freezes/hangs detected (gap>100ms; see # STALL_ lines in raw csv)\n", (unsigned long long)stall_count_);
    std::fprintf(sf, "total_stall_ms,%s,cumulative stalled wall-time\n", F(total_stall_ms_).c_str());
    std::fprintf(sf, "present_fps_avg,%s,all presented frames\n", F(avg_fps).c_str());
    std::fprintf(sf, "present_fps_min,%s,1000/max_frametime\n", F(min_fps).c_str());
    std::fprintf(sf, "present_fps_max,%s,1000/min_frametime\n", F(max_fps).c_str());
    std::fprintf(sf, "real_fps_avg,%s,mean source_fps (the honest input rate)\n", F(real_fps).c_str());
    std::fprintf(sf, "fg_multiplier,%s,present_fps_avg / real_fps_avg\n", F(present_fps>0&&real_fps>0?present_fps/real_fps:-1).c_str());
    std::fprintf(sf, "1pct_low_fps_integral,%s,CapFrameX integral (recommended default)\n", F(low1_int).c_str());
    std::fprintf(sf, "1pct_low_fps_average,%s,CapFrameX average-of-worst\n", F(low1_avg).c_str());
    std::fprintf(sf, "0p1pct_low_fps_integral,%s,sample-guarded (>=300 frames)\n", F(low01_int).c_str());
    std::fprintf(sf, "0p1pct_low_fps_average,%s,sample-guarded (>=300 frames)\n", F(low01_avg).c_str());
    std::fprintf(sf, "P99_frametime_ms,%s,1%% low FPS = 1000/this\n", F(p99).c_str());
    std::fprintf(sf, "P95_frametime_ms,%s,\n", F(p95).c_str());
    std::fprintf(sf, "P90_frametime_ms,%s,\n", F(p90).c_str());
    std::fprintf(sf, "stutter_count_pct,%s,ft>2.5*movavg(1000ms window)\n", F(st.count_pct).c_str());
    std::fprintf(sf, "stutter_time_pct,%s,time fraction in stutter frames\n", F(st.time_pct).c_str());
    std::fprintf(sf, "adaptive_stdev_ms,%s,std of frametime residuals vs moving average\n", F(st.adaptive_stdev).c_str());
    std::fprintf(sf, "present_fps_inst_avg,%s,mean output_fps column\n", F(present_fps).c_str());
    // ── STAGE-104: the FG per-tick 4090 slice + occupancy (the make-space cap input, FG_SATURATION_PRIOR_ART §4) ──
    // slice = the present-thread warp+present wall per output tick (r.warp_ms). occupancy = the fraction of
    // wall-time the FG holds the 4090 at the achieved output rate = present_fps * mean_slice. THIS is the
    // honest, measured number for "make space": the game must be capped to leave the 4090 at least this free
    // (plus margin) so the FG keeps its slice. We deliberately DON'T emit a single cap_fps — that needs the
    // game's own per-frame GPU cost (unobservable from external capture, §4 open-q4); occupancy is what we know.
    double slice_avg=-1, slice_sum=0; for(double s:slice_ms_) slice_sum+=s;
    if(!slice_ms_.empty()) slice_avg=slice_sum/(double)slice_ms_.size();
    const double slice_p99 = percentile_ft(slice_ms_, 99);
    const double slice_max = [&]{ double m=-1; for(double s:slice_ms_) if(s>m) m=s; return m; }();
    const double fg_occ_pct = (present_fps>0 && slice_avg>0) ? (present_fps * slice_avg / 1000.0 * 100.0) : -1.0;
    std::fprintf(sf, "fg_slice_ms_4090_avg,%s,per-tick warp+present wall on the 4090 (the FG's GPU cost/frame)\n", F(slice_avg).c_str());
    std::fprintf(sf, "fg_slice_ms_4090_p99,%s,worst-case per-tick FG 4090 cost\n", F(slice_p99).c_str());
    std::fprintf(sf, "fg_slice_ms_4090_max,%s,max per-tick FG 4090 cost\n", F(slice_max).c_str());
    std::fprintf(sf, "fg_4090_occupancy_pct,%s,present_fps*slice = %% of the 4090 the FG needs; the game must leave this free (make-space)\n", F(fg_occ_pct).c_str());
    std::fflush(sf); std::fclose(sf);
    std::printf("[ra] --csv: wrote %llu rows over %.1fs (%llu drops) + stats\n",
                (unsigned long long)n_present_, dur_s, (unsigned long long)drops_.load());
}

} // namespace phyriadfg
// Made with my soul - Swately <3
