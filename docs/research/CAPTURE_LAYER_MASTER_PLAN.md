# CAPTURE_LAYER_MASTER_PLAN — the owned, full-rate, non-injecting capture layer (`CaptureSurface`)

> **Diátaxis type:** Planning / explanation (a master plan, per
> [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)).
> **Status:** `designed` — no capture-layer code shipped under this plan yet. The SOTA grounding is
> `measured` (third-party, cited) in [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md)
> §6.1.1/§6.1.2; the in-tree DDA reuse sites are `verified` first-hand (cited below).
> **Plan tier:** **Tier-2** (risk-bearing — cross-API D3D11↔Vulkan interop, a crash/deadlock class with
> first-hand history [the BF6 keyed-mutex deadlock], concurrency [the capture↔present ring + the zero-copy
> shared-fence], and a dogma surface [byte-identical-off, no-game-cap, a global DWM-state mutation in the
> composed-flip fallback]). The required linked set: this master plan ·
> [`CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md`](CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md) ·
> [`CAPTURE_LAYER_RISK_REGISTER.md`](CAPTURE_LAYER_RISK_REGISTER.md). Cited foundation:
> [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md) §6.1. **No CR risk may be `open` at commit.**
> **Parent arc:** the minimal-FG arc ([`MINIMAL_FG_MASTER_PLAN.md`](MINIMAL_FG_MASTER_PLAN.md); this layer
> realizes its component **MC-1 capture**). The durable spine is [`../ACTION_PLAN.md`](../ACTION_PLAN.md)
> (▷ NOW 2026-06-26d).
> **Provenance:** the 2026-06-26 capture-rate SOTA barrido (workflow `wvfhoot01`: 6 angles, 3-vote
> adversarial verify, 23/26 confirmed, 0 refuted) + first-hand reads of the in-tree DDA path. Citations
> defer to the dossier — no number or API is asserted here the dossier does not cite first-hand.

## §0 — Objective + the operator's direction

**Principal objective (unchanged):** perfect PhyriadFG — match then surpass LSFG — within the four pillars
and the efficiency mandate. **Priority order (operator, this arc):** **MAXIMUM PERFORMANCE > scalability >
ease-of-consumption.**

**The operator's direction (2026-06-26):** the minimal-FG core ingests only ~50-62 fps of a BF6 that
delivers ~90-100 (it accepts 100% of what WGC `CreateForWindow` hands it — `ring_dropped=0` — so the
limiter is the capture *method*, not our pipeline). The instruction: *"tomamos del que ya habíamos hecho,
tomamos lo mejor, corregimos y construimos el que de verdad va a servir"* — lift the DDA capture already
proven in the full FG, correct it (lock-free, no keyed-mutex, GPU-resident where it earns it), and build
ONE owned capture primitive that actually serves. This is the **convergence model** applied to MC-1: the
full FG (`render_assistant`) is the donor of a proven piece; the minimal base is the architectural target.

## §0.1 — The honest SOTA correction (load-bearing — do not re-litigate)

The capture-rate SOTA (dossier §6.1.1) **corrected a tempting assumption**: WGC and DDA **share one
limiter — both capture from the DWM compositor**, so WGC→DDA is **not by itself** a rate lever. The real
cap is the **Independent/Direct-Flip DWM-bypass** (flip-model fullscreen sends frames straight to scanout,
invisible to WGC AND DDA at full rate). Two facts make the build the right move anyway:

1. **The pre-24H2 WGC framerate-capture bug is RULED OUT** — the rig is Windows 11 **25H2, build
   26200.8457** (verified first-hand via the registry), past the 24H2/26100 fix.
2. **LSFG-Windows is an external capturer on DXGI Desktop Duplication** (default backend) — the same
   DWM-bound method available to us, NOT a present hook. The operator observes LSFG consuming **all** fps
   on this rig → **first-hand evidence DDA reaches the full rate HERE** (BF6 is effectively composed for the
   capturer, or LSFG forces it) → the WGC-**window** ~60 is a window-specific shortfall a DDA path closes.

⇒ The build is **measurement-gated** (CP2): the operator's one BF6 run of the DDA backend confirms it.
If DDA also caps, the cause is the Independent-Flip bypass and the only non-injecting lever is composed-flip
forcing (CL-4, opt-in). **Present-hook (the only >refresh method) stays REJECTED for BF6** — EA Javelin is
a kernel anti-cheat; injection risks a ban and breaks the external/non-injecting property.

## §0.2 — The shared spine (canonical component IDs — used across all three docs)

The anti-drift backbone. The strategies + risk register refer to these verbatim.

