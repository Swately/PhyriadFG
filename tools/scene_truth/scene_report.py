#!/usr/bin/env python3
"""scene_truth — score a candidate's intermediate frames against the exact truth, term by term.

WHAT "HALLUCINATION" IS, COMPUTABLY
-----------------------------------
A per-pixel difference is L2, and L2 rewards blur: a ghost smeared to half intensity scores BETTER
than a sharp object one pixel off. A1 measured that inversion on edges; this corpus makes it visible
on whole objects. So the score is never one number. Each term below IS a named artefact, and the
verdict is conjunctive — a candidate passes only if it fails none of them:

    pos_err      centroid of the candidate's silhouette vs the truth's, object k               px
    shape_err    symmetric boundary (chamfer) distance, candidate silhouette vs truth's         px
    halluc       candidate silhouette where the truth silhouette is absent (ghosts, doubles,
                 crescents)                                                                    px²
                 + a SIGN along the object's motion: leading (ahead of where it is going) or trailing
    missing      truth silhouette where the candidate's is absent (holes)                      px²
    bg_err       RMS error on backdrop pixels far from every object (seams, reclaim)           lum
    sharp        edge strength on the truth's boundaries, candidate / truth  (1 = as sharp;
                 < 1 = softer = BLUR, and blur is an error here, not "close" — the operator's F2)
    disocc       RMS error on class-0 pixels (visible in NEITHER real frame) — reported APART,
                 never in the verdict, next to `graceful` = RMS distance to the nearest REAL frame,
                 which is what failing gracefully looks like
    l2_det       RMS on determinable pixels only, carried so the inversion stays visible

ONE OPERATOR FOR BOTH SILHOUETTES — the correction the gate forced
-------------------------------------------------------------------
The first version took the truth's silhouette from the exact `id` plane (a hard, centre-sampled
outline) and the candidate's from its difference against the backdrop (an anti-aliased ramp). Two
operators; their disagreement showed up as 1.5 px of "error" in the truth scored against itself, and
a neighbouring object inside the "near k" dilation dragged the centroid 10 px. Now BOTH silhouettes
come from the same operator — object-like = differs from the static, exactly known backdrop-only
render — restricted to a window around object k that EXCLUDES pixels the exact labels give to other
objects, and wide enough to hold the object wherever a candidate may have displaced it. The truth
scored against itself is then exactly zero on all four object terms, which is what a floor should
be. The exact labels still decide everything they alone can: which pixels are backdrop far from any
object, which belong to another object, and which no real frame saw.

THE GATE — arms whose scores are known in advance, seen red first
----------------------------------------------------------------
Five candidate arms are built from the corpus itself, each with a predicted signature:
    truth     the held-out frame              pos = shape = halluc = missing = 0 EXACTLY, sharp = 1
    nearest   the nearer real source frame    pos_err = the travel of the truth's own centroid
                                              between mid and that frame, read from the exact id
                                              planes — the closed form this scorer checks itself
                                              against, valid under rotation too
    blend     ½(N + N+1), the classic ghost   pos_err ≈ 0 yet halluc > 0 and sharp < 1 — position
                                              alone would ACCEPT it; the verdict must not
    oracle2   exact bidirectional warp        ACCEPT on the determinable; error only in disocc
    blur      the truth, Gaussian-blurred     BLUR flagged, pos_err ≈ 0 (the veto fires; shape and
                                              halluc may also move — a blur widens a silhouette)
The run-to-run spread (DI-3) comes from two corpora with different seeds.

Made with my soul - Swately <3
"""
import argparse, json, os, sys
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')     # reconfigure, never re-wrap (see scene_zoo)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scene_zoo as Z   # noqa: E402  (the single source of the scene's truth)

OBJ_TAU = 0.06          # per-channel |image − backdrop| above this = object-like
NEAR_R = 6              # px added to an object's window beyond its own displacement
FLOOR_SHARP = 0.90      # sharp ratio below this = softer than the truth = blur


# ── io ───────────────────────────────────────────────────────────────────────────────────────────
def load_rgb(p, W, H):
    return np.fromfile(p, np.uint8).reshape(H, W, 4)[:, :, :3].astype(np.float64) / 255.0


def load_id(d, i, W, H):
    return np.fromfile(os.path.join(d, 'id', 'f_%06d.u8' % i), np.uint8).reshape(H, W)


def lum(rgb):
    return rgb @ np.array([0.299, 0.587, 0.114])


def load_corpus(d):
    global STRIP
    t = json.load(open(os.path.join(d, 'truth.json')))
    sc = Z.Scene(t['objects'][1:], t['width'], t['height'], t['fov_deg'], t['seed'])
    s = t.get('barcode_strip')
    STRIP = (s['x0'], s['x1'], s['y0'], s['y1']) if s else None
    return t, sc


def triples(d, K):
    p = os.path.join(d, 'manifest_k%d.txt' % K)
    if not os.path.exists(p):
        raise SystemExit('%s: no manifest for k=%d (render with --manifest-k %d)' % (d, K, K))
    out = []
    for line in open(p, encoding='utf-8'):
        if not line.startswith('triple'):
            continue
        f = dict(x.split('=') for x in line.split()[2:])
        idx = lambda s: int(os.path.basename(s)[2:8])
        out.append({'N': idx(f['prev']), 'mid': idx(f['mid']), 'N1': idx(f['next']), 'phase': float(f['phase'])})
    return out


# ── masks and geometry ───────────────────────────────────────────────────────────────────────────
STRIP = None            # (x0, x1, y0, y1) of a baked barcode strip, set per corpus; masked out of every term


SIL_MODE = 'tau'    # 'tau' = the 2026-09-08 operator (default, byte-identical) | 'coverage' = --silhouette coverage
COV_ANY = 0.004     # |rgb - bg| above this = the pixel carries SOME object (the backdrop is the exact render: 0 elsewhere)


def box3(x):
    """3x3 neighbourhood sum with edge replication (2-D, or 3-D channels-last)."""
    p = np.pad(x, ((1, 1), (1, 1)) + ((0, 0),) * (x.ndim - 2), mode='edge')
    out = np.zeros_like(x, dtype=np.float64)
    H, W = x.shape[:2]
    for dy in (0, 1, 2):
        for dx in (0, 1, 2):
            out += p[dy:dy + H, dx:dx + W]
    return out


