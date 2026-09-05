# R3 — gate record (G-R3, milestone M-R3) · 2026-09-05

> Stage R3 of [`../CONVERGENCE_MASTER_PLAN.md`](../CONVERGENCE_MASTER_PLAN.md) §3: `shaders/fg_core.comp` + the fused
> rows replace `shaders/wap_warp.comp` for the default set, OPT-IN behind `--fg-core` until M4 passes. Every number
> below is quoted from command output captured in the session; the raw logs live in the session scratchpad
> (ephemeral). Tree: `d8ec5ae` + the R3 working tree (committed at the end of this record's day).
> **Verdict: G-R3 PASSED (§8)** — byte-identical to the legacy kernel on 24,227 compared ticks once FMA contraction is
> forbidden in both modules; with the driver's contraction allowed, ~5 × 10⁻⁹ one-level pixels (and their
> threshold flips on knife-edge content), attributed by experiment. Product unchanged; `--fg-core` opt-in.

## 0 · What R3 was asked to be (the contract, re-read before building)

`CONVERGENCE_MASTER_PLAN.md` §R3: the eight rows of `aap/CANDIDATE_C.md` §4.1–4.8 as bodies + the generated
chains + `fg_core.comp`; the 58-float push → `CorePush` (20 B) + `layer_arm_mask(ArmInputs)`; bg-reclaim
bug-for-bug (XR7); `--fg-core` opt-in; **MUST NOT change any default output byte (M4 veto)**; the THREE
constraints from the R0 exit gate (`WEIGHT` stage with the core split; `CH_BLEND` + `select` as a row; the
declared-`needs` rule for cross-row parameter reads); the four risk obligations XR1 (packed value first), XR5
(arm mask at one point), XR7, XR13 (the `.spv` depfile). **Gate G-R3 (M4):** the T6 CPU reference warp on ≥ 200
`--qdump+` triples, byte-identical or every differing pixel explained; packed value first, clean second; M3 two
runs per side; M2b `unmeasured` until run.

