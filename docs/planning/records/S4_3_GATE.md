# S4_3_GATE — the tests are wired, and every one of them was seen RED (2026-09-06)

**Status: CLOSED.** Operator directive the same day: *"buscamos que el trabajo este bien hecho, no rutas comodas"* —
which is the reason this record exists at all. The comfortable version of 4.3 is three `add_test` lines and a green
run. What follows is the other version: the wiring, the two false greens it exposed, the defect it found in the
binary's exit code, and a deliberate perturbation per gate to prove each one can fail.

## 0 · What was true before

`enable_testing()` / `add_test()` appeared **zero** times in `CMakeLists.txt`. Two test binaries were built by every
build and run by nothing:

- `pfg_seam_test` — R2's seam engine, **148 checks**, self-contained (mock handles, no device).
- `pfg_clock_test` — R1's `PhaseClock`: a **bit-parity replay oracle** plus synthetic-arrival checks.

The oracle had never run against a real log in its life. Its `replay()` takes a path; nothing ever gave it one.

## 1 · The two false greens (found by wiring, not by reading)

1. **`pfg_clock_test` with no argument printed `RESULT: all checks passed (0)` and exited 0** — having replayed
   zero ticks. Worse, given a path it could not open or parse it printed `SKIP` and *still* exited 0: a test told to
   check something, unable to, reporting success. A renamed fixture would have reported green forever.
   **Fixed:** `replay()` returns −1 (a failure) when the log cannot be opened or has no cfg header / no ticks; `main`
   now distinguishes three states in both wording and exit code — replayed (`PASS — replay oracle over N ticks`),
   not asked (`PASS (synthetic checks only) — THE REPLAY ORACLE DID NOT RUN`), asked and unable (exit 1).

2. **A parse error exited 0.** `main.cpp:206` was `if (!parse_args(...)) return 0;` and `parse_args` returns false
   for `--help` *and* for a user error, so `phyriad_fg --no-such-flag` and `phyriad_fg --mv-smooth` (a flag whose
   required value is missing) both printed a message and exited **0**. Any script, harness or CI checking the exit
   code could not tell a typo from a good run — and this arc's own harnesses check exit codes.
   **Fixed:** `Config::parse_failed` is set at the two error sites — the `next()` helper (which covers the whole
   missing-value class in one place) and the unknown-option branch — and `main` exits **2** when it is set. `--help`
   and every informational stop keep 0. This is a behaviour change to a shipped binary, deliberate and recorded.

## 2 · The suite (43 tests, ~1.5 s, CPU-only, no GPU or window needed)

| # | test | what it guards |
|---|---|---|
| 1 | `seam_graph` | R2's 148 derived-barrier checks |
| 2 | **`clock_replay_oracle`** | R1's bit-parity oracle over `tests/clock/fixtures/ball_zoo_60fps_240hz.alog` (2,877 ticks) — **the check that had never run** |
| 3 | `clock_synthetic` | the synthetic half, and that it SAYS the oracle did not run |
| 4 | `clock_replay_missing_log_fails` | negative: told to replay an unopenable log, it must FAIL |
| 5–38 | `layer_parity_1..34` | the registry's shadow parser vs the hand parser over 34 token combinations (`--dump-config` exits 3 on a mismatch), including the `--gme-gpu-verify` / `--no-gme-gpu` ordering that broke it during R5 step 1 |
| 39 | `layer_contract_hash` | the pinned contract hash of the default resolved chain (`0x9517AE73A530EAFE`) |
| 40–42 | `cli_unknown_option_exits_2`, `cli_missing_value_exits_2`, `cli_help_exits_0` | §1.2's fix, both directions |
| 43 | `layer_gen_rejects_bad_body` | negative: the generator must refuse a row body with a direct `lp.` UBO read (exit 3) — and the script first proves the UNMODIFIED tree generates cleanly, so a failure is attributable |

The fixture is a real recording (`phyriad_fg --arrival-log`, ball zoo 60 fps → 240 Hz, 12 s, 2,880 lines, 1.1 MB).
Committed deliberately: without it the oracle cannot run anywhere but on a machine with a GPU and a window.

**Where it runs:** `build-release.bat` now runs `ctest` after a successful build and exits non-zero if the suite is
red — the binary is already written at that point, so nothing is blocked; what fails is the claim that the build is
good. `tools/run_tests.bat` is the standalone entry point.

## 3 · Every gate seen RED (EMPIRICAL_TEST §3.3)

A test whose failure has never been observed is a claim, not a gate. Four perturbations, each built, run, and
reverted; after every revert the suite returned to 43/43.

| # | perturbation | expected red | result |
|---|---|---|---|
| A₀ | `phase_clock.cpp`: `+0.5` → `+0.5000001` in the phase-quantisation key | `clock_replay_oracle` | **STAYED GREEN — 43/43** |
| A | `phase_clock.cpp`: `t_use = phase_global` → `× (1 + 1e-15)` (one ulp) | `clock_replay_oracle` | **RED**: `mismatches … t_use=2877` of 2,877 ticks; `clock_synthetic` red too |
| B | `layer_table.def`: `bg_reclaim` rank 30 → 31 | `layer_contract_hash` | **RED** |
| C | `layer_registry.cpp`: the shadow parser stops applying `PF_IMPLIES_ON` | some `layer_parity_*` | **RED**: cases 10, 11, 18, 34 — exactly the `--mv-smooth` / `--gme-gpu-verify` / `--no-shapefield` families |
| D | `seam_graph.hpp`: the derived barrier widened with `ALL_COMMANDS` | `seam_graph` | **RED** |

**A₀ is the most useful row in this table.** The first perturbation was too subtle to change any observable — a
1e-7 nudge before an `int` truncation lands on a different integer only when the value sits within 1e-7 of a
boundary, which never happened in 2,877 ticks. Had the exercise stopped at "I perturbed the clock and the suite
stayed green", the honest conclusion would have been "the oracle does not guard the clock" — and it would have been
wrong. The perturbation must be observable at the comparison the oracle makes; A is that, and it produced a
mismatch on **every single tick**.

## 4 · Honesty ledger

- The suite is **offline**: it exercises the seam engine, the clock's math, the registry's parser/resolver and the
  CLI's exit codes. It runs no GPU work, presents no frame, and therefore guards **none** of R3/R4/R5's runtime
  behaviour — those live in their gate records and in the two-oracle instruments inside the binary. What 4.3 buys is
  that a regression in the four things above now costs a build, not a discovery weeks later.
- `layer_contract_hash` is brittle by design: any deliberate table edit fails it until the pin is updated in the
  same commit. That is the point (touchpoints move together), and it is the one test that will annoy a future
  session — the annoyance is the mechanism.
- The parity corpus is 34 hand-chosen combinations, not an exhaustive product of the token space; it covers every
  token R5 added plus the ordering that was seen red during R5 step 1.
- The fixture is one content at one rate on one rig. A clock regression that only appears at another source rate
  would not be caught. A second fixture is cheap and is not done.
- `parse_failed` was added at two sites. There are 43 `return false` paths in `parse_args`; the rest are
  informational stops that keep exit 0. If any of them is actually an error, it still exits 0 — unclassified, and
  said here rather than claimed otherwise.
- The perturbation runs were each a full rebuild; the restores were `git checkout --` of the single file, and the
  tree was verified clean (`git status`) before the record was written.

*Made with my soul - Swately <3*
