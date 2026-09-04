#!/usr/bin/env python3
"""check_qdump_plus.py — validate a `--qdump+` replay record (S2.T1's verification).

A triple plus `t` is not a replay record; the sidecars are what make R3's M4 gate runnable at all.
This checks they are actually usable, rather than merely present:

  * every manifest line carries the sidecar tokens, and every file it names exists;
  * the MV and SAD planes are exactly mvw*mvh*4 bytes (RG16F);
  * the MV plane DECODES as float16 pairs and its magnitudes are plausible pixel displacements
    (a wildly wrong stride or format shows up here as garbage, not as a size mismatch);
  * push.bin is `pushsz` bytes and `pushsz` is constant across ticks;
  * the first four floats of the push block are the core's own parameters, which have known
    defaults (residual_ceil, improvement_frac, agreement_threshold, t) — t must equal the
    manifest's own t for that tick. That is the check that proves the push bytes belong to
    THIS tick and not to a stale copy.

  python tools/check_qdump_plus.py <dump_dir> [--max-mv-px N]

numpy + stdlib only. Made with my soul - Swately <3
"""
import argparse, os, struct, sys
import numpy as np

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
    for r in rows:
        rid = r['id']
        missing = [k for k in ('mv', 'sad', 'push', 'pushsz', 'gen', 'mvw', 'mvh', 'gme_valid', 'gme') if k not in r]
        if missing:
            print(f'  FAIL {rid}: manifest lacks {missing} (not a + record)'); fails += 1; continue
        mvw, mvh, pushsz = int(r['mvw']), int(r['mvh']), int(r['pushsz'])
        want = mvw * mvh * 4
        pushsz_seen.add(pushsz)
        for key, expect in (('mv', want), ('sad', want), ('push', pushsz)):
            p = os.path.join(a.dir, r[key])
            if not os.path.exists(p):
                print(f'  FAIL {rid}: missing {r[key]}'); fails += 1; continue
            got = os.path.getsize(p)
            if got != expect:
                print(f'  FAIL {rid}: {r[key]} is {got} B, expected {expect}'); fails += 1
        # decode the MV plane: RG16F pairs, pixel units
        mv = np.fromfile(os.path.join(a.dir, r['mv']), dtype=np.float16).astype(np.float32)
        if mv.size != mvw * mvh * 2:
            print(f'  FAIL {rid}: MV decodes to {mv.size} halves, expected {mvw*mvh*2}'); fails += 1; continue
        mv = mv.reshape(mvh, mvw, 2)
        mag = np.sqrt((mv ** 2).sum(axis=2))
        finite = np.isfinite(mag)
        if not finite.all():
            print(f'  FAIL {rid}: MV plane has {int((~finite).sum())} non-finite texels'); fails += 1
        mx, p99 = float(np.nanmax(mag)), float(np.nanpercentile(mag[finite], 99))
        moving = float((mag > 0.5).mean() * 100.0)
        # the push block: first four floats are the core contract (res_ceil, improv, agree, t)
        raw = open(os.path.join(a.dir, r['push']), 'rb').read()
        rc, ci, ag, tp = struct.unpack_from('<4f', raw, 0)
        tm = float(r['t'])
        t_ok = abs(tp - tm) <= 1e-4
        if not t_ok:
            print(f'  FAIL {rid}: push t={tp:.6f} != manifest t={tm:.6f} (the push bytes are not this tick\'s)')
            fails += 1
        if mx > a.max_mv_px:
            print(f'  WARN {rid}: max |MV| {mx:.2f} px exceeds the plausibility ceiling {a.max_mv_px}')
        print(f'  {rid}: gen={r["gen"]} mv {mvw}x{mvh} |MV| max={mx:.2f} p99={p99:.2f} px, moving={moving:.1f}% '
              f'| push {len(raw)} B rc={rc:g} improv={ci:g} agree={ag:g} t={tp:.4f} (manifest {tm:.4f}) {"OK" if t_ok else "MISMATCH"} '
              f'| gme_valid={r["gme_valid"]}')
    if len(pushsz_seen) != 1:
        print('  FAIL: pushsz varies across ticks:', sorted(pushsz_seen)); fails += 1
    else:
        print(f'pushsz constant = {pushsz_seen.pop()} B')

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
        lo, hi = min(hist.values()), max(hist.values())
        if len(bins) >= 3 and hi > 2 * lo:
            print(f'  WARN: the phase histogram is lopsided ({lo}..{hi} per bin) - some phases are'
                  ' under-represented in this corpus.')
        if len(gens) < 2:
            print('  WARN: every triple came from ONE generation — the record does not exercise the ring.')
    print('RESULT:', 'all checks passed' if fails == 0 else f'{fails} FAILURE(S)')
    return 1 if fails else 0

if __name__ == '__main__':
    sys.exit(main())
