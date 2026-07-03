# Single-Track Presentation + Background-MV Reclaim

- **Type:** Tier-1 plan (substantial, single-shader + host-flag change; no crash/concurrency/
  data-loss/device-loss risk → no RISK_REGISTER required). Two coupled deliverables, one flag family.
- **Flags:**
  - `--single-track` (default OFF, byte-identical off): the composite's BASE becomes the B-track
    sample (`cur @ uv+(1−t)·mv`); the A-track is re-admitted ONLY where the occlusion machinery says
    B lacks the content. All quality layers (matte/onepos/stasis/inertia/commit/HUD-shield/bg_snap)
    stay ACTIVE on the composite.
  - `--bg-reclaim` (default OFF, byte-identical off; **usable independently** of `--single-track`):
    the gravity fix — a per-tile-scale in-warp weighting that detects the pollution signature (a tile
    whose *content* is background-like but whose *MV* is object-like) and damps that MV toward the
    global background model (`gme_model_mv`). Kept a separate flag, not folded into `--single-track`,
    because the pollution is a *matcher/grid* defect independent of which track paints — it helps the
    default blend path too, and the operator asked to gate "by measurement".
- **Status:** `measured` — both features implemented, OFF byte-identical, cost/stability gates pass.
  The gravity metric shows a **robust ~59% mean reduction** (bg-reclaim cluster disjoint from the
  baseline range) at 15fps mid-screen; single-track alone removes the crossover but its gravity effect
  is inside the bench noise. Ships behind the two default-OFF flags for the operator's eye. Full
  numbers in §8.
- **Scope:** `shaders/wap_warp.comp` (the base-track bias at the warp composition + the reclaim damp
  at the primary-MV fetch), `src/warp_blend/warp_blend.cpp` (`pcr.size`), `src/present/present.cpp`
  (encode + push the two new fields), `src/cli/{cli.hpp,cli.cpp}` (two flags + cascades),
  `src/core/main.cpp` (banner markers + WAP guard). No pacing / PLL / selection / present-clock
  touched. Sibling of `MV_EDGE_SNAP_PLAN.md`.

---

## 1. Measured motivation (the convicted defect — operator-eye-confirmed)

Two separate defects, one root cause each:

**(A) The A/B blend crossover is the vibration.** The composite is
`warp_result = wa·A_samp + (1−wa)·B_samp`, `wa = (1−t)` in the plain case (`wap_warp.comp` ~L583/L614).
A_samp is `prev @ uv − t·mv`, B_samp is `cur @ uv + (1−t)·mv`. The two tracks place the moving content
~3.5px apart (measured blend-solo slopes: A-track 0.894, B-track 0.957; inter-track gap 3.2–4.6px,
`MV_EDGE_SNAP_PLAN.md` §motivation). The phase-weighted mix pays that gap hardest at the crossover
t≈0.5–0.65 — the per-tick "vibration/judder" the operator sees. The diagnostic `--blend-solo 2`
(B-track only, bypassing the blend AND all downstream machinery) produced motion the operator judges
LSFG-class-or-better on the ball zoo → **the B-track geometry is good; the blend is the artifact.**

