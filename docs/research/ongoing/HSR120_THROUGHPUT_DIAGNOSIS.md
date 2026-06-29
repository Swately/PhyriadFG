# HSR@120 throughput diagnosis + STAGE-85 plan — resume brief

**Status:** `designed` (analysis verified by code-reading; not yet profiled live).
**Date:** 2026-06-13. **Author of analysis:** Fable-5 supervisor + a 4-lens workflow
(`wf_75c1a9fd-e74`), grounded on the operator's HSR@120 field log + first-hand code reading.
**Why this doc exists:** the operator restarted mid-analysis; this is the durable record so the
next session resumes without re-deriving. The workflow's temp output (`tasks/wn6gtxr1v.output`)
is ephemeral — the essentials are captured here.

---

## 0. The operator's NEW directive (binding, 2026-06-13)

> "ahora como objetivo genuino, me gustaría **ganarle a LSFG**; quiero creer que nuestra
> arquitectura es sólida, solo tenemos problemas 'de libro que resolver'."

**The goal is reframed: BEAT LSFG, including at high source-fps.** The prior stance ("not an
LSFG-killer, niche utility, concede high-fps") is SUPERSEDED for the FG product line. The
diagnosis below VINDICATES the operator's instinct: the HSR@120 wall is **not physics** — it is a
synchronous fence-wait (a textbook concurrency stall) that pipelining fixes. Do not revert to the
"concede high-fps" framing; treat high-fps as winnable and pursue the pipelining lever.

---

## 1. The field result (HSR Star Rail, 1080p, WGC, igpu-convert ON, --fg-gpu AUTO, WAP ON)

- present **240-244 fps** (240 Hz output clock — fine).
- arr ~**116/s**, cap ~**116/s** (the source IS arriving at 120fps; capture keeps up).
- **cons ~78/s** (mostly 73-89) → **F consumes only ~67% of real frames; ~1/3 are DROPPED.**
- uniq ~130-170 of 242 presented (the rest are duplicates of the held real).
- **NOTHING saturated:** `gpu(A:39-42% B:0% G:23-28%)`.
- tier:2/tier:3 fire **constantly**; bwd-skip:100%; lat 28-35ms (zoo was flat 26).
- STAGE-84 spin-guard fires periodically (lap:3/s) → cons dips to 51-56 those windows.

**Operator's perception:** "se pierde la fluidez de 120fps; los congelos son menores [STAGE-84
helped the death] pero no existe la fluidez de 240 generados; se siente que parte de los frames
completos se cortan en todo el holon." → This is EXACTLY cons<arr: real anchors are dropped, the
interp holon spans 2× gaps, motion is stretched.

---

## 2. THE CORRECTED DIAGNOSIS — the wall is a synchronous fence wait, not the transfer round-trip

(My earlier "transfer round-trip is the wall" hypothesis was REFINED by the cost-path lens —
recorded here as the corrected understanding.)

Per consumed pair, the F thread walks **one serial chain** (everything before
`stat_cons.fetch_add(1)` at `main.cpp:4361`):

| Stage | Cost | Where | Blocking? |
|---|---|---|---|
| capture ingest / `c_cv.wait_for` | ~0ms (capture always pending at 120 arr) | 3757 | no |
| STAGE-84 spin-guard | 0 in steady state (fires only on lap windows) | 3807-3822 | only when P frozen |
| cmdB record (iGPU-pack→B device copy, unpack, **fwd flow**, MV/SAD copy-out) | sub-ms CPU record | 3824-3892 | no (deferred) |
| **`submit_wait(B,cmdB,fB)` — THE DOMINANT STALL** | **~6.7ms blocking** | **3893** | **YES — `vkWaitForFences UINT64_MAX`** |
| gme fwd fit (`gme_fit_affine`) | ~2.7ms CPU | 4024 | (runs only AFTER the wait) |
| mem_advect/merge + object_repair fwd | ~2.0ms CPU | 4051-4066 | (serial after) |
| mem_refresh | CPU | 4198 | (serial) |

**Sum ≈ 6.7ms GPU-wait + 2.7ms fit + 2.0ms objects ≈ 11.4-12.8ms/pair** → cons ~78/s against an
**8.3ms budget** (120fps). Matches the observed cons exactly.

**Key facts:**
- `submit_wait` (defined `main.cpp:1380`) = `vkQueueSubmit` + **`vkWaitForFences(…UINT64_MAX)`** —
  a fully blocking CPU wait. F idles ~6.7ms on the 1080 Ti pyramid, THEN starts the ~4.7ms of CPU
  work. **The CPU and GPU never overlap.** That is why `B:0%` in PDH (the GPU is busy < half the
  pair period, idle the rest, sampler rounds to 0) and why nothing is saturated — the wall is
  **serial per-pair latency, not throughput.**
- The transfer is NOT a separate round-trip: there is **one** H2D upload (`hRP_b→hRP_b_dev`,
  `3827`) folded INTO the fwd submit, and the MV/SAD path is a **~130KB host-bounce** that both
  devices import (`hostMV[g]` imported on B AND A at `2272-2274`) — comments call it "≈free". No
  device P2P exists; the host-bounce adds bandwidth, not a serial round-trip.
- `bwd-skip:100%` (do_bwd=false, `3980`) already drops the entire backward pass, and cons STILL
  can't reach arr → the residual deficit is the **fwd GPU-wait + fwd CPU fit**, the two
  irreducible serial costs the tiers cannot touch.
- The pressure tiers (`3958-3976`) only skip CPU holon work (~1.5ms: objects 2ms + memory 1ms
  every 2nd/4th pair) — **the wrong cost axis.** They fire constantly yet cons stays ~78.

---

## 3. THE LEVER — STAGE-85: forward-pass cross-pair pipelining (the prize)

**The structural fix is the same overlap STAGE-55 already applies to the BWD pass** (separate cmd
buffer `cmdB_bwd` + fence `fB2`, submitted no-wait at `4004`, waited later at `4105`). Apply it to
the FORWARD pass:

> Submit pair N's fwd flow to `B.q` with its own fence. Run pair **N-1**'s CPU gme-fit / objects /
> memory work while N's GPU pyramid executes. Wait N's fence only at the point the host MV/SAD is
> actually consumed.

This hides the ~4.7ms CPU under the ~6.7ms GPU wait (or vice-versa), collapsing the serial
~12.8ms → **max(GPU 6.7, CPU 4.7) ≈ 6.7-7ms/pair → cons ~140-150/s → tracks arr 120.**

- **Feasibility:** medium-code. **Quality:** UNAFFECTED — same flow, same fits; pure scheduling.
- **Mechanics:** needs a second fwd cmd buffer + fence and a one-pair-deep MV/SAD host bridge (the
  gen ring `kGenRing` already gives 2-3 slots). Pair N's fit reads N-1's durable (post-fence)
  copy, so the `f_pair_*` publish (`4359`) and `f_seq` bump (`4362`) shift **one pair later** →
  **+1 pair of latency** (lat 28-35ms has headroom under the 240Hz clock).
- **Must preserve:** the STAGE-84 spin-guard (`3811`) and the in-order ingest selector
  (`3782-3793`) semantics.
- **Instrument first:** split `submit_wait` into submit-timestamp vs fence-wait-timestamp so
  `t_pair_ema` (already computed at `4246`) reveals the GPU-wait fraction directly — this CONFIRMS
  the ~6.7ms before committing the redesign (the 6.7ms is inferred from memory, not yet measured
  on this build).

### Secondary levers (smaller, optional)
- **Spin-guard bound 64→16ms** (`kRingGuardSpinMax`, `3808`) or a **value-stability escape**
  (break when `p_presenting` is byte-stable for ~6ms = P frozen; keep waiting while it moves = P
  transiently busy). Small-code. Cuts the lap-window stall ~4-10×. Safe (the guard is a quality
  opt per the `3802-3804` comment, not correctness). Recovers a few cons/s on lap windows; does
  NOT address the primary wall.
- **Move the `3827` upload + unpack dispatch to `B.q2`** (already used for the x4 leg) to shave
  the upload leg off the critical fence — smaller term than the fwd-pipelining.

### The 4090-spare path (ofpA) — why NOT preferred
A-primary FG exists (`ofpA`, STAGE-26b, init `2511`) but is **hard-disabled under igpu-convert**
(`pfg_enabled=false` at `2546`) AND is **mutually exclusive with WAP** (`4352`): WAP is hardwired
to B's `ofp.motion_image()` grid (`3860/3877`); ofpA emits finished interp IMAGES (`4249-4263`),
not an MV grid. A true "everything on the 4090, no B" needs a **STAGE-26b×WAP fusion** (medium-
code: route flow to ofpA AND expose its MV/SAD into the WAP bridges AND feed the CPU fit from
A-local memory). Even then the CPU fit+objects (~4.7ms) remain the floor. **Forward-pipelining is
strictly better:** keeps the multi-GPU edge + WAP exact-phase warp + quality, and the same ~4.7ms
CPU is what it hides rather than leaves serial.

---

## 4. Strategic frame (updated for the beat-LSFG directive)

- The regime arithmetic still holds: 60fps budget 16.7ms > pair → cons tracks (STAGE-83 fluidity);
  120fps budget 8.3ms < pair → 1/3 dropped. **But the conclusion flips:** the pair is ~12.8ms only
  because of the serial fence stall. Pipelined to ~6.7ms, the pair **fits the 8.3ms budget** → high-
  fps becomes winnable WITHOUT conceding the architecture. The operator's "textbook problem" read
  is correct.
- LSFG/DLSS-FG context (web-sourced, directional): in-chain FG (DLSS3/FSR3) keeps ALL reals,
  ~1-3ms, no capture/transfer; LSFG is external-capture like us and pays a latency tax (~+13ms at
  40fps base) — our flat ~26ms is the low-fps win, and pipelining extends competitiveness UP into
  high-fps where LSFG's single-GPU in-process loop currently beats our un-pipelined one.
