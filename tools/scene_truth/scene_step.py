#!/usr/bin/env python3
"""scene_truth — the frame stepper: the sequence AS THE FG WOULD PRESENT IT, one frame at a time.

The operator's ask, verbatim in intent: walk the whole sequence at full frame — real, generated,
generated, generated, real, ... — with each frame marked, see the motion, and be able to say
"THAT one" about the frame that carries the hallucination. So:

    ←  →         one frame; HOLD the key and it advances on its own at an adjustable rate
    [  ]         slower / faster auto-advance
    space        play / pause     L  loop     Home / End
    T            show the TRUTH in place of a generated frame (nothing changes on a real one)
    D            |candidate − truth| amplified, computed in the browser, no extra files
    click the timeline to jump

Every frame is written full-size as PNG (lossless) from the raw RGBA8 — the same bytes the scorer
read. Real frames are the corpus's own source frames at multiplier k; generated ones come from an
arm: a materialised `arms/<name>/` directory (the FG's own output, later) or one of the synthetic
arms computed in memory. With `--scores` (the scorer's --json) every generated frame carries its
own terms and verdict in the panel, so the number and the picture are looked at together.

Made with my soul - Swately <3
"""
import argparse, json, os, sys, html, re
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scene_report as SR   # noqa: E402


def measured_rates(fg_log):
    """From a raw FG log: the present rate and the capture rate it reported, second by second."""
    if not fg_log or not os.path.exists(fg_log):
        return None
    pres, cap, done = [], [], None
    for line in open(fg_log, encoding='utf-8', errors='replace'):
        m = re.search(r'\] ([0-9.]+) fps \(present\).*?cap (\d+)/s', line)
        if m:
            pres.append(float(m.group(1))); cap.append(int(m.group(2)))
        m2 = re.search(r'done \(real=(\d+) interp=(\d+) total_presents=(\d+)\)', line)
        if m2:
            done = {'real': int(m2.group(1)), 'interp': int(m2.group(2)), 'total': int(m2.group(3))}
    if not pres:
        return None
    return {'present_fps': float(np.median(pres)), 'present_min': float(min(pres)), 'present_max': float(max(pres)),
            'capture_fps': float(np.median(cap)), 'capture_min': int(min(cap)), 'capture_max': int(max(cap)),
            'seconds': len(pres), 'done': done}


def png_rgba(path, rgba8):
    """Lossless RGBA PNG (colour type 6), stdlib only — the overlay needs an alpha channel, which
    scene_report.png (RGB, type 2) does not carry. Same chunk writer, one byte of IHDR apart."""
    import struct, zlib
    H, W = rgba8.shape[:2]
    raw = b''.join(b'\x00' + rgba8[y].tobytes() for y in range(H))
    def ch(tag, dat):
        return struct.pack('>I', len(dat)) + tag + dat + struct.pack('>I', zlib.crc32(tag + dat) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0))
                           + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))


def overlay_png(path, masks, W, H):
    """The scorer's own masks as a transparent overlay: RED where the candidate put mass the truth has
    none (`halluc`), BLUE where the truth has mass the candidate missed (`missing`). Nothing is
    recomputed here — these are the arrays the px² numbers were counted from."""
    ov = np.zeros((H, W, 4), np.uint8)
    hal, mis = masks.get('halluc'), masks.get('missing')
    if hal is not None:
        ov[hal] = (224, 64, 48, 168)
    if mis is not None:
        ov[mis] = (56, 120, 232, 168)
    png_rgba(path, ov)
    return int(hal.sum()) if hal is not None else 0, int(mis.sum()) if mis is not None else 0


def interior_png(path, cand, truth, obj, erode=2):
    """The error INSIDE the silhouette, where no object term looks.

    Every one of the six verdict terms is a silhouette term: position, shape, hallucinated and missing mass are
    all computed from where the outline is, and `sharp` is measured on a one-pixel band around the truth's own
    boundary. So a warp that reproduces the outline and deforms what is inside it — a checker that bends, a
    texture that slides — scores at the floor. `l2_det` sees it, but over the whole determinable frame and never
    per object. This map is |candidate − truth| in luminance on the truth's silhouette eroded by `erode` px, so
    the boundary band the terms DO cover is excluded and only the unwatched interior remains. It is a view, not
    a term: nothing here enters a verdict (the operator decides whether it ever should).

    Returns (rms, p99, max) over the interior, on the 0..1 luminance scale, or None when there is no interior.
    """
    m = obj
    for _ in range(erode):
        m = SR.erode4(m)
    if not m.any():
        return None
    e = np.abs(SR.lum(cand) - SR.lum(truth))
    v = e[m]
    hot = np.clip(e / max(float(v.max()), 1e-6), 0, 1)
    rgba = np.zeros(m.shape + (4,), np.uint8)
    # a single-hue ramp: dark red where the interior is wrong a little, white-hot where it is wrong a lot
    rgba[..., 0] = np.clip(60 + 195 * hot, 0, 255)
    rgba[..., 1] = np.clip(255 * (hot ** 2.2), 0, 255)
    rgba[..., 2] = np.clip(255 * (hot ** 4.0), 0, 255)
    rgba[..., 3] = np.where(m, np.clip(40 + 215 * hot, 0, 255), 0)
    png_rgba(path, rgba)
    return float(np.sqrt((v ** 2).mean())), float(np.percentile(v, 99)), float(v.max())


