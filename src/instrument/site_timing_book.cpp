// PhyriadFG — src/instrument/site_timing_book.cpp : SiteBook, the CPU half of `--site-timing` (sample storage,
// warm-up exclusion, the statistics, the report text). No Vulkan call is made here; tests/instrument/
// test_site_book.cpp exercises all of it without a GPU. Contract: site_timing.hpp.
// Made with my soul - Swately <3
#include "instrument/site_timing.hpp"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace pfg::instrument {

namespace {
constexpr SiteInfo kInfo[kSiteCount] = {
    {"C.cap_copy",       SiteKind::Host}, {"C.upload",       SiteKind::Gpu},  {"C.convert",     SiteKind::Gpu},
    {"C.download",       SiteKind::Gpu},  {"C.submit_wait",  SiteKind::Host},
    {"F.ring_wait",      SiteKind::Host}, {"F.upload",       SiteKind::Gpu},  {"F.downsample",  SiteKind::Gpu},
    {"F.pyramid",        SiteKind::Gpu},  {"F.match",        SiteKind::Gpu},  {"F.warp_discard",SiteKind::Gpu},
    {"F.post",           SiteKind::Gpu},  {"F.copyout",      SiteKind::Gpu},  {"F.submit_wait", SiteKind::Host},
    {"B.downsample",     SiteKind::Gpu},  {"B.pyramid",      SiteKind::Gpu},  {"B.match",       SiteKind::Gpu},
    {"B.warp_discard",   SiteKind::Gpu},  {"B.copyout",      SiteKind::Gpu},  {"B.wait",        SiteKind::Host},
    {"P.upload_frames",  SiteKind::Gpu},  {"P.upload_fields",SiteKind::Gpu},  {"P.consensus",   SiteKind::Gpu},
    {"P.submit_wait",    SiteKind::Host}, {"P.warp",         SiteKind::Gpu},
    {"D.cap_copy",       SiteKind::Gpu},  {"D.bridge_copy",  SiteKind::Gpu},
    {"D.km_acquire",     SiteKind::Host}, {"D.copy_call",    SiteKind::Host}, {"D.present_call", SiteKind::Host},
    {"F.iter",           SiteKind::Host}, {"F.cpu",          SiteKind::Host},
    {"C.span",           SiteKind::Span}, {"F.span",         SiteKind::Span}, {"B.span",        SiteKind::Span},
    {"P.span",           SiteKind::Span},
};

// Appends the formatted text WHOLE. It used a fixed 512-byte buffer and cut anything longer: the header line grew past
// it with the D3D11 note, lost its tail and its newline, and the table header ran onto it (T8; found in the first
// D3D11 campaign's logs, 2026-09-24 - rows and the done-line are separate appends, so no number was affected).
void appendf(std::string& out, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    const int n = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        const size_t at = out.size();
        out.resize(at + (size_t)n + 1u);
        std::vsnprintf(&out[at], (size_t)n + 1u, fmt, ap2);
        out.resize(at + (size_t)n);   // drop the terminator vsnprintf wrote
    }
    va_end(ap2);
}

// Nearest-rank percentile over an ascending-sorted vector: the smallest sample with at least p of the mass
// at or below it (p99 of 100 samples = the 99th smallest).
double nearest_rank(const std::vector<float>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    const double r = std::ceil(p * (double)sorted.size());
    const size_t i = (size_t)std::clamp(r, 1.0, (double)sorted.size()) - 1u;
    return (double)sorted[i];
}
}  // namespace

const SiteInfo& site_info(Site s) { return kInfo[(int)s]; }

SiteBook::SiteBook(uint32_t cap, double t_arm_ms, double warmup_ms)
    : cap_(cap), warmup_ms_(warmup_ms), warm_end_(t_arm_ms + warmup_ms) {
    for (auto& v : v_) v.reserve(cap_);
}

