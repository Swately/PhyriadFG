// PhyriadFG - src/present/stall_trace.hpp : the present loop's stall trace, pure (no clock of its own, no I/O), so a
// CPU test can pin it (tests/core/test_stall_trace.cpp).
//
// The tapped lever-1 runs lose 0.3-1 s of present-loop ticks at a time while the per-tick work ("iter worst") stays
// under 8 ms (docs/planning/records/LEVER1_RECORD.md s4.7). So the time goes somewhere the loop does not time. Each tick
// stamps five points; when the gap from one tick's top to the next exceeds the threshold, the report splits that gap
// into the previous tick's segments. Instrument only: nothing here changes what the loop does.
// Made with my soul - Swately <3
#pragma once

namespace pfg::present {

// One tick's stamps (ms, the loop's own now_ms()); -1 = the tick never reached that point.
struct TickStamps {
    double top      = -1.0;   // the loop body's first statement
    double bound    = -1.0;   // the tick boundary (before the pacing block)
    double t0p      = -1.0;   // after the pacing block (the loop's own t0_p, where "iter" starts)
    double iter_end = -1.0;   // where "iter" ends (before the per-second stats)
    double tail_end = -1.0;   // the WAP tick's last statement (after the stats and their prints)
};

struct StallReport {
    bool   report = false;
    double gap    = 0.0;      // top-to-top
    double top    = -1.0;     // top -> bound: the log block and the guards
    double pace   = -1.0;     // bound -> t0p: the pacing wait
    double tick   = -1.0;     // t0p -> iter_end: the timed tick work (selection, upload, warp, present)
    double tail   = -1.0;     // iter_end -> tail_end: the per-second stats and their prints
    double loop   = -1.0;     // tail_end -> the next top
};

// The previous tick's stamps against this tick's top. A gap at or under the threshold, or no previous tick, is not
// reported. A segment whose two ends were not both reached reads -1.
inline StallReport stall_check(const TickStamps& prev, double now_top, double threshold_ms) noexcept {
    StallReport r;
    if (prev.top < 0.0) return r;
    r.gap = now_top - prev.top;
    if (r.gap <= threshold_ms) return r;
    r.report = true;
    auto seg = [](double a, double b) noexcept { return (a >= 0.0 && b >= 0.0 && b >= a) ? b - a : -1.0; };
    r.top  = seg(prev.top, prev.bound);
    r.pace = seg(prev.bound, prev.t0p);
    r.tick = seg(prev.t0p, prev.iter_end);
    r.tail = seg(prev.iter_end, prev.tail_end);
    r.loop = seg(prev.tail_end, now_top);
    return r;
}

}  // namespace pfg::present
