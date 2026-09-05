# M1 finding: the 1.7 px low-phase error is one default-on layer, not the core · 2026-09-04

> The operator asked whether the 1.7 px positional error at low interpolation phase (T5's baseline) is
> acceptable. The honest answer has two halves. **By what standard:** the project froze no absolute
> bar, and the literature has none either (§3). **By experiment:** the number is not a property of
> frame generation at all — it is produced by a single default-on layer, the 3×3 vector-median
> consensus pass on the MV field, and with that pass off the shipping warp lands moving content at
> **0.27 px across every phase**. Four conditions, two runs each, every number from command output.

## 1 · The question, decomposed before it was answered

The T5 baseline (`docs/evidence/MOTION_TRUTH_BASELINE.md`) showed the error falling with phase:
1.668 px at `t ≈ 0.1`, 0.373 at `t ≈ 0.85`. Three measurements on that same data, before any new
capture, turned "is it acceptable" into "what is it":

**It is not the flow.** The dumped MV field, sampled at each marker and compared with the analytic
displacement, has an endpoint error of **0.464 px mean, 0.180 median** — and **0.164 px** on the
`linear` class. `(1−t)·EPE` explains only 0.52 of the 1.67 px at low phase; **the warp adds +0.64 px
beyond what its MV justifies**, concentrated at low phase (`err − (1−t)·EPE` by phase: 1.15 → 0.77 →
0.55 → 0.31).

**It is not the phase as such.** A fit `err = a + b·(1−t)` gives a floor of 0.144 px and a slope of
1.565 px, but R² = 0.246 — the (1−t) trend is real and does not dominate.

**It has a sign, along the motion.** Projecting the error vector onto each marker's velocity:
**88–100 % of markers are displaced forward, toward their `t = 1` position**, by 0.41–0.64 of
`(1−t)·|v|`, with an across-track error of +0.01 px. The generated frame shows the marker roughly
half-way between where it should be and where it will be next frame. That is not noise; it is a
mechanism that under-applies the motion.

## 2 · The mechanism, isolated by four conditions

Same zoo (`zoo_static`: value noise, no pan, 15 markers at 1.75–8 px/frame), same player, same FG
binary, two independent 16-triple captures per condition, `fast` excluded. Low phase = `t < 0.25`.

| condition | flags | found | err, all phases | **err, low phase** | along, low phase | run A / run B |
|---|---|---|---|---|---|---|
| **DEFAULT** | (shipping) | 227/384 | 0.847 px | **1.668** | +1.609 | 0.918 / 0.776 |
| stasis mix off | `--st-no-stasis` | 268/384 | 0.933 | **1.693** | +1.672 | 0.881 / 0.984 |
| guided fetch off, blind median on | `--no-mv-guided --mv-median` | 255/384 | 0.788 | **1.493** | +1.417 | 0.835 / 0.745 |
| **consensus pass off, bilinear fetch** | `--no-mv-guided` | 248/384 | **0.285** | **0.271** | **+0.072** | 0.267 / 0.301 |

Per-phase, the last condition: 0.271 / 0.246 / 0.321 / 0.301 px at `t` ≈ 0.1 / 0.35 / 0.65 / 0.85 —
**flat**. The forward drag is gone (ratio 0.02–0.07 of `(1−t)·|v|` against 0.41–0.64).

What the four conditions say, one at a time:

- `--st-no-stasis` changes nothing (1.693 vs 1.668). The screen-static evidence mix `w_s` — the first
  suspect, because it re-admits `cur` — is **not** the cause.
- `--no-mv-guided --mv-median` keeps the error (1.493). This condition turns the warp's colour-guided
  *corner pick* off but leaves the 3×3 vector-median *consensus pass* running. So the corner pick is
  **not** the cause either.
- `--no-mv-guided` alone removes it. The only thing this condition removes that the previous one did
  not is the consensus pass itself, which runs `if(use_mv_median || use_mv_guided)`
  (`src/present/present.cpp:820`) and rewrites the GPU MV image *after* the host field is copied.

**So the cause is `shaders/mv_median.comp`, the 3×3 component-wise vector-median on the MV field,
armed by default through `mv_guided = true`.** Its own header says what it is for: "isolated bad
vectors at a moving rim … a stray vector that disagrees with its 8 neighbours is replaced by the
neighbourhood median — content-independent, no photometry". On a 6–24 px marker crossing a static
background, the marker's tile IS the stray vector: its eight neighbours are static, the median is
≈ 0, and the marker's motion is voted away. The warp then samples `cur` at nearly `uv`, which is the
marker's `t = 1` position — the forward drag, by construction. The `(1−t)` scaling follows because the
lost displacement is `(1−t)·mv`.

**Why the dump did not show it directly:** `--qdump+` writes `hostMV[gen]`, the field *before* the
consensus pass; the pass rewrites `wapMVA` on the GPU. The record's MV (EPE 0.16 px) is honest about
the matcher and blind to the pass. That is a hole in the replay record, now known: T1c's completeness
audit checks that every plane the shader reads is present, not that the plane's *content* is what the
shader read. A `mv1` post-consensus readback is the fix and is not taken here.

### 2.1 · The pass, seen directly (added the same day)

The hole named in §2 is closed: `--qdump+` now reads `wapMVA` back **after** the pass (`mv1=`). On two
default captures of the same zoo, the pass touches **97.5 % of MV texels** (median change 0.048 px — a
light smoothing everywhere) and **at the marker tiles doubles the endpoint error, 0.829 → 1.642 px**,
leaving **63 % (mean) / 83 % (median)** of a moving marker's motion; the largest per-tile change is
9.5 px. Fed `mv1` instead of `mv`, the T6 oracle goes from k 0.73 to **1.000 at ≥ 4 px** (§8 of its
record) — the same mechanism, seen from the other side.

### 2.2 · Reliability of the comparison (DI-3)

Per-condition run-to-run `r` on `(marker, phase-bin)` means: DEFAULT **0.86** (n 40), stasis-off
**0.78** (44), median-only **0.85** (43), consensus-off **0.36** (45). The last is low because with
every error near the floor there is little variance left to correlate — the per-*cell* rule marks it
UNRELIABLE and that tag stands. **The finding is a between-condition effect**, 1.668 → 0.271 px at low
phase, and its reliability is the per-run spread: 0.918/0.776, 0.881/0.984, 0.835/0.745 against
0.267/0.301. A 1.4 px effect over spreads of 0.03–0.14 px, on 227–268 detections per condition.

## 3 · "Acceptable" — by what standard

Researched in parallel (three Sonnet readers + a critic; findings quoted from sources, unsourced
numbers flagged by the critic and not used here).

**The project's own criterion.** `aap/A0_FROZEN_OBJECTIVE.md:32` freezes M1 as **comparative**: a
candidate core must not shift markers more than **0.10 px paired mean** against the shipping default,
p95 within the run-to-run spread. There is **no absolute px or ms bar anywhere in the repo**, and
`SINGLE_TRACK_MODE_PLAN.md` §3.3 left the low-phase step to "the operator's eye" without a number.
So by the project's frozen standard the question is not "is 1.7 px acceptable" but "does a candidate
core move markers relative to the default" — and §2 shows the default itself is the outlier: a core
without the consensus pass would fail M1's 0.10 px comparison *against the default* by 1.4 px, while
being 6× closer to the truth. **M1 as frozen would penalise the fix.** That is a defect in the
criterion's framing, recorded for A0's next revision, not a reason to keep the error.

**The field's standard.** No published positional-error acceptance figure exists for frame
interpolation; commercial FG (DLSS-FG, FSR 3, LSFG) is assessed by PSNR/SSIM/LPIPS or by eye, never
geometrically. The nearest anchors are optical-flow endpoint errors in the 0–10 px/frame bucket:
0.58–0.99 px for learned flow (GMFlow Table 1, read directly), ~1 px for classical methods; and one
2025 paper's motion-field metric on mainstream VFI at 2.06–3.32 (Daly, Ramsook, Kokaram,
arXiv:2508.09078, Table 4). Those are flow-vector errors, not marker positions — apples to oranges,
as the critic noted — but they place this warp's **0.27 px** without the pass and its raw matcher's
**0.16 px** on `linear` at the good end of the regime, and the default's 1.67 px at the poor end.

