# FG_TESTBENCH_IMPLEMENTATION_STRATEGIES — concrete build steps for the controlled FG test environment (TB-C1–TB-C7 + the TB-C12 minigame)

> **Diátaxis type:** How-to / strategy (the buildable companion to the MASTER_PLAN's
> Explanation; placed in the planning family). **Status:** `designed` — the testbench is
> NOT built; every component below is a spec with verified reuse points, not shipped code.
> **Plan tier:** **Tier 2 (risk-bearing)** per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)
> §1.1 triggers 1 (TB-C3 can TDR the GPU) and 5 (a render_assistant `main.cpp` change risks the
> byte-identical-off dogma).
>
> **Document set (mutually linked — tier protocol §2.3):**
> - [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) — the why, the three axes, the
>   phases/gates (sibling, authored in parallel);
> - this `_IMPLEMENTATION_STRATEGIES` — the concrete per-component edit/build sites + the
>   byte-identical-off discipline + validation;
> - [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md) — every TB-* failure mode with
>   its mitigation AS CODE + first-hand verification (Tier-2 gate: no `open` risk at commit).
>
> **Verifiable-reference mandate (FDP §2).** Every in-repo `path:line` / API below was re-verified
> first-hand against the working tree this session. Anchors I could not confirm first-hand are tagged
> **(to verify)** rather than invented. External figures carried from the task brief are tagged
> **(brief — to verify)**.
>
> **Normativity (BCP 14):** MUST / MUST NOT / SHOULD / MAY per RFC 2119 / RFC 8174, only where a real
> constraint binds.

---

## §0 — The discipline that binds every strategy

