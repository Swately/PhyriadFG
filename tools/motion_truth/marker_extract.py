#!/usr/bin/env python3
"""marker_extract.py — where did the FG PUT each marker? (MOTION_TRUTH phase T4)

THE QUESTION IT ANSWERS. For a `--qdump+` triple (prev, live, next) captured from the marker zoo, and
for every marker: where should the marker be at the FG's phase t, and where is it actually? The
answer is a TABLE — `(marker, t, expected, observed, error_px)` — which is what the operator asked for:
"I want data, not images". Nobody looks at a pixel to judge a row.

TWO EXPECTATIONS, DELIBERATELY. The barcode gives k_prev and k_next exactly, so:
    p_model = p(k_prev) + t * (p(k_next) - p(k_prev))     what a correct TRANSLATIONAL warp of THIS
                                                          pair must produce, whatever the true motion
    p_true  = p(t_real),  t_real = (k_prev + t*(k_next-k_prev)) / fps
                                                          the analytic truth at that instant
They coincide for linear motion and diverge for accelerating/circular. That divergence is the MODEL's
error, not the FG's, and it is reported as its own column so the two are never confused.

THE MATCH. Normalised cross-correlation of the marker's own pattern (from the record — the bits live in
trajectories.json, the extractor never re-derives them) over a (2R+1)^2 window centred on p_model. Every
local maximum above 0.6*max and above an absolute floor is reported, each refined to sub-pixel by the
1-D parabolic fit on both axes — the SAME form the block matcher uses
(optical_flow_hier_match.comp:240-256), with the sign for a maximum instead of a minimum, the same
denominator guard and the same +-0.5 clamp. n_peaks >= 2 is the ghost flag (recorded, not scored now);
n_peaks = 0 is a miss.

THE GATE SEEN RED. `--selftest` shifts a real plane by a known (dx, dy) and asserts the extractor
reports it within 0.1 px, and asserts an UNshifted plane reports ~0. On the real prev/next planes the
expected position is p(k) exactly, so the reported error there measures the CAPTURE PATH, not the FG,
and must be <= 0.25 px mean. An extractor that has not been seen to fail cannot be trusted to pass.

    python tools/motion_truth/marker_extract.py --dump DIR --zoo ZOODIR --out detections.csv [--fps 60]
    python tools/motion_truth/marker_extract.py --dump DIR --zoo ZOODIR --selftest

numpy + stdlib only. Made with my soul - Swately <3
"""
import argparse, csv, json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from marker_zoo import decode_barcode, eval_traj, BC_X0, BC_Y0, BC_W, BC_Y1, TOP_BORDER   # noqa: E402


# ── loading ────────────────────────────────────────────────────────────────────────────────────
def load_manifest(path):
    size, rows = None, []
    for ln in open(path, encoding='utf-8', errors='replace'):
        ln = ln.strip()
        if ln.startswith('size '):
            _, w, h = ln.split()[:3]; size = (int(w), int(h)); continue
        if not ln.startswith('triple '):
            continue
        toks = ln.split(); rec = {'id': toks[1]}
        for t in toks[2:]:
            if '=' in t:
                k, v = t.split('=', 1); rec[k] = v
        rows.append(rec)
    return size, rows


def load_plane(path, w, h):
    a = np.fromfile(path, dtype=np.uint8)
    if a.size != w * h * 4:
        raise ValueError(f'{os.path.basename(path)}: {a.size} B, expected {w*h*4}')
    return a.reshape(h, w, 4)


def luma(rgba):
    """Grey float32 in [0,1]. The zoo is grey by construction, so any channel would do; the mean is
    used so a capture path that touched one channel differently is averaged, not amplified."""
    return rgba[..., :3].astype(np.float32).mean(axis=-1) / 255.0


