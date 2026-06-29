# FG_SATURATION_STABILITY_INTEGRATION — making PhyriadFG's latency STABLE under GPU saturation, from the free-FG optimization+architecture SOTA

> **Diátaxis type:** Analysis + Planning (a SOTA dossier that lands an ordered integration
> plan). Sibling of [`FG_SATURATION_PRIOR_ART.md`](FG_SATURATION_PRIOR_ART.md) /
> [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md); the implementation-
> grounded refinement of the saturation cluster (P0/P1/P3/P5) of
> [`PHYRIADFG_PERFECTION_ROADMAP.md`](PHYRIADFG_PERFECTION_ROADMAP.md).
>
> **Status:** `designed` (the integration plan is forward work). The SOTA findings are
> `measured`/external-leveled; the OUR-STATE claims are `[VS]` first-hand-verified this
> session or carried from the named dossier.
>
> **Provenance:** an 11-agent two-wave workflow (`wp0cmcjfd`, ~1.64 M subagent tokens: 5
> per-FG deep-dives + 5 cross-cutting-technique dives, each reading our dossiers +
> `apps/render_assistant/src/main.cpp` first-hand, then deep web research; + a chief-
> architect synthesizer). The FSR3 pacing math was read first-hand from the FidelityFX SDK
> MIT source. The full raw output (10 dims + the synthesis, ~245 KB) is the backing
> evidence; this dossier is the actionable distillation.
>
> **Verifiable-reference mandate (FDP).** First-hand-verified code claims are tagged `[VS]`
> with the site. External SOTA inherits its dive's level; no benchmark number is minted here.
>
> **Operator framing (governing).** The chosen bench is **BF6 max settings, RTX 4090 at
> 99% — the HARDEST case** (solve the worst case → the easier follow). The felt problem is
> **UNSTABLE latency** (heaviness + unstable mouse), which is **VARIANCE**, not just the
> mean. Integrate **gradually** ("de poco a poco") under the philosophy: máximo rendimiento
> · escalabilidad · facilidad de consumo. Budget free; depth over brevity.

---

## §1 — The verdict: one convergent architecture, our primitives shipped OFF

The ten dives collapse onto ONE picture. **Every shipping FG (FSR3, DLSS-G, LSFG, AFMF2,
XeSS-FG) solves saturated-latency with the SAME three-layer split**, and PhyriadFG — an
external-capture, MV-free, DComp-overlay member of LSFG's exact class — **has built the
right primitives but ships them OFF and un-orchestrated.** The operator is running the
exact synchronous-blocking path the SOTA structurally avoids.

The measured symptom (BF6 combat, `bf6_p1_baseline.csv`, `[VS]`): stable regime 240 fps /
AddedLatency 24 ms / slip 0.06 ms; **saturated regime** present ~100 fps / **AddedLatency
24→85 ms (p95 124, max 153)** / **slip 0.06→5.3 ms** / present interval erratic (CoV 0.65)
/ warp balloons 1.2→5.2 ms / **4090 99% while the 1080Ti STARVES to 31%** / the present
**blocks synchronously** on the warp fence. The instability (variance) is the mouse-feel.

---

## §2 — The three layers (SOTA detail + our gap)

### Layer 1 — CLOCK: a display-locked, MOVING-AVERAGE, variance-damped target
- **SOTA (FSR3, MIT, first-hand):** the present target is a `SimpleMovingAverage<10>` of
  inter-present QPC deltas, `target = avg*0.5 − varianceFactor*var − safetyMargin`, with a
  `>10 fps / 0.1 s` **reset** so a hitch never poisons the window. The **minus-variance term
  is the explicit anti-spike**: jitterier frametimes pull the target IN, so a saturated/
  erratic GPU cannot lock a slow interval and then overshoot. Defaults `safetyMargin 0.1 ms`,
  `varianceFactor 0.1`; tuning-set A `{0.75 ms, 0.1}`. Spin-finished (coarse-sleep to
  target−margin under `timeBeginPeriod(1)`, then a pure QPC busy-spin). DLSS-G/LSFG instead
  lock to the **DWM composition clock** (`DwmGetCompositionTimingInfo` `qpcVBlank`).
