# FG_VFI_PRIOR_ART.md — state-of-the-art dossier (frame-generation / video-frame-interpolation quality: measurement + the ghosting cure)

Status: `0.1.0-experimental` · Type: **Analysis** (honest explanation / prior-art
dossier, per [FDP §3](../canon/FORMAL_DOCUMENT_PROTOCOL.md)). §1–§8 honest as of
access date **2026-06-13**; **§9** (surgical crossfade fix) added **2026-06-14**;
**§10** (image-boundary-gated disocclusion) and **§11** (multi-candidate generation
& selection without ground truth) added **2026-06-15** — each from a further
deep-research-harness pass with its load-bearing citations re-fetched first-hand
per the §2 mandate.

**Scope.** This dossier consolidates the published state of the art for two
questions an *external-capture* frame generator (a VFI tool that sees only the
rendered output — **no** engine motion vectors, depth, or G-buffer) must answer:

1. **Measurement** — how to score VFI/FG quality *objectively and reproducibly*
   over a recorded segment, replacing manual slow-motion frame-by-frame
   eyeballing.
2. **Generation** — what the *ghosting / double-image / crossfade* artifact is,
   its root cause, and the published cure family.

**Non-scope.** This is a *prior-art record*, not an implementation. It does not
describe, modify, or claim anything about the render_assistant code; it links to
what Phyriad already BUILT (the `fg_quality_scorer` and HSR120 §11.7) as the
single authoritative home of those facts (FDP §4.6), and does not re-describe
them. It does not establish any new measured number; the only quantities it
states are the cited sources' own.

**Method.** Three deep-research-harness passes (fan-out web search → fetch →
adversarial 3-vote verification → synthesize): one on VFI-quality measurement
(20 sources, 91 claims, 25 verified, 22 confirmed / 3 refuted) and one on the
ghosting root-cause + cure (18 sources, 82 claims, 25 verified, 21 confirmed /
4 refuted) — both consolidated into §1–§8 on 2026-06-13 — and a third, on the
**surgical (non-neural, real-time) fix for the now-identified crossfade** (5
angles, 21 sources, 100 claims, 25 verified, 25 confirmed / 0 refuted), added as
**§9 on 2026-06-14**. **The harness verification is NOT this dossier's
verification.** Per the FDP verifiable-reference mandate (§2), every load-bearing
external reference below was re-fetched first-hand and carries a level
**[V1]/[V2]/[V3]** (§2.3) in §7; what could not be confirmed first-hand is stated
as such in §8 (§2.4).

**Companions / link targets** (the single authoritative home of each fact — this
dossier links, does not copy):
- [`HSR120_THROUGHPUT_DIAGNOSIS.md` §11.7](ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md)
  — the FG-quality test-field blueprint and the operator-side findings this prior
  art grounds.
- [`fg_quality_scorer/README.md`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md)
  — the instrument Phyriad BUILT (the metric definitions: `dbl_edg_m`, `flowdsc`,
  the continuous held-out `sequence` mode, truth-less mode T).

---

## §1 — Verdict (the synthesis)

- **The measurement recipe is well-defined and adoptable.** Ground truth comes
  free from the **held-out-frame protocol**: decimate a deterministic high-rate
  source, reconstruct the dropped intermediate frame, compare to the withheld
  real. This is the canonical full-reference VFI protocol behind Middlebury,
  Vimeo90K, UCF101, Adobe240, and the MSU VFI Benchmark (F1, F2).
- **Global PSNR/SSIM are the WRONG instrument for perceived VFI quality.**
  Multiple independent peer-reviewed studies (FloLPIPS, VFIPS, BVI-VFI,
  VFIPQA) establish that classic full-reference fidelity metrics correlate
  poorly with human judgment of interpolation artifacts, because VFI distortions
  — ghosting, double-image, object deformation — are *temporal and localized to
  the moving region*, where a global average dilutes them against the static
  background (F3). The robust takeaway is the **ordering** (PSNR/SSIM ≪
  LPIPS/DISTS ≪ bespoke VFI metrics), not any single SROCC number, which is
  dataset-dependent (§8).
- **The measurement fix is flow-weighted / motion-masked metrics.** **FloLPIPS**
  weights the perceptual feature map by the per-pixel optical-flow discrepancy
  between reference and output — concentrating the score where the interpolator's
  motion is *wrong* (F4). This is the published mechanism the operator's
  motion-masked instinct independently reached.
- **The ghosting artifact has a canonical name and a named root cause.** The
  academic VFI literature calls the "object at two places with a half-intensity
  crescent" artifact **ghosting** (a.k.a. double-image); its root cause is
  **symmetric blending of misaligned samples** — averaging two endpoint frames
  with equal contribution when motion estimation has failed (textureless
  interiors where the aperture problem starves block-matching of gradient),
  producing the half-intensity double-edge (F5, F6).
- **The cure family is "always warp a single coherent surface, never raw-blend."**
  Three published sub-families: (a) **forward-splat with an importance/occlusion
  mask** (softmax splatting, Niklaus & Liu) advects pixels along the flow and
  z-buffers same-target collisions instead of averaging (F7); (b) **warp-then-
  soft-mask** dense-flow estimators (RIFE/IFNet, FILM, EMA-VFI) backward-warp both
  frames and fuse with a *spatially-varying* mask, not a static alpha cross-
  dissolve (F8, F9, F10); (c) **commit to one source** — asymmetric blending
  (PerVFI) (F6). All convert flow error into *smooth deformation* rather than a
  double-edge.
- **THE GENUINE OPEN PROBLEM.** Dense flow produces *a* vector in a flat interior,
  but whether it is the *correct* interior motion or merely a *plausible* one is
  unresolved — a smoothness prior fills a plausible vector, not necessarily the
  true one. So "always-warp" trades the safe double-edge for a graceful
  deformation that may be the *wrong* deformation (a wrong-direction stretch — the
  "gravity" look). No surviving claim provides a controlled perceptual study that
  viewers prefer warp-deformation over blend-ghost; it is the universal *design
  choice* of every SOTA method, not a measured human preference (§5, §8).
- **Proprietary FG internals are UNVERIFIED.** No surviving claim establishes the
  flow+warp internals of Lossless Scaling (LSFG), DLSS Frame Generation, or FSR3.
  Any statement that "LSFG always warps and never blends" is a **consistent
  inference** from the published academic cure family and the artifact's visible
  character — **not** a cited fact (§8). These are closed-source.

---

## §2 — Measurement findings (each with its sources + verification level)

**Level legend** (FDP §2.3): **[V1]** primary, first-hand (source retrieved and
fact confirmed this session); **[V2]** primary, partial (source reached but the
detail is paper-body / a figure not transcribed — gap stated); **[V3]** secondary.
The harness's 3-vote tally is shown as context (`3-0` = unanimous survival), but
the *level* is this dossier's own first-hand check.

### F1 — The held-out-frame protocol is the canonical full-reference VFI ground-truth method `[harness 3-0]`
Decimate a high-fps source, synthesize the dropped intermediate frame at time
*t* ∈ (0,1), compare pixel-wise to the withheld truth. The canonical benchmark
suite is **Middlebury** (Interpolation Error / Normalized IE), **Vimeo90K** and
**UCF101** (PSNR/SSIM), and **Adobe240** (×2/×4/×8 high-fps tests). This is
exactly the decimate-and-reconstruct trick a deterministic source supplies for
free. Level **[V2]** — the protocol and benchmark roster are corroborated across
the harness's primary sources (BMBC/ECCV2020, the MSU methodology page); this
dossier did not re-fetch each benchmark's tooling page first-hand, so it cites the
*protocol*, not per-benchmark specifics, as primary.

### F2 — The MSU VFI Benchmark operationalizes it at 1080p with a 5-metric stack `[harness 3-0]`
The MSU Graphics & Media Lab benchmark interpolates one frame between an adjacent
pair at 1920×1080 and scores it with **PSNR, SSIM, MS-SSIM, VMAF, LPIPS**,
anchored to a 413-participant Bradley-Terry pairwise human study. A directly
adoptable multi-metric template. Level **[V2]** — the source is a first-party but
non-peer-reviewed methodology page; it was reached by the harness, **not re-fetched
first-hand** here (§8). Cite the 5-metric *stack idea*, not the exact study
parameters, as load-bearing.

### F3 — Global PSNR/SSIM correlate poorly with perceived VFI quality; the artifact is localized `[harness 3-0, 5 independent sources]`
VFI produces *temporal + localized* distortions (ghosting, double-image, object
deformation) that per-pixel metrics miss and per-frame scoring discards. Four
independent peer-reviewed sources converge:
- **FloLPIPS** (PCS 2022): PSNR/SSIM/LPIPS "exhibit poor correlation with
  subjective opinion scores for frame interpolated videos." Level **[V1]** — title
  and the flow-weighting mechanism confirmed first-hand in the abstract.
- **BVI-VFI** (IEEE TIP 2023): benchmarks 33 objective metrics against 10,800+
  human ratings and concludes there is an "urgent requirement for more accurate
  bespoke quality assessment methods for VFI." Level **[V1]** (supervisor-fetched
  2026-06-13; re-confirmed via the shared §11.7 anchor — see §7).
- **VFIPS** (the VFI-specific Swin spatio-temporal metric): per-frame application
  "ignores the temporal information." Level **[V1]** for the VFI-specific /
  temporal claim; the specific *ghosting / object-deformation* example quotes are
  **[V2]** (paper-body, not in the fetched abstract).
- **VFIPQA**: a no-reference triplet net; its results table reports PSNR SROCC
  collapsing to ≈0.13 on one VFI-MOS set. Level **[V2]** — the SROCC≈0.13 figure
  is in the PDF's results table, **not author-verified** (harness-transcribed) and
  is **dataset-fragile** (§8); do **not** present it as [V1].

**Honest nuance the harness flagged.** PSNR/SSIM remain *valid full-reference
baselines* on held-out frames; they are *insufficient / suboptimal for perceptual
VFI ranking*, not literally worthless. The robust design driver is the **ordering**
(PSNR/SSIM ≪ LPIPS/DISTS ≪ bespoke), not the absolute SROCC magnitude.

### F4 — The fix is flow-weighted / motion-masked metrics (FloLPIPS) `[harness 3-0]`
**FloLPIPS** weights the per-pixel LPIPS feature-difference map by a normalized
map of the **optical-flow discrepancy** between reference and distorted video,
concentrating the score on regions where the interpolator's motion is wrong (where
ghosting/deformation live) instead of averaging every pixel equally. This is the
exact motion-masked mechanism the external-capture measurement problem needs.
Level **[V1]** — title and the flow-discrepancy-weighting mechanism confirmed
first-hand in the abstract (the full Eq. 4–6 derivation is paper-body **[V2]**).
> Cross-link: Phyriad's `flowdsc` adopts this mechanism with the pipeline's own MV
> field — see the [scorer README](../../framework/render/vulkan/bench/fg_quality_scorer/README.md)
> (FLOW-DISCREPANCY section). The honest difference (motion-*presence* vs
> flow-*discrepancy* weighting) is recorded there, not here.

