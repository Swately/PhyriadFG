#!/usr/bin/env python3
"""check_flag_roundtrip.py — the flag-surface round-trip instrument (CONVERGENCE strategy X2, risk XR6/DR2).

Extracts every `--token` the hand parser matches in src/cli/cli.cpp (exact-match string literals),
launches the FG once per token with `--dump-config` (parse -> resolve -> print -> exit; no device is
created), records per token: the parse status (ok / needs-arg / unknown / parity-fail / other), the
value used, and the `[old]` / `[new]` / `[hash]` dump lines. Writes a deterministic record file.

  python tools/check_flag_roundtrip.py --exe build-release/phyriad_fg.exe --out roundtrip.txt
  python tools/check_flag_roundtrip.py --compare a.txt b.txt        # diff two records (token by token)

R0 use: the record of the R0 binary is the BASELINE that R3 (which deletes the hand-written layer
cases) must reproduce token-for-token. A `parity-fail` status on ANY token fails the gate today.
stdlib only (no numpy). Made with my soul - Swately <3
"""
import argparse, os, re, subprocess, sys

VALUE_GUESSES = ["1", "0.5", "auto", "x"]   # tried in order when a token reports "needs arg"

def extract_tokens(cli_cpp):
    src = open(cli_cpp, encoding="utf-8", errors="replace").read()
    toks = set(re.findall(r'strcmp\s*\(\s*\w+\s*,\s*"(--[a-z0-9][a-z0-9-]*)"\s*\)', src))
    return sorted(toks)

def run(exe, args, timeout=15):
    # `--dump-config` goes FIRST: a value-taking token placed before it would swallow it as its value
    # (`--window --dump-config`) and the FG would start for real (observed 2026-09-03: a 434 MB capture
    # process left behind). On timeout the whole process tree is killed (taskkill /T), never left running.
    p = subprocess.Popen([exe, "--dump-config"] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, encoding="utf-8", errors="replace")
    try:
        out, _ = p.communicate(timeout=timeout)
        return p.returncode, out or ""
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(p.pid)], capture_output=True)
        else:
            p.kill()
        try: out, _ = p.communicate(timeout=5)
        except Exception: out = ""
        return -999, "TIMEOUT\n" + (out or "")

def classify(rc, out):
    if rc == -999: return "timeout"
    if "PARITY FAIL" in out: return "parity-fail"
    if "needs arg" in out: return "needs-arg"
    if "unknown option" in out: return "unknown"
    if "[dump-config]" in out: return "ok" if rc == 0 else f"ok-rc{rc}"
    return f"other-rc{rc}"

def dump_lines(out):
    keep = []
    for ln in out.splitlines():
        if ln.startswith("[old]") or ln.startswith("[new]") or ln.startswith("[hash]") or ln.startswith("[dump-config]"):
            keep.append(ln.rstrip())
    return keep

def record(exe, cli_cpp, out_path, skip):
    toks = [t for t in extract_tokens(cli_cpp) if t not in skip]
    lines = [f"# roundtrip record exe={os.path.basename(exe)} tokens={len(toks)}"]
    stats = {}
    for t in toks:
        rc, out = run(exe, [t])
        st = classify(rc, out)
        used = ""
        if st == "needs-arg":
            for v in VALUE_GUESSES:
                rc, out = run(exe, [t, v]); st2 = classify(rc, out)
                if st2 != "needs-arg" and st2 != "unknown": st, used = st2, v; break
        stats[st] = stats.get(st, 0) + 1
        lines.append(f"## {t} {used}".rstrip())
        lines.append(f"status={st}")
        lines.extend(dump_lines(out))
    lines.append("# summary " + " ".join(f"{k}={v}" for k, v in sorted(stats.items())))
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    print("\n".join(lines[-1:]))
    print(f"wrote {out_path} ({len(toks)} tokens)")
    # exit 1 only on a real gate failure: a parity mismatch or a launch that did not exit (timeout).
    # `other-rc0` = a token whose guessed value was rejected (enum flags) or a retired flag — recorded, not a failure.
    return 0 if stats.get("parity-fail", 0) == 0 and stats.get("timeout", 0) == 0 else 1

def parse_record(path):
    rec, cur = {}, None
    for ln in open(path, encoding="utf-8"):
        ln = ln.rstrip("\n")
        if ln.startswith("## "): cur = ln[3:]; rec[cur] = []
        elif cur is not None and not ln.startswith("#"): rec[cur].append(ln)
    return rec

def compare(a, b):
    ra, rb = parse_record(a), parse_record(b)
    only_a = sorted(set(ra) - set(rb)); only_b = sorted(set(rb) - set(ra))
    diff = [t for t in sorted(set(ra) & set(rb)) if ra[t] != rb[t]]
    print(f"tokens: {len(ra)} vs {len(rb)} | only in A: {len(only_a)} | only in B: {len(only_b)} | differing: {len(diff)}")
    for t in only_a: print("  only-A", t)
    for t in only_b: print("  only-B", t)
    for t in diff:
        print("  DIFF", t)
        for x in ra[t]: print("     A:", x)
        for x in rb[t]: print("     B:", x)
    return 0 if not (only_a or only_b or diff) else 1

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe"); ap.add_argument("--cli", default=os.path.join(os.path.dirname(__file__), "..", "src", "cli", "cli.cpp"))
    ap.add_argument("--out", default="roundtrip.txt"); ap.add_argument("--skip", default="--help,--list-monitors,--list-windows")
    ap.add_argument("--compare", nargs=2)
    a = ap.parse_args()
    if a.compare: sys.exit(compare(*a.compare))
    if not a.exe: ap.error("--exe required")
    sys.exit(record(a.exe, a.cli, a.out, set(a.skip.split(","))))
