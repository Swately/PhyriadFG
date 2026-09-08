# MINIMAL_FG_RISK_REGISTER — the clean minimal-FG core (capture→flow→warp→blend→paced-present on a render-graph seam)

> **Diátaxis type:** Risk register (the Tier-2 document of [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)).
> **Status:** `designed` — **no code is built under this plan; every risk starts `open`** and is tagged
> **`DESIGN — not yet built`**. A risk reaches `mitigated` ONLY when its code is in **and** its
> Verification was run first-hand; `accepted` records a residual + the rationale + who accepted it.
> **Linked set (Tier-2 triad):** master plan [`MINIMAL_FG_MASTER_PLAN.md`](../research/MINIMAL_FG_MASTER_PLAN.md) ·
> strategies [`MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md`](MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md) ·
> this register. Cited foundation: [`MINIMAL_FG_SOTA_DOSSIER.md`](../research/MINIMAL_FG_SOTA_DOSSIER.md).
> **Component IDs** (canonical, from master-plan §0.1): MC-1 capture · MC-2 flow · MC-3 warp · MC-4 blend ·
> MC-5 present · SG the render-graph seam. Strategy edits cite the MR-ID they mitigate.
> **Provenance:** the 2026-06-26 SOTA barrido + the operator's minimalism diagnosis; the parent
> reconciliation is verified first-hand (the parent register files exist — confirmed this authoring pass).

## The gate (PLAN_TIER_PROTOCOL §3)

1. **No commit of minimal-core code while any MR is `open`** (each → `mitigated` or `accepted`).
2. **The OPERATOR-EYE gate (MR-6/MR-8):** the minimal core MUST NOT replace the default until the operator's
   first-hand eye confirms minimal-NET ≥ complex-NET. Numbers do not see the vibration — the operator's
   hand-eye sensation is the arbiter (the load-bearing lesson of this arc). Until then the minimal core is
   opt-in / parallel and the full pipeline remains the runnable reference.
3. **Byte-identical-off (MR-4)** is a release gate: a CSV byte-diff proving the full-pipeline default is
   unchanged + `lint_hal` clean, before any default flip toward the minimal core.
4. **No-game-cap (MR-5)** is a dogma gate: zero frame-limit / affinity / priority call against the game PID.
5. **Build-state caveat:** the Verification cells are **pending build / pending rig** — specified, not run —
   EXCEPT MR-3's derivation half (built + verified first-hand, see below).

**Commit scoping (2026-06-26 — the SG seam engine):** the GPU-free SG seam engine
(`apps/minimal_fg/include/minimal_fg/seam_graph.hpp` + `apps/minimal_fg/test/test_seam_graph.cpp`) is
committed as **infrastructure**. It engages ONLY MR-3's barrier-derivation correctness — it is a pure
compile-time deriver with a GPU-free golden + adversarial test (built green MSVC + Vulkan 1.4.350; 122
checks pass; the 2-phase `minimal-fg-sg-engine` workflow's adversarial pass found a CRITICAL silent-RAW
[decoupled stage/access visibility unions] + an over-sync [redundant WAW on RMW] + a layout-after-read
silent-wrong-barrier — all three FIXED + regression-guarded by added tests). It is **NOT wired into any FG
runtime** (no capture, no present, no GPU-shared-with-a-game, no async/cross-queue), so MR-1/MR-2/MR-4/MR-5/
MR-6/MR-7/MR-8 — and MR-3's runtime `sync-validation clean` sub-gate — are **not engaged by this commit and
remain `open`**, to be discharged as the engine is integrated into the capture→present pipeline (later
phases). No risk-bearing FG runtime behaviour ships here.

---

## Risk table (MR-1 … MR-8, all `open`)