- **OUR GAP `[VS]`:** `paced_wait_P` (`main.cpp:6931`) targets a **free-running self-anchored
  `refresh_hz` NCO** at a **constant** interval — `grep DwmGetCompositionTimingInfo|qpcVBlank`
  = **0 hits**. No moving-average, no variance term. Our internal slip is ~0 **by
  construction** (measured against our own timer), which is exactly why slip 0.06 ms in the
  stable regime says nothing about real compose drift. The **spin-finish IS banked**
  (`hal::cpu_wait_for_ns` + `timeBeginPeriod(1)` at `main.cpp:2908`) — only the moving-average
  + DWM lock are missing. = roadmap **P0**.

### Layer 2 — PRESENT: a DECOUPLED, non-blocking, DROP-ON-SLIP present
- **SOTA:** a dedicated HIGHEST-priority thread owns the present, decoupled from the
  generation fence; present non-blocking; **drop the late interpolated frame, always present
  the real** (FSR3 `presenterThread`, DLSS-G "SL pacer", AFMF driver flip-meter). The 2 ms
  spin-vs-event-wait split: predicted-time-to-target > 2 ms → event-wait the fence (free the
  core); < 2 ms → spin (precision). A dropped interpolated frame is one repeated frame
  (invisible at 240 Hz); a 5 ms present stall is a felt hitch.
- **OUR GAP `[VS]`:** the shipping default **BLOCKS** —
  `vkQueueSubmit(A.q,…,fBridge); vkWaitForFences(A.dev,1,&fBridge,VK_TRUE,UINT64_MAX)` at
  `main.cpp:7003/7152/7510`. Under the pegged 4090 the warp balloons and the present
  **inherits its variance verbatim** (slip 0.06→5.3 ms, CoV 0.65). **The decouple+drop is
  ALREADY BUILT** — `--async-present` (STAGE-102): the `async_inflight/async_front` machine,
  the non-blocking `vkGetFenceStatus` poll (`main.cpp:7218`), the drop-this-tick logic
  (`main.cpp:7228`) — but **DEFAULT-OFF**. = roadmap **P3** (T2, crash/device-loss class).

### Layer 3 — COST: bound the per-tick slice; hold a target output rate
- **SOTA:** keep a slice for the FG pass by **capping the base game** (FSR3/LSFG/AFMF L1 —
  forbidden to us by dogma + structurally unavailable to an external tool) OR **bounding the
  per-tick cost**: reduced-internal-res flow (LSFG "Flow Scale", FSR3 `displaySize/8`
  fixed-grid 8×8 SAD = content-independent bounded cost), async-compute overlap, and the
  **fractional output-multiplier controller** (LSFG-3.1 AFG / DLSS 4.5: hold a target output
  fps, FLOAT the realized multiplier — never over-produce, never cap the game).
