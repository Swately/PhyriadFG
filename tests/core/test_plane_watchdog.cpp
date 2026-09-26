// PhyriadFG - tests/core/test_plane_watchdog.cpp : the OwnWindow present watchdog's decisions
// (framework/render/present/include/phyriad/render/present/PlaneWatchdog.hpp), tested without a window or a GPU.
// It pins the force-hide (only after a real stall, once, never before the first heartbeat, never while hidden) and
// its end (the stall measured at the first heartbeat after it, the plane re-asserted twice unless the foreground
// wants a yield). W10 replays the 2026-09-25 defect: a stall past the threshold must NOT leave the plane hidden for
// the rest of the run. Exit 0 = every case as expected.
// Made with my soul - Swately <3
#include "phyriad/render/present/PlaneWatchdog.hpp"

#include <cstdint>
#include <cstdio>

namespace {

using namespace phyriad::render::present;

int g_fail = 0;

void expect(bool cond, const char* what) {
    if (!cond) { std::printf("FAIL: %s\n", what); ++g_fail; }
    else       { std::printf("ok:   %s\n", what); }
}

constexpr int64_t kStall = 250;   // PresentSurface.cpp's kWatchdogStallMs

// A replay of one run: the present thread bumps the heartbeat every `period` ms except inside [stall_from,
// stall_to); the watchdog polls every 50 ms. The window is modelled as visible / hidden only.
struct Replay {
    int      hides = 0, ended = 0, reasserts = 0;
    int64_t  last_stall = -1;
    bool     visible = true;
};

Replay replay(int64_t end_ms, int64_t period, int64_t stall_from, int64_t stall_to, bool want_yield) {
    Replay out;
    WatchdogPoll poll; WatchdogResume res;
    int64_t hb = 0;
    bool wd_hidden = false;
    for (int64_t t = 1; t <= end_ms; ++t) {
        const bool beat = (t % period == 0) && !(t >= stall_from && t < stall_to);
        if (beat) {                                   // submit(): exchange the heartbeat, end a pending hide
            const int64_t prev = hb; hb = t;
            const bool ended = wd_hidden; wd_hidden = false;
            const WatchdogAction a = watchdog_on_heartbeat(res, ended, prev, t, want_yield);
            if (a.stall_ms >= 0) { ++out.ended; out.last_stall = a.stall_ms; }
            if (a.reassert) { ++out.reasserts; out.visible = true; }
        }
        if (t % 50 == 0) {                            // the watchdog thread
            if (watchdog_should_hide(poll, hb, t, wd_hidden, kStall)) {
                out.visible = false; ++out.hides; wd_hidden = true;   // hide posted first, then published
            }
        }
    }
    return out;
}

}  // namespace

int main() {
    std::printf("PLANE_WATCHDOG TEST: the present watchdog's force-hide and its end (PlaneWatchdog.hpp)\n");

    {   // W1: a live heartbeat never fires
        WatchdogPoll p; bool fired = false;
        for (int64_t t = 50; t <= 5000; t += 50) fired |= watchdog_should_hide(p, t, t, false, kStall);
        expect(!fired, "W1 a moving heartbeat never fires");
    }
    {   // W2: before the first heartbeat (hb == 0) nothing fires, however long
        WatchdogPoll p; bool fired = false;
        for (int64_t t = 50; t <= 5000; t += 50) fired |= watchdog_should_hide(p, 0, t, false, kStall);
        expect(!fired, "W2 no fire before the first heartbeat");
    }
    {   // W3: a stall fires once past the threshold, not at it
        WatchdogPoll p;
        (void)watchdog_should_hide(p, 100, 100, false, kStall);           // sees hb=100 at t=100
        const bool at  = watchdog_should_hide(p, 100, 350, false, kStall); // exactly 250 ms: not yet
        const bool past = watchdog_should_hide(p, 100, 351, false, kStall);
        expect(!at && past, "W3 fires strictly past the threshold (250 no, 251 yes)");
    }
    {   // W4: while hidden (yielded, or an unended watchdog hide) it never fires again
        WatchdogPoll p; (void)watchdog_should_hide(p, 100, 100, false, kStall);
        bool fired = false;
        for (int64_t t = 150; t <= 3000; t += 50) fired |= watchdog_should_hide(p, 100, t, true, kStall);
        expect(!fired, "W4 no fire while the plane is already hidden");
    }
    {   // W5: the end of a hide measures the stall and re-asserts, then once more, then nothing
        WatchdogResume r;
        const WatchdogAction a = watchdog_on_heartbeat(r, true, 96, 500, false);
        const WatchdogAction b = watchdog_on_heartbeat(r, false, 500, 504, false);
        const WatchdogAction c = watchdog_on_heartbeat(r, false, 504, 508, false);
        expect(a.stall_ms == 404 && a.reassert, "W5a the hide ends: stall 404 ms, re-assert");
        expect(b.stall_ms == -1 && b.reassert, "W5b the next heartbeat re-asserts once more (the posted-hide race)");
        expect(c.stall_ms == -1 && !c.reassert, "W5c then nothing");
    }
    {   // W6: the foreground wants a yield when the hide ends: measured, not re-asserted (the yield takes over)
        WatchdogResume r;
        const WatchdogAction a = watchdog_on_heartbeat(r, true, 96, 500, true);
        const WatchdogAction b = watchdog_on_heartbeat(r, false, 500, 504, false);
        expect(a.stall_ms == 404 && !a.reassert && !b.reassert, "W6 a yield wanted at the end: no re-assert, no second");
    }
    {   // W7: an ordinary heartbeat does nothing
        WatchdogResume r;
        const WatchdogAction a = watchdog_on_heartbeat(r, false, 96, 100, false);
        expect(a.stall_ms == -1 && !a.reassert, "W7 an ordinary heartbeat: no stall, no re-assert");
    }
    {   // W8: a run with no stall: no hide, the plane stays visible
        const Replay r = replay(20000, 4, -1, -1, false);
        expect(r.hides == 0 && r.ended == 0 && r.visible, "W8 no stall: 0 hides, plane visible");
    }
    {   // W9: a 200 ms hitch is below the threshold: no hide
        const Replay r = replay(20000, 4, 3000, 3200, false);
        expect(r.hides == 0 && r.visible, "W9 a 200 ms hitch: no hide");
    }
    {   // W10: the 2026-09-25 defect, replayed. One 400 ms stall early in the run: exactly one hide, its stall
        //      measured, and the plane VISIBLE at the end of the run (before the fix it stayed hidden to the end).
        const Replay r = replay(20000, 4, 3000, 3400, false);
        expect(r.hides == 1 && r.ended == 1, "W10a one stall: one hide, one end");
        expect(r.last_stall >= 400 && r.last_stall < 410, "W10b its stall measured (400-409 ms)");
        std::printf("      stall measured: %lld ms, re-asserts: %d\n", (long long)r.last_stall, r.reasserts);
        expect(r.visible, "W10c the plane is visible at the end of the run");
    }
    {   // W11: a longer stall later in the run: one hide, its stall measured, visible at the end
        const Replay s = replay(20000, 4, 9000, 9700, false);
        expect(s.hides == 1 && s.ended == 1 && s.last_stall >= 700 && s.last_stall < 710 && s.visible,
               "W11 a 700 ms stall later in a run: one hide, measured, plane back");
    }

    if (g_fail == 0) { std::printf("PLANE_WATCHDOG TEST: all passed\n"); return 0; }
    std::printf("PLANE_WATCHDOG TEST: %d FAILED\n", g_fail);
    return 1;
}
