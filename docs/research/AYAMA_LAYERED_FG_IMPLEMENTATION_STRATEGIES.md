# PhyriadFG Tri-Tier Gap-Aware FG — Implementation Strategies

Status: `0.1.0-experimental` · Type: Planning / design (Explanation, per
[FDP §3](../canon/FORMAL_DOCUMENT_PROTOCOL.md)) · Version anchor:
[`version.hpp:35`](../../framework/_meta/include/phyriad/version.hpp)

> **Normative keywords.** The key words **MUST**, **MUST NOT**, **REQUIRED**,
> **SHALL**, **SHALL NOT**, **SHOULD**, **SHOULD NOT**, **RECOMMENDED**, **MAY**,
> and **OPTIONAL** are to be interpreted as in BCP 14
> ([RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) /
> [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html)) when, and only when,
> in all capitals.

This document is the **code-level HOW** for the layered PhyriadFG architecture: the
exact seams, shader sketches, POD schemas, control wiring, and the staged build
order with per-stage gates. It is written against the binding **ARCHITECTURE
SPINE** (the anti-drift contract) and uses the spine's fixed interface / stage /
signal names verbatim (spine §10). Its sibling is the
[`AYAMA_LAYERED_FG_MASTER_PLAN.md`](AYAMA_LAYERED_FG_MASTER_PLAN.md) (the WHY +
the FR gate); both share the spine's section list and MUST NOT diverge on any
name. Where this document and the master plan disagree, the spine is authoritative
and the change is made there first (spine §10 anti-drift rule; D-16).

**Scope.** Seam-level strategies for: the `ofpA` revival as STAGE-A-FILL; the
iGPU STAGE-G1 field pass; the `RaFieldSet` POD schema; the GapSignal assembly;
the latency/cadence reconciliation; the adaptive back-off; the test-field
validation; and the phased build order.

**Non-scope.** The conceptual rationale, the prior-art landscape, and the FR
qualification gate (Q1–Q8) — those are the master plan's. The pillar internals of
`gpu`/`behavior`/`stigmergy` — those are their reference docs. This is an app-level
plan inside [`apps/render_assistant/`](../../apps/render_assistant/); it introduces
**no new pillar** (spine §6 D-12).

> **Honesty regime (declared once, per FDP §4.8).** Every capability claim below
> carries a status bucket — `shipping` / `designed` / `measured` / `conjectured`
> (FDP §4.2). `shipping` = present in the code at the cited line today; `designed`
> = spec in this document, no code yet; `measured` = a number taken this session;
> `conjectured` = stated, not proven. The verification of each file:line was done
> first-hand this session at one anchor; downstream mentions link, they do not
> re-attest.

---

## 1. The 3-tier holarchy — the integration seams (file:line)

The three GPU holons coordinate **only through host-resident shared fields**
(D-10 stigmergy), never by shipping each other RGBA frames (spine §1; NG-2). The
seams this document modifies:

| Tier | Device | Holon role (spine §1) | The seam this doc touches | Bucket |
|---|---|---|---|---|
| **G (substrate)** | iGPU | Stigmergic substrate provider; adds **STAGE-G1** field pass | New pipeline in the `use_igpu_convert` block [`main.cpp:3085-3103`](../../apps/render_assistant/src/main.cpp); dispatch after the convert dispatch [`main.cpp:3698`](../../apps/render_assistant/src/main.cpp) on `G.q2` [`main.cpp:3707`](../../apps/render_assistant/src/main.cpp); buffer alloc at the `hostRP` site [`main.cpp:2716-2728`](../../apps/render_assistant/src/main.cpp) | convert = `shipping`; field pass = `designed` |
| **B (base)** | 1080 Ti | Heavy MC interpolation **base MC layer** (existing OFP + holon tail) | OFP `record_optical_flow` [`main.cpp:4898`](../../apps/render_assistant/src/main.cpp); CPU tail `object_repair` call site [`main.cpp:4626`](../../apps/render_assistant/src/main.cpp); gme offload `--gme-gpu` default-ON [`main.cpp:566`](../../apps/render_assistant/src/main.cpp) | `shipping` |
| **A (embellish)** | 4090 | Carries game + capture + present; adds **STAGE-A-FILL** | Primary seam: between the warp dispatch [`main.cpp:5558`](../../apps/render_assistant/src/main.cpp) and the `TRANSFER_SRC` barrier+blit [`main.cpp:5576-5578`](../../apps/render_assistant/src/main.cpp); dormant `ofpA` decl [`main.cpp:2453`](../../apps/render_assistant/src/main.cpp), disabled [`main.cpp:3101`](../../apps/render_assistant/src/main.cpp) | capture+present = `shipping`; STAGE-A-FILL = `designed` |

