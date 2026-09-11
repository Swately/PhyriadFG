#!/usr/bin/env python3
"""scene_truth — the pages index: every run the operator can walk, as DATA (runs.json), rendered to index.html.

Why: the first index (2026-09-09) was written by hand in a session script and had to be rewritten for every
new run. The rows now live in <pages>/runs.json and this file renders them; a new run is one JSON entry (or one
`add` call) and the numbers in it are copied from the scorer's own --json, never typed. Two tables: the scene
runs (scene_truth: silhouette terms) and the marker runs (motion_truth: per-class err_model), each linking its
stepper and its review page.

    python tools/scene_truth/scene_pages.py --pages F:\\Phyriad\\scene_pages build
    python tools/scene_truth/scene_pages.py --pages F:\\Phyriad\\scene_pages add-scene NAME --json <fg_k4.json> --k 4 \\
        --seed 7 --speed 1.0 --note "every tick, async path" [--arm fg_k4] [--obj 1]
    python tools/scene_truth/scene_pages.py --pages F:\\Phyriad\\scene_pages add-marker NAME --md <report.md> --note "..."

Made with my soul - Swately <3
"""
import argparse, html, json, os, re, sys
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')

CSS = ("body{margin:0;background:#f6f5f2;color:#1c1b19;font:14px/1.5 system-ui,Segoe UI,Roboto,sans-serif}"
       "@media(prefers-color-scheme:dark){body{background:#121210;color:#e8e4da}} main{max-width:1180px;margin:0 auto;padding:28px}"
       "h1{font-size:20px;margin:0 0 6px}h2{font-size:16px;margin:26px 0 6px}p{margin:6px 0 14px;opacity:.75}"
       "table{border-collapse:collapse;width:100%} th,td{padding:8px 10px;text-align:left;border-bottom:1px solid rgba(128,128,128,.25)}"
       "th{font-weight:600;font-size:12px;opacity:.7}td b{font-weight:600}a{color:#2f6f5e}"
       "@media(prefers-color-scheme:dark){a{color:#6fbfa5}} .k{opacity:.6} small{opacity:.65}")


def load(pages):
    p = os.path.join(pages, 'runs.json')
    return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {'scene': [], 'marker': []}


def save(pages, db):
    json.dump(db, open(os.path.join(pages, 'runs.json'), 'w', encoding='utf-8'), indent=1)


def scene_row_from_json(json_path, K, arm, obj):
    """The row's numbers from the scorer's --json: the arm's summary (all objects) and the named object's."""
    blob = json.load(open(json_path, encoding='utf-8'))
    key = next(k for k in blob if k.endswith('|k%d' % K))
    rows = blob[key]['rows']
    a = arm if arm in rows else next(x for x in rows if x.startswith('fg'))
    S = blob[key]['summary'][a]
    o = S['objects'].get(str(obj)) or S['objects'].get(obj) or {}
    allpos = float(np.nanmean([v['pos_err'] for v in S['objects'].values()]))
    return {'frames': S['n'], 'disp': o.get('disp_src_px'), 'pos_all': allpos, 'pos_obj': o.get('pos_err'),
            'halluc': o.get('halluc_px'), 'lead': o.get('lead_px'), 'sharp': S['sharp'], 'arm': a}


def presented_cell(pages, row):
    """The link to a run's presented-sequence page, with the count of what that page actually holds.

    The count is read back from the page's own frames.json and never carried in runs.json. The two counts
    in this table answer different questions -- how many frames the base-grid page walks, and how many
    ticks the tap stored -- and a remembered second number is exactly how they would drift apart.
    """
    fj = os.path.join(pages, row['name'], 'presented', 'frames.json')
    if not os.path.exists(fj):
        return '<span class="k">-</span>'
    try:
        blob = json.load(open(fj, encoding='utf-8'))
    except (ValueError, OSError):
        return '<span class="k">-</span>'
    n = len(blob.get('frames', []))
    pc = (blob.get('meta') or {}).get('pace') or {}
    tail = (' <small>%d pasos dobles</small>' % pc['double']) if pc.get('double') else ''
    return '<a href="%s/presented/index.html">totalidad (%d)</a>%s' % (html.escape(row['name']), n, tail)


