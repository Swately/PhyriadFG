// PhyriadFG - tests/core/test_stall_trace.cpp : the present loop's stall trace (src/present/stall_trace.hpp), tested
// without a GPU. It pins when a tick-to-tick gap is reported (strictly over the threshold, never for the first tick)
// and how the gap is split into the previous tick's segments. Exit 0 = every case as expected.
// Made with my soul - Swately <3
#include "present/stall_trace.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_fail = 0;

void expect(bool cond, const char* what) {
    if (!cond) { std::printf("FAIL: %s\n", what); ++g_fail; }
    else       { std::printf("ok:   %s\n", what); }
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

pfg::present::TickStamps full(double top, double bound, double t0p, double iter_end, double tail_end) {
    pfg::present::TickStamps s;
    s.top = top; s.bound = bound; s.t0p = t0p; s.iter_end = iter_end; s.tail_end = tail_end;
    return s;
}

}  // namespace

int main() {
    using pfg::present::stall_check;
    std::printf("STALL_TRACE TEST: the present loop's tick-gap report (stall_trace.hpp)\n");

    {   // S1: an ordinary 4.2 ms tick is not reported
        const auto r = stall_check(full(1000.0, 1000.1, 1003.0, 1004.0, 1004.1), 1004.2, 100.0);
        expect(!r.report, "S1 a 4.2 ms tick: no report");
    }
    {   // S2: the first tick (no previous stamps) is never reported
        const auto r = stall_check(pfg::present::TickStamps{}, 5000.0, 100.0);
        expect(!r.report, "S2 no previous tick: no report");
    }
    {   // S3: exactly the threshold is not reported; just over it is
        const auto a = stall_check(full(0.0, 0.1, 1.0, 2.0, 2.1), 100.0, 100.0);
        const auto b = stall_check(full(0.0, 0.1, 1.0, 2.0, 2.1), 100.5, 100.0);
        expect(!a.report && b.report, "S3 100.0 ms not reported, 100.5 ms reported");
    }
    {   // S4: a 906 ms gap spent in the tail (the stats and their prints) is attributed there
        const auto r = stall_check(full(3000.0, 3000.2, 3004.0, 3008.0, 3904.0), 3906.0, 100.0);
        expect(r.report && near(r.gap, 906.0), "S4a the gap is 906 ms");
        expect(near(r.top, 0.2) && near(r.pace, 3.8) && near(r.tick, 4.0) && near(r.tail, 896.0) && near(r.loop, 2.0),
               "S4b split: top 0.2 + pacing 3.8 + tick 4.0 + tail 896.0 + loop 2.0");
        expect(near(r.top + r.pace + r.tick + r.tail + r.loop, r.gap), "S4c the segments sum to the gap");
    }
    {   // S5: a long pacing wait is attributed to pacing
        const auto r = stall_check(full(0.0, 0.1, 500.1, 504.0, 504.2), 504.3, 100.0);
        expect(r.report && near(r.pace, 500.0) && near(r.tick, 3.9), "S5 a 500 ms pacing wait: pace 500.0, tick 3.9");
    }
    {   // S6: a tick that left early (never reached iter_end / tail_end): the missing segments read -1
        pfg::present::TickStamps s; s.top = 0.0; s.bound = 0.1; s.t0p = 1.0;
        const auto r = stall_check(s, 400.0, 100.0);
        expect(r.report && near(r.pace, 0.9) && r.tick < 0.0 && r.tail < 0.0 && r.loop < 0.0,
               "S6 a tick that left early: tick/tail/loop read -1");
    }

    if (g_fail == 0) { std::printf("STALL_TRACE TEST: all passed\n"); return 0; }
    std::printf("STALL_TRACE TEST: %d FAILED\n", g_fail);
    return 1;
}
