# PHYRIADFG_PERFECTION_ROADMAP — the obsessive, prioritized path to LSFG parity, then surpass

> **Diátaxis type:** Planning (Explanation + roadmap). The single cross-dimension
> SEQUENCE that unifies every per-gap dossier and STAGE-level "blind" lever into one
> ordered plan, organized by the governing philosophy **máximo rendimiento · escalabilidad
> · facilidad de consumo · calidad perceptual**. It does not re-derive the per-gap SOTA —
> it cites the dossiers that own it and orders the work between them.
>
> **Status:** `designed` (the roadmap / P-list / S-list is forward work). The STANDING
> assessment in §1–§2 is `measured`/`shipping` — every load-bearing fact there was
> verified first-hand or carried from a named dossier; each is tagged inline.
>
> **Provenance:** synthesized from an 11-agent two-wave per-dimension audit workflow
> (task `w9pb7hkt5`, ~1.29 M subagent tokens: 10 dimension auditors that each read
> `apps/render_assistant/src/main.cpp` + the shaders + the FG dossiers first-hand, plus a
> chief-architect synthesizer). The author re-verified the five load-bearing
> prioritization claims first-hand before writing (§ Verification ledger).
>
> **Verifiable-reference mandate (FDP).** No fabricated path, line, API, or benchmark.
> First-hand-verified claims are tagged `[VS]` (verified in-session, with the grep/site);
> dossier-sourced numbers are tagged with the dossier and inherit its level — this doc
> does NOT mint new benchmark numbers. Where a build is a research bet, it is labelled
> as such, not as a result.
>
> **Operator framing (governing).** LSFG is the apt north star (external-capture, MV-free,
> lightweight-neural on Pascal/iGPU-class hardware) — not the free tools, which are the
> floor. The prize is parity where we lose, and the SURPASS levers our unique assets
> (multi-GPU, the image-field edge-gate, NVOFA, the substrate) make possible that LSFG
> structurally cannot follow. Perfection is **pointed at, not obsessed over** — this is a
> sequence to walk, with honest floors named (§10).
>
> **Normativity (BCP 14).** The key words **MUST**, **MUST NOT**, **SHOULD**, **MAY** are
> used per RFC 2119/8174 only where a genuine requirement binds (dependencies, dogma
> constraints, plan-tier obligations). Everything else is descriptive.

---

## §0 — How to read this

Three orthogonal axes label every item, so the same work can be found three ways:

