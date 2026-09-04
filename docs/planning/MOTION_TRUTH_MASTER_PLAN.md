# MOTION_TRUTH_MASTER_PLAN — exact-motion verification of generated frames, as data

> **Diátaxis type:** Planning / design (Explanation). **Plan tier:** **Tier-1** (substantial, no
> qualifying risk: an instrument; it touches the present path only through the existing `--qdump`
> readback and adds sidecar dumps on the SAME synchronous path — the async-present path is explicitly
> out of scope, so no use-after-reset surface is opened). Companion:
> [`MOTION_TRUTH_IMPLEMENTATION_STRATEGIES.md`](MOTION_TRUTH_IMPLEMENTATION_STRATEGIES.md).
> **Status:** `in execution`.
> **DONE and gated:** T0, T1, T1b, T1c (2026-09-03 — [`records/S2_T0_T1_GATE.md`](records/S2_T0_T1_GATE.md),
> [`S2_T1B_GATE.md`](records/S2_T1B_GATE.md), [`S2_T1C_GATE.md`](records/S2_T1C_GATE.md));
> **T2** ([`S2_T2_GATE.md`](records/S2_T2_GATE.md), 2026-09-04) and **T3**
> ([`S2_T3_GATE.md`](records/S2_T3_GATE.md), 2026-09-04) — the zoo is now generated AND consumed, and the
> frame-ID barcode survives the whole capture chain (step 1 on 12 of 12 triples).
> **T6 is BUILT, gate NOT passed** ([`S2_T6_GATE.md`](records/S2_T6_GATE.md)): §6 closed the sub-pixel
> deadzone — it was the periodic test content, not the shader — and §7 characterised what replaced it:
> the residual is driven by displacement MAGNITUDE, not phase, and the oracle reaches **k = 0.989 at
> 4–8 px**. M4's corpus must be aperiodic and scored at ≥ 2 px.
> **T4 is DONE** ([`S2_T4_GATE.md`](records/S2_T4_GATE.md), 2026-09-04): §4.1 at 0.045–0.060 px on raw
> frames, §4.2 at 0.085 px through the FG, the gate seen red twice and fixed. The first M1 numbers exist
> as a first look (HUD 0.046 px kept exact; moving classes 0.9–1.5 px; the `fast` control fires).
> **T5 is DONE** ([`S2_T5_GATE.md`](records/S2_T5_GATE.md), 2026-09-04): the baseline table exists at
> `docs/evidence/MOTION_TRUTH_BASELINE.md` with `r` on every class. **§4 status:** 4.1 met, 4.2 met,
> 4.4 met; **4.3 NOT met on `linear`** (r = 0.47, n = 9 — UNRELIABLE, marked by the tool) and met on
> accel/circular/crossing/fast. The instrument is `measured` for four classes and not for the one the
> criterion names; sample size is the remedy, jitter and duplicates being excluded.
> **Every phase T0–T5 is now built and gated.** T6 remains built with its gate not passed.
> **§2.3's premise was WRONG and is corrected there**: a replay record is not (prev, next, MV, gme,
> push, t) — the shipping default reads four more planes, and T1c dumps those plus the two dissidence
> masks that other feature sets need. Every "exists / builds / measured" claim below was verified
> first-hand this session; the instrument itself is not built. MUST/SHOULD/MAY are BCP-14.
> **Serves:** the frozen objective's metric **M1** in
> [`aap/A0_FROZEN_OBJECTIVE.md`](aap/A0_FROZEN_OBJECTIVE.md) and the fixed points **C8** (quality
> regression gate) / **C11** (controlled testbench) of `../research/PHYRIADFG_OBJECTIVE_VISTA.md`.
> It is the geometry/phase half of the June testbench design (**TB-C1** source + **TB-C7** frame-ID,
> `../research/FG_TESTBENCH_MASTER_PLAN.md` §2), narrowed to what the operator asked for now.

---

## 0 · The operator's directive (2026-09-02) and what this plan does NOT do

"No me interesan de momento los efectos de la generación como el ghosting; me interesa la generación
de frames exactos que simulen de forma correcta el movimiento… quiero ver datos, no imágenes."

**Goal:** a deterministic process that yields the POSITIONS of known moving points in real and generated
frames, so that "the generated frame is correct" becomes a table — `(marker, t, expected_xy, observed_xy,
error_px)` — that an LLM session verifies exactly, without looking at pixels.

