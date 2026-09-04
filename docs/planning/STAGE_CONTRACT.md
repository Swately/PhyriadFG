# STAGE_CONTRACT — the six stages of PhyriadFG, as module boundaries with declared data contracts

> **Diátaxis type:** reference / explanation (the contract every restructure stage binds to).
> **Status:** **`approved`** (operator, 2026-09-03: "Sí adelante" — the six stages, the two planes, the
> order R0–R6 and the four §6 decisions as proposed). NOT an implemented layout: it is the boundary
> definition every restructure stage binds to from here on. Every "today" site was read first-hand this session (donor: `c043780` + the E1 working tree;
> base: `F:\Phyriad\apps\minimal_fg`, 1,560 / 549 lines). MUST / SHOULD / MAY are BCP-14.
> **Serves:** the operator's directive (2026-09-03): restructure the donor in place, by stage, with the
> base as the blueprint and the seam engine as the one adopted piece. The layer contract inside stage 5
> is the ATF-released LAYERTAB (`aap/A3_CHOSEN_DESIGN.md`) and is not re-opened here.
> **Spine:** [`ACTION_PLAN.md`](ACTION_PLAN.md) S4 — this document is the boundary definition the
> re-aimed `CONVERGENCE_*` triad will cite at every edit site.

---

## 0 · The rule that makes it a structure

**A stage is a module with one input contract and one output contract. A thread is a container that
runs one or more stages in order. Nothing crosses a stage boundary except the contract types.**

Today the module boundary is the THREAD (`run_capture`, `run_flow`, `run_present`), and the contract
is `FgContext` — 326 lines of references to `main()`'s locals, ~230 fields, with cross-thread
couplings that are real but undeclared (§3). The shader's boundary is the OVERRIDE (53 gates over one
`result`). This document replaces both with six stages and two planes:

```
 OS ─► [1 CAPTURE] ─RawFrame─► [2 INGEST] ─RealFrame─► FrameRing ─Pair─► [3 FLOW] ─FlowSet[gen]─► FlowRing
                                                │                                                 │
                                          arrivals (c_seq, t_cap)                                  │
                                                ▼                                                 ▼
                                          [4 CLOCK] ─Phase(gen, pair_c, t_use, dup/drop)─► [5 GENERATE] ─GenFrame─► [6 PRESENT] ─► screen
                                                ▲                                                                       │
                                                └──────────────────────── FlipStats (feedback) ─────────────────────────┘
 planes (read contracts, never write stage state):  CONTROL (Config → resolve → registry → arm inputs; UI model)
                                                    INSTRUMENT (csv · qdump+ · --layer-dump · --sg-dump · overlay · phaselog · latency-trace)
```

Threads as containers (unchanged count, unchanged priorities): **C** runs 1 → 2; **F** runs 3;
**P** runs 4 → 5 → 6. The base's loop (`apps/minimal_fg/src/main.cpp:1220–1460`) is the target SHAPE
of P: `for each tick: select → execute the graph → present` — with stage 4 inserted before the
graph, which the base lacks (its `t` is `0.5f`, `:801`).

## 1 · The contract types (the ONLY things that cross a boundary)

