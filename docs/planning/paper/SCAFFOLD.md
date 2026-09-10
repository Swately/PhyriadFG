# SCAFFOLD.md — the empty machine of the metric paper (KAP Phase 2)

Status: **`SCAFFOLD 2026-09-10 — T2 APPROVE WITH WARNINGS (two rounds, every warning dispositioned; record below) · PHASE 3: 42 of 55 slots filled, the data-free ones (record below)`** — KAP
Phase 2 output (`F:\Phyriad\protocols\analysis\KNOWLEDGE_ANALYSIS_PROTOCOL.md` §3.2: "lay out the structure (sections, order,
slots) without content"). Input: the frozen identity `../../research/FG_METRIC_MODEL_SPINE.md`, block SHA-256 `427ac7489c5fdd7f15a34907e20313597256a0f52d0361824d193c29aa3314bf` (verified `INTACT` when
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
   already cites, its axes in clause (a)'s own enumeration — phase (§05.2), displacement (§05.3), source rate (§05.4), so
   that the source-rate term reads back against the displacement axis it is compared with — then the asynchronous path
   (§05.5); §05.6 holds the four built presets the boundary already contains and no row has visited (`translate`,
   `occlude`, `spin`, `cross` — inside the boundary, so they have a slot, entering a claim only once measured); §05.7–05.11
   are F1–F4 in the identity's order, F1 split into its fast and its erratic component because the two are of unequal
   readiness (one has corpora, the other has no scene family) and a slot carries one status; §05.12 holds the provenance
   readings whose claim altitude clause (a) bounds. Every family the boundary names has a slot; no family beyond the nine
   has one.
5. **The second instrument gets no Results slot.** The marker chain (`tools/motion_truth/`) reports `err_model` / `absent` /
   `ghost` per class, not the six verdict terms, so a marker row cannot be a verdict row under the frozen identity. Marker
   families (matrix 1, 2, 4, 6) enter as corroborating readings in §06.3 and as method in §03.8. Consequence, stated here so
   it is decided and not discovered: **F2 and F4 need a scene family in the six terms** (a thin preset and `period_backdrop`;
   a base rendered at the low rate) — the identity already names F4's condition; F2's thin preset is a new build.
6. **Every Results slot carries N and r or the one-run label as a column, by construction** (the identity's Reliability
   paragraph); a slot that cannot carry them stays empty rather than filled with a single number. **Every Results slot
   that carries an FG row carries its rulers by the same construction** — `truth` / `nearest` / `oracle2` scored on the same
   corpus at the same k, `blend` from the sweep — because "a row without its rulers is not a result" (the identity's Rulers
   paragraph); such slots declare `RU`, and a row whose rulers were not scored is not written.
7. **Threats to validity live in the Discussion, Limitations in-body** (FDP §9); the identity's NOT clauses map one-to-one
   onto §07.
8. **The master's front matter is two slots of its own** (title, authorship), both filled last: a title is a consequence of
   the filled body, and the CRediT roles (FDP §10) are the operator's to state. They are inventoried below with the body slots.

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
| `DECISION` | filled NOW in the two-sided form the slot prescribes (both readings shown, the operator named as the one who decides); its final one-sided form waits on a decision that is the operator's (`REGIME_TEST_MATRIX.md` §7, KAP §7) |
| `AFTER` | filled last, from the filled Results (never before them) |

## Section order and slots (11 sections, 53 body slots — 41 `EXISTS`, 6 `CAPTURE`, 3 `BUILD`, 1 `DECISION`, 2 `AFTER`; plus the master's 2 front-matter slots)

### `PAPER.md` — the master (front matter)

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| F.1 | Title: a noun phrase drawn from the thesis sentence, claiming nothing the identity does not | TH | written from the filled body | `AFTER` |
| F.2 | Authorship: the operator (Swately) and the LLM sessions, with CRediT roles (FDP §10) as the operator states them | ET | the operator's statement; the reproducibility anchor names the sessions' tools | `AFTER` |