**Non-goals (parked, not dropped):** ghosting / crossfade / seam / blur metrics (the photometric axis —
`fg_quality_scorer`'s `dbl_edg_m`, `xfade_res`, PSNR/SSIM stay available but are not this plan's gate);
the perceptual 2AFC axis (TB-C5); the photonic camera axis (TB-C6); real-game content.

## 1 · What exists today (first-hand, 2026-09-02)

| Asset | State | Gap for this plan |
|---|---|---|
| `--qdump DIR N` (`src/present/present.cpp:1315–1356`) | writes `q%06d_{prev,live,next}.rgba` + `manifest.txt` with `size W H` and `t=` per triple | fires only when `!ap` — **inert under the shipping default `async_present=true`** (`src/cli/cli.hpp:819`); dumps no MV field, no gme model, no push block |
| `fg_quality_scorer` | in the container catalog (`F:\Phyriad\catalog\cpp\render\vulkan\bench\fg_quality_scorer`), **builds on this rig** (exit 0, 155,648 B); Modes A/B/T run (T on live triples: 6 rows; A on the zoo: `pan` 99.0 dB, `occlude` 27.5–29.1 dB) | not in the PhyriadFG repo (stripped in `2f47ea9`); Mode A re-runs the CATALOG's flow, which diverged from the vendored one (427/146 diff lines) |
| `prep_zoo_sequence.py` | numpy-only, deterministic, 7 motion presets, exact closed-form midpoint | markers/positions are not exported; no frame-ID tag; offline files only (no on-screen player) |
| `tools/ball_zoo.ps1` | live GDI+ source, exact fixed step per frame, up to 480 fps | one ball, no frame index, no analytic export |
| `tools/capture_dump` | captures a window/monitor to `.rgba` + manifest | capture only; no playback |
| Python on the rig | 3.13, **numpy 2.5.2 only** (no Pillow / OpenCV / scikit) | every tool here is numpy + stdlib |
| The warp math | `shaders/wap_warp.comp:518–521` samples `A[uv−t·mv]`, `B[uv+(1−t)·mv]`; the minimal core's `optical_flow_warp.comp:126–130` is the same form | none — the instrument measures both trees with one extractor |

## 2 · The instrument (the design)

Five pieces, all data-in / data-out, none of which requires a human or an LLM to look at an image:

```
[1] marker_zoo.py ──frames k + p_i(k) + frame-ID barcode──▶ [2] player (on-screen, fixed cadence)
                                                                      │ (the FG captures it, DDA/WGC)
        [3] --qdump+ : prev/live/next + t + MV + SAD + gme + push  ◀──┘
                                                                      │
[4] marker_extract.py ──decode k_prev,k_next from barcodes; NCC search per marker──▶ detections CSV
                                                                      │
[5] motion_report.py ──per class / per t-bin: mean, p95, ghost-rate (n_peaks≥2), miss-rate; two runs → r──▶ verdict table
```

1. **`marker_zoo.py` (numpy).** Renders a sequence of `T` frames at `W×H`: a background of class
   {flat, value-noise, periodic grating} plus `K` markers — 12×12 px unique binary patterns with a 1-px
   border, sub-pixel placed by bilinear splat (AA — so consecutive frames are never byte-identical and
   the capture dedup cannot collapse them; the `ball_zoo` lesson) — on trajectories of class {linear,
   accelerating, circular, crossing (two markers whose paths cross → occlusion), static-HUD (screen-fixed
   over a panning background)}. Each frame carries a **16-bit frame-ID barcode**: 16 blocks of 8×8 px
   (black/white) in one row inside the top 16-px border, which the scorer's metrics already crop (16 px)
   and which the extractor masks out. Exports `positions.csv`: `frame k, marker i, class, x, y` and the
   analytic function parameters, so `p_i(t)` is evaluable at ANY real t (not only integer k).
2. **The player.** Presents the frames on-screen at a fixed cadence (60 / 120 fps) in a normal, capturable
   window (it MUST NOT set `WDA_EXCLUDEFROMCAPTURE` — the June TB-C1 note), so the FG captures it exactly
   as it captures a game. First cut: `ball_zoo.ps1`'s proven GDI+ / Stopwatch loop extended to blit
   pre-rendered frames from RAM (1280×720 × 120 frames = 442 MB, preloaded); target cut: a small D3D11
   FIFO swapchain presenter (the TB-C1 design) when the GDI path proves cadence-limited.
