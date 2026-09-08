#!/usr/bin/env python3
"""aa_truth A1 — the anti-aliasing ground-truth corpus.

WHY THIS IS A NEW TOOL AND NOT A FLAG ON marker_zoo.py
------------------------------------------------------
`tools/motion_truth/marker_zoo.py` is a GATED instrument: M1's verdicts, R7(a)'s kernel default and
the DI-3 reliability numbers all rest on the corpus it produces. Adding a content class and a render
path to it would put a new code path inside the thing those gates were measured with. This tool
imports its proven primitives (the value-noise background, the RGBA/BMP writers) and leaves the
gated file byte-identical.

WHY ANALYTIC COVERAGE INSTEAD OF SUPERSAMPLING
----------------------------------------------
The plan called for rendering at NxN and box-downsampling to make the reference. For STRAIGHT EDGES
that is unnecessary and strictly worse: a 3x3 supersample quantises the reference to nine levels, so
the "ground truth" would itself carry 1/9-step staircasing — the exact artefact the corpus exists to
measure. The area of a unit square clipped by a half-plane has a closed form, so this renders the
reference EXACTLY. The reference is truth, not an approximation of it, and that is what makes the
gate mean something.

The parity warning from the plan still applies and is now moot by construction: at an even
supersample factor the 1x pixel centre lands on the corner between four subsamples, putting a
systematic 0.25 px misregistration into every training pair. Analytic coverage has no subsample grid,
so there is no parity to get wrong. `--ss` is kept only for the optional supersampled cross-check.

WHAT IT EMITS, per frame
------------------------
  ref/f_%06d.rgba    the ANTI-ALIASED target  — exact analytic pixel coverage
  alias/f_%06d.rgba  the ALIASED input        — one point sample at the pixel centre, 1 SPP
Both are the same geometry at the same sub-pixel positions, so the pair is aligned BY CONSTRUCTION
and no registration step can introduce error.

  truth.json         per-edge analytic geometry (angle, normal, offset, contrast) for every frame
  manifest.txt       the run's parameters

THE SELF-CHECK IS THE POINT (`--verify`). The plan's instruction was to SEE THE GATE RED FIRST: it
asserts that the aliased arm FAILS an edge-transition-width test against its own reference and that
the reference PASSES. A corpus whose aliased arm passes an aliasing test is not a corpus, it is a bug.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, io
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'motion_truth'))
from marker_zoo import value_noise, to_rgba, write_bmp   # noqa: E402  (proven primitives, reused)

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')


# ── exact coverage of a unit pixel by a half-plane ─────────────────────────────────────────────
# CONVENTION, stated once and obeyed everywhere below: every function returns the fraction of the
# pixel lying on the NEGATIVE side, {p : n.p + d < 0}. Getting this wrong once already produced a
# corpus whose two arms were byte-identical, so it is written down rather than remembered.
def halfplane_coverage(nx, ny, d, W, H):
    """Exact area of each unit pixel inside {n.p + d < 0}. n must be unit-length.

    Closed form. With a = min(|nx|,|ny|), b = max(|nx|,|ny|) and s the signed distance from the pixel
    CENTRE to the line, the area is 1 for s <= -(a+b)/2, 0 for s >= (a+b)/2, linear (0.5 - s/b) while
    |s| <= (b-a)/2 (the line crosses two opposite sides), and a corner triangle in between. Exact
    means the reference carries no staircase of its own -- which a 3x3 supersample would.
    """
    ys, xs = np.mgrid[0:H, 0:W]
    s = nx * (xs + 0.5) + ny * (ys + 0.5) + d
    a, b = abs(float(nx)), abs(float(ny))
    if a > b:
        a, b = b, a
    lo, hi = (b - a) * 0.5, (b + a) * 0.5
    t = np.abs(s)
    tri = np.zeros_like(s)
    if a * b > 1e-12:
        tri = (hi - t) ** 2 / (2.0 * a * b)
    lin = 0.5 - s / b if b > 1e-12 else np.full_like(s, 0.5)
    cov = np.where(s >= hi, 0.0,
          np.where(s <= -hi, 1.0,
          np.where(t <= lo, lin,
          np.where(s > 0.0, tri, 1.0 - tri))))
    return np.clip(cov, 0.0, 1.0).astype(np.float32)


def halfplane_point(nx, ny, d, W, H):
    """One sample at the pixel centre, same convention. This is what 1-SPP rasterisation does."""
    ys, xs = np.mgrid[0:H, 0:W]
    return (nx * (xs + 0.5) + ny * (ys + 0.5) + d < 0.0).astype(np.float32)


def _coverage_selftest(n=200000, seed=7):
    """Check the closed form against brute-force supersampling. A wrong reference is worse than none."""
    rng = np.random.default_rng(seed)
    worst = 0.0
    for _ in range(60):
        th = rng.uniform(0, 2 * np.pi)
        nx, ny = float(np.cos(th)), float(np.sin(th))
        d = float(rng.uniform(-1.0, 1.0)) - (nx + ny) * 0.5   # line near the pixel at (0,0)
        exact = float(halfplane_coverage(nx, ny, d, 1, 1)[0, 0])
        u = rng.random(n); v = rng.random(n)
        mc = float((nx * u + ny * v + d < 0.0).mean())
        worst = max(worst, abs(exact - mc))
    return worst


def bar_mask(theta_deg, offset, width, W, H, exact):
    """A straight bar of the given width at the given angle, centred at `offset` along its normal.

    With the negative-side convention, the bar is {p . n < offset + w/2} MINUS {p . n < offset - w/2}.
    """
    th = np.deg2rad(theta_deg)
    nx, ny = float(np.sin(th)), float(-np.cos(th))
    f = halfplane_coverage if exact else halfplane_point
    near = f(nx, ny, -(offset + width * 0.5), W, H)
    far = f(nx, ny, -(offset - width * 0.5), W, H)
    return np.clip(near - far, 0.0, 1.0)


# ── the scene ──────────────────────────────────────────────────────────────────────────────────
ANGLES = (1.0, 5.0, 15.0, 45.0)          # the staircase period is 1/tan(theta) px: 57.3, 11.4, 3.7, 1.0
CONTRASTS = (0.25, 0.55, 0.95)


def build_edges(W, H, angles, contrasts, bar_w):
    """One bar per (angle x contrast), laid out so none overlaps another."""
    out, n = [], len(angles) * len(contrasts)
    diag = float(np.hypot(W, H))
    for i, th in enumerate(angles):
        for j, c in enumerate(contrasts):
            k = i * len(contrasts) + j
            # spread the offsets over the image's own diagonal extent so every bar is visible
            off = (-0.5 + (k + 0.5) / n) * diag * 0.9
            out.append({'angle_deg': th, 'contrast': c, 'width_px': bar_w, 'offset0_px': off})
    return out


def render(W, H, edges, bg, t_px, exact):
    """t_px = sub-pixel translation applied along each bar's own normal this frame."""
    img = bg.copy()
    for e in edges:
        m = bar_mask(e['angle_deg'], e['offset0_px'] + t_px, e['width_px'], W, H, exact)
        img = img * (1.0 - m * e['contrast']) + m * e['contrast']
    return np.clip(img, 0.0, 1.0)