**The disable to lift, and why it MUST NOT be naively un-set.** `pfg_enabled=false`
at [`main.cpp:3101`](../../apps/render_assistant/src/main.cpp) frees A on the
default iGPU-convert path. The dormant `ofpA` (`OpticalFlowPipeline` declared at
[`main.cpp:2453`](../../apps/render_assistant/src/main.cpp), `init` at
[`main.cpp:3069`](../../apps/render_assistant/src/main.cpp)) flows the **original
captures** — `ofpA.record_optical_flow(cmdA_fg, AframeA[prv_f].view,
AframeA[cur_f].view, ...)` at
[`main.cpp:4979`](../../apps/render_assistant/src/main.cpp), where `AframeA[2]`
are the A-resident ping-pong originals
([`main.cpp:2454`](../../apps/render_assistant/src/main.cpp),
[`main.cpp:2904-2905`](../../apps/render_assistant/src/main.cpp)). That is the
**wrong input** for STAGE-A-FILL (the alternative "un-set `pfg_enabled`" is
rejected, spine §7). STAGE-A-FILL re-points to the **warp output** `wapOutA` (in
`GENERAL` layout at [`main.cpp:5579`](../../apps/render_assistant/src/main.cpp),
written by the warp dispatch at
[`main.cpp:5558`](../../apps/render_assistant/src/main.cpp)) — a net-new A-local
data path that needs no B→A frame plane (`designed`).

---

## 2. STAGE-A-FILL — the `ofpA` revival as a 4090 gap-aware FILL/REFINE pass, matte-decoupled

### 2.1 The exact seam

STAGE-A-FILL is a **single extra compute dispatch on `A.q`** inserted into the
existing `cmdBridge` command buffer between the warp dispatch and the present
blit. The verified seam (all `shipping` anchors, the new dispatch `designed`):

```
[main.cpp:5558]  vkCmdDispatch(cmdBridge, ...);          // warp → wapOutA (GENERAL)   [shipping]
   ── INSERT STAGE-A-FILL here ──                        // refine wapOutA in place    [designed]
   (a) barrier wapOutA SHADER_WRITE→SHADER_READ_WRITE    (the fill samples its own input)
   (b) vkCmdDispatch(cmdBridge, fillPipeA, ...)          // reads wapOutA + hFIELD_a + GapSignal pc
   (c) barrier wapOutA SHADER_WRITE→TRANSFER_READ        (= the existing [main.cpp:5576] barrier)
[main.cpp:5576]  img_barrier(... wapOutA GENERAL→TRANSFER_SRC ...);                    [shipping]
[main.cpp:5578]  vkCmdBlitImage(... wapOutA → bridge_img ...);                         [shipping]
```

This is a refine **in place on `wapOutA`** before the existing blit — it adds
**no new image, no host bounce, no extra submit**. The present path's
zero-host-bounce property is `shipping` (the comment "No host bounce, no G in the
present path (lowest latency)" at
[`main.cpp:5379`](../../apps/render_assistant/src/main.cpp)); STAGE-A-FILL
preserves it by living inside the same `A.q` submit that the warp+blit already use
(the warp/blit run in ONE `A.q` submit, [`main.cpp:5584-5589`](../../apps/render_assistant/src/main.cpp)).

### 2.2 Decoupling from the matte (NG-5)

STAGE-A-FILL **MUST NOT** revive the matte two-layer composite. The matte's
execution doubles the figure — it `mix`es an object-layer warp against a
background-layer warp at
[`wap_warp.comp:1404-1409`](../../apps/render_assistant/shaders/wap_warp.comp),
gated on `pc.matte_on > 0.5` at
[`wap_warp.comp:843`](../../apps/render_assistant/shaders/wap_warp.comp). That
doubling **is** the crossfade (spine §2, §7 NG-5). STAGE-A-FILL instead reads the
`hFIELD_a` boundary field (§3) directly and refines the **single** already-warped
`wapOutA` — no second layer, no composite. The matte gate
(`use_matte=cfg.matte&&use_gme`,
[`main.cpp:3353`](../../apps/render_assistant/src/main.cpp)) stays default-OFF and
is **independent** of STAGE-A-FILL; the fill MUST run correctly with `use_matte`
off.

### 2.3 The fill shader (`wap_fill.comp`, sketch — `designed`)

A new compute shader, NOT a modification of `wap_warp.comp` (keeps the byte-identical
warp A/B path intact; D-13). Binds `wapOutA` as a read-write storage image,
samples `hFIELD_a` and the GapSignal push-constants:

