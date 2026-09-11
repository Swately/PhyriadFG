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

`--presented` walks a DIFFERENT sequence: not the base grid, but every frame the tap stored, in the
order the FG presented it. The two differ by far more than their order. The base-grid page shows one
frame per base index and marks every k-th REAL, loading the corpus's own source frame for it; the
aligner that feeds it files ONE frame per base index, so a looped capture -- the same corpus shown
twenty times -- collapses to a single lap and 95 % of what was on the screen never reaches the page.
It is also the only sequence in which a PACING defect is visible at all: every verdict term scores one
frame against the truth at that frame's own phase, so a frame emitted where a later one was due is
correct, and only the relation between consecutive presented frames is wrong (P-037).

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


def presented_rows(d, arm, qdump=None):
    """Every tick of a live capture, in presentation order, joined to what the aligner decoded from it.

    Three files, three different keys, and the joins are the part that is easy to get wrong -- both of
    them were got wrong once, on 2026-09-11, and both times the error looked like a finding:

      align.json      one row per triple, including the rows it marked `dup_of` and `skipped`. It is the
                      ONLY place the base indices N and N1 come from, because it is the only thing that
                      decoded the barcodes burnt into the two source frames.
      gdump_map.tsv   triple -> (seq, tick, pair). `seq` is the presentation order. `pair` is a RUNNING
                      COUNTER of pair presentations, NOT a corpus index: pair 66 is the 66th pair the FG
                      was shown, not base frame 528. Used as a base index it renders every truth at the
                      wrong scene time, which showed up as ~7 px of position error.
      <triple>_live   the frame itself, in the qdump.

    Grouping by (N, N1) alone is the other trap: a looped corpus shows the same pair once per lap, so all
    laps merge into one bucket and no pair appears to have fewer frames than the multiplier implies. The
    running counter separates the laps; the align row names the pair. Both keys are needed and they answer
    different questions.

    Returns (rows, qdump). Each row carries its scene POSITION in base frames -- N + t*(N1-N) -- which is
    the quantity the pacing statistics are computed over.
    """
    ap_ = os.path.join(d, 'arms', arm, 'align.json')
    if not os.path.exists(ap_):
        raise SystemExit('%s: no align.json. Run scene_align.py on the qdump first -- the presented '
                         'sequence needs the N, N1 and t it decodes, for every triple including the '
                         'ones it files as dup_of / skipped.' % ap_)
    blob = json.load(open(ap_, encoding='utf-8'))
    qd = qdump or blob.get('qdump')
    if not qd or not os.path.isdir(qd):
        # Every align.json written before 2026-09-11 predates the `qdump` key. The naming is a convention
        # the whole runs/ tree already follows -- arm `fg_k4_base` was captured into `qdump_k4_base` -- so
        # try it rather than make the operator re-run the aligner over twenty arms just to record a path
        # that is already implied. Verified against the directory listing before being relied on.
        guess = os.path.join(d, 'qdump_' + (arm[3:] if arm.startswith('fg_') else arm))
        if os.path.isdir(guess):
            qd = guess
    if not qd or not os.path.isdir(qd):
        raise SystemExit('the qdump this capture lives in is unknown (align.json carries no `qdump` key, '
                         'and --qdump was not given): the presented sequence reads every frame from there, '
                         'not from arms/, because the frames the aligner discarded have no file in arms/.')
    order = {}
    mp = os.path.join(qd, 'gdump_map.tsv')
    if os.path.exists(mp):
        for ln in open(mp, encoding='utf-8'):
            if ln.startswith('#'):
                continue
            c = ln.split()
            if len(c) >= 4:
                order[c[0]] = (int(c[1]), int(c[2]), int(c[3]))
    rows = []
    for r in blob['rows']:
        if 'error' in r or 'N' not in r or 'N1' not in r or 't' not in r:
            continue
        o = order.get(r['triple'])
        seq, tick, pas = o if o else (len(rows) + 1, None, None)
        filed = ('cut' if not r.get('pair_ok', True) else
                 'dup' if 'dup_of' in r else 'skipped' if 'skipped' in r else 'kept')
        rows.append({'triple': r['triple'], 'seq': seq, 'tick': tick, 'pass': pas,
                     'N': r['N'], 'N1': r['N1'], 't': r['t'], 'pair_ok': r.get('pair_ok', True),
                     'filed': filed, 'mid': r.get('mid'),
                     'pos': r['N'] + r['t'] * (r['N1'] - r['N'])})
    rows.sort(key=lambda r: r['seq'])
    return rows, qd


