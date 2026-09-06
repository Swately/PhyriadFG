> **PORTED INTO PhyriadFG on 2026-09-03** (`tools/fg_quality_scorer/`, phase S2.T0 of
> [`../../docs/planning/MOTION_TRUTH_MASTER_PLAN.md`](../../docs/planning/MOTION_TRUTH_MASTER_PLAN.md)).
> The ONLY edit is the CMake `_render_vulkan` path: it now points at the repo's VENDORED
> `framework/render/vulkan`, not the container catalog's copy. The two lineages diverged (427/146 diff
> lines, measured 2026-09-02), so numbers produced against the catalog do NOT describe this product's
> flow. Any baseline in this file that predates the port was measured against the CATALOG flow and is
> historical until re-locked here.
>
> **`--qdump` AND THE PRESENT PATH:** the triple dump is written only on the SYNCHRONOUS present path
> (`src/present/present.cpp`, the block is gated on `!ap`). The shipping default IS `--async-present`,
> but `resolve_config` (`src/control/cli.cpp:227–231`) AUTO-DISABLES it for any run that asks for `--qdump`
> or `--outdump`, and prints the reason — so `--qdump DIR N` alone is enough; `--no-async-present` is
> equivalent, not required. VERIFIED 2026-09-03: the flag alone wrote 4 triples. (An earlier note in the
> planning docs called `--qdump` "inert under the shipping default"; that was a misreading of the
> present-side gate without the config-side auto-disable, and is corrected there.) Making the ASYNC path
> itself dumpable is a separate Tier-2 item, deliberately out of scope (MOTION_TRUTH master plan §6).
>
> The consequence that DOES matter for measurement: a `--qdump` run is therefore **not** the shipping
> present path. Numbers from it describe the synchronous path, and that must be said wherever they are
> quoted.

# fg_quality_scorer — the FG-quality "number producer"

> Standalone rig tool for `docs/planning/ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md`
> §11.4 **layer 2** (the offline scorer). Not wired into the render pillar's
> CMake / CI. Headless compute (no window). Reuses, read-only, the committed
> `OpticalFlowPipeline.cpp` + its shaders + `spv_to_header.cmake`. It does **not**
> touch the live app or `stage11`/`stage31` files — it copies their proven
> pattern. Diátaxis type: **reference** (the contract + the metric definitions).

Honesty status: `designed` — authored and reasoned from first-hand code reading;
the supervisor integrates, builds, and verifies the numbers first-hand. No metric
value in this README is a measured claim.

## What it does

Turns the eyeballed FG artifacts (the operator's manual 240fps-frame-decomposition
method) into objective metrics on **held-out real triples** (§11.3). For each
triple `(anchor_prev = real N, truth_mid = real N+1 @ t=0.5, anchor_next = real
N+2)` it scores two candidate midpoints against the held-out ground truth
`truth_mid`:

- **Mode A (flow+warp, offline-deterministic):** runs the **committed**
  `OpticalFlowPipeline` flow+warp at `t=0.5` on `(anchor_prev, anchor_next)` →
  candidate, scored vs `truth_mid`. Measures **our flow+warp** quality.
  Deterministic and **GPU-independent within a vendor** (integer-exact SAD +
  deterministic shaders — verified byte-identical across NVIDIA GPUs 2026-06-22;
  minor cross-vendor fp drift, see "Regression gate" below — and the `stage11`
  README).
- **Mode B (full-pipeline, only if `live=` is present):** scores the live app's
  dumped `wapOutA` `.rgba` for that midpoint vs `truth_mid` **directly, no
  re-run**. Measures the **full shipped pipeline** including the holonic layers
  (object_repair / scene-memory / WAP).
- **Mode T (TRUTH-LESS — the in-app `--qdump` form):** a `triple` with
  `prev`/`next`/`live` but **no `mid=`**. The live FG output has **no held-out
  ground truth** (you cannot withhold a real without creating the very artifact
  you measure — §11.3), so only the **truth-independent** metrics run:
  `crossfade_residual` (+ `alpha`) and `double_edge_energy` of `live` vs
  `(prev, next)`. `PSNR`/`SSIM`/`obj_IoU`/`nonrigid` are reported **N/A**. This
  is **how OUR live FG output is scored for crossfade without perturbing the
  pipeline** — the in-app `--qdump` (§11.4 layer 1) emits exactly these
  truth-less triples (the three live device images: `wapPrevA` = real N,
  `wapOutA` = the live FG frame, `wapCurA` = real N+2). The row carries a
  `(truth-less: crossfade-only)` tag.