| Type | Producer → consumer | Fields (today's names in parentheses) |
|---|---|---|
| **`RawFrame`** | 1 → 2 | a host-staged buffer or D3D11 staging slot (`Astage` / `wgc_ctx->ring[]` / `raw_astage_*`), native size + format (`NAT_W/NAT_H/nat_bpp`, `IS_HDR`, rot180), `t_cap_ms`, a monotone raw index (`raw_seq` / `wpub`) |
| **`RealFrame`** (in the **`FrameRing`**) | 2 → 3, 2 → 4 | the WORKING VK image at `WW×WH` (`hR_a[s]` / `Bframe[s]` today; one image per slot), slot `s = c_seq % cap_slots`, **`c_seq`** (seq_cst publish, `capture.cpp:792`), `t_cap_ms`, `t_pub_ms` (`RealSlot`). Ring depth `kCapSlots = 32`, backlog `kIngestBacklog = 3`. **Single producer (C), two readers (F for pairs, P for arrivals).** |
| **`Pair`** | FrameRing → 3, 5 | `(A, B) = (slot(c−1), slot(c))`, `cur_c`, `span` (frames skipped under pressure), `t_cap` of B |
| **`FlowSet[gen]`** (in the **`FlowRing`**, `gen = f_seq % NS`) | 3 → 5, 3 → 4 | MV fwd (W/8 · RG16F, `hostMV`/`wapMVA`), SAD (RG16F: `sad_best, sad_zero`, `hostSAD`), MV bwd (`hostMVB`), gme model fwd/bwd (6 floats + valid: `f_pair_gme_a`, `f_pair_gme_valid_a`, `f_pair_gme_bwd_a`), dissidence fwd/bwd (`hostDIS`, `hostDISB`), persistence (`hostPER`), candidates (`hostC2`), `mv_target` (prev-pair MV), `bwd_valid` (`f_pair_bwd_valid_a`), the pair identity (`f_pair_cseq_a`, `f_pair_slot_a`, `f_pair_tcap_a`, `f_pair_span_a`, `f_pair_n_a`), the mass stats (`f_pair_mfwd_a`, `f_pair_mbwd_a`, `f_pair_disp_a`). Published by the seq_cst **`f_seq.fetch_add`** (`flow.cpp:1777` serial, `:2191` pipelined). Consumer position `p_presenting` (the lap guard, `flow.cpp:1858–1878`). |
| **`Phase`** | 4 → 5, 4 → 6 | `gen` (the selected set), `pair_c`, `t_use ∈ [0,1]` (`present.cpp:2370–2497`, FINAL after the backwards clamp), `t_display`, `disp_src` (source-time depicted, `:2634`), the tick decision `{warp, dup, drop, decimated}` |
| **`GenFrame`** | 5 → 6 | the interpolated image (`wapOutA`, `WW_warp×WH_warp`), the contract hash (LAYERTAB), the `arm` mask used, `gpu_us` of the record |
| **`FlipStats`** | 6 → 4 | `sync_qpc`, `present_count` (`PresentSurface::last_flip_qpc()`), `ps_ok/timeout/err`, device-loss latch |
| **`ArmInputs`** | 3 → CONTROL → 5 | the per-generation validity booleans (`gme_ok`, `bwd_ok`, `matte_ok`, `appear_ok`, `commit_ok`) — derived from `FlowSet[gen]` fields, never from P-local state |

Rules: a contract type is a **plain struct** (no references to `main()` locals); a ring publishes with
ONE seq_cst counter and its consumer reads the counter before the fields (the existing discipline,
kept); every field of every type is named in a header the INSTRUMENT plane can serialise (`--qdump+`
writes `Pair + FlowSet + Phase + GenFrame`, MOTION_TRUTH T1).

## 2 · The six stages

### Stage 1 — CAPTURE (thread C, backend-specific, no Vulkan)

- **Purpose:** acquire the composed desktop / a window from the OS at the OS's cadence; never cap the
  source; never block the OS on us.
- **In:** the OS (WGC `FrameArrived` ring, `wgc_ctx.hpp`; DDA `AcquireNextFrame`, `capture.cpp:307`).
  **Out:** `RawFrame`.
- **Invariants:** zero foreign-PID affinity/priority/limit calls (MR-5); a stale/late frame is DROPPED
  to newest, never queued unboundedly (`drop-to-newest`, `fg_context.hpp:38–45`); `ACCESS_LOST` re-arms
  (`capture.cpp:59`); rotation is detected here and REPORTED, corrected in stage 2 (`:250–260`).
- **Today (donor):** `run_capture` `capture.cpp:185–~800` (WGC branch `:409–560`, DDA branch `:286–404`),
  `capture_init.cpp` (528). **Base:** `main.cpp:270–336` (WGC), `:337–470` (DDA), the `CapCtx` ring.
- **Does NOT belong here (today it does):** the convert dispatch (`capture.cpp:706–714`, stage 2); the
  PLL arrival-delta counters fed to the clock (`wgc_ctx->arr_delta_us`, `:557`) — they are stage 4 INPUT
  data produced here: keep the measurement, move the ownership of its meaning to stage 4.
- **Isolation test:** `tools/capture_dump` replay → identical `RawFrame` index/timestamp sequence.

### Stage 2 — INGEST (thread C, or the convert worker under `--ingest-async`)

- **Purpose:** turn a `RawFrame` into the working `RealFrame` exactly once per real frame: format
  (BGRA/HDR scRGB tone-map, `hdr_convert.comp`), rotation (`rot180`), scale to `WW×WH`, the optional
  iGPU pack path (`igpu_convert_pack.comp`), then PUBLISH into the `FrameRing`.
- **In:** `RawFrame`. **Out:** `RealFrame` + the seq_cst `c_seq` publish.
- **Invariants:** **one upload per real frame** (the base violates this: both anchors re-uploaded per
  present tick, `main.cpp:1301–1330` — the CONVERGENCE C2 defect); the cross-API boundary is a CPU
  buffer, never a keyed mutex on the FG's Vulkan path (the BF6 lesson); the publish stamps `t_pub_ms`
  immediately before `fetch_add` (`fg_context.hpp:33–36`).
- **Today (donor):** the serial tail `capture.cpp:~600–792` (convert `:706–714`, publish `:792`), the
  async worker `run_convert_worker` `:805–~971`. **Base:** `map_copy` + `up_load` (`main.cpp:1196–1216`).
- **Does NOT belong here:** nothing extra today; the P-side `upPipe` upscale (`present.cpp`, `use_upscale`,
  `UP_W/UP_H`) is stage 6's output scaling, not ingest.
- **Isolation test:** a zoo frame with the 16-bit frame-ID barcode (MOTION_TRUTH T2) in → the published
  `RealFrame` decodes the same ID; `uploads/s == in ± 1` at any present rate.

### Stage 3 — FLOW (thread F)

- **Purpose:** from a `Pair` (and the previous set, the temporal prior), produce the `FlowSet`: the
  motion source and every per-pair field the generation reads.
- **In:** `Pair` (+ `FlowSet[gen−1]` for the prior, `mv_target`). **Out:** `FlowSet[gen]` + `f_seq`.
- **Invariants:** the MV path is the vendored `OpticalFlowPipeline` (pyramid block-match, MV+SAD at W/8;
  `framework/render/vulkan/include/phyriad/render/vulkan/OpticalFlowPipeline.hpp:31`) — A0 §3 "reuse,
  not rewrite"; every field in `FlowSet` is produced HERE or is absent (validity bit 0) — stage 5 never
  computes a flow-class field; the GPU passes of this stage form ONE SG graph on F's queue (the seam:
  declared reads/writes, derived barriers, cull-when-off).
