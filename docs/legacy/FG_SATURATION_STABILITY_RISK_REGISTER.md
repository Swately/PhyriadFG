# FG_SATURATION_STABILITY_RISK_REGISTER — the Tier-2 risks (STEP 3 + STEP 4)

> **Diátaxis type:** Risk register (PLAN_TIER_PROTOCOL Tier-2). The crash / concurrency /
> device-loss / data-loss / dogma risks of the two Tier-2 steps of
> [`FG_SATURATION_STABILITY_MASTER_PLAN.md`](FG_SATURATION_STABILITY_MASTER_PLAN.md) — **STEP 3**
> (flip `--async-present` default-ON + drop-on-slip) and **STEP 4** (the make-space cross-adapter
> router). Buildable detail in
> [`FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md`](FG_SATURATION_STABILITY_IMPLEMENTATION_STRATEGIES.md).
>
> **Status:** `designed` — all risks **`open`** until the step is built + each mitigation is
> verified first-hand. **HARD GATE (protocol):** a Tier-2 step MUST NOT be committed while any
> of its risks is `open`; each must be `mitigated` (verified) or explicitly `accepted`. STEP
> 0/1/2 are Tier-0/1 and carry no register.
>
> **Reconciliation with prior registers:** `DEVICE_LOST_RECOVERY_RISK_REGISTER` (the `vk_live`
> graceful-exit, committed `2754b57`) and `REAL_FAST_PATH_RISK_REGISTER` (the `--rfp` CR1
> use-after-reset) are the parents; this register records what they already discharge vs what
> is NEW for the default-flip. `SINGLE_GPU_COLLAPSE_RISK_REGISTER` covers the FD-routing STEP 4
> builds on.

Each row: **failure mode (+ site) · mitigation AS CODE · first-hand Verification · Status.**

---

## STEP 3 — flip `--async-present` default-ON + drop-on-slip

### CR1 — use-after-reset (an async warp's bslot reset/reused while in flight) — crash-class
- **Failure (site):** the present thread resets/records into a `bslot` whose warp cmd-buffer +
  fence are still in flight on `A.q` → GPU fault → `VK_ERROR_DEVICE_LOST`. The original
  STAGE-102 reason the bridge buffers were split.
- **Mitigation as code:** ALREADY built — dedicated `cmdBridgeA0/fBridgeA0` for slot-0 when async
  (`main.cpp:7186-7192`), the `back = (async_front==0)?1:0` choice never records into the
  in-flight slot (`:7227`), the `record_this_tick` guard (`:7228`) + the `rfp` `record_this`
  guard (`:7669`). **NEW invariant for the metered drop:** on a drop, leave `async_inflight` on
  the still-running slot and poll it next tick — **never reset it** to record the real (the real
  falls through `rfp_present`, which picks the non-front free slot `:7668`).
- **Verification:** `VK_LAYER_KHRONOS_validation` clean (zero use-after-reset / cmd-buffer-reuse)
  across a 30 s BF6-combat `--async-present` run; the metered-drop branch exercised (force a slow
  warp via `--flow-scale 1` under saturation).
- **Status:** `open` (built+verified at S3b).

### CR2 — device-loss through the async poll (THE new hole the default-flip exposes) — crash-class, LEAD
- **Failure (site):** the three async `vkGetFenceStatus` polls (`main.cpp:7218`, `7617`, `7662`)
  test only `==VK_SUCCESS`; on a lost device the call returns `VK_ERROR_DEVICE_LOST`
  (`!=VK_SUCCESS`) → treated as "warp not ready" → the present loop **spins forever re-showing
  the front, never exits** (the window-death watchdog at `:8854` covers a dead SOURCE, not a lost
  DEVICE). Replacing the sync `vkWaitForFences(UINT64_MAX)` removes the one site (`vk_live` at
  `7003/7152/7510`) that catches a TDR today.
- **Mitigation as code (S3a, lands FIRST):** wrap every async poll +
  the `rfp` `!ap` wait (`:7688`): `VkResult r=vkGetFenceStatus(...); if(!vk_live(r)) break;
  if(r==VK_SUCCESS){…}`. `vk_live` (`:2882`) sets `g_quit` on the terminal code → the existing
  teardown → graceful exit. `vk_live` is identity on non-DEVICE_LOST → byte-identical.
