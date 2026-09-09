# gdump_xcheck.py - gate G4 of docs/planning/GDUMP_PLAN.md: in a run that carried BOTH taps, every frame --qdump
# dumped must be BYTE-IDENTICAL to the frame --gdump streamed for the same tick. The join key is qdump_xref.tsv
# (seq <-> qdump index), written by P on the sync path (P7-6: qdump's own manifest carries no tick).
#
#   python gdump_xcheck.py --gdump <gdump dir> --qdump <qdump dir>
#
# Prints one line per pair and a verdict; exit 0 only when every joined pair matches and at least one was joined.
# Made with my soul - Swately <3
import argparse, io, os, sys
import numpy as np
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')


def read_hdr(d):
    h = {}
    for ln in open(os.path.join(d, 'gdump.hdr'), encoding='utf-8'):
        t = ln.split()
        if t and not t[0].startswith('#'):
            h[t[0]] = t[1:]
    return h


def read_ticks(d):
    rows = {}
    for ln in open(os.path.join(d, 'ticks.tsv'), encoding='utf-8'):
        t = ln.split()
        if not t or t[0].startswith('#'):
            continue
        # seq slot tick t gen tgen pair decision flags qpc live_off push_off pairset
        rows[int(t[0])] = dict(seq=int(t[0]), tick=int(t[2]), t=float(t[3]), live_off=int(t[10]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gdump', required=True)
    ap.add_argument('--qdump', required=True)
    a = ap.parse_args()
    h = read_hdr(a.gdump)
    W, H = int(h['live_div'][1]), int(h['live_div'][2])
    nbytes = W * H * 4
    ticks = read_ticks(a.gdump)
    live = np.memmap(os.path.join(a.gdump, 'live.rgba'), dtype=np.uint8, mode='r')
    xref = os.path.join(a.gdump, 'qdump_xref.tsv')
    if not os.path.exists(xref):
        print('FAIL: no qdump_xref.tsv in', a.gdump, '(the run did not carry --qdump, or P never dumped on a captured tick)')
        return 1
    joined = ok = 0
    for ln in open(xref, encoding='utf-8'):
        t = ln.split()
        if len(t) < 2 or t[0].startswith('#'):
            continue
        seq, qi = int(t[0]), int(t[1])
        r = ticks.get(seq)
        qp = os.path.join(a.qdump, 'q%06d_live.rgba' % qi)
        if r is None or not os.path.exists(qp):
            print('  seq %d <-> q%06d: %s' % (seq, qi, 'no ticks.tsv row' if r is None else 'qdump file missing'))
            joined += 1
            continue
        g = np.asarray(live[r['live_off']:r['live_off'] + nbytes])
        q = np.fromfile(qp, dtype=np.uint8)
        joined += 1
        if q.size != g.size:
            print('  seq %d <-> q%06d: SIZE %d vs %d' % (seq, qi, g.size, q.size))
            continue
        diff = int(np.count_nonzero(q != g))
        if diff == 0:
            ok += 1
            print('  seq %d <-> q%06d: identical (%d bytes, tick %d, t=%.4f)' % (seq, qi, nbytes, r['tick'], r['t']))
        else:
            print('  seq %d <-> q%06d: %d bytes differ (max |d| = %d)' % (seq, qi, diff, int(np.max(np.abs(q.astype(np.int16) - g.astype(np.int16))))))
    print('G4: %d joined, %d byte-identical' % (joined, ok))
    return 0 if joined > 0 and ok == joined else 1


if __name__ == '__main__':
    sys.exit(main())
