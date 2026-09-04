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

**`P → S2.T1b (fix the dump SAMPLING) → S4.R3 (fg_core.comp) · next`**
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
  - **S2.T1b** fix the dump SAMPLING · **`next`** — **an R3 PRECONDITION.** Measured on 16 triples: `t`
    landed in only 2 eighth-of-a-pair bins (ten at ≈0.125, six at ≈0.375) and every triple came from ONE
    generation. `kQdumpStride=11`'s comment claims successive dumps land on different phases; at 60 fps
    source on a 240 Hz panel (4 phase-steps/pair) they do not. M4 over such a set would test the core at
    one or two phases only. `check_qdump_plus.py` now WARNS on it so it cannot be inherited silently.
  - **S2.T2–T3** marker zoo + player · `next` (parallel to T1)
  - **S2.T4–T5** extractor + report + DI-3 baseline of the shipping default · `blocked` on T1–T3
  - **S2.T6** CPU reference warp (E7) · `blocked` on T1
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
    `blocked (S2.T1b — the corpus samples 2 phases / 1 generation today; and S2.T6, the CPU reference warp)`
  - **S4.R4** Stage 6 PRESENT extracted (`PresentStage`, `Phase.decision`, `FlipStats`) = M-R4 · `blocked (R1)`
  - **S4.R5** Stage 3: `FlowSet` + `FlowRing` declared; holons → `kind = P` rows (off); MV median → stage 3;
    `wap_upload` conditional on device count = M-R5 · `blocked (R2)`
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