3. **`--qdump+`** — the replay record. The existing triple dump plus sidecars for the SAME tick:
   `q%06d_mv.rg16f` (mvw×mvh×4 B), `q%06d_sad.rg16f`, `q%06d_push.bin` (the warp's push block as
   submitted, `sizeof(pcw)` bytes), and manifest tokens `mv= sad= push= gme=a,b,c,d,e,f mvw= mvh= gen=`
   appended to the `triple` line. **CORRECTED 2026-09-03 (T1c, `records/S2_T1C_GATE.md`): that list is
   NOT sufficient.** Decoding a real push block against the shader's bindings showed the SHIPPING DEFAULT
   also reads the backward MV field (`occl_thresh`, `phase_anchor_on`), the persistence field
   (`inertia_thresh`), the second-best SAD candidates (`ambig_on` + `gme_on`) and the target-generation MV
   (`vblend_on`) — four more planes; those and both dissidence masks (which THIS default does not read:
   every ordinary site is gated on `matte_on` = 0) are dumped as `mvb= c2= dis= disb= per= mvt=`
   with `tgen=` recorded at the upload site. `u_field` and `u_prev_out` are genuinely unread under this
   default (their gates are all 0), verified from the same push block (trailing tokens are ignored by existing parsers — the manifest's own
   contract). The dump needs the SYNC present path, and `resolve_config` (`cli.cpp:227-231`) auto-disables
   `--async-present` for any run that asks for it, printing the reason — so `--qdump` alone is enough
   (verified 2026-09-03; the earlier "inert under the default" note was a misreading of the present-side
   `!ap` gate). Passing `--no-async-present` explicitly is equivalent. Making the async
   path dumpable is a separate Tier-2 item, out of scope here).