| ID | Class | Failure mode (what goes wrong) | Mitigation — AS CODE (`DESIGN — not yet built`) | Verification (pending build / rig) | Status |
|---|---|---|---|---|---|
| **MR-1** | crash / device-loss | The new **MC-5 present-pacing + DROP** path: use-after-reset of the interpolated-frame slot, or device-loss through the non-blocking fence-status polls under the drop decision. | **Reconcile, do not re-derive** the discharged async machinery (parents below): dedicated present slot split + back-slot guard; `vk_live`-wrap EVERY non-blocking poll → `g_quit` on `VK_ERROR_DEVICE_LOST`; provision the slot at INIT, never alloc/free on the hot path; the drop decision is a P-thread-local scalar compare (no shared lock). | `VK_LAYER_KHRONOS_validation` clean across a 30 s testbench soak; force a TDR in the drop-poll → clean `g_quit` exit (not a fence hang); confirm zero hot-path allocation. | `open` |
| **MR-2** | concurrency / cross-queue | Async-compute split: MC-2/3/4 on the compute queue, MC-5 present on the graphics queue, joined by a timeline semaphore. (a) two submitters on one `VkQueue` (external-sync violation), (b) missing/wrong queue-family **ownership transfer** of the warp-output image → corruption / device-loss, (c) present samples the warp output **before** the semaphore signal. | **First cut = single universal queue** (graphics+compute capable) with `VkEvent` intra-queue sync → NO ownership transfer, NO cross-queue race (isolates SG correctness from async complexity). Async (separate compute queue) is a **measured follow-up**: exactly one submitter per `VkQueue`; explicit ownership transfer on the warp-output image; present (graphics) waits on the timeline value the blend signals. | Sync-validation clean; trace confirms one submitter per `VkQueue`; the present's first read of the warp output is gated by the semaphore wait; the async variant is added ONLY after the universal-queue core is proven. | `open` |
| **MR-3** | concurrency / correctness (the SG seam) | The render-graph derives barriers from declared reads/writes — a **wrong or missing declaration** → a data hazard (read-before-write, or a missing cache flush: execution-order ≠ memory-order) that validation does not always catch and is non-deterministic. | First-cut SG = a **"poor-man's render graph"**: explicit resource handles + declared per-pass access + linear topo-sort + a derived barrier on every write→read edge with **PRECISE** masks (both stage AND access scopes — flush+invalidate, not just execution). A debug mode **dumps the derived barrier list** for a hand-audit. Start on the single universal queue (MR-2) to prove the graph before adding async. | The derived-barrier dump matches a hand-audit for the MC-1..MC-5 core; sync-validation clean; a **golden-frame determinism test** (identical inputs → byte-identical output across N runs) passes. | `mitigated` (derivation; runtime sync-val pending) |
| **MR-4** | dogma / byte-identical-off | The minimal core is a NEW path; while it coexists with the full `main.cpp` pipeline it could change the default's scheduling/output even when not selected. | The minimal core is a **separate path** (a sibling app OR a default-off flag); the full pipeline's code is **untouched**; core-not-selected → zero new code executes (assertable). New flags in `parse_extra` (not the C1061 chain). | CSV byte-diff of the full-pipeline **default** run vs the prior baseline binary (freshage / `frz` / present-count identical); symbol/trace inertness; `lint_hal` clean (default `seq_cst`, no raw `memory_order_*` in `apps/`). | `open` |
| **MR-5** | dogma / NO-GAME-CAP | The core shares ONE GPU with the game (MC-1 captures it). A present that back-pressures, or any frame-limit / affinity / priority call aimed at the game, is an implicit cap. | The core **only** captures + presents its OWN window; it issues **zero** `set_process_affinity`/`set_process_priority`/frame-limit against the game PID (the THREAD_PROTECTION precedent — verified first-hand zero such calls); MC-5 **DROPS** the interpolated frame under load rather than back-pressuring the source. | Grep proves zero foreign-PID affinity/priority/limit calls on the core path; the game's own frametime under the core vs off (the **operator's** in-situ measurement, deferrable like THREAD_PROTECTION R2). | `open` |
| **MR-6** | measurement validity / net-gain gate | The minimal-vs-full A/B is unfair (different source/scene/scorer), OR the re-layer gate **silently drops** a layer that fixes a real measured artifact (bg-snap killed object-fragments; deficit-tier+ring killed freezes 242→0). | A/B uses the **SAME** TB-C12 source + TB-C9 cadence scorer + the input→photon latency tap, same scene. Each re-layer decision **records BOTH** the artifact it fixes AND its latency/cadence cost; a dropped layer is logged **LOUD** with the artifact it was fixing (no silent truncation — the §RR10 no-silent-cap precedent). | The A/B harness self-test (same input → same score); every re-layer decision has a recorded artifact + cost; the operator's eye is the final arbiter on each. | `open` |
| **MR-7** | external-capture / present surface | MC-5 presents from our own window; the external-capture overlay can break the game's Independent Flip (+1 frame) or fail to get an MPO hardware plane — a latency cost outside our control. | **Reuse the existing validated in-process DComp own-window present** (no new cross-process present is attempted — cross-process DComp-into-the-game is OS-blocked anyway). The drop-model bounds the worst case. The IFlip interaction is a **documented external ceiling**, not a regression we introduce. | The present path is byte-identical to the existing validated own-window DComp path; no new cross-process present API is called; the IFlip cost is characterized, not silently absorbed. | `open` |
| **MR-8** | scope / minimal-NET regression | The arc ships a core that is SIMPLER but **perceptually WORSE** (a net-positive layer got dropped, or the seam rebuild introduced a fresh artifact) — "fewer layers" mistaken for "better". | The **P4/P5 gate REQUIRES minimal-NET ≥ complex-NET on the operator's eye** before the minimal core may replace the default; until then it is opt-in/parallel and the full pipeline is the runnable reference. The success condition is **measured + eye-confirmed**, never "fewer layers" as a slogan. | The recorded P4/P5 A/B + the operator's verdict; the default does NOT flip until the gate passes. | `open` |

