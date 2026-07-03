# `--predict` — predictive presentation mode: plan, strategy, risk register

Tier-2 planning doc (this re-anchors the presentation clock = the pacing heart; the past cadence-bug
sagas — vibración/bistability — all lived in this exact machinery, so it is risk-bearing by the
plan-tier protocol). One doc, three sections. Status: **measured — R5 FAILED (feature no-go as
premised)**. Implemented + tested 2026-07-03. The implementation is CORRECT and cadence-clean (R1/R2/
R4/R6/R7 all pass), but the CENTRAL PREMISE — a measurable latency drop from 15→7-9 ms (R5) — is
architecturally unsound and did **NOT** materialize (`lat` stays ~13 ms). See the boxed VERDICT below
and the R5 row + the RESULTS matrix. **This doc is NOT a green-to-ship; it documents a measured
negative result.** No commit.

> **⚠ VERDICT (2026-07-03, measured first-hand this session): `--predict` does NOT reduce the `lat`
> metric.** The re-anchor fires correctly (the content_clock leads forward, the ASW extrapolation
> engages with a bounded overshoot), and cadence stays perfectly clean (uniq 241/s, frz 0/s, 0
> retrocesos, no vibration across 120/240/bounce/60s-soak). BUT `lat = present − tcap_of_the_real_pair`
> is **structurally floored by freshage** (~7 ms) + the pipeline — it measures how OLD the underlying
> real frame is, which re-anchoring the *display phase* cannot change. Predict shifts WHICH CONTENT
> MOMENT is displayed (projected forward past the endpoint), not HOW OLD the source frame is. To move
> the metric you would have to project a FULL span+ forward (`predict_e ≈ lead_frames`), which
> saturates the extrapolation at `asw_max` = a constant maximal GUESS that the `lat` number still
> measures against the same old tcap. **The R5 claim in the original plan (below) was wrong**; it is
> kept struck-through for the honesty trail. Per the operator's mandate (STOP rather than add
> compensating machinery), the feature is reported as a **no-go as premised**. It remains available as
> an opt-in *perceptual* forward-projection (it does change displayed content-moment, cheaply and
> cadence-safely), but it is NOT the latency win it was designed to be, and it is NOT promoted.

