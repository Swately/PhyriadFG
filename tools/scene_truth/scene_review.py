#!/usr/bin/env python3
"""scene_truth — the operator's review page: the worst frames, term by term, with a label field.

WHY A PAGE AND NOT A NUMBER
---------------------------
The scorer names WHICH term failed; only a person can name the FORM of the failure — ghost, crescent,
smear, seam, hole — and that name is the one thing that turns "a hallucination happened" into a rule
for the kernel. So the page shows, for every arm, the K worst frames by the chosen term, each as
truth / candidate / |difference| / the silhouette overlay, and puts a label field beside each. Labels
persist in the browser (localStorage) and export as JSON, keyed by (corpus, k, arm, frame, object),
so they accumulate into a set the operator built himself.

Static HTML, no server, no CDN: it must open from a local folder on the rig and nowhere else.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, struct, zlib, html
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scene_report as SR   # noqa: E402

TERMS = ('pos_err', 'shape_err', 'halluc_px', 'missing_px')


def png(path, rgb8):
    H, W = rgb8.shape[:2]
    raw = b''.join(b'\x00' + rgb8[y].tobytes() for y in range(H))
    def ch(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
                           + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))


def u8(x):
    return np.clip(np.rint(x * 255), 0, 255).astype(np.uint8)


def overlay(cand, truth, bg, ids, cls):
    """candidate in grey; truth silhouette green, candidate-only red, missing blue, class-0 magenta."""
    tl, cl = SR.object_like(truth, bg), SR.object_like(cand, bg)
    g = SR.lum(cand)[..., None] * np.ones(3)
    out = 0.55 * g + 0.15
    out[tl & cl] = out[tl & cl] * 0.5 + np.array([0.1, 0.8, 0.2]) * 0.5
    out[cl & ~tl] = np.array([0.95, 0.15, 0.15])
    out[tl & ~cl] = np.array([0.2, 0.35, 0.95])
    out[cls == 0] = out[cls == 0] * 0.4 + np.array([0.9, 0.2, 0.9]) * 0.6
    return out


def build(json_path, out_dir, term, K, crop):
    data = json.load(open(json_path, encoding='utf-8'))
    os.makedirs(os.path.join(out_dir, 'img'), exist_ok=True)
    cards = []
    for key, blob in data.items():
        d, kk = key.split('|k')
        k = int(kk)
        t, sc = SR.load_corpus(d)
        W, H = t['width'], t['height']
        bg = SR.Z.Scene([], W, H, t['fov_deg'], t['seed']).render(0.0, t['ss'])[0]
        for arm, rows in blob['rows'].items():
            if arm == 'truth' or not rows:
                continue
            flat = []
            for r in rows:
                for ok, o in r['objects'].items():
                    if o.get(term) == o.get(term):
                        flat.append((float(o[term]), r, int(ok), o))
            flat.sort(key=lambda x: -x[0])
            for val, r, ok, o in flat[:K]:
                mid = r['mid']
                truth = SR.load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % mid), W, H)
                cand = SR.load_rgb(os.path.join(d, 'arms', arm, 'f_%06d.rgba' % mid), W, H)
                ids = SR.load_id(d, mid, W, H)
                N, N1 = mid - int(round(r['phase'] * k)), mid - int(round(r['phase'] * k)) + k
                cls = sc.visibility(t['t'][mid], t['t'][N], t['t'][N1])
                ys, xs = np.nonzero(ids == ok)
                if len(xs) == 0:
                    continue
                r0 = max(int(ys.mean()) - crop // 2, 0); c0 = max(int(xs.mean()) - crop // 2, 0)
                r1, c1 = min(r0 + crop, H), min(c0 + crop, W)
                sl = (slice(r0, r1), slice(c0, c1))
                base = '%s_k%d_%s_f%06d_o%d' % (os.path.basename(os.path.normpath(d)), k, arm, mid, ok)
                png(os.path.join(out_dir, 'img', base + '_t.png'), u8(truth[sl]))
                png(os.path.join(out_dir, 'img', base + '_c.png'), u8(cand[sl]))
                diff = np.abs(cand - truth)[sl]; diff = diff / max(diff.max(), 1e-6)
                png(os.path.join(out_dir, 'img', base + '_d.png'), u8(diff))
                png(os.path.join(out_dir, 'img', base + '_o.png'), u8(overlay(cand, truth, bg, ids, cls)[sl]))
                cards.append({'id': base, 'corpus': os.path.basename(os.path.normpath(d)), 'k': k, 'arm': arm,
                              'frame': mid, 'phase': r['phase'], 'object': ok, 'term': term, 'value': val,
                              'terms': {x: o.get(x) for x in TERMS + ('lead_px', 'disp_src_px')},
                              'sharp': r['sharp'], 'disocc_px': r['disocc_px']})
    write_html(out_dir, cards, term, K, crop)
    print('%d cards -> %s' % (len(cards), os.path.join(out_dir, 'index.html')))


def write_html(out_dir, cards, term, K, crop):
    esc = lambda s: html.escape(str(s))
    css = """
