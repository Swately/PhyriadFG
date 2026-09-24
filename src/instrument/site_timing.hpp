#pragma once
// PhyriadFG — src/instrument/site_timing.hpp : `--site-timing`, the per-site cost profile of ONE source frame's
// whole path (INSTRUMENT plane; nothing here touches a pixel).
//
// Why it exists: E0B's partition covered the PRESENTER tick only, and the OAP judge (OV1) closed E0C as
// UNATTRIBUTED because nothing timed the matcher on the F-thread. This instrument splits the chain a source
// frame travels into named sites and reports each one's distribution — the OAP session's spec a-f
// (docs/FOTO_MENTAL.md section 5(0)):
//   capture->host   C.cap_copy (the CPU readback of the mapped staging slot; host wall)
//   host->device    C.upload (a_src -> Anative), F.upload (hR_b -> Bframe), P.upload_frames / P.upload_fields
//   the matcher     F/B .downsample .pyramid .match (split inside record_optical_flow by two framework marks),
//                   F/B .warp_discard (the k=0.5 warp WAP never reads + the MV/SAD layout barriers),
//                   F.post (mv_smooth + the GPU gme fit when on), P.consensus (the 3x3 MV consensus)
//   device->host    C.download (Awork -> hR_a), F.copyout / B.copyout (MV, SAD, candidates / MV_bwd, + bwd gme)
//   the presenter   P.warp (the --warp-timing pair around the warp batch; --site-timing arms it)
//   host waits      C/F/P .submit_wait (submit -> fence signalled, INCLUDING the a_q2_mtx lock wait), B.wait
//   the remainder   F.cpu = F.iter - (F.ring_wait + F.submit_wait + B.wait): the F-thread time no site names
// GPU intervals come from vkCmdWriteTimestamp pairs, CPU intervals from now_ms(). Every figure is a
// DISTRIBUTION (mean, p50, p99, max, n) over the window after a fixed warm-up, never an EMA, plus its cost per
// second of wall time (a per-iteration figure hides the rate: F runs ~120/s, P ~240/s).
//
// Rules (spec (f)): queries are read only AFTER the lane's fence was waited, with no WAIT bit — a lane whose
// results are not available is counted as lost, never waited for on the hot path. A lane is armed only on
// the path it was written for (the serial WAP flow with the classical matcher, the fenced bridge upload, the
// convert on the Vulkan primary); on any other path it prints why and records nothing.
//
// Two classes, one header, so the arithmetic can be tested without a GPU (the GdumpBook pattern):
//   SiteBook   — pure CPU: sample storage, warm-up exclusion, the statistics, the report text.
//   SiteTiming — the Vulkan glue: one query pool per lane, the marks, the non-blocking reads.
//
// Off = byte-identical: site_timing() is null unless cfg.site_timing, every hook is `if(st)`, no query pool
// exists, no command is recorded, nothing is printed.
// Made with my soul - Swately <3
#include "core/device.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

namespace phyriad::render::present { class D3d11StampRing; }   // the D3D11 sites' rings (framework, header-only)

