# A1 — the anti-aliasing instrument, and its own acceptance

> **DEFAULT CHANGE, 2026-09-11 — every number below predates it.** `single_track`'s screen-static ramp
> shipped as `smoothstep(1.2, 3.0, ...)` when these rows were measured; it now ships as
> `smoothstep(1.0, 1.6, ...)` (`hold_lo` / `hold_hi`, P-042 in `docs/LEARNING_LOG.md`). Under the new
> default the same corpora score BETTER on hallucinated mass everywhere (-8 % to -54 %), better on the
> spinning box's position at 6.7 and 13.4 px/pair (-15 %, -25 %), and WORSE on the sphere's position at
> 6.7 px/pair (+11 %). **To reproduce a row on this page exactly, pass `--st-hold-lo 1.2 --st-hold-hi 3.0`.**
> These rows are not re-measured: re-running the twelve-corpus record is its own job.

**Date:** 2026-09-07 · **Tool:** `tools/aa_truth/aa_report.py` · **Corpus:** `tools/aa_truth/aa_zoo.py`
**Status:** the instrument exists, verifies itself against a closed form, and its central claim is
measured rather than asserted. **The AA route is not yet decided — that is A2.**

---

## 1. What this had to be, and why the obvious answer is wrong

Day 1 produced the corpus: an exact analytic reference (`ref/`) and a 1-SPP point-sampled input
(`alias/`), aligned by construction. Day 1's own gate compared them with a whole-image probe and
noted its own weakness in the commit that created it:

> those margins are thin because the probe is a whole-image aggregate dominated by the noise
> background. The real instrument must measure transition width NORMAL TO EACH KNOWN ANALYTIC EDGE.

That whole-image probe, re-run today on an untouched generator, still reports **transition width
39.853 px (reference) vs 39.083 px (aliased)** — a 2 % margin on the quantity that is supposed to
separate an exact edge from a staircase. The instrument built here reports **35.1×** on the same
material. The difference is not tuning; it is that the two are measuring different things.

The reason the obvious measures fail is that **PSNR, SSIM and warping error all reward blur.** This
repository already owns that precedent: a configuration that improved warping error 18.34 → 12.96
while collapsing Sobel edge magnitude by 60 %. A metric win that was a quality loss.

The usual patch — "add a sharpness term" — is **also insufficient**, and this corpus proves it
rather than arguing it. Scored on transition width alone, **the aliased arm wins: width ratio
0.34× against its own exact reference.** A staircase is not blurry. It is sharp and wrong. Width is
therefore kept here as a *veto* and never as a score.

## 2. Three estimators, two of them wrong, and why that is the useful part

**Version one** binned every pixel of a bar by its distance to the true line and read the scatter
inside each bin. On a 960-px bar that pools ~1000 pixels per bin, which **averages the staircase
away** — the measurement was itself an anti-aliaser. It reported a scatter ratio of **1.01×** on a
corpus whose two arms are exact and point-sampled.

**Version two** cut each bar into short chunks and solved each chunk's mean intensity for the
crossing. Still wrong, and wrong more instructively: **a point-sampled mask IS the set {s < 0}**, so
the area it lights inside any window defined by `s` is exactly correct. No area estimator pooled
across scanlines can ever see the error, at any chunk size.

> **Aliasing is not an error in how many pixels are lit. It is an error in which ones.**

That is visible only one scanline at a time. **Version three** walks the dominant axis (columns for a
shallow edge, rows for a steep one — the image is transposed and the normal components swapped,
which leaves the `s` field identical) and recovers the edge's sub-pixel position from **that
scanline's own integral**:

    with samples at spacing D = |ny| in signed distance, and n normalized 1 inside -> 0 outside,
        offset = s_first - D/2 + D * SUM(n)

Nothing is resampled anywhere: interpolating the image would anti-alias the very thing being
measured, and the inside/outside levels are re-read per scanline so a varying background is tracked
rather than assumed away. A first attempt used a two-point level crossing instead of the integral;
that biases the *exact* arm by a ~0.08 px sawtooth at the kink where the coverage ramp saturates,
and showed up as a reference that appeared to wobble 0.17 px when its truth is zero.