def object_like(rgb, bg):
    """The ONE silhouette operator, applied to truth and candidate alike (the gate's own correction: a truth scored
    against itself must be exactly zero, so both sides go through the same function).

    'tau' (default): a pixel is object where any channel differs from the exact backdrop render by more than OBJ_TAU.
    Its boundary is where the anti-aliased edge coverage crosses OBJ_TAU / |object - backdrop|, which depends on the
    backdrop NOISE under the edge -- at x1 the truth and the candidate sit over the same backdrop and the flip cancels;
    at x8 (13 px apart) it does not: the x8/x16 gates of 2026-09-10 went red on T2/T3/T4 for this reason alone (the
    nearest arm's residual 2.2 px, the exact-flow oracle 0.9-1.3 px of shape).
    'coverage' (--silhouette coverage): the boundary is the HALF-COVERAGE contour. Interior pixels (any coverage,
    eroded twice) are object; an edge pixel is object when its coverage a > 0.5, where a solves
    rgb = a*obj + (1-a)*bg with obj = the mean colour of the interior pixels within 2 px (the object colour that
    edge pixel is a partial sample of). The same estimator on both sides, and it no longer depends on what the
    backdrop is under the edge."""
    d = np.abs(rgb - bg).max(axis=-1)
    if SIL_MODE == 'tau':
        m = d > OBJ_TAU
    else:
        any_cov = d > COV_ANY
        interior = erode4(erode4(any_cov))
        w = interior.astype(np.float64)
        num = box3(box3(rgb * w[..., None]))          # 5x5 sums via two 3x3 passes
        den = box3(box3(w))
        has = den > 0
        obj = np.where(has[..., None], num / np.maximum(den, 1e-9)[..., None], rgb)
        v = obj - bg
        u = rgb - bg
        a = (u * v).sum(-1) / np.maximum((v * v).sum(-1), 1e-9)
        m = interior | (any_cov & has & (a > 0.5))
    if STRIP is not None:
        x0, x1, y0, y1 = STRIP
        m[y0:y1, x0:x1] = False
    return m


def erode4(m):
    e = m.copy()
    e[1:] &= m[:-1]; e[:-1] &= m[1:]; e[:, 1:] &= m[:, :-1]; e[:, :-1] &= m[:, 1:]
    return e


def dilate(m, r):
    out = m.copy()
    for _ in range(int(r)):
        d = out.copy()
        d[1:] |= out[:-1]; d[:-1] |= out[1:]; d[:, 1:] |= out[:, :-1]; d[:, :-1] |= out[:, 1:]
        out = d
    return out


def boundary_pts(m):
    b = m & ~erode4(m)
    ys, xs = np.nonzero(b)
    return np.stack([xs + 0.5, ys + 0.5], axis=1)


def chamfer(P, Q):
    """Symmetric mean nearest-point distance between two boundary point sets."""
    if len(P) == 0 or len(Q) == 0:
        return float('nan')
    def one_way(A, B):
        out = np.empty(len(A))
        for i in range(0, len(A), 512):
            a = A[i:i + 512]
            d2 = ((a[:, None, :] - B[None, :, :]) ** 2).sum(-1)
            out[i:i + 512] = np.sqrt(d2.min(axis=1))
        return out.mean()
    return 0.5 * (one_way(P, Q) + one_way(Q, P))


def centroid(m):
    ys, xs = np.nonzero(m)
    return np.array([xs.mean() + 0.5, ys.mean() + 0.5]) if len(xs) else np.array([np.nan, np.nan])


def grad_mag(g):
    gx = np.zeros_like(g); gy = np.zeros_like(g)
    gx[:, 1:-1] = 0.5 * (g[:, 2:] - g[:, :-2]); gy[1:-1] = 0.5 * (g[2:] - g[:-2])
    return np.hypot(gx, gy)


# ── scoring one frame ────────────────────────────────────────────────────────────────────────────
def touches_border(m):
    """True when a silhouette mask reaches any edge of the image: its centroid is then the VISIBLE part's, not the
    object's, and no term computed from it is exact. At --speed 8 the `mixed` sphere is partially outside the view in
    24 of 240 frames (seen 2026-09-10: T2/T3/T4 red on that corpus for this reason alone)."""
    return bool(m[0, :].any() or m[-1, :].any() or m[:, 0].any() or m[:, -1].any())


