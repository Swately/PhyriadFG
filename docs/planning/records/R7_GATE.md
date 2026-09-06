# R7_GATE — the instrument retired, the duplications closed (the residuals R5 and R6 named)

**Status: CLOSED for R7(b) — G-R7(b) PASSED (2026-09-06). R7(a) remains open and BLOCKED (§0).**
Operator's word (2026-09-06): *"continua con los residuos si dependen de mi, hazlos segun tu recomendacion,
recuerda que no debemos tomar el camino comodo si no el correcto"*, after R6 closed and the position was
reported to him as *"lo que queda es R7, y es del operador"*.

## 0 · What R7 is, and the half of it this record does NOT close

`CONVERGENCE_MASTER_PLAN.md` §R7 has two halves and they are not the same kind of thing:

| | what it is | state |
|---|---|---|
| **(a)** `--legacy-warp` retired from the default build | a **product default**, gated on the M1 baseline table on both paths (`≤ 0.10 px` mean shift, p95 within spread, `r ≥ 0.5`) | **BLOCKED, not deferred.** M1 needs MOTION_TRUTH T2–T5, which are `designed` with zero code (`records/BACKLOG_AUDIT.md` rows A6, S2.T2–T5). No proxy closes an M1 gate — the plan says so and this record does not reinterpret it. |
| **(b)** the two duplicated oracles retired | a **code residual** R5 left deliberately, gated on *"a second pressured run has counted 0 again"* (`R5_GATE.md:307-309`) | closed here, §1–§3. |

Two more residuals were named and not done by their own gates; they are closed here as §4 (`R6_GATE.md:174`,
the convert duplication) and §5 (`R6_GATE.md:175-177`, the RawRing owning only its counter).

**What this record must not be read as:** it does not close R7, it does not touch `--legacy-warp`, and it
makes no claim about motion accuracy. It closes the code residuals of R5 and R6.

## 1 · The measurement R5 demanded: the second pressured run

`tools/r5_pressure.ps1`, load = `projects/gpu_oc/escalera_arbiter.exe --profile chaos` (96–100 % of the 4090),
source 120 fps so the per-pair budget halves and the CPU ladder actually climbs — the lever P-008 identified,
not GPU saturation. Two runs, 40 s each:

| run | flags | governor | flow decisions | transport decisions | mismatches | exit | arbiter |
|---|---|---|---|---|---|---|---|
| 1 | default | `tier:4 ×1`, `tier:5 ×2` | 31,115 | 25,380 | **0 / 0** | clean, 8,974 presents | `STABLE` |
| 2 | `--fwd-pipeline` | `tier:5 ×3`, `tier:4 ×1` | 27,738 | 23,634 | **0 / 0** | clean, 9,281 presents | `STABLE` |

Quoted, run 2: `[layertab] flow rows vs the hand conditions: 27738 site decisions, 0 mismatches` ·
`[layertab] 3->5 transport rows vs the hand flags: 23634 site decisions, 0 mismatches` ·
`[ra] bounded-run clean exit: total_presents=9281`.

**Run 2 is not a repeat of run 1.** `R5_GATE.md:329` named the gap in its own coverage: *"`--fwd-pipeline`
combined with pressure were not run together"*. `--fwd-pipeline` is the ONLY producer of
`ArmInputs.pipelined` (`fin.pipelined = !allow_bwd`, `flow_consume.cpp`) — an arm input every FLOW row's mask
reads — so a second run at the same settings would have re-certified the same states. The script grew a
`-FgArgs` parameter for it.

**Totals across the two runs: 107,867 row/transport decisions, 0 disagreements.** The condition R5 wrote
down is met.

## 2 · What the instrument could never prove, and what replaces it

A runtime oracle only visits the states the run reaches. Both pressured runs report `bwd-skip:100%` — which is
exactly the condition under which `do_bwd` is false, so the four sites nested inside `if(do_bwd)` were barely
exercised **by the very runs that certify the shedding branches**. Deleting the instrument on the strength of
those runs alone would trade a sampled proof for nothing.

