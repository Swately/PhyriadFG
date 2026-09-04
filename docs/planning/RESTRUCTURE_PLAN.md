# Structure finish — completing the de-monolithization (the RESTRUCTURE arc)

Tier-2 planning doc (`*_MASTER_PLAN` + `*_IMPLEMENTATION_STRATEGIES` fused, one doc — house style
per `SATURATION_PLAN.md`; the qualifying risk makes it Tier-2, so the risk treatment lives in the
linked **[`RESTRUCTURE_RISK_REGISTER.md`](RESTRUCTURE_RISK_REGISTER.md)** and this plan only
summarizes it). Status: **`designed`**, with **E0 executed and gate G0 passed** (2026-08-31:
four inventories delivered and spot-verified first-hand — record in
[`RESTRUCTURE_INVENTORY.md`](RESTRUCTURE_INVENTORY.md) §0; raw delegate tables in
[`RESTRUCTURE_INVENTORY_RAW.md`](RESTRUCTURE_INVENTORY_RAW.md)). **E1 executed and gate G1
PASSED (2026-08-31/09-01)** — see §3-E1 for the result record. E2+ have not executed. Classification rationale: stages E2/E3 convert
`[&]` thread-body lambdas into free functions; a capture slip (an accidental copy where a reference
was captured) over cross-thread SPSC state is a presumptive concurrency hazard
(PLAN_TIER_PROTOCOL §1.1.2), and "when in doubt, escalate one tier".