| ID | Component | What it is | Source today |
|---|---|---|---|
| **CL-1** | **DDA acquire core** | D3D11 device on the output-**owning** adapter; `IDXGIOutput5::DuplicateOutput1` (native format; `DuplicateOutput` fallback); `AcquireNextFrame(finite)` event-driven; `ReleaseFrame` each frame | proven in `render_assistant` (reuse) |
| **CL-2** | **Lock-free SPSC ingest ring** | producer (capture thread) CopyResources the acquired GPU texture into a ring slot, publishes by `seq_cst` index; consumer reads. **NO keyed-mutex on the hot path.** Stage-1 = host-staged (proven) | `minimal_fg` ring exists (reuse) |
| **CL-3** | **GPU-resident zero-copy ingest** *(max-perf follow-up)* | `CreateSharedHandle` (NT, `D3D11_TEXTURE_BIT`) → VK import once → **shared TIMELINE semaphore** sync (NOT keyed-mutex) → sample direct; per-frame = one fence wait, no PCIe round-trip | new (designed) |
| **CL-4** | **Composed-flip forcing** *(conditional fallback)* | force the captured surface through DWM composition (least-invasive: 1×1 click-through topmost composited overlay reasserted ~500 ms; MPO-disable only if needed, save/restore). Opt-in, default-off; +1 frame | new (designed) |
| **CL-5** | **The owned `CaptureSurface` seam** | the primitive boundary mirroring `PresentSurface` (opaque Impl, POD desc, `std::expected`, cold `create()`/hot `acquire()`, single-thread, Windows-only stub) — **FR-RENDER-2 candidate** | new (app-local first) |
| **CL-6** | **Backend selection + WGC fallback** | `--capture-backend dda\|wgc` (default `wgc` → **byte-identical-off**); DDA → WGC on init failure | new (flag) |

