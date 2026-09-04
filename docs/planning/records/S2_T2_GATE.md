# S2.T2 — gate record: the ground-truth marker zoo · 2026-09-04

> Phase T2 of [`../MOTION_TRUTH_MASTER_PLAN.md`](../MOTION_TRUTH_MASTER_PLAN.md) (strategy S2).
> **Verdict: PASSED.** `tools/motion_truth/marker_zoo.py` renders sequences whose true position is a
> closed form evaluable at any real `t`, every frame carries its own index, and the gate was run
> against what is **on disk**, not against the arrays that produced it. Every number below is quoted
> from command output.

## 1 · Why it was built now, and why it is two things

It is the source of **M1** — the metric the whole objective rests on and which has never been measured
for any path. A position table is ground truth only if the true position is known at any real `t`, not
just at the integer frames the source rendered, because the FG generates *between* frames. So the
generator writes the analytic trajectory parameters beside the pixels.

It is also the only thing in the repo that can answer **S2.T6's own largest open question**. Every
number in the deadzone diagnosis came from `ball_zoo`: a hard-edged, non-antialiased 24 px lattice.
`--bg noise` gives gradient at every pixel with no period; `--bg grating` reproduces the lattice
deliberately, so the two are measured the same way.

## 2 · What it produces

| Output | Content |
|---|---|
| `f_%06d.rgba` | RGBA8 row-major — the format the scorer and `--qdump+` already speak |
| `positions.csv` | `k,marker,class,size,x,y,visible,occluded_frac` |
| `trajectories.json` | the closed forms + the background model — `p_i(t)` at any real `t` |
| `manifest.txt` | `size W H`, `sequence f_ T`, `fps`, `bg`, `seed` |

Six trajectory classes (`linear, accel, circular, crossing, hud, fast`), three marker sizes
(6, 12, 24 px — so the sub-block case below the matcher's 8 px tile is covered), three background
classes (`flat, noise, grating`), and a background pan that arms automatically when the `hud` class is
present (a screen-fixed marker over a moving world is the HUD case).

**The `fast` class is an expected-failure control.** It runs at 12–16 px/frame, outside the 8 px block
matcher's reach, while the others span 0.5–8 px/frame inside it. A run whose fast markers do **not**
show error is a run whose instrument is not measuring — the "gate seen red" the empirical-test
discipline requires before any green is believed.

## 3 · The gate, run against the files on disk

```
zoo: 1280x720 x 60 frames @ 60 fps | bg=noise pan=120px/s | 18 markers, sizes [6, 12, 24] | seed 20260904
VERIFY (reading back from disk):
  barcode: 60/60 frames decode to their own index
  p(t) at t=k/fps vs positions.csv: worst |delta| = 4.994e-10 px
  p(t) at a NON-integer frame (t = 0.5/fps): marker 0 -> (24.8750, 40.0000);
      frame 0 -> (24.0000, 40.0000), frame 1 -> (25.7500, 40.0000)
RESULT: T2 GATE PASSED
```

| Gate criterion | Result |
|---|---|
| frames render | 60/60 at 1280×720, 2.0 s wall clock |
| the barcode decodes losslessly from the **saved** frames | **60/60 decode to their own index** |
| `p(t)` evaluates at non-integer `t` | yes — marker 0 at `t = 0.5/fps` lands at 24.875, exactly between the frame-0 and frame-1 positions |
| `p(t)` agrees with the written table | worst \|Δ\| = **4.99e-10 px** (the gate asks for 1e-6) |

**Checked beyond the gate**, because the plan names them as design requirements:

| Property | Measured |
|---|---|
| no consecutive frame pair is byte-identical (the `ball_zoo` lesson — dedup must not be able to collapse the source) | **0 identical pairs** of 59; the smallest consecutive delta sums to 2,043,658 |
| all six classes and all three sizes present | `{linear:3, accel:3, circular:3, crossing:3, hud:3, fast:3}` · `{6:6, 12:6, 24:6}` |
| per-frame displacement spans the matcher's reach and past it | **1.75 – 15.33 px/frame** |
| markers never intrude on the barcode strip | topmost marker edge **y = 28.00**, the top border is 16 |
| the crossing class actually occludes | **10 rows** with more than 5 % occlusion |

## 4 · Deviation from the plan, recorded

The strategy doc asks for markers with "Hamming distance ≥ 40 bits between any two", stated for the
12×12 case whose interior holds 100 bits. **At size 6 the interior holds 16 bits and 40 is
unreachable.** The floor implemented is `max(4, 40 % of the interior bits)` — which *is* 40 bits at
size 12 and scales honestly at 6 and 24. Patterns are generated at their native size rather than
downscaled from a 12×12 master, because downscaling a binary pattern to 6×6 destroys the distinctness
the constraint exists to guarantee.

## 5 · Honesty ledger

- **The zoo is generated, not yet consumed.** Nothing in the FG has read one of these frames. T3 (the
  player) is what puts them on screen where the capture path can see them; until then this is a file
  writer with a passing self-test, not a measurement.
- `value_noise` smoothsteps its lattice fractions on purpose: plain bilinear leaves lattice-aligned
  creases, which are themselves periodic structure — the exact thing this background must not have.
  That the result is *aperiodic enough* for the deadzone question is a design argument, **not a
  measurement**; nothing here has computed its spectrum.
- `occluded_frac` is computed from an id buffer at a 0.5 coverage threshold. That threshold is chosen,
  not derived.
- The BMP writer exists for a one-time human glance and is default-off. The gate is the CSVs.
- Not measured: generation cost at larger sizes; whether 18 markers is the right density for the NCC
  search radius T4 will use (that is a T4 question and may send this back for a parameter change).

*Made with my soul - Swately <3*
