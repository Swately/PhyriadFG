// shaders/layers/ambig.glsl — MVCOND rank 50: the AMBIGUITY rule (wap_warp.comp:426-444, verbatim).
// DEFAULT ON. arm = GME (the referee is the model). Runner-up arbitration of SAD ties: a tile is ALIASED
// when the runner-up is near-tied AND a real alternative; pick whichever of {mv, cand} sits nearer the
// smooth global expectation. The two magic numbers (1.15, 2.0 px) are this row's HIDDEN params
// (tie_ratio, min_sep_px) — same float32 bits as the literals.
// The mv-free OBJECT proxy (:433-437) reads the matte threshold; no matte row in R3 and matte_on = 0
// under the default → object_proxy = false, the identical value.
// Made with my soul - Swately <3
vec2 pfg_mvcond_ambig(vec2 mv_in, in LayerCtx ctx) {
    vec2 mv = mv_in;
    const vec4 cand    = texture(u_candidates, ctx.uv);                 // .xy = runner-up MV, .z = runner-up SAD
    const bool aliased = (cand.z <= AMBIG_tie_ratio * ctx.sad.x) && (length(cand.xy - mv) > AMBIG_min_sep_px);
    const bool object_proxy = false;                                    // :433-437 (no matte row → false)
    if (aliased && !object_proxy) {
        const vec2 referee = gme_model_mv(ctx.uv);                      // the smooth global background expectation
        mv = (distance(cand.xy, referee) < distance(mv, referee)) ? cand.xy : mv;
    }
    return mv;
}
