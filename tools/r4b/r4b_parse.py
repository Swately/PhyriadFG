# r4b_parse.py — the fresh-frame measurement from the r4b runs' CSVs and logs: per run — presents, fresh count and
# rate, re-shows, the warp batch's GPU time (p50/p95) and its submit->completion latency (p50/p95) over the fresh
# presents, lat EMA, rdrop/s from the stats; per configuration mean + spread (DI-3). Made with my soul - Swately <3
import csv, glob, os, re, sys, io, statistics
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'r4b')
def pct(a, q):
    if not a: return float('nan')
    a = sorted(a); return a[min(len(a) - 1, int(q * len(a)))]
mean = lambda a: statistics.fmean(a) if a else float('nan')
def read_log(path):   # UTF-8 (BOM) when the harness ran under bash; UTF-16 LE when it ran under the PowerShell host
    raw = open(path, 'rb').read()
    return raw.decode('utf-16') if raw[:2] == b'\xff\xfe' else raw.decode('utf-8', 'replace')
rows = []
for log in sorted(glob.glob(os.path.join(d, '*_run*.log'))):
    tag = os.path.basename(log)[:-4]
    txt = read_log(log)
    m = re.search(r'total_presents=(\d+)', txt); presents = int(m.group(1)) if m else -1
    stats_n = len(re.findall(r'fps \(present\)', txt))
    rdrop = sum(float(x) for x in re.findall(r'rdrop:([0-9.]+)/s', txt)) / max(1, stats_n)
    fresh_s = [float(x) for x in re.findall(r'fresh:([0-9.]+)/s', txt)]
    csvp = os.path.join(d, tag + '.csv')
    n = 0; fresh = 0; gpu = []; lat = []; addlat = []
    if os.path.exists(csvp):
        with open(csvp, encoding='utf-8', errors='replace') as f:
            rd = csv.reader(l for l in f if not l.startswith('#')); hdr = next(rd)
            i_f, i_g, i_l, i_a = hdr.index('phyriadfg_fresh'), hdr.index('phyriadfg_warp_gpu_ms'), hdr.index('phyriadfg_warp_lat_ms'), hdr.index('MsAddedLatency')
            for r in rd:
                n += 1
                try:
                    if r[i_f] == '1':
                        fresh += 1
                        if r[i_g]: gpu.append(float(r[i_g]))
                        if r[i_l]: lat.append(float(r[i_l]))
                    if r[i_a]: addlat.append(float(r[i_a]))
                except (ValueError, IndexError): pass
    rows.append(dict(tag=tag, presents=presents, n=n, fresh=fresh, fresh_frac=(fresh / n if n else float('nan')), fresh_s=mean(fresh_s), rdrop=rdrop,
                     gpu50=pct(gpu, 0.5), gpu95=pct(gpu, 0.95), lat50=pct(lat, 0.5), lat95=pct(lat, 0.95), latmax=(max(lat) if lat else float('nan')), addlat=mean(addlat)))
# the per-second evolution from the stats lines (fresh:N/s, sq:H/M, sub2fence, lat): first 10 windows vs the last 40
for log in sorted(glob.glob(os.path.join(d, '*_run*.log'))):
    tag = os.path.basename(log)[:-4]
    txt = read_log(log)
    wins = re.findall(r'fps \(present\)[^\n]*', txt)
    def col(w, pat):
        m = re.search(pat, w); return float(m.group(1)) if m else float('nan')
    fr = [col(w, r'fresh:([0-9.]+)/s') for w in wins]; hit = [col(w, r'sq:([0-9]+)H/') for w in wins]
    s2f = [col(w, r'sub2fence ([0-9.]+)ms') for w in wins]; lt = [col(w, r'lat ([0-9.]+)ms') for w in wins]
    if not wins: continue
    f10, fr40 = [v for v in fr[:10] if v == v], [v for v in fr[-40:] if v == v]
    print(f"{tag:<18} windows={len(wins):>3} | fresh/s first10={mean(f10):6.1f} last40={mean(fr40):6.1f} | sq hits/s first10={mean([v for v in hit[:10] if v==v]):6.1f} last40={mean([v for v in hit[-40:] if v==v]):6.1f} | sub2fence first10={mean([v for v in s2f[:10] if v==v]):5.2f} last40={mean([v for v in s2f[-40:] if v==v]):5.2f} | lat last40={mean([v for v in lt[-40:] if v==v]):5.1f}")
print()
print(f"{'run':<18}{'presents':>9}{'rows':>7}{'fresh':>7}{'fresh%':>8}{'fresh/s':>9}{'rdrop/s':>9}{'gpu p50':>9}{'gpu p95':>9}{'lat p50':>9}{'lat p95':>9}{'lat max':>9}{'MsAddedLat':>11}")
for r in rows:
    print(f"{r['tag']:<18}{r['presents']:>9}{r['n']:>7}{r['fresh']:>7}{100*r['fresh_frac']:>8.1f}{r['fresh_s']:>9.1f}{r['rdrop']:>9.1f}{r['gpu50']:>9.3f}{r['gpu95']:>9.3f}{r['lat50']:>9.2f}{r['lat95']:>9.2f}{r['latmax']:>9.2f}{r['addlat']:>11.2f}")
print()
for cfg in ('async', 'sync', 'sq_default', 'sq_wide'):
    rs = [r for r in rows if r['tag'].startswith(cfg + '_run')]
    if not rs: continue
    for key, name in (('fresh_s', 'fresh/s'), ('fresh_frac', 'fresh frac'), ('gpu50', 'gpu p50 ms'), ('lat50', 'lat p50 ms'), ('lat95', 'lat p95 ms'), ('addlat', 'MsAddedLat')):
        v = [r[key] for r in rs]
        print(f"{cfg:<11}{name:<12} mean={mean(v):.3f}" + (f" spread={max(v)-min(v):.3f} (n={len(v)})" if len(v) > 1 else " (n=1, reliability not measured)"))
