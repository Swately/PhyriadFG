# ACTION_PLAN — PhyriadFG (the project's durable objective spine)

> Governed by `F:\Phyriad\protocols\core\ACTION_PLAN_PROTOCOL.md`. This file holds the **objective
> hierarchy + the current position**; the foto mental points INTO it (node ID); the durable memory holds
> the *knowledge*. Parked / superseded nodes are KEPT, never deleted. Detail lives in the cited docs —
> this is the index + the state. **Origin:** the June spine lived in the container's catalog
> (`F:\Phyriad\catalog\cpp\docs\plans\ACTION_PLAN.md`, last position 2026-06-26d) and was never carried
> into this repo at the split; the 0.2.x–0.4.0 arcs (2026-06-28 → 07-03) ran without it — the exact
> objective-drift the protocol exists to prevent (the PRINCIPAL was buried under quality layers). This
> file re-establishes it. Created 2026-09-02.

## ▶ CURRENT POSITION (read this first)

**`P → S4.R5 (FlowSet + FlowRing; the holons as rows; the MV median → a stage-3 row — where the consensus-pass default becomes the operator's declared switch) · **R5 DONE 2026-09-06, G-R5 PASSED** (`records/R5_GATE.md`): the FLOW stage declared (14 rows, `Kind::H`), `FlowSet`/`FlowRing`, the holon leaves and the orchestrator extracted (100 % verbatim), the rows deciding on F, the 3→5 transport row-governed; the A/B vs the pre-R5 binary inside the noise and the pressured run covering the tier ladder. **4.3 DONE 2026-09-06** (`records/S4_3_GATE.md`: 45 tests wired, every gate seen red, two false greens fixed). **R6 DONE 2026-09-06, G-R6 PASSED** (`records/R6_GATE.md`: stage 2 is its own module, the ingest rings + one publish that owns the order, the directory names adopted — `src/` now reads `capture · ingest · flow · clock · generate · present · control · instrument`). **Next: R7 (the operator's — it needs the M1 baselines)** · 4.2, R5's paper half, done 2026-09-06 (`aap/FLOW_ROW_MAP.md`: 13 FLOW rows, **0 new columns** → PROCEED; XR3's residual discharged); R4 done 2026-09-05 (records/R4_GATE.md); A0/M1 is R7's`**
> **The deadzone is closed** (2026-09-04, `records/S2_T6_GATE.md` §6): it was the periodic test
> background, not the shader. On aperiodic content the matcher's spurious sub-pixel MV collapses from
> a median of 0.500 px to 0.034 px and the deadzone population from 138,696 pixels to 67.
> **M4's corpus MUST be aperiodic** — the lattice records describe a matcher failure mode, not the core.

> **Full inventory of what is left:** [`records/BACKLOG_AUDIT.md`](records/BACKLOG_AUDIT.md)
> (2026-09-04) — 182 candidate items, every one checked against the tree by an independent skeptic;
> the critical path, 19 distinct gate debts, the shipped-but-unfinished code, the repo-level facts
> (no CI, no CTest, no upstream ref, tags 26 commits stale), and the 19 documented claims that
> disagreed with the repo — corrected there and here.
· **R0 and R1 are DONE** (2026-09-03). R0: `records/R0_GATE.md` (parity 0 FAIL / 297 launches, M2a = 2 files,
M3 inside spread, column closure = 0 new columns → PROCEED). R1: `records/R1_GATE.md` — the CLOCK is now
`src/clock/phase_clock.{hpp,cpp}` with a CPU test; replay bit-parity on 14,390 live ticks, 0 mismatches;
the measured content step per present is 0.2501 source frames (the 4x multiplication).
· the layer contract is RELEASED (S3 `done` 2026-09-03) and the HOST decision is taken (2026-09-03,
"Sí adelante"): restructure the shipping repo IN PLACE by the six stages of
[`STAGE_CONTRACT.md`](STAGE_CONTRACT.md) (`approved`); the base `apps/minimal_fg` is the blueprint, not the
host; only its seam header + test are adopted (R2). Detail = the v2 Tier-2 triad
[`CONVERGENCE_MASTER_PLAN.md`](CONVERGENCE_MASTER_PLAN.md) + `_IMPLEMENTATION_STRATEGIES.md` +
`_RISK_REGISTER.md`; the rule "no new plan without a new measurement" is in force. **R2 is DONE**
(`records/R2_GATE.md`, G-R2 passed): the seam drives stage 5's barriers behind `--sg-barriers`, deriving
exactly the three hand-written ones; 148 checks; 0 validation lines under a saturated 60 s soak; M3
inside spread. Next, on the operator's word: **S2.T0/T1** (the scorer port + the `--qdump+` sidecars),
which R3's M4 gate REQUIRES, then **R3** (`fg_core.comp`), carrying the column-closure experiment's two
constraints. Flipping `--sg-barriers` to the default is its own decision (a longer soak + the eye).