- **Objective** (the operator's philosophy) — `perf` (máximo rendimiento) · `scale`
  (escalabilidad) · `ux` (facilidad de consumo) · `quality` (calidad perceptual).
- **Honesty bucket** (FDP) — `shipping` (default-on, ships) · `built` (in code,
  default-off) · `measured` (a number exists) · `designed` (planned) · `research`
  (an open bet, outcome unknown).
- **Build tier** (PLAN_TIER_PROTOCOL) — `T0` (mundane, no plan) · `T1` (substantial →
  `*_MASTER_PLAN` + `*_IMPLEMENTATION_STRATEGIES`) · `T2` (risk-bearing: crash /
  concurrency / data-loss / a dogma at stake → **also a `*_RISK_REGISTER`**, no commit
  while any risk is `open`).

The roadmap has two ordered lists — **parity-critical** (`P0–P7`, §3: where we lose
today) and **surpass** (`S1–S6`, §4: where we exceed) — bound by one **dependency spine**
(§5). The **single first build** is argued in §6.

---

## §1 — Honest standing vs LSFG  (`measured`/`shipping`)

The only fair peer is LSFG (same class, only we are harder-constrained: no Reflex hook,
no HUD-less mask). Per dimension:

### WE LOSE TODAY — parity-critical
| Dimension | Why we lose | Source |
|---|---|---|
| **Disocclusion FILL** of the revealed region | LSFG is a lightweight NEURAL model; our stack is all-non-neural, strictly two-frame. Our DETECT + SIDE-PICK is at/beyond published non-neural FRUC art, but revealed-region quality is a research bet (a small inpaint/fusion net), not a tuning win. The conceded "gravity" frontier. | FG_VFI §9–11; FG_OPEN_ALTERNATIVES §5 |
| **Selection STABILITY** (the "vibration") | The per-tick medoid pick is STATELESS — the candidate flips frame-to-frame. The shader's own TODO names it: `wap_warp.comp:178` "hysteresis … not done here". `[VS]` | FG_VFI §11 |
| **Present PACING to the display clock** | We pace to a free-running self-anchored `refresh_hz` NCO at a CONSTANT interval. `[VS]` grep `DwmGetCompositionTimingInfo\|qpcVBlank` in `main.cpp` → **0 matches**. LSFG/FSR3/DLSS-G meter against the DWM composition clock on a moving-average frame time; our own-clock slip (~0) cannot see the drift. | FG_CADENCE_LATENCY §1/§4 |
| **Fractional output-rate control** under shortage | `[VS]` no `target_output`/`frac_mult`/`realized_mult` symbols exist; the 6 `frac` hits in `main.cpp` are all unrelated (vblend tilt, mc_perturb, PLL phase-gain, lowd_span, cadence remainder, dis% block-fraction). Under combat the multiplier collapses to ~1.07× uncontrolled; the governor sheds QUALITY where LSFG-3.1 AFG holds a target output fps. `--motion-fallback` (default-OFF) repeats a real = the AFMF-1 cadence-break we still ship. | FG_SATURATION; DCAD §7 |
| **The entire CONSUMPTION layer** | No GUI / tray / hotkey / profiles; CLI-only, ~61 flags. `[VS]` grep `controlplane` in `apps/render_assistant` → **0 hits**, so CP2 (the FG pushing telemetry through the built boundary) is unwired. LSFG = two-click. | PHYRIADFG_UI; CONTROLPLANE_MASTER_PLAN |

### AT / NEAR PARITY
- **Cadence LADDER** — the PLL/NCO + `--sc-select` + `--phase-norm` + `--cphase` is *more*
  principled than the public LSFG record; missing only DWM alignment (P0) + the
  selection-hysteresis term (P2). `built`/`shipping`.
- **Flow / motion-estimation REACH** — same 8×8-SAD family as FSR3; the fast-object levers
  (`mv_prior`, a consistency map) are built-but-default-OFF, not research. `built`.
- **Device-loss / robustness** — `vk_live()` graceful exit + window-death watchdog +
  fallback-to-vanilla SHIPS and is arguably cleaner than the field. `shipping`.

### WE LEAD (surpass, validated by eye or measurement)
- **Static-HUD / UI separation** — we ship two default-on mechanisms (the `--stasis`
  pixel-identity bypass + the `--inertia` HUD-shield) that the ENTIRE external-capture
  class explicitly cannot do (LSFG/AFMF2 both concede HUD ghosting as unfixable). `shipping`.

### WE HAVE A UNIQUE LEVER (surpass, mostly unmeasured)
Multi-GPU make-space (escape the 1.07× collapse WITHOUT capping the game), the image-field
Sobel edge-gate (no shipping FG uses an image-edge trust gate), NVOFA (LSFG is vendor-
agnostic shader-only — cannot pull HW OFA), and the built measurement instrument
`fg_quality_scorer` (LSFG's 240 fps overlay is digitally uncapturable, so it literally
cannot self-measure full-reference; we can).

---

## §2 — The integrated picture: our "blind" work made coherent  (`measured`)

~115 STAGEs of flag-gated levers, almost all default-OFF byte-identical-off. Grouped by
what they actually ATTACK — the blind work is a **parts bin, not yet a machine**:

1. **Fast-object / flow** — fixed 4-level pyramid block-match (`optical_flow_hier_match.comp`,
   ~±48 px coarse reach). `shipping` `mv_subpel`; `built`-off parity levers `mv_candsel`,
   `mv_prior` (free temporal seed, UNUSED), `mv_affine` (warp doesn't consume M),
   `emit_second_best`, `coarse_wide` (measured net-negative), `flow-scale` DRS. NVOFA
   integrated (STAGE-115, off; −30 % flow-compute but +1.45 ms slice — bidir paid twice).
   **HOLE:** no per-pixel fwd/bwd consistency MAP.
2. **Disocclusion** (`wap_warp.comp`, 1867 lines — our most-worked frontier) — the SOTA
   detect+side-pick skeleton exists (STAGE-48 bidir round-trip class, 49 mask-weighted
   blend, 50 `--fill-div`, 52 gme bg-MV fill, 89 `--disoccl-commit`). LIVE DEFAULT =
   `--bg-snap` + `--band-xfade` (the gravity-cancellation crossfade). **HOLES:** no
   importance-Z overlap ordering, no history-frame bg memory wired, true-hole = irreducible
   crossfade.
3. **Cadence / pacing / stability** — present pacer `paced_wait_P` is a FIXED-STEP
   self-anchored timer (NOT DWM). Ladder = `--sync-clock` PLL/NCO + `--sc-select` (on) +
   `--phase-norm`/`--cphase` (off, built). Selection = STAGE-98 `--multicand` medoid
   PER-TICK STATELESS (**the vibration root**); STAGE-57 inertia gates MV-adoption, not the
   candidate PICK; `--ts-smooth 0.1` masks but cannot eliminate.
4. **Latency / extrapolation** (the one axis interpolation-class LSFG cannot beat) — full
   suite default-OFF: `--asw` (bounded fwd extrap), `--camera-twarp` (responsiveness lead,
   gme-affine), `--rfp`/`--rfp-fresh`, `--shallow-queue`. Gated behind disocclusion-fill
   quality. Anchors `measured` (BF6 `--phaselog` 797 pairs): AddedLat ~108 ms = D-anchor
   ~68 ms + ~40 ms residual WGC floor.
5. **Saturation / make-space** (`perf`) — SHIPPING DEFAULT present is SYNCHRONOUS: `[VS]`
   `vkWaitForFences(A.dev,1,&fBridge,VK_TRUE,UINT64_MAX)` at `main.cpp:7001/7150/7507`;
   `--async-present` built default-OFF. Governor / deficit-tier SHEDS quality (the
   SOTA-wrong move, named honestly). `--upload-xfer` moves only the recipe upload to the
   A.qT DMA, not the warp compute (no three-queue split).
6. **Multi-GPU / scalability** — single-GPU collapse SHIPPED (`--force-single-gpu`,
   FD-routing, validation-clean, RISK_REGISTER discharged) → DCAD's "cannot run on one GPU"
   is STALE. BUT the `framework/gpu` ROUTER has ~zero in-product callers: `[VS]`
   `break_even_decide` appears **once** in `main.cpp`; `offload=false` effectively
   hardcoded; `GpuDescriptor` has `compute_gflops` only, no fp16/DP4a field. ONE present
   authority by construction.
7. **HUD** (where we LEAD) — `--stasis` (on, ZERO-cost reuse of the Gate-1 SAD tap) +
   `--inertia`/HUD-shield (on) + the novel iGPU Sobel edge-gate (`igpu_field.comp`, off).
   `--lead-commit` pixel-stasis is named in plans but does NOT exist (grep-clean).
8. **Robustness + measurement + UX substrate** — device-loss `vk_live()` + window-death
   watchdog SHIP. `fg_quality_scorer` BUILT (PSNR/SSIM + motion-masked `dbl_edg_m` +
   `flowdsc` FloLPIPS-analogue + `crescent_band_energy` + held-out sequence mode +
   worst-frame surfacing). `controlplane` pillar BUILT (CP0/CP1/CP3); **CP2 NOT wired.** No
   Tauri/React UI exists.

---

## §3 — The ORDERED parity-critical list (P0–P7)

Build in this order. Each row: what · why · dependency · build tier · honesty · per-gap SOTA.

### P0 — DWM-metered shared clock  ·  `quality` · `T0`→`T1` · `designed`
Replace the free-running self-anchored `paced_wait_P` constant interval with
`DwmGetCompositionTimingInfo` `qpcVBlank` metering + an EWMA frame-time, folded into the
existing `T_robust` PLL so present-pacing and content-selection share ONE locked clock.
~1 API call + 1 EWMA term, code-local, NOT research. **Gates P1, P3, P5(present-pin).**
SOTA: FG_CADENCE_LATENCY §1/§4 (marks the DWM-clock pacer as BUILD = not yet built).

### P1 — The fractional output-multiplier controller  ·  `perf`+`quality`+`ux` · `T1` · `designed`
Hold a target output fps; FLOAT the realized multiplier; **MUST NOT** touch the game's
render rate (external, dogma-clean — it is NOT L1 base-game-cap). Replaces the
whole-multiplier governor shed for the single-GPU case; fixes the ~1.07× combat collapse.
It was MIS-FILED as a physics floor and the audits re-labelled it `addressable-design`.
A controller over signals we ALREADY compute (`flow_ms`/`warp_ms`/`ring_occ`/`slip`).
**Depends on P0** for an honest target-rate timebase. **THE SINGLE FIRST BUILD (§6).**
SOTA: FG_SATURATION (L2 keep-generating + drop-the-late-frame); DCAD §7 (the highest-value
single-GPU lever); LSFG-3.1 AFG is the shipping reference.

### P2 — Selection-stability hysteresis  ·  `quality` · `T0`→`T1` · `designed`
A temporal-consistency term on the per-tick medoid/crossfade pick: hold the candidate
decision across N ticks, gated by the existing inertia persistence counter (already a
hysteresis substrate) AND the iGPU Sobel edge field. The literal vibration root
(`wap_warp.comp:178` + FG_VFI:876 both name it). Near-free per-tick scalar op.
**Independent of P0/P1 — MAY parallelize.** SOTA: FG_VFI §11 (content-class agreement,
3DRS semi-global regularization, the medoid rule).

### P3 — Flip `--async-present` default-ON + runtime-adaptive flow-scale  ·  `perf` · `T2` · `designed`
The ONE SOTA saturation lever PhyriadFG fully implements yet ships OFF; the synchronous
`fBridge` fence wait `[VS]` IS the present-side latency under combat. Tie `--flow-scale` to
MEASURED runtime util (init-time-only today) so the governor KEEPS GENERATING + drops the
late frame (L2), not shed quality (L3). **Depends on P0** (drop-on-slip needs the metered
clock). Crash/device-loss class → **MUST carry a `*_RISK_REGISTER`** (PLAN_TIER §T2).
SOTA: FG_SATURATION §3; UPLOAD_OFFLOAD (the queue-contention map).

### P4 — The disocclusion arbiter  ·  `quality` · `T1` · `designed`  **(gates S5)**
A one-pass fwd/bwd flow-CONSISTENCY MAP (range-map occupancy from the existing MV field —
one scatter pass, **NO second flow**) for a clean per-pixel HARD-PICK at the reveal band;
default-ON `mv_prior` + the cheap parity-fillers. **This is the named dependency: the mask
gates the extrapolation latency-win (S5)** — extrapolation reveals more holes, so
`--asw`/`--camera-twarp` stay default-OFF until the band fills cleanly. Cross-cutting
safety fix: `--camera-twarp` **MUST** lead by camera ROTATION only — translation/parallax
judders without depth (the residual snap the operator felt; DCAD §4 HARD CORRECTION).
**Depends on nothing; unblocks the latency surpass arm.** SOTA: FG_OPEN_ALTERNATIVES §5;
FG_VFI §9–11.

### P5 — Wire the dead router  ·  `scale` · `T2` · `designed`  **(gates all general multi-GPU + S1)**
Replace the deviceType/LUID hardcodes with measured CHARACTERIZE: add the fp16/DP4a field
to `GpuDescriptor` (a published cross-process POD layout change → **MUST receive operator
approval** before it ships), wire `measure_transfer_bw`/`select_participants` into startup,
persist+invalidate the capability table, and **MUST NOT** ship the author's ~596 FLOP/byte
break-even crossover as a default (gate every offload on a per-rig measured break-even).
Pin PRESENT to the monitor's GPU in the 2-GPU stage pipeline. `[VS]` routing is hardcoded
to one rig — the single biggest multi-GPU parity gap. POD layout + concurrency → `T2`.
**Independent of the perceptual cluster.** SOTA: DCAD §5; SINGLE_GPU_COLLAPSE_RISK_REGISTER;
GPU_MULTI_GPU_PRIOR_ART.

### P6 — The consumption layer: CP2 then the frontend  ·  `ux` · `T1` · `designed`
Wire CP2 (the FG pushes its `TelemetryFrame` through the built `controlplane` boundary —
`[VS]` zero hits today), then the Tauri/React frontend (UI master-plan P0–P6: window-pick +
Start, per-game profiles, the quality↔latency slider surface over the existing
`--flow-scale auto` mechanism). **Depends on P1** (the slider's Auto = the fractional
target) and **P5** (first-run auto-characterization). Most needs no operator eye until the
final slider-tuning pass. SOTA/plan: PHYRIADFG_UI_MASTER_PLAN; CONTROLPLANE_MASTER_PLAN;
FG_TELEMETRY.

### P7 — Measurement parity  ·  `quality`+`ux` · `T1` · `designed`
An eye-test GATING LOOP (log the operator's A/B verdicts into a durable, regression-gated
signal) + metric CALIBRATION against a human-rated set (BVI-VFI). These do not block builds
but they are what turn every "we beat LSFG" claim from eye to evidence — the cheapest,
highest-ROI verification moves. SOTA: FG_VFI §2/§5; the built `fg_quality_scorer`.

---

## §4 — The ORDERED surpass list (S1–S6)

How our unique assets let us EXCEED LSFG. Each builds on a parity foundation.

### S1 — Independent FG instances, one per GPU / window / screen  ·  `scale` · `T2` · `research`/`designed`  **(depends on P5)**
**The headline bet — the one capability LSFG STRUCTURALLY cannot follow.** LSFG is provably
single-instance (multi-frame-gen = stacked passes on ONE target; multi-monitor mode just
keeps ONE game alive across focus). Our holonic substrate makes the multi-instance case
NATURAL: each capture→flow→warp→present chain is a self-contained role graph;
`select_participants` partitions a measured device set; the topology Scheduler pins each
instance to disjoint CCX/cores. GPU-2 runs full FG on game/window B while GPU-1 runs game A,
each with its OWN `PresentSurface` on its OWN monitor — the "one present authority"
invariant holds **per panel**, so N panels do NOT reintroduce SLI runt-frames. The n-GPU
thesis finally paying off as a product differentiator (streamer / dual-box / multibox; the
operator's background-anime-on-a-second-screen idea). **Needs its own design plan
(`T2`).** SOTA/design: DCAD; GPU_MULTI_GPU_PRIOR_ART.

### S2 — The image-field edge-gate, promoted from stub  ·  `quality` · `T1` · `built`→`designed`
Our asset no shipping FG has. Promote `igpu_field.comp` from the luma-Sobel "P0 stub" to a
real per-pixel occlusion-class + inward chamfer-distance producer, and use it as the TRUST
GATE on four payoffs from one asset: (a) the disocclusion hard side-commit (sub-block
precision LSFG's flow-only detection cannot reach), (b) fast-object crossfade rejection,
(c) the HUD hard-commit, (d) the selection hysteresis (P2). UNVALIDATED-NOVEL → each
default flip **MUST be gated behind a measured held-out scorer win** (the honest flip side).
Cheapest, most cross-cutting unique asset → promote early. SOTA: FG_VFI §10/§11 (F20/F29/F43).

### S3 — Multi-GPU make-space → true three-queue split + NVOFA net-win  ·  `perf`+`scale` · `T1`/`T2` · `designed`  **(depends on P5)**
(a) Extend `--upload-xfer` to a true graphics / async-compute-interp / present three-queue
split so even the single-GPU present stops contending on A.q (the audit's named miss). (b)
Convert NVOFA to a NET slice win by collapsing the bidir redundancy to ONE
`BOTH_DIRECTIONS` execute feeding both convert passes (built-capable, eye-blocked), then
auto-enable on OFA-capable GPUs as a capability-detected flow provider — freeing the 4090
compute cores so an FG instance there leaves MORE headroom for a SECOND instance (an S1
synergy LSFG's CUDA-core flow lacks). SOTA: FG_SATURATION; FG_NVOFA; UPLOAD_OFFLOAD.

### S4 — Neural-on-idle-4090  ·  `quality`+`scale` · `T1` · `research`  **(depends on P4, P5)**
Two uses of the ~50 %-idle 4090 tensor cores the substrate's break_even router scales into:
(a) a NeuFlow-v2-class lightweight CNN at reduced internal res as a measured-fp16-gated
FLOW provider; (b) a small inpaint/fusion net for the true-hole band on the 4090 spare
headroom (flow on 1080Ti, warp+inpaint on 4090, convert+field on iGPU) — the route to MATCH
neural-LSFG revealed-region quality while keeping the non-neural fast path default.
**A research bet → MUST be measure-gated per the efficiency mandate and degrade gracefully
to non-neural.** **This dimension REQUIRES its own dedicated SOTA before building** (which
net, fp16 budget on Ada, pretrained-vs-trained, the latency envelope) — the operator
authorized unlimited per-gap SOTA; spend it here. Lead on DETECTION via S2, match on FILL
via S4. SOTA seed: DCAD §4 (the tier-conditional FLOW class); FG_NVOFA.

### S5 — The extrapolation latency win  ·  `quality`+`perf` · `T1` · `designed`  **(HARD-gated on P4)**
The one axis a no-MV external tool can STRUCTURALLY beat interpolation-class LSFG: once P4's
arbiter fills the reveal band cleanly, unblock `--asw`/`--camera-twarp` (rotation-only-safe)
to warp-the-latest-real-forward with NO +1-frame hold. NVOFA + the droppable multi-GPU
cascade fund the extra passes without stealing the game's budget; degrades
4090+1080Ti+iGPU → single-4090 gracefully. SOTA: FG_CADENCE_LATENCY (F5: extrapolation cuts
the hold but pays in prediction artifacts — "no-latency" REFUTED); DCAD §4/§6.2.

### S6 — Measurement surpass  ·  `quality` · `T1` · `designed`  (runs continuously)
(a) The NO-REFERENCE physically-captured axis (FCAT color-overlay / high-speed camera) —
the ONE method that defeats LSFG's digital-uncapturability wall; run `dbl_edg_m`/DIV/E_warp
on camera-captured LSFG vs ours = the field's-first honest head-to-head. (b) Adopt the
verified cheap NR DIV (flow divergence, ~87 % of FloLPIPS correlation at ~37 % cost; FG_VFI
/ arXiv 2508.09078) into the truth-less Mode T. (c) Run the scorer / a learned metric
CONCURRENTLY on the idle 1080Ti — a continuous quality EMA no single-GPU tool can afford.
(d) Our digital self-capture is a strict advantage: TRUE full-reference on our own live
output, which LSFG cannot do. (e) Surface the multi-GPU telemetry in the UI — out-INFORM
LSFG (which shows only base/final fps). SOTA: FG_VFI §2/§5; FG_TELEMETRY.

---

## §5 — The dependency spine

```
        P0 (DWM clock) ──┬──► P1 (fractional controller) ──► P6 (slider Auto, frontend)
                         └──► P3 (async-present default-ON)
        P2 (selection hysteresis)         ── independent, parallel
        P4 (disocclusion arbiter) ─────────────────────────────► S5 (extrapolation win)
        P5 (router) ──┬──► general multi-GPU claims
                      ├──► P6 (first-run auto-config)
                      ├──► S1 (independent instances)   ◄── the headline differentiator
                      └──► S3 / S4 (capability-gated offload)
        S2 (edge-gate) ── feeds P2, P4, HUD, fast-object  ── promote early, gate on measured win
        S4 (neural fill) ── needs P4 (where to fill) + P5 (routing) + its own dedicated SOTA
        S6 (measurement) ── runs continuously alongside everything
```

Reading: **P0 → {P1, P3}**; **P4 → S5** (the named gate); **P5 → {general multi-GPU, P6
first-run, S1, S3, S4}**; **P1 → P6 slider Auto**. P2, P4, P5 can run in parallel with
P0/P1. S2 is the cheap cross-cutting asset — promote it early, behind a measured win.

---

## §6 — The single first build:  P1 (co-sequenced with P0)

**P1 — the fractional output-multiplier controller**, timebased against **P0**'s
DWM-metered clock. Why it leads the more glamorous perceptual and multi-GPU work:

1. **It is parity-critical in THREE of the four objectives at once.** `perf`: replaces the
   governor's quality-shed and fixes the verified ~1.07× combat collapse by holding a
   target output fps under saturation (the LSFG-3.1-AFG escape — dogma-compatible because
   it never caps the game). `quality`: a stable target rate is the precondition for a flat
   ladder — the cadence work has nothing to lock to without it. `ux`: it is literally the
   ONE knob LSFG's AFG exposes ("set your fps and forget it") — it converts ~61 flags into a
   single trustworthy default.
2. **It corrects a false "impossible."** It was mis-filed as a physics floor and the audits
   re-labelled it addressable-design — the team previously believed it impossible and
   stopped. Unblocking the most leverage per line of code.
3. **Verified UNBUILT and verified buildable.** `[VS]` no controller symbols exist; it does
   not touch the game's render rate (external, dogma-clean); it is a controller over signals
   we already compute (the per-present telemetry).
4. **Shortest critical path to a FELT improvement** the operator can eye-validate in his
   BF6-combat test — the exact scene where today's collapse is most visible.

The disocclusion FILL (the conceded frontier, S4) matters more to *ultimate* perfection but
is a research bet, gated on P4's arbiter — weeks before anything ships. The
independent-instances bet (S1) is the biggest differentiator but is gated on P5's router
rewrite. P1 is the one build that is high-leverage, cheap, verified-unbuilt, dogma-clean,
and immediately eye-testable — and it lays the target-rate timebase the cadence, saturation,
and UI work all consume. **Co-sequence P0** (the metered clock) with it, since P1 needs that
timebase to be honest.

---

## §7 — Philosophy mapping

One observation unifies the plan: **ONE controller (P1) discharges the parity-critical gap
in THREE objectives at once** — which is why it is first.

| Objective | Owns | Standing |
|---|---|---|
| **`quality` — calidad perceptual** (the north star) | Disocclusion, flow/fast-object, cadence/selection, static-HUD, latency/extrapolation, measurement | The largest, hardest, most eye-blocked cluster — where we LOSE (fill, vibration) AND where our novel asset lives (edge-gate). |
| **`scale` — escalabilidad** | Multi-GPU (P5), independent instances (S1), the controlplane generalizing to many producers | Home of the single biggest SURPASS bet. The dead router (P5) MUST be wired before any general multi-GPU claim is honest. |
| **`perf` — máximo rendimiento** | GPU-saturation / make-space (P1, P3, S3) | The instinct (keep generating, shed from idle silicon, never cap the game) is the correct LSFG-side of the fork — but the IMPLEMENTATION currently sheds quality and ships the present-fence wait synchronous. |
| **`ux` — facilidad de consumo** | Frontend / robustness (P6), the slider, profiles, CP2 | Engine + hard robustness at/past parity; the entire consumption layer is unbuilt and IS the dimension. |

---

## §8 — Per-gap SOTA backing (the dossier index)

This roadmap orders work; the deep SOTA for each gap lives in its dossier. Build agents
**SHOULD** read the relevant dossier first-hand before implementing (FDP verifiable-
reference mandate carries to them).

| Item | Dossier(s) | Needs a fresh dedicated SOTA before building? |
|---|---|---|
| P0 clock | FG_CADENCE_LATENCY_PRIOR_ART | No — DWM API is documented |
| P1 controller | FG_SATURATION_PRIOR_ART; FG_ARCHITECTURE_DCAD §7 | No — LSFG AFG is the reference |
| P2 hysteresis | FG_VFI_PRIOR_ART §11 | No |
| P3 async-present | FG_SATURATION_PRIOR_ART; UPLOAD_OFFLOAD | No (but a RISK_REGISTER is mandatory) |
| P4 arbiter | FG_OPEN_ALTERNATIVES §5; FG_VFI §9–11 | No |
| P5 router | FG_ARCHITECTURE_DCAD §5; SINGLE_GPU_COLLAPSE_RR; GPU_MULTI_GPU_PRIOR_ART | No |
| P6 consumption | PHYRIADFG_UI_MASTER_PLAN; CONTROLPLANE_MASTER_PLAN; FG_TELEMETRY | No |
| P7/S6 measurement | FG_VFI §2/§5; FG_TELEMETRY; `fg_quality_scorer` | No |
| S1 independent instances | FG_ARCHITECTURE_DCAD; GPU_MULTI_GPU_PRIOR_ART | **Design plan (T2)** — architecture, not external SOTA |
| S2 edge-gate | FG_VFI §10/§11 (F20/F29/F43) | No — but each default flip needs a measured win |
| S3 three-queue + NVOFA | FG_SATURATION; FG_NVOFA; UPLOAD_OFFLOAD | No |
| **S4 neural fill/flow** | DCAD §4; FG_NVOFA | **YES — a dedicated SOTA** (which net, fp16 budget on Ada, pretrained-vs-trained, latency envelope) |
| S5 extrapolation | FG_CADENCE_LATENCY (F5); DCAD §4/§6.2 | No |

---

## §9 — Build-tier & plan-tier obligations (PLAN_TIER_PROTOCOL)

- **`T2` (RISK_REGISTER mandatory, no commit while any risk is `open`):** **P3** (flip
  `--async-present` default — crash/device-loss class), **P5** (cross-process `GpuDescriptor`
  POD layout change + concurrency), **S1** (independent present authorities + scheduler
  partition + multi-surface DComp).
- **`T1` (MASTER_PLAN + IMPLEMENTATION_STRATEGIES):** **P1**, **P2** (if it grows beyond a
  scalar term), **P4**, **P6**, **P7**, **S2**, **S3**, **S4**, **S5**.
- **`T0` (no plan):** the smallest code-local pieces — the P0 EWMA term, a single-scalar P2.
- **Operator-gated, non-negotiable:** the **P5 `GpuDescriptor` POD layout change MUST get
  operator approval** before it ships (a published cross-process ABI); each **S2 default
  flip MUST clear a measured held-out scorer win**; **P1 MUST NOT cap the game** (dogma).

---

## §10 — Honest floors (NOT on this roadmap — irreducible)

Named so no future pass re-proposes them as wins:

- **The interpolation HOLD** — LSFG and we both pay ~1 source-frame by definition; not
  removable by tuning. (Only S5 extrapolation escapes it, at the cost of prediction
  artifacts — and only behind P4.)
- **The true-hole** (visible in NEITHER frame) — no non-neural cure exists; the whole field
  (incl. neural LSFG/FSR3/GFFE) concedes it. S4 is a *bet* to narrow it, not a guarantee.
- **Moving-background-behind-moving-object** + **block-resolution boundary** — near-
  irreducible without depth, which an external-capture tool cannot have.
- **L1 base-game cap** — structurally unavailable to an external tool AND blocked by the
  no-cap dogma. Parity here is NOT about L1.
- **Consumer-GPU queue priority/preemption** — does NOT preempt a saturating dispatch
  (proven dead-end; do NOT re-propose a priority/currency scheme).
- **In-engine Reflex/anti-lag offset & hardware flip metering** — the external class is
  structurally ~1–2 frames worse than in-engine FG on a GPU-bound single dGPU.

---

## §11 — Verification ledger (`[VS]` — verified first-hand this session)

| Claim | Check | Result |
|---|---|---|
| No fractional output-multiplier controller | grep `target_output\|frac_mult\|realized_mult\|fractional` in `main.cpp` | 6 hits, ALL unrelated (vblend tilt L135, mc_perturb L141, sc_phase_gain PLL L173, lowd_span_frac L594, cadence remainder math, dis% block-fraction L1723) → no controller |
| No DWM display-clock metering | grep `DwmGetCompositionTimingInfo\|qpcVBlank\|DWM_TIMING_INFO` in `main.cpp` | 0 matches |
| CP2 unwired | grep `controlplane` in `apps/render_assistant` | 0 hits |
| Router effectively dead in-product | grep `break_even_decide` in `main.cpp` | 1 occurrence |
| Synchronous present fence wait | grep `vkWaitForFences(...fBridge...UINT64_MAX)` in `main.cpp` | 3 sites: L7001, L7150, L7507 |

---

## §12 — References (leveled)

The external SOTA is owned by the per-gap dossiers (§8), each carrying its own leveled
references; this roadmap inherits them and does not restate their `[V1]/[V2]/[V3]` levels.
The in-repo anchors are first-hand (`[VS]`, §11). The honest standing vs LSFG (§1) is the
synthesis of the `w9pb7hkt5` audit, whose 10 dimension reports and ~120 fact-rows are the
backing evidence; load-bearing prioritization facts were re-verified by the author (§11).
