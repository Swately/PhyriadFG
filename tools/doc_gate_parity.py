#!/usr/bin/env python3
"""doc_gate_parity.py — a LIVE document may not say that a gated phase is unbuilt.

WHY THIS EXISTS. On 2026-09-06 a session wrote "MOTION_TRUTH T2-T5 have no code" into five live
documents and a commit message. T2, T3, T4 and T5 had closed on 2026-09-04, each with a gate record
sitting in `docs/planning/records/`, and the tools were committed at `tools/motion_truth/`. The source
of the error was `records/BACKLOG_AUDIT.md` -- a dated snapshot whose rows went stale hours after it was
written -- read as if it were a live tracker. The foto mental already carried a warning about this exact
mistake (`docs/LEARNING_LOG.md` P-018 records the recurrence).

A note did not stop it the first time, so this is not a note. It is the cheapest mechanical statement of
the rule that would have caught it:

  (1) A LIVE planning document must not assert that a phase is unbuilt when a gate record for that phase
      exists on disk.
  (2) A RECORD may assert it -- records are dated evidence and are never rewritten -- but the file must
      carry a staleness marker (`SUPERSEDED` or `STALENESS`), so a reader meets the correction before the
      claim.

It reads files only. Exit 0 = clean, 1 = a violation (with file:line and the contradicting record), 2 =
the repository layout is not what this check assumes (a missing directory is a broken check, not a pass).

Made with my soul - Swately <3
"""
import io
import os
import re
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RECORDS = os.path.join(ROOT, 'docs', 'planning', 'records')
PLANNING = os.path.join(ROOT, 'docs', 'planning')

# The live documents: the spine, the live-thread snapshot, and the plan-tier docs beside them. NOT the
# records (dated evidence) and NOT archive/ (history).
LIVE = [os.path.join(ROOT, 'docs', 'FOTO_MENTAL.md')]
LIVE += [os.path.join(PLANNING, f) for f in sorted(os.listdir(PLANNING))
         if f.endswith('.md')] if os.path.isdir(PLANNING) else []

# "this is not built" in the vocabulary these documents actually use.
UNBUILT = re.compile(
    r'(zero code|no code|not built|unbuilt|never measured|never built|sin código|cero código|'
    r'no está construid|`designed`,\s*zero)', re.IGNORECASE)
STALE_MARK = re.compile(r'SUPERSEDED|STALENESS', re.IGNORECASE)
# A line that makes the claim AND retracts it is a correction. Flagging those would punish the writing
# this check exists to produce.
RETRACTION = re.compile(r'\bFALSE\b|\bfalso\b|corrected|CORREGIDO|SUPERSEDED|retract|no longer|ya no',
                        re.IGNORECASE)
NEAR = 90   # characters between the phase id and the "unbuilt" phrase before they stop being one claim


def gated_phases():
    """phase id -> the gate record that closes it, from the filenames on disk."""
    out = {}
    if not os.path.isdir(RECORDS):
        print('doc_gate_parity: FATAL - %s does not exist; the check cannot run' % RECORDS)
        sys.exit(2)
    for fn in sorted(os.listdir(RECORDS)):
        if not fn.endswith('_GATE.md'):
            continue
        stem = fn[:-len('_GATE.md')]          # S2_T0_T1 | S2_T1B | R6 | S4_3
        parts = stem.split('_')
        if parts[0] == 'S2':                  # every T-token in the name is its own phase
            for t in parts[1:]:
                out.setdefault('S2.' + t[0] + t[1:].lower(), fn)   # T1B -> T1b, T4 -> T4
        elif parts[0] == 'S4' and len(parts) == 2 and parts[1].isdigit():
            out.setdefault('4.' + parts[1], fn)                    # S4_3 -> 4.3
        elif re.match(r'^R\d$', stem):
            out.setdefault(stem, fn)                               # R6
            out.setdefault('S4.' + stem, fn)                       # and its spine id
    return out


def scan(path, phases, allow_stale_marker):
    body = open(path, encoding='utf-8', errors='replace').read()
    # A record is exempt only if it carries a BANNER -- the marker in its opening lines, where a reader
    # meets it before the claim. One occurrence of the word "superseded" 250 lines down (R4_GATE.md:255
    # says a latency reading is superseded) must NOT exempt the whole file: that is a file-level pass for
    # a line-level fact, and it is how a check quietly stops binding.
    marked = bool(STALE_MARK.search('\n'.join(body.split('\n')[:20])))
    bad = []
    lines = body.split('\n')
    for i, line in enumerate(lines, 1):
        hits = list(UNBUILT.finditer(line))
        # A retraction usually spans the sentence that QUOTES the claim and the one that KILLS it, so the
        # window is the line plus its two neighbours -- the paragraph a reader actually reads. Without
        # this, writing an honest correction is itself a violation.
        window = '\n'.join(lines[max(0, i - 3):i + 2])
        if not hits or RETRACTION.search(window):
            continue
        if allow_stale_marker and marked:
            continue

        def near(span):
            return any(not (h.start() > span[1] + NEAR or h.end() < span[0] - NEAR) for h in hits)

        named = []
        for pid, rec in sorted(phases.items()):
            m = re.search(r'\b' + re.escape(pid) + r'\b', line)
            if m and near(m.span()):
                named.append((pid, rec))
        if not named:                       # ranges: "T2-T5" / "T2–T5"
            m = re.search(r'\bT(\d)\s*[-–—]\s*T(\d)\b', line)
            if m and near(m.span()):
                named = [('S2.T%d' % k, phases['S2.T%d' % k])
                         for k in range(int(m.group(1)), int(m.group(2)) + 1) if 'S2.T%d' % k in phases]
        if named:
            bad.append((i, ', '.join(n[0] for n in named), named[0][1], line.strip()[:120]))
    return bad


def main():
    phases = gated_phases()
    if not phases:
        print('doc_gate_parity: FATAL - no *_GATE.md found in %s; the check cannot run' % RECORDS)
        return 2
    print('doc_gate_parity: %d gated phases on disk: %s' % (len(phases), ', '.join(sorted(phases))))
    fails = 0
    for path in LIVE:
        if not os.path.isfile(path):
            continue
        for ln, pid, rec, text in scan(path, phases, allow_stale_marker=False):
            print('FAIL %s:%d says %s is unbuilt, but %s closes it' % (os.path.relpath(path, ROOT), ln, pid, rec))
            print('     %s' % text)
            fails += 1
    for fn in sorted(os.listdir(RECORDS)):
        if not fn.endswith('.md'):
            continue
        path = os.path.join(RECORDS, fn)
        for ln, pid, rec, text in scan(path, phases, allow_stale_marker=True):
            print('FAIL %s:%d says %s is unbuilt (%s closes it) and the file carries NO staleness marker'
                  % (os.path.relpath(path, ROOT), ln, pid, rec))
            print('     %s' % text)
            fails += 1
    if fails:
        print('doc_gate_parity: %d violation(s)' % fails)
        return 1
    print('doc_gate_parity: OK - no live document contradicts a gate record')
    return 0


if __name__ == '__main__':
    sys.exit(main())