**The measures.** `wobble_px` = std of the offset across scanlines in a frame (the staircase, in
pixels). `crawl_px` = std of the same offset across frames (temporal edge motion). `centre_px` = mean
offset against the analytic line — this exists because a filter can smooth an edge into the right
*shape* in the wrong *place*, which nothing comparing two renders to each other can see. `scatter` =
along-edge intensity variation, the measure at steep angles. `width_1090` = the blur veto.
`band_err` = the L2-style number, carried so the inversion stays visible.

## 3. A defect this exposed in the corpus, and its exact fix

The reference arm at first measured **0.95 px of wobble at 15°** where its truth is zero. The cause
was not the estimator: `aa_zoo` places bars at four different angles, so **they necessarily cross**,
and a scanline through a crossing shows a clean, monotone, single step that belongs to the *wrong*
edge. Monotonicity cannot catch that — `col 414` read `0.976 0.976 0.976 0.976 0.875 0.620 0.624 …`,
a textbook step, entirely inside the overlap of two bars.

Because the corpus carries the geometry analytically, the exclusion is **exact rather than
heuristic**: a scanline is rejected if any window pixel falls within another bar's band plus one
pixel of coverage ramp. Reference wobble at 15° fell from 0.95 px to **0.0111 px**. The generator was
not modified — crossings are realistic and belong in training data; they simply cannot carry a
per-edge number.

## 4. The acceptance check: the closed form predicts every individual measurement

`--gate` does not merely assert "the aliased arm is worse". For a point-sampled arm `SUM(n)` is an
integer, so the estimator must return the smallest lattice point at or above zero, minus D/2:

    predicted offset = (beta mod D) - D/2,   beta = nx(c + 0.5) + ny/2 + d,   EXACTLY, per scanline

This is angle-agnostic, which matters: the familiar `D/sqrt(12)` spread assumes the lattice phase is
uniform across scanlines, and **at exactly 45° the phase does not advance between scanlines at all**,
so every scanline carries the same staircase step and the spatial measure is degenerate there (the
temporal one still carries it). Measured wobble at 45° is 0.1466 px against a `D/sqrt(12)` of
0.2041 — the per-scanline model reproduces it anyway.

**Flat background, 960×540, 32 frames, 12 edge classes:**

| angle | period px | wobble alias | predicted | wobble ref | ratio | model resid |
|---|---|---|---|---|---|---|
| 1° | 57.3 | 0.2887 | 0.2886 | 0.0124 | **23.3×** | 0.00000 |
| 5° | 11.4 | 0.2875 | 0.2876 | 0.0122 | **23.5×** | 0.00000 |
| 15° | 3.7 | 0.2788 | 0.2788 | 0.0111 | **25.1×** | 0.00000 |
| 45° | 1.0 | 0.1466 | 0.2041 † | 0.0079 | **18.5×** | 0.00000 |

*(contrast 0.25 shown; the full 12-class table is `--md`. † the degenerate case above.)*

**The model residual is 0.00000 px on all twelve classes** — the closed form predicts every
individual scanline of the aliased arm — against a floor of 0.0124 px, which is where the exact arm
itself sits relative to analytic truth (uint8 quantization of a ramp; the binary arm has none).

**Noise background**, same geometry: both arms rise together — model residual 0.3008 px on the
aliased arm against 0.2919 px on the exact one at 1°/0.25. **The same background perturbs both
scanline integrals equally**, so the model still explains the aliased arm as well as analytic truth
explains the exact one (worst excess **+0.00888 px**). The gate is written in that form for exactly
this reason, and the noise floor is reported rather than hidden: **0.30 px at contrast 0.25, 0.10 px
at contrast 0.95, against 0.012 px on a flat background.** At low contrast that floor is the same
size as the staircase, so the positional measure carries no verdict there.

**DI-3.** Two corpora differing in **both seed and drift phase**. Individual scanline offsets are not
comparable across such a pair (a different sub-pixel phase is a different quantity), so the
run-to-run Pearson is taken over the cells, as `motion_report` takes it:

* flat: **r = 1.000**, max per-class discrepancy **1.3 %**, median 0.2 %
* noise: **r = 0.979**, max per-class discrepancy **8.5 %**, median 3.2 %

Every class is inside the 20 % threshold; all numbers above may be cited.

## 5. The central claim, measured

The instrument claims a blur cannot game it. That is a prediction, so it was tested: four candidate
arms built from the *aliased* input and scored against the same exact reference.