def score_frame(cand, truth, bg, ids, cls, motion, nearest, ids_near, ids_ab=(), masks=None):
    """All terms for one intermediate frame.

    masks     an optional dict. When given, it is filled with the boolean masks the object terms are
              computed FROM — `halluc` (candidate silhouette where the truth's is absent, the union over
              objects) and `missing` (the truth's where the candidate's is absent) — so a viewer can show
              the operator WHERE the mass the number counts actually sits. The masks are the scorer's own,
              taken inside each object's window, never recomputed by an approximation elsewhere. Passing
              nothing changes no behaviour and no output (`score_arm` never passes it).

    motion[k] = (unit direction, displacement per SOURCE pair in px) of object k.
    ids_near  = the exact id plane of the nearer REAL frame; with it each object also carries
                pred_travel_px = the truth centroid's own travel between mid and that frame, on the
                SAME support the candidate silhouette is measured on — the closed form the `nearest`
                arm must reproduce.
    ids_ab    = the exact id planes of the two real frames the candidate was generated from. A pixel that is
                ANOTHER object in the mid or in either real is outside object k's window: the operator is
                object-like-vs-backdrop and cannot tell a blue box from a red sphere, so where a mover passes in
                front of a static object the near real shows that object's pixels exactly where the mid had the
                mover (fast_train x8, mid 50: nearest's centroid 6 px short of the exact travel; mixed at x2/x4
                reaches the box the same way). Excluding those pixels on BOTH sides keeps the truth at zero and
                the rulers honest; the object's silhouette is then overlap-clipped, consistently.
    An object whose exact silhouette touches the image border in the mid truth or in the nearer real frame
    carries NO object terms in that frame (row['clipped'] lists it): a clipped centroid is not the object's,
    and a fast mover spends part of every crossing at the edges.
    """
    tlike, clike = object_like(truth, bg), object_like(cand, bg)
    valid = np.ones(ids.shape, bool)              # the barcode strip is outside EVERY term, not only the masks
    if STRIP is not None:
        x0, x1, y0, y1 = STRIP
        valid[y0:y1, x0:x1] = False
    row = {'mask_iou': float((tlike & (ids > 0)).sum() / max((tlike | (ids > 0)).sum(), 1))}
    ct, cc = lum(truth), lum(cand)
    det = (cls == 3) & valid
    row['l2_det'] = float(np.sqrt(((cc - ct)[det] ** 2).mean())) if det.any() else float('nan')
    far_bg = (ids == 0) & ~dilate(ids > 0, NEAR_R) & valid
    row['bg_err'] = float(np.sqrt(((cc - ct)[far_bg] ** 2).mean())) if far_bg.any() else float('nan')
    d0 = (cls == 0) & valid
    row['disocc_px'] = int(d0.sum())
    row['disocc'] = float(np.sqrt(((cc - ct)[d0] ** 2).mean())) if d0.any() else float('nan')
    row['graceful'] = float(np.sqrt(((cc - lum(nearest))[d0] ** 2).mean())) if d0.any() else float('nan')
    tb = (ids > 0) & ~erode4(ids > 0)
    tband = dilate(tb, 1) & valid
    gt, gc = grad_mag(ct), grad_mag(cc)
    row['sharp'] = float(gc[tband].mean() / max(gt[tband].mean(), 1e-9)) if tband.any() else float('nan')
    per = {}
    clipped = []
    if masks is not None:
        masks['halluc'] = np.zeros(ids.shape, bool)
        masks['missing'] = np.zeros(ids.shape, bool)
        masks['truth_obj'] = np.zeros(ids.shape, bool)   # the truth silhouettes themselves, for an INTERIOR view:
        # every object term is a silhouette term, so a warp that keeps the outline and scrambles what is inside it
        # (a deformed checker, a smeared texture) scores at the floor. The mask is exported so a viewer can show
        # that error where it lives; it enters no term and no verdict.
    for k, (vn, disp) in motion.items():
        tm = ids == k
        if not tm.any():
            continue
        # only a MOVING object can be clipped by the view: a static one that spans the frame (mixed's occluder
        # quad covers the full height by design) has the same visible centroid in the mid and in the nearer real
        if disp > 0.5 and (touches_border(tm) or touches_border(ids_near == k)):
            clipped.append(int(k))
            continue
        others = (ids > 0) & (ids != k) & (ids != 255)
        for ida in ids_ab:
            others |= (ida > 0) & (ida != k) & (ida != 255)
        # class-0 pixels are the disocclusion bucket: reported apart, NEVER inside an object term --
        # a candidate is free to fill them with the nearest real frame (the graceful answer) without
        # that showing up here as missing or hallucinated mass
        reach = dilate(tm, NEAR_R + int(np.ceil(disp)))
        win = reach & ~others & (cls != 0)
        tsil, csil = tlike & win, clike & win                 # ONE operator, both sides
        o = {'area_px': int(tsil.sum()), 'perim_px': int(len(boundary_pts(tsil))), 'disp_src_px': float(disp),
             'overlap_px': int((reach & others).sum())}   # another object's pixels inside this object's reach (mid, A or B)
        if not tsil.any():
            continue
        near_m = (ids_near == k) & win
        o['pred_travel_px'] = float(np.linalg.norm(centroid(tm & win) - centroid(near_m))) if near_m.any() else float('nan')
        o['pos_err'] = float(np.linalg.norm(centroid(csil) - centroid(tsil))) if csil.any() else float('nan')
        o['shape_err'] = chamfer(boundary_pts(csil), boundary_pts(tsil)) if csil.any() else float('nan')
        hal = csil & ~tsil
        mis = tsil & ~csil
        o['halluc_px'] = int(hal.sum())
        o['missing_px'] = int(mis.sum())
        if masks is not None:
            masks['halluc'] |= hal
            masks['missing'] |= mis
            masks['truth_obj'] |= tsil
        if hal.any():
            ys, xs = np.nonzero(hal)
            o['lead_px'] = float(((np.stack([xs + 0.5, ys + 0.5], axis=1) - centroid(tsil)) @ vn).mean())
        else:
            o['lead_px'] = 0.0
        per[k] = o
    row['objects'] = per
    row['clipped'] = clipped
    return row


def score_cut_frame(cand, A, B, idsA, idsB, bg, t):
    """One frame generated ACROSS A CUT (its two real frames were not k apart -- the loop seam, or a
    bridged drop), scored against BOTH real endpoints it bridged. There is no interpolation truth for
    a cut, so there is no pos_err / shape_err / lead_px here: only what the candidate IS relative to
    each side, with the SAME primitives score_frame uses for a continuous mid -- object_like for both
    silhouettes, the same edge-ratio helper for sharpness, an RMS distance for how far it fell from the
    nearer side (score_frame's `graceful` restricts that RMS to the class-0 disocclusion bucket; a cut
    has no exact-truth id plane to build that bucket from, so this reuses the same RMS-to-nearest-real
    form over every non-strip pixel instead).

    idsA, idsB = the exact id planes of the two real endpoints (class-0 is not applicable: neither
    endpoint has an undetermined bucket, each IS a real frame).
    """
    valid = np.ones(idsA.shape, bool)
    if STRIP is not None:
        x0, x1, y0, y1 = STRIP
        valid[y0:y1, x0:x1] = False
    clike, alike, blike = object_like(cand, bg), object_like(A, bg), object_like(B, bg)
    cc, ca, cb = lum(cand), lum(A), lum(B)
    row = {'l2_vs_A': float(np.sqrt(((cc - ca)[valid] ** 2).mean())),
           'l2_vs_B': float(np.sqrt(((cc - cb)[valid] ** 2).mean()))}
    per = {}
    for k in sorted(int(x) for x in set(np.unique(idsA)) | set(np.unique(idsB)) if 0 < x < 255):
        o = {}
        mA = idsA == k
        if mA.any():
            winA = dilate(mA, NEAR_R) & ~((idsA > 0) & (idsA != k) & (idsA != 255))
            tsil, csil = alike & winA, clike & winA
            o['halluc_vs_A'] = int((csil & ~tsil).sum())
            o['missing_vs_A'] = int((tsil & ~csil).sum())
        mB = idsB == k
        if mB.any():
            winB = dilate(mB, NEAR_R) & ~((idsB > 0) & (idsB != k) & (idsB != 255))
            tsil, csil = blike & winB, clike & winB
            o['halluc_vs_B'] = int((csil & ~tsil).sum())
            o['missing_vs_B'] = int((tsil & ~csil).sum())
        if o:
            per[k] = o
    row['objects'] = per
    tbandA = dilate((idsA > 0) & ~erode4(idsA > 0), 1) & valid
    tbandB = dilate((idsB > 0) & ~erode4(idsB > 0), 1) & valid
    gc, ga, gb = grad_mag(cc), grad_mag(ca), grad_mag(cb)
    row['sharp_vs_A'] = float(gc[tbandA].mean() / max(ga[tbandA].mean(), 1e-9)) if tbandA.any() else float('nan')
    row['sharp_vs_B'] = float(gc[tbandB].mean() / max(gb[tbandB].mean(), 1e-9)) if tbandB.any() else float('nan')
    row['nearer'] = nearer = 'A' if t <= 0.5 else 'B'
    row['graceful'] = row['l2_vs_A'] if nearer == 'A' else row['l2_vs_B']
    return row


