# 6. Discussion

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 7 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## D.1 — What the terms separate, as measured: phase, displacement, source rate; the lead sign as a reading

*serves:* CA · *evidence:* §05.1–05.4 today (§05.3 in its two-sided form); §05.6–05.12 when filled · *status:* `EXISTS`

Clause (a)'s three conditions are separated by rows that hold the other two fixed, each at its own reliability.

**Phase.** On the canonical row (§05.1) the fastest mover's position error falls across the phase bands:
0.668 px at n 20, 0.490 px at n 93, 0.252 px at n 20 (B1_FIRST_FG_ROW.md §2) — the speed path's only
two-seed point, "seeds 7 and 11 within 13 %" (B1_SPEED_TEST.md §6). The shape is not fixed: at 4× "the middle
band (0.814) exceeds the low band (0.758) while the high band stays low (0.267)" (B1_SPEED_TEST.md §3), one
run, reliability not measured (B1_SPEED_TEST.md §6). Phase and displacement interact (§05.2, §05.3).

**Displacement.** The sphere's position error rises 0.355 → 0.481 → 0.544 → 0.690 px as displacement goes
1.68 → 3.36 → 6.72 → 13.28 px/pair (B1_SPEED_TEST.md §3), the 0.5×, 2× and 4× points single runs, reliability
not measured (B1_SPEED_TEST.md §6). Which law describes that rise is open: §05.3 carries both fits and names
the operator as the one who decides (item ii below).

**Source rate.** At matched displacement and matched phase band the coarser source is worse by 1.10×, 1.33×
and 1.72× in the three bands (B1_SPEED_TEST.md §3), one run on the speed arm, second seed owed
(B1_SPEED_TEST.md §6). The record names candidates and stops: "Not attributed; named." (B1_SPEED_TEST.md §3).
A second instrument answers the same comparison the other way (item iv below).

**The lead sign.** Reported beside hallucinated mass, never a veto by itself; across the speed path the sphere's
lead falls from +47.8 px to +24.0 px between ×0.5 and ×4 (B1_SPEED_TEST.md §2), both single runs, reliability not
measured (B1_SPEED_TEST.md §6). Its mechanism belongs to §05.12, unfilled: "These are readings, not proofs of
cause." (B1_SPEED_TEST.md §4); "No ablation yet." (B1_SPEED_TEST.md §6).

## D.2 — Threats to validity: (i) the instrument floor binds per corpus and T3 is open at ×4 / ×8; (ii) the `tau` under-report and the two speed laws; (iii) the one-run points; (iv) the two-instrument disagreement on the source-rate term (P-026); (v) the `mv_guided` confound; (vi) sim-to-real

*serves:* ET · RE · N-R · *evidence:* `REGIME_TEST_MATRIX.md` §4, §9; `docs/LEARNING_LOG.md` P-026, P-029 · *status:* `EXISTS`

**(i) The instrument floor binds per corpus; T3 is open at ×4 / ×8.** "The gate had only ever been run on the
×1 corpus (P-029)." (REGIME_TEST_MATRIX.md §9). Under the gate-passing operator: "×1 and ×2 **PASS T1..T6**;
×4 and ×8 pass all but **T3**, whose residual on overlap-free pairs grows with displacement — 0.06 (×1),
0.11 (×2), 0.26 (×4), 0.57 px (×8) against a 0.15 px bar." (REGIME_TEST_MATRIX.md §9); "The bar was NOT
relaxed." (REGIME_TEST_MATRIX.md §9). So "a gate binds to the corpus it ran on", and "the gate's residual at
the corpus's displacement IS the floor under that row" (LEARNING_LOG.md P-029).

**(ii) The `tau` under-report and the two speed laws.** "The `tau` operator under-reported the FG's error at
high speed" (REGIME_TEST_MATRIX.md §9); re-scored, the fit becomes pos ≈ 0.191·disp^0.75 in place of
pos ≈ 0.30·disp^0.32 (REGIME_TEST_MATRIX.md §9), on rows that are "One seed, one run per point; the ×4 corpus
fails T3." (REGIME_TEST_MATRIX.md §9). Clause (a) quotes the older law and is not edited; "the finding stands
beside it as a post-freeze consequence for the operator's decision" (REGIME_TEST_MATRIX.md §9). §05.3 shows
both.