- **OUR GAP `[VS]`:** **no fractional output controller** — `grep
  target_output|frac_mult|realized_mult` = **0 hits**; under combat the multiplier collapses
  to ~1.07× **uncontrolled** (= roadmap **P1**, "the single first build"). `--flow-scale`
  exists but is **INIT-ONLY** (`flow_scale_auto`, `main.cpp:845/3062`, "STARTUP pick NOT
  runtime"). Our saturation response is the **SOTA-WRONG move**: `--load-governor` +
  `--deficit-tier` + `warp_light` **SHED quality/passes** (FG_SATURATION F5 names this "the
  exact opposite" of keep-generating-and-drop-the-late-frame). The make-space router is dead
  (= **P5**, §4 Phase D).

---

## §3 — Transferable vs NOT (the honest class boundary)

**Transfers directly onto our DComp overlay** (the CPU-side present/pacing subsystem is
queue-agnostic): (1) the FSR3 variance-aware moving-average pacing target — **the single
highest-value steal**, pure CPU over telemetry we already log; (2) the decoupled
drop-on-slip present — **already built** (`--async-present`); (3) the spin-finish — banked;
(4) shallow-queue / queue-depth-1 — built (`--shallow-queue`); (5) the DWM-compose phase
lock — 1 API call; (6) reduced-res flow tied to util — mechanism exists; (7) the fractional
output controller — output-side, dogma-clean; (8) the hysteretic fallback (AFMF2 evidence —
microstutter = "opening/closing the fallback window too fast") — debounce `--motion-fallback`
reusing the `--inertia` counter; (9) **OUR MULTI-GPU SLICE — the one place we BEAT
single-GPU saturation at the ROOT**: route flow→1080Ti, OFA→NVOFA engine, convert→iGPU so
the 4090 warp overlaps the game against **disjoint silicon** (no SM contention) — the clean
async overlap a single-GPU tool structurally cannot achieve.

**Does NOT transfer (needs engine/driver data we cannot have):** Reflex / Anti-Lag 2 / XeLL
render-queue elimination + just-in-time input (the 28–40% mean-latency win); NVIDIA Frame
Warp / Reflex 2 (engine camera+depth+late-latched input — our `--camera-twarp` is the
rotation-only depthless approximation); hardware Flip Metering (Blackwell) + Independent
Flip / VRR ownership (an external overlay cannot drive monitor VRR — the "G-Sync inversa"
door is closed); the swapchain PROXY (FSR3/DLSS-G intercept the GAME's Present; we present a
separate composited overlay → a structural double-present + one-compose capture beat);
engine-MV+depth disocclusion masks; **the L1 base-game CAP** (their primary saturation
lever — forbidden by our no-cap dogma; **our equivalent IS make-space on a second GPU**);
`DXGI SetMaximumFrameLatency` (the GAME's swapchain API, not ours); the queue-PRIORITY /
preemption lever (proven dead on consumer GPUs — **do NOT re-propose**).

---

## §4 — The ordered integration plan (gradual, dependency-correct)

The operator's symptom is **VARIANCE**. Build in this order; each is default-off /
byte-identical-off until a measured BF6-combat eye + CSV win (**the gate: slip-CoV drops +
present-interval variance flattens under ~4 ms** — the NVIDIA "Variable Frame Timing"
perceptual boundary; ~12 ms is clearly degraded).

### ▶ STEP 0 — the variance-aware moving-average pacing target  ·  `perf`+`quality` · `T0` · **the immediate first move**
The cheapest, safest, highest-ROI slice of P0, **decoupled from the DWM read** (the FSR3
brief: the variance term "matters MORE than the DWM read for this symptom"). In the present
pacer's CALLER, keep a `SimpleMovingAverage` of the last ~10 measured present-to-present
deltas (we already capture `now_ms()` per tick) + an online variance, and replace the
constant target interval with `target = movingAvg − varianceFactor·sqrt(var) − safetyMargin`,
**reset when a delta exceeds ~100 ms** (scene-cut/hitch). Start at FSR tuning-set-A
`{safetyMargin 0.75 ms, varianceFactor 0.1}`. **Pure CPU math over telemetry we already log;
does NOT touch the GPU path; default-off behind a flag, byte-identical off.** Directly
collapses the CoV 0.65 erratic interval — the operator's felt unstable-mouse is the ladder
lurch. Eye + CSV in BF6 combat.

### STEP 1 — complete P0: the DWM compose-clock phase lock  ·  `quality`+`ux` · `T0→T1`  **(gates P1, P3, P5-present)**
Read `DwmGetCompositionTimingInfo(NULL,&dti)` on the present thread at **~1 Hz** (NOT
per-frame — `qpcVBlank` is noisy, can report a vblank before the previous or in the past),
extract `{qpcVBlank, qpcRefreshPeriod}`, predict next-compose `= qpcVBlank + n·qpcRefreshPeriod`,
and fold the phase error into the **existing `--sync-clock` `T_robust` PLL** so
present-pacing and content-selection share ONE display-locked clock. Keep the spin-finish.
**NEVER `DwmFlush`** (it blocks). Removes the timer-vs-composition drift our free-running NCO
is blind to, AND gives the drop-on-slip test (STEP 3) an honest present instant to judge
"late" against.

### STEP 2 — P1: the fractional output-multiplier controller  ·  `perf`+`quality`+`ux` · `T1`  **(depends on STEP 1)**
The roadmap's named "single first build" and the first user-felt win. A controller over
signals already computed (`warp_ms`/`flow_ms`/`g_gpu_a_util`/`slip`/`ring_occ`): hold
`target_output_fps`, FLOAT `realized_mult = clamp(target/measured_base)`, so combat collapses
to a CONTROLLED fractional rate instead of an uncontrolled 1.07×, and the present interval
stays flat. **MUST NOT cap the game** (external, dogma-clean). The ONE "set your fps and
forget it" knob (LSFG AFG / DLSS 4.5) — the Auto behind the future quality↔latency slider.

### STEP 3 — P3: flip `--async-present` default-ON + drop-on-slip + util-tied flow-scale  ·  `perf` · `T2` → **RISK_REGISTER (DEVICE_LOST lead)**  **(depends on STEP 1)**
THE variance fix. After a clean 30 s BF6-combat crash gate + byte-identical-off proof,
replace the synchronous `vkWaitForFences(fBridge,UINT64_MAX)` (`main.cpp:7003/7152/7510`)
with the built `vkGetFenceStatus` poll-and-present-freshest + drop-the-late-tick (via the
`--rfp` bslot path); add the 2 ms spin-vs-event-wait split; keep `--shallow-queue` as the
depth companion; **tie `--flow-scale` to runtime `g_gpu_a_util`** (shrink the slice when the
4090 pegs, instead of the governor's quality-shed — retire the SOTA-wrong move). Converts the
warp spike into a quiet drop. Tier-2 → a `*_RISK_REGISTER` is **mandatory**, no commit while
any risk is `open`.

### STEP 4 — P5: the make-space router (the unique escape)  ·  `scale` · `T2` → **RISK_REGISTER**  **(depends on STEP 1 + operator-gated POD change)**
Attack the variance at its SOURCE. Wire the dead `break_even` router: add the fp16/DP4a field
to `GpuDescriptor` (**a cross-process POD layout change → MUST get operator approval**),
`measure_transfer_bw` at real gen×lanes, per-rig break-even (**never ship the author's ~596
FLOP/byte**), gate on render-GPU-saturated AND secondary-clears-transfer+compute. Route
flow→1080Ti, **OFA→the NVOFA dedicated engine OFF the 4090 compute cores** (1.12 ms @1080p,
loses 0.8–3.0% under a pegged 4090, BOTH_DIRECTIONS, its own queue family idx 5),
convert→iGPU, warp→4090; extend `--upload-xfer` to a true three-queue split so the present
stops contending on A.q. Shrinks the warp slice so the async present DROPS less and PACES
flatter — the make-space LSFG's dual-GPU validates but its single-GPU shader cannot pull.
**This is the surpass lever: keep generating + shed from idle silicon + never cap the game.**

### STEP 5 (later) — the present-side floor
Wire `PresentSurface::submit_at` to `IPresentationManager` (`SetTargetTime` + skip-to-latest
free drop + `SHARED_DISPLAYABLE` independent-flip = −1 compositor frame). Today it returns
`ErrorCode::Unavailable`.

**Dependency spine:** STEP 0 (variance pacer) ─ stands alone ─► STEP 1 (DWM lock) → { STEP 2
controller, STEP 3 decouple+drop, STEP 4 router-present-pin }.

---

## §5 — Honest floors (named, not chased)

None of this lowers the inherent **~1-source-frame interpolation HOLD** (~12–17 ms,
definitional, identical to LSFG/DLSS-G/FSR3). We get **no in-engine Reflex/Anti-Lag/XeLL
offset** → structurally ~1–2 frames worse *mean* than in-engine FG on a saturated single
dGPU; the lever is **STABILITY (variance), not the mean**. We cannot drive monitor VRR,
cannot ride Independent Flip / hardware Flip Metering, **cannot preempt a saturating dispatch
(queue priority is a proven dead-end — do NOT re-propose)**. LSFG's "BF6 at 240 sin
despeinarse" is the **capped/headroom-left (likely dual-GPU)** regime, NOT a pegged-GPU
miracle — our honest equivalent of its headroom is **make-space (STEP 4)**, the one escape
its single-GPU vendor-agnostic shader cannot pull.

## §6 — References (leveled)

External SOTA is owned by the per-target dives of `wp0cmcjfd` and the sibling dossiers
(`FG_SATURATION_PRIOR_ART`, `FG_CADENCE_LATENCY_PRIOR_ART`, `FG_NVOFA_PRIOR_ART`,
`FG_OPEN_ALTERNATIVES_PRIOR_ART`), each carrying its own `[V1]/[V2]/[V3]` levels; this
dossier inherits them. The FSR3 pacing math is first-hand from the FidelityFX SDK MIT source
(`FrameInterpolationSwapchainDX12.cpp` + `_Helpers.cpp`). The OUR-STATE `[VS]` claims
(synchronous fence at 7003/7152/7510; 0 DWM hits; 0 fractional-controller symbols; the dead
router; `--async-present` built default-off) were verified first-hand against
`apps/render_assistant/src/main.cpp` this session.