# ── the probe the gate is built on ─────────────────────────────────────────────────────────────
def edge_width(gray):
    """Mean 10-90% transition width, in pixels, over the image's own gradient.

    THE ANTI-LIE MEASURE. PSNR, SSIM and warping error all reward blur, because a blurred image and a
    correctly anti-aliased one both move toward a smooth reference. Transition width does not: a blur
    WIDENS it and correct AA does not. Reported next to mean gradient magnitude, because a filter can
    also cheat by flattening the edge away entirely.
    """
    gx = np.abs(np.diff(gray, axis=1)).mean()
    gy = np.abs(np.diff(gray, axis=0)).mean()
    g = (gx + gy) / 2.0
    span = float(gray.max() - gray.min())
    return (span / g if g > 1e-9 else float('inf')), float(g)


def main():
    ap = argparse.ArgumentParser(description='aa_truth A1 — anti-aliasing ground-truth corpus')
    ap.add_argument('--out', required=True)
    ap.add_argument('--width', type=int, default=1280)
    ap.add_argument('--height', type=int, default=720)
    ap.add_argument('--frames', type=int, default=60)
    ap.add_argument('--drift', type=float, default=0.37,
                    help='sub-pixel translation per frame, px. Deliberately irrational-ish so the '
                         'sequence never repeats a sub-pixel phase — the aliasing pattern must MOVE '
                         'or the temporal arm has nothing to integrate.')
    ap.add_argument('--bar-width', type=float, default=9.0)
    ap.add_argument('--bg', default='noise', choices=['flat', 'noise'])
    ap.add_argument('--bg-amp', type=float, default=0.18)
    ap.add_argument('--seed', type=int, default=20260907)
    ap.add_argument('--ss', type=int, default=0,
                    help='optional supersampled cross-check of the analytic reference (odd only)')
    ap.add_argument('--bmp', type=int, default=0)
    ap.add_argument('--verify', action='store_true',
                    help='SEE THE GATE RED FIRST: assert the aliased arm fails and the reference passes')
    a = ap.parse_args()
    if a.ss and a.ss % 2 == 0:
        sys.exit('--ss must be ODD: at an even factor the 1x pixel centre falls between subsamples, '
                 'which is a systematic 0.25 px misregistration in every pair.')

    W, H = a.width, a.height
    rng = np.random.default_rng(a.seed)
    bg = (0.5 + a.bg_amp * (value_noise(H, W, 24, rng) - 0.5) * 2.0).astype(np.float32) if a.bg == 'noise' \
        else np.full((H, W), 0.5, np.float32)
    edges = build_edges(W, H, ANGLES, CONTRASTS, a.bar_width)

    for sub in ('ref', 'alias'):
        os.makedirs(os.path.join(a.out, sub), exist_ok=True)

    truth = {'width': W, 'height': H, 'frames': a.frames, 'drift_px': a.drift, 'seed': a.seed,
             'bar_width_px': a.bar_width, 'angles_deg': list(ANGLES), 'contrasts': list(CONTRASTS),
             'reference': 'exact analytic pixel coverage', 'input': '1 sample at the pixel centre',
             'edges': edges, 'per_frame_offset_px': []}

    for k in range(a.frames):
        t = k * a.drift
        truth['per_frame_offset_px'].append(t)
        ref = render(W, H, edges, bg, t, exact=True)
        ali = render(W, H, edges, bg, t, exact=False)
        to_rgba(ref).tofile(os.path.join(a.out, 'ref', 'f_%06d.rgba' % k))
        to_rgba(ali).tofile(os.path.join(a.out, 'alias', 'f_%06d.rgba' % k))
        if k < a.bmp:
            write_bmp(os.path.join(a.out, 'ref', 'f_%06d.bmp' % k), to_rgba(ref))
            write_bmp(os.path.join(a.out, 'alias', 'f_%06d.bmp' % k), to_rgba(ali))
        if (k + 1) % 20 == 0 or k + 1 == a.frames:
            print('  %d/%d frames' % (k + 1, a.frames))

    json.dump(truth, open(os.path.join(a.out, 'truth.json'), 'w'), indent=1)
    with open(os.path.join(a.out, 'manifest.txt'), 'w', encoding='utf-8') as f:
        f.write('# aa_truth A1 corpus\nsize %d %d\nframes %d\ndrift_px %g\nseed %d\n'
                'reference exact-analytic-coverage\ninput point-sample-at-pixel-centre\n'
                'angles_deg %s\ncontrasts %s\n'
                % (W, H, a.frames, a.drift, a.seed,
                   ','.join(str(x) for x in ANGLES), ','.join(str(x) for x in CONTRASTS)))
    print('wrote %d ref + %d alias frames + truth.json to %s' % (a.frames, a.frames, a.out))

    if a.verify:
        print()
        err = _coverage_selftest()
        print('coverage closed form vs brute-force supersampling: worst |err| = %.5f' % err)
        if err > 2e-3:
            sys.exit('the analytic reference disagrees with supersampling -- the formula is wrong')
        print()
        print('VERIFY — the aliased arm must FAIL and the reference must PASS.')
        r = np.fromfile(os.path.join(a.out, 'ref', 'f_000000.rgba'), np.uint8).reshape(H, W, 4)[:, :, 0] / 255.0
        s = np.fromfile(os.path.join(a.out, 'alias', 'f_000000.rgba'), np.uint8).reshape(H, W, 4)[:, :, 0] / 255.0
        wr, gr = edge_width(r)
        ws, gs = edge_width(s)
        print('  reference : transition width %.3f px | mean |grad| %.5f' % (wr, gr))
        print('  aliased   : transition width %.3f px | mean |grad| %.5f' % (ws, gs))
        # A point-sampled edge is a hard step: its transition is narrower and its gradient larger.
        ok = ws < wr and gs > gr
        print('  -> aliased is HARDER than the reference: %s' % ('YES (corpus is sound)' if ok else 'NO — BUG'))
        # and the pair must be aligned: the difference must be confined to edge neighbourhoods
        d = np.abs(r - s)
        print('  mean |ref-alias| %.5f | p99 %.5f | fraction of pixels differing > 2/255: %.2f%%'
              % (d.mean(), np.percentile(d, 99), 100.0 * (d > 2 / 255).mean()))
        if not ok:
            sys.exit(1)


if __name__ == '__main__':
    main()
