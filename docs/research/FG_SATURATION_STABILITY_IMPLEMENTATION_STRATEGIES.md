# FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES — the buildable S0–S5

> **Diátaxis type:** How-to / strategy. The **how** sibling of
> [`FG_SATURATION_STABILITY_MASTER_PLAN.md`](FG_SATURATION_STABILITY_MASTER_PLAN.md) (the what)
> and [`FG_SATURATION_STABILITY_RISK_REGISTER.md`](FG_SATURATION_STABILITY_RISK_REGISTER.md)
> (the risks) — same Tier-2 spine (master-plan §0). Implementation-grade, grounded first-hand
> in `apps/render_assistant/src/main.cpp` + the FSR3 MIT source + the targeted SOTA `w9enrtnv2`.
>
> **Status:** `designed`. Every code line/site cited was read first-hand this session. Build
> each strategy default-off byte-identical; flip the default only after its master-plan gate.

## Cross-cutting discipline
Every flag changes NOTHING when off (gate the WHOLE feature behind the flag, not just one
branch — verify with a CSV diff: an off-run vs the current binary). `lint_hal` clean (default
`seq_cst`, no raw `memory_order_*` in `apps/`). New value-flags go in `parse_extra` (dodge
C1061), not the main `else-if` chain. The gate for each: a BF6-combat eye + `--csv` showing
**slip-CoV drops + present-interval variance < ~4 ms**, frz not up, watchdog 0 stalls.

---

## S0 — the variance-aware moving-average pacing target  ·  `T0`  (the keystone; build FIRST, alone)
**Site:** `paced_wait_P` caller at `main.cpp:7831-7838` (computes `tgt = tick_t0 +
tick_k·tick_period_ms`); `paced_wait_P` lambda at `:6931-6940` (coarse `sleep_until(tgt−2ms)`
+ `hal::cpu_wait_for_ns` spin — KEEP).
**Build:**
1. Keep a `SimpleMovingAverage<10>` of the last 10 present-to-present `now_ms()` deltas (we
   already call `now_ms()` per tick) + an online variance.
2. Replace the constant `tgt = tick_t0 + k·tick_period_ms` with
   `target_interval = avg/realized_mult − varianceFactor·stddev − safetyMargin`;
   `tgt = last_present_t + target_interval`. **`realized_mult` is 2 (nominal) until S2 lands;**
   then S2 drives it. **Reset** the window when a delta exceeds ~100 ms (scene-cut/hitch).
3. Defaults tuning-set-A `{safetyMargin 0.75 ms, varianceFactor 0.1}`, runtime-overridable.
**Pitfalls (first-hand, FSR3 `..._Helpers.h:304-360`):** FSR3's `getVariance()` returns
**`sqrt(variance)` = the std-dev** — apply `varianceFactor·stddev` ONCE (do not sqrt twice).
`getAverage()` returns 0 until 10 samples → during warmup fall back to the constant target.
**Flag:** `--pace-variance` (default off → the constant-interval schedule = byte-identical).

## S1 — the DWM compose-clock phase lock (refinement)  ·  `T0`  (DEMOTED; not a gate)
**Site:** read on thread P; slew **ONLY** `tick_t0` (`main.cpp:7831`). Epoch pattern at
`main.cpp:4038-4042` (`QueryPerformanceFrequency` → `qpc_ms` → `dcm` epoch-delta) — reuse
verbatim for `vblank_ms`. `now_ms()` is `steady_clock` (`main.cpp:1566`), NOT raw QPC.
**Build:** `DwmGetCompositionTimingInfo(NULL,&dti)` (hwnd MUST be NULL post-Win8.1) at ~1 Hz;
`epoch_off` once via the 4038-4042 pattern; `next_vblank_ms = vblank_ms +
ceil((now−vblank_ms)/period_ms)·period_ms`; `phase_err = wrap(tgt − nearest_predicted_vblank,
±period/2)`; `tick_t0 −= 0.05·phase_err` per tick; re-seat if `|phase_err| > period_ms`;
fall back to the free-running anchor on non-`S_OK` (composition disabled).
**CRITICAL pitfall:** **MUST NOT** touch the `--sync-clock` content PLL (`main.cpp:7933-7945`)
— it drives source-frame SELECTION; a display-timeline slew there mis-picks the pair (the
STAGE-93 lurch). Keep the two PLLs disjoint (a code-review gate). Never `DwmFlush`.
**Flag:** `--dwm-lock` (default off). Optional honest-late signal for S3:
`DWM_TIMING_INFO.cFramesMissed/Late/Dropped` (valid only after a 2nd call).

