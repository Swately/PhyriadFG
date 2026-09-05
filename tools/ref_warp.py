#!/usr/bin/env python3
"""ref_warp.py — the CPU REFERENCE WARP (S2.T6): replay a `--qdump+` triple and rebuild `live`.

This is the independent oracle R3's M4 gate is built on. `fg_core.comp` will be judged by whether it
reproduces `wap_warp.comp`; two shaders agreeing only proves they agree. This says what the output
SHOULD be, from the shader's text, in a language that shares nothing with it.

WHAT IT IMPLEMENTS — the shipping default's pre-store path, which is far smaller than the shader's
1,336 lines suggest. With `single_track = 1.0` the final store is

    result = mix(B_samp, cur[uv], w_s)

and the whole commit / matte / onepos / blend / mc / ts-smooth / extrap cascade is written and then
overwritten. A_samp and B_samp are `const`, born at shader line ~520 and never mutated. So everything
that decides the output happens in lines 279–522 plus the stasis bool, and it is exactly this:

    uv        = (coord + 0.5) / out_size                     (cam_lead = 0 -> no lead)
    mv        = guided_mv(uv, mv_guided - 1)  or  bilinear MV
    if inertia_static and |mv| > 2:  mv = bilinear MV        (gate (b): no fast-corner lending)
    mv_fwd    = mv                                           (captured BEFORE the reclaim)
    mv        = bg_reclaim(mv)                               (three-hypothesis damp)
    mv        = mix(mv_fwd, -mv_bwd, w_b)                    (phase anchor — see the note below)
    mv        = ambiguity(mv)                                (runner-up arbitration)
    mv_eff    = mix(mv, 2*mv - mv_target, vb_w)              (vblend tilt)
    A_samp    = prev[uv - mv_eff*t],  B_samp = cur[uv + mv_eff*(1-t)]
    w_s       = 1 if stasis else smoothstep(1.2, 3, (d_pixel + .02)/(d_zero + .02))
    result    = mix(B_samp, cur[uv], w_s)

BUG-FOR-BUG, deliberately (the convergence plan's XR7): the phase anchor mixes from `mv_fwd`, the
value captured BEFORE `bg_reclaim` ran — so whenever the anchor is active the reclaim's damping is
DISCARDED. That is what the shader does. The reference reproduces it and reports it; it does not
quietly "fix" it, because an oracle that improves on its subject cannot judge it.

SCOPE IS ENFORCED, NOT ASSUMED. The push block says what was armed. Anything armed that this
reference does not implement makes it REFUSE the triple by name, rather than return a number that
looks like a verdict. See `unsupported()`.

    python tools/ref_warp.py <dump_dir> [--triples N] [--save-worst DIR] [--json OUT]

numpy + stdlib only. Made with my soul - Swately <3
"""
import argparse, json, os, struct, sys
import numpy as np

PUSH_NAMES = """residual_ceil improvement_frac agreement_threshold t soft_gate commit_thresh commit_real
occl_thresh div_eps rescue_on mv_guided gme_on gme_a gme_b gme_c gme_d gme_e gme_f matte_on matte_thresh
stasis_thresh inertia_thresh crescent_on appear_on appear_band travel_on contour_on obj_crescent_on
phase_anchor_on ambig_on member_commit_on commit_default_on onepos_on onepos_band disoccl_commit_on
bg_snap_on bg_snap_strength bg_snap_norm extrap vblend_on vblend_t0 vblend_strength band_xfade
vblend_exact ts_smooth mc_on mc_nperturb mc_perturb mc_disp mc_edge cam_lead_x cam_lead_y
disoccl_hardpick predict_p2 blend_solo mv_edge_snap single_track bg_reclaim""".split()


# ── GLSL primitives ────────────────────────────────────────────────────────────────────────────
def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)

def mix(a, b, w):
    return a + (b - a) * w

