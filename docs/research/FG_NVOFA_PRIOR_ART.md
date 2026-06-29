# FG_NVOFA_PRIOR_ART — the "more silicon" optical-flow lever (NVIDIA NVOFA vs neural) for PhyriadFG

> **Diátaxis type:** Analysis (a SOTA dossier, sibling of [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) /
> [`FG_SATURATION_PRIOR_ART.md`](FG_SATURATION_PRIOR_ART.md) / [`FG_CADENCE_LATENCY_PRIOR_ART.md`](FG_CADENCE_LATENCY_PRIOR_ART.md)).
> **Status:** `measured`/`designed` — the SOTA is from an 8-agent adversarial workflow (`wf_861a363a-8af`: 6 web
> researchers + feasibility designer + adversarial critic, 663 k tokens); the **driver-support gate is supervisor-verified
> first-hand** (the 4090 exposes `VK_NV_optical_flow` extension revision 1 — `vulkaninfoSDK.exe`); **the speed win itself
> is a PROJECTION until the microbenchmark lands** (§5). **Verifiable-reference mandate (FDP):** every external claim is
> leveled [V1/V2/V3] (§6); the catch about `sad_zero` is grounded first-hand in `main.cpp` (`sad_raw=RG16F(sad_best,
> sad_zero)` ~`1680`; the change-gate ~`456-459/1579`). **Why this exists:** the
> [efficiency-campaign measurement](FG_ARCHITECTURE_DCAD_MASTER_PLAN.md) found the single-GPU FG slice is DOMINATED by
> the classical block-match flow (~7.5 ms, saturating the 4090's compute cores to 95 % and inflating the rest by
> contention); the pure-efficiency headroom is ~1-2 ms. The big quality-free lever is the **idle dedicated silicon**.

## §1 — Verdict: NVOFA (the hardware OFA), NOT neural; a measure-first single-GPU lever

**The "more silicon" lever is the NVIDIA Optical Flow Accelerator (NVOFA) — a DEDICATED hardware engine on Ampere+/Ada
(the 4090), the same engine DLSS-G used — reachable VULKAN-NATIVELY via `VK_NV_optical_flow` (zero CUDA interop).** It
is recommended as a **drop-in FLOW PROVIDER only, behind a default-OFF `--nvofa` flag**: replace ONLY the inner
block-match (the ~7.5 ms cost); keep PhyriadFG's entire warp / gme / bg-snap / band-xfade / matte stack untouched.

**Neural flow is REJECTED for SPEED** (it is a future *quality* play, not a latency win for this contract): RIFE 4.6 is
~5.85 ms on a 4090 [V3] and **burns the very CUDA/tensor cores we want to free** + emits a finished frame, not the
separable MV+SAD the pipeline needs; NeuFlow-v2's "<5 ms on a 4090" is an UNMEASURED projection (15 ms on a 2080 [V1],
scaled by a generational guess from a different workload). Classical block-match is NOT obsolete in shipping FG (FSR3's
flow is itself a hierarchical 8×8 SAD block-match [V1]).

**The two wins (one defensible, one projected):**
- **Defensible — contention relief:** moving the flow OFF the compute cores onto the dedicated OFA engine attacks the
  A:95 % saturation that inflates the single-GPU slice. This is the more reliable claim than the raw ms. **(Now measured
  — §8: the relief is real at the component level (flow-`compute` −30 %, 5.7→3.95 ms), but the per-direction OFA path
  overhead offsets it and the TOTAL slice does not improve; the multi-sample classical baseline is ~11.7 ms, not the
  ~16 ms single-sample figure used below.)**
- **Projected — raw speed:** NVIDIA's App Note gives Ada 1080p/4×4 ~0.77-1.87 ms (FAST/SLOW, single-dir, no-cost,
  CUDA) [V1]; with fwd+bwd + the cost buffer + the converts, a realistic 1080p budget is ~2-3.5 ms — **likely beats
  7.5 ms, but UNVERIFIED at PhyriadFG's resolution/format/path** (§5 is the gate).