_S = {}   # per-PROCESS stepper state, the twin of scene_report._W (see _frame_worker_init)


def _frame_worker_init(d, K, arm, out, sil, overlay):
    """Load, ONCE per process, everything a frame needs: the scorer's own state plus this file's context.

    Why this exists: 72 % of the cost of a page is the truth render, one per generated frame, and this file
    was the last scoring path in the project still running it in a single thread (the scorer has been pooled
    since 2026-09-09). A 177-frame page took ~12 minutes of wall clock on a 32-thread machine.
    """
    global _S
    SR._worker_init(d, arm, sil)
    t = SR._W['t']
    _S = {'d': d, 'K': K, 'arm': arm, 'out': out, 'overlay': overlay,
          'W': t['width'], 'H': t['height'], 'trs': {tr['mid']: tr for tr in SR.triples(d, K)}}


def _frame_worker(args):
    """One frame -> its record and its PNGs. Returns ONLY the small record: the images are written here, so
    no float image and no mask array ever crosses a process boundary."""
    i, real, is_cut = args
    d, K, arm, out, W, H = (_S[k] for k in ('d', 'K', 'arm', 'out', 'W', 'H'))
    t, sc = SR._W['t'], SR._W['sc']
    truth = SR.load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % i), W, H)
    if real:
        cand = truth
    else:
        tr = _S['trs'].get(i)
        if tr is None:
            return None
        cand = SR.arm_frame(t, sc, d, tr, arm)
    missing = (not real) and cand is None
    mrec = None
    if _S['overlay'] and not real and not missing and not is_cut:
        kind, rr = SR._score_triple((K, _S['trs'][i], True))
        if kind == 'row':
            mrec = rr
            truth = rr['_truth']            # the truth the scorer used: at the FG's OWN phase
    cpath = 'seq/f_%06d_c.png' % i
    tpath = cpath
    if missing:
        tpath = 'seq/f_%06d_t.png' % i
        SR.png(os.path.join(out, tpath), SR.u8(truth))
        cpath = tpath
    else:
        SR.png(os.path.join(out, cpath), SR.u8(cand))
        if not real:
            tpath = 'seq/f_%06d_t.png' % i
            SR.png(os.path.join(out, tpath), SR.u8(truth))
    rec = {'i': i, 'real': real, 'c': cpath, 't': tpath, 'missing': missing, 'cut': is_cut,
           'time_s': i / float(t['base_fps'])}
    if mrec is not None:
        opath = 'seq/f_%06d_o.png' % i
        hp, mp = overlay_png(os.path.join(out, opath), mrec['_masks'], W, H)
        rec['o'] = opath
        rec['ov'] = {'halluc_px': hp, 'missing_px': mp}
        ipath = 'seq/f_%06d_i.png' % i
        st = interior_png(os.path.join(out, ipath), cand, mrec['_truth'], mrec['_masks']['truth_obj'])
        if st:
            rec['ii'] = ipath
            rec['iv'] = {'rms': st[0], 'p99': st[1], 'max': st[2]}
    return rec