**(iii) The one-run points.** "the 0.5×, 2× and 4× points are single runs" (B1_SPEED_TEST.md §6); the
1.10–1.72× gap "is owed its second seed" (B1_SPEED_TEST.md §6); "the second-seed rows of the re-scored speed
law" stand under Not run (REGIME_TEST_MATRIX.md §9).

**(iv) Two instruments disagree on the source-rate term.** A marker record's method sentence was refuted by
its own generator: it "was already a matched-displacement comparison" — "the SAME comparison
`B1_SPEED_TEST.md` §3 made with the opposite answer (1.10–1.72×)" (LEARNING_LOG.md P-026). That 1.72× cell
"is a 0.126 px absolute gap (0.302 vs 0.176 px, n = 19 on one side, single seed), close to the capture-path
floor of 0.116–0.119 px" (REGIME_TEST_MATRIX.md §1). Until the discriminating family runs, "no source-rate
number is the project's answer" (REGIME_TEST_MATRIX.md §4).

**(v) The `mv_guided` confound.** Default-ON "and is the largest single term on the record — 1.668 px at
t ≈ 0.1 on the default vs 0.271 px flat across phase with `--no-mv-guided`, two runs per condition with the
run A / run B split printed" (REGIME_TEST_MATRIX.md §1). Every row above is at that default: "Any regime
measured only at the default is partly a measurement of that layer." (REGIME_TEST_MATRIX.md §1). The response
is both levels in every family (REGIME_TEST_MATRIX.md §4); the default is the operator's decision, untaken.

**(vi) Sim-to-real.** Every scene is rendered and transfer is unmeasured (§07.2): "progress on Sintel, KITTI
and Spring only weakly predicts accuracy on real-world data" (FG_METRIC_MODEL_PRIOR_ART.md Q3-7), and this
content "is closer to the synthetic side than that paper's targets, and that is an argument to make, not to
assume." (FG_METRIC_MODEL_PRIOR_ART.md §1 item 8).

## D.3 — The second instrument's readings (marker families 1, 2, 4, 6): corroboration beside the scene rows, never verdict rows

*serves:* SS (the boundary) · PO · *evidence:* the marker corpora exist; no live row · *status:* `CAPTURE`

*[empty slot]*

## D.4 — What this identity authorizes next and does not contain: the reference-free predictor (I-B) and the knob sweep that needs no model

*serves:* N-L · *evidence:* `REGIME_TEST_MATRIX.md` §3 · *status:* `EXISTS`

Two things are separated. Tuning the kernel's knobs against this instrument needs no learned model: "Offline
optimisation over the knobs against `scene_truth` — needs NO learned model and is buildable today", because
"When the label is the exact truth, the error is measured, not predicted." (REGIME_TEST_MATRIX.md §3). The
surface is "roughly 83 generation-affecting flags, ~27 of them continuous thresholds", a count called
"near-complete, not exhaustive" (REGIME_TEST_MATRIX.md §3); the limit is the screen, not the search: "one
candidate ≈ 25 s of exclusive screen, 100 candidates ≈ 40 min, two seeds double it" (REGIME_TEST_MATRIX.md
§3), which rules out black-box search over 27 dimensions. And "such an optimiser's first find is already
known and untaken — `--no-mv-guided`, 1.668 → 0.271 px at low phase" (REGIME_TEST_MATRIX.md §3), two runs per
condition (REGIME_TEST_MATRIX.md §1).

The reference-free predictor (I-B) is what this identity authorizes and does not contain: "Reference-free
feedback on real content — needs I-B and its generalisation; nothing is measured." (REGIME_TEST_MATRIX.md
§3). Three limits the same section states: the decision planes come from the CPU reference warp, so a
predictor trained on them predicts the reproduction's error, an identity that "is a gate result, not an
assumption"; one scene family and one kernel today; and any I-B claim "is scored against that flow-only
baseline or it claims a novelty it has not earned" (REGIME_TEST_MATRIX.md §3). Nothing of I-B is claimed here.

*Made with my soul - Swately <3*
