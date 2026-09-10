# FG_METRIC_MODEL_SPINE.md — the frozen conceptual identity of the metric paper (KAP Phase 1)

Status: **`FROZEN 2026-09-09`** — identity block SHA-256 `427ac7489c5fdd7f15a34907e20313597256a0f52d0361824d193c29aa3314bf` (UTF-8, LF-normalised, the text between the two HTML markers exclusive of the marker lines); T1 verdicts below; the block is byte-intact from this stamp on · Type: **Analysis** (the Phase 1 output artifact of
`F:\Phyriad\protocols\analysis\KNOWLEDGE_ANALYSIS_PROTOCOL.md` §3.1: the frozen-identity document). Input: the Phase 0
anchor table [`FG_METRIC_MODEL_PRIOR_ART.md`](FG_METRIC_MODEL_PRIOR_ART.md) (all five T0 gates `SWEEP VERIFIED WITH
GAPS`). The operator chose candidate **I-A** on 2026-09-09 (*"Congela I-A, adelante con la Fase 1"*). The identity
block between the two HTML markers is byte-intact from the frozen stamp on: no later pass edits it; a gate that finds
it wanting reports `DRIFTED` (KAP §7, INV-14). The T1 gate that admitted it, its fixes, and the supervisor's
first-hand re-verification of each are recorded after the block.

**The operator's scenario words (2026-09-09, verbatim, the primary source of the scenario set):** *"el objetivo ideal
es poder ajustar lo maximo posible la exactitud de los frames generados y en sus escenarios tipicos: movimientos
extremadamente rapidos o erraticos / objetos delgados, repetitivos o patrones complejos / cambios bruscos de escena /
baja tasa de fps"*.

---

<!-- IDENTITY BLOCK BEGIN -->
## Frozen conceptual identity (DO NOT drift)

**Thesis (one sentence).** The fidelity of a real-time frame generator is measurable against an exact analytic
truth: a 3-D rigid-body scene with closed-form pose(t) is re-rendered at the phase the generator actually produced,
the error of each generated frame is decomposed into named terms — position error (px), hallucinated mass (px²),
missing mass (px²), shape (px), sharpness, background error — with the disocclusion bucket reported apart, and
every cited number carries its run-to-run reliability or the label that it has none.

**Object under test.** PhyriadFG's shipping default kernel (`fg_core.comp`) as configured by its default flags, at
the build each row names: `f2e4a9a` for the synchronous `--qdump` rows of `docs/evidence/B1_FIRST_FG_ROW.md` and
`docs/evidence/B1_SPEED_TEST.md`; the tree committed as `0df0332` (md5 `186CB46C` at the gate; base `364efc5`,
md5 `F36FDE32`) for the `--gdump` rows of `docs/planning/records/GDUMP_GATE.md`. Both present paths are in scope
— the synchronous path under the `--qdump` sampler and the asynchronous shipping path under the `--gdump`
every-tick tap — and a claim about a path rests only on rows measured on that path at a named build; a later
build re-runs the rows it cites. Sources are 640×360 frames decimated by k from a 240 fps analytic base; the
canonical row is k = 4 (a 60 fps source) on `mixed`; k ∈ {2, 8, 16} (120 / 30 / 15 fps sources) and 1080p enter
only where a measured row exists. Every knob change is an A/B against the default, never a redefinition of the
object.

