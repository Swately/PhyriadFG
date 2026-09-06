# LEARNING_LOG — PhyriadFG (project tier)

> Governed by the container's [`METACOGNITION_PROTOCOL`](../../../protocols/core/METACOGNITION_PROTOCOL.md);
> the container ledger points here (L-004). **What belongs here:** a fact about this project that a
> future session would pay to re-derive — a plan premise the code refuted, a build trap, the measured
> character of an instrument, a tool that is weaker than its name suggests. **What does not:** an
> ordinary bug, a completed task, a number that lives in a gate record. Entry shape and triggers: the
> protocol's §3–§4. Newest first.

---

### P-016 · A sampled oracle starves exactly the sites the run is certifying
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** R5 said the two-oracle instrument could be retired once a pressured run counted 0 mismatches.
  Both pressured runs report `bwd-skip:100% tier:5` — the tier-5 shed forces `do_bwd` false, which is precisely
  when the four `row_check` sites nested inside `if(do_bwd)` stop executing. The runs that certify the shedding
  branches are the runs that cannot exercise the backward ones.
- **lesson:** A runtime oracle's coverage is not uniform and can be ANTI-correlated with the condition being
  certified: the state that engages the branch under test is the same state that disables its neighbours. A
  pass count is not coverage — ask which sites the run could not reach before reading a 0 as proof.
- **corrective:** the retirement does not rest on the runs. `tests/layers/test_arm_parity.cpp` enumerates the
  11 sites over every combination of the arm inputs (4,157 checks, 4,052 site comparisons, seen red three
  ways), so the states no run reaches are the ones the test covers best. `records/R7_GATE.md` §2.

