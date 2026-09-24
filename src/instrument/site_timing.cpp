// PhyriadFG — src/instrument/site_timing.cpp : SiteTiming, the Vulkan half of `--site-timing` (one timestamp
// query pool per lane, the marks, the non-blocking reads after the lane's fence). Contract: site_timing.hpp.
// Made with my soul - Swately <3
#include "instrument/site_timing.hpp"
#include <phyriad/render/present/D3d11StampRing.hpp>   // drain_d3d: the D3D11 sites' rings
#include <chrono>
#include <cstdio>
#include <vector>

namespace pfg::instrument {

namespace {
SiteTiming* g_site_timing = nullptr;

struct Interval { Site site; uint32_t a, b; };
// The named intervals of each lane, between the marks documented beside kLaneMarks (site_timing.hpp).
// A site listed more than once is the SUM of its segments (the X.transitions sites, P.upload_frames' two copies).
constexpr Interval kC[] = { {Site::C_TRANSITIONS, 0, 1}, {Site::C_UPLOAD, 1, 2}, {Site::C_CONVERT, 2, 3},
                            {Site::C_TRANSITIONS, 3, 4}, {Site::C_DOWNLOAD, 4, 5} };
constexpr Interval kF[] = { {Site::F_TRANSITIONS, 0, 1}, {Site::F_UPLOAD, 1, 2}, {Site::F_TRANSITIONS, 2, 3},
                            {Site::F_DOWNSAMPLE, 3, 4}, {Site::F_PYRAMID, 4, 5}, {Site::F_MATCH, 5, 6},
                            {Site::F_WARP_DISCARD, 6, 7}, {Site::F_POST, 7, 8}, {Site::F_COPYOUT, 8, 9} };
constexpr Interval kB[] = { {Site::B_DOWNSAMPLE, 0, 1}, {Site::B_PYRAMID, 1, 2}, {Site::B_MATCH, 2, 3},
                            {Site::B_WARP_DISCARD, 3, 4}, {Site::B_COPYOUT, 4, 5} };
constexpr Interval kP[] = { {Site::P_TRANSITIONS, 0, 1}, {Site::P_UPLOAD_FRAMES, 1, 2}, {Site::P_TRANSITIONS, 2, 3},
                            {Site::P_UPLOAD_FRAMES, 3, 4}, {Site::P_TRANSITIONS, 4, 5}, {Site::P_UPLOAD_FIELDS, 5, 6},
                            {Site::P_CONSENSUS, 6, 7} };
constexpr Site kSpan[4] = { Site::C_SPAN, Site::F_SPAN, Site::B_SPAN, Site::P_SPAN };
}  // namespace

SiteTiming* site_timing() { return g_site_timing; }
void site_timing_set(SiteTiming* st) { g_site_timing = st; }

SiteTiming::SiteTiming(double t_arm_ms) : book_(kCapSamples, t_arm_ms, kWarmupMs) {
    // the offset from the steady clock the samples carry to unix time, taken once here (the report's absolute window)
    const double unix_s = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    book_.set_epoch_offset(unix_s - t_arm_ms / 1000.0);
}

// The owner (main.cpp) lives in the block that also owns the worker threads, which closes before the device is
// destroyed at `done:` — so this runs while every dev_[i] is still valid. After report_and_destroy it is a no-op.
SiteTiming::~SiteTiming() {
    for (int i = 0; i < 4; ++i)
        if (pool_[i] != VK_NULL_HANDLE) { vkDestroyQueryPool(dev_[i], pool_[i], nullptr); pool_[i] = VK_NULL_HANDLE; }
}

bool SiteTiming::arm(Lane l, const VDev& d, const char* why_if_not) {
    const int i = (int)l;
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(d.phys, &props);
    uint32_t nf = 0; vkGetPhysicalDeviceQueueFamilyProperties(d.phys, &nf, nullptr);
    std::vector<VkQueueFamilyProperties> fp(nf); vkGetPhysicalDeviceQueueFamilyProperties(d.phys, &nf, fp.data());
    // The lane's command buffers are recorded from `pool` (qfam) and may be submitted to q2 (qfam2): both
    // families must write timestamps; the narrower valid-bit count sets the wrap mask.
    uint32_t bits = 64u;
    for (uint32_t f : { d.qfam, d.qfam2 })
        if (f != UINT32_MAX && f < nf && fp[f].timestampValidBits < bits) bits = fp[f].timestampValidBits;
    if (props.limits.timestampPeriod <= 0.0f || bits == 0u) { note_[i] = why_if_not; return false; }
    VkQueryPoolCreateInfo qi{}; qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO; qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qi.queryCount = kLaneMarks[i];
    if (vkCreateQueryPool(d.dev, &qi, nullptr, &pool_[i]) != VK_SUCCESS) { pool_[i] = VK_NULL_HANDLE; note_[i] = why_if_not; return false; }
    dev_[i] = d.dev;
    period_ns_[i] = (double)props.limits.timestampPeriod;
    mask_[i] = bits >= 64u ? ~0ull : ((1ull << bits) - 1ull);
    return true;
}

void SiteTiming::disarm(Lane l, const char* why) { note_[(int)l] = why; }

void SiteTiming::begin(Lane l, VkCommandBuffer cmd) {
    if (!on(l)) return;
    const int i = (int)l;
    vkCmdResetQueryPool(cmd, pool_[i], 0u, kLaneMarks[i]);   // every mark unavailable until this submission writes it
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_[i], 0u);   // after everything queued before the lane
}