# ── the match ──────────────────────────────────────────────────────────────────────────────────
def ncc_window(img, tpl, cx, cy, R):
    """NCC of `tpl` (s x s, float) at every integer top-left inside a (2R+1)^2 window whose CENTRE
    top-left corresponds to marker centre (cx, cy). Returns (scores, x0, y0): scores[j, i] is the NCC
    with the template's top-left at (x0 + i, y0 + j). Direct correlation on the small window — at
    K=24 markers x 41^2 positions x 12^2 taps it is ~5.8 M multiply-adds per plane, which numpy does
    in well under a second without an FFT."""
    s = tpl.shape[0]
    x0 = int(np.round(cx - s / 2.0)) - R
    y0 = int(np.round(cy - s / 2.0)) - R
    n = 2 * R + 1
    h, w = img.shape
    tz = tpl - tpl.mean()
    tn = np.sqrt((tz * tz).sum())
    scores = np.full((n, n), -2.0, np.float32)
    if tn < 1e-6:
        return scores, x0, y0
    # sliding windows over the region [y0, y0+n+s) x [x0, x0+n+s), edge-clamped by declaring
    # out-of-frame placements invalid (-2) rather than padding with fake content
    for j in range(n):
        yy = y0 + j
        if yy < 0 or yy + s > h:
            continue
        rows_ = img[yy:yy + s]
        for i in range(n):
            xx = x0 + i
            if xx < 0 or xx + s > w:
                continue
            win = rows_[:, xx:xx + s]
            wz = win - win.mean()
            wn = np.sqrt((wz * wz).sum())
            if wn < 1e-6:
                continue
            scores[j, i] = float((wz * tz).sum() / (wn * tn))
    return scores, x0, y0


def local_maxima(scores, floor_abs, floor_rel):
    """All strict local maxima above both floors. Returns [(score, j, i)] sorted by score desc."""
    mx = float(scores.max())
    if mx <= floor_abs:
        return []
    thr = max(floor_abs, floor_rel * mx)
    n = scores.shape[0]
    out = []
    for j in range(1, n - 1):
        for i in range(1, n - 1):
            v = scores[j, i]
            if v < thr:
                continue
            nb = scores[j - 1:j + 2, i - 1:i + 2]
            if v >= nb.max() and (nb < v).sum() >= 7:     # strictly above at least 7 of 8 neighbours
                out.append((float(v), j, i))
    out.sort(reverse=True)
    return out


def parabolic(sm, s0, sp):
    """The matcher's own sub-pixel form (optical_flow_hier_match.comp:250-256), for a MAXIMUM:
    delta = 0.5*(sm - sp)/(sm - 2*s0 + sp). The denominator is the discrete 2nd derivative, NEGATIVE at
    a concave peak; the guard rejects a flat or non-concave sample (keep the integer pick), and the
    clamp bounds the vertex to +-0.5 because a genuine peak's vertex lies within half a pixel of the
    integer winner or a neighbour would have won."""
    den = sm - 2.0 * s0 + sp
    if den > -1e-4:
        return 0.0
    return float(np.clip(0.5 * (sm - sp) / den, -0.5, 0.5))


def splat_template(tpl, fx, fy):
    """The zoo's own rendering of a marker at sub-pixel phase (fx, fy) in [0,1): the same bilinear
    four-weight splat as marker_zoo.splat, producing an (s+1)x(s+1) footprint. Matching THIS against
    the image, instead of the sharp pattern, is what removes the parabola's pixel-locking bias."""
    s = tpl.shape[0]
    acc = np.zeros((s + 1, s + 1), np.float32)
    for dy, wy in ((0, 1.0 - fy), (1, fy)):
        for dx, wx in ((0, 1.0 - fx), (1, fx)):
            ww = wy * wx
            if ww:
                acc[dy:dy + s, dx:dx + s] += tpl * np.float32(ww)
    return acc


