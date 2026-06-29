# FG_VFI_MEASUREMENT_SOTA — how frame generation / video interpolation is measured rigorously, and what the testbench must provide

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` for the cited
> literature facts (peer-reviewed papers + vendor/tool primary docs, verified 3-0
> in the source workflow) · `designed` for how the Phyriad testbench applies them
> (the testbench is **not built** — task #13). Single declared type held throughout;
> this is not a how-to and not the plan itself.
>
> **Scope.** The measurement methodology for real-time frame generation (FG) and the
> broader video-frame-interpolation (VFI) field: how QUALITY (with ground truth),
> LATENCY, THROUGHPUT, and PACING are each measured, what a controlled FG test
> ENVIRONMENT must supply, and which parts are documented-standard versus
> proprietary/unsourced. It is the SOTA input that the forthcoming **controlled
> FG test-environment** master-plan (task #13) consumes. It does **not** specify the
> build (that is the master-plan + implementation-strategies + risk-register — now authored,
> see §10) and it does **not** restate the PhyriadFG roadmap.
>
> **Reconcile-not-duplicate (FDP §4.6).** Two prior-art dossiers already own facts this
> dossier defers to and MUST NOT re-state as new: [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md)
> owns the ghosting/crossfade artifact-cure family and the metric-correlation-ceiling detail;
> [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md) owns the
> LSFG head-to-head, the closeable-objective framing, and the in-repo `fg_quality_scorer`
> facts (its modes/metrics/CI gate). They are cited here only as the Phyriad anchor for each
> measurement component, never re-described as new findings (full cross-links in §11).
>
> **Provenance.** Literature facts are from deep-research workflow `whsdgxiez`
> (5 angles → 20 sources → 83 claims → 25 verified → 23 confirmed, 2 killed; all
> load-bearing findings 3-0 against PRIMARY sources). The research agents fetched and
> verified those URLs first-hand; **the author did NOT independently re-fetch every
> external URL in this session** — they carry the workflow's verification levels (§9),
> per FDP §2.4. In-repo anchors (the scorer README, the PhyriadFG master-plan) WERE
> read first-hand this session.
>
> **Normativity (BCP 14 / RFC 2119+8174):** MUST / MUST NOT / SHOULD / MAY are used in
> their RFC senses only where a real constraint binds.

---

## §0 — The testbench MEASUREMENT-CONCERN map (M1–M7) and the three ID namespaces

This dossier organizes the SOTA around the **seven MEASUREMENT CONCERNS a controlled FG/VFI
testbench must answer**. Each is given a stable ID **M1–M7** (`M` = *measurement concern*,
DISTINCT from the master-plan's BUILD components `TB-C1..TB-C8`); the per-section analysis ties
its findings to the concern it informs, and §8 maps each to the build component + the fixed point
it enables.

| ID | Measurement concern | SOTA basis | Status |
|---|---|---|---|
| **M1** | Deterministic GROUND-TRUTH source (dense-high-fps held-out OR synthetic analytic motion) | §1, §2 | designed |
| **M2** | Objective QUALITY scorer (FR pixel/perceptual + no-reference) | §3 | partly built (`fg_quality_scorer`) |
| **M3** | LATENCY instrumentation (present-side software + external hardware) | §4 | designed |
| **M4** | THROUGHPUT + PACING instrumentation (PresentMon present/display + FrameType) | §5 | designed |
| **M5** | Difficulty STRATIFICATION (motion-magnitude strata) | §6 | designed |
| **M6** | GPU SATURATION generator (hold a precise target utilization) — **the key gap** | §7 | designed, **unsourced** |
| **M7** | Harness ORCHESTRATION (resolution/fps sweep + telemetry capture + reproducible A/B) | §8 | designed |

> **★ Namespace (THREE distinct ID schemes — do not conflate).** The existing
> [`PHYRIADFG_MASTER_PLAN.md`](PHYRIADFG_MASTER_PLAN.md) already uses **C1–C10** for the
> project's *closeable fixed points* (objectives — e.g. `C7` = "input latency ≤ LSFG +
> Δ", `C8` = "FG-quality regression gate in CI", `C6` = "pacing jitter < X ms"; see
> [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md` §4](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md)).
> The **M1–M7 in THIS dossier are *measurement concerns*** — the questions the testbench must
> answer. They are DISTINCT from the **build components `TB-C1..TB-C8`** in
> [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) (SourceWindow … scorer — the
> apparatus that ANSWERS these concerns). Read every `M<n>` here as a measurement concern, every
> bare `C<n>` as a master-plan fixed point, and `TB-C<n>` (where cited) as a build component;
> §8 maps each M-concern to the build component + the fixed point it enables.

