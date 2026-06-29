# FG_REALFAST_PATH_RISK_REGISTER — the Tier-2 gate for the real-fast-path own-latency arc

> **Diátaxis type:** Risk register (the Tier-2 document of [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)).
> **Status:** `partially-built` — **L2 `--fwd-prestage` is BUILT + verified first-hand** (RFP-CR2 + RFP-DOGMA1
> `mitigated`, see below; supervisor pass 2026-06-24); **L3 `--copy-device` is BUILT (STAGE-121) + supervisor-
> verified** — RFP-FR2 + RFP-DOGMA1 `mitigated` (implementer code-analysis + **supervisor pass 2026-06-24**: build
> + alias-gate + teardown + a null-at-HSR A/B + 2 clean runs; the BF6-4K rig effect + the 30 s-clean stay pending);
> L0 `--rfp` ships (its CR1 governed by the L0 register);
> **L1's FR1-clamp remains UNBUILT** (RFP-FR1 `open`, for its future build). A risk is `mitigated` only when its
> lever's code is IN and its verification was run first-hand. **Supervisor re-verified L3 first-hand 2026-06-24**
> (build + alias-gate + teardown + a null-at-HSR A/B + no-crash); the BF6-4K rig run (effect + the 30 s-clean) is
> the remaining operator step.
> **Linked set (Tier-2 triad):** master-plan [`FG_REALFAST_PATH_MASTER_PLAN.md`](FG_REALFAST_PATH_MASTER_PLAN.md)
> §5 · strategies [`FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md`](FG_REALFAST_PATH_IMPLEMENTATION_STRATEGIES.md)
> · this register. Each strategy edit MUST cite the risk ID it mitigates; the commit MUST assert every risk
> `mitigated` or `accepted`.
> **Reconciles with (does NOT duplicate):** [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md)
> (the STAGE-109 `--rfp` register — its CR1-CR5/FR1 govern Lever L0, the BUILT path; this register adds the NEW
> risks of L1/L2/L3 and INDEXES L0's crash-class invariant as RFP-CR1 without re-deriving it).
> **Provenance:** authored from the verified-findings pass; `file:line` anchors re-confirmed first-hand against
> `apps/render_assistant/src/main.cpp` — symbolic (function/variable) anchors drift, so the implementer + verifier
> re-confirm first-hand. **Correction pass (2026-06-23):** the RFP-CR1 present-path anchor had drifted to
> `:6111-6129` (which is the `cfg.tiers` pressure-tier ladder, NOT a present helper); repointed first-hand to
> `do_present_P` `:7188` / `bridge_present_src` `:7165-7184`.

The key words MUST, MUST NOT, SHOULD, MAY are BCP 14 (RFC 2119 / RFC 8174).

## What this arc is (scope, honestly)

Reduce PhyriadFG's OWN added latency on single-GPU (the ~73-84 ms `measured` floor, `freshage` ~33-41 ms
dominant) — distinct from the game-pacing own-window lever (Option A). It is Tier-2 because: (1) a real present
on the wrong path is **use-after-reset → `VK_ERROR_DEVICE_LOST`** (crash-class — the STAGE-102 reason the bridge
buffers were split); (2) the #1 floor lever (L2) mutates the HOT F-build path and may add a dedicated transfer
queue (concurrency hazard + the byte-identical-off dogma at stake). Default-off, byte-identical-off throughout.

## The gate (PLAN_TIER_PROTOCOL §3)

A risk `open` BLOCKS the commit. `mitigated` = the code is in AND its Verification was run first-hand.
`accepted` = a residual, with the rationale + who accepted it.

