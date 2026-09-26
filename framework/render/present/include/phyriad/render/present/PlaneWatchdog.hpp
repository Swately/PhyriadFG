// PhyriadFG framework - render/present : PlaneWatchdog.hpp - the OwnWindow present watchdog's two decisions, pure
// (no Win32), so a CPU test can pin them (tests/core/test_plane_watchdog.cpp).
//
// The watchdog thread polls the heartbeat that submit() bumps. If the heartbeat stops moving for longer than the
// stall threshold while the plane is displayed, the FG is taken as wedged and the plane is force-hidden, so it can
// never hold the user's panel on a stale frame. That hide is NOT a foreground yield. Before 2026-09-26 nothing ever
// re-asserted it: one stall past the threshold left the plane hidden, non-topmost and unlogged for the rest of the
// run (PhyriadFG docs/planning/records/LEVER1_RECORD.md s4.4). Now submit() ends the hide at the first heartbeat
// after the stall, measures the stall, and re-asserts the plane unless the foreground wants a yield.
// Made with my soul - Swately <3
#pragma once
#include <cstdint>

namespace phyriad::render::present {

// The watchdog thread's state across polls.
struct WatchdogPoll {
    int64_t last_seen      = -1;   // the heartbeat value seen at its last change
    int64_t last_change_at = 0;    // when it last changed (ms)
};

// Every poll: true = force-hide the plane now. There is nothing to guard while the plane is already hidden (yielded
// by the foreground monitor, or hidden by an earlier fire that submit() has not ended yet), nor before the first
// heartbeat (hb == 0). A fire re-arms the timer.
inline bool watchdog_should_hide(WatchdogPoll& p, int64_t hb, int64_t now_ms, bool hidden, int64_t stall_ms) noexcept {
    if (hidden) return false;
    if (hb != p.last_seen) { p.last_seen = hb; p.last_change_at = now_ms; return false; }
    if (hb != 0 && (now_ms - p.last_change_at) > stall_ms) { p.last_change_at = now_ms; return true; }
    return false;
}

// submit()'s state across heartbeats (the present thread only).
struct WatchdogResume {
    bool pending_second = false;   // re-assert once more on the NEXT heartbeat
};

struct WatchdogAction {
    bool    reassert = false;      // re-assert the plane now
    int64_t stall_ms = -1;         // the stall that ended on this heartbeat; -1 = none ended here
};

// Every heartbeat, after submit() stored the new one (prev_hb = the value it replaced; hide_ended = a watchdog hide
// was pending and is now consumed). A hide ends here, and its stall is now_ms - prev_hb. The plane is re-asserted
// unless the foreground wants a yield; in that case the yield takes over, and the yielded-to-displayed edge re-asserts
// it later. It is re-asserted TWICE, now and on the next heartbeat. The watchdog POSTS its hide from another thread,
// while a re-assert on the owner thread is synchronous, so a hide posted just before the flag was read can land after
// the first re-assert. The second one, after the next message pump, closes that race.
inline WatchdogAction watchdog_on_heartbeat(WatchdogResume& r, bool hide_ended, int64_t prev_hb, int64_t now_ms,
                                            bool want_yield) noexcept {
    WatchdogAction a;
    if (hide_ended) {
        a.stall_ms = (prev_hb > 0 && now_ms >= prev_hb) ? now_ms - prev_hb : 0;
        a.reassert = !want_yield;
        r.pending_second = a.reassert;
        return a;
    }
    if (r.pending_second) { r.pending_second = false; a.reassert = !want_yield; }
    return a;
}

}  // namespace phyriad::render::present
