# MV Edge-Snap — cross-bilateral (edge-aware) primary-MV fetch

- **Type:** Tier-1 plan (substantial, single-shader change; no crash/concurrency/data-loss risk → no RISK_REGISTER required).
- **Flag:** `--mv-edge-snap {1|2}` (default OFF, byte-identical off). `1` = G1 dissidence-class guidance; `2` = G2 color guidance. Optional `--mes-sim F` overrides the guidance band (else `--mv-sim`, default 0.10).
- **Status:** `measured` — **INCONCLUSIVE on the ball-zoo bench** (see verdict). The code is correct,
  OFF is byte-identical, cost/stability gates pass; the geometry gates cannot be resolved above the
  bench's run-to-run measurement variance. Ships behind the default-OFF flag for the operator's eye.
- **Scope:** `shaders/wap_warp.comp` (the fetch), `src/generate/warp_blend.cpp` (`pcr.size`), `src/present/present.cpp` (encode + push), `src/control/{cli.hpp,cli.cpp}` (flag), `src/core/main.cpp` (banner + WAP guard). No pacing / selection / PLL touched.

## Measured motivation (the convicted defect)

The warp's per-pixel **bilinear** sampling of the 8px-grid MV field dilutes the effective MV at
object silhouettes: a boundary output pixel mixes object-MV with background-MV(≈0), so the rendered
edge advances short/unevenly.

Evidence (ball zoo 15fps, true 22.0 px/frame):
- blend-track slopes A 0.894 / B 0.957; inter-track gap 3.2–4.6 px.
- per-tick edge steps oscillate 0–7 px around the 2.75 ideal, jumps clustered at blend crossover
  t≈0.5–0.65.
- matcher / repair / consensus stages all EXONERATED by per-stage audit (raw cluster 94%, ball CORE
  tiles 98–100%, max 24.9): the magnitude EXISTS in the grid; it dies at SAMPLING.
- the error is ~constant in px → at 120fps (2.75 px/frame) it exceeds the whole step = the operator's
  "base framerate + noise" feel in games. This is the coherent-blur frontier.

## What already exists (not to duplicate)

