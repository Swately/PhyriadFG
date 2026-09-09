#!/usr/bin/env python3
"""scene_truth — a 3-D scene rendered EXACTLY at any instant, so the FG's intermediates have a truth.

WHY A RENDERER, AND WHY OUR OWN
--------------------------------
The FG is measured by the held-out-frame protocol: give it frames N and N+1, ask it for the frame
in between, compare against the frame that really belongs there. On a game or a video that frame
does not exist. Here it does: the scene is a closed-form function of continuous time, so the frame at
t = N + 0.37 is rendered the same way as the frame at t = N, and the "digital slow-motion camera" the
operator asked for is this function evaluated wherever the FG claims to be.

The renderer is ours and not Blender's for ONE rule that the whole measurement rests on, learnt the
hard way in A0: **the frames the FG is given and the truth it is scored against must come from the
same render, with the same settings, differing only in t.** A0 died because render scale changed the
texture LOD, not just the aliasing — the "truth" and the "input" were different pictures of the same
scene. A rasteriser we wrote cannot drift like that; a black box has to be frozen and audited.

WHAT THE 3-D SCENE BUYS OVER 2-D FIGURES — the labels come free
----------------------------------------------------------------
Every pixel of every frame carries, besides colour:
    id       which object the pixel sees (0 = the textured backdrop)
    depth    camera-space z of the surface point
    flow     where that SAME material point lands one base frame later (fwd) and earlier (bwd),
             in pixels, exact from the geometry — not estimated
and, for any held-out triple (A, t, B), a per-pixel VISIBILITY class computed by re-projecting the
material point seen at t into frames A and B and checking it against their own depth:
    3 = visible in both     2 = only in B     1 = only in A     0 = in NEITHER
Class 0 is the disocclusion bucket: content the FG could not have known. The operator's own criterion
for a perfect FG — "for everything the frames determine, be exact; for everything else fail
gracefully and never invent" — becomes a per-pixel partition, and the scorer keeps the two apart.

CONVENTIONS (stated once; the scorer reproduces them rather than re-deriving them)
-----------------------------------------------------------------------------
    camera   at the origin, looking down +z; x right, y DOWN (image convention); fixed by default
    pixel    (i, j) has its centre at (i + 0.5, j + 0.5); projection u = f·x/z + W/2, v = f·y/z + H/2
    depth    camera-space z of the hit, NOT distance along the ray
    flow     flow_fwd[j, i] = pixel position of this material point at t + 1/fps, minus (i+.5, j+.5)
             flow_bwd is the same toward t − 1/fps.  Units: pixels. +x right, +y down.
    shading  Lambert + ambient, one fixed directional light, NO shadows, NO motion blur — the truth is
             sharp on purpose: blur is a post-generation filter, and an FG that blurs is scored as
             wrong, not as close (the operator's F2)
    AA       ss×ss supersampling with a box filter, ss ODD so the centre subsample IS the pixel centre
             (the aa_zoo lesson); labels are taken from that centre subsample

THE VERIFY GATE — seen red first, like every zoo in this repo
--------------------------------------------------------------
    G1 determinism   the same frame rendered twice is byte-identical
    G2 flow          the emitted flow is checked INDEPENDENTLY: the material point seen at t is
                     projected into t+dt, the hit that frame t+dt itself records at that pixel is
                     projected back to t, and the round trip must land on the original pixel
    G3 visibility    on a STATIC scene class 0 must be exactly empty (green side); on the `cross`
                     scene — two occluders passing each other — it must be non-empty (red side),
                     because the backdrop between them is covered by one at A and the other at B
    G4 coverage      every pixel sees something; there is no sky
Every one of these was watched failing under a deliberate perturbation before it was trusted.

Made with my soul - Swately <3
"""
import argparse, json, os, sys
import numpy as np
# reconfigure, never re-wrap: a second TextIOWrapper over the same buffer closes it when the first is
# collected, and this module is IMPORTED by the scorer, which has already set its own stdout
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'motion_truth'))
from marker_zoo import write_bmp, draw_barcode, decode_barcode   # noqa: E402  (proven primitives, reused)
from marker_zoo import BC_X0, BC_Y0, BC_Y1, BC_W                # noqa: E402  (the strip the scorer must mask)