## S2 — the fractional output-rate controller (P1)  ·  `T1`  **BUILT `STAGE-119`, default-off**  (full design: workflow `wrkdmmdoi`)
**Control-theory framing (the engineering, NOT a copy):** a 3-stage cascade — (1) a SETPOINT
GOVERNOR (deadband-P + feedforward of a MEASURED ceiling), (2) the existing `content_clock` NCO
(the phase accumulator), (3) an OVER-PRODUCTION GATE (conditional-integration anti-windup).
`--pace-variance` (STEP 0) failed because it was the WRONG primitive — a present-interval pacer
with no emission ceiling pegged to a decoupled present = integral windup → the surplus became
duplicate presents (the measured 395 fps race). STEP 2 is the two anti-windup cures: clamp the
setpoint to the achievable ceiling + conditionally stop emitting when the warp is saturated.
**The OUR-CASE crux:** measure the warp ceiling, don't assume it.

**As built (`--target-output-fps N`, default 0 = OFF = byte-identical):**
- **E1 state** (`main.cpp` ~7841, by `pv_ring`): `s2_pres_ema` (the realized achievable-rate EMA),
  `s2_mult` (held `realized_mult`), `s2_opdrops`.
- **sustain** (the pacer caller, ~7880): a slow EMA (0.05) of the realized present interval = the
  MEASURED achievable rate under saturation.
- **E2 governor** (before the even-grid, ~8336): `base_fps=1000/T_robust_ms`;
  `target_eff = min(target_output_fps, refresh_hz, kSustainFrac·sustain_fps)` (`kSustainFrac=0.93`
  — DELIBERATELY below the ceiling, "a steady 180 beats an erratic 191");
  `realized_mult = clamp(target_eff/base_fps, 1, 8)` with a **0.15 deadband** (no pumping;
  `T_robust` is the integrator → no PI/Kalman).
- **E3 N override** (the `--phase-norm` even-grid, ~8336): force the even-grid ON when `N>0` and
  set `N = realized_mult·span` (the sustainable count, LOWER than the passive
  `span·T_robust/tick_period` under saturation). `te=(pe_j+0.5)/N` verbatim.
- **E4 over-production gate** (the `--fdrop` decision, ~8394): mandatory drop of a tick whose
  `(pair_c,cand_k)` did not advance (the grid slot held) → `do_warp=false` → the async tail
  re-shows the front. The free-actuator clamp `async_inflight<0` is already in `record_this_tick`.
- Auto-enables `--async-present` (its natural home). `--target-output-fps auto` → `refresh_hz`.
  Tuning: `--s2-sustain` (kSustainFrac).

**Measured (BF6 firing range):** in the STABLE 240 fps/18 ms regime STEP 2 is correctly INERT
(`sustain≈refresh` → `realized_mult≈passive`; MASD 3.48→3.79, ~flat) and does NOT race (the
fixed grid + the gate bound the present). **Its target regime — the motion-collapse — needs
sustained motion (a real match) to validate; that awaits the operator's eye/play.**
**Gate metric: MASD + absolute stddev, NOT CoV** (CoV inverts the ranking under a mean shift).
The `++pe_j`-on-emit-only conditional integration + the present-pacer floor are deferred
refinements (the existing clamp-to-1.0 + the gate cover the core).