def _cut_mean(row, key):
    """Mean of one per-object term over the objects that carry it (an object absent from one side
    of the cut carries only the other side's terms -- see score_cut_frame)."""
    vals = [o[key] for o in row['objects'].values() if key in o]
    return float(np.nanmean(vals)) if vals else float('nan')


def _cut_vs_nearer(row, prefix):
    return _cut_mean(row, '%s_vs_%s' % (prefix, row['nearer']))


# ── synthetic arms ───────────────────────────────────────────────────────────────────────────────
def gauss_blur(rgb, sigma=1.2):
    r = int(np.ceil(3 * sigma)); x = np.arange(-r, r + 1); k = np.exp(-0.5 * (x / sigma) ** 2); k /= k.sum()
    out = rgb.copy()
    for ax in (0, 1):
        acc = np.zeros_like(out)
        for i, w in enumerate(k):
            acc += w * np.roll(out, i - r, axis=ax)
        out = acc
    return out


def bilinear(img, uv):
    H, W = img.shape[:2]
    u = np.clip(uv[..., 0] - 0.5, 0, W - 1.001); v = np.clip(uv[..., 1] - 0.5, 0, H - 1.001)
    x0, y0 = np.floor(u).astype(int), np.floor(v).astype(int)
    fx, fy = (u - x0)[..., None], (v - y0)[..., None]
    return (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy)
            + img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)


SYNTH = ('truth', 'nearest', 'blend', 'oracle2', 'blur')


def synth_frame(t, sc, d, tr, name):
    """One synthetic arm's frame for one triple, computed in memory from the corpus alone.

    These are pure functions of the corpus, so they are NOT stored by default (a full sweep would
    otherwise write ~900 MB of recomputable pixels per k); --keep-arms materialises them for
    consumers that read files. Nothing is lost: the bytes are identical either way.
    """
    W, H = t['width'], t['height']
    fr = lambda i: load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % i), W, H)
    truth = fr(tr['mid'])
    if name == 'truth':
        return truth
    A, B = fr(tr['N']), fr(tr['N1'])
    if name == 'nearest':
        return A if tr['phase'] <= 0.5 else B
    if name == 'blend':
        return 0.5 * (A + B)
    if name == 'blur':
        return gauss_blur(truth)
    if name == 'oracle2':
        tm, tA, tB = t['t'][tr['mid']], t['t'][tr['N']], t['t'][tr['N1']]
        k, _, L = sc.labels(tm)
        cls = sc.visibility(tm, tA, tB)
        uvA, _ = sc.reproject(k, L, tA); uvB, _ = sc.reproject(k, L, tB)
        return np.where((cls & 1).astype(bool)[..., None], bilinear(A, uvA),
                        np.where((cls & 2).astype(bool)[..., None], bilinear(B, uvB),
                                 A if tr['phase'] <= 0.5 else B))
    raise KeyError(name)


def arm_frame(t, sc, d, tr, name):
    """A candidate frame: a materialised arms/<name>/ file if present (the FG's own output lives
    there), else a synthetic arm in memory, else None."""
    p = os.path.join(d, 'arms', name, 'f_%06d.rgba' % tr['mid'])
    if os.path.exists(p):
        return load_rgb(p, t['width'], t['height'])
    if name in SYNTH:
        return synth_frame(t, sc, d, tr, name)
    return None


def make_arms(d, K, names):
    """Materialise synthetic arms to arms/<name>/ (--keep-arms). Same bytes the scorer uses."""
    t, sc = load_corpus(d)
    names = [n for n in names if n in SYNTH]
    for n in names:
        os.makedirs(os.path.join(d, 'arms', n), exist_ok=True)
    for tr in triples(d, K):
        for n in names:
            Z.to_rgba(synth_frame(t, sc, d, tr, n)).tofile(os.path.join(d, 'arms', n, 'f_%06d.rgba' % tr['mid']))
    print('%s: arms %s materialised for k=%d' % (d, ', '.join(names), K))


def u8(x):
    return np.clip(np.rint(x * 255), 0, 255).astype(np.uint8)


def png(path, rgb8):
    """Lossless PNG from RGB8, stdlib only — for the pages; the corpus itself stays raw RGBA8."""
    import struct, zlib
    H, W = rgb8.shape[:2]
    raw = b''.join(b'\x00' + rgb8[y].tobytes() for y in range(H))
    def ch(tag, dat):
        return struct.pack('>I', len(dat)) + tag + dat + struct.pack('>I', zlib.crc32(tag + dat) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
                           + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))


# ── a whole arm over a corpus at multiplier K ────────────────────────────────────────────────────
def exact_phase(d, arm):
    """mid -> the FG's OWN phase for a live arm, from align.json. Absent for synthetic arms."""
    p = os.path.join(d, 'arms', arm, 'align.json')
    if not os.path.exists(p):
        return {}
    return {r['mid']: r for r in json.load(open(p, encoding='utf-8'))['rows']
            if 'mid' in r and 'skipped' not in r and 'dup_of' not in r}


_W = {}   # per-PROCESS scoring state (the corpus, the scene, the backdrop, the arm's exact phases) — see _worker_init


