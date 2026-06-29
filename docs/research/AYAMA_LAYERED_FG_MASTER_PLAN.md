<!-- PhyriadFG tri-tier gap-aware frame-gen — master plan (design/explanation). -->
<!-- Why: the architecture FORCED by the measured F-thread wall; the design contract A reader -->
<!-- acts on before implementation. Sibling impl-strategy doc holds the "how"; this holds the "why". -->

# Ayama Layered Multi-GPU Frame-Gen — Master Plan

Status: `0.1.0-experimental` · Type: **Planning / design (Explanation)** per
[FDP §3](../canon/FORMAL_DOCUMENT_PROTOCOL.md). Product: **Ayama** (the
multi-GPU frame-gen assistant; app `apps/render_assistant/`). Sibling document:
[`AYAMA_LAYERED_FG_IMPLEMENTATION_STRATEGIES.md`](AYAMA_LAYERED_FG_IMPLEMENTATION_STRATEGIES.md)
(the "how" — both are written against one binding **architecture spine**; the
spine owns every fixed interface/stage name and is the anti-drift authority,
§11).

> **Normative keywords.** The key words **MUST**, **MUST NOT**, **REQUIRED**,
> **SHALL**, **SHALL NOT**, **SHOULD**, **SHOULD NOT**, **RECOMMENDED**, **MAY**,
> and **OPTIONAL** are to be interpreted as described in BCP 14
> ([RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
> [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)) when, and only when,
> they appear in all capitals. This is a design document; its requirements are
> the contract the implementation phases (§10) MUST meet, not shipped behavior.

> **Honesty regime (declared once, per [FDP §4.8](../canon/FORMAL_DOCUMENT_PROTOCOL.md)).**
> Every capability claim carries a status bucket — `shipping` · `designed` (spec,
> no code) · `measured` · `unmeasured` · `conjectured` — per
> [FDP §4.2](../canon/FORMAL_DOCUMENT_PROTOCOL.md). Each load-bearing reference is
> grounded in a real `file:line` confirmed first-hand this session; the
> verification record is anchored once and linked, not re-attested per mention.
> The novelty and prior-art assessment (§9) is **web-unverified** and flagged as
> needing a first-hand web pass; nothing in it is stated as established fact.

Absolute file references (this session's grounding set):
`G:\phyriad\apps\render_assistant\src\main.cpp`,
`G:\phyriad\apps\render_assistant\shaders\wap_warp.comp`,
`G:\phyriad\apps\render_assistant\shaders\igpu_convert_pack.comp`,
`G:\phyriad\framework\gpu\include\phyriad\gpu\GpuDescriptor.hpp`,
`G:\phyriad\framework\holons\include_gpu\phyriad\holons\RoutingPolicy.hpp`,
`G:\phyriad\framework\behavior\include\phyriad\behavior\PressureScore.hpp`,
`G:\phyriad\docs\planning\FG_VFI_PRIOR_ART.md`.

---

## 1. Scope, mission, and the re-asserted main objective

**Scope.** This document is the design contract for a **layered, heterogeneous
multi-GPU frame-generation architecture** inside the existing Ayama app — no new
pillar (§8, NG-4). It defines *why* the architecture takes the shape it does,
*which* existing building blocks it re-points, and *what* the phased build must
prove. It is **not** a claim that any of the `designed` layers ships today.

**Mission.** Lift Ayama's frame-gen from a single capped interpolation path to a
**three-tier holarchy** in which each GPU's value is contextual and the idle
headroom on the panel-owner GPU is converted into a gap-aware fill/refine layer.

**The re-asserted main objective (operator, this session):** beat LSFG in **every
approach the architecture allows**. A wall is treated as a challenge to break,
not a reason to stop. This objective is `aspirational` until the §10 P5 gate
measures it; **no part of this document claims it is met** (§12).

### 1.1 Non-scope

- A model-training pipeline, a neural VFI network, or any per-frame ML inference.
- A general public frame-gen product. Ayama is a personal tool at
  `0.1.0-experimental`; this is not a v1.x framing.
- A re-litigation of the design intent. The tri-tier split, refine-not-cascade,
  and the stigmergic-iGPU role are the operator + supervisor co-designed premise;
  this document builds the contract on them, it does not re-argue them.

---

## 2. The architecture is FORCED by the measured wall (not chosen for elegance)

Per [D-11](../canon/DOGMA_SPECIFICATIONS.md) (dynamic-first, measure — don't
assume), the architecture follows from a **measured** bottleneck, not a
speculative one.

**The wall (`measured` this session, Halo at ~700 source fps).** The F-thread
consume side walls at **~170 real-pairs/s**. The `--fsub` per-pair split
instrument ([`main.cpp:6269-6276`](../../apps/render_assistant/src/main.cpp))
reads the F-published microsecond atomics and reports: **flow ≈ 3.8 ms / pair ≈
4.2 ms / cpu ≈ 0.3 ms**. The CPU tail is already solved — the `--gme-gpu` offload
(1080 Ti, default-ON, bit-faithful with CPU fallback,
[`main.cpp:1079`](../../apps/render_assistant/src/main.cpp)) drove `cpu` to
~0.3 ms. The residual ~170 floor is the **F/P generation-protocol coupling** plus
the **cross-device host-bounce transfer** (no P2P).

> Measurement provenance. The 3.8/4.2/0.3 split and the ~170/s figure are this
> session's `--fsub` readings on the named workload; they are `measured` but
> are **runtime instrument readings on one scene**, not a benchmark-fairness
> entry. They MUST be reproduced under the §10 gates before any "beat-LSFG"
> ratio is asserted (the single source of truth for any published ratio is
> [`BENCHMARK_FAIRNESS.md`](BENCHMARK_FAIRNESS.md), per [D-16](../canon/DOGMA_SPECIFICATIONS.md)).

**The consequence (`measured`-derived).** In this regime the 4090 (device **A**)
runs at ~42% utilization — **~58% idle** — per the live per-adapter PDH stat
`gpu(A:NN% B:NN% G:NN%)`
([`main.cpp:6263-6268`](../../apps/render_assistant/src/main.cpp), a 1 Hz
`\GPU Engine(*)\Utilization Percentage` reading,
[`main.cpp:5142-5161`](../../apps/render_assistant/src/main.cpp)). On the default
iGPU-convert path the primary-FG layer is **disabled** —
`pfg_enabled=false` at [`main.cpp:3101`](../../apps/render_assistant/src/main.cpp)
— freeing A to capture + present only.

> Honesty on the idle figure. The ~58%/~42% split is a **runtime PDH reading**,
> not a controlled measurement this session. It is the *motivation*, not a gate
> result; P0 (§10) re-confirms headroom before sizing any new A-side or G-side
> work.

**The forcing logic.** Idle A-spare + a capped B base + a host-proximate iGPU is
exactly a heterogeneous split problem. The architecture is the smallest design
that (a) converts A's idle into useful frames without re-occupying A's capture/
present duty, (b) does not push the F-thread throughput wall directly (a flagged
dead-end, §8), and (c) keeps each device on the work it is physically best at.

---

## 3. The 3-tier holarchy — each holon's role and integration seam

The architecture is a **stigmergic holarchy** ([D-10](../canon/DOGMA_SPECIFICATIONS.md)):
three GPU holons coordinate **only through host-resident shared fields**, never by
shipping each other synthesized RGBA frames. The **sole** B→A data plane today is
the MV/SAD/mask recipe grid — confirmed first-hand: A imports `hMV_a`/`hSAD_a`
and uploads them per pair-advance
([`main.cpp:5321-5324`](../../apps/render_assistant/src/main.cpp) per the spine;
the host-bridge template is `hostMV[kGenRing]`/`hostSAD[kGenRing]` at
[`main.cpp:2360-2364`](../../apps/render_assistant/src/main.cpp)). B re-derives
its recipe from the **original capture**, not from any A frame.

| Tier | Device | Holon role | Status | Integration seam (`file:line`) |
|---|---|---|---|---|
| **G (substrate)** | iGPU | **Stigmergic substrate provider — STAGE-G1.** Today: fused convert+pack at source cadence (`igpu_convert_pack.comp`; dispatch [`main.cpp:3698`](../../apps/render_assistant/src/main.cpp) on `G.q2` [`main.cpp:3707`](../../apps/render_assistant/src/main.cpp)). **New:** a second per-source-frame pass on the same `G.q2` computing image-derived boundary/occlusion/distance fields + hosting the gap/cadence shared state. **NOT** a frame producer (full-FG = ~256 ms/frame, infeasible, NG-1). | convert = `shipping`; field producer = `designed` | New pipeline in the `use_igpu_convert` block [`main.cpp:3093-3102`](../../apps/render_assistant/src/main.cpp) (alongside `cpPipe`); dispatch after the convert dispatch [`main.cpp:3698`](../../apps/render_assistant/src/main.cpp); buffer alloc mirrors the `hostMV` host-bridge site [`main.cpp:2360-2364`](../../apps/render_assistant/src/main.cpp) |
| **B (base)** | 1080 Ti | **Heavy motion-compensated interpolation BASE layer.** Produces the accurate-but-capped MV+SAD recipe from the original captures via OFP on `Bframe[prv_f]`/`Bframe[cur_f]` ([`main.cpp:4896-4898`](../../apps/render_assistant/src/main.cpp)). The CPU holon tail (object-holon clustering + STAGE-63 shapefield chamfer + scene-memory, `object_repair` lambda [`main.cpp:3943`](../../apps/render_assistant/src/main.cpp)) runs on the F-thread and rewrites the MV/DIS fields. Walls at ~170 real-pairs/s. | base = `shipping`; tail = `shipping`; ~170/s = `measured` | OFP [`main.cpp:4896-4898`](../../apps/render_assistant/src/main.cpp); `object_repair` [`main.cpp:3943`](../../apps/render_assistant/src/main.cpp); gme via `--gme-gpu` (1080 Ti, default-ON) [`main.cpp:1079`](../../apps/render_assistant/src/main.cpp) |
| **A (embellish)** | 4090 | Carries game + capture + present, plus a **new gap-aware EMBELLISH/FILL layer — STAGE-A-FILL** on its ~58% spare: fills B's deficit up to panel Hz, masks B's cadence instability, refines quality. **Re-pointed** from the dormant `ofpA` (independent flow on original captures) to **fill/refine on the warp output, gap-aware, matte-decoupled**. | capture+present = `shipping`; fill/refine = `designed`; `ofpA` exists but off = `shipping`, disabled | Primary seam: inside the WAP command buffer between the warp dispatch [`main.cpp:5558`](../../apps/render_assistant/src/main.cpp) and the `TRANSFER_SRC` barrier + blit [`main.cpp:5576-5578`](../../apps/render_assistant/src/main.cpp); dormant `ofpA` declared [`main.cpp:2453`](../../apps/render_assistant/src/main.cpp), disabled [`main.cpp:3101`](../../apps/render_assistant/src/main.cpp) |

**The disable to lift, and how NOT to lift it.** `pfg_enabled=false`
([`main.cpp:3101`](../../apps/render_assistant/src/main.cpp)) frees A on the
default path. The new layer MUST NOT naively un-set this: the dormant `ofpA`
flows the *original captures* (init at
[`main.cpp:3069`](../../apps/render_assistant/src/main.cpp)) — the wrong input.
STAGE-A-FILL re-points to the **warp output** `wapOutA` (declared
[`main.cpp:2515`](../../apps/render_assistant/src/main.cpp); in `GENERAL` layout
at the seam [`main.cpp:5558`](../../apps/render_assistant/src/main.cpp), then
transitioned to `TRANSFER_SRC` at
[`main.cpp:5576`](../../apps/render_assistant/src/main.cpp)) — a net-new
consumption path of an existing image, not a new B→A frame plane.

---

## 4. Data flow and the stigmergic field interface (`RaFieldSet`)

```
A.thr_c capture (main.cpp:3608 per spine)
   └─> hostA sysRAM (EMH-imported on A+G)
        └─> G: igpu_convert_pack on G.q2 (main.cpp:3698)
             ├─> hostR[s]   RGBA8 unpacked  (main.cpp:2352)  ───────────────┐
             ├─> hostRP[s]  packed RGB8     (main.cpp:2353)  ──> B ingest    │
             └─> [NEW] hostFIELD[s] boundary/occ/distance  (designed)        │
                                                                             │
B.thr_f: OFP (main.cpp:4896-4898) -> CPU holon tail (object_repair 3943, gme,
   scene-memory) -> writes hostMV[gen]/hostSAD[gen] (main.cpp:2363)          │
   -> [NEW] MAY read hostFIELD[s] as a per-block prior (designed)            │
        │ F copies fields out per-gen (stigmergic ship, main.cpp:2360-2364)  │
        ▼                                                                    ▼
A.thr_p WAP: import hMV_a + hSAD_a (main.cpp:5321-5324 per spine), upload per pair
   -> warp dispatch -> wapOutA (main.cpp:5558, GENERAL)
   -> [NEW] STAGE-A-FILL reads wapOutA + hFIELD_a + GapSignal (designed)
   -> blit wapOutA -> bridge_img (main.cpp:5578) -> present
```

**`RaFieldSet` — the fixed field interface.** The new iGPU field follows the
existing `hostMV`/`hostSAD` host-bridge template verbatim
([`main.cpp:2360-2364`](../../apps/render_assistant/src/main.cpp)): **G writes; B
and A import + read locally.** It is sized at `start()` over `kCapSlots` (=4,
[`main.cpp:2351`](../../apps/render_assistant/src/main.cpp)) with a parametric
stride — no growth ([D-4](../canon/DOGMA_SPECIFICATIONS.md)). POD sketch
(`designed`, binding name `RaFieldTexel`):

```cpp
// host-resident, per-cap-slot, EMH-imported on G(write)/B(read)/A(read).
struct RaFieldTexel {           // R8 or RG8 per block; 1-2 B/px (D-8 POD)
    uint8_t  contour_dist;      // STAGE-G1: chamfer distance from silhouette rim
    uint8_t  occ_class;         // STAGE-G1: 0=interior 1=disoccl 2=occl boundary
};
```

`RaFieldTexel` is POD and stays **intraprocess** (sysRAM EMH, not cross-process
SHM): per [CANON principle 7](../../CANON.md) + [D-8](../canon/DOGMA_SPECIFICATIONS.md),
a schema `Hash128` is REQUIRED only **if** it ever crosses an IPC/SHM boundary —
the same constraint the existing `hostMV`/`hostSAD` bridges meet today.

**Reads (`designed`).** B MAY read the field as an extra prior gating the gme
dissidence mask / object clustering. A uploads it per pair-advance (mirroring the
`hMV_a` upload path) and samples it at the disocclusion fill/blend selection sites
in `wap_warp.comp` — the one-sided disocclusion-fill family already present
([`wap_warp.comp:1068`](../../apps/render_assistant/shaders/wap_warp.comp) and
the STAGE-49/50 one-sided blend at
[`wap_warp.comp:1041-1080`](../../apps/render_assistant/shaders/wap_warp.comp)).

**Why this revives the matte's right idea without its wrong execution.** The
STAGE-63 shapefield already proves the object-delimitation mechanism exists (a
chamfer distance+feature transform cast inward from the closed silhouette). The
**matte** that consumed it doubled the figure — the two-layer composite at
[`wap_warp.comp:1397-1411`](../../apps/render_assistant/shaders/wap_warp.comp)
(the crossfade source). With matte default-OFF (`use_matte=cfg.matte&&use_gme`,
[`main.cpp:3353`](../../apps/render_assistant/src/main.cpp)) and `cfg.matte`
defaulting off, the clustering + shapefield still **run** but feed the (off)
composite → orphaned. STAGE-A-FILL feeds a **sharper image-derived field directly
to the A fill layer**, with **no** matte composite (NG-5).

---

## 5. The GapSignal — how the base tells the fill WHERE and WHEN

The base (B) **tells the fill (A) where and when**; this gap-awareness is the
architecture's core mechanism. **GapSignal** is assembled from primitives that
already exist (so this is `designed` wiring, not new invention — [D-12](../canon/DOGMA_SPECIFICATIONS.md)):

- **Staleness / skip (gen-age).** F publishes `f_pair_cseq_a[gen]` /
  `f_pair_span_a[gen]` **before** `f_seq.fetch_add`
  ([`main.cpp:4732-4735`](../../apps/render_assistant/src/main.cpp)); P records
  which gen it presents via `p_presenting`
  ([`main.cpp:3583`](../../apps/render_assistant/src/main.cpp)). The age of the
  presented gen (`p_presenting` vs `f_seq`) is the "B frame is late/stale" signal.
- **Lap-escape (B skipped).** F's bounded lap-escape fires
  `stat_lap.fetch_add(1)` at
  [`main.cpp:4835`](../../apps/render_assistant/src/main.cpp) when P pins on a
  gen past the `kRingGuardSpinMax=64` ceiling
  ([`main.cpp:4829`](../../apps/render_assistant/src/main.cpp), a ~64 ms wait the
  fill masks).
- **Phase (WHEN to fill).** Under `--sync-clock` (§6) the NCO-derived phase
  ([`main.cpp:6038-6043`](../../apps/render_assistant/src/main.cpp) per spine) is
  the locked content phase; A fills into the phase where the next B gen has not
  yet published.
- **Where (spatial).** `hostFIELD` occlusion/distance tells A *where* the fill is
  risky (disocclusion bands), reusing the disocclusion logic now gated with the
  matte.

**Binding rule.** GapSignal is **read-only on A's present path**: A reads
`stat_lap` / `p_presenting` / `hFIELD_a`; A **MUST NOT** write back into the B
ring. This preserves single-writer ([D-3](../canon/DOGMA_SPECIFICATIONS.md),
[D-6](../canon/DOGMA_SPECIFICATIONS.md)).

---

## 6. Latency and cadence — refine-not-cascade, and the `--sync-clock` interplay

**Latency is the #1 design risk** — stacking FG layers stacks delay.

**Binding: REFINE, NOT CASCADE.** STAGE-A-FILL MUST be a **refine/fill of the
already-synthesized `wapOutA`**, attached at the seam
[`main.cpp:5558-5578`](../../apps/render_assistant/src/main.cpp), adding only
**sub-frame compute latency** (same A-side command buffer, same `fBridge`, no
host bounce). It MUST NOT be a second interpolation stage that needs a look-ahead
frame (which would add a full frame of delay). (`designed`; the seam's
zero-host-bounce property is `shipping` — the WAP path keeps the present on A.)

**`--sync-clock` owns the cadence (the fill consumes it, never owns it).**
`--sync-clock` (STAGE-90, **default-ON**, `sync_clock=true` at
[`main.cpp:515`](../../apps/render_assistant/src/main.cpp)) runs the NCO
accumulator `content_clock` + 2nd-order PLL `T_robust_ms` with named gains
`kScFreqAlpha=0.05`, `kScPhaseGain=0.10`, re-seat threshold `kScReseatErr=4.0`
([`main.cpp:5749-5769`](../../apps/render_assistant/src/main.cpp)). STAGE-A-FILL
**consumes** that phase
([`main.cpp:6038-6043`](../../apps/render_assistant/src/main.cpp) per spine); it
MUST NOT introduce its own clock (NG-6). The fill masks B's cadence instability
(the lap-escape pins) by presenting a refined A frame at the **locked** phase when
B's gen is stale — riding the existing PLL.

> Open validation (`unmeasured`). The sync-clock under **non-stationary** input
> (camera-turns / combat) is a known un-closed item from the design brief. If the
> PLL lags under those conditions, STAGE-A-FILL's correctness under that lag MUST
> be validated, not assumed (§10 P2 gate).

---

## 7. Adaptive headroom — yield to the game (D-20), and the honest GPU-busy gap

[D-20](../canon/DOGMA_SPECIFICATIONS.md) (graceful degradation) is binding: the A
fill layer **MUST yield to the game**. The control composition:

1. **Capability gate (`shipping`).** STAGE-A-FILL is dispatched only when
   `break_even_decide` says it pays
   ([`GpuDescriptor.hpp:94-115`](../../framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp)),
   with the degenerate-input guard
   ([`GpuDescriptor.hpp:102-104`](../../framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp))
   and the "not characterized → supervisor only" honest default
   ([`RoutingPolicy.hpp:63`](../../framework/holons/include_gpu/phyriad/holons/RoutingPolicy.hpp)).
   For an A-local refine (no cross-device transfer) the gate is near-trivial — so
   the *binding* back-off lever is the pressure feedback below.
2. **Pressure back-off (`designed`).** A consumer-side controller samples
   `PressureScore::level()`
   ([`PressureScore.hpp:94`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp);
   thresholds `kLevelGreenMax=0.30f` / `kLevelYellowMax=0.70f` at
   [`PressureScore.hpp:70-71`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp),
   level computed at
   [`PressureScore.hpp:152-154`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp)).
   Yellow → shrink the fill share / raise the gate threshold; red → drop the A
   fill participant entirely, the game reclaims the 4090.
3. **The HONEST GAP (binding to state).** `PressureScore` measures
   **CPU-runtime** pressure (rings / migrations / tick-variance / jitter), **not
   GPU-busy**. `GpuDescriptor` is a **capability** record (queried + measured
   once) with **no live utilization field**. A faithful "back off when the **4090
   is GPU-bound**" needs a **net-new GPU-busy signal** that does not exist as a
   pillar primitive today. The live `gpu(A:NN% …)` PDH stat
   ([`main.cpp:6263-6268`](../../apps/render_assistant/src/main.cpp)) is the
   candidate source, but it is a runtime PDH reading, not a pillar edge. **Until
   that GPU-pressure primitive exists, "yield when the 4090 is GPU-bound" is
   `designed`, not `shipping`.**

---

## 8. Non-goals and rejected alternatives

**Non-goals (binding):**

- **NG-1 — the iGPU is NOT a frame producer.** Full-FG on the iGPU = ~256 ms/frame
  (design brief), infeasible. G computes cheap host-resident fields only.
- **NG-2 — NO B→A frame plane.** B ships the MV/SAD/mask recipe; A re-warps
  originals. The fill layer does NOT make B send synthesized RGBA to A.
- **NG-3 — NO cascade interpolation.** STAGE-A-FILL is refine/fill of `wapOutA`,
  never a second interpolation stage needing a look-ahead frame (§6, latency).
- **NG-4 — NOT a new pillar.** Stays consumer code in `apps/render_assistant/`;
  no new pillar dependency, no forbidden pillar edge ([CANON principle 4](../../CANON.md)).
- **NG-5 — NOT reviving the matte composite.** The two-layer composite
  ([`wap_warp.comp:1397-1411`](../../apps/render_assistant/shaders/wap_warp.comp))
  doubled the figure (the crossfade) — it stays OFF. The boundary *idea* is
  revived via `RaFieldSet`, not the composite.
- **NG-6 — the fill layer does NOT own a clock.** It consumes the `--sync-clock`
  NCO/PLL phase; never a competing cadence source.
- **NG-7 — NO magic constants tied to 240 Hz.** All cadence math is parametric on
  `refresh_hz` in ms units (must scale to 500/1000 Hz — [D-11](../canon/DOGMA_SPECIFICATIONS.md)).

**Alternatives rejected (with why):**

| Alternative | Rejected because |
|---|---|
| Un-set `pfg_enabled`, run `ofpA` as-is | `ofpA` flows the original captures (init [`main.cpp:3069`](../../apps/render_assistant/src/main.cpp)), not B's output; it is independent flow, not gap-aware fill, and re-occupies A (defeats the igpu-convert freeing). |
| Revive the matte to consume the repaired masks | The matte's execution doubles the figure ([`wap_warp.comp:1397-1411`](../../apps/render_assistant/shaders/wap_warp.comp)); fix the *consumption* (field→fill direct), not the composite. |
| Full FG on the iGPU | ~256 ms/frame (NG-1). |
| A reads B's GPU-busy to decide back-off | No GPU-busy **pillar** signal exists (§7); the PDH stat is a future candidate, not a primitive today. |
| Coarse-grained F-thread pipelining for the ~170 wall | Flagged dead-end from the prior arc (STAGE-85 pipelining broke present; recorded in render-assistant-arc-legacy). STAGE-A-FILL attacks the *deficit*, not the F-thread throughput. |

---

## 9. Feature-Request gate (Q1–Q8) and prior-art / novelty assessment

### 9.1 The FR Q1–Q8 result (per [FEATURE_REQUEST_PROCEDURE.md](../canon/FEATURE_REQUEST_PROCEDURE.md))

This capability is being assessed against the FR gate. The honest verdict:
**most of STAGE-A-FILL is consumer code, not an FR** — it re-points existing app
machinery. The *only* candidate FR is the GPU-busy pressure edge (§7.3). The gate:

| # | Criterion | Result |
|---|---|---|
| **Q1** Generalizable | The GPU-busy-pressure edge generalizes (any consumer co-resident with a GPU workload — Ayama, a future Holonium chunk-mesher, any render-assistant). The *fill layer itself* is Ayama-specific → not generalizable → consumer code, not an FR. **PARTIAL: only the pressure edge passes.** |
| **Q2** Pillar-compatible | A GPU-busy reading fits `behavior` (runtime observation) or `gpu` (capability/measure); no circular dep. **PASS for the edge.** |
| **Q3** Structural-benefit threshold | A net-new live-utilization signal is meaningful structuring. The fill wiring is not. **PASS for the edge only.** |
| **Q4** Dogma-aligned | §10 checklist; the edge upholds D-1..D-22. **PASS.** |
| **Q5** Sound-base precondition | The base (B layer, OFP, host-bridge ship, sync-clock PLL) is `shipping` and correct. **PASS.** |
| **Q6** Non-duplication | Verified first-hand: `PressureScore` has no GPU-busy field; `GpuDescriptor` has no live-utilization field (§7.3). The PDH stat is app-local, not a pillar primitive. **PASS — the gap is real.** |
| **Q7** Measurable benefit | The acceptance measurement is the §10 P3 gate (GPU-bound scene → fill share → 0, game budget never starved). **DEFINED, unmeasured.** |
| **Q8** Self-contained spec | Deferred to the sibling impl-strategy doc + a future `FR_<pillar>` pair if the edge is lifted. **PENDING.** |

**Verdict (`designed`):** STAGE-A-FILL ships as **consumer code** (NG-4). The
GPU-busy pressure edge is a **candidate FR** to be filed separately (its own
`FR_<pillar>` + `_IMPL` pair) if and when P3 proves the need; it is **not** lifted
into a pillar by this document.

### 9.2 Prior-art and novelty — HONEST, web-unverified

**Verified scope of the in-repo dossier** ([`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md)):
it is a VFI-**quality** dossier (measurement + ghosting cure). It does **not**
establish the product landscape for multi-frame gen, FG-on-FG stacking, or
multi-GPU FG. **Binding fabrication rule:** do **not** cite `FG_VFI_PRIOR_ART.md`
for any of the landscape/novelty claims below.

| Claim | Honest assessment | Bucket / action |
|---|---|---|
| Multi-frame generation exists (DLSS4 MFG, LSFG X3/X4) | Known commercially; internals undocumented in-repo. | `conjectured`-internals — **needs first-hand web check**; do not cite the dossier. |
| FG-on-FG stacking done informally by users | Plausible, not in the dossier. | **needs web check** before any "known" claim. |
| External-capture FG | LSFG already does frames-only capture FG. NOT a Phyriad novelty. | NOT a novelty claim; field comparison **needs web check**. |
| **Heterogeneous multi-GPU split** (hard MC on B + cheap gap-aware fill on A-spare + stigmergic iGPU substrate) | The genuine candidate novelty; not assessed anywhere in-repo for FG. | `conjectured`-novel — **needs first-hand web check**; the least-grounded, flagged loudest. |
| Base-tells-fill WHERE/what (gap-awareness) | Mechanism grounded in the existing motion-masked / one-sided fill code; novelty vs MEMC/FRC patents not assessed. | `designed`-technique; novelty **needs web check** vs patents. |
| Stigmergic-iGPU substrate | Dossier silent. | **entirely needs web check**; least grounded. |

All novelty assertions are `conjectured` pending an independent first-hand web
pass (FDP §2.3 levels would apply once that pass runs). See §12.

---

## 10. Dogma compliance + phased roadmap with gates

### 10.1 Dogma D-1..D-22 compliance

| Dogma | How upheld (binding) |
|---|---|
| **D-1** max perf is the point | Justified by the `measured` ~170/s wall + ~58% A idle; the win MUST be `measured` (D-12/Q7), not asserted. |
| **D-2** zero-alloc hot path | The fill pass + `RaFieldSet` use caller-owned arenas sized at `start()`; no per-tick/per-pair `new`. |
| **D-3** lock-free hot path | GapSignal read-only on A; reuses the seq-cst `f_seq`/`p_presenting`/`stat_lap` channel — no new lock. |
| **D-4** fixed-capacity at start | `hostFIELD[kCapSlots]` ([`main.cpp:2351`](../../apps/render_assistant/src/main.cpp)), parametric stride, no growth. |
| **D-5** HAL discipline | App is consumer code; any new atomic routes through `hal::`; reuses existing seq-cst publishers. |
| **D-6** cache awareness | Single-writer-per-slot for `hostFIELD` (G writes, B/A read). |
| **D-7** noexcept boundary | App, not a public pillar API; the pillar calls it uses are already `noexcept`. |
| **D-8** POD boundaries | `RaFieldTexel` is POD, intraprocess; schema-hash only if it ever crosses IPC/SHM (§4). |
| **D-9** topology scalability | GPU-side; the refresh-Hz parametricity (D-11) is the analogue. |
| **D-10** stigmergic coordination | The architecture's spine: G writes a field, B+A read + decide locally; no central scheduler, no B→A frame messaging. |
| **D-11** dynamic-first, measure | Time-units (ms), parametric on `refresh_hz`, no magic 240; iGPU field-pass cost + A-spare break-even MUST be measured before sizing. |
| **D-12** no speculative generality | Substrate checked FIRST: shapefield, host-bridge ship, `ofpA`, `break_even_decide`, `PressureScore` already exist — the design re-points them. |
| **D-13** correctness before speed | The fill MUST be correctness-preserving; a faster ghosted frame is worthless. Needs the FG-quality test-field as oracle. |
| **D-14** honest benchmarks | Beating LSFG claimed only via a fair held-out measurement; report the loss region; one source of truth for ratios (D-16). |
| **D-15** tests + TSan + perf gate | The GapSignal concurrency is TSan-verified; non-regression gate runs before any "safe" claim. |
| **D-16** single source of truth | One `RaFieldSet` schema, one GapSignal definition; both docs link, never duplicate. |
| **D-17** DX | Default-OFF flag, opt-in (mirrors matte/subpel defaults). |
| **D-18** API duality | App-level; n/a. |
| **D-19** portability | The iGPU path is the platform-sensitive piece (EMH); gated on `have_igpu` with the A-convert fallback. |
| **D-20** graceful degradation | The §7 pressure back-off IS this dogma; the honest GPU-busy gap is stated, not hidden. |
| **D-21** doc discipline | Both planning docs follow lifecycle + register in `FORMAL_DOCUMENTS_REGISTER.md`. |
| **D-22** memory tiering | Session-continuity; the render-assistant-arc memory is updated per the tiering protocol when this lands. |

### 10.2 Phased roadmap (P0..P5)

| Phase | Scope | Validation gate (binding) |
|---|---|---|
| **P0 — field producer** | Stand up `hostFIELD[kCapSlots]` + the STAGE-G1 iGPU pass on `G.q2` (no consumer). | Field bytes correct vs a CPU reference (D-13 oracle); iGPU pass cost **measured** at target source-fps + `gpu(G:..)` headroom confirmed; no source-cadence regression. |
| **P1 — A reads field, refine-only** | Re-point a minimal STAGE-A-FILL refine at the seam [`main.cpp:5558-5578`](../../apps/render_assistant/src/main.cpp) reading `hFIELD_a` + `wapOutA`; NO gap fill yet. | Sub-frame latency only (no look-ahead) **measured**; FG-quality held-out test-field shows no quality regression; D-13 correctness. |
| **P2 — GapSignal** | Wire `stat_lap` / `p_presenting` gen-age as the read-only GapSignal; A fills the B-deficit gen at the sync-clock phase. | cons/s deficit visibly filled to panel Hz; lap-escape pins masked; NO new clock; sync-clock lock preserved (incl. the §6 non-stationary check). |
| **P3 — adaptive back-off** | `PressureScore::level()` (+ the candidate GPU-busy edge) drives shrink-on-yellow / drop-on-red. | D-20: GPU-bound scene → fill share → 0, game frame budget never starved (**measured**); the GPU-busy gap documented if the edge is not yet first-class. |
| **P4 — B-side field consume (optional)** | B reads `hostFIELD` as a prior gating gme dissidence / clustering. | Improves base-layer boundary accuracy (held-out); no F-thread regression below ~170/s. |
| **P5 — beat-LSFG measurement** | Fair held-out A/B vs LSFG across the approaches the arch allows. | D-14 honest benchmark; report the loss region; one source of truth for the ratio ([`BENCHMARK_FAIRNESS.md`](BENCHMARK_FAIRNESS.md)). |

Each phase: the non-regression gate runs **before** any "safe"/"done" claim;
distinguish "Not Run" (not built) from a real "Failed"; confirm own-vs-preexisting
on any failure.

**Status (2026-06-15) — what actually landed (honest divergence from the P-plan; `measured`):**
- **P0** — DONE (`426915e`): STAGE-G1 iGPU contour field; byte-correct vs the CPU oracle; G 42→78%.
- **P1** — DONE (`8ef7f53`): STAGE-A-FILL `--afill` field visualizer (phase-advected); operator-eye-validated the
  field lands ON the object — which localized the crossfade root cause = `(1−t)·motion` misalignment.
- **P2b** — DONE (`53f2cf4`): STAGE-90 `--bg-snap` — the gravity/disocclusion fix (image-field-gated background-MV
  snap + a reveal-side static-background fill). NOT in the original P-list; the operator's gravity finding drove it.
  Default-OFF, byte-identical-off. Residual: a soft band difuminado (refinable).
- **P2 (the deficit)** — DONE (`8eda61d` + `306c5e7`), but the ROOT CAUSE was **NOT** the GapSignal-refine this row
  designed. The high-source freeze was the **4-slot capture ring (`kCapSlots`) lapping the reals** before WAP could
  interpolate (`prev_slot_safe` failed ~every tick at source>~170 → freeze-at-cur → "show-reals+repeat"). Fix:
  `kCapSlots` → a runtime **auto-sized `cap_slots`** (≤32; from the source/flow/resolution gap, NOT the CPU) with the
  in-order ingest backlog decoupled (`kIngestBacklog=3`). MEASURED (Halo): 240Hz `frz 242→13/s`; 480Hz `frz 480→40`,
  `uniq 255→335`. The GapSignal-fill idea (STAGE-91 `--asw` bounded forward extrapolation, `cur[uv−extrap·mv]` when the
  sync-clock phase overshoots) WAS built but turned **REDUNDANT** (the ring fix eliminated the freezes it targeted) →
  retained default-OFF for the extreme source>500. So P2's gate ("deficit filled to panel Hz") is MET — via the ring,
  not the designed refine. The `--asw` per-pixel refine of `wapOutA` (the literal P2-design) was deliberately NOT
  pursued past the gate (it would be dead code).
- **P3 / P4 / P5** — pending. P5 (fair vs-LSFG) is the objective's gate; its LSFG side needs the camera (LSFG's output
  is digitally uncapturable). Open throughput item: the present-thread `slip` ABOVE the monitor rate (~1.5 @480Hz; the
  HardwareTopology/Scheduler pillar — thread-core pinning — is the right tool, a separate concern from the ring).
- NOTE: the `main.cpp:NNNN` seam/line refs in P0–P2 above are **STALE** (the file grew ~218 lines since this doc was
  written); treat them as semantic pointers, not literal addresses.

---

## 11. Shared spine + fixed vocabulary (anti-drift — binding)

Both this doc and the sibling impl-strategy doc are written against one spine and
MUST use these names **identically** (neither may rename):

| Concept | FIXED name | Anchor |
|---|---|---|
| iGPU field-set | `RaFieldSet` / buffer `hostFIELD[kCapSlots]` / imports `hFIELD_g`/`hFIELD_b`/`hFIELD_a` | host-bridge template [`main.cpp:2360-2364`](../../apps/render_assistant/src/main.cpp) |
| Field texel POD | `RaFieldTexel` (`contour_dist` + `occ_class`) | §4 |
| iGPU field pass | **STAGE-G1** (contour/occlusion/distance on `G.q2`) | [`main.cpp:3698`](../../apps/render_assistant/src/main.cpp) |
| A fill/refine layer | **STAGE-A-FILL** (gap-aware refine of `wapOutA`) | seam [`main.cpp:5558-5578`](../../apps/render_assistant/src/main.cpp) |
| B base layer | **base MC layer** (existing OFP + holon tail) | [`main.cpp:4896-4898`](../../apps/render_assistant/src/main.cpp) |
| Gap signal | **GapSignal** = {gen-age from `p_presenting`/`f_seq`, `stat_lap`} | [`main.cpp:3583`](../../apps/render_assistant/src/main.cpp) / [`main.cpp:4835`](../../apps/render_assistant/src/main.cpp) |
| Cadence owner | `--sync-clock` NCO/PLL (`content_clock`) — the fill consumes, never owns | [`main.cpp:5749-5769`](../../apps/render_assistant/src/main.cpp) |
| Back-off control | **PressureScore back-off** + **the GPU-busy honest gap** | [`PressureScore.hpp:94`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp) |
| The rejected composite | **the matte two-layer composite (OFF, NG-5)** | [`wap_warp.comp:1397-1411`](../../apps/render_assistant/shaders/wap_warp.comp) |

**Anti-drift rule (binding, [D-16](../canon/DOGMA_SPECIFICATIONS.md)):** changing
any name, interface field, or non-goal MUST be made to the spine first, then
propagated to **both** docs in the same change — never diverge silently.

---

## 12. What this document does NOT claim

- It does **not** claim STAGE-A-FILL beats LSFG — that is the P5 gate (`aspirational`
  until measured).
- It does **not** claim the heterogeneous multi-GPU split, the stigmergic-iGPU
  substrate, or external-capture FG are **novel** versus the commercial field —
  all are `conjectured` and **web-unverified**, flagged for a first-hand web pass
  (§9.2). No citation in §9.2 is presented as established fact.
- It does **not** claim the iGPU has the headroom for STAGE-G1 — the ~58%/~42% A
  figures are runtime PDH readings, not a controlled measurement (§2); the P0
  gate confirms G headroom before sizing.
- It does **not** claim "yield when the 4090 is GPU-bound" ships today — the
  GPU-busy pillar primitive does not exist; that capability is `designed`, with
  the gap stated in §7.3.
- It does **not** claim any `designed` layer is implemented. At
  `0.1.0-experimental`, every `designed`/`aspirational` item here is **spec, not
  code**; only the items tagged `shipping`/`measured` reflect the current build.
- It does **not** lift a new pillar (NG-4) or amend any dogma.
