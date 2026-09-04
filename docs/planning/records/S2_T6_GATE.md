# S2.T6 — gate record: the CPU reference warp, built and NOT yet passing · 2026-09-03

> The independent oracle R3's M4 gate rests on. **Verdict: BUILT, PARTIAL — the gate does NOT pass,
> but the pathology that blocked it is CLOSED (§6, 2026-09-04): the sub-pixel deadzone was the test
> content, not the shader. On an aperiodic background the deadzone population drops from 138,696
> pixels to 67, and the oracle reaches k = 0.956 (corr 0.987) where motion is real. What remains is a
> 5–13 % over-displacement on moving content — a precision question, not a pathology.**
>
> *(Original verdict, kept:)* **BUILT, PARTIAL — the gate does NOT pass.**
> The reference reproduces 99.88% of the shipping default's stored pixels exactly. A controlled
> diagnostic then shows where it is wrong, and the shape of that error is the useful part: **above 4 px
> of motion the reference is right (k = 0.955, correlation 0.97); below 0.5 px the shader moves
> essentially nothing while the reference moves content** (§4b). It is a sub-pixel deadzone, measured
> and bracketed, and not yet explained — six candidate causes were tested and refuted, each by a number.
> M4 must not run on this oracle until that is closed. Every number below is from command output; the
> failing measurements are stated as prominently as the passing one.

## 1 · Why the surface is small

`tools/ref_warp.py` (numpy + stdlib) implements the shipping default's **pre-store** path. That path is
far smaller than `wap_warp.comp`'s 1,336 lines suggest. With `single_track = 1.0` the store is

```
result = mix(B_samp, cur[uv], w_s)
```

and `A_samp` / `B_samp` are `const`, born around shader line 520 and never mutated afterwards. Everything
the commit / matte / onepos / blend / medoid / ts-smooth / extrap cascade computes is written into
`result` and then overwritten. So the output is decided by shader lines 279–522 plus the stasis bool:

| Step | What the reference does |
|---|---|
| primary MV | `guided_mv` hard corner pick at `mv_guided − 1 = 0.1`, else bilinear |
| inertia gate (b) | a static-history block refuses a corner MV over 2 px |
| `bg_reclaim = 4` | the three-hypothesis damp (local vs model vs screen-static) |
| phase anchor | `mix(mv_fwd, −mv_bwd, smoothstep(0.35, 0.65, t))` |
| ambiguity | runner-up arbitration against the gme referee |
| vblend | `mix(mv, 2·mv − mv_target, smoothstep(0.6, 1, t)·0.5)` |
| store | `A/B` samples, `d_pixel`, stasis, `w_s`, the single-track mix |

