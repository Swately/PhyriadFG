# E1d — the whole stack, measured: game + frame generator + a filter-class pass

**Date** 2026-09-07 · **Rig** RTX 4090, driver 610.88 · **Game** Battlefield 6, minimum settings,
**capped to 30 fps**, run by the operator on his own machine · **FG** `build-release/phyriad_fg.exe`
at defaults, bound by `--window-pid`, `--duration` bounded runs · **Probe** the E0 cooperative-matrix
head at 1080p (device timestamps, best of 25)

## 1. The frame generator from a 30 fps source — the thing this session doubted

This session wrote, in the E1c record: *"what PhyriadFG's frame generator can do from a 30 fps source
is unmeasured and doubtful ... interpolating 30 -> 240 would be 8x, far beyond anything measured."*
The operator said he had just tried it and it worked, and asked for the numbers anyway.

**It sustains 8x, cleanly.** Quoted from the run:

    [ra] 240.1 fps (present) | wap tick 240/s (arr 29) | cap 29/s | cons 29/s
         | uniq 240/s fresh:240/s | frz 0.0/s | warp 4.04ms | iter 4.17/worst 4.41ms
         | lat 28.8ms | slip 5.90/max 6.17ms | gme(dis:0% fit:3.48ms) | ps 240/s ok=5760 to=0

| | value |
|---|---|
| Source arrival | **29-32 fps** (the 30 cap) |
| Presented | **240.0-240.1 fps** |
| **Multiplier** | **7.5x - 8.3x** |
| Unique / fresh presented frames | **240/s and 240/s** — every one |
| Frozen frames | **0.0/s** |
| Tick deadline | `iter 4.16-4.17 ms` against a 4.167 ms budget, **worst 4.29-4.41 ms** |
| Added latency | **28.8-32.3 ms** (against 82.8 ms MEASURED on BF6 at defaults) |
| Dropped presents / timeouts | `ok=5760 to=0` |
| Total run | 5991 presents in 25 s, clean bounded exit |

**The doubt was wrong and the correction belongs in the record.** The reasoning behind it — that
inter-frame displacement at 30 fps would exceed what an 8x8-grid block matcher handles — is not
refuted by this; what is refuted is the conclusion. These are PIPELINE-HEALTH metrics: they prove the
FG delivers distinct, fresh frames on time at 8x. They do **not** prove the interpolated content is
*correct* at that displacement. The instrument for that is M1 (marker-position motion truth) on the
synthetic zoo, and it has never been run against a 30 fps source.

One observation worth carrying: `slip` climbed monotonically across the run — 3.20, 3.60, 3.97, 4.36,
4.76, 5.14, 5.52, 5.90 ms, about +0.38 ms/s. It did not affect delivery in 25 s. Over a long session
it might, and nobody has looked.

## 2. What the frame generator costs, here

| configuration | GPU util | power | VRAM |
|---|---|---|---|
| BF6 @ 30 alone | **6-7 %** | 86-87 W | 11.34 GB |
| BF6 @ 30 + PhyriadFG @ 240 | **25-26 %** | 94-95 W | 11.53 GB |
| **delta** | **+19-20 points** | **+9 W** | +190 MB |

+9 W for +20 points of utilization says this workload is latency-bound, not compute-bound — it
occupies the device without heating it.

## 3. The real deployment scenario — a filter-class pass on top of BOTH

E1c measured the contention factor against the game alone. That is not the shipping configuration:
the filter would run alongside the game **and** the frame generator. Measured with both live:

| work | idle ms | vs (BF6@30 + FG@240) | ratio |
|---|---|---|---|
| 1 layer (0.38 ms) | 0.376 | 0.377 | **1.00x** |
| 8 layers (2.77 ms) | 2.767 | 2.504 | **0.91x** |
| **32 layers (9.58 ms — the diffusion-step class)** | 9.580 | 9.805 | **1.02x** |
| 64 layers (19.1 ms) | 19.107 | 22.119 | 1.16x |

**And the frame generator did not degrade while the probe consumed the GPU:** 240.8-242.5 fps
presented, `uniq` and `fresh` 241-243/s, `frz 0.0/s`, `cap`/`cons` 32/s, `iter` 2.39/worst 2.92 ms —
comfortably inside the 4.167 ms deadline. Utilization during the probe read 99 %, 231 W.

## 4. The whole budget

| component | duty | basis |
|---|---|---|
| BF6 @ 30 fps, minimum | **6 %** | MEASURED |
| PhyriadFG @ 240 Hz, 8x multiplier | **+20 %** | MEASURED |
| A 512x512 one-step style pass at 30/s | **+32 %** | ESTIMATE: 30 x 10.65 ms, the anchor measured elsewhere, x the 1.02 factor measured here |
| **total** | **~58 %** | leaves ~42 % unused |
| the same at 960x540 (~21 ms ESTIMATE) | +63 % | **~89 %** — fits |

**Every leg of the stack is now measured except the diffusion model itself.** The remaining estimate
is the 10.65 ms anchor, which comes from a different workload (RTX 4090 + TensorRT, batch 1) and is
combined with a contention factor measured on a different shader. That is the one number E2 still owes.

## 5. What is still not known

1. **Image quality of the 8x interpolation.** Health metrics say the frames arrive; M1 says whether
   they are in the right place, and M1 has not been run from a 30 fps source.
2. **What the game loses.** Every figure here is the cost to PhyriadFG and to the probe. Nobody has
   watched BF6's own delivery while both ran.
3. **The 30 fps cap is a real cost to the player**, paid before any of this. These measurements say
   the filter is free *given* the cap; they do not say the cap is free.
4. **The probe is not a UNet.** Right cost class, right contention test, wrong workload shape.
5. **`slip` drift** over long sessions (section 1).

*Made with my soul - Swately <3*
