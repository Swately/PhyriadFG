# FG_TESTBENCH_MASTER_PLAN — a controlled FG test environment (one deterministic source · three measurement axes)

> **Diátaxis type:** Planning / design (Explanation). **Status:** `designed` — the testbench is
> **not built**; this document is the conceptual spine, not a record of shipped code. Per-component
> standing is tagged inline (the *reused* assets are `shipping`/`measured`; the *new* components are
> `designed`).
> **Plan tier:** **Tier 2 (risk-bearing)** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)
> §1.1 trigger 1 (crash / device-loss / TDR): the **TB-C3 SaturationLoad** sustains graphics-queue + present
> pressure on the same GPU to a held target utilization — a load that can plausibly **TDR the GPU or
> destabilize the system** (risk **TB-1**). TB-C2's bounded-run touches the exit/present path (risk **TB-3**).
> The Tier-1 components (TB-C1, TB-C4, TB-C5, TB-C6) carry no qualifying risk on their own, but the testbench ships as
> one risk-bearing set → the triad is mandatory.
>
> **Document set (mutually linked, §2.3 of the tier protocol):**
> - this MASTER_PLAN — the why, the three axes, the components, the build order, the gates;
> - [`FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md`](FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md) — the concrete
>   per-component edit sites, the byte-identical-off discipline, and the validation (authored — `designed`);
> - [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md) — every failure mode TB-1..TB-6 (+ the
>   minigame's MG-1..MG-9) with its mitigation AS CODE + first-hand verification; Tier-2 gate: **no `open`
>   risk at commit** (authored — `designed`).
>
> **★ Extension (2026-06-25) — TB-C12 testbench_minigame (Axis 5: input→photon).** This plan is extended
> with a NEW sibling app `framework/render/vulkan/bench/testbench_minigame/` — ONE customizable minigame
> (dialable GPU saturation · controllable movement · live input) that measures the felt input→photon
> latency (the operator's "~0.5 s") and **resolves the open TB-2 realism risk**: a real minigame whose FIFO
> present lets the render-ahead queue build IS the graphics+present back-pressure that **TB-C3 — which
> offscreen-decouples its present to hold a util target — structurally cannot be** (§2 TB-C12 + the §2 TB-C3
> correction note). It composes three reusable headers (`latency_tap.hpp` / `gfx_present_load.hpp` /
> `interactive_scene.hpp`) over the **reused TB-C1 scene** (`testsource.comp`) + an external
> `fg_latency_scorer.py`; **render_assistant is UNTOUCHED** (captured/bounded/decomposed via shipping flags
> only). Its risks are the **MG-1..MG-9** namespace in the RISK_REGISTER (disjoint from TB-1..TB-6), all
> seeded `open`; **MG-2 (saturation realism) is liftable ONLY by the operator's first-hand rig A/B** — a
> monitor-free supervisor can only compile-verify. `designed` — no code written.
>
> **Backing SOTA dossiers (do not duplicate; link):**
> [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md) (how VFI/FG quality · latency · throughput ·
> pacing are measured rigorously, the GT-source routes, and the saturation-generator gap) and
> [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md) (the subjective/eye axis — BT.500 / 2AFC / JOD,
> artefact amplification, the MOS-correlation ceiling behind §4-C). These are this testbench's own backing
> dossiers.
> **Related / prior prior-art (real and valuable, but NOT this testbench's backing dossiers):**
> [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) (VFI quality measurement + held-out protocol + the disocclusion
> frontier) and [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md)
> (measurement parity, the closeable-objective framing, the field's perceptual/SROCC numbers); the two backing
> dossiers above defer to these for their owned facts (FDP §4.6).
>
> **Verifiable-reference mandate (FDP).** Every code `path:line` below was re-verified first-hand this session
> against the working tree. Figures reproduced verbatim from the originating brief (§4-C: PLCC 0.6, LPIPS 0.597)
> are **carried claims**, not re-fetched first-hand here — their primary-source verification belongs to the
> perceptual-eval SOTA dossier [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md) and is marked as such.
> **Normativity (BCP 14):** the key words MUST / MUST NOT / REQUIRED / SHALL / SHOULD / SHOULD NOT / RECOMMENDED
> / MAY / OPTIONAL are to be read per [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
> [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html) only where they appear in all capitals.

---

## §0 — Motivation: why a synthetic testbench at all

**Real-game testing is scene-confounded, and the confound is large enough to drown every per-lever signal we
care about.** The render_assistant FG arc has, across its STAGE history, measured an FG multiplier spanning
roughly **0.59×–3.1×** on real games depending on scene content, motion, and GPU saturation. That spread is
not noise around a lever's effect; it is the *content* changing under our feet. When a single A/B change (a
new selection rule, a governor tier, a flow-scale) moves quality or pacing by a few percent, that change is
invisible inside a ±200% content-driven envelope. Every "did this help?" answer on a live game is therefore
contaminated by *which* game, *which* fight, *which* camera turn — the textbook scene confound.

The published state of the art reaches the same verdict and prescribes the same escape: a **deterministic
synthetic source** with **exact held-out ground truth** is the only rigorous way to isolate "how good is OUR
flow+warp / our pipeline" from "what was on screen." This is the held-out-frame protocol
(Middlebury / Vimeo90K / Adobe240 / MSU VFI Benchmark) already adopted offline by
`framework/render/vulkan/bench/fg_quality_scorer/prep_zoo_sequence.py`, and it is the methodological position
of [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) and
[`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md).

What does **not** yet exist is that rigor on the **live, shipped pipeline under controlled GPU saturation**.
The deterministic source lives only offline; the live app is WGC-capture-only and runs until Ctrl-C; the
saturation that produces the phenomenon under test (a graphics-queue / blocking-present bottleneck) comes
today from an *uncontrolled* external game. This master plan defines the environment that closes that gap:
**ONE deterministic synthetic source feeding THREE measurement axes** — objective numbers, a perceptual
blind A/B eye, and a photonic slow-mo camera — joined by a frame-ID alignment lever so the live output can be
scored against analytic ground truth even when the live path drops and paces frames non-deterministically.

---

## §1 — The three measurement axes (what each measures · the asset that serves it)

The testbench is organized as three axes over **one** source. Each axis answers a different question; the
source and the frame-ID tag are shared so the three are commensurable.

| Axis | The question it answers | Instrument (HAVE vs NEW) |
|---|---|---|
| **Axis 1 — Objective** | "What do the numbers say?" — per-frame fidelity (PSNR/SSIM/motion-masked seam/flow-discrepancy) + pacing (fps, 1%/0.1% lows, P99 frametime, stutter, fg_multiplier, 4090 occupancy) under a held saturation. | **HAVE:** `fg_quality_scorer` (the **TB-C8** metric engine, `measured`, calibrated baseline) + `--csv` telemetry (`shipping`). **NEW:** the live→GT bridge via **TB-C7**, the **TB-C4** sweep, the **TB-C3** controlled load. |
| **Axis 2 — Perceptual** | "Which one does a human prefer, blind?" — a 2-alternative-forced-choice (2AFC) verdict, randomized + counterbalanced, aggregated to a preference scale. | **NEW:** the **TB-C5** EyeHarness (presenter + verdict log + Bradley-Terry/JOD + chi-square). The source's **TB-C1 "eye" skin** + **TB-C4** produce the stimulus pairs. |
| **Axis 3 — Photonic** | "What actually hit the panel, in real time?" — an external high-speed camera films the screen and reads the on-screen frame-ID markers to recover true present cadence / duplicates / tearing independent of our own clocks. | **NEW:** the **TB-C6** CameraKit (the **TB-C1 "camera" skin** with large frame-ID markers + slow-mo footage analysis). |
| **Axis 5 — Input→photon** | "What does the hand feel?" — the end-to-end input→photon latency (the felt ~0.5 s) under a REAL graphics+present back-pressure, with FG-interpolated frames rejected so the number is the latency of a real input-bearing frame. | **NEW:** the **TB-C12** minigame (TB-C12a `latency_tap` + TB-C12b `gfx_present_load` + TB-C12c `interactive_scene`) over the reused TB-C1 scene + the external **`fg_latency_scorer.py`**. Ground truth = the operator's **S24 Ultra slow-mo** (TB-C6-class); proxies = the SPSC CSV + PresentMon. |

*(Axis 4 — temporal fluidity — is served by TB-C9; see §2.)* The axes are deliberately redundant where they overlap (Axis 1's pacing telemetry and Axis 3's photonic
cadence measure the *same* present stream by two independent clocks) — that redundancy is the cross-check
that catches the cross-clock measurement errors named in **TB-5**.

---

## §2 — The components (TB-C1–TB-C9)

Each subsection states the component's **purpose**, its **reuse-vs-build** standing, and **the axis it
serves**. The component IDs are the shared spine; they are used verbatim across all three testbench
documents and at every commit (consistency is REQUIRED).

### TB-C1 — SourceWindow (the one deterministic synthetic source)
**Purpose.** A standalone, windowed, WGC-capturable app that renders **deterministic synthetic motion** at a
chosen resolution and fps — the single source feeding all three axes. It exposes three **skins** over the
same motion model: **GT** (clean content for the objective scorer), **eye** (content tuned to expose the
artifacts a human judges), and **camera** (content carrying large, unambiguous frame-ID markers the photonic
analysis reads). Reuse-vs-build: **mostly NEW** (a new windowed animation loop with res/fps/skin knobs), but
it reuses the analytic motion model of `framework/render/vulkan/bench/.../prep_zoo_sequence.py` (fixed seed,
7 motion presets incl. `occlude`, exact closed-form midpoint) and the `Swapchain.hpp` windowed-present
scaffolding (`framework/render/vulkan/include/phyriad/render/vulkan/Swapchain.hpp` — a real `VkSwapchainKHR`,
FIFO, that WGC can capture). It MUST NOT set `WDA_EXCLUDEFROMCAPTURE` (that flag is the FG's own-overlay
anti-feedback guard at `apps/render_assistant/src/main.cpp:16-18`); a normal capturable HWND is the point.
**Axis: all three** (the shared root).

### TB-C2 — FG bounded-run mode
**Purpose.** A deterministic exit for the live `render_assistant` so a per-config run can be bounded and
batched: `--duration` / `--max-frames` / `--exit-after`, with a **clean teardown** (drain the present loop,
finalize the `--csv` stats file, print the descriptor/object count so a leak shows up across a sweep).
Reuse-vs-build: **NEW flag + a new clean-exit path**. Today `g_quit` is set only by the console handler or
device-loss (`apps/render_assistant/src/main.cpp` — the Ctrl-C / window-death / device-loss sites); a grep
for `--duration/--max-frames/--exit-after/--headless` returns nothing. The flag MUST be added in the
extra-arg parser, **not** the main `parse` chain (the file has hit MSVC C1061 nesting limits before), and it
MUST be **byte-identical-off** (TB-3, TB-6): absent the flag, the present/teardown path is bit-for-bit the
current build. **Axis: 1** (it is the harness's run primitive; TB-C4 drives it).

### TB-C3 — SaturationLoad
**Purpose.** A **controllable** competing GPU load that pressures the **graphics queue and present** the way
a real game does — *not* a compute-only burn — with **NVML closed-loop feedback** to hold a target
utilization %. This is the component that reproduces the actual phenomenon under test: the FG slice
host-blocks on the graphics queue under saturation, so the load MUST contend on graphics+present, not merely
peg the ALU (this is **TB-2**, the realism risk). Reuse-vs-build: **NEW controller** around verified
primitives — the `iters` compute-burn push-constant of
`framework/render/vulkan/bench/stage9_singlegpu/shaders/game_render.comp:10,19-20` (a real, tunable GPU-load
knob) is the *starting* primitive but is **insufficient alone** (compute-only); the load must also submit a
real graphics workload and present to a swapchain to create queue/fence/present contention. The NVML reader
currently lives inside `apps/render_assistant/src/main.cpp` (~`:7254`–`:7330`) and MUST be factored into a
small shared helper for the feedback loop. Building a sustained graphics+present saturator is **pioneered
work** — the SOTA does not source a standard for it (§4-B, §5). **Axis: all three** (it sets the regime every
axis measures in). **This component is the Tier-2 driver — TB-1.**

> **★ Correction (2026-06-25) — TB-C3 as BUILT holds util by DECOUPLING its present; it does NOT reproduce
> the felt present back-pressure.** An earlier framing of this section read TB-C3's graphics+present as
> reproducing the FG's blocking-present bottleneck. First-hand re-verification of the shipped TB-C3
> (`framework/render/vulkan/bench/testbench_saturation/main.cpp`) shows the opposite by design: the burn
> renders to a **device-local OFFSCREEN image** and the present is a **throttled, non-blocking
> `vkCmdCopyImage`** of a completed offscreen into the swapchain every `kPresentEvery=32` frames
> (`:26-27`, `:36-37`, `:100`) — the decouple is *deliberate*, so the windowed present can never stall the
> burn loop and the NVML loop can HOLD a target util. The consequence (already recorded in the
> RISK_REGISTER Wave-3 reconciliation): TB-C3 reproduces **SOURCE-STARVATION**, not the in-game
> **PRESENT-COLLAPSE**. TB-C3 remains correct for what it is — a held-util graphics+ALU+compositor load —
> but the **felt present back-pressure (the FG's `vkWaitForFences(fBridge)` stretch)** is exactly what its
> util-hold removes. That regime is reproduced by **TB-C12** (§2 below), which inverts the one decision: a
> real FIFO present that lets the render-ahead queue build. This is why TB-C12 — not TB-C3 — is the
> resolution of **TB-2 / MG-2**; the held-util sweep stays TB-C3's job.

### TB-C4 — SweepOrchestrator
**Purpose.** One parameterized driver that takes the cross product
**{ source(res, fps, skin) × load(target util) × FG(flags) }**, launches TB-C1 + TB-C3 + the TB-C2-bounded FG per
cell, harvests each cell's `--csv` (raw per-present + derived stats) and its objective GT-quality score,
and aggregates the cells into an **A/B matrix**. Reuse-vs-build: **NEW orchestration**, but the *pattern*
exists offline — `scripts/check_fg_quality_regression.ps1` / `scripts/check_fg_perf_regression.ps1` already
do fixture-regen → per-config CSV harvest → baseline diff with deterministic exit codes; TB-C4 is the same
discipline lifted onto the **live** pipeline. **Axis: 1** (it produces the objective matrix; it also stages
the TB-C5 stimulus pairs and TB-C6 capture runs).

### TB-C5 — EyeHarness (Axis 2)
**Purpose.** A **2AFC blind, randomized, counterbalanced** A/B presenter: it plays two FG configurations
(or FG vs reference) back to back in random order, records the observer's forced-choice verdict, and analyzes
the verdict log with **Bradley-Terry / JOD** (a preference/just-objectionable-difference scale) plus a
**chi-square** significance test; an **artefact-amplification mode** exaggerates a suspected failure
(slow motion, contrast boost, loop the disocclusion band) to make a subtle difference decidable. Reuse-vs-
build: **NEW**. The stimulus content is TB-C1's **eye** skin; the run scheduling is TB-C4. **Axis: 2.** The honest
ceiling on this axis is **TB-5 / §4-D**: a single observer is, by ITU terminology, **"informal"** — TB-C5
compensates with many randomized within-subject trials, never by claiming panel-grade validity.

### TB-C6 — CameraKit (Axis 3)
**Purpose.** The **camera** skin (TB-C1 rendering large, high-contrast, unambiguous **frame-ID markers**) plus
the **slow-mo footage analysis** that reads those markers off external high-speed camera footage to recover
the *true* presented sequence — real cadence, duplicated/dropped frames, tearing — with **zero dependence on
our own present timestamps** (the independent photonic clock that cross-checks Axis 1). Reuse-vs-build:
**NEW** (the marker renderer is a TB-C1 skin; the footage analyzer is new offline tooling). **Axis: 3.**

### TB-C7 — FrameID alignment (the live ground-truth lever)
**Purpose.** The mechanism that makes **live-absolute** ground truth possible. TB-C1 visually **tags each source
frame** with a recoverable ID; the live FG output (and the TB-C6 footage) carries that tag through; the scorer
then aligns each live present to the **analytic GT** the source can emit for that exact instant — even though
the live path **drops and paces frames non-deterministically** under load. Where a presented frame cannot be
aligned to a single emittable GT instant, TB-C7 MUST **honestly mark it unalignable** rather than score it
against the wrong target (**TB-4**). TB-C7 is the thread that ties **TB-C1 (tag-emit) + TB-C4 (capture) + TB-C8 (the
scorer that consumes the tag)** together; it is the operator's chosen route to live-absolute GT (§3) and the
reason the objective axis can say anything fidelity-grade about the *shipped* pipeline rather than only the
offline flow+warp slice. Reuse-vs-build: **NEW** (a tag-emit in TB-C1 + a tag readback at the FG present + a
scorer alignment mode). The tag-emit on the FG side touches `render_assistant` and MUST be byte-identical-off
(**TB-6**). **Axis: 1 (and 3 via TB-C6).**

### TB-C8 — Objective scorer / metric engine (reused)
**Purpose.** The objective metric engine that turns frames into numbers — `fg_quality_scorer`
(`framework/render/vulkan/bench/fg_quality_scorer/main.cpp`): PSNR, SSIM, the motion-masked double-edge
artifact metric (`dbl_edg_m`, FloLPIPS-motivated), flow-discrepancy, object-deform; Mode A (re-run committed
flow+warp), Mode B (score a live dump vs held-out truth), Mode T (truth-less live triples). It is `measured`
and **calibrated** against the locked baseline `docs/framework/FG_QUALITY_BASELINE.json` with a CI gate.
Reuse-vs-build: **HAVE, reused as-is**, extended only by a TB-C7-fed alignment mode (consume the emitted GT
midpoint + frame-ID instead of a withheld real). *ID note:* the originating brief refers to this asset as
"the TB-C8 scorer" in §4-C; it is defined here as TB-C8 to keep the ID space closed. **Axis: 1.**

### TB-C9 — Fluidity / motion-placement metric (new — the "escalonado" / temporal-fluidity axis)
**Purpose.** The TEMPORAL-fluidity metric — the objective measure of the "escalonado" (stepping/cadence) the
operator identified as the felt gap vs LSFG (distinct from TB-C8's SPATIAL fidelity). It scores WHERE-IN-TIME
each displayed frame's content lands (the placement / Animation-Error analogue) + the display-interval pacing
uniformity, for BOTH real and generated frames. **The key unlock:** the SOTA's Animation Error cannot score
generated frames (no engine animation-time), but **TB-C1's analytic `pos(t)` supplies it** → generated-frame
placement becomes computable (the metric the field calls otherwise impossible). Implemented as two `--csv`
columns (`disp_phase` = the presented interpolation phase `t_use`; `disp_src` = the content-source-time
`(pair_c−span)+t_use·span`, `main.cpp` push ~9238 / rfp ~8945; `telemetry_csv.hpp` CsvRow) + the analysis
`scripts/fg_testbench_fluidity.py` (placement RMS + per-FrameType split, display-interval pacing
stddev/lows, a combined fluidity score). Measurement-only, byte-identical-off (the `--csv` gate; columns
default −1.0, written only inside `if(tcsv.active())`). **VERIFIED first-hand:** a TB-C1+FG run scores
synthesized-placement RMS ~1.55 ms + pacing stddev ~4.24 ms — the escalonado quantified (the phase
oscillating 0.77→0.32→0.75→0.30 is the stepping in the numbers). The FIX direction (per
[`FG_FLUIDITY_PACING_SOTA.md`](FG_FLUIDITY_PACING_SOTA.md)): a uniform display metronome + t=0.5 placement
(the present-pacing / `--async-present` work). The TB-C6 camera is the photon-truth validation of the same
trajectory. Reuse-vs-build: **BUILT this session.** **Axis: 4 (temporal fluidity).**

### TB-C12 — testbench_minigame (the input-connected evolution — Axis 5: input→photon)
**Status:** `designed` (this Phase-0 governance; **no code written**). **Purpose.** A **single customizable
minigame** that makes the felt input→photon latency (the operator's "~0.5 s") *measurable*: three
**dialable axes** — GPU **saturation** (a real graphics+present load), controllable **movement**, and a
live **input** connection — over the **reused TB-C1 scene** (`testsource.comp`), measuring the
**input→photon** delta of a REAL input-bearing frame while rejecting FG-interpolated frames. It is the
**evolution that resolves TB-2** (§4-B): a real minigame whose FIFO present lets the render-ahead queue
build IS the graphics+present back-pressure that **TB-C3 — which offscreen-decouples its present to hold a
util target — structurally cannot be** (the TB-C3 correction note above). It is composed from three
independently-reusable headers over the existing TB-C1 source, plus an external scorer; **render_assistant
is UNTOUCHED** (captured/bounded/decomposed via shipping flags only — `--window` `main.cpp:1263`, the TB-C2
bounded-run `--duration`/`--exit-after` `:1166` / `--max-frames` `:1167`, `--latency-trace` `:1193`). It
registers as **TB-C12** with three sub-components, keeping the TB-C ID space closed (no new namespace):

- **TB-C12a — `latency_tap.hpp`** (`designed`; NEW header). Raw `WM_INPUT` sampler that QPC-stamps the input
  AT the raw-input callback (the earliest tap — the arXiv ~47 ms-engine vs ~10 ms-OS-hook lesson), binds the
  sample to a sim-tick id, emits a **hard binary dark→white solid-block flash** (the Reflex "Flash
  Indicator" shape, ≥6 % luminance step, NEVER a fade) on the input-consuming frame, and **encodes the
  originating tick-id into TB-C1's EXISTING 16-bit gray-code `cameraBand`** (`testsource.comp:140`, the bits
  at `:139` / `:153-156`) via the existing `pc.frame_id` + a free `pc.flags` bit — NO push-constant growth
  (the 64-byte `static_assert` tripwire at `testbench_source/main.cpp:171` holds). Interpolated frames get a
  DISTINCT id class so the scorer rejects them. Logs the Reflex/PresentMon **I2FS/FS2P/P2D** breakdown to an
  SPSC lock-free CSV (the `telemetry_csv.hpp` ring discipline — `CsvRow` `:33`, lock-free `push` `:83`,
  never a mutex on the present hot path). **Axis: 5.**
- **TB-C12b — `gfx_present_load.hpp`** (`designed`; NEW header). The dialable graphics+present saturator:
  CPU-issued instanced `vkCmdDrawIndexed` **`--load-count`** (the graphics-queue front-end + driver-
  submission pressure — the TB-2 fix; a REAL raster pass, NOT compute-only) + an internal RESOLUTION-scale
  offscreen pre-pass **`--res-scale`** (the smooth fragment/ROP/bandwidth knob) + the EXISTING per-pixel
  **`--burn`** iters (`testbench_source/main.cpp:169`, `pc.burn`) as a magnitude trim — all issued CPU-side
  on the SAME graphics queue that presents a **real FIFO swapchain**, letting the render-ahead queue build
  (the deliberate inverse of TB-C3's decouple). It **factors TB-C3's TDR watchdog + NVML reader +
  EMA/deadband controller VERBATIM** out of `testbench_saturation/main.cpp` (`kTdrSoftCapMs=250` `:87`,
  `kHardWaitNs=1.8 s` `:88`, `kKp=0.12` `:95`, `kUtilEmaAlpha=0.15` `:96`, `kCtrlDeadband=3.0` `:97`,
  `kFramesInFlight=4` `:99`, `--ramp-ms` `:134`, the dynamically-loaded NVML reader `:178`/`:181`) so both
  apps consume ONE copy. Primary setpoint = self-measured **frame-time ms** (low-lag, tracks the real
  bottleneck); NVML util is a SECONDARY sanity readout only. **Axis: 5 (it sets the regime).** **This is the
  Tier-2 driver — MG-1 (TDR) + MG-2 (realism).**
- **TB-C12c — `interactive_scene.hpp`** (`designed`; NEW header). A fixed-timestep accumulator sim
  (Gaffer/Doom-demo/TAS pattern) whose SOLE nondeterminism source is a per-tick `usercmd` command stream
  sourced from {**live** raw input | **recorded** file | **TB-C1 closed-form `make_pc` autopilot**}
  (`testbench_source/main.cpp:176`, the 7 motion presets `:91`) — one engine, two modes: **REPLAY** =
  analytic GT (feeds TB-C8/TB-C9 via the unchanged `--gt-emit` contract), **LIVE** = input→photon. A
  per-tick **state-hash** self-check re-verifies a replay before its GT is trusted; the load is forced
  OPEN-LOOP under replay (determinism). **Axis: 5 (+ 1/4 in REPLAY).**

The **scene** stays the reused `testsource.comp` compute→blit (the byte-identical deterministic baseline);
the saturation **raster** pass (TB-C12b) is a SEPARATE pipeline on the present queue (`scene.vert` /
`scene.frag`, NEW). Measurement is **external** — a NEW **`fg_latency_scorer.py`** reads the gray-code id
off each photon over the FG OUTPUT window, pairs it to its `T_input_sample`, **rejects interpolated
photons** (the PresentMon drop-attribution analogue), and emits the A/B (minigame-window vs FG-output-
window) input→photon delta as **distributions** (median + spread, never single numbers). It reuses the
red-fiducial band-rectify + gray-code decode of `scripts/fg_testbench_camera_analysis.py` (`:50` / `:91-98`).
Ground truth = the operator's **Samsung S24 Ultra slow-mo** (a TB-C6-class camera — verify the TRUE sensor
fps; 240/960 fps) in v1; an owned-hardware photodiode path (Arduino UNO + Lattice MachXO2-7000 FPGA,
OpenLDAT-style electrical injection — **no new purchase**) is a documented **v1.1** option.
**Reuse-vs-build:** NEW sibling app `framework/render/vulkan/bench/testbench_minigame/` (window / swapchain
/ present-loop scaffold lifted from TB-C1's `testbench_source/main.cpp`, switching its IMMEDIATE-preferring
present-mode default `:297-300` to FIFO) + the 3 new headers + the new shader pair + the new Python scorer;
**render_assistant unchanged.** **Axis: 5 (input→photon — the felt latency).**

> **How TB-C12 resolves TB-2.** TB-2 was *open* because the only graphics+present load we had (TB-C3)
> decouples its present to hold util — so it cannot stretch the FG's `vkWaitForFences(fBridge)`. TB-C12
> removes that escape by construction: a real FIFO present + render-ahead build on the same graphics queue.
> But the design does **not** close the risk — see the **honest floor in §5**: only the operator's
> first-hand rig A/B (fg_multiplier collapse toward ~1.0 + `lt_spin_us` rise under TB-C12b's load, while a
> matched compute-only `--burn` does NOT) lifts **MG-2** `open→mitigated`; a monitor-free supervisor can
> compile-verify only.

---

## §3 — Build order (under the operator's "three axes in parallel" steer)

**Operator steer (2026-06-24):** build **all three axes** (objective + perceptual + photonic), **in
parallel**, and use **FrameID alignment (TB-C7)** for live-absolute ground truth — the harder, more complete
route (rather than settling for offline-only or relative-only live scoring).

The three axes share a **spine** that MUST land first, then the axis-specific legs proceed concurrently:

1. **Spine (serial prerequisite for all axes).**
   - **TB-C1 SourceWindow** (the deterministic source + the three skins) — nothing measures without a source.
   - **TB-C7 frame-ID tag-emit** baked into TB-C1 from the start (retrofitting tags is worse than designing for them).
   - **TB-C2 bounded-run** in `render_assistant` (byte-identical-off) — the run primitive every axis batches.
   These three are the critical path; they unblock everything.

2. **Axis 1 (objective), in parallel after the spine.**
   - **TB-C3 SaturationLoad** (graphics+present, NVML-held) — the Tier-2 component; its RISK_REGISTER mitigations
     (TB-1, TB-2) MUST be `mitigated`/`accepted` before it runs unattended.
   - **TB-C8 alignment mode** (consume TB-C7's tag + the emitted analytic GT) + **TB-C4 SweepOrchestrator**.

3. **Axis 2 (perceptual), in parallel.** **TB-C5 EyeHarness** over TB-C1's *eye* skin; depends on TB-C1 + TB-C2 + TB-C4 for
   stimulus generation, not on TB-C3 being final.

4. **Axis 3 (photonic), in parallel.** **TB-C6 CameraKit** (TB-C1's *camera* skin + footage analysis); depends on
   TB-C1's frame-ID markers (TB-C7) and a real run (TB-C2), not on TB-C3.

Parallelism is real because the axes share only the spine (TB-C1/TB-C2/TB-C7); their legs (TB-C3+TB-C4+TB-C8, TB-C5, TB-C6) are
independent. The dependency that MUST be respected: **no axis is trustworthy before TB-C7's alignment is either
working or honestly reporting unalignable frames (TB-4)** — that is the lever the whole "live-absolute GT"
claim rests on.

---

## §4 — The honest hard-truths (state plainly; do not inflate)

These four are reproduced verbatim from the originating brief and are load-bearing limitations, not asides.
They are stated here so no downstream document silently rounds them up.

- **(A) Live-path absolute ground-truth is structurally hard.** Held-out manufactures the artifact; TB-C7
  frame-ID is the **mitigation, not a guarantee**. (The held-out protocol withholds a real frame to score
  against — which *creates the very artifact being measured*; this is the code-verified reason `--qdump`
  ships truth-less. TB-C7 sidesteps it by emitting analytic GT, but only buys a score where a present can be
  aligned frame-exact to a GT instant — and the live path's defining behavior under load is to drop/pace
  non-deterministically. See TB-4.)

- **(B) The saturation load is UNSOURCED in the SOTA — we pioneer it; it MUST be graphics+present
  WITHOUT decoupling the present.** No published standard prescribes how to synthesize a controlled
  graphics-queue+present saturation that reproduces the FG bottleneck. A compute-only burn does **not**
  reproduce the queue/fence/blocking-present contention that is the actual phenomenon (TB-2 / MG-2). Nor
  does a graphics+present load that **decouples** its present to hold a util target: that is precisely what
  the shipped **TB-C3 does** (offscreen + non-blocking FIFO copy — the §2 TB-C3 correction), so TB-C3
  reproduces source-starvation, not present-collapse. **TB-C12** is the component that reproduces the
  back-pressure (a real FIFO present + render-ahead build); it is original work and — like TB-C3 — must be
  *validated as representative by the operator's first-hand rig A/B (MG-2), never assumed.*

- **(C) The eye-proxy (calibrating the TB-C8 scorer to MOS) is an OPEN PROBLEM.** No standard metric exceeds
  PLCC ≈ 0.6 for FG (BVI-VFI: LPIPS 0.597 best); the **verdict DATASET is the deliverable, the proxy is
  research.** *(Provenance: PLCC 0.6 / LPIPS 0.597 are carried from the brief; their primary-source
  verification belongs to the perceptual-eval SOTA dossier [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md) — not re-fetched first-hand here.)* The
  testbench's defensible objective output is the **labelled A/B verdict dataset** (Axis 2 ground truth);
  fitting an objective metric (TB-C8) to predict it is a research goal, not a deliverable to be claimed done.

- **(D) Single observer = ITU "informal".** Compensate with many randomized within-subject trials; **never
  claim panel-grade validity.** TB-C5's randomization/counterbalancing + chi-square raise *within-subject*
  confidence; they do not convert one observer into a subjective-quality *panel* in the ITU-R BT.500 sense.

---

## §5 — Honest scope and floors

**In scope.** A controlled, repeatable live-FG test environment: one deterministic source, three measurement
axes, a held graphics+present saturation, a sweep orchestrator, and live-absolute GT via frame-ID alignment.

**Non-goals / named floors (FDP §4.4 — stated so no phase re-proposes one as a win):**

- **A guaranteed live-absolute fidelity score on every presented frame** — STRUCTURALLY FLOORED (hard-truth
  A / TB-4). TB-C7 raises the *fraction* of alignable frames and honestly marks the rest; it does not promise a
  score for a frame the live path dropped or re-paced to no GT instant.
- **A validated objective metric that predicts human FG preference** — OPEN RESEARCH (hard-truth C). The
  deliverable is the **verdict dataset**; an eye-proxy fitted to it is explicitly *not* claimed.
- **Panel-grade subjective validity** — OUT (hard-truth D). Single-observer is "informal"; the harness
  compensates with trial count, not by relabelling its statistical class.
- **A SOTA-sourced saturation load** — DOES NOT EXIST (hard-truth B); TB-C3 is pioneered and must be
  *validated* as graphics+present-representative, never assumed.
- **HDR / >8-bit source and scoring** — out of this plan's scope (the scorer and capture path are RGBA8
  today; HDR is its own arc, `FG_HDR_PIPELINE_MASTER_PLAN.md`).
- **Capping or pacing the source under test as a *product* behavior** — out; the source here is a *test
  instrument* we control, distinct from the no-game-cap operator invariant on real games.

**TB-C12 minigame floors (Axis 5 — stated so no phase re-proposes one as a win):**

- **TB-2 / MG-2 is NOT closed by the TB-C12 design.** The controlled graphics+present saturator is unsourced
  in the SOTA; the architecture makes the back-pressure regime *reproducible*, but **only the operator's
  first-hand rig A/B lifts MG-2 `open→mitigated`** (fg_multiplier collapse toward ~1.0 + `lt_spin_us` rise
  under TB-C12b's load, while a matched compute-only `--burn` does NOT). A monitor-free supervisor can
  compile-verify and reason but CANNOT produce the measured signature. Representativeness is argued from the
  measured BF6-signature parity, never assumed.
- **Absolute input→photon TRUTH comes ONLY from the camera/photodiode over the FG OUTPUT window.** The
  software SPSC CSV and PresentMon both EXCLUDE display scanout + pixel response (the part the FG's
  re-present changes) — they are proxies/cross-checks, structurally below truth.
- **GT-scored quality and live input→photon latency are DIFFERENT runs, never simultaneous** (live motion
  is not pre-known — hard-truth A). A "GT-scored quality under live input" claim is structurally
  impossible; live frames are marked unalignable, not scored against a wrong target.
- **Camera ground truth is frame-quantized** (±1 video frame: 4.17 ms @240 fps, 1.04 ms @960 fps; a single
  high-speed frame samples the panel scanout ~1 ms @1000 fps) → only **distributions** (median + spread)
  over many trials are reportable; single measurements are meaningless. Verify the phone's TRUE sensor fps.
- **Bit-exact replay holds only over SHORT hash-verified windows.** FP nondeterminism / threading / build
  drift cause TAS-style desync; the per-tick state-hash DETECTS divergence but does not prevent it.
- **The minigame is a controlled synthetic source, not a real game** — it reproduces a graphics-queue /
  present-contention REGIME, not any specific title's exact pipeline mix.
- **Engine-input latency cannot be driven to zero in-process** (WM_INPUT-at-sample approaches but is not a
  true OS/hardware click edge — the ~10 ms arXiv floor); it is reported as a SEPARATE CSV stage, never
  hidden inside input→photon.
- **The felt ~0.5 s this rig produces is hardware/driver/present-mode-specific** (the 4090+1080Ti+iGPU at a
  CHOSEN dialed load); it does not generalize across GPUs/drivers/present modes without re-measurement.

**Reused floors that already hold (do not rebuild):** the deterministic source + exact held-out GT
(`prep_zoo_sequence.py`, `measured`), the calibrated objective metric engine (TB-C8 / `fg_quality_scorer`,
`measured`, CI-gated), and the machine-readable live telemetry (`--csv` / `telemetry_csv.hpp`, `shipping`)
are solved and reused as-is.

---

## §6 — Tier declaration + document linkage

**Tier: 2 (risk-bearing).** WHY: **TB-1** — the **TB-C3 SaturationLoad** deliberately sustains graphics-queue +
present stress on the GPU to a held utilization, a load that can plausibly **TDR the GPU or destabilize the
system** (PLAN_TIER_PROTOCOL §1.1 trigger 1: crash / device-loss / hang / TDR). Secondary qualifying risk:
**TB-3** — TB-C2's bounded-run and **TB-6** — TB-C7's tag-emit both touch the `render_assistant` exit/present path
and engage the byte-identical-off dogma (trigger 5). The other risks (TB-2 load realism, TB-4 alignment
robustness, TB-5 measurement validity) are validity/correctness risks tracked in the same register.

**The Tier-2 triad (mutually linked, all three REQUIRED before any TB-C3/TB-C2/TB-C7 code is committed):**

| Document | Role | Status |
|---|---|---|
| `FG_TESTBENCH_MASTER_PLAN.md` (this) | the why · axes 1–5 · components TB-C1–TB-C9 + TB-C12 (the minigame) · build order · the floors | `designed` |
| [`FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md`](FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md) | per-component edit sites · byte-identical-off discipline · validation; cites each TB-/MG-ID at its edit site | `designed` (authored) |
| [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md) | TB-1..TB-6 + the minigame's MG-1..MG-9, each with mitigation AS CODE + first-hand verification; no `open` risk at commit | `designed` (authored) |

**The risk catalog (named here; detailed in the RISK_REGISTER):**

| ID | Class | One-line failure mode |
|---|---|---|
| **TB-1** | crash / device-loss | TB-C3's sustained graphics+present stress TDRs the GPU / destabilizes the system. |
| **TB-2** | dogma (validity) | A compute-only TB-C3 fails to reproduce the graphics-queue / blocking-present bottleneck that is the phenomenon under test. |
| **TB-3** | dogma (byte-identical-off) | TB-C2 bounded-run touches the exit/present path (`g_quit`) → must be byte-identical-off + clean teardown. |
| **TB-4** | validity | TB-C7 frame-ID alignment fails under non-deterministic drops/pacing → must survive or honestly mark frames unalignable. |
| **TB-5** | validity | Measurement error: cross-clock present-timestamp drift (the observed `compose:24.9` artifact), EMA/window artifacts, single-observer ITU-"informal", scene-confound (escaped ONLY by the synthetic source). |
| **TB-6** | dogma (byte-identical-off) | Any `render_assistant` change for TB-C2 + TB-C7 tag-emit must be byte-identical with the flags off. |

**The TB-C12 minigame risk catalog (namespace MG-1..MG-9, disjoint from TB-1..TB-6; detailed in the
RISK_REGISTER).** TB-C12 extends this SAME Tier-2 triad (it does not open a new one); it adds qualifying
risk on triggers 1 (sustained-load TDR — MG-1), concurrency on the shared present queue (MG-8), and dogma
(byte-identical-off / FG-touch — MG-6). All seeded `open`; the hard gate is on **MG-2**.

| ID | Class | One-line failure mode |
|---|---|---|
| **MG-1** | crash / device-loss | A `--load-count` × `--res-scale` × `--burn` max-dial submit blows the Windows ~2 s TDR → device-loss; could take the FG under test down. |
| **MG-2** | validity / dogma (UNSOURCED) | The integrated graphics+present load fails to reproduce the FG `fBridge` stretch (lands async/decoupled/wrong-GPU) → measures the wrong regime; reported resolved while it is not. **Liftable ONLY by the operator's first-hand rig A/B.** |
| **MG-3** | data-loss / irreversibility | Replay desync (wall-clock in sim / unseeded RNG / FP / flag drift) corrupts the analytic GT TB-C8/TB-C9 score against. |
| **MG-4** | validity | Marker pairing under FG interpolation — interpolated frames carry no fresh input but a warped flash; refresh-quantization, optical-flow smear, letterbox → naive flash-pairing yields garbage. |
| **MG-5** | validity | Measuring our own input glue, not the FG — a late input-sample timestamp / re-injection inflates input→photon and hides the FG's contribution. |
| **MG-6** | dogma (byte-identical-off / FG-touch) | A new knob changes default presented/dumped bytes, or an FG-side tag-readback regresses byte-output-off / hits the MSVC C1061 nesting limit. |
| **MG-7** | validity | WGC capture-path regression — DWM promotes the source to Hardware Independent Flip → WGC silently delivers ~half the frames → corrupted source blamed on the FG. |
| **MG-8** | concurrency | Present back-pressure on the shared queue starves the load/pacing loop; an NVML util-hold fights the back-pressure → the loop oscillates / the queue never builds. |
| **MG-9** | validity (proxy-as-truth) | Treating PresentMon / Reflex PCL as ground truth — they exclude scanout + pixel response and cannot know which input the game consumed → understate the felt FG lag. |

**Cross-links.** Backing SOTA: [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md) +
[`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md); related/prior prior-art:
[`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md),
[`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md). Plan triad: this MASTER_PLAN +
[`FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md`](FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md) +
[`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md). Protocols:
[`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md), [`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md).

---

## §7 — What could NOT be verified first-hand (FDP §2.4)

- **The two backing SOTA dossiers** ([`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md),
  [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md)) **now exist** and are linked above as this
  testbench's backing dossiers. (An earlier draft of this plan, written during the write-race before the two
  landed, mis-cited the prior-art `FG_VFI_PRIOR_ART.md` / `FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md` as the
  backing set; those are now re-labelled *related/prior* prior-art that the backing dossiers defer to for their
  owned facts, FDP §4.6.)
- **The §4-C figures (PLCC ≈ 0.6, BVI-VFI LPIPS 0.597)** are reproduced verbatim from the brief and are
  **carried claims**, not re-fetched from their primary sources this session; their `[V1]`/`[V2]` grading
  belongs to the perceptual-eval SOTA dossier [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md) (§2.G).
- **`prep_zoo_sequence.py`'s seed value** is taken as fixed (deterministic, byte-identical) from the file's
  documented held-out protocol; the exact numeric seed was reported `code-verified` by the inventory pass
  but the specific seed *line* was not re-opened in this session — treat the seed *value* as `(to verify)`
  at the strategies stage, the *determinism property* as confirmed by the file's header.
- **The TB-C2 deterministic-exit edit site** (which exact `g_quit` path) is `designed`, not pinned to a final
  `file:line` here — the precise insertion point is the IMPLEMENTATION_STRATEGIES' job; what is verified is
  the *absence* of any `--duration/--max-frames/--exit-after` flag today.
- **Registration.** Per FDP §7, this triad MUST be added to
  [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) and the live
  [`../ACTION_PLAN.md`](../ACTION_PLAN.md); those shared files are **not** edited from this document to avoid
  a collision with parallel work — the supervisor performs the registration.
