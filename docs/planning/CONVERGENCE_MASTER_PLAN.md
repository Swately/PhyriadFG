# CONVERGENCE_MASTER_PLAN — the final FG rebuilt IN PLACE, by stage, on the LAYERTAB contract (v2)

> **Diátaxis type:** Planning / explanation (a master plan, per
> `F:\Phyriad\protocols\quality\PLAN_TIER_PROTOCOL.md`). **Plan tier:** **Tier-2** (risk-bearing: the
> stages that extract the clock and the present policy touch cross-thread SPSC state and the present
> path — the concurrency / crash class — and R0/R3 touch the shipping default under the
> byte-identical-off dogma). Linked set: this master plan ·
> [`CONVERGENCE_IMPLEMENTATION_STRATEGIES.md`](CONVERGENCE_IMPLEMENTATION_STRATEGIES.md) ·
> [`CONVERGENCE_RISK_REGISTER.md`](CONVERGENCE_RISK_REGISTER.md).
> **Status:** `in execution` (v2, 2026-09-03) — **R0 and R1 built and gated (M-R0 `records/R0_GATE.md`,
> M-R1 `records/R1_GATE.md`, M-R2 `records/R2_GATE.md`, **M-R3 `records/R3_GATE.md` (2026-09-05, gate PASSED (residual attributed))**, **M-R5 `records/R5_GATE.md` (2026-09-06, gate PASSED)**, **M-R4 `records/R4_GATE.md` (2026-09-05, gate PASSED except the forced-TDR item (built, awaiting the operator's word))**); R5+ not built**; every "exists / line N /
> measured" claim was read or computed first-hand in the authoring session; every forward number is
> labelled `estimated` or `unmeasured`. MUST / SHOULD / MAY are BCP-14.
> **v2 re-aim (2026-09-03, operator: "hagamos tu recomendación" → "Sí adelante"):** v1 (same day, ATF-
> released) hosted the convergence on `apps/minimal_fg` as the BASE and mined the shipping repo as the
> DONOR. The measurement of the base that same day (`FOTO_MENTAL.md` §4b: `in ≈ 22 fps`, `out = 240`,
> one warp per pair at `t = 0.5`, no frame multiplication) and the cost analysis (v1's base track
> re-implemented the donor's hard-won ingest, clock and pacing in a second tree) led the operator to
> choose: **restructure the shipping repo IN PLACE, by stage, with the base as the blueprint of the P
> loop and the seam engine (`seam_graph.hpp` + its test) as the ONE adopted piece.** The boundary
> definition is [`STAGE_CONTRACT.md`](STAGE_CONTRACT.md) (`approved`). v1's stage list C0–C6 is mapped
> to R0–R7 in §2.1 and kept there as the record; the search result (LAYERTAB + G1–G5,
> [`aap/A3_CHOSEN_DESIGN.md`](aap/A3_CHOSEN_DESIGN.md), gates in [`aap/AT3_AT4_ATF_VERDICTS.md`](aap/AT3_AT4_ATF_VERDICTS.md))
> is unchanged — the AAP froze the layer contract, not the host tree.
> **Supersedes** the stage list P2–P5 of `../research/MINIMAL_FG_MASTER_PLAN.md` §5 (its §0–§4, §7 and
> its register MR-1..MR-8 stay in force and are cited). **Spine:** [`ACTION_PLAN.md`](ACTION_PLAN.md) S4.

---

## 0 · Objective (carried verbatim from the spine, then narrowed)

**P (principal):** perfect PhyriadFG — match then surpass LSFG — as ONE final FG built from a clean
minimal core with the right seam, re-layered only with what earns its place, measured, not assumed.

**S4 (this plan):** make the shipping repo BE that FG, one stage at a time, without ever leaving it
unrunnable: six stages with declared contracts (STAGE_CONTRACT §1–§2), the LAYERTAB layer contract
inside stage 5, the SG seam under the GPU passes of stages 3 and 5, a CPU-testable clock as stage 4,
and the pacing/drop policy as stage 6 — with the ~50 learned runtime behaviours of the donor (the
host-staged capture, the `DO_NOT_WAIT` step-back, the fence slices, the device-loss latches, the
window-death watchdog, the saturation cure) MOVED, never re-learned.

**What "exact motion" means here (operator, 2026-09-02):** the generated frame places every moving
point where the analytic trajectory says it is at phase `t` — measured as DATA by the MOTION_TRUTH
instrument ([`MOTION_TRUTH_MASTER_PLAN.md`](MOTION_TRUTH_MASTER_PLAN.md), A0 metric **M1**). Stage 4
is where that is decided; R1 puts it first.

**A rule adopted with the re-aim (operator's diagnosis, 2026-09-03):** the documentation (34,137 lines)
outweighs the app code (13,989) 2.4:1 and most plans have no measured number behind them. **No new plan
document is written under this arc until a new measurement exists**; each R-stage ends in a number.

## 1 · What exists today — first-hand (2026-09-02/03)

### 1.1 The host (`projects/PhyriadFG`, branch `analysis/0.3.0-quality-push`, HEAD `c043780` + the E1 working tree)

| Asset | State (read) | Stage it belongs to (STAGE_CONTRACT) |
|---|---|---|
| `src/capture/capture.cpp` (971) + `capture_init.cpp` (528): WGC + DDA, the host-staged ring, `--ingest-async` worker, the convert tail (`:706–714`), the publish (`c_seq.fetch_add`, `:792`) | works; the ingest is already one upload per real frame | stages 1 + 2 (only naming + the convert/publish split are missing) |
| `src/flow/flow.cpp` (2,234): OFP fwd/bwd, CPU `gme_fit_affine` (`:44`), GPU gme (`:357–420`), candidates / dissidence / persistence / mv_target, the publish (`f_seq.fetch_add`, `:1777`, `:2191`); the holon family `object_repair` (`:885–1245`), `mem_*` (`:1275–1380`), `consume_wap` (`:1396–1809`) | works; the `FlowSet` exists as ~20 host arrays + `f_pair_*` arrays, not as a type | stage 3; the holons = FLOW rows (off) |
| `src/present/present.cpp` (3,026): the OUTPUT-CLOCK loop `:1491–3020` interleaving the clock (`:1558–1596`, `:1964–1976`, `:2022–2061`, `:2370–2497`), the warp caller `wap_warp_present` (`:879–1402`), the present (`:377–460`, `:564`, `:2593`, `:2921`), qdump (`:1315–1357`), overlay (`:1188`), MV median in the bridge cmd (`:779`), `wap_upload` (`:713–836`) | works; three stages and two planes in one body | stages 4, 5, 6 + INSTRUMENT |
| `shaders/wap_warp.comp` (1,336): core `:496–529/637/679`; `mv_fwd` `:344`, bg-reclaim damp `:394`, phase-anchor `:410`, single-track `:1321–1328` | the product's math; the override chain | stage 5 (LAYERTAB) |
| `src/cli/cli.hpp` (1,011) + `cli.cpp` (787) + `ui/src/main.js`: 257 flags vs 173 UI entries | the four-site drift | CONTROL plane |
| E1 result: `main()` 1,222; 13 `init_*`; G1 baseline 240 presents/s, 28,799 / 120 s | the M3 baseline | — |
| Instruments: `--csv`, `--qdump` (needs the sync present path; `resolve_config` AUTO-DISABLES `--async-present` for the run and says so — verified 2026-09-03, an earlier note calling it "inert under the default" was wrong), `tools/ball_zoo.ps1`, `tools/gate_zoo.ps1` (operator's), `tools/gpu_load.exe`, `fg_quality_scorer` (catalog, builds) | the M3 / M4 / load fixtures | INSTRUMENT plane |
| MOTION_TRUTH | **corrected 2026-09-04:** T0/T1/T1b/T1c `done` (gate records); T6 built, gate NOT passed; **T2–T5 unbuilt** | **M1 still not measurable** (§4.3) — the table comes from T4–T5 |

### 1.2 What is taken from the base (`apps/minimal_fg`, container, no git) — and only this

| Taken | Where it goes | Why |
|---|---|---|
| `include/minimal_fg/seam_graph.hpp` (549 lines) + `test/test_seam_graph.cpp` (436 lines, 122 checks) | `src/seam/seam_graph.hpp` + `tests/seam/` in the repo (R2) | the only asset the host lacks: declared reads/writes → derived precise barriers, cull-when-off, deterministic `dump()`; its three known gaps become grafts G3/G4/G5 |
| the SHAPE of its loop (`main.cpp:1220–1460`: select → execute graph → present) | the target shape of the P thread after R1/R4 | a blueprint, not code |
| nothing else | — | its capture/present plumbing duplicates the host's in a weaker form (measured 2026-09-03: `ALL_COMMANDS` barriers on the hot path, both anchors re-uploaded per tick, naive present, `t = 0.5`); it stays in the container as the seam's proof of concept, frozen |

## 2 · The convergence model — one tree, by stage

```
 R0  CONTROL: the layer registry (+ --layer-dump, --layer-model-json); wap_warp.comp UNCHANGED   = M-R0 (v1 M-C1)
      │  exit gate: the column-closure experiment (≥ 4 new columns → RE-SELECT toward Candidate A)
 R1  Stage 4 CLOCK extracted as a CPU value type + its synthetic-arrival test                   = M-R1
 R2  the seam adopted; stage 5's hand-written barriers become declared SG passes; grafts G3/G4/G5  = M-R2
 R3  Stage 5: fg_core.comp + the 8 fused rows replace wap_warp.comp for the default set        = M-R3 (v1 M-C2)
 R4  Stage 6 PRESENT extracted (pacing + drop + FlipStats) in the base's loop shape             = M-R4
 R5  Stage 3: FlowSet declared + FlowRing; holons → kind=P rows (off); MV median → stage 3;
     wap_upload conditional on device count                                                     = M-R5
 R6  Stages 1–2 named (capture/ + ingest/); the convert+publish tail as the ingest module         = M-R6
 R7  Closing: --legacy-warp retired only after MOTION_TRUTH M1 baselines exist on both paths;
     MR-4 byte-diff + MR-8 operator eye                                                          = M-R7 (v1 C6)
```

- **The product runs at every gate.** Every R-stage is a structure-preserving transformation on the
  shipping binary (the E1 technique: verbatim bodies + reference-only binding structs + a green build
  gate); R3 is the one stage that changes the shader and it is opt-in until M4 passes.
- **Nothing is deleted.** `wap_warp.comp` stays behind `--legacy-warp`; the holons stay as rows; the
  container's `apps/minimal_fg` stays as it is.
- **The threads stay.** C, F, P keep their count, priorities and MMCSS/pin policy; a thread becomes a
  body that CALLS its stages (STAGE_CONTRACT §0).

### 2.1 v1 → v2 mapping (the record)

| v1 (base-hosted) | v2 (in place) | What changed |
|---|---|---|
| C0 registry (donor) = M-C1 | **R0** = M-R0 | unchanged |
| C1 SG for all barriers on the base + G3/G4/G5 | **R2** in the host | the seam is adopted INTO the repo; the base's `img_barrier()` is no longer our problem |
| C2 ingest = one upload per real frame on the base | **R6** naming | the host already does this (`capture.cpp:706–792`); C2's defect was the base's |
| C3a `fg_core.comp` (donor) = M-C2 | **R3** = M-R3 | unchanged |
| C3b the core inside the base's `mc_interp` | — superseded | no base |
| C4 port the clock INTO the base | **R1** EXTRACT the clock in place, first | port → extraction; moved ahead of R3 (operator decision 4) |
| C5 pacing + drop on the base | **R4** extract PRESENT in place | the host's pacing exists; the drop policy becomes a declared `Phase.decision` |
| S4.2 per-layer ports | **R5** + the S5 net-gain gate | the holons become rows in place |
| C6 default flip; donor = A/B reference | **R7** | "the pre-R3 binary" (a git tag) is the reference, not a second tree |
| S4.0 adopt the app | **S4.0 = adopt the header + test** (R2) | operator decision 2026-09-03 |

## 3 · The stages

Each: scope · MUST NOT change · gate (DI-3 wherever a number decides: two runs per side, spread
reported, `r` for per-cell comparisons; a gate binds to the exact tree it tested).

### R0 — CONTROL: the registry with zero product risk (= milestone M-R0)

`src/layers/` (the `.def` registry, the row schema, the generated `LayerConfig` / parser / help / JSON
model, `layer_registry.cpp`) + `tools/gen_layer_glsl.cmake` + the CMake hook + `--layer-dump` +
`--layer-model-json` + the UI rendering its layer section from the binary's model. **`shaders/wap_warp.comp`
unchanged; the push block still assembled from `Config`.** A startup `layer_config_parity()` asserts the
generated `LayerConfig` field-by-field against today's `Config`.
- MUST NOT change: any default value, any shader byte, the present path's per-tick work.
- **Gate G-R0:** build ×2; parity passes; the argv round-trip over every `--flag` token is identical
  before/after AND the `--help` diff is reviewed line-by-line (XR6/DR2); `--layer-dump` prints the twelve
  rows of C §3.5 in order with the contract hash, byte-identical across two runs; M2a measured = a trivial
  `HOST` row → `git diff --stat` = 2 files; M3: `ball_zoo` 60 s × 2 runs/side vs the E1 baseline within
  spread. **The column-closure experiment** (`aap/COLUMN_CLOSURE_EXPERIMENT.md`: ten more layers mapped on
  paper, new columns counted) is R0's exit gate: `≤ 1` proceed; `2–3` design + review first; `≥ 4` →
  RE-SELECT at A3 toward Candidate A (R0's registry survives as the flag single-source).

**Result record — R0 DONE, G-R0 PASSED (2026-09-03; the full evidence is [`records/R0_GATE.md`](records/R0_GATE.md)):**
`src/layers/` (schema + 12-row registry + tables + runtime) · `tools/layer_gen.cpp` (a C++ generator from the
same tables — deviation from C §1.1's `cmake -P`, recorded) · `tools/check_flag_roundtrip.py` · host edits in
`cli.hpp/.cpp`, `main.cpp`, `CMakeLists.txt` · UI: `layer_model` command + the model-driven `Layers ·` groups,
15 hand entries deleted. **Numbers:** build ×2 exit 0 · `--layer-dump` byte-identical across 2 runs, contract
`0xB8C7BD1BC5BACB3C` · parity 0 FAIL over the default + 38 combinations + the 258-token round-trip (ok 251,
needs-arg 1, other 6, timeout 0) · `--help` diff = −7 hand lines / +24 generated · JSON 18 controls, rendered
by the UI from the binary · **M2a = 2 files** (hash snapshot; the row built, appeared in the dump/chains,
reverted) · **M3** (2 runs/side, interleaved, 60 s, `ball_zoo` 60 fps 1920×1080): presents A 14,391 ± 12 vs
B 14,383.5 ± 1 (Δ −0.05 %); uniq/s 239.15 vs 239.06; lat 15.18 ± 2.26 ms vs 16.58 ± 0.12 ms — all inside the
run-to-run spread · **column closure: 0 new columns on ten more layers → PROCEED** (2 new stage values
`WEIGHT`/`UV`, the `CH_BLEND` channel + `select` row, the core split `fg_sample`/`fg_blend` — carried into R3
as design constraints, `aap/COLUMN_CLOSURE_EXPERIMENT.md` §2). Defects fixed on the way: `requires` keyword;
the round-trip's argv order (a value flag swallowed `--dump-config`); a contaminated first M3 discarded.

### R1 — Stage 4 CLOCK, extracted as a CPU value type, with its test (= M-R1)

`src/clock/phase_clock.hpp/.cpp`: `struct PhaseClock` holding the NCO/PLL state (`present.cpp:1574–1580`),
`tick()` = the frequency loop + NCO advance (`:1964–1976`), `lock(cur_c, expected)` = the phase loop
(`:2050–2061`), `select()` = step 3 (`:1978–2021`) and step 4 (`:2188–2202`), `t_use()` = the base
derivation + the backwards clamp (`:2370–2387`, `:2497`), `monotone()` = step 6 (`:2904`). The vblank
grid origin (`--pace-vblank`, `:1734–1790`) is a `PhaseClock` input (`grid_origin_ms`). Constants
`kScFreqAlpha`, `kScPhaseGain`, `kScReseatErr` move by name. The P loop calls the type at the same
points it computed inline. **Out of R1 (phase LAYERS, S5):** `--phase-norm`, the opening-ease cubic, the
dispersion latch, the `realized_mult` governor, the laser mass feedback — they stay inline behind their
flags, reading `Phase`, until they become rows.
- MUST NOT change: any constant; the evaluation ORDER of the double arithmetic (XR14); the seq_cst reads
  of `c_seq` / `f_seq` and their positions in the tick (XR5's discipline).
**Result record — R1 DONE, G-R1 PASSED (2026-09-03; evidence in [`records/R1_GATE.md`](records/R1_GATE.md)):**
`src/clock/phase_clock.{hpp,cpp}` (621 lines, the four methods) + `tests/clock/test_phase_clock.cpp`
(`pfg_clock_test`) + the `--arrival-log` instrument; `present.cpp` 3,054 -> 2,818 lines. **Numbers:**
verbatim **98.80 %** of the moved body, five documented transforms (T1..T5) + one hoist · **replay
bit-parity: 14,390 live ticks, 0 mismatches on every field** (D, t_display, content_clock, T_robust,
selection, phase, t_use, order) · synthetic arrivals: locks in 6 ticks, 0 backsteps, 4 distinct phases
per pair, PLL tracked a 60->30 fps step · live A/B (2 runs/side, interleaved, 60 s): presented-phase
mean 0.5020 -> 0.5015, sd 0.2829 -> 0.2823, pinned-at-1 0.0181 -> 0.0159, **content step per present
0.2502 -> 0.2501 source frames (the 4x multiplication, measured)**, uniq/s and lat inside spread ·
120 s smoke exit 0, 28,797 presents. The phase LAYERS stayed in `present.cpp` behind their flags.

- **Gate G-R1:** the new test `tests/clock/test_phase_clock.cpp` (CPU-only, no Vulkan): (a) a RECORDED
  arrival log from a live run (`--phaselog` + `--csv`, 60 s `ball_zoo`) replayed through `PhaseClock`
  reproduces the recorded `disp_phase` / `disp_src` **bit-for-bit** (XR14 — the extraction oracle); (b)
  synthetic arrivals: 60 fps ± 2 ms jitter → lock within N ticks, phase error bounded; a 60→30 step →
  re-seat once; a dropped frame → no backwards step. (a) is pass/fail today; (b)'s bounds are SET from the
  first run and recorded, not asserted here. Plus build ×2, the 120 s smoke, and the `disp_phase` column of
  a live run byte-identical to the pre-R1 binary on the same replayed source (2 runs).

### R2 — the seam adopted; stage 5's barriers declared; grafts G3/G4/G5 (= M-R2)

`src/seam/seam_graph.hpp` (the header, verbatim from the container, signature added) + `tests/seam/`
(the 122-check test wired into the repo build as a standalone CMake target). Stage 5's hand-written
barriers on the warp/blit path (`present.cpp:1215–1218`; the overlay's `:1188`; `--afill`'s) become an SG
graph on P: passes `warp` (reads Pair + FlowSet images, writes `wapOutA`), `overlay` (INSTRUMENT, RMW
`wapOutA`, culled when off), `blit` (reads `wapOutA`, writes the bridge). The grafts: **G3** dominance
warning + `dump_warnings()`; **G4** `optional_write` + liveness spec constants; **G5** zero-allocation
`execute()`. The header's "no UNDEFINED→first-write transition" limitation is lifted by one explicit
rule (a layout-only barrier with `srcStage = NONE`).
- MUST NOT change: the golden `dump()` text of the existing test; the recorded command stream of the
  default path beyond barrier ENCODING (sync2 `vkCmdPipelineBarrier2` instead of `vkCmdPipelineBarrier` —
  the device must enable synchronization2; verify `core_init.cpp`'s feature chain, to confirm).
**Result record — R2 DONE, G-R2 PASSED (2026-09-03; `records/R2_GATE.md`):** Vulkan 1.3 + `synchronization2`
queried and enabled (loader 1.4.357; both devices ENABLED; `--no-sync2` is the A/B arm); `--validation`
added and **0 validation lines over 12 s**; the seam adopted as `src/seam/seam_graph.hpp` + `tests/seam/`
with grafts G3/G4/G5 and **142 checks passing** (122 adopted + 13 grafts + 7 stage-5 shape). The live
two engine gaps found by the stage-5 shape check are CLOSED — a per-image import layout
(`declare_image(..., import_layout)`) and a declared resting layout (`set_resting`) that emits a
cross-frame EPILOGUE barrier — and `--sg-barriers` now records stage 5's output barriers through the
graph, deriving EXACTLY the three hand-written ones (asserted field by field; `--sg-dump` byte-identical
x2). **148 checks**; G3 caught a real dominance in the shipping path (the blit overwrites the whole
bridge image) which is now DECLARED with its reason; **0 validation lines** over a 60 s soak saturated
with `gpu_load`; M3 on 2 runs/side inside spread (presents 14,384 on all four; warp/GPU 2.787 -> 2.880 ms
against A's own 0.315 ms spread). The derived path is OPT-IN; the default keeps the hand-written arm.

- **Gate G-R2:** the seam test passes with its new checks (G3/G4/G5 each add a counted check);
  `--sg-dump` of P's graph hand-audited and byte-identical across two runs; `grep -c img_barrier
  src/present/present.cpp` drops by exactly the declared count (quoted); `VK_LAYER_KHRONOS_validation` +
  sync-validation clean over 60 s; heap allocations on P's steady state = 0 per `execute()` (counted); M3
  within spread (2 runs/side).

### R3 — Stage 5: `fg_core.comp` replaces `wap_warp.comp` for the default set (= M-R3)

As v1 C3a, unchanged in substance: `shaders/fg_core_math.glsl` + `shaders/layers/<name>.glsl` + the
generated chains + `shaders/fg_core.comp`; `present.cpp`'s 58-float push (`:1119–1150`) + gate derivation
(`:960–1118`) → `CorePush` (20 B) + `layer_arm_mask(ArmInputs)`; the eight rows per `aap/CANDIDATE_C.md`
§4.1–4.8; bg-reclaim bug-for-bug (XR7); `--fg-core` opt-in until M4 passes, then `--legacy-warp` selects
the old path. The `ArmInputs` are derived from `FlowSet[gen]` validity ONLY (STAGE_CONTRACT §1).
- **CONSTRAINTS FROM THE R0 EXIT GATE — all three, not two** (re-attached 2026-09-04; the column-closure
  experiment `aap/COLUMN_CLOSURE_EXPERIMENT.md` §2 imposed three and this section carried none of them,
  while the spine and §2.1 carried only the first two. §2.3 survived in the experiment doc alone — a
  `grep -rn "cross-row" docs/ src/ tools/` hit exactly one file — which is how a constraint becomes a
  forgotten sentence):
  1. **§2.1 — the `WEIGHT` stage with the core split.** The frozen core splits into `fg_sample()` +
     a `WEIGHT` stage + `fg_blend()`; rows that re-weight the A/B contribution attach at `WEIGHT`.
  2. **§2.2 — the `CH_BLEND` channel with `select` as a row.** Note this contradicts the sentence below:
     the eight rows of `CANDIDATE_C.md` §4.1–4.8 (mv_guided, inertia, bg_reclaim, phase_anchor, ambig,
     vblend, single_track, stasis) contain **no `select` row and no `CH_BLEND`**. R3 builds nine rows and
     a new channel, not eight rows.
  3. **§2.3 — a declared-`needs` rule for cross-row parameter reads.** Three sites read another row's
     parameter (the matte colour cross-check reads `L_MV_GUIDED` + `commit.thresh`; `bg_fill` reads
     `bgs_w`/`bx_w`). The generator MUST require the read to be DECLARED as a `needs` bit on the row whose
     param is read, and emit the other row's UBO field by its generated alias. Today `needs` is non-zero on
     exactly one row (INERTIA) and **no code applies it** — it is declared, range-validated and printed only.
- MUST NOT change: any default output byte (M4 veto); `resolve_config()` as the owner of non-layer cascades.
- **Gate G-R3 (M4):** the T6 CPU reference warp on ≥ 200 `--qdump+` triples (motion-truth zoo + `gate_zoo`,
  `--no-async-present`): byte-identical or every differing pixel explained; the packed `1.0 + sim` value
  FIRST (XR1), the clean value second, both recorded; M3 2 runs/side; M2b by the SPIR-V variant diff +
  Nsight (`unmeasured` until run); `--layer-dump` + the hash in `--csv` and the manifest. **Precondition:**
  MOTION_TRUTH T1 (`--qdump+`) and T6 (the reference warp) built.

### R4 — Stage 6 PRESENT extracted; the drop policy declared (= M-R4)

`src/present/present_stage.hpp/.cpp`: the PresentSurface ownership (`present.cpp:377–460`), the bridge
blit + submit (`:564`, `bridge_present` `:466–507`, `bridge_present_src` `:654–693`), `rfp_present`
(`:1416–1458`), the async-present slot machinery (`:860–930`, `:1361`), step 7 (`:2593`, `:2921`), the
decimation gate (`:2062`) and the per-second stats (`:2954`) — as ONE stage module called by the P loop
with `Phase` + `GenFrame`, returning `FlipStats`. The tick decision `{warp, dup, drop, decimated}`
becomes a field of `Phase` written by stage 4 and READ here (today: `fdrop`, `async-drop`, the
decimation gate — three sites; **site references re-anchored 2026-09-04: the async drop is at
`present.cpp:962-978`, not `:1186` — that line is now inside the 58-float warp push. `present.cpp` has
been edited since this plan was written, so R4 must re-anchor every offset it cites before using them**).
The guard-band drop of a late interpolated frame (MINIMAL_FG S8, the
DLSS-G model) is added as a `Phase.decision = drop` case IF it does not exist today (to confirm: the
async-drop at `:1186` covers the in-flight case; the late-target case is `unverified`).
- MUST NOT change: the drop decision is P-local (no lock); the slot is provisioned at init; every poll
  `vk_live`-wrapped (MR-1); own-window only (MR-7); no queue split in this stage (MR-2 stays as today's
  single-submitter design).
- **Gate G-R4:** `disp_phase`/`disp_src` and presents/s byte-/spread-identical to pre-R4 on the replayed
  source (2 runs/side); under `tools/gpu_load.exe`: drops/s > 0, real frames dropped = 0 (counted);
  validation + sync-validation clean 30 s soak; forced TDR (`--tdr-test N`) → clean `g_quit` exit.

### R5 — Stage 3: `FlowSet` declared; holons as rows; the median moved; `wap_upload` conditional (= M-R5)

`src/flow/flow_set.hpp`: `struct FlowSet` (STAGE_CONTRACT §1's field list — one struct replacing the ~20
`host*` arrays + the `f_pair_*` arrays, indexed by `gen`) + the `FlowRing` (publish = `f_seq.fetch_add`,
consumer position = `p_presenting`, both kept). `object_repair` / `mem_*` / `consume_wap` extracted to
`src/flow/holons.cpp` with reference-only binding (E3's spec, CR1) and registered as FLOW-stage `kind = P`
rows, **off by default** (the shipping default already has `igpu_field=false` cascading them off — verified
by designer A; to re-confirm per row). The MV median/consensus (`present.cpp:779`, `medPipe`) moves to
stage 3 as a row. `wap_upload` (`:713–836`) is made conditional: `FlowSet` device == `GenFrame` device ⇒
no copy (single-GPU = the rig).
- MUST NOT change: the default output (all moved rows are off by default → byte-identical, MR-4); the
  governor floor and `src_interval_us` couplings become declared fields (STAGE_CONTRACT §3), same values.
- **Gate G-R5:** CSV byte-diff of the default run before/after (the rows are off); `--layer-dump` lists the
  FLOW rows `OFF`; `--sg-dump` of F's graph hand-audited; M3 2 runs/side; the 120 s smoke; PR1's A/B for
  the holon extraction (2 runs/side, the `--objects` opt-in path measured separately).

### R6 — Stages 1–2 named; the ingest module (= M-R6)

`src/ingest/ingest.cpp`: the convert + publish tail (`capture.cpp:~600–792`) and `run_convert_worker`
(`:805–971`) as the stage-2 module; `capture.cpp` keeps the acquisition branches (stage 1). The
`RawFrame` / `RealFrame` structs replace the loose locals; `RealSlot` becomes `RealFrame`'s timestamp
pair. Directory names per STAGE_CONTRACT §6 decision 2 (`warp_blend/` → `generate/`, `cli/` + `layers/`
→ `control/`).
- MUST NOT change: the publish order (`t_pub_ms` stamped before `fetch_add`); the drop-to-newest rule;
  the WGC/DDA behaviour (the byte-identical fps lines of a default run).
- **Gate G-R6:** build ×2; the 120 s smoke; the startup log diff = 0 lines (the E1 instrument); `in` /
  `uniq` / `arrived` per-second lines within spread on `ball_zoo` (2 runs/side); the `--help` round-trip.

### R7 — Closing: the override chain retired (operator decision; = M-R7)

`--legacy-warp` (and the pre-R3 binary, tagged in git as the A/B reference) is retired from the DEFAULT
build only when: MOTION_TRUTH T4–T5 are `measured` and the M1 baseline table exists for BOTH paths
(≤ 0.10 px mean shift, p95 within spread, `r ≥ 0.5`); MR-4's CSV byte-diff; MR-8's operator eye. The legacy
shader stays in the tree (never deleted).

## 4 · Milestones, measurement, the honest ceiling

### 4.1 The ladder

| Milestone | After | The number it produces |
|---|---|---|
| **M-R0** ✔ 2026-09-03 | R0 | **M2a = 2 files (measured); M3 Δ −0.05 % presents, inside spread; column-closure count = 0** |
| **M-R1** OK 2026-09-03 | R1 | **replay 14,390 ticks / 0 mismatches; lock in 6 ticks; 0 backsteps; content step 0.2501 src-frames per present** |
| **M-R2** ✔ 2026-09-03 | R2 | **148 checks; the derived barriers are field-identical to the three hand-written ones; `--sg-dump` byte-identical ×2; 0 validation lines over a 60 s saturated soak; M3 in spread** (the hand arm is kept as the opt-out fallback, so no count drop yet) |
| **M-R3** | R3 | M4 byte-diff (identical / N explained); M3; M2b attribution |
| M-R4 | R4 | drops/s under load; real-frames-dropped = 0; TDR clean |
| M-R5 | R5 | default CSV byte-identical with the rows off; holon A/B (opt-in) |
| M-R6 | R6 | startup diff = 0; ingest lines within spread |
| M-R7 | R7 | the M1 tables on both paths; the operator's verdict |

### 4.2 What measures what

**M1** MOTION_TRUTH T4–T5 (blocked until `measured`; R1's replay parity is NOT M1 — it proves the
extraction preserved the clock, not that the clock is right) · **M2a** `git diff --stat` · **M2b** SPIR-V
variant diff + Nsight / `--sg-dump` · **M2c** `sizeof(CorePush) = 20`, the `fg_core()` signature (6 contract
parameters / 9 arguments / 5 descriptors, all three stated) · **M3** `--csv` presents/s, warp-record, GPU
time, `lat` · **M4** the T6 reference warp on the `--qdump+` record. **DI-3** everywhere a number decides.

### 4.3 The honest ceiling

- M1 has no measured number for ANY path yet; every M1 gate is stated as blocked, never proxied (XR9).
- LAYERTAB's column set is not proven closed; R0's exit experiment is the falsifier with the RE-SELECT
  path written in. The `shadows` reach of `single_track` is a declared wart.
- R1's synthetic-test bounds do not exist until the first run; only the replay parity is pass/fail now.
- The guard-band drop's existence in the host is `unverified` (R4 confirms or adds it).
- M3 forward numbers are `estimated` (C §6) or `unmeasured`; M-R0's is the first measured one.

## 5 · Tier-2 risk surface (detail → the register)

Carried: MR-1..MR-8 (re-scoped to R2/R4/R5/R7), CR1 (R1, R4, R5), PR1 (R1, R3, R4, R5), DR2 (R0). New:
XR1 (packing 1-ulp, R3), XR2 (generator miscompile, R0/R3), XR3 (column non-closure, R0 exit), XR4
(pipeline rebuild on toggle, R3), XR5 (`arm_mask` snapshot, R3), XR6 (two instruments for the flags, R0),
XR7 (bg-reclaim bug-for-bug, R3), XR8 `superseded` (no base build), XR9 (gate substitution, R1/R7), XR10
(re-scoped: the header's two copies drift), XR11 (G5 handle patching, R2), XR12 `superseded` (the host's
ring is the ingest), XR13 (CMake `DEPENDS` stale build, R0/R3), **XR14** (new: `t_use` bit-parity after the
clock extraction, R1). No stage is committed while a risk scoped to it is `open`.

## 6 · What this plan deliberately does NOT do

- Does not adopt `apps/minimal_fg` as an app, does not build it further, does not delete it.
- Does not fix the dead bg-reclaim (XR7), re-tune any constant, or remove any layer or flag.
- Does not port the perceptual layers into the default (they become rows, off; S5 net-gain gate later).
- Does not build MOTION_TRUTH (S2, its own plan) — but R3's gate needs its T1 + T6, and R7 needs T4–T5.
- Does not touch multi-GPU beyond making the `wap_upload` condition explicit (decision 3).
- Does not write another plan document before a measurement exists (§0's rule).

## 7 · Dead-ends kept from the spine (do not re-try)

- A `--minimal` strip flag on the accreted `main.cpp` — rejected 2026-06-26 (measures with the bad seam).
  R2 is not that: it replaces the bad seam in place.
- Present-hook / injection — rejected (anti-cheat; the external-capture identity).
- Keyed-mutex cross-thread capture — deadlocked under saturation (BF6); the host-staged ring stays.
- Order-independence claims for MV-conditioning layers — false; the contract is a declared, printed order.
- A second FG tree as the host of the convergence — measured 2026-09-03 as a re-implementation of the
  donor's hard-won behaviours; retired with v1 (this v2 is the record).

## 8 · Honesty ledger

- Nothing in §3 is built. Line numbers cite the trees as read on 2026-09-02/03 (`c043780` + the E1
  working tree); they shift as stages land and each stage's record SHOULD refresh them.
- The R1 site list (steps 1–6 of the OUTPUT-CLOCK loop) is read from the numbered markers; the exact
  span of step 3 (`:1978–2021`) and step 4 (`:2188–2202`) is inferred from the neighbouring markers and
  is *to confirm* before extraction.
- synchronization2 availability in the host's device creation is *to confirm* (R2).
- The v1 triad was ATF-released; this v2 changes the HOST, not the contract. The AAP gates judged the
  layer contract (A0 §0: "how the generation core and its layers are expressed"); they did not judge the
  host-tree choice, which A0 §3 explicitly left as an operator decision. No gate is re-run for v2.
- Didn't measure: any forward M3 number; pipeline-creation time (C §6.6); whether the driver DCEs a
  spec-constant-gated body.

## 9 · Links

- Triad: [`CONVERGENCE_IMPLEMENTATION_STRATEGIES.md`](CONVERGENCE_IMPLEMENTATION_STRATEGIES.md) ·
  [`CONVERGENCE_RISK_REGISTER.md`](CONVERGENCE_RISK_REGISTER.md). Boundaries: [`STAGE_CONTRACT.md`](STAGE_CONTRACT.md).
- The decided layer contract: `aap/A3_CHOSEN_DESIGN.md` · `aap/A3_SELECTION_RATIONALE.md` ·
  `aap/CANDIDATE_C.md` · `aap/A0_FROZEN_OBJECTIVE.md` · `aap/AT3_AT4_ATF_VERDICTS.md`.
- The June arc: `../research/MINIMAL_FG_MASTER_PLAN.md` (§0–§4, §7–§8 in force), `_IMPLEMENTATION_STRATEGIES.md`
  (S8 reused by R4), `_RISK_REGISTER.md` (MR-1..MR-8 carried).
- The host's preparation: `RESTRUCTURE_PLAN.md` (E1 done; E2 → R1/R4, E3 → R5, E4 → R0).
- The instrument: `MOTION_TRUTH_MASTER_PLAN.md` (T1/T6 feed R3; T4–T5 gate R7).
- The spine: [`ACTION_PLAN.md`](ACTION_PLAN.md) S4.

*Made with my soul - Swately <3*
