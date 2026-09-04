#!/usr/bin/env python3
"""marker_zoo.py — the ground-truth marker sequence generator (MOTION_TRUTH phase T2).

WHY THIS EXISTS, TWICE OVER.

(1) It is the source of M1. The operator's directive is "frames that simulate the motion CORRECTLY —
    I want data, not images". A position table is only ground truth if the true position is known at
    ANY real t, not just at the integer frames the source rendered. So this generator writes the
    analytic trajectory parameters beside the pixels: `p_i(t)` is a closed form the extractor
    evaluates at the exact phase the FG generated.

(2) It is the only thing in the repo that can answer S2.T6's own open question. Every number in the
    deadzone diagnosis was measured on `ball_zoo`: a hard-edged, NON-antialiased 24 px lattice. The MV
    field there carries a period-3 sub-pixel pattern whose period matches that lattice exactly, and
    `shaders/wap_warp.comp:1312-1313` independently documents a period-aliasing limit. Three periodic
    facts, currently tracked as unrelated. `--bg noise` produces texture with gradient everywhere and
    NO period, which settles whether the deadzone is a property of the shader or of the test content.
    `--bg grating` reproduces the lattice deliberately, so the two are measured the same way.

WHAT IT WRITES
    f_%06d.rgba        RGBA8, row-major — the format the scorer and `--qdump+` already speak
    positions.csv      k,marker,class,size,x,y,visible,occluded_frac
    trajectories.json  the closed forms + the background model; `p_i(t)` at any real t
    manifest.txt       size W H · sequence f_ T · fps · bg · seed

EVERY FRAME CARRIES ITS OWN INDEX. A 16-bit barcode (16 blocks of 8x8 px, MSB left, black=0/white=1)
sits at x in [8,136), y in [4,12) — inside the top 16 px that the scorer's metrics already crop and
that the extractor masks out. Recovering k from a captured frame is then exact and immune to capture
timing: duplicates and drops show up as a step != 1 instead of silently corrupting a position table.

    python tools/motion_truth/marker_zoo.py --out DIR [--bg noise] [--seconds 4] [--verify]

numpy + stdlib only (this rig has numpy 2.5.2 and nothing else). Made with my soul - Swately <3
"""
import argparse, json, os, struct, sys
import numpy as np

# ── the barcode geometry (shared with the decoder, and with T4's extractor later) ───────────────
BC_BITS   = 16
BC_BLOCK  = 8            # px per bit, square
BC_X0     = 8            # first block's left edge
BC_Y0     = 4            # top edge
BC_W      = BC_BITS * BC_BLOCK          # 128
BC_Y1     = BC_Y0 + BC_BLOCK            # 12
TOP_BORDER = 16          # markers stay below this; the scorer crops it


# ── background models ──────────────────────────────────────────────────────────────────────────
def value_noise(h, w, cell, rng, octaves=3):
    """Smooth, APERIODIC texture: a random lattice per octave, bilinearly interpolated and summed.

    This is the background the deadzone question needs. A flat field gives the block matcher nothing
    to match (mv = 0 everywhere, no gradient to fit against); a grating gives it a period to alias
    on. Value noise gives gradient at every pixel with no repeating structure, so a sub-pixel MV the
    matcher reports there cannot be a period artefact.
    """
    acc = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        c = max(2, int(cell / (2 ** o)))
        gh, gw = h // c + 2, w // c + 2
        lat = rng.random((gh, gw), dtype=np.float32)
        ys = np.arange(h, dtype=np.float32) / c
        xs = np.arange(w, dtype=np.float32) / c
        y0 = np.floor(ys).astype(np.int64); fy = (ys - y0)[:, None]
        x0 = np.floor(xs).astype(np.int64); fx = (xs - x0)[None, :]
        # smoothstep the fractions: bilinear alone leaves lattice-aligned creases, which are
        # themselves periodic structure — exactly what this background must not have.
        fy = fy * fy * (3.0 - 2.0 * fy)
        fx = fx * fx * (3.0 - 2.0 * fx)
        a = lat[np.ix_(y0, x0)]; b = lat[np.ix_(y0, x0 + 1)]
        c2 = lat[np.ix_(y0 + 1, x0)]; d = lat[np.ix_(y0 + 1, x0 + 1)]
        acc += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (c2 * (1 - fx) + d * fx) * fy)
        total += amp; amp *= 0.5
    return acc / total