So the knowledge moved instead of being dropped: **`tests/layers/test_arm_parity.cpp`** (`pfg_arm_test`,
ctest case `layer_arm_parity`) carries the 11 flow sites as a table of *row decision vs the hand condition the
site used before R5*, enumerated over every combination of the arm inputs — `eff` bit × `has_prev` × `tier`
0..6 × `holon_skip` × `pipelined` × `bwd_skipping` × `gme_did_fit`, inside each site's own nesting guard.

```
  bidir           2112 states compared      gme_bwd          144 states compared
  gme_ran         2112                      gme_gpu/bwd       72
  gme_gpu/fwd      704                      mem_bwd           72
  mem_fwd          704                      objects_bwd       72
  objects/fwd      704                      mem_refresh      704
                                            objects/tail     704
31320 states outside a site's guard or unreachable (not compared)
OK: all 8209 checks passed
```

The first version of this table read half those numbers, because the eff bit of the row a site is NESTED INSIDE
was the same variable as the site's own. `--no-memory` with gme and bidir on is a real configuration, and
folding the two bits together silently dropped every state where the guard row is ON and the guarded row is
OFF — the only states that can catch a row that stopped reading its own eff bit. The guard bit is now its own
enumerated variable (`World::eff_guard`), and the three backward sites went 36 → 72 states each.

It is not a tautology: it calls the real `layer_arm_mask` over the real `layer_table.def`, so a changed ArmId
or a changed arm rule fails on the site whose behaviour it changes. **Seen red three ways** (EMPIRICAL_TEST
§3.3), each reverted:

| perturbation | result | first failing line |
|---|---|---|
| `mem_fwd`'s row: `HOLON` → `ALWAYS` | RED, 224/8209 | `FAIL mem_fwd  eff=1 … hskip=1 … -> row=1 hand=0` |
| `holon_skip_for`: `tier >= 4` → `tier >= 5` | RED, 15/8209 | `FAIL holon>=4  tier=4 tiers=0 ctr=0: shed must be unconditional` |
| `ArmId::BIDIR_OK`: drop the `!pipelined` term | RED, 144/8569 | `FAIL bidir  … pipe=1 … -> row=1 hand=0` |

The three were re-run after the widening: a gate binds to the exact claim it tested, and the test is not the
one the first run of them exercised (CONDUCT §2).

**One code change was needed to make a property testable.** The HOLON arm carries a `tier < 4` term the hand
conditions never had; the two agree only because `tier >= 4` forces `holon_skip` — an implication that lived
as an expression in the middle of `flow_consume`'s pair body, where no test can reach it. It moved verbatim to
`control/layer_abi.hpp` as `holon_skip_for()` / `holon_period_for()`, beside the arm that reads it. The
decimation periods (tier 2 keeps 6 of 12 pairs, tier 3 keeps 3 of 12) are pinned there too.

**What is NOT covered here, named.** Each site's decision is `eff_bit && arm_bit`; this test pins the ARM half
and enumerates the eff bit. The EFF half — that `cfg.layers.eff` equals the init cascade — is
`layer_flow_resolve`'s loud exit 3 at every startup plus the 34 `layer_parity_*` ctest cases through the real
binary. The six transport sites in `present.cpp` read an eff bit and nothing else, so their retirement rests
on that same eff oracle; the one composite among them, `eff(GME) || eff(GME_GPU)`, is check 3 of the new test.

## 3 · The retirement, and the A/B it had to survive

Removed: 11 `row_check` + 6 `up_check` call sites, 2 lambdas, 2 counters, 2 report lines, and the two
`ConsumeState` references that carried the counters into `flow_consume`. The binary shrank 1,323,008 →
1,321,472 bytes; 46/46 ctest green.

**On warnings — a claim withdrawn and replaced by what the compiler said.** An earlier draft of this record
said "zero compiler warnings". That came from a TRUNCATED build log (`Select-Object -Last 40`) and it is false:
the tree builds at `/W4` and carries pre-existing `C4996` (fopen), `C4456` (shadowing) and `C4189` (unused
locals) lines in `present.cpp` and `main.cpp`.

Worse, my own dead-alias check reported **0 orphans** for the moves in §4 — and it was wrong. It counted name
matches on raw lines, so `\bA\b` matched the "A" in a comment like *"A-path"* and an alias with no reader left
looked used. The compiler's C4189 list was right and mine was not; the locals this session's moves
orphaned were found and removed against that list, not against my reading. **Eight** locals in all lost their
last reader: two I caught by reading (`shed_holon`, `holon_period`), and six only the compiler saw.

