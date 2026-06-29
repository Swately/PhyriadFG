# FG_FLUIDITY_FIX_MASTER_PLAN — perfect the escalonado (a uniform display metronome + correct interpolation-phase placement)

> **Diátaxis type:** Planning (master plan / Explanation). Single declared type, held throughout — this is the
> conceptual spine of the fluidity fix, not a how-to and not a risk register.
> **Normativity:** BCP 14 (MUST / SHOULD / MAY per [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
> [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)) — keywords bind only where they appear in capitals.
> **Status:** `designed` — the fix is **not built as a single shipped capability.** Its two halves stand at
> different maturity (the honesty point of this plan, §1): the **pacing** half is largely `shipping` (it is the
> [`FG_PRESENT_TARGET_PACER`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) triad, indexed here, NOT re-authored); the
> **placement** half (the content-clock / interpolation-phase ladder) is `designed`. Calling either the other
> would be a reporting defect (FDP §4.2). Version anchor: `0.1.0-experimental`, snapshot **2026-06-25**.
> **Tier:** **T2 (risk-bearing)** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §1.1 — the fix
> touches the FG **present** (the most sensitive path: own scanout plane on the Option A own-window): trigger 1
> (crash / hang / freeze — a mis-timed metronome wait or a held frame) and trigger 5 (a dogma at stake — the
> no-lock-out / freeze-floor invariant + byte-identical-off). See §4.
> **The Tier-2 triad (mutually linked, PLAN_TIER_PROTOCOL §2.3) — the two annexes this spine governs:**
> - [`FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES.md`](FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES.md) — the
>   concrete per-strategy edit sites of the **placement** half + the TB-C9 gating discipline; `designed`
>   (authored this session). It MUST cite each risk ID at its edit site.
> - [`FG_FLUIDITY_FIX_RISK_REGISTER.md`](FG_FLUIDITY_FIX_RISK_REGISTER.md) — the failure modes of the
>   placement-half present-path changes, each with mitigation AS CODE + first-hand verification; `designed`
>   (authored this session). **Hard gate (§3 of the tier protocol): no `open` risk at commit.** It MUST **defer**
>   the already-treated pacing-half risks to the pacer's register (next bullet), not duplicate them (FDP §4.6).
>
> **Reconcile-not-duplicate (FDP §4.6) — the load-bearing discipline of this plan.** This spine **indexes**,
> and MUST NOT re-author, three existing bodies of work it builds on:
> - [`FG_PRESENT_TARGET_PACER`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) triad (MASTER_PLAN +
>   [`_IMPLEMENTATION_STRATEGIES`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md) +
>   [`_RISK_REGISTER`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md)) — the **PACING half** (the display metronome).
>   It is the single source of truth for the pacer's design, numbers, and risk treatment; this plan motivates it
>   with the now-measurable escalonado and ties it to TB-C9, nothing more.
> - [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) (+ its
>   [`_RISK_REGISTER`](FG_OPTION_A_RISK_REGISTER.md)) — the own-window present path the metronome runs on, and
>   the same-display vs own-window present constraint (§3).
> - the `--async-present` decouple (the render-assistant arc; the
>   [`FG_REALFAST_PATH`](FG_REALFAST_PATH_MASTER_PLAN.md) triad implies it) — the present↔generation-fence
>   decouple the metronome composes above. Indexed, not re-specified here.
>
> **Evidence single-sources (this plan CITES, never re-derives, the numbers — FDP §4.6):**
> - [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 (TB-C9) — the measured escalonado
>   (placement RMS ~1.55 ms, pacing stddev ~4.24 ms, the oscillating phase 0.77→0.32→0.75→0.30).
> - [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) — the field's fix direction (uniform display
>   metronome + correct phase; the display-time measurement rule; the proprietary-LSFG and JND gaps).
> - [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md) — the `--pace-hard`
>   result (own-window present-MASD ~3.1 → ~0.95 ms ≈ 3.3× for +2.2 ms latency, `frz 0.0`) and the LSFG
>   ~0.004 ms `MsBetweenPresents`-MASD target.

---

## §0 — Motivation: the escalonado is now MEASURABLE, so it is now FIXABLE

The "escalonado" (the stepping / staircase felt against LSFG, the operator's named live FG frontier) stopped
being a subjective complaint and became a **measured signal** the moment TB-C9 — the temporal-fluidity axis —
landed. On a deterministic TB-C1 source + FG run, TB-C9 scores a synthesized-placement **RMS ~1.55 ms** and a
display-interval **pacing stddev ~4.24 ms**, and it exposes the mechanism directly: the presented
interpolation phase **oscillates 0.77→0.32→0.75→0.30** instead of holding a clean ladder
([`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2, TB-C9 — the single source for these
`measured` numbers; the columns are `disp_phase`/`disp_src` at
[`apps/render_assistant/src/main.cpp:9238-9239`](../../apps/render_assistant/src/main.cpp) and `:8943-8945`,
defined in [`telemetry_csv.hpp:64-65`](../../apps/render_assistant/src/telemetry_csv.hpp), analysed by
[`scripts/fg_testbench_fluidity.py`](../../scripts/fg_testbench_fluidity.py)).

That measurability is the new capability this plan exploits. It converts "the motion looks steppy" into two
separable, gateable numbers — a **placement** error and a **pacing** stddev — which is exactly the
measure→fix→measure loop the spatial axis already enjoys via the TB-C8 scorer (§2). Without TB-C9 a fluidity
"fix" was unfalsifiable; with it, every change is gated by a measured score drop.

The felt gap vs LSFG is the target this loop closes against. LSFG presents a near-perfect metronome
(~0.004 ms `MsBetweenPresents`-MASD; ~0.026 ms `MsBetweenDisplayChange`-MASD — the references recorded in
[`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md) and
[`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md)); PhyriadFG's primitive cadence **presents a
frame when the warp is ready, not to a uniform display metronome** ([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md)
§5). Both the pacing variance and the placement oscillation above are symptoms of that one structural fact.

---

## §1 — The fix is TWO halves (and they are at DIFFERENT maturity — state it plainly)

The SOTA is unambiguous that fluidity has two orthogonal temporal axes
([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §1): **pacing** (how uniformly frames are shown)
and **placement** (where in time each frame's content lands). The fix has one half per axis. The honest,
load-bearing distinction this plan adds is that the two halves are **not equally far along**, and conflating
them would mis-state what remains to build.

### §1.1 — PACING half = the display metronome (INDEX the pacer triad; do NOT duplicate)

PACING means presenting each frame at a uniform display-interval target — a vblank-phase metronome — so the
display-interval stddev (TB-C9's pacing number, §0) collapses toward LSFG-class. **This half is already
designed AND largely built: it is the [`FG_PRESENT_TARGET_PACER`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md)
triad**, which this plan ADOPTS and INDEXES per FDP §4.6 — it is NOT re-specified here. Its standing, cited
from its own register (the single source of truth):

- the soft drift-corrector `--pace-present` (grid-anchor slew) and the hard per-frame pin `--pace-hard`
  (a bounded sleep-then-spin before `Present()`) are `shipping`; `--pace-hard` cuts own-window present-MASD
  **~3.1 → ~0.95 ms (≈3.3×)** for a cleanly isolated **+2.2 ms** latency, `frz 0.0`
  ([`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md));
- the remaining pacing gap to LSFG-class is the **vblank phase-lock** (HOOK-C, `--pace-vblank` /
  `DwmGetCompositionTimingInfo`, the P3 stage, `designed`-deferred), the lever that references the metronome
  to the true display flip rather than free-running `steady_clock`.

So the fluidity fix does **not** add a new pacer; it motivates the pacer's deferred P3 with the now-measurable
escalonado and gates it on TB-C9. The metronome composes above the `--async-present` present↔fence decouple
(indexed, §header) — the SOTA's prescribed structure ([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §5).

### §1.2 — PLACEMENT half = the interpolation-phase ladder (the net-new `designed` work)

PLACEMENT means the displayed phase `t_use` lands on the correct **uniform ladder** (for a 2× cadence, the
geometric ideal is **t = 0.5**; for N×, the even fractions), instead of oscillating. This is where the §0
signal lives: the phase wandering 0.77→0.32→0.75→0.30 is a placement defect, not a pacing one — a perfectly
metronomic present that lands each frame at the wrong moment still reads as stepping
([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §1(b), §5 honest-boundary). **This half is the
net-new `designed` work this triad governs.**

The mechanism is the content-clock: `t_use` is the intra-pair phase the warp re-renders per panel tick, driven
by the `content_clock` NCO accumulator ([`apps/render_assistant/src/main.cpp:8344`](../../apps/render_assistant/src/main.cpp)
region; the `t_use` ladder is instrumented by `--phaselog`, `:8289-8298`). Selection-vs-phase clock work
already exists as shipping levers (`--sc-select`, `--vblend`, `--mv-subpel` — the cadence package in the
render-assistant arc). The fix is to make the **displayed** `t_use` track the uniform ladder under the
metronome's clock — the IMPLEMENTATION_STRATEGIES annex owns the exact edit sites; this spine only fixes the
contract: *post-fix, `disp_phase` MUST be metronomic on the uniform ladder, measured by TB-C9.*

### §1.3 — Why both, and in this order

The halves are independent (orthogonal axes) but the **pacing half is the platform for the placement half**:
placement error is only cleanly attributable once the present interval is uniform (a jittery interval
contaminates the placement read-back). The order is therefore: bank the shipping pacer (§1.1) → drive the
placement ladder onto it (§1.2) → push the pacer's vblank P3 only if TB-C9 says the residual is pacing, not
placement. The measure→fix→measure loop (§2) decides which half a given residual belongs to — that
attribution is the whole reason TB-C9 separates the two numbers.

---

## §2 — The measure→fix→measure loop (TB-C9 is the temporal gate, the analogue of TB-C8 for the spatial axis)

The new capability is not a new control; it is a **gate**. Just as the TB-C8 objective scorer
([`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2) lets every *spatial* change be accepted or
rejected by a measured fidelity delta against held-out truth, **TB-C9 is the *temporal* gate**: every
fluidity change MUST be accepted or rejected by a measured drop in the TB-C9 fluidity score (placement RMS +
pacing stddev), on the deterministic TB-C1 source under a controlled TB-C3 saturation.

The loop, normatively:

1. **Measure** the baseline TB-C9 score (placement RMS, pacing stddev, the phase ladder) for the current
   build, rate-pinned, on TB-C1 (FG_TESTBENCH §3 build order; rate-pinning matters — a free-running scene is
   the confound the synthetic source exists to remove).
2. **Fix** one half (a pacing lever or a placement lever), default-off, byte-identical-off (§3).
3. **Re-measure.** A change is KEPT only if it drops the score it targets **without regressing the other
   axis or the TB-C8 spatial score** (a placement fix that smears an edge is not a win). The A/A null run
   MUST return ~zero (no spurious signal).
4. **Validate on the photon axis + the eye.** The TB-C6 high-speed camera is the photon-truth cross-check of
   the software TB-C9 number (two independent clocks on the same present stream); the operator's eye is the
   final arbiter (§3 eye-veto). A software score drop that the eye does not confirm is not shipped on.

This loop is what makes "perfect the escalonado" a falsifiable engineering task rather than an aesthetic one.
Until each number is produced first-hand under it, the corresponding fluidity claim stays `designed` /
`unmeasured` (FDP §4.2).

---

## §3 — Honest scope and floors (does / does NOT)

**Does:** drive PhyriadFG's displayed cadence toward a uniform metronome (pacing) at the correct uniform
interpolation phase (placement), on the displayed own-window path, each lever default-off and gated by a
measured TB-C9 drop.

**Does NOT (named floors — FDP §4.4, stated so no later phase re-proposes one as a win):**

- **No game cap, ever.** The fluidity fix times only OUR own present; it never throttles, caps, downscales, or
  injects into the game. This is the FG-arc operator invariant ("make-space, never recommend-cap"), inherited
  from the Option A back-pressure-not-cap posture ([`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md)
  §5) — a provenance invariant, not a numbered CANON dogma.
- **No byte-identical-off violation.** A present-path change MUST be flag-gated and bit-identical with the flag
  absent (the established `--pace-present` / `--pace-hard` / `--csv` precedent). The TB-C9 columns are
  themselves measurement-only and default `-1.0`, written only inside `if(tcsv.active())`
  ([`telemetry_csv.hpp:64-65`](../../apps/render_assistant/src/telemetry_csv.hpp)). The C8/TB-C8 regression
  gate proves the off-path unchanged.
- **No LSFG-exact metronome guaranteed on a same display.** Whether a same-display own-window even exposes a
  stable vblank phase to lock to is a per-config measured outcome — if our window is demoted to Composed Flip
  to keep the game WGC-capturable, the achievable floor may be the composed floor, not the LSFG floor
  ([`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §4; the present-constraint annex of FG_OPTION_A).
  The clean both-floors case is the two-monitor split; surfaced honestly, not as a defect.
- **No input-to-photon latency claim.** `MsAllInputToPhotonLatency` is unmeasurable for external-capture FG
  ([`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §5); pacing/placement (display-clock) is the
  metric, latency parity is neither promised nor the target. Pinning to a uniform phase costs latency on
  purpose (the smoothness↔latency trade, measured, default-off — the pacer's `+2.2 ms`).
- **No JND number invented.** No literature gives a ms-level perceptible-pacing or degrees-of-phase threshold
  ([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §7). **The operator's eye sets "enough"** (the
  eye-veto) — the camera/TB-C6 photon validation + his eye-test are the final arbiter, above the software
  number. Do not cite a JND figure as if one existed.

**Open gap carried, not hidden:** LSFG's internal pacing/placement logic is **proprietary** — its smoother
cadence is observed, its mechanism unverified ([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §7.1).
We target the DLSS-G / FSR3 documented metronome mechanisms (the §5 SOTA analogs), not a reverse-engineered LSFG.

---

## §4 — Tier-2 declaration + the crash-class risk + the two annexes

**Tier: 2 (risk-bearing).** WHY: the fix touches the FG **present** — the most sensitive path. The pacing
half's metronome inserts a bounded wait before `Present()` on the **own-window scanout-owning path**
([`FG_OPTION_A`](FG_OPTION_A_MASTER_PLAN.md)); a mis-timed, overshot, or wedged wait raises the blast radius
from "a little jitter" to **"the displayed frame is held"** — the freeze-floor / no-lock-out class
(PLAN_TIER_PROTOCOL §1.1 trigger 1: crash / hang / freeze; trigger 5: a dogma — freeze-floor + byte-identical-off
— at stake). The placement half's changes ride the same present path. This is the same crash-class the pacer
and Option A triads already carry.

**The risk treatment is SPLIT by the reconcile (FDP §4.6) — this is the honesty point of the Tier-2 set:**

- The **pacing-half** crash-class risks (the freeze floor, overshoot-past-vblank, wedged spin, device-loss on
  the owned present) are **already enumerated, mitigated-as-code, and verified** in
  [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md) (PP-1..PP-7;
  PP-1/2/3/4/7 `mitigated`, PP-5/6 `accepted`, no row `open`) and in
  [`FG_OPTION_A_RISK_REGISTER.md`](FG_OPTION_A_RISK_REGISTER.md) (OA-2/OA-3/OA-10 the yield/watchdog/crash
  give-back floors). This plan's RISK_REGISTER MUST **link** these, not copy them.
- The **placement-half** risks (new content-clock / `t_use`-ladder code on the present path: a phase
  discontinuity that stalls or holds a frame; a clock-domain mix between the metronome and the content-clock;
  a placement change that regresses the TB-C8 spatial score) are the **net-new** failure modes this triad
  must enumerate. They are the subject of [`FG_FLUIDITY_FIX_RISK_REGISTER.md`](FG_FLUIDITY_FIX_RISK_REGISTER.md)
  (`designed`, to be authored), each with mitigation AS CODE + first-hand verification.

**The two annexes (the Tier-2 triad this spine heads):**

| Document | Role | Status |
|---|---|---|
| `FG_FLUIDITY_FIX_MASTER_PLAN.md` (this) | the why · the two halves · the measure→fix→measure loop · the floors · the tier | `designed` |
| [`FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES.md`](FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES.md) | the placement-half edit sites · byte-identical-off discipline · the TB-C9 gate per strategy; cites each risk ID | `designed` (authored this session) |
| [`FG_FLUIDITY_FIX_RISK_REGISTER.md`](FG_FLUIDITY_FIX_RISK_REGISTER.md) | the placement-half present-path failure modes + mitigation-as-code; defers the pacing-half risks to the pacer register | `designed` (authored this session) |

**Hard gate (PLAN_TIER_PROTOCOL §3):** no placement-half code is committed while any risk in its register is
`open`; each MUST be `mitigated` (recorded first-hand verification) or explicitly `accepted` (bounded
residual, operator-recorded).

---

## §5 — Open questions / open measurements (before any number is written `measured`)

1. **Attribution of the §0 residual.** Of the ~1.55 ms placement RMS + ~4.24 ms pacing stddev, how much of the
   *felt* escalonado is placement (the oscillating ladder) vs pacing (interval variance)? The SOTA leaves the
   phase-vs-interval contribution underspecified ([`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md)
   §5 boundary, §7.4); TB-C9's two separate numbers are exactly the instrument to settle it — but it is not yet
   settled. This decides the §1.3 order in practice.
2. **Does pinning `t_use` to the uniform ladder drop the placement RMS without regressing TB-C8?** The placement
   fix is `designed`; the no-spatial-regression conjunction (§2 step 3) is unverified.
3. **Is a stable vblank phase lockable on a same-display Composed-vs-Independent-Flip own-window?** (§3 floor;
   inherited open from [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) §4 /
   PP-6 and [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md) §4). This caps how close the metronome
   can get on one monitor.
4. **Does the software TB-C9 number agree with the TB-C6 photon camera, and with the operator's eye?** The
   software↔photon cross-check and the eye-veto (§2 step 4) are the validity gate; neither is run yet for the
   fix.
5. **Clock-domain safety.** The metronome clock, the content-clock NCO, and the TSC spin primitive are distinct
   domains; the placement fix MUST bridge by duration, never by mixing epochs (the same constraint the pacer
   carries, [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md) §3 Clock domain)
   — to be discharged in the RISK_REGISTER.

---

## §6 — Cross-links

- The measured escalonado + the TB-C9 columns/analyser (the gate this plan runs on) →
  [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 (TB-C9).
- The fix direction (uniform metronome + correct phase), the display-time rule, the proprietary-LSFG / JND
  gaps → [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md).
- The PACING half (indexed, not re-authored) → the [`FG_PRESENT_TARGET_PACER`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md)
  triad ([`_IMPLEMENTATION_STRATEGIES`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md) ·
  [`_RISK_REGISTER`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md)).
- The own-window present path + the same-display present constraint → [`FG_OPTION_A_MASTER_PLAN.md`](FG_OPTION_A_MASTER_PLAN.md)
  (+ [`_RISK_REGISTER`](FG_OPTION_A_RISK_REGISTER.md)); the present↔fence decouple → the `--async-present` /
  [`FG_REALFAST_PATH_MASTER_PLAN.md`](FG_REALFAST_PATH_MASTER_PLAN.md) work.
- The pacing/latency floors + why the metronome is software-on-DWM not VRR →
  [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md).
- The objectives this fix closes (C6 pacing jitter on the panel clock; C11 controlled testbench A/B) →
  [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §2 (the vista is supervisor-wired; linked here
  read-only, not edited).
- Protocols: [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md),
  [`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md).

**Registration:** this triad MUST be added to [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md)
and the live [`../ACTION_PLAN.md`](../ACTION_PLAN.md); those shared files are **not** edited from this document
(to avoid a collision with parallel work) — the supervisor performs the registration.

---

*Made with my soul — the supervisor, for Swately.*