```glsl
// wap_fill.comp — STAGE-A-FILL. One dispatch on A.q, in-place on wapOutA. [designed]
layout(local_size_x=8, local_size_y=8) in;
layout(binding=0, rgba8) uniform image2D u_warp;     // wapOutA, read-write in place
layout(binding=1)        uniform sampler2D u_field;  // hFIELD_a: contour_dist + occ_class
layout(push_constant) uniform PC {
    float gap;          // GapSignal scalar in [0,1] — 0 = B gen fresh, 1 = B gen stale/skipped
    float phase;        // sync-clock phase (consumed, never owned — §5)
    float refresh_hz;   // parametric; NO magic 240 (D-11, NG-7)
    float fill_strength;// adaptive: shrunk by the §6 back-off
} pc;
void main() {
    const ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    const vec2  uv = (vec2(p) + 0.5) / vec2(imageSize(u_warp));
    const vec4  occ = texture(u_field, uv);           // .r=contour_dist .g=occ_class
    const vec4  cur = imageLoad(u_warp, p);
    // disocclusion band (occ_class==DISOCCL): the warp's revealed-region fill is least reliable.
    // STAGE-A-FILL refines ONLY where the field flags risk AND the gap signal says B is stale —
    // a boundary-aware edge-preserving refine, NOT a second interpolation (NG-3, refine-not-cascade).
    const float risk = field_disoccl_weight(occ.g) * occ.r;   // distance-weighted boundary risk
    const float w    = pc.fill_strength * pc.gap * risk;       // 0 when B is fresh OR field is interior
    if (w < 1e-3) return;                                      // byte-identical when nothing to do
    imageStore(u_warp, p, mix(cur, boundary_refine(u_warp, p, occ), w));
}
```

The `w < 1e-3` early-out gives the **off-proof**: when `gap==0` (B fresh) or the
field marks interior, STAGE-A-FILL leaves `wapOutA` byte-identical — the same
A/B-discipline the matte/subpel/disoccl-commit toggles already follow (e.g.
`disoccl_commit_on ≤ 0.5 → BYTE-IDENTICAL`,
[`wap_warp.comp:944`](../../apps/render_assistant/shaders/wap_warp.comp)).

**Latency property (binding, §5):** the refine samples `wapOutA` and its
spatial neighbours **only** — never a future frame. It adds sub-frame compute,
not a look-ahead frame (NG-3).

---

## 3. STAGE-G1 — the iGPU contour / occlusion / distance-field pass

### 3.1 Where it runs

STAGE-G1 is a **second per-source-frame compute dispatch on `G.q2`**, appended to
the existing convert+pack command buffer right after the convert dispatch at
[`main.cpp:3698`](../../apps/render_assistant/src/main.cpp). The iGPU is otherwise
**blocking-idle between source frames** (the convert is one dispatch then a
fence-wait, [`main.cpp:3698-3709`](../../apps/render_assistant/src/main.cpp)), so
STAGE-G1 consumes that idle. Its input is the converted `hostR[s]` RGBA8 (the
unpacked real slot, [`main.cpp:2352`](../../apps/render_assistant/src/main.cpp));
its output is the new `hostFIELD[s]` (§4). The pipeline is created alongside
`cpPipe`/`ubPipe`/`ugPipe` in the `use_igpu_convert` block
([`main.cpp:3085-3103`](../../apps/render_assistant/src/main.cpp)), one per
`kCapSlots` ([`main.cpp:2351`](../../apps/render_assistant/src/main.cpp)).

### 3.2 The relationship to STAGE-63 (the chamfer mechanism already exists)

The object-delimitation mechanism is **already proven in the codebase**: STAGE-63
casts a 2-pass integer chamfer **distance + feature** transform inward from the
closed silhouette contour over the filled silhouette — the chamfer body is at
[`main.cpp:4138-4249`](../../apps/render_assistant/src/main.cpp), integer weights
`kChamfOrtho=3`/`kChamfDiag=4` at
[`main.cpp:711-712`](../../apps/render_assistant/src/main.cpp), the interior depth
gate `kObjShapeDepthMin=3` at
[`main.cpp:713`](../../apps/render_assistant/src/main.cpp). That transform runs
**on the CPU F-thread today**, on the MV-derived object silhouette, and feeds
`object_repair` ([`main.cpp:4626`](../../apps/render_assistant/src/main.cpp)).

STAGE-G1 is the **image-derived analogue**: the same chamfer idea (distance from a
boundary + a feature class), but cast from **image-edge contours** on the iGPU at
sub-ms regular per-pixel cost, host-resident, available **before** B's MV exists.
It does **not** replace STAGE-63 — it produces a *sharper image-grounded prior*
the matte's MV-derived shapefield never had. This is the spine's revival of "the
matte's right idea (boundary-aware compositing) without its wrong execution"
(spine §2).

