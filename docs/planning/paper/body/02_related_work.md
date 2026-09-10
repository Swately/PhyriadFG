# 2. Background and related work

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 3 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## R.1 — Distribution metrics (FID / CMMD / IS class) and why they do not measure fidelity here

*serves:* N-D · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q1 (rows Q1-9, Q1-10) · *status:* `EXISTS`

Distribution metrics — the FID / CMMD / IS class, extended to video as FVD — are set-level: computed over a batch of generated outputs against a batch of real samples, never paired with a single output's own ground truth. Two primary sources document that this makes the family insensitive to exactly the kind of error this identity's terms decompose. A CVPR 2024 study of FVD finds it "increases only slightly with large temporal corruption" and can be driven down, via sampling motionless generated videos, "without improving the temporal quality," concluding "FVD's bias towards the quality of individual frames" (Q1-9, [V1]). A video-interpolation paper that reports FID and FVD alongside reconstruction metrics states these are used precisely because they, "unlike reconstruction metrics, do not penalize plausible extrapolations that differ from the ground truth," and that "FID does not consider any aspects of temporal consistency and only considers each frame individually" (Q1-10, [V1]). A metric selected for not penalizing deviation from the ground truth cannot carry a thesis whose object is that deviation.

## R.2 — Full-reference perceptual metrics and the VFI benchmarks; their pixel-scale sensitivity as the table states it (Q2-4, Q2-12; absence N6)

*serves:* N-Q · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q2 · *status:* `EXISTS`

The full-reference perceptual family for video frame interpolation — LPIPS, DISTS, FloLPIPS, VFIPS, PSNR_DIV — measures agreement with human opinion against a full reference, not a per-pixel analytic truth. No source states a smallest detectable spatial error for LPIPS, DISTS, FloLPIPS or VFIPS (absence N6, §R.6). Where the family's pixel-scale behavior is stated at all, it runs the other way from what this identity's rulers need: DISTS is designed to be "relatively insensitive to geometric transformations (e.g., translation and dilation)" (Q2-4, [V2]), and a shift-tolerant LPIPS variant exists because, in the authors' own framing, "these metrics are often sensitive to a small alignment error that is imperceptible to the human eyes" (Q2-12, [V1]) — a sensitivity its own authors removed, not one available to reuse.

## R.3 — Synthetic / analytic ground truth and the sim-to-real objection (Q3-7)

*serves:* N-R · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q3 · *status:* `EXISTS`

Synthetic and analytic ground truth is established practice for optical flow and frame interpolation: Sintel, FlyingThings3D, Spring, Kubric and Middlebury's synthetic set all supply per-pixel truth from a render engine, and FlyingThings3D states that "the information is complete even in occluded regions since the render engine always has full knowledge about all (visible and invisible) scene points" (Q3-4, [V2]). The nearest neighbour to an analytic 3-D scene scored at a generator's own phase is Kiefhaber et al. 2024: 666 nonuplets rendered under homographies that "strictly follow the constraint of linearity," with in-between frames obtained by lerping and error split by occlusion count (Q3-6, [V1]) — two-dimensional sprites on a fixed seven-point grid, not a 3-D scene re-rendered at an arbitrary continuous phase.

Any claim resting on such truth carries the sim-to-real objection. A 2026 optical-flow study reports that "progress on Sintel, KITTI and Spring only weakly predicts accuracy on real-world data," over a benchmark of 8,204 real frame pairs (Q3-7, [V1]). The finding is for optical flow; no VFI-specific sim-to-real source was found. PhyriadFG's own content is a rendered game, closer to the synthetic side of that objection than to the paper's real-world targets — a caveat carried beside every number (cross-ref §07.2), not a rebuttal of it.

## R.4 — Reference-free error prediction and self-diagnosis (abstract-level rows)

*serves:* N-L · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q4 · *status:* `EXISTS`

Reference-free error prediction has precedent starting from optical flow itself: a flow network can "estimate their local uncertainty about the correctness of their prediction" in one forward pass (Q4-1, [V2]). For stereo matching the precedent runs further: per-pixel confidence learned without ground truth, from "contradictions and consistencies between multiple depth maps generated with the same stereo algorithm" (Q4-5, [V2]); a learned Laplacian variance over the disparity map, "large for low confident pixels while small for high-confidence pixels" (Q4-6, [V2]); and a self-supervised estimate from "the minimum information available in any stereo setup (i.e., the input stereo pair and the output disparity map)" (Q4-7, [V2]). For video frame interpolation the precedent is narrower: one method examines "the correlation between optical flow and IE" and proposes "novel error prediction metrics that partition the middle frame into distinct regions corresponding to different IE levels" (Q4-2, [V2]); another evaluates "the perceptual coherence of frames incorporating the original pair of VFI inputs," without the missing frame itself (Q4-3, [V2]). All six rows are abstract-level readings ([V2]); none predicts error from an interpolator's own internal decision traces — SAD, warp weights, stasis, guided picks. That predictor is the next body of work this identity authorizes and does not contain (cross-ref §06.4).

## R.5 — Error decomposition, "hallucination", and reliability practice

*serves:* PO · RE · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §2 Q5 · *status:* `EXISTS`

