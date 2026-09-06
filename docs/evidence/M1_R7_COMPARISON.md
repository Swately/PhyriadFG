# M1 — the two warp paths compared (R7(a)) · 2026-09-06

> **What this is.** R7's gate asks for the M1 baseline table on BOTH paths and then compares them
> (`≤ 0.10 px` mean shift, p95 within spread, `r ≥ 0.5`). The per-path tables are
> [`M1_R7_DEFAULT.md`](M1_R7_DEFAULT.md) and [`M1_R7_FGCORE.md`](M1_R7_FGCORE.md); this is the comparison
> and what it turned out to be about. The full account, including the instrument that answers the same
> question 6–8 orders of magnitude more sharply, is `../planning/records/R7_GATE.md` §7.
>
> **Method.** The same zoo the 2026-09-04 baseline used (`zoo_static`, seed 20260904, noise background,
> pan 0, 1280×720 @ 60 fps), both sides captured on the same day, alternated, at two sampling levels:
> 2 runs × 16 triples per path, then 2 runs × 48 triples per path. The 48-triple pair is the measurement.

## 1 · The capture chain's own floor — the control

`marker_extract.py` checks itself on the REAL plane before it reports on the live one. All four 48-triple
runs: **0.095 / 0.097 / 0.096 / 0.097 px mean** against its 0.25 px bar. The floor is the same on both
paths, which is what says the two sides were photographed the same way.

## 2 · Mean placement error, and the shift between paths

`spread` is the larger of the two within-side ranges — the resolution the instrument actually has.

| class | default | `--fg-core` | within-side spread | shift | verdict |
|---|---|---|---|---|---|
| linear | 0.617 | 0.624 | 0.048 | **+0.007** | inside the 0.10 px bar **and resolved** |
| circular | 0.821 | 0.842 | 0.078 | **+0.021** | inside the bar **and resolved** |
| accel | 0.749 | 0.785 | 0.128 | **+0.036** | inside the bar; spread marginally above it |
| fast (control) | 5.981 | 5.895 | 0.587 | −0.086 | inside the bar, not resolved |
| crossing | 1.289 | 1.160 | 0.182 | **−0.129** | over the bar, under its own spread |

p95: every between-path delta is inside the within-side spread.

**`crossing`, the one class over the bar, reverses sign with sampling:** +0.171 at 16 triples, −0.129 at 48.
A systematic difference does not change sign when the sample grows; a sampling artefact does.

## 3 · The criterion's `r ≥ 0.5` clause fails for the SHIPPING DEFAULT

At 48 triples the report has enough population to use **exact** `(k_prev, marker)` matching instead of the
`(marker, phase-bin)` MEANS fallback the 2026-09-04 baseline fell back to:

| class | `r` default | `r` --fg-core |
|---|---|---|
| linear | 0.16 | 0.17 |
| accel | 0.32 | 0.42 |
| circular | 0.58 | −0.17 |
| crossing | 0.75 | −0.07 |
| fast | 0.27 | 0.21 |

The published baseline's `r` 0.76–1.00 came from averaging each marker's detections inside a phase bin —
which removes exactly the run-to-run variation `r` exists to detect. Measured per detection, the error is
largely **not** reproducible run to run, **on either path**. A gate the incumbent fails cannot judge the
challenger: the criterion needs restating, and that is the operator's call, not a number to be re-picked.

## 4 · What this comparison does NOT claim

It does not say the two kernels are identical — that is `--fg-core-ab`'s job and it answers far more
sharply (13.3 billion pixel comparisons, 1.9×10⁻⁸ differing; zero when FMA contraction is forbidden). It
covers the STATIC scene; the panning scene is measured separately at four runs per side (`R7_GATE.md` §7.5) and agrees. And it says nothing about behaviour
**under the load governor**, where the legacy path spends most of its ticks in a mode the pure core cannot
reproduce — `R7_GATE.md` §7.3 measures that, and it is the finding that decides R7(a).

*Made with my soul - Swately <3*