### 3.3 The field shader (`igpu_field.comp`, sketch — `designed`)

```glsl
// igpu_field.comp — STAGE-G1 on G.q2, after the convert dispatch. [designed]
layout(local_size_x=8, local_size_y=8) in;
layout(binding=0) uniform sampler2D u_rgb;            // hostR[s] converted RGBA8
layout(binding=1, rg8) uniform image2D u_field;       // hostFIELD[s]: RaFieldTexel
// Pass intent (single dispatch; a 2-pass chamfer is two dispatches if exactness needed):
//  1. edge response (Sobel/gradient magnitude on luma) → silhouette contour
//  2. occ_class: interior / disocclusion-candidate / occlusion-boundary from the contour sign
//  3. contour_dist: approximate chamfer distance inward (3-4 weights, mirrors kChamfOrtho/Diag)
void main() {
    const ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    const float g = gradient_mag_luma(u_rgb, p);          // image-edge response
    const uint  occ = classify_occ(u_rgb, p, g);          // 0 interior / 1 disoccl / 2 occl-bound
    const uint  d   = chamfer_approx_inward(u_rgb, p, g);  // ~kChamfOrtho/kChamfDiag weighting
    imageStore(u_field, p, vec4(float(d)/255.0, float(occ)/255.0, 0, 0));  // R8/RG8 packed
}
```

**Cost discipline (binding, D-11):** STAGE-G1's per-source-frame cost on `G.q2`
**MUST** be `measured` at the target source-fps before sizing, and the iGPU's
break-even at that fps **MUST** be re-confirmed — the iGPU's host-buffer-BW niche
is established for low-intensity work, but the specific STAGE-G1 cost is `designed`,
not measured (spine §1, P0 gate). The iGPU path is platform-sensitive (EMH import,
already gated on `have_igpu`,
[`main.cpp:2560`](../../apps/render_assistant/src/main.cpp); the broader gate
`use_igpu_convert`, [`main.cpp:2584`](../../apps/render_assistant/src/main.cpp));
STAGE-G1 inherits the same gate and the A-convert fallback path.

---

## 4. The stigmergic shared-field — `RaFieldSet` POD schema + the zero-copy host model

### 4.1 The host-bridge template it follows verbatim

`RaFieldSet` follows the existing `hostMV`/`hostSAD` host-bridge template
**verbatim** (the STAGE-41 warp-at-presenter bridge,
[`main.cpp:2360-2364`](../../apps/render_assistant/src/main.cpp)): a sysRAM buffer
`_aligned_malloc`'d once, EMH-imported on every device that touches it (G writes,
B and A import + read locally — exactly the `hostRP` alloc+import loop at
[`main.cpp:2716-2728`](../../apps/render_assistant/src/main.cpp), which imports the
same host pointer on G, B, and a device-local B copy). The buffers and their
imports (fixed names, spine §10):

| Buffer | Role | Template anchor |
|---|---|---|
| `hostFIELD[kCapSlots]` | the sysRAM field, one slot per cap-slot | mirrors `hostRP[kCapSlots]` [`main.cpp:2353`](../../apps/render_assistant/src/main.cpp), sized at the `hostRP` site [`main.cpp:2716-2728`](../../apps/render_assistant/src/main.cpp) |
| `hFIELD_g` | G import (writer) | mirrors `hRP_g[_s]` [`main.cpp:2354`](../../apps/render_assistant/src/main.cpp) |
| `hFIELD_b` | B import (reader, optional P4) | mirrors `hRP_b[_s]` |
| `hFIELD_a` | A import (reader, STAGE-A-FILL) | uploaded per pair-advance, mirrors the `up_imgA(wapMVA, hMV_a[gen].buf, ...)` path [`main.cpp:5323`](../../apps/render_assistant/src/main.cpp) |

### 4.2 The POD schema (binding — both docs use this identically, spine §10)

```cpp
// host-resident, per-cap-slot (kCapSlots = 4, main.cpp:2351), EMH-imported on
// G(write)/B(read)/A(read). Stride parametric, sized at start() (D-4). [designed]
struct RaFieldTexel {           // RG8: 2 B/texel (D-8 POD). 1 B/texel if occ folded into dist sign.
    uint8_t  contour_dist;      // STAGE-G1: approx chamfer distance from image-edge silhouette rim
    uint8_t  occ_class;         // STAGE-G1: 0=interior 1=disoccl-candidate 2=occlusion-boundary
};
```

