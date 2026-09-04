# S2.T0 + S2.T1 — gate record: the scorer ported, the replay record built · 2026-09-03

> Phases T0 and T1 of [`../MOTION_TRUTH_MASTER_PLAN.md`](../MOTION_TRUTH_MASTER_PLAN.md) (strategies S0
> and S1 of its companion). These exist because **R3's M4 gate cannot run without them**: M4 asks whether
> `fg_core.comp` reproduces `wap_warp.comp` byte-for-byte on a replay set, and a triple plus `t` is not a
> replay set. **Verdict: T0 PASSED, T1 PASSED with one finding that must be fixed before the record is
> used as R3's corpus** (§4). Every number is quoted from command output.

## 1 · T0 — the scorer, ported and building from the repo

`tools/fg_quality_scorer/` (main.cpp, CMakeLists.txt, prep_zoo_sequence.py, README.md) from the
container catalog. **The one substantive edit** is the CMake `_render_vulkan` path: it now points at the
repo's VENDORED `framework/render/vulkan`, not the catalog's copy. That is the whole point of the port —
the two lineages diverged (427/146 diff lines, measured 2026-09-02), so a scorer built against the
catalog does not score this product's flow.

| Check | Result |
|---|---|
| builds from the repo | `cmake --build build-fgq` exit 0; `fg_quality_scorer.exe` 155,648 B |
| Mode T on a fresh dump | `phyriad_fg.exe --window 'RA Ball Zoo' --no-async-present --qdump DIR 6 --exit-after 12` → `qdump: wrote 6 triples`; scorer → **6 CSV rows**, the expected shape |

**Two build defects fixed in the port**, both from the catalog CMake:
- `/O2` was hard-coded in `target_compile_options`. With no build type CMake defaults to Debug on MSVC,
  which adds `/RTC1`, and cl refuses the pair (`D8016`). The optimisation now comes from the build type;
  only `/W4` is unconditional.
- The file also now defaults `CMAKE_BUILD_TYPE` to Release when none is given: it is a measurement tool,
  and an unoptimised build is the wrong thing to measure with.

## 2 · A documented claim that was WRONG, corrected

Four documents (and my own port README, first draft) stated that `--qdump` is **inert under the shipping
default** because the dump block is gated on `!ap` and `--async-present` is on by default. **That is
false.** `resolve_config` (`src/cli/cli.cpp:227–231`) auto-disables `--async-present` for any run that
asks for `--qdump` or `--outdump`, and prints the reason.

Verified by running it: `phyriad_fg.exe --window 'RA Ball Zoo' --qdump DIR 4 --exit-after 10` with **no**
`--no-async-present` printed the auto-disable line and wrote 4 triples (13 files). The original claim came
from reading the present-side gate without the config-side auto-disable. Corrected in
`CONVERGENCE_MASTER_PLAN.md`, `MOTION_TRUTH_MASTER_PLAN.md`, `MOTION_TRUTH_IMPLEMENTATION_STRATEGIES.md`,
the scorer README, and `--help`.

What remains true, and is what actually matters for measurement: a `--qdump` run is **not** the shipping
present path. Its numbers describe the synchronous path and must be labelled as such.

## 3 · T1 — `--qdump+`, the replay record

Added to the existing dump block (same synchronous path, no new synchronisation), per tick:

| Sidecar | Content | Measured |
|---|---|---|
| `q%06d_mv.rg16f` | the MV plane the warp sampled, RG16F at W/8 | 129,600 B = 240 × 135 × 4 |
| `q%06d_sad.rg16f` | the SAD plane its gates read | 129,600 B |
| `q%06d_push.bin` | the push block **as submitted** | 232 B, constant across ticks |
| manifest tokens | `gen= mvw= mvh= mv= sad= push= pushsz= gme_valid= gme=a,b,c,d,e,f` | appended, so older parsers are unaffected |

**The generation index is recorded, never recomputed.** `qd_gen` is set at the `wap_upload()` call site,
so the sidecars describe exactly the fields the warp read. Ring safety: the dump reads generation
`qd_gen` while FLOW may be writing `qd_gen+1`; `kGenRing = 3` makes that safe by construction.

**The push block is snapshotted at the submit site**, not read later: `pcw` lives in a nested block that
closes before the dump tap, and a copy taken at submit is also the truer record — those are the bytes the
GPU received.

`tools/check_qdump_plus.py` (numpy + stdlib) validates a record rather than trusting it:

```
manifest: size=(1920, 1080) triples=6
  q000000: gen=0 mv 240x135 |MV| max=24.36 p99=2.01 px, moving=32.9% | push 232 B rc=32 improv=0.2 agree=0.05 t=0.1246 (manifest 0.1246) OK | gme_valid=1
  ...
pushsz constant = 232 B
RESULT: all checks passed
```

The check that matters most is the last field of each line: the push block's 4th float is the core's `t`,
and it is compared against the manifest's own `t` for that tick. Equal on every tick, so the push bytes
provably belong to that tick and are not a stale copy.

**Backward compatibility:** the ported scorer read the `+` manifest unchanged — 6 rows, exit 0.

## 4 · The finding: the record is valid, but the SAMPLING is not representative

The sidecars are correct and complete. The **corpus** they form is not, and the check now says so.

Measured on a 16-triple run: `t` landed in **two** eighth-of-a-pair bins — ten triples at ≈0.125 and six
at ≈0.375 — and **every** triple came from generation 2.

```
coverage: 16 triples | t in [0.125,0.380] span=0.255 | distinct t-bins(1/8) = 2 | generations = ['2']
  WARN: fewer than 3 distinct phase bins — this record is NOT a representative M4 corpus.
  WARN: every triple came from ONE generation — the record does not exercise the ring.
```

The dump stride's own comment (`present.cpp`, `kQdumpStride=11`) claims 11 is "coprime with the ~16
phase-steps/span so successive dumps land on DIFFERENT phases". On this configuration — 60 fps source on
a 240 Hz panel, four phase-steps per pair — that reasoning does not hold, and the measurement shows it
does not happen.

**Consequence, stated plainly:** M4 over such a set would test the core at one or two phases and one
generation. That is a weak oracle for a gate whose whole job is to prove the new core reproduces the old
one everywhere. **The sampling must be fixed before R3 uses this as its replay set** — rotate the
within-pair offset, or target phases explicitly. The warning is now emitted by the checker, so the
weakness cannot be forgotten or silently inherited.

This does not block T1: the record FORMAT is what T1 delivers, and it is verified. It blocks using an
unfixed record as R3's corpus, which is an R3 precondition and is written as one.

## 5 · Honesty ledger

- The scorer's own baseline (`FG_QUALITY_BASELINE.json`) was **not** re-locked here. The plan's S0 asks
  for it; it needs a full 7-preset Mode A run over the zoo and belongs with T2/T3, not with the port.
  Until then, any baseline value in the scorer's README was measured against the CATALOG flow and is
  historical — the README now says so.
- The `--qdump` path is the synchronous present path, so the record does not describe the shipping
  async path. Making the async path dumpable stays out of scope (master plan §6).
- `check_qdump_plus.py`'s plausibility ceiling for |MV| (64 px) is a guard against a decode/stride error,
  not a quality threshold.
- Not measured: the dump's cost per tick (it stalls the tick by design, like `--outdump`), and whether
  the fixed sampling offset also biases WHICH content the triples capture.

*Made with my soul - Swately <3*