---

## §1 — The confound this testbench exists to defeat

Phyriad's external-capture FG, measured on real combat scenes, produced a frame-gen
multiplier ranging **0.59×–3.1×** across scenes — a spread that drowns any per-lever
signal (`measured`, Phyriad's own observation; render-assistant arc, the BF6-combat
telemetry sessions). That spread is not noise in the instrument; it is the **VFI
scene-confound**, and the field has a name and a proof for it.

Using real-video *skipped frames* as ground truth "violates the linear-motion
assumption that is all two input frames can support, making the true in-between frame
unrecoverable without an oracle" (Kiefhaber/Niklaus/Liu/Schaub-Meyer, arXiv:2403.17128,
3-0). Verbatim: *"By using skipped frames as ground truth, any video can in theory be
used to evaluate frame interpolation methods … given two frames one can only reasonably
expect the motion to be linear … Any other assumption would require an oracle when
synthesizing the in-between frames."* Real scenes vary in motion magnitude, occlusion,
and non-linearity from shot to shot; an A/B run over them measures the *scene*, not the
*lever*. **The 0.59×–3.1× spread is that confound class.** Defeating it is the reason
component **M1** (a deterministic source with recoverable ground truth) and component
**M5** (motion-magnitude stratification) exist.

---

## §2 — M1 · GROUND TRUTH: two rigorous routes, and why synthetic is the only full escape

Rigorous VFI quality needs the **true intermediate frame**. There are exactly two ways
to obtain it; everything else is the confound above.

**Route A — dense high-frame-rate capture, hold out the middle.** Shoot the source far
above the interpolator's input rate; feed the interpolator a temporally subsampled
subset; the withheld real frames ARE the exact reference. This is the still-standard VFI
GT protocol:
- **Middlebury** (Baker et al., ICCV 2007, 3-0): captured at 100 fps — *"We provide every
  4th image to the optical flow algorithms (i.e., 25 Hz) and retain the remaining
  intermediate frames as ground-truth for evaluating frame interpolation."*
- **X4K1000FPS / XVFI** (ICCV 2021, arXiv:2103.16206, 3-0): 4K videos at **1000 fps** with
  extreme motion — dense sampling makes the true t+0.5 an actually-captured frame.
- **LAVIB** (NeurIPS 2024, arXiv:2406.09754, 3-0): held-out-middle protocol — triplets for
  single-frame interpolation, septuplets for multi-frame, scored against the withheld real
  frame with PSNR/SSIM/LPIPS.

**Route B — synthetic rendered scenes with analytic motion.** The true per-pixel
displacement (and thus the true intermediate) is computable *by construction*, by
projecting known 3D scene motion onto the image plane or constraining transforms to strict
linearity, then re-rendering at t+0.5:
- **Middlebury §3.2** (3-0): scenes rendered with Mental Ray; *"The ground truth was computed
  using a custom renderer ('lens shader' plugin), which projects the 3D motion of the scene
  corresponding to a particular image onto the 2D image plane,"* giving precise ground-truth
  motion and object boundaries with full control of occlusion, motion blur, materials.
- **Kiefhaber et al. 2024** (arXiv:2403.17128, 3-0): 666 4K nonuplets built via *"random
  homography transforms that strictly follow the constraint of linearity"*; *"Thanks to the
  synthetic nature of our data, we know the true optical flow."* The strict-linearity
  constraint is precisely what real skipped-frame GT violates.

