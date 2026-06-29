# FG_TELEMETRY_PRIOR_ART.md — state-of-the-art dossier (comprehensive frame-generation telemetry: what to measure, the exact schemas, the derived metrics, what no tool can see)

Status: `0.1.0-experimental` · Type: **Analysis** (prior-art dossier + a build
contract, per [FDP §3](../canon/FORMAL_DOCUMENT_PROTOCOL.md)). Access date
**2026-06-16**.

**Scope.** This dossier answers one question for PhyriadFG (the external-capture frame
generator, `apps/render_assistant`): *what is the complete, SOTA-grounded telemetry an
FG should emit*, so we can export a per-frame CSV that (a) is **compatible** with
PresentMon / CapFrameX on every column those tools also define, (b) **adds** the
pipeline-internal signals no external tool can see, and (c) lets us compute the
standard derived frame-pacing metrics (1% low, stutter, percentiles) the way reviewers
do. It is the **build contract** for the `--csv` export.

**Non-scope.** Not an implementation; the code lives in `apps/render_assistant`. It
states no new *measured* PhyriadFG number — only the cited tools' own facts and our
verified reading of their schemas.

**Method.** A 6-angle deep-research-harness pass (PresentMon, CapFrameX, the derived
metrics, FG/latency, system telemetry, CSV formats; 54 claims → 48 survived / 6 killed,
13 protocol-loaded agents). **Honesty note (load-bearing):** the adversarial-verify
stage **failed on 4 of 6 angles** (a transient socket error), so those angles' claims
carried only the search agent's self-leveling. Per the FDP verifiable-reference
mandate, the supervisor **re-fetched the load-bearing sources first-hand** on the
access date (the exact PresentMon column lists, the NVML signatures/units, the
CapFrameX 1%-low definitions) — those are **[V1]** below; anything not re-confirmed
first-hand is marked lower and flagged.

**Level legend** (FDP §2.3): **[V1]** primary, re-fetched + confirmed first-hand this
pass; **[V2]** primary reached but detail partial; **[V3]** secondary / harness-only,
not re-confirmed first-hand.

---

## §1 — Verdict (the build, in one paragraph)

