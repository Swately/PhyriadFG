# PHYRIADFG_ARCHITECTURE_GAP_AUDIT — Where is PhyriadFG at the physics floor, and where is there real headroom?

> **Diátaxis type:** Explanation + audit (a verdict + an action list, not a how-to). **Status:** `measured`
> — SOTA web-research + first-hand code audit across 14 angles, adversarially verified (workflow
> `wf_5d7aac34`, 28 agents — 14 auditor + 14 verifier — ~2.3M subagent tokens). **Verifiable-reference
> mandate (FDP):** the SOTA citations below are from the audit agents' web research and carry the
> verification level the agents assigned; the `file:line` anchors are from the first-hand code audit of
> `apps/render_assistant/`; the adversarial *verify* pass is folded in throughout — where it killed or
> demoted a gap, the demotion is stated; where audit and verify disagree, both are shown and marked open.
> **What this does NOT claim:** it does not re-measure the rig (no new benchmark run was performed for this
> document); the latency numbers it cites are the project's own prior measurements, themselves flagged below
> as photon-unvalidated (§3.11). It is an *explanation of where the headroom is*, not a measurement campaign.
> **Companion:** [`HOLONIC_CONFORMANCE_AUDIT.md`](HOLONIC_CONFORMANCE_AUDIT.md) — the sibling audit that
> asks whether PhyriadFG *embodies* the holonic thesis (verdict: description-only); §5 here ties to it.

---

## §0 — Scope and method

**The operator's question.** *"We work más o menos. How do we get to muy muy bien — is it just the
theoretical limits?"* This audit answers it for the PhyriadFG external-capture multi-GPU frame generator
(product = Ayama; A = RTX 4090 capture+warp+present, B = GTX 1080 Ti optical flow, G = AMD iGPU
convert+pack). It separates **what is irreducible physics** from **what is genuinely addressable**, for an
external, anti-cheat-safe tool on this rig.

**What was audited — 14 angles.** capture · cross-device · optical-flow · interpolation · extrapolation ·
disocclusion · cadence-present · latency-arch · gpu-saturation · concurrency-safety · measurement ·
color-format-hdr · router-capability · theoretical-limits.

**The classification.** Every gap the audit found is tagged with one **nature**:

- **`physics-floor`** — irreducible for the external/anti-cheat-safe class on this hardware. Not a gap to
  close; closing it would require leaving the class (in-engine injection, different silicon, or a paradigm
  change). A subset of these are *frontier* floors — places where **the SOTA itself is stuck** (disocclusion
  true-holes, VFI ill-posedness), distinct from *class* floors that only bind us (no-Reflex, no-VRR).
- **`addressable`** — a known, anti-cheat-safe SOTA technique exists that PhyriadFG does not yet use; it has an
  honest est-impact **and a cost**.
- **`design-debt`** — a capability that exists or is cheap, but is mis-wired, default-off, or unvalidated.
- **`known-deferred`** — a real future capability, correctly parked (scoped, not careless).

**The anti-cheat-safe constraint.** The product's reason to exist is that it injects nothing (captures the
game window like OBS). Every "addressable" gap in the action table (§2) is `anti_cheat_safe = true`. The few
gaps marked `anti_cheat_safe = false` (present-hook capture, fresh-input reads, in-engine depth/Reflex) are
listed only to **name the class floor**, never as things to do.

**The adversarial verification.** Each auditor's findings were checked by an independent verifier that hunted
*missed gaps*, *overstated gaps*, and *SOTA citation errors*. This document **folds the verifier in**: an
overstated gap is demoted with a note; a fabricated or mis-attributed citation is corrected; where the two
genuinely disagree, both are presented and the item is marked **open**. This is the operator's
anti-sycophancy bar in document form — the audit does not get to keep an inflated win.

---

## §1 — THE HEADLINE: at the floor *for interpolation*, not at the floor *for frame generation*

**The operator's intuition is correct, with one sharp correction.** PhyriadFG is genuinely near a theoretical
limit — **but it is the limit for the INTERPOLATION paradigm it is in, not the limit for what an external,
anti-cheat-safe, multi-GPU frame generator could feel like.** The honest split:

**What IS irreducible physics (do not chase these):**

1. **The interpolation hold (~12–17 ms).** To place a frame at phase *t* between reals N and N+1, the pair
   must already contain N+1 — so the display is held back by the time to capture+publish N+1. Every
   interpolating FG (DLSS-G, FSR3, AFMF, LSFG, PhyriadFG) pays this; it is definitional (GFFE, ExWarp, Blur
   Busters). It is why the D-anchor can never reach zero without freezing.
2. **The external-capture acquisition floor (~one DWM compose, ~0.8–6 ms).** WGC shares a DWM-composited
   texture, so the captured frame is at minimum one compose behind the game's real present. The only thing
   below it is present-hook injection — an anti-cheat ban. LSFG eats the identical cost. **This is the
   strongest dimension of the whole stack: PhyriadFG is SOTA-correct here.**
3. **The cross-device freshage tax (~3–8 ms structural).** PhyriadFG's pair round-trips 4090 → iGPU → 1080 Ti
   → 4090. Every hop is a real serial latency term a single-GPU pipeline never pays — **the dark side of the
   holonic thesis** (see §5). LSFG's dual-GPU mode pays *one* such hop (~3–5 ms); PhyriadFG's *second* hop is
   the price of using three devices instead of two.
4. **The no-Reflex / no-VRR class floor (~7–15 ms permanently out of reach).** An external DComp overlay
   cannot drive the monitor's VRR (the game's swapchain owns it), cannot hook Reflex (no engine), and cannot
   read fresh input. PhyriadFG can match LSFG (same constraint) but structurally **cannot** match in-engine
   DLSS-3+Reflex on absolute click-to-photon.
5. **GPU work under 99% saturation.** Consumer GPU priority does **not** preempt a saturating dispatch (WDDM
   dispatch-boundary floor; D3D12 `PRIORITY_HIGH` a no-op on AMD/NVIDIA; Vulkan priority a non-binding hint).
   The warp's ~8 ms on the saturated 4090 is silicon. The currency/priority idea is a confirmed dead-end —
   do not re-propose it.

**Where there IS genuinely addressable headroom (the real answer to "muy muy bien"):**

- **The pipeline-depth excess vs LSFG.** PhyriadFG measures ~73 ms added in BF6 combat; LSFG, at the *same*
  external/dual-GPU architecture, measures ~5–11 ms *added*. The hold physics explains only ~12 ms of the
  gap. The rest is PhyriadFG-specific pipeline depth: the **~11 ms B-side ingest serialized into F's build**,
  the ring-guard spin (~7 ms), lap-escape-inflated D, and the throughput-config async buffering (~28 ms).
  **Honest correction (verify):** LSFG's small *added* number is mostly its **capped-low-base regime**
  (16.7 ms frames, big headroom), which the no-cap dogma blocks; LSFG's *absolute* total at 60→240 is
  ~66 ms, **not** a 30–45 ms system. So the realistically quality-free reclaim is **~11–18 ms** (the B-side
  build-gap + some ring-guard spin), not the ~25–40 ms the audit's headline first claimed.
- **The interpolation→extrapolation/reprojection pivot.** The entire SOTA says the way *below* the
  interpolation floor is to stop interpolating. PhyriadFG already has the seeds in-tree (`--camera-twarp`
  MV-reprojection, `--asw` bounded forward extrapolation) but they are off-path/opt-in. This is the
  **unexplored headroom** — but it is a research bet, not a tuning win, and it **inherits the disocclusion
  frontier where PhyriadFG already loses to LSFG.**
- **Quality at the disocclusion boundary** (the "gravity"), where the addressable wins are a non-neural
  forward-splat z-ordering, a history-frame background fetch, and a census (illumination-invariant) flow
  cost — all classical, all anti-cheat-safe, none needing the tensor hardware the 1080 Ti lacks.

**The decisive synthesis for the operator:** PhyriadFG can match LSFG's "feels like it isn't there" for
**fluidity and cadence** (it is already close — `--phase-norm` + sync-clock + the even ladder), and the
multi-GPU holonic structure is a **genuine structural advantage** for total compute / assert-without-cap that
single-GPU LSFG cannot have. For **input-lag** in the same regime, the honest ceiling is: close the ~11–18 ms
pipeline-depth excess, then accept the residual as hold + cross-device + no-Reflex floor. To go *below* that
needs the reprojection branch — research, not tuning.

---

## §2 — RANKED ADDRESSABLE GAPS (the "what to try next" table)

Across all 14 dimensions, the gaps tagged `addressable` or `design-debt` **and** `anti_cheat_safe`, ranked by
value (est-impact × tractability). Gaps the verifier flagged `overstated` are demoted with a note, not
dropped. **Read this as the operator's action list.** Every "Est. impact" carries its cost in §3's
per-dimension detail.

