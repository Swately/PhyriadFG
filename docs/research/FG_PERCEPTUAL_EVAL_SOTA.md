# FG_PERCEPTUAL_EVAL_SOTA — rigorous EYE evaluation for an external-capture FG

> **Diátaxis type:** Analysis (SOTA dossier). **Status:** `measured` (the cited
> literature reports measured numbers from real subjective studies) / `designed`
> (Phyriad's *application* — the single-expert eye-evaluation harness + the
> verdict dataset + the eye-proxy calibration loop are specified here, not built).
> Resolves the methodological substrate for objective **H1/H2** (objective +
> honest FG-quality measurement) on the perceptual axis the operator put first:
> *"hand-eye SENSATION > numbers/latency."* Companion to
> [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md)
> (the instrument/PresentMon side) and [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md)
> (the no-reference metric ceiling).
>
> **Provenance.** From deep-research workflow `w770dvln7` (109 agents; 26 sources
> → 113 claims → 25 adversarially verified, 22 confirmed / 3 killed, 3-vote).
> External claims carry that sweep's `[V1]/[V2]/[V3]` levels; the load-bearing
> standards facts (BT.500, P.910) and the calibration/dataset papers are primary
> `[V1]`, fetched first-hand **by the research pass** (BT.500-14 via `pdftotext`).
> Per [`../canon/FORMAL_DOCUMENT_PROTOCOL.md`](../canon/FORMAL_DOCUMENT_PROTOCOL.md)
> §2.4 the author did **not** independently re-fetch every URL this session — §7
> flags this honestly. No fabricated citation, path, line, or number.
>
> **Normativity (BCP 14, [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html)
> / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)):** MUST / SHOULD /
> MAY appear in capitals only where a real constraint binds. The cited ITU
> Recommendations themselves use advisory **"should"**, not mandatory "shall" —
> §2.A keeps that distinction; do not read their "should" as our MUST.

---

## §0 — Scope (and non-scope)

**In scope.** How the perceptual/subjective quality of real-time frame generation
— motion smoothness and interpolation artifacts — is assessed *rigorously by human
observers*, and what a controlled single-expert eye-evaluation environment needs:
the ITU subjective-assessment standards, the forced-choice A/B paradigm for subtle
FG configs, artefact amplification, the artifact taxonomy + structured scoring, and
how a small expert-verdict dataset calibrates an objective scorer into an eye-proxy.

**Non-scope.** The objective instrument itself (PresentMon `FrameType`,
All-Input-to-Photon, the `fg_quality_scorer`) → that is
`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`. The no-reference metric design → that is
`FG_VFI_PRIOR_ART.md` §11. This dossier is the **perceptual / human-arbiter** layer
above both. It is an *Analysis* doc, not a plan: the testbench's buildable plan, if
lifted, is a separate Tier-1/Tier-2 trio per
[`../canon/PLAN_TIER_PROTOCOL.md`](../canon/PLAN_TIER_PROTOCOL.md) (an eye-judgement
loop that gates default-promotion touches `quality`, not crash/concurrency/security,
so it is **Tier-1** unless it drives an irreversible default flip, which would pull
in the existing `PHYRIADFG_RISK_REGISTER.md`).

---

## §1 — Phyriad eye-eval current standing (in-repo, first-hand)

- **The arbiter is already the eye, but the protocol is ad-hoc.** Every shipped FG
  default (`--ts-smooth`, `--sc-select`, `--vblend`, `bg-snap`, …) was promoted by
  an unblinded, un-counterbalanced operator eye-pass on a live game, then flipped to
  default with a `--no-X` hatch (the perfection-tradeoff default-promotion gate,
  [`../canon/PERFECTION_TRADEOFF_DOCTRINE.md`](../canon/PERFECTION_TRADEOFF_DOCTRINE.md)).
  That is exactly the regime the standards in §2 call **"informal"** and the bias
  controls in §2.B are designed to remove.
- **A full-reference self-scorer exists, unwired + uncalibrated.** `fg_quality_scorer`
  (held-out triples, modes A/B/T, `dbl_edg_m` / `flowdsc`) is built but not in
  CMake/CI and not calibrated to any human-rated set
  (`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md` §1; the README line not re-opened
  first-hand — `framework/render/vulkan/bench/fg_quality_scorer/README.md` *(to verify)*).
  **The verdict dataset this dossier specifies is the missing input that turns that
  scorer into an eye-proxy.**
