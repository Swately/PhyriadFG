# B1 — what the default flip actually did to the translating sphere (11.16 / 11.9)

**Date:** 2026-09-15 · **Corpus** `sc_live/source_k8` at **6.72 px per source pair** on the sphere ·
**Frames** 216, one complete lap of the `fg_k8_ph8` capture · **Instrument** `tools/ref_warp.py`, the
CPU reference that reproduces the shipping composite, with `WS_EDGE0/WS_EDGE1` set per arm ·
**Scorer** `scene_report.score_live` at each frame's own exact phase

---

## 1. Why this instrument and not a live capture

Live runs cannot resolve this. The run-to-run band on the sphere's position at this displacement is
3–11 %, and the effect is of that order, which is why four live A/B rounds across two sessions could
report it, reproduce its sign, and still not isolate it.

Here **both arms are computed from the same triple** — the same two real frames, the same motion
vectors, the same everything the FG produced in one run — and only the two ramp edges differ. Every
source of run-to-run variation is removed by construction, so a difference that survives is the edges
and nothing else. The price is that this is the CPU reference rather than the GPU (99.47 % agreement,
measured at the old edges; see §5).

## 2. The result: it is a trade, not a regression

216 identical frames, `smoothstep(1.2, 3.0, …)` against `smoothstep(1.0, 1.6, …)`:

| object | term | 1.2 / 3.0 | 1.0 / 1.6 | delta | worse in |
|---|---|---|---|---|---|
| sphere (translates) | pos_err px | 0.6814 | 0.7687 | **+12.8 %** | 44 % of frames |
| sphere | halluc px² | 391.0 | 186.2 | **−52.4 %** | 0 % of frames |
| sphere | lead px | 19.16 | 38.72 | +19.56 | 67 % of frames |
| panel (static) | pos_err px | 0.0064 | 0.0009 | −86 % | 2 % of frames |
| box (spins) | pos_err px | 0.2904 | 0.2641 | **−9.0 %** | 33 % of frames |
| box | halluc px² | 125.6 | 100.8 | −19.8 % | 4 % of frames |

The sphere's hallucinated mass halves and its position degrades. **That is one exchange, not two
findings**, and the box gets the benefit with no matching cost because it spins in place.

## 3. Where it happens, and it is not the contour

Distance to the sphere's truth silhouette, signed, in whole pixels. The totals mislead — the far field
is 97 % of the frame — so the per-pixel density is the column that matters:

| band \|d\| | change, total | px/frame | change PER PIXEL | vs far field |
|---|---|---|---|---|
| 1–2 px | 1.20 | 1,043 | 0.001149 | **3.6×** |
| 2–4 px | 2.96 | 2,075 | 0.001428 | **4.5×** |
| 4–8 px | 4.14 | 4,144 | 0.000998 | **3.1×** |
| > 8 px | 70.87 | 223,138 | 0.000318 | 1.0× |

And the ramp's own output in those bands:

| band \|d\| | w_s at 1.2/3.0 | w_s at 1.0/1.6 | factor |
|---|---|---|---|
| 1–2 px | 0.0469 | 0.0855 | ×1.82 |
| 2–4 px | 0.0479 | **0.1670** | **×3.49** |
| 4–8 px | 0.0823 | **0.1957** | **×2.38** |
| > 8 px | 0.8543 | 0.8595 | ×1.01 |

The far field is already saturated near 0.85 under both settings — it is backdrop, and `stasis`
governs it. **The only band the narrowing moves is 2–8 px from the silhouette, and at 6.72 px per pair
that ring is precisely the band the sphere sweeps between A and B.**

## 4. The mechanism, end to end

In the swept band the two displaced samples disagree by construction: one carries sphere, the other
carries backdrop. So `d_pixel` is large there, the ratio is large, and `w_s` is what decides. Narrowing
the edges raises the hold in that ring from ~0.05 to ~0.18, and holding means presenting `cur0`, the
**unwarped** current frame — the sphere where it is at B, not at phase t.

That single action does both things at once:

- it suppresses the trailing ghost at the A position → hallucinated mass **−52 %**
- it biases the silhouette toward B → position **+12.8 %**, and `lead_px` 19.2 → 38.7 px, the
  hallucinated mass moving from straddling the object to sitting ahead of it

A spinning box sweeps no band, so the same hold removes interior damage and costs it no position.

## 5. What this does not claim

- **Not a GPU measurement.** `ref_warp.py` reproduces the GPU at 99.47 %, and that figure was measured
  at the retired edges. It has not been re-run at 1.0 / 1.6.
- **Not a verdict on the trade.** −52 % hallucinated mass against +12.8 % centroid on one object is an
  exchange, and which side is worth more is a judgment about what a viewer sees, not a number this
  page can produce.
- One corpus, one displacement (6.72 px/pair). The sign of the exchange at other displacements is in
  `B1_FG_k4*.md`; the mechanism here was measured only at this one.
- The 0–1 px band holds no pixels under this banding, so the contour itself is not separately resolved.

*Made with my soul - Swately <3*