Error decomposition has precedent, not this identity's own decomposition. Middlebury conditions the same error over three region masks — All, Disc (near motion discontinuities), Untext (textureless) — and excludes semi-occluded pixels from the interpolation truth outright, rather than scoring them as a bucket (Q5-1, Q5-2, [V2]). DISTS separates a texture term from a structure term internally but sums them into one scalar (Q5-3, [V2]). "Hallucination" is used as a named, separately-scored failure mode in 2025 super-resolution work, where "generated details fail to perceptually match the low resolution image (LRI) or ground-truth image (GTI)" (Q5-4, [V2]) — for super-resolution, not for interpolation. One precedent gates an appearance term by a motion term without reporting them apart: an appearance metric is penalised "when that motion integrity is poor, because the underlying image would probably be of poor quality" (Q5-6, [V2]).

Reliability of the metric itself, as distinct from the reliability of what it measures, is not established practice. VMAF ships a 95 % confidence interval, but it is "a consequence of the fact that the VMAF model is trained on a sample of subjective scores, while the population is unknown," established "through bootstrapping using the full training data" — model uncertainty, not run-to-run variance (Q5-5, [V1]). Where test-retest reliability appears, it is applied to human subjects, not to the metric: a subject was rejected when a repeated rating on 5 of 43 images differed by more than a threshold on at least 3 of them (Q5-7, [V2]).

## R.6 — The bounded absences N1–N7, each with its search box and its date

*serves:* CB · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §4 · *status:* `EXISTS`

> **CORRECTION, 2026-09-10 (`../../../LEARNING_LOG.md` P-032).** Six of the seven rows below are refuted, by a
> re-sweep of two fields this table never searched. **N1, N2** — real-time graphics: *Amulet* (arXiv 2608.10423),
> *Mob-FGSR* (SIGGRAPH 2024), *Image-Based Bidirectional Scene Reprojection* (SIGGRAPH Asia 2011), *ExtraNet*
> (SIGGRAPH Asia 2021), *GFFE* (SIGGRAPH Asia 2024). **N3** — *The FID Lottery* (arXiv 2606.20536, 2026), on the
> generative half only. **N4** — Plack et al., *Frame Interpolation Transformer and Uncertainty Guidance*, CVPR
> 2023, which estimates "the expected error together with the interpolated frame": that is candidate I-B, already
> published. **N5** — US 6,064,393 (Lengyel, Snyder, Kajiya, 1997) defines a warped frame's geometric error in
> pixels. **N6** — Alabau-Bosque et al. (arXiv 2407.17927, 2024) gives per-metric translation-invisibility
> thresholds; §7 of the dossier had named this very paper as the one most likely to change N6, and it was not read.
> **N7 stands.** The table is kept as written, with this correction above it, until the operator decides whether
> Phase 1 re-opens (KAP §7).

All seven searches below were run with web access on 2026-09-09 and are time-boxed, not exhaustive (`FG_METRIC_MODEL_PRIOR_ART.md` §7); each row is reported as found none within its own search box, not as a proof of absence.

| id | absence | search box | nearest neighbour found |
|---|---|---|---|
| N1 | Prior work scoring an interpolation at an arbitrary continuous phase against an analytic 3-D scene re-evaluable at any t | two search-term sets combining "synthetic frame interpolation benchmark", "arbitrary time continuous phase" and "analytic ground truth OR closed-form ground truth"; direct inspection of Kiefhaber 2024 (7 fixed lerp points) and XVFI (real capture) | Kiefhaber 2024 (Q3-6); Middlebury's synthetic set re-rendered at t + 0.5 |
| N2 | The four-way visibility taxonomy (visible in both / only A / only B / neither) | as N1, plus the occlusion-class inspection of Middlebury and Kiefhaber | Kiefhaber 0/1/2-occ (does not say which input); Middlebury occluded/unoccluded + Disc/Untext |
| N3 | A VFI or generative-evaluation metric that reports its own run-to-run reliability | "metric test-retest reliability video quality"; VMAF CI documentation; Ghadiyaram & Bovik | VMAF CI (model uncertainty, Q5-5); subject test-retest (Q5-7) |
| N4 | VFI self-diagnosis from the interpolator's internal decision traces (SAD, warp weights, consistency checks) | 17 searches across uncertainty / confidence / error prediction for flow, VFI and stereo | Stereo confidence from own outputs (Q4-5…Q4-7); VFI error from own flow (Q4-2) |
| N5 | A named position-error column in pixels in a standard VFI benchmark table | Middlebury, Kiefhaber, BVI-VFI, MSU inspected | Middlebury EPE for flow (not for the interpolated frame); PSNR by region |
| N6 | A stated pixel-scale sensitivity floor for LPIPS / DISTS / FloLPIPS / VFIPS | FloLPIPS full text searched for pixel/subpixel statements; shift-tolerant LPIPS; Alabau-Bosque 2024 abstract | Qualitative only: LPIPS registers "imperceptible" shifts (Q2-12); DISTS forgives translation (Q2-4) |
| N7 | A VFI paper evaluating with KID or CMMD | "frame interpolation diffusion model FID OR KID evaluation table generative"; VIDIM; LDMVFI | VIDIM uses FID + FVD (Q1-10); LDMVFI uses LPIPS + FloLPIPS (Q1-11) |

(`FG_METRIC_MODEL_PRIOR_ART.md` §4, rows N1–N7)

*Made with my soul - Swately <3*