def _worker_init(d, arm, sil='tau'):
    """Load what every frame of one (corpus, arm) needs, ONCE per process. Runs in the parent for --jobs 1 and in
    each pool worker otherwise (ProcessPoolExecutor initializer). The truth render per frame is the cost that
    made a 4-arm × 180-frame run take ~20 min on one core (2026-09-09, the operator: "apenas usa recursos")."""
    global _W, SIL_MODE
    SIL_MODE = sil
    t, sc = load_corpus(d)
    W, H = t['width'], t['height']
    bg = Z.Scene([], W, H, t['fov_deg'], t['seed']).render(0.0, t['ss'])[0]
    _W = {'d': d, 'arm': arm, 't': t, 'sc': sc, 'W': W, 'H': H, 'bg': bg, 'ex': exact_phase(d, arm)}


def _score_triple(args):
    """One triple of one arm -> ('row', r) | ('cut', None) | ('skip', None). The former loop body of score_arm,
    verbatim; it reads only _W and its arguments, so the same triple gives the same row in any process.

    args is (K, tr) or (K, tr, want_masks). With want_masks the row carries `_masks`, the scorer's own
    halluc / missing boolean masks for that frame (see score_frame). `score_arm` always passes the 2-tuple,
    so nothing it writes changes; only an in-process caller that asks for masks gets them, and `_masks`
    holds arrays that are never serialised into a report."""
    K, tr = args[0], args[1]
    want_masks = len(args) > 2 and bool(args[2])
    d, arm, t, sc, W, H, bg, ex = (_W[k] for k in ('d', 'arm', 't', 'sc', 'W', 'H', 'bg', 'ex'))
    cand = arm_frame(t, sc, d, tr, arm)
    if cand is None:
        return ('skip', None)
    tm, tA, tB = t['t'][tr['mid']], t['t'][tr['N']], t['t'][tr['N1']]
    if tr['mid'] in ex:
        # A LIVE arm is scored against the truth AT ITS OWN PHASE, rendered on demand: the FG's t
        # need not sit on the base grid, and comparing to the nearest base frame would charge the
        # FG up to half a base frame of motion that is the alignment's, not its own.
        e = ex[tr['mid']]
        if not e.get('pair_ok', True):
            # The two real frames were NOT k apart -- the player looped, or the FG paired across a
            # drop. What the FG bridged there is a CUT, and a cut has no interpolation truth: the
            # first live run's single catastrophic frame (2,865 px^2, a doubled sphere) was exactly
            # this, the loop seam, and was nearly recorded as a hallucination on continuous motion.
            # Counted, never scored.
            return ('cut', None)
        tA, tB = t['t'][e['N']], t['t'][e['N1']]
        tm = tA + e['t'] * (tB - tA)
        truth, (ids, _, _) = sc.render(tm, t['ss'])
        if STRIP is not None:
            Z.draw_barcode(truth, tr['mid'])          # the strip is masked anyway; keep the bytes alike
    else:
        truth = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % tr['mid']), W, H)
        ids = load_id(d, tr['mid'], W, H)
    cls = sc.visibility(tm, tA, tB)
    k, _, L = sc.labels(tm)
    uvA, _ = sc.reproject(k, L, tA); uvB, _ = sc.reproject(k, L, tB)
    fl = uvB - uvA                                          # per SOURCE pair
    motion = {}
    for kk in sorted(int(x) for x in np.unique(ids) if 0 < x < 255):
        v = fl[ids == kk].mean(axis=0); n = np.linalg.norm(v)
        motion[kk] = (v / (n + 1e-12), float(n))
    if tr['mid'] in ex:
        e = ex[tr['mid']]
        near_i = e['N'] if e['t'] <= 0.5 else e['N1']
        phase = e['t']
    else:
        near_i = tr['N'] if tr['phase'] <= 0.5 else tr['N1']
        phase = tr['phase']
    nearest = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % near_i), W, H)
    if tr['mid'] in ex:
        nA, nB = ex[tr['mid']]['N'], ex[tr['mid']]['N1']
    else:
        nA, nB = tr['N'], tr['N1']
    mk = {} if want_masks else None
    r = score_frame(cand, truth, bg, ids, cls, motion, nearest, load_id(d, near_i, W, H),
                    (load_id(d, nA, W, H), load_id(d, nB, W, H)), masks=mk)
    r.update({'mid': tr['mid'], 'near': near_i, 'phase': phase, 'k': K, 'exact_phase': tr['mid'] in ex})
    if mk is not None:
        # the truth this frame was actually scored against. For a LIVE arm it is rendered at the FG's own
        # phase and is NOT the base-grid frame at that index (up to half a base frame of motion apart), so a
        # viewer that draws the grid frame instead shows a difference the FG did not make.
        r['_masks'], r['_truth'] = mk, truth
    return ('row', r)


def score_arm(d, K, arm, frames=None, jobs=1, sil='tau'):
    """Score one arm over the corpus's triples. jobs > 1 = a process pool over the triples (each process
    loads the corpus once; the per-frame work is independent, so the rows are identical to --jobs 1 and in
    the same order — verified 2026-09-09 on g5_live, 6 frames, serial vs 8 workers: JSON-equal)."""
    trs = [(K, tr) for tr in triples(d, K)[:frames]]
    if jobs <= 1 or len(trs) < 2:
        _worker_init(d, arm, sil)
        results = [_score_triple(a) for a in trs]
    else:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=min(jobs, len(trs)), initializer=_worker_init, initargs=(d, arm, sil)) as pool:
            results = list(pool.map(_score_triple, trs, chunksize=2))
    rows = [r for kind, r in results if kind == 'row']
    cut = sum(1 for kind, _ in results if kind == 'cut')
    if cut:
        print('%s/%s k=%d: %d frame(s) whose real pair was NOT %d apart (a cut) counted and excluded'
              % (os.path.basename(os.path.normpath(d)), arm, K, cut, K))
    return rows


_WC = {}   # per-process state for score_cuts — the corpus dims and backdrop, loaded once (see _cuts_worker_init)


def _cuts_worker_init(d, sil='tau'):
    """Load what every cut needs, ONCE per process — same pattern as _worker_init, but a cut needs no
    Scene reprojection (there is no interpolation truth to reproject to), only the corpus dims and bg."""
    global _WC, SIL_MODE
    SIL_MODE = sil
    t, _ = load_corpus(d)
    W, H = t['width'], t['height']
    bg = Z.Scene([], W, H, t['fov_deg'], t['seed']).render(0.0, t['ss'])[0]
    _WC = {'d': d, 'W': W, 'H': H, 'bg': bg}