- **The deterministic synthetic scene HAS ground truth.** Phyriad scores on a
  controlled synthetic motion source with known intermediate frames — which is the
  precondition that makes *artefact amplification* (§2.C) directly usable, an
  advantage real-video VFI studies (BVI-VFI) do not have.
- **Test-environment components (the methodological pivot).** A controlled FG test
  environment is in flight (live task: *"Controlled FG test ENVIRONMENT — the
  methodological pivot"*). Its components are referenced here as the master-plan BUILD
  components **TB-C1** (the deterministic synthetic scene + source), **TB-C6** (CameraKit —
  physical high-speed pursuit-camera capture of the panel), and **TB-C7** (frame-ID — the
  PresentMon/frame-identification logging harness); this dossier itself informs the eye-A/B
  harness, component **TB-C5**. TB-C7 has first-hand artifacts in
  the repo root (`c7_status.txt`, `c7_diag.txt`, `c7_pmlog.txt`); at the time of
  reading, `c7_status.txt` = `DONE tag=bf6_lsfg presentrows=0 gpusamples=0` — i.e. a
  wired but **not-yet-yielding** capture run, so TB-C7 is `designed`/in-progress, not
  `measured`. **Naming caveat (§7):** `TB-C<n>` here are the canonical BUILD components in
  [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md) (`TB-C1..TB-C8`); do NOT confuse them
  with the unrelated **C5/C6/C7 closeable-fixed-point** tests in
  [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §2 (vendor-run /
  pacing-jitter / latency-vs-LSFG). They are different schemes; this dossier uses
  the test-environment meaning throughout.

**Net:** Phyriad has the deterministic-scene + ground-truth + self-scorer pieces
LSFG structurally cannot have, but lacks (a) a bias-free protocol, (b) a verdict
dataset, (c) a calibrated eye-proxy. This dossier is the SOTA grounding for all three.

---

## §2 — SOTA findings (leveled)

### A — Subjective-assessment standards: BT.500 / P.910 / P.913

- **`[V1]` A single expert is, by the standard's own language, "informal."**
  ITU-R BT.500 RECOMMENDS *"at least 15 observers"*; *"For studies with limited
  scope, e.g. of exploratory nature, fewer than 15 observers may be used. In this
  case, the study should be identified as 'informal'."* Verbatim, identical across
  BT.500-13/-14/-15. Observers SHOULD be screened (Snellen/Landolt acuity + Ishihara
  colour). → **The operator-only harness is formally informal; it cannot claim
  panel-grade validity and MUST compensate with many repeated, randomized,
  counterbalanced *within-subject* trials** (the substitution the standard implies
  but does not script).
- **`[V1]` Session ≤ 30 min; warm-up discarded.** *"A session should not last more
  than half an hour. At the beginning of the first session, about five 'dummy
  presentations' should be introduced to stabilize the observers' opinion. The data
  issued from these presentations must not be considered… about three dummy
  presentations are only necessary at the beginning of the following session."* →
  the fatigue/stabilization structure the harness MUST observe: ~5 throw-away warm-up
  trials (≈3 on resumed sessions), hard ≤30-min blocks.
- **`[V1]` Randomized, counterbalanced order; blind permitted.** *"A random order
  should be used for the presentations (for example, derived from Graeco-Latin
  squares); but the test condition order should be arranged so that any effects on
  the grading of tiredness or adaptation are balanced out from session to session."*
  The Recommendation also explicitly permits a **blind test**: *"the laboratory will
  carry out the test by gathering the assessors' votes without necessarily knowing
  the quality parameters under evaluation."* (Caveat: this is lab-level
  division-of-labour, not a turnkey per-trial observer-blind A/B — it is the *basis*
  for bias control, not the harness itself.)
- **`[V2/V3]` P.910 / P.913 = the multimedia + out-of-lab siblings.** ITU-T P.910
  (2023 revision) is the current standard scoped to subjective video quality for
  multimedia; ITU-T P.913 explicitly permits testing **outside a typical lab** —
  directly relevant to a desk-side operator rig. (P.910/P.913 specifics reached via
  landing page / secondary, not full-text; marked accordingly.)

### B — The paradigm for subtle FG A/B: 2AFC forced choice

- **`[V1]` 2AFC pairwise is the recommended method when differences are small.**
  Perez-Ortiz & Mantiuk (IEEE TIP 2019): pairwise comparisons *"were found to be
  more accurate in most cases and took less time compared to rating,"* have *"lower
  cognitive load, require little training and generally eliminate the bias of the
  observer,"* and are most suitable when the visual difference is small — the exact
  FG-config A/B regime (async-present on/off, `--ts-smooth` 0.1 vs 0.5). (Honest
  caveat: "eliminate bias" = anchoring/scale bias, not all noise; on one dataset
  (LIVE) rating beat pairwise; pairwise cost scales quadratically in #conditions →
  use sampling.)
- **`[V1]` Results scale to JOD units.** **1 JOD (just-objectionable-difference) =
  75% of observers preferring one condition over the other**, under the Thurstone
  Case-V normal-observer model (σ = 1.0484 per-stimulus / s_ij = 1.4826 on the
  difference; Φ(0.6745) = 0.75, constants confirmed in the authors' `pwcmp` code).
  For a single observer, "75% of observers" becomes "75% of *trials*" — the same unit
  read on repeated within-subject decisions.
- **`[V1]` Analyse with Bradley-Terry / Thurstone; test with chi-square uniformity.**
  Paired-comparison data is fit by Bradley-Terry (BT) or Thurstone-Mosteller Case-V
  (TM) — near-identical interval-scale estimates for complete data; BT is preferred
  in imaging for tractable ML scale estimates, simultaneous confidence intervals and
  hypothesis tests. **Whether observed preferences are real is decided by a chi-square
  test of the null that all scale values are uniform** (worked example: T_U = 74.01,
  3 df, exceeds the 95% cutoff 7.815 → non-uniform, i.e. a real preference). (Caveat
  → §7: the source's "whereas TM does not [yield tractable ML]" overstates; TM *can*
  be ML-fit (Tsukida & Gupta 2011), just less conveniently — this drew the sweep's
  one split vote, 2-1.)
- **`[V1]` Modern fit = MLE to a binomial decision model**, not an ad-hoc neural net
  (Hepburn et al. 2024) — a perceptual-distance model is evaluated against the 2AFC
  data by maximum-likelihood fit to the binomial of each forced choice. This is the
  bridge from the verdict dataset to a calibrated scorer (§2.F).

### C — Artefact amplification (surfacing minute interpolation differences)

- **`[V1]` v' = v + α·(v̂ − v), α > 1** (Men, Hosu, Lin, Bruhn, Saupe, *Quality and
  User Experience* 2020) — linearly scale each pixel's deviation from ground truth to
  *"increase the sensitivity of observers when judging minute difference in paired
  comparisons."* Per-RGB-component; Algorithm 1 locally reduces α near clamp
  boundaries to avoid saturation artifacts. **Directly usable because Phyriad's
  synthetic source HAS ground truth `v`** — two FG configs that look identical at
  α=1 become discriminable at α>1, then the *preference* (not the amplified image)
  is the datum. This is the lever that makes a single eye productive on subtle deltas.

### D — Motion-specific perception (the partly-uncovered axis)

- **`[blog]` Pursuit-camera + MPRT are the named, documented motion-clarity tools.**
  Blur Busters TestUFO provides a *"Ghosting / Pursuit Camera"* test (reveals display
  motion blur, ghosting, overdrive) and an *"MPRT Indicator"* (Moving Picture Response
  Time = persistence). MPRT ≠ GtG: on a sample-and-hold display even a perfect-0ms-GtG
  panel still shows persistence blur equal to the frametime. The pursuit-camera
  (camera slid/rotated to track on-screen motion, or a high-speed camera doing
  spatio-temporal integration along the trajectory) captures motion *"as the human
  eye sees it"* and is the method RTINGS / TFTCentral / HDTVtest use.
- **`[blog]` "Blur Busters Law": 1 ms of persistence ⇒ 1 px of motion blur per 1000
  px/s of motion** — a deterministic objective motion-clarity floor a synthetic
  scrolling scene can exercise. (Presented as a guaranteed *minimum* blur,
  acuity-independent.)
- **`[secondary]` Frame-pacing irregularity is perceptually distinct from average
  fps** (micro-stutter: *"the time intervals between consecutively displayed frames
  are uneven, even though the average frame rate… appears adequate"*). This grounds
  why a flat *cadence ladder* matters independent of mean fps — but **no surviving
  claim supplied a quantitative ms-level JND** for pacing error or added latency (the
  named open gap, §6 / §7).

### E — Artifact taxonomy + structured scoring

- **`[V1]` CGVQM+D (Jindal et al., Intel, CGF/EGSR 2025) is the directly reusable
  rendering-artifact template** — and **its dataset explicitly includes FG**: a P.910
  double-stimulus impairment study, 20 experienced observers, 80 videos ×3 = 4,800
  ratings, continuous 0–100 *"Imperceptible"→"Very annoying"* scale, DMOS(v) =
  MOS(v) − MOS(v_ref) + 100, inter-rater ICC(2,k) = 0.97. The interpolation condition
  is NVIDIA NVFRUC (12–30 fps → 60 fps), inside a broad **artifact taxonomy**:
  *spatio-temporal aliasing, flicker, ghosting, moiré, fireflies, noise, blur, tiling,
  and (neural) hallucinations.* → the seed for Phyriad's per-artifact rubric, mapped
  to our own observed classes (disocclusion halo / "gravity" stretch / boundary pulse
  / edge warble / HUD-text break-up). (Caveat → §7: CGVQM+D is double-stimulus
  *degradation rating* (DCR/DSIS-style), **not** forced-choice pairwise — one sweep
  claim mislabelled it "pairwise-DMOS.")
- **`[V1]` BVI-VFI (Danier/Zhang/Bull, ICIP 2022) is the VFI-specific protocol**:
  DSCQS, 60 paid observers, viewing distance 1008 mm (3× screen height, per BT.500),
  ~30-min sessions of 60 trials, DMOS. The protocol transfers; the content (real
  video, no ground truth) does not — Phyriad's synthetic-GT scene is the difference.
- **Open:** no surviving claim validated a **structured per-artifact-per-trial rubric
  vs a single global score** for FG. Taxonomies exist (above); a validated rubric does
  not (§6).

### F — Calibrating an objective scorer to the eye

- **`[secondary/V1]` VMAF is the canonical "fuse metrics → eye-proxy via supervised
  regression on MOS" method**: features = VIF + DLM + a temporal term (mean co-located
  pixel difference), fused by an SVM/nuSVR trained on subjective MOS; reaches PLCC
  ~0.96 *in-distribution* (do not overgeneralize — §2.G). ST-VMAF/E-VMAF extend it
  with the same SVR-on-MOS recipe.
- **`[V1]` Small-parameter calibration is the realistic shape for a tiny dataset.**
  CGVQM-5 freezes a Kinetics-400-pretrained 3D-ResNet-18 and optimizes **only 1,027
  channel-wise weights** (Adam, lr 1e-6, objective = minimize 1 − PLCC) — i.e. fit a
  handful of weights on top of a frozen backbone, not train a net. A logistic mapping
  alone lifts an existing metric's Pearson correlation with MOS by 2–20.2% (`[V1]`,
  arXiv:2104.12448).
