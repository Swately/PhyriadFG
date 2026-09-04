# S2.T3 — gate record: the player, and the barcode surviving the whole capture chain · 2026-09-04

> Phase T3 of [`../MOTION_TRUTH_MASTER_PLAN.md`](../MOTION_TRUTH_MASTER_PLAN.md) (strategy S3).
> **Verdict: PASSED**, and it closes the gap the T2 record named as its own honest limit — "the zoo is
> generated, not yet consumed". It is consumed now, and the ground-truth identity of every captured
> frame is recoverable **through the entire FG**. Every number is quoted from command output.

## 1 · What it is

`tools/motion_truth/play_frames.ps1` presents a `marker_zoo` sequence on screen at a fixed cadence in an
ordinary, capturable, borderless window — **no `WDA_EXCLUDEFROMCAPTURE`**, per the June TB-C1 note: a
window the FG cannot capture measures nothing.

The pacing is **borrowed, not reinvented**: `ball_zoo.ps1`'s proven loop — Stopwatch deadlines, winmm
`timeBeginPeriod(1)`, BufferedGraphics, sleep(1) far from the deadline and spin the tail, resync rather
than spiral on a miss. `Windows.Forms.Timer`'s ~15.6 ms granularity caps at ~64 fps and could not have
held 120.

Frames are preloaded, and the RGBA→BGRA swap plus the Bitmap fill happen in **compiled** code
(`Add-Type`), because per-pixel PowerShell over 921,600 pixels × N frames would cost minutes at load.

## 2 · The gate

### 2.1 · Cadence held at both rates

```
[zoo-play] fps=60.1  target=60  frame=12/60 loops=12 missed=0
[zoo-play] fps=120.1 target=120 frame=34/60 loops=68 missed=0
```

| Criterion | Result |
|---|---|
| achieved fps within 1 % at 60 | **60.1–60.3**, error ≤ 0.5 % |
| achieved fps within 1 % at 120, for 30 s | **120.1**, error **0.08 %**, sustained over 30+ s |
| missed ticks | **0** at both rates |

A missed tick is **counted and printed**, never smoothed: the point of a fixed-cadence source is that
its rate is known, and an unreported drop turns a position table into a lie about when a frame was shown.

### 2.2 · The FG ingests it as ordinary content

`phyriad_fg.exe --window 'RA Motion Zoo' --qdump DIR 12`:

```
[ra] 240.1 fps (present) | wap tick 240/s (arr 59) | cap 59/s | cons 61/s | uniq 240/s
     | warp 3.98ms | lat 15.8ms | ps 240/s ok=9360 to=0 er=0
```

`in` ≈ **59–61/s** against a 60 fps source, `uniq 240/s`, present 240 — the 4× ladder runs on the zoo
exactly as it runs on a game. Nothing was special-cased for measurement.

### 2.3 · The identity survives the entire chain — the result that matters

The barcode was decoded from the `prev` and `next` planes **as dumped by `--qdump+`**, i.e. after zoo
render → GDI+ blit → window → WGC capture → convert → the warp's anchor images → readback:

| | |
|---|---|
| triples decoding to a valid index (0–59) | **12 / 12** |
| **step from `prev` to `next`** | **1 on 12 of 12 (100 %)**, the 59→0 wrap handled |
| duplicates or drops | **none** |

```
q000000 t=0.1257  k_prev 53  k_next 54  step 1
q000003 t=0.8866  k_prev 59  k_next  0  step 1     <- the loop wrap, decoded correctly
q000007 t=0.8747  k_prev 39  k_next 40  step 1
```

This is what makes M1 possible: the FG's `t` can now be tied to an **exact pair of known source
frames**, so `p_model = p(k_prev) + t·(p(k_next) − p(k_prev))` is computable and `p_true = p(t_real)` is
evaluable at the same instant. A duplicate or a dropped source frame would show as a step ≠ 1 instead of
silently corrupting the table.

## 3 · A documented claim the measurement corrects

`src/cli/cli.cpp:105-106` (the `--help` text) and `src/present/present.cpp:1409` both describe the
triple as **"real N / live FG / real N+2"**. The decoded barcodes say the span is **exactly 1**:
`k_next − k_prev = 1` on 12 of 12 triples.

The `N+2` wording is a survivor of the **held-out** design, where the middle real is withheld as ground
truth (`prev = N`, truth = `N+1`, `next = N+2`). This tap is explicitly **truth-less** — its own
manifest header says "TRUTH-LESS held-out triples (live FG, no `mid=`)" — so no frame is held out and
the pair is the FG's own working pair, `(N, N+1)`. Corrected in both places.

The extractor is unaffected either way, because it reads the decoded indices rather than assuming a
span — which is precisely why the barcode exists.

## 4 · Honesty ledger

- **Memory is a real bound, not a caveat.** 1280×720×4 = 3,686,400 B per frame, so 120 frames is 442 MB
  preloaded. `-MaxFrames` caps it. At 1920×1080 the plan's own advice (cap at 60 frames, or stream a
  ring of 16) still stands and is **untested here** — every number above is 720p.
- The sequence used was 60 frames and **loops**. A loop boundary is a content discontinuity the flow
  stage sees as a scene cut; it did not disturb the step measurement, but no run here was long enough
  for a loop artefact to matter and none was looked for.
- Playing at a rate different from the generated one is allowed and warned about, because `p(t)` is
  parameterised in **seconds**: the extractor must use the playback rate, not the manifest's. That
  warning is printed, not enforced.
- Achieved-rate numbers are the player's own accounting. They were corroborated by the FG's independent
  `cap`/`arr` count (59–61/s), but no third clock was used.
- `NearestNeighbor` + `PixelOffsetMode.Half` are set so the blit is a 1:1 copy. If the window is ever
  scaled, that assumption breaks and the markers move sub-pixel for a reason that has nothing to do
  with the FG. Untested outside 1:1.

*Made with my soul - Swately <3*
