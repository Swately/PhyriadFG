# UPLOAD_OFFLOAD_MASTER_PLAN — STAGE-106 `--upload-xfer`

> **Diátaxis type:** Explanation + plan (a design master-plan, not a tutorial).
> **Status buckets used:** `measured` (a first-hand number, or one the operator
> verified), `designed` (specified, not yet built), `assumed` (a model/estimate,
> flagged). Nothing here is `shipping` — none of STAGE-106 is built yet.
> **Scope:** move the per-pair WAP **upload** (`wap_upload`) off device A's
> contended **graphics** queue (`A.q`) onto a dedicated **transfer / async-compute**
> queue (`A.qT`, the 4090's DMA engine), so the present thread stops stalling ~10 ms
> per pair under GPU saturation. Opt-in, default-off, byte-identical-off. Sibling:
> `UPLOAD_OFFLOAD_IMPLEMENTATION_STRATEGIES.md`.
> **Verification note:** every `file:line` below was confirmed first-hand against the
> working tree (incl. the uncommitted STAGE-104/105) unless tagged `[wf]` — those come
> from the `fg-upload-offload-design` workflow (run `wf_df39e50e`, vulkaninfo on the
> rig) and the implementer MUST re-confirm them (the `--upload-xfer` startup path will
> log the enumerated families, closing the gap).

---

## §0 — The honest verdict, up front

**The per-pair upload — not the warp — is the saturated-GPU bottleneck, and it is on
the wrong engine.** This is measured, not assumed:

- Under BF6 combat with the 4090 pegged at **98.8 %** (`measured`, NVML, this session),
  the present loop delivers only ~110–120 fps at a **1.1–1.4× multiplier** while LSFG,
  uncapped on the same saturated GPU, delivers a comfortable 240.
- `--wsub` sub-timing (`measured`, this session) splits the per-tick cost:
  `up ≈ 10 ms`, `gpu (warp) ≈ 3–4 ms`, `prs (present) ≈ 0.7 ms`, `rec ≈ 0.03 ms`.
  **The upload dominates** and stalls the present thread ~45 % of wall-time.
- `wap_upload` (`apps/render_assistant/src/main.cpp:6027-6106`) records `vkCmdCopyBufferToImage`
  of two full frames (`wapPrevA`+`wapCurA`, 1920×1080 BGRA8 ≈ 8 MB each) + the iGPU
  contour field (≈ 8 MB, when `--bg-snap`) + ~9 small grids, submits to the **graphics**
  queue `A.q`, then **host-blocks**: `vkQueueSubmit(A.q,…,fBridge); vkWaitForFences(…,UINT64_MAX);`
  (`main.cpp:6104-6105`, `measured`). On the graphics engine these copies serialize with
  the warp and contend with the game's render; the host wait freezes the present thread.
- `--async-present` (STAGE-102) decoupled only the **warp**, never this upload — which is
  why the async path (`--fdrop`) still capped at ~119 fps (`measured`).

**The fix is therefore not "deepen the warp pipeline" (the warp is only 3–4 ms) and not
"cap the game" (rejected by the operator: it breaks ease-of-consumption and is a mediocre
implementation — both violate `CANON.md`).** The fix is to move the upload's ~10 ms onto
the 4090's **dedicated transfer/copy engine**, which runs in parallel to the graphics
engine. Freed of the upload stall, the present loop ticks faster, **asserts** more frames,
and the game yields its GPU share by contention — the documented LSFG behaviour
("makes space to enter" at 99 %, no user cap). This is the make-space lever that respects
the no-cap / no-injection / external dogmas.

The hardware premise **holds**: the 4090 exposes a `COMPUTE|TRANSFER` family with no
graphics bit (8 queues), distinct from the graphics family — a real parallel DMA/async-compute
engine. `[wf]` (vulkaninfo, run `wf_df39e50e`; re-confirmed by the startup enumeration log
the `--upload-xfer` path adds).

---

## §1 — Motivation (why this is on the table)

The arc's `MAIN OBJECTIVE` is to **beat LSFG in every approach the architecture allows**,
not to retreat behind a wall. The operator's observation is the anchor: LSFG, uncapped,
enters at 99 % GPU and generates comfortably; PhyriadFG collapses to ~1.2× because it is
*polite* under contention — its present thread blocks ~10 ms/pair on a synchronous upload
that sits on the contended graphics engine. Removing that stall is the single
highest-leverage change measured this session, and it is fully compatible with the
external-capture / no-injection / no-cap constraints (`FG_SATURATION_PRIOR_ART` §1, §4).

The make-space cap test (`measured`, this session) confirmed the mechanism from the other
direction: capping BF6 to 30 fps freed the 4090 to 45 % util and the FG immediately hit
**234 fps at a 1.22 ms true slice** — proving the headroom-bound throughput model and that
the uncapped 4.57 ms "slice" was ~73 % contention/fence-wait, not warp work. STAGE-106
recovers that headroom **without** a cap, by taking the upload off the contended engine.

---

## §2 — The walls (measured, and why they bind)

1. **The upload is on the graphics engine.** `wap_upload` submits to `A.q` (`main.cpp:6104`),
   the same queue the warp and present use, the same engine the game saturates. `measured`.
2. **The upload host-blocks.** `vkWaitForFences(…,UINT64_MAX)` at `main.cpp:6105` freezes the
   present thread until the contended copies finish (~10 ms). `measured`.
3. **Consumer GPUs do not preempt** (`FG_SATURATION_PRIOR_ART` F1/N1, `[V1]`): we cannot make
   the warp/upload jump the game's queue. The only lever is to use a **different engine**
   (the DMA/copy engine) that runs in parallel, and to **assert** throughput so the
   scheduler rebalances and the game yields. `measured` (the negative) + `designed` (the lever).
4. **No cap, no injection** (operator constraint + the external dogma): we may not limit the
   game's fps with a limiter or an injected hook. The yield must come from honest contention.

---

## §3 — Architecture: what "upload on the transfer engine" requires

The upload writes images (`wapPrevA` … `wapDISBA`) that the warp, on `A.q`, reads. Moving the
write to a second queue `A.qT` introduces a **cross-queue, cross-family** hazard. Three
elements make it crash-class-safe (the prior `A.q` device-lost was exactly a cross-family /
cross-thread queue hazard; this design must not reintroduce it):

- **A.qT = a `COMPUTE|TRANSFER`-non-graphics family queue** (`[wf]` fam2; transfer-only fam1 as
  fallback; `UINT32_MAX` → feature force-off → byte-identical). `A.q` stays **present-exclusive**
  (the STAGE-103 partition holds); the upload moves to the NEW `A.qT`, never to `A.q2`.
- **CONCURRENT sharing on the uploaded images** (`VK_SHARING_MODE_CONCURRENT`, families
  `{qfam, qfamT}`) → **no queue-family ownership transfer (QFOT)**. The buffer precedent is
  `hbuf_import` (`main.cpp:1784`, `measured`). This is the dogma-faithful way to avoid the
  family-trap that crashed `A.q`. A latent bug must be fixed first: `img_barrier`
  (`main.cpp:1803`) zero-inits `src/dstQueueFamilyIndex` to **0** (a real family), not
  `VK_QUEUE_FAMILY_IGNORED` — benign under EXCLUSIVE, a malformed QFOT under CONCURRENT
  (`measured`, verified first-hand; the workflow's catch).
- **Two timeline semaphores** (`VK_SEMAPHORE_TYPE_TIMELINE`) order the two queues without a CPU
  mutex (lock-free dogma intact — GPU sync, not host locking):
  - `semUpTL` (upload→warp): the warp's `A.q` submit waits for the upload's `A.qT` signal so it
    never samples a half-written image.
  - `semWarpTL` (warp→upload, the **WAR back-edge**): the next upload waits for the prior warp to
    finish *reading* the in-place images before overwriting them.
  **A binary semaphore is WRONG here** (`designed`, the workflow's unanimous catch): the
  `--fdrop`/async drop paths make `record_this_tick` false on some ticks (`main.cpp:6179`,
  `measured`) → a warp submit does not happen every tick → a 1:1 binary signal/wait desyncs.
  A timeline (monotonic counter, wait-on-recorded-value) is balance-free.
- **Ping-pong upload command buffers + a reset-in-flight guard** so a new upload never resets a
  command buffer still executing on `A.qT` (the STAGE-102 bug class).

### Staged order (revised from the first instinct)

The intuition "do the volume-reduction ring first" is **refuted** (`designed`, the workflow):
while the upload still host-blocks on `A.q`, halving the copied bytes (16 MB→8 MB) does **not**
touch the bottleneck (the stall is the *fence wait*, not the byte count) and introduces the
hardest hazards (per-pair descriptor rebind racing an in-flight warp, A-VRAM lap-safety). So:

- **Stage 1 — `--upload-xfer`** (queue-move + async, indivisible): the bottleneck lever. A
  transfer-queue submit with no host wait *is* async — (1) and (3) of the original triad are one
  change. This is the smaller, bisectable, byte-identical-off step that delivers the measured win.
- **Stage 2 — `--upload-ring`** (requires `--upload-xfer`): the volume-reduction A-image ring,
  deferred behind a separate default-off flag, landed only if Stage 1's measured win justifies the
  descriptor-rebind surface. The volume win (each frame copied once: `cur` of pair N == `prev` of
  pair N+1) is real but secondary to the engine move.

---

## §4 — Design contract (the principles the build MUST hold)

- **D1 — Byte-identical-off.** `--upload-xfer` default-off → `wap_upload` stays on `A.q` with the
  sync wait exactly as today; no `A.qT`, no semaphores, no CONCURRENT images allocated/used. The
  runtime gate `xfer_on = cfg.upload_xfer && A.qT != VK_NULL_HANDLE` forces off if no transfer
  family exists (portability; the bridge-alloc-fail fallback is the precedent).
- **D2 — Lock-free / zero-alloc hot path.** Cross-queue ordering is GPU semaphores, never a CPU
  mutex. Semaphores, command buffers, and the transfer pool are created ONCE at init.
- **D3 — `A.q` stays present-exclusive.** The STAGE-103 partition is preserved; the upload goes to
  the NEW `A.qT`, not `A.q` and not `A.q2`. No new thread submits to `A.q`.
- **D4 — No QFOT, no family-trap.** CONCURRENT sharing on the uploaded images; the `img_barrier`
  family-index bug fixed to `IGNORED` for those images.
- **D5 — No reset-in-flight.** Ping-pong upload command buffers + a `vkGetFenceStatus`/timeline
  guard before reuse.
- **D6 — Honest measurement.** Success = the `--wsub` `up` segment drops AND `present_fps` rises
  under saturation, with the watchdog clean. A null/negative result is reported as such.

---

## §5 — Phases and validation gates

| Phase | Deliverable | Gate |
|---|---|---|
| **P1** | `A.qT` + `poolT` + two timeline semaphores in `vdev_create` (gated on `want_xfer_q`); startup logs the enumerated families | build-green; off path byte-identical; the log confirms the `[wf]` family premise first-hand |
| **P2** | CONCURRENT images (under `xfer_on`) + the `img_barrier` `IGNORED` fix | build-green; off path byte-identical; validation-layer clean (no QFOT warnings) |
| **P3** | upload→`A.qT` async submit + the two-timeline sync + ping-pong buffers | 30 s BF6 clean, watchdog 0 stalls, NO device-lost |
| **P4** | measure under saturation | `--wsub` `up` ↓, `present_fps` ↑ vs the STAGE-105 baseline; operator eye-pass |
| **P5 (deferred)** | `--upload-ring` volume reduction | only if P4's win justifies the descriptor-rebind surface |

Each phase builds green and is byte-identical with `--upload-xfer` off. The crash-config
discipline from the arc applies: probe incrementally, never the all-on-4090 combo that crashed.

---

## §6 — Recommendation

Build **Stage 1 (`--upload-xfer`)** incrementally P1→P4, default-off and byte-identical-off,
with the two-timeline sync and the CONCURRENT/`img_barrier` fixes as hard prerequisites. It is
the measured bottleneck lever and the make-space mechanism that honours the no-cap dogma. Defer
**Stage 2 (`--upload-ring`)** until P4 proves the engine move pays — the ring is byte-volume, not
the bottleneck, and carries the higher hazard. The implementation contract is the sibling
`UPLOAD_OFFLOAD_IMPLEMENTATION_STRATEGIES.md`.
