# FG_ARCHITECTURE_DCAD_MASTER_PLAN — the perfect external-capture frame-gen architecture (general, 1/2/n-GPU)

> **Diátaxis type:** Explanation + plan (a design/master-plan, not a how-to). **Status:** `designed` for the
> architecture; the SOTA findings are `measured`/cited; the code anchors (`file:line`) are **first-hand
> supervisor-verified** (this pass); the single-GPU FG slice is the load-bearing **UNMEASURED** unknown (Phase 1).
> **Provenance:** adversarial workflow `wf_d6686327-178` (19 agents — 8 SOTA researchers + 5 architects + 5
> adversarial critics + 1 synthesizer; 2.7 M subagent tokens). **Verifiable-reference mandate (FDP):** every
> `file:line` was re-read first-hand before publishing; the two highest-value web citations (LSFG Adaptive Frame
> Generation; NVIDIA Reflex 2 Frame Warp) were supervisor-re-fetched and confirmed; the remaining web references
> carry the workflow's adversarially-vetted [V1/V2/V3] levels (§10). **Companions:**
> [`PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md`](PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md) (the prior all-angles gap audit — this
> plan CORRECTS one of its findings, §11), [`HOLONIC_CONFORMANCE_AUDIT.md`](HOLONIC_CONFORMANCE_AUDIT.md),
> [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md), [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md),
> [`FG_SATURATION_PRIOR_ART.md`](FG_SATURATION_PRIOR_ART.md), [`GPU_MULTI_GPU_PRIOR_ART.md`](GPU_MULTI_GPU_PRIOR_ART.md).
> **Supersedes the FRAMING** of [`AYAMA_LAYERED_FG_MASTER_PLAN.md`](AYAMA_LAYERED_FG_MASTER_PLAN.md) (the holonic
> tri-tier plan): the partition is now by MEASURED capability, not by a named device holarchy.

## §0 — The pivot (the operator's redirect)

The "holonic" framing is **dropped**. The [conformance audit](HOLONIC_CONFORMANCE_AUDIT.md) showed PhyriadFG borrows
the vocabulary without the empirical content — the router that would *force* the holonic partition collapses to a
constant, the partition was chosen by hardware, and the design optimizes for the *opposite* of near-decomposability.
That ornament was steering the architecture instead of serving the problem. This document is **just an architecture**,
with one objective: **perceptual perfection — "feels like the FG isn't there" — for ANY user's GPUs**, scaling
gracefully across three configurations the author happens to test on (1 GPU = 4090; 2 GPU = iGPU+4090; n GPU =
iGPU+4090+1080 Ti) but designed for the **general** case (one dGPU; iGPU+dGPU; multiple dGPUs of any vendor/tier).
Multi-GPU stays viable but is **demoted**: it is a capability the architecture scales *into*, not the center.

**Hard constraints (non-negotiable, inherited):** external-capture only (anti-cheat-safe; no engine / motion-vector
injection; must run on Battlefield 6); parameter-free (no manual tuning); honest (no invented benchmark numbers, no
fabricated citations). LSFG is the legitimate comparable — it is also external-capture.

## §1 — Headline verdict: DCAD, and the single-GPU user is the load-bearing case

**One spine — DCAD (Detect → Characterize → Assign → Degrade) — one FG core, three placements.** The architecture
treats GPUs as a **measured participant set**, not named devices; four logical roles (CAPTURE, FLOW, WARP, PRESENT)
are decoupled from physical devices, so one role graph maps onto 1..n GPUs.