void SiteBook::record(Site s, double ms, double t_now_ms) {
    const int i = (int)s;
    if (t_now_ms < warm_end_) { ++warm_[i]; return; }
    if (v_[i].size() >= cap_) { ++drop_[i]; return; }
    v_[i].push_back((float)ms);
    if (t_now_ms > last_t_[i]) last_t_[i] = t_now_ms;
}

SiteStats SiteBook::stats(Site s) const {
    SiteStats st;
    const auto& v = v_[(int)s];
    st.n = v.size();
    if (v.empty()) return st;
    std::vector<float> w(v);
    std::sort(w.begin(), w.end());
    double sum = 0.0;
    for (float x : w) sum += (double)x;
    st.sum = sum;
    st.mean = sum / (double)w.size();
    st.p50 = nearest_rank(w, 0.50);
    st.p99 = nearest_rank(w, 0.99);
    st.max = (double)w.back();
    return st;
}

double SiteBook::window_s() const {
    double last = 0.0;
    for (double t : last_t_) last = std::max(last, t);
    return last > warm_end_ ? (last - warm_end_) / 1000.0 : 0.0;
}

std::string SiteBook::report(const char* const lane_note[4]) const {
    std::string o;
    const double W = window_s();
    SiteStats S[kSiteCount];
    for (int i = 0; i < kSiteCount; ++i) S[i] = stats((Site)i);
    const double fpairs = W > 0.0 ? (double)S[(int)Site::F_ITER].n / W : 0.0;
    auto per_s = [&](const SiteStats& st) { return W > 0.0 ? st.sum / W : 0.0; };   // ms of the site per second of wall
    auto rate  = [&](const SiteStats& st) { return W > 0.0 ? (double)st.n / W : 0.0; };

    uint64_t warm_total = 0;
    for (uint64_t w : warm_) warm_total += w;
    double last = 0.0;
    for (double t : last_t_) last = std::max(last, t);
    const double t0_epoch = epoch_off_s_ + warm_end_ / 1000.0, t1_epoch = epoch_off_s_ + std::max(last, warm_end_) / 1000.0;
    appendf(o, "[site] window %.2f s after a %.1f s warm-up (%llu warm-up samples not kept), unix %.3f .. %.3f | F pairs "
               "%.1f/s | percentiles are nearest-rank | GPU sites are ELAPSED time between timestamps; each lane's mark 0 is "
               "bottom-of-pipe, so work queued on that queue before the lane is outside it; the other queue runs beside it "
               "(A.q present vs A.q2 flow+convert) and overlap is counted in both, so a sum can exceed the device's busy time; "
               "D.* GPU sites are D3D11 timestamps on the capture and present devices (same GPU, their own queues)\n",
            W, warmup_ms_ / 1000.0, (unsigned long long)warm_total, t0_epoch, t1_epoch, fpairs);
    static const char* const kLaneName[4] = {"C (convert, A.q2)", "F (fwd flow, A.q2)", "B (bwd flow, A.q2)", "P (bridge upload, A.q)"};
    for (int l = 0; l < 4; ++l)
        if (lane_note[l] && lane_note[l][0]) appendf(o, "[site] lane %s: NOT ARMED -- %s\n", kLaneName[l], lane_note[l]);
    appendf(o, "[site] %-16s %-4s %8s %9s %9s %9s %9s | %9s %8s\n", "site", "kind", "n", "mean ms", "p50", "p99", "max", "ms/s", "rate/s");
    std::string empty;
    for (int i = 0; i < kSiteCount; ++i) {
        const SiteInfo& in = kInfo[i];
        if (in.kind == SiteKind::Span) continue;
        if (S[i].n == 0) { if (!empty.empty()) empty += ' '; empty += in.name; continue; }
        appendf(o, "[site] %-16s %-4s %8llu %9.4f %9.4f %9.4f %9.4f | %9.3f %8.1f\n", in.name,
                in.kind == SiteKind::Gpu ? "gpu" : "host", (unsigned long long)S[i].n, S[i].mean, S[i].p50, S[i].p99, S[i].max,
                per_s(S[i]), rate(S[i]));
    }
    if (!empty.empty()) appendf(o, "[site] no samples: %s\n", empty.c_str());

    // The lane checks: the GPU span against the host's submit->signalled wall. Their difference is time the
    // submitting thread waited that no GPU interval of its own lane explains (the a_q2_mtx lock, queueing
    // behind the other lane or queue) — B has no such pair: its wait starts after the fwd consume's CPU work.
    struct LaneCheck { const char* name; Site span; Site host; };
    static const LaneCheck kChk[4] = { {"C", Site::C_SPAN, Site::C_SUBMIT_WAIT}, {"F", Site::F_SPAN, Site::F_SUBMIT_WAIT},
                                       {"B", Site::B_SPAN, Site::B_WAIT},         {"P", Site::P_SPAN, Site::P_SUBMIT_WAIT} };
    for (const auto& c : kChk) {
        const SiteStats& sp = S[(int)c.span];
        const SiteStats& h  = S[(int)c.host];
        if (sp.n == 0 && lost_[(int)c.span] == 0) continue;
        appendf(o, "[site] lane %s: GPU span mean %.4f p99 %.4f ms (n %llu, lost %llu) | %s mean %.4f ms",
                c.name, sp.mean, sp.p99, (unsigned long long)sp.n, (unsigned long long)lost_[(int)c.span], kInfo[(int)c.host].name, h.mean);
        if (c.span != Site::B_SPAN && h.n > 0 && sp.n > 0)
            appendf(o, " | host minus span %.4f ms per submit (waiting, not this lane's GPU execution)", h.mean - sp.mean);
        o += '\n';
    }

    // The two remainders (spec (f)).
    double gpu_sum = 0.0;
    for (int i = 0; i < kSiteCount; ++i) if (kInfo[i].kind == SiteKind::Gpu) gpu_sum += per_s(S[i]);
    appendf(o, "[site] remainder GPU: the named GPU sites sum to %.3f GPU-ms/s", gpu_sum);
    if (fpairs > 0.0) appendf(o, " = %.4f GPU-ms per source frame (per F pair)", gpu_sum / fpairs);
    o += " -- the device's busy total is not visible from inside; an external busy figure minus this is the UNBUCKETED "
         "GPU work (the D3D11 capture copy, DWM, the present copy outside the warp batch, the driver)\n";
    const SiteStats& it = S[(int)Site::F_ITER];
    const SiteStats& cpu = S[(int)Site::F_CPU];
    if (it.n > 0)
        appendf(o, "[site] remainder F-thread: F.iter mean %.4f ms; F.cpu = iter - (F.ring_wait + F.submit_wait + B.wait) mean %.4f "
                   "p99 %.4f ms = %.3f ms/s -- the F-thread time no site names (the record CPU, the host tail: gme fit, objects, memory)\n",
                it.mean, cpu.mean, cpu.p99, per_s(cpu));
    std::string ld;
    for (int i = 0; i < kSiteCount; ++i) {
        if (drop_[i] == 0 && lost_[i] == 0) continue;
        appendf(ld, " %s(dropped %llu, lost %llu)", kInfo[i].name, (unsigned long long)drop_[i], (unsigned long long)lost_[i]);
    }
    if (!ld.empty()) appendf(o, "[site] not kept:%s\n", ld.c_str());

    // The one parseable line (spec (e)).
    appendf(o, "[site] done (window_s=%.3f t0_epoch=%.3f t1_epoch=%.3f f_pairs_per_s=%.3f", W, t0_epoch, t1_epoch, fpairs);
    for (int i = 0; i < kSiteCount; ++i) {
        if (S[i].n == 0) continue;
        appendf(o, " %s=%.4f,%.4f,%llu,%.3f", kInfo[i].name, S[i].mean, S[i].p99, (unsigned long long)S[i].n, per_s(S[i]));
    }
    o += ")\n";
    return o;
}

}  // namespace pfg::instrument
