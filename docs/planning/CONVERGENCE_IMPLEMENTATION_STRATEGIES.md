# CONVERGENCE_IMPLEMENTATION_STRATEGIES — the concrete HOW per R-stage, with the risk IDs at every edit site (v2)

> **Diátaxis type:** how-to (the IMPLEMENTATION_STRATEGIES leg of the Tier-2 triad with
> [`CONVERGENCE_MASTER_PLAN.md`](CONVERGENCE_MASTER_PLAN.md) and
> [`CONVERGENCE_RISK_REGISTER.md`](CONVERGENCE_RISK_REGISTER.md); PLAN_TIER_PROTOCOL §2.3: every
> strategy cites the risk ID(s) it mitigates). **Status:** `designed` (v2, 2026-09-03) — nothing below is
> built; every cited site was read first-hand; *to confirm* marks a site the implementer re-reads before
> editing. MUST / SHOULD / MAY are BCP-14.
> **v2:** the host is the shipping repo, in place (master plan §2). Strategy IDs from v1 are KEPT where the
> strategy survives (X0–X3, X5, X7–X9, X13); v1's base-side strategies X4, X6, X10, X11, X12 are marked
> `superseded`/`absorbed` with their successor. New: X14 (the clock extraction), X15 (the seam in the
> host), X16 (`FlowSet` + rows), X17 (ingest module). Boundaries: [`STAGE_CONTRACT.md`](STAGE_CONTRACT.md).

---

## X0 — Cross-cutting disciplines (every stage)

- **Byte-identical-off (D-13 / MR-4).** Every new path is opt-in until its gate passes (`--fg-core`,
  then `--legacy-warp`); a moved body is not a new path — its gate is the byte-/spread-identity of the
  product's telemetry on a replayed source.
- **Verbatim bodies + reference-only binding (CR1).** A body lifted out of a lambda or a loop scope binds
  through a struct whose data members are ALL `T&` (or moves into a value type whose members ARE the
  former locals, R1); scalars read-only per call are parameters. The moved region is checked with
  `git diff --color-moved=zebra` and the byte-identical percentage is COMPUTED into the gate report (E1: 98.37 %).
- **Evaluation-order preservation (XR14).** Floating-point expressions move as written — no algebraic
  rewrite, no `fma`, no reordering; `/fp:precise` stays (the CMake flags unchanged, *to confirm*).
- **DI-3.** Two runs per side; spread reported; `r` for per-cell comparisons.
- **Signature** `Made with my soul - Swately <3` on every touched code file; docs in English.
- **Gate binding.** A gate binds to the exact tree it tested; an edit after it voids it.
- **Stage modules are headers + single-caller functions** called from the thread bodies (`ARCHITECTURE.md`'s
  single-TU-per-hot-thread rationale kept; LTO covers the rest; PR1 measured at every gate).

---

## R0 — CONTROL: the registry (milestone M-R0)

### X1 — `src/control/` + the generator + `--layer-dump` / `--layer-model-json` · *mitigates XR2, XR6, DR2, MR-4, XR13*

