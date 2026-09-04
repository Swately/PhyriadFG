# R1 — gate record (G-R1, milestone M-R1): the CLOCK extracted · 2026-09-03

> Stage R1 of [`../CONVERGENCE_MASTER_PLAN.md`](../CONVERGENCE_MASTER_PLAN.md) §3 (strategy X14).
> **Why this stage is the operator's objective:** the clock is what makes frame MULTIPLICATION real —
> it decides, for every present tick, which pair the generated frame depicts and at what phase. The
> rest of the FG samples and composites; this is the part that says "this frame belongs at t = 0.37 of
> pair 812". Extracted, it is a testable function instead of a stretch of a 1,500-line loop.
> **Verdict: G-R1 PASSED.** Every number below is quoted from command output. Tree: `c043780` + the E1
> working tree + R0 + R1 (uncommitted; commit not requested).

## 1 · What moved

| | Before | After |
|---|---|---|
| `src/present/present.cpp` | 3,054 lines, the clock interleaved through the OUTPUT-CLOCK loop | 2,818 lines; the loop CALLS the clock |
| `src/clock/phase_clock.hpp` | — | the stage-4 contract: `Cfg`, `PairRing`, `AdvanceIn/Out`, `SelectIn`, `Selection`, `Order`, `PhaseClock` |
| `src/clock/phase_clock.cpp` | — | 621 lines: `advance()` (PLL frequency loop + NCO advance + D calibration + the per-arrival phase lock), `select()` (t_display + the published-set selection + the phase + the sync-clock override + the ASW overshoot), `order()` (the content-order key + the backwards guard + the base `t_use`), `commit()` |
| `tests/clock/test_phase_clock.cpp` | — | the CPU test (`pfg_clock_test`), GPU-free and Vulkan-free |
| instrument | — | `--arrival-log FILE`: one line per WAP tick, every clock input and output in exact hex-float |