def complete_laps(rows, pairs_in_corpus, K):
    """The runs of pair presentations that walk the corpus end to end, as lists of `pass` ids.

    A lap is `pairs_in_corpus` consecutive pair presentations whose N1 is the next one's N. The LOOP SEAM
    breaks a lap and is never inside one: at the seam the player restarts, so the FG pairs the corpus's
    last real frame with its first and N1 - N is not k. Chaining on N1 == next N alone does not notice
    that -- the seam chains perfectly (232 -> 0, then 0 -> 8) and the laps come out straddling it.
    """
    seen, seq = {}, []
    for r in rows:
        if r['pass'] is None or r['pass'] in seen:
            continue
        seen[r['pass']] = (r['N'], r['N1'])
        seq.append(r['pass'])
    laps, run = [], []
    for p in seq:
        N, N1 = seen[p]
        if N1 - N != K:
            run = []
            continue
        if run and seen[run[-1]][1] != N:
            run = []
        run.append(p)
        if len(run) == pairs_in_corpus:
            laps.append(list(run)); run = []
    return laps


def slot_grid(phases, K):
    """How many phase slots the generator is actually aiming at, discovered from the capture.

    Not assumed: the prompt this page was written from said 7 slots at k = 8 because that is what the
    multiplier implies, and the capture says 8. The generator aims at S evenly spaced INTERIOR slots,
    (j+0.5)/S, so the test is whether the phases lock to such a grid. `|mean(exp(2*pi*i*S*phi))|` is 1
    when they sit exactly on it and 0 when they are spread; every multiple of the true S scores high
    too (a grid of 16 contains a grid of 8), so the answer is the SMALLEST S within 90 % of the best,
    never the argmax. Returns (S, lock) with lock in [0, 1] -- report it, because a capture whose
    phases do not lock has no slots to be missing.
    """
    ph = np.asarray(phases, float)
    Ss = np.arange(1, 4 * K + 1)
    R = np.array([abs(np.mean(np.exp(2j * np.pi * S * ph))) for S in Ss])
    S = int(min(x for x, r in zip(Ss, R) if r >= 0.9 * R.max()))
    return S, float(R[S - 1])


def pacing(frames, K, S=None):
    """The two things a per-frame term cannot see: which phase SLOTS the generator dropped, and how far
    the scene advanced between one presented frame and the next.

    Every verdict term scores one frame against the truth at THAT frame's phase, and at its own phase a
    frame emitted where a later one was due is correct. The defect lives in the relation between
    consecutive presented frames: a dropped slot makes the next frame advance two base frames instead of
    one, so the object moves twice its distance in one frame time (P-037).
    """
    ok = [f for f in frames if not f.get('cut')]
    if not ok:
        return None
    ph = np.array([f['phase'] for f in ok], float)
    # The grid is a property of the CAPTURE, not of whatever subset this page shows, so the caller passes
    # the S it measured over every aligned row. Measuring it on one lap of 213 frames returned 7 with a
    # lock of 0.40 where the 4340-phase capture returns 8 with 0.88: the first lap of a capture sits 0.26
    # base frames off the grid (a warm-up), and a subset that small cannot separate 7 from 8.
    if S is None:
        S, lock = slot_grid(ph, K)
    else:
        lock = float(abs(np.mean(np.exp(2j * np.pi * S * ph))))   # how well THIS subset sits on that grid
    centres = (np.arange(S) + 0.5) / S
    for f in ok:
        f['slot'] = int(min(int(f['phase'] * S), S - 1))
    bypair = {}
    for f in ok:
        bypair.setdefault(f.get('pass'), []).append(f)
    P = len(bypair)
    census = []
    for j in range(S):
        miss = sum(1 for p in bypair if not any(x['slot'] == j for x in bypair[p]))
        census.append({'j': j, 'phase': float(centres[j]),
                       'n': int(sum(1 for f in ok if f['slot'] == j)),
                       'missing': miss, 'pct': 100.0 * miss / P if P else 0.0})
    per = {p: len(v) for p, v in bypair.items()}
    # The advance, over the frames AS PRESENTED. A lap wrap (the player restarting the corpus) is a jump
    # of a whole pair or more backwards and is counted apart; anything smaller and negative is a genuine
    # reversal, which is the thing the k = 4 run had none of.
    adv, wraps, revs = [], 0, 0
    prev = None
    for f in frames:
        f['adv'] = None
        if f.get('cut') or 'pos' not in f:
            prev = None
            continue
        if prev is not None:
            dpos = f['pos'] - prev
            if dpos <= -K:
                wraps += 1
            else:
                f['adv'] = float(dpos); adv.append(dpos)
                if dpos < 0:
                    revs += 1
        prev = f['pos']
    a = np.array(adv, float)
    med = float(np.median(a)) if a.size else float('nan')
    dbl = [j for j, f in enumerate(frames) if f.get('adv') is not None and f['adv'] > 1.5 * med]
    return {'S': S, 'lock': lock,
            'resid_base': float(np.mean(np.abs(ph - (np.minimum((ph * S).astype(int), S - 1) + 0.5) / S)) * K),
            'slots': census, 'pairs': P,
            'per_pair': {int(c): sum(1 for p in per if per[p] == c) for c in sorted(set(per.values()))},
            'mean_per_pair': float(np.mean(list(per.values()))) if per else float('nan'),
            'on_real': int((ph >= 0.99).sum()),
            'steps': int(a.size), 'median_adv': med,
            'mean_adv': float(a.mean()) if a.size else float('nan'),
            'double': len(dbl), 'double_pct': 100.0 * len(dbl) / a.size if a.size else 0.0,
            'reversals': revs, 'wraps': wraps, 'double_at': dbl}