| file | orphaned by | names |
|---|---|---|
| `flow_consume.cpp` | the oracle retirement (§2–§3) | `shed_holon`, `holon_period`, `use_objects` |
| `ingest.cpp` | the convert unification (§4) | `c_cv`, `A`, `G`, `d` — plus the 22 counted there |
| `capture.cpp` | the RawRing taking the publish (§5) | `raw_seq` |

The claim this record makes, and it is now checked by name: **this work added no warning that survives it.**
Every C4189 in the final build log belongs to `main.cpp` or `present.cpp` and names a local none of these
changes touched (`bframe_use`, `gsrc_use`, `have_igpu`, `hFIELD_g`, `xfer_fams`, `B`, `cmdG`, `flow_div`,
`WW_flow`, `fG`, `d`, `s2_last_opdrops`).

**A/B: four runs per side, alternated, the second block running post-first so a warm-up drift lands on the
other side than in the first.** `ball_zoo` 60 fps, 1920×1080, 45 s, `--csv`.

| metric | pre mean [min..max] | post mean [min..max] | within-side range | Δ |
|---|---|---|---|---|
| frame_count | 10788.0 [10787..10789] | 10789.0 [10789..10789] | 2.0 | 1.0 |
| fresh_count | 10771.0 [10770..10772] | 10770.5 [10768..10772] | 4.0 | 0.5 |
| present_fps_avg | 239.7880 [239.7633..239.8091] | 239.8092 [239.8078..239.8102] | 0.0458 | 0.0212 |
| real_fps_avg | 59.9204 | 59.9244 | 0.0178 | 0.0040 |
| **fg_multiplier** | **4.0022** [4.0017..4.0025] | **4.0023** [4.0018..4.0025] | 0.0008 | 0.0001 |
| P99_frametime_ms | 4.2862 | 4.2885 | 0.0250 | 0.0024 |
| fg_slice_ms_4090_avg | 3.7793 [3.7451..3.7915] | 3.7582 [3.7364..3.7855] | 0.0491 | 0.0211 |
| freeze_count / stall_count / dropped_rows | 0 | 0 | 0 | 0 |

**Every metric is inside the within-side range.** This is reported with the reason the FIRST attempt was not:
at n=2 per side the range of `frame_count` came out exactly **0** (both pre runs 10788, both post 10789), so a
one-present difference read as "outside the spread" and four metrics flagged. A range estimated from two
samples can be degenerate, and a degenerate range makes every difference look significant. n=4 is what the
table above rests on; the n=2 table is not quoted anywhere as a result.

**The log's shape.** Numbers, signs and paths blanked, the multiset of line shapes compared, with a
**same-binary control** (pre1 vs pre2) as the calibration:

- control, pre vs pre: 64 shapes each, **1 shape differs** — the stats line carries an `rdrop:N/s` field only
  in runs that dropped a real frame. That is run noise, and it is the reason the control exists.
- treatment, pre vs post: 64 → 62 shapes. The two that disappear are
  `[layertab] flow rows vs the hand conditions: …` and `[layertab] N->N transport rows vs the hand flags: …`
  — the retired instrument's own reports. The only other difference is the same `rdrop` shape the control
  already produced.

## 4 · R6 §1's residual: one convert, two callers

`R6_GATE.md:174` named it: *"the convert logic is still duplicated between the serial tail and the worker"*.
It was found in R6 because an anchor matched twice; it is closed here because a duplication is a correctness
hazard (every future edit must remember both), not a tidiness one.

**Measured before touched** (`r7_convert_diff.py`, comments and indentation stripped): the serial copy is 71
executable lines, the worker's 74, and the diff is **exactly two things** — the A-path copy SOURCE
(`Astage.buf` vs `raw_astage_a[rk].buf`) and a three-line `vkUpdateDescriptorSets` the worker needs because
its iGPU source rotates through the raw ring. Both became parameters of
`pfg::ingest::convert_record_submit(ctx, cap_rot180, s, a_src, g_src, g_range)`; the body is the serial copy
**verbatim**, so the serial path is byte-identical by construction and the worker differs from its old self
only where the measured diff said it did. `g_src == VK_NULL_HANDLE` means "leave the init-time binding alone",
which is precisely the serial path.

