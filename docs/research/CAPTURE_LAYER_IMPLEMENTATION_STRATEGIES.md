# CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES — the concrete HOW for the owned DDA capture layer

> **Diátaxis type:** how-to / explanation (implementation strategies — the per-strategy edit sites + the
> reuse map). **Normativity:** BCP 14 (MUST / SHOULD / MAY in their RFC 2119 senses). **Status:** `designed`
> — **no code is built under this plan.** Each code reference not first-hand-verified this authoring pass is
> marked **"to confirm during implementation"**; the DDA reuse sites + the `PresentSurface` API were read
> first-hand this pass (cited). **Plan tier:** **Tier-2** — the IMPLEMENTATION_STRATEGIES leg of the triad
> with [`CAPTURE_LAYER_MASTER_PLAN.md`](CAPTURE_LAYER_MASTER_PLAN.md) and
> [`CAPTURE_LAYER_RISK_REGISTER.md`](CAPTURE_LAYER_RISK_REGISTER.md) (the risks **CR-1 … CR-10**). Per
> [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §2.3 **every strategy cites the CR-ID(s) it
> mitigates.** Cited foundation: [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md) §6.1.
> **Component IDs** are the master-plan §0.2 spine (CL-1…CL-6).

---

## §0 — Component map (echo of MASTER_PLAN §0.2 — self-contained)

| ID | Component | Stage-1 scope |
|---|---|---|
| **CL-1** | DDA acquire core | D3D11 device on the output-owning adapter; `DuplicateOutput1`; `AcquireNextFrame(finite)`; `ReleaseFrame` |
| **CL-2** | Lock-free SPSC ingest ring | capture thread CopyResources into the existing staging ring; `seq_cst` publish; NO keyed-mutex |
| **CL-3** | GPU-resident zero-copy | *(CP3 follow-up)* shared NT-handle + timeline semaphore |
| **CL-4** | Composed-flip forcing | *(CP4, conditional)* topmost-overlay first; MPO-disable only if needed |
| **CL-5** | The `CaptureSurface` seam | *(CP5)* consolidate into the owned primitive (FR-RENDER-2) |
| **CL-6** | Backend select + WGC fallback | `--capture-backend dda\|wgc` (default `wgc`) |

The **CP1 deliverable** is CL-1 + CL-2 + CL-6 — the rate-fix build for the operator's BF6 run.

---

## CP1 — DDA host-staged lock-free backend (the rate fix)

> **Gate (master-plan §3):** builds green; validation-clean; **byte-identical-off** (WGC default unchanged);
> zero keyed-mutex on the path.

### CS-1 — CL-6: the backend flag, WGC stays the default  ·  *mitigates CR-6*

- Add `--capture-backend dda|wgc`, **default `wgc`**. Parse it in the same place `minimal_fg` parses
  `--capture`/`--no-overlay`/`--sg-dump` (the extra-flag site — confirm it is the non-C1061 path during
  implementation). A small `enum class CapBackend { Wgc, Dda }` on the heap `CapCtx` (or a local) selects
  the producer at init.
- **Byte-identical-off (CR-6):** when `Wgc` (the default), the DDA code is never entered → zero new code on
  the default path. The existing WGC `FrameArrived` producer + the consume side are **untouched**.
- **CL-6 fallback:** if DDA init fails (`UNSUPPORTED` / output-ownership, CR-4), log a NAMED reason and
  fall back to the WGC producer rather than running without capture.

### CS-2 — CL-1: the DDA acquire core on the output-owning adapter  ·  *mitigates CR-1, CR-4*

- **Reuse the proven pattern** from `apps/render_assistant/src/main.cpp` — `d3d_init`'s
  `IDXGIOutput1::DuplicateOutput` (**:1906-1927**, read first-hand) + the `stage28_wgc` adapter/output
  enumeration that matches `DXGI_OUTPUT_DESC.Monitor` to the target (**stage28_wgc/main.cpp:299-336**, read
  first-hand). **Correct it** to `IDXGIOutput5::DuplicateOutput1` with the game's native format list (avoid
  a forced BGRA conversion; `DuplicateOutput` is the fallback if `DuplicateOutput1`/`IDXGIOutput5` is
  unavailable).
- **CR-4 (multi-GPU ownership):** create the duplication on the adapter that **owns** the 240 Hz output
  (here the 4090 — `minimal_fg`'s `d3d_dev` is already on the primary; confirm it matches the output owner
  during implementation; if not, create a second D3D11 device on the owner and CopyResource intra-that-device).
  This keeps the producer single-device (CopyResource is intra-device — no cross-device on the hot path).
- **CR-1 (ACCESS_LOST/DEVICE_REMOVED):** wrap acquire in the recreate loop — on `DXGI_ERROR_ACCESS_LOST`
  release+`DuplicateOutput1` again under bounded backoff; on `DEVICE_REMOVED`/`NOT_FOUND` re-enumerate +
  rebind. Reuse the `render_assistant` E2-GRACE NAMED-reason pattern (main.cpp:3183-3185) for the
  init-failure surface.