def _score_cut(args):
    """One cut row of one arm -> the full row: its own terms plus the two reference candidates (hold,
    blend) scored through the same score_cut_frame. Reads only _WC and its arguments, so the same cut
    gives the same row in any process (mirrors _score_triple)."""
    arm, row, K = args
    d, W, H, bg = (_WC[k] for k in ('d', 'W', 'H', 'bg'))
    n = row['cut']
    cand = load_rgb(os.path.join(d, 'arms', arm, 'cut_%d.rgba' % n), W, H)
    N, N1, t = row['N'], row['N1'], row['t']
    A = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % N), W, H)
    B = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % N1), W, H)
    idsA, idsB = load_id(d, N, W, H), load_id(d, N1, W, H)
    r = {'cut': n, 'triple': row['triple'], 'N': N, 'N1': N1, 't': t, 'k': K}
    r.update(score_cut_frame(cand, A, B, idsA, idsB, bg, t))
    hold_ref = A if r['nearer'] == 'A' else B
    r['hold'] = score_cut_frame(hold_ref, A, B, idsA, idsB, bg, t)
    r['blend'] = score_cut_frame(0.5 * (A + B), A, B, idsA, idsB, bg, t)
    return r


def score_cuts(d, K, arm, jobs=1, sil='tau'):
    """Score every cut row of one arm's align.json (empty for an arm with no align.json, or none marked
    'cut' -- the synthetic arms have neither). Same process-pool pattern as score_arm: a worker init that
    loads the corpus and bg once; serial when jobs <= 1 or fewer than 2 cuts."""
    p = os.path.join(d, 'arms', arm, 'align.json')
    if not os.path.exists(p):
        return []
    cuts = [r for r in json.load(open(p, encoding='utf-8'))['rows'] if 'cut' in r]
    if not cuts:
        return []
    args = [(arm, r, K) for r in cuts]
    if jobs <= 1 or len(args) < 2:
        _cuts_worker_init(d, sil)
        rows = [_score_cut(a) for a in args]
    else:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=min(jobs, len(args)), initializer=_cuts_worker_init, initargs=(d, sil)) as pool:
            rows = list(pool.map(_score_cut, args, chunksize=2))
    return rows


def summarize(rows):
    if not rows:
        return None
    f = lambda key: float(np.nanmean([r[key] for r in rows]))
    objs = sorted(set(k for r in rows for k in r['objects']))
    po = {}
    for k in objs:
        g = lambda key: float(np.nanmean([r['objects'][k][key] for r in rows if k in r['objects']]))
        po[k] = {key: g(key) for key in ('pos_err', 'shape_err', 'halluc_px', 'missing_px', 'lead_px',
                                          'disp_src_px', 'area_px', 'perim_px', 'pred_travel_px', 'overlap_px')}
    return {'n': len(rows), 'mask_iou': f('mask_iou'), 'l2_det': f('l2_det'), 'bg_err': f('bg_err'),
            'sharp': f('sharp'), 'disocc': f('disocc'), 'graceful': f('graceful'), 'disocc_px': f('disocc_px'),
            'objects': po}


def verdict(S, floor):
    """Conjunctive. `floor` = the truth arm's own summary (exactly zero on the object terms)."""
    bad = []
    for k, o in S['objects'].items():
        fo = floor['objects'].get(k, o)
        slop = 0.5 * o['perim_px']            # half a pixel of edge slop: what a resampler is owed
        if o['pos_err'] > fo['pos_err'] + 0.5: bad.append('pos')
        if o['shape_err'] > fo['shape_err'] + 0.5: bad.append('shape')
        if o['halluc_px'] > fo['halluc_px'] * 1.5 + slop: bad.append('halluc')
        if o['missing_px'] > fo['missing_px'] * 1.5 + slop: bad.append('missing')
    if S['bg_err'] > floor['bg_err'] + 0.01: bad.append('bg')
    if S['sharp'] < FLOOR_SHARP: bad.append('BLUR')
    return 'ACCEPT' if not bad else ' / '.join(sorted(set(bad)))


def table(d_label, K, arms, results, floor):
    L = ['## %s · k = %d' % (d_label, K), '',
         '| arm | pos_err px | shape_err px | halluc px² | lead px | missing px² | bg_err | sharp | l2_det | disocc (apart) | graceful | verdict |',
         '|---|---|---|---|---|---|---|---|---|---|---|---|']
    for a in arms:
        S = results[a]
        if S is None:
            L.append('| `%s` | (no frames) |' % a); continue
        po = S['objects']
        m = lambda key: float(np.nanmean([po[k][key] for k in po])) if po else float('nan')
        L.append('| `%s` | %.3f | %.3f | %.0f | %+.2f | %.0f | %.4f | %.3f | %.4f | %.4f | %.4f | %s |'
                 % (a, m('pos_err'), m('shape_err'), m('halluc_px'), m('lead_px'), m('missing_px'),
                    S['bg_err'], S['sharp'], S['l2_det'], S['disocc'], S['graceful'], verdict(S, floor)))
    return L


