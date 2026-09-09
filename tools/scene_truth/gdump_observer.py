# gdump_observer.py - the G1/G2 summary of docs/planning/GDUMP_PLAN.md over the logs gdump_observer.ps1 kept.
#
#   python gdump_observer.py --dir <run>/observer_k4 --repeats 2 [--md out.md]
#
# From each <arm>_r<i>.log it takes the per-second "[ra] X fps (present) | ... | warp X ms | ... fresh:X/s
# [rdrop:X/s] ... wsub(up:X rec:X gpu:X prs:X) ... wt(...)" lines (the first 3 s are warm-up and dropped),
# and the exit line "done (real=... interp=... total_presents=...)". Per arm and per number it reports the
# mean of each repeat, the run-to-run r over the per-second series (DI-3: that r IS the number's
# reliability; below 0.5 the per-second value is not usable and only the mean is quoted), and the delta
# of each arm against `base`. A number that never appears in a log is reported as "not in log", never as 0.
# Made with my soul - Swately <3
import argparse, glob, io, math, os, re, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

STATS = re.compile(r'\[ra\] ([\d.]+) fps \(present\)')
FIELDS = {
    'present_fps': re.compile(r'\[ra\] ([\d.]+) fps \(present\)'),
    'warp_ms':     re.compile(r'warp ([\d.]+)ms'),
    'iter_ms':     re.compile(r'iter ([\d.]+)/'),
    'lat_ms':      re.compile(r'lat ([\d.]+)ms'),
    'fresh_s':     re.compile(r'fresh:([\d.]+)/s'),
    'rdrop_s':     re.compile(r'rdrop:([\d.]+)/s'),
    'wsub_up':     re.compile(r'wsub\(up:([\d.]+)'),
    'wsub_gpu':    re.compile(r'wsub\(up:[\d.]+ rec:[\d.]+ gpu:([\d.]+)'),
    'wsub_prs':    re.compile(r'prs:([\d.]+)\)'),
    'wt_gpu_ms':   re.compile(r' gpu ([\d.]+)ms'),            # --warp-timing (present_stage.cpp timing_line): " gpu %.2fms sub2fence %.2fms"
    'wt_lat_ms':   re.compile(r'sub2fence ([\d.]+)ms'),
    'gpu_a_pct':   re.compile(r'gpu\(A:(\d+)%'),
}
DONE = re.compile(r'done \(real=(\d+) interp=(\d+) total_presents=(\d+)\)')


def series(path, warm=3):
    per = {k: [] for k in FIELDS}
    done = None
    n = 0
    for ln in open(path, encoding='utf-8', errors='replace'):
        if STATS.search(ln):
            n += 1
            if n <= warm:
                continue
            for k, rx in FIELDS.items():
                m = rx.search(ln)
                if m:
                    per[k].append(float(m.group(1)))
                elif k == 'rdrop_s' and 'fresh:' in ln:
                    per[k].append(0.0)   # the FG prints rdrop only when > 0.5/s; a fresh-only line means ~0
        m = DONE.search(ln)
        if m:
            done = tuple(int(x) for x in m.groups())
    return per, done


def mean(v):
    return sum(v) / len(v) if v else float('nan')


def pearson(a, b):
    n = min(len(a), len(b))
    if n < 3:
        return float('nan')
    a, b = a[:n], b[:n]
    ma, mb = mean(a), mean(b)
    sa = math.sqrt(sum((x - ma) ** 2 for x in a)); sb = math.sqrt(sum((y - mb) ** 2 for y in b))
    if sa == 0 or sb == 0:
        return float('nan')
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / (sa * sb)


def fmt(x, nd=2):
    return 'nan' if x != x else ('%.*f' % (nd, x))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', required=True)
    ap.add_argument('--repeats', type=int, default=2)
    ap.add_argument('--md', default=None)
    a = ap.parse_args()
    arms = ['base', 'off', 'nocopy', 'gdump']
    rows = {}
    for arm in arms:
        logs = sorted(glob.glob(os.path.join(a.dir, arm + '_r*.log')))
        if not logs:
            continue
        runs = [series(p) for p in logs]
        rows[arm] = runs
    out = []
    out.append('# gdump observer — %s (%d repeats)\n' % (a.dir, a.repeats))
    out.append('| arm | present fps | warp ms | fresh/s | rdrop/s | wsub up | wsub gpu | wt gpu ms | wt lat ms | gpu A % | presents (exit line) |')
    out.append('|---|---|---|---|---|---|---|---|---|---|---|')
    keys = ['present_fps', 'warp_ms', 'fresh_s', 'rdrop_s', 'wsub_up', 'wsub_gpu', 'wt_gpu_ms', 'wt_lat_ms', 'gpu_a_pct']
    means = {}
    for arm in arms:
        if arm not in rows:
            continue
        runs = rows[arm]
        cells = []
        means[arm] = {}
        for k in keys:
            vals = [mean(r[0][k]) for r in runs if r[0][k]]
            if not vals:
                cells.append('not in log'); means[arm][k] = float('nan'); continue
            rr = pearson(runs[0][0][k], runs[1][0][k]) if len(runs) >= 2 else float('nan')
            means[arm][k] = mean(vals)
            cells.append('%s (r %s; %s)' % (fmt(mean(vals)), fmt(rr, 2), ' / '.join(fmt(v) for v in vals)))
        dones = [r[1] for r in runs if r[1]]
        cells.append(' / '.join('%d' % d[2] for d in dones) if dones else 'no exit line')
        out.append('| %s | %s |' % (arm, ' | '.join(cells)))
    if 'base' in means:
        out.append('\nDeltas vs base (mean − base mean):')
        for arm in ['off', 'nocopy', 'gdump']:
            if arm in means:
                out.append('- %s: ' % arm + ', '.join('%s %+s' % (k, fmt(means[arm][k] - means['base'][k])) for k in keys if means[arm][k] == means[arm][k] and means['base'][k] == means['base'][k]))
    out.append('\nReading rule (DI-3): a per-number r ≥ 0.7 makes the per-second series usable; below 0.5 only the run means are quoted, and the delta must exceed the spread between the two repeats to mean anything. `not in log` = the FG did not print that field (a flag missing), never 0.')
    txt = '\n'.join(out)
    print(txt)
    if a.md:
        open(a.md, 'w', encoding='utf-8').write(txt + '\n')


if __name__ == '__main__':
    main()