The mandate for this feature (from the operator): *if cadence regresses and it cannot be fixed within
the phase mapping described here, STOP and report — do NOT add compensating machinery.* Cadence did
NOT regress — the stop-condition that fired is the **R5 latency-win no-go** (the plan explicitly
armed R5 as a no-go trigger: "the expected lat drop 15→7-9ms at 120 source MUST materialize, else the
re-anchor is wrong"). The re-anchor is not *wrong* mechanically — it does exactly what it says — but
the LATENCY PREMISE built on it is unsound, and I did not add machinery to fake a metric drop.

---

## 1. PLAN — what / why

**What.** Add an opt-in `--predict` flag: a *predictive presentation mode* that glues the displayed
content to real time by presenting FORWARD PROJECTIONS of the latest real frame (extrapolation) while
the next real is still in flight, instead of waiting for the next real to arrive before showing an
in-between frame.

**The latency problem it targets (measured this rig).** Today the presentation is *interpolation
between two reals*. An intermediate frame between reals `(n−1, n)` can only be presented AFTER real
`n` has arrived and the pair `(n−1,n)` has been built — the displayed timeline therefore runs ~1
source span behind real time. The lead is baked in as `D` (`present.cpp:1888-1903`,
`D = freshage_ema + span_fresh`) and applied as `t_display = now − D` (`present.cpp:1933`); the
sync-clock positions `content_clock` at `expected = cur_c − lead_frames` (`present.cpp:1916-1920`,
`lead_frames = D/T_robust`). Net measured added latency: **~15 ms at a 120 fps source on this rig**
(the arc's standing figure; re-measured as the T1 baseline, not reused from another session).

**What predict does instead.** Re-anchor the content clock ~1 span FORWARD so the freshest real `n`
is presented at its pair's endpoint (phase `e=0`) as soon as the pair is built, and every subsequent
intermediate tick presents a forward PROJECTION of `n` — extrapolation phase `e ∈ (0,1)` beyond the
endpoint, along a PREDICTED next-span motion vector — until real `n+1` lands and its pair takes over.
The displayed content is then glued to real time; expected added latency drops to **~7–9 ms**
(≈ freshage + one tick) at a 120 fps source. **This latency drop is the whole point of the feature —
if it does not materialize (R5), the re-anchor is wrong and the feature is a no-go.**

**Why it is cheap to build (reuse, do not rebuild).** Every mechanical piece already exists and is
shipping behind other flags:

- **The forward-projection shader path EXISTS** (`--asw`, `wap_warp.comp:1052-1063`). Under `--asw`,
  when the content_clock phase overshoots the held pair (`ph > 1`), the present loop sets
  `extrap_amt = ph − 1` (bounded by `cfg.asw_max`, `present.cpp:2082`) and the shader projects the
  latest real forward: `result.rgb = texture(u_cur_real, uv − extrap·mv/out_size)`
  (`wap_warp.comp:1060-1062`). This is *exactly* a predictive present — today it is a
  DEFICIT-only path (fires only when the flow producer falls behind). Predict mode makes it the NORM
  for intermediate ticks by re-anchoring the clock so `ph > 1` is the steady state, not the
  exception.
- **The predicted MV EXISTS in two forms**, already plumbed in the `--vblend` gen-ring machinery
  (`present.cpp:2386-2416`, `wap_warp.comp:368-382`):
  - **P1 — constant velocity:** the current pair's own MV. This is the vector `--asw` already uses
    at the extrapolation tap (`mv` at `wap_warp.comp:1061`).
  - **P2 — constant acceleration:** `2·mv − mv_prev`, formed in the shader as `mv_target_s`
    (`wap_warp.comp:380-381`) from the PREV pair's MV field uploaded as `u_mv_target` (the vblend
    PREDICT default; the host selects `target_gen = gen_back+1` at `present.cpp:2409-2414`).
- **The clock re-anchor is a ONE-LINE, in-units change** to the `expected` computation
  (`present.cpp:1920`). `--vblend-exact` already demonstrates the pattern: it subtracts `1.0` from
  `expected` to track ~1 pair further BACK (for exact lookahead). Predict does the mirror image —
  ADDS a forward projection amount so the clock runs forward onto/past `cur_c`. The PLL machinery
  (slew, reseat, `T_robust`) is **untouched in its units** — the SAME `content_clock` in the SAME
  source-frame units, only the target position shifts.

**Opt-in only.** Default stays OFF, byte-identical (R7). Promoting to default is a later operator
decision after real-game soak, not part of this change.

---

## 2. STRATEGY — the exact mechanical mapping

All line anchors verified first-hand at the current working tree (2026-07-03). `NS = kGenRing = 3`
(`flow/flow.hpp:22`, `present.cpp:57`).

### S1 — the clock re-anchor (`present.cpp:1914-1929`) — the ONLY units-touching change

The single lever. Today:

```cpp
const double expected = (double)cur_c - lead_frames - (cfg.vblend_exact ? 1.0 : 0.0);
```

Predict adds a FORWARD term `pred_lead` (source-frames). **The as-built code (corrected during Phase
2, see the CORRECTION note) is:**

```cpp
const double pred_lead = cfg.predict ? (double)cfg.predict_e : 0.0;
const double expected = (double)cur_c - lead_frames - (cfg.vblend_exact ? 1.0 : 0.0) + pred_lead;
```

- `+ cfg.predict_e` leads the clock forward by `predict_e` source-frames PAST its normal (interpolating)
  position, so it overshoots the freshest published pair's endpoint into EXTRAPOLATION by a BOUNDED
  amount. This makes `ph > 1` in the phase override (S2), which the ASW extrapolation path consumes.

**CORRECTION (Phase 2, first-hand):** the ORIGINAL plan used `pred_lead = lead_frames + predict_e`
(cancelling the pipeline lead to "land the clock on `cur_c`"). Measured (`[PDBG]` probe): this
**over-leads**. The freshest *published pair* (`pair_c`) already trails `cur_c` by ~1 (the F-thread
build lag = the pipeline lead expressed in pair-index terms). Adding `lead_frames` (~1.5) back on top
pushed `content_clock ≈ cur_c + 0.5`, which is ~1.5 spans PAST the pair endpoint (`pair_c = cur_c−1`)
→ `ph_raw ≈ 2.5` → the extrapolation **saturated at `asw_max` (1.0) on ~90% of ticks** = a constant
maximal guess, not a bounded lead. Dropping the `+ lead_frames` term gives the intended bounded
overshoot: measured `content_clock ≈ pair_c + 0.05-0.16`, `ph_raw ≈ 1.05`, `extrap ≈ 0.05-0.16` — a
small, phase-continuous forward projection. The struck design was `pred_lead = lead_frames +
predict_e`; the shipped-for-test code is `pred_lead = predict_e`.

- ~~`+ lead_frames` **cancels** the pipeline lead so the clock lands on `cur_c` (the latency win).~~
  **STRUCK — this is the double-lead bug + the false premise. See VERDICT: cancelling the lead does
  NOT produce a latency-metric win; it only over-projects. `lat` is floored by freshage regardless.**
- **UNITS INVARIANT:** `pred_lead` is in SOURCE-FRAMES (same as `lead_frames`, `cur_c`,
  `content_clock`). No tick counts, no panel-rate constant. Scale-invariant 240→500Hz by
  construction, exactly as the rest of the loop.
- **Interaction with `--vblend-exact`:** contradictory (one leads back, the other forward) →
  force-off in resolve (S5).
- **PLL untouched:** `kScPhaseGain`, `kScFreqAlpha`, `kScReseatErr`, `T_robust_ms` — all read/write
  identically. Only the `expected` TARGET moves; the slew/reseat that chases it is byte-identical in
  form. The one second-order effect to watch (R4): the reseat threshold `kScReseatErr` (4.0
  src-frames) is compared against `err = expected − content_clock`; a larger constant forward offset
  is a constant shift of BOTH `expected` and the acquired `content_clock` (line 1922 seeds
  `content_clock = expected`), so `err` in steady state is unchanged — the reseat behaves identically.
  Verified by inspection; R4 soak confirms no re-pick flapping.

### S2 — the phase override + extrapolation (`present.cpp:2072-2085`) — already correct, verify only

The sync-clock phase override computes `ph = (content_clock − phase0_c)/span` where
`phase0_c = pair_c − span` (`present.cpp:2073-2074`). With the re-anchored clock, `content_clock ≥
pair_c` in steady state ⇒ `ph ≥ 1` ⇒ the existing ASW block fires:

```cpp
if(cfg.asw && ph>1.0){ extrap_amt = ph-1.0; if(extrap_amt>(double)cfg.asw_max) extrap_amt=(double)cfg.asw_max; }
if(ph>1.0) ph=1.0;   // phase clamps to 1 for the warp's [0,1] math; the overshoot rides extrap_amt
phase_global = ph;
```

- **`extrap_amt = ph − 1` = the extrapolation phase `e`.** At the pair endpoint `content_clock =
  pair_c` ⇒ `ph = 1` ⇒ `extrap_amt = 0` ⇒ **`e = 0` presents `cur` byte-exact** (the asw endpoint
  guarantee — the shader's `pc.extrap > 0.0` gate at `wap_warp.comp:1060` is FALSE, so `result` is
  the phase-1 interpolation, which at `phase_global=1` samples cur; NO extra tap). **This must hold**
  (R5 / R7): the re-anchor must not perturb the endpoint.
- **`--predict` REQUIRES `--asw`** (the extrapolation path) AND `--sync-clock`/`--sc-select` (the
  content_clock that supplies the overshoot). asw/sync_clock/sc_select are all DEFAULT ON, so predict
  composes with the shipping default stack. If any is off, predict is force-off + honest print (S5).
- **`asw_max` bounds the projection.** Default 1.0 = up to one full source-span forward. This is the
  natural predict horizon (project no further than the next real, which then takes over). Predict
  reuses `--asw-max` as-is; no new bound.

### S3 — P1 vs P2: the extrapolation MV (`wap_warp.comp:1060-1063`) — the one shader change

Today the extrapolation tap uses the raw current-pair `mv` (P1, constant velocity):

```glsl
if (pc.extrap > 0.0) {
    const vec2 euv = clamp(uv - pc.extrap * mv / vec2(out_size), vec2(0.0), vec2(1.0));
    result.rgb = texture(u_cur_real, euv).rgb;
}
```

P2 (constant acceleration) reuses `mv_target_s` — ALREADY IN SCOPE at `wap_warp.comp:381` as
`2·mv − mv_ut` (the vblend PREDICT form, `u_mv_target` = the PREV pair's MV uploaded by the host).
The change is a gated MV select at the tap:

```glsl
if (pc.extrap > 0.0) {
    const vec2 mv_ex = (pc.predict_p2 > 0.5) ? mv_target_s : mv;   // P2 = 2*mv - mv_prev (const-accel); P1 = mv (const-vel)
    const vec2 euv = clamp(uv - pc.extrap * mv_ex / vec2(out_size), vec2(0.0), vec2(1.0));
    result.rgb = texture(u_cur_real, euv).rgb;
}
```

- `mv_target_s` is computed unconditionally at line 380-381 today (only READ when `vblend_on > 0.5`
  downstream). When `--predict` P2 is armed, the host MUST ensure `u_mv_target` holds the prev-pair
  MV — i.e. the vblend PREDICT upload path must run. Predict therefore **arms the vblend upload**
  (S4) so `mv_target_s = 2·mv − mv_prev` is valid at the tap. If vblend's target degrades to
  self (`target_gen == f_gen`, at a ring edge / startup) then `mv_ut == mv` ⇒ `mv_target_s == mv` ⇒
  P2 gracefully reduces to P1 (no crash, no discontinuity).
- **New push-constant `predict_p2`** appended LAST after `dhp` (offset 208 today → `predict_p2` at
  offset 212, `pcw` grows 53→54 floats/216B; within the 256B device limit both target GPUs report).
  Pushed `0.f` when `--predict` off OR P1 selected → the shader select yields `mv` → **byte-identical
  to today's ASW** (R7).
- **A/B them on the bounce test (T2/T3), pick the default by data.** P2 is theoretically better on
  smooth curved motion (it tracks acceleration) but WORSE on direction changes (it overshoots harder
  — 2·mv doubles the wrong-direction velocity for one span; R2). The default (`--predict` with no
  sub-flag) is chosen from the `-Bounce` overshoot numbers at equal cadence, not assumed.

### S4 — arming vblend for P2 (`present.cpp:2382-2419`)

The vblend per-pair upload (`wap_upload(prev_slot, rs, f_gen, target_gen)`, `present.cpp:2417`)
fills `u_mv_target` from `hMV_a[target_gen]`. For P2, `target_gen` must be the PREV pair
(`gen_back+1`, the PREDICT branch at `present.cpp:2409-2414`). That branch already runs whenever
`cfg.vblend` is on. So predict does NOT need new upload plumbing — it needs `cfg.vblend` armed AND
NOT `cfg.vblend_exact` (which would upload the NEXT pair). Both are the shipping defaults
(`vblend=true`, `vblend_exact=false`), so on the default stack `u_mv_target` already holds the prev
pair. The predict resolve (S5) makes this explicit: `--predict` with P2 requires `vblend && !vblend_exact`;
if vblend is off (`--no-vblend`), P2 is force-off → falls back to P1 + honest print (P1 needs no
target upload; it uses the raw `mv` already bound).

**Cost note (R6):** the vblend upload already fires every pair on the default stack, so P2 adds ZERO
new host upload. The shader adds one `select` + (when P2) reads `mv_target_s` which is already
computed — no new texture fetch beyond the `u_mv_target` sample that vblend already does at line 379.
Warp cost stays ~3.5–4.0 ms (R6 measures it).

### S5 — `resolve_config` interactions (`cli.cpp:221-241`, the `resolve_config` audit block)

Predict is a presentation-clock mode; several flags are cadence-incompatible with it. All handled in
`resolve_config` with force-off + honest print, and **byte-identical without `--predict`**:

| flag | interaction | resolve action |
|---|---|---|
| `--vblend-exact` | CONTRADICTS predict (leads the clock BACK for exact lookahead; predict leads it FORWARD) | force `vblend_exact=false` + print; keep `vblend=true` (P2 needs the PREDICT upload) |
| `--asw` off (`--no-asw`) | predict's projection IS the asw path | force `predict=false` + print "predict needs --asw (the extrapolation path); disabling predict" |
| `--sync-clock` / `--sc-select` off | predict needs the content_clock overshoot | force `predict=false` + print (same shape) |
| `--rfp` / `--rfp-fresh` | present a real OFF the predicted cadence (a distinct real-present tick) → fights the glued-to-real-time projection | force `real_fast_path=false, rfp_fresh=false` + print |
| `--motion-fallback` | same — off-cadence real presents on fast motion | force `motion_fallback=false` + print |
| `--vblend` off + P2 | P2 needs the prev-pair MV upload | force P2→P1 + print "predict-P2 needs --vblend for the prev-pair MV; falling back to P1 (const-velocity)" |

**Is plain `--asw` subsumed by predict?** NO — `--asw` stays independently meaningful (the
deficit-only extrapolation when predict is OFF). Predict does not force asw off; it REQUIRES asw on
(the projection path). When predict is on, asw's deficit-fill and predict's steady-state projection
are the SAME code (`extrap_amt` from `ph − 1`); predict just makes `ph > 1` the norm rather than the
exception. Documented in the flag help.

### S6 — CLI plumbing (`cli.hpp` + `cli.cpp`)

- `cli.hpp`: three new Config fields near `asw`/`asw_max` (`cli.hpp:175-182`):
  ```cpp
  bool  predict=false;       // --predict: predictive presentation (forward-project the latest real; glue to real time)
  bool  predict_p2=false;    // --predict-p2: constant-ACCELERATION MV (2*mv - mv_prev) vs default P1 (const-velocity)
  double predict_e=0.5;      // --predict-e: steady-state forward-projection phase past cur (source-frames); clamp [0,1]
  ```
  `predict_e` default 0.5 = project half a span ahead in steady state (the display sits between cur
  and the predicted next-real position; the pair takeover at the next real then covers the second
  half). Bounded [0,1] so the projection never exceeds one span (matches `asw_max=1.0`).
- `cli.cpp` parse (`parse_extra`, near `--asw` at `cli.cpp:327-331`): `--predict` sets
  `predict=true` + arms `async_present=true` (the projection presents through the non-blocking bslot
  path, same as asw's co-arm expectation) + honest print. `--predict-p2` sets `predict_p2=true`
  (implies `--predict`). `--predict-e <v>` parses + clamps [0,1].
- `--predict` is DEFAULT OFF (opt-in). The `--no-predict` disabler is unnecessary (default already
  off) but added for symmetry with the flag family convention.

### S7 — OFF-path discipline (byte-identical, R7)

`--predict` absent ⇒ `cfg.predict=false` ⇒ `pred_lead=0` (S1 ternary) ⇒ `expected` unchanged ⇒ the
clock re-anchor is a `+0` no-op. `predict_p2=false` ⇒ `predict_p2` push = `0.f` ⇒ the shader MV
select yields `mv` ⇒ the extrapolation tap is byte-identical to today's ASW. No resolve force-offs
fire (all gated on `cfg.predict`). The new push field is appended LAST so no prior field shifts. Net:
without `--predict`, every presented pixel and every telemetry number is identical to the current
tree (T5 spot-check proves it).

---

## 3. RISK REGISTER

Every risk names its mitigation **as code** and its first-hand verification. Status legend:
`mitigated` (code + verified) / `accepted` (bounded, documented, deliberate) / `open` (MUST NOT ship).
This doc is `designed`; the Status column below records the TARGET each risk must reach (verified in
Phase 2) before commit. **No risk may ship `open`.**

| # | Risk | Mitigation AS CODE | Verification (first-hand, Phase 2) | Status target |
|---|---|---|---|---|
| R1 | **Cadence regression (DOBLES/PLANOS/retrocesos)** — the re-anchored clock re-picks pairs differently → doubled frames, flat/held frames, or backward steps in `disp_src` | The pair SELECTION (`sc-select` loop `present.cpp:1970-1976`), the 0.15 anti-flap hysteresis (`present.cpp:1983-1989`), and the monotonicity/backwards guards (`present.cpp:2219-2241`) are UNTOUCHED — predict only shifts the `expected` clock TARGET (S1). Extrapolated ticks are DISTINCT forward content (extrap_amt in the content-order key `cand_k`, `present.cpp:2215`), never a retroceso | **MEASURED PASS.** `--csv` P1 @120, 20s+, vs same-session OFF baseline: fwd_med 0.497 (baseline 0.500), plano 18, **retro 0**, fwd_max 1.00 (bounded). No new doubles/planos, zero retrocesos | **mitigated** |
| R2 | **Direction-change overshoot** — extrapolation projects along a stale MV; at a reversal the projected content shoots the wrong way | Bounded by `asw_max` (1.0 span, `present.cpp:2093`). P1 (const-velocity) overshoots LESS than P2 (2·mv doubles the pre-reversal velocity). NO new smoothing (measure first, per the mandate) | **MEASURED PASS + confirms P1 as default.** `ball_zoo -Bounce` @120, `--csv`: **P1** fwd_max 1.00 (bounded, 0 retro) — the reversal absorbed as a phase clamp; **P2** fwd_max **4.51** (one large forward jump at each reversal = the 2·mv overshoot, still 0 retro). P1 is the safe default; both self-correct at the next pair (no runaway). NOTE: `disp_src` under-reports the true px excursion (it uses the clamped `t_use`, not `extrap_amt`) → P2's visual snap is larger than 4.5 src-frames | **mitigated** (P1 default) |
| R3 | **Disocclusion trails** — extrapolation has NO B frame to fill the reveals behind the moving object (unlike interpolation, which samples cur for the leading edge); the static grid behind the ball may smear/trail | The extrapolation samples ONLY `u_cur_real` forward along per-pixel MV (`wap_warp.comp:1061-1062`); background pixels carry `mv≈0` so the STATIC grid does not move (the shader comment's guarantee, verified on the zoo's static grid). `bg_snap` (default ON, `cli.hpp:112`) snaps background-side MVs toward the gme model → the reveal band behind the object stays clean; predict inherits it. NO new fill machinery in v1 — the trail (if any) is MEASURED first | **NOT INDEPENDENTLY MEASURED** (the feature is a no-go on R5, so the deeper R3 visual characterization was not pursued). At the shipped bounded `predict_e` the projection is tiny (extrap ~0.05-0.16 span), so any trail is negligible; the 240fps + bounce runs showed no grid-smear symptom in the stats (frz 0, uniq 241). Would need dedicated visual inspection before any promotion | accepted (bounded, inherits bg_snap; not the blocker) |
| R4 | **Selection/PLL destabilization (the vibration class)** — the constant forward clock offset destabilizes the PLL lock or makes the selector flap (the historic vibración/bistability saga) | The forward offset is a CONSTANT shift of both `expected` AND the seeded `content_clock` (S1: line 1922 seeds `content_clock=expected`), so the steady-state `err = expected − content_clock` is UNCHANGED → the slew (`kScPhaseGain·err`) and reseat (`|err|>kScReseatErr`) behave identically; only the absolute clock position shifts. The 0.15 selection hysteresis (`present.cpp:1983`) is untouched. No PLL gain/threshold changes | **MEASURED PASS.** 60s soak, predict ON @120: mean 240.2 fps, **frz sum 0.0** over the whole soak, er=0, 0 errors. The only sub-235 line is a single startup window (line 54); every subsequent window is 240.7-240.9. No reseat storms, no selector flapping, no bistable freshage — no vibración symptom | **mitigated** |
| R5 | **Latency metric dishonesty at e≈0** — the re-anchor must produce a REAL latency drop, not just a smaller reported number | `lat = t_present − tcap_r` (`present.cpp:2456`) measures present-time vs the SELECTED PAIR's capture-time. **MEASURED FAILURE (this is the no-go):** `lat` stays ~13 ms with predict ON (P1 and P2, `predict_e` 0.5 AND 1.0), IDENTICAL to the ~13 ms same-session baseline. Root cause (verified via `[PDBG]`): `tcap_r` is the freshest *real pair's* capture time, structurally ~freshage(7ms)+half-span behind — re-anchoring the display PHASE (interpolation→extrapolation) does not change which real the lat is measured against. Predict shifts the displayed CONTENT MOMENT forward (a prediction), not the SOURCE-FRAME AGE. The endpoint IS byte-exact-cur at e=0 (verified: `extrap=0` when the clock sits at the endpoint), so the metric is HONEST — it honestly reports NO drop, because there is none to report | T1 (measured): `--latency-trace`, predict ON P1, zoo 120, vs same-session baseline (~13-15 ms). Result = **lat ~13.2-13.8 ms = NO DROP**; freshage unchanged (~7-8 ms). The clock DOES lead forward (`extrap` fires, bounded ~0.05-0.16 at `predict_e=0.5`) but the latency METRIC does not move. **FAIL → STOP + report (done); no compensating machinery added** | **FAILED — no-go** |
| R6 | **Warp cost regression** — the extrapolation-as-norm + P2 MV select inflates the per-tick warp | P1 adds ZERO shader work (same `mv` tap as today's ASW). P2 adds one `select` + reads `mv_target_s` (already computed at `wap_warp.comp:381`) — no new texture fetch (the `u_mv_target` sample at line 379 already fires under vblend, default ON). The vblend upload already runs every pair (S4) → no new host upload. Push grows 4B (one float) → negligible | **MEASURED PASS.** `warp` EMA is indistinguishable P1 vs P2 and matches the OFF baseline regime-for-regime. (The ~3.6 ms vs ~0.4 ms spread across runs is the pre-existing async in-order-drain bistability, NOT predict — both the pre-change and post-change builds show both regimes) | **mitigated** |
| R7 | **OFF-path not byte-identical** — a stray predict code path perturbs the default run | `pred_lead=0` when `!cfg.predict` (S1 ternary) → `expected` unchanged; `predict_p2` push = `0.f` when off → shader MV select yields `mv` (today's tap); all resolve force-offs gated on `cfg.predict`; the new push field appended LAST (no field shift). Every predict branch is dead off-path | **MEASURED PASS.** Predict-OFF run (new exe): 0 "predict" mentions; `--csv` cadence fwd_med **0.5000** (baseline 0.4998), retro 3, freshage ~8 ms — deterministic cadence + freshage byte-identical to the pre-change baseline. Caveat: the async warp-EMA regime varies run-to-run on BOTH builds (the documented bistability, orthogonal to predict) | **mitigated** |

**Out-of-scope, noted:** the pair-takeover snap (R2's prediction-error correction at the next real)
is DELIBERATELY left un-smoothed in v1 — the mandate is to measure the snap first, not to pre-empt it
with smoothing machinery. A v2 could add a bounded takeover crossfade IF the measured snap warrants
it; that is a separate change with its own doc. Predict does not touch the F/P protocol, the ingest
path, or the capture layer.

---

### Self-test matrix (acceptance) — RESULTS 2026-07-03 (all runs: VS release build, fresh zoo per run, Normal non-minimized console, 3s settle after zoo launch, kill stale 'RA Ball Zoo' first)

**Same-session baseline (predict OFF, old exe, zoo 120, 240Hz panel):** lat ~12-14 ms, freshage ~7.0 ms,
warp 0.3-0.5 ms (fast regime) / ~3.6 ms (async-backlog regime), cadence fwd_med 0.4998, 3 retro / 46
plano / 5507 fwd, uniq 240/s, frz 0/s.

| T | run | pass criteria | result |
|---|---|---|---|
| T1 | predict ON, P1, zoo 120, `--latency-trace`, 20s+ | `lat` → ~7-9 ms; freshage unchanged; cadence parity | **FAIL on the latency criterion (the no-go).** `lat` = **13.2-13.8 ms = NO DROP** vs the ~13 ms baseline. freshage 8.0-8.2 (unchanged). Cadence CLEAN (uniq 241/s, frz 0/s, 240.8 fps). The re-anchor fires (extrap bounded ~0.05-0.16) but the metric does not move — R5 root cause: lat is freshage-floored, not phase-driven |
| T2 | same with P2 | pick P1 vs P2 by `-Bounce` overshoot at equal cadence | **P1 WINS.** Both clean cadence (uniq 241/s, frz 0/s). Bounce: P1 fwd_max 1.00 (bounded); **P2 fwd_max 4.51** (hard forward jump at each reversal). lat identical (~13 ms) for both. → P1 is the default; P2 not worth its reversal snap |
| T3 | `ball_zoo -Bounce` @120, P1 and P2 | overshoot/snap characterization, bounded + self-correcting | **PASS (characterized).** P1: reversal absorbed as a phase clamp (fwd_max 1.00, 0 retro). P2: one large forward jump per reversal (fwd_max 4.51 in `disp_src`; the true px excursion is larger since `disp_src` uses clamped `t_use`), 0 retro, self-corrects at the next pair. No runaway either way. (240 not separately dumped — the no-go on R5 made deeper characterization moot) |
| T4 | zoo 240 (ratio ~1.0) | graceful degradation, no pathological behavior | **PASS.** cap 241/s, uniq 241/s, frz 0/s, 240.6 fps, lat ~11.3 ms, no crash, no vibración. Extrapolation naturally small when reals arrive back-to-back |
| T5 | predict OFF byte-identical + 60s soak predict ON | OFF byte-identical; soak stable | **PASS.** OFF: 0 "predict" mentions, cadence fwd_med 0.5000 + freshage ~8 ms = baseline (deterministic parity). Soak: 60s, mean 240.2 fps, frz sum 0.0, er=0, 0 errors, no drift, PLL/selector stable |

**Net:** the code is correct and cadence-safe (T2/T3/T4/T5 pass; R1/R2/R4/R6/R7 mitigated), but **T1's
latency criterion FAILS** — the feature's reason to exist. Reported as a measured no-go; not promoted,
not committed.
