# FG_METRIC_MODEL_PRIOR_ART.md — KAP Phase 0 anchor table: could PhyriadFG's tuning yield its own FID/CMMD/IS/LPIPS-class model?

Status: `0.1.0-experimental` · Type: **Analysis** (prior-art dossier; the Phase 0 output artifact of
`F:\Phyriad\protocols\analysis\KNOWLEDGE_ANALYSIS_PROTOCOL.md` §3.0 — a SOTA anchor table, claim → source
→ V-level). Honest as of access date **2026-09-09**. Nothing here is frozen: Phase 1 (the frozen identity)
is the operator's decision and §6 only proposes candidates for it.

**The question (the operator, 2026-09-09, verbatim):** *"es probable que la afinacion del FG nos pueda
llevar a un modelo matematico tipo FID, CMMD, IS, LPIPS, si podemos crear o interpretar nuestro propio
modelo, seria muy bueno, asi que tomalo en cuenta y analiza como podriamos llegar a un trabajo asi para un
posible trabajo de investigacion"* — could tuning the FG lead to a mathematical model of the FID / CMMD /
IS / LPIPS kind, created or interpreted as our own, and how would one reach a research paper from it.

**Scope.** Five questions, one per sweep: **Q1** the distribution-metric family (FID, IS, KID, CMMD, FVD,
improved precision/recall) — what each measures, its documented failure modes, its use for VFI, and whether
a distribution metric is a valid measure of interpolation *fidelity*; **Q2** the full-reference perceptual
family for VFI (LPIPS, DISTS, FloLPIPS, VFIPS, PSNR_DIV) and its benchmarks — re-verifying the June 2026
dossiers and what is new since; **Q3** synthetic / analytic ground truth — whether any prior work scores an
interpolation at an arbitrary continuous phase against analytic truth, and the sim-to-real objection;
**Q4** reference-free error prediction and self-diagnosis from an algorithm's own decisions; **Q5**
error decomposition (geometric vs appearance, region / visibility classes), "hallucination" as a named
failure mode, and reliability-reporting practice.

**Non-scope.** No implementation, no new measured number: the FG's own numbers in §5 are quoted from the
evidence documents that own them and linked, never re-established. Not a Phase 1 document — §6 proposes,
the operator freezes. The four June dossiers this one cross-links are not rewritten; their defects found
here are recorded in §3 and in `../LEARNING_LOG.md` (P-025), not silently patched.

**Method (KAP §3.0 + SUBAGENT_DELEGATION R5).** Five Sonnet sweeps (Q1–Q5, web access, one anchor table
each: claim / source / verbatim quote / how verified / V-level / relevance) → five Opus VISTA T0 gates
(clean context, the T0 template verbatim with mode-lock; each gate opened every URL of its sweep and
compared every quote literally) → this synthesis by the supervisor, who **re-verified first-hand** (§9.2
"a delegated verdict is itself delegated work") the sources the argument rests on: 16 URLs opened today,
two venue searches, the BVI-VFI TIP paper read from the PDF (pp. 9–12) and its HTML Table II. Workflow
`wf_c44d141d-43b`, 10 agents, 0 errors, 267 tool uses, 851,889 subordinate tokens, 519 s; journal
`C:\Users\Swately\.claude\projects\F--Phyriad\33534136-64c5-4b1d-b3f2-483dd35195b5\subagents\workflows\wf_c44d141d-43b\journal.jsonl`.
**All five T0 verdicts: `SWEEP VERIFIED WITH GAPS`, zero fabrications.** The gates' findings (quote drift,
claim over-reach, one wrong venue) are in §3; the levels below are THIS dossier's, per the June convention:
**[V1]** the supervisor opened the primary source (today, or a June 2026 session — the date column says
which); **[V2]** primary source reached by the sweep AND the gate, not by the supervisor, or reached but
the fact is paper-body / a figure not read; **[V3]** secondary only. A sweep's own "V1" is context, not a
level of this dossier.