- Dossier: still build the lat-vs-source-fps graph; now ALSO target the high-fps cons==arr proof
  post-STAGE-85 (the HSR@120 redo should show cons≈arr, no 1/3 drop).

---

## 5. RESUME CHECKLIST (next session)

1. **Instrument** `submit_wait`/`t_pair_ema` split (submit vs fence-wait ms) → confirm the GPU-wait
   fraction is ~6.7ms (the redesign's premise). Cheap, do FIRST.
2. **STAGE-85 fwd-pipelining** (medium-code): second fwd cmd buffer + fence; N-1 CPU work overlaps
   N GPU flow; publish/`f_seq` shift one pair; preserve spin-guard + in-order ingest. Gate: a gate
   run + the HSR@120 redo showing cons≈arr (no 1/3 drop), quality byte-identical, lat +≤1 pair.
3. Optional secondary: spin-guard value-stability escape; `3827` upload → `B.q2`.
4. Update `render-assistant-arc.md` memory + the kickoffs doc; commit via pathspec.

**Code anchors:** F loop `main.cpp:3753`; the fence wall `3893` (`submit_wait` def `1380`); fwd
flow record `3860`; MV/SAD copy-out `3877-3878`; the BWD overlap template `cmdB_bwd` `2773` /
submit `4004` / wait `4105`; tiers `3958-3976`; spin-guard `3807-3822`; `t_pair_ema` `4246`;
`stat_cons` `4361`; pyramid `OpticalFlowPipeline.cpp:474-479,626-694`.

---

## 6. STAGE-85 refined design + honest competitive correction (research `wf_2273ea11`, 4 lenses + 3 adversarial refutations)

### 6.1 The refined STAGE-85 design (the textbook way)
- **The binding constraint (first-hand):** `ofp` owns ONE set of internal images (`mv_image_`,
  `sad_img_`, pyramid) — `record_optical_flow`'s `slot` cycles only DESCRIPTOR SETS
  (`OpticalFlowPipeline.cpp:591-592`); every match overwrites the SAME `mv_image_`. So **two fwd
  matches cannot be in flight in one `ofp`.** Pipeline at **cmd-buffer granularity**, not by running
  two matches at once: keep record+copy-out in one submit; the per-gen host buffer
  (`hMV_b[f_gen]`, `3877`) is the decoupling point.
- **The design = the STAGE-55 BWD template applied to FWD:** add `cmdB_fwd[2]` + `fB_fwd[2]`
  (ping-pong, depth 2 = textbook frames-in-flight) next to `cmdB_bwd@2773`/`fB2@2793`. Submit pair
  N's fwd NO-WAIT; then **deferred-block** on pair N-1's fwd fence (`vkWaitForFences UINT64_MAX`, the
  `fB2@4105` idiom) at the host-MV-consume point; run N-1's CPU work (gme fit `4024`, mem `4051-52`,
  objects `4066`) while N's GPU match runs; **publish N-1** (the publish shifts one pair later:
  `f_pair_*@4359` + `f_seq@4362`). `NS=3` already covers the three live gens {N gpu, N-1 cpu, N-2 P};
  per-gen host buffers already `kGenRing`-deep. One-pair warm-up + shutdown fence drain required.
- **NOT a single queue/timeline-semaphore problem:** single B.q, single CPU submitter, host-side
  wait → a plain fence is correct (a timeline semaphore is for multi-queue graphs).
- **Expected floor:** serial ~11.4-12.8ms → `max(GPU 6.7, CPU 4.7) ≈ 6.7ms/pair` → cons ~145-150
  → tracks arr 116. The new hard floor is the 1080 Ti fwd flow leg itself.

### 6.2 ⚠ "Pure scheduling / quality byte-identical" is the WEAKEST claim — the torn-read hazard
The per-pair CPU work mutates **process-wide state carried in strict pair order**, NOT per-gen:
`persist[]` (`3150`, reset/accumulated each fwd `3916-3917`), `mem_prior`/`mem_adv` (`3201-3202`,
advect/merge/refresh `4051/4128/4198`), `obj_slots_fwd/bwd` (`3193`, mutated `4066/4144`). Pair N's
clustering reads `mem_prior` written by N-1's refresh; identity tables carry across pairs. **Byte-
identity holds ONLY if the refactor keeps these mutations in strict pair order AND defers the
inertia/persist update (`3907-3922`) to read gen_{N-1}.** A single misplaced read of gen_N's host
buffer while gen_N's GPU match is still in flight = a **torn/garbage MV** feeding the affine fit and
identity tables = a SILENT quality regression (not a crash). This is an error-prone interleaved-block
refactor, not a free reorder. **Verify gate:** byte/gate_zoo quality equality vs pre-85 on the SAME
deterministic input + confirm no torn-MV.

### 6.3 STEP 1 BEFORE BUILDING: instrument the `submit_wait` split
The ~6.7ms GPU / ~4.7ms CPU split is **inferred from memory, NOT profiled on this build.** Split
`submit_wait` into submit-timestamp vs fence-wait-timestamp so `t_pair_ema` reveals the GPU-wait
fraction directly. Cheap stats change; confirms the whole premise. If the 1080 Ti SAD leg is
texelFetch-bound or >8.3ms at any moment, even pipelined cons falls below arr.

### 6.4 The headroom lever (only if 1440p / higher source-fps): HALF-RESOLUTION FLOW
The finest pyramid level L0 is **70.4% of all match SAD work** (tile-count dominated, not radius).
Computing flow at 960×540 + MV-upsample (the warp ALREADY bilinearly upsamples the 1/8-res MV,
`optical_flow_warp.comp:114` — free) cuts match work **3.96×** (~6.7ms → ~1.7-2.5ms). It is the
standard DLSS3/FSR3 architecture. **Quality risk LOW-MEDIUM** (sub-8px / thin-object precision;
the STAGE-20/24 confidence+agreement gates already blend low-confidence tiles — mitigates, doesn't
eliminate). MUST be measured on hud_fine / thin-object cases before shipping. All other GPU levers
(fewer levels, Rc trim, membar, async-intra-pyramid) are marginal or quality-negative — rejected.
CPU side: GPU-offloading the gme fit is a net loss (re-introduces the readback fence STAGE-85
removes); the right CPU lever (only if GPU drops below 4.7ms) is a separate fit thread.

### 6.5 HONEST COMPETITIVE CORRECTION (adversarial-verified — corrects standing overstatements)
The operator's "solid + innovative + beat LSFG (high-fps included)" was stress-tested by 3 skeptics.
Verdicts: STAGE-85 **holds-with-caveats**; "solid AND innovative" **holds-with-caveats**; "beat LSFG
including high-fps" **REFUTED**. The honest corrections:
1. **The "load-aware multi-GPU router" is NOT a runtime differentiator for FG.** First-hand:
   `break_even_decide()` returns `offload=false` ALWAYS for FG (AI≈2.5 vs ~596 crossover) and was
   **inlined as a constant** (`main.cpp:37-39`); the FG device is a **static `--fg-gpu` flag**
   (`671-676`); under the field-tested igpu-convert path primary-A FG is **hard-disabled**
   (`pfg_enabled=false @2546`). At runtime the device split is **as static as LSFG's**; only the
   interpolation FACTOR (`live_n_f`) adapts. The router exists and its FG verdict is correct — but it
   is a frozen one-time decision, not a live per-frame edge. **Stop citing it as "the 3-GPU edge."**
2. **The object-holon is an engineering COMBINATION, not a research-novel primitive.** Production
   MEMC (US 8259225/8289444/8576341; segmentation-map US 12328441) + 1995 ICIP object-based MEMC
   already do per-object MV + covering/uncovering. Novel = the *setting* (persistent multi-pair object
   identity as a CPU correction layer in a real-time EXTERNAL-CAPTURE tool), not the technique. The FG
   core (pyramid block-match + warp+blend) + WAP are textbook MEMC/VFI.