`mv_guided` (default ON, `guided_mv`, `wap_warp.comp`) fixes **tile-level** selection: at the primary
MV fetch it picks ONE of the 4 bilinear-footprint corners by color membership (`cur_real` at the pixel
vs each corner's block-centre color), committing to the single best corner's MV — unless no corner is
similar or there is a near-tie, in which case it falls FULLY back to plain bilinear. It is a hard
single-corner pick OR full bilinear; it has no weighted combination. The present-side color consensus
on `wapMVA` fixes tile-level outliers. **The gap is the SUB-TILE bilinear MIX at sample time**, which
`mv_guided` cannot address on the large edge-pixel population that lands on its near-tie/degenerate
fallback (→ back to the diluted bilinear).

## Design — cross-bilateral MV sampling (`edge_snap_mv`)

Joint-bilateral upsample of the MV grid. For the 4 bilinear-footprint corners with spatial fractions
`f = fract(uv*G − 0.5)`:

- **spatial weight** `sp_k` = the standard bilinear weight `(1∓f.x)(1∓f.y)` (sums to 1 — LINEAR
  footprint preserved in the flat case).
- **guidance weight** `guide_k = exp(−d_k² / (2σ²))`, gaussian falloff, `σ = sim` (the packed band):
  - **G1 (dissidence, preferred):** `d_k = |dis_pix − dis_corner|` on the R8 dissidence byte
    (binding 6, the prev-anchored object silhouette). Directly separates object vs background class.
  - **G2 (color, joint-bilateral):** `d_k = maxch|c_pix − c_corner|` on `cur_real` (the same
    membership signal `guided_mv` scores). Always available.
- **combine:** `w_k = sp_k · guide_k`; `mv = Σ(w_k·mv_k) / Σ(w_k)`.
- **degenerate guard:** `Σw < 1e-5` (every corner cross-class) → return plain `texture()` bilinear
  (never NaN, never 0/0).

So an object-edge pixel draws its MV continuously from the object-side texels (the background corners,
whose class/color differs, are suppressed) — no hard flip, no seam. When armed, edge-snap SUPERSEDES
`guided_mv`'s hard pick for that pixel (it is the strictly-richer weighted form of the same footprint).
The `inertia_static` GATE (b) fallback (fast-corner refusal) applies to edge-snap exactly as to guided.

**Encoding:** push-constant `mv_edge_snap = variant + sim_band` (variant ∈ {1,2}, sim ∈ [0.02,0.5]);
value in [1.02, 2.5]. Shader recovers `use_dis = (v < 2.0)`, `sim = fract(v)`. Host auto-falls-back
**G1→G2** when the dissidence mask is not valid this generation (`gme_push` false) so the fix still
runs on a color guide instead of a stale/placeholder mask.

**Endpoint guarantee:** edge-snap only chooses the vector `mv`; the A/B taps still scale it by `t` and
`1−t`, so at t=0 the A tap is `prev[uv]` and at t=1 the B tap is `cur[uv]` byte-exact — unchanged.

**Push-constant safety:** the block grew 220B → 224B (56 floats), `mv_edge_snap` at offset 220. Shader
`PushConsts`, host `pcw` struct, host `sizeof(pcw)` push, and `warp_blend.cpp` `pcr.size` all agree at
224B; device `maxPushConstantsSize` is 256B on both target GPUs.

## Risks

- **Visual regression on non-object content.** Mitigation: gaussian guidance with `σ = sim` reduces to
  ≈plain bilinear on flat/uniform regions (all corners similar → all `guide_k ≈ 1` → spatial-only =
  bilinear). Degenerate guard returns exact bilinear. Default OFF; A/B gated on the ball first.
- **Cost.** 4 extra guidance taps (dissidence or color) + 4 texelFetch vs the guided path's existing 4
  color taps — roughly one extra texture-fetch set per pixel. Gate 4 bounds warp ms delta ≤ +0.5ms.
- **NaN / degenerate.** Mitigation: `Σw < 1e-5` → plain bilinear; `σ` floored at 1e-3.

## Acceptance gates (measured; operator's eye is final)

Bench: `ball_zoo.ps1 -Fps 15 -Bounce` (bounce so the ball never wraps → clean monotone triples),
`--qdump` stride-11 held-out triples, right-edge/centroid ratio-vs-phase. All runs `cap≈source`,
`er=0`, `dd_lost=0`, no device-lost.

| Gate | Target | OFF | ON G2 (best) | Verdict |
|------|--------|-----|--------------|---------|
| 1. 15fps per-tick right-edge steps within ±1px of ideal, no crossover cluster | ±1px whole span | not resolvable | not resolvable | **UNRESOLVED** — outdump/qdump edge steps swamped by AA-edge p98 quantization (±1–2px ≈ ±0.05–0.09 ratio) + unstable 13–17fps source cadence |
| 2. blend-solo A/B track slopes ≥0.97, inter-track gap <1px | slopes≥0.97, gap<1px | A cen 0.881±0.035 / edge 1.049±0.10; B cen 0.914 / edge 0.769 | A cen 0.887±0.004 / edge 1.071; B cen 0.905 / edge 0.763 | **FAIL / INSIDE NOISE** — G2's A-track numbers sit inside the OFF run-to-run band; B-track unchanged; neither reaches 0.97 |
| 3. 120fps displacement ratio materially improves | improve vs OFF | not resolvable | not resolvable | **UNRESOLVED** — at 2.75px/frame the AA-edge quantization is ~50–70% of the whole step; the qdump ratio is pure noise (the same reason the defect "exceeds the whole step") |
| 4. warp ms delta ≤ +0.5ms @ 240Hz | ≤ +0.5ms | 0.31–1.96ms | 0.31–1.73ms (≤ OFF within noise) | **PASS** — no measurable added cost (4 guidance taps + 4 texelFetch; G2 read even slightly lower) |
| 5. one 240fps run clean; OFF byte-identical | clean; identical | ref | 240fps: cap 256/s, er=0, dd_lost=0, no device-lost; 120fps clean; OFF banner has NO mv-edge-snap marker (mes_push=0 → OFF code path) | **PASS** |

### G1 vs G2 (15fps ball, right-edge |resid| — a single run each; see variance caveat)

| Variant | n | \|resid\| mean | resid stdev | note |
|---------|---|---------------|-------------|------|
| OFF | 36 | 0.148 | 0.253 | (a *different* OFF repeat gave 0.073 / n=76 — the between-run swing) |
| G1 (dissidence) | 44 | 0.193 | 0.243 | **worse than OFF** — the ball tracks the global affine model (`gme(dis:1%)`, 0% at 120/240fps), so the dissidence field is near-uniform → G1's guidance term barely discriminates the corners → collapses toward bilinear with extra noise. **G1 needs a meaningful dissidence field to have any signal.** |
| G2 (color) | 59 | 0.083 | 0.096 | best in *this* run, but the OFF repeat matched it → not separable from noise |

**G2 is the correct default when armed** (color is a strong, always-available discriminant; G1 is
signal-less on model-tracking content and auto-falls-back to G2 anyway when the mask is invalid).

### Honest verdict

The fix is **implemented correctly and is cost-free and safe** (gates 4/5 pass, OFF byte-identical),
but on the ball-zoo bench **no geometry improvement survives run-to-run variance**. The blend-solo
A-track centroid slope is nudged toward ideal and made more *consistent* by G2 (0.883–0.891, σ≈0.004
vs OFF's 0.850–0.920 spread), but the means are statistically indistinguishable and the B-track is
unchanged. The measurement floor (AA-edge p98 ±1–2px; clean-triple population varying 2–4× between
identical runs; unstable source cadence) is **larger than the OFF↔ON delta** at 15fps and completely
dominates at 120fps. Per the "partial progress beats nothing, but say so plainly" clause: this is
**not** a clear >50% cluster reduction — it is *no resolvable change*. Shipping behind the default-OFF
flag with these honest numbers; the operator's eye is the remaining judge on real game content, where
the color guide may separate object/background more strongly than the single gold-ball/navy bench does.

### What remains / next diagnostic ideas (not done here)

- A **synthetic, aliasing-free** edge probe (a hard-edged non-AA bar at a known integer speed, dumped
  at a *pinned* phase) would drop the measurement floor below the effect — the current AA ball is the
  limiter. `blend-solo` + a pinned-phase outdump is the path.
- G2 asymmetry (helps A, not B) suggests the guide should also key off `prev_real` for the A-tap and
  `cur_real` for the B-tap separately (currently both use `cur_real` as the membership color); a
  per-tap guide is the obvious refinement if the operator sees the A/B asymmetry on real content.