`ingest.cpp` 322 → 244 lines. **22 of `run_convert_worker`'s aliases lost their last reader** and were
removed — that count is the size of the duplication, stated as a number rather than as an adjective.

**Gate.** Both paths, against the binary from §3 (the retirement, before this change) — the serial path because
it must be byte-identical by construction, and `--ingest-async` because it is the path that actually changed.

| path | n per side | verdict |
|---|---|---|
| serial (default) | 4 | every metric inside the within-side range: `frame_count` 9590.0 → 9589.25 (range 2.0), `fg_multiplier` 4.0038 → 4.0031 (range 0.0015), `fresh_count` 9572.0 → 9573.5 (range 3.0), 0 freezes / 0 stalls / 0 dropped rows on both |
| `--ingest-async` | 2 | every metric inside the range: `frame_count` 9589.5 both sides (Δ 0.0), `fg_multiplier` 4.0034 → 4.0036 (range 0.0007), `fresh_count` 9573.5 → 9571.5 (range 3.0) |

One serial run was **discarded and re-run**: `c2pre_ser1` came back at 10.8 fps with `warp 833.07ms` — a 200×
outlier, not a deviation. No stray process was found (`Get-Process` showed one `RA Ball Zoo` window and no
arbiter), the next run on the same machine with the other binary was nominal, and that same binary ran nominally
five other times in this session. It is recorded as an unexplained transient rather than dropped silently; the
discard rule was its own internal evidence, not which side it favoured.


## 5 · R6 §2's residual: the RawRing owns its rules

`R6_GATE.md:175-177`: *"the RawRing is declared and its fields are still addressed through the old aliases
everywhere — only its counter is owned, so it is one step less real than the FrameRing, which also owns
publish()"*.

What makes a ring real is not owning storage; it is owning the RULE. The RawRing's rules were two, both
hand-repeated:

- **publish** — write the slot, store the counter **under `raw_mtx`** (so it cannot land between the worker's
  predicate check and its `wait()` — a lost wakeup stalls ingest until the next frame), notify **outside** the
  lock. Written out twice: the DDA acquire and the WGC pickup.
- **drop-to-newest** — read the counter, ignore anything not newer than the last converted, address
  `(newest-1) % N`; the backlog is discarded, never queued.

Both are now `RawRing::publish()` and `RawRing::take_newest()`; three call sites go through them.

`capture.cpp` lost its `raw_seq` alias with them: the acquirer no longer touches the counter at all, which is
the point — it hands the ring an index and the ring decides how that index becomes visible.

**Gate.** `--ingest-async`, **three runs per side**, alternated, against the binary from §4. The serial path is
not measured here and does not need to be: it never publishes a raw frame (the acquirer converts in place), so
neither rule is on its path — stated rather than left implied.

| metric | pre mean [min..max] | post mean [min..max] | range | Δ |
|---|---|---|---|---|
| frame_count | 9590.0 [9590..9590] | 9589.67 [9589..9590] | 1.0 | 0.33 |
| fresh_count | 9573.67 [9573..9574] | 9573.0 [9571..9574] | 3.0 | 0.67 |
| **fg_multiplier** | **4.0034** [4.0032..4.0036] | **4.0036** [4.0032..4.0041] | 0.0009 | 0.0002 |
| present_fps_avg | 239.8109 | 239.8038 | 0.0370 | 0.0071 |
| P99_frametime_ms | 4.2858 | 4.2947 | 0.0319 | 0.0088 |
| freeze / stall / dropped_rows | 0 | 0 | 0 | 0 |

Every metric inside the within-side range. A lost wakeup would show as a drop in `real_fps_avg` (the worker
would sleep through a published frame); it reads 59.9073 → 59.9033, Δ 0.0040 against a range of 0.0118.


## 6 · R4's residual, MEASURED and left open: `stats_second()`

