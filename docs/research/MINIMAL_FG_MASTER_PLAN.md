# MINIMAL_FG_MASTER_PLAN — the "hello world" of Frame Generation, as a rock-solid base

> **Diátaxis type:** Planning / explanation (a master plan, per
> [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md)).
> **Status:** `designed` — no code shipped under this plan yet; the SOTA grounding is `measured`
> (third-party, cited) and the minimal-core architecture is a design.
> **Plan tier:** **Tier-2** (risk-bearing — the present-pacing / async-compute / multi-queue rebuild
> touches the crash / device-loss / concurrency class). The required linked set:
> this master plan · [`MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md`](MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md)
> · [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md). The cited prior-art dossier
> [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md) is the verifiable-reference foundation.
> **Provenance:** the 2026-06-26 5-agent SOTA barrido (FSR3 open/documented · LSFG dev-stated + reverse-
> engineered · academia RIFE/softmax-splatting/AceVFI · GPU-pipeline architecture), conclusion-first +
> adversarially verified; the raw corpus is captured in `scratchpad/minimal_fg_sota_notes.md` and is
> lifted into the dossier. Citations in this plan defer to the dossier — **no number or API is asserted
> here that the dossier does not cite first-hand.**

## §0 — Objective + the operator's sharpened direction

**Principal objective (unchanged):** perfect PhyriadFG — match then surpass LSFG — within Phyriad's four
pillars and the efficiency mandate.

**The sharpened direction (operator, 2026-06-26):** build the **simplest possible Frame Generator that is
still a rock-solid base** — the "hola mundo del FG" — and from there **strategically re-place** the layers
we have already built (the ones that earn their cost) **plus create the ones our blind implementation never
let us see.** This is explicitly **NOT** a `--minimal` flag bolted onto the accreted `apps/render_assistant/
src/main.cpp`: that path drags the very seam overhead we are trying to measure against. It is a **clean
minimal CORE with the right SEAM**, measured against the full pipeline, then re-layered under a net-gain gate.

**Why now (the diagnosis this plan acts on):** the operator's hypothesis — *our bottleneck is the
inter-component connection/sync overhead of many accreted layers, not the per-pass GPU timings; LSFG wins by
minimalism* — is **CONFIRMED by SOTA with measured numbers** (see §3). The residual "vibration / stick" the
operator's eye caught is two things stacked: (a) the **seam overhead** (accumulated input lag + cadence
jitter from fence-per-stage + host round-trips), attacked directly by this arc, and (b) the **universal
no-MV disocclusion frontier** (gravity/pulse), which is information-theoretically irreducible (§7) and is
*not* our bug.

## §0.1 — The shared spine (canonical component IDs — used across all four docs)

These IDs are the anti-drift backbone. The dossier, strategies, and risk register refer to them verbatim.

| ID | Component | What it is (minimal-core scope) | Source today |
|---|---|---|---|
| **MC-1** | **Capture** | acquire game frames A, B as GPU-resident textures, zero host copy | WGC path exists in `main.cpp` (reuse) |
| **MC-2** | **Flow** | bidirectional optical flow F(A→t), F(B→t) — the motion source (no engine MVs) | flow compute shader exists (reuse as a node) |
| **MC-3** | **Warp** | BACKWARD-warp A and B to t=0.5 (bilinear) → Â, B̂ — dense, no holes | warp shader exists (reuse as a node) |
| **MC-4** | **Blend** | soft occlusion-mask composite I_t = M·Â + (1−M)·B̂ | new minimal node (the mask is the minimal disoccl handler) |
| **MC-5** | **Present** | paced present on a dedicated thread, **DROP** the interpolated frame under load (never block) | DComp present exists; the pacing+drop is new |
| **SG** | **Seam (the graph)** | the render-graph discipline that wires MC-1..MC-5: declare reads/writes → derive barriers; 1 fence/frame-in-flight; GPU-resident; async-compute; cull-when-off | **does not exist — the layer the blind impl never let us see** |

The **deliverable of the minimal arc is SG + MC-4 + MC-5-pacing**, wiring the existing MC-1/MC-2/MC-3
primitives as graph nodes. It is **re-cabling on a correct seam, not rewriting the shaders.**