- **G writes** `contour_dist` + `occ_class` per block from `hostR[s]` (§3).
- **B reads** (P4, optional) as an extra prior gating the gme dissidence mask /
  object clustering (the holon tail at
  [`main.cpp:4626`](../../apps/render_assistant/src/main.cpp)).
- **A reads** via a sampled image uploaded per pair-advance (mirror the `hMV_a`
  upload, [`main.cpp:5323`](../../apps/render_assistant/src/main.cpp)), sampled at
  the STAGE-A-FILL disocclusion-fill site (the divergence-only `fill_div` path at
  [`main.cpp:185`](../../apps/render_assistant/src/main.cpp) is the today-analogue
  STAGE-A-FILL supersedes).

### 4.3 Schema-hash — intraprocess, so none REQUIRED today

`RaFieldSet` stays **intraprocess** (sysRAM EMH across device handles, not
cross-process SHM). Per CANON principle 7 + D-8, a schema hash
(`schema::Hash128` baking `kPhyriadHalVersion` + `kDestructivePad`) is **REQUIRED
only if** the field ever crosses an IPC / SHM boundary. Within one process across
EMH-imported handles, **no** schema hash is required — the same constraint the
existing `hostMV`/`hostSAD` bridges meet today (they carry no hash and cross no
process). If a future stage ships the field cross-process, the hash becomes
mandatory at that seam (`designed`, conditional).

---

## 5. The GapSignal — how A learns which B frames are late/stale/skipped + at which phase

The base (B) **tells the fill (A) WHERE and WHEN** (the architecture's core
novelty, spine §3). The signal is assembled from primitives that **already exist**
— STAGE-A-FILL adds no new clock and no write-back into B's ring.

### 5.1 Staleness / skip (the temporal component)

The per-gen sequence machinery already publishes everything needed. F writes the
pair descriptors `f_pair_cseq_a[f_gen]` / `f_pair_span_a[f_gen]`
([`main.cpp:4732-4733`](../../apps/render_assistant/src/main.cpp)) **before** the
`f_seq.fetch_add(1)` ([`main.cpp:4735`](../../apps/render_assistant/src/main.cpp)),
the seq-cst pair ordering them for P. P records which gen it presents via
`p_presenting.store(fs - gen_back)`
([`main.cpp:6016`](../../apps/render_assistant/src/main.cpp), declared
[`main.cpp:3583`](../../apps/render_assistant/src/main.cpp)). When P out-runs F
(the HSR deficit), F's bounded lap-escape fires — `stat_lap.fetch_add(1)` at
[`main.cpp:4835`](../../apps/render_assistant/src/main.cpp) (declared
[`main.cpp:3551`](../../apps/render_assistant/src/main.cpp)), ceiling
`kRingGuardSpinMax=64` (~64 ms) at
[`main.cpp:4829`](../../apps/render_assistant/src/main.cpp).

**GapSignal definition (fixed name, spine §10):**

```
GapSignal.gen_age = f_seq.load() - p_presenting.load()   // how many gens behind P is presenting
GapSignal.stale   = stat_lap delta over the present window // B gen was pinned/skipped
GapSignal.gap     = saturate(f(gen_age, stale))           // the [0,1] scalar STAGE-A-FILL reads
```

`gap == 0` ⇒ B's gen is fresh ⇒ STAGE-A-FILL is a near-no-op (the `w < 1e-3`
early-out, §2.3). `gap → 1` ⇒ B's gen is stale/skipped ⇒ STAGE-A-FILL fills into
the deficit. (`designed`; the underlying counters are `shipping`.)

### 5.2 Phase to fill (the cadence component)

The fill phase is the existing WAP phase `t` (pushed per tick at
[`main.cpp:5557`](../../apps/render_assistant/src/main.cpp) into the warp
push-constants), and under `--sync-clock` the NCO-derived phase
`ph = clamp((content_clock − (pair_c − span)) / span, 0, 1)` at
[`main.cpp:6038-6043`](../../apps/render_assistant/src/main.cpp). STAGE-A-FILL
**consumes** this phase; it MUST NOT compute its own.

### 5.3 Where (the spatial component)

`hFIELD_a` (§4) tells A *where in the frame* the fill is risky — the disocclusion
bands — reusing the disocclusion logic presently gated off with the matte
(`pc.matte_on > 0.5`,
[`wap_warp.comp:843`](../../apps/render_assistant/shaders/wap_warp.comp)).

### 5.4 Binding read-only rule

The GapSignal is **read-only on A's present path**: A reads `f_seq` /
`p_presenting` / `stat_lap` / `hFIELD_a`; A **never** writes back into the B ring.
This preserves single-writer-per-slot (D-3 / D-6). The existing channels are
already seq-cst atomics (`f_seq`, `p_presenting`, `stat_lap`) — STAGE-A-FILL adds
**no new lock and no new atomic** on the hot path (D-3, D-5).

