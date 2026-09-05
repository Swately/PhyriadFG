// shaders/layers/phase_anchor.glsl — MVCOND rank 40: the phase-anchored primary MV (wap_warp.comp:397-411).
// DEFAULT ON. arm = BWD (the bwd field valid this generation = the legacy `occl_thresh > 0.0`).
// At low t the forward field, at high t the cur-anchored backward field negated, blended by
// smoothstep(0.35, 0.65, t). It mixes from ctx.mv_raw_fwd (the rank-25 snapshot), NOT from mv_in — that
// is what makes bg_reclaim's damp dead under the default (XR7), reproduced here bug-for-bug.
// The MATTE claim term (:401-406) is a matte row's contribution; there is no matte row in R3 and
// matte_on = 0 under the default, so claim = 0.0 — the identical value, kept in the expression so the
// shape of :407 is preserved.
// Made with my soul - Swately <3
vec2 pfg_mvcond_phase_anchor(vec2 mv_in, in LayerCtx ctx) {
    const vec2  mv_bwd = texture(u_motion_vectors_bwd, ctx.uv).xy;   // :398 cur-anchored field (cur→prev), pixel units
    const float claim  = 0.0;                                          // :400 (no matte row → 0)
    const float w_b    = clamp(smoothstep(0.35, 0.65, ctx.t) + claim, 0.0, 1.0);   // :407
    return mix(ctx.mv_raw_fwd, -mv_bwd, w_b);                          // :408 phase(+claim)-anchored primary
}
