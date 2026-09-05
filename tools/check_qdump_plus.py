#!/usr/bin/env python3
"""check_qdump_plus.py — validate a `--qdump+` replay record (S2.T1 / T1b / T1c verification).

A triple plus `t` is not a replay record; the sidecars are what make R3's M4 gate runnable at all.
This checks they are actually usable, rather than merely present:

  * every manifest line carries the sidecar tokens, and every file it names exists;
  * each plane is exactly its declared size (RG16F 4 B/texel, RGBA16F 8, R8 1);
  * the MV planes DECODE as float16 pairs and their magnitudes are plausible pixel displacements
    (a wildly wrong stride or format shows up here as garbage, not as a size mismatch);
  * push.bin is `pushsz` bytes and `pushsz` is constant across ticks;
  * the first four floats of the push block are the core's own parameters (residual_ceil,
    improvement_frac, agreement_threshold, t) — t must equal the manifest's own t for that tick.
    That is the check that proves the push bytes belong to THIS tick and not to a stale copy;
  * REPLAYABILITY: the push block states which features are armed, and each armed feature makes
    the warp read a particular binding. The record is replayable only if every plane those
    bindings need is in it. This is the check that decides whether a record can serve as R3's
    M4 corpus — a full record of the wrong scope is not a corpus;
  * CORPUS COVERAGE: the phase and generation spread of the record.

  python tools/check_qdump_plus.py <dump_dir> [--max-mv-px N]

numpy + stdlib only. Made with my soul - Swately <3
"""
import argparse, os, struct, sys
import numpy as np

# The warp push block, in order. 58 floats = 232 B; the ordering was verified against a live
# q*_push.bin whose 4th float matched the manifest's own t on every tick.
PUSH_NAMES = """residual_ceil improvement_frac agreement_threshold t soft_gate commit_thresh commit_real
occl_thresh div_eps rescue_on mv_guided gme_on gme_a gme_b gme_c gme_d gme_e gme_f matte_on matte_thresh
stasis_thresh inertia_thresh crescent_on appear_on appear_band travel_on contour_on obj_crescent_on
phase_anchor_on ambig_on member_commit_on commit_default_on onepos_on onepos_band disoccl_commit_on
bg_snap_on bg_snap_strength bg_snap_norm extrap vblend_on vblend_t0 vblend_strength band_xfade
vblend_exact ts_smooth mc_on mc_nperturb mc_perturb mc_disp mc_edge cam_lead_x cam_lead_y
disoccl_hardpick predict_p2 blend_solo mv_edge_snap single_track bg_reclaim""".split()

# An armed feature -> the plane the warp then reads. A `None` token means --qdump+ dumps no such
# plane, so arming that feature makes the record unreplayable by construction rather than by
# omission — and the audit says which binding, so the gap is actionable.
REPLAY_NEEDS = [
    ('backward MV',          lambda p: p['occl_thresh'] > 0 or p['phase_anchor_on'] > 0.5, 'mvb',  'binding 5'),
    ('SAD candidates',       lambda p: p['ambig_on'] > 0.5 and p['gme_on'] > 0.5,          'c2',   'binding 10'),
    # The dissidence masks are gated on matte_on at every ordinary site (the phase-anchor claim, the
    # ambiguity object proxy, the matte block, the member-commit strength). Two further sites: the
    # edge-snap G1 variant guides on them, and the bg-snap / band-xfade reveal-fill reads both. gme_on
    # alone does NOT read them -- an earlier version of this table said it did, and the shader says
    # otherwise (grep texture(u_dissidence and read each enclosing gate).
    ('dissidence (fwd)',     lambda p: p['matte_on'] > 0.5
                                       or 0.5 < p['mv_edge_snap'] < 2.0
                                       or ((p['bg_snap_on'] > 0.5 or p['band_xfade'] > 0)
                                           and p['occl_thresh'] > 0),                       'dis',  'binding 6'),
    ('dissidence (bwd)',     lambda p: p['matte_on'] > 0.5
                                       or ((p['bg_snap_on'] > 0.5 or p['band_xfade'] > 0)
                                           and p['occl_thresh'] > 0),                       'disb', 'binding 7'),
    ('persistence',          lambda p: p['inertia_thresh'] > 0,                            'per',  'binding 8'),
    ('target-generation MV', lambda p: p['vblend_on'] > 0.5,                               'mvt',  'binding 12'),
    # the consensus pass rewrites binding 2 on the GPU; with it armed the shader reads mv1, not mv
    ('post-consensus MV',    lambda p: p['mv_guided'] > 0.5,                               'mv1',  'binding 2 after mv_median.comp'),
    ('previous output',      lambda p: p['ts_smooth'] > 0,                                 None,   'binding 13'),
    ('iGPU contour field',   lambda p: p['bg_snap_on'] > 0.5 or p['disoccl_hardpick'] > 0
                                       or p['mc_on'] > 0.5,                                None,   'binding 11'),
]