def build(pages, db):
    f = lambda v, d: ('—' if v is None else ('%+.1f' % v if d == 'lead' else ('%.*f' % (d, v))))
    L = ['<!doctype html><meta charset="utf-8"><title>scene_truth — the FG runs</title><style>%s</style><main>' % CSS,
         '<h1>scene_truth — every live FG run, ready to walk</h1>',
         '<p>Each row is one live capture of the shipping default scored against exact truth at its own phase. '
         '<b>step</b> walks the BASE GRID frame by frame (real / generated, hold to advance); <b>review</b> '
         'shows the worst frames per term. The numbers are the arm\'s summary from the scorer\'s own JSON, and '
         'the <b>frames</b> column counts what those two pages carry.</p>',
         '<p><b>presentado</b> is a different sequence and a different count: every frame the tap STORED, in the '
         'order the FG presented it, nothing deduplicated and nothing real. The aligner behind <b>step</b> files '
         'one frame per base index, so a looped capture -- the same corpus shown twenty times -- collapses to one '
         'lap and most of what was on the screen never reaches that page. The presented page also carries the '
         'pacing block: which phase slots the generator dropped, and where the scene advances two base frames in '
         'one presented frame instead of one.</p>',
         '<h2>Scene runs (scene_truth: silhouette terms)</h2>',
         '<table><tr><th>run</th><th>seed</th><th>speed</th><th>k</th><th>frames</th><th>sphere px/pair</th>'
         '<th>pos px (all obj)</th><th>sphere pos</th><th>halluc px²</th><th>lead</th><th>sharp</th><th></th><th></th><th>presentado</th></tr>']
    for r in db['scene']:
        note = (' <small>%s</small>' % html.escape(r['note'])) if r.get('note') else ''
        L.append('<tr><td><b>%s</b>%s</td><td>%s</td><td>%s×</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>'
                 '<td><a href="%s/step/index.html">step</a></td><td><a href="%s/review/index.html">review</a></td><td>%s</td></tr>'
                 % (html.escape(r['name']), note, r.get('seed', '—'), r.get('speed', '—'), r['k'], r['frames'], f(r.get('disp'), 2),
                    f(r.get('pos_all'), 3), f(r.get('pos_obj'), 3), f(r.get('halluc'), 0), f(r.get('lead'), 'lead'), f(r.get('sharp'), 3),
                    html.escape(r['name']), html.escape(r['name']), presented_cell(pages, r)))
    L.append('</table>')
    if db.get('marker'):
        L += ['<h2>Marker runs (motion_truth: err_model per class, px; r = run-to-run reliability)</h2>',
              '<table><tr><th>run</th><th>fps</th><th>bg</th><th>sizes</th><th>arm</th><th>classes (err px · r)</th><th>real-plane floor</th><th></th><th></th></tr>']
        for r in db['marker']:
            note = (' <small>%s</small>' % html.escape(r['note'])) if r.get('note') else ''
            cls = ' · '.join('%s %s%s' % (c['class'], f(c.get('err'), 3), (' (r %.2f)' % c['r']) if c.get('r') is not None else '')
                             for c in r.get('classes', []))
            L.append('<tr><td><b>%s</b>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>'
                     '<td><a href="%s/step/index.html">step</a></td><td>%s</td></tr>'
                     % (html.escape(r['name']), note, r.get('fps', '—'), html.escape(str(r.get('bg', '—'))), html.escape(str(r.get('sizes', '—'))),
                        html.escape(str(r.get('arm', 'default'))), cls, f(r.get('floor'), 3), html.escape(r['name']),
                        ('<a href="%s/report.md">report</a>' % html.escape(r['name'])) if r.get('report') else ''))
        L.append('</table>')
    L.append('<p class="k">Records: <code>docs/evidence/B1_FIRST_FG_ROW.md</code>, <code>B1_SPEED_TEST.md</code>, '
             '<code>docs/planning/REGIME_TEST_MATRIX.md</code>. Made with my soul - Swately &lt;3</p></main>')
    open(os.path.join(pages, 'index.html'), 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    print('index.html: %d scene rows, %d marker rows' % (len(db['scene']), len(db.get('marker', []))))


def parse_marker_md(md_path):
    """The per-class table of a motion_report --md: class, err_model mean, r (None when UNRELIABLE-less or absent)."""
    out = []
    for ln in open(md_path, encoding='utf-8', errors='replace'):
        m = re.match(r'\|\s*(linear|accel|circular|crossing|hud|fast|reverse)\s*\|(.*)', ln)
        if not m:
            continue
        cells = [c.strip() for c in m.group(2).split('|')]
        try:
            err = float(cells[5])
        except (ValueError, IndexError):
            continue
        r = None
        for c in cells:
            mm = re.match(r'^(-?\d\.\d\d)\b', c)
            if mm and 'n=' in c:
                r = float(mm.group(1))
        out.append({'class': m.group(1), 'err': err, 'r': r})
    return out


def main():
    ap = argparse.ArgumentParser(description='scene_truth — the pages index as data')
    ap.add_argument('--pages', required=True)
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('build')
    a1 = sub.add_parser('add-scene'); a1.add_argument('name'); a1.add_argument('--json', required=True); a1.add_argument('--k', type=int, default=4)
    a1.add_argument('--seed', type=int); a1.add_argument('--speed', type=float, default=1.0); a1.add_argument('--note', default='')
    a1.add_argument('--arm', default=None); a1.add_argument('--obj', type=int, default=1)
    a2 = sub.add_parser('add-marker'); a2.add_argument('name'); a2.add_argument('--md', required=True); a2.add_argument('--note', default='')
    a2.add_argument('--fps'); a2.add_argument('--bg'); a2.add_argument('--sizes'); a2.add_argument('--arm', default='default'); a2.add_argument('--floor', type=float)
    a = ap.parse_args()
    db = load(a.pages)
    if a.cmd == 'add-scene':
        row = {'name': a.name, 'seed': a.seed, 'speed': a.speed, 'k': a.k, 'note': a.note}
        row.update(scene_row_from_json(a.json, a.k, a.arm or ('fg_k%d' % a.k), a.obj))
        db['scene'] = [r for r in db['scene'] if r['name'] != a.name] + [row]
        save(a.pages, db)
    elif a.cmd == 'add-marker':
        row = {'name': a.name, 'note': a.note, 'fps': a.fps, 'bg': a.bg, 'sizes': a.sizes, 'arm': a.arm, 'floor': a.floor,
               'classes': parse_marker_md(a.md), 'report': True}
        db.setdefault('marker', [])
        db['marker'] = [r for r in db['marker'] if r['name'] != a.name] + [row]
        save(a.pages, db)
    build(a.pages, db)


if __name__ == '__main__':
    main()