def pacing_text(pc, K):
    """The same numbers as printed lines, so the terminal and the page cannot disagree."""
    if not pc:
        return []
    L = ['', 'RITMO -- lo que ningun termino por cuadro puede ver',
         '  ranuras DESCUBIERTAS en la captura: %d  (el multiplicador k=%d implica %d)   enganche %.3f, '
         'residuo medio a la rejilla %.3f cuadros base' % (pc['S'], K, K - 1, pc['lock'], pc['resid_base']),
         '  %-7s %-9s %-9s %s' % ('ranura', 'fase', 'cuadros', 'pares sin ella')]
    for sl in pc['slots']:
        L.append('  %-7d %-9.4f %-9d %d  (%.0f %%)' % (sl['j'], sl['phase'], sl['n'], sl['missing'], sl['pct']))
    L += ['  presentaciones de par: %d   media %.3f cuadros por par' % (pc['pairs'], pc['mean_per_pair']),
          '  reparto: %s' % ', '.join('%d cuadros x%d pares' % (c, n) for c, n in pc['per_pair'].items()),
          '  cuadros en fase >= 0.99 (encima del propio cuadro real siguiente): %d' % pc['on_real'],
          '  avance de tiempo de escena entre cuadros presentados (cuadros base):',
          '    mediana %.4f   media %.4f   pasos %d   DOBLES %d (%.1f %%)   reversiones %d   vueltas del bucle %d'
          % (pc['median_adv'], pc['mean_adv'], pc['steps'], pc['double'], pc['double_pct'],
             pc['reversals'], pc['wraps'])]
    return L


_P = {}   # per-PROCESS presented-sequence state


def _pres_worker_init(d, K, arm, out, sil, qdump):
    global _P
    SR._worker_init(d, arm, sil)
    t = SR._W['t']
    _P = {'d': d, 'K': K, 'out': out, 'qdump': qdump, 'W': t['width'], 'H': t['height']}


def _pres_worker(row):
    """One presented frame -> its record and its PNGs. The candidate comes from the QDUMP, never from
    arms/: the frames the aligner discarded as duplicates or skipped were never written there, and they
    are most of what the operator watched. Scoring needs no base index -- N, N1 and the FG's own t are
    the whole input -- so a discarded frame is scored by exactly the same function as a kept one
    (scene_report.score_live)."""
    d, K, out, qd, W, H = (_P[k] for k in ('d', 'K', 'out', 'qdump', 'W', 'H'))
    t = SR._W['t']
    p = os.path.join(qd, row['triple'] + '_live.rgba')
    if not os.path.exists(p):
        return None
    cand = SR.load_rgb(p, W, H)
    i = row['seq']
    cpath = 'seq/f_%06d_c.png' % i
    SR.png(os.path.join(out, cpath), SR.u8(cand))
    rec = {'i': i, 'real': False, 'c': cpath, 't': cpath, 'missing': False, 'cut': not row['pair_ok'],
           'N': row['N'], 'N1': row['N1'], 'phase': row['t'], 'pass': row['pass'], 'tick': row['tick'],
           'filed': row['filed'], 'mid': row['mid'], 'pos': row['pos'],
           'time_s': row['pos'] / float(t['base_fps'])}
    if not row['pair_ok']:
        return rec                     # a cut bridges two frames that are not k apart: no interpolation truth
    r, near_i, truth, mk = SR.score_live(cand, row['N'], row['N1'], row['t'],
                                         barcode=row['N'], want_masks=True)
    tpath = 'seq/f_%06d_t.png' % i
    SR.png(os.path.join(out, tpath), SR.u8(truth))
    rec['t'] = tpath
    opath = 'seq/f_%06d_o.png' % i
    hp, mpx = overlay_png(os.path.join(out, opath), mk, W, H)
    rec['o'] = opath
    rec['ov'] = {'halluc_px': hp, 'missing_px': mpx}
    ipath = 'seq/f_%06d_i.png' % i
    st = interior_png(os.path.join(out, ipath), cand, truth, mk['truth_obj'])
    if st:
        rec['ii'] = ipath
        rec['iv'] = {'rms': st[0], 'p99': st[1], 'max': st[2]}
    ob = r['objects']
    m = lambda key: float(np.nanmean([ob[k][key] for k in ob])) if ob else None
    rec['s'] = {'pos': m('pos_err'), 'shape': m('shape_err'), 'halluc': m('halluc_px'),
                'lead': m('lead_px'), 'missing': m('missing_px'), 'sharp': r['sharp'],
                'disocc_px': r['disocc_px']}
    return rec


