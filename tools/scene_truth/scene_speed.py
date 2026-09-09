#!/usr/bin/env python3
"""scene_truth — the speed test: does the FG see speed, or only pixels per source pair?

The operator's observation (2026-09-08): speed increases hallucination enormously, and there may be a
limit set by the frames per second that capture the motion — "the number of times or exposure it has
to the frames". Stated as a measurable: the FG is given two real frames per pair; what it can know
about the motion between them is bounded by how far the content moved BETWEEN THOSE TWO FRAMES.
That quantity — displacement per source pair, in pixels — is reached two ways:

    the k path      same scene, same speed, every k-th base frame shown     (k = 2, 4, 8, 16)
    the speed path  same scene, k fixed at 4, every object's motion x0.5, x1, x2, x4

Matched pairs land on the same displacement: (k=2 · 1x) ~ (k=4 · 0.5x), (k=8 · 1x) ~ (k=4 · 2x),
(k=16 · 1x) ~ (k=4 · 4x). If the FG's error at matched displacement is the same on both paths, the
FG sees only pixels-per-pair and the operator's intuition holds exactly: the "exposure" is the whole
story, and the limit is the source rate's, not the kernel's. If the speed path is worse than the k
path at equal displacement, absolute speed costs something on its own (the clock, the block matcher's
search range, a temporal term) — and THAT is the kernel's, and findable.

Every point is a live run scored at the FG's own phase; the x-axis is the displacement the scorer
measured for that object in that run (`disp_src_px`), not the nominal one.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, glob
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')


def load_points(pattern, arm='fg'):
    pts = []
    for p in sorted(glob.glob(pattern)):
        d = json.load(open(p, encoding='utf-8'))
        for key, blob in d.items():
            corpus, kk = key.split('|k'); K = int(kk)
            rows = blob['rows'].get(arm) or []
            if not rows:
                continue
            t = json.load(open(os.path.join(corpus, 'truth.json')))
            speed = float(t.get('speed', 1.0)); seed = t.get('seed')
            for o in sorted(set(k for r in rows for k in r['objects'])):
                rs = [r['objects'][o] for r in rows if o in r['objects']]
                if len(rs) < 8:
                    continue
                g = lambda f: float(np.nanmean([x[f] for x in rs]))
                pts.append({'corpus': os.path.basename(os.path.normpath(corpus)), 'seed': seed, 'k': K,
                            'speed': speed, 'obj': int(o), 'n': len(rs), 'disp': g('disp_src_px'),
                            'pos': g('pos_err'), 'shape': g('shape_err'), 'halluc': g('halluc_px'),
                            'missing': g('missing_px'), 'lead': g('lead_px'),
                            'halluc_rel': g('halluc_px') / max(g('area_px'), 1)})
    return pts


def main():
    ap = argparse.ArgumentParser(description='scene_truth — the speed test')
    ap.add_argument('--json', action='append', required=True, help='glob(s) of scorer --json files')
    ap.add_argument('--arm', default='fg')
    ap.add_argument('--obj', type=int, default=1, help='object to plot (1 = the translating sphere)')
    ap.add_argument('--md')
    a = ap.parse_args()
    pts = [p for pat in a.json for p in load_points(pat, a.arm) if p['obj'] == a.obj]
    if not pts:
        sys.exit('no points')
    kpath = sorted([p for p in pts if abs(p['speed'] - 1.0) < 1e-9], key=lambda p: p['disp'])
    spath = sorted([p for p in pts if p['k'] == 4], key=lambda p: p['disp'])
    L = ['# scene_truth — the speed test (arm `%s`, object %d)' % (a.arm, a.obj), '',
         '> x = displacement per SOURCE PAIR as the scorer measured it, px. Two paths reach the same x:',
         '> the **k path** (speed 1x, k varied) and the **speed path** (k = 4, speed varied). If the FG',
         '> sees only pixels-per-pair the two curves coincide.', '',
         '| path | corpus | seed | k | speed | n | **disp px/pair** | pos px | shape px | halluc px² | halluc/area | lead px | missing px² |',
         '|---|---|---|---|---|---|---|---|---|---|---|---|---|']
    for name, path in (('k', kpath), ('speed', spath)):
        for p in path:
            L.append('| %s | %s | %s | %d | %.1fx | %d | **%.2f** | %.3f | %.3f | %.0f | %.3f | %+.1f | %.0f |'
                     % (name, p['corpus'], p['seed'], p['k'], p['speed'], p['n'], p['disp'], p['pos'],
                        p['shape'], p['halluc'], p['halluc_rel'], p['lead'], p['missing']))
    # matched pairs: nearest displacement across the two paths
    L += ['', '## Matched displacements (k path vs speed path)', '',
          '| disp k-path | pos k | disp speed-path | pos speed | ratio speed/k | halluc/area k | halluc/area speed |',
          '|---|---|---|---|---|---|---|']
    for p in kpath:
        if not spath:
            break
        q = min(spath, key=lambda s: abs(np.log(max(s['disp'], 1e-6)) - np.log(max(p['disp'], 1e-6))))
        if abs(np.log(max(q['disp'], 1e-6)) - np.log(max(p['disp'], 1e-6))) > np.log(1.5):
            continue
        L.append('| %.2f | %.3f | %.2f | %.3f | **%.2fx** | %.3f | %.3f |'
                 % (p['disp'], p['pos'], q['disp'], q['pos'], q['pos'] / max(p['pos'], 1e-9), p['halluc_rel'], q['halluc_rel']))
    # a power-law fit on each path, pos = a * disp^b, so the growth is one number
    for name, path in (('k path', kpath), ('speed path', spath)):
        xs = np.array([p['disp'] for p in path]); ys = np.array([p['pos'] for p in path])
        distinct = len(set(np.round(xs, 2))) >= 2           # two points at one x fit nothing
        if distinct and (xs > 0).all() and (ys > 0).all():
            b, la = np.polyfit(np.log(xs), np.log(ys), 1)
            L.append('')
            L.append('- **%s**: pos ≈ %.3f · disp^%.2f over %d points (a slope of 1 = error proportional to displacement)'
                     % (name, np.exp(la), b, len(xs)))
    L += ['', '*Generated by tools/scene_truth/scene_speed.py — Made with my soul - Swately <3*']
    print('\n'.join(L))
    if a.md:
        open(a.md, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')


if __name__ == '__main__':
    main()