**What deliberately did NOT move** (they are phase LAYERS, not the clock; S5): `--phase-norm`'s even
grid, the s2 realized-mult governor, `--cphase`'s opening ease, `--fdrop` / the over-production drop,
the laser mass feedback. They still reshape `t_use` after the clock, behind their own flags. Nor did
the arrival-delta extraction (stage 1/2 data: the caller reads the capture backend's atomics).

## 2 · Verbatim + the documented transforms

`scratchpad/verbatim_r1.py` over `phase_clock.cpp` vs the pre-extraction `present.cpp` snapshot:

```
phase_clock.cpp: 621 lines total | 371 scaffolding (header/signature/alias/return) | 250 moved-body lines
VERBATIM: 247 / 250 = 98.80 % of the moved body appears byte-identical in the pre-extraction present.cpp
non-matching moved-body lines (3):
   258 | const int    cap_slots= k_.cap_slots;          <- an alias line the scaffold filter missed
   433 | tcap_r = in.real_tcap(in.real_ctx, rs);        <- T2
   605 | if(async_front_ready) backstep_freeze=true;    <- T4
```

The five transforms, complete (each is in `phase_clock.cpp`'s own header too):

| # | Transform | Why it is safe |
|---|---|---|
| T1 | `const double now_d=now_ms();` dropped from `select()` — the read is an input | Same single read, same place; the call site reads the clock and hands it in (needed for a deterministic replay). |
| T2 | `c_slots[rs].t_cap_ms` → `in.real_tcap(in.real_ctx, rs)` | Same value through a function pointer; keeps `FgContext`/Vulkan out of a CPU-testable unit. Read ONLY on the startup no-interp path. |
| T3 | `p_presenting.store(fs-gen_back)` moved to the call site | A ring-guard publish, not a clock computation. It now runs BEFORE the gme/bwd/mass reads instead of after — strictly earlier (more conservative for F's guard), and those reads do not touch it. |
| T4 | `cfg.async_present && async_front>=0` → the `async_front_ready` parameter | A present-stage fact the clock reads; now declared instead of reached for. |
| T5 | the F→P publish ring is read ONCE per tick into a snapshot, and the clock reads the snapshot | F writes a generation's fields BEFORE the seq_cst `f_seq` bump, so a snapshot taken right after reading `fs` is at least as consistent as the per-access reads it replaces. It also makes the tick replayable. |

Plus one hoist, made BEFORE the extraction and carried by both sides of the A/B: the set-detect clock
read is now one read per tick (`now_b`) instead of one inside the set-detect branch.

## 3 · The gate

| Check | Result (quoted) |
|---|---|
| build ×2 | `build.bat` exit 0, `build-release.bat` exit 0 (both after the extraction) |
| **replay bit-parity (XR14's oracle)** | `pfg_clock_test` on a live 60 s `ball_zoo` recording: **`replay: 14390 ticks \| mismatches D=0 t_display=0 content_clock=0 T_robust=0 selection=0 phase=0 t_use=0 order=0`**. Bit-identical (memcmp of the doubles) on every field of every tick — the unit is pure and its declared input set is complete. Same result from the debug and release builds. |
| **synthetic arrivals** | `synthetic: lock_tick=6 pairs=246 sweeps=156 max_distinct_phases_per_pair=4 backsteps=0 T_robust=35.659ms` — 60 fps ± 2 ms jitter, a 60→30 fps step at 3 s, one dropped pair, 240 Hz tick, 6 s. Asserted: locks within 1 s (6 ticks), content never steps backwards (0), every pair gets a multi-phase sweep (156 of 246 pair changes — the rest are the pre-lock and post-step transients), ≥ 3 distinct phases per pair (4), the PLL tracked the rate step (T_robust 35.7 ms ≈ the 30 fps period). |
| **live A/B, 2 runs/side, interleaved, 60 s** | pre-extraction binary vs R1 binary on `ball_zoo` 60 fps 1920×1080. presents: A 14,384 / 14,383 · B 14,384 / 14,384. The clock's own signature, the presented-phase distribution: |
| | | metric | A (pre) | B (R1) | Δ vs A's spread |
| | |---|---|---|---|
| | | phase mean | 0.5020 ± 0.0006 | 0.5015 ± 0.0001 | −0.0005, inside |
| | | phase sd | 0.2829 ± 0.0007 | 0.2823 ± 0.0002 | −0.0006, inside |
| | | fraction pinned at 1.0 | 0.0181 ± 0.0035 | 0.0159 ± 0.0013 | −0.0022, inside |
| | | **content step per present** | **0.2502 ± 0.0000** | **0.2501 ± 0.0000** | −0.0001 |
| | | uniq/s | 239.02 ± 0.02 | 239.03 ± 0.09 | inside |
| | | lat (ms) | 16.57 ± 0.36 | 16.54 ± 0.25 | inside |
| 120 s smoke | `--monitor 0 --exit-after 120` exit 0, `bounded-run clean exit: total_presents=28797` (the 240 Hz panel rate) |

**The multiplication, measured.** `phyriadfg_disp_src_frames` is the content-source time each present
depicts. Its mean step per present is **0.25 source frames** on both sides: a 60 fps source on a
240 Hz panel yields four distinct content moments per real pair. That is the frame multiplication, and
it is now produced by a unit with a test.

## 4 · What this proves, and what it does not

- **Proves:** the clock is a pure function of a declared input set (the replay reproduces a live 60 s
  run bit-for-bit); the extraction did not move the product (verbatim 98.80 %, five argued transforms,
  the phase distribution and every rate metric inside the run-to-run spread); the clock behaves
  correctly under jitter, a rate step and a dropped pair (the synthetic test).
- **Does NOT prove:** that the clock is RIGHT — that the phase it picks is where the content actually
  was. That is metric **M1**, and it stays blocked on MOTION_TRUTH T4–T5 (risk XR9). The replay is an
  extraction oracle, not a motion oracle.
- The replay oracle was recorded with the POST-extraction binary. A pre-extraction recording could not
  drive the replay because the pre-extraction code lacked the per-tick ring snapshot the replay needs
  (T5). The pre-vs-post evidence is therefore the verbatim diff + the transform arguments + the live
  A/B, stated as such rather than dressed as a bit-parity result.

## 5 · Defects found while doing R1 (kept for the record)

- The `--arrival-log` v1 could not drive a replay: it recorded the selected generation but not the
  whole ring, and not the commit outcome. The clock's monotonicity state advances only on a COMMITTED
  tick and with the LAYER-reshaped `cand_k`, so the log had to record both. Two instrument iterations.
- My synthetic simulator was wrong twice, and each error was a real lesson about the clock: (a) it
  wrote the pair's capture time in the FUTURE, so `det` was negative, D stayed 0 and the clock never
  locked; (b) it reported the INTENDED inter-arrival instead of the REALIZED one, so the PLL learned a
  period the source never had and the clock ran ahead of every published pair, pinning the phase at 1.
  Both now carry a comment in the test.
- A heredoc ate a `\n` escape twice (a broken string literal, then a silently-skipped patch that made
  a stale binary look like a passing build). Patches are written to files, never to heredocs.

## 6 · Honesty ledger

- The `--arrival-log` write is one `fprintf` per tick on the present thread. It is default-off (no file
  opened, no write) and the runs that use it are measurement runs; the A/B above ran WITHOUT it.
- The synthetic test's numeric bounds are baselines from the first passing run, recorded in the test,
  not targets derived from a specification.
- `sweeps=156 of 246` pair changes: the shortfall is the pre-lock transient and the 60→30 step, where
  a pair legitimately gets one tick. The assertion is `>= 100`, not "every pair", and the printed
  counters make the shortfall visible rather than hidden.
- Not measured: M1 (blocked), M3 GPU-time per tick (unchanged code path, not re-profiled), the
  `--arrival-log` overhead.

*Made with my soul - Swately <3*
