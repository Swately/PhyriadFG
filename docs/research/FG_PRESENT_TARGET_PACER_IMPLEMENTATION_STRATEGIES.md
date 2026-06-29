# FG_PRESENT_TARGET_PACER_IMPLEMENTATION_STRATEGIES — the buildable sibling

> **Diátaxis type:** Planning (how-to / strategy). **Normativity:** BCP 14 (MUST / SHOULD / MAY per
> [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)).
> **Status:** largely `shipping` — **`--pace-hard` (P1) + `--pace-vblank` / `--pv-lock-gain` (P3) are BUILT +
> committed** (the QPC/vblank target source + the bounded hard wait + the consumer flags EXIST — verified
> first-hand 2026-06-25 at `main.cpp:206-209`/`:1088`/`:1223`/`:8375-8501`). The remaining hardening is tracked
> in the RISK_REGISTER (all risks `mitigated`/`accepted`). **[Reconciled 2026-06-25: previously read 'designed —
> NOT built', STALE vs the shipping code; the code + registers are authoritative.]**
> **Tier:** **T2** sibling of [`FG_PRESENT_TARGET_PACER_MASTER_PLAN.md`](FG_PRESENT_TARGET_PACER_MASTER_PLAN.md)
> + [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md).
> **Build order is GATED by the RISK register (PLAN_TIER_PROTOCOL §3):** no commit while any risk is `open`.
> Each edit site below cites the risk **ID** it exists to mitigate (§2.3 traceability).

---

## §1 — The pacer locus (the existing tick scheduler)

The present tick (thread P) runs the loop at `apps/render_assistant/src/main.cpp:7927`. Within it:
- the grid anchor + counter are `tick_t0` / `tick_k`; the period is `tick_period_ms = 1000/refresh_hz`
  (`main.cpp:7830`);
- the per-tick target `tgt` is computed at `main.cpp:7937-7953` (the `--pace-variance` branch vs the default
  fixed grid + the `>4-period` re-seat);
- the wait to `tgt` is `paced_wait_P(tgt)` (`main.cpp:6994-7003, 7954`) — coarse `sleep_until(tgt−2ms)` then
  `hal::cpu_wait_for_ns` spin-finish; `tgt` is a `steady_clock` instant (`now_ms()`, `main.cpp:1595`);
- the soft slew is `main.cpp:7955-7983` (verified `cfg.pace_present`-gated; slews `tick_t0` by
  `pp_slew_gain·phase_err`, `:7979`);
- the present is then `ra_surface.submit(h)` (`bridge_present`, `main.cpp:6981-6985`; the steady-state sites
  `main.cpp:7718-7727`).

**The exact insertion point (CORRECTED).** The draft said "between `tgt` computation and the `Present` call",
which a first reading placed at the TICK BOUNDARY (right after `paced_wait_P(tgt)`). That is a **NO-OP**:
`paced_wait_P` already spins to `tgt` (`main.cpp:7004-7005`), so the boundary residual is `≤ 0` and the pin
never waits (verified first-hand; it `measured` identical to `--pace-present`). The MASD jitter is born in the
VARIABLE work `W` (upscale + copy/blit + the warp fence wait `main.cpp:7098`) that runs AFTER the boundary, so
the present lands at `tgt + W`. **The pin must sit at the present chokepoint, inside `bridge_present()`
(`main.cpp:6986-6990`) — the single point reached after all of `W`, immediately before
`ps_account(ra_surface.submit(h))`** — reached ONLY when the new flag is set (D-2 / byte-identical-off). It
composes with — does not replace — the soft slew: the slew supplies the drifted anchor `tgt`; the pin holds the
present to `tgt + budget` with `budget = EMA(W) + margin`.

## §2 — Concrete build steps (each cites its mitigating risk ID)

**(a) The flag.** Add a default-off flag in the `parse_extra` block, following the `--pp-*` precedent and
the **C1061 lesson** (the flags live in `parse_extra`, NOT the main parse chain). Its `cfg` field sits beside
`pace_present` / `pp_safety_ms` / `pp_var_factor` / `pp_slew_gain` (verified `main.cpp:187-190`). Default
keeps the wait unreached → byte-identical (mitigates **PP-3**).

**(b) The present-target derivation — pure arithmetic, no allocation.** At the tick boundary, when the flag
is set, publish `ph_tgt = tgt` (the drifted grid target) for the present-side pin (hoisted thread-P-local
state captured by the `bridge_present` lambda — declared ABOVE the lambda so it is in scope). At the present
chokepoint, derive the work-time `W = now_ms() − ph_tgt` and update an EMA `ph_w_ema = 0.9·ph_w_ema + 0.1·W`
(skip hitches `W≥100ms`). The present target is `present_target = ph_tgt + budget`,
`budget = ph_w_ema + margin` (the P1 margin reuses `ph_spin_ms`; a dedicated `--ph-margin` is a clean future
split). All pure arithmetic on registers; no allocation, nothing queried-with-allocation on the tick
(D-2; mitigates **PP-2**). The deferred vblank-phase source (`DwmGetCompositionTimingInfo` / the DXGI waitable
`PresentSurface.cpp:373-380` / `IDXGIOutput::WaitForVBlank`) would be set up at create/cold time, never on the
steady tick — kept for P3+.

**(c) The bounded sleep-then-spin wait — INSIDE `bridge_present()`.** Reuse the `paced_wait_P` shape (coarse
`sleep_until(present_target − ph_spin_ms)` + `hal::cpu_wait_for_ns` spin-finish, `main.cpp:6997-7000`) to
`present_target` in the timer domain. The bridge is by DURATION (`present_target − now_ms()` handed to the
TSC-based `cpu_wait_for_ns`, `CpuWait.hpp:127-136`), not an absolute-QPC epoch — strictly epoch-safe. A
**HARD cap** keeps the total wait strictly below one vblank period: `budget` is clamped to
`< tick_period_ms` and a residual `rem > tick_period_ms` (an absurd/un-derivable target) degrades instead of
waiting — so the spin can never block the present thread past a vblank while displayed (mitigates **PP-1**).
Allocation-free (mitigates **PP-2**). The busy-wait stays bounded at `ph_spin_ms` (efficiency mandate).

**(d) The producer-stall early-out.** When the producer / keyed-mutex path is already stalled the pacer adds
NO wait. Compose with the verified own-window early-returns: the foreground-yield early-return — the
OA-2/OA-10 yield block (`PresentSurface.cpp:464-489`) whose no-op return is `if (impl_->yielded.load()) return {};`
(`PresentSurface.cpp:489`); the create-time OwnWindow window-style recipe is the distinct `PresentSurface.cpp:299-303` —
and the keyed-mutex / yielded-plane skip-present behavior (the submit path
degrades by skipping the present, never blocks — `PresentSurface.hpp:142-143`). A bad/unstable target
degrades to the soft slew rather than blocking (mitigates **PP-1 / PP-6**).

**(e) The watchdog floor.** Because the total hard wait is hard-capped strictly below one vblank period
(§2c), a wedged spin is structurally impossible — the wait self-releases in `< tick_period_ms` (~4 ms @240Hz),
far under the STAGE-103 drain-watchdog's 100 ms stall threshold (`telemetry_csv.hpp:101`) and the OA-10
`f_seq`/`wd_last_ms` 2 s window (`main.cpp` window-death guard). The cap IS the primary PP-1/PP-5 mitigation;
the watchdogs are the independent floor for any OTHER stall (device loss, source death). For P1 no separate
pre-wait heartbeat bump is required (the cap makes the spin undetectably short by design); a dedicated
present-pacer heartbeat is only warranted if a future variant relaxes the cap (mitigates **PP-1 / PP-5**).

## §3 — The consumer-side wiring (`apps/render_assistant/src/main.cpp`)

- the tick-boundary side only PUBLISHES `ph_tgt = tgt` in the scheduler block (where `--pace-present` lives),
  guarded by `cfg.pace_hard && cfg.present_own_window`;
- **the wait itself is a guarded branch as the FIRST thing inside `bridge_present()` (`main.cpp:6986-6990`)**,
  before `ps_account(ra_surface.submit(h))` — the chokepoint reached after all the variable per-tick work;
- the Present site is the own-window flip `Present` (`Style::OwnWindow`, `PresentSurface.cpp:334-343`;
  `Present(sync_interval,0)`, `PresentSurface.cpp:512`), reused from the Option A triad — NOT a new present
  path;
- **no new POD / `GpuDescriptor` field** is added (the hard pacer is CPU-side timing state on thread P,
  hoisted ABOVE the `bridge_present` lambda so it is captured by `[&]`), so the operator-approval gate for
  ABI-bearing POD changes does NOT trip.

The mode is own-window-only by design: `Style::OwnWindow` is the displayed Independent-Flip plane that a
hard target actually paces; the composed styles (`DcompCt`/`Dcomp`) present into DWM composition and are
jitter-limited by DWM regardless of a CPU-side target (`MASTER_PLAN` §3 architecture).

## §4 — Pacing-target continuity / fallback

- **Re-seat.** On the existing `>4·period` re-anchor (`main.cpp:7952`, `tick_t0=tn; tick_k=0`) the next tick
  publishes a fresh `ph_tgt = tgt`, so the present target re-derives cleanly; the `ph_w_ema` work EMA also
  self-heals (hitch `W≥100ms` is skipped, so a re-seat's giant `W` does not poison the EMA).
- **Bad-target fallback.** If `present_target − now_ms() ≤ 0` (work overran the budget) or `> tick_period_ms`
  (an absurd target), the pin presents IMMEDIATELY — no wait, `++ph_overshoot` — i.e. degrade to the soft
  grid, NEVER block (the freeze-floor-safe default; mitigates PP-1). The deferred vblank-phase variant's
  no-stable-phase case (Composed Flip — PP-6) shares this degrade.

## §5 — Default-off / byte-identical discipline

Flag default-off → `ph_tgt` is never set (stays `0.0`), so the `ph_tgt>0.0` guard inside `bridge_present()`
is never entered → the hard wait branch is dead and no shared state (`tick_t0`, the `pp_*` window) is mutated
by the new path → the present path is exactly today's soft/grid path → byte-identical (the `--pace-present` /
`--pp-*` precedent, verified gating `main.cpp:7955`). There is no always-on tick-math change (the discarded
HOOK-B re-domaining of the steady spin is NOT used — P1 bridges by duration, leaving the existing
`paced_wait_P` untouched), so PP-3 holds by the gate alone. Verify on an unchanged HSR run (the C8 regression
gate).

## §6 — Build & validation gates

- build: vcvars64 + `cmake --build build-ra-msvc2`;
- `lint_hal` clean: default `seq_cst` in `apps/`, no raw `memory_order_*` — the heartbeat / flag atomics MUST
  be justified;
- the **C8 FG-quality regression gate** green (the byte-identical-off proof);
- runtime: PresentMon 2.x present-MASD on HSR own-window + the no-lock-out invariant **exercised first-hand**
  (an injected overshoot / a wedged-spin probe) BEFORE any freeze-floor risk flips `open`→`mitigated`.

## §7 — Staging order (gated by the RISK register)

`P1` (the hard wait, default-off) → measure present-MASD + the freeze counter → `P2` (freeze-floor /
watchdog hardening, **REQUIRED to commit**) → exercise the held-frame + overshoot + bad-target paths
first-hand → only THEN an eye/measure gate for a possible default flip. The unbuilt author's flags (the
vblank-phase estimator; the spin budget `N`) are seeded `open` in the RISK_REGISTER with their mitigation-
as-code specified. No Tier-2 commit while any risk is `open` (PLAN_TIER_PROTOCOL §3).

---

Made with my soul — the supervisor, for Swately.
