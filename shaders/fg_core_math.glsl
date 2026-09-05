// shaders/fg_core_math.glsl — THE CORE of PhyriadFG's frame generation, in isolation (stage 5, R3).
//
// This is the "~35 lines" A0 M2c names: the two displaced samples, the per-pixel disagreement, Gate 1,
// the (1-t, t) blend. Every line is the verbatim math of shaders/wap_warp.comp at the cited line — the
// M4 veto (byte-identical default output) binds this file to those lines. It has NO access to any layer
// parameter and NO access to the layer UBO (Candidate C §2): a layer that wants to change the core's
// inputs conditions `mv` (MVCOND/SAMPLE), re-weights `wa` (WEIGHT), or composes the output (COMPOSE).
//
// The core split (COLUMN_CLOSURE_EXPERIMENT.md §2.1): fg_sample() → [WEIGHT stage] → fg_blend(). Same six
// contract parameters, same math; two functions instead of one so a row may re-weight the A/B
// contribution between the samples and the blend without reaching into the core.
//
// The workgroup tile reduction (wap_warp.comp:523-528: s_d[64], barrier(), the mean) is NOT in here: it
// touches workgroup state and its barrier must sit at main()'s top level (uniform control flow), so
// fg_core.comp owns it and hands the result in as `tile_agrees`.
//
// Made with my soul - Swately <3

// ── the layer context (Candidate C §3.2) — assembled once by fg_core.comp before the chains run ─────
struct LayerCtx {
    vec2  uv;               // the sampling base (uv0; R3 has no UV stage, so uv == uv0 — wap_warp.comp:289 with cam_lead off)
    ivec2 coord;
    vec2  out_size;
    float t;                // the temporal phase (CorePush.t)
    vec2  mv_raw_fwd;       // CH_MV_RAW_FWD: the rank-25 snapshot (wap_warp.comp:344)
    vec2  sad;              // CH_SAD: .x = sad_best, .y = sad_zero (rank 45, wap_warp.comp:412-414)
    vec4  A_samp, B_samp;   // CH_A_SAMP / CH_B_SAMP: filled after fg_sample(); zero during MVCOND/SAMPLE
    float d_pixel;          // CH_D_PIXEL: |A_samp - B_samp| (wap_warp.comp:522)
    vec4  blend;            // CH_BLEND: the core's SECOND accumulator — the un-warped (1-t)/t crossfade
                            // (wap_warp.comp:694-695). COLUMN_CLOSURE §2.2: a channel, read by the select row.
    bool  stasis_block;     // CH_STASIS: published at rank 45, read by stasis (280) and single_track (290)
    bool  warp_ok;          // CH_WARP_OK: Gate 1 && Gate 2 (wap_warp.comp:497-498, 528)
};

// ── the core's sample half ───────────────────────────────────────────────────────────────────────
struct CoreSample {
    vec4  A;         // the A anchor (real N)  warped to phase t
    vec4  B;         // the B anchor (real N+1) warped to phase t
    float d_pixel;   // per-pixel source disagreement
    bool  gate1;     // Gate 1 (confidence)
};

// Gate 1 (confidence) — wap_warp.comp:497-498. No divide (cancels dither).
bool fg_gate1(vec2 sad, float residual_ceil, float improvement_frac) {
    const bool good_match  = (sad.x < residual_ceil);
    const bool good_improv = (sad.y * (1.0 - improvement_frac) > sad.x);
    return good_match && good_improv;
}

// The two displaced samples + the disagreement — wap_warp.comp:518-522, verbatim expression order
// (the division by out_size happens AFTER the multiply by t, exactly as the source writes it).
CoreSample fg_sample(sampler2D prev, sampler2D cur, vec2 uv, vec2 mv, float t, vec2 out_size, vec2 sad,
                     float residual_ceil, float improvement_frac) {
    const vec2 t_mv_uv     = (mv * t)         / out_size;   // :518
    const vec2 inv_t_mv_uv = (mv * (1.0 - t)) / out_size;   // :519
    CoreSample s;
    s.A       = texture(prev, uv - t_mv_uv);                 // :520
    s.B       = texture(cur,  uv + inv_t_mv_uv);             // :521
    s.d_pixel = length(s.A.rgb - s.B.rgb);                   // :522
    s.gate1   = fg_gate1(sad, residual_ceil, improvement_frac);
    return s;
}

// The blend half — wap_warp.comp:679. `wa` arrives from the WEIGHT stage (identity: 1 - t, :637).
vec4 fg_blend(float wa, vec4 A, vec4 B) {
    return wa * A + (1.0 - wa) * B;
}

// The second accumulator (CH_BLEND) — wap_warp.comp:694-695: the un-warped crossfade of the two reals.
vec4 fg_crossfade(sampler2D prev, sampler2D cur, vec2 uv, float t) {
    return (1.0 - t) * texture(prev, uv) + t * texture(cur, uv);
}

// What the compose chain starts from (the generator's `core.color`).
struct CoreOut { vec4 color; };