- **Verification:** induce a TDR (a long dummy 4090 dispatch / a forced reset) → confirm a CLEAN
  exit (not a hang on the never-signaled fence); `lint_hal` clean.
- **Status:** `open` (built+verified at S3a — the gating prerequisite).

### CR3 — hang on a never-signaled fence (a stuck GPU, not a clean TDR) — liveness
- **Failure:** a slot's fence never completes (a wedged but not lost device) → the poll loop
  never advances.
- **Mitigation as code:** a drain watchdog — if a slot's fence is not `VK_SUCCESS` within
  `N·frame_period` (N≈30), treat as a hang → `g_quit` → graceful exit (mirror the window-death
  watchdog pattern at `:8854`).
- **Verification:** simulate a stuck fence (skip the submit for one slot) → watchdog trips +
  clean exit within the budget.
- **Status:** `open`.

### CR4 — torn / racing present (non-blocking present races the DComp compositor copy) — correctness
- **Failure:** a non-blocking present races `PresentSurface`'s `CopyResource` → tear/torn frame.
- **Mitigation as code:** keep the keyed mutex (key 0) serializing producer-vs-compositor on the
  async slots; on a present timeout, SKIP the present (count it), never block (the pillar already
  does this — `PresentSurface.hpp:111`).
- **Verification:** validation-layer clean; visual check (no tear) on the 30 s gate.
- **Status:** `open`.

### CR5 — byte-identical-off broken (the default-flip changes the off path) — dogma
- **Failure:** flipping the default + the `bslot[1]→bslot[0]` aliasing-when-off drift makes the
  OFF run differ from today's binary.
- **Mitigation as code:** keep the slot-0 aliasing exact when off; add an explicit
  `--no-async-present` hatch; gate the whole metered-drop behaviour behind `async on`.
- **Verification:** a CSV diff of a `--no-async-present` run vs the current binary on a fixed
  scene — `freshage`/`frz`/present-count identical (the `--nvofa` byte-identical precedent,
  `FG_NVOFA_PRIOR_ART §8`).
- **Status:** `open`.

---

## STEP 4 — the make-space cross-adapter router

### CR6 — device-loss across TWO adapters — crash-class
- **Failure:** a TDR on either the render-GPU (4090) or the FG-GPU (1080Ti/iGPU) with present +
  warp split across adapters → one side hangs/faults.
- **Mitigation as code:** `vk_live` on BOTH devices' submit/wait sites → `g_quit` → graceful exit
  to all-on-A passthrough (the external-overlay degrade floor).
- **Verification:** induce a TDR on each adapter independently → clean exit / graceful degrade.
- **Status:** `open`.

### CR7 — GpuDescriptor POD layout change (a cross-process ABI break) — data-integrity
- **Failure:** adding the fp16/DP4a field to `GpuDescriptor`
  (`framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp:49`) changes a published cross-process POD
  → a stale producer/consumer mismatch = garbage routing.
- **Mitigation as code:** a versioned/`static_assert`-sized POD; bump the version; **OPERATOR
  APPROVAL required** before it ships; verify both the producer and every consumer rebuild
  against the new layout.
- **Verification:** `static_assert(sizeof(GpuDescriptor)==…)`; a round-trip test of the new field;
  operator sign-off recorded.
- **Status:** `open` (operator-gated).

### CR8 — the monitor-on-FG-GPU topology requirement (a physical cable move) — usability/correctness
- **Failure:** for the FG-GPU to scan out, the monitor must be driven by the FG-GPU — the
  cross-adapter present otherwise pays a copy-BACK that erases the make-space win (or cannot scan
  out at all).
- **Mitigation as code:** detect the display-adapter (`IDXGIOutput`→adapter); if the monitor is
  NOT on the chosen FG-GPU → `recommend-disable` (surface the guidance, keep present on A) rather
  than silently degrade. Honest: this lever requires the user to connect the monitor to the FG-GPU.
- **Verification:** with the monitor on A (4090) → the router keeps present on A + logs the
  recommendation; with the monitor on the FG-GPU → the make-space path engages + measures a net win.
- **Status:** `open` (the contract may narrow to "route flow/OFA/convert off A, keep present on A"
  if the cable-move is not worth it — master-plan §3 D5).

