# CANDIDATE B — "Stages as SG passes": the generation as a fixed stage skeleton with slotted layer nodes

> AAP pass A1, isolated DESIGNER, **ANGLE B**. Written against `A0_FROZEN_OBJECTIVE.md` + `DESIGNER_BRIEF.md`
> and the files they cite, read first-hand (`shaders/wap_warp.comp`, `apps/minimal_fg/include/minimal_fg/seam_graph.hpp`,
> `src/core/app_init.hpp`, `src/core/fg_context.hpp`, `src/cli/cli.hpp`, `src/cli/cli.cpp`,
> `src/present/present.cpp`, `src/warp_blend/warp_blend.hpp`, `ui/src/main.js`,
> `docs/research/MINIMAL_FG_MASTER_PLAN.md` §2–§4, `docs/BENCHMARKS.md`,
> `docs/research/UPLOAD_OFFLOAD_MASTER_PLAN.md`). No other candidate was consulted.

**The bet in one sentence:** make the generation a *fixed, declared skeleton of four stages* whose boundaries
are real images on the SG seam, so that a layer is a graph node plugged into exactly one stage slot, the
composition rule of each slot is an *algebra* (one-of / OR / weighted-mix / max) rather than an assignment,
and a disabled or unreachable layer is culled by `SeamGraph::compile()` with no human deciding — paying for
that modularity in intermediate-image bandwidth, which is measured and budgeted below.

**Trust tiers used throughout:** `measured` (an instrument produced the number, cited), `estimated` (derived
by arithmetic from a measured or spec quantity, with the arithmetic shown), `unmeasured` (a claim no
instrument on this rig has produced). Nothing in this document is asserted without one of these three.

---

## 0. The two facts that shaped this candidate (both read first-hand, both `measured` by reading)

**Fact 1 — the sequential-override defect is not hypothetical; it is currently *eating a shipping default
layer*.** In `shaders/wap_warp.comp`:

- line 344: `const vec2 mv_fwd = mv;` captures the post-fetch, post-inertia primary MV.
- lines 358–396 (`bg_reclaim`, default ON at strength 4.0, `src/cli/cli.hpp:470`) damp `mv` toward the
  background/zero hypothesis: `mv = mix(mv, target, w);` (line 394).
- line 410 (`phase_anchor`, default ON, `src/cli/cli.hpp:566`) then does
  `mv = mix(mv_fwd, -mv_bwd, w_b);`

The phase-anchor write takes `mv_fwd` — the value from *before* bg-reclaim — as its base. It does not read
`mv`. **Under the shipping default (both layers ON, `bidir=true` so `occl_thresh=1.5>0`), bg-reclaim's damp
is computed, its six extra full-res texture taps are paid, and its result is then discarded in its entirety
by the next layer.** This is exactly kill criterion 2, live, in the default configuration. It is also the
single strongest argument for a structure in which "who writes what" is declared and machine-checked: this
bug is invisible in a 1,336-line shader and is a *compile-time warning* in the design below (§3.5).

*(This candidate REPRODUCES the bug in its port — M4 is an oracle for the structure change, not a licence to
change the product. The fix is a one-line manifest edit gated behind the M1 instrument; see §4.3.)*

**Fact 2 — under the shipping default, the composition of the frame is three lines.** `single_track=1.0`
(`src/cli/cli.hpp:446`) makes lines 1321–1330 the last writer:

```glsl
result = B_samp;
const vec4  cur0   = texture(u_cur_real, uv);
const float d_zero = length(cur0.rgb - texture(u_prev_real, uv).rgb);
float w_s = smoothstep(1.2, 3.0, (d_pixel + 0.02) / (d_zero + 0.02));
if (stasis) w_s = 1.0;
result = mix(B_samp, cur0, w_s);
```

Everything between lines 531 and 1320 — the occlusion round-trip, the commit family, the matte, onepos,
the soft/hard gate selection, `d_tile` and its shared-memory barrier — contributes **nothing** to the
shipped pixel. The shader's own comment (1315–1317) says so. A stage graph makes that fact *structural*:
those nodes are not backward-reachable from the graph output and `SeamGraph::compile()` removes them,
including their dispatches, their barriers and (with one small extension) their images. That is objective
M2b — "GPU work of a disabled layer = exactly 0" — obtained from the existing, golden-tested culling code
(`seam_graph.hpp:266-301`) rather than from a promise.

---

## 1. Units: passes / shaders / structs, named, with file placement

### 1.1 The stage skeleton (fixed; four stages, never per-layer)

| Stage | Name | Purpose | Declared output images |
|---|---|---|---|
| S1 | **COND** — MV conditioning | turn the W/8×H/8 MV grid into the two per-pixel MV fields the sampler needs | `mv_fwd`, `mv_cond`, `mv_samp` |
| S2 | **SAMPLE** — the CORE | the two displaced fetches at phase `t` and their disagreement | `samp_A` (optional), `samp_B`, `d_pixel` |
| S3 | **COMPOSE** | turn the sample packet into one colour | `result` |
| S4 | **POST** | whole-image operations after composition (declared ordered chain) | `result_post` |

The skeleton is **not** configurable. A layer never invents a stage, never invents an image, and never
declares a read of a resource a later stage owns. `--sg-dump` prints the skeleton with the live nodes in it.

### 1.2 Slots (the stage's declared plug points, with their composition algebra)

| Slot | Stage | Cardinality | Composition rule |
|---|---|---|---|
| `COND/FETCH` | S1 | **exactly one** | selection; two candidates = a build error |
| `COND/RESTRICT` | S1 | **many** | boolean **OR** of "fires"; all nodes restore the *same* value (the plain bilinear fetch) → commutative |
| `COND/DAMP` | S1 | **many** | weighted mix toward each node's target: `mv' = mv·(1−ΣW) + Σ wᵢ·Tᵢ`, `ΣW` clamped to 1 → commutative |
| `COND/ANCHOR` | S1 | **at most one** | selection |
| `COND/ARBITRATE` | S1 | **at most one** | discrete re-selection among a declared candidate set |
| `COND/PREDICT` | S1 | **at most one** | derives `mv_samp` from `mv_cond`; empty ⇒ S2 reads `mv_cond` directly |
| `SAMPLE/GATE` | S2 | **many** | each writes its own confidence image; consumers name them individually |
| `COMPOSE/BASE` | S3 | **exactly one** | selection (may be a *decorator*: `matte(inner=<base>)`, nesting declared in the manifest) |
| `COMPOSE/PIN` | S3 | **many, target-keyed** | same target key ⇒ `W = maxᵢ wᵢ`; different keys ⇒ weighted mix in registry order |
| `POST/CHAIN` | S4 | **many, ordered** | the registry declares the order; it is printed by `--sg-dump` and is part of the contract |

### 1.3 File placement in the PhyriadFG tree

```
src/seam/
  seam_graph.hpp                     ADOPTED from apps/minimal_fg/include/minimal_fg/ + 4 declared extensions (§A)
  test_seam_graph.cpp                ADOPTED verbatim (436 lines, 122 checks) — must stay green
