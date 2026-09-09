#!/usr/bin/env python3
"""scene_truth — align what the LIVE FG produced with the corpus it was shown (B1, TB-C7).

The live FG paces and drops non-deterministically, so which real pair produced which generated
frame is not known from order. It IS known from content: every source frame carries its BASE index
as a 16-bit barcode, the FG's --qdump writes each generated frame with the two real frames it was
generated from, and decoding the barcodes on those two gives (N, N1) exactly. The FG's own phase t
then places the generated frame at base index N + t·k.

Two honesties, both recorded per frame in arms/fg/align.json:
    pair_ok   N1 − N == k. If not, the FG paired across a dropped frame; the frame is kept but
              flagged, and the scorer is told the real k for that frame.
    t_resid   |t·k − round(t·k)| in base frames. The FG's phase is its own; the truth it is scored
              against is the nearest base frame. A residual of 0.1 at k=4 is 0.1 base frames of
              motion — small, but it is a systematic and it is written down, not absorbed.

Output: arms/fg/f_<mid>.rgba, the FG's generated frame at its aligned base index, in the layout the
scorer and the stepper already read.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, shutil
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'motion_truth'))
from marker_zoo import decode_barcode   # noqa: E402


def main():
    ap = argparse.ArgumentParser(description='scene_truth — align a --qdump run to the corpus')
    ap.add_argument('--qdump', required=True, help='the --qdump directory the FG wrote')
    ap.add_argument('--run', required=True, help='the corpus the FG was shown')
    ap.add_argument('--k', type=int, required=True)
    ap.add_argument('--arm', default='fg')
    a = ap.parse_args()

    t = json.load(open(os.path.join(a.run, 'truth.json')))
    W, H = t['width'], t['height']
    out = os.path.join(a.run, 'arms', a.arm)
    os.makedirs(out, exist_ok=True)
    man = os.path.join(a.qdump, 'manifest.txt')
    if not os.path.exists(man):
        sys.exit('no manifest.txt in %s' % a.qdump)
    rows, dup = [], {}
    for line in open(man, encoding='utf-8', errors='replace'):
        if not line.startswith('triple'):
            continue
        f = dict(x.split('=', 1) for x in line.split()[2:] if '=' in x)
        rd = lambda name: np.fromfile(os.path.join(a.qdump, name), np.uint8).reshape(H, W, 4)
        try:
            prev, nxt, live = rd(f['prev']), rd(f['next']), rd(f['live'])
        except (FileNotFoundError, ValueError) as e:
            rows.append({'triple': line.split()[1], 'error': str(e)}); continue
        N, N1, tt = decode_barcode(prev), decode_barcode(nxt), float(f['t'])
        k_real = N1 - N
        pos = N + tt * k_real
        mid = int(round(pos))
        rec = {'triple': line.split()[1], 'N': N, 'N1': N1, 't': tt, 'k_real': k_real,
               'pair_ok': k_real == a.k, 't_resid': float(abs(pos - mid)), 'mid': mid}
        if not (0 < mid < t['frames']) or mid == N or mid == N1:
            rec['skipped'] = 'lands on a real frame or outside the corpus'
        else:
            if mid in dup:
                rec['dup_of'] = dup[mid]           # two generated frames claim the same base index
            else:
                dup[mid] = rec['triple']
                live.tofile(os.path.join(out, 'f_%06d.rgba' % mid))
        rows.append(rec)
    good = [r for r in rows if 'mid' in r and 'skipped' not in r and 'dup_of' not in r]
    bad_pairs = sum(1 for r in good if not r['pair_ok'])
    json.dump({'k': a.k, 'rows': rows}, open(os.path.join(out, 'align.json'), 'w', encoding='utf-8'), indent=1)
    print('%d triples read, %d aligned to arms/%s/ (%d dup, %d skipped, %d errors); pairs not k apart: %d; '
          't_resid mean %.3f max %.3f base frames'
          % (len(rows), len(good), a.arm, sum('dup_of' in r for r in rows), sum('skipped' in r for r in rows),
             sum('error' in r for r in rows), bad_pairs,
             float(np.mean([r['t_resid'] for r in good])) if good else float('nan'),
             float(np.max([r['t_resid'] for r in good])) if good else float('nan')))


if __name__ == '__main__':
    main()
