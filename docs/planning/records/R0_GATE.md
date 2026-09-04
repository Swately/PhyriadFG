# R0 — gate record (G-R0, milestone M-R0) · 2026-09-03

> Stage R0 of [`../CONVERGENCE_MASTER_PLAN.md`](../CONVERGENCE_MASTER_PLAN.md) §3 (strategies X1 · X2 · X3 of
> `../CONVERGENCE_IMPLEMENTATION_STRATEGIES.md`). Every number below is quoted from command output captured in
> the session; the raw artifacts named are in the session scratchpad (ephemeral) except the round-trip record,
> which lives beside this file. Tree: `c043780` + the E1 working tree + the R0 edits (uncommitted; commit not
> requested). **Verdict: G-R0 PASSED. The R0 exit gate (column closure) → PROCEED to R1/R3.**

## 1 · What was built (the registry as a SHADOW; the product path untouched)

| Piece | File(s) | Size |
|---|---|---|
| the row schema | `src/layers/layer_abi.hpp` | LAYER columns + CLI-COMPAT columns (see the closure experiment §0) |
| THE registry | `src/layers/layer_table.def` | 12 rows (10 layers + 2 pseudo-rows), 12 params — the shipping default set of `wap_warp.comp` per `aap/CANDIDATE_C.md` §4 |
| the tables + static checks | `src/layers/layer_table.hpp` | dense ids · unique rank per stage · targets exist · params sane · stage write legality · pseudo-rows clean |
| the runtime | `src/layers/layer_config.hpp`, `src/layers/layer_registry.cpp` | shadow parser · pre-cascade snapshot · parity oracle · `--layer-dump` · `--layer-model-json` · `--dump-config` · generated help · FNV-1a contract hash |
| the generator | `tools/layer_gen.cpp` → `build*/gen/shaders/{layer_specconst,layer_params,layer_includes,chain_mvcond,chain_sample,chain_compose}.glsl`, `layer_offsets.hpp`, `layer_manifest.txt` | a host tool built from the SAME X-macro tables (deviation from C §1.1's `cmake -P` script — C offered the stdlib alternative; the compiled table cannot drift) |
| the round-trip instrument | `tools/check_flag_roundtrip.py` → `records/r0_roundtrip.txt` | 258 tokens |
| host edits | `src/cli/cli.hpp` (+`LayerConfig`, the shadow, 3 diagnostic bools), `src/cli/cli.cpp` (shadow call at the loop top; pre-cascade snapshot; 3 tokens in `parse_extra`; 7 hand help lines → `print_layer_help()`), `src/core/main.cpp` (parity abort exit 3; the 3 diagnostics), `CMakeLists.txt` (`pfg_layer_gen` + the custom command with `DEPENDS` on the .def/.hpp) | `git diff --stat`: cli.cpp 15 ±, cli.hpp 11 ±, main.cpp +8, CMakeLists +26 |
| UI | `ui/src-tauri/src/lib.rs` (+`layer_model` command, 6 s guard like `list_monitors`), `ui/src/main.js` (−15 hand entries, + the async model-driven `Layers ·` groups) | lib.rs +40, main.js +24/−30 |

**What R0 does NOT do (by design):** the 58-float push is still assembled from `Config` (R3); the cascades
(`apply_cascades`) still own the `requires/excludes` semantics — parity is measured PRE-cascade; the hand
parser's cases stay (both parsers run; the registry only peeks); nothing includes the generated GLSL.

## 2 · The gate, item by item

| Check | Result (quoted) |
|---|---|
| build ×2 | `build.bat` exit 0 (after one fix: `requires` is a C++20 keyword → field renamed `needs`); `build-release.bat` exit 0, "LTO/IPO enabled" |
| `--layer-dump` determinism | run 1 vs run 2: `RUN1==RUN2 byte-identical`; `contract=0xB8C7BD1BC5BACB3C (9 rows enabled of 10; 12 params)`; prints the 12 rows in (stage, rank) order + the `CORE fg_core res_ceil=32.000 improv=0.200 agree=0.050` line; `unless={mv_edge_snap}`, `requires={mv_edge_snap|mv_guided}`, `OVERRIDE`, `shadows=0xF`, the two `dominates_ok` lines |
| `--layer-dump` with flags | `--no-stasis --bg-reclaim 2 --mv-edge-snap 2 --vblend-exact --st-no-stasis --mes-sim 0.3 --inertia-thresh 0.7` → `mv_edge_snap ON variant=2 sim=0.300`, `inertia thresh=0.700`, `bg_reclaim strength=2.000`, `vblend exact=1`, `stasis OFF`, `single_track no_stasis=1`; hash `0xB9035B9CC1475711` |
| parity (`layer_config_parity`, effective values, pre-cascade) | default + 38 hand-picked combinations (incl. `--no-mv-guided --mv-sim 0.2`, `--bg-reclaim` bare/0/2.5/9/−1/`abc`, `--bg-reclaim-strength 0/1`, `--mv-edge-snap 0/1/2/3`, `--mes-sim 0.001`, `--no-vblend --vblend-exact` both orders, `--inertia-thresh 5`, `--stasis-thresh 0.01`, `--no-single-track --single-track`, `--single-track --st-no-stasis --no-stasis`): **0 PARITY FAIL**, rc=0 each; the equivalence classes collapse to the same hash (e.g. `--bg-reclaim 9` = default; `--mv-edge-snap 3` = default; `--no-bg-reclaim` = `--bg-reclaim 0` = `--bg-reclaim-strength 0` = `0xDB4276C224F3CE77`) |
| round-trip (X2, instrument 1) | `records/r0_roundtrip.txt`: **258 tokens, ok=251, needs-arg=1 (`--qdump`, 2 args), other-rc0=6 (`--capture-api --convert-gpu --fg-gpu --gpu-priority --output-clock`: enum values my guesses did not hit; `--windowed`: retired flag), parity-fail=0, timeout=0**; 4.7 s. This record is R3's oracle. |
| `--help` diff (X2, instrument 2) | 11,127 → 14,205 bytes; exactly: the `Usage:` path line (argv[0]), **−7** hand lines (`--no-mv-guided --no-stasis --no-inertia --no-ambig --mv-sim --stasis-thresh --inertia-thresh`), **+24** lines of the generated `LAYERS` section (which ADDS the previously undocumented `--mv-edge-snap --mes-sim --bg-reclaim [F] --bg-reclaim-strength --no-phase-anchor --no-vblend --vblend --vblend-t0 --vblend-strength --vblend-exact --no-single-track --single-track --st-no-stasis` — the CLI-only gap closing for the rows) |
| `--dump-config` | `sizeof(Config)=1432 rows=12 params=12`; `[old]`/`[new]`/`[hash]` lines |
| `--layer-model-json` | parses (`json.load`): **18 controls**, contract `0xB8C7BD1BC5BACB3C`; groups MV(10) / Warp(4) / Composition(4) |
| UI renders from the model | `cargo build --release` 46.8 s exit 0; `ui.exe` launched beside the R0 `phyriad_fg.exe` (md5 `E0BB3122` both): PrintWindow 3384×3761 — three groups `LAYERS · MV`, `LAYERS · WARP`, `LAYERS · COMPOSITION`, each noted "From the binary's registry (contract 0xB8C7BD1BC5BACB3C)", 18 controls with the binary's defaults (mv_guided sim 0.1, inertia 0.5, bg_reclaim 4, vblend 0.6/0.5, stasis 0.5, …); the 15 hand entries GONE from Quality / Flow / Warp-Blend / GME groups (verified on the same capture) |
| generated GLSL (the mechanism) | 8 files in `build/gen/shaders/`; `chain_mvcond.glsl` = the ranked chain with `!L_MV_EDGE_SNAP` exclusion, `(L_MV_EDGE_SNAP \|\| L_MV_GUIDED)` requires-ANY, `arm_mask` tests on bits 5/6/8, the two pseudo-row statements at ranks 25/45; `layer_params.glsl` 12 fields / 48 B std140; `layer_specconst.glsl` 10 constants |
| **M2a** (A0 M2a, target ≤ 2) | add a trivial COMPOSE row `demo_noop` (.def block) + its body file: hash-snapshot diff of `src shaders tools CMakeLists.txt` = **2 files** (`src/layers/layer_table.def`, `shaders/layers/demo_noop.glsl`); the row built (incremental `build.bat` exit 0), appeared in `--layer-dump` (`COMPOSE 295 demo_noop F arm=ALWAYS OFF gain=1.000`), in `chain_compose.glsl` and `layer_includes.glsl`; reverted; tree hash-identical to before. (`git diff --stat` cannot count inside an untracked dir — the whole tree is uncommitted — hence the hash instrument.) |
| **M3** (A0 M3: no regression beyond the run-to-run spread) | `ball_zoo.ps1 -Fps 60 -W 1920 -H 1080`, `--window 'RA Ball Zoo' --exit-after 60 --csv`, interleaved A,B,A,B; A = the pre-R0 release binary (md5 `885c2cf0`), B = R0. **presents/60 s: A 14,397 / 14,385 (mean 14,391, spread 12) · B 14,384 / 14,383 (mean 14,383.5, spread 1)** → Δ −7.5 (−0.05 %), inside A's spread. **uniq/s (CSV `uniq_per_s`): A 239.15 (spread 0.23) · B 239.06 (0.00)**. **lat (CSV `MsAddedLatency`, EMA): A 15.18 ms (spread 2.26) · B 16.58 ms (0.12)** → Δ +1.40 ms, inside A's spread. `in/s` 59.9 on all four. (`output_fps` column is NA in this build; presents come from the exit line.) A FIRST A/B was discarded: a stray FG process from the round-trip bug was alive during it (its numbers: A 14,382/14,383, B 14,397/14,383 — indistinguishable, but the protocol says quiet machine). |
| **R0 exit gate — column closure** (X3, XR3) | `aap/COLUMN_CLOSURE_EXPERIMENT.md`: ten more `wap_warp.comp` layers mapped on paper → **0 new columns** (rule `≤ 1` → PROCEED); 2 new stage VALUES (`WEIGHT`, `UV`), 11 channel payloads, 1 invariant relaxation (`CH_BLEND`), 1 core split (`fg_sample` + WEIGHT + `fg_blend`), 7 gates that are params not rows; R0's own growth vs C §1.4 recorded: 6 columns (2 LAYER, 4 CLI-COMPAT) |

## 3 · Defects found and fixed during R0 (kept for the record)

- `requires` as a struct field name — a C++20 keyword (C2059) → `needs`.
- `tools/check_flag_roundtrip.py` v1 appended `--dump-config` AFTER the token: a value-taking token
  (`--window`, `--low-d-span-cap`) swallowed it and the FG started for real (a 434 MB capture process left
  behind, killed). Fix: `--dump-config` first in argv; timeout → `taskkill /F /T`. The M3 A/B was re-run
  clean after the kill (§2).
- My PowerShell `Where-Object CommandLine` filter matched nothing in 5.1 (no such property on `Get-Process`)
  → the stray python survived one kill pass; `Get-CimInstance Win32_Process` was used instead.
- `git status` snapshots cannot count a file edited inside an untracked directory → the M2a instrument is a
  hash snapshot.

## 4 · Honesty ledger

- R0 changes NO product behaviour: no shader byte, no push field, no default; the present path is untouched
  (the diff touches `cli/`, `core/main.cpp` after parse, `CMakeLists.txt`, `ui/`). MR-4's CSV byte-diff was not
  run (the telemetry has timestamps); the M3 A/B + the parity sweep are the evidence that the default is the
  same default.
- The parity oracle compares EFFECTIVE values (what the push reads: a gated param is 0 when its row is off),
  pre-cascade. A cascade that later disables a layer (`--no-warp-at-presenter`, `--no-gme`, `--no-bidir`) is
  NOT mirrored in the registry yet — R3's `needs` resolution takes that over.
- `--bg-reclaim abc` → the hand parser fails on `abc` as an unknown option (exit before parity) — both parsers
  agree on the peek rule, but the case is "parse fails", not "parity holds".
- The UI verification is a rendered capture read by me, not a data assertion; the data-level proof is the
  JSON (18 controls) + the Rust command being the `list_monitors` pattern. The PNG stays in the scratchpad
  (ephemeral); the observation is recorded above.
- The M3 `lat` delta (+1.4 ms) is 62 % of A's own spread; by DI-3 it is not a regression signal, and R0 does
  not touch the present path, but it is reported as measured, not rounded away.
- Not measured: M2b/M2c (R3), M1 (blocked on MOTION_TRUTH), pipeline-creation time (no pipeline changed).

*Made with my soul - Swately <3*
