#!/usr/bin/env python3
"""gdump_adapter.py — turn a `--gdump <dir>` record (GDUMP_PLAN.md §2, the every-tick capture tap)
into a `--qdump`-shaped directory that the existing tools consume UNCHANGED: `scene_align.py`,
`ref_warp.py`, `check_qdump_plus.py`, `marker_extract.py` (GDUMP_PLAN.md §2's "Reader contract"
line, strategy S8, risk rows DR3 / RR4).

This is a RENAMING plus a SLICE, not a recomputation — every byte this script writes already exists
in the gdump record. `live.rgba` and `push.bin` are one big stream per captured tick (sliced at the
tick's `live_off` / `push_off` from `ticks.tsv`, never loaded whole — RR5's "the logical size is
what is written" is honoured by slicing exactly `WW_warp*WH_warp*4` / `push_bytes`, the header's own
numbers); the pair planes under `pairs/p<pair>_<plane>.<ext>` are the qdump plane names already
(GDUMP_PLAN.md §2 row `pairs/p<pair>_<plane>.<ext>`), so they are copied or hard-linked verbatim.

    python gdump_adapter.py --dir <gdump dir> --out <qdump-shaped dir> [--seq A:B] [--link]

`--seq A:B` restricts the adaptation to ticks whose `ticks.tsv` `seq` falls in [A,B] (inclusive).
`--link` hard-links the pair-plane files instead of copying them (falls back to a copy per-file on
any OSError — e.g. `--out` on a different volume than `--dir`); `live_*`/`push_*` are always written
fresh (they are SLICES of a shared stream, not whole files, so there is nothing to link).

A captured tick whose pair has no `pairs.tsv` row — or whose row is missing a plane `ref_warp.py`
opens unconditionally (`prev`, `next`, `sad`; RR4: "ref_warp needs sad" generalizes to all three,
since `ref_warp.py`'s `rgba()` calls on `prev`/`next`/`live` are unconditional too, load_manifest
row's optional planes are the ones spelled `-`) — is written to `skipped.txt` with the reason and
NOT materialised, rather than producing a triple `ref_warp.py` can only fail on.

stdlib only (no numpy — every slice here is an exact byte range; plain seek+read, RR5's own
discipline). Made with my soul - Swately <3
"""
import argparse
import os
import shutil
import sys

# ── §2's column contracts, positional (the files are "space-separated tokens", not literal TSV) ───
TICK_COLS = ['seq', 'slot', 'tick', 't', 'gen', 'tgen', 'pair', 'decision', 'flags',
             'qpc', 'live_off', 'push_off', 'pairset']
PAIR_COLS = ['pair', 'gen', 'tgen', 'seq_recorded', 'gme_valid', 'g0', 'g1', 'g2', 'g3', 'g4', 'g5',
             'prev', 'next', 'mv1', 'mvb1', 'sad', 'c2', 'dis', 'disb', 'per', 'mvt', 'mv', 'mvb']

# pairs.tsv plane column -> its file extension (GDUMP_PLAN §2 row `pairs/p<pair>_<plane>.<ext>`),
# in the same order as PAIR_COLS so a listing reads the same way the contract table does.
PLANE_EXT = {
    'prev': 'rgba', 'next': 'rgba', 'mv1': 'rg16f', 'mvb1': 'rg16f', 'sad': 'rg16f',
    'c2': 'rgba16f', 'dis': 'r8', 'disb': 'r8', 'per': 'r8', 'mvt': 'rg16f', 'mv': 'rg16f', 'mvb': 'rg16f',
}
# `ref_warp.py` opens these three unconditionally (no `if nm != '-'` guard) — a tick whose pair is
# missing any of them cannot be replayed, so it is refused here rather than handed downstream to fail.
REQUIRED_PLANES = ('sad', 'prev', 'next', 'mv')   # `mv` too: ref_warp's default --mv-plane loads r['mv'] with no '-' guard


