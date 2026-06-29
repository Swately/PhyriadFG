# FG_SATURATION_STABILITY_MASTER_PLAN — latency-STABILITY under GPU saturation, the Tier-2 plan

> **Diátaxis type:** Planning (master-plan). The **what + contract** for making PhyriadFG's
> latency STABLE and the present SMOOTH when the game saturates the GPU. Tier-2 triad —
> paired with [`FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md`](FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md)
> (the **how**) and [`FG_SATURATION_STABILITY_RISK_REGISTER.md`](FG_SATURATION_STABILITY_RISK_REGISTER.md)
> (the crash/concurrency/device-loss risks), the three bound to ONE anti-drift spine (§0).
> SOTA backing: [`FG_SATURATION_STABILITY_INTEGRATION.md`](FG_SATURATION_STABILITY_INTEGRATION.md)
> (the **why**, from workflow `wp0cmcjfd`).
>
> **Status:** `designed`. The STEP-0/1 contracts are SOTA-complete (FSR3 MIT + DWM docs);
> the STEP-2/3/4 implementation detail is being grounded by the targeted SOTA `w9enrtnv2`
> (fractional-controller algorithm, cross-adapter present, DWM↔PLL fold, async-present
> hardening) — the contracts below are stable; their `*_STRATEGIES`/`*_RISK_REGISTER` detail
> finalizes when it lands.
>
> **Plan-tier (PLAN_TIER_PROTOCOL):** **Tier-2** — STEP 3 (flip `--async-present` default) is
> crash/device-loss class and STEP 4 (the make-space router) is concurrency + a cross-process
> POD change. Therefore the RISK_REGISTER is **mandatory**, and **no Tier-2 step is committed
> while any of its risks is `open`**. STEP 0/1/2 are Tier-0/1.
>
> **Verifiable-reference mandate (FDP).** First-hand code claims are `[VS]`. No benchmark is
> minted here; numbers cite the SOTA dossier or the measured `bf6_p1_baseline.csv`.
>
> **Operator framing (governing).** Bench = **BF6 max, RTX 4090 at 99% — the hardest case**
> (solve the worst → the easier follow). The symptom is **VARIANCE** (unstable mouse), not
> the mean. Build the **whole** thing, **well**, gradually; investigate any uncertain detail
> specifically. Philosophy: máximo rendimiento · escalabilidad · facilidad de consumo.

---

## §0 — The binding spine (the anti-drift invariant — all three docs hold this)

1. **The target is latency VARIANCE, not the mean.** The gate for EVERY step:
   the BF6-combat `--csv` shows the **MASD (mean-absolute-successive-difference) + the
   ABSOLUTE present-interval stddev drop toward <~4 ms** (the NVIDIA "Variable Frame Timing"
   perceptual boundary; ~12 ms is clearly degraded), with the **held mean reported alongside**.
   **NOT the CoV** — CoV (`stddev/mean`) INVERTS the ranking under a mean shift (verified
   first-hand: `bf6_p1_baseline` has the WORST present-interval CoV alongside the BEST
   latency, because a steady-but-slightly-slower cadence raises the mean). Also NOT the
   self-referential `slip_ms` (it measures the pacer against its own timer). The mean (~1-frame
   hold) is an unbeatable floor (§6) — we do not chase it.
2. **Every step is a default-OFF, byte-identical-OFF flag** until it clears its gate; only
   then is the default flipped (each flip is itself gated + reversible via a `--no-X` hatch).
3. **NEVER cap the game** (external tool, dogma). Our headroom-equivalent is **make-space**
   (STEP 4), not an L1 base-game cap.
4. **The three-layer model** (the convergent SOTA, §2): CLOCK (moving-average, variance-damped,
   display-locked) · PRESENT (decoupled, non-blocking, drop-on-slip) · COST (fractional output
   controller + bounded slice). We have the primitives; we orchestrate them.
5. **Zero-overhead hot path; measure-don't-assume.** Pure-CPU pacing math touches no GPU path;
   every claimed win is a measured CSV delta, not an assertion.
6. **Tier-2 steps (3, 4) carry their risks in the RISK_REGISTER, each with a
   mitigation-AS-CODE + a first-hand verification; none commits while a risk is `open`.**

---

## §1 — Objective, scope, non-goals

**Objective.** Make PhyriadFG's present interval flat and its added-latency low-variance
under GPU saturation, by orchestrating the built-but-off primitives into the convergent
three-layer architecture every shipping FG uses, plus our unique make-space escape.

**In scope:** the present pacer (variance-aware moving-average + DWM lock), the decoupled
drop-on-slip present, the fractional output-multiplier controller, runtime-adaptive
flow-scale, and the multi-GPU make-space routing of the FG slice onto unsaturated silicon.

