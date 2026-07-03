# Single-Track Presentation + Background-MV Reclaim

- **Type:** Tier-1 plan (substantial, single-shader + host-flag change; no crash/concurrency/
  data-loss/device-loss risk → no RISK_REGISTER required). Two coupled deliverables, one flag family.
- **Flags:**
  - `--single-track` — **DEFAULT ON** (v3.2 semantics, §3.5 + §10): the pre-store B-track override
    (`result = B_samp`, exactly what `--blend-solo 2` outputs) + the **screen-static re-admission mix**
    (per-pixel three-hypothesis evidence `d_zero` vs `d_warp` → `w_s`; the block-proven stasis bool is
    the `w_s=1` extreme — the HUD/overlay protection, now robust under camera pan). `--no-single-track`
    restores the A/B blend world exactly; `--st-no-stasis` isolates stage v3.0 (the pure mirror).
    (The v0/v1/v2 designs described in §3.1–§3.4 are the superseded history — kept as the audit
    trail; their gates remain upstream, shadowed by the final override.)
  - `--bg-reclaim [strength]` — **DEFAULT ON at 4.0** (hard snap; **usable independently** of
    `--single-track`): the gravity fix — a per-tile-scale in-warp weighting that detects the pollution
    signature (a tile whose *content* is background-like but whose *MV* is object-like) and damps that
    MV toward the winning hypothesis: the global background model (`gme_model_mv`), or **(v3.2, §10)
    ZERO where the screen-static evidence beats the model** (a screen-fixed overlay under camera pan
    must not be damped toward the camera). `--no-bg-reclaim` / `--bg-reclaim 0` disables. Kept a
    separate flag because the pollution is a *matcher/grid* defect independent of which track paints —
    it helps the default blend path too, gated by measurement (§8).
- **Status:** **SHIPPING, DEFAULT-ON — v3.2** (2026-07-03). Operator verdict on v3.1 + `--bg-reclaim
  4.0`, verbatim: **"la alucinación es mejor que LSFG"** — smooth, no crossfade/vibration, statics
  crisp. Defaults flipped (`single_track=true`, `bg_reclaim=4.0f`); UI switches synced (the default-ON
  pattern: emit nothing when ON, the off-flag when OFF). The gravity metric: **~59% mean outside-gold
  reduction**, ranges disjoint from baseline (§8). The road here: THREE operator-eye refutations of
  subtractive recompositions (v0 crossfade fallback → v1 static-cur fallback → v2 law gates — each
  still vibrated while `--blend-solo 2` stayed smooth on the same builds) before the **v3 strategy
  flip (§3.5): build UP from the known-smooth blend-solo-2 base**. The HSR **field report** then
  convicted a screen-static-overlay regression under camera pan (HUD self-ghost, "inverse crossfade")
  → **v3.2 (§10): the screen-static evidence re-admission**, bench-gated (pan-bench glass-ghost
  134.5 → 15.7 px, ~92% of the regression closed; operator eye-test pending). Known residuals
  (operator-observed, non-blockers) in §9.
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
| **soft/hard select** (~1026) | `result = mix(blend_result, warp_result, w)`. | **THE v1 MISS (corrected):** the selection itself is warp-vs-FALLBACK, and the fallback `blend_result` was still the (1−t)/t crossfade → the crossover re-entered through every low-confidence block. FIXED: under single-track `blend_result`'s base = pure `cur[uv]` (track-consistent). See the §6 risk entry. |

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

### 3.4 THE SINGLE-TRACK LAW + the consistency audit (v2 — two operator-eye bisections deep)

**The failure history (the honesty trail):**