**Why synthetic is the only *full* escape.** Route A still rests on captured photons of a
real scene: it is exact, but its motion content is whatever was filmed, so it cannot
*isolate* a chosen displacement, and a too-sparse hold-out re-introduces the linearity
violation. Route B lets you *dial* the motion (a known sweep velocity, a known rotation, a
set resolution and source framerate) and have the analytic t+0.5 by re-rendering — the only
construction that yields a clean, repeatable A/B at a chosen difficulty. **M1 therefore
SHOULD prefer the synthetic analytic-motion route for per-lever A/B**, and MAY use Route A
held-out corpora for breadth.

> **Honest framing slip carried from the source (do not over-read "analytic").** "Analytically
> exact" loosely spans two distinct cases: Middlebury §3.3's *interpolation* GT was
> EMPIRICALLY camera-captured (exact real frames, not formula-computed), whereas a fully
> *analytic* GT comes only from a fully-specified synthetic scene re-rendered at t+0.5. Both
> are valid; only the second is computable without an oracle from the scene definition alone.

**Phyriad application (designed/partly-built).** The shipped `fg_quality_scorer` already
implements Route A: its `sequence <prefix> <count>` mode decimates a dense source and
reconstructs the held-out middle by the canonical VFI protocol
([`fg_quality_scorer/README.md`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md),
read first-hand: the manifest `sequence`/`triple` forms, modes A/B/T). Its companion
`prep_zoo_sequence.py` renders a deterministic **non-repeating value-noise** source — a
Route-B-flavoured synthetic source chosen specifically to avoid the periodic-texture aperture
confound the README documents. The M1 gap is not the scorer; it is a *controlled synthetic
motion source* (known displacement, set resolution/framerate) wired to the sweep — see M7.

---

## §3 — M2 · QUALITY METRICS: pixel-fidelity → VFI-perceptual → no-reference

With a true intermediate (M1), score it. The SOTA spans three tiers; the testbench scorer
SHOULD carry one of each, because their failure modes differ.

- **Pixel-fidelity.** Middlebury's **gradient-normalized SSD** against the withheld GT (3-0):
  *"For image interpolation, we use the (square root of the) SSD between the ground-truth image
  and the estimated interpolated image. We also include a gradient-normalized SSD,"* kept
  SEPARATE from flow angular/endpoint error because interpolation quality ranks algorithms
  differently than flow accuracy. Plus the standard **PSNR / SSIM / LPIPS**.
- **VFI-perceptual (full-reference).** **FloLPIPS** (Danier/Zhang/Bull, ICIP 2022,
  arXiv:2207.08119, 3-0): *"a bespoke full reference video quality metric for VFI … re-designed
  its spatial feature aggregation step by using the temporal distortion (through comparing
  optical flows) to weight the feature difference maps,"* reporting *"superior correlation
  performance (with statistical significance) with subjective ground truth over 12 popular
  quality assessors."* It concentrates the metric where the interpolator's motion is *wrong*
  (where ghosting lives), which global PSNR/SSIM dilute. **Best subjective correlation in the
  field.**
- **Efficient VFI-perceptual.** **DIV** (Motion Field Divergence, arXiv:2508.09078, 3-0):
  *"DIV emerges as the most computationally efficient metric, requiring only 112.4ms per
  frame"* — ~**2.7× faster** than FloLPIPS (305.1 ms) while retaining ~**87%** of its
  correlation. The harness scorer for a tight regression loop.
- **No-reference (the live-path metric).** **Temporal Smoothness (TS)** — *"A no-reference
  metric which evaluates how consistently motion evolves"* (same source family, arXiv:2508.09078,
  3-0). This is the **only** quality axis available when no GT frame exists — i.e. the live FG
  output, which by construction has no held-out truth.