## §1 — The SOTA verdict in one screen (detail → the dossier)

FSR3 (open/documented), LSFG (dev-stated + reverse-engineered), and academic VFI (RIFE / softmax-splatting /
AceVFI survey) **converge on the same minimal shape**, and GPU-pipeline architecture (Frostbite FrameGraph,
Granite, Khronos sync) supplies the seam discipline:

1. **The irreducible motion-compensated core = flow + backward-warp + soft occlusion mask.** Pure blend /
   crossfade is the TOY (a temporal average — ghosts every moving edge; a fail-safe fallback only). Backward
   warp (not forward/splatting) is the minimal SOLID choice: dense output, no holes, what RIFE uses, AceVFI-
   favored; forward splatting needs a synthesis network on top (more complex, not simpler).
2. **External-capture irreducibles:** WGC capture (cross-GPU, stable, ≥1 VBLANK floor); the only motion source
   is estimated optical flow (no engine MVs, no depth → no correct occlusion ordering — a structural ceiling);
   present from our own window (cross-process DComp-into-the-game is OS-blocked).
3. **Present under load: DROP, never block.** DLSS-G drops the interpolated frame when late (real frame always
   shown); FSR3's blocking-present stalls the game thread under saturation. The external tool must adopt the
   drop model. FSR3's cadence algorithm (open: moving-avg frametime → target present time → wait-until-target)
   is the verifiable pacing reference.
4. **LSFG's actual edge is minimalism + pacing, not interpolation quality.** Its developer deliberately traded
   quality for a light pass and even display-level pacing (EMA jitter smoothing). The lesson: **weight goes to
   pacing + pass-lightness, not stacked quality passes.**

## §2 — The minimal core design (the hello-world)

The mandatory path, ~4 compute stages + a paced present, all behind the SG seam:

```
MC-1 capture(A,B)  ──►  MC-2 flow F(A→t),F(B→t)  ──►  MC-3 warp Â,B̂  ──►  MC-4 blend M·Â+(1−M)·B̂  ──►  MC-5 paced present (DROP-late)
        │ GPU-resident texture import (zero host copy)        │ compute queue          │              │ graphics queue
        └──────────────── one timeline-semaphore: compute(MC-2/3/4) signals → graphics(MC-5) waits ────┘
```

- **MC-4 (blend) is the minimal disocclusion handler.** A non-trivial soft mask M (NOT 0.5): where A is
  occluded, M→0 and B̂ wins; where B is occluded, M→1. It degrades to a *soft* artifact at true disocclusion,
  never a hole. The mask's first cut is a flow-consistency / photometric heuristic (the academic minimal); a
  learned/neural mask is an explicit later layer, not part of the hello-world.
- **MC-5 (present) is a dedicated pacing thread** that computes the target present time from a moving average
  of arrival intervals (the FSR3 reference), presents the interpolated frame at t, and **drops it if it would
  land too close to the next real frame** (the DLSS-G model). Present never blocks the generation fence.
- **No quality passes in the hello-world.** Object detection, contour, bg-snap, band-xfade, matte, gme, medoid,
  ts-smooth — all OUT of the minimal core; they re-enter only via §4 under the net-gain gate.

## §3 — The seam discipline (SG) — the heart of the arc

The operator's diagnosis, **measured**: "many layers = overhead" is caused by three things, none of which is
the per-pass GPU cost:

1. **A fence per stage** instead of one per frame-in-flight. Each `vkWaitForFences` blocks the host; each
   `vkQueueSubmit` costs ~24–36 µs CPU (cited). Our `vkWaitForFences(fBridge, UINT64_MAX)` in `main.cpp` is the
   canonical instance.
2. **Host round-trips** (GPU→CPU→GPU) between stages → flush the full GPU L2 to RAM over PCIe. The latency killer.
3. **Over-broad barriers** (`ALL_COMMANDS`) → drain the pipeline. Khronos measured **13 % frame-time wasted**
   vs precise stage masks.