**Companions (the single home of each fact — this dossier links, does not copy):**
[`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) (F1–F7: the held-out protocol, PSNR/SSIM ≪ perceptual,
FloLPIPS/VFIPS, BVI-VFI, VFIPQA), [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md) (§2 Middlebury
and the synthetic route, §3 the metric stack, DIV/TS), [`FG_PERCEPTUAL_EVAL_SOTA.md`](FG_PERCEPTUAL_EVAL_SOTA.md)
(§2F calibration, §2G "the eye-proxy is an OPEN problem"), [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md);
the instrument and its rows: [`../evidence/B1_FIRST_FG_ROW.md`](../evidence/B1_FIRST_FG_ROW.md),
[`../evidence/B1_SPEED_TEST.md`](../evidence/B1_SPEED_TEST.md), [`../planning/records/GDUMP_GATE.md`](../planning/records/GDUMP_GATE.md),
[`../planning/GDUMP_PLAN.md`](../planning/GDUMP_PLAN.md), `../../tools/scene_truth/`.

---

## §1 — Verdict (the synthesis; every sentence rests on a §2 row)

1. **The FID family answers a different question than fidelity.** FID, IS, KID, CMMD, FVD and improved
   precision/recall are all computed over a SET of outputs against a SET of real samples (IS over the
   output set alone) and collapse to one or two scalars per batch; none pairs a generated frame with its
   own ground truth (Q1-1…Q1-7). Two primary sources show the blindness is active, not latent: FVD
   "increases only slightly with large temporal corruption" and can be driven down by sampling
   *motionless* videos (Q1-9, CVPR 2024); a diffusion VFI paper states in its own words that "FID does not
   consider any aspects of temporal consistency and only considers each frame individually" and that
   generative metrics are reported precisely because they "do not penalize plausible extrapolations that
   differ from the ground truth" (Q1-10). CMMD fixes FID's estimator (normality, sample complexity), not
   its level (Q1-4). **A model "of the FID/CMMD/IS type" is therefore not the family for a fidelity claim**
   ("the right content at the right place at the right time"); it would measure the realism of the FG's
   output distribution, which is a different, valid, and much weaker question for a frame generator whose
   input is the truth two frames away. No VFI paper using KID or CMMD was found (§4-N7).
2. **The full-reference perceptual family measures agreement with human opinion, and is capped there.**
   LPIPS learns a distance from human 2AFC judgments (Q2-1); FloLPIPS re-weights it by optical-flow
   discrepancy (Q2-2); VFIPS models the video with a Swin spatio-temporal net (Q2-3); PSNR_DIV (QoMEX 2025,
   v2 January 2026) weights PSNR by motion-field divergence and reports +0.09 PLCC over FloLPIPS at 2.5×
   the speed and 4× less memory on 180 BVI-VFI sequences (Q2-10). On the full BVI-VFI (540 sequences, 189
   subjects, 33 metrics) the authors conclude that "none of the tested metrics exhibit satisfactory overall
   correlation with the subjective quality scores", the best being FAST at SRCC 0.70 (Q2-6). **Correction
   to the June synthesis (§3-C3):** on that full database PSNR is the second-best metric overall (SRCC
   0.65) and outranks FloLPIPS (0.61) and LPIPS (0.56); the ordering "PSNR/SSIM ≪ LPIPS ≪ bespoke" holds on
   the 180-sequence ICIP set (LPIPS 0.599 vs PSNR 0.520) and inverts on the full set. The ordering is
   dataset-fragile, not robust.
3. **None of these metrics is built for the FG's error scale, and two are built against it.** No source
   states a smallest detectable spatial error for LPIPS / DISTS / FloLPIPS / VFIPS (§4-N6); DISTS is
   designed to be "relatively insensitive to geometric transformations (e.g., translation and dilation)"
   (Q2-4), and the shift-tolerant LPIPS variant exists because the original is "sensitive to a small
   alignment error that is imperceptible to the human eyes" and the authors wanted that sensitivity
   *removed* (Q2-12). The FG's measured errors are 0.2–0.5 px of position (§5): exactly the band the
   perceptual family either cannot resolve or deliberately forgives.
4. **Exact-truth evaluation exists; evaluation at the generator's own phase does not.** Synthetic
   benchmarks with per-pixel truth are standard — Sintel, FlyingThings3D ("complete even in occluded
   regions since the render engine always has full knowledge"), Spring, Kubric/MOVi, Middlebury's synthetic
   set (Q3-1…Q3-5) — and every one of them evaluates at fixed rendered or lerped time points. The nearest
   neighbour to `scene_truth` is Kiefhaber, Niklaus, Liu & Schaub-Meyer 2024: 666 nonuplets of 2-D sprites
   under homographies that "strictly follow the constraint of linearity", seven in-betweens obtained by
   "lerping", error split into 0-occ / 1-occ / 2-occ, and the word "hallucinate" for the 2-occ case (Q3-6).
   It is 2-D, on a fixed 7-point grid, with PSNR/SSIM as the score. Three bounded absences (§4): no prior
   work re-evaluates an analytic 3-D scene at the phase the generator actually produced (N1); none uses the
   four-way visibility taxonomy both / only-A / only-B / neither (N2); none reports a named position-error
   column in pixels (N5).
5. **Decomposition has precedent; the FG's particular decomposition does not.** Middlebury reports the
   same error over three region masks (All / Disc / Untext) and excludes semi-occluded pixels from the
   interpolation truth (Q5-1, Q5-2); DISTS separates a texture term from a structure term but sums them
   into one scalar (Q5-3); Daly et al. 2025 gate the appearance metric by motion integrity (Q5-6);
   "hallucination" is a named, separately-scored failure mode in 2025 super-resolution work (Q5-4). No
   source reports position, shape, hallucinated mass and a signed lead as separate named outputs with the
   disocclusion bucket apart.
6. **Reference-free self-diagnosis exists for stereo, not for interpolation from its own decisions.**
   Per-pixel confidence learned from a stereo matcher's own outputs, without ground truth, is established
   (Q4-5…Q4-7, all abstract-level); for VFI the found precedents predict error from the *flow* the
   interpolator computed (Q4-2) or from the result's perceptual features (Q4-3), never from the
   interpolator's internal decision traces — SAD, warp weights, stasis, guided picks (§4-N4).
7. **Reliability of the metric itself is not reported in this field.** VMAF ships a 95 % confidence
   interval, but it is model uncertainty from bootstrapping the training ratings, not run-to-run variance
   (Q5-5 — the sweep's "run-to-run reliability" reading was wrong, §3-C4); test-retest appears for human
   subjects, not for metrics (Q5-7). No source reports a DI-3-style run-to-run r for a metric (§4-N3).
8. **The sim-to-real objection is live and recent.** ECCV 2026: "progress on Sintel, KITTI and Spring only
   weakly predicts accuracy on real-world data" (Q3-7). Any claim built on analytic truth carries it; the
   FG's content is a rendered game, which is closer to the synthetic side than that paper's targets, and
   that is an argument to make, not to assume.

**What PhyriadFG holds that the swept literature does not (each measured, §5):** (i) analytic 3-D truth
at the generator's own phase; (ii) a decomposition into named terms with the disocclusion bucket apart
and a signed lead; (iii) every-tick provenance of the kernel's own decisions; (iv) a run-to-run r on every
number. **What it lacks that the literature has:** any link to human perception (no MOS on FG output);
real content; more than one scene family; more than one kernel under test. **Reading of the question:**
"interpret our own model" is already true in the structural sense — the decomposition IS a model of the
error, computed not learned; "create our own model" in the LPIPS sense (a learned function) is I-B in §6,
and its supervisory signal would be exact truth where LPIPS's is human opinion. The FID sense is refuted
for fidelity by rows Q1-9 and Q1-10.

---

## §2 — The anchor table (claim → source → level)

Columns: **id** · **claim (with the quote this dossier relies on)** · **source** · **level / who opened
it / date** · **note** (gate finding or supervisor correction). Quotes are verbatim from the fetched text;
where a gate found the sweep's quote drifted, the corrected wording is used here.

### Q1 — the distribution-metric family

| id | claim | source | level | note |
|---|---|---|---|---|
| Q1-1 | FID is a set-vs-set distance on Inception-v3 features: "we introduce the "Fréchet Inception Distance" (FID) which captures the similarity of generated images to real ones better than the Inception Score." Needs a real set; no per-sample pairing. | Heusel, Ramsauer, Unterthiner, Nessler, Hochreiter — *GANs Trained by a Two Time-Scale Update Rule Converge to a Local Nash Equilibrium*, NIPS 2017, https://arxiv.org/abs/1706.08500 | **[V1]** supervisor 2026-09-09 (abstract) | The "Gaussian-fitted feature distributions" mechanism is paper-body, not in the abstract (gate). |
| Q1-2 | IS scores a generated set alone by the entropy of a classifier's label posterior; reference-free, distribution-level: "Images that contain meaningful objects should have a conditional label distribution p(y\|x) with low entropy … the marginal … should have high entropy." | Salimans, Goodfellow, Zaremba, Cheung, Radford, Chen — *Improved Techniques for Training GANs*, NIPS 2016, https://arxiv.org/abs/1606.03498 | **[V2]** sweep + gate (HTML body, literal match) | — |
| Q1-3 | KID: "We also propose an improved measure of GAN convergence, the Kernel Inception Distance"; an MMD-based set-vs-set distance. | Bińkowski, Sutherland, Arbel, Gretton — *Demystifying MMD GANs*, ICLR 2018, https://arxiv.org/abs/1801.01401 | **[V1]** supervisor 2026-09-09 (abstract) | The "unbiased polynomial-kernel estimator" detail is paper-body; the abstract's unbiasedness sentence is about gradient estimators (gate). |
| Q1-4 | CMMD replaces FID's estimator, not its level: "based on richer CLIP embeddings and the maximum mean discrepancy distance with the Gaussian RBF kernel. It is an unbiased estimator that does not make any assumptions on the probability distribution of the embeddings and is sample efficient." | Jayasumana, Ramalingam, Veit, Glasner, Chakrabarti, Kumar — *Rethinking FID: Towards a Better Evaluation Metric for Image Generation*, arXiv v2 2024-01-25, https://arxiv.org/abs/2401.09603 | **[V1]** supervisor 2026-09-09 (abstract) | "CVPR 2024" is the sweep's attribution; the arXiv page shows no venue line — venue **[V3]**. |
| Q1-5 | FID's documented failure modes, primary source: "Inception's poor representation of the rich and varied content generated by modern text-to-image models, incorrect normality assumptions, and poor sample complexity … FID contradicts human raters, it does not reflect gradual improvement of iterative text-to-image models, it does not capture distortion levels, and that it produces inconsistent results when varying the sample size." | same as Q1-4 | **[V1]** supervisor 2026-09-09 | — |
| Q1-6 | FVD extends FID to video clips; motivated by "(1) the lack of qualitative metrics that consider visual quality, temporal coherence, and diversity of samples, and (2) the wide gap between purely synthetic video data sets and challenging real-world data sets"; "we propose Fréchet Video Distance (FVD), a new metric for generative models of video". Set-level, needs a real video set. | Unterthiner, van Steenkiste, Kurach, Marinier, Michalski, Gelly — *Towards Accurate Generative Models of Video: A New Metric & Challenges*, arXiv 1812.01717 (v2 2019-03), https://arxiv.org/abs/1812.01717 | **[V1]** supervisor 2026-09-09 (abstract) | The I3D-feature detail is paper-body. Venue: the arXiv page carries none; a workshop version "FVD: A New Metric for Video Generation", ICLR 2019 workshop, appears in search summaries only — **[V3]**. |
| Q1-7 | Improved precision/recall splits one scalar into fidelity and coverage, still set-vs-set: "an evaluation metric that can separately and reliably measure both of these aspects … by forming explicit, non-parametric representations of the manifolds of real and generated data"; the abstract also extends it "to estimate the perceptual quality of individual samples". | Kynkäänniemi, Karras, Laine, Lehtinen, Aila — *Improved Precision and Recall Metric for Assessing Generative Models*, NeurIPS 2019, https://arxiv.org/abs/1904.06991 | **[V1]** supervisor 2026-09-09 (abstract) | The per-sample extension is a realism score against the real manifold, not a comparison with that sample's own truth. |
| Q1-8 | IS "fails to provide useful guidance when comparing models. We discuss both suboptimalities of the metric itself and issues with its application." | Barratt, Sharma — *A Note on the Inception Score*, ICML 2018 workshop (Theoretical Foundations and Applications of Deep Generative Models), https://arxiv.org/abs/1801.01973 | **[V2]** sweep + gate | Sweep wrote "arXiv preprint"; the comments line names the workshop (gate). |
| Q1-9 | FVD is biased to per-frame quality and nearly blind to motion: "we find that the FVD increases only slightly with large temporal corruption … via careful sampling from a large set of generated videos that do not contain motions, one can drastically decrease FVD without improving the temporal quality. Both studies suggest FVD's bias towards the quality of individual frames." | Ge, Mahapatra, Parmar, Zhu, Huang — *On the Content Bias in Fréchet Video Distance*, CVPR 2024, https://arxiv.org/abs/2404.12391 | **[V1]** supervisor 2026-09-09 (abstract; comments line "CVPR 2024") | The most direct primary evidence for §1-1. |
| Q1-10 | A diffusion VFI paper reports PSNR, SSIM, LPIPS, FID and FVD and states: "We additionally report more popular metrics for generative models (specifically, FID and FVD), which, unlike reconstruction metrics, do not penalize plausible extrapolations that differ from the ground truth." and "This is an important quantity to consider, as FID does not consider any aspects of temporal consistency and only considers each frame individually." | Jain et al. (Google Research) — *Video Interpolation with Diffusion Models* (VIDIM), arXiv 2024, §4.2, https://arxiv.org/html/2404.01203v1 | **[V1]** supervisor 2026-09-09 (HTML §4.2, both sentences located) | The sweep's bracketed rendering "[generative metrics] do not penalize" dropped "which," (gate); the full sentence is used here. Author list not re-read. |
| Q1-11 | Not every generative VFI paper adopts the FID family: LDMVFI evaluates with LPIPS and FloLPIPS, "These metrics have shown superior correlation with human judgments of VFI quality compared to commonly used quality measurements, PSNR and SSIM". | Danier, Zhang, Bull — *LDMVFI: Video Frame Interpolation with Latent Diffusion Models*, AAAI 2024, https://arxiv.org/abs/2303.09508 | **[V2]** sweep + gate (HTML v2) | A self-correction inside the sweep: a search snippet had implied LDMVFI reports FID; it does not. |

### Q2 — the full-reference perceptual family and its benchmarks

| id | claim | source | level | note |
|---|---|---|---|---|
| Q2-1 | LPIPS is a learned distance calibrated on human judgments: "we introduce a new dataset of human perceptual similarity judgments … We find that deep features outperform all previous metrics by large margins on our dataset." | Zhang, Isola, Efros, Shechtman, Wang — *The Unreasonable Effectiveness of Deep Features as a Perceptual Metric*, CVPR 2018, https://arxiv.org/abs/1801.03924 | **[V1]** supervisor 2026-09-09 (abstract) | Not in the June tables; added here. |
| Q2-2 | FloLPIPS = LPIPS re-weighted by optical-flow discrepancy: "FloLPIPS combines the spatial distortion captured by LPIPS with the temporal degradation estimated using the discrepancy between the optical flow maps of the reference and distorted videos"; computed "for every two frames in a sliding window with a stride of 1"; needs reference + distorted video + flow. No pixel-sensitivity statement anywhere in the text (verified absence). | Danier, Zhang, Bull — *FloLPIPS: A Bespoke Video Quality Metric for Frame Interpolation*, **IEEE Picture Coding Symposium (PCS) 2022, pp. 283–287, best-paper finalist**, https://arxiv.org/abs/2207.08119 · https://github.com/danier97/flolpips | **[V1]** June 2026-06-13 (abstract, mechanism) + supervisor 2026-09-09 (venue: the authors' repository tagline "[IEEE PCS 2022 best paper finalist]"; Edinburgh Research Explorer; the PCS 2022 proceedings ToC) | **Venue correction, §3-C1**: the sweep wrote "ICIP 2022 per IEEE Xplore listing" (unsupported) and `FG_VFI_MEASUREMENT_SOTA.md` carries the same wrong venue twice as [V1]. The sliding-window and mechanism quotes are HTML full text (sweep + gate). |
| Q2-3 | VFIPS is full-reference video-to-video: "It takes an interpolated video and its reference ground-truth video as input and outputs the perceptual similarity between them" (Introduction, p. 2); trained on 25,887 triplets; image metrics' "performance on videos is compromised since they do not consider temporal information." | Hou, Ghildyal, Liu — *A Perceptual Quality Metric for Video Frame Interpolation*, ECCV 2022, https://arxiv.org/abs/2210.01879 | **[V1]** June 2026-06-13 (abstract, the VFI-specific/temporal claim); the p. 2 sentence and the 25,887 figure **[V2]** (gate read the arXiv PDF) | The arXiv PDF is image-embedded; the sweep fell back to a Hugging Face mirror, the gate read the PDF. |
| Q2-4 | DISTS combines "correlations of these spatial averages ("texture similarity") with correlations of the feature maps ("structure similarity")", with "explicit tolerance to texture resampling" and is "relatively insensitive to geometric transformations (e.g., translation and dilation)". | Ding, Ma, Wang, Simoncelli — *Image Quality Assessment: Unifying Structure and Texture Similarity*, IEEE TPAMI 44(5) 2022 (arXiv 2020), https://arxiv.org/abs/2004.07728 | **[V2]** sweep + gate (abstract, two independent fetches) | The geometric tolerance is the opposite of what a 0.2–0.5 px error scale needs (§1-3). |
| Q2-5 | BVI-VFI, the subjective study (ICIP 2022): 36 references × 3 frame rates, 180 distorted videos, 5 VFI algorithms, 60 participants, 8 metrics; "none of these metrics provide acceptable correlation with the perceived quality on interpolated content, with the best-performing metric, LPIPS, offering a SROCC value below 0.6"; "an urgent need to develop a bespoke perceptual quality metric for VFI". Table 2 (PLCC / SROCC): PSNR 0.471 / 0.520, SSIM 0.475 / 0.581, MS-SSIM 0.529 / 0.593, **LPIPS 0.597 / 0.599**, VIF 0.489 / 0.535, FRQM 0.456 / 0.535, ST-GREED 0.214 / 0.112, VMAF 0.564 / 0.595. | Danier, Zhang, Bull — *A Subjective Quality Study for Video Frame Interpolation*, ICIP 2022 (DOI 10.1109/ICIP46576.2022.9897364), https://arxiv.org/abs/2202.07727 | **[V1]** supervisor 2026-09-09 (abstract; Table 2 via the arXiv HTML rendering) | **This** is the paper the June `FG_PERCEPTUAL_EVAL_SOTA.md` §2G numbers come from (§3-C2). The sweep searched for "SROCC < 0.6" in the TIP abstract and reported it unverified; it is in this abstract. |
| Q2-6 | BVI-VFI, the database (TIP 2023): "540 distorted sequences generated by applying five commonly used VFI algorithms to 36 diverse source videos … more than 10,800 quality ratings … 189 human subjects"; 33 metrics benchmarked. Section V.A: "none of the tested metrics exhibit satisfactory overall correlation with the subjective quality scores", "the best-performing metric, FAST, achieving a SRCC value of only 0.70", "the metrics most commonly used in the VFI literature (PSNR, SSIM and LPIPS) all achieved SRCC values below 0.65". Table II, Overall (SRCC / KRCC / PLCC / RMSE): FAST 0.70 / 0.50 / 0.63 / 14.54; PSNR 0.65 / 0.45 / 0.58 / 15.16; SpEED 0.64 / 0.45 / 0.57 / 15.37; GMSD 0.63 / 0.43 / 0.57 / 15.36; MS-SSIM 0.62 / 0.43 / 0.54 / 15.67; FloLPIPS 0.61 / 0.43 / 0.58 / 15.26; SSIM 0.60 / 0.42 / 0.54 / 15.74; VMAF 0.58 / 0.40 / 0.52 / 15.97; DISTS 0.57 / 0.40 / 0.55 / 15.54; LPIPS 0.56 / 0.39 / 0.52 / 16.00. | Danier, Zhang, Bull — *BVI-VFI: A Video Quality Database for Video Frame Interpolation*, IEEE TIP 2023, https://arxiv.org/abs/2210.00823 | **[V1]** June 2026-06-13 (abstract counts) + supervisor 2026-09-09 (PDF pp. 9–12 read; Table II digits confirmed against the arXiv HTML rendering; the three V.A sentences located) | **Refutes the June ordering claim, §3-C3.** FAST (Xu et al.) compares moving parts along motion trajectories and adds a flow-discrepancy term — the same family of idea as FloLPIPS, ranked above it here. |
| Q2-7 | Vimeo-90K: "we build Vimeo-90K, a large-scale, high-quality video dataset for low-level video processing"; IJCV 127(8) 2019. | Xue, Chen, Wu, Wei, Freeman — *Video Enhancement with Task-Oriented Flow*, IJCV 2019, https://arxiv.org/abs/1711.09078 | **[V2]** sweep + gate (abstract; the 89,800 figure is body, gate read it) | Author order corrected by the sweep (Tianfan Xue first). |
| Q2-8 | X4K1000FPS: "a dataset (X4K1000FPS) of 4K videos of 1000 fps with the extreme motion"; dense REAL capture, not rendered. | Sim, Oh, Kim — *XVFI: eXtreme Video Frame Interpolation*, ICCV 2021 (Oral), https://arxiv.org/abs/2103.16206 | **[V1]** June 2026 (dataset row); the real-camera-capture characterization **[V2]** (search summaries; PDF not opened by anyone) | Relevant to §4-N1: "arbitrary time" here means dense real sampling. |
| Q2-9 | SNU-FILM (Easy/Medium/Hard/Extreme) was built from 240 fps footage because dropping frames from 30 fps sources produces unrealistic time gaps. | Choi, Kim, Han, Xu, Lee — *Channel Attention Is All You Need for Video Frame Interpolation*, AAAI 2020, https://cdn.aaai.org/ojs/6693/6693-13-9922-1-10-20200521.pdf | **[V2]** gate read the AAAI PDF p. 10667 (the sweep had not) | — |
| Q2-10 | **New since June.** PSNR_DIV: "Metrics like PSNR, SSIM and LPIPS ignore temporal coherence"; "enhances PSNR through motion divergence weighting, a technique adapted from archival film restoration"; on BVI-VFI (180 sequences) "+0.09 Pearson Linear Correlation Coefficient over FloLPIPS, while being 2.5× faster and using 4× less memory"; "robust to the motion estimator used"; usable "as a loss function". | Daly, Ramsook, Kokaram — *An Efficient Quality Metric for Video Frame Interpolation Based on Motion-Field Divergence*, IEEE QoMEX 2025 accepted manuscript (arXiv v1 2025-10-01, v2 2026-01-22), https://arxiv.org/abs/2510.01361 · code github.com/conalld/psnr-div | **[V1]** supervisor 2026-09-09 (abstract, complete) | The strongest live prior-art thread for a motion-weighted fidelity metric; note it uses an ESTIMATED motion field and human MOS as the target. |
| Q2-11 | The earlier conference version: DIV "correlates reasonably with these perceptual scores (PLCC=0.51) and is more computationally efficient (x2.7 speedup) compared to FloLPIPS"; the motion metrics "tend to favour more perceptual pleasing interpolated frames that may not score highly in terms of PSNR or SSIM". | Daly, Ramsook, Kokaram — *Efficient motion-based metrics for video frame interpolation*, SPIE Applications of Digital Image Processing XLVIII 2025, https://arxiv.org/abs/2508.09078 | **[V1]** June 2026 (`FG_VFI_MEASUREMENT_SOTA.md` §3, 112.4 ms/frame, ~2.7×) + gate (abstract literal) | — |
| Q2-12 | LPIPS-class metrics register imperceptible misalignment: "Existing perceptual similarity metrics assume an image and its reference are well aligned. As a result, these metrics are often sensitive to a small alignment error that is imperceptible to the human eyes." No numeric pixel threshold is stated. | Ghildyal, Liu — *Shift-tolerant Perceptual Similarity Metric*, ECCV 2022, https://arxiv.org/abs/2207.13686 | **[V1]** supervisor 2026-09-09 (abstract, complete) | The direction the FG needs (sub-pixel sensitivity) is what this paper removes. |
| Q2-13 | MSU VFI benchmark: PSNR, SSIM, MS-SSIM, VMAF, LPIPS as objective metrics; subjective study of 413 participants, 32 pairs each, Bradley-Terry. | MSU Graphics & Media Lab, https://videoprocessing.ai/benchmarks/video-frame-interpolation-methodology.html | **[V2]** sweep + gate (live page); June [V2] | First-party, not peer-reviewed. |

### Q3 — synthetic / analytic ground truth and the sim-to-real objection

| id | claim | source | level | note |
|---|---|---|---|---|
| Q3-1 | MPI-Sintel: "The dataset contains flow fields, motion boundaries, unmatched regions, and image sequences." Rendered from the open-source film; fixed frames. | Butler, Wulff, Stanley, Black — MPI-Sintel (ECCV 2012), http://sintel.is.tue.mpg.de/about | **[V2]** sweep + gate (page) | — |
| Q3-2 | Spring: "photo-realistic HD datasets with state-of-the-art visual effects and ground truth training data" from the Blender movie; the body claims (3-D motion vectors from a modified render pipeline, double resolution) were not opened by anyone. | Mehl et al. — *Spring: A High-Resolution High-Detail Dataset and Benchmark for Scene Flow, Optical Flow and Stereo*, CVPR 2023, https://arxiv.org/abs/2303.01943 | **[V2]** abstract only (PDF over the fetch size limit) | — |
| Q3-3 | Kubric: "A data generation pipeline for creating semi-realistic synthetic multi-object videos with rich annotations such as instance segmentation masks, depth maps, and optical flow." (PyBullet + Blender; MOVi-A..F under challenges/movi.) | Greff et al. — google-research/kubric (CVPR 2022), https://github.com/google-research/kubric | **[V2]** sweep + gate (README; MOVi confirmed one level deeper by the gate) | Truth per rendered frame at the simulation's own rate. |
| Q3-4 | FlyingThings3D: "the information is complete even in occluded regions since the render engine always has full knowledge about all (visible and invisible) scene points" (§4, p. 3). | Mayer et al. — *A Large Dataset to Train Convolutional Networks for Disparity, Optical Flow, and Scene Flow Estimation*, CVPR 2016, https://arxiv.org/pdf/1512.02134 | **[V2]** sweep + gate (PDF pp. 1–3) | Anticipates the visibility classes at the level of "the renderer knows the occluded pixels". |
| Q3-5 | Middlebury (2007/2011): region masks "everywhere (All), around motion discontinuities (Disc), and in textureless regions (Untext)"; interpolation error = "the root-mean-square (RMS) difference between the ground-truth image and the estimated interpolated image" (§4.1); "For the interpolation ground truth, we did exclude these regions because the baseline interpolation algorithm does not reason about these areas" (§4.3). | Baker, Scharstein, Lewis, Roth, Black, Szeliski — *A Database and Evaluation Methodology for Optical Flow*, ICCV 2007 / IJCV 92(1) 2011, https://vision.middlebury.edu/flow/floweval-ijcv2011.pdf | **[V1]** June 2026 (`FG_VFI_MEASUREMENT_SOTA.md` §2: the hold-out and synthetic routes, ICCV 2007 PDF); the §4.1/§4.3 sentences **[V2]** (gate rendered p. 16) | The sweep's quote dropped "did" and inserted an ellipsis (gate); corrected here. Untext is motivated by flow difficulty, and the paper says interpolating there "should be easier" — not "fails differently" (gate). |
| Q3-6 | Kiefhaber et al. 2024: "Our benchmark consists of 666 nonuplets at up to 4K resolution, which are synthetically rendered and thus allow us to conform to the constraint of linear motion."; motion by "random homography transforms that strictly follow the constraint of linearity"; "we perform lerping to obtain the sprites and background for the in-between times"; "we separately assess non-occluded areas (0-occ.), areas occluded in one input (1-occ.), and areas occluded in both inputs (2-occ.)"; "After all, if an area is occluded in both inputs, the interpolation method needs to hallucinate the content."; scores PSNR, PSNR\*, SSIM, PSNRσ\*. | Kiefhaber, Niklaus, Liu, Schaub-Meyer — *Benchmarking Video Frame Interpolation*, arXiv 2403.17128 (2024-03-25; no venue line), https://arxiv.org/html/2403.17128 | **[V1]** June 2026 (`FG_VFI_MEASUREMENT_SOTA.md` row 4: strict linearity) + supervisor 2026-09-09 (HTML: all five quotes located) | **The nearest neighbour to `scene_truth`** on three axes at once (synthetic truth, linearity by construction, visibility classes) — 2-D sprites, seven fixed in-betweens, occlusion count only. |
| Q3-7 | **New since June.** Sim-to-real for motion: "Our experiments show that progress on Sintel, KITTI and Spring only weakly predicts accuracy on real-world data, highlighting the need for a broad real-world optical flow benchmark."; benchmark of 8,204 real frame pairs incl. FlowFactor (1,000 HD pairs, four confounders: large displacements, repetitive textures, occlusions, lighting). | Reijalt, Gielisse, Karlsson, van Gemert — *On the Real-World Generalisability of Optical Flow Models*, ECCV 2026 (arXiv 2607.10470, submitted 2026-07-11), https://arxiv.org/abs/2607.10470 | **[V1]** supervisor 2026-09-09 (abstract, complete; comments "Accepted @ ECCV 2026") | Flow, not VFI; no VFI-specific sim-to-real source was found (sweep gap). |

### Q4 — reference-free error prediction and self-diagnosis (all abstract-level)

| id | claim | source | level | note |
|---|---|---|---|---|
| Q4-1 | A flow network can "estimate their local uncertainty about the correctness of their prediction" in one forward pass (multi-hypothesis, winner-takes-all). | Ilg et al. — *Uncertainty Estimates and Multi-Hypotheses Networks for Optical Flow*, ECCV 2018, https://arxiv.org/abs/1802.07095 | **[V2]** | — |
| Q4-2 | VFI error predicted from the interpolator's own flow: "By closely examining the correlation between optical flow and IE, the paper proposes novel error prediction metrics that partition the middle frame into distinct regions corresponding to different IE levels." | Chi et al. — *Error-Aware Spatial Ensembles for Video Frame Interpolation*, arXiv 2207.12305 (2022), https://arxiv.org/abs/2207.12305 | **[V2]** | Closest VFI precedent to I-B (§6): the predictor's input is the flow, not the kernel's decisions. |
| Q4-3 | VFIPQA is no-reference w.r.t. the missing frame but conditions on the two real inputs: "The proposed method evaluates the perceptual coherence of frames incorporating the original pair of VFI inputs". | Han et al. — *Perceptual Quality Assessment for Video Frame Interpolation*, arXiv 2312.15659 (2023), https://arxiv.org/abs/2312.15659 | **[V2]** (June [V2] too) | The sweep's "(VFIPQA)" acronym in the title is editorial (gate). |
| Q4-4 | MMD: "the largest difference in expectations over functions in the unit ball of a reproducing kernel Hilbert space (RKHS)" — the mechanism under KID and CMMD. | Gretton, Borgwardt, Rasch, Schölkopf, Smola — *A Kernel Two-Sample Test*, JMLR 13 (2012) 723–773, https://jmlr.org/beta/papers/v13/gretton12a.html | **[V2]** | — |
| Q4-5 | Stereo confidence learned without ground truth from "contradictions and consistencies between multiple depth maps generated with the same stereo algorithm". | Mostegel et al. — *Using Self-Contradiction to Learn Confidence Measures in Stereo Vision*, CVPR 2016, https://arxiv.org/abs/1604.05132 | **[V2]** | Supervisory signal only; the sweep's "input signal" reading is unsupported (gate). |
| Q4-6 | A dense confidence map from a learned Laplacian variance: "the variance in the Laplacian distribution is large for low confident pixels while small for high-confidence pixels." | *Confidence Inference for Focused Learning in Stereo Matching*, arXiv 1809.09758 (2018), https://arxiv.org/abs/1809.09758 | **[V2]** | — |
| Q4-7 | Self-supervised stereo confidence from "the minimum information available in any stereo setup (i.e., the input stereo pair and the output disparity map)". | Poggi et al. — *Self-adapting confidence estimation for stereo*, ECCV 2020, https://arxiv.org/abs/2008.06447 | **[V2]** | A lower-bound baseline for I-B: input pair + output only, no internal traces. |

### Q5 — decomposition, "hallucination", and reliability practice

| id | claim | source | level | note |
|---|---|---|---|---|
| Q5-1 | Middlebury conditions the SAME error by region (All / Disc / Untext); no separate geometric sub-metric in pixels. | as Q3-5 | see Q3-5 | — |
| Q5-2 | Middlebury excludes semi-occluded pixels from the interpolation truth (exclusion, not a bucket). | as Q3-5 | see Q3-5 | Contrast: `scene_truth` scores disocclusion as its own bucket with a signed lead. |
| Q5-3 | DISTS separates texture and structure terms internally and reports ONE scalar D(x,y). | as Q2-4 | **[V2]** | The sweep's "rather than reporting one global distortion number" is false (gate). |
| Q5-4 | "Hallucination" as a named, separately-measured failure mode: "generated details fail to perceptually match the low resolution image (LRI) or ground-truth image (GTI)" — a dedicated Hallucination Score (MLLM-prompted) because existing IQA does not characterize it. | *Hallucination Score: Towards Mitigating Hallucinations in Generative Image Super-Resolution*, arXiv 2507.14367 (2025), https://arxiv.org/abs/2507.14367 | **[V2]** | Super-resolution, not VFI; the word in VFI is Q3-6. |
| Q5-5 | VMAF's 95 % CI: "The CI is a consequence of the fact that the VMAF model is trained on a sample of subjective scores, while the population is unknown. The CI is established through bootstrapping using the full training data." | Netflix/vmaf `resource/doc/conf_interval.md` (feature since v1.3.7, June 2018), https://github.com/Netflix/vmaf/blob/master/resource/doc/conf_interval.md | **[V1]** supervisor 2026-09-09 (raw file) | Model uncertainty, **not** run-to-run reliability (§3-C4). The nearest deployed precedent to "a number with its uncertainty", and a different quantity from DI-3's r. |
| Q5-6 | Motion integrity gates the appearance metric: "Conversely we penalise the metric when that motion integrity is poor, because the underlying image would probably be of poor quality." (§3.3) | as Q2-11, https://arxiv.org/html/2508.09078v1 | **[V2]** (gate, two fetches) | The sweep dropped "that" (gate); corrected here. Position/appearance are distinct channels fused into one scalar. |
| Q5-7 | Test-retest reliability is applied to SUBJECTS, not metrics: "5 of each group of 43 test images were randomly presented twice to each subject … If the difference between the two ratings … exceeded a threshold on at least 3 of the 5 images, then that subject was rejected." (§IV-C-2) | Ghadiyaram, Bovik — *Massive Online Crowdsourced Study of Subjective and Objective Picture Quality*, IEEE TIP 2016, https://arxiv.org/abs/1511.02919 | **[V2]** (gate rendered p. 7) | — |

---

## §3 — Defects the audit found, and what this dossier does with them

- **C1 — FloLPIPS venue.** `FG_VFI_MEASUREMENT_SOTA.md` states "FloLPIPS (Danier/Zhang/Bull, ICIP 2022)"
  twice, tagged [V1] (§3 bullet and source table row 5); `FG_VFI_PRIOR_ART.md` states PCS 2022 (F3 and
  §7), also [V1]. The record contradicted itself and the sweep reproduced the wrong half with an invented
  justification ("per IEEE Xplore listing"). Settled today: **PCS 2022, pp. 283–287, best-paper finalist**
  (Q2-2). The June file is not edited (it is a dated record); this dossier and P-025 carry the correction.
- **C2 — BVI-VFI is two papers.** The 60-observer / 8-metric study with LPIPS 0.597 / 0.599 is the ICIP
  2022 paper (arXiv 2202.07727); the 540-sequence / 189-subject / 33-metric database is TIP 2023 (arXiv
  2210.00823). `FG_PERCEPTUAL_EVAL_SOTA.md` cites the right URL for its numbers but labels the row
  "BVI-VFI (ICIP 2022)", which reads as the database paper. Sweep 2 searched the TIP abstract for the
  ICIP number and reported it unverified.
- **C3 — the ordering claim is refuted on the full database.** `FG_VFI_MEASUREMENT_SOTA.md` §3 (honest
  limits box) and `FG_VFI_PRIOR_ART.md` F3 state that "the metric ordering (PSNR/SSIM ≪ LPIPS ≪
  bespoke-VFI) is robust; absolute SROCC is dataset-fragile". On TIP 2023 Table II Overall the order is
  FAST 0.70 > PSNR 0.65 > SpEED 0.64 > GMSD 0.63 > MS-SSIM 0.62 > FloLPIPS 0.61 > SSIM 0.60 > VMAF 0.58 >
  DISTS 0.57 > LPIPS 0.56 (SRCC). PSNR outranks both LPIPS and the bespoke FloLPIPS; the authors say PSNR is
  second-best and "not significantly better" than SSIM, SpEED and FovVideoVDP (Table IV F-tests). What
  survives: *no metric is satisfactory* (both papers, verbatim). What does not: any design argument that
  leans on LPIPS ≫ PSNR for VFI. Recorded as P-025; the container ledger's L-009 ("grep evidence dirs
  first") gets its second recurrence with a sharper form: the evidence dir must be read *for its
  contradictions*, not only for its presence.
- **C4 — sweep over-reach the gates caught (all corrected in §2):** DISTS "rather than one global number"
  (false — one scalar); VMAF CI as "run-to-run reliability" (false — bootstrap model uncertainty) and "one
  of the very few widely-deployed" (unsourced); Middlebury masks motivated by "where interpolation fails
  differently" (not the paper's motivation); FID's Gaussian mechanism, KID's polynomial kernel, FVD's I3D
  features — each true in the body, none in the cited abstract; "(VFIPQA)" appended to a title; the
  self-contradiction paper's signal called an "input".
- **C5 — quote fidelity.** Fourteen non-material deviations (dropped "did", "that", "which", "proposed";
  unmarked truncations; a parenthetical elided without ellipsis; capitalization at splice points), zero
  material ones, zero fabrications, across 55 claims. Every quote in §2 is either the gate's corrected
  wording or the supervisor's own fetch.
- **C6 — what the sweeps did not reach (their own declared gaps, consolidated):** no full PDF read for Ilg
  2018, Spring, XVFI, VFIPS's method section, CAIN (the gate read it), DAVIS J&F (Perazzi 2016, dropped);
  *Assessing invariance to affine transformations in image quality metrics* (Alabau-Bosque et al. 2024,
  arXiv 2407.17927) — likely to hold a numeric invisibility threshold — abstract only; AutoFlow, TAP-Vid,
  PointOdyssey not searched; no VFI-specific sim-to-real source; KID/CMMD/precision-recall in VFI papers not
  exhaustively searched; CMMD's venue not confirmed.

---

## §4 — FOUND NONE: the bounded absences (time-boxed searches, not proofs of absence)

| id | absence | searches (from the sweeps' own lists) | nearest neighbour found |
|---|---|---|---|
| N1 | Prior work scoring an interpolation at an **arbitrary continuous phase** against an **analytic 3-D scene** re-evaluable at any t. | "synthetic frame interpolation benchmark ground truth arbitrary time continuous phase"; "analytic ground truth OR closed-form ground truth video frame interpolation continuous timestep evaluation camera motion render"; direct inspection of Kiefhaber 2024 (7 fixed lerp points) and XVFI (real capture). | Kiefhaber 2024 (Q3-6); Middlebury's synthetic set re-rendered at t + 0.5 (`FG_VFI_MEASUREMENT_SOTA.md` §2). |
| N2 | The **four-way visibility taxonomy** (visible in both / only A / only B / neither). | as N1 plus the occlusion-class inspection of Middlebury and Kiefhaber. | Kiefhaber 0/1/2-occ (does not say which input); Middlebury occluded/unoccluded + Disc/Untext. |
| N3 | A VFI or generative-evaluation metric that reports its own **run-to-run reliability**. | "metric test-retest reliability video quality"; VMAF CI documentation; Ghadiyaram & Bovik. | VMAF CI (model uncertainty, Q5-5); subject test-retest (Q5-7). |
| N4 | VFI **self-diagnosis from the interpolator's internal decision traces** (SAD, warp weights, consistency checks). | 17 searches across uncertainty / confidence / error prediction for flow, VFI and stereo. | Stereo confidence from own outputs (Q4-5…Q4-7); VFI error from own flow (Q4-2). |
| N5 | A **named position-error column in pixels** in a standard VFI benchmark table. | Middlebury, Kiefhaber, BVI-VFI, MSU inspected. | Middlebury EPE for flow (not for the interpolated frame); PSNR by region. |
| N6 | A stated **pixel-scale sensitivity floor** for LPIPS / DISTS / FloLPIPS / VFIPS. | FloLPIPS full text searched for pixel/subpixel statements; shift-tolerant LPIPS; Alabau-Bosque 2024 abstract. | Qualitative only: LPIPS registers "imperceptible" shifts (Q2-12); DISTS forgives translation (Q2-4). |
| N7 | A VFI paper evaluating with **KID or CMMD**. | "frame interpolation diffusion model FID OR KID evaluation table generative"; VIDIM; LDMVFI. | VIDIM uses FID + FVD (Q1-10); LDMVFI uses LPIPS + FloLPIPS (Q1-11). |

---

## §5 — What PhyriadFG already holds against this map (quoted from the documents that own the numbers)

- **The instrument.** `tools/scene_truth/`: a 3-D scene rendered exactly at any t; a scorer that
  decomposes the error into named terms (position px, shape, hallucinated mass px², a signed lead) with a
  conjunctive verdict and the disocclusion bucket apart; the live bridge (barcoded corpus → the FG →
  `--qdump` sampler or the `--gdump` every-tick tap → `scene_align.py` → scored at the FG's own phase).
  Memory `phyriadfg-scene-truth.md`; gates seen red (`B1_FIRST_FG_ROW.md` §1).
- **The first DI-3 row against exact truth** (`../evidence/B1_FIRST_FG_ROW.md`, default kernel, k = 4
  from 60 fps, seeds 7 / 11): pos **0.216 / 0.212 px** (dev 2.2 %); `nearest` 0.275 / 0.279; the exact-flow
  `oracle2` 0.097 / 0.102; the translating sphere 0.481 / 0.468 against a 0.5 threshold; **"a second live run
  on a corpus of a different seed, same protocol, agrees on every citable term within 13 %"**. Error falls
  with phase on the fastest mover: φ < 0.25 → 0.668 / 0.653, 0.25–0.75 → 0.490 / 0.507, φ > 0.75 → 0.252 /
  0.221 — the M1_LOWPHASE signature reproduced by an instrument that did not know it.
- **The speed law** (`../evidence/B1_SPEED_TEST.md`): "pos ≈ 0.30 · disp^0.32" over seven live points
  to 13 px/pair; the k path is 1.10× (mid phase) to 1.72× (late phase) worse than the speed path at
  matched displacement AND phase — a source-rate term beyond exposure.
- **Provenance of the kernel's decisions** (`ref_warp.py --decisions`, identity gate byte-identical,
  `B1_SPEED_TEST.md` §5): on the four worst frames the guided MV pick "falls back to the bilinear MV on
  99 % of the sphere", stasis "fires on 18 % of the sphere" and paints it ahead — readings, not
  attributions; each names a live flag to A/B.
- **The every-tick tap** (`../planning/records/GDUMP_GATE.md`): `--gdump` captured "180 of the 180
  possible k=4 mids of the corpus in 8 s (the sampler gave ~133 in 20 s)"; `ref_warp` replays the record at
  "exact match mean 99.50 % (min 99.41)"; observer effect +0.06 ms GPU batch per tick, 0 rdrops; **the
  shipping async path's first row: pos 0.240 px, lead +8.93 — one run, DI-3's second run owed**.
- **What is missing, plainly:** no human-opinion data on FG output (the eye-proxy is OPEN,
  `FG_PERCEPTUAL_EVAL_SOTA.md` §2G); one scene family (`mixed`) at 640×360; one kernel; 1080p under the tap
  not measured; the async row unreplicated; a phase-dependent hallucination reading on a rotating cube
  (106.5 → 23.8 px² low → high phase, one run, 2026-09-09, not yet in an evidence document).

---

## §6 — Candidates for the Phase 1 frozen identity (proposals; the operator freezes — KAP §3.1)

Phase 1 freezes ONE closed thesis; everything after conforms to it. Three candidates, each with its nearest
prior art, what it can claim on today's evidence, and its exposure.

**I-A — the instrument paper: "fidelity, exactly and decomposably, at the generator's own phase".**
*Thesis:* a frame generator's fidelity is measurable against an analytic 3-D scene at the phase the
generator actually produced, decomposed into named terms (position, shape, hallucinated mass, signed lead)
with the disocclusion bucket apart, and every number carries its run-to-run reliability. *Nearest prior
art:* Kiefhaber 2024 (extends it on four axes: 3-D analytic scene, continuous phase, four-way visibility,
named terms + r); Middlebury (region-conditioned error). *Claimable today:* the B1 row, the speed law, the
phase dependence, the async-path row (after DI-3). *Exposure:* sim-to-real (Q3-7) — it is a fidelity
metric, not a quality metric, and must say so; one scene family and one kernel; no perception link, so no
comparison with FloLPIPS/PSNR_DIV on their own terms is possible, only a demonstration that they cannot see
what this sees (N6, Q2-4, Q2-12). *Boundary:* FG/VFI on rendered content; excludes human studies.

**I-B — the model paper: "the generator's own decisions predict its exact-truth error".** *Thesis:* a
reference-free predictor of the decomposed error, trained from the kernel's decision traces (SAD, warp
weights, stasis, guided picks, consensus agreement — `--gdump` + `ref_warp --decisions`) with `scene_truth`
as the label. This is "our own model" in the LPIPS sense (a learned function) with exact truth as the
supervisory signal instead of human opinion. *Nearest prior art:* stereo confidence from own outputs
(Q4-5…Q4-7), VFI error from own flow (Q4-2); N4 says nobody has done it from internal traces. *Claimable
today:* nothing measured — the 99 % / 18 % reading on four frames is a hypothesis, not a result.
*Exposure:* requires I-A's instrument and rows first; a predictor of one kernel's error on one scene family
is that kernel's diagnostic unless the trace features are shown generic; the training set is the FG's own
provenance, so it is FG-specific by construction.

**I-C — the distribution paper: an FID/CMMD-style metric over error-decomposition features.** Refuted as
a fidelity measure by Q1-9 and Q1-10; meaningful only as a realism measure of the output distribution,
which for an interpolator with the truth two frames away is the weaker question. **Not recommended as the
identity.**

**Recommendation (the supervisor's, for the operator's decision):** freeze **I-A**, with I-B named in
the identity's boundary as the *next* body of work it authorizes but does not contain. Reason: KAP freezes a
body of knowledge to be analysed, and I-A is the only candidate whose central claim is measured today
(§5); freezing I-B would freeze a hypothesis and every later gate would certify conformance to an
unmeasured thesis (KAP §9.4). The sequence I-A → I-B is also the sequence of the evidence: I-B's labels are
I-A's rows. Before the freeze, three of I-A's own gaps are cheap to close and change the identity's
boundary if they fail: the DI-3 second run of the async row (20 s of screen), a second scene family, and a
1080p row under the tap.

**What a Phase 1 identity block would need to state (KAP §5, INV-1…INV-16):** the thesis in one sentence;
the boundary (rendered content, fidelity not quality, k and resolutions covered); the priority order of the
named terms; the evidence tiers admitted (V1 only for load-bearing numbers); the sim-to-real caveat as part
of the thesis, not a footnote.

---

## §7 — Not run, and the limits of this dossier

- The supervisor did not open: Salimans 2016, Barratt 2018, LDMVFI, Vimeo-90K, CAIN, Sintel, Spring,
  Kubric, FlyingThings3D, the Middlebury IJCV pages, any Q4 source, Hallucination Score, Ghadiyaram &
  Bovik — each is [V2] above on the strength of two independent subordinate reads that agreed.
- The BVI-VFI TIP Table II digits were read from a rendered PDF page and confirmed against the arXiv HTML
  rendering's table as returned by the fetch tool; the ICIP Table 2 digits come from the HTML rendering
  only. The three V.A sentences were located in both.
- The venue of FloLPIPS rests on the authors' repository tagline, the Edinburgh Research Explorer entry and
  a search summary of the PCS 2022 proceedings ToC; the IEEE Xplore record itself was not opened.
- FVD's workshop venue and CMMD's CVPR acceptance are [V3].
- The sweeps ran with web access on 2026-09-09 and are time-boxed; §4's absences are bounded by the
  searches listed, not exhaustive. The Alabau-Bosque 2024 threshold paper is the one most likely to change
  N6 if read.
- No number of the FG's was produced or re-run for this dossier.

*Made with my soul - Swately <3*