# GPU texture units do not filter with full float weights. Vulkan only guarantees
# `subTexelPrecisionBits` (8 on this class of hardware), so the fractional coordinate is quantized to
# 1/256 of a texel before the four taps are weighted. FILTER_BITS switches the reference between exact
# float filtering (None) and that quantization, which is how the hypothesis "the residual disagreement
# is filter precision, not a semantic error" gets TESTED instead of asserted. See --filter-bits.
FILTER_BITS = None

def _quantize_frac(f):
    if FILTER_BITS is None:
        return f
    n = np.float32(1 << FILTER_BITS)
    return np.floor(f * n) / n

def sample(img, u, v):
    """`texture(img, vec2(u,v))` with VK_FILTER_LINEAR + CLAMP_TO_EDGE — the one sampler every warp
    binding uses (warp_blend.cpp:100–115). u,v are normalized; the texel frame is uv*size - 0.5."""
    h, w = img.shape[0], img.shape[1]
    x = u * w - 0.5
    y = v * h - 0.5
    x0 = np.floor(x); y0 = np.floor(y)
    fx = _quantize_frac(x - x0)[..., None]; fy = _quantize_frac(y - y0)[..., None]
    xi0 = np.clip(x0.astype(np.int64), 0, w - 1); xi1 = np.clip(xi0 + 1, 0, w - 1)
    yi0 = np.clip(y0.astype(np.int64), 0, h - 1); yi1 = np.clip(yi0 + 1, 0, h - 1)
    p00 = img[yi0, xi0]; p10 = img[yi0, xi1]; p01 = img[yi1, xi0]; p11 = img[yi1, xi1]
    return ((p00 * (1 - fx) + p10 * fx) * (1 - fy) + (p01 * (1 - fx) + p11 * fx) * fy)

def length3(v):
    return np.sqrt((v * v).sum(axis=-1))

def length2(v):
    return np.sqrt((v * v).sum(axis=-1))


# ── the record ─────────────────────────────────────────────────────────────────────────────────
def load_manifest(path):
    size, live_div, rows = None, 1, []
    for ln in open(path, encoding='utf-8', errors='replace'):
        ln = ln.strip()
        if ln.startswith('size '):
            _, w, h = ln.split()[:3]; size = (int(w), int(h)); continue
        if ln.startswith('live_div '):
            live_div = int(ln.split()[1]); continue
        if not ln.startswith('triple '):
            continue
        toks = ln.split(); rec = {'id': toks[1]}
        for t in toks[2:]:
            if '=' in t:
                k, v = t.split('=', 1); rec[k] = v
        rows.append(rec)
    return size, live_div, rows

def rgba(path, w, h):
    a = np.fromfile(path, dtype=np.uint8)
    if a.size != w * h * 4:
        raise ValueError(f'{os.path.basename(path)}: {a.size} B, expected {w*h*4}')
    return a.reshape(h, w, 4)

def planef16(path, w, h, c):
    a = np.fromfile(path, dtype=np.float16).astype(np.float32)
    if a.size != w * h * c:
        raise ValueError(f'{os.path.basename(path)}: {a.size} halves, expected {w*h*c}')
    return a.reshape(h, w, c)

def planer8(path, w, h):
    a = np.fromfile(path, dtype=np.uint8)
    if a.size != w * h:
        raise ValueError(f'{os.path.basename(path)}: {a.size} B, expected {w*h}')
    return (a.reshape(h, w, 1).astype(np.float32)) / 255.0


