# MINIMAL_FG_IMPLEMENTATION_STRATEGIES — the concrete HOW for the clean minimal-FG core

> **Diátaxis type:** how-to / explanation (implementation strategies — the per-strategy edit sites and the
> seam discipline that wires the minimal core). **Normativity:** BCP 14 (MUST / SHOULD / MAY in their RFC 2119
> senses). **Status:** `designed` — **no code is built under this plan.** Every strategy below is a design;
> each code reference that is not first-hand-verified in this authoring pass is marked **"to confirm during
> implementation"**. The present-pillar API references (§3, §6) were read first-hand from the header this pass.
> **Plan tier:** **Tier-2** — this is the IMPLEMENTATION_STRATEGIES leg of the linked triad with
> [`MINIMAL_FG_MASTER_PLAN.md`](MINIMAL_FG_MASTER_PLAN.md) (the why/architecture/phases) and
> [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md) (the risks **MR-1 … MR-8**). Per
> [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) §2.3 **every strategy cites the MR-ID(s) it
> mitigates** so a reviewer can trace edit → risk. Cited foundation: `MINIMAL_FG_SOTA_DOSSIER.md`.
> **Component IDs** are the master-plan §0.1 spine, used verbatim (§0 below).

---

## §0 — Component map (echo of MASTER_PLAN §0.1 — self-contained)

| ID | Component | Minimal-core scope | Source today |
|---|---|---|---|
| **MC-1** | Capture | acquire frames A, B as GPU-resident textures, zero host copy | WGC path exists in `main.cpp` (reuse) |
| **MC-2** | Flow | bidirectional optical flow F(A→t), F(B→t) | flow compute shader exists (reuse as a node) |
| **MC-3** | Warp | backward bilinear warp A,B → t=0.5 ⇒ Â, B̂ (dense, no holes) | warp shader exists (reuse as a node) |
| **MC-4** | Blend | soft occlusion-mask composite I_t = M·Â + (1−M)·B̂ | new minimal node (heuristic mask, NOT 0.5) |
| **MC-5** | Present | paced present on a dedicated thread, DROP the interpolated frame under load | DComp present exists; pacing+drop is new |
| **SG** | Seam (graph) | declare reads/writes → derive barriers; 1 fence/frame-in-flight; cull-when-off | does not exist — the layer the blind impl never let us see |

The minimal-arc deliverable is **SG + MC-4 + MC-5-pacing**, wiring the existing MC-1/MC-2/MC-3 primitives as
graph nodes. **Re-cabling on a correct seam, not rewriting the shaders.**

This document is organized by the master-plan §5 phases **P1–P5**. Each phase holds one or more strategies
(`S1…S10`); each strategy declares the MR-ID(s) it mitigates.

---

## P1 — Scaffold the clean minimal core (MC-1..MC-4 on a first-cut SG, present naive)

> **Gate to advance (master-plan §5):** builds green; produces an interpolated frame on the testbench;
> GPU-resident verified (no host round-trip in the core).

### S1 — SG: the first cut is a "poor-man's render graph"  ·  *mitigates MR-3*

The first SG is deliberately **not** a Frostbite-class FrameGraph. It is the smallest construct that derives
correct barriers from declared intent, so a layer re-enters for the cost of one declared edge (master-plan §3)
rather than one hand-written barrier. Four parts:

1. **Opaque resource handles.** A resource (an image, a buffer) is referenced everywhere by an opaque
   `uint32_t` ID minted by the graph at registration (`ResHandle{ uint32_t id; }`, a POD; the underlying
   `VkImage`/`VkBuffer` + its current layout/stage/access live in a graph-side `ResourceState` table keyed by
   that ID). Passes never touch raw Vulkan handles for synchronisation — only the IDs. This is what makes the
   declared dependency graph the *single source of truth* for hazards.
2. **Per-pass declared reads/writes.** Each pass registers, at build time, the IDs it READS and the IDs it
   WRITES (`struct PassDecl { small_vector<ResHandle> reads, writes; PFN record; }`). A WRITE additionally
   declares the producing stage+access (e.g. compute storage-image write); a READ declares the consuming
   stage+access. No pass may touch an undeclared resource (debug-asserted).