:root{--bg:#f6f5f2;--ink:#1c1b19;--mut:#6b675f;--line:#dcd8d0;--acc:#2f6f5e;--red:#b03a2e}
@media(prefers-color-scheme:dark){:root{--bg:#14140f;--ink:#e8e4da;--mut:#9b968a;--line:#2d2b25;--acc:#6fbfa5;--red:#e0705f}}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{padding:20px 28px;border-bottom:1px solid var(--line)} h1{margin:0 0 4px;font-size:20px;font-weight:600}
.sub{color:var(--mut)} .bar{display:flex;gap:14px;flex-wrap:wrap;align-items:center;padding:12px 28px;border-bottom:1px solid var(--line)}
select,button,input{font:inherit;padding:4px 8px;border:1px solid var(--line);background:transparent;color:var(--ink);border-radius:6px}
button{cursor:pointer} .card{display:grid;grid-template-columns:repeat(4,%dpx) 1fr;gap:12px;padding:16px 28px;border-bottom:1px solid var(--line);align-items:start}
.card img{width:%dpx;height:%dpx;image-rendering:pixelated;border:1px solid var(--line);display:block}
.card .cap{font-size:11px;color:var(--mut);margin-top:3px} .meta{font-size:13px} .meta b{font-weight:600}
.meta .val{font-size:18px;font-weight:600;color:var(--red)} .lab{margin-top:8px;display:flex;gap:8px;flex-wrap:wrap}
.lab label{border:1px solid var(--line);border-radius:14px;padding:2px 10px;cursor:pointer}
.lab input:checked+span{color:var(--acc);font-weight:600} .lab input{display:none}
textarea{width:100%%;max-width:420px;margin-top:6px;font:inherit;background:transparent;color:var(--ink);border:1px solid var(--line);border-radius:6px;padding:6px}
.legend{font-size:12px;color:var(--mut)} .legend i{display:inline-block;width:10px;height:10px;border-radius:2px;margin:0 4px 0 10px;vertical-align:middle}
""" % (crop, crop, crop)
    parts = ['<title>scene_truth review</title><style>%s</style>' % css,
             '<header><h1>scene_truth — the worst %d per arm, by <code>%s</code></h1>' % (K, esc(term)),
             '<div class="sub">%d cards. Name the FORM of each failure; labels stay in this browser and export as JSON.</div>' % len(cards),
             '<div class="legend">overlay: <i style="background:#2bd06a"></i>truth ∧ candidate <i style="background:#e8332b"></i>candidate only (hallucinated) '
             '<i style="background:#3d5fe8"></i>truth only (missing) <i style="background:#e030e0"></i>class 0 — no real frame saw it</div></header>',
             '<div class="bar"><label>arm <select id="farm"><option value="">all</option>%s</select></label>'
             % ''.join('<option>%s</option>' % esc(a) for a in sorted(set(c['arm'] for c in cards))),
             '<label>k <select id="fk"><option value="">all</option>%s</select></label>'
             % ''.join('<option>%s</option>' % c for c in sorted(set(c['k'] for c in cards))),
             '<button id="exp">export labels (JSON)</button><span id="cnt" class="sub"></span></div>']
    forms = ['ghost', 'crescent', 'smear', 'seam', 'hole', 'wrong-place', 'fine', 'other']
    for c in cards:
        b = c['id']
        parts.append('<div class="card" data-arm="%s" data-k="%s" data-id="%s">' % (esc(c['arm']), c['k'], esc(b)))
        for sfx, cap in (('_t', 'truth'), ('_c', c['arm']), ('_d', '|difference|'), ('_o', 'overlay')):
            parts.append('<div><img src="img/%s%s.png"><div class="cap">%s</div></div>' % (esc(b), sfx, esc(cap)))
        tm = c['terms']
        parts.append('<div class="meta"><div class="val">%s = %.3f</div>' % (esc(term), c['value']))
        parts.append('<div><b>%s</b> · k=%d · frame %d · φ=%.2f · object %d</div>' % (esc(c['corpus']), c['k'], c['frame'], c['phase'], c['object']))
        parts.append('<div class="sub">pos %.3f · shape %.3f · halluc %s px² (lead %+.1f) · missing %s px² · sharp %.3f · disp/pair %.2f px · class-0 %d px</div>'
                     % (tm.get('pos_err') or 0, tm.get('shape_err') or 0, tm.get('halluc_px'), tm.get('lead_px') or 0,
                        tm.get('missing_px'), c['sharp'], tm.get('disp_src_px') or 0, c['disocc_px']))
        parts.append('<div class="lab">' + ''.join('<label><input type="radio" name="l_%s" value="%s"><span>%s</span></label>' % (esc(b), f, f) for f in forms) + '</div>')
        parts.append('<textarea rows="2" placeholder="what you see, in your words" data-note="%s"></textarea></div></div>' % esc(b))
    parts.append("""<script>
const KEY='scene_truth_labels';let L={};try{L=JSON.parse(localStorage.getItem(KEY)||'{}')}catch(e){}
function save(){try{localStorage.setItem(KEY,JSON.stringify(L))}catch(e){}cnt()}
function cnt(){const n=Object.keys(L).length;document.getElementById('cnt').textContent=n?n+' labelled':''}
document.querySelectorAll('.card').forEach(c=>{const id=c.dataset.id;const s=L[id]||{};
 c.querySelectorAll('input[type=radio]').forEach(r=>{if(s.form===r.value)r.checked=true;r.onchange=()=>{L[id]=Object.assign(L[id]||{},{form:r.value});save()}});
 const t=c.querySelector('textarea');t.value=s.note||'';t.oninput=()=>{L[id]=Object.assign(L[id]||{},{note:t.value});save()}});
function filt(){const a=document.getElementById('farm').value,k=document.getElementById('fk').value;
 document.querySelectorAll('.card').forEach(c=>{c.style.display=((!a||c.dataset.arm===a)&&(!k||c.dataset.k===k))?'':'none'})}
document.getElementById('farm').onchange=filt;document.getElementById('fk').onchange=filt;
document.getElementById('exp').onclick=()=>{const b=new Blob([JSON.stringify(L,null,1)],{type:'application/json'});
 const a=document.createElement('a');a.href=URL.createObjectURL(b);a.download='scene_truth_labels.json';a.click()};
cnt();</script>""")
    open(os.path.join(out_dir, 'index.html'), 'w', encoding='utf-8', newline='\n').write('\n'.join(parts) + '\n')


def main():
    ap = argparse.ArgumentParser(description='scene_truth — the operator review page')
    ap.add_argument('--json', required=True, help='the --json output of scene_report.py')
    ap.add_argument('--out', required=True)
    ap.add_argument('--term', default='halluc_px', choices=TERMS)
    ap.add_argument('--worst', type=int, default=8, help='cards per arm')
    ap.add_argument('--crop', type=int, default=160, help='crop size around the object, px')
    a = ap.parse_args()
    build(a.json, a.out, a.term, a.worst, a.crop)


if __name__ == '__main__':
    main()