def unsupported(p):
    """What this reference does NOT implement. Each entry is a shader path that would change the
    value stored, so proceeding with it armed would produce a confident wrong answer."""
    bad = []
    if p['single_track'] <= 0.5:
        bad.append('single_track <= 0.5: the full commit/matte/onepos/blend cascade decides the store')
    if p['blend_solo'] > 0.5:
        bad.append('blend_solo: the diagnostic override wins over the single-track store')
    if p['mv_edge_snap'] > 0.5:
        bad.append('mv_edge_snap: the cross-bilateral MV fetch replaces guided/linear')
    if p['cam_lead_x'] != 0.0 or p['cam_lead_y'] != 0.0:
        bad.append('cam_lead: the whole sampling base uv shifts')
    if p['matte_on'] > 0.5:
        bad.append('matte_on: the phase-anchor claim and the ambiguity object proxy read the dissidence masks')
    if p['bg_snap_on'] > 0.5 or p['band_xfade'] > 0.0:
        bad.append('bg_snap / band_xfade: they mutate mv from the iGPU contour field, which is not dumped')
    return bad


def reference(triple, push, W, H, gw, gh):
    """Rebuild the stored value for one tick. Returns (result_float, diagnostics)."""
    p = push
    t = np.float32(p['t'])
    prev = triple['prev'][..., :3].astype(np.float32) / 255.0
    cur  = triple['next'][..., :3].astype(np.float32) / 255.0
    mvg  = triple['mv']                       # (gh, gw, 2) pixel units
    sadg = triple['sad']                      # (gh, gw, 2) R=sad_best G=sad_zero

    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
    u = (xs + 0.5) / np.float32(W)
    v = (ys + 0.5) / np.float32(H)

    def s_cur(uu, vv):  return sample(cur, uu, vv)
    def s_prev(uu, vv): return sample(prev, uu, vv)

    mv_lin = sample(mvg, u, v)                                    # the LINEAR (bilinear) MV

    # ── inertia prior ──────────────────────────────────────────────────────────────────────────
    inertia_on = p['inertia_thresh'] > 0.0
    if inertia_on:
        inertia_p = sample(triple['per'], u, v)[..., 0]
        inertia_static = inertia_p >= p['inertia_thresh']
    else:
        inertia_static = np.zeros((H, W), dtype=bool)

    # ── primary MV: guided (hard single-corner pick) or linear ─────────────────────────────────
    if p['mv_guided'] > 0.5:
        sim = np.float32(p['mv_guided'] - 1.0)
        sx = u * np.float32(gw) - np.float32(0.5)
        sy = v * np.float32(gh) - np.float32(0.5)
        i00x = np.floor(sx).astype(np.int64); i00y = np.floor(sy).astype(np.int64)
        # the block-centre colour table: cur sampled at ((ci + 0.5) / grid), one value per MV texel.
        cix, ciy = np.meshgrid(np.arange(gw), np.arange(gh))
        cc_tab = s_cur(((cix + 0.5) / gw).astype(np.float32),
                       ((ciy + 0.5) / gh).astype(np.float32))       # (gh, gw, 3)
        c_pix = s_cur(u, v)                                          # this pixel's membership colour
        best_d = np.full((H, W), 1e9, np.float32)
        second_d = np.full((H, W), 1e9, np.float32)
        best_mv = np.zeros((H, W, 2), np.float32)
        for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
            cx = np.clip(i00x + dx, 0, gw - 1); cy = np.clip(i00y + dy, 0, gh - 1)
            c_c = cc_tab[cy, cx]
            dmx = np.max(np.abs(c_pix - c_c), axis=-1)
            win = dmx < best_d
            second_d = np.where(win, best_d, np.minimum(second_d, dmx))
            best_mv = np.where(win[..., None], mvg[cy, cx], best_mv)
            best_d = np.where(win, dmx, best_d)
        fallback = (best_d > sim) | ((second_d - best_d) <= sim)
        mv = np.where(fallback[..., None], mv_lin, best_mv)
    else:
        mv = mv_lin.copy()

    # gate (b): a static-history block never adopts a fast corner MV
    if p['mv_guided'] > 0.5 or p['mv_edge_snap'] > 0.5:
        refuse = inertia_static & (length2(mv) > 2.0)
        mv = np.where(refuse[..., None], mv_lin, mv)

    mv_fwd = mv.copy()          # captured BEFORE the reclaim — the phase anchor mixes from THIS

    # ── the gme affine model, evaluated on the MV lattice ──────────────────────────────────────
    gx = u * np.float32(gw) - np.float32(0.5)
    gy = v * np.float32(gh) - np.float32(0.5)
    model_mv = np.stack([p['gme_a'] + p['gme_b'] * gx + p['gme_c'] * gy,
                         p['gme_d'] + p['gme_e'] * gx + p['gme_f'] * gy], axis=-1).astype(np.float32)

    szf = np.float32([W, H])
    def warp_pair(m):
        a = s_prev(u - (m[..., 0] * t) / W, v - (m[..., 1] * t) / H)
        b = s_cur (u + (m[..., 0] * (1 - t)) / W, v + (m[..., 1] * (1 - t)) / H)
        return a, b

    # ── bg_reclaim: the three-hypothesis damp ──────────────────────────────────────────────────
    reclaim_w = np.zeros((H, W), np.float32)
    if p['bg_reclaim'] > 0.001 and p['gme_on'] > 0.5:
        nonconf = smoothstep(2.0, 6.0, length2(mv - model_mv))
        A_l, B_l = warp_pair(mv)
        A_m, B_m = warp_pair(model_mv)
        d_loc = length3(A_l - B_l)
        d_mod = length3(A_m - B_m)
        bg_like = smoothstep(1.2, 3.0, (d_loc + 0.02) / (d_mod + 0.02))
        d_zero0 = length3(s_cur(u, v) - s_prev(u, v))
        st_over_model = smoothstep(1.2, 3.0, (d_mod + 0.02) / (d_zero0 + 0.02))
        zero_like = smoothstep(1.2, 3.0, (d_loc + 0.02) / (d_zero0 + 0.02))
        target = mix(model_mv, np.zeros_like(model_mv), st_over_model[..., None])
        evid = mix(bg_like, zero_like, st_over_model)
        w = np.clip(p['bg_reclaim'] * nonconf * evid, 0.0, 1.0)
        # the shader only enters this branch where nonconf > 0; outside, mv is untouched
        w = np.where(nonconf > 0.0, w, 0.0).astype(np.float32)
        mv = mix(mv, target, w[..., None])
        reclaim_w = w

    # ── phase anchor. NOTE: mixes from mv_fwd, so it DISCARDS the reclaim. Bug-for-bug. ────────
    anchor_active = False
    if p['phase_anchor_on'] > 0.5 and p['occl_thresh'] > 0.0:
        mv_bwd = sample(triple['mvb'], u, v)
        claim = 0.0                                   # matte_on is refused above, so claim is 0
        w_b = np.clip(smoothstep(0.35, 0.65, t) + claim, 0.0, 1.0)
        mv = mix(mv_fwd, -mv_bwd, np.float32(w_b))
        anchor_active = True

    sad = sample(sadg, u, v)
    sad_best = sad[..., 0]
    sad_zero = sad[..., 1]

    # ── ambiguity: runner-up arbitration ───────────────────────────────────────────────────────
    ambig_hits = 0
    if p['ambig_on'] > 0.5 and p['gme_on'] > 0.5:
        cand = sample(triple['c2'], u, v)
        aliased = (cand[..., 2] <= 1.15 * sad_best) & (length2(cand[..., :2] - mv) > 2.0)
        referee = model_mv                            # gme_model_mv(uv)
        nearer = length2(cand[..., :2] - referee) < length2(mv - referee)
        take = aliased & nearer                       # object_proxy is False (matte_on refused)
        mv = np.where(take[..., None], cand[..., :2], mv)
        ambig_hits = int(take.sum())

    # ── vblend tilt of the SAMPLE OFFSETS only ─────────────────────────────────────────────────
    if p['vblend_on'] > 0.5:
        vb_w = np.float32(smoothstep(p['vblend_t0'], 1.0, float(t)) * p['vblend_strength'])
        mv_ut = sample(triple['mvt'], u, v)
        mv_target_s = mv_ut if p['vblend_exact'] > 0.5 else (2.0 * mv - mv_ut)
        mv_eff = mix(mv, mv_target_s, vb_w)
    else:
        vb_w = np.float32(0.0)
        mv_eff = mv

    A_samp = s_prev(u - (mv_eff[..., 0] * t) / W, v - (mv_eff[..., 1] * t) / H)
    B_samp = s_cur (u + (mv_eff[..., 0] * (1 - t)) / W, v + (mv_eff[..., 1] * (1 - t)) / H)
    d_pixel = length3(A_samp - B_samp)

    stasis = (p['stasis_thresh'] > 0.0) & (sad_zero <= p['stasis_thresh'])

    # ── the single-track store ─────────────────────────────────────────────────────────────────
    cur0 = s_cur(u, v)
    if p['single_track'] < 1.5:                       # v3.2 — the shipping default
        d_zero = length3(cur0 - s_prev(u, v))
        w_s = smoothstep(1.2, 3.0, (d_pixel + 0.02) / (d_zero + 0.02))
        w_s = np.where(stasis, np.float32(1.0), w_s)
        result = mix(B_samp, cur0, w_s[..., None])
    else:                                             # v3.0 — pure B_samp
        w_s = np.zeros((H, W), np.float32)
        result = B_samp

    diag = dict(vb_w=float(vb_w), anchor_active=anchor_active, ambig_hits=ambig_hits,
                reclaim_frac=float((reclaim_w > 0.0).mean()),
                stasis_frac=float(stasis.mean()), ws_mean=float(w_s.mean()))
    return result, diag