**Scenario set (the boundary of the demonstration).** At most nine analytic scene families, each with closed-form
pose(t) and a barcoded base index: five of the six built presets of `tools/scene_truth/scene_zoo.py` — `translate`,
`occlude`, `spin`, `cross`, `mixed` (shapes sphere / box / quad; textures flat / checker / aperiodic noise; motion =
linear velocity + constant spin; a `--speed` multiplier); the sixth, `static`, is the verify gate's control,
carries no FG row and stays outside the demonstration — and four families named TO BUILD, one per regime the
operator named: (F1) extremely fast or erratic motion; (F2) thin, repetitive or complex-patterned objects;
(F3) abrupt scene changes, whose entry must also state how a cut pair is scored at all, since the scorer today
excludes every live frame whose real pair is not k apart ("there is no interpolation truth for it",
`B1_FIRST_FG_ROW.md` §4); (F4) low source frame rate below or beside the k sweep already measured on `mixed`
(k = 2 / 4 / 8 / 16 = 120 / 60 / 30 / 15 fps sources, `B1_FIRST_FG_ROW.md` §3) — F4 is a family only where it
adds what the k sweep does not (a base rendered at the low rate with a display-rate multiple; the `--asw`
extrapolation path), stated before its first row. A family other than `mixed` enters the demonstration only after
its predicted failure signature is written in the terms above BEFORE its first row and its gate has been seen red
on a synthetic arm; and, as with k and resolution, a family enters a claim only where a measured FG row exists —
today only `mixed` at 640×360 carries FG rows.

**Priority order of the terms (frozen).** 1 position error · 2 hallucinated mass · 3 missing mass · 4 shape ·
5 sharpness · 6 background error. These six are the verdict's terms, exactly as `tools/scene_truth/scene_report.py`
`verdict()` defines them: per object, position and shape within 0.5 px of the truth arm's own value, hallucinated
and missing mass within 1.5× the truth arm's plus half a pixel of perimeter slop; per frame, background error within
0.01 of the truth arm's and sharpness above its floor (`BLUR`). The lead sign is reported with hallucinated mass and
never vetoes by itself. The disocclusion bucket, `graceful` (distance to the nearest real frame) and `l2_det` are
reported beside the six and never folded into the verdict. The verdict is conjunctive and per object; the
corpus-level number is a summary, never the verdict.

**Rulers.** Every FG row is read against `truth` (zero by construction — the verdict's floor), `nearest` (show
the nearest real frame) and `oracle2` (warp with the exact flow), scored by the same code on the same corpus at
the same k; `blend` (the 50/50 mix of the two reals) is the fourth arm and is carried from the synthetic sweep at
the same k (`B1_FIRST_FG_ROW.md` §3), as is `blur`, a synthetic arm of the sweep and not a ruler. A row without its
rulers is not a result. The displacement floor is about one pixel per source pair: below it a blend passes the
conjunction (k = 2, `B1_FIRST_FG_ROW.md` §3), so an object below the floor discriminates nothing and carries no
claim, leaving the conjunction to the others.

**Reliability.** Every cited number is computed twice under independent randomness (two corpus seeds, same
protocol) and the run-to-run agreement is reported beside it; a term whose two runs disagree by more than 20 %
carries no verdict and is cited only with its disagreement stated; a single run is labelled "one run —
reliability not measured".

**Evidence tiers.** A load-bearing number is [V1]: a run, its command, its quoted output line, its record under
`docs/evidence/` or `docs/planning/records/`. Prior art is cited at the level the Phase 0 anchor table assigns
(`FG_METRIC_MODEL_PRIOR_ART.md` §2); nothing [V3] is stated as fact.

**What this identity claims about the world.** (a) That the terms separate the conditions under which the
generator's error changes — phase, displacement, source rate — mechanism attribution not being claimed (the
provenance readings are "readings, not proofs of cause", no knob yet ablated: `B1_SPEED_TEST.md` §4, §6); measured
on the synchronous `--qdump` path at `f2e4a9a` on `mixed`: the M1_LOWPHASE signature (error falling with phase on
the fastest mover) reproduced by an instrument that did not know it, two seeds within 13 %; the speed law
pos ≈ 0.30·disp^0.32 with no cliff to 13 px per pair on this content, its 0.5× / 2× / 4× points one run each —
reliability not measured; a source-rate term beyond exposure (the k path 1.10–1.72× worse than the speed path at
matched displacement and phase), one run on the speed arm, its second seed owed. The asynchronous path holds one
row (pos 0.240 px, one run — reliability not measured, `GDUMP_GATE.md` G5). (b) That the swept literature holds no
evaluation of an interpolation at the generator's own phase against an analytic 3-D truth, no four-way visibility
taxonomy, and no run-to-run reliability of a metric (Phase 0 §4, bounded absences N1–N3, time-boxed searches, not
proofs of absence).