def build_presented(d, K, arm, out, qdump, laps, max_frames, fg_log=None, sil='coverage', jobs=1):
    """The page of the sequence AS PRESENTED. No frame here is real: at k > 1 every tick the tap recorded
    is a synthesised frame, and the corpus's own source frames -- exact by construction -- never appear."""
    t, sc = SR.load_corpus(d)
    W, H, base = t['width'], t['height'], float(t['base_fps'])
    rows, qd = presented_rows(d, arm, qdump)
    all_rows = rows                      # kept whole: the phase grid is measured over the CAPTURE, not
    pairs_in_corpus = (t['frames'] - 1) // K   # over the subset a --laps / --max-frames choice leaves
    print('%d ticks stored, %d pair presentations, %d distinct scene positions possible (%d pairs x slots)'
          % (len(rows), len({r['pass'] for r in rows}), pairs_in_corpus * (K - 1), pairs_in_corpus))
    if laps != 'all':
        want = int(laps)
        L = complete_laps(rows, pairs_in_corpus, K)
        if not L:
            raise SystemExit('no complete lap of the corpus in this capture; use --laps all')
        keep = set()
        for lap in L[:want]:
            keep |= set(lap)
        dropped = len(rows) - sum(1 for r in rows if r['pass'] in keep)
        rows = [r for r in rows if r['pass'] in keep]
        print('--laps %s: %d complete lap(s) of %d available, %d frames kept, %d DROPPED'
              % (laps, min(want, len(L)), len(L), len(rows), dropped))
    if max_frames and len(rows) > max_frames:
        print('--max-frames %d: %d frames DROPPED from the end of the sequence' % (max_frames, len(rows) - max_frames))
        rows = rows[:max_frames]
    os.makedirs(os.path.join(out, 'seq'), exist_ok=True)
    if jobs <= 1 or len(rows) < 2:
        _pres_worker_init(d, K, arm, out, sil, qd)
        recs = [_pres_worker(r) for r in rows]
    else:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=min(jobs, len(rows)), initializer=_pres_worker_init,
                                 initargs=(d, K, arm, out, sil, qd)) as pool:
            recs = list(pool.map(_pres_worker, rows, chunksize=2))
    frames = [r for r in recs if r is not None]
    if len(frames) != len(rows):
        print('WARNING: %d of %d rows had no .rgba in the qdump and are NOT on the page'
              % (len(rows) - len(frames), len(rows)))
    S, lock_all = slot_grid([r['t'] for r in all_rows if r['pair_ok']], K)
    print('rejilla de fase descubierta sobre las %d filas alineadas: %d ranuras, enganche %.3f'
          % (sum(1 for r in all_rows if r['pair_ok']), S, lock_all))
    pc = pacing(frames, K, S)
    label = '%s | SECUENCIA PRESENTADA' % arm
    rates = {'base_fps': base, 'source_fps': base / K, 'output_fps': base, 'k': K,
             'source_interval_ms': 1000.0 * K / base, 'output_interval_ms': 1000.0 / base,
             'measured': measured_rates(fg_log)}
    meta = {'W': W, 'H': H, 'K': K, 'arm': label, 'corpus': os.path.basename(os.path.normpath(d)),
            'rates': rates, 'overlay': True, 'silhouette': sil, 'presented': True, 'pace': pc,
            'qdump': qd, 'stored_ticks': len(rows)}
    json.dump({'meta': meta, 'frames': frames}, open(os.path.join(out, 'frames.json'), 'w', encoding='utf-8'))
    write_html(out, frames, W, H, K, label, meta['corpus'], rates, True, pc)
    print('\n%d presented frames (%d con las mascaras del puntuador, %d cortes) -> %s'
          % (len(frames), sum('o' in f for f in frames), sum(f['cut'] for f in frames),
             os.path.join(out, 'index.html')))
    for ln in pacing_text(pc, K):
        print(ln)


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
    write_html(out, frames, W, H, K, arm, os.path.basename(os.path.normpath(d)), rates, overlay, None)
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
    if key == 'advance':
        return f.get('adv')
    return (f.get('s') or {}).get(key)


def worst_lists(frames, n=10):
    """The frames a reviewer should look at first, ranked by the terms that name an artefact: the most
    hallucinated mass, and the largest position error. Scored generated frames only; a cut carries no score."""
    scored = [(j, f) for j, f in enumerate(frames) if f.get('s')]
    def top(key):
        v = [(j, worst_value(f, key)) for j, f in scored if worst_value(f, key) is not None]
        return [j for j, _ in sorted(v, key=lambda p: -p[1])[:n]]
    return {'halluc': top('halluc'), 'pos': top('pos'), 'interior': top('interior')}


