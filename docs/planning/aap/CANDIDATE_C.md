# CANDIDATE C — **LAYERTAB**: the layer table is the contract

> AAP pass A1, DESIGNER angle **C** ("data-driven layer table"). Written against
> `A0_FROZEN_OBJECTIVE.md` + `DESIGNER_BRIEF.md` in isolation from candidates A and B. Every file,
> line number, API and count below was read first-hand in this session; every number carries a trust
> tier (§8). This document is the deliverable; it proposes, it does not modify.

---

## 0. The bet, in one paragraph

The objective names two defects. The one the operator feels is the shader's override chain; the one
that *costs him every single time he adds a layer* is the **four-site drift** — `Config` struct,
parser, help text, UI model, hand-synced, already diverged (my own count, §5: **259** distinct `--flag`
tokens in `src/cli/cli.cpp` against **173** `flag:` entries in `ui/src/main.js`). LAYERTAB's bet is
that the second defect is the one you can kill *structurally*, and that killing it structurally also
kills the first — because once a layer is a **row** with a declared **stage**, a declared **rank**, a
declared **channel it writes** and a declared **override bit**, "the frame that ships is whatever
wrote `result` last" stops being an emergent property of a 1,336-line file and becomes a printable,
hashable, one-column fact.

The shader side I pick is **fused, generated**: one compute kernel per tick for the default set, its
layer-call chain emitted from the table, each layer gated by a SPIR-V specialization constant so a
disabled layer is compiled out. But fusion is *itself a table column* (`kind = F | P`): a layer that
needs its own SG pass flips one character and the SG registration, the barriers and the culling
follow automatically. That is the difference between this candidate and one that bets on fusion or on
staging: **LAYERTAB does not bet on either. It bets that the granularity decision should be data.**

---

## 1. Units + placement

### 1.1 New files (the whole candidate)

```
src/layers/layer_table.def      THE REGISTRY. One PFG_LAYER(...) block per layer. Nothing else
                                declares a layer. ~40 lines per layer incl. help text.
src/layers/layer_abi.hpp        The row schema: LayerId/Stage/Kind/ArmId enums, LayerDesc, ParamDesc,
                                the column set (a generator error on any unknown column).
src/layers/layer_table.hpp      Expands the .def -> `enum LayerId`, `constexpr LayerDesc kLayers[]`,
                                `constexpr ParamDesc kParams[]`, the static_asserts (unique dense
                                ranks per stage; every `requires` target exists; std140 offsets).
src/layers/layer_config.hpp     Expands the .def -> `struct LayerConfig` (the fields), and the
                                generated parser / help / JSON-model emitters (constexpr tables +
                                one loop each — no if/else chain).
src/layers/layer_registry.cpp   Runtime: resolve enables -> the VkSpecializationInfo key; write the
                                LayerParams UBO; register kind=P rows as SG passes; `--layer-dump`;
                                `--layer-model-json`; the FNV-1a 64-bit contract hash.
shaders/layers/<name>.glsl      ONE FILE PER LAYER. A pure function, signature fixed by its stage.
shaders/fg_core_math.glsl       The generation core (the 8-parameter function, §2).
shaders/fg_core.comp            The fused kernel: bindings + ctx assembly + the three generated
                                chains + fg_core(). Replaces `shaders/wap_warp.comp` at M-C2.
shaders/fg_layer_<name>.comp    Only for kind=P rows (NONE in the default set).
tools/gen_layer_glsl.cmake      Build-time generator (CMake script mode, `cmake -P`) producing the
                                GENERATED GLSL into ${CMAKE_BINARY_DIR}/gen/shaders/ (§1.3).
```

### 1.2 Files modified (not created)

- `CMakeLists.txt` — one `add_custom_command` invoking `tools/gen_layer_glsl.cmake`, then the existing
  `pfg_spv()` on `shaders/fg_core.comp` with `-I${_gen}/shaders -I${CMAKE_SOURCE_DIR}/shaders`.
  Exact precedent already in the tree: `pfg_spv()` at `CMakeLists.txt:30–41` already runs
  `cmake -P framework/render/vulkan/cmake/spv_to_header.cmake` as a build step, so **codegen adds no
  new build tool** (Python is *not* required; `tools/gen_layer_glsl.py`, stdlib-only, is the drop-in
  alternative if the CMake string parser proves brittle — see §7 weakest-point discussion).
- `src/cli/cli.hpp` / `src/cli/cli.cpp` — `struct Config` gains `LayerConfig layers;`; `parse_args`
  gains one `if (parse_layer_flag(argv[i], c.layers)) continue;` before its existing chain;
  `print_help` gains one `print_layer_help();` where the layer block used to be. The ~150 hand-written
  layer flag lines and ~55 layer fields are *deleted* from these two files, not duplicated.
- `src/present/present.cpp` — the 58-float push struct at line ~1119 and its ~140 lines of gate
  derivation (lines ~960–1118) are replaced by `layer_arm_mask(cfg, gen_state)` + a 5-field core push.
- `ui/src-tauri/src/lib.rs` — one `#[tauri::command] fn layer_model()` that runs
  `phyriad_fg.exe --layer-model-json` (via the existing `resolve_exe()` at `lib.rs:164`) and returns
  the JSON string.
- `ui/src/main.js` — the 173-entry literal's **layer section** is replaced by
  `const model = JSON.parse(await invoke('layer_model'))` + the existing control builder.

### 1.3 Generated artifacts (build outputs; **not** in the repo, **not** in `git diff --stat`)

```
${gen}/shaders/layer_params.glsl     std140 `uniform LayerParams { ... } lp;` mirroring the C++ POD
${gen}/shaders/layer_specconst.glsl  `layout(constant_id=N) const bool L_<NAME> = false;` per row
${gen}/shaders/layer_includes.glsl   `#include "layers/<name>.glsl"` per row, rank-ordered
${gen}/shaders/chain_mvcond.glsl     the ranked MVCOND call chain
${gen}/shaders/chain_sample.glsl     the ranked SAMPLE call chain
${gen}/shaders/chain_compose.glsl    the ranked COMPOSE call chain
${gen}/layer_offsets.hpp             the std140 offsets, static_asserted against layer_abi.hpp
```

### 1.4 The row schema (`src/layers/layer_abi.hpp`)

```cpp
enum class Stage : uint8_t { MVCOND, SAMPLE, COMPOSE, HOST };   // HOST = no shader side
enum class Kind  : uint8_t { F, P };                            // Fused into the stage kernel | own SG pass
enum class ArmId : uint8_t { ALWAYS, GME, BWD, GME_AND_BWD, COMMIT };  // per-GENERATION validity (§6.4)

