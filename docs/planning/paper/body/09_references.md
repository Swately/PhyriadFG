# 9. References

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 10 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## Ref.1 — Prior art, each entry at the verification level the Phase 0 table assigns ([V1]/[V2]/[V3]); nothing [V3] cited as fact

*serves:* ET · CB · *evidence:* `FG_METRIC_MODEL_PRIOR_ART.md` §2 (41 sources) · *status:* `EXISTS`

**Distribution-metric family (`FG_METRIC_MODEL_PRIOR_ART.md` §2, Q1).**

| # | Citation | Level | id |
|---|---|---|---|
| 1 | Heusel, Ramsauer, Unterthiner, Nessler, Hochreiter — *GANs Trained by a Two Time-Scale Update Rule Converge to a Local Nash Equilibrium*, NIPS 2017, https://arxiv.org/abs/1706.08500 | [V1] | Q1-1 |
| 2 | Salimans, Goodfellow, Zaremba, Cheung, Radford, Chen — *Improved Techniques for Training GANs*, NIPS 2016, https://arxiv.org/abs/1606.03498 | [V2] | Q1-2 |
| 3 | Bińkowski, Sutherland, Arbel, Gretton — *Demystifying MMD GANs*, ICLR 2018, https://arxiv.org/abs/1801.01401 | [V1] | Q1-3 |
| 4 | Jayasumana, Ramalingam, Veit, Glasner, Chakrabarti, Kumar — *Rethinking FID: Towards a Better Evaluation Metric for Image Generation*, arXiv v2 2024-01-25, https://arxiv.org/abs/2401.09603 | [V1] abstract; the "CVPR 2024" venue is the sweep's own attribution, not confirmed on the arXiv page — [V3], not stated as fact here | Q1-4, Q1-5 |
| 5 | Unterthiner, van Steenkiste, Kurach, Marinier, Michalski, Gelly — *Towards Accurate Generative Models of Video: A New Metric & Challenges*, arXiv 1812.01717 (v2 2019-03), https://arxiv.org/abs/1812.01717 | [V1] abstract; a workshop venue ("ICLR 2019 workshop") appears only in search summaries — [V3], not stated as fact here | Q1-6 |
| 6 | Kynkäänniemi, Karras, Laine, Lehtinen, Aila — *Improved Precision and Recall Metric for Assessing Generative Models*, NeurIPS 2019, https://arxiv.org/abs/1904.06991 | [V1] | Q1-7 |
| 7 | Barratt, Sharma — *A Note on the Inception Score*, ICML 2018 workshop (Theoretical Foundations and Applications of Deep Generative Models), https://arxiv.org/abs/1801.01973 | [V2] | Q1-8 |
| 8 | Ge, Mahapatra, Parmar, Zhu, Huang — *On the Content Bias in Fréchet Video Distance*, CVPR 2024, https://arxiv.org/abs/2404.12391 | [V1] | Q1-9 |
| 9 | Jain et al. (Google Research) — *Video Interpolation with Diffusion Models* (VIDIM), arXiv 2024, §4.2, https://arxiv.org/html/2404.01203v1 | [V1] | Q1-10 |
| 10 | Danier, Zhang, Bull — *LDMVFI: Video Frame Interpolation with Latent Diffusion Models*, AAAI 2024, https://arxiv.org/abs/2303.09508 | [V2] | Q1-11 |

**Full-reference perceptual family and its benchmarks (§2, Q2).**