`R4_GATE.md:58` declared it: *"Stayed in the loop (declared residual): the per-second stats blocks
(`stats_second()` in X12) — ~190 lines"*. It is still there, and this record says why with numbers rather than
leaving the reader to assume it was forgotten.

**What it is:** `present.cpp:2644–2836`, **193 lines**, firing once per ~90 presents (~2.7 times a second). It
is reporting, not present machinery, so `STAGE_CONTRACT` puts its home in the INSTRUMENT plane.

**What moving it by the R5/R6 method would cost** (measured, `r7_stats_scan.py`): the block uses **208 distinct
identifiers**; 71 are declared inside it, leaving **131 from outside** after library names are dropped. Of those:

| | count | what it means for the extraction |
|---|---|---|
| reachable through `FgContext` | 32 | free — the function takes `ctx` |
| block-PRIVATE state, declared outside only because it must survive the loop iteration | 21 | free — they move WITH the block into the struct that owns them |
| genuinely SHARED with the present loop | **60** | each one a reference in the binding struct |

Sixty references is **twice** what `consume_wap` needed (R5 s3c: 30 refs + 5 callables), and unlike that case the
struct would have to grow every time a statistic is added to the line — which is the one thing this block does
often. The extraction would move 193 lines and create a binding surface of comparable size.

**The comfortable version of this task, named** (the operator's standing test): wrap the 193 lines in a lambda
declared just above the loop and call the residual closed. `present.cpp` would lose 190 lines from the loop
body, the count would look right, and nothing whatever would have changed — same translation unit, same
function, same 60 captured locals, now captured implicitly instead of listed. This record declines that.

**What would actually close it**, and it is a design step, not a move: the 60 shared names are mostly per-window
ACCUMULATORS the present loop feeds (`slip_sum` / `slip_n` / `slip_max`, `sum_iter` / `worst`, `stat_ticks`,
`uniq_ticks`, and ~25 `last_*` marks the window differences against). Give the instrument plane a type that
OWNS them — the loop calls `w.slip(x)`, `w.iter(x)`; the reporter reads `w` — and the binding count collapses,
because most of those names stop being shared at all. That is a stage-6 instrument step with its own gate (the
stats line is a product surface: its shape is compared in every gate in this directory), and it is not R7.

**Status:** open, measured, method specified. Not closed here, and not closed quietly.

## 7 · Verdict

**G-R7(b) PASSED (2026-09-06).** The code residuals R5 and R6 named are closed, each with its own measurement:

| | closed | the number it rests on |
|---|---|---|
| the two-oracle instrument | §1–§3 | 107,867 decisions / 0 disagreements over two more pressured runs, one of them the `--fwd-pipeline` combination R5 named as untested — **plus** an exhaustive static replacement, because the runs alone could not reach the sites they were certifying |
| the convert duplication | §4 | 71 vs 74 executable lines, differing in exactly two places, both now parameters; `ingest.cpp` 322 → 244; A/B on BOTH paths inside the within-side range |
| the RawRing's rules | §5 | the publish and the drop-to-newest, hand-written at three sites, now one each; `--ingest-async` A/B inside the range |
| `stats_second()` | §6 | **not closed** — 193 lines, 60 genuinely shared references, method specified |

Unchanged by all of it, checked directly: `--help` differs only in `argv[0]`; `--dump-config` and `--layer-dump`
are byte-identical, so the contract hash did not move; 46/46 ctest.

**R7 IS NOT CLOSED.** Its other half, `--legacy-warp` out of the default, is blocked before it is even a
decision: no M1 table exists for the `--fg-core` path, MOTION_TRUTH T2–T5 have no code, and the plan's own rule
is that no proxy closes an M1 gate. Building T2–T5 is the only route, and it is a large arc of its own
(`records/BACKLOG_AUDIT.md` A6). When it exists, the flip is still the operator's: a product default with
visible history.

**What this record does not claim.** Nothing here says the generated frames are more correct than they were.
Every measurement in it is a NON-difference — the point of the work was to remove duplicated decision logic
without moving the product, and what is demonstrated is exactly that. The one thing that did improve is
coverage: 8,209 enumerated checks now stand where a sampled runtime counter stood, and the sampled counter's
blind spot was the branch set it was there to certify.

*Made with my soul - Swately <3*
