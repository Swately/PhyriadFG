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


def build(d, K, arm, out, scores_json, start, count, fg_log=None):
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
    frames = []
    for i in range(lo, hi + 1):
        real = (i % K == 0)
        truth = SR.load_rgb(os.path.join(d, 'frames', 'f_%06d.rgba' % i), W, H)
        if real:
            cand = truth
        else:
            tr = trs.get(i)
            if tr is None:
                continue
            cand = SR.arm_frame(t, sc, d, tr, arm)
        missing = (not real) and cand is None       # the live FG's --qdump SAMPLES: not every tick is dumped
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
        rec = {'i': i, 'real': real, 'c': cpath, 't': tpath, 'missing': missing, 'cut': i in cuts,
               'time_s': i / base}
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
        if (i - lo + 1) % 40 == 0:
            print('  %d/%d' % (i - lo + 1, hi - lo + 1))
    write_html(out, frames, W, H, K, arm, os.path.basename(os.path.normpath(d)), rates)
    print('%d frames (%d real, %d generated) -> %s' % (len(frames), sum(f['real'] for f in frames),
                                                       sum(not f['real'] for f in frames), os.path.join(out, 'index.html')))


def write_html(out, frames, W, H, K, arm, corpus, rates):
    css = """
:root{--bg:#f6f5f2;--ink:#1c1b19;--mut:#6b675f;--line:#dcd8d0;--real:#2f8f5e;--gen:#c98a1a;--red:#b03a2e}
@media(prefers-color-scheme:dark){:root{--bg:#121210;--ink:#e8e4da;--mut:#9b968a;--line:#2d2b25;--real:#5fd39a;--gen:#f0b545;--red:#e0705f}}
html,body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;height:100%%}
#wrap{display:grid;grid-template-rows:auto 1fr auto auto;height:100vh}
header{display:flex;gap:18px;align-items:baseline;padding:10px 18px;border-bottom:1px solid var(--line)}
header h1{margin:0;font-size:16px;font-weight:600} header .sub{color:var(--mut);font-size:12px}
#stage{position:relative;display:flex;align-items:center;justify-content:center;background:#000;overflow:hidden}
canvas{max-width:100%%;max-height:100%%;image-rendering:pixelated}
#badge{position:absolute;left:18px;top:14px;font-size:28px;font-weight:700;letter-spacing:.04em;padding:6px 14px;border-radius:8px;color:#000}
#badge.real{background:var(--real)} #badge.gen{background:var(--gen)}
#idx{position:absolute;right:18px;top:14px;font-size:22px;font-weight:600;color:#fff;text-shadow:0 1px 3px #000}
#mode{position:absolute;left:18px;bottom:12px;color:#fff;font-size:13px;text-shadow:0 1px 3px #000}
#panel{display:grid;grid-template-columns:1fr auto;gap:14px;padding:10px 18px;border-top:1px solid var(--line);font-size:13px;min-height:52px}
#panel b{font-weight:600} .k{color:var(--mut)} .bad{color:var(--red);font-weight:600}
#tl{position:relative;height:38px;margin:0 18px 10px;border:1px solid var(--line);border-radius:6px;cursor:pointer;overflow:hidden}
#tl canvas{position:absolute;inset:0;width:100%%;height:100%%;image-rendering:auto}
#help{padding:0 18px 10px;color:var(--mut);font-size:12px}
#prog{position:absolute;left:0;right:0;bottom:0;height:3px;background:var(--gen);transform-origin:left;transform:scaleX(0)}
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
    parts = ['<!doctype html><meta charset="utf-8"><title>scene_truth step · %s k=%d %s</title><style>%s</style>' % (html.escape(corpus), K, html.escape(arm), css),
             '<div id="wrap"><header><div><h1>%s · k = %d · arm <code>%s</code></h1>%s</div>' % (html.escape(corpus), K, html.escape(arm), ratesline),
             '<span class="sub">%d frames · %d×%d · real every %d</span></header>' % (len(frames), W, H, K),
             '<div id="stage"><canvas id="cv" width="%d" height="%d"></canvas><div id="badge"></div><div id="idx"></div><div id="mode"></div><div id="prog"></div></div>' % (W, H),
             '<div id="panel"><div id="info"></div><div id="rate" class="k"></div></div>',
             '<div id="tl"><canvas id="tlc"></canvas></div>',
             '<div id="help">← → step · <b>hold</b> to auto-advance · [ ] slower/faster · space play · L loop · T truth in place · D difference · Home/End · click the timeline</div></div>',
             '<script>const F=%s;const W=%d,H=%d,K=%d;const RATES=%s;' % (data, W, H, K, json.dumps({k: v for k, v in rates.items() if k != 'measured'})),
             r"""
const cv=document.getElementById('cv'),cx=cv.getContext('2d'),badge=document.getElementById('badge'),idx=document.getElementById('idx'),
 mode=document.getElementById('mode'),info=document.getElementById('info'),rate=document.getElementById('rate'),prog=document.getElementById('prog'),
 tl=document.getElementById('tl'),tlc=document.getElementById('tlc'),tx=tlc.getContext('2d');