**The three operator items left open on 2026-09-04, investigated as delegated ("investígalos cuando los
necesites"):** none is on R3's path. The consensus-pass default (`mv_median.comp`, armed via `mv_guided`) runs
in **stage 3**, upstream of the kernel R3 replaces; R5 makes it a row, and its default stays the operator's —
deferred to R5. A0's M1 (comparative ≤ 0.10 px) gates **R7**, not R3 (G-R3 is M4) — deferred to R7 with
`M1_LOWPHASE_FINDING.md` on record. R3 reproduces the default bug-for-bug, so it needed no word on the
default.

## 1 · What was built

| Piece | File(s) | What it is |
|---|---|---|
| **the core** | `shaders/fg_core_math.glsl` | `LayerCtx`; `fg_gate1` (:497-498); `fg_sample` (:518-522, verbatim expression order); `fg_blend` (:679); `fg_crossfade` (:694-695, the `CH_BLEND` channel). The core split of COLUMN_CLOSURE §2.1: sample half → WEIGHT → blend half. No layer parameter, no UBO. |
| **the row bodies** | `shaders/layers/{fetch_mv, mv_edge_snap, mv_guided, inertia, bg_reclaim, phase_anchor, ambig, vblend, single_track_wa, select, stasis, single_track}.glsl` | twelve fused bodies, each citing its source lines; `fg_helpers.glsl` = `guided_mv` / `edge_snap_mv` / `gme_model_mv` extracted VERBATIM by script from `wap_warp.comp` (39+62+11 lines, comments included) |
| **the kernel** | `shaders/fg_core.comp` | bindings 0..13 = the same set layout as `wap_warp.comp` (the host writes the same fourteen views) + 15 = the generated `LayerParams` UBO; `#include`s the generated chains; ONE `imageStore`, ONE source (G1) |
| **the generator, extended** | `tools/layer_gen.cpp` | `WEIGHT` stage (`chain_weight.glsl`, identity `wa = 1 − t`); the body checks (missing body · entry point · `lp.` direct read · **cross-row alias without `needs`** · COMPOSE ignoring `c_in` without `overrides` · signature) → exit 3, nothing generated |
| **the schema, extended** | `src/layers/layer_abi.hpp`, `layer_table.hpp`, `layer_table.def` | `Stage::WEIGHT`; `CH_BLEND`; COMPOSE may write `CH_BLEND`; two new rows — `single_track_wa` (WEIGHT 190, requires single_track) and `select` (COMPOSE 260); `ArmInputs` struct; `FgPush` |
| **the host** | `src/warp_blend/warp_blend.{hpp,cpp}` (`fgcore_create`, `abdiff_create`), `warp_blend_init.cpp`, `present.cpp`, `core/{app_init,fg_context,main}`, `cli/{cli.hpp,cli.cpp}`, `layers/{layer_config.hpp,layer_registry.cpp}` | the pipeline with a `VkSpecializationInfo` (one `VkBool32` per fused row, `constant_id` = layer id), the config-time UBO (`layer_params_fill`: XR1 packed sim by default, the `mv_edge_snap` "0 = use --mv-sim" cascade resolved host-side), `layer_arm_mask(ArmInputs)` derived ONCE per tick beside the legacy gates (XR5), the dispatch routing (`--fg-core` = the product; `--fg-core-ab` = both kernels), `--layer-dump` prints `fg_sample` / WEIGHT / `fg_blend` / the R3 flags, the qdump manifest gains `core= contract= ab=` |
| **the M4 instrument** | `shaders/fg_ab_diff.comp`, `--fg-core-ab` | both kernels every tick from the SAME descriptor inputs; a compute pass counts the differing 8-bit pixels (running totals in VRAM, copied to the host each tick, printed ~1 Hz and at exit) and keeps the first 256 differing pixels as EVIDENCE `{x, y, legacy rgba, fg_core rgba, tick}` |
| **the attribution instrument** | `tools/spv_nocontract.py`, CMake `PFG_NOCONTRACT` (EXPERIMENT, default OFF) | decorates every float result of every `.spv` `NoContraction` INSIDE the glslc rules (so both kernels get it symmetrically) |
| **build** | `CMakeLists.txt` | `fg_core.spv` compiled with `-I` + a **DEPFILE** (XR13): a body or `.def` edit rebuilds the kernel; the generator takes the shaders dir and checks the bodies; `pfg_shaders_r3` |

**Declared deviations from the letter of the plan (each with its reason):**
1. **The push is 44 B, not 20.** `FgPush = CorePush (20) + GenScalars (24)` — the gme affine model is FlowSet[gen]
   data (like `t` and `arm_mask`, which the 20-B block already carries), not a layer parameter, so it cannot live
   in the config-time UBO; push constants are race-free under the async present, a host-written per-gen UBO would
   need a ring. `fg_core()`'s own contract stays six parameters (`res_ceil, improv, agree, t, mv-source, output`).
2. **Three of `single_track`'s four `shadows` are not reproduced.** `SH_WA_EFF_ZERO` became the WEIGHT row
   (§2.1's bonus). `SH_BLEND_BASE` (:692 `blend_result = cur`), `SH_FORCE_WARP` (:1089 `result = warp_result`)
   and `SH_COMMIT_INERT` (:810/:1267) write accumulators that the :1321 override DISCARDS — read in the shader
   text 2026-09-05: under `single_track ON` the stored pixel is born at :1321-1329 from `B_samp`, `cur0`,
   `d_pixel` and the stasis boolean, all fixed by line 522. No output byte depends on the three; they stay
   declared (`shadows=0xE`) and printed.
3. **The byte-identity envelope is the shipping default set with `single_track ON`.** Under
   `--no-single-track` the `select` row reproduces the HARD path (:1177-1184); the legacy default also carries
   `soft_gate` / `commit_default` / `multicand` variants of the selection, which are not rows in R3. The host
   prints an envelope warning when a non-row feature is armed (matte, blend-solo, camera-twarp, ts-smooth,
   bg-snap, band-xfade, multicand, no-single-track).
4. **`warp_light` cannot be reproduced.** The load governor (default ON) sheds `vblend` / `band_xfade` /
   `multicand` / `bg_snap` per tick in the legacy push under 4090 saturation; a specialization constant cannot
   be shed. `ArmInputs` are FlowSet-derived by contract ("never from P-local state"), so this is not an arm bit.
   The instrument skips light ticks (`light_skips` counted; 0 on every run below). Under `--fg-core` as the
   product, vblend is NOT shed under saturation — a product-behaviour difference under load, for the operator.
5. **`extrap` (ASW) and `cam_lead` (camera-twarp) have no row.** `cam_lead` is the `UV` stage the closure
   experiment named, default OFF. The ASW extrapolation TAP (:1246-1253) is dead under the shipping default —
   it writes `result` before the :1321 override discards it; ASW's visible behaviour survives through `t > 1`
   in the B-track sample itself, which the port computes identically.

## 2 · Cold checks (no GPU)

| Check | Result (quoted) |
|---|---|
| build ×2 (`build-release.bat`) | `layer_gen: 14 rows, 12 params, 12 bodies checked`; `[6/21] glslc fg_core (+depfile)`; `[20/21] Linking CXX executable phyriad_fg.exe`; `=== build OK`. No new warning in a touched file (the only new-looking one, `layer_registry.cpp(25): C4127`, is R0's `if (kParamCount == 0)`, present at HEAD). |
| `--layer-dump` determinism | `RUN1==RUN2 byte-identical`; `contract=0xD4D55F5B671294EF  (11 rows enabled of 12; 12 params)` — R0's was `0xB8C7BD1BC5BACB3C (9 of 10)`: the hash moved because the chain did (two rows, a stage value). Prints `CORE fg_sample … push=CorePush 20 B + gen-scalars 24 B` before `WEIGHT 190 single_track_wa … requires={single_track}` and `CORE fg_blend` before `COMPOSE 260 select`; `single_track … shadows=0xE`. |
| the generated chains | `chain_mvcond.glsl`: rank 0 fetch → 5 `L_MV_EDGE_SNAP` → 10 `L_MV_GUIDED && !L_MV_EDGE_SNAP` → 20 `L_INERTIA && (L_MV_EDGE_SNAP \|\| L_MV_GUIDED)` → 25 snapshot → 30 `L_BG_RECLAIM && arm bit 5` → 40 `L_PHASE_ANCHOR && arm bit 6` → 45 `ctx.sad = …; ctx.stasis_block = (L_STASIS && (ctx.sad.y <= STASIS_thresh))` → 50 `L_AMBIG && arm bit 8`; `chain_sample`: 110 vblend; `chain_weight`: `wa = 1.0 - ctx.t` then 190; `chain_compose`: `c_out = core.color` → 260 select → 280 stasis OVERRIDE → 290 single_track OVERRIDE. |
| **generator negatives (exit 3 expected)** | (a) a body reading `MV_GUIDED_sim` without `needs`: `layer_gen: row vblend: reads MV_GUIDED_sim (row mv_guided's parameter) without declaring row mv_guided in its needs mask` → **exit 3**. (b) a COMPOSE body without `c_in`, `overrides=false`: **first attempt exit 0 — the check could not fail** (the function signature names `c_in`); fixed to scan the function body after its opening brace with `//` comments stripped → `layer_gen: row select: a COMPOSE body that never references c_in must declare overrides = true` → **exit 3**. (c) a direct `lp.` read: `layer_gen: row stasis: body reads the UBO directly` → **exit 3**. Bodies restored (`cmp` identical) → exit 0. |
| **XR13 depfile** | `touch shaders/layers/stasis.glsl` → `cmake --build … --target pfg_shaders_r3`: `[2/4] glslc fg_core (+depfile)`, `[3/4] spv->hdr: kFgCoreSpv`. A body edit rebuilds the kernel. |
| the audit tool on an OLD record | `tools/check_qdump_plus.py qd_defb1_a`: every triple line now carries `core=wap_warp(pre-R3) contract=- ab=-`; parsing unchanged. |
| the fidelity review (delegated, Sonnet, 14 agents: 12 rows + the kernel structure + a completeness critic; 931,078 tokens, 144 tool uses, 392 s) | **0 value-affecting deviations under the default**; 105 expressions verified identical (1+8+14+6+15+6+6+6+1+6+2+7 per row + 27 structure). Two notes re-verified first-hand: `select` drops the `member_strength > 0.5` disjunct — it is 0 without a matte row (correct); the structure reader called `ctx.blend = crossfade` a discrepancy "if single_track is OFF" — misread: under OFF the legacy `blend_result` IS the crossfade (:694-695), under ON it is discarded. The critic's four: `warp_light` (deviation 4 above); `cam_lead`/`extrap` (deviation 5); `mv_edge_snap`'s variant is downgraded G1→G2 per tick in the legacy when gme is invalid, static in the port — a default-OFF row, recorded as its known static-vs-dynamic gap; **`layer_arm_mask` had no `ArmId::COMMIT` branch — a silent-disarm trap for a future row: FIXED**, the function now takes the contract's `ArmInputs` and `switch`es over every `ArmId`. |

## 3 · The M4 instrument, first runs (product build, packed sim, the ball zoo 1920×1080, static lattice)

The strongest form of M4 available: not "the oracle agrees with both", but **the two kernels compared on the
same tick from the same descriptor inputs, every tick**.

| run | compared ticks | diff_px | max_delta | where |
|---|---|---|---|---|
| smoke 12 s | 2,063 | 22 | 1 | — |
| smoke 20 s | 2,372 | 33 | 1 | all in rows y ∈ [480, 597] = the moving ball's band; none on the static background; seven in ONE column (`px(292,540…546)`, tick 1098); deltas ±1 in one or two channels |
| smoke 20 s | 3,100 | 25 | 1 | — |

Quoted: `[fg-core-ab] TOTAL compared=2372 diff_px=33 max_delta=1 sum_delta=33  (NOT identical)`;
`[fg-core-ab]   px(292,540) legacy=786717FF fg_core=796717FF tick=1098` … `px(292,546) legacy=3E3621FF fg_core=3E3721FF tick=1098`.

Rate: 33 / (2,372 × 2,073,600) = **6.7 × 10⁻⁹ of the pixels**, every one exactly one 8-bit level. A logic
difference produces many pixels with large deltas; this is the signature of a 1-ulp difference in a sampling
coordinate at a bilinear boundary (a whole column flips together when the x-coordinate arithmetic lands on the
other side for every row of that column).

## 4 · The residual attributed by experiment (the M4 clause "explained by a documented floating-point-order change")

Hypothesis: the driver's shader compiler contracts `a*b+c` into FMA differently in two modules whose
expression trees are identical but whose surrounding code is not. Test: decorate EVERY float result of BOTH
modules `NoContraction` (SPIR-V decoration 42; `wap_warp.spv` 570 results, `fg_core.spv` 147) and compare
again.

| build | compared | diff_px | max_delta | verdict |
|---|---|---|---|---|
| product (contraction allowed) | 2,063 / 2,372 / 3,100 | 22 / 33 / 25 | 1 | the residual |
| **asymmetric — INVALID** (`NoContraction` on `wap_warp` only: Ninja's depfile log saw the externally rewritten `fg_core.spv` as stale and recompiled it from source) | 2,681 | 23 | **5** | one-sided contraction makes it WORSE — consistent with the hypothesis, but not the test |
| **symmetric** (`PFG_NOCONTRACT=ON`, applied inside both glslc rules) | **2,382** | **0** | **0** | `[fg-core-ab] TOTAL compared=2382 diff_px=0 max_delta=0 sum_delta=0  (BYTE-IDENTICAL on every compared tick)` |

**Verdict: the residual IS the driver compiler's FMA contraction, and nothing else.** With contraction forbidden
in both modules the two kernels are byte-identical on every compared tick. The product build was then restored
(`PFG_NOCONTRACT=OFF`) and both modules verified byte-identical to the pre-experiment ones
(`wap_warp.spv == pre-experiment module: True`, `fg_core.spv == pre-experiment module: True`; md5
`226d14b01a7f77b6730989aba7625da0` / `0f82fb5d7ececabd35c7856b0518a788`). The product's own numerics are
compiler-dependent in exactly this way already (a driver update can move the same pixels); the port did not
introduce a new class of difference, it exposed the existing one by putting two modules side by side.

### 4b · The same experiment at the harness's length — grid, noise AND pan (`r3_nc60.ps1`, 60 s each)

The 20-s run of §4 could not vouch for rare events (the delta-10 pair once per ~8,000 ticks; the pan tick with
256 column pixels). Repeated with `PFG_NOCONTRACT=ON` on both modules (`[38/53] glslc fg_core (+depfile)
+NoContraction`, `[42/53] glslc wap_warp +NoContraction`), 60 s per content:

| content | product build (§5) | **both modules `NoContraction`** |
|---|---|---|
| grid | 77 / 90 px, max 1 / 10 | `TOTAL compared=8139 diff_px=0 max_delta=0 sum_delta=0  (BYTE-IDENTICAL on every compared tick)` |
| noise | 50 / 62 px, max 1 | `TOTAL compared=7915 diff_px=0 max_delta=0 sum_delta=0  (BYTE-IDENTICAL on every compared tick)` |
| grid + pan 120 px/s | **2,554 px, max 13** | `TOTAL compared=8173 diff_px=0 max_delta=0 sum_delta=0  (BYTE-IDENTICAL on every compared tick)` |

**24,227 compared ticks, zero differing pixels, on the three contents — including the one that sat on the
inertia gate's knife-edge and showed multi-level, column-structured differences under contraction.** The whole
residual, the one-level rounding flips AND the decision flips, is the driver compiler's FMA contraction differing
between the two modules; nothing in the port's logic contributes. The product build was restored
(`PFG_NOCONTRACT=OFF`: `[36/53] glslc fg_core (+depfile)`, `[41/53] glslc wap_warp`) and `wap_warp.spv ==
pre-experiment module: True`.

What this does and does not say: it says the two kernels compute the SAME function of their inputs; it does not
say which of the two contracted results is "more exact" (both are 1-ulp neighbours of the same value, and both
are what a driver may legitimately produce). It also says the legacy product's own output is already
compiler-dependent at this level — the port made that visible, it did not create it.

## 5 · The M4 instrument, the full A/B (product build, 60 s runs, the ball zoo 1920×1080, `r3_ab.ps1`)

| side | content | run | compared | diff_px | max_delta | rate (px / compared px) |
|---|---|---|---|---|---|---|
| AB_packed | grid (static lattice) | 1 | 7,594 | 77 | 1 | 4.9 × 10⁻⁹ |
| AB_packed | grid | 2 | 8,134 | 90 | **10** (two pixels, `px(1035,536)` / `(1035,543)`, tick 5322: on the ball, at a lattice-line column) | 5.3 × 10⁻⁹ |
| AB_packed | noise (aperiodic texture) | 1 | 7,779 | 50 | 1 | 3.1 × 10⁻⁹ |
| AB_packed | noise | 2 | 7,143 | 62 | 1 | 4.2 × 10⁻⁹ |
| AB_packed | grid + **pan 120 px/s** (gme ≠ 0: bg_reclaim / ambig referee armed; \|mv\| = 2.0 px/frame sits ON the inertia gate's `length(mv) > 2.0`) | 1 | 7,326 | **2,554** | **13** | 1.7 × 10⁻⁷ — the evidence list saturated at tick 353: 256 pixels in ONE tick, all on lattice-line columns x ≡ 3 (mod 24) (x = 1035, 1275, 1515, 1755), every 24-px row from y = 23 to 703, deltas 2–4 — a whole MV-grid column choosing a different vector for one tick |
| **RED control** `--no-single-track` (the legacy takes its soft-gate path, fg_core the hard select — MUST differ, EMPIRICAL_TEST §3.3) | grid | 1 | 7,236 | **16,554,084** | **237** | 1.1 × 10⁻³ — **the gate seen red**: the instrument sees a difference when one exists (contract `0xC8267C30E9C32A8C`, the envelope warning printed) |
| AB_clean `--fg-core-clean-sim` (XR1's second row: the exact `--mv-sim`) | grid | 1 | 7,819 | 75 | 1 | 4.6 × 10⁻⁹ — indistinguishable from the packed rows |
| AB_clean | noise | 1 | 7,804 | 50 | 1 | 3.1 × 10⁻⁹ |

Quoted: `[fg-core-ab] TOTAL compared=7594 diff_px=77 max_delta=1 sum_delta=77  (NOT identical)`;
`[fg-core-ab] TOTAL compared=7326 diff_px=2554 max_delta=13 sum_delta=2854  (NOT identical)`;
`[fg-core-ab] TOTAL compared=7236 diff_px=16554084 max_delta=237 sum_delta=412755323  (NOT identical)`;
`[fg-core-ab] TOTAL compared=7819 diff_px=75 max_delta=1 sum_delta=75  (NOT identical)`. `light_skips=0` on every run
(the governor never shed on the ball zoo). `compared` < presents because dup/drop ticks record no warp.

**XR1, both rows on record:** packed 77 / 50 (grid / noise), clean 75 / 50 — the 1-ulp band shift produced no
observable difference on 15,600 compared ticks; the clean value is safe to ship, and the packed reproduction stays
the default until the operator says otherwise (it costs nothing and keeps the M4 argument exact).

**The pan run is the one that matters.** Grid and noise put the two kernels within 50–90 one-level pixels per
~7,500 ticks (the rounding-boundary class of §3). Pan multiplies that by 35 and produces multi-level,
column-structured differences in single ticks: the signature of a 1-ulp difference crossing a DECISION —
`length(mv) > 2.0` at \|mv\| = 2.0 exactly (`length` = `sqrt(dot)`, and `dot` is an FMA chain the two modules
may contract differently), the guided corner pick's `best_d > sim`, the ambiguity referee's `distance <
distance`. Each flip changes the sampled vector discretely and moves a lattice-line pixel by several levels.
Whether that is the WHOLE story is what §4b decided: it is.

## 6 · M3 — presents / 60 s, the product on each kernel, interleaved (A,B,A,B), 2 runs/side, the grid zoo

| side | run 1 | run 2 | mean | spread |
|---|---|---|---|---|
| legacy (`wap_warp.comp`) | 14,385 | 14,383 | 14,384.0 | 2 |
| `--fg-core` (the product on `fg_core.comp`) | 14,384 | 14,384 | 14,384.0 | 0 |

Δ = 0.0 presents/60 s — inside the run-to-run spread by construction (both sides pinned to the 240 Hz output clock:
14,384 presents = 239.7/s). Quoted: `[ra] bounded-run clean exit: total_presents=14385 …` / `…=14384 …` /
`…=14383 …` / `…=14384 …`. M3 says the fused kernel costs nothing the presenter can see at this load; it does
NOT measure GPU time per warp (not instrumented here — the `wap_warp_ema` column of the CSV would, on a
saturated run; not run).

## 7 · The oracle corpus (`r3_corpus.ps1`: `--fg-core --qdump <dir> 100`, two runs, the ball zoo over the noise field at 1280×720 = T6 §9's content and size)

Both records: `qdump: wrote 100 triples`; the audit prints `REPLAYABLE: every binding this push arms has its
plane in the record`, `core=fg_core contract=0xD4D55F5B671294EF`, `coverage: 100 triples | t in [0.112,1.000]
| distinct t-bins(1/8) = 8 | generations = ['0', '1', '2']` (run 2: `t in [0.113,1.000]`). Scored with the SAME
command as the legacy — and the legacy record `qd_defb1_a` re-scored in the same session with it, so the
comparison is like for like (it reproduced §9: `exact match : mean 99.88%`, `k mean 0.977`, `corr mean 0.986`).

| record | triples | exact | within 1 LSB | k mean | corr | worst px, median of per-triple max | `> 8` levels, median per triple |
|---|---|---|---|---|---|---|---|
| legacy `qd_defb1_a` (T6 §9, `wap_warp.comp`) | 16 | 99.88 % (min 99.55) | 99.96 % | 0.977 | 0.986 | 32 | 3 |
| **fg_core run 1** | 100 | **99.96 %** (min 99.87) | **99.99 %** | 0.967 | 0.995 | **1** | **0** |
| **fg_core run 2** | 100 | **99.96 %** (min 99.86) | **99.99 %** | 0.965 | 0.994 | **1** | **0** |

Quoted: `over 100 triples: exact match : mean 99.96%  min 99.87%  max 100.00% / within 1 LSB: mean 99.99% /
worst pixel : max 173  median-of-max 1 / displacement: k mean 0.967`. By phase quartile, `k` — legacy
1.000 / 1.000 / 1.000 / **0.910**; fg_core run 1 1.002 / 0.961 / 1.001 / **0.904**; run 2 1.000 / 0.962 /
1.001 / **0.891**: the oracle's own high-phase residual (`S2_T6_GATE.md` §9, t ≈ 0.87) appears identically
in all three — inherited, as §9 said it would be. The second-quartile dip (0.96, absent in the 16-triple
legacy sample) is a corpus effect: 100 triples include stationary-ball triples whose fit is degenerate
(`k min 0.000`), which a 16-triple sample did not draw. **DI-3, the two fg_core runs against each other over
8 phase bins:** exact r = **0.900**, k r = **0.984** (per-bin exact 99.94–99.97 % in both; k
1.00/1.00/0.90/1.00/1.00/1.00/0.96/0.85 vs 1.00/1.00/0.92/1.00/1.00/1.00/0.95/0.83).

Reading: through the oracle the fg_core product path is indistinguishable from the legacy — slightly MORE
exact on the pooled numbers (99.96 vs 99.88 %, worst-pixel median 1 vs 32), which is the larger, more
stationary-heavy corpus, not a kernel property (§4b already proved the two kernels compute the same
function; this section proves nothing was lost between the kernel and the record).

## 8 · Verdict — **G-R3 PASSED, with the residual attributed**

- **M4 (the veto):** the port does not change the default output — `fg_core.comp` and `wap_warp.comp` are
  **byte-identical on every one of 24,227 compared ticks** (grid, aperiodic noise, panning lattice) once the
  driver's FMA contraction is forbidden in both (§4b); with contraction allowed, the only differences are
  the ~5 × 10⁻⁹ one-level flips and, on the knife-edge pan content, the threshold flips they seed (§5) — the
  A0 M4 clause "every difference explained by a documented floating-point-order change", satisfied by an
  experiment rather than by an argument. The instrument was seen RED on a real difference (§5). The oracle
  corpus (≥ 200 triples: 100 + 100, both `REPLAYABLE`, `core=fg_core`) scores like the legacy's (§7).
- **XR1:** packed and clean rows both on record; no observable difference on this content (§5).
- **XR5:** `arm_mask` is derived at one point of the tick, beside the legacy's own per-generation gates.
- **XR7:** bug-for-bug — `phase_anchor` mixes from the rank-25 snapshot (`dominates_ok` printed); the pan
  runs (gme ≠ 0, bg_reclaim armed) are byte-identical under §4b.
- **XR13:** the depfile fires on a body edit (§2).
- **M3:** Δ 0.0 presents/60 s, both sides 14,384.0 (§6).
- **M2b:** `unmeasured` (§9). **The three R0-exit constraints:** built (WEIGHT stage; `CH_BLEND` + `select`;
  the declared-`needs` rule — each seen red on its negative test).
- **The product is UNCHANGED:** `--fg-core` is opt-in; the default still runs `wap_warp.comp`. Flipping it
  is R7's decision (the M1 baselines on both paths + the operator's eye), not R3's.

## 9 · Honesty ledger

- **M2b is `unmeasured`.** The named route (the SPIR-V variant diff) cannot see specialization: the driver folds
  the constants at pipeline creation, so "a disabled row's body absent" is visible only in the driver's ISA
  (`VK_KHR_pipeline_executable_properties` would give instruction counts per variant — not built).
- **The gate's oracle corpus is scored by the same T6 oracle that scored the legacy** — the oracle's own residual
  (t ≈ 0.87, ≤ 0.06 % of the frame, `S2_T6_GATE.md` §9) is inherited, not re-explained here.
- **The RED control is a different configuration, not a perturbed kernel:** it shows the instrument sees a
  difference when one exists; it does not prove the instrument would see every class of difference.
- **The fidelity review is delegated work:** every load-bearing note was re-verified first-hand (§2); the
  per-row "verified identical" counts are the readers' counts, not mine.
- **Not run:** `--fg-core` on a real game (the operator's eye, R7's business); a soak under `--validation`; the
  `--no-single-track` byte-identity (outside the envelope by design); GPU time per warp (M3 here is presents/s
  on an unsaturated rig — the fused kernel's cost under load is unmeasured).
- **The attribution experiment decorates ALL float results, extended instructions included;** it proves the
  residual vanishes without contraction, not which specific contraction site produced each pixel. The evidence
  list (§3, §5) locates them; naming the site per pixel was not attempted.

*Made with my soul - Swately <3*
