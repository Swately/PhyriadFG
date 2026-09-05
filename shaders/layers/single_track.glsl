// shaders/layers/single_track.glsl — COMPOSE rank 290: single-track synthesis v3.2 (wap_warp.comp:1321-1329,
// verbatim). DEFAULT ON. Declared OVERRIDE: c_in is deliberately discarded — the composite base becomes
// the B-track (cur warped back to phase t) plus the per-pixel SCREEN-STATIC evidence mix; the block-
// proven stasis (CH_STASIS, a declared read) saturates the mix. --st-no-stasis = v3.0: pure B_samp.
// The legacy `pc.single_track < 1.5` float compare becomes an exact uint test.
// Made with my soul - Swately <3
vec4 pfg_compose_single_track(vec4 c_in, in LayerCtx ctx) {   // OVERRIDE: c_in is deliberately discarded
    vec4 r = ctx.B_samp;                                       // :1322 the known-smooth B-track base
    if (SINGLE_TRACK_no_stasis == 0u) {                        // :1323 v3.2: screen-static evidence mix
        const vec4  cur0   = texture(u_cur_real, ctx.uv);                                   // :1324 the unwarped real (the mix target)
        const float d_zero = length(cur0.rgb - texture(u_prev_real, ctx.uv).rgb);           // :1325
        float w_s = smoothstep(1.2, 3.0, (ctx.d_pixel + 0.02) / (d_zero + 0.02));           // :1326
        if (ctx.stasis_block) w_s = 1.0;                                                    // :1327 block-proven static = the w_s=1 extreme
        r = mix(ctx.B_samp, cur0, w_s);                                                     // :1328
    }
    return r;
}