const IMG={};let loaded=0,total=0;
function load(src){if(IMG[src])return IMG[src];const im=new Image();IMG[src]=im;total++;im.onload=()=>{loaded++;prog.style.transform='scaleX('+(loaded/total)+')';if(loaded===total)prog.style.display='none'};im.src=src;return im}
F.forEach(f=>{load(f.c);if(f.t!==f.c)load(f.t)});
let cur=0,showTruth=false,diff=false,fps=8,timer=null,playing=false,loop=true,hold=null;
function fmt(v,d){return v==null?'—':Number(v).toFixed(d)}
function draw(){const f=F[cur];const a=load(f.c),b=load(f.t);
 const useT=showTruth&&!f.real;const im=useT?b:a;
 if(!im.complete||!im.naturalWidth){requestAnimationFrame(draw);return}
 if(diff&&!f.real&&b.complete&&b.naturalWidth){cx.drawImage(a,0,0);const A=cx.getImageData(0,0,W,H);cx.drawImage(b,0,0);const B=cx.getImageData(0,0,W,H);
  const o=cx.createImageData(W,H);for(let p=0;p<A.data.length;p+=4){const d=Math.min(255,4*(Math.abs(A.data[p]-B.data[p])+Math.abs(A.data[p+1]-B.data[p+1])+Math.abs(A.data[p+2]-B.data[p+2]))/3);o.data[p]=d;o.data[p+1]=Math.max(0,d-60);o.data[p+2]=Math.max(0,d-120);o.data[p+3]=255}cx.putImageData(o,0,0)}
 else cx.drawImage(im,0,0);
 badge.textContent=f.real?'REAL':(f.missing?'NOT CAPTURED — truth shown':(f.cut?'GENERATED ACROSS A CUT — not scored':'GENERATED'));badge.className=f.real?'real':'gen';
 cv.style.opacity=f.missing?'0.45':'1';
 idx.textContent='#'+f.i+'  t '+fmt(f.time_s,4)+' s'+(f.real?'':'  φ '+fmt(f.phase,2));
 mode.textContent=(diff&&!f.real?'|candidate − truth| ×4':useT?'TRUTH in place':'')+(playing?'  ▶':'');
 let h='';
 if(f.real)h='<b>real frame</b> <span class="k">source index '+f.i+' — the FG saw this one, '+RATES.source_interval_ms.toFixed(2)+' ms after the previous real</span>';
 else{h='<b>generated</b> between real <b>'+f.N+'</b> → <b>'+f.N1+'</b>, phase '+fmt(f.phase,3)+' <span class="k">('+(f.phase*RATES.source_interval_ms).toFixed(2)+' ms after real '+f.N+', presented '+RATES.output_interval_ms.toFixed(2)+' ms after the previous frame)</span>'+(f.cut?' — <span class="bad">the two real frames were not k apart (the loop seam): a CUT, no interpolation truth exists</span>':'');
  if(f.s){const s=f.s;h+='<br>pos <b>'+fmt(s.pos,3)+'</b> px · shape <b>'+fmt(s.shape,3)+'</b> px · halluc <b>'+fmt(s.halluc,0)+'</b> px² (lead '+fmt(s.lead,1)+') · missing <b>'+fmt(s.missing,0)+'</b> px² · sharp <b>'+fmt(s.sharp,3)+'</b>'+(s.sharp!=null&&s.sharp<0.9?' <span class="bad">BLUR</span>':'')+' · class-0 '+fmt(s.disocc_px,0)+' px'}}
 info.innerHTML=h;rate.textContent=fps+' fps auto · '+(loop?'loop':'stop at end');drawTL()}
function drawTL(){const w=tl.clientWidth,h=tl.clientHeight;if(tlc.width!==w||tlc.height!==h){tlc.width=w;tlc.height=h}
 tx.clearRect(0,0,w,h);const n=F.length;for(let i=0;i<n;i++){const x=(i+0.5)/n*w;const f=F[i];tx.fillStyle=f.real?getComputedStyle(document.documentElement).getPropertyValue('--real'):getComputedStyle(document.documentElement).getPropertyValue('--gen');
  const t=f.real?4:h*0.45;tx.fillRect(x-0.5,h-t,Math.max(1,w/n-0.5),t)}
 const x=(cur+0.5)/n*w;tx.fillStyle='#fff';tx.fillRect(x-1,0,2,h)}
function go(i){if(i<0)i=loop?F.length-1:0;if(i>=F.length){if(!loop){stop();i=F.length-1}else i=0}cur=i;draw()}
function step(d){go(cur+d)}
function startAuto(d){stopAuto();hold=d;timer=setInterval(()=>step(d),1000/fps)}
function stopAuto(){if(timer){clearInterval(timer);timer=null}hold=null}
function play(){playing=true;startAuto(1)}function stop(){playing=false;stopAuto();draw()}
document.addEventListener('keydown',e=>{
 if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();const d=e.key==='ArrowRight'?1:-1;if(e.repeat)return;step(d);if(!playing){hold=d;setTimeout(()=>{if(hold===d&&!timer)startAuto(d)},260)}return}
 if(e.key===' '){e.preventDefault();playing?stop():play();return}
 if(e.key==='t'||e.key==='T'){showTruth=!showTruth;draw()}
 if(e.key==='d'||e.key==='D'){diff=!diff;draw()}
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
    ap = argparse.ArgumentParser(description='scene_truth — frame stepper (real / generated, full frame)')
    ap.add_argument('--run', required=True, help='corpus dir')
    ap.add_argument('--k', type=int, default=4)
    ap.add_argument('--arm', default='oracle2', help='materialised arms/<name>/ or a synthetic arm')
    ap.add_argument('--out', required=True)
    ap.add_argument('--scores', help='the scorer --json, to show each generated frame\'s terms')
    ap.add_argument('--start', type=int, default=0)
    ap.add_argument('--count', type=int, default=0, help='0 = to the last real frame')
    ap.add_argument('--fg-log', help='the raw FG stdout of this run, for the MEASURED present and capture rates')
    a = ap.parse_args()
    build(a.run, a.k, a.arm, a.out, a.scores, a.start, a.count, a.fg_log)


if __name__ == '__main__':
    main()