**Perception.** On this panel (27″ 1080p at 60–70 cm) one pixel is 1.5–1.8 arcmin, so the low-phase
mean is **2.5–3.0 arcmin** and its p95 **6–7 arcmin**; the high-phase 0.37 px is 35–41 arcsec.
Expressed as a fraction of the true per-frame displacement — the way cinema judder is quantified —
the 1.7 px is **21–97 % of a frame's motion** at the mean, against double-flash (25 %) and 3:2
pulldown (20 % + 34 %) judder amplitudes that are long established as visible. Two caveats the critic
insisted on and that stand: vernier acuity degrades past ~2.5 deg/s and these markers move at
3–13 deg/s, so static jitter thresholds (5–30 arcsec) are the wrong comparison; and no source reaches
240 Hz or this exact once-per-source-frame sawtooth, so the frequency dependence is **unmeasured**.
Smooth pursuit converts the temporal error into retinal displacement rather than averaging it —
which argues for *more* visibility, not less.

**Verdict on the standard.** The number sits in a range the field's own judder literature treats as
visible, on content the vision literature says is hard to judge precisely, and the project never set
a bar for it. That would have been an unsatisfying answer on its own. It is moot: **the error is
removable, and the removal costs nothing the instrument can see** — same found rate (248 vs 227),
lower degraded count at low phase (42 vs 47), and the raw matcher underneath is already at 0.16 px.