def compare(ref_rgb, live, worst_path=None):
    """ref_rgb float [0,1] -> RGBA8 the way imageStore(rgba8) does: clamp, then round-to-nearest."""
    q = np.rint(np.clip(ref_rgb, 0.0, 1.0) * 255.0).astype(np.int16)
    got = live[..., :3].astype(np.int16)
    d = np.abs(q - got)
    dmax = d.max(axis=-1)
    n = dmax.size
    out = dict(
        exact=float((dmax == 0).mean() * 100.0),
        le1=float((dmax <= 1).mean() * 100.0),
        le2=float((dmax <= 2).mean() * 100.0),
        le4=float((dmax <= 4).mean() * 100.0),
        maxd=int(dmax.max()),
        mean=float(dmax.mean()),
        over8=int((dmax > 8).sum()),
        n=int(n),
    )
    if worst_path is not None and out['maxd'] > 0:
        yx = np.unravel_index(int(np.argmax(dmax)), dmax.shape)
        out['worst_xy'] = [int(yx[1]), int(yx[0])]
        out['worst_ref'] = [int(c) for c in q[yx]]
        out['worst_got'] = [int(c) for c in got[yx]]
    return out, dmax


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('--triples', type=int, default=0, help='limit to the first N triples (0 = all)')
    ap.add_argument('--json', help='write the per-triple results to this JSON file')
    ap.add_argument('--save-worst', help='directory for the |diff| map of the worst triple (raw u8)')
    ap.add_argument('--mv-plane', default='mv',
                    help='manifest token of the MV plane to feed the reference: mv (the host field, BEFORE '
                         'the consensus pass -- what the matcher produced) or mv1 (read back from wapMVA '
                         'AFTER the pass -- what the shader actually sampled). The difference between the '
                         'two fits IS the consensus pass, measured through the oracle.')
    ap.add_argument('--mvb-plane', default='mvb',
                    help='manifest token of the BACKWARD MV plane to feed the reference: mvb (host field, '
                         'before the consensus pass) or mvb1 (read back from wapMVBA after it). The phase '
                         'anchor mixes in -mv_bwd above t~0.65, so at high phase this is the field that matters.')
    ap.add_argument('--fit', action='store_true',
                    help='also report the least-squares scale k and correlation between the change the '
                         'reference makes to cur[uv] and the change the GPU made. k=1 means the '
                         'displacement magnitude matches; corr says how much of it is explained at all.')
    ap.add_argument('--filter-bits', type=int, default=None,
                    help='quantize the bilinear fractional coordinate to this many bits, as a GPU '
                         'texture unit does (Vulkan subTexelPrecisionBits; 8 is typical). Omit for '
                         'exact float filtering.')
    a = ap.parse_args()
    global FILTER_BITS
    FILTER_BITS = a.filter_bits
    if FILTER_BITS is not None:
        print(f'filtering: fractional coordinate quantized to {FILTER_BITS} bits (1/{1<<FILTER_BITS} texel)')

    manp = os.path.join(a.dir, 'manifest.txt')
    if not os.path.exists(manp):
        print('FAIL: no manifest.txt in', a.dir); return 1
    size, live_div, rows = load_manifest(manp)
    if live_div != 1:
        print(f'FAIL: live_div={live_div} — the live plane is scaled; this reference assumes warp_div 1')
        return 1
    W, H = size
    if a.triples:
        rows = rows[:a.triples]
    print(f'record: {a.dir}\n  {W}x{H}, {len(rows)} triples')

    results = []
    fails = 0
    for r in rows:
        rid = r['id']
        raw = open(os.path.join(a.dir, r['push']), 'rb').read()
        if len(raw) < 4 * len(PUSH_NAMES):
            print(f'  SKIP {rid}: push is {len(raw)} B, shorter than the 58-float contract'); fails += 1; continue
        push = dict(zip(PUSH_NAMES, struct.unpack_from('<%df' % len(PUSH_NAMES), raw, 0)))
        bad = unsupported(push)
        if bad:
            print(f'  REFUSED {rid}: this reference does not implement —')
            for b in bad:
                print('      ' + b)
            fails += 1
            continue
        gw, gh = int(r['mvw']), int(r['mvh'])
        try:
            triple = dict(
                prev=rgba(os.path.join(a.dir, r['prev']), W, H),
                next=rgba(os.path.join(a.dir, r['next']), W, H),
                live=rgba(os.path.join(a.dir, r['live']), W, H),
                mv=planef16(os.path.join(a.dir, r[a.mv_plane]), gw, gh, 2),   # --mv-plane: which MV the oracle is fed
                sad=planef16(os.path.join(a.dir, r['sad']), gw, gh, 2),
            )
            for key, loader in (('mvb', lambda p: planef16(p, gw, gh, 2)),   # NOTE: r['mvb'] is remapped below when --mvb-plane is given
                                ('mvt', lambda p: planef16(p, gw, gh, 2)),
                                ('c2',  lambda p: planef16(p, gw, gh, 4)),
                                ('per', lambda p: planer8(p, gw, gh))):
                nm = r.get(a.mvb_plane if key == 'mvb' else key, '-')   # --mvb-plane: which BACKWARD field the oracle is fed
                triple[key] = loader(os.path.join(a.dir, nm)) if nm != '-' else None
        except (OSError, ValueError) as e:
            print(f'  FAIL {rid}: {e}'); fails += 1; continue
        need = [k for k, cond in (('mvb', push['phase_anchor_on'] > 0.5 and push['occl_thresh'] > 0),
                                  ('mvt', push['vblend_on'] > 0.5),
                                  ('c2',  push['ambig_on'] > 0.5 and push['gme_on'] > 0.5),
                                  ('per', push['inertia_thresh'] > 0)) if cond and triple.get(k) is None]
        if need:
            print(f'  FAIL {rid}: the push arms features whose planes are absent: {need}'); fails += 1; continue

        ref, diag = reference(triple, push, W, H, gw, gh)
        if a.fit:
            # HOW MUCH of the GPU's own change does the reference explain? Compare the CHANGE each
            # side makes to cur[uv], on pixels that carry a gradient (a flat pixel cannot show a
            # displacement, so including it only dilutes the fit). k is the least-squares scale:
            # k = 1 means the reference moves content exactly as far as the GPU did; corr is how much
            # of the GPU's change the reference explains at all. Exact-match alone cannot tell a
            # slightly-too-large displacement from a wrong one; this can.
            g = triple['next'][..., :3].astype(np.float32).mean(-1)
            lv = triple['live'][..., :3].astype(np.float32).mean(-1)
            gx = np.zeros_like(g); gy = np.zeros_like(g)
            gx[:, 1:-1] = (g[:, 2:] - g[:, :-2]) * 0.5
            gy[1:-1, :] = (g[2:, :] - g[:-2, :]) * 0.5
            msk = (np.abs(gx) + np.abs(gy)) > 20.0
            pred = np.clip(ref, 0, 1).mean(-1) * 255.0 - g
            obs = lv - g
            k = float((pred[msk] * obs[msk]).sum() / max(float((pred[msk] ** 2).sum()), 1e-9))
            cc = float(np.corrcoef(pred[msk], obs[msk])[0, 1])
            diag['fit_k'] = k; diag['fit_corr'] = cc; diag['fit_n'] = int(msk.sum())
        stats, dmax = compare(ref, triple['live'], worst_path=a.save_worst)
        stats['id'] = rid; stats['t'] = float(r['t']); stats.update(diag)
        results.append(stats)
        print(f'  {rid} t={float(r["t"]):.4f}: exact {stats["exact"]:6.2f}%  <=1 {stats["le1"]:6.2f}%  '
              f'<=2 {stats["le2"]:6.2f}%  <=4 {stats["le4"]:6.2f}%  max {stats["maxd"]:3d}  '
              f'mean {stats["mean"]:.3f}  >8: {stats["over8"]}  | vb_w={diag["vb_w"]:.3f} '
              f'anchor={"y" if diag["anchor_active"] else "n"} ambig={diag["ambig_hits"]} '
              f'reclaim={diag["reclaim_frac"]*100:.1f}% stasis={diag["stasis_frac"]*100:.1f}%'
              + (f' | fit k={diag["fit_k"]:.3f} corr={diag["fit_corr"]:.3f} n={diag["fit_n"]}'
                 if 'fit_k' in diag else ''))
        if a.save_worst:
            os.makedirs(a.save_worst, exist_ok=True)
            np.clip(dmax, 0, 255).astype(np.uint8).tofile(
                os.path.join(a.save_worst, f'{rid}_absdiff.r8'))

    if results:
        ex = np.array([r['exact'] for r in results])
        l1 = np.array([r['le1'] for r in results])
        l2 = np.array([r['le2'] for r in results])
        mx = np.array([r['maxd'] for r in results])
        print(f'\nover {len(results)} triples:')
        print(f'  exact match : mean {ex.mean():.2f}%  min {ex.min():.2f}%  max {ex.max():.2f}%')
        print(f'  within 1 LSB: mean {l1.mean():.2f}%  min {l1.min():.2f}%')
        print(f'  within 2 LSB: mean {l2.mean():.2f}%  min {l2.min():.2f}%')
        print(f'  worst pixel : max {mx.max()}  median-of-max {int(np.median(mx))}')
        if 'fit_k' in results[0]:
            fk = np.array([r['fit_k'] for r in results]); fc = np.array([r['fit_corr'] for r in results])
            print(f'  displacement: k mean {fk.mean():.3f} (min {fk.min():.3f} max {fk.max():.3f})  '
                  f'corr mean {fc.mean():.3f}')
    if a.json:
        json.dump(results, open(a.json, 'w'), indent=1)
        print('wrote', a.json)
    print('RESULT:', 'no refusals' if fails == 0 else f'{fails} triple(s) refused or failed')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