### F5 — No-reference temporal metrics exist for the live (ground-truth-free) case, but are weaker `[harness 3-0, with a refuted sub-claim — see §4]`
For the live output with no held-out truth, blind temporal-stability metrics
motion-compensate (warp along flow) before differencing: **E_warp** (Lai et al.'s
flow warping error), **Temporal Smoothness (TS)**, **Motion Field Divergence
(DIV)**. They localize occlusion/heavy-warp artifacts cheaply. **Critical caveat
(harness-refuted, 3×):** the claim that flow-warping *fully cancels motion
magnitude* so the residual reflects only temporal inconsistency was **REFUTED** —
the occlusion mask removes disocclusion confounds but motion-magnitude
independence is **not proven**. Treat E_warp/DIV as artifact *localizers/flags*,
**not** motion-decoupled quality scores. Level **[V2]** — primary sources reached
by the harness; not re-fetched first-hand here.

### F6 — A no-reference VFI-specific model is viable for live monitoring, but held-out ground truth stays the arbiter `[harness 3-0]`
**VFIPQA** (a ResNet50 triplet net taking the interpolated frame + its two
neighbors) predicts human MOS without a high-fps reference and beats FR/NR
baselines *on its own dataset* — viable as a live monitor, but its strong results
are in-distribution, not cross-dataset validated. **Held-out full-reference
remains mandatory whenever a validated, trustworthy quality number is required.**
Level **[V2]** (title + no-reference triplet design confirmed; results table
**[V2]**, §8).

### F7 — A reusable human-perception baseline exists: BVI-VFI `[harness 3-0]`
**BVI-VFI** — 540 distorted sequences (5 VFI algorithms × 36 diverse sources),
10,800+ ratings from 189 subjects, 33 objective metrics already benchmarked
against the scores — is the dataset to calibrate/select a metric against before
trusting it. **Caveat:** it covers VFI-*network* artifacts, **not** game-engine FG
artifacts (HUD/UI ghosting), so it validates metric-vs-human correlation for
interpolation generally, not the specific FG-tool failure modes. Level **[V1]**
(see F3 / §7).

---

## §3 — Generation findings (root cause + the cure family)

### F8 — The artifact's canonical name is GHOSTING; root cause is symmetric blending of misaligned samples `[harness 3-0]`
The VFI literature and the GPU-FG community call the "object at two places with a
half-intensity crescent" artifact **ghosting** (a.k.a. double-image). **RIFE**
contrasts flow-based methods' "ghosting artifacts" with flow-free methods'
"missing-parts" artifacts (paper-body, Fig. 7 / §4.2 — **[V2]**). The named root
cause comes from **PerVFI**: "in cases of inaccurate motion estimation, the network
struggles to discern the correct frame [and] often produce[s] blurred and ghosted
results by averaging multiple frames" — i.e. **symmetric blending** of two endpoint
frames with equal contribution even when they are misaligned, which in textureless
interiors and occlusions (the aperture problem) is exactly the half-intensity
double-image. Level: PerVFI title, venue (CVPR 2024), the "blur and ghosting"
manifestation, and the asymmetric-blending remedy are **[V1]** (confirmed
first-hand in the abstract); the *symmetric-blending-as-root-cause* sentence and
the RIFE "ghosting" quote are **paper-body [V2]**.

### F9 — Forward warping via softmax splatting moves the object as ONE surface (Niklaus & Liu) `[harness 3-0]`
**Softmax Splatting** (Niklaus & Liu, CVPR 2020) forward-warps source pixels along
the flow to their destination at the intermediate time — and resolves the central
problem that *multiple source pixels can map to the same target* via a per-pixel
importance mask (a large weight yields hard z-buffering so a foreground object
cleanly occludes; a small weight yields averaging). This is the canonical
"single coherent displaced surface, not a blend" mechanism. Level **[V1]** — the
abstract confirms first-hand "we forward-warp the frames … using softmax splatting"
and "the softmax splatting seamlessly handles cases where multiple source pixels
map to the same target location." The Eq. 13 z-buffering derivation is paper-body
**[V2]**.

### F10 — RIFE embodies "always-warp, never raw-blend" via warp-then-soft-mask, in real time `[harness 3-0]`
**RIFE** (Huang et al., ECCV 2022) — full title *"Real-Time Intermediate Flow
Estimation for Video Frame Interpolation"* — uses **IFNet** to estimate the
intermediate flows end-to-end (no pre-trained external flow model), producing dense
flow even in flat regions, then synthesizes by **warp-then-fuse**: backward-warp
both inputs to the intermediate time and combine with a *learned, spatially-varying
soft mask* M — **not** a uniform alpha cross-dissolve of un-warped endpoints. It is
**4–27× faster** than prior flow-based VFI (SuperSloMo, DAIN). Level: title,
authors, venue, the end-to-end IFNet claim, and the 4–27× speed claim are **[V1]**
(confirmed first-hand in the abstract); the warp-then-soft-mask `I_t = M·I_{t←0} +
(1−M)·I_{t←1}` synthesis form is **paper-body [V2]**.
> Honest nuance: M is a *spatially-varying confidence* blend — not *zero* blending
> — but it is motion-compensated *post-warp* fusion, the opposite of a raw
> cross-dissolve of un-aligned endpoints.

### F11 — FILM is frames-only and applies directly to the external-capture constraint `[harness 3-0]`
**FILM** (Reda et al., Google Research, ECCV 2022) — *"FILM: Frame Interpolation
for Large Motion"* — is a single unified network **trainable from frames alone**
(no GT optical flow or depth network), inferring motion internally from only the
two RGB inputs. Its large-motion mechanism is a multi-scale feature extractor that
**shares weights across scales**; it synthesizes by **backward-warping** the feature
pyramids into alignment and fusing via a U-Net decoder — **NOT** forward/softmax
splatting. This matters for an external-capture tool: it needs only rendered RGB
frames. Level: title, authors, venue, and the **trainable-from-frames-alone** claim
are **[V1]** (abstract, first-hand); the *scale-agnostic* / coarse-to-fine framing
and the backward-warp-then-fuse synthesis detail are **paper-body [V2]** (the
abstract says "shares weights at all scales" but does not use "scale-agnostic" nor
spell out the warp direction).

### F12 — EMA-VFI derives motion from inter-frame attention, not block-matching `[harness 3-0]`
**EMA-VFI** (Zhang et al., CVPR 2023) — *"Extracting Motion and Appearance via
Inter-Frame Attention for Efficient Video Frame Interpolation"* — reuses an
inter-frame attention map for both appearance enhancement and motion extraction.
Relevant because attention-derived motion does not depend on a trackable local
gradient the way block-matching does — the regime where block-matching fails
(textureless interiors). Level: title, authors, venue, and the attention-for-motion
claim are **[V1]** (abstract, first-hand); the harness's stronger phrasing that
motion is derived from *correlation in the attention map* is **[V2]** — the abstract
says "reuse its attention map for … motion information extraction," it does not
spell out "correlation." The paper does **not** claim to *solve* the aperture
problem (appropriately hedged).

### F13 — The cure-family synthesis for an external-capture FG tool `[harness synthesis of 3-0 claims]`
For a tool that already block-matches + confidence-gates a warp and falls back to a
blend, the SOTA implies two levers, in order: (a) **eliminate the blend fallback**
in favor of an always-warp commitment — forward-splat with an importance/occlusion
mask (F9) or warp-then-soft-mask (F10) — so flow error becomes *smooth deformation*
rather than a double-edge; (b) **infill textureless interiors** with a dense
edge-aware / smoothness-prior flow so the flat object inherits a single coherent
motion vector instead of triggering the blend. FILM and RIFE are *directly
applicable* because they require only rendered frames. **The cure is the universal
design stance of every method surveyed.** Level — synthesis; rests on the [V1]/[V2]
findings above, **not** an independent source.

---

## §4 — Refuted claims (excluded — kept visible so we don't believe them)

These were killed by the harness's adversarial pass *or* corrected by a
supervisor first-hand fetch. Kept per the FDP open-conjecture practice.

1. **"PerVFI uses softmax splatting" / "PerVFI's fix is to base output on a single
   frame"** — the harness's two sub-claims here were **refuted (1-2)** *and the
   harness MIS-LABELED the paper*. The supervisor's first-hand fetch
   (re-confirmed here, §7) shows the real title is *"Perception-Oriented Video
   Frame Interpolation via **Asymmetric** Blending"* and the abstract **does**
   support asymmetric blending as the remedy. **This is a worked example of FDP
   §2.2:** the harness erroneously refuted a claim its own mis-labeling created;
   the burden is on the *citing author* to verify first-hand. The corrected fact:
   PerVFI's named root causes are "motion errors and misalignment in supervision,"
   and its remedy is asymmetric (synergistic) blending.
2. **"FILM's Gram-matrix style loss is the SOTA fix for the double-image"** —
   **REFUTED (0-3)**. FILM's Gram-matrix loss improves sharpness/realism; it is not
   the published mechanism that resolves the *blend → double-edge* problem (that is
   warp-then-fuse, F11).
3. **"FILM reports SoftSplat exhibiting visible smooth DEFORMATION of the toy
   car"** — **REFUTED (1-2)**. Do not attribute a specific warp-vs-blend
   head-to-head visual to FILM.
4. **"No-reference VSFA scores SROCC 0.108 … so full-reference is far more
   reliable"** — **REFUTED (0-3)** as stated; the *general* "NR is weaker than FR
   for VFI" stance survives (F6), but this specific number/argument did not.
5. **"Flow-warping fully cancels real motion so the residual reflects only
   inconsistency"** — **REFUTED (0-3 / 1-2 across 3 sub-claims)**. The occlusion
   mask removes disocclusion confounds; motion-magnitude independence is **not
   proven** (this is *why* E_warp/`dbl_edg_m`/`flowdsc` stay flags, not verdicts).

---

## §5 — Open problems (the research could NOT close — these bound the honest claim)

1. **Correct vs plausible flat-interior motion.** In a genuinely textureless
   interior, the aperture problem gives no true interior motion. Dense
   smoothness-prior / neural flows produce *a* vector there, but whether it is
   **correct** or merely **plausible** is unresolved — and there may be a regime
   where always-warp of a flat object yields a *worse* artifact (wrong-direction
   stretch / "gravity" look) than a conservative blend. This is a genuine open
   problem, not a solved one.
2. **No perceptual study confirms warp > blend.** No surviving claim provides a
   controlled human study that viewers prefer the graceful-deformation (warp) look
   over the safe-ghost (blend) look, nor at what flow-error magnitude the
   deformation itself becomes objectionable. The "warp is better" premise rests on
   *SOTA design convention*, not a cited human study.