def ncc_at(img, tpl, xx, yy):
    """NCC of a template with its top-left at integer (xx, yy); -2 if it does not fit."""
    s0, s1 = tpl.shape
    h, w = img.shape
    if xx < 0 or yy < 0 or xx + s1 > w or yy + s0 > h:
        return -2.0
    win = img[yy:yy + s0, xx:xx + s1]
    wz = win - win.mean(); tz = tpl - tpl.mean()
    wn = np.sqrt((wz * wz).sum()); tn = np.sqrt((tz * tz).sum())
    if wn < 1e-6 or tn < 1e-6:
        return -2.0
    return float((wz * tz).sum() / (wn * tn))


SUBPEL_STEPS = 16     # the phase grid: 1/16 px, so the grid's own residual is 1/32 px before the parabola


def refine_subpel(img, tpl, xx, yy):
    """Sub-pixel refinement by PHASE SEARCH. The integer NCC peak says the marker's top-left is near
    (xx, yy). The true top-left is (xx + fx, yy + fy) for some fx, fy in [-0.5, 0.5); render the
    template at that phase the way the zoo did and find the phase whose rendering matches best. A
    negative phase is a positive phase one pixel earlier, so the search covers [-0.5, 0.5) as
    top-left (xx-1, yy-1) with phases in [0.5, 1.5) folded back into [0, 1)."""
    n = SUBPEL_STEPS
    best = (-3.0, 0.0, 0.0)
    grid = np.zeros((n + 1, n + 1), np.float32)
    for jy in range(n + 1):
        fy = -0.5 + jy / n
        oy, py = (yy - 1, fy + 1.0) if fy < 0 else (yy, fy)
        for ix in range(n + 1):
            fx = -0.5 + ix / n
            ox, px = (xx - 1, fx + 1.0) if fx < 0 else (xx, fx)
            v = ncc_at(img, splat_template(tpl, px, py), ox, oy)
            grid[jy, ix] = v
            if v > best[0]:
                best = (v, fx, fy)
    v, fx, fy = best
    # the parabola on the 1/16 grid takes the last fraction of a step; guarded and clamped as before
    jy = int(round((fy + 0.5) * n)); ix = int(round((fx + 0.5) * n))
    ddx = ddy = 0.0
    if 0 < ix < n:
        ddx = parabolic(grid[jy, ix - 1], grid[jy, ix], grid[jy, ix + 1]) / n
    if 0 < jy < n:
        ddy = parabolic(grid[jy - 1, ix], grid[jy, ix], grid[jy + 1, ix]) / n
    return v, fx + ddx, fy + ddy


def floor_for(size):
    """The plan's 0.5 for a 12 px (100-bit) or larger pattern. A 6 px pattern has a 16-bit interior,
    and at 0.5 a noise patch can pass as a detection; a false position is worse than a miss."""
    return 0.5 if size >= 12 else 0.7


def detect(img, tpl, cx, cy, R, floor_abs=None, floor_rel=0.6):
    """Every peak near (cx, cy) with its sub-pixel CENTRE position and NCC, plus the best SUB-floor
    match so a miss can be told apart as degraded (present, weak) or absent. Returns (peaks, near)
    where peaks == [] is a miss and near = {'ncc','x','y'} is the strongest thing in the window."""
    s = tpl.shape[0]
    if floor_abs is None:
        floor_abs = floor_for(s)
    scores, x0, y0 = ncc_window(img, tpl, cx, cy, R)
    j, i = np.unravel_index(int(np.argmax(scores)), scores.shape)
    near = {'ncc': float(scores[j, i]), 'x': x0 + i + s / 2.0, 'y': y0 + j + s / 2.0}
    peaks = []
    for v, j, i in local_maxima(scores, floor_abs, floor_rel):
        v2, fx, fy = refine_subpel(img, tpl, x0 + i, y0 + j)
        # top-left -> centre: the template covers [x, x+s), whose centre is x + s/2
        peaks.append({'x': x0 + i + fx + s / 2.0, 'y': y0 + j + fy + s / 2.0, 'ncc': max(v, v2)})
    return peaks, near