### CR9 — break-even mis-route (shipping the author's rig crossover) — correctness/perf
- **Failure:** hardcoding the ~596 FLOP/byte crossover (the dead router's `offload=false`)
  mis-routes on any other rig (the LSFG 4080+1080 net-loss case).
- **Mitigation as code:** `measure_transfer_bw` at real gen×lanes per rig + a per-rig break-even
  gate (`break_even_decide`); gate on render-GPU-saturated (NVML) AND secondary-clears-transfer
  +compute; never a compile-time constant.
- **Verification:** the route decision flips correctly on a synthetic high-headroom vs
  saturated case; the measured net win is logged, not asserted.
- **Status:** `open`.

---

## Discharge progress (2026-06-22, autonomous session)
- **CR2 (LEAD) — `mitigated`:** S3a wraps the 3 async `vkGetFenceStatus` polls + the dead `rfp` !ap wait in `vk_live` (committed `3f6cbd2`); identity on the happy path (byte-id), `lint_hal` clean.
- **CR3 — `accepted`:** the naive in-flight-age watchdog FALSE-POSITIVED — it read `async_inflight` at the tick-top, BEFORE the in-lambda preamble that clears it runs later, so it ~always read `>=0` and fired at 2 s on a HEALTHY plain `--async-present` run (verified: it killed the measurement runs; `--shallow-queue`/`--target-output-fps` masked it because their drops let `async_inflight` reach −1). REVERTED. A correct warp-wedge watchdog needs a warp-COMPLETION counter advanced in the preamble (a future refinement). **Accepted:** device-loss is caught by S3a (`vk_live`), source-death by the window watchdog; a wedged-not-lost GPU that never recovers is rare + externally observable (the user kills it). A buggy watchdog that kills healthy runs is worse than none.
- **CR1 — `mitigated` (design + soak):** the STAGE-102 `bslot` split was BUILT to prevent use-after-reset (the back-slot choice never resets the in-flight slot; the `record_this_tick`/`rfp record_this` guards — read first-hand). **4 clean soaks** (BF6 `--async-present --shallow-queue`, ~18–22 s each, both saturated-combat `gme dis:79%` and stable-240 regimes): no crash, no hang, `to=0 er=0`, the drop-on-slip exercised (`sq` H/M). **HONEST GAP:** the Khronos validation layer LOADS but routes its messages to the debugger (OutputDebugString), not stderr — the per-VUID sync/threading report was NOT capturable from this harness. A `--validation` `VK_EXT_debug_utils` messenger (a useful future artifact) is the way to capture it rigorously.
- **CR4 — `mitigated` (design + soak):** the keyed mutex (key 0) serializes producer↔compositor; the soaks showed `to=0` (no present timeouts) + no visible tear in the stat path. Same validation-capture gap as CR1.
- **CR5 — `mitigated`:** byte-identical-off verified — the new binary's all-off run (`bf6_s3_off.csv`: mean 7.81 ms / AddedLat 73 ms) matches the prior baseline's collapse regime; the off-path is the exact original code.

## Discharge ledger
| Risk | Step | Class | Status |
|---|---|---|---|
| CR1 use-after-reset | S3 | crash | mitigated (design `bslot` split + 4 clean soaks; validation-capture gap noted) |
| CR2 device-loss async-poll | S3 | crash (LEAD) | mitigated (S3a `vk_live`, `3f6cbd2`) |
| CR3 never-signaled-fence hang | S3 | liveness | accepted (naive watchdog false-positived → reverted; rare + externally observable; a completion-counter watchdog is a future refinement) |
| CR4 torn present | S3 | correctness | mitigated (keyed mutex + clean soaks `to=0`; validation-capture gap) |
| CR5 byte-identical-off | S3 | dogma | mitigated (`bf6_s3_off` == baseline) |
| CR6 cross-adapter device-loss | S4 | crash | open |
| CR7 GpuDescriptor POD | S4 | data-integrity | open (operator-gated) |
| CR8 monitor-on-FG-GPU | S4 | usability | open |
| CR9 break-even mis-route | S4 | perf | open |

**STEP-3 status:** CR1/CR2/CR4/CR5 `mitigated`, CR3 `accepted` → `--async-present` is **READY to flip default-ON**, but the flip is **HELD for the operator's eye** (a shipped-behaviour / ease-of-consumption decision = his end-of-session call) + ideally the `--validation` messenger to close the validation-capture gap first. Until then `--async-present` ships default-OFF; the measured latency win (73→32 ms) is available via the flag. **No STEP-4 commit until CR6–CR9 are `mitigated`/`accepted`.**
