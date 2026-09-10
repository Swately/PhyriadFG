# 1. Introduction

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 2 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## I.1 — The gap: what the swept literature does not hold (N1–N3, stated as time-boxed absences, not proofs)

*serves:* CB · *evidence:* `docs/research/FG_METRIC_MODEL_PRIOR_ART.md` §4 · *status:* `EXISTS`

> **CORRECTION, 2026-09-10 (after this section was drafted; `../../../LEARNING_LOG.md` P-032).** The searches behind
> this table were bounded to the video-frame-interpolation and image-quality literature and never entered real-time
> graphics, where the object under test is published. An adversarial re-sweep refuted **N1** — *Amulet*
> (arXiv 2608.10423, 11 Aug 2026) scores DLSS Frame Generation running live in the Falcor engine against
> "ground-truth images created with standard deferred rendering for every frame", and *Mob-FGSR* (SIGGRAPH 2024)
> references frames "at desired times" between two rendered frames — and **N2**, by *Image-Based Bidirectional Scene
> Reprojection* (SIGGRAPH Asia 2011) §4. **N3 survives only in half**: no run-to-run reliability was found for a
> fidelity metric, while *The FID Lottery* (arXiv 2606.20536, 2026) supplies the generative-metric analogue. The gap
> this section may claim is therefore not the phase-exact protocol, which is established practice in graphics, but
> the instrument: named geometric terms per object, the disocclusion bucket scored apart, a conjunctive verdict, a
> gate seen red first, and a reliability figure on every number. Every graphics paper found evaluates with aggregate
> PSNR / SSIM / LPIPS / FLIP. This slot is rewritten when the operator decides whether Phase 1 re-opens (KAP §7).

Claim (b) is an absence, worth what the search behind it is worth. The searches are
recorded as "time-boxed searches, not proofs of absence" (`FG_METRIC_MODEL_PRIOR_ART.md` §4, the source of
every cell below) and are carried at that strength: each says a bounded search did not find the work, not
that none exists. Three are load-bearing; §02.6 carries the full set with each search box and date.

| id | the absence | the nearest thing the searches found |
|---|---|---|
| N1 | Prior work scoring an interpolation at an arbitrary continuous phase against an analytic 3-D scene re-evaluable at any t. | Kiefhaber 2024 (Q3-6), 7 fixed lerp points; Middlebury's synthetic set re-rendered at t + 0.5. |
| N2 | The four-way visibility taxonomy: both / only A / only B / neither. | Kiefhaber's 0-occ / 1-occ / 2-occ split, which does not say which input; Middlebury's occluded/unoccluded masks. |
| N3 | A VFI or generative-evaluation metric reporting its own run-to-run reliability. | The VMAF confidence interval (Q5-5): model uncertainty from bootstrapped training ratings, not run-to-run variance (`FG_METRIC_MODEL_PRIOR_ART.md` §1); subject test-retest (Q5-7). |

## I.2 — The claim: the thesis sentence, verbatim from the frozen block

*serves:* TH · *evidence:* `docs/research/FG_METRIC_MODEL_SPINE.md` identity block · *status:* `EXISTS`

The claim, verbatim from the frozen identity block:

> "The fidelity of a real-time frame generator is measurable against an exact analytic truth: a 3-D
> rigid-body scene with closed-form pose(t) is re-rendered at the phase the generator actually produced,
> the error of each generated frame is decomposed into named terms — position error (px), hallucinated
> mass (px²), missing mass (px²), shape (px), sharpness, background error — with the disocclusion bucket
> reported apart, and every cited number carries its run-to-run reliability or the label that it has none."
> (`FG_METRIC_MODEL_SPINE.md` identity block)

The terms, their frozen priority order and the verdict are §03.3–§03.4; the reliability rule is §04.4.

## I.3 — The object and the boundary in one paragraph (kernel, builds, paths; the families; what today carries rows)

*serves:* OB · SS · *evidence:* the identity block; `docs/evidence/B1_FIRST_FG_ROW.md` header; `docs/planning/records/GDUMP_GATE.md` · *status:* `EXISTS`