| # | Citation | Level | id |
|---|---|---|---|
| 11 | Zhang, Isola, Efros, Shechtman, Wang — *The Unreasonable Effectiveness of Deep Features as a Perceptual Metric*, CVPR 2018, https://arxiv.org/abs/1801.03924 | [V1] | Q2-1 |
| 12 | Danier, Zhang, Bull — *FloLPIPS: A Bespoke Video Quality Metric for Frame Interpolation*, IEEE Picture Coding Symposium (PCS) 2022, pp. 283–287, best-paper finalist, https://arxiv.org/abs/2207.08119 · https://github.com/danier97/flolpips | [V1] | Q2-2 |
| 13 | Hou, Ghildyal, Liu — *A Perceptual Quality Metric for Video Frame Interpolation*, ECCV 2022, https://arxiv.org/abs/2210.01879 | [V1] abstract, the VFI-specific/temporal claim; [V2] the p. 2 sentence and the 25,887-triplet figure | Q2-3 |
| 14 | Ding, Ma, Wang, Simoncelli — *Image Quality Assessment: Unifying Structure and Texture Similarity*, IEEE TPAMI 44(5) 2022 (arXiv 2020), https://arxiv.org/abs/2004.07728 | [V2] | Q2-4, Q5-3 |
| 15 | Danier, Zhang, Bull — *A Subjective Quality Study for Video Frame Interpolation*, ICIP 2022 (DOI 10.1109/ICIP46576.2022.9897364), https://arxiv.org/abs/2202.07727 | [V1] | Q2-5 |
| 16 | Danier, Zhang, Bull — *BVI-VFI: A Video Quality Database for Video Frame Interpolation*, IEEE TIP 2023, https://arxiv.org/abs/2210.00823 | [V1] | Q2-6 |
| 17 | Xue, Chen, Wu, Wei, Freeman — *Video Enhancement with Task-Oriented Flow*, IJCV 2019, https://arxiv.org/abs/1711.09078 | [V2] | Q2-7 |
| 18 | Sim, Oh, Kim — *XVFI: eXtreme Video Frame Interpolation*, ICCV 2021 (Oral), https://arxiv.org/abs/2103.16206 | [V1] the dataset row; [V2] the real-camera-capture characterization | Q2-8 |
| 19 | Choi, Kim, Han, Xu, Lee — *Channel Attention Is All You Need for Video Frame Interpolation*, AAAI 2020, https://cdn.aaai.org/ojs/6693/6693-13-9922-1-10-20200521.pdf | [V2] | Q2-9 |
| 20 | Daly, Ramsook, Kokaram — *An Efficient Quality Metric for Video Frame Interpolation Based on Motion-Field Divergence*, IEEE QoMEX 2025 accepted manuscript (arXiv v1 2025-10-01, v2 2026-01-22), https://arxiv.org/abs/2510.01361 · code github.com/conalld/psnr-div | [V1] | Q2-10 |
| 21 | Daly, Ramsook, Kokaram — *Efficient motion-based metrics for video frame interpolation*, SPIE Applications of Digital Image Processing XLVIII 2025, https://arxiv.org/abs/2508.09078 | [V1] | Q2-11, Q5-6 |
| 22 | Ghildyal, Liu — *Shift-tolerant Perceptual Similarity Metric*, ECCV 2022, https://arxiv.org/abs/2207.13686 | [V1] | Q2-12 |
| 23 | MSU Graphics & Media Lab, https://videoprocessing.ai/benchmarks/video-frame-interpolation-methodology.html | [V2] | Q2-13 |

**Synthetic / analytic ground truth and the sim-to-real objection (§2, Q3).**

| # | Citation | Level | id |
|---|---|---|---|
| 24 | Butler, Wulff, Stanley, Black — MPI-Sintel (ECCV 2012), http://sintel.is.tue.mpg.de/about | [V2] | Q3-1 |
| 25 | Mehl et al. — *Spring: A High-Resolution High-Detail Dataset and Benchmark for Scene Flow, Optical Flow and Stereo*, CVPR 2023, https://arxiv.org/abs/2303.01943 | [V2] | Q3-2 |
| 26 | Greff et al. — google-research/kubric (CVPR 2022), https://github.com/google-research/kubric | [V2] | Q3-3 |
| 27 | Mayer et al. — *A Large Dataset to Train Convolutional Networks for Disparity, Optical Flow, and Scene Flow Estimation*, CVPR 2016, https://arxiv.org/pdf/1512.02134 | [V2] | Q3-4 |
| 28 | Baker, Scharstein, Lewis, Roth, Black, Szeliski — *A Database and Evaluation Methodology for Optical Flow*, ICCV 2007 / IJCV 92(1) 2011, https://vision.middlebury.edu/flow/floweval-ijcv2011.pdf | [V1] the hold-out and synthetic routes; [V2] the §4.1/§4.3 sentences | Q3-5, Q5-1, Q5-2 |
| 29 | Kiefhaber, Niklaus, Liu, Schaub-Meyer — *Benchmarking Video Frame Interpolation*, arXiv 2403.17128 (2024-03-25; no venue line), https://arxiv.org/html/2403.17128 | [V1] | Q3-6 |
| 30 | Reijalt, Gielisse, Karlsson, van Gemert — *On the Real-World Generalisability of Optical Flow Models*, ECCV 2026 (arXiv 2607.10470, submitted 2026-07-11), https://arxiv.org/abs/2607.10470 | [V1] | Q3-7 |