def make_background(kind, h, w, rng, pad):
    """Return a field LARGER than the frame by `pad` on each side, so panning samples real content
    instead of wrapping (a wrap seam is a hard edge, and hard edges are what we are studying)."""
    H, W = h + 2 * pad, w + 2 * pad
    if kind == 'flat':
        f = np.full((H, W), 0.10, np.float32)
    elif kind == 'noise':
        f = 0.12 + 0.55 * value_noise(H, W, cell=48, rng=rng)
    elif kind == 'grating':
        # deliberately the ball_zoo condition: 24 px minor / 96 px major, hard-edged, no AA
        f = np.full((H, W), 0.07, np.float32)
        ys, xs = np.mgrid[0:H, 0:W]
        f[(xs % 24 == 0) | (ys % 24 == 0)] = 0.43
        f[(xs % 96 == 0) | (ys % 96 == 0)] = 0.65
    else:
        raise ValueError('unknown --bg ' + kind)
    return f


# ── markers ────────────────────────────────────────────────────────────────────────────────────
def make_patterns(sizes, per_size, rng):
    """Binary patterns with a 1-px white border and a rejection-sampled distinct interior.

    DEVIATION FROM THE PLAN, RECORDED. The strategy doc asks for "Hamming distance >= 40 bits between
    any two". That is stated for the 12x12 case, whose interior holds 100 bits. At size 6 the interior
    holds 16 bits and 40 is unreachable, so the floor here is `max(4, 40% of the interior bits)`,
    which is 40 bits at size 12 (100 bits) and scales honestly at 6 and 24. Patterns are generated at
    their native size rather than downscaled from a 12x12 master, because downscaling a binary pattern
    to 6x6 destroys the very distinctness the constraint exists to guarantee.
    """
    out = {}
    for s in sizes:
        inner = s - 2
        nbits = inner * inner
        floor = max(4, int(round(0.40 * nbits)))
        pats, tries = [], 0
        while len(pats) < per_size:
            tries += 1
            if tries > 20000:
                raise RuntimeError(f'could not find {per_size} distinct {s}px patterns (floor {floor})')
            cand = (rng.random((inner, inner)) < 0.5)
            if cand.sum() < nbits * 0.3 or cand.sum() > nbits * 0.7:
                continue                      # avoid near-blank / near-solid interiors
            if any(int((cand ^ p).sum()) < floor for p in pats):
                continue
            pats.append(cand)
        full = []
        for p in pats:
            m = np.ones((s, s), np.float32)   # the 1-px white border
            m[1:-1, 1:-1] = p.astype(np.float32)
            full.append(m)
        out[s] = full
    return out


def splat(dst, idbuf, pat, x, y, mid):
    """Sub-pixel bilinear splat of `pat` with its TOP-LEFT at continuous (x, y).

    Antialiasing is not cosmetic here: at 60 fps a 0.5 px/frame marker whose edges snapped to the
    pixel grid would render byte-identical consecutive frames, the capture dedup would collapse them,
    and the "source" would lie about its own rate. This is the ball_zoo lesson, applied.
    """
    h, w = dst.shape
    s = pat.shape[0]
    ix, iy = int(np.floor(x)), int(np.floor(y))
    fx, fy = np.float32(x - ix), np.float32(y - iy)
    acc = np.zeros((s + 1, s + 1), np.float32)
    wgt = np.zeros((s + 1, s + 1), np.float32)
    for dy, wy in ((0, 1.0 - fy), (1, fy)):
        for dx, wx in ((0, 1.0 - fx), (1, fx)):
            ww = np.float32(wy * wx)
            if ww == 0.0:
                continue
            acc[dy:dy + s, dx:dx + s] += pat * ww
            wgt[dy:dy + s, dx:dx + s] += ww
    x0, y0 = ix, iy
    x1, y1 = ix + s + 1, iy + s + 1
    sx0, sy0 = max(0, -x0), max(0, -y0)
    x0c, y0c = max(0, x0), max(0, y0)
    x1c, y1c = min(w, x1), min(h, y1)
    if x1c <= x0c or y1c <= y0c:
        return
    a = acc[sy0:sy0 + (y1c - y0c), sx0:sx0 + (x1c - x0c)]
    g = wgt[sy0:sy0 + (y1c - y0c), sx0:sx0 + (x1c - x0c)]
    view = dst[y0c:y1c, x0c:x1c]
    dst[y0c:y1c, x0c:x1c] = view * (1.0 - g) + a
    core = g > 0.5
    idv = idbuf[y0c:y1c, x0c:x1c]
    idv[core] = mid


