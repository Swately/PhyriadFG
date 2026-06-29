# FG_PRESENT_TARGET_PACER_MASTER_PLAN — the hard present-target pacer (own-window)

> **Diátaxis type:** Planning (master plan). **Normativity:** BCP 14 (MUST / SHOULD / MAY per
> [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)).
> **Status:** largely `shipping` — **P1 (`--pace-hard`) + P3 (`--pace-vblank` / `--pv-lock-gain`) are BUILT +
> committed** (the per-frame present-target PIN + the DWM-vblank-phase lock; verified first-hand 2026-06-25 at
> `main.cpp:26`/`:206-209`/`:1088`/`:1223`/`:8375-8501`; the RISK_REGISTER's risks are all `mitigated`/`accepted`,
> none `open`). `measured` rig facts: `--pace-present` present-MASD 10.49→4.28 ms / display-flip 9.38→1.79 ms
> (2026-06-23); `--pace-hard` own-window present-MASD ~3.1→~0.95 ms; `--pace-vblank` −39 % present-MASD
> (ACTION_PLAN). The LSFG metronome target is ~0.004 ms present-MASD. **[Reconciled 2026-06-25: this header
> previously read 'designed — NOT built', which was STALE vs the shipping code + the RISK_REGISTER; the registers
> + code are authoritative.]** Every
> code reference is a `file:line` verified first-hand against the tree on the authoring date (2026-06-23);
> every behavioral/architectural claim about the *hard* pacer is `designed`, not observed.
> **Tier:** **T2** (risk-bearing). A hard wait before `Present()` on the panel-owning own-window path trips
> PLAN_TIER_PROTOCOL §1.1 **trigger 1** (crash / hang / freeze) and **trigger 5** (a dogma at stake — the
> no-lock-out / freeze-floor invariant + D-2). It builds directly ON the Option A own-window path, itself
> T2 because that path OWNS the displayed scanout plane. See the RISK_REGISTER's *Why T2*.
> **Triad (mutually linked, PLAN_TIER_PROTOCOL §2.3):**
> [`FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md`](FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES.md)
> · [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md).
> **Hard gate (PLAN_TIER_PROTOCOL §3):** a T2 change MUST NOT be committed while any risk in its register is
> `open`; each MUST be `mitigated` (recorded first-hand verification) or explicitly `accepted` (bounded
> residual, operator-recorded).
> **Evidence single-sources (this plan CITES, never re-derives, the numbers — FDP §4.6):**
> - [`FG_OPTION_A_RISK_REGISTER.md`](FG_OPTION_A_RISK_REGISTER.md) Rig-verification log (the soft-pacer line,
>   `:266-267`) — the soft-pacer result (present-MASD 10.49→4.28 ms, display-flip-MASD 9.38→1.79 ms), the
>   LSFG **~0.004 ms `MsBetweenPresents`-MASD metronome** the hard pacer targets, **and its explicit pointer**
>   "the hard present-target pacer is the next lever (separate triad)". The same ~0.004 ms figure is recorded
>   at the `--pace-present` config comment (`apps/render_assistant/src/main.cpp:187`, "the present MASD … was
>   measured ~7.1ms vs LSFG ~0.004ms").
> - [`FG_LSFG_HEADTOHEAD_MEASURED.md`](FG_LSFG_HEADTOHEAD_MEASURED.md) — the broader LSFG-vs-Phyriad scorecard;
>   its pacing line is the **`MsBetweenDisplayChange` MASD = 0.026 ms** (`:23`, a *display-change* metric, NOT
>   the present-MASD figure above). The hard pacer's stated target is the present-MASD metronome; this doc is
>   cited for the qualitative "LSFG locks, we over-present" gap, not for the 0.004 ms number.

---

## §1 — Why (the confirmed gap)

The soft `--pace-present` drift-corrector (STAGE-121) tightens own-window present-MASD to **4.28 ms** (from
10.49 ms unpaced) and display-flip-MASD to **1.79 ms** (from 9.38 ms) — `measured` 2026-06-23 on HSR
single-GPU, own-window, PresentMon 2.x (the single-source: `FG_OPTION_A_RISK_REGISTER.md` Rig-verification
log, `:266-267`). That is a real tightening, but it is **necessary-not-sufficient**: it does not reach LSFG's
~0.004 ms present-MASD (`MsBetweenPresents`-MASD) metronome — the `measured` design TARGET recorded in the
same Rig log (`FG_OPTION_A_RISK_REGISTER.md:266-267`) and at the `--pace-present` config comment
(`apps/render_assistant/src/main.cpp:187`). (The head-to-head scorecard's pacing line is a *different* metric
— `MsBetweenDisplayChange` MASD = 0.026 ms, `FG_LSFG_HEADTOHEAD_MEASURED.md:23` — not the 0.004 ms figure.)

The mechanism of the residual is verified first-hand in the code. The soft pacer KEEPS the fixed
`refresh_hz` grid (`tgt = tick_t0 + tick_k·tick_period_ms`, `apps/render_assistant/src/main.cpp:7951`) and
**only slews the grid ANCHOR** `tick_t0` by a bounded fraction of the phase error
(`tick_t0 += pp_slew_gain·phase_err`, default `pp_slew_gain=0.05`, `main.cpp:7979`; config `main.cpp:190`).
The slew is a first-order low-pass on the *average* phase: it deliberately filters per-frame jitter and only
chases CONFIRMED sustainable lateness (`fwd_cap = ti_ach − tick_period_ms`, `main.cpp:7975`). It therefore
corrects slow drift but never pins an *individual* frame to a precise instant — and the grid free-runs on
`std::chrono::steady_clock` (`now_ms()`, `main.cpp:1595`) with **no phase reference to the actual display
flip**. Per-tick scheduler / spin / `CopyResource` / `Present` jitter leaks through; that leak is the
~4.28 ms floor.

The hard pacer is a **different control objective**: pin each frame to a precise QPC / vblank-phase target,
rather than track the average. The two compose (slew the anchor for drift; hard-target each frame to
`anchor + k·period`) — they are not interchangeable (`main.cpp:7955-7979` is the slew; this plan adds the
per-frame pin).

## §2 — The contract (what "done" means)

A new **default-off** mode (a flag in the established `--pp-*` family, e.g. `--pace-hard`) that, **on the
displayed own-window path only** (`Style::OwnWindow`, `PresentSurface.cpp:334-343`), presents each frame AT
a computed QPC / vblank-phase target via a bounded sleep-then-spin wait before `Present()`. "Done" is the
conjunction of:

1. **Pacing** — present-MASD `measured` (PresentMon 2.x, HSR own-window) materially below the 4.28 ms soft
   floor, toward LSFG-class. The plan does **not** promise the exact 0.004 ms (see §5).
2. **Freeze-floor invariant holds first-hand** — a missed, overshot, or wedged target NEVER holds the
   displayed panel. The wait is hard-capped below one vblank and the OA-10-class watchdog
   (`main.cpp:8013-8020`) plus the OA-3 crash give-back are independent release floors.
3. **Byte-identical-off** — flag absent → the present path is exactly today's soft/grid path (the
   `--pace-present` / `--pp-*` gating precedent, `main.cpp:7955`). The C8 regression gate proves it.
