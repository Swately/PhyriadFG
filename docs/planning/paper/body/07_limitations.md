# 7. Limitations

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 8 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## L.1 — Not a quality metric: no MOS, no perceptual claim, no measured comparison with LPIPS / FloLPIPS / PSNR_DIV

*serves:* N-Q · *evidence:* the identity block; `FG_METRIC_MODEL_PRIOR_ART.md` Q2-4, Q2-12, N6 · *status:* `EXISTS`

This is not a quality metric: no human-opinion data, no MOS, no claim about perceived quality. There is no measured
comparison with LPIPS / FloLPIPS / PSNR_DIV at all — whether they resolve "the 0.2–0.5 px band the instrument
measures" (the identity block) is unmeasured here. The swept literature states no pixel-scale sensitivity floor for
LPIPS / DISTS / FloLPIPS / VFIPS: a time-boxed search, not a proof of absence (`FG_METRIC_MODEL_PRIOR_ART.md` §4 N6).
What it holds is qualitative — DISTS "relatively insensitive to geometric transformations" (Q2-4), LPIPS-class
metrics "sensitive to a small alignment error that is imperceptible to the human eyes" (Q2-12) — and neither
measures this band.

## L.2 — Not real content: transfer unmeasured, stated beside every number

*serves:* N-R · *evidence:* the identity block; Q3-7 · *status:* `EXISTS`

Every scene under test is rendered. Transfer to real game content is unmeasured and is stated as such beside every
number; this identity carries the sim-to-real objection as a limit, not answered (the identity block). At abstract
level: "progress on Sintel, KITTI and Spring only weakly predicts accuracy on real-world data" (Q3-7). That row is
flow, not VFI; no VFI-specific sim-to-real source was found (Q3-7).

## L.3 — Not a learned model; not a distribution metric; not a claim about another generator

*serves:* N-L · N-D · N-G · *evidence:* the identity block · *status:* `EXISTS`

Not a learned model: the reference-free predictor of the error from the kernel's own decisions (candidate I-B) is
"the next body of work this identity authorizes and does not contain" (the identity block). Not a distribution
metric: candidate I-C, refuted for fidelity by Phase 0 rows Q1-9 and Q1-10 (the identity block). Not a claim about
any frame generator other than the object under test (the identity block): no other kernel was measured.

## L.4 — What is one run, what is owed, what is unmeasured (the list, with the record that owes each)

*serves:* RE · ET · *evidence:* `B1_FIRST_FG_ROW.md` §5; `B1_SPEED_TEST.md` §6; `REGIME_TEST_MATRIX.md` §9 (not run) · *status:* `EXISTS`

- **One run.** The 0.5× / 2× / 4× points of the speed path are single runs (`B1_SPEED_TEST.md` §6); the k = 4 point
  "has DI-3 (seeds 7 and 11 within 13 %)" (`B1_SPEED_TEST.md` §6). "The matched-pair gap (1.10–1.72×) is larger than
  the k = 4 seed spread (2.6 % on the sphere) … but it is owed its second seed." (`B1_SPEED_TEST.md` §6)
- **Not ablated.** "§4 names mechanisms; nothing has been switched off to confirm them." (`B1_SPEED_TEST.md` §6)
- **Sampled, not every tick.** Every row samples ticks on the `--qdump` path; at k = 2 the subset is 5 frames
  (`B1_SPEED_TEST.md` §6). The file backend (B2) — every tick, the async path, no sampler — is owed
  (`B1_FIRST_FG_ROW.md` §5).
- **The content.** The record: "the 'no cliff to 13 px/pair' holds for this content and is not a statement about a
  game" (`B1_SPEED_TEST.md` §6).
- **Also owed** (`B1_FIRST_FG_ROW.md` §5): the FG's own curve at k = 2 and k = 8 live; provenance on the worst
  genuine frames.
- **Lost.** The raw k = 4 sampler capture behind the canonical row was deleted (P-028); the scored rows stand, its
  provenance replay does not: "A re-capture of `sc_live` at k = 4 is a new sample, not a restoration."
  (`B1_FIRST_FG_ROW.md` §5)
- **Not run** (`REGIME_TEST_MATRIX.md` §9): any live capture (the operator's screen); the second-seed rows of the
  re-scored speed law; `period_backdrop`; a T3 bar that scales with displacement; the marker chain on a real
  capture; `scene_pages` rows for coverage-scored runs.

*Made with my soul - Swately <3*