EPS = 1e-6


# ── rigid transforms ─────────────────────────────────────────────────────────────────────────────
def axis_angle(axis, deg):
    a = np.asarray(axis, np.float64)
    n = np.linalg.norm(a)
    if n < EPS:
        return np.eye(3)
    a = a / n
    th = np.deg2rad(deg)
    c, s = np.cos(th), np.sin(th)
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + s * K + (1 - c) * (K @ K)


def apply(R, tv, P):                       # local -> world
    return P @ R.T + tv


def unapply(R, tv, P):                     # world -> local
    return (P - tv) @ R


# ── procedural albedo ────────────────────────────────────────────────────────────────────────────
def _hash01(ix, iy, seed):
    h = (ix.astype(np.uint32) * np.uint32(73856093)) ^ (iy.astype(np.uint32) * np.uint32(19349663)) \
        ^ np.uint32(seed * 83492791 & 0xffffffff)
    h ^= h >> np.uint32(13); h *= np.uint32(0x5bd1e995); h ^= h >> np.uint32(15)
    return (h & np.uint32(0xffffff)).astype(np.float64) / float(0xffffff)


def noise2(x, y, seed, octaves=3, base=1.0):
    """Smooth aperiodic value noise as a FUNCTION of coordinates (not an image) — continuous in t."""
    acc = np.zeros_like(x); amp, tot = 1.0, 0.0
    for o in range(octaves):
        fx, fy = x * base * (2 ** o), y * base * (2 ** o)
        ix, iy = np.floor(fx), np.floor(fy)
        tx, ty = fx - ix, fy - iy
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
        ix, iy = ix.astype(np.int64), iy.astype(np.int64)
        a = _hash01(ix, iy, seed + o); b = _hash01(ix + 1, iy, seed + o)
        c = _hash01(ix, iy + 1, seed + o); d = _hash01(ix + 1, iy + 1, seed + o)
        acc += amp * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)
        tot += amp; amp *= 0.5
    return acc / tot