### CS-3 — CL-2: feed the EXISTING lock-free ring from a DDA capture thread  ·  *mitigates CR-2, CR-3, CR-7*

- **A dedicated capture thread** (DDA is a pull API — `AcquireNextFrame` — unlike WGC's push callback) runs
  the acquire loop and feeds the **existing `CapCtx` staging ring** exactly as the WGC `FrameArrived` does
  (the loop at `minimal_fg` main.cpp ~**824-848**, read first-hand): check `running`; `AcquireNextFrame`;
  on a new frame (`fi.LastPresentTime≠0 || fi.AccumulatedFrames≠0`) `arrived.fetch_add(1)`; ring-full →
  `dropped.fetch_add(1)` + `ReleaseFrame` + continue; else `cctx->CopyResource(ring[w%RING_N], acquired)`,
  `ReleaseFrame`, `ring_write.fetch_add(1)` (seq_cst), `count.fetch_add(1)`. The consumer (present/OFP
  thread) is **unchanged** — it already Maps the ring slots into the host buffers.
- **CR-3 (THE crash class):** the producer does a **plain** `CopyResource` into the staging slot — **no
  `AcquireSync`/`ReleaseSync`, no `KEYEDMUTEX`** anywhere on this path (the staging textures are plain, like
  the WGC ring). The CPU `Map`/memcpy on the consumer side is the only cross-API barrier. This is the exact
  lock-free shape that fixed the BF6 deadlock; a `grep` of the capture path for `AcquireSync` MUST be empty.
- **CR-2 (timeout/liveness):** `AcquireNextFrame` uses a **finite** timeout (8-16 ms), NEVER INFINITE; on
  `WAIT_TIMEOUT` reuse the last frame, bump a `dda_timeouts` counter, continue. A sustained streak is the
  Independent-Flip runtime signal — log it (this is the CP2 diagnostic, and the CL-4 trigger if enabled).
- **CR-7 (no-game-cap):** the capture thread is OURS; it issues zero affinity/priority/limit against the
  game PID and never back-pressures the source (ring-full DROPS, like WGC). Any thread-priority change
  applies to our capture thread only.
- **CR-1 teardown:** mirror the WGC teardown ordering (`running=false` → drain → release dup/device) so a
  free-threaded/looping producer cannot touch a freed ring (the `minimal_fg` teardown at ~**1210-1217**).

### CS-4 — the instrument: make the operator's ONE BF6 run measure + validate  ·  *mitigates CR-10*

- Extend the existing `[mfg] fps in=.. out=.. | wgc_arrived=.. ring_dropped=..` line so the DDA backend
  reports the same fields (`arrived` = DDA frames delivered, `dropped` = ring-full, `in` = accepted/s) plus
  `dda_timeouts`/s. This is the CP2 measurement: **if `in` rises to ~90-100, DDA lifted the rate (gate
  passed); if it stays ~60 with a high timeout streak, it is the Independent-Flip bypass (→ CP4 / CL-4).**
- **CR-10:** the run characterizes the regime first-hand; no claim that "DDA fixed the rate" ships without
  this number. Optionally cross-check against `PresentMon`'s `PresentMode` column on BF6.

---

## CP3 — CL-3 GPU-resident zero-copy (the max-perf follow-up, after CP2 proves the rate)

> **Gate:** latency delta measured; kept only if it wins; no keyed-mutex; validation-clean 30 s soak;
> device-loss-safe.

### CS-5 — zero-copy ingest: shared NT handle + shared timeline semaphore  ·  *mitigates CR-3, CR-5*

- Replace the host-staged ring slots with **VK-imported shared textures**: `IDXGIResource1::CreateSharedHandle`
  (NT handle) → import ONCE into Vulkan as `VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT` (the
  reference-owning variant — **never `_KMT_BIT`**, which dangles; `allocationSize` is ignored on import,
  queried from the OS — dossier §6.1.2, confirm against the LOCAL Vulkan SDK headers during implementation).
  The OFP samples the imported image directly → **no host round-trip** (the master-plan §2 latency win).
- **CR-3 / CR-5 (sync):** producer signals a **shared TIMELINE semaphore** value N after its CopyResource;
  the consumer waits value N before sampling — `VK_KHR_external_semaphore` ↔ a shared D3D fence. **No keyed
  mutex.** A ring of N pre-imported surfaces (provisioned at INIT) keeps SPSC lock-free; zero hot-path
  allocation.
- **Measured-or-reverted:** time the input→sample latency vs the CP1 host-staged path; keep CL-3 only if it
  wins (efficiency mandate). The host-staged path remains the crash-safe fallback (CL-2).

---

## CP4 — CL-4 composed-flip forcing (conditional — only if CP2 shows Independent Flip caps DDA)

> **Gate:** lifts `in`; +1 frame characterized; opt-in default-off; global DWM-state save/restore verified.

### CS-6 — force composition without injecting  ·  *mitigates CR-8, CR-10*

- **Least-invasive first:** spawn a 1×1 click-through `WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST`
  composited window, reasserted via `SetWindowPos` every ~500 ms, so DWM MUST composite the desktop every
  frame → the bypassed frames become capturable. **No registry change** (dossier §6.1.1 — ForceComposedFlip).
- **MPO-disable only if the overlay alone fails:** `HKLM\SOFTWARE\Microsoft\Windows\Dwm` `OverlayTestMode`=5.
  **CR-8:** save the prior value first; restore on a normal exit AND on crash (a guard / `atexit` / SEH);
  opt-in, **default-off**. Characterize the +1 frame latency (PresentMon), do not silently absorb it (CR-10).
- Behind a flag (e.g. `--force-composed`), default-off — it taxes MAX-PERFORMANCE and is engaged only on
  measured necessity.

---

## CP5 — CL-5 consolidate into the owned `CaptureSurface` primitive (the FR)

> **Gate:** the FR-RENDER-2 procedure; pillar consolidation of the 8 scattered DDA copies.

### CS-7 — promote the proven backend into a `PresentSurface`-symmetric primitive  ·  *mitigates (architecture)*

- Once CP1-CP4 are proven app-local in `minimal_fg`, lift the backend into a `CaptureSurface` primitive that
  mirrors `PresentSurface` exactly (read first-hand this pass:
  `framework/render/present/include/phyriad/render/present/PresentSurface.hpp`): an opaque `Impl` (header
  free of `<windows.h>`/`<d3d11.h>`/`<dxgi.h>`), a POD `CaptureSurfaceDesc` (D-8), `std::expected` errors
  (D-7), cold `create()` / hot `acquire()` with zero steady-state allocation (D-2), a single-thread
  contract, and a non-Windows `Unavailable` stub (D-19). Sketch in `scratchpad/capture_layer_design.md`.
- **The FR procedure decides the pillar move** ([`FEATURE_REQUEST_PROCEDURE.md`](../canon/FEATURE_REQUEST_PROCEDURE.md)).
  Strong case (the symmetric twin of the shipped FR-RENDER-1; 8 in-tree copies to consolidate), but this
  plan does **not** pre-bake it — CL-5 stays a *candidate* until the gate passes. The first home is
  app-local.

---

## §Links — the triad + the dossier

- **Triad siblings (Tier-2 set, mutually linked per PLAN_TIER_PROTOCOL §2.3):** master plan
  [`CAPTURE_LAYER_MASTER_PLAN.md`](CAPTURE_LAYER_MASTER_PLAN.md) · risk register
  [`CAPTURE_LAYER_RISK_REGISTER.md`](CAPTURE_LAYER_RISK_REGISTER.md) (CR-1 … CR-10; no `open` CR at commit).
- **Cited foundation:** [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md) §6.1 (the capture-rate
  SOTA — the DWM-compositor ceiling, the Independent-Flip bypass, the zero-copy/timeline-semaphore path, the
  anti-cheat verdict; the URLs).
- **Reuse sites (first-hand this pass):** `apps/render_assistant/src/main.cpp` `DuplicateOutput`
  (:1906-1927) + the acquire loop (:5556-5581); `framework/render/vulkan/bench/stage28_wgc/main.cpp` (the
  WGC-vs-DD bench, adapter/output match :299-336, DD loop :349-374); `apps/minimal_fg/src/main.cpp` the
  WGC producer (~:824-848) + ring + teardown (~:1210-1217).
- **Process specs:** [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) ·
  [`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md) (the verifiable-reference mandate
  this doc obeys) · [`FEATURE_REQUEST_PROCEDURE.md`](../canon/FEATURE_REQUEST_PROCEDURE.md) (the CL-5 gate).
- **Parent arc / durable spine:** [`../ACTION_PLAN.md`](../ACTION_PLAN.md) (▷ NOW 2026-06-26d).

## §CR coverage map (strategy → risk; the Tier-2 traceability)

| Strategy | Phase | Mitigates |
|---|---|---|
| CS-1 — backend flag, WGC default | CP1 | **CR-6** |
| CS-2 — DDA acquire on the output-owner | CP1 | **CR-1, CR-4** |
| CS-3 — feed the lock-free ring (no keyed-mutex) | CP1 | **CR-2, CR-3, CR-7** |
| CS-4 — the measure+validate instrument | CP1 | **CR-10** |
| CS-5 — zero-copy NT-handle + timeline semaphore | CP3 | **CR-3, CR-5** |
| CS-6 — composed-flip forcing (overlay-first, restore) | CP4 | **CR-8, CR-10** |
| CS-7 — promote to `CaptureSurface` (FR-RENDER-2) | CP5 | architecture |

> **Coverage check:** CR-1 (CS-2) · CR-2 (CS-3) · CR-3 (CS-3, CS-5) · CR-4 (CS-2) · CR-5 (CS-5) ·
> CR-6 (CS-1) · CR-7 (CS-3) · CR-8 (CS-6) · CR-9 (CS-2/CS-3 defensive acquire) · CR-10 (CS-4, CS-6).
> All ten CR-IDs are cited by at least one strategy. (CR-9 protected-content is handled in the defensive
> acquire/skip of CS-2/CS-3 — the all-black/`UNSUPPORTED` skip.)
