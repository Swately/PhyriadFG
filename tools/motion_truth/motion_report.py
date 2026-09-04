#!/usr/bin/env python3
"""motion_report.py — the M1 table, with its reliability attached (MOTION_TRUTH phase T5).

WHAT IT PRODUCES. From one or two `detections.csv` files (T4's output): per class × marker size × phase
bin — the mean and p95 of the position error against the translational model and against the
analytic truth, the same error as a PHASE error in milliseconds (err_px / |v| · 1000/fps: how far in
TIME the frame is from where it claims to be), the miss rate, the degraded rate (present but below
the NCC floor — the FG smeared it) and the ghost rate (n_peaks ≥ 2). Recorded, not gated.

THE RULE THAT MAKES A NUMBER USABLE (DI-3). A cell measured once is an anecdote. Given TWO runs, the
report computes the Pearson r between the runs' per-marker errors per class and prints it beside every
table; a class with r < 0.5 is printed UNRELIABLE and excluded from any verdict line. A single run is
reported with `r = n/a` on every cell and is explicitly NOT a baseline.

HOW THE TWO RUNS ARE MATCHED. The plan says by (k_prev, marker). Two independent runs of a looping
sequence rarely land the coverage sampler on the same source frames, so that intersection can be
empty; when it is, the report falls back to matching by (marker, phase bin) — each cell being the
mean over that run's triples in the bin — and SAYS SO, with n. The fallback compares the FG's
per-phase behaviour per marker, which is what M1 is about; the exact-frame match is stronger when
available, so it is tried first.

    python tools/motion_truth/motion_report.py --zoo ZOO --run A.csv [--run B.csv] [--md OUT.md]

numpy + stdlib only. Made with my soul - Swately <3
"""
import argparse, csv, json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from marker_zoo import eval_traj   # noqa: E402

NBINS = 10
CLASSES = ('linear', 'accel', 'circular', 'crossing', 'hud', 'fast')


def load_rows(path):
    rows = list(csv.DictReader(open(path, encoding='utf-8')))
    for r in rows:
        for k in ('t', 'err_model_px', 'err_true_px', 'ncc', 'ncc_max', 'near_dist', 'obs_x', 'obs_y',
                  'exp_model_x', 'exp_model_y'):
            r[k] = float(r[k]) if r.get(k) not in (None, '', 'nan') else float('nan')
        for k in ('k_prev', 'k_next', 'span', 'marker', 'size', 'n_peaks'):
            r[k] = int(r[k])
    return rows


def tbin(t):
    return min(NBINS - 1, int(t * NBINS))


def speed_px_per_frame(traj, marker_id, k_prev, span, fps):
    m = next(m for m in traj['markers'] if m['id'] == marker_id)
    a = eval_traj(m['model'], k_prev / fps); b = eval_traj(m['model'], (k_prev + span) / fps)
    return float(np.hypot(*(b - a)))


def cell_stats(rows):
    """One aggregate over a list of live-plane rows."""
    n = len(rows)
    found = [r for r in rows if r['n_peaks'] > 0]
    miss = [r for r in rows if r['n_peaks'] == 0]
    degraded = [r for r in miss if r['ncc_max'] >= 0.3]
    ghost = [r for r in rows if r['n_peaks'] >= 2]
    em = np.array([r['err_model_px'] for r in found]) if found else np.array([])
    et = np.array([r['err_true_px'] for r in found]) if found else np.array([])
    ms = np.array([r['phase_ms'] for r in found if np.isfinite(r['phase_ms'])]) if found else np.array([])
    return {
        'n': n, 'found': len(found), 'miss': len(miss), 'degraded': len(degraded), 'ghost': len(ghost),
        'em_mean': float(em.mean()) if em.size else float('nan'),
        'em_p95': float(np.percentile(em, 95)) if em.size else float('nan'),
        'et_mean': float(et.mean()) if et.size else float('nan'),
        'ms_mean': float(ms.mean()) if ms.size else float('nan'),
        'ms_p95': float(np.percentile(ms, 95)) if ms.size else float('nan'),
    }