**What it does NOT claim.** Not a quality metric: no human-opinion data, no MOS, no claim about perceived
quality, and no measured comparison with LPIPS / FloLPIPS / PSNR_DIV at all — whether they resolve the 0.2–0.5 px
band the instrument measures is unmeasured here; the prior art states no pixel-scale sensitivity floor for them
(absence N6), documents DISTS as tolerant to translation (Q2-4) and LPIPS-class metrics as registering
misalignment imperceptible to a human (Q2-12). Not real content: every scene is rendered; transfer to real game
content is unmeasured and is stated as such beside every number (Phase 0 row Q3-7, the sim-to-real objection,
carried in this identity as a limit, not answered). Not a learned model: the reference-free predictor of the
error from the kernel's own decisions (candidate I-B) is the next body of work this identity authorizes and does
not contain. Not a distribution metric (candidate I-C, refuted for fidelity by Phase 0 rows Q1-9, Q1-10). Not a
claim about any frame generator other than the object under test.
<!-- IDENTITY BLOCK END -->

---

## Phase 1 procedure (this document's own audit trail)

1. **Draft** written by the supervisor 2026-09-09 from the Phase 0 table §6, candidate I-A, with the operator's
   scenario words folded into the boundary as four families TO BUILD.
2. **T1 gate** (`KNOWLEDGE_ANALYSIS_AUDITOR_PROMPTS.md` §6, template verbatim, mode-lock; `{{INVARIANTS}}` =
   INV-1…INV-4; `{{SCOPE}}` = the identity block): three independent clean-context Opus auditors, workflow
   `wf_912d3e9d-36c` (journal in the session's `subagents/workflows/`), 3 agents, 0 errors, 295,955 tokens, 285 s.
3. **Freeze**: the fixes below applied as bounded point-edits, the block's SHA-256 computed and written into the
   status line, the status set to `FROZEN`.
4. **After the freeze**: Phase 2 (scaffold) and Phase 3 (content) conform to the block; the identity gate
   (`INTACT | DRIFTED`) is the only gate that may speak to it (KAP §6, §7).

### T1 verdicts (2026-09-09)

| auditor | verdict | invariants failed | fixes proposed |
|---|---|---|---|
| 1 | `FREEZE WITH FIXES` | INV-2 (boundary not detectably explicit: preset count, "nine", no build pin), INV-4 (`missing`, `bg_err` unranked) | 11 |
| 2 | `FREEZE WITH FIXES` | INV-4 | 8 |
| 3 | `FREEZE WITH FIXES` | INV-4 | 9 |

No auditor returned `NOT READY TO FREEZE`; all three found INV-1 (one quotable thesis sentence) and INV-3 (no
competing thesis) satisfied. The strictest verdict binds: `FREEZE WITH FIXES`.

### The consolidated fixes, each re-verified first-hand by the supervisor before it was applied (KAP §9.2)

| # | fix (auditors) | first-hand check | disposition |
|---|---|---|---|
| 1 | `static` is a sixth built preset; "the five BUILT presets" was false against the file (1, 2, 3) | `scene_zoo.py` PRESETS: `static`, `translate`, `occlude`, `spin`, `cross` + `mixed` (line 215); `verify()` calls `make_scene('static', …)` at line 395 | applied |
| 2 | "Nine families" → "at most nine: five built, four named to build" (1) | four presets do not exist | applied |
| 3 | pin the build per row (1, 2) | `B1_FIRST_FG_ROW.md` and `B1_SPEED_TEST.md` headers: "`build-release/phyriad_fg.exe` at `f2e4a9a`"; `GDUMP_GATE.md` lines 5–7: base `364efc5` md5 `F36FDE32`, new = the working tree md5 `186CB46C` (committed as `0df0332`, `git log`) | applied |
| 4 | thesis: "or the label that it has none" (1) | the Reliability paragraph admits single runs; the thesis said "every number" | applied |
| 5 | label the speed-law points and the k-path gap "one run"; "on this content" (1, 2, 3) | `B1_SPEED_TEST.md` line 124 "4× points are single runs", line 125 "owed its second seed" | applied |
| 6 | "discriminate mechanisms" → "separate the conditions … mechanism attribution not claimed" (1, 3) | `B1_SPEED_TEST.md` line 73 "Not attributed; named.", line 109 "These are readings, not proofs of cause", line 129 "No ablation yet" | applied |
| 7 | tag claim (a) with the path it was measured on (1) | both B1 headers: `--qdump` (sync present path) | applied, and the async row named with its one-run label |
| 8 | rulers: FG rows carry three arms; `blend` comes from the sweep (1, 2, 3) | `B1_FIRST_FG_ROW.md` §2 header: `truth`/`nearest`/`oracle2`/`fg`; §3 table carries `blend` per k | applied (auditor 3's "same corpus at the same k" wording) |
| 9 | `missing px²` and `bg_err` are unranked; "never in the verdict" (1, 2, 3) | **The auditors read the tables, not the code.** `scene_report.py` `verdict()` lines 421–433: `missing` (line 430) and `bg` (line 431) DO veto, as do `pos`, `shape`, `halluc`, `BLUR`; `disocc`, `graceful`, `l2_det` do not (docstring lines 10–22) | applied with correction: both terms RANKED as verdict terms (order 3 and 6), thresholds quoted from the code |
| 10 | floor per object, not per row (1) | `verdict()` loops per object; `B1_FIRST_FG_ROW.md` §3 k = 2: "blend ACCEPTED — the gate's floor is ~1 px/pair" | applied as "an object below the floor discriminates nothing and carries no claim" |
| 11 | "they do not resolve the band" was refuted by its own citation Q2-12 (1, 2, 3) | Phase 0 Q2-12: LPIPS-class metrics ARE "sensitive to a small alignment error that is imperceptible to the human eyes"; N6 is an absence | applied (auditor 3's wording: unmeasured here; the three rows characterised correctly) |
| 12 | family entry: only `mixed` carries FG rows; the entry rule cannot bind `mixed` retroactively (2) | `B1_SPEED_TEST.md` header "Scene: `mixed`"; `verify()` exercises `translate`, `static`, `cross`, `spin` (lines 359–421) | applied |
| 13 | > 20 % disagreement: "carries no verdict, cited with its disagreement" (2) | `B1_FIRST_FG_ROW.md` lines 106–108: blur 0.17 vs 0.27 px (41 %), "its position carries no verdict and its sharpness does"; `scene_report.py` line 553 prints the same rule | applied |
| 14 | F3 must say how a cut pair is scored (3) | `B1_FIRST_FG_ROW.md` lines 134–136: "the honest output for a cut: there is no interpolation truth for it … not k apart" | applied |
| 15 | "carried in the thesis" → "carried in this identity" (3) | the thesis sentence holds no sim-to-real clause | applied |
| 16 | F4's floor vs the measured k sweep (3) | `B1_FIRST_FG_ROW.md` §3 table: k = 2 / 4 / 8 / 16 rows on `mixed` | applied |
| — | `blur` "is not a ruler and carries no verdict" (2) | `B1_FIRST_FG_ROW.md` line 108: blur's sharpness DOES carry a verdict | applied in part: "a synthetic arm of the sweep and not a ruler"; the "carries no verdict" half rejected |

Not re-run: no FG row was re-scored for the freeze; the identity states what the rows hold. The auditors did not
open `scene_report.py` (outside their permitted set), which is why fix 9 needed the supervisor's correction — a
gate's finding is a claim, not a fact (CONDUCT §2, delegated-work row).

*Made with my soul - Swately <3*
