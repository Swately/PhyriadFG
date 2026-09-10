# 5. Results

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 6 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## 5.1 — The canonical row: k = 4 on `mixed`, two seeds, with its three rulers and `blend` from the sweep

*serves:* CA · RU · RE · *evidence:* `B1_FIRST_FG_ROW.md` §2, §2b (0.216 / 0.212 px) · *status:* `EXISTS`

The canonical row is `mixed`, 640×360, k = 4 (a 60 fps source) on the synchronous `--qdump` path at build
`f2e4a9a` (identity block), seed 7 (`B1_SPEED_TEST.md` §2), 133 frames at exact phase, cut pairs excluded
(`B1_FIRST_FG_ROW.md` §2). Its three rulers are scored by the same code on the same corpus at the same k.

| arm | pos_err px | shape_err px | halluc px² | lead px | missing px² | bg_err | sharp | disocc (apart) | verdict |
|---|---|---|---|---|---|---|---|---|---|
| `truth` | 0.000 | 0.000 | 0 | +0.00 | 0 | 0.0000 | 1.000 | 0.0000 | ACCEPT |
| `nearest` | 0.275 | 0.140 | 57 | −12.76 | 29 | 0.0001 | 0.934 | 0.0670 | pos |
| `oracle2` | 0.097 | 0.156 | 27 | −23.03 | 56 | 0.0001 | 1.000 | 0.0670 | ACCEPT |
| **`fg`** | **0.216** | 0.145 | 52 | **+8.55** | 25 | 0.0008 | 0.955 | 0.0411 | ACCEPT |

All numbers: `B1_FIRST_FG_ROW.md` §2.

The fourth arm, `blend`, comes from the synthetic sweep at the same k: 0.245 px, 123 px², verdict
"blend: BLUR / halluc / pos" (§3). The FG's position sits below `nearest`'s and at 2.2× `oracle2`'s; its
hallucinated mass carries the opposite sign, leading the motion where `nearest`'s trails, and the
disocclusion bucket is reported apart (§2).

Per object:

| object | motion | pos_err | shape | halluc | lead | missing |
|---|---|---|---|---|---|---|
| 1 sphere, translating | 3.36 px/pair | **0.481** | 0.292 | 76 | +48.4 | 59 |
| 2 occluder quad, static | 0.00 | 0.009 | 0.009 | 6 | 0 | 0 |
| 3 box, spinning | 1.61 px/pair | 0.159 | 0.135 | 76 | −22.8 | 17 |

All numbers: `B1_FIRST_FG_ROW.md` §2.

The conjunction is per object and passes on the sphere by 0.019 px under the 0.5 px tolerance: "The verdict is
a threshold pass, not a comfortable one." (§2).

Reliability: a second corpus seed (11; 128 frames, 3 cut pairs excluded) gives pos_err 0.212 px (2.2 %),
shape 0.137 px (5.9 %), halluc 51.2 px² (2.1 %), lead +8.82 px (3.1 %), missing 23.2 px² (8.9 %), sharp 0.948
(0.7 %); the sphere's position moves 0.481 → 0.468 px (2.6 %) and the box's 0.159 → 0.160 px (0.1 %); the
rulers move as little, `nearest` 0.275 → 0.279 and `oracle2` 0.097 → 0.102. The worst deviation on the citable
terms is 13.1 %, under the 20 % threshold; the verdict is ACCEPT on both seeds (§2b).

## 5.2 — The phase signature: error falling with phase on the fastest mover (M1_LOWPHASE reproduced by an instrument that did not know it)

*serves:* CA (phase) · *evidence:* `B1_FIRST_FG_ROW.md` §2; `docs/planning/records/M1_LOWPHASE_FINDING.md` · *status:* `EXISTS`

On the fastest mover of §05.1 the position error falls with the interpolation phase φ:

| phase φ | sphere pos_err (seed 7) | n | seed 11 | dev |
|---|---|---|---|---|
| < 0.25 | **0.668** | 20 | 0.653 | 2.2 % |
| 0.25 – 0.75 | 0.490 | 93 | 0.507 | 3.5 % |
| > 0.75 | **0.252** | 20 | 0.221 | 13.1 % |

Seed-7 columns: `B1_FIRST_FG_ROW.md` §2; seed-11 and dev columns: §2b.

The fall is "a factor of ~2.7 from φ < 0.25 to φ > 0.75" (§2b); the high bin is the smallest, 20 frames a side,
and carries the run's worst seed deviation, 13.1 % (§2b).