src/fg/
  fg_graph.hpp / fg_graph.cpp        builds a SeamGraph from the resolved Config; owns the stage skeleton
  fg_resources.hpp / .cpp            the ResId table + image create/destroy (takes the wap* images out of WapInit)
  fg_registry.hpp / fg_registry.cpp  LayerDesc / ParamDesc / SlotPolicy + the conflict & dominance checks
  layers.inc                         THE registration list — one line per layer
  core/
    core_sample.comp                 the CORE shader; entry point main(), local_size 8x8x1
    core_sample.hpp / core_sample.cpp
  layers/<id>/
    desc.hpp                         constexpr LayerDesc + the params POD + the ParamDesc table
    node.comp                        the node shader; entry point main(), local_size 8x8x1
    record.cpp                       the 4-call record fn (bind pipeline, bind set, push t, dispatch)
```

Layer directories created by this candidate (one per node named in §4, plus the non-default hosts):
`mv_fetch_linear/ mv_fetch_guided/ mv_fetch_edge_snap/ inertia_restrict/ bg_reclaim/ bg_snap/
phase_anchor/ ambig_arbitrate/ vblend_predict/ gate_confidence/ gate_agreement/ base_b_track/
base_phase_blend/ base_matte/ pin_screen_static/ pin_stasis/ post_ts_smooth/ post_afill/ post_overlay_fps/`
plus the remaining `wap_warp.comp` families (`occl_*`, `commit_*`, `onepos`, `mc_*`, `crescent`, `travel`,
`contour`, `disoccl_*`, `appear`, `extrap`, `band_xfade`, `cam_lead`) as further nodes in the same slots.

### 1.4 What is NOT re-created

`framework/render/vulkan/OpticalFlowPipeline` (pyramid block match, MV+SAD at W/8), the `FgContext` C/F/P
thread model (`src/core/fg_context.hpp`), the 13 E1 `init_*` functions and their ownership structs
(`src/core/app_init.hpp`), the host bridges (`HostBridgeInit`), the DDA/WGC capture, the present bridge, the
pacing loop, `fg_quality_scorer`, `prep_zoo_sequence.py`. `WapInit` loses only its image members (they become
SG-declared resources bound with `bind_image`); `WapPipe` is replaced by one pipeline per live node.

---

## 2. The core contract

### 2.1 The core pass

```cpp
// src/fg/core/core_sample.hpp
struct CoreSamplePush { float t; };                       // 4 bytes. THE ENTIRE core parameter list.
```

```glsl
// src/fg/core/core_sample.comp — the CORE. Textually the same arithmetic as wap_warp.comp:518-522.
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set=0, binding=0) uniform sampler2D u_prev_real;   // A (phase 0)
layout(set=0, binding=1) uniform sampler2D u_cur_real;    // B (phase 1)
layout(set=0, binding=2, rg32f)   uniform readonly  image2D u_mv_samp;   // the MV SOURCE
layout(set=0, binding=3, rgba32f) uniform writeonly image2D u_samp_A;    // optional_write
layout(set=0, binding=4, rgba32f) uniform writeonly image2D u_samp_B;
layout(set=0, binding=5, r32f)    uniform writeonly image2D u_d_pixel;
layout(push_constant) uniform P { float t; } pc;
layout(constant_id = 0) const bool kStoreA = true;        // driven by graph liveness (§A.2)

