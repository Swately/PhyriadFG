# E1b — the contention factor against a REAL game, measured

**Date** 2026-09-07 · **Rig** RTX 4090, driver 610.88 · **Instrument** the E0 probe
(`cmbench.exe` + `mlp1080_<N>L.spv`, 16,200 workgroups = 1920x1080, device timestamps, best of 25)
**Game** KovaaK's (`FPSAimTrainer-Win64-Shipping`), launched with the operator's explicit permission
and closed immediately after. Chosen as the lightest installed title — an aim trainer with minimal
geometry, the closest available stand-in for "an old game that barely demands anything".

## Why it was run

[E0](E0_CONTENTION_LAW.md) established that the contention penalty on this GPU is MULTIPLICATIVE, and
measured it at **6-8x** against a coarse synthetic competitor and **2.8-3.2x** against a fine one. It
also stated the one thing it could not settle: `gpu_load` submits continuously with no gaps, and a
real game has vsync waits, CPU-bound frames and idle windows. E0's own closing line set the bar:

> for a 10.65 ms diffusion step to fit a 33.3 ms budget with the game still getting useful GPU, a real
> game would have to cost **~1.5x or less** — half the friendliest synthetic number.

The operator's hypothesis was that the whole picture changes on a light game, and that a light game is
also exactly where frame generation is unnecessary — so the filter would have the device largely to
itself. This measures that.

## Game load, before the probe

nvidia-smi, six samples while KovaaK's ran alone: **35, 12, 36, 37, 35, 37 % utilization**, 2610 MHz,
**85-135 W** (against 483 W under `gpu_load`). This is a genuinely light load, and the low clock is
itself evidence — the GPU is not being asked for much.

## Result

| work | idle ms | with the game | **ratio** |
|---|---|---|---|
| 1 layer (0.38 ms class) | 0.376 | 0.378 | **1.00x** |
| 8 layers | 2.767 | 4.710 | **1.70x** |
| 32 layers (≈ a diffusion step) | 9.580 | 16.865 | **1.76x** |
| 64 layers | 19.107 | 32.487 | **1.70x** |

**A flat ~1.70x, across a 50x range of dispatch sizes.** Compare against the same probe:

| competitor | factor |
|---|---|
| `gpu_load --heavy 4` (100 % util, 483 W) | 5.8-8.2x |
| `gpu_load --heavy 1` (100 % util) | 2.8-3.2x |
| `gpu_load --load 30` | 2.0-2.1x |
| **KovaaK's, a real light game (35 % util, 130 W)** | **1.70x** |

And the sub-cliff dispatch is free even with a game running: **1.00x at 0.376 ms.**

## What it means, arithmetically

Taking the measured 10.65 ms one-step 512x512 diffusion anchor and applying the measured 1.70x:

| styled frames/s | per-frame cost | GPU duty for the filter | + game (35 %) |
|---|---|---|---|
| 30 | **18.1 ms** ESTIMATE | 54 % | 89 % — tight but fits |
| 20 | 18.1 ms | 36 % | 71 % — comfortable |
| 15 | 18.1 ms | 27 % | 62 % — room to spare |

**This is the first configuration in the whole investigation where a diffusion filter is
arithmetically alive.** It required both halves of the operator's reframe: 30 fps as the style rate,
and a light game as the target.

## The honest limits of this measurement

1. **KovaaK's at 35 % is very light.** A medium title at 60-70 % has not been measured and will be
   worse. The factor is a function of the competitor, and E0 proved that with the same dispatch
   costing 3.66 ms or 8.38 ms purely by changing the other workload's granularity.
2. **The probe is a cooperative-matrix compute shader, not a UNet.** It is the right *cost class* and
   the right *contention* test; it is not a diffusion pipeline. VAE encode/decode and any structural
   conditioning are additional and unmeasured here.
3. **The 10.65 ms anchor is itself from elsewhere** (RTX 4090 + TensorRT, batch 1, no game). Applying
   a factor measured on a different shader to a number measured on a different workload is an
   ESTIMATE, and it is labelled as one above.
4. The probe drove the GPU to 100 % while it ran, so this measures a filter competing with a light
   game — which is the intended scenario — not two light workloads coexisting.

## What follows

The product map this supports, and it is the operator's framing rather than this session's:

| game class | GPU | frame generation | the filter |
|---|---|---|---|
| heavy (BF6) | 99 % util; the FG already measures a 0.84x multiplier, a NET LOSS | broken before anything is added | dead — 3-8x |
| light / old | 35 % util, 130 W | **not needed** — these titles already run at hundreds of fps | **viable at 1.70x** |

They do not compete for the same user at the same moment; they divide the library. And on a light
game the FG can be off entirely, which removes the warp — and with it the 1.668 px biased positional
error, the 8x8 MV-grid resolution floor, and the whole question of where to hook the filter, since
there is no warp to hook around.

*Made with my soul - Swately <3*
