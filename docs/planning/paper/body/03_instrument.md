# 3. Method I — the instrument

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 4 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## M1.1 — The analytic scene: closed-form pose(t), shapes / textures / motion, the barcoded base index, the presets (the five in the boundary; `static` as the control; `fast_train` as F1's candidate, his call)

*serves:* SS · *evidence:* `tools/scene_truth/scene_zoo.py`; `B1_FIRST_FG_ROW.md` §1 · *status:* `EXISTS`

The truth is rendered, not recorded: `pose(t)` returns the axis-angle rotation `rot0_deg + omega_deg·t`
and the translation `pos + vel·t` (`scene_zoo.py:133`), so any real t is renderable by the code path that
renders the base grid. Shapes are `sphere`, `box`, `quad`; albedo is `flat`, `checker` or aperiodic value
noise (`scene_zoo.py:180`); motion is a linear velocity plus a constant spin, both scaled by `--speed`
(`scene_zoo.py:455`). The labels are exact, not estimated: object id, depth, flow, and a per-pixel
visibility class whose class 0 is the disocclusion bucket (`scene_zoo.py:21`, `scene_zoo.py:26`).
`--barcode` bakes the 16-bit base index into every frame (`scene_zoo.py:463`), a strip the scorer masks
out of every term (`scene_report.py:230`).

Six presets are built (`scene_zoo.py:193`): five inside the frozen boundary — `translate`, `occlude`,
`spin`, `cross`, `mixed` — and `static`, the verify gate's control, which carries no FG row. `fast_train`
(seven spheres 5.5 units apart, `scene_zoo.py:218`–`scene_zoo.py:225`) is not among the presets the frozen
boundary names; it was built on 2026-09-10, and admitting it as F1's scene family is the operator's
decision. Corpora are 640×360, 240 fps base, 1 s, k = 4 manifests (`REGIME_TEST_MATRIX.md` §9).

## M1.2 — Exact-phase alignment: reading the generator's own phase, the k-apart rule, cut pairs filed apart

*serves:* TH · SS (F3 clause) · *evidence:* `tools/scene_truth/scene_align.py`; `B1_FIRST_FG_ROW.md` §2, §4 · *status:* `EXISTS`

A generated frame is placed by content, not by order: "decoding the barcodes on those two gives (N, N1)
exactly. The FG's own phase t then places the generated frame at base index N + t·k"
(`scene_align.py:7`), with `pair_ok` (N1 − N == k) recorded per frame (`scene_align.py:12`). The live arm
is then scored against a truth rendered on demand at its own phase (`scene_report.py:489`), because
"comparing to the nearest base frame would charge the FG up to half a base frame of motion that is the
alignment's, not its own" (`scene_report.py:477`); the FG's phases sat "a mean 0.487 base frames" from the
nearest base frame (`B1_FIRST_FG_ROW.md` §1).

A frame whose two reals are not k apart has no base-grid position: it is filed apart as a cut, scored
against both endpoints, "never mixed into the base-grid sequence" (`scene_align.py:12`), counted and
excluded by the scorer (`scene_report.py:486`). One frame forced the rule — 2,865 px² of hallucinated mass
at 197.7 px of displacement per pair on an object that moves 3.4, the loop seam; excluding it moved the
φ > 0.75 mean from 1.25 px to 0.25 px (`B1_FIRST_FG_ROW.md` §4). One cut was excluded from the seed-7 run
(133 scored) and 3 from seed 11 (128) (§2, §2b). Cut scoring is §03.7.

## M1.3 — The terms: the six verdict terms and the three reported apart, each definition quoted from `scene_report.py`

*serves:* PO · *evidence:* `tools/scene_truth/scene_report.py` docstring and `verdict()` · *status:* `EXISTS`