- **Today (donor):** `run_flow` `flow.cpp:473–2234`: OFP fwd/bwd, `gme_fit_affine` on CPU (`:44`, a
  declared host readback of MV+SAD → model upload), the GPU gme path (`GmePipe`, `:357–420`),
  `mv_smooth`, the candidates/dissidence/persistence fields, the publish (`:1773–1777`, `:2186–2191`).
  **Base:** the `mc_interp` pass's OFP call (`main.cpp:791–802`) — flow and warp fused in one pillar call.
- **Does NOT belong here (perceptual layers, parked S5):** `object_repair` (`flow.cpp:885–1245`),
  `mem_advect/merge/refresh` (`:1275–1380`), `consume_wap` (`:1396–1809`) — the holon family. They become
  **FLOW-stage `kind = P` rows** of the registry (each a declared SG pass over `FlowSet` fields), off by
  default, re-entering under the net-gain gate. Also misplaced TODAY in stage 5: the 3×3 MV consensus /
  median recorded in P's bridge command buffer (`present.cpp:779`, `medPipe`, `use_mv_median`) — a
  FLOW output computed in the wrong thread; it becomes a FLOW row (or an MVCOND row of stage 5), never
  a P-side ad-hoc pass.
- **Isolation test:** the zoo pair with a known translation → MV within ε of the analytic (the scorer's
  Mode A already does this: `pan` 99.0 dB, measured 2026-09-02); the SG `--sg-dump` of F's graph
  byte-identical across two runs.

