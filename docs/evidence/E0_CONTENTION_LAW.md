# E0 — the contention law on this RTX 4090, measured

**Date** 2026-09-07 · **Rig** RTX 4090, driver 610.88, Vulkan SDK 1.4.357.0, Windows 11
**Instrument** `cmbench.exe` + `mlp1080_<N>L.spv` (a per-pixel cooperative-matrix MLP head: 8 input
channels/pixel from VRAM, expanded to a 64-wide feature in shared memory, N fully-connected 64x64
layers on `GL_KHR_cooperative_matrix` with ReLU between, 4 channels written back; 16,200 workgroups =
1920x1080). Timing is **device timestamps**, best of 25-30 reps, not wall clock.
**Competitor** `gpu_load.exe --gpu 0 --load 99 --heavy {1,4}`, nvidia-smi-confirmed at **100 % util,
2925-2940 MHz, 483 W** during every contended run.

## Why this was run

The project's whole filter question reduced to one unmeasured quantity: whether the contention penalty
on this GPU is **additive** (a fixed lost scheduling quantum, ~+2 to +7 ms) or **multiplicative** (a
scale factor). Under the additive model an 11 ms style pass becomes 13-18 ms and fits a 33.3 ms budget
at 30 fps; under the multiplicative model it becomes 66-88 ms and does not fit at any rate. Every
measurement taken before today was consistent with BOTH models, because every one of them was a short
dispatch.

## Harness validation, before trusting anything

Two previously-reported figures were reproduced first:

| | previously reported | reproduced here |
|---|---|---|
| 1 layer, 1080p, idle | 0.3772 ms | **0.3761 ms** (+0.3 %) |
| 4 layers, 1080p, idle | 1.4075 ms | **1.3987 ms** (+0.6 %) |

## Result 1 — the penalty is MULTIPLICATIVE

Idle scales cleanly and linearly with layer count, so the shaders really do differ:

| layers | idle ms | contended `--heavy 4` | delta | ratio | contended `--heavy 1` | delta | ratio |
|---|---|---|---|---|---|---|---|
| 1 | 0.3762 | 0.3351 | **-0.04** | **0.89x** | 0.3361 | -0.04 | 0.89x |
| 2 | 0.7222 | 5.8329 | +5.11 | 8.08x | 1.2559 | +0.53 | 1.74x |
| 4 | 1.3980 | 8.3491 | +6.95 | 5.97x | 3.4931 | +2.10 | 2.50x |
| 8 | 2.7667 | 21.8632 | +19.10 | 7.90x | 8.7317 | +5.97 | 3.16x |
| 16 | 5.4641 | 38.5231 | +33.06 | 7.05x | 16.9854 | +11.52 | 3.11x |
| 32 | 9.5798 | 59.5942 | +50.01 | 6.22x | 28.1788 | +18.60 | 2.94x |
| 64 | 19.1066 | 110.3784 | **+91.27** | 5.78x | 52.7054 | +33.60 | 2.76x |

**The delta grows from 5 ms to 91 ms while the ratio stays in a band.** That is the multiplicative
model, and it is not close. The additive hypothesis is REFUTED.

**The competitor sets the factor**, and this is the one degree of freedom left: a coarse competitor
(`--heavy 4`) costs **~6-8x**, a fine one (`--heavy 1`) costs **~2.8-3.2x**. The same dispatch, two
different penalties, from changing nothing but the other workload's submission granularity.

## Result 2 — many small dispatches are WORSE than one big one

The strongest surviving argument for diffusion was that a UNet is not one monolithic dispatch but a
dependent chain of ~100 small ones, and that a sub-0.4 ms dispatch was measured to take no penalty at
all. `cmbench` was extended with an N-dispatch control (`argv[8]`), serialising N copies of the
1-layer shader with a full memory barrier between them — the shape of a UNet layer chain — so that
"many small" and "one big" do the same total work.

| total work | many small (Nx1L) | ratio | one big (NL) | ratio |
|---|---|---|---|---|
| 8 layers | 23.99 ms | **7.96x** | 21.88 ms | 8.22x |
| 16 layers | 47.98 ms | **8.32x** | 38.53 ms | 8.00x |
| 32 layers | 101.16 ms | **9.56x** | 59.53 ms | 6.21x |
| 64 layers | 199.15 ms | **9.40x** | 110.76 ms | 5.78x |

**Splitting the work makes contention worse, not better.** Every barrier is a synchronisation point
the competitor can wedge into. The UNet-sidesteps-the-cliff hypothesis is REFUTED.

## The law, stated for the design

> **Any SUBMIT whose GPU work exceeds ~0.4 ms pays a 3x-8x multiplier under a saturating competitor.
> The threshold is a property of the whole submit, not of each dispatch: a single 0.376 ms dispatch is
> free, and a chain of eight of them is not.**

Measured threshold: free at 0.3762 ms, penalised at 0.7222 ms. Consistent with the ~0.40 ms cliff
found earlier by a different route.

## What this permits and forbids

| tier | GPU work | verdict |
|---|---|---|
| 3D LUT fused as a COMPOSE row in `fg_core.comp` | +0.02-0.05 ms on a 0.08-0.11 ms warp batch (ESTIMATE) | **free** |
| Tier 1b: DoG/Sobel edge + soft quantise + LUT | 0.10-0.20 ms (ESTIMATE) | **free** |
| Tier 1b + separable 5x5 bilateral | 0.30-0.55 ms (ESTIMATE) | **at the cliff — must be measured, not assumed** |
| Cooperative-matrix head, 1 layer, 1080p | 0.3762 ms MEASURED | **free — 0.89x, no penalty** |
| Cooperative-matrix head, 2 layers | 0.7222 ms MEASURED | pays 1.7x-8x |
| One distilled diffusion step, 512x512 | 10.65 ms MEASURED idle (elsewhere) | **30-85 ms with a game running (ESTIMATE from the 2.8x-8x band) — dead at every rate** |

## The one thing still unmeasured

`gpu_load` is a pathological competitor: it submits continuously with no gaps. **A real game has vsync
waits, CPU-bound frames and idle windows, and its factor is unmeasured.** The finest synthetic
competitor gives 2.8x; for a 10.65 ms diffusion step to fit a 33.3 ms budget with the game still
getting useful GPU, a real game would have to cost **~1.5x or less** — half the friendliest synthetic
number. That is the remaining experiment, and the direction of the evidence is not encouraging.

*Made with my soul - Swately <3*