4. **`marker_extract.py` (numpy).** For each triple: decode `k_prev`, `k_next` from the barcodes of the
   two real planes (exact, jitter-immune); compute the two references for each marker at the triple's
   `t`: **`p_model` = `p(k_prev) + t·(p(k_next) − p(k_prev))`** (what a correct translational warp of
   this pair MUST produce) and **`p_true` = `p(t_real)`** with `t_real = k_prev + t·(k_next−k_prev)`
   (the analytic truth; equals `p_model` for linear motion, diverges for accelerating/circular — that
   divergence is the MODEL's error, not the code's, and is reported separately). Then NCC template
   search in a ±R window around `p_model` on the `live` plane (and on `prev`/`next` as the self-check,
   where the error MUST be ~0). Output per marker: **the detection record** — all peaks above a threshold
   with sub-pixel position (parabolic fit, the matcher's own sub-pel form) and NCC score. `n_peaks ≥ 2`
   is the ghost flag (not scored now, but recorded so the parked axis costs nothing later); `n_peaks = 0`
   is a miss.
5. **`motion_report.py`.** Aggregates: per marker class × per `t` bin: mean, p95 of `|obs − p_model|` and
   of `|obs − p_true|`; phase error `err_px / |v|` in ms-equivalent; miss rate; ghost rate. **Two runs per
   condition; the run-to-run correlation `r` of the per-marker errors is reported with every table**
   (DATA_INSPECTION_PROTOCOL DI-3; below `r = 0.5` a per-cell number MUST NOT be used).

## 3 · Why this is the right abstraction for the objective

- The warp MOVES content to positions: `live ≈ cur[uv + (1−t)·mv]` under the default. Position error
  per marker IS the warp's geometric correctness projected through the MV field; measured on `prev`/`next`
  it calibrates the extractor; measured on `live` it isolates what the FG did.
- The phase axis — is the frame at the right MOMENT — is invisible to image metrics and visible here.
- Prior art (known methodology, not novelty): fiducial markers; point-tracking evaluation (TAP-Vid class);
  flow benchmarks with analytic ground truth (Middlebury class). The June SOTA dossiers already cite the
  held-out-frame protocol; this plan adds the analytic-at-any-t reference via the frame-ID tag (TB-C7).
- The two gaps of a pure point system, closed by design: (a) ghosting is invisible to a single position →
  the multi-peak detection record; (b) markers are easier than real content → the zoo deliberately includes
  sub-block (< 8 px) markers, periodic-grating backgrounds (aperture), crossings (occlusion) and the
  static-HUD-over-pan case (the HSR ghosting reproduction), each reported as its own class.

## 4 · Acceptance — when is the instrument `measured`?

The instrument is `measured` (usable as a gate) only when ALL hold, each verified first-hand:
1. **Extractor self-test seen red then green:** a real frame shifted by N px artificially reports N ± 0.1 px;
   an unshifted real frame reports ≤ 0.1 px mean over all markers.
2. **Real-plane check on a live run:** on `prev`/`next` planes captured through the FG, per-marker error
   ≤ 0.25 px mean (the capture path adds no geometry).
3. **Reliability:** two independent live runs of the shipping default on the `linear` class give
   per-marker-error run-to-run `r ≥ 0.5` (else the cell is unusable and the cause — player jitter,
   duplicates, pairing — is chased before any verdict).
4. **A baseline table** for the shipping default (`wap_warp` on the E1 tree) exists with the four
   numbers of A0's M1 per class, `r` attached — the reference every later candidate is compared against.

## 5 · Phase plan

| Phase | Deliverable | Gate |
|---|---|---|
| **T0** | Port `fg_quality_scorer` into `tools/fg_quality_scorer/` pointing at the vendored pipeline; document `--no-async-present` as the `--qdump` precondition | scorer builds from the repo; Mode T on a fresh qdump reproduces this session's 6-row smoke shape |
| **T1** | `--qdump+` sidecars (MV, SAD, push, gme, gen) | a dumped tick's MV plane has `mvw×mvh×4` bytes and decodes to plausible px units; push block size == `sizeof(pcw)` |
| **T2** | `marker_zoo.py` + `positions.csv` + barcode | zoo frames render; barcode decodes losslessly from the saved frames; `p(t)` evaluates at non-integer t |
| **T3** | player (GDI+ first cut) | 60/120 fps held (printed achieved rate), captured by the FG with `in` ≈ source fps |
| **T4** | `marker_extract.py` + the self-tests (§4.1–4.2) | red-then-green shift test; real-plane ≤ 0.25 px |
| **T5** | `motion_report.py` + DI-3 two-run baseline of the shipping default (§4.3–4.4) | `r` reported; the baseline table exists |
| **T6** | The CPU reference warp (the E7 equivalence test of `RESTRUCTURE_PLAN` §3): replay (prev, next, MV, gme, push, t) → reference `live`; byte-diff vs the dumped `live` | identical, or every differing pixel explained |

T1–T2 and T3–T4 are independent pairs and MAY proceed in parallel; T5 needs all; T6 needs T1.

## 6 · Trade-offs and alternatives considered

- **Synthetic zoo vs real content.** Chosen: synthetic, because only it gives `p(t)` at any t exactly.
  Real content re-enters as a later axis (the June TB plan's eye/camera axes). The synthetic classes are
  chosen to include the pipeline's known failure modes so "easy" is not the default.
- **Frame-ID barcode vs centroid-based index recovery.** Chosen: barcode — exact and independent of
  capture timing; centroids would fail under duplicates/drops and need per-marker disambiguation.
- **GDI+ player first vs D3D11 presenter first.** Chosen: GDI+ first (proven to 480 fps in
  `ball_zoo.ps1`, one afternoon), D3D11 when cadence-limited. The player's own jitter is measured, not
  assumed (its achieved-fps print + the barcode gaps in the captured sequence).
- **Extending `--qdump` vs a new dump path.** Chosen: extend — the triple dump already has the tick
  scope, the host pointers and the manifest; the sidecars are additive and backward-compatible.
- **Async-present dump.** Deferred: it requires reading back the async slot's output across the
  non-blocking fence — a Tier-2 change of the class the REAL_FAST_PATH register warns about. Measurement
  runs do not need async present.

## 7 · Honesty ledger

- Nothing in §2 is built; the timings in §5 are not estimated (no calendar claim).
- The 0.1 / 0.25 px thresholds in §4 are design targets, not measurements — the first live run may move
  them, and if it does the change is recorded here with its cause.
- The instrument measures geometry and phase; a frame that is geometrically right and photometrically
  wrong (a seam, a ghost) passes it — by the operator's directive, deliberately, for now.