void SiteTiming::mark(Lane l, VkCommandBuffer cmd, uint32_t idx) {
    if (!on(l)) return;
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_[(int)l], idx);
}

bool SiteTiming::read(Lane l, double t_now_ms) {
    if (!on(l)) return false;
    const int i = (int)l;
    const uint32_t n = kLaneMarks[i];
    uint64_t ts[16] = {};
    // No WAIT bit: the caller already waited the lane's fence, so every mark it recorded is available; a mark the
    // path did not write (or a device loss) returns VK_NOT_READY and the whole sample is counted as lost.
    const VkResult r = vkGetQueryPoolResults(dev_[i], pool_[i], 0u, n, sizeof ts, ts, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (r != VK_SUCCESS) { book_.lost(kSpan[i]); return false; }
    const uint64_t m = mask_[i];
    auto ms = [&](uint32_t a, uint32_t b, bool& ok) {
        const uint64_t d = (ts[b] - ts[a]) & m;
        if (d > m / 2u) { ok = false; return 0.0; }   // b before a: a mark out of order — never a real interval
        return (double)d * period_ns_[i] / 1.0e6;
    };
    const Interval* iv = nullptr; size_t niv = 0;
    switch (l) {
        case Lane::C: iv = kC; niv = sizeof kC / sizeof kC[0]; break;
        case Lane::F: iv = kF; niv = sizeof kF / sizeof kF[0]; break;
        case Lane::B: iv = kB; niv = sizeof kB / sizeof kB[0]; break;
        case Lane::P: iv = kP; niv = sizeof kP / sizeof kP[0]; break;
        default: return false;
    }
    bool ok = true;
    double acc[kSiteCount] = {};
    bool   seen[kSiteCount] = {};
    for (size_t k = 0; k < niv; ++k) { acc[(int)iv[k].site] += ms(iv[k].a, iv[k].b, ok); seen[(int)iv[k].site] = true; }
    const double span = ms(0u, n - 1u, ok);
    if (!ok) { book_.lost(kSpan[i]); return false; }
    for (int s = 0; s < kSiteCount; ++s) if (seen[s]) book_.record((Site)s, acc[s], t_now_ms);   // one sample per site
    book_.record(kSpan[i], span, t_now_ms);
    return true;
}

void SiteTiming::drain_d3d(phyriad::render::present::D3d11StampRing& ring, Site gpu_site, double t_now_ms) {
    using R = phyriad::render::present::D3d11StampRing;
    double iv[R::kMaxMarks - 1] = {};
    for (R::Take t; (t = ring.take(iv)) != R::Take::None; ) {
        if (t != R::Take::Sample) continue;                    // Disjoint: counted by the ring, reaches note_lost below
        if (iv[0] >= 0.0) book_.record(gpu_site, iv[0], t_now_ms);
        else book_.lost(gpu_site);                             // a negative interval is lost, never silently dropped
    }
    note_lost(gpu_site, ring.skipped() + ring.disjoint());
}

void SiteTiming::note_lost(Site s, uint64_t running_total) {
    const int i = (int)s;
    while (lost_seen_[i] < running_total) { book_.lost(s); ++lost_seen_[i]; }
}

void SiteTiming::f_pair_end(double t_ms) {
    if (f_t0_ <= 0.0) return;
    const double iter = t_ms - f_t0_;
    book_.record(Site::F_ITER, iter, t_ms);
    book_.record(Site::F_CPU, iter - f_waits_, t_ms);
    f_t0_ = 0.0;
}

void SiteTiming::report_and_destroy() {
    const char* notes[4];
    for (int i = 0; i < 4; ++i)
        notes[i] = !note_[i].empty() ? note_[i].c_str() : (pool_[i] == VK_NULL_HANDLE ? "never armed on this run" : "");
    const std::string s = book_.report(notes);
    std::fputs(s.c_str(), stdout);
    std::fflush(stdout);
    for (int i = 0; i < 4; ++i)
        if (pool_[i] != VK_NULL_HANDLE) { vkDestroyQueryPool(dev_[i], pool_[i], nullptr); pool_[i] = VK_NULL_HANDLE; }
}

}  // namespace pfg::instrument