3. **STAGE-85 buys THROUGHPUT PARITY at 120fps (stop dropping ⅓ reals), NOT a win.** High-fps is
   LSFG's STRONGEST regime (its base latency is already low at a 120 source; FG adds ~+11ms at 4×;
   years-tuned flow; single-process loop) and our WEAKEST (external WGC capture + no-P2P host-bounce +
   serial CPU holon stack on one thread — taxes LSFG's loop does not pay). The +1-pair publish lag
   pushes our latency UP at exactly the moment it needs to be down.
4. **ZERO head-to-head flow-QUALITY measurement vs LSFG 3.x exists.** "Beat" requires beating quality;
   there is no evidence we do. **This is the missing gate for any "beat" claim.**
5. **The defensible, true target:** match-or-beat LSFG **latency under load at LOW-to-MID source fps**
   on this specific 3-GPU box (the operator's own original thesis), being the only tool that uses all
   three GPUs — conceding raw flow quality (unmeasured) and conceding in-chain DLSS/FSR (engine
   MVs+depth we structurally lack). STAGE-85 is necessary (removes a self-inflicted defect) but not
   sufficient for "beat."

### 6.7 ⚑ MEASURED (STAGE-85a `--fsub` on Star Rail 1080p/120, 2026-06-13) — THE PREMISE WAS BACKWARDS
The instrument ran live (Star Rail; LoL would not WGC-capture — fullscreen/Vanguard). The inferred
~6.7ms-GPU/~4.7ms-CPU split is **WRONG**. Measured, steady across the run:
- **`flow` (the blocking fwd fence wait = the 1080 Ti GPU leg) ≈ 3.7ms** — CHEAP, not 6.7ms.
- **`cpu` (pair − flow = gme fit + objects + memory) ≈ 6-8ms** — the DOMINANT serial cost.
- **`pair` ≈ 10-12ms** (= flow + cpu, fully serial, confirmed additive). cons ~67-86 (mostly ~75).
This is with `tier:2/tier:3` + `bwd-skip:100%` ALREADY firing (the holon work is already being
skipped part of the time) — yet cons still can't track arr ~116.

**Consequences for the plan (corrected lever ladder):**
1. **STAGE-85 forward-pipelining is STILL the right fix** — overlap pair N's GPU (3.7ms) under pair
   N-1's CPU (7ms) → per-pair F time collapses from ~10.7ms serial to `max(GPU 3.7, CPU 7) ≈ 7ms`
   (now **CPU-bound**) → cons ~140 → tracks arr 116. Floor is the CPU, not the GPU.
2. **HALF-RES FLOW (§6.4) is DEPRIORITIZED** — it cuts the 3.7ms GPU leg, which is NOT the floor.
   Pointless for 120fps throughput. (Still relevant only at 1440p where the GPU leg grows.)
3. **The real secondary lever is the CPU** (only if the ~7ms CPU floor is too tight — it spikes to
   ~9.7ms, occasionally clipping the 8.3ms budget): (a) trim gme IRLS 3→2 passes (~0.7ms, trivial);
   (b) **move the gme fit + object_repair to a SEPARATE thread** off the F critical path → F floor →
   GPU 3.7ms → cons ~270 (big headroom; bigger refactor; the right move only if 120fps isn't enough
   or for 1440p/higher-fps). For the immediate 120fps goal, STAGE-85 (cons ~140) suffices.

**Why the feel is "extremely bad" even post-STAGE-84 (from the same log):** lat SPIKES to 60-75ms
(vs ~26 baseline); the spin-guard lap-escapes fire every few seconds (`lap:3-5/s`, P frozen 64ms);
cons jitters 30-110. All three are downstream of cons<arr: P starves → 64ms freeze (the escape) →
latency spike. STAGE-85 attacks the root (feed P enough pairs). RISK (adversarial-flagged): the
+1-pair publish latency STAGE-85 adds (~7-10ms) could blunt the latency win — must be measured.

### 7. PROJECT-WIDE LEVERAGE AUDIT (operator's "is this systemic?" — answered, 2026-06-13)
Two workflows (`we1mpgvrj` render_assistant deep audit; `wyked13es` 6-region project sweep + 3
adversarial verdicts) answered whether the dogfooding gap (hand-rolling serial work that ignores
Phyriad's holons/pool/SIMD/gpu pillars) is systemic. **Verdict: the broad framing is REFUTED — the
gap is REAL but CONCENTRATED, not a project-wide hidden performance regression.**

- **FACT A (true):** primitive ADOPTION is near-zero repo-wide. `hal/Simd.hpp` = ZERO application
  consumers anywhere. holons parallel-for (`SplittableRange`/`DivisibleUnit`) = only 2 demos
  (`examples/holons_minimal`, `examples/mandelbrot`). `pool` = only `apps/eeg_nsga2`.
- **FACT B (the load-bearing claim, REFUTED):** all 5 framework regions + all audited apps returned
  **ZERO flagged missed-leverage sites.** Reason: the overwhelming majority of the project is
  control / orchestration / state-machine / IO / UI / hardware-probe code with **no heavy
  data-parallel loop to parallelize** — you cannot dogfood a parallel-for where no loop exists.
- **Where heavy data-parallel work genuinely lives, the RIGHT primitive is ALREADY adopted:**
  eeg_nsga2 → pool at the correct grain (one 5-fold SVM-CV per individual); render_assistant's
  per-pixel/MV work → GPU Vulkan compute shaders; mandelbrot → holons/pool/TBB.
- **The ONE measured hot CPU path is render_assistant's per-pair CPU tail** (§6.7, ~6-8ms) — and its
  fix is **STAGE-85 pipelining (concurrency), NOT pillar retrofit.** The only pillar-shaped sub-part
  is `gme_fit_affine`'s inner normal-equation reductions (SIMD-able; the IRLS outer loop is serial),
  worth at most a small **opt-in dogfooding experiment**, not a campaign.
- **Sobering prior art:** `docs/STL_AVOIDANCE_AUDIT.md` (2026-05-18) ran this exact "retrofit hot
  callsites" analysis → measured 3-8% aggregate, dominated by DON'T / DEFER verdicts (blanket
  replacement = ~0% measurable, high API + regression cost).

**The genuine narrow residual (a coherence finding, not a perf bug):** `hal/Simd.hpp` is essentially
**unbuilt and unused** (it ships only caps-detection + 2 fixed memcpy kernels — no math kernels — and
has zero consumers), and the holons parallel-for has **no production consumer** (demos only). For a
substrate whose *mandato de eficiencia* is a pillar, that is a real self-consistency gap — but it is
"low adoption because the applicable heavy work is rare + the SIMD pillar is thin," not "serial
regressions hiding across the codebase."

**HONEST PRIORITY (from the campaign verdict = REFUTED):**
- **[P0]** Ship STAGE-85 pipelining for render_assistant (the only measured-impact perf work; it is
  concurrency, not pillar retrofit). Instrument the submit/fence split first (§6.3 — done: `--fsub`),
  then the cmd-buffer-granularity overlap with the §6.2 torn-read discipline.
- **[P1, optional, only if the goal is DOGFOODING]** a SINGLE bounded experiment: SIMD-vectorize
  `gme_fit_affine`'s inner reduction behind a feature flag, gated by `hal::simd_caps()`, accepted ONLY
  if it clears a >5% microbench AND passes gate_zoo byte-identity. (Or lift a generic SIMD reduce into
  `hal/` via the FR procedure — the "proper substrate" move.) NOT a multi-site campaign.
- **[LEAVE SERIAL — by conscience]** eeg (pool already correct), ayama (documented single-thread),
  voyager (IO-bound/throttled/inherently serial), and every framework pillar (providers + control/IO/
  probe). Serial is the CORRECT state there.

### 7.1 STAGE-86 — complete hal/Simd for the FG CPU tail (MEASURED + design proven, 2026-06-13)
Operator directive: give Ayama's FG what it needs (the CPU tail) by **completing `hal/Simd` as a real
pillar** (max perf + scalable + easy to consume); don't touch the small project pieces. Measure-first
(`apps/render_assistant/bench/gme_fit_bench/`, standalone console — no overlay; verbatim copy of
`gme_fit_affine`/`half_to_float`, synthetic 240×135 RG16F, median of 400 iters):
- **`gme_fit_affine` MEASURED ~1.0ms/call solo (sub2=false), 0.43ms (sub2=true)** — NOT the inferred
  ~2.8ms (that was the COMBINED fwd+bwd pre-latch; the field's 2.7ms is under game+3-thread load).
- **the fp16 decode is ~44-51% of the call**, and `gme_fit` decodes each block **4×** (3 IRLS + the
  dissidence pass).
- **PROVEN design (byte-identical):** an F16C 8-wide batch decode (`_mm256_cvtph_ps`) is **bit-identical
  to the scalar `half_to_float` (0/64800 mismatches)**; the **decode-once** pattern (decode the grid to
  a float scratch ONCE via SIMD, run the 4 passes over floats) yields **byte-identical `out6` + dis-mask
  (0/6, 0)** and **1.48× (1.006 → 0.680 ms)**. Zero quality-regression risk (pure decode + dedup).

**The pillar piece = a reusable `hal::f16_to_f32_batch(src, dst, n)`** (runtime-dispatched on a new
`f16c` SimdCaps bit; scalar fallback) — consumed via the decode-once pattern across the WHOLE CPU tail
(gme_fit, object_repair's recompute, mem_*/matte grid-ops all decode fp16). "Easy to consume" =
runtime dispatch so the consumer's TU needs NO `-mavx2`: implement in a compiled TU with per-file
AVX2/F16C flags (the established `framework/transport/src/SlotCopy.cpp` + `bench/copy_bench` pattern:
`set_source_files_properties(... "/arch:AVX2"|"-mavx2")`) exposed via a small lib target consumers link.
**FR discipline:** lifting this into `hal/` is a pillar change → FEATURE_REQUEST_PROCEDURE + INVENTORY
+ hal doc-sync + FORMAL_DOCUMENTS_REGISTER. **Gate:** bench byte-identity (done) + the in-app A/B
(operator's run: dis:NN%/mass parity + the `--fsub` cpu drop) + both toolchains green.

### 7.2 IN-APP A/B (STAGE-86b live on Star Rail 1080p/120, 2026-06-13) — passed, and confirms STAGE-85
The `--fsub` field run AFTER the consumer wiring landed (aac2f3e):
- **STAGE-86b gate PASSED:** feel improved (operator), `er=0`/`to=0` throughout, no crash, no quality
  regression (byte-identical by construction). The codec dedup + decode-once are live.
- **`fsub` measured IN-APP:** `flow ~3.7-4.0ms` (the 1080 Ti GPU fence leg), `cpu ~5.4-9.3ms`
  (median ~6.5), `pair ~9.4-13.0ms`. The CPU shaved slightly vs pre-86b but is STILL the dominant
  serial term — as predicted, shrinking CPU magnitude alone barely moves cons.
- **cons STILL ~70-80/s vs arr ~116-122/s** (~65%) — the ~1/3 dropped reals persist. The operator
  still feels frame jumps; the worst ("saltos muy intensos") are the lap-escapes (`P pinned 64ms`,
  `lat` spiking to **69-95ms**, cons dipping to 32-57) — P starving because cons<arr.
- **THE DATA NOW PROVES STAGE-85 IS THE FIX:** flow 3.8 + cpu ~6 = ~10ms SERIAL > 8.3ms budget.
  Overlapped (forward-pass pipelining) → `max(3.8, 6) ≈ 6ms < 8.3ms` → cons tracks arr → jumps gone.
  STAGE-86b lowered the CPU floor + closed the dogfooding regression; **STAGE-85 (§6.1) is the
  throughput lever the field now empirically demands.** (Secondary: the 64ms spin-guard bound could
  be shortened / value-stability-escaped, §6.3 — symptom relief for the worst jumps if STAGE-85 slips.)

### 7.3 STAGE-85 A/B (supervisor, gate_zoo HF85 1080p@8ms, 2026-06-13) — ON works but has a defect; default stays OFF
Ran `--fwd-pipeline` ON vs OFF on the HF zoo (screen free). The zoo caps at **arr~67** (GDI/WGC) so
it does NOT reproduce the HSR deficit — at arr 67 both OFF and ON have cons=67=arr (no ⅓ to recover),
so **the cons benefit is UNVERIFIED here** (needs the operator's HSR run at arr~120).
- **ON is correct + the overlap WORKS:** er=0, no crash, and `fsub` shows **flow:0.0** (vs OFF flow:4.0)
  — the GPU fence is already signalled when F reaches the deferred wait, i.e. the GPU leg is hidden.
- **DEFECT the A/B exposed (not visible in a build):** in ON, `t_pair_ema` is mis-measured — the
  deferred consume of pair N-1 runs inside pair N's iteration, so `now − tp0_{N-1}` includes the
  inter-arrival gap (~15ms at arr 67) → `pair:~21ms` (vs OFF ~12.6ms). That pollutes the bwd-skip +
  tier control signal → ON over-fires **tier:2/3 + bwd-skip:100% at LIGHT load** (OFF runs tier:0-1,
  bwd-skip:25% there), needlessly throttling the holon. Moot at HSR (both tier:2/3), but a real
  control-signal bug at light load. ON also adds ~+7ms latency (the +1-pair shift, as predicted).
- **VERDICT: default stays OFF (safe, byte-identical). ON is NOT ready to flip default-on** — needs
  STAGE-85b: measure `t_pair_ema` as the actual F-work duration (not wall-clock-since-WAP-entry) so
  the tier signal is correct in pipeline mode. The cons benefit still awaits the operator's HSR
  `--fwd-pipeline` run (the only source of the real arr~120 deficit).

**STAGE-85b LANDED + verified (supervisor, gate_zoo, 2026-06-13):** the fix = in pipeline mode measure
`t_pair_ema = now − tf0` (consume duration = fence-wait + CPU = the real F-busy cost) instead of
`now − tp0` (which folded in the next pair's ingest-wait); SERIAL path unchanged (`now − tp0`) → OFF
byte-identical. Re-ran `--fwd-pipeline` on the zoo: **`fsub pair` 20.9ms → 7.0ms** (now = the true
F-cost), **tier:2/tier:3 over-fire GONE** (0 of 52 windows, was constant), er=0. The residual
`bwd-skip:100%` in ON is NOT the bug — it is the documented structural limit (pipeline forces
do_bwd=false: single ofp / 2 Bframe slots; irrelevant in the HSR regime which already runs
bwd-skip:100%). Key arithmetic now clean: pair ~7ms (= CPU; GPU hidden, flow:0.0) < 8.3ms/120fps
budget → at HSR, cons should track arr, and the corrected tier signal won't throttle it. The cons
win itself still needs the operator's HSR `--fwd-pipeline` run (zoo arr~67 has no deficit to show).

### 8. ⛔ STAGE-85 pipelining FAILED in-app (operator HSR@120 A/B, 2026-06-13) — abandoned
The operator's HSR@120 `--fwd-pipeline` run (the real arr~120 deficit the zoo can't make) is in:
the pipeline is a **catastrophic failure**, NOT the fix.
- **ON (`--fwd-pipeline`): cons CRASHED to 13-25/s** (vs arr ~115, vs the serial ~75) — WORSE than
  serial. **lat exploded to 66-139ms.** Operator: "literal 5fps + hallucinations." The `fsub pair`
  *did* read correctly (~6-13ms, STAGE-85b worked) — but it didn't matter: cons is gated not by the
  pair cost but by **pathological P-pinning**: lap-escapes fire 3-8/s ("P pinned 64ms"), each freezing
  F ~64ms → cons collapses. The +1-pair publish lag + the g_seq-leads-f_seq protocol leave P without a
  fresh generation → it pins on the spin-guard constantly. The overlap theory did not survive the F↔P
  coordination. The hallucinations = ~16 real anchors stretched over 240 presented frames.
- **OFF (control): unchanged** — cons ~75/115, lat ~30, fsub flow:4.0 pair:10 cpu:6. STAGE-85b kept
  OFF byte-identical (operator: "se siente como antes"). No regression in the default.
- **VERDICT: abandon STAGE-85 pipelining as designed.** The in-app gate caught it (default stays OFF
  — nothing shipped). Fixing the P-pinning would mean rethinking the F/P generation protocol under the
  +1-pair lag — a deep, risky change for the WRONG regime (HSR@120 is high-fps, the architecture's
  weakest). The remaining serial lever (move CPU fit/objects to a 4th thread → floor = GPU ~3.8ms) is
  another big delicate concurrency refactor carrying the SAME risk profile the pipeline just realized.
- **HONEST STRATEGIC POSITION (confirmed by this failure):** HSR@120 is past the architecture's
  throughput wall (serial pair ~10ms > 8.3ms budget; the overlap lever just failed). This is the
  ceiling the operator already sensed ("creo ya estamos tocando techo"). The defensible win is
  **LOW-fps sources (30-60→240)** where the 16-33ms budget clears the ~10ms pair → cons tracks →
  smooth + flat latency (the operator's own thesis: LSFG bleeds latency at low-fps+high-FG). Pivot
  there, OR accept the HSR@120 ceiling. STAGE-85/85b stay committed but default-OFF (a flagged,
  documented dead-end; revert is optional cleanup if the pipeline approach is fully dropped).

### 6.6 Updated resume order
1. **Instrument** the submit/fence-wait split (confirm ~6.7ms) — do FIRST, cheap.
2. **STAGE-85** fwd-pipelining (§6.1) with the §6.2 torn-read discipline; gate = cons≈arr at 120 +
   gate_zoo quality byte-equal + lat not regressed by the +1-pair shift.
3. **The missing measurement:** a head-to-head latency + flow-quality A/B vs LSFG (on LoL@120 or
   gate_zoo clips) — this, not STAGE-85, is what decides whether "beat" is real.
4. (Headroom, later) half-res flow (§6.4) for 1440p / higher-fps, measured for quality first.

---

### 9. ⟲ The premise was wrong — it is a REGRESSION, not a ceiling (operator correction, 2026-06-13)

§8's strategic conclusion ("accept the HSR@120 ceiling / pivot to low-fps sources") was built on a
framing the operator **explicitly overturned**, and that framing was the supervisor's, not his:

> "Ayama funcionaba en parte en 120 fps, **nunca fue un mejorador de fuentes de bajos fps, esa
> afirmación la hiciste tú** — solo mejora la sensación que justo LSFG tiene por su diseño
> arquitectónico. Ahora estamos en una **clara regresión**, después de todos los fix de alucinación.
> El objetivo de Ayama es **superar LSFG**, e íbamos en buen camino. **Algo cambió después de los
> fixes de generación de frames y la mejora de alucinaciones.**"

So the §8 "ceiling" was a misread: Ayama *did* deliver fluidity at 120fps, then **lost it**. The
target is LSFG's by-design fluidity, at the fps it already worked at — not a retreat to low-fps. §6.6
item 2 (STAGE-85) and §8's pivot are **superseded by this section**.

#### 9.1 Regression CONFIRMED — culprit = cumulative CPU tail, NOT a discrete bug (status: **diagnosed**)
A 3-lens read-only investigation (anatomy / archaeology / recovery, Opus agents, 2026-06-13)
converged: the F-thread walks **one serial chain per consumed pair**, entirely upstream of
`stat_cons.fetch_add(1)` (main.cpp:~4030). The STAGE-48→66 hallucination-fix campaign piled CPU work
into exactly that chain:

| Stage | Added to the serial F-pair tail | Cost under load |
|---|---|---|
| STAGE-48 bidir | the whole second (backward) match+fit pass | dropped (bwd-skip:100%) |
| STAGE-52 GME | the affine IRLS fit | ~2.7ms |
| STAGE-53b dual matte | **doubled** the fit (both anchors) | (in the 2.7) |
| STAGE-61 object-holon | BFS + identity + chamfer + recompute | ~2.4ms |
| STAGE-66 scene-memory | advect / merge / refresh | ~1–1.5ms |

Sum: CPU tail ~6ms **+** GPU fwd-fence leg ~3.8ms = **serial pair ~10ms > 8.3ms/120fps budget** →
cons ~75 < arr ~115 → ⅓ of reals dropped → the felt jumps. The record shows `cons==arr` through
STAGE-55 (kickoffs.md:1321) and the deficit appears in the **unmeasured STAGE-56→82 window** — the
fall below arr at 120fps is *undated* (no single-commit bisect; attributed to accumulation). Two prior
supervisor hypotheses were **refuted** by the read:
- **The STAGE-84 spin-guard (64ms) is NOT the cause** — it is a *symptom amplifier*. It freezes F only
  when P is already starving *because* cons<arr. Remove the deficit and it stops firing.
- **Half-res flow is REJECTED for 120fps** — it cuts the GPU leg, which is *not* the floor (the CPU is);
  it is a 1440p lever and the only proposed change with real thin-object quality risk.

#### 9.2 The recovery — shed serial work, never overlap (the constraint STAGE-85 violated)
The architectural fact that makes shedding **safe**: `gme_fit_affine` (main.cpp:1077) writes the
**complete** dissidence mask + 6-param affine model on its own; `object_repair` and the `mem_*` lambdas
only **refine** it; the matte/warp consume gme's output directly and **already** run on the un-refined
mask every 2nd/4th pair (the existing `holon_skip_pair` gates). A hard "shed refinement entirely" tier
is just the *every-1st* case the matte already tolerates → byte-identical when not in deficit.

#### 9.3 STAGE-87 — BUILT + flags verified; cons recovery + quality trade UNMEASURED (status: **designed/built**)
Two flag-gated, default-OFF levers (the STAGE-85 precedent: a throughput lever ships OFF until the
operator's eyes confirm). Both build green on **build-ra-msvc2 (MSVC/Ninja)** and **build-ra-mingw**;
parse verified; default behavior byte-unchanged (tier 4 unreachable without the flag; 3 IRLS passes).

1. **`--deficit-tier` (the primary move):** a **tier-4** above the STAGE-84 ladder. Escalate at
   `t_pair_ema > 1.85×budget`, de-escalate to tier 3 at `< 1.50×budget` (dead-band [1.50,1.85] vs
   tier-3's 1.20× drop → no flap). At tier 4, `shed_holon` drops `object_repair` + scene-memory on
   **every** pair (not every-4th), keeping gme-fit + matte + warp + inertia = the raw-flow WAP that
   worked at 120fps pre-STAGE-61. Projected: shed ~3.4–3.9ms → pair ~6–6.6ms < 8.3ms → cons should
   track arr. **Quality cost lands ONLY under deficit**; hysteresis restores full hallucination quality
   the instant the source eases below 0.80×budget. (main.cpp tier ladder ~3863, gate ~3876.)
2. **`--gme-irls2` (cheap, always-on under flag):** `gme_fit_affine` runs 2 IRLS passes (OLS + 1
   Geman-McClure reweight) instead of 3 — ~0.3–0.7ms off **every** pair on the dominant axis. Default
   3 = byte-identical to STAGE-86b. (main.cpp:1098, threaded via a new `irls_iters` param.)

Held for later: **SIMD-ize object_repair's per-block recompute** via the FR-HAL-1 / `ra::decode_f16`
batch (byte-identical; raises the load at which tier-4 trips, preserving more quality) — only if 1+2
leave the budget tight.

#### 9.4 The A/B protocol (the gate — needs the operator's screen)
The cons recovery and the quality-under-deficit trade are **projections from the diagnosis numbers,
not measured** — the gate_zoo caps at arr~67 and cannot make the 120fps deficit (§7.3). Required run,
HSR@120 (the real arr~120 deficit):
- **Baseline:** `--fsub` alone → confirm the standing cons ~75 / arr ~115 / lat / `tier:2/3`.
- **Recovery:** `--fsub --deficit-tier --gme-irls2` → read whether `tier:4` engages, cons climbs
  toward arr, lat falls, and — **the operator's call** — whether the felt fluidity returns and the
  shed-state hallucination level (under load only) is acceptable vs the jumps it replaces.
- **Decision:** if it delivers, flip the defaults ON + commit; if the shed-state quality is wrong,
  fall back to 1+SIMD-object_repair (keep quality, smaller throughput win). The North Star holds
  either way: full quality when there's budget, fluidity when there isn't — each when affordable.

#### 9.5 STAGE-87 deficit-tier CORRECTED (supervisor, 2026-06-13) — the threshold was doubly broken; rebuilt, default still OFF
Returning to Ayama after the CPU-substrate epic, a first-hand re-read of the tier ladder
(`main.cpp:3886-3917`) found STAGE-87's `--deficit-tier` was **doubly broken** and so **never did
anything** (which is why the prior A/B saw "tier-4 never fired"):
1. **Escalate unreachable:** tier 3→4 required `t_pair_ema > 1.85×budget`. `pair_budget_ms =
   src_interval_us/1000` ≈ **8.5 ms** at arr~120, so 1.85× = **15.7 ms** — but the measured deficit
   pair is only ~10-11 ms (~1.2×budget, §7.2). The gate could never trip.
2. **De-escalate shed-corrupted:** tier 4→3 fired at `t_pair_ema < 1.50×budget` (12.75 ms). Shedding
   *drops* the pair to ~7 ms (≪ 12.75) — so even if tier-4 were reached, it would de-escalate on the
   very next pair → could never sustain. The `[1.50,1.85]×` dead-band was calibrated for a ~1.5×-budget
   pair this regime never produces.

**Fix (the corrected lever):** the deficit shed-tier is now a **budget-relative OVERRIDE** sitting
beside the STAGE-84 sub-ladder (whose 0-3 rungs + constants are UNCHANGED): ENGAGE tier-4 when
`t_pair_ema > 1.10×budget` (the cons<arr deficit, ~9.4 ms — reachable by the ~10-11 ms pair); HOLD
until the SHED pair drops below `0.65×budget` (~5.5 ms). The shed pair (~0.8×budget ≈ 7 ms) sits
INSIDE the `[0.65,1.10]×` dead-band, so tier-4 **sticks** instead of flapping, and only releases when
the source eases enough that full quality refits. **Still `--deficit-tier`-gated, DEFAULT OFF →
byte-identical ladder when the flag is off** (tier-4 unreachable; tiers 0-3 untouched). Built green on
**build-ra-mingw** AND **build-ra-msvc2** (vcvars). It is a pure THROTTLE (does *less* work per pair) —
**none of STAGE-85's concurrency/P-pinning risk**, fully reversible.

**The A/B is unchanged (§9.4) and still the gate** — only now tier-4 will actually engage at HSR@120:
`--fsub --deficit-tier --gme-irls2`, watch for ` tier:4` in the stats line + cons climbing toward arr +
lat falling, and judge the shed-state (raw-flow WAP, no object_repair) quality-under-load. The cons
recovery + quality trade remain the operator's screen call (the zoo at arr~67 has no deficit to show,
and with the corrected gate `--deficit-tier` on the zoo should leave tier-4 OFF — no spurious shed).

---

## 10. BUDGET-ELASTICITY — the root design + the honest competitive map (deep analysis `wf_6e79ce68`, 2026-06-13)

*Operator reframe (binding): the budget is NOT 120fps — it is the VARIABLE `= src_interval = 1/source_fps`
(120→8.5ms, 240→4.2ms, 360→2.8ms, 500→2.0ms). Make the FG pipeline budget-ELASTIC across the whole
spectrum so we WIN every regime our architecture's tradeoffs permit and never lose by a self-inflicted
design error; concede honestly where a real tradeoff loses. Measure what we can (240Hz box) + extrapolate
PARAMETRICALLY (per-pass work is fixed per resolution; only the budget shrinks) — that is dogma-compliant
(measurable + parametric), a hard-coded fps threshold is the opposite. A 4-lens workflow (cost-model /
regime-ladder / SOTA / adversarial arch-sufficiency) + 24-claim adversarial verify (0 refuted, 16 honesty
refinements applied below) produced this.*

### 10.1 Cost-model (foundation, first-hand)
- **Work grid CLAMPED to 1920×1080** (`main.cpp:1823` `WW=min(NAT_W,1920)`); at 1440p/4K the FG work stays
  1080p unless the clamp is lifted — so 1440p rows are DOUBLY-designed (the binary does no 1440p flow today).
- Every F-pair pass loops over `mvw·mvh` (∝ resolution) — **NO source-fps or output-refresh term in any cost**
  (verified). F produces one MV grid per consumed real-pair; P's per-tick warp (`:4852`, ∝ pixels) scales with
  output-refresh but is P's cheap decoupled cost. ⇒ **per-pass work is FIXED per resolution; the budget is the
  only fps-variable.** The parametric extrapolation the operator wants is sound.
- Scaling laws (corrected): both legs **O(pixels)** (1440p ×1.778, 4K ×4.0). `gme_fit` cost ∝
  `nblk·(c_decode + irls·c_reduce/step² + c_diss)` — AFFINE in IRLS with a large IRLS-independent floor (decode
  + dissidence ≈ half the call). `object_repair` = O(nblk) BFS + per-cluster fill (STAGE-63 shape-field arm).
  L0 dominance = **75.2%** of match-SAD (corrected from 70.4%); half-res cut **3.96×** verified.
- MEASURED 1080p@120 (post-86b): flow 3.85ms, cpu ~6.5ms (gme 2.7 + object_repair 2.4 + scene-mem 1.4), serial
  pair ~10.35ms > 8.33ms budget. Tier-4 SHED pair ≈ **6.7ms** (DESIGNED/derived; confirm directly via `--fsub
  --deficit-tier`). **Win envelope of the CURRENT code: full quality ≤~96fps@1080p; tier-4 shed ≤~150fps@1080p.**

### 10.2 The lever ladder (cheapest-quality-cost first) + the architectural floor
| Lever | ms saved (1080p) | Quality cost | Engages where |
|---|---|---|---|
| **R1** shed scene-memory | ~1.4 (designed) | loses cross-pair persistence; per-pair gme still corrects | budget < full pair |
| **irls 3→2** (`--gme-irls2`) | ~0.5 (designed) | drops marginal 2nd reweight | always-on cheap rung |
| **R2** shed object_repair (= current binary tier-4) | +2.4 (cum ~3.8) → pair ~6.55 | falls to raw-flow WAP (pre-STAGE-61) | the load-bearing 120fps WIN |
| **half-res flow** (960×540) | flow 3.85→0.97 (3.96×) | sub-8px/thin-object risk (measure-first) | **1440p / high-res only** — useless at 1080p (CPU is the floor, not flow) |
| **off-thread CPU** (4th fit-thread) | floors pair → flow leg (~3.85ms) | — | the ONLY 240fps@1080p path — but **STAGE-85 P-pinning risk class** (§8); needs F/P protocol redesign first |
| **source decimation** (1-of-N reals) | breaks pair-per-real | the dropped-anchor look made deliberate (DRS-grade) | budget < flow leg (>~260fps) — the constraint-breaking floor |

**The genuine architectural floor = ONE GPU flow leg per source-pair** (~3.85ms full / ~0.97ms half): below
`budget < flow_leg`, no lever that keeps one-grid-per-real fits → only decimation. That is a **real tradeoff**
of external-capture + single-flow-engine, not a bug.

### 10.3 The ROOT FIX — the budget-aware quality scheduler (a Phyriad pillar primitive)
Generalize the binary deficit-tier into a **`BudgetGovernor`/`QualityLadder`**: ordered work-stages with measured
cost EMAs + a quality rank + a live budget + hysteretic engage/release; per tick, pick the **shallowest** rung
set whose projected pair < budget (DRS shape). **Unify** the consume-side shed ladder with the present-side
`live_n_f` factor-degrade (`:4080-4100`, already budget-elastic) against ONE budget signal — the **DLSS-4.5-Dynamic
shape** (SOTA, §10.4). Lift into `framework/orchestration/` next to `CircuitBreaker`/`PressureScore` (same
measured-pressure→graded-response family) **via the FR procedure** — it has a real consumer (Ayama), is measurable,
codifies an existing hand-rolled controller (no-speculative-generality), and does LESS work under pressure
(zero-overhead-when-off; none of STAGE-85's concurrency risk). This is the dogma-compliant root fix.

### 10.4 The honest competitive map vs LSFG (adversarial; "win where we can, concede honestly")
**SOTA VINDICATES the parametric thesis:** DLSS 4.5 Dynamic MFG (Mar 2026) = a continuous controller keyed on the
GPU/refresh gap (NVIDIA endorsing the operator's exact thesis); DRS = measure-cost-vs-budget-shed-in-fine-steps;
LSFG AFG = adapt the FG factor. Ayama already has the present-side half (`live_n_f`); the unfinished half is the
consume-side cost budget.

- **WHERE WE WIN (lean in):** (1) **low-to-mid source-fps latency-under-load** — budget 16-33ms ≫ ~10ms pair → cons
  tracks arr, flat latency; LSFG pays its capture tax here (+~13ms at 40→80, measured). (2) **multi-GPU: the game
  keeps its GPU** — LSFG steals from the game's single GPU; ours runs flow on the spare 1080 Ti (honest framing:
  "FG that doesn't tax the game's GPU"; the split is STATIC, not a runtime router — stop citing it as that).
  (3) **the budget-elastic holarchy itself** — graded graceful quality degradation; LSFG only adapts the FACTOR,
  it has no graded-QUALITY axis. (4) **generality** (any game/engine/vendor — shared moat with LSFG vs in-chain
  DLSS/FSR/XeSS). (5) **holonic occlusion/object-identity layer** — a quality CANDIDATE vs LSFG's ML, **gated on
  §10.5 measurement** (could win disocclusion or not — unproven).
- **WHERE WE CONCEDE (real tradeoffs, not bugs):** high-source-fps (240+) raw-flow quality + pacing — no engine
  MVs/depth (we reconstruct from pixels), no in-engine Reflex/XeLL, no hardware flip-metering (Blackwell display
  silicon), WGC capture + no-P2P host-bounce, and at 500Hz whether WGC even delivers 500 distinct frames/s is
  unverified. The budget-scheduler buys the **120fps@1080p** cell; 240fps needs off-thread CPU (STAGE-85 risk);
  360/500 is below the flow leg (real wall). The +1-pair latency of ANY overlap pushes latency UP exactly where
  it must be low.
- **The ONLY self-inflicted losses:** (i) the un-shed CPU tail under deficit at 120fps (FIXED by the corrected
  deficit-tier, §9.5, default OFF); (ii) any hard-coded fps threshold (SOTA uses continuous controllers).

### 10.5 THE GATE for any "beat" claim — flow-quality A/B vs LSFG (designed, NOT run)
Every "WIN" above is **throughput** (cons tracks arr). **A throughput win is not a quality win** — zero head-to-head
flow/interp-QUALITY measurement vs LSFG 3.x exists. Protocol (designed): identical offline clips through both tools;
**VFI held-out-frame accuracy** (decimate a 240+ ground truth, interpolate the dropped frame, compare to truth) on
PSNR/SSIM/**LPIPS** + a **disocclusion-region** mask + temporal-artifact rubric (double-blind) + latency-under-matched-
rate. Honest adversarial prior: **Ayama likely LOSES raw flow quality (no MVs, younger model) and must win on
latency-under-load at low-to-mid fps + generality** (the §6.5 #5 defensible target). The protocol PROVES the conceded
position with numbers; it does not manufacture a win.

### 10.6 Recommended path (what's smart, root-first, dogma-clean)
1. **[BUILD — root fix, low-risk]** the `BudgetGovernor` graded ladder + the corrected deficit-tier as its first
   rung, unified consume+present against one budget signal. Lifts into orchestration via the FR procedure. Wins the
   120fps cell properly + is parametric for any budget. The operator's HSR@120 A/B (§9.4) is still the gate for the
   shed-quality trade.
2. **[MEASURE — the "beat" gate]** the §10.5 flow-quality VFI A/B vs LSFG. This, not throughput, decides "beat."
3. **[DEFER — high-risk / wrong-regime]** off-thread CPU (240fps lever, STAGE-85 P-pinning class — needs an F/P
   protocol that tolerates decoupled publish first); half-res flow (1440p lever, measure thin-object quality first);
   source decimation (graceful-degradation floor, operator's-eyes threshold).
4. **[CONCEDE — honest]** high-fps raw-flow-quality dominance + pacing-at-high-refresh (real tradeoffs). Win
   low-to-mid fps + generality + (pending #2) the holonic quality layer.

---

## 11. THE FG-QUALITY TEST-FIELD — instrument blueprint (operator-directed, 2026-06-13)

*Operator reframe (binding): before refining/comparing FG quality, build the **complete automated test-field
that removes human error** — extend the LIVE app (the `render_assistant.exe --window …` instrument that already
prints the real-time arr/cap/cons/lat/gme/tier/gpu% telemetry) into a serious metrics instrument that gives ALL
metrics of the WHOLE architecture in real time. His manual method was: record 240fps time-extended (5s→~35s ≈ 7×),
decompose into frames, eyeball the artifacts. The shared 240fps frame-decomposition (PLAYER_01 ball) shows the
canonical **crossfade artifact**: the interpolated frames carry a double-disc/crescent-seam edge + ghosted text =
`α·posN + (1-α)·posN+2` with a visible seam, NOT a clean motion-compensated single object — on the INTERPOLATED
frames (~2 of 3 presented at 240Hz when cons~80<arr~120). The test-field must turn that eyeball judgment into a
NUMBER.*

### 11.1 Honest correction (verified): NO head-to-head flow-quality vs LSFG exists
First-hand: there is **no completed FG flow-quality (ghosting/disocclusion/crossfade) measurement vs LSFG** in the
repo. The one MEASURED LSFG comparison is STAGE39's slow-mo "% uniform transitions" (LSFG 84% vs ours 74.6%) — a
**pacing** metric, doc says explicitly "the gap is uniformity, not content correctness." "LSFG dominates us in
quality" is a documented *expectation* (§10.5 adversarial prior) + the operator's manual single-tool eyeball of OUR
artifacts — not a measured head-to-head. The test-field is what makes any quality claim real.

### 11.2 What already exists (build on it) — verified
- **Offline VFI scorers** (the seed): `framework/render/vulkan/bench/stage11_blockmatch_quality/` +
  `stage14_hierarchical/` + `stage31_extrapolation/` (the last has a `disocclusion` preset). All are offline,
  file-fed `.rgba`-triple, **PSNR+SSIM** scorers with a **held-out-midpoint ground truth** (`prep_frames.py` renders
  MID at t=0.5, never from an interpolator) that re-run the committed `OpticalFlowPipeline`. GPU-independent on quality.
- **In-app readback**: `--outdump` (`wapOutA`→`hOutD_a`, the synthesis plane, `main.cpp:4899-4906`) and `--pairdump`
  (the 2 warp anchors `hostR[prev_slot]`/`hostR[rs]` + `pair_c`/`prev_cseq`/`span`/slots, `:5253-5263`). `hostR[]` is
  host memory (`:2251`) → dumping reals is pure file-write, NON-perturbing.
- The gate_zoo (`frames/gate_zoo.ps1`, a synthetic GDI scene of balls/flags/HUD/tunnels/translucency captured live)
  is a **stimulus generator, eyeball-judged, capped arr~67 by GDI/WGC** — NOT a scorer.
- **Missing**: any file-input INTO the live app (capture is DD/WGC live-only); LPIPS; a crossfade metric; a live
  quality number.

### 11.3 The held-out method (the human-error eliminator)
You cannot withhold a real LIVE without creating the very artifact (skip a real = 2× gap = the decimation/crossfade).
So: a **non-perturbing held-out** — capture 3 CONSECUTIVE reals (N, N+1, N+2), use (N, N+2) as the FG anchors,
interpolate at t=0.5, and score the result against **N+1, the real that actually arrived = the ground-truth
midpoint.** True PSNR/SSIM/LPIPS on live game footage, no slow-mo camera, no eye. (Under the deficit, dropped reals
ARE free ground truth — the worst-crossfade regime self-supplies the truth.)

### 11.4 The three build layers (gated, DEFAULT-OFF, byte-identical when off)
1. **`--qdump N` (data tap, low-risk, FIRST).** A 3-deep host snapshot ring fed at the capture/FrameArrived write
   (the `c_slots[]`/ring at `main.cpp:2474/2936`, `kCapSlots=4`); every N reals, write the 3 consecutive reals (+ the
   live `wapOutA` for the full-pipeline number) as `.rgba` + a manifest in the stage11 format. Pure file-write of
   existing host buffers — no new Vulkan, no perturbation of the present path. Automates the operator's manual
   capture.
2. **Offline scorer (the NUMBER producer).** A tool generalizing stage11/31: consume the `--qdump` manifest; (a)
   re-run flow+warp on (N, N+2)@0.5 → the FLOW+WARP quality vs N+1; (b) score the dumped live `wapOutA` vs N+1 → the
   FULL-pipeline quality (incl. object_repair/scene-memory — "toda la arquitectura"). Metrics: **PSNR + SSIM + LPIPS**
   (perceptual) + **disocclusion-region error** + the **CROSSFADE metric** = residual of the interpolated frame
   against the pure α-blend model `α·N+(1-α)·N+2` (LOW residual ⇒ it IS crossfading; a clean warp does not match the
   blend) + **double-edge energy** (spurious internal edges/gradients not present in either anchor nor the truth =
   the crescent seam). The "post-run analysis in parts" the operator accepts when live cost is high. Reuses the
   verified stage11/31 PSNR/SSIM core.
3. **`--qprobe` (live shadow, real-time number, heavier, LAST).** A non-perturbing periodic shadow that runs the
   FULL `consume_wap` on the held-out triple off the present path, scores it on host, emits a rolling quality EMA in
   the stats line + dumps the worst triples (generated/truth/diff). Measures the whole architecture's quality LIVE.

### 11.5 First-class artifact metrics (operator's manual-experience list)
Crossfade (the §11 evidence), trails (motion-stretch ghosts), disocclusion (covering/uncovering edges — the
object-holon's job), HUD halo (static-overlay edge artifacts), directional degeneracy (the gate_zoo's striped-flag
witness — flow ambiguity on 1-D textures). Each a dedicated column the scorer reports, so "refine the bases" is
driven by numbers, and the with/without-tier-4 (or with/without object_repair) delta quantifies what each holonic
layer actually fixes.

### 11.6 Build discipline
All three layers gated default-OFF (the STAGE-85/87 precedent — byte-identical live behavior when off, verified
both toolchains). Validation: it builds (mingw + msvc/vcvars), default-off byte-identical, and the metric core is
reused from the verified stage11/31; the LIVE quality numbers are then the operator's runs (the zoo as deterministic
stimulus + real games). Build order: 1 (qdump) → 2 (offline scorer) → 3 (qprobe). This is the durable spec; execute
as a focused build, not a rushed in-place edit.

### 11.7 SOTA grounding + the continuous time-extended harness (BUILT + verified)

**SOTA (deep-research harness, adversarially verified — 22/25 claims survived 3-vote refutation):** the
external-capture VFI-quality recipe is the **held-out-frame protocol** (decimate a *deterministic* high-rate
source, reconstruct the dropped frame, compare to the withheld real — Middlebury / Vimeo90K / Adobe240; the
**MSU VFI Benchmark** runs it at 1080p with a 5-metric stack PSNR/SSIM/MS-SSIM/VMAF/LPIPS). The load-bearing
finding: **global PSNR/SSIM correlate poorly with perceived VFI artifacts** because the artifact is *localized
to the moving region* (the **BVI-VFI** conclusion: 33 metrics benchmarked, "urgent requirement for more accurate
bespoke quality assessment methods for VFI"). The SOTA fix = **flow-weighted / motion-masked** metrics
(**FloLPIPS** weights LPIPS by the per-pixel optical-flow discrepancy). This **vindicated the operator's eye**
(global `dbl_edge` diluted the q1 ghost ~13× against the static background).

**References (FDP §2.3 verification levels; access date 2026-06-13).** Per the verifiable-reference mandate, the
load-bearing externals were re-fetched first-hand by the supervisor (not just the harness):
- **FloLPIPS** — Danier, Zhang, Bull, "FloLPIPS: A Bespoke Video Quality Metric for Frame Interpolation",
  [arXiv:2207.08119](https://arxiv.org/abs/2207.08119) — **[V1]** (title + the flow-discrepancy-weighting
  mechanism confirmed in the abstract).
- **BVI-VFI** — Danier, Zhang, Bull, "BVI-VFI: A Video Quality Database for Video Frame Interpolation",
  [arXiv:2210.00823](https://arxiv.org/abs/2210.00823) — **[V1]** (540 seq / 36 src / 189 subjects / >10,800
  ratings / 33 metrics / the "bespoke QA needed" conclusion all confirmed first-hand).
- **VFIPS** — Hou, Ghildyal, Liu, "A Perceptual Quality Metric for Video Frame Interpolation",
  [arXiv:2210.01879](https://arxiv.org/abs/2210.01879) — **[V1]** for (VFI-specific, Swin spatio-temporal,
  "per-frame metrics ignore temporal info"); the specific *ghosting/object-deformation* quotes are **[V2]**
  (paper-body, not in the fetched abstract).
- **VFIPQA** — Han et al., "Perceptual Quality Assessment for Video Frame Interpolation",
  [arXiv:2312.15659](https://arxiv.org/abs/2312.15659) — **[V2]** (title + no-reference triplet-net confirmed;
  the **SROCC≈0.13 for PSNR** figure is in the PDF's results table, *not* author-verified — harness-transcribed).
- **MSU VFI Benchmark** (videoprocessing.ai) — **[V2]** (the 5-metric @1080p stack, via the harness; not
  re-fetched first-hand here).

**Could not verify first-hand (FDP §2.4):** the `SROCC≈0.13` magnitude (VFIPQA Table I) and the MSU 5-metric
methodology page were *not* re-fetched by the supervisor; they rest on the deep-research harness's adversarial
pass. Treat the qualitative ordering (PSNR/SSIM ≪ LPIPS/DISTS ≪ bespoke) as the design driver, not the absolute
number. The earlier draft of this section cited these as if author-verified and carried no verification levels —
corrected 2026-06-13.

**Built on the §11.4-layer-2 scorer + validated first-hand (this session):**
1. **`dbl_edg_m` (motion-masked double-edge)** — the global seam energy restricted to + normalised by the moving
   region (`|luma(prev)−luma(next)|>thr`). Undilutes a localized ghost; on `our_zoo` q1 jumps 0.94→12.48 (its
   ghost occupies 7.6% of the frame). **PRIMARY artifact metric.**
2. **Continuous held-out `sequence` mode** — the operator's "extensión de tiempo": a `sequence <prefix> <count>`
   manifest directive decimates a continuous recording/synthetic source into held-out triples
   (prev=2j, mid=2j+1 HELD OUT, next=2j+2), scored per frame over the whole segment. Source =
   **`prep_zoo_sequence.py`** (procedural, deterministic, resolution-agnostic — the SAME instrument measures
   quality at 720/1080/higher; NOTE: the budget already FAILS at HSR-native 1080p, so 1080p is the failing
   environment, not a safe ceiling) emitting 6 motion types AT ONCE (pan/pan_diag/accel/zoom/orbit/mixed).
   Validated: clean translation PSNR 30 dB / `dbl_edg_m` 0.07 vs non-translational ghost 17–18 dB / 1.1–1.4 —
   a ~13 dB clean→ghost gap, the instrument isolating WHERE our flow+warp fails.
3. **Worst-frame surfacing** — the N highest-`dbl_edg_m` frames, listed worst-first (replaces the manual
   slow-mo hunt for the bad frame).
4. **`flowdsc` (flow-discrepancy — the FloLPIPS-analogue)** — Mode A runs the flow twice
   (`flow(prev→next)` + `flow(prev→truth)`), reads back the pipeline's MV field, reports
   `mean |0.5·mv(prev→next) − mv(prev→truth)|` px = **flow-estimate error**, separating it from the warp/blend
   error PSNR captures. **Finding → fix (verified):** raw `flowdsc` surfaced that the block-match is **ambiguous
   on diagonal motion over periodic texture** (pan_diag read ~10 px flow-error despite a clean PSNR). Root cause:
   periodic texture yields a wrong match with a **LOW** SAD (it matches a shifted period) — so a SAD-confidence
   gate alone did **not** fix it. The real fix was the **test source**: `prep_zoo_sequence.py` now renders a
   deterministic *non-repeating* value-noise scene → unique matches → well-posed flow → pan_diag drops to 0.4 px
   and flowdsc rises monotonically with motion difficulty (pan 0.17 → orbit 1.32). The SAD-gate (BOTH matches
   `sad_best < 32`) is kept as a complementary filter for genuinely-failed high-SAD tiles; `conf_tiles %` flags
   low-confidence frames. Honest residual limit: periodic *real* content (fences/grates) can still confound it.

**Honest walls (the "muros que no cruzamos"):** (a) **LSFG asymmetry** — full-reference held-out is possible on
OURS (digital `--outdump`/`--qdump`) but **not** on LSFG (digitally uncapturable: WGC delivers ~33fps DWM
composite, not its 240fps overlay — verified). The fair shared terrain is the **no-reference axis** (E_warp,
`dbl_edg_m` on camera-captured LSFG vs our captured frames). (b) Warp-error motion-decoupling is **not provably**
independent of motion magnitude (3 sub-claims refuted) → the no-reference metrics are flags, the FR held-out is
the arbiter. (c) **Genuine SOTA gap:** no published automated detector for FG-tool-specific failure modes
(HUD/UI ghosting, disocclusion) — reviewers of DLSS-FG/FSR3/LSFG still eyeball frame-by-frame; our holonic
per-object `obj_iou`/`nonrigid` is a modest contribution to that open niche, heuristic. (d) Metric SROCC
magnitudes are dataset-dependent — use the *ordering* (PSNR/SSIM ≪ LPIPS/DISTS ≪ bespoke), calibrate against
BVI-VFI before trusting an absolute number.

### 11.8 Live-FG crescent/gravity dive (2026-06-14) — operator-eye localization

Goal: kill the disocclusion crescent/estela on the moving ball (zoo@1280). The camera bridge (give the supervisor
digital eyes past the WDA-excluded present) **FAILED** — Samsung blocks the shell-UID camera (scrcpy camera = 0
frames, both encoders) and scrcpy screen-mirror = 0 KB; the operator's EYE remains the instrument (aided by
`--cap-fps`). Levers built, ALL default-OFF, **UNCOMMITTED** (in `build-ra-msvc2`):

- **STAGE-88 `--obj-fill-rim`** — coherent slot-MV interior infill (rigidity-gated). ~8% ghost reduction on OTHER
  artifacts; does NOT fix the crescent.
- **STAGE-89 `--disoccl-commit`** — occlusion-aware one-sided commit (`asym = dis_fwd−dis_bwd` → clean side) at the
  object + bg `w_sum<1e-4` fallbacks. MISSED (measured 0, eye confirms).
- **STAGE-90** (broad bg commit, same flag) — the asym commit on the FINAL bg base blend (`wap_warp.comp` ~:1363).
  ALSO MISSED (crescent persists with matte).
- **`--cap-fps N`** — throttle WGC capture to N fps (eyeball aid; low N → more interpolated frames/pair).

**Localize (operator eye, definitive):** `--no-obj-crescent`/`--no-crescent` → crescent UNCHANGED (not the
STAGE-62/72 weighting). `--no-matte` → crescent GONE but **gravity** appears → **the crescent IS the matte/bg
compositing path; the pure warp underneath does LSFG-style gravity deformation.** Best config (his eye):
`--obj-fill-rim --no-matte --cap-fps 15` = **LSFG-equivalent** (no crescent; gravity remains; `--disoccl-commit`
inert under `--no-matte`). A "jump/acceleration" (non-linear interpolated motion) is prominent at low cap-fps.

**Honest re-diagnosis (the load-bearing correction):** the **remaining root is GRAVITY** — the OBJECT warp
*deforming* at the disocclusion edge (the jump/accel is its face), not the crescent (which `--no-matte` already
removes). The operator's deep-truth **background plate** addresses the crescent/BACKGROUND, **NOT** the gravity —
so the plate is *not* the gravity fix. Gravity fix candidates: (a) TRUE rigid forward-splat (Lever-C scatter —
never built; the C agent shipped only a gather-approximation); (b) better rigid object flow; (c) the jump/accel as
a phase/pacing issue (`t_use` ladder) — likely cheaper to probe first. Full session state: memory
`render-assistant-arc` (CRESCENT/GRAVITY DEEP-DIVE block).

---

## 12. 2026-06-14 session — timing axis closed, affinity refuted, the cost-ordered coin ladder, coin-0 refuted

*Status: **measured** (first-hand live HSR@120 + bench). Raw per-run logs in `frames/timing_probe/*.out.txt`
(gitignored scratch); THIS section is the durable record. Failed attempts are documented as first-class — each
maps a wall (the FDP honesty discipline).*

Binding operator reframe: the goal is **budget-elasticity across the whole spectrum up to 1080p@500Hz** (real
source fps<500), 240Hz as the MEASURABLE point — NOT "beat 120fps." The temporal budget (per-pair cost >
source-interval → cons<arr → freezes) is the north star.

### 12.1 Timing axis — RESOLVED; the jump is NOT pacing
Corrected `--phaselog` method (the prior probe ran <20s so it never cleared `kPlogWarmupMs=20000` → 0 ladder
lines; the channel was always stdout `[ra] phase`). 3 configs >30s on the Zoo:
- `--no-phasefix`: uniq ~209/251, ladder starts ~0.42 + OVER-HOLDS 1.00 ×3-6 (the textbook defect). **phasefix
  (DEFAULT ON) fixes it → uniq 251/251.**
- `--cap-fps 15`: the CLEANEST ladder (n=15-17 uniform [0..1]) — cap-fps REGULARIZES the jittery PowerShell Zoo
  source → **REFUTES "cap-fps is a pacing artifact"** (its only cost is +latency, 42-55 vs 33ms).
- slip ~0.31/max~12ms CONSTANT across all three → background noise → **REFUTES "slip = the jump's prime candidate."**
- **VERDICT: the `t_use` pacing is SOUND. The jump/acceleration lives in the WARP/gravity (the visual axis), NOT
  timing — the cheap/digital axis is exhausted with a clean negative.**

### 12.2 Affinity — REFUTED (wall: the field CPU cost is intrinsic, not placement)
Topology (`build-bench/probe_topology.exe`): CCD0 = V-Cache = mask `0xFFFF`; CCD1 = freq = `0xFFFF0000`. Decisive
run (correct decimal masks, HSR@120):

| config | cons | gme_fit | pair |
|---|---|---|---|
| control (no pin) | 65 | 2.85 ms | 13.82 ms |
| CCD1-isolated (`0xFFFF0000`) | 87 | 2.72 ms | 9.79 ms |
| CCD0-4core (`0xFF`) | 83 | 2.91 ms | 9.96 ms |

**gme_fit ~2.7-2.9 ms STABLE across ALL affinity configs → CPU affinity does NOT fix the field cost.** The field
gme_fit is **2.77× the bench's solo ~1.0 ms on the SAME 240×135 grid** = intrinsic contention from
render_assistant's OWN concurrent threads (240 Hz present, aggregator, iGPU-convert) — NOT placement (CCD1
isolation did not move it), NOT DRAM bandwidth (~370 MB/s, tiny), NOT V-Cache capacity (600 KB working set fits a
normal L3). render_assistant does not pin threads; Phyriad HAS the V-Cache pin (`framework/topology`
`hw::set_thread_affinity` + `VCacheDetector` + Ayama FR-3) but it does not help here. **Wall: the per-pair CPU cost
under live concurrency is irreducible by core placement.**

### 12.3 The cost-ordered coin ladder (the decision model)
Operator rule: weight each lever (savings vs price), spend the **cheapest coin first**, exhaust it, escalate.

| coin | lever | savings | pays with |
|---|---|---|---|
| 0 | SIMD (done 1.47×) + parallelize | ~1-2 ms | nothing (byte-near-identity) |
| 1-2 | shed scene-mem / object_repair | ~1.4 / ~2.4 ms | quality (refinement) |
| 3 | flow-res ½/¼ grid | ~4× / ~16× the WHOLE pair (O(pixels)) | quality (coarser MVs) |
| 5 | decimate source | breaks pair-per-real | **latency** (sacred for gaming) |

Pay-with-quality (shed / flow-res) before pay-with-latency (decimate). The general primitive = a budget-aware
DRS+shed **BudgetGovernor** (§10.3) — greenlit as an FR.

### 12.4 STAGE-87b shed — deployed (default-OFF); helps but insufficient alone
The deficit-tier made PROACTIVE + DWELLED (engage >0.92×budget, dwell ≥90 pairs, release <0.55×) — it was reactive
(1.10×) + eager (0.65×) → it flapped. Field A/B HSR@120:

| | baseline | shed v2 |
|---|---|---|
| cons | 64.9 | 94.4 |
| lat | 43.5 ms | 33 ms |
| lap-escapes (the 64 ms freezes) | 17 | 6 |
| tier:4 | 0/14 windows | 13/14 (sticks) |

**Helps ~65% (operator eye: the throttle-feel is GONE) but INSUFFICIENT alone** — cons 94 < arr 110, pair still ≈
budget. The shed floor is `gme(2.7) + flow(3.7) ≈ 8 ms` (object_repair + scene-memory shed; gme is the irreducible
model) — it cannot go lower without cutting gme or flow → coin-3.

### 12.5 ⛔ COIN-0 (parallelize gme via the holons pillar) — REFUTED in the field, nothing shipped
Wall: per-pair fork-join is hostile to a sub-ms reduce at 120 Hz with a competing 240 Hz present thread.
- **Bench (solo, verified first-hand):** `gme_fit_affine_par` (holons DivisibleUnit + PoolRuntime) = **12.3×
  @240×135** (6.5× @½, 3.2× @¼), **BIT-EXACT model + dis-mask**, ~1.6 µs/submit — the survey's dispatch-overhead
  worry refuted SOLO.
- **Field A/B (HSR@120, `--par-gme`, default-OFF):** gme_fit **3.05→21.42 ms (7× WORSE)**, pair 10.09→29.41 ms,
  cons 81→35, dis **9.8%→86.9% (model CORRUPTED)**.
- **Root:** the holons/pool dispatch fits COARSE work, not a ~1 ms reduce at 120 Hz — SPIN contends with the
  present (the very contention), PARK (the contention fix) pays wakeup latency ×4/pair → 21 ms; neither works. Plus
  a correctness bug the SOLO bench could not exercise. **The default-OFF gate caught it — held OFF until the field
  test EXACTLY because the bench is solo and cannot predict contention. The measure-in-field discipline worked.**
  `--par-gme` was reverted (gone from the tree). Only SIMD (in-core, done) or a persistent pipeline (STAGE-85,
  which broke P) fit this regime — coin-0's multi-core form is **exhausted**.

### 12.6 Coin-3 flow-resolution DRS — the path (investigated; build next)
Run the flow + the whole F-pair CPU tail on a HALF-/QUARTER-resolution MV grid → the per-pair cost shrinks ~4× per
halving (O(pixels)). Read-only investigation, first-hand: **MIXED invasiveness, the cheap side is SMALL** — the
warp auto-upsamples a smaller grid (`textureSize`+normalized-UV at `wap_warp.comp:599`, NO shader change) and the
entire CPU tail reads the grid as a variable (`ofp.motion_width()`, auto-scales); the work = (1) feed the flow a
half-res input + (2) **collapse the 4 independent `(WW+7)/8` grid recomputations** (`main.cpp:2415/2718/4383/4771`)
to one source of truth (else silent corruption in the copy-out at `:4383`). Flag `--flow-scale N` (default 1 =
byte-identical), size M.
**Correction to §10.2 (load-bearing):** §10.2 called half-res "1440p-only, useless at 1080p (CPU is the floor)."
But the pair is serial `flow + cpu`, and the CPU tail is ALSO ∝ grid → half-res cuts BOTH legs: flow 3.7→~1, cpu
6.5→~1.6 → **pair ~10→~2.6 ms → fits 120 AND 240.** Flow-res IS the 1080p@120 fix (and scales the spectrum), not
merely a 1440p lever; the §10.2 arithmetic under-weighted the CPU-tail shrink. The cost is **quality** (coarser
16×16 MVs → small objects coarsen / drop from the holon layer) — measured by the §11 test-field (`dbl_edg_m` /
`flowdsc`, flow-scale 1 vs 2 vs 4) + the operator's eye. Elastic per-frame flow-scale (vs static) = L (needs
realloc-free pyramid-level selection or N pre-allocated pipelines, wired into the BudgetGovernor).
