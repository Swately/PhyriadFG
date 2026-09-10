# 4. Method II — the object under test and the protocol

<!-- KAP Phase 2 stub (SCAFFOLD.md, section 5 of 11). Every slot below is EMPTY; Phase 3 fills the ones marked EXISTS -->

## M2.1 — The object: `fg_core.comp` under its default flags; the two present paths (`--qdump` sampler, `--gdump` every-tick tap); the build each row names

*serves:* OB · *evidence:* the identity block; `B1_FIRST_FG_ROW.md` and `B1_SPEED_TEST.md` headers; `GDUMP_GATE.md` G0 · *status:* `EXISTS`

The object under test is `fg_core.comp`, PhyriadFG's shipping default kernel, run under its default flags (`FG_METRIC_MODEL_SPINE.md` identity block). Two present paths are in scope: the synchronous path sampled through the `--qdump` sampler, and the asynchronous shipping path tapped every tick through `--gdump` (identity block); "a claim about a path rests only on rows measured on that path at a named build; a later build re-runs the rows it cites" (identity block).

The `--qdump` rows are read against build `f2e4a9a`: "Binary: `build-release/phyriad_fg.exe` at `f2e4a9a`, default kernel, `--fg-factor 4`, `--qdump` (sync present path, sampled ticks)" (`B1_FIRST_FG_ROW.md` header); "Binary: `build-release/phyriad_fg.exe` at `f2e4a9a`, default kernel" (`B1_SPEED_TEST.md` header). The `--gdump` rows are read against the tree committed as `0df0332`, md5 `186CB46C` at the gate, measured against a base of `364efc5`, md5 `F36FDE32` (identity block; `GDUMP_GATE.md` header: "base = HEAD `364efc5` built from a detached worktree, md5 `F36FDE32` ...; new = the working tree ... md5 `186CB46C`"). That `0df0332` tree "build ×2 (`build-release.bat`) | clean, `LTO/IPO enabled`; `100% tests passed out of 51` (50 before + `gdump_book`)" (`GDUMP_GATE.md` G0).

## M2.2 — Sources: 640×360 from a 240 fps analytic base decimated by k; the canonical row k = 4 on `mixed`; where k ∈ {2, 8, 16} and 1080p enter

*serves:* OB · *evidence:* the identity block; `B1_FIRST_FG_ROW.md` §3 · *status:* `EXISTS`

Sources are 640×360 frames decimated by k from a 240 fps analytic base (identity block). The canonical row is k = 4, a 60 fps source, on the `mixed` scene family (identity block); k ∈ {2, 8, 16} — 120, 30 and 15 fps sources — and 1080p enter only where a measured row exists (identity block). The k sweep measured on `mixed` at 640×360 gives displacement per source pair of ~0.85 px at k = 2, ~1.7 px at k = 4, ~3.4 px at k = 8 and ~6.8 px at k = 16 (`B1_FIRST_FG_ROW.md` §3).

## M2.3 — The A/B rule: every knob change against the default, never a redefinition; the `mv_guided` level stated in every record

*serves:* OB · *evidence:* the identity block; `REGIME_TEST_MATRIX.md` §4 · *status:* `EXISTS`

Every knob change is scored as an A/B against the default, never a redefinition of the object under test (identity block). The `mv_guided` level is stated in every record because it is named a cross-cutting confound: "state the level in every record; run both levels where the predicted effect is smaller than the ~6× low-phase term — every family here" (`REGIME_TEST_MATRIX.md` §4).

## M2.4 — The reliability protocol: two corpus seeds, run-to-run r beside every number, the 20 % rule, the one-run label

*serves:* RE · *evidence:* `B1_FIRST_FG_ROW.md` §2b; `scene_report.py` (the printed rule) · *status:* `EXISTS`

Reliability is read from two corpus seeds run under the same protocol, with the run-to-run deviation reported beside every cited number. The k = 4 canonical row was run on seed 7 and seed 11 — same binary, same protocol — and every citable term carries its deviation: pos_err 0.216 vs 0.212 px (2.2 %), shape_err 0.145 vs 0.137 px (5.9 %), halluc 52.3 vs 51.2 px² (2.1 %), lead +8.55 vs +8.82 px (3.1 %), missing 25.3 vs 23.2 px² (8.9 %), sharp 0.955 vs 0.948 (0.7 %), sphere pos 0.481 vs 0.468 px (2.6 %), box pos 0.159 vs 0.160 px (0.1 %) (`B1_FIRST_FG_ROW.md` §2b). "Worst discrepancy on the citable terms 13.1 %, on the smallest bucket (20 frames a side); every term is under the 20 % threshold and may be cited" (`B1_FIRST_FG_ROW.md` §2b). The scorer prints the same bound whenever two corpora are given: "DI-3: two corpora given; compare the per-arm rows above run against run. A term whose two runs disagree by more than 20 % carries no verdict." (`scene_report.py:819-820`). A term measured on a single corpus carries the alternate label instead of a comparison: "one run, reliability not measured" (`GDUMP_GATE.md` G5).