Unchanged from v1. New files exactly `aap/CANDIDATE_C.md` §1.1's host-side list (`layer_table.def`,
`layer_abi.hpp`, `layer_table.hpp`, `layer_config.hpp`, `layer_registry.cpp`, `tools/gen_layer_glsl.cmake`);
the shader-side files are R3's. Modified: `CMakeLists.txt` (the generator `add_custom_command`; **XR13:**
its outputs declared as `OUTPUT`s, and R3's `fg_core.spv` command lists them + a `CONFIGURE_DEPENDS` glob
of `shaders/layers/*.glsl` in `DEPENDS` AND uses `glslc -MD -MF` with `DEPFILE` — the `pfg_spv()` precedent
at `CMakeLists.txt:30–41` tracks only the top-level `.comp`, the hole `aap/AT2_SCORECARDS.md:623` names);
`src/control/cli.hpp` (`Config` gains `LayerConfig layers;` — the OLD layer fields KEPT until R3); `src/control/cli.cpp`
(`if (parse_layer_flag(argv[i], c.layers)) continue;` before the chain; old cases KEPT so both parse);
`ui/src-tauri/src/lib.rs` (`layer_model()` via `resolve_exe()` at `lib.rs:164`, *to confirm*); `ui/src/main.js`
(the layer section replaced by the binary's model; the ~150 host entries stay).

**The parity check (XR2):**

```cpp
// src/control/layer_registry.cpp — startup; a mismatch is a loud abort.
bool layer_config_parity(const Config& c) {
    bool ok = true;
#define PFG_PARITY(field, layer_field) \
    if (!(c.field == c.layers.layer_field)) { std::printf("[layertab] PARITY FAIL %s: %g vs %g\n", #field, (double)c.field, (double)c.layers.layer_field); ok = false; }
    PFG_PARITY(mv_sim,         mv_guided_sim)
    PFG_PARITY(inertia_thresh, inertia_thresh)
    PFG_PARITY(bg_reclaim,     bg_reclaim_strength)
    /* … generated from the .def by an X-macro so the list cannot drift … */
#undef PFG_PARITY
    return ok;
}
// main.cpp, after resolve_config(): if (!layer_config_parity(cfg)) return 3;
```

The contract hash (FNV-1a 64 over `(id, rank, stage, kind, overrides, arm, param bits)` in rank order) is
printed by `--layer-dump` and stamped into the `--csv` header and the `--qdump` manifest (`contract=0x…`).
**Validation (G-R0):** master plan §3-R0.

### X2 — the flag surface's two instruments · *mitigates XR6, DR2*

Unchanged: (a) `tools/check_flag_roundtrip.py` (stdlib) + a new `--dump-config` diagnostic (every `Config`
field as `name=value`) before/after over EVERY `--flag` token of `cli.cpp`; (b) the reviewed `--help` diff
attached to the gate report with every changed line attributed to the generated block.

### X3 — the column-closure experiment (paper) · *mitigates XR3*

Unchanged: `docs/planning/aap/COLUMN_CLOSURE_EXPERIMENT.md`, ten more `wap_warp.comp` gates mapped, new
columns counted; `≤ 1` proceed, `2–3` design + review, `≥ 4` RE-SELECT toward Candidate A. Runs BEFORE any
shader file exists. (The one plan-class document this arc still writes: it is the record of a measurement.)

---

## R1 — Stage 4 CLOCK extracted (milestone M-R1)

### X14 — `PhaseClock`, a CPU value type, moved verbatim from the P loop · *mitigates CR1, XR14, XR9* (absorbs v1 X11)

**Destination:** `src/clock/phase_clock.hpp` / `.cpp`:

```cpp
// src/clock/phase_clock.hpp — Stage 4 (STAGE_CONTRACT §2, stage 4). NO Vulkan, NO threads: a value type.
struct PhaseClockConsts { double freq_alpha, phase_gain, reseat_err; };   // = kScFreqAlpha, kScPhaseGain, kScReseatErr, copied by NAME
struct PhaseInputs  { uint64_t cur_c; double t_cap_ms; double tick_period_ms; double sc_delta_ms; double lead_frames; bool vblend_exact; double pred_lead; uint64_t f_seq; /* … every former loop-local the steps read, one field each, listed in the review table */ };
struct Phase        { int gen; uint64_t pair_c; double t_use; double t_display; double disp_src; enum Decision { Warp, Dup, Drop, Decimated } decision; };
struct PhaseClock {
    // the former P-locals, verbatim names (present.cpp:1574–1580):
    double content_clock = 0.0, T_robust_ms = 0.0; bool sc_init = false; uint64_t sc_last_c = 0, tr_last_cseq = 0;
    PhaseClockConsts k;
    void   tick(const PhaseInputs& in);              // = the FREQUENCY loop + NCO advance (present.cpp:1964–1976), body verbatim
    void   lock(const PhaseInputs& in, double expected);  // = the PHASE loop (2050–2061), body verbatim
    Phase  select(const PhaseInputs& in) const;      // = step 3 (1978–2021) + step 4 (2188–2202) + t_use base + backwards clamp (2370–2387, 2497)
    static bool monotone(const Phase& prev, const Phase& next);   // = step 6 (2904)
};
```

**Technique:** each method body is the source range MOVED (not retyped); every former loop-local it read
becomes a `PhaseInputs` field (read-only per call) and every former loop-local it WROTE becomes a member —
the review artifact is a table `source local → field/member` covering both directions (CR1's field-by-field
review). The P loop calls `clk.tick(in)` / `clk.lock(in, expected)` / `Phase ph = clk.select(in)` at
EXACTLY the source lines the bodies came from (no motion of the call in the tick; XR5's discipline). The
phase LAYERS (`--phase-norm` `:2394–2427`, the ease cubic `:2431–2468`, the dispersion latch `:2315`, the
`realized_mult` governor `:2397–2495`, the laser feedback `:2583/2664`) stay inline where they are, reading
and writing `ph.t_use` behind their flags — untouched in R1.

**The two tests (`tests/clock/test_phase_clock.cpp`, CPU-only CMake target):**
1. **Replay parity (the extraction oracle, XR14):** a NEW diagnostic `--arrival-log FILE` (P appends one
   line per tick: `tick, cur_c, t_cap_ms, sc_delta_ms, f_seq, t_use, disp_src` — INSTRUMENT plane) recorded
   on a 60 s `ball_zoo` run with the PRE-R1 binary; the test feeds the inputs to `PhaseClock` and asserts
   `t_use` / `disp_src` **bit-identical** (`memcmp` of the doubles) to the recorded columns, every tick.
2. **Synthetic arrivals:** 60 fps ± 2 ms jitter → lock within N ticks; a 60→30 fps step → exactly one
   re-seat; a dropped frame → `monotone()` never false. N and the phase-error bound are SET from the first
   run and recorded in the test as the baseline (not asserted a priori — master plan §4.3).

**XR9:** the test proves extraction fidelity and clock behaviour on synthetic input; it is NOT M1. The stage
record says so.

---

## R2 — the seam adopted; stage 5's barriers declared; grafts (milestone M-R2)

### X15 — `src/seam/seam_graph.hpp` in the host; P's warp/blit path as a graph · *mitigates MR-3, KC3, XR10* (absorbs v1 X4)

- **Adopt:** copy `apps/minimal_fg/include/minimal_fg/seam_graph.hpp` → `src/seam/seam_graph.hpp`
  (namespace `minimal_fg` → `pfg::seam`, the signature line added, nothing else) and
  `apps/minimal_fg/test/test_seam_graph.cpp` → `tests/seam/test_seam_graph.cpp` with a standalone CMake
  target `pfg_seam_test` (GPU-free; runs in `build.bat` as a post-build step). **XR10:** the repo copy is
  canonical from this commit; the container copy gets a one-line note in `apps/minimal_fg/CMakeLists.txt`'s
  header comment pointing at the repo (the container file is otherwise frozen; the operator relocates, the
  session does not).
- **synchronization2:** `seam_graph.hpp` requires `VK_VERSION_1_3` + `vkCmdPipelineBarrier2`; verify the
  host's device creation enables `synchronization2` (`src/core/core_init.cpp`, the `VkPhysicalDeviceVulkan13Features`
  chain, *to confirm*); if absent, enable it — a feature bit, no behaviour change for existing barriers.
- **P's graph (built once at init, executed per tick):**

```cpp
// present_init.cpp (WapInit) — declared once; STAGE_CONTRACT stage 5 + INSTRUMENT overlay + stage 6 blit
auto rPrev = sg.declare_image("pair_A", true), rCur = sg.declare_image("pair_B", true);
auto rMV = sg.declare_image("flow_mv", true), rSAD = sg.declare_image("flow_sad", true); /* … one per FlowSet image bound to the warp … */
auto rOut = sg.declare_image("gen_out");                    // wapOutA (GENERAL, storage)
auto rBridge = sg.declare_image("bridge", true);            // the DXGI-shared present image
sg.add_pass("warp",    { rd(rPrev), rd(rCur), rd(rMV), rd(rSAD), /*…*/ }, { wr_storage(rOut) }, record_warp);
sg.add_pass("overlay", { rw_storage(rOut) }, { rw_storage(rOut) }, record_overlay);   // culled when cfg.fps_overlay == false (G4 optional_write not needed: it is a full RMW)
sg.add_pass("blit",    { rd_transfer(rOut) }, { wr_transfer(rBridge) }, record_blit);
sg.mark_output(rBridge);
compiled = sg.compile();   // static graph; --sg-dump prints it
```

  The three hand-written `img_barrier` calls at `present.cpp:1215–1218` and the overlay's at `:1188` are
  DELETED (counted); `execute(cmd, compiled)` records the derived barriers + the three record callbacks in
  order. The `wapOutA` TRANSFER_SRC→GENERAL restore is the WAR the engine derives on the next tick's
  `warp` write (the header's WAR rule) — the "no cross-frame transition" limitation is handled by the
  per-tick re-execution of the same compiled graph (same as the base, `main.cpp:810`).
- **Grafts (in the header, each with a counted golden check):** G3 — the `has_producer && !read_here` WAW
  branch (`seam_graph.hpp:411–417`) pushes a `Warning` into `Compiled::warnings`; `dump_warnings()` prints;
  `dump()` untouched; `.dominates_ok = "reason"` on a pass silences; `PFG_SEAM_DOMINANCE_IS_ERROR` promotes.
  G4 — `Access::optional`; an unread optional write culls its pass or exposes `Compiled::dead_optional_writes`
  for the caller's spec constant. G5 — `CompiledPass::vk_barriers` filled at `compile()`; `execute()` patches
  `image` only (XR11: patched on EVERY call from `res_[…].image`; `graph_id` asserted).
- The first-write-from-UNDEFINED rule: a declared write whose resource is internal and has no prior access
  emits a layout-only barrier `srcStage = NONE, srcAccess = 0` (the semantics of today's
  `img_barrier(…, UNDEFINED, …, 0, …)`); golden check: "+1 pass == +1 barrier" still holds for non-first writes.

**Validation (G-R2):** master plan §3-R2 (the `img_barrier` count drop quoted; sync-validation; 0 allocations).

---

## R3 — Stage 5: `fg_core.comp` (milestone M-R3)

### X7 — the shader side · *mitigates XR1, XR7, the M4 veto, PR1* — unchanged from v1

`shaders/fg_core_math.glsl` (the core = `wap_warp.comp:520–529` + `:497–498` + `:637` + `:679`, moved, the
`s_d[64]` reduction inside), `shaders/layers/<name>.glsl` (bodies from C §4.1–4.8's ranges, verbatim),
`shaders/fg_core.comp` (the same descriptor set as `wap_warp.comp` — the placeholder trick of
`src/core/app_init.hpp:366–381` kept; the generated includes; ONE `imageStore` — G1; G2 enforced by the
generator's `c_in` check; the rank-25 `:snapshot mv_raw_fwd` pseudo-row where `:344` sits; bg-reclaim
bug-for-bug with `dominates_ok` printed — XR7). **XR1:** the first M4 run writes the PACKED value
(`(1.0f + cfg.mv_sim) - 1.0f`), the second the clean one; both recorded.

### X8 — the host side · *mitigates XR5, PR1, MR-4* — unchanged from v1, now inside the R2 graph

The 58-float push (`present.cpp:1119–1150`) + gate derivation (`:960–1118`) → `CorePush` (20 B) +
`layer_arm_mask(cfg, ArmInputs{…})` where `ArmInputs` is built ONLY from `FlowSet[gen]` validity fields
(`f_pair_gme_valid_a[gen]`, `f_pair_bwd_valid_a[gen]`, …) at the SAME tick position (XR5); the `LayerParams`
UBO written at init / config change. `--fg-core` selects `fg_core.comp` inside R2's `warp` pass record
callback; default stays `wap_warp.comp` until G-R3 passes; then `--legacy-warp`. The `--qdump` manifest and
`--csv` header gain `contract=0x…` + `arm=0x…`.

### X9 — M4 by the T6 reference warp · *the A0 veto* — unchanged from v1

Precondition MOTION_TRUTH T1 + T6 on the host. ≥ 200 triples, `--no-async-present`, both paths, same
seed/flags; per-triple verdict `identical` / `N pixels explained` / `FAIL`; one unexplained pixel = FAIL.

---

## R4 — Stage 6 PRESENT extracted; the decision declared (milestone M-R4)

### X12 — `present_stage`, in the host, the base's loop shape · *mitigates MR-1, MR-2, MR-7, CR1* (v1 X12 re-aimed)

- **Destination:** `src/present/present_stage.hpp/.cpp`: `struct PresentStage` owning the `PresentSurface`
  (`present.cpp:377–460`), the bridge images/keyed mutex/NT handles (the `bridge_*` FgContext fields), the
  async-present slots (`:860–930`, `:1361`), and the methods `present(const Phase&, const GenFrame&) → FlipStats`
  (= `bridge_present` `:466–507` + step 7 `:2593–2660` / `:2921–2949`), `present_real(const Phase&, const RealFrame&)`
  (= `bridge_present_src` `:654–693` + `rfp_present` `:1416–1458`), `stats_second()` (= step 9 `:2954`).
  Reference-only binding for everything that stays in `main()` (CR1); bodies verbatim.
- **The decision field:** stage 4 writes `Phase.decision ∈ {Warp, Dup, Drop, Decimated}` at the three
  sites that today decide inline — `fdrop` (exact-dup, `:919` `do_warp=false`), the async-drop (`:1186`,
  a warp in flight), the decimation gate (`:2062`) — and `PresentStage` READS it. The late-target guard-band
  drop (MINIMAL_FG S8): `if (target_qpc + guard > next_real_qpc) decision = Drop` — added ONLY if R4's
  first-hand read confirms no equivalent exists (`unverified`, master plan §4.3); if added, behind a flag
  (`--drop-late`, default OFF → byte-identical), measured under `gpu_load`.
- **MR-1:** the slot machinery moves as is (provisioned at init; `vk_live`-wrapped polls; no hot-path
  alloc); **MR-2:** no queue split (the host's single submitter per queue stays); **MR-7:** own-window only;
  `last_flip_qpc()` → `FlipStats` returned to the loop and forwarded to `PhaseClock` (the feedback edge of
  STAGE_CONTRACT §0).
- **The loop after R1 + R4 (the base's shape):**

```cpp
for (;;) {                                   // present.cpp OUTPUT-CLOCK loop, after R1 + R4
    tick_boundary();                         // step 1 (unchanged)
    clk.tick(in); clk.lock(in, expected);    // stage 4
    Phase ph = clk.select(in);               // stage 4 (+ the inline phase layers behind their flags)
    if (ph.decision == Phase::Warp) gen = generate(ph, pair, flow[ph.gen]);   // stage 5: sg.execute(...)
    FlipStats fs = pres.present(ph, gen);    // stage 6
    in.flip = fs;                            // feedback
    stats(ph, gen, fs);                      // INSTRUMENT (csv / qdump+ / phaselog)
}
```

---

## R5 — Stage 3: `FlowSet` + `FlowRing`; holons as rows; the median moved; `wap_upload` conditional (milestone M-R5)

### X16 — `src/flow/flow_set.hpp` + `holons.cpp` + the two moves · *mitigates CR1, PR1, MR-4, MR-6* (absorbs v1 X6/X10)

- **`FlowSet`:** one struct per `gen` replacing the parallel arrays — the host pointers (`hostMV[gen]`,
  `hostSAD`, `hostMVB`, `hostDIS`, `hostDISB`, `hostPER`, `hostC2`, `hostGmeM`, `hostGmeMB`, `hostI[gen][…]`)
  become members; the `f_pair_*_a[gen]` scalars become members (`cur_c, slot, t_cap_ms, span, n, gme[6],
  gme_valid, gme_bwd[6], bwd_valid, mfwd, mbwd, disp`). `FlowRing { FlowSet set[NS]; std::atomic<uint64_t>& f_seq;
  std::atomic<uint64_t>& p_presenting; }` — the publish (`f_seq.fetch_add`, `flow.cpp:1777`/`:2191`) and the
  lap guard (`:1858–1878`) unchanged in ORDER (fields written, then the seq_cst bump).
- **Holons → rows:** `object_repair` / `mem_advect` / `mem_merge` / `mem_refresh` / `consume_wap` extracted
  to `src/flow/holons.cpp` per `RESTRUCTURE_PLAN.md` §3-E3 (leaves first, `consume_wap` last; the three
  helper lambdas `flow_submit_nowait` `:632`, `flow_submit_q2_chain` `:650`, `flow_downsample` `:706` passed
  as callables or extracted — decided at entry), each registered as a FLOW-stage `kind = P` row
  (`PFG_LAYER(OBJECT_REPAIR, FLOW, …)`) whose enable is the existing flag (`use_objects`, `use_memory`), off
  under the rig default (re-confirmed per row at G-R5 by `--layer-dump`).
- **The median moves:** the in-bridge 3×3 consensus (`present.cpp:779`, `medPipe`, `use_mv_median`) is
  recorded in F's graph as a FLOW row (`MV_MEDIAN`, `kind = P`, reads `flow_mv`, writes `flow_mv_med`); stage
  5 binds `flow_mv_med` when the row is on. Default OFF → byte-identical (verify `use_mv_median`'s default,
  *to confirm*).
- **`wap_upload` conditional:** `if (flow_device != gen_device) wap_upload(…)` — on the rig (one device)
  stage 5 samples F's images directly; the multi-GPU path keeps the copy (decision 3). The single-device
  path needs F's images to be SAMPLED by P's queue: same device, same queue family (*to confirm* the
  A/G device split under `single_gpu`) → no ownership transfer; otherwise the SG acquire/release pair.
- **The couplings declared (STAGE_CONTRACT §3):** `GovernorFloor` (P → F), `SourceRate` (P → F) become
  fields of a small `ControlSignals` struct with one writer each; values unchanged.

**Validation (G-R5):** master plan §3-R5.

---

## R6 — Stages 1–2 named; the ingest module (milestone M-R6)

### X17 — `src/ingest/` + `RawFrame` / `RealFrame` · *mitigates MR-5, MR-7, CR2-class*

The convert + publish tail of `run_capture` (`capture.cpp:~600–792`) and `run_convert_worker` (`:805–971`)
move to `src/ingest/ingest.cpp` as `ingest_frame(const RawFrame&, RealFrame& out)` (verbatim; the convert
dispatch `:706–714`, the iGPU path `:737–750`, the publish `:792` with `t_pub_ms` stamped before the
`fetch_add`). `RealSlot` → `RealFrame::stamps`. `capture.cpp` keeps the WGC/DDA acquisition branches
(stage 1) and calls `ingest_frame`. Directory renames (`warp_blend/` → `generate/`, `cli/` + `layers/` →
`control/`) are `git mv` (history preserved). **Gate:** the startup-log diff = 0 lines (the E1 instrument);
the fps lines within spread.

---

## R7 — Closing

### X13 — MR-4 + MR-8 as the release gate · *mitigates MR-4, MR-8* — unchanged from v1, host-only

`--legacy-warp` leaves the DEFAULT build only when MOTION_TRUTH's M1 tables exist for both paths; the
pre-R3 binary is tagged in git as the A/B reference; the legacy shader stays in the tree.

---

## Superseded / absorbed (v1 → v2)

| v1 | Disposition |
|---|---|
| X4 retire `img_barrier()` on the base | absorbed by **X15** (the host's hand barriers declared) |
| X6 the C-thread contract on the base | superseded — the host IS the contract; naming in **X17** |
| X10 the core inside the base's `mc_interp` | superseded — no base |
| X11 the clock PORTED into the base | absorbed by **X14** (extracted in place, tested) |
| X12 pacing on the base | re-aimed as **X12** (extracted in place) |

## §Coverage map (strategy → stage → risks)

| Strategy | Stage | Mitigates |
|---|---|---|
| X0 disciplines | all | MR-4, CR1, XR14, DI-3 |
| X1 registry + parity + `DEPENDS` | R0 | **XR2, XR6, DR2, MR-4, XR13** |
| X2 two instruments for the flags | R0 | **XR6, DR2** |
| X3 column-closure experiment | R0 exit | **XR3** |
| X14 `PhaseClock` + tests | R1 | **CR1, XR14, XR9** |
| X15 seam adopted + P's graph + G3/G4/G5 | R2 | **MR-3, KC3, XR10, XR11** |
| X7 shader side | R3 | **XR1, XR7, M4, PR1** |
| X8 host side | R3 | **XR5, PR1, MR-4** |
| X9 M4 by T6 | R3 | **the A0 veto** |
| X12 `PresentStage` + `Phase.decision` | R4 | **MR-1, MR-2, MR-7, CR1** |
| X16 `FlowSet` + rows + moves | R5 | **CR1, PR1, MR-4, MR-6** |
| X17 ingest module | R6 | **MR-5, MR-7** |
| X13 closing | R7 | **MR-4, MR-8** |

> Coverage check: MR-1 (X12) · MR-2 (X12) · MR-3 (X15) · MR-4 (X0, X1, X8, X16, X13) · MR-5 (X17) · MR-6 (X16)
> · MR-7 (X12, X17) · MR-8 (X13) · CR1 (X0, X14, X12, X16) · PR1 (X7, X8, X16) · DR2 (X1, X2) · XR1 (X7) ·
> XR2 (X1) · XR3 (X3) · XR4 (accepted, register) · XR5 (X8) · XR6 (X1, X2) · XR7 (X7) · XR8 (superseded) ·
> XR9 (X14) · XR10 (X15) · XR11 (X15) · XR12 (superseded) · XR13 (X1) · XR14 (X0, X14). Every register ID is
> cited by at least one strategy or recorded as accepted/superseded.

*Made with my soul - Swately <3*