namespace pfg::instrument {

// The sites, in the order the report prints them (the chain a source frame travels).
enum class Site : uint8_t {
    C_CAP_COPY, C_UPLOAD, C_CONVERT, C_DOWNLOAD, C_TRANSITIONS, C_SUBMIT_WAIT,
    F_RING_WAIT, F_UPLOAD, F_TRANSITIONS, F_DOWNSAMPLE, F_PYRAMID, F_MATCH, F_WARP_DISCARD, F_POST, F_COPYOUT, F_SUBMIT_WAIT,
    B_DOWNSAMPLE, B_PYRAMID, B_MATCH, B_WARP_DISCARD, B_COPYOUT, B_WAIT,
    P_UPLOAD_FRAMES, P_TRANSITIONS, P_UPLOAD_FIELDS, P_CONSENSUS, P_SUBMIT_WAIT, P_WARP,
    // the D3D11 side (added after the OV1 judgement): D3D11 timestamps on the capture device and the present device
    D_CAP_COPY,                            // GPU: the WGC callback's CopyResource(staging ring slot, captured surface)
    D_BRIDGE_COPY,                         // GPU: PresentSurface's CopyResource(backbuffer, imported bridge)
    D_KM_ACQUIRE, D_COPY_CALL, D_PRESENT_CALL,   // host walls of the present side: keyed-mutex acquire, the copy call, Present
    F_ITER, F_CPU,                        // the F-thread pair wall and its unbucketed remainder
    C_SPAN, F_SPAN, B_SPAN, P_SPAN,        // each lane's first->last timestamp (a check, never summed)
    kCount
};
inline constexpr int kSiteCount = (int)Site::kCount;

enum class SiteKind : uint8_t { Gpu, Host, Span };
struct SiteInfo { const char* name; SiteKind kind; };
const SiteInfo& site_info(Site s);

struct SiteStats { uint64_t n = 0; double mean = 0, p50 = 0, p99 = 0, max = 0, sum = 0; };

// ── SiteBook — the CPU bookkeeping (testable) ───────────────────────────────────────────────────────────────
class SiteBook {
public:
    // cap = samples kept per site (preallocated; beyond it a sample is counted as dropped, never allocated).
    // A sample stamped before t_arm_ms + warmup_ms is counted as warm-up and not kept.
    SiteBook(uint32_t cap, double t_arm_ms, double warmup_ms);

    // One writer thread per site (the site's owner) — no locks. The report runs after every writer joined.
    void record(Site s, double ms, double t_now_ms);
    void lost(Site s) { ++lost_[(int)s]; }   // a lane whose query results were not available after its fence

    SiteStats stats(Site s) const;           // nearest-rank percentiles over the kept samples
    double window_s() const;                 // warm_end -> the latest kept sample, seconds (0 if none)
    uint64_t warmup(Site s) const { return warm_[(int)s]; }
    uint64_t dropped(Site s) const { return drop_[(int)s]; }
    uint64_t lost_n(Site s) const { return lost_[(int)s]; }

    // The report. Human lines, one per site with samples, the lane checks, the two remainder lines, then ONE
    // parseable line: `[site] done (window_s=W f_pairs_per_s=R <site>=mean,p99,n,ms_per_s ...)`.
    // lane_note[i] (i = C,F,B,P) is printed verbatim when a lane was not armed ("" = armed).
    std::string report(const char* const lane_note[4]) const;
    // unix seconds = this offset + a now_ms() stamp / 1000; set once at arming so the report can print the window
    // in absolute time (another instrument's capture is then matched by clock, not by guess). 0 = not set.
    void set_epoch_offset(double s) { epoch_off_s_ = s; }

private:
    uint32_t cap_;
    double warmup_ms_, warm_end_;
    double epoch_off_s_ = 0.0;
    std::vector<float> v_[kSiteCount];
    uint64_t warm_[kSiteCount] = {}, drop_[kSiteCount] = {}, lost_[kSiteCount] = {};
    double last_t_[kSiteCount] = {};
};

// ── SiteTiming — the Vulkan glue ────────────────────────────────────────────────────────────────────────────
enum class Lane : uint8_t { C = 0, F = 1, B = 2, P = 3, kCount = 4 };

class SiteTiming {
public:
    static constexpr double   kWarmupMs   = 5000.0;     // start-up excluded: the first 5 s after arming
    static constexpr uint32_t kCapSamples = 1u << 17;   // 131,072 per site = 9.1 min at 240/s

    explicit SiteTiming(double t_arm_ms);
    ~SiteTiming();   // destroys any pool report_and_destroy did not (an exit that skipped the report); before the device
    SiteTiming(const SiteTiming&) = delete;
    SiteTiming& operator=(const SiteTiming&) = delete;
    // Creates the lane's query pool on `d` (the device whose queue the lane's command buffer is submitted to).
    // false = timestamps unsupported there; the lane stays unarmed and `why` says so.
    bool arm(Lane l, const VDev& d, const char* why_if_not);
    void disarm(Lane l, const char* why);   // a lane written for a path this run does not take
    bool on(Lane l) const { return pool_[(int)l] != VK_NULL_HANDLE && note_[(int)l].empty(); }
    VkQueryPool pool(Lane l) const { return pool_[(int)l]; }