## M2.5 — Family entry: the pre-registered failure signature of each family, quoted from the matrix as written BEFORE its first row; the red-first check (the family's gate seen red on a synthetic arm) quoted per corpus

*serves:* SS · *evidence:* `REGIME_TEST_MATRIX.md` §2 (signatures); §9 (gate logs) — the per-family red-first line is quoted at fill time · *status:* `EXISTS`

The matrix pre-registers eight families; each family's predicted failure signature and its red-first check are written before its first row:

| family | pre-registered signature | red-first check |
|---|---|---|
| 0 attribution | "neither is predicted to move `pos_err` by more than the 13 % seed spread" | "the provenance planes must show the stasis bit on the default BEFORE its absence is read on the arm" |
| 1 thin_size | "the effect must exceed 0.573 px's reproduction and the seed spread" | "read the `prev` / `next` planes at size 6 to settle whether 0.573 px is extractor bias or an FG effect on static content" |
| 2 grating_pan | "`absent` / `ghost` rise (a 24-px-wrong warp moves a 6–12 px marker out of the ~20 px window), err p95 gains a mode near the lattice period" | "`--objdump N` the MV field on a grating capture and confirm a 24-px error would show" |
| 3 speed_extend | "the failure changes KIND ...; the readable signature is therefore `lead` swinging toward TRAILING ..., `missing` jumping, and sharpness possibly RISING" | "gate T2 at the NEW speed — `nearest` vs the truth centroid's own travel, p90 \|resid\| < 0.25 px — a T2 failure at ×16 means pos_err is censored, not that the FG is fine" |
| 4 src_rate_matched | "If B1 is right: the 30 fps side is worse and the excess sits at HIGH phase (> 0.75) ...; if M1 is right: the sides overlap inside r" | "the capture-path floor line and `fps=30.0 target=30 missed=0` in the log" |
| 5 cut_score | "Default: large, roughly symmetric halluc, sharp ≈ 0.5, graceful far above the within-scene value. HOLD policy (ideal): halluc vs the nearer real → ~0, graceful → 0" | "synthetic `nearest` and `blend` built AT the seam endpoints ... — nearest must give graceful ≈ 0 and blend must be rejected" |
| 6 reversal | "a reversal costs no more than its instantaneous displacement ...; a departure indicts the other temporal carriers" | "a marker whose reversal magnitude sits BELOW `kMvCut = 6.0` px ...; it MUST lag, or the EMA is not running" |
| 7 period_backdrop | "default holds bg_err at the aperiodic floor; `--no-ambig` raises it and the verdict gains `bg`. Objects: pos/shape show a DISCRETE step near a period multiple" | "T1 must pass on the new corpus ...; positive `blend` rejected (T3), `blur` flagged (T5)" |

All quotations in this table (`REGIME_TEST_MATRIX.md` §2). The matrix's families map onto the Results slots as
`SCAFFOLD.md` states (3 → §05.7; 6 → §05.8; 1, 2, 7 → §05.9; 5 → §05.10; 4 → §05.11; 0 → §05.12 and §05.5). The four
built presets of §05.6 — `translate`, `occlude`, `spin`, `cross` — have no pre-registered signature: the matrix
names none for them, and under the identity's entry rule theirs are written here, in the six terms, before their
first capture. Owed; not written today.

## M2.6 — The capture procedure: one command per arm, dry-run discipline, KEEP corpora, the scoring commands (`--jobs`, `--silhouette`)

*serves:* ET · *evidence:* `REGIME_TEST_MATRIX.md` §8 · *status:* `EXISTS`

Each arm is captured by one command: the runbook lists exactly one PowerShell command per family per arm, e.g. family 0's default arm, `tools\scene_truth\scene_live.ps1 -Run C:\PhyriadFG\runs\sc_live2 -K 4 -Gdump -Loop -Tag async` (`REGIME_TEST_MATRIX.md` §8). "A capture is ONE command that takes his screen for ~Seconds and then extracts and scores in parallel (`--jobs`, cores − 2)" (`REGIME_TEST_MATRIX.md` §8). The arm is named by `-Tag` and its FG flags go in `-FgFlags`; "the default capture of a run is never overwritten by an arm, and a run marked KEEP refuses an overwrite unless `-Overwrite` is passed" (`REGIME_TEST_MATRIX.md` §8). DI-3 is the same arm captured twice: "`-Tag r1`, `-Tag r2` for marker runs; a second seed corpus for scene runs" (`REGIME_TEST_MATRIX.md` §8). Before any capture of a scene corpus its gate is run and must pass: "the scorer's gate is run on it (`scene_report.py --run <corpus> --k 4 --gate`, CPU, parallel) and must print `GATE PASSED (T1..T6)`" (`REGIME_TEST_MATRIX.md` §8).

*Made with my soul - Swately <3*