**Reference-free error prediction and self-diagnosis (§2, Q4 — all abstract-level).**

| # | Citation | Level | id |
|---|---|---|---|
| 31 | Ilg et al. — *Uncertainty Estimates and Multi-Hypotheses Networks for Optical Flow*, ECCV 2018, https://arxiv.org/abs/1802.07095 | [V2] | Q4-1 |
| 32 | Chi et al. — *Error-Aware Spatial Ensembles for Video Frame Interpolation*, arXiv 2207.12305 (2022), https://arxiv.org/abs/2207.12305 | [V2] | Q4-2 |
| 33 | Han et al. — *Perceptual Quality Assessment for Video Frame Interpolation*, arXiv 2312.15659 (2023), https://arxiv.org/abs/2312.15659 | [V2] | Q4-3 |
| 34 | Gretton, Borgwardt, Rasch, Schölkopf, Smola — *A Kernel Two-Sample Test*, JMLR 13 (2012) 723–773, https://jmlr.org/beta/papers/v13/gretton12a.html | [V2] | Q4-4 |
| 35 | Mostegel et al. — *Using Self-Contradiction to Learn Confidence Measures in Stereo Vision*, CVPR 2016, https://arxiv.org/abs/1604.05132 | [V2] | Q4-5 |
| 36 | *Confidence Inference for Focused Learning in Stereo Matching*, arXiv 1809.09758 (2018), https://arxiv.org/abs/1809.09758 | [V2] | Q4-6 |
| 37 | Poggi et al. — *Self-adapting confidence estimation for stereo*, ECCV 2020, https://arxiv.org/abs/2008.06447 | [V2] | Q4-7 |

**Decomposition, "hallucination", and reliability practice (§2, Q5 — Q5-1, Q5-2, Q5-3, Q5-6 already listed above under their primary sources).**

| # | Citation | Level | id |
|---|---|---|---|
| 38 | *Hallucination Score: Towards Mitigating Hallucinations in Generative Image Super-Resolution*, arXiv 2507.14367 (2025), https://arxiv.org/abs/2507.14367 | [V2] | Q5-4 |
| 39 | Netflix/vmaf, `resource/doc/conf_interval.md` (feature since v1.3.7, June 2018), https://github.com/Netflix/vmaf/blob/master/resource/doc/conf_interval.md | [V1] | Q5-5 |
| 40 | Ghadiyaram, Bovik — *Massive Online Crowdsourced Study of Subjective and Objective Picture Quality*, IEEE TIP 2016, https://arxiv.org/abs/1511.02919 | [V2] | Q5-7 |

Not opened first-hand by this document's own supervisor (`FG_METRIC_MODEL_PRIOR_ART.md` §7): entries 2, 6, 10, 17, 24–27, and every Q4 entry (31–37) rest on the sweep-plus-gate pair described in their Level column, not on a supervisor fetch; the BVI-VFI TIP Table II digits (entry 16) were read from a rendered PDF page and cross-checked against the arXiv HTML rendering, not opened as the publisher's own PDF.

## Ref.2 — The project's own records ([V1]): each evidence file and gate record cited by path and commit

*serves:* ET · *evidence:* `docs/evidence/`, `docs/planning/records/`, `docs/research/` · *status:* `EXISTS`

Commits are the tip of `git log -1 --format=%h -- <path>` for each path, run 2026-09-10 against the PhyriadFG repository.