4. **No game cap** — we time only OUR own-window present; we never throttle, cap, downscale, or inject into
   the game (the FG_OPTION_A §3 back-pressure-not-cap control). This is an FG-arc operator invariant (the
   "make-space, never recommend-cap" rule established across the render-assistant arc), **not** a numbered
   CANON dogma — cited here by that provenance.

## §3 — Architecture

**Where the hard wait sits (CORRECTED — the load-bearing fact the draft got wrong).** The present tick
(thread P) computes `tgt` at the tick boundary (`main.cpp:7937-7953`) and waits to it via `paced_wait_P`
(`main.cpp:6994-7003`). **`paced_wait_P` already spins to `tgt`** (its own `cpu_wait_for_ns(tgt−now)`
finish). So a hard pin placed *at the tick boundary* (right after that call) is a **NO-OP**: by then
`now_ms() ≥ tgt`, the residual is `≤ 0`, the pin never waits — it would `measure` identical to
`--pace-present`. (P1 was initially placed there; verified-first-hand to be the no-op; corrected. Recorded
for honesty, not buried.)

**The present-MASD jitter is not born at the tick boundary** — it is born in the VARIABLE per-tick work `W`
that runs BETWEEN the tick wait and the actual `Present`: the upscale + `CopyResource`/blit + the warp fence
wait (`do_present_P` → `bridge_present_src`, `main.cpp:7081-7100`; the `vkWaitForFences` at `:7098` is the
dominant jittering term) → then the real present `bridge_present` (`main.cpp:6986-6990`,
`ps_account(ra_surface.submit(h))` → `PresentSurface::submit` → `Present(sync_interval,0)`,
`PresentSurface.cpp:512`). The present lands at **`tgt + W`** with `W` jittering — that `W`-jitter IS the
~4.28 ms floor.

