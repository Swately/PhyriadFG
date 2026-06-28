// compat_reason.hpp — PhyriadFG compatibility reason-code taxonomy (graceful-fail-with-reason).
//
// Every excluded/broken capture-or-present cell is reached via a NAMED reason code with a one-line
// human advisory — NO silent failure, NO bare `return 1` on the cold path.
//
// SINGLE SOURCE OF TRUTH: this enum is the ONLY definition of the failure-reason set. A flat
// `enum class : uint32_t` with stable values, kept APP-LOCAL — this is product policy, not a
// pillar capability.
//
// COLD-PATH ONLY (the efficiency mandate): every function here runs at init / at a failure bail,
// never per-frame. The steady present path never includes this header → byte-identical when every
// cell is reachable (the reason machinery is inert on the happy path).
//
// REASON_UNCLASSIFIED is the tripwire sentinel: its runtime count must be 0 (every failure must map
// to a named code). Any failure that maps to none of the named codes increments it — it is never a
// silent pass. The FLOOR-tagged codes are SURFACED, never retried-to-victory: they explain an
// irreducible floor, they do not promise to defeat it.

#pragma once

#include <cstdint>
#include <cstdio>   // std::printf in emit() — the header is self-contained (cold-path only)

namespace ra::compat {

// The fixed, closed reason set. Stable uint32_t values — append only; never renumber (a log harness
// parses the machine token, and stable values keep the reason set comparable across runs).
enum class ReasonCode : uint32_t {
    OK                   = 0,   // capture+present init succeeded; not a failure (the happy-path verdict)
    EXCLUSIVE_FULLSCREEN = 1,   // FLOOR — target presents true exclusive-FS (bypasses DWM → WGC unreliable)
    PROTECTED_CONTENT    = 2,   // FLOOR — DRM/HDCP: first captured frame is black to ALL OS capture paths
    HYBRID_DD_WRONG_GPU  = 3,   // FLOOR (DD path) — Microsoft-Hybrid dGPU: DuplicateOutput → UNSUPPORTED
    WGC_UNSUPPORTED_OS   = 4,   // GraphicsCaptureSession::IsSupported()==false (build < 1803) — descend
    VRR_WILL_BE_DISABLED = 5,   // FLOOR (NVIDIA) — a non-hooking overlay can't drive the game's VRR (advisory)
    ANTICHEAT_UNVERIFIED = 6,   // a known kernel-AC title not in the verified set (advisory)
    WINDOW_NOT_FOUND     = 7,   // --window SUBSTR matched no visible window
    UNSUPPORTED_FORMAT   = 8,   // route_for() rejected the captured DXGI surface format
    CAPTURE_INIT_FAILED  = 9,   // backend device/duplication/session create failed for an OS reason
    PRESENT_INIT_FAILED  = 10,  // PresentSurface::create failed (no in-thread fallback → clean quit)
    REASON_UNCLASSIFIED  = 11,  // THE TRIPWIRE — none of the above; its count MUST be 0
};

// The machine token: the stable string a log harness greps from the run log. One per enumerator;
// the switch has NO default so a newly-added code that forgets a token is a -Wswitch warning at
// compile time, not a silent gap.
inline constexpr const char* token(ReasonCode r) {
    switch (r) {
    case ReasonCode::OK:                   return "OK";
    case ReasonCode::EXCLUSIVE_FULLSCREEN: return "EXCLUSIVE_FULLSCREEN";
    case ReasonCode::PROTECTED_CONTENT:    return "PROTECTED_CONTENT";
    case ReasonCode::HYBRID_DD_WRONG_GPU:  return "HYBRID_DD_WRONG_GPU";
    case ReasonCode::WGC_UNSUPPORTED_OS:   return "WGC_UNSUPPORTED_OS";
    case ReasonCode::VRR_WILL_BE_DISABLED: return "VRR_WILL_BE_DISABLED";
    case ReasonCode::ANTICHEAT_UNVERIFIED: return "ANTICHEAT_UNVERIFIED";
    case ReasonCode::WINDOW_NOT_FOUND:     return "WINDOW_NOT_FOUND";
    case ReasonCode::UNSUPPORTED_FORMAT:   return "UNSUPPORTED_FORMAT";
    case ReasonCode::CAPTURE_INIT_FAILED:  return "CAPTURE_INIT_FAILED";
    case ReasonCode::PRESENT_INIT_FAILED:  return "PRESENT_INIT_FAILED";
    case ReasonCode::REASON_UNCLASSIFIED:  return "REASON_UNCLASSIFIED";
    }
    return "REASON_UNCLASSIFIED";   // unreachable (the switch is exhaustive); the safe sentinel default
}

// The human advisory: the one-line honest string the operator reads. A FLOOR string states the
// floor plainly — it explains, it never promises to defeat it.
inline constexpr const char* advisory(ReasonCode r) {
    switch (r) {
    case ReasonCode::OK:
        return "capture+present init OK.";
    case ReasonCode::EXCLUSIVE_FULLSCREEN:
        return "the target presents EXCLUSIVE-FULLSCREEN — WGC/DD capture is unreliable there (it "
               "bypasses DWM; only injection reaches it = an anti-cheat ban). FLOOR: switch the game "
               "to BORDERLESS/windowed.";
    case ReasonCode::PROTECTED_CONTENT:
        return "the captured surface is PROTECTED (DRM/HDCP) — it is black to ALL OS capture paths, no "
               "software workaround. FLOOR: not a PhyriadFG fault.";
    case ReasonCode::HYBRID_DD_WRONG_GPU:
        return "Desktop Duplication is UNSUPPORTED on this Microsoft-Hybrid laptop dGPU "
               "(DuplicateOutput → DXGI_ERROR_UNSUPPORTED by design). FLOOR for the DD path — use "
               "WGC capture (--capture-api wgc).";
    case ReasonCode::WGC_UNSUPPORTED_OS:
        return "Windows Graphics Capture is unavailable on this OS build (GraphicsCaptureSession::"
               "IsSupported()==false; needs >= 1803). Falling back to Desktop Duplication.";
    case ReasonCode::VRR_WILL_BE_DISABLED:
        return "a non-hooking external overlay cannot drive the captured game's VRR (G-Sync off on "
               "NVIDIA; AMD/Intel FreeSync may persist — verify the OSD). FLOOR shared with LSFG; we "
               "never hook the game swapchain.";
    case ReasonCode::ANTICHEAT_UNVERIFIED:
        return "the target matches a known kernel-anti-cheat title not in the verified set — capture "
               "MAY be blocked or flagged server-side. Advisory only; PhyriadFG never injects.";
    case ReasonCode::WINDOW_NOT_FOUND:
        return "--window matched no visible window with that title substring. Check the title and that "
               "the game is running and not minimized.";
    case ReasonCode::UNSUPPORTED_FORMAT:
        return "the captured surface format is not one PhyriadFG routes (RGBA8/BGRA8/FP16-HDR/10bpc). "
               "Capture cannot proceed with this output format.";
    case ReasonCode::CAPTURE_INIT_FAILED:
        return "capture backend init failed for an OS reason (device/duplication/session create). "
               "Check the output index (--capture-monitor) and the capture API (--capture-api).";
    case ReasonCode::PRESENT_INIT_FAILED:
        return "PresentSurface::create failed — no in-thread fallback path exists; PhyriadFG quits "
               "cleanly (the game keeps rendering, this is pure passthrough exit).";
    case ReasonCode::REASON_UNCLASSIFIED:
        return "a failure that maps to NO named reason code — this is the unclassified-failure tripwire and a bug: "
               "the failure path must be classified.";
    }
    return advisory(ReasonCode::REASON_UNCLASSIFIED);   // unreachable; the safe sentinel default
}

// The unclassified-failure tripwire counter. Incremented ONLY when a failure cannot be mapped to a
// named code. Plain non-atomic uint32_t: every emit site runs on the main thread at init/failure,
// before the capture/present worker threads spawn — no hot-path concurrency, so no atomic is needed.
// Its value must be 0 over the test set (a failure that maps to no named code is a bug).
inline uint32_t g_unclassified_count = 0u;

// Emit a named reason to stdout in the "[ra] " house style. One call per surfaced cold-path
// condition. Prints the machine TOKEN + the human ADVISORY. On REASON_UNCLASSIFIED it bumps the
// tripwire counter. Returns the same code so a bail can `return emit(r), 1;` inline. COLD-PATH
// ONLY — never call this per frame.
inline ReasonCode emit(ReasonCode r) {
    if (r == ReasonCode::REASON_UNCLASSIFIED) ++g_unclassified_count;
    std::printf("[ra] REASON %s: %s\n", token(r), advisory(r));
    return r;
}

}  // namespace ra::compat

// Made with my soul - Swately <3
