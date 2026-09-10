#!/usr/bin/env python3
"""marker_step.py — the frame stepper for marker runs (MOTION_TRUTH's own stepper).

WHY THIS EXISTS. marker_extract.py answers "I want data, not images" with a table
(detections.csv). But the operator's OTHER ask, the one scene_truth/scene_step.py already
answers for the exact-scene corpus, is the same question asked of the eyes: walk the marker
chain one frame at a time — real, generated, real, ... — see the marker where the FG put it, and
be able to say "THAT one" about the frame that carries the miss. This mirrors scene_step.py
page-for-page (the HTML/CSS/JS is the same stepper, copied and adapted): only the badges, the
panel text and what T/D mean are different, because the marker chain is a different shape —
prev/live/next triples instead of one interpolated frame per real pair, and per-marker detection
status instead of a scorer's terms.

THE SEQUENCE. Every captured triple (marker_extract.load_manifest's rows) gives one GENERATED
frame — its live plane, at fractional index k_prev + t*span — and two REAL planes, prev at
k_prev and next at k_next. The REAL planes are the PHOTOGRAPHED path: the same real k can be a
prev in one triple and a next in another (or appear in neither and both), so the real list here
is built from what the triples actually captured, deduplicated by k, not from the zoo's own
frame files. A triple whose k_next < k_prev is the loop seam — the player wrapped, and marker_extract
still computes a (wrapped) expectation for it, but that expectation crosses the join and is not a
measurement of anything; it is shown, badged, and carries no numbers, the same discipline
scene_report.py applies to a CUT.

WHAT IS DRAWN, AND WHAT IS NOT BAKED IN. Per marker, a small circle at the model-expected
position (exp_model_x/y — always this column, on every plane, per the operator's spec) coloured
by class, and — unless the marker is ABSENT — a cross at the detected position coloured by
status: found (n_peaks == 1, green) at (obs_x, obs_y); ghost (n_peaks >= 2, magenta) at the same;
degraded (n_peaks == 0, ncc_max >= 0.3, amber) at (near_x, near_y), the strongest sub-floor match;
absent (n_peaks == 0, ncc_max < 0.3, red) gets the circle only. That overlay is baked into a PNG
(the `ov` file) so it can be looked at without a browser doing any work; the `raw` file is the
same plane undrawn, and T swaps between them. D draws the expected-to-detected line live, in the
browser, from the same per-marker data the overlay was baked from — so it works whether T is
showing the overlay or the raw plane.

    python tools/motion_truth/marker_step.py --zoo ZOODIR --dump DUMPDIR --out DIR [--jobs N]

numpy + stdlib only. Made with my soul - Swately <3
"""
import argparse, csv, json, os, sys, html
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'scene_truth'))
import scene_report as SR                 # noqa: E402  — its lossless PNG writer, stdlib zlib
from marker_extract import load_manifest   # noqa: E402  — the dump's own triples format

CLASS_COLORS = {
    'linear':   (60, 200, 255),
    'accel':    (255, 150, 40),
    'circular': (180, 110, 255),
    'crossing': (255, 225, 40),
    'hud':      (245, 245, 245),
    'fast':     (255, 90, 160),
    'reverse':  (120, 255, 200),   # the sign-flip class (marker_zoo 'sine' trajectory, 2026-09-10)
}
STATUS_COLORS = {
    'found':    (60, 220, 100),
    'ghost':    (230, 50, 220),
    'degraded': (255, 185, 35),
    'absent':   (230, 55, 55),
}
_NUM_FIELDS = ('t', 'exp_model_x', 'exp_model_y', 'exp_true_x', 'exp_true_y', 'obs_x', 'obs_y',
               'ncc', 'err_model_px', 'err_true_px', 'ncc_max', 'near_x', 'near_y', 'near_dist')
_INT_FIELDS = ('k_prev', 'k_next', 'span', 'marker', 'size', 'n_peaks')


# ── loading ──────────────────────────────────────────────────────────────────────────────────
def load_plane(path, w, h):
    a = np.fromfile(path, dtype=np.uint8)
    if a.size != w * h * 4:
        raise ValueError('%s: %d B, expected %d' % (path, a.size, w * h * 4))
    return a.reshape(h, w, 4)[:, :, :3].copy()