def load_hdr(path):
    """gdump.hdr: one `key v1 v2 ...` record per line, `#` comments, space-separated (§2 row 1)."""
    hdr = {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith('#'):
                continue
            toks = ln.split()
            hdr[toks[0]] = toks[1:]
    return hdr


def load_cols(path, cols):
    """A §2 record file: space-separated tokens, `#` comments, one row per line, `len(cols)` tokens
    per row. A row with the wrong token count is a corrupt record, not a schema variant — it is
    dropped with a warning rather than silently mis-columned."""
    rows = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for lineno, ln in enumerate(f, 1):
            ln = ln.strip()
            if not ln or ln.startswith('#'):
                continue
            toks = ln.split()
            if len(toks) != len(cols):
                print(f'WARN: {path}:{lineno}: {len(toks)} tokens, expected {len(cols)} — row dropped',
                      file=sys.stderr)
                continue
            rows.append(dict(zip(cols, toks)))
    return rows


def parse_seq_range(s):
    if s is None:
        return None
    a, b = s.split(':')
    return int(a), int(b)


def link_or_copy(src, dst, use_link):
    if use_link:
        try:
            os.link(src, dst)
            return
        except OSError:
            pass  # cross-volume, or the filesystem refuses hard links — fall back to a copy
    shutil.copyfile(src, dst)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--dir', required=True, help='a --gdump capture directory (GDUMP_PLAN.md §2)')
    ap.add_argument('--out', required=True, help='the --qdump-shaped directory to write (created if absent)')
    ap.add_argument('--seq', default=None, help='restrict to ticks.tsv seq in A:B inclusive')
    ap.add_argument('--link', action='store_true', help='hard-link pair-plane files instead of copying them')
    a = ap.parse_args()

    hdr_path = os.path.join(a.dir, 'gdump.hdr')
    if not os.path.exists(hdr_path):
        print('FAIL: no gdump.hdr in', a.dir)
        return 1
    hdr = load_hdr(hdr_path)
    missing_hdr = [k for k in ('size', 'live_div', 'mv', 'push_bytes', 'kernel', 'contract') if k not in hdr]
    if missing_hdr:
        print(f'FAIL: gdump.hdr is missing {missing_hdr}')
        return 1

    WW, WH = (int(x) for x in hdr['size'][:2])
    D, WW_warp, WH_warp = (int(x) for x in hdr['live_div'][:3])
    mvw, mvh = (int(x) for x in hdr['mv'][:2])
    push_bytes = int(hdr['push_bytes'][0])
    kernel = hdr['kernel'][0]
    contract_tok = hdr['contract'][0]
    contract = int(contract_tok, 16) if contract_tok.lower().startswith('0x') else int(contract_tok)
    live_frame_bytes = WW_warp * WH_warp * 4

    ticks_path = os.path.join(a.dir, 'ticks.tsv')
    pairs_path = os.path.join(a.dir, 'pairs.tsv')
    live_path = os.path.join(a.dir, 'live.rgba')
    push_path = os.path.join(a.dir, 'push.bin')
    for p, label in ((ticks_path, 'ticks.tsv'), (pairs_path, 'pairs.tsv'),
                     (live_path, 'live.rgba'), (push_path, 'push.bin')):
        if not os.path.exists(p):
            print(f'FAIL: no {label} in {a.dir}')
            return 1

    tick_rows = load_cols(ticks_path, TICK_COLS)
    pair_rows = load_cols(pairs_path, PAIR_COLS)
    pairs_by_id = {}
    for r in pair_rows:
        try:
            pairs_by_id[int(r['pair'])] = r
        except ValueError:
            print(f'WARN: pairs.tsv row with unparsable pair id {r["pair"]!r} dropped', file=sys.stderr)

    seq_range = parse_seq_range(a.seq)
    if seq_range:
        lo, hi = seq_range
        tick_rows = [r for r in tick_rows if lo <= int(r['seq']) <= hi]

    os.makedirs(a.out, exist_ok=True)
    pairs_dir = os.path.join(a.dir, 'pairs')

    triples = []
    skipped = []   # (tick row, reason)
    map_rows = []  # (qid, seq, tick, pair_id)

    with open(live_path, 'rb') as flive, open(push_path, 'rb') as fpush:
        q = 0
        for row in tick_rows:
            try:
                pair_id = int(row['pair'])
            except ValueError:
                skipped.append((row, f'unparsable pair id {row["pair"]!r}'))
                continue
            prow = pairs_by_id.get(pair_id)
            if prow is None:
                skipped.append((row, f'pair {pair_id} has no pairs.tsv row (ref_warp needs sad)'))
                continue
            missing = [p for p in REQUIRED_PLANES if prow.get(p, '-') == '-']
            if missing:
                skipped.append((row, f'pair {pair_id} row is missing required plane(s) {missing} '
                                      f'(ref_warp opens prev/next/live and needs sad unconditionally)'))
                continue

            qid = f'q{q:06d}'

            live_off = int(row['live_off'])
            flive.seek(live_off)
            buf = flive.read(live_frame_bytes)
            if len(buf) != live_frame_bytes:
                skipped.append((row, f'live.rgba short read at off {live_off}: got {len(buf)} B, '
                                      f'wanted {live_frame_bytes} B'))
                continue

            push_off = int(row['push_off'])
            fpush.seek(push_off)
            pbuf = fpush.read(push_bytes)
            if len(pbuf) != push_bytes:
                skipped.append((row, f'push.bin short read at off {push_off}: got {len(pbuf)} B, '
                                      f'wanted {push_bytes} B'))
                continue

            with open(os.path.join(a.out, f'{qid}_live.rgba'), 'wb') as fo:
                fo.write(buf)
            with open(os.path.join(a.out, f'{qid}_push.bin'), 'wb') as fo:
                fo.write(pbuf)

            plane_name = {}
            for plane, ext in PLANE_EXT.items():
                src_name = prow.get(plane, '-')
                if src_name == '-' or not src_name:
                    plane_name[plane] = '-'
                    continue
                dst_name = f'{qid}_{plane}.{ext}'
                link_or_copy(os.path.join(pairs_dir, src_name), os.path.join(a.out, dst_name), a.link)
                plane_name[plane] = dst_name

            gme = [float(prow.get(f'g{i}', '0')) for i in range(6)]
            triples.append({
                'qid': qid, 't': float(row['t']), 'gen': int(row['gen']), 'tgen': int(row['tgen']),
                'live': f'{qid}_live.rgba', 'push': f'{qid}_push.bin',
                'gme_valid': int(prow.get('gme_valid', '0')), 'gme': gme, 'plane': plane_name,
            })
            map_rows.append((qid, row['seq'], row['tick'], pair_id))
            q += 1

    # ── manifest.txt — present.cpp:1513-1531's EXACT token order (that fprintf is the contract; this
    # reproduces its key set and order field-for-field so a `+`-aware consumer sees no difference
    # between a qdump manifest and an adapted one). `mv0`/`mvt0` name the SAME file as `mv`/`mvt`: the
    # gdump pair set has no separate upload-time snapshot the way qdump's per-tick `qd_mv0`/`qd_mvt0`
    # locals do — the pair's `mv`/`mvt` planes ARE the upload-time snapshot (memcpy'd once, at
    # `on_pair_upload`, GDUMP_PLAN §1.3), so the two tokens are the same name by construction, not a
    # coincidence. `ab=-` always: the `--fg-core-ab` running totals are a live present-thread counter
    # with no equivalent in the gdump record. ──
    man_path = os.path.join(a.out, 'manifest.txt')
    with open(man_path, 'w', encoding='utf-8', newline='\n') as mf:
        mf.write(f'# gdump adapter (tools/scene_truth/gdump_adapter.py) — {a.dir} reshaped into a qdump-shaped '
                  'record (GDUMP_PLAN.md S8 / DR3 / RR4): a renaming plus a slice, not a recomputation.\n')
        mf.write('# q-index = selection order (0-based); gdump_map.tsv carries seq/tick/pair per q-index.\n')
        mf.write(f'size {WW} {WH}\n')
        if D > 1:
            # Mirrors --qdump's own rule (present.cpp:1507, only emitted when warp_div>1): omitting the
            # line at D==1 keeps ref_warp.load_manifest's default (live_div=1) in force, so its `!= 1`
            # refusal (ref_warp.py:410) never fires on an unscaled gdump record — the D=1 case is
            # byte-identical to a plain qdump manifest in this respect.
            mf.write(f'live_div {D} {WW_warp} {WH_warp}\n')
        for tr in triples:
            pn = tr['plane']
            mv0, mvt0 = pn['mv'], pn['mvt']
            gme_s = ','.join(f'{v:.9g}' for v in tr['gme'])
            mf.write(
                f"triple {tr['qid']} prev={pn['prev']} next={pn['next']} live={tr['live']} t={tr['t']:.4f}"
                f" gen={tr['gen']} mvw={mvw} mvh={mvh} mv={pn['mv']} sad={pn['sad']} push={tr['push']} "
                f"pushsz={push_bytes} gme_valid={tr['gme_valid']} gme={gme_s} tgen={tr['tgen']} "
                f"mvb={pn['mvb']} c2={pn['c2']} dis={pn['dis']} disb={pn['disb']} per={pn['per']} mvt={pn['mvt']} "
                f"mv0={mv0} mvt0={mvt0} mv1={pn['mv1']} mvb1={pn['mvb1']} core={kernel} "
                f"contract=0x{contract:016X} ab=-\n"
            )

    with open(os.path.join(a.out, 'gdump_map.tsv'), 'w', encoding='utf-8', newline='\n') as mf:
        mf.write('# q seq tick pair\n')
        for qid, seq, tick, pair_id in map_rows:
            mf.write(f'{qid} {seq} {tick} {pair_id}\n')

    if skipped:
        with open(os.path.join(a.out, 'skipped.txt'), 'w', encoding='utf-8', newline='\n') as sf:
            sf.write('# a captured tick that could not be materialised into a triple, and why\n')
            for row, reason in skipped:
                sf.write(f"seq={row.get('seq', '?')} tick={row.get('tick', '?')} "
                         f"pair={row.get('pair', '?')} : {reason}\n")

    print(f'[gdump-adapter] {a.dir} -> {a.out}: {len(triples)} triples written, {len(skipped)} skipped'
          + (' (see skipped.txt)' if skipped else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
# Made with my soul - Swately <3
