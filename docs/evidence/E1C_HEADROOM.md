# E1c — the contention factor is a function of the GAME'S CAP, and at 30 fps it disappears

**Date** 2026-09-07 · **Rig** RTX 4090, driver 610.88 · **Instrument** the E0 probe (`cmbench.exe` +
`mlp1080_<N>L.spv`, 16,200 workgroups = 1920x1080, device timestamps, best of 25)
**Game** Battlefield 6, minimum settings, run by the operator on his own machine at two frame caps.

## The framing correction that produced this measurement

This session had been modelling the budget as *"the filter costs X % of the GPU, the game costs Y %,
and X+Y must be under 100 — so shave the styled rate until it fits."* The operator corrected it:

> *"el presupuesto no es que bajemos los fps o bloqueemos el juego, es lo que entregaria el filtro,
> por que por ejemplo bf6 bloqueado a 30 tiene una holgura extrema"*

He is right, and the correction has two parts. First, the styled rate is **not a free parameter to
shave**: you cannot style more frames than the source produces, so a 30 fps game needs exactly 30
styled frames per second and no more. Second, capping the game is **not a sacrifice made to fit the
filter in** — it is what creates the headroom, and the right question is what the filter DELIVERS
inside that headroom, not how small it can be made.

## The measurement

Same probe, same rig, three competitors, in ascending order of how much GPU the game is allowed:

| competitor | game GPU util | game power | 1L (0.38 ms) | 8L (2.77 ms) | 32L (9.58 ms) | 64L (19.1 ms) |
|---|---|---|---|---|---|---|
| `gpu_load --heavy 4` | 100 % | 483 W | 0.89x | 7.90x | 6.22x | 5.78x |
| `gpu_load --heavy 1` | 100 % | — | 0.89x | 3.16x | 2.94x | 2.76x |
| KovaaK's (aim trainer) | 35-37 % | 130 W | 1.00x | 1.70x | 1.76x | 1.70x |
| BF6, min settings, **240 fps cap** | 44-50 % | 210-232 W | 0.89x | 1.74x | **1.87x** | 1.85x |
| **BF6, min settings, 30 fps cap** | **6-7 %** | **87 W** | **1.00x** | **1.00x** | **1.00x** | **1.00x** |

**At a 30 fps cap the penalty vanishes entirely.** Measured 9.581 ms against an idle 9.580 ms at the
diffusion-step work class, and 19.162 against 19.107 at four times that. Repeated (DI-3): 9.605 and
19.160, a run-to-run spread of 0.00x.

## What this changes

The filter runs **at full idle speed**. Taking the measured 10.65 ms one-step 512x512 anchor with no
contention multiplier at all:

| | value |
|---|---|
| Styled frames needed per second | **30** — one per captured frame, and no more is useful |
| Cost per styled frame | **10.65 ms** (MEASURED elsewhere, RTX 4090 + TensorRT, batch 1) |
| GPU duty for the filter | 30 x 10.65 = 320 ms/s = **32 %** |
| Game duty | **6-7 %** MEASURED |
| **Total** | **~38 %** — leaving ~62 % of the device unused |
| Deadline per frame at 30 fps | 33.3 ms; the filter uses 10.65 | **3.1x faster than required** |

The surplus is the design freedom, and it should be spent on quality rather than left on the table:

| where to spend it | cost | duty at 30/s | total with the game |
|---|---|---|---|
| style at 512x512 (the baseline) | 10.65 ms | 32 % | 38 % |
| style at 960x540 (~1.98x pixels) | ~21 ms ESTIMATE | 63 % | ~70 % |
| style at 768x768 (~2.25x pixels) | ~24 ms ESTIMATE | 72 % | ~79 % |

Styling at 960x540 and letting the warp's own bilinear fetch upscale 2x to 1080p is a far smaller
quality loss than styling at 512x512 and upscaling 3.75x — and E1c says it fits.

VRAM also fits: BF6 holds **11.34 GB** MEASURED, leaving ~12 GB. SD-Turbo is 2.58 GB at FP16, or
**1.90 GB without the text encoder**, which the plan already deletes by precomputing the presets'
conditioning tensors.

## The honest limits

1. **What the GAME loses is still unmeasured.** Every number here is the cost to the FILTER. The duty
   arithmetic is the proxy; nobody has watched BF6's own frame delivery while the probe ran.
2. **A 30 fps cap is a real cost to the player**, paid before the filter does anything. This
   measurement says the filter is free *given* that cap; it does not say the cap is free.
3. **The probe is a cooperative-matrix shader, not a UNet.** It is the right cost class and the right
   contention test, not a diffusion pipeline. VAE encode/decode and structural conditioning are extra.
4. **The 10.65 ms anchor is from a different workload** (elsewhere-measured, TensorRT). Combining it
   with a factor measured on this shader is an ESTIMATE and is labelled so.
5. **What PhyriadFG's frame generator can do from a 30 fps source is unmeasured and doubtful.** Its
   measured multiplier on BF6 at defaults was 0.84x, and 2.74-2.78x with `--gpu-priority realtime`.
   Interpolating 30 -> 240 would be 8x, far beyond anything measured, and inter-frame displacement at
   30 fps is much larger than the 59-61 pairs/s the flow stage was tuned against.

*Made with my soul - Swately <3*
