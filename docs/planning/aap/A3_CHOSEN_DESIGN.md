# A3 — CHOSEN DESIGN: LAYERTAB (Candidate C) as the spine, with five traceable grafts

> AAP pass A3 (supervisor convergence), 2026-09-02. Selected against the frozen objective
> (`A0_FROZEN_OBJECTIVE.md`, sha256 `670687d0…6933`) using the nine AT2 scorecards
> (`AT2_SCORECARDS.md`) — the reasoning is in `A3_SELECTION_RATIONALE.md`. **Scoped claim:** this is
> the best of the THREE angles searched (fused-variants / staged-SG-passes / data-driven table) against
> the declared objective; no absolute-optimality claim is made (AINV-11). Status: `chosen` pending
> AT3/AT4/ATF.

## 1. The spine — Candidate C (LAYERTAB), adopted as written in `CANDIDATE_C.md`

One X-macro registry row per layer (`src/layers/layer_table.def`) is the sole declaration site; the
`Config` fields, the parser, `print_help`, the UI JSON model (`--layer-model-json`, the UI renders the
binary's own model), the SG registration, the GLSL specialization constants, the layer UBO layout and
the execution order all derive from it. The core contract is C §2: `fg_core()` = the verbatim port of
`wap_warp.comp:496–529, 637, 679` (Gate 1, the two displaced samples, Gate 2, the base blend), with the
20-byte push `{residual_ceil, improvement_frac, agreement_threshold, t, arm_mask}` and the six-parameter
contract of A0 M2c. A row's `kind` column selects fused (`F`, spec-constant-gated inside `fg_core.comp`)
or pass (`P`, an SG node) per layer — the shipping default is 8 fused rows, **1 dispatch, 0 intermediate
images, 0 added barriers** (C §6.1–6.3, recomputed by C/budget-auditor and C/performance-realist).

**Order (kill criterion 2), from C §3.5, adopted verbatim:** no commutativity is claimed. Stage order is
fixed and total (`MVCOND → SAMPLE → fg_core() → COMPOSE`); within a stage `rank` is the order, unique by
`static_assert`; a COMPOSE layer is `vec4 f(vec4 c_in, ctx)` — no shared mutable `result` exists; an
override is a declared, printed bit (`overrides = true`) with a build-time check; the compiled chain is
stamped by a FNV-1a contract hash into `--csv` and the `--qdump` manifest. `--layer-dump` prints the
resolved order.

## 2. The grafts (each traceable to a salvage the adversaries named; none new)

| # | Graft | Source | Named as salvage by | What it adds to C |
|---|---|---|---|---|
| G1 | **Single-writer store as a structural guarantee** — no slot exists after `compose()`; the `imageStore` has ONE source expression unreachable from any layer body | `CANDIDATE_A.md` §3.4 item 1 | A/performance-realist salvage 1; A/adoption-skeptic salvage 1 | C already removes the shared `result` (`vec4 f(c_in)`); G1 additionally forbids a COMPOSE row from touching the store or the output image — the literal repair of today's six `result =` sites |
| G2 | **Convex fold for weighted COMPOSE contributors** — `r = mix(r, target, w)`, `w ∈ [0,1]`, with the obligation `w == 0 ⇒ identity`, checked by the M4 replay | `CANDIDATE_A.md` §3.4 item 3 | A/budget-auditor salvage; A/performance-realist salvage 4 | Gives C's COMPOSE stage a monotone, non-discarding composition law for every row that is NOT a declared override; the two legitimate overrides (`stasis`, `single_track`) keep C's printed `overrides` bit |
| G3 | **Dominance warning in `SeamGraph::compile()`** (`seam_graph.hpp:411–417`, the `has_producer && !read_here` WAW branch) + `dump_warnings()` (never `dump()`, golden test intact); promotable to a hard error unless `.dominates_ok = "reason"` | `CANDIDATE_B.md` §3.5 + Appendix A.1 | all three B lenses (salvage 1 in each) | Mechanizes kill criterion 2 for every `kind = P` row: a pass that overwrites what it does not read is named at build time. For `kind = F` rows the equivalent is C's generator check on `c_in` usage |
| G4 | **`optional_write` + liveness-driven specialization constants** — a store nothing live reads is compiled out | `CANDIDATE_B.md` Appendix A.2 | B/performance-realist salvage 3; B/adoption-skeptic salvage 2 | Closes the M2b attribution gap for `kind = P` rows and for optional outputs of fused rows |
| G5 | **Zero-allocation `SeamGraph::execute()`** — the per-pass barrier vector built at compile time; `execute()` patches handles only (~1,920 heap allocations/s removed at 240 Hz) | `CANDIDATE_B.md` Appendix A.3 | B/budget-auditor salvage 2; B/performance-realist salvage 6; B/adoption-skeptic salvage 4 | A real defect in the adopted SG engine, fixed before it carries the present thread |

Not grafted (not named as salvage by any adversary): B's Appendix A.4 (`add_pass(const LayerDesc&)`) —
C's generated registration subsumes its purpose; A's per-layer UBO-bind-when-enabled (C's UBO layout is
generator-emitted and supersedes it).

## 3. Facts the design must honor (found by the adversaries, verified first-hand by the supervisor)

- **bg-reclaim is dead under the shipping default.** `wap_warp.comp:344` captures `mv_fwd` BEFORE
  bg-reclaim damps `mv` (394); phase-anchor (410) rebases on `mv_fwd`, discarding the damp whenever the
  bwd field is valid (default ON). Found by designer B and confirmed by B/performance-realist; **verified
  by the supervisor 2026-09-02 (sed 342–412)**. M4 requires reproducing this bug-for-bug; the row carries
  `overrides`/`dominates_ok` with this citation, and the FIX is a separate operator decision recorded in
  the convergence plan, never a silent change.
- The 8 default-affecting layers (rig default: `igpu_field=false` cascades bg-snap/band-xfade OFF —
  verified by designer A) are exactly: mv-guided, inertia gate, phase-anchor, bg-reclaim (dead), ambig,
  vblend, single-track/screen-static, stasis.
- C's disclosed M4 risk: unpacking `mv_guided = 1.0 + sim` shifts `sim` by up to 1 ulp. The M-C2 replay
  MUST first reproduce the packed value host-side, then flip to the clean field (C §4.1, §7.1).

## 4. Measurement route (the "not blind" clause)

1. **M-C1 (first milestone, zero product risk):** the registry + generator + `--layer-dump` +
   `--layer-model-json` with `wap_warp.comp` UNCHANGED; a startup `layer_config_parity()` assert against
   today's `Config`; M2a measured (add a trivial row → `git diff --stat` = 2 files); M3 by `ball_zoo` 60 s
   × 2 runs vs the E1 baseline (240 presents/s). (C §6.8.)
2. **The column-closure experiment (the weakest point's falsifier, gates M-C2):** map TEN more layers of
   `wap_warp.comp` onto the schema and count new columns — `≤ 1` proceed; `≥ 4` the central claim is
   false → return to A3 (RE-SELECT toward A's fused-variant contract, which shares the fused core).
3. **M-C2:** `fg_core.comp` replaces `wap_warp.comp` for the default set; M4 by the CPU reference warp on
   the `--qdump+` replay (MOTION_TRUTH T6/S6) — byte-identical or explained; M3 within the run-to-run
   spread, 2 runs/side; M2b for fused rows attributed by pipeline-variant SPIR-V diff + Nsight occupancy
   (the attribution route A-M1 named — adopted as the instrument, not as design), for `kind = P` rows
   by `--sg-dump`.
4. **M1** measured only when MOTION_TRUTH T4–T5 pass their acceptance (`ACTION_PLAN.md` S2 → S4.2).

## 5. The honest weakest point (C's own, kept as THE weakest point)

The column set is not proven closed: mapping eight layers already forced four schema extensions
(`requires`-ANY, `shadows`, a `CH_STASIS` channel, pseudo-rows), and `single_track` needs a backward
`shadows` reach into the core — the very coupling the stage model forbids, kept only because M4 forbids
changing the product. The failure mode is a bespoke DSL that re-accretes the shader's complexity in a
schema. §4.2 is the falsifier and is run BEFORE any shader change. Secondary: `arm_mask` makes
"compiled out" true only for statically disabled layers; four default layers disarm per generation and
pay a uniform branch, as today.