def build(d, K, arm, out, scores_json, start, count, fg_log=None, overlay=False, sil='tau', jobs=1):
    t, sc = SR.load_corpus(d)
    W, H, n = t['width'], t['height'], t['frames']
    base = float(t['base_fps'])
    if not fg_log and os.path.exists(os.path.join(d, 'fg_k%d.log' % K)):
        fg_log = os.path.join(d, 'fg_k%d.log' % K)          # where scene_live.ps1 keeps the FG's stdout
    rates = {'base_fps': base, 'source_fps': base / K, 'output_fps': base, 'k': K,
             'source_interval_ms': 1000.0 * K / base, 'output_interval_ms': 1000.0 / base,
             'measured': measured_rates(fg_log)}
    os.makedirs(os.path.join(out, 'seq'), exist_ok=True)
    trs = {tr['mid']: tr for tr in SR.triples(d, K)}
    rows = {}
    if scores_json:
        blob = json.load(open(scores_json, encoding='utf-8'))
        key = next((k for k in blob if k.endswith('|k%d' % K) and os.path.normpath(k.split('|k')[0]) == os.path.normpath(d)), None)
        if key:
            # the arm the scores were filed under may predate the per-k naming (fg vs fg_k4)
            akey = arm if arm in blob[key]['rows'] else next((a for a in blob[key]['rows'] if a.startswith('fg')), None)
            if akey:
                rows = {r['mid']: r for r in blob[key]['rows'][akey]}
    # a live arm's align.json says which generated frames bridged a CUT (real pair not k apart --
    # the looped player's seam); those are shown, marked, and never carry a score
    cuts = set()
    ap_ = os.path.join(d, 'arms', arm, 'align.json')
    if os.path.exists(ap_):
        cuts = {r['mid'] for r in json.load(open(ap_, encoding='utf-8'))['rows']
                if 'mid' in r and not r.get('pair_ok', True) and 'skipped' not in r and 'dup_of' not in r}
    last_real = ((n - 1) // K) * K
    lo, hi = max(0, start), min(last_real, start + count - 1 if count else last_real)
    # The frame work is independent per index and 72 % of it is one truth render, so it is pooled exactly the
    # way scene_report.score_arm pools its triples: each process loads the corpus once and writes its own PNGs,
    # and only the small record comes back. jobs = 1 runs the same function in this process, so the serial and
    # the pooled page are produced by ONE code path (verified byte-identical, --jobs 1 vs 12).
    todo = [(i, (i % K == 0), (i in cuts)) for i in range(lo, hi + 1)
            if (i % K == 0) or i in trs]
    if jobs <= 1 or len(todo) < 2:
        _frame_worker_init(d, K, arm, out, sil, overlay)
        recs = [_frame_worker(a) for a in todo]
    else:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=min(jobs, len(todo)),
                                 initializer=_frame_worker_init,
                                 initargs=(d, K, arm, out, sil, overlay)) as pool:
            recs = list(pool.map(_frame_worker, todo, chunksize=2))
    frames = []
    for rec in recs:
        if rec is None:
            continue
        i, real = rec['i'], rec['real']
        if not real:
            tr = trs[i]
            rec.update({'phase': tr['phase'], 'N': tr['N'], 'N1': tr['N1']})
            r = rows.get(i)
            if r and i not in cuts:
                objs = r['objects']
                m = lambda key: float(np.nanmean([objs[k][key] for k in objs])) if objs else None
                rec['s'] = {'pos': m('pos_err'), 'shape': m('shape_err'), 'halluc': m('halluc_px'),
                            'lead': m('lead_px'), 'missing': m('missing_px'), 'sharp': r['sharp'],
                            'disocc_px': r['disocc_px']}
        frames.append(rec)
    meta = {'W': W, 'H': H, 'K': K, 'arm': arm, 'corpus': os.path.basename(os.path.normpath(d)),
            'rates': rates, 'overlay': overlay, 'silhouette': sil}
    json.dump({'meta': meta, 'frames': frames}, open(os.path.join(out, 'frames.json'), 'w', encoding='utf-8'))
    write_html(out, frames, W, H, K, arm, os.path.basename(os.path.normpath(d)), rates, overlay)
    print('%d frames (%d real, %d generated%s) -> %s'
          % (len(frames), sum(f['real'] for f in frames), sum(not f['real'] for f in frames),
             ', %d with the scorer\'s masks' % sum('o' in f for f in frames) if overlay else '',
             os.path.join(out, 'index.html')))


def worst_value(f, key):
    """The number a chip ranks and shows. For hallucinated / missing mass that is the DRAWN total (the union
    over objects, what the overlay puts on the screen) when the masks exist, so the ranking and the picture
    agree; the per-object mean the report tables carry is a different quantity and is labelled as such in the
    panel. pos has no union form and is always the per-object mean."""
    if key in ('halluc', 'missing') and f.get('ov'):
        return f['ov'][key + '_px']
    if key == 'interior':
        return (f.get('iv') or {}).get('p99')
    return (f.get('s') or {}).get(key)


def worst_lists(frames, n=10):
    """The frames a reviewer should look at first, ranked by the terms that name an artefact: the most
    hallucinated mass, and the largest position error. Scored generated frames only; a cut carries no score."""
    scored = [(j, f) for j, f in enumerate(frames) if f.get('s')]
    def top(key):
        v = [(j, worst_value(f, key)) for j, f in scored if worst_value(f, key) is not None]
        return [j for j, _ in sorted(v, key=lambda p: -p[1])[:n]]
    return {'halluc': top('halluc'), 'pos': top('pos'), 'interior': top('interior')}