The key words MUST / MUST NOT / SHOULD / MAY are BCP-14. Every number under "MEASURED" below was
computed first-hand on 2026-08-31 (grep/wc/brace-balance scripts over the working tree at commit
`c043780` + the operator's uncommitted `cli.hpp` change); nothing in this document is an estimate
shaped like data.

---

## 0 · Objective and origin

The operator's ask (2026-08-31): the codebase *feels* like "multiple monoliths"; produce a better
structure and an ordered implementation plan. The analysis confirmed the feeling but sharpened the
diagnosis: the repo is the **planned residue of an already-executed de-monolithization**
(`docs/SEPARATION_PLAN.md`, steps 1–5.4 done: `main.cpp` went 9,912 → 3,133 lines). What remains
is not un-modularized code — it is three giant *function bodies* plus one four-way-synchronized
flag surface. This arc finishes that separation.

**This arc creates no new features.** Every stage is a structure-preserving transformation with a
green build gate; observable behavior MUST be unchanged at every gate (E4's gate makes that
byte-comparable).

## 1 · MEASURED — the four real monoliths (first-hand, 2026-08-31)

| # | Body | Size (computed) | Content |
|---|---|---|---|
| M1 | `src/core/main.cpp` — one `main()` from line 164 | 3,133 lines file; ~2,270-line init-seq (≈200–2440) + main-loop/pump + teardown | cold init, runs once |
| M2 | `src/present/present.cpp` — one `run_present()` from line 55 | 3,026 lines; alias preamble 56–235; 7 nested lambdas; OUTPUT-CLOCK loop ≈1494–3020 | hot thread P body |
| M3 | `src/flow/flow.cpp` — `run_flow()` from line 473 | 2,234 lines file; ~1,760-line body | hot thread F body |
| M4 | the flag surface | `Config` ≈207 fields (`cli/cli.hpp`, 1,011 lines); **257 distinct `--flags` in cli** vs **173 in the UI model** (`ui/src/main.js`); 84 CLI-only, 0 UI-only (computed set-diff) | four hand-synced sites: struct + parse + help + UI model |

Measured lambda spans (brace-balance script): `wap_warp_present` **524** lines
(`present.cpp:879–1402`), `wap_upload` **124** (713–836), `rfp_present` 43 (1416–1458),
`bridge_present` 42 (466–507), `bridge_present_src` 40 (654–693); `object_repair` **361**
(`flow.cpp:885–1245`), `consume_wap` **414** (1396–1809), `mem_refresh` 57 (1324–1380),
`mem_merge` 21, `mem_advect` 16. `warp_blend/warp_blend.cpp` is **156 lines** — the module the
architecture map names as the warp/blend/quality home is a shell; the real code is M2's lambdas.

## 2 · The design contract — what this arc MUST NOT change

1. **Hot thread bodies stay single-TU per thread.** The performance rationale in
   `docs/ARCHITECTURE.md` (hot work in thread bodies, normal inlining, LTO for the rest) is a
   standing design decision. E2/E3 move code *between* TUs but each extracted function is called
   from exactly one thread body; LTO (`INTERPROCEDURAL_OPTIMIZATION`, on since the current build)
   covers the cross-TU inlining. This is that document's claim, adopted as a constraint —
   **not re-measured here** (see §7 and risk PR1).
2. **`resolve_config()` stays the single owner** of cascades and derived predicates. No stage
   moves a predicate out of it.
3. **The governor control-word contract** (single producer P, single consumer F, advisory atomic)
   is untouched.
4. **The verbatim-body discipline** (SEPARATION_PLAN's technique: bodies move verbatim; only the
   binding layer changes) applies to every extraction in this arc.
5. **Cuerpos-verbatim implies diffs are mechanical:** a stage's review artifact is "the moved
   region is byte-identical modulo the binding preamble", checkable with `git diff --color-moved`.

## 3 · The stages (ordered by value/risk; each gate-bound)

### E0 — Pre-flight inventories (no code touched) — **DONE, G0 PASSED (2026-08-31)**

Four read-only inventories, delegated to light models per `SUBAGENT_DELEGATION_PROTOCOL`
(R2: their output is a claim; the supervising session re-verifies samples first-hand before any
stage consumes them). Deliverable: `RESTRUCTURE_INVENTORY.md` beside this plan.

- I1: `main.cpp` init-seq section map (line ranges → destination module → key locals).
- I2: capture lists of M2's five extractable lambdas (identifier → alias/loop-local/other-lambda).
- I3: capture lists of M3's holon family + the extraction-order dependency.
- I4: triage of the 84 CLI-only flags (UI-covered-indirectly / deliberate-CLI-only / likely-drift).

**Gate G0:** each inventory spot-checked first-hand (≥5 samples per inventory, both directions:
a listed item verified true, an unlisted candidate verified absent). The plan's later stages MUST
cite the verified inventory, never the raw delegate output.

### E1 — Thin `main()`: finish SEPARATION step 5.5 — **DONE, G1 PASSED (2026-09-01)**

**Result record** (every number computed; command outputs in the session log):
`main.cpp` 3,133 → **1,222 lines**; the init-seq now lives as 13 `init_*` functions in
`core/core_init.cpp` (758), `capture/capture_init.cpp` (528), `warp_blend/warp_blend_init.cpp`
(371), `flow/flow_init.cpp` (226), `present/present_init.cpp` (220), with the ownership structs
in `core/app_init.hpp` (448). The main loop (main.cpp 530–1058), the `done:` teardown
(1059–1222) and the FgContext aggregate are textually untouched (the 3 diff hunks end at the
main-loop line). Deviations from the first cut, documented: the hoisted-declaration block was
502–844 (not 502–622 as I1 first mapped — it is ALL declarations, the goto-cleanup hoist); the
--rfp/--motion-fallback co-arm guards moved WITH the bridge function (same execution order);
27 `goto done` → `return false` substitutions (counted per extraction); EOLs normalized to CRLF.
**G1 evidence:** build.bat + build-release.bat exit 0 on the final tree · `--help` byte-identical
to the pre-E1 baseline (`FC: no differences encountered`, 11,052 bytes) · 120 s smoke
`--monitor 0 --exit-after 120` exit 0 with clean teardown (28,799 presents = the 240 Hz panel
rate, same as baseline) + a 30 s post-CRLF confirmation run exit 0 (7,199 presents) · startup
log diff vs baseline = **0 lines** (32 startup lines identical) · verbatim check computed over
the 2,138 removed lines: 98.37 % reappear byte-identical in the new files; the 32 non-matching
are all documented transform classes (declaration→member moves of Phase A, reworded moved
comments, `goto`→`return`) — zero executable lines lost.

Original stage spec (kept for the record):

Move the ~2,270-line init-seq into per-module init functions, verbatim, in the current execution
order: `init_capture(...)` → `capture/`, `init_devices/init_images/init_cmd(...)` → `core/`,
`init_flow_pipes(...)` → `flow/`, `init_wap(...)` → `warp_blend/`, `init_present_bridge(...)` →
`present/`. Each function takes `Config&` + an **ownership struct** it fills (the locals its
section declares that survive into the main loop — per inventory I1); `main()` becomes
parse → resolve → `init_*` chain → `FgContext` aggregate → 3 threads → join → teardown
(target ≈300–400 lines).

- The init ORDER is 1:1 with today's; no reordering, no interleaving change (risk CR2).
- Teardown stays in `main()` in this stage (its inverse-order coupling is the trap; moving it is
  NOT in scope — risk FR1).
- **Gate G1:** build green (both .bats) + `--help` byte-identical + smoke
  `--monitor 0 --exit-after 120` runs to clean exit + `git diff --color-moved` shows moved-verbatim.

> **Re-scope (2026-09-02, operator directive: modular structure + exact motion first).** E1 stands as
> the donor-preparation it was. E2–E6 below are **`parked`**: the shape of the extraction depends on the
> LAYER CONTRACT the architecture search in [`aap/A0_FROZEN_OBJECTIVE.md`](aap/A0_FROZEN_OBJECTIVE.md)
> selects (a staged-pass design would make E2/E3 its first convergence step; a fused-variant design would
> make them moot), and E7's equivalence test is now S2.T6 of `MOTION_TRUTH_MASTER_PLAN.md`. The spine is
> [`ACTION_PLAN.md`](ACTION_PLAN.md) (S1 = this plan; S2 = motion truth; S3 = the search; S4 = convergence).
> The stage specs are kept below unchanged for the record.
>
> **Re-aim (2026-09-03, design released).** The search chose **LAYERTAB** (`aap/A3_CHOSEN_DESIGN.md`;
> AT3 `CONVERGED`, AT4/ATF `APPROVED FOR HANDOFF` in `aap/AT3_AT4_ATF_VERDICTS.md`). Consequences for
> this arc, recorded here so the stage list stays honest: **E2**'s `wap_warp_present` half is subsumed by
> [`CONVERGENCE_MASTER_PLAN.md`](CONVERGENCE_MASTER_PLAN.md) stage C3a (the 58-float push assembly becomes
> `CorePush` + `layer_arm_mask()`); the bridge/rfp lambdas stay `parked`. **E3** is re-aimed to spine node
> S4.2 (the flow producers port into the base as `kind = P` SG passes, one at a time). **E4** is
> `superseded` by C0 for layer flags (the registry is the single source; V-B stays an option for the ~150
> host flags). Risks CR1 / PR1 / DR2 are carried into `CONVERGENCE_RISK_REGISTER.md` (see its "Carried"
> table); they stay `open` here until the arc that exercises them closes them.

### E2 — Make `warp_blend/` real (M2's five lambdas → free functions) — **`parked` (see re-scope note)**

Extract `bridge_present`, `bridge_present_src`, `wap_upload`, `wap_warp_present`, `rfp_present`
from `run_present` into `warp_blend/` (the warp/blend family) and `present/` (the bridge/rfp
family) as free functions. Binding technique (risk CR1's mitigation): each function receives
`FgContext&` plus ONE call-site struct whose members are **all references** (`T&`), built at the
old lambda's definition point from inventory I2's verified capture list — a by-value member is the
capture-slip bug this register exists for, so the struct definitions are reviewed field-by-field
against I2 and the compiler binds the rest.

- `wap_warp_present` calls sibling lambdas (I2's dependency note): extraction order within E2 is
  leaves-first.
- **Gate G2:** build green + the G1 smoke + a 60 s `ball_zoo.ps1` A/B (same flags, before/after)
  with output uniq/s and `lat` within run-to-run noise (two runs per side — the DI-3 discipline:
  the run-to-run spread IS the noise bar the comparison must clear).

### E3 — Extract the holon/quality family from `run_flow` (highest value)

`object_repair`, `mem_advect/merge/refresh`, `consume_wap` → `flow/holons.cpp` (+ `holons.hpp`),
same reference-struct binding, order per I3's dependency note (the four leaves first;
`consume_wap` last — it calls all four). Scope notes from the verified I3: the `ObjCluster` /
`ObjSlot` / `WakeRec` struct definitions live INSIDE `run_flow` (791/807/826) and move to
`holons.hpp`; `consume_wap` also uses three flow-local helper lambdas (`flow_submit_nowait` 632,
`flow_submit_q2_chain` 650, `flow_downsample` 706) — E3 passes them as callables or extracts them
too, decided at stage entry. This is the algorithmically densest code in the project; extraction
is what makes E6 (CPU-kernel testbench) possible.

- **Gate G3:** = G2 (build + smoke + ball_zoo A/B, two runs per side).

### E4 — One source of truth for the flag surface (the only behavior-surface stage)

Today four sites sync by hand (M4). Two variants, decided at E4 entry with the I4 triage in hand:

- **V-A (generator):** a single flag table (X-macro `flags.def` or equivalent) generates the
  `Config` fields + `parse_args` cases + `print_help` text, and emits the JSON model `main.js`
  consumes. Kills the drift class; adds build tooling (risk PR2).
- **V-B (verifier, cheaper):** keep the four sites; add `tools/check_flag_sync.py` that extracts
  both sets (the §1 M4 computation, scripted) + the UI model's emit rules and FAILS on drift;
  wire it as a manual pre-release gate. Catches the drift class without generation complexity.

The plan RECOMMENDS V-B first (it is one afternoon and immediately catches today's real drift, if
I4 confirms any bucket-C entries), with V-A as a later arc if the drift recurs. **Gate G4:**
`--help` byte-identical + an argv round-trip test over the full flag set (`parse_args` →
re-serialize → compare) + (V-A only) the generated `main.js` model diff-reviewed against the old.

### E5 (optional) — `cli.hpp` split (Config / Derived / telemetry)

Navigation value only; measured full rebuild is 15.3 s, so the compile-cost argument is weak.
MAY be folded into E4-V-A (the generator naturally splits the header) or dropped.

### E6 (optional) — CPU-kernel testbench

Fixture-driven tests for `gme_fit_affine`, `object_repair`, the pacing math — takes the
`FG_TESTBENCH_*` research docs (present in `docs/research/`, execution status **not verified**)
from paper to code. Depends on E3. Out of this arc's gates; listed so the order is visible.

## 4 · The gate ladder (summary)

| Gate | After | Must pass |
|---|---|---|
| G0 | E0 | inventories spot-verified first-hand (≥5 samples each, both directions) |
| G1 | E1 | build ×2 + `--help` byte-identical + 120 s smoke + `--color-moved` verbatim check |
| G2 | E2 | G1 set + ball_zoo A/B, 2 runs/side, deltas within measured run-to-run spread |
| G3 | E3 | same as G2 |
| G4 | E4 | `--help` byte-identical + argv round-trip over the full flag set |

A stage MUST NOT begin while the previous gate is unpassed. Every gate run is reported with its
actual command output (CONDUCT §3.8), and a gate binds to the exact tree it tested — any edit
after a gate voids it for the new tree.

## 5 · Risk summary

Full treatment in [`RESTRUCTURE_RISK_REGISTER.md`](RESTRUCTURE_RISK_REGISTER.md) (CR1 capture-slip
copy over SPSC state; CR2 init-order perturbation; FR1 teardown-order coupling; PR1 de-inlining
regression vs the max-performance dogma; PR2 E4-V-A tooling debt; DR1 delegated-inventory error
propagation). Per PLAN_TIER §3, no stage of this arc is committed while a risk whose scope covers
that stage is `open`.

## 6 · What this plan deliberately does NOT do

No feature work; no shader moves (the 15 flat `.comp` files are fine); no `framework/` changes
(vendored pillars); no teardown extraction (FR1); no test framework beyond E6's scoped testbench;
no renaming of modules or namespaces. `HardwareTopology.cpp` (1,686 lines, `framework/`) is
vendored substrate code and out of scope.

## 7 · Honesty ledger

- The three thread bodies were NOT read line-by-line; spans come from a brace-balance script
  (computed, but a pathological brace-in-string could shift an end line).
- The "LTO makes the split perf-free" claim is `docs/ARCHITECTURE.md`'s, adopted as a constraint
  and re-checked empirically only at G2/G3's A/B — not before.
- The SATURATION/0.4.0 arc's formal closure was not verified; if it is still open, this arc
  starts only after it closes (restructure between arcs, never during — the operator decides).
- E0's inventories are delegated output and remain claims until G0's spot-checks pass.
- Didn't measure: any perf number for the post-restructure tree (none exists yet).