A different instrument on a different corpus measured the same shape on 2026-09-04: 1.668 px at t ≈ 0.1 and
0.373 px at t ≈ 0.85 (`M1_LOWPHASE_FINDING.md` §1), there attributed to "a single default-on layer, the 3×3
vector-median consensus pass on the MV field" (same record, header). The scene record states "An independent
renderer, scorer and corpus reproduce its shape." (§2); its instrument did not carry the finding, and nothing
is attributed from the scene rows themselves (§05.12).

## 5.3 — The displacement axis on `mixed`: the recorded rows (`tau`) AND the re-scored rows (`coverage`), one run each, with the gate state of each corpus; both fitted laws shown; the identity's clause (a) stands as written until the operator re-opens Phase 1

*serves:* CA (displacement) · RU · *evidence:* `B1_SPEED_TEST.md` §2–§3; `REGIME_TEST_MATRIX.md` §9 (re-scored table); SPINE post-freeze note · *status:* `DECISION`

Displacement per source pair is reached on `mixed` by the speed path: k fixed at 4, motion ×0.5, ×1, ×2, ×4.
Object 1, the translating sphere, under the recorded `tau` silhouette operator:

| corpus | seed | speed | n | disp px/pair | pos px | shape px | halluc px² | halluc/area | lead px | missing px² |
|---|---|---|---|---|---|---|---|---|---|---|
| sc_v05 | 7 | 0.5× | 96 | 1.68 | 0.355 | 0.230 | 67 | 0.0048 | +47.8 | 34 |
| sc_live | 7 | 1× | 133 | 3.36 | 0.481 | 0.292 | 76 | 0.0056 | +48.4 | 59 |
| sc_v2 | 7 | 2× | 129 | 6.72 | 0.544 | 0.273 | 72 | 0.0068 | +29.7 | 56 |
| sc_v4 | 7 | 4× | 109 | 13.28 | 0.690 | 0.396 | 98 | 0.0096 | +24.0 | 65 |

All numbers: `B1_SPEED_TEST.md` §2. The ×1 point carries the two seeds of §05.1; "the 0.5×, 2× and 4× points
are single runs" (§6) — one run, reliability not measured. The fitted law is pos ≈ 0.30 · disp^0.32, with
"There is no cliff up to 13 px per pair on this content." (§3).

The same aligned frames, re-scored under the gate-passing operator `--silhouette coverage`, one seed and one
run per point — reliability not measured:

| corpus | px/pair | sphere pos | shape | halluc px² | lead | `nearest` | `oracle2` |
|---|---|---|---|---|---|---|---|
| `sc_v05` | 1.68 | 0.355 → 0.311 | 0.230 → 0.231 | 67 → 56 | +47.8 → +46.1 | 0.416 → 0.284 | 0.153 → 0.009 |
| `sc_live` | 3.36 | 0.481 → 0.443 | 0.292 → 0.400 | 76 → 105 | +48.4 → +36.1 | — | — |
| `sc_v2` | 6.72 | 0.544 → **0.684** | 0.273 → 0.614 | 72 → 225 | +29.7 → +6.6 | 1.299 → 1.292 | 0.127 → 0.010 |
| `sc_v4` | 13.43 | 0.690 → **1.522** | 0.396 → 1.200 | 98 → 607 | +24.0 → +5.7 | 2.514 → 2.809 | 0.131 → 0.009 |

All numbers: `REGIME_TEST_MATRIX.md` §9. Re-fitted over the four `coverage` rows, pos ≈ 0.191 · disp^0.75,
"nearly proportional to displacement" (§9).

The scorer's gate, which binds to the corpus it ran on, under both operators:

| corpus (`mixed`) | under the recorded `tau` operator | under `--silhouette coverage` | T3 blend pos px | n (T3) |
|---|---|---|---|---|
| ×0.5 `sc_v05` | not stated | "fails" T2 / T3 by the gate's own floors (mean travel 0.132 against 0.2); T2 residual 0.015 px | 0.053 | 59 / 177 |
| ×1 `sc_live` | `GATE PASSED (T1..T6)` | PASS T1..T6 | 0.060 | 42 / 177 |
| ×2 `sc_v2` | FAILED T2, T3 (nearest residual p90 0.387 px, blend pos 0.464) | PASS T1..T6 | 0.109 | 21 / 177 |
| ×4 `sc_v4` | FAILED T2, T3 (0.771 / 1.056) | all but T3, against a 0.15 px bar | 0.263 | 10 / 156 |
| ×8 `sc_v8_s7` | FAILED T2, T3, T4 | PASS T1..T6 — but on 4 pairs, so it decides nothing | 0.054 | 4 / 137 |
| ×16 `sc_v16_s7` | FAILED T2, T3, T4 | FAILED T3 — on 2 pairs, so it decides nothing | 0.698 | 2 / 127 |