**Non-goals (explicitly out):** lowering the inherent ~1-frame interpolation hold; any
in-engine Reflex/Anti-Lag/Frame-Warp hook (we have no engine access); driving monitor VRR;
hardware flip metering; an L1 base-game cap; any queue-PRIORITY/preemption scheme (proven
dead on consumer GPUs — §6). The disocclusion-quality arc (STAGE-116 hard-pick, P2/P4) is a
SEPARATE track, parked until this lands.

---

## §2 — Architecture (target vs current)

**Target (the convergent CPU-side subsystem ported onto our DComp overlay):**
ONE present thread (we hold this) paces against a **display-locked, moving-average,
variance-damped clock**, presents **non-blocking**, **drops the late interpolated frame**
(always presents the real), with the per-tick cost **bounded** and a **fractional output
controller** holding a target rate — and, our unique lever, the heavy slice routed onto
**silicon the game is not using** so the warp never balloons on the pegged GPU.

**Current `[VS]` (the inversion):** the present pacer is a free-running `refresh_hz` NCO at a
constant interval (`paced_wait_P`, `main.cpp:6931`; 0 DWM hits); the present **blocks
synchronously** on the warp fence (`vkWaitForFences(fBridge,UINT64_MAX)`,
`main.cpp:7003/7152/7510`) so it inherits the warp's variance verbatim (CoV 0.65); there is
**no fractional controller** (0 symbols); `--flow-scale` is init-only; the saturation
response **sheds quality** (governor/deficit-tier — the SOTA-wrong move); the make-space
router is dead (`break_even_decide` one caller; `offload=false` hardcoded; `GpuDescriptor`
lacks fp16/DP4a). The decouple+drop **is built** (`--async-present`, STAGE-102) but OFF. The
spin-finish (`hal::cpu_wait_for_ns`) **is banked**.

---

## §3 — The phased contract (STEP 0–4)

Each step states its **design contract**, **tier**, **dependency**, and **gate**. The
buildable code-site strategies are in the `*_STRATEGIES` doc; the Tier-2 risks in the
`*_RISK_REGISTER`.

### STEP 0 — D1: the variance-aware moving-average pacing target  ·  `T0` · perf+quality
**Contract.** In the present pacer's caller, replace the constant target interval with the
FSR3 form: `target = movingAvg(last~10 present deltas) − varianceFactor·sqrt(var) −
safetyMargin`, with a **reset** when a delta exceeds ~100 ms (scene-cut/hitch). Tuning-set-A
defaults `{safetyMargin 0.75 ms, varianceFactor 0.1}`, runtime-overridable. **Pure CPU over
telemetry we already log; touches no GPU path; default-off byte-identical.** **Dependency:**
none (stands alone). **Gate:** BF6-combat eye + CSV — slip-CoV drops, interval variance
flattens, frz not up. *(SOTA-complete from the FSR3 MIT source; ready to build now.)*