### P-015 · A pattern match that ignores structure (indentation, comments) reads the wrong thing
- **class:** recurrence · **date:** 2026-09-06 · **recurrences:** 2 · **status:** corrected
- **evidence:** `s2.index("        if(!use_igpu_convert){")` (8 spaces, the worker's copy) also matches inside
  the 16-space copy, because a substring search knows nothing about lines. It landed in the function just
  written and `src/ingest/ingest.cpp` went 322 → 68 lines in one run. R6 §1 hit the sibling failure — the same
  anchor matching in BOTH copies — which is why the count is 1, not 0.
- **lesson:** In a patch script a "line" anchor must be bracketed by the line terminator (`eol + text + eol`)
  and its match count asserted. Indentation makes every shallower anchor a suffix of a deeper one, and a
  duplication being removed is exactly what puts two matching regions in the file.
- **corrective:** `r7_convert.py` asserts `count == 1` on both bracketed anchors AND that the extracted block
  length falls in a plausible range; every search is bounded to the enclosing function's own region.
- **second instance, same day:** the dead-alias check written to find which `auto& x = ctx.x;` lines lost their
  last reader counted name matches on RAW lines, so `\bA\b` matched the "A" in a comment like *"A-path"* and
  `\bd\b` matched the `d` in `ctx.d.cap_rot`. It reported **0 orphans**; the compiler's `/W4` `C4189` list
  reported **seven**, and the compiler was right.
- **STRONGER corrective** (a second recurrence obliges more than a third note — METACOGNITION §7): for "is this
  local still read", do not write the check. Build and read `C4189` from the FULL log. The compiler already
  parses C++; a regex over lines does not, and the failure is silent in the direction that says "nothing to do".
  Where a script must match code, it strips comments and string literals first — the same `strip()` the block
  finders use — and its result is confirmed against a build before it is believed.


### P-014 · At n = 2 the spread can be exactly zero, and then every difference looks significant
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** the R7 A/B at 2 runs per side: `frame_count` was 10788 in both pre runs and 10789 in both post
  runs, so the within-side spread was **0** and a one-present difference over 10,788 read as "outside the
  spread"; three more metrics flagged the same way. At 4 runs per side the range is 2 and every metric is
  inside it (`records/R7_GATE.md` §3).
- **lesson:** DI-3's "compute it twice" is a floor for detecting gross error, not a resolution estimate. A
  quantised metric (an integer count) can return the same value twice by luck; the spread then comes out 0 and
  the comparison rejects noise as signal. When the question is "did this change anything", n = 2 cannot say no.
- **corrective:** the A/B table in R7 is n = 4 per side, alternated, with the second block run in the reverse
  order; the n = 2 table is quoted nowhere as a result.

### P-013 · A Python heredoc carrying a Windows path is a syntax error waiting to happen
- **class:** recurrence · **date:** 2026-09-06 · **recurrences:** 3 · **status:** corrected
- **evidence:** `python - <<EOF` with a `C:\Users\...` path in a string raises
  `SyntaxError: (unicode error) codec can't decode bytes ... truncated \UXXXXXXXX escape` (`\U` from
  `\Users`). It fired three times in one session — the third time while writing THIS entry through a heredoc.
  The shell's quoting is not the problem; Python's own escape processing of the literal is.
- **lesson:** Any script that mentions a Windows path is written to a FILE in the scratchpad and run by path.
  A heredoc is for one-liners with no backslashes. The file form is what the by-anchor method needs anyway:
  re-runnable, reviewable, and quotable in the gate record.
- **corrective:** every patch and analysis script of this session is a scratchpad file; the three heredocs
  that failed were rewritten as files (`r7_red.py`, `r7_convert.py`, this one).

### P-012 · Probing whether a flag exists starts a real run
- **class:** recurrence-risk · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `phyriad_fg --latency-trace` was run to find out whether the token was accepted. It is accepted, so
  the binary started a full FG run with no `--exit-after` and no window match, and ran until it was killed by PID
  (the shell call had to be moved to the background first).
- **lesson:** Use `--dump-config <flag>`: it parses, runs the registry parity, prints and exits. Since 4.3 it also
  returns 2 on an unknown option, so the probe answers the question by its exit code alone. A bare invocation is
  never a probe — every flag that only sets configuration falls through to the run loop.
- **corrective:** recorded here; the harnesses in `tools/` already pass `--exit-after`.

### P-011 · A perturbation that changes nothing observable proves nothing
- **class:** recurrence-risk · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** 4.3's first attempt to see the clock oracle red changed `+0.5` to `+0.5000001` inside an `int`
  truncation (`phase_clock.cpp`, the phase-quantisation key). The suite stayed **43/43 green** over 2,877 ticks: the
  nudge only changes the truncated value when the operand sits within 1e-7 of an integer boundary, which never
  happened. The second attempt moved `t_use` by one ulp — the quantity the oracle compares with `memcmp` — and
  produced a mismatch on **every** tick (`t_use=2877` of 2,877).
- **lesson:** When proving a gate can fail, perturb the exact quantity the gate compares, by an amount that quantity
  can carry. A green run after a perturbation has two readings — "the gate is blind" and "the perturbation was
  invisible" — and only the second is usually true. Stopping at the first would have retired a working oracle.
- **corrective:** `records/S4_3_GATE.md` §3 keeps the failed attempt (row A₀) in the table on purpose.

### P-010 · A parse error exited 0 for the life of the project
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `main.cpp:206` was `if (!parse_args(argc, argv, cfg)) return 0;` and `parse_args` returns false both
  for `--help` and for a user error, so `phyriad_fg --no-such-flag` and `phyriad_fg --mv-smooth` (missing value)
  printed a message and exited **0** — measured directly before the fix.
- **lesson:** Every harness in `tools/` checks `$LASTEXITCODE` / `rc`. A typo'd flag ran the DEFAULT configuration
  and reported success, which means any measurement taken with a misspelled flag was silently a default-config
  measurement. No such case is known to have occurred; none could have been detected either.
- **corrective:** `Config::parse_failed` set at the two error sites, `main` exits 2; three ctest cases pin both
  directions (`cli_unknown_option_exits_2`, `cli_missing_value_exits_2`, `cli_help_exits_0`).

### P-009 · A test that cannot fail is not a test
- **class:** dormancy · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `pfg_clock_test` with no argument printed `RESULT: all checks passed (0)` and exited 0 having
  replayed zero ticks; given an unopenable path it printed `SKIP` and still exited 0. Meanwhile `enable_testing()`
  and `add_test()` appeared zero times in `CMakeLists.txt`, so neither test binary had ever run outside a hand
  invocation. R1's bit-parity oracle — which works, and catches a one-ulp change on every tick — had never once been
  handed a log.
- **lesson:** Two failure modes travelled together here and both are the same shape: a check that exists and does
  not run, and a check that runs and cannot fail. Wiring the first exposes the second; neither is visible from
  reading the code, only from asking "what does this print when it is wrong?".
- **corrective:** 43 tests wired (`records/S4_3_GATE.md`), each seen red under a deliberate perturbation; the build
  script runs them and fails on red; the clock test now tells its three states apart in wording and exit code.

### P-008 · GPU saturation does not move the FG's pressure ladder; the source rate does
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** 45 s at `escalera_arbiter --profile heavy` (96–100 % GPU) with a 60 fps source: the FG held
  `240.0 fps … fresh:240/s … gpu(A:86%)` and printed **no** `gov-floor ENGAGE` — tier 0 throughout. The same load
  with a **120 fps** source: `tier:4 ×3`, `tier:5 ×6`, `bwd-skip:96%`.
- **lesson:** The tier ladder compares `t_pair_ema` against `pair_budget_ms = src_interval_us/1000` (`flow.cpp`) —
  it is a CPU-time-per-pair ladder. GPU load raises only the GPU legs F waits on, one term of that time. To reach
  the shedding branches, shrink the budget (raise the source rate) or lengthen F's CPU work; "make the GPU busy" is
  the wrong lever and will look like the ladder is dead.
- **corrective:** `tools/r5_pressure.ps1` takes `-ZooFps` and its header says why; `records/R5_GATE.md` §5 records
  both runs, including the one that proved the wrong lever.

### P-007 · `gpu_load.exe` is a moderate loader, not a saturator — and the strong tool already exists
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** open
- **evidence:** `records/R4_GATE.md` §4.6.1 (measured this session): under `tools/gpu_load.exe` the
  4090 sits at **32–35 % utilisation** — the "under load" A/B of XR15 was a moderate-load A/B, said
  so in the record. `SATURATION_PLAN.md:141–144` had already named the reason: it is "a pure
  **compute** loader (a ring-fed dispatch) … it does NOT contend for the graphics/copy engines, the
  present queue, or VRAM bandwidth the way a real game's render does". Its buffer is 1 MiB fixed
  (`tools/gpu_load.cpp`, `kElems = 256*1024` floats), one D3D11 context, `kInflight = 3`.
- **lesson:** "Under `gpu_load`" in any PhyriadFG record means *moderate compute contention on one
  queue*, not saturation. A gate that needs real pressure (the FG governor's tier ladder, the
  `HOLON` / `BIDIR_OK` shedding arms of R5 step 3b) will not be exercised by it.
- **corrective:** the sibling project `projects/gpu_oc/` already contains `escalera_arbiter.exe`
  (prebuilt, D3D11, six detectors, profiles `mixed|heavy|light|chaos|sweep`, `--gpu N --secs S`,
  a 9 GB VRAM pass and a power-virus profile) — verified present first-hand 2026-09-06. **Measured the same day
  (`nvidia-smi` sampled at 1.2 s over a 25 s `--profile heavy` run): 96–100 % utilisation, 355–361 W, ~10.7 GB of
  VRAM, verdict `STABLE`** — against `gpu_load.exe`'s 32–35 %. R5's pressured run uses it (`tools/r5_pressure.ps1`);
  `SATURATION_PLAN`'s unbuilt `--graphics-load` idea is superseded by reuse. Its build script had rotted (two stale
  paths: `G:\gpu_oc` and "Visual Studio\18") and was repaired in place — it rebuilds and the fresh binary runs
  (container ledger L-003). **It is a stability tool used as a load: its verdict is captured per run, so a GPU fault
  is never mistaken for an FG measurement.**