The six verdict terms in the frozen priority order, quoted from the scorer's docstring: 1 `pos_err`,
"centroid of the candidate's silhouette vs the truth's, object k", px (`scene_report.py:11`);
2 `halluc`, "candidate silhouette where the truth silhouette is absent (ghosts, doubles, crescents)", px²
(`scene_report.py:13`); 3 `missing`, "truth silhouette where the candidate's is absent (holes)", px²
(`scene_report.py:16`); 4 `shape_err`, "symmetric boundary (chamfer) distance, candidate silhouette vs
truth's", px (`scene_report.py:12`); 5 `sharp`, "edge strength on the truth's boundaries, candidate /
truth" (`scene_report.py:18`); 6 `bg_err`, "RMS error on backdrop pixels far from every object (seams,
reclaim)" (`scene_report.py:17`). `halluc` also carries "a SIGN along the object's motion: leading (ahead
of where it is going) or trailing" (`scene_report.py:15`), never a veto by itself.

Three sit beside the six, never folded in: `disocc`, "RMS error on class-0 pixels
(visible in NEITHER real frame) — reported APART, never in the verdict" (`scene_report.py:20`);
`graceful`, "RMS distance to the nearest REAL frame, which is what failing gracefully looks like"
(`scene_report.py:21`); and `l2_det`, "RMS on determinable pixels only, carried so the inversion stays
visible" (`scene_report.py:23`). Both silhouettes come from one operator, which is what puts the floor at
zero: "The truth scored against itself is then exactly zero on all four object terms"
(`scene_report.py:33`).

## M1.4 — The verdict: conjunctive, per object; the thresholds; the displacement floor (an object below it carries no claim)

*serves:* PO · RU · *evidence:* `scene_report.py` `verdict()`; `B1_FIRST_FG_ROW.md` §3 (k = 2) · *status:* `EXISTS`

`verdict()` is "Conjunctive. `floor` = the truth arm's own summary (exactly zero on the object terms)"
(`scene_report.py:613`). Per object, `pos_err` and `shape_err` stay within 0.5 px of the floor's own value
and `halluc` and `missing` within 1.5× it plus `slop = 0.5 * o['perim_px']`, "half a pixel of edge slop:
what a resampler is owed"; per frame, `bg_err` within 0.01 of the floor and `sharp` at or above
`FLOOR_SHARP = 0.90` (`scene_report.py:63`, `scene_report.py:617`–`scene_report.py:623`). One failure is
enough, and the corpus-level number is a summary, never the verdict.

The tolerances are policy and a pass can be narrow: "the sphere's 0.481 px sits under the 0.5 px tolerance
by 0.019. The tolerance is this instrument's policy; the number is the finding" (`B1_FIRST_FG_ROW.md` §2);
the second seed reads 0.468 px, deviation 2.6 % (§2b). Below about one pixel of displacement per source
pair the conjunction stops discriminating: at k = 2, ~0.85 px per pair, `blend` is ACCEPTED, because "the
ghost fringe is inside the perimeter tolerance and sharpness stays above 0.90" (§3). An object under that
floor carries no claim.

## M1.5 — The rulers: `truth` / `nearest` / `oracle2` scored by the same code on the same corpus; `blend` and `blur` from the synthetic sweep

*serves:* RU · *evidence:* `B1_FIRST_FG_ROW.md` §2, §3; `B1_SWEEP_seed7.md` / `_seed11.md` · *status:* `EXISTS`

Five arms are built from the corpus itself — `SYNTH = ('truth', 'nearest', 'blend', 'oracle2', 'blur')`
(`scene_report.py:371`): `truth` is the held-out frame, `nearest` the nearer real, `blend` their 50/50
mix, `blur` the truth Gaussian-blurred, and `oracle2` a warp of both reals by the exact reprojection of
each material point, falling back to the nearer real in class 0 (`scene_report.py:384`). Three are the
identity's rulers — `truth` (zero by construction), `nearest`, `oracle2` — scored by the same code on the
same corpus at the same k: 0.000 / 0.275 / 0.097 px on the k = 4 `mixed` corpus, and 0.279 / 0.102 on the
second seed (`B1_FIRST_FG_ROW.md` §2, §2b). `blend` is the fourth arm, carried from the synthetic sweep at
the same k, where from k = 2 to k = 16 `nearest` runs 0.217 → 1.105 px, `blend` 0.052 → 1.260 px and
`oracle2` 0.081 → 0.116 px, "the ceiling of a warp with a perfect flow" (§3).

