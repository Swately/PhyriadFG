# SCAFFOLD.md — the empty machine of the metric paper (KAP Phase 2)

Status: **`SCAFFOLD 2026-09-10`** — KAP Phase 2 output (`F:\Phyriad\protocols\analysis\KNOWLEDGE_ANALYSIS_PROTOCOL.md` §3.2:
"lay out the structure (sections, order, slots) without content"). Gate T2 (structural): **pending** — recorded below when it
runs. Input: the frozen identity `../../research/FG_METRIC_MODEL_SPINE.md`, block SHA-256 `427ac7489c5fdd7f15a34907e20313597256a0f52d0361824d193c29aa3314bf` (verified `INTACT` when
this scaffold was written). This document is the paper's **master plan**: the section order is its structural commitment,
frozen for Phase 3 (KAP §3.3: content may change "within each slot"; the order and the slots may not). The paper lives here
per `FORMAL_DOCUMENT_PROTOCOL.md` §3 (`docs/planning/paper/`) and follows its §9 template (Abstract · Introduction (gap →
contributions → what is NOT claimed) · Background/Related work · Method · Results · Discussion (+ threats to validity) ·
Limitations in-body · Conclusions · References · Reproducibility anchor).

**The operator paused Phase 2 on 2026-09-10 for the scenario testing, then (after the 5-hour limit) said to continue with
whatever of the KAP needs no testing data. This scaffold needs none: every slot names its evidence or what it waits on.**

## Structural decisions (Phase 2's only freedom: "structure / ordering")

1. **Files.** Markdown section files under `body/`, one per section, `NN_name.md`; `PAPER.md` is the master and lists the
   include order (the `\input` order of the worked example, `F:\Phyriad\phyriad-papers\disconnection_paper\paper_en.tex`).
   The project's documents are Markdown; a LaTeX master can be generated from the same order later. Reversible, his call.
2. **Order = the logical flow** (KAP §12: the structure serves the demonstration, not the numbering): gap → claim → what the
   literature holds → the instrument → the object and the protocol → the rows → what they separate and what threatens them →
   the limits → the conclusions → the references → how to reproduce. Here the logical order and the numeric order coincide
   by construction, so `PAPER.md` includes the files in numeric order.
3. **Two Method sections.** The instrument (§03) is separated from the object under test and the protocol (§04) because the
   identity's rulers and gate belong to the instrument and are valid across objects, while the builds, paths and A/B rule
   belong to the object. A reader who replaces the object keeps §03.
4. **Results follow the identity's own axes, then its four families.** §05.1–05.5 are the rows on `mixed` the identity
   already cites (canonical row, phase, source rate, displacement, the asynchronous path); §05.6–05.9 are F1–F4 in the
   identity's order; §05.10 holds the provenance readings whose claim altitude clause (a) bounds. No family beyond the nine
   the boundary allows has a slot.
5. **The second instrument gets no Results slot.** The marker chain (`tools/motion_truth/`) reports `err_model` / `absent` /
   `ghost` per class, not the six verdict terms, so a marker row cannot be a verdict row under the frozen identity. Marker
   families (matrix 1, 2, 4, 6) enter as corroborating readings in §06.3 and as method in §03.8. Consequence, stated here so
   it is decided and not discovered: **F2 and F4 need a scene family in the six terms** (a thin preset and `period_backdrop`;
   a base rendered at the low rate) — the identity already names F4's condition; F2's thin preset is a new build.
6. **Every Results slot carries N and r or the one-run label as a column, by construction** (the identity's Reliability
   paragraph); a slot that cannot carry them stays empty rather than filled with a single number.
7. **Threats to validity live in the Discussion, Limitations in-body** (FDP §9); the identity's NOT clauses map one-to-one
   onto §07.

## Identity elements (the legend every slot uses)

| code | element of the frozen block |
|---|---|
| `TH` | Thesis (one sentence) |
| `OB` | Object under test |
| `SS` | Scenario set (the boundary) |
| `PO` | Priority order of the terms |
| `RU` | Rulers |
| `RE` | Reliability |
| `ET` | Evidence tiers |
| `CA` | Claim (a): the terms separate phase / displacement / source rate |
| `CB` | Claim (b): the bounded absences in the swept literature |
| `N-Q` | NOT a quality metric |
| `N-R` | NOT real content |
| `N-L` | NOT a learned model (I-B authorized, not contained) |
| `N-D` | NOT a distribution metric (I-C) |
| `N-G` | NOT a claim about another generator |