### P-006 · A wedged present thread cannot be rescued from inside the process
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** the operator's three `--tdr-test` runs (`records/R4_GATE.md` §5). After the device
  loss the P thread never returned from a driver/DXGI call (F printed `P pinned on gen … 64ms` from
  the dispatch tick on; the DXGI-loss line never printed), and the pillar's OA-10 watchdog could not
  hide the plane because `ShowWindow`/`SetWindowPos` from another thread route through the wedged
  thread's message loop.
- **lesson:** Two independent bounds are needed on a device-loss exit: every GPU wait must be
  bounded (`vk_wait_live`, 14 sites) **and** the worker joins must have a deadline, because a thread
  stuck inside the driver runs no code of ours again. The only give-back of an own-window plane is
  the process ending.
- **corrective:** both shipped (`fbb9566`, `90e2e2d`); the clean teardown (CSV finalize) does not
  run on that path and the exit line says so.

### P-005 · The async present's lost frames were a wait, not work
- **class:** refuted-premise · **date:** 2026-09-05/06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `records/R4_GATE.md` §4.6: the warp batch costs 0.08–0.11 ms of GPU time and CAN
  complete 0.3 ms after submit, yet under the former default only 49.9 % of presents carried a new
  frame. `--present-waitable` (an existing knob) took it to 99.8 %.
- **lesson:** `uniq/s`, `disp_src` and `MsBetweenDisplayChange` count DECISIONS and cannot see a
  re-presented frame; only a per-tick fresh/re-shown column can. When a rate looks right and the
  result looks wrong, suspect the instrument's blindness before the algorithm.
