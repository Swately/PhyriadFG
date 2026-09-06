# R7_GATE — the instrument retired, the duplications closed (the residuals R5 and R6 named)

**Status: CLOSED for R7(b) — G-R7(b) PASSED (2026-09-06). R7(a) remains open and BLOCKED (§0).**
Operator's word (2026-09-06): *"continua con los residuos si dependen de mi, hazlos segun tu recomendacion,
recuerda que no debemos tomar el camino comodo si no el correcto"*, after R6 closed and the position was
reported to him as *"lo que queda es R7, y es del operador"*.

## 0 · What R7 is, and the half of it this record does NOT close

`CONVERGENCE_MASTER_PLAN.md` §R7 has two halves and they are not the same kind of thing:

| | what it is | state |
|---|---|---|
| **(a)** `--legacy-warp` retired from the default build | a **product default**, gated on the M1 baseline table on both paths (`≤ 0.10 px` mean shift, p95 within spread, `r ≥ 0.5`) | **OPEN, and the blocker is a RUN, not a build** — see the correction below. |

> ### ⚠ Correction (2026-09-06, same day): what (a) needs was stated wrongly here
>
> The row above originally read *"M1 needs MOTION_TRUTH T2–T5, which are `designed` with zero code"*,
> citing `records/BACKLOG_AUDIT.md`. **That was false.** T2, T3, T4 and T5 all closed on 2026-09-04 —
> `S2_T2_GATE.md`, `S2_T3_GATE.md`, `S2_T4_GATE.md`, `S2_T5_GATE.md` sit in this same directory, the
> tools are committed at `tools/motion_truth/`, and **M1 is measured with `r`** for the shipping default
> at `docs/evidence/MOTION_TRUTH_BASELINE.md`. The audit is a dated snapshot that went stale hours after
> it was written; the session read it and believed it over the gate records beside it
> (`docs/LEARNING_LOG.md` P-018 — a recurrence: the foto mental already carries a warning about this
> exact mistake).
>
> **What R7(a) actually lacks:** the same M1 table on the **`--fg-core` path**. The instrument exists and
> runs; this is a measurement, not a build. It is measured in §7 of this record.
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

## 7 · R7(a) MEASURED — M1 on both paths, and the question it turned out to be

