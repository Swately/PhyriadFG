#!/usr/bin/env python3
"""scene_truth — keep or discard runs, by what can and cannot be recomputed.

The operator's rule: quality exact, disk not the constraint, but many runs will be made and most
deleted — so a run must be droppable with one decision and nothing of value lost. What is worth
keeping is exactly what cannot be recomputed:

    frames/ id/ truth.json manifest*.txt   the corpus (the FG's input AND the truth)      KEEP
    arms/fg* (or any non-synthetic arm)    the FG's own output — the measurement           KEEP
    *.json scores, pages                   kilobytes                                       KEEP
    depth/ flow/ flowb/                    exact functions of the geometry (--labels)      recomputable
    arms/{truth,nearest,blend,oracle2,blur} pure functions of the corpus                    recomputable

    list        every run under --root, with size, what it holds, and its KEEP mark
    keep NAME   mark a run KEEP (a marker file; nothing else changes)
    gc          delete the recomputable material in every run NOT marked KEEP (default), or the
                whole run with --all; --dry shows what would go

Made with my soul - Swately <3
"""
import argparse, os, sys, shutil
sys.stdout.reconfigure(encoding='utf-8')
SYNTH = ('truth', 'nearest', 'blend', 'oracle2', 'blur')
RECOMP_DIRS = ('depth', 'flow', 'flowb')


def size(p):
    tot = 0
    for r, _, fs in os.walk(p):
        for f in fs:
            try:
                tot += os.path.getsize(os.path.join(r, f))
            except OSError:
                pass
    return tot


def gb(n):
    return '%.2f GB' % (n / 1e9) if n >= 1e8 else '%.0f MB' % (n / 1e6)


def runs(root):
    for n in sorted(os.listdir(root)):
        p = os.path.join(root, n)
        if os.path.isdir(p) and os.path.exists(os.path.join(p, 'truth.json')):
            yield n, p


def recomputable(p):
    out = [os.path.join(p, d) for d in RECOMP_DIRS if os.path.isdir(os.path.join(p, d))]
    arms = os.path.join(p, 'arms')
    if os.path.isdir(arms):
        out += [os.path.join(arms, a) for a in os.listdir(arms) if a in SYNTH]
    return out


def main():
    ap = argparse.ArgumentParser(description='scene_truth — run keeper')
    ap.add_argument('--root', required=True)
    ap.add_argument('cmd', choices=['list', 'keep', 'gc'])
    ap.add_argument('name', nargs='?')
    ap.add_argument('--all', action='store_true', help='gc: delete whole runs not marked KEEP')
    ap.add_argument('--dry', action='store_true')
    a = ap.parse_args()

    if a.cmd == 'keep':
        p = os.path.join(a.root, a.name or '')
        if not os.path.exists(os.path.join(p, 'truth.json')):
            sys.exit('not a run: %s' % p)
        open(os.path.join(p, 'KEEP'), 'w').write('kept by the operator\n')
        print('KEEP:', p); return

    total = 0
    for n, p in runs(a.root):
        kept = os.path.exists(os.path.join(p, 'KEEP'))
        s = size(p); total += s
        rec = recomputable(p)
        rs = sum(size(x) for x in rec)
        fg = [x for x in (os.listdir(os.path.join(p, 'arms')) if os.path.isdir(os.path.join(p, 'arms')) else []) if x not in SYNTH]
        if a.cmd == 'list':
            print('%-14s %9s  %s  recomputable %8s  fg-arms %s' % (n, gb(s), 'KEEP' if kept else '    ', gb(rs), ','.join(fg) or '-'))
        elif a.cmd == 'gc':
            if kept:
                print('%-14s KEEP — untouched' % n); continue
            if a.all:
                print('%-14s %s DELETE whole run %s' % (n, 'would' if a.dry else '', gb(s)))
                if not a.dry: shutil.rmtree(p)
            else:
                for x in rec:
                    print('%-14s %s delete %s (%s)' % (n, 'would' if a.dry else '', os.path.relpath(x, p), gb(size(x))))
                    if not a.dry: shutil.rmtree(x)
    if a.cmd == 'list':
        print('total %s' % gb(total))


if __name__ == '__main__':
    main()