1. **v0 (the base collapse, §3.1):** the operator's eye — STILL VIBRATES, even with ALL optional
   layers disabled (`--no-commit --no-commit-default --no-member-commit --occl-thresh 0 --no-matte
   --no-phase-anchor --no-soft-gate --no-onepos --no-obj-crescent --no-stasis --no-inertia
   --no-crescent --no-appearance --no-travel --no-contour --no-ambig --no-rescue --no-band-xfade
   --no-fill-div --no-vblend --no-bg-snap`), while `--blend-solo 2` stayed smooth on the same build.
   Root cause: the UNCONDITIONAL final selection's fallback content, `blend_result` = the un-warped
   (1−t)/t crossfade of the two reals — the crossover re-entering through every low-confidence block.
2. **v1 (the fallback made pure cur):** the operator's eye — STILL VIBRATES, "looks like a crossfade".
   Root cause: STATIC cur on MOVING content still positionally disagrees with the warped track at
   intermediate phases (the textureless ball interior is low-confidence → falls back → sits at cur's
   position while the warped silhouette advances = content at two positions = the crossfade look).
   The v1 fix was necessary but not sufficient.
3. **v2 (this section):** the general law + the forced-warp selection + the injector audit below.

**THE LAW (the v2 spec's core):** *in single-track mode, ONLY warped-at-phase content may paint
MOVING regions; unwarped real content may paint only where staticity is PROVEN (stasis / the matte's
model-conforming background). Every layer that substitutes unwarped reals onto arbitrary (possibly
moving) content re-creates the positional disagreement the mode exists to remove.* The old blend
world tolerated real-injection because `warp_result` was itself blend-ish; the single-track world
does not.

**The enforcement (all gated on `pc.single_track > 0.5`; OFF byte-identical):**

1. **The final selection is forced to warp:** `result = warp_result` at the result birth — the ENTIRE
   selection cascade (multicand medoid / soft gate / hard keep-freeze) is inert. It has NO off-flag
   (which is exactly why no flag combination could smooth it). NOTE: the medoid (`mc_on`, DEFAULT ON)
   was the actually-EXECUTING path in the operator's bisection (`--no-multicand` was not in the set);
   its dispersion guard paints `blend_result` and its candidate set includes `A_samp` + `wa`-blended
   perturbed warps — all track-inconsistent; the one gate silences all of them.
2. **The injector audit** — every site that writes real (or lagged) content, classified:

| Layer (shader site) | Paints MOVING content with unwarped/lagged reals? | Verdict / action |
|---|---|---|
| final soft/hard selection (result birth) | **YES** — fallback `blend_result` selected BY low confidence; no off-flag | **FORCED INERT** (`result = warp_result`) |
| multicand medoid (`mc_on`, DEFAULT ON) | **YES** — dispersion guard → `blend_result`; candidates incl. `A_samp` + two-track perturbed blends | **FORCED INERT** (same gate) |
| commit family (`commit_thresh`, incl. `commit_use_A`/`near_real`, the appear re-blend, rescue) | **YES** — `near_real` = unwarped real at raw uv, selected BY `d_ab` = exactly the moving silhouette + textureless interior; rescue's replacement = a TWO-track candidate average | **GATED OFF** under single-track |
| ts-smooth (`ts_smooth`, default 0) | **YES when armed** — prev-OUTPUT at same uv trails on moving content; the garbage gate selects moving pixels | **GATED OFF** under single-track |
| occl winner (one-side-owns branch) | NO — `winner_samp` is a WARPED sample (A/B at phase) | **KEEP** |
| fill-div (default OFF) | NO — paints model-displaced reals (gme) or warped A/B samples; domain = the disocclusion sliver (background) | **KEEP** |
| matte BG override (default OFF) | NO — `bg` = blend of MODEL-displaced reals; for model-conforming background both samples show the SAME content (the model is exact there) → an agreeing blend, no positional split; domain = background-classified | **KEEP** |
| crescent / disoccl-commit / travel / contour / obj-crescent | NO — matte-gated side-weightings among the same model-displaced bg samples (contour's time-nearest real is a REFEREE only, never painted) | **KEEP** |
| stasis (the FINAL override) | NO — `cur[uv]` on PROVEN-static blocks (`sad_zero ≤ thresh` = the block is identical in both reals) — positionally safe by definition | **KEEP** |
| disoccl reveal-fill (`bg_snap`/`band_xfade`, default ON) | NO — paints `cur[uv]` ONLY on evidence-gated REVEALED background (the two reals disagree at uv AND dissidence-fwd > bwd = the object left); the painted content is the static revealed bg | **KEEP** |
| extrap / ASW | NO — cur warped FORWARD along mv = the B-track extended past phase 1 | **KEEP** |
| ambig / phase-anchor / bg-snap / bg-reclaim / mv-guided / mv-edge-snap / inertia | NO — MV-level (they choose the vector, never inject reals) | **KEEP** |
| `blend_result` birth (the v1 pure-cur fix) | now a DEAD VALUE under single-track (every consumer inert/gated) | **KEPT** as defense-in-depth (any future consumer inherits track-consistency) |

**Status of the vibration claim:** **v2 REFUTED by the operator's eye — STILL VIBRATES.** Third
subtractive failure. Superseded by the v3 strategy flip (§3.5).

**THE v2 AUDIT CRITERION WAS WRONG (the correction):** "warped samples = consistent" is INSUFFICIENT.
The occl winner re-admits PREV-warped (**A-track**) samples — and A-track vs B-track placement
disagrees by the measured **3.2–4.6px**; likewise the fill-div/matte/crescent **model-displaced**
samples place the ball at a THIRD (gme-track) position. Three tracks, three positions. The correct
consistency criterion is: **SAME-TRACK (B: `cur @ uv+(1−t)·mv_local`) or PROVEN-STATIC.** Under that
bar, the v2 "KEEP" verdicts for the occl winner (A-side winners), fill-div, matte-bg, and the crescent
family were all still able to inject other-track positions — plausibly the v2 residual vibration.

### 3.5 v3 — THE STRATEGY FLIP: build UP from the known-smooth base

**Rationale:** three subtractive recompositions failed the operator's eye in a row — v0 (crossfade
fallback), v1 (static-cur fallback), v2 (forced-warp selection + medoid/commit/ts-smooth inerts) —
while `--blend-solo 2` stayed smooth on the SAME builds every time. Subtracting inconsistency from the
full pipeline keeps losing to un-audited injectors; v3 inverts the construction: **start from exactly
what `--blend-solo 2` outputs, and re-admit layers ONE AT A TIME, each operator-eye-gated.**

**The mirror (what `B_samp_final` is, exactly):** `--blend-solo 2` outputs `B_samp` — a `const vec4`
fetched ONCE at the Gate-2 site (`texture(u_cur_real, uv + inv_t_mv_uv)`,
`inv_t_mv_uv = (mv_fwd_eff·(1−t))/out_size`), **never mutated after birth** (it is `const`; the blend
consumes it by value). The blend-solo override is the LAST modification of `result` (after ts-smooth),
immediately before `imageStore`. v3 overrides at the SAME site with the SAME value, placed just BEFORE
the blend-solo branch so the diagnostic still wins when both flags are set.

**The stage ladder (each stage eye-gated by the operator; no stage advances without the gate):**

| Stage | Composition at the override site | Flag | Eye gate | Status |
|---|---|---|---|---|
| **v3.0** | `result = B_samp` — nothing else; byte-equal to `--blend-solo 2` | `--single-track --st-no-stasis` | must be INDISTINGUISHABLE from `--blend-solo 2` | passed implicitly (v3.1 is v3.0 + stasis and passed outright) |
| **v3.1** | v3.0 + ONLY the stasis re-admission: `if (stasis) result = cur[uv]` — proven-static (identical-in-both-reals, the main-scope `stasis` bool the block already computes; no hoisting needed) presents the crisp real; moving content stays pure B-track | (superseded by v3.2) | still smooth AND the zoo's grid/crosshair crisp | **PASS — operator verdict "la alucinación es mejor que LSFG"; smooth, no crossfade/vibration, statics crisp (with `--bg-reclaim 4.0`). THEN field-convicted under camera pan (HSR): screen-fixed overlays self-ghost — the block-level proof cannot see gme-nonconform HUD tiles (§10)** |
| **v3.2** | v3.1 generalized to ONE unified mix: per-pixel three-hypothesis screen-static evidence (`d_zero` vs `d_warp` → `w_s`, stasis = the `w_s=1` extreme; `result = mix(B_samp, cur[uv], w_s)`) + the bg-reclaim screen-static exemption (damp target slides model→ZERO) | `--single-track` (**the shipping DEFAULT-ON**) | pan bench: HUD crisp, world smooth (G1–G4 measured, §10.4) | **BENCH PASS (§10.4); operator eye-test on the pan bench + HSR pending** |
| v3.3+ | further re-admissions (occlusion one-sided, matte bg, …) — one at a time, LATER | — | one eye gate each | NOT BUILT (deliberate); §9 lists the residual candidates |

**Why v3.2 ships:** HUD protection (static witnesses crisp at source) is what HSR needs — v3.1's
block-level proof delivered it for a static camera but broke under camera pan (§10); v3.2 broadens the
staticity PROOF to per-pixel evidence while staying inside the consistency law. The disocclusion look
of the pure B-track was already operator-accepted in `--blend-solo 2`. `--no-stasis` (stasis_thresh=0)
now removes only the block-proof `w_s` saturation; the per-pixel evidence remains.

**Encoding:** the existing `single_track` push float — 0 = OFF, 1.0 = v3.2 (was v3.1), 2.0 = v3.0; the
shader's final override reads `>0.5` (armed) and `<1.5` (the screen-static evidence mix). No new push
field; the block stays 232B. The v1/v2 gates remain upstream (they key on `>0.5`, so both stages arm
them) — now SHADOWED by the final override: harmless, they save dead work, and their documented
rationale stands as the audit trail.

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
- **FOUND POST-MEASURE (operator-eye bisection) — the fallback content was TRACK-INCONSISTENT: the
  vibration re-injection path.** v1 collapsed only `warp_result`'s base; `blend_result` — the fallback
  content of the UNCONDITIONAL soft/hard final selection (`result = mix(blend_result, warp_result, w)`
  soft / the binary keep hard) — stayed the un-warped (1−t)/t crossfade of the two reals. Every
  low-confidence block (the ball's silhouette + textureless interior) fell back to a crossfade of two
  positions ~22px apart, per-block per-phase = the A/B crossover vibration re-entering through the
  fallback. Bisected by the operator's eye: `--single-track` vibrated even with ALL optional layers
  disabled (`--no-commit --no-commit-default --no-member-commit --occl-thresh 0 --no-matte
  --no-phase-anchor --no-soft-gate --no-onepos --no-obj-crescent --no-stasis --no-inertia --no-crescent
  --no-appearance --no-travel --no-contour --no-ambig --no-rescue --no-band-xfade --no-fill-div
  --no-vblend --no-bg-snap`), while `--blend-solo 2` — which bypasses the selection entirely — stayed
  smooth on the same build. v1 FIX: under `single_track>0.5` the `blend_result` BASE becomes pure
  `cur[uv]` (track-consistent; precedent — the occl-winner branch already replaces `blend_result` with
  pure cur/prev reals). **v1 WAS NECESSARY BUT NOT SUFFICIENT** — the operator's eye still saw the
  vibration ("looks like a crossfade"): STATIC cur on MOVING content still positionally disagrees with
  the warped track at intermediate phases. The full cure is the v2 LAW recomposition (§3.4): forced-warp
  final selection + the commit/ts-smooth gates + the injector audit. The v1 pure-cur form is KEPT as
  defense-in-depth (blend_result is now a dead value under single-track). OFF path keeps the plain
  crossfade (byte-identical).
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

---

## 9. Known residuals (operator-observed; next iteration — neither a blocker)

Both observed by the operator's eye on the shipping default (v3.1 + reclaim 4.0); both accepted for
this release.

- **(a) Tile-quantized reclaim boundaries.** The reclaim's damping decision inherits the 8px MV-block
  granularity (the `nonconf`/`bg_like` signature is evaluated on the per-pixel *sampled* MV, but the
  underlying pollution and the model conformance change block-wise), so the residual gravity snaps to
  block edges instead of fading smoothly. **Candidate fix:** a per-pixel / bilinear reclaim weight —
  blend the damp weight `w` across the 4 MV-grid corners (the same footprint the fetch already reads)
  so the reclaim boundary is sub-tile smooth.
- **(b) Leading-edge sticky lines.** Tiles the ball is ENTERING are genuinely ambiguous: the object is
  partially present in the tile's content, so the "background-like" test (`bg_like` — the model
  explains the pixel better than the local MV) correctly FAILS → no reclaim → the tile carries the
  object MV and background lines inside it travel briefly until the object fully owns the tile.
  **Candidate fixes:** temporal hysteresis (a tile that was background last pair needs stronger
  evidence to adopt an object MV), or an iGPU contour-field assist (the Sobel band marks the true
  silhouette sub-tile — gate the reclaim's bg-side by it, the same signal `bg_snap` reads).

---

## 10. v3.2 — the screen-static evidence re-admission (the HSR HUD-ghost fix)

### 10.1 The field report (verbatim) and the convicted mechanism

HSR field result on the shipping v3.1 + bg-reclaim, relayed 2026-07-03:

> moving objects MUCH better (operator), BUT screen-static overlays regressed — the HUD (and
> semi-static character) self-ghost toward the motion, "an inverse crossfade"; WORSE on translucent
> elements, but solid HUD shows it too.

**Root cause (two mechanisms, both from the same blind spot):** under camera pan the gme model IS the
camera motion, so a **screen-fixed overlay is gme-NONCONFORM** — a hypothesis neither v3.1 mechanism
could represent:

- **(a) The B-track warp displaces HUD pixels whose tile MV is polluted by the moving background**
  (the matcher block straddles HUD/world; v3.1's stasis re-admission is BLOCK-level `sad_zero`, so
  exactly those straddling/translucent blocks fail the proof and follow the wrong MV).
- **(b) bg-reclaim actively damps HUD tiles TOWARD the camera model** — the reclaim's two-way contest
  (`d_loc` vs `d_mod`) was designed for the object-fringe case; a translucent/boundary HUD tile whose
  *visible majority* is background reads "the model explains it better" and gets snapped to camera
  motion: for a screen-static overlay the model is exactly wrong. The operator's report is the (b)
  signature amplified by (a); translucency is worst because the background component legitimately
  votes for the camera.

### 10.2 The bench reproduction (now permanent in the zoo)

`tools/ball_zoo.ps1 -PanPx S`: the background lattice scrolls left at S px/s (camera-pan analog; the
96px-periodic lattice pans via a cached W+96 tile) while a **screen-fixed HUD** stays put: an opaque
panel with white strokes at `rect(10,10,290,64)`, a translucent panel at `rect(430,600,420,90)`, the
center crosshair. Repro: `ball_zoo -Fps 60 -PanPx 300` (+ the default ball). Metrics
(`hud_ghost.py`, session scratchpad): **m2** = mean |live − real| inside the opaque rect (HUD is
static across the pair, so the real IS ground truth there — crispness); **m3** = glass-tint px in the
40px band OUTSIDE the translucent rect (the translucent ghost); **m4** = panel-gray px in the band
OUTSIDE the opaque rect (the solid-edge ghost); crosshair centroid sd; ball centroid ratio-vs-phase
(the G2 motion witness). NOTE: at 300 px/s / 60 fps the per-pair displacement is 5px, so the
coordinator's literal white-stroke-outside-rect count (**m1**) is structurally ~0 in every config (the
text ghosts land INSIDE the rect); m2/m3/m4 carry the signal.

### 10.3 The design (all evidence, no new hard threshold, no new push field)

**The law stands; the PROOF broadens.** Unwarped real content may still paint only where staticity is
proven — v3.2 replaces the block-level proof with per-pixel **three-hypothesis evidence** at two sites:

1. **The v3 override site** (`wap_warp.comp` final override): `d_zero = |cur[uv] − prev[uv]|` (the
   screen-static hypothesis; 2 same-uv taps — the per-pixel/soft form the block `sad_zero` cannot
   give, which is what translucent HUD needs) vs `d_warp = d_pixel = |A_samp − B_samp|` (the warp
   hypothesis — **REUSED**, already computed for onepos/commit; zero new taps). Winner-with-margin:
   `w_s = smoothstep(1.2, 3.0, (d_warp+0.02)/(d_zero+0.02))` (the `bg_like` ratio idiom; translucent
   pixels get intermediate `w_s`), block-proven `stasis` saturates `w_s = 1`, then
   `result = mix(B_samp, cur[uv], w_s)` — **ONE unified mix; the v3.1 strict re-admission is its
   `w_s=1` extreme (unified, not stacked)**. Where `mv≈0` the evidence is inert by construction
   (`B_samp == cur[uv]`, ratio = 1); on moving content `d_zero` is high by construction → `w_s→0`.
2. **The bg-reclaim exemption** (the reclaim block): the same `d_zero` (2 taps, only inside
   `nonconf>0` — the existing budget discipline) joins the `d_loc`/`d_mod` contest.
   `st_over_model = smoothstep(1.2,3.0,(d_mod+0.02)/(d_zero+0.02))` arbitrates model-vs-zero; the damp
   TARGET slides `model_mv → vec2(0)` and the evidence slides `bg_like → zero_like`, so a
   screen-static pixel with a polluted MV is damped toward **ZERO** (the overlay stops traveling)
   instead of toward the camera. **When the model ≈ 0 (static camera) `d_mod ≡ d_zero` → ratio 1 →
   the v3.1 reclaim is preserved BY CONSTRUCTION** — the non-pan G3 anchor.

Encoding: the existing `single_track` push float, 1.0 now = v3.2 (2.0 stays v3.0); `--no-stasis`
removes only the block-proof saturation. OFF paths untouched (all new code inside the
`single_track>0.5 && <1.5` and `bg_reclaim>0.001 && nonconf>0` branches).

**Known limit (documented, accepted):** content moving EXACTLY one texture period per pair aliases
`d_zero ≈ 0` — the same two-frame ambiguity the SAD matcher has (the ambig rule is its MV-side
counterpart). Not exercised at 300px/s·60fps (5px/pair vs the 24px period).

### 10.4 Gates (measured 2026-07-03, this session, FRESH zoo per run, 60 qdump triples each)

**G1 — pan bench HUD-ghost** (`ball_zoo -Fps 60 -PanPx 300`, defaults vs references):

| Metric (mean over 60 frames) | old-world (`--no-single-track --no-bg-reclaim`) | v3.1 (pre-change HEAD) | **v3.2 (×2 runs)** | floor (reals) |
|---|---|---|---|---|
| m3 glass-ghost band px | 5.6 (max 93) | **134.5** (max 410) | **14.9 / 16.6** (max 89) | 5.4–6.8 |
| m4 panel-ghost band px | 81.1 (max 597) | **321.4** (max 875) | **125.1 / 106.8** (max 779/735) | 0 |
| m2 opaque-rect MAD | 0.203 | 0.092 | **0.054 / 0.055** | — |
| crosshair centroid sd (x px) | 1.71 | 1.90 | **0.58 / 1.20** | — |

m3 closes ~92% of the v3.1 regression (134.5 → 15.7 mean vs the 5.6 baseline); m4 closes ~85%
(321 → 116 vs 81 — and the old-world class itself swings: a fresh old-world run on the v3.2 build
measured m4 105.4, m2 0.423). Solid HUD reads crisper than EITHER predecessor (m2 best-of-three), the
crosshair steadiest. **PASS (near-baseline; residuals honest below).**

**G2 — the moving world stays single-track-smooth:** ball ratio-vs-phase corr 0.969/0.973 (v3.1:
0.963; old-world: 0.967), span/npx preserved; the panned lattice tracks phase (low-t: |live−prev| 3.41
< |live−next| 6.25; high-t: 2.41 > 6.49 — NOT pinned to cur; v3.1 measures the same structure with
LARGER residuals 5.84/4.73, i.e. v3.2 also cleaned spurious damping noise). `d_zero` loses everywhere
the content moves, as constructed, and the evidence verified it. **PASS.**

**G3 — non-pan regression** (`ball_zoo -Fps 15`, defaults): outside-gold% v3.1 0.343 → v3.2 0.291
(same-day, same-harness pair — no regression); ratio-vs-phase corr 0.880 → 0.995, ratio climbing 0.15→
1.00 in both; span_live/prev 127/119 both; npx_live/prev ≈ 11.4k/11.25k both. NOTE the absolute
outside-gold of BOTH same-day runs sits above the §7.1 v3.1 cluster (0.127–0.185) and inside the
documented baseline swing (0.29–0.65) — the bench's known run-to-run variance (§8 caveat), not a
v3.2 effect (the v3.1 comparator was measured the same hour on the same build lineage). **PASS.**

**G4 — cadence CSV / OFF-identity / cost / build:** 15fps `--csv` intact (headers byte-identical
pre/post, 2876 vs 2878 rows over 12.0s, 0 drops, stats file headers identical). OFF-flags run on the
v3.2 build: banner carries NO single-track/bg-reclaim markers; metrics land in the old-world class
(m3 8.4, m4 105.4); structurally byte-identical (all new code branch-gated). Warp cost: pan v3.1
3.86ms → v3.2 3.99ms (**+0.13ms ≤ +0.5 gate**); non-pan 0.78ms (the pre-change 4.10ms reading was
present-blocking noise — the `warp ms` stat wraps warp+present and vsync-quantizes to ~4.16ms when the
present blocks; slip 4.29 vs 0.00 fingerprints it). All runs 240.4–241.0 fps, `er=0`, `to=0`,
`dd_lost=0`, no device-lost. Build clean (pre-existing warnings only, none in touched files).
**PASS.**

### 10.5 Honest anomalies / residuals

- **m4 residual ~35px above the old-world mean** (116 vs 81): panel-EDGE tiles remain partially
  ambiguous (half HUD / half moving lattice — the same class as §9(b) leading-edge tiles). The §9(b)
  candidates (temporal hysteresis, contour-field assist) apply here too.
- **m3 residual ~10px above floor:** translucent-edge pixels where the glass tint's share of the
  evidence is genuinely intermediate — `w_s` partial by design (soft, no hard threshold).
- **The operator's eye is the final gate** — the bench closes the metric; HSR + pan-bench eye-tests
  decide the ship verdict on v3.2's look (specifically: the translucent panel's interior now shows the
  world moving through it at the camera rate with the tint/text pinned — the physically correct
  single-layer rendering, but a look the operator has not yet judged).
- **`warp ms` conflation:** the stats-line `warp` wraps `wap_warp_present` (warp + present); under
  blocking presents it quantizes to the 4.16ms vblank. The §7.4 "0.38–0.50ms" figures and today's
  pan ~3.9–4.0ms are therefore not directly comparable; the honest cost signal is the same-bench
  delta (+0.13ms) and the shader-side structure (2 extra same-uv taps per single-track pixel; 2 extra
  taps inside the reclaim's `nonconf>0` branch).