PresentMon/CapFrameX compatibility is achievable **only on the present-boundary
timing, latency, and system-telemetry columns**; every pipeline-internal PhyriadFG signal
(flow/warp/pack ms, cross-device transfer, ring occupancy, freeze, pair cadence) is
**invisible to all existing tools** and must be defined as native extensions (F14
[V1]). So the export is: **(1)** one CSV **row per presented frame**, leading with
PresentMon's **exact console-frontend column names** for the subset we can populate —
anchored on **`FrameType`** (Real vs Synthesized), `MsBetweenPresents`,
`MsUntilDisplayed`, `MsBetweenDisplayChange`, `DisplayedTime`, `MsRenderPresentLatency`,
the `MsGPU*` block — so PresentMon's own analysis and CapFrameX's metric engine apply to
those columns unchanged; **(2)** append the **architecture-specific** columns
(`flow_ms`, `warp_ms`, `pack_ms`, `h2d_ms`, `d2h_ms`, `ring_occ`, `frz`, `pair_age_ms`,
`source_fps`, `slip_ms`, `gme_dissidence_pct`, `route_device`) that have no standard
name; **(3)** attach **per-GPU system telemetry** under PresentMon's capture-app names
(`GPUUtilization`/`GPUPower`/`GPUTemperature`/`GPUFrequency`/`GPUMemorySizeUsed`…),
**suffixed per device** (`_4090`/`_1080Ti`/`_iGPU`), from NVML (the two NVIDIA cards) +
PDH (the AMD iGPU). Two hard schema facts: the **console** frontend calls the frame
interval `MsBetweenPresents` while the **capture** frontend calls the identical concept
`FrameTime` — **not interchangeable**; we pick the **console** vocabulary and document
it (F2 [V1]). And `FrameType` is **gated** behind `--track_frame_type` in PresentMon —
without it all frames look equal and FPS is inflated by the FG multiplier; **we always
emit it** (F3 [V1]). Derived metrics (1% low, 0.1% low, stutter, percentiles) live in a
**separate stats file** (mirroring pmcap's two-file output, F13), computed post-hoc;
the "1% low" has **two competing definitions** that must both be reported with the
method named (F10 [V1]). **Hard real-time constraint:** the present loop must **never**
call NVML/PDH or do file I/O — it appends a QPC-stamped struct to a lock-free ring; a
dedicated low-priority thread drains it, queries telemetry (~10–50 Hz), and writes the
CSV.

---

## §2 — Findings (each with its source + verification level)

### F1 — PresentMon 2.x console CSV column set (the compatibility anchor) `[V1]`
Re-fetched first-hand: the console app's default columns, verbatim — **Application,
ProcessID, SwapChainAddress, PresentRuntime, SyncInterval, PresentFlags, AllowsTearing,
PresentMode, FrameType, CPUStartTime, MsCPUBusy, MsCPUWait, MsGPULatency, MsGPUTime,
MsGPUBusy, MsGPUWait, VideoBusy, DisplayLatency, DisplayedTime, MsAnimationError,
AnimationTime, MsClickToPhotonLatency, MsAllInputToPhotonLatency, InstrumentedLatency,
MsBetweenPresents, MsInPresentAPI, MsBetweenDisplayChange, MsUntilDisplayed,
MsRenderPresentLatency, MsBetweenSimulationStart, MsPCLatency, MsBetweenAppStart**.
`CPUStartTime` mutates to `CPUStartQPC`/`CPUStartQPCTime`/`CPUStartDateTime` under the
time flags. ([PresentMon README-ConsoleApplication.md](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-ConsoleApplication.md))
**Relevance:** the exact names PhyriadFG reuses for any column it genuinely computes.

### F2 — `MsBetweenPresents` (console) ≠ `FrameTime` (capture) for the same concept `[V1]`
The console frontend names the CPU frame interval **`MsBetweenPresents`** ("The time
between this Present() call and the previous one"); the **capture** frontend names the
identical concept **`FrameTime`** ("The total amount of time in between frames on the
CPU"). Both re-fetched first-hand. **Decision: pick the console vocabulary
(`MsBetweenPresents`) and state it in the file header** — a blind choice breaks
downstream parsers. ([Console](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-ConsoleApplication.md) ·
[Capture](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-CaptureApplication.md))

### F3 — `FrameType` is THE real-vs-synthesized column, and it is gated `[V1]`
`FrameType` ("Whether the frame was rendered by the application or generated by a
driver/SDK") is the only PresentMon column separating a real frame from a generated
one; it requires `--track_frame_type` (added v2.3.0 for Intel XeFG / AMD AFMF). Without
it, FPS is inflated by the FG multiplier. ([Console README](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-ConsoleApplication.md))
**Relevance — the single most important FG column:** PhyriadFG **always** emits it
(`Real` = captured, `Synthesized` = interpolated), so honest throughput (real-fps
separate from present-fps) is computable.

### F4 — PresentMon CPU/GPU busy/wait timing definitions `[V1]`
Capture-app definitions (verbatim): `GPUTime`="Total amount of time between when GPU
started frame and when it finished"; `GPUBusy`="How long the GPU spent working on this
frame"; `GPUWait`="…waiting while working"; `CPUBusy`/`CPUWait` analogous;
`DisplayedTime`="How long this frame was displayed". GPU-bound vs CPU-bound has **no
official threshold** — it is comparative (`MsGPUBusy` ≈ `MsBetweenPresents` ⇒
GPU-bound). ([Capture README](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-CaptureApplication.md))
**Relevance:** PhyriadFG's 4090 warp time maps to `MsGPUTime`/`MsGPUBusy`; consistent with
the arc's measured GPU/fence-bound present slip.

### F5 — The latency columns PhyriadFG CANNOT populate (external capture) `[V1]`
`InstrumentedLatency`, `MsClickToPhotonLatency`, `MsAllInputToPhotonLatency`,
`MsPCLatency` require game/driver markers or input-event tracking PhyriadFG does not have.
**Emit them NA — never fill them with our added-latency proxy** (F15). The
present-boundary columns (`MsBetweenPresents`, `MsUntilDisplayed`,
`MsBetweenDisplayChange`, `DisplayedTime`) ARE computable from our present timestamps.
([Console README](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-ConsoleApplication.md))

### F6 — `MsAnimationError` directly scores stutter — but needs an animation clock `[V2]`
`MsAnimationError` = `(AnimationTime_N − AnimationTime_{N−1}) − MsBetweenDisplayChange_N`
(GamersNexus white paper): signed cadence error, positive = shown too early. It scores
stutter directly rather than inferring it from frametime spikes. **[V2]** — the
PresentMon column name is re-confirmed [V1]; the GamersNexus formula was harness-read
(its angle's adversarial verify failed) and is **not re-fetched first-hand this pass**.
**Relevance + honest caveat:** external capture has no game `AnimationTime`, so we can
only populate this from a **proxy** animation clock — flag it as proxy, do not claim
parity with a game-instrumented value. ([GamersNexus Animation Error white paper](https://gamersnexus.net/gpus-gn-extras-cpus/problem-gpu-benchmarks-reality-vs-numbers-animation-error-methodology-white))

### F7 — PresentMon capture-app system-telemetry column names `[V1]`
Re-fetched first-hand. GPU: **GPUPower, GPUVoltage, GPUFrequency, GPUTemperature,
GPUFanSpeed, GPUUtilization, 3D/ComputeUtilization, MediaUtilization, GPUMemoryPower,
GPUMemoryVoltage, GPUMemoryFrequency, GPUMemoryEffectiveFrequency, GPUMemoryTemperature,
GPUMemorySizeUsed, GPUMemoryWriteBandwidth, GPUMemoryReadBandwidth**. CPU:
**CPUUtilization, CPUPower, CPUTemperature, CPUFrequency**. ([Capture README](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-CaptureApplication.md))
**Relevance:** reuse these names, **suffixed per device** (`_4090`/`_1080Ti`/`_iGPU`) —
a documented divergence from PresentMon's single-GPU schema, forced by our 3-GPU rig.

### F8 — NVML per-GPU telemetry signatures + units `[V1]`
Re-fetched first-hand: `nvmlDeviceGetMemoryInfo`→**bytes** (`nvmlMemory_t`
total/free/used); `nvmlDeviceGetPowerUsage`→**milliwatts** (÷1000 for W);
`nvmlDeviceGetClockInfo(nvmlClockType_t)`→**MHz** (enum `NVML_CLOCK_GRAPHICS=0`,
`_SM=1`, `_MEM=2`, `_VIDEO=3`); `nvmlDeviceGetTemperature(NVML_TEMPERATURE_GPU)`→**°C**;
`nvmlDeviceGetUtilizationRates`→**percent** (`nvmlUtilization_t {gpu, memory}`).
([NVML Device Queries, R535](https://docs.nvidia.com/deploy/archive/R535/nvml-api/group__nvmlDeviceQueries.html))
**Relevance:** PhyriadFG already calls `nvmlDeviceGetUtilizationRates` (STAGE-99); this
extends the same handles to power/clock/temp/VRAM. **Units MUST be converted before
logging** (power ÷1000, memory ÷1048576 for MiB).

### F9 — NVML per-call overhead is UNDOCUMENTED — must be measured `[V1]`
The NVML reference documents no per-call cost, no recommended polling interval, no
per-frame guidance. The one measured real-world figure: a host-telemetry paper reports
**1.21 % CPU overhead at 100 Hz combined sampling with NVML at 10 Hz**. ([NVML ref](https://docs.nvidia.com/deploy/archive/R535/nvml-api/group__nvmlDeviceQueries.html) ·
[arXiv 2510.16946](https://arxiv.org/html/2510.16946)) **Relevance:** Phyriad's
efficiency mandate forbids "non-perturbing" as an assumption — NVML goes on the drain
thread at ~10–50 Hz, and we **measure our own call cost** before claiming the export is
free.

### F10 — CapFrameX "1% low" has TWO definitions; both must be named `[V1]`
Re-fetched first-hand: **(A) x% low average** = mean of the lowest x% of values
("every outlier… is included"); **(B) x% low integral** = sort frametimes
highest→lowest, accumulate until the cumulative sum reaches x% of total benchmark time,
report the FPS of the boundary frame. `FPS = 1000 / frametime_ms`. A frametime
percentile = "the value for which X% of all remaining values are smaller". CapFrameX
**warns 0.1% low is unreliable** from only 2–3 frames. ([CapFrameX metrics blog](https://www.capframex.com/blog/post/Explanation%20of%20different%20performance%20metrics))
**Relevance:** PhyriadFG reports **both**, names the method, and **sample-guards** 0.1%
low. A bare "1% low" violates the honesty discipline.

### F11 — CapFrameX stutter + AdaptiveSTDEV `[V3]`
From CapFrameX source (`FrametimeStatisticProvider.cs`): a frame stutters if
`frametime > stutteringFactor × moving_average` (default **2.5**); `stutter count%` and
`stutter time%` are reported; the reference is a **time-based moving average** (v1.6.9+),
not the global average. `AdaptiveSTDEV` = std of residuals vs that moving average.
**[V3]** — harness-read from the repo; the moving-average **window length is uncertain**
(a secondary snippet says ~20 s) and was **not** re-confirmed first-hand. **Relevance:**
add a CapFrameX-shaped `stutter_pct` + `AdaptiveSTDEV`; **pick and document our window**
rather than citing "20 s" as fact. ([FrametimeStatisticProvider.cs](https://github.com/CXWorld/CapFrameX/blob/master/source/CapFrameX.Statistics.NetStandard/FrametimeStatisticProvider.cs))

### F12 — pmcap's two-file layout; CapFrameX's CSV is raw-PresentMon pass-through `[V3]`
pmcap writes a **raw per-frame CSV** + a **`-stats.csv`** (duration, frame count,
avg/min/max FPS, 99/95/90 FPS percentiles). CapFrameX's CSV export is a raw-PresentMon
pass-through and adds **no** proprietary columns (its derived metrics live in the app /
JSON). **[V3]** — harness-read, not re-fetched first-hand this pass. **Relevance:** copy
the **two-file** pattern; our derived metrics go in the stats file.

### F13 — Pipeline-internal stages are invisible to ALL tools (the value-add) `[V1]`
No external tool (PresentMon/OCAT/CapFrameX/FrameView) sees per-stage FG internals — it
observes only the final `Present()` boundary. Optical-flow time (1080 Ti), warp time
(4090), iGPU pack, cross-device PCIe transfer, ring occupancy/freeze, pair cadence have
**no standard name**. ([Console README](https://raw.githubusercontent.com/GameTechDev/PresentMon/main/README-ConsoleApplication.md) ·
[OCAT manual](https://gpuopen.com/manuals/ocat/ocat-index/)) **Relevance — this is the
entire value-add:** the `flow_ms`/`warp_ms`/`pack_ms`/`h2d_ms`/`ring_occ`/… columns no
other tool can produce. Most already exist as live stdout fields (`cap`/`cons`/`uniq`/
`frz`/`warp`/`slip`/`gme dissidence`, confirmed in `apps/render_assistant/src/main.cpp`)
→ per-frame logging is mostly plumbing existing instrumentation into the ring.

---

## §3 — The recommended CSV schema (the build contract)

One **raw per-frame** file, three column groups, in order. `<dev>` ∈ {`4090`,
`1080Ti`, `iGPU`}.

**(A) PresentMon-compatible (console names, verbatim — F1/F2/F3/F4/F5/F7):**
`Application, ProcessID, SwapChainAddress, PresentRuntime, SyncInterval, PresentFlags,
AllowsTearing, PresentMode, FrameType, CPUStartTime, MsBetweenPresents, MsInPresentAPI,
MsBetweenDisplayChange, MsUntilDisplayed, MsRenderPresentLatency, DisplayedTime,
MsGPUTime, MsGPUBusy, MsGPUWait, MsGPULatency, MsCPUBusy, MsCPUWait, DisplayLatency`.
NA (no proxy): `InstrumentedLatency, MsClickToPhotonLatency, MsAllInputToPhotonLatency,
MsPCLatency`. Proxy-only (flagged): `MsAnimationError, AnimationTime`.

**(B) Architecture-native (snake_case, unit-suffixed, F13) — the value-add:**
`ayama_frame_index, qpc_present, flow_ms, warp_ms, pack_ms, h2d_ms, d2h_ms, cap_ms,
ring_occ, cap_slots, frz, pair_age_ms, source_fps, output_fps, cons_per_s, uniq_per_s,
slip_ms, iter_ms, gme_dissidence_pct, MsAddedLatency, route_device`.

**(C) Per-device system telemetry (PresentMon names + `_<dev>` suffix, F7/F8):**
`GPUUtilization_<dev>, GPUPower_<dev>, GPUTemperature_<dev>, GPUFrequency_<dev>,
GPUMemoryFrequency_<dev>, GPUMemorySizeUsed_<dev>, GPUFanSpeed_<dev>` for each device
(NVML for the two NVIDIA; PDH/NA for the iGPU), plus `CPUUtilization, CPUFrequency`
(`CPUPower`/`CPUTemperature` where available, else NA).

A companion **`-stats.csv`** holds the derived metrics (§4) + the pmcap stats
(duration, count, avg/min/max FPS, 99/95/90 FPS percentiles).

---

## §4 — Derived metrics (the stats file) with exact formulas

- **1% low (integral — default, F10):** sort `ft[]` descending, `T=Σft`, accumulate
  until `acc ≥ 0.01·T`, report `1000/ft_boundary`.
- **1% low (average, F10):** `k=ceil(0.01·N)`; mean of the `k` largest frametimes →
  `1000/mean`. Report **both**, named.
- **0.1% low (both, sample-guarded, F10):** same with `0.001`; suppress/flag if the
  eligible sample is `< ~3` frames. Compute **over all frames AND real-frames-only**
  (synthesized frames bias the tail).
- **Frametime percentiles (P99/95/90/P1/P0.1, F10):** sort ascending,
  `P_x = ft[round((x/100)(N−1))]`; FPS percentile = `1000/ft_percentile`.
- **Stutter %% (count + time, F11):** stutter iff `ft > 2.5·movavg`; `count% =
  100·#stutter/N`; `time% = 100·Σ(stutter ft)/T`. **Document the moving-average window.**
- **AdaptiveSTDEV (F11):** `sqrt(Σ(ft−movavg)²/(N−1))`.
- **FG honest split (F3):** `present_fps = N/dur`; `real_fps = #(FrameType==Real)/dur`;
  `fg_multiplier = present_fps/real_fps`. Compute all pacing metrics **twice**
  (all-frames, real-only).

---

## §5 — Honest gaps (what comprehensive telemetry still cannot reach)

1. `MsAnimationError`/`AnimationTime` need the game's simulation clock — external
   capture has none → proxy-only, not comparable to game-instrumented PresentMon.
2. `InstrumentedLatency` + all input-to-photon latency need driver/input markers PhyriadFG
   lacks → NA; true click-to-photon needs an LDAT-style hardware path.
3. Cross-device PCIe transfer time and 1080 Ti↔4090 P2P latency have no standard column
   and no external tool; attributing an async copy to a specific frame is itself an
   unspecified measurement-design problem.
4. Per-stage GPU time (`flow_ms`/`warp_ms`/`pack_ms`) needs Vulkan timestamp queries
   (NVML is too coarse); their overhead and pipeline-stall risk are **unmeasured** —
   the efficiency mandate requires measuring before claiming non-perturbing.
5. The CapFrameX stutter moving-average **window length** is not firmly established (F11).
6. AMD iGPU PDH telemetry does not map cleanly onto PresentMon's NVIDIA/Intel-shaped
   `GPU*` columns; several will be **NA** for that device (never a fabricated zero).
7. NVML per-call cost on **this** rig's 1080 Ti/4090 is unmeasured (F9) — "non-perturbing"
   is currently an assumption.
8. GPU-bound/CPU-bound classification has no standard threshold (F4) — any derived
   bound-state column is our heuristic, documented as such.
9. VRR/G-Sync interaction with `MsBetweenDisplayChange`/`DisplayedTime` for an external
   FG present is unaddressed by the sources — pacing under VRR needs separate validation.

---

## §6 — Design (how the export stays non-perturbing) + the CapFrameX path

- **Real-time isolation (F9, efficiency mandate):** the present loop appends ONLY a
  QPC-stamped struct (frame index + the stage-time deltas already in registers) to a
  **lock-free SPSC ring** each tick. A dedicated **low-priority drain thread** does ALL
  NVML/PDH queries, derived-metric math, and file I/O.
- **Decoupled cadence:** per-frame columns logged every present; system telemetry
  sampled at **~10–50 Hz** on the drain thread and nearest-joined onto frame rows. Never
  call NVML per present.
- **Master clock:** QPC ticks; derive ms columns from it (PresentMon's `CPUStartTime`
  name changes under time flags — we pick one and document it).
- **CapFrameX path (honest):** emit the **two files** (raw per-frame + `-stats`) with
  PresentMon **console** names on the shared columns, `FrameType` always populated, and
  our extension columns as trailing fields tools ignore. **We did NOT verify that
  CapFrameX *imports* an arbitrary external CSV** (it is built around its own PresentMon
  capture service) — the verified, low-risk path is **parse-compatibility on the shared
  columns + running CapFrameX's documented formulas ourselves**, not asserting
  drag-and-drop import. The `_<dev>` suffix is a deliberate, documented divergence.
- **Measurement gate (before any "non-perturbing" claim):** measure (a) our NVML/PDH
  call cost on the 1080 Ti + 4090, (b) Vulkan timestamp-query cost for per-stage ms,
  (c) the ring + drain overhead under the HSR120 worst case.

---

## §7 — What this grounds (cross-link row — descriptive)
| Finding | Where it grounds in Phyriad |
|---|---|
| FrameType / honest real-vs-present split (F3) | the `--csv` export's FrameType column + the dual-set stats; the proposal's "honest throughput" claim |
| PresentMon column compatibility (F1/F2/F7) | the raw per-frame CSV's group-A/C columns |
| Pipeline-internal columns (F13) | the value-add: the live stdout fields (`cap/cons/uniq/frz/warp/slip/gme`) plumbed per-frame |
| NVML telemetry (F8) | extends STAGE-99 (the NVML utilization fix) to power/clock/temp/VRAM on the drain thread |
| Derived metrics (F10/F11) | the `-stats.csv` (1%-low both methods, stutter, percentiles) |
| The efficiency gate (F9) | Phyriad's mandate — measure the export's cost before claiming it is free |

---

*SOTA pass 2026-06-16 (6 angles, 48/54 survived). The adversarial-verify stage failed on
4 angles (transient socket error); the load-bearing PresentMon column lists, NVML
signatures, and CapFrameX formulas were re-fetched first-hand per the FDP mandate and are
[V1]; F6/F11/F12 remain [V2]/[V3] and are flagged. Refuted/uncertain items kept visible
so we do not build on them.*