| # | Gap | Dimension | Nature | Est. impact (honest) | SOTA technique (cited) |
|---|---|---|---|---|---|
| 1 | **VK_ERROR_DEVICE_LOST detection + graceful recovery** (zero handling exists today; every submit/wait result discarded) | concurrency | addressable (**critical**) | Robustness: turns a hard-crash/spin on every driver TDR — exactly in the 99%/dual-GPU regime — into a recoverable hiccup. **Demoted scope (verify):** the SOTA is *detect + graceful exit*, **not** the days-long in-process device recreate the audit first prescribed; rivals do **not** demonstrably recover either | VK_EXT_device_fault + check every `VkResult`; on lost, stop/flush/teardown/exit (Khronos crash-handling) |
| 2 | **The ~11 ms B-side ingest serialized into F's per-pair build** (build 20 ms vs compute 8.7 ms) | cross-device / latency | design-debt | **~11 ms off freshage** if fully overlapped, plus a double-win (F catches source → pickup backlog shrinks). The single largest quality-free latency lever the project's own card-deck names. **Verify caveat:** part of the 11 ms is the *recipe export* (downstream), not only ingest — decompose before claiming the full 11 ms | Pre-stage B-copy+unpack off the F iteration onto B.q2 (prima.cpp PRP overlap, the way `--upload-xfer` decoupled the A-side) |
| 3 | **Pace the P-thread to the real DWM composition clock**, not a free-running internal `refresh_hz` timer | cadence | addressable | Removes the timer-vs-compose **beat judder** that makes the overlay "not feel native". Low cost, latency-neutral. Already greenlit in the project's own cadence dossier. **Verify:** *predict* the next compose QPC from `DwmGetCompositionTimingInfo` and spin — do **not** use `DwmFlush` (it blocks, would add latency) | `DwmGetCompositionTimingInfo` qpcVBlank/qpcRefreshPeriod + moving-avg (FSR3/DLSS-G mechanism) |
| 4 | **One photon-vs-internal cross-validation pass** (the two latency tools have never been reconciled to ground truth) | measurement | addressable | One afternoon turns the whole ~73 ms dossier from "internally measured" to "photon-validated" and reveals the size of the untraced present tail. **Verify correction:** the present surface **IS** a real DXGI flip swapchain (`CreateSwapChainForComposition` + `FLIP_SEQUENTIAL`), so `GetFrameStatistics`/`SyncQPCTime` is reachable **today** — the audit's "no swapchain" premise was false, making this *more* addressable | LDAT/OSLTT photodiode on `latency_probe.html`; DXGI `GetFrameStatistics` |
| 5 | **Flip `--async-present` default-ON** after its Tier-2 crash validation (the SOTA drop-on-slip present is built but off; the default still blocks the present thread on the warp fence every tick, `main.cpp:6878`) | gpu-saturation / latency | design-debt | Removes the synchronous present block from the steady path (the addressable portion of the present-side ~32 ms). Cost: ~1 frame of depth + the crash-class surface (REAL_FAST_PATH_RISK_REGISTER). Does **not** lower freshage | DLSS-G SL pacer / FSR3 async present — decouple present from the generation fence |
| 6 | **Non-neural forward-splat z-ordering** of same-target collisions over the existing block-MV field | interpolation / disocclusion | addressable | Directly attacks the disocclusion "gravity" — PhyriadFG's named loss vs LSFG. One scatter pass on the 4090; bounded. **Verify caveat:** SoftSplat's Z is a *learned* metric in an end-to-end net — the non-neural hand-tuned-alpha port is **`designed, unvalidated`**, must not borrow the paper's quality numbers | SoftSplat importance-Z (Niklaus & Liu, CVPR 2020) — backward warp structurally cannot z-buffer |
| 7 | **Census / gradient illumination-invariant flow cost** instead of raw RGB SAD | optical-flow | addressable | Hardens flow exactly in combat (muzzle flash/exposure breaks SAD). GPU-cheap (bit-pack + XOR+popcount), classical, no tensor cores. A strong combat-regime lever | Census transform / gradient constancy (TV-L1 structure-texture) |
| 8 | **History-frame background hole-fill** (the 4–32-slot real ring exists but the warp binds only the pair) | disocclusion | addressable | Converts a *subset* of true-holes into real-content fetches — the one move that can beat the 2-frame bound. **Demoted (verify):** the win is real **only** for static/camera-moving background — it *inherits the gme global-affine floor* (gap #4 in disocclusion) for moving backgrounds; the ring holds frames but the *tracking/validation* machinery is the hard part and does not exist; deepening the in-order drain measured +15 ms | Classic background-memory MEMC (the depth-free analogue; GFFE's idea but GFFE itself needs depth — see correction §3.6) |
| 9 | **Confidence / direction-change gating of the extrapolation horizon** (`asw_max`/`camera_twarp amt` are fixed scalars) | extrapolation | addressable | Kills the overshoot artifact on flicks/direction-changes (the FPS-combat failure mode), letting `amt` rise safely on steady motion. Highest value/cost ratio in its dimension; inputs (gme IRLS residual + `|mv−mv_prev|`) already computed | ExWarp/PatchEX per-region warp-vs-extrapolate gating |
| 10 | **A 10bpc (R10G10B10A2) present + HDR path** (bridge, VK image, DComp swapchain all hard-locked BGRA8 SDR) | color/HDR | addressable (**critical** for HDR rigs) | Removes the SDR-in-HDR mismatch — parity with LSFG's headline HDR feature. 10bpc is 4 bytes/px (no bandwidth penalty). **Verify caveat:** "near-zero cost" is half-true — it still needs a PQ encode + `SetColorSpace1` + BT.2100 metadata; the *format* is cheap, the *colorimetry* is not | DLSS-G/XeSS-FG HDR10 R10G10B10A2/PQ; LSFG HDR + auto tone-map |
| 11 | **Seqlock the multi-field pair publish** (8 fields behind one `f_seq.fetch_add` over a wrapping gen-ring → torn-read window under a lap) | concurrency | design-debt | Eliminates the rare cross-generation torn record (a one-frame wrong-span warp). ~2 atomic loads/publish — negligible at ≤240/s. Verifier confirmed this is sound and correctly classified *debt, not physics* | rigtorp/Seqlock versioned snapshot, or a wider `kGenRing` with the invariant `static_assert`ed |
| 12 | **Wire the existing device-characterization harness into startup** (the router pillar is complete but has **zero** in-product callers; roles assigned by device *type*, the MEASURED block is permanently zero) | router | design-debt | Robustness/honesty: converts the assumed `offload=false` / iGPU-always-converts / B=first-discrete hard-codes into per-rig MEASURED facts. One-time startup probe, zero hot-path cost. **Verify:** B-selection needs an **fp16** probe specifically (FG is fp16-bound; "first discrete" can pick a raster-fast/fp16-slow card) | StarPU history tables / prima.cpp measured coefficients — the harness (`measure_transfer_bw`) already exists |
| 13 | **An FG-quality scalar** (disocclusion is measured only by operator eye; every quality change was tuned without a number to A/B) | measurement | addressable | Turns "operator eye" into reproducible A/B and enables apples-to-apples vs LSFG. **Verify correction:** the proposed metric `arXiv:2510.01361` is **full-reference (needs GT)**, usable only in a held-out-frame harness — *not* a live no-GT proxy; the only live no-GT option is the triplet net `arXiv:2312.15659` | Held-out-frame FloLPIPS/PSNR_DIV self-validation, or no-ref triplet net |
| 14 | **A pacing/animation-error metric** (the whole cadence-tuning history was eyeballed; PhyriadFG knows its own intended phase ladder and can log it self-contained) | measurement | addressable | A scalar for the temporal "feel" the operator chases — internally computable, no external hardware. Verifier raised this as a missed gap arguably above #13 | GamersNexus `MsAnimationError = Δanimation − ΔdisplayChange` |
| 15 | **Consume the per-tile AFFINE model the matcher already computes** (built, but the warp does not read it; `mv_affine` default-off) | optical-flow | design-debt | Attacks the zoom/rotation crossfade directly (the fit's own held-out numbers quantify it). Pure wiring, no research | Layered/parametric motion — wire the existing companion field into `wap_warp.comp`'s sample base |
| 16 | **Per-region (object-vs-camera) responsiveness lead** (camera-twarp leads *everything* by the global gme model; a fast object on a static camera gets ~zero lead) | extrapolation | addressable | Responsive object motion tracks too, not just camera pan. Reuses the existing matte gate. **Verify correction:** the title "translation-only" is wrong — 114c already leads by the *full 6-param affine*; the residual is depth-correct homography, not translation | PatchEX FG/bg stencil segmentation + per-region warp |
| 17 | **WGC `DirtyRegionMode` (24H2) duplicate suppression** instead of the blunt 8 ms `MinUpdateInterval` throttle | capture | addressable | Efficiency + cadence-cleanliness; lets the source cadence run fresher. **Upgraded by verify:** this is not a mere "efficiency nicety" — it is the **only in-API signal of a genuine game-frame change** (WGC fires on *compose* ticks, not game presents), the principled replacement for the heuristic throttle + downstream `--fdrop` | `GraphicsCaptureSession.DirtyRegionMode` + `DirtyRegions` metadata (24H2; needs a version guard) |
| 18 | **WGC teardown: revoke `FrameArrived` + counted quiesce** instead of `session.Close()` + `Sleep(100)` | concurrency | addressable | Closes a teardown use-after-free on the WGC pool thread (rare, exit-only; worse with `--copy-fence`). Cheap | Revoke the delegate token before Close; gate on an in-flight refcount (MS Learn / robmikh) |
| 19 | **Highlight-rolloff tone-map** instead of `min(c,1)` hard clip at HDR ingest | color/HDR | addressable | Recovers blown specular/sky on HDR sources even if present stays SDR. A few shader lines. **Verify:** pair it with dither in the *same* edit (rolloff into 8-bit without dither bands visibly) | Filmic/Reinhard/ACES rolloff (LSFG auto tone-map) |
| 20 | **Linear-light compositing** for the matte/crossfade/disocclusion blends (warp math runs in gamma space) | color/HDR | design-debt | Removes the slight darkening/hue-shift in crossfades — *exactly* at the boundary where PhyriadFG loses. **Verify:** the error is *largest* on the high-contrast moving edges camera-twarp targets — the "subtle" label undersells it. Touches the hot path → measure-gated | Decode sRGB→linear before blend, re-encode after |

**Lower-tier addressable items** (real, smaller, in §3): in-matcher fwd-bwd consistency as a shared per-tile
confidence channel (demoted by verify — the occlusion *reasoning* already exists at warp time, this is a
confidence-sharing refactor, not a new capability); 2D cost-surface subpixel; motion-adaptive ingest backlog;
asymmetric attack/decay freshage estimator; per-stage latency percentiles (the EMA hides the felt tail);
3-frame temporal flow prior; iGPU occlusion field beyond the luma-Sobel "P0 stub"; waitable-swapchain shallow
queue (demoted by verify — a *composition* swapchain may not expose the waitable object cleanly; closer to
known-deferred); LSFG-3.1 Adaptive-Frame-Generation-style fractional output multiplier (the verifier insists
this is addressable and **mis-filed as physics** in gpu-saturation — see §3.9).

---

## §3 — Per-dimension audit (all 14)

Each subsection: **(a)** SOTA in 1–3 sentences; **(b)** what PhyriadFG does (with `file:line`); **(c)** the
gaps, classified; **(d)** the verifier's corrections, folded in; **(e)** the honest physics-floor note.

### 3.1 Capture (the strongest dimension — PhyriadFG is SOTA-correct here)

**(a) SOTA.** Anti-cheat-safe external acquisition has converged on exactly PhyriadFG's choice: WGC. LSFG (the
rival) independently landed on WGC for 24H2+ ("less latency than DXGI + 6–9% less overhead"; 24H2 MPO broke
DD for same-monitor FG). WGC's structural cost is one DWM compose. Under saturation every capture API starves
— priority does not preempt on consumer GPUs.

**(b) PhyriadFG.** WGC live (`main.cpp:3479-3489`) with DD fallback; free-threaded `FrameArrived` does one
`CopyResource` into a staging ring (`:4552`); 6-slot pool to absorb backlog (`:3502`); 8 ms `MinUpdateInterval`
anti-duplicate-flood floor (`:3599`); C-thread `Map(DO_NOT_WAIT)` with older-slot fallback + `--copy-fence`
event-driven pickup (`:4500-4529`, STAGE-113, commit `17d1326`, measured latency-neutral, −40% busy-poll).

**(c) Gaps.** `addressable`: separate copy-device for the staging `CopyResource` (~5.6 ms — but **demoted**,
see below); WGC `DirtyRegionMode` duplicate suppression (§2 #17); default-flip the built copy-fence.
`design-debt`: the busy-poll default; the legacy DD fallback. `physics-floor`: the ~0.8–6 ms compose+copy
INVISIBLE pre-`tcap` cost; the older-slot fallback under saturation; present-hooking (the only sub-compose
capture) is correctly **not** pursued (injection = ban).

**(d) Verify — folded in.** The audit's **strongest gap (separate D3D11 device for the copy) was OVERSTATED
and is demoted.** On D3D11 the immediate context is a single graphics-engine queue with **no** API-level copy
queue — a second D3D11 *device* still submits to the same saturated 4090 graphics engine, so "up to ~5.6 ms
if the copy fully overlaps" is not achievable without D3D12 copy-queue plumbing. The **CASO citation was
misapplied** (it is about dGPU→iGPU scan-out, not same-adapter readback) — struck. The STAGE-32b "Fix-2"
comment is a *contingency that never fired*, not corroboration of a win. **Missed gaps added:** WGC trades
frame-pacing cleanliness + VRR for latency/overhead (the other half of LSFG's own documented tradeoff —
PhyriadFG pays for it downstream in the whole cadence pipeline); the duplicate flood is **structural** (WGC has
no notion of a "new game frame", only a "new compose"), which **upgrades `DirtyRegionMode` from "low/efficiency"
to "the correct source-of-truth for source cadence"**; capture starvation is **graduated** (drops at ~50%
GPU, not a 99%-only cliff). **Correction:** the NVIDIA forum thread cited for "priority doesn't preempt" is
question-only — the claim is true but must be sourced to WDDM docs, not that thread.

**(e) Floor.** The one-DWM-compose acquisition lag is irreducible for the external class. **The operator's
belief that the capture side is near the theoretical limit is CORRECT** — and the capture API is *not* the
freshage bottleneck (the 41 ms freshage is dominated by downstream pickup, not acquisition). Do not
over-invest here.

### 3.2 Cross-device / multi-GPU

**(a) SOTA.** prima.cpp/Halda (ICLR 2026) pools heterogeneous GPUs by *measured* per-device coefficients and
**removes net-negative weak devices**. The binding wall is data movement; consumer P2P is driver-blocked
(Turing was the last GeForce with it; the 4090 has none). LSFG dual-GPU presents *from the secondary* → no
copy-back of generated frames.

**(b) PhyriadFG.** Inverts the LSFG topology: A captures **and** presents (forced by WGC + the WDA-excluded
DComp overlay), so the MV+SAD **recipe** travels back from B to A every pair — but it ships the **recipe, not
the cake** (~130 KB, not the 58 MB warped frame; `main.cpp:5882`). Every hop is an EMH host-bounce (no P2P).
`break_even_decide` returns `offload=false` always for the warp (AI≈2.5 vs the ~596 crossover, inlined
`main.cpp:50-52`); flow crosses to B on a *saturation* argument, not a break-even one. `--upload-xfer`
(STAGE-106) overlaps the A-side recipe upload onto the 4090's DMA engine A.qT.

**(c) Gaps.** `design-debt`: the **~11 ms B-side ingest** serialized into F's build (§2 #2 — the named
"prize"); `hRP_b → hRP_b_dev` full-frame DMA dodging x4 read-amplification; no measured iGPU-absent fallback.
`addressable`: the synchronous-blocking convert (~3.2 ms in-band, async-able). **Unmeasured (the operator
wanted nothing to escape):** the controlled all-on-4090-vs-split ablation under saturation — the split is
**asserted, not A/B-measured**, and prima.cpp's strongest lesson is that a weak device is sometimes worth
*removing*. `physics-floor`: the inverted-present recipe copy-back; the host-bounce (no P2P, cross-vendor).

**(d) Verify — folded in.** **Citation errors corrected:** `arXiv:2601.19910` was mis-titled/mis-attributed
("Meng/Lee/Wang, PCIe roofline") — its real title is about LLM-serving bottlenecks; the specific "39× faster"
figure is **uncited** and dropped. **The ~596 FLOP/byte crossover is PHYRIAD'S OWN measured constant**
(`framework/gpu/tests/test_break_even.cpp`), *not* a literature number — attributing it to the substrate's own
gate **strengthens** it (first-hand measured, not borrowed). The legacy EMH transfer numbers
(2.90 ms@1080p) are flagged **"measured per legacy memory, source not re-verified this pass."** **Missed
gaps:** the recipe-*export* (up to four image copy-outs + barriers on B's queue) is a co-contributor to the
"11 ms" the audit attributed wholly to *ingest* — decompose it; A-side `--upload-xfer` is **asserted, not
measured** net-positive under saturation; host-memory-controller contention (iGPU convert write + WGC host
write + B's read all share the one DDR5 controller) is a third front the per-device decomposition misses, so
the async-convert win is **less than the full 3.2 ms** under combat. **Overstated:** the gap-4 appeal to an
"LSFG auto-fall-back to single-GPU" regime is unsupported — LSFG has **no** load-based device fallback; the
gap rests on prima.cpp alone.

**(e) Floor.** No-P2P host-bounce (portable, correct); the inverted present topology (forced); the iGPU as a
*convert* engine only (it physically cannot compute frame-gen, ~135× too slow). The two genuine **debt** items
(B-side ingest overlap, async convert) are the prima.cpp "overlap I/O with compute" lesson not yet applied
per-pair.

### 3.3 Optical flow

**(a) SOTA.** Neural flow is the production frontier (DLSS-G OFA; LSFG a *lightweight neural net*). Classical
tier: DIS (300–600 Hz/core), TV-L1, SAD block-matching. The failure modes are universal: aperture problem,
occlusion, large motion; robust costs (census/gradient) handle illumination; block-matchers need explicit
subpixel.

**(b) PhyriadFG.** Classical SAD block-matcher on B (Pascal). 4-level pyramid, ±48 px coarse reach, RG16F MV
field at width/8, SAD confidence gate. Enhancements opt-in: parabolic subpixel (default-on), temporal MV
prior (dual-centre), candidate-selection, affine per-tile fit (**built, not consumed by the warp**), global
IRLS GME (Geman-McClure). Consumed bidirectionally in the warp (true backward field, STAGE-48).

**(c) Gaps.** `addressable`: **8×8 grid floors flow resolution** (finer grid / variational densification);
**photometric SAD → census** (§2 #7, the strongest addressable — combat lighting); in-matcher fwd-bwd
consistency as a shared confidence channel; 2D-vs-separable subpixel. `design-debt`: **affine model built but
not consumed** (§2 #15, pure wiring); fixed-alpha mv_smooth EMA. `physics-floor`: classical-not-neural is the
silicon (Pascal has no tensor cores); the coarse-to-fine reach-vs-small-object tension. `known-deferred`:
single global affine GME (no piecewise/multi-layer).

**(d) Verify — verdict SOUND.** **Missed:** 3-frame temporal flow is the standard occlusion/acceleration
lever and is **classical-realizable on Pascal** — the single temporal lever the audit underweights; a shared
per-tile confidence channel from the already-computed SAD field would tighten gme/median/EMA at near-zero
cost. **Corrections that *strengthen* the floor:** Pascal GP102 runs fp16 at **1/64** of fp32 (worse than the
audit's "fp16 at fp32 rate") — this **forecloses** the "quantize a small net" escape hatch the audit left
ajar. NeuFlow-v2 speedup mis-quoted ("5-110× vs SEA-RAFT" → ~5× vs SEA-RAFT specifically). **Overstated:**
the fwd-bwd-consistency gap — the warp *already* does symmetric round-trip + STAGE-89 commit-by-evidence, so
this is a confidence-sharing *refactor*, not a SOTA gap where PhyriadFG is behind; the 8×8 "floors resolution"
is mildly overstated — the field is sampled *bilinearly* (fractional), not a hard step grid. **Split the
"gravity" frontier:** the *fill* of one-frame-only content is SOTA-stuck (ill-posed); the *ordering* at
overlaps is **solved** classically by softmax-splatting z-ordering, which PhyriadFG already adopts in STAGE-89.

**(e) Floor.** Classical-because-Pascal is real. The disocclusion *fill* is a field-wide frontier. The
addressable value is census + finer grid + consuming the affine field + 3-frame prior — none needs new
research or breaks the class.

### 3.4 Interpolation / synthesis

**(a) SOTA.** Backward warp dominates for coverage (AceVFI 2026); the one case forward splatting *wins* is
overlapping motion with z-ordering (SoftSplat). Quality leaders use a **learned** spatially-varying fusion
mask (RIFE/FILM/PerVFI), never a static blend; PerVFI proves quasi-binary beats symmetric averaging
perceptually. No-GT selection is **provably ill-posed** — every scorer is a plausibility proxy.

**(b) PhyriadFG.** Backward warp (the SOTA-correct base, `wap_warp.comp:998-999`) + a **~40-flag hand-engineered
cascade** replacing the learned mask + a multicand **medoid** select (`--multicand`, default-off) with an
**iGPU-Sobel image-edge gate** + `d_pixel` (the L1 back-warp residual = softmax-splatting's Z) as the no-GT
proxy + ts-smooth garbage masking.

**(c) Gaps.** `addressable`: **non-neural forward-splat z-ordering** (§2 #6, the strongest — attacks
"gravity"); the medoid is the *weakest* selector family member (no confidence weight, flat RGB L1); narrow
candidate diversity (speed-axis only — feed the already-computed runner-up + gme MV). `design-debt`: the
40-flag cascade vs a learned/unified-energy mask (brittle, per-scene re-tuning treadmill); ts-smooth as a
symptom-masker. `physics-floor`: `d_pixel` is structurally **blind in the disocclusion band** (a confident
hallucination scores low residual there); block-resolution synthesis; no depth for z-order. `known-deferred`:
the iGPU edge-gate is **genuinely novel** (no shipping FG uses an image-edge trust gate) **but unvalidated** —
the project's strongest potential differentiator is unmeasured.

**(d) Verify — folded in (the largest single correction in the whole audit).** **LSFG — the exact rival
PhyriadFG loses to on disocclusion — is itself a NEURAL model running on Pascal/iGPU-class hardware.** This
reframes the loss: it is *not* non-neural-PhyriadFG vs non-neural-rival; it is non-neural-PhyriadFG vs
**lightweight-neural-LSFG**, the neural rival winning on the disocclusion axis. The audit's recurring
"neural fusion is out of class on Pascal" is therefore **OVERSTATED as a clean physics floor** — true for
the *heavyweight academic* nets, **false** as a general claim (LSFG and GFFE run real-time on this class). The
correct honest statement: *the heavyweight nets are out of class; a purpose-built lightweight net on this
class is demonstrated by the rivals and is unexplored by PhyriadFG — a deliberate project choice, not a
hardware impossibility.* **Missed in-class SOTA:** GFFE (G-buffer-free extrapolation) and LPVFI
(zero-overhead masks) are the directly-applicable recent art the audit (academic-VFI-heavy) omitted.
**Corrections:** the code is a true **medoid** (min total pairwise L1), **not** the Apple "nearest-to-median"
rule the audit equated it to; AceVFI lead author is **Kye**, not "Kim"; the FidelityFX reference is *also* a
hand-engineered non-learned selector that ships — so "a cascade can't generalize like a learned mask"
overstates it (the honest gap is **brittleness/flag-interaction debt**, not categorical inferiority); the
non-neural forward-splat port must be labelled **`designed, unvalidated`** (SoftSplat's Z is learned).

**(e) Floor & open item.** VFI ill-posedness, the disocclusion-band blind spot, the true-hole, and heavyweight
neural fusion are floors. **OPEN (audit vs verify):** *can a lightweight neural disocclusion/fusion net run on
B or on the 4090's ~50% spare?* The audit asserted it away as floor; the verifier holds it is the addressable
frontier the audit closed prematurely. **Marked open** — it is a research probe, not a settled verdict.

### 3.5 Extrapolation / reprojection / responsiveness

**(a) SOTA.** Extrapolation/reprojection is the latency-*positive* complement to interpolation: ASW 1.0/2.0,
Intel ExtraSS, ExWarp/PatchEX, and **Reflex 2 Frame Warp** (samples *fresh input* late, reprojects, ~75%
latency cut). Depth is the parallax enabler; fresh-input sampling is what makes it *feel* responsive.

**(b) PhyriadFG.** Three forward-projection mechanisms, all default-off, all anti-cheat-safe: **`--camera-twarp`**
(the proven responsiveness win — leads the warp sampling base by the *smooth gme affine*, not raw MV;
`wap_warp.comp:770-773`); `--asw` (deficit-only forward extrapolation); `vblend` (constant-acceleration
object prediction). Hard ceiling: no game input, no depth, no Reflex hook — its "camera velocity" is the gme
fit on already-captured frames, one-pipeline-stale.

**(c) Gaps.** `addressable`: **confidence/direction-change gating of the extrap horizon** (§2 #9, highest
value/cost); **per-object lead via the existing matte** (§2 #16); led-edge prev-frame/history fill instead of
clamp-stretch. `design-debt`: camera-twarp measured by *feel* only — no objective latency/overshoot telemetry.
`physics-floor`: no fresh-input prediction (the Reflex-2 gap — barred by the external constraint); no depth →
no parallax-correct reprojection.

**(d) Verify — folded in.** **Overstated:** gap "models TRANSLATION only" **contradicts the code** — 114c
already evaluates the *full 6-param affine* (`gme_model_mv`), so retitle to "models 2D affine, not a
depth-correct homography" (§2 #16). The fresh-input mitigation is overstated in feasibility — PureDark's
external Frame Warp runs *reverse-engineered Reflex-2 binaries*, it is **not** a clean OS-mouse-hook, so the
citation doesn't support the raw-hook approach (which is genuinely anti-cheat-grey). **Missed:** GFFE is the
closest analog and **strengthens the floor** — even the paper whose selling point is "free us from the
G-buffer" still needs engine depth+pose+MV; PhyriadFG's no-depth/no-pose/no-engine-MV position is *strictly
harder* than the hardest SOTA in this lineage. **Correction:** the audit said ASW "fails below half refresh"
— that is **ATW/ASW-1.0**, not ASW 2.0 (which fixed it); attribute correctly. Even a perfect camera-twarp is
a *perceptual* lever, **not** a click-to-photon lever (the overlay still presents D-anchor-late) — state this
so no Reflex-2-class latency number is ever claimed for it.

**(e) Floor.** Fresh-input and depth are barred by the external constraint (the product's reason to exist).
The remaining addressable wins are all *refinements* of the one anti-cheat-safe + latency-helpful lever that
already works — not a new class.

### 3.6 Disocclusion / hole-filling

**(a) SOTA.** Converges on: side-commit (sample one side, never average two misaligned), forward-splat with
an importance metric, depth-aware ordering, neural inpaint of wide holes, and **hierarchical background
collection from multiple history frames** (GFFE). The true-hole (visible in neither frame) is irreducible
without a learned generator.

**(b) PhyriadFG.** An exceptionally layered, **all-non-neural, strictly-two-frame** stack in `wap_warp.comp`:
bidir round-trip class (STAGE-48), divergence fill (STAGE-50), gme global-affine background MV, the
fluid-matte two-layer compositing (**default-off** — operator A/B found it *doubled the figure*), `--bg-snap`
(the live default — advects the iGPU contour, snaps background to the gme model to kill "gravity"),
`--band-xfade`, ts-smooth masking. The iGPU field is a self-described luma-Sobel **"P0 stub"**.

**(c) Gaps.** `addressable`: **softmax-splatting importance-Z for overlap ordering** (RGB-computable,
non-neural); **history-frame background fetch** (§2 #8 — the ring already holds the frames). `design-debt`:
the iGPU field beyond the stub; the **most-complete machinery (the matte) ships default-off** with its
figure-doubling never root-caused; ts-smooth IIR masking. `physics-floor`: the gme global-affine is **wrong
for a moving background behind a moving object** (near-irreducible without depth); block-resolution boundary;
the true-hole (no honest cheap fix — crossfade or hallucinate).

**(d) Verify — folded in (sharp demotions).** **The strongest gap (history hole-fill) is OVERSTATED on two
counts.** (1) **It depends on the gme-floor gap and the audit never says so:** to fetch real background from
N-2/N-3 you must warp it by *some* background model — and the only available one is the same global affine
that gap-4 itself calls a near-irreducible floor. So the win is real **only** for static/camera-moving
background (the regime `--bg-snap` *already* handles) and **degrades to the floor exactly where it would add
the most value.** (2) **GFFE is mis-characterized** in the audit as RGB/non-neural — first-hand, GFFE's
hierarchical background collection uses the **depth buffer + camera pose + MVs** and a neural shading net; it
is "G-buffer free" only relative to the *full* G-buffer. The transplantable idea is **classic background-memory
MEMC** (RGB-feasible), *not* GFFE's depth-driven machinery. The +15 ms cost of deepening the in-order drain
(measured) and the absent frame-to-frame tracking machinery make the "modest VRAM, bounded cost" estimate
optimistic. **PerVFI mis-attributed:** its quasi-binary mask is *self-learned* — it validates the *target
property*, it does not supply a hand-codable cure for the matte's figure-doubling (that is bespoke
hand-tuning). **Verified-correct:** all the first-hand code claims (igpu_field "P0 stub", matte default-off,
the ring holds packed reals), and gaps 7/8 (block-resolution, true-hole) are genuine floors.

**(e) Floor.** The true-hole, the block boundary, and the moving-background regime are floors (the last is
*frontier*-stuck, not just ours). PhyriadFG's stack is **already at or beyond the published non-neural FRUC
art** — the two real addressable wins are the importance-Z and the (scoped-down) history fetch.

### 3.7 Cadence / present path

**(a) SOTA.** The flat ladder comes from **owning the present timing on a high-priority pacer paced against
the fixed DWM clock** (FSR3/DLSS-G/LSFG) — *not* from VRR, which is architecturally blocked for an external
overlay. DLSS 4 moved pacing into a **hardware flip-metering** unit (5–10× lower variability) — out of reach
for any external tool.

**(b) PhyriadFG.** Presents through a **DComp overlay, not a VkSwapchain** (WDA-excluded, click-through).
**Paces to an internal `refresh_hz` timer, NOT the DWM clock** — grep confirms zero `DwmGetCompositionTimingInfo`/
waitable-swapchain/`SetMaximumFrameLatency`/`ALLOW_TEARING` in the app. Content cadence is the STAGE-90
sync-clock (NCO + 2nd-order PLL, default-on). `submit_at()` (IPresentationManager present-at-target) is
**designed but returns `Unavailable`**. Latency is measured to `Present()` *return*, before compose+scanout.

**(c) Gaps.** `addressable`: **pace to the real DWM clock** (§2 #3, the strongest — the one documented
difference vs every shipping FG, already greenlit in the project's own dossier); waitable-swapchain shallow
queue (**demoted**, below). `design-debt`: present-to-photon uninstrumented. `physics-floor`: VRR/G-Sync/Reflex
categorically unavailable to a DComp overlay (LSFG hits the identical wall, disables G-Sync); the +1
DWM-compose frame; the velocity-discontinuity boundary pulse (no engine MVs across the seam); CPU-spin pacing
vs hardware flip metering. `known-deferred`: Adaptive-Frame-Generation fractional pacing.

**(d) Verify — verdict SOUND.** **Corrections:** the FSR3 "requires V-Sync ON" claim is **stale** — fixed to
work V-Sync-OFF + VRR (the "owns the present path" point still stands, date it to launch). `IPresentationManager::Present`
takes **no arguments** — the timing setter is `SetTargetTime` (the `submit_at` design targets the right API
family). **Missed:** the present-to-photon instrument for a *DComp* surface is the IPresentationManager
present-statistics path — the *same* sub-pillar already stubbed `Unavailable` — so gap-2 (measurement) and the
submit_at design are **one unified build, not two levers**; the capture clock itself is DWM-compose-quantized
(a *second*, upstream beat the present-side fix alone does not address). **`DwmFlush` ≠ predictive spin** —
DwmFlush *blocks* and would add latency; the SOTA pattern is to *predict* the next compose QPC and spin to it
(§2 #3). **Demoted:** the waitable-swapchain gap is partially blocked — a *composition* swapchain does not
expose `GetFrameLatencyWaitableObject` the way an HWND flip swapchain does; closer to known-deferred.

**(e) Floor.** VRR/Reflex/hardware-flip-metering and the compose frame are the class floor (shared with LSFG).
The one un-built piece of the *known ceiling* is the DWM-clock alignment — that is the strongest cadence move.

### 3.8 Latency architecture (the D-anchor, freshage, the floor)

**(a) SOTA.** Interpolation pays a ~1-source-frame hold *by definition*; extrapolation is latency-neutral;
reprojection (Reflex 2) goes *below native* via fresh-input late warp. For external FG there is no Reflex hook
— LSFG defaults to Queue-Target-1 (one buffered frame) and does **not** run zero-buffer at GPU-max.

**(b) PhyriadFG.** A pure interpolator. `D = freshage_ema_ms + span_fresh_ms` (`main.cpp:7357-7363`), clamped so
`D ≥ freshage` always (the freeze floor). Measured combat ~73–84 ms; first-hand light-scene floor
`B_floormin` median 20.96 ms / min 7.54 ms (n=7114). The floor-min levers are **exhaustively measured
null/neutral** (`--low-d`, `--shallow-queue`, `--ingest-backlog`, `flow-scale`, `--copy-fence`). The
responsiveness lever that works is `--camera-twarp`.

**(c) Gaps.** `addressable`: **the interpolation→extrapolation pivot** (the strongest — display the freshest
real warped forward, no hold; PhyriadFG has every ingredient but no default extrapolation mode); **collapse the
lap-escape** (the §8 build-gap → span→1 → the D span term collapses, a double win); **OS raw-mouse fresh-input
for the camera arm** (the anti-cheat-safe slice of Reflex 2). `design-debt`: the D span term over-anchors to
phase-0 interior (~30–60 ms of removable excess in lap-escape combat — but see verify); fixed async buffer;
static ingest backlog; symmetric freshage EMA lags transients. `physics-floor`: the interpolation hold; the
WGC copy (proven GPU-bound by `--copy-fence`); no-Reflex CPU-render-queue elimination (class boundary).

**(d) Verify — verdict SOUND.** **Overstated:** the extrapolation pivot's "~1 frame off EVERY tick" is the
*optimistic* bound — ExWarp itself RL-selects extrap-vs-warp per-region because full extrapolation is too
unreliable; the realized win is *gated/bounded by the cleanly-extrapolable fraction*, and it inherits the
disocclusion frontier. The "30–60 ms removable D excess" assumes *sustained* span 4–6 — **no exported CSV
carries a span column**, so the figure is from `--phaselog`/memory, a worst-case-tick number, not the
steady-state mean; `--low-d` in lap-escape combat was **never A/B'd**. The async-frame removal leans toward
**physics-floor in the saturated regime** (removing a buffer risks re-collapsing the multiplier). **Missed:**
the **depthless reprojection ceiling** bounds *both* the pivot and the mouse-yaw lever — yaw-from-mouse is a
flat 2D rotation, correct for far-scene yaw but **cannot parallax near objects** without depth, so the
"~3 ms VALORANT-class" est-impact is over-optimistic (VALORANT reprojects *with* engine camera+depth). **All
primary SOTA citations verified, no fabrications.**

**(e) Floor.** The hold, the WGC copy, no-Reflex, no-VRR are floors. The arc's strategic conclusion ("floor =
physics + the deliberate quality/freeze budget") is **correct for the interpolation paradigm** — the one thing
it underweights is that the paradigm itself is the remaining lever, and the lap-escape span inflation is a
real, unbuilt, quality-free excess.

### 3.9 GPU saturation / make-space

**(a) SOTA.** Three levers: (L1) **leave headroom** — cap the base below ~85–90%; (L2) **timer-paced present
decoupled from the generation fence + drop-on-slip** — never shed the multiplier, drop only the *late* frame;
(L3) **bound generation cost** via reduced internal resolution. The negative (N1): **queue priority/preemption
is the wrong lever** on consumer GPUs (proven). The winning move: move FG *off* the saturated GPU onto a
second GPU.

**(b) PhyriadFG.** `--load-governor` (default-off): util-driven graduated tier floor that **sheds optional
work** (bwd-flow off, single-pass gme, drops object-repair/disocclusion passes) — "warp-light" on the 4090
side. L2 is **partial**: `--async-present` (default-off) IS the drop-on-slip pattern, but the **shipping
default is synchronous** (`vkWaitForFences(fBridge, UINT64_MAX)` on every tick, `main.cpp:6878` — the
present-side latency under saturation). L1 (the primary SOTA lever) is **structurally unavailable** (can't cap
a game you don't control) **and** blocked by the no-cap dogma. N1 correctly respected.

**(c) Gaps.** `design-debt`: **flip `--async-present` default-on** (§2 #5, the strongest — the only SOTA
lever PhyriadFG fully implements). `addressable`: the governor **sheds the multiplier where the SOTA keeps
generating + drops the late frame** (use L3 reduced-resolution instead of pass-shedding — keep disocclusion
quality under combat); no async-compute overlap (modest mid-motion, ~null at true 99%). `physics-floor`: L1
headroom (external + dogma); hardware flip metering; the 1Hz NVML governor signal lags transients.
`known-deferred`: principal-on-4090 cascade (the re-architecture).

**(d) Verify — folded in (a key demotion).** **The "combat multiplier collapse to 1.07× is the regime, not a
bug" is OVERSTATED as pure physics.** The GPU-time *shortage* is physics, but the **uncontrolled collapse** is
the *absence of an output-rate controller* — LSFG 3.1's **Adaptive Frame Generation** holds a target output
fps via a *fractional multiplier* under exactly this shortage. **Critically, this is NOT L1** (it does not cap
the game) — it manages the **output-side** multiplier, which an external tool **can** fully control. So
re-label the "multiplier collapse" portion from `physics-floor` to **`addressable-design`** (§2's lower-tier
AFG item). The L1 *base-game-cap* unavailability stays correctly physics/policy-floor. **Corrections:**
preemption is at **thread-block (CTA) granularity** on modern NVIDIA, *not* "whole dispatch runs to
completion" (the *conclusion* — priority can't starve the game — still holds, via non-binding-priority +
shared-cores, not whole-dispatch); the dual-GPU "~11 ms" conflates the **~3–5 ms PCIe transfer** with a larger
total-added figure, and the official guide does **not** claim the primary sees "near-zero hit" (soften to "a
real but not zero-cost offload"). **Missed:** the SOTA pattern is a **three-queue split** (graphics /
async-compute interp / present) — even with `--async-present` on, present still contends on A.q.

**(e) Floor.** L1-cap (external + dogma), priority/preemption (proven dead-end — **do not re-propose a
currency scheme**), and hardware flip metering are floors. The operator's belief that they are near the limit
**for the single-shared-GPU case is essentially correct**; the open frontier is the **multi-GPU thesis done
right** (principal-on-4090, B/iGPU as droppable cascades) — *plus* the AFG-style output controller the audit
mis-filed as physics.

### 3.10 Concurrency / safety / crash-class

**(a) SOTA.** Lock-free SPSC (relaxed/acquire/release + cache-line separation); **seqlock/versioned snapshot**
for a multi-word record published behind one index; **VK_ERROR_DEVICE_LOST detection on every submit/wait**
(the dominant field-crash class — recover or exit, never spin); documented WGC teardown ordering (revoke
`FrameArrived` *before* Close); RT pacing + a bounded watchdog.

**(b) PhyriadFG.** C/F/P over two seq_cst SPSC counters (`c_seq`, `f_seq`); the async-present path was split into
dedicated `bslot[]` buffers precisely because resetting the shared bridge mid-flight = DEVICE_LOST; the
present poll is non-blocking (`vkGetFenceStatus` + hard cap, **never** `vkWaitForFences`); two watchdogs
(F-stall/window-death, present-clock resync); `--rfp` carries a Tier-2 RISK_REGISTER with **every risk still
`open`**.

**(c) Gaps.** `addressable` **(critical)**: **zero DEVICE_LOST detection anywhere** (§2 #1 — every
submit/wait result discarded, exactly in the 99%/dual-GPU/ASPM regime where TDRs are most likely); WGC
teardown revoke + counted quiesce (§2 #18); a P-thread heartbeat watchdog; validation/sync layers in the
crash gate. `design-debt`: **the multi-field pair publish is not torn-read-safe** (no seqlock, §2 #11);
blanket seq_cst pushes the ordering contract into *prose comments* not code; `--rfp` ships its risk register
all-`open` (a Tier-2 gate violation or doc-drift); WGC ring-full drops the **newest** frame (policy
inversion); pinning/RT built but unmeasured *for jitter* (the slip measurement tested the wrong metric).

**(d) Verify — folded in (honesty corrections).** **The strongest-gap comparative claim is FABRICATED and
demoted:** "Both rivals (DLSS-G, LSFG) survive a TDR; PhyriadFG hard-crashes" is **uncited and contradicted by
field evidence** — there are extensive DLSS-G/FSR3 device-lost crash reports and **no** evidence either rival
does in-process device recreation. Restate as: *the robust target is graceful **exit**, which PhyriadFG also
lacks* — **not** "rivals recover and we crash." The fix is **detect + graceful exit/passthrough**, *not* the
days-long in-process recreate the audit prescribed (`vkDeviceWaitIdle` itself fails on a lost device;
in-process recreation is largely-undefined per Khronos). **Missed:** the WGC ring-full drop path *skips the
copy-fence Signal* — a liveness double-fault under `--copy-fence`; no DXGI `DEVICE_REMOVED` handling on the
capture D3D11 device; `vkDeviceWaitIdle` at teardown is itself a device-lost trap. **MMCSS caveat:** MMCSS
*demotes* a thread that exceeds its quota — for a present thread that legitimately runs longer than a tick
under 99% load, MMCSS can make pacing *worse*. **Verified-correct:** the seqlock gap (sound, design-debt not
physics) and the no-global-priority verdict (correct dead-end).

**(e) Floor.** No external tool can *prevent* a driver TDR — only *recover*. The torn-read window is **not
physics** (a seqlock or wider ring closes it cheaply). The genuine narrow floor: WGC gives no synchronous
"callbacks quiesced" signal — but even that is closable with an in-flight refcount. **The critical takeaway:
DEVICE_LOST graceful-exit is the highest-priority robustness work and is entirely absent today.**

### 3.11 Measurement / telemetry

**(a) SOTA.** Three tracks: end-to-end **photon** latency needs a physical sensor (LDAT/OSLTT) — no software
sees scanout+panel; software present-boundary telemetry (PresentMon, DXGI `GetFrameStatistics`); FG quality
without GT is ill-posed (proxies validated against BVI-VFI).

**(b) PhyriadFG.** Unusually disciplined: `--latency-trace` per-stage EMAs (single-clock, no cross-clock
subtraction); `telemetry_csv.hpp` (lock-free SPSC, PresentMon column names verbatim, **NA not a proxy** for
columns it cannot compute); `latency_probe.html` (camera vernier + spatial grid). The internal "lat ms" =
`t_present_ret − tcap_r` — capture-QPC to the moment `Present()` *returns*, **not to photon**.

**(c) Gaps.** `addressable`: **no present-to-photon instrumentation** (the "lat" omits 1–2 frames of
compose+scanout); **the cross-validation pass has never been run** (§2 #4 — the highest-value single action);
**no FG-quality scalar at all** (§2 #13 — the disocclusion frontier is eye-only); the spatial-grid probe has
no per-cell output. `design-debt`: per-stage **EMAs hide the latency tail** (the felt combat spike); FrameType
conflates drops/freezes; NVML/timestamp overhead *assumed* non-perturbing (violates the project's own
efficiency gate); silent-zero overloading on compose. `physics-floor`: input-to-photon's absolute value is
unreachable from external capture (correctly emits NA).

**(d) Verify — folded in (a load-bearing correction).** **The biggest gap's premise was FALSE:** the audit
claimed "DComp overlay, NOT a VkSwapchain, so no `GetFrameStatistics`." First-hand,
`framework/render/present/src/PresentSurface.cpp:204-235` shows a **real DXGI flip swapchain**
(`CreateSwapChainForComposition` + `FLIP_SEQUENTIAL` + `Present(0,0)`) attached to a DComp visual — MS docs
state this swapchain **is compatible with `GetFrameStatistics`** (`SyncQPCTime`/`PresentCount` reachable
**today**). This makes the gap **more** addressable, not less, and the audit's "harder DComp-stats path" is
unnecessary. The audit also **mis-cited** `DCompositionGetFrameStatistics` (that is a *forward-looking* compose
estimator, not a per-present record). **Citation corrections:** `arXiv:2510.01361` is **full-reference (needs
GT)** — usable only in a held-out-frame harness, *not* live on the B-flow (§2 #13); its headline is **+0.09
PLCC over FloLPIPS**, not an absolute "0.51"; FloLPIPS's "0.71/0.68" are **unverified** from the abstract —
soften to "SOTA correlation". **Missed:** **no pacing/animation-error metric** (§2 #14 — internally
computable, arguably above the quality-metric gap); the stage EMAs use the same silent-clamp-to-zero idiom the
audit flagged for compose but **didn't flag for build/detect/compute**; no realized step-count (refresh/source)
metric despite it being the proven smoothness ceiling.

**(e) Floor.** Input-to-photon's absolute value and the photon last-mile are hardware-only **for everyone**
(even PresentMon can't see scanout). No-GT quality tops out at PLCC ~0.5–0.7 — a noisy proxy, not an oracle.
The real deficits are that PhyriadFG **never closed the loop** between its two latency tools and ground truth,
and has **zero quality scalar** despite having the raw materials.

### 3.12 Color / format / HDR / scaling

**(a) SOTA.** DLSS-G/XeSS-FG support HDR **only** in HDR10 (10bpc PQ) — both explicitly **refuse scRGB FP16**
("too expensive in compute and bandwidth"). LSFG (the external rival) supports HDR **end-to-end** + auto
tone-map. MS guidance: carry FP16 through every stage to avoid clipping; present 10bpc to avoid banding.

**(b) PhyriadFG.** An **8-bit SDR pipeline end-to-end** with HDR-ingest-then-tone-map-to-SDR. HDR is **hard-clipped
to [0,1]** at convert (irreversible). The WGC capture path **hardcodes BGRA8** (`main.cpp:1769`) — HDR can't
even enter under production capture. Present is hard-locked `B8G8R8A8_UNORM` with **no `SetColorSpace1`**
(`PresentSurface.cpp:206`). Upscaling is **structurally disabled** (a real `0xC0000005` on the per-tick bridge
upscale).

**(c) Gaps.** `addressable` **(critical for HDR rigs)**: **no HDR present** (§2 #10 — 10bpc R10G10B10A2 is the
SOTA-cheapest target); **hard-clip → highlight-rolloff tone-map** (§2 #19); **no 10bpc/dither → banding**;
fixed exposure 1.0 (no metadata-aware tone-map). `design-debt`: WGC forces BGRA8 (HDR can't enter);
**warp/blend math runs in gamma space, not linear** (§2 #20 — wrong exactly at the crossfade boundary); the
8-bit iGPU pack is a hidden chokepoint. `known-deferred`: upscaling disabled (a documented crash → real
re-architecture).

**(d) Verify — verdict SOUND.** **The load-bearing SOTA claim verified verbatim:** NVIDIA *and* Intel both
refuse scRGB FP16 — so PhyriadFG carrying fp16 through flow/warp on Pascal (fp16 at 1/64 fp32, ~2× bandwidth)
would be a **measured regression**, confirming "no internal fp16" is a floor while "no 10bpc" is addressable.
**Overstated:** "10bpc near-zero cost" is half-true — the format is cheap but PQ encode + `SetColorSpace1` +
BT.2100 metadata are not (§2 #10 caveat). **Missed:** under Win11 Auto-Color-Management the BGRA8 overlay is
washed-out *even on an SDR desktop* (a separate already-reachable correctness bug — the same `SetColorSpace1`
fix); gap #2 (clip) and gap #4 (no dither) interact — fix both in one shader edit; the gamma-space error rides
the camera-twarp lever the operator most relies on (§2 #20). **Correction:** AFMF2's internal HDR working
format is *undocumented* — don't imply confirmed in-PQ processing.

**(e) Floor.** Internal scRGB-FP16 (the SOTA itself refuses it); HDR-to-SDR is inherently lossy if the
operator chooses an SDR present; exclusive-fullscreen HDR + HDCP/DRM are MPO/protection-walled (shared with
LSFG). Colorimetric wide-gamut interpolation is an **open frontier for the whole external class** — not a
place PhyriadFG is uniquely behind.

### 3.13 Router / break-even / device characterization

**(a) SOTA.** Task-based runtimes with **auto-calibrated, history-driven** per-task performance models
(StarPU) feeding HEFT/DMDA; the roofline under-predicts (SM-partitioning/wave-quantization) so production
refines it with micro-benchmarks; the FG real-time regime needs **deadline-aware**, not throughput-optimal,
scheduling. prima.cpp/Halda is the closest device-pooling prior art; LSFG dual-GPU does **no** measured
break-even (static user-pinned).

**(b) PhyriadFG.** A **complete capability-router pillar the product does not use.** `break_even_decide` +
`select_participants` + `run_routed` + `measure_transfer_bw` exist and are Mock-tested — but grep for
`GpuDescriptor`/`enumerate_gpus`/`measure_transfer_bw`/`compute_gflops` in `main.cpp` returns **zero**. Roles
are device-**type**-based hard-codes; the warp's break-even is **inlined as `offload=false`** (`main.cpp:50-52`).

**(c) Gaps.** `design-debt`: **the product never characterizes its own devices** (§2 #12 — the MEASURED block
is permanently zero; roles by type, not throughput); the break-even objective is **throughput-optimal where FG
needs deadline-optimal** (the wrong loss function); the model ignores `dispatch_overhead_us`; the iGPU is
engaged by *availability*, not by a gate; dead descriptor fields + a router with no in-product caller.
`addressable`: no dynamic re-routing under load (**demoted** — see verify). `physics-floor`: no P2P (the high
~596 crossover is a *direct consequence*, the correct honest conclusion, not a gap).

**(d) Verify — verdict SOUND.** **Citation errors (must fix):** `arXiv:2601.19910` mis-titled (it is about KV
offloading, not a roofline crossover); DARIS, MultiPath, Konstantinidis PII, GCAPS id all have title/identifier
drift — corrected. **Missed:** B-selection needs an **fp16** probe specifically (FG is fp16-bound; "first
discrete" can pick a raster-fast/fp16-slow card — Intel/AMD often beat higher-raster NVIDIA per-fp16 for FG);
the `--load-governor` is *already* a deadline-flavored adaptation (util-driven shed) — the gap is it sheds
quality rather than re-routing, not that PhyriadFG has no deadline mechanism. **Overstated:** the no-P2P gap's
"requires P2P-capable interconnect" is too absolute (MultiPath uses spare *PCIe* peer paths) — but the
conclusion holds (needs a spare peer path + injection, neither available on this x4 rig); the work-stealing
gap is "addressable but **measured-likely-null**" (the 4090 serialization leaves little idle device-time) —
the `addressable` *nature tag* overstates it.

**(e) Floor.** The inlined `offload=false` is **correct** (FG's AI≈2.5 sits 240× below the ~596 crossover,
itself a direct consequence of the no-P2P x4 link). The router is the **wrong tool for the latency goal**
(throughput vs deadline) — the measured dead-ends already prove router-class levers are null. The honest gap
is only that the decline is **assumed from the author's rig** rather than **measured per-rig** (§2 #12), plus
the design-debt of a product-unreachable router surface.

### 3.14 Theoretical limits (the synthesis)

This dimension *is* §1 and §4 — the SOTA taxonomy (interpolation hold / extrapolation-neutral /
reprojection-negative), the measured LSFG comparison, and the synthesis. **Verdict: SOUND**, with the
overstatement noted in §1: the "reclaim ~25–40 ms toward an LSFG-class ~30–45 ms total" headline is too
optimistic — LSFG's *absolute* total at 60→240 is ~66 ms (its small *added* number is the capped-base regime,
not a shallower hold), so the quality-free reclaim is **~11–18 ms** (the B-side build-gap + some ring-guard
spin), not the full ~25–40 ms. The verifier confirms the **`--asw` lever is a stronger reprojection seed than
credited** (it already projects `cur` forward — the pivot has a *working primitive in-tree*, making the first
step closer to engineering than "multi-arc research"), and that the adaptive-multiplier lever **also bounds the
hold**, not just cadence. Newer SOTA the audit should note: FSR "Redstone" ML-FG (Dec 2025) combines a neural
net over depth+MVs *with* classic MV-reprojection — a third data point that **the field is converging on
reprojection+neural-fill**, reinforcing (not weakening) the synthesis that the reprojection branch is the
long-term home of "feels like it isn't there."

---

## §4 — THE PHYSICS FLOOR (consolidated)

The irreducible limits for an external, anti-cheat-safe, multi-GPU FG, each with the SOTA that says so. These
are **not gaps** — naming them stops them being mis-filed as addressable, and tells the operator exactly where
*not* to spend effort.

| Floor | What it costs | Why irreducible (class vs frontier) | SOTA basis |
|---|---|---|---|
| **Capture-pipeline depth** (one DWM compose) | ~0.8–6 ms, INVISIBLE pre-`tcap` | **Class.** Below it is present-hook injection = anti-cheat ban. LSFG eats the identical cost | WGC shares a DWM-composited texture; eugen15 present-hook is injection (BattlEye/EAC/Vanguard, ACM 2025) |
| **The interpolation hold** | ~12–17 ms (one source frame) | **Class (paradigm).** Definitional for *any* interpolator; only extrapolation/reprojection escapes it | GFFE, ExWarp, Blur Busters — "interpolation adds latency by definition" |
| **GPU-saturation compute floor** | warp ~8 ms on the 99% 4090 | **Class.** Consumer priority does **not** preempt (CTA-granularity, non-binding priority, shared cores); L1-cap unavailable to an external tool + blocked by the no-cap dogma | WDDM preemption; D3D12 `PRIORITY_HIGH` no-op (MJP); Vulkan priority non-binding (Khronos); FG_SATURATION_PRIOR_ART |
| **Panel-cadence floor** | only ~N=refresh/source steps/pair; the +1 compose frame; the seam velocity-pulse | **Class.** No VRR/Reflex for a DComp overlay (LSFG hits it too, disables G-Sync); no engine MVs across the seam; hardware flip metering is proprietary | FG_CADENCE_LATENCY_PRIOR_ART; Raph Levien (compositor adds one frame); DLSS 4 flip metering |
| **Cross-device freshage tax** | ~3–8 ms structural (the **second** hop) | **Class, multi-GPU-specific.** PhyriadFG's 3-device chain (capture→flow→warp) pays a hop LSFG's single-GPU and even its dual-GPU (2-device) pipeline do not. This is the **honest cost of the anti-obsolescence thesis** (§5) | No-P2P host-bounce (4090 has none; cross-vendor); prima.cpp accepts the cross-PCIe coordination cap |
| **No-Reflex / no-VRR / no-fresh-input** | ~7–15 ms in-engine advantage, permanently out of reach | **Class.** Reflex/Frame Warp/depth-PTW require engine integration = injection. Even GFFE ("G-buffer free") still needs engine depth+pose+MV — PhyriadFG's position is *strictly harder* | Reflex 2 (in-engine); ASW 2.0 (app depth); GFFE (still needs depth) |
| **Disocclusion true-hole** | the residual "gravity" | **Frontier — the SOTA itself is stuck.** Content visible in neither frame has no honest non-neural fill; even neural extrapolation names disocclusion as its open failure mode | AceVFI 2025; SoftSplat; FILM; the FG_VFI_PRIOR_ART dossier |
| **VFI ill-posedness / no-GT** | correctness is unrecoverable | **Frontier.** From two frames there are infinitely many valid trajectories; no scorer recovers truth, only plausibility; no-GT quality metrics top out at PLCC ~0.5–0.7 | Velocity Disambiguation `arXiv:2311.08007`; VIDIM; AceVFI |
| **Internal scRGB-FP16** | would be a *regression* on Pascal | **Class + SOTA-shared.** NVIDIA *and* Intel both refuse scRGB-FP16 internally ("too expensive"); Pascal fp16 is 1/64 fp32. The SOTA answer is the *same* — process/present in 10bpc, which **is** addressable | DLSS-G + XeSS-FG programming guides (verbatim) |

**The crucial honest distinction (the operator's anti-sycophancy bar):** a *frontier* floor (disocclusion
true-hole, VFI ill-posedness, colorimetric gamut interpolation) is where **the whole field is stuck** —
PhyriadFG losing there to LSFG is a quality-tuning gap on a shared frontier, **not** a known technique it
ignores. A *class* floor (no-Reflex, no-VRR, no-fresh-input, the compose frame) binds **us** but not in-engine
tools — it is the price of admission to the anti-cheat-safe class, shared exactly with LSFG.

---

## §5 — Cross-cutting + relation to the holonic verdict

**The multi-GPU "holonic" split carries a real, named cost — and a real, named advantage.** The companion
[`HOLONIC_CONFORMANCE_AUDIT.md`](HOLONIC_CONFORMANCE_AUDIT.md) found that PhyriadFG is holonic in the
*containment/composition* sense but **does not embody the paper's empirical content** — and, decisively, that
its load-bearing axes optimize for the *opposite* of near-decomposability (tight F↔P coupling, a deliberate
coupling lead). **This architecture audit corroborates that from the performance side:**

- **The freshage tax is the engineering face of the conformance verdict.** The cross-device chain
  (capture→convert→flow→warp) is a tight *serial dependency* with inter-holon staging that **dominates**
  intra-holon compute (build 20 ms vs compute 8.7 ms; ~11 ms is B-side ingest+contention). That is precisely
  the **inter ≳ intra** coupling the conformance audit cited as the *inverse* of a near-decomposability
  spectral gap. The multi-GPU split is an **engineering convenience** (idle silicon = more total compute),
  **not** a near-decomposable holarchy — and it **pays a cross-device freshage tax single-GPU LSFG avoids**
  (§4). This is the honest cost of the anti-obsolescence thesis, stated plainly.

- **The genuine structural advantage is real and under-claimed.** A single-GPU LSFG **must** steal GPU time
  from the game (why it needs a cap); PhyriadFG runs flow+gme on the *otherwise-idle* 1080 Ti and convert on the
  *idle* iGPU — the FG's heaviest compute runs on hardware the game is not using, so PhyriadFG can **assert its
  slice at 99% without capping the game** (the `--upload-xfer` result). The anti-obsolescence thesis (idle
  GPUs = more total compute, assert-without-cap) is the **one axis where PhyriadFG has a real structural edge**
  over single-GPU LSFG. It is under-exploited.

**Doc/memory drift this audit surfaced (beyond the conformance doc's B-abort finding):**

1. **The router pillar is product-unreachable, but the "1.139× validated" claim has never been exercised by
   the shipping binary** (§3.13). The inlined `offload=false` is *correct* but the decline is **assumed from
   the author's rig, not measured per-rig** — and a reader of `main.cpp` would not know the router was
   "considered-and-correctly-declined" without finding line 50, 2700 lines from where the roles are assigned.
   **Correction owed:** either wire the one-time characterization in (so the decline is measured per-rig) or
   document the static-decline contract *at the role assignment* (`main.cpp:2724-2727`).
2. **`--rfp` ships its Tier-2 RISK_REGISTER with every risk still `open`** (§3.10) — a PLAN_TIER_PROTOCOL gate
   violation *or* a doc-drift (verifications run, register not updated). Resolve before any release framing.
3. **The latency dossier's ~73 ms is photon-*unvalidated*** (§3.11). The internal "lat" stops at `Present()`
   return; the two latency tools have never been reconciled to ground truth; per the FDP verifiable-reference
   mandate, the ~73 ms should be cited as "internally measured, present-tail untraced" until the §2 #4
   cross-validation pass is run. **And the audit's own "no swapchain → no `GetFrameStatistics`" belief was
   false** — the present surface IS a flip swapchain, so this validation is *more* reachable than assumed.
4. **Several SOTA citations in the raw audit were fabricated or mis-attributed** (the `arXiv:2601.19910`
   roofline mis-title, the `~596` crossover wrongly presented as a literature number when it is Phyriad's own
   measured constant, AceVFI's lead author, the NeuFlow-v2 speedup, the ASW-2.0 "fails below half refresh",
   DARIS/MultiPath/GCAPS identifier drift). **All are corrected in §3 and must not be reintroduced** — this
   document folds the corrections in; the raw audit JSON does not.

**The bottom line for the operator.** To get from "más o menos" to "muy muy bien": the **largest quality-free
latency win** is the ~11 ms B-side ingest overlap (§2 #2) + flipping `--async-present` default-on (§2 #5) +
pacing to the DWM clock (§2 #3); the **largest quality wins** are the non-neural forward-splat z-ordering
(§2 #6) + census flow cost (§2 #7) + consuming the affine field (§2 #15); the **largest robustness win** is
DEVICE_LOST graceful-exit (§2 #1, currently absent); and the **largest honesty win** is the one photon
cross-validation pass (§2 #4). Beyond those, the residual is the interpolation hold + cross-device + no-Reflex
floor — and the only thing *below* it is the reprojection branch (the `--camera-twarp`/`--asw` seeds run from
the freshest real every tick), which is **research, not tuning, and inherits the disocclusion frontier the
whole field is stuck on.** Near the limit *for this architecture* — yes. Near the limit for what an external,
anti-cheat-safe, multi-GPU FG could feel like — no; the reprojection branch is the unexplored headroom.