## S3 — `--async-present` default-ON + drop-on-slip + util-flow-scale (P3)  ·  `T2` → RISK_REGISTER
**S3a — HARDEN FIRST (the gating prerequisite, byte-identical):** wrap the three async
`vkGetFenceStatus` polls (`main.cpp:7218`, `7617`, `7662`) + the `rfp` `!ap` wait (`7688`) in
`vk_live`: `VkResult r=vkGetFenceStatus(...); if(!vk_live(r)) break; if(r==VK_SUCCESS){…}`.
Closes the spin-forever-on-DEVICE_LOST hole (RISK CR2). Build green + `lint_hal`.
**S3b — metered drop-on-slip:** generalize the `--shallow-queue` poll (`:7612-7624`, budget
`sq_budget_us` default 350) so on a MISS it presents the freshest REAL via `rfp_present`
(`:7656`, the DLSS-G "always present the real") instead of re-showing the stale front; add a
present DEADLINE `= tgt + k_slip·tick_period` (`k_slip≈0.5-1.0`); add the "too close to the
last real" guard (track `t_last_real_present_ms`); add the 2 ms hybrid split (predicted-ready
> `kSpinMs(2.0)` → finite-timeout event-wait, `vk_live`-wrapped; < 2 ms → `hal::spin_hint`
poll). **Invariant:** on a drop, leave `async_inflight` on the still-running slot, poll next
tick, **never reset it** (use-after-reset, RISK CR1; the real falls through `rfp_present` which
picks the non-front free slot `:7668`).
**S3c — util-tied flow-scale:** re-select `flow_div` at runtime from `g_gpu_a_util` (the
governor's current consumer) — shrink the slice when the 4090 pegs, retire the quality-shed.
**Flip:** default-ON + a `--no-async-present` hatch, only after the RISK_REGISTER is fully
`mitigated`/`accepted` + the 30 s BF6 crash gate.

## S4 — the make-space cross-adapter present (P5)  ·  `T2` → RISK_REGISTER  (LAST, operator-gated)
**Verdict (`w9enrtnv2`):** the LSFG dual-GPU rule is REAL and the right target — **monitor on
the FG-GPU**, ONE PCIe copy render→FG, no copy-back. Feasible: create the present-side D3D11
device on the secondary adapter (`IDXGIFactory6::EnumAdapterByGpuPreference` → `D3D11CreateDevice`
with `DriverType=UNKNOWN` + the adapter — a non-null adapter with `HARDWARE` returns
`E_INVALIDARG`); DComp composites on that adapter's device; for scan-out the present MUST reach
the GPU driving that monitor (the load-bearing topology — a **cable move** is required, RISK
CR8). Cross-adapter transport = a copy (linear cross-adapter resource, 128-byte pitch / 4-row
align) OR a shared `SHARED_DISPLAYABLE` surface on WDDM 3.0.
**Build:** wire the dead router — add fp16/DP4a to `GpuDescriptor` (`framework/gpu/include/.../
GpuDescriptor.hpp:49`, a cross-process POD change → **operator approval**, RISK CR7),
`measure_transfer_bw` at real gen×lanes, per-rig break-even (never the author's 596 FLOP/byte,
RISK CR9); route flow→1080Ti, OFA→NVOFA engine, convert→iGPU, warp→4090; the present-side
device on the FG-GPU; degrade to all-on-A with a recommend-disable gate when no net win. **The
contract MAY narrow** to "route flow/OFA/convert off the 4090" (keep present on A) if the
cross-adapter present proves not worth the cable-move for an external overlay.

## S5 (later) — the present-side floor
Wire `PresentSurface::submit_at` to `IPresentationManager` (`SetTargetTime` + skip-to-latest +
`SHARED_DISPLAYABLE` independent-flip = −1 compositor frame). Today returns
`ErrorCode::Unavailable`.

**Build order:** S0 → S3a (harden) → S3b/c (flip) → S2 → S1 → S4. S2 and S3 are independent
once S0 lands.