def write_html(out, frames, W, H, K, arm, corpus, rates, overlay=False):
    css = """
:root{--bg:#f6f5f2;--ink:#1c1b19;--mut:#6b675f;--line:#dcd8d0;--real:#2f8f5e;--gen:#c98a1a;--red:#b03a2e}
@media(prefers-color-scheme:dark){:root{--bg:#121210;--ink:#e8e4da;--mut:#9b968a;--line:#2d2b25;--real:#5fd39a;--gen:#f0b545;--red:#e0705f}}
html,body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;height:100%}
#wrap{display:grid;grid-template-rows:auto 1fr auto auto;height:100vh}
header{display:flex;gap:18px;align-items:baseline;padding:10px 18px;border-bottom:1px solid var(--line)}
header h1{margin:0;font-size:16px;font-weight:600} header .sub{color:var(--mut);font-size:12px}
#stage{position:relative;display:flex;align-items:center;justify-content:center;background:#000;overflow:hidden}
canvas{max-width:100%;max-height:100%;image-rendering:pixelated}
#badge{position:absolute;left:18px;top:14px;font-size:28px;font-weight:700;letter-spacing:.04em;padding:6px 14px;border-radius:8px;color:#000}
#badge.real{background:var(--real)} #badge.gen{background:var(--gen)}
#idx{position:absolute;right:18px;top:14px;font-size:22px;font-weight:600;color:#fff;text-shadow:0 1px 3px #000}
#mode{position:absolute;left:18px;bottom:12px;color:#fff;font-size:13px;text-shadow:0 1px 3px #000}
#panel{display:grid;grid-template-columns:1fr auto;gap:14px;padding:10px 18px;border-top:1px solid var(--line);font-size:13px;min-height:52px}
#panel b{font-weight:600} .k{color:var(--mut)} .bad{color:var(--red);font-weight:600}
#tl{position:relative;height:38px;margin:0 18px 10px;border:1px solid var(--line);border-radius:6px;cursor:pointer;overflow:hidden}
#tl canvas{position:absolute;inset:0;width:100%;height:100%;image-rendering:auto}
#help{padding:0 18px 10px;color:var(--mut);font-size:12px}
#prog{position:absolute;left:0;right:0;bottom:0;height:3px;background:var(--gen);transform-origin:left;transform:scaleX(0)}
#worst{display:flex;flex-wrap:wrap;gap:6px;align-items:center;padding:0 18px 8px;font-size:12px}
#worst .lab{color:var(--mut)} #worst .sep{width:14px}
#worst button{font:inherit;cursor:pointer;border:1px solid var(--line);background:transparent;color:var(--ink);border-radius:5px;padding:2px 8px}
#worst button:hover{border-color:var(--gen)} #worst button.on{background:var(--gen);color:#000;border-color:var(--gen)}
.sw{display:inline-block;width:9px;height:9px;border-radius:2px;vertical-align:baseline;margin-right:4px}
#fb{display:flex;gap:8px;align-items:center;padding:0 18px 10px;font-size:12px}
#fb input{flex:1;font:inherit;padding:5px 9px;border:1px solid var(--line);border-radius:5px;background:transparent;color:var(--ink)}
#fb input:focus{outline:none;border-color:var(--gen)}
#fb button{font:inherit;cursor:pointer;border:1px solid var(--line);background:transparent;color:var(--ink);border-radius:5px;padding:4px 10px;white-space:nowrap}
#fb button:hover{border-color:var(--gen)} #mk.on{background:var(--red);color:#fff;border-color:var(--red)}
#fbn{color:var(--mut);white-space:nowrap}
#flag{position:absolute;right:18px;bottom:12px;font-size:22px;color:var(--red);text-shadow:0 1px 3px #000;display:none}
"""
    data = json.dumps(frames)
    m = rates.get('measured')
    meas = ('' if not m else
            ' · <b>measured</b> present %.1f fps (%.1f–%.1f), capture %.0f/s (%d–%d) over %d status lines%s'
            % (m['present_fps'], m['present_min'], m['present_max'], m['capture_fps'], m['capture_min'], m['capture_max'], m['seconds'],
               (' · %d real + %d generated = %d presented' % (m['done']['real'], m['done']['interp'], m['done']['total'])) if m['done'] else ''))
    ratesline = ('<div class="sub" id="rates"><b>REAL %.1f fps</b> (every %d-th frame of a %.0f fps base, %.2f ms apart) → '
                 '<b>FG ×%d → %.0f fps presented</b> (%.2f ms apart)%s</div>'
                 % (rates['source_fps'], K, rates['base_fps'], rates['source_interval_ms'], K, rates['output_fps'], rates['output_interval_ms'], meas))
    wl = worst_lists(frames)
    if wl.get('interior'):
        inner = ('<span class="sep"></span><span class="lab">peores por error DENTRO de la silueta (p99, ningún término lo mide)</span>'
                 + ''.join('<button data-j="%d">#%d <span class="k">%.3f</span></button>' % (j, frames[j]['i'], worst_value(frames[j], 'interior'))
                           for j in wl['interior']))
    else:
        inner = ''
    if wl['halluc'] or wl['pos']:
        chip = lambda j, val, dec: '<button data-j="%d">#%d <span class="k">%s</span></button>' % (
            j, frames[j]['i'], ('%.*f' % (dec, val)) if val is not None else '—')
        worstline = ('<div id="worst"><span class="lab">peores por masa alucinada (px², la dibujada)</span>'
                     + ''.join(chip(j, worst_value(frames[j], 'halluc'), 0) for j in wl['halluc'])
                     + '<span class="sep"></span><span class="lab">peores por error de posición (px)</span>'
                     + ''.join(chip(j, worst_value(frames[j], 'pos'), 3) for j in wl['pos'])
                     + inner
                     + (('<span class="sep"></span><span class="lab"><span class="sw" style="background:#e04030"></span>hallucinated'
                         ' <span class="sw" style="background:#3878e8;margin-left:8px"></span>missing — the scorer\'s own masks (H)</span>')
                        if overlay else '')
                     + '</div>')
    else:
        worstline = ''
    parts = ['<!doctype html><meta charset="utf-8"><title>scene_truth step · %s k=%d %s</title><style>%s</style>' % (html.escape(corpus), K, html.escape(arm), css),
             '<div id="wrap"><header><div><h1>%s · k = %d · arm <code>%s</code></h1>%s</div>' % (html.escape(corpus), K, html.escape(arm), ratesline),
             '<span class="sub">%d frames · %d×%d · real every %d</span></header>' % (len(frames), W, H, K),
             '<div id="stage"><canvas id="cv" width="%d" height="%d"></canvas><div id="badge"></div><div id="idx"></div><div id="mode"></div><div id="flag">● marcado</div><div id="prog"></div></div>' % (W, H),
             '<div id="panel"><div id="info"></div><div id="rate" class="k"></div></div>',
             '<div id="tl"><canvas id="tlc"></canvas></div>',
             worstline,
             '<div id="fb"><button id="mk" title="M">● marcar</button>'
             '<input id="note" placeholder="qué ve en este cuadro (se guarda solo; N para escribir, Esc para salir)" autocomplete="off">'
             '<span id="fbn"></span><button id="cp" title="C">copiar retroalimentación</button>'
             '<button id="cl">borrar todo</button></div>',
             '<div id="help">← → step · <b>hold</b> to auto-advance · [ ] slower/faster · space play · L loop · T truth in place · D difference%s · '
             '<b>M</b> marcar · <b>N</b> nota · <b>C</b> copiar · Home/End · click the timeline</div></div>'
             % (' · <b>H</b> masa alucinada / faltante · <b>I</b> error DENTRO de la silueta · <b>W</b> peor cuadro' if overlay else ''),
             '<script>const F=%s;const W=%d,H=%d,K=%d;const RATES=%s;const WORST=%s;const HASOV=%s;const PAGE=%s;'
             % (data, W, H, K, json.dumps({k: v for k, v in rates.items() if k != 'measured'}),
                json.dumps(worst_lists(frames)), 'true' if overlay else 'false',
                json.dumps('%s k=%d arm %s' % (corpus, K, arm))),
             r"""
const cv=document.getElementById('cv'),cx=cv.getContext('2d'),badge=document.getElementById('badge'),idx=document.getElementById('idx'),
 mode=document.getElementById('mode'),info=document.getElementById('info'),rate=document.getElementById('rate'),prog=document.getElementById('prog'),
 tl=document.getElementById('tl'),tlc=document.getElementById('tlc'),tx=tlc.getContext('2d');
const IMG={};let loaded=0,total=0;
function load(src){if(IMG[src])return IMG[src];const im=new Image();IMG[src]=im;total++;im.onload=()=>{loaded++;prog.style.transform='scaleX('+(loaded/total)+')';if(loaded===total)prog.style.display='none'};im.src=src;return im}
F.forEach(f=>{load(f.c);if(f.t!==f.c)load(f.t);if(f.o)load(f.o);if(f.ii)load(f.ii)});
let cur=0,showTruth=false,diff=false,ovOn=false,inOn=false,fps=8,timer=null,playing=false,loop=true,hold=null;
const WFLAT=[].concat(WORST.halluc||[],WORST.pos||[],WORST.interior||[]).filter((v,i,a)=>a.indexOf(v)===i);let wi=-1;
function fmt(v,d){return v==null?'—':Number(v).toFixed(d)}
function draw(){const f=F[cur];const a=load(f.c),b=load(f.t);
 const useT=showTruth&&!f.real;const im=useT?b:a;
 if(!im.complete||!im.naturalWidth){requestAnimationFrame(draw);return}
 if(diff&&!f.real&&b.complete&&b.naturalWidth){cx.drawImage(a,0,0);const A=cx.getImageData(0,0,W,H);cx.drawImage(b,0,0);const B=cx.getImageData(0,0,W,H);
  const o=cx.createImageData(W,H);for(let p=0;p<A.data.length;p+=4){const d=Math.min(255,4*(Math.abs(A.data[p]-B.data[p])+Math.abs(A.data[p+1]-B.data[p+1])+Math.abs(A.data[p+2]-B.data[p+2]))/3);o.data[p]=d;o.data[p+1]=Math.max(0,d-60);o.data[p+2]=Math.max(0,d-120);o.data[p+3]=255}cx.putImageData(o,0,0)}
 else cx.drawImage(im,0,0);
 if(inOn&&f.ii&&!useT){const iv=load(f.ii);if(iv.complete&&iv.naturalWidth)cx.drawImage(iv,0,0)}
 if(ovOn&&f.o&&!useT){const ov=load(f.o);if(ov.complete&&ov.naturalWidth)cx.drawImage(ov,0,0)}
 badge.textContent=f.real?'REAL':(f.missing?'NOT CAPTURED — truth shown':(f.cut?'GENERATED ACROSS A CUT — not scored':'GENERATED'));badge.className=f.real?'real':'gen';
 cv.style.opacity=f.missing?'0.45':'1';
 idx.textContent='#'+f.i+'  t '+fmt(f.time_s,4)+' s'+(f.real?'':'  φ '+fmt(f.phase,2));
 mode.textContent=(diff&&!f.real?'|candidate − truth| ×4':useT?'TRUTH in place':'')+(inOn&&f.ii&&!useT?' + interior de la silueta':'')+(ovOn&&f.o&&!useT?' + halluc / missing':'')+(playing?'  ▶':'');
 let h='';
 if(f.real)h='<b>real frame</b> <span class="k">source index '+f.i+' — the FG saw this one, '+RATES.source_interval_ms.toFixed(2)+' ms after the previous real</span>';
 else{h='<b>generated</b> between real <b>'+f.N+'</b> → <b>'+f.N1+'</b>, phase '+fmt(f.phase,3)+' <span class="k">('+(f.phase*RATES.source_interval_ms).toFixed(2)+' ms after real '+f.N+', presented '+RATES.output_interval_ms.toFixed(2)+' ms after the previous frame)</span>'+(f.cut?' — <span class="bad">the two real frames were not k apart (the loop seam): a CUT, no interpolation truth exists</span>':'');
  if(f.s){const s=f.s;h+='<br><span class="k">media por objeto:</span> pos <b>'+fmt(s.pos,3)+'</b> px · shape <b>'+fmt(s.shape,3)+'</b> px · halluc <b>'+fmt(s.halluc,0)+'</b> px² (lead '+fmt(s.lead,1)+') · missing <b>'+fmt(s.missing,0)+'</b> px² · sharp <b>'+fmt(s.sharp,3)+'</b>'+(s.sharp!=null&&s.sharp<0.9?' <span class="bad">BLUR</span>':'')+' · class-0 '+fmt(s.disocc_px,0)+' px';
   if(f.ov)h+='<br><span class="k">lo que está dibujado (unión de todos los objetos):</span> <span style="color:#e04030">halluc <b>'+f.ov.halluc_px+'</b> px²</span> · <span style="color:#3878e8">missing <b>'+f.ov.missing_px+'</b> px²</span>';
   if(f.iv)h+=' <span class="k">· interior de la silueta (NO entra en ningún término): rms <b>'+fmt(f.iv.rms,4)+'</b> · p99 <b>'+fmt(f.iv.p99,4)+'</b> · máx <b>'+fmt(f.iv.max,3)+'</b> (tecla I)</span>'}}
 info.innerHTML=h;rate.textContent=fps+' fps auto · '+(loop?'loop':'stop at end');fbSync()}
function drawTL(){const w=tl.clientWidth,h=tl.clientHeight;if(tlc.width!==w||tlc.height!==h){tlc.width=w;tlc.height=h}
 tx.clearRect(0,0,w,h);const n=F.length;for(let i=0;i<n;i++){const x=(i+0.5)/n*w;const f=F[i];tx.fillStyle=f.real?getComputedStyle(document.documentElement).getPropertyValue('--real'):getComputedStyle(document.documentElement).getPropertyValue('--gen');
  const t=f.real?4:h*0.45;tx.fillRect(x-0.5,h-t,Math.max(1,w/n-0.5),t)}
 if(typeof FB!=='undefined'){const red=getComputedStyle(document.documentElement).getPropertyValue('--red');
  for(let i=0;i<n;i++){if(FB[F[i].i]){const x=(i+0.5)/n*w;tx.fillStyle=red;tx.fillRect(x-1,0,Math.max(2,w/n),h*0.4)}}}
 const x=(cur+0.5)/n*w;tx.fillStyle='#fff';tx.fillRect(x-1,0,2,h)}
function go(i){if(i<0)i=loop?F.length-1:0;if(i>=F.length){if(!loop){stop();i=F.length-1}else i=0}cur=i;draw()}
function step(d){go(cur+d)}
function startAuto(d){stopAuto();hold=d;timer=setInterval(()=>step(d),1000/fps)}
function stopAuto(){if(timer){clearInterval(timer);timer=null}hold=null}
function play(){playing=true;startAuto(1)}function stop(){playing=false;stopAuto();draw()}
/* ---- feedback: a mark and a note per frame, kept in this browser, exported as one anchored block ---- */
const mk=document.getElementById('mk'),note=document.getElementById('note'),fbn=document.getElementById('fbn'),
 cp=document.getElementById('cp'),cl=document.getElementById('cl'),flag=document.getElementById('flag');
const FBKEY='scene_truth_fb::'+PAGE;
let FB={};try{FB=JSON.parse(localStorage.getItem(FBKEY)||'{}')}catch(e){FB={}}
function fbSave(){try{localStorage.setItem(FBKEY,JSON.stringify(FB))}catch(e){}}
function fbEntry(i){return FB[i]||null}
function fbCount(){return Object.keys(FB).length}
function fbSync(full){const f=F[cur],e=fbEntry(f.i);
 mk.classList.toggle('on',!!e);flag.style.display=e?'block':'none';
 if(full!==false)note.value=(e&&e.n)||'';
 fbn.textContent=fbCount()?fbCount()+' marcado(s)':'';drawTL()}
function fbToggle(){const f=F[cur],e=fbEntry(f.i);
 if(e){if((e.n||'').trim()&&!confirm('Este cuadro tiene una nota. ¿Quitar la marca y borrar la nota?'))return;delete FB[f.i]}
 else FB[f.i]={m:1,n:''};
 fbSave();fbSync()}
function fbNote(){const f=F[cur];
 FB[f.i]=Object.assign({m:1},FB[f.i]||{},{n:note.value});
 fbSave();fbSync(false)}
function fbText(){const ids=Object.keys(FB).map(Number).sort((a,b)=>a-b);if(!ids.length)return'';
 const L=['# scene_truth — retroalimentación · '+PAGE+(HASOV?' · operador coverage':''),
          '# pos/shape/sharp = media por objeto; halluc/missing = la masa DIBUJADA, unión de todos los objetos'];
 ids.forEach(i=>{const f=F.find(x=>x.i===i);if(!f)return;const s=f.s;
  let ln='#'+i+(f.real?'  REAL':'  φ '+fmt(f.phase,3));
  if(s)ln+='  pos '+fmt(s.pos,3)+'  sharp '+fmt(s.sharp,3);
  if(f.ov)ln+='  halluc '+f.ov.halluc_px+'  missing '+f.ov.missing_px;
  else if(s)ln+='  halluc(media) '+fmt(s.halluc,0)+'  missing(media) '+fmt(s.missing,0);
  if(f.iv)ln+='  interior_p99 '+fmt(f.iv.p99,4);
  if(f.cut)ln+='  [CUT, sin verdad de interpolación]';
  ln+='  → '+((FB[i].n||'').trim()||'(marcado, sin nota)');L.push(ln)});
 return L.join('\n')}
cp.addEventListener('click',()=>{const t=fbText();if(!t){fbn.textContent='nada marcado todavía';return}
 navigator.clipboard.writeText(t).then(()=>{fbn.textContent='copiado: '+fbCount()+' cuadro(s)'},
  ()=>{const w=window.open('','_blank');w.document.write('<pre>'+t.replace(/[&<]/g,c=>c==='&'?'&amp;':'&lt;')+'</pre>')})});
cl.addEventListener('click',()=>{if(confirm('¿Borrar las '+fbCount()+' marcas de esta página?')){FB={};fbSave();fbSync()}});
mk.addEventListener('click',fbToggle);
note.addEventListener('input',fbNote);
note.addEventListener('keydown',e=>{if(e.key==='Escape'){note.blur()}e.stopPropagation()});
document.addEventListener('keydown',e=>{
 if(e.target===note)return;
 if(e.key==='m'||e.key==='M'){fbToggle();return}
 if(e.key==='n'||e.key==='N'){e.preventDefault();note.focus();return}
 if(e.key==='c'||e.key==='C'){cp.click();return}
 if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();const d=e.key==='ArrowRight'?1:-1;if(e.repeat)return;step(d);if(!playing){hold=d;setTimeout(()=>{if(hold===d&&!timer)startAuto(d)},260)}return}
 if(e.key===' '){e.preventDefault();playing?stop():play();return}
 if(e.key==='t'||e.key==='T'){showTruth=!showTruth;draw()}
 if(e.key==='d'||e.key==='D'){diff=!diff;draw()}
 if(e.key==='h'||e.key==='H'){if(HASOV){ovOn=!ovOn;draw()}}
 if(e.key==='i'||e.key==='I'){if(HASOV){inOn=!inOn;draw()}}
 if(e.key==='w'||e.key==='W'){if(WFLAT.length){wi=(wi+1)%WFLAT.length;go(WFLAT[wi])}}
 if(e.key==='l'||e.key==='L'){loop=!loop;draw()}
 if(e.key===']'){fps=Math.min(60,Math.round(fps*1.5));draw();if(timer)startAuto(hold||1)}
 if(e.key==='['){fps=Math.max(1,Math.round(fps/1.5));draw();if(timer)startAuto(hold||1)}
 if(e.key==='Home'){go(0)}if(e.key==='End'){go(F.length-1)}});
document.addEventListener('keyup',e=>{if((e.key==='ArrowRight'||e.key==='ArrowLeft')&&!playing){stopAuto()}});
tl.addEventListener('click',e=>{const r=tl.getBoundingClientRect();go(Math.floor((e.clientX-r.left)/r.width*F.length))});
document.querySelectorAll('#worst button').forEach(b=>b.addEventListener('click',()=>{
 const j=+b.dataset.j;if(HASOV)ovOn=true;go(j);
 document.querySelectorAll('#worst button').forEach(x=>x.classList.toggle('on',+x.dataset.j===j))}));
window.addEventListener('resize',drawTL);go(0);
</script>"""]
    open(os.path.join(out, 'index.html'), 'w', encoding='utf-8', newline='\n').write('\n'.join(parts) + '\n')