## P — PRINCIPAL (enduring, carried verbatim from the June spine)

**Perfect PhyriadFG — match then surpass LSFG — as ONE final FG built from a clean minimal core with the
right seam, re-layered only with what earns its place, measured, not assumed.** (Operator, 2026-06-26:
"NOT a --minimal flag on the accreted main.cpp; a clean minimal CORE + the right SEAM, then re-place the
layers that earn it." Re-affirmed 2026-09-02: "busquemos una estructura modular… la generación de frames
exactos que simulen de forma correcta el movimiento".)

## The secondary tree

- **S1 — RESTRUCTURE (the donor made mineable)** · `active → done for E1; E2–E6 parked` ·
  [`RESTRUCTURE_PLAN.md`](RESTRUCTURE_PLAN.md) + `RESTRUCTURE_RISK_REGISTER.md` + `RESTRUCTURE_INVENTORY*.md`.
  - **S1.E1** · thin `main()` — the init-seq as 13 `init_*` functions + ownership structs · **`done`**
    (2026-09-01, G1 passed: builds ×2, `--help` byte-identical, 120 s smoke exit 0, startup diff 0 lines,
    98.37 % verbatim computed).
  - **S1.E2** · extract the P-side lambdas · **`parked` → partly subsumed** (2026-09-03): the
    `wap_warp_present` half is S4.C3a's work (the push assembly becomes `CorePush` + `layer_arm_mask()`);
    the bridge/rfp lambdas stay parked.
  - **S1.E3** · extract the holon family from `run_flow` · **`parked` → re-aimed to S4.2** (the flow
    producers port into the base as `kind = P` SG passes, one at a time).
  - **S1.E4** · one source of truth for flags · **`superseded`** by S4.C0 for layer flags (the registry);
    V-B (the verifier) stays an option for the ~150 host flags.
  - **S1.E7** · core-vs-full equivalence test · **`superseded`** by S2.T6 (the CPU reference warp).
- **S2 — MOTION_TRUTH (exact motion as data)** · **`next`** ·
  [`MOTION_TRUTH_MASTER_PLAN.md`](MOTION_TRUTH_MASTER_PLAN.md) + `_IMPLEMENTATION_STRATEGIES.md` (Tier-1).
  - **S2.T0** scorer port + the `--qdump` present-path fact documented · **`done`** (2026-09-03,
    `records/S2_T0_T1_GATE.md`): `tools/fg_quality_scorer/` builds from the repo against the VENDORED
    pipeline (exit 0, 155,648 B); Mode T on a fresh dump → 6 rows. Two catalog build defects fixed
    (hard-coded `/O2` vs Debug's `/RTC1`; no default build type). **A documented claim was WRONG and is
    corrected**: `--qdump` is NOT inert under the shipping default — `resolve_config` auto-disables
    `--async-present` for the run and says so (verified).
  - **S2.T1** `--qdump+` replay sidecars (MV, SAD, push, gme) · **`done`** (2026-09-03, same record):
    per tick `q*_mv.rg16f` + `q*_sad.rg16f` (129,600 B = 240×135×4) + `q*_push.bin` (232 B, constant) and
    the manifest tokens; the generation is RECORDED at the `wap_upload()` site, never recomputed; the push
    is snapshotted at the submit site. `tools/check_qdump_plus.py` validates a record: sizes, the float16
    MV decode, and the push block's own `t` against the manifest's `t` (equal on every tick — the bytes
    provably belong to that tick). The scorer reads the `+` manifest unchanged.
  - **S2.T1b** fix the dump SAMPLING · **`done`** (2026-09-03, `records/S2_T1B_GATE.md`) — the R3 corpus
    precondition, **lifted**. The stride is replaced by a COVERAGE sampler: dump on a tick whose phase bin
    is the least-covered bin the ladder has actually produced, and whose ring slot is likewise least-covered
    (slot condition dropped after 64 skips so the two cannot deadlock; an 8-tick gap keeps the next
    candidate off the stall we just caused). A stride could never work here — the dump stalls its own tick
    and the clock recovers identically every time, so the phase N ticks later is a function of the stall.
    Measured, two independent runs of 16 triples: **8/8 bins at 2 each** and **4/4 reachable bins at 4
    each**, 3/3 ring slots in both (was 2 bins, 10/6, 1 slot). The 8-vs-4 spread is the clock ACQUISITION
    transient, not sampler variance: a locked 4× ladder emits exactly four phases, and that ceiling is the
    refresh ratio, not the sampler.
  - **S2.T1c** COMPLETE the replay record · **`done`** (2026-09-03, `records/S2_T1C_GATE.md`) — unplanned,
    and opened by decoding a real push block instead of trusting the plan's premise. The plan says a
    replay record is (prev, next, MV, gme, push, t); against the SHIPPING DEFAULT that is false. Four more
    planes are read: the backward MV (`occl_thresh`, `phase_anchor_on`), persistence (`inertia_thresh`),
    the SAD candidates (`ambig_on` + `gme_on`) and the target-generation MV (`vblend_on`). Those, plus
    both dissidence masks for the runs that DO read them, are now dumped, and `tgen` is recorded at the
    upload site; an absent plane is named `-` so a record states its own scope. Verified NOT read under
    this default, each at its call site: `u_field`, `u_prev_out`, and the dissidence masks (every
    ordinary site is gated on `matte_on`, which is 0 — my first pass wrongly gated them on `gme_on` and
    the correction is recorded in the gate). `check_qdump_plus.py` now AUDITS replayability per record —
    the pre-T1c record reads NOT REPLAYABLE with the four gaps named, the T1c record reads REPLAYABLE.
  - **S2.T6** the CPU reference warp · **`done — gate PASSED (§9, 2026-09-04): byte-exact on flat content, k 0.999 from 1 px on the textured default, fed both post-pass fields`** (2026-09-03, `records/S2_T6_GATE.md`)
    — `tools/ref_warp.py` replays a `--qdump+` triple and rebuilds the store. The surface is small
    because `single_track = 1.0` makes the store `mix(B_samp, cur[uv], w_s)` and shadows the whole
    commit/matte/onepos/blend cascade, so only shader lines 279–522 plus the stasis bool decide the
    output. The phase anchor's use of the PRE-reclaim `mv_fwd` is reproduced bug-for-bug (XR7), and
    `unsupported()` refuses any push arming a path the reference does not implement.
    **Passes:** 99.88% of pixels exact on the shipping default (worst 739–788 px per frame, all in a
    ring at the moving silhouette). **Does NOT pass:** under `--st-no-stasis`, which strips the 99%
    stasis copy and leaves a bare `result = B_samp`, exact match falls to 84.39% and the least-squares
    displacement scale is **k = 0.702** (0.829 on the default record) against the 1.000 the gate needs.
    **The error's SHAPE matters more than its size:** binning the fit by displacement magnitude shows a
    sub-pixel DEADZONE, not a scale. Above 4 px the reference is right (k = 0.955, corr 0.97); below
    0.5 px the shader moves essentially nothing (mean change 0.03–0.09 of 255) while the reference moves
    0.9–4.0. The frame-wide k is just those two populations averaged.
    **Ruled out, each by a number:** a stale record (a snapshot taken at the `wap_upload` call is
    identical to the late read on 100.00% of texels), GPU sub-texel filter quantization (8 and 6 bits
    both make the fit worse), every MV stage individually (ablation moves k by 0.005), the ambiguity
    rule as a damper (fires on 0.06% of blocks), and `bg_reclaim` as a damper (its `nonconf` gate is 0
    on the background). Open lead: the MV field carries a period-3 sub-pixel pattern matching the zoo's
    24 px lattice on blocks whose own `sad_best` is 0.
  - **S2.T2** the ground-truth marker zoo · **`done`** (2026-09-04, `records/S2_T2_GATE.md`) —
    `tools/motion_truth/marker_zoo.py`: six trajectory classes, three marker sizes, three background
    classes (`flat`/`noise`/`grating`), a 16-bit per-frame barcode, and the closed forms written beside
    the pixels so `p(t)` is evaluable at any real `t`. Gate run against the files ON DISK: 60/60
    barcodes decode to their own index; `p(t)` vs the written table worst |delta| **4.99e-10 px**;
    0 byte-identical consecutive frame pairs; displacement span 1.75–15.33 px/frame including the
    `fast` expected-failure control outside the matcher's reach.
  - **S2.T3** the on-screen player · **`done`** (2026-09-04, `records/S2_T3_GATE.md`) —
    `tools/motion_truth/play_frames.ps1`, borrowing `ball_zoo.ps1`'s proven Stopwatch/`timeBeginPeriod`
    loop. Held **60.1** and **120.1 fps** with **0 missed ticks** (120 sustained 30+ s, error 0.08%).
    The FG ingests it as ordinary content: `arr/cap 59–61/s` against a 60 fps source, `uniq 240/s`.
    **The result that matters:** the frame-ID barcode survives the WHOLE chain — zoo render → blit →
    window → WGC capture → convert → the warp's anchors → `--qdump+` readback — and the decoded step
    from `prev` to `next` is **1 on 12 of 12 triples**, the 59→0 loop wrap included. The FG's `t` can
    now be tied to an exact pair of KNOWN source frames, which is what M1 needs. It also corrected a
    documented claim: the triple is (N, N+1), not (N, N+2) — the `N+2` wording survived from the
    held-out design and this tap holds nothing out (`cli.cpp:105-106`, `present.cpp:1409` fixed).
  - **S2.T4** the extractor · **`done`** (2026-09-04, `records/S2_T4_GATE.md`) —
    `tools/motion_truth/marker_extract.py`: NCC of the marker's own pattern (now carried in
    `trajectories.json`) around `p_model`, sub-pixel by PHASE SEARCH (the plan's parabola pixel-locked
    at 0.188 px; inverting the zoo's own splat brought it to 0.05). Seen red twice and fixed twice.
    §4.1 on RAW frames: **0.060 / 0.045 / 0.052 px** (12/24 px, three frames); whole-pixel rolls 0.060;
    the RED check reports 6.401 px against a true 6.403. §4.2 through the FG: **0.085 px** mean
    (0.052 on 12/24 px). The 6 px class is REPORTED at 0.14–0.33 px, above the gate, with its 16-bit
    interior as the reason. A miss now records the strongest sub-floor match, so "degraded" and
    "absent" are separable — 65 of the 82 first-run misses were degraded, not absent.
    **The first M1 numbers ever** (one scene, one run, NOT a baseline): HUD class **0.046 px** kept
    exact; moving classes 0.9–1.5 px from a correct warp with roughly half arriving degraded; the
    `fast` control 4.7 px — the expected failure fires.
  - **S2.T5** the report + the DI-3 two-run baseline · **`done`** (2026-09-04, `records/S2_T5_GATE.md`,
    the table at `docs/evidence/MOTION_TRUTH_BASELINE.md`) — `tools/motion_truth/motion_report.py`
    computes `r` per class between two runs and MARKS `r < 0.5` UNRELIABLE itself, excluding it from
    the verdict line. **M1 is measured, with its reliability attached, for the first time.** Shipping
    default, static aperiodic background, 2 × 16 triples: accel **0.646 px (r 0.76)**, circular
    **1.001 (r 0.95)**, crossing **1.213 (r 0.91)**, the `fast` control **5.160 (r 1.00)**; `linear`
    0.620 px at **r 0.47 — UNRELIABLE**, not usable per cell (§4.3 NOT met on the class it names;
    the remedy is sample size, jitter and duplicates being excluded). **The error falls with phase:
    1.668 px at t≈0.1 → 0.373 at t≈0.85** — the single-track default's own "t=0+ pays the full backward
    warp", measured. In time: the generated frame sits **4–6 ms** from where it claims to be, against a
    16.7 ms pair. The panning scene is a second document, one run, stamped NOT a baseline by the tool;
    its HUD class (screen-fixed over a moving world) is **0.046 px**.
  - **S2.F1** the low-phase error, explained · **`done`** (2026-09-04, `records/M1_LOWPHASE_FINDING.md`) —
    the operator asked whether 1.7 px at low phase is acceptable. **It is not a property of frame
    generation: it is ONE default-on layer.** The dumped MV field is honest (EPE 0.164 px on `linear`);
    the error is signed forward along the motion (88–100 % of markers displaced toward their t=1
    position); and four conditions, two runs each, isolate it: `--st-no-stasis` leaves it (1.693),
    `--no-mv-guided --mv-median` leaves it (1.493), **`--no-mv-guided` removes it — 1.668 → 0.271 px,
    flat across phase**. The cause is `shaders/mv_median.comp`, the 3×3 vector-median consensus pass,
    armed by default through `mv_guided = true`, which votes a small moving object's tile toward its
    static neighbours' zero. **By standard:** A0 froze M1 as COMPARATIVE (≤ 0.10 px vs the shipping
    default) with no absolute bar — which, as frozen, would penalise the fix; the field has no
    positional-error bar; perceptually the mean is 2.5–3 arcmin and 21–97 % of a frame's motion,
    in the range cinema judder literature treats as visible. **The default is untouched; the switch
    is the operator's.** The pass's own purpose (flat-content rim stamps) was NOT tested here.
  - **S2.F2** the pass seen directly, and T6's residual explained · **`done`** (2026-09-04, `S2_T6_GATE.md`
    §8, `S2_T1C_GATE.md` §6) — `--qdump+` gains `mv1=`, `wapMVA` read back AFTER the consensus pass; the
    `mv=` plane was the field BEFORE it, so T6's oracle had modelled a warp reading a field the warp
    never read. Fed `mv1`: **k = 1.000 at 4–8 px, 0.957 at 2–4** on the shipping default (two runs:
    0.891 / 0.867 whole-frame, from 0.730 / 0.735). The pass touches 97.5 % of texels and doubles the
    marker-tile EPE (0.83 → 1.64 px). **T6's own criterion is met at ≥ 4 px.** The audit now requires
    `mv1` for any push with the pass armed.
  - **S2.F3** the pass on its OWN content · **`done`** (2026-09-04, `M1_LOWPHASE_FINDING.md` §6) —
    `ball_zoo.ps1 -BgClass flat`, a 260 px disc over a uniform field: the matcher emits 500–900 stray
    vectors per frame there and **the pass removes none** (stamps 7,494 → 7,500), 5 % of the holes, and
    leaves the disc's motion untouched; at the output the disc lands at 0.13 px with the pass and
    0.06 px without, 0 % rim tearing either way. Its design premise (an ISOLATED outlier) does not match
    what the matcher produces (clusters). **Both halves of the decision now exist; the default is still
    untouched.** The pass also filters the BACKWARD field — `mvb1=` now reads it back too.
  - **S2.F4** T6's gate PASSED · **`done`** (2026-09-04, `S2_T6_GATE.md` §9) — with `mv1` + `mvb1` the oracle
    is **byte-exact on 12 of 16 triples per run** on the flat scene (max 1 level, 0 px > 8, k = 1.000) and
    reaches **k 0.966 / 0.999 / 1.000 / 1.000** at 0.5–1 / 1–2 / 2–4 / 4–8 px on the textured shipping
    default (exact 99.88 / 99.86 %, two runs). M4's corpus rule, final: aperiodic, carrying `mv1` +
    `mvb1`, scored from 1 px. Remaining residual: `t ≈ 0.87` only, ≤ 0.06 % of the frame, recorded.
  - **S2.T6** CPU reference warp (E7) · **`superseded`** by the dated row above (`built, gate NOT
    passed`, 2026-09-03). Left in place per the never-delete rule; do not read this line as a state.