3. **Proprietary FG internals are unknown.** Whether LSFG / DLSS-FG / FSR3 always
   warp and never visibly blend is **not publicly documented**; the research found
   **no surviving claim** on their flow+warp internals. "LSFG always-warps" is a
   *consistent inference* from the academic cure family and the artifact's visible
   character — labelled as an inference, never a cited fact.
4. **No published automated FG-tool-specific artifact detector.** There is no
   published, automated, region-masked detector for FG-tool failure modes (HUD/UI
   ghosting, disocclusion); reviewers of DLSS-FG/FSR3/LSFG still rely on subjective
   frame-by-frame inspection. This is the genuine SOTA gap the Phyriad per-object
   `obj_iou`/`nonrigid` heuristics make a modest, honest contribution toward (see
   the scorer README and §11.7 — *not re-described here*).

---

## §6 — What this grounds (cross-links, single source of truth)

This dossier is the prior-art record; the *instrument* and the *operator findings*
it grounds live elsewhere and are **not** duplicated here (FDP §4.6):

| Prior-art finding (here) | Where Phyriad acts on it (the authoritative home) |
|---|---|
| Held-out-frame protocol (F1) | `sequence` mode + `prep_zoo_sequence.py` — [scorer README](../../framework/render/vulkan/bench/fg_quality_scorer/README.md) / [§11.7](ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md) |
| Global metrics dilute the localized artifact (F3) | `dbl_edg_m` (motion-masked double-edge) — [scorer README](../../framework/render/vulkan/bench/fg_quality_scorer/README.md) |
| Flow-weighted scoring (FloLPIPS, F4) | `flowdsc` (flow-discrepancy, the FloLPIPS-analogue) — [scorer README](../../framework/render/vulkan/bench/fg_quality_scorer/README.md) |
| No-reference flags for the live case (F5) | truth-less **mode T** (`crossfade_residual` + `double_edge_energy`) — [scorer README](../../framework/render/vulkan/bench/fg_quality_scorer/README.md) |
| Warp-vs-blend cure family (F8–F13) | the operator's flow+warp pipeline + the `nonrigid`/`obj_iou` deformation metrics — [§11.7](ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md) |

---

## §7 — Full external references (with verification levels — FDP §2.3; access date 2026-06-13)

Every external reference carries a resolvable URL, title + author, the access
date, and a level. **[V1]** items were retrieved and the cited fact confirmed
first-hand in this work (the fetch is the attestation; FDP §4.8 — stated once
here, not re-attested per mention). Where a fetch was performed by the supervisor
on 2026-06-13 and re-confirmed via the shared §11.7 anchor, that provenance is
noted. **[V2]** = source reached, detail is paper-body / a results table not
author-verified.

