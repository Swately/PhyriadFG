// shaders/layers/eco_anchor.glsl — MVCOND rank 42: the eco phase anchor (--eco-anchor {1|2}). DEFAULT OFF. arm = GME.
//
// The shipping phase_anchor (rank 40) replaces the forward MV by the negated backward field at high t; --eco does not
// compute the backward field, so without an anchor its t~0.75 frame loses quality (Q1b). This row re-selects the primary
// MV from the FORWARD field alone. Validated offline before it was written: docs/planning/records/ECO_ANCHOR_PREREG.md
// and ECO_ANCHOR_VALIDATION.md (the replay harness tools/scene_truth/anchor_replay.py, and tools/ref_warp.py's twin of
// this body, which the GPU is checked against).
//
//   w = smoothstep(0.35, 0.65, t). At w == 0 (slot 0) the row returns mv_in BEFORE any texture read: byte-identical
//   to --eco. It never reads binding 5 (the backward field).
//   mode 1 (FP1)  one fixed-point tap: q = F(x - t*mv_in); mix(mv_in, q, w).
//   mode 2 (lean BCR, the mode V2 selected) candidates {mv_in, the gme vector, F at 8 ring offsets of 8 px at 640
//     wide, scaled by W/640}. Early-out to mv_in when every ring vector is within 0.75 px of mv_in. A candidate c is
//     CONSISTENT iff min(|F(x - t c) - c|, |F(x - c) - c|) < 1 + 0.15|c|; its cost is the luma difference
//     |Y(prev at x - t c) - Y(cur at x + (1-t) c)| + 0.01 r. The cheapest consistent candidate wins, first in the
//     order above on a tie; mv_in is KEPT unless the winner costs less than hyst * mv_in's own cost (or mv_in is
//     inconsistent); with no consistent candidate the result is the gme vector (the fallback the validation traced
//     the sphere-missing cost to, at the loop seam and at an object's exit).
//   F = the W/8 forward field (binding 2), bilinear, pixel units; positions are in the output's pixel frame.
//   Luma is the dot of the bilinear RGB sample with (0.299, 0.587, 0.114) (the twin samples RGB then dots, as here).
//
// Made with my soul - Swately <3
const vec3 ECO_LUMA = vec3(0.299, 0.587, 0.114);

vec2 pfg_mvcond_eco_anchor(vec2 mv_in, in LayerCtx ctx) {
    const float w = smoothstep(0.35, 0.65, ctx.t);
    if (w <= 0.0) return mv_in;                                            // slot 0: --eco's own MV, no tap taken
    const vec2 sz = ctx.out_size;
    if (ECO_ANCHOR_mode == 1) {                                            // FP1
        const vec2 q = texture(u_motion_vectors, ctx.uv - (mv_in * ctx.t) / sz).xy;
        return mix(mv_in, q, w);
    }
    // mode 2: lean BCR
    const float r_px = 8.0 * sz.x / 640.0;
    const float r_d  = r_px / sqrt(2.0);
    const vec2 offs[8] = vec2[8](vec2( r_px, 0.0), vec2(-r_px, 0.0), vec2(0.0,  r_px), vec2(0.0, -r_px),
                                 vec2( r_d,  r_d), vec2( r_d, -r_d), vec2(-r_d,  r_d), vec2(-r_d, -r_d));
    vec2 cand[10];
    cand[0] = mv_in;
    cand[1] = gme_model_mv(ctx.uv);
    float spread = 0.0;
    for (int k = 0; k < 8; ++k) {
        cand[2 + k] = texture(u_motion_vectors, ctx.uv + offs[k] / sz).xy;
        spread = max(spread, length(cand[2 + k] - mv_in));
    }
    if (spread < 0.75) return mv_in;                                       // locally constant field: nothing to re-select
    float S0 = 0.0;
    bool  C0 = false;
    float best = 0.0;
    int   bi = -1;
    for (int k = 0; k < 10; ++k) {
        const vec2  c    = cand[k];
        const vec2  fa   = texture(u_motion_vectors, ctx.uv - (c * ctx.t) / sz).xy;
        const vec2  fb   = texture(u_motion_vectors, ctx.uv - c / sz).xy;
        const float r    = min(length(fa - c), length(fb - c));
        const bool  cons = r < 1.0 + 0.15 * length(c);
        const vec3  pa   = texture(u_prev_real, ctx.uv - (c * ctx.t) / sz).rgb;
        const vec3  pb   = texture(u_cur_real,  ctx.uv + (c * (1.0 - ctx.t)) / sz).rgb;
        const float S    = abs(dot(pa, ECO_LUMA) - dot(pb, ECO_LUMA)) + 0.01 * r;
        if (k == 0) { S0 = S; C0 = cons; }
        if (cons && (bi < 0 || S < best)) { best = S; bi = k; }
    }
    vec2 v;
    if (bi < 0)                                       v = cand[1];        // no consistent candidate: the gme vector
    else if (C0 && best >= ECO_ANCHOR_hyst * S0)      v = mv_in;          // hysteresis: mv_in is kept
    else                                              v = cand[bi];
    return mix(mv_in, v, w);
}