**The cure = the render-graph seam (SG).** Each pass declares the resources it reads and writes; a CPU-side
compiler derives every barrier. The properties that make a minimal core *layerable without re-introducing the
overhead* (this is the load-bearing requirement):

- **Add a layer = 0 hand-written barriers, 0 new fences** (the compiler re-derives; at most +1 intra-queue
  `VkEvent`).
- **A disabled layer is CULLED from the graph** — 0 execution, 0 allocation, 0 barrier — not an `if` inside a
  shader and not a registered-but-skipped pass.
- **1 fence per frame-in-flight**, never per stage; inter-stage sync = `VkEvent` (same queue) / timeline
  semaphore (cross-queue).
- **Data stays GPU-resident** — no host round-trip between stages, ever.
- **Async-compute**: MC-2/MC-3/MC-4 on the compute queue overlap MC-5 present on the graphics queue, joined by
  one timeline semaphore → present is decoupled from generation (measured 5–29 % wins in the cited samples).
- **Precise stage masks** (signal-early / wait-late), barriers batched per frame boundary.

**SG is "the layer the blind implementation never let us see."** It is what lets us re-place the good layers as
graph nodes for the cost of one declared edge each, instead of one bolt-on fence each.

## §4 — The layering strategy (clean-room core → measure → re-layer under a net-gain gate)

1. **Build the clean minimal core (MC-1..MC-5 on SG)** as a sibling, reusing the validated flow/warp shaders as
   nodes. Keep `main.cpp`'s full pipeline untouched and runnable as the A/B reference.
2. **Measure minimal-vs-full** on the TB-C12 testbench + the operator's eye: input→photon latency, cadence
   jitter (the escalonado metric, TB-C9), and the operator's hand-eye sensation (the true arbiter — numbers do
   not see the vibration; that lesson is load-bearing here).
