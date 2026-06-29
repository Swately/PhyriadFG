# REAL_FAST_PATH_RISK_REGISTER — STAGE-109 `--real-fast-path` (`--rfp`)

> **Diátaxis type:** Risk register (the Tier-2 document of `PLAN_TIER_PROTOCOL.md`). **The first
> native Tier-2 application** under that protocol. **Status:** `designed` — no risk is `mitigated`
> until its Verification gate is run first-hand against the built code.
> **Linked set (Tier-2 triad):** master-plan
> [`INPUT_LAG_DREDUCTION_MASTER_PLAN.md`](INPUT_LAG_DREDUCTION_MASTER_PLAN.md) §3/§4 ·
> strategies [`REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md`](REAL_FAST_PATH_IMPLEMENTATION_STRATEGIES.md)
> · this register. Each strategy edit MUST cite the risk ID it mitigates; the commit MUST assert
> every risk `mitigated` or `accepted`.
> **Provenance:** the `fg-dreduction-design` workflow (run `wf_e95d2e87`) adversarial pass; line
> anchors are symbolic (function/variable names) — they drift, so the implementer + verifier
> re-confirm first-hand.

## What `--real-fast-path` is (scope, honestly)

The FG_REARCHITECTURE stage-0 responsiveness lever: present the freshest captured **REAL** frame
on a real tick with **minimal D** (a real needs no interpolation pair → no pair-wait), while
**interpolated** ticks keep the (Stage-1-trimmed) D. A **PARTIAL** latency cut (reals only), NOT
a full pipeline-depth reduction. Default-off, byte-identical-off. It is crash-class because the
present path it must reuse is shared with the async warp.

## The gate (PLAN_TIER_PROTOCOL §3)

A risk `open` BLOCKS the commit. `mitigated` = the code is in AND its Verification was run
first-hand. `accepted` = a residual, with the rationale + who accepted it.

| ID | Class | Failure mode (what goes wrong, where) | Mitigation — AS CODE | Verification | Status |
|---|---|---|---|---|---|
| **CR1** | crash / use-after-reset | The naive real-fast-path presents the real via `do_present_P`→`bridge_present_src` (SYNCHRONOUS on `A.q`, `vkResetCommandBuffer(cmdBridge)`/`vkResetFences(fBridge)`/`vkWaitForFences`) while an `--async-present` warp still holds slot-0's dedicated `cmdBridgeA0`/`fBridgeA0`/`bslot[0]`. Resetting/awaiting the SHARED bridge resources mid-async-flight → use-after-reset GPU-fault (`VK_ERROR_DEVICE_LOST`) — the exact STAGE-102 reason the buffers were split. | Present the real through the **DEDICATED async slot path** (the `bslot[]` re-present tail in `wap_warp_present`, `ra_surface.submit(bslot[front].nt)`), NEVER `do_present_P`, whenever `cfg.async_present`. The real-fast-path records the real into a dedicated slot (or re-uses the async front-slot re-present), exactly mirroring the async warp's non-blocking present. No `cmdBridge`/`fBridge` reset on the present thread while a warp is in flight. | 30 s BF6 combat with `--upload-xfer --async-present --rfp`: NO `nvlddmkm`/device-lost, watchdog 0 stalls, clean exit. Validation layers (if armed) clean of use-after-reset. | open |
| **CR2** | crash-adjacent / freeze | Using `last_pres_k = INT_MAX` as the "real shown" sentinel: the backwards guard (`pair_c==last_pres_cseq && cand_k<last_pres_k`) then trips on the next interp of the same fresh pair → forces `t_use→1.0` → the pair freezes at phase 1 until `pair_c` advances. | Set the real tick's `last_pres_k` to the **true phase-1 key** `(int)(span_ms/0.1+0.5)`, not INT_MAX, so the next interp compares against a real phase and does not self-trip the backwards clamp. | `--phaselog`: after a real-fast-path tick, the next interp ladder advances normally (no stuck `t=[1.00]` runs); `frz` does NOT rise vs `--rfp` off. | open |
| **CR3** | correctness / wrong-size copy | `do_present_P`/the real path skips the `do_upscale_real_P(rs)` pre-pass; the bridge copy is `pres_w×pres_h` which ≠ `WW` when `--upscale` is on → a wrong-size/garbage present. | Replicate the `do_upscale_real_P` pre-pass on the real-fast-path tick, OR gate `--rfp` off when `use_upscale` (assert `!use_upscale`) with a printed notice. | A `--rfp` + `--upscale` run is either correct (upscale replicated) or cleanly refused (the notice); no garbage/wrong-size present. | open |
| **CR4** | correctness / double-count | A manual `++total_frames` on the real-fast-path tick double-counts — `total_frames.fetch_add(1)` already lives ONLY in the present helpers (`bridge_present` / the async path). | Do NOT manually bump `total_frames`; rely on the present helper's existing increment on the path actually taken. | The `-stats` `frame_count` / present-fps is consistent (no ~2× inflation) vs `--rfp` off. | open |
| **CR5** | correctness / cadence regression | A low-lag real interleaved among delayed interp ticks breaks the `--phase-norm` even ladder (a fresh real at the "wrong" phase among the even interp grid → a visible cadence discontinuity). | Advance `last_pres_*` cleanly on a real-fast-path tick (CR2 key) so the NEXT interp's `--phase-norm` grid resumes from the real's content position; eye-validate the ladder with `--rfp` + `--phase-norm` both on. | `--phaselog` + operator eye: with `--rfp --phase-norm`, the ladder stays coherent (the real is a clean step, not a lurch); no new judder. | open |
| **FR1** | freeze (low risk) | A real-fast-path tick presents the freshest captured real — but if no real is captured yet (startup) or the capture ring is empty, presenting "the real" reaches for nothing → black/stale. | Gate the real-fast-path on a valid captured real available (the capture ring has a fresh slot, mirroring the `async_front>=0` guard used by `--fdrop`); else fall through to the normal interp present. | Startup + steady-state: no black/stale frame on `--rfp`; the gate falls through cleanly when no real is ready. | open |

## Residual / accepted (none yet)

To be filled at the pre-commit gate: any risk that ends `accepted` records the rationale + the
residual here. As of design time, all are `open` (the build has not run).