The uncomfortable, load-bearing truth the workflow surfaced: **the most common user has a single dGPU, and that case
is the one PhyriadFG cannot even run today** (`main.cpp:2732` hard-exits without a second discrete GPU — verified
first-hand, §2) **and has never measured** (only the author's 3-device rig was ever profiled). Every "the FG slice
fits in the headroom" claim for the common user is **unfounded until that slice is measured** (Phase 1). The
multi-GPU work is real and valuable, but it cannot rescue the single-GPU user — so the single-GPU tier is where
"perfect" has to be earned first.

## §2 — The DCAD general spine

A single codebase that produces the optimal external-capture FG topology for any rig. **Four phases run ONCE at
startup, off the hot path; placement is then FIXED for the session.** Per-frame re-routing is **rejected** — global
heterogeneous scheduling latency (~10–12 ms, prima.cpp/Halda class [V1]) dwarfs the ~4 ms frame slice; the FG grain
is too fine for runtime re-decision.

1. **DETECT (vendor-neutral).** Vulkan `VkPhysicalDeviceProperties` (vendorID / deviceType — the inventory) +
   DXGI `IDXGIFactory6::EnumAdapterByGpuPreference` [V1] (the Windows present-side ordering / which GPU drives the
   monitor). No hard-coded card names.
2. **CHARACTERIZE (by MEASUREMENT, never by name/type).** FG is **fp16-bound**: raster throughput and VRAM are NOT
   the routing key (Pascal GP102 runs fp16 at 1/64 fp32 — a "fast" card can be useless for FLOW). Probe each device's
   fp16 (or DP4a/int8 on no-fp16 silicon) rate + measured host-link bandwidth at the **real** negotiated gen×lanes.
   Persist the table cross-session (StarPU history-model style [V1]) so session 2+ skips cold-start; **invalidate on
   any device-set or driver-version change**.
3. **ASSIGN (deadline-aware min-completion-time).** Pick the device with the minimum *predicted finish including
   transfer cost*, subject to the present deadline (a `dmda`/HEFT structure [V1], NOT throughput-optimal HEFT), gated
   by a per-rig break-even **re-derived from this rig's measured constants** — never ship the author's ~596 FLOP/byte
   crossover (verified at `GpuDescriptor.hpp:78` as Phyriad's own 4090+1080 Ti measurement, not literature) — plus a
   vendor-ordering guardrail.
4. **DEGRADE (the same mechanism inverted).** A missing or lost device is an empty slot; roles collapse onto
   survivors down to one GPU; **terminal state = FG-OFF passthrough** (the game keeps running). This is also the
   device-loss recovery path (§8 Phase 0).

**Invariants (MUST hold):**
- **Single-crossing dataflow.** A frame crosses any device boundary **exactly once, one direction**
  (capture → flow/warp → present → scanout). **Never copy a warped frame back.** When FLOW and WARP sit on different
  devices, ship the **compact recipe** (~MV + SAD field) across PCIe, not the full frame (the existing `--upload-xfer`
  DMA copy-out is the right direction).
- **One present authority, always.** N present threads = SLI/CrossFire runt-frame micro-stutter. Assistant devices are
  producers on a bounded surface queue; they **never present independently**.

**First-hand grounding (this pass).** `main.cpp:2713-2732` selects devices purely by LUID (primary `pA`) +
`VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU` (`pG`) + `VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU` (`pB`), and **hard-exits
`return 1` when `!pA||!pB`** — the product literally cannot run on one GPU today. The `framework/gpu` router
(`break_even_decide` / `measure_transfer_bw` / `GpuDescriptor`) has **zero in-product callers** (only a comment at
`main.cpp:50`). **DCAD replaces this block** (§8 Phase 2).

## §3 — The three tier instantiations

### T1 — single dGPU shared with the game (the most common, the hardest, the load-bearing case)
All four roles on one device; frames never leave it. Governed by ONE identity: **per base frame the game cedes
`FG_slice_ms` of its render budget**, so when GPU-bound the real framerate DROPS and the realized multiplier falls
below nominal. The measured ~1.07× combat collapse **IS this arithmetic** (a fixed slice subtracted from a pegged
budget), **not a scheduling bug** — consumer-GPU queue priority does NOT preempt a saturating dispatch (a proven
dead-end). The convert that is zero-copy on an iGPU now runs on the saturated dGPU, *worsening* the slice (named, not
hidden).

Two — and only two — dogma-compatible levers:
- **(A) Shrink the slice.** Adaptive internal **flow-scale** (the LSFG Resolution-Scale analog; the existing
  `--flow-scale`) tied to measured GPU util. It governs the FG slice, **not the game** → no-cap-dogma-safe.
- **(B) Stop pegging the OUTPUT side.** An **LSFG-3.1-style fractional output-multiplier controller** [V1, verified]
  that holds a target output fps and *floats* the realized multiplier (e.g. 2.0× at base 72, 1.4× at base 103)
  **without touching the game's input/render rate**. This is the dogma-compatible escape from the 1.07× collapse and
  the **highest-value UNTESTED single-GPU lever** — it replaces today's whole-multiplier `--load-governor` shed.

Plus an **AFMF2-style image-derived motion-threshold fallback** [V2] (repeat-freshest-real / global crossfade) for
fast motion and scene cuts.

**Honest floors specific to T1** (these are the structural ceiling, §6): the external overlay pays a **double present**
(the second present is on a separate DComp surface on the same saturated GPU); and it gets the interpolation hold
**without** the mandatory-Reflex/anti-lag offset that in-engine FG gets — so on a GPU-bound single dGPU its added
latency is **structurally ~1–2 frames worse than DLSS-G/XeSS-FG**, uncloseable without an engine hook. **AFMF2**
(driver-level, optical-flow-only, same-GPU-as-game) is the truest head-to-head comparable — **not** DLSS-G/FSR3.

### T2 — two GPUs as a unidirectional STAGE PIPELINE (not a work-split)
The shipped LSFG dual-GPU pattern, field-validated [V1/V2]. The strong/render device **P** keeps 100 % of the game's
render budget and pays only ONE async copy-out; the secondary **S** runs the **entire** FG slice
(convert+flow+warp+blend+pace+present). FG compute never competes with the game for P's silicon — **this is the one
genuine structural edge a 2-GPU external FG has: it asserts the slice on hardware the game is not using, escaping the
1.07× collapse without capping the game.** The real-frame stream crosses PCIe **exactly once** (P→S→scanout);
copying the warped frame back is rejected.

Two secondary classes differ only in transfer physics:
- **(a) iGPU** = zero-copy shared system RAM via `VK_EXT_external_memory_host` (no PCIe write-back leg; first-hand
  4090→iGPU ≈ 0.76 ms — **FLAGGED legacy-memory, re-verify**). A first-class S for low-res/high-fps (convert / pack /
  present), and a FG-compute participant **only if its measured fp16/int8 clears the target** (the author's *old*
  iGPU is convert-only, ~135× too slow; modern RDNA2+/Xe2 iGPUs run full FG — the role is by measured probe, never by
  "it is an iGPU").
- **(b) weak dGPU** = a real host-bounce hop (first-hand ≈ 2.90 ms, 1080p DEVICE_LOCAL, gen3 ×4 ≈ 3.2 GB/s; SOTA ≈ one
  extra frame / 3–5 ms).

**Enable-S is a MEASURED break-even**, not a default: offload only when P is genuinely game-saturated AND S clears its
transfer+compute cost. Field data [V2]: 4060Ti+1660S gave 70 vs 54 fps ("16 fps free") when the primary was game-bound,
but 4080+1080 **dropped** fps (the 4080 had internal headroom → the second device was pure loss). Display ideally on S
so generated frames never copy back; the iGPU-contention penalty (iGPU convert + WGC host-write + dGPU host-read share
one DDR controller) must be **in** the break-even, not the isolated 0.76 ms.

### T(n) — same role graph, every hop pays its own way
Every added cross-device hop must **independently** clear its measured tax (~one extra frame) or it is **dropped**
(prima.cpp "remove the net-negative weak device" [V1]; add a device only when measured(compute) AND measured(link)
both clear the per-rig break-even — never by device count). The author's 4090 + iGPU-convert + 1080Ti-flow+present
3-device chain pays a **second serial host round-trip** a 2-device pipeline does not; two serial ~2.90 ms bounces plus
iGPU contention can approach the ~4 ms slice it is protecting. **Whether the second hop clears its own cost is
UNMEASURED and beyond documented art** (no shipping FG gives 3+-GPU guidance). **The clean 2-GPU stage pipeline is the
general target; n>2 is a measured probe, not an assumption** (open decision, §9).

Cross-vendor device-local P2P is a **hard `deviceUUID` wall** (Vulkan external-memory requires matching deviceUUID
[V1]) → every cross-device hop is a serial host round-trip through a shared host buffer; on D3D12-cooperative rigs
prefer cross-adapter shared heaps [V1], but the PCIe cost is irreducible either way and the transport is
vendor-driver-brittle → wrap every share/open in failure handling, validate at startup, fall back to all-on-capture-GPU.

## §4 — The FG-core decision (one core; interpolation base + a bounded reprojection arm)

**The core is IDENTICAL on every tier** (warp/blend/ordering is not hardware-tier-dependent — a deliberate
simplification for the 1→n mandate): backward-warp for coverage (the shipped `wap_warp.comp`) + one-sided commit at
disocclusion (never average two misaligned samples) + cheap importance-Z ordering at overlaps (no tensor cores) +
freshest-real history-ring fetch for static/camera-moving background holes (degrades to the gme global-affine floor
for *moving* backgrounds — named).

**Paradigm = confidence-gated HYBRID** (the convergent SOTA answer [V2]: ExWarp/ExtraSS/FSR-Redstone gate per-region
between warp and extrapolate), minus the neural/depth legs the external class forbids:
- **INTERPOLATION is the safe base everywhere.** It pays the ~12–17 ms one-source-frame **HOLD by definition** — a
  paradigm-class floor identical for LSFG/AFMF/DLSS-G/FSR3, **not removable by pipeline tuning** (§6).
- **A BOUNDED, confidence-gated reprojection ARM** is bolted on as a *perceptual responsiveness* lever — **never** a
  Reflex-2-class click-to-photon claim. PhyriadFG is structurally **ASW-1.0 class** [V1] (color-only, self-derived MVs,
  NO depth, NO fresh input); even a perfect arm presents pipeline-late.

**THE HARD CORRECTION (verified first-hand this pass — refutes every prior design's self-description).** The
depthless-safe reprojection is **rotation-only** (without depth, only camera rotation is correctable; translation /
parallax MUST stretch/judder [V2]). But the shipped `--camera-twarp` (STAGE-114c) leads by `gme_model_mv(uv)` = the
**full 6-parameter affine** evaluated per pixel (`wap_warp.comp:738-743`: `a + b·gx + c·gy` / `d + e·gx + f·gy` —
carries translation `a,d` AND zoom/shear `b,c,e,f`; the lead is applied at `wap_warp.comp:770-773`), fit from
`main.cpp:1572-1608`. **The code does the depthless-UNSAFE thing (translation+zoom) while the design prose claimed
rotation-only.** This is almost certainly the residual *snap*/judder the operator felt. It MUST be reconciled before
`--camera-twarp` is defaulted on in any regime: **either** restrict the lead to the extracted rotational component,
**or** relabel the arm honestly as a full-affine perceptual lead with its judder ceiling stated (open decision, §9).
Note also: the arm's "camera velocity" is the gme affine fit on **one-pipeline-stale captured frames** — even the
responsiveness claim is stale-fit extrapolation, not fresh-input reprojection.

**Tier-conditional choices (only two):**
1. **FLOW algorithm class by MEASURED device budget** (NOT the classical/neural label): weak / no-2nd-GPU →
   DIS/SAD block-match (portable to CPU at 300–600 Hz/core [V1]; the shipped SAD matcher is the right family),
   **never** variational TV-L1 (saturates a 1080Ti sub-game-rate); a measured-fp16-capable device → optionally a
   purpose-built lightweight CNN (NeuFlow-v2-class, recurrent-free) at reduced internal res, or DP4a/int8 on Pascal
   (the XeSS-FG shipping pattern [V2], with an explicit accepted quality penalty). RAFT-lineage nets are off-budget
   even on a 4090 and are never selected.
2. **Whether a boundary-only lightweight neural disocclusion-fill is enabled** (only where measured budget supports
   it — unexplored quality headroom; LSFG proves lightweight-neural runs on Pascal via plain CUDA cores, so
   all-classical is a deliberate engineering choice, not a hardware floor).

## §5 — The hardware-adaptation policy (parameter-free, measured, persisted, invalidated)

- **STEP A — per-device capability vector:** `{fp16_gflops (or DP4a int8 on no-fp16 silicon), host_link_GBps (measured
  H2D at real gen×lanes), is_uma, is_monitor_gpu, vendor, deviceType}`. **PILLAR CHANGE OWED:** `GpuDescriptor`
  (verified first-hand, `GpuDescriptor.hpp:48-58`) has `compute_gflops` only — it **must gain an fp16/DP4a field**
  before ASSIGN can rank FLOW correctly (a raster-fast / fp16-slow card must be rejected even if it is "first
  discrete"). This is a published cross-process POD layout change (currently 128 B) → an explicit FR / pillar-mod the
  operator must approve (§9).
- **STEP B — role→device deadline-aware MCT:** minimum predicted completion time *including* measured transfer-to-reach
  cost, subject to the present deadline. FLOW ranks by measured fp16/DP4a (not raster); PRESENT prefers the monitor's
  GPU (one-way hop); CONVERT/PACK prefers a UMA iGPU (break-even dominated by the avoided copy, minus a measured
  contention term).
- **STEP C — admission gate per offload:** a device JOINS only if measured(compute) AND measured(link) both clear the
  **per-rig** crossover (re-derived from this rig's measured FLOP/s + link BW — a fast-link rig has a low crossover, an
  ×4 rig a high one; shipping the author's 596 would mis-route every other machine). A weak device on a starved link
  (copy/bus-engine pinned 100 % + low VRAM use = the transfer-bound signature) is **removed**, Halda-style.
- **STEP D — guardrails on the ranking:** the vendor-ordering matrix (prefer the stronger/feature-bearing vendor as P;
  **never** AMD-render + NVIDIA-secondary — games fail to launch; flag OpenGL titles that ignore the OS GPU preference
  — all community-leveled [V3] → validate at startup before encoding as hard rules); the transfer-ceiling clamp on the
  offered output fps (`max_output_fps ≈ link_GBps / frame_bytes`). **The whole decision is LOGGED** so the static
  decline is measured per-rig, not assumed (closes the doc-drift where `offload=false` is effectively hard-coded from
  the author's rig).
- **QUALITY GOVERNOR (parameter-free, content-adaptive, anti-cheat-safe):** above a motion-dispersion bound (from the
  already-computed gme IRLS residual + |mv−mv_prev|) the warp degrades gracefully (reduce flow-scale / repeat-real /
  global crossfade) rather than strobing disocclusion garbage (the AFMF2 Fast-Motion-Response pattern [V2]).
- **PACING (same on every tier):** own the present timing against the **predicted DWM compose QPC**
  (`qpcVBlank + n·qpcRefreshPeriod`, `DWM_TIMING_INFO` [V1]) + spin-finish — **never** `DwmFlush` (blocks), **never**
  the current free-running `refresh_hz` NCO. Target sub-4 ms present-interval jitter (the perceptual constant, §6);
  never gap the cadence (drop-on-slip REPLACES, never gaps — the composition-swapchain skip-to-latest queue gives this
  for free **IF** the surface is allocated "displayable", currently UNPROVEN for this overlay — `submit_at` /
  `SetTargetTime` returns Unavailable today).

## §6 — Honest physics floors (NON-addressable — the class price)

1. **Interpolation hold** (~12–17 ms, ~1 source frame): to place a frame between reals N and N+1 the pair must already
   contain N+1. Definitional for ALL interpolation; identical for LSFG/AFMF/DLSS-G/FSR3; not removable by tuning
   (LSFG 3 still adds ~10–13 ms at 40→80 after a 24 % improvement).
2. **Depthless reprojection is limited to 2D rotation**: without depth, only camera rotation is correctable;
   translation/parallax MUST stretch/judder (ASW-1.0 result). **The current `--camera-twarp` crosses this floor**
   (§4) — a floor the implementation violates, not one it respects.
3. **No in-engine Reflex/anti-lag offset** (external class): in-engine FG adds a hold but SUBTRACTS render-queue
   latency via mandatory anti-lag, often netting flat. The external tool gets the hold WITHOUT the offset → ~1–2
   frames structurally worse on a GPU-bound single dGPU; uncloseable without an engine hook.
4. **Double present** (external overlay): a second present on a separate DComp surface; on a single dGPU it contends
   on the same saturated GPU. A structural cost term that belongs in the slice budget.
5. **One-DWM-compose capture floor**: WGC shares the DWM-composited texture (≥1 compositor pass behind the game's real
   present) and fires on COMPOSE ticks, so the source cadence is itself DWM-compose-quantized — a second beat in-engine
   FSR3/XeSS-FG do not have, which may dominate the present-side jitter the ~4 ms budget assumes.
6. **Hardware flip metering** (Blackwell DLSS 4 [V2]): pacing moved into the display engine; unreplicable by software.
   The external ceiling is FSR3-class CPU-spin pacing — match that tier, name flip metering as the floor above it, do
   not chase it.
7. **Cross-vendor device-local P2P wall** [V1]: every cross-device hop is a serial host round-trip; consumer GeForce
   has no P2P; the 1080Ti slot is ×4-electrical ~3.2 GB/s — irreducible by any API.
8. **Disocclusion true-hole** (content in NEITHER frame): no honest non-neural fill; the artifact concentrates exactly
   at moving-object boundaries where gaze also concentrates. Rivals detect-and-dodge it, they do not solve it.
9. **No valid full-reference FG-quality metric** [V1]: PSNR/SSIM/LPIPS/VMAF/FloLPIPS (SROCC well below ~0.6–0.7) do not
   correlate with human VFI perception → an internal quality scalar is a regression-catcher/guardrail, NOT the
   objective function; **operator-eye (or a held-out-frame harness) remains the arbiter.**
10. **Queue priority does not preempt** a saturating dispatch on consumer GPUs → the 1.07× single-GPU collapse is
    arithmetic, not a scheduling bug.

## §7 — Addressable gains (ranked)

1. **Fractional output-multiplier controller** (single-GPU, **highest value**): the dogma-compatible escape from the
   1.07× collapse — no second GPU needed; holds target output fps, floats the realized multiplier, never touches the
   game input rate. **The prior gap audit mis-filed this collapse as physics; it is addressable design** (§11).
   Untested — must be implemented + measured.
2. **Wire the router + add fp16/DP4a characterization** (generality, design-debt not research): the pillar exists with
   zero callers; the fp16 field is missing. Closing this is what makes the system actually hardware-adaptive instead
   of hard-coded to the author's rig.
3. **AFMF2-style motion-threshold fallback** (universal quality, cheap): repeat-real / global-crossfade above a
   gme-dispersion bound, using signals already computed; anti-cheat-safe, no 2nd GPU, no neural net.
4. **DEVICE_LOST detect + graceful exit** (robustness, must-do): turns a TDR from a hard crash into clean passthrough;
   gates the entire degradation ladder.
5. **Predicted-DWM-compose pacing + spin-finish** (perceptual, sub-4 ms jitter): replace the free-running NCO — the
   load-bearing perceptual constant for "feels native", bounded by the upstream capture beat.
6. **Displayable-surface independent-flip / MPO scanout** (present-side latency): the single biggest external-overlay
   latency lever (removes the ~1 compositor frame) + free skip-to-latest drop-on-slip — best-effort, currently
   unproven (`submit_at` returns Unavailable).
7. **Adaptive `--flow-scale` tied to measured util** (single-GPU slice-shrink): automatable within the no-cap dogma.
8. **Recipe-not-frame cross-device transport** (multi-GPU): ship the compact MV+SAD field over PCIe, not the full
   frame — PhyriadFG's structural edge when warp happens on the present-side device (`--upload-xfer` is the right
   direction).
9. **Honest net-benefit / recommend-disable gate** (single-GPU, the honesty win): when the slice cannot fit even at
   min flow-scale + fractional multiplier, surface that PhyriadFG is net-negative vs the plain game — the one config the
   multi-GPU thesis cannot rescue, stated rather than hidden.
10. **Boundary-only lightweight neural disocclusion-fill** (strong-tier-only, research probe): unexplored quality
    headroom on capable devices; int8/DP4a, region-masked to the small disoccluded fraction — scoped, not committed,
    unmeasured against the 4090's ~50 % spare.

## §8 — Phased path from the current PhyriadFG

> **PLAN_TIER note:** Phases 0, 3, 5 (and the n→2→1 re-route in 7) touch the hot present path / concurrency /
> device-loss → they are **Tier-2** and REQUIRE a `*_RISK_REGISTER.md` (each risk mitigated-as-code + first-hand
> verify, byte-identical-off behind default-off flags) before commit. **DEVICE_LOST is the LEAD risk item.**

- **PHASE 0 — robustness, lands FIRST.** Add `VK_ERROR_DEVICE_LOST` / `DXGI_DEVICE_REMOVED` detection on EVERY
  `VkResult` + graceful EXIT/passthrough (NOT in-process device recreation — undefined per Khronos; `vkDeviceWaitIdle`
  itself traps on a lost device). Verified ABSENT today. Shipping any multi-GPU re-route without this turns a TDR — most
  likely in the 99 %/dual-GPU regime DCAD targets — into a hard crash.
- **PHASE 1 — measure the load-bearing unknown.** Instrument + report the single-dGPU FG slice in ms (capture +
  on-dGPU convert + flow + warp + double-present) at 1080p/1440p/4K for the DEGENERATE one-GPU topology. NEVER
  measured. **Gate all T1 generality claims on this number.**
- **PHASE 2 — wire the existing-but-dead router into startup.** Replace the deviceType hard-codes + the `!pA||!pB`
  hard-exit (`main.cpp:2713-2732`) with a one-time characterization (EnumAdapterByGpuPreference + Vulkan deviceType
  scan + break_even_decide/measure_transfer_bw + an ADDED fp16/DP4a probe). Add the fp16 field to `GpuDescriptor`
  first (pillar change). Persist + invalidate. Degenerate single-dGPU assignment + the net-benefit gate fall out.
  Design-debt closure, not research.
- **PHASE 3 — the highest-value single-GPU lever.** Implement the fractional output-multiplier controller in the
  DComp present path; replace the whole-multiplier `--load-governor` for the single-GPU case; tie adaptive
  `--flow-scale` to measured util. Measure headroom recovery vs the current shed.
- **PHASE 4 — the cheap universal quality win.** Make the AFMF2-style motion-threshold fallback the centerpiece for the
  single-GPU user (gme IRLS residual + |mv−mv_prev| already computed → repeat-real / global crossfade). Decide the
  threshold + which fallback feels best via an on-rig eye A/B against AFMF2.
- **PHASE 5 — present-side latency + pacing unlock.** Probe whether the DComp surface can be "displayable"
  (independent-flip / MPO scanout, removes ~1 compositor frame); pace to the predicted DWM compose QPC + spin-finish;
  flip `--async-present` default-ON after Tier-2 crash validation. Measure jitter with a display-change-time proxy, NOT
  `MsBetweenPresents`.
- **PHASE 6 — reconcile the responsiveness arm with physics.** EITHER restrict `--camera-twarp` to the extracted
  rotational component (depthless-safe) OR relabel it honestly as a full-affine perceptual lead with its judder ceiling
  stated; resolve whether an OS raw-mouse fresh-input hook is anti-cheat-safe before adding it. Do NOT default the arm
  on until reconciled.
- **PHASE 7 — generalize transport + degradation, measured.** Wire n→2→1 re-route as the same scheduler with a
  shrunken participant set; measure whether the primary can absorb FLOW within the present deadline when the assistant
  is lost; ablate D3D12 cross-adapter heaps vs the D3D11 host-bounce on the no-P2P ×4 rig; validate
  `VK_EXT_external_memory_host` + vendor-ordering hazards on non-author rigs before encoding guardrails.

## §9 — Open decisions for the operator

1. **MEASURE FIRST** — the single-dGPU FG slice (Phase 1). The whole T1 value proposition is unfounded until this
   number exists. *(Recommend: do this before any T1 code.)*
2. **Recommend-disable policy** — is the architecture willing to **recommend disabling itself** on a saturated single
   dGPU where the slice cannot fit (the prima.cpp remove-the-only-device case)? An honesty stance, not just code.
3. **`--camera-twarp` reconciliation** (§4/§6.2) — rotational-component-only (depthless-safe) vs honest full-affine
   relabel; and whether to pursue an OS raw-mouse fresh-input hook AT ALL given the unverified BattlEye/EAC/Vanguard
   (BF6) flag risk for a global raw-input listener. Do not default the arm on until decided.
4. **Anti-cheat overlay fingerprint** — a game-sized WDA-excluded topmost DComp overlay matches a known EAC heuristic
   pattern (community-leveled [V3]). For BF6 this is product-killing if it fires — does it warrant a first-class
   hardening plan + empirical test, or is WDA-exclusion + OBS-class track record sufficient? Needs a real-rig verdict.
5. **Tier-2 ceremony** — confirm a RISK_REGISTER with DEVICE_LOST sequenced as the LEAD item (§8).
6. **fp16/DP4a field on `GpuDescriptor`** — approve the published cross-process POD layout change (128 B → +field)
   before ASSIGN can rank FLOW correctly.
7. **Cache-invalidation policy** — whether a cheap first-frame re-validation against the cached capability table is
   worth the cost to avoid mis-routing on silent driver/thermal drift.
8. **n>2 go/no-go** — pursue the 3+-device chain (the second serial host-bounce may not clear its cost; unmeasured,
   beyond documented art) or **freeze the general target at the clean 2-GPU stage pipeline?**

## §10 — References (leveled; ✔ = supervisor-re-verified first-hand this pass)

| # | Claim (abbrev.) | Source | Level |
|---|---|---|---|
| ✔ | LSFG 3.1 Adaptive FG = fractional multiplier to a target fps, game rate untouched | https://losslessscaling.com/adaptive-frame-generation/ | V1 |
| ✔ | Reflex 2 Frame Warp needs in-engine camera+depth+input; cannot be a standalone overlay | https://www.nvidia.com/en-us/geforce/news/reflex-2-even-lower-latency-gameplay-with-frame-warp/ | V1 |
| ✔ | Code anchors (main.cpp:50/1572-1608/2713-2732; wap_warp.comp:738-743,770-773; GpuDescriptor.hpp:48-79) | `G:/phyriad/apps/render_assistant/...` | V1 first-hand |
|  | XeSS-FG: FG is a fixed GPU-time slice; in-engine FG needs mandatory anti-lag (XeLL) | https://github.com/intel/xess/blob/main/doc/xess_fg_developer_guide_english.md | V1 |
|  | LSFG dual-GPU = unidirectional stage pipeline; display on secondary or copy-back; PCIe fps ceiling | https://sageinfinity.github.io/docs/Guides/DualGPUGuide | V1 |
|  | Field break-even: 4060Ti+1660S "16fps free"; 4080+1080 dropped fps | https://benhaslett.substack.com/p/dual-gpu-frame-generation-with-lossless | V2 |
|  | AFMF 2.1 Fast Motion Response = graceful repeat/blend fallback; DLSS-G Auto Scene Change Detection | https://www.igorslab.de/en/amd-expands-afmf-2-1-with-fast-motion-response-finally-an-answer-to-fast-game-scenes/ | V2 |
|  | Depthless reprojection limited to 2D rotation; translation/parallax judders | https://www.uploadvr.com/reprojection-explained/ | V2 |
|  | Meta ASW 2.0 quality needs app depth + projection; ASW 1.0 (no depth) = "noticeable artifacts" | https://developers.meta.com/horizon/blog/developer-guide-to-asw-20/ | V1 |
|  | Convergent SOTA = per-region RL selector gating warp vs extrapolate (not a paradigm winner) | https://arxiv.org/abs/2307.12607 | V2 |
|  | Frame-time VARIANCE governs smoothness; ~4 ms imperceptible, ~12 ms clearly degraded | https://research.nvidia.com/publication/2024-08_variable-frame-timing-affects-perception-smoothness-first-person-gaming | V1 |
|  | Subjective VFI quality dominated by disocclusion-boundary artifacts; FR metrics correlate poorly | https://arxiv.org/abs/2202.07727 | V1 |
|  | No FR VFI metric (incl. FloLPIPS) correlates acceptably with perception | https://arxiv.org/abs/2207.08119 | V1 |
|  | prima.cpp/Halda: measured heterogeneity, removes net-negative weak devices | https://openreview.net/pdf/7a27facdef568023ecd5333d99e377f1b97fc46a.pdf | V1 |
|  | StarPU: auto-detect + persistent perf models; dmda min predicted completion incl. transfer | https://starpu.gitlabpages.inria.fr/features.html | V1 |
|  | Vulkan external-memory requires matching deviceUUID → no cross-vendor device-local P2P | https://github.com/KhronosGroup/Vulkan-Docs/issues/614 | V1 |
|  | DXGI EnumAdapterByGpuPreference reorders adapters by preference (vendor-neutral select) | https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference | V1 |
|  | DWM_TIMING_INFO: qpcVBlank + qpcRefreshPeriod → predict next compose; do not DwmFlush | https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ns-dwmapi-dwm_timing_info | V1 |
|  | FSR3 pacing: high-priority thread + busy-wait present to a target delta (external-overlay recipe) | https://gpuopen.com/manuals/fsr_sdk/techniques/frame-interpolation-swap-chain/ | V1 |
|  | DLSS 4 Flip Metering in the display engine; measure with MsBetweenDisplayChange | https://www.techpowerup.com/344255/amd-fsr-redstone-has-a-frame-pacing-problem-doesnt-use-flip-metering | V2 |
|  | XeSS-FG via DP4a on Pascal/RDNA1 (int8, not fp16 — Pascal fp16 = 1/64) | https://www.club386.com/intel-drops-xmx-requirement-for-xess-frame-generation-bringing-the-technology-to-older-geforce-and-radeon-cards/ | V2 |
|  | DIS/inverse-search flow 300–600 Hz on one CPU core; TV-L1 saturates a 1080Ti | https://ar5iv.labs.arxiv.org/html/1603.03590 | V1 |
|  | Composition swapchain: per-present target times, skip-to-latest queue, independent-flip if "displayable" | https://learn.microsoft.com/en-us/windows/win32/comp_swapchain/comp-swapchain | V1 |
|  | WGC anti-cheat-safe (OBS-class, no injection); residual risk = overlay fingerprint, not capture | https://store.steampowered.com/app/993090/Lossless_Scaling/ | V2 |
|  | D3D12 cross-adapter shared heaps = vendor-agnostic transport (standardizes, doesn't remove PCIe cost) | https://microsoft.github.io/DirectX-Specs/d3d/IndependentDevices.html | V1 |

> The author's iGPU transfer numbers (4090→iGPU 0.76 ms; weak-dGPU host-bounce 2.90 ms) carry a legacy-memory flag —
> re-verify on the live rig before treating them as `measured` (Phase 1/2/7).

## §11 — What this plan corrects

- **The 1.07× single-GPU collapse is ADDRESSABLE design, not a physics floor.** The
  [gap audit](PHYRIADFG_ARCHITECTURE_GAP_AUDIT.md) listed the multiplier collapse among the structural costs; the
  fractional output-multiplier controller (§3 T1, §7.1) is the dogma-compatible escape. The arithmetic *cause* (a fixed
  slice on a pegged budget) is real; the *response* (timidly shedding the multiplier) was the mistake.
- **`--camera-twarp` violates the depthless-rotation-only floor** (§4, §6.2) — a first-hand finding that overturns the
  arm's own STAGE-114 design comments (which framed the affine lead as the *fix*; it is depthless-unsafe for the
  translation/zoom components). Reconcile before defaulting it on (Phase 6).
- **The partition is by MEASURED capability, not a named holarchy** — superseding the framing of
  [`AYAMA_LAYERED_FG_MASTER_PLAN.md`](AYAMA_LAYERED_FG_MASTER_PLAN.md) and discharging the holonic conformance verdict
  into a concrete, falsifiable, general design.
- **The single-GPU user — the majority — is the load-bearing case and is unrun + unmeasured today.** Every prior plan
  optimized the author's 3-device rig; "perfect for any GPU" starts by making the one-GPU case run and measuring its
  slice.