3. **Re-layer ONLY net-gain passes.** Each existing layer re-enters as an SG node and must pass a gate:
   *does it improve the operator's eye / a measured artifact MORE than it costs in latency + cadence on the
   clean seam?* The honest caveat — **the passes fix REAL measured artifacts** (bg-snap eliminated object-
   fragment hallucination; deficit-tier + the deep capture ring took heavy-scene freezes 242→0). The plan is
   **not** to delete them blind; it is to re-measure each on the clean seam and keep the ones that still win.
   Validate the whole: **minimal-NET (core + kept layers) must beat complex-NET** (the LSFG evidence says a
   light, well-paced pipeline wins perceptually — but we prove it on our rig, we don't assume it).
4. **Create the missing layers** the blind impl never surfaced — first among them SG itself; then better PACING
   (the LSFG lesson) before any new quality pass.

**The build-vs-strip decision (recorded):** clean-room core was chosen over a `--minimal` strip of `main.cpp`
because the strip would measure *with the bad seam still in place*, contaminating the very variable under test.
The operator then elected **plan-formal-first** (this triad) before the build.

## §5 — Phase plan

| Phase | Deliverable | Gate to advance |
|---|---|---|
| **P0** | This triad + the cited dossier, registered. | Tier-2 risk register has **no `open` risk**; operator steer. |
| **P1** | Scaffold the clean minimal core: MC-1 capture + MC-2 flow + MC-3 warp + MC-4 blend, wired on a first-cut SG, present naive (no drop yet). | Builds green; produces an interpolated frame on the testbench; GPU-resident verified (no host round-trip in the core). |
| **P2** | Harden SG: 1 fence/frame, async-compute compute‖graphics, precise batched barriers, cull-when-off. | Validation-layer clean; barrier/submit/fence count meets the §3 budget; no host sync between stages. |
| **P3** | MC-5 present-pacing + DROP-interpolated (DLSS-G model) + the FSR3 moving-avg cadence. | No block on the generation fence; drop path exercised under load; device-loss-safe (risk register). |
| **P4** | Measure minimal-vs-full: latency + cadence + operator eye. | A recorded A/B; the operator's eye verdict. |
| **P5** | Re-layer under the net-gain gate; then improve PACING. | Each re-added layer passes its gate; minimal-NET ≥ complex-NET. |

## §6 — Tier-2 risk surface (detail → the risk register)

The risk-bearing classes this arc opens (each enumerated, mitigated-as-code, and first-hand-verified in
[`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md); **no `open` risk may be committed**):

- **Crash / device-loss:** the new present-pacing + drop path (use-after-reset, device-loss-through-poll) —
  reconciles against the discharged parents (`FG_SATURATION_STABILITY`, `DEVICE_LOST_RECOVERY`, the
  `--async-present` precedent).
- **Concurrency:** the async-compute compute‖graphics split + the timeline semaphore + cross-queue ownership
  transfer (external-sync violations, present/generation races).
- **Dogma — byte-identical-off / no-game-cap:** the clean core is a *new* path; while it coexists with the
  full pipeline it must not change the default; it shares one GPU with the game and must never cap it.
- **Measurement validity:** minimal-vs-full must be a fair A/B (same source, same scoring); the net-gain gate
  must not silently drop a layer that fixes a real artifact.

## §7 — Honest hard-truths (stated plainly, not inflated)

- **The disocclusion floor is real and ours-too.** True disocclusion (a region hidden in both A and B) is
  information-theoretically unrecoverable without hallucination; with no depth buffer there is no correct
  occlusion ordering. The LSFG developer says it plainly: *"it's all guesswork and will always be somewhat
  inaccurate."* Our gravity/pulse/stick is this universal no-MV frontier — FSR3 and LSFG have it too. The
  minimal arc does **not** claim to beat it; it claims to remove the **seam** component of the artifact and to
  pace better.
- **Minimal is not automatically better.** The accreted layers fix measured artifacts. The arc's success
  condition is **minimal-NET > complex-NET**, proven on our rig + the operator's eye — not "fewer layers" as a
  slogan.
- **We already protect threads more than LSFG does** (`--fg-protect`, the THREAD_PROTECTION arc) — that axis is
  closed and orthogonal; it is not what this arc is about.
- **Our edge over LSFG remains the multi-GPU + the image-field trust gate**, but the hello-world is single-path
  first; multi-GPU re-enters as a layer under §4 (cross-GPU MV sync was a *source* of the abrupt-jerk component
  — it must re-earn its place).

## §8 — Pillar faithfulness

- **Consume, don't reinvent:** MC-1 capture and MC-5 present build on the existing present pillar
  (`PresentSurface`) and the WGC/DComp paths already in the tree; MC-2/MC-3 reuse the validated flow/warp
  shaders. The `hal` fp16 path and the `gpu`/`topology` pillars stand.
- **SG (the render-graph seam) is the open architectural question:** it is either a new app-local capability or
  — if it proves general — a Feature Request into a pillar (the FR procedure decides; do not pre-bake a pillar
  change). The strategies doc scopes the first-cut SG as app-local and minimal (a "poor-man's render graph":
  resource handles + declared reads/writes + a linear topo-sort + derived barriers), explicitly NOT a full
  Frostbite-class graph on day one.
- **Efficiency mandate:** the whole arc IS the mandate applied — zero-overhead seam over the accreted one,
  measured over assumed.

## §9 — Links + registration

- Triad siblings: [`MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md`](MINIMAL_FG_IMPLEMENTATION_STRATEGIES.md) ·
  [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md). Cited foundation:
  [`MINIMAL_FG_SOTA_DOSSIER.md`](MINIMAL_FG_SOTA_DOSSIER.md).
- Parent arc: the render-assistant / PhyriadFG arc; the durable spine is
  [`../ACTION_PLAN.md`](../ACTION_PLAN.md) (CURRENT POSITION → the minimal-FG arc).
- Reconciled registers (do not re-derive): `FG_SATURATION_STABILITY_RISK_REGISTER`,
  `DEVICE_LOST_RECOVERY_RISK_REGISTER`, `THREAD_PROTECTION_RISK_REGISTER` (closed, `45fb505`),
  `SINGLE_GPU_COLLAPSE_RISK_REGISTER` (the single-path degenerate).
- All four docs are added to [`../FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) in the same
  change (handled at the P0 close).