3. **A linear topological sort.** The minimal core is a straight chain MC-1 → MC-2 → MC-3 → MC-4 → (naive
   present), so the first cut MAY sort linearly in declared order and assert it is a valid topo order (every
   read's producer precedes it). A general DAG sort is a later refinement; the linear assertion is enough to
   prove the seam and catches a mis-ordered declaration loudly.
4. **A derived barrier on every write→read edge — PRECISE masks.** For each resource, the compiler walks the
   sorted passes; when a pass READS an ID last WRITTEN by an earlier pass it emits ONE barrier carrying **both
   scopes**: the **execution scope** (`srcStageMask`→`dstStageMask`) AND the **memory scope**
   (`srcAccessMask` flush + `dstAccessMask` invalidate) + the layout transition. Execution-order ≠
   memory-order: a barrier that orders stages but omits the access masks leaves a stale cache and is exactly
   the non-deterministic hazard MR-3 names. Concrete first-cut example for the flow→warp edge (a compute
   storage image): `src = COMPUTE_SHADER / SHADER_WRITE`, `dst = COMPUTE_SHADER / SHADER_READ`,
   layout `GENERAL→GENERAL` (the precise stage/access/layout of each validated shader to confirm during
   implementation by reading its actual descriptor + image usage). The compiler emits these as a **batched**
   set at the frame boundary (master-plan §3: barriers batched per frame boundary), never one-per-stage with a
   host wait between.
5. **A debug barrier-dump for hand-audit.** A `--sg-dump` mode prints the derived list — for each edge: the
   two resource IDs (producer pass, consumer pass), the full `src/dstStageMask`, `src/dstAccessMask`, and the
   layout transition — so the derived barriers can be checked against a hand-audit of the MC-1..MC-5 chain.
   This dump is the primary MR-3 verification artifact (the register's "derived-barrier dump matches a
   hand-audit").

**Pillar faithfulness.** SG is **app-local** in the render_assistant for the first cut (master-plan §8); it is
a *possible* future Feature Request into a pillar only if it proves general — this document does **not**
pre-bake a pillar change. The graph wraps, and does not replace, the existing Vulkan primitives.

### S2 — Prove the graph on a SINGLE universal queue first  ·  *mitigates MR-2*

Async-compute is the *second* move, not the first. The MR-2 failure modes (two submitters on one `VkQueue`;
a missing/wrong queue-family **ownership transfer** of the warp-output image; present sampling the warp output
before the cross-queue semaphore signals) are all consequences of the multi-queue split. The first cut
**eliminates them by construction**:

- Run the whole core (MC-2 flow, MC-3 warp, MC-4 blend, and the naive present copy) on **one queue** that is
  both graphics- and compute-capable. Intra-queue ordering between SG passes uses **`VkEvent`** (set after a
  producing pass, waited before a consuming pass on the same queue) — **no queue-family ownership transfer,
  no cross-queue semaphore, no cross-queue race.** This isolates SG correctness (S1) from async complexity.
- Exactly one submission path records the SG's batched command buffer; a single submitter per `VkQueue` is
  trivially satisfied while there is one queue.

The separate compute queue + timeline semaphore + ownership transfer are deferred to **P2 / S7**, added only
after this universal-queue core is proven (the register's MR-2 mitigation: "the async variant is added ONLY
after the universal-queue core is proven").

### S3 — MC-1 capture: reuse the WGC zero-copy import + frame-pair A,B buffering  ·  *mitigates MR-5, MR-7*

- **Reuse the existing WGC GPU-resident, zero-copy import path** in `apps/render_assistant/src/main.cpp` (the
  Windows.Graphics.Capture → GPU texture import already in the tree; exact symbols/sites to confirm during
  implementation — the file carries the capture machinery densely). MC-1 wraps the imported texture as an SG
  resource handle (S1); it does **not** re-implement capture and does **not** introduce a host round-trip
  (master-plan §3: data stays GPU-resident — a P1 gate).
- **Frame-pair A,B buffering.** MC-2/MC-3 need the two most-recent real frames simultaneously. Maintain a
  tiny GPU-resident ring of imported frames and hand MC-2 the pair (A = previous real, B = current real) as
  two SG read-handles. The ring is provisioned at INIT (cold path), never allocated on the hot path
  (consistent with the no-hot-path-allocation discipline MR-1 reuses).
- **No-game-cap (MR-5):** MC-1 captures the game window via WGC only; it issues **zero**
  affinity/priority/frame-limit call against the game PID. The capture path back-pressures nothing — under
  load the *present* side drops (MC-5/S8), it never stalls the source. (Verification: grep proves zero
  foreign-PID affinity/priority/limit calls on the core path — the THREAD_PROTECTION precedent.)
- **Present-surface ceiling (MR-7):** MC-1 is capture, but it shares the external-capture topology with MC-5;
  the present is from our OWN window (S8), never a cross-process present into the game (OS-blocked). The
  Independent-Flip / MPO interaction is the documented external ceiling, characterised in MC-5, not a
  regression MC-1 introduces.

### S4 — MC-2 flow / MC-3 warp: re-cable the validated shaders as SG nodes  ·  *mitigates MR-3*

- **Re-cable, do not rewrite.** The validated optical-flow compute shader (MC-2) and the backward-warp shader
  (MC-3) already exist and are validated in `main.cpp` (reuse as nodes — exact shader file paths + dispatch
  sites to confirm during implementation). The work is to register each as an SG pass with its declared
  reads/writes (S1), **not** to touch the shader math.
- **MC-2 bidirectional flow.** Register flow as a pass that READS the A,B pair (S3) and WRITES two flow fields
  F(A→t) and F(B→t) at t=0.5 — the two motion sources for the two backward warps. Two write-handles, declared.
- **MC-3 backward bilinear warp.** Register warp as a pass that READS {A, F(A→t)} → WRITES Â and READS
  {B, F(B→t)} → WRITES B̂ (backward warp: for each output pixel, sample the source along the flow with bilinear
  filtering → dense output, no holes; the master-plan §1 minimal-solid choice, what RIFE uses). Two output
  handles Â, B̂ feed MC-4.
- **MR-3 linkage:** the *only* new correctness surface here is the **declared dependency edges** between these
  reused shaders — flow-write → warp-read, warp-write → blend-read. S1's derived barrier with precise scopes
  governs each edge; the `--sg-dump` hand-audit (S1.5) is the verification that the re-cabling introduced no
  read-before-write or stale-cache hazard. A **golden-frame determinism test** (identical A,B inputs →
  byte-identical I_t across N runs) is the register's MR-3 runtime gate that the wiring is hazard-free.

### S5 — MC-4 blend: the soft occlusion mask M (heuristic first cut, NOT 0.5)  ·  *mitigates MR-8, MR-6*

- **The minimal disocclusion handler is a soft mask, not a crossfade.** MC-4 is a new minimal SG node
  computing `I_t = M·Â + (1−M)·B̂` where **M is a non-trivial soft mask, explicitly NOT the constant 0.5**
  (constant blend is the toy temporal average — ghosts every moving edge; master-plan §1).
- **First-cut M = a flow-consistency / photometric heuristic** (the academic minimal mask). Per output pixel,
  derive M from forward/backward flow agreement and the photometric residual between Â and B̂: where A's warp
  is inconsistent (occluded in A) M→0 so B̂ wins; where B's warp is inconsistent M→1 so Â wins; elsewhere a
  smooth mid value. It degrades to a *soft* artifact at true disocclusion, never a hole. No learned/neural
  mask, no quality passes — those are explicit **later** layers (master-plan §2), re-entering only under the
  P5 net-gain gate.
- **MR-8 linkage:** keeping the learned mask OUT of the hello-world is precisely the "fewer layers ≠ better"
  discipline working in the other direction — the minimal core must be *honestly minimal* so P4's minimal-vs-
  full A/B measures the seam, not a smuggled quality layer. The mask is a single declared SG node (READS Â,
  B̂, and the two flow fields; WRITES I_t), so when a better mask re-enters in P5 it swaps this node for the
  cost of one declared edge (S1), and its artifact-fixed/cost pair is recorded (MR-6, S10).

---

## P2 — Harden SG (1 fence/frame, async-compute, precise batched barriers, cull-when-off)

> **Gate to advance:** validation-layer clean; barrier/submit/fence count meets the master-plan §3 budget;
> no host sync between stages.

### S6 — Harden the seam: one fence/frame, batched precise barriers, cull-when-off  ·  *mitigates MR-3*

- **One fence per frame-in-flight, never per stage.** The accreted pipeline's canonical defect is a
  `vkWaitForFences(fBridge, UINT64_MAX)` per stage that blocks the host (master-plan §3.1; the site exists in
  `main.cpp` — to confirm during implementation). SG submits the core's batched command buffer once per frame
  and waits on **one** fence per frame-in-flight; intra-frame ordering is `VkEvent` (S2), not a host wait.
- **Batched precise barriers.** The S1 compiler emits the derived barriers as a single batched set at the
  frame boundary, with precise stage masks (signal-early / wait-late), never the over-broad `ALL_COMMANDS`
  the master-plan §3.3 cites as 13 % frame-time wasted. This is the P2 "barrier count meets the §3 budget"
  gate.
- **Cull-when-off.** A disabled layer/pass is **removed from the sorted pass list** at build time → 0
  execution, 0 allocation, 0 barrier (master-plan §3) — not an `if` inside a shader, not a registered-but-
  skipped pass. The graph re-derives barriers over the surviving passes; the `--sg-dump` confirms a culled
  pass contributes no edge. This is the property that makes P5 re-layering free of seam re-introduction.
- **MR-3 verification:** sync-validation (`VK_LAYER_KHRONOS_validation`) clean across the core; the
  golden-frame determinism test still byte-identical after the barriers are batched/tightened (tighter masks
  must not introduce a hazard).

### S7 — Async-compute SECOND: separate compute queue + timeline semaphore + ownership transfer  ·  *mitigates MR-2*

Added ONLY after S2's universal-queue core is proven (S6 green). This is the measured follow-up the register
gates:

- **Split:** MC-2/MC-3/MC-4 on a dedicated **compute** queue overlap MC-5 present on the **graphics** queue,
  joined by **one timeline semaphore** (master-plan §3: present decoupled from generation; the cited 5–29 %
  async wins).
- **Exactly one submitter per `VkQueue`** — the compute-queue submission and the graphics-queue present
  submission are distinct threads/paths, each the sole submitter of its own queue (MR-2 external-sync
  violation guard). The drop decision (S8) stays present-thread-local.
- **Explicit queue-family ownership transfer of the warp/blend-output image** from the compute family to the
  graphics family: a release barrier on the compute queue + an acquire barrier on the graphics queue, both
  derived by SG from the cross-queue read declaration (S1 extended to emit the QF-transfer pair when producer
  and consumer queues differ). Missing/wrong transfer → corruption / device-loss (MR-2b).
- **Present waits on the timeline value the blend signals** — the graphics present's FIRST read of I_t is
  gated by a timeline-semaphore wait on the value MC-4 signals (MR-2c: never sample the warp output before the
  signal). This is timeline (≥ value), not a binary semaphore, so it composes with the frame-in-flight fence.
- **MR-2 verification:** sync-validation clean; a trace confirms one submitter per `VkQueue` and that the
  present's first read is gated by the semaphore wait; the async variant is enabled behind a flag so the
  universal-queue core remains the runnable reference until async is proven.

---

## P3 — MC-5 present-pacing + DROP (the DLSS-G model, FSR3 cadence)

> **Gate to advance:** no block on the generation fence; drop path exercised under load; device-loss-safe
> (risk register).

### S8 — MC-5: a dedicated pacing thread, FSR3 moving-average cadence, DROP-late  ·  *mitigates MR-1, MR-7*

- **A dedicated pacing thread** owns the present. It is decoupled from the generation path: it **never blocks
  the generation fence** (master-plan §2). The generation side signals "I_t ready" via the SG fence/timeline
  (S6/S7); the pacing thread consumes the freshest ready interpolated frame and presents it at the target
  time, or drops it.
- **The FSR3 cadence reference (open/documented):** maintain a **moving average of real-frame arrival
  intervals** (EMA of the inter-arrival of MC-1's real frames) → compute the **target present time** for the
  interpolated frame as the midpoint of the predicted next interval → wait-until-target on the pacing thread
  (not on the generation fence). This is the verifiable FSR3 pacing algorithm cited in the dossier
  (moving-avg frametime → target present time → wait-until-target).
- **DROP the interpolated frame (the DLSS-G model):** if the interpolated frame's target present time would
  land **too close to the next real frame's arrival** (within a guard band of the predicted interval), the
  pacing thread **drops it** — the real frame is always shown, the interpolated one is skipped — rather than
  presenting late and pacing-jittering, and rather than back-pressuring the source (which would be an implicit
  game cap; the drop is also the MR-5 no-cap mechanism). FSR3's blocking-present stalls the game thread under
  saturation; the external tool MUST adopt the drop model (master-plan §1).
- **Reuse the existing in-process DComp own-window present + the validated async-present slot machinery.**
  The present builds on the present pillar `framework/render/present/include/phyriad/render/present/PresentSurface.hpp`
  — verified first-hand this pass: `PresentSurface::submit()` is the hot path (zero steady-state allocation,
  single presenter thread per the header's THREADING CONTRACT); `Style::OwnWindow` is the own-window flip
  topology; `Pacing::PresentationManager` + `submit_at(src, target_qpc)` is the DESIGNED timestamp-pacing
  upgrade (the header notes it returns `Unavailable` until the displayable-surface path is built — so the
  first cut paces on the thread with `Pacing::Immediate` + `submit()`, and `submit_at` is the later upgrade,
  to confirm during implementation). The interpolated-frame double-buffer / async-present slot ping-pong +
  non-blocking `vkGetFenceStatus` already exist in `main.cpp` (the `--async-present` precedent; exact sites to
  confirm) and are **reused, not re-derived** — the register's MR-1 mitigation reconciles this machinery from
  the parent `FG_SATURATION_STABILITY` / `REAL_FAST_PATH` registers rather than re-implementing it.
- **MR-1 (crash / device-loss) linkage:** the interpolated-frame slot is provisioned at INIT, never
  allocated/freed on the hot path; EVERY non-blocking fence-status poll in the drop decision is `vk_live`-
  wrapped → `g_quit` on `VK_ERROR_DEVICE_LOST` (reconciled from the parent registers, do not re-derive); the
  drop decision is a pacing-thread-local scalar compare (no shared lock). `PresentSurface::device_lost()` (read
  first-hand: latches a terminal DXGI device loss at the present site) is the per-instance device-loss signal
  the pacing thread can poll. **Verification:** validation clean across a 30 s soak; force a TDR in the
  drop-poll → clean `g_quit` exit, not a fence hang; confirm zero hot-path allocation (the register's MR-1
  gate).
- **MR-7 (present surface) linkage:** the present is byte-identical to the existing validated own-window DComp
  path; **no new cross-process present API is called** (cross-process DComp-into-the-game is OS-blocked). The
  Independent-Flip/MPO +1-frame interaction is characterised (the pacing thread can read
  `PresentSurface::last_flip_qpc()` → `FlipStats{sync_qpc, present_count}`, verified first-hand in the header,
  to score on display-flip time), not silently absorbed — the documented external ceiling, not a regression.

---

## P4 — Measure minimal-vs-full (latency + cadence + operator eye)

> **Gate to advance:** a recorded A/B; the operator's eye verdict.

### S9 — The measurement harness: reuse TB-C12 + TB-C9 + the input→photon tap, fair A/B  ·  *mitigates MR-6*

Reuse the existing FG testbench — **do not build a new one** (the IDs are the FG_TESTBENCH triad's; reuse as
the shared scorer):

- **TB-C12 — source + saturation.** The `testbench_minigame` supplies the dialable source (movement) + a
  REAL graphics+present **saturation** load (`framework/render/vulkan/bench/testbench_minigame/gfx_present_load.hpp`;
  the standalone `framework/render/vulkan/bench/testbench_saturation/main.cpp` also exists). This reproduces
  the saturated-GPU regime under which the seam overhead and the drop path actually matter. **Honest status:**
  TB-C12's harness headers exist in-tree (read first-hand) but TB-C12 is itself `designed`/in-progress per the
  FG_TESTBENCH master plan — its build state to confirm during implementation; the minimal-FG harness reuses
  whatever is green at P4.
- **TB-C9 — the escalonado / cadence scorer.** Score temporal fluidity (the "escalonado" stepping) via the
  TB-C9 placement-RMS + display-interval-pacing metric, analysed by `scripts/fg_testbench_fluidity.py` (the
  `disp_phase` / `disp_src` CSV columns; verified first-hand that this scorer + columns are the TB-C9
  instrument). This is the cadence half of the A/B.
- **The input→photon latency tap.** `framework/render/vulkan/bench/testbench_minigame/latency_tap.hpp` (read
  first-hand) is the WM_INPUT + QPC input→photon marker; `scripts/fg_latency_scorer.py` rejects FG-
  interpolated frames and scores the real input-bearing frame's input→photon delta. This is the latency half.
- **The fair-A/B discipline (MR-6):** minimal-core vs full-pipeline MUST run the **SAME** TB-C12 source +
  scene, the **SAME** TB-C9 cadence scorer, and the **SAME** input→photon tap — never a different
  source/scene/scorer between the two arms. The harness self-test (same input → same score) is the MR-6
  verification that the A/B is unbiased. The operator's eye is the final arbiter on the perceptual verdict
  (numbers do not see the vibration — the load-bearing lesson); the recorded A/B feeds, but does not replace,
  the eye gate (MR-8, S10).

---

## P5 — Re-layer under the net-gain gate, then improve pacing

> **Gate to advance:** each re-added layer passes its gate; minimal-NET ≥ complex-NET.

### S10 — Re-layer each existing pass as an SG node under the net-gain gate  ·  *mitigates MR-6, MR-8, MR-4*

Each existing layer re-enters the clean core **as an SG node** (S1; culled-when-off, S6) and must earn its
place:

- **The candidate layers** (each a known artifact-fixer, master-plan §4.3): object detect / contour /
  disocclusion handling / bg-snap / band-xfade / matte / gme / medoid consensus / ts-smooth / multi-GPU.
  Each is re-cabled as one SG node with declared reads/writes — *not* bolted on with a new fence.
- **The gate (MR-6):** each re-layer decision **records BOTH** (a) the **artifact it fixes** (e.g. bg-snap
  eliminated object-fragment hallucination; deficit-tier + the deep capture ring took heavy-scene freezes
  242→0) **and** (b) its **latency / cadence cost** on the clean seam (measured with the S9 harness — same
  source/scorer). A layer is kept only if it improves the operator's eye / a measured artifact MORE than it
  costs. **No silent truncation:** any dropped layer is logged **LOUD** with the artifact it was fixing (the
  no-silent-cap precedent) — a real-artifact-fixing layer is never silently removed (MR-6).
- **The eye gate (MR-8):** the minimal core MUST NOT replace the default until **minimal-NET (core + kept
  layers) ≥ complex-NET on the operator's eye**. Until then the minimal core is **opt-in / parallel** and the
  full pipeline remains the runnable reference. "Fewer layers" is never accepted as a slogan — the success
  condition is measured + eye-confirmed (master-plan §7).
- **Byte-identical-off (MR-4):** while the minimal core coexists with the full `main.cpp` pipeline it is a
  **separate path** (a sibling or a default-off flag; new flags in `parse_extra`, NOT the C1061 main chain —
  to confirm during implementation). Core-not-selected → **zero new code executes**. Before ANY default flip
  toward the minimal core: a **CSV byte-diff** proving the full-pipeline default run is unchanged
  (freshage / `frz` / present-count identical to the prior baseline binary) + `lint_hal` clean (default
  `seq_cst`, no raw `memory_order_*` in `apps/`). This is the release gate the register names.
- **Then improve PACING** (the LSFG lesson, master-plan §4.4) before any *new* quality pass — better pacing
  outranks a stacked quality pass in where the weight goes.

---

## §Links — the triad + the dossier

- **Triad siblings (Tier-2 set, mutually linked per PLAN_TIER_PROTOCOL §2.3):**
  - master plan — [`MINIMAL_FG_MASTER_PLAN.md`](MINIMAL_FG_MASTER_PLAN.md) (the why, the SOTA verdict, the
    phase plan P0–P5, the §0.1 component IDs).
  - risk register — [`MINIMAL_FG_RISK_REGISTER.md`](MINIMAL_FG_RISK_REGISTER.md) (the risks MR-1 … MR-8; no
    `open` risk may be committed).
- **Cited foundation:** `MINIMAL_FG_SOTA_DOSSIER.md` (the verifiable-reference dossier — FSR3 cadence,
  DLSS-G drop model, LSFG minimalism, RIFE/softmax-splatting/AceVFI, the Frostbite/Granite/Khronos seam
  discipline; the numbers/APIs this document defers to).
- **Process specs:** [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) (the tier spec) ·
  [`FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md) (the honesty buckets / verifiable-
  reference mandate this doc obeys).
- **Consumed pillar:** the present pillar —
  `framework/render/present/include/phyriad/render/present/PresentSurface.hpp` (read first-hand:
  `submit`/`submit_at`, `Style::OwnWindow`, `Pacing::PresentationManager`, `last_flip_qpc`/`FlipStats`,
  `device_lost`). SG is **app-local first-cut**, a *possible* future FR — not a pre-baked pillar change
  (master-plan §8).
- **Parent arc / durable spine:** [`../ACTION_PLAN.md`](../ACTION_PLAN.md) (CURRENT POSITION → the
  minimal-FG arc).

## §MR coverage map (strategy → risk; the Tier-2 traceability)

| Strategy | Phase | Mitigates |
|---|---|---|
| S1 — poor-man's render graph (handles + declared R/W + topo-sort + derived precise barriers + `--sg-dump`) | P1 | **MR-3** |
| S2 — single universal queue first (`VkEvent`, no ownership transfer) | P1 | **MR-2** |
| S3 — MC-1 capture (WGC zero-copy reuse + A,B pair buffering) | P1 | **MR-5, MR-7** |
| S4 — MC-2 flow / MC-3 warp re-cabled as SG nodes (bidir flow, backward warp Â,B̂) | P1 | **MR-3** |
| S5 — MC-4 blend (soft mask M heuristic, NOT 0.5; learned mask = later layer) | P1 | **MR-8, MR-6** |
| S6 — harden SG (1 fence/frame, batched precise barriers, cull-when-off) | P2 | **MR-3** |
| S7 — async-compute second (compute queue + timeline semaphore + ownership transfer) | P2 | **MR-2** |
| S8 — MC-5 pacing thread + FSR3 moving-avg cadence + DROP-late (own-window DComp reuse) | P3 | **MR-1, MR-7** |
| S9 — measurement harness (TB-C12 + TB-C9 + input→photon tap, fair A/B) | P4 | **MR-6** |
| S10 — re-layer net-gain gate (artifact+cost record, loud drop, byte-identical-off) | P5 | **MR-6, MR-8, MR-4** |

> **Coverage check:** MR-1 (S8) · MR-2 (S2, S7) · MR-3 (S1, S4, S6) · MR-4 (S10) · MR-5 (S3) · MR-6 (S5, S9,
> S10) · MR-7 (S3, S8) · MR-8 (S5, S10). All eight MR-IDs are cited by at least one strategy.
