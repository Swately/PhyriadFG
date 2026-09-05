// shaders/layers/bg_reclaim.glsl — MVCOND rank 30: the tile-scale gravity fix (wap_warp.comp:358-396,
// verbatim). DEFAULT ON at 4.0. arm = GME (the model must be valid THIS generation); the legacy gate
// `bg_reclaim > 0.001 && gme_on > 0.5` is the spec constant AND the arm bit.
// XR7 (A3_CHOSEN_DESIGN §3): under the shipping default this damp is DEAD — phase_anchor (rank 40)
// rebases on the rank-25 snapshot ctx.mv_raw_fwd and discards it whenever the bwd field is valid.
// Reproduced bug-for-bug; the FIX is a separate operator decision, never folded in here.
// Made with my soul - Swately <3
vec2 pfg_mvcond_bg_reclaim(vec2 mv_in, in LayerCtx ctx) {
    vec2 mv = mv_in;
    const vec2  model_mv = gme_model_mv(ctx.uv);
    const float nonconf  = smoothstep(2.0, 6.0, length(mv - model_mv));   // object-like MV: |mv−model| in px
    if (nonconf > 0.0) {
        const vec2  sz_r  = ctx.out_size;
        const vec3  A_l   = texture(u_prev_real, ctx.uv - (mv       * ctx.t)         / sz_r).rgb;   // local-mv warp
        const vec3  B_l   = texture(u_cur_real,  ctx.uv + (mv       * (1.0 - ctx.t)) / sz_r).rgb;
        const vec3  A_m   = texture(u_prev_real, ctx.uv - (model_mv * ctx.t)         / sz_r).rgb;   // model warp
        const vec3  B_m   = texture(u_cur_real,  ctx.uv + (model_mv * (1.0 - ctx.t)) / sz_r).rgb;
        const float d_loc = length(A_l - B_l);   // HIGH if the local mv is wrong for this (bg) pixel
        const float d_mod = length(A_m - B_m);   // LOW  if the model mv fits (it IS background)
        const float bg_like = smoothstep(1.2, 3.0, (d_loc + 0.02) / (d_mod + 0.02));
        // v3.2 SCREEN-STATIC EXEMPTION (the HSR HUD-ghost fix): the ZERO-motion hypothesis (:378-395).
        const float d_zero  = length(texture(u_cur_real, ctx.uv).rgb - texture(u_prev_real, ctx.uv).rgb);
        const float st_over_model = smoothstep(1.2, 3.0, (d_mod + 0.02) / (d_zero + 0.02));
        const float zero_like     = smoothstep(1.2, 3.0, (d_loc + 0.02) / (d_zero + 0.02));
        const vec2  target = mix(model_mv, vec2(0.0), st_over_model);
        const float evid   = mix(bg_like, zero_like, st_over_model);
        const float w      = clamp(BG_RECLAIM_strength * nonconf * evid, 0.0, 1.0);
        mv = mix(mv, target, w);   // hard snap when strength·band saturates; soft LSFG-style decay otherwise
    }
    return mv;
}