## §2 — Findings (first-hand-verified facts)

- **Driver gate PASSED (supervisor-verified):** the 4090 exposes `VK_NV_optical_flow` rev 1. The path is real on the rig.
- **Vulkan-native, zero CUDA interop [V1]:** `VK_NV_optical_flow` (Optical Flow SDK 5.0+, Ampere+) takes the captured
  `VkImage` pair via `VK_OPTICAL_FLOW_SESSION_BINDING_POINT_INPUT_NV`, syncs with standard semaphores on
  `VK_QUEUE_OPTICAL_FLOW_BIT_NV`, **no host round-trip, no external-memory dance**. Input `VK_FORMAT_B8G8R8A8_UNORM` is
  EXACTLY what WGC capture delivers (zero pre-convert). The existing flow is already a Vulkan compute pipeline (not
  CUDA), so the no-interop advantage is real (critic-confirmed).
- **Output [V1]:** MV as `R16G16_SFIXED5_NV` (S10.5; /32 → pixel units), an optional `R8_UINT`/`R32_UINT` cost map,
  `BOTH_DIRECTIONS` (fwd+bwd in one call), an external `HINT` (a free temporal prior — matches STAGE-42's
  `set_temporal_prior` intent), and `GLOBAL_FLOW` (serves gme's background-motion estimate directly).
- **The OFA does flow ONLY [V1]:** no pyramid generation, no post-processing; its single-layer search range is small →
  the software pyramid + the large-motion handling + the grid upsample STAY in PhyriadFG. This is why it is a *hybrid*
  (hardware match + software warp/disocclusion/SAD), not a wholesale swap.

## §3 — The catch (the honest obstacle; all three must hold, none provable from the corpus)

1. **`sad_zero` has NO NVOFA equivalent (first-hand, the load-bearing risk).** PhyriadFG's whole disocclusion / dissidence
   / stasis machinery (the `gme_fit_affine` change-gate `kChangeGateSadZ`, the per-8×8 `SUM|A−B|` scale, `soft_gate`,
   `stasis_thresh`) is built on `sad_zero` (the in-place residual) **and** `sad_best` in calibrated `SUM|A−B|` units.
   NVOFA gives a flow + an **8-bit derived cost** (0-255, higher=worse [V1]) on a different scale, and **nothing for
   `sad_zero`**. Mitigation: synthesize `sad_best` by a calibrated remap of the cost, and compute `sad_zero` with a
   **cheap separate `|A−B|` reduction pass** — which eats into the thin 1440p/4K latency margin, and the gme/matte
   thresholds need a recalibration sweep. An occlusion/confidence signal is recoverable from the fwd/bwd consistency
   (NVOFA gives both directions) — the standard FG safety net.
2. **The win is UNMEASURED at our resolution/format/path.** All the ms are 1080p/4×4/no-cost/single-dir/CUDA App-Note
   arithmetic; the real number is fwd+bwd+cost at PhyriadFG's flow grid (`mvw×mvh` = `WW_flow/8`) via the Vulkan path,
   and at 4K the margin over 7.5 ms could be slim. **§5 is the gate.**
3. **It PINS the flow to the 4090** (no OFA on the Pascal 1080 Ti). So it is a **single-GPU lever** — it fits the
   single-4090 contention problem, NOT the multi-GPU balance (where the arc offloads flow to the 1080 Ti and the slice
   is already ~11 ms). The corrected framing (the critic dropped the synth's "CRITICAL routing conflict"): under
   `single_gpu` the flow is **already** on the 4090 (`main.cpp:3748`), so for the single-GPU case there is NO conflict —
   NVOFA slots in cleanly there. For the multi-GPU rig it is a routing decision (`break_even_decide`: the idle-OFA win
   vs the weak-GPU-offload win are mutually exclusive).
4. **Quality is UNPROVEN on our content + needs an eye A/B (DEFERRED, non-blocking).** NVIDIA's KITTI/Sintel parity is
   on natural footage, not rendered+captured frames with HUD/UI/particles; the native 4×4 grid is coarser than the
   per-8×8 field (the fine-text-misalignment regime PhyriadFG already fought — `vblend-strength 1.0` broke text); and
   disocclusion (the hard frontier where it LOSES to LSFG) is NOT improved by swapping the flow. → an operator-eye A/B
   on a real scene is the final quality gate, deferred (it does not block the implementation/measurement).
5. **Strategic shadow [V1]:** NVIDIA itself DROPPED the OFA from DLSS 4 (replaced with an AI model) because chaining it
   per generated frame would throttle the GPU — a known ceiling at HIGH multipliers; for PhyriadFG's 1×-2× single
   interpolation it is likely fine.

## §4 — Refuted / corrected

- **Neural flow as a SPEED lever — REJECTED** (§1): burns the cores we want freed + emits a frame not MV+SAD. It is a
  future *quality* axis, not this contract's win.
- **The synth's "CRITICAL routing conflict" — OVERSTATED, corrected by the critic:** under `single_gpu` the flow is
  already on the 4090, so there is no conflict for the single-GPU case NVOFA targets (§3.3).

## §5 — The prototype plan (measure-first; the dogma: "si no puedes medir el hot path, no puedes afirmar que es rápido")

1. **MICROBENCHMARK (the gate, objective):** time `vkCmdOpticalFlowExecuteNV` (BOTH_DIRECTIONS + cost) on the 4090 at
   PhyriadFG's actual flow grid, across FAST/MEDIUM/SLOW presets. Capture (a) the isolated flow-call ms (the real
   beat-7.5 datapoint), (b) whether it runs uncontended under a dummy 4090 compute load (the A:95 % relief claim), (c)
   the cost values vs a known motion (to start the SAD remap calibration). **STATUS: ✅ DONE + supervisor-verified
   first-hand (`apps/render_assistant/bench/nvofa_bench.cpp`, the 4090, 0 validation errors). RESULT — the gate PASSED
   DECISIVELY.** At the operating point (FAST preset, BOTH_DIRECTIONS + cost, 100-iter GPU-timestamp medians): **1080p
   = 1.12 ms, 1440p = 1.81 ms, 4K = 4.02 ms** → vs the ~7.5 ms classical flow that is **6.8× / 4.2× / 1.9× faster**.
   **Contention relief CONFIRMED:** under a fully-pegged 4090 compute load the OFA loses only **0.8-3.0 %** (1080p
   +3.0 %, 4K +0.8 %) — it genuinely runs on the dedicated OFA engine, OFF the compute cores (the win that attacks the
   A:95 % saturation). MV sanity exact (a +6/+4 px known shift returned (+6.00,+4.00), cost 0; the S10.5 /32 decode
   validated). The OFA is its OWN queue family (idx 5 on the 4090: TRANSFER+OPTICAL_FLOW only, count 1) — uploads/layout
   transitions go on a graphics/compute family; OF-bound images chain `VkOpticalFlowImageFormatInfoNV` + live in
   `GENERAL`; cost format is `R8_UINT`; `performanceLevel` is in the session create; BGRA8 input is supported (idx 9).
2. **ONLY if (a) beats 7.5 ms at our resolution AND (b) shows real contention relief** → the in-pipeline `--nvofa`
   (default-OFF, byte-identical off): enable `VK_NV_optical_flow` + the OFA queue in `vdev_create` (gated); the
   `S10.5→RG16F` MV convert + the `cost→sad_best` remap + the `|A−B|→sad_zero` pass; everything downstream
   (`gme_fit_affine`, bg-snap, band-xfade, the host MV/SAD bridges) consumes the synthesized images UNCHANGED.
3. **Operator-eye A/B on a real scene** against the current block-match (DEFERRED, non-blocking — the final quality
   gate). Scalability/consumption framing: if it wins, `--nvofa` becomes a clean auto-enabled flow provider on
   NVOFA-capable GPUs (capability-detected), the single-GPU user's slice dropping toward the multi-GPU ~11 ms.

## §6 — References (leveled; ✔ = supervisor-verified first-hand)

| # | Claim | Source | Level |
|---|---|---|---|
| ✔ | The 4090 exposes `VK_NV_optical_flow` rev 1 (driver gate) | `vulkaninfoSDK.exe` on the rig | V1 first-hand |
|  | NVOFA Ada 1080p/4×4: ~0.77 ms (FAST)-1.87 ms (SLOW), dedicated engine | https://docs.nvidia.com/video-technologies/optical-flow-sdk/nvofa-application-note/index.html | V1 |
|  | `VK_NV_optical_flow`: R16G16_SFIXED5_NV MV + R8_UINT cost, B8G8R8A8 input, BOTH_DIRECTIONS+hint+global, no CUDA interop | https://docs.vulkan.org/spec/latest/chapters/VK_NV_optical_flow/optical_flow.html | V1 |
|  | NVOFA via native Vulkan (SDK 5.0, Ampere+), semaphore sync, no CUDA context | https://developer.nvidia.com/blog/accelerated-motion-processing-brought-to-vulkan-with-optical-flow-sdk/ | V1 |
|  | NVOFA cost = 8-bit derived confidence (0-255, higher=worse), not a literal SAD | https://forums.developer.nvidia.com/t/nvidia-optical-flow-sdk-how-to-interpret-the-output-cost-confidence-of-the-flow-vectors/82649 | V1 |
|  | NVIDIA replaced the HW OFA with an AI model in DLSS 4 (per-frame chaining throttles the GPU) | https://www.nvidia.com/en-us/geforce/news/dlss4-multi-frame-generation-ai-innovations/ | V1 |
|  | OFA single-layer search is small → a software pyramid is required (no pyramid/post-proc on the OFA) | https://developer.nvidia.com/docs/drive/drive-os/6.0.6/public/drive-os-linux-sdk/common/topics/nvmedia_understand/OpticalFlowAccelerator.html | V1 |
|  | Full NvOFFRUC (flow+warp+interp) = 4.41 ms on a 4090 1080p NV12 → the isolated flow sub-step is well under 7.5 ms | https://docs.nvidia.com/video-technologies/optical-flow-sdk/pdf/NVOFA_FRUC.pdf | V1 |
|  | NeuFlow-v2: 15 ms on a 2080 @~1024×436, 9M params, Apache-2.0 (the <5ms-on-4090 is a projection) | https://arxiv.org/html/2408.10161v1 | V1 |
|  | RIFE 4.6 ~5.85 ms on a 4090 1080p (tensor/CUDA cores, emits a finished frame) | https://github.com/hzwer/ECCV2022-RIFE/issues/217 | V3 |
|  | FSR3 optical flow is itself a hierarchical SAD block-match (8×8, 24×24 search, 7-level pyramid) — classical not obsolete | https://github.com/GPGPU-Design-Agent/FidelityFX-SDK/blob/main/docs/techniques/optical-flow.md | V1 |
|  | Lossless Scaling (the comparable) does NOT use NVOFA (vendor-agnostic shader model) — a lever LSFG cannot pull, PhyriadFG can | https://www.gamingonlinux.com/2025/07/lossless-scalings-frame-generation-for-linux-gets-upgraded-to-the-latest-v3-1/ | V2 |

## §7 — What could not be verified (honest gaps)

- All NVOFA ms are 1080p/4×4/no-cost projections; **the fwd+bwd+cost number at PhyriadFG's `mvw×mvh` via Vulkan is
  unmeasured** (§5.1 is exactly this gate).
- The `mvw×mvh`-to-NVOFA-native-grid mapping + upsample quality cost; the cost→`sad_best` remap calibration; the OF SDK
  EULA for redistribution — all unverified, to be settled in the prototype.
- Whether NVOFA's quality holds on rendered+captured game content (HUD/particles) — the deferred operator-eye A/B.

## §8 — Measured in-pipeline result (STAGE-115, `shipping` default-OFF) — the honest verdict

The `--nvofa` lever is **integrated, correct, and default-OFF** (`--force-single-gpu --nvofa`; auto-falls-back to the
classical OFP on any unmet precondition — non-single-GPU, `flow_div>1`, no `VK_NV_optical_flow`; **byte-identical when
off**, re-confirmed: classical single-GPU `freshage` median unchanged). It replaces the classical block-match flow with
the hardware OFA via the prep→OFA→convert semaphore chain (one CPU wait per pair on `fConv`).

**The component-level OFA win is real and measured in-pipeline** (Honkai: Star Rail, single-GPU 4090, 56–57 `--latency-trace`
samples each, medians):

| metric (median) | classical block-match | `--nvofa` | Δ |
|---|---|---|---|
| F-thread flow-`compute` (what the OFA replaces) | 5.7 ms | **3.95 ms** | **−30 %** (the HW OFA, in-pipeline) |
| total `freshage` (the whole single-GPU slice) | **11.7 ms** | 13.15 ms | **+1.45 ms — NO net win** |

**Honest verdict: the proven flow-compute speedup does NOT (yet) reach the total slice.** The OFA path's per-pair
overhead — the prep down-blit into the OFA inputs + the cross-queue (`A.q2`→OFA-queue→`A.q2`) semaphore chain + the
F-thread block on `fConv` — is **paid TWICE (once per direction: `nvofa_run` runs at `main.cpp:6523` fwd and `:6126`
bwd)** and offsets the ~1.75 ms compute saving, netting `freshage` slightly *worse*. This **corrects an earlier
single-sample overclaim** ("slice ~16→12 ms"): that compared a noisy high classical sample against a low NVOFA one; the
proper multi-sample classical baseline is ~11.7 ms, not ~16 ms.

**The lever to convert the component win into a slice win = collapse the bidir redundancy.** The OFA session is already
`BOTH_DIRECTIONS` (`main.cpp:2202`): a SINGLE execute on canonical inputs (`prv→in0, cur→in1`) produces *both*
`FLOW_VECTOR` (fwd = `prv→cur`) and `BACKWARD_FLOW_VECTOR` (bwd = `cur→prv`). Feeding both convert passes from that one
execute would halve the prep+chain overhead — the path most likely to push `freshage` below the classical 11.7 ms. It is
**NOT done here**: the bwd consumer currently re-executes with *swapped* inputs, so whether the single-execute `n.mvB`
matches what the bwd warp/gme needs is a **bwd-flow-quality question that requires the operator-eye A/B** (a wrong bidir
mapping degrades the backward warp silently — measurement alone cannot gate it). Flagged as the next NVOFA step, eye-blocked.

**Residual (non-blocking, default-OFF path):** `VUID-vkCmdDraw-None-09600`, 2 occurrences at startup under validation only
— a deferred submit-time layout check reports an OFP-adjacent image `UNDEFINED` for ~the first 2 frames (`ofp.execute()`
never runs on the OFA path). Benign (content is don't-care at startup; cap recovers, `frz 0`, runs clean). Not motion/sad
(a one-shot RO seed of those did not clear it) nor cand (null without `--ambig`). Full root-cause + barrier = a v1.1 item.

**SAD eye-calibration (deferred):** `sad_best`=`cost_scale·cost` remap and `sad_zero`=separate `|A−B|` pass both need the
operator-eye A/B against the classical `SUM|A−B|` units (`--nvofa-cost-scale`/`--nvofa-sadz-scale`) — the §3 catch, the
final quality gate.