struct ParamDesc {
    const char* name;      // "sim"
    ParamType   type;      // F32 | I32 | BOOL | ENUM
    float       dflt, lo, hi;
    const char* flag;      // "--mv-sim"  (nullptr => internal: in the dump + hash, not in the CLI)
    UiKind      ui;        // NUMBER | SWITCH | SELECT | TEXT | HIDDEN
    const char* help;
    uint16_t    ubo_offset;   // std140 offset inside LayerParams (assigned by the generator)
};

struct LayerDesc {
    LayerId     id;
    const char* name;         // "mv_guided"      -> pfg_mvcond_mv_guided / L_MV_GUIDED / --mv-guided
    Stage       stage;
    uint16_t    rank;         // THE DECLARED ORDER inside the stage. Unique + dense (static_assert).
    Kind        kind;
    bool        default_on;
    bool        overrides;    // a COMPOSE row that may DISCARD its input colour. Printed as OVERRIDE.
    uint32_t    requires;     // bitmask of LayerIds that must also be on (replaces apply_cascades
                              // for layer-scoped deps; generator errors on a missing target)
    uint32_t    reads_ch;     // declared CHANNEL reads   (MV | MV_RAW_FWD | SAD | D_PIXEL | STASIS | ...)
    uint32_t    writes_ch;    // declared CHANNEL writes  (a MVCOND row may only write MV, etc.)
    ArmId       arm;
    const char* group;        // UI group ("MV", "Composition", ...)
    const char* help;
    uint8_t     param_first, param_count;
    uint8_t     spec_id;      // dense SPIR-V specialization-constant id (generated)
};
```

### 1.5 What a row looks like

```c
/* src/layers/layer_table.def */
PFG_LAYER(BG_RECLAIM, MVCOND, 30, F, /*default_on*/true, /*overrides*/false,
          /*requires*/ PFG_REQ(GME), /*reads*/ CH_MV|CH_PREV|CH_CUR, /*writes*/ CH_MV,
          /*arm*/ ARM_GME, /*group*/ "MV",
          "Tile-scale gravity fix: damp a gme-nonconform background-fringe MV toward the global "
          "model (or toward ZERO for screen-static overlay pixels). Both warp tracks re-sample.")
  PFG_PARAM(BG_RECLAIM, strength, F32, 4.0f, 0.0f, 16.0f, "--bg-reclaim", UI_NUMBER,
            "Snap weight scale. w = strength*nonconform*evidence; ~1 = soft, >=4 = hard snap.")
PFG_LAYER_END(BG_RECLAIM)
```

That block is the *only* place `bg_reclaim` is declared. Its `Config` field, its `--bg-reclaim`
parse, its `--help` line, its UI number-control (with min/max/default/desc), its shader spec
constant, its UBO slot, its position in the chain and its per-generation arming all derive from it.

---

## 2. The core contract

`shaders/fg_core_math.glsl` — the generation core, in isolation. It is the ~35 lines the objective
identifies, expressed as a pure function with **no access to any layer parameter and no access to the
layer UBO**:

```glsl
struct CoreOut { vec4 color; float d_pixel; bool warp_ok; };

CoreOut fg_core(
    sampler2D  prev,               //  1. the A anchor (real N)
    sampler2D  cur,                //  2. the B anchor (real N+2)
    vec2       uv,                 //  3. the sample base
    vec2       mv,                 //  4. THE MV SOURCE (pixel units) - conditioned upstream
    float      t,                  //  5. temporal phase
    vec2       sad,                //  6. .x = sad_best, .y = sad_zero (Gate 1's evidence)
    float      residual_ceil,      //  7.
    float      improvement_frac,   //  8.
    float      agreement_threshold //  9.   <- see the count note below
);
```

Body (verbatim math from `wap_warp.comp` 496–529, 637, 679):

```glsl
    const vec2  sz     = vec2(textureSize(cur, 0));
    const vec4  A      = texture(prev, uv - (mv * t)         / sz);   // == line 520
    const vec4  B      = texture(cur,  uv + (mv * (1.0 - t)) / sz);   // == line 521
    const float d      = length(A.rgb - B.rgb);                       // == line 522
    // tile agreement via the SAME shared-memory reduction (s_d[64], barrier(), mean)  == 524-529
    const bool  g1     = (sad.x < residual_ceil)
                      && (sad.y * (1.0 - improvement_frac) > sad.x);  // == 497-498
    const float wa     = 1.0 - t;                                     // == line 637
    return CoreOut(wa * A + (1.0 - wa) * B, d, g1 && tile_agrees);    // == line 679
```

**Parameter count.** By the objective's own M2c enumeration (`residual_ceil, improvement_frac,
agreement_threshold, t, mv-source, output`) this is **6**. By function-signature count it is **9
arguments**, of which `prev`/`cur`/`sad` are the *sources* the objective folds into "the generation"
and does not enumerate. By descriptor count the core touches **5 bindings** (prev, cur, sad, mv_eff or
the register-resident `mv`, output). I state all three readings rather than pick the flattering one;
the ≤8 target is met on the objective's enumeration (6) and on descriptors (5), and missed by one on
raw argument count (9) — that one is `agreement_threshold`, which is genuinely a core parameter (it
is Gate 2's threshold), so the honest number is **6 contract parameters, 9 arguments, 5 descriptors**.

**Where `t` enters.** `t` enters in exactly one place: the core push constant. It is copied into
`ctx.t` for layers that need phase (vblend, phase-anchor, single-track), and nowhere else.

```cpp
// src/layers/layer_abi.hpp — the ONLY push-constant block. 20 B of fields, 32 B block.
struct CorePush {
    float    residual_ceil;
    float    improvement_frac;
    float    agreement_threshold;
    float    t;
    uint32_t arm_mask;   // per-GENERATION layer validity (§6.4). NOT read by fg_core().
};
static_assert(sizeof(CorePush) == 20);
```

**Where the MV source enters.** As a table row, rank 0 of MVCOND:

```c
PFG_LAYER(FETCH_MV, MVCOND, 0, F, true, false, PFG_REQ_NONE, CH_NONE, CH_MV, ARM_ALWAYS, "MV",
          "The primary MV source: LINEAR (bilinear) fetch of the W/8 forward field.")