def pace_block(pc, frames, K):
    """The pacing panel: the discovered slot grid with the pairs that dropped each slot, the advance
    between consecutive presented frames, and a chip per double step so the reviewer lands on the frame
    where the jump happens instead of hunting for it."""
    if not pc:
        return ''
    tr = ''.join('<tr><td>%d</td><td>%.4f</td><td>%d</td><td>%s</td></tr>'
                 % (s['j'], s['phase'], s['n'], ('<b class="bad">%d (%.0f %%)</b>' % (s['missing'], s['pct']))
                    if s['missing'] else '0')
                 for s in pc['slots'])
    chips = ''.join('<button data-j="%d">#%d <span class="k">x%.2f</span></button>'
                    % (j, frames[j]['i'], frames[j]['adv']) for j in pc['double_at'][:80])
    more = ('' if len(pc['double_at']) <= 80 else
            ' <span class="k">(los primeros 80 de %d; la tecla J recorre todos)</span>' % len(pc['double_at']))
    worst_slot = max(pc['slots'], key=lambda x: x['missing'])
    return ('<div id="pace"><details><summary><b>ritmo</b> · <b>%d</b> ranuras · la ranura %.4f falta en '
            '<b class="bad">%d de %d pares (%.0f %%)</b> · <b class="bad">%d pasos dobles</b> de %d (%.1f %%) · '
            '%d reversiones <span class="k">&mdash; abrir para el censo completo</span></summary>'
            '<div class="ps"><span class="k">lo que ningun termino por cuadro puede ver: el cuadro esta bien para '
            'SU fase, y aun asi el salto desde el anterior es doble. Rejilla descubierta sobre la captura, no '
            'supuesta (el multiplicador k=%d implica %d): enganche %.3f, residuo medio %.3f cuadros base.</span></div>'
            '<table><tr><th>ranura</th><th>fase</th><th>cuadros</th><th>pares sin ella</th></tr>%s</table>'
            '<div class="ps"><b>%d</b> presentaciones de par · media <b>%.3f</b> cuadros por par · '
            '%d cuadros en fase &ge; 0.99 <span class="k">(encima del propio cuadro real siguiente)</span> · '
            'avance mediano <b>%.4f</b> cuadros base · %d vueltas del bucle</div>'
            '%s</details></div>'
            % (pc['S'], worst_slot['phase'], worst_slot['missing'], pc['pairs'], worst_slot['pct'],
               pc['double'], pc['steps'], pc['double_pct'], pc['reversals'],
               K, K - 1, pc['lock'], pc['resid_base'], tr,
               pc['pairs'], pc['mean_per_pair'], pc['on_real'], pc['median_adv'], pc['wraps'],
               ('<div class="ps chips"><span class="k">saltar al salto (tecla J):</span> ' + chips + more + '</div>')
               if chips else ''))


def write_html(out, frames, W, H, K, arm, corpus, rates, overlay=False, pace=None):
    css = """
:root{--bg:#f6f5f2;--ink:#1c1b19;--mut:#6b675f;--line:#dcd8d0;--real:#2f8f5e;--gen:#c98a1a;--red:#b03a2e}
@media(prefers-color-scheme:dark){:root{--bg:#121210;--ink:#e8e4da;--mut:#9b968a;--line:#2d2b25;--real:#5fd39a;--gen:#f0b545;--red:#e0705f}}
html,body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;height:100%}
#wrap{display:grid;grid-template-rows:auto 1fr auto auto;height:100vh}
header{display:flex;gap:18px;align-items:baseline;padding:10px 18px;border-bottom:1px solid var(--line)}
header h1{margin:0;font-size:16px;font-weight:600} header .sub{color:var(--mut);font-size:12px}
#stage{position:relative;display:flex;align-items:center;justify-content:center;background:#000;overflow:hidden;min-height:42vh}
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
#worst{display:flex;flex-wrap:wrap;gap:6px;align-items:center;padding:0 18px 8px;font-size:12px;max-height:74px;overflow-y:auto}
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
#pace{padding:0 18px 10px;font-size:12px}#pace table{border-collapse:collapse;margin:4px 0}
#pace th,#pace td{padding:1px 14px 1px 0;text-align:left;font-weight:400}#pace th{color:var(--mut);font-size:11px}
#pace .ps{margin-top:4px}#pace .chips{max-height:58px;overflow-y:auto}
#pace summary{cursor:pointer;list-style:none}#pace summary::-webkit-details-marker{display:none}
#pace summary:before{content:'\25b8 ';color:var(--mut)}#pace details[open] summary:before{content:'\25be '}#pace button{font:inherit;cursor:pointer;border:1px solid var(--line);background:transparent;color:var(--ink);border-radius:5px;padding:2px 8px;margin-right:4px}
#pace button:hover{border-color:var(--red)}#pace button.on{background:var(--red);color:#fff;border-color:var(--red)}
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
             '<span class="sub">%d frames · %d×%d · %s</span></header>'
             % (len(frames), W, H, ('ninguno es real: cada uno es un cuadro sintetizado que el FG presentó' if pace else 'real every %d' % K)),
             '<div id="stage"><canvas id="cv" width="%d" height="%d"></canvas><div id="badge"></div><div id="idx"></div><div id="mode"></div><div id="flag">● marcado</div><div id="prog"></div></div>' % (W, H),
             '<div id="panel"><div id="info"></div><div id="rate" class="k"></div></div>',
             '<div id="tl"><canvas id="tlc"></canvas></div>',
             worstline,
             pace_block(pace, frames, K),
             '<div id="fb"><button id="mk" title="M">● marcar</button>'
             '<input id="note" placeholder="qué ve en este cuadro (se guarda solo; N para escribir, Esc para salir)" autocomplete="off">'
             '<span id="fbn"></span><button id="cp" title="C">copiar retroalimentación</button>'
             '<button id="cl">borrar todo</button></div>',
             '<div id="help">← → step · <b>hold</b> to auto-advance · [ ] slower/faster · space play · L loop · T truth in place · D difference%s · '
             '<b>M</b> marcar · <b>N</b> nota · <b>C</b> copiar · <b>J</b> siguiente salto doble · <b>,</b> <b>.</b> par anterior/siguiente · '
             'PgUp/PgDn ±50 · Home/End · click the timeline</div></div>'
             % (' · <b>H</b> masa alucinada / faltante · <b>I</b> error DENTRO de la silueta · <b>W</b> peor cuadro' if overlay else ''),
             '<script>const F=%s;const W=%d,H=%d,K=%d;const RATES=%s;const WORST=%s;const HASOV=%s;const PAGE=%s;const PACE=%s;'
             % (data, W, H, K, json.dumps({k: v for k, v in rates.items() if k != 'measured'}),
                json.dumps(worst_lists(frames)), 'true' if overlay else 'false',
                json.dumps('%s k=%d arm %s' % (corpus, K, arm)),
                json.dumps(pace or None)),
             r"""
