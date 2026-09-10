# 3. Method I — the instrument

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 4 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## M1.1 — The analytic scene: closed-form pose(t), shapes / textures / motion, the barcoded base index, the presets (the five in the boundary; `static` as the control; `fast_train` as F1's candidate, his call)

*serves:* SS · *evidence:* `tools/scene_truth/scene_zoo.py`; `B1_FIRST_FG_ROW.md` §1 · *status:* `EXISTS`

*[empty slot]*

## M1.2 — Exact-phase alignment: reading the generator's own phase, the k-apart rule, cut pairs filed apart

*serves:* TH · SS (F3 clause) · *evidence:* `tools/scene_truth/scene_align.py`; `B1_FIRST_FG_ROW.md` §2, §4 · *status:* `EXISTS`

*[empty slot]*

## M1.3 — The terms: the six verdict terms and the three reported apart, each definition quoted from `scene_report.py`

*serves:* PO · *evidence:* `tools/scene_truth/scene_report.py` docstring and `verdict()` · *status:* `EXISTS`

*[empty slot]*

## M1.4 — The verdict: conjunctive, per object; the thresholds; the displacement floor (an object below it carries no claim)

*serves:* PO · RU · *evidence:* `scene_report.py` `verdict()`; `B1_FIRST_FG_ROW.md` §3 (k = 2) · *status:* `EXISTS`

*[empty slot]*

## M1.5 — The rulers: `truth` / `nearest` / `oracle2` scored by the same code on the same corpus; `blend` and `blur` from the synthetic sweep

*serves:* RU · *evidence:* `B1_FIRST_FG_ROW.md` §2, §3; `B1_SWEEP_seed7.md` / `_seed11.md` · *status:* `EXISTS`

*[empty slot]*

## M1.6 — The instrument's own gate (T1–T6): what each tests; the two silhouette operators (`tau`, `coverage`) and the occlusion-aware window; the border-clip rule; the gate state per corpus — a gate binds to the corpus it ran on (T3 open at ×4 / ×8, bar not relaxed)

*serves:* ET · RE · *evidence:* `REGIME_TEST_MATRIX.md` §9; `docs/planning/records/S2_T*_GATE.md`; `docs/LEARNING_LOG.md` P-029 · *status:* `EXISTS`

*[empty slot]*

## M1.7 — Cut scoring: a cut frame against BOTH real endpoints on the terms that survive (halluc, sharp, graceful); the `hold` and `blend` references; gate T6

*serves:* SS (F3 clause) · *evidence:* `REGIME_TEST_MATRIX.md` §9 (cut scoring); `C:\PhyriadFG\runs\sc_live2\cuts_k4.md` · *status:* `EXISTS`

*[empty slot]*

## M1.8 — The second instrument (`tools/motion_truth/`, marker classes / sizes / backdrops / `--fps`, its capture floor, the `ambig` exemption): used for CORROBORATING readings only, because its terms are not the six verdict terms

*serves:* SS · PO · *evidence:* `docs/evidence/MOTION_TRUTH_BASELINE.md`; `M1_SRC_RATE.md`; `REGIME_TEST_MATRIX.md` §2 · *status:* `EXISTS`

*[empty slot]*

*Made with my soul - Swately <3*