```

whose body is one line — `return texture(u_motion_vectors, ctx.uv).xy;` (== `wap_warp.comp:326`).
Swapping the source (NVOFA, a different grid, a smoothed field) is a change to *this row's body*, not
a change to the core. The core has no opinion about where `mv` came from; that is the whole point of
counting "mv-source" as one parameter.

---

## 3. The layer contract

### 3.1 Declaration

A layer is declared **once**, by one `PFG_LAYER(...)` block in `src/layers/layer_table.def` (§1.5).
There is no second declaration site anywhere in the tree. The `layer_abi.hpp` column set is closed by
construction: the X-macro takes a fixed argument list, so an unknown column is a compile error, not a
convention.

### 3.2 What a layer may read and write

Fixed per stage, and *declared per row* (`reads_ch` / `writes_ch`), with the generator refusing a row
whose `writes_ch` is illegal for its stage:

| Stage | Signature | May write | May read |
|---|---|---|---|
| `MVCOND` | `vec2 pfg_mvcond_<n>(vec2 mv_in, in LayerCtx ctx)` | `CH_MV` only | uv, coord, t, mv_raw_fwd, sad, the bound images, its own UBO slice |
| `SAMPLE` | `vec2 pfg_sample_<n>(vec2 mv_in, in LayerCtx ctx)` | `CH_MV_SAMPLE` only | as above + `CH_MV` (final) |
| `COMPOSE` | `vec4 pfg_compose_<n>(vec4 c_in, in LayerCtx ctx)` | its return value only | as above + `CH_A_SAMP`, `CH_B_SAMP`, `CH_D_PIXEL`, `CH_STASIS`, `CH_WARP_OK` |
| `HOST` | (no shader side) | — | — |

`LayerCtx` is a `const` struct assembled once by `fg_core.comp` before the chains run. Samplers are
the global bindings (GLSL opaque types stay at file scope); `LayerCtx` carries the scalars/vectors:

```glsl
struct LayerCtx {
    vec2  uv; ivec2 coord; vec2 out_size; float t;
    vec2  mv_raw_fwd;      // the CH_MV_RAW_FWD snapshot (see rank 25 below)
    vec2  sad;             // .x sad_best  .y sad_zero
    vec4  A_samp, B_samp;  // filled after the core runs; zero during MVCOND/SAMPLE
    float d_pixel;         //         "
    bool  stasis_block;    // the CH_STASIS channel (published by the STASIS row)
    bool  warp_ok;
};
```

### 3.3 Where layer parameters live

In **one generated uniform buffer**, never in the core's push block:

```glsl
// ${gen}/shaders/layer_params.glsl  (GENERATED - do not edit)
layout(set = 0, binding = 15, std140) uniform LayerParams {
    float mv_guided_sim;        // offset  0
    float inertia_thresh;       // offset  4
    float bg_reclaim_strength;  // offset  8
    float ambig_tie_ratio;      // offset 12
    float ambig_min_sep_px;     // offset 16
    float vblend_t0;            // offset 20
    float vblend_strength;      // offset 24
    uint  vblend_exact;         // offset 28
    uint  single_track_variant; // offset 32
    float stasis_thresh;        // offset 36
    ...
} lp;
```

The core never names `lp`. A layer names only its own slice (the generator emits a
`#define BGR_strength lp.bg_reclaim_strength` alias per row so a layer body cannot typo into a
neighbour's field without a compile error). The UBO is `HOST_VISIBLE|HOST_COHERENT`, written on
**config change**, not per tick.

### 3.4 Enable / disable

Per row, a dense specialization constant:

```glsl
// ${gen}/shaders/layer_specconst.glsl  (GENERATED)
layout(constant_id = 3) const bool L_BG_RECLAIM = false;
```

`layer_registry.cpp` builds a `VkSpecializationInfo` from the resolved `LayerConfig` and creates the
pipeline. A disabled row's branch is folded at `vkCreateComputePipeline` time and its body dead-code
eliminated — **no runtime branch, no texture tap, no register pressure**. The honest floor if a
driver declines to specialize: the branch degenerates to a warp-uniform `if`, i.e. exactly today's
`if (pc.bg_reclaim > 0.001)` cost — a strict non-regression either way.

For `kind = P` rows, disable means the SG node is **not registered**, so SG culls it:
zero dispatches, zero barriers, zero allocation (`seam_graph.hpp:266–306`, the backward-reachability
cull).

### 3.5 Composition order — the answer to kill criterion 2

LAYERTAB does **not** claim order-independence. Mixing is not commutative
(`mix(mix(a,b,w1),c,w2) != mix(mix(a,c,w2),b,w1)`), and any candidate that claims MV-conditioning
layers commute is wrong. LAYERTAB takes the objective's other option — **an explicit declared order
that is part of the contract** — and makes it enforceable:

1. **Stage order is fixed and total:** `MVCOND -> SAMPLE -> fg_core() -> COMPOSE`. It is a property of
   `fg_core.comp`'s structure, not of any row.
2. **Within a stage, `rank` is the order.** One integer per row, in one file. The generator
   `static_assert`s that ranks are unique within a stage.
3. **A COMPOSE layer is `vec4 f(vec4 c_in, ctx)`.** There is no shared mutable `result` in scope.
   Today's defect — line 1321 rewriting a variable declared 800 lines earlier and thereby discarding
   lines 531–1320 — is *not expressible*: a COMPOSE layer receives the prior value as an argument. If
   it chooses to ignore that argument, that choice is visible in a 6-line function.