> **Honest limits carried from the source.** DIV and TS are NOVEL, single-dataset (BVI-VFI,
> 180 sequences), community-unvalidated; DIV's *absolute* correlation is only moderate (PLCC
> ~0.51). FloLPIPS's "12 assessors" result is a self-evaluation on the authors' own BVI-VFI
> benchmark. The metric *ordering* (PSNR/SSIM ≪ LPIPS ≪ bespoke-VFI) is robust; absolute SROCC
> is dataset-fragile (~0.6–0.83 on BVI-VFI). VMAF — named in the original question — had **no
> surviving verified claim** for FG/VFI use (§10).

**Phyriad application (built).** The `fg_quality_scorer` already implements PSNR + SSIM + two
artifact metrics: `dbl_edg_m` (motion-masked spurious-edge energy — the README's PRIMARY
metric, a truth-less FloLPIPS-*analogue* by motion-presence masking) and `flowdsc`
(flow-discrepancy, the FloLPIPS flow-*discrepancy* mechanism using the pipeline's own MV
field). The truth-less **mode T** (`crossfade_residual` + `double_edge_energy`) is Phyriad's
TS-class instrument for the live, no-GT path. LPIPS is deliberately deferred (needs an ONNX
net). The README is explicit that these are *localizers read with PSNR/`mcov`*, not standalone
verdicts — matching the source's caveat that warp-error is not provably motion-independent.
Adopting **DIV** into mode T is a named, low-cost upgrade. M2 is the most-built component; its
remaining work is calibration (an asymptote, not a fixed point).

---

## §4 — M3 · LATENCY: an external-capture FG cannot measure input-to-photon in software

This is the hard constraint for Phyriad's architecture (WGC capture, **no engine/motion-vector
hook**, DComp overlay present). Latency decomposes, in software, as **PC Latency = I2FS + FS2P +
P2D** (NVIDIA, "Understanding and Measuring PC Latency", 3-0): *"PC Latency is the summation of
the average input-to-frame-start (I2FS) latency, frame-start-to-present (FS2P) latency, and
present-to-displayed (P2D) latency."* The input side is gated on engine-emitted markers: I2FS
*"is the time between the PCLStatsInput ETW event and the SIMULATION_START marker"* — Reflex-SDK
markers a third-party capture process cannot emit. **Therefore an external-capture FG with no
engine hook cannot measure input-to-frame-start in software at all.**

What IS available splits cleanly:
- **Present-side, hook-free.** PresentMon exposes *"Until Displayed"* (*"time between the
  Present() call and when the frame was displayed"*) and *"Display Latency"* (*"Time between
  frame submission and scan out to display"*) (PresentMon `README-CaptureApplication.md`, 3-0).
  This is the only latency portion an external capture can self-measure (plus its own internal
  added latency). Caveat: PresentMon "displayed" is the flip/display-change event and scan-out
  is **modeled, not a true panel photon**.
- **True input-to-photon → EXTERNAL HARDWARE only.** NVIDIA **LDAT** is *"a discrete hardware
  analyzer that uses a luminance sensor to … measure the motion-to-photon (click-to-muzzle
  flash) latency,"* with NO game/engine integration and works across all GPU vendors (NVIDIA
  developer page, 3-0). The affordable equivalent: an open-source Teensy-4.1 clone that
  *"measures the complete end-to-end latency from mouse click to photon on screen for any GPU or
  system"* (S4N-T0S/Open-Source-LDAT, corroborating). **A phone slow-mo capture is the
  poor-man's photodiode** — count frames between the input event (visible on a synced overlay)
  and the on-screen response (author inference, `designed`; not a source claim, but the same
  photon-level principle).

> **Refuted specifics (do not rely on; FDP §2.4).** The claim that input-to-photon is exposed
> only as a PresentMon `MsClickToPhotonLatency` metric was **killed 0-3**. The claim that
> PresentMon separates rates via distinct overlay indicators "FPS-Presents vs FPS-Display" was
> **killed 1-2**.

