# CANDIDATE A — `FUSED-VARIANT` : one compute pass, compiled layer variants

> AAP pass A1, isolated DESIGNER, **ANGLE A** ("one fused pass, compiled variants"). Written against
> `A0_FROZEN_OBJECTIVE.md` + `DESIGNER_BRIEF.md` + the files they cite, all read first-hand
> (2026-09-02). Every number carries a trust tier: **`measured`** (I read it off a file/instrument in
> this session), **`estimated`** (arithmetic on measured inputs, stated), **`unmeasured`** (needs a run
> on the rig — named, never guessed at a value).
>
> **The bet in one sentence:** keep the generation core as ONE compute dispatch; make a layer a
> *compile-time* participant (a specialization constant that the driver dead-code-eliminates) writing
> only into a *typed, ranked slot* of the core — never into the store — so a disabled layer costs
> exactly zero instructions, zero bytes, zero barriers, and adding one costs zero new images.

---

## 0. What I verified first-hand (the facts this design stands on)

| Fact | Where | Tier |
|---|---|---|
| Push block = 57 shader fields (`cam_lead` is a `vec2`) = **58 floats / 232 B**; `pcr.size=232`, 8 B headroom under the 256 B device limit | `shaders/wap_warp.comp:77–135`; `src/warp_blend/warp_blend.cpp:117` | measured |
| The core needs 4 of them: `residual_ceil, improvement_frac, agreement_threshold, t` | `wap_warp.comp:78–81` | measured |
| One dispatch: `vkCmdDispatch(cmdBridge,(WW_warp+7)/8,(WH_warp+7)/8,1)`; 14 descriptor bindings, all always bound (placeholder views when off) | `present.cpp:1152`; `warp_blend.cpp:101–154` | measured |
| Shipping defaults on this rig: `mv_guided=true, mv_sim=0.10, inertia=true/0.50, phase_anchor=true, bg_reclaim=4.0, ambig=true, vblend=true/0.6/0.5, single_track=true, st_no_stasis=false, stasis=true/0.50, gme=true, bidir=true, occl_thresh=1.5, soft_gate=true, commit_thresh=0.08, commit_default=true, onepos=true, matte=false, ts_smooth=0, mc_on=false, camera_twarp=false, mv_edge_snap=0` | `src/cli/cli.hpp:16,112–141,388–624` | measured |
| `bg_snap` and `band_xfade` are `true`/`1.0` in the struct but **cascade OFF** because `igpu_field=false` by default (and cascade again with no iGPU convert path) → they are **not** in the rig's default-affecting set | `cli.hpp:98`; `cli.cpp:207–212`; `core_init.cpp:393–399` | measured |
| Under `single_track=1.0` the pre-store override at `1321–1328` discards everything written between 531 and 1320; the surviving default-affecting layers are the ones mutating `mv` **before** line 521, plus `stasis` folded into `w_s` | `wap_warp.comp:1315–1332` | measured |
| 257 unique CLI flag literals in `cli.cpp` vs 173 `flag:` entries in `ui/src/main.js` | `grep -o '"--[a-z0-9-]*"' \| sort -u \| wc -l` = 257; `grep -c 'flag: "'` = 173 | measured |
| The mass-counter block (2 buffer barriers + a 4-byte `vkCmdCopyBuffer`) is recorded **unconditionally every tick**, although `matte=false` by default | `present.cpp:1200–1215` (no enclosing `if`) | measured |
| Default post-dispatch barrier set per tick (`ts_smooth=0`, `afill=false`, overlay off): **2 buffer + 3 image barriers**, each its own `vkCmdPipelineBarrier` call | `present.cpp:1200–1219` | measured (static read) |
| SG derives **image** barriers only (`VkImageMemoryBarrier2`); it has no buffer-resource concept | `seam_graph.hpp:88–105, 393–420` | measured |
| SG culling is backward reachability from `mark_output`; an unreachable pass is dropped entirely | `seam_graph.hpp:266–306` | measured |
| Shaders are compiled offline: `glslc --target-env=vulkan1.2 -O` → `.spv` → a `constexpr` array header, one `pfg_spv(...)` line per shader | `CMakeLists.txt:30–43,61` | measured |
| `--wsub` already splits the warp tick into `up / rec / gpu / prs` EMAs — the CPU-record-cost instrument exists | `present.cpp:850–859, 1237–1238, 2801–2803` | measured |

---

## 1. Units + placement in the PhyriadFG tree

New tree (nothing existing is deleted in the milestone build — the old `wap_warp` path stays and is the
A/B reference, selected by `--fg-core` / `--no-fg-core`):