`tau` column and the ×0.5–×4 `coverage` rows: `REGIME_TEST_MATRIX.md` §9, which also states "The bar was NOT
relaxed." The ×8 and ×16 rows and the `n` column: the `gate_cov3_*.log` files, run 2026-09-10 (§03.6 carries the
full twelve-corpus table). **The `mixed` corpora cannot decide T3 above ×4**: the overlap-free sample T3 reads
falls from 42 pairs to 4 and then 2 as the sphere leaves the view. The family that can decide it is `fast_train`,
where T3 fails and reproduces across two seeds — 0.568 / 0.568 px at ×8 and 1.105 / 1.100 px at ×16 (§03.6). So
the displacement rows above sit under a measured instrument floor at ×1 and ×2, an exceeded one at ×4, and an
unmeasurable one at ×8 and beyond on this scene family.

The rulers say which operator to weigh: under `coverage` the exact-flow oracle scores 0.01 px against 0.13
under `tau` while `nearest` is unchanged; the record reads `tau` as having "under-reported the FG's error at
high speed" (§9).

Clause (a) quotes pos ≈ 0.30 · disp^0.32 and is not edited; the corrected law stands beside it, and "Whether
Phase 1 is re-opened to restate clause (a) is the operator's deliberate act" (SPINE post-freeze note). Both are shown; neither replaces the other here.

The phase profile of §05.2 changes shape with displacement: at 0.5×–2× the error falls monotonically with
phase, while at 4× the middle band (0.814) exceeds the low band (0.758) and the high band stays low (0.267)
(`B1_SPEED_TEST.md` §3).

## 5.4 — The source-rate axis on `mixed`: k = 2 / 4 / 8 / 16 with `blend` per k; the k-path vs speed-path term read against §05.3 at matched displacement and phase (one run, second seed owed)

*serves:* CA (source rate) · SS (F4 clause) · RU · *evidence:* `B1_FIRST_FG_ROW.md` §3; `B1_SPEED_TEST.md` §3 · *status:* `EXISTS`

The k path reaches displacement the other way: same scene, same speed, every k-th base frame shown. The
synthetic arms were swept at k = 2 / 4 / 8 / 16 on two seeds, agreeing within "0.0–10.9 %" on every term at
every k (§3):

| k | displacement/pair | `nearest` pos | `blend` pos · halluc | `oracle2` pos | note |
|---|---|---|---|---|---|
| 2 | ~0.85 px | 0.217 | 0.052 · 46 | 0.081 | **blend ACCEPTED** — the gate's floor is ~1 px/pair |
| 4 | ~1.7 px | 0.275 | 0.245 · 123 | 0.097 | blend: BLUR / halluc / pos |
| 8 | ~3.4 px | 0.571 | 0.590 · 286 | 0.107 | nearest and blend fail everything |
| 16 | ~6.8 px | 1.105 | 1.260 · 622 | 0.116 | oracle2 still ACCEPT |

All numbers: `B1_FIRST_FG_ROW.md` §3. The oracle is flat in k, 0.08 → 0.12 px; the `blend` acceptance at k = 2
is the instrument's displacement floor, below which an object discriminates nothing (§3). `blur`, a synthetic
arm and not a ruler, disagrees between seeds by 41 % on position, so its position carries no verdict and its
sharpness does (§3).

The FG's own k rows are k = 2, 4 and 8, each a single run on seed 7 — reliability not measured: sphere position
0.314 px at 1.68 px/pair (n 5), 0.481 px at 3.36 (n 133), 0.631 px at 6.72 (n 168) (`B1_SPEED_TEST.md` §2). The
k = 2 point is thin: "It is shown, and it is not leaned on", and the matched low-displacement point is the
speed path's 0.5×, 96 frames (§2).

At the matched displacement 6.72 px/pair the k path is worse than the speed path of §05.3: 0.631 against
0.544 px (1.16×), hallucinated fraction 0.0086 against 0.0068 (1.26×); repeated at matched phase bands (§3):