4. **An override is a declared bit.** A COMPOSE row that ignores `c_in` must set `overrides = true`.
   The generator emits a build error if a row's body never references `c_in` and `overrides` is false
   (a `grep`-level check in the CMake generator — crude, but it catches the accident that produced
   today's situation). `--layer-dump` prints such a row as `OVERRIDE` and the contract hash covers it.
5. **The order is attributable to a measurement.** `layer_registry.cpp` computes a FNV-1a 64-bit
   **contract hash** over the compiled chain (each row's id, rank, stage, kind, override bit, and every
   resolved parameter value) and stamps it into `--csv` telemetry and the `--qdump` manifest. Two runs
   with the same hash had the same layer contract; two with different hashes did not. That closes the
   "which build produced this CSV" hole that makes today's A/B comparisons fragile.

`--layer-dump` on the shipping default prints, in execution order:

```
[layertab] contract=0x8F31C2A0DE447B15  (8 rows enabled of 53)
MVCOND   0 fetch_mv        F  arm=ALWAYS
MVCOND  10 mv_guided       F  arm=ALWAYS   sim=0.100
MVCOND  20 inertia         F  arm=ALWAYS   thresh=0.500   requires={mv_guided|mv_edge_snap}
MVCOND  25 :snapshot mv_raw_fwd                                  (pseudo-row, no body)
MVCOND  30 bg_reclaim      F  arm=GME      strength=4.000 requires={gme}
MVCOND  40 phase_anchor    F  arm=BWD                     requires={occl}
MVCOND  45 :sample sad -> ctx.sad ; :eval stasis -> ctx.stasis_block
MVCOND  50 ambig           F  arm=GME      tie_ratio=1.150 min_sep_px=2.000 requires={gme}
SAMPLE 110 vblend          F  arm=ALWAYS   t0=0.600 strength=0.500 exact=0
CORE       fg_core                          res_ceil=? improv=? agree=0.050 t=<per-tick>
COMPOSE280 stasis          F  arm=ALWAYS   thresh=0.500          OVERRIDE
COMPOSE290 single_track    F  arm=ALWAYS   variant=v3_2          OVERRIDE  reads={stasis_block}
```

Every hidden ordering fact I had to dig out of `wap_warp.comp`'s comments is a printed row above.

---

## 4. The default-set mapping (all eight, each so M4's replay equivalence can be run)

Each entry gives: the table row, the source lines its `shaders/layers/<name>.glsl` body is moved from
**verbatim**, and any representation change with its M4 argument.

### 4.1 `mv_guided` — MVCOND rank 10, kind F, default ON

Body = `guided_mv()` (`wap_warp.comp:179–202`) plus the `mv_edge_snap` precedence test
(`319–327`, which becomes its own row `MV_EDGE_SNAP` at rank 5, default OFF, `overrides` of the MV
channel declared via `writes_ch = CH_MV` + `requires_absent` on rank 10 — the generator emits
`if (!L_MV_EDGE_SNAP)` around rank 10's call, exactly today's `else`).
Params: `sim` (F32, 0.10, `--mv-sim`).
**Representation change:** today the host packs `mv_guided = 1.0 + sim` into one float
(`present.cpp` `use_mv_guided?1.0f+cfg.mv_sim:0.f`) and the shader recovers `sim = mv_guided - 1.0`
(`wap_warp.comp:325`). LAYERTAB stores `sim` as a real `float` in the UBO and the enable as a spec
constant. The *value* reaching `guided_mv()`'s `sim_thresh` is bit-identical for any `sim` that is
exactly representable, and `1.0f + 0.10f - 1.0f` is **not** exactly `0.10f` in float32 — so this
change *can* alter the last bit of `sim_thresh`. `sim_thresh` is used only in two comparisons
(`best_d > sim_thresh`, `(second_d - best_d) <= sim_thresh`), so a 1-ulp shift changes the output only
for a pixel sitting exactly on the band. **M4 protocol:** run the equivalence with the *packed* value
(`sim = fma(1.0f, 1.0f, sim) - 1.0f` reproduced host-side) to prove byte-identity first, then flip to
the clean value and re-measure PSNR. This is the kind of detail that decides an M4 veto and I flag it
rather than assume it away.

### 4.2 `inertia` (the inertia gate) — MVCOND rank 20, kind F, default ON at 0.50

Body = the persistence tap (`304–306`) + the gate (`332–334`). Params: `thresh` (F32, 0.50,
`--inertia-thresh`). Reads `u_persistence`.
**Structural change:** today the tap is hoisted above the primary fetch and the gate tests
`(pc.mv_guided > 0.5 || pc.mv_edge_snap > 0.5)`. In LAYERTAB the tap moves *inside* the layer body
(same sampler, same `uv`, same `texture()` -> same value) and the predicate becomes
`if (!(L_MV_GUIDED || L_MV_EDGE_SNAP)) return mv_in;` on spec constants, plus a declared
`requires = PFG_REQ(MV_GUIDED) | PFG_REQ(MV_EDGE_SNAP)` with `REQ_ANY` semantics. Numerically
identical. This row is the first evidence that `requires` needs an ANY/ALL distinction — a column the
8 default layers already forced me to add (§7).

### 4.3 `bg_reclaim` — MVCOND rank 30, kind F, default ON at 4.0

Body = `358–396` verbatim (the three-hypothesis form: `nonconf`, the 4 warp taps, `d_zero`'s 2 taps,
`st_over_model`, `zero_like`, `target`, `evid`, `w`, `mv = mix(mv, target, w)`). Params: `strength`
(F32, 4.0, `--bg-reclaim`). `requires = GME`, `arm = ARM_GME`.
**No representation change** — `bgr_push` is already the raw strength
(`present.cpp`: `cfg.bg_reclaim > 0.f && gme_push && use_wap ? cfg.bg_reclaim : 0.f`); the gating half
becomes the spec constant AND the arm bit, the value half becomes the UBO field. Byte-identical.

### 4.4 `phase_anchor` — MVCOND rank 40, kind F, default ON

Body = `397–411` verbatim (the bwd tap, the optional matte `claim` term, `w_b`, `mv = mix(mv_fwd, -mv_bwd, w_b)`).
No exposed params (the `smoothstep(0.35,0.65,t)` constants become internal `ParamDesc`s with
`flag = nullptr`: in the dump and the hash, absent from the CLI).
`requires = OCCL` (the bwd field), `arm = ARM_BWD` (the per-generation `bwd_ok`).
**The `mv_fwd` snapshot** (`wap_warp.comp:344`, "the classification's raw fwd leg") becomes an
explicit pseudo-row at **rank 25** — `:snapshot mv_raw_fwd` — sitting exactly where line 344 sits
(after mv-guided + inertia, before bg-reclaim). Today that placement is a comment; in LAYERTAB it is a
row you can see in `--layer-dump` and that the hash covers. Byte-identical.

### 4.5 `ambig` — MVCOND rank 50, kind F, default ON

Body = `426–444` verbatim. Params: `tie_ratio` (1.15) and `min_sep_px` (2.0) promoted from magic
numbers to internal `ParamDesc`s (`flag = nullptr`, in the dump/hash, not on the CLI). Reads
`u_candidates`, `u_dissidence`, `u_dissidence_bwd`. `requires = GME`, `arm = ARM_GME`.
Rank 50 > 40 preserves today's order (ambig runs after phase-anchor). The `sad` tap (`412–414`) that
today sits between them becomes the pseudo-row `:sample sad` at rank 45. Byte-identical.

### 4.6 `vblend` — **SAMPLE** rank 110, kind F, default ON

Body = `507–514` verbatim (`vb_w`, `mv_ut`, `mv_target_s`, `mv_fwd_eff = mix(mv, mv_target_s, vb_w)`).
Params: `t0` (0.6), `strength` (0.5), `exact` (BOOL, false = PREDICT). Reads `u_mv_target`.
**This row is the clearest win of the stage column.** Today the invariant "only the two warp sample
offsets use `mv_fwd_eff`; everything downstream keeps the RAW `mv`" (lines 504–506) is a *comment* —
nothing stops the next layer from consuming `mv_fwd_eff`. In LAYERTAB it is the **stage**: a `SAMPLE`
row's return value flows only into `fg_core()`'s `mv` argument and is not written back to `CH_MV`, so
the occlusion round-trip's raw fields are structurally protected. Byte-identical.

### 4.7 `single_track` / screen-static — **COMPOSE** rank 290, kind F, `overrides = true`, default ON (v3.2)

Body = `1321–1329` verbatim:
```glsl
vec4 pfg_compose_single_track(vec4 c_in, in LayerCtx ctx) {   // OVERRIDE: c_in is deliberately discarded
    vec4 r = ctx.B_samp;
    if (ST_variant == 0u) {                                   // v3.2
        const vec4  cur0   = texture(u_cur_real, ctx.uv);
        const float d_zero = length(cur0.rgb - texture(u_prev_real, ctx.uv).rgb);
        float w_s = smoothstep(1.2, 3.0, (ctx.d_pixel + 0.02) / (d_zero + 0.02));
        if (ctx.stasis_block) w_s = 1.0;                      // the declared CH_STASIS read
        r = mix(ctx.B_samp, cur0, w_s);
    }
    return r;
}
```
Params: `variant` (ENUM {v3_2, v3_0}, `--st-no-stasis` selects v3_0). Today packed as a single float
(1.0/2.0); the UBO carries a real `uint`. The comparison `pc.single_track < 1.5` becomes
`ST_variant == 0u` — exact, no float compare. Byte-identical.
**Its four upstream shadows** (`wa_eff` collapse at 678, `blend_result` base at 692–695, the forced
warp selection, commit/ts-smooth inertness) become a declared `shadows = {WA_EFF_ZERO,
BLEND_BASE_CUR, FORCE_WARP, COMMIT_INERT}` column, emitted by the generator as four additional spec
constants the CORE reads. **This is a genuine wart** — a COMPOSE row reaching back into the core — and
I name it as such in §7 rather than dress it as a feature. It is, however, *declared and printed*,
where today it is four scattered `if (pc.single_track > 0.5)` sites.

### 4.8 `stasis` — **two rows**, because in the shader it is two things

- **`STASIS`** — a pseudo-row at MVCOND rank 45 evaluates the block predicate
  (`sad_zero <= stasis_thresh` from `u_sad_field.g`) and **publishes** `ctx.stasis_block`
  (`writes_ch = CH_STASIS`); and a COMPOSE row at **rank 280**, `overrides = true`, whose body is
  line 1191: `if (ctx.stasis_block) return texture(u_cur_real, ctx.uv); return c_in;`.
  Params: `thresh` (F32, 0.50, `--stasis-thresh` / `--no-stasis`).
- The **saturation** inside single-track (`if (stasis) w_s = 1.0`, line 1327) is not a third layer: it
  is `single_track`'s **declared read** of `CH_STASIS` (§4.7). The coupling that today is an
  in-scope boolean becomes a declared channel edge the generator checks (a row may not read a channel
  no earlier-ranked row writes).

Under the shipping default both are on, ranks 280 < 290 preserve today's order (stasis at 1191 before
single-track at 1321), and the composed value is byte-identical.