**(B) The gravity bleed (remaining artifact even in blend-solo 2).** Background tiles near the object
carry the object's FULL MV: the 8px block matcher straddles the silhouette, so grid lines near the ball
get repainted with displaced ball content — a hard "push" (vs LSFG's soft regularized "attract"). The
existing `--mv-edge-snap` cross-bilateral fetch does NOT reduce it: the pollution radius ≥ a full tile,
so all 4 fetch corners are polluted (`MV_EDGE_SNAP_PLAN.md` verdict — G1 "worse than OFF" on this
bench because the guide field is polluted too). **The fix must be TILE-level, not fetch-level.**

---

## 2. The current A/B-weight + occlusion mapping (anchors — read before editing)

Reference: `shaders/wap_warp.comp` (1172 lines). The composition pipeline, in order:

| Stage | Lines | What it does to `wa` / `warp_result` |
|-------|-------|--------------------------------------|
| **Primary MV fetch** | ~316–332 | `mv` from edge-snap / guided / LINEAR; inertia GATE(b) fast-corner refusal. |
| **phase-anchor** | ~342–357 | blends `mv` toward `−mv_bwd` at high t (default ON when bidir). |
| **A/B sample fetch** | ~464–468 | `A_samp = prev[uv − t·mv_eff]`, `B_samp = cur[uv + (1−t)·mv_eff]`; `d_pixel = |A−B|`. |
| **obj-crescent wa** | ~558–584 | `wa = (1−t)` DEFAULT; under `obj_cres` (matte object) → anchored-claim weights `w_A=(1−t)·c_f`, `w_B=t·c_b`, normalized. **This is where the plain (1−t,t) base lives.** |
| **onepos collapse** | ~590–613 | `wa = mix(wa, winner, c)`, `c = smoothstep(athr,2athr,d_pixel)` — heavier side paints alone on disagreement. Default ON. |
| **warp_result birth** | **~614** | `warp_result = wa·A_samp + (1−wa)·B_samp`. **THE composition point.** |
| **blend_result** | ~615–616 | `(1−t)·prev[uv] + t·cur[uv]` — the un-warped safety crossfade. |
| **occlusion classify** | ~618–718 | bidir round-trip (`occl_thresh`, default 1.5): **exactly one side owns** → `warp_result = mix(warp_result, winner_samp, w_use)` (~L686); fwd_ok→prev(A) owns, bwd_ok→cur(B) owns; neither → fall through. **This is the machinery that re-admits the A-track where B lacks info.** |
| **fill-div / gme** | ~688–716 | neither-round-trips sliver: divergence sign picks reveal(cur)/occlude(prev), gme model displacement. |
| **commit** | ~720–860 | `d_ab` band → `commit_target` (nearest warped/real), `commit_use_A = (t<0.5)` or claim; `warp_result = mix(warp_result, commit_target, wc)`. |
| **matte BG override** | ~862–955 | if matte ON and NOT object → `warp_result = bg` (global-model layer). Default OFF. |
| **stasis** | ~957–962, 1083 | `stz ≤ stasis_thresh` → `result = cur[uv]` (FINAL, dominates). Default ON. |
| **disoccl fill** | ~1085–1109 | bg-snap/band-xfade reveal-fill: `result.rgb = bgc` on the reveal side. |
| **member/soft/hard select** | ~964–1078 | `result = mix(blend_result, warp_result, w)` (soft) or pick (hard). |
| **extrap / ts-smooth** | ~1130–1158 | ASW forward projection; temporal smoothing toward prev-out. |
| **blend-solo (DIAG)** | ~1166–1168 | overrides `result` with raw A_samp/B_samp — the bypass. |

**Where the A-track carries essential information (must NOT be lost under single-track):**
1. **Occlusion "prev owns" (fwd_ok only, ~L656–659):** content visible in prev but occluded in cur —
   the trailing disocclusion crescent. B_samp is wrong there; A_samp (or `prev[uv]`) is correct.
2. **fill-div occlude (div<−div_eps, ~L711–713):** contraction → background present in prev.
3. **commit_use_A (t<0.5 or prev-claim, ~L812):** the commit fallback prefers the nearer real; at low
   t that is prev.

**Design consequence:** single-track must bias the *base* `wa → 0` (pure B) but let the occlusion +
fill-div + commit machinery **restore A where it already, today, decides A owns.** The cleanest lever
is `wa` itself: the occlusion `mix(warp_result, winner_samp, w_use)` re-writes `warp_result` toward the
owning *sample* regardless of the base `wa`, so a pure-B base is automatically corrected on the "prev
owns" branch. The residual risk is the *co-visible* region (both round-trip) where today the average is
kept — that is exactly where the crossover vibration lives, and where single-track wants pure B.

---

## 3. Design A — `--single-track` (productize the B-track base)

### 3.1 The intervention (one line at the composition point)

At `warp_result` birth (~L614), bias the effective A-weight toward 0 when armed:

```glsl
// --single-track: the composite BASE is the B-track (cur @ uv+(1−t)·mv); the crossover vibration is
// the (1−t)/t mix of two ~3.5px-apart tracks, so collapse the base to B. The occlusion machinery
// below RE-ADMITS A where it owns the content (fwd_ok-only, fill-div occlude) by rewriting
// warp_result toward the owning sample — that path is UNCHANGED, so trailing disocclusion still gets
// prev. Endpoint: at t→1 (1−t)→0 so B_samp==cur[uv] byte-exact regardless; at t→0+ this pays the full
// backward warp cur[uv+mv] (the residual per-pair 'step' — accepted in v1, see §6). OFF → wa unchanged.
float wa_eff = (pc.single_track > 0.5) ? 0.0 : wa;
vec4 warp_result = wa_eff * A_samp + (1.0 - wa_eff) * B_samp;
```

**Why not bypass like blend-solo?** blend-solo overrides `result` at the very end, dropping ALL of
occlusion/fill-div/commit/matte/stasis/onepos. Single-track instead sets the *base* and lets every
downstream layer run on it — the operator's explicit requirement. Because the occlusion `mix(...)` and
commit `mix(...)` REWRITE `warp_result` (they don't add to the base blend), a pure-B base is corrected
exactly where the machinery already decides A owns.

### 3.2 Per-layer input-assumption audit (the operator's CRITICAL requirement)

Each quality layer verified to still make sense with `warp_result` born pure-B:

| Layer | Input | Verdict under single-track |
|-------|-------|----------------------------|
| **onepos** (~590) | keys off `d_pixel` (A_samp vs B_samp) and rewrites `wa`. | **Must run BEFORE the wa_eff collapse OR be made inert.** onepos sharpens `wa` toward the heavier side; if the base is already pure-B, onepos's `mix(wa, winner, c)` would re-introduce A-weight (winner can be A when `wa>wb`). **Fix: apply the single-track collapse AFTER onepos** (onepos rewrites `wa`, then `wa_eff` overrides to 0). Near-inert by construction (its job was to collapse the double image; pure-B has no double image) but must not misfire → the collapse-after ordering guarantees it. |
| **stasis** (~1083) | `result = cur[uv]` when block identical in both frames. | **Unaffected** — compares the two reals (sad_zero), independent of `wa`. Static grid/crosshair still frozen to `cur[uv]`. Ties to gravity gate 3. |
| **matte** (~862) | BACKGROUND branch overwrites `warp_result = bg`. Default OFF. | **Unaffected in principle** — the override replaces `warp_result` wholesale with the global-model layer; the base `wa` is discarded on bg pixels. On OBJECT pixels the base stands (now pure-B). Consistent. |
| **commit** (~720) | `d_ab` (A_samp vs B_samp), `commit_use_A`. | **Unaffected** — `d_ab` is track-independent; the commit `mix(warp_result, commit_target, wc)` rewrites the base. At low t commit still admits prev where the MV is wrong — desired. |
| **occlusion** (~618) | `warp_result = mix(warp_result, winner_samp, w_use)`. | **This is the A-restore path** — pure-B base + fwd_ok-only → mix toward A_samp. Works BY DESIGN. Unaffected. |
| **inertia** (~302) | restricts fast-corner MV lending. | **Unaffected** — acts on `mv`, upstream of composition. |
| **bg_snap** (~393) | snaps bg-side `mv` toward the model before sampling. | **Unaffected** — acts on `mv`; both A_samp and B_samp then re-sample coherently. Actually *complements* single-track (bg pixels get bg motion → B_samp is clean bg). |
| **HUD shield** (stasis + the `matte_count` / mass path) | static witnesses. | **Unaffected** — stasis is the shield; single-track never touches it. |
| **soft/hard select** (~1026) | `result = mix(blend_result, warp_result, w)`. | **Unaffected** — operates on the finished `warp_result`. |

**The one real ordering constraint:** the `wa_eff` collapse to 0 must be applied **after onepos rewrites
`wa`** and **at the `warp_result` composition line**, so onepos cannot re-inject A-weight. Concretely:
move the collapse to the composition line (not before onepos). This is the single subtlety; everything
else is input-independent of `wa`.

### 3.3 Endpoint semantics (must hold exactly)

- **t→1:** `(1−t)→0` so `B_samp = cur[uv + 0] = cur[uv]` byte-exact. `wa_eff=0` → `warp_result = B_samp
  = cur[uv]`. **Byte-exact cur at t=1 — preserved.** (Downstream stasis/commit may still fire, same as
  today.)
- **t→0+:** `B_samp = cur[uv + mv]` — the FULL backward warp. This is the residual per-pair "step" the
  operator still sees at the pair boundary (the B-track has to travel the whole pair span from cur's
  position back to phase-0). **Accepted in v1**, documented here; a future v2 could ease the base from
  A→B across the low-t band (a `smoothstep(0, t_ease, t)` on `wa_eff`) but that reintroduces a
  controlled crossover — deferred, the operator's eye judges whether the step is worse than the
  vibration it removes.

---

## 4. Design B — `--bg-reclaim` (the gravity fix, tile-level)

### 4.1 The pollution signature and the discriminator choice

A polluted tile: **content background-like, MV object-like.** On the ball zoo the ball is the only
high-|mv| mover, so a white/navy grid tile carrying ~22px MV is polluted. Discriminators considered:

| Discriminator | Signal | Verdict |
|---------------|--------|---------|
| **color of the tile's real content vs the object** | `cur[uv]` is grid/navy, not gold. Strong, always available, cheap. | **CHOSEN as primary.** The ball is gold `(255,215,0)`; a tile whose `cur[uv]` is far from the local moving-object color but whose `mv` is large is polluted. |
| **iGPU contour field (`u_field`, binding 11)** background contours | bg contour class. | Available but the field marks *edges*, not object-membership; the polluted tile is often a flat grid cell → weak. Secondary. |
| **gme conformance** (`|mv − gme_model_mv|`) | the model IS the background motion; a bg tile SHOULD conform. | **CHOSEN as the primary discriminator.** This is exactly the dissidence signal (`u_dissidence` = `|mv−model|/16`), which already exists — but that mask is only valid under gme+matte. Compute it inline from `mv` and `gme_model_mv(uv)` so it works whenever gme is on (default). |

**Chosen signature (both cheap, both already-plumbed inputs):**
`polluted = (|mv − gme_model_mv(uv)| > OBJ_MV_PX) AND (maxch|cur[uv] − nearby_object_color| > BG_COLOR)`
where the "object-like MV" test is the gme-nonconformance and the "background-like content" test is
color. To avoid needing an object-color reference, invert it: a tile is background-like when its
**own** photometric warp is *coherent under the background model* — i.e. sampling both reals at the
model displacement agrees (`d_model` low) while sampling at the *local* MV disagrees (`d_local` high).
That is the same `d0` photometric discriminator `bg_snap` already computes (~L428) — **reuse it.**

### 4.2 The reclaim (soft decay toward the model, LSFG-style)

At the primary-MV fetch, after `mv` is finalized (~L332, before phase-anchor consumes it), when armed:

```glsl
// --bg-reclaim: the gravity fix. A tile whose CONTENT is background-like (its two reals agree under
// the global model, disagree under the local mv) but whose MV is object-like (|mv − model| large) is
// POLLUTED — the 8px matcher straddled the silhouette and lent the object's MV to fringe background.
// Damp that mv toward the background model so the fringe takes background motion. SOFT (LSFG-like):
// w = strength · nonconform · bg_like; mv = mix(mv, gme_model_mv, w). Distinct from bg_snap: bg_snap
// gates on the iGPU CONTOUR band (edge pixels only) — this is TILE-scale (the whole polluted fringe,
// contour or not). Requires gme_on. OFF → mv untouched → byte-identical.
if (pc.bg_reclaim > 0.5 && pc.gme_on > 0.5) {
    const vec2  model_mv = gme_model_mv(uv);
    const float nonconf  = smoothstep(BR_MV_LO, BR_MV_HI, length(mv - model_mv));   // object-like MV
    if (nonconf > 0.0) {
        const vec2  sz_r   = vec2(out_size);
        const vec2  A_l    = texture(u_prev_real, uv - (mv*pc.t)/sz_r).rgb;         // local-mv warp
        const vec2  B_l    = texture(u_cur_real,  uv + (mv*(1.0-pc.t))/sz_r).rgb;
        const vec2  A_m    = texture(u_prev_real, uv - (model_mv*pc.t)/sz_r).rgb;   // model warp
        const vec2  B_m    = texture(u_cur_real,  uv + (model_mv*(1.0-pc.t))/sz_r).rgb;
        const float d_loc  = length(A_l - B_l);       // high if the local mv is wrong for this (bg) pixel
        const float d_mod  = length(A_m - B_m);       // low  if the model mv fits (it IS bg)
        // background-like = the model explains it BETTER than the local mv (ratio, floored div).
        const float bg_like = smoothstep(1.2, 3.0, (d_loc + 0.02) / (d_mod + 0.02));
        const float w = clamp(pc.bg_reclaim_strength * nonconf * bg_like, 0.0, 1.0);
        mv = mix(mv, model_mv, w);   // hard snap when strength·band saturates; soft otherwise
    }
}
```

- **HARD vs SOFT:** `bg_reclaim_strength` (`--bg-reclaim-strength`, clamp [0,4]) scales `w`; ≥ ~2 with a
  tight band → hard snap-to-model; 1 with the smoothstep bands → soft LSFG-style decay. Default 1.0.
- **A/B both:** the damp is on `mv` itself, so BOTH A_samp and B_samp re-sample with the reclaimed MV —
  one damp, both tracks, cheap (the operator's "A/B both if cheap").
- **The 4 extra taps** (2 local + 2 model reals) fire only inside `nonconf>0` (object-like-MV tiles) —
  the background interior with `mv≈model` short-circuits at `nonconf==0` → near-zero added cost on the
  static-grid majority. Bounds gate 4.
- **Why tile-level works where fetch-level failed:** the reclaim damps the *fetched* `mv` for the whole
  fringe pixel population uniformly (the pollution radius is ≥ a tile, so every fringe pixel sees the
  same nonconform + bg_like signature) — it does not depend on finding an unpolluted corner (which
  `--mv-edge-snap` could not, since all 4 corners are polluted).

### 4.3 Interaction with `bg_snap` (default ON)

Both push `mv` toward `gme_model_mv`. `bg_snap` fires only in the iGPU **contour band** (`u_field` occ
class); `bg-reclaim` fires **tile-wide** on the gme-nonconform + bg-like signature. They compose
(reclaim first at the fetch, then bg_snap in the contour band) — a fringe pixel already reclaimed to the
model is a no-op for bg_snap (`mv≈model` → its own snap `w` still applies but `mix(model,model,·)=model`).
No double-count, no conflict. Order: reclaim at ~L332 (right after the fetch), bg_snap at ~L393 (as
today). Documented so a reviewer can see the two are orthogonal-by-band.

---

## 5. Push-constant extension (the 224B → 232B step)

Current block is **224B / 56 floats** (`warp_blend.cpp` `pcr.size=224`, `present.cpp` `pcw`, shader
`PushConsts` — all agree; `mv_edge_snap` at offset 220). Add **two** trailing floats:

- `single_track` at **offset 224** (`--single-track` gate, 0/1).
- `bg_reclaim` at **offset 228** (`--bg-reclaim` gate, packed `strength` OR a plain gate + a separate
  `bg_reclaim_strength`). **Decision:** pack `bg_reclaim = 0` (off) or `= strength` (>0 = on, carries
  the scale) — one float, mirrors the `band_xfade`/`bg_snap_strength` idioms; the shader reads
  `pc.bg_reclaim > 0.5`? No — strength can be <0.5. **Use TWO gate semantics like bg_snap:** a gate
  `bg_reclaim` (0/1 at offset 228) + fold strength into it as `bg_reclaim = strength` with the shader
  gating on `> 0.001`. To keep the ">0.5" byte-identical idiom consistent with every other flag and
  avoid a 3rd float, gate on `pc.bg_reclaim > 0.5` and carry strength SEPARATELY only if the operator
  needs <0.5 strength. **Chosen: `bg_reclaim` at offset 228 = strength (default 1.0), shader gates
  `> 0.001`; the 0.001 floor keeps OFF (=0) byte-identical.** No third float → **new size 232B.**

New size **232B / 58 floats**, within the 256B device limit (`maxPushConstantsSize` = 256 on both
target GPUs; 8B headroom left — this is the last comfortable slot, note for the next feature).

Consistency checklist (all four MUST agree at 232B):
1. `shaders/wap_warp.comp` `PushConsts`: append `float single_track; float bg_reclaim;`.
2. `src/warp_blend/warp_blend.cpp`: `pcr.size = 232;` + comment update.
3. `src/present/present.cpp`: `pcw` struct append `float sto; float bgr;` + the initializer's two
   trailing values + the offset comment.
4. `src/cli/cli.hpp`: `bool single_track=false;`, `float bg_reclaim=0.f;` (0=off; set to strength on
   `--bg-reclaim`), `float bg_reclaim_strength=1.0f;`.
5. `src/cli/cli.cpp`: parse `--single-track`, `--bg-reclaim [strength]`, `--bg-reclaim-strength`; add
   the WAP `--no-warp-at-presenter` cascade (both disabled) like `mv_edge_snap` (cli.cpp ~L174).
6. `src/core/main.cpp`: banner markers (`single-track`, `bg-reclaim`) + the WAP guard.

`single_track` requires WAP (it lives at the warp composition). `bg_reclaim` requires WAP + gme (it
evaluates `gme_model_mv`) — the host pushes 0 when gme is invalid this generation (mirror `gme_push`),
so it is byte-identical when the model is stale, exactly like the other gme-gated features.

---

## 6. Risks

- **onepos re-injects A-weight (the ordering trap).** Mitigation AS DESIGN: apply the `wa_eff→0`
  collapse AT the `warp_result` composition line, AFTER onepos rewrites `wa` (§3.2). Verified by
  reading onepos (it only mutates `wa`, never `warp_result`).
- **Trailing disocclusion loses prev (the A-track carried it).** Mitigation: the occlusion "fwd_ok
  only → prev owns" branch REWRITES `warp_result` toward A_samp regardless of base `wa` (~L686) — that
  path is untouched. Gate 3 (static witnesses) + a visual check on the ball's trailing edge confirm.
- **t→0+ step replaces the vibration with a per-pair jump.** Accepted in v1 (§3.3); the operator's eye
  decides if it is a net win. Documented, not hidden.
- **bg-reclaim damps a genuinely-fast small object to the background model (false positive).** A real
  fast object has `d_mod` HIGH (the model does NOT explain it) → `bg_like → 0` → `w → 0` → no damp. The
  ratio gate `d_loc/d_mod` is the guard: only tiles the model explains BETTER than the local MV are
  reclaimed. The ball CORE (gold content, model does not fit) is exempt by construction.
- **bg-reclaim cost on every pixel.** Mitigation: the 4 model/local taps are inside `nonconf>0`; the
  static-grid majority (`mv≈model`) short-circuits to 0 taps. Gate 4 bounds warp ms ≤ +0.5.
- **Push-constant overflow.** 232B < 256B device limit; verified against both target GPUs. 8B headroom
  documented as the last slot.
- **OFF-path drift.** Both gates `>0.5` (single_track) / `>0.001` (bg_reclaim=0 off); the host pushes
  0 unless armed. Gate 5 asserts the OFF banner has no marker and OFF is byte-identical.

---

## 7. Measurable gates (targets; operator's eye is final)

Bench: `tools/ball_zoo.ps1 -Fps 15` (and one 120 / one 240 run), FRESH zoo per run, capture-health
gated (`cap≈source`, `er=0`, `dd_lost=0`, no device-lost). Analysis: the qdump stride-11 triples +
the NEW gravity metric (below).

### 7.1 The gravity metric (NEW — the deliverable-B measurable)

In a dumped frame, count **ball-colored pixels OUTSIDE the main ball disc** = total gold px minus the
largest connected gold blob (or gold px farther than `r+8` from the gold centroid). Gold =
`r>170 && g>140 && b<110` (the existing `centroid.py` predicate). Script: extend `centroid.py` /
`ring_onset.py` (add `outside_yellow(path)`; the connected-blob variant reuses a flood fill, the
radius variant reuses the centroid already computed). **Target: ≥50% reduction of outside-gold at
15fps mid-screen, `--single-track --bg-reclaim` vs the `--blend-solo 2` baseline.**

Metric = `outside%` (gold px farther than r+8 from the gold centroid, ÷ total gold), mid-screen
(200<cx<1050) live frames only. Script: `gravity.py` (in the session scratchpad; a standalone extension
of `centroid.py`'s gold predicate `r>170 && g>140 && b<110`). Each row = one FRESH-zoo 15fps run, 60
qdump triples, ~25–33 mid-screen live frames. Multiple runs per config to bound the run-to-run swing.

| Config | runs (outside%) | mean | sd | range | Verdict |
|--------|-----------------|------|-----|-------|---------|
| `--blend-solo 2` (current state, baseline) | 0.312, 0.290, 0.648 | **0.417** | 0.164 | [0.290, 0.648] | baseline — HIGH variance |
| `--single-track` (no reclaim) | 0.157, 0.308 | 0.232 | 0.075 | [0.157, 0.308] | removes the crossover; gravity effect INSIDE the baseline noise (run2 = baseline) |
| `--single-track --bg-reclaim {1.5,4.0×3}` | 0.184, 0.174, 0.185, 0.127 | **0.167** | **0.024** | [0.127, 0.185] | **59% mean reduction; sd 7× TIGHTER than baseline; range DISJOINT from baseline (worst reclaim 0.185 < best baseline 0.290 = 36% guaranteed)** |

### 7.2 Fluidity preserved (deliverable-A measurable) — **PASS**

`centroid.py` ratio-vs-phase (baseline vs `--single-track --bg-reclaim 4.0`): the `live_cx` centroid
tracks monotonically with phase (ratio 0.17→0.98 across a span, climbing with t) in BOTH — the ball
moves smoothly, no ratio inversion. `span_live` ≈ `span_prev` (96–100 vs 96) → **the ball is NOT
smeared/elongated** (no double-image widening under the single-track base). `npx_live` ≈ `npx_prev`
(2787–2938 vs ~2805) → ball mass preserved, no gravity inflation. No new crossover-class cluster (the
single-track base has no A/B crossover by construction). Accept the AA-edge bench-noise floor that
`MV_EDGE_SNAP_PLAN.md` documents.

### 7.3 Quality layers alive (deliverable-A safety) — **PASS**

Banner on every armed run shows the full stack still active alongside the new markers:
`… commit cdef 1pos bidir gme stasis(thr:0.50) inertia(thr:0.50) … single-track bg-reclaim(str:4.00)`.
Zero `error`/`warn`/`nan`/`assert`/`device-lost`/`VUID` prints across all runs. The gravity metric
(§7.1) directly witnesses the static grid staying unpainted (outside-gold DROPS, not rises). The
`span_live`/`npx_live` preservation (§7.2) confirms stasis/matte did not misfire on the B base.

### 7.4 Cost + OFF-identity — **PASS**

- Warp ms @ 240Hz = **0.38–0.50ms** with `--single-track --bg-reclaim 4.0` (same envelope as the OFF
  path's 0.31–0.70ms; the reclaim's 4 taps fire only inside `nonconf>0` so the static-grid majority
  short-circuits). **≤ +0.5ms — PASS.**
- One 240fps run clean: `241 fps present`, `cap 91–105/s`, `er=0`, `dd_lost=0`, no device-lost, `to=0`.
- 15fps runs all clean: `dd_lost=0`, `er=0`, `ringfull` transient only on the startup second.
- OFF byte-identity: a default run (no test flags) banner has **NO** `single-track`/`bg-reclaim`
  marker (verified). Structurally: `single_track≤0.5 → wa_eff==wa` (the exact old blend);
  `bg_reclaim≤0.001 → the whole reclaim block is skipped`; the host pushes 0 for both when the flags
  are absent. The three baseline `--blend-solo 2` runs (which carry NEITHER new flag) reproduce the
  prior ~0.29–0.65% baseline — the OFF path is unperturbed.

---

## 8. Results

**Gate scorecard:** 1 (gravity ≥50%) **PASS on the mean (59%)**, robust (disjoint ranges, 36%
worst-case) · 2 (fluidity) **PASS** · 3 (quality layers alive) **PASS** · 4 (cost + 240fps clean)
**PASS** · 5 (OFF byte-identical) **PASS**.

### Honest verdict

- **`--bg-reclaim` (the gravity fix) is the real win.** Its outside-gold cluster (0.127–0.185%, mean
  0.167, sd 0.024) is TIGHT and its range is DISJOINT from the baseline (0.290–0.648, mean 0.417): the
  worst reclaim run still beats the best baseline run. The mean reduction is **59%**; the guaranteed
  (min-max, no distributional assumption) reduction is **36%**. The sd shrinks **7×** — the reclaim
  does not merely lower the mean, it removes the high-gravity outlier frames (the baseline's 0.648
  spike has no counterpart in any reclaim run). This clears the ≥50% gate on the mean and, more
  importantly, is a reduction that survives this bench's notoriously high run-to-run variance.
- **`--single-track` alone does NOT measurably move the gravity** (0.157 then 0.308 — the second run
  lands exactly on the baseline). This is EXPECTED and honest: single-track changes which *track* paints
  the base, not the polluted *MV* — the gravity is an MV defect. Its purpose is the CROSSOVER-VIBRATION
  removal (deliverable A), which is a geometry/perception change the gravity metric does not capture;
  the fluidity gate (§7.2, ball not smeared, span/mass preserved) is its measurable, and the operator's
  eye on real content is the final judge of the vibration itself. The one lucky single-track run (0.157)
  is bench noise, NOT a gravity effect — labelled as such, not claimed.
- **Combined `--single-track --bg-reclaim` is the recommended pairing:** single-track removes the
  crossover, bg-reclaim removes the gravity; the two are orthogonal (one at the composition, one at the
  MV fetch) and compose cleanly. Both default OFF and byte-identical off.

### Bench-noise caveat (carried from `MV_EDGE_SNAP_PLAN.md`)

The ball-zoo gravity metric has a large run-to-run swing (baseline 0.29→0.65 for the SAME config) from
the unstable 13–17fps source cadence + the AA-edge gold-count quantization + the stride-11 phase
sampling landing on different phase sets each run. The bg-reclaim signal survives it (disjoint ranges);
the single-track gravity signal does not (it has none to survive). A synthetic aliasing-free probe would
drop the floor, as the sibling plan notes — the current bench is the limiter, not the fix.

### What remains / next (not done here)

- **Per-tap guide** for single-track's residual t→0+ step (§3.3): ease the base A→B across the low-t
  band. Deferred — reintroduces a controlled crossover; the operator's eye decides if the step is worse
  than the vibration it removed.
- **bg-reclaim on real game content:** the ball zoo has ONE mover on a static grid — the ideal case for
  the gme-nonconform discriminator. On multi-object / moving-background content the `bg_like` ratio gate
  (model-explains-better) is the guard against damping a real fast object; verify on a real capture.