void main() {
    const ivec2 c  = ivec2(gl_GlobalInvocationID.xy);
    const ivec2 sz = imageSize(u_samp_B);
    if (c.x >= sz.x || c.y >= sz.y) return;               // NO shared memory here -> an early return is legal
    const vec2 uv  = (vec2(c) + vec2(0.5)) / vec2(sz);
    const vec2 mv  = imageLoad(u_mv_samp, c).xy;          // pixel units
    const vec4 A   = texture(u_prev_real, uv - (mv * pc.t)         / vec2(sz));
    const vec4 B   = texture(u_cur_real,  uv + (mv * (1.0 - pc.t)) / vec2(sz));
    if (kStoreA) imageStore(u_samp_A, c, A);
    imageStore(u_samp_B, c, B);
    imageStore(u_d_pixel, c, vec4(length(A.rgb - B.rgb), 0, 0, 0));
}
```

**Core parameter count: 1** (`t`). Objective M2c target is ≤ 8. The three gate thresholds
(`residual_ceil`, `improvement_frac`, `agreement_threshold`) are *not deleted* — they move into the
`gate_confidence` / `gate_agreement` layer nodes in the `SAMPLE/GATE` slot, where they belong, and where the
default configuration culls them (§4.9).

The early `return` is legal here precisely because the core has **no `barrier()`** — the tile-mean reduction
that forces `wap_warp.comp` to run out-of-bounds invocations through the whole body (its comment, lines
23–28) lives in `gate_agreement`, which keeps that discipline locally.

### 2.2 Images the core reads and writes

| Resource | Format | Extent | Bytes | Producer |
|---|---|---|---|---|
| `prev_real` | RGBA8 UNORM, sampled, SRO | 1920×1080 | 8,294,400 | imported (host upload, outside the graph) |
| `cur_real` | RGBA8 UNORM, sampled, SRO | 1920×1080 | 8,294,400 | imported |
| `mv_samp` | **RG32F**, storage, GENERAL | 1920×1080 | 16,588,800 | `COND/PREDICT`, else aliased to `mv_cond` at build |
| `samp_A` | **RGBA32F**, storage, GENERAL | 1920×1080 | 33,177,600 | core (optional write) |
| `samp_B` | **RGBA32F**, storage, GENERAL | 1920×1080 | 33,177,600 | core |
| `d_pixel` | **R32F**, storage, GENERAL | 1920×1080 | 8,294,400 | core |

**Why fp32 and not fp16 for the stage-crossing images:** `A_samp`/`B_samp` are the results of bilinear
filtering of an 8-bit UNORM texture, returned as fp32 in the fused shader's registers and only quantised
once, at the final `imageStore` to RGBA8 (line 1334). The Vulkan spec fixes only a *minimum* filter-weight
precision for linear filtering and leaves the arithmetic precision implementation-defined, so there is no
narrower format that is *provably* a lossless carrier of that value. fp16 would make M4 an empirical
PSNR question instead of a byte-identity proof (§8 quantifies it). Same argument for `mv_cond`/`mv_samp`:
the conditioned MV is a chain of fp32 `mix()`es of RG16F grid samples, so RG16F would round it. This
choice is the single biggest line in the budget (§6) and it is the honest price of the angle.

**Where `t` enters:** as the core's only push constant, written per output-clock tick by
`wap_warp_present` (`src/present/present.cpp:879`), from the existing `t_use` computation
(`present.cpp` 2221→2370). Every other node that needs `t` (vblend, phase-anchor, and today's extrap
family) receives the same 4-byte push. Nothing else is per-tick.

**Where the MV source enters:** as the *resource* `mv_samp`. The core names a resource, never a layer. The
whole MV provenance — grid fetch policy, restriction, damping, anchoring, arbitration, prediction — is
upstream, and swapping it (e.g. an NVOFA field, `src/core/app_init.hpp:240`) is a change to which pass
writes `mv_cond`, one line in `fg_graph.cpp`, with the core untouched.

### 2.3 Per-pair and per-config data (not core parameters)

- `PairConsts` UBO, written once per pair-advance by the P thread:
  `struct PairConsts { float gme_fwd[6]; float gme_bwd[6]; float matte_thresh; uint32_t bwd_valid; };`
  (56 bytes). Bound at set 1, binding 0 to every node that declares `needs_pair`.
- `L_<id>Params` UBO per layer, written when the config epoch changes (startup, or a UI edit), never per
  tick. Bound at set 2, binding 0. **A layer's parameters are never in the core's push block, and never in
  another layer's UBO** — that is the contract that makes 57 push fields structurally impossible.

---

## 3. The layer contract

### 3.1 Declaration — one directory, one line

```cpp
// src/fg/layers/bg_reclaim/desc.hpp
namespace fg::bg_reclaim {
struct Params { float strength; };                          // the layer's OWN UBO POD
inline constexpr ParamDesc kParams[] = {
  { "--bg-reclaim-strength", "BG reclaim: strength",
    "Damp weight scale [0,4] (1 = soft LSFG-style decay, 4 = hard snap, the shipping default). 0 = off.",
    PType::Float, offsetof(Params, strength), /*def*/4.0f, /*lo*/0.0f, /*hi*/4.0f, /*step*/0.5f },
};
inline constexpr LayerDesc kDesc = {
  .id           = "bg_reclaim",
  .slot         = Slot::COND_DAMP,
  .enable_flag  = "--no-bg-reclaim",  .default_on = true,
  .requires_    = Req::Gme,                                  // the declared dependency; resolve_config cascades it
  .params       = kParams, .n_params = 1, .params_size = sizeof(Params),
  .reads        = R(Res::mv_cond) | R(Res::prev_real) | R(Res::cur_real),
  .writes       = W(Res::mv_cond),
  .needs_tick   = true, .needs_pair = true,
  .spv          = kSpv, .spv_words = kSpvWords,
};
}  // namespace fg::bg_reclaim
```

```cpp
// src/fg/layers.inc — the ONLY other file an added layer touches
FG_LAYER(mv_fetch_guided)
FG_LAYER(inertia_restrict)
FG_LAYER(bg_reclaim)
...
```

`src/fg/fg_registry.cpp` includes `layers.inc` once, expanding `FG_LAYER(x)` to `&fg::x::kDesc,`. That is
**2 files for a new layer** (its own directory + one line), objective M2a target ≤ 2.

### 3.2 What a layer may read and write

A layer may read any resource *its own stage or an earlier stage* produces, plus any imported resource. A
layer may write **only the resource(s) its slot owns**: `COND/*` own `mv_fwd`/`mv_cond`/`mv_samp`,
`SAMPLE/GATE` own their private gate images, `COMPOSE/*` own `result`, `POST/CHAIN` owns `result_post`.
A `writes` mask naming a resource outside the slot's ownership is rejected by `fg_registry`'s build-time
check with the layer id and the offending resource in the message — the check runs in the same
`resolve_config` pass that already prints the flag cascades (`src/cli/cli.cpp:204-224`).

### 3.3 Enabling and disabling

`fg_graph.cpp` walks the registry in slot order, and calls `sg.add_pass(...)` **only for layers whose
`enable_flag` resolved on and whose `requires_` are satisfied**. A disabled layer is therefore never
declared: it has no `VkPipeline`, no descriptor set, no dispatch, no barrier, and no entry in `--sg-dump`.
A layer that is declared but whose output nothing live consumes is removed by `compile()`'s backward
reachability from `mark_output` (`seam_graph.hpp:266-301`) — the second, automatic path to zero cost, and
the one that makes Fact 2 (§0) structural rather than a promise.

Because SG's execution order is "the declared (add) order, filtered by culling, then asserted to be a valid
topological order" (`seam_graph.hpp:50-51`, and the read-before-write check at 356-360), the registry's slot
order *is* the graph order, and a mis-ordered declaration is a loud `Compiled::errors` entry, never a
silently mis-synchronised graph.

### 3.4 How N enabled layers compose — the algebra, slot by slot

The decisive property. **No slot's rule is "assign".**

- **`COND/FETCH` (exactly one).** A selection among `linear` / `guided` / `edge_snap`. Today's implicit
  precedence (`mv_edge_snap` silently overrides `mv_guided`, `wap_warp.comp:313-322`) becomes an explicit
  resolution in `resolve_config` that prints the same message it prints for every other cascade. Two enabled
  FETCH nodes can never both run.
- **`COND/RESTRICT` (many, commutative).** Each node computes a boolean "restrict" and, where it fires,
  writes the *plain bilinear grid fetch*: the restored value is identical for every RESTRICT node, so the
  composition is `OR` and the result is independent of order — provably, not by convention.
- **`COND/DAMP` (many, commutative).** Each node emits `(target Tᵢ, weight wᵢ)`. The slot composes
  `mv' = mv·(1−ΣW) + Σ wᵢTᵢ` with `ΣW = clamp(Σwᵢ,0,1)`.
  *Singleton fast path (a property of the slot, not of any layer):* with exactly one DAMP node the slot
  instantiates that node's direct-apply variant — an RMW of `mv_cond` computing `mix(mv, T, w)` — which is
  byte-identical to `wap_warp.comp:394`. With ≥2 nodes the slot instantiates an accumulator image
  `damp_acc` (RGBA16F: `.xy = Σwᵢ Tᵢ`, `.z = Σwᵢ`), each node RMWs it, and one `damp_apply` node consumes it.
  **Honest limit:** fp addition is not associative, so with ≥2 damp nodes the composition is algebraically
  commutative but numerically order-dependent in the last ULP. The registry fixes a canonical order and
  `--sg-dump` prints it, which is kill-criterion 2's second branch ("an EXPLICIT declared order that is part
  of the contract"), not its first.
- **`COND/ANCHOR`, `COND/ARBITRATE`, `COND/PREDICT` (at most one each).** Selections, not accumulations.
- **`COMPOSE/BASE` (exactly one).** A base *may* be a declared decorator over another base
  (`base = matte_two_layer(inner = phase_blend)`); the nesting is written in the manifest, is visible in
  `--sg-dump`, and is the only legal way one composition layer may sit on top of another.
- **`COMPOSE/PIN` (many, target-keyed).** Each pin emits `(target key, target value, weight)`. Pins sharing
  a target key compose by `W = max wᵢ`; pins with different keys compose by the weighted form in registry
  order. Under the shipping default both live pins carry the key `cur@uv`, so the rule reduces to `max` —
  which is *exactly* `wap_warp.comp:1326-1328` (`w_s = smoothstep(...); if (stasis) w_s = 1.0;` ≡
  `max(w_s, stasis ? 1 : 0)` because `w_s ∈ [0,1]`). Byte-identical by construction, not by testing.
- **`POST/CHAIN` (many, ordered).** The one slot with a genuine sequential semantics. The order is a field
  of the registry, printed by `--sg-dump`, and reviewable in one place.

### 3.5 The mechanism that makes kill criterion 2 *checkable*, not merely promised

Composition rules are a discipline; disciplines rot. This candidate mechanises the check in the graph
compiler, using state `compile()` already tracks.

In `SeamGraph::compile()`'s WRITES loop (`seam_graph.hpp:411-417`) the branch
`else if (s.has_producer && !read_here)` is entered **exactly** when a pass overwrites a resource that an
earlier live pass wrote and that this pass does not read. That branch already emits the WAW barrier; this
candidate adds, in the same branch, a `Compiled::warnings` entry:

```
dominates: pass "L_phase_anchor" writes res 3:"mv_cond" without reading it
           -> the write by "L_bg_reclaim" is discarded
```

Every last-writer-wins composition in the whole graph produces exactly one such line, at build time,
naming both parties. Running it against a faithful port of today's default set produces the line above —
i.e. **the design's own conformance check finds the live bug of §0 without anyone looking for it.**
`fg_registry` promotes the warning to a hard error unless the layer carries an explicit
`.dominates_ok = "reason"` field in its `LayerDesc`; the phase-anchor port carries
`.dominates_ok = "M4 fidelity: reproduces wap_warp.comp:410, which bases on mv_fwd; see CANDIDATE_B §4.3"`.

The warning is printed by a **new** `dump_warnings()`, never by `dump()`, so the existing 122-check golden
test's byte-exact `dump()` output is unchanged (`seam_graph.hpp:479-507`, golden determinism note at 40-41).

---

## 4. The default-set mapping (each of the eight default-affecting layers)

Shipping defaults read first-hand from `src/cli/cli.hpp`: `mv_guided=true` (:428), `inertia=true`,
`inertia_thresh=0.50` (:617,:624), `phase_anchor=true` (:566), `bg_reclaim=4.0f` (:470), `ambig=true`
(:577), `vblend=true`, `vblend_t0=0.6`, `vblend_strength=0.5`, `vblend_exact=false` (:120-128),
`single_track=true` (:446), `stasis=true`, `stasis_thresh=0.50f` (:588,:592), with `gme=true` (:485),
`bidir=true`, `occl_thresh=1.5` (:409,:411), `matte=false` (:492), `igpu_field=false` (:98) — the last of
which cascades `bg_snap=false`, `band_xfade=0`, `afill=false`, `disoccl_hardpick=0`
(`src/cli/cli.cpp:207-212`), so the `wap_warp.comp:460` contour-band block does not run in the default.

| # | Layer | Node id | Slot | Reads | Writes | Ports shader lines |
|---|---|---|---|---|---|---|
| 1 | mv-guided | `mv_fetch_guided` | `COND/FETCH` | `mv_grid`, `cur_real` | `mv_cond`, `mv_fwd`* | 179-200, 324-327 |
| 2 | inertia gate | `inertia_restrict` | `COND/RESTRICT` | `mv_cond`, `mv_grid`, `persist_grid` | `mv_cond`, `static_mask`* | 304-306, 332-334 |
| 3 | bg-reclaim | `bg_reclaim` | `COND/DAMP` | `mv_cond`, `prev_real`, `cur_real`, `PairConsts` | `mv_cond` | 358-396 |
| 4 | phase-anchor | `phase_anchor` | `COND/ANCHOR` | `mv_fwd`, `mvb_grid` (+`dis_*` when matte live) | `mv_cond` | 397-411 |
| 5 | ambig | `ambig_arbitrate` | `COND/ARBITRATE` | `mv_cond`, `cand_grid`, `sad_grid`, `PairConsts` | `mv_cond` | 416-444 |
| 6 | vblend | `vblend_predict` | `COND/PREDICT` | `mv_cond`, `mvt_grid` | `mv_samp` | 500-514 |
| 7 | single-track / screen-static | `base_b_track` + `pin_screen_static` | `COMPOSE/BASE` + `COMPOSE/PIN` | `samp_B`, `d_pixel`, `prev_real`, `cur_real` | `result` | 1321-1330 |
| 8 | stasis | `pin_stasis` | `COMPOSE/PIN` | `sad_grid` | `result` (weight contribution) | 1049, 1327 |

`*` = an `optional_write` (§A.2): the store is compiled out when no live pass reads the resource.

### 4.1 mv-guided → `COND/FETCH` (`mv_fetch_guided`)
Body = `guided_mv()` verbatim (`wap_warp.comp:179-200`), including its two bilinear fall-backs (`best_d >
sim_thresh` or a near-tie), which are what make the OFF path byte-identical today. Params
`{ float sim_thresh; }` — the host's `mv_guided = 1.0 + sim` packing (`present.cpp` push assembly) is
**dropped**: the node's own UBO carries `sim` directly, and enablement is graph membership, not a
magnitude test on a float. Writes `mv_cond`; also writes `mv_fwd` when a live node reads it (phase-anchor,
or the occlusion classification when `--bidir` composition nodes are live).

### 4.2 inertia gate → `COND/RESTRICT` (`inertia_restrict`)
Body = `wap_warp.comp:304-306` (the persistence tap and `inertia_static`) plus 332-334 (fall back to the
plain bilinear MV when the fetched MV exceeds 2 px in a static-history block). Params
`{ float thresh; }` (0.50). It additionally publishes `static_mask` (R8 full-res, `optional_write`) so the
two *other* inertia gates — reject fast rescue candidates, skip the gme model rescue candidate
(`src/cli/cli.hpp:617-623`) — can be expressed as reads by the `commit_rescue` / `gme_rescue` nodes
without those nodes re-deriving the predicate. Under the shipping default those nodes are absent
(`use_rescue` requires `--rescue`, `src/core/app_init.hpp:78`), so `static_mask` is culled and only the
fetch gate is live — which is what today's default push produces.

### 4.3 phase-anchor → `COND/ANCHOR` (`phase_anchor`)
Body = `wap_warp.comp:397-411`. It declares `reads = {mv_fwd, mvb_grid}` and `writes = {mv_cond}` — i.e.
it does **not** read `mv_cond`, exactly as the shader does not read `mv`. `compile()` therefore emits the
WAW barrier *and* the dominance warning of §3.5 naming `bg_reclaim`, and the node carries the explicit
`.dominates_ok` justification quoted there. **This is deliberate:** M4 requires the port to reproduce
today's default output byte-for-byte, bug included. Changing `.reads` to include `mv_cond` and re-basing the
mix on it is a **one-line manifest edit** whose effect is a genuine product change and therefore belongs to
the M1 gate (`ACTION_PLAN.md` S4.2), not to this restructure. The `claim` modulation (lines 403-408) reads
`dis_fwd`/`dis_bwd`, which only exist when the matte node is live; with `matte=false` the registry drops
those two reads and the node compiles to `claim = 0.0` — byte-identical to today, where `matte_on = 0`
leaves `claim` at its initialiser.

### 4.4 bg-reclaim → `COND/DAMP` (`bg_reclaim`)
Body = `wap_warp.comp:358-396`, including the `nonconf > 0` short-circuit that keeps the 4+2 taps off the
model-conforming majority, the three-hypothesis `st_over_model` arbitration, and the final
`mv = mix(mv, target, w)`. Params `{ float strength; }` (4.0). It is the **only** DAMP node in the default
graph (`bg_snap` is cascaded off by `igpu_field=false`), so the slot's singleton fast path applies and the
emitted code is `mix(mv, T, w)` — byte-identical. With `--igpu-field --bg-snap` both damps are live and the
composition becomes the commutative accumulator, which **differs from today's sequential
`mix(mix(mv,T₁,w₁),T₂,w₂)`**. That is a declared semantic change in a non-default configuration, outside
M4's scope (M4 is the default set), and it is a deliberate KC-2 repair rather than an accident.

### 4.5 ambig → `COND/ARBITRATE` (`ambig_arbitrate`)
Body = `wap_warp.comp:426-444` verbatim: the aliasing test (`cand.z <= 1.15*sad_best` and
`|cand.xy − mv| > 2px`), the mv-free object proxy (dropped with matte, as above), and the gme referee pick.
Params `{}` — every constant in this layer is a literal in the shader today and stays one. It RMWs
`mv_cond`, so no dominance warning.

### 4.6 vblend → `COND/PREDICT` (`vblend_predict`)
Body = `wap_warp.comp:500-514`. Params `{ float t0; float strength; uint32_t exact; }` (0.6, 0.5, 0).
It writes `mv_samp` and leaves `mv_cond` untouched — which is precisely the invariant the shader states in
prose ("ONLY the two warp sample offsets use `mv_fwd_eff`; everything downstream keeps the RAW mv", lines
504-506) and which the two-resource contract now *enforces*: a downstream node that wants the tilted MV must
declare a read of `mv_samp`, and `--sg-dump` shows it.

### 4.7 single-track / screen-static → `COMPOSE/BASE` + `COMPOSE/PIN`
- `base_b_track`: `base = imageLoad(u_samp_B, c)` — `wap_warp.comp:1322`.
- `pin_screen_static`: key `cur@uv`, target `texture(u_cur_real, uv)`, weight
  `smoothstep(1.2, 3.0, (d_pixel + 0.02) / (d_zero + 0.02))` with
  `d_zero = length(cur[uv].rgb − prev[uv].rgb)` — lines 1324-1326.
- The `--st-no-stasis` variant (push 2.0 = pure `B_samp`) becomes "the base with no pins" — a graph
  configuration, not a magnitude branch inside a float.

The v1/v2 gates the shader keeps upstream and calls "shadowed (harmless; they save dead work)" (lines
1315-1317) — the `wa_eff` collapse (678), the pure-`cur` `blend_result` (692-695), the forced-warp
selection, the inert commit/ts-smooth — are **structurally unrepresentable** here and are dropped (§7):
a node is either in the graph or it is not; it cannot be live-but-shadowed.

### 4.8 stasis → `COMPOSE/PIN` (`pin_stasis`)
`stasis = (thresh > 0) && (texture(u_sad_field, uv).g <= thresh)` (`wap_warp.comp:1048-1049`), emitted as
key `cur@uv`, target `texture(u_cur_real, uv)`, weight `stasis ? 1.0 : 0.0`. Params
`{ float stasis_thresh; }` (0.50). Composed with `pin_screen_static` by the same-key `max` rule (§3.4),
giving `mix(B_samp, cur0, max(w_s, stasis ? 1 : 0))` ≡ lines 1326-1328 exactly. The line-1191 override
(`if (stasis) result = texture(u_cur_real, uv);`) contributes nothing under the default because 1321-1330
overwrite it, and it disappears with the rest of the shadowed tail.

### 4.9 What the default graph therefore contains — and what it does not

**Live (8 compute passes):** `mv_fetch_guided → inertia_restrict → bg_reclaim → phase_anchor →
ambig_arbitrate → vblend_predict → core_sample → compose (base_b_track + pin_screen_static + pin_stasis)`.

**Declared-but-culled or not declared:** `gate_confidence`, `gate_agreement` (and with them the 8×8
shared-memory reduction, `d_tile`, and the whole `sources_agree` path), the soft/hard selection, the
occlusion round-trip family, the commit family, `onepos`, `matte` and its five satellites, `mc_*`,
`ts_smooth`, `extrap`, `cam_lead`, `band_xfade`, `bg_snap`, `blend_solo`. **M2b for each of them: 0
dispatches, 0 barriers, verified by `--sg-dump`'s census, not by inspection.**

### 4.10 M4 and M1 measurability under this mapping

- **M4 (veto).** Every default node above is a verbatim port of a contiguous region of `wap_warp.comp`,
  operating in fp32 on fp32 stage-crossing images, with the only compositions being (a) a single-node DAMP
  reducing to the identical `mix`, and (b) a same-key `max` of two pins that is algebraically identical to
  the shader's `if (stasis) w_s = 1.0`. The remaining risk is arithmetic *ordering*, not arithmetic
  *content*, and it is bounded to the two places where a value crosses a memory boundary instead of staying
  in a register (§8).
- **M1 / kill criterion 1.** The graph's imported resources are exactly today's warp bindings 0-13
  (`prev_real`, `cur_real`, `mv_grid`, `sad_grid`, `mvb_grid`, `cand_grid`, `mvt_grid`, `persist_grid`, and
  the matte/field/prev-out images when their nodes are live). **No new external input is introduced**, so the
  `--qdump+` replay record's requirement set is unchanged, and marker positions remain a function of the
  record because `core_sample`'s sampling form is textually identical to lines 518-521. (Separately observed:
  A0 §4.1 describes the record as carrying "prev/next/live + t + MV + SAD + gme + push block", which does not
  name the persistence, candidate or backward-MV grids that today's *default* set already reads. That gap is
  the instrument's, identical for every candidate, and this candidate neither widens nor narrows it.)

---

## 5. The flags / UI touchpoint

Today: `Config` fields (`src/cli/cli.hpp`, 1,011 lines), the parser and the help text
(`src/cli/cli.cpp`, 787 lines), and the hand-written `controls` arrays in `ui/src/main.js` (1,153 lines) —
four sites, drifted to 257 CLI flags vs 173 UI entries, 84 CLI-only (A0 §0).

This candidate collapses that to **one** for the layer surface:

1. **The single source is `ParamDesc`** in the layer's `desc.hpp`. It carries the flag string, the UI label,
   the one help/tooltip string, the type, the byte offset into the layer's params POD, and
   default/min/max/step. There is no second copy of any of those.
2. **The parser** gains a generic pre-pass in `parse_args`: for each `LayerDesc`, match `enable_flag`; for
   each `ParamDesc`, match `flag` and write the parsed value at `offset` into that layer's slice of a
   `std::array<std::byte, kLayerParamsArena>` held by `Config`. The hand-written `Config` fields for every
   registered layer are deleted. Unregistered flags (capture, pacing, telemetry, governor — the majority of
   the 257) keep today's parser untouched.
3. **`--help`** is generated by walking the same registry. It cannot drift because there is nothing to sync.
4. **The UI** stops hand-maintaining its arrays. A new `--dump-flags` emits the registry as JSON
   (`[{id, slot, enable_flag, default_on, params:[{flag,name,desc,type,def,lo,hi,step}]}]`); a Tauri command
   in `ui/src-tauri/src/lib.rs` runs `PhyriadFG.exe --dump-flags` once at startup and hands the JSON to
   `ui/src/main.js`, which renders it with its existing control renderers. That is a **one-time** refactor of
   `main.js`, not per-layer work.
5. **The drift is then machine-checkable:** a startup self-audit (extending the one already in
   `resolve_config`, `src/cli/cli.cpp:227+`) asserts that every registered flag is parseable and that
   `--dump-flags`'s entry count equals the registry's — a CI-able equality, replacing "257 vs 173" with an
   invariant.

**Honest scope:** this kills the four-site drift for the ~40 warp-layer flags this restructure owns. The
other ~215 flags are untouched and still hand-synced. Claiming otherwise would be inflation.

---

## 6. Budget arithmetic at 1920×1080

1920 × 1080 = **2,073,600 px**. Per-image bytes:
RGBA8 = 8,294,400 B (7.91 MiB) · R32F = 8,294,400 B (7.91 MiB) · RG32F = 16,588,800 B (15.82 MiB) ·
RGBA32F = 33,177,600 B (31.64 MiB). MV grid 240×135 = 32,400 texels: RG16F = 129,600 B (126.6 KiB) ·
RGBA16F = 259,200 B (253.1 KiB) · R8 = 32,400 B (31.6 KiB). *(`estimated` only in the sense of arithmetic;
the extents and formats are read from `wap_warp.comp` bindings and `WapInit`.)*

### 6.1 Resident VRAM added by the default graph

| Image | Format | MiB |
|---|---|---|
| `mv_fwd` | RG32F | 15.82 |
| `mv_cond` | RG32F | 15.82 |
| `mv_samp` | RG32F | 15.82 |
| `samp_B` | RGBA32F | 31.64 |
| `d_pixel` | R32F | 7.91 |
| `samp_A` | RGBA32F | **culled** (0) |
| **Total added** | | **87.01 MiB** |

`result` reuses today's `wapOutA` (RGBA8, 7.91 MiB). 87 MiB against 24 GB is `estimated` negligible in
capacity and material in bandwidth — which is the whole point of the next table.

### 6.2 Per-tick DRAM traffic, default graph (`estimated`; arithmetic shown, worst case = every read counted at full image size)

| # | Pass | Reads (MiB) | Writes (MiB) | Total |
|---|---|---|---|---|
| 1 | `mv_fetch_guided` | mv_grid 0.12 + cur_real 7.91 | mv_cond 15.82 + mv_fwd 15.82 | 39.67 |
| 2 | `inertia_restrict` | mv_cond 15.82 + mv_grid 0.12 + persist 0.03 | mv_cond 15.82 | 31.79 |
| 3 | `bg_reclaim` | mv_cond 15.82 + prev 7.91 + cur 7.91 | mv_cond 15.82 | 47.46 |
| 4 | `phase_anchor` | mv_fwd 15.82 + mvb_grid 0.12 | mv_cond 15.82 | 31.76 |
| 5 | `ambig_arbitrate` | mv_cond 15.82 + cand 0.25 + sad 0.12 | mv_cond 15.82 | 32.01 |
| 6 | `vblend_predict` | mv_cond 15.82 + mvt_grid 0.12 | mv_samp 15.82 | 31.76 |
| 7 | `core_sample` | prev 7.91 + cur 7.91 + mv_samp 15.82 | samp_B 31.64 + d_pixel 7.91 | 71.19 |
| 8 | `compose` | samp_B 31.64 + d_pixel 7.91 + prev 7.91 + cur 7.91 + sad 0.12 | result 7.91 | 63.40 |
| | **Total (worst case)** | | | **≈ 349 MiB / tick** |

Crediting AD102's 72 MB L2 for the three repeat reads of `prev_real` and of `cur_real` (−47.5 MiB) gives
**≈ 302 MiB/tick**; the working set (87 MiB of fp32 fields alone) exceeds L2, so the true figure sits in the
**300–350 MiB/tick** band. `estimated`.

**Baseline for comparison:** today's single fused dispatch reads `prev` 7.91 + `cur` 7.91 + `mv` 0.12 +
`sad` 0.12 + `persist` 0.03 and writes `result` 7.91 ≈ **24.0 MiB/tick** of distinct-image traffic
(`estimated`; the fused shader's 10-20 taps/pixel of `prev`/`cur` are absorbed by the texture cache and are
not additional DRAM traffic). **Ratio ≈ 13–15×.**

**In time.** RTX 4090 theoretical bandwidth 1,008 GB/s = 938.7 GiB/s; assuming 65–75 % achievable
(610–704 GiB/s, `unmeasured` on this rig): 302–349 MiB/tick ⇒ **0.42–0.56 ms/tick added**, against
0.03–0.04 ms for the baseline's DRAM term. At the 240 Hz panel the tick budget is 4.17 ms, so this is
**10–13 % of a tick**; at 180 Hz (5.56 ms) it is 7.5–10 %.

**Against the only measured warp cost this project has:** `--wsub` under BF6 combat with the 4090 at 98.8 %
gives `gpu (warp) ≈ 3–4 ms` per tick (`measured`, cited `docs/research/UPLOAD_OFFLOAD_MASTER_PLAN.md:28-29`).
Adding 0.42–0.56 ms is **+11 % to +19 %** on that. On the objective's actual workload — `ball_zoo` at
1920×1080 on an otherwise idle 4090 — the fused warp's cost is **`unmeasured`**, so the same 0.42–0.56 ms
could be a small fraction or could be a doubling. This is the honest state of the M3 estimate and it is why
the first milestone (§6.5) measures exactly this before anything else moves.

### 6.3 Barrier census (derived by hand from `SeamGraph::compile()`, `seam_graph.hpp:343-448`)

Internal fields are `imageLoad`/`imageStore` in `VK_IMAGE_LAYOUT_GENERAL` throughout (no filtering is needed
— every internal read is at the invocation's own integer coord), so there is **no layout churn** on them.
Imported grids/reals are sampled in `SHADER_READ_ONLY_OPTIMAL`.

| Pass | Barriers emitted before it | Why |
|---|---|---|
| 1 `mv_fetch_guided` | 2 | acquire `mv_grid`, acquire `cur_real` (imported, first read) |
| 2 `inertia_restrict` | 2 | RAW `mv_cond`; acquire `persist_grid`. (`mv_grid` read is *covered* — same scope+layout as pass 1's, `seam_graph.hpp:362-370` — so **no** barrier. Its own write is RMW ⇒ WAW skipped, 399-403.) |
| 3 `bg_reclaim` | 2 | RAW `mv_cond`; acquire `prev_real`. (`cur_real` covered.) |
| 4 `phase_anchor` | 3 | RAW `mv_fwd`; acquire `mvb_grid`; **WAW `mv_cond`** (writes without reading → also the dominance warning of §3.5) |
| 5 `ambig_arbitrate` | 3 | RAW `mv_cond`; acquire `cand_grid`; acquire `sad_grid` |
| 6 `vblend_predict` | 2 | RAW `mv_cond`; acquire `mvt_grid` |
| 7 `core_sample` | 1 | RAW `mv_samp`. (`prev_real`/`cur_real` covered.) |
| 8 `compose` | 2 | RAW `samp_B`; RAW `d_pixel`. (`prev`/`cur`/`sad` covered.) |
| | **17 total** | across **8 passes**, **1 command buffer**, **1 submit**, **1 fence** |

Against today: the default `wap_warp_present` path records 1 buffer-memory barrier for the mass counter and
1 image barrier for the `GENERAL→TRANSFER_SRC` blit (the `--afill` and FPS-overlay `VkMemoryBarrier`s at
`present.cpp:1165` / `:1188` are default-off). So **+15 barriers per tick**, all derived, all precise-mask.

**Kill criterion 3 is satisfied by construction:** no per-stage fence (`sg.execute()` records into the
existing `cmdBridge`, and the existing single `vkWaitForFences(fBridge)` is untouched), no host round-trip
(every stage boundary is a device-local image), and `ALL_COMMANDS` is never emitted — SG builds each
barrier from the declared producer/consumer stage+access only (`seam_graph.hpp:518-542`, and the explicit
prohibition at 38-39).

**The real cost of the 17 barriers is not the API call, it is serialisation:** 8 compute→compute
dependencies mean the eight dispatches cannot overlap. The bandwidth figure in §6.2 already assumes no
overlap, so it is not double-counted; dispatch launch overhead adds an estimated **~5 µs × 8 = 40 µs/tick**
(`unmeasured`).

### 6.4 CPU record cost

Today: 4 `vkCmd*` calls for the warp (bind pipeline, bind descriptor set, push 232 B, dispatch —
`present.cpp:1145-1152`); measured whole-tick record term `rec ≈ 0.03 ms`
(`measured`, `UPLOAD_OFFLOAD_MASTER_PLAN.md:29`).
This candidate: 8 passes × 4 calls = 32, plus ≤ 8 `vkCmdPipelineBarrier2` calls carrying 17 barriers.
At a conservative 1 µs per `vkCmd*` (`unmeasured`) that is **≈ +40 µs/tick**, roughly doubling `rec` to an
estimated 0.07 ms — about **1.7 % of the 4.17 ms tick at 240 Hz**.

One real defect must be fixed for this to hold: `SeamGraph::execute()` heap-allocates a
`std::vector<VkImageMemoryBarrier2>` **per pass, per call** (`seam_graph.hpp:458-459`). At 240 Hz × 8 passes
that is 1,920 allocations/s on the present thread. Extension §A.3 pre-builds those vectors inside
`CompiledPass` at compile time so `execute()` allocates nothing.

### 6.5 First measurable milestone on this rig — **B1, "the two-pass identity split"**

The smallest change that puts this candidate's single largest risk on an instrument, before any layer moves.

*Change:* split today's `wap_warp.comp` into exactly **two** SG passes with the shipping default set
otherwise untouched — `core_sample` (writes `samp_B` RGBA32F + `d_pixel` R32F) and `compose_st`
(`base_b_track` + `pin_screen_static` + `pin_stasis`, storing RGBA8) — and move the *entire remaining
shader body* into a third pass `legacy_tail` that writes a resource nothing live reads, so `compile()`
culls it. Registry, per-layer UBOs and `--dump-flags` are **not** in B1.

*Acceptance, all on the operator's rig, 1920×1080:*
1. **M4:** `--qdump+` replay record → the T6/S6 CPU-reference warp → byte-diff vs the pre-split binary on
   the same record. **Byte-identical required.** This is the one place where the fp32-intermediate question
   is answered by an instrument instead of by argument, and it is answerable *today*, before MOTION_TRUTH's
   T4-T5 exist.
2. **M3:** `tools/ball_zoo.ps1` at 60 fps source, 240 Hz panel, `--csv`, 60 s, **two runs per side**
   (DI-3, run-to-run `r` reported): present cadence vs the E1 baseline (240 presents/s; 28,799 presents/120 s,
   `RESTRUCTURE_PLAN` G1), plus the `--wsub` `gpu` and `rec` segments — the first number that will tell us
   whether §6.2's 0.42–0.56 ms estimate is right, and by how much.
3. **M2b:** `--sg-dump` shows 3 declared passes, 2 live, and **0** dispatches/barriers attributed to
   `legacy_tail`.
4. **Build-green** on MSVC 19.44 / CMake 4.4.1 / Ninja 1.13.2 / SDK 1.4.357.0, and the adopted
   `test_seam_graph.cpp` still passes 122/122 with byte-identical `dump()` output.

*Decision rule:* if (1) fails, the fp32-intermediate assumption is wrong and this candidate is in trouble
(§8). If (2) shows the added GPU term exceeding the run-to-run spread by more than the tick has room for,
the mitigation ladder of §8 is entered *before* B2 (the six-node conditioning split), not after.

Subsequent stages (not part of the first milestone): **B2** the COND split into six nodes + the barrier
census check; **B3** the registry, per-layer UBOs, `--dump-flags` and the UI refactor + the M2a "add a
trivial layer" `git diff --stat` exercise; **B4** the dominance warning enabled as a hard error, and the
phase-anchor/bg-reclaim decision of §4.3 taken under M1.

---

## 7. What it drops

**Dropped outright:**
- The 57-field push block (`wap_warp.comp:78-134`, 232 bytes — the last field `bg_reclaim` sits at offset
  228, `present.cpp:1139`). The core's push becomes 4 bytes.
- The magnitude-encoded gates: `mv_guided = 1.0 + sim`, `mv_edge_snap = variant + sim`,
  `single_track ∈ {0,1.0,2.0}`, `bg_reclaim` carrying both enablement and strength. Enablement becomes
  graph membership; parameters become typed UBO fields.
- `result` as a shared mutable that any code region may assign. No pass can write another slot's resource.
- The "armed but shadowed" upstream single-track gates (`wa_eff` collapse 678, pure-`cur` `blend_result`
  692-695, the forced-warp selection, the inert commit/ts-smooth) — the shader keeps them deliberately
  (1315-1317); here they are unrepresentable.
- The hand-written `vkCmdPipelineBarrier` calls inside `wap_warp_present` (`present.cpp:1165`, `:1188`) and
  the in-place RMW pattern they guard: `--afill` and the FPS overlay become declared `POST/CHAIN` nodes
  reading `result` and writing `result_post`.
- `--blend-solo` as a shader branch (1331-1333): it becomes a graph substitution of the COMPOSE stage
  (`compose := debug_a_track | debug_b_track`), which is what a diagnostic override *is*.
- `WapPipe`'s single 14-binding / one-SPIR-V layout (`src/warp_blend/warp_blend.hpp`): one pipeline per
  live node instead.

**Not dropped, moved:** every other layer family in `wap_warp.comp` (occlusion, fill-div, rescue, commit,
appear, matte + crescent/travel/contour/obj-crescent/member-commit, onepos, commit-default, multicand,
ts-smooth, extrap, cam-lead, band-xfade, bg-snap, disoccl-hardpick, mv-edge-snap, predict-p2) becomes a node
in one of the slots of §1.2, absent from the default graph and hosted for A/B.

**Explicitly kept:** `OpticalFlowPipeline`, the FgContext C/F/P thread model and its SPSC rings, the E1
`init_*` functions and ownership structs, the host bridges, the DDA/WGC capture identity, the present bridge
and pacing, `fg_quality_scorer` + `prep_zoo_sequence.py` as the photometric fixture.

**New runtime dependencies: none** (kill criterion 6). `seam_graph.hpp` is a single header over
`<vulkan/vulkan.h>` and the C++ standard library.

---

## 8. My honest weakest point

**The sample-packet crossing.** `samp_B` (RGBA32F) and `d_pixel` (R32F) exist for exactly one reason: to
carry, through DRAM, values that the fused shader holds in registers for free. That is
**31.64 + 7.91 = 39.55 MiB written and read back = 79.1 MiB/tick**, roughly a quarter of my whole budget,
buying zero arithmetic. And it is the one place where M4 and M3 pull directly against each other:

- **fp32 (chosen):** byte-identity is *provable* — the value stored and the value loaded are the same IEEE
  fp32 the fused shader would have kept — but it is the single largest line in §6.2.
- **fp16 (RGBA16F/R16F):** halves that line to 39.6 MiB/tick, and breaks the proof. Half has ~11 bits of
  mantissa, so a value near 1.0 acquires an absolute error up to ≈ 2.4 × 10⁻⁴ against a final RGBA8 LSB of
  1/255 ≈ 3.92 × 10⁻³. A pixel flips a byte when the exact value lies within that error of a half-LSB
  boundary — roughly `2 × 2.4e-4 / 3.92e-3 ≈ 12 %` of pixels are close enough for a flip to be *possible*
  and, assuming uniform placement, about half of those flip: **≈ 6 % of pixels off by ±1 LSB**, giving
  `MSE ≈ 0.06/255² = 9.2 × 10⁻⁷` ⇒ **PSNR ≈ 60.3 dB** — sitting *on* M4's 60 dB threshold rather than
  comfortably above it, and not byte-identical. (`estimated`, uniform-distribution assumption stated.)

So the candidate is committed to fp32, and therefore committed to the bandwidth. Whether that is affordable
is `unmeasured` on the objective's actual workload: the only warp cost this project has measured
(`gpu ≈ 3–4 ms`) was taken under BF6 combat on a saturated 4090, not on an idle rig running `ball_zoo`.
**I do not know today's per-tick warp cost on the M3 workload, and my +0.42–0.56 ms is therefore a number
without a denominator.** That is why B1 (§6.5) measures the two-pass split *before* anything else.

The mitigation ladder, in order, if B1's M3 result is bad:
1. Drop `mv_fwd`'s materialisation by re-basing phase-anchor on `mv_cond` — saves 31.6 MiB/tick and fixes
   the §0 bug, but it is a product change and must go through M1 first.
2. Fuse `core_sample` and `compose` into one pass whenever the COMPOSE base is sample-local (which the
   shipping default's `base_b_track` is) — saves the entire 79.1 MiB crossing.
3. Generalise (2): tag element-wise passes in the registry and let `compile()` concatenate adjacent ones
   into a single dispatch (SG-FUSE).

**And I have to name what that ladder means:** steps 2 and 3 are Angle A's move. If the bandwidth verdict
is bad, this candidate's remedy is to become progressively less distinguishable from a fused design that
keeps the registry — which is an honest admission that **Angle B's modularity is purchased with bandwidth,
and the exchange rate is not yet known.** The structural wins (the dominance check, automatic culling of the
shadowed tail, the 1-parameter core, the 2-file layer) survive fusion; the "every layer is its own
dispatch" property does not.

*Second-order weak points, stated so they are not discovered later:* (a) the ≥2-node DAMP composition is
numerically order-dependent in the last ULP (§3.4) — declared, not hidden; (b) four SG extensions (§A) are
required and one of them changes the `record` callback signature, so the adoption of `apps/minimal_fg` into
the repo is not a pure copy; (c) `resolve_config`'s dependency cascades (`src/cli/cli.cpp:204-224`) must
become registry-driven or a layer's `requires_` will drift from its cascade — that is real work this
document scopes but does not detail.

---

## Appendix A — Required `SeamGraph` extensions (all in `src/seam/seam_graph.hpp` after adoption)

All four are small, and none may change `dump()`'s bytes (the 122-check golden test asserts determinism,
`seam_graph.hpp:40-41`).

**A.1 — Dominance warnings.** Add `std::vector<std::string> warnings;` to `Compiled` and a `dump_warnings()`
method (never `dump()`). In the WRITES loop's `else if (s.has_producer && !read_here)` branch
(`:411-417`) — the branch that is entered exactly when a pass overwrites a resource it does not read —
push one warning naming the writer, the resource and the dominated producer. **≈ 10 lines.** This is the
mechanised form of kill criterion 2 (§3.5).

**A.2 — `optional_write` + resource liveness.** Add `bool optional` to `Access` and, after culling, compute
resource liveness (a resource is live iff a live pass reads it, or it is a marked output). Pass a per-pass
`uint32_t live_writes` mask to the record callback, which the pass turns into a specialization constant
(`kStoreA` in §2.1). Requires widening the callback to
`std::function<void(VkCommandBuffer, uint32_t)>` — an API change; the golden test does not exercise
`execute()` (stated at `:20-21`), so it stays green. **≈ 25 lines.**

**A.3 — Zero-allocation `execute()`.** The per-pass `std::vector<VkImageMemoryBarrier2>` built inside
`execute()` (`:458-459`) is static once compiled; build it into `CompiledPass` at compile time and have
`execute()` only patch in the `VkImage` handles. Removes ~1,920 heap allocations/s from the present thread
at 240 Hz. **≈ 15 lines.**

**A.4 — `add_pass(const LayerDesc&)`.** An overload that derives `reads`/`writes` from a layer's constexpr
descriptor instead of a hand-written `std::vector<Access>` at the call site. This is what makes M2a = 2
files: without it, every new layer also edits `fg_graph.cpp`. **≈ 20 lines.**

## Appendix B — Trust-tier index (every number in this document)

| Claim | Tier | Source |
|---|---|---|
| `wap_warp.comp` is 1,336 lines; push block lines 78-134 = 57 fields, last field at offset 228 ⇒ 232 bytes | measured (read) | the file; `src/present/present.cpp:1139` |
| bg-reclaim's result is discarded by phase-anchor under the shipping default | measured (read) | `wap_warp.comp:344, 394, 410` + defaults at `cli.hpp:470,566,409` |
| Shipping defaults for the eight layers, and the `igpu_field` cascade that turns off bg-snap/band-xfade | measured (read) | `src/cli/cli.hpp:120-128,428,446,470,485,492,566,577,588,592,617,624`; `src/cli/cli.cpp:207-212` |
| `seam_graph.hpp` is 549 lines; culling at 266-301; coverage rule at 362-370; RMW WAW skip at 399-403; per-pass vector alloc at 458-459 | measured (read) | the file |
| Warp GPU cost `≈ 3–4 ms/tick`, upload `≈ 10 ms`, `rec ≈ 0.03 ms` — BF6 combat, 4090 at 98.8 % | **measured** (cited) | `docs/research/UPLOAD_OFFLOAD_MASTER_PLAN.md:28-29`; `docs/BENCHMARKS.md:83-89` |
| E1 baseline cadence: 240 presents/s, 28,799 presents/120 s | measured (cited) | A0 §1 M3 / `RESTRUCTURE_PLAN` G1 |
| Image byte sizes at 1920×1080 and the grid sizes at 240×135 | estimated (arithmetic over read extents/formats) | §6 |
| 87.01 MiB resident added; 300–350 MiB/tick traffic; 13–15× the fused baseline | estimated | §6.1, §6.2 |
| 0.42–0.56 ms/tick added GPU time | estimated (65–75 % of 1,008 GB/s assumed) | §6.2 |
| 17 barriers / 8 passes / 1 submit / 1 fence | estimated (hand-derived from the `compile()` source) | §6.3 |
| +40 µs/tick CPU record; ~5 µs dispatch launch overhead | unmeasured | §6.3, §6.4 |
| fp16 intermediates ⇒ ~6 % of pixels ±1 LSB ⇒ PSNR ≈ 60.3 dB | estimated (uniform-distribution assumption stated) | §8 |
| Achievable fraction of the 4090's 1,008 GB/s on this rig | **unmeasured** | §6.2 |
| Today's warp GPU cost on `ball_zoo` (idle 4090, the M3 workload) | **unmeasured** — the denominator §8 names | §8 |