const cv=document.getElementById('cv'),cx=cv.getContext('2d'),badge=document.getElementById('badge'),idx=document.getElementById('idx'),
 mode=document.getElementById('mode'),info=document.getElementById('info'),rate=document.getElementById('rate'),prog=document.getElementById('prog'),
 tl=document.getElementById('tl'),tlc=document.getElementById('tlc'),tx=tlc.getContext('2d');
/* The page used to decode EVERY frame up front. That is fine for the ~200 frames a base-grid page carries
   and impossible for the presented sequence, which is every tick the tap stored -- thousands of frames, up to
   four PNGs each. Images are now fetched in a window around the cursor and dropped outside a wider one, so the
   browser holds a bounded number of decoded frames however long the sequence is. */
const IMG={};const WIN=40;let loaded=0,total=0;
const BLANK='data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';  /* pointing a dropped <img> at one transparent pixel releases its decoded
   bitmap; pointing it at '' would make the browser fetch the document itself instead */
function load(src){let im=IMG[src];if(im)return im;im=new Image();IMG[src]=im;total++;
 im.onload=()=>{if(im.dropped)return;loaded++;prog.style.transform='scaleX('+Math.min(1,loaded/total)+')';
  if(loaded>=total)prog.style.opacity='0';
  /* the frame the cursor is on just arrived: repaint. This used to be a requestAnimationFrame retry loop
     inside draw(), which polls and, in a pane that is not compositing, never fires at all -- the picture
     and the panel then hold the last frame that happened to be decoded in time. An onload is the event
     that was being polled for. */
  const f=F[cur];if(f&&(src===f.c||src===f.t||src===f.o||src===f.ii))draw()};
 im.onerror=()=>{if(!im.dropped){im.broken=true;loaded++}};im.src=src;return im}
function srcsIn(lo,hi){const s=[];for(let j=Math.max(0,lo);j<=Math.min(F.length-1,hi);j++){const f=F[j];
 s.push(f.c);if(f.t!==f.c)s.push(f.t);if(f.o)s.push(f.o);if(f.ii)s.push(f.ii)}return s}
function prefetch(){prog.style.opacity='1';srcsIn(cur-WIN,cur+WIN).forEach(load);
 const keep=new Set(srcsIn(cur-3*WIN,cur+3*WIN));
 for(const k in IMG){if(!keep.has(k)){IMG[k].dropped=true;IMG[k].src=BLANK;delete IMG[k]}}}
