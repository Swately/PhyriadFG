// shaders/layers/stasis.glsl — COMPOSE rank 280: the stasis override (wap_warp.comp:1191). DEFAULT ON.
// Declared OVERRIDE: a block proven static (sad_zero <= thresh, evaluated at rank 45 into CH_STASIS)
// presents the real pixel directly, immune to every motion layer. The predicate lives in the rank-45
// pseudo-row so single_track (290) reads the SAME boolean (its w_s = 1 saturation, :1327).
// Made with my soul - Swately <3
vec4 pfg_compose_stasis(vec4 c_in, in LayerCtx ctx) {
    if (ctx.stasis_block) return texture(u_cur_real, ctx.uv);   // :1191
    return c_in;
}