**Coverage check against KC4:** mv-guided ✔ (4.1), inertia gate ✔ (4.2), phase-anchor ✔ (4.4),
bg-reclaim ✔ (4.3), ambig ✔ (4.5), vblend ✔ (4.6), single-track/screen-static ✔ (4.7), stasis ✔ (4.8).
All eight expressed; nothing dropped; M4's A/B is runnable at M-C2 (§6.5).

---

## 5. The flags / UI touchpoint

### 5.1 The four sites, and what each becomes

| Site | Today | Under LAYERTAB |
|---|---|---|
| `Config` fields | ~204 hand-written declaration lines in `src/cli/cli.hpp` (my count: `grep -cE '^\s+(bool\|int\|float\|...)\s+[a-z_]'` over lines 14–995) | `struct LayerConfig` **generated** by the X-macro from the same `.def` |
| Parser | a hand-written chain in `src/cli/cli.cpp` (787 lines) | `constexpr` `{flag, LayerId, ParamId, kind}` table + **one loop**, generated |
| Help | a multi-hundred-line `std::printf` literal (`cli.cpp:7–…`) | `print_layer_help()` walks `kLayers`/`kParams`, groups by `group` |
| UI model | a 173-entry literal in `ui/src/main.js` | **deleted.** `--layer-model-json` emits the model; one Tauri command shells the exe; the JS builds controls from the reply |

### 5.2 Why the UI drift becomes impossible (not merely unlikely)

The UI stops being a source of truth and becomes a **renderer of the binary's own model**. If the
binary does not have a flag, the UI cannot show it; if the binary has it, the UI shows it with the
binary's default, range, group and help text. The 84 CLI-only flags cited in the objective cannot
recur for table rows, because there is no second list to fall behind. `ui/src/main.js:29–34` already
documents exactly the control kinds (`switch`, `switch-off`, `switch-on`, `select`, `number`, `text`)
that `UiKind` enumerates, so the JS control builder is reused unchanged — only its *input* changes.