| arm | staircase (wobble ratio) | vs input | width ratio | max centre px | band err | verdict |
|---|---|---|---|---|---|---|
| `alias` (the input) | 35.1× | +0 % | 0.34× | 0.0058 | 0.03411 | no gain |
| `blur_norm` — Gaussian **along the normal** | 32.7× | **−7 %** | **4.01×** | 0.0104 | 0.05213 | BLURRED |
| `blur_iso` — isotropic Gaussian | 28.0× | −20 % | 2.28× | 0.0067 | **0.03388** | BLURRED |
| `box3` — plain 3×3 box | 27.1× | −23 % | 2.34× | 0.0073 | 0.04163 | BLURRED |
| `blur_tan` — Gaussian **along the edge** | **24.4×** | **−31 %** | **0.79×** | 0.0078 | 0.02322 | ACCEPT |

Both corpora, DI-3 satisfied: worst run-to-run discrepancy on wobble and width **2.8 %**.

This is the predicted behaviour, and it is the reason to trust the measure:

* Blurring **along the normal** preserves each scanline's integral, so it buys **7 %** of the
  staircase and pays a **4.01×** width penalty. Almost pure cost.
* Blurring **along the edge** mixes neighbouring scanlines' integrals, buys **31 %**, and pays
  **nothing** (0.80×). That is not cheating — it is the operation correct AA must perform on a
  shallow edge, and the measure rewards it.
* Isotropic and box blurs land in between: partial credit for their tangential component, full
  penalty for their normal component.

**The inversion is visible in the same table.** `blur_iso` scores **band_err 0.03388 against the
input's 0.03411** — on the L2-style number it *improved* the image, while widening every edge 2.28×.
An L2 metric would have accepted it. The conjunctive verdict rejects it.

The same ranking on the **noise** corpus reaches the same order and the same verdicts, compressed by
the background floor (DI-3 worst discrepancy 5.9 %):

| arm | staircase | vs input | width ratio | band err | verdict |
|---|---|---|---|---|---|
| `alias` | 2.1× | +0 % | 0.53× | 0.03413 | no gain |
| `blur_norm` | 2.1× | **+0 %** | 3.19× | 0.05212 | no gain / BLURRED |
| `blur_iso` | 1.8× | −16 % | 1.94× | **0.03367** | BLURRED |
| `box3` | 1.7× | −18 % | 2.00× | 0.04203 | BLURRED |
| `blur_tan` | 1.6× | −24 % | 0.88× | 0.02337 | ACCEPT |

Over real content the normal-direction blur buys **literally nothing** (+0 %) while still widening
every edge 3.19×, and the L2 inversion is larger, not smaller: `blur_iso` beats the input by 0.00046
on `band_err` while widening 1.94×.

## 6. What this sets up, and the number that decides the project

The best fixed blur recovers **31 %** of the staircase. That is the bar. A2's least-squares kernel
oracle — the best possible linear predictor of the exact reference from an aliased neighbourhood — is
the *upper bound* on what any small learned kernel can do, and it is now measurable in one command:

```bash
python tools/aa_truth/aa_report.py --run <corpusA> --run <corpusB> --arm alias --arm cmaa2 --arm oracle
```

**The stop rule stands unchanged:** if the oracle does not beat CMAA2 by more than three times the
run-to-run spread, no network can, and the correct answer is to ship CMAA2 in the `--afill` pass and
stop. The run-to-run spread is now known — **1.3 % on flat, 8.5 % on noise** — so that rule has a
number attached to it for the first time.

## 7. What is NOT established

* **Nothing about a real game.** This is a synthetic corpus of straight bars. A0 established that
  neither DSR nor render-scale can supply real-game ground truth; the untried escape (static 3-D
  geometry with the HUD hidden) remains untried.
* **Nothing about cost.** No kernel has been run on a GPU. The contention law (E0) says any submit
  over ~0.4 ms pays 3–8× under a saturating competitor, and that budget has not been spent yet.
* **Nothing about whether learned AA beats CMAA2.** That is A2, and it is the decision point.
* The positional measure **carries no verdict on the noise corpus at contrast 0.25**, where the
  background floor equals the signal. Only the flat corpus and the higher contrasts are citable
  there.
* Exactly 45° is **degenerate for the spatial measure**. The corpus should use a non-degenerate steep
  angle (43°, say) if that regime ever becomes load-bearing; today the temporal measure covers it.

---

*Made with my soul - Swately <3*