### Stage 4 — CLOCK (thread P, pure CPU, the heart of "exact motion")

- **Purpose:** decide, for every present tick, WHICH pair and WHAT phase the generated frame depicts —
  and whether this tick warps, duplicates, drops or is decimated. Exact motion is exactly this stage
  being right.
- **In:** arrivals (`c_seq`, `t_cap_ms`), the tick period (panel rate), `f_seq` (which sets exist),
  `FlipStats` (feedback), the source-rate estimate. **Out:** `Phase`.
- **Invariants:** a value type with NO GPU dependency (`struct PhaseClock`), so it is testable on a CPU
  with synthetic arrivals; monotone content (never step backwards, `present.cpp:2904`); the loop gains
  are named constants (`kScFreqAlpha`, `kScPhaseGain`, `kScReseatErr`), never re-tuned inside a port;
  the NCO + 2nd-order PLL (`:1558–1596`, `:1964–1976`, `:2022–2061`) is the ONE phase source — the
  "phase OVERRIDE" swap (`:2203`) is the only place the source changes, and it is printed.
- **Today (donor):** the numbered steps of the OUTPUT-CLOCK loop: 1 tick boundary (`:1662`), 2
  `src_interval` (`:1930`), 3 select the set (`:1978`), 4 phase within the window (`:2188`), the `t_use`
  derivation (`:2370–2497`), 5 nearest pre-generated phase (`:2889`), 6 monotonicity (`:2904`), 8
  bookkeeping (`:2950`). The vblank phase-lock (`--pace-vblank`, `:1734–1790`) is the clock's grid
  origin. **Base:** none (`t = 0.5f`).
- **Does NOT belong here (phase LAYERS, parked S5, re-enter as rows):** `--phase-norm` (`:2394–2427`),
  the opening-ease cubic (`:2431–2468`), the dispersion Schmitt latch (`:2315`), the `realized_mult`
  governor (`:2397`) and its over-production drop (`:2485`) — policies over the phase, not the phase.
  The laser mass-feedback (`:1465`, `:2583`, `:2664`) is a GENERATE→CLOCK feedback of a perceptual
  layer: parked. Decimation (`:2062`, `:1500–1530`) is a stage-6 slot policy that READS `Phase`.
- **Isolation test (the one that does not exist and matters most):** a CPU test feeding synthetic
  arrivals (60 fps with ±2 ms jitter, then a 60→30 fps step, then a dropped frame) and a 240 Hz tick →
  assert `t_use` monotone within a pair, lock within N ticks, phase error bounded, re-seat only past
  `kScReseatErr`. The MOTION_TRUTH phase error `err_px/|v|` is this stage's live measurement.

### Stage 5 — GENERATE (thread P, GPU; the LAYERTAB kernel on the SG seam)

- **Purpose:** `Pair + FlowSet[gen] + Phase.t_use → GenFrame` — the 35-line core plus the layers that
  earn their place, composed under the declared order `MVCOND → SAMPLE → fg_core() → COMPOSE`.
- **In:** `Pair` images, `FlowSet[gen]` images, `Phase.t_use`, `CorePush{residual_ceil, improvement_frac,
  agreement_threshold, t, arm_mask}` (20 B), `LayerParams` UBO (config-time), the registry's
  specialization constants. **Out:** `GenFrame`.
