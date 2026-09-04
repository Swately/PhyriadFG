# S2.T6 — gate record: the CPU reference warp, built and NOT yet passing · 2026-09-03

> The independent oracle R3's M4 gate rests on. **Verdict: BUILT, PARTIAL — the gate does NOT pass.**
> The reference reproduces 99.88% of the shipping default's stored pixels exactly, but a controlled
> diagnostic shows it moves content about 30% further than the shader does, and that gap is measured,
> not explained. M4 must not run on this oracle until §4 is closed. Every number below is from command
> output; the two failing measurements are stated as prominently as the passing one.

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

A sixth observation, recorded because it will matter to whoever closes this: the MV field carries a
**period-3 sub-pixel pattern** — the values −0.5, +0.1666, 0 repeating in both axes — on blocks whose own
`sad_best` is **0** (a perfect match) for 99.5% of the grid. Three MV blocks of 8 px is exactly the zoo's
24 px minor grid period. So the matcher's sub-pixel refinement is writing a systematic non-zero vector
onto content it simultaneously reports as a perfect zero-motion match. Whether the shader is somehow
insensitive to that, or the field is wrong, is the open question.

## 5 · Honesty ledger

- **The gate does not pass and M4 must not run on this oracle yet.** A 30% displacement error is exactly
  the class of error M4 exists to catch, so an oracle carrying it would launder the very defect it is
  meant to detect.
- All measurements are on the synthetic ball zoo: a hard-edged, non-antialiased 24 px lattice with one
  gold disc. That content is unusually hostile to a block matcher (periodic, aperture-ambiguous) and
  unusually kind to an exact-match count (91.7% of pixels are one flat colour). Neither number should be
  read as a general figure.
- `--fit`'s gradient mask (`|∇| > 20`) and the 8-pixel disagreement threshold are chosen, not derived.
- The reference was validated against the shader's TEXT, not against a second implementation. A shared
  misreading of the shader would not show up in any number here.
- Not measured: whether the residual behaves differently on non-periodic content; whether `k` depends on
  `t` (the per-triple spread, 0.447 to 0.797, hints that it might and nothing here settles it).
- The `mv0` snapshot added for the diagnosis is kept in the dump. It costs one 57,600-byte plane per
  triple and it is the only thing that can distinguish a stale record from a wrong reference, which is a
  question worth being able to answer twice.

*Made with my soul - Swately <3*