**Phyriad application (designed).** M3 = present-side PresentMon (`MsUntilDisplayed`) for the
self-measurable portion + an external photodiode/phone-slow-mo rig for true input-to-photon.
This is the apparatus behind master-plan **fixed point C7** ("input latency ≤ LSFG + Δ").
**Open probe carried forward:** whether PresentMon's "All Input to Photon" attributes latency
correctly *through a third-party DComp overlay* is unconfirmed
([`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md` §5](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md):
"Probe before declaring C7 turnkey"). The decisive `--async-present` multiplier test — the
root-caused fix for the combat collapse — MUST be measured here under controlled conditions,
not on confounded combat.

---

## §5 — M4 · THROUGHPUT + PACING: axes SEPARATE from latency (the FG-multiplier point)

The load-bearing conceptual point: **the FG multiplier is a throughput fraction, not a latency
figure.** A frame-gen present rate of N× tells you how many frames reach the panel, and says
*nothing* about input responsiveness — they are orthogonal axes, and the testbench MUST keep
them so. PresentMon instruments both throughput and pacing:

- **Throughput axis.** *"Presented FPS"* = *"Rate of application calls to a Present() function"*
  (submission throughput) vs *"Displayed FPS"* = *"Rate of frame change measurable at display"*
  (PresentMon README, 3-0). v2.3.0: *"The Displayed FPS metric now tracks both application and
  generated frames,"* and **FrameType** classifies each frame (Application / Repeated / vendor
  FG) — the means to measure the **FG present-throughput fraction as a distinct axis**
  (PresentMon releases, 3-0).
- **Pacing axis.** `MsBetweenPresents` (present cadence) vs `MsBetweenDisplayChange` (display-change
  pacing), plus `MsInPresent`, `MsRenderPresentLatency`, `MsUntilDisplayed` (reintroduced in
  v2.3.1). **Animation Error** = `(AnimationTimeN − AnimationTimeN-1) − MsBetweenDisplayChangeN`
  captures stutter invisible to FPS — GamersNexus: *"a system could maintain perfectly consistent
  framerates while exhibiting poor animation error"* (3-0, secondary).

> **★ Animation Error provably cannot score fake frames** — the *animation-time* half has no
> reference point for a generated frame ("there's no animation time for fake frames"). The
> *display-interval* half (`MsBetweenDisplayChange` jitter) DOES apply to generated frames. So FG
> pacing is scored by display-interval variance, not by full Animation Error. Caveat: FrameType
> auto-classification needs the Intel-PresentMon provider; the most accurate Animation Error needs
> source SimStart (Reflex/XeLL) — an uninstrumented external-capture FG gets present/display pacing
> ONLY, not engine-true simulation time.

**Phyriad application (designed).** M4 = PresentMon CSV capture of Presented-vs-Displayed FPS +
FrameType (the multiplier as a throughput fraction) + `MsBetweenDisplayChange` p99 jitter (the
pacing axis, behind master-plan fixed point C6). The measured 0.59×–3.1× spread is a *throughput*
observation and MUST NOT be reported as a latency or quality result — keeping the four axes
(quality M2 / throughput M4 / latency M3 / pacing M4) un-conflated is the master pitfall this
testbench is built to avoid.

---

## §6 — M5 · DIFFICULTY STRATIFICATION: isolate the per-lever signal

Interpolation quality is *highly* sensitive to the motion domain, so difficulty MUST be
stratified by objective scene attributes — above all motion magnitude — or it confounds the A/B.
**LAVIB** (NeurIPS 2024, 3-0) stratifies by four per-video metrics: **Average Flow Magnitude**
(motion intensity), **Average Laplacian Variance** (sharpness), **Average RMS** (contrast),
**Average Relevant Luminance** (brightness), used to *"obtain segments, create splits, and define
challenges."* Models trained on low motion show a **−2.64 PSNR** drop when tested on high motion;
*"The imbalance in performance shows the sensitivity of current models to the motion magnitudes of
the training data."*

**Phyriad application (designed).** M5 = run the synthetic-motion sweep (M1) across a *graded
motion-magnitude ladder* and report each lever's effect *per stratum*, so a win in low-motion is
not averaged against a loss in high-motion (exactly the failure the 0.59×–3.1× spread embodies).
This is the methodological core that converts "real-game testing is scene-confounded" into "the
testbench controls difficulty as an explicit axis."