def load_detections(path):
    rows = list(csv.DictReader(open(path, encoding='utf-8')))
    for r in rows:
        for k in _NUM_FIELDS:
            v = r.get(k)
            r[k] = float(v) if v not in (None, '', 'nan') else float('nan')
        for k in _INT_FIELDS:
            v = r.get(k)
            r[k] = int(v) if v not in (None, '') else None
    return rows


# ── per-marker status, from the same columns marker_extract wrote ─────────────────────────────
def status_of(row):
    """(status, (x, y) or None) — the point a cross is drawn at, or None for an absent marker."""
    if row['n_peaks'] == 1:
        return 'found', (row['obs_x'], row['obs_y'])
    if row['n_peaks'] >= 2:
        return 'ghost', (row['obs_x'], row['obs_y'])
    if row['ncc_max'] >= 0.3:
        return 'degraded', (row['near_x'], row['near_y'])
    return 'absent', None


def build_markers(rows):
    """detections.csv rows (one plane of one triple, one row per marker) -> (draw list for the
    numpy overlay, json list for the browser's live D-line — the same classification, twice)."""
    draw, js = [], []
    for r in rows:
        status, obs = status_of(r)
        ex = (r['exp_model_x'], r['exp_model_y'])
        d = {'ex': ex, 'excolor': CLASS_COLORS.get(r['class'], (200, 200, 200)),
             'obs': obs, 'obscolor': STATUS_COLORS[status], 'status': status,
             'cls': r['class'], 'err': r['err_model_px']}
        draw.append(d)
        js.append({'x': ex[0], 'y': ex[1], 'ox': obs[0] if obs else None, 'oy': obs[1] if obs else None,
                    's': status})
    return draw, js


def frame_stats(markers):
    """found/ghost/degraded/absent counts, mean err_model over found, and per-class mean err with n."""
    found = [m for m in markers if m['status'] == 'found']
    ghost = [m for m in markers if m['status'] == 'ghost']
    degraded = [m for m in markers if m['status'] == 'degraded']
    absent = [m for m in markers if m['status'] == 'absent']
    errs = [m['err'] for m in found if np.isfinite(m['err'])]
    by_cls = {}
    for m in found:
        if np.isfinite(m['err']):
            by_cls.setdefault(m['cls'], []).append(m['err'])
    per_class = {c: {'mean': float(np.mean(v)), 'n': len(v)} for c, v in by_cls.items()}
    return {'found': len(found), 'ghost': len(ghost), 'degraded': len(degraded), 'absent': len(absent),
            'err_mean': float(np.mean(errs)) if errs else None, 'per_class': per_class}


# ── drawing (numpy, before PNG) ─────────────────────────────────────────────────────────────
def draw_disc(img, cx, cy, r, color):
    """A thin ring at (cx, cy) — the model-expected position, coloured by class."""
    h, w = img.shape[:2]
    x0, x1 = max(0, int(cx - r - 1)), min(w, int(cx + r + 2))
    y0, y1 = max(0, int(cy - r - 1)), min(h, int(cy + r + 2))
    if x1 <= x0 or y1 <= y0:
        return
    yy, xx = np.mgrid[y0:y1, x0:x1]
    d = np.hypot(xx - cx, yy - cy)
    ring = (d >= r - 1.0) & (d <= r + 0.6)
    img[y0:y1, x0:x1][ring] = color


def draw_cross(img, cx, cy, r, color):
    """A small '+' at (cx, cy) — the detected (or nearest sub-floor) position, by status."""
    h, w = img.shape[:2]
    icx, icy = int(round(cx)), int(round(cy))
    for d in range(-r, r + 1):
        for x, y in ((icx + d, icy), (icx, icy + d)):
            if 0 <= x < w and 0 <= y < h:
                img[y, x] = color