def pearson(a, b):
    a = np.asarray(a, np.float64); b = np.asarray(b, np.float64)
    if a.size < 3 or a.std() < 1e-12 or b.std() < 1e-12:
        return float('nan')
    return float(np.corrcoef(a, b)[0, 1])


def reliability(A, B):
    """Per-class r between two runs' live-plane errors. Exact (k_prev, marker) matching first; if the
    two runs share too few source frames, the (marker, phase-bin) mean fallback, and the method is
    returned with the numbers so the table can say which it used."""
    out = {}
    for cls in CLASSES:
        ra = [r for r in A if r['class'] == cls and r['plane'] == 'live' and r['n_peaks'] > 0]
        rb = [r for r in B if r['class'] == cls and r['plane'] == 'live' and r['n_peaks'] > 0]
        da = {(r['k_prev'], r['marker']): r['err_model_px'] for r in ra}
        db = {(r['k_prev'], r['marker']): r['err_model_px'] for r in rb}
        common = sorted(set(da) & set(db))
        if len(common) >= 6:
            out[cls] = {'r': pearson([da[k] for k in common], [db[k] for k in common]),
                        'n': len(common), 'method': 'exact (k_prev, marker)'}
            continue
        def agg(rs):
            d = {}
            for r in rs:
                d.setdefault((r['marker'], tbin(r['t'])), []).append(r['err_model_px'])
            return {k: float(np.mean(v)) for k, v in d.items()}
        ga, gb = agg(ra), agg(rb)
        common = sorted(set(ga) & set(gb))
        out[cls] = {'r': pearson([ga[k] for k in common], [gb[k] for k in common]) if common else float('nan'),
                    'n': len(common), 'method': 'fallback (marker, phase-bin) means'}
    return out


def fmt(v, w=6, d=3):
    return ('%*.*f' % (w, d, v)) if np.isfinite(v) else ('%*s' % (w, '—'))


