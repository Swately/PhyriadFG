#!/usr/bin/env python3
"""aa_truth A1 — the anti-aliasing instrument.

WHY THE OBVIOUS MEASURES ARE THE WRONG ONES
-------------------------------------------
PSNR, SSIM and warping error all REWARD BLUR: against a smooth reference, a Gaussian blur scores
better than the aliased input, because blur and correct AA both move the image toward the reference
on an L2 or structural measure and neither separates them. This repository owns the precedent — a
configuration that improved warping error 18.34 -> 12.96 while collapsing Sobel edge magnitude by
60 %: a metric win that was a quality loss.

The usual patch is "add a sharpness term". Not sufficient either, and this tool demonstrates why on
its own corpus: measured on TRANSITION WIDTH ALONE, THE ALIASED ARM WINS — a staircase is not blurry,
it is sharp and wrong. So width is kept here as a blur VETO and never as a score.

A third trap cost this tool two rewrites and is the one worth reading. Version one binned every pixel
of a bar by its distance to the true line and read the scatter inside each bin: on a 960-px bar that
pools ~1000 pixels per bin, which AVERAGES THE STAIRCASE AWAY. The measurement was itself an
anti-aliaser, and it duly reported a scatter ratio of 1.01x on a corpus whose two arms are exact and
point-sampled. Version two cut each bar into chunks and solved each chunk's mean intensity for the
crossing — still wrong, and wrong more instructively: a point-sampled mask IS the set {s < 0}, so the
area it lights inside any window defined by s is exactly correct, and no area estimator POOLED ACROSS
SCANLINES can ever see the error. ALIASING IS NOT AN ERROR IN HOW MANY PIXELS ARE LIT. IT IS AN ERROR
IN WHICH ONES — which is visible only one scanline at a time.

WHAT THIS TOOL MEASURES
-----------------------
Every bar is walked ONE SCANLINE AT A TIME along the dominant axis (columns for a shallow edge, rows
for a steep one — the image is transposed and the normal components swapped, which leaves the s field
identical). On each scanline the edge's sub-pixel position is recovered from that scanline's OWN
integral, which is exact for a resolved edge and quantized for a point-sampled one:

    with samples at spacing D = |ny| in signed distance, normalized n = 1 inside -> 0 outside,
        offset = s_first - D/2 + D * SUM(n)

For the EXACT-COVERAGE arm this is exact — the sum of a symmetric coverage ramp sampled at spacing D
is exactly linear in the crossing position, so the reference reads ~0. For a POINT-SAMPLED arm SUM(n)
is an integer, so the offset snaps to a lattice and spreads uniformly over one spacing: predicted std
D/sqrt(12) = 0.289 px at 1 degree, 0.204 px at 45. THAT CLOSED FORM IS THE TOOL'S OWN ACCEPTANCE
CHECK, and --gate asserts it rather than merely asserting "worse than the reference".

Nothing is resampled anywhere. Interpolating the image would anti-alias the very thing being
measured, and A and B (inside/outside levels) are re-read per scanline so a varying background is
tracked rather than assumed away.

    wobble_px   std of the offset ACROSS SCANLINES in a frame -> THE STAIRCASE, in pixels
    crawl_px    std of the offset ACROSS FRAMES on a scanline -> temporal edge crawl
    centre_px   mean offset against the ANALYTIC line          -> did the edge MOVE?
    scatter     RMS along-edge intensity variation in a fixed-distance bin
    width_1090  10-90 % transition width, px                   -> BLUR VETO, never a score
    band_err    RMS |arm - reference| within +/-3 px of the line

`centre_px` exists because a filter can smooth an edge into the right shape in the WRONG PLACE.
Nothing that compares two renders to each other can see that; it is visible here only because the
truth is analytic and the corpus drifts sub-pixel every frame.

WHY THIS IS NOT GAMEABLE BY BLUR, which is the whole point
-----------------------------------------------------------
A 1-D blur ALONG THE NORMAL preserves each scanline's integral, so it does NOT lower wobble — while
it does widen the transition, which the width veto catches. A blur ALONG THE EDGE mixes neighbouring
scanlines' integrals, which DOES lower wobble — and that is not cheating, it is exactly the operation
correct anti-aliasing must perform on a shallow edge. An isotropic Gaussian gets partial credit for
its tangential component and pays the width veto for its normal component. The measure therefore
rewards the right operation and only the right operation.

THE TWO REGIMES, a property of aliasing and not a defect of the tool
--------------------------------------------------------------------
The staircase period is 1/tan(theta) px, so the corpus's angle ladder spans 57.3, 11.4, 3.7, 1.0 px.
At SHALLOW angles the error is POSITIONAL and `wobble_px` is the measure. At STEEP angles the
positional error falls to one sub-sample step and what remains is INTENSITY error — each pixel 0 or 1
instead of its true coverage — where `scatter` is the measure. Neither covers both ends; read both.

THE NOISE FLOOR, NAMED
----------------------
`aa_zoo --bg noise` lays the bars over value noise of amplitude 0.18. It varies along the bar, so it
contributes intensity `scatter` that is NOT the staircase, and it perturbs each scanline's integral,
so it puts a floor under `wobble` too — MEASURED at 0.30 px at contrast 0.25 and 0.10 px at contrast
0.95, against a flat-background floor of 0.012 px. At low contrast that floor is the same size as the
staircase, so the positional measure carries no verdict there. The floor is identical in both arms
(same seed, same background), so it biases every ratio TOWARD 1.0 — conservative, never generous, and
the gate is written to compare the two arms' floors rather than to pretend either is absent. This
tool is meant to be run on BOTH corpora: the flat one reads the staircase, the noisy one says how
much of that reading survives contact with content.

DI-3. Pass --run twice over corpora differing in BOTH seed and drift phase. Individual scanline
offsets are NOT comparable across such a pair (different sub-pixel phase = a different quantity), so
the run-to-run Pearson is taken the way motion_report takes it: over the CELLS, correlating each
class's aggregate between the two runs. Per-class reliability is reported as the relative discrepancy
between runs; a class above DEV_MAX carries no verdict.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, io, glob
import numpy as np
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

HALF_PX = 3.0         # how far along the normal each scanline is walked, px
REACH_PX = 6.0        # profile reach for the width / scatter probe
BIN_PX = 0.25
MIN_N = 24
DEV_MAX = 0.20        # per-class run-to-run relative discrepancy above which no verdict may be cited


def load_gray(path, W, H):
    a = np.fromfile(path, np.uint8)
    if a.size != W * H * 4:
        raise SystemExit('%s: %d bytes, expected %d' % (path, a.size, W * H * 4))
    return (a.reshape(H, W, 4)[:, :, :3].astype(np.float32) / 255.0) @ np.float32([0.299, 0.587, 0.114])


def edge_lines(e, t_px):
    """The two analytic lines bounding this bar this frame, in aa_zoo's own convention.

    aa_zoo renders bar_mask(angle, offset0 + t, width) as near - far with near = f(-(off + w/2)) and
    far = f(-(off - w/2)), both on the negative side. Those same (nx, ny, d) triples are reproduced
    here rather than re-derived, so the instrument cannot drift away from the generator.
    """
    th = np.deg2rad(e['angle_deg'])
    nx, ny = float(np.sin(th)), float(-np.cos(th))
    off = e['offset0_px'] + t_px
    return [(nx, ny, -(off - e['width_px'] * 0.5)), (nx, ny, -(off + e['width_px'] * 0.5))]


def signed_distance(nx, ny, d, W, H):
    """Exact signed distance from every pixel centre to the analytic line."""
    ys, xs = np.mgrid[0:H, 0:W]
    return (nx * (xs + 0.5) + ny * (ys + 0.5) + d).astype(np.float32)


def local_offsets(g, nx, ny, d, others=()):
    """Sub-pixel edge offset from the analytic line, measured independently on every scanline.

    See the module docstring for why this is an integral and not a level crossing: a two-point
    interpolation across the kink at the end of a coverage ramp biases the EXACT arm by ~0.08 px of
    sawtooth, which showed up as a reference that appeared to wobble 0.17 px when the truth is zero.
    The integral has no such bias — it is exact for the reference by construction.
    """
    H, W = g.shape
    swap = abs(nx) > abs(ny)
    if swap:                                    # scan rows instead of columns; s is unchanged by this
        g = np.ascontiguousarray(g.T)
        nx, ny = ny, nx
        H, W = W, H
    D = abs(ny)                                  # sample spacing in signed distance along the scan
    K = int(np.ceil(HALF_PX / D)) + 1
    cols = np.arange(W)
    r0 = np.rint((-(nx * (cols + 0.5) + d)) / ny - 0.5).astype(np.int64)
    rows = r0[None, :] + np.arange(-K, K + 1)[:, None]
    valid = (rows >= 0) & (rows < H)
    rc = np.clip(rows, 0, H - 1)
    s = (nx * (cols[None, :] + 0.5) + ny * (rc + 0.5) + d).astype(np.float64)
    v = g[rc, cols[None, :]].astype(np.float64)

    # EXCLUDE SCANLINES ANOTHER BAR REACHES INTO. The corpus places bars at four angles, so they
    # necessarily CROSS, and near a crossing a scanline can show a clean monotone step that belongs
    # to the wrong edge -- which no purely local test can catch. The geometry is known analytically,
    # so the exclusion is exact rather than heuristic: reject the scanline if ANY window pixel falls
    # within another bar's band plus one pixel of coverage ramp.
    clear = np.ones(W, bool)
    for (onx, ony, oc, ow) in others:
        if swap:
            onx, ony = ony, onx
        sj = onx * (cols[None, :] + 0.5) + ony * (rc + 0.5) + oc
        clear &= ~((np.abs(sj) <= ow * 0.5 + 1.0) & valid).any(axis=0)
    order = np.argsort(np.where(valid, s, np.inf), axis=0)         # ascending in s
    s, v = np.take_along_axis(s, order, 0), np.take_along_axis(v, order, 0)
    # A second, weaker filter, kept because it costs nothing and catches contamination the geometric
    # exclusion above cannot know about (a filter's own ringing, say): a clean step is monotone, so
    # require the rise and the fall to be lopsided -- one carries the edge, the other is quantization.
    dv = np.diff(v, axis=0)
    up, dn = np.clip(dv, 0, None).sum(axis=0), np.clip(-dv, 0, None).sum(axis=0)
    tv_hi, tv_lo = np.maximum(up, dn), np.minimum(up, dn)
    A, B = v[:2].mean(axis=0), v[-2:].mean(axis=0)                 # inside / outside, per scanline
    span = A - B
    n = np.clip((v - B) / np.where(np.abs(span) > 1e-9, span, 1.0), 0.0, 1.0)
    off = s[0] - D * 0.5 + D * n.sum(axis=0)
    good = (valid.all(axis=0) & clear & (np.abs(span) > 0.05)      # a real, uncontaminated edge
            & (tv_hi > 0.05) & (tv_lo <= 0.08 * tv_hi)             # one monotone step, not two
            & (n[0] > 0.75) & (n[-1] < 0.25)                       # the window brackets the whole edge
            & (np.abs(off) <= HALF_PX))
    # THE CLOSED FORM, per scanline. The samples on scanline c lie on the lattice {beta + D*Z}, and a
    # point-sampled arm makes SUM(n) an integer, so the estimator returns the smallest lattice point
    # at or above zero, minus D/2. This predicts EVERY INDIVIDUAL MEASUREMENT of the aliased arm --
    # not merely its spread -- and it needs no assumption about how the phase is distributed, which
    # matters because at exactly 45 degrees the phase does not advance between scanlines at all.
    beta = nx * (cols + 0.5) + ny * 0.5 + d
    pred = np.mod(beta, D) - D * 0.5
    return cols[good], off[good], pred[good]


def profile_stats(g, s):
    """Whole-bar profile: the 10-90 % transition width and the in-transition intensity scatter."""
    m = np.abs(s) <= REACH_PX
    if int(m.sum()) < MIN_N * 8:
        return None
    sv, gv = s[m].astype(np.float64), g[m].astype(np.float64)
    nb = int(2 * REACH_PX / BIN_PX)
    idx = np.clip(((sv + REACH_PX) / BIN_PX).astype(np.int32), 0, nb - 1)
    cnt = np.bincount(idx, minlength=nb).astype(np.float64)
    sm = np.bincount(idx, weights=gv, minlength=nb)
    sq = np.bincount(idx, weights=gv ** 2, minlength=nb)
    ok = cnt >= MIN_N
    if int(ok.sum()) < 16:
        return None
    cen, mean = ((np.arange(nb) + 0.5) * BIN_PX - REACH_PX)[ok], sm[ok] / cnt[ok]
    sca = np.sqrt(np.maximum(sq[ok] / cnt[ok] - mean ** 2, 0.0))
    lo, hi = float(mean.min()), float(mean.max())
    if hi - lo < 0.05:
        return None
    nrm = (mean - lo) / (hi - lo)
    if nrm[0] > nrm[-1]:
        cen, nrm, sca = -cen[::-1], nrm[::-1], sca[::-1]

    def crossing(level):
        i = int(np.argmax(nrm >= level))
        if i == 0:
            return float(cen[0])
        x0, x1, y0, y1 = cen[i - 1], cen[i], nrm[i - 1], nrm[i]
        return float(x0 + (level - y0) * (x1 - x0) / max(y1 - y0, 1e-9))

    c10, c90 = crossing(0.10), crossing(0.90)
    band = (cen >= c10 - 1.0) & (cen <= c90 + 1.0)
    if not band.any():
        return None
    return {'width_1090': float(c90 - c10), 'scatter': float(sca[band].mean())}


def band_err(g, gref, s, half=3.0):
    m = np.abs(s) <= half
    return float(np.sqrt(np.mean((g[m].astype(np.float64) - gref[m].astype(np.float64)) ** 2))) \
        if m.any() else float('nan')


def run_one(d, arm, ref_arm, frames):
    """Measure `arm` and `ref_arm` frame by frame. Rows are keyed (angle, contrast, line)."""
    t = json.load(open(os.path.join(d, 'truth.json')))
    W, H = t['width'], t['height']
    fa = sorted(glob.glob(os.path.join(d, arm, 'f_*.rgba')))[:frames]
    fr = sorted(glob.glob(os.path.join(d, ref_arm, 'f_*.rgba')))[:frames]
    if len(fa) != len(fr):
        raise SystemExit('%s: %d %s frames vs %d %s frames' % (d, len(fa), arm, len(fr), ref_arm))
    if not fa:
        raise SystemExit('%s: no %s frames found' % (d, arm))
    cent, prof, resid = {}, {}, {}
    for k, (pa, pr) in enumerate(zip(fa, fr)):
        ga, gr = load_gray(pa, W, H), load_gray(pr, W, H)
        tp = t['per_frame_offset_px'][k]
        others = {}                       # every OTHER bar's centreline, for the crossing exclusion
        for e in t['edges']:
            others[id(e)] = [(float(np.sin(np.deg2rad(o['angle_deg']))),
                              float(-np.cos(np.deg2rad(o['angle_deg']))),
                              -(o['offset0_px'] + tp), o['width_px'])
                             for o in t['edges'] if o is not e]
        for e in t['edges']:
            for li, (nx, ny, dd) in enumerate(edge_lines(e, tp)):
                key = (e['angle_deg'], e['contrast'], li)
                for side, img in (('a', ga), ('r', gr)):
                    ci, cv, cp = local_offsets(img, nx, ny, dd, others[id(e)])
                    tgt = cv if side == 'r' else cv - cp     # the aliased arm is scored on its
                    resid.setdefault((key, side), []).extend(tgt.tolist())   # residual vs the model
                    for i, val in zip(ci.tolist(), cv.tolist()):
                        cent.setdefault((key, side), {}).setdefault(i, {})[k] = val
                s = signed_distance(nx, ny, dd, W, H)
                pa_, pr_ = profile_stats(ga, s), profile_stats(gr, s)
                if pa_ and pr_:
                    pa_['band_err'] = band_err(ga, gr, s)
                    prof.setdefault(key, []).append((k, pa_, pr_))
    return cent, prof, resid, t


def _wobble_crawl(lines):
    """lines = {scanline: {frame: offset}} -> (wobble, crawl, centre, n_scanlines)."""
    fr = sorted(set(f for v in lines.values() for f in v))
    per_frame = [[lines[c][f] for c in lines if f in lines[c]] for f in fr]
    wob = [float(np.std(x)) for x in per_frame if len(x) >= 8]
    crw = [float(np.std(list(v.values()))) for v in lines.values() if len(v) >= 4]
    allv = [x for v in lines.values() for x in v.values()]
    return (float(np.mean(wob)) if wob else float('nan'),
            float(np.mean(crw)) if crw else float('nan'),
            float(np.mean(allv)) if allv else float('nan'), len(lines))


def summarize(cent, prof, resid):
    out = {}
    for ang, con in sorted(set((k[0][0], k[0][1]) for k in cent)):
        row = {}
        for side, sfx in (('a', ''), ('r', '_ref')):
            merged = {}
            for (key, sd), ch in cent.items():
                if sd == side and key[0] == ang and key[1] == con:
                    for ci, fv in ch.items():
                        merged.setdefault((key[2], ci), {}).update(fv)
            w, c, m, n = _wobble_crawl(merged)
            row['wobble' + sfx], row['crawl' + sfx], row['centre' + sfx], row['lines' + sfx] = w, c, m, n
        for side, sfx in (('a', ''), ('r', '_ref')):
            rr = [x for (key, sd), v in resid.items()
                  if sd == side and key[0] == ang and key[1] == con for x in v]
            row['model_rms' + sfx] = float(np.sqrt(np.mean(np.square(rr)))) if rr else float('nan')
        rows = [r for k, v in prof.items() if k[0] == ang and k[1] == con for r in v]
        for f, src, dst in (('scatter', 1, 'scatter'), ('scatter', 2, 'scatter_ref'),
                            ('width_1090', 1, 'width'), ('width_1090', 2, 'width_ref'),
                            ('band_err', 1, 'band_err')):
            row[dst] = float(np.mean([r[src][f] for r in rows])) if rows else float('nan')
        out[(ang, con)] = row
    return out


def predicted_wobble(angle_deg):
    """D/sqrt(12), the spread a point-sampled edge has IF the lattice phase is uniform over the
    scanlines. Kept only as a reference column: at exactly 45 degrees the phase does not advance
    between scanlines at all, so the true prediction is the per-scanline model, not this."""
    th = np.deg2rad(angle_deg)
    return float(max(abs(np.sin(th)), abs(np.cos(th))) / np.sqrt(12.0))


def cross_run(SA, SB, field):
    """motion_report-style DI-3: correlate the metric across CELLS between the two runs."""
    ks = sorted(set(SA) & set(SB))
    x = np.array([SA[k][field] for k in ks]); y = np.array([SB[k][field] for k in ks])
    m = np.isfinite(x) & np.isfinite(y)
    r = float('nan') if m.sum() < 3 or x[m].std() < 1e-12 or y[m].std() < 1e-12 \
        else float(np.corrcoef(x[m], y[m])[0, 1])
    dev = {k: (abs(SA[k][field] - SB[k][field]) / max(abs(SA[k][field] + SB[k][field]) * 0.5, 1e-12))
           for k in ks}
    return r, dev


def table(tag, S, dev, arm, ref):
    L = ['## %s' % tag, '',
         '| angle | period px | contrast | scanlines | **wobble %s** | predicted | wobble %s '
         '| **ratio** | model resid %s | model resid %s | crawl %s | crawl %s | scatter %s | scatter %s | width %s | width %s '
         '| centre err | band err | run dev |'
         % (arm, ref, arm, ref, arm, ref, arm, ref, arm, ref),
         '|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|']
    for k in sorted(S):
        s = S[k]
        dv = dev.get(k, float('nan')) if dev else float('nan')
        L.append('| %.0f° | %.1f | %.2f | %d | **%.4f** | %.4f | %.4f | **%.1fx** | **%.4f** | %.4f '
                 '| %.4f | %.4f | %.5f | %.5f | %.3f | %.3f | %+.4f | %.5f | %s |'
                 % (k[0], 1.0 / max(np.tan(np.deg2rad(k[0])), 1e-9), k[1], s['lines'],
                    s['wobble'], predicted_wobble(k[0]), s['wobble_ref'],
                    s['wobble'] / max(s['wobble_ref'], 1e-9),
                    s['model_rms'], s['model_rms_ref'], s['crawl'], s['crawl_ref'],
                    s['scatter'], s['scatter_ref'], s['width'], s['width_ref'],
                    s['centre'], s['band_err'],
                    ('%.1f%%%s' % (100 * dv, '' if dv <= DEV_MAX else ' NO')) if dv == dv else '—'))
    ks = sorted(S)
    rat = lambda f: float(np.nanmean([S[k][f] / max(S[k][f + '_ref'], 1e-9) for k in ks]))
    wr, cr, sr, ww = rat('wobble'), rat('crawl'), rat('scatter'), rat('width')
    pe = float(np.nanmax([S[k]['model_rms'] for k in ks]))          # worst class, not the average
    L += ['', '- mean **wobble** ratio %s/%s: **%.1fx**  (the staircase, in pixels)' % (arm, ref, wr),
          '- mean **crawl** ratio %s/%s: **%.1fx**  (temporal edge motion)' % (arm, ref, cr),
          '- mean **scatter** ratio %s/%s: **%.2fx**  (intensity; the steep-angle measure)' % (arm, ref, sr),
          '- mean **width** ratio %s/%s: **%.2fx**%s' % (arm, ref, ww,
            '  — BELOW 1: on transition width alone the aliased arm reads as the SHARPER image. '
            'That is the measure behaving correctly, and it is exactly why width is a veto and not '
            'a score.' if ww < 1.0 else ''),
          '- **worst-class RMS of (measured - closed-form model) on the %s arm: %.5f px**, against '
          'a floor of %.5f px, which is what the %s arm itself sits at relative to analytic truth. '
          'The model predicts EVERY INDIVIDUAL SCANLINE, so this is the instrument verifying itself '
          'rather than agreeing with a spread; the floor is the corpus\'s, not the estimator\'s.'
          % (arm, pe, float(np.nanmax([S[k]['model_rms_ref'] for k in ks])), ref)]
    return L, wr, cr, sr, ww, pe


def arm_totals(runs_dirs, arm, against, frames):
    """One arm's aggregate over every corpus given, plus the per-corpus spread for DI-3."""
    per = []
    for d in runs_dirs:
        c, p, rs, _ = run_one(d, arm, against, frames)
        S = summarize(c, p, rs)
        ks = sorted(S)
        f = lambda k: float(np.nanmean([S[j][k] for j in ks]))
        rat = lambda k: float(np.nanmean([S[j][k] / max(S[j][k + '_ref'], 1e-9) for j in ks]))
        per.append({'wobble': rat('wobble'), 'crawl': rat('crawl'), 'width': rat('width'),
                    'scatter': rat('scatter'), 'band_err': f('band_err'),
                    'centre': float(np.nanmax([abs(S[j]['centre']) for j in ks]))})
    out = {k: float(np.mean([p[k] for p in per])) for k in per[0]}
    out['dev'] = {k: (abs(per[0][k] - per[-1][k]) / max(abs(per[0][k] + per[-1][k]) * 0.5, 1e-12))
                  for k in per[0]} if len(per) > 1 else None
    return out