def cut_table(d_label, K, arm, rows):
    """One row per cut frame, plus a mean row: the terms score_cut_frame carries, and the two
    reference candidates (hold, blend) scored alongside it as a built-in sanity check the operator
    can read straight off the table (hold ≈ 0 everywhere; blend ghosts and blurs)."""
    L = ['## %s · k = %d · arm `%s` · cut frames' % (d_label, K, arm), '',
         '| cut | t | nearer | halluc vs A px² | halluc vs B px² | missing vs A px² | missing vs B px² '
         '| sharp vs A | sharp vs B | graceful | hold graceful | hold halluc | blend graceful | blend halluc |',
         '|---|---|---|---|---|---|---|---|---|---|---|---|---|---|']
    for r in rows:
        L.append('| %d | %.3f | %s | %.0f | %.0f | %.0f | %.0f | %.3f | %.3f | %.4f | %.4f | %.0f | %.4f | %.0f |'
                 % (r['cut'], r['t'], r['nearer'], _cut_mean(r, 'halluc_vs_A'), _cut_mean(r, 'halluc_vs_B'),
                    _cut_mean(r, 'missing_vs_A'), _cut_mean(r, 'missing_vs_B'), r['sharp_vs_A'], r['sharp_vs_B'],
                    r['graceful'], r['hold']['graceful'], _cut_vs_nearer(r['hold'], 'halluc'),
                    r['blend']['graceful'], _cut_vs_nearer(r['blend'], 'halluc')))
    if rows:
        f = lambda fn: float(np.nanmean([fn(r) for r in rows]))
        L.append('| mean | %.3f | — | %.0f | %.0f | %.0f | %.0f | %.3f | %.3f | %.4f | %.4f | %.0f | %.4f | %.0f |'
                 % (f(lambda r: r['t']), f(lambda r: _cut_mean(r, 'halluc_vs_A')), f(lambda r: _cut_mean(r, 'halluc_vs_B')),
                    f(lambda r: _cut_mean(r, 'missing_vs_A')), f(lambda r: _cut_mean(r, 'missing_vs_B')),
                    f(lambda r: r['sharp_vs_A']), f(lambda r: r['sharp_vs_B']), f(lambda r: r['graceful']),
                    f(lambda r: r['hold']['graceful']), f(lambda r: _cut_vs_nearer(r['hold'], 'halluc')),
                    f(lambda r: r['blend']['graceful']), f(lambda r: _cut_vs_nearer(r['blend'], 'halluc'))))
    return L


# ── the gate ─────────────────────────────────────────────────────────────────────────────────────
def gate(d, K, jobs=1, sil='tau'):
    print('GATE on %s at k=%d — five arms with predicted signatures; every check seen RED first.' % (d, K))
    arms = list(SYNTH)                      # computed in memory; nothing is written by the gate
    t, _ = load_corpus(d); W, H = t['width'], t['height']
    bg = Z.Scene([], W, H, t['fov_deg'], t['seed']).render(0.0, t['ss'])[0]
    global SIL_MODE
    SIL_MODE = sil
    print('  silhouette operator: %s' % sil)
    R = {a: score_arm(d, K, a, None, jobs, sil) for a in arms}
    S = {a: summarize(R[a]) for a in arms}
    fl = S['truth']
    fail = []
    obj = lambda a, key: float(np.nanmean([S[a]['objects'][k][key] for k in S[a]['objects']]))

    c1 = fl['mask_iou'] > 0.97 and obj('truth', 'pos_err') == 0.0 and obj('truth', 'shape_err') == 0.0 \
        and obj('truth', 'halluc_px') == 0.0 and obj('truth', 'missing_px') == 0.0 \
        and abs(fl['sharp'] - 1.0) < 1e-9 and fl['l2_det'] < 1e-12
    print('  T1 truth      : operator-vs-id IoU %.4f; pos %.4f shape %.4f halluc %.0f missing %.0f (all must be exactly 0); sharp %.6f -> %s'
          % (fl['mask_iou'], obj('truth', 'pos_err'), obj('truth', 'shape_err'), obj('truth', 'halluc_px'),
             obj('truth', 'missing_px'), fl['sharp'], c1))
    if not c1: fail.append('T1')

    # the closed form: nearest's centroid error is the truth centroid's own travel, from the exact id
    # planes, on the same support the silhouettes are measured on (score_frame carries it per object)
    pred, meas = [], []
    for r in R['nearest']:
        for k, o in r['objects'].items():
            if o['pos_err'] == o['pos_err'] and o.get('pred_travel_px', np.nan) == o.get('pred_travel_px', np.nan):
                pred.append(o['pred_travel_px']); meas.append(o['pos_err'])
    pred, meas = np.array(pred), np.array(meas)
    resid = np.abs(meas - pred)
    c2 = len(pred) > 0 and np.percentile(resid, 90) < 0.25 and resid.max() < 0.6 and pred.mean() > 0.2
    print('  T2 nearest    : pos_err vs the truth centroid\'s own travel, %d (frame,object): mean travel %.3f px, p90 |resid| %.3f, max %.3f -> %s'
          % (len(pred), pred.mean() if len(pred) else float('nan'),
             np.percentile(resid, 90) if len(pred) else float('nan'), resid.max() if len(pred) else float('nan'), c2))
    if not c2: fail.append('T2')

    # the inversion in its pure form: at phase 0.5 the blend's centroid is EXACT by symmetry -- a
    # position-only verdict would accept the classic ghost. The conjunctive one must not.
    # ... on the (frame, object) pairs no OTHER object reaches into: a mover passing in front of a static object
    # clips the two displaced ghosts asymmetrically (2026-09-10, x2/x4/x8) and the symmetry premise stops holding.
    # The excluded pairs are counted; the FG's own rows keep them (candidate and truth are clipped alike).
    def free(rows):
        return [(r, k, o) for r in rows for k, o in r['objects'].items() if o.get('overlap_px', 0) == 0]
    half_rows = [r for r in R['blend'] if abs(r['phase'] - 0.5) < 1e-9] or R['blend']
    half = summarize(half_rows) or S['blend']
    f3 = free(half_rows)
    n3_all = sum(len(r['objects']) for r in half_rows)
    p3 = float(np.nanmean([o['pos_err'] for _, _, o in f3])) if f3 else float('nan')
    h3 = float(np.nanmean([o['halluc_px'] for _, _, o in f3])) if f3 else float('nan')
    v3 = verdict(half, fl)
    c3 = bool(f3) and p3 < 0.15 and h3 > 0 and v3 != 'ACCEPT'
    print('  T3 overlap    : %d of %d (frame,object) pairs at phase 1/2 are free of other objects; T3 and T5 read those'
          % (len(f3), n3_all))
    print('  T3 blend φ=½  : pos %.3f (exact by symmetry — position alone would accept), halluc %.0f px², sharp %.3f, verdict %s -> %s'
          % (p3, h3, half['sharp'], v3, c3))
    if not c3: fail.append('T3')

    v4 = verdict(S['oracle2'], fl)
    c4 = v4 == 'ACCEPT' and (S['oracle2']['disocc'] != S['oracle2']['disocc'] or S['oracle2']['disocc'] > 0.005)
    print('  T4 oracle2    : verdict %s on the determinable; disocc bucket %.4f over %.0f px carries the only error -> %s'
          % (v4, S['oracle2']['disocc'], S['oracle2']['disocc_px'], c4))
    if not c4: fail.append('T4')

    v5 = verdict(S['blur'], fl)
    f5 = free(R['blur'])
    p5 = float(np.nanmean([o['pos_err'] for _, _, o in f5])) if f5 else obj('blur', 'pos_err')
    c5 = 'BLUR' in v5 and p5 < 0.3
    print('  T5 blur       : sharp %.3f, pos %.3f, verdict %s (BLUR must be flagged; pos ≈ 0) -> %s'
          % (S['blur']['sharp'], p5, v5, c5))
    if not c5: fail.append('T5')

    # T6: a cut candidate, built from the corpus's own loop seam (frame 0 and frame n-4=236 — the
    # SAME two real frames sc_live's live arm bridges across its seam), scored with no align.json in
    # the loop at all. hold (the nearer real, unchanged) must show pure identity; blend (the classic
    # ghost) must show the same hallucination + blur signature score_cut_frame is built to catch.
    A6 = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % 0), W, H)
    B6 = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % (t['frames'] - 4)), W, H)
    idsA6, idsB6 = load_id(d, 0, W, H), load_id(d, t['frames'] - 4, W, H)
    hold6 = score_cut_frame(A6, A6, B6, idsA6, idsB6, bg, 0.0)
    c6h = hold6['graceful'] == 0.0 and _cut_mean(hold6, 'halluc_vs_A') == 0.0 and abs(hold6['sharp_vs_A'] - 1.0) < 1e-9
    print('  T6 cut hold   : graceful %.9f, halluc_vs_A %.0f, sharp_vs_A %.9f (all exact identity) -> %s'
          % (hold6['graceful'], _cut_mean(hold6, 'halluc_vs_A'), hold6['sharp_vs_A'], c6h))
    blend6 = score_cut_frame(0.5 * (A6 + B6), A6, B6, idsA6, idsB6, bg, 0.0)
    c6b = _cut_mean(blend6, 'halluc_vs_A') > 0 and _cut_mean(blend6, 'halluc_vs_B') > 0 and blend6['sharp_vs_A'] < 0.9
    print('  T6 cut blend  : halluc_vs_A %.0f, halluc_vs_B %.0f, sharp_vs_A %.3f (BLUR expected) -> %s'
          % (_cut_mean(blend6, 'halluc_vs_A'), _cut_mean(blend6, 'halluc_vs_B'), blend6['sharp_vs_A'], c6b))
    c6 = c6h and c6b
    if not c6: fail.append('T6')

    print()
    print('\n'.join(table(os.path.basename(os.path.normpath(d)), K, arms, S, fl)))
    print()
    if fail:
        sys.exit('GATE FAILED: %s' % ', '.join(fail))
    print('GATE PASSED (T1..T6).')