- **S3 — LAYER CONTRACT (the AAP search)** · **`done`** (2026-09-03) · `docs/planning/aap/`
  - **S3.0** A0 frozen objective written · `done` (2026-09-02) → **S3.1** AT0 clean gate · `done`
    (`FREEZE WITH FIXES` → 3 point-fixes applied → frozen, sha256 `670687d0…6933`)
  - **S3.2** A1 three isolated designers (fused-variants / staged-SG-passes / data-driven table) + AT1 +
    AT2 (3 lenses × 3) + ATG (2 lenses on the minimal core) · `done` (2026-09-02: AT1 `DIVERSE`; 9/9
    `SCORED`, 0 FATAL; ATG 2/2 `SCORED`, 11 FATAL on the base → dispositioned)
  - **S3.3** A3 convergence (supervisor, citing scorecards) → **C (LAYERTAB) + 5 grafts**
    (`aap/A3_CHOSEN_DESIGN.md`) · `done` → AT3 `CONVERGED` · AT4 `APPROVED FOR HANDOFF` · ATF `APPROVED
    FOR HANDOFF` (2026-09-03, verbatim in `aap/AT3_AT4_ATF_VERDICTS.md`; the AT3 judge's one uncarried
    item became `CONVERGENCE_RISK_REGISTER` XR13) · **`done`** — released to KAP + PLAN_TIER (S4's triad)
- **S4 — CONVERGENCE (the shipping repo rebuilt IN PLACE, by the six stages, on the LAYERTAB contract)** ·
  **`active`** (host decision 2026-09-03) · boundaries = [`STAGE_CONTRACT.md`](STAGE_CONTRACT.md)
  (`approved`) · detail = the v2 triad [`CONVERGENCE_MASTER_PLAN.md`](CONVERGENCE_MASTER_PLAN.md) +
  `CONVERGENCE_IMPLEMENTATION_STRATEGIES.md` + `CONVERGENCE_RISK_REGISTER.md` (Tier-2; supersedes
  `../research/MINIMAL_FG_MASTER_PLAN.md` §5 P2–P5; carries MR-1..MR-8 + CR1/PR1/DR2 + XR1..XR14; XR8/XR12
  superseded). v1 (2026-09-03 morning: base-hosted, C0–C6) is kept as the record inside the master plan
  §2.1; the old node IDs map: S4.1 ≡ R1, S4.2 ≡ R5 + the S5 gate, S4.3 ≡ R4, S4.4 ≡ R7.
  - **S4.0** adopt the seam: `apps/minimal_fg/include/minimal_fg/seam_graph.hpp` + its 122-check test into
    the repo (`src/seam/`, `tests/seam/`) — the app is NOT adopted (operator, 2026-09-03) · **`done`**
    (2026-09-03, inside R2a; the container copy is frozen with a pointer note, XR10)
  - **S4.R0** CONTROL: the layer registry + `--layer-dump` + `--layer-model-json`, `wap_warp.comp`
    UNCHANGED = **M-R0** (M2a, M3 vs E1); exit gate = the column-closure experiment (`≥ 4` new columns →
    RE-SELECT toward Candidate A) · **`done`** (2026-09-03 — `records/R0_GATE.md`; closure = 0 new columns;
    two R3 design constraints recorded: the `WEIGHT` stage with the core split, and `CH_BLEND` + `select` as a row)
  - **S4.R1** Stage 4 CLOCK extracted as `PhaseClock` (CPU value type) + the arrival-log replay test
    (bit-parity, XR14) + the synthetic-arrival test = **M-R1** · **`done`** (2026-09-03, `records/R1_GATE.md`;
    98.80 % verbatim, 5 transforms; 14,390-tick replay with 0 mismatches; `--arrival-log` + `pfg_clock_test`
    are now the standing regression harness for every later change to the phase)
  - **S4.R2** the seam adopted + grafted + DRIVING stage 5's barriers = **M-R2** · **`done`**
    (2026-09-03, `records/R2_GATE.md`, G-R2 passed): Vulkan 1.3 + `synchronization2` queried and ENABLED,
    `--validation` added, `src/seam/` + `tests/seam/` with grafts G3/G4/G5, the two engine gaps CLOSED
    (a per-image import layout; a declared resting layout emitting a cross-frame EPILOGUE barrier), and
    `--sg-barriers` recording stage 5's output barriers through the graph. **148 checks**; the derived
    barriers are field-identical to the three hand-written ones; `--sg-dump` byte-identical x2; **0
    validation lines** over a 60 s soak saturated with `gpu_load`; M3 inside spread on 2 runs/side.
    The derived path is OPT-IN: the default still records the hand-written barriers, so the product is
    unchanged. Flipping the default is a separate decision (a longer soak + the operator's eye).
  - **S4.R3** Stage 5: `fg_core.comp` + the 8 fused rows replace `wap_warp.comp` for the default set =
    **M-R3**; M4 by S2.T6 on the `--qdump+` replay (packed value first, XR1); bg-reclaim bug-for-bug (XR7) ·
    **`done`** (2026-09-05, `records/R3_GATE.md`, gate PASSED (residual attributed)) — `shaders/fg_core.comp` + twelve row bodies + the
    core in `fg_core_math.glsl` (sample half → WEIGHT → blend half), the generator with the WEIGHT stage,
    `CH_BLEND`, the declared-`needs` rule and the overrides/`c_in` check (all three seen RED on negative tests),
    the depfile (XR13), `--fg-core` opt-in, and the M4 INSTRUMENT `--fg-core-ab` (both kernels every tick on
    the same inputs, a byte-diff pass with an evidence list). byte-identical to the legacy kernel on every compared tick once FMA contraction is forbidden in both modules; with the driver's contraction allowed, ~7×10⁻⁹ of the pixels differ by one level — attributed by experiment, `records/R3_GATE.md` §4. Declared deviations: the 44-B push
    (CorePush + the gme gen-scalars), three dead `shadows` not reproduced, the `single_track ON` envelope,
    `warp_light` unreproducible, no `cam_lead`/`extrap` row. Former block text kept: (S2.T6's RESIDUAL, much smaller since 2026-09-04: the deadzone was the periodic test
    content and is closed — on aperiodic content the oracle reaches k = 0.956 / corr 0.987 where motion
    is real. What remains is a 5–13 % over-displacement on moving content against the 1.000 the gate
    asks. The fear that the shipping default was insensitive to sub-pixel motion is NOT confirmed.
    M4's corpus must be APERIODIC. Corpus machinery ready: S2.T1b coverage + S2.T1c completeness)`
  - **S4.R4** Stage 6 PRESENT extracted (`PresentStage`, `Phase.decision`, `FlipStats`) = M-R4 ·
    **`done`** (2026-09-05, `records/R4_GATE.md`, gate PASSED except the forced-TDR item (built, awaiting the operator's word)) — the present stage is `src/present/present_stage.{hpp,cpp}` — the surface, the accounting, the two bridge slots, the async preamble (one body for the two former copies), the slot decision with `{Warp, Dup, Drop}` as its INPUT, the submit, `--shallow-queue`, the present tail, `FlipStats` consumed by the CSV row — 83.95 % of the moved body byte-identical, the 13 differences all declared renames; the loop keeps its names as aliases; A/B against the pre-R4 binary in `records/R4_GATE.md` §4. Declared residual: the per-second stats
    (`stats_second()`) stayed in the loop (~30 counters to bind); the S8 guard-band drop NOT added (no
    legacy equivalent; a measured decision, not an extraction). **R4b (same day):** the `--exit-after` guard hoisted to the tick boundary (grid mode self-bounds now); `phyriadfg_fresh` per tick + `fresh:N/s` + `--warp-timing` (GPU timestamps around the warp batch); measured over 17 runs: the default async present delivers **49.9 % fresh frames** (119/s); the warp's GPU time is 0.1 ms and it can complete in 0.3 ms, but under the default it waits ~one panel period (the keyed-mutex hand-off behind the previous present's copy — tested with `--present-waitable`: 99.85 / 99.84 % fresh on the async path (14,362 / 14,360 of 14,383), the fence at the first poll, 0.25 ms p50 when spun, `MsAddedLatency` 21.21 vs the default's 20.86 — the wait IS the chain; option (4) is that knob, pending a `gpu_load` + real-content pair); the 16 s shallow-queue cycle is the grid re-seat at `present.cpp:1819`. Register **XR15**, the operator's present-policy decision. **R4c (same evening):** the operator ran `--tdr-test 15`: detection PROVEN (`VK_ERROR_DEVICE_LOST` latched + printed), the teardown HUNG on `vkWaitForFences(UINT64_MAX)` after the loss → `vk_wait_live` (bounded, device-loss-aware) at all 14 sites; his second run: P wedged inside a driver/DXGI call, the own-window plane left on the panel (the pillar's watchdog cannot hide a wedged thread's window) → the joins carry a 3 s deadline under loss, survivors named, `TerminateProcess`; healthy paths re-verified after each; **his third run closed the item** (`-- P(present) --` named, the process terminated, the panel released). Former status text: `startable` (corrected 2026-09-04 — the old `blocked (R1)` was stale: R1 is `done`). What
    actually constrains it: the STAGE_CONTRACT Order table puts it after R3 (a sequencing
    preference, not a dependency); the register forbids COMMITTING it while MR-1/MR-2/MR-7/CR1 are
    `open`; and G-R4 needs `--tdr-test`, which does not exist and must be built inside R4.
  - **S4.R5** Stage 3: `FlowSet` + `FlowRing` declared; holons → `kind = P` rows (off); MV median → stage 3;
    `wap_upload` conditional on device count = M-R5 · **`startable, sequenced after R4`** (corrected
    2026-09-04 — the old `blocked (R2)` was stale: R2 is `done`). Its scope overlaps R4's (it moves
    `present.cpp:713-836` and `:779`, which R4 extracts), so doing it first re-does work. Its paper
    half — mapping `flow.cpp`'s holon family onto the LAYERTAB schema, the exercise R0 did for ten
    shader layers — is unblocked TODAY and is XR3's accepted residual.
  - **S4.R6** Stages 1–2 named (`capture/` + `ingest/`), `RawFrame`/`RealFrame` = M-R6 · `blocked (R5)`
  - **S4.R7** closing: `--legacy-warp` out of the default only with M1 baselines on both paths (S2.T4–T5) +
    MR-4 byte-diff + MR-8 operator eye = M-R7 · `blocked (operator, S2)`
- **S5 — PERCEPTUAL QUALITY (ghosting / crescents / seams; the 0.3.0–0.4.0 layer arcs)** · **`parked`**
  (operator, 2026-09-02: "no me interesa de momento"). Kept whole: the layers stay in the donor; each
  re-enters only through S4.2's net-gain gate. Detail: `SINGLE_TRACK_MODE_PLAN.md`, `SATURATION_PLAN.md`,
  `MV_EDGE_SNAP_PLAN.md`, `PREDICT_MODE_PLAN.md`, `WGC_INGEST_ASYNC_PLAN.md`.
- **S6 — MULTI-GPU** · **`parked` (operator, 2026-09-03)** — the 1080 Ti is physically INSTALLED but not
  driver-available, so the 4090 is the only usable FG device today (the AMD iGPU also enumerates; the app
  collapses to SINGLE-GPU). Deliberately deferred: it is RESUMED once the generation core is stable,
  not abandoned. Nothing in R0–R7 removes a multi-GPU path; R5 only makes the `wap_upload` copy
  conditional on the device count, which is the shape multi-GPU needs anyway.
- **S7 — TESTBENCH axes 2/3/5 (eye 2AFC, camera, input→photon)** · **`parked`** — `../research/FG_TESTBENCH_*`;
  S2 is their Axis-1 geometry slice, narrowed.

## Dead-ends not to re-try (from the June spine, kept)
- A `--minimal` strip flag on the accreted `main.cpp` (measures with the bad seam) — rejected 2026-06-26.
- Present-hook / injection — rejected (anti-cheat; the external-capture identity).
- Keyed-mutex cross-thread capture in the minimal core — deadlocked under saturation (BF6); the full
  FG's lock-free host-staged ring is the solved mechanism.

## Links
- Objective vista + fixed points: `../research/PHYRIADFG_OBJECTIVE_VISTA.md` (C8 regression gate, C11
  testbench). June triads: `../research/MINIMAL_FG_*`, `../research/FG_TESTBENCH_*`,
  `../research/CAPTURE_LAYER_*`. The container's historical spine: `F:\Phyriad\catalog\cpp\docs\plans\ACTION_PLAN.md`.
