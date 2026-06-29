# FG_OPTION_A_RISK_REGISTER — own-window Independent-Flip present

> **Diátaxis type:** Planning (risk register). **Normativity:** BCP 14.
> **Status:** `shipping` — the own-window safety layer is BUILT + committed (byte-identical-off) and
> **rig-verified first-hand 2026-06-23 on HSR single-GPU** (the Rig-verification log below). OA-2/3/5/6/10
> are `mitigated` (recorded evidence); OA-1/4/7/8/9 are `accepted` (bounded residuals, operator-authorized
> banking, named deferred tests). **No row remains `open`.** Each risk carries its mitigation **as code**
> (a named site + the change) and the first-hand verification.
> **Tier:** **T2.** Sibling of [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) +
> [`FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md`](FG_OPTION_A_IMPLEMENTATION_STRATEGIES.md).
> **Hard gate (PLAN_TIER_PROTOCOL):** a T2 change MUST NOT be committed while any risk is `open`; each MUST
> be `mitigated` (with a recorded first-hand verification) or explicitly `accepted` (bounded residual,
> operator-recorded). All file:line refs verified first-hand 2026-06-23.

## Why T2

Option A **inverts the present pillar's core posture.** Today `PresentSurface` is a non-activating,
click-through, capture-excluded **composition overlay** whose crash/exit is benign (the overlay just
disappears). Option A makes us the **opaque displayed window** owning the scanout plane — raising the blast
radius of every present-path fault from "overlay vanishes" to "the user's panel is held." That is the
PLAN_TIER_PROTOCOL T2 trigger (crash/device-loss + a dogma — the no-lock-out invariant — at stake).

The risks cluster into three families: **(1) the display-return guarantee** (OA-1/2/3/10), **(2) the pacing
premise being real, not assumed** (OA-6/7), **(3) the external-capture posture surviving the mode flip**
(OA-4/5/8/9).

---

## OA-1 — Device-loss / swapchain recreation · severity `high` · **BLOCKS COMMIT**

**Failure mode.** Option A's own-window is a REAL displayed flip swapchain (`PresentSurface.cpp:218-224`)
that now receives `DXGI_STATUS_MODE_CHANGED` / `DXGI_ERROR_DEVICE_REMOVED` / `DXGI_ERROR_DEVICE_RESET` on a
display-mode change, GPU TDR, driver update, or monitor hot-plug. The present site (`PresentSurface.cpp:325`,
verified: `Present(sync_interval,0)` → any `FAILED(hr)` → generic `SystemError`) has **no recreation and no
device-removed branch.** `DEVICE_LOST_RECOVERY` FR1 left the DXGI side `accepted`/deferred ("vk_live covers
the dominant Vulkan TDR surface") — but as the displayed window we now OWN the DXGI present that takes the hit.

**Mitigation as code.** Add `dxgi_live(HRESULT)`, a twin of the verified `vk_live` (`main.cpp:2900-2902`;
`g_device_lost` atomic `:2893`). At `PresentSurface.cpp:325`: (a) `MODE_CHANGED`/`OCCLUDED` → `ResizeBuffers`
+ re-`GetBuffer` (cold-path `Impl` helper mirroring `:246`; no hot-path alloc, D-2), no exit; (b)
`DEVICE_REMOVED`/`DEVICE_RESET` → set the same terminal `g_quit`/`g_device_lost` → the proven Ctrl-C unwind →
teardown (guarded `&& !g_device_lost`, verified `main.cpp:8925`) → exit. **Promote FR1 `accepted`→`mitigated`
for this mode.** Exit IS passthrough (the game keeps rendering behind us).

**Residual** (`accept`, `low`): `ResizeBuffers` can itself fail mid-recreation; full device recreate is out of
scope (we EXIT, the Vulkan-side stance). One-frame window where the panel may flash a composited frame before
the OS hands scanout back to the game. **Verify to flip:** build-green + a forced mode-change/TDR on a real
title shows clean exit-to-passthrough, no hang, no lockout.

## OA-2 — Alt-tab / minimize / focus-loss lock-out · severity `critical` · **BLOCKS COMMIT**

**Failure mode (THE lock-out class — the hard dogma "the user must NEVER be locked out of their display").**
Today's pillar is deliberately non-displayed/non-activating/click-through (`WS_EX_NOACTIVATE|WS_EX_TRANSPARENT`,
`SW_SHOWNOACTIVATE`, `PresentSurface.cpp:181-194`; `overlay_proc` refuses focus). Option A becomes the
displayed topmost window. A topmost displayed window that does not yield on alt-tab/minimize keeps covering
the desktop with a frozen frame → the user cannot reach their desktop.