# ── objects ──────────────────────────────────────────────────────────────────────────────────────
class Obj:
    """One rigid body: a shape in its own frame, a closed-form pose(t), a procedural albedo."""

    def __init__(self, spec, idx, seed):
        self.spec, self.idx, self.seed = spec, idx, seed
        self.shape = spec['shape']
        self.size = np.asarray(spec['size'], np.float64)
        self.pos0 = np.asarray(spec.get('pos', [0, 0, 6]), np.float64)
        self.vel = np.asarray(spec.get('vel', [0, 0, 0]), np.float64)
        self.axis = np.asarray(spec.get('axis', [0, 1, 0]), np.float64)
        self.omega = float(spec.get('omega_deg', 0.0))
        self.rot0 = float(spec.get('rot0_deg', 0.0))
        self.albedo = np.asarray(spec.get('albedo', [0.8, 0.8, 0.8]), np.float64)
        self.tex = spec.get('tex', 'flat')
        self.tex_freq = float(spec.get('tex_freq', 4.0))

    def pose(self, t):
        return axis_angle(self.axis, self.rot0 + self.omega * t), self.pos0 + self.vel * t

    def intersect(self, o, d):
        """Nearest hit in LOCAL space. Returns (t_hit with inf for miss, local normal)."""
        n = np.zeros_like(d); th = np.full(o.shape[0], np.inf)
        if self.shape == 'sphere':
            r = float(self.size[0])
            b = np.einsum('ij,ij->i', o, d); c = np.einsum('ij,ij->i', o, o) - r * r
            disc = b * b - c
            ok = disc > 0
            t = -b - np.sqrt(np.where(ok, disc, 0))
            ok &= t > EPS
            th[ok] = t[ok]
            P = o[ok] + t[ok, None] * d[ok]
            n[ok] = P / r
        elif self.shape == 'box':
            h = self.size
            with np.errstate(divide='ignore', invalid='ignore'):
                inv = 1.0 / d
                t1 = (-h - o) * inv; t2 = (h - o) * inv
            zero = np.abs(d) < EPS
            inside = np.abs(o) <= h
            t1 = np.where(zero, np.where(inside, -np.inf, np.inf), t1)
            t2 = np.where(zero, np.where(inside, np.inf, -np.inf), t2)
            tn, tf = np.minimum(t1, t2), np.maximum(t1, t2)
            tmin, tmax = tn.max(axis=1), tf.min(axis=1)
            ok = (tmax > np.maximum(tmin, EPS)) & (tmin > EPS)
            th[ok] = tmin[ok]
            ax = tn.argmax(axis=1)
            sgn = -np.sign(d[np.arange(len(d)), ax])
            nn = np.zeros_like(d); nn[np.arange(len(d)), ax] = sgn
            n[ok] = nn[ok]
        elif self.shape == 'quad':
            hx, hy = float(self.size[0]), float(self.size[1])
            with np.errstate(divide='ignore', invalid='ignore'):
                t = -o[:, 2] / d[:, 2]
            P = o + t[:, None] * d
            ok = (np.abs(d[:, 2]) > EPS) & (t > EPS) & (np.abs(P[:, 0]) <= hx) & (np.abs(P[:, 1]) <= hy)
            th[ok] = t[ok]
            n[ok] = np.stack([np.zeros(ok.sum()), np.zeros(ok.sum()), -np.sign(d[ok, 2])], axis=1)
        else:
            raise ValueError(self.shape)
        return th, n

    def colour(self, L):
        """Albedo as a function of LOCAL coordinates — so it rides with the object exactly."""
        if self.tex == 'flat':
            return np.broadcast_to(self.albedo, L.shape).copy()
        if self.tex == 'checker':
            par = (np.floor(L[:, 0] * self.tex_freq) + np.floor(L[:, 1] * self.tex_freq)
                   + np.floor(L[:, 2] * self.tex_freq)) % 2
            return self.albedo[None, :] * (0.55 + 0.45 * par)[:, None]
        if self.tex == 'noise':
            v = noise2(L[:, 0], L[:, 1], self.seed + 17 * self.idx, base=self.tex_freq)
            return self.albedo[None, :] * (0.45 + 0.55 * v)[:, None]
        raise ValueError(self.tex)


# ── the scene ────────────────────────────────────────────────────────────────────────────────────
PRESETS = {
    # the textured backdrop is object 0 in every scene, static, far enough to fill the view
    'static':    [{'shape': 'sphere', 'size': [0.7], 'pos': [-1.2, 0.2, 6], 'albedo': [0.9, 0.35, 0.3]},
                  {'shape': 'box', 'size': [0.6, 0.6, 0.6], 'pos': [1.3, -0.3, 6.5], 'rot0_deg': 25,
                   'axis': [0.3, 1, 0.1], 'albedo': [0.3, 0.6, 0.95], 'tex': 'checker', 'tex_freq': 3}],
    'translate': [{'shape': 'sphere', 'size': [0.7], 'pos': [-2.4, 0.0, 6], 'vel': [2.0, 0.0, 0.0],
                   'albedo': [0.9, 0.35, 0.3]}],
    'occlude':   [{'shape': 'sphere', 'size': [0.7], 'pos': [-2.4, 0.1, 6.5], 'vel': [2.0, 0.0, 0.0],
                   'albedo': [0.9, 0.35, 0.3]},
                  {'shape': 'quad', 'size': [0.35, 1.6], 'pos': [0.2, 0.0, 4.5], 'albedo': [0.25, 0.8, 0.4],
                   'tex': 'noise', 'tex_freq': 6}],
    'spin':      [{'shape': 'box', 'size': [0.8, 0.8, 0.8], 'pos': [0.0, 0.0, 6], 'axis': [0.35, 1, 0.15],
                   'omega_deg': 90.0, 'albedo': [0.3, 0.6, 0.95], 'tex': 'checker', 'tex_freq': 3}],
    # two thin occluders passing each other: a backdrop pixel covered by one at A and by the other at B
    # is visible in between — the physical source of class-0 (in NEITHER endpoint). They cross at
    # t = 2.2/4.0 = 0.55 s; each takes 0.1 s to sweep its own width, so a pair 0.35 s wide around the
    # crossing sees each quad travel 3.5 widths.
    'cross':     [{'shape': 'quad', 'size': [0.20, 1.7], 'pos': [-2.2, 0.0, 5.0], 'vel': [4.0, 0.0, 0.0],
                   'albedo': [0.25, 0.8, 0.4], 'tex': 'noise', 'tex_freq': 6},
                  {'shape': 'quad', 'size': [0.20, 1.7], 'pos': [2.2, 0.0, 5.4], 'vel': [-4.0, 0.0, 0.0],
                   'albedo': [0.95, 0.75, 0.2], 'tex': 'noise', 'tex_freq': 6}],
}
PRESETS['mixed'] = PRESETS['translate'] + [PRESETS['occlude'][1]] + [dict(PRESETS['spin'][0], pos=[1.5, -0.6, 7])]
BACKDROP = {'shape': 'quad', 'size': [30, 30], 'pos': [0, 0, 12], 'albedo': [0.75, 0.72, 0.68],
            'tex': 'noise', 'tex_freq': 0.9}
