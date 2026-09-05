// shaders/layers/vblend.glsl — SAMPLE rank 110: velocity-continuity tilt of the A/B SAMPLE offsets
// (wap_warp.comp:507-514, verbatim). DEFAULT ON. Its return flows ONLY into fg_sample()'s mv argument
// (the SAMPLE stage), never back into CH_MV — the invariant that was a comment at :504-506 is now the
// stage. PREDICT (default): u_mv_target holds the PREV pair's MV → 2*mv - mv_prev; EXACT: the real
// next-pair MV.
// Made with my soul - Swately <3
vec2 pfg_sample_vblend(vec2 mv_in, in LayerCtx ctx) {
    const float vb_w        = smoothstep(VBLEND_t0, 1.0, ctx.t) * VBLEND_strength;    // :507
    const vec2  mv_ut       = texture(u_mv_target, ctx.uv).xy;                          // :511
    const vec2  mv_target_s = (VBLEND_exact != 0u) ? mv_ut                              // :512 EXACT
                                                   : (2.0 * mv_in - mv_ut);             // :513 PREDICT
    return mix(mv_in, mv_target_s, vb_w);                                               // :514
}
