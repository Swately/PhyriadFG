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

### P-041 · The ladder that places the phase slots has no upper bound, and nothing decides how many frames a pair gets
- **class:** premise refuted (a debt of [P-037](#) / [P-038](#): "the answer is in the present loop") · **date:** 2026-09-11 · **recurrences:** 0 · **status:** read in the source, first-hand; the fix is not attempted
- **evidence:** P-037 and P-038 measured the abandoned last slot (14 % at k=2, 26-35 % at k=4, 44 % at k=8)
  and owed the cause. It is four lines of `src/present/present.cpp`. `pe_j` — the rung index inside a pair —
  appears exactly four times in 3068 lines: declared `:1744`, reset `:2425` (`if(!pe_have || pair_c!=pe_pair)`),
  read `:2429` (`double te = ((double)pe_j + 0.5) / N;`), incremented `:2433` (`++pe_j;`, unconditional).

  **There is no `if (pe_j >= N)`.** No emit counter, no budget, no "pair complete" branch. The ladder is a
  pure slave: it places whatever tick arrives, and a pair stops getting frames the instant `pair_c` changes.
  `pair_c` is decided elsewhere — `clk.select()` at `:2218`, whose loop is `src/clock/phase_clock.cpp:209-214`
  and whose condition is `if((double)cand_c >= content_clock)`. **The free-running content clock crossing the
  pair's own content index is what abandons the last slot**, and nothing in the ladder knows it happened.

  Two measured facts fall out of the same four lines:
  - `N` is not k. It is `span * clk.T_robust_ms() / tick_period_ms` (`:2427`) — a MEASURED ratio of the
    source period to the present period. That is why the discovered slot grid locks at 0.883 and not at 1.0
    (P-038): N wobbles with the clock, so the rungs wobble with it.
  - `te` is clamped (`if(te>1.0) te=1.0;`, `:2431`). A pair that receives MORE ticks than N has rungs
    keeps climbing: `pe_j = 8` at `N = 8` gives `te = 8.5/8 = 1.0625`, clamped to **exactly 1.0** — a
    generated frame placed ON the next real frame. The k = 8 capture holds **45 frames at phase >= 0.99**
    and 15 pairs with nine frames; those are the same event seen from two sides.

  A previous session already met the symptom from the clock's side without connecting it to the ladder:
  `phase_clock.cpp:216-219` carries an anti-flap hysteresis whose comment names *"los saltos ±2.0 medidos en
  el CSV de cadencia"*. The ±2.0 step was measured, mitigated inside the selector, and never traced to the
  unbounded rung index it lands on.
- **lesson:** a defect that every per-frame term is blind to (P-037) was also invisible to code reading,
  because the thing to look for was an ABSENCE — a bound that is not there. Grepping a symbol and counting
  its four occurrences found in minutes what the measurement took two sessions to corner.
- **corrective:** none applied. The shipping pacing is the operator's call and the change is not a
  one-liner: bounding `pe_j` at N would drop the extra rung but would NOT emit the missing one, because the
  missing rung is a tick that never arrived. **Owed:** whether the clock can be made to hold a pair for its
  full N ticks without adding latency, and what a bounded ladder does to the 45 phase-1.0 duplicates.

### P-040 · The rotating object fails inside its own outline, the model that cannot represent it is named in the repo, and the pass that would fix it ships OFF
- **class:** premise refuted (the instrument's worst object is its best-scoring one) · **date:** 2026-09-11 · **recurrences:** 0 · **status:** measured on one corpus; the remedy is identified and NOT applied
- **evidence:** per-object interior error (|luminance| on the truth silhouette eroded 2 px, a view and not a
  verdict term), over 1692 presented frames of `sc_live k=8 fg_k8_ph8` under `coverage`:

  | object | what it does | pos px | shape px | halluc px² | **interior p99** |
  |---|---|---|---|---|---|
  | 1 sphere | translates 6.7 px/pair | 0.751 | 0.911 | 443 | 0.0063 |
  | 2 quad | static | 0.008 | 0.011 | 5 | 0.0016 |
  | **3 box** | **spins 90 °/s** | **0.323** | **0.270** | **145** | **0.1005** |

  **The spinning box has the best silhouette score in the scene and the worst interior by 16×.** Nineteen
  readers who were shown candidate / truth / difference panels with no numbers at all reproduced the
  dissociation blind: the sphere's records are `silueta_desplazada` (47 of its 84), the box's worst group is
  `patron_roto` (19 of 25) with **zero** silhouette classes, and the control group — the twelve frames with
  the LOWEST interior error — came back `sin_defecto` 10 times out of 12, mean severity 0.42 against 3.00.

  **Where it comes from, measured in three steps.**
  1. *Not the field's resolution.* The exact offset field projected onto what an 80×45 bilinear field can
     represent leaves a residual of 0.100 px rms / 0.415 px p99 on the box and 0.012 / 0.062 on the sphere.
     Real, and two orders below what follows.
  2. *The field itself is wrong, by more than the motion it measures.* Against the exact per-pair
     displacement, over 195,502 blocks: the box's true displacement is **3.35 px** and its vector error is
     **5.01 px mean, 15.03 p90**. 20.9 % of its blocks miss by more than 4 px, against 2.5 % of the sphere's,
     whose displacement is twice as large. On one frame the field carries +8.9 px of downward motion across
     the top half of the box where the truth has +0.95 (`mv1` block mean (−0.588, +8.946) vs exact
     (−2.781, +0.951)); the bottom half is correct. That is where the checker visibly breaks.
  3. *The model forecloses it.* `optical_flow_hier_match.comp:189` adds the SAME `mv` to all 64 pixels of a
     tile and `:258` stores one `vec2` per tile: no divergence, no curl, no shear. The repo says so itself —
     `framework/render/vulkan/shaders/optical_flow_affine_fit.comp:4-7`: *"the hierarchical block-match emits
     ONE TRANSLATIONAL MV per 8×8 tile. On NON-translational motion (zoom / rotation / scale) a single MV
     cannot represent the intra-tile divergence/curl → the warp samples wrong content"*. **That pass exists
     and is armed `false` at both init sites** (`src/flow/flow_init.cpp:40` and `:142`,
     `/*mv_affine*/false`), and `OpticalFlowPipeline.hpp:108` records that the in-pillar warp does not
     consume it even when armed.

  **And the delivery law decides how much of it reaches the eye.** Under the shipping default the store is
  `mix(B_samp, cur0, w_s)` with `B_samp = texture(cur, uv + mv*(1-t))`, so a vector error e arrives scaled by
  (1−t). The box's interior error falls monotonically across the discovered slots — 0.1547 at φ = 0.0625 to
  **0.0518** at φ = 0.9375, correlation with (1−φ) = +0.590, a factor of three. **The slot the generator
  abandons in 44 % of pairs (P-038) is φ = 0.9375: the cleanest frame it makes is the one it throws away.**
- **lesson:** every verdict term is a silhouette term (P-033), so the object whose silhouette is easiest —
  a box that spins in place and barely translates — scores best while looking worst. A per-object interior
  view was needed to see it, and the blind readers agreeing with it is what makes it a finding rather than a
  session's eye. Second lesson: the repository already contained both the diagnosis and the remedy, written
  by an earlier session, switched off, and unread.
- **corrective:** `masks['by_obj']` exported from `score_frame` (opt-in, output-identical: verified
  byte-identical JSON) so interior error can be measured per object at all. **Owed and NOT done:** arming
  `mv_affine` needs a consumer in the warp (the pipeline header says there is none), a build (MSVC `cl` is
  not on PATH on this rig — only cmake and glslc are), and an A/B; it is a default-affecting change and the
  operator's call.

### P-039 · The confidence gate is computed on every pixel and thrown away: three CLI flags do nothing under the shipping default
- **class:** premise refuted (a documented control is inert) · **date:** 2026-09-11 · **recurrences:** 0 · **status:** measured and read; nothing changed
- **evidence:** `--residual-ceil` ("FG gate (a): max sad_best, default 32.0", `src/control/cli.cpp:60`),
  `--conf-improv` ("FG gate (b)", `:58`) and `--agreement` build `ctx.warp_ok = cs.gate1 && tile_agrees`
  (`shaders/fg_core.comp:94`). The generated compose chain — `build-release/gen/shaders/chain_compose.glsl`,
  the code that compiles, not a comment — is three rows, all `default=1`:

  ```glsl
  vec4 c_out = core.color;
  if (L_SELECT)       c_out = pfg_compose_select(c_out, ctx);        /* rank 260 */
  if (L_STASIS)       c_out = pfg_compose_stasis(c_out, ctx);        /* rank 280 OVERRIDE */
  if (L_SINGLE_TRACK) c_out = pfg_compose_single_track(c_out, ctx);  /* rank 290 OVERRIDE */
  ```

  `select` (rank 260) is the ONLY consumer of `warp_ok` in the whole layer set (`shaders/layers/select.glsl:10`,
  `return ctx.warp_ok ? c_in : ctx.blend;`) and two rows declared OVERRIDE discard `c_in` after it.
  **Two independent confirmations, neither of them a reading of the code:**
  - Sweeping `residual_ceil` over 32 / 8 / 4 / 2 and `improvement_frac` over 0.20 / 0.50 through the
    validated CPU reference, scored against exact truth on 433 frames across two disjoint laps, moved
    **every term of every object by nothing at all** — identical to four decimals, both laps.
  - `tools/ref_warp.py` does not implement Gate 1 anywhere (`residual_ceil` appears only in its push-name
    list) and reproduces the GPU's own stored output at **99.47 % exact / 100.00 % within 1 LSB**. A
    reproduction that omits the gate entirely could not match if the gate did anything.

  What is left deciding warp-vs-hold is one scalar: `w_s = smoothstep(1.2, 3.0, (d_pixel+0.02)/(d_zero+0.02))`
  (`shaders/layers/single_track.glsl:1326`), and it is the one a periodic texture is built to fool. On the
  box's damaged blocks the ratio is **2.28** against **0.98** on its sound ones — the signal is there and the
  curve sits above it: **81.4 % of the damaged pixels still receive w_s < 0.5** and take the warp nearly
  intact.
- **lesson:** a knob with a help string, a default, a parser entry and a push-block slot can still be
  disconnected from the output, and every one of those is evidence of intent rather than of effect. The
  sweep that found it was run to tune the gate, not to test whether it was connected; the flat result was
  the finding. A tuning sweep whose every arm is identical is not a null — it is a wiring check that failed.
- **corrective:** the one gate that IS connected is now tunable instead of hard-coded, at both ends. Offline:
  `tools/ref_warp.py` gained `WS_EDGE0` / `WS_EDGE1` (defaults 1.2 / 3.0, parity re-verified at 99.45 % exact)
  so it can be swept against exact truth without a build. In the product: `single_track.hold_lo` /
  `hold_hi` are layer parameters with `--st-hold-lo` / `--st-hold-hi`, **defaulting to the shipping literals**
  — the layer dump reads `hold_lo=1.200 hold_hi=3.000` and the expression is unchanged by construction (byte
  identity on a rendered frame is NOT measured; that needs a live capture). The contract hash moves
  0x9517AE73A530EAFE -> 0xC4D941C47BF7EE5B, which is the stamp doing its job. **Owed:** whether `select`
  should run after the overrides, or `single_track` respect `warp_ok` — a change to the shipping composition
  and the operator's call; and either the three dead flags get wired or their help text stops promising a gate.

### P-038 · The generator targets k phase slots, not k-1, and abandons the last one in every capture the project holds
- **class:** premise refuted (twice: the slot count, and the reach of the finding) · **date:** 2026-09-11 · **recurrences:** 1 (P-037 is the same defect on one corpus) · **status:** measured on 16 every-tick captures; the cause in the present loop is still not read
- **evidence:** P-037 measured the dropped last slot on ONE corpus at ONE multiplier and said so. The
  presented-sequence page built for `sc_live · k = 8 · fg_k8_ph8` (4488 stored ticks, all of them, none
  deduplicated) made the same measurement cheap everywhere, because it needs no rendering: N, N1, the FG's
  own t and the presentation order are all in `align.json` and `gdump_map.tsv`. Two premises fell.

  **First: the slot count is k, never k - 1.** A multiplier of k implies k-1 generated frames between two
  real ones, and the prompt this page was written from asserted 7 slots at k = 8 for that reason. The
  capture locks to EIGHT, at (2j+1)/16, with a grid concentration of 0.883 and a mean residual of 0.042
  base frames. The same test returns 4 at k = 4 and 2 at k = 2. Sixteen every-tick captures, three
  multipliers, five corpora, two seeds, two speeds: the answer is k every time.

  **Second: the drop is everywhere, and it is the LAST slot everywhere.** Percentage of pair
  presentations missing the final slot, one every-tick capture per row, the rest of the slots missing in
  0-6 %:

  | corpus | arm | k | ticks | last slot missing | frames/pair | double steps |
  |---|---|---|---|---|---|---|
  | sc_live | `fg_k2_ph2` | 2 | 4260 | **14 %** | 1.89 | 7.1 % |
  | g5_live | `fg_k4` | 4 | 1805 | 26 % | 3.75 | 7.0 % |
  | sc_v4 | `fg_k4_v4` | 4 | 4500 | 27 % | 3.76 | 7.0 % |
  | sc_live2 | `fg_k4_s11mvg` | 4 | 4501 | 27 % | 3.75 | 7.1 % |
  | sc_v2 | `fg_k4_v2` | 4 | 4090 | 27 % | 3.74 | 7.2 % |
  | sc_live2 | `fg_k4_s11base` | 4 | 4338 | 28 % | 3.75 | 7.2 % |
  | sc_live | `fg_k4_base` | 4 | 4382 | 28 % | 3.75 | 7.3 % |
  | sc_live | `fg_k4_nocand` | 4 | 4467 | 28 % | 3.74 | 7.4 % |
  | sc_live | `fg_k4_mvg` | 4 | 4465 | 30 % | 3.72 | 7.9 % |
  | sc_live | `fg_k4_nostasis` | 4 | 4250 | 30 % | 3.72 | 8.0 % |
  | sc_live | `fg_k4_noambig` | 4 | 3875 | 32 % | 3.70 | 8.5 % |
  | sc_live | `fg_k4_smooth` | 4 | 4272 | 32 % | 3.69 | 8.5 % |
  | sc_live | `fg_k4_nocand2` | 4 | 4094 | 33 % | 3.69 | 8.8 % |
  | sc_live | `fg_k4_prior` | 4 | 3796 | 35 % | 3.66 | 9.4 % |
  | sc_live | `fg_k8_ph8` | 8 | 4488 | **44 %** | 7.46 | 6.1 % |
  | sc_live | `fg_k8_ph8mvg` | 8 | 2174 | **85 %** | 5.20 | 25.8 % |

  Read across, not down. **The drop scales with k** (14 / 26-35 / 44 %) and is flat in everything else:
  two seeds, two scene speeds and two corpora give 26-28 % for the plain default, and the eight MV knobs
  span 28-35 % with no knob outside that band. A defect that ignores the seed, the speed, the corpus and
  every knob of the warp is not in the warp. P-037 conjectured the present loop; this is the evidence for
  it, and the reverse test is in the table: `--no-mv-candsel`, `--mv-prior`, `--mv-smooth`, `--no-ambig`,
  `--no-stasis` and `--no-mv-guided` all change the PICTURE and none of them moves the pacing out of band.

  **What it costs on the screen.** 6-9 % of every presented frame advances the scene twice as far as the
  one before it. Zero reversals in any capture. At a 240 fps presented rate that is 15-22 double steps per
  second, and every one of those frames is CORRECT for its own phase: `fg_k8_ph8` scores pos_err 0.316 px
  under `coverage` and the report's verdict on it is ACCEPT.

  **The outlier is a lead, not noise.** `fg_k8_ph8mvg` -- the `--no-mv-guided` ablation at k = 8 -- loses
  the last slot in 85 % of pairs, runs 5.20 frames per pair against 7.46, and its phases barely lock to
  the grid at all (0.540 against 0.883). It also stored 2174 ticks where its twin stored 4488. Whatever
  the present loop is doing, that arm does much more of it; it is one run and it has not been repeated.
- **lesson:** a count the multiplier implies is a premise, not a measurement, and it was wrong by one in
  the very document written to guard the measurement. The estimator that caught it assumes only that the
  slots are evenly spaced and asks the data how many there are -- and the first version of it, which split
  sorted phases on gaps, returned 7 and hid the drop by merging the last slot with its own jitter tail. An
  instrument that discovers a grid must be shown a case where it would report the wrong grid.
- **corrective:** `scene_step.py --presented` (every stored tick, in presentation order, nothing
  deduplicated, nothing real) with the pacing block on the page; `slot_grid()` as the named estimator with
  its lock reported beside every census, because a capture whose phases do not lock has no slots to be
  missing. The page for `sc_live k=8 fg_k8_ph8` is `F:\Phyriad\scene_pages\ph8_k8\presented` -- 4488
  frames, 264 double steps, each one reachable by a chip or the J key. **Still owed:** why the last slot
  is abandoned (the present loop, not the warp -- now with a reverse test saying so); a term over
  CONSECUTIVE presented frames, which would be the first term this instrument has that is not per-frame;
  and a second run of the `fg_k8_ph8mvg` outlier.

### P-037 · The generator drops the last slot of one pair in four, and every term the instrument has is blind to it
- **class:** premise refuted (the instrument's population was never the presented sequence) · **date:** 2026-09-11 · **recurrences:** 0 · **status:** measured on ONE corpus at ONE multiplier; generalised by P-038, which finds k slots rather than k-1 and the same abandoned last slot in sixteen captures
- **evidence:** the operator kept reporting displacements and hallucinations on screen that the review pages did
  not contain, and finally described it exactly: "the motion is correctly presented but frame by frame it shifts
  in presentation time". Four hypotheses were tested and killed first — the tap misses frames (it records
  1805 of 1805 presents), the discards are phase-biased (mean phase 0.439 scored vs 0.526 discarded), the
  "duplicates" are not duplicates (69 % are within 0.02 of the kept frame's phase), and the generator is
  non-deterministic across loop passes (the same instant reproduces to 0.001/255). All false.
  **What is true.** At k = 4 the generator does not emit the 3 frames the multiplier implies at 0.25/0.50/0.75.
  It targets FOUR interior slots — 0.125, 0.375, 0.625, 0.875 — and over 481 presented pairs of `g5_live`:

  | slot | missing in |
  |---|---|
  | 0.125 | 0 % |
  | 0.375 | 0 % |
  | 0.625 | 1 % |
  | **0.875** | **26 %** |

  127 of 481 pairs (26 %) drop at least one slot, almost always the last. A dropped slot is a presented frame
  whose scene time advances TWO base frames instead of one: on a contiguous six-pair run the advance is 1.000
  in twenty of twenty-one steps and 1.986 / 2.055 in the two that follow a dropped slot, with zero reversals.
  At a 60 fps source that is about **16 double-steps per second** — the object jumping twice its distance, 16
  times a second, while every individual frame is correctly placed for its own phase.
  **Why nothing saw it.** Every one of the six verdict terms scores ONE frame against the truth rendered at
  THAT frame's phase. A frame emitted at 0.625 when 0.875 was due is not wrong — it is a correct frame at
  0.625. The defect exists only in the RELATION between consecutive presented frames, and the instrument has
  no term over pairs of frames. The review page compounds it twice over: it walks the base grid rather than
  the presented order, and it marks every k-th frame REAL and loads the corpus's own source frame for it, so a
  quarter of that page is the truth itself, exact by construction, while the panel shows only synthesised frames.
- **lesson:** an instrument that scores frames cannot see a defect that lives between frames, and a viewer that
  reconstructs an idealised ladder cannot show one either. Both were built to answer "is this frame right?" and
  both answer it correctly. The operator was looking at the thing they were not built to show, which is why he
  could see it for days and the numbers could not. Second lesson, paid twice in this session: he found the
  three discontinuities in the first version of this very test and located them exactly — they were the seams
  of MY OWN pair selection, which had picked six pairs by frame count rather than contiguity. A test for a
  continuity defect must itself be continuous.
- **corrective:** the presented-sequence page (`F:\Phyriad\scene_pages\presentado_g5\step`, six contiguous
  pairs, every frame the tap recorded, in presentation order, each with its exact-phase truth and masks).
  Owed: why the last slot is abandoned — the candidates are the clock that paces the slots and the arrival of
  the next source frame, and the answer is in the present loop, not in the warp; a term over CONSECUTIVE
  presented frames, which is the first term this instrument would gain that is not a per-frame term; and the
  same count on the other corpora, since 26 % is one run on one corpus.

### P-036 · The knob that costs 42 % at one displacement pays for itself at twice that, and the sign flips with phase
- **class:** premise refuted · **date:** 2026-09-11 · **recurrences:** 0 · **status:** the default was NOT flipped; the evidence says it should not be
- **evidence:** the operator, after seeing the colour-guided MV pick cost 42 % of the position error at k = 4,
  said to apply it as the default and re-run the same scene to compare. The comparison run refutes the flip.
  Same corpus, same every-tick tap, one run per arm, n = 203 each, scored under `coverage`:

  | | k = 4 (3.4 px/pair) | k = 8 (6.7 px/pair) |
  |---|---|---|
  | default | 0.213 px | 0.316 px |
  | `--no-mv-guided` | 0.124 px | 0.316 px |
  | change | **−42.0 %** | **−0.2 %** |

  At k = 8 the net is zero because the sign flips inside the pair, and the flip is clean at n = 29 per bin:

  | phase | default | `--no-mv-guided` | change |
  |---|---|---|---|
  | 0.125 | 0.645 | 0.690 | **+7.0 %** |
  | 0.250 | 0.473 | 0.508 | **+7.5 %** |
  | 0.375 | 0.341 | 0.358 | +5.0 % |
  | 0.500 | 0.234 | 0.271 | +15.6 % |
  | 0.625 | 0.213 | 0.177 | −16.8 % |
  | 0.750 | 0.183 | 0.111 | **−39.2 %** |
  | 0.875 | 0.125 | 0.094 | −24.6 % |

  Removing the layer HURTS the first half of the pair and HELPS the second, and the two halves cancel.
  This lines up with the mechanism read from the shaders (P-034): `phase_anchor.glsl:11-14` switches the
  vector source from the forward field to the backward one by `smoothstep(0.35, 0.65, t)`, and `cli.hpp:491`
  states that `mv_guided` weights the 3x3 consensus by colour membership on BOTH fields. So the colour-guided
  consensus helps the forward field at large displacement and hurts the backward one, and which effect wins
  depends on the phase — and, through the displacement, on k.
- **lesson:** a knob measured in ONE regime is a knob measured in one regime. The k = 4 result was two seeds
  and a 42 % effect, which felt like enough to move a shipping default; one capture at twice the displacement
  showed the effect is not a property of the knob but of the knob crossed with phase and displacement. The
  project's own scenario matrix exists to say exactly this, and the session was one command away from
  flipping a default on evidence from a single point of it.
- **corrective:** the default is untouched. What a flip would take, mapped and recorded so the decision is
  cheap when it is taken: `layer_table.def:43` (the layer row), `cli.hpp:491` (the struct field),
  `cli.cpp:591` and `:645` (the two flag handlers trade places), the three help lines at `cli.cpp:15, 23, 130`,
  and `CMakeLists.txt:268`, which pins the contract hash `0x9517AE73A530EAFE` as a test and would have to be
  updated to the value the build prints. The frozen paper identity also names the object as "the shipping
  default kernel as configured by its default flags", so a flip makes every existing row a measurement of a
  different object — the identity already handles that by pinning a build per row, but the record must say so.
  What is owed before any flip: the same A/B at ×2 and ×4 speed, and a second seed at k = 8.

### P-035 · The give-back that holds the panel: the plane watchdog and the thread that joins it deadlock each other on a normal quit
- **class:** premise refuted (a hazard the repo had written down was fixed on one path and left open on the other) · **date:** 2026-09-11 · **recurrences:** 1 (R4c, the TDR path) · **status:** fixed, unverified live
- **evidence:** the operator reported the FG frozen with its window scaled to fullscreen while the process still
  ran. It was blocked, not working: CPU pinned at 17.2 s for over two minutes, 852 handles, not responding, and
  the panel held until it was killed. Its log ends on the every-tick tap's summary — `4494 captured / 4494
  recorded ticks, 1195 pairs written, 3950.8 MB` — and never prints the `bounded-run clean exit` line that every
  healthy run of that session printed next. So the wedge sits between those two.
  **The mechanism, read first-hand.** `PresentSurface.cpp:305-306` was `wd_run.store(false); if
  (wd_thread.joinable()) wd_thread.join();` — an unbounded join, with the message pump fifteen lines BELOW it at
  `:321-323`. The watchdog it joins (`:537-551`) calls `yield_plane()` (`:253-258`), whose `SetWindowPos` and
  `ShowWindow` act on a window owned by the present thread: across threads those are inter-thread sends that
  block until the owner dispatches. The owner is the very thread sitting in the join. The watchdog arms whenever
  the heartbeat stops for more than `kWatchdogStallMs = 250` (`:135`) **with the plane still displayed**
  (`:539` `if (impl->yielded.load()) continue;`), and a `--gdump` teardown drains a multi-gigabyte ring, which
  takes about a second. Both threads then wait on each other at zero CPU, the window is never hidden, and
  `DestroyWindow` at `:319` is never reached.
  **The repo already knew.** `src/core/main.cpp:1050-1052`, from the R4c hardening: "the pillar's OA-10 watchdog
  cannot hide a window whose owning thread is wedged (ShowWindow/SetWindowPos from another thread wait on that
  thread's message loop)". That pass gave the joins a 3 s deadline **only once the device is LOST**, and says so:
  "On a normal quit (no loss) the joins are the unbounded ones they were — byte-identical." This wedge was a
  normal quit, so none of that hardening applied, and the deadlock was in the pillar's own `destroy()` rather
  than in the joins R4c fixed.
  **The flag is a coincidence, checked rather than assumed.** The run carried `--no-mv-candsel` and the three
  runs before it had exited cleanly, which made the flag look causal. `mv_candsel` is read at exactly two sites,
  `src/flow/flow_init.cpp:40` and `:142`, both flow-pipeline construction; nothing in the present, instrument or
  teardown paths reads it. What actually varies per run is whether the FG's plane was DISPLAYED or YIELDED when
  the deadline hit — the watchdog's own guard at `:539`.
- **lesson:** a give-back must not be able to block. The watchdog exists to return the operator's panel when the
  present thread wedges, and it became the reason the panel was not returned. And a hazard fixed on the path
  where it was DISCOVERED is not fixed: R4c found this exact interaction under forced TDR, bounded the joins it
  was looking at, and wrote the mechanism down in a comment — while the same mechanism sat in the pillar's
  ordinary teardown, one file away, reachable by any run whose shutdown outlives 250 ms.
- **corrective:** `PresentSurface.cpp` `destroy()` now pumps this thread's message queue while waiting for the
  watchdog, on a 2 s deadline, and detaches instead of joining if it does not come back, so the teardown always
  reaches `DestroyWindow`. A `wd_done` flag published by the watchdog's own loop is what `destroy()` waits on.
  Built clean, 51/51 tests pass. **NOT verified live:** reproducing the wedge needs a capture that ends with the
  plane displayed, which is the operator's screen. Owed: that one run, and a decision on whether the watchdog
  should post an asynchronous message instead of a blocking one, which would remove the hazard rather than
  bound it.

### P-034 · The operator's eye found the phase law the instrument's own terms had already measured and nobody had read as one thing
- **class:** re-derivation + attribution · **date:** 2026-09-10 · **recurrences:** 1 (the phase shape is M1_LOWPHASE, 2026-09-04) · **status:** attributed, the default flip is the operator's
- **evidence:** the operator walked the every-tick page and reported, unprompted, that a checker deformation is
  "born" at certain frames, "lasts one extra frame" and "resolves" two frames later, naming 33/35, 37/39,
  185, 189/191, 193/195, 197/199. Every frame he called a birth is ≡ 1 (mod 4) and every one he called resolved
  is ≡ 3 (mod 4); of the 24 worst frames by interior error, 17 sit at ≡1, 7 at ≡2 and **none** at ≡3. Against
  the generator's OWN reported phase (not the grid), interior p99 falls 0.0515 → 0.0447 → 0.0347 → 0.0286 across
  t bins, Pearson −0.516 over 177 frames; position does the same, −0.462. **The "extra frame" is not the warp:**
  the FG emitted frames 33 and 34 at t = 0.3746 and 0.3793, four thousandths apart, so it drew nearly the same
  image while the scene moved (candidates differ by 0.004/255, truths by 0.146). 30 of 124 consecutive pairs are
  under 0.05 of a pair apart in t. That is the clock, not the kernel.
  **The cause, read first-hand in the shaders and then ablated live.** Under the shipping default there is no
  A/B blend at all: `single_track_wa.glsl:7` returns 0.0 and `single_track.glsl:7-14` is a declared OVERRIDE that
  discards the blended colour and rebuilds the pixel from `B_samp = texture(cur, uv + mv*(1-t)/out_size)`
  (`fg_core_math.glsl:55,58`). The generated frame is the NEXT real frame resampled by `mv*(1-t)`, so every
  motion-vector error reaches the screen multiplied by (1-t): maximal at t = 0, zero at t = 1. A least-squares
  fit over the 177 frames gives interior p99 = 0.0226 + 0.0343·(1-t), which reproduces the bin means to within
  0.002 — but the intercept is 40 % of the value at t = 0, so the law explains the trend, not everything, and
  per-frame R² is only 0.267. Which field supplies `mv` is itself phase-switched:
  `phase_anchor.glsl:11-14` mixes the forward and backward fields by `smoothstep(0.35, 0.65, t)`, so below
  t = 0.35 the vector is the forward field after the guided pick and inertia, and above t = 0.65 it is the raw
  backward field. **Paired live A/B, same corpus, same session, every tick, n = 177 each, one run per arm —
  reliability not measured:**

  | arm | pos px | shape px | halluc px² | missing px² | interior p99 | (1-t) slope |
  |---|---|---|---|---|---|---|
  | default | 0.226 | 0.148 | 53 | 26 | 0.0398 | 0.0357 |
  | `--no-mv-guided` | **0.078** | **0.079** | 39 | **5** | 0.0345 | **0.0167** |

  Turning the guided pick off costs 42 % of the position error and **halves the phase-dependent part of the
  interior deformation** (the slope 0.0357 → 0.0167) while leaving the phase-independent floor alone
  (0.0212 → 0.0256). At the same time it drops the position error BELOW the exact-flow oracle's 0.097 px.
  The low-t bin moves 0.0520 → 0.0399 while the high-t bin does not move (0.0252 → 0.0267) — exactly what
  `phase_anchor` predicts, since the guided pick only feeds the field that is used below t = 0.35.
- **lesson:** the record already held this. `M1_LOWPHASE_FINDING.md` measured the same phase shape on position
  in September and named the same layer, and `B1_FIRST_FG_ROW.md` §2 printed the per-phase table. What was
  missing was not measurement but a VIEW: nobody had looked at a generated frame beside its truth, so a fact
  the tables carried for days arrived instead from a human eye on a picture. A term reported per corpus and a
  term reported per frame with the frame next to it are not the same instrument.
- **corrective:** the interior view (P-033) and the six every-tick captures now on disk. Still the operator's:
  `mv_guided` is default ON and its own project record now shows it costing a factor of 2.9 in position against
  exact 3-D truth on the shipping async path. Owed before that becomes a recommendation: a second seed for each
  arm of the A/B, and the same pair at ×2 and ×4, since both arms here are one run.

### P-033 · Every verdict term is a silhouette term, so a warp that keeps the outline and deforms the interior scores at the floor
- **class:** premise refuted · **date:** 2026-09-10 · **recurrences:** 0 · **status:** a view exists; whether it becomes a term is the operator's
- **evidence:** the operator, looking at the new frame-by-frame pages, pointed at the spinning box and asked whether
  the session could see the checker deforming. It can be seen and the instrument does not count it. On `sc_live`
  k = 4 frame 193, the frame with the WORST whole-frame image error of the arm (`l2_det` 0.0095), the box scores
  `pos_err` **0.043 px** and `shape_err` **0.096 px** against a 0.5 px tolerance, `halluc` 46 px² and `missing`
  19 px². It passes every object term by a factor of twenty while its interior lattice is visibly bent. The cause
  is structural, not a bug: `pos_err` is a centroid, `shape_err` a chamfer between boundaries, `halluc` and
  `missing` are silhouette set-differences, and `sharp` is edge strength on a one-pixel band around the truth's
  own boundary (`scene_report.py` `tb = (ids>0) & ~erode4(ids>0)`). None of the six looks strictly inside the
  silhouette. `bg_err` is backdrop-only. `l2_det` does see it, but over the whole determinable frame, never per
  object, and by the identity's own text it is "reported beside the six and never folded into the verdict".
  Measured on the same arm, the relation runs the wrong way: frames 193 and 197 carry the arm's LOWEST position
  error (0.112 / 0.095 px) and its HIGHEST interior error (p99 0.0858 / 0.0865), while 191 and 195 are the
  reverse. The verdict likes best the frames whose interiors are worst.
- **lesson:** a decomposition is blind exactly where none of its terms is defined, and the gate cannot find that
  blind spot because the gate's five synthetic arms are built from the corpus itself — `blend`, `blur`, `nearest`
  and `oracle2` all produce artefacts on or across the boundary, so every predicted signature the gate checks is a
  signature the terms already cover. An instrument validated only against the failures it was designed for will
  pass while missing a whole artefact class. This one was found by a human looking at a picture, which is the
  argument for the frame-by-frame view existing at all. Related: this is the `mixed` corpus, and the checker is a
  repetitive pattern — the artefact class F2 was written to probe, arriving before F2 was ever captured.
- **corrective:** `scene_step.py --overlay` now writes an interior map per generated frame (key `I`): the
  luminance error on the truth's silhouette eroded by 2 px, which excludes the boundary band the terms do cover,
  with its rms / p99 / max in the panel labelled as entering no term, a "worst by interior error" chip row, and
  the value carried into the exported feedback line. `score_frame(masks=...)` exports `truth_obj` for it. **It is
  a view and a reported-apart number, NOT a seventh verdict term:** the identity's priority order of the terms is
  frozen, so adding one re-opens Phase 1 and that is the operator's deliberate act (KAP §7). What a term would
  have to answer first: whether the right quantity is interior luminance error, or the chamfer between the
  candidate's and the truth's INTERIOR edge maps, which measures how far the pattern moved rather than how much
  it differs, and which would not fire on a legitimate shading difference.

### P-032 · The prior-art sweep never entered the field the object lives in, and six of its seven absences fall
- **class:** premise refuted · **date:** 2026-09-10 · **recurrences:** 0 · **status:** recorded; re-opening Phase 1 is the operator's
- **evidence:** the object under test is a REAL-TIME frame generator in a present pipeline — the DLSS-FG / FSR-FG
  category. `FG_METRIC_MODEL_PRIOR_ART.md`'s five sweeps (Q1 distribution metrics, Q2 perceptual metrics, Q3
  synthetic truth, Q4 reference-free prediction, Q5 decomposition) are all inside the video-frame-interpolation and
  image-quality literature. A grep of the dossier for `extranet|dlss|fsr|g-buffer|extrapolat|siggraph|i3d|hpg`
  returns three hits, none of them a source row: **41 sources, zero from real-time graphics.** §7 declares the
  searches time-boxed but never declares the domain boundary. A two-angle adversarial sweep (2026-09-10, ten
  agents, every claimed paper re-opened by a separate confirmer) found the field and refuted six of the seven
  absences:
  - **N1** (no evaluation at the generator's own phase against a 3-D truth) — refuted. *Amulet*, arXiv 2608.10423
    (11 Aug 2026), §7.4 verbatim, re-fetched first-hand by the supervisor: "We compare the rendering times and the
    visual quality of extrapolated frames using ground-truth images created with standard deferred rendering for
    every frame", with "Amulet and DLSS Frame Generation run live in the Falcor engine" and a per-n quality curve
    (Fig. 9), metrics PSNR / SSIM / FLIP / LPIPS. Peer-reviewed precedent: *Mob-FGSR* (SIGGRAPH 2024) generates and
    references frames "at desired times" between two rendered frames; *Image-Based Bidirectional Scene Reprojection*
    (SIGGRAPH Asia 2011) re-renders its reference at t+0.25 / 0.5 / 0.75.
  - **N2** (the four-way visibility taxonomy) — refuted by Yang et al. 2011 §4 (visible in both / only one / neither).
  - **N3** (a metric reporting its own run-to-run reliability) — refuted on the generative half by *The FID Lottery*,
    arXiv 2606.20536 (June 2026). Not found for a fidelity metric over corpus randomness: that half stands.
  - **N4** (self-diagnosis from the interpolator's own decisions) — refuted by Plack et al., *Frame Interpolation
    Transformer and Uncertainty Guidance*, CVPR 2023: the network estimates "the expected error together with the
    interpolated frame" and the estimate steers partial re-rendering. This is candidate I-B, already published.
  - **N5** (a named position-error column in px) — refuted by US 6,064,393 (Lengyel, Snyder, Kajiya, 1997): the
    geometric error of a warped frame in pixels, plus the NIST rendering-metrology report.
  - **N6** (a pixel-scale sensitivity floor for the perceptual family) — refuted by Alabau-Bosque et al.,
    arXiv 2407.17927 (2024), which gives translation-invisibility thresholds per metric. **The dossier's own §7
    named this paper as "the one most likely to change N6 if read" and it was not read.**
  - **N7** (KID/CMMD in a VFI paper) — stands.
- **lesson:** an absence is bounded by its search box, and a search box is bounded by the FIELD it was drawn in. The
  question arrived in the operator's words as "a model of the FID / CMMD / IS / LPIPS type", and that framing chose
  the literature for five sweeps, three gates, a frozen identity and a drafted paper — while the artifact under test
  had never been a video-interpolation problem. Nobody re-asked "which field publishes about THIS object". A frame
  that arrives with the question is the hardest to see (METACOGNITION §: an inherited frame organising the work).
  Second lesson, cheaper: when a dossier names a specific unread source as the one most likely to overturn a
  finding, that source is not a footnote, it is the finding's open flank.
- **corrective:** recorded beside the frozen block, not in it (KAP §7: re-opening Phase 1 is the operator's
  deliberate act). Clause (b) of the identity — "the swept literature holds no evaluation of an interpolation at the
  generator's own phase against an analytic 3-D truth, no four-way visibility taxonomy, and no run-to-run
  reliability of a metric" — is refuted in its first two parts and survives only in the third, and only for a
  fidelity metric. The paper's §01.1, §02.6 and §07 carry a dated correction block naming these papers. What the
  attack did NOT refute, and what the work therefore still holds: the six-term decomposition with the disocclusion
  bucket scored apart, the conjunctive per-object verdict, the five-arm gate seen red first, and a run-to-run
  figure on every number — no paper found does that combination, and every graphics paper found evaluates with
  aggregate PSNR / SSIM / LPIPS / FLIP.

### P-031 · A gate series printed as one speed progression was spliced across two scene families, and two of its points were verdicts on n = 2 and n = 4
- **class:** correction to the record · **date:** 2026-09-10 · **recurrences:** 0 · **status:** corrected
- **evidence:** `REGIME_TEST_MATRIX.md` §9 stated the T3 residual "grows with displacement — 0.06 (×1), 0.11 (×2),
  0.26 (×4), 0.57 px (×8)". The first three are `mixed` (`sc_live`, `sc_v2`, `sc_v4`); the fourth is `fast_train`
  (`sc_train8_s7`), a different scene family built four hours earlier. The coverage gate had never been run on
  `mixed` at ×8. Run tonight on the eight corpora that lacked it: `sc_v8_s7` / `_s11` give T3 blend pos 0.054 /
  0.057 px and PASS, not 0.57 — because T3 reads only the (frame, object) pairs no other object reaches into
  (`scene_report.py:711`), and on `mixed` that sample collapses 42 → 21 → 10 → **4** → **2** pairs as speed rises
  and the sphere leaves the view. The ×8 pass rests on 4 pairs and the ×16 failure on 2.
- **lesson:** a series is a claim about ONE thing varying. Printing four numbers with a speed label made a
  cross-corpus splice look like a law, and it survived a T2 structural gate and three first-hand reads — mine
  included — because each number was individually true and correctly sourced. Truth per cell does not make a row.
  And a gate verdict carries its n: PASS on 4 pairs is not a pass, it is an absence of measurement.
- **corrective:** the twelve-corpus table with an explicit `n (T3)` column is now in the paper's §03.6, and §05.3
  carries the `mixed` half of it with the collapse stated; `REGIME_TEST_MATRIX.md` §9 is corrected. The finding that
  survives, and it is stronger than the splice was: on `fast_train`, built so that a sphere is always fully inside
  the view (35 and 24 overlap-free pairs), T3 fails and **reproduces across two seeds** — 0.568 / 0.568 px at ×8 and
  1.105 / 1.100 px at ×16. T2 passes on all twelve corpora, p90 0.014–0.037 px.

### P-030 · The matrix's marker families were planned as the regime tests; under the frozen identity a marker row cannot be a verdict row
- **class:** premise refuted · **date:** 2026-09-10 · **recurrences:** 0 · **status:** recorded, the build is the operator's
- **evidence:** `REGIME_TEST_MATRIX.md` §2 assigns the regimes "thin objects" (family 1), "repetitive patterns"
  (2), "low fps" (4) and "erratic" (6) to the marker instrument (`tools/motion_truth/`), whose terms are
  `err_model` / `err_true` / `phase_ms` and the per-class counts found / degraded / absent / ghost
  (`MOTION_TRUTH_BASELINE.md`). The frozen identity (`FG_METRIC_MODEL_SPINE.md`, block INTACT) makes the
  verdict "exactly as `scene_report.py` `verdict()` defines" it — the six terms per object — and the
  scenario set "analytic scene families, each with closed-form pose(t)"; so the paper's Results slots for
  F1-erratic, F2 and F4 have no instrument that can fill them today. Found while laying out the Phase 2
  scaffold (`docs/planning/paper/SCAFFOLD.md`, structural decision 5); the T2 auditors cleared the
  demotion and the round-2 auditor confirmed nine families and no tenth.
- **lesson:** a plan written before an identity was frozen carries premises the freeze can retire. The
  matrix was designed 2026-09-10 02:30, the identity froze 2026-09-09 — the matrix was later, and still
  it inherited "each regime has an instrument" from the instrument that existed, not from the terms the
  identity had just fixed. A regime test enters the paper only in the identity's terms; a second
  instrument corroborates, at the altitude its own records state, and is never a verdict row.
- **corrective:** the scaffold denies the marker chain a Results slot (§06.3 corroboration, §03.8 method)
  and gives F1-erratic / F2 / F4 `BUILD` slots that name what is missing — a closed-form reversing preset,
  a thin preset and `period_backdrop` (already in the matrix as family 7), a base rendered at the low rate
  — each a preset in `scene_zoo.py`, the operator's instrument: announced, not built. The marker corpora
  and their captures stay useful as the fast pre-screen the matrix designed them to be.

### P-028 · A dry-run that mutated: the session's own -DryRun deleted a KEEP run's raw capture
- **class:** refuted-premise · **date:** 2026-09-10 · **recurrences:** 0 · **status:** corrected
- **evidence:** `tools/scene_truth/scene_live.ps1` gained `-DryRun` on 2026-09-10 (commit `7f2c815`);
  the block was inserted after the script's existing `if (Test-Path $qd) { Remove-Item -Recurse -Force
  $qd }`, so the three dry-runs the session ran at 01:59 to verify the new flags emptied
  `C:\PhyriadFG\runs\sc_live\qdump_k4` (the raw k=4 sampler capture behind `B1_FIRST_FG_ROW.md`: 351
  triples, the four worst-frame provenance planes of `B1_SPEED_TEST.md` §4, the six cut triples) and
  created two empty tag directories. Found by the cut-scoring validator of `wf_54bcf616-271` ("qdump_k4
  is empty, mtime 01:59:26"). The aligned arm `arms/fg_k4/` (136 frames) and `fg_k4.json/.md` survive,
  so B1's numbers stand; the only copy elsewhere (`sc_bc/qdump_k4` in the 2026-09-08 session's
  scratchpad) is a different corpus (40 triples). The raw capture is gone: the provenance replay on that
  run and its cut frames are not reproducible; a re-capture is a new sample. The same misplaced block
  sat below the player's Start-Process too, so every dry-run left an "RA Motion Zoo" window looping on
  the operator's screen — seven of them (five on sc_live 01:58-01:59, two on sc_live2 02:34), noticed by
  him ("hay multiples zoo abiertos") and closed by the session at 02:50.
- **lesson:** a dry-run is a promise about the disk, and the promise is checked by the file counts
  before and after, not by reading the script. The order of statements in a runner is a safety property:
  nothing that mutates may precede the dry-run exit, and a run the operator marked KEEP (`scene_runs.py
  keep`) is refused an overwrite by the TOOL, not by the session's memory of the marker.
- **corrective:** `scene_live.ps1` now exits on -DryRun before any Remove-Item/New-Item (proved on
  `sc_live2`: 5473 files before and after), refuses to overwrite a capture directory of a KEEP run
  unless `-Overwrite` is passed (proved: the throw, 5473 files still), and a tag never collides;
  `marker_live.ps1` was written with the dry-run block first. The loss is recorded beside the row it
  affects (`B1_FIRST_FG_ROW.md` §5 addendum).

### P-029 · The scorer's gate was run on one corpus and every other corpus was scored on trust
- **class:** dormancy · **date:** 2026-09-10 · **recurrences:** 0 · **status:** corrected
- **evidence:** `scene_report.py --gate` (T1-T5, "every check seen RED first") was run on the k = 4 x1
  corpus for `B1_FIRST_FG_ROW.md`; the speed-test corpora were scored without it. Run tonight, pooled:
  `sc_v2` (6.7 px/pair) FAILS T2, T3 (nearest residual p90 0.387 px, blend pos 0.464), `sc_v4` (13.3
  px/pair) FAILS T2, T3 (0.771 / 1.056 px), every x8/x16 corpus FAILS T2, T3, T4 (residual 2.2 px at x8,
  the exact-flow oracle 0.9-1.3 px of shape). Diagnosis (`gate_diag`, one frame per arm per object): the
  silhouette operator `object_like` thresholds |rgb - bg| > 0.06, so an anti-aliased edge pixel flips
  with the backdrop NOISE under it; at x1 truth and candidate sit over the same backdrop and the flip
  cancels, at 13 px apart it does not. The operator's floor grows with displacement and at x2/x4 it is
  of the same order as the FG's measured position error (0.544 / 0.690 px).
- **lesson:** a gate binds to the corpus it ran on (CONDUCT §2: a gate binds to the exact claim it
  tested). Every corpus that carries a row runs its own gate first, and the gate's residual at the
  corpus's displacement IS the floor under that row. The speed law `pos ~ 0.30 * disp^0.32` of
  `B1_SPEED_TEST.md` rests, above x1, on rows whose instrument floor was never measured; it is to be
  re-derived with an operator that passes the gate at those displacements.
- **corrective:** `scene_report.py --silhouette coverage` (the half-coverage contour, the object colour
  of an edge pixel estimated from its interior neighbours; the default `tau` stays byte-identical); the
  gate is pooled (`--jobs`) so it costs ~4 min per corpus; `REGIME_TEST_MATRIX.md` §9 records each
  corpus's gate under both operators and the re-scored speed rows. The frozen identity is not edited:
  its clause (a) already labels the x0.5/x2/x4 points "one run"; this entry adds that their floor was
  unmeasured — a consequence under the identity, recorded, not a rewrite of it.

### P-026 · A measurement record's own method sentence was refuted by the generator it used
- **class:** refuted-premise · **date:** 2026-09-10 · **recurrences:** 0 · **status:** corrected
- **evidence:** `docs/evidence/M1_SRC_RATE.md:21` — "p(t) is parameterised in SECONDS, so the 30 fps
  corpus is the same motion with twice the [per-pair displacement]" — and its verdict "Halving the
  source rate — doubling the per-pair displacement — did not degrade positional accuracy" (line 42-43).
  The generator authors speed in px PER FRAME and converts it: `tools/motion_truth/marker_zoo.py:213`
  `v_px_s = spf * fps` with `spf = 0.5 + 7.5 * frac` (line 210), so displacement per source frame is
  INVARIANT under `--fps`. Found by the measurement-validity verifier of workflow `wf_cce9fbbd-22b`, who
  executed `build_markers` + `eval_traj` at both rates (1.7500 / 4.2500 / 6.7500 px per frame at sizes 6
  / 12 / 24, identical); the supervisor re-read both lines.
- **lesson:** M1_SRC_RATE was already a matched-displacement comparison (30 fps × 8 vs 60 fps × 4 at the
  same px/pair); its "Indistinguishable" is about the multiplier and the phase density, not displacement
  — and it is then the SAME comparison `B1_SPEED_TEST.md` §3 made with the opposite answer (1.10–1.72×).
  A record's method paragraph is a claim about the generator's code and is checked there before the
  record's finding steers a design; the 'source-rate term' is unresolved until `REGIME_TEST_MATRIX.md`
  family 4 runs.
- **corrective:** `docs/planning/REGIME_TEST_MATRIX.md` §1-5 and family 4 (no speed-band halving; the
  multiplier held with `--fg-factor`; `--no-asw`; an absolute bar above the 0.116–0.119 px capture
  floor). The June-era record is a dated document and is not edited; this entry is the correction.

### P-027 · Two help texts and three documents state a default the struct contradicts
- **class:** refuted-premise · **date:** 2026-09-10 · **recurrences:** 0 · **status:** open
- **evidence:** `src/control/cli.hpp:947` `bool mv_candsel=true;` (and both init sites pass
  `cfg.mv_candsel`, `src/flow/flow_init.cpp:40,142`) while `cli.cpp:104` (usage) and `cli.cpp:809` (the
  `--mv-candsel` printf) say "DEFAULT OFF", and `docs/research/FG_VFI_PRIOR_ART.md:639` ("built,
  default-OFF"), `PHYRIADFG_PERFECTION_ROADMAP.md:101` ("built-off parity levers mv_candsel") and
  `catalog/cpp/docs/evidence/baselines/FG_PERF_BASELINE.json:21` ("ALL levers OFF … mv_candsel = false")
  repeat it — they describe the framework's generic `OpticalFlowPipeline` default, not PhyriadFG's.
  Likewise `cli.hpp:222` `bool asw=true;` while the `--asw` printf (`cli.cpp:448`) says "DEFAULT OFF,
  byte-identical off." and `--no-asw` (`cli.cpp:353`) says "DEFAULT es ON". Found by two verifiers of
  `wf_cce9fbbd-22b`; each line re-read by the supervisor.
- **lesson:** the struct initialiser is the only authoritative default; a printf or a doc row is a claim
  about it. An A/B whose arm is recorded from the binary's own output can record the wrong arm — the
  record states the arm by the printf actually emitted (`--no-mv-candsel` at `cli.cpp:344` fires only
  when passed).
- **corrective:** none applied — the source and the doc rows are the operator's to fix (his repo);
  `REGIME_TEST_MATRIX.md` §4 carries the trap and every family states its arm by the emitted printf.
  `status: open` until the texts are fixed.

### P-025 · Two [V1] rows contradicted each other, and a synthesis sentence was refuted by the source it cited
- **class:** refuted-premise · **date:** 2026-09-09 · **recurrences:** 0 · **status:** corrected
- **evidence:** (1) `docs/research/FG_VFI_MEASUREMENT_SOTA.md` (§3 and source row 5) gives FloLPIPS the venue
  "ICIP 2022" tagged [V1]; `FG_VFI_PRIOR_ART.md` (F3, §7) gives "PCS 2022", also [V1]. The KAP sweep of 2026-09-09
  reproduced the wrong half with an invented justification ("per IEEE Xplore listing") and the VISTA T0 gate caught
  it; settled first-hand: PCS 2022, pp. 283–287, best-paper finalist (the authors' repository tagline, the Edinburgh
  Research Explorer). (2) Both June dossiers state "the metric ordering (PSNR/SSIM ≪ LPIPS ≪ bespoke-VFI) is robust;
  absolute SROCC is dataset-fragile". The primary source they cite for it (BVI-VFI, TIP 2023, arXiv 2210.00823) has
  Table II Overall SRCC: FAST 0.70 > PSNR 0.65 > … > FloLPIPS 0.61 > … > LPIPS 0.56, and says PSNR is the second-best
  metric. The ordering holds on the 180-sequence ICIP 2022 study (LPIPS 0.599 vs PSNR 0.520) and inverts on the full
  database. What survives is only "none of the tested metrics exhibit satisfactory overall correlation".
- **lesson:** A [V1] tag certifies that ONE fact was read first-hand, not that the row agrees with its siblings or
  that the synthesis built on it survives the source's own tables. The evidence directory must be read for its
  CONTRADICTIONS, not only for its presence (the container ledger's L-009, second recurrence): grep the same
  entity across every dossier before citing any one of them, and when a sentence says "robust", open the table.
- **corrective:** `docs/research/FG_METRIC_MODEL_PRIOR_ART.md` §3 (C1–C3) carries the corrections with the numbers
  quoted; the June files are dated records and are NOT edited. Any future design argument that leans on LPIPS ≫ PSNR
  for VFI cites Q2-5/Q2-6 of that dossier, not the June sentence.

### P-024 · Twenty-five readers cleared a design; the first live run found two defects in it
- **class:** refuted-premise · **date:** 2026-09-09 · **recurrences:** 0 · **status:** corrected
- **evidence:** `--gdump` (GDUMP_PLAN.md) was refuted by 7 readers, its refutations re-verified by 14 more, its
  bodies reviewed by 3, and every finding re-derived by the supervisor before a line compiled. The first observer
  run then (1) wedged the FG at exit — a recorded tick whose ring was full still signalled the timeline (the signal
  rides every submit so the value stays monotone) but pushed no descriptor, so when the LAST ticks were ring-full
  nobody waited their values and `vkDestroySemaphore` ran with a signal pending; and (2) lost 6 % of the ticks to
  writer stalls of 0.26–0.73 s at 16.5 s and 17.8 s into a 20 s run, while the bytes were 170 MB/s against a disk
  measured at 1.3–1.9 GB/s — the OS write cache flushing a 3 GB stream, not the disk. Neither is in any of the 25
  reports; both were visible in the first log (`observer_k4/gdump_r2.log`, PID 18032 alive with 0 CPU).
- **lesson:** a reading panel finds what the CODE says; it cannot find what the RUN does — the lifetime of a signal
  nobody waits, the cache manager's flush cadence. The panel's value was real (ten corrections, all confirmed)
  and its limit is exact: it verified the design against the source, and the two defects lived in the driver
  and the OS. "Every reviewer confirmed" is a statement about the source.
- **corrective:** `stop()` waits the final timeline value before any destroy (CR3); the frame stream and the pair
  reals bypass the cache when sector-aligned, the default ring is 256 (PR2); the observer runner waits with a
  bound and names a kill. The standing rule this earns: **a crash-class change's first gate is a bounded live
  run whose exit is checked, before any measurement is read from it** — the run that measures is not the run
  that proves the exit.

### P-023 · The round-trip instrument pointed at a directory that no longer existed
- **class:** dormancy · **date:** 2026-09-09 · **recurrences:** 0 · **status:** corrected
- **evidence:** `python tools/check_flag_roundtrip.py --exe build-release/phyriad_fg.exe` →
  `FileNotFoundError: ... 'tools\..\src\cli\cli.cpp'`. R6 (2026-09-06, `records/R6_GATE.md`) renamed `src/cli/`
  to `src/control/`; the tool's `--cli` default (line 105) kept the old path. Nobody ran it between R6 and the
  `--gdump` gate three days later, so the flag-surface oracle that R0 built (`records/r0_roundtrip.txt`, 258
  tokens) was dormant while R6, R7 and the QoL batch changed the parser (274 tokens today).
- **lesson:** a rename gate that checks "the build is green and the tests pass" does not exercise the tools that
  read the SOURCE by path — they fail only when someone next runs them. An instrument's own path defaults are
  touchpoints of the rename (BOOT §3 rule 4), and a rename record that lists the moved files should list the
  tools that name them.
- **corrective:** the default now points at `src/control/cli.cpp`, with the history in the docstring. The run on
  the pre-gdump binary reproduces R0's shape (needs-arg=1 for `--qdump`, other-rc0=6) at 274 tokens, so the
  record is again a usable baseline; `GDUMP_PLAN.md` G0 uses the base-vs-new compare. The stronger step, not
  taken here: `grep -rn "src/cli" tools/` as part of any future directory rename.

### P-022 · Three shipped regressions, and the one that mattered was flagged UNVERIFIED before it shipped
- **class:** shipped defect · **date:** 2026-09-06 · **recurrences:** 1 · **status:** corrected
- **evidence:** the operator downloaded the release and it broke in three ways, all readable in his own
  `observer-live.log`. The worst: `[error] Failed to start '...': The handle is invalid. (os error 6)`,
  seven times in a row. After a single Stop the launcher could never spawn again. Cause: C-7's stop
  called `AttachConsole` to reach a console-less child; `AttachConsole`/`FreeConsole` **replace and then
  close the calling process's own standard handles**, and Rust duplicates the inherited stdin into every
  child, so `CreateProcess` failed forever after.
- **the shape to recognise — a call can succeed and still poison the process.** That code was careful: it
  checked every return value and every failure path fell back to the old behaviour. The reviewer verified
  the axis the author was thinking about. The damage was a PROCESS-WIDE SIDE EFFECT on state neither of
  them was looking at. "Every error path is handled" is not the same claim as "this call changes nothing
  else", and only the second one protects the code that runs afterwards.
- **the part with no excuse:** its own designer wrote **"THE DESIGNER COULD NOT VERIFY the Win32 premise"**
  into the plan, and the reconciler carried that forward, and it shipped anyway. An UNVERIFIED marker
  travelled through a design, a reconciliation, an implementation and a release without ever converting
  into either a test or a removal. That is the failure — not the Win32 subtlety, which is genuinely
  obscure. **A premise marked unverified is a blocking item, not a footnote.**
- **the other two, briefly:** a mid-run resize guard treated a ONE-PIXEL flutter as fatal (`1920x1080 ->
  1920x1079`), killing sessions — its own behaviour note had PREDICTED exactly that ("any app that
  transiently changes its client size by even one pixel now terminates the run") and it shipped
  unchanged; and the launcher re-validated the picked window BY TITLE, discarding the pid at precisely
  the moment a rename made the pid the only thing worth having — see [[P-021]], the same defect one layer
  up, twice in one batch.
- **corrective:** the console path is REMOVED rather than repaired (its benefit was already reachable via
  `--duration`/`--max-frames`, as its own reviewer had noted); the resize guard now fires only on a GROW
  past the built size; identity is re-validated by handle then pid, never by title. The standing rule this
  earns: **an "unverified" or "predicted failure" note in a design is a gate, and the only ways past it
  are a test that executes the path or a decision not to ship it.**
- **method note:** all three were invisible to reading, to the compiler, to 47 ctest cases, to three build
  gates AND to my own bounded runs — because my runs never STOPPED and restarted, never resized a source,
  and never let a window rename itself. The operator found them in about a minute of ordinary use. A
  verification plan that only exercises the happy path start-to-finish is not a verification plan.

### P-021 · A new identity was added to the resolver and five sites still asked the old question
- **class:** regression caught before shipping · **date:** 2026-09-06 · **recurrences:** 1 · **status:** corrected
- **evidence:** the QoL batch gave the capture target two stable identities, `--window-pid` and `--hwnd`,
  and rewrote `find_window_by_substr` to honour them. Running the result:
  `phyriad_fg.exe --window-pid 34500 --duration 2` printed **`[ra] WGC: capturing monitor 0`** and exited 0.
  A pid alone captured the whole screen, silently. Cause: five call sites gate on `cfg.window_substr[0]`
  — *"did the user pass a title?"* — when the question is *"did the user ask for a window?"*
  (`capture_init.cpp` ×3, `cli.cpp` ×2, plus `present.cpp` naming the CSV row "monitor"). The rewrite
  touched the resolver and every site that CALLED it, but not the sites that decide whether to call it.
- **the shape to recognise:** when a feature gains a second way to express the same intent, the risk is
  not in the code that consumes the intent — that code was rewritten, it is where the attention was. It
  is in every predicate that *tests* for the intent, because those read like unrelated boolean checks and
  no compiler links them to the change. `grep` for the OLD expression of the intent, not for the new one:
  the new name has few hits by construction, the old one has all of them.
- **why it matters more than its size:** the failure it produced — a silent fallback to the wrong capture
  source, exit 0, no warning — is the exact defect class this batch was built to remove (L-3 / E-5).
  A change set can reintroduce, in its own new code, the thing it was written to delete.
- **corrective:** one predicate, `wants_window_target(cfg)` in `cli.hpp`, used at every site that gated on
  the title; and inside `init_wgc_backend` the stronger test — `wgc_target_hwnd` is a parameter there, so
  it asks whether a window was RESOLVED, not whether one was requested.
- **method note, and the reason this was caught at all:** it is invisible to reading, to the compiler, to
  47 ctest cases and to three build gates. It surfaced on the first *bounded run of the actual binary*.
  Three of the four defects found that way in this batch were pre-existing; this one was ours. A build
  that compiles is not a change that works, and the gap between them is one 2-second run.

### P-020 · A defensive guard written against an impossible event, catching the real one
- **class:** premise refuted by the code · **date:** 2026-09-06 · **recurrences:** 1 · **status:** open
- **evidence:** the operator reported that changing the frame-gen GPU with auto-restart on did not restart
  and then would not let him stop the FG. Root cause: `ui/src/main.js:1035-1048,1099` — a `restarting` flag
  set before `await invoke("restart")` and cleared by a bare 600 ms timer, with an unconditional
  `if (restarting) return;` on the `fg-exit` listener. The comment beside it states its purpose verbatim:
  *"si hay un reinicio en vuelo, este `fg-exit` es del hijo VIEJO"*. **The backend makes that event
  impossible.** `ui/src-tauri/src/lib.rs:320-346` holds an epoch guard whose own comment says the stale
  reader *"deja el slot + el hijo nuevo intactos y sale en silencio (NO emite `fg-exit`)"*. The old child
  cannot emit. So the frontend window — opened AFTER the spawn, covering the new child's entire early life
  — can only ever swallow the NEW child's death. It then wedges: `running` stays true, Start stays
  disabled, `stop()` takes an empty slot and emits nothing, and `is_running` is called at exactly one site
  (`:1119`, DOMContentLoaded), so nothing resyncs.
- **the shape to recognise:** a belt-and-suspenders guard is written when the layer below is *believed* not
  to handle a case. If the layer below already handles it — and here it says so, in a comment, in the same
  repository — the guard is not redundant, it is a second filter positioned over a different event. The two
  comments contradict each other and both were written in good faith; neither was read against the other.
  A guard's justification is a claim about another component's behaviour and is verifiable like any other.
- **corrective:** stated, not yet applied — the fix is the operator's call. Scope the guard to the child,
  not to a stopwatch: the child's epoch travels in the `fg-exit` payload and the listener ignores only
  events older than the epoch the restart created. The one-line stopgap
  (`setTimeout(async () => { restarting = false; setRunning(await invoke("is_running")); }, 600)`) is worth
  more than it looks: it converts an unrecoverable state into a self-healing one, because it is the only
  thing in the file that would ever call `is_running` twice.
- **method note:** three of the audit's five dimensions found this independently, and this session then
  verified every line by hand before reporting it. That order is the rule, not a courtesy — a subordinate's
  output is a claim. It also cut the other way: one finding was REFUTED and two downgraded to PLAUSIBLE by
  the verify pass, and a verifier raised one severity after finding a faster fatal exit than its finder had.

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
