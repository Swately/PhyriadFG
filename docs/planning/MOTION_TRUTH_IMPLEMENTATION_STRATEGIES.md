# MOTION_TRUTH_IMPLEMENTATION_STRATEGIES — the concrete edit sites, formats and validations

> Tier-1 companion of [`MOTION_TRUTH_MASTER_PLAN.md`](MOTION_TRUTH_MASTER_PLAN.md). **Status:**
> `designed` (2026-09-02). Every path / line / symbol below was read first-hand this session; each
> strategy names its verification. MUST/SHOULD/MAY are BCP-14. Byte-identical-off discipline: every
> change to `phyriad_fg.exe` is gated on a flag that already exists (`--qdump`) or a new default-off
> flag; the default run path is bit-for-bit unchanged.

## S0 — Port the scorer; document the `--qdump` precondition (phase T0)

- **Copy** `F:\Phyriad\catalog\cpp\render\vulkan\bench\fg_quality_scorer\{main.cpp, CMakeLists.txt,
  prep_zoo_sequence.py, README.md}` → `tools/fg_quality_scorer/`. In its `CMakeLists.txt` set
  `_render_vulkan` to `${CMAKE_CURRENT_SOURCE_DIR}/../../framework/render/vulkan` (the vendored pipeline)
  instead of `../..` in the catalog. The five `*.comp` and `src/OpticalFlowPipeline.cpp` it compiles exist
  there (verified: `framework/render/vulkan/shaders/optical_flow_{pyr_down,hier_match,hier_match_fg,warp,
  affine_fit}.comp`, `src/OpticalFlowPipeline.cpp`).
- **Verify the API drift** before building: the scorer calls `ofp.init(phys, dev, W, H, 2u, …)`
  (`main.cpp:847`); the vendored `init` (`OpticalFlowPipeline.hpp:152–`) shares the first 9 parameters
  with the catalog's and adds defaults after — expected to compile unchanged; if not, the diff of the two
  headers is the fix list (`diff` reported 386 lines, mostly comments and the fg-variant/candsel/prebake
  members).