CL-1/CL-2/CL-6 are the **CP1 deliverable** (the rate fix for the operator's BF6 run). CL-3 is the measured
max-perf follow-up (CP3). CL-4 is conditional on CP2. CL-5 is the consolidation into a pillar-symmetric
primitive (CP5 / the FR).

## §1 — The architecture (one screen)

```
 the 240Hz output (owned by the 4090)
        │  DuplicateOutput1 (native fmt)           CL-1
        ▼
 AcquireNextFrame(finite) ──► IDXGIResource (GPU-resident, capture device)
        │  (had_upd = LastPresentTime≠0 || AccumulatedFrames≠0  → a genuinely NEW presented frame)
        ▼
   CL-2 producer: CopyResource into ring[w] ──► ReleaseFrame ──► publish w (seq_cst)   [NO keyed-mutex]
        │                                                            ▲
        │   ── stage-1: host-staged D3D11 ring (PROVEN, crash-safe) ─┘
        │   ── stage-3 (CL-3): the ring slots are VK-imported shared NT-handle textures,
        │                       producer signals a shared TIMELINE value, consumer waits it (zero-copy)
        ▼
   consumer (present/OFP thread, UNCHANGED): read ring[r] ──► flow → warp → blend → paced present
```

The producer/consumer split, the ring, and the `seq_cst` indices are the existing `minimal_fg` lock-free
pattern; CL-1 swaps the **producer** (WGC free-threaded `FrameArrived` → a DDA capture thread). The
consumer side (OFP flow, warp, blend, overlay, present) is untouched.

## §2 — Why DDA, why lock-free, why staged (the three load-bearing decisions)

1. **DDA over WGC** — not for rate (they share the DWM ceiling, §0.1) but because: (a) it is what LSFG uses
   and LSFG works on this rig (the evidence); (b) it hands us the **raw `IDXGIResource`** (a real GPU
   texture we own) — the prerequisite for CL-3 zero-copy, which WGC's managed `Direct3D11CaptureFramePool`
   abstracts away; (c) it removes the WGC-window-specific shortfall the operator measured. The WGC path
   stays as the default + the fallback (CL-6).
2. **Lock-free, NO keyed-mutex** — the BF6 crash was a cross-API keyed-mutex `AcquireSync(INFINITE)`
   deadlock: a GPU-starved D3D11 producer holding the mutex starved the VK consumer's acquire forever
   (dossier §6.1.1, MS surface-sharing docs). CL-2 host-staged makes the CPU memcpy the cross-API barrier
   (no shared lock at all); CL-3 replaces the keyed-mutex with a **shared timeline semaphore** (wait/signal
   back-pressures, never deadlocks). Keyed-mutex on the hot path is **forbidden by construction** (CR-3).
3. **Staged (rate first, latency second)** — CP1 ships the PROVEN host-staged DDA = the rate fix the
   operator asked for, crash-safe, immediately runnable on BF6. CP3 removes the host round-trip
   (GPU→CPU→GPU, ~0.6-1.5 ms @1080p — the latency the mandate forbids as a default) only after the rate is
   proven and the zero-copy latency win is **measured** (efficiency mandate: measured over assumed).

## §3 — Phase plan

| Phase | Deliverable | Gate to advance |
|---|---|---|
| **CP0** | This triad + the SOTA dossier §6.1.1/§6.1.2, registered. | Risk register has **no `open` CR**; operator steer. **(operator: "adelante", 2026-06-26d)** |
| **CP1** | CL-1 + CL-2 + CL-6: DDA host-staged lock-free backend behind `--capture-backend dda` (WGC default). The instrument (`in`/`wgc_arrived`/`ring_dropped`) reports DDA delivery. | Builds green; validation-clean; **byte-identical-off** verified (WGC-default run unchanged); zero keyed-mutex on the path. |
| **CP2** | **MEASURE** — the operator's BF6 run of `--capture-backend dda`: does `in` rise to ~90-100? | The operator's one run. (a) yes → CP3; (b) ~60 (Independent Flip) → CP4. |
| **CP3** | CL-3 GPU-resident zero-copy (NT-handle + shared timeline semaphore). | Latency delta measured; kept only if it wins; no keyed-mutex; validation-clean 30 s soak; device-loss-safe. |
| **CP4** | *(conditional, only if CP2-b)* CL-4 composed-flip forcing. | Lifts `in`; +1 frame characterized; opt-in default-off; global DWM-state save/restore verified (CR-8). |
| **CP5** | Consolidate into the owned `CaptureSurface` primitive (CL-5); the FR-RENDER-2 procedure if it generalizes. | The FR gate; pillar consolidation of the 8 scattered DDA copies. |

## §4 — Pillar faithfulness

- **Consume, don't reinvent.** The DDA acquire pattern is lifted from `apps/render_assistant/src/main.cpp`
  (`d3d_init` `DuplicateOutput` at **:1906-1927**; the acquire loop `AcquireNextFrame(33)`→`CopyResource`→
  map at **:5556-5581** — both read first-hand) and the 7 bench-stages (`stage3_capture`, `stage5_assistant`,
  `stage5b_present`, `stage7_livefire`, `stage7b_multimon`, `stage44a_present_probe`, `stage28_wgc` [the
  WGC-vs-DD bench]). CL-5 **consolidates** these into one owned primitive (the efficiency mandate: one
  owned layer replacing 8 copies).
- **The symmetric twin of `PresentSurface` (FR-RENDER-1).** PresentSurface owns *how a frame reaches the
  panel*; CaptureSurface owns *how a frame reaches us from the source*. CL-5 mirrors its contract exactly
  (opaque Impl, POD desc, `std::expected`, cold/hot split, single-thread, Windows-only stub). It is a
  strong **FR-RENDER-2** candidate — but this plan does **not** pre-bake the pillar change; CP1-CP4
  prototype **app-local** in `apps/minimal_fg`, and CP5 lifts it via the FR procedure only if it generalizes.
- **Efficiency mandate.** DDA is the lightest correct non-injecting method; the zero-copy path (CL-3)
  collapses the per-frame cost to one fence wait; the host round-trip is a degraded fallback, never the
  default.

## §5 — Honest hard-truths (stated plainly)

- **No non-injecting capture exceeds the monitor refresh.** The full game rate above 240 Hz is reachable
  ONLY by present-hook injection (AC-unsafe for BF6). For the operator's regime (90-100 < 240) this ceiling
  is **not binding** — the goal is "every game frame up to the 240 ceiling", which DDA can deliver IF the
  frames reach the compositor.
- **DDA is not a guaranteed fix.** It shares the Independent-Flip bypass with WGC. The plan's success at
  CP2 rests on the LSFG-DXGI evidence; if it fails, CL-4 (composed-flip forcing) is the documented fallback
  at a +1-frame cost — a deliberate trade against MAX-PERFORMANCE, taken only on measured necessity.
- **The keyed-mutex deadlock is a known, mitigated crash class** (CR-3), not a residual risk we discover —
  it is forbidden on the hot path by construction.

## §6 — Links + registration

- Triad siblings: [`CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md`](CAPTURE_LAYER_IMPLEMENTATION_STRATEGIES.md) ·
  [`CAPTURE_LAYER_RISK_REGISTER.md`](CAPTURE_LAYER_RISK_REGISTER.md). Cited foundation:
  [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md) §6.1.
- Parent: [`MINIMAL_FG_MASTER_PLAN.md`](MINIMAL_FG_MASTER_PLAN.md) (MC-1) · the durable spine
  [`../ACTION_PLAN.md`](../ACTION_PLAN.md) (▷ NOW 2026-06-26d).
- Consumed pillar: the present pillar
  `framework/render/present/include/phyriad/render/present/PresentSurface.hpp` (the FR-RENDER-1 precedent
  CL-5 mirrors).
- Reconciled registers (do not re-derive): `MINIMAL_FG_RISK_REGISTER` (MR-4 byte-identical-off, MR-5
  no-game-cap, MR-7 present ceiling), `DEVICE_LOST_RECOVERY_RISK_REGISTER` (`vk_live`),
  `FG_SATURATION_STABILITY_RISK_REGISTER` (the lock-free ring precedent).
- All three docs are added to [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) in the
  same change (handled at the CP0 close).
