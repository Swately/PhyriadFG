# R6_GATE — Stages 1 and 2 named: the ingest module, the contract types, the directory names (M-R6)

**Status: CLOSED — G-R6 PASSED (2026-09-06).** Operator's word: *"Continua en el orden que recomiendes"*
after R5 and 4.3 closed, with the standing test *"que el trabajo este bien hecho, no rutas comodas"*. This record
grows one section per step; the verdict is written when the three steps and G-R6 have run.

## 0 · The contract and the scope

`CONVERGENCE_MASTER_PLAN.md` §R6 and `STAGE_CONTRACT.md` §2: stage 1 (CAPTURE) is backend-specific acquisition;
stage 2 (INGEST) is the Vulkan convert and the publish into the FrameRing. The operator's decision of 2026-09-03
(`STAGE_CONTRACT` §6, decisions 1 and 2) settled two things this step rests on: **stages 1 and 2 are two modules**,
and the **directory names** are adopted (`src/ingest/` here; `warp_blend/` → `generate/` and `cli/` + `layers/` →
`control/` in step 3).

**MUST NOT change:** the publish order (`t_pub_ms` stamped before `c_seq.fetch_add`), the drop-to-newest rule, and
the WGC/DDA behaviour.

## 1 · Step 1 — the ingest module (2026-09-06) — CLOSED

**Built (`r6s1_patch.py`; `src/ingest/ingest.{hpp,cpp}` new).** Two bodies moved BY ANCHOR out of `run_capture`:

- **the serial convert + publish tail** (`capture.cpp:703–793` pre-extraction) → `pfg::ingest::convert_and_publish
  (ctx, cap_rot180, s)`. The region was located by its opening `if(!use_igpu_convert){` and its closing
  `c_cv.notify_all();`, and **verified brace-balanced (26 open / 26 close) before the move**. Its **32
  FgContext-derived captures** are rebound at the top of the function by the SAME `auto& X = ctx.X;` lines
  `run_capture` uses — the script extracts those lines from `run_capture` itself and filters them by what the body
  references, so they are computed, not chosen. Its only two genuine locals became parameters: `cap_rot180` (the DDA
  ROTATE180 correction) and the slot index `s`. **Zero lambdas were captured** — this tail is much cleaner than R5's
  orchestrator was.
- **`run_convert_worker`** (`:797–971`), already a top-level `FgContext&` function, moved whole with its comment
  header.

**Verbatim: 225 of 225 moved body lines (100.00 %)** byte-identical (whitespace-trimmed) to the pre-extraction file
(`r6/capture_pre_extract.cpp` is the reference).

**A duplication the anchor exposed.** `if(!use_igpu_convert){` matches **twice** — once in the serial tail and once
inside `run_convert_worker`: the two ingest paths carry their own copies of the convert logic. The script had to be
constrained to the region before `run_convert_worker`'s definition to tell them apart. **Not deduplicated here** —
that is a behaviour change, not a move, and it belongs to whoever measures the two paths against each other. It is
recorded because the next person to touch either path must know the other exists.

**Dead code the move exposed:** with the tail gone, the compiler named **25 `auto& X = ctx.X;` aliases in
`run_capture` that nothing referenced any more**. Removed (deleting a live one fails the build, so the compiler is
the proof). `capture.cpp` 972 → **683 lines** and compiles with **zero warnings**.

### 1.1 · Gate