Two seeds: `nearest`, `blend` and `oracle2` agree "within 0.0–10.9 % on every term at every k", while
`blur` — an arm of the sweep, not a ruler — disagrees by 41 % on position, "so its position carries no
verdict and its sharpness does" (§3). The FG rows read against these rulers are §05.1 and §05.4.

## M1.6 — The instrument's own gate (T1–T6): what each tests; the two silhouette operators (`tau`, `coverage`) and the occlusion-aware window; the border-clip rule; the gate state per corpus — a gate binds to the corpus it ran on (T3 open at ×4 / ×8, bar not relaxed)

*serves:* ET · RE · *evidence:* `scene_report.py` (the gate: its docstring and `gate()`); `REGIME_TEST_MATRIX.md` §9; the gate logs under `C:\PhyriadFG\runs\_render_logs\`; `docs/LEARNING_LOG.md` P-029 · *status:* `EXISTS`

The gate scores the five arms against their predicted signatures — "every check seen RED first"
(`scene_report.py:670`). **T1** `truth`: exactly 0 on the four object terms. **T2** `nearest`: its
`pos_err` reproduces the truth centroid's own travel, p90 |residual| < 0.25 px. **T3** `blend` at φ = ½:
pos < 0.15 px, halluc > 0, verdict ≠ ACCEPT, since "a position-only verdict would accept the classic
ghost" (`scene_report.py:706`). **T4** `oracle2`: ACCEPT on the determinable, the disocclusion bucket
carrying the only error. **T5** `blur`: BLUR flagged, pos < 0.3 px. **T6** a cut: `hold` exact identity,
`blend` hallucinating on both sides at sharp_vs_A < 0.9 (`scene_report.py:683`–`scene_report.py:753`);
T3 and T5 read only the pairs no other object reaches into (`scene_report.py:711`).

`tau`, the default and the operator of every recorded row, marks a pixel object where any channel differs
from the exact backdrop render by more than `OBJ_TAU = 0.06` (`scene_report.py:61`), so its boundary
depends on "the backdrop NOISE under the edge -- at x1 the truth and the candidate sit over the same
backdrop and the flip cancels; at x8 (13 px apart) it does not" (`scene_report.py:126`); `coverage` takes
the half-coverage contour, which "no longer depends on what the backdrop is under the edge"
(`scene_report.py:133`). The window is occlusion-aware — object k's reach is its truth mask dilated by
`NEAR_R = 6` px (`scene_report.py:62`) plus its displacement per pair, minus every pixel the labels give
to another object in the mid or either real, minus class 0 (`scene_report.py:218`,
`scene_report.py:265`) — and a moving object
whose silhouette touches the image border carries no terms in that frame: at `--speed 8` the `mixed`
sphere is partially outside the view in 24 of 240 frames (`scene_report.py:205`).

The gate "had only ever been run on the ×1 corpus" (`REGIME_TEST_MATRIX.md` §9). It has since been run on
every scene corpus under both operators, one run each, `--jobs 30`; the state below is read from the logs
under `C:\PhyriadFG\runs\_render_logs\` (`gate_<corpus>.log` = `tau`, `gate_cov3_<corpus>.log` = `coverage`).
The `n` column is the count T3 and T5 actually read: the (frame, object) pairs at φ = ½ that no other object
reaches into (`scene_report.py:711`).

| corpus | scene | speed | seed | `tau` | `coverage` | T2 p90 resid px | T3 blend pos px | n (T3) |
|---|---|---|---|---|---|---|---|---|
| `sc_v05` | `mixed` | ×0.5 | 7 | — | FAILED T2, T3 (its own floors) | 0.015 | 0.053 | 59 / 177 |
| `sc_live` | `mixed` | ×1 | 7 | PASSED T1..T6 | PASSED T1..T6 | 0.015 | 0.060 | 42 / 177 |
| `sc_live2` | `mixed` | ×1 | 11 | — | PASSED T1..T6 | 0.016 | 0.062 | 42 / 177 |
| `sc_v2` | `mixed` | ×2 | 7 | FAILED T2, T3 | PASSED T1..T6 | 0.014 | 0.109 | 21 / 177 |
| `sc_v4` | `mixed` | ×4 | 7 | FAILED T2, T3 | FAILED T3 | 0.016 | 0.263 | 10 / 156 |
| `sc_v8_s7` | `mixed` | ×8 | 7 | FAILED T2, T3, T4 | PASSED T1..T6 | 0.016 | 0.054 | **4** / 137 |
| `sc_v8_s11` | `mixed` | ×8 | 11 | FAILED T2, T3, T4, T5 | PASSED T1..T6 | 0.018 | 0.057 | **4** / 137 |
| `sc_v16_s7` | `mixed` | ×16 | 7 | FAILED T2, T3, T4 | FAILED T3 | 0.022 | 0.698 | **2** / 127 |
| `sc_train8_s7` | `fast_train` | ×8 | 7 | FAILED T2, T3, T4 | FAILED T3 | 0.023 | 0.568 | 35 / 116 |
| `sc_train8_s11` | `fast_train` | ×8 | 11 | FAILED T2, T3, T4 | FAILED T3 | 0.023 | 0.568 | 35 / 116 |
| `sc_train16_s7` | `fast_train` | ×16 | 7 | FAILED T2, T3, T4, T5 | FAILED T3 | 0.027 | 1.105 | 24 / 113 |
| `sc_train16_s11` | `fast_train` | ×16 | 11 | FAILED T2, T3, T4 | FAILED T3 | 0.037 | 1.100 | 24 / 113 |

Three readings, and the third corrects an earlier one. **T2 passes on every corpus under `coverage`**, its p90
residual degrading only from 0.014 to 0.037 px across a 32× range of displacement: the exact-phase alignment
and the silhouette operator hold everywhere the corpora go. **T3 stays open, and on the one scene family that
can measure it the failure reproduces across seeds** — `fast_train`, built so that a sphere is always fully
inside the view, gives 0.568 / 0.568 px at ×8 and 1.105 / 1.100 px at ×16 on two independent corpus seeds,
against a 0.15 px bar (`scene_report.py:720`), with the mechanism offered for it labelled an inference.
**On `mixed`, T3 above ×4 is not a measurement:** the overlap-free sample collapses from 42 pairs at ×1 to 4
at ×8 and 2 at ×16 as the sphere leaves the view and the remaining objects overlap, so `mixed`'s ×8 pass
(0.054 px on 4 pairs) and ×16 failure (0.698 px on 2) decide nothing either way. An earlier statement of this
project's own record read the series 0.060 / 0.109 / 0.263 / 0.568 px as ×1 / ×2 / ×4 / ×8 of one progression;
the first three are `mixed` and the fourth is `fast_train`, so that series was a splice across two scene
families and is withdrawn here (`LEARNING_LOG.md` P-031). **"The bar was NOT relaxed"**
(`REGIME_TEST_MATRIX.md` §9). The rule that follows: "a gate binds to the corpus it ran on … Every corpus
that carries a row runs its own gate first, and the gate's residual at the corpus's displacement IS the
floor under that row" — and under `tau` at ×2 and ×4 that floor "is of the same order as the FG's measured
position error (0.544 / 0.690 px)" (`LEARNING_LOG.md` P-029). What this does to the displacement rows is
§05.3; the threat is §06.2.

## M1.7 — Cut scoring: a cut frame against BOTH real endpoints on the terms that survive (halluc, sharp, graceful); the `hold` and `blend` references; gate T6

*serves:* SS (F3 clause) · *evidence:* `REGIME_TEST_MATRIX.md` §9 (cut scoring); `C:\PhyriadFG\runs\sc_live2\cuts_k4.md` · *status:* `EXISTS`

"There is no interpolation truth for a cut, so there is no pos_err / shape_err / lead_px here: only what
the candidate IS relative to each side" (`scene_report.py:292`). What survives is `halluc` and `missing`
against A and against B, `sharp` against A and against B, and `graceful`, the RMS to the nearer real
(`scene_report.py:296`). Two references go through the same function on every cut: `hold`, the nearer real
unchanged, and `blend` (`scene_report.py:570`). Gate T6 builds its cut candidate from the corpus's own
loop seam, "with no align.json in the loop at all" (`scene_report.py:741`), and "Gate T6 passes on every
corpus" (`REGIME_TEST_MATRIX.md` §9), including corpora that fail T2, T3 or T4.

On `sc_live2`, the nine cut triples of one capture (one run — reliability not measured) carry the two
references as a built-in check: `hold` reads graceful 0.0000 and halluc 0 on every cut, `blend` 0.0355 and
391 px² in the mean (`cuts_k4.md`). The FG's own cut rows are §05.10.

## M1.8 — The second instrument (`tools/motion_truth/`, marker classes / sizes / backdrops / `--fps`, its capture floor, the `ambig` exemption): used for CORROBORATING readings only, because its terms are not the six verdict terms

*serves:* SS · PO · *evidence:* `docs/evidence/MOTION_TRUTH_BASELINE.md`; `M1_SRC_RATE.md`; its own gate records `docs/planning/records/S2_T2_GATE.md` … `S2_T6_GATE.md`; `REGIME_TEST_MATRIX.md` §2 · *status:* `EXISTS`

An older chain measures markers, not scenes: `marker_zoo.py` → `play_frames.ps1` → `--qdump 48` →
`marker_extract.py` → `motion_report.py` (`M1_SRC_RATE.md`, Instrument). Its corpora are 1280×720 marker
fields on a noise backdrop panned at 0 px/s, in the classes `linear`, `accel`, `circular`, `crossing` and
`fast` at sizes 6, 12 and 24 px
(`MOTION_TRUTH_BASELINE.md`), the analytic trajectory sampled at a chosen rate — the same motion at
`--fps 60` and `--fps 30` (`M1_SRC_RATE.md`, Method). Its capture floor is measured before anything is
reported and read 0.116 / 0.116 / 0.119 / 0.118 px over four runs against its 0.25 px bar
(`M1_SRC_RATE.md`, Method). Its reliability statistic is its own — `r`, the correlation of per-marker
errors between two runs — under its own rule, "a class with r < 0.5 is UNRELIABLE and carries no verdict",
which excludes `linear` at r 0.47 (`MOTION_TRUTH_BASELINE.md`). Its gates are recorded apart: S2.T2 to
S2.T5 PASSED (the zoo, the player, the extractor, the report), S2.T6 "BUILT, PARTIAL — the gate does NOT
pass" (the CPU reference warp) (`S2_T2_GATE.md` … `S2_T6_GATE.md`).

Its terms are `err_model`, `err_true`, `phase_ms` and the per-class counts found / degraded / absent /
ghost (`MOTION_TRUTH_BASELINE.md`) — not the six terms of §03.3. It also has a blind spot the scene
instrument does not: because the kernel's `ambig` pass "EXEMPTS object pixels, a null here is 'not visible to
this instrument', never 'period-immune'" (`REGIME_TEST_MATRIX.md` §2, family 2). A marker reading therefore cannot be a
verdict row under the frozen identity; every marker family enters as corroboration only, at the altitude
its own records state, in §06.3.

*Made with my soul - Swately <3*