---

## §7 — M6 · GPU SATURATION generator: the key gap we pioneer

**This is the one component with NO surviving verified SOTA source.** The research swept
FurMark / 3DMark / Unigine sustained-stress and controllable compute-shader loads but produced
**no surviving claim documenting how to hold a GPU at a PRECISE target utilization
deterministically** for a measurement sweep. It is asserted as a harness need and remains
**unsourced** (workflow `whsdgxiez` caveats + openQuestions, explicit). Phyriad therefore
**pioneers** M6; status is `designed`, and any future citation MUST NOT be fabricated to fill
this gap (FDP §2).

The design constraint is Phyriad-specific and `measured`: our root-caused bottleneck is **not**
raw compute occupancy. The FG slice serializes on the 4090's single graphics engine; the
saturating cost is the **upload on the graphics queue** plus the **synchronous present blocking
on the warp fence** (render-assistant arc, STAGE-104..106; the `--upload-xfer`/`--wsub`
diagnoses). A pure compute load (FurMark, a compute-shader pegging the SMs) can drive the
utilization counter to 99% while **failing to reproduce the graphics-queue + present-path
contention that is our actual bottleneck**. Therefore:

> **M6 design requirement (designed, derived from a measured root-cause).** The saturation
> generator MUST pressure the **graphics queue and the present path**, not (only) compute —
> e.g. a controllable graphics-pipeline + present load whose target is a held graphics-queue
> occupancy and present-fence pressure, verified against PresentMon `GPUBusy`/display pacing,
> NOT merely a compute-occupancy percentage. A compute-only pegger is necessary-but-insufficient
> and would mis-measure the lever under test.

This component is the reason real-game combat is unusable as a controlled condition (the GPU load
co-varies with the scene), and the reason the testbench must *generate* the saturation
deterministically.

---

## §8 — M7 · THE HARNESS as a whole: orchestration, reproducibility, the four-axis discipline

A proper FG/VFI evaluation harness composes the prior components into a repeatable run (synthesis,
confidence medium — no single dedicated source, inferred from the verified parts):

1. a **deterministic source with recoverable ground truth** (M1) — synthetic analytic motion
   preferred, dense held-out for breadth;
2. an **objective scorer** (M2) — FR pixel/perceptual + a no-reference live metric;
3. **latency instrumentation** (M3) — present-side software + external photon hardware;
4. **throughput + pacing instrumentation** (M4) — PresentMon present/display + FrameType;
5. **difficulty stratification** (M5) — motion-magnitude strata;
6. a **controllable saturation load** (M6) — graphics+present pressure to a target;
7. **orchestration** (M7) — a resolution + source-framerate **sweep**, automated telemetry
   capture, and **reproducible A/B** (deterministic camera path / captured-replay).

The deterministic-source substrate for the A/B is the field's pursuit-camera / TestUFO method:
*"Slide the camera sideways … at the same speed as on-screen motion, while pressing the shutter
button"* — known velocity, set resolution/framerate, repeatable (Blur Busters, 3-0;
*"camera-tracking blur equals eye-tracking blur"*). Its application to an FG motion-clarity A/B is
a sound author inference, not a source claim.

**The master pitfall the harness defeats** is conflating the four measurement axes. The single
most common error in FG analysis is reporting a throughput multiplier (M4) as if it were
responsiveness (M3), or a global PSNR (M2) as if it captured localized ghosting. Each axis has a
different instrument, a different ground-truth requirement, and a different "what good looks like."

**Full component → SOTA → Phyriad → enabled-fixed-point map:**