def _render_job(job):
    """One frame's PNG(s): the raw plane always; the overlay only when its path differs from the
    raw one (a seam frame carries no markers and reuses the raw file for both roles)."""
    src, raw_path, ov_path, markers, w, h = job
    img = load_plane(src, w, h)
    SR.png(raw_path, img)
    if ov_path != raw_path:
        ov = img.copy()
        for m in markers:
            ex, ey = m['ex']
            if np.isfinite(ex) and np.isfinite(ey):
                draw_disc(ov, ex, ey, 4, m['excolor'])
            if m['obs'] is not None:
                ox, oy = m['obs']
                if np.isfinite(ox) and np.isfinite(oy):
                    draw_cross(ov, ox, oy, 4, m['obscolor'])
        SR.png(ov_path, ov)


# ── the sequence: reals (photographed, deduplicated by k) interleaved with generateds ─────────
def build(zoo, dump, detections, out, jobs, start, count):
    traj = json.load(open(os.path.join(zoo, 'trajectories.json'), encoding='utf-8'))
    W, H = traj['width'], traj['height']
    size, recs = load_manifest(os.path.join(dump, 'manifest.txt'))
    if size != (W, H):
        raise SystemExit('%s: dump is %dx%d, zoo %s is %dx%d' % (dump, size[0], size[1], zoo, W, H))
    det_path = detections or os.path.join(dump, 'detections.csv')
    if not os.path.exists(det_path):
        raise SystemExit('%s: not found — run marker_extract.py first (--dump %s --zoo %s)' % (det_path, dump, zoo))
    rows = load_detections(det_path)
    by_triple = {}
    for r in rows:
        by_triple.setdefault(r['triple'], []).append(r)

    real_paths, real_rows, order = {}, {}, []
    for rec in recs:
        tid = rec['id']
        trows = by_triple.get(tid)
        if not trows:
            raise SystemExit('%s: triple %s has no rows in %s' % (dump, tid, det_path))
        k_prev, k_next, t = trows[0]['k_prev'], trows[0]['k_next'], trows[0]['t']
        seam = k_next < k_prev
        for plane, k in (('prev', k_prev), ('next', k_next)):
            if k not in real_paths:
                real_paths[k] = os.path.join(dump, rec[plane])
                real_rows[k] = [r for r in trows if r['plane'] == plane]
        span = 1.0 if seam else float(k_next - k_prev)
        order.append({'kind': 'seam' if seam else 'gen', 'tid': tid, 't': t, 'frac': k_prev + t * span,
                       'k_prev': k_prev, 'k_next': k_next, 'live_path': os.path.join(dump, rec['live'])})
    for k, p in real_paths.items():
        order.append({'kind': 'real', 'k': k, 'frac': float(k), 'path': p})
    order.sort(key=lambda e: (e['frac'], 0 if e['kind'] == 'real' else 1))

    lo = max(0, start)
    hi = len(order) if not count else min(len(order), lo + count)
    order = order[lo:hi]

    os.makedirs(os.path.join(out, 'seq'), exist_ok=True)
    frames, jobs_list = [], []
    for e in order:
        if e['kind'] == 'real':
            k = e['k']
            draw, js = build_markers(real_rows[k])
            raw_rel, ov_rel = 'seq/real_%06d_raw.png' % k, 'seq/real_%06d_ov.png' % k
            jobs_list.append((e['path'], os.path.join(out, raw_rel), os.path.join(out, ov_rel), draw, W, H))
            frames.append({'kind': 'real', 'k': k, 'raw': raw_rel, 'ov': ov_rel, 'mk': js,
                            'st': frame_stats(draw)})
        elif e['kind'] == 'gen':
            tid = e['tid']
            draw, js = build_markers([r for r in by_triple[tid] if r['plane'] == 'live'])
            raw_rel, ov_rel = 'seq/%s_raw.png' % tid, 'seq/%s_ov.png' % tid
            jobs_list.append((e['live_path'], os.path.join(out, raw_rel), os.path.join(out, ov_rel), draw, W, H))
            frames.append({'kind': 'gen', 'tid': tid, 't': e['t'], 'frac': e['frac'], 'k_prev': e['k_prev'],
                            'k_next': e['k_next'], 'raw': raw_rel, 'ov': ov_rel, 'mk': js,
                            'st': frame_stats(draw)})
        else:  # seam — excluded: no marker data used, raw doubles as ov
            tid = e['tid']
            raw_rel = 'seq/%s_raw.png' % tid
            jobs_list.append((e['live_path'], os.path.join(out, raw_rel), os.path.join(out, raw_rel), [], W, H))
            frames.append({'kind': 'seam', 'tid': tid, 't': e['t'], 'frac': e['frac'], 'k_prev': e['k_prev'],
                            'k_next': e['k_next'], 'raw': raw_rel, 'ov': raw_rel, 'mk': [], 'st': None})
    for i, f in enumerate(frames):
        f['i'] = i

    if jobs <= 1 or len(jobs_list) < 2:
        for j in jobs_list:
            _render_job(j)
    else:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=min(jobs, len(jobs_list))) as pool:
            list(pool.map(_render_job, jobs_list, chunksize=2))

    n_real = sum(1 for f in frames if f['kind'] == 'real')
    n_gen = sum(1 for f in frames if f['kind'] == 'gen')
    n_seam = sum(1 for f in frames if f['kind'] == 'seam')
    write_html(out, frames, W, H, os.path.basename(os.path.normpath(zoo)), os.path.basename(os.path.normpath(dump)))
    print('%d frames (%d real, %d generated, %d seam) -> %s'
          % (len(frames), n_real, n_gen, n_seam, os.path.join(out, 'index.html')))