- **Invariants:** the core is `fg_core()` = `wap_warp.comp:496–529, 637, 679` verbatim; one `imageStore`
  with one source expression (G1); every layer is a registry row (`src/layers/layer_table.def`) with a
  stage, a rank, declared reads/writes and a printed override bit; the GPU work of this stage is ONE SG
  graph on P's queue (fused rows = 1 dispatch; `kind = P` rows = +1 pass +1 derived barrier); **no field
  of flow class is computed here** (stage 3's invariant, mirrored); the arm mask is derived from
  `FlowSet[gen]` validity through `ArmInputs`, read at ONE point of the tick (XR5).
- **Today (donor):** `wap_warp_present` (`present.cpp:879–1402`: the 58-float push `:1119–1150`, the gate
  derivation `:960–1118`, the dispatch, the blit barriers `:1215–1218`), `wap_upload` (`:713–836`: the
  FlowSet host→G upload — a multi-GPU heritage copy that on ONE device is a transfer that should not exist:
  under the contract `FlowSet` images are GPU-resident and shared; the upload exists only when stages 3 and
  5 run on different devices), `shaders/wap_warp.comp` (1,336), `warp_blend_init.cpp` (371). **Base:** the
  same `mc_interp` pass (fused with flow), `optical_flow_warp.comp` via the pillar.
- **Does NOT belong here:** `--afill` (`wap_fill.comp`), the overlay RMW (`:1188`, INSTRUMENT), the
  qdump readback (`:1315–1357`, INSTRUMENT), the upscale (`upPipe`, stage 6), the MV median (stage 3).
- **Isolation test:** M4 — the T6 CPU reference warp on the `--qdump+` record: byte-identical or explained;
  `--layer-dump` prints the resolved chain + contract hash; the pipeline-variant SPIR-V diff shows a
  disabled row's body absent.

### Stage 6 — PRESENT (thread P; pacing, drop, the surface)

- **Purpose:** put `GenFrame` (or a real frame on `--rfp` / a dup tick) on the panel at the target time;
  DROP when it would land inside the next real frame's guard band; never back-pressure the source; never
  block the generation fence; report `FlipStats`.
- **In:** `GenFrame` or `RealFrame`, `Phase` (the tick decision), the pacing target. **Out:** the screen,
  `FlipStats` → stage 4, the per-tick telemetry row (INSTRUMENT).
- **Invariants:** own-window `PresentSurface` (`Style::OwnWindow`; no cross-process present — MR-7); the
  async slot machinery (slot split + back-slot guard + `vk_live`-wrapped polls — MR-1, reconciled from
  `FG_SATURATION_STABILITY` / `REAL_FAST_PATH`); the slot is provisioned at init; the drop decision is a
  P-local scalar compare; the output scaling to the panel (`upPipe`, `UP_W/UP_H`, `bridge_w/h`) lives here.
- **Today (donor):** the PresentSurface creation `present.cpp:377–460`, the bridge blit + submit `:564`,
  `bridge_present` / `bridge_present_src` (`:466–507`, `:654–693`), `rfp_present` (`:1416–1458`), the
  async-present preamble `:860–930`, `--shallow-queue` `:1361`, step 7 "present exactly one frame" (`:2593`,
  `:2921`), the decimation gate (`:2062`), the per-second stats (`:2954`). **Base:** submit → fence wait →
  `surface.submit()` → `Sleep(2)` (`main.cpp:1414–1460`) — naive, the shape without the policy.
- **Does NOT belong here:** the clock (stage 4 — today interleaved in the same loop body); the warp
  dispatch (stage 5); the PDH/NVML GPU% sampling (`:282–350`, INSTRUMENT/CONTROL governor input).
- **Isolation test:** `FlipStats` vs target grid (`disp_phase`/`disp_src`, the TB-C9 fluidity scorer);
  drops/s > 0 and real-frames-dropped = 0 under `tools/gpu_load.exe`; a forced TDR → clean `g_quit`.

## 3 · The couplings that exist today and are NOT in the picture above (declare or remove)

| Coupling (verified site) | Direction | Disposition |
|---|---|---|
| `g_gov_floor` — the util-driven tier floor computed by P, applied by F (`flow.cpp:1459–1464`) | 6 → 3 | a **CONTROL-plane signal** (`GovernorFloor`), declared, one writer (P's governor), one reader (F's ladder) |
| `src_interval_us` — P keeps it honest, F reads it for live-N feasibility (`present.cpp:1930`, `flow.cpp:662`) | 4 → 3 | a `Phase`-adjacent field published by stage 4 (`SourceRate`), declared |
| `p_presenting` — the FlowRing lap guard (`flow.cpp:1858–1878`) | 5/6 → 3 | the **FlowRing consumer position**, part of the ring contract, not a stage output |
| `present_cost_us`, `stat_*` atomics (F reads P's present cost; P reads F's flow cost) | 3 ↔ 6 | INSTRUMENT plane; readers never act on them except the governor (CONTROL) |
| `wap_upload` — P uploads F's host buffers to G's images (`present.cpp:713–836`) | 3 → 5 | exists ONLY when stages 3 and 5 are on different devices; single-device = shared GPU-resident `FlowSet` images, zero copy (a CONVERGENCE stage, not a layer) |
| MV median/consensus in P's bridge cmd (`present.cpp:779`) | 5 computes a 3-field | moves to stage 3 (a FLOW row) — the only stage that may produce a flow-class field |
| the overlay RMW on the output (`present.cpp:1188`; base `main.cpp:1340–1358`) | INSTRUMENT writes 5's output | a declared post-GENERATE INSTRUMENT pass in the SG graph (its barrier derived, not hand-written) |
| `ra_fgprotect_demote_p` and the MMCSS/pin calls (`phyriad::hw`) | CONTROL → threads | thread-container policy (CONTROL), never inside a stage |

## 4 · The planes

- **CONTROL:** `Config` (parse) → `resolve_config()` (the single owner of non-layer cascades — RESTRUCTURE
  §2.2, kept) → the layer registry (`src/layers/`, LAYERTAB: `LayerConfig`, spec constants, `LayerParams`,
  `layer_arm_mask(ArmInputs)`) → the UI model (`--layer-model-json`; the ~150 host flags stay hand-written
  until migrated as `HOST` rows) → the governor (tier floor, fg-protect). It READS `FlowSet` validity and
  `FlipStats`; it WRITES only its own outputs (`ArmInputs`, `GovernorFloor`, thread policy).
- **INSTRUMENT:** `--csv` (per-tick row from `Phase + GenFrame + FlipStats`), `--qdump+` (the replay
  record: `Pair + FlowSet + Phase + GenFrame`), `--layer-dump`, `--sg-dump` (per stage graph), `--phaselog`,
  `--latency-trace`, the overlay, `--outdump`. It reads contract types only. **The MOTION_TRUTH instrument
  is this plane's consumer:** if a fact is not in a contract type, the instrument cannot measure it — which
  is the test of whether the contract is complete.

## 5 · What the restructure does, in this vocabulary (the re-aim of CONVERGENCE, for approval)

| Order | Move | Why first / risk |
|---|---|---|
| R0 | CONTROL: the registry (CONVERGENCE C0 = M-C1), `wap_warp.comp` untouched | zero product risk; makes stage 5 and the flag surface legible |
| R1 | **Stage 4 extracted as `PhaseClock` (CPU value type) + its synthetic-arrival test** | the heart of exact motion; pure CPU; the first test that can fail for the right reason; CR1 (reference-only binding) |
| R2 | The seam: `seam_graph.hpp` + test adopted into the repo; stage 5's hand-written barriers (`:1215–1218`, the overlay's) become declared passes | one header; the base's only unique asset; MR-3 |
| R3 | Stage 5: `fg_core.comp` + the 8 fused rows (CONVERGENCE C3a = M-C2), M4 by T6 | the AAP result; XR1/XR7 |
| R4 | Stage 6 extracted from the P loop (pacing + drop + FlipStats) as the base's loop SHAPE, `PresentSurface` reused | MR-1/MR-7 reconciled, not re-derived |
| R5 | Stage 3: `FlowSet` declared as a struct + the FlowRing; the holons become `kind = P` rows (off); the MV median moves here; `wap_upload` becomes conditional on device count | E3 re-aimed; the biggest line count, the smallest product risk once rows are off |
| R6 | Stages 1–2 named as modules (`capture/` already is; `ingest/` = the convert + publish tail) | mostly naming; the `--ingest-async` worker is already the stage-2 body |