The hard pacer therefore **pins the PRESENT, not the tick** — inside `bridge_present()` (the single
chokepoint right before `submit`→`Present`), on the existing thread P (the threading contract
`PresentSurface.hpp:144-147` forbids moving the present to a pacer thread). It holds the present back to a
FIXED phase **`tgt + budget`** where **`budget = EMA(W) + margin`** (an EMA of the realized work-time plus a
small margin), hard-capped below one vblank period. Successive presents are then `tick_period_ms` apart
(present-MASD → 0) whenever `W ≤ budget`. It does NOT replace the soft slew; it composes above it (the slew
supplies the drifted grid anchor `tgt`; the present pins to `tgt + budget`).

**Latency cost (honest, not hidden).** Pinning to `tgt + budget` instead of presenting at `tgt + W` adds
`budget − W` of latency per frame (positive when the frame's work finished early). That is the
smoothness↔latency trade — paid on purpose, default-off, `measured` on the rig, never concealed.

**The target-instant source (the timer reference — §4 is the crux).** The P1 increment pins to `tgt + budget`
in the existing timer domain (the residual `present_target − now_ms()` handed to the TSC-based
`cpu_wait_for_ns` — a DURATION bridge, epoch-safe; see Clock domain below). Higher-fidelity sources, in
increasing order, remain deferred (§6 P3+):
- the existing grid `tgt` re-expressed in the QPC domain (HOOK B: a tighter, CPU-side-deterministic spin,
  but still no display-flip phase reference);
- the DXGI frame-latency waitable object already plumbed (`PresentSurfaceDesc.waitable`,
  `PresentSurface.cpp:330-331, 373-380`, default-off) — a "system-ready-to-accept" edge;
- `DwmGetCompositionTimingInfo` / `IDXGIOutput::WaitForVBlank` for a true vblank phase
  (`qpcVBlank + N·qpcRefreshPeriod`), folded into the slew so the grid anchor is vblank-referenced (HOOK C).
  This is the lever that closes the gap to an LSFG-class metronome — and the SOTA-documented
  `IPresentationManager::SetTargetTime` path (the `submit_at` stub, `PresentSurface.cpp:540-545`;
  `Pacing::PresentationManager`, `PresentSurface.hpp:62-66`, DESIGNED-not-shipped) is the kernel-side form
  of the same idea.

**The sleep-then-spin split.** Reuse the verified `paced_wait_P` shape inside `bridge_present()`: a coarse
timed wait to within the margin (`present_target − ph_spin_ms`), then a short final spin
(`hal::cpu_wait_for_ns`, TPAUSE on Intel / bounded PAUSE on AMD) to `present_target = tgt + budget`. The spin
window stays bounded at the existing ~2 ms (the efficiency mandate forbids widening the busy-wait).

**Clock domain.** The pacer clock today is `steady_clock` (`now_ms()`, `main.cpp:1595`), NOT
`QueryPerformanceCounter`; and the verified spin primitive `hal::cpu_wait_for_ns` spins against the
calibrated TSC (`CpuWait.hpp:127-136`), NOT QPC. P1 therefore bridges by DURATION, never by epoch: the
residual `present_target − now_ms()` is a small duration (clock-agnostic) handed to the TSC spin, so it pins
to `present_target`'s instant without ever mixing the steady-clock and TSC epochs. (The absolute-QPC /
vblank-phase target named in the deferred sources above is the same idea via a third clock; mixing clocks
silently would be a correctness bug — kept as a constraint into the strategies + RISK_REGISTER.)