| Concern (M) | SOTA basis (§) | Phyriad anchor / status | Master-plan fixed point it enables |
|---|---|---|---|
| **M1** GT source | §2 (Middlebury, X4K, LAVIB, Kiefhaber) | `fg_quality_scorer` `sequence` mode + `prep_zoo_sequence.py` — `designed`/partly-built | C8 (regression corpus) |
| **M2** quality scorer | §3 (gradient-SSD, FloLPIPS, DIV, TS) | `fg_quality_scorer` PSNR/SSIM/`dbl_edg_m`/`flowdsc`/mode-T — `built` | C8 |
| **M3** latency | §4 (PC-Latency split, LDAT, PresentMon present-side) | DComp overlay → input-to-photon software-unmeasurable; needs photodiode — `designed` | C7 (latency ≤ LSFG + Δ) |
| **M4** throughput+pacing | §5 (Presented/Displayed FPS, FrameType, Animation Error) | PresentMon CSV; the 0.59×–3.1× multiplier is a throughput fraction — `designed` | C6 (pacing), C7 |
| **M5** stratification | §6 (LAVIB four metrics, −2.64 PSNR) | motion-magnitude ladder over the sweep — `designed` | (de-confounds all) |
| **M6** saturation gen | §7 (**unsourced**) | graphics+present pressure per root-cause — `designed`, pioneered | enables clean C7/C6/B1 measurement |
| **M7** orchestration | §8 (pursuit-camera, sweep, replay) | the controlled test environment, task #13 — `designed` | the whole measurement programme |

---

## §9 — Sources (leveled per FDP §2.3)

All external facts below were verified **3-0 against primary sources** in workflow `whsdgxiez`;
they carry **[V1]** (primary, first-hand to the research agents). Per the provenance note, the
*author* did not independently re-fetch these in this session — the verification is the workflow's,
recorded honestly here rather than re-attested falsely.

| # | Source | What it grounds | Level |
|---|---|---|---|
| 1 | Middlebury (Baker et al., ICCV 2007) https://vision.middlebury.edu/flow/flowEval-iccv07.pdf | held-out 100fps GT; synthetic lens-shader GT; gradient-normalized SSD | **[V1]** |
| 2 | XVFI / X4K1000FPS (ICCV 2021) https://arxiv.org/abs/2103.16206 | 4K@1000fps dense GT source | **[V1]** |
| 3 | LAVIB (NeurIPS 2024) https://arxiv.org/html/2406.09754v1 | held-out triplets/septuplets; four-metric difficulty stratification; −2.64 PSNR | **[V1]** |
| 4 | Kiefhaber/Niklaus/Liu/Schaub-Meyer (2024) https://arxiv.org/pdf/2403.17128 | skipped-frame GT violates linearity / "oracle"; strict-linearity synthetic dataset | **[V1]** |
| 5 | FloLPIPS (Danier/Zhang/Bull, ICIP 2022) https://arxiv.org/abs/2207.08119 | bespoke FR VFI metric; best subjective correlation | **[V1]** |
| 6 | DIV — Motion Field Divergence (2025) https://arxiv.org/html/2508.09078v1 | 112.4 ms/frame, ~2.7× faster than FloLPIPS, ~87% corr; no-reference TS | **[V1]** |
| 7 | NVIDIA LDAT https://developer.nvidia.com/nvidia-latency-display-analysis-tool | photodiode click-to-photon, no engine hook, all vendors | **[V1]** |
| 8 | Open-Source-LDAT (Teensy-4.1) https://github.com/S4N-T0S/Open-Source-LDAT | affordable photon-latency clone (poor-man's hardware) | **[V3]** |
| 9 | NVIDIA "Understanding and Measuring PC Latency" https://developer.nvidia.com/blog/understanding-and-measuring-pc-latency/ | I2FS+FS2P+P2D; I2FS needs PCLStatsInput ETW + SIMULATION_START | **[V1]** |
| 10 | PresentMon `README-CaptureApplication.md` https://github.com/GameTechDev/PresentMon/blob/main/README-CaptureApplication.md | Presented vs Displayed FPS; Until Displayed; Display Latency | **[V1]** |
| 11 | PresentMon releases https://github.com/GameTechDev/PresentMon/releases | FrameType; Displayed-FPS counts generated frames; reintroduced CSV pacing metrics | **[V1]** |
| 12 | GamersNexus — Animation Error https://gamersnexus.net/gpus-cpus-deep-dive/fps-benchmarks-are-flawed-introducing-animation-error-engineering-discussion | Animation Error definition; FPS-decoupled-from-pacing | **[V3]** |
| 13 | Blur Busters — Pursuit Camera https://blurbusters.com/motion-tests/pursuit-camera/ | deterministic known-velocity synthetic-motion A/B substrate | **[V1]** |