- **`[V1]` How many votes per condition.** Crowdsourced ACR/P.910 reproduces lab MOS
  (r̄ = 0.952 per sequence) and **reliability saturates at ~40 votes**, with **~21–25
  accepted votes already at PCC 0.96–0.97** (Naderi & Cutler, MSR 2022). → a concrete
  floor: **target ~30, accept ~21–25 repeated within-subject trials per condition**;
  beyond ~40 adds little.
- **Validation metrics:** a calibrated scorer is judged against MOS by PLCC / SRCC /
  KRCC / RMSE — the standard correlation battery.

### G — THE HARD TRUTH (state plainly): the eye-proxy for FG is an OPEN problem

- **`[V1]` No standard objective metric exceeds PLCC ≈ 0.6 for FG/VFI.** On BVI-VFI
  the *best* standard metric is LPIPS at **PLCC 0.597 / SROCC 0.599**; PSNR
  0.471/0.520, SSIM 0.475/0.581, VMAF 0.564/0.595 — all below 0.6. Even *bespoke* VFI
  metrics (FloLPIPS) reach only ~0.58–0.71. The paper concludes there is an *"urgent
  need… [for a] bespoke perceptual quality metric for VFI."*
- **Therefore:** calibrating *any* existing scorer (including `fg_quality_scorer`)
  into a trustworthy FG eye-proxy is **research, not a turnkey deliverable.** The
  **verdict dataset is the dependable product**; the proxy trained on it is a hypothesis
  whose ceiling is unknown.
