# PhyriadFG — Measured results

All numbers in this file were measured first-hand on the rig below. Nothing is
estimated, projected, or borrowed from another tool's numbers. Where a flag or
tool produced the reading it is named; where a measurement was taken at the eye
(subjective) that is stated explicitly.

> `0.1.0-experimental` · one rig · student-built. Treat hardware coverage as
> experimental until tested on additional configurations.

---

## Test rig

| Component | Detail |
|---|---|
| Primary GPU (warp + present) | NVIDIA RTX 4090 |
| Secondary GPU (optical flow) | NVIDIA GTX 1080 Ti |
| Integrated GPU (capture + convert) | AMD iGPU (system-RAM-native) |
| OS | Windows 11 |
| Display | 240 Hz (primary test); 480 Hz (slip / throughput tests) |
| Game used for saturation tests | Battlefield 6 (BF6) — combat scenario |
| Game used for freeze tests | Honkai: Star Rail — heavy particle scene |

---

## 1. iGPU zero-copy bandwidth

The iGPU's VRAM is system RAM. Host-buffer bandwidth was measured with
`--measure-bw` (a direct DMA read from a host-visible buffer, no PCIe hop):

| Path | Measured BW |
|---|---|
| iGPU host-buffer read | **~73 GB/s** |
| PCIe transfer (discrete GPU → system RAM) | ~2 % of that at the frame sizes used |

**Why this matters for the convert/pack step:** the captured frame already lives
in system RAM after DDA delivers it. The iGPU reads and packs it there — no
upload needed. A discrete GPU doing the same step would have to upload over PCIe
first, then download the packed result, paying two crossings. The iGPU pays
zero extra crossings for this one step.

---

## 2. Structural pipeline latency

Measured with `--phaselog` over a 797-pair window (stable game, ~21 fps source,
240 Hz display):

| Component | Measured contribution |
|---|---|
| D-anchor (pair-publish lag, pair not yet available at present time) | **~68 ms** |
| Residual (warp + pacing + present) | **~40 ms** |
| **Total added latency** | **~108 ms** |

The D-anchor is a structural cost of external-capture FG: the intermediate frame
is shown before the real frame it precedes, so the pipeline must hold the pair
long enough to warp to both ends. Reducing it requires a real-fast-path (closing
the pair-publish gap), not faster shaders.

---

## 3. GPU saturation — BF6 combat

Measured live in a heavy combat scene with the 4090 pegged:

| Metric | Value |
|---|---|
| Presented fps (game real frames, no FG) | ~85 fps |
| FG multiplier under saturation | **~1.07×** (near-collapse) |
| Added latency range | 95 – 176 ms |
| 4090 utilization (NVML) | **99 %** |
| 1080 Ti utilization (NVML, after PDH-LUID bug fix) | **23 %** (CPU-starved) |

The multiplier collapsing to 1.07× was the root cause under investigation: the
governor shed work too aggressively when the 4090 saturated, rather than holding
throughput and shedding only non-essential quality.

---

## 4. Upload bottleneck (`wap_upload`)

Measured with `--wsub` (per-frame GPU timestamp pairs on the upload command):

| Path | Measured cost |
|---|---|
| `wap_upload` on the 4090 GRAPHICS queue | **~10 ms / frame** |

