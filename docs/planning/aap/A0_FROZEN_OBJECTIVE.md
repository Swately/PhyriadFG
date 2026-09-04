# A0 — FROZEN OBJECTIVE (DO NOT DRIFT): the layer contract of the final PhyriadFG

> AAP pass A0 (`F:\Phyriad\protocols\analysis\ARCHITECTURE_ANALYSIS_PROTOCOL.md` §3.A0). Written
> 2026-09-02 BEFORE any candidate exists. Once the AT0 gate passes, this block is frozen byte-for-byte
> (AINV-1/12); re-opening is a deliberate return to A0, never a side-effect. Status: `frozen pending
> AT0` → `frozen` (the gate record is appended at the end, the block above it never edited).

## 0. The design problem (raw)

PhyriadFG's frame generator today is one 1,336-line compute shader (`shaders/wap_warp.comp`) whose
generation core is ~35 lines (primary MV fetch → two displaced samples `A[uv−t·mv]`, `B[uv+(1−t)·mv]` →
confidence/agreement gates → blend → store) wrapped by 53 push-constant-gated quality layers composed
as SEQUENTIAL OVERRIDES of one `result` variable ("the FINAL word", "pre-store override"): the frame that
ships is whatever wrote `result` last. Under the shipping defaults the last writer (lines 1321–1328)
discards everything computed between lines 531 and 1320. Adding a layer today touches four hand-synced
sites (Config struct, parse, help, UI model — 257 CLI flags vs 173 UI entries, 84 CLI-only) plus the
shader. The operator's verdict: "parche tras parche tras parche"; the ask: a modular structure that
separates the essential generation core from the layers.

A prior arc (MINIMAL_FG, 2026-06-26, `docs/research/MINIMAL_FG_*`) already fixed the SEAM: a
render-graph engine (`apps/minimal_fg/include/minimal_fg/seam_graph.hpp`, "SG") where passes declare
reads/writes, barriers are derived, and a disabled pass is culled at zero cost. That decision is a
DECLARED CONSTRAINT here (adversarially verified then; not re-litigated). What SG does NOT decide — and
what this search decides — is **how the generation core and its layers are expressed as passes and
contracts**: the granularity, the data contract between them, and the mechanism by which a disabled
layer contributes nothing.

## 1. Success metrics (measurable — each read off an instrument, never adjectival)