- **Single-observer ceiling.** With one operator, the achievable metric-vs-human
  correlation is bounded above by **the operator's own test-retest self-consistency**
  (his agreement with himself on repeated identical trials) — a proxy cannot correlate
  with a noisy target better than the target correlates with itself. Measuring that
  self-consistency (repeat a fraction of trials) is REQUIRED to even state a ceiling,
  and is itself an open quantity for this rig (§7).

---

## §3 — The designed application (the harness + dataset + loop) — `designed`

Mapping §2 onto Phyriad's single-expert, deterministic-scene reality. **Status of
everything in §3 is `designed`** — the testbench is not built.

| Element | Designed form | Grounded in |
|---|---|---|
| **Paradigm** | 2AFC forced choice ("which feels smoother / less artifacted: A or B?"), NOT a rating scale | §2.B |
| **Blinding** | config identity hidden; A/B side randomized per trial; operator records only the preference | §2.A blind clause + §2.B |
| **Order** | Graeco-Latin / randomized, counterbalanced across ≤30-min blocks; ~5 discarded warm-ups (≈3 on resume) | §2.A |
| **Single-observer substitution** | many repeated within-subject trials replace panel size; **~21–25 accepted (target ~30) per condition pair**; honestly labelled **"informal"** | §2.A, §2.F |
| **Sensitivity lever** | artefact amplification v' = v + α(v̂−v), α>1, on the synthetic-GT scene to surface subtle deltas | §2.C, §1 |
| **Significance** | chi-square uniformity test on the preference tallies; scale via BT/Thurstone → JOD (1 JOD = 75% of trials) | §2.B |
| **Self-consistency** | a fraction of trials repeated to measure operator test-retest agreement = the proxy's correlation ceiling | §2.G |
| **Verdict dataset** | (config-pair, scene, α, preference, JOD, n_trials, self-consistency) — **the deliverable** | §2.F, §2.G |
| **Eye-proxy (research)** | small-parameter / logistic fit of `fg_quality_scorer` (or a frozen backbone, ~1k weights) to the verdict dataset; validate by PLCC/SRCC, **ceiling-bounded, do not over-trust** | §2.F, §2.G |