let cur=0,showTruth=false,diff=false,ovOn=false,inOn=false,fps=8,timer=null,playing=false,loop=true,hold=null;
const WFLAT=[].concat(WORST.halluc||[],WORST.pos||[],WORST.interior||[]).filter((v,i,a)=>a.indexOf(v)===i);let wi=-1;
const MEDADV=PACE&&PACE.median_adv?PACE.median_adv:1;
const DBL=(PACE&&PACE.double_at)||[];let di=-1;
function fmt(v,d){return v==null?'—':Number(v).toFixed(d)}
function draw(){const f=F[cur];const a=load(f.c),b=load(f.t);
 const useT=showTruth&&!f.real;const im=useT?b:a;
 if(!im.complete||!im.naturalWidth)return;   /* its onload will call draw() back */
 if(diff&&!f.real&&b.complete&&b.naturalWidth){cx.drawImage(a,0,0);const A=cx.getImageData(0,0,W,H);cx.drawImage(b,0,0);const B=cx.getImageData(0,0,W,H);
  const o=cx.createImageData(W,H);for(let p=0;p<A.data.length;p+=4){const d=Math.min(255,4*(Math.abs(A.data[p]-B.data[p])+Math.abs(A.data[p+1]-B.data[p+1])+Math.abs(A.data[p+2]-B.data[p+2]))/3);o.data[p]=d;o.data[p+1]=Math.max(0,d-60);o.data[p+2]=Math.max(0,d-120);o.data[p+3]=255}cx.putImageData(o,0,0)}
 else cx.drawImage(im,0,0);
 if(inOn&&f.ii&&!useT){const iv=load(f.ii);if(iv.complete&&iv.naturalWidth)cx.drawImage(iv,0,0)}
 if(ovOn&&f.o&&!useT){const ov=load(f.o);if(ov.complete&&ov.naturalWidth)cx.drawImage(ov,0,0)}
 badge.textContent=f.real?'REAL':(f.missing?'NOT CAPTURED — truth shown':(f.cut?'GENERATED ACROSS A CUT — not scored':'GENERATED'));badge.className=f.real?'real':'gen';
 cv.style.opacity=f.missing?'0.45':'1';
 idx.textContent='#'+f.i+'  t '+fmt(f.time_s,4)+' s'+(f.real?'':'  φ '+fmt(f.phase,2))
  +(f.adv!=null&&f.adv>1.5*MEDADV?'  ➔➔ SALTO x'+f.adv.toFixed(2):'');
 mode.textContent=(diff&&!f.real?'|candidate − truth| ×4':useT?'TRUTH in place':'')+(inOn&&f.ii&&!useT?' + interior de la silueta':'')+(ovOn&&f.o&&!useT?' + halluc / missing':'')+(playing?'  ▶':'');
 let h='';
 if(f.real)h='<b>real frame</b> <span class="k">source index '+f.i+' — the FG saw this one, '+RATES.source_interval_ms.toFixed(2)+' ms after the previous real</span>';
 else{h='<b>generated</b> between real <b>'+f.N+'</b> → <b>'+f.N1+'</b>, phase '+fmt(f.phase,3)+' <span class="k">('+(f.phase*RATES.source_interval_ms).toFixed(2)+' ms after real '+f.N+', presented '+RATES.output_interval_ms.toFixed(2)+' ms after the previous frame)</span>'+(f.cut?' — <span class="bad">the two real frames were not k apart (the loop seam): a CUT, no interpolation truth exists</span>':'');
  if(f.pass!=null)h+=' <span class="k">· presentacion de par '+f.pass+', posicion de escena '+fmt(f.pos,3)+' cuadros base'
   +(f.filed&&f.filed!=='kept'?', el alineador lo archivo como <b>'+f.filed+'</b> (no estaba en la pagina anterior)':'')+'</span>';
  if(f.adv!=null)h+='<br><span class="k">avance desde el cuadro presentado anterior:</span> <b'+(f.adv>1.5*MEDADV?' class="bad"':'')+'>'
   +fmt(f.adv,3)+'</b> cuadros base'+(f.adv>1.5*MEDADV?' <span class="bad">— una ranura de fase se perdio; el objeto salta el doble</span>':'');
  if(f.s){const s=f.s;h+='<br><span class="k">media por objeto:</span> pos <b>'+fmt(s.pos,3)+'</b> px · shape <b>'+fmt(s.shape,3)+'</b> px · halluc <b>'+fmt(s.halluc,0)+'</b> px² (lead '+fmt(s.lead,1)+') · missing <b>'+fmt(s.missing,0)+'</b> px² · sharp <b>'+fmt(s.sharp,3)+'</b>'+(s.sharp!=null&&s.sharp<0.9?' <span class="bad">BLUR</span>':'')+' · class-0 '+fmt(s.disocc_px,0)+' px';
   if(f.ov)h+='<br><span class="k">lo que está dibujado (unión de todos los objetos):</span> <span style="color:#e04030">halluc <b>'+f.ov.halluc_px+'</b> px²</span> · <span style="color:#3878e8">missing <b>'+f.ov.missing_px+'</b> px²</span>';
   if(f.iv)h+=' <span class="k">· interior de la silueta (NO entra en ningún término): rms <b>'+fmt(f.iv.rms,4)+'</b> · p99 <b>'+fmt(f.iv.p99,4)+'</b> · máx <b>'+fmt(f.iv.max,3)+'</b> (tecla I)</span>'}}
 info.innerHTML=h;rate.textContent=fps+' fps auto · '+(loop?'loop':'stop at end');fbSync()}
function drawTL(){const w=tl.clientWidth,h=tl.clientHeight;if(tlc.width!==w||tlc.height!==h){tlc.width=w;tlc.height=h}
 tx.clearRect(0,0,w,h);const n=F.length;for(let i=0;i<n;i++){const x=(i+0.5)/n*w;const f=F[i];tx.fillStyle=f.real?getComputedStyle(document.documentElement).getPropertyValue('--real'):getComputedStyle(document.documentElement).getPropertyValue('--gen');
  const t=f.real?4:h*0.45;tx.fillRect(x-0.5,h-t,Math.max(1,w/n-0.5),t)}
 if(DBL.length){tx.fillStyle=getComputedStyle(document.documentElement).getPropertyValue('--red');
  DBL.forEach(j=>{const x=(j+0.5)/n*w;tx.fillRect(x-0.5,h*0.45,Math.max(1,w/n),h*0.2)})}
 if(typeof FB!=='undefined'){const red=getComputedStyle(document.documentElement).getPropertyValue('--red');
  for(let i=0;i<n;i++){if(FB[F[i].i]){const x=(i+0.5)/n*w;tx.fillStyle=red;tx.fillRect(x-1,0,Math.max(2,w/n),h*0.4)}}}
 const x=(cur+0.5)/n*w;tx.fillStyle='#fff';tx.fillRect(x-1,0,2,h)}