| ID | Class | Failure mode (what goes wrong, where) | Mitigation — AS CODE | Verification | Status |
|---|---|---|---|---|---|
| **RFP-CR1** | crash / use-after-reset | A real present (L0 base AND L1's widened firing) that reaches the SYNCHRONOUS `do_present_P`→`bridge_present_src` (`do_present_P` lambda `main.cpp:7188`; `bridge_present_src` lambda `:7165-7184`, which `vkResetCommandBuffer(cmdBridge)` at `:7167` / `vkResetFences(fBridge)` at `:7176` / `vkQueueSubmit(A.q)`+`vkWaitForFences(fBridge,UINT64_MAX)` at `:7182` on the SHARED bridge) while an `--async-present` warp holds slot-0's dedicated buffers → use-after-reset GPU-fault (`VK_ERROR_DEVICE_LOST`). The STAGE-102 split reason. (Indexes the prior register's CR1 — UNCHANGED by L1, which only widens the FIRE condition, never the present body.) | The real is presented ONLY via `rfp_present(slot)` (the dedicated async bslot re-present, `main.cpp:8485`), NEVER `do_present_P`. `--rfp` is FORCE-DISABLED when `--async-present` is not live (`main.cpp:4904-4906`, the printed refusal). L1 changes only the gate at `:8476-8478`; the present body `:8485-8508` is the verified L0 path. | 30 s BF6 combat `--upload-xfer --async-present --rfp --rfp-window <wide>`: NO `nvlddmkm`/device-lost, watchdog 0 stalls, clean exit; validation layers (if armed) clean of use-after-reset. | open |
| **RFP-CR2** | crash / concurrency | L2's B-copy reshape: a dedicated B-transfer queue (or a pre-staged double-buffered copy) writes the device staging buffer (`hRP_b_dev[s]`, target of the copy at `main.cpp:6550`) while the F-build's first read (`vkCmdCopyBuffer`/the compute consuming it, `:6550-6553`) has NO happens-before edge → torn buffer / read-before-write / device-fault; OR the new queue shares command-buffer/fence state with F across families → ownership violation. | **AS BUILT** (`do_prestage`, `main.cpp:6625-6638`): the copy is split into a double-buffered prestage `cmdF_pre[g_seq&1]` submitted NO-WAIT on the **SAME** F flow lane as `cmdF`, BEFORE it — `flow_submit_nowait`/`_wait` BOTH route to **A.q2** (single-GPU) / **B.q** (multi-GPU), verified first-hand `:5377-5393`. `cmdF` KEEPS the `hRP_b_dev` TRANSFER_WRITE→SHADER_READ barrier (`:6638`). Same queue + submission order ⇒ the barrier's first scope reaches the prior-submission copy (the standard same-queue cross-submission edge — NO QFOT, NO semaphore, NO dedicated queue, NO mutex on the F hot path). Ping-pong reuse is safe (the copy completes within the pair: `cmdF`'s barrier serializes it ahead of `cmdF`'s compute, drained by `flow_submit_wait`). Gated `cfg.fwd_prestage`. | **VERIFIED FIRST-HAND** (supervisor 2026-06-24): same-queue routing (`:5378`/`:5387` both A.q2) + the submission-order barrier semantics + the shutdown drain (`vkDeviceWaitIdle(A)` before `fF_pre`/pool destroy `:9316`) read first-hand; 2 clean runs HSR 1080p (18 s + 13 s, no device-lost, `frz 0`, clean exit); `lint_hal` exit 0. **RESIDUAL (follow-ups, NOT crash-blockers):** the Vulkan validation-layer sync-check could not be armed here (no SDK / empty stderr); the `--latency-trace` `build` drop is **NULL at HSR 1080p** — the ~11 ms B-ingest copy is a 4K-CONTENDED phenomenon (at 1080p the copy is sub-ms, `build` already compute-bound ~9 ms), so the EFFECT awaits a BF6-4K run. | mitigated |
| **RFP-FR1** | freeze | L1 widens the firing window so a fresh real fires at a `phase_global` LOW enough that the next interp's backwards guard (`pair_c==last_pres_cseq && cand_k<last_pres_k`, key `(int)(span_ms/0.1+0.5)`) self-trips → `t_use→1.0` → the pair freezes at phase 1. | Clamp the exposed `--rfp-window` to a verified safe CEILING so the fire phase stays above the backwards-guard key boundary; KEEP `gen_back==0` + `cur_c!=last_rfp_c` (`main.cpp:8477`) UNCHANGED; reuse the L0 phase-1 key tail (`:8495`) verbatim so the next interp resumes cleanly. | `--phaselog` with `--rfp-window <wide>`: after a real tick the interp ladder advances normally (no stuck `t=[1.00]` runs); `frz` does NOT rise vs shipped `--rfp`. | open |
| **RFP-DOGMA1** | dogma (byte-identical-off) | L2's `--fwd-prestage` (and L3's `--copy-device`) touch the hot F-build path / the WGC copy rather than adding a gated read-side branch; an imperfect gate leaves the OFF path NOT character-for-character identical → the efficiency/byte-identical dogma (D-2) breaks (a silent behavior or perf change when off). | The new pipeline reshape is gated on a `cfg` bool (`fwd_prestage`/`copy_device`); when false the else-arm is the CURRENT code character-for-character (the `:6550` copy on `cmdF`; the current WGC copy device) — the proven `--upload-xfer`/`--fwd-pipeline` discipline. Flags live in `parse_extra`, NOT the main else-if chain (C1061). | **VERIFIED FIRST-HAND for L2** (supervisor 2026-06-24): the OFF path is character-for-character the original — `do_prestage` false ⇒ the prestage block (`:6626-6633`) is skipped AND `if(!do_prestage)` records the `:6637` copy inline on `cmdF` exactly as before; the `cmdF_pre`/`fF_pre` alloc+fence are guarded `if(use_fwd_prestage)` (`:4802`/`:4824`) ⇒ no alloc, no fence, no hot-path branch when off (D-2). Flag in `parse_extra` (not the C1061 chain). `lint_hal` exit 0 (1032 files, zero `memory_order_*` outside hal). RESIDUAL: the `--csv` present-stream A/B is a recommended empirical confirm (the code-proof is exact). **L3's `--copy-device` is now BUILT (STAGE-121) with the same discipline:** a single `cfg.copy_device` bool gates an alias-pointer pair `cap_dev`/`cap_ctx` that EQUAL `d.dev`/`d.ctx` when off — so every WGC-path device reference (the pool's `d3d_winrt` `:4140`, the ring alloc `:4167-4171` `else`-arm, the copy-fence probe `:4190`/`:4193`, the callback `raw_ctx` `:4217`, the C-thread `Map`/`Unmap` `:5290`/`:5292`/`:5305`, the teardown `:9385`) executes the IDENTICAL instruction stream when off; the new device-create + ring[0]/cdev/cctx-release branches are all `if(cfg.copy_device)`-dead when off. Flag in `parse_extra`. **VERIFIED FIRST-HAND (implementer 2026-06-24):** read each gate's `else`/dead-when-off arm; `lint_hal` exit 0 (1032 files); build green (link OK). RESIDUAL: the `--csv` A/B empirical confirm is the operator-rig step (the code-proof is exact). | mitigated (L2 + L3) |
| **RFP-FR2** | freeze (low risk) | L3's separate-device WGC copy hands pickup a torn/partial staged frame (copy not complete when the slot is marked captured) → a black/garbage real enters `freshage`. | **AS BUILT** (STAGE-121 `--copy-device`): the capture-ready gate is UNCHANGED — the C-thread Maps the newest ring slot with `D3D11_MAP_FLAG_DO_NOT_WAIT` (`main.cpp:5290`/`:5292`, now on `cap_ctx` = the 2nd device when armed); a copy still in flight ⇒ `Map != S_OK` ⇒ it falls to the prior valid slot (`use_cnt=w-1`, `stat_mapfb`) or `mapmiss`+`Sleep(1)`+`continue` — and **`c_slots[s].t_cap_ms` is set at `:5262` only on the success path AFTER the Map+memcpy of the COMPLETED copy**. So a torn/partial copy never reaches `t_cap_ms` → never enters `freshage`. The completion edge is made explicit by REUSING the STAGE-113 `--copy-fence` machinery created on the SAME 2nd device (`cap_dev`/`cap_ctx`, `:4190`/`:4193` probe; the `copyFence`/`ctx4`/`copyEvt` wait `:5272-5278`) — `--copy-device` is best paired with `--copy-fence`. Gated `cfg.copy_device`; a 2nd-device create-fail FORCES it off (`:4135` → `cap_dev=d.dev`). | **VERIFIED FIRST-HAND (implementer 2026-06-24, code-analysis):** the `DO_NOT_WAIT` Map → fall-through → `t_cap_ms`-only-on-success chain read first-hand (`:5290`/`:5296`/`:5262`); the same-device fence routing (probe on `cap_dev`/`cap_ctx`, `:4190`/`:4193`) and the C-thread-joined-before-`cctx`-release lifetime (`thr_c.join()` `:9353` precedes the teardown `:9378+`) read first-hand. **PENDING the operator rig:** the 30 s BF6 no-black/garbage run + the `--latency-trace` `copy` drop (likely needs the 4K-contended regime where the copy is ~5.6 ms; at HSR 1080p the copy is sub-ms — same caveat as L2). **SUPERVISOR RE-VERIFIED 2026-06-24:** build green (re-run), the alias gate (`cap_dev=d.dev` off, create-fail forces off `:4135`) + the teardown lifetime (`thr_c.join()` `:9353` precedes the `cdev` release `:9389`) read first-hand; 2 clean 16 s HSR runs (no crash). The HSR A/B measured the EFFECT **NULL** (`copy:` OFF≈ON ~3.4 ms — POLLING-dominated at unsaturated 1080p, NOT the copy execution; the "sub-ms" framing was imprecise, see Residual) → a 4K-contended regime, unmeasured-effective. | mitigated (supervisor; rig-effect pending) |