- **corrective:** `phyriadfg_fresh` per CSV row + `fresh:N/s`; XR15 decided 2026-09-06.

### P-004 · The registry's parser chain is at MSVC's nesting limit
- **class:** recurrence-risk · **date:** 2026-09-06 · **recurrences:** 0 · **status:** accepted
- **evidence:** adding a second `else if` case to `parse_args`'s chain in `src/control/cli.cpp` produced
  `fatal error C1061: compiler limit: blocks nested too deeply` (R5 step 1, build log).
- **lesson:** New CLI tokens go in `parse_extra`'s `if(...){ …; return 0; }` matcher, not in the
  long `else if` chain. This is a hard compiler ceiling, not a style preference.
- **corrective:** `--mv-consensus` / `--no-mv-consensus` live in `parse_extra`; noted in
  `records/R5_GATE.md` §1 as a finding for E4 (the flag-surface single-source stage).
- **accepted by:** the session — the chain is not restructured here; E4 owns that.

### P-019 · A zoo from an earlier session is not interchangeable with one from today
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** R7(a)'s panning half was going to reuse `zoo_noise` (2026-09-04, `bg noise pan 120`) rather
  than regenerate it — same generator, same seed, no reason to doubt it. `marker_extract.py` died on it with
  `KeyError: 'patterns'`. The two files' key sets differ: `zoo_static` has
  `['background','barcode','fps','frames','height','markers','patterns','seed','width']`, `zoo_noise` has the
  same list **without `patterns`**. Both were written on 2026-09-04; the generator grew the field between them,
  and the extractor now requires it.
- **lesson:** A generated corpus is only as reusable as the CONTRACT between the generator and its readers,
  and that contract moves silently — nothing in the file says which version wrote it. Reusing an old zoo is a
  premise, not a saving: check the reader accepts it (or regenerate, which for a seeded generator costs
  seconds and removes the question). `zoo_static` worked only because it happened to be written after the
  field was added.
- **corrective:** the panning zoo was regenerated with today's `marker_zoo.py` at the same parameters
  (1280×720, 60 fps, 2 s, `--bg noise --bg-pan 120 --markers 18 --sizes 6,12,24 --seed 20260904`), and the
  capture harness now takes a full path so a regenerated corpus can be pointed at without editing it. A
  version stamp in `trajectories.json` would make this mechanical; it is not written here because the file
  is an input to a bit-parity chain and changing its shape is its own change.

### P-018 · A dated audit read as a live tracker — "T2–T5 have no code" was false and shipped everywhere
- **class:** recurrence · **date:** 2026-09-06 · **recurrences:** 1 · **status:** corrected
- **evidence:** the session reported R7(a) as blocked because *"MOTION_TRUTH T2–T5 have no code"*, citing
  `records/BACKLOG_AUDIT.md` rows A6 / S2.T2–T5 (`designed`, zero code). **T2, T3, T4 and T5 all closed on
  2026-09-04**, hours after that audit was written: `S2_T2_GATE.md`, `S2_T3_GATE.md`, `S2_T4_GATE.md`,
  `S2_T5_GATE.md` sit in the SAME directory, the tools are committed at `tools/motion_truth/`, and M1 was
  already measured with `r` at `docs/evidence/MOTION_TRUTH_BASELINE.md`. The false claim reached
  `R7_GATE.md`, `ACTION_PLAN.md`, `FOTO_MENTAL.md`, the operator's desktop sequence, the durable memory,
  commit `693b02c` and a report to the operator. The session had listed that records directory earlier in
  the same session and had read the sequence log entries that closed T2–T5.
- **why it is a RECURRENCE:** `docs/FOTO_MENTAL.md` §4 already carried, in writing, *"An earlier version of
  this very paragraph said 'Nothing under CONVERGENCE or MOTION_TRUTH is built' — that was false and cost is
  the reason this warning is here."* The warning was read and the same mistake was made anyway. That is what
  distinguishes a note from a gate.
- **lesson:** `BACKLOG_AUDIT.md` is a **dated snapshot**, not a tracker; it went stale the same day it was
  written. The live state is the ACTION_PLAN's node states and, above them, the GATE RECORDS on disk — a
  file in `docs/planning/records/` is evidence that a phase closed, and it outranks any prose that says
  otherwise. Before writing "X is not built", list that directory.