## Slot status vocabulary

| status | meaning |
|---|---|
| `EXISTS` | the evidence is on record today; Phase 3 may fill the slot now |
| `CAPTURE` | the slot waits on a live capture (the operator's screen; `REGIME_TEST_MATRIX.md` §8) |
| `BUILD` | the slot waits on a scene family that does not exist yet (the operator's instrument; announced, not built) |
| `DECISION` | the slot's content depends on a decision that is the operator's (`REGIME_TEST_MATRIX.md` §7, KAP §7) |
| `AFTER` | filled last, from the filled Results (never before them) |

## Section order and slots (11 sections, 51 slots — 41 `EXISTS`, 5 `CAPTURE`, 2 `BUILD`, 1 `DECISION`, 2 `AFTER`)

### `body/00_abstract.md` — Abstract

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| A.1 | The thesis sentence, the headline rows with their N and r (or the one-run label), what is NOT claimed | TH · CA · CB · N-* | written from §05 once its rows are filled; today §05.1–05.5 hold rows, §05.6–05.10 are empty | `AFTER` |

### `body/01_introduction.md` — Introduction

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| I.1 | The gap: what the swept literature does not hold (N1–N3, stated as time-boxed absences, not proofs) | CB | `docs/research/FG_METRIC_MODEL_PRIOR_ART.md` §4 | `EXISTS` |
| I.2 | The claim: the thesis sentence, verbatim from the frozen block | TH | `docs/research/FG_METRIC_MODEL_SPINE.md` identity block | `EXISTS` |
| I.3 | The object and the boundary in one paragraph (kernel, builds, paths; the families; what today carries rows) | OB · SS | the identity block; `docs/evidence/B1_FIRST_FG_ROW.md` header; `docs/planning/records/GDUMP_GATE.md` | `EXISTS` |
| I.4 | Contributions, enumerated, each pointing at the section that carries it; the regime contributions stated as CONDITIONAL until their §05 slot is filled | CA · CB · ET | the identity block; §05 slot states | `EXISTS` |
| I.5 | What is NOT claimed (the block's last paragraph, in the paper's words, nothing added) | N-Q · N-R · N-L · N-D · N-G | the identity block | `EXISTS` |

### `body/02_related_work.md` — Background and related work

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| R.1 | Distribution metrics (FID / CMMD / IS class) and why they do not measure fidelity here | N-D | `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q1 (rows Q1-9, Q1-10) | `EXISTS` |
| R.2 | Full-reference perceptual metrics and the VFI benchmarks; their pixel-scale sensitivity as the table states it (Q2-4, Q2-12; absence N6) | N-Q | `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q2 | `EXISTS` |
| R.3 | Synthetic / analytic ground truth and the sim-to-real objection (Q3-7) | N-R | `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q3 | `EXISTS` |
| R.4 | Reference-free error prediction and self-diagnosis (abstract-level rows) | N-L | `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q4 | `EXISTS` |
| R.5 | Error decomposition, "hallucination", and reliability practice | PO · RE | `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q5 | `EXISTS` |
| R.6 | The bounded absences N1–N7, each with its search box and its date | CB | `FG_METRIC_MODEL_PRIOR_ART.md` §4 | `EXISTS` |

### `body/03_instrument.md` — Method I — the instrument

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| M1.1 | The analytic scene: closed-form pose(t), shapes / textures / motion, the barcoded base index, the presets (the five in the boundary; `static` as the control; `fast_train` as F1's candidate, his call) | SS | `tools/scene_truth/scene_zoo.py`; `B1_FIRST_FG_ROW.md` §1 | `EXISTS` |
| M1.2 | Exact-phase alignment: reading the generator's own phase, the k-apart rule, cut pairs filed apart | TH · SS (F3 clause) | `tools/scene_truth/scene_align.py`; `B1_FIRST_FG_ROW.md` §2, §4 | `EXISTS` |
| M1.3 | The terms: the six verdict terms and the three reported apart, each definition quoted from `scene_report.py` | PO | `tools/scene_truth/scene_report.py` docstring and `verdict()` | `EXISTS` |
| M1.4 | The verdict: conjunctive, per object; the thresholds; the displacement floor (an object below it carries no claim) | PO · RU | `scene_report.py` `verdict()`; `B1_FIRST_FG_ROW.md` §3 (k = 2) | `EXISTS` |
| M1.5 | The rulers: `truth` / `nearest` / `oracle2` scored by the same code on the same corpus; `blend` and `blur` from the synthetic sweep | RU | `B1_FIRST_FG_ROW.md` §2, §3; `B1_SWEEP_seed7.md` / `_seed11.md` | `EXISTS` |
| M1.6 | The instrument's own gate (T1–T6): what each tests; the two silhouette operators (`tau`, `coverage`) and the occlusion-aware window; the border-clip rule; the gate state per corpus — a gate binds to the corpus it ran on (T3 open at ×4 / ×8, bar not relaxed) | ET · RE | `REGIME_TEST_MATRIX.md` §9; `docs/planning/records/S2_T*_GATE.md`; `docs/LEARNING_LOG.md` P-029 | `EXISTS` |
| M1.7 | Cut scoring: a cut frame against BOTH real endpoints on the terms that survive (halluc, sharp, graceful); the `hold` and `blend` references; gate T6 | SS (F3 clause) | `REGIME_TEST_MATRIX.md` §9 (cut scoring); `C:\PhyriadFG\runs\sc_live2\cuts_k4.md` | `EXISTS` |
| M1.8 | The second instrument (`tools/motion_truth/`, marker classes / sizes / backdrops / `--fps`, its capture floor, the `ambig` exemption): used for CORROBORATING readings only, because its terms are not the six verdict terms | SS · PO | `docs/evidence/MOTION_TRUTH_BASELINE.md`; `M1_SRC_RATE.md`; `REGIME_TEST_MATRIX.md` §2 | `EXISTS` |

### `body/04_object_and_protocol.md` — Method II — the object under test and the protocol

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| M2.1 | The object: `fg_core.comp` under its default flags; the two present paths (`--qdump` sampler, `--gdump` every-tick tap); the build each row names | OB | the identity block; `B1_FIRST_FG_ROW.md` and `B1_SPEED_TEST.md` headers; `GDUMP_GATE.md` G0 | `EXISTS` |
| M2.2 | Sources: 640×360 from a 240 fps analytic base decimated by k; the canonical row k = 4 on `mixed`; where k ∈ {2, 8, 16} and 1080p enter | OB | the identity block; `B1_FIRST_FG_ROW.md` §3 | `EXISTS` |
| M2.3 | The A/B rule: every knob change against the default, never a redefinition; the `mv_guided` level stated in every record | OB | the identity block; `REGIME_TEST_MATRIX.md` §4 | `EXISTS` |
| M2.4 | The reliability protocol: two corpus seeds, run-to-run r beside every number, the 20 % rule, the one-run label | RE | `B1_FIRST_FG_ROW.md` §2b; `scene_report.py` (the printed rule) | `EXISTS` |
| M2.5 | Family entry: the pre-registered failure signature of each family, quoted from the matrix as written BEFORE its first row; the red-first check (the family's gate seen red on a synthetic arm) quoted per corpus | SS | `REGIME_TEST_MATRIX.md` §2 (signatures); §9 (gate logs) — the per-family red-first line is quoted at fill time | `EXISTS` |
| M2.6 | The capture procedure: one command per arm, dry-run discipline, KEEP corpora, the scoring commands (`--jobs`, `--silhouette`) | ET | `REGIME_TEST_MATRIX.md` §8 | `EXISTS` |

### `body/05_results.md` — Results

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| 5.1 | The canonical row: k = 4 on `mixed`, two seeds, with its three rulers and `blend` from the sweep | CA · RU · RE | `B1_FIRST_FG_ROW.md` §2, §2b (0.216 / 0.212 px) | `EXISTS` |
| 5.2 | The phase signature: error falling with phase on the fastest mover (M1_LOWPHASE reproduced by an instrument that did not know it) | CA (phase) | `B1_FIRST_FG_ROW.md` §2; `docs/planning/records/M1_LOWPHASE_FINDING.md` | `EXISTS` |
| 5.3 | The source-rate axis on `mixed`: k = 2 / 4 / 8 / 16 with `blend` per k; the k-path vs speed-path term (one run, second seed owed) | CA (source rate) · SS (F4 clause) | `B1_FIRST_FG_ROW.md` §3; `B1_SPEED_TEST.md` §3 | `EXISTS` |
| 5.4 | The displacement axis on `mixed`: the recorded rows (`tau`) AND the re-scored rows (`coverage`), one run each, with the gate state of each corpus; both fitted laws shown; the identity's clause (a) stands as written until the operator re-opens Phase 1 | CA (displacement) | `B1_SPEED_TEST.md` §2–§3; `REGIME_TEST_MATRIX.md` §9 (re-scored table); SPINE post-freeze note | `DECISION` |
| 5.5 | The asynchronous path: its one row, and its second seed | OB (async path) · RE | `GDUMP_GATE.md` G5 (0.240 px, one run) EXISTS; the DI-3 second run is family 0's `-Gdump` capture | `CAPTURE` |
| 5.6 | F1 — extremely fast or erratic motion: `speed_extend` ×8 / ×16 on `fast_train` (corpora rendered, gated); the erratic component has no scene family yet (the marker `reverse` class is corroboration, §06.3) | SS (F1) | families 3 and 6 of the matrix; the ×8 / ×16 corpora exist on `C:\PhyriadFG\runs\`; no FG row | `CAPTURE` |
| 5.7 | F2 — thin, repetitive or complex-patterned objects: needs a scene family in the six terms (a thin preset; `period_backdrop`); the marker families `thin_size` / `grating_pan` are corroboration, §06.3 | SS (F2) | family 7 TO BUILD; a thin scene preset TO BUILD; no FG row | `BUILD` |
| 5.8 | F3 — abrupt scene changes: the cut frames of the looped corpus against both endpoints, the references, per phase bin; the every-tick capture and its no-loop null | SS (F3) | `sc_live2` cuts (9 cuts, one capture) EXISTS; family 5's `-Gdump` capture + null | `CAPTURE` |
| 5.9 | F4 — low source frame rate beside the k sweep: a base rendered at the low rate with a display-rate multiple; the `--asw` path; the marker `src_rate_matched` family is corroboration, §06.3 | SS (F4) | the k sweep (5.3) EXISTS as the floor; the low-rate scene base TO BUILD (the identity's own F4 condition); no FG row | `BUILD` |
| 5.10 | The provenance readings (family 0: `--no-stasis`, `--mv-sim`): reported as READINGS; an attribution claim would exceed clause (a) and needs Phase 1 re-opened | CA (its bound) | family 0 of the matrix; `B1_SPEED_TEST.md` §4 holds today's readings | `CAPTURE` |

### `body/06_discussion.md` — Discussion

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| D.1 | What the terms separate, as measured: phase, displacement, source rate; the lead sign as a reading | CA | §05.1–05.4 today; §05.6–05.9 when filled | `EXISTS` |
| D.2 | Threats to validity: (i) the instrument floor binds per corpus and T3 is open at ×4 / ×8; (ii) the `tau` under-report and the two speed laws; (iii) the one-run points; (iv) the two-instrument disagreement on the source-rate term (P-026); (v) the `mv_guided` confound; (vi) sim-to-real | ET · RE · N-R | `REGIME_TEST_MATRIX.md` §4, §9; `docs/LEARNING_LOG.md` P-026, P-029 | `EXISTS` |
| D.3 | The second instrument's readings (marker families 1, 2, 4, 6): corroboration beside the scene rows, never verdict rows | SS (the boundary) · PO | the marker corpora exist; no live row | `CAPTURE` |
| D.4 | What this identity authorizes next and does not contain: the reference-free predictor (I-B) and the knob sweep that needs no model | N-L | `REGIME_TEST_MATRIX.md` §3 | `EXISTS` |

### `body/07_limitations.md` — Limitations

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| L.1 | Not a quality metric: no MOS, no perceptual claim, no measured comparison with LPIPS / FloLPIPS / PSNR_DIV | N-Q | the identity block; `FG_METRIC_MODEL_PRIOR_ART.md` Q2-4, Q2-12, N6 | `EXISTS` |
| L.2 | Not real content: transfer unmeasured, stated beside every number | N-R | the identity block; Q3-7 | `EXISTS` |
| L.3 | Not a learned model; not a distribution metric; not a claim about another generator | N-L · N-D · N-G | the identity block | `EXISTS` |
| L.4 | What is one run, what is owed, what is unmeasured (the list, with the record that owes each) | RE · ET | `B1_FIRST_FG_ROW.md` §5; `B1_SPEED_TEST.md` §6; `REGIME_TEST_MATRIX.md` §9 (not run) | `EXISTS` |

### `body/08_conclusions.md` — Conclusions

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| C.1 | What was measured, at what reliability, and what the boundary leaves outside | TH · CA · CB | written from §05 and §06 once filled | `AFTER` |

### `body/09_references.md` — References

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| Ref.1 | Prior art, each entry at the verification level the Phase 0 table assigns ([V1]/[V2]/[V3]); nothing [V3] cited as fact | ET · CB | `FG_METRIC_MODEL_PRIOR_ART.md` §2 (41 sources) | `EXISTS` |
| Ref.2 | The project's own records ([V1]): each evidence file and gate record cited by path and commit | ET | `docs/evidence/`, `docs/planning/records/`, `docs/research/` | `EXISTS` |

### `body/10_reproducibility.md` — Reproducibility anchor

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| RA.1 | Code: the repository, the tool files, the kernel builds each row names (`f2e4a9a`, `0df0332` + md5) | ET · OB | the identity block; `GDUMP_GATE.md` G0 | `EXISTS` |
| RA.2 | Seeds and corpora: the seeds per row, the corpus manifests and their paths, what is KEEP | RE · ET | `C:\PhyriadFG\runs\*` manifests; `REGIME_TEST_MATRIX.md` §9 | `EXISTS` |
| RA.3 | Commands: render, capture, align, score (with `--jobs` and `--silhouette`), gate — one per arm | ET | `REGIME_TEST_MATRIX.md` §8 | `EXISTS` |
| RA.4 | Hardware: the GPU and CPU per record — the B1 records name no GPU (to be quoted from the rig before the anchor is filled; `M1_SRC_RATE.md` line 3 names the RTX 4090) | ET | `docs/evidence/M1_SRC_RATE.md`; `docs/planning/records/QOL_FINDINGS.md` lines 136–143 | `EXISTS` |

## What Phase 3 may and may not do

- **May:** fill any `EXISTS` slot with content that serves its named element, necessity-only (KAP §3.3), every number
  quoted from the record it names with its N and r or its one-run label; leave every other slot as the marker it is.
- **May not:** add, remove, merge or reorder sections or slots (that is Phase 4, gated by T2 again); touch the identity
  block; state a `CAPTURE` / `BUILD` slot's result before its capture; present the coverage-refitted speed law as the
  identity's clause (a) — both laws are shown in §05.4 with the operator named, until he re-opens Phase 1.
- **Fill order:** §01, §02, §03, §04, §07, §09, §10 have every slot `EXISTS`; §05.1–05.4, §06.1, §06.2, §06.4 next; §00,
  §08 last.

## Gate T2 (structural conformance) — record

*Pending.* When it runs: template `KNOWLEDGE_ANALYSIS_AUDITOR_PROMPTS.md` §7 verbatim, clean-context auditors, `{{ARTIFACT}}` =
`PAPER.md` + `body/*.md`, `{{MASTER_PLAN}}` = this file, `{{HIERARCHY}}` = the section order above, `{{FROZEN_IDENTITY}}` = the
block; verdict, per-question breakdown and the supervisor's first-hand check of each finding recorded here (KAP §9.2).

## Not done

- No content was written into any slot (Phase 2 writes none). No LaTeX master, no bibliography file, no compile.
- The identity block was not touched (hash verified before writing).

*Made with my soul - Swately <3*
