# FG_FLUIDITY_FIX_RISK_REGISTER — the display-metronome + interpolation-phase fix (the "escalonado")

> **Diátaxis type:** Planning (risk register). **Normativity:** BCP 14 (MUST / MUST NOT / REQUIRED / SHALL /
> SHOULD / MAY per [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
> [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)) — read only in all capitals.
> **Status:** `designed` — **NOT built. No code ships from this triad.** The fluidity FIX (a uniform display
> metronome + correct interpolation-phase placement) is designed, not implemented; **every risk row below is
> `open`**. The only `measured` facts cited are the TB-C9 *baseline* escalonado numbers (synthesized-placement
> RMS ~1.55 ms, display-interval pacing stddev ~4.24 ms) — they are CITED from the testbench plan + the
> fluidity analyser, never re-derived here (FDP §4.6). Every `file:line` was verified first-hand against the
> working tree on **2026-06-25** (HEAD on `main`); the pacer triad's own dated `:line` refs (2026-06-23) have
> since drifted because `--pace-hard` shipped — the refs here are the *current* tree.
> **Tier:** **T2 (risk-bearing)** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1.1. The fix
> changes the FG **present timing + interpolation-phase placement** on the displayed **own-window** path
> (`Style::OwnWindow`, the plane that OWNS scanout), tripping **trigger 1** (crash / device-loss / hang /
> freeze — it touches the `vkWaitForFences(fBridge)` present path and inserts a CPU hold before `Present`),
> **trigger 2** (concurrency — any move of the present off thread P breaks the single-owner pacer-state
> invariant), and **trigger 5** (dogmas at stake — byte-identical-off, no-game-cap, the freeze-floor / no-lock-out
> invariant).
> **Triad (mutually linked, PLAN_TIER_PROTOCOL §2.3 — the siblings are authored by the supervisor; this
> register is the first leg written):**
> `FG_FLUIDITY_FIX_MASTER_PLAN.md` (the why · the two halves · the TB-C9 gate · build order) ·
> `FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES.md` (the per-edit-site strategy citing each FX-ID).
> **Hard gate (PLAN_TIER_PROTOCOL §3):** this change MUST NOT be committed while any risk below is `open`; each
> MUST be `mitigated` (recorded first-hand verification) or explicitly `accepted` (bounded residual,
> operator-recorded). No row may be silently dropped.
> **Evidence single-sources (this register CITES, never re-derives — FDP §4.6):**
> - The PACING half is **ADOPTED/INDEXED, not duplicated** from the shipping pacer triad
>   [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) +
>   [`FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md) +
>   [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md) (`--pace-hard` /
>   `--pace-present` / `--pace-vblank`, `shipping`). Its `PP-1..PP-7` rows own the present-fence / freeze-floor /
>   D-2 / no-game-cap mechanism mitigations; the FX rows below **link** to them and add only the *fluidity-fix-
>   specific* surface (the PLACEMENT half + the combined-change re-verification against the new TB-C9 gate).
> - The escalonado is MEASURED by **TB-C9** ([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §2/§5 ;
>   [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 TB-C9 ; analyser
>   [`../../scripts/fg_testbench_fluidity.py`](../../scripts/fg_testbench_fluidity.py)). The baseline numbers
>   are the TB-C9 plan's `measured` figures, cited.
> - The displayed-plane / own-window crash posture is inherited from
>   [`FG_OPTION_A_RISK_REGISTER.md`](FG_OPTION_A_RISK_REGISTER.md) (`OA-3` crash give-back, `OA-10` watchdog).

---

## §0 — Scope (and non-scope)

**In scope.** The risk treatment of the **fluidity FIX** — the change that eliminates the measured
"escalonado" (stepping) by combining **two halves**:

1. **PACING half** — present each frame at a uniform display-interval target (a vblank-phase metronome). This
   half is **already shipping** as the pacer triad's `--pace-hard` (+ `--pace-present` / `--pace-vblank`); the
   fluidity fix **adopts and indexes** it (motivating it from the now-measurable escalonado), it does **not**
   re-implement a pacer (FDP §4.6).
2. **PLACEMENT half** — the displayed interpolation phase `t_use` lands on a correct **uniform ladder**
   (`t = 0.5` for 2×, the geometric midpoint of DLSS3-style backward interpolation,
   [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §5) instead of the measured oscillation. This
   half is the **new code** — the content-clock / `sc-select` / phase-reshape work.

Each change is **gated by a measured TB-C9 fluidity-score drop** (placement RMS + pacing stddev) — the
temporal analogue of the C8 spatial-quality gate.

**Non-scope.** The pacer mechanism's own internal risks (`PP-1..PP-7`, present-fence overshoot, D-2, vblank
unlockability) — owned by [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md),
linked not copied. The spatial/image-quality cure (ghosting/disocclusion) — [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md).
The testbench/source build risks (`TB-1..TB-6`) — [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md).

---

## Why T2

The fix touches the **most sensitive path in the FG arc**: the displayed own-window present. A mis-timed CPU
hold, a fence interaction, or a forced phase placed wrong does not merely jitter present timing — on
`Style::OwnWindow` (`PresentSurface.cpp:360`, the OPAQUE flip plane that OWNS scanout) it can **hold or tear
the user's panel**, **wedge on the unbounded copy-into-bridge fence** (`vkWaitForFences(A.dev,1,&fBridge,
VK_TRUE,UINT64_MAX)`, `main.cpp:7559`), or **reintroduce freezes** (the deficit / repeated-frame path,
`main.cpp:8945`). The PLACEMENT half rewrites `t_use` (`main.cpp:8989`, `:9238-9239`) — the value that drives
both the warp (`wap_warp_present`, `main.cpp:9203`) and the cadence selection (`sc-select`, `main.cpp:8773-8798`)
— so a careless change can also **break existing cadence** (the STAGE-93 two-clock bug, the STAGE-94 vblend
text-misalignment ceiling). That is `PLAN_TIER_PROTOCOL` §1.1 triggers 1 + 2 + 5. The Tier-2 triad is therefore
mandatory; this register is the pre-commit checklist.

---

## FX-1 — Present-fence × metronome interaction: stall / torn-present / device-loss · class crash/device-loss · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The metronome inserts a CPU hold to `tgt + budget` at the present chokepoint (the `--pace-hard`
pin inside `bridge_present()`, `main.cpp:7415-7443`). That present path ALSO blocks thread P on an **unbounded
GPU fence**: the copy-into-bridge submit waits `vkWaitForFences(...,UINT64_MAX)` on the saturated 4090
(`main.cpp:7559`, also the steady-state sites `:7708` / `:8077`). Two ways this turns crash-class on the
displayed own-window plane: (a) if the placement-half re-timing moves the pin so its work-EMA `budget` no
longer counts the fence term, the hold + the fence COMPOUND and the present thread crosses a vblank → a
**held/torn frame** on scanout; (b) if the fence never signals (a TDR under saturation), the present thread
**wedges** and the device-loss surfaces as a hang on the panel rather than a recoverable error.

**Mitigation as code.** (1) **Keep the pin AFTER the fence.** `bridge_present()` is called from
`bridge_present_src()` (`main.cpp:7560`) immediately *after* the fence wait at `:7559`, so the pin's
`W = now_ms() − ph_tgt` (`main.cpp:7416`) already INCLUDES the fence term — `budget = EMA(W) + margin` is
correct by construction; the placement half MUST NOT relocate the pin earlier. (2) **Hard cap < one vblank.**
`ph_period_ms = 1000.0/cfg.refresh_hz` (`main.cpp:7418`); a residual `rem ≤ 0` or `rem > ph_period_ms` degrades
to present-now (`++ph_overshoot`, `main.cpp:7430`) — the CPU hold can never block past a vblank (this is the
pacer's `PP-1`, indexed). (3) **Device-loss stays caught.** The fence wait keeps its P0 TDR-catch
`vk_live(vkWaitForFences(...))` (`main.cpp:7559`, comment: "catch a TDR on the saturated 4090 → g_quit →
graceful exit"); the metronome adds NO new fence and does not change `sync_interval` (the present is
`Present(impl_->desc.sync_interval, 0)`, default 0, `PresentSurface.cpp:603`). (4) The independent floors hold:
the OA-10 watchdog (`PresentSurface.cpp:496`), the STAGE-103 drain watchdog (`telemetry_csv.hpp` `kStallMs=100`,
`:114`), and the window-death guard (`main.cpp:8569-8571`).

**Verification (first-hand, before this row flips `open`→`mitigated`).** build-green; a ≥30 s BF6/HSR own-window
soak with `--pace-hard` + the placement change → `telemetry_csv` `stall_count = 0` / `max_stall_ms < 100`
(`:406-407`), `frz 0`, no torn/held frame by eye; confirm the work-EMA `W` includes the fence (`W` ≈ fence+copy,
not ≈0) via the `ph:` DIAG; force a fence stall / induce a TDR under the saturation load → `vk_live` catches
device-loss → clean exit, panel returns, no hang.

**Status:** `open`.

## FX-2 — Drop-interpolated (async-present) × metronome interaction · class crash/concurrency · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The SOTA fix direction names `--async-present` (decouple present from the generation fence +
**drop the interpolated frame** when it is not ready, [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md)
§5). That work is **NOT shipped**. If built naively it collides with the metronome two ways: (a) **stale
target.** The tick boundary publishes `ph_tgt = tgt` (`main.cpp:8490`); the pin consumes it with `ph_tgt = 0.0`
(`main.cpp:7443`) only when `bridge_present()` runs. A tick that DROPS its interpolated frame never reaches
`bridge_present()` → `ph_tgt` stays non-zero into the next tick → the next pin computes `W` against a
two-ticks-old `ph_tgt` → wrong budget / overshoot. (b) **Concurrency hazard.** The pacer state
(`ph_tgt` / `ph_w_ema` / `ph_held` / `ph_overshoot`) is documented **thread-P-local**, hoisted above the
`bridge_present` lambda so `[&]` captures it on the single present thread (`main.cpp:7391-7400`). Moving the
present submission to a second thread would make that state cross-thread → a data race, violating Phyriad's
lock-free mandate (`PLAN_TIER_PROTOCOL` §1.1 trigger 2).

**Mitigation as code.** (1) **Async-present is OUT of P1 scope.** The P1 fluidity fix uses the SYNCHRONOUS pin
only (`--pace-hard` composed with the placement half); the drop path is deferred to its own designed change
(a bounded residual, recorded). (2) When async-present IS built, it MUST (i) reset `ph_tgt = 0.0` on a drop —
mirroring the consume at `main.cpp:7443` — so a stale target can never re-pin; and (ii) keep
`ph_tgt`/`ph_w_ema` **single-writer on thread P**: only the GPU present *submission* may move off P; the drop
decision and ALL pacer-state writes stay on P (the `PresentSurface.hpp` threading contract — the present stays
on the present thread; the strategies' "no new POD field, thread-P-local timing state" rule). (3) The freeze-
floor degrade (FX-3 / `PP-1`) absorbs a dropped tick: no fresh pair → the repeated-frame path
(`main.cpp:8945`) holds at `frz 0`, it does not wedge.

**Verification.** For P1: grep confirms no second present thread touches `ph_tgt`; the consume at `:7443` zeroes
it every presented tick; `lint_hal` clean (no raw `memory_order_*` on the pacer atomics). When async-present is
built: a soak with forced drops shows the next-tick `W` computed against a FRESH `ph_tgt` (the `ph:` DIAG), no
overshoot spike, `stall_count = 0`, and a ThreadSanitizer/inspection pass confirming single-writer.

**Status:** `open` (P1 synchronous-only; async-present explicitly deferred — bounded scope, re-opened in its own
register when designed).

## FX-3 — Phase-ladder change breaks existing cadence (sc-select / vblend) or the freeze-floor · class crash/freeze + dogma · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The PLACEMENT half rewrites the presented phase `t_use` (the final value at `main.cpp:8989`,
post phase-norm/reshape `:8994-9077`, written to the TB-C9 columns at `:9238-9239`). `t_use` drives BOTH the
warp (`wap_warp_present((float)t_use,...)`, `main.cpp:9203`) AND, via the same content-clock, the cadence
**selection** (`sc-select`, default-on, `main.cpp:8773-8798`, which picks the pair whose `cand_c ≥
content_clock`). Three ways a careless forced-`t=0.5` ladder regresses: (a) **breaks sc-select** — decoupling
the displayed phase from `content_clock` reintroduces the STAGE-93 two-clock bug (selection on one clock,
phase on another → a truncated ladder, start-stalls); (b) **fights vblend** — the velocity-continuity tilt
(default-on, `main.cpp:7937` / `:7975`) assumes the warp samples at the content-clock phase; a forced ladder
plus vblend's tilt double-correct → the STAGE-94 v1 fine-text misalignment; (c) **breaks the freeze-floor** —
a forced phase landing past the freshest pair reintroduces the deficit / repeated-frame freeze
(`disp_phase = 1.0`, `main.cpp:8945`) the ring + deficit-tier hold at `frz 0`.

**Mitigation as code.** (1) **Flag-gated, default-off** (a new `--pp-*`/`--sc-*` flag in the `parse_extra`
block, NOT the main `parse` chain — the C1061 nesting lesson; the precedent is `--pace-hard` parse at
`main.cpp:1223` and the `--phase-norm`/`--phase-reshape` default-off config at `:248` / `:256-258`). Default
leaves `t_use = phase_global` (`main.cpp:8989`) untouched → byte-identical (FX-4). (2) **Compose with
sc-select, do not bypass it** — drive the SAME `content_clock` toward the uniform ladder by slewing its NCO
increment `Δ = tick_period_ms / T_robust` (`main.cpp:8667`), NOT a hard snap; reuse the existing monotone-cubic
reshape `g(t)` (`main.cpp:9040-9077`) whose endpoints are EXACT (`g(0)=0`, `g(1)=1`) and which is
Fritsch-Carlson-clamped so it never reverses and never breaks text. (3) **Do not touch the deficit/repeated
path** — the freeze write at `main.cpp:8945` still fires when no fresh pair exists, so the freeze-floor is
untouched. (4) **vblend interaction** — either apply the reshape to the SAME `t_use` vblend reads, or gate the
two mutually; the default-off gate makes OFF byte-identical regardless (pinned in the strategies).

**Verification.** TB-C9 (`scripts/fg_testbench_fluidity.py`) placement RMS DROPS below the ~1.55 ms baseline
AND pacing stddev not worsened; `--phaselog` (`main.cpp:8298` / sample `:9120`) shows the `t_use` ladder
sweeping a uniform 0→1 (not the measured 0.77→0.32→0.75→0.30 oscillation); a fine-text scene shows no
misalignment by eye; `frz 0` across a 30 s soak; the OFF run byte-identical (FX-4).

**Status:** `open`.

## FX-4 — Byte-identical-off broken · class dogma · severity `high` · **BLOCKS COMMIT**

**Failure mode.** The fix (metronome adoption + placement reshape + the TB-C9 columns) executes with its flag
off, or perturbs the soft slew / grid / `content_clock` when off → not byte-identical (`DOGMA_SPECIFICATIONS`
byte-identical-off; the CLAUDE.md efficiency mandate's zero-overhead-off).

**Mitigation as code.** Every new lever is a default-off flag in `parse_extra` (the `--pace-hard` precedent
`main.cpp:1223`, the `--pp-*` precedent `:1219-1221`). The placement reshape default leaves
`t_use = phase_global` (`main.cpp:8989`); no shared state (`tick_t0`, `content_clock`, the `pp_*` window,
`ph_tgt`) is mutated by the new path when off (the `--pace-hard` gate precedent: flag-off → `ph_tgt` never set
at `:8490` → the pin at `:7415` is never entered). The TB-C9 telemetry is measurement-only: `disp_phase` /
`disp_src` default `-1.0` (`telemetry_csv.hpp:64-65`), written only inside the `--csv` path
(`D(fp_, r.disp_phase); D(fp_, r.disp_src)`, `:307`; header appended "positions of all prior columns
unchanged", `:275`) → absent `--csv` they cost nothing, absent the flag the present path is exactly today's.

**Verification.** The C8 FG-quality regression gate green; an unchanged HSR `--csv` run bit-matches the
pre-change output column-for-column; `lint_hal` clean (default `seq_cst` in `apps/`, no raw `memory_order_*`).

**Status:** `open`.

## FX-5 — No-game-cap dogma: the metronome must pace OUR present, never the game · class dogma · severity `medium`

**Failure mode.** The display metronome is perceived as, or implemented as, throttling the GAME — e.g. calling
into the captured game's swapchain, setting its `SetMaximumFrameLatency`/sync, or back-pressuring the WGC
capture to slow the source.

**Mitigation as code.** The metronome times only OUR own-window present: the pin is inside `bridge_present()`
(`main.cpp:7415`) and the present is OUR `IDXGISwapChain1::Present` (`PresentSurface.cpp:603`), not the game's;
no call into the game swapchain / `SetMaximumFrameLatency` / sync exists on the pacer or placement path; the
WGC capture is read-only and the source keeps rendering+presenting behind us untouched. This is the operator's
FG-arc invariant ("make-space, never recommend-cap"; provenance, not a numbered CANON dogma) and is identical
to the pacer register's `PP-4` (indexed, not duplicated).

**Verification.** Grep: no game-swapchain / `SetMaximumFrameLatency` call on the metronome/placement path; the
captured game's own present cadence (PresentMon on the game process) is unchanged with our metronome on vs off.

**Status:** `open`.

## FX-6 — Eye-veto / measurement validity: a TB-C9 win that the eye rejects, or a measurement artifact · class dogma (honesty) · severity `high` · **BLOCKS COMMIT**

**Failure mode.** A placement/pacing change IMPROVES the TB-C9 number but the operator's eye REJECTS it (the
**warp-scale lesson**: a metric win that *feels* worse), OR the TB-C9 improvement is a measurement artifact —
present-time used as a proxy for display-time (the SOTA's binding rule, §3: pace on `MsBetweenDisplayChange`,
NOT `MsBetweenPresents`), cross-clock present-timestamp drift, or a scene confound on a non-deterministic run.
The risk is shipping a perceptual regression dressed as a measured win — a reporting defect (FDP §4.1/§4.2).

**Mitigation as code / process.** A change closes ONLY when BOTH hold: (a) a **measured TB-C9 drop on the
DETERMINISTIC TB-C1 source** (`scripts/fg_testbench_fluidity.py` — fixed-seed, so the FG lever is the only
variable and the scene confound is escaped; placement RMS + pacing stddev reported SEPARATELY, never silently
collapsed, `fg_testbench_fluidity.py` analyser); AND (b) the **operator's eye on a real game** plus the
**TB-C6 photon-camera cross-check** (the analyser FLAGS that present-time is a proxy when no display-flip
timestamp exists, `fg_testbench_fluidity.py:294-295`, `pacing_is_display=0`; TB-C6 is the photon-truth
validation that the logged present cadence equals the actual display-flip). The TB-C9 number is
**necessary-not-sufficient**; the eye is the final arbiter. A change that wins TB-C9 but the eye rejects MUST
NOT ship — the row cannot flip to `mitigated`.

**Verification.** The TB-C9 A/B on TB-C1 (deterministic) records the drop; the operator eye-test on BF6/HSR
does not reject; where run, the TB-C6 camera agrees the display-flip cadence matches the logged present cadence
(catches the present-vs-display proxy error). All three recorded in the rig log below before the row closes.

**Status:** `open`.

---

## Commit-gate table (no row `open` before commit — PLAN_TIER_PROTOCOL §3)

| Risk | Class | Severity | Status | Closes when (first-hand) |
|---|---|---|---|---|
| FX-1 present-fence × metronome | crash / device-loss | high | `open` | 30 s soak `stall_count=0`/`frz 0`, pin after fence (`W` includes fence), `vk_live` catches an induced TDR → clean exit |
| FX-2 async-present × metronome | crash / concurrency | high | `open` | P1 synchronous-only (grep: no 2nd thread on `ph_tgt`; consume `:7443` zeroes it); async-present deferred to its own register |
| FX-3 phase-ladder breaks cadence / freeze | crash/freeze + dogma | high | `open` | TB-C9 placement RMS drops, `--phaselog` ladder sweeps uniform 0→1, no text misalignment, `frz 0`, OFF byte-identical |
| FX-4 byte-identical-off | dogma | high | `open` | C8 gate green + `--csv` bit-match OFF + `lint_hal` clean |
| FX-5 no-game-cap | dogma | medium | `open` | grep: no game-swapchain / `SetMaximumFrameLatency` on the path; game cadence unchanged on vs off |
| FX-6 eye-veto / measurement validity | dogma (honesty) | high | `open` | TB-C9 drop on TB-C1 (deterministic) AND operator eye does not reject AND TB-C6 cross-check agrees |

**ALL SIX ROWS ARE `open`. The fluidity FIX MUST NOT be committed in this state** (PLAN_TIER_PROTOCOL §3) — no
code ships from this triad until each row is `mitigated` (verified) or explicitly `accepted` with a recorded
residual.

## Dogma checks

- **Never lock out / freeze-floor** (FX-1 / FX-3): the hard cap `< one vblank` (`main.cpp:7418` /
  `:7430`) + the OA-10 watchdog (`PresentSurface.cpp:496`) + the drain watchdog (`telemetry_csv.hpp:114`) +
  the window-death guard (`main.cpp:8569-8571`) are independent give-back floors; the deficit/repeated path
  (`main.cpp:8945`) keeps `frz 0`.
- **No game cap** (FX-5): the metronome times OUR own-window present only (`main.cpp:7415`,
  `PresentSurface.cpp:603`); no injection, the game's per-frame cost unchanged.
- **Byte-identical / zero-overhead off** (FX-4): default-off flags in `parse_extra`; no shared-state mutation
  when off; TB-C9 columns measurement-only (`telemetry_csv.hpp:64-65`, `:307`).
- **Lock-free mandate** (FX-2): the pacer state stays single-writer on thread P (`main.cpp:7391-7400`); only
  the GPU present submission may ever move off P.
- **Honest reporting / eye-veto** (FX-6): a TB-C9 number is necessary-not-sufficient; the eye is the arbiter;
  a metric win the eye rejects is not shipped (AGENT_PROTOCOL §7.1 anti-reward-seeking).

---

## TB-C9 measurement gate / rig-verification log (seeded; filled first-hand when built)

The fluidity score is `sqrt(placement_RMS² + pacing_stddev²)` (lower = smoother), the two axes reported
separately (`scripts/fg_testbench_fluidity.py`). Every row below is filled from a first-hand TB-C1+FG run
before the corresponding FX rows may close.

| Date | Config | placement RMS (ms) | pacing stddev (ms) | fluidity score (ms) | frz / stall | Eye / TB-C6 | Notes |
|---|---|---|---|---|---|---|---|
| baseline (cited) | escalonado, no fix | ~1.55 | ~4.24 | ~4.51 | — | — | TB-C9 baseline, CITED from [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 (phase oscillating 0.77→0.32→0.75→0.30) — not re-derived here |
| (to fill) | + PACING half (`--pace-hard`) | | | | | | indexes the pacer triad's rig log; measured on TB-C1 deterministic |
| (to fill) | + PLACEMENT half (uniform `t=0.5` ladder) | | | | | | the new code; FX-3 gate |
| (to fill) | + both (the full fix) | | | | | | the combined A/B; FX-6 closes only with eye + TB-C6 agreement |

---

## Cross-links

- **The Tier-2 triad (siblings):** `FG_FLUIDITY_FIX_MASTER_PLAN.md` + `FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES.md`
  (authored by the supervisor — this register is the first leg).
- **PACING half (adopted, not duplicated):** the pacer triad —
  [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) /
  [`FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md) /
  [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md) (`PP-1..PP-7`).
- **The measurement axis (TB-C9):** [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) (the temporal
  SOTA + the fix direction) · [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 (TB-C9) +
  [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md) (`TB-1..TB-6`) ·
  [`../../scripts/fg_testbench_fluidity.py`](../../scripts/fg_testbench_fluidity.py).
- **The displayed-plane crash posture (inherited):** [`FG_OPTION_A_RISK_REGISTER.md`](FG_OPTION_A_RISK_REGISTER.md)
  (`OA-3` give-back, `OA-10` watchdog).
- **The pacing/latency floors + why the metronome is software-on-DWM not VRR:**
  [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md).
- **Protocols:** [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) (the Tier-2 triad + the gate) ·
  [`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md) (BCP 14, verifiable references, the
  honesty buckets).

**Registration.** Per FDP §7 this triad MUST be added to [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md)
and the live [`../ACTION_PLAN.md`](../ACTION_PLAN.md); those shared files are **NOT** edited from this document
(to avoid a collision with parallel work) — the supervisor performs the registration.

---

Made with my soul — the supervisor, for Swately.