def mask_barcode(img):
    """The extractor must never match INTO the barcode strip or the top border it lives in."""
    out = img.copy()
    out[:TOP_BORDER, :] = out[TOP_BORDER:TOP_BORDER + 1, :].mean()
    return out


# ── the record ─────────────────────────────────────────────────────────────────────────────────
def load_zoo(zoo_dir):
    traj = json.load(open(os.path.join(zoo_dir, 'trajectories.json'), encoding='utf-8'))
    pats = {int(k): [np.array(p, np.float32) for p in v] for k, v in traj['patterns'].items()}
    return traj, pats


def template_for(marker, pats):
    return pats[marker['size']][marker['pattern']]


def displacement_reach(traj, fps):
    """R = ceil(max per-frame displacement) + 4, from the record's own nominal speeds."""
    mx = max(float(m.get('px_per_frame_nominal', 8.0)) for m in traj['markers'])
    return int(np.ceil(mx)) + 4


# ── the per-triple work ────────────────────────────────────────────────────────────────────────
def extract_triple(rec, dump_dir, W, H, traj, pats, fps, R, planes=('prev', 'live', 'next')):
    t = float(rec['t'])
    imgs = {p: load_plane(os.path.join(dump_dir, rec[p]), W, H) for p in planes}
    k_prev = decode_barcode(imgs['prev']) if 'prev' in imgs else None
    k_next = decode_barcode(imgs['next']) if 'next' in imgs else None
    T = int(traj['frames'])
    # the sequence loops; the pair straddling the wrap is k_next = 0 after k_prev = T-1
    span = ((k_next - k_prev) % T) if (k_prev is not None and k_next is not None) else None
    rows = []
    grey = {p: mask_barcode(luma(im)) for p, im in imgs.items()}
    for m in traj['markers']:
        tpl = template_for(m, pats)
        p_prev = eval_traj(m['model'], k_prev / fps)
        p_next = eval_traj(m['model'], (k_prev + span) / fps)      # the SAME instant the next plane shows
        p_model = p_prev + t * (p_next - p_prev)
        t_real = (k_prev + t * span) / fps
        p_true = eval_traj(m['model'], t_real)
        expect = {'prev': (p_prev, p_prev), 'next': (p_next, p_next), 'live': (p_model, p_true)}
        for plane in planes:
            e_model, e_true = expect[plane]
            peaks, near = detect(grey[plane], tpl, e_model[0], e_model[1], R)
            best = peaks[0] if peaks else None
            rows.append({
                'triple': rec['id'], 't': t, 'k_prev': k_prev, 'k_next': k_next, 'span': span,
                'marker': m['id'], 'class': m['class'], 'size': m['size'],
                'plane': plane,
                'exp_model_x': e_model[0], 'exp_model_y': e_model[1],
                'exp_true_x': e_true[0], 'exp_true_y': e_true[1],
                'n_peaks': len(peaks),
                'obs_x': best['x'] if best else float('nan'),
                'obs_y': best['y'] if best else float('nan'),
                'ncc': best['ncc'] if best else float('nan'),
                'err_model_px': float(np.hypot(best['x'] - e_model[0], best['y'] - e_model[1])) if best else float('nan'),
                'err_true_px': float(np.hypot(best['x'] - e_true[0], best['y'] - e_true[1])) if best else float('nan'),
                # the strongest thing in the window even when below the floor: a miss with ncc_max
                # 0.3-0.5 near the expectation is a DEGRADED marker; ncc_max ~0 is an ABSENT one
                'ncc_max': near['ncc'], 'near_x': near['x'], 'near_y': near['y'],
                'near_dist': float(np.hypot(near['x'] - e_model[0], near['y'] - e_model[1])),
            })
    return rows


