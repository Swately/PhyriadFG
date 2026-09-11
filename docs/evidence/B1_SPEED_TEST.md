# B1b — the speed test, and the first provenance reading

> **DEFAULT CHANGE, 2026-09-11 — every number below predates it.** `single_track`'s screen-static ramp
> shipped as `smoothstep(1.2, 3.0, ...)` when these rows were measured; it now ships as
> `smoothstep(1.0, 1.6, ...)` (`hold_lo` / `hold_hi`, P-042 in `docs/LEARNING_LOG.md`). Under the new
> default the same corpora score BETTER on hallucinated mass everywhere (-8 % to -54 %), better on the
> spinning box's position at 6.7 and 13.4 px/pair (-15 %, -25 %), and WORSE on the sphere's position at
> 6.7 px/pair (+11 %). **To reproduce a row on this page exactly, pass `--st-hold-lo 1.2 --st-hold-hi 3.0`.**
> These rows are not re-measured: re-running the twelve-corpus record is its own job.

**Date:** 2026-09-09 · **Tools:** `tools/scene_truth/` (`--speed`, `scene_speed.py`), `tools/ref_warp.py --decisions`
**Binary:** `build-release/phyriad_fg.exe` at `f2e4a9a`, default kernel · **Scene:** `mixed`, 640×360, base
240 fps, seed 7 (seed 11 for the k = 4 DI-3 point) · **Protocol:** as `B1_FIRST_FG_ROW.md` — looped
player, `--qdump`, truth rendered at the FG's own phase, cut pairs excluded, one arm directory per k.

---

## 1. The question, as the operator put it

*Speed increases hallucinations enormously; find the relation — and whether there is a limit set by
the frames per second that capture the motion: "the number of times or exposure it has to the
frames".* Stated as a measurable: the FG is given two real frames per pair, so what it can know about
the motion between them is bounded by **how far the content moved between those two frames** —
displacement per source pair, in pixels. That quantity is reached two ways, and the test runs both:

- **the k path** — same scene, same speed, every k-th base frame shown (k = 2, 4, 8)
- **the speed path** — k fixed at 4, every object's motion ×0.5, ×1, ×2, ×4

If the FG's error at matched displacement is the same on both paths, it sees only pixels-per-pair and
the "exposure" is the whole story. If the k path is worse, the source rate costs something on its own.

## 2. Every live point on one axis (sphere = object 1, box = object 3)

| obj | corpus | seed | k | speed | n | **disp px/pair** | pos px | shape px | halluc px² | halluc/area | lead px | missing px² |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | sc_v05 | 7 | 4 | 0.5× | 96 | **1.68** | 0.355 | 0.230 | 67 | 0.0048 | +47.8 | 34 |
| 1 | sc_live | 7 | 2 | 1× | 5 | **1.68** | 0.314 | 0.153 | 64 | 0.0049 | +45.6 | 25 |
| 1 | sc_live | 7 | 4 | 1× | 133 | **3.36** | 0.481 | 0.292 | 76 | 0.0056 | +48.4 | 59 |
| 1 | sc_live2 | 11 | 4 | 1× | 128 | **3.36** | 0.468 | 0.279 | 77 | 0.0057 | +47.4 | 53 |
| 1 | sc_live | 7 | 8 | 1× | 168 | **6.72** | 0.631 | 0.366 | 116 | 0.0086 | +46.6 | 71 |
| 1 | sc_v2 | 7 | 4 | 2× | 129 | **6.72** | 0.544 | 0.273 | 72 | 0.0068 | +29.7 | 56 |
| 1 | sc_v4 | 7 | 4 | 4× | 109 | **13.28** | 0.690 | 0.396 | 98 | 0.0096 | +24.0 | 65 |
| 3 | sc_live | 7 | 2 | 1× | 5 | 0.74 | 0.071 | 0.075 | 40 | 0.0018 | −19.2 | 7 |
| 3 | sc_v05 | 7 | 4 | 0.5× | 96 | 0.88 | 0.167 | 0.108 | 46 | 0.0025 | −30.2 | 20 |
| 3 | sc_live | 7 | 4 | 1× | 133 | 1.61 | 0.159 | 0.135 | 76 | 0.0037 | −22.8 | 17 |
| 3 | sc_live2 | 11 | 4 | 1× | 128 | 1.62 | 0.160 | 0.124 | 72 | 0.0036 | −20.9 | 16 |
| 3 | sc_v2 | 7 | 4 | 2× | 129 | 2.93 | 0.262 | 0.186 | 107 | 0.0057 | −24.0 | 39 |
| 3 | sc_live | 7 | 8 | 1× | 168 | 3.26 | 0.291 | 0.202 | 129 | 0.0065 | −22.7 | 34 |
| 3 | sc_v4 | 7 | 4 | 4× | 129 | 6.08 | 0.518 | 0.258 | 157 | 0.0084 | −22.1 | 85 |

The k = 2 point is **thin** (5 frames): at a 120 fps source `--qdump`'s phase-bin balancing starves and
yields 12 triples in 20 s against 400 at k = 8. It is shown, and it is not leaned on; the matched
low-displacement point is the speed path's 0.5× (96 frames).

## 3. What the axis says

**Displacement per pair explains most of it, and the growth is gentle.** On the speed path the sphere's
position error goes 0.355 → 0.481 → 0.544 → 0.690 px as displacement goes 1.68 → 3.36 → 6.72 → 13.28:
an 8× displacement costs 2× the error. A power-law fit gives **pos ≈ 0.30 · disp^0.32** — the error grows
as roughly the cube root of the displacement, not proportionally. Hallucinated mass as a fraction of the
object doubles over the same range (0.0048 → 0.0096). **There is no cliff up to 13 px per pair on this
content.** The operator's "enormous" increase is not a threshold in this range; it is a smooth, sub-linear
rise — with the caveat in §6 about what this content is.