### 5.3 Honest scope boundary

The 173 UI entries are **not** all layers. Capture backend, monitor pick, GPU selection, pacing,
diagnostics — roughly 150 of them — are host concerns with no shader side. LAYERTAB covers them with
`stage = HOST` rows (schema identical, shader columns unused), but **this candidate's committed scope
is the layer rows.** The mechanism generalizes; migrating the host flags is a later, mechanical pass
with no product risk. Claiming otherwise would be inflation.

### 5.4 M2a — files touched to add one trivial layer

1. `src/layers/layer_table.def` — the `PFG_LAYER(...)` block (the registration).
2. `shaders/layers/<name>.glsl` — the body.

**= 2 files** on `git diff --stat`; the generated GLSL and headers live in `${CMAKE_BINARY_DIR}/gen`
and are not tracked. Target met.

**The honest exception:** a layer that needs a *new device image* (a new aux field to sample) also
needs host-side creation + per-pair upload — real code the table cannot generate. The table's `binds`
column emits the descriptor-set-layout entry and the placeholder-binding "completeness trick" that
today is hand-written (see the `wapC2A` / `wapMVTA` / `wapPrevOutA` placeholder comments in
`src/core/app_init.hpp:366–381`), but the allocation and the upload are 2 more files
(`warp_blend/warp_blend_init.cpp`, `present/present.cpp`). So: **2 files for a layer over existing
channels, 4 for a layer that needs a new field.** M2a's reference exercise is the former.

---

## 6. Budget arithmetic at 1920×1080

### 6.1 Passes per tick (the default set)

**1 compute dispatch**, exactly as today. All eight default rows are `kind = F`; the chains are inlined
into `fg_core.comp`. No stage is a separate pass in the default configuration.

### 6.2 Intermediate images

**0 bytes for the default set.** No intermediate image exists: `mv`, `mv_sample`, `A_samp`, `B_samp`,
`d_pixel`, `stasis_block` are all registers inside one invocation.

The two channels that *would* materialize if a row is flipped to `kind = P`:

| Channel | Format | Bytes at 1920×1080 | Traffic/tick (W+R) |
|---|---|---|---|
| `CH_MV` (`mv_eff`) | `R16G16_SFLOAT` | 1920·1080·4 = **8,294,400 B = 7.91 MiB** | 15.83 MiB |
| `CH_COLOR` | `R8G8B8A8_UNORM` | 1920·1080·4 = **8,294,400 B = 7.91 MiB** | 15.83 MiB |

At the RTX 4090's spec 1,008 GB/s, 15.83 MiB of traffic is ≈ **15.7 µs** of pure-bandwidth time per
tick (`estimated`: arithmetic from the spec figure and the pixel count; ignores cache hits and latency
overlap). At 240 ticks/s that is ≈ 3.8 ms/s ≈ **0.38 %** of wall time per split. So a *single* split
is affordable; splitting every MVCOND row (5 splits = ~79 MiB/tick) would cost ~1.9 % — which is why
`kind` is a per-row decision made against measurement, not a global architecture bet.

### 6.3 Barriers

- **Default set: 0 added.** The fused kernel is one SG node. For reference, the current warp path in
  `src/present/present.cpp` records **1 dispatch + 3 `img_barrier` calls** around the output blit
  (`wapOutA` GENERAL→TRANSFER_SRC at ~1215, `bridge_img` UNDEFINED→TRANSFER_DST at ~1216, `wapOutA`
  TRANSFER_SRC→GENERAL at ~1218) in the default configuration (no `--afill`, no overlay, no
  `--ts-smooth`) — counted first-hand. Under LAYERTAB those three become SG-derived instead of
  hand-written; the count is unchanged.