This loop is the perceptual companion to the `fg_quality_scorer` regression loop the
measurement dossier calls P7; it supplies the human ground truth that loop lacks.

---

## §4 — The gaps (thin SOTA evidence) + how TB-C1/TB-C6/TB-C7 fill them empirically

The sweep's surviving evidence is **thin or absent** on three things a motion-FG eye
harness specifically needs. Named as open, with the practitioner pointers that exist
and how the in-flight test-environment components fill them empirically for our rig:

1. **Quantitative ms-level JND for pacing & latency.** No surviving claim gives a
   ms threshold for perceptible frame-pacing irregularity or added latency. Practice
   pointer: Blur Busters pursuit-camera + MPRT (the *tools*, no published thresholds)
   and Digital Foundry's qualitative practice. → **TB-C7 (frame-ID)** logs per-frame
   present/display timing (PresentMon-class), letting the operator empirically locate
   *his own* pacing-JND on the deterministic scene rather than cite a literature number
   that does not exist.
2. **High-speed-camera panel methodology.** No recording/replay claim survived; the
   limits of screen-capturing the FG's own WDA-excluded overlay (re-interpolation /
   capture-the-capture) make software self-capture unreliable. Practice pointer: Blur
   Busters pursuit-camera HOWTO. → **TB-C6 (CameraKit)** is the physical 240/480/1000-fps
   pursuit/slow-motion capture of the actual panel — the only path that escapes the
   uncapturable-overlay wall and the one cross-comparable axis vs LSFG.
3. **Validated per-artifact rubric.** Taxonomies exist (CGVQM, BVI-VFI); a *validated*
   per-artifact-per-trial rubric for FG does not. → built empirically on **TB-C1 (the
   deterministic synthetic scene)** whose known ground truth lets each artifact class
   (disocclusion halo / "gravity" / boundary pulse / edge warble / HUD-text) be
   isolated and amplified (§2.C) for per-class 2AFC.