### STEP 1 — D2: the DWM compose-clock phase lock  ·  `T0` · quality  **(a REFINEMENT, NOT a gate — DEMOTED by `w9enrtnv2`)**
**Contract.** Read `DwmGetCompositionTimingInfo(NULL,&dti)` `qpcVBlank`/`qpcRefreshPeriod` at
**~1 Hz** (not per-frame — it is noisy/can report a vblank in the past), establish the
epoch offset between raw QPC and our `steady_clock` `now_ms()` (reuse the verified
`main.cpp:4038-4042` QPC↔ms pattern), predict `next_vblank`, and slew **ONLY the present-pacer
anchor `tick_t0` / `tgt`** (`main.cpp:7831`) by a small fraction of the phase error.
**MUST NOT fold into the `--sync-clock` content PLL** (`main.cpp:7933-7945`) — that PLL drives
source-frame SELECTION (`sc-select`, the warp phase); a display-timeline error there mis-picks
the pair and re-introduces the STAGE-93 lurch. **Keep the two PLLs disjoint.** Keep the
spin-finish; **never `DwmFlush`**; fall back to the free-running anchor when composition is
disabled. **Dependency:** STEP 0 (it refines STEP 0's anchor). **NOT a gate** for STEP 2/3 —
STEP 0's variance-damped target already evens the present instants; DWM removes the residual
timer-vs-compose drift. **Gate:** the metered slip (vs the real compose cadence) stays bounded.

### STEP 2 — D3: the fractional output-multiplier controller (P1)  ·  `T1` (self-contained, no fence/device-loss surface) · perf+quality+ux  **(depends on STEP 0, NOT STEP 1)**
**Contract.** One knob `--target-output-fps N` (default 0 = OFF = byte-identical; the whole
controller gated behind `N>0`, not just the `N` source). A proportional ratio target over
signals already computed: `base_fps = 1000/T_robust_ms` (**reuse the existing PLL estimate at
`main.cpp:7942` — do NOT add a second estimator**); `mult_raw = clamp(target/base, 1.0,
mult_cap≈8)`; a **0.15 deadband** on `realized_mult` (DLSS-style hysteresis — no pumping; the
input is already EMA-damped via `T_robust`, so no PI/Kalman); feed `N_target = realized_mult ·
span` into the existing even-grid `te = (pe_j+0.5)/N` (`main.cpp:8282`, our `content_clock` NCO
is already the fractional accumulator). **MUST NOT cap the game** (output-side only).
**Over-production gate (dogma-safe):** emit a NEW warp iff the accumulator crossed a new grid
slot AND the pair is published (`found`, `main.cpp:8065`) with `te∈[0,1]` — never extrapolate
(that is `--asw`, separate). **Dependency:** STEP 0 (its even present instants make the
even-content-grid honest in TIME). **Gate:** under saturation the present interval stays flat
at a held target; no over-production; byte-identical off.

### STEP 3 — D4: flip `--async-present` default-ON + drop-on-slip + util-tied flow-scale  ·  `T2` → RISK_REGISTER (DEVICE_LOST lead) · perf  **(depends on STEP 0)**
**Prerequisite (NEW crash-class, lands FIRST, byte-identical):** wrap the three async
`vkGetFenceStatus` polls (`main.cpp:7218/7617/7662`) + the `rfp` wait (`7688`) in `vk_live` —
today a lost device returns `DEVICE_LOST` (`!=VK_SUCCESS`) → treated as "not ready" → the
present loop **spins forever, never exits** (the watchdog covers a dead SOURCE, not a lost
DEVICE). `vk_live` is identity on non-DEVICE_LOST → byte-identical when no loss.
**Contract.** Then replace the synchronous `vkWaitForFences(fBridge,UINT64_MAX)`
(`7003/7152/7510`) with the non-blocking poll + **metered drop-on-slip**: generalize the
`--shallow-queue` block (`7612`) so on a budget MISS it presents the freshest REAL via
`rfp_present` (the DLSS-G "always present the real") instead of re-showing the stale front;
add the present-instant deadline + the "too close to the last real" guard + the 2 ms
spin-vs-event-wait split; on a drop **leave `async_inflight` on the still-running slot, poll
next tick, never reset it** (the use-after-reset invariant); tie `--flow-scale` to runtime
`g_gpu_a_util` (shrink the slice, retire the governor's quality-shed). **Dependency:** STEP 0
(the drop judges "late" against STEP 0's metered target; STEP 1's DWM signal is a later
upgrade, not a prerequisite). **Gate:** 30 s BF6-combat crash-clean + validation-layer clean +
byte-identical-off (`--no-async-present`) + the variance gate; **every risk `mitigated`/
`accepted` before commit.** *(`w9enrtnv2` + the existing DEVICE_LOST/REAL_FAST_PATH registers
ground the hardening.)*

### STEP 4 — D5: the make-space router (P5, the unique escape)  ·  `T2` → RISK_REGISTER · scalability  **(depends on STEP 1 + an operator-gated POD change)**
**Contract.** Wire the dead `break_even` router: add the fp16/DP4a field to `GpuDescriptor`
(**a cross-process POD layout change — MUST get operator approval**), `measure_transfer_bw`
at real gen×lanes, per-rig break-even (**never ship the author's ~596 FLOP/byte**), gated on
render-GPU-saturated AND secondary-clears-transfer+compute. Route flow→1080Ti, OFA→the NVOFA
engine off the 4090 compute cores, convert→iGPU, warp→4090; extend `--upload-xfer` toward a
true three-queue split so the present stops contending on A.q; degrade gracefully to all-on-A
with a net-benefit/recommend-disable gate. **Dependency:** STEP 1 + operator approval of the
POD change. **Gate:** measured per-rig net win under saturation (warp slice shrinks, present
paces flatter) + crash/device-loss-across-adapters clean; all risks closed before commit.
*(`w9enrtnv2` grounds the cross-adapter present-on-secondary feasibility + the break-even
measurement — the most uncertain detail; the contract may narrow to "route flow/OFA/convert
off the 4090" if cross-adapter present proves infeasible for an external overlay.)*

**Dependency spine (REVISED by `w9enrtnv2`):** **STEP 0 is the keystone** (its variance-damped
target is what evens the present instants) ─► { STEP 2 (controller), STEP 3 (async/drop) };
**STEP 1 (DWM) is DEMOTED to a present-pacer refinement** (not a gate); STEP 4 (make-space)
last. **Recommended BUILD ORDER:** STEP 0 → [STEP 3 vk_live hardening] → STEP 3 (default-flip)
→ STEP 2 → STEP 1 (refinement) → STEP 4. (STEP 2 and STEP 3 are independent once STEP 0 lands
and MAY be sequenced by risk appetite — STEP 2 is the lower-risk felt win, STEP 3 the bigger
variance fix.)

---

## §4 — Dogma compliance (DOGMA_SPECIFICATIONS checklist)

- **Efficiency mandate / zero-overhead-off:** every step default-off byte-identical; the
  hot-path pacing math is pure CPU; the win is a measured CSV delta (§0.1, §0.5). ✓
- **No-cap-the-game (external dogma):** STEP 2/3/4 are all output-side or make-space — none
  caps the game (§0.3). ✓
- **Crash/concurrency/data-loss (D-Tier-2):** STEP 3/4 carry the RISK_REGISTER, mitigation-
  as-code + first-hand verify, no `open` risk at commit (§0.6). ✓
- **Honest reporting:** the ~1-frame hold + no-Reflex-offset floors are named, not hidden
  (§6); the make-space contract honestly admits it may narrow if cross-adapter present is
  infeasible (§3 D5). ✓
- **Measure-don't-assume:** the gate is a measured variance drop, not an eye-only claim. ✓

---

## §5 — Plan-tier classification + the RISK_REGISTER

| Step | Tier | Why | Plan artifact |
|---|---|---|---|
| STEP 0 (variance pacer) | T0 | pure CPU, default-off, reversible — **the keystone** | this plan + strategies |
| STEP 1 (DWM lock, **refinement**) | T0 | 1 API @~1Hz, slews the present-pacer anchor only | this plan + strategies |
| STEP 2 (fractional controller) | T1 | CPU scheduling math, **no fence/device-loss surface** | this plan + strategies |
| STEP 3 vk_live hardening (prereq) | T1 | wrap 3 async polls + rfp wait; byte-identical | this plan + strategies |
| **STEP 3 (async-present default)** | **T2** | crash / device-loss / use-after-reset | **+ RISK_REGISTER** |
| **STEP 4 (make-space router)** | **T2** | concurrency + cross-process POD + cross-adapter device-loss | **+ RISK_REGISTER** |

The RISK_REGISTER enumerates, for STEP 3 + STEP 4, every failure mode with its
mitigation-as-code + a first-hand verification + a status; a Tier-2 step **MUST NOT** be
committed while any of its risks is `open` (each must be `mitigated` or explicitly
`accepted`). It reconciles with the existing `DEVICE_LOST_RECOVERY_RISK_REGISTER`,
`REAL_FAST_PATH_RISK_REGISTER`, `SINGLE_GPU_COLLAPSE_RISK_REGISTER` (what is already
discharged vs new for the default-flip).

---

## §6 — Honest floors (named, not chased)

The inherent **~1-source-frame interpolation HOLD** (~12–17 ms) is definitional and identical
to LSFG/DLSS-G/FSR3 — unbeatable. We get **no in-engine Reflex/Anti-Lag/XeLL offset** →
structurally ~1–2 frames worse *mean* than in-engine FG on a saturated single dGPU; **our
lever is VARIANCE, not the mean.** We cannot drive monitor VRR, ride Independent Flip /
hardware Flip Metering, or **preempt a saturating dispatch (queue priority is a proven dead
end — do NOT re-propose).** LSFG's "BF6 240 sin despeinarse" is the capped/headroom (likely
dual-GPU) regime; our honest equivalent is **make-space (STEP 4)** — the one escape its
single-GPU shader cannot pull.

---

## §7 — The triad + backing

- **Why (SOTA):** [`FG_SATURATION_STABILITY_INTEGRATION.md`](FG_SATURATION_STABILITY_INTEGRATION.md)
  (+ `FG_SATURATION_PRIOR_ART`, `FG_CADENCE_LATENCY_PRIOR_ART`, `FG_NVOFA_PRIOR_ART`).
- **How:** [`FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md`](FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md)
  (the S0–Sn buildable strategies; finalized with `w9enrtnv2`).
- **Risks:** [`FG_SATURATION_STABILITY_RISK_REGISTER.md`](FG_SATURATION_STABILITY_RISK_REGISTER.md)
  (Tier-2, STEP 3 + STEP 4; finalized with `w9enrtnv2`).
- **Umbrella:** the saturation cluster (P0/P1/P3/P5) of
  [`PHYRIADFG_PERFECTION_ROADMAP.md`](PHYRIADFG_PERFECTION_ROADMAP.md).