| phase band | k = 8 @ 1× | 2× @ k = 4 | ratio k / speed |
|---|---|---|---|
| 0.25 – 0.75 | 0.666 (n 97) | 0.605 (n 91) | **1.10×** |
| < 0.25 | 0.828 (n 38) | 0.621 (n 19) | 1.33× |
| > 0.75 | 0.302 (n 33) | 0.176 (n 19) | 1.72× |

All numbers: `B1_SPEED_TEST.md` §3. "The gap survives phase matching." (§3): beside displacement there is a
second, smaller term that depends on the source rate itself. The record names the PLL clock,
inertia/persistence and the gme history as candidates, then "Not attributed; named." (§3). Both sides are
scored under the recorded `tau` operator; the re-scored table of §05.3 covers the `fg_k4` arm of the four
speed corpora. The gap is "larger than the k = 4 seed spread (2.6 % on the sphere) … but it is
owed its second seed" (§6): one run on the speed arm.

The sweep is the floor the identity's F4 clause must exceed: F4 is a family only where it adds what it does
not — a base rendered at the low rate with a display-rate multiple, or the `--asw` path (§05.11 holds no FG
row).

## 5.5 — The asynchronous path: its one row, and its second seed

*serves:* OB (async path) · RE · RU · *evidence:* `GDUMP_GATE.md` G5 (0.240 px, one run) EXISTS; the DI-3 second run is family 0's `-Gdump` capture · *status:* `CAPTURE`

*[empty slot]*

## 5.6 — The four built presets inside the boundary that carry no FG row — `translate`, `occlude`, `spin`, `cross`: a row each with its rulers, entering a claim only once measured; their predicted signatures are written in §04.5 before the first capture (the matrix pre-registers none of them)

*serves:* SS (the built presets) · RU · *evidence:* the presets exist in `scene_zoo.py`; no corpus rendered for them, no signature written, no FG row · *status:* `CAPTURE`

*[empty slot]*

## 5.7 — F1, fast — extremely fast motion: `speed_extend` ×8 / ×16 on `fast_train` (corpora rendered, gated), read with the gate state at each speed

*serves:* SS (F1) · RU · *evidence:* family 3 of the matrix; the ×8 / ×16 corpora exist on `C:\PhyriadFG\runs\`; no FG row · *status:* `CAPTURE`

*[empty slot]*

## 5.8 — F1, erratic — reversing or erratic motion: needs a scene family in the six terms (a closed-form reversing preset); the marker `reverse` class is corroboration, §06.3

*serves:* SS (F1) · RU · *evidence:* family 6 of the matrix is a marker family; the scene preset TO BUILD; no FG row · *status:* `BUILD`

*[empty slot]*

## 5.9 — F2 — thin, repetitive or complex-patterned objects: needs a scene family in the six terms (a thin preset; `period_backdrop`); the marker families `thin_size` / `grating_pan` are corroboration, §06.3

*serves:* SS (F2) · RU · *evidence:* family 7 TO BUILD; a thin scene preset TO BUILD; no FG row · *status:* `BUILD`

*[empty slot]*

## 5.10 — F3 — abrupt scene changes: the cut frames of the looped corpus against both endpoints, the references, per phase bin; the every-tick capture and its no-loop null

*serves:* SS (F3) · RU · *evidence:* `sc_live2` cuts (9 cuts, one capture) EXISTS; family 5's `-Gdump` capture + null · *status:* `CAPTURE`

*[empty slot]*

## 5.11 — F4 — low source frame rate beside the k sweep: a base rendered at the low rate with a display-rate multiple; the `--asw` path; the marker `src_rate_matched` family is corroboration, §06.3

*serves:* SS (F4) · RU · *evidence:* the k sweep (§05.4) EXISTS as the floor; the low-rate scene base TO BUILD (the identity's own F4 condition); no FG row · *status:* `BUILD`

*[empty slot]*

## 5.12 — The provenance readings (family 0: `--no-stasis`, `--mv-sim`): reported as READINGS; an attribution claim would exceed clause (a) and needs Phase 1 re-opened

*serves:* CA (its bound) · RU · *evidence:* family 0 of the matrix; `B1_SPEED_TEST.md` §4 holds today's readings · *status:* `CAPTURE`

*[empty slot]*

*Made with my soul - Swately <3*
