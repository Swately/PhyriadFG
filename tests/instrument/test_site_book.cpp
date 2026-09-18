// PhyriadFG — tests/instrument/test_site_book.cpp : SiteBook's CPU test (--site-timing, src/instrument/site_timing.hpp).
// GPU-free, Vulkan-free (vulkan.h's TYPE definitions come in through the header, nothing is called): the statistics
// and the report text that every --site-timing number passes through. Each check prints [OK]/[FAIL]; the exit code
// is the number of failures (the style of tests/instrument/test_gdump_book.cpp).
//
//   T1 — the statistics against hand-computed values: mean, nearest-rank p50/p99, max, n; ms/s over the window.
//   T2 — warm-up exclusion: a sample stamped before t_arm + warmup is counted, not kept; the window starts at the
//        warm-up's end and ends at the latest kept sample.
//   T3 — the capacity: past `cap` a sample is dropped and counted, never stored (no allocation on the hot path).
//   T4 — the F-thread remainder and the lane check: F.cpu = F.iter - the named waits reaches the report's
//        remainder line; "host minus span" is the difference of the two means.
//   T5 — the parseable `[site] done (...)` line: every site with samples appears as name=mean,p99,n,ms_per_s, and
//        it round-trips through a parser; a lane note reaches the report verbatim.
//   T6 — the checks can fail: a deliberately wrong expectation is seen red (then not counted).
//
// Build: the `pfg_site_test` target (CMakeLists.txt). Run: pfg_site_test
// Made with my soul - Swately <3
#include "instrument/site_timing.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace pfg::instrument;

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  %s  %s\n", ok ? "[OK]" : "[FAIL]", what);
    if (!ok) ++g_fail;
}
static bool approx(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const char* const kNoNotes[4] = {"", "", "", ""};

// The field `name=` of the parseable done-line, split into its four values; false if absent.
static bool parse_done(const std::string& rep, const char* name, double& mean, double& p99, double& n, double& msps) {
    const size_t d = rep.find("[site] done (");
    if (d == std::string::npos) return false;
    const std::string key = std::string(" ") + name + "=";
    const size_t k = rep.find(key, d);
    if (k == std::string::npos) return false;
    return std::sscanf(rep.c_str() + k + key.size(), "%lf,%lf,%lf,%lf", &mean, &p99, &n, &msps) == 4;
}

int main() {
    // ── T1 — statistics. t_arm = 0, warm-up 0: every sample is kept. 100 samples 1..100 ms of C.convert, stamped
    //    at t = 10..1000 ms -> window = 1.0 s; sum = 5050 ms -> 5050 ms/s; mean 50.5; p50 = 50; p99 = 99; max 100.
    std::printf("T1 statistics\n");
    {
        SiteBook b(1024u, 0.0, 0.0);
        for (int i = 1; i <= 100; ++i) b.record(Site::C_CONVERT, (double)i, 10.0 * i);
        const SiteStats s = b.stats(Site::C_CONVERT);
        check(s.n == 100, "n = 100");
        check(approx(s.mean, 50.5), "mean = 50.5");
        check(approx(s.p50, 50.0), "p50 (nearest rank) = 50");
        check(approx(s.p99, 99.0), "p99 (nearest rank) = 99");
        check(approx(s.max, 100.0), "max = 100");
        check(approx(b.window_s(), 1.0), "window = 1.0 s (warm-up end 0 -> the latest sample at 1000 ms)");
        double mean, p99, n, msps;
        const std::string rep = b.report(kNoNotes);
        check(parse_done(rep, "C.convert", mean, p99, n, msps) && approx(msps, 5050.0, 1e-3), "ms/s = sum / window = 5050");
        SiteBook one(8u, 0.0, 0.0);
        one.record(Site::F_MATCH, 3.0, 5.0);
        const SiteStats o = one.stats(Site::F_MATCH);
        check(o.n == 1 && approx(o.p50, 3.0) && approx(o.p99, 3.0), "one sample: p50 = p99 = the sample");
        check(one.stats(Site::F_PYRAMID).n == 0, "a site never recorded has n = 0");
    }

    // ── T2 — warm-up. t_arm 1000, warm-up 5000 -> samples before t = 6000 are counted, not kept.
    std::printf("T2 warm-up exclusion\n");
    {
        SiteBook b(1024u, 1000.0, 5000.0);
        for (int i = 0; i < 10; ++i) b.record(Site::F_MATCH, 1.0, 1000.0 + 500.0 * i);   // t = 1000..5500: all warm-up
        for (int i = 0; i < 4; ++i)  b.record(Site::F_MATCH, 2.0, 6000.0 + 500.0 * i);   // t = 6000..7500: kept
        check(b.warmup(Site::F_MATCH) == 10, "10 warm-up samples counted");
        check(b.stats(Site::F_MATCH).n == 4, "4 samples kept");
        check(approx(b.stats(Site::F_MATCH).mean, 2.0), "no warm-up value leaks into the mean");
        check(approx(b.window_s(), 1.5), "window = 7500 - 6000 = 1.5 s (starts at the warm-up's end)");
        const std::string rep = b.report(kNoNotes);
        check(rep.find("10 warm-up samples not kept") != std::string::npos, "the report states the warm-up count");
    }

    // ── T3 — capacity. cap 5: the 6th..9th samples are dropped and counted.
    std::printf("T3 capacity\n");
    {
        SiteBook b(5u, 0.0, 0.0);
        for (int i = 1; i <= 9; ++i) b.record(Site::P_WARP, 0.1, (double)i);
        check(b.stats(Site::P_WARP).n == 5, "5 kept");
        check(b.dropped(Site::P_WARP) == 4, "4 dropped, counted");
        b.lost(Site::F_SPAN);
        const std::string rep = b.report(kNoNotes);
        check(rep.find("P.warp(dropped 4, lost 0)") != std::string::npos, "the report names the dropped samples");
        check(rep.find("F.span(dropped 0, lost 1)") != std::string::npos, "the report names a lost lane read");
    }

    // ── T4 — the F-thread remainder and the lane check.
    std::printf("T4 remainder + lane check\n");
    {
        SiteBook b(64u, 0.0, 0.0);
        // Two pairs: iter 8 and 6 ms; named waits 5 and 3 ms -> F.cpu 3 and 3 ms. Stamped at 500 and 1000 ms.
        b.record(Site::F_ITER, 8.0, 500.0);  b.record(Site::F_CPU, 3.0, 500.0);
        b.record(Site::F_ITER, 6.0, 1000.0); b.record(Site::F_CPU, 3.0, 1000.0);
        b.record(Site::F_SPAN, 2.0, 500.0);  b.record(Site::F_SPAN, 2.5, 1000.0);
        b.record(Site::F_SUBMIT_WAIT, 4.0, 500.0); b.record(Site::F_SUBMIT_WAIT, 3.0, 1000.0);
        b.record(Site::F_MATCH, 1.0, 500.0); b.record(Site::F_MATCH, 1.0, 1000.0);
        const std::string rep = b.report(kNoNotes);
        check(rep.find("F pairs 2.0/s") != std::string::npos, "F pairs/s = F.iter n / window (2 over 1.0 s)");
        check(rep.find("F.iter mean 7.0000 ms") != std::string::npos, "remainder line: F.iter mean 7");
        check(rep.find("mean 3.0000 p99 3.0000 ms = 6.000 ms/s") != std::string::npos, "remainder line: F.cpu 3 ms = 6 ms/s");
        check(rep.find("lane F: GPU span mean 2.2500") != std::string::npos, "lane F: span mean 2.25");
        check(rep.find("host minus span 1.2500 ms per submit") != std::string::npos, "lane F: submit_wait 3.5 - span 2.25 = 1.25");
        check(rep.find("named GPU sites sum to 2.000 GPU-ms/s = 1.0000 GPU-ms per source frame") != std::string::npos,
              "GPU remainder: F.match 2 ms over 1 s = 2 GPU-ms/s = 1 GPU-ms per F pair (spans are NOT summed)");
    }

    // ── T5 — the parseable line and the lane notes.
    std::printf("T5 the done-line + lane notes\n");
    {
        SiteBook b(64u, 0.0, 0.0);
        b.record(Site::C_UPLOAD, 0.25, 250.0); b.record(Site::C_UPLOAD, 0.75, 500.0);
        b.record(Site::B_MATCH, 1.5, 500.0);
        const char* const notes[4] = {"", "", "bidir is off: there is no backward flow", ""};
        const std::string rep = b.report(notes);
        double mean = 0, p99 = 0, n = 0, msps = 0;
        check(parse_done(rep, "C.upload", mean, p99, n, msps), "C.upload present in the done-line");
        check(approx(mean, 0.5, 1e-4) && approx(p99, 0.75, 1e-4) && approx(n, 2.0) && approx(msps, 2.0, 1e-3),
              "C.upload = 0.5000,0.7500,2,2.000 (mean, p99, n, ms per s over the 0.5 s window)");
        check(!parse_done(rep, "F.match", mean, p99, n, msps), "a site with no samples is absent from the done-line");
        check(rep.find("lane B (bwd flow, A.q2): NOT ARMED -- bidir is off") != std::string::npos, "a lane note reaches the report");
        check(rep.find("no samples:") != std::string::npos && rep.find("F.match") != std::string::npos,
              "sites without samples are listed, not silently absent");
        const size_t d = rep.find("[site] done (");
        check(d != std::string::npos && rep.find(")\n", d) != std::string::npos && rep.find('\n', d) == rep.size() - 1,
              "the done-line is ONE line and the last one");
    }

    // ── T7 — the absolute window (added after the OV1 judgement, 2026-09-18): unix = offset + stamp/1000. t_arm 1000,
    //    warm-up 5000 -> the window starts at stamp 6000 (unix 1006.000 with offset 1000) and ends at the last kept
    //    sample, stamp 9000 (unix 1009.000); both appear in the header and in the done-line.
    std::printf("T7 the absolute window\n");
    {
        SiteBook b(16u, 1000.0, 5000.0);
        b.set_epoch_offset(1000.0);
        b.record(Site::F_MATCH, 1.0, 7000.0); b.record(Site::F_MATCH, 1.0, 9000.0);
        const std::string rep = b.report(kNoNotes);
        check(rep.find("t0_epoch=1006.000 t1_epoch=1009.000") != std::string::npos, "done-line carries t0_epoch / t1_epoch");
        check(rep.find("unix 1006.000 .. 1009.000") != std::string::npos, "the header carries the same window");
        double mean = 0, p99 = 0, n = 0, msps = 0;
        check(parse_done(rep, "F.match", mean, p99, n, msps) && approx(n, 2.0), "the site fields still parse after the new fields");
    }

    // ── T6 — a check can fail: a wrong expectation, seen red, then not counted.
    std::printf("T6 the checks can fail (one deliberate red, excluded from the count)\n");
    {
        SiteBook b(8u, 0.0, 0.0);
        b.record(Site::F_MATCH, 1.0, 1.0); b.record(Site::F_MATCH, 3.0, 2.0);
        const int before = g_fail;
        check(approx(b.stats(Site::F_MATCH).mean, 3.0), "(deliberately wrong) mean of {1,3} is 3 -- must print [FAIL]");
        const bool saw_red = (g_fail == before + 1);
        g_fail = before;
        check(saw_red, "the deliberate wrong expectation was seen red");
    }

    if (g_fail == 0) std::printf("OK: site_book -- all checks passed\n");
    else std::printf("FAILED: site_book -- %d check(s) failed\n", g_fail);
    return g_fail;
}