# ── the page — mirrors scene_truth/scene_step.py; only the badges and the panel text differ ──
def write_html(out, frames, W, H, zoo_name, dump_name):
    css = """
:root{--bg:#f6f5f2;--ink:#1c1b19;--mut:#6b675f;--line:#dcd8d0;--real:#2f8f5e;--gen:#c98a1a;--seam:#8a3fd1;--red:#b03a2e}
@media(prefers-color-scheme:dark){:root{--bg:#121210;--ink:#e8e4da;--mut:#9b968a;--line:#2d2b25;--real:#5fd39a;--gen:#f0b545;--seam:#c990ff;--red:#e0705f}}
html,body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;height:100%}
#wrap{display:grid;grid-template-rows:auto 1fr auto auto;height:100vh}
header{display:flex;gap:18px;align-items:baseline;padding:10px 18px;border-bottom:1px solid var(--line)}
header h1{margin:0;font-size:16px;font-weight:600} header .sub{color:var(--mut);font-size:12px}
#stage{position:relative;display:flex;align-items:center;justify-content:center;background:#000;overflow:hidden}
canvas{max-width:100%;max-height:100%;image-rendering:pixelated}
#badge{position:absolute;left:18px;top:14px;font-size:22px;font-weight:700;letter-spacing:.03em;padding:6px 14px;border-radius:8px;color:#000;max-width:70%}
#badge.real{background:var(--real)} #badge.gen{background:var(--gen)} #badge.seam{background:var(--seam);font-size:14px;color:#fff}
#idx{position:absolute;right:18px;top:14px;font-size:20px;font-weight:600;color:#fff;text-shadow:0 1px 3px #000}
#mode{position:absolute;left:18px;bottom:12px;color:#fff;font-size:13px;text-shadow:0 1px 3px #000}
#panel{display:grid;grid-template-columns:1fr auto;gap:14px;padding:10px 18px;border-top:1px solid var(--line);font-size:13px;min-height:52px}
#panel b{font-weight:600} .k{color:var(--mut)} .bad{color:var(--red);font-weight:600}
#tl{position:relative;height:38px;margin:0 18px 10px;border:1px solid var(--line);border-radius:6px;cursor:pointer;overflow:hidden}
#tl canvas{position:absolute;inset:0;width:100%;height:100%;image-rendering:auto}
#help{padding:0 18px 10px;color:var(--mut);font-size:12px}
#prog{position:absolute;left:0;right:0;bottom:0;height:3px;background:var(--gen);transform-origin:left;transform:scaleX(0)}
"""
    data = json.dumps(frames)
    n_real = sum(1 for f in frames if f['kind'] == 'real')
    n_gen = sum(1 for f in frames if f['kind'] == 'gen')
    n_seam = sum(1 for f in frames if f['kind'] == 'seam')
    parts = ['<!doctype html><meta charset="utf-8"><title>marker_step · %s / %s</title><style>%s</style>'
             % (html.escape(zoo_name), html.escape(dump_name), css),
             '<div id="wrap"><header><div><h1>%s / %s</h1>'
             '<span class="sub">%d frames · %d real · %d generated · %d seam · %dx%d</span></div></header>'
             % (html.escape(zoo_name), html.escape(dump_name), len(frames), n_real, n_gen, n_seam, W, H),
             '<div id="stage"><canvas id="cv" width="%d" height="%d"></canvas><div id="badge"></div><div id="idx"></div><div id="mode"></div><div id="prog"></div></div>' % (W, H),
             '<div id="panel"><div id="info"></div><div id="rate" class="k"></div></div>',
             '<div id="tl"><canvas id="tlc"></canvas></div>',
             '<div id="help">← → step · <b>hold</b> to auto-advance · [ ] slower/faster · space play · L loop · T raw/overlay · D expected→detected line · Home/End · click the timeline</div></div>',
             '<script>const F=%s;const W=%d,H=%d;' % (data, W, H),
             r"""
const cv=document.getElementById('cv'),cx=cv.getContext('2d'),badge=document.getElementById('badge'),idx=document.getElementById('idx'),
 mode=document.getElementById('mode'),info=document.getElementById('info'),rate=document.getElementById('rate'),prog=document.getElementById('prog'),
 tl=document.getElementById('tl'),tlc=document.getElementById('tlc'),tx=tlc.getContext('2d');
const STATUS_COLOR={found:'#3adc64',ghost:'#e632dc',degraded:'#ffb923',absent:'#e63737'};
const IMG={};let loaded=0,total=0;
function load(src){if(IMG[src])return IMG[src];const im=new Image();IMG[src]=im;total++;im.onload=()=>{loaded++;prog.style.transform='scaleX('+(loaded/total)+')';if(loaded===total)prog.style.display='none'};im.src=src;return im}
F.forEach(f=>{load(f.ov);if(f.raw!==f.ov)load(f.raw)});
let cur=0,showRaw=false,showLine=false,fps=8,timer=null,playing=false,loop=true,hold=null;
function fmt(v,d){return v==null?'—':Number(v).toFixed(d)}
function draw(){const f=F[cur];const im=load(showRaw?f.raw:f.ov);
 if(!im.complete||!im.naturalWidth){requestAnimationFrame(draw);return}
 cx.drawImage(im,0,0);
 if(showLine)for(const m of f.mk){if(m.ox==null)continue;cx.strokeStyle=STATUS_COLOR[m.s]||'#fff';cx.lineWidth=1.5;cx.beginPath();cx.moveTo(m.x,m.y);cx.lineTo(m.ox,m.oy);cx.stroke()}
 badge.textContent=f.kind==='real'?'REAL':(f.kind==='seam'?'GENERATED ACROSS THE LOOP SEAM — excluded':'GENERATED');
 badge.className=f.kind;
 idx.textContent='#'+f.i+(f.kind==='real'?'  k '+f.k:'  t '+fmt(f.t,3)+'  idx '+fmt(f.frac,3));
 mode.textContent=(showRaw?'RAW':'OVERLAY')+(showLine?' + lines':'')+(playing?'  ▶':'');
 let h='';
 if(f.kind==='real')h='<b>real frame</b> <span class="k">the photographed plane at k '+f.k+' — every triple that captured it agrees</span>';
 else if(f.kind==='seam')h='<b>GENERATED ACROSS THE LOOP SEAM</b> <span class="bad">triple '+f.tid+' — k_next ('+f.k_next+') < k_prev ('+f.k_prev+'): the player wrapped, no interpolation truth exists</span>';
 else h='<b>generated</b> triple <b>'+f.tid+'</b> real <b>'+f.k_prev+'</b> → <b>'+f.k_next+'</b>, t '+fmt(f.t,3)+' <span class="k">(fractional index '+fmt(f.frac,3)+')</span>';
 if(f.st){const s=f.st;h+='<br>found <b>'+s.found+'</b> · ghost <b>'+s.ghost+'</b> · degraded <b>'+s.degraded+'</b> · absent <b>'+s.absent+'</b> · mean err <b>'+fmt(s.err_mean,3)+'</b> px';
  const cls=Object.keys(s.per_class);if(cls.length)h+='<br>'+cls.map(c=>c+' '+fmt(s.per_class[c].mean,3)+' px (n='+s.per_class[c].n+')').join(' · ')}
 info.innerHTML=h;rate.textContent=fps+' fps auto · '+(loop?'loop':'stop at end');drawTL()}
function drawTL(){const w=tl.clientWidth,h=tl.clientHeight;if(tlc.width!==w||tlc.height!==h){tlc.width=w;tlc.height=h}
 tx.clearRect(0,0,w,h);const n=F.length;const col={real:'--real',gen:'--gen',seam:'--seam'};
 for(let i=0;i<n;i++){const x=(i+0.5)/n*w;const f=F[i];tx.fillStyle=getComputedStyle(document.documentElement).getPropertyValue(col[f.kind]);
  const t=f.kind==='real'?4:h*0.45;tx.fillRect(x-0.5,h-t,Math.max(1,w/n-0.5),t)}
 const x=(cur+0.5)/n*w;tx.fillStyle='#fff';tx.fillRect(x-1,0,2,h)}
function go(i){if(i<0)i=loop?F.length-1:0;if(i>=F.length){if(!loop){stop();i=F.length-1}else i=0}cur=i;draw()}
function step(d){go(cur+d)}
function startAuto(d){stopAuto();hold=d;timer=setInterval(()=>step(d),1000/fps)}
function stopAuto(){if(timer){clearInterval(timer);timer=null}hold=null}
function play(){playing=true;startAuto(1)}function stop(){playing=false;stopAuto();draw()}
document.addEventListener('keydown',e=>{
 if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();const d=e.key==='ArrowRight'?1:-1;if(e.repeat)return;step(d);if(!playing){hold=d;setTimeout(()=>{if(hold===d&&!timer)startAuto(d)},260)}return}
 if(e.key===' '){e.preventDefault();playing?stop():play();return}
 if(e.key==='t'||e.key==='T'){showRaw=!showRaw;draw()}
 if(e.key==='d'||e.key==='D'){showLine=!showLine;draw()}
 if(e.key==='l'||e.key==='L'){loop=!loop;draw()}
 if(e.key===']'){fps=Math.min(60,Math.round(fps*1.5));draw();if(timer)startAuto(hold||1)}
 if(e.key==='['){fps=Math.max(1,Math.round(fps/1.5));draw();if(timer)startAuto(hold||1)}
 if(e.key==='Home'){go(0)}if(e.key==='End'){go(F.length-1)}});
document.addEventListener('keyup',e=>{if((e.key==='ArrowRight'||e.key==='ArrowLeft')&&!playing){stopAuto()}});
tl.addEventListener('click',e=>{const r=tl.getBoundingClientRect();go(Math.floor((e.clientX-r.left)/r.width*F.length))});
window.addEventListener('resize',drawTL);go(0);
</script>"""]
    open(os.path.join(out, 'index.html'), 'w', encoding='utf-8', newline='\n').write('\n'.join(parts) + '\n')


def main():
    ap = argparse.ArgumentParser(description='marker_step — frame stepper for marker runs (mirrors scene_truth/scene_step.py)')
    ap.add_argument('--zoo', required=True, help='the marker_zoo directory (trajectories.json)')
    ap.add_argument('--dump', required=True, help='a --qdump+ directory captured from the zoo (manifest.txt)')
    ap.add_argument('--detections', default=None, help='detections.csv from marker_extract.py (default: <dump>/detections.csv)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 2),
                    help='worker processes writing PNGs (default: cores - 2); 1 = serial')
    ap.add_argument('--start', type=int, default=0, help='position in the ORDERED sequence to start at')
    ap.add_argument('--count', type=int, default=0, help='0 = to the end of the ordered sequence')
    a = ap.parse_args()
    build(a.zoo, a.dump, a.detections, a.out, a.jobs, a.start, a.count)


if __name__ == '__main__':
    main()