## Manifest format (the `--qdump` contract)

A simple text file. Paths are resolved **relative to the manifest's own
directory**. Lines:

```
# comment
size <W> <H>                                          # RGBA8 work-resolution; ONE per file
triple <name> prev=<p> mid=<m> next=<n> [live=<l>]    # explicit-path held-out triple
triple <name> prev=<p> next=<n> live=<l>              # TRUTH-LESS triple (NO mid=) — the --qdump form
sequence <prefix> <count>                             # CONTINUOUS held-out sequence (decimate → triples)
preset <name> [trailing tokens ignored]               # back-compat: <name>_N/_M/_P.rgba
```

- `sequence <prefix> <count>` — the **continuous time-extended** form (the
  operator's "grabar n tiempo y extenderla en frames"). Expands a recorded /
  synthetic sequence `<prefix>000000.rgba .. <prefix>{count-1}.rgba` (6-digit
  zero-pad) into **held-out triples** by the canonical VFI protocol: drop every
  other frame and reconstruct it. Triple `j` = `prev=<prefix>{2j}`,
  `mid=<prefix>{2j+1}` (HELD OUT — the ground truth), `next=<prefix>{2j+2}`. A
  sibling `<prefix>{2j+1}_live.rgba`, if present, is scored as Mode B (the
  full-pipeline replay output for that midpoint). `count` frames → `⌊(count−1)/2⌋`
  triples, a continuous per-frame quality curve over the whole segment. Generate a
  deterministic source with **`prep_zoo_sequence.py`** (below); a real recording
  from `apps/render_assistant/tools/capture_dump` works too (`--out <dir>` writes
  `cap_000000.rgba …`, so `sequence cap_ <N>`).

- `size W H` — every `.rgba` is raw RGBA8, row-major, `W*H*4` bytes, **no
  header** (same as `stage11`/`stage31`). The tool skips any triple whose file
  size ≠ `W*H*4`.
- `triple` (full, held-out) — `prev`/`mid`/`next` required (the held-out N /
  N+1 / N+2); `live` optional (enables Mode B). Used for offline held-out
  triples (e.g. decimated 240 fps ground truth).
- `triple` (**truth-less**, the in-app `--qdump` form) — `prev`/`next`/`live`
  present, **`mid=` ABSENT**. This is what `render_assistant.exe --qdump <dir>
  N` emits live: it reads back the three live device images per sampled present
  tick — `wapPrevA` (real N) → `q%06d_prev.rgba`, `wapOutA` (the live FG
  output) → `q%06d_live.rgba`, `wapCurA` (real N+2) → `q%06d_next.rgba` — and
  appends one `triple … (no mid=)` line, with a single `size W H` header on the
  first dump. There is no held-out real to score against (withholding one would
  manufacture the artifact, §11.3), so the scorer runs **crossfade + double-edge
  only** (mode **T**), reporting the truth-based metrics as N/A. This is how the
  pipeline's crossfade is measured **without perturbing the present path**.
- `preset` — back-compat with a `stage11` manifest: `preset foo …` maps to
  `foo_N.rgba` / `foo_M.rgba` / `foo_P.rgba` (the `stage11` triple naming).
  Trailing tokens (`D=`, `tx=`, …) are ignored. Use the explicit `triple` form
  for `stage31` frames (they are named `_A`/`_B`/`_M`, not `_N`/`_M`/`_P`).

Example manifest the in-app `--qdump` produces (TRUTH-LESS — note: no `mid=`):

```
# qdump (--qdump) HSR FG-quality test-field — TRUTH-LESS held-out triples (live FG, no mid=)
size 1920 1080
triple q000000 prev=q000000_prev.rgba next=q000000_next.rgba live=q000000_live.rgba
triple q000001 prev=q000001_prev.rgba next=q000001_next.rgba live=q000001_live.rgba
```

Example **full** held-out manifest (offline decimation — carries the ground truth `mid=`):

```
# offline: decimated 240 fps ground truth → held-out N+1
size 1920 1080
triple dump0007 prev=dump0007_N.rgba mid=dump0007_M.rgba next=dump0007_P.rgba live=dump0007_live.rgba
```

## Metrics (all vs `truth_mid`, 16 px border crop)

| column | meaning | direction |
|---|---|---|
| `PSNR dB` | RGB mean-squared-error PSNR | higher = better |
| `SSIM` | windowed luma SSIM (win 8, step 4, 8-bit constants) | higher = better |
| `xfade_res` | RMSE of the best-fit pure crossfade model (see below) | see below |
| `alpha` | the fitted blend weight α | diagnostic |
| `dbl_edge` | per-pixel spurious-edge (seam) energy, whole-frame normalised | lower = better |
| `dbl_edg_m` | **same seam energy, restricted to + normalised by the MOVING region** (PRIMARY artifact metric) | lower = better |
| `mcov` | motion coverage = moving px / interior px (context for `dbl_edg_m`) | diagnostic |
| `obj_IoU` | dominant-object silhouette overlap, cand vs truth | higher = better |
| `nonrigid` | centroid-aligned object shape mismatch / area | lower = better |
| `flowdsc` | **flow-discrepancy px** — how far our motion estimate is from the true midpoint motion (Mode A only) | lower = better |

**PSNR + SSIM** are reused **verbatim** from
`framework/render/vulkan/bench/stage11_blockmatch_quality/main.cpp`: `psnr_rgb`
(lines 96–110 of that file) and `ssim_luma` (lines 113–141). Identical bodies,
same 16 px interior crop.

### CROSSFADE metric (the §11 crossfade / seam artifact)

- **`crossfade_residual`** — the candidate is fit to the best pure crossfade
  model `cand ≈ α·prev + (1−α)·next`, minimising MSE over `α∈[0,1]` (closed-form
  least-squares α, clamped). The reported value is the per-channel **RMSE
  (0..255)** of that best fit.
  - **LOW residual ⇒ the candidate IS ~a linear blend of the two anchors =
    the crossfade artifact** (the double-disc / ghosted look). A clean
    motion-compensated warp moves content to new positions a blend cannot
    reproduce → **HIGH** residual. So here **LOW = bad, HIGH = good**.
  - **Honest limit:** this is a **model-fit proxy**, not a direct artifact
    detector. A frame with little motion also fits the blend with a low residual
    — which is correct (static ⇒ blend ≈ truth ⇒ nothing to flag). Read it
    **with PSNR**: low residual **and** low PSNR = a damaging crossfade; low
    residual **and** high PSNR = harmless (little motion).
- **`double_edge_energy`** — per-pixel mean luma-gradient magnitude in the
  candidate at pixels where **neither anchor nor the truth** has a strong edge
  (`> edge_thr`, default 12 luma/px) — the spurious crescent seam crossfading
  introduces.
  - **Honest limit:** it is gradient-, not structure-aware; a sub-pixel-shifted
    real edge can register as "spurious". The per-pixel AND-mask against all
    three real frames (prev, next, truth) suppresses most of that, but it is a
    proxy.
- **`dbl_edg_m` (motion-masked — the PRIMARY artifact metric)** — the **same**
  spurious-edge energy, but accumulated **only** inside the moving region and
  **normalised by the moving-pixel count**, not the whole frame. The moving
  region is a coarse mask: pixels where `|luma(prev) − luma(next)| > motion_thr`
  (default 12) — where content moved between N and N+2, i.e. where ghosting
  lives. `mcov` (motion coverage = moving px / interior px) is reported alongside.
  - **Why it exists:** the whole-frame `dbl_edge` **dilutes** a *localized* ghost
    against the static background. Measured on the `our_zoo` dump, q1's ghost sits
    in ~7.6 % of the frame, so global `dbl_edge` reports it ~13× weaker than the
    masked value (`0.94` vs `12.48`). The peer-reviewed VFI-quality literature
    reaches the same conclusion — global PSNR/SSIM correlate poorly with perceived
    interpolation artifacts because the artifact is localized to the moving region
    (FloLPIPS, arXiv 2207.08119, weights LPIPS by the per-pixel optical-flow
    discrepancy; BVI-VFI / VFIPS, ECCV'22). Reading: **high `dbl_edg_m` + LOW
    `mcov` = a localized ghost** (q1); near-zero on clean frames even when motion
    is present (clean q10 = 0.13).
  - **Honest scope:** this is motion-**presence** masking (where content *moved*),
    the truth-less analogue of FloLPIPS flow-**discrepancy** weighting (where *our*
    motion is *wrong*). The discrepancy form needs the held-out truth + the flow
    field (the full-`triple` / FR-mode upgrade). The research also flagged that
    warp-error motion-decoupling is **not** provably independent of motion
    magnitude, so `dbl_edg_m` stays a **localizer/flag**, read with `mcov` + PSNR,
    never a standalone verdict.

### GRAVITY / OBJECT-DEFORMATION metric (the §11 LSFG object-warp artifact)

Segments the dominant moving object by a **high-saturation OR high-luma-deviation**
mask, keeps its largest 4-connected component, and compares the candidate's
silhouette to the truth's:

- **`obj_IoU`** = `|maskC ∩ maskT| / |maskC ∪ maskT|` — silhouette overlap
  (1 = identical). Higher = better.
- **`nonrigid`** = after translating `maskC` so its centroid coincides with
  `maskT`'s, the **symmetric-difference area / mean object area**. A *rigid*
  object that merely moved aligns to ≈0; an object that changed **shape** (the
  LSFG "gravity" / squish) leaves a residual > 0. Lower = better.

**Honest assumptions / limits (this is a HEURISTIC proxy, threshold-based):**

- "dominant object" = the largest connected component of a colour/luma mask. It
  assumes **one** salient object distinguishable from the background by
  saturation or luma. On busy / multi-object scenes it segments whatever
  component is largest, which may not be the artifact-bearing one. It is a
  synthetic-/simple-scene proxy, **not** a general segmenter.
- the thresholds (`sat_thr=0.18`, `luma_dev_thr=40`) are **fixed** (compile-time
  constants in `main.cpp`). A low-contrast object can be missed entirely → both
  `obj_IoU` and `nonrigid` are reported as **N/A** when either mask is empty.
- centroid-only alignment models **pure translation**; it does not correct
  rotation/scale, so a rotating *rigid* object scores some `nonrigid`. Treat
  `nonrigid` as a relative ranking signal, not an absolute deformation measure.

### FLOW-DISCREPANCY metric (the FloLPIPS-analogue — `flowdsc`, Mode A only)

> External references (FloLPIPS, BVI-VFI, VFIPS, VFIPQA) carry their FDP §2.3
> verification levels at the single source —
> [`HSR120_THROUGHPUT_DIAGNOSIS.md` §11.7](../../../../../docs/planning/ongoing/HSR120_THROUGHPUT_DIAGNOSIS.md).
> FloLPIPS [V1], BVI-VFI [V1] (supervisor-fetched 2026-06-13); the `SROCC≈0.13`
> figure is [V2] (harness-transcribed, not author-verified).

The VFI-QA SOTA (FloLPIPS, arXiv 2207.08119) weights the artifact map by the
**optical-flow discrepancy** between reference and output — concentrating the
metric where the interpolator's motion is *wrong* (where ghosting lives), not
merely where motion *exists*. We adopt that mechanism honestly with what the
pipeline already produces — its **MV field** (`motion_image()`, RG16F at
W/8×H/8):

- For a held-out triple, Mode A runs the flow **twice**: `flow(prev→next)` (the
  candidate's motion — the field the warp used) and `flow(prev→truth)` (the
  **true** motion to the held-out midpoint). Both MV fields are read back and
  decoded (RG16F → float, pixel units).
- **`flowdsc`** = mean over **SAD-confident tiles** of
  `|0.5·mv(prev→next) − mv(prev→truth)|` in pixels. `0.5·mv(prev→next)` is the
  midpoint motion a translational interpolator *predicts*; `mv(prev→truth)` is the
  *true* midpoint motion. Their difference is how far our motion estimate is from
  reality.
- **The periodic-texture confound (verified, and what actually fixed it):** a
  *periodic* background makes the block-match genuinely **ambiguous** (aperture
  problem — equally-good matches one period apart). Critically, the wrong match has
  a **LOW** SAD (it really does match a shifted period), so it *looks* confident.
  This inflated raw `flowdsc` — the verified `pan_diag` anomaly: ~10 px flow-error
  on a clean diagonal pan whose PSNR was fine. **A SAD-confidence gate alone did NOT
  fix it** (the bad matches are low-SAD, so the gate keeps them). The fix was the
  **test source**: `prep_zoo_sequence.py` now renders a deterministic *non-repeating*
  value-noise scene → every tile has a unique match → well-posed flow → `pan_diag`
  drops to 0.4 px and `flowdsc` rises monotonically with motion difficulty.
- **SAD-confidence gate (kept, complementary):** a tile contributes only if BOTH
  matches (prev→next, prev→truth) have `sad_best < 32` (= the warp's own
  `residual_ceil` — tiles it itself trusts). This correctly drops *genuinely failed*
  high-SAD matches (textureless/occluded regions); it does **not** resolve low-SAD
  periodic ambiguity (that is the source's job, above). The reported `conf_tiles %`
  flags when few trustworthy tiles remain — read `flowdsc` with caution there, and
  expect periodic *real* content (fences, grates) to still confound it.
- **Why it is additive:** it separates **flow-estimate error** from **warp/blend
  error**. PSNR/SSIM tell you *how wrong* the candidate is; `flowdsc` tells you
  whether the wrongness is a **motion** failure (the flow could not represent the
  motion — zoom, rotation, occlusion) vs a sampling/blend artifact. ~0 px = the
  motion was tracked (clean translation); large = non-translational motion the
  block-match cannot model.
- **Honest scope / limits:** (1) Mode A only — it needs the held-out truth *and*
  re-runs the committed flow; Mode B (live full-pipeline output) and Mode T
  (truth-less) report `N/A`. (2) It measures the discrepancy of OUR flow estimate
  at tile (W/8) resolution; it is **not** the learned-perceptual FloLPIPS (that
  still needs the deferred ONNX LPIPS net — see below). (3) The research flagged
  that warp-error motion-decoupling is not *provably* independent of motion
  magnitude, so read `flowdsc` as a **flow-quality localizer**, together with
  PSNR/`dbl_edg_m` and `conf_tiles %`, not a standalone verdict.

### LPIPS — deliberately NOT implemented

LPIPS needs a learned model (an ONNX network + a runtime). It is a **deliberate
future add**, out of scope for v1. **The v1 suite is PSNR + SSIM + the two
artifact metrics (crossfade + gravity).** Adding LPIPS later means bundling an
ONNX runtime + the LPIPS weights and a preprocessing path; none of that is
present here, and the scorer does not pretend to compute a perceptual metric.

## Output

- **stdout:** one line per triple per mode (`A` / `B` / `T`) + an aggregate
  (mean ± σ over the triples that produced a value, per mode) + an environment
  block (GPU, driver, Vulkan API, the flow config). A truth-less (`T`) row
  carries `N/A` in the PSNR/SSIM/IoU/nonrigid columns and a
  `(truth-less: crossfade-only)` tag; the mode-T aggregate reports only
  `xfade_res` + `dbl_edge`.
- **CSV (optional, 3rd arg):** one row per triple per mode; columns =
  `triple,mode,psnr_db,ssim,crossfade_residual,crossfade_alpha,double_edge_energy,double_edge_masked,motion_coverage,obj_iou,nonrigid_deform,flow_discrepancy`.
  `obj_iou` / `nonrigid_deform` / `flow_discrepancy` are `-1` (or empty for a
  mode-`T` row) when N/A.
- **worst-frame surfacing (stdout):** after the aggregate, the N frames with the
  highest `dbl_edg_m` (the localized-artifact metric) are listed worst-first —
  the automated replacement for hunting the bad frame in a slow-mo capture.
  A **mode-`T`** row leaves the truth-based columns (`psnr_db`, `ssim`,
  `obj_iou`, `nonrigid_deform`) **empty** — only `crossfade_residual`,
  `crossfade_alpha`, `double_edge_energy`, `double_edge_masked`,
  `motion_coverage` are populated.

## Run / self-test on the existing synthetic frames

The in-app `--qdump` does not exist yet, so validate the metrics **now** on the
known-ground-truth `stage11` / `stage31` synthetic frames.

**stage11 (pure translation + a zoom/rot combo — back-compat `preset` lines):**

```sh
# 1. generate stage11 frames (needs Pillow + a real source PNG; see stage11 README)
python framework/render/vulkan/bench/stage11_blockmatch_quality/prep_frames.py

# 2. build the scorer
cmake -S framework/render/vulkan/bench/fg_quality_scorer -B build-fgq -G Ninja
cmake --build build-fgq

# 3. score the stage11 frames directly off their own manifest
#    (stage11 writes preset lines → the scorer maps them to _N/_M/_P triples)
./build-fgq/fg_quality_scorer \
    framework/render/vulkan/bench/stage11_blockmatch_quality/frames/manifest.txt \
    -1 fgq_stage11.csv
```

Validation reading (Mode A only — stage11 has no `live=`):

- **`trans08` (D=8):** the committed flow at the shipping `search_radius=2`
  reaches `D=8` via the pyramid → expect **high PSNR** and a **high
  `xfade_res`** (genuine motion comp, not a blend), low `dbl_edge`.
- **`combo08` (zoom+rotation):** a translational per-tile model cannot fully
  reconstruct zoom/rotation → expect **lower PSNR** than `trans08` and a
  **lower `xfade_res`** (closer to a blend) — the crossfade-prone case the
  metric is built to flag.
- **Caveat (read `main.cpp` / stage11 README):** Mode A drives the **committed**
  `optical_flow_warp.comp`, which stage11 documents as carrying a **sign error**
  (it samples `A[x+mv/2], B[x−mv/2]` instead of the temporal-midpoint
  `A[x−mv/2], B[x+mv/2]`), doubling the displacement. On pure translation this
  can push Mode-A PSNR **below** a naive 50/50 blend. The scorer reports the
  committed shader's **true** number on purpose; it does **not** substitute the
  local sign-corrected warp. If the supervisor wants the corrected-warp curve,
  that is `stage11`'s `fix` column, not this tool's job.

**stage31 (disocclusion + edge/HUD cases — explicit `triple` lines):**

stage31 frames are named `_A`/`_B`/`_M`, so they need explicit `triple` lines
(map `A→prev`, `M→mid`, `B→next`). Generate them, then write a one-off manifest
pointing `prev/mid/next` at the `_A`/`_M`/`_B` files (the `disocclusion` preset
is the canonical covering/uncovering case for the gravity + double-edge metrics):

```sh
python framework/render/vulkan/bench/stage31_extrapolation/prep_frames.py
# then a manifest like:
#   size 256 256
#   triple disocclusion prev=disocclusion_A.rgba mid=disocclusion_M.rgba next=disocclusion_B.rgba
./build-fgq/fg_quality_scorer <that-manifest> -1 fgq_stage31.csv
```

## Build discipline / caveats

- **Standalone**, mirrors `stage11`'s CMake exactly (same Vulkan link, headless
  compute, no WSI). Compiles only the three committed shaders
  `OpticalFlowPipeline.cpp` consumes (verified `src/OpticalFlowPipeline.cpp:63-65`):
  `optical_flow_pyr_down`, `optical_flow_hier_match`, `optical_flow_warp`.
- **No change to the live app, no change to `stage11`/`stage31`'s files** —
  copy/reuse only.
- The flow runs at the **shipping default** `search_radius=2` (the pillar's
  documented Rc6/Rf2 default, `OpticalFlowPipeline.hpp:54-57`), so Mode A
  mirrors what the app ships, not a swept radius.
- `frames/`, `build-fgq/`, and `*.csv` are git-ignored.

## Regression gate (C8 — the scorer wired into a number that fails a build)

The scorer is wired into a **deterministic FG-quality regression gate** (PhyriadFG
master-plan fixed point C8 / family H1 — turn the eyeball into a reproducible gate):

- **Fixture:** `prep_zoo_sequence.py` regenerates a procedural held-out VFI test-field
  (7-motion zoo, deterministic / fixed-seed → no committed binary blobs).
- **Baseline:** [`docs/framework/FG_QUALITY_BASELINE.json`](../../../../../docs/framework/FG_QUALITY_BASELINE.json)
  — per-preset Mode-A means of `psnr_db` / `ssim` / `dbl_edg_m` / `flowdsc`, ±
  per-metric tolerance (a **pct + absolute** floor; the abs floor is what makes the
  near-zero metrics — `pan`'s `dbl_edg_m` ~ 0 — gateable).
- **Gate:** [`scripts/check_fg_quality_regression.ps1`](../../../../../scripts/check_fg_quality_regression.ps1)
  — `pwsh -File scripts/check_fg_quality_regression.ps1` (exit 1 on a regression);
  `-UpdateBaseline` to relock after an approved change; `-CsvPath <csv>` to judge an
  existing CSV without re-scoring.
- **CI:** [`.github/workflows/fg-quality-regression.yml`](../../../../../.github/workflows/fg-quality-regression.yml)
  — self-hosted-GPU-gated (the scorer needs a GPU; GitHub-hosted runners have none),
  mirroring `bench-regression`'s "real gate is dedicated hardware" pattern.

**Determinism / portability (measured first-hand 2026-06-22):** Mode-A is
**byte-identical across NVIDIA GPUs** (RTX 4090 == GTX 1080 Ti) but **drifts slightly
across vendors** (AMD iGPU: ≤ 1.12 dB PSNR / 0.078 `dbl_edg_m` / 0.26 px `flowdsc`).
The README's earlier blanket "GPU-independent" wording is true **within** a vendor,
overstated **across** — the baseline is NVIDIA-canonical and the tolerances absorb the
cross-vendor fp drift, so a clean tree passes on NVIDIA *or* AMD.

## Made with my soul - Swately <3