```
F:\Phyriad\projects\PhyriadFG\
  src/fg/
    core/
      seam_graph.hpp            ADOPTED verbatim from apps/minimal_fg/include/minimal_fg/seam_graph.hpp
                                (549 lines) + its golden test test/test_seam_graph.cpp (436 lines,
                                122 checks) moved to tests/. NO edit in the milestone; the buffer
                                extension of §6.4 is a later, separately-gated change.
      fg_core_contract.hpp      CoreParams (the 5-parameter uniform block), CorePush{float t},
                                SlotId enum, LayerId enum, VariantKey (uint64 spec-const mask).
      layer_registry.hpp/.cpp   the constexpr table built from the generated headers; the per-slot
                                rank-ordered lists; `--layer-dump` (prints slot → ordered layer list).
      variant_cache.hpp/.cpp    VariantKey -> VkPipeline. Owns ONE VkPipelineCache persisted to
                                %LOCALAPPDATA%\PhyriadFG\pipeline.cache; a background compile thread;
                                atomic pipeline swap at a tick boundary.
      fg_graph.cpp              builds the SeamGraph for a tick: declare images, add the upload pass
                                (per-pair), the fg_core pass, the blit pass; mark_output(bridge).
    layers/
      LAYERS.txt                the registration list — ONE line per layer, order fixes spec-const ids
      mv_guided.layer     inertia_gate.layer   phase_anchor.layer   bg_reclaim.layer
      ambig.layer         vblend.layer         single_track.layer   screen_static.layer
      stasis.layer        (then, ported one at a time: occl.layer, commit.layer, onepos.layer,
                           commit_default.layer, matte.layer, crescent.layer, travel.layer,
                           contour.layer, obj_crescent.layer, bg_snap.layer, band_xfade.layer,
                           ts_smooth.layer, multicand.layer, cam_lead.layer, mv_edge_snap.layer,
                           extrap.layer, blend_solo.layer(diagnostic variant), …)
  shaders/
    fg_core.comp              THE fused pass. Bindings + CoreParams + the fixed slot skeleton +
                              compose() + the single imageStore. Target size ~200 lines
                              (estimated: today's main() minus the layer bodies).
    fg_slots.glsl             #included by fg_core.comp; GENERATED (the rank-ordered slot bodies).
    fg_layer_params.glsl      #included by fg_core.comp; GENERATED (the LayerParams UBO).
  scripts/
    gen_layers.py             Python 3.13, standard library only (no numpy needed) — .layer -> the
                              four generated artifacts. Wired as a CMake custom command ahead of
                              pfg_spv("${_sh}/fg_core.comp" ...).
  build gen/
    fg_layer_params.glsl  fg_layer_params.hpp  fg_layer_flags.inc  fg_layer_ui.json
```

Reused unchanged (objective §3 "MUST reuse, not rewrite"): `OpticalFlowPipeline`, the FgContext thread
model (C/F/P, SPSC rings — `src/core/fg_context.hpp`), the E1 `init_*` functions + ownership structs
(`src/core/app_init.hpp`), `fg_quality_scorer`, `prep_zoo_sequence.py`. `init_wap()`
(`warp_blend/warp_blend_init.cpp`) gains a sibling `init_fg_core()` filling a new `FgCoreInit` struct
in `app_init.hpp` — the same ownership-struct grouping rule the E1 pass established.

**Passes per tick: 1 compute dispatch** (`fg_core`), plus the pre-existing blit, plus the per-pair
upload pass (culled by SG on non-advance ticks). Same as today. That is the bet.

---

## 2. The core contract

`fg_core.comp` consumes exactly **5 parameters** (objective M2c target ≤ 8; today 57):

```glsl
// set = 0, binding = 20 — UNIFORM_BUFFER_DYNAMIC (the ring slot is a bind-time dynamic offset,
// NOT a shader parameter).
layout(set = 0, binding = 20, std140) uniform CoreParams {
    float residual_ceil;          // 1  Gate 1 (a): sad_best < residual_ceil
    float improvement_frac;       // 2  Gate 1 (b): sad_zero*(1-f) > sad_best
    float agreement_threshold;    // 3  Gate 2: d_tile < thr
    uint  mv_source;              // 4  0 = fwd field (binding 2) | 1 = bwd field negated (binding 5)
                                  //    | 2 = external field (binding 12). THE MV-source selector.
} C;

layout(push_constant) uniform CorePush {
    float t;                      // 5  temporal phase, set by the presenter per output-clock tick
} P;                              //    4 bytes. Was 232.
```

- **Where `t` enters:** the push block, one float, rewritten every tick — the only per-tick host write
  in the record path.
- **Where the MV source enters:** `C.mv_source`, an enum resolved once per configuration change, not
  per tick. The core's fetch is `mv_raw = fetch_mv(C.mv_source, uv)`; a *layer* may later adjust `mv`
  (slot `MV_ADJUST`) but may not change the source.
- **Images the core reads:** binding 0 `u_prev_real` (RGBA8, full-res), 1 `u_cur_real` (RGBA8,
  full-res), 2 `u_mv_fwd` (RG16F, W/8×H/8), 3 `u_sad` (RG16F, W/8×H/8, `.r=sad_best .g=sad_zero`).
- **Image the core writes:** binding 4 `u_output` (rgba8 `writeonly image2D`), **one `imageStore`, one
  source expression** — `compose()`'s return value. This is the structural repair.
- **Everything else** (bindings 5–13 today: bwd MV, dissidence fwd/bwd, persistence, mass SSBO,
  candidates, iGPU field, MV target, prev output) belongs to LAYERS, not to the core. The descriptor
  set layout keeps all of them (fixed layout, placeholder views when off — today's proven trick,
  `warp_blend.cpp:101–115`); what changes is that a disabled layer's *resource and its producer pass*
  are culled by SG (§4.3), so nothing is uploaded, dispatched, or barriered for it.

The core body is exactly today's core math, unchanged in form and in operation order:

```
uv        = (coord + 0.5) / out_size                     [slot SAMPLE_BASE may adjust uv]
mv_raw    = fetch_mv(C.mv_source, uv)                    [slot MV_FETCH selects the fetch function]
mv        = apply(MV_ADJUST, mv_raw)                     [ranked slot]
mv_eff    = apply(SAMPLE_MV, mv)                         [ranked slot — sample-offset MV only]
sad       = texture(u_sad, uv).xy
A_samp    = texture(u_prev_real, uv - mv_eff*t      /size)     // wap_warp.comp:519
B_samp    = texture(u_cur_real,  uv + mv_eff*(1-t)  /size)     // wap_warp.comp:520
d_pixel   = length(A_samp.rgb - B_samp.rgb)
s_d[lid] = valid ? d_pixel : 0; barrier(); d_tile = sum/64          // UNCONDITIONAL, top-level
gates     = { sad.r < C.residual_ceil, sad.g*(1-C.improvement_frac) > sad.r, d_tile < C.agreement_threshold }
wa        = apply(WEIGHT, 1.0 - t)                       [ranked slot]
base      = select(BASE, wa*A_samp + (1-wa)*B_samp)      [rank-max selection]
fallback  = select(FALLBACK, (1-t)*prev[uv] + t*cur[uv]) [rank-max selection]
w         = apply(GATE, gates)                           [ranked slot]
result    = mix(fallback, base, w)
result    = fold(RESULT_MIX, result)                     [ranked slot, convex]
if (valid) imageStore(u_output, coord, result);          [ONE store, ONE source]
```

The `barrier()` stays where it is today and for the same reason (`wap_warp.comp:20–26`: the MV is
bilinear-sampled so the tile decision is not workgroup-uniform; the barrier must be at the top level of
`main()`). **Contract rule L-7: no layer body may contain `barrier()`, `groupMemoryBarrier()`, or an
early `return`.** A layer needing a tile reduction declares `@reduce` and the generator hoists it into
the core's single existing reduction (adding a `shared` array slot); this is the only mechanism by
which a layer can obtain workgroup-scope data, and it is checked at generation time.

---

## 3. The layer contract

### 3.1 One file per layer

```
# src/fg/layers/bg_reclaim.layer
@layer   bg_reclaim
@slot    MV_ADJUST
@rank    20                                   # the DECLARED position inside the slot
@needs   gme_model                            # a PREDICATE dependency; unsatisfied = build error
@reads   prev_real, cur_real                  # feeds the SG read set (§4.3)
@enable  bool  on = true   cli:"--bg-reclaim|--no-bg-reclaim"  ui:switch-off  name:"BG reclaim"
         desc:"TILE-scale gravity fix: a background-content tile carrying an object-like MV is damped
               toward the winning hypothesis (gme model, or zero for screen-static overlay pixels)."
@param   float strength = 4.0  range:[0,4] step:0.5
         cli:"--bg-reclaim-strength"  ui:number  name:"BG reclaim: strength"
         desc:"Damp weight scale. 1 = soft LSFG-style decay, 4 = hard snap (shipping default)."
@glsl
vec2 LAYER_APPLY(vec2 mv, in CoreState s) {
    // verbatim wap_warp.comp:358-395, with pc.bg_reclaim -> L.bg_reclaim.strength
    const vec2  model_mv = gme_model_mv(s.uv);
    const float nonconf  = smoothstep(2.0, 6.0, length(mv - model_mv));
    if (nonconf <= 0.0) return mv;
    ...
    return mix(mv, target, clamp(L.bg_reclaim.strength * nonconf * evid, 0.0, 1.0));
}
```

`LAYERS.txt` gains **one line**: `bg_reclaim`. That line fixes the layer's specialization-constant id
(its 0-based index in the file), so ids are stable across builds and a removed layer leaves a
`@retired` tombstone rather than renumbering everything.

### 3.2 What a layer may read and write