function go(i){if(i<0)i=loop?F.length-1:0;if(i>=F.length){if(!loop){stop();i=F.length-1}else i=0}cur=i;prefetch();draw()}
function jumpPair(d){const p=F[cur].pass;if(p==null){go(cur+d*10);return}
 let j=cur;while(j+d>=0&&j+d<F.length&&F[j+d].pass===p)j+=d;go(Math.max(0,Math.min(F.length-1,j+d)))}
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
  if(f.pos!=null)ln+='  pos '+fmt(f.pos,2)+(f.pass!=null?'  par '+f.pass:'')+(f.filed&&f.filed!=='kept'?'  ['+f.filed+']':'');
  if(f.adv!=null)ln+='  avance '+fmt(f.adv,3)+(f.adv>1.5*MEDADV?' SALTO':'');
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
 if(e.key==='j'||e.key==='J'){if(DBL.length){di=(di+1)%DBL.length;go(DBL[di])}}
 if(e.key===','){jumpPair(-1)}if(e.key==='.'){jumpPair(1)}
 if(e.key==='PageUp'){e.preventDefault();go(cur-50)}if(e.key==='PageDown'){e.preventDefault();go(cur+50)}
 if(e.key==='l'||e.key==='L'){loop=!loop;draw()}
 if(e.key===']'){fps=Math.min(60,Math.round(fps*1.5));draw();if(timer)startAuto(hold||1)}
 if(e.key==='['){fps=Math.max(1,Math.round(fps/1.5));draw();if(timer)startAuto(hold||1)}
 if(e.key==='Home'){go(0)}if(e.key==='End'){go(F.length-1)}});
document.addEventListener('keyup',e=>{if((e.key==='ArrowRight'||e.key==='ArrowLeft')&&!playing){stopAuto()}});
tl.addEventListener('click',e=>{const r=tl.getBoundingClientRect();go(Math.floor((e.clientX-r.left)/r.width*F.length))});
document.querySelectorAll('#worst button,#pace button').forEach(b=>b.addEventListener('click',()=>{
 const j=+b.dataset.j;if(HASOV)ovOn=true;go(j);
 document.querySelectorAll('#worst button,#pace button').forEach(x=>x.classList.toggle('on',+x.dataset.j===j))}));
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
    ap.add_argument('--presented', action='store_true',
                    help='build the page from the PRESENTED SEQUENCE -- every frame the tap stored, in the '
                         'order it went to the panel -- instead of the base grid. Nothing is deduplicated and '
                         'nothing is REAL: at k > 1 every stored tick is a synthesised frame. Needs a live arm '
                         'whose align.json exists (scene_align.py) and the qdump the frames live in')
    ap.add_argument('--qdump', help='where the presented frames are; default: the `qdump` key align.json carries')
    ap.add_argument('--laps', default='all',
                    help="--presented only: 'all' (every stored tick, the default) or an integer number of "
                         'COMPLETE laps of the corpus to keep. What a narrower choice drops is printed, never silent')
    ap.add_argument('--max-frames', type=int, default=0, dest='max_frames',
                    help='--presented only: a hard cap on the page, applied last; what it drops is printed')
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 2),
                    help='processes over the frames. The truth render is 72 %% of a page and is independent per '
                         'frame, so this is the same pooling scene_report.py uses; 1 = serial, same code path')
    a = ap.parse_args()
    if a.jobs > 1:
        # one BLAS/OpenMP thread per worker: the parallelism is across frames, never inside one. The same
        # line scene_report.main has carried since the pool was added; this file grew a pool later and did
        # not get it.
        for v in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS'):
            os.environ.setdefault(v, '1')
    if a.rehtml:
        blob = json.load(open(os.path.join(a.out, 'frames.json'), encoding='utf-8'))
        m = blob['meta']
        write_html(a.out, blob['frames'], m['W'], m['H'], m['K'], m['arm'], m['corpus'], m['rates'],
                   m['overlay'], m.get('pace'))
        print('index.html rewritten from frames.json (%d frames) -> %s' % (len(blob['frames']), os.path.join(a.out, 'index.html')))
        return
    if a.presented:
        build_presented(a.run, a.k, a.arm, a.out, a.qdump, a.laps, a.max_frames, a.fg_log, a.silhouette, a.jobs)
        return
    build(a.run, a.k, a.arm, a.out, a.scores, a.start, a.count, a.fg_log, a.overlay, a.silhouette, a.jobs)


if __name__ == '__main__':
    main()