1. **Default-off, byte-identical-off (the TB-C2/TB-C7 dogma).** Any `main.cpp` change (TB-C2 bounded-run,
   TB-C7 tag-emit) MUST be reachable only behind a new flag whose default leaves the existing code path
   literally unentered (`if (cfg.X)` with `X` defaulting to today's value). Validated by a
   `--qdump`/`--outdump` byte-diff = 0 vs the pre-change build. → **TB-3, TB-6.** The precedent is
   already in the tree: the device-lost detector `vk_live` is identity on `VK_SUCCESS` so the wrapped
   sites are byte-identical when no device is lost (`apps/render_assistant/src/main.cpp:3024-3036`, the
   `DEVICE_LOST_RECOVERY_RISK_REGISTER` CR4/CR5 pattern). Copy that shape.
2. **Verify the site before editing.** The line numbers below are this-session-verified; re-grep before
   touching — the codebase drifts (the HDR sibling's dossier numbers had already drifted once).
3. **Soft, not hard, on a platform refusal.** A swapchain-format request, an NVML load, a WGC capture
   of the source window — each MUST degrade (fall back / skip / mark-unavailable) rather than abort the
   process. A test harness that crashes the rig measures nothing.
4. **Cite the risk ID** at each edit/build site that exists to mitigate a TB-* risk (Tier-2
   traceability — tier protocol §2.3).
5. **Three axes in parallel (operator steer, 2026-06-24).** The objective axis (TB-C4 → the TB-C8 scorer),
   the perceptual axis (TB-C5 → the eye), and the photonic axis (TB-C6 → the real camera) are built
   concurrently on the shared TB-C1+TB-C2+TB-C3+TB-C7 foundation. The live-absolute ground-truth lever is TB-C7
   frame-ID alignment — the harder, more complete route — NOT a held-out withhold on the live path
   (which manufactures the artifact; see §8-A).

---

## §1 — TB-C1 SourceWindow — the deterministic synthetic-motion windowed app

**Goal.** A standalone, windowed, WGC-capturable app that animates a *known-per-frame-displacement*
pattern at a chosen resolution + fps, emits the exact closed-form midpoint as ground truth, and
stamps each frame with a decodable frame-ID. It replaces the uncontrolled real game as the FG's
capture source, so the full live chain (WGC → convert → flow → warp → present) runs against a
deterministic, scene-confound-free input. Three **skins** share one animation core: **GT** (clean
synthetic motion, the scorer's analytic target), **eye** (high-frequency edges + clear silhouettes for
the 2AFC presenter), **camera** (the high-contrast marker + calibration timer — TB-C6).

**Reuse points (verified first-hand):**

1. **The windowed present path** — `framework/render/vulkan/include/phyriad/render/vulkan/Swapchain.hpp`.
   A real `VkSwapchainKHR`, `VK_FORMAT_B8G8R8A8_UNORM`, `VK_PRESENT_MODE_FIFO_KHR` (vsync) — header
   lines 5-6. The frame API is `init(ctx, width, height)` (`Swapchain.hpp:53`), `acquire_next_image()`
   (`:71`), `submit_and_present(image_index)` (`:76`), `recreate(new_w, new_h)` (`:59`); double-buffered
   `kMaxFramesInFlight = 2` (`:39`). This is a normal windowed HWND present — exactly what WGC captures
   (no exclusive-fullscreen needed, confirmed by the FG's own capture path, §1.5 below). The
   `VulkanContext*` it takes supplies the WSI surface (the GLFW/Win32 surface-creation site is
   **(to verify)** — grep `VulkanContext` for the surface ctor before wiring).
2. **The analytic motion model** — `framework/render/vulkan/bench/stage31_extrapolation/prep_frames.py`.
   Closed-form integer-pixel translation `D = 16 px/step` via `np.roll` (`prep_frames.py:54-56`), with
   the exact temporal midpoint `M = frame at t=0.5` (`:21-27`) and a disocclusion preset
   (`gen_disocclusion`, `:177-200`). Port this displacement law into the GLSL/host animation loop: at
   frame *n* the pattern is translated by the closed-form `disp(n)`, so the midpoint between frame *n*
   and *n+2* is `disp(n+1)` — a *renderable* exact GT, not a withheld real.
3. **The richer scene + motion zoo** — `framework/render/vulkan/bench/fg_quality_scorer/prep_zoo_sequence.py`.
   Fixed seed `rng(7)` (`prep_zoo_sequence.py:78`) → byte-identical; 7 motion presets including
   `occlude` the crossfade-cause (`:30-44`); resolution-parameterized `--width/--height/--frames`
   (`:47`). The **eye** and **GT** skins reuse its non-repeating value-noise background (well-posed
   flow, no periodic-texture aperture confound — `:70-77`) plus the high-contrast foreground shapes.

**Build steps:**

- **B1.** New app dir `apps/fg_testbench/source_window/` (or `framework/render/vulkan/bench/testbench_source/`
  — placement decided in the MASTER_PLAN). `main.cpp` opens a `Swapchain` window titled with a fixed,
  matchable substring (e.g. `"PhyriadFG-TestSource"`) so the FG's `--window` substring matcher finds it
  (§1.5). CLI knobs: `--res WxH`, `--fps N`, `--skin {gt|eye|camera}`, `--preset {pan|...|occlude}`,
  `--seed` (default 7), `--frame-id {on|off}` (TB-C7), `--gt-emit DIR` (the GT hook).
- **B2 (GT-emit hook).** When `--gt-emit DIR` is set, write the closed-form midpoint frame `disp(n+1)`
  as raw RGBA8 (`W*H*4`, row-major, no header — the scorer's contract, `fg_quality_scorer/main.cpp`
  read path) plus append a `triple <name> prev=<n> mid=<n+1> next=<n+2> live=<liveFrameForId>` line to a
  manifest in the scorer's full-triple form (`fg_quality_scorer/main.cpp:119,145`). This is what lifts
  the live path from truth-LESS (today's `--qdump`, `:121-122`) to a real held-out-equivalent score —
  the synthetic source emits the GT that withholding a real cannot (§8-A).
- **B3 (frame-ID encoder — part of TB-C7).** Render a machine-decodable frame-ID into a reserved screen
  region each frame (see §7).

**Risk:** TB-C1 is a standalone app that reads nothing destructive and changes no shipping code → it
carries **no qualifying Tier-2 risk on its own.** Its GT-emit + frame-ID feed **TB-4** (alignment) and
its scene determinism is the *only* escape from the scene-confound half of **TB-5**.

### §1.5 — The capture handshake (why the FG will capture TB-C1 unmodified)

The FG captures **any** visible window whose title contains the `--window` substring, with no
game-specific gate: `enum_wnd_cb` requires only `IsWindowVisible(h)` + `strstr(title, substr)`
(`main.cpp:1930-1933`), resolved by `find_window_by_substr` (`:1936`), then
`interop->CreateForWindow(wgc_target_hwnd, ...)` (`:4277`). `WDA_EXCLUDEFROMCAPTURE` is set ONLY on the
FG's own overlay (define `main.cpp:16-18`, note `:3207`) — TB-C1 simply MUST NOT set it, so same-monitor
capture is fine. TB-C1 therefore needs **zero** FG changes to be captured: launch TB-C1, then
`render_assistant --window PhyriadFG-TestSource`.

**Validation gate (TB-C1):** TB-C1 builds green; the FG captures it (`[ra] WGC: capturing window '...'`,
`main.cpp:4279`); a `--gt-emit` manifest scores clean through the TB-C8 scorer in full-triple mode.

---

## §2 — TB-C2 FG bounded-run mode (`--duration` / `--max-frames` / `--exit-after`)

**Goal.** Let the sweep orchestrator bound a per-config FG run: run for N seconds or M presents, then
tear down cleanly, finalize the `--csv`, and print the teardown descriptor counts. Without this the app
runs until Ctrl-C / window-death / device-loss only (grep for `--duration/--max-frames/--exit-after`
returns nothing today). **This is the first of the two `main.cpp` touches → TB-3 + TB-6.**

**Reuse points (verified first-hand):**

1. **The flag parser** — add the new flags in the `parse_extra` lambda (`main.cpp:1124`), **NOT** the
   main `else-if` chain (the main chain hits the MSVC C1061 nested-`if` limit; this is why `--csv`
   (`:1157`), `--async-present` (`:1158`), `--present-colorspace` (`:1191`) etc. all live in
   `parse_extra`). Copy the `--present-colorspace` shape (`:1191`) for a value flag; `--async-present`
   (`:1158`) for a bool. Add the `cfg` fields next to `csv_path` (`:698`).
2. **The existing clean-exit path** — `g_quit` (declared `volatile bool` at `main.cpp:3012`) is the
   single unwind signal. It is set by the console Ctrl-C handler (`console_ctrl_handler`, sets
   `g_quit=true` at `:3039`) and by the device-lost detector (`vk_live`, `:3033`). Every present-thread
   loop exits at its next `while(!g_quit...)` check → the existing `done:` teardown runs. TB-C2 sets the
   SAME `g_quit` from a deadline check — **no new exit machinery** (the documented design intent at
   `:3026-3028`). → **TB-3.**
3. **The CSV finalize** — `TelemetryCsv::stop()` already signals + joins the drain thread + flushes +
   writes the `<path>-stats.csv` and is idempotent / RAII (`telemetry_csv.hpp:72-73`). The bounded exit
   path already flows through scope-destruction, so `stop()` fires; TB-C2 only needs to ensure the deadline
   exit reaches the SAME teardown (it does, via `g_quit`). Verify `tcsv` is destroyed on the `done:`
   path (it is started at `main.cpp:7303-7306`).

**Build steps:**

- **B1.** `cfg.run_max_ms` / `cfg.run_max_frames` (default 0 = unbounded → byte-identical). Flags
  `--duration <s>`, `--max-frames <N>`, `--exit-after <s>` (alias) in `parse_extra`.
- **B2.** In the present loop, near the per-present `--csv` push (`main.cpp:9190-9211`), add a guarded
  deadline check: `if (cfg.run_max_ms && now_ms()-t_start >= cfg.run_max_ms) g_quit=true;` and the
  frame-count analogue against `csv_fi` (the present counter, `:9195`). The guard is unentered at the
  default 0 → byte-identical-off. → **TB-3, TB-6.**
- **B3 (descriptor-count print).** On the `done:` teardown, before destroying the device, print the
  live descriptor / resource counts (the app already tracks its pools; **the exact counters to read are
  (to verify)** — grep the `done:` block for the VkDescriptorPool / image vectors). This is the clean-
  teardown evidence the gate checks.

**Risk:** **TB-3** (touches the exit/present path — a deadline that races the present thread or skips a
join could leak the drain thread or present a torn frame) and **TB-6** (any `main.cpp` change must be
byte-identical-off). Both detailed in the RISK_REGISTER; mitigation is the reuse-the-existing-`g_quit`-
path discipline above + the byte-diff gate.

**Validation gate (TB-C2):** `--duration` absent → `--qdump` byte-diff = 0 vs pre-change build
(byte-identical-off). `--duration 5` → process exits at ~5 s, `<path>-stats.csv` is written and parses,
descriptor-count line printed, validation layers clean on the run, no leaked drain thread (the watchdog
`telemetry_csv.hpp:95-103` does not fire a terminal STALL).

---

## §3 — TB-C3 SaturationLoad — a co-runnable graphics+present load with NVML closed-loop

**Goal.** A controllable GPU load that pressures the **graphics queue + present** like a real game
(NOT compute-only) and holds a target util % via NVML feedback, so the FG can be measured under a
known, repeatable saturation regime instead of an uncontrolled external game. **This is the risk-
bearing component** (it deliberately stresses the GPU). → **TB-1, TB-2.**

**Why graphics+present, not a compute kernel (TB-2).** The phenomenon under test is a *present-
architecture / graphics-queue* contention bottleneck — the FG slice host-blocks on the graphics queue
and the present path (per the render-assistant arc's measured make-space findings). A pure compute
burn pegs the ALU but does NOT reproduce the queue/fence/present contention. TB-C3 MUST therefore submit
**real graphics work AND present** on the graphics queue, using the compute burn only as the tunable
intensity knob. This load is **unsourced in the VFI SOTA** — we pioneer it (§8-B); its realism is the
single most important design property, not its convenience.

**Reuse points (verified first-hand):**

1. **The tunable burn primitive** — `framework/render/vulkan/bench/stage9_singlegpu/shaders/game_render.comp`.
   Push-constant `PC { uint frame; uint iters; }` (`game_render.comp:10`); the burn loop
   `for (uint i=0; i<pc.iters; ++i) { ... }` (`:19-20`); rgba8 storage output (`:9,21`). `iters` is the
   light→saturated knob the comment already documents (`:2-6`). TB-C3 reuses `iters` as the intensity the
   NVML controller modulates.
2. **The graphics present** — `Swapchain.hpp` (as in TB-C1: `init`/`acquire_next_image`/`submit_and_present`,
   lines 53/71/76). TB-C3 is a windowed app that each frame (a) does heavy graphics work — port the
   `game_render.comp` burn into a fragment shader over a full-screen draw, OR keep it as a compute pass
   feeding a sampled draw — and (b) `submit_and_present`s on the graphics queue. The present + graphics
   submit is the realism element (TB-2); the `iters` knob is the magnitude.
3. **The NVML util reader to close the loop** — `apps/render_assistant/src/main.cpp:7260-7295`:
   `LoadLibraryW(L"nvml.dll")` (`:7269`, with the NVSMI fallback path `:7270`),
   `nvmlDeviceGetUtilizationRates` resolved at `:7278`, device matched by name (`:7289-7290`). This is
   locked inside `main.cpp`, not a pillar API → TB-C3 MUST factor it into a small shared helper OR re-
   implement the same `LoadLibrary + GetProcAddress("nvmlDeviceGetUtilizationRates")` pattern (it is
   ~30 lines). → **TB-1** (the controller reads achieved util to stay below the TDR-risk ceiling).

**Build steps:**

- **B1.** New app `apps/fg_testbench/saturation_load/`. Knobs: `--target-util P` (0–100),
  `--device <name-substr>` (matched like the FG's NVML matcher), `--max-iters` (the safety ceiling),
  `--ramp-ms` (gradual approach).
- **B2 (closed loop).** Each control tick (~the NVML ~40 Hz cadence the FG uses): read util; adjust
  `iters` by a clamped proportional step toward `--target-util`; **never exceed `--max-iters`**. Hold,
  do not overshoot.
- **B3 (TDR safety — TB-1).** A watchdog: if a single submit-to-present interval exceeds a hard cap
  (the Windows TDR delay is ~2 s; cap well under, e.g. 250 ms), back `iters` off immediately and, on a
  repeated breach, abort the load cleanly. Ramp up (`--ramp-ms`), never step to max in one frame. The
  load app MUST be killable by the orchestrator out-of-band (so a hung load cannot wedge the rig).

**Risk:** **TB-1** (sustained graphics+present stress can TDR the GPU / destabilize the system — the
crash class) and **TB-2** (a compute-only load would NOT reproduce the graphics-queue/blocking-present
bottleneck — the realism class). Both are RISK_REGISTER entries with mitigation-as-code (the iters
ceiling + the per-frame TDR watchdog + the ramp). TB-C3 MUST NOT be committed while either is `open`.

**Validation gate (TB-C3):** TB-C3 holds `--target-util 90` within a tolerance band on the 4090 for a 60 s
run with the TDR watchdog clean (no device-lost, no `nvlddmkm` reset); abort-on-breach exercised once
and observed clean.

---

## §4 — TB-C4 SweepOrchestrator — `{source × load × FG} → bounded run → A/B matrix`

**Goal.** One parameterized driver that, for each cell of `{source(res, fps, skin) × load(util) ×
FG(flags)}`, launches TB-C1 + TB-C3 + the bounded FG (TB-C2), harvests the `--csv` stats + the TB-C8 GT-quality
score, and aggregates into an A/B matrix. The offline regression scripts already do this shape for the
shader benches; nothing does it for the live external-capture pipeline.

**Reuse points (verified first-hand):**

1. **The orchestration template** — `scripts/check_fg_quality_regression.ps1`. Model TB-C4 on it: pure
   PowerShell, `param(...)` block (`:59-68`), deterministic exit codes (`:47-51`), the
   build-scorer → regenerate-fixture → score → compare-per-cell flow, `-CsvPath` to re-judge an existing
   CSV, `-Regenerate`, `-UpdateBaseline`. TB-C4 is the live-path analogue: build/launch → run-bounded →
   harvest → compare.
2. **The machine-readable harvest** — the FG `--csv` (`telemetry_csv.hpp`, flag `main.cpp:1157`): the
   raw per-present CSV + the derived `<path>-stats.csv` (`write_stats`, `telemetry_csv.hpp:365-424` per
   the inventory — **the exact line is (to verify)**, but `stop()` writing the stats file is confirmed
   at `:72`). The stats give `fg_multiplier`, 1%/0.1% lows, P99 frametime, stutter, and
   `fg_4090_occupancy` — the per-cell objective row.
3. **The GT-quality score** — the TB-C8 scorer (`fg_quality_scorer/main.cpp`) consuming TB-C1's `--gt-emit`
   manifest (full-triple form, `:119,145`) aligned by TB-C7. One CSV row per triple per mode.

**Build steps:**

- **B1.** `scripts/fg_testbench_sweep.ps1`: nested loops over the sweep axes; per cell, launch TB-C1
  (skin/res/fps), launch TB-C3 (`--target-util`), wait for the load to settle, launch the FG with
  `--window <TB-C1 title> --csv <cell>.csv --duration <s>` + the FG flags under test, join all three,
  parse the stats CSV, run the scorer on the cell's `--gt-emit` dir, write one matrix row.
- **B2.** Aggregate to a per-cell A/B matrix (CSV + a summary). Deterministic exit code (0 = all cells
  ran + parsed). Inherit the TB-C3 kill-on-timeout so a wedged cell cannot stall the whole sweep.

**Risk:** TB-C4 drives TB-C3, so it inherits **TB-1** (it MUST enforce the out-of-band kill + a per-cell
wall-clock timeout so a TDR-risk load is bounded) and it is the locus of **TB-5** measurement validity:
the cross-clock present-timestamp errors (the observed `compose:24.9` artifact), EMA/window artifacts in
the derived stats, and the scene-confound — the last escaped ONLY because the source is TB-C1 synthetic, not
a real game. TB-C4 MUST record the raw per-present CSV (not only the EMA-derived stats) so a cross-clock
artifact is auditable post-hoc.

**Validation gate (TB-C4):** a 2×2×2 smoke sweep runs end-to-end, every cell produces a parseable stats
row + a scorer row, no orphaned TB-C1/TB-C3 process survives the sweep, and the matrix is reproducible run-to-
run on a fixed seed (determinism check — the source is byte-identical, so cell-to-cell variance is the FG
+ load, not the input).

---

## §5 — TB-C5 EyeHarness — blind 2AFC A/B presenter + Bradley-Terry / JOD analysis

**Goal.** A two-alternative-forced-choice presenter that shows the operator two outputs (two pre-
recorded TB-C1+FG captures OR two live configs), blind + randomized + counterbalanced, logs each verdict,
and analyses the verdict log into a ranking (Bradley-Terry / JOD) with a chi-square significance test
and an artefact-amplification mode. This is the **perceptual axis** — the operator's eye turned into a
defensible (within-subject) preference statistic.

**Reuse points (verified first-hand):**

1. **The recorded-output substrate** — TB-C1's `--gt-emit` / `--outdump` RGBA8 frames + the FG's
   `--qdump`/`--outdump` live-output readback (`main.cpp:4823-4831`, gated on `qdump_n>0` → zero cost
   off, `:4826`). A pre-recorded pair = two FG configs run over the SAME TB-C1 seed → frame-aligned by TB-C7,
   so the only difference the eye sees is the config, not the scene.
2. **The capture interchange** — the `.rgba` + manifest contract (`fg_quality_scorer/main.cpp` reader),
   shared with the scorer, so the same captured frames feed both the objective (TB-C4) and perceptual (TB-C5)
   axes.

**Build steps:**

- **B1.** A presenter (a small windowed player, reusing `Swapchain.hpp`, or an offline frame-pair
  viewer) that plays config-A and config-B in a randomized left/right order, hides which is which,
  counterbalances order across trials, and records `{trial, A_id, B_id, side_shown, verdict, latency}`.
- **B2.** `--amplify` mode: temporally hold / loop the disocclusion window (the `occlude` preset) so the
  artefact is perceptible — the harness's job is to make a real difference *visible*, not to flatter.
- **B3.** An analysis script (`scripts/fg_testbench_eye_analysis.{ps1|py}`): Bradley-Terry (or JOD)
  ranking over the pairwise verdicts + a chi-square on the win/loss counts vs the null (50/50).

**Risk:** **TB-5**, and specifically the hardest sub-case — **single observer = ITU "informal".** With
one observer this is, by the ITU definition, an informal assessment; it MUST NOT be reported as panel-
grade. The compensation is **many randomized, counterbalanced, within-subject trials** (the verdict
DATASET is the deliverable), never a claim of population validity (§8-D). The harness MUST log enough
trials for the chi-square to have power, and the analysis MUST report n and the within-subject scope
honestly.

**Validation gate (TB-C5):** a synthetic sanity pair (identical A==B) yields a non-significant chi-square
(no false preference); a known-different pair (FG-on vs FG-off, or `--amplify` on the `occlude` preset)
yields a significant, correctly-signed preference; the verdict log round-trips through the analysis
script.

---

## §6 — TB-C6 CameraKit — the camera skin + slow-mo footage analysis (the photonic axis)

**Goal.** Measure what the **display actually emits** — the photons — by filming the screen with a
high-speed camera and reading the TB-C1 frame-ID markers off the footage. This is the only axis that
captures present-time pacing / tearing / dropped-frame reality *outside* the software clocks (it escapes
the cross-clock half of TB-5 by using an independent physical clock — the camera's frame rate).

**Reuse points (verified first-hand):**

1. **The TB-C1 source + frame-ID** — TB-C6 is the TB-C1 **camera skin**: the frame-ID encoder (§7) plus a
   high-contrast, camera-legible marker (large, saturated, high-temporal-frequency so a rolling-shutter
   slow-mo can resolve it) plus an on-screen calibration timer (a known-rate counter to map camera
   frames ↔ wall time).
2. **The displacement law** — TB-C1's closed-form `disp(n)` (from `prep_frames.py:54-56`) means a filmed
   frame's marker decodes to *which* source frame it is, and the analytic GT for that instant is known.

**Build steps:**

- **B1.** Add the camera marker + calibration timer to TB-C1's `--skin camera` path (a reserved high-
  contrast screen band rendering the frame-ID in a camera-decodable code).
- **B2.** A footage-analysis script (`scripts/fg_testbench_camera_analysis.py`, OpenCV): per filmed
  frame, locate the marker band, decode the frame-ID, and build the *displayed* frame-ID timeline → from
  which cadence, judder, duplicate-display, and the real present interval are read directly off the
  photons.

**Risk:** **TB-4** (the footage analysis must survive missing / motion-blurred / rolling-shutter-split
markers — it MUST mark an undecodable filmed frame as *unaligned* rather than guess) and **TB-5** (a
single high-speed clip is still one observer's setup; report it as illustrative, with the camera rate +
exposure stated, not as a population metric).

**Validation gate (TB-C6):** film TB-C1 at a known fps with FG off → the decoded displayed-frame-ID timeline
matches the source cadence within the camera's resolution; undecodable frames are flagged, not
interpolated.

---

## §7 — TB-C7 FrameID alignment — the live-absolute ground-truth lever

**Goal.** Visually tag each source frame so the live FG output can be aligned to the analytic GT **even
under non-deterministic drop / pacing / governor behavior**. This is the lever that makes a real
ground-truth score possible on the LIVE shipped pipeline (not just the offline flow+warp slice), and the
operator's chosen route (the harder, more complete one). It threads TB-C1 (the tag) + TB-C4 (the scorer
alignment) + TB-C6 (the footage decode). **It is the second `main.cpp` touch (the tag-emit) → TB-6**, and
its alignment robustness is **TB-4**.

**Reuse points (verified first-hand):**

1. **The tag source** — TB-C1 renders the frame-ID into a reserved region each frame (§1 B3). A robust
   encoding: a small block-code (e.g. a binary/gray-code bar or a corner glyph) that survives the FG's
   warp + 8-bit quantization enough to decode the integer ID. It MUST sit where the warp will not smear
   it into ambiguity (a static screen corner the flow sees as MV≈0, analogous to the `hud_static`
   preset's static-HUD region, `prep_frames.py:147-174`).
2. **The de-tag (capture / scorer side)** — the scorer's manifest already distinguishes a full triple
   (`mid=`, `fg_quality_scorer/main.cpp:119,145`) from the truth-less `--qdump` form (`:121-122`). TB-C7's
   alignment writes, per captured FG frame, the decoded source frame-ID, and the scorer pairs that FG
   frame with the TB-C1 `--gt-emit` midpoint of the SAME ID — so a dropped/re-paced FG frame is scored
   against the *correct* analytic GT, or honestly skipped.
3. **The live FG output tap** — `--qdump` already dumps the live `wapOutA` warp output + a manifest
   (`main.cpp:1858, 4823-4831`; readback buffers `:3691-3693`). TB-C7's `main.cpp` tag-emit extends THIS
   path (measurement-only, default-off) to also record the decoded source frame-ID per dumped frame into
   the manifest, so the offline scorer can align without a separate decode pass.

**Build steps:**

- **B1 (TB-C1 side).** The frame-ID encoder in the source (no shipping-code change — TB-C1 is new).
- **B2 (`main.cpp` tag-emit — TB-6).** In the `--qdump` dump path (`main.cpp:1858` / the readback at
  `:4823-4831`), behind a new `parse_extra` flag `--frame-id-emit` (default off), decode the source
  frame-ID from the captured input (or pass through the ID the convert stage already saw) and append it
  to the qdump manifest line. Default-off → the dump path is byte-identical (the `qdump_n>0` gate
  already zero-costs the buffers, `:4826`). Validate with a byte-diff = 0 vs pre-change when the flag is
  absent.
- **B3 (alignment, drop-robust — TB-4).** The scorer/aligner builds the mapping `decoded-FG-ID →
  GT-midpoint-ID`. When an ID is missing (dropped) or decodes ambiguously, mark that FG frame
  **unalignable** and exclude it from the fidelity average (with a logged count) — NEVER silently align
  to the nearest ID. The unalignable fraction is itself a reported metric (it measures the live path's
  drop behavior under load).

**Risk:** **TB-4** (the live path's defining behavior under load is to drop / pace / govern frames non-
deterministically — alignment MUST survive that or honestly mark frames unalignable) and **TB-6** (the
tag-emit `main.cpp` change must be byte-identical-off). Both are RISK_REGISTER entries.

**Validation gate (TB-C7):** `--frame-id-emit` absent → qdump byte-diff = 0. With it on: a TB-C1 run at a
known drop rate (forced via TB-C3 saturation) produces an alignment with the expected unalignable fraction,
and aligned frames score against the correct GT (a deliberately-misaligned control scores worse — proving
the alignment is load-bearing, not cosmetic).

---

## §8 — Honest hard-truths (stated plainly, per FDP §4.4 / §2.4)

These are NOT defects to hide; they are the boundaries the testbench is designed around.

- **(A) Live-path absolute ground-truth is structurally hard.** The canonical held-out protocol
  withholds a real frame and scores against it — but on the LIVE path that *manufactures the very
  artifact being measured*, which is the code-verified reason `--qdump` ships truth-LESS (no `mid=`,
  `main.cpp:226-227`, `fg_quality_scorer/main.cpp:121-122`). TB-C7 frame-ID alignment is the **mitigation,
  not a guarantee**: it lets a *synthetic-emitted* exact GT (TB-C1 `--gt-emit`) align to the live output by
  ID, but only for frames the FG actually presents and whose tag decodes. Frames it drops/re-paces are
  honestly excluded, not fabricated.
- **(B) The saturation load (TB-C3) is UNSOURCED in the VFI SOTA — we pioneer it.** No standard FG benchmark
  specifies a co-runnable graphics+present saturator. That means it carries no external validation AND
  that it MUST be graphics+present (TB-2), not compute-only, or it measures the wrong contention. We own
  both the design and the burden of showing it is representative.
- **(C) The eye-proxy is an OPEN PROBLEM.** Calibrating the TB-C8 scorer's metrics to a mean-opinion score
  is unsolved for FG: no standard metric is reported to exceed ~PLCC 0.6 for frame interpolation (the
  task brief cites BVI-VFI with LPIPS ≈ 0.597 as the best — **(brief — to verify)** against the BVI-VFI
  source; not first-hand confirmed here). Therefore the **verdict DATASET (TB-C5) is the deliverable**; a
  metric→MOS proxy is research, not a shipped claim. TB-C4's numbers and TB-C5's verdicts are reported side by
  side, never collapsed into a single "quality" scalar that implies a calibration we do not have.
- **(D) Single observer = ITU "informal".** With one observer, TB-C5/TB-C6 are informal by the ITU definition.
  The compensation is many randomized, counterbalanced, within-subject trials with n and significance
  reported — never a claim of panel-grade or population validity.

---

## §9 — Build order + the gate map

**Foundation (parallelizable):** TB-C1 (source + GT-emit + frame-ID encoder) ‖ TB-C2 (FG bounded-run) ‖ TB-C3
(saturation load). Then TB-C7 (alignment + the `main.cpp` tag-emit) once TB-C1's encoder + TB-C2's bounded run
exist. Then the **three axes in parallel** (operator steer): TB-C4 (objective) ‖ TB-C5 (perceptual) ‖ TB-C6
(photonic), all on the TB-C1+TB-C2+TB-C3+TB-C7 foundation.

| # | Component | Touches `main.cpp`? | Risk IDs | Gate to pass |
|---|---|---|---|---|
| TB-C1 | SourceWindow (3 skins + GT-emit + frame-ID) | No | (feeds TB-4, escapes TB-5 scene-confound) | build green; FG captures it; GT manifest scores clean |
| TB-C2 | FG bounded-run (`--duration`/`--max-frames`/`--exit-after`) | **Yes** | **TB-3, TB-6** | byte-identical-off; clean teardown + stats CSV + descriptor print |
| TB-C3 | SaturationLoad (graphics+present + NVML loop) | No (factors NVML out) | **TB-1, TB-2** | holds target util 60 s, TDR watchdog clean, abort exercised |
| TB-C7 | FrameID alignment (+ `main.cpp` tag-emit) | **Yes** | **TB-4, TB-6** | byte-identical-off; correct-GT alignment + honest unalignable fraction |
| TB-C4 | SweepOrchestrator | No (drives TB-C2/TB-C3) | **TB-1** (inherited), **TB-5** | smoke sweep end-to-end, no orphan procs, reproducible |
| TB-C5 | EyeHarness (2AFC + BT/JOD + χ²) | No | **TB-5** | identical-pair → null; different-pair → significant; log round-trips |
| TB-C6 | CameraKit (camera skin + footage decode) | No | **TB-4, TB-5** | decoded displayed timeline matches source cadence |

**Cross-cutting gates (every component):**
- **Build green** — the MSVC build clean; validation layers clean on a 30 s run (the project build
  invocation is `cmake --build build-ra-msvc2` after `vcvars64`, per the render-assistant arc —
  **the exact target name is the arc's; re-confirm before relying on it**).
- **Byte-identical-off** — for TB-C2 and TB-C7 ONLY (the `main.cpp` touches): `--qdump`/`--outdump` byte-diff
  = 0 vs the pre-change build with the new flag absent. → TB-3, TB-6.
- **Lint** — `lint_hal` (no raw `memory_order_*` in `apps/`; default `seq_cst`), `lint_docs`, and
  `check_inventory` (every new code file — TB-C1/TB-C3 apps, the TB-C4/TB-C5/TB-C6 scripts — added to
  [`INVENTORY.md`](../../INVENTORY.md) in the same commit).
- **Tier-2 commit gate** — no component with an `open` risk in
  [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md) may be committed (TB-1 and TB-2 on TB-C3,
  TB-3/TB-6 on TB-C2/TB-C7, TB-4 on TB-C6/TB-C7, TB-5 on TB-C4/TB-C5 must each be `mitigated` or `accepted`).

---

## §10 — What could NOT be verified first-hand (honest gaps)

- **`telemetry_csv.hpp:365-424` (`write_stats`)** and the stall-watchdog lines `:95-103,321-348` are
  carried from the inventory; this session confirmed `CsvRow` (`:33-53`), `push` (`:63-70`), `start`
  (`:58`), and `stop` (`:72-73`) first-hand but did not re-read the stats-writer body — the specific
  stat-line numbers are **(to verify)** before citing in code.
- **`capture_dump`'s all-black detector** (`kBlackMean`/`kBlackVar`, inventory ~`:495-520`): the file
  `apps/render_assistant/tools/capture_dump/main.cpp` exists (confirmed), but those internal lines were
  not read first-hand → **(to verify)**.
- **`OffscreenScene.hpp` internal lines** (inventory `:46,64,71`): the file exists (confirmed); the
  static-checker / external-VkImage line numbers were not re-read → **(to verify)**. (TB-C1 uses
  `Swapchain.hpp`, fully verified, not `OffscreenScene`.)
- **The `VulkanContext` WSI-surface creation site** (GLFW/Win32) that TB-C1+TB-C3 need is **(to verify)** —
  `Swapchain::init` takes a `VulkanContext*` (`Swapchain.hpp:53`) but the surface ctor was not located
  this session.
- **The TB-C2 descriptor-count counters** on the `done:` teardown are **(to verify)** — grep the teardown
  block for the actual Vk pool / image vectors before wiring the print.
- **BVI-VFI / LPIPS ≈ 0.597 / PLCC < 0.6** (§8-C) is from the task brief, **(brief — to verify)** against
  the primary BVI-VFI source; it is NOT a first-hand-confirmed external reference and MUST carry a
  verification level before any doc states it as `[V1]`.
- **The mandelbrot SPIR-V load primitive + `run_routed`** (`framework/holons`, framework/gpu
  `dispatch()`/`VulkanCompute`) are an *alternative* compute-load path noted in the inventory but NOT
  used by TB-C3's design (which is graphics+present per TB-2); their line numbers are not re-verified here.

---

## §12 — TB-C12 testbench_minigame — build order (Phases 1–7) + edit sites + gates

**Goal.** Build the input-connected minigame (MASTER_PLAN §2 TB-C12): a NEW sibling app
`framework/render/vulkan/bench/testbench_minigame/` composing three reusable headers
(`latency_tap.hpp` / `gfx_present_load.hpp` / `interactive_scene.hpp`) over the **reused TB-C1 scene**
(`testsource.comp`), measuring **input→photon** with an EXTERNAL `fg_latency_scorer.py`. It inherits the §0
discipline VERBATIM (default-off byte-identical-off, verify-the-site, soft-fail, cite the risk ID) and the
operator's binding decisions: **ZERO `render_assistant` edit** (the recommended posture — only `parse_extra`
default-off if ever truly unavoidable, and not planned), v1 saturation = `--load-count` + `--res-scale` +
`--burn` only (overdraw/tessellation deferred to v1.1), and the namespace stays **TB-C** (the risks are
**MG-1..MG-9**, RISK_REGISTER). Each phase below names the **edit sites** (verified first-hand where a line
is cited; fine line-level detail MAY be deferred to build) and the **compile / byte-identical GATE** it
must pass before the next phase. Per the Tier-2 gate, **no commit while any MG risk is `open`** — and
**MG-2 is liftable ONLY by the operator's first-hand rig A/B** (Phase 6), which a monitor-free supervisor
cannot satisfy.

### Phase 1 — scaffold the NEW sibling app (the byte-identical baseline)
- **Edit sites.** New dir `framework/render/vulkan/bench/testbench_minigame/` with `main.cpp`,
  `CMakeLists.txt`, `shaders/`. Lift the window / swapchain / present-loop scaffold + the `make_pc`
  autopilot + the `--gt-emit` path **verbatim** from `testbench_source/main.cpp` (the raw-WSI `struct Swap`
  at `:277`, `make_pc` at `:176`, the 7 presets enum at `:91`, the `PushC` struct `:165` with its 64-byte
  `static_assert` `:171`, the `--gt-emit` write at `:323-344`). The scene stays the reused
  `testsource.comp` (compute→blit). **Switch the present mode** from TB-C1's IMMEDIATE-preferring default
  (`testbench_source/main.cpp:297-300`) to a **real FIFO swapchain that lets the render-ahead queue build**
  (MASTER_PLAN §2 TB-C12; the deliberate inverse of TB-C3's decouple). Add the new target to
  `INVENTORY.md` in the same commit (§9 inventory rule).
- **GATE.** Builds green; the FG captures it (`render_assistant --window PhyriadFG-Mini…`, the existing
  by-title path, `main.cpp:1263`); a **no-flag `--gt-emit` RGBA byte-diff = 0 vs TB-C1's `--gt-emit`** on
  the same seed/preset (the byte-identical baseline established). → **MG-6.**

### Phase 2 — `interactive_scene.hpp` (the bimodal sim, GT preserved)
- **Edit sites.** NEW header `testbench_minigame/interactive_scene.hpp`: a fixed-timestep accumulator sim
  whose SOLE nondeterminism is a per-tick `usercmd` stream sourced from {live | recorded file | the TB-C1
  `make_pc` autopilot}. NO wall-clock (`now_ms`) in integration (render/telemetry only); seed value-noise
  with `--seed 7` (the TB-C1 determinism contract, `prep_zoo_sequence.py` `rng(7)`); bind each `usercmd` to
  a sim-tick INDEX (not wall-clock); force the load OPEN-LOOP under replay. Keep `--gt-emit` on the
  UNCHANGED closed-form `make_pc` path so it never reads the live sim. Emit a per-tick **state-hash**.
- **GATE.** A REPLAY of a recorded `usercmd` reproduces the per-tick state-hash EXACTLY (desync self-check
  green) before any GT is trusted; the autopilot path is still byte-identical to Phase 1. → **MG-3.**

### Phase 3 — `gfx_present_load.hpp` (the graphics+present saturator; the Tier-2 driver)
- **Edit sites.** NEW header `testbench_minigame/gfx_present_load.hpp`. **Factor** TB-C3's TDR watchdog +
  NVML reader + EMA/deadband controller out of `testbench_saturation/main.cpp` (`kTdrSoftCapMs=250` `:87`,
  `kHardWaitNs=1.8 s` `:88`, `kKp=0.12` `:95`, `kUtilEmaAlpha=0.15` `:96`, `kCtrlDeadband=3.0` `:97`,
  `kFramesInFlight=4` `:99`, `--ramp-ms` `:134`, the NVML dynamic-load `:178`/`:181`) into the shared header
  so **both apps consume ONE copy** (DRY — refactor TB-C3 to consume it IF the operator approves touching a
  green Tier-2 app; else duplicate-and-accept-drift, the OPEN DECISION). Add the NEW instanced-draw raster
  load pass (`--load-count`, CPU-issued `vkCmdDrawIndexed`, the `scene.vert`/`scene.frag` pair) + the
  `--res-scale` offscreen pre-pass + the existing per-pixel `--burn` (`testbench_source/main.cpp:169`) on
  the SAME graphics queue that presents the FIFO swapchain. Wire the **frame-time ms** primary setpoint
  (NVML util secondary). The load raster pipeline is a SEPARATE object, SKIPPED entirely when
  `--load-count 0` / `--res-scale 1.0`.
- **GATE.** A ≥5 min ramped run holds the frame-time setpoint with **no TDR** (watchdog clean); an
  intentional over-budget submit recovers cleanly; `--load-count 0` / `--res-scale 1.0` / `--burn 0` is
  **byte-identical** to Phase 2. → **MG-1, MG-8** (and MG-2 is reproduced-not-proven until Phase 6).

### Phase 4 — `latency_tap.hpp` (the input tap + the marker)
- **Edit sites.** NEW header `testbench_minigame/latency_tap.hpp`: `RegisterRawInputDevices` + `WM_INPUT`,
  QPC-stamp `T_input` AT the raw-input callback (the earliest tap), bind the `usercmd` to a sim-tick index
  sampled just-in-time before the sim step. The marker: on the input-CONSUMING tick draw the HARD binary
  dark→white SOLID BLOCK (≥6 % luminance step, NEVER a fade) inside the presented surface, and **encode the
  originating tick-id into TB-C1's EXISTING 16-bit gray-code `cameraBand`** (`testsource.comp:140`, bits
  `:139`/`:153-156`) via `pc.frame_id` + a free `pc.flags` bit — NO `PushC` growth (the 64-byte
  `static_assert` `:171` is the tripwire). Give FG-interpolated frames a DISTINCT id class. The flash +
  ID-into-band is a flag-gated `~40`-GLSL-line addition to `testsource.comp` behind a free `pc.flags` bit
  (the existing `pc.flags&1u → applyFrameId` pattern at `testsource.comp:217` is the model). Log the SPSC
  lock-free I2FS/FS2P/P2D CSV via the `telemetry_csv.hpp` ring discipline (`CsvRow` `:33`, lock-free `push`
  `:83`) — never a mutex on the present hot path.
- **GATE.** Flash OFF / `--latency-csv` OFF is **byte-identical** to Phase 3; the CSV separates the
  engine-input stage (sample→simstart) from input→present as a SEPARATE column. → **MG-4, MG-5, MG-6.**

### Phase 5 — `fg_latency_scorer.py` (the EXTERNAL scorer)
- **Edit sites.** NEW `scripts/fg_latency_scorer.py` (alongside the TB-C6 `fg_testbench_camera_analysis.py`).
  Read the gray-code id off each photon over the FG OUTPUT window (reuse the red-fiducial band-rectify +
  gray-code decode of `fg_testbench_camera_analysis.py` `:50`/`:91-98`), look up `T_input_sample`, compute
  input→photon for THAT input, **reject interpolated photons** (non-advancing gray id + sub-threshold
  flash — the PresentMon drop-attribution analogue), and emit the A/B (minigame-window vs FG-output-window)
  delta as **DISTRIBUTIONS** (median + spread), never single numbers. ALL latency logic lives in TB-C12 +
  this scorer — **zero FG edit.**
- **GATE.** An A/A null (the same window twice) reads ~0 delta; interpolated photons are correctly
  rejected on a synthetic fixture (a `--self-test`, the `fg_testbench_camera_analysis.py --self-test`
  precedent). → **MG-9** (proxy-as-truth flagged), **MG-4.**

### Phase 6 — VERIFY the rig (operator-run; the supervisor compile-verifies only)
- **The MG-2 hard gate.** Launch the minigame, capture with `render_assistant --window`, dial the
  graphics+present load, and run the **TB-2/MG-2 A/B**: the graphics+present load reproduces the BF6
  signature — `fg_multiplier` collapse toward ~1.0 (`-stats.csv`) **+** `lt_spin_us` rise
  (`--latency-trace`, `main.cpp:1193`) — while a **matched compute-only `--burn` does NOT**. Then run the
  input→photon A/B (minigame window vs FG output window) under saturation to quantify the felt ~0.5 s as a
  distribution against the S24 Ultra slow-mo. This is the ONLY thing that lifts **MG-2** `open→mitigated`;
  a monitor-free supervisor CANNOT produce it. Verify the WGC PresentMode is Composed Flip (not Hardware
  Independent Flip) before the run — the optional `--force-composed-flip` mitigation (MG-7).
- **GATE.** The collapse signature reproduces first-hand under graphics+present and NOT under compute-only
  `--burn`. → **MG-2** (the lift), **MG-7** (capture-path confirmed).

### Phase 7 — close-out (the Tier-2 commit gate)
- **ONLY if Phase 6 reproduced the signature**, move **MG-2** `open→mitigated` in the RISK_REGISTER;
  confirm every other MG risk is `mitigated`/`accepted`; THEN commit (Tier-2: **no commit while any MG risk
  is `open`**), updating `INVENTORY.md` + the doc-sync in the same commit. Lint clean: `lint_hal` (no raw
  `memory_order_*` in `apps/` — the headers live under `framework/`, but any `apps/` reuse follows the same
  rule), `lint_docs`, `check_inventory`.

**Gate map (TB-C12).**

| Phase | New artifact | Touches `main.cpp`? | Risk IDs | Gate |
|---|---|---|---|---|
| 1 | sibling app scaffold + CMake + FIFO present | No | MG-6 | build green; FG captures; no-flag `--gt-emit` byte-diff = 0 vs TB-C1 |
| 2 | `interactive_scene.hpp` | No | MG-3 | replay state-hash exact; autopilot byte-identical |
| 3 | `gfx_present_load.hpp` (+ shared factor) | No | MG-1, MG-8 | ≥5 min holds setpoint, no TDR; over-budget recovers; `count=0/res=1` byte-identical |
| 4 | `latency_tap.hpp` + flag-gated shader add | No | MG-4, MG-5, MG-6 | flash-off byte-identical; CSV isolates engine-input |
| 5 | `fg_latency_scorer.py` | No | MG-9, MG-4 | A/A null ~0; interpolated rejected (`--self-test`) |
| 6 | rig A/B (operator) | No | **MG-2**, MG-7 | graphics+present reproduces collapse; compute-only does NOT |
| 7 | commit | No | all MG terminal | no `open` MG risk; INVENTORY + doc-sync same commit |

---

## §11 — Provenance + registration

Built on the first-hand inventory of existing FG telemetry / quality / source / load assets (the
`wgu3rhp0o` scoping workflow) and the three sibling Tier-2 documents (MASTER_PLAN + RISK_REGISTER,
authored in parallel). Every in-repo `path:line` re-verified first-hand this session except the §10
gaps. The supervisor registers this triad in
[`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) and reflects the position in the
live [`ACTION_PLAN.md`](../ACTION_PLAN.md) — not touched here, to avoid a shared-file collision with the
parallel siblings.
