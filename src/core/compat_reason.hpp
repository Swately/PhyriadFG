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
    WINDOW_NOT_FOUND     = 7,   // no window matched --hwnd / --window-pid / --window SUBSTR
    UNSUPPORTED_FORMAT   = 8,   // route_for() rejected the captured DXGI surface format
    CAPTURE_INIT_FAILED  = 9,   // backend device/duplication/session create failed for an OS reason
    PRESENT_INIT_FAILED  = 10,  // PresentSurface::create failed (no in-thread fallback → clean quit)
    REASON_UNCLASSIFIED  = 11,  // THE TRIPWIRE — none of the above; its count MUST be 0
    // Appended 2026-09-06 (QOL audit: I-3/B-1/E-4, I-6, B-3, B-6, OP-FGGPU-2). The set is APPEND-ONLY
    // by the contract at the head of this file: 12+ leaves every existing value — REASON_UNCLASSIFIED
    // included — exactly where it was, so an old log stays comparable. The numbering below is the
    // RECONCILED one: three independent designs each appended starting at 12, so the two device codes
    // were moved to 16/17 to keep every value unique.
    SOURCE_MINIMIZED     = 12,  // --window matched a MINIMIZED window (IsIconic / degenerate client rect)
    SOURCE_RESIZED       = 13,  // the captured source changed size mid-run; the pipeline is sized once at init
    SOURCE_CLOSED        = 14,  // the capture item's source (window destroyed / display removed) went away
    NO_FRAMES            = 15,  // the WGC session started but delivered no frame within the first-frame timeout
    // The device-init cold path violated this header's own opening contract — it bailed with a bare
    // printf and a `goto done` that fell through to `return 0`, so a failed run was byte-identical to a
    // clean quit as far as the launcher could tell.
    DEVICE_INIT_FAILED   = 16,  // a Vulkan device / host-bridge / pipeline init stage refused or failed
    DEVICE_LOST          = 17,  // VK_ERROR_DEVICE_LOST latched during the run — the exit is NOT clean
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
    case ReasonCode::SOURCE_MINIMIZED:     return "SOURCE_MINIMIZED";
    case ReasonCode::SOURCE_RESIZED:       return "SOURCE_RESIZED";
    case ReasonCode::SOURCE_CLOSED:        return "SOURCE_CLOSED";
    case ReasonCode::NO_FRAMES:            return "NO_FRAMES";
    case ReasonCode::DEVICE_INIT_FAILED:   return "DEVICE_INIT_FAILED";
    case ReasonCode::DEVICE_LOST:          return "DEVICE_LOST";
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
        return "no visible window matched the target identity (--hwnd, then --window-pid, then the "
               "--window title substring, in that order). Check that the app is running, not minimized, "
               "and that the pid/handle still belongs to it -- a handle dies when its window closes.";
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
    case ReasonCode::SOURCE_MINIMIZED:
        return "--window matched a MINIMIZED window. Its client rect is empty, so the capture would be "
               "sized to the WHOLE MONITOR and the window's content would sit in the top-left corner. "
               "Restore the window and start again.";
    case ReasonCode::SOURCE_RESIZED:
        return "the captured source CHANGED SIZE mid-run (resize / maximise / F11 / a DPI change). The "
               "whole pipeline — staging ring, WGC pool, Vulkan images, flow/warp extents — is sized ONCE "
               "at init and there is no re-init path, so continuing would silently crop or letterbox every "
               "remaining frame. PhyriadFG exits instead; restart at the new size.";
    case ReasonCode::SOURCE_CLOSED:
        return "the captured source was CLOSED by the OS (the window was destroyed, or the captured display "
               "was removed) — no further frames can arrive. PhyriadFG exits cleanly instead of "
               "re-presenting the stale pair forever.";
    case ReasonCode::NO_FRAMES:
        return "the capture session started but delivered NO frame within the first-frame timeout. The "
               "source may be minimized, occluded by an OS capture policy, or on a GPU this session cannot "
               "be fed from. PhyriadFG exits instead of idling.";
    case ReasonCode::DEVICE_INIT_FAILED:
        return "a device-init stage failed (Vulkan device, host bridge, images, pipelines or the present "
               "bridge). The line printed just above names the exact allocation that refused. This is a "
               "FATAL init bail: the process exits 1 and presents nothing.";
    case ReasonCode::DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST was latched during the run — PhyriadFG unwound cleanly but the run "
               "did NOT complete. The game is unaffected (we never hook its swapchain).";
    }
    return advisory(ReasonCode::REASON_UNCLASSIFIED);   // unreachable; the safe sentinel default
}

// The unclassified-failure tripwire counter. Incremented ONLY when a failure cannot be mapped to a
// named code. Plain non-atomic uint32_t.
//
// THREADING, corrected 2026-09-06: it is NO LONGER true that every emit site runs on the main thread
// at init/failure before the capture/present worker threads spawn. SOURCE_RESIZED is emitted from the
// WGC FrameArrived callback and SOURCE_CLOSED from GraphicsCaptureItem::Closed — both winrt
// thread-pool threads, each guarded one-shot by WgcCtx::bail_said (an exchange(true)), so each of
// those codes is emitted at most once per run. What those callbacks share with the main thread is
// std::printf, which is itself thread-safe; a concurrent bail can interleave lines but cannot corrupt
// state. This counter stays a plain uint32_t because it is bumped ONLY by REASON_UNCLASSIFIED, and no
// callback emits that code — every callback bail is a named one. If a callback path ever emits
// REASON_UNCLASSIFIED, this must become a std::atomic<uint32_t>.
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

// The FATAL latch. Set by emit_fatal() only; read once, at main()'s single `return`, so a failed init or
// a latched device loss exits NON-ZERO instead of falling through the shared `done:` teardown to
// `return 0`. Every emit_fatal site is the main thread at init, or the one-shot device-loss latch at
// teardown — the WGC callback bails above use plain emit(), not this — so a plain bool is enough.
inline bool g_fatal_reason = false;

// Emit a named reason AND latch the process as failed. `stage` (optional) names the init stage that
// refused, so the launcher log carries both the class (the token) and the site (the stage) even though
// the leaf allocation printed only its own bare line.
inline ReasonCode emit_fatal(ReasonCode r, const char* stage = nullptr) {
    g_fatal_reason = true;
    if (stage) std::printf("[ra] init stage FAILED: %s\n", stage);
    return emit(r);
}

}  // namespace ra::compat

// Made with my soul - Swately <3