- **Each `kind = P` row: exactly +1 RAW barrier.** This is SG's own documented layerability property
  (`apps/minimal_fg/include/minimal_fg/seam_graph.hpp:47`, "This is what makes '+1 pass == +1 barrier'
  hold", and the coverage logic at 361–390 that suppresses a second barrier for a same-scope reader).
- **No seam regression (KC3):** no per-stage fence, no host round-trip, no `ALL_COMMANDS` — SG's
  derivation emits precise stage/access masks only (`seam_graph.hpp:38–39`).

### 6.4 CPU record cost per tick

| | Today | LAYERTAB |
|---|---|---|
| Push assembly | 58 float stores into an anonymous struct (`present.cpp:1119–1150`) | 5 stores into `CorePush` |
| `vkCmdPushConstants` | 232 B | 20 B (32 B block) |
| Gate derivation | ~140 lines of per-tick `bool *_push` computation (`present.cpp:960–1118`) | one `layer_arm_mask()` call producing a `uint32_t` |
| Layer parameter upload | (folded into the push) | **none per tick** — the UBO is written on config change |
| Dispatch | 1 | 1 |

Structurally ≤ today on every line. I claim **no M3 win** from this — the saving is at most single-digit
microseconds and no profile of the push assembly exists. The M3 target is "no regression", and this is
non-regressive by construction. `estimated`.

**The `arm_mask` — the load-bearing detail.** Specialization constants are fixed at pipeline creation,
but four of the eight default layers must disarm **per generation**: `gme_push` (the affine model can
be stale), `bwd_push` (`bwd_ok` — F skipped the bwd pass under pressure), `matte_push`, `appear_push`
(`present.cpp:968–1050`). A pure compile-out mechanism *cannot* express that. LAYERTAB's answer is the
`arm` column plus a 32-bit `arm_mask` in the core push, ANDed with the spec constant in the generated
chain:

```glsl
if (L_BG_RECLAIM && (pc.arm_mask & (1u << 3u)) != 0u) mv = pfg_mvcond_bg_reclaim(mv, ctx);
```

Cost: 4 bytes of push and one uniform test per armed row. Benefit: the per-generation degrade that
keeps today's shader correct survives intact. I surface this because it is exactly where a naive
"just compile it out" design breaks, and because it is a partial admission against my own mechanism
(§7).

### 6.5 The layer UBO

Fields for the eight default rows: `mv_guided_sim` 4 + `inertia_thresh` 4 + `bg_reclaim_strength` 4 +
`ambig_tie_ratio` 4 + `ambig_min_sep_px` 4 + `vblend_t0` 4 + `vblend_strength` 4 + `vblend_exact` 4 +
`single_track_variant` 4 + `stasis_thresh` 4 = **40 B**, std140-rounded to a **48 B** block. Extended
to all 53 layers currently gated in `wap_warp.comp` at ~2 params each: ≈ **450 B**. Two copies for
frames-in-flight: **< 1 KiB VRAM**. `computed` (arithmetic over the field list above).

### 6.6 Pipeline creation

One `vkCreateComputePipeline` with a `VkSpecializationInfo` of N bools, at init and on every
layer-enable change. Cost `unmeasured`; NVIDIA compute pipeline creation from cached SPIR-V is
typically O(1–10 ms) but I have not measured it on this rig. Mitigations: (a) it happens on config
change only, and the UI already restarts the FG on a flag change (`ui/src/main.js:19` — "con el FG
vivo, cambiar un flag reinicia el FG"); (b) a `VkPipelineCache` persisted to disk makes repeats cheap.
**This is a real regression axis** for any future live-toggle-without-restart feature, where today a
toggle is a free push-constant value.

### 6.7 M1 measurability (kill criterion 1)

The core keeps the MV path and the sampling form exactly: the same conditioned `mv`, the same
`A[uv − t·mv]` / `B[uv + (1−t)·mv]` at the same full-res sampler with the same clamp-to-edge, the same
`t`. Marker positions therefore remain a function of the `--qdump+` record. The record's push-block
component becomes `CorePush` (5 typed fields) + `LayerParams` (a self-describing, generator-emitted
layout) + the contract hash — strictly *more* recoverable than 58 anonymous floats, and the hash makes
a replay attributable to an exact layer contract. The existing `--qdump` writer
(`present.cpp:1315–1357`, prev/live/next + `t` + a manifest) is untouched by this candidate; the
`--qdump+` extension the instrument needs writes `CorePush` + the UBO blob + the hash into the
manifest, which is a ~15-line addition. KC1 is not triggered.

### 6.8 First measurable milestone on the rig

**M-C1 — "the registry, with zero product risk" (the first milestone; GPU math untouched).**
Deliverables: `src/layers/*` exists and generates; `phyriad_fg.exe --layer-dump` prints the eight
default rows with rank/stage/kind/params/contract-hash; `--layer-model-json` emits the model; the UI's
layer section renders from it. **`shaders/wap_warp.comp` is unchanged** — the generated `LayerConfig`
is asserted field-by-field against the existing `Config` by a startup self-check
(`layer_config_parity()`), so any divergence is a loud abort, and the push block is still assembled
from `Config`.
*Acceptance:* build green; `layer_config_parity()` passes; `tools/ball_zoo.ps1` at 60 fps source,
1920×1080, 60 s × 2 runs shows present cadence within the run-to-run spread of the E1 baseline
(the objective's G1 figure: 240 presents/s on the 240 Hz panel, 28,799 presents/120 s —
`measured` by the cited `RESTRUCTURE_PLAN` record, not by me). M2a is measurable here (add a trivial
`HOST`-stage row, count `git diff --stat` = 2 files). M4 is trivially satisfied (no math changed).

**M-C2 — the first real cut.** `shaders/fg_core.comp` replaces `wap_warp.comp` for the default set,
with the eight rows fused from the table.
*Acceptance:* M4 — byte-identical output on the `--qdump` replay set against the pre-change binary via
the CPU reference warp (`MOTION_TRUTH_MASTER_PLAN` §5 T6 / S6), with the §4.1 packing question
resolved first in its packed form; then M3 — cadence + warp-record + GPU time per tick within the
measured run-to-run spread, 2 runs per side.

Shipping the drift fix *before* the shader risk is deliberate: M-C1 delivers the candidate's entire
M2a/M5 value with an M4 veto that cannot fire.

---

## 7. What it drops, and my weakest point

### 7.1 What it drops (explicitly)

- **The 232 B / 57-field push block** and with it the float **packing tricks**:
  `mv_guided = 1.0 + sim`, `mv_edge_snap = variant + sim_band`, `single_track ∈ {1.0, 2.0}`,
  `bg_reclaim` doubling as gate+strength. Replaced by typed UBO fields + spec constants + `arm_mask`.
  One of these (§4.1) carries a real 1-ulp M4 risk that I flag rather than hide.
- **`apply_cascades()` for layer-scoped dependencies** (`src/cli/cli.hpp:1008`) — replaced by the
  `requires` column, which the generator checks at build time instead of at startup. Non-layer
  cascades (capture / GPU / present) stay hand-written.
- **`print_help`'s hand-maintained literal** for layer flags (~150 of its lines).
- **`ui/src/main.js`'s 173-entry literal** for its layer section, and with it the 84-flag CLI-only gap
  for table rows.
- **The invisibility of an override.** Not the override itself — `single_track` and `stasis` keep
  their exact behavior (M4 and KC4 forbid otherwise). What is dropped is the ability to write one by
  accident.
- **Nothing else.** No layer, no math, no default is removed. This candidate is deliberately
  non-subtractive; "fewer layers" is a forbidden driver (A0 §6).

### 7.2 My honest weakest point

**The column set is not proven closed, and every escape hatch I needed came from just eight layers.**

In mapping the eight default-affecting layers I was forced to add four columns beyond the clean
schema I started with:

1. `requires` needed **ANY** semantics, not just ALL, because `inertia` gates on
   `mv_guided || mv_edge_snap` (§4.2).
2. `single_track` needed a **`shadows`** column — a COMPOSE row reaching *backwards* into the core's
   blend weights, `blend_result` base, selection and commit inertness (§4.7). That is precisely the
   coupling the stage model is supposed to forbid, and I kept it because M4 forbids changing the
   product. It is a declared, printed wart; it is still a wart.
3. `stasis` needed a **channel** (`CH_STASIS`) so `single_track` could read its boolean (§4.8) —
   layer-to-layer data flow that the stage pipeline does not otherwise model.
4. Several rows needed **pseudo-rows** (`:snapshot mv_raw_fwd` at rank 25, `:sample sad` / `:eval
   stasis` at rank 45) to pin ordering facts that are not layers at all.

Eight layers, four schema extensions. The shader has **53**. The failure mode is a slow slide into a
table whose columns are a bespoke DSL only one person can read — the same accretion the shader has
today, relocated into a schema, with the *added* cost that a mis-designed column is now a
build-system change rather than a shader edit, and a generator bug is a silent miscompile rather than
a visible wrong pixel. My mitigation is real but weak: the X-macro's fixed argument list makes an
unknown column a compile error, so growth is at least visible and reviewed. I have **not**
demonstrated that the column set converges. That is the first thing an adversarial pass should
attack, and the cheapest experiment that would settle it is: **map ten more layers from
`wap_warp.comp` onto the schema and count new columns.** If it is ≤ 1, the schema is converging; if
it is ≥ 4 again, LAYERTAB's central claim is false and the candidate should lose.

**Secondary weakness.** The spec-constant enable mechanism is not sufficient on its own — the
`arm_mask` (§6.4) is a runtime gate I had to reintroduce for the four layers that must disarm per
generation. "A disabled layer is compiled out" is therefore true for a *statically* disabled layer and
false for a *dynamically* disarmed one, where the cost degrades to today's uniform branch. And layer
toggling moves from free (a push value) to a pipeline rebuild (§6.6), which is a genuine regression
for any future live-toggle UI.

**Third.** M2b ("GPU work of a disabled layer = 0 dispatches + 0 barriers") is satisfied
*vacuously* for `kind = F` rows: there are no per-layer dispatches to count. The real proof for a
fused layer is a timestamp delta (which M2b does mention), and that measurement does not exist yet.
I decline to score M2b as passed on the dispatch/barrier reading alone.

---

## 8. Trust tiers on every number

| Claim | Value | Tier | Source |
|---|---|---|---|
| Push block fields | 57 fields / 58 floats / 232 B | `measured` (read) | `wap_warp.comp:77–135`; `present.cpp:1119` struct + `sizeof(pcw)` comment "58 floats/232B" |
| Core needs 6 parameters | 6 contract / 9 args / 5 descriptors | `measured` (read) + `computed` | A0 §1 M2c enumeration; my signature in §2 |
| Distinct `--flag` tokens in `cli.cpp` | **259** | `measured` | `grep -o '\-\-[a-z0-9][a-z0-9-]*' src/cli/cli.cpp \| sort -u \| wc -l` (includes tokens appearing only in comments; brief reports 257) |
| UI model entries | **173** | `measured` | `grep -c 'flag: "' ui/src/main.js` (exact match with the brief) |
| `Config` declaration lines | **~204** | `measured` | `awk 'NR>14&&NR<995' cli.hpp \| grep -cE '^\s+(bool\|int\|float\|…)\s+[a-z_]'` |
| Current warp path barriers (default) | 1 dispatch + 3 image barriers | `measured` (source count) | `src/present/present.cpp` ≈1215–1218; `--afill`/overlay/`--ts-smooth` paths excluded |
| Passes per tick, default set | **1** | `computed` | all 8 default rows are `kind = F` |
| Intermediate image bytes, default set | **0** | `computed` | no channel materializes |
| `CH_MV` image if split | 8,294,400 B = 7.91 MiB | `computed` | 1920·1080·4 (R16G16_SFLOAT) |
| Bandwidth time for one split | ≈ 15.7 µs/tick, ≈ 0.38 % at 240 ticks/s | `estimated` | 15.83 MiB ÷ 1,008 GB/s spec; ignores cache/latency overlap |
| `kind = P` barrier cost | +1 RAW per pass | `measured` (by SG's golden test), `unmeasured` on this rig | `seam_graph.hpp:47`, 361–390; 122-check test |
| Layer UBO, 8 default rows | 40 B fields / 48 B block | `computed` | field list, §6.5 |
| Layer UBO, all 53 layers | ≈ 450 B, < 1 KiB with 2 frames-in-flight | `estimated` | 53 × ~2 params × 4 B |
| Per-tick CPU: 5 stores + 20 B push vs 58 stores + 232 B | non-regressive | `estimated` | no profile of push assembly exists |
| Pipeline creation on toggle | O(1–10 ms) | `unmeasured` | typical NVIDIA figure; not measured on this rig |
| M3 baseline | 240 presents/s, 28,799 presents/120 s | `measured` elsewhere | A0 §1 M3 citing `RESTRUCTURE_PLAN` G1; not re-measured by me |
| Files to add a trivial layer | **2** (4 if it needs a new device image) | `computed` | §5.4 |
| Spec constants dead-code-eliminate the body | — | `unmeasured` | driver behavior; the stated floor is a warp-uniform branch = today's cost |
| §4.1 packing 1-ulp risk | `1.0f + 0.10f - 1.0f != 0.10f` | `computed` (IEEE-754 binary32) | not verified against a running shader |
| Column-set convergence (the central claim) | — | **`unmeasured`** | 8 layers mapped, 4 new columns; 53 layers exist. §7.2 names the experiment that settles it. |

---

## 9. Kill-criteria self-check (A0 §4)

| # | Criterion | Verdict |
|---|---|---|
| 1 | Unmeasurable by MOTION_TRUTH | **Clear.** Same MV path, same sampling form, same `t`; the record becomes typed + hashed (§6.7). |
| 2 | Order-dependent (last-writer-wins) semantics | **Clear.** No shared mutable `result`; COMPOSE is `vec4 f(vec4 c_in, ctx)`; order is one declared `rank` column, uniqueness `static_assert`ed, printed by `--layer-dump`, covered by the contract hash; an override is a declared bit (§3.5). I do **not** claim order-independence — I claim explicit declared order, which the criterion permits. |
| 3 | Seam regression (per-stage fence / host round-trip / `ALL_COMMANDS`) | **Clear.** Default set is one fused dispatch; `kind = P` rows go through SG's precise-mask derivation (§6.3). |
| 4 | Cannot express the shipping default | **Clear.** All eight mapped verbatim with source lines (§4). |
| 5 | Injection / multi-GPU dependency | **Clear.** Nothing in the table touches capture identity or device count. |
| 6 | New runtime dependency | **Clear.** Codegen is a *build* step in CMake script mode (precedent: `spv_to_header.cmake` at `CMakeLists.txt:36–40`); the shipped binary gains nothing beyond Vulkan + D3D11/DXGI + the VC++ runtime. No runtime shader compiler. |

---

*Made with my soul - Swately <3*