### `body/00_abstract.md` — Abstract

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| A.1 | The thesis sentence, the headline rows with their N and r (or the one-run label), what is NOT claimed | TH · CA · CB · N-* | written from §05 once its rows are filled; today §05.1–05.5 hold rows, §05.6–05.12 are empty | `AFTER` |

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
| M1.6 | The instrument's own gate (T1–T6): what each tests; the two silhouette operators (`tau`, `coverage`) and the occlusion-aware window; the border-clip rule; the gate state per corpus — a gate binds to the corpus it ran on (T3 open at ×4 / ×8, bar not relaxed) | ET · RE | `scene_report.py` (the gate: its docstring and `gate()`); `REGIME_TEST_MATRIX.md` §9; the gate logs under `C:\PhyriadFG\runs\_render_logs\`; `docs/LEARNING_LOG.md` P-029 | `EXISTS` |
| M1.7 | Cut scoring: a cut frame against BOTH real endpoints on the terms that survive (halluc, sharp, graceful); the `hold` and `blend` references; gate T6 | SS (F3 clause) | `REGIME_TEST_MATRIX.md` §9 (cut scoring); `C:\PhyriadFG\runs\sc_live2\cuts_k4.md` | `EXISTS` |
| M1.8 | The second instrument (`tools/motion_truth/`, marker classes / sizes / backdrops / `--fps`, its capture floor, the `ambig` exemption): used for CORROBORATING readings only, because its terms are not the six verdict terms | SS · PO | `docs/evidence/MOTION_TRUTH_BASELINE.md`; `M1_SRC_RATE.md`; its own gate records `docs/planning/records/S2_T2_GATE.md` … `S2_T6_GATE.md`; `REGIME_TEST_MATRIX.md` §2 | `EXISTS` |

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
| 5.3 | The displacement axis on `mixed`: the recorded rows (`tau`) AND the re-scored rows (`coverage`), one run each, with the gate state of each corpus; both fitted laws shown; the identity's clause (a) stands as written until the operator re-opens Phase 1 | CA (displacement) · RU | `B1_SPEED_TEST.md` §2–§3; `REGIME_TEST_MATRIX.md` §9 (re-scored table); SPINE post-freeze note | `DECISION` |
| 5.4 | The source-rate axis on `mixed`: k = 2 / 4 / 8 / 16 with `blend` per k; the k-path vs speed-path term read against §05.3 at matched displacement and phase (one run, second seed owed) | CA (source rate) · SS (F4 clause) · RU | `B1_FIRST_FG_ROW.md` §3; `B1_SPEED_TEST.md` §3 | `EXISTS` |
| 5.5 | The asynchronous path: its one row, and its second seed | OB (async path) · RE · RU | `GDUMP_GATE.md` G5 (0.240 px, one run) EXISTS; the DI-3 second run is family 0's `-Gdump` capture | `CAPTURE` |
| 5.6 | The four built presets inside the boundary that carry no FG row — `translate`, `occlude`, `spin`, `cross`: a row each with its rulers, entering a claim only once measured; their predicted signatures are written in §04.5 before the first capture (the matrix pre-registers none of them) | SS (the built presets) · RU | the presets exist in `scene_zoo.py`; no corpus rendered for them, no signature written, no FG row | `CAPTURE` |
| 5.7 | F1, fast — extremely fast motion: `speed_extend` ×8 / ×16 on `fast_train` (corpora rendered, gated), read with the gate state at each speed | SS (F1) · RU | family 3 of the matrix; the ×8 / ×16 corpora exist on `C:\PhyriadFG\runs\`; no FG row | `CAPTURE` |
| 5.8 | F1, erratic — reversing or erratic motion: needs a scene family in the six terms (a closed-form reversing preset); the marker `reverse` class is corroboration, §06.3 | SS (F1) · RU | family 6 of the matrix is a marker family; the scene preset TO BUILD; no FG row | `BUILD` |
| 5.9 | F2 — thin, repetitive or complex-patterned objects: needs a scene family in the six terms (a thin preset; `period_backdrop`); the marker families `thin_size` / `grating_pan` are corroboration, §06.3 | SS (F2) · RU | family 7 TO BUILD; a thin scene preset TO BUILD; no FG row | `BUILD` |
| 5.10 | F3 — abrupt scene changes: the cut frames of the looped corpus against both endpoints, the references, per phase bin; the every-tick capture and its no-loop null | SS (F3) · RU | `sc_live2` cuts (9 cuts, one capture) EXISTS; family 5's `-Gdump` capture + null | `CAPTURE` |
| 5.11 | F4 — low source frame rate beside the k sweep: a base rendered at the low rate with a display-rate multiple; the `--asw` path; the marker `src_rate_matched` family is corroboration, §06.3 | SS (F4) · RU | the k sweep (§05.4) EXISTS as the floor; the low-rate scene base TO BUILD (the identity's own F4 condition); no FG row | `BUILD` |
| 5.12 | The provenance readings (family 0: `--no-stasis`, `--mv-sim`): reported as READINGS; an attribution claim would exceed clause (a) and needs Phase 1 re-opened | CA (its bound) · RU | family 0 of the matrix; `B1_SPEED_TEST.md` §4 holds today's readings | `CAPTURE` |

### `body/06_discussion.md` — Discussion

| slot | holds | serves | evidence now / waits on | status |
|---|---|---|---|---|
| D.1 | What the terms separate, as measured: phase, displacement, source rate; the lead sign as a reading | CA | §05.1–05.4 today (§05.3 in its two-sided form); §05.6–05.12 when filled | `EXISTS` |
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

- **May:** fill any `EXISTS` slot, and the `DECISION` slot in its prescribed two-sided form, with content that serves its
  named element, necessity-only (KAP §3.3), every number quoted from the record it names with its N and r or its one-run
  label; leave every `CAPTURE`, `BUILD` and `AFTER` slot as the marker it is.
- **May not:** add, remove, merge or reorder sections or slots (that is Phase 4, gated by T2 again); touch the identity
  block; state a `CAPTURE` / `BUILD` slot's result before its capture; present the coverage-refitted speed law as the
  identity's clause (a) — both laws are shown in §05.3 with the operator named, until he re-opens Phase 1.
- **Fill order:** §01, §02, §03, §04, §07, §09, §10 have every slot `EXISTS`; §05.1, §05.2, §05.4 (`EXISTS`) and §05.3
  (`DECISION`, two-sided), then §06.1, §06.2, §06.4; §00, §08 and the master's F.1, F.2 last.

## Gate T2 (structural conformance) — record

Template `KNOWLEDGE_ANALYSIS_AUDITOR_PROMPTS.md` §7 verbatim, mode-lock; `{{ARTIFACT}}` = `PAPER.md` + `body/*.md`,
`{{MASTER_PLAN}}` = this file, `{{HIERARCHY}}` = the include order + the slot tables, `{{FROZEN_IDENTITY}}` = the block,
`{{SCOPE}}` = the structure only; each auditor a clean-context Opus subagent permitted to open the paper directory and the
SPINE, nothing else. A subordinate's finding is a claim: each is checked first-hand before its disposition (KAP §9.2).

**Round 1 (2026-09-10, on the scaffold as first written: 11 sections, 51 body slots).** Two auditors, both
`APPROVE WITH WARNINGS` — auditor 1: Q1 MINOR, Q2–Q5 PASS (154,090 tokens, 296 s); auditor 2: Q1 MINOR, Q3 MINOR, Q2 / Q4 / Q5
PASS (160,219 tokens, 308 s). Both machine-diffed the 51 body slots against this plan's tables and found 0 mismatches on all
five fields; both found every legend code served and no code outside the legend. The strictest verdict binds.

| # | warning (auditor) | first-hand check | disposition |
|---|---|---|---|
| 1 | `PAPER.md` carried a Title "slot" and an Authorship block outside the 51-slot inventory; the Authorship line held prose against "no content written" (1, 2) | true: `PAPER.md` lines 6–9 as first written | two front-matter slots `F.1` / `F.2` (`AFTER`), decision 8, inventoried in the tables; the prose removed, the master carries the markers |
| 2 | §05 ordered the axes phase → source rate → displacement while clause (a) enumerates "phase, displacement, source rate"; the source-rate slot's k-path-vs-speed-path term pointed forward to the displacement axis (1) | true: the block's clause (a); the old 5.3 compared against the old 5.4's speed path | 5.3 and 5.4 swapped (displacement, then source rate reading back against it); decision 4 states the enumeration |
| 3 | the F1 slot carried two conditions of unequal readiness under one status — corpora rendered for the fast component, no scene family for the erratic one (1) | true: the slot's own text | F1 split: 5.7 fast (`CAPTURE`), 5.8 erratic (`BUILD`); decision 4 |
| 4 | the four built presets inside the boundary (`translate`, `occlude`, `spin`, `cross`) had no Results slot while the four unbuilt regimes did (2) | true: the block names the five built presets inside the boundary; only `mixed` had slots | new 5.6 (`CAPTURE`): a row each once measured, their signatures owed in §04.5 (the matrix pre-registers none of them) |
| 5 | 5.4 was `DECISION` ("waits on a decision") yet listed in the fill order and named as evidence by the `EXISTS` slot D.1 (2) | true: the old vocabulary line, the fill order, D.1's evidence | `DECISION` redefined: filled now in its two-sided form, its one-sided form waits on the operator; the "May" and fill-order lines rewritten; D.1 names the form |
| 6 | *(supervisor, not an auditor)* M1.6 cited `docs/planning/records/S2_T*_GATE.md` as the scene scorer's gate records | false citation: those are the marker chain's gates (`S2_T2_GATE.md` "the ground-truth marker zoo", `S2_T3` "the player", `S2_T4` "the extractor", `S2_T5` "the report", `S2_T6` "the CPU reference warp"); the scene gate lives in `scene_report.py` `gate()` and its logs under `C:\PhyriadFG\runs\_render_logs\` | M1.6's evidence corrected; M1.8 gains the S2 records |

Nothing was rejected. Auditor 2 considered and cleared §03-before-§04 (the block lists the object second) with decision 3's
reason. Because fixes 2–4 change the structure the round-1 verdict was given on, that verdict is void for the corrected
scaffold (CONDUCT: a gate binds to the exact claim it tested) — hence round 2.

**Round 2 (the corrected scaffold: 11 sections, 53 body slots + 2 front-matter slots).** One auditor, `APPROVE WITH WARNINGS`
— Q1 PASS, Q2 MINOR, Q3 MINOR, Q4 PASS, Q5 PASS (183,552 tokens, 412 s). Verified first-hand by it: 55 slots against the 55
inventory rows, 0 mismatches on all five fields; per-file counts agree three ways; one H1 per file, slots at H2, nothing
deeper; one empty marker per slot; the identity block re-hashed to the declared SHA-256 (`INTACT`); every internal
cross-reference resolves; nine families and no tenth (`static` method-only, `fast_train` inside F1).

| # | warning | first-hand check | disposition |
|---|---|---|---|
| 7 | the "May not" rule still said the two speed laws are shown in "§05.4" — the pre-swap number; the slot is 5.3 | true: one line survived round 1's disposition 2 | corrected to §05.3 |
| 8 | Reliability is guaranteed at every Results slot by construction (decision 6) but Rulers is not, although the identity says "a row without its rulers is not a result"; `RU` was declared by 5.1 only | true: the block's Rulers paragraph; the serves column | decision 6 extended: every row-bearing Results slot carries its rulers by construction and declares `RU` (5.3–5.12; 5.2 reads 5.1's row) |

Both fixes are point edits (a section number; a code in ten `serves` cells and one sentence) that change no section,
order or slot; applied and re-checked mechanically (slot parity, path resolution, the block's hash), not re-gated —
the T1 precedent for bounded point-edits. Cleared by the auditor without a flag: §03 before §04 (decision 3); §05's
order against the block's preset enumeration (decision 4); `CAPTURE` slots whose evidence names a row already on record
(their text separates what exists from what is owed); the front-matter slots as bold markers, not H2s. **Gate T2 closed:**
`APPROVE WITH WARNINGS`, every warning dispositioned. Phase 3 is open on the slots the fill order names.

## Phase 3 record (content pass, 2026-09-10 — the slots that need no testing data)

*From this section on the paper directory is edited by hand; the Phase 2 generator (`paper_scaffold.py`, the session's
scratchpad) is no longer run, because it would overwrite the drafted bodies.*

**Filled: 42 of the 55 slots — every `EXISTS` slot and the `DECISION` slot in its two-sided form.** Empty, by the
scaffold's own rule: §05.5–05.12 (`CAPTURE` / `BUILD`), §06.3 (`CAPTURE`), §00, §08, F.1, F.2 (`AFTER`). Nine drafters,
one per section, each in a clean context with one brief (its file, its slots, its permitted sources and nothing else,
eight binding rules: every number anchored to its source, reliability as the source states it, deflationary register,
the identity binding, necessity-only) — Opus for §01, §03, §05, §06, §07; Sonnet for §02, §04, §09, §10 (the model chosen
per task, R5). Sizes: §01 998 words, §02 1,369, §03 2,071, §04 1,213, §05 1,690, §06 1,100, §07 522, §09 1,872, §10 621
— 11,456 words; 1,497,313 subordinate tokens in all (120,598–273,417 per drafter; 390–1,404 s each).

**Verification (a drafter's output is a claim).** Mechanical, every section: headings and slot metadata byte-identical
to the committed stubs; only the brief's slots filled; every number in the filled text present in at least one
permitted source of that section (1,230 numbers checked; the only misses were code line numbers, each then read);
inflation lexicon absent. First-hand, every section: the quoted sentences and tables re-read against
`FG_METRIC_MODEL_PRIOR_ART.md` §1/§2/§4/§7 (and the [V] level of each cited row), `B1_FIRST_FG_ROW.md` §1–§5,
`B1_SPEED_TEST.md` §2–§6, `GDUMP_GATE.md` header/G0/G5, `M1_LOWPHASE_FINDING.md` line 13, `M1_SRC_RATE.md` lines 3/29,
`MOTION_TRUTH_BASELINE.md`, `REGIME_TEST_MATRIX.md` §1–§4/§8/§9, `LEARNING_LOG.md` P-026/P-029, `QOL_FINDINGS.md`
133–145, the 37 `scene_zoo.py` / `scene_align.py` / `scene_report.py` lines cited by number, the ten `tau` and four
`coverage` gate logs, `sc_live2\cuts_k4.md`, the 14 per-file commit hashes of §09, and the rig (`nvidia-smi -L`,
`Get-CimInstance`). Supervisor edits after the reads: RA.3's scoring command quoted from `scene_report.py:769-782`
(the drafter had named the viewers); §01's per-number anchors consolidated; M2.5 given the matrix→slot map and the note
that the four built presets of §05.6 have no pre-registered signature yet; M1.8 given the `ambig` sentence its slot
names. Commits `fec7a43`, `8651f67`, `5e358ba`, `927cc61` and the one carrying §03.

**What Phase 3 leaves and what follows.** The 13 empty slots wait on the operator: eight Results slots on his screen
(§05.5, §05.6, §05.7, §05.10, §05.12) or on scene families that do not exist (§05.8, §05.9, §05.11), §06.3 on the marker
captures, and the four `AFTER` slots on the filled Results. Phase 4 (the structural pass over real content, T2 again),
Phase 5 (T3, the hostile reviewer) and Phase 6 (corrections) are not run: a structural pass cannot judge whether the
structure "carries the demonstrative load" while eight Results slots are empty. T3 on the filled sections alone is
available on his word. No LaTeX master, no bibliography file, no compile.

## Not done

- Phase 2 wrote no content; Phase 3 wrote content only into the slots listed above.
- The identity block was not touched (hash verified before writing, and again by the round-2 auditor).

*Made with my soul - Swately <3*