Every move is verbatim-body + reference-struct binding (the E1 technique), gated by build ×2, the 120 s
smoke, the `--help`/round-trip pair, and the DI-3 two-run A/B where a number decides. `minimal_fg` is kept
in the container as the seam's proof of concept; it is NOT adopted as an app. The CONVERGENCE triad is
re-aimed to "in the donor" (its base track C1/C2/C4/C5 become R2/R6/R1/R4 here); its risk register
carries unchanged.

## 6 · Decisions — TAKEN by the operator 2026-09-03, as proposed (kept verbatim for the record)

> Record: (1) stages 1 and 2 are TWO modules; (2) the directory names below are adopted; (3) multi-GPU stays
> a conditional path made explicit by the contract; (4) R1 (the clock) precedes R3 (the core). The
> re-aimed plan is `CONVERGENCE_MASTER_PLAN.md` v2 (stages R0–R7).

1. **Stages 1 and 2 as two modules or one.** Split proposed (backend-specific acquisition vs the Vulkan
   convert+publish) because `--ingest-async` already runs them on different threads; merging is defensible
   if the operator prefers five stages + one plane.
2. **Directory names.** Proposed: `src/capture/`, `src/ingest/`, `src/flow/`, `src/clock/`, `src/generate/`
   (today `warp_blend/`, 156 lines, a shell), `src/present/`, `src/control/` (today `cli/` + the new
   `layers/`), `src/instrument/` (exists). Threads stay as `run_capture/run_flow/run_present` bodies that
   CALL stage modules (the single-TU-per-hot-thread rationale of `ARCHITECTURE.md` is kept: the stage
   modules are headers + single-caller functions, LTO covers the rest — PR1 measured at each gate).
