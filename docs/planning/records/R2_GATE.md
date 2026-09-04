# R2 — record: the SEAM adopted, grafted and shape-verified · 2026-09-03

> Stage R2 of [`../CONVERGENCE_MASTER_PLAN.md`](../CONVERGENCE_MASTER_PLAN.md) §3 (strategy X15) and
> spine node **S4.0** (the adoption the operator authorised: the header + its test, not the app).
> **Status: R2 is PARTIALLY DONE — the seam is adopted, grafted and verified; the live rewiring of the
> present path's barriers is NOT done and is the next step.** The gate G-R2 as written is therefore NOT
> claimed. Every number below is quoted from command output.

## 1 · Done and verified

| Piece | Evidence |
|---|---|
| **Vulkan 1.3 + `synchronization2`** — the seam's `vkCmdPipelineBarrier2` is core 1.3; the app ran 1.2 and enabled the KHR extension only under `--nvofa`. Now: the loader version is QUERIED (`vkEnumerateInstanceVersion`), the instance asks for 1.3 only if offered, and the feature is enabled only if the physical device reports 1.3 AND reports it supported. `--no-sync2` forces the old 1.2 path (the A/B arm). | `[ra] vulkan: loader 1.4.357 -> instance 1.3 (synchronization2 available)` · `[ra] device 'NVIDIA GeForce RTX 4090': synchronization2=ENABLED` · `[ra] device 'AMD Radeon(TM) Graphics': synchronization2=ENABLED` |
| **`--validation`** — the KHRONOS layer + a debug-utils messenger that prints every warning/error, so "sync-validation clean" is a repeatable instrument instead of an environment variable. Default off: no layer, no messenger, byte-identical. | 12 s run on the new 1.3 instance: **0 validation lines** (`[vk-` count = 0) |
| **The seam adopted** — `apps/minimal_fg/include/minimal_fg/seam_graph.hpp` → `src/seam/seam_graph.hpp` (namespace `minimal_fg` → `pfg::seam`, an adoption note, the signature; the derivation untouched); the test → `tests/seam/test_seam_graph.cpp` with the `pfg_seam_test` target. The container copy got ONE pointer line saying the repo copy is canonical (XR10). | `OK: all 122 checks passed` on the first build inside the repo |
| **Graft G3 — dominance warning.** A pass that OVERWRITES a resource it does not read is named at compile time (`Compiled::warnings`), unless it declares why via `add_pass_dominating(..., reason)`. Printed by `dump_warnings()`, never by `dump()`. | the 122 golden checks still pass byte-identically; a new check asserts the declared form is silent AND that `dump()` is identical either way |
| **Graft G4 — `optional_write` + liveness.** `Access::optional`; `compile()` reports optional writes nothing live reads in `Compiled::dead_optional_writes`. | new checks: a dead optional write is reported as `gen:aux`; the same write becomes live when a later pass reads it |
| **Graft G5 — zero-allocation `execute()`.** The `VkImageMemoryBarrier2` array is built at `compile()`; `execute()` patches only the `VkImage` handles. Fixes a real defect: a per-pass, per-frame heap allocation on the recording thread (~1,920/s at 240 Hz × 8 passes). XR11: handles re-patched every call, and a `graph_id` makes `execute()` refuse a `Compiled` from another graph. | new checks: `vk_barriers` count == derived barrier count for every pass; two graphs never share an id |
| **Test total** | `OK: all 142 checks passed` (122 adopted + 13 grafts + 7 stage-5 shape) |
| builds | `build.bat` and `build-release.bat` exit 0 throughout |

## 2 · The stage-5 shape, and the two gaps that block the flip

Rather than assert in prose what the graph "would" derive, check `[10] STAGE-5 SHAPE` declares the real
generation-output graph (`warp` writes `wapOutA`; `blit` reads it and writes the imported `bridge_img`)
and asserts what the engine actually produces. It reproduces the hand-written RAW barrier exactly:

```
wapOutA  COMPUTE_SHADER:SHADER_WRITE -> COPY:TRANSFER_READ   GENERAL -> TRANSFER_SRC_OPTIMAL
```

and it pins the two things the engine cannot yet express, as assertions so they cannot be forgotten:

- **Gap A — the imported-image layout.** The engine assumes every imported resource arrives in
  `kImportLayout` (`SHADER_READ_ONLY_OPTIMAL`). `bridge_img` actually arrives as `UNDEFINED`, because the
  hand-written barrier DISCARDS its contents (the blit overwrites the whole image). Deriving
  `SRO → TRANSFER_DST` is not "the same but slower": it declares contents that must be preserved. The
  engine needs a per-image import layout — its header already names this as a later refinement.
- **Gap B — the cross-frame restore.** The hand-written path returns `wapOutA` to `GENERAL` at the end of
  every tick so the NEXT tick's warp finds it there. One compiled graph describes ONE frame, so the
  engine emits no such barrier: the restore has no consumer inside the graph. It needs either a trailing
  write-back pass or a declared per-frame resting layout.

Both are engine changes, and both sit on the crash class (a wrong layout or a missing cross-frame
dependency is corruption or device-loss, not a slow frame). Rushing them at the end of a long session
would be the wrong call; they are the next step's scope, with the flip behind an opt-in flag and the
G-R2 gate (the `img_barrier` count drop, `--sg-dump` identical ×2, sync-validation clean, 0 steady-state
allocations, M3 within spread) applied to it.

## 3 · Defects found and fixed while doing R2

- **The validation instrument caught its own author first.** The first `--validation` run reported
  `vkDestroyInstance(): VkInstance has 1 leaked object` — my debug messenger, never destroyed. Fixed
  (the handle is a global the teardown destroys before the instance); the re-run reports 0 lines.
- The `synchronization2` report first landed inside the `--gpu-priority` retry branch, so it printed
  only on a retry. Moved after the create block.
- A CRLF-blind multi-line anchor aborted a patch script mid-file, leaving `device.cpp` untouched while
  its sibling files were already written. Line-based, EOL-agnostic patching from then on.

## 4 · Honesty ledger

- **G-R2 is not passed and is not claimed.** What is verified is the adoption, the three grafts, the
  prerequisites and the derived-shape comparison. The live present path still records its own barriers,
  unchanged; the product's behaviour is untouched by R2 so far.
- The A0 envelope says "the RTX 4090 is the ONLY Vulkan device enumerated". Measured now: **two** devices
  enumerate — the 4090 and the CPU's AMD Radeon integrated GPU. **Operator, 2026-09-03:** the 1080 Ti is
  physically installed but NOT driver-available, so the 4090 is the only usable FG device; multi-GPU is
  deliberately deferred until the generation core is stable, and is then resumed. The app collapses to
  `SINGLE-GPU: PhyriadFG collapsed onto device A alone. B/G suppressed`, so the envelope's conclusion
  holds; its wording ("only device enumerated") was imprecise, and the reason is the driver, not absence.
- The 1.3 instance changes device creation — the crash class. It is queried and degrades to 1.2, and it
  was exercised by the 12 s validated run plus every build's smoke, but it has NOT had a long soak or a
  saturated run under `gpu_load`. That belongs to the flip's gate.
- `--validation` costs real time (it is a layer). No measurement run used it.

*Made with my soul - Swately <3*