## 4 · What this means, calibrated

1. **The pure core is better than the shipping default at the thing the objective names.** With the
   consensus layer off, the shipping warp on a bilinear MV places moving content at 0.27 px, flat in
   phase. The operator's thesis — patches over a core that was sound — is measured true for this
   layer on this content.
2. **The consensus pass has a real purpose** (rim stamps and holes on flat content, per its header)
   and this measurement did not test that purpose: the zoo has no flat content and no rims of the
   kind it was built for. Turning it off is **not** a verdict on that failure mode; it is a verdict
   on what it does to small moving objects. Both must be measured before the default changes, and
   the instrument now can.
3. **M1 as frozen is mis-framed.** A comparative bar against a default that is itself the outlier
   penalises the fix. A0 is sealed and cannot be quietly edited; the disposition belongs in its next
   revision, and this record is the evidence for it.
4. **The replay record has a content hole.** The consensus pass rewrites the GPU MV after the host
   copy; `--qdump+` dumps the host copy. T6's oracle was therefore fed the pre-pass field and cannot
   see the pass — which also explains part of T6's own residual: the reference applied the honest MV
   while the shader applied the voted-away one. A post-pass `mv1` readback closes it.
5. **Nothing in the default changes here.** The finding is recorded, the control conditions are on
   disk with their `r`, and the switch is the operator's — it is a product default with a visible
   history behind it.

## 5 · Honesty ledger

- One scene class: small binary markers over aperiodic noise, static camera. The consensus pass's
  own target content (flat regions at a moving rim) is **absent** from this test. Real game content
  is untested.
- The `fast` control behaves differently under `--no-mv-guided` (12 found vs 48) — the pass helps
  hold content the matcher cannot track. That is reported, not explained.
- Per-cell `r` for the consensus-off condition is 0.36 and is marked UNRELIABLE by the tool; the
  finding rests on the between-condition effect and the per-run spread, stated in §2.1.
- The perception numbers are angular conversions of a mean; the frequency dependence of a 60 Hz
  sawtooth on a 240 Hz sample-and-hold panel is **not in the literature found**, and two of the
  research findings that would bear on it were unsourced and are not used.
- No `mv1` readback exists yet; the claim that the pass rewrites `wapMVA` is from the code
  (`present.cpp:820–830`, `mv_median.comp`), not from a dumped plane. The four-condition result does
  not depend on it.