The object, as the identity block states it, is PhyriadFG's shipping default kernel, `fg_core.comp`, under
its default flags, at the build each row names: `f2e4a9a` for the synchronous `--qdump` rows, and the tree committed as `0df0332` (md5
`186CB46C`; base `364efc5`, md5 `F36FDE32`, `GDUMP_GATE.md` header) for the `--gdump` rows. Both present
paths are in scope — the `--qdump` sampler on the synchronous path, the `--gdump` every-tick tap on the
asynchronous shipping path — and a claim about a path rests only on rows measured on that path at a named
build. Sources are 640×360 frames decimated by k from a 240 fps analytic base; the canonical row is k = 4,
a 60 fps source, on `mixed`, seed 7, barcoded (`B1_FIRST_FG_ROW.md` header); k ∈ {2, 8, 16} and 1080p enter
only where a measured row exists. The boundary is at most nine analytic scene families: the built
presets of `tools/scene_truth/scene_zoo.py` other than `static` — `translate`, `occlude`, `spin`, `cross`,
`mixed` — with `static` the verify gate's control, which carries no FG row and stays outside; and families
named TO BUILD, one per regime the operator named (F1 extremely fast or erratic motion; F2 thin, repetitive or complex-patterned objects;
F3 abrupt scene changes; F4 low source frame rate). A family enters a claim only where a measured FG row
exists, and today only `mixed` carries FG rows: the synchronous rows of §05.1–§05.4, and on the
asynchronous path one row, pos 0.240 px, one run — reliability not measured (`GDUMP_GATE.md` G5), in §05.5.

## I.4 — Contributions, enumerated, each pointing at the section that carries it; the regime contributions stated as CONDITIONAL until their §05 slot is filled

*serves:* CA · CB · ET · *evidence:* the identity block; §05 slot states · *status:* `EXISTS`

1. **The instrument** (§03): the analytic scene, exact-phase alignment, the six terms in their frozen
   priority order with the disocclusion bucket apart, the rulers, and its gate.
2. **The object and the protocol** (§04): the kernel at named builds, both present paths, the A/B rule, and
   the reliability protocol — two corpus seeds, run-to-run agreement beside every number, the 20 % rule,
   the one-run label (identity block).
3. **Measured rows on `mixed`** for claim (a), that the terms separate phase, displacement and source rate:
   the canonical row with its rulers (§05.1), the phase signature (§05.2), the displacement axis (§05.3,
   two-sided: two laws fit, the choice the operator's), the source-rate axis (§05.4).
4. **The bounded absences** N1–N3, the whole of claim (b) (§02.6).
5. **The evidence discipline** (§10 for reproduction): a load-bearing number is [V1] — a run, its command,
   its quoted output line, its record; prior art at the level the Phase 0 table assigns, nothing [V3] as
   fact (identity block).

CONDITIONAL until their Results slot is filled: the asynchronous path's second seed (§05.5), the built
presets with no FG row (§05.6), and the regimes F1–F4 (§05.7–§05.11) — F1's erratic component, F2 and F4
wait on scene families that do not exist yet. The provenance readings (§05.12) are readings; attribution
would exceed claim (a).

## I.5 — What is NOT claimed (the block's last paragraph, in the paper's words, nothing added)

*serves:* N-Q · N-R · N-L · N-D · N-G · *evidence:* the identity block · *status:* `EXISTS`

Not a quality metric: no human-opinion data, no MOS, no claim about perceived quality, and no measured
comparison with LPIPS / FloLPIPS / PSNR_DIV at all — whether they resolve the 0.2–0.5 px band the
instrument measures is unmeasured here (the identity block, whose last paragraph this section restates); the swept prior art states no pixel-scale sensitivity
floor for them (absence N6), documents DISTS as tolerant to translation (Q2-4) and LPIPS-class metrics as
registering misalignment imperceptible to a human (Q2-12). Not real content: every scene is rendered, and
transfer to real game content is unmeasured and is stated as such beside every number (Phase 0 row Q3-7,
the sim-to-real objection, carried as a limit and not answered). Not a learned model: the reference-free
predictor of the error from the kernel's own decisions (candidate I-B) is the next body of work this
identity authorizes and does not contain. Not a distribution metric (candidate I-C, refuted for fidelity by
Phase 0 rows Q1-9, Q1-10). Not a claim about any frame generator other than the object under test. §07
carries each limit.

*Made with my soul - Swately <3*
