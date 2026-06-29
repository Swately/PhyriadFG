# UPLOAD_OFFLOAD_IMPLEMENTATION_STRATEGIES — STAGE-106 `--upload-xfer`

> **Diátaxis type:** How-to (implementation strategies), sibling to
> `UPLOAD_OFFLOAD_MASTER_PLAN.md`. **Status:** `designed` — none of this is built.
> **Verification note:** `file:line` confirmed first-hand against the working tree
> (incl. uncommitted STAGE-104/105) unless tagged `[wf]` (from run `wf_df39e50e`);
> the implementer MUST re-confirm `[wf]` sites — line numbers drift as edits land,
> and the `--upload-xfer` startup enumeration log is the first-hand close on the
> queue-family premise.

## How to read this

Each section is a self-contained strategy with its concrete edit site, the GPU-sync
contract, and the byte-identical-off proof. **S1–S6 are Stage 1 (`--upload-xfer`)** and
ship together (the queue move is indivisible from going async). **S7 is the deferred
Stage 2 (`--upload-ring`).** Build order at the end.

The non-negotiable invariants (from the master-plan D-contract): GPU semaphores not CPU
mutexes; `A.q` stays present-exclusive; CONCURRENT sharing (no QFOT); no reset-in-flight;
byte-identical when `--upload-xfer` is off.

---

## S1 — The transfer / async-compute queue `A.qT` (`vdev_create`)

The 4090 exposes a `COMPUTE|TRANSFER`-non-graphics family (8 queues; `[wf]` fam2) plus a
transfer-only family (`[wf]` fam1) — a real parallel DMA/copy engine. Device A is created
`prefer_same_family_q2=true` (`main.cpp:2849`, `measured`) → it has `A.q`/`A.q2` (same
graphics family, `q2mode=1`) and **no** transfer queue today.

- **`VDev` struct (`main.cpp:1679`, beside `q2`/`qfam2`/`pool2`):** add
  `VkQueue qT=VK_NULL_HANDLE; uint32_t qfamT=UINT32_MAX; VkCommandPool poolT=VK_NULL_HANDLE;
  VkSemaphore semUpTL=VK_NULL_HANDLE, semWarpTL=VK_NULL_HANDLE;`
- **`vdev_create` signature (`main.cpp:1686`):** add a trailing `bool want_xfer_q=false`.
- **Family search:** insert AFTER the `q2mode` decision (after `main.cpp:1726`, before the
  `qcis` fill at `1727`), **index-relative**, preferring `COMPUTE && !GRAPHICS` (the
  async-compute+transfer family, distinct from `qfam`/`qfam2` so it is a parallel engine and
  can run the in-upload median COMPUTE dispatch), falling back to transfer-only, else
  `UINT32_MAX`. Skip `i==d.qfam` and `i==qfam2`. `[wf]` for the exact predicate.
- **Queue create:** widen `prio[2]→[3]` and `qcis[2]→[3]` (`main.cpp:1727-1728`); append an
  index-relative `qcis[nqci]` for `qfamT` when found (`assert(nqci<3)` before the append —
  never a hardcoded slot). After `vkCreateDevice` (`main.cpp:1734-1738`):
  `vkGetDeviceQueue(d.dev,d.qfamT,0,&d.qT)`, create `poolT` (`RESET_COMMAND_BUFFER_BIT`,
  family `qfamT`), and create the two timeline semaphores (`VK_SEMAPHORE_TYPE_TIMELINE`,
  `initialValue=0`) — all gated on `d.qfamT!=UINT32_MAX`.
- **Timeline feature:** add `VkPhysicalDeviceTimelineSemaphoreFeatures{.timelineSemaphore=VK_TRUE}`
  to `dci.pNext` **only when `want_xfer_q`** (Vulkan 1.2 core; verify the driver exposes it at
  create). Off → `pNext` unchanged → byte-identical device.
- **`vdev_destroy` (`main.cpp:1747` `[wf]`):** destroy the semaphores, then `poolT`, before
  `pool`/`pool2`.
- **A-create call site (`main.cpp:2849`):** pass `want_xfer_q = cfg.upload_xfer` (parse the flag
  before the device create). **`B`/`G` pass `false`** → byte-identical for the assist GPUs.

Byte-identical-off: `want_xfer_q=false` → no extra family, no queue, no semaphores, no `pNext`
change → the device is created exactly as today.

## S2 — CONCURRENT images + the `img_barrier` QFOT fix