## Residual / accepted (none yet)

To be filled at each commit gate. As of design time **all eight are `open`** (no code built). The MR-5
game-frametime evidence and the MR-6/MR-8 eye gate are expected to carry the same operator-deferred /
operator-arbiter residuals the THREAD_PROTECTION close did — recorded here when reached.

---

## Parent reconciliation (link, do not re-derive)

This register is scoped to the NEW minimal-core seam + present-pacing. The async crash-class and device-loss
are discharged in the parents below (files confirmed present this authoring pass); MR-1/MR-2 reconcile rather
than re-enumerate.

| Parent register | What it already discharges (reused here) |
|---|---|
| [`FG_SATURATION_STABILITY_RISK_REGISTER.md`](FG_SATURATION_STABILITY_RISK_REGISTER.md) | async-present use-after-reset (slot split + back-slot guard), device-loss-through-poll (`vk_live`), torn-present (keyed mutex), byte-identical-off, the operator-gated default-flip pattern. |
| [`DEVICE_LOST_RECOVERY_RISK_REGISTER.md`](../research/DEVICE_LOST_RECOVERY_RISK_REGISTER.md) | `vk_live` graceful-exit on `VK_ERROR_DEVICE_LOST` → `g_quit` (the device-loss backstop wrapping every submit/wait + async poll). |
| [`REAL_FAST_PATH_RISK_REGISTER.md`](REAL_FAST_PATH_RISK_REGISTER.md) | the dedicated-slot present path (never present while a warp is in flight) — the canonical async use-after-reset mitigation. |
| [`THREAD_PROTECTION_RISK_REGISTER.md`](THREAD_PROTECTION_RISK_REGISTER.md) | the no-game-cap discipline (zero foreign-PID affinity/priority calls, verified first-hand) — MR-5 reuses it; closed at commit `45fb505`. |

> **Honest gap:** the parent registers' internal commit hashes are quoted from those registers, not re-run via
> `git` in this authoring pass; the implementer re-confirms first-hand when closing MR-1 (verify-before-claim).

## Registration

This register + its two companions + the dossier are added to
[`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) in the same change (handled by the
supervisor at the P0 close).