    // Recording (the lane owner's thread, inside its command buffer). begin = reset the lane's queries + mark 0;
    // mark = a bottom-of-pipe mark (every command submitted before it on that queue has completed). Mark 0 is
    // bottom-of-pipe too (was top-of-pipe until the OV1 judgement of 2026-09-18): a top-of-pipe mark 0 put the wait for
    // work ALREADY queued (the in-flight async warp on A.q, reached by the first ALL_COMMANDS barrier) inside the
    // lane's first site, and made that first interval the only TOP->BOTTOM one.
    void begin(Lane l, VkCommandBuffer cmd);
    void mark(Lane l, VkCommandBuffer cmd, uint32_t idx);
    // After the lane's fence was waited: one non-blocking read; the intervals go to the book. false = lost.
    bool read(Lane l, double t_now_ms);

    // A sample computed by the caller: every host wall, and P.warp (the GPU interval --warp-timing already read).
    void host(Site s, double ms, double t_now_ms) { book_.record(s, ms, t_now_ms); }
    // The D3D11 sites. drain_d3d: every completed bracket of a D3D11 timestamp ring whose first interval is the
    // site (called by that site's single reading thread). note_lost: a running total of brackets the source lost
    // (skipped + disjoint) -> the book's lost count for the site, by delta.
    void drain_d3d(phyriad::render::present::D3d11StampRing& ring, Site gpu_site, double t_now_ms);
    void note_lost(Site s, uint64_t running_total);
    void lost(Site s) { book_.lost(s); }   // one sample the site could not produce (e.g. a negative D3D11 interval)
    // The F-thread pair: begin at pickup, add each named wait, end after the consume -> F.iter + F.cpu.
    void f_pair_begin(double t_ms) { f_t0_ = t_ms; f_waits_ = 0.0; }
    void f_pair_wait(double ms) { f_waits_ += ms; }
    void f_pair_end(double t_ms);

    // After every worker joined: print the report, destroy the pools (the device is still alive).
    void report_and_destroy();

private:
    SiteBook book_;
    VkDevice dev_[4] = {};
    VkQueryPool pool_[4] = {};
    double period_ns_[4] = {};
    uint64_t mask_[4] = {};
    std::string note_[4];
    double f_t0_ = 0.0, f_waits_ = 0.0;
    uint64_t lost_seen_[kSiteCount] = {};   // note_lost's last running total per site
};

// The process-wide instance: null when --site-timing is off. Set once before the worker threads start (the
// thread creation orders it), cleared after they joined.
SiteTiming* site_timing();
void site_timing_set(SiteTiming* st);

// The marks each lane writes (indices into its pool). Mark 0 is always begin(). Since the OV1 judgement of
// 2026-09-24 every whole-frame COPY site is bracketed by its own two marks, written after the copy's opening layout
// barrier and before its closing one, so the site holds the copy alone; the barriers go to X.transitions.
inline constexpr uint32_t kLaneMarks[4] = { 6u, 10u, 6u, 8u };   // C, F, B, P
// C: 0 begin | 1 before the upload copy | 2 after it | 3 after the convert dispatch | 4 before the download copy |
//    5 after it.  C.upload [1,2] . C.convert [2,3] (with the upload's closing and the convert's opening barrier) .
//    C.download [4,5] . C.transitions [0,1] + [3,4]
// F: 0 begin | 1 before the upload copy | 2 after it | 3 after its closing barrier | 4 after downsample |
//    5 pyramid (framework) | 6 match (framework) | 7 after the record (warp_discard) | 8 after post | 9 end (after the
//    copy-outs).  F.upload [1,2] . F.transitions [0,1] + [2,3] . then [3,4] [4,5] [5,6] [6,7] [7,8] [8,9]. On the
//    iGPU-convert path (not the default) marks 1-2 bracket the whole unpack, barriers included.
// B: 0 begin | 1 after downsample | 2 pyramid (framework) | 3 match (framework) | 4 after the record | 5 end
// P: 0 begin | 1 before the prev copy | 2 after it | 3 before the cur copy | 4 after it | 5 after the frame uploads
//    (+ the field when on) | 6 after the field uploads | 7 end (after consensus).  P.upload_frames [1,2] + [3,4] .
//    P.transitions [0,1] + [2,3] + [4,5] . P.upload_fields [5,6] . P.consensus [6,7]

}  // namespace pfg::instrument