def main():
    # the Windows console is cp1252; the table carries en-dashes and a >= sign
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except AttributeError:
        pass
    ap = argparse.ArgumentParser()
    ap.add_argument('--zoo', required=True)
    ap.add_argument('--run', action='append', required=True, help='a detections.csv; give it twice for DI-3')
    ap.add_argument('--label', default='the shipping default')
    ap.add_argument('--md', default=None, help='write the baseline table as Markdown here')
    ap.add_argument('--fps', type=float, default=None)
    a = ap.parse_args()

    traj = json.load(open(os.path.join(a.zoo, 'trajectories.json'), encoding='utf-8'))
    fps = a.fps or float(traj['fps'])
    runs = [load_rows(p) for p in a.run]
    for rows in runs:
        for r in rows:
            if r['plane'] == 'live' and r['n_peaks'] > 0:
                v = speed_px_per_frame(traj, r['marker'], r['k_prev'], r['span'], fps)
                r['phase_ms'] = (r['err_model_px'] / v) * 1000.0 / fps if v > 1e-6 else float('nan')
            else:
                r['phase_ms'] = float('nan')

    two = len(runs) >= 2
    rel = reliability(runs[0], runs[1]) if two else {}
    live_all = [r for rows in runs for r in rows if r['plane'] == 'live']
    bg = traj['background']
    scene = f"{bg['class']} background, pan {bg['pan_px_s'][0]:g} px/s, {traj['width']}x{traj['height']} @ {fps:g} fps"

    lines = []
    P = lines.append
    P(f'# MOTION_TRUTH baseline — {a.label}')
    P('')
    P(f'> Scene: {scene}. Runs: {len(runs)} ({", ".join(os.path.basename(os.path.dirname(p)) or p for p in a.run)}), '
      f'{sum(1 for r in runs[0] if r["plane"]=="live")//max(1,len(traj["markers"]))} triples in run 1. '
      f'Errors are on the `live` plane, in pixels, against the translational model of the captured pair '
      f'(`err_model`) and the analytic truth (`err_true`); `phase_ms` = err / |v| · 1000/fps, how far in '
      f'TIME the generated frame is from where it claims to be. `r` is the Pearson correlation of per-marker '
      f'errors between the two runs (DI-3); **a class with r < 0.5 is UNRELIABLE and carries no verdict.**')
    if not two:
        P('>')
        P('> **ONE RUN ONLY — r = n/a on every cell. This is NOT a baseline.** (DI-3)')
    P('')
    P('## Per class' + (' — with run-to-run r' if two else ''))
    P('')
    hdr = '| class | n | found | degraded | absent | ghost | err_model mean | p95 | err_true mean | phase ms mean | p95 |' + (' r | match |' if two else '')
    P(hdr); P('|' + '---|' * (hdr.count('|') - 1))
    verdict_ok = []
    for cls in CLASSES:
        rs = [r for r in live_all if r['class'] == cls]
        if not rs:
            continue
        s = cell_stats(rs)
        row = (f"| {cls} | {s['n']} | {s['found']} | {s['degraded']} | {s['miss']-s['degraded']} | {s['ghost']} | "
               f"{fmt(s['em_mean'])} | {fmt(s['em_p95'])} | {fmt(s['et_mean'])} | {fmt(s['ms_mean'],6,2)} | {fmt(s['ms_p95'],6,2)} |")
        if two:
            rr = rel[cls]
            tag = '' if (np.isfinite(rr['r']) and rr['r'] >= 0.5) else ' **UNRELIABLE**'
            row += f" {fmt(rr['r'],5,2)}{tag} (n={rr['n']}) | {rr['method']} |"
            if not tag:
                verdict_ok.append(cls)
        P(row)
    P('')
    P('## Per class × size')
    P('')
    hdr2 = '| class | size | n | found | degraded | err_model mean | p95 | phase ms |'
    P(hdr2); P('|' + '---|' * (hdr2.count('|') - 1))
    for cls in CLASSES:
        for sz in (6, 12, 24):
            rs = [r for r in live_all if r['class'] == cls and r['size'] == sz]
            if not rs:
                continue
            s = cell_stats(rs)
            P(f"| {cls} | {sz} | {s['n']} | {s['found']} | {s['degraded']} | {fmt(s['em_mean'])} | {fmt(s['em_p95'])} | {fmt(s['ms_mean'],6,2)} |")
    P('')
    P(f'## Per phase bin (all classes except `fast`)')
    P('')
    hdr3 = '| t bin | n | found | degraded | err_model mean | p95 |'
    P(hdr3); P('|' + '---|' * (hdr3.count('|') - 1))
    for b in range(NBINS):
        rs = [r for r in live_all if r['class'] != 'fast' and tbin(r['t']) == b]
        if not rs:
            continue
        s = cell_stats(rs)
        P(f"| [{b/NBINS:.1f},{(b+1)/NBINS:.1f}) | {s['n']} | {s['found']} | {s['degraded']} | {fmt(s['em_mean'])} | {fmt(s['em_p95'])} |")
    P('')
    if two:
        P('## Verdict line')
        P('')
        if verdict_ok:
            P('Classes with r ≥ 0.5, the only ones a verdict may cite: **' + ', '.join(verdict_ok) + '**. ' +
              'Every other class above is measured but UNRELIABLE at this run count and must not be used per cell.')
        else:
            P('**No class reached r ≥ 0.5. No cell above may be used in a verdict.** The measurement exists; its reliability does not yet.')
    P('')
    P('*Generated by tools/motion_truth/motion_report.py — Made with my soul - Swately <3*')
    text = '\n'.join(lines)
    print(text)
    if a.md:
        os.makedirs(os.path.dirname(os.path.abspath(a.md)), exist_ok=True)
        open(a.md, 'w', encoding='utf-8').write(text + '\n')
        print(f'\nwrote {a.md}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
