# R2 — gate record (G-R2, milestone M-R2): the SEAM adopted, grafted, and driving stage 5 · 2026-09-03

> Stage R2 of [`../CONVERGENCE_MASTER_PLAN.md`](../CONVERGENCE_MASTER_PLAN.md) §3 (strategy X15) and
> spine node **S4.0** (the adoption the operator authorised: the header + its test, not the app).
> **Verdict: G-R2 PASSED**, with the derived path opt-in behind `--sg-barriers` and the hand-written path
> kept as the reference and the fallback. Every number is quoted from command output.

## 1 · What was built

| Piece | What it is |
|---|---|
| **Vulkan 1.3 + `synchronization2`** | The seam's `vkCmdPipelineBarrier2` is core 1.3; the app ran 1.2 and enabled the KHR extension only under `--nvofa`. The loader version is now QUERIED (`vkEnumerateInstanceVersion`), the instance asks for 1.3 only if offered, and the feature is enabled only if the physical device reports 1.3 AND reports it supported. `--no-sync2` forces the old path. |
| **`--validation`** | The KHRONOS layer + a debug-utils messenger printing every warning/error, so "sync-validation clean" is a repeatable instrument. Default off: no layer, no messenger, byte-identical. |
| **The seam adopted** | `src/seam/seam_graph.hpp` + `tests/seam/test_seam_graph.cpp` (target `pfg_seam_test`), from `apps/minimal_fg`. Namespace, an adoption note and the signature are the only edits; the derivation is untouched. The container copy carries one pointer line and is frozen (XR10). |
| **Grafts G3 / G4 / G5** | G3: a pass that overwrites a resource it does not read is named at compile time (`dump_warnings()`), unless it declares why (`add_pass_dominating`). G4: `Access::optional` + `dead_optional_writes` liveness. G5: the `VkImageMemoryBarrier2` array is built at `compile()`; `execute()` patches handles only, and a `graph_id` makes it refuse a `Compiled` from another graph (XR11). |
| **The two engine gaps CLOSED (R2b)** | **Per-image import layout:** `declare_image(name, imported, import_layout)` — the present bridge arrives as `UNDEFINED` because the blit overwrites the whole image; assuming `SHADER_READ_ONLY` would declare contents that must be preserved. **Resting layout:** `set_resting(res, layout, stage, access)` — one compiled graph describes ONE frame, so the cross-frame restore (`wapOutA` back to `GENERAL` for the next tick's warp) had no consumer inside the graph. `compile()` now emits it as an EPILOGUE barrier; `execute()` records it last. Both default to the previous behaviour, so the 122-check golden is unchanged. |
| **Stage 5 wired** | `--sg-barriers` makes the generation-output path record its barriers through the graph instead of the three hand-written `img_barrier` calls. The blit is the graph's own record callback. Default off → the hand-written arm runs, byte-identical. A missing `synchronization2` or a compile error falls back automatically and says so. |

## 2 · The gate

| Check | Result (quoted) |
|---|---|
| build ×2 | `build.bat` and `build-release.bat` exit 0 |
| the seam's own tests | **`OK: all 148 checks passed`** (122 adopted + 13 grafts + 13 stage-5 shape/epilogue) |
| **derived == hand-written, field by field** | check `[10] STAGE-5 SHAPE` asserts the engine derives exactly the three hand-written barriers: `wapOutA GENERAL→TRANSFER_SRC (SHADER_WRITE→TRANSFER_READ)`, `bridge_img UNDEFINED→TRANSFER_DST (0→TRANSFER_WRITE)`, and the epilogue `wapOutA TRANSFER_SRC→GENERAL (TRANSFER_READ→SHADER_WRITE)` with `srcAccess = NONE` because reads do not dirty memory |
| the live graph | `--sg-dump`: `SG: 2 passes, 2 barriers` + `1 epilogue`; the RAW and the WAW exactly as above; **byte-identical across 2 runs** |
| G3 in the field | the first dump warned that `blit` overwrites `bridge_img` without reading it. That dominance IS intended (the blit overwrites the whole image), so it is now DECLARED with its reason and the warning count is `0` — the graft caught a real property of the shipping path and made it explicit rather than silent |
| no `ALL_COMMANDS` | asserted for every derived barrier and every epilogue barrier |
| **sync-validation clean** | `--validation` on the derived path: **0 lines** over a 10 s run, and **0 lines** over a 60 s soak with `ball_zoo` at 60 fps AND `gpu_load.exe` saturating the 4090, exiting `bounded-run clean exit: total_presents=14383` |
| **M3, 2 runs/side, interleaved, 60 s** | `ball_zoo` 60 fps 1920×1080. presents: **14,384 on all four runs**. |
| | | metric | A hand-written | B derived | Δ vs A's spread |
| | |---|---|---|---|
| | | presented-phase mean | 0.5013 ± 0.0004 | 0.5019 ± 0.0002 | +0.0006 |
| | | uniq/s | 238.981 ± 0.046 | 238.991 ± 0.023 | inside |
| | | lat (ms) | 16.64 ± 0.32 | 16.72 ± 0.08 | +0.08, inside |
| | | warp / GPU time (ms) | 2.787 ± 0.315 | 2.880 ± 0.104 | +0.093, inside |
| zero steady-state allocation | structural (G5: the arrays are built at `compile()`; `execute()` only patches `VkImage` handles) and test-checked (`vk_barriers` count == derived barrier count per pass). **Not** counted at runtime with an allocation hook — stated as designed-and-asserted, not measured. |

## 3 · Defects found and fixed while doing R2

- **The validation instrument caught its own author first.** The first `--validation` run reported
  `vkDestroyInstance(): VkInstance has 1 leaked object` — my debug messenger, never destroyed. Fixed;
  the re-run reports 0.
- The `synchronization2` report first landed inside the `--gpu-priority` retry branch, so it printed
  only on a retry. Moved after the create block.
- A CRLF-blind multi-line anchor aborted a patch script mid-file, leaving `device.cpp` untouched while
  its siblings were already written. Line-based, EOL-agnostic patching from then on.
- The stage-5 graph state was first declared next to the clock, inside the OUTPUT-CLOCK loop; the blit
  site lives inside the `wap_warp_present` lambda, which is DEFINED earlier. Moved above the lambda.
- The barrier text at the blit site is textually identical to one inside the `--ts-smooth` copy; the
  first patch attempt matched both. Anchored on the first occurrence with the +3 span asserted.

## 4 · Honesty ledger

- The derived path is **opt-in**. The default still records the hand-written barriers, so the shipping
  product is unchanged by R2 unless `--sg-barriers` is passed. Flipping the default is a separate
  decision with its own evidence (a longer soak, and the operator's eye on a real game).
- `img_barrier` calls in `present.cpp`: still 40. R2 did not remove any — it added a derived arm beside
  the three stage-5 ones. The count drops when the default flips and the fallback arm is retired.
- The A/B ran on `ball_zoo`, a synthetic source, on an otherwise idle machine plus one saturated soak.
  No real-game run, and no operator-eye verdict.
- The 1080 Ti is installed but not driver-available (operator, 2026-09-03), so the 4090 is the only
  usable FG device; the AMD iGPU also enumerates and the app collapses to `SINGLE-GPU`. Multi-GPU is
  deliberately deferred until the generation core is stable, then resumed. The A0 envelope's wording
  ("the only device enumerated") was imprecise; the reason is the driver, not absence.
- The Vulkan 1.3 instance is a device-creation change (crash class). It is queried, degrades to 1.2, and
  has now had a 60 s saturated validated soak plus every build's smoke — but not a multi-hour session.

*Made with my soul - Swately <3*