## Residual / accepted

**L2 `--fwd-prestage` (built + verified 2026-06-24, supervisor pass) — two documented residual follow-ups (NOT
crash-blockers; the lever is correct-by-analysis + clean-runs + default-off byte-identical):**
- **The Vulkan validation-layer sync-check is PENDING an SDK environment.** RFP-CR2's mitigation (the same-queue
  cross-submission barrier) is verified by first-hand code-analysis + 2 clean HSR runs, NOT by the gold-standard
  armed validation layer (absent here — empty stderr, no SDK). Re-run with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`
  + sync-validation on an SDK box for the deterministic confirmation.
- **The EFFECT (the `build`/`freshage` drop) is NULL at HSR 1080p and awaits a BF6-4K-contended run.** Measured
  first-hand: HSR 1080p `build` OFF ~8.6-9.0 ms ≈ ON ~9.2-9.4 ms (the ~11 ms B-ingest copy L2 targets is sub-ms
  there; `build` is already compute-bound). L2's regime is the 4K-contended B-queue (the triad's BF6-4K
  measurement). So L2 is committed **default-off as a 4K-contended lever, NOT an HSR-heaviness lever** — the HSR
  felt heaviness is the compute-bound `freshage` + structural FG latency, a different lever.

**L3 `--copy-device` (built + supervisor-verified 2026-06-24) — the SAME shape as L2 (correct-by-analysis +
clean-runs + default-off byte-identical; a 4K-contended lever, NOT an HSR-heaviness lever):**
- **The EFFECT is NULL at HSR 1080p (supervisor A/B, first-hand).** `--latency-trace` `copy:` OFF ~3.2-3.6 ms ≈
  ON ~3.2-3.4 ms; `freshage` OFF ~14.9 ≈ ON ~14.5 — no drop. The `lt_copy_us` (`tcap − ring_submit`) at the
  UNSATURATED 1080p (4090 ~50%) is dominated by the C-thread Map-retry POLLING (`mapmiss:65/s` + `Sleep(1)`), not
  by the copy EXECUTION (already fast) nor d.dev-queue contention — so decoupling the copy to a 2nd device drops
  nothing. (The implementer's "copy is sub-ms at 1080p" framing was imprecise: the copy *execution* is low, but
  the *lt_copy_us latency* is ~3.4 ms of polling, and L3 touches neither.) L3's regime is the 4K-CONTENDED case
  (the copy ~5.6 ms executing behind the saturated game queue); its effect awaits a BF6-4K run.
- **The 30 s BF6 no-black/garbage (RFP-FR2) + the validation-layer check are PENDING the rig / SDK** (same as L2).
  The crash-safety (the UNCHANGED capture-ready gate + the C-thread-joined-before-`cdev`-release teardown
  `:9353`→`:9389`) is verified by supervisor code-analysis + 2 clean 16 s HSR runs (no crash).

To be filled at each pre-commit gate. Any risk that ends `accepted` records here the rationale + the residual +
who accepted it. The KNOWN accepted residual
inherited from L0 (NOT re-opened here): the `--rfp` **content sawtooth** (fresh real → stale interp) — an
opt-in visual effect, the cost of the real-tick latency win (see [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md)
RFR-UNIT); L1 widening makes it MORE frequent → eye-validated tolerable is the operator gate, not a register row.