*Made with my soul - Swately <3*

---

## 6 · The pass on its OWN target content (2026-09-04) — the other number the decision needed

§4.2 said the cost was measured and the purpose was not. `ball_zoo.ps1` gained `-BgClass flat` — a
uniform field with a 260 px disc moving 7 px/frame over it — which is the content `mv_median.comp`'s
header describes: "a wrong block MV that lands both warp samples on identical flat content passes the
photometric check … isolated bad vectors at a moving rim (block holes inside the object, detached block
stamps outside)". Two default captures (both `mv=` and `mv1=` in the same record) and two with the
pass off, bare B-track (`--st-no-stasis`), 16 triples each.

### 6.1 · In the MV field, before and after the pass, on the default records

| | before (`mv`) | after (`mv1`) |
|---|---|---|
| **stamps** — tiles outside the disc with \|mv\| > 1 px | **7,494** | **7,500** |
| **holes** — tiles inside the disc with \|mv\| < 3.5 px (half the true step) | 1,166 | 1,106 (−5 %) |
| the disc's own tiles, mean \|mv\| (true 7.0) | 6.1–9.9 | unchanged to ±0.05 |

The raw matcher emits **500–900 stray vectors per frame** on the flat field — the failure mode the header
names is real — and **the pass removes none of them**. Its design premise is an *isolated* outlier a 3×3
median can vote down; what the matcher produces on flat content is *clusters*, which a 3×3 median
preserves. It removes 5 % of the holes. It does not touch a large object's motion, so its cost is
specific to objects at or below the tile size — which §2 measured.

### 6.2 · At the output

| | pass ON (fed `mv1`) | pass OFF |
|---|---|---|
| disc position error, T4 method (run A / B) | **0.129 / 0.126 px** | **0.050 / 0.062 px** |
| rim tearing (gold pixels > 2 px outside the expected disc) | 0.00 % | 0.00 % |
| oracle, triples at `t ≤ 0.4` | max 1–7 levels, 0 px > 8 | max 1–2 levels, 0 px > 8 |
| oracle, triples at `t ≈ 0.62–0.88` | max 42–175, **424–1,058 px > 8** | max 30–103, 259–471 px > 8 |

On the content it was built for, the pass **produces no measurable benefit** — no stamp removed, no
rim protected (there was nothing to protect: 0 % tearing either way) — and a small cost: the disc lands
0.07 px further from where it should, and the oracle agrees with the output less.

### 6.3 · A second content hole, found here and NOT closed

The pass filters the **backward** field too (`if(use_bidir) median_filter(medPipe.set_mvb, wapMVBA)`,
`present.cpp:837`), and `--qdump+` reads back only the forward one. The phase anchor uses `−mv_bwd`
above `t ≈ 0.65`, so at high phase the oracle is still fed a pre-pass field — which is exactly where the
pass-ON records keep 424–1,058 disagreeing pixels while the pass-OFF ones keep fewer. **Closed the same
day:** `mvb1=` added; fed both post-pass fields the oracle is byte-exact on those triples (0 px > 8,
max 1 level) — `S2_T6_GATE.md` §9. The pass rewrites the backward field as much as the forward.

### 6.4 · What the two numbers say together

| content | the pass's cost | the pass's benefit |
|---|---|---|
| small objects (6–24 px) over aperiodic texture (§2) | **1.4 px** at low phase, half the markers degraded | — |
| a large object over flat content (§6) | 0.07 px | **none measured**: 0 of 7,494 stamps, 5 % of holes, 0 % tearing either way |

**The default is still untouched, and the switch is still the operator's.** What has changed is that the
decision now has both halves. Honesty: one disc, one speed, one flat field; the header's specific
scenario — a *moving rim* over flat content — is present but the disc is far larger than a tile, and a
rim at tile scale was not tested. The DI-3 `r` for the pass-ON disc error is 0.06 (both runs sit at the
0.1 px floor with no variance to correlate); the pass-OFF one is 0.86.