| # | Record | What it holds (SCAFFOLD.md's own description) | Path | Commit |
|---|---|---|---|---|
| 1 | The frozen identity | Thesis, object under test, scenario set, priority order of the terms, rulers, reliability, evidence tiers, claims (a) and (b), the NOT clauses | `docs/research/FG_METRIC_MODEL_SPINE.md` | `9987742` |
| 2 | The Phase 0 anchor table | §2 the 45-claim source table this section's Ref.1 draws from; §4 the bounded absences N1–N7 | `docs/research/FG_METRIC_MODEL_PRIOR_ART.md` | `16bde4a` |
| 3 | The canonical FG row | §1 the scene family and barcoded base index; §2/§2b the canonical k = 4 `mixed` row and its two-seed agreement; §3 the k sweep and the displacement floor; §4 the cut-pair exclusion; §5 what is one run | `docs/evidence/B1_FIRST_FG_ROW.md` | `9987742` |
| 4 | The speed-law evidence | §2–§3 the displacement-axis rows and the fitted speed law; §3 the k-path vs speed-path source-rate reading; §4 the provenance readings; §6 what is owed | `docs/evidence/B1_SPEED_TEST.md` | `d475809` |
| 5 | The synthetic sweep, seed 7 | The `blend` and `blur` rulers carried into the sweep at the canonical k | `docs/evidence/B1_SWEEP_seed7.md` | `96ac1ef` |
| 6 | The synthetic sweep, seed 11 | The second-seed counterpart of the same sweep | `docs/evidence/B1_SWEEP_seed11.md` | `96ac1ef` |
| 7 | The second instrument's baseline | The marker-chain (`tools/motion_truth/`) baseline reading | `docs/evidence/MOTION_TRUTH_BASELINE.md` | `d1fd686` |
| 8 | The source-rate rig record | Names the rig's GPU for the reproducibility anchor | `docs/evidence/M1_SRC_RATE.md` | `e4e6d06` |
| 9 | The every-tick tap gate | G0 the build identity (tree `0df0332`, md5 `186CB46C`, base `364efc5`, md5 `F36FDE32`); G5 the asynchronous path's one row | `docs/planning/records/GDUMP_GATE.md` | `0df0332` |
| 10 | The phase-dependence finding | The M1_LOWPHASE signature record | `docs/planning/records/M1_LOWPHASE_FINDING.md` | `d8ec5ae` |
| 11 | Hardware and QoL findings | Lines 136–143 name the rig for the reproducibility anchor | `docs/planning/records/QOL_FINDINGS.md` | `906f449` |
| 12 | The second instrument's gate, T2 | "the ground-truth marker zoo" | `docs/planning/records/S2_T2_GATE.md` | `ff06a8e` |
| 13 | The second instrument's gate, T3 | "the player" | `docs/planning/records/S2_T3_GATE.md` | `a477120` |
| 14 | The second instrument's gate, T4 | "the extractor" | `docs/planning/records/S2_T4_GATE.md` | `8c3dfbd` |
| 15 | The second instrument's gate, T5 | "the report" | `docs/planning/records/S2_T5_GATE.md` | `0d4e8b7` |
| 16 | The second instrument's gate, T6 | "the CPU reference warp" | `docs/planning/records/S2_T6_GATE.md` | `d8ec5ae` |
| 17 | The scenario test matrix | §2 the pre-registered family signatures; §3 the I-B / knob-sweep authorization; §4 the A/B rule; §8 the capture procedure; §9 the gate logs and the re-scored tables | `docs/planning/REGIME_TEST_MATRIX.md` | `9987742` |
| 18 | The project's learning ledger | P-025, P-026, P-029 | `docs/LEARNING_LOG.md` | `9987742` |

Entry 17's real path is `docs/planning/REGIME_TEST_MATRIX.md`, not `docs/planning/records/`; entry 18's real path is `docs/LEARNING_LOG.md`, not under `docs/research/`. Both are cited under their real paths above. The gate logs `C:\PhyriadFG\runs\_render_logs\`, the cut corpus `C:\PhyriadFG\runs\sc_live2\cuts_k4.md`, and the corpus manifests under `C:\PhyriadFG\runs\*` that M1.6, M1.7 and RA.2 also name are outside this repository and outside the three directories this slot's permitted sources cover; they carry no commit and are not listed here.

*Made with my soul - Swately <3*