---

## 6. Latency + cadence reconciliation (refine-not-cascade; `--sync-clock` interplay)

### 6.1 REFINE, NOT CASCADE — binding (latency is the #1 risk)

STAGE-A-FILL is a **refine of the already-synthesized `wapOutA`** attached at the
verified seam (§2.1), adding only **sub-frame compute latency** (same `cmdBridge`,
same `fBridge` fence, no host bounce — the present path comment at
[`main.cpp:5379`](../../apps/render_assistant/src/main.cpp)). A cascade-interpolation
pass would add a **look-ahead frame** of delay; it is rejected (NG-3). The contract:
STAGE-A-FILL **MUST** be a refine/fill of `wapOutA`, **never** a second
interpolation stage needing a future frame. (`designed`; the seam's
zero-host-bounce property is `shipping`.)

### 6.2 `--sync-clock` owns the cadence; STAGE-A-FILL consumes it

`--sync-clock` (STAGE-90, default-ON, flag at
[`main.cpp:515`](../../apps/render_assistant/src/main.cpp)) owns the cadence: the
NCO accumulator `content_clock` + 2nd-order PLL frequency estimate `T_robust_ms`
(declared [`main.cpp:5749-5752`](../../apps/render_assistant/src/main.cpp)),
loop gains `kScFreqAlpha=0.05`
([`main.cpp:5758`](../../apps/render_assistant/src/main.cpp)),
`kScPhaseGain=0.10`
([`main.cpp:5764`](../../apps/render_assistant/src/main.cpp)), re-seat threshold
`kScReseatErr=4.0`
([`main.cpp:5769`](../../apps/render_assistant/src/main.cpp)). STAGE-A-FILL
consumes the phase (§5.2) and **MUST NOT** introduce its own clock (NG-6). It
masks B's cadence instability (the lap-escape ~64 ms pins) by refining the A frame
at the locked phase when B's gen is stale — i.e. STAGE-A-FILL is the
cadence-stability mechanism the operator named, **riding the existing PLL**.

### 6.3 The open non-stationarity validation (honest gap)

`--sync-clock` non-stationarity under camera-turns / combat is a **known un-closed
item** (the spine flags it; the flag comment claims "Halo-validated 2026-06-14:
slip ~0" for the stationary case at
[`main.cpp:516`](../../apps/render_assistant/src/main.cpp), but the non-stationary
case is not validated). STAGE-A-FILL's correctness under PLL lag **MUST** be
validated, not assumed (P2 gate). If the PLL lags, the design brief's
adaptive-gain / Kalman path is the escalation — but the current PLL comment
asserts "no Kalman filter needed" for steady tracking
([`main.cpp:5747-5748`](../../apps/render_assistant/src/main.cpp)); that assertion
holds only for the stationary case and is `conjectured` for the non-stationary one.

---

## 7. Adaptive-headroom control (`break_even_decide` + PressureScore → yield to the game)

D-20 graceful degradation is binding: STAGE-A-FILL **yields the 4090 to the game**.
The composition (spine §5):

### 7.1 Capability gate (first yield, `shipping` primitive)

STAGE-A-FILL is dispatched **only when** `break_even_decide` says it pays
([`GpuDescriptor.hpp:94-115`](../../framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp)).
The degenerate-input guard returns "never offload" on uncharacterized inputs
([`GpuDescriptor.hpp:102-104`](../../framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp)),
and the routing default is supervisor-only when uncharacterized
([`RoutingPolicy.hpp:63`](../../framework/holons/include_gpu/phyriad/holons/RoutingPolicy.hpp)).
**Honest note:** for an A-local refine (no cross-device transfer) the gate is
near-trivial — the link term vanishes — so the *binding* back-off lever is the
pressure feedback below, not the capability gate.

### 7.2 Pressure back-off (`designed` controller over a `shipping` primitive)

A consumer-side controller samples `PressureScore::level()`
([`PressureScore.hpp:94`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp);
bands `kLevelGreenMax=0.30f` / `kLevelYellowMax=0.70f` at
[`PressureScore.hpp:70-71`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp),
semantics green `<0.30` / yellow `[0.30,0.70)` / red `≥0.70` at
[`PressureScore.hpp:24-26`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp)).
Wiring: **yellow** → shrink `fill_strength` (the §2.3 push-constant) / raise the
`break_even_decide` threshold; **red** → drop STAGE-A-FILL entirely (`fill_strength=0`,
the §2.3 early-out makes it byte-identical), the game reclaims the 4090.