**But the two paths do not coincide.** At 6.72 px/pair the k path (k = 8 at 1×) is worse than the speed
path (2× at k = 4): position 0.631 vs 0.544 (**1.16×**), hallucinated fraction 0.0086 vs 0.0068
(**1.26×**). Since k = 8 has seven intermediates and reaches phases the k = 4 ladder never visits, the
comparison was repeated at **matched phase bands**:

| phase band | k = 8 @ 1× | 2× @ k = 4 | ratio k / speed |
|---|---|---|---|
| 0.25 – 0.75 | 0.666 (n 97) | 0.605 (n 91) | **1.10×** |
| < 0.25 | 0.828 (n 38) | 0.621 (n 19) | 1.33× |
| > 0.75 | 0.302 (n 33) | 0.176 (n 19) | 1.72× |

The gap survives phase matching. So the "exposure" is the first-order term and there is a **second,
smaller term that depends on the source rate itself**: at the same pixels-per-pair, a coarser source
(30 fps → k = 8) costs ~10 % more in the middle of the pair and more at its ends than a faster object at
60 fps. The candidates are the parts of the kernel that carry temporal state across pairs — the PLL
clock, inertia/persistence, the gme history — all of which see fewer, farther-apart samples at k = 8.
Not attributed; named.

**The phase profile changes shape with displacement.** At 0.5×–2× the sphere's error falls monotonically
with phase (the `M1_LOWPHASE` signature); at 4× (13 px/pair) the middle band (0.814) exceeds the low band
(0.758) while the high band stays low (0.267). At large displacement the middle of the pair becomes the
hardest place, not its start.

## 4. The first provenance reading — why the spurious mass leads

`ref_warp.py --decisions` was run on the four worst genuine frames of the k = 4 run (#54 φ 0.38, #61,
#93, #117 at φ ≈ 0.13), reading the stage bitmask and the store weight **on the sphere** and **on a
one-pixel rim outside it** (where hallucinated mass sits):

| frame | on the sphere: guided→linear fallback | stasis | store weight w_s | on the rim: fallback | stasis | w_s |
|---|---|---|---|---|---|---|
| #54 | 99 % | 18 % | 0.18 | 87 % | 0 % | 0.00 |
| #61 | 99 % | 18 % | 0.18 | 92 % | 0 % | 0.01 |
| #93 | 99 % | 18 % | 0.18 | 94 % | 0 % | 0.01 |
| #117 | 99 % | 18 % | 0.18 | 87 % | 2 % | 0.02 |

Three readings, all from planes the oracle computed and would otherwise have discarded:

1. **The guided MV pick falls back to the bilinear MV on 99 % of the sphere and ~90 % of its rim.** The
   guided pick exists to snap a pixel to the MV of the block whose colour it matches — precisely the
   defence against a block field interpolated across a silhouette. On this content its similarity
   threshold rejects almost everything, so the rim is warped with a **bilinear MV across the motion
   discontinuity** — the 8–16 px band `LEARNED_AA.md` §6 named as wrong on both sides.
2. **Stasis fires on 18 % of the sphere.** Stasis stores the NEXT frame at zero motion where the
   zero-motion SAD is small; on a flat-shaded object interior it is small, so a fifth of a moving
   object is stored from the *next* real frame **at the next frame's position** — ahead of where the
   object is at phase t. **That is the sign of the lead**: the FG's spurious mass leads the motion
   (+47 px along it) because part of the object is painted where it will be, not where it is.
3. **The rim is pure backward warp** (w_s ≈ 0): its pixels come from NEXT at the MV-displaced coordinate,
   with no zero-motion mixing — so whatever error the bilinear MV carries at the silhouette goes
   straight into the rim.

These are readings, not proofs of cause. Each names a knob, and the knobs are live flags: an A/B with
stasis off, and one with the guided pick's threshold opened, on this corpus, would turn the reading into
an attribution. That is the shape of the operator's first measuring session.

## 5. Reproducibility

Live corpora (`sc_live`, `sc_live2`, `sc_v05`, `sc_v2`, `sc_v4`) with their `qdump_k*/` and `arms/fg_k*/`
are marked KEEP under the session scratchpads; the FG output arms are the irrecoverable part. Per-run
tables: `B1_FG_k2.md`, `B1_FG_k4.md`, `B1_FG_k4_seed11.md`, `B1_FG_k8.md`, `B1_FG_k4_speed05.md`,
`B1_FG_k4_speed2.md`, `B1_FG_k4_speed4.md`. The oracle accepts every triple of these runs with no
refusals and reproduces them at 99.45 % exact / 100 % within 1 LSB (k = 1.000, corr 1.000).

## 6. What is not established

- **One seed on the speed path.** The k = 4 point has DI-3 (seeds 7 and 11 within 13 %); the 0.5×, 2× and
  4× points are single runs. The matched-pair gap (1.10–1.72×) is larger than the k = 4 seed spread
  (2.6 % on the sphere), which is why it is reported — but it is owed its second seed.
- **The content.** Large, flat-shaded, smoothly moving objects on a textured backdrop. Thin features,
  high-frequency texture and objects narrower than an 8-px block will meet the matcher's limits sooner;
  the "no cliff to 13 px/pair" holds for this content and is not a statement about a game.
- **No ablation yet.** §4 names mechanisms; nothing has been switched off to confirm them.
- **The sampler.** Every row is a subset of ticks on the sync present path (`--qdump`), and at k = 2 the
  subset is 5 frames. The file backend (B2) is what makes every tick scoreable.

---

*Made with my soul - Swately <3*