def rank(a, arms):
    """Score several candidate arms against one reference and rank them by the conjunctive verdict."""
    T = {}
    for arm in arms:
        T[arm] = arm_totals(a.run, arm, a.against, a.frames)
        print('scored arm %-10s wobble %.1fx  width %.2fx  band_err %.5f'
              % (arm, T[arm]['wobble'], T[arm]['width'], T[arm]['band_err']))
    base = T[arms[0]]['wobble']
    L = ['# %s' % (a.title or 'aa_truth A1 — candidate ranking against %s' % a.against), '',
         '> Every arm is scored against the SAME exact-coverage reference on the same corpus.',
         '> **staircase** is the wobble ratio: 1.00x would mean the arm sits exactly on the analytic',
         '> edge everywhere, and the input arm sets the bar to beat.',
         '> **width** is the blur veto — above ~1.3x the arm bought its score by softening the edge.',
         '> **centre** is the largest mean displacement from the analytic line, px: an arm that',
         '> smooths the edge into the right SHAPE in the wrong PLACE is caught only here.',
         '> **band err** is the L2-style number, shown so the inversion is visible: it can rank an',
         '> arm first while the conjunctive verdict rejects it.', '',
         '| arm | **staircase** (wobble ratio) | vs input | width ratio | scatter ratio '
         '| max centre px | band err | verdict |', '|---|---|---|---|---|---|---|---|']
    for arm in arms:
        t = T[arm]
        ok_w, ok_b, ok_c = t['wobble'] < base * 0.95, t['width'] < 1.30, t['centre'] < 0.05
        v = 'ACCEPT' if (ok_w and ok_b and ok_c) else ' / '.join(
            x for x, c in (('no gain', not ok_w), ('BLURRED', not ok_b), ('MOVED', not ok_c)) if c)
        L.append('| `%s` | **%.1fx** | %+.0f %% | %.2fx | %.2fx | %.4f | %.5f | %s |'
                 % (arm, t['wobble'], 100 * (t['wobble'] / base - 1), t['width'], t['scatter'],
                    t['centre'], t['band_err'], v))
    if T[arms[0]]['dev'] is not None:
        d = max(max(T[arm]['dev'][k] for k in ('wobble', 'width')) for arm in arms)
        L += ['', 'DI-3: worst run-to-run discrepancy across %d corpora, on wobble and width: '
                  '**%.1f %%** (threshold %.0f %%).' % (len(a.run), 100 * d, 100 * DEV_MAX)]
    else:
        L += ['', 'Only one corpus given: DI-3 NOT satisfied, reliability not measured.']
    L += ['', '*Generated by tools/aa_truth/aa_report.py — Made with my soul - Swately <3*']
    print(); print('\n'.join(L))
    if a.md:
        open(a.md, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
        print('\nwrote', a.md)
    return


def main():
    ap = argparse.ArgumentParser(description='aa_truth A1 — the anti-aliasing instrument')
    ap.add_argument('--run', action='append', required=True, metavar='DIR')
    ap.add_argument('--label', action='append', default=None)
    ap.add_argument('--arm', action='append', default=None,
                    help='an arm to score. Repeat it to RANK several candidates.')
    ap.add_argument('--against', default='ref')
    ap.add_argument('--frames', type=int, default=32)
    ap.add_argument('--md')
    ap.add_argument('--title', default=None)
    ap.add_argument('--gate', action='store_true',
                    help='SEE IT RED FIRST. Three conjunctive checks: the scored arm wobbles more '
                         'than the reference, it crawls more, and — the strong one — the '
                         'per-scanline closed form explains the scored arm at least as well as '
                         'analytic truth explains the reference.')
    a = ap.parse_args()

    arms = a.arm or ['alias']
    if len(arms) > 1:
        return rank(a, arms)
    a.arm = arms[0]
    runs, sums = [], []
    for d in a.run:
        c, p, rs, t = run_one(d, a.arm, a.against, a.frames)
        runs.append((c, p)); sums.append(summarize(c, p, rs))
        print('scored %s — %dx%d, %d frames, %d line-instances'
              % (d, t['width'], t['height'], min(a.frames, t['frames']), len(p)))

    r_w, dev = cross_run(sums[0], sums[1], 'wobble') if len(sums) > 1 else (float('nan'), None)
    labels = a.label or [os.path.basename(os.path.normpath(d)) for d in a.run]

    L = ['# %s' % (a.title or 'aa_truth A1 — %s scored against %s' % (a.arm, a.against)), '',
         '> **wobble** = std of the local sub-pixel edge position ACROSS SCANLINES of one bar, px.',
         '> THIS IS THE STAIRCASE, and it is the score at shallow angles. `predicted` is the closed',
         '> form for a point-sampled edge, D/sqrt(12) with D the sample spacing along the scan axis.',
         '> **crawl** = std of that same position ACROSS FRAMES, px. Temporal edge motion.',
         '> **scatter** = RMS along-edge intensity variation in a fixed-distance bin. The score at',
         '> steep angles, where the positional error falls to one sub-sample step.',
         '> **width_1090** = 10-90 % transition width, px. A blur veto, never a score.',
         '> **centre err** = mean position against the ANALYTIC line, px. Did the edge MOVE?',
         '> **band err** = RMS |arm - reference| within ±3 px of the line.',
         '> **run dev** = |run A - run B| / mean, on wobble, over two corpora differing in seed AND',
         '> drift phase. Per DI-3 a class above %.0f %% is UNRELIABLE and carries no verdict (NO).'
         % (100 * DEV_MAX),
         '> **period px** = 1/tan(angle): the axis the two regimes divide on.', '']
    agg = []
    for i, S in enumerate(sums):
        sec = table(labels[i], S, dev, a.arm, a.against)
        L += sec[0] + ['']
        agg.append(sec[1:])
    if len(sums) < 2:
        L += ['> Only one corpus was given: DI-3 is NOT satisfied and every number above is '
              '"reliability not measured".', '']
    else:
        bad = sorted([k for k, v in dev.items() if v > DEV_MAX])
        L += ['### DI-3', '',
              'Cross-run Pearson on per-class wobble, over %d classes: **r = %.3f**.' % (len(dev), r_w),
              'Max per-class discrepancy %.1f %%, median %.1f %%.'
              % (100 * max(dev.values()), 100 * float(np.median(list(dev.values())))),
              ('Every class is within %.0f %%; all numbers above may be cited.' % (100 * DEV_MAX))
              if not bad else 'UNRELIABLE, no verdict may cite these: %s'
              % ', '.join('%.0f°/%.2f (%.0f %%)' % (k[0], k[1], 100 * dev[k]) for k in bad), '']
    L += ['*Generated by tools/aa_truth/aa_report.py — Made with my soul - Swately <3*']
    print(); print('\n'.join(L))
    if a.md:
        open(a.md, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
        print('\nwrote', a.md)

    if a.gate:
        wr, cr, pe = agg[0][0], agg[0][1], agg[0][4]
        S0 = sums[0]
        rr = float(np.nanmax([S0[k]['model_rms_ref'] for k in S0]))
        excess = float(np.nanmax([S0[k]['model_rms'] - S0[k]['model_rms_ref'] for k in S0]))
        checks = [('%s wobbles more than %s: %.1fx > 1' % (a.arm, a.against, wr), wr > 1.0),
                  ('%s crawls more than %s:  %.1fx > 1' % (a.arm, a.against, cr), cr > 1.0),
                  ('closed form explains %s as well as truth explains %s: worst excess %+.5f px '
                   '< 0.02' % (a.arm, a.against, excess), excess < 0.02)]
        print()
        print('The corpus\'s own positional floor (%s arm vs analytic truth, worst class): %.5f px.'
              % (a.against, rr))
        print('On a flat background that is uint8 quantization of the coverage ramp and runs ~0.01 px;'
              ' on a noise background it is the background itself and runs ~0.3 px, which is the same'
              ' size as the staircase and is why the flat corpus exists.')
        print()
        for name, ok in checks:
            print('GATE  %-52s %s' % (name, 'PASS' if ok else 'FAIL'))
        if not all(ok for _, ok in checks):
            sys.exit('the instrument does not read this corpus the way the closed form says it must')


if __name__ == '__main__':
    main()