| Ref | Title (author, venue) | Level | Who fetched first-hand | URL |
|---|---|---|---|---|
| **FloLPIPS** | "FloLPIPS: A Bespoke Video Quality Metric for Frame Interpolation" (Danier, Zhang, Bull; PCS 2022) | **[V1]** (title + flow-discrepancy-weighting mechanism, abstract) | supervisor 2026-06-13; re-anchored §11.7 | https://arxiv.org/abs/2207.08119 |
| **BVI-VFI** | "BVI-VFI: A Video Quality Database for Video Frame Interpolation" (Danier, Zhang, Bull; IEEE TIP 2023) | **[V1]** (540 seq / 36 src / 189 subjects / 10,800+ ratings / 33 metrics / "bespoke QA needed") | supervisor 2026-06-13; re-anchored §11.7 | https://arxiv.org/abs/2210.00823 |
| **VFIPS** | "A Perceptual Quality Metric for Video Frame Interpolation" (Hou, Ghildyal, Liu; ECCV 2022) | **[V1]** for VFI-specific / temporal; the *ghosting/object-deformation* quotes **[V2]** (paper-body) | supervisor 2026-06-13; re-anchored §11.7 | https://arxiv.org/abs/2210.01879 |
| **VFIPQA** | "Perceptual Quality Assessment for Video Frame Interpolation" (Han et al.; 2023) | **[V2]** (title + no-reference triplet net; the SROCC≈0.13-for-PSNR figure is a results-table value, **not** author-verified — harness-transcribed) | supervisor 2026-06-13 (abstract) | https://arxiv.org/abs/2312.15659 |
| **Softmax Splatting** | "Softmax Splatting for Video Frame Interpolation" (Niklaus, Liu; CVPR 2020) | **[V1]** (forward-warp + "multiple source pixels map to the same target", abstract) | **this session 2026-06-13** | https://arxiv.org/abs/2003.05534 |
| **PerVFI** | "Perception-Oriented Video Frame Interpolation via **Asymmetric** Blending" (Wu, Tao, Li, Wang, Liu, Zheng; CVPR 2024) | **[V1]** (title-corrected; blur/ghosting manifestation, asymmetric-blending remedy, root causes "motion errors + misalignment in supervision" — abstract); symmetric-blending-as-root-cause sentence **[V2]** (paper-body) | **this session 2026-06-13** | https://arxiv.org/abs/2404.06692 |
| **RIFE** | "Real-Time Intermediate Flow Estimation for Video Frame Interpolation" (Huang, Zhang, Heng, Shi, Zhou; ECCV 2022) | **[V1]** (title, authors, venue, end-to-end IFNet, 4–27× faster — abstract); "ghosting artifacts" quote + warp-then-soft-mask synthesis **[V2]** (paper-body) | **this session 2026-06-13** | https://arxiv.org/abs/2011.06294 |
| **FILM** | "FILM: Frame Interpolation for Large Motion" (Reda, Kontkanen, Tabellion, Sun, Pantofaru, Curless; ECCV 2022) | **[V1]** (title, authors, venue, **trainable from frames alone** — abstract); scale-agnostic framing + backward-warp-then-fuse synthesis **[V2]** (paper-body) | **this session 2026-06-13** | https://arxiv.org/abs/2202.04901 |
| **EMA-VFI** | "Extracting Motion and Appearance via Inter-Frame Attention for Efficient Video Frame Interpolation" (Zhang, Zhu, Wang, Chen, Wu, Wang; CVPR 2023) | **[V1]** (title, authors, venue, attention-for-motion — abstract); the "correlation in the attention map" phrasing **[V2]** (paper-body) | **this session 2026-06-13** | https://arxiv.org/abs/2303.00440 |
| **MSU VFI Benchmark** | MSU Graphics & Media Lab — VFI benchmark methodology (5-metric @1080p stack) | **[V2]** (first-party but non-peer-reviewed page; reached by the harness, **not re-fetched first-hand**) | harness only | https://videoprocessing.ai/benchmarks/video-frame-interpolation-methodology.html |
| **E_warp** | "Learning Blind Video Temporal Consistency" (Lai et al.; ECCV 2018) | **[V2]** (flow-warping-error metric; reached by the harness, not re-fetched first-hand; motion-decoupling sub-claims **refuted**, §4) | harness only | https://openaccess.thecvf.com/content_ECCV_2018/papers/Wei-Sheng_Lai_Real-Time_Blind_Video_ECCV_2018_paper.pdf |
| **3DRS-mobile (Pohl 2018)** | "Real-time 3DRS motion estimation for frame-rate conversion" (Pohl, Anisimovskiy, Kovliga, Gruzdev, Arzumanyan; Samsung R&D Russia; IS&T Electronic Imaging 2018) | **[V1]** (PDF read first-hand: candidate set `CS(X⃗)={CS_spatial, CS_temporal, CS_random}` verbatim; FullHD 15→30 fps at **<500 mA** on a Galaxy S8 / MSM8998, no ASIC; future-work lists **flat areas / periodic patterns** + **occlusion detection and handling** as unsolved — all verbatim) | **this session 2026-06-14** | https://library.imaging.org/admin/apis/public/api/ist/website/downloadArticle/ei/30/13/art00014 |
| **3DRS (de Haan 1993)** | "True-Motion Estimation with 3-D Recursive Search Block Matching" (de Haan, Biezen, Huijgen, Ojo; IEEE Trans. CSVT 3(5), Oct 1993, pp. 368–379; DOI 10.1109/76.246088) | **[V2]** (bibliographic + "only eight candidate vectors per block" confirmed via the TU/e portal + search summary; the smoothness-via-penalty / spatial-candidate-preference mechanism is **paper-body, paywalled, not first-hand-read** — §8) | this session 2026-06-14 (abstract/portal only) | https://research.tue.nl/en/publications/true-motion-estimation-with-3-d-recursive-search-block-matching/ |
| **RU2656785C1** | "Motion estimation through 3DRS in real time for FRC" (Samsung; Google Patents EN translation) | **[V1]** (penalty function `f(Prior, MVcand)` regularizes the MV field toward semi-global MVs so flat blocks where MAD ties across candidates inherit a coherent SGMV; spatial + temporal + random candidates — all confirmed first-hand) | **this session 2026-06-14** | https://patents.google.com/patent/RU2656785C1/en |
| **MSR-Asia low-complexity MCFI** | "A Low Complexity Motion Compensated Frame Interpolation Method" (Zhai, Yu, Li, Li; Microsoft Research Asia; ISCAS 2005) | **[V1]** (PDF read first-hand: selective vector-median rule — "If the variation exceeds a certain threshold, the motion vector is regarded as a single bad motion vector and then vector median filtering is applied" verbatim; SAD+BAD confidence gate; re-estimation fractions 2.0%–63.0% — Foreman 24.2%, Salesman 2.7%, Mobile 63.0%, Clair 2.0% — verbatim; input MVs are **bit-stream-embedded H.264 MVs, NOT RGB-derived**) | **this session 2026-06-14** | https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/lowcomplexitymc.pdf |
| **US9148622B2** | "Frame rate up-conversion" occlusion-handling patent (Google Patents) | **[V1]** (covered region built from the **prior frame only**, uncovered from the **next frame only**, verbatim; cover = only forward MVs intersect, uncover = only backward MVs intersect — confirmed first-hand) | **this session 2026-06-14** | https://patents.google.com/patent/US9148622B2/en |
| **Çiğla & Alatan** | "Occlusion Adaptive Frame Rate Up-Conversion" (Çiğla, Alatan; academia.edu mirror) | **[V1]** (forward-3DRS reliable on uncover / backward on cover; Table 1 fill rule — uncover=fwd-only MC, cover=bwd-only MC, both-valid=bidirectional, neither-valid="simple averaging without motion"; OBMC keeps weights on nullified erroneous MVs at minimum, "halo artifacts … negligibly noticeable" — all confirmed first-hand) | **this session 2026-06-14** | https://www.academia.edu/974176/Occlusion_Adaptive_Frame_Rate_Up_Conversion |
| **Dane & Nguyen** | "Occlusion Aware Motion Compensation for Video Frame Rate Up-Conversion" (Dane, Nguyen; academia.edu mirror) | **[V1]** (sign rule Eq. 10 "uncover if dVx≥Th or dVy≥Th; cover if dVx≤−Th or dVy≤−Th"; ternary occlusion map K∈{0,0.5,1}; Eq. 14 collapse-to-single-source "if BP D_{m,n}≤ε₄ then w_{k*}=1, w_k=0 for k≠k*" — confirmed first-hand); the "contrasted symmetric blend is 0.5/0.5" framing **could NOT be confirmed** (§8) | **this session 2026-06-14** | https://www.academia.edu/1440576/Occlusion_Aware_Motion_Compensation_for_Video_Frame_Rate_Up_Conversion |
| **PerVFI (paper body)** | PerVFI CVPR 2024 — paper body (CVF open-access PDF; the abstract is the row above) | **[V1] for the paper-body items used in §9** (symmetric-blending root-cause sentence "use symmetric blending, which easily blends the features from two sides with equal contribution, even when they are misaligned" verbatim; ASB "one reference frame emphasizes primary content, while the other contributes complementary information"; quasi-binary mask; Table 3 ablation on **DAVIS (480P)** — Adaptive/symmetric **27.61 PSNR / 78.20 VFIPS** vs Quasi-B./asymmetric **27.16 / 83.30**; Fig. 6 "fully-adaptive mask tends to center around 0.5, signifying an equal contribution" — all read first-hand from the PDF) | **this session 2026-06-14** | https://openaccess.thecvf.com/content/CVPR2024/papers/Wu_Perception-Oriented_Video_Frame_Interpolation_via_Asymmetric_Blending_CVPR_2024_paper.pdf |
| **US20120033130** (§10 pass) | "Detecting occlusion" MC-FRC patent (Dynamic Data Technologies, orig. Entropic; Google Patents) | **[V1]** (Claim 1 match-error sign mapping; Claim 8 one-sided "uncovering→current frame, covering→first previous frame" verbatim — read first-hand) | **this session 2026-06-15** | https://patents.google.com/patent/US20120033130 |
| **US8576341B2** (§10 pass) | "Occlusion Adaptive Motion Compensated Interpolator" (STMicroelectronics, Petrides; Google Patents) | **[V1]** (histogram filter → reveal/conceal/normal; MCCURR/MCPREV/MEDIAN routing; fwd-projection-hole=reveal, bwd-hole=conceal — read first-hand; overturns the harness's 1-2 mis-refutation, §10-R1) | **this session 2026-06-15** | https://patents.google.com/patent/US8576341B2/en |
| **US9013584B2** (§10 pass) | "Border handling for motion compensated temporal interpolator using camera model" (STMicroelectronics; Google Patents) | **[V1]** (pan/rotation/zoom camera model; letterbox detector + border occlusion generator; FRAME-EDGE scope, object-motion bypasses the model — read first-hand; the F22 caveat) | **this session 2026-06-15** | https://patents.google.com/patent/US9013584B2/en |
| **US10154230B2** (§10 pass) | "Generating an output frame for inclusion in a video sequence" FRUC/halo patent (Imagination Technologies; Google Patents) | **[V1] for the mechanism** (per-pixel trust from fwd/bwd/double-ended agreement + image halo-analysis; smoothed-MC fallback — read first-hand); the "needs no engine MVs/depth" framing is **[V2]/inference** (not the patent's words, §10-R) | **this session 2026-06-15** | https://patents.google.com/patent/US10154230B2/en |
| **US9094561B1** (§10 pass) | "Frame Interpolation and Motion Vector Reconstruction" (Pixelworks; Google Patents) | **[V1]** (double-confirmation occlusion; signed kocc ">0⇒cover, <0⇒uncover"; background-MV-near-object for halo — read first-hand; kocc formula symbols summarizer-garbled, the sign rule clean) | **this session 2026-06-15** | https://patents.google.com/patent/US9094561B1/en |

---

## §8 — What could NOT be verified first-hand (honest gaps — FDP §2.4)

An honest gap is a feature; a hidden one is a defect. Stated explicitly:

- **The SROCC≈0.13-for-PSNR magnitude (VFIPQA Table I)** rests on the harness's
  transcription of the paper's results table, **not** an author-verified or
  first-hand-read figure. SROCC/PLCC magnitudes are **highly dataset- and
  pooling-dependent** (the same metric scores ≈0.13 on one VFI-MOS set and ≈0.52
  on another). **Use the qualitative ordering** (PSNR/SSIM ≪ LPIPS/DISTS ≪
  bespoke) as the design driver, never the absolute number. It is cited **[V2]**.
- **The MSU 5-metric methodology page** (the exact study parameters: 413
  participants, 32 pairs, Bradley-Terry) was **not re-fetched first-hand** here; it
  was reached by the harness. The 5-metric *stack idea* is the load-bearing
  takeaway, not the study parameters. Cited **[V2]**.
- **E_warp / TS / DIV** (Lai et al.; the Trinity-College SPIE-2025 motion-metrics)
  were **not re-fetched first-hand**; and the harness **refuted (3×)** the claim
  that flow-warping is motion-magnitude-independent. These are *flags/localizers*,
  not motion-decoupled scores.
- **Proprietary FG internals — LSFG / DLSS-FG / FSR3.** The research found **no
  surviving verified claim** on their flow+warp internals (closed-source). Any
  statement that "LSFG always warps and never blends" in this dossier or its link
  targets is a **consistent inference** from the published academic cure family and
  the artifact's visible character — **labelled as an inference, never a cited
  fact**. A camera-captured no-reference comparison (the only shared terrain, since
  LSFG's 240fps overlay is digitally uncapturable — see §11.7) would be the way to
  test it; that experiment is not done.
- **Paper-body details tagged [V2] in §7** (RIFE's warp-then-soft-mask form and
  "ghosting" quote; FILM's scale-agnostic/backward-warp synthesis; EMA-VFI's
  "correlation" wording; PerVFI's symmetric-blending-as-root-cause sentence;
  FloLPIPS Eq. 4–6; softmax-splatting Eq. 13; VFIPS's specific ghosting/deformation
  examples) come from the paper bodies, **not** the fetched abstracts. They are
  consistent with the harness's primary-source extraction but were not transcribed
  clause-by-clause first-hand this session. Do **not** elevate them to [V1] without
  reading the PDF body.

**Added by the 2026-06-14 surgical-fix pass (§9):**
- **de Haan 1993 internal mechanism is NOT first-hand-read** (IEEE-paywalled). The
  bibliographic facts and "only eight candidate vectors per block" are confirmed
  via the TU/e portal + search summary, but the *smoothness-via-penalty /
  spatial-candidate-preference* claim the harness attributes to it is **paper-body
  [V2]**. The same mechanism IS confirmed first-hand in the Pohl 2018 and
  RU2656785C1 successors, so §9 rests the aperture claim on those, not on de Haan's
  unread body.
- **"Dane & Nguyen contrast against a 0.5/0.5 symmetric blend" — could NOT be
  confirmed.** The sign rule (Eq. 10), the ternary occlusion map K∈{0,0.5,1}, and
  the Eq. 14 collapse-to-single-source were all confirmed first-hand; the specific
  framing that their *baseline* is a 0.5/0.5 average was **not** in the fetched
  text. The corollary that {0,0.5,1} weighting equals (1−t,t) is therefore exact
  **only at t=0.5** by construction — an inference, not a paper statement (§9-R1).
- **Several MEMC primaries remain harness-only, not re-fetched first-hand:** the
  IEEE/SPIE/J-STAGE halo-reduction corroborators (Song/Men/Shi 2009 ICCE 5012399;
  Han 2013 SPIE 8768; IEICE E98.A "median-filter-when-occluded, else BDMC"). They
  back the one-sided-fill rule, which is **independently [V1]-confirmed** in
  US9148622B2 + Çiğla/Alatan + Dane/Nguyen, so §9 does not lean on the unread ones.
- **No measured cost for THIS pipeline.** Every "cheap / real-time / sub-ms" claim
  is the *sources'* measurement (mobile FRC silicon at <500 mA), **not** a Phyriad
  measurement on the 4090/1080 Ti at the pipeline's resolution and frame budget.
  Per the efficiency mandate that cost MUST be measured first-hand before any
  "fast" claim — it is an open question (§9 cross-link, and the harness's own
  `openQuestions`), not an established fact.

**Added by the 2026-06-15 image-boundary-gated-disocclusion pass (§10):**
- **The "+0.5 to +1.4 dB" halo-reduction range is only partially verified.** Only the
  academia/974176 **City** sequence was confirmed first-hand (**+1.38 dB**, 28.40 →
  29.78 dB, Table II); the other per-sequence gains bounding the range were not
  surfaced. Cite **+1.38 dB (City)** as verified; the broader range is unverified (§10-R2).
- **Author attribution of the academia/974176 "Occlusion Adaptive FRUC" mirror is
  summarizer-uncertain.** The §9 pass rendered it "Çiğla & Alatan" (Table 1); the §10
  pass fetch rendered it "Ozkalayci, Alatan, Batu" (Table II). The TITLE and the
  load-bearing facts (one-sided fill, +1.38 dB City) are stable across both; the exact
  authorship is **[V2]/uncertain** (WebFetch summarizer-rendered) — do not cite a
  definitive author for this mirror without a direct PDF read.
- **The §10 patent quotes are summarizer-rendered.** WebFetch extracts page text via a
  small summarizer model, so the §10 patent quotes are **first-hand-confirmed facts but
  near-verbatim**, not character-exact (the load-bearing sign-rules / titles / routing
  are unambiguous; the longest clauses and the Pixelworks kocc formula symbols are
  approximate). The softmax-splatting PDF was read directly. (§10-R4.)
- **DLSS-FG / FSR 3-FG / LSFG disocclusion handling stays UNANSWERED** (Q5).

---

## §9 — Surgical fix for the identified crossfade (2026-06-14 pass)

**Scope of this pass.** §1–§8 established the artifact's name (ghosting), root
cause (symmetric blend of misaligned samples, F8), and the *neural* cure family
(F9–F13). This pass answers the narrower, post-diagnosis question the operator
now holds: the crossfade is **symmetric blending of misaligned samples in two
specific regions — APERTURE (textureless interiors) and DISOCCLUSION** — and the
*always-warp / dense-neural-flow* cure is **rejected** (it produces the
geometric-hallucination "gravity" stretch, §5-1). What cheap, **non-neural,
real-time, surgical** fix reduces *this* crossfade *without* the always-warp
hallucination? Finding: the **MC-FRUC / MEMC** industry (TV frame doublers, FRUC
ASICs, 1993–2018) solved exactly this without neural nets or engine MVs, and its
named techniques map onto the existing block-match + confidence-gate + blend
pipeline. The harness ran 5 angles, 21 sources, 100 claims → 25 verified, **25
confirmed / 0 killed**; the load-bearing items below were re-fetched first-hand
this pass (levels in §7).

**Same level legend as §2** (FDP §2.3). The harness 3-vote tally is context; the
level is this dossier's own first-hand check.

### F14 — APERTURE fix (cheap, highest ROI): 3DRS candidate-selection + semi-global-MV penalty makes a flat interior inherit ONE coherent region MV `[harness 3-0, 4 corroborating claims]`
Instead of exhaustive search, each block draws its MV from a small fixed
**candidate set** — spatial predictors (neighbouring blocks of the *current* MV
field), temporal predictors (the *previous* frame's MV field), and small random
offsets — plus a **penalty/regularizer** biasing the choice toward the
neighbours' MVs. In a textureless interior the block-match cost (MAD) is
uninformative and ties across candidates, so the spatial-candidate + penalty bias
carries a *coherent region motion* into the flat block instead of a noisy
per-block MV. This is the aperture lever: it does **not** invent a warp, it
propagates the surrounding surface's already-estimated motion inward.
Level **[V1]** for the mechanism and its real-time cost: the candidate set
`CS(X⃗) = {CS_spatial, CS_temporal, CS_random}` is **verbatim** in the Pohl 2018
IS&T PDF (read first-hand), which also **measures** FullHD 15→30 fps at **<500 mA**
on a Galaxy S8 / MSM8998 with *no ASIC*; the semi-global-MV penalty `f(Prior,
MVcand)` that makes flat blocks inherit a coherent SGMV when MAD ties is
**verbatim** in patent RU2656785C1 (read first-hand). The *originating* peer-
reviewed source (de Haan 1993, "only eight candidate vectors per block") is
bibliographically confirmed but **IEEE-paywalled** — its internal
smoothness-penalty mechanism is **[V2]** (paper-body, not read; §8), so this
finding rests on the two first-hand-read successors, not on de Haan's unread body.

### F15 — APERTURE cleanup (trivial cost): SELECTIVE vector-median filtering, applied ONLY where the MV field breaks `[harness 3-0]`
After estimation, compute each MV's variation against its 8 neighbours; **only if
it exceeds a threshold** flag it a "single bad motion vector" and replace it by
the vector-median of its neighbours (the neighbour MV minimising the L1 distance
sum). The *selective* (conditional) application is the surgical property — it
removes outlier MVs at motion-field discontinuities without globally
over-smoothing. Level **[V1]**: the rule is **verbatim** in the MSR-Asia PDF (read
first-hand) — "If the variation exceeds a certain threshold, the motion vector is
regarded as a single bad motion vector and then vector median filtering is
applied." (Vector-median is the classical Astola/Haavisto/Neuvo 1990 operator —
non-neural.)

### F16 — DISOCCLUSION detection (RGB-only, highest confidence): from MV-field consistency alone `[harness 3-0; sub-claim [15] 2-1 but high-confidence]`
Covered/uncovered regions are detectable **from the block-matched MV field alone**
— no depth, no segmentation, no engine MVs — by three convergent named heuristics:
(a) **forward-backward MV similarity** — a region a *forward* path intersects but a
*backward* path does not is **covered**; the reverse is **uncovered** (forward
3DRS estimates uncovered reliably, backward estimates covered reliably);
(b) the **SIGN of the inter-block MV difference** — `dVx≥Th ⇒ uncover`,
`dVx≤−Th ⇒ cover`; (c) **MV-field divergence** — expanding ⇒ uncover, contracting
⇒ cover. Level **[V1]**: US9148622B2 (covered = only forward MVs intersect,
uncovered = only backward) and Çiğla/Alatan (forward-reliable-on-uncover /
backward-on-cover) read first-hand; the sign rule Eq. 10 read first-hand in
Dane/Nguyen. **Honest bound:** the sign/divergence heuristic is acknowledged
*noisy* (poor MVs corrupt it; foreground/background ambiguity) — it distinguishes
cover/uncover but is not perfectly robust, so it MUST be gated by the per-tile
confidence the pipeline already computes.

### F17 — DISOCCLUSION fill (the cheapest surgical fix, multiply-confirmed): the ASYMMETRIC ONE-SIDED rule `[harness 3-0, six convergent primary sources]`
The fix is a branch, not a network: in a **covered** region reconstruct from the
**prior frame ONLY** (correspondence exists only there); in an **uncovered**
region from the **next frame ONLY**; use a symmetric/bidirectional blend **only
where both directions are valid**; fall back to plain motion-less averaging
**only when neither is valid** (the irreducible gap, F19). It **never averages two
misaligned samples** in the failure region — so it kills the crossfade *without*
the always-warp hallucination, because it re-weights existing samples rather than
inventing content. Level **[V1]**: US9148622B2 — "The covered regions are
constructed from the pixel data of the prior frame … while the uncovered regions
are constructed from the pixel data of the next frame" (verbatim, read first-hand);
Çiğla/Alatan Table 1 — uncover=forward-only MC, cover=backward-only MC,
both-valid=bidirectional, neither-valid="simple averaging without motion"
(read first-hand). This is decades-stable, foundational, non-neural FRUC logic.

### F18 — The drop-in for the (1−t,t) blend on failed tiles: an occlusion-map {0, 0.5, 1} weighting that COLLAPSES to one source `[harness 3-0]`
The surgical replacement for the symmetric `(1−t,t)` blend *on the failed/occluded
tiles only* is a directional weighting controlled by an **occlusion map K∈{0,0.5,1}**
that collapses to a single source in the reliable/occluded case (one prediction
weight = 1, the other = 0), leaving the symmetric blend on confident tiles. It
maps directly onto a block-match + per-tile-confidence pipeline: weight toward the
more-confident / less-occluded source where the gate fired. Level **[V1]**:
Dane/Nguyen Eq. 14 read first-hand — "if BP D_{m,n} ≤ ε₄ then w_{k*}=1, w_k=0 for
k≠k*" literally collapses to a single source; the map K∈{0,0.5,1}
(uncover/uncertain/cover) modulates the weighting. **Peer-reviewed validation of
the principle (PerVFI, CVPR 2024):** the paper names symmetric blending — "blends
the features from two sides with equal contribution, even when they are
misaligned" — as the ghosting mechanism, and its **Table 3 ablation on DAVIS
(480P)** is the controlled measurement of the crossfade trade-off: the *adaptive
(symmetric)* mask scores **27.61 PSNR / 78.20 VFIPS**, the *quasi-binary
(asymmetric)* mask **27.16 / 83.30** — *higher PSNR but lower perceptual score for
the symmetric average* (the fully-adaptive mask "tends to center around 0.5,
signifying an equal contribution," Fig. 6). The PSNR↑-but-perceptual↓ direction is
exactly the crossfade trade-off. **Cost caveat (load-bearing):** PerVFI *itself* is
a HEAVY neural method (normalizing-flow generator + deformable conv) — it validates
the asymmetric-blend **principle**, it is **NOT** the cheap implementation. The
cheap implementation is the FRUC one-sided fill (F17) + the {0,0.5,1} weighting
above. The PerVFI paper-body items used here were read first-hand this pass
(**[V1]**, §7 "PerVFI (paper body)" row); the §1–§8 PerVFI *abstract* attestation
is reused, not re-stated (FDP §4.8).

### F19 — THE IRREDUCIBLE GAP (the honest open problem): a true hole, visible in NEITHER frame `[harness 3-0, inference]`
Where a disocclusion region is visible in **neither** frame (a true hole), or both
candidate sources are occluded/unreliable, the one-sided rule has **no valid
source to commit to**. Every cheap MC-FRUC method then falls back to plain
motion-less averaging `(1−t,t)` — **which is exactly the crossfade the pipeline is
trying to avoid**. Genuinely filling that region requires inpainting /
background-hallucination — the heavy method the operator rejected to avoid the
"gravity" geometric hallucination (§5-1). This is the irreducible trade-off: in
the true-hole region the choice is between a soft crossfade (cheap, no
hallucination, the artifact returns) and inpainting (heavy, risks hallucination).
The cheap techniques **bound** the crossfade to the genuine no-correspondence
region; they cannot **eliminate** it there. Level: **inference** synthesising the
[V1] sources — Çiğla/Alatan Table 1 explicitly lists "neither valid ⇒ simple
averaging without motion" as the last resort, and PerVFI's complementary-info
design only works because a learned generator *can* hallucinate plausible content.
Not a single cited measurement.

### §9-R — Refuted / uncertain (this pass) — kept visible
1. **"Dane & Nguyen's contrasted baseline is a 0.5/0.5 symmetric blend"** —
   **could NOT be confirmed first-hand** (§8). The sign rule, the {0,0.5,1} map,
   and the collapse-to-single-source were all confirmed; the *baseline-is-0.5/0.5*
   framing was not in the fetched text. Consequence: the corollary "{0,0.5,1}
   weighting ≡ (1−t,t)" holds **only at t=0.5** by construction — an inference, not
   a paper claim. At t≠0.5 (arbitrary-phase interpolation) the directional
   weighting still applies but the exact-equivalence argument weakens.
2. **The MSR-Asia confidence gate (F15's source) operates on H.264 BIT-STREAM MVs,
   not RGB-derived MVs** (confirmed first-hand: "the motion vectors embedded in the
   bit-stream"). The gate **pattern** (SAD+BAD classify → re-estimate only the bad
   2.0%–63.0% fraction) transfers to the pipeline; the **MV source does not**. This
   is an application-fit caveat, not a falsification — recorded so the prior art is
   not over-claimed as a no-bitstream RGB precedent.
3. **3DRS's OWN authors list the aperture + occlusion regions as UNSOLVED future
   work** (Pohl 2018, verbatim, read first-hand): "a decrease of algorithm
   sensitivity to periodic patterns, **flat areas** and changes in scene
   brightness" and "improve the precision of **occlusion detection and handling**."
   Consequence: candidate-selection (F14) **mitigates** the aperture crossfade, it
   does **not cure** it; disocclusion quality is bounded by the fill/MCI stage
   (F17–F19), not the ME stage. This tempers any "3DRS solves the aperture" reading.

### §9 — What this grounds (cross-link row, single source of truth — NOT an implementation directive)
Per FDP §4.6 this dossier records prior art; it does **not** modify or direct the
render_assistant code. The mapping below is descriptive, linking to where Phyriad
already acts (the authoritative home), not a build order.

| Surgical fix (here) | Where it grounds in Phyriad (authoritative home — not re-described) |
|---|---|
| 3DRS candidate-selection aperture lever (F14) | the **`candsel`** candidate-selection path (built, **default-OFF**) — see the [`OpticalFlowPipeline`](../../framework/render/vulkan/include/phyriad/render/vulkan/OpticalFlowPipeline.hpp) hier-match shader + [§11.7](ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md) |
| Asymmetric one-sided disocclusion blend (F17–F18) | the planned **disocclusion Lever 2** (the {0,0.5,1}-style collapse on failed tiles) — [§11.7](ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md), *designed*, not shipped |
| "Don't let crossfade-failed tiles seed the temporal priors" (operator's selective-temporal-feedback idea) | the MEMC complement — *don't propagate unreliable vectors* (F15 selective cleanup + F16 noisy-detection gating) — [§11.7](ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md) |
| The true-hole irreducible gap (F19) | the honest bound on the cheap path; inpainting is the rejected heavy method (§5-1) — no Phyriad home, by design |

---

## §10 — Image-boundary-gated disocclusion: the "gravity" pass (2026-06-15)

**Scope.** §9 (F16–F19) established the disocclusion cure from the **MV field
alone** (detect via fwd-bwd consistency / sign / divergence; fill one-sided; the
{0,0.5,1} collapse; the irreducible true-hole gap). This pass answers a narrower
question raised once the cure was *visualized*: the P1 `--afill` overlay (the iGPU
image-contour advected to the present phase) made the residual **"gravity"** —
background pixels near a moving object attracted toward it — *directly visible* as
the disocclusion smear (the block-match MV is object-biased at the boundary, so
background inherits the object's MV). The new question: does the SOTA support using
our **image-derived boundary field** (which §9's flow-only detection lacked) to
GATE that fill, and what is the honest ceiling? A 5-angle harness pass (22 sources,
100 claims → 25 verified, 22 confirmed / 3 killed) + a first-hand re-verification of
7 load-bearing primary sources (all reachable; levels below). **Verdict: it largely
CONFIRMS §9 with more primary patents, and adds two things §9 lacked — the
image-field as a trust signal (F20) and the camera-model-fill = frame-edge-only
caveat (F22) — plus a correction of a harness mis-refutation (§10-R).** Level legend
as §2; the harness vote is context, the level is this dossier's own first-hand check.

### F20 — The IMAGE-derived boundary as a per-pixel TRUST/gating signal (the differentiator §9's MV-only detection lacked) `[harness 3-0; partial first-hand]`
A FRUC/halo-reduction pipeline can set a **per-pixel trust** from BOTH (a) the
AGREEMENT among forward / backward / double-ended motion predictions AND (b)
**image analysis of halo artefacts**, and for untrusted pixels output a **smoothed**
motion-compensated value (perceptual masking) rather than synthesize. This is the
SOTA hook for our setup: the iGPU's aligned image-contour field is an **independent
trust signal** that confirms/gates the flow-derived occlusion class where the flow
itself is unreliable (the exact failure that produces the gravity). It GATES the
existing one-sided fill; it is **not** an MV-corrector (the "edge-weighted MV
smoothing removes the attraction" framing is refuted, §10-R). Level **[V1] for the
mechanism, with one honest downgrade:** US10154230B2 (Imagination Technologies) read
first-hand — trust from prediction-agreement + "analysing the image … to identify …
halo artefacts," and the smoothed-MC fallback, are present. **But** the patent does
**not** itself claim "needs no engine MVs / depth" — it is image-only *in practice*
(internal block ME, no engine data) but that phrasing is the harness's inference,
not the patent's words; so the "no-engine-data" framing is **[V2]/inference**.

### F21 — The asymmetric ONE-SIDED fill, corroborated by 3 more first-hand primary patents (deepens §9 F17) `[harness 3-0]`
§9 F17 rests on US9148622B2 + Çiğla/Alatan + Dane/Nguyen. This pass adds three
independently-read primary patents, all **[V1]**: **US20120033130** (Dynamic Data
Technologies, orig. Entropic) — Claim 1's relative match-error sign mapping
(`fwd-err > bwd-err ⇒ covering`, inverse ⇒ uncovering) and Claim 8's one-sided rule
("uncovering … interpolated using blocks in the current frame and covering … using
blocks in the first previous frame," verbatim); **US8576341B2** (STMicroelectronics,
Petrides) — an "assignment status histogram filter" classifies reveal/conceal/normal
and routes "MCCURR … in conceal … MCPREV … everywhere else … the MEDIAN," AND
projects full-frame pixels along fwd/bwd MVs so a **forward-projection hole = reveal,
backward-projection hole = conceal**; **US9094561B1** (Pixelworks) — a SIGNED
occlusion measure `kocc` (">0 ⇒ cover, <0 ⇒ uncover") plus detection of a
**background MV near the foreground object** to suppress halo. The directional
one-sided fill is thus decades-stable, multiply-patented, non-neural FRUC logic.

### F22 — The global/camera model fills FRAME-EDGE disocclusion, NOT interior object bands (the honest caveat for our gme-background fill) `[harness 3-0]`
The "fill the revealed background from a global/camera model" idea — which our gme
affine model invites — is, in the SOTA, scoped to the **frame border**. Level
**[V1]:** US9013584B2 (STMicroelectronics) is literally titled "**Border handling**
for motion compensated temporal interpolator **using camera model**"; it detects
"pan, rotation, and zoom," builds a whole-frame average-motion field, and a
"letterbox detector" + "border occlusion generator" handle reveal/conceal "near the
borders." Object-motion reveals/conceals explicitly **bypass** the camera model.
**Implication (honest):** using our gme global model to fill an *interior*
object-boundary disocclusion is an **extension beyond the cited prior art**, not a
validated technique — the revealed interior background's true motion equals the
camera model only when the background is static/camera-moving; otherwise the
reliable-side one-sided fetch (F21), not the global model, is the SOTA answer. Our
gme background-extension is a heuristic to *measure*, not a proven fill.

### F23 — The irreducible gap, re-confirmed by two more primary sources (corroborates §9 F19) `[harness 3-0]`
For the band where **no** reliable side exists (true hole, or the halo pixels are
untrusted), the SOTA does **not** synthesize: it **commits to a side via fallback**
(US8576341B2) or **perceptually masks via a SMOOTHED motion-compensated version**
(US10154230B2 — "blur[s] out … the halo artefacts"), both read first-hand **[V1]**.
Neither invents content. Same irreducible bound §9 F19 named — confirmed, not
closed: the cheap path bounds the smear to the genuine no-correspondence region; it
cannot eliminate it there.

### §10-R — Corrections / refuted / honest gaps (this pass)
1. **The harness MIS-REFUTED US8576341's side-projection.** Its adversarial vote
   killed (1-2) the claim that US8576341 locates reveal/conceal by projecting
   full-frame pixels along fwd/bwd MVs. First-hand reading **overturns that
   refutation**: the patent DOES it (later embodiments) — "a reveal region where
   there is a hole in the **forward** projected frame store … conceal … in the
   **backward** projected frame store." So the side-projection IS prior art (mapping
   `fwd-hole=reveal, bwd-hole=conceal`). Recorded so the harness's false-negative
   does not propagate.
2. **The "+0.5 to +1.4 dB" range is only partially verified.** The Ozkalayci/Alatan/
   Batu paper's **City** sequence is confirmed first-hand at **+1.38 dB** (28.40 →
   29.78 dB, Table II); the other per-sequence gains that would establish the *range
   bounds* were not surfaced. Treat **+1.38 dB (City)** as the one verified number;
   the broader range is **unverified** (§8).
3. **The "edge-weighted MV smoothing removes the attraction" framing is refuted**
   (MRF boundary-preserving smoothing 0-3; edge-weighted WVM removing outlier MVs
   1-2 — both killed). The image boundary's role is **gating the fill** (F20), not
   "smoothing the MV field to stop the attraction."
4. **Verification-method honesty:** the WebFetch tool extracts page text through a
   small summarizer model; the quotes above are **first-hand-confirmed facts** but
   **summarizer-rendered**, so treat them as *near-verbatim* (the load-bearing
   sign-rules / titles / routing are unambiguous; exact character-level wording of
   the longest clauses is not guaranteed). The arXiv softmax-splatting PDF and the
   patent bodies were read to raise confidence; softmax-splatting is **NEURAL** (the
   importance metric Z is learned), its non-neural antecedent is z-buffered forward
   warp (Niklaus). Q5 (DLSS-FG / FSR 3-FG / LSFG disocclusion handling) remains
   **unanswered** (§8).

### §10 — What this grounds (cross-link row — descriptive, NOT an implementation directive)
| Finding (here) | Where it grounds in Phyriad (authoritative home) |
|---|---|
| Image-field as a trust/gating signal (F20) | the **iGPU contour field** (STAGE-G1, `igpu_field.comp`, shipped P0) + the **P1 `--afill`** advected visualizer that made the gravity visible — the trust signal P2 feeds to the warp's one-sided fill |
| One-sided fill + signed occlusion class (F21) | the EXISTING warp machinery — STAGE-48 occlusion-class (`occl_thresh`, default-ON) + STAGE-50 divergence-directed fill (`--fill-div`) in `wap_warp.comp` — which P2 gates with F20 |
| Camera-model fill = frame-edge only (F22) | the honest bound on the gme background-extension for *interior* disocclusion — measure, don't assume (D-11) |
| The irreducible true-hole gap (F23, = F19) | the honest ceiling; no Phyriad home, by design |

---

## §11 — Multi-candidate generation & selection: the "discriminate the worst" pass (2026-06-15)

**Scope.** §9–§10 settled the *disocclusion* sub-problem (detect via MV
consistency / divergence sign; fill one-sided; gate with the image field; the
irreducible true-hole). This pass widens the question to the **general** synthesis
decision the operator's *"generar más + discriminar + SELECCIONAR el más limpio"*
names: given several candidate colors per pixel (we already compute WARP,
CROSSFADE, GME-background, and the C2 second-best candidate, and can cheaply add
MV-perturbed warps), **how does the SOTA SCORE and SELECT the cleanest candidate
WITHOUT ground truth** — to kill the *generation* "big-jump"/ghosting artifacts
that temporal smoothing (`--ts-smooth`) only masks? A 6-angle harness pass (48
claims → **43 survived / 5 killed**, 13 Phyriad-protocol-loaded agents) + a
**first-hand re-verification of the 10 design-load-bearing primary sources**
(levels below; 3 assignee corrections). Level legend as §2; the harness vote is
context, the level is this dossier's own first-hand check.

**Verdict (the load-bearing one — read it before building).** The SOTA **does
NOT, and provably CANNOT, solve selection-without-ground-truth.** VFI is inherently
ill-posed: from two frames there are infinitely many valid trajectories, so no
scorer recovers *the* correct intermediate — only a *plausible* one (F37–F39).
Every surveyed system — neural and shipping-non-neural alike — therefore converges
on the architecture **we already have**: generate multiple candidates from
multiple motion hypotheses, then resolve per-pixel by a **no-GT confidence
proxy** — overwhelmingly (a) photometric self-consistency (the L1 back-warp
residual = our `d_pixel`), (b) forward-backward flow consistency, (c)
divergence/trajectory-existence disocclusion class (= our STAGE-50 sign). The
single most useful empirical result: **averaging two warps at mask ≈ 0.5 is the
proven source of blur/ghosting**, and the perceptual leaders replace the soft
blend with a **near-binary HARD pick** at disocclusions (F36, F33, F40) — exactly
the operator's "select, don't blend-smooth," now externally corroborated. The
closest shipping non-neural analog (AMD FidelityFX FI) and the production FRUC
patents pick between candidate warps by color-similarity / hit-distance /
trajectory-existence — **none uses an image-edge trust gate**, so our Sobel
boundary field is a *genuinely additional* signal (F29, F43) — which also means it
is **unvalidated novel territory** (the honest flip side: no published evidence it
works for selection — we must measure it). The honest bound: our pass reduces
**artifacts** and improves **plausibility/sharpness**; it does **not** recover
ground truth (F37), and photometric self-consistency itself goes **uninformative
in the disocclusion band** (a revealed pixel matches neither anchor), which is why
production falls back to single-anchor half-length vectors + mip inpaint there
(F40, F29).

### F24 — Multi-hypothesis MCP: gain scales with hypothesis count, but it is a BLEND result `[V2]`
Girod's efficiency analysis ([IEEE TIP 9(2), 2000, pp. 173–183](https://pubmed.ncbi.nlm.nih.gov/18255384/))
shows averaging N motion-compensated hypotheses can gain ~0.5 bits/sample per
doubling (up to 8 hypotheses, 10 reference frames). **[V2]** — pubmed abstract; the
bits/sample figure not re-read first-hand. **Relevance:** formal motivation for a
*rich candidate set*, but the gain is a coding/BLEND benefit; our goal is the
opposite (hard-select to avoid blend-blur) — motivation for generate-more, not for
fusing the set.

### F25 — Per-pixel hypothesis LABELING by color+shape+smoothness `[V2]`
Jeong/Lee/Kim ([IEEE TIP 22(11), 2013, pp. 4497–4509, DOI 10.1109/TIP.2013.2274731](https://pubmed.ncbi.nlm.nih.gov/23893726/))
form per-pixel hypotheses by varying block size + direction, then SELECT by a
labeling energy (color + shape + smoothness). **[V2]** — pubmed listing, not
re-opened first-hand this pass. **Relevance:** direct prior art for
generate-then-select; the energy decomposition is a scorer template (we add the
image-field as a shape/edge term). Corroborates F32 (verified first-hand).

### F26 — Reliability-weighted overlapped blend (AOBMC) `[V2]`
Choi/Han/Kim/Ko ([IEEE TCSVT 17(4), 2007, DOI 10.1109/TCSVT.2007.893835](https://pure.korea.ac.kr/en/publications/motion-compensated-frame-interpolation-using-bilateral-motion-est/)):
bilateral ME on the interpolated plane + OBMC whose window coefficients are gated
by NEIGHBOR MV reliability (disocclusion neighbors weigh less). **[V2]**.
**Relevance:** a cheap spatial-coherence term (neighbor `d_pixel`), but a soft
window blend — weaker than the hard-pick perceptual results (F33/F36/F40).

### F27 — Nearest-to-median candidate selection, occlusion-robust (Apple) `[V1]`
US7548664B2 / US20080085056A1 (Apple Inc, inventor Souchard), read first-hand:
"apply a scalar median filter componentwise … then sets pixel P(x,y) … nearest (in
the sense of L1 norm) to Pm(x,y)"; "particularly robust to the presence of
occlusion." **[V1]** ([patent](https://patents.google.com/patent/US20080085056A1/en)).
**Relevance:** the single most transplantable selection rule — pick the candidate
L1-nearest the per-pixel median of {WARP, CROSSFADE, GME-BG, C2, perturbed}:
near-free GPU op, rejects the lone hallucinating outlier. (Caveat: fails when the
majority are wrong the same way — open problem 6.)

### F28 — Content-class agreement acceptance gate (Qualcomm FRUC) `[V2]`
US20060017843A1 (Qualcomm, Shi/Raveendran) generates forward/backward/acceleration-
extrapolated candidate MVs and switches bi-/uni-directional mode by a content-class
AGREEMENT criterion; blocks down to 4×4. **[V2]** ([patent](https://patents.google.com/patent/US20060017843)) —
not re-opened first-hand. **Relevance:** the "fwd and bwd must agree" acceptance
gate is a cheap no-GT validity check per candidate; acceleration-extrapolated
candidates are a recipe for generate-more.

### F29 — Dual-MV + color-similarity + dual disocclusion masks; NO edge gate (AMD FidelityFX FI) `[V1]`
GPUOpen FidelityFX FI manual, read first-hand: blends an optical-flow-warp with a
game-MV-warp "based on the color similarity … if optical flow motion vectors result
in a better match the algorithm prefers them"; builds "two disocclusion masks" and
"determine[s] if any reprojection direction is to be discarded"; a 16-bit priority
(1 primary/secondary + 10 camera-distance + 5 color-similarity); holes filled by an
SPD mip-chain "averaged from the closest color values." **No image-edge / Sobel
trust gate** (confirmed absent). **[V1]**
([manual](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/frame-interpolation/)).
**Relevance:** the reference shipping non-neural architecture — and it INDEPENDENTLY
lands on photometric color-match as the selector (corroborates `d_pixel`), yet has
the engine MVs/depth we lack AND no edge gate → our Sobel field is strictly
additional. Dual-mask "discard a direction" = our reveal/conceal class; mip-fill =
a proven cheap fallback.

### F30 — 3DRS ranked candidate-list + bilateral-SAD minimum `[V3]`
The 3-D Recursive Search family propagates a small candidate-MV list from
spatial/temporal neighbors, evaluates each by bilateral SAD, keeps the minimum.
**[V3]** — neither primary opened first-hand
([IS&T 2018](https://library.imaging.org/ei/articles/30/13/art00014);
[US10225587B2](https://patents.google.com/patent/US10225587B2/en)); well-attested
folklore, re-verify before relying. **Relevance:** the cheap way to GENERATE a good
candidate set (seed from neighbors + global motion, rank by SAD, keep the few best)
— directly applicable to our perturbed-MV candidates without exhaustive search.

### F31 — 8 candidates + weighted hit-distance + double-confirmation (Pixelworks) `[V1]`
US9094561B1 (**Pixelworks Inc**), read first-hand: "8 candidate motion vectors …
minimal hit distance between the interpolated phase motion vector and the P1 and CF
phase motion vectors" (candidates = spatial neighbors, prior/current phase blocks,
3×3 best-fit, global), selection by `weighted_dist[i]=hitdist_p1[i]+(64−w)*(hitdist_cf[i]−hitdist_p1[i])/64`;
occlusion by "double confirmation of motion vectors between phases." **[V1]**
([patent](https://patents.google.com/patent/US9094561B1/en)). **Relevance:** a
complete production no-GT generate-and-select recipe; the hit-distance
(temporal-consistency) metric is a scorer we do not yet compute and could add.

### F32 — Energy scorer: image-likelihood + motion-likelihood + smoothness MRF + occlusion prior (Google) `[V1]`
EP2979243A1 (**Google LLC**), read first-hand: selects among hit-list candidates
(forward/backward/8-neighborhood/current-site) by minimizing a combined energy —
image likelihood (photometric difference under the warp) + motion agreement +
**Gibbs-prior** spatial smoothness (penalizing motion discontinuities) + an
occlusion prior. **[V1]** ([patent](https://patents.google.com/patent/EP2979243A1/en)).
**Relevance:** THE explicit non-neural scorer template; maps onto `d_pixel`
(image-likelihood) + MV-agreement + neighbor-coherence (smoothness) + divergence-
class (occlusion prior). We add a **5th term unique to us: image-boundary
agreement from the Sobel field** (F43).

### F33 — Hard infilling-vector select beats soft blend at disocclusions `[V2]`
Infilling Vector Prediction ([arXiv 2110.08805](https://arxiv.org/abs/2110.08805)):
each disoccluded pixel assigned exactly ONE source location (temporal prior + depth);
authors report it outperforms soft blending. **[V2]** — authors' own evaluation, not
an independent study; needs depth + a learned prior we lack. **Relevance:** borrow
the PRINCIPLE (hard per-pixel source pick at reveals), not the mechanism.

### F34 — Photometric self-consistency is the no-GT metric; exp(α·Z) is the soft→hard knob (softmax splatting) `[V1]`
Niklaus & Liu ([CVPR 2020](https://arxiv.org/abs/2003.05534)); the
[official implementation](https://github.com/sniklaus/softmax-splatting) read
first-hand: `tenMetric = l1_loss(tenOne, backwarp(tenTwo, tenFlow))` (the L1
back-warp residual = **our `d_pixel`**) combined as `(-20.0 * tenMetric)` —
"-20.0 is a hyperparameter, called 'alpha' in the paper." **[V1] for the code-level
facts** (Z + α=−20, optionally learnable); the paper-body symbolic form via
abstract. **Relevance:** validates `d_pixel` as THE field-standard no-GT proxy;
`exp(α·Z)` is one tunable knob spanning soft-blend (small |α|) → hard-argmax
(large |α|) — sweep by region. (Mechanism is non-neural; only the optional
fine-tune of α is — corrects §10-R(4)'s "softmax-splatting is NEURAL.")

### F35 — FB-consistency-weighted soft fusion, NO neural confidence (OCAI) `[V1]`
OCAI ([CVPR 2024](https://arxiv.org/html/2403.18092v1)), read first-hand:
confidence `C₀,₁ = exp(−|V̂₀→₁+V̂₁→₀∘V̂₀→₁|² / (γ₁(…)+γ₂))`, γ₁=0.01, γ₂=0.5 (no
neural component in C); fuses by `C_{t,0}/(C_{t,0}+C_{t,1})`; forward-collision mask
uses α=50 and "above 100 … the result becomes not a number" (Table 7). **[V1]**.
**Relevance:** a transplantable non-neural confidence weighting if we compute a
forward MV field; the **α=50 / NaN-at-100** is a hard stability bound for any
exp-weighted mask we build (clamp α).

### F36 — mask ≈ 0.5 = blur; the quasi-binary mask is the perceptual fix (PerVFI) `[V1]`
PerVFI ([CVPR 2024](https://arxiv.org/html/2404.06692v1)), read first-hand: Fig 6
"the fully-adaptive mask tends to center around 0.5 … equal contribution … leading
to blur"; replaced by a quasi-binary tanh mask `M̃ = tanh(|M̂ + α·n| + β·M_bl)`;
asymmetric philosophy "basing the output on a single frame while utilizing other
frames to supplement specific details"; base mask threshold ε = 0.5 default.
**[V1]**. **Relevance:** the strongest external corroboration of "select, don't
blend-smooth" — avoid the 0.5-region soft blend; bias to a hard pick wherever
`d_pixel`/disocclusion-class flags a boundary. (The learned Adaptive Dilation
Module is neural and NOT transplantable; the binary-mask principle is.)

### F37 — VFI is fundamentally ill-posed: infinitely many trajectories `[V1]`
Velocity Disambiguation ([arXiv 2311.08007](https://arxiv.org/abs/2311.08007)),
read first-hand: "infinitely many possible trajectories: accelerating or
decelerating, straight or curved"; + a directional ambiguity for objects equidistant
from both frames. **[V1]**. **Relevance:** the load-bearing limit on the whole
endeavor — no scorer (ours or anyone's) recovers the CORRECT motion from two frames;
it picks a PLAUSIBLE one. Our claim must be **artifact-reduction, not correctness.**

### F38 — L2/L1 supervision vs one GT frame mathematically forces blur (the expected value) `[V2]`
The same two papers (velocity 2311.08007 + PerVFI 2404.06692, the latter naming the
root cause "Temporal Supervision Misalignment"): a deterministic model under L1/L2 to
one GT frame learns the EXPECTED VALUE over plausible motions → blur. **[V2]** —
principle confirmed via the two verified papers; the verbatim "expected value" not
re-checked. **Relevance:** why averaging yields blur and selection avoids it; do NOT
score toward closeness-to-an-averaged-target (that target IS the blurry mean).

### F39 — perceptual quality ⊥ GT pixel accuracy for ambiguous motion (VIDIM) `[V2]`
VIDIM ([arXiv 2404.01203](https://arxiv.org/abs/2404.01203)): a conditional-generative
sampler produces outputs that "correspond to a different choice of motion" than the
GT yet are human-preferred over blurry deterministic baselines. **[V2]** — the
verbatim quotes not re-confirmed from the abstract this pass. **Relevance:** optimize
plausibility/sharpness (the operator's sensation goal), not pixel-fidelity to an
imagined GT.

### F40 — production halo reduction: single-anchor half-length vectors in covered/uncovered (ASTRI) `[V1]`
US9148622B2 (**ASTRI**, NOT Samsung), read first-hand: classify covered/uncovered/
normal by fwd/bwd trajectory existence; covered → half-length vectors from the prior
frame, uncovered → half-length from the current frame, bi-directional for
background/objects; "motion vectors that are unreliable are deleted … large block
errors." **[V1]** ([patent](https://patents.google.com/patent/US9148622B2/en)).
**Relevance:** the concrete production recipe for OUR reveal/conceal class — at a
reveal, don't trust the symmetric two-anchor warp; use only the current anchor's
motion (half-magnitude). A hard per-class anchor SELECTION our divergence-sign
already supports.

### F41 — range-map occupancy = a one-flow disocclusion mask `[V1]`
Occlusion-Aware Unsupervised Flow ([arXiv 1711.05890](https://ar5iv.labs.arxiv.org/html/1711.05890)),
read first-hand: `V(x,y)=Σ max(0,1−|x−(i+F21x)|)·max(0,1−|y−(j+F21y)|)`, `O=min(1,V)`;
`V<1` ⇒ disoccluded; uses ONLY the backward flow. **[V1]**. **Relevance:** a cheap
disocclusion mask from our existing MV field in one scatter pass — no second
opposite-direction flow; gates the single-anchor switch (F40) / inpaint.

### F42 — motion-field DIVERGENCE localizes reveal vs conceal `[V2]`
Rüfenacht et al. (IEEE TCSVT 29(2), 2019): a "discontinuity likelihood map derived
from the divergence of a motion field," +2–2.5 dB. The primary
[IEEE page](https://ieeexplore.ieee.org/document/8278267/) is **HTTP-418-blocked**;
the mechanism is confirmed via the authors' project page and secondary sources. The
sign — div > 0 (source, spreading) = reveal; div < 0 (sink, converging) = conceal —
is standard vector calculus and is the convention our **STAGE-50** already
implements and tests in `wap_warp.comp`. **[V2]** (primary blocked; grounded in the
authors' page + our own implemented behavior). **Relevance:** the published basis
for our existing divergence class.

### F43 — no-reference ghosting detection from edge-neighborhood blend modeling `[V2]`
A ghosting-artifact detector ([ACM APGV 2009](https://dl.acm.org/doi/10.1145/1620993.1621022)):
Canny edges on low-passed patches + a Laplacian color-blend model — a patch is
ghosted if adjacent colors cannot be explained by a LINEAR blend. **[V2]** — read
via the authors' mirror. **Relevance:** the rare prior work that, like us, uses
IMAGE-edge structure (not flow) to score ghosting → our Sobel gate can REJECT the
CROSSFADE candidate where the field shows a double edge (the ghost signature).
Confirms our edge-gate is legitimate but under-explored.

### F44 — multi-field flow: several fine flows from one coarse pair (AMT) `[V2]`
AMT ([CVPR 2023, arXiv 2304.09790](https://arxiv.org/abs/2304.09790)) derives
"multiple groups of fine-grained flow fields from one pair of … coarse bilateral
flows" and warps each separately. **[V2]** — abstract; whether outputs are SELECTED
or FUSED is not stated. **Relevance:** the cheap generate-more principle — spawn
several candidate flows by perturbing ONE base flow (don't re-estimate); pair with
F27/F33/F36 (which say SELECT) for the resolution.

### §11-R — Corrections / refuted / honest gaps (this pass)
1. **Assignee corrections (first-hand):** EP2979243A1 (F32) = **Google LLC**;
   US9094561B1 (F31) = **Pixelworks Inc**; US9148622B2 (F40) = **ASTRI**, NOT
   Samsung (the harness's source metadata was wrong — corrected before citing).
2. **softmax-splatting is NON-neural in its core (corrects §10-R(4)):** the official
   repo computes Z as the L1 back-warp residual in code; α is a fixed constant
   (−20) that *can* be learned but need not be. `d_pixel` IS this metric.
3. **Refuted (killed by the adversarial pass, kept visible):** (a) "the VFI SOTA
   solves selection-without-GT" — refuted by F37/F39; (b) "photometric/FB
   self-consistency is a reliable winner-selector AT disocclusions" — refuted: at a
   true reveal the correct pixel is in NEITHER anchor, so low residual is
   uninformative there (a confident hallucination also scores low); (c) "image-prior
   (edge/seg/depth) methods give a ready-made RUNTIME candidate selector" — refuted:
   EpicFlow/FeatureFlow/EA-Net/VOS-VFI/DAIN use these at TRAINING or as
   flow-estimation guidance, not as a runtime hard selector, and several are neural →
   our Sobel-gated runtime selection is unvalidated novel territory.
4. **Honest level gaps:** F24/F25/F26/F28/F33/F38/F39/F43/F44 carry **[V2]** (the
   harness opened them; I did not re-fetch each first-hand this pass — the design
   rests on the **[V1]** set: F27, F29, F31, F32, F34, F35, F36, F37, F40, F41).
   F30 is **[V3]** (3DRS, neither primary opened). F42's primary is IEEE-418-blocked.
   None of the [V2]/[V3] is load-bearing for the build; they corroborate.
5. **Verification-method honesty (as §10-R(4)):** WebFetch renders pages through a
   summarizer — the quotes are *near-verbatim* first-hand facts (the sign-rules,
   numbers, titles, assignees are unambiguous; longest-clause character-level wording
   is not guaranteed).

### §11 — Open problems (the honest ceiling on the build)
1. **Plausible vs correct is unsolvable from two frames** (F37–F39). Our select pass
   rejects *incoherent* candidates; it cannot recover true motion. State as
   artifact-reduction, never correctness.
2. **Self-consistency degrades in the disocclusion band** — the one region we most
   care about — because the revealed pixel matches neither anchor; production
   abandons residual-blending there for single-anchor half-length + inpaint (F40,
   F29).
3. **No published runtime image-edge candidate selector** — our Sobel gate is novel,
   hence unvalidated; we must measure it (the flip side of novelty).
4. **No turnkey soft→hard crossover schedule** — `exp(α·Z)` gives the knob (F34) with
   a stability bound (F35, NaN at α≥100); the per-region schedule over {`d_pixel`,
   divergence-class, boundary field} is ours to tune + measure.
5. **Candidate-set diversity vs cost is unquantified perceptually** — F24's ~0.5
   bits/doubling is a coding result; the perceptual marginal value of the Nth
   perturbed-MV warp is unmeasured (efficiency mandate: measure before spending GPU).
6. **Median-consensus fails when the majority are wrong the same way** (F27) — need a
   "no good member" detector (candidates too dispersed → fall to GME-BG/inpaint, not
   select among bad ones).

### §11 — What this grounds (cross-link row — descriptive, NOT an implementation directive)
| Finding (here) | Where it grounds in Phyriad |
|---|---|
| Median-of-candidates select (F27) | the planned per-pixel selection operator over {WARP, CROSSFADE, GME-BG, C2, perturbed-MV} in `wap_warp.comp` |
| Energy scorer template (F32) + edge term (F43) | the per-pixel scorer = `d_pixel` (image-likelihood) + MV-agreement + neighbor-coherence + divergence-class (occlusion prior) + the iGPU Sobel field (binding 11) as the 5th, novel term |
| `exp(α·Z)` soft→hard knob (F34) + stability bound (F35) | one continuous mechanism spanning the existing CROSSFADE (soft) and the new hard SELECT, gated by `d_pixel`/divergence; clamp α |
| single-anchor half-length in the band (F40, F42) | the existing STAGE-48/50 one-sided fill, driven hard by the divergence-sign class |
| range-map occupancy mask (F41) | a cheap one-pass disocclusion gate from the existing MV field (no second flow) |
| hard-pick > soft-blend at reveals (F36, F33) | the operator's "select, don't blend-smooth" — the design's core choice |
| ill-posed bound (F37–F39) | the honest claim: artifact-reduction + plausibility, NOT GT recovery; the hand-eye-sensation yardstick |

---

*§11 added 2026-06-15 from a fourth deep-research-harness pass (multi-candidate
generation & selection without ground truth; 6 angles, 48 claims, 43 survived /
5 killed) by re-fetching its 10 design-load-bearing citations first-hand (3
assignee corrections). §9 added 2026-06-14 from a third deep-research-harness pass (surgical fix for the
identified crossfade) by re-fetching its load-bearing citations first-hand. §1–§8
consolidated 2026-06-13 from two deep-research-harness passes (VFI-quality
measurement; ghosting root-cause + cure) by re-fetching the load-bearing
citations first-hand per the FDP verifiable-reference mandate. The harness votes
are the harness's; the verification levels are this dossier's own first-hand
checks. Refuted claims (§4) and open problems (§5) are kept visible so we do not
accidentally believe them.*
