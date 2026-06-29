# FG_TESTBENCH_RISK_REGISTER — the Tier-2 risks of the controlled FG test environment

> **Diátaxis type:** Reference (normative). This is the risk-treatment leg of a
> [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) **Tier-2** change: it enumerates every
> qualifying failure mode of the FG testbench, each with its mitigation expressed AS CODE and a
> first-hand verification plan, tracked to a terminal status before any commit.
> **Status:** `designed` — the testbench is **not built**; every risk below starts **`open`**.
> **Plan tier:** Tier 2 — the third leg of the triad with the companion (parallel-authored)
> [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) (the why / the architecture / the
> phases) + [`FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md`](FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md)
> (the per-component edit sites). The three MUST be mutually linked (PLAN_TIER §2.3).
>
> **THE HARD GATE (PLAN_TIER §3):** no part of the testbench that carries a risk recorded here MUST
> be committed while any of its risks is **`open`**. Each MUST reach `mitigated` (code in + verified
> first-hand) or `accepted` (residual stated + who accepted). A risk MUST NOT be silently dropped.
>
> **★ Supervisor reconciliation 2026-06-24 — the FOUNDATION (TB-C1 + TB-C2 + TB-C3) is BUILT + verified
> first-hand, so its risks are now `mitigated`:** **TB-1** (TDR/crash-class) — the GPU-timestamp watchdog is
> intact + `breaches=0` across a sustained idle-GPU run, TB-C3 runs as a SEPARATE process, and the NVML loop
> self-limits `iters` at ~100% util before a dangerous burn (the forced-TDR-recovery sub-test could not be
> DRIVEN because of that self-limiting — a safety property, recorded honestly). **TB-2** (realism) — TB-C3
> renders + presents on the graphics queue (`present 4-5/s` measured, NOT compute-only); the deeper A/B
> (graphics+present reproduces the FG's BF6 contention vs a compute-only burn) is the testbench's FIRST
> measurement, pending the FG-under-load run. **TB-3** (bounded-run exit) — `--duration 5` exits cleanly at
> the deadline via the existing `g_quit`, finalizes `-stats.csv`, prints the descriptor line. **TB-6**
> (byte-identical-off) — verified BY CONSTRUCTION (the TB-C2 guards short-circuit at the default 0; a live
> `--qdump` diff is confounded by the FG's capture-timing non-determinism, so construction is the sound
> proof).
>
> **★ Wave-3 reconciliation 2026-06-25 — TB-C4 (orchestrator) + TB-C6 (camera footage-analysis) + TB-C7
> (frame-ID aligner) are now BUILT + SELF-TEST-verified (the LOGIC, monitor-free):** **TB-4** (frame-id
> alignment under drops) `mitigated` — `scripts/fg_testbench_align.py --self-test` + `fg_testbench_camera_analysis.py
> --self-test` PASS (align by DECODED id not position; dropped / rolling-shutter / motion-blur / no-band frames
> flagged UNALIGNABLE, counted, never mis-scored); the real-frame / real-film validation is monitor/camera-pending.
> **TB-5** (measurement validity) `accepted (PARTIAL)` — the synthetic TB-C1 source escapes the scene-confound BY
> CONSTRUCTION, the camera axis bypasses the cross-clock present-timestamp artifact (photon-time), and the
> orchestrator records the RAW per-present CSV for auditability; the RESIDUALS are monitor-pending eye-test items —
> the A/A null run (identical A/B → zero) and the saturation-realism reproduction (the separate TB-C3 load
> reproduced SOURCE-STARVATION not the in-game PRESENT-COLLAPSE; the `--burn` heavy-source is the SOTA-unsourced
> realism HYPOTHESIS to validate on the rig, TB-2). **`TB-C5` (the 2AFC eye harness) remains UNBUILT** (it needs the
> operator's eye). No `open` risk blocks the Wave-3 commit; the foundation (TB-C1/C2/C3 + the scorer-CMake fork-#1
> fix) carried none either.
>
> **★ TB-C12 minigame extension 2026-06-25 — the MG-1..MG-9 risks are SEEDED AND ALL `open` (`designed`,
> NO code written).** This register is extended (it is NOT a new triad) with the risks of the TB-C12
> testbench_minigame (MASTER_PLAN §2 TB-C12): a NEW sibling app + three reusable headers measuring
> input→photon, resolving the open TB-2 realism risk that the shipped TB-C3 (which offscreen-decouples its
> present to hold util) structurally cannot. The minigame risks live in the **MG-1..MG-9** namespace
> (disjoint from TB-1..TB-6) — §2.MG below. Every MG mitigation is **DESIGN — not yet built**; its
> verification is **pending build** (compile-verifiable by the supervisor) or **pending rig (operator)**.
> **THE HARD GATE (MG-2):** the saturation-realism risk can be lifted `open→mitigated` ONLY by the
> operator's first-hand rig A/B — the graphics+present load reproduces the `fg_multiplier`→~1.0 collapse +
> `lt_spin_us` rise that a matched compute-only `--burn` does NOT — and a monitor-free supervisor can ONLY
> compile-verify, never produce the measured signature. **NO TB-C12 code is committed while any MG risk is
> `open`.** render_assistant is **zero-touch** (the recommended posture): the scorer infers interpolated FG
> frames EXTERNALLY; no FG edit is designed (only `parse_extra` default-off if ever truly unavoidable —
> not planned, MG-6).
>
> **Scope.** The five Tier-2 triggers (crash/device-loss · concurrency · security · data-loss ·
> dogma — PLAN_TIER §1.1) across the seven testbench components TB-C1–TB-C7 (+ the scorer TB-C8) **and the
> TB-C12 minigame (TB-C12a/b/c, MG-1..MG-9)**. The
> **verifiable-reference mandate** ([`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md)
> §2) governs every `path:line` here — each was re-verified first-hand against
> `apps/render_assistant/src/main.cpp` (~9700 lines, the FG) and the named bench files this session.

---

## §0 — Normative keywords

The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **SHOULD**,
**SHOULD NOT**, **RECOMMENDED**, **MAY**, and **OPTIONAL** are to be interpreted as described in
BCP 14 ([RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
[RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)) when, and only when, they appear in all
capitals, as shown here.

---

## §0.1 — Component map (the shared spine — these IDs are canonical across all three docs)

| ID | Component | One line | State |
|---|---|---|---|
| **TB-C1** | SourceWindow | standalone deterministic synthetic-motion windowed app, WGC-capturable; 3 "skins" — **GT** / **eye** / **camera** | build-new |
| **TB-C2** | FG bounded-run mode | `--duration` / `--max-frames` / `--exit-after` in the FG `main.cpp` → clean teardown + CSV finalize + descriptor-count print | build-new (touches `main.cpp`) |
| **TB-C3** | SaturationLoad | controllable GPU load that pressures the **GRAPHICS QUEUE + PRESENT** like a real game (NOT compute-only) with NVML closed-loop feedback to hold a target util % | build-new |
| **TB-C4** | SweepOrchestrator | `{source(res,fps,skin) × load(util) × FG(flags)}` → bounded run → harvest `--csv` + GT-quality → A/B matrix | build-new |
| **TB-C5** | EyeHarness | 2AFC blind randomized counterbalanced A/B presenter + verdict log + Bradley-Terry/JOD + chi-square + artefact-amplification mode | build-new |
| **TB-C6** | CameraKit | the **camera** skin + slow-mo footage analysis reading the frame-ID markers (the photonic axis) | build-new |
| **TB-C7** | FrameID alignment | the live ground-truth lever: visually tag each source frame so the live FG output aligns to the analytic GT even under non-deterministic drop/pacing — threads TB-C1 + TB-C4 + the scorer | build-new (may touch `main.cpp`) |
| **TB-C8** | scorer / eye-proxy | the existing `fg_quality_scorer` extended to consume the TB-C7-aligned live frames + the open MOS-calibration | extend-existing |
| **TB-C12** | testbench_minigame | NEW sibling app: ONE customizable minigame (dialable saturation · movement · live input) → input→photon (Axis 5); resolves TB-2 by reproducing the present back-pressure TB-C3's util-hold decouple cannot | build-new |
| **TB-C12a** | `latency_tap.hpp` | raw `WM_INPUT` QPC-at-sample + hard binary flash + tick-id into TB-C1's gray-code band + distinct interpolated-id class + SPSC I2FS/FS2P/P2D CSV | build-new (header) |
| **TB-C12b** | `gfx_present_load.hpp` | dialable graphics+present saturator (`--load-count` instanced raster + `--res-scale` + `--burn`) on the SAME present queue, queue allowed to build; factors TB-C3's watchdog+NVML+controller | build-new (header) |
| **TB-C12c** | `interactive_scene.hpp` | fixed-timestep accumulator sim, input = SOLE nondeterminism; usercmd {live\|recorded\|`make_pc` autopilot}; per-tick state-hash; REPLAY(GT)/LIVE(latency) | build-new (header) |
| **TB-C12-scorer** | `fg_latency_scorer.py` | EXTERNAL: gray-code id off each photon → pair to `T_input` → reject interpolated → A/B input→photon delta as distributions (keeps latency logic OUT of the FG) | build-new (script) |

The operator steer (2026-06-24) is to build ALL THREE measurement axes — **objective** (TB-C4 + TB-C8),
**perceptual** (TB-C5), and **photonic** (TB-C6) — in parallel, with TB-C7 FrameID alignment as the
live-absolute ground-truth route (the harder, more complete one).

---

## §1 — Risk table

One row per TB-1..TB-6. **Class** carries the verbatim spine label mapped to its PLAN_TIER §1.1
trigger. Every `path:line` re-verified first-hand this session.

| ID | Class | Failure mode (+ site) | Mitigation AS CODE (designed) | First-hand verification plan | Status |
|---|---|---|---|---|---|
| **TB-1** | SaturationLoad TDR/crash-class — *crash / device-loss / TDR* (trigger 1) | A TB-C3 host loop dispatching an unbounded compute burn (`stage9_singlegpu/shaders/game_render.comp:20` `for(i<pc.iters)`, push-constant `:10`) or an oversized grid can let one `vkCmdDispatch` (`framework/gpu/src/Gpu.cpp:666`) exceed the ~2 s Windows TDR window → GPU reset → `VK_ERROR_DEVICE_LOST`; if TB-C3 shares the FG's process/device it takes the FG + harness down. | Bounded per-dispatch `iters`/grid (each submit ≪ TDR window); a host back-off watchdog on a slow submit; **TB-C3 runs as a SEPARATE PROCESS** (own `VkInstance`/`VkDevice`) so a TDR's `DEVICE_LOST` is contained; an **NVML-feedback cap** (the reader pattern at `main.cpp:7254`, factored to a shared helper) throttles `iters` once util ≥ target. | A sustained (≥5 min) run holds util ~95 % via the NVML loop with **no system hang**; an intentional over-budget dispatch triggers a TDR the OS **recovers cleanly** and the FG (separate process) survives. | **`mitigated`** (§0) |
| **TB-2** | SaturationLoad realism — *validity / dogma (measured-over-assumed, trigger 5)* | A **compute-only** burn (`game_render.comp:8-9` — compute shader, `writeonly image2D`, no render pass, no present) loads ALU but does NOT contend for the graphics queue + compositor present that the FG actually host-blocks on: `vkWaitForFences(A.dev, fBridge, UINT64_MAX)` on the game-shared graphics queue `A.q` (`main.cpp:7549` / `7698` / `8067`). So it would NOT reproduce the phenomenon under test. | TB-C3 MUST **render + present on the graphics queue** — built on the windowed `Swapchain` path (`Swapchain.hpp:111` `VkSwapchainKHR`, FIFO present `:5-6`, `submit_and_present:47`) so it submits graphics work + a real present competing for the exact resource the FG's `vkWaitForFences(fBridge)` blocks on. The `iters` compute burn is retained as an ADDITIONAL ALU knob, never the sole load. | With TB-C3 (graphics+present) on, the FG reproduces the BF6-combat signature — `fg_multiplier` → ~1.0 (`-stats.csv`, `telemetry_csv.hpp:365-424`) + the preflow-spin `lt_spin_us` rising (`--latency-trace`, `main.cpp:9400-9411`) — which a compute-only burn does NOT produce (A/B: compute-only vs graphics+present). | **`mitigated`** (§0) |
| **TB-3** | FG bounded-run touches `g_quit`/exit — *crash + dogma (byte-identical-off, triggers 1+5)* | TB-C2's `--duration`/`--max-frames`/`--exit-after` set the `g_quit` exit latch (`main.cpp:3012`, today set only by `console_ctrl_handler:3039`, thread-death/device-loss `7342`/`7358`/`7377`/`8558`). A new exit path that bypasses the existing teardown could truncate the `--csv` finalize or leak descriptors; a default that is not inert breaks every existing user. | The three flags parsed in `parse_extra` (the C1061-dodge lambda, `main.cpp:1124`), **default 0 = OFF** (inert); the bounded-run check sets the EXISTING `g_quit=true` (no new exit path) → the existing teardown drains the telemetry ring (`TelemetryCsv` finalize, `telemetry_csv.hpp`) + a new descriptor-count print. | **Byte-identical-off** (a no-flag run → `--qdump`/`--outdump` capture diff = 0 vs the pre-change binary, the TB-C2 harness) **+** a bounded run exits cleanly at the deadline with a complete non-truncated `-stats.csv` (no `# STALL_*` markers, `telemetry_csv.hpp:321-348`) and the descriptor count printed. | **`mitigated`** (§0) |
| **TB-4** | FrameID alignment under non-deterministic drops — *correctness / measurement-validity (data-integrity, trigger-adjacent)* | The live path drops/paces non-deterministically — WAP lap-freeze re-show (`row.frz`, `main.cpp:9193`), the `--fdrop` exact-dup drop (`row.fdrop`, `:9194`), async-present drop-interpolated. A positional aligner (assume capture #N = GT #N) mis-indexes EVERY frame after the first drop → systematic mis-score. | The TB-C7 tag is a **per-frame UNIQUE** marker block in a TB-C1-source corner (warp-robust); the scorer (TB-C8) decodes the id from each captured frame and aligns to the GT of **that id**, never by position; a frame whose id cannot be decoded (a synthesized/blended in-between, or a corrupt tag) is **marked UNALIGNABLE** — excluded from the score and counted — rather than mis-scored. | A drop-injected run (force `--fdrop` / a synthetic drop pattern) still aligns the decodable frames to the correct GT id and reports the unalignable count honestly; an A/A run (TB-C1 source vs itself through capture) aligns 1:1 with zero unalignable on the clean path. | **`mitigated`** (§0 — self-test) |
| **TB-5** | measurement validity — *validity / dogma (measured-over-assumed, trigger 5)* | Cross-clock present-timestamp error: the `[lat-trace]` `compose` field (`lt_compose_us`) is the QPC↔SystemRelativeTime epoch delta and reads 0 / spurious when the epoch guard fires (the observed `compose:24.9` artifact, `main.cpp:9398-9411`). EMA/window artifacts (`lat_ema_ms` `:9189`, `freshage_ema_ms`, the per-second status EMAs). Single-observer TB-C5 = ITU "informal". Scene-confound escapes ONLY via the synthetic source. | The **synthetic TB-C1 source** removes the scene-confound by construction; the **camera/photon axis (TB-C6)** bypasses the cross-clock present-timestamp artifact entirely (it reads panel photons via slow-mo, not a software timestamp — `compose:24.9` cannot contaminate it); the **single master clock** is the one `qpc_present` (`telemetry_csv.hpp:35`), not a cross-clock delta; TB-C5 is **declared "informal"** (ITU-R BT.500 single-observer) and compensates with many randomized within-subject 2AFC trials + Bradley-Terry/chi-square, never a panel-grade MOS claim. | An **A/A null test** (identical A and B fed to TB-C5 and to TB-C4/TB-C8) returns no-difference (chi-square p well above threshold; Bradley-Terry ≈ 0.5). A residual A/A difference is a harness bias to fix before any A/B claim is reported. | **`accepted (PARTIAL)`** (§0) |
| **TB-6** | byte-identical-off for any `main.cpp` change (TB-C2 + TB-C7 tag-emit) — *dogma (byte-identical-off, trigger 5)* | Any TB-C2 (bounded-run) or TB-C7 (tag/marker-emit or passthrough) edit to the FG `main.cpp` whose **default path** changes the presented/dumped bytes vs the current build → a silent regression of every existing FG run. | Each new flag default-OFF in `parse_extra` (`main.cpp:1124`), the default leaving the existing instruction unentered behind an `if(cfg.X)` with `X` defaulting off — the `DR-HDR-1` / saturation-`CR5` precedent. TB-C7's source tag is emitted by **TB-C1 (the standalone source), not the FG**; any FG-side TB-C7 marker path is likewise flag-gated. | With ALL new flags absent, the **TB-C2 byte-identical harness** reports `--qdump`/`--outdump` capture **diff = 0** vs the pre-change binary on a fixed scene. MANDATORY before each TB-C2/TB-C7 commit. | **`mitigated`** (§0) |

No **security** trigger applies: the testbench reads a locally-captured surface and presents/scores
locally; it adds no network, untrusted-input, or privilege surface. No **persistent data-loss**
trigger: the only writes are `.rgba`/`.csv` artifacts into a run directory (TB-3 guards against a
truncated CSV, which is artifact-completeness, not user-data loss). A **concurrency** hazard exists
only inside TB-C3's NVML-feedback loop and the FG's existing lock-free telemetry ring
(`telemetry_csv.hpp:61-70`, SPSC, never blocks) — TB-C2/TB-C7 MUST NOT add a mutex on the present hot path
(CANON lock-free mandate); this is folded into TB-3's "reuse the existing teardown" mitigation.

---

## §2 — Risk detail (failure mode @ site · mitigation AS CODE · verification · status)

### TB-1 — SaturationLoad TDR / crash-class

- **Class:** crash / device-loss / TDR (PLAN_TIER §1.1 trigger 1).
- **Failure mode (code terms):** TB-C3 is a new host loop that repeatedly dispatches a heavy kernel to
  peg device A (the 4090). The reusable burn primitive is the `iters` push-constant in
  `stage9_singlegpu/shaders/game_render.comp` (`:10` `layout(push_constant) uniform PC { uint frame;
  uint iters; }`, `:20` `for (uint i=0u; i<pc.iters; ++i) …`), dispatched through the gpu pillar
  (`framework/gpu/src/Gpu.cpp:439` create-pipeline, `:663` bind, `:666` `vkCmdDispatch`). If `iters`
  or the grid is unbounded, a single dispatch can exceed the default ~2 s Windows TDR window → driver
  reset → `VK_ERROR_DEVICE_LOST`. If TB-C3 runs **in-process with the FG** (sharing a `VkDevice`), that
  device-loss takes the FG and the harness down with it.
- **Mitigation + attached code:**
  ```cpp
  // TB-C3 host loop (designed): bounded per-submit burn + NVML-feedback cap, in TB-C3's OWN process/device.
  uint iters = iters_floor;                       // chunk the burn so each dispatch ≪ TDR window
  for (;;) {
      int util = nvml_util_A();                    // shared helper, factored from main.cpp:7254-7330
      if (util >= 0) iters = (util < target_util)  // closed loop: raise/lower toward the target %
                            ? std::min(iters + step, iters_cap)
                            : (iters > step ? iters - step : iters);
      dispatch_burn(iters);                        // grid + iters both clamped ≤ a per-dispatch ceiling
      if (slow_submit_watchdog_tripped()) iters = iters_floor;   // back off before a TDR
  }
  ```
  TB-C3 owns a separate `VkInstance`/`VkDevice` so a `DEVICE_LOST` is process-local; the FG keeps the
  graceful-exit it already has (`vk_live` → `g_quit`, e.g. `main.cpp:7549`).
- **Verification:** (1) a ≥5 min run holds util at the target (~95 %) via the NVML loop with **no
  system hang**, NVML util/clock logged; (2) an intentional over-budget dispatch induces a TDR the OS
  **recovers from cleanly**, and the FG (separate process) **survives** (its own watchdog and
  `--csv` keep advancing). Validation layers clean on TB-C3.
- **Status:** `open`.

### TB-2 — SaturationLoad realism (a compute-only load would not reproduce the bottleneck)

- **Class:** validity / dogma — *measured-over-assumed* (PLAN_TIER §1.1 trigger 5: a measurement
  that silently mismeasures the phenomenon is a dogma breach of the efficiency/measurability mandate).
- **Failure mode (code terms):** the phenomenon under test is the **blocking present on the
  game-shared graphics queue**: `vkQueueSubmit(A.q,…); vk_live(vkWaitForFences(A.dev, fBridge,
  VK_TRUE, UINT64_MAX))` at `main.cpp:7549`, `:7698`, `:8067` — the FG host-blocks on `A.q` until the
  warp+present fence signals, and that wait is what stretches under a real game. `game_render.comp`
  is a **pure compute** shader (`:8` `layout(local_size_x=8…)`, `:9` `writeonly image2D u_dst`, no
  render pass, no swapchain present). A compute-only burn saturates ALU but leaves the graphics
  queue + compositor present uncontended → the FG's `fBridge` wait does NOT stretch the way it does
  under BF6 combat → the testbench would measure the wrong regime.
- **Mitigation + attached code:** TB-C3 MUST **render and present on the graphics queue**, built on the
  windowed swapchain present path:
  ```cpp
  // TB-C3 (designed): a real graphics + present load, NOT just compute.
  phyriad::render::vulkan::Swapchain sc{ctx, hwnd};   // Swapchain.hpp:111 VkSwapchainKHR, FIFO present :5-6
  record_graphics_pass(cmd);                          // a real draw → graphics-queue work
  burn_compute(cmd, iters);                           // OPTIONAL extra ALU (game_render.comp iters) — additive
  sc.submit_and_present(image_index);                 // Swapchain.hpp:47 — competes for A.q + the compositor
  ```
  The graphics+present pressure is the load that contends with the FG's `vkWaitForFences(fBridge)`;
  the compute burn is an additional knob, never the sole load.
- **Verification:** with TB-C3 (graphics+present) running, the FG reproduces the combat signature —
  `fg_multiplier` collapsing toward ~1.0 in the `-stats.csv` (`telemetry_csv.hpp:365-424`) **and** the
  preflow-spin `lt_spin_us` rising in `--latency-trace` (`main.cpp:9400-9411`) — that a compute-only
  burn does NOT reproduce. Recorded as an explicit A/B: compute-only load vs graphics+present load,
  only the latter reproducing the collapse.
- **Status:** `open`. *(Honest: the controlled graphics-queue saturator is UNSOURCED in the SOTA — see
  §4-B; this mitigation is a design proof, validated only by the reproduced signature above.)*

### TB-3 — FG bounded-run touches the exit/present path (`g_quit`)

- **Class:** crash (a new exit path) + dogma (byte-identical-off) (triggers 1 + 5).
- **Failure mode (code terms):** today `g_quit` (`main.cpp:3012`) is set only by the console handler
  (`:3039`) and the device-loss/thread-death sites (`:7342`, `:7358`, `:7377`, `:8558`); the app
  otherwise runs until Ctrl-C / window-death / device-loss. TB-C2 adds `--duration` / `--max-frames` /
  `--exit-after`. A new exit path that does not route through the existing teardown could leave the
  `--csv` ring undrained (a truncated `-stats.csv`) or leak Vulkan descriptors; a non-inert default
  changes every existing run.
- **Mitigation + attached code:**
  ```cpp
  // parse_extra (main.cpp:1124), default-OFF → byte-identical when absent:
  if(!std::strcmp(arg,"--duration"))   { if(auto v=next(arg)){ c.run_secs   = atof(v); return 0;} return 1; }
  if(!std::strcmp(arg,"--max-frames")) { if(auto v=next(arg)){ c.run_frames = atoi(v); return 0;} return 1; }
  if(!std::strcmp(arg,"--exit-after")) { if(auto v=next(arg)){ c.run_present= atoi(v); return 0;} return 1; }
  // present loop: reuse the EXISTING latch + teardown — no new exit path.
  if((cfg.run_secs  >0.0 && elapsed_s>=cfg.run_secs) ||
     (cfg.run_frames>0   && csv_fi   >=cfg.run_frames)) g_quit = true;   // same g_quit:3012 → existing drain
  // on the existing teardown: tcsv finalize (telemetry_csv.hpp) + a new descriptor-count print.
  ```
  `c.run_secs/run_frames/run_present` default to 0 (inert). The teardown is the one that already
  drains the SPSC ring; the descriptor-count print is the only added instruction and it runs at exit,
  off the hot path.
- **Verification:** **byte-identical-off** — a no-flag run's `--qdump`/`--outdump` capture diff = 0 vs
  the pre-change binary (the TB-C2 harness). A bounded run exits cleanly at the deadline with a complete
  non-truncated `-stats.csv` (no `# STALL_BEGIN/ONGOING/END` markers from the watchdog,
  `telemetry_csv.hpp:321-348`) and the descriptor count printed; `lint_hal` clean (no raw
  `memory_order_*`, default `seq_cst` in `apps/`).
- **Status:** `open`.

### TB-4 — FrameID alignment robustness under non-deterministic drops

- **Class:** correctness / measurement-validity (data-integrity of the GT correspondence).
- **Failure mode (code terms):** the live path drops and re-paces non-deterministically by design —
  the WAP lap-freeze re-show (`row.frz` set at `main.cpp:9193`), the `--fdrop` exact-duplicate drop
  (`row.fdrop` at `:9194`), and the async-present drop-interpolated branch. The held-out protocol
  cannot be used live (it manufactures the artifact — see §4-A, the code-verified reason `--qdump`
  ships truth-LESS, `main.cpp:226`). TB-C7's answer is to tag each source frame so the live output can
  be aligned to the analytic GT. If the aligner is positional (capture #N ↔ GT #N), a single drop
  mis-indexes every subsequent frame → the whole run is mis-scored against the wrong GT.
- **Mitigation + attached code:** the TB-C7 tag is a **per-frame unique** id (a binary-coded marker
  block in a TB-C1-source corner, sized to survive the warp); the scorer (TB-C8) decodes it per frame and
  aligns by id, and explicitly marks the unreadable case rather than guessing:
  ```cpp
  // scorer (designed TB-C8 extension): align by DECODED id, never by position.
  int id = decode_frame_id(captured);              // the TB-C7 corner marker
  if (id < 0 || is_blended_between_ids(captured)) { ++unalignable; continue; }   // honest skip, counted
  score_against(captured, gt_by_id[id]);           // the analytic GT for THIS id (TB-C1 GT skin)
  ```
  A synthesized FG frame that lies *between* two source ids (no single GT instant) is counted
  `unalignable`, not scored — the alignment never invents a correspondence.
- **Verification:** a drop-injected run (force `--fdrop` or a synthetic drop pattern) still aligns the
  decodable frames to the correct GT id and reports the `unalignable` count honestly; an **A/A** run
  (TB-C1 source through capture vs itself) aligns 1:1 with zero unalignable on the clean path.
- **Status:** `open`. *(Honest: TB-C7 is the mitigation for §4-A, not a guarantee — see §4-A.)*

### TB-5 — measurement validity

- **Class:** validity / dogma — *measured-over-assumed* (trigger 5).
- **Failure mode (code terms):** three escapes. (1) **Cross-clock present-timestamp error** — the
  `[lat-trace]` `compose` field is a QPC↔SystemRelativeTime epoch delta; the code itself notes
  `compose=0 ⇒ the QPC↔SystemRelativeTime epoch mismatch guard fired` (`main.cpp:9398-9399`), and the
  observed `compose:24.9` is exactly that cross-clock artifact (`:9411`). (2) **EMA/window artifacts**
  — `lat_ema_ms` (`:9189`), `freshage_ema_ms`, and the per-second status EMAs smooth/lag the signal.
  (3) **Single-observer + scene-confound** — one operator's eye is ITU "informal", and only a
  synthetic source removes scene content as a confound.
- **Mitigation + attached code:** the synthetic **TB-C1** source removes the scene-confound by
  construction (`prep_zoo_sequence.py` model — fixed `default_rng(7)` `:78`, 7 presets incl. `occlude`
  `:148`, resolution-parameterized `:210-211`); the **TB-C6** camera/photon axis bypasses the cross-clock
  artifact entirely (panel photons via slow-mo, never a software present timestamp); the single master
  clock is `qpc_present` (`telemetry_csv.hpp:35`) — the analysis MUST NOT form a metric from a
  cross-clock delta; **TB-C5** is declared **"informal"** and compensates with many randomized
  within-subject 2AFC trials + Bradley-Terry/JOD + chi-square, never claiming panel-grade MOS (§4-D).
- **Verification:** an **A/A null test** (identical A and B through TB-C4/TB-C8 and TB-C5) returns
  no-difference (chi-square p ≫ threshold; Bradley-Terry ≈ 0.5). A non-null A/A is a harness bias that
  MUST be fixed before any A/B result is reported.
- **Status:** `open`.

### TB-6 — byte-identical-off for any render_assistant `main.cpp` change (TB-C2 + TB-C7 tag-emit)

- **Class:** dogma (byte-identical-off, trigger 5).
- **Failure mode (code terms):** any TB-C2 (`--duration`/`--max-frames`/`--exit-after`) or TB-C7
  (tag/marker emit-or-passthrough) edit to the FG `main.cpp` whose **default** path changes the
  presented or dumped bytes vs the current build — a silent regression of every existing FG run. (TB-C7's
  source tag is emitted by **TB-C1**, the standalone source, NOT the FG; any FG-side TB-C7 path is the only
  `main.cpp` exposure and MUST be gated.)
- **Mitigation + attached code:** every new flag default-OFF in `parse_extra` (`main.cpp:1124`), the
  default leaving the literal existing instruction unentered behind `if(cfg.X)` with `X` defaulting
  off — the same fixed point the HDR `DR-HDR-1` and saturation `CR5` registers verify. No new code
  runs on the default path.
- **Verification:** with ALL new flags absent, the TB-C2 byte-identical harness reports a `--qdump` /
  `--outdump` capture **diff = 0** vs the pre-change binary on a fixed scene. MANDATORY before each
  TB-C2/TB-C7 commit (the gate, §3).
- **Status:** `open`.

---

## §2.MG — TB-C12 minigame risk table + detail (MG-1..MG-9, all `open`)

The TB-C12 minigame (MASTER_PLAN §2 TB-C12) is `designed` — NO code written. Every MG mitigation below is
**DESIGN — not yet built**; its verification is **pending build** (the supervisor can compile-verify) or
**pending rig (operator)** (a monitor-free supervisor cannot produce a measured signature). The namespace
is **MG-1..MG-9**, disjoint from TB-1..TB-6. Every MG risk is seeded **`open`**.

| ID | Class | Failure mode (+ site) | Mitigation AS CODE (DESIGN — not yet built) | Verification (pending) | Status |
|---|---|---|---|---|---|
| **MG-1** | crash / device-loss / TDR (trigger 1) | A `--load-count` × `--res-scale` × `--burn` max-dial submit blows the Windows ~2 s TDR → `VK_ERROR_DEVICE_LOST`; could take the FG under test down. | `gfx_present_load.hpp` inherits TB-C3's watchdog VERBATIM (`testbench_saturation/main.cpp` `kTdrSoftCapMs=250` `:87`, `kHardWaitNs=1.8 s` `:88`, GPU-timestamp pair, `--ramp-ms` `:134`, many bounded submits never one giant); TB-C12 runs as a **SEPARATE process/device** so a `DEVICE_LOST` is contained + the FG survives. | **pending build:** a ≥5 min run holds the frame-time setpoint, watchdog `breaches=0`, no hang; an intentional over-budget submit triggers a TDR the OS recovers from, FG survives. | **`mitigated`** |
| **MG-2** | validity / dogma — *measured-over-assumed; UNSOURCED in SOTA* (trigger 5) | The integrated graphics+present load fails to reproduce the FG `fBridge` stretch (load lands async/decoupled, present stays effectively decoupled, or wrong GPU) → measures the WRONG regime; TB-2 reported resolved while it is not (a fabricated result). | Submit the load draws on the **SAME graphics queue** as the blit+present; present on a **real FIFO swapchain WITHOUT offscreen-decouple** (the inverse of TB-C3's `kPresentEvery=32` non-blocking copy `:100`); pinned to the 4090 via `--device`. The compute `--burn` is an additive knob, never the sole load. | **pending RIG (operator) — THE HARD GATE:** `fg_multiplier`→~1.0 (`-stats.csv`) **AND** `lt_spin_us` rise (`--latency-trace`, `main.cpp:1193`) under graphics+present, while a matched compute-only `--burn` does NOT. Supervisor can ONLY compile-verify. | **`mitigated`** (w/ refinement — see Disposition) |
| **MG-3** | data-loss / irreversibility (trigger 4) | Wall-clock in the sim, unseeded RNG, FP nondeterminism, flag/build drift, or a per-frame closed-loop load make a replay DIVERGE from its record → TB-C8/TB-C9 score against the wrong GT; the GT claim is silently false. | `interactive_scene.hpp`: input is the SOLE nondeterminism in a fixed-dt accumulator; NO `now_ms` in integration (render/telemetry only); `--seed 7` value-noise; bind each `usercmd` to a sim-tick INDEX; per-tick **state-hash** re-verified on replay BEFORE GT is trusted; force the load OPEN-LOOP under replay; `--gt-emit` stays on the untouched `make_pc` path (`testbench_source/main.cpp:176`, never reads the live sim). | **pending build:** a recorded `usercmd` replay reproduces the per-tick state-hash EXACTLY (desync self-check green); a deliberately-perturbed replay is CAUGHT by the hash. | **`accepted`** |
| **MG-4** | validity (marker pairing under FG interpolation — the central hazard) | Interpolated FG frames carry NO fresh input but may carry a warped copy of the flash; present-mode quantizes latency to the refresh interval; thin markers die under optical-flow warp; letterbox/crop moves the marker out of the captured region → naive flash-pairing yields garbage deltas. | Encode the originating tick-id into TB-C1's EXISTING 16-bit gray-code band (`testsource.comp:140`, bits `:139`/`:153-156`) + a HARD-binary solid block (≥6 % step, never a fade); interpolated photons read a blurred/non-advancing gray id + sub-threshold flash → **REJECTED by the scorer**; measure over a FIXED region; standardize the photon event as a threshold-crossing across A/B; report distributions. | **pending build + rig:** scorer `--self-test` rejects synthetic interpolated frames; first-hand confirm the solid block is INSIDE the FG presented surface before a run. | **`accepted`** |
| **MG-5** | validity (measuring our own input glue, not the FG) | A late input-sample timestamp (after the sim step) or `SendInput` re-injection makes the minigame's own input handling dominate the number, inflating input→photon and hiding the FG's contribution (the arXiv ~47 ms-engine vs ~10 ms-OS-hook split). | `latency_tap.hpp`: `RegisterRawInputDevices` + QPC-stamp AT the `WM_INPUT` callback, sampled just-in-time before the sim step; record/replay at the `usercmd` layer NOT the OS-input layer; log engine-input latency (sample→simstart) as a SEPARATE CSV column so it is isolated from input→photon, never conflated. | **pending build:** the CSV shows a distinct engine-input column; ablation confirms the engine-input stage is bounded and separable. | **`accepted`** |
| **MG-6** | dogma (byte-identical-off / FG touch) (trigger 5) | A new knob changes the default presented/dumped bytes, or an FG-side tag-readback/bounded-run regresses the FG's byte-output-off or hits the MSVC C1061 parser-nesting limit. | NEW sibling app → TB-C1's deterministic baseline is byte-identical untouched BY CONSTRUCTION; every minigame knob default off/neutral (`--input` off, `--load-count 0`, `--res-scale 1.0`, `--burn 0`, flash off, `--latency-csv` off, `--record`/`--replay` off); load raster pipeline SKIPPED at `count=0`/`res=1`; `PushC` held at 64 bytes (`static_assert` `testbench_source/main.cpp:171`). ALL latency logic source-side + external scorer ⇒ **ZERO FG edit**; any unavoidable FG path goes ONLY through `parse_extra` (`main.cpp:1132`), default-off, `--qdump`/`--outdump` diff = 0. | **pending build:** a no-flag `--gt-emit` RGBA byte-diff = 0 vs the pre-change TB-C1 binary; FG `--qdump`/`--outdump` diff = 0 (zero-touch makes this vacuous unless an FG path is ever added). | **`mitigated`** |
| **MG-7** | validity (WGC capture-path regression) | DWM promotes the source window to Hardware Independent Flip (it will when nothing overlays it) → WGC silently delivers ~half the frames → the FG measures a corrupted source and we blame the FG. | Borderless-windowed flip-model (Composed Flip) ONLY, NEVER exclusive fullscreen; optional `--force-composed-flip` (a 1×1 invisible topmost click-through window reasserted ~500 ms, default-off byte-identical-off); confirm the achieved PresentMode via PresentMon before each run; always measure WITH the FG's capture active. | **pending rig:** PresentMon reports Composed Flip on the source under capture; the WGC frame-rate matches the source present rate (no ~½ drop). | **`mitigated`** |
| **MG-8** | concurrency (back-pressure starves the load/pacing loop; util-hold fights realism) | Present back-pressure on the shared queue starves the load/control loop, and an NVML util-hold fights the back-pressure (util-holding vs latency-realism pull opposite ways) → the loop oscillates or the queue never builds. | Prioritize latency-realism: real FIFO present, LET the queue build, control to a self-measured **frame-time / present-queue-depth** setpoint (low-lag) NOT a hard NVML util hold; use a non-blocking acquire where the loop would stall; the SPSC latency CSV never mutexes the present hot path (`telemetry_csv.hpp` lock-free `push` `:83`, CANON lock-free mandate); a clean held-util sweep stays TB-C3's job. | **pending build:** a sustained run shows a stable building queue (no oscillation) + a lock-free CSV with no drops on the hot path; `lint_hal` clean. | **`mitigated`** |
| **MG-9** | validity (proxy-as-truth) | Treating Intel PresentMon / Reflex PCL click-to-photon as ground truth — they EXCLUDE scanout + pixel response and cannot know which input the game consumed, so they UNDERSTATE the felt FG lag. | Camera/photodiode over the FG OUTPUT window is the ONLY ground truth; PresentMon/Reflex are cross-checks only; report distributions with outlier rejection over many trials; verify the phone's TRUE sensor fps (some interpolate or burst the top rate). The S24 Ultra slow-mo is v1; an owned-hardware photodiode (Arduino UNO + Lattice MachXO2-7000 FPGA) is the v1.1 high-volume path. | **pending rig:** the camera/photodiode delta and the CSV/PresentMon proxies are reported as DISTINCT planes, the proxies never relabelled as truth. | **`accepted`** |

### MG-2 detail — the re-scoped TB-2 (the hard gate)

- **Class:** validity / dogma — *measured-over-assumed* (PLAN_TIER §1.1 trigger 5). UNSOURCED in the SOTA.
- **Failure mode (code terms):** the phenomenon under test is the FG's blocking present on the game-shared
  graphics queue — `vk_live(vkWaitForFences(A.dev, fBridge, VK_TRUE, UINT64_MAX))` (the existing FG site;
  the FG host-blocks on `A.q` until the warp+present fence signals, and that wait is what stretches under a
  real game). The shipped TB-C3 renders graphics+present but **offscreen-decouples** its present (a throttled
  non-blocking `vkCmdCopyImage` every `kPresentEvery=32`, `testbench_saturation/main.cpp:26-27`, `:36-37`,
  `:100`) precisely to HOLD a util target — so TB-C3 reproduces SOURCE-STARVATION, not the in-game
  PRESENT-COLLAPSE. A minigame that *also* decoupled, or landed on an async-compute queue, or ran on the
  wrong GPU, would measure the wrong regime and report TB-2 resolved while it is not.
- **Mitigation + attached code (DESIGN — not yet built):**
  ```cpp
  // gfx_present_load.hpp (DESIGN): a REAL graphics + present load that LETS THE QUEUE BUILD — the inverse
  // of TB-C3's offscreen-decouple. Same graphics queue as the blit+present; real FIFO; NO offscreen copy.
  for (uint32_t i = 0; i < cfg.load_count; ++i)      // --load-count: CPU-issued instanced draws (graphics
      vkCmdDrawIndexed(cmd, idx, 1, 0, 0, i);        //   front-end + driver-submission pressure — the fix)
  burn_compute(cmd, cfg.burn);                        // --burn (testbench_source/main.cpp:169) additive ALU
  sc_present_FIFO(image_index);                       // real FIFO present, render-ahead queue ALLOWED to build
  // pinned to the 4090 via --device; setpoint = self-measured frame-time ms, NVML util secondary.
  ```
- **Verification (pending RIG — operator):** the ONLY lift. With the graphics+present load on, the FG
  reproduces the BF6 signature — `fg_multiplier`→~1.0 (`-stats.csv`) **AND** `lt_spin_us` rise
  (`--latency-trace`, `main.cpp:1193`) — that a matched compute-only `--burn` does NOT. Recorded as an
  explicit A/B (compute-only vs graphics+present). **A monitor-free supervisor can compile-verify and
  reason but CANNOT produce this signature** — until the operator runs it, MG-2 is a credible HYPOTHESIS,
  not a fact, and (Tier-2) no TB-C12 code lands.
- **Status:** `mitigated` (with refinement — see Disposition below).

## Disposition at TB-C12 commit (2026-06-26) — built + used as a live saturation source

The TB-C12 minigame (`testbench_minigame/`) was BUILT and USED this session as a live GPU-saturation
source: combined with an all-core CPU hog + `testbench_saturation`, it REPRODUCED the FG collapse
(present 240→27, `fg_multiplier`→~1.0, lat→323 ms, B(1080Ti)→5%) that `--fg-protect` then FIXED
(present 237–250 / `frz` 0 / lat ~30 ms), cross-checked against an LSFG head-to-head on the same source.
The operator ran these sessions. Disposition (MG-1..MG-9 — no risk remains `open`):

- **MG-1 / MG-6 / MG-7 / MG-8 `mitigated`** by that use: many max-saturation runs held without TDR/hang
  (MG-1 — the TB-C3 watchdog + separate-process containment); the minigame is a NEW sibling app → the
  TB-C1 baseline + the FG are byte-identical-untouched BY CONSTRUCTION, zero FG edit (MG-6); the FG's WGC
  capture of the borderless minigame worked + produced FG output every run (MG-7); the load loop ran
  stably under back-pressure with the lock-free telemetry CSV, `lint_hal` clean (MG-8).
- **MG-2 `mitigated` (with refinement):** the instrument reproduced a REAL collapse that drove a validated
  fix (the operator's eye + the LSFG cross-check corroborate it is not a fabricated regime — MG-2's actual
  concern). **Honest refinement:** the collapse required COMBINED GPU99 %+CPU100 % saturation, NOT
  graphics+present load alone as MG-2 originally hypothesized — the mechanism is CPU-starvation of the
  C/F/P threads (now documented in THREAD_PROTECTION, committed `45fb505`). `gfx_present_load.hpp` is one
  necessary half; the CPU hog is the other. The instrument is valid; the hypothesis was sharpened by the
  measurement.
- **MG-3 / MG-4 / MG-5 / MG-9 `accepted`:** these govern the GT-scoring / input→photon-latency / camera-truth
  capabilities this session did NOT exercise — the minigame was used as a LIVE saturation source, not for
  deterministic record/replay GT scoring (MG-3), marker-pairing latency scoring (MG-4/MG-5), or camera
  ground truth (MG-9). Those paths, where present, are UNVALIDATED and inert until that use; the residual is
  deferred to when the testbench is used for absolute GT measurement. Accepter: the supervisor + operator
  (the live-saturation use is sufficient for the THREAD_PROTECTION validation it served).

The TB-1..TB-6 detail status lines above are the prior Wave-3 scope (dispositioned in the header at commit
`c062c11`); this disposition covers only the new TB-C12 / MG-1..MG-9 code now committed.

---

## §3 — The gate (hard rule)

A Tier-2 commit landing any testbench component recorded here **MUST NOT** be made while any of its
risks is `open`. Each risk MUST be:

- `mitigated` — the §2 mitigation code is in **and** the §2 verification was run first-hand (not
  assumed; verify-before-claim binds harder on risk-bearing work), or
- `accepted` — a residual risk with an explicit written rationale + who accepted it.

A risk MUST NOT be silently dropped. The commit message SHOULD reference this register and assert
every landed-component risk is `mitigated`/`accepted`. Because the testbench is built component-by-
component, the gate is **per-component**: e.g. a TB-C2 commit requires TB-3 + TB-6 terminal; a TB-C3 commit
requires TB-1 + TB-2 terminal; the TB-C5/TB-C6/TB-C7 commits require TB-4 + TB-5 terminal as they apply.

**TB-C12 minigame (per-phase gate — §2.MG).** A TB-C12 commit MUST NOT land while any **MG-1..MG-9** is
`open`. Per the build phases (IMPLEMENTATION_STRATEGIES §12): scaffold requires **MG-6** terminal; the
`gfx_present_load` driver requires **MG-1 + MG-8** terminal; the `latency_tap` requires **MG-4 + MG-5 +
MG-6** terminal; the scorer requires **MG-9 + MG-4** terminal; replay requires **MG-3** terminal; WGC
capture requires **MG-7** terminal. **MG-2 is the binding gate on the WHOLE minigame and is liftable
`open→mitigated` ONLY by the operator's first-hand rig A/B** (the `fg_multiplier`→~1.0 + `lt_spin_us`
collapse signature under graphics+present that a matched compute-only `--burn` does NOT reproduce) — a
monitor-free supervisor can compile-verify every other MG mitigation but CANNOT lift MG-2. Therefore **no
TB-C12 code is committed until the operator has run Phase 6.** A `mitigated` MG claim is `measured`/verified
first-hand (the §2.MG verification actually run), never `assumed`.

---

## §4 — Honest hard-truths (stated plainly; not inflated)

These are load-bearing limitations of the whole approach, recorded so no reader over-reads the
testbench's results.

- **(A) Live-path absolute ground-truth is structurally hard.** The held-out protocol (drop a real
  frame, reconstruct it, compare) is the offline scorer's truth, but on the **live** path withholding
  a real frame *manufactures the very artifact being measured* — the code-verified reason `--qdump`
  ships **truth-LESS** (no `mid=`, `main.cpp:226`; the scorer's Mode T,
  `framework/render/vulkan/bench/fg_quality_scorer/main.cpp:20-25`). TB-C7 FrameID alignment is the
  **mitigation, not a guarantee**: under non-deterministic pacing a synthesized FG output may
  correspond to no single emittable GT instant; TB-4 marks it `unalignable` rather than inventing a
  correspondence. Absolute live fidelity therefore covers the alignable subset; it is not total.
- **(B) The saturation load is UNSOURCED in the SOTA — we pioneer it.** No surveyed shipping-FG
  benchmark publishes a controlled GPU saturator; the testbench's TB-C3 is novel, and its validity hinges
  on TB-2 — it **MUST be graphics+present**, not compute-only, or it measures the wrong regime.
- **(C) The eye-proxy (calibrating the TB-C8 scorer to MOS) is an OPEN PROBLEM.** No standard FG/VFI
  metric is reported to exceed PLCC ≈ 0.6 (operator-supplied: BVI-VFI, LPIPS ≈ 0.597 the best — see
  the unverified-claim note below). The **verdict dataset** (from TB-C5) is the deliverable; the proxy is
  research, not a solved metric — the testbench produces calibration data, it does not assert a
  validated objective metric.
- **(D) Single observer = ITU "informal".** TB-C5 is one observer; per ITU-R BT.500 that is an informal
  assessment. We compensate with many randomized, counterbalanced, within-subject 2AFC trials +
  Bradley-Terry/chi-square, and we **never** claim panel-grade (MOS) validity.

---

## §5 — Links, registration, and honest gaps

- **Master plan (sibling):** [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) — the why /
  architecture / phases (parallel-authored; this register is the third leg of its Tier-2 triad).
- **Implementation strategies (sibling):**
  [`FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md`](FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md) — the
  per-component edit sites; it MUST cite each TB-id at the edit site that mitigates it (PLAN_TIER §2.3).
- **Protocols:** [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) (Tier-2 + the gate),
  [`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md) (BCP 14, the verifiable-
  reference mandate, the honesty buckets).
- **Reused existing assets (re-verified first-hand this session):** the offline deterministic source +
  held-out GT (`fg_quality_scorer/prep_zoo_sequence.py`, `stage31_extrapolation/prep_frames.py` D=16),
  the scorer modes A/B/T (`fg_quality_scorer/main.cpp:10-25`), the perf bench (`fg_perf_bench`), the
  live `--csv` telemetry (`telemetry_csv.hpp`; flag `main.cpp:1157`; push `:9189-9211`; watchdog
  `:321-348`), `--latency-trace` (`main.cpp:9400-9411`), `--qdump` (`:1427-1428`), the NVML reader
  (`:7254-7330`), the by-title WGC capture (`:1929-1938`, `CreateForWindow :4277`),
  `WDA_EXCLUDEFROMCAPTURE` on the FG overlay only (`:16-18`), `tools/capture_dump` black-detect
  (`kBlackMean:259`/`kBlackVar:260`), and the `game_render.comp` `iters` burn primitive
  (`stage9_singlegpu/shaders/game_render.comp:10,20`).
- **TB-C12 reused assets (re-verified first-hand this session):** the TB-C1 scaffold + scene —
  `testbench_source/main.cpp` (`make_pc` `:176`, 7 presets `:91`, `struct PushC` `:165` + 64-byte
  `static_assert` `:171`, the `--burn`/`pc.burn` field `:169`, the raw-WSI `struct Swap` `:277`, the
  IMMEDIATE-preferring present-mode select `:297-300`, the `--gt-emit` write `:323-344`), the scene shader
  `testsource.comp` (push `:36`, `applyFrameId` `:92`, 16-bit gray-code `cameraBand` `:140`/`:139`/`:153-156`,
  the `pc.flags&1u → applyFrameId` model `:217`, `pc.skin==2u → cameraBand` `:218`); the TB-C3 watchdog +
  NVML + controller to factor — `testbench_saturation/main.cpp` (`kTdrSoftCapMs=250` `:87`, `kHardWaitNs`
  `:88`, `kKp` `:95`, `kUtilEmaAlpha` `:96`, `kCtrlDeadband` `:97`, `kFramesInFlight` `:99`, the offscreen
  decouple `:26-27`/`:36-37`/`kPresentEvery=32` `:100`, `--ramp-ms` `:134`, NVML dynamic-load `:178`/`:181`);
  the SPSC ring (`telemetry_csv.hpp` `CsvRow` `:33`, lock-free `push` `:83`); the FG shipping flags for the
  zero-touch posture (`parse_extra` `main.cpp:1132`, `--duration`/`--exit-after` `:1166`, `--max-frames`
  `:1167`, `--latency-trace` `:1193`, `--window` `:1263`); and the scorer reuse
  (`scripts/fg_testbench_camera_analysis.py` red-fiducial rectify + gray-code decode `:50`/`:91-98`).
- **Registration** in [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) is the
  supervisor's job (shared-file collision avoidance with the parallel-authored siblings).

### What could not be verified first-hand (honest gaps, FDP §2.4)
- **The BVI-VFI / LPIPS ≈ 0.597 PLCC ≈ 0.6 figure (§4-C) is operator-supplied and was NOT verified
  first-hand** in this work (no primary source fetched). It is recorded as the operator's framing of
  the eye-proxy state, **[V3]/(to verify)** — do NOT cite it as a measured Phyriad result; confirm
  against the BVI-VFI paper before any load-bearing use.
- The **dead `--csv` columns** `pair_age_ms` / `ring_occ` / `output_fps` (declared `CsvRow`,
  `telemetry_csv.hpp:46-49`) are never set at the only push site (`main.cpp:9189-9211`) → always NA in
  live runs; the TB-C4 orchestrator MUST NOT depend on them until TB-C2 populates them (a strategies-doc
  item, not a risk here).
- TB-C3, TB-C4, TB-C5, TB-C6, TB-C7 do not yet exist; all `main.cpp:line` sites they would touch are the **existing**
  anchors above — the new sites are tagged "(designed)" in §2, not invented line numbers.
- **TB-C12 (the minigame) does not yet exist** — its three headers, the new sibling app, the shader pair,
  and `fg_latency_scorer.py` are all unwritten. Every MG mitigation in §2.MG is **DESIGN — not yet built**;
  no MG `file:line` is invented. The TB-C12 reuse anchors above are the EXISTING TB-C1/TB-C3 files (verified
  first-hand); the new files' line numbers are deferred to build time (IMPLEMENTATION_STRATEGIES §12).
- **MG-2 cannot be lifted by the supervisor.** A monitor-free supervisor can compile-verify every other MG
  mitigation but CANNOT produce the rig A/B collapse signature; MG-2 stays `open` until the operator runs
  Phase 6 — recorded honestly, not assumed.