| | |
|---|---|
| **May read** | the immutable `CoreState s` (uv, uv0, coord, out_size, t, sad, mv_raw, mv, A_samp, B_samp, d_pixel, d_tile, the three gate booleans — whichever are defined *at its slot's position*), any binding listed in its `@reads`, its own `LayerParams` sub-struct, and any PREDICATE whose name it lists in `@needs`. |
| **May write** | **only its slot's variable**, as the return value of `LAYER_APPLY`. The generator emits the layer body as a function; it has no lvalue access to core state at all. |
| **May never** | touch `u_output`, call `barrier()`, `discard`, `return` early from `main`, declare `shared` storage, or reference another layer's params. Enforced by the generator (a grep-level check on the `@glsl` body) plus the GLSL type system (core state arrives `in`, by value). |

### 3.3 Slots — the fixed extension points

| Slot | Type it produces | Reduction over N enabled layers | Default-set members |
|---|---|---|---|
| `PREDICATE` | named scalars/bools, **pure** | none (no mutation) — trivially order-independent | `inertia_p`, `stasis`, `gme_model` |
| `SAMPLE_BASE` | `vec2 uv` | ranked fold | (none: `cam_lead` off) |
| `MV_FETCH` | selects the fetch fn | **rank-max selection** (commutative) | `mv_guided` (rank 10) |
| `MV_ADJUST` | `vec2 mv` | ranked fold | `inertia_gate`(10) → `bg_reclaim`(20) → `phase_anchor`(30) → `ambig`(40) |
| `SAMPLE_MV` | `vec2 mv_eff` | ranked fold | `vblend`(10) |
| `WEIGHT` | `float wa` | ranked fold | (`onepos`, `commit` — shadowed under the default, see §4.2) |
| `BASE` | `vec4` base + `vec4` fallback | **rank-max selection** (commutative) | `single_track`(100) |
| `GATE` | `float w ∈ [0,1]` | ranked fold | (`soft_gate`, `commit_default` — shadowed) |
| `RESULT_MIX` | `(vec4 target, float weight)` | ranked convex fold | `stasis_present`(10) → `screen_static`(20) |
| ~~`FINAL`~~ | **does not exist** | — | — |

### 3.4 How order is handled (kill criterion 2, head-on)

Kill criterion 2 forbids *"the meaning of the output depends on the ORDER in which layers write
(last-writer-wins)"*. This candidate attacks it on three axes, and I state honestly which one is a
guarantee and which is a discipline:

1. **HARD GUARANTEE — single-writer output.** There is no slot after `compose()`. The store's source
   expression is fixed in `fg_core.comp` and is not reachable from any layer. Today's defect is
   literally `result = <something>` executed by six different sites (`1136–1185`, `1191`, `1321–1328`,
   `1332`) with the last one winning; that construct is unrepresentable here. A layer that wants the
   output to be `X` must claim `BASE` (a rank-max selection) or contribute to `RESULT_MIX` (a convex
   mix with a declared weight) — it cannot assert.
2. **HARD GUARANTEE — order-independence in the selection slots.** `MV_FETCH` and `BASE` resolve by
   `max(rank)` over the enabled claimants. `max` is commutative and associative, so those slots are
   order-independent by construction, not by convention.
3. **DECLARED ORDER — the fold slots.** `MV_ADJUST`, `SAMPLE_MV`, `WEIGHT`, `GATE`, `RESULT_MIX`
   compose by a left fold in **rank order**, where rank is an integer in the manifest, duplicate ranks
   in one slot are a **generation-time error**, and the resolved order is printed by `--layer-dump`
   and emitted into the generated `fg_slots.glsl` as a comment. Kill criterion 2's own escape clause —
   *"or has an EXPLICIT declared order that is part of the contract"* — is what this uses.
   Additionally `RESULT_MIX` is constrained to be **convex**: a contributor returns
   `(target, weight ∈ [0,1])` and the fold is `r = mix(r, target, weight)`. A contributor therefore
   cannot discard the accumulated value except at exactly `weight == 1`, and every layer carries the
   obligation **`weight == 0 ⇒ identity`** (checked by the M4 replay, §7).

**The honest residual:** two layers in the same fold slot still do not commute in general. I am not
claiming commutativity; I am claiming (a) the output has one writer, (b) the order is a declared,
diffable, dumpable integer rather than an emergent property of shader line numbers, and (c) the
composition in the result slot is monotone/non-discarding. That is strictly the escape clause, not the
stronger property.

### 3.5 Where layer parameters live, and how enable/disable works

**Enable = a specialization constant.** `gen_layers.py` emits, into `fg_slots.glsl`:

```glsl
layout(constant_id = 17) const bool kL_bg_reclaim = false;   // id from LAYERS.txt position
...
// slot MV_ADJUST — declared rank order: inertia_gate(10) bg_reclaim(20) phase_anchor(30) ambig(40)
if (kL_inertia_gate) mv = inertia_gate_apply(mv, s);
if (kL_bg_reclaim)   mv = bg_reclaim_apply(mv, s);
if (kL_phase_anchor) mv = phase_anchor_apply(mv, s);
if (kL_ambig)        mv = ambig_apply(mv, s);
```

A `false` specialization constant is folded at pipeline-creation time and the branch is dead-code
eliminated by the driver: **no instruction, no register, no texture tap, no bandwidth.** This is the
angle's core mechanism and it is what makes objective M2b (GPU work of a disabled layer = 0) hold *by
construction* rather than by a runtime `if`.

**Numeric parameters = ONE always-declared UBO.** Specialization constants gate *code*, not
*declarations*, so per-layer UBO blocks cannot be conditionally declared without a preprocessor
variant — and preprocessor variants would multiply the *descriptor set layout*, which is host-side and
must stay fixed. The resolution: one generated block, always declared, always bound, read only from
inside a live branch:

```glsl
layout(set = 0, binding = 21, std140) uniform LayerParams {
    struct { float sim;                          } mv_guided;
    struct { float thresh;                       } inertia_gate;
    struct { float on;                           } phase_anchor;
    struct { float strength;                     } bg_reclaim;
    struct { float on;                           } ambig;
    struct { float t0, strength; uint exact;     } vblend;
    struct { uint  mode;                         } single_track;
    struct { float thresh;                       } stasis;
    /* … one sub-struct per registered layer … */
} L;
```

`UNIFORM_BUFFER_DYNAMIC`, ring depth 2 (matching the `--async-present` bridge slot count,
`app_init.hpp:306–335`); the host writes slot `n&1` before `vkQueueSubmit`, which is exactly the case
Vulkan's host-write ordering guarantee covers — **no barrier needed, and SG (image-only) does not need
extending for it.** A dead layer's members are never loaded because the load lives inside the DCE'd
branch, so an always-declared UBO costs a disabled layer nothing.

**Runtime toggle = a pipeline swap, not a recompile-in-the-hot-path.** The enabled set is a 64-bit
`VariantKey`; `variant_cache` maps key → `VkPipeline`. Cold key ⇒ compile on a background thread with
the persisted `VkPipelineCache`, keep presenting with the current variant, atomically swap at a tick
boundary. Two keys are pre-warmed at startup: the resolved default, and its `warp_light` sibling (§8).

### 3.6 Layer-to-CLI/UI in one hop (brief item 5)

`gen_layers.py` (Python 3.13, stdlib only — inside the envelope) emits four artifacts from the same
manifest:

| Artifact | Replaces the hand-synced site |
|---|---|
| `gen/fg_layer_params.glsl` | the shader's push-block declaration (`wap_warp.comp:77–135`) |
| `gen/fg_layer_params.hpp` | the `Config` fields + `constexpr` spec-const ids + the defaults |
| `gen/fg_layer_flags.inc` | the `cli.cpp` parse cases + the `--help` text + the cascade table |
| `gen/fg_layer_ui.json` | the `ui/src/main.js` 173-entry hard-coded array |

`ui/src/main.js` stops declaring flags and instead fetches `fg_layer_ui.json` (emitted next to the
binary; the Tauri front-end reads it via the existing `src-tauri` command surface). The **257-vs-173
drift becomes structurally impossible** for layer flags: one manifest, four consumers, generated. The
84 CLI-only flags that are *not* layer flags (device selection, capture backend, telemetry, pacing)
stay hand-written in `cli.cpp` — they are outside this search's scope and I do not claim to fix them.

**Honest scope note:** a per-layer manifest with codegen is machinery Angle C makes its *thesis*. Here
it is subordinate plumbing — the bet is the fused pass + spec-const compile-out, and the manifest is
the minimum needed to satisfy brief item 5 and objective M2a (≤ 2 files to add a layer). A designer
whose bet *is* the table would push it much further (deriving the SG registration, the cascade graph,
and the record format from it); I derive only the four artifacts above.

---

## 4. The default-set mapping (M4's A/B target)

Every entry below is a *port*, not a redesign: the GLSL body moves verbatim into the `.layer` file with
`pc.X` rewritten to `L.<layer>.X`, and the rank is chosen to reproduce today's source order exactly.

| Layer | Today | Slot / rank | `.layer` file | Default value |
|---|---|---|---|---|
| **mv-guided** | `wap_warp.comp:324` (`guided_mv`, 179–202: 5 full-res taps + 1–2 MV taps) | `MV_FETCH` / 10 | `mv_guided.layer` | on, `sim = 0.10` |
| **inertia gate** | `332` (persistence read) + `332–334` (the fast-corner refusal) | `PREDICATE` (`inertia_p`) + `MV_ADJUST` / 10 | `inertia_gate.layer` | on, `thresh = 0.50` |
| **bg-reclaim** | `358–395` (4 warp taps + 2 same-uv taps, inside `nonconf>0`) | `MV_ADJUST` / 20 | `bg_reclaim.layer` | on, `strength = 4.0` |
| **phase-anchor** | `397–410` (`mv = mix(mv_fwd, -mv_bwd, w_b)`) | `MV_ADJUST` / 30 | `phase_anchor.layer` | on (needs `occl_thresh>0`; `bidir=true`) |
| **ambig** | `426–453` (runner-up arbitration vs the gme referee) | `MV_ADJUST` / 40 | `ambig.layer` | on (needs `gme=true`) |
| **vblend** | `507–514` (`mv_fwd_eff = mix(mv, mv_target_s, vb_w)`) | `SAMPLE_MV` / 10 | `vblend.layer` | on, `t0=0.6, strength=0.5, exact=0` |
| **single-track** | `677` (`wa_eff = 0`), `692–695` (fallback base = pure `cur`), the forced-warp selection, and `1321–1322` (`result = B_samp`) | `BASE` / 100 — a rank-max **selection** claiming BOTH the base (`B_samp`) and the fallback (`cur[uv]`) | `single_track.layer` | on, `mode = 1` |
| **screen-static** | `1323–1328` (`w_s` ratio → `mix(B_samp, cur0, w_s)`) | `RESULT_MIX` / 20 | `screen_static.layer` | on (armed by `single_track.mode==1`) |
| **stasis** | `1049` (the `stz <= thresh` predicate), `1191` (the override), `1327` (`w_s = 1` saturation) | `PREDICATE` (`stasis`) + `RESULT_MIX` / 10 (`stasis_present`) | `stasis.layer` | on, `thresh = 0.50` |

### 4.1 Why the mapping is byte-exact under the shipping default (the M4 argument)

Under `single_track=1.0` the `BASE` selection makes `base = 0*A_samp + 1*B_samp = B_samp` and the
fallback `cur[uv]`; the forced-warp gate makes `w = 1`, so `result = mix(cur[uv], B_samp, 1) = B_samp`
— exactly today's line 1321. Then the `RESULT_MIX` fold, in declared rank order:

```
r = B_samp
r = mix(r, cur[uv], stasis ? 1.0 : 0.0)                                  // stasis_present, rank 10
r = mix(r, cur0,    stasis ? 1.0 : smoothstep(1.2,3.0,(d_pixel+.02)/(d_zero+.02)))  // screen_static, rank 20
```

- `stasis == true`: `mix(B,cur,1) = cur`, then `mix(cur,cur0,1) = cur0 = cur[uv]`.
  Today: `w_s = 1` → `mix(B_samp, cur0, 1) = cur0`. **Same value.**
- `stasis == false`: `mix(B,cur,0) = B_samp` (GLSL `mix(a,b,0) = a + 0*(b-a) = a` exactly for finite
  colors), then `mix(B_samp, cur0, w_s)`. Today: identical expression. **Same value, same operation
  order, same rounding.**

`A_samp`/`B_samp` are unchanged because `mv` is produced by the same four adjusters in the same order
and `mv_eff` by the same vblend tilt. Everything today computes between 531 and 1320 (occlusion
round-trip, commit family, matte, onepos, soft/hard selection) is *discarded* today; here it is
**compiled out**, since those layers' spec constants are false in the default variant — a strictly
smaller instruction stream producing the identical bytes.

### 4.2 The layers that are ON in `Config` but shadowed under the default

`soft_gate`, `commit_thresh`, `commit_default`, `onepos`, `bidir`/`occl_thresh`, and the whole matte
family are enabled in `cli.hpp` but their results are discarded by the `single_track` override
(`wap_warp.comp:1315–1317` says so in the shader's own comment). Two treatments, and the choice is a
declared operator decision, not mine to make silently:

- **(a) faithful** — include them in the default variant with their today ranks. Byte-identical
  trivially; the variant carries dead-by-value code the driver *may* partly eliminate (its results
  feed `WEIGHT`/`GATE` slots that `single_track`'s rank-max `BASE` claim overrides). This is what the
  A-M1 milestone builds, because M4 is a veto and I will not risk it.
- **(b) honest** — exclude them from the default variant (their outputs are provably unread), making
  the shipping variant genuinely ~9 layers. Byte-identical *if* the exclusion is provably output-dead;
  `--layer-dump` prints `shadowed-by: single_track(BASE,100)` for each, and the A-M1 byte-diff is the
  proof obligation. This is where the fused-variant design pays: the shadowed set becomes *visible*
  and *removable*, which is exactly what "parche tras parche" made impossible.

### 4.3 What a disabled layer costs (objective M2b = 0)

- **Instructions/registers/bandwidth:** zero. The spec constant is false; the branch is DCE'd.
- **Dispatches:** zero — there is only ever one dispatch, and it does not grow.
- **Barriers:** zero, and *negative* for some layers. The `fg_core` SG pass declares
  `reads = core_reads ∪ ⋃_{enabled layers} layer.@reads`. With `matte` disabled, the mass SSBO and the
  dissidence images leave the read set, so their producer/upload passes fail SG's backward
  reachability (`seam_graph.hpp:266–306`) and are **culled**: the 2 buffer barriers + the 4-byte
  `vkCmdCopyBuffer` that `present.cpp:1200–1215` records *unconditionally today* disappear. Same for
  `ambig` off (the `hC2_*` candidate bridge upload, 259,200 B/pair) and `vblend` off (the `wapMVTA`
  upload, 129,600 B/pair).
- **The descriptor binding stays** (fixed set layout, a 1×1 placeholder view — today's proven
  technique). A placeholder view has no per-tick cost and SG never sees it.

---

## 5. Budget arithmetic at 1920×1080

Grid: 2,073,600 px; dispatch `(1920+7)/8 × (1080+7)/8 = 240 × 135 = 32,400` workgroups of 64 threads.

### 5.1 Intermediate images introduced by this candidate

**Zero. 0 bytes.** The image set is exactly today's set. That is the entire point of the fused angle and
it is the one budget line where this candidate is unambiguously strongest. For reference, the images
that exist (all measured from format × extent):

| Image | Extent | Format | Bytes |
|---|---|---|---|
| `wapPrevA` / `wapCurA` | 1920×1080 | RGBA8 | 8,294,400 each |
| `wapOutA` | 1920×1080 | RGBA8 | 8,294,400 |
| `bridge_img` | 1920×1080 | BGRA8 | 8,294,400 |
| `wapMVA` / `wapSADA` / `wapMVBA` / `wapMVTA` | 240×135 | RG16F | 129,600 each |
| `wapC2A` | 240×135 | RGBA16F | 259,200 |
| `wapDISA` / `wapDISBA` / `wapPERA` | 240×135 | R8 | 32,400 each |

*(tier: estimated — arithmetic on the formats/extents read from `warp_blend.cpp` and `app_init.hpp`;
the 129,600 B figure independently corroborates the "~130KB each at 1080p" comment at
`app_init.hpp:137–139`.)*

### 5.2 New device memory

Only the two uniform buffers:

- `CoreParams`: 16 B (std140-padded) × ring 2 = **32 B**.
- `LayerParams`: today's 57 push fields minus the 5 core = 52 layer scalars; grouped into ~24 std140
  sub-structs rounded to 16 B ⇒ ~384–512 B per slot × ring 2 = **~1 KiB**.

**Total new allocation ≈ 1.1 KiB** *(estimated: field count × std140 rounding; not yet measured with a
real `sizeof`)*. Against ~41 MiB of existing warp-path images, this is 0.003 %.

### 5.3 Per-tick DRAM traffic (unchanged from today, by construction)

| Term | Bytes | Note |
|---|---|---|
| `imageStore` → `wapOutA` | 8,294,400 | compulsory |
| Blit read `wapOutA` + write `bridge_img` | 16,588,800 | compulsory, pre-existing |
| Sampled `prev`/`cur` compulsory footprint | 16,588,800 | lower bound (every texel once) |
| Sampled `prev`/`cur` with zero cache reuse | up to 74,649,600 | upper bound at ~9 taps/px in the default path (5 in `guided_mv` + 6 in `bg_reclaim` (gated) + 2 warp + `cur0`/`prev[uv]`, with heavy overlap) |
| MV + SAD | 259,200 | compulsory |
| `LayerParams` UBO | ~512 | workgroup-uniform, broadcast |

**Estimate: 42–100 MB/tick.** At 240 ticks/s (the E1 G1 baseline cadence) that is **10–24 GB/s**, i.e.
**1–2.4 % of the RTX 4090's 1,008 GB/s vendor-spec bandwidth** *(tier: estimated for the traffic;
unmeasured (vendor spec) for the 1,008 GB/s)*.

> **I have to state the uncomfortable consequence of my own arithmetic:** the fused pass's headline
> justification — "one pass for bandwidth" — is **weak on this rig at this resolution**. At ~2 % of
> peak bandwidth, a staged design could afford several full-res RGBA8 intermediates (7.91 MB each,
> +3.8 GB/s per read-write round trip at 240 Hz) before bandwidth became the binding constraint. The
> defensible benefits of Angle A are therefore **not** bandwidth: they are (1) zero new allocations and
> zero image-lifetime management, (2) M2b = 0 holding by construction with no culling subtlety,
> (3) zero added `vkQueueSubmit` (the master plan's cited 24–36 µs each) and zero added fences, and
> (4) no runtime branch divergence inside the warp. I would rather say this than sell a number I just
> computed against myself.

### 5.4 Barriers per frame

| | Today (default: `ts_smooth=0`, `afill=false`, overlay off) | This candidate (default variant) |
|---|---|---|
| Post-dispatch, hand-written | 2 buffer + 3 image = **5 `vkCmdPipelineBarrier` calls** (`present.cpp:1200–1219`) *(measured, static read)* | 0 hand-written |
| SG-derived per tick | n/a (no SG) | RAW `prev,cur,mv,sad → fg_core` (4 image barriers, batched into **1** `vkCmdPipelineBarrier2`), RAW `out → blit` + acquire `bridge` (2, batched into **1**) = **6 image barriers in 2 calls** *(estimated from `seam_graph.hpp`'s derivation rules; will be read off `--sg-dump` at A-M1)* |
| Mass-counter buffer barriers | 2 per tick, unconditional | **0** — culled with `matte` off (§4.3) |
| Fences | 1 (`fBridge`) | 1 — unchanged. **No per-stage fence, no host round-trip, no `ALL_COMMANDS`** (kill criterion 3 satisfied: SG emits precise masks only, `seam_graph.hpp:38–39`). |

### 5.5 CPU record cost per tick

Removed: assembling the 232-B push struct from 58 conditional expressions
(`present.cpp:1103–1150`) every tick, and `vkCmdPushConstants(…, 232)`.
Added: `vkCmdPushConstants(…, 4)`, one `vkCmdBindDescriptorSets` dynamic offset, and `SG::execute`
walking a **pre-`compile()`d** `Compiled` (compiled once per variant/topology change, cached — never
per tick) to issue 2 `vkCmdPipelineBarrier2` calls.

**Estimated net: −1 to +3 µs/tick, i.e. inside noise.** *(unmeasured — the instrument already exists:
`--wsub` prints `rec:` as an EMA of the reset→end window, `present.cpp:1237–1238`. A-M1 reports it for
both sides.)* No number is claimed until that runs.

---

## 6. First measurable milestone on the rig — **A-M1**

Buildable in one stage, green at every step, using **only instruments that exist today** (it does not
depend on the unbuilt MOTION_TRUTH instrument).

**Build:** `fg_core.comp` + the 9 `.layer` files of §4 + `gen_layers.py` + `variant_cache` + SG adopted
verbatim. New flag `--fg-core` selects the new pipeline; **the default stays the old `wap_warp` path**,
so one binary does the A/B and nothing regresses if A-M1 fails.

**Run** (1920×1080, `tools/ball_zoo.ps1` at 60 fps source, RTX 4090, 240 Hz panel):

1. **M4 (veto).** `--no-async-present --qdump C:\qd\old 240` then `--fg-core --no-async-present
   --qdump C:\qd\new 240` (`--qdump` fires only with `--no-async-present`, `present.cpp:1315`).
   Byte-diff the 240 output frames with a numpy-only script.
   **Gate: 100 % byte-identical.** Anything else is disqualifying under M4 unless every difference is
   a documented FP-order change reaching PSNR ≥ 60 dB.
2. **M2a.** The "add a trivial layer" exercise (a no-op `RESULT_MIX` layer with `weight = 0`):
   `git diff --stat` must show **2 files** (`src/fg/layers/noop.layer`, `src/fg/layers/LAYERS.txt`).
3. **M2b.** `--sg-dump` with `--no-ambig --no-vblend`: the candidate/MV-target upload passes must be
   **absent** from the pass list and contribute **0 barriers**.
4. **M2c.** Header count on `fg_core_contract.hpp`: **5** core parameters.
5. **M3.** `--csv` + `--wsub`, 60 s × 2 runs per side (DI-3), reporting presents/s vs the
   `RESTRUCTURE_PLAN` G1 baseline (240 presents/s on the 240 Hz panel, 28,799 presents/120 s) and the
   `rec:` / `gpu:` EMAs. **Gate: no regression beyond run-to-run spread.**
6. **Occupancy (the weakest-point probe, §8).** Nsight Compute on both variants: registers/thread and
   achieved occupancy of `fg_core` (default variant) vs `wap_warp`. This is the number that decides
   whether Angle A scales as layers are re-added.
7. **M1.** Analysis only at this stage, per A0's gating note: the MV path and the sampling form are
   preserved literally (§4.1), so M1 equivalence is *expected*; the `--qdump+` record carries
   prev/next/live + t + MV + SAD + gme, and the per-tick state this candidate adds is the 64-bit
   variant key + the ~512 B `LayerParams` blob — strictly smaller than the 232 B push block it
   replaces plus 8 B, and deterministically derivable from `Config`. **Kill criterion 1 does not
   bite:** the record format extension is `+8 B key, +512 B params, −232 B push`.

---

## 7. What it drops

1. **The 232-byte / 57-field push block as the layer mechanism.** Replaced by 4 B push + 2 UBOs.
2. **The pre-store override chain** (`wap_warp.comp:1187–1332`): `if (stasis) result = …`,
   `if (single_track) result = …`, `if (blend_solo) result = …`. No layer can write the store. This is
   the defect under repair and it is dropped structurally, not by convention.
3. **The four hand-synced sites** for layer flags (Config struct, parse, help, UI array) — generated
   from the manifest. The 84 non-layer CLI-only flags are **not** fixed (declared, not hidden).
4. **`ui/src/main.js`'s hard-coded 173-entry flag array** for layer flags → `fg_layer_ui.json`.
5. **`--blend-solo` as a runtime push field** → a diagnostic *variant* (`kDiag_blend_solo`). Honest
   cost: switching it now costs a pipeline compile instead of a push-constant write. It is a
   measurement tool used between runs, so this is acceptable — but it *is* a behaviour change and I
   name it rather than bury it.
6. **Per-tick adaptive layer shedding at push granularity.** Today `--load-governor`'s `warp_light`
   zeroes `mc_on` / `bg_snap` / `band_xfade` / `vblend` per tick with no recompile
   (`present.cpp:1069, 1137–1140`). Under spec constants that becomes a variant swap. **Mitigation
   (concrete):** pre-warm the `{default, default & ~warp_light_mask}` pair at startup — 2 cached
   pipelines, swap at a tick boundary, zero runtime compile. Cost: 2× pipeline objects on that axis.
7. **Nothing about the seam** is dropped: SG is adopted, not replaced; 1 fence per frame-in-flight, no
   host round-trip, precise masks only.

---

## 8. My honest weakest point

**A fused pass has no cost isolation. Every layer shares one register budget and one occupancy class,
so adding a layer is a NON-LOCAL cost that lands on every pixel — including pixels the layer never
touches.**

`wap_warp.comp` is 1,336 lines with a large live state at the composition point (`A_samp`, `B_samp`,
`d_pixel`, `d_tile`, `dis_fwd`, `dis_bwd`, `bgs_w`, `bx_w`, `mv`, `mv_fwd`, `mv_fwd_eff`, `wa`,
`warp_result`, `blend_result`, `result`, plus 64 floats of `shared`). If a future layer pushes the
variant over a register-count cliff, the SM's occupancy drops for the *whole* dispatch and M3 regresses
for the default set even though the default set did not change. A staged design (an intermediate image
between stages) pays bandwidth — which §5.3 shows is ~2 % utilised here — and buys back exactly this
isolation, plus per-stage timestamp attribution that a fused pass cannot give.

I cannot bound this without measuring: **registers/thread and achieved occupancy per variant are
`unmeasured`**, which is why step 6 of A-M1 exists and why I would treat a register regression as a
gate, not a footnote. If it bites, the honest fallback is a *hybrid* — keep the core fused and split
only the register-heaviest family (the matte/occlusion group) into a second pass — and at that point
this candidate has partially conceded to Angle B.

**Second weakest point (named, smaller):** variant explosion and the toggle hitch. K registered layers
give 2^K keys (K ≈ 24 today ⇒ 1.7 × 10⁷ theoretical); only the reachable handful is ever compiled, but
a cold key mid-session costs a driver compile of a ~1,300-line shader. Cost `unmeasured`; mitigated by
the persisted `VkPipelineCache`, a background compile thread, and the tick-boundary swap — but the
first toggle after a driver update or a cache wipe will hitch, and I will not pretend otherwise.

**Third (accepted, not solved):** §3.4's fold slots are order-*declared*, not order-*free*.

---

## 9. Trust-tier index

| Claim | Tier |
|---|---|
| Every fact in §0, all file/line citations, all CLI defaults, the 257/173 counts, the 5-barrier post-dispatch set, the unconditional mass-counter block | **measured** (read in this session) |
| Image byte sizes (§5.1), new-allocation ~1.1 KiB (§5.2), per-tick DRAM 42–100 MB and 10–24 GB/s (§5.3), the 6-barrier SG prediction (§5.4), `fg_core.comp` ≈ 200 lines | **estimated** (arithmetic on measured inputs; every input named) |
| RTX 4090 = 1,008 GB/s | **unmeasured** (vendor spec, not read off this rig) |
| CPU record delta −1…+3 µs, GPU time of either variant, registers/occupancy per variant, cold-variant compile time, `--wsub rec/gpu` values, presents/s of the new path | **unmeasured** — each has a named instrument and a step in A-M1; no value is asserted |
| M4 byte-identity of the mapping (§4.1) | **estimated** — argued expression-by-expression against the read source; the proof is A-M1 step 1, and until it runs it is an argument, not a result |

---
*Made with my soul - Swately <3*