| ID | Metric | Instrument | Target |
|---|---|---|---|
| **M1 · Motion exactness** | Per-marker position error of GENERATED frames vs the analytic trajectory `p(t)` on the motion-truth zoo (markers of classes: linear / accelerating / circular / crossing / static-HUD over moving background), mean + p95 in px per class; plus phase error in ms-equivalent (`err_px / |v|`). | `MOTION_TRUTH` instrument (`docs/planning/MOTION_TRUTH_MASTER_PLAN.md` §2, phases T1–T5): `--qdump+` replay record → marker extractor → CSV. Two runs per condition, run-to-run `r` reported (DI-3). **Gating note (AT0 fix 2):** the instrument is `designed`, not built; until its phases T4–T5 pass their acceptance (§4 of that plan), candidates are scored on M1 by *analysis of measurability and of core-math preservation* (does the candidate keep the MV path and the sampling form so that M1 equivalence is expected, and can the instrument measure it unchanged?); the MEASURED M1 verdict is the acceptance gate of the convergence plan's port stages (`ACTION_PLAN.md` S4.2), never assumed. | For the SAME enabled-layer set, the candidate's error distribution is statistically indistinguishable from the current shipping `wap_warp` default (paired, ≤ 0.10 px mean shift, p95 within run-to-run spread). Any candidate that IMPROVES it is scored higher. |
| **M2 · Modularity** | (a) Files touched to ADD one new layer, counted by `git diff --stat` on a reference "add a trivial layer" exercise; (b) GPU work of a DISABLED layer: dispatches + barriers attributed to it in the SG dump / timestamp query = 0; (c) size of the CORE contract: number of parameters the core pass consumes (today 57 push-constant fields; the core needs 6: `residual_ceil, improvement_frac, agreement_threshold, t, mv-source, output`). | `--sg-dump` pass/barrier census; `git diff --stat`; a header count. | (a) ≤ 2 files (the layer's own + one registration line); (b) exactly 0; (c) ≤ 8 core parameters. |
| **M3 · Overhead** | Present cadence (presents/s vs panel rate) and warp-record + GPU time per tick on `ball_zoo` at 60 fps source, 1920×1080, default layer set; input→photon proxy (`lat` stat). | The existing `--csv` telemetry + `ball_zoo.ps1`; 60 s runs, 2 per side (DI-3). | No regression beyond the measured run-to-run spread vs the E1 tree (`RESTRUCTURE_PLAN` G1 baseline: 240 presents/s on the 240 Hz panel, 28,799 presents/120 s). |
| **M4 · Default-output equivalence (the correctness oracle, D-13)** | With the shipping default layer set enabled, the candidate's output vs today's `wap_warp` on the same replay record (prev, next, MV, gme, t). | Byte-diff / PSNR on the replay set — the CPU reference warp of `docs/planning/MOTION_TRUTH_MASTER_PLAN.md` §5 phase **T6** (implementation strategy S6 of its companion), fed by the `--qdump+` record (T1). *(AT0 fix 1: the earlier "E7" label named this test only in conversation; the plan-doc home is T6/S6.)* | Byte-identical, or PSNR ≥ 60 dB with every difference explained by a documented floating-point-order change. |

## 2. Priority ordering (who wins on conflict — PERFECTION_TRADEOFF_DOCTRINE applied)

**M1 (exact motion) > M2 (modularity) > M3 (overhead)**, with **M4 as a VETO** (a candidate that cannot
reproduce today's default output for the layers it keeps is disqualified — it changed the product, not
the structure). Perceptual quality (ghosting, crescents, seams) is **NOT a metric of this search**
(operator directive 2026-09-02: parked, not dropped — it re-enters as the P5 net-gain gate of the
MINIMAL_FG plan once motion is exact).

## 3. Feasibility envelope (hard constraints)

- **Rig:** the operator's desktop — NVIDIA RTX 4090 is the ONLY Vulkan device enumerated today (the
  2026-08-31 smoke ran `SINGLE-GPU: B/G suppressed`; the 1080 Ti / iGPU of the June record are not
  present). Panels 1920×1080 @ 240 Hz and @ 180 Hz. Multi-GPU paths cannot be measured on this rig today.
- **Toolchain:** MSVC 19.44 (Build Tools 17.14), CMake 4.4.1, Ninja 1.13.2, Vulkan SDK 1.4.357.0
  (`glslc`), C++23, Rust 1.98 (UI only), Python 3.13 with **numpy only** (no Pillow/OpenCV/scikit).
- **Composition mechanism:** the SG seam engine (`apps/minimal_fg/include/minimal_fg/seam_graph.hpp`,
  549 lines, 122-check golden test) — passes declare reads/writes, barriers derived, cull-when-off.
  A candidate MAY extend SG; it MUST NOT replace it with per-stage fences or host round-trips.