### 7.3 The HONEST GAP (binding — both docs state this exactly)

`PressureScore` measures **CPU-runtime** pressure (migrations / rings / tick
variance / jitter — the score combines normalized rates, e.g.
`kCoeffMigration=0.3f`,
[`PressureScore.hpp:57`](../../framework/behavior/include/phyriad/behavior/PressureScore.hpp)),
**not GPU-busy**. `GpuDescriptor` carries **no live utilization field** — it is a
*capability* record (FLOP/s + link BW, the inputs to `break_even_decide`,
[`GpuDescriptor.hpp:90-92`](../../framework/gpu/include/phyriad/gpu/GpuDescriptor.hpp)),
queried + measured-once. A faithful "back off when the **4090 is GPU-bound**"
needs a **net-new GPU-busy signal** absent from the pillars today. The live
`gpu(A:NN% B:NN% G:NN%)` PDH stat exists in the app (printed in the stat line,
the `stat_lap` read site neighbourhood at
[`main.cpp:6227`](../../apps/render_assistant/src/main.cpp)) and is the candidate
source, but it is a runtime PDH reading, not a pillar primitive. **Until that
GPU-pressure edge exists, "yield when the 4090 is GPU-bound" is `designed`, not
`shipping`** (spine §5).

---

## 8. Test-field validation plan (the held-out `fg_quality_scorer`)

STAGE-A-FILL is a correctness-preserving refine; a faster ghosted frame is
worthless (D-13). The oracle is the **already-built**
[`fg_quality_scorer`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md)
(`shipping`):

- **Crossfade regression guard.** STAGE-A-FILL exists to *avoid* re-introducing
  the matte's crossfade. The scorer's truth-less Mode (live-only) computes
  `crossfade_residual` + `double_edge_energy` of the live FG output — "how OUR
  live FG output is scored for crossfade without perturbing the present path"
  ([`fg_quality_scorer/README.md:35-41`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md)).
  STAGE-A-FILL **MUST NOT** raise `crossfade_residual` vs the warp-only baseline.
- **The PRIMARY artifact metric** is `dbl_edg_m` — motion-masked double-edge
  energy
  ([`fg_quality_scorer/README.md:117`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md),
  lower = better; read with `mcov`,
  [`fg_quality_scorer/README.md:118`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md)).
- **Flow-discrepancy** `flowdsc` (Mode A held-out)
  ([`fg_quality_scorer/README.md:121`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md))
  guards that STAGE-A-FILL does not worsen the motion estimate.
- **Held-out harness:** the continuous `sequence <prefix> <count>` mode
  ([`fg_quality_scorer/README.md:53`](../../framework/render/vulkan/bench/fg_quality_scorer/README.md))
  decimates a recorded capture into held-out triples by the canonical VFI
  protocol (drop the midpoint, score the synthesized one).

**Beat-LSFG measurement (P5)** follows D-14: a fair held-out A/B across the
approaches the architecture allows, reporting the loss region, with one source of
truth for any ratio ([`BENCHMARK_FAIRNESS.md`](BENCHMARK_FAIRNESS.md)). LSFG's
240 fps overlay is digitally uncapturable, so the only shared terrain is a
camera-captured no-reference comparison — that experiment is **not yet done**
([`FG_VFI_PRIOR_ART.md:417-419`](FG_VFI_PRIOR_ART.md)).

---

## 9. Staged build order — per-stage validation gates + rollback

Each phase: the non-regression gate runs **before** any "safe" claim; distinguish
"Not Run" (not built) from a real "Failed"; confirm own-vs-preexisting on any
failure (the no-regression-gate discipline). Each stage is **default-OFF behind
its own flag** (mirrors matte/subpel/disoccl-commit defaults, D-17) so rollback is
flipping the flag.