## §4 — The crux tension (the honest #1 open question)

The **timer-resolution / spin-budget vs panel-hold trade**. A spin tight enough to hit the vblank edge
burns CPU and, if it ever blocks the present thread past a vblank *while displayed*, risks a held frame —
the freeze floor (RISK PP-1). A sleep coarse enough to be unconditionally safe is too loose to reach
LSFG-class MASD. The resolution is not "pick one" but "bound the tight one": the hard cap on total wait
(< one vblank) + the watchdog floor make the tight path safe, and where a safe target cannot be derived the
pacer degrades to the soft slew rather than block on a bad target.

A second, distinct unknown: whether a same-display own-window in **Composed-vs-Independent-Flip**
(`FG_OPTION_A_MASTER_PLAN.md` §4 records the same-display capture-vs-IF conflict) even exposes a *stable*
vblank phase to lock to. If the own-window is demoted to Composed Flip on the same display, the achievable
floor may be the composed floor, not the LSFG floor — a `measured` per-config outcome (RISK PP-6), surfaced
honestly, not a defect.

## §5 — Honest scope (does / does NOT)

**Does:** collapse own-window present-MASD below the soft-pacer floor toward LSFG-class, on the displayed
own-window path, as a default-off mode.

**Does NOT:**
- claim input-to-photon latency — unmeasurable for external-capture FG (`FG_OPTION_A_MASTER_PLAN.md` §5);
- cap, downscale, or inject into the game (§2.4);
- guarantee LSFG's exact 0.004 ms on a same-display own-window (the floor may be Independent-Flip- or
  two-monitor-only — a measured per-config outcome, §4 / PP-6);
- require or support exclusive fullscreen (borderless-only, inherited from the Option A posture).

## §6 — Staged plan

- **Stage P0 — bank the soft pacer.** `--pace-present` already ships and is rig-verified; no new work, the
  baseline the hard pacer is measured against.
- **Stage P1 — the hard target wait (flag default-off).** Derive the QPC/vblank target on a cold path; spin
  to it before `Present()`; measure present-MASD + a freeze/held-frame counter. Default-off byte-identical.
- **Stage P2 — freeze-floor / watchdog hardening (REQUIRED to commit).** Exercise the held-frame + overshoot
  + bad-target paths first-hand; confirm the hard cap self-releases and the watchdog force-hides on a wedged
  spin. The PP-1/PP-2/PP-3/PP-5 rows flip from `open` only here.
- **Stage P3+ — deferred.** Adaptive spin-budget, a vblank-phase Kalman/PLL refinement, the two-monitor
  Independent-Flip target, and the `IPresentationManager::SetTargetTime` (`submit_at`) kernel path — each
  gated by a measured need (D-12), each its own follow-up.

## §7 — Open measurements (before any number is stated `measured`)

- achieved present-MASD + display-flip-MASD at the hard target vs the 4.28 / 1.79 ms soft baseline, **rate-
  pinned** (the Rig-verification log flagged that the soft A/B was HSR-free-running → a scene confound);
- the spin CPU cost (must stay within the existing ~2 ms/present budget);
- the freeze / held-frame counter under a soak (the PP-1 gate);
- whether a stable vblank phase is lockable on a same-display Composed-vs-Independent-Flip own-window.

Until each is run first-hand, the corresponding claim stays `designed` / `unmeasured` (FDP §4.2).

---

Made with my soul — the supervisor, for Swately.