- **Identity:** external capture only (DDA/WGC of the composed desktop); NO game hook / injection
  (`PHYRIADFG_OBJECTIVE_VISTA` §3 — anti-cheat; the project's defining property).
- **Home:** the PhyriadFG git repo (`F:\Phyriad\projects\PhyriadFG`, branch `analysis/0.3.0-quality-push`,
  HEAD `c043780` + the E1 working tree). `apps/minimal_fg` lives in the container (no git) and would be
  ADOPTED into the repo — the adoption is a declared operator decision, not a candidate's assumption.
- **Existing assets a candidate MUST reuse, not rewrite:** `OpticalFlowPipeline` (pyramid block-match,
  MV+SAD fields at W/8), the `wap_warp.comp` core math, the FgContext thread model (C/F/P, SPSC rings),
  the E1 `init_*` functions + ownership structs, `fg_quality_scorer` + `prep_zoo_sequence.py` **for the
  photometric fixture (M4's PSNR path and the parked quality axis)**. *(AT0 fix 3:)* the **motion-truth
  zoo of §5 is a SEPARATE generator** — `tools/motion_truth/marker_zoo.py`, strategy S2 of
  `MOTION_TRUTH_IMPLEMENTATION_STRATEGIES.md` — with trackable markers, analytic trajectories, the
  static-HUD class and selectable backgrounds; `prep_zoo_sequence.py`'s presets (pan/pan_diag/accel/zoom/
  orbit/mixed/occlude) are NOT that zoo and are not extended to become it.
- **Time:** staged milestones, each build-green and measurable on this rig; no calendar deadline is declared.

## 4. Kill criteria (a candidate is DISQUALIFIED if any holds)

1. **Unmeasurable:** its M1 cannot be produced by the MOTION_TRUTH instrument as designed (e.g. it needs
   data the `--qdump+` replay record — prev/next/live + t + MV + SAD + gme + push block — cannot carry,
   or it changes the sampling so that marker positions are no longer a function of the record). Decided
   at AT2/AT4 by analysis of the instrument's definition (`MOTION_TRUTH_MASTER_PLAN.md` §2), since the
   instrument is not yet built (AT0 fix 2).
2. **Order-dependent semantics:** the meaning of the output depends on the ORDER in which layers write
   (last-writer-wins) — the defect under repair, reintroduced.
3. **Seam regression:** it adds a per-stage fence, a host round-trip between stages, or an `ALL_COMMANDS`
   barrier (the overhead the MINIMAL_FG arc removed, §3 of its master plan).
4. **Cannot express the shipping default:** it cannot host the default layer set (mv-guided, inertia gate,
   phase-anchor, bg-reclaim, ambig, vblend, single-track/screen-static, stasis) → no A/B against today.
5. **Injection / multi-GPU dependency:** it requires a game hook, or only functions with a second GPU.
6. **New runtime dependency** in the shipped binary (beyond Vulkan + D3D11/DXGI + the VC++ runtime).

## 5. Named workload

The **motion-truth zoo** (markers: linear, accelerating, circular, crossing, static-HUD-over-pan; backgrounds
flat / value-noise / periodic grating) presented on-screen at 60 and 120 fps, captured by the FG on this
rig, 1920×1080 — plus `ball_zoo.ps1` for cadence. Real games are the LATER eye axis, not this search's
workload.

## 6. Declared constraints — FORBIDDEN or DROPPED drivers (smuggling check)

- Perceptual/ghosting metrics as selection criteria (parked).
- A `--minimal` strip-flag on the accreted `main.cpp` (rejected 2026-06-26 — it measures with the bad seam).
- Present-hook / injection (rejected; anti-cheat + identity).
- The push-constant override chain as the layer mechanism (the defect).
- "Fewer layers" as a slogan (MINIMAL_FG §7: minimal-NET must beat complex-NET, measured).

---
*(AT0 gate record appended below by the supervisor; the block above is never edited after freeze.)*

## AT0 gate record (appended by the supervisor; the block above is frozen from here)

- **Gate:** AT0 (clean context, foreign judge; sonnet; 26 tool reads; 331 s). **Verdict: `FREEZE WITH FIXES`.**
- **Checks passed as written:** priority (M1>M2>M3, M4 veto) explicit; envelope complete and hard (every
  checkable claim verified exactly: branch/HEAD, CMake 4.4.1, Ninja 1.13.2, SDK 1.4.357.0, 1,336- and
  549-line files, the single-GPU log line); 5 of 6 kill criteria directly falsifiable; workload named.
- **Point-fixes required and APPLIED before freeze (no redesign):** (1) M4's instrument citation pointed
  at a non-existent "E7" → now `MOTION_TRUTH_MASTER_PLAN.md` §5 T6 / S6; (2) M1's instrument doc did not
  exist when the judge ran → it exists now, and M1/KC1 carry an explicit gating note (scored by analysis
  until the instrument's T4–T5 acceptance; measured at S4.2); (3) the motion-truth zoo is declared a
  SEPARATE generator from `prep_zoo_sequence.py`, which stays the photometric fixture.
- **Frozen 2026-09-02** — sha256 of this file at freeze recorded in `A0_FROZEN_OBJECTIVE.sha256`
  (AINV-1/12: ATF byte-checks against it).
