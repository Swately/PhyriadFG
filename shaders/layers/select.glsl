// shaders/layers/select.glsl — COMPOSE rank 260: the warp-vs-blend select (wap_warp.comp:1177-1184, the
// HARD binary path). DEFAULT ON, not user-toggleable. Reads CH_WARP_OK and CH_BLEND (COLUMN_CLOSURE §2.2:
// the second accumulator is a channel and the select is a row). `pick_warp = (good_match && good_improv
// && sources_agree) || member_strength > 0.5` with member_strength = 0 (no matte row) = ctx.warp_ok.
// ENVELOPE: the legacy default ALSO carries soft_gate/commit_default/multicand variants of this
// selection (:1089-1185); under the shipping default (single_track ON) every variant is discarded by the
// rank-290 override, so none reaches the output. This row reproduces the hard path only.
// Made with my soul - Swately <3
vec4 pfg_compose_select(vec4 c_in, in LayerCtx ctx) {
    return ctx.warp_ok ? c_in : ctx.blend;   // :1184
}