This is the single largest contributor to the 4090's per-frame budget at combat
load. It runs host-blocked on the graphics queue, stalling the warp behind it.
The planned fix (moving it to the 4090's parallel transfer queue A.qT) is
implemented but not yet merged.

---

## 5. Make-space — capped vs uncapped

Tested by capping the game's fps externally (via `--cap-fps`) to give the FG a
fixed GPU-time slice:

| Scenario | Presented fps | FG multiplier | Per-frame GPU slice |
|---|---|---|---|
| Uncapped (combat) | ~85 | 1.07× | **4.57 ms** (~73 % fence-wait) |
| Capped to 30 fps | **234 fps** | **7.4×** | **1.22 ms** (true work) |

At 30 fps source the FG has a large headroom window and achieves ×7.4
multiplication. The "uncapped 4.57 ms" was not real GPU work — ~73 % of it was
fence-wait (the pipeline stalled behind the game's draw commands).

**Finding:** when the GPU is saturated, the FG's multiplier collapse is
fence-contention, not shader cost. Capping the game to leave headroom is the
primary lever.

---

## 6. Capture ring — freeze vs slot count

The capture ring holds real frames in system RAM. Measured by sweeping
`--cap-fps` (source rate) and observing `frz` (freeze counter, frames/s where
no real frame was available):

| Source rate (`--cap-fps`) | Ring slots auto-selected | Freeze rate |
|---|---|---|
| 1000 fps (stress) | 22 slots | **frz ≈ 0** |
| 120 fps | 6 slots | frz ≈ 0 |
| < 4 slots at 240 Hz display | — | frz 150 – 240 / s (Star Rail) |

The auto-size formula (`src_ceil / 55 + 4`, clamped [4, 32]) was derived from
these measurements. Below 4 slots at 240 Hz the ring laps the real-frame
delivery rate and the freeze counter spikes.

**Star Rail heavy scene:** freezes dropped from **150 – 240 / s → 0** after
switching from 4 hard-coded slots to the auto-sized ring + the deficit-tier
governor.

---

## 7. Quality layer overhead

Each layer is opt-in; its overhead was measured as the delta to the warp
command time with the flag on vs off (byte-identical off path):

| Layer | Flag | Overhead |
|---|---|---|
| Temporal smoothing | `--ts-smooth 0.1` (default) | **0 fps delta** (fps 242.4 ON == OFF) |
| Candidate selection (medoid) | `--multicand` | **+0.05 ms** on warp |
| Content-clock sc-select | `--sc-select` (default on) | no measurable fps cost |

`--ts-smooth` and `--multicand` were validated byte-identical-off (the off path
produces the same output as the pre-flag binary, confirmed by frame diff).

---

## 8. Content-clock quality (`--sc-select`)

`--sc-select` fixes a two-clock drift between the phase selector and the NCO
(the selector used `t_display`, the NCO used `content_clock` — two independent
clocks that truncated the phase ladder):

| Metric | Before (`--sc-select` off) | After (on) |
|---|---|---|
| Median phase top (ladder sweep 0 → 1) | 0.89 | **0.96** |
| Start-stalls (frames stalled at phase 0) | 51 / window | **0** |

No fps regression. The fix is a read-side selector swap; it does not change the
NCO or the warp.

---

## 9. Thread pinning (`--pin`) — result: no effect on slip

A CPU-placement audit hypothesised that the ~1 frame slip at 480 Hz was
thread-scheduling jitter. After implementing `--pin` (binds C/F/P to fixed
cores using the topology pillar):

| Metric | Unpinned | Pinned (`--pin`) |
|---|---|---|
| Per-tick iteration time | ~2.08 ms @ 480 Hz budget | **~1.9 ms** |
| Slip | present at both | **no change** |

**Finding:** the slip is GPU/fence-bound, not CPU-placement. `--pin` is kept as
an opt-in scalability tool for hybrid-CPU machines but is not the slip fix.

---

## 10. Async present (`--async-present`) — result: no latency win

`--async-present` decouples the present fence from the generation loop (non-
blocking `vkGetFenceStatus` + drop-interpolated on miss):

| Metric | Default | `--async-present` |
|---|---|---|
| Added latency | ~105 ms | **~98 ms** |

The improvement is real but not the structural win needed. The dominant cost
remains the D-anchor (~68 ms), which `--async-present` does not reduce.

---

---

## 11. BF6 saturation — stable vs saturated regime

Measured from `bf6_p1_baseline.csv` (first-hand, BF6 max settings, 4090 at 99% in combat).
Tool: render_assistant `--csv` telemetry (`warp_ms` / `iter_ms` / `MsAddedLatency` /
`GPUUtilization_4090`) + `nvidia-smi` (NVML).

| Metric | Stable regime | Saturated regime (combat) |
|---|---|---|
| Presented fps | **240 fps** | **~100 fps** |
| AddedLatency | **24 ms** | **24 → 85 ms** (p95 124 ms, max 153 ms) |
| Slip | **0.06 ms** | **5.3 ms** |
| Present interval CoV | low | **0.65** (erratic) |
| Warp time (`warp_ms`) | 1.2 ms | **5.2 ms** |
| 4090 utilization | moderate | **99%** |
| 1080 Ti utilization | normal | **31%** (CPU-starved) |

The saturation symptom is **variance**, not just the mean. The slip jump (0.06→5.3 ms) is the
felt "heavy / unstable mouse". Root cause: the present thread blocks synchronously on the warp
fence (`vkWaitForFences UINT64_MAX`) — under a pegged 4090 the warp balloons and the present
inherits its variance verbatim.

---

## 12. PhyriadFG vs LSFG — head-to-head (HSR, single-GPU)

Measured 2026-06-22. Scene: Honkai: Star Rail (GPU-light, hardlocked 120 fps base,
PhyriadFG `--force-single-gpu`). Tool: PresentMon 2.4.1 + `--csv` internal telemetry.

| Metric | LSFG | PhyriadFG (default) |
|---|---|---|
| Present mode | Hardware Composed: Independent Flip | Composed: Flip (DComp overlay) |
| Output rate | **240 fps locked** (MsBetweenDisplayChange med 4.167 ms) | 256–284 fps (no lock) |
| Pacing — MsBetweenDisplayChange MASD | **0.026 ms** | **12.7 ms** |
| Per-frame GPU cost (MsGPUBusy) | **0.86 ms** | **3.36 ms** (internal iter_ms: 2.73 ms) |
| AddedLatency | unmeasurable via PresentMon (see §methodology) | **43.7 ms** |
| 4090 load (light HSR scene) | — | **95% util / 294 W** (game alone: 38% / 185 W) |

**Gap diagnosis:**
- **Cost gap (3–4×):** per-frame GPU cost is the root of the saturation collapse.
- **Pacing gap (~490×):** architectural — DComp overlay ("Composed: Flip") vs LSFG's own
  Independent Flip plane. Not tuneable away; the LSFG topology difference is documented.

**Methodology finding:** `MsAllInputToPhotonLatency` was empty on 100% of rows for both LSFG
and PhyriadFG — the FG frames carry no game input, so PresentMon cannot attribute input→photon
for either tool. Pacing (MsBetweenDisplayChange) is the correct FG-agnostic metric.

---

## 13. `--flow-scale` sweep — cost vs quality

Measured on HSR single-GPU (internal telemetry medians, same session as §12):

| Config | warp_ms | iter_ms | AddedLatency | 4090 util | Power |
|---|---|---|---|---|---|
| flow-scale 1 (default) | 1.41 | **2.73** | 43.7 ms | **95%** | **294 W** |
| **flow-scale 2** ★ | 0.47 | **0.94** | **13.2 ms** | **56%** | **138 W** |
| flow-scale 4 | 0.70 | 1.43 | 15.4 ms | 57% | 137 W |
| flow-scale 2 + target-output-fps 240 | 0.69 | 1.21 | 17.0 ms | 89% | 271 W |
| *(LSFG reference)* | — | *0.86 ms* | — | — | *251 W* |

**`--flow-scale 2` is the cost sweet spot:** iter 2.73 → 0.94 ms (near LSFG's 0.86 ms), power
294 → 138 W (−53%), utilization 95 → 56%. Flow-scale 4 is worse than 2: the coarser MV grid
costs back more in warp/upsampling than the tile reduction saves.

**Trade-off (eye-gated):** coarser flow raises `gme_dissidence` 0.02 (fs1) → 0.15 (fs2) →
0.74 (fs4). Quality cost is an operator-eye call; under saturation fs2 is unconditionally
better than the default collapse.

**`--target-output-fps 240` is inert on stable HSR:** output stayed at 270 fps, and the
async-present it auto-enables added load back (56 → 89% util). It is a saturation-regime
lever, not a stable-regime one.

---

## 14. Pacing improvement — `--present-waitable --present-sync 1`

Measured (committed `c63b5c1`). Still Composed: Flip — does not reach Independent Flip:

| Metric | Default | `--present-waitable --present-sync 1` |
|---|---|---|
| MASD (MsBetweenDisplayChange) | 12.7 ms | **7.4 ms (−42%)** |
| p99 present interval | 174 ms | **67 ms** |
| Output mode | Composed: Flip | Composed: Flip (unchanged) |

Partial improvement; byte-identical-off confirmed.

---

## 15. LSFG power behavior — regime-dependent (measured via `nvidia-smi`)

Refutes the naive "LSFG lowers GPU consumption" claim:

| Regime | LSFG-OFF power | LSFG-ON power | Direction |
|---|---|---|---|
| GPU-light (HSR locked 120 fps) | 185 W | **251 W** | **+36 W — addition** |
| GPU-bound (BF6, uncapped 110–130 fps) | 573 W | **493 W** | **−80 W — reduction via pacing** |

**Mechanism (GPU-bound case):** LSFG owns the present → paces the racing render → it stops
burning max power chasing uncapped fps. The game still renders at 120 Hz in both cases
(hardlock). The "LSFG lowers consumption" claim is true only in the GPU-bound+racing regime,
via pacing control, not downscaling.

**Verification:** game GPU cost with LSFG-OFF 2.97 ms vs LSFG-ON 3.07 ms → **identical**.
LSFG does not touch the game's render resolution. Its "Flow Scale" knob reduces LSFG's own
optical flow resolution, not the game render.

---

## Honest scope of these numbers

- All measurements are on **one rig**, one Windows configuration.
- GPU utilization via NVML; frame times via Vulkan GPU timestamps (`--phaselog`,
  `--wsub`); fps via the live overlay counter.
- "Byte-identical off" means a frame-diff against the pre-flag binary confirmed
  no output change when the flag is off — not a formal test suite.
- The 4090's stuck-clock issue observed early in development was a transient
  P-state cleared by reboot, not an Afterburner or driver bug.