FIELDS = ['triple', 't', 'k_prev', 'k_next', 'span', 'marker', 'class', 'size', 'plane',
          'exp_model_x', 'exp_model_y', 'exp_true_x', 'exp_true_y', 'n_peaks',
          'obs_x', 'obs_y', 'ncc', 'err_model_px', 'err_true_px',
          'ncc_max', 'near_x', 'near_y', 'near_dist']


# ── the self-test: the gate seen red before green ──────────────────────────────────────────────
def selftest(dump_dir, zoo_dir, fps, R, zoo_frame=None):
    """Section 4.1 on a RAW zoo frame (the extractor alone) or section 4.2 on a captured plane (the
    extractor plus the capture path). Whole-pixel shifts only: a whole-pixel roll of the image is
    EXACT, whereas a fractional resample blurs a marker that was already splatted once and then
    measures that second blur, not the extractor. Fractional phases are exercised by the markers
    themselves -- each sits at its own sub-pixel position -- and by using several frames."""
    traj, pats = load_zoo(zoo_dir)
    W, H = traj['width'], traj['height']

    def load_frame(k):
        return load_plane(os.path.join(zoo_dir, 'f_%06d.rgba' % k), W, H)

    if zoo_frame is not None:
        frames = [(k % traj['frames'], load_frame(k % traj['frames'])) for k in (zoo_frame, zoo_frame + 1, zoo_frame + 7)]
        src = f'RAW zoo frames {[k for k, _ in frames]} (no capture path)'
    else:
        size, recs = load_manifest(os.path.join(dump_dir, 'manifest.txt'))
        frames = []
        for rec in recs[:3]:
            im = load_plane(os.path.join(dump_dir, rec['prev']), size[0], size[1])
            frames.append((decode_barcode(im), im))
        src = f'CAPTURED prev planes of {[r["id"] for r in recs[:3]]} (through the FG)'
    print(f'selftest on {src}')

    def run(grey, k, shift, label):
        errs, by_size = [], {}
        for m in traj['markers']:
            p = eval_traj(m['model'], k / fps)
            ex = (p[0] + shift[0], p[1] + shift[1])
            peaks, _ = detect(grey, template_for(m, pats), p[0], p[1], R)   # search around the UNSHIFTED expectation
            if not peaks:
                continue
            e = float(np.hypot(peaks[0]['x'] - ex[0], peaks[0]['y'] - ex[1]))
            errs.append(e); by_size.setdefault(m['size'], []).append(e)
        errs = np.array(errs)
        gated = np.array([e for s_, v in by_size.items() if s_ >= 12 for e in v])   # the gate is on 12/24 px
        sz = '  '.join(f'{s_}px:{np.mean(v):.3f}' for s_, v in sorted(by_size.items()))
        print(f'  {label:<40s} n={errs.size:2d}  gated(>=12px) mean {gated.mean():.3f}  max {gated.max():.3f} px  | by size {sz}')
        return gated

    ok = True
    # (a) unshifted, several frames: 18 markers x 3 frames = 54 distinct fractional phases
    for k, im in frames:
        e = run(mask_barcode(luma(im)), k, (0.0, 0.0), f'k={k:2d} unshifted (expect ~0)')
        ok &= e.mean() <= 0.1
    # (b) whole-pixel rolls of the first frame: exact, no resampling
    k0, im0 = frames[0]
    g0 = mask_barcode(luma(im0))
    for dx, dy in ((3, 0), (0, -2), (-5, 4)):
        rolled = np.roll(np.roll(g0, dy, axis=0), dx, axis=1)
        e = run(rolled, k0, (float(dx), float(dy)), f'k={k0:2d} rolled ({dx:+d},{dy:+d}) (expect ~0)')
        ok &= e.mean() <= 0.1
    # (c) the gate seen RED: the rolled frame searched with the UNrolled expectation must report the
    #     roll itself -- an extractor that says 0 here is blind, whatever it says elsewhere
    rolled = np.roll(np.roll(g0, 4, axis=0), -5, axis=1)
    e_red = run(rolled, k0, (0.0, 0.0), f'k={k0:2d} RED: rolled (-5,+4), expected unrolled')
    true = float(np.hypot(5, 4))
    red_ok = abs(e_red.mean() - true) <= 0.15
    print(f'  RED: reported {e_red.mean():.3f} px vs the true shift {true:.3f} px -> '
          f'{"the extractor SEES the move" if red_ok else "FAIL: it did not see the move"}')
    ok &= red_ok
    print('SELFTEST:', 'PASSED' if ok else 'FAILED')
    return 0 if ok else 1


