#!/usr/bin/env python3
"""scene_truth — the cut-frame review page: candidate vs both real endpoints it bridged.

A cut frame has no interpolation truth (scene_align.py: the two real frames were not k apart, the
looped player's seam or a bridged drop) so it cannot go on the ordinary review page (scene_review.py),
which shows a candidate against ITS truth frame. What a cut CAN be shown against is what it actually
bridged: the two real endpoints, and |candidate − nearer real| ×4 — the same amplification scene_step
uses in its own difference view — so a ghost or a blur reads at a glance next to the numbers
score_cut_frame already put on it.

Reads the --cuts JSON scene_report.py writes (the 'cuts' key of its per-run dict); does not re-score.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, html
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scene_report as SR   # noqa: E402

png, u8, load_rgb = SR.png, SR.u8, SR.load_rgb

_WD = {}   # per-process corpus dims, loaded once (see _worker_init)


def _worker_init(d):
    global _WD
    t, _ = SR.load_corpus(d)
    _WD = {'d': d, 'W': t['width'], 'H': t['height']}


def _write_cut(args):
    """One cut's four PNGs (candidate, real A, real B, |candidate − nearer| ×4), written to
    out_dir/img/. Runs in a pool worker; reads only its args and the per-process corpus dims."""
    out_dir, arm, row = args
    d, W, H = _WD['d'], _WD['W'], _WD['H']
    n = row['cut']
    base = '%s_%s_cut%03d' % (os.path.basename(os.path.normpath(d)), arm, n)
    cand = load_rgb(os.path.join(d, 'arms', arm, 'cut_%d.rgba' % n), W, H)
    A = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % row['N']), W, H)
    B = load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % row['N1']), W, H)
    nearer = A if row['nearer'] == 'A' else B
    png(os.path.join(out_dir, 'img', base + '_cand.png'), u8(cand))
    png(os.path.join(out_dir, 'img', base + '_a.png'), u8(A))
    png(os.path.join(out_dir, 'img', base + '_b.png'), u8(B))
    png(os.path.join(out_dir, 'img', base + '_d.png'), u8(np.clip(np.abs(cand - nearer) * 4.0, 0, 1)))
    return base


def build(run, K, arm, json_path, out_dir, jobs):
    data = json.load(open(json_path, encoding='utf-8'))
    key = next((k for k in data if k.endswith('|k%d' % K)
                and os.path.normpath(k.split('|k')[0]) == os.path.normpath(run)), None)
    if key is None:
        sys.exit('%s: no entry for %s at k=%d' % (json_path, run, K))
    cuts = data[key].get('cuts', {}).get(arm)
    if not cuts:
        sys.exit('%s: no cut rows for arm %s at k=%d (run --cuts on scene_report.py first)' % (json_path, arm, K))
    os.makedirs(os.path.join(out_dir, 'img'), exist_ok=True)
    args = [(out_dir, arm, r) for r in cuts]
    if jobs <= 1 or len(args) < 2:
        _worker_init(run)
        bases = [_write_cut(a) for a in args]
    else:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=min(jobs, len(args)), initializer=_worker_init, initargs=(run,)) as pool:
            bases = list(pool.map(_write_cut, args))
    write_html(out_dir, list(zip(bases, cuts)), os.path.basename(os.path.normpath(run)), K, arm)
    print('%d cut card(s) -> %s' % (len(bases), os.path.join(out_dir, 'index.html')))


def write_html(out_dir, cards, corpus, K, arm):
    esc = lambda s: html.escape(str(s))
    css = """
:root{--bg:#f6f5f2;--ink:#1c1b19;--mut:#6b675f;--line:#dcd8d0;--acc:#2f6f5e;--red:#b03a2e}
@media(prefers-color-scheme:dark){:root{--bg:#14140f;--ink:#e8e4da;--mut:#9b968a;--line:#2d2b25;--acc:#6fbfa5;--red:#e0705f}}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{padding:20px 28px;border-bottom:1px solid var(--line)} h1{margin:0 0 4px;font-size:20px;font-weight:600}
.sub{color:var(--mut)} .card{display:grid;grid-template-columns:repeat(4,200px) 1fr;gap:12px;padding:16px 28px;border-bottom:1px solid var(--line);align-items:start}
.card img{width:200px;image-rendering:pixelated;border:1px solid var(--line);display:block}
.card .cap{font-size:11px;color:var(--mut);margin-top:3px} .meta{font-size:13px} .meta b{font-weight:600}
"""
    parts = ['<!doctype html><meta charset="utf-8"><title>scene_truth cuts</title><style>%s</style>' % css,
             '<header><h1>scene_truth — cut frames · %s · k = %d · arm <code>%s</code></h1>' % (esc(corpus), K, esc(arm)),
             '<div class="sub">%d cards. Candidate vs both real endpoints it bridged; |candidate − nearer real| ×4.</div></header>'
             % len(cards)]
    for base, r in cards:
        parts.append('<div class="card">')
        for sfx, cap in (('_cand', 'candidate'), ('_a', 'real A (N=%d)' % r['N']), ('_b', 'real B (N1=%d)' % r['N1']),
                         ('_d', '|candidate − nearer| ×4')):
            parts.append('<div><img src="img/%s%s.png"><div class="cap">%s</div></div>' % (esc(base), sfx, esc(cap)))
        ha = SR._cut_mean(r, 'halluc_vs_A'); hb = SR._cut_mean(r, 'halluc_vs_B')
        ma = SR._cut_mean(r, 'missing_vs_A'); mb = SR._cut_mean(r, 'missing_vs_B')
        parts.append('<div class="meta"><div><b>cut %d</b> · triple %s · t=%.3f · nearer %s</div>'
                     '<div class="sub">halluc vs A %.0f px² · vs B %.0f px² · missing vs A %.0f px² · vs B %.0f px² · '
                     'sharp vs A %.3f · vs B %.3f · graceful %.4f · hold graceful %.4f · blend graceful %.4f</div></div></div>'
                     % (r['cut'], esc(r['triple']), r['t'], r['nearer'], ha, hb, ma, mb,
                        r['sharp_vs_A'], r['sharp_vs_B'], r['graceful'], r['hold']['graceful'], r['blend']['graceful']))
    open(os.path.join(out_dir, 'index.html'), 'w', encoding='utf-8', newline='\n').write('\n'.join(parts) + '\n')


def main():
    ap = argparse.ArgumentParser(description='scene_truth — cut-frame review page')
    ap.add_argument('--run', required=True, help='corpus dir')
    ap.add_argument('--k', type=int, required=True)
    ap.add_argument('--arm', required=True, help='the live arm whose cut frames to show')
    ap.add_argument('--json', required=True, help='the --cuts JSON output of scene_report.py')
    ap.add_argument('--out', required=True)
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 2))
    a = ap.parse_args()
    build(a.run, a.k, a.arm, a.json, a.out, a.jobs)


if __name__ == '__main__':
    main()
