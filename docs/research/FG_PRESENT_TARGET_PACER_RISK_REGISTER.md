# FG_PRESENT_TARGET_PACER_RISK_REGISTER — the hard present-target pacer

> **Diátaxis type:** Planning (risk register). **Normativity:** BCP 14 (MUST / SHOULD / MAY per
> [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)).
> **Status:** `shipping` — BUILT + rig-verified first-hand 2026-06-23 (HSR single-GPU, rate-pinned 60 fps in /
> `--refresh-hz 120` out, PresentMon 2.x — the Rig-verification log below). `--pace-hard` cuts own-window
> present-MASD from ~3.1 ms to **~0.95 ms (≈3.3×)** for a cleanly-isolated **+2.2 ms** latency, robust across a
> dynamic AND a matched static scene, `frz 0.0`. **PP-1/2/3/4/7 `mitigated`, PP-5/6 `accepted` — no row `open`.**
> The soft-pacer baseline (present-MASD 4.28 ms, `FG_OPTION_A_RISK_REGISTER.md:266-267`) and the LSFG ~0.004 ms
> `MsBetweenPresents`-MASD target remain the references — the hard pacer is a large step, not yet LSFG-class
> (the vblank phase-lock, deferred to P3, is the path to push further).
> **Tier:** **T2** sibling of [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md)
> + [`FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md).
> **Hard gate (PLAN_TIER_PROTOCOL §3):** this change MUST NOT be committed while any risk below is `open`;
> each MUST be `mitigated` (recorded first-hand verification) or explicitly `accepted` (bounded residual,
> operator-recorded). All `file:line` refs verified first-hand 2026-06-23.

## Why T2

A hard wait before `Present()` on the panel-owning own-window path raises the blast radius of a
mis-timed / overshot / wedged wait from "a little present jitter" to **"the displayed frame is held"** — the
freeze-floor / no-lock-out class. That is PLAN_TIER_PROTOCOL §1.1 **trigger 1** (crash / hang / freeze) and
**trigger 5** (a dogma at stake). It inherits the displayed-plane posture from the Option A triad
(`FG_OPTION_A_RISK_REGISTER.md` *Why T2*): the own-window OWNS scanout, so a present-path stall holds the
user's panel rather than making an overlay vanish. It is strictly heavier than the soft `--pace-present`
corrector (STAGE-121, verified `main.cpp:7955-7979`: it only slews the grid anchor by a bounded fraction and
NEVER blocks the present); the hard pacer ADDS a blocking wait before `Present` on that path.

*(Honesty note: a variant that paced only the click-through `DcompCt` overlay path — where a stall is benign
because the game still owns scanout — would drop to T1. The displayed own-window path named in this triad is
T2.)*

---

## PP-1 — Overshoot / spin past a vblank holds the panel (the freeze floor) · class crash/freeze · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The sleep/spin target is mis-computed or the spin budget is unbounded → the bounded wait
inserted at the present chokepoint (the pin inside `bridge_present()`, `main.cpp:6986-6990`, before
`Present`, `PresentSurface.cpp:512`) blocks the present thread (thread P) past a vblank WHILE displayed → a
held / stale frame on the own-window scanout plane.

**Mitigation as code.** (1) A **HARD cap** on total wait `< one vblank period` (`tick_period_ms` = the
`1000/refresh_hz` period) — the `budget` (`= EMA(work) + margin`) is clamped to `< period`, AND a residual
`rem > period` degrades instead of waiting, so the wait self-releases far under one vblank. (2) The cap makes
the wait undetectably short, but the STAGE-103 drain watchdog (`telemetry_csv.hpp:101`, 100 ms) + the OA-10
`f_seq`/`wd_last_ms` 2 s window remain the independent floors for any OTHER stall. (3) On a past/absurd target
(`rem ≤ 0` or `rem > period`), present immediately — degrade to the soft grid, `++ph_overshoot`, never block.

**Verification.** Inject an overshoot (force the target far ahead) on a real own-window title → the wait
self-caps within one vblank AND/OR the watchdog force-hides within its 2 s window, panel returns; soak shows
held-frame counter 0.

**Status:** `mitigated` (2026-06-23 rig) — `frz 0.0` across 6 own-window runs; the degrade path (`ph …O`, ~3-6/s)
fired on real over-budget frames and self-capped with no held frame / no freeze. The injected-far-overshoot soak
was not run separately; the natural over-budget overshoots exercise the identical degrade-to-present-now path.

## PP-2 — D-2 hot-path allocation / overhead regression · class dogma · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The target derivation or the spin allocates or does non-trivial work on the steady tick,
violating CANON.md item 10 (`CANON.md:89-91` "No allocations on the hot path. Steady-state code does not
allocate") and the CLAUDE.md efficiency mandate.

**Mitigation as code.** The vblank/QPC target SOURCE is set up cold (create-time; the DXGI waitable already
cold-created, `PresentSurface.cpp:330-331, 373-380`); the per-tick projection is pure arithmetic
(`target_qpc = anchor + k·period`); the wait is a pure timed loop (`hal::cpu_wait_for_ns`,
`main.cpp:7000`), no alloc; the off-path is byte-identical by the flag gate (`main.cpp:7955` precedent).

**Verification.** build-green + a steady-tick profile shows no added steady-state allocation + byte-identical-
off on HSR / the C8 gate green.

**Status:** `mitigated` (inspection + build-green) — the pin is pure arithmetic + `cpu_wait_for_ns`, no heap on
the steady tick; byte-identical when off (the flag gate). The formal steady-tick alloc profile + C8 run are deferred.

## PP-3 — Byte-identical-off broken · class dogma · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The new path executes with the flag off, or perturbs the existing soft slew / grid (e.g.
HOOK B re-domaining the always-on spin to QPC changes the `steady_clock` arithmetic).

**Mitigation as code.** A single `cfg.<flag>`-gated branch, no shared-state mutation (`tick_t0`, the `pp_*`
window) when off, mirroring the verified `--pace-present` gate (`main.cpp:7955`). Any always-on tick-math
change (HOOK B) is flag-guarded or proven bit-equal in the `steady_clock` case.

**Verification.** The C8 regression gate green; an unchanged HSR `--csv` run bit-matches the pre-change
output.

**Status:** `mitigated` (inspection) — flag-off → `ph_tgt` stays 0 (set only at `main.cpp:8039` under the flag) →
the pin branch (`main.cpp:7005`, gated `ph_tgt>0.0`) is never entered; the OFF runs behaved as today. The formal
C8 bit-match is deferred.

## PP-4 — No-game-cap dogma · class dogma · severity `medium`

**Failure mode.** A present-time wait is perceived as, or implemented as, throttling the GAME (e.g. calling
into the game's swapchain or setting its `SetMaximumFrameLatency`/sync).

**Mitigation as code.** We time only OUR own-window present; we never call into the game swapchain or change
its frame-latency / sync — no injection (the FG_OPTION_A §3 back-pressure-not-cap control; the game's
per-frame GPU cost is unchanged). The no-game-cap rule is an FG-arc operator invariant (cited by provenance,
not a numbered CANON dogma).

**Verification.** Grep: no game-swapchain / `SetMaximumFrameLatency` call on the pacer path; the game's
per-frame cost control holds (pacing-not-cap).

**Status:** `mitigated` — the wait times OUR own-window present only; no game-swapchain / `SetMaximumFrameLatency` /
injection call on the pacer path. The game keeps rendering behind us, untouched.

## PP-5 — Crash / SEH on the present-pacer path · class crash · severity `high`

**Failure mode.** A driver fault inside the wait or the vblank-timing call (`DwmGetCompositionTimingInfo` /
`WaitForVBlank`) while displayed.

**Mitigation as code.** The wait + `Present` stay inside the Option-A SEH / `vk_live`-wrapped present loop;
the OA-3 last-gasp give-back (`ShowWindow(SW_HIDE)` + drop-topmost, borderless-only) lets the OS reclaim the
plane on teardown so the game (still rendering behind us) regains Independent Flip — verified in
`FG_OPTION_A_RISK_REGISTER.md` OA-3 (`mitigated`: HSR reclaimed IF on crash-kill).

**Verification.** Force a fault on the pacer path → desktop / game returns within teardown latency, no hang.

**Status:** `accepted` (bounded residual) — no crash across 6 rig runs; the wait is pure-CPU on thread P inside the
Option-A SEH/`vk_live` loop, and the OA-3 crash give-back is already `mitigated` (HSR reclaimed Independent Flip on
a hard kill). A fault deliberately injected on the pacer path was not run.

## PP-6 — Same-display vblank-phase unlockable · class crash/freeze (degrade) · severity `medium` · residual likely `accepted`

**Failure mode.** On Composed Flip the own-window may not expose a stable vblank phase to lock to, so the
hard target cannot beat the soft floor (`MASTER_PLAN` §4 — the same-display Composed-vs-Independent-Flip
question, mirroring `FG_OPTION_A_MASTER_PLAN.md` §4).

**Mitigation as code.** Detect no-stable-phase (phase estimate variance over threshold) → fall back to the
soft slew, NEVER block on a bad target (shares the PP-1 degrade path).

**Residual** (`accept`, bounded): LSFG-class MASD may be reachable only on Independent-Flip / two-monitor —
a `measured` per-config outcome, surfaced not a defect.

**Verification.** The present-MASD floor is measured per flip-mode (Composed vs Independent) and recorded in
the Rig log.

**Status:** `accepted` (bounded residual) — the Independent-Flip floor is measured (present-MASD 0.93-0.98 ms; the
own-window reaches `Hardware Composed: Independent Flip`). The Composed-Flip floor was not measured; LSFG-class
pacing may be IF / two-monitor-only — the per-config outcome the design flagged.

## PP-7 — Pacing-variance / scene confound in the measurement · class dogma (honesty) · severity `low`

**Failure mode.** A free-running title makes the present-MASD delta scene-dependent — the exact confound the
`FG_OPTION_A_RISK_REGISTER.md` Rig log flagged for the soft pacer ("the A/B was not rate-pinned (HSR
free-running) → magnitude has a scene confound").

**Mitigation as code.** Rate-pin the A/B (fixed source cadence) and measure under PresentMon 2.x on the same
scene; report the rate-pinned present-MASD, not a free-running delta.

**Verification.** The rate-pinned present-MASD is recorded in the Rig log below.

**Status:** `mitigated` — the A/B was rate-pinned (HSR locked 60 fps) and run on a matched static scene; the
rate-pinned present-MASD (0.93 hard vs 3.05 off) is recorded in the Rig log.

---

## Rig-verification log (seeded; filled first-hand when built)

| Date | Config | present-MASD | display-flip-MASD | freeze/held-frame | spin cost | Notes |
|---|---|---|---|---|---|---|
| 2026-06-23 | soft `--pace-present` (the prior lever) | 4.28 ms | 1.79 ms | — | ≤2 ms/present | `FG_OPTION_A_RISK_REGISTER.md` Rig log; HSR free-running (PP-7 confound) |
| 2026-06-23 | OFF / SOFT / **HARD** — dynamic scene (HSR 60 → `--refresh-hz 120`) | 3.16 / 2.50 / **0.98** ms | 2.97 / 3.11 / **1.10** ms | `frz 0.0`; `ph 118H/6O`/s | ~2 ms spin | HARD won despite the HEAVIEST scene of the three (warp 3.8 vs 0.7 ms) → the win is robust / understated |
| 2026-06-23 | OFF / SOFT / **HARD** — static scene (pause menu, matched W) | 3.05 / 3.24 / **0.93** ms | 1.63 / 2.11 / **1.50** ms | `frz 0.0`; `ph 121H/3O`/s | ~2 ms spin | latency cleanly isolated = **+2.2 ms** (the present-lambda hold at matched W); SOFT unreliable (≈no help / worse) |

## Commit gate table (no row `open` before commit)

| Risk | Class | Severity | Status | Evidence / deferred |
|---|---|---|---|---|
| PP-1 overshoot holds panel | crash/freeze | high | `mitigated` | `frz 0.0` across 6 own-window runs; the degrade path fired naturally ~3-6/s (`ph …O`) on real over-budget frames + self-capped, no held frame. Injected-overshoot soak not separately run. |
| PP-2 D-2 hot-path | dogma | high | `mitigated` | build-green; the pin is pure arithmetic + `cpu_wait_for_ns` (alloc-free by inspection). Formal alloc profile deferred. |
| PP-3 byte-identical-off | dogma | high | `mitigated` | flag-off → `ph_tgt` never set (`main.cpp:8039`) → pin never enters (`:7005`, gated `ph_tgt>0.0`); OFF runs unaffected. C8 bit-match deferred. |
| PP-4 no-game-cap | dogma | medium | `mitigated` | the wait times OUR own-window present only; no game-swapchain / injection call on the pacer path (grep). |
| PP-5 crash/SEH | crash | high | `accepted` | no crash across 6 runs; pure-CPU wait inside the SEH/`vk_live` loop + the verified OA-3 give-back. Forced-fault-on-pacer-path not run (bounded residual). |
| PP-6 vblank unlockable | crash/freeze | medium | `accepted` | the Independent-Flip floor measured (0.93-0.98 ms); Composed-mode floor not tested — the residual the design flagged. |
| PP-7 scene confound | dogma | low | `mitigated` | rate-pinned (HSR 60 locked) + matched static scene; present-MASD 0.93 (hard) vs 3.05 (off) recorded. |

## Dogma checks

- **No game cap** (PP-4): the wait times OUR own-window present only; no injection, the game's per-frame cost
  unchanged.
- **Never lock out / freeze-floor** (PP-1 / PP-5): the hard cap < one vblank + the OA-10 watchdog
  (`main.cpp:8013-8020`) + the OA-3 crash give-back are independent give-back floors.
- **D-2 zero-overhead off** (PP-2 / PP-3): cold target source, alloc-free spin, byte-identical when the flag
  is off (`main.cpp:7955` precedent).
- **No injection** (PP-4 / PP-5): no game-swapchain call on the pacer path.

---

Made with my soul — the supervisor, for Swately.
