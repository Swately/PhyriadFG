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
        // the two edges are this row's params since 2026-09-11; their DEFAULTS are :1326's literals
        // (1.2, 3.0), so the shipping value of this expression is unchanged. They are exposed because
        // this ramp is the only warp-vs-hold decision left in the path: Gate 1 / Gate 2 reach `select`
        // at rank 260 and are discarded by the two OVERRIDE rows after it.
        // The two edges of the screen-static ramp. DEFAULT 1.0 / 1.6 since 2026-09-11; they were 1.2 / 3.0,
        // the value wap_warp.comp:1326 carried. Measured live across five corpora, matched frame-for-frame
        // against exact truth, two captures per arm at k = 8 agreeing to 0.4 points on a +-2 % noise floor:
        // 13.4 px/pair -21 % position and -38 % hallucinated mass on the spinning box and -23 % / -35 % on
        // the sphere; 6.7 px/pair -14 % / -26 % on the box; 3.4 px/pair mixed; 1.7 px/pair a LOSS of +7.8 %,
        // in the x0.5 regime the gate already treats as below its own floor. The static panel improves
        // 50-93 % everywhere. --st-hold-lo 1.2 --st-hold-hi 3.0 restores the old value exactly.
        //
        // A REFUTED design lived here for one afternoon and was removed: making the edges slide with the
        // LOCAL displacement (length(ctx.mv_raw_fwd)) measured worse than the constant everywhere (+15.5 %
        // on sc_live k=4 against the constant's -1.7 %), because the spinning box barely TRANSLATES in any
        // corpus (~2 px/pair) -- the object carrying the damage never crossed the threshold. Displacement
        // magnitude does not predict the damage; the vector being WRONG does, and this row cannot know that.
        // It was removed rather than left inert because declaring CH_MV_RAW_FWD as a read of this row moved
        // the output even with the slide disarmed (sc_v4 sharp 0.8938 -> 0.9032 at identical edges), so the
        // declaration was not free. The finding is in docs/LEARNING_LOG.md, not in a dead parameter.
        float w_s = smoothstep(SINGLE_TRACK_hold_lo, SINGLE_TRACK_hold_hi,
                               (ctx.d_pixel + 0.02) / (d_zero + 0.02));                   // :1326
        if (ctx.stasis_block) w_s = 1.0;                                                    // :1327 block-proven static = the w_s=1 extreme
        r = mix(ctx.B_samp, cur0, w_s);                                                     // :1328
    }
    return r;
}