def main():
    ap = argparse.ArgumentParser(description='scene_truth — frame stepper (real / generated, full frame)')
    ap.add_argument('--run', required=True, help='corpus dir')
    ap.add_argument('--k', type=int, default=4)
    ap.add_argument('--arm', default='oracle2', help='materialised arms/<name>/ or a synthetic arm')
    ap.add_argument('--out', required=True)
    ap.add_argument('--scores', help='the scorer --json, to show each generated frame\'s terms')
    ap.add_argument('--start', type=int, default=0)
    ap.add_argument('--count', type=int, default=0, help='0 = to the last real frame')
    ap.add_argument('--fg-log', help='the raw FG stdout of this run, for the MEASURED present and capture rates')
    ap.add_argument('--overlay', action='store_true',
                    help="show what the SCORER saw: its own halluc / missing masks per generated frame (key H), and "
                         "the truth rendered at the FG's OWN phase instead of the base-grid frame. Costs one truth "
                         "render per generated frame, serially — the same work one arm of the scorer does")
    ap.add_argument('--silhouette', choices=['tau', 'coverage'], default='tau',
                    help='the silhouette operator the masks are taken with; must match the operator the --scores '
                         'JSON was produced with, or the picture and the number disagree')
    ap.add_argument('--rehtml', action='store_true',
                    help='rewrite only index.html from the frames.json an earlier build left in --out. No frame is '
                         'rendered and no PNG is touched, so changing the page itself costs seconds instead of the '
                         'truth render of every generated frame')
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 2),
                    help='processes over the frames. The truth render is 72 %% of a page and is independent per '
                         'frame, so this is the same pooling scene_report.py uses; 1 = serial, same code path')
    a = ap.parse_args()
    if a.rehtml:
        blob = json.load(open(os.path.join(a.out, 'frames.json'), encoding='utf-8'))
        m = blob['meta']
        write_html(a.out, blob['frames'], m['W'], m['H'], m['K'], m['arm'], m['corpus'], m['rates'], m['overlay'])
        print('index.html rewritten from frames.json (%d frames) -> %s' % (len(blob['frames']), os.path.join(a.out, 'index.html')))
        return
    build(a.run, a.k, a.arm, a.out, a.scores, a.start, a.count, a.fg_log, a.overlay, a.silhouette, a.jobs)


if __name__ == '__main__':
    main()