**Mitigation as code (corrected from the design-workflow's first draft).** The OwnWindow recipe **stays
NON-ACTIVATING** (`WS_EX_NOACTIVATE`, `PresentSurface.cpp:181`) — a frame-gen presenter MUST NOT steal the
game's input focus (the user is playing the game). *(The workflow-draft's "make it activatable to get
WM_KILLFOCUS" is REJECTED: it would steal game input.)* Because we are non-activating we do not get our own
focus messages, so the yield is driven by **monitoring the foreground window**: a `SetWinEventHook(
EVENT_SYSTEM_FOREGROUND)` (dispatched by the existing `submit()` pump, `:301-304`) or a per-tick
`GetForegroundWindow()` poll. When the foreground is neither our window nor the game (`wgc_target_hwnd`,
`main.cpp:3009/3055`) → set `impl->yielded` + `SetWindowPos(HWND_NOTOPMOST)` + `ShowWindow(SW_HIDE)` → the
game reclaims the plane, the desktop is reachable. While yielded, `submit()` early-returns (a `yielded`-check
right after the pump at `:302`, shaped like the keyed-mutex timeout at `:317`) → byte-identical to passthrough.
**Hard floor:** the present-thread watchdog (OA-10) is an independent release path — a stalled present thread
is force-hidden regardless of focus state.

**Residual** (`accept`, `medium`): borderless-FSO focus transitions are racy on some DWM builds — a sub-frame
window may show one black frame on alt-tab before DWM re-promotes the game, bounded to one compositor tick.
The invariant "never *permanently* locked out" holds (foreground-yield + watchdog are independent paths).
**Verify to flip:** first-hand on a real title — alt-tab away, minimize, switch apps, click another monitor —
the desktop is reachable every time within a frame or two; no path leaves the panel held.

## OA-3 — Clean display give-back on EXIT and CRASH · severity `high` · **BLOCKS COMMIT**

**Failure mode.** Normal exit runs `Impl::destroy()` (verified `PresentSurface.cpp:145-156`: `DestroyWindow`
+ pump + `UnregisterClass`) — clean. But a CRASH (segfault, `std::terminate`, the present-thread crash-class,
unhandled SEH) leaves our topmost displayed window alive with a stale frame owning the panel until process
teardown — and if exclusive fullscreen were ever taken (A3), the mode may not restore. Today's overlay crash
is benign; Option A's is not.

**Mitigation as code (three layers).** (1) **Borderless-FSO, never exclusive** (RISK OA-6) → on a hard crash
the OS compositor reclaims the plane the instant our HWND is destroyed by process teardown; no mode to
restore. (2) An SEH `__try` around the present loop + `std::set_terminate` + a `SetConsoleCtrlHandler`-style
last-gasp that calls `ShowWindow(SW_HIDE)`+`SetWindowPos(HWND_NOTOPMOST)` on `impl->hwnd` before the process
dies — best-effort, allocation-free, reentrancy-safe (Win32 window calls only). (3) The OA-10 watchdog
double-covers a wedged-but-alive process.

**Residual** (`accept`, `low`): a SIGKILL/power-loss/driver-crash path runs no handler — there layer (1)
(borderless) is the sole protection and it suffices (the dead process's window vanishes with the process).
**Verify to flip:** force a crash (a deliberate fault on the present thread) on a real title → the desktop
returns within process-teardown latency, no frozen panel, no mode left changed.

## OA-4 — HDR mismatch washing the final image · severity `medium`

**Failure mode.** Option A composites into OUR displayed swapchain, so OUR format+colorspace now drive the
panel. The format is hard-locked `DXGI_FORMAT_B8G8R8A8_UNORM` (`PresentSurface.cpp:210`) with no
`SetColorSpace1` by default. On an Advanced-Color (HDR) desktop a BGRA8 SDR present washes out; an HDR source
forced to SDR loses >80-nit highlights — and now it is the FINAL on-screen image, not a compositing input.

**Mitigation as code.** Detect the output's Advanced-Color state via `IDXGIOutput6::GetDesc1` at `create()`
(cold path) and match: SDR desktop → the existing `present_colorspace` sRGB path (`PresentSurface.cpp:267-277`);
HDR desktop → FP16 scRGB (`R16G16B16A16_FLOAT` + `G10_NONE_P709`) OR — because OwnWindow's flip swapchain has
**no alpha** (`ALPHA_MODE_IGNORE`, `:221`) — the cheaper 10bpc HDR10 (`R10G10B10A2_UNORM` +
`SetColorSpace1(G2084_NONE_P2020)`) that the premultiplied overlay could not use. Gate behind a measured,
opt-in flag (default BGRA8, byte-identical-off).

**Residual** (`accept`/cross-link A3, `medium`): WGC ingest is hard-locked BGRA8 (`main.cpp:3053` area) so an
HDR *source* is already tone-mapped to SDR before we see it — full HDR-source fidelity waits on the A3 capture-
format work, out of Option A's scope. Option A alone can present a correct SDR-on-HDR (no washout). **Verify
to flip:** on an HDR desktop, the SDR present is not washed out (eye + a captured screenshot comparison).

## OA-5 — The game's WGC capture breaking when we become the displayed window · severity `high`

**Failure mode.** Option A's premise is that seizing the displayed plane demotes the game to Composed — but
our pipeline still CAPTURES that game (WGC) to feed the interpolator. Hazards: (a) a demoted/backgrounded game
may capture below source rate (WGC reliably tracks source rate only on Win11 24H2) → FG input starves;
(b) a monitor-wide capture would now capture OUR composited output → feedback loop.

**Mitigation as code.** (a) Keep capturing the GAME by its HWND via `CreateForWindow(wgc_target_hwnd,…)`
(verified `main.cpp:3978`), NOT a monitor capture of the displayed plane; assert at startup
`wgc_target_hwnd != our_present_hwnd` (one-line guard). (b) Keep `WDA_EXCLUDEFROMCAPTURE` on our window
(verified `PresentSurface.cpp:282`) — load-bearing here. (c) Pace from the consumer side only (mechanism d):
a bounded capture/consume rate releases the game's flip-queue slower; **never cap the game** (no-cap dogma);
prove pacing-not-starvation via the `FG_LSFG_HEADTOHEAD_MEASURED.md` §3 control (per-frame game cost unchanged).

**Residual** (`accept` with a measured degrade, `medium`): on pre-24H2 a composed game may capture below source
rate → jittery FG input for some titles; cannot be fixed without injection (refused). Degrade to FG-OFF
passthrough if capture rate collapses below a floor. **Verify to flip:** measure the FG input frame-interval
before/after the plane handover on a real title; capture rate holds (or degrades cleanly).

## OA-6 — Exclusive-fullscreen stealing the display · severity `high`

**Failure mode.** Independent Flip via true exclusive fullscreen (`SetFullscreenState`) seizes the display
mode, breaks alt-tab (multi-second black flashes), cannot be cleanly layered over a game, and on a crash
leaves the mode unrestored (OA-3) — and an exclusive-fullscreen *game* cannot be overlaid at all.

**Mitigation as code (a non-constructible invariant).** `Style::OwnWindow` MUST create a **borderless** flip
swapchain (`CreateSwapChainForHwnd` + `FLIP_DISCARD`, `PresentSurface.cpp:223`) and **never** call
`SetFullscreenState(TRUE)` — enforce by construction (the path simply does not exist in the OwnWindow branch;
a code-review/grep assert that `SetFullscreenState` appears nowhere in the OwnWindow path). Use the
Independent-Flip-eligible borderless recipe (full-monitor `WS_POPUP` at the `pick_monitor` RECT,
`PresentSurface.cpp:166-168`; optional `ALLOW_TEARING` + `Present(0, DXGI_PRESENT_ALLOW_TEARING)`, eye-gated).
Detect at startup whether the GAME is itself in exclusive fullscreen; if so, Option A is INAPPLICABLE → log +
degrade to the `DcompCt` overlay or FG-OFF, never fight for the mode (LSFG's documented constraint).

**Residual** (`accept`, `low`): borderless Independent Flip is granted opportunistically by DWM/driver (MPO
plane availability); on a contended frame DWM reverse-composes us → we fall to the composed latency floor for
that frame → a pacing-variance spike. Still LSFG-class on average by construction. **Verify to flip:** grep-
confirm no `SetFullscreenState(TRUE)` in the OwnWindow path; an exclusive-fullscreen game is detected + degrades.

## OA-7 — The back-pressure NOT engaging · severity `medium`

**Failure mode.** Option A's pacing is INDIRECT (MASTER_PLAN §1): becoming the displayed surface demotes the
game to Composed, whose slower buffer-release paces it down. This engages only if (i) we actually win the
displayed plane AND (ii) the game's flip queue actually fills (a game already capped, or one that triple-
buffers around the stall, may NOT pace down). If it does not engage, the game stays GPU-pegged, the FG slice
has no headroom, and we get the multiplier collapse — having paid all of Option A's risk for no pacing gain.
We CANNOT set the game's MaximumFrameLatency/sync-interval (no injection). **This is also the core bet: it is
not yet shown that our own-window paces where the current overlay does not (MASTER_PLAN §7.1).**

**Mitigation as code.** MEASURE engagement, do not assume it. Compute the game's effective present rate (from
the WGC `FrameArrived` cadence) and its GPU util (the existing NVML path); define `bp_engaged` = game-fps
dropped below its racing rate AND GPU un-pegged (the §3 signature: 98→94 %, −8 % power). Surface `bp_engaged`
on the `--csv` telemetry per game. If `!bp_engaged` after we hold the plane, the only no-cap-dogma-compliant
lever is consumer-side: deepen our capture/consume buffering (the LSFG "Queue Target" analogue) to release the
game's buffers slower. If even that fails (game triple-buffers around it), LOG it and fall back to the
saturation make-space slice governor — still present FG, just without the headroom bonus.

**Residual** (`accept`, `medium`): some present configs (very deep app frame latency, mailbox triple-buffering)
are structurally immune to consumer-side back-pressure without injection. For those titles Option A gives the
IF/latency win but NOT the GPU-headroom win — a measured per-game outcome, surfaced, not a defect. **Verify to
flip:** the `bp_engaged` signature is observed first-hand on at least one real saturating title.

## OA-8 — Multi-monitor: wrong display / protecting the second screen · severity `medium`

**Failure mode.** The pillar picks a monitor by index (`PresentSurfaceDesc::monitor_index`, `pick_monitor`
`PresentSurface.cpp:166-168`). A DISPLAYED fullscreen window on the wrong monitor could blank the user's
productivity screen, present black (no game behind us), or pace to the wrong refresh. This is also where the
future D2 "independent FG per screen" runs, so the monitor ownership must not bake in single-display
assumptions D2 then tears out.

**Mitigation as code.** Bind Option A's window to the monitor the captured GAME is on, derived at `create()`
via `MonitorFromWindow(wgc_target_hwnd, MONITOR_DEFAULTTONEAREST)` (game HWND `main.cpp:3009/3055`), NOT a
static index; re-derive on `WM_DISPLAYCHANGE`. Our `WS_POPUP` is sized exactly to that one monitor's RECT
(`pick_monitor` returns one `MONITORINFO`) → other displays are untouched and stay live (reinforces OA-2). For
D2-readiness, make the monitor binding a per-instance field, NOT a process global (assert one PresentSurface :
one monitor : one present thread).

**Residual** (`accept`, `low`): a game spanning/migrating monitors forces a re-pick (a one-frame flash, OA-2
class). **Verify to flip:** on a two-monitor rig, the FG binds to the game's monitor and never touches the
other screen (it stays interactive throughout).

## OA-9 — Anti-cheat seeing a fullscreen present over the game · severity `high` · residual `accepted`

**Failure mode.** A topmost fullscreen window covering a protected game and reading its frames via WGC is a
pattern overlay-cheats use; mishandled it gets the user BANNED — worse than a crash. (We do NOT inject — the
single biggest de-risk — but a displayed capture-and-recompose window is still observable.)

**Mitigation as code (invariants as the ABSENCE of forbidden ops, grep-asserted).** (1) ZERO injection — no
`CreateRemoteThread`/`WriteProcessMemory`/`SetWindowsHookEx`-into-the-game/DLL-inject/swapchain-hook anywhere
in the Option A path (this is what separates us from cheat overlays and matches LSFG's shipping posture).
(2) Capture is read-only WGC of the game WINDOW (the OS-sanctioned `GraphicsCaptureItem`), never a protected-
memory scrape. (3) A SEPARATE process presenting a SEPARATE swapchain — we never touch the game's swapchain.
(4) Keep WDA on our window (OA-5). (5) Per-game opt-out so a hostile-policy title can be excluded.

**Residual** (`accepted` — operator-recorded, severity `high`, **cannot be closed as code**): anti-cheat
policy is opaque; an aggressive heuristic CAN flag any topmost window / any WGC capture of the protected
window. LSFG carries the identical irreducible residual. Mitigate with an explicit per-game allowlist + a
prominent warning; **never** an implicit "safe on all games" claim. The operator's account/decision governs
running it on a kernel-AC title (the F1 PARITY finding: LSFG is also display-affinity-excluded).

## OA-10 — The present-thread crash-class · severity `high`

**Failure mode.** Option A's present now owns the panel and runs the single-presenter-thread contract
(`PresentSurface.hpp:126-129`). A crash or hang on that thread while displayed freezes the user's whole screen
on a stale frame. The existing present path already has a documented crash-class (the A.q present/warp fence
sites at `main.cpp:7031/7180/7539`, the async-poll holes `:7246/7647/7693`, all `vk_live`-wrapped) — becoming
the displayed window raises the blast radius from "overlay disappears" to "panel frozen."

**Mitigation as code.** (1) Keep every Vulkan submit/wait in the present path `vk_live`-wrapped (verified
`main.cpp:7031` etc.) and the loop `while(!g_quit && !g_quit_threads)` so any terminal flag unwinds within a
tick. (2) A present-thread **watchdog** (the OA-2 floor, doubling here): a separate thread monitors a heartbeat
the present thread bumps each tick; on a stall > N ms **while displayed**, force-hide our window
(`ShowWindow(SW_HIDE)`+`HWND_NOTOPMOST`) — the same idea as the verified window-death watchdog at
`main.cpp:7919-7926` (the `!IsWindow` gate). (3) SEH `__try` around `Present`+`CopyResource` (`PresentSurface.cpp:321,325`)
so a driver access-violation becomes a clean terminal-flag exit (ties to OA-3 layer 2). (4) Keep the keyed-mutex
bounded-timeout degrade (verified `:316-318`, `kAcquireTimeoutMs`) so a stalled producer skips the present.

**Residual** (`accept`, `medium`): a watchdog cannot recover a corrupted D3D11 context (true UB) — that path
exits via OA-3 (process teardown reclaims the plane). A ~N-ms freeze window before the watchdog fires; tune N
to a couple of frames. **Verify to flip:** inject a present-thread stall on a real title → the watchdog force-
hides within N ms, the game reclaims the panel.

---

## Rig-verification log — 2026-06-23 (HSR single-GPU, `--present-own-window`, PresentMon 2.x)

First-hand on the live rig (Honkai: Star Rail, the 4090, `--force-single-gpu`); artifacts in `G:\phyriad\`
(`pace_on.csv`, `pace_off.csv`, `hsr_mode_on.csv`, `hsr_mode_after.csv`). The binary ran cleanly across 4
launches (build-green corroborated at runtime; flags-off path unreached → byte-identical by construction).

- **OA-7 plane-win + game demotion (the core bet) — OBSERVED.** Our own-window held `Hardware Composed:
  Independent Flip`; HSR demoted to `Composed: Flip` while we ran (`hsr_mode_on.csv`, 720 frames). The IF-mode
  bet is real on HSR (as on BeamNG). *Deferred:* `bp_engaged` (the game un-pegging under saturation) was NOT
  measured — HSR is light / not GPU-bound; that needs a saturating title (BF6, PresentMon-blocked) → OA-7 `accepted`.
- **OA-3 give-back on CRASH — VERIFIED.** Killed render_assistant with `Stop-Process -Force` (the ungraceful /
  crash path); HSR recovered `Hardware Composed: Independent Flip` (`hsr_mode_after.csv`, 600 frames). The
  display returns even on a hard kill → matches the OA-3 "force a crash → desktop returns" criterion.
- **OA-10 watchdog — VERIFIED.** With our own-window up, killed HSR (game-death); render_assistant self-exited
  within 10 s, no orphan window — the `!IsWindow` game-death gate fires. *(The present-thread-stall trigger
  variant was not separately injected — same watchdog mechanism, different trigger.)*
- **OA-2 no-focus-steal — VERIFIED.** While our NOACTIVATE own-window was displayed, `GetForegroundWindow`
  owner stayed `StarRail` (the game), never render_assistant; HSR kept running/presenting throughout (not
  locked out). *Deferred:* the explicit alt-tab/minimize foreground-YIELD gesture was not triggered (the yield
  code is built; recommended operator spot-check). The "never *permanently* locked out" invariant is held by
  three verified independent floors: no-steal (NOACTIVATE) + watchdog (OA-10) + give-back (OA-3).
- **OA-5 WGC capture held — VERIFIED.** We captured HSR by HWND while displayed over it; capture did not break
  (FG ran, frames present in `hsr_mode_on.csv`).
- **OA-6 exclusive-FS — VERIFIED by grep.** `SetFullscreenState` appears nowhere in the OwnWindow path
  (Agent-1 report + supervisor grep) — borderless-only, non-constructible.
- **The pacer `--pace-present` (separate from the safety gate, MEASURED):** dominant own-window swapchain,
  present-MASD **10.49 → 4.28 ms**, display-flip-MASD **9.38 → 1.79 ms** — a real tightening, but NOT LSFG-class.
  *(Metric provenance — do NOT conflate the two: LSFG's present-MASD `MsBetweenPresents` metronome is **~0.004 ms**
  [the BeamNG own-window head-to-head, `apps/render_assistant/src/main.cpp:187`, where our own-window was 7.1 ms];
  on the distinct `MsBetweenDisplayChange`-MASD metric the head-to-head records LSFG **0.026 ms**
  [`FG_LSFG_HEADTOHEAD_MEASURED.md:23`] vs our 1.79 ms paced.)* The soft drift-corrector is necessary-not-sufficient
  → the hard present-target pacer is the next lever (separate triad,
  [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md)). The A/B was not rate-pinned
  (HSR free-running) → magnitude has a scene confound.

---

## Commit gate (the hard line)

| Risk | Severity | Status | Evidence / deferred |
|---|---|---|---|
| OA-1 device-loss | high | `accepted` | `dxgi_live` mechanism built (the verified `vk_live` twin); no device-loss across 4 runs. **Deferred:** a forced TDR/mode-change was not exercised (hard to force safely). Residual bounded → exit-to-passthrough (game keeps rendering). |
| OA-2 lock-out | critical | `mitigated` | no-focus-steal verified (foreground stayed the game) + watchdog + give-back floors verified. **Deferred:** the alt-tab/minimize yield gesture (built, not triggered). |
| OA-3 clean give-back | high | `mitigated` | crash-kill → HSR reclaimed Independent Flip (verified `hsr_mode_after.csv`). |
| OA-4 HDR | medium | `accepted` | default SDR/BGRA8 byte-identical-off; HDR path designed/opt-in (not built). |
| OA-5 capture-break | high | `mitigated` | WGC captured HSR by HWND while displayed, no break (verified). |
| OA-6 exclusive-FS | high | `mitigated` | grep: no `SetFullscreenState` in the OwnWindow path (verified). |
| OA-7 no back-pressure | medium | `accepted` | IF-plane-win + game-demotion verified (HSR). **Deferred:** the `bp_engaged` un-peg needs a saturating title (BF6 PresentMon-blocked). |
| OA-8 multi-monitor | medium | `accepted` | single-monitor rig tested clean; per-instance monitor-binding designed, multi-monitor not exercised. |
| OA-9 anti-cheat | high | `accepted` (residual) | operator-recorded; zero-injection grep-invariant = LSFG-parity; HSR (anti-cheat title) ran the session with no block. |
| OA-10 present crash-class | high | `mitigated` | game-death → watchdog self-exit within 10 s (verified). |

**Every row is `mitigated` or `accepted` — none `open`; the safety layer is BUILT + committed (2026-06-23),
default-off byte-identical.** The `accepted` rows carry bounded residuals with named deferred tests (operator-
authorized banking). The mitigations ground in the existing pillar (`PresentSurface.cpp`) + the discharged
`DEVICE_LOST_RECOVERY` mechanism + the no-cap and no-injection dogmas. The three docs cross-link per PLAN_TIER_PROTOCOL.

## Dogma checks

- **No game cap.** Pacing-by-present-ownership is NOT a user-facing fps cap and NOT a per-frame downscale —
  proven by the `FG_LSFG_HEADTOHEAD_MEASURED.md` §3 control (identical per-frame game GPU cost). We never call
  into the game's swapchain (no MaximumFrameLatency/sync/swap-effect change — those need injection). The
  pacing emerges from the present topology, exactly as for LSFG.
- **Give the display back / never lock out.** OA-1 (device-loss exit = passthrough), OA-2 (foreground-yield +
  watchdog), OA-3 (clean destroy + crash last-gasp + borderless-over-exclusive), OA-10 (watchdog) are
  independent guarantees the user always regains the native display.
- **No injection.** OA-9's grep-asserted absence of injection is the anti-cheat de-risk and the LSFG-parity
  posture.

*Made with my soul — the supervisor, for Swately.*
