# GDUMP_GATE — the gates G0–G5 of `GDUMP_PLAN.md`, run 2026-09-09 (afternoon)

> **What this is:** the first-hand record every `mitigated`/`accepted` row of `GDUMP_PLAN.md` §4 cites. Numbers
> are quoted from the runs' own output; the raw artifacts live under `C:\PhyriadFG\runs\` (`sc_live\observer_k4\`,
> `sc_live\g4_*`, `g5_live\`) — not in this repo. **Binaries:** base = HEAD `364efc5` built from a detached
> worktree, md5 `F36FDE32`, kept at `C:\PhyriadFG\bin\phyriad_fg_base_364efc5.exe`; new = the working tree,
> md5 `186CB46C`. Both with IPO (`/GL`): the project's `build-release/` had been configured on 2026-09-08 with
> IPO **off** (the MinGW `ar.exe` probe) and was wiped and reconfigured before G1 — otherwise G1 would have
> compared LTO, not the tap. The operator's screen was taken for ≈ 9 minutes in total.

## G0 — build, tests, the flag surface (no screen)

| Check | Result |
|---|---|
| build ×2 (`build-release.bat`) | clean, `LTO/IPO enabled`; `100% tests passed out of 51` (50 before + `gdump_book`) |
| `pfg_gdump_test` | `RESULT: PASS`, 26 checks; T1's counter-model overwrites a streaming slot (RR1 seen red), `begin_tick()` never (green); T3's identity seen red under a corrupted counter |
| `--dump-config` base vs new | `sizeof(Config) 1824 → 2088` only (the three new fields); `rows=28 params=21` and `contract=0x9517AE73A530EAFE` on both |
| `--help` base vs new | the `--gdump` / `--gdump-ring` / `--gdump-pairs` lines only |
| flag round-trip (`check_flag_roundtrip.py`, repaired — P-023) | base 274 tokens `ok=267 needs-arg=1 other-rc0=6`; new 277 = 274 identical + the 3 new tokens `status=ok`; the one "differing" block is the trailing summary line |
| the refusal | `--gdump <d> --afill --dump-config` → `[ra] --gdump: refused with --afill / --fps-overlay / --ts-smooth ... DISARMED for this run.`, exit 0, no directory created |
| MSVC C1061 | the three flags overflowed the main `else if` chain (P-004) and live in `parse_extra` |

## G1 + G2 — the observer effect, four arms × 2, interleaved, 20 s each, `sc_live` k=4 looped, `--wsub --warp-timing`

`tools/scene_truth/gdump_observer.ps1` → `gdump_observer.py`. Per-second series over 17 s (3 s warm-up dropped);
the run-to-run r of every per-second number is < 0.5 (the series are flat at 240 Hz), so only the run means
are quoted and the deltas are read against the base's own repeat spread.

| arm | present fps | warp ms (EMA) | fresh/s | rdrop/s | `wsub up` ms | `wt gpu` ms | gpu A % | presents / 20 s |
|---|---|---|---|---|---|---|---|---|
| base (pre-change binary) | 242.04 (242.02 / 242.06) | 0.92 (0.95 / 0.89) | 241.91 | 0.00 | 0.31 | 0.03 | 16.65 | 4457 / 4459 |
| off (new binary, no flag) | 242.10 (242.05 / 242.14) | 0.84 (0.91 / 0.78) | 241.82 | 0.00 | 0.31 | 0.03 | 16.84 | 4459 / 4459 |
| nocopy (`--gdump --gdump-ring 0`) | 242.13 (242.04 / 242.23) | 0.80 (0.93 / 0.66) | 241.89 | 0.00 | 0.31 | 0.03 | 16.59 | 4459 / 4461 |
| gdump (`--gdump`) | 241.96 (241.98 / 241.94) | 1.05 (1.04 / 1.06) | 241.85 | 0.00 | 0.32 | 0.09 | 17.93 | 4449 / 4454 |

- **G1 (DR1):** off − base = fps +0.06, warp −0.08 ms, fresh −0.10/s, `up` 0.00, gpu +0.18 % — every delta inside
  the base's own repeat spread. Byte-identical-off on rates: **PASS**.
- **G2 (CR4, CR5, PR1):** nocopy − base = fps +0.10, warp −0.12, gpu −0.07 % — the creation-time changes (usage
  bits, the timeline feature, the writer thread, the semaphore signal) cost **nothing measurable**. gdump − base =
  **+0.06 ms of GPU batch per tick** (`wt gpu` 0.03 → 0.09; the copy + its barriers), **+0.13 ms warp EMA** (P's
  record + the pair-plane memcpy amortised), **+0.01 ms** on the pair-advance upload wait (CR4), **+1.3 points**
  of GPU utilisation, **rdrop 0.00/s**, presents **−0.1 %**. The design's morning estimate (~0.04 ms of copy) was
  right; the residual is the observer effect, named.
- **The first pass (17:00) is the red of two rows:** with a 64-slot ring, `ring_full` 236 / 222 of ~4,000 recorded
  ticks (6 %) in two writer stalls of 0.73 s and 0.26 s at 16.5 s and 17.8 s (`ticks.tsv` gaps of 153 and 53 seqs;
  the OS write cache flushing a growing buffered stream — PR2), and the gdump r2 process **hung at exit** after
  printing its own summary: its last recorded ticks were ring-full, their timeline signals had no waiter, and
  `vkDestroySemaphore` ran with a signal pending (CR3). Fixed (unbuffered stream + ring 256 + pairs 16; `stop()`
  waits the last value) and the re-run above is the record. `docs/LEARNING_LOG.md` P-024.

## G3 — captured = recorded (`summary.txt`, the re-run)

| run | ticks | recorded | captured | ring_full | drop / dup | pairs uploaded / written / busy / clobbered | identity |
|---|---|---|---|---|---|---|---|
| observer gdump r1 | 4450 | 4449 | 4449 | **0** | 1 / 0 | 1199 / 1180 / 19 / 0 | ok |
| observer gdump r2 | 4455 | 4455 | 4455 | **0** | 0 / 0 | 1202 / 1202 / 0 / 0 | ok |
| G4 run | — | 4507 | 4507 | **0** | — | 1178 written | ok |
| G5 run (8 s) | 1805 | 1805 | 1805 | **0** | — | — | ok |

3.9 GB written per 20 s run, `writer_timeouts 0`. The 19 `pairs_busy` of r1 (1.6 % of its pairs) did not recur
in the other three runs; reported, not explained.

## G4 — the byte cross-check (`gdump_g4.ps1`, both taps, one sync run, 20 s)

`gdump_xcheck.py`: **`G4: 16 joined, 16 byte-identical`** — every frame the `--qdump` sampler dumped equals the
tap's streamed frame for the same tick, 921,600 bytes each, joined through `qdump_xref.tsv`.

## G5 — the tools (`scene_live.ps1 -Gdump`, 8 s on a copy of the corpus, `g5_live`)

- adapter: `1805 triples written, 0 skipped`; align: `180 aligned to arms/fg_k4/ (1111 dup, 514 skipped, 0 errors)` —
  **180 of the 180 possible k=4 mids of the corpus in 8 s** (the sampler gave ~133 in 20 s).
- `check_qdump_plus.py`: `pushsz constant = 232 B`, `REPLAYABLE: every binding this push arms has its plane in the
  record`, `RESULT: all checks passed` (a WARN on the phase histogram's lopsidedness — the tap records every phase
  the clock produces; the sampler balanced bins by construction).
- `ref_warp.py --triples 20 --mv-plane mv1 --mvb-plane mvb1`: `exact match mean 99.50 % (min 99.41)`, `within 1 LSB
  100.00 %`, `RESULT: no refusals` — the same fidelity the sampler's record gave (99.45 % / 100 %).
- `scene_report.py` (parallelised the same afternoon — `--jobs`, 24 min → 226 s, rows JSON-identical to serial on
  a 6-frame check), one run, **reliability not measured** (no second corpus/seed yet — DI-3 is owed before any
  verdict is read from it):

| arm | pos_err px | shape_err px | halluc px² | lead px | missing px² | sharp | verdict |
|---|---|---|---|---|---|---|---|
| truth | 0.000 | 0.000 | 0 | +0.00 | 0 | 1.000 | ACCEPT |
| nearest | 0.275 | 0.140 | 57 | −12.76 | 29 | 0.934 | pos |
| oracle2 | 0.097 | 0.156 | 27 | −23.03 | 56 | 1.000 | ACCEPT |
| **fg_k4, the async path, every mid** | **0.240** | 0.160 | 58 | **+8.93** | 28 | 0.950 | pos |

  Read beside B1's sampled sync-path row (pos 0.216 / 0.212 over two seeds, ACCEPT by a 0.02 px margin): this
  is a different population (every mid at every phase the clock produced, on the shipping async path) and one
  run; the lead keeps its positive sign (M1_LOWPHASE's signature). It is the first row of its kind, not a verdict.

## Validation layer (CR1, CR2, CR6, CR7), three 5 s runs, `g5_live\validation\*.log`

| run | validation lines | `VUID-vkCmdCopyImageToBuffer-srcImage-00186` |
|---|---|---|
| base binary + `--qdump` | 25 | **10** (the red: the sampler's copies violated the spec since it was written) |
| new binary + `--gdump` | 0 spec messages | 0 |
| new binary + `--qdump` | 0 spec messages | 0 |

## Not run (declared)

1080p (PR1/PR2: expected a counted sampler from the disk arithmetic); `--sg-barriers --gdump` and
`--upload-xfer --gdump` under the validation layer (CR6/CR7 `accepted` on inspection); Ctrl-C mid-run (CR3's
second verification — the bounded-run exit is the one exercised, eight times); the DI-3 second run of the G5 row.

*Made with my soul - Swately <3*