- **corrective (mechanical, not a note — METACOGNITION §7):** `tools/doc_gate_parity.py`, wired as the ctest
  case `docs_gate_parity` and therefore part of `build-release.bat`. It fails the build when a LIVE planning
  document says a phase is unbuilt while a gate record for that phase exists, and when a record says it
  without carrying a staleness marker. Its first run found **three more** stale claims nobody had noticed:
  `FOTO_MENTAL.md` §2 ("everything downstream R3→R7 is unbuilt") and §4 ("NOT BUILT: R3…R7; T2…T5"), and
  `CONVERGENCE_RISK_REGISTER.md` MR-5 ("the drop-model half is R4 code and is genuinely unbuilt"). The audit
  itself now opens with a staleness banner and its six affected rows are struck as SUPERSEDED — kept, per
  the container's never-delete rule.

### P-017 · A stray `\r\r\n` suppresses git's CRLF normalisation — removing it renormalises the whole file
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** accepted
- **evidence:** `src/core/main.cpp` carries one `\r\r\n` at line 108, left by R6 step 2's patch script (P-003).
  Removing that single byte took `git diff --numstat src/core/main.cpp` from **1 0** to **1279 1278** — the
  entire file. The repo has `core.autocrlf=true` and this file's BLOB is CRLF; while the working copy contains
  an irreversible sequence git skips the CRLF→LF conversion, so the two sides matched. Clean the sequence and
  git normalises the working copy, which then differs from a CRLF blob on every line.
- **lesson:** In a repo with `autocrlf=true` and CRLF blobs, a `\r\r\n` is load-bearing by accident: it is what
  keeps a file out of git's conversion path. "Tidy up the line endings" in a file you are also editing hides a
  one-line change inside a whole-file diff, and the reviewer loses the change.
- **corrective:** the byte is LEFT IN PLACE (`status: accepted` — accepted by the session, re-openable). MSVC
  compiles it: C4335 fires on a file that is Mac-format throughout, not on one stray sequence. If it is ever
  cleaned it must be its OWN commit, touching nothing else, so the renormalisation is visible as what it is.
  Rule for this session's kind of work: **restore from the index and re-apply the intended change by bytes**
  rather than repairing a file's endings while editing it.

### P-003 · Files written by a patch script can carry `\r\r\n`
- **class:** recurrence-risk · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `flow/holons.{hpp,cpp}` were written with a double CRLF conversion (text already
  containing `\r\n`, then written with `newline='\r\n'`) → MSVC `warning C4335: Mac file format
  detected` on both files (R5 step 3a build).
- **lesson:** When a Python patch script writes a NEW source file, write `'\n'` text with
  `newline=''` and let the content carry its own endings, or normalise once before writing. The
  compiler is the only thing that notices.
- **corrective:** both files normalised; the pattern is named here.

### P-002 · A PowerShell-host launch writes UTF-16 logs
- **class:** recurrence-risk · **date:** 2026-09-05 · **recurrences:** 1 · **status:** corrected
- **evidence:** `tools/r4b/*.ps1` run from the PowerShell tool wrote `> $log` as UTF-16 LE
  (`ff fe` BOM); the same scripts launched under bash wrote UTF-8 with a BOM. The parser silently
  found zero windows on the UTF-16 logs until it was taught both.
- **lesson:** Any log parser in `tools/` reads the first two bytes and decodes `utf-16` on `ff fe`,
  else `utf-8`. A log that parses to zero rows is more likely an encoding than an empty run.
- **corrective:** `tools/r4b/r4b_parse.py` `read_log()`.

### P-001 · The plan said the holons ship OFF; they ship ON
- **class:** refuted-premise · **date:** 2026-09-06 · **recurrences:** 0 · **status:** corrected
- **evidence:** `CONVERGENCE_MASTER_PLAN.md` §R5 said the holon rows would be registered "**off by
  default**", claiming "the shipping default already has `igpu_field=false` cascading them off". The
  code: `cfg.gme`, `bidir`, `ambig`, `objects`, `scene_memory`, `inertia` are all `true`
  (`src/control/cli.hpp:413, 489, 581, 600, 621, 675`) and every run log prints them ACTIVE.
- **lesson:** R5's gate is therefore **byte-identity of the default output**, not a "rows off"
  identity — a different and stronger test. More generally: a plan sentence about what "ships by
  default" is a claim about `cli.hpp`, and is checked there before a step is built on it.
- **corrective:** `records/R5_GATE.md` §0 entry decision 1; the container ledger's L-001 carries the
  frame half of this (the collective noun "the holon family" produced two false inferences in one
  day, this being one).

*Made with my soul - Swately <3*
