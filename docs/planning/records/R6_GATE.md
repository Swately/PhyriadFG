# R6_GATE — Stages 1 and 2 named: the ingest module, the contract types, the directory names (M-R6)

**Status: IN PROGRESS — step 1 of 3 closed (2026-09-06).** Operator's word: *"Continua en el orden que recomiendes"*
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

## 2 · Step 2 — `RawFrame` / `RealFrame` (pending)

## 3 · Step 3 — the directory names (pending)

## 4 · G-R6 (pending)

## 5 · Honesty ledger (running)

- The A/B is 2 runs per side on one content at one rate; the metrics compared are the ingest-side rates, which is
  what this step can affect.
- ~~It says nothing about the `--ingest-async` path: `run_convert_worker` moved but was never executed.~~
  **Closed immediately after that sentence was written** (it was named as the step's weakest claim, so it was
  measured): one 30 s run with `--ingest-async`, which is the only path that spawns the moved worker —
  `in` 59.89/s, `acq` 60.05/s, `uniq` 59.89/s, 7,189 presents, clean exit, the same rates as the serial path
  (59.90 / 60.06 / 59.90). One run, not a DI-3 pair: the claim is "the moved worker runs and paces as before",
  not a latency measurement.
- "The comfortable version" of this step: move the two bodies, watch the build go green, and stop. What that would
  have missed: the 25 dead aliases (found only by reading the compiler's warnings after the move), the convert
  duplication (found only because an anchor matched twice), and the startup-log control run (without a
  before-vs-before diff, the 2-line result would have looked like a real difference).

*Made with my soul - Swately <3*
