# PhyriadFG — Architecture

PhyriadFG is an **external-capture**, **multi-GPU** frame generator: it captures a game's
already-rendered output, estimates the motion between real frames, synthesizes intermediate frames, and
presents them — with no access to the game engine or its motion vectors. One executable, one build.
Design focus: **maximum performance** (the hot work lives on the GPU and in thread bodies with no added
indirection).

## Build (one command)

`build.bat` → `build\phyriad_fg.exe`. Requires MSVC, the Vulkan SDK, and Ninja. One target, one `build\`,
LTO enabled (splitting into modules costs no performance).

## Thread model and multi-GPU split

Three threads in a pipeline, communicating through `FgContext` (explicit shared state; SPSC rings +
sequence counters `c_seq` / `f_seq` / `p_presenting`):

| Thread | Role | GPU |
|---|---|---|
| **C** — capture | acquires the real frame (DDA or WGC), converts/packs it into a ring | iGPU (convert+pack) |
| **F** — flow | optical flow (MV + SAD) via an image pyramid, plus the global motion model (gme) and the holons | GTX 1080 Ti |
| **P** — present | motion-compensated warp to phase `t`, blend, quality, pacing, presentation | RTX 4090 |

The GPU split is **contextual**, not arbitrary: the iGPU converts/packs with zero copy (no VRAM — it
lives in system RAM), the 1080 Ti is the optical-flow workhorse, and the 4090 does the warp + present
(it owns the display). Each role sits where its work pays off best.

## One frame's pipeline

```
capture(C) ─► flow(F: MV/SAD + gme) ─► warp+blend(P: backward-warp prev,cur to t) ─► quality(P) ─► present(P, paced, drop-late)
```

Between each pair of real frames `(prev, cur)`, one or more intermediate frames `I_t` are synthesized:
`Â = prev[x − t·mv]`, `B̂ = cur[x + (1−t)·mv]`, blended and arbitrated by the quality layers
(bidirectional occlusion classification, global-model matte, background snap, band crossfade, edge-gated
hard-pick). The phase `t` is chosen by a content clock (NCO + PLL) — the multiplier is variable, not
fixed.

## The central config resolver (the single source of truth)

Flags parse into a `Config` struct (`cli/cli.hpp`). When parsing finishes, **`resolve_config()`**
(`cli/cli.cpp`) does, in one place:

1. **The dependency cascades** (order-independent): a flag that requires another, or disables another, is
   reconciled here — regardless of command-line order.
2. **The derived predicates** (`Config::Derived d`): the single owner of cross-layer decisions. The main
   one is `d.field_to_warp` = "the iGPU contour field (binding 11) must be bridged to the 4090" =
   `igpu_field && (afill || bg_snap || band_xfade>0 || disoccl_hardpick>0 || mc_on)`. The three sites
   that create/import/upload that resource read **this** predicate instead of re-deriving the gate by
   hand (when each layer re-derived its own condition, they drifted apart).
3. **A startup self-audit**: it warns when a requested flag can have no effect because its dependency is
   absent (rather than a silent no-op).

`resolve_config()` is idempotent and re-callable: if a resource turns out to be unavailable at runtime
(e.g. no iGPU convert path), `main.cpp` clears the flag and re-resolves — the cascade and predicates are
recomputed in the same place. **To add a new capability that reads a shared resource: register it in the
predicate in `resolve_config`; the consumer sites are untouched and cannot desynchronize.**

## The governor control word (per-tick decision, single owner)

Under RTX 4090 saturation, an optional *governor* graduates down non-essential work so the FG keeps
generating. The decision is a **control word** published by a single owner:

- `governor_floor_for_util()` (`core/globals.hpp`) — the **single decode** of utilization → tier floor
  (0/3/4/5).
- `g_gov_floor` (atomic) — **computed and published by thread P** (which owns the utilization reading and
  the hysteresis), **read by thread F** and applied as `max(cpu_ladder, floor)`. One producer, one
  consumer, advisory and lock-free (same contract as `g_gpu_a_util`). With the governor off or the GPU
  cool the floor is 0 → no-op. One decode site, never two reading utilization independently.

## Where do I change what?

| To change… | File |
|---|---|
| flags / defaults / cascades | `cli/cli.{hpp,cpp}` (`resolve_config`) |
| capture (DDA/WGC) / ingest | `capture/capture.cpp` (`run_capture`) |
| optical flow / gme / holons | `flow/flow.cpp` (`run_flow`) |
| warp / blend / quality | `warp_blend/warp_blend.cpp` + the warp inside `run_present` |
| pacing / present / content clock | `present/present.cpp` (`run_present`) |
| cross-thread shared state | `core/fg_context.hpp` |
| global signals / control word | `core/globals.{hpp,cpp}` |
| startup / device / allocation | `core/main.cpp` (init — cold, runs once) |
| a shader | `shaders/*.comp` |

## Repository layout

```
src/
  core/        orchestrator + infrastructure: main (init → FgContext → 3 threads → join),
               fg_context, device, vk_util, globals, ra_simd, telemetry_csv
  cli/         Config + flag parsing + resolve_config (cascades + predicates + self-audit)
  capture/     run_capture [thread C]: D3D/DXGI (DDA), WGC, convert/unpack
  flow/        run_flow    [thread F]: flow pyramid, gme, mv-smooth/median, holons, optional NVOFA
  warp_blend/  the warp-at-presenter + the field/fill + stat helpers
  present/     run_present [thread P]: PresentSurface, pacing, bridge, the present loop
  instrument/  dump/diagnostic helpers
framework/     supporting pillars: render/present (PresentSurface), render/vulkan (optical-flow
               pipeline + shaders), hal, topology, schema
shaders/       the frame generator's .comp shaders
tools/         capture_dump   ·   bench/   micro-benchmarks
```

## Performance — why the structure doesn't risk it

- The **hot** work (per-pixel / per-frame) lives in the **shaders** (GPU) and **inside each `run_X`**
  (thread body, one translation unit → normal inlining).
- The hot command-recording helpers are **inlined in headers**.
- The new cross-translation-unit calls are **cold** (init factories) or **GPU-bound** (fence waits).
- `FgContext` is references → the same indirection as a `[&]` capture. No new allocations, locks, or
  copies on the hot path.
- LTO covers any remaining cross-translation-unit inlining.
</content>