def main():
    ap = argparse.ArgumentParser(description='scene_truth — score candidate intermediates against exact truth')
    ap.add_argument('--run', action='append', required=True, metavar='DIR', help='corpus dir; twice for DI-3')
    ap.add_argument('--k', type=int, action='append', default=None, help='multiplier(s); default 4')
    ap.add_argument('--arm', action='append', default=None, help='arm dir name(s) under <corpus>/arms/')
    ap.add_argument('--keep-arms', '--make-arms', dest='keep_arms', action='store_true',
                    help='materialise the synthetic arms to arms/<name>/ (off by default: they are '
                         'computed in memory and are pure functions of the corpus)')
    ap.add_argument('--gate', action='store_true', help='run the red-first gate on the first --run')
    ap.add_argument('--cuts', action='store_true',
                    help='also score and print the cut frames (align.json rows with no interpolation '
                         'truth) of every --arm that has any; off by default')
    ap.add_argument('--frames', type=int, default=None)
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 2),
                    help='worker processes over the frames (default: cores - 2, COMPUTE_DISCIPLINE); 1 = serial')
    ap.add_argument('--silhouette', choices=['tau', 'coverage'], default='tau',
                    help="the silhouette operator (object_like): 'tau' = the 2026-09-08 threshold (default, every recorded row); "
                         "'coverage' = the half-coverage contour that does not depend on the backdrop under the edge "
                         "(2026-09-10, for displacements above ~7 px per pair)")
    ap.add_argument('--md'); ap.add_argument('--json')
    a = ap.parse_args()
    if a.jobs > 1:
        # one BLAS/OpenMP thread per worker: the parallelism is across frames, never inside one (an oversubscribed
        # pool is slower than the serial loop it replaces). Spawned workers inherit the environment.
        for v in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS'):
            os.environ.setdefault(v, '1')
    ks = a.k or [4]
    if a.gate:
        gate(a.run[0], ks[0], a.jobs, a.silhouette); return
    arms = a.arm or ['truth', 'nearest', 'blend', 'oracle2', 'blur']
    L = ['# scene_truth report', '', 'silhouette operator: `%s`' % a.silhouette, '']
    out = {}
    for d in a.run:
        for K in ks:
            if a.keep_arms:
                make_arms(d, K, arms)
            R = {x: score_arm(d, K, x, a.frames, a.jobs, a.silhouette) for x in arms}
            S = {x: summarize(R[x]) for x in arms}
            fl = S.get('truth') or next(v for v in S.values() if v)
            L += table(os.path.basename(os.path.normpath(d)), K, arms, S, fl) + ['']
            entry = {'summary': S, 'rows': R}
            if a.cuts:
                cuts_out = {}
                for x in arms:
                    cut_rows = score_cuts(d, K, x, a.jobs, a.silhouette)
                    if cut_rows:
                        cuts_out[x] = cut_rows
                        L += cut_table(os.path.basename(os.path.normpath(d)), K, x, cut_rows) + ['']
                if cuts_out:
                    entry['cuts'] = cuts_out
            out['%s|k%d' % (d, K)] = entry
    if len(a.run) > 1:
        L += ['DI-3: two corpora given; compare the per-arm rows above run against run. '
              'A term whose two runs disagree by more than 20 % carries no verdict.', '']
    L += ['*Generated by tools/scene_truth/scene_report.py — Made with my soul - Swately <3*']
    print('\n'.join(L))
    if a.md:
        open(a.md, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    if a.json:
        json.dump(out, open(a.json, 'w', encoding='utf-8'), indent=1, default=float)


if __name__ == '__main__':
    main()
