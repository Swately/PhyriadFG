# REGIME_TEST_MATRIX.md — the operator's four fidelity regimes: what limits the FG in each, what the instruments can already measure, and the test matrix that discriminates

Status: **`proposed · corpora rendered, tooling built and gated 2026-09-10`** (§8, §9) · Type: **Planning / design** (FDP §3: goals, non-goals, alternatives, trade-offs; nothing here
is shipped or measured by this document — every number below is quoted from the record that owns it). **The operator decides
which families run and when: every live capture takes his screen.**

**The operator's words (2026-09-09, verbatim):** *"el objetivo ideal es poder ajustar lo maximo posible la exactitud de los
frames generados y en sus escenarios tipicos: movimientos extremadamente rapidos o erraticos / objetos delgados, repetitivos o
patrones complejos / cambios bruscos de escena / baja tasa de fps"* — and *"necesitamos hacer mas pruebas donde la velocidad
sea mucho mayor de los objetos en movimiento"*.

**Relation to the frozen identity.** [`../research/FG_METRIC_MODEL_SPINE.md`](../research/FG_METRIC_MODEL_SPINE.md) freezes the
paper's object and boundary: the six `scene_report.py` verdict terms on `scene_zoo.py` families, with four families F1–F4 named
TO BUILD. This document serves the **tuning objective**, which is wider: it uses BOTH analytic instruments — `tools/scene_truth/`
(silhouette terms, the paper's instrument) and `tools/motion_truth/` (`marker_zoo.py` → `play_frames.ps1` → `--qdump` /
`--gdump` → `marker_extract.py` → `motion_report.py`: barcoded markers with a closed-form p(t), per-class × per-size error tables,
per-phase bins, the M1 records) — because the marker zoo was BUILT for exactly these regimes: "the zoo deliberately includes
sub-block (< 8 px) markers, periodic-grating backgrounds (aperture), crossings (occlusion) and the static-HUD-over-pan case"
(`MOTION_TRUTH_MASTER_PLAN.md:139-141`). A marker row steers a knob decision and decides which `scene_zoo` family is worth
building; it does NOT enter the paper's demonstration (the identity admits only rows in its own terms). Nothing here touches the
identity block. Workload note: the frozen objective named the zoo "at 60 and 120 fps … 1920×1080" (`aap/A0_FROZEN_OBJECTIVE.md`
§5); every M1 record so far is 1280×720 @ 60 fps (marker_zoo's defaults) — the operator's call which to run here.

**Method.** Workflow `wf_cce9fbbd-22b` (2026-09-09/10): six Sonnet readers (one per question), one Opus designer, three Opus
verifiers with distinct lenses (code-truth; measurement validity; anti-recurrence + rules) — the verifiers hit the session limit
on the first run and were re-run from cache. All three returned **`PARTIAL`**: 31 issues, none fatal to the matrix, four of them
changing a family's design (§6). The supervisor re-verified first-hand every claim a design decision rests on (§6); readers',
designer's and verifiers' outputs are claims, what §6 lists as checked is fact.

---

## §1 — What limits fidelity in each regime, on the record (verified)

1. **Extremely fast motion.** The matcher is a 4-level pyramid with coarsest radius 6 → a reach of **±48 px per source pair**
   at 640×360 and 1080p (`framework/render/vulkan/src/OpticalFlowPipeline.cpp:149-156`, `.hpp:319` kLevels = 4; depth loop keeps
   the coarsest level ≥ 32 px). The wider radius (8 → ±64 px) exists but `coarse_wide` is hard-wired `false` at both init
   sites (`src/flow/flow_init.cpp:40,142`) and no flag exposes it. The speed test reached **13.28 px/pair** with no cliff
   (`docs/evidence/B1_SPEED_TEST.md` §2: pos ≈ 0.30·disp^0.32), 3.6× inside the reach — so search-window exhaustion is NOT what
   limited the measured range; the provenance reading names the guided pick's bilinear fallback (99 % of the sphere) and stasis
   (18 %) instead, "readings, not proofs of cause" (§4). B1 §4 also names the operator's first measuring session in those words:
   "an A/B with stasis off, and one with the guided pick's threshold opened, on this corpus, would turn the reading into an
   attribution" (lines 108-110). **Above ~48 px/pair the failure must change in kind**; that is untested.
2. **Erratic motion.** The default carries no temporal MV state: `mv_smooth = 0` (off) and `mv_prior = false`
   (`src/control/cli.hpp:359-361`); the EMA's reversal guard is a fixed `kMvCut = 6.0f` px (`src/flow/flow.cpp:478`). No corpus
   in either instrument reverses direction: `marker_zoo.py` has `linear / accel / circular / crossing / hud / fast`
   (`traj()` lines 175-186 raise on anything else); `scene_zoo.py` is constant velocity + constant spin (`Obj.pose`,
   lines 117-134), `--speed` only scales both. **"Erratic" is the one regime with no content today.**
3. **Thin / repetitive / complex patterns.** The SAD block is 8×8 at flow resolution. The shipped defences are default-ON:
   `mv_candsel` (an ambiguous tile adopts the coarse region MV unless its own match beats it by 12 %, `cli.hpp:947-954`),
   `ambig` (best vs runner-up MV arbitrated by the gme background model — **object pixels are exempt**, `cli.hpp:647-648`),
   `mv_consensus`, `mv_guided`; `mv_median` is default-OFF. **Stale record on `mv_candsel`:** the struct is `true`
   (`cli.hpp:947`) and both init sites pass `cfg.mv_candsel` (`flow_init.cpp:40,142`), but the usage line (`cli.cpp:104`), the
   `--mv-candsel` printf (`cli.cpp:809`), `FG_VFI_PRIOR_ART.md:639` ("built, default-OFF"), `PHYRIADFG_PERFECTION_ROADMAP.md:101`
   and `catalog/cpp/docs/evidence/baselines/FG_PERF_BASELINE.json:21` all say OFF — they describe the framework's generic
   default, not PhyriadFG's (P-027). The marker instrument already stratifies by size 6 / 12 / 24 px (`marker_zoo.py:303`), but
   **no size effect is citable today**: r is computed per class only (`motion_report.py:84-96`), the per-size table carries no r
   and no `absent` column, the classes with size rows that look monotone (linear 0.912 / 0.632 / 0.242 px) are UNRELIABLE
   (r −0.13), and the only reliable moving class, circular, is NON-monotone (0.811 / 0.999 / 0.423, `M1_SRC60.md:26-28`). Two
   instrument facts bound any thin-object claim: the static `hud` class reads **0.573 / 0.034 / 0.011 px at size 6 / 12 / 24**
   (`M1_SRC60.md:30-32`, identical in `M1_SRC30.md`) — a size-dependent pedestal on content with no motion — and in one corpus
   size is confounded with speed by construction: `s = sizes[mid % 3]` while `spf = 0.5 + 7.5·(j + 0.5)/per_class` rises with the
   same index (`marker_zoo.py:203-216`), so size 6 is always the slowest marker of its class. It also ships a hard 24 / 96 px
   lattice background (`--bg grating`, `marker_zoo.py:90-91`) with `--bg-pan` — **never played to the live FG**: none of the six
   runs under `C:\PhyriadFG\runs\` nor any `docs/evidence` record used it (absence in what was searched, not a proof).
4. **Abrupt scene changes.** The FG has **no cut detector at any level**: `--mv-prior` says so in its own comment
   ("self-healing on cuts, no detector"), the EMA bypass and the stasis / change gates are per-tile. The only measured cut is
   the loop seam: "2,865 px² of hallucinated mass on the sphere — a doubled sphere with a seam, sharpness 0.515, the run's
   worst by a factor of ten", 197.7 px of displacement for an object that moves 3.4 (`B1_FIRST_FG_ROW.md` §4). The scorer
   **counts and excludes** cut frames — `return ('cut', None)`, "Counted, never scored" (`scene_report.py:350-356`) — so the FG's
   behaviour AT a cut has never been graded. The looped player injects a cut at every seam (`scene_live.ps1:34-37`), so every
   archived run holds cut frames — but the `--qdump` sampler caught ONE per 20 s capture (133 scored, one excluded, B1 §1), and
   the marker chain mis-models the seam (`span = (k_next − k_prev) % T` turns the wrap into span 1, `marker_extract.py:258-264`),
   scoring the seam marker as `absent`.
5. **Low source frame rate.** Two instruments disagree, and the record's own account of one of them is wrong. `M1_SRC_RATE.md:21`
   says "p(t) is parameterised in SECONDS, so the 30 fps corpus is the same motion with twice the [displacement]"; the generator
   says otherwise — speeds are authored in px PER FRAME and `v_px_s = spf * fps` (`marker_zoo.py:213`), so displacement per source
   frame is invariant under `--fps`. **M1_SRC_RATE was already a matched-displacement comparison** (30 fps × 8 vs 60 fps × 4 at
   the same px/pair; P-026), and its "Indistinguishable" (line 42) is a statement about the multiplier and the phase density,
   not about displacement. The silhouette instrument's k path — "1.10–1.72× worse than the speed path at matched displacement
   AND phase" (`B1_SPEED_TEST.md` §3) — is structurally the same comparison with the opposite answer; its 1.72× cell is a
   0.126 px absolute gap (0.302 vs 0.176 px, n = 19 on one side, single seed), close to the capture-path floor of
   0.116–0.119 px (`M1_SRC_RATE.md:29`). `asw = true` is the struct default (`cli.hpp:222`): bounded forward extrapolation that
   fires exactly when the source falls behind the display — a confound in every low-fps arm unless `--no-asw` is passed.
6. **Cross-cutting: `mv_guided` is default-ON and is the largest single term on the record** — 1.668 px at t ≈ 0.1 on the
   default vs 0.271 px flat across phase with `--no-mv-guided`, two runs per condition with the run A / run B split printed
   (`records/M1_LOWPHASE_FINDING.md:38-43`). Any regime measured only at the default is partly a measurement of that layer. Its
   default is **the operator's decision** (memory `phyriadfg-verification-as-data`), untaken. The whole-corpus "~3× on every
   moving class" figure (`MOTION_TRUTH_CONTROL_NO_MV_GUIDED.md` vs `M1_SRC60.md`) is NOT citable: no class clears r ≥ 0.5 on both
   sides (control: linear 0.47, circular 0.44, crossing 0.33, fast 0.17 UNRELIABLE; default: linear −0.13, accel −0.00,
   crossing −0.24 UNRELIABLE), the control corpus has no `hud` class, and n is 96 vs 288.

**What no instrument has today:** a reversal / sign-flip trajectory; a periodic MOVING backdrop in `scene_zoo` (its
`BACKDROP` is aperiodic noise with no `vel`, lines 217-218); a graded cut; phase bucketing inside `scene_report.py`
(every phase table in the B1 docs is post-hoc filtering); size-normalised silhouette terms (halluc/missing are absolute px²
against a flat 1.5× + half-perimeter slop, `scene_report.py:425-428`); per-size r in the marker report.

**Two instrument properties every marker family must respect:** `err_model` is a survivorship statistic — computed over
`found` rows only (`motion_report.py:60`) inside a search window of R = ceil(max px/frame) + 4 ≈ 20 px (`marker_extract.py:244-245`),
so an error beyond ~20 px is reported as `absent`, not as error, and a knob that changes the detection rate changes the population
the mean is taken over (report the PAIRED subset, the rows found in both arms, beside the unpaired means; `detections.csv` carries
`triple / marker / size / n_peaks`). And the loop seam is scored `absent` (item 4): drop rows with `k_next < k_prev` from every arm
and print the excluded count, as `scene_report.py:400-402` does.

---

## §2 — The matrix (eight families; each carries its predicted signature BEFORE its first row)

Conventions: **NULL arm** must not move; **KNOWN-POSITIVE arm** must move or stay bad; the gate is **seen red** before any green
is believed (EMPIRICAL_TEST §3); every number twice (DI-3), the run-to-run r reported, r < 0.5 carries no verdict (the marker
records' own rule, per class — the size cells are read WITHOUT their own r, and the record says so); `fast` is the marker
instrument's presence check ("must be the worst class in its own capture and its `absent` count must stay high" — the default
span is 4.87–6.12 px across five records; 2.283 px at `--no-mv-guided`, so no absolute bar); the `hud` null is read PER SIZE
(0.573 / 0.034 / 0.011) from the SAME capture, never as the 0.206 px class mean; `--bg-pan 0` is passed explicitly wherever the
`hud` class is present (its absence auto-arms a 120 px/s pan, `marker_zoo.py:299-301, 317`); the `mv_guided` level is STATED in
every record. Screen seconds: `scene_live.ps1` takes the screen for ~Seconds (`:9`, `$Seconds = 20` at `:28`) and the corpus must
outlast 3 s + Seconds (`:34`); the marker player loops until stopped, so its wall-clock is whoever stops the FG. Runs on
`C:\PhyriadFG\runs\`. Every record prints n per arm and the excluded-seam count.

| # | family | regime | instrument | built? | A/B knobs (exact) | predicted signature | null / known-positive / red-first | screen |
|---|---|---|---|---|---|---|---|---|
| 0 | **attribution** — the session B1 §4 already names | the reading behind every fast-motion number | scene_truth on `sc_live` k = 4 (KEEP corpus, no render) + the async row's DI-3 second run | BUILT | default vs `--no-stasis` (`cli.cpp:657`) vs the guided pick's band opened, `--mv-sim 0.30` (default 0.10, clamp [0.02, 0.5]); `-Gdump` 20 s each; plus one `scene_live.ps1 -Gdump` 20 s on the second seed for the async row (pos 0.240 px, one run) | stasis off: the +lead sign shrinks or flips and the stasis bit vanishes from `ref_warp --decisions`'s `stage.u8` (B1 §4 reading 2); band opened: the 99 % bilinear fallback on the sphere drops and the rim's `shape_err` tightens (reading 1). Either turns a reading into an attribution; neither is predicted to move `pos_err` by more than the 13 % seed spread | null: the `truth` / `nearest` / `oracle2` rulers, same corpus, must reproduce B1's row; the default re-run must land on 0.216 / 0.240 inside spread; red-first: the provenance planes must show the stasis bit on the default BEFORE its absence is read on the arm | ~100 s (3 arms + the DI-3 run) |
| 1 | **thin_size** | thin objects | marker_zoo, ONE CORPUS PER SIZE (`--sizes 6`, then `--sizes 24`, same `--seed`, `--bg noise --bg-pan 0`) so the speed band is identical and size is the only difference | BUILT (CLI values; one extra CPU render) | default vs `--no-mv-candsel` (`cli.cpp:344`; the arm is recorded by the printf actually emitted — the `--mv-candsel` help says "DEFAULT OFF" and lies); `mv_guided` held at default and stated | an INTERACTION: at size 6 the 8-px tile is background-dominated, so candsel's adopted "region MV" is the background's — `err_model` falls with candsel off on the paired subset, size 24 unmoved. The room is bounded: linear size 6 = 0.912 px of which 0.573 px is the static pedestal, so the effect must exceed **0.573 px's reproduction** and the seed spread, not just the spread. A main effect in either direction kills the hypothesis and points at the 8×8 grid (compile-time) | null: `hud` per size from the same capture (void if size 6 ≠ 0.573); positive: `fast` presence; red-first: read the `prev` / `next` planes at size 6 to settle whether 0.573 px is extractor bias or an FG effect on static content (CPU, no screen) | ~200 s (2 sizes × 2 arms × 2 runs) |
| 2 | **grating_pan** (staged) | repetitive patterns | marker_zoo, `--bg grating --bg-pan 120` vs `--bg noise --bg-pan 120` | BUILT, never played live | stage A content only; stage B `--no-ambig` (`cli.cpp:736`, documented byte-identical off) only if A separates | period-lock = SAD ties one period apart: `absent` / `ghost` rise (a 24-px-wrong warp moves a 6–12 px marker out of the ~20 px window), err p95 gains a mode near the lattice period; `--no-ambig` worse. Because `ambig` EXEMPTS object pixels, a null here is "not visible to this instrument", never "period-immune" | **null: `--bg grating --bg-pan 0`** — the same periodic field with zero background motion, so stasis owns it and the tie arbiter is never reached; it must be indistinguishable from `--bg noise --bg-pan 0` (the noise-pan-120 arm is the COMPARISON, not a null); positive `fast`; red-first: `--objdump N` (`cli.hpp:363`) the MV field on a grating capture and confirm a 24-px error would show | A ~150 s (3 arms × 2 runs), B ~100 s |
| 3 | **speed_extend** | extremely fast | scene_zoo `--speed {1, 8, 16}` at k = 4, `-Gdump` | BUILT for the ×8 row; the ×16 row needs a preset variant | content only; FG arm default vs `--no-stasis`. The reach-widening arm does NOT exist as a flag (`coarse_wide` hard-wired false) | ×8 (~27 px/pair, inside the ±48 reach): pos follows 0.30·disp^0.32 (~0.86 px), no change in kind. ×16 (~54 px/pair, OUTSIDE): the failure changes KIND — but `pos_err` alone cannot show it: the scoring window is `dilate(truth, NEAR_R + ceil(disp))` (`scene_report.py:62, 198`), so a candidate painted ~disp away sits at the window edge and its centroid is clipped back toward truth — pos_err is censored near disp + 6 px. The readable signature is therefore `lead` swinging toward TRAILING (the default's lead already falls with speed: +47.8 → +24.0 from ×0.5 to ×4), `missing` jumping, and sharpness possibly RISING (a sharp wrong interior) — the conjunctive verdict, not the position term | null: `--speed 1` re-run lands on B1's k = 4 row (0.481 / 0.468 px) inside 2.6 %; second null: the spinning box at ×0.5 (0.88 px/pair, under the ~1 px floor); **red-first: gate T2 at the NEW speed** — `nearest` vs the truth centroid's own travel, p90 \|resid\| < 0.25 px (`scene_report.py:481-484`) — a T2 failure at ×16 means pos_err is censored, not that the FG is fine; T3 `blend` rejected. **Before any screen:** at ×16 the sphere leaves the view after ~0.21 s of the 1.0 s corpus (×8: ~0.41 s; f = 554 px at fov 60, half-width 3.46 units at z = 6) — count on-screen frames from the `id/` plane and report n on that subset, or render a variant with the sphere's start x shifted / `--seconds` raised (a preset edit in his instrument — announced, not built) | ~100 s (2 speeds × 2 seeds) |
| 4 | **src_rate_matched** | low fps | marker_zoo `--fps 30` vs `--fps 60`, **identical marker parameters** (displacement per pair is matched by construction), the multiplier held with `--fg-factor`, **`--no-asw` in every arm** | BUILT | second A/B: default vs `--no-mv-guided` (`cli.cpp:645`) at each rate | the open question is not "nobody matched displacement" but "two instruments made the same matched comparison and disagreed". If B1 is right: the 30 fps side is worse and the excess sits at HIGH phase (> 0.75) as a phase-binned gradient (the marker report prints phase bins). If M1 is right: the sides overlap inside r, and B1's k-path term was an ASW / multiplier artefact. Separable second prediction: the default-vs-`--no-mv-guided` gap WIDENS at 30 fps. **Pre-registered absolute bar:** a source-rate gap counts only above the 0.116–0.119 px capture floor AND both sides' spread on a class with r ≥ 0.5 on both sides | null `hud` (per size, 0.206 mean on both rates today); positive `fast` presence only — its direction is NOT a gate (the record already has fast 5.462 at 30 vs 5.995 at 60 fps, "30 fps better", r30 0.35 unreliable, `M1_SRC_RATE.md:56`); red-first: the capture-path floor line and `fps=30.0 target=30 missed=0` in the log | ~200 s (2 rates × 2 knob levels × 2 runs) |
| 5 | **cut_score** | scene cuts | scene_truth, **`-Gdump`** captures of a short looped corpus (the every-tick tap records all k − 1 generated frames at every seam; the archived `--qdump` runs hold one cut frame each) | scorer change TO BUILD (bounded) | none exists to switch; two no-effect predictions: `--mv-prior` on/off; `--mv-smooth 0.5` vs 0 | no interpolation truth at a cut → score the frame against BOTH real endpoints on the terms that survive: `halluc_px` vs each, `sharp`, and `graceful` (RMS to the nearest real frame, computed today for the disocclusion bucket, `scene_report.py:20-22`). Default: large, roughly symmetric halluc, sharp ≈ 0.5, graceful far above the within-scene value. HOLD policy (ideal): halluc vs the nearer real → ~0, graceful → 0. Deliverable = a magnitude per phase bin and a specification for a detector, not a tuning result | null: the same corpus WITHOUT `-Loop` (zero cuts; every non-cut term reproduces inside spread; corpus > 23 s); **red-first: synthetic `nearest` and `blend` built AT the seam endpoints** (`e['N']`, `e['N1']` from `align.json`) — nearest must give graceful ≈ 0 and blend must be rejected; T3 does NOT traverse the cut branch (synthetic arms have no `align.json`, `scene_report.py:315-320`) | ~50 s (2 captures) + ~50 s for the no-loop null |
| 6 | **reversal** | erratic | marker_zoo, a new `reverse` class with a CLOSED-FORM trajectory (e.g. p(t) = p0 + A·sin(ωt)) | TO BUILD (two edits in `marker_zoo.py`: a class branch, a `traj()` type; extractor and report unchanged) | default (both temporal carriers OFF) vs `--mv-smooth 0.5` (`cli.cpp:529`) vs `--mv-prior` (`cli.cpp:530`); repeat at `--no-mv-guided` | default: a reversal costs no more than its instantaneous displacement (nothing carries direction); a departure indicts the other temporal carriers B1 §3 names (PLL clock, inertia, gme history). `--mv-smooth 0.5`: the EMA LAGS unless its 6-px cut fires → error vector points BACKWARD along the new velocity — the opposite sign of the forward lead measured on constant velocity (the along-velocity projection M1_LOWPHASE already computes) | null `linear` matched on \|v\| and `hud` per size; positive `fast` presence, and for the EMA arm a marker whose reversal magnitude sits BELOW `kMvCut = 6.0` px so the bypass cannot fire — it MUST lag, or the EMA is not running | ~150 s (3 conditions × 2 runs); +100 s at `--no-mv-guided` |
| 7 | **period_backdrop** (conditional) | repetitive patterns, the silhouette escalation | scene_zoo: a new preset with a `checker` BACKDROP (period ≈ 16–32 screen px) moving 2–6 px/pair | TO BUILD (data-only: one preset + a per-preset BACKDROP override; no new texture, no new term) | default vs `--no-ambig`; second arm `--no-mv-candsel` | `bg_err` (definition `scene_report.py:17`) is IN the verdict (`:431`, floor + 0.01) and sits exactly where `ambig` acts: default holds bg_err at the aperiodic floor; `--no-ambig` raises it and the verdict gains `bg`. Objects: pos/shape show a DISCRETE step near a period multiple (read off the pos-vs-phase scatter — an analyst's construction, labelled) | null: the identical scene with the backdrop at `noise`, same velocity (only the texture function differs); T1 must pass on the new corpus (the shared `object_like` operator meets a high-frequency background for the first time); positive `blend` rejected (T3), `blur` flagged (T5) | ~100 s after the corpus and the T1–T5 gate (CPU only) |

**Ordering (cheapest discriminating first):** 0 attribution (the session B1 already names; KEEP corpus; also the cheapest item in
the record, the async row's DI-3 second run) → 1 thin_size → 2 grating_pan A (→ B) → 3 speed_extend ×8 (×16 after its variant)
→ 4 src_rate_matched → 5 cut_score (gated on the scorer branch) → 6 reversal (after its build) → 7 period_backdrop only if 2
does not separate. **Before any of them, decide the `mv_guided` factor:** either every family carries both levels, or every record
states plainly that its numbers are conditional on a default the project's own measurement already questions.

**Screen budget:** families 0–4 ≈ 750 s (~12–13 min) exclusive; the full matrix ≈ 20–25 min plus two builds. Arms multiply
(2 knobs × 2 `mv_guided` levels × 2 seeds = 8 captures for one A/B) — the reason grating_pan is staged.

---

## §3 — The operator's question: can our own model (I-B) help automate the FG's tuning?

Three different questions hide in it; only one needs I-B.

1. **Offline optimisation over the knobs against `scene_truth` — needs NO learned model and is buildable today.** When the
   label is the exact truth, the error is measured, not predicted. The pieces exist: a knob surface of roughly 83
   generation-affecting flags, ~27 of them continuous thresholds (the tuning-surface reader's count over `cli.hpp`/`cli.cpp`,
   near-complete, not exhaustive); the conjunctive scorer; the capture harness; and R5's layer registry, which gives every
   configuration a machine-readable identity — `--layer-dump` prints the resolved chain and its contract hash (`cli.hpp:1069`,
   `records/R5_GATE.md:59`) — so a sweep's run directories are labelled by hash, not by a hand-copied flag string. **The binding
   constraint is the screen, not the search:** one candidate ≈ 25 s of exclusive screen, 100 candidates ≈ 40 min, two seeds
   double it. That rules out black-box optimisers over 27 dimensions and rules in one-factor-at-a-time over a shortlist, through
   `-Gdump` (every tick, the shipping path; `--qdump` starves at fast sources — 12 triples captured in 20 s at k = 2 against 400
   at k = 8, 5 of them scorable, `B1_SPEED_TEST.md:42-44`). And the honest observation: **such an optimiser's first find is
   already known and untaken** — `--no-mv-guided`, 1.668 → 0.271 px at low phase. The bottleneck on this branch is a decision
   about a default, not a search.
2. **Reference-free feedback on real content — needs I-B and its generalisation; nothing is measured.** The training set can be
   assembled today with no new instrument: the FEATURES are the per-pixel decision planes `tools/ref_warp.py --decisions`
   writes (`stage.u8` bitmask: guided-pick fell back / gate-b refused / bg_reclaim damped / ambiguity swapped / stasis / phase
   anchor; `w_s`, `mv_eff/mv_lin/mv_bwd`, `sad_best/sad_zero`, `d_pixel/d_zero`, `reclaim_w` — `ref_warp.py:162-172`) computed
   from the `--gdump` every-tick record; the LABELS are `scene_report.py`'s per-object terms. Three limits, all load-bearing:
   (a) the planes come from the CPU reference warp, which reproduces the kernel ("changes no computed value") — a predictor
   trained on them predicts the reproduction's error, and that identity is a gate result, not an assumption; (b) one scene
   family and one kernel today — a predictor of one kernel's error on one family is that kernel's diagnostic unless the
   trace features are shown generic; (c) the nearest prior art predicts VFI error from the interpolator's own FLOW
   (`FG_METRIC_MODEL_PRIOR_ART.md` Q4-2) with a lower-bound baseline of input pair + output only (Q4-7) — any I-B claim is
   scored against that flow-only baseline or it claims a novelty it has not earned. **This is where the matrix pays twice:**
   every family produces labelled rows, and the four regimes are exactly the distribution shift a reference-free predictor
   must survive; trained on `mixed` alone it would never have seen a thin object, a periodic field, a cut or a slow source.
   A reconciliation is owed: `docs/research/FG_COMPETITIVE_COMPARISON.md:111` says "live FG output has no ground truth →
   quality is permanently no-reference"; I-B is built to soften exactly that sentence.
3. **Runtime per-region adaptation — needs I-B at runtime cost, and the existing loop points the other way.** The only adaptive
   machinery today adapts to LOAD, not accuracy (the tier ladder / deficit tier / load governor shed object repair, scene
   memory and backward flow against the pair budget — reader claim at `cli.hpp:832-849`, not re-opened). No mechanism reads an
   error signal and moves a threshold; the ~27 continuous knobs are parse-time constants. An accuracy loop would pay I-B
   inference every pair inside a budget that already sheds quality under pressure: two controllers fighting, structurally.
   Do not start here.

**Answer:** yes, in sense (1) now, without a learned model; in sense (2) as the paper-shaped follow-on the matrix makes possible,
scored against the flow-only baseline; in sense (3) not until (2) has a measured cross-regime generalisation number.

---

## §4 — Risks and confounds (each with its design response)

- **`mv_guided` confound** (§1-6): state the level in every record; run both levels where the predicted effect is smaller than
  the ~6× low-phase term — every family here.
- **Two instruments contradict each other on low fps, and M1_SRC_RATE's own method sentence is refuted by the generator** (§1-5,
  P-026): family 4 holds the multiplier and removes ASW; until it runs, no source-rate number is the project's answer.
- **Stale defaults in help texts and docs** (P-027): `--asw` prints "DEFAULT OFF, byte-identical off." (`cli.cpp:448`) while the
  struct is `asw=true` (`cli.hpp:222`) and `--no-asw` prints "DEFAULT es ON" (`cli.cpp:353`); `--mv-candsel` prints "DEFAULT OFF"
  (`cli.cpp:104, 809`) while the struct is `true` (`cli.hpp:947`), and three documents repeat OFF. The header is authoritative;
  every record states the arm by the printf actually emitted; the source fixes are the operator's (his repo).
- **`ambig` exempts object pixels**: a marker-scored null on a periodic field is "not visible to this instrument"; family 7 is
  the escalation because `bg_err` looks where `ambig` acts.
- **The reach-widening arm is not a flag**: if family 3 finds a cliff, attributing it to the search window needs one bool
  exposed (`coarse_wide` at `flow_init.cpp:40,142`); without it the attribution stays named-not-proven, as B1's own k-path term.
- **The silhouette scorer's window scales with displacement** (`scene_report.py:198`): at extreme speed `pos_err` is censored;
  `lead`, `missing` and `sharp` carry the signature, and T2 at the new speed is the red-first check.
- **`err_model` is a survivorship statistic with a ~20 px ceiling** (§1): paired-subset deltas beside the unpaired means; R
  stated in the record.
- **`--qdump` is a sampler on the sync path and starves at fast sources**; fast or high-multiplier families use `-Gdump`.
- **The loop seam**: the generator for family 5 and a contaminant elsewhere — the silhouette chain excludes it and prints the
  count; the marker chain scores it `absent` (§1-4) — every marker record drops `k_next < k_prev` rows and prints the count.
- **The r ≥ 0.5 rule kills classes** (four of six on the 60 fps default record; no per-size r exists): a signal landing on an
  unreliable class yields nothing citable; the response is more triples, never a reinterpretation of r.
- **No phase bucketing in `scene_report.py`**; two predictions here are phase-shaped (family 4 at high phase, `mv_guided` at
  low phase) — the analysis is an analyst's construction and is labelled so; the marker report prints phase bins natively.
- **No size normalisation in the silhouette terms**: thin-object families are read on the marker instrument.
- **Screen time is exclusive and adds up** (§2 budget).

---

## §5 — What this plan does NOT claim

No family has run. No number in §1 is new; each is the record's own. The predicted signatures are hypotheses written before
the rows so that a green can be believed (EMPIRICAL_TEST §3), not findings. The marker instrument's rows will not enter the
paper's demonstration; they steer which `scene_zoo` family (F1–F4) is built, and each of those enters the identity's boundary
only under the entry rule the identity states.

---

## §6 — Verification status (KAP §9.2: a subordinate's finding is a claim)

**The three adversarial verifiers (Opus, clean context, re-run 2026-09-10 01:19 after the session limit):** code-truth
`PARTIAL` (11 issues, 22 files checked); measurement validity `PARTIAL` (12 issues, 25 checked); anti-recurrence + rules
`PARTIAL` (8 issues, 33 checked). Zero fabrications found; every quoted number in the design was exact; the issues were premises,
gates and locators. Consolidated and dispositioned:

| # | issue (verifier) | supervisor's first-hand check | disposition |
|---|---|---|---|
| 1 | the "~3× on every moving class" `mv_guided` figure is uncitable under the r rule (1-I1, 2-I7, 3-I4) | `MOTION_TRUTH_CONTROL_NO_MV_GUIDED.md` rows 9-13 and `M1_SRC60.md` rows 9-14 — confirmed | applied (§1-6): only the low-phase term is cited |
| 2 | thin_size's "measured, monotone size effect" rests on UNRELIABLE classes; circular is non-monotone; no per-size r (1-I2, 2-I2, 3-I2, 3-I3) | `M1_SRC60.md:9-14, 20-28`; `motion_report.py:84-96` — confirmed | applied: premise rewritten as "no size effect citable"; size cells read without r, stated |
| 3 | size is confounded with speed in one corpus (2-I1) | `marker_zoo.py:203-216` (`s = sizes[mid % 3]`, `spf` rising with `j`) — confirmed | applied: one corpus per size |
| 4 | the `hud` null is a size-averaged mean; size 6 reads 0.573 px static (1-I3, 2-I3) | `M1_SRC60.md:30-32` — confirmed | applied: per-size null and floor; red-first prev/next read at size 6 |
| 5 | `--bg-pan` default arms a 120 px/s pan when `hud` is present (1-I4) | `marker_zoo.py:299-301, 317` — confirmed | applied: `--bg-pan 0` explicit |
| 6 | src_rate_matched's premise is false — px/frame is invariant under `--fps`; M1_SRC_RATE was already matched (1-I5, 2-I6) | `marker_zoo.py:213` `v_px_s = spf * fps`; `M1_SRC_RATE.md:21` says the opposite — confirmed | applied: family re-scoped; **P-026** (a record premise refuted by the code) |
| 7 | the "fast under ~4 px voids" bar is invented; `fast` improving at 30 fps already happened (1-I6, 1-I7) | five default records 4.87–6.12 px; `MOTION_TRUTH_CONTROL_NO_MV_GUIDED.md:13` 2.283; `M1_SRC_RATE.md:56` — confirmed | applied: presence check, no direction rule |
| 8 | the 1.72× cell is a 0.126 px absolute gap, n = 19, single seed, near the 0.116 px floor (1-I10) | `B1_SPEED_TEST.md` §3; `M1_SRC_RATE.md:29` — confirmed | applied: pre-registered absolute bar |
| 9 | `--mv-candsel` help texts and three docs say DEFAULT OFF; the struct is `true` (1-I8, 3-I5) | `cli.cpp:104, 809`; `cli.hpp:947`; `FG_VFI_PRIOR_ART.md:639`; `PHYRIADFG_PERFECTION_ROADMAP.md:101`; `FG_PERF_BASELINE.json:21` — confirmed | applied (§1-3, §4); **P-027** with the `--asw` text |
| 10 | B1 §4 already names the operator's first session (`--no-stasis`, the guided band opened); the async DI-3 run is the cheapest item (1-I9, 1-I11) | `B1_SPEED_TEST.md:108-116`; `FG_METRIC_MODEL_PRIOR_ART.md` §6 — confirmed | applied: family 0 |
| 11 | A0's named workload (1920×1080, 60 + 120 fps) vs marker_zoo's 1280×720 @ 60 (1-I11) | `aap/A0_FROZEN_OBJECTIVE.md` §5; `marker_zoo.py:293-295` — confirmed | recorded (head note); the operator's call |
| 12 | grating_pan's "null" is the comparison condition; `hud` sits over a moving field (2-I8) | `marker_zoo.py:317` — confirmed | applied: third arm `--bg grating --bg-pan 0` |
| 13 | `err_model` is survivorship-censored at R ≈ 20 px (2-I4) | `motion_report.py:60`; `marker_extract.py:244-245` — confirmed | applied: paired subset, R stated |
| 14 | the marker chain mis-models the loop seam as span 1 → `absent` (2-I5) | `marker_extract.py:258-264` — confirmed | applied: seam filter + count in every marker record |
| 15 | cut_score: the archived sampler runs hold ~1 cut frame each; T3 never traverses the cut branch (2-I9, 2-I10) | `B1_FIRST_FG_ROW.md` §1 (133 scored, one excluded); `scene_report.py:315-320` — confirmed | applied: `-Gdump`, seam-endpoint synthetic arms |
| 16 | speed_extend: the window scales with displacement (censoring), and the sphere exits at ~0.21 s at ×16 (2-I11, 2-I12) | `scene_report.py:62, 198`; `scene_zoo.py` fov 60 / seconds 1.0 / sphere at x = −2.4, v = 2 — arithmetic re-done: exit at 3.28 / 0.41 / 0.21 s for ×1 / ×8 / ×16 | applied: T2 at the new speed; on-screen frame count first; the variant announced, not built |
| 17 | locators off by one to six lines (`bg_err` at `:17` not `:20`; `scene_live.ps1:9/28/34`; `--objdump` at `cli.hpp:363`; the chain sentence at `M1_SRC_RATE.md:4-6`; `R5_GATE.md:59`; `scene_zoo --fps :440`; B1 starvation `:42-44`) (3-I1, 3-I6, 3-I7, 3-I8) | each re-opened — confirmed | applied |

**Re-opened first-hand by the supervisor (2026-09-10), the full list:** `scene_report.py:17-22, 62, 198, 315-320, 348-357,
399-402, 421-433, 481-484, 553`; `B1_FIRST_FG_ROW.md` §1, §3, §4 (lines 118-136); `B1_SPEED_TEST.md` header, §2, lines 73,
108-116, 124-129; `marker_zoo.py:90-91, 175-186, 194-216, 295-303, 317`; `motion_report.py:60-63, 84-96`; `marker_extract.py:244-264`;
`M1_SRC_RATE.md:19-30, 42-43, 56`; `M1_SRC60.md` rows 9-14, 17-33; `MOTION_TRUTH_CONTROL_NO_MV_GUIDED.md` rows 9-13, 19-21;
`M1_LOWPHASE_FINDING.md` lines 13, 38-43; `MOTION_TRUTH_MASTER_PLAN.md:137-142`; `aap/A0_FROZEN_OBJECTIVE.md:86-91`;
`cli.hpp:222, 359-361, 646-648, 947-954`; `cli.cpp:104, 344, 353, 447-448, 809`; `flow_init.cpp:40,142`;
`OpticalFlowPipeline.cpp:6, 149-156, 554-559`; `OpticalFlowPipeline.hpp:318-319`; `flow.cpp:478`; `scene_live.ps1:9, 28, 34-37`;
`scene_zoo.py:117-134, 193-218, 439-441`; `ref_warp.py:162-172`; `FG_COMPETITIVE_COMPARISON.md:111`; `FG_VFI_PRIOR_ART.md:639`;
`PHYRIADFG_PERFECTION_ROADMAP.md:101`; `FG_PERF_BASELINE.json:21`; the six run directories under `C:\PhyriadFG\runs\`.

**Not re-opened (labelled where used):** `cli.hpp:832-849` (the tier ladder); `ambig.glsl` constants; `phase_clock.cpp`; the
tuning-surface total of ~83 knobs (the individual flags cited above were verified); `marker_zoo.py:107-112` (the size-4 pattern
floor); whether the 8-px tile changes under `--flow-scale` 2 / 4 (default 1: it is 8 screen px).

---

## §7 — Decisions that are the operator's

1. Which families run, in what order, and the screen budget (§2); the workload resolution / rate (1280×720 @ 60 today vs A0's
   1920×1080 @ 60 + 120).
2. The `mv_guided` factor: both levels in every family, or the default only with the caveat stated.
3. The bounded builds — the `reverse` class in `marker_zoo.py` (family 6), the cut-scoring branch with seam-endpoint synthetic
   arms in `scene_report.py` (family 5), a per-size r and `absent` column in `motion_report.py` (family 1) — are tooling; the
   session can build them on his word. The `speed_extend` ×16 preset variant and the `period_backdrop` preset (families 3, 7)
   touch his instrument's scene data — announced here, not built.
4. The stale help texts and docs (P-027) are his repo's source and doc fixes; the reconciliation of `FG_COMPETITIVE_COMPARISON.md:111`
   with I-B's framing belongs to whoever writes I-B; the `M1_SRC_RATE.md:21` sentence (P-026) is corrected in the record by the
   log entry, the June-era file itself left as the dated record it is.

---

## §8 — Runbook (2026-09-10): every command an arm needs, ready for his screen

Every corpus below is rendered under `C:\PhyriadFG\runs\` (CPU only, no screen was used); every tool
named exists, compiles, and passed its identity gate (§9). A capture is ONE command that takes his
screen for ~Seconds and then extracts and scores in parallel (`--jobs`, cores − 2). The arm is named by
`-Tag`; the default capture of a run is never overwritten by an arm, and a run marked KEEP refuses an
overwrite unless `-Overwrite` is passed. The FG flags of an arm go in `-FgFlags` (both runners use that
name — the case-insensitive `$fgArgs` collision is documented in `scene_live.ps1`). DI-3 = the same arm
twice (`-Tag r1`, `-Tag r2` for marker runs; a second seed corpus for scene runs).

| family | corpus (rendered) | command (his screen) | then |
|---|---|---|---|
| 0 attribution (default) | `sc_live` (KEEP; async DI-3 second run) | `tools\scene_truth\scene_live.ps1 -Run C:\PhyriadFG\runs\sc_live2 -K 4 -Gdump -Loop -Tag async` | `scene_step.py` / `scene_review.py` on `arms/fg_k4_async`; `scene_pages.py add-scene` |
| 0 attribution (`--no-stasis`) | `sc_live` | `scene_live.ps1 -Run C:\PhyriadFG\runs\sc_live -K 4 -Gdump -Loop -Tag nostasis -FgFlags "--no-stasis"` | `ref_warp.py --decisions` on the arm: the stasis bit must vanish |
| 0 attribution (band opened) | `sc_live` | `... -Tag mvsim030 -FgFlags "--mv-sim 0.30"` | the 99 % fallback must drop; rim `shape_err` |
| 1 thin_size | `mk_thin_s6`, `mk_thin_s12`, `mk_thin_s24` (one size each, noise, pan 0) | `tools\motion_truth\marker_live.ps1 -Zoo C:\PhyriadFG\runs\mk_thin_s6\zoo -Tag r1` then `-Tag r2`; then `-Tag c1 -FgFlags "--no-mv-candsel"` and `-Tag c2 ...`; same for s24 | `motion_report.py --zoo ... --run detections_r1.csv --run detections_r2.csv --md ...`; `marker_step.py --zoo ... --dump qdump_r1 --out ...`; read `hud` per size |
| 2 grating_pan A | `mk_grat_pan120`, `mk_noise_pan120`, `mk_grat_pan0` (the null) | `marker_live.ps1 -Zoo C:\PhyriadFG\runs\mk_grat_pan120\zoo -Tag r1` / `r2`, same for the other two | stage B only if A separates: `-Tag a1 -FgFlags "--no-ambig"` |
| 3 speed_extend | `sc_train8_s7`, `sc_train8_s11` (×8, 26.9 px/pair); `sc_train16_s7`, `sc_train16_s11` (×16, 53.8 px/pair); the `mixed` ×8/×16 corpora `sc_v8_*`, `sc_v16_s7` kept for the on-screen count | `scene_live.ps1 -Run C:\PhyriadFG\runs\sc_train8_s7 -K 4 -Gdump -Loop` (then s11; then the ×16 pair) | read `lead`, `missing`, `sharp` and T2 at the new speed before `pos` (§4) |
| 4 src_rate_matched | `mk_rate60`, `mk_rate30` (identical params, 4 s) | `marker_live.ps1 -Zoo C:\PhyriadFG\runs\mk_rate30\zoo -Tag r1 -FgFlags "--no-asw"` (+ `r2`); same at 60; then `-Tag g1 -FgFlags "--no-asw --no-mv-guided"` at each rate | the phase-binned section of the report; the absolute bar 0.116–0.119 px |
| 5 cut_score | `sc_live2` (KEEP; 9 cut triples already captured) — or any `-Gdump -Loop` capture | `scene_report.py --run C:\PhyriadFG\runs\sc_live2 --k 4 --arm fg_k4 --cuts --json cuts.json --md cuts.md`; `scene_cuts.py --run ... --k 4 --arm fg_k4 --json cuts.json --out <pages>` | 0 s of screen for the archived cuts; `-Gdump -Loop` on a short corpus for the phase-binned n |
| 6 reversal | `mk_reverse` (classes reverse, linear, hud, fast; 4 s) | `marker_live.ps1 -Zoo C:\PhyriadFG\runs\mk_reverse\zoo -Tag r1` / `r2`; `-Tag s1 -FgFlags "--mv-smooth 0.5"`; `-Tag p1 -FgFlags "--mv-prior"` | the along-velocity projection; the below-cut marker must lag under the EMA |
| 7 period_backdrop | not rendered (conditional on 2) | — | — |

Before ANY capture of a scene corpus the scorer's gate is run on it (`scene_report.py --run <corpus> --k
4 --gate`, CPU, parallel) and must print `GATE PASSED (T1..T6)`; §9 records what it printed on each
corpus tonight.

---

## §9 — Preparation log (2026-09-10, no screen used)

**What was built and verified (commits `7f2c815`, `43f3197`, `57b175b`, `d7fc6e0`):** five Sonnet implementers + five
clean-context validators (`wf_54bcf616-271`, 10 agents, 1.29 M tokens, 84 min; one implementer returned a stub report and its
identity claim was proved by the supervisor instead), every line reviewed and every gate re-run by the supervisor:
`marker_extract.py --jobs` (31 s → 6 s on 8 workers, 21 triples, CSV byte-identical; trailing `wrap` column) ·
`motion_report.py` drops seam rows by default (`--keep-wrap`) · `marker_zoo.py` class `reverse` (default output byte-identical
old vs new; `--verify` passes on `mk_reverse`) · `marker_live.ps1` (dry-run exits first) · `marker_step.py` (PNGs byte-identical at
1 and 4 workers) · `scene_align.py` files cuts apart · `scene_report.py --cuts` + T6 + `scene_cuts.py` (cut table reproduced
byte-for-byte by the validator; serial = pool) · `scene_live.ps1 -Tag/-FgFlags/-Jobs/-DryRun/-Overwrite` + KEEP guard ·
`scene_pages.py` + `runs.json` · `scene_zoo.py` `fast_train` · both steppers' `%%` CSS fix.

**Corpora rendered (CPU, no screen):** `mk_thin_s6/s12/s24`, `mk_grat_pan120`, `mk_noise_pan120`, `mk_grat_pan0`, `mk_rate60`,
`mk_rate30`, `mk_reverse` (marker zoo, 1280×720, 4 s); `sc_v8_s7/s11`, `sc_v16_s7` (`mixed`), `sc_train8_s7/s11`,
`sc_train16_s7/s11` (`fast_train`) at 640×360, 240 fps, 1 s, k = 4 manifests. On-screen count from the id planes: `mixed`
keeps the sphere fully inside 240 / 76 / 38 frames at ×1 / ×8 / ×16; `fast_train` (7 spheres 5.5 units apart) keeps exactly one
fully inside 234 / 232 of 240 frames at ×8 / ×16 (a first try at 8.33 spacing left 85 frames empty).

**The scorer's gate, corpus by corpus (pooled, `--jobs 30`, ~4 min each).** The gate had only ever been run on the ×1 corpus
(P-029). Tonight, under the recorded `tau` operator: ×1 `GATE PASSED (T1..T6)`; ×2 FAILED T2, T3 (nearest residual p90
0.387 px, blend pos 0.464); ×4 FAILED T2, T3 (0.771 / 1.056); every ×8 / ×16 corpus FAILED T2, T3, T4 (residual 2.2 px, the
exact-flow oracle 0.9–1.3 px of shape). Three causes were found and fixed in turn, each with its identity test at ×1:

| fix (scene_report.py) | mechanism found | ×1 identity | effect |
|---|---|---|---|
| border-clip rule (a MOVING object whose silhouette touches the frame border carries no terms; `row['clipped']`) | the ×8 sphere is partially outside in 24 frames; a clipped centroid is not the object's | rows identical (the static occluder quad spans the full height by design and is exempt by `disp > 0.5`) | necessary, not sufficient |
| `--silhouette coverage` (half-coverage contour; the default `tau` byte-identical) | `tau` thresholds \|rgb − bg\| > 0.06, so an anti-aliased edge pixel flips with the backdrop noise under it; cancels at ×1 (same backdrop both sides), not at 13 px apart | rows identical under the default | T4 passes at ×8 (oracle 0.01 px); T2 at ×1 tightens 0.1 → 0.015 px; T2 at ×8 unchanged |
| other objects excluded from the window in the mid AND both reals (`ids_ab`) | `fast_train`'s spheres pass in front of the spinning box; `mixed` at ×2 / ×4 reaches it; the near real shows the box's pixels where the mid had the sphere (seen in `t2_view.png`) | only the occluder quad changes (−7 px of area; the oracle's spurious 0.0255 px on it → 0) | **T2 passes at every speed: p90 residual 0.014–0.023 px at ×2 / ×4 / ×8** |
| `overlap_px` per object; T3 / T5 read the overlap-free pairs | the blend's two ghosts and the blurred disc are clipped asymmetrically where objects overlap | — (gate only) | T5 passes everywhere; T3 passes at ×1, ×2 |

Final state, `--silhouette coverage`: ×1 and ×2 **PASS T1..T6**; ×4 and ×8 pass all but **T3**, whose residual on overlap-free
pairs grows with displacement — 0.06 (×1), 0.11 (×2), 0.26 (×4), 0.57 px (×8) against a 0.15 px bar. Mechanism (inference,
consistent with every number): the blend's two ghosts expose opposite limbs of an obliquely lit sphere, and the union loses
more of the dark limb as they separate; a candidate that sits near the truth shares its shading and is not biased that way,
the blend ruler is. **The bar was NOT relaxed.** ×0.5 "fails" T2 / T3 by the gate's own floors (mean travel 0.132 < 0.2; the
blend legitimately ACCEPTED below ~1 px/pair, `B1_FIRST_FG_ROW.md` §3), with a T2 residual of 0.015 px.

**The speed rows re-scored under the gate-passing operator** (`fg_k4_cov.json` / `.md` beside each run; arm `fg_k4`, the
intact aligned frames; one seed, one run each; `scene_speed.py` fit):

| corpus | px/pair | operator | sphere pos | shape | halluc px² | lead | nearest | oracle2 |
|---|---|---|---|---|---|---|---|---|
| `sc_v05` | 1.68 | tau → coverage | 0.355 → 0.311 | 0.230 → 0.231 | 67 → 56 | +47.8 → +46.1 | 0.416 → 0.284 | 0.153 → 0.009 |
| `sc_live` | 3.36 | tau → coverage | 0.481 → 0.443 | 0.292 → 0.400 | 76 → 105 | +48.4 → +36.1 | — | — |
| `sc_v2` | 6.72 | tau → coverage | 0.544 → **0.684** | 0.273 → 0.614 | 72 → 225 | +29.7 → +6.6 | 1.299 → 1.292 | 0.127 → 0.010 |
| `sc_v4` | 13.43 | tau → coverage | 0.690 → **1.522** | 0.396 → 1.200 | 98 → 607 | +24.0 → +5.7 | 2.514 → 2.809 | 0.131 → 0.009 |

The `tau` operator under-reported the FG's error at high speed (its threshold read the FG's smeared edge as "not object");
the rulers say which side to trust: the exact-flow oracle scores 0.01 px under coverage (0.13 under tau) and `nearest` is
unchanged. **Power-law re-fit over the four coverage rows: pos ≈ 0.191·disp^0.75** (nearly proportional to displacement) in
place of `B1_SPEED_TEST.md`'s 0.30·disp^0.32 (gentle growth). One seed, one run per point; the ×4 corpus fails T3. The frozen
identity's clause (a) quotes the old law: it is not edited (KAP §7); the finding stands beside it as a post-freeze consequence
for the operator's decision.

**Cut scoring** (`sc_live2`, 9 cut triples captured 2026-09-08, arm `fg_k4_cuts`, `cuts_k4.md/.json`; review page
`F:\Phyriad\scene_pages\sc_live2_cuts\index.html`): the FG blends across the seam — halluc vs the nearer real 86–88 px² but
vs the farther real 183–402, missing 55–4244, sharp 0.29–0.83, graceful 0.016–0.062; the `hold` reference scores graceful
0.0000 and halluc 0 on every cut, the `blend` reference 88–770 px². Gate T6 passes on every corpus.

**Lost tonight (P-028):** `sc_live/qdump_k4`, the raw k = 4 sampler capture behind B1 (aligned frames and scores survive; the
provenance replay of that run does not); seven "RA Motion Zoo" windows left on the operator's screen by the same defect,
closed at 02:50.

**Not run:** any live capture (every family needs the operator's screen); the second-seed rows of the re-scored speed law;
`period_backdrop`; a T3 bar that scales with displacement (the operator's call); the marker chain on a real capture (only the
synthetic dump and the M1 records); `scene_pages` rows for coverage-scored runs.

*Made with my soul - Swately <3*
