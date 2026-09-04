# A3 — SELECTION RATIONALE (the supervisor's convergence, citing the scorecards)

> Companion of `A3_CHOSEN_DESIGN.md`. Every grade below is quoted from `AT2_SCORECARDS.md` (nine clean
> sonnet contexts, three lenses × three candidates; all nine returned `SCORED`; AT1 returned `DIVERSE`).
> The frozen priority is applied as written: **M1 > M2 > M3, M4 veto**.

## 1. The scorecard grid (lens order: budget-auditor / performance-realist / adoption-skeptic)

| Metric | A · FUSED-VARIANT | B · Stages as SG passes | C · LAYERTAB |
|---|---|---|---|
| **M1** motion exactness | 4 / 4 / 5 | 3 / 5 / 4 | 5 / 4 / 5 |
| **M2a** files to add a layer (≤ 2) | 4 / 5 / 4 | **2 / 2 / 2** | 5 / 5 / 4 |
| **M2b** disabled-layer GPU work = 0 | 5 / 4 / 4 | 5 / 5 / 5 | **2 / 2 / 2** |
| **M2c** core params ≤ 8 | 5 / 5 / 5 | 5 / 5 / 5 | 4 / 4 / 5 |
| **M3** overhead | 2 / 2 / 3 | 2 / 2 / 2 | 4 / 5 / 3 |
| **M4** default equivalence (veto) | 4 / 4 / 4 | (row) / 4 / 4 | (row) / 3 / 4 |
| FATAL defects | 0 | 0 | 0 |
| MAJOR defects (counted) | 1 / 2 / 2 | 2 / 5 / 2 | 3 / 3 / 5 |

"(row)" = the lens wrote the M4 row without a numeric cell (its text: B "every default-graph node is a
verbatim, line-cited port"; C "7/8 rows claimed byte-identical … 1/8 carries a disclosed 1-ulp risk").
No candidate crossed a kill criterion per the D-sections; no FATAL was recorded for any candidate.

## 2. Applying the frozen priority

**M1 (first).** C and A tie at the top (all three lenses verified the core math is a verbatim port of
`wap_warp.comp:496–529/518–521`); B is a step below because its fp32 stage-crossing packets are what
make M4 provable but are the only path where geometry could drift (B/budget-auditor: "verified
textually identical … registers-only arithmetic" now crosses DRAM). M1 does not separate C from A.

**M2 (second) separates the field.** B **fails its own M2a target**: all three B lenses recomputed its
§1.3 layout as 3 new files + 1 registration line = 4 under `git diff --stat` (B/adoption-skeptic:
"Fails as specified"). A and C both meet ≤ 2 files. On M2b A scores highest (spec-constant DCE is
"provably" zero — A/budget-auditor 5/5) and C lowest (2/2/2), but the three C lenses agree on the same
reason: **the deficit is the instrument, not the cost** — for the fused shipping default there is no
per-layer SG node for `--sg-dump` to census (C/performance-realist: "Not demonstrable by the objective's
own instrument … for the case that is actually shipped"). C's fused rows use the SAME spec-constant
mechanism A is graded 5/5 for; the chosen design therefore adopts A's attribution route (pipeline-variant
SPIR-V diff + Nsight occupancy, A-M1 step) as the M2b instrument for `kind = F` rows and B's/SG's
`--sg-dump` for `kind = P` rows. M2c: all three pass.

**M3 (third).** C is graded non-regressive by construction (4/5/3: "identical-or-better than today on
every line"); A is honest but unmeasured (2/2/3) with a named structural risk — register/occupancy
non-isolation of a fused pass — that A itself flags as its weakest point and that A/performance-realist
calls "the one input that would let anyone judge M3 … not computed anywhere"; B costs an estimated
+0.42–0.56 ms per 4.17 ms tick (+10–13 %), **recomputed and confirmed by all three B lenses**
(B/performance-realist: "independent recompute ≈309–349 MiB/tick"). Under M1 > M2 > M3, B's M3 does not
decide (M2a already did); A vs C on M3 favors C.

**M4 (veto).** All three pass; none is disqualified. C's 1-ulp packing risk is disclosed and has a
concrete mitigation (reproduce the packed value host-side before the flip — C §4.1); B's bug-for-bug
reproduction of the dead bg-reclaim is the same fact C must honor (A3_CHOSEN_DESIGN §3).

**Result:** C wins on the priority ordering (ties A on M1, beats B on M2a and both on M3), with A's
structural guarantees grafted where C is weaker (the composition law) and B's SG mechanisms grafted
where C is silent (dominance detection, liveness spec constants, the allocation defect in the adopted
engine). B is not chosen because its central bet — modularity bought with intermediate images — pays in
the metric the objective ranks third and fails the one it ranks second; A is not chosen because its
central bet (bandwidth) is, in A's own words, "weak" (1–2.4 % of peak), and its unmeasured occupancy
risk is exactly what C's `kind` column lets a later measurement decide per layer instead of globally.

## 3. Grafts — traceability (AT3 check 4)

Every graft in `A3_CHOSEN_DESIGN.md` §2 is quoted from a scorecard's "salvageables" section: G1 and G2
from A/performance-realist (items 1 and 4) and A/adoption-skeptic (item 1) and A/budget-auditor; G3 from
all three B lenses (item 1); G4 from B/performance-realist (3) and B/adoption-skeptic (2); G5 from
B/budget-auditor (2), B/performance-realist (6), B/adoption-skeptic (4). B's Appendix A.4 was NOT named
by any lens and is NOT grafted.

## 4. The weakest point, named

C's own (C §7.2): the column set's closure is unproven — eight layers cost four schema extensions; the
falsifying experiment (map ten more layers, count new columns) is placed BEFORE the first shader change
as a gate with an explicit RE-SELECT path toward A if it fails (A shares the fused core, so the port
work is not lost). The `shadows` backward reach for `single_track` is a declared, printed wart kept
only because M4 forbids changing the product.

## 5. The honest ceiling

Best of three searched angles against the declared objective — not the best possible design. M1 has
no measured number for ANY candidate yet (the MOTION_TRUTH instrument is `designed`); the M1 grades are
analyses of core-math preservation, as A0's gating note prescribes. M3 numbers are estimates or
"unmeasured" for A; only C's structural argument and B's recomputed traffic are grounded in arithmetic
the lenses redid. The first milestone (M-C1) is the first real number.