**Bug-for-bug, deliberately** (the convergence plan's XR7): the phase anchor mixes from `mv_fwd`, captured
*before* `bg_reclaim` ran, so wherever the anchor is active the reclaim's damping is discarded. The
reference reproduces that rather than fixing it. An oracle that improves on its subject cannot judge it.

**Scope is enforced, not assumed.** `unsupported()` refuses a triple by name when the push arms
`single_track ≤ 0.5`, `blend_solo`, `mv_edge_snap`, `cam_lead`, `matte_on`, `bg_snap_on` or `band_xfade`.
A refusal is loud; a wrong number would not be.

## 2 · What passes

On a 16-triple `--qdump+` record of the shipping default (ball zoo, 60 fps source, 240 Hz panel):

| | value |
|---|---|
| pixels reproduced **exactly** | mean **99.88%**, min 99.87%, max 99.91% |
| within 1 LSB | 99.90% |
| worst single pixel | 237 of 255 |
| pixels off by more than 8 | 739–788 per frame, of 921,600 |

The disagreement is **not** scattered: it sits in a ring at the moving object's silhouette, bounded to
about 800 pixels, in the region the MV field marks as moving.

## 3 · What does NOT pass, and how it was found

Exact-match on the default record flatters the reference, because `stasis` is true on 99% of a static
zoo frame and those pixels are a straight copy of `cur`. Re-running with `--st-no-stasis`
(`single_track = 2.0`) strips that away: the store becomes a bare `result = B_samp`, so the output *is*
the warp.

| record | exact match | displacement scale `k` | correlation |
|---|---|---|---|
| shipping default | 99.88% | 0.829 | 0.949 |
| bare B-track | **84.39%** | **0.702** | 0.871 |

`k` is the least-squares scale between the change the reference makes to `cur[uv]` and the change the
GPU made, over pixels carrying a gradient (`--fit`, now part of the tool). **`k = 1` would mean the
reference moves content exactly as far as the shader does. It measures 0.70–0.83.** The reference
over-displaces by roughly a third, consistently, and correlation 0.87–0.95 says the direction is mostly
right. That is a systematic error, not noise, and it is the reason this gate does not pass.

## 4 · Five explanations tested and RULED OUT, each by a number

| Hypothesis | Test | Result |
|---|---|---|
| the record's MV is not the MV the shader read | snapshot `hostMV[gen]` at the `wap_upload` call, dump it beside the late read, difference them | **identical on 100.00% of texels, max delta 0.0000 px, all 6 triples** — the record is faithful |
| GPU sub-texel filter quantization | re-run with the bilinear fraction quantized to 8 and 6 bits (Vulkan `subTexelPrecisionBits`) | exact match **drops** 99.87 → 99.75 → 99.61; the hard disagreements stay at ~788. Quantizing makes it worse, so it is not the cause |
| one of the MV refinement stages | ablate each of guided / inertia / bg_reclaim / ambiguity / phase anchor / vblend and refit | `k` moves from 0.738 to 0.736–0.741, correlation 0.869–0.873. **No stage accounts for it** |
| the ambiguity rule silently damping the field | count where it fires | **8 blocks of 14,400 (0.06%)**; the runner-up sits a median 0.85 px from the primary, and the rule needs 2 px |
| `bg_reclaim` damping the background | evaluate its own gate | `nonconf = smoothstep(2, 6, ‖mv − model‖)` is **0** on the background, where `‖mv − model‖ < 2 px`. It cannot damp there |

A sixth hypothesis, added after the first five: **quantizing the normalized sampling coordinate** (the
model in which the texture unit carries a coarse fixed-point coordinate rather than a coarse sub-texel
fraction). Sweeping 16 down to 10 bits raises exact match to a peak of 88.33% at 12 bits, but `k` falls
to 0.573 and correlation to 0.767 — **both worse**. It improves the count by suppressing displacement
everywhere, not by matching the shader. Refuted.

## 4b · What the error actually is: a sub-pixel DEADZONE, not a scale

`k = 0.70` averaged over a frame hides the shape of the error. Binning the same fit by the size of the
displacement the reference applies makes it unmistakable:

| displacement the reference applies | pixels | `k` | corr | mean change the GPU made | mean change the reference made |
|---|---|---|---|---|---|
| 0.00 – 0.10 px | 27,779 | 0.014 | 0.12 | **0.03** | 4.01 |
| 0.10 – 0.20 px | 41,804 | 0.023 | 0.04 | 0.07 | 2.10 |
| 0.20 – 0.30 px | 32,427 | 0.042 | 0.09 | 0.08 | 0.91 |
| 0.30 – 0.40 px | 21,399 | 0.076 | 0.18 | 0.08 | 1.67 |
| 0.40 – 0.50 px | 15,287 | 0.137 | 0.31 | 0.09 | 1.57 |
| 1 – 2 px | 278 | 0.688 | 0.78 | 23.42 | 28.13 |
| 2 – 4 px | 458 | 0.877 | 0.94 | 42.64 | 46.91 |
| over 4 px | 478 | **0.955** | **0.97** | 60.80 | 62.49 |

**Above 1 px the reference is right** — `k` climbs to 0.955 and correlation to 0.97 on the moving object.
**Below 0.5 px the shader moves essentially nothing** (mean change 0.03 to 0.09 of 255) while the
reference moves 0.9 to 4.0. The whole error lives in the sub-pixel regime, and the frame-wide `k = 0.70`
is just the average of those two populations weighted by how many pixels each holds.

That reframes what remains. The reference's handling of the MV chain and the taps is validated where the
motion is real. What is missing is whatever makes the shader ignore sub-pixel vectors — and nothing in
shader lines 279–522 does that on its face, which is why it is still open.

The likely place to look, recorded for whoever picks this up: the MV field carries a **period-3
sub-pixel pattern** — the values −0.5, +0.1666, 0 repeating in both axes — on blocks whose own `sad_best`
is **0** (a perfect match) for 99.5% of the grid. Three MV blocks of 8 px is exactly the zoo's 24 px
minor grid period, and −0.5 and +1/6 are what a 3-point parabolic sub-pixel fit returns for the
neighbour ratios that periodic content produces. So the matcher's refinement writes a systematic
non-zero vector onto content it simultaneously reports as a perfect zero-motion match. Either the
shader is insensitive to that by a mechanism not yet found, or these vectors never reach it.

## 5 · Honesty ledger

- **The gate does not pass and M4 must not run on this oracle yet.** The error is exactly the class M4
  exists to catch, so an oracle carrying it would launder the very defect it is meant to detect. Note
  the shape though (§4b): the disagreement is confined to sub-pixel displacements, so a future M4 run
  restricted to genuinely moving content would already be meaningful — that narrowing is a decision for
  whoever runs it, and it is not taken here.
- All measurements are on the synthetic ball zoo: a hard-edged, non-antialiased 24 px lattice with one
  gold disc. That content is unusually hostile to a block matcher (periodic, aperture-ambiguous) and
  unusually kind to an exact-match count (91.7% of pixels are one flat colour). Neither number should be
  read as a general figure.
- `--fit`'s gradient mask (`|∇| > 20`) and the 8-pixel disagreement threshold are chosen, not derived.
- The reference was validated against the shader's TEXT, not against a second implementation. A shared
  misreading of the shader would not show up in any number here.
- Not measured: whether the deadzone survives on non-periodic content (the zoo's lattice is what makes
  the matcher emit sub-pixel noise in the first place, so a scene without it might never enter the
  regime where the reference and the shader disagree); whether `k` depends on `t` (the per-triple
  spread, 0.447 to 0.797, hints that it might and nothing here settles it). The 0.5–1.0 px band is
  nearly EMPTY in this content (97 and 67 pixels), so the deadzone's edge is bracketed, not located.
- The `mv0` snapshot added for the diagnosis is kept in the dump. It costs one 57,600-byte plane per
  triple and it is the only thing that can distinguish a stale record from a wrong reference, which is a
  question worth being able to answer twice.

*Made with my soul - Swately <3*

---

## 6 · ANSWERED (2026-09-04): the deadzone was the test content

The record above left one question open — *"Either the shader is insensitive to that by a mechanism not
yet found, or these vectors never reach it."* It is now measured, and neither branch is what happened:
**the vectors themselves were an artefact of the lattice.**

### 6.1 · The experiment

`tools/ball_zoo.ps1` gained `-BgClass noise`: an aperiodic value-noise field of comparable contrast,
inside the *same* harness — same pacing loop, same ball, same window, same capture path — so a
difference can be attributed to the background and to nothing else. Then the identical S2.T6
measurement: `--st-no-stasis` (the bare `result = B_samp` store, which strips the 99 % stasis copy that
flatters any reference), `--qdump` 16 triples, `ref_warp.py --fit`. Two runs per number (DI-3).

### 6.2 · The mechanism, measured

| First triple of a run | LATTICE (`-BgClass grid`) | APERIODIC (`-BgClass noise`) |
|---|---|---|
| median \|mv\| over the grid | **0.5000 px** | **0.0341 px** |
| MV texels below 0.05 px | 11.8 % | **80.6 %** |
| `sad_best == 0` (a perfect match claimed) | 99.6 % | 99.5 % |
| median \|mv\| on stasis blocks (`sad_zero ≤ 0.5`) | **0.5000 px** | **0.0340 px** |

**The matcher claims a perfect match on ~99.5 % of blocks in BOTH cases, and on the lattice it
nonetheless emits a half-pixel vector there.** Fifteen times more spurious sub-pixel motion, driven by
nothing but the background's periodicity. The period-3 pattern (−0.5, +0.1666, 0) recorded in §4b is
that artefact: a 3-point parabolic sub-pixel fit on content whose SAD minimum repeats every 24 px.

### 6.3 · The deadzone does not survive

Binned exactly as §4b, same tool, same mask:

| displacement | LATTICE `n` | LATTICE `k` | APERIODIC `n` | APERIODIC `k` |
|---|---|---|---|---|
| 0.00 – 0.10 px | 27,779 | 0.014 | **39** | — |
| 0.10 – 0.20 px | 41,804 | 0.023 | **3** | — |
| 0.20 – 0.30 px | 32,427 | 0.042 | **8** | — |
| 0.30 – 0.40 px | 21,399 | 0.076 | **11** | — |
| 0.40 – 0.50 px | 15,287 | 0.137 | **6** | — |
| 2 – 4 px | 458 | 0.877 | 379 | **0.873** (corr 0.961) |
| over 4 px | 478 | 0.955 | 498 | **0.956** (corr 0.987) |

**The deadzone population — 138,696 sub-pixel gradient pixels on the lattice — is 67 pixels on
aperiodic content.** There is no deadzone because there is nothing in the deadzone: where nothing
moves, the matcher now reports that nothing moves, and the reference has nothing to over-apply.

### 6.4 · What the whole-frame numbers do

| | exact match | `k` | corr |
|---|---|---|---|
| lattice, bare B-track (§3) | 84.39 % | 0.702 | 0.871 |
| **aperiodic, bare B-track** — runs c / d | **99.02 % / 99.09 %** | **0.889 / 0.903** | **0.977 / 0.975** |
| aperiodic, smaller ball — runs a / b | 99.55 % / 99.55 % | 0.829 / 0.800 | 0.968 / 0.954 |

DI-3 spread on the strengthened pair: `k` 0.889 vs 0.903 (**0.014**), corr 0.977 vs 0.975, exact
99.02 % vs 99.09 %.

### 6.5 · Verdict, and what is still open

**The blocking pathology is closed.** It was the test content, not the shader: the shipping default is
not insensitive to sub-pixel motion — it was declining to move content that the matcher, confused by a
periodic background, wrongly claimed had moved. The fear this record raised — that the deadzone might
be a motion-fidelity defect in the product — **is not confirmed**.

**The gate still does not formally pass**, and the reason is now a different and much smaller one:
`k` reaches **0.956 above 4 px** and 0.873 in the 2–4 px band, against the 1.000 the gate asks for. A
residual 5–13 % over-displacement remains **on genuinely moving content**, where the population is real
and the correlation is 0.987. That is a precision question, not a pathology, and it is what the next
round of work on this oracle should attack.

**Consequence for R3.** M4's corpus MUST be aperiodic. The `--qdump+` records taken on the 24 px
lattice describe a matcher failure mode, not the core's behaviour, and using them would test `fg_core`
against a confound. The narrowed-M4 option left undecided in §4b is also much stronger now: restricted
to moving content on an aperiodic background, the oracle sits at k = 0.956 / corr 0.987.

### 6.6 · Honesty ledger for this section

- The aperiodic background is value noise built from two random lattices upscaled bicubically. That it
  is *aperiodic enough* is a design argument backed by the measured collapse in spurious MV — **its
  spectrum was not computed.**
- The moving-content bins hold 379 and 498 pixels. That is enough to read `k` to two digits and not
  enough to resolve its dependence on `t`; the per-triple `k` still ranges 0.644–0.994.
- Only the ball moves. A scene with several objects, or with a moving background, is untested — and
  the lattice's own failure mode is a reminder that content class changes the answer.
- The lattice disagreement itself is **still unexplained**. This section shows the population that
  exhibited it does not occur on aperiodic content; it does not show why the shader ignored those
  vectors when they did occur. That question is now confined to a content class M4 will avoid, which
  is why it is no longer blocking — not because it was answered.
- `-BgClass grid` remains the default and is byte-identical to before the option existed.