# ── trajectories: closed forms, evaluable at any real t ────────────────────────────────────────
def eval_traj(model, t):
    """p(t) in pixels. t in SECONDS, any real value — this is the whole point of the file."""
    ty = model['type']
    if ty == 'static':
        return np.array(model['p0'], np.float64)
    if ty == 'linear':
        p0 = np.array(model['p0'], np.float64); v = np.array(model['v'], np.float64)
        return p0 + v * t
    if ty == 'accel':
        p0 = np.array(model['p0'], np.float64); v = np.array(model['v'], np.float64)
        a = np.array(model['a'], np.float64)
        return p0 + v * t + 0.5 * a * t * t
    if ty == 'circular':
        c = np.array(model['c'], np.float64)
        R = float(model['R']); w = float(model['omega']); ph = float(model['phase'])
        return c + R * np.array([np.cos(w * t + ph), np.sin(w * t + ph)], np.float64)
    raise ValueError('unknown trajectory type ' + ty)


def build_markers(classes, K, W, H, fps, sizes, patterns, rng):
    """Trajectory parameters chosen so the PER-FRAME displacement lands where each class needs it.

    linear/accel/circular span 0.5-8 px/frame — inside the 8 px block matcher's reach. `fast` sits at
    12-16 px/frame, OUTSIDE it: an expected-failure control. A run whose fast markers do NOT show
    error is a run whose instrument is not measuring, which is the "gate seen red" the empirical-test
    protocol asks for before any green is believed.
    """
    y_lo, y_hi = TOP_BORDER + 24, H - 24
    x_lo, x_hi = 24, W - 24
    ms = []
    mid = 0
    per_class = max(1, K // max(1, len(classes)))
    for ci, cls in enumerate(classes):
        for j in range(per_class):
            s = sizes[(mid) % len(sizes)]
            pat_list = patterns[s]
            pat_i = mid % len(pat_list)
            # a per-frame speed spread across the class's band
            frac = (j + 0.5) / per_class
            if cls == 'fast':
                spf = 12.0 + 4.0 * frac                      # px per frame, outside the matcher
            else:
                spf = 0.5 + 7.5 * frac                       # px per frame, inside it
            v_px_s = spf * fps
            y = y_lo + (y_hi - y_lo) * ((mid * 0.37) % 1.0)
            if cls == 'linear' or cls == 'fast':
                model = {'type': 'linear', 'p0': [float(x_lo), float(y)], 'v': [float(v_px_s), 0.0]}
            elif cls == 'accel':
                # start at half the band speed and accelerate to it over the sequence
                model = {'type': 'accel', 'p0': [float(x_lo), float(y)],
                         'v': [float(v_px_s * 0.4), 0.0], 'a': [float(v_px_s * 0.6), 0.0]}
            elif cls == 'circular':
                R = min(W, H) * 0.22
                omega = v_px_s / R                            # |v| = R*omega
                model = {'type': 'circular', 'c': [W * 0.5, (y_lo + y_hi) * 0.5],
                         'R': float(R), 'omega': float(omega), 'phase': float(2 * np.pi * frac)}
            elif cls == 'crossing':
                # two linears whose paths intersect mid-sequence; z-order is the draw order, so the
                # LOWER id is occluded when they meet — an occlusion event with a known instant.
                down = (j % 2 == 1)
                y0 = y_lo if not down else y_hi
                y1 = y_hi if not down else y_lo
                model = {'type': 'linear', 'p0': [float(x_lo), float(y0)],
                         'v': [float(v_px_s), float((y1 - y0) / 4.0)]}
            elif cls == 'hud':
                model = {'type': 'static', 'p0': [float(x_lo + (x_hi - x_lo) * frac), float(y)]}
            else:
                raise ValueError('unknown class ' + cls)
            ms.append({'id': mid, 'class': cls, 'size': int(s), 'pattern': int(pat_i),
                       'px_per_frame_nominal': float(spf), 'model': model})
            mid += 1
    return ms


# ── the barcode ────────────────────────────────────────────────────────────────────────────────
def draw_barcode(img, k):
    """16 bits of k, MSB left. A 1-px black frame around the strip so a decoder can find its edge
    even against a light background."""
    img[BC_Y0 - 1:BC_Y1 + 1, BC_X0 - 1:BC_X0 + BC_W + 1] = 0.0
    for b in range(BC_BITS):
        bit = (k >> (BC_BITS - 1 - b)) & 1
        x = BC_X0 + b * BC_BLOCK
        img[BC_Y0:BC_Y1, x:x + BC_BLOCK] = 1.0 if bit else 0.0


def decode_barcode(rgba, w=None):
    """Recover k from a saved frame. Mean of each block's 6x6 CENTRE, thresholded at 0.5 — the centre
    so that a resampled or slightly shifted capture cannot bleed a neighbour's edge into the vote."""
    g = rgba[..., 0].astype(np.float32) / 255.0
    k = 0
    for b in range(BC_BITS):
        x = BC_X0 + b * BC_BLOCK
        cell = g[BC_Y0 + 1:BC_Y1 - 1, x + 1:x + BC_BLOCK - 1]
        k = (k << 1) | (1 if float(cell.mean()) > 0.5 else 0)
    return k


# ── output helpers ─────────────────────────────────────────────────────────────────────────────
def to_rgba(gray):
    v = np.clip(gray, 0.0, 1.0)
    u = np.rint(v * 255.0).astype(np.uint8)
    out = np.empty(u.shape + (4,), np.uint8)
    out[..., 0] = u; out[..., 1] = u; out[..., 2] = u; out[..., 3] = 255
    return out


def write_bmp(path, rgba):
    """A 24-bit BMP, stdlib only — for a one-time human glance. The gate is the CSVs, not this."""
    h, w = rgba.shape[:2]
    row = (w * 3 + 3) & ~3
    data = bytearray()
    for y in range(h - 1, -1, -1):
        line = rgba[y, :, [2, 1, 0]].T.tobytes()
        data += line + b'\x00' * (row - w * 3)
    hdr = struct.pack('<2sIHHI', b'BM', 14 + 40 + len(data), 0, 0, 14 + 40)
    dib = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    open(path, 'wb').write(hdr + dib + bytes(data))


# ── main ───────────────────────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description='MOTION_TRUTH T2 — the ground-truth marker zoo')
    ap.add_argument('--out', required=True)
    ap.add_argument('--width', type=int, default=1280)
    ap.add_argument('--height', type=int, default=720)
    ap.add_argument('--fps', type=float, default=60.0)
    ap.add_argument('--seconds', type=float, default=2.0)
    ap.add_argument('--classes', default='linear,accel,circular,crossing,hud,fast')
    ap.add_argument('--bg', default='noise', choices=['flat', 'noise', 'grating'])
    ap.add_argument('--bg-pan', type=float, default=None,
                    help='background pan in px/s (default: 0, or 120 when the hud class is present — '
                         'a screen-fixed marker over a moving world is the HUD case)')
    ap.add_argument('--markers', type=int, default=18)
    ap.add_argument('--sizes', default='6,12,24')
    ap.add_argument('--seed', type=int, default=20260904)
    ap.add_argument('--bmp', type=int, default=0, help='also write the first N frames as .bmp')
    ap.add_argument('--verify', action='store_true',
                    help='decode the barcode from every SAVED frame and re-evaluate p(t) from '
                         'trajectories.json against positions.csv')
    a = ap.parse_args()

    os.makedirs(a.out, exist_ok=True)
    rng = np.random.default_rng(a.seed)
    W, H = a.width, a.height
    T = int(round(a.seconds * a.fps))
    classes = [c.strip() for c in a.classes.split(',') if c.strip()]
    sizes = [int(s) for s in a.sizes.split(',') if s.strip()]
    pan = a.bg_pan if a.bg_pan is not None else (120.0 if 'hud' in classes else 0.0)

    patterns = make_patterns(sizes, per_size=max(4, a.markers), rng=rng)
    markers = build_markers(classes, a.markers, W, H, a.fps, sizes, patterns, rng)
    pad = int(abs(pan) * a.seconds) + 8
    bg = make_background(a.bg, H, W, rng, pad)

    print(f'zoo: {W}x{H} x {T} frames @ {a.fps:g} fps | bg={a.bg} pan={pan:g}px/s | '
          f'{len(markers)} markers, sizes {sizes} | seed {a.seed}')

    rows = []
    for k in range(T):
        t = k / a.fps
        ox = int(round(pan * t))
        img = bg[pad:pad + H, pad + ox:pad + ox + W].copy()
        idbuf = np.full((H, W), -1, np.int32)
        for m in markers:
            p = eval_traj(m['model'], t)
            s = m['size']
            # p is the marker CENTRE; the splat takes the top-left
            splat(img, idbuf, patterns[s][m['pattern']], p[0] - s * 0.5, p[1] - s * 0.5, m['id'])
        draw_barcode(img, k)
        rgba = to_rgba(img)
        rgba.tofile(os.path.join(a.out, 'f_%06d.rgba' % k))
        if k < a.bmp:
            write_bmp(os.path.join(a.out, 'f_%06d.bmp' % k), rgba)
        for m in markers:
            p = eval_traj(m['model'], t)
            s = m['size']
            onscreen = (TOP_BORDER <= p[1] - s * 0.5) and (p[1] + s * 0.5 < H) and \
                       (0 <= p[0] - s * 0.5) and (p[0] + s * 0.5 < W)
            own = int((idbuf == m['id']).sum())
            occ = 0.0 if not onscreen else max(0.0, 1.0 - own / float(max(1, (s - 1) * (s - 1))))
            rows.append((k, m['id'], m['class'], s, float(p[0]), float(p[1]),
                         1 if onscreen else 0, occ))
        if (k + 1) % 30 == 0 or k + 1 == T:
            print('  %d/%d frames' % (k + 1, T))

    with open(os.path.join(a.out, 'positions.csv'), 'w', encoding='utf-8', newline='') as f:
        f.write('k,marker,class,size,x,y,visible,occluded_frac\n')
        for r in rows:
            f.write('%d,%d,%s,%d,%.9f,%.9f,%d,%.6f\n' % r)

    traj = {'fps': a.fps, 'width': W, 'height': H, 'frames': T, 'seed': a.seed,
            'background': {'class': a.bg, 'pan_px_s': [pan, 0.0], 'pad': pad},
            'barcode': {'bits': BC_BITS, 'block': BC_BLOCK, 'x0': BC_X0, 'y0': BC_Y0,
                        'msb': 'left', 'white': 1, 'top_border': TOP_BORDER},
            'markers': markers}
    json.dump(traj, open(os.path.join(a.out, 'trajectories.json'), 'w', encoding='utf-8'), indent=1)

    with open(os.path.join(a.out, 'manifest.txt'), 'w', encoding='utf-8') as f:
        f.write('# motion-truth marker zoo (T2) — ground truth is trajectories.json, NOT the pixels\n')
        f.write('size %d %d\n' % (W, H))
        f.write('sequence f_ %d\n' % T)
        f.write('fps %g\n' % a.fps)
        f.write('bg %s pan %g\n' % (a.bg, pan))
        f.write('seed %d\n' % a.seed)

    print('wrote %d frames + positions.csv (%d rows) + trajectories.json + manifest.txt'
          % (T, len(rows)))

    if a.verify:
        return verify(a.out)
    return 0


def verify(d):
    """The T2 gate, run against what is ON DISK — not against the arrays that produced it."""
    traj = json.load(open(os.path.join(d, 'trajectories.json'), encoding='utf-8'))
    W, H, T, fps = traj['width'], traj['height'], traj['frames'], traj['fps']
    print('\nVERIFY (reading back from disk):')

    bad = 0
    for k in range(T):
        rgba = np.fromfile(os.path.join(d, 'f_%06d.rgba' % k), dtype=np.uint8)
        if rgba.size != W * H * 4:
            print('  FAIL f_%06d: %d B, expected %d' % (k, rgba.size, W * H * 4)); bad += 1; continue
        got = decode_barcode(rgba.reshape(H, W, 4))
        if got != k:
            print('  FAIL f_%06d: barcode decodes to %d' % (k, got)); bad += 1
    print('  barcode: %d/%d frames decode to their own index%s' % (T - bad, T, '' if bad == 0 else '  <-- FAIL'))

    csv = [l.strip().split(',') for l in open(os.path.join(d, 'positions.csv'), encoding='utf-8')][1:]
    byid = {m['id']: m for m in traj['markers']}
    worst, worst_at = 0.0, None
    for r in csv:
        if len(r) < 6:
            continue
        k, mid = int(r[0]), int(r[1])
        x, y = float(r[4]), float(r[5])
        p = eval_traj(byid[mid]['model'], k / fps)
        e = max(abs(p[0] - x), abs(p[1] - y))
        if e > worst:
            worst, worst_at = e, (k, mid)
    print('  p(t) at t=k/fps vs positions.csv: worst |delta| = %.3e px%s (at %s)'
          % (worst, '' if worst <= 1e-6 else '  <-- FAIL', worst_at))

    # the property the whole file exists for: p(t) is defined BETWEEN frames
    m0 = traj['markers'][0]
    half = eval_traj(m0['model'], 0.5 / fps)
    p0 = eval_traj(m0['model'], 0.0)
    p1 = eval_traj(m0['model'], 1.0 / fps)
    mid_ok = np.all(np.isfinite(half))
    print('  p(t) at a NON-integer frame (t = 0.5/fps): marker 0 -> (%.4f, %.4f); frame 0 -> (%.4f, %.4f), '
          'frame 1 -> (%.4f, %.4f)%s' % (half[0], half[1], p0[0], p0[1], p1[0], p1[1],
                                         '' if mid_ok else '  <-- FAIL'))

    ok = (bad == 0) and (worst <= 1e-6) and mid_ok
    print('RESULT:', 'T2 GATE PASSED' if ok else 'T2 GATE FAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