These three are **empirical fills for *our* rig**, not general SOTA contributions —
they make the operator's eye productive where the literature is silent; they do not
resolve the open problems for the field.

---

## §5 — Sources (leveled)

External, with the sweep's verification levels (access via workflow `w770dvln7`;
re-fetch obligation per §7). `[V1]` = primary, first-hand in the research pass;
`[V2]` = primary partial (landing page / abstract); `[V3]`/`[secondary]`/`[blog]` as
marked.

| Ref | What it anchors | Level | URL |
|---|---|---|---|
| ITU-R BT.500-14 (2019) | ≥15-else-informal · 30-min · 5/3 dummies · Graeco-Latin · blind | **[V1]** | https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.500-14-201910-S!!PDF-E.pdf |
| ITU-R BT.500-13 (2012) | same passages, prior revision | **[V1]** | https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.500-13-201201-I!!PDF-E.pdf |
| ITU-T P.910 (2023) | multimedia subjective-quality standard | **[V2]** | https://www.itu.int/epublications/publication/itu-t-p-910-2023-10-subjective-video-quality-assessment-methods-for-multimedia-applications |
| NTIA/ITS subjective-testing | P.913 out-of-lab; standards map | **[V2]** | https://its.ntia.gov/research/qoe/video-quality-research/standards/subjective-testing/ |
| Perez-Ortiz & Mantiuk 2019 (IEEE TIP) | 2AFC advantages · JOD=75% · Thurstone Case-V constants | **[V1]** | https://www.cl.cam.ac.uk/~rkm38/pdfs/perezortiz2019unified_quality_scale.pdf |
| Hepburn et al. 2024 | 2AFC robustness · MLE binomial-decision fit | **[V1]** | https://arxiv.org/abs/2403.10390 |
| Handley 2001 (IS&T PICS) | BT vs TM · chi-square uniformity (T_U=74.01, df=3) | **[V1]** (BT-vs-TM line overstated → 2-1) | https://www.academia.edu/3156463/Comparative_Analysis_of_Bradley-Terry_and_Thurstone-Mosteller_Paired_Comparison_Models_for_Image_Quality_Assessment |
| Naderi & Cutler 2022 (MSR) | MOS saturates ~40 votes (~21–25 good); not-well-correlated motivation | **[V1]** | https://arxiv.org/pdf/2204.06784 |
| Men et al. 2020 (QUE) | artefact amplification v'=v+α(v̂−v), α>1 | **[V1]** | https://arxiv.org/pdf/2001.06409 |
| Jindal et al. 2025 (CGVQM+D, Intel) | FG-inclusive taxonomy · DMOS P.910 study · 1,027-weight calibration | **[V1]** | https://arxiv.org/html/2506.11546v1 |
| Danier et al. 2022 (BVI-VFI, ICIP) | DSCQS VFI protocol · **all standard metrics < PLCC 0.6** | **[V1]** | https://arxiv.org/html/2202.07727 |
| Blur Busters — pursuit camera | motion-clarity capture method | **[blog]** | https://blurbusters.com/motion-tests/pursuit-camera/ |
| TestUFO — MPRT / Ghosting | named pursuit + MPRT tests | **[blog]** | https://testufo.com/ · https://testufo.com/mprt |
| Blur Busters — GtG vs MPRT | persistence ≠ transition; Blur Busters Law | **[blog]** | https://blurbusters.com/gtg-versus-mprt-frequently-asked-questions-about-display-pixel-response/ |
| Wikipedia — micro stuttering | pacing irregularity ≠ average fps | **[secondary]** | https://en.wikipedia.org/wiki/Micro_stuttering |
| Wikipedia — VMAF | SVM-on-MOS fusion (eye-proxy mechanism) | **[secondary]** | https://en.wikipedia.org/wiki/Video_Multimethod_Assessment_Fusion |
| Bampis et al. (ST-VMAF/E-VMAF) | SVR-on-MOS extension | **[V1]** | https://arxiv.org/pdf/1804.04813 |
| Logistic-mapping calibration | +2–20.2% PLCC from a logistic fit | **[V1]** | https://arxiv.org/abs/2104.12448 |
| VMAF reproducibility (RealNetworks/IEEE) | feature set + training recipe | **[V1]** | https://realnetworks.com/sites/default/files/vmaf_reproducibility_ieee.pdf |