The instrument existed all along (§0's correction). This section runs it, and then runs the three things
M1 alone could never answer.

**Method.** The SAME zoo the 2026-09-04 baseline used (`zoo_static`, seed 20260904, noise background, pan 0,
1280×720 @ 60 fps), played by `tools/motion_truth/play_frames.ps1`, captured with `--qdump+`, extracted and
reported by the committed `tools/motion_truth/` chain. **Both sides re-captured the same day, alternated** —
a path-vs-path question wants one set of conditions, not a table from another day. Two sampling levels,
because the first could not resolve the criterion: 2 runs × 16 triples per path, then 2 runs × 48 triples.

**The capture chain's own floor** (`marker_extract.py` checks itself on the REAL plane first), all four
48-triple runs: **0.095 / 0.097 / 0.096 / 0.097 px** mean against its 0.25 px bar — the same on both paths,
which is what says the two sides were photographed the same way. Full tables:
[`../../evidence/M1_R7_DEFAULT.md`](../../evidence/M1_R7_DEFAULT.md),
[`../../evidence/M1_R7_FGCORE.md`](../../evidence/M1_R7_FGCORE.md),
[`../../evidence/M1_R7_COMPARISON.md`](../../evidence/M1_R7_COMPARISON.md).

### 7.1 · Placement: no difference the instrument can resolve

`spread` is the larger of the two within-side ranges — the resolution actually available (P-014).

| class | default | `--fg-core` | within-side spread | shift | vs the 0.10 px criterion |
|---|---|---|---|---|---|
| linear | 0.617 | 0.624 | 0.048 | **+0.007** | inside the bar **and resolved** |
| circular | 0.821 | 0.842 | 0.078 | **+0.021** | inside the bar **and resolved** |
| accel | 0.749 | 0.785 | 0.128 | **+0.036** | inside the bar; spread marginally above it |
| fast (control) | 5.981 | 5.895 | 0.587 | −0.086 | inside the bar, not resolved |
| crossing | 1.289 | 1.160 | 0.182 | **−0.129** | over the bar, under its own spread |

p95: every between-path delta inside the within-side spread. And the one class over the bar **reverses sign
with sampling** — +0.171 at 16 triples, −0.129 at 48. A systematic difference does not change sign when the
sample grows.

### 7.2 · The criterion's `r ≥ 0.5` clause fails — for the SHIPPING DEFAULT

With 48 triples the report has the population to use **exact** `(k_prev, marker)` matching instead of the
`(marker, phase-bin)` MEANS fallback the 2026-09-04 baseline used on most classes:

| class | `r` default | `r` --fg-core |
|---|---|---|
| linear | 0.16 | 0.17 |
| accel | 0.32 | 0.42 |
| circular | 0.58 | −0.17 |
| crossing | 0.75 | −0.07 |
| fast | 0.27 | 0.21 |

The baseline's `r` 0.76–1.00 came from averaging each marker's detections inside a phase bin — which removes
exactly the run-to-run variation `r` exists to detect. Per detection, the error is largely **not** reproducible
run to run, on either path. **A gate the incumbent fails cannot judge the challenger.** The criterion needs
restating, and picking the sampling that flatters it instead is the comfortable route this record declines.

### 7.3 · The sharper instrument — and the 86 % of loaded ticks it cannot see

`--fg-core-ab` runs BOTH kernels every tick from the same inputs and counts differing output pixels. On the
marker zoo, unloaded:

```
[fg-core-ab] TOTAL compared=14382 diff_px=257 max_delta=16 sum_delta=340   light_skips=0
```

14,382 × 921,600 = **13,254,451,200 pixel comparisons, 257 differ = 1.9×10⁻⁸**, mean 1.32 levels of 255.
**`max_delta = 16` is stated, not smoothed:** R3's record says "one level", and here the tail reaches 16 on a
single pixel — the mechanism R3 named (a rounding difference flipping a discrete decision at a gate's
knife-edge, where it measured max 13). R3's decisive run stands: with `PFG_NOCONTRACT=ON` on both modules,
**0 differing pixels over 24,227 ticks**. The residual is FMA contraction, not algebra.

**Then the same instrument under the load governor** (arbiter `--profile chaos`, source 120 fps, `tier:5`
engaged three times):

```
[fg-core-ab] TOTAL compared=967 diff_px=11 max_delta=1 sum_delta=11   light_skips=5993
```

**5,993 ticks skipped against 967 compared — 86 % of the warp ticks are OUTSIDE the comparison envelope.**
The reason is in the source (`present.cpp`): *"the governor shed vblend/band-xfade in the legacy push this
tick; fg_core cannot shed a spec constant -> not compared (declared deviation)"*. Under the exact conditions
the product exists for, the legacy path spends most of its ticks in `warp_light`, and the pure core has no
such mode. **The byte-diff does not find a difference there; it cannot look.** That is the single most
important fact in this section, and no amount of M1 would have surfaced it.

### 7.4 · So what does the missing shed COST? Measured, not argued

If the two cannot be compared byte-wise under load, the question becomes behavioural. Same load, same source
rate, same content, **2 runs per side, alternated**:

| metric | legacy [1, 2] | `--fg-core` [1, 2] | within-side range | Δ |
|---|---|---|---|---|
| presents | 10353, 10144 | 10100, 10371 | 271 | −13 |
| fresh presents | 7361, 7095 | 7137, 7318 | 266 | −0.5 |
| present fps | 230.14, 225.48 | 224.52, 230.54 | 6.02 | −0.28 |
| **fg_multiplier** | 2.009, 1.948 | 1.940, 2.010 | 0.070 | **−0.003** |
| P99 frametime (ms) | 7.444, 7.492 | 7.774, 7.430 | 0.344 | +0.134 |
| FG GPU slice (ms) | 2.603, 2.668 | 2.679, 2.587 | 0.092 | −0.003 |
| freezes | 0, 0 | 0, 0 | 0 | 0 |

**Every delta is far inside the within-side range.** The governor engaged `tier:5` on all four runs. On this
rig, at this load, **not being able to shed costs nothing measurable** — the fresh fraction is 70.6–71.1 % on
both sides. (`1pct_low_fps_integral` returned 118.86 on two runs and 0.998 on two others; it is a broken
column here and is not cited.)

### 7.5 · The panning scene — measured, and the n=2 answer that was wrong

Every M1 number this project has produced, the 2026-09-04 baseline included, is on a STATIC background.
Panning is where a warp path is most likely to differ, so it is measured here rather than named as an open
question. The 2026-09-04 `zoo_noise` could NOT be reused — it predates the `patterns` key the current
extractor reads and dies on it (`LEARNING_LOG` P-019) — so the zoo was regenerated with today's generator at
the same parameters (`--bg noise --bg-pan 120 --markers 18 --sizes 6,12,24 --seed 20260904`, 1280×720, 2 s
@ 60 fps). **Four runs × 48 triples per path**, alternated.

**At two runs per side this section said something else, and it was wrong.** The first pass put three of six
classes OUTSIDE their own within-side spread — `accel` +0.174 (spread 0.113), `circular` −0.218 (0.146),
`fast` −0.559 (0.239) — with mixed signs. Two more runs per side dissolved it: the spreads grew to 0.378,
0.196 and 1.038 and now cover every delta. That is P-014 a third time, and it is left visible here rather
than replaced quietly, because the wrong version is the one a reader would otherwise have believed.

| class | default mean [min..max] | `--fg-core` mean [min..max] | spread | shift | vs the criterion |
|---|---|---|---|---|---|
| hud (static control) | 0.228 [0.224..0.231] | 0.223 [0.213..0.232] | 0.019 | **−0.005** | inside the 0.10 px bar |
| linear | 1.326 [1.218..1.388] | 1.298 [1.169..1.377] | 0.208 | **−0.027** | inside the bar |
| crossing | 1.420 [1.305..1.513] | 1.353 [1.316..1.394] | 0.208 | **−0.067** | inside the bar |
| fast (control) | 3.774 [3.421..3.987] | 3.677 [3.166..4.204] | 1.038 | **−0.097** | inside the bar |
| circular | 0.766 [0.672..0.868] | 0.612 [0.554..0.700] | 0.196 | **−0.154** | over the bar, inside its spread |
| accel | 1.204 [0.971..1.348] | 1.445 [1.212..1.575] | 0.378 | **+0.241** | over the bar, inside its spread |

**Every class is inside its own within-side spread at n = 4**, and two facts say how to read the two that
remain over the 0.10 px bar:

- **The static control is exact.** `hud` markers do not move, and the two paths place them at 0.228 vs 0.223
  px — a shift of −0.005 at a spread of 0.019, with `r = 1.00` over n = 48 in the report. Where there is no
  displacement, the two paths agree.
- **The pixel byte-diff was run on THIS content**, not inferred from the static run:
  `compared=14375 diff_px=3577 max_delta=5 sum_delta=3617` — **3,577 of 13,248,000,000 pixel comparisons
  differ, 2.7×10⁻⁷, essentially all by one level of 255** (mean 1.011, max 5). Panning is ~14× noisier for the
  byte-diff than the static scene (1.9×10⁻⁸) and its capture floor is higher too (0.118–0.124 px vs
  0.095–0.097) — both properties of the content, identical on both paths. One differing pixel in ~3.7 million
  at ±1/255 cannot move an NCC centroid by tenths of a pixel.

The residual M1 shift on a moving class is therefore the sampling, not the kernel. That is an inference, and
the two observations it rests on are above it.

Tables: [`../../evidence/M1_R7_PAN_DEFAULT.md`](../../evidence/M1_R7_PAN_DEFAULT.md) and
[`../../evidence/M1_R7_PAN_FGCORE.md`](../../evidence/M1_R7_PAN_FGCORE.md).

### 7.6 · Recommendation on R7(a)

**What is established.**

| question | instrument | answer |
|---|---|---|
| are the two kernels the same function? | `--fg-core-ab`, static content | 257 of 13,254,451,200 pixels differ = **1.9×10⁻⁸**; **0** over 24,227 ticks when FMA contraction is forbidden (R3) |
| … on panning content? | `--fg-core-ab`, panning | 3,577 of 13,248,000,000 = **2.7×10⁻⁷**, essentially all ±1 level |
| do they place motion differently? | M1, static, 2×48 triples/side | no — shifts +0.007…−0.129 px, every one inside its within-side spread; the one over the bar reverses sign with sampling |
| … on panning content? | M1, panning, 4×48 triples/side | no — every class inside its spread; the static-marker control agrees to −0.005 px |
| does the pure core cost anything under load? | pressured A/B, 2 runs/side | no — presents Δ13, fresh Δ0.5, multiplier Δ0.003, p99 Δ0.134, GPU slice Δ0.003, all far inside range |

**What is NOT established, and travels with any decision.**

1. **The M1 criterion as written is unmeetable by either path** (§7.2): `r ≥ 0.5` fails for the SHIPPING
   DEFAULT at honest per-detection matching. It must be restated before it can gate anything — a plan
   change, and the operator's.
2. **`warp_light` equivalence is unmeasurable by construction** (§7.3): under the load governor the legacy
   path spends 86 % of its warp ticks in a mode the pure core cannot enter, and the byte-diff skips them.
   §7.4 substitutes a behavioural equivalence for a byte one. **That substitution is a judgement, not a
   measurement**, and it should be made explicitly rather than absorbed.
3. **R3's other declared deviations stay outside the envelope**: `extrap`/`cam_lead` have no row, three of
   `single_track`'s four shadows are not reproduced, `mv_edge_snap`'s variant is per-tick dynamic in the
   legacy and static in the port. The envelope is **the shipping default set with `single_track ON`**.

**The recommendation, in order.**

1. **Restate the criterion first, and restate it because the incumbent fails it** — not because a looser one
   is convenient. The honest replacement for *"≤ 0.10 px mean shift, p95 within spread, r ≥ 0.5"* is
   **"the between-path shift is smaller than the within-side spread, the spread is reported, and n ≥ 4 runs
   per side at ≥ 48 triples"**, which §7.1 and §7.5 satisfy on every class of both scenes. Two sections of
   this record exist only because n = 2 gave a different answer than n = 4 on the same data-generating
   process; the run count belongs in the criterion.
2. **Then flip, staged.** `--fg-core` becomes the default for the measured envelope; `--legacy-warp` stays as
   the fallback and the legacy shader stays in the tree, never deleted. The basis is the two byte-diffs and
   the behaviour under load — **not M1**, which is 6–8 orders of magnitude coarser than the question and
   whose job here was only to catch a placement difference the pixel comparison had already excluded.
3. **MR-8 — the operator's eye — is untouched**, and so is the flip: a product default with visible history.

**In one line:** the kernel question is answered and the answer is *they are the same*; what stands between
here and the default moving is a criterion that must be rewritten because the incumbent fails it, and one
judgement about accepting behavioural equivalence in the one mode the pure core cannot enter.

## 8 · Verdict

**G-R7(b) PASSED (2026-09-06).** The code residuals R5 and R6 named are closed, each with its own measurement:

| | closed | the number it rests on |
|---|---|---|
| the two-oracle instrument | §1–§3 | 107,867 decisions / 0 disagreements over two more pressured runs, one of them the `--fwd-pipeline` combination R5 named as untested — **plus** an exhaustive static replacement, because the runs alone could not reach the sites they were certifying |
| the convert duplication | §4 | 71 vs 74 executable lines, differing in exactly two places, both now parameters; `ingest.cpp` 322 → 244; A/B on BOTH paths inside the within-side range |
| the RawRing's rules | §5 | the publish and the drop-to-newest, hand-written at three sites, now one each; `--ingest-async` A/B inside the range |
| `stats_second()` | §6 | **not closed** — 193 lines, 60 genuinely shared references, method specified |

Unchanged by all of it, checked directly: `--help` differs only in `argv[0]`; `--dump-config` and `--layer-dump`
are byte-identical, so the contract hash did not move; **47/47 ctest** (the suite gained `docs_gate_parity`,
the mechanical corrective for P-018 — §0).

**R7(a) is MEASURED, and R7 is still not CLOSED, for a different reason than §0 first gave.** The M1 table on
the `--fg-core` path did not exist; the instrument that produces it did, and had since 2026-09-04. §7 runs it,
on two scenes, alongside the two byte-diffs and a pressured A/B — and the answer to "are these the same" is
yes at every resolution available. What is left is not a measurement: it is **a criterion the incumbent itself
fails and that only the operator can restate**, and **his eye (MR-8)**. §7.6 is the recommendation.

**What this record does not claim.** Nothing here says the generated frames are more correct than they were.
Every measurement in it is a NON-difference — the point of the work was to remove duplicated decision logic
without moving the product, and what is demonstrated is exactly that. The one thing that did improve is
coverage: 8,209 enumerated checks now stand where a sampled runtime counter stood, and the sampled counter's
blind spot was the branch set it was there to certify.

*Made with my soul - Swately <3*