def parse_manifest(path):
    size = None; rows = []
    for ln in open(path, encoding='utf-8', errors='replace'):
        ln = ln.strip()
        if ln.startswith('size '):
            _, w, h = ln.split()[:3]; size = (int(w), int(h)); continue
        if not ln.startswith('triple '):
            continue
        toks = ln.split()
        rec = {'id': toks[1]}
        for t in toks[2:]:
            if '=' in t:
                k, v = t.split('=', 1); rec[k] = v
        rows.append(rec)
    return size, rows

def mv_stats(path, mvw, mvh):
    """Decode an RG16F plane; return (max, p99, moving%) or an error string."""
    a = np.fromfile(path, dtype=np.float16).astype(np.float32)
    if a.size != mvw * mvh * 2:
        return f'decodes to {a.size} halves, expected {mvw*mvh*2}'
    a = a.reshape(mvh, mvw, 2)
    mag = np.sqrt((a ** 2).sum(axis=2))
    fin = np.isfinite(mag)
    if not fin.all():
        return f'{int((~fin).sum())} non-finite texels'
    return (float(np.nanmax(mag)), float(np.nanpercentile(mag[fin], 99)),
            float((mag > 0.5).mean() * 100.0))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('--max-mv-px', type=float, default=64.0,
                    help='plausibility ceiling for |MV| in pixels (default 64)')
    a = ap.parse_args()
    man = os.path.join(a.dir, 'manifest.txt')
    if not os.path.exists(man):
        print('FAIL: no manifest.txt in', a.dir); return 1
    size, rows = parse_manifest(man)
    if not rows:
        print('FAIL: manifest has no triples'); return 1
    print(f'manifest: size={size} triples={len(rows)}')

    fails = 0
    pushsz_seen = set()
    push0 = None
    for r in rows:
        rid = r['id']
        missing = [k for k in ('mv', 'sad', 'push', 'pushsz', 'gen', 'mvw', 'mvh', 'gme_valid', 'gme')
                   if k not in r]
        if missing:
            print(f'  FAIL {rid}: manifest lacks {missing} (not a + record)'); fails += 1; continue
        mvw, mvh, pushsz = int(r['mvw']), int(r['mvh']), int(r['pushsz'])
        rg16f, rgba16f, r8 = mvw * mvh * 4, mvw * mvh * 8, mvw * mvh
        pushsz_seen.add(pushsz)
        # every named plane, at its declared size. '-' means the feature was off for this run.
        # mv1 = wapMVA read back AFTER the consensus pass (mv_median.comp): the field the shader actually
        # sampled. mv= is the host field BEFORE it. A record without mv1 is replayable only for a push
        # with the pass off (mv_guided <= 0.5 and mv_median off); see M1_LOWPHASE_FINDING.md.
        planes = [('mv', rg16f), ('mv1', rg16f), ('sad', rg16f), ('push', pushsz), ('mvb', rg16f), ('mvt', rg16f),
                  ('c2', rgba16f), ('dis', r8), ('disb', r8), ('per', r8)]
        present = set()
        for key, expect in planes:
            nm = r.get(key)
            if nm is None or nm == '-':
                continue
            p = os.path.join(a.dir, nm)
            if not os.path.exists(p):
                print(f'  FAIL {rid}: missing {nm}'); fails += 1; continue
            got = os.path.getsize(p)
            if got != expect:
                print(f'  FAIL {rid}: {nm} is {got} B, expected {expect}'); fails += 1; continue
            present.add(key)
        # the MV planes decode as float16 pairs in pixel units
        line_mv = ''
        for key, label in (('mv', 'MV'), ('mvb', 'MVb'), ('mvt', 'MVt')):
            if key not in present:
                continue
            s = mv_stats(os.path.join(a.dir, r[key]), mvw, mvh)
            if isinstance(s, str):
                print(f'  FAIL {rid}: {r[key]} {s}'); fails += 1; continue
            mx, p99, moving = s
            line_mv += (f'|MV| max={mx:.2f} p99={p99:.2f} px, moving={moving:.1f}% '
                        if key == 'mv' else f'{label} max={mx:.2f} ')
            if mx > a.max_mv_px:
                print(f'  WARN {rid}: max |{label}| {mx:.2f} px exceeds the plausibility ceiling {a.max_mv_px}')
        # the push block: first four floats are the core contract (res_ceil, improv, agree, t)
        raw = open(os.path.join(a.dir, r['push']), 'rb').read()
        rc, ci, ag, tp = struct.unpack_from('<4f', raw, 0)
        tm = float(r['t'])
        t_ok = abs(tp - tm) <= 1e-4
        if not t_ok:
            print(f'  FAIL {rid}: push t={tp:.6f} != manifest t={tm:.6f} '
                  '(the push bytes are not this tick\'s)')
            fails += 1
        if push0 is None and len(raw) >= 4 * len(PUSH_NAMES):
            push0 = dict(zip(PUSH_NAMES, struct.unpack_from('<%df' % len(PUSH_NAMES), raw, 0)))
        extra = '+'.join(sorted(present - {'mv', 'sad', 'push'})) or '(base only)'
        print(f'  {rid}: gen={r["gen"]}->tgen={r.get("tgen","?")} mv {mvw}x{mvh} {line_mv}'
              f'| push {len(raw)} B rc={rc:g} improv={ci:g} agree={ag:g} t={tp:.4f} '
              f'(manifest {tm:.4f}) {"OK" if t_ok else "MISMATCH"} | planes {extra}')
    if len(pushsz_seen) != 1:
        print('  FAIL: pushsz varies across ticks:', sorted(pushsz_seen)); fails += 1
    else:
        print(f'pushsz constant = {pushsz_seen.pop()} B')

    # ── REPLAYABILITY ──────────────────────────────────────────────────────────────────────────
    # The push block states which features are armed; each armed feature makes the warp read a
    # binding. A record missing one of those planes cannot be replayed, however many triples it
    # holds. This is the check that decides whether the record can serve as R3's M4 corpus.
    if push0 is None:
        print('replayability: the push block is shorter than the 58-float contract — cannot audit')
    else:
        armed = [n for n in PUSH_NAMES if abs(push0[n]) > 0.0 and n != 't']
        print('push contract: %d floats; armed = ' % len(PUSH_NAMES) +
              ' '.join(f'{n}={push0[n]:g}' for n in armed))
        blockers = []
        for label, pred, token, binding in REPLAY_NEEDS:
            if not pred(push0):
                continue
            if token is None:
                blockers.append(f'{label} ({binding}) is READ but --qdump+ dumps no such plane')
            elif not all(r.get(token, '-') != '-' for r in rows):
                blockers.append(f'{label} ({binding}) is READ but the record has no `{token}=` plane')
        if blockers:
            print('  NOT REPLAYABLE for this push:')
            for b in blockers:
                print('    - ' + b)
        else:
            print('  REPLAYABLE: every binding this push arms has its plane in the record.')

    # ── CORPUS COVERAGE ────────────────────────────────────────────────────────────────────────
    # A record whose ticks all sit at one phase tests the core at that phase only. R3's M4 gate
    # (byte-identical or explained, over the replay set) is only as strong as the phases the set
    # covers, so coverage is CHECKED, not assumed.
    #
    # THE CEILING IS THE REFRESH RATIO, NOT THE SAMPLER. A locked ladder at panel/source = 4 emits
    # exactly four phases (~0.125, 0.375, 0.625, 0.875 = eighth-bins 1,3,5,7); no sampler can dump a
    # fifth. So read the HISTOGRAM below, not just the bin count: uniform hits across the bins that
    # appear is full coverage of what this configuration can produce. A finer phase sweep needs a
    # different ratio (a non-integer source rate), which is a corpus-DESIGN choice, not a bug here.
    # History: the pre-S2.T1b stride sampler dumped ten triples at t≈0.125 and six at t≈0.375, all in
    # one ring slot, because the dump stalls its own tick and the clock recovers identically each time.
    ts = sorted(float(r['t']) for r in rows if 't' in r)
    gens = sorted({r['gen'] for r in rows if 'gen' in r})
    if ts:
        bins = sorted({min(7, int(t * 8)) for t in ts})    # eighth-of-a-pair buckets, binned EXACTLY as the C++ sampler does
        span = max(ts) - min(ts)
        hist = {b: sum(1 for t in ts if min(7, int(t * 8)) == b) for b in bins}
        print(f'coverage: {len(rows)} triples | t in [{min(ts):.3f},{max(ts):.3f}] span={span:.3f} '
              f'| distinct t-bins(1/8) = {len(bins)} | generations = {sorted(gens)}')
        print('  phase histogram (bin/8 -> triples): ' +
              '  '.join(f'{b}:{n}' for b, n in sorted(hist.items())))
        if len(bins) < 3:
            print('  WARN: fewer than 3 distinct phase bins — this record is NOT a representative M4 corpus.')
            print('        Either the sampler is pinning, or the ladder itself is that coarse. Check the')
            print('        refresh ratio before blaming the sampler.')
        # Singleton bins are almost always the clock ACQUISITION transient: a phase the ladder
        # emitted once before locking. The coverage sampler dumps such a bin once and then cannot
        # dump it again (it never reappears), so counting singletons in the balance test cries wolf
        # on a record that is in fact uniform over its sustained ladder. Test the sustained bins.
        singles = [b for b, n in hist.items() if n == 1]
        sustained = {b: n for b, n in hist.items() if n > 1}
        if singles:
            print(f'  note: {len(singles)} bin(s) seen once ({singles}) - the clock acquisition'
                  ' transient, not part of the locked ladder.')
        if len(sustained) >= 3:
            lo, hi = min(sustained.values()), max(sustained.values())
            if hi > 2 * lo:
                print(f'  WARN: the sustained phase histogram is lopsided ({lo}..{hi} per bin) -'
                      ' some phases are under-represented in this corpus.')
        if len(gens) < 2:
            print('  WARN: every triple came from ONE generation — the record does not exercise the ring.')
    print('RESULT:', 'all checks passed' if fails == 0 else f'{fails} FAILURE(S)')
    return 1 if fails else 0

if __name__ == '__main__':
    sys.exit(main())