3. **Multi-GPU.** `wap_upload` and the A/B/G device split are kept as a conditional path (unmeasurable on
   this rig); the contract makes the condition explicit (`FlowSet` device == `GenFrame` device ⇒ no copy).
4. **The order R1 before R3.** The clock first because it is the operator's directive ("frames exactos que
   simulen de forma correcta el movimiento") and because it is the one stage with no test today.

## 7 · Honesty ledger

- The `FgContext` field count (~230) and the coupling table are read from the header and the cited sites;
  I did not trace every atomic's readers — the table lists the ones verified this session.
- The contract types are DESIGNED here; **8/8 are unbuilt** as structs in either tree. `RealSlot`
  (2 doubles) and the `f_pair_*` arrays are the closest existing forms.
- **NAME COLLISION, recorded 2026-09-04:** `FlipStats` is already taken. `framework/render/present/include/
  phyriad/render/present/PresentSurface.hpp:209` declares a 2-field `struct FlipStats { sync_qpc;
  present_count; }` with no `ps_ok`, no `timeout`, no `err` and no device-loss latch — i.e. not the type
  this contract describes. R4 must either extend that struct or name its own something else; picking the
  same name in a different namespace and hoping is the failure mode. Nobody had recorded this.
- Stage 4's test does not exist; its acceptance numbers (lock ticks, phase error bound) are to be set from
  the first run, not asserted here.
- The base's `mc_interp` fuses stages 3 and 5 in one pillar call; separating them in the base was never
  done and is not needed if the base is not adopted.

*Made with my soul - Swately <3*