**Refuted (do not reintroduce):** (1) δ = 1.96·S/√N as *"the statistic for deciding
whether two FG configs differ"* — **killed 0-3**; it is a single-mean CI (breaks at
N=1), not a two-condition / within-subject test → use the sign/chi-square test, BT,
or Thurstone. (2) DSCQS as *"the canonical BLIND A/B protocol"* — **killed 0-3**;
DSCQS is reference-anchored double-stimulus, not the blind FG-config A/B → prefer
2AFC. (3) CGVQM-5 *"PLCC 0.871, near the ~0.88–0.89 ceiling"* — **killed 1-2**; treat
exact ceiling figures as unverified.

---

## §6 — Cross-links

- The objective VFI/FG measurement axes (GT source · quality/latency/throughput/pacing
  instruments · the saturation-generator gap) this perceptual layer sits above →
  [`FG_VFI_MEASUREMENT_SOTA.md`](FG_VFI_MEASUREMENT_SOTA.md) (the sibling measurement dossier).
- Instrument / PresentMon / `fg_quality_scorer` regression loop →
  [`FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`](FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md).
- No-reference metric ceiling (~0.7–0.83 SROCC) + ghosting/disocclusion cure family →
  [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) §11.
- The default-promotion gate the eye-pass feeds →
  [`../canon/PERFECTION_TRADEOFF_DOCTRINE.md`](../canon/PERFECTION_TRADEOFF_DOCTRINE.md).
- Objective space + the (colliding) closeable-fixed-point C-tests →
  [`PHYRIADFG_OBJECTIVE_VISTA.md`](PHYRIADFG_OBJECTIVE_VISTA.md) §2.
- The uncapturable-overlay wall (why TB-C6 is physical) →
  [`FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md`](FG_COMPATIBILITY_COVERAGE_PRIOR_ART.md).

## §7 — What could NOT be verified (first-hand) / hedges

1. **URLs not re-fetched this session.** The §5 references carry workflow
   `w770dvln7`'s verification levels; the author did **not** independently re-open
   every URL (FDP §2.4). The BT.500 / P.910 / calibration primaries were fetched
   first-hand *by the research pass* (BT.500-14 via `pdftotext`); re-confirm before
   quoting any specific figure as load-bearing.
2. **The TB-C1 / TB-C6 / TB-C7 references** (scene-source / CameraKit / frame-ID) are the
   canonical BUILD components in [`FG_TESTBENCH_MASTER_PLAN.md`](FG_TESTBENCH_MASTER_PLAN.md)
   (`TB-C1..TB-C8`); the bare **C5/C6/C7** in `PHYRIADFG_OBJECTIVE_VISTA.md` are the unrelated
   closeable-fixed-point tests. The TB-C7 (frame-ID) artifacts were verified first-hand
   (`c7_status.txt`: `presentrows=0` = wired-but-not-yielding, so TB-C7 is `designed`/in-progress).
   The *roles* (deterministic scene-source / physical pursuit-camera / frame-ID logging) are the
   load-bearing content.
3. **`fg_quality_scorer` README line** — quoted via
   `FG_MEASUREMENT_METHODOLOGY_PRIOR_ART.md`, not re-opened first-hand → marked
   *(to verify)* rather than given a fabricated line number.
4. **No ms-level motion/latency JND** survives (§2.D, §4 gap 1); **no validated
   per-artifact FG rubric** survives (§2.E, §4 gap 3); **no high-speed-camera panel
   methodology** survived the sweep (§2.D, §4 gap 2). These are stated as open, not
   papered over.
5. **Single-observer correlation ceiling** is bounded by the operator's test-retest
   self-consistency, which **has not been measured** for this rig — so no PLCC target
   can yet be stated honestly; measuring it is part of the designed loop (§3).
6. **BT-vs-TM tractability** — the Handley "whereas TM does not" line overstates (TM
   is ML-fittable, just less conveniently); read "tractable" as attached to BT, the
   one split vote (2-1) in the sweep.
7. **"Objective metrics not well correlated"** is solid for PSNR/SSIM and for
   FG/VFI/out-of-distribution content; VMAF reaches PLCC ~0.96 *in-distribution* — do
   not overgeneralize to "all metrics, all content."