# ── main ───────────────────────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dump', required=True, help='a --qdump+ directory captured from the zoo')
    ap.add_argument('--zoo', required=True, help='the marker_zoo directory (trajectories.json + patterns)')
    ap.add_argument('--out', default=None, help='detections CSV')
    ap.add_argument('--fps', type=float, default=None,
                    help='the PLAYBACK rate. p(t) is in seconds; if the zoo was played at a rate other '
                         'than its manifest fps this MUST be given. Default: the zoo manifest fps.')
    ap.add_argument('--reach', type=int, default=None, help='search radius R (default: ceil(max px/frame)+4)')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--selftest-frame', type=int, default=None,
                    help='run the self-test on this RAW zoo frame instead of a captured plane '
                         '(section 4.1: the extractor alone, no capture path)')
    a = ap.parse_args()

    traj, pats = load_zoo(a.zoo)
    fps = a.fps if a.fps else float(traj['fps'])
    R = a.reach if a.reach else displacement_reach(traj, fps)
    if a.selftest or a.selftest_frame is not None:
        return selftest(a.dump, a.zoo, fps, R, zoo_frame=a.selftest_frame)

    size, recs = load_manifest(os.path.join(a.dump, 'manifest.txt'))
    W, H = size
    if (W, H) != (traj['width'], traj['height']):
        print(f'FAIL: dump is {W}x{H}, zoo is {traj["width"]}x{traj["height"]} — the capture was scaled; '
              f'positions would be in the wrong frame'); return 1
    out = a.out or os.path.join(a.dump, 'detections.csv')
    print(f'extract: {len(recs)} triples, {len(traj["markers"])} markers, R={R}, fps={fps:g}')
    allrows = []
    for rec in recs:
        rows = extract_triple(rec, a.dump, W, H, traj, pats, fps, R)
        allrows.extend(rows)
        live = [r for r in rows if r['plane'] == 'live' and r['n_peaks'] > 0]
        real = [r for r in rows if r['plane'] in ('prev', 'next') and r['n_peaks'] > 0]
        em = np.mean([r['err_model_px'] for r in live]) if live else float('nan')
        er = np.mean([r['err_model_px'] for r in real]) if real else float('nan')
        miss = sum(1 for r in rows if r['plane'] == 'live' and r['n_peaks'] == 0)
        ghost = sum(1 for r in rows if r['plane'] == 'live' and r['n_peaks'] >= 2)
        print(f'  {rec["id"]} t={float(rec["t"]):.4f} k={rows[0]["k_prev"]}->{rows[0]["k_next"]} '
              f'| real-plane err {er:.3f} px | live err vs model {em:.3f} px | miss {miss} ghost {ghost}')
    with open(out, 'w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        for r in allrows:
            w.writerow(r)
    real = [r['err_model_px'] for r in allrows if r['plane'] in ('prev', 'next') and r['n_peaks'] > 0]
    print(f'wrote {out}: {len(allrows)} rows')
    print(f'REAL-PLANE CHECK (the capture path, must be <= 0.25 px mean): '
          f'{np.mean(real):.3f} px mean, {np.percentile(real, 95):.3f} px p95, n={len(real)}'
          f' -> {"OK" if np.mean(real) <= 0.25 else "FAIL"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