- **Relock:** run `prep_zoo_sequence.py` → Mode A over all 7 presets → write
  `docs/evidence/FG_QUALITY_BASELINE.json` for PhyriadFG (copy the catalog schema; the values WILL differ
  from the catalog's because the flow differs — that is the point of relocking, stated in the file).
- **Document** in `README.md` (Run section) and in `print_help` (`src/control/cli.cpp`, the `--qdump` line):
  "`--qdump` needs the synchronous present path; `resolve_config` auto-disables `--async-present` for the
  run and prints why, so the flag alone suffices". Do NOT change
  the gating code (`present.cpp:1315`) in this plan.
- **Verification:** scorer builds from the repo (exit 0); `phyriad_fg.exe --monitor 0 --no-async-present
  --qdump <dir> 6 --exit-after 12` then scorer Mode T → 6 rows (the shape of this session's smoke).

## S1 — `--qdump+`: the replay-record sidecars (phase T1)

Edit site: `src/present/present.cpp` **1315–1356** (the existing dump block, sync path only).

- **What to dump per tick, and where it lives (all host-visible already):**
  - the MV plane the warp used: `hostMV[g]` and `hostSAD[g]` where `g` is the generation index of the
    pair presented on that tick — the SAME index `wap_upload` consumed on the last pair-advance
    (`present.cpp:713–836`; the implementation MUST read `g` from the same variable `wap_upload` was
    called with, not recompute it — verify by grep at implementation time). Sizes: `mvw*mvh*4` bytes each
    (RG16F), with `mvw = ofp.motion_width()`, `mvh = ofp.motion_height()` (the accessors, not a formula).
  - the gme model: `f_pair_gme_a[g]` (6 floats) and `f_pair_gme_valid_a[g]` (`fg_context.hpp:192–193`).
  - the push block as submitted: the `pcw` struct passed at `present.cpp:1151` — copy the exact bytes
    (`sizeof(pcw)`) into `q%06d_push.bin`; write `sizeof` into the manifest so the reader validates.
  - `t` (already emitted), `gen=g`, `mvw=`, `mvh=`.
- **Files:** `q%06d_mv.rg16f`, `q%06d_sad.rg16f`, `q%06d_push.bin` next to the triple; manifest line becomes
  `triple q%06d prev=… next=… live=… t=%.4f mv=q%06d_mv.rg16f sad=q%06d_sad.rg16f push=q%06d_push.bin
  pushsz=%u gme=%.6f,… gme_valid=%d gen=%d mvw=%u mvh=%u`. Backward-compatible: the scorer ignores trailing
  tokens (its own README states parsers ignore them; verify by running Mode T on a `+` manifest).
- **No new synchronization:** the block already runs after the tick's fence wait on the sync path; the host
  pointers are stable for the tick. If `hostMV[g]` could be overwritten by F during the copy (F writes the
  NEXT generation; `kGenRing=3`), note that the dump reads generation `g` while F may write `g+1` — the
  ring depth makes this safe by construction; state it in a comment with the ring constant cited.
- **Verification:** dump 6 ticks; a Python check reads `mv.rg16f` as `float16` pairs, asserts the file size,
  and prints the MV range in px (plausible: ≤ the per-frame displacement of the zoo); `pushsz` equals
  `sizeof(pcw)` printed by the exe at startup under `--qdump`.

## S2 — `marker_zoo.py` (phase T2) — `tools/motion_truth/marker_zoo.py`, numpy + stdlib only

- **CLI:** `--width --height --fps --seconds --classes linear,accel,circular,crossing,hud --bg flat|noise|grating
  --markers K --seed --out DIR`.
- **Markers:** 12×12 binary patterns from a seeded RNG, each with a 1-px white border and a distinct
  interior (Hamming distance ≥ 40 bits between any two — rejection-sampled); three sizes per class
  (6, 12, 24 px) so the sub-block case (< 8 px, the matcher's tile) is covered. Rendered by sub-pixel
  bilinear splat of the pattern at `p_i(k/fps)` (AA by construction).
- **Trajectories** (continuous in t seconds): linear `p0 + v·t`; accelerating `p0 + v·t + ½a·t²`; circular
  `c + R·(cos ωt, sin ωt)`; crossing = two linear markers whose paths intersect mid-sequence (z-order
  fixed: the first drawn is occluded); HUD = static markers over a background that pans at `v_bg`.
  Parameters chosen so per-frame displacement spans 0.5–8 px at 60 fps (inside the matcher's search reach)
  plus one "fast" class at 12–16 px (outside it — an expected-failure control that must show up as error,
  the EMPIRICAL_TEST "gate seen red").
- **Frame-ID barcode:** frame index `k` (16 bits) as 16 blocks of 8×8 px, MSB left, black=0/white=1, at
  `x ∈ [8, 136)`, `y ∈ [4, 12)` — inside the 16-px top border the scorer crops. A 1-px black frame around
  it. Decoder = mean of each block's 6×6 centre > 0.5.
- **Outputs:** `f_%06d.rgba` (RGBA8 row-major, the scorer/extractor format), `positions.csv`
  (`k,marker,class,size,x,y,visible`), `trajectories.json` (the parameters, so `p_i(t)` is evaluable at any
  t by the extractor), `manifest.txt` (`size W H`, `sequence f_ T`, `fps`).
- **Verification:** decode the barcode from every saved frame → equals `k` for all `T` frames; render
  `p_i(t)` at `t = k/fps` → equals `positions.csv` to 1e-6; visually irrelevant — the LLM checks the CSVs.

## S3 — The player (phase T3) — `tools/motion_truth/play_frames.ps1` (first cut)

- Extend `tools/ball_zoo.ps1`'s harness (Stopwatch pacing + `timeBeginPeriod(1)` + `BufferedGraphics`,
  the proven high-fps loop) to preload `f_%06d.rgba` frames into `System.Drawing.Bitmap`s (RGBA→BGRA swap
  once at load) and blit one per tick at `-Fps`; loop the sequence; print the achieved fps every second
  and the count of missed ticks. Borderless window at `-W -H -X -Y` like `ball_zoo`; no
  `WDA_EXCLUDEFROMCAPTURE`.
- **Memory bound:** 1280×720 × 120 frames = 442 MB preloaded (measured arithmetic: 3,686,400 B/frame); at
  1920×1080 cap the sequence at 60 frames (498 MB) or stream from a ring of 16 (the D3D11 cut).
- **Verification:** run at 60 and 120; achieved fps within 1% for 30 s; then a FG run with `--qdump+`
  shows `in` ≈ source fps and the decoded `k` of consecutive `prev/next` planes advances by a constant
  step (1 at 60-fps source with the FG at 240 Hz; duplicates/drops show as steps ≠ 1 and are counted).

## S4 — `marker_extract.py` (phase T4) — `tools/motion_truth/marker_extract.py`

- **Inputs:** a `--qdump+` manifest + the zoo's `trajectories.json` + `positions.csv`.
- **Per triple:** read `prev`, `next`, `live` (`.rgba` → luma float32); decode `k_prev`, `k_next` from the
  barcodes (mask the barcode region out of all matching); for each marker compute `p_model` and `p_true`
  (master plan §2.4); build the marker's template from the zoo's pattern rendered at the sub-pixel phase
  of `p_model` (or simply the 12×12 pattern — the NCC is tolerant; state which); NCC over a
  `(2R+1)²` window centred on `p_model` with `R = ceil(max displacement) + 4`; find all local maxima above
  `0.6·max(NCC)` and above an absolute floor (0.5); sub-pixel refine each by the 1-D parabolic fit on both
  axes (the same form as `optical_flow_hier_match.comp:240–256`); report.
- **Self-checks (the gate seen red):** `--selftest` shifts a real plane by `(dx, dy)` and asserts the
  extractor reports it within 0.1 px; on the real `prev`/`next` planes the expected position is `p(k)`
  exactly and the reported error MUST be ≤ 0.25 px mean.
- **Output CSV:** `triple,t,k_prev,k_next,marker,class,size,bg,px_model_x,px_model_y,px_true_x,px_true_y,
  n_peaks,obs_x,obs_y,ncc,err_model_px,err_true_px,plane` (one row per marker per plane in
  {prev,live,next}).
- **Cost:** K=24 markers × window 41² × template 12² ≈ 5.8 M multiply-adds per plane — trivial in numpy
  via FFT-free direct correlation on the small windows.

## S5 — `motion_report.py` (phase T5)

- Groups the CSV by `class × size × bg × t-bin (10 bins)`; reports mean/p95 of `err_model_px` and
  `err_true_px`, phase error `err_model_px / |v(t)|·1000/fps` in ms, miss rate (`n_peaks=0`), ghost rate
  (`n_peaks≥2`) — recorded, not gated.
- **DI-3:** takes TWO run directories; computes the Pearson `r` between the two runs' per-marker
  `err_model_px` (matched by `k_prev, marker`) per class; prints `r` beside every table; cells with
  `r < 0.5` are printed with `UNRELIABLE` and excluded from any verdict line.
- **The baseline table** (`docs/evidence/MOTION_TRUTH_BASELINE.md`): the shipping default on the E1 tree,
  per class, with `r` — the reference for the A/B in `aap/A0_FROZEN_OBJECTIVE.md` M1.

## S6 — The CPU reference warp (phase T6) — `tools/motion_truth/ref_warp.py`

- Implements the ~35-line core of `wap_warp.comp` in numpy on the replay record: primary MV bilinear
  fetch at `uv`; `A = prev[uv − t·mv]`, `B = cur[uv + (1−t)·mv]` (bilinear, clamp-to-edge); Gate 1 from the
  SAD pair; Gate 2 from the 8×8 tile mean of `|A−B|`; the default path's last writer
  `result = mix(B, cur[uv], w_s)` with `w_s = smoothstep(1.2, 3.0, (d_pixel+0.02)/(d_zero+0.02))` and
  stasis folding (`wap_warp.comp:1321–1328`). The MV-mutating default layers (mv-guided 324, inertia gate
  332, phase-anchor 397–410, bg-reclaim 358–395, ambig 426–453, vblend 507–514) are reproduced ONLY as far
  as the replay record carries their inputs; each omitted mutation is listed, and the byte-diff against
  the dumped `live` is reported per omitted layer — the diff IS the finding (which "dead" layer is alive).
- **Verification:** on a triple with all default-affecting layers disabled via flags
  (`--no-mv-guided --no-inertia --no-phase-anchor --bg-reclaim 0 --no-ambig --no-vblend`), the reference
  MUST be byte-identical to `live` (bilinear filtering differences excepted and bounded ≤ 1 LSB); then
  re-enable one layer at a time.

## S7 — Repository placement and the touchpoints

- `tools/motion_truth/` (the four Python tools + `README.md`), `tools/fg_quality_scorer/` (S0),
  `docs/evidence/` (baselines), `docs/planning/MOTION_TRUTH_*` (this pair).
- `README.md` §Run gains the measurement recipe (one paragraph); `CHANGELOG`-class note in the commit.
- Every LLM-written file ends with the authorship signature.

## Verification summary (what "done" means per strategy)

| S | First-hand check |
|---|---|
| S0 | scorer builds from repo; Mode T 6 rows; baseline relocked and committed with its differing values explained |
| S1 | sidecar sizes and `pushsz` verified; MV px range plausible; Mode T still parses the `+` manifest |
| S2 | barcode round-trip 100%; positions == p(t) at 1e-6 |
| S3 | achieved fps within 1% at 60/120; FG `in` ≈ source; k-steps counted |
| S4 | shift self-test red-then-green; real planes ≤ 0.25 px |
| S5 | two-run `r` printed; baseline table exists |
| S6 | byte-identical with layers off; per-layer diffs listed |