The uploaded images are created EXCLUSIVE today (`img_create`, `main.cpp:1750-1762`, default
`sharingMode` = `EXCLUSIVE`, `measured`). Writing them on `A.qT` and reading on `A.q` without a
QFOT requires CONCURRENT sharing.

- **`img_create` (`main.cpp:1750`):** add an optional `const uint32_t* concurrent_fams=nullptr,
  uint32_t n_fams=0`. When non-null set `ici.sharingMode=VK_SHARING_MODE_CONCURRENT;
  .queueFamilyIndexCount=n_fams; .pQueueFamilyIndices=concurrent_fams;` (mirrors `hbuf_import`
  `main.cpp:1784`, `measured`). Default args → EXCLUSIVE, byte-identical.
- **The wap input images** (`wapPrevA`,`wapCurA`,`wapMVA`,`wapMVTA`,`wapSADA`,`wapMVBA`,`wapC2A`,
  `wapDISA`,`wapDISBA`,`wapPERA`,`wapFIELDA`): pass `{qfam,qfamT}` **only when `xfer_on`**. Off →
  EXCLUSIVE. (CONCURRENT can cost compression on some HW; these are transient upload targets, so
  acceptable — note it, do not claim free.)
- **`img_barrier` (`main.cpp:1803`) — the latent bug (`measured`):** `VkImageMemoryBarrier b{}`
  leaves `src/dstQueueFamilyIndex=0`, a real family, not `VK_QUEUE_FAMILY_IGNORED`. Add explicit
  `b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;`. This is correct for BOTH
  EXCLUSIVE-single-family (no behaviour change today) and CONCURRENT (no malformed QFOT) → safe to
  fix unconditionally; do it as a tiny standalone first edit and build-green to confirm no regression.

## S3 — The two-timeline sync (the crux; cross-queue, no mutex)

Two timeline semaphores order `A.qT` (upload) and `A.q` (warp) without a CPU mutex:

- **`semUpTL` (upload→warp):** the upload's `A.qT` submit **signals** `semUpTL = U` (a monotonic
  per-upload counter); the warp's `A.q` submit **waits** `semUpTL >= U_last_uploaded` at
  `pWaitDstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|…COMPUTE_SHADER_BIT` (wherever the warp
  samples the images). → the warp never reads a half-written image.
- **`semWarpTL` (warp→upload, the WAR back-edge):** the warp's `A.q` submit **signals**
  `semWarpTL = W`; the next upload's `A.qT` submit **waits** `semWarpTL >= W_at_last_pair_advance`
  before overwriting the in-place images. → a new upload never clobbers images the prior warp is
  still sampling.
