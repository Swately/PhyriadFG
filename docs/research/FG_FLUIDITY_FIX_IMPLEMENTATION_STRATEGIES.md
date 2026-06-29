# FG_FLUIDITY_FIX_IMPLEMENTATION_STRATEGIES — the buildable sibling (the escalonado fix)

> **Diátaxis type:** Planning (how-to / strategy). Single declared type, held throughout — this is the
> concrete edit-site annex, not the why (the MASTER_PLAN) and not the failure catalog (the RISK_REGISTER).
> **Normativity:** BCP 14 (MUST / SHOULD / MAY per
> [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)).
> **Status:** `designed` — **NOT built.** Every `file:line` below was verified first-hand against the working
> tree on the authoring date (2026-06-25), `apps/render_assistant/src/main.cpp` (9 790 lines). The PACING-half
> machinery (the `--pace-*` pacer) **already ships** (`shipping`, per
> [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md)); the TB-C9 fluidity
> metric **already ships** (`shipping` this session, per [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md)
> §2). What is `designed`-not-built is the **fix's PLACEMENT lever** (the realized-N uniform ladder) and the
> **adoption** of both halves under a measured TB-C9 gate. The new code is seeded in the RISK_REGISTER with
> mitigation-as-code.
> **Tier:** **T2** (risk-bearing) — it touches the FG present path (the displayed own-window scanout plane) and
> the per-tick interpolation-phase placement: PLAN_TIER_PROTOCOL §1.1 **trigger 1** (crash / hang / freeze) and
> **trigger 5** (a dogma at stake — byte-identical-off + the no-lock-out / freeze-floor invariant). Sibling of
> [`FG_FLUIDITY_FIX_MASTER_PLAN.md`](FG_FLUIDITY_FIX_MASTER_PLAN.md) +
> [`FG_FLUIDITY_FIX_RISK_REGISTER.md`](FG_FLUIDITY_FIX_RISK_REGISTER.md) (the other two authored in parallel;
> the supervisor wires the cross-links into the shared registers).
> **Build order is GATED by the RISK register (PLAN_TIER_PROTOCOL §3):** no commit while any `FF-*` risk is
> `open`. Each edit site below cites the risk **ID** it exists to mitigate (traceability).
> **Evidence single-sources (this annex CITES, never re-derives — FDP §4.6):**
> - [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) §2 TB-C9 — the **measured baseline** the fix
>   must beat: synthesized-placement RMS **~1.55 ms** + pacing stddev **~4.24 ms** on a TB-C1+FG run, the phase
>   oscillating **0.77→0.32→0.75→0.30** (the escalonado in the numbers). The fix is gated against THESE figures.
> - [`FG_PRESENT_TARGET_PACER_RISK_REGISTER.md`](FG_PRESENT_TARGET_PACER_RISK_REGISTER.md) Rig log — the
>   PACING-half result already banked: `--pace-hard` cuts own-window present-MASD ~3.1 ms → **~0.95 ms (≈3.3×)**
>   for a clean **+2.2 ms** latency, `frz 0.0`. The fluidity fix ADOPTS this; it does not re-measure the pacer.
> - [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §5 — the fix DIRECTION: a uniform display
>   metronome (PACING) + correct t≈0.5 phase placement (PLACEMENT), `designed` for Phyriad, `measured` of the
>   named production systems (FSR3 two-thread pacer, DLSS-G even pacing, Blackwell Flip Metering).

---

## §1 — The fix in one screen (two halves, one TB-C9 score)

The "escalonado" the operator put as the live FG frontier is now **measurable** (TB-C9), so the fix is the
measure→fix→measure loop, not a guess. TB-C9 decomposes the felt stepping into two orthogonal axes (the field's
master separation, [`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md) §1):

- **PACING** — the display-interval uniformity (`pacing_stddev_ms`). The lever is the **uniform display
  metronome**: present each frame at a uniform interval target. This machinery **already ships** as the
  `--pace-*` pacer triad — this annex **indexes** it (§2), it does NOT rebuild it (FDP §4.6).
- **PLACEMENT** — the temporal-placement error (`placement_rms_ms`, the Animation-Error analogue). The lever is
  making the displayed interpolation phase `t_use` a **clean uniform ladder** (t≈0.5 for 2×) instead of the
  measured oscillation. This is the `designed`-not-built half (§3), built at the `t_use` / content-clock /
  sc-select sites.

The single TB-C9 fluidity score is their Euclidean combine, `sqrt(placement_rms² + pacing_stddev²)` (lower =
smoother; `scripts/fg_testbench_fluidity.py:255-268`). The two components are reported separately and never
silently collapsed; **each strategy below is gated by a measured drop in its own component**, the analogue of
the C8 quality gate for the temporal axis.

**Discipline (every step, no exception):** default-off · flag-gated · byte-identical-off · cites its `FF-*`
risk ID · no game cap · the operator's eye + the TB-C6 photon camera are the final arbiter (the eye-veto).

---

## §2 — The PACING half: INDEX the shipping pacer (do not duplicate)

The uniform display metronome is the `FG_PRESENT_TARGET_PACER` triad, **already built and rig-verified**
(`shipping`). The fluidity fix's PACING half is **adoption under the TB-C9 gate**, not new pacer code. The
edit sites are indexed here so the placement work and the validation know exactly where the metronome lives;
the canonical contract is the pacer triad, linked above.

| Pacer element | `file:line` (verified 2026-06-25) | Role for the fluidity fix |
|---|---|---|
| `--pace-present` flag (soft slew) | decl `main.cpp:202`; parse `main.cpp:1218` | banks the metronomic grid (drift-corrects the AVERAGE phase) — the baseline below `--pace-hard` |
| the soft slew loop | `main.cpp:8451-8478` (the anchor slew `tick_t0 += pp_slew_gain·phase_err`, `:8475`) | KEEPS the fixed `refresh_hz` grid; composes under the hard pin |
| `--pace-hard` flag (the per-frame pin) | decl `main.cpp:206`; parse `main.cpp:1222` | **the PACING lever** — pins each present to `ph_tgt + budget`; this is what drops `pacing_stddev_ms` |
| the pin (present chokepoint) | inside `bridge_present()` `main.cpp:7415-7443` (lambda at `:7406`); tick-boundary publish `ph_tgt = tgt` at `main.cpp:8480-8490` | the bounded sleep-then-spin before `Present`; HARD-capped < one vblank (`FF-2`) |
| `--pace-vblank` flag (the phase-lock) | decl `main.cpp:208`; parse `main.cpp:1224`; the DWM query + anchor slew `main.cpp:8502-8536` | **the PACING refinement** — aligns the grid anchor to the true vblank phase (`DwmGetCompositionTimingInfo`); the path to an LSFG-class metronome |
| the grid / tick scheduler | `tick_period_ms` `main.cpp:8311`; `tick_t0`/`tick_k` `main.cpp:8374`; `tgt` compute `main.cpp:8434-8448`; `paced_wait_P` `main.cpp:7456`, called `main.cpp:8450` | the metronome the pin rides; not modified |
| own-window gate | `cfg.present_own_window` (`main.cpp:7337`, `:7415`, `:8480`) | the metronome paces only the displayed Independent-Flip plane — the fluidity fix runs there (`FF-2`/`FF-7`) |
| the `ph:` DIAG telemetry | `main.cpp:9425-9431` | the held/overshoot counters surfaced live (the freeze-floor evidence for `FF-2`) |

**What fluidity ADDS to the pacer — exactly one optional thing.** TB-C9's pacing axis would honor the SOTA's
binding §3 rule (*"pacing MUST be measured on display-time, NOT present-time"*) only if the CSV carried a real
display-flip interval; today it falls back to `MsBetweenPresents` and FLAGS it as a proxy
(`scripts/fg_testbench_fluidity.py:165-177`). The pacer's `--pace-vblank` path already queries the DWM
`qpcVBlank`/`qpcRefreshPeriod` (`main.cpp:8502-8536`) — a display-flip phase reference. **Strategy FL-Padd
(OPTIONAL, deferred, measured-need-gated):** when `--pace-vblank` is active, derive the per-present interval
from successive `qpcVBlank`-snapped targets and write it into a `MsBetweenDisplayChange` CSV column the TB-C9
analyser already PREFERS (`fg_testbench_fluidity.py:170-174`). This is measurement-only, byte-identical-off
(the `--csv` gate + a default-NA column), and cites `FF-5`. It is the *software* improvement of the gate; the
**TB-C6 high-speed camera remains the photon truth** (the camera is the real display-time, the column only
narrows the proxy). Do NOT build it ahead of a measured need (D-12) — the present-time proxy is already a
closer analogue here than for DLSS/FSR because we ARE the pacer (no game-Present→pacer indirection,
`fg_testbench_fluidity.py:36-44`).

---

## §3 — The PLACEMENT half: make `t_use` a clean uniform ladder

### 3.1 — The placement sites (where the displayed phase is born)

| Placement element | `file:line` (verified 2026-06-25) | What it does |
|---|---|---|
| `--sync-clock` (the content-clock) | decl `main.cpp:678` (default **ON**); parse `main.cpp:1465` / `--no-sync-clock` `:1469` | the free-running NCO that drives the phase |
| the content-clock NCO | decl `main.cpp:8353`; increment `content_clock += tick_period_ms / T_robust_ms` `main.cpp:8667` | advances UNIFORMLY per tick (source-frame units) |
| `--sc-select` (content-clock-driven SELECTION) | decl `main.cpp:210` (default **ON**); parse `main.cpp:1226` / `--no-sc-select` `:1227` | selection + phase read the SAME clock → full 0→1 sweep per pair |
| the selection scan | `main.cpp:8773-8798` (the `cand_c ≥ content_clock` branch, `:8788`) | picks the pair `content_clock` lies within |
| the sync-clock phase override | `main.cpp:8872-8885` (`phase = (content_clock − phase0_c)/span`, `:8874`) | the SOLE phase-source swap when armed |
| the wall-time fallback phase | `t_display = now_ms()−D` `main.cpp:8749`; `phase_global` `main.cpp:8853` | the jittery edge-snapping path (when sync-clock off) |
| `t_use` initial + the monotone-key guard | `t_use = phase_global` `main.cpp:8989`; backwards clamp `main.cpp:8985-8997`; `cand_k` `:8983` | `t_use` is the FINAL presented phase; the guard forbids a backward content step |
| **the even-grid ladder (`--phase-norm`)** | decl `main.cpp:238` (default **OFF**); parse `main.cpp:1176`; the body `main.cpp:9028-9038` (`N` source `:9030-9031`; `te = (pe_j+0.5)/N` `:9033`; `t_use = te` `:9035`) | **the existing intra-pair uniform-ladder lever** — the seed of the fix |
| the realized-multiplier controller | `main.cpp:9013-9027` (`s2_N_over = s2_mult·span`, `:9025`) | the MEASURED sustainable count (the correct N source) |
| `--cphase` (velocity-continuity reshape) | `main.cpp:9048-9078` | an orthogonal seam-pulse lever; out of this fix's scope |
| the TB-C9 write (generated path) | `main.cpp:9238-9239` (`row.disp_phase = t_use`; `row.disp_src = (pair_c−span)+t_use·span`), inside `if(tcsv.active())` `:9210` | the placement metric reads THIS |
| the TB-C9 write (real-fast-path) | `main.cpp:8945` (`disp_phase=1.0; disp_src=pair_c`), inside `if(tcsv.active())` `:8934` | the rfp tick's placement row |

### 3.2 — Root cause of the oscillation (verified in the code, not assumed)

The content-clock advances uniformly (`main.cpp:8667`), so the *raw* phase is uniform; the escalonado comes
from the **intra-pair tick count not matching the prediction**. The `--phase-norm` even grid
(`main.cpp:9028-9038`) already places tick `j` of a pair on `(j+0.5)/N`, BUT its `N` is the *passive
prediction* `span·T_robust/tick_period` (`main.cpp:9031`); when the realized ticks-per-pair differs from
`N_pred`, the boundary frame over/undershoots — the residual stepping the `--phase-norm` self-comment names
("the boundary step still over/undershoots when N_pred mismatches", `main.cpp:1176`). That mismatch IS the
`0.77→0.32→0.75→0.30` oscillation TB-C9 measures.

### 3.3 — The fix: the even grid driven by the REALIZED count (`FF-1` / `FF-3` / `FF-4`)

**Strategy FL2 (the PLACEMENT lever).** Add a default-off flag — **`--fluid-ladder`**, in the `parse_extra`
block (the **C1061 lesson**: flags live in `parse_extra`, NOT the main parse chain — the `--phase-norm` /
`--pace-*` precedent, `main.cpp:1176`, `:1218-1224`) with a `cfg.fluid_ladder=false` field beside
`phase_norm` (`main.cpp:238`). When set, it reuses the existing even-grid body (`main.cpp:9028-9038`) but
selects `N` from the **realized** count rather than the passive prediction:

- the realized-multiplier controller's `s2_N_over` (`main.cpp:9025`) is already the MEASURED sustainable
  count; reuse it as the `N` source when available (the controller path is `main.cpp:9013-9027`), OR
- a thread-P-local EMA of the actual ticks-per-pair (incremented at the existing pair-advance detection
  `pe_pair != pair_c`, `main.cpp:9029`) — pure scalar arithmetic, no allocation (`FF-4`).

The edit is confined to the `N`-selection at `main.cpp:9030-9031` (a guarded branch under
`cfg.fluid_ladder`); `te = (pe_j+0.5)/N` and the `cand_k` re-quantise (`main.cpp:9033-9036`) are reused
verbatim. **`t_use` and `cand_k` MUST pass the existing monotone-key guard** (`main.cpp:8985-8997`, mirrored
at the `--cphase` guard `:9077`): never assign a `cand_k` below `last_pres_k` within a pair → never trip the
STAGE-34e backwards guard → no rewind/freeze (`FF-1`). The phase override (`main.cpp:8872-8885`) and the
selection (`main.cpp:8773-8798`) are UNCHANGED — `--sync-clock` + `--sc-select` (both default-ON) still select
the pair; FL2 only makes the intra-pair sweep land on the realized-N even grid. The TB-C9 writes
(`main.cpp:9238-9239`, `:8945`) are unchanged (they read whatever `t_use` is produced) — so the metric scores
the fix without a measurement edit.

**Honest scope (FDP §4.4).** FL2 attacks the intra-pair placement RMS + the boundary over/undershoot. It does
NOT promise the perceptual t=0.5 ideal removes all felt stepping — the SOTA flags the *phase-vs-interval
contribution as underspecified* (`FG_FLUIDITY_PACING_SOTA.md` §5 boundary, §7 gap 4); TB-C9 measures which
half moves, and the operator's eye sets "enough" (no JND number exists, SOTA §7 gap 2).

---

## §4 — Default-off / byte-identical-off discipline (`FF-3`)

Both new toggles are pure gates:

- **PACING** — `--pace-hard` / `--pace-vblank` are the shipping pacer's own gates: flag-off → `ph_tgt` is
  never set → the pin branch in `bridge_present()` is never entered (`main.cpp:7415`, gated `ph_tgt>0.0`) →
  byte-identical (the pacer's PP-3, already `mitigated`).
- **PLACEMENT** — `--fluid-ladder` off → the `N`-source branch (`main.cpp:9030-9031`) keeps the passive
  prediction (i.e. behaves exactly as `--phase-norm` does today, itself default-off) → `t_use` is unchanged →
  byte-identical. No always-on tick-math change; no shared state (`tick_t0`, the content-clock, the `pp_*`
  window) is mutated by the off path.
- The TB-C9 columns are already measurement-only (`disp_phase`/`disp_src` default `−1.0`, written only inside
  `if(tcsv.active())`, `telemetry_csv.hpp:64-65`, `main.cpp:9210`/`:8934`) — the fix adds nothing to that path.

Verify on an unchanged TB-C1 run: the C8 FG-quality regression gate green AND a flags-off `--csv` capture
bit-matches the pre-change output.

---

## §5 — The TB-C9 validation gate (per step — the measure→fix→measure loop)

This is the new capability and the spine of the discipline. Every step is gated by a **measured TB-C9 drop**
on a deterministic TB-C1 source (fixed seed → cell-to-cell variance is the lever, not the input;
`fg_testbench_fluidity.py:62-63`):

1. **Capture** a bounded run (`--csv`, the TB-C2 bounded-run) OFF vs the step's flag ON, same TB-C1 source +
   the held TB-C3 saturation if the regime is under test.
2. **Score** each with `python scripts/fg_testbench_fluidity.py <raw_per_present.csv>` → `placement_rms_ms`,
   `pacing_stddev_ms`, and the combined `fluidity_score_ms` (`fg_testbench_fluidity.py:286-306`).
3. **Gate (BCP 14, MUST):**
   - a **PACING** step (FL1 `--pace-hard`, FL3 `--pace-vblank`) MUST drop `pacing_stddev_ms` vs OFF with no
     regression in `placement_rms_ms`;
   - a **PLACEMENT** step (FL2 `--fluid-ladder`) MUST drop `placement_rms_ms` vs OFF with no regression in
     `pacing_stddev_ms`;
   - the combined step (FL4) MUST drop `fluidity_score_ms` vs the `~sqrt(1.55² + 4.24²)` baseline
     (TB-C9 single-source, §0 header).
4. **Honesty (`FF-5`).** The pacing component is a **present-time proxy** today (no display-flip ts;
   `fg_testbench_fluidity.py:34-44`) — report it as such, and **cross-check the felt result against the TB-C6
   camera** (photon truth) and the operator's eye. A TB-C9 number that drops but reads no smoother to the eye
   is NOT a win (the eye-veto is the final arbiter; SOTA §7 gap 2 — there is no JND figure to cite).
5. **A/A null.** An OFF-vs-OFF run MUST return ~zero delta (the deterministic-source sanity check, the
   analogue of the testbench A/A null).

---

## §6 — Build & validation gates

- build: `vcvars64` + `cmake --build build-ra-msvc2`;
- `lint_hal` clean: default `seq_cst` in `apps/`, no raw `memory_order_*` (no new atomics are needed — the
  pacer state and the ladder state are thread-P-local; if any is added it MUST be justified);
- the **C8 FG-quality regression gate** green (the byte-identical-off proof, §4);
- the **TB-C9 gate** green per §5 (the temporal-axis proof);
- runtime: a bounded TB-C1 capture + `fg_testbench_fluidity.py`, AND — for the crash-class PACING steps — the
  no-lock-out invariant **exercised first-hand** (an injected overshoot / a wedged-spin probe) BEFORE any
  freeze-floor risk flips `open`→`mitigated` (inherited from the pacer's PP-1/PP-2 verification).

---

## §7 — Build order (gated by the RISK register; no commit while any `FF-*` is `open`)

- **FL0 — bank the shipping halves.** The `--pace-*` pacer (`shipping`) and the TB-C9 metric (`shipping`) are
  the baseline; no new work. The fix is measured against the TB-C9 baseline (§0).
- **FL1 — PACING adoption (`--pace-hard`).** Compose the shipping hard pin on the own-window path under TB-C9;
  measure the `pacing_stddev_ms` drop. No new code — index-only (§2). Crash-class → its PP-* risks are already
  `mitigated`/`accepted` in the pacer register; the fluidity register references them via `FF-2`/`FF-6`/`FF-7`.
- **FL2 — PLACEMENT ladder (`--fluid-ladder`, default-off).** The realized-N even grid (§3.3); measure the
  `placement_rms_ms` drop; exercise the monotone-key guard first-hand (`FF-1`) and the byte-identical-off proof
  (`FF-3`/`FF-4`). This is the only genuinely-new code; its `FF-*` rows MUST be `mitigated` before commit.
- **FL3 — PACING phase-lock (`--pace-vblank`).** Compose the shipping vblank phase-lock for a display-phase
  reference; measure the further `pacing_stddev_ms` drop. OPTIONAL FL-Padd (the display-time column, `FF-5`)
  rides here, deferred behind a measured need.
- **FL4 — the combined A/B + the eye-veto.** `--pace-hard --fluid-ladder` (+ `--pace-vblank`); the combined
  `fluidity_score_ms` drops; the TB-C6 camera photon cross-check + the operator's eye decide a possible
  default flip (no default change without the eye-veto).

The unbuilt levers (FL2's `--fluid-ladder`, the optional FL-Padd column) are seeded `open` in
[`FG_FLUIDITY_FIX_RISK_REGISTER.md`](FG_FLUIDITY_FIX_RISK_REGISTER.md) with their mitigation-as-code. No
Tier-2 commit while any risk is `open` (PLAN_TIER_PROTOCOL §3).

---

## §8 — The risk catalog (named here; detailed in the RISK_REGISTER)

| ID | Class | One-line failure mode | Half |
|---|---|---|---|
| **FF-1** | crash / freeze | the placement reshape steps the content-order `cand_k` backwards → rewind/freeze (the STAGE-34e guard). Mitigation: the existing monotone-key guard (`main.cpp:8985-8997`, `:9077`). | PLACEMENT |
| **FF-2** | crash / freeze | the pacing hard wait holds the displayed panel. Mitigation: INHERITS the pacer's PP-1 — total hard wait HARD-CAPPED < one vblank, degrades to the soft path, never blocks. | PACING |
| **FF-3** | dogma | byte-identical-off broken (either half). Mitigation: the flag gates + no off-path state mutation (§4). | both |
| **FF-4** | dogma (D-2) | hot-path allocation / overhead on the per-tick placement path. Mitigation: pure scalar arithmetic, reuse the even-grid body (`main.cpp:9028-9038`). | PLACEMENT |
| **FF-5** | validity (honesty) | the TB-C9 pacing axis is a present-time PROXY, not true display-time (SOTA §3 MUST). Mitigation: report-as-proxy + the TB-C6 camera cross-check; OPTIONAL display-time column. | gate |
| **FF-6** | dogma | no-game-cap: the fix must pace/place OUR present only, never cap/inject the game. Mitigation: INHERITS the pacer's PP-4 (the source-under-test is a controlled instrument, distinct from the real-game no-cap invariant). | both |
| **FF-7** | crash / device-loss | SEH / device-loss on the present path. Mitigation: INHERITS the Option-A OA-3 give-back + the pacer's PP-5 (the wait + present stay inside the SEH/`vk_live`-wrapped loop). | PACING |

---

## §9 — Open questions (returned to the supervisor)

1. **`N`-source for FL2.** Prefer the realized-multiplier controller's `s2_N_over` (`main.cpp:9025`, but it is
   only live under `--target-output-fps>0`) OR a standalone ticks-per-pair EMA (always available)? The latter
   is more general and decoupled from the governor; the former reuses a measured count. The MASTER_PLAN should
   pick one as the canonical mechanism.
2. **Flag granularity.** One `--fluid-ladder` flag, or fold the realized-N source into the existing
   `--phase-norm` as an `--phase-norm-N realized` mode? Reusing `--phase-norm` keeps the flag surface smaller
   but couples two STAGE histories; a fresh flag is cleaner to gate/measure. Leaning fresh flag.
3. **Default-flip threshold.** What measured `fluidity_score_ms` drop + which eye-veto outcome justifies
   flipping any of FL1/FL2/FL3 default-ON (vs staying opt-in like the pacer)? The pacer stayed default-off
   despite a 3.3× win for its +2.2 ms latency cost — the placement lever has no latency cost, so its bar may be
   lower. Needs the operator's eye to set it.
4. **FL-Padd priority.** Is the display-time CSV column worth building before the TB-C6 camera lands, or does
   the camera make it redundant? Currently parked as OPTIONAL/deferred (`FF-5`).
5. **Saturation regime.** Should the TB-C9 gate be run at the held TB-C3 saturation (the real escalonado
   regime) or unsaturated first? The pacer win was robust across scenes; the placement oscillation may be
   worse under saturation (tick-count starvation) — likely measure both, gate on the saturated case.

---

Made with my soul — the supervisor, for Swately.