LIGHT = np.array([0.35, -0.5, -0.8]); LIGHT /= np.linalg.norm(LIGHT)
AMBIENT = 0.3


class Scene:
    def __init__(self, objects, W, H, fov_deg=60.0, seed=7):
        self.objs = [Obj(BACKDROP, 0, seed)] + [Obj(o, i + 1, seed) for i, o in enumerate(objects)]
        self.W, self.H, self.fov, self.seed = W, H, float(fov_deg), seed
        self.f = (W * 0.5) / np.tan(np.deg2rad(self.fov) * 0.5)
        self.cx, self.cy = W * 0.5, H * 0.5

    # rays for every subsample of every pixel, shape (ss*ss, H*W, 3), unit length
    def rays(self, ss):
        js, is_ = np.mgrid[0:self.H, 0:self.W]
        out = []
        for b in range(ss):
            for a in range(ss):
                u = is_ + (a + 0.5) / ss; v = js + (b + 0.5) / ss
                d = np.stack([(u - self.cx) / self.f, (v - self.cy) / self.f, np.ones_like(u)], axis=-1)
                d = d.reshape(-1, 3); d /= np.linalg.norm(d, axis=1)[:, None]
                out.append(d)
        return np.stack(out)

    def project(self, P):
        z = P[:, 2]
        return np.stack([self.f * P[:, 0] / z + self.cx, self.f * P[:, 1] / z + self.cy], axis=1), z

    def hit(self, t, d):
        """Nearest hit per ray: object index, world point, world normal, LOCAL point, depth (cam z)."""
        N = d.shape[0]
        o = np.zeros_like(d)
        best_t = np.full(N, np.inf); best_k = np.full(N, 255, np.uint8)
        P = np.zeros_like(d); Nw = np.zeros_like(d); L = np.zeros_like(d)
        for k, ob in enumerate(self.objs):
            R, tv = ob.pose(t)
            ol, dl = unapply(R, tv, o), d @ R
            th, nl = ob.intersect(ol, dl)
            win = th < best_t
            if not win.any():
                continue
            best_t[win] = th[win]; best_k[win] = k
            Pl = ol[win] + th[win, None] * dl[win]
            L[win] = Pl
            P[win] = apply(R, tv, Pl)
            Nw[win] = nl[win] @ R.T
        return best_k, P, Nw, L, P[:, 2]

    def shade(self, k, Nw, L):
        col = np.zeros_like(L)
        lam = AMBIENT + (1 - AMBIENT) * np.clip(np.einsum('ij,j->i', Nw, LIGHT), 0, 1)
        for i, ob in enumerate(self.objs):
            m = k == i
            if m.any():
                col[m] = ob.colour(L[m]) * lam[m, None]
        return col

    def render(self, t, ss=3):
        """RGB (H,W,3) float in [0,1] box-filtered over ss×ss, plus CENTRE-sample labels."""
        if ss % 2 == 0:
            raise ValueError('ss must be ODD: at an even factor the pixel centre falls between subsamples')
        d = self.rays(ss)
        acc = np.zeros((self.H * self.W, 3))
        centre = (ss // 2) * ss + ss // 2
        labels = None
        for s in range(ss * ss):
            k, P, Nw, L, z = self.hit(t, d[s])
            acc += self.shade(k, Nw, L)
            if s == centre:
                labels = (k.reshape(self.H, self.W), z.reshape(self.H, self.W), L.reshape(self.H, self.W, 3))
        rgb = np.clip(acc / (ss * ss), 0, 1).reshape(self.H, self.W, 3)
        return rgb, labels

    def labels(self, t):
        d = self.rays(1)[0]
        k, P, Nw, L, z = self.hit(t, d)
        return k.reshape(self.H, self.W), z.reshape(self.H, self.W), L.reshape(self.H, self.W, 3)

    def reproject(self, k, L, t_to):
        """Where does each material point (k, L) sit in the image at time t_to? -> (uv, z)."""
        uv = np.zeros((k.size, 2)); z = np.zeros(k.size)
        kf, Lf = k.ravel(), L.reshape(-1, 3)
        for i, ob in enumerate(self.objs):
            m = kf == i
            if m.any():
                R, tv = ob.pose(t_to)
                uvm, zm = self.project(apply(R, tv, Lf[m]))
                uv[m] = uvm; z[m] = zm
        return uv.reshape(k.shape + (2,)), z.reshape(k.shape)

    def flow(self, t, dt, k, L):
        """Exact pixel motion of the material point seen at t, toward t + dt."""
        js, is_ = np.mgrid[0:self.H, 0:self.W]
        here = np.stack([is_ + 0.5, js + 0.5], axis=-1)
        uv, _ = self.reproject(k, L, t + dt)
        return (uv - here).astype(np.float32)

    def visibility(self, t, tA, tB, eps_rel=2e-3, details=False):
        """Per-pixel class for the material point seen at t: 3 both · 2 only B · 1 only A · 0 neither.

        Two mechanisms hide a point, and the gate exercises BOTH by name: another object in front of
        it (the ID test — `cross`) and its own body in front of it, same ID, nearer depth (the depth
        test — `spin`, a face rotated to the back). `details=True` also returns the mask of points
        that landed on their own object and were rejected on depth alone, which is the quantity the
        second mechanism's gate counts.
        """
        k, _, L = self.labels(t)
        out = np.zeros(k.shape, np.uint8)
        self_hidden = np.zeros(k.shape, bool)
        for bit, tt in ((1, tA), (2, tB)):
            kk, zz, _ = self.labels(tt)
            uv, z = self.reproject(k, L, tt)
            ui = np.floor(uv[..., 0]).astype(np.int64); vi = np.floor(uv[..., 1]).astype(np.int64)
            inb = (ui >= 0) & (ui < self.W) & (vi >= 0) & (vi < self.H) & (z > 0)
            uic, vic = np.clip(ui, 0, self.W - 1), np.clip(vi, 0, self.H - 1)
            same_id = kk[vic, uic] == k
            same_z = np.abs(zz[vic, uic] - z) <= eps_rel * z
            out |= np.where(inb & same_id & same_z, bit, 0).astype(np.uint8)
            self_hidden |= inb & same_id & ~same_z
        return (out, self_hidden) if details else out


def make_scene(name, W, H, fov, seed):
    if name not in PRESETS:
        raise SystemExit('unknown scene %r; have %s' % (name, ', '.join(PRESETS)))
    return Scene(PRESETS[name], W, H, fov, seed)


def to_rgba(rgb):
    u = np.rint(np.clip(rgb, 0, 1) * 255.0).astype(np.uint8)
    out = np.empty(u.shape[:2] + (4,), np.uint8)
    out[..., :3] = u; out[..., 3] = 255
    return out


# ── the gate ─────────────────────────────────────────────────────────────────────────────────────
def verify(W=320, H=180, fps=240.0, seed=7):
    print('VERIFY — every gate below was seen RED under a deliberate perturbation before it was trusted.')
    fail = []
    dt = 1.0 / fps

    # G1 — determinism
    sc = make_scene('translate', W, H, 60.0, seed)
    a, _ = sc.render(0.3, 3); b, _ = sc.render(0.3, 3)
    same = np.array_equal(to_rgba(a), to_rgba(b))
    print('  G1 determinism  : two renders of t=0.3 byte-identical -> %s' % same)
    if not same: fail.append('G1')

    # G4 — coverage (checked before flow so a hole cannot masquerade as a flow error)
    k0, _, _ = sc.labels(0.3)
    holes = int((k0 == 255).sum())
    print('  G4 coverage     : pixels seeing nothing = %d -> %s' % (holes, holes == 0))
    if holes: fail.append('G4')

    # G2 — flow, checked independently through frame t+dt's OWN hits
    t = 0.3
    k, z, L = sc.labels(t)
    fl = sc.flow(t, dt, k, L)
    kn, zn, Ln = sc.labels(t + dt)
    js, is_ = np.mgrid[0:H, 0:W]
    q = np.stack([is_ + 0.5, js + 0.5], axis=-1) + fl
    qi = np.floor(q[..., 0]).astype(np.int64); qj = np.floor(q[..., 1]).astype(np.int64)
    inb = (qi >= 0) & (qi < W) & (qj >= 0) & (qj < H)
    qi, qj = np.clip(qi, 0, W - 1), np.clip(qj, 0, H - 1)
    samek = inb & (kn[qj, qi] == k) & (k > 0)              # moving object pixels that land on themselves
    back, _ = sc.reproject(kn[qj, qi], Ln[qj, qi], t)        # frame t+dt's hit, projected back to t
    here = np.stack([is_ + 0.5, js + 0.5], axis=-1)
    res = np.linalg.norm(back - here, axis=-1)[samek]
    mv = np.linalg.norm(fl, axis=-1)
    print('  G2 flow         : object flow mean %.3f px/frame, backdrop flow max %.2e px; round-trip '
          'residual on %d object pixels p99 %.3f px, max %.3f px'
          % (mv[k > 0].mean(), mv[k == 0].max(), samek.sum(), np.percentile(res, 99), res.max()))
    ok2 = (mv[k > 0].mean() > 0.2) and (mv[k == 0].max() < 1e-9) and (np.percentile(res, 99) < 1.0) and (res.max() < 2.5)
    print('                    -> %s' % ok2)
    if not ok2: fail.append('G2')

    # G3 — visibility: static must have NO class-0; cross must have SOME
    K = 8
    st = make_scene('static', W, H, 60.0, seed)
    v = st.visibility(4 * dt, 0.0, K * dt)
    n0s = int((v == 0).sum()); n3s = int((v == 3).sum())
    cr = make_scene('cross', W, H, 60.0, seed)
    tA, tB = 0.55 - 0.175, 0.55 + 0.175                    # a pair centred on the crossing
    tm = 0.5 * (tA + tB)
    v2 = cr.visibility(tm, tA, tB)
    km, _, _ = cr.labels(tm)
    n0c = int((v2 == 0).sum()); n0c_bg = int(((v2 == 0) & (km == 0)).sum())
    print('  G3 visibility   : static -> neither=%d both=%d of %d (need 0 / all); cross -> neither=%d, '
          'of which backdrop=%d (need >0, and the class must be the physical one)'
          % (n0s, n3s, W * H, n0c, n0c_bg))
    # the depth mechanism, by name: a box face rotated 126 degrees between A and B is hidden at B by
    # the box's OWN nearer faces -- same id, different depth. That count is exactly zero if the depth
    # test is disabled, which is how this half of G3 was seen red.
    sp = make_scene('spin', W, H, 60.0, seed)
    tA2, tB2 = 0.2, 0.2 + 1.4
    _, selfh = sp.visibility(0.5 * (tA2 + tB2), tA2, tB2, details=True)
    nself = int(selfh.sum())
    print('                    spin -> box points hidden by the box itself (id match, depth reject) = %d (need >0)'
          % nself)
    ok3 = (n0s == 0) and (n3s == W * H) and (n0c > 0) and (n0c_bg > 0) and (nself > 0)
    print('                    -> %s' % ok3)
    if not ok3: fail.append('G3')

    # information, not a gate: a translating sphere reveals a sliver of surface under perspective
    tr = make_scene('translate', W, H, 60.0, seed)
    vt = tr.visibility(0.5 * (0.4 + 0.4 + K * dt), 0.4, 0.4 + K * dt)
    print('  (info) translate at k=%d: neither=%d px — the limb sliver perspective reveals; real, not a bug'
          % (K, int((vt == 0).sum())))

    print()
    if fail:
        sys.exit('VERIFY FAILED: %s' % ', '.join(fail))
    print('VERIFY PASSED (G1 G2 G3 G4).')


# ── emit a corpus ────────────────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description='scene_truth — an exactly-renderable 3-D scene for FG truth')
    ap.add_argument('--out')
    ap.add_argument('--scene', default='mixed', choices=sorted(PRESETS))
    ap.add_argument('--width', type=int, default=640)
    ap.add_argument('--height', type=int, default=360)
    ap.add_argument('--fov', type=float, default=60.0)
    ap.add_argument('--fps', type=float, default=240.0, help='BASE rate; the FG is fed every k-th frame')
    ap.add_argument('--seconds', type=float, default=1.0)
    ap.add_argument('--ss', type=int, default=3, help='supersampling factor, ODD')
    ap.add_argument('--seed', type=int, default=7)
    ap.add_argument('--speed', type=float, default=1.0,
                    help='multiply every object\'s velocity and spin. THE SPEED TEST (operator, 2026-09-08): '
                         'the same displacement per source pair reached two ways -- speed x2 at fixed k, or k x2 '
                         'at fixed speed. If the FG\'s error curves coincide, it sees only pixels-per-pair; if '
                         'they diverge, absolute speed matters on its own.')
    ap.add_argument('--bmp', type=int, default=0, help='also write the first N frames as BMP, for a glance')
    ap.add_argument('--manifest-k', type=int, action='append', default=[],
                    help='write held-out triples (prev/mid/next) for multiplier K, gt-emit format')
    ap.add_argument('--barcode', action='store_true',
                    help='bake the 16-bit frame index into every frame (marker_zoo\'s strip) and write the '
                         '`sequence`/`fps` manifest lines, so play_frames.ps1 can present the corpus to the '
                         'LIVE FG and marker_extract can align what came back (TB-C7). The scorer masks the strip.')
    ap.add_argument('--labels', action='store_true',
                    help='also write the depth / flow / flowb planes (1.1 GB of a 1.3 GB corpus at 640x360). '
                         'OFF by default: every consumer re-derives them from the geometry, bit-identically '
                         '(G1), so nothing is lost. frames/ and id/ are always written.')
    ap.add_argument('--verify', action='store_true')
    a = ap.parse_args()

    if a.verify:
        verify(seed=a.seed)
        if not a.out:
            return
    if not a.out:
        sys.exit('--out DIR is required (or --verify)')

    W, H = a.width, a.height
    objects = [dict(o, vel=[v * a.speed for v in o.get('vel', [0, 0, 0])],
                    omega_deg=o.get('omega_deg', 0.0) * a.speed) for o in PRESETS[a.scene]]
    sc = Scene(objects, W, H, a.fov, a.seed)
    n = int(round(a.fps * a.seconds))
    dt = 1.0 / a.fps
    for sub in (('frames', 'id', 'depth', 'flow', 'flowb') if a.labels else ('frames', 'id')):
        os.makedirs(os.path.join(a.out, sub), exist_ok=True)

    truth = {'renderer': 'scene_zoo v1', 'scene': a.scene, 'width': W, 'height': H, 'fov_deg': a.fov,
             'base_fps': a.fps, 'frames': n, 'seed': a.seed, 'ss': a.ss, 'speed': a.speed,
             'objects': [BACKDROP] + objects, 'light': LIGHT.tolist(), 'ambient': AMBIENT,
             'conventions': {'camera': 'origin, +z forward, x right, y down, fixed',
                             'pixel_centre': '(i+0.5, j+0.5)', 'depth': 'camera z',
                             'flow': 'pixels toward t±1/base_fps, +x right +y down',
                             'files': {'frames': 'RGBA8 H*W*4', 'id': 'u8 H*W (0=backdrop, 255=none)',
                                       'depth': 'f32 H*W', 'flow': 'f32 H*W*2 forward',
                                       'flowb': 'f32 H*W*2 backward'}},
             't': [i * dt for i in range(n)]}

    truth['barcode'] = bool(a.barcode)
    if a.barcode:
        truth['barcode_strip'] = {'x0': BC_X0 - 1, 'x1': BC_X0 + BC_W + 1, 'y0': BC_Y0 - 1, 'y1': BC_Y1 + 1}
    for i in range(n):
        t = i * dt
        rgb, (k, z, L) = sc.render(t, a.ss)
        if a.barcode:
            draw_barcode(rgb, i)                      # broadcasts over the 3 channels: black/white blocks
        rgba = to_rgba(rgb)
        if a.barcode and i == 0 and decode_barcode(rgba) != 0:
            sys.exit('barcode round-trip failed on frame 0')
        rgba.tofile(os.path.join(a.out, 'frames', 'f_%06d.rgba' % i))
        k.astype(np.uint8).tofile(os.path.join(a.out, 'id', 'f_%06d.u8' % i))
        if a.labels:
            z.astype(np.float32).tofile(os.path.join(a.out, 'depth', 'f_%06d.f32' % i))
            sc.flow(t, dt, k, L).tofile(os.path.join(a.out, 'flow', 'f_%06d.f32' % i))
            sc.flow(t, -dt, k, L).tofile(os.path.join(a.out, 'flowb', 'f_%06d.f32' % i))
        if i < a.bmp:
            write_bmp(os.path.join(a.out, 'frames', 'f_%06d.bmp' % i), rgba)
        if (i + 1) % 24 == 0 or i + 1 == n:
            print('  %d/%d frames' % (i + 1, n))

    json.dump(truth, open(os.path.join(a.out, 'truth.json'), 'w'), indent=1)
    with open(os.path.join(a.out, 'manifest.txt'), 'w', encoding='utf-8') as f:
        f.write('# scene_truth corpus — scene=%s seed=%d ss=%d\nsize %d %d\nbase_fps %g\nframes %d\n'
                % (a.scene, a.seed, a.ss, W, H, a.fps, n))
        if a.barcode:                                 # the two lines play_frames.ps1 parses
            f.write('sequence frames/f_ %d\nfps %g\n' % (n, a.fps))
    # B1: what the LIVE FG is shown. play_frames.ps1 plays CONTIGUOUS indices, so the k-th subsequence
    # is written out as its own contiguous directory; the barcode inside each frame still carries the
    # BASE index, which is how a captured frame maps back to its truth and its pair.
    for K in a.manifest_k:
        if a.barcode:
            sd = os.path.join(a.out, 'source_k%d' % K)
            os.makedirs(sd, exist_ok=True)
            js = list(range(0, n, K))
            for j, i in enumerate(js):
                src = os.path.join(a.out, 'frames', 'f_%06d.rgba' % i)
                dst = os.path.join(sd, 'f_%06d.rgba' % j)
                if not os.path.exists(dst):
                    os.link(src, dst) if hasattr(os, 'link') else open(dst, 'wb').write(open(src, 'rb').read())
            with open(os.path.join(sd, 'manifest.txt'), 'w', encoding='utf-8') as f:
                f.write('# source for the live FG at multiplier %d: base frame j*%d, barcode = BASE index\n'
                        'size %d %d\nsequence f_ %d\nfps %g\n' % (K, K, W, H, len(js), a.fps / K))
    for K in a.manifest_k:
        with open(os.path.join(a.out, 'manifest_k%d.txt' % K), 'w', encoding='utf-8') as f:
            f.write('# held-out triples for multiplier %d: source = every %d-th base frame\nsize %d %d\n'
                    % (K, K, W, H))
            for N in range(0, n - K, K):
                for j in range(1, K):
                    f.write('triple f_%06d prev=frames/f_%06d.rgba mid=frames/f_%06d.rgba next=frames/f_%06d.rgba phase=%.6f\n'
                            % (N + j, N, N + j, N + K, j / K))
    print('wrote %d frames + labels + truth.json to %s' % (n, a.out))


if __name__ == '__main__':
    main()
