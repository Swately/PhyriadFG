// shaders/layers/inertia.glsl — MVCOND rank 20: the inertia prior (wap_warp.comp:304-306 the persistence
// tap, :332-334 the gate). DEFAULT ON at 0.50. requires ANY of {mv_guided, mv_edge_snap} — the chain's
// `needs` (req_any) is today's `(pc.mv_guided > 0.5 || pc.mv_edge_snap > 0.5)` predicate.
// A block with a SUSTAINED static history (p >= thresh) refuses a fast corner lent by the guided /
// edge-snap fetch and falls back to the plain LINEAR result. The tap moved from above the fetch to
// inside the body: same sampler, same uv, same value (Candidate C §4.2).
// Made with my soul - Swately <3
vec2 pfg_mvcond_inertia(vec2 mv_in, in LayerCtx ctx) {
    const float inertia_p      = texture(u_persistence, ctx.uv).r;   // :305
    const bool  inertia_static = (inertia_p >= INERTIA_thresh);       // :306
    if (inertia_static && length(mv_in) > 2.0) {                      // :332
        return texture(u_motion_vectors, ctx.uv).xy;                  // :333 — the standard LINEAR result, no fast corner
    }
    return mv_in;
}