| Phase | Scope | Validation gate (binding) | Rollback |
|---|---|---|---|
| **P0 — STAGE-G1 producer** | Stand up `hostFIELD[kCapSlots]` + the `igpu_field.comp` pass on `G.q2` (no consumer). | Field bytes correct vs a CPU reference (D-13 oracle); STAGE-G1 cost **measured** at target source-fps + `gpu(G:..)` headroom confirmed (§3.3); no source-cadence regression. | `--no-igpu-field` flag off; G reverts to convert-only. |
| **P1 — STAGE-A-FILL refine-only** | Insert the `wap_fill.comp` dispatch at the seam (§2.1) reading `hFIELD_a`+`wapOutA`; NO gap fill yet (`gap` forced 0 ⇒ near-no-op). | Sub-frame latency only (no look-ahead) **measured**; `crossfade_residual` / `dbl_edg_m` show no held-out regression (§8); byte-identical when off. | `--no-afill` flag; the §2.3 early-out makes the dispatch a no-op even if compiled in. |
| **P2 — GapSignal** | Wire `gen_age`/`stat_lap` (§5) as the read-only GapSignal; STAGE-A-FILL fills the B-deficit gen at the sync-clock phase. | cons/s deficit visibly filled to panel Hz; lap-escape pins masked; NO new clock (NG-6); sync-clock lock preserved; **non-stationarity validated** (§6.3). | GapSignal gain → 0 reverts to P1 behaviour. |
| **P3 — adaptive back-off** | `PressureScore::level()` (+ the candidate `gpu(A)` PDH edge) drives shrink-on-yellow / drop-on-red (§7). | D-20: GPU-bound scene → `fill_strength` → 0, game frame budget never starved (**measured**); the GPU-busy-signal gap (§7.3) documented if the PDH edge is not first-class. | Controller disabled → STAGE-A-FILL runs at fixed strength (P2 behaviour). |
| **P4 — B-side field consume (optional)** | B reads `hostFIELD` as a prior gating gme dissidence / object clustering ([`main.cpp:4626`](../../apps/render_assistant/src/main.cpp)). | Improves base-layer boundary accuracy (held-out); no F-thread throughput regression below the ~170/s floor (`measured` this session). | `hFIELD_b` read disabled; B reverts to MV-only priors. |
| **P5 — beat-LSFG measurement** | Fair held-out A/B vs LSFG across the approaches the architecture allows. | D-14 honest benchmark; report loss region; one source of truth for the ratio ([`BENCHMARK_FAIRNESS.md`](BENCHMARK_FAIRNESS.md)). | n/a (measurement, not a code path). |

**TSan story (D-15).** The GapSignal read-side (A's present path) races nothing it
writes — it only loads the existing seq-cst atomics `f_seq` / `p_presenting` /
`stat_lap`. P2's concurrency **MUST** be TSan-verified before "safe" (the
single-writer-per-slot model is preserved, §5.4).

**Dead-ends NOT to re-try (spine §7).** Coarse-grained F-thread pipelining for the
~170 wall is a flagged dead-end (STAGE-85 pipelining broke P; render-assistant-arc
legacy). STAGE-A-FILL attacks the **deficit** (fill the gap), not the F-thread
throughput directly.

---

## What this document does NOT claim

- That STAGE-A-FILL **beats LSFG** — unmeasured; that is P5's gate (`conjectured`
  until P5).
- That the **heterogeneous multi-GPU split**, the **stigmergic-iGPU substrate**,
  or **external-capture FG** are **novel vs the commercial field** — all
  web-unverified, `conjectured`, flagged for a first-hand web pass (the
  prior-art assessment lives in the master plan + spine §8; the
  [`FG_VFI_PRIOR_ART.md`](FG_VFI_PRIOR_ART.md) dossier is a VFI-*quality* dossier
  and **MUST NOT** be cited for any multi-frame-gen / FG-on-FG / multi-GPU
  landscape claim, [`FG_VFI_PRIOR_ART.md:412-419`](FG_VFI_PRIOR_ART.md)).
- That the iGPU has the **headroom** for STAGE-G1 — the ~58% A-idle / ~42% A-use
  figures are runtime PDH readings, not measured this session for STAGE-G1; P0's
  gate measures it (`designed`).
- That **"yield when the 4090 is GPU-bound" ships today** — `designed`; the
  GPU-busy pillar signal does not exist (§7.3).
- That STAGE-A-FILL's correctness **holds under sync-clock non-stationarity** —
  `conjectured`; P2 validates it (§6.3).

All `designed` items are spec, not code, at `0.1.0-experimental`. Every code
reference above was confirmed first-hand this session at its cited file:line.

---

### Spine + reference anchors (absolute paths)

- `G:\phyriad\apps\render_assistant\src\main.cpp`
- `G:\phyriad\apps\render_assistant\shaders\wap_warp.comp`
- `G:\phyriad\apps\render_assistant\shaders\igpu_convert_pack.comp`
- `G:\phyriad\framework\gpu\include\phyriad\gpu\GpuDescriptor.hpp`
- `G:\phyriad\framework\holons\include_gpu\phyriad\holons\RoutingPolicy.hpp`
- `G:\phyriad\framework\behavior\include\phyriad\behavior\PressureScore.hpp`
- `G:\phyriad\framework\render\vulkan\bench\fg_quality_scorer\README.md`
- `G:\phyriad\docs\planning\FG_VFI_PRIOR_ART.md`
- Sibling: `G:\phyriad\docs\planning\AYAMA_LAYERED_FG_MASTER_PLAN.md`