In-repo (read first-hand this session): `fg_quality_scorer/README.md` (the scorer's modes,
metrics, and the C8 regression-gate section); [`PHYRIADFG_MASTER_PLAN.md`](PHYRIADFG_MASTER_PLAN.md)
and [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md) (the C1–C10
fixed-point namespace and the DComp-attribution probe caveat).

---

## §10 — What could NOT be verified, open questions, and reconciliation items

Honest gaps (FDP §2.4 / D6):

1. **M6 saturation generator is unsourced.** No surviving source documents holding a GPU at a
   precise target utilization deterministically; the resolution/framerate sweep methodology and the
   reviewer captured-replay/deterministic-camera-path reproducibility (DLSS-G vs FSR3 vs LSFG) also
   produced no surviving claim. Phyriad pioneers M6; its graphics+present-pressure requirement is a
   `designed` inference from a `measured` root-cause (render-assistant arc), not a literature fact.
2. **VMAF** was named in the original question but had **no surviving verified claim** for FG/VFI
   quality or pacing — its applicability is unestablished here.
3. **DIV / TS** are novel, single-dataset (BVI-VFI, 180 seqs), community-unvalidated; DIV absolute
   PLCC ~0.51; FloLPIPS's "12 assessors" is a self-evaluation. Treat absolute SROCC as dataset-fragile.
4. **PresentMon attribution through a DComp overlay** (the external-capture present path) is
   unconfirmed — probe before declaring the latency fixed point turnkey.
5. **Cross-clock present-timestamp error magnitudes and EMA/measurement-window artifacts** were
   flagged as pitfalls but not quantified by any surviving claim.
6. **PresentMon is version-volatile** ("for the Frame Generation age": 2.0 removed then 2.3.1
   reintroduced legacy CSV names; Animation Error enhanced through 2.5.0) — verify metric names
   against the installed version.
7. **The author did not re-fetch the §9 external URLs first-hand this session** — they carry the
   research workflow's 3-0 levels, stated rather than re-attested.

Reconciliation / follow-up items (process):

- **Namespace (§0) — three distinct ID schemes.** This dossier's **M1–M7 are *measurement
  concerns***, DISTINCT from the **build components `TB-C1..TB-C8`** in `FG_TESTBENCH_MASTER_PLAN.md`
  and the **closeable fixed points C1–C10** in `PHYRIADFG_MASTER_PLAN.md` / the objective vista; read
  every bare `C<n>` here as a master-plan fixed point.
- **Companion plan — AUTHORED.** Per [`PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md),
  building this testbench is **Tier-2** (it adds a controllable GPU-saturation load and a present-path
  rig — crash/device-loss-adjacent and dogma-touching), so it requires a master-plan +
  implementation-strategies + risk-register, mutually linked, with no commit while any risk is `open`.
  **Those three now exist** (authored this session): [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md)
  + [`FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md`](FG_TESTBENCH_IMPLEMENTATION_STRATEGIES.md)
  + [`FG_TESTBENCH_RISK_REGISTER.md`](FG_TESTBENCH_RISK_REGISTER.md); this dossier is their SOTA input.
- **Registration — DONE.** This dossier is registered in
  [`FORMAL_DOCUMENTS_REGISTER.md`](../FORMAL_DOCUMENTS_REGISTER.md) (Analysis/SOTA dossier).

## §11 — Cross-links

Built scorer + the measurement-loop framing → [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md).
The quality-metric ceiling → `FG_VFI_PRIOR_ART.md`. Latency/pacing floors + the DWM clock →
`FG_CADENCE_LATENCY_PRIOR_ART.md`. The objective spine these feed →
[`PHYRIADFG_MASTER_PLAN.md`](PHYRIADFG_MASTER_PLAN.md) / [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md).