- **Build:** 0 errors, **0 warnings** (`capture.cpp` and `ingest.cpp` both clean); the 45-test suite green on every
  build (4.3's wiring runs it automatically).
- **The ingest invariants, 2 runs per side** (ball zoo 60 fps, 30 s, the pre-R6 binary rebuilt from `706d2ba` then
  HEAD restored and rebuilt):

| | before ×2 | after ×2 | Δ |
|---|---|---|---|
| `in`/s (frames the acquirer took) | 59.90 / 59.91 | 59.90 / 59.91 | **+0.00** |
| `acq`/s | 60.06 / 60.06 | 60.06 / 60.06 | **+0.00** |
| `uniq`/s (post-dedup — the drop-to-newest observable) | 59.90 / 59.91 | 59.90 / 59.91 | **+0.00** |
| `arr` (the flow's arrival count) | 60.1 / 60.1 | 60.1 / 60.1 | **+0.00** |
| `ringfull`/s · `dd_lost` | 0 · 0 | 0 · 0 | — |
| presents / 30 s | 7,190 / 7,189 | 7,190 / 7,190 | +0.5 |

- **The startup log diff (the E1 instrument):** raw diff 18 lines, of which every one is either a measured value
  (the gme fit cost, the objects cost EMA), the OS-allocated MMCSS task index, or the order of the three MMCSS
  join lines, which race. **Normalised** (floats → `#`, `idx=N`, lines sorted) the diff is **2 lines** — and those
  two are the `--csv` path, which the harness varies per run. The **before-vs-before control shows the same 2-line
  diff**, so the before/after difference is exactly the run-to-run difference: the startup output did not change.
- **The 120 s smoke:** clean exit, 28,768 presents, and R5's two-oracle instruments still at **0 mismatches**
  (79,105 row decisions, 43,086 transport decisions).
- **The `--help` round-trip:** proven by construction rather than by running two binaries — `git diff --name-only
  HEAD~1 HEAD` is `CMakeLists.txt`, `capture.cpp`, `ingest.{hpp,cpp}`, and the help text is generated from
  `cli/cli.cpp` + `layers/layer_registry.cpp` only, neither of which the step touched. The suite's
  `cli_help_exits_0` runs it on every build.

**Not in this step:** the `RawFrame` / `RealFrame` contract types (step 2), the directory renames (step 3), and the
convert duplication above.

## 2 · Step 2 — the ingest rings, and the publish order made structural (2026-09-06) — CLOSED

**Built (`src/ingest/frames.hpp`, new).** `pfg::ingest::FrameRing` (2 → 3, 2 → 4) and `RawRing` (1 → 2), the same
shape R5 gave the flow side: each OWNS its seq_cst publish counter (`c_seq`, `raw_seq`) and BINDS the slot storage
by reference, with `main()` keeping the former names as aliases so every consumer reads and writes the same memory.

**The half that has a consumer — `FrameRing::publish(slot, stamp_ms)`.** The invariant R6 must not change
(`t_pub_ms` stamped BEFORE the `fetch_add`, because that store/load pair is the only thing ordering the slot's
fields for the reader that observes the new sequence) was written out by hand at **two** sites — the serial tail and
the `--ingest-async` worker. Both now call one function; the order lives in its body instead of in a comment
repeated twice. `grep c_seq.fetch_add src/` returns only `frames.hpp`.

**What was deliberately NOT built, and why.** `STAGE_CONTRACT` §1 names `RawFrame` / `RealFrame`, and the plan says
the structs "replace the loose locals". They were written, then **removed before committing**: the eight readers of
`c_slots[]` (capture, flow, flow_consume, present ×2) address the ring by an **arbitrary slot** —
`c_slots[rfp_slot]`, `c_slots[mf_slot]`, `c_slots[s]` for a slot the caller already chose — not by the publish
sequence. A view keyed on `seq` does not fit them; a view keyed on `slot` would be a struct with one member. Either
would be a wrapper with no consumer, which the container's rule 1 forbids. The header says this in place, so the
next session does not re-derive it. **The plan's letter is not met here, deliberately, and this is the record of
that choice.**

**Gate:** build 0 errors; the 45-test suite green; the ingest rates against step 1's binary — `in` 59.92 vs 59.91,
`acq` 60.07 vs 60.06, `uniq` 59.92 vs 59.91, `arr` 60.08 both (deltas ≤ 0.013/s, inside these metrics' run-to-run
spread), presents 7,188–7,190, every run a clean exit; and one `--ingest-async` run (the second publish site, the
only path that reaches the worker's call) at `in` 59.91 / `acq` 60.06 / `uniq` 59.91, 7,190 presents.



## 3 · Step 3 — the directory names (2026-09-06) — CLOSED

The operator adopted them on 2026-09-03 (`STAGE_CONTRACT` §6 decision 2); this is the move:
`src/warp_blend/` → **`src/generate/`** (stage 5) and `src/cli/` + `src/layers/` → **`src/control/`** (the CONTROL
plane: the flag surface and the registry are one thing). Ten files moved with `git mv` (history preserved), 25
reference sites rewritten, `src/` now reads as the contract does: `capture · ingest · flow · clock · generate ·
present · control · instrument · core · seam`.

**The trap the script was built around.** There are **two** `layers/` paths: `src/layers/` (the registry's C++) and
`shaders/layers/` (the row BODIES). The generator emits `#include "layers/<row>.glsl"` and builds
`<shaders_dir>/layers/<row>.glsl` at runtime — rewriting either would leave every fused row's body unfindable. The
rewrite is therefore anchored on **`layers/layer_`**, which only ever names the C++ side, and the script asserts
before it starts that no row body is called `layer_*` and asserts afterwards that the shader include and the body
path survived verbatim. Confirmed after the build: `build-release/gen/shaders/layer_includes.glsl` still reads
`#include "layers/fetch_mv.glsl"`.

**Which documents were rewritten, and which were not.** The rule applied: **a document that DESCRIBES THE CURRENT
SYSTEM is updated; a document that RECORDS WHAT WAS DONE keeps the paths it recorded.** Updated: `ARCHITECTURE.md`,
`STAGE_CONTRACT.md`, the three CONVERGENCE plans, the MOTION_TRUTH plans, `MV_EDGE_SNAP_PLAN`,
`SINGLE_TRACK_MODE_PLAN`, `LEARNING_LOG.md`, `tools/*` — their path mentions are navigational (a scope list points
at code to edit). Reverted after the sweep touched them: **`docs/FOTO_MENTAL.md`** (a dated journal — entry 4d says
what was created on 2026-09-03, when the directory was `src/layers/`; rewriting it would make the record claim
something that was not true on its date) and **`docs/planning/aap/*`** (the design candidates and the two analysis
records, same class as `records/`, whose citations are evidence of what was read).

**Gate:** build 0 errors; **no new warning** — the warning set is the pre-existing one (the two the step did create,
`c_seq` and `c_slots` left dead in `ingest.cpp` by step 2's `publish()`, were removed; the worker still needs its
own pair, so only the serial function's were dead); the 45-test suite green; `grep` for every old path in
`src/ tools/ tests/ CMakeLists.txt build-release.bat` returns nothing.

## 4 · G-R6 — **PASSED** (2026-09-06)

The whole of R6 against the pre-R6 binary (`706d2ba`, rebuilt for the comparison then HEAD restored), 30 s runs on
the ball zoo, 2 per side:

| | pre-R6 ×2 | R6 complete ×2 | Δ |
|---|---|---|---|
| `in`/s | 59.90 / 59.91 | 59.92 / 59.94 | **+0.025** |
| `acq`/s | 60.06 / 60.06 | 60.06 / 60.08 | **+0.006** |
| `uniq`/s (the drop-to-newest observable) | 59.90 / 59.91 | 59.92 / 59.94 | **+0.025** |
| `arr` | 60.1 / 60.1 | 60.1 / 60.1 | **+0.000** |
| presents / 30 s | 7,190 / 7,189 | 7,189 / 7,189 | −0.5 |
| clean exit | yes | yes | — |

Every delta is at or inside these metrics' run-to-run spread (0.01–0.03/s). Plus, per the plan's letter: build ×2
(many), the **120 s smoke** (clean, 28,768 presents, R5's instruments at 0 mismatches), the **startup log diff**
(structurally 0 — the raw diff is timings, the OS-allocated MMCSS index and the racing join order, and the
before-vs-before control produces the same residue), the **`--help` round-trip** (proven by construction in §1.1 and
run by the suite on every build), and one **`--ingest-async`** run per step, the only path that reaches the moved
worker.

**Verdict: G-R6 PASSED.** Stages 1 and 2 are two modules; the ingest crossings have their rings and one publish
that owns the order; the directory names are the contract's.

## 5 · Honesty ledger

- The A/B is 2 runs per side on one content at one rate; the metrics compared are the ingest-side rates, which is
  what this step can affect.
- ~~It says nothing about the `--ingest-async` path: `run_convert_worker` moved but was never executed.~~
  **Closed immediately after that sentence was written** (it was named as the step's weakest claim, so it was
  measured): one 30 s run with `--ingest-async`, which is the only path that spawns the moved worker —
  `in` 59.89/s, `acq` 60.05/s, `uniq` 59.89/s, 7,189 presents, clean exit, the same rates as the serial path
  (59.90 / 60.06 / 59.90). One run, not a DI-3 pair: the claim is "the moved worker runs and paces as before",
  not a latency measurement.
- "The comfortable version" of this arc: move the two bodies, watch the build go green, and stop. What that would
  have missed: the 25 dead aliases (found only by reading the compiler's warnings after the move), the convert
  duplication (found only because an anchor matched twice), the startup-log control run (without a before-vs-before
  diff, the 2-line result would have looked like a real difference), the two view structs that had no consumer
  (§2), and the two `layers/` paths that a naive rename would have merged (§3).
- **What R6 did NOT do, named:** the convert logic is still duplicated between the serial tail and the worker (§1);
  `RawFrame` / `RealFrame` do not exist as types (§2, with the reason); the `RawRing` is declared and **its fields
  are still addressed through the old aliases** everywhere — only its counter is owned, so it is one step less real
  than the `FrameRing`, which also owns `publish()`. The rename moved `src/seam/` nowhere: it is the R2 engine and
  `STAGE_CONTRACT` §6 does not name it.
- The A/B is 30 s runs on one content at one rate; it measures the ingest-side rates, which is what these steps can
  affect. Latency and phase were not compared here — R5's gate covers those for the present side and nothing in R6
  touches them.

*Made with my soul - Swately <3*