- **Why timeline, not binary (`designed`, the workflow's unanimous catch):** `record_this_tick`
  can be **false** (`main.cpp:6179`, under `--fdrop`/async drops, `measured`) → a warp submit does
  NOT happen every tick → a 1:1 binary signal/wait pair desyncs. Timeline semaphores carry a value;
  a wait on an already-passed value is a no-op, so the warp **waits on every recorded tick** against
  the last-uploaded value with no balance requirement. The counters (`U`,`W`) are P-thread-local
  monotonic — no shared mutable state, lock-free.
- Use `VkTimelineSemaphoreSubmitInfo` on both submits (`pWaitSemaphoreValues`/`pSignalSemaphoreValues`).

## S4 — Ping-pong upload command buffers + reset-in-flight guard

Today `wap_upload` records into the shared `cmdBridge` and the host wait at `main.cpp:6105`
guarantees it is idle before the next reset. Removing the host wait (S3) means a new upload could
reset a command buffer still on `A.qT`.

- Allocate **N≥2** upload command buffers from `poolT` at init (`cmdUpload[uslot]`), plus a fence or
  timeline value per slot. Before `vkResetCommandBuffer(cmdUpload[uslot])`, check the slot's prior
  completion (`vkGetFenceStatus` non-blocking, or `vkWaitSemaphores` with a 0-timeout poll on
  `semUpTL`); wait only if still in flight (rare). This is the STAGE-102 `bslot` ping-pong pattern
  (`main.cpp:6136-6185`, `measured`) applied to the upload.
- The median COMPUTE dispatch inside `wap_upload` (`main.cpp:6084-6102`) runs on `A.qT` — valid
  because `qfamT` has the COMPUTE bit (`[wf]` fam2). If only a transfer-only family is available
  (`[wf]` fam1, no COMPUTE), the median pass must stay on `A.q` (a fork): prefer fam2 to avoid it.

## S5 — Integration with `--async-present` (STAGE-102) and `--fdrop` (STAGE-105)

`--upload-xfer` **implies `--async-present`** (auto-enable at parse, mirroring `--fdrop`'s
auto-enable at `main.cpp:948`): the warp must be the non-blocking `bslot` path so the present
thread is not re-blocked elsewhere. The warp submit (`main.cpp:6429-6440`) adds the `semUpTL` wait +
the `semWarpTL` signal via `VkTimelineSemaphoreSubmitInfo`. On a `--fdrop` drop (`do_warp=false`,
`record_this_tick=false`) **no warp submit happens** — the timeline design (S3) handles this:
`semWarpTL` is not advanced that tick, the next upload waits on the last real warp value, and the
upload→warp edge is a no-op wait. Verify the drop path explicitly (it is the binary-semaphore
failure mode).

## S6 — Byte-identical-off discipline (D1, non-negotiable)

- The flag lives in `Cfg` beside `async_present` (`main.cpp:622`); the matcher is in `parse_extra`
  near `--async-present` (`main.cpp:932/948`), NOT the main else-if chain (MSVC C1061).
- Every new path is gated on `xfer_on = cfg.upload_xfer && A.qT!=VK_NULL_HANDLE`. Off → `img_create`
  default args (EXCLUSIVE), the upload submits to `A.q` with the `vkWaitForFences` exactly as today,
  no semaphores touched. The `img_barrier` `IGNORED` fix (S2) is the only unconditional change and is
  behaviour-preserving under EXCLUSIVE — prove it with a default-flags before/after run (fps + frz
  unchanged) as the first build-green checkpoint.
- lint_hal: no raw `memory_order_*` (default seq_cst). §9 signature stays the last line of any file.

## S7 — Measurement and validation

- **Primary:** under BF6 saturation (uncapped, 4090 ~99 %), `--wsub` `up` should fall sharply (the
  copies now overlap the graphics engine) and `present_fps` rise toward the headroom-bound rate; the
  STAGE-104 `fg_slice_ms_4090` / `fg_4090_occupancy` and the watchdog (0 stalls) corroborate.
- **Crash gate:** 30 s BF6 clean, no `nvlddmkm`/device-lost, validation layers (if enabled) clean of
  QFOT/sync warnings.
- **Honest framing:** if `up` does not fall (e.g. the driver serializes the "parallel" engine under
  this contention), report the null result; the engine premise is `[wf]`-measured, not guaranteed at
  runtime under a saturating game.

## S8 — Deferred Stage 2: `--upload-ring` (volume reduction)

`cur` of pair N == `prev` of pair N+1, so each captured frame is copied twice over its life. A ring
of A-images (copy each captured frame ONCE, the warp references prev/cur by ring index via
`vkUpdateDescriptorSets`) halves the ~16 MB frame traffic. **Deferred** because: (a) it does not
touch the bottleneck while the upload host-blocks (Stage 1 is the lever); (b) per-pair descriptor
rebind can race an in-flight warp, and the A-VRAM ring needs lap-safety vs the host capture ring —
the highest hazard surface in the design. Land only if Stage 1's P4 measurement shows the residual
byte cost still bounds throughput. Requires `--upload-xfer`.

---

## S9 — Crash-risk analysis (per behavior — the guardrail for the implementer)

The change is cross-queue GPU sync — the exact class that caused the prior `A.q` device-lost. Each
behavior below is a concrete way THIS plan could crash; the implementer must hold every mitigation
and the verifier must check each one first-hand. `device-lost` = `nvlddmkm`/`VK_ERROR_DEVICE_LOST`.

| # | Crash behavior (what goes wrong in code) | Mitigation (must hold) | How to verify |
|---|---|---|---|
| **CR1** | **Reset-in-flight**: `vkResetCommandBuffer(cmdUpload)` while it still executes on A.qT → device-lost (the STAGE-102 bug). | Ping-pong N≥2 upload cmd buffers; before reset, poll the slot's prior completion (`vkGetFenceStatus`/0-timeout `vkWaitSemaphores` on semUpTL); wait only if still in flight. | Read every reset site; confirm a guard precedes each. 30 s BF6 clean. |
| **CR2** | **Warp reads an incomplete upload**: the warp samples wapPrevA/etc. before the A.qT copy finished → garbage or device-lost. | The warp's A.q submit WAITS `semUpTL >= U_last_uploaded` (at the shader-read stage). | Confirm every warp submit carries the semUpTL wait with the correct value; validation layers clean. |
| **CR3** | **WAR clobber**: the next upload overwrites an in-place image the prior warp is still reading → corruption. | The upload's A.qT submit WAITS `semWarpTL >= W_at_last_advance` (the back-edge) before the copies. | Confirm the upload submit carries the semWarpTL wait; reason the value is the last warp that read these images. |
| **CR4** | **Binary-semaphore desync**: under `--fdrop`/async, `record_this_tick==false` (main.cpp:6179) → NO warp submit that tick → a 1:1 binary signal/wait goes unbalanced → deadlock or a permanently-behind wait. | TIMELINE semaphores only; wait on the LAST RECORDED value (a wait on an already-passed value is a no-op); never assume one-signal-per-one-wait. | Trace the drop path explicitly: a tick with no warp submit must not strand a signal. |
| **CR5** | **Timeline deadlock**: a submit waits a value that is never signaled (e.g. first iteration, or an off-by-one) → the GPU queue hangs → eventual TDR. | initialValue 0; first-upload waits `semWarpTL>=0` (immediately satisfied); the counters U/W are P-local monotonic; never wait `>current max`. | Reason each wait value is ≤ what will be signaled; first-iteration path checked. |
| **CR6** | **Malformed QFOT under CONCURRENT**: a barrier on a CONCURRENT image with `src/dstQueueFamilyIndex` ≠ IGNORED → undefined → device-lost. | S2-fix (img_barrier IGNORED, done) + VK_SHARING_MODE_CONCURRENT on every upload-written, warp-read image. | Confirm img_barrier IGNORED covers ALL barriers on these images; every such image is CONCURRENT. |
| **CR7** | **A.q present-exclusivity broken**: the upload accidentally submits to A.q (or A.q2) → re-introduces the cross-thread A.q race (the prior crash). | The upload submits ONLY to A.qT; P keeps A.q present-exclusive; F/C keep A.q2. | Grep every `vkQueueSubmit` near the upload; confirm A.qT, not A.q/A.q2. |
| **CR8** | **Compute on a transfer-only queue**: the in-upload median COMPUTE dispatch on a qfamT lacking COMPUTE → invalid → device-lost. | Prefer a COMPUTE-capable qfamT (fam2, 0xE here); if only transfer-only is available, the median pass stays on A.q. | Confirm qfamT has the COMPUTE bit before recording the dispatch on A.qT; else fork. |
| **CR9** | **CONCURRENT when xfer-off**: creating CONCURRENT / using A.qT when `cfg.upload_xfer` is off or qfamT==UINT32_MAX → not byte-identical / null-queue submit. | Gate EVERYTHING on `xfer_on = cfg.upload_xfer && A.qT!=VK_NULL_HANDLE`; off → EXCLUSIVE + the A.q sync upload exactly as today. | Default-flags before/after run: fps + frz unchanged; no A.qT path taken. |
| **CR10** | **New host-blocking on the present thread**: introducing a blocking `vkWaitForFences`/`vkWaitSemaphores` (non-zero timeout) on the present hot path → re-stalls (defeats the whole change). | No blocking host waits on the present hot path; only non-blocking polls (0-timeout / `vkGetFenceStatus`). The cross-queue ordering is GPU-side (semaphores). | Grep every wait the upload/warp path adds; confirm 0-timeout or GPU-only. |

A red on ANY of these is a STOP — do not stack a later phase on it.

## Recommended build order (composing the picks)

1. **S2 `img_barrier` `IGNORED` fix** alone → build-green, default-flags before/after unchanged
   (the safe standalone first edit).
2. **S1** `A.qT` + `poolT` + timelines in `vdev_create` (gated) + the startup family-enumeration log
   → build-green, off byte-identical, the log first-hand-confirms the `[wf]` family premise.
3. **S2** CONCURRENT images under `xfer_on`.
4. **S3 + S4 + S5** the two-timeline sync + ping-pong + the warp-submit wiring + `--async-present`
   auto-enable → the crash gate (30 s BF6 clean).
5. **S7** measure under saturation; operator eye-pass.
6. **S8** `--upload-ring` only if (5) justifies it.

Stop and report at any phase that fails its gate; do not stack an unverified phase on a red gate.
