// shaders/layers/mv_guided.glsl — MVCOND rank 10: color-guided MV assignment (wap_warp.comp:324-326 →
// guided_mv(), :179-202). DEFAULT ON. Picks ONE MV-grid corner by color membership; falls back to the
// LINEAR fetch when the membership is ambiguous.
// XR1: the legacy packs mv_guided = 1.0 + sim and recovers sim = mv_guided - 1.0 (:325), which is NOT
// exactly sim in binary32. The host reproduces that packed value in the UBO by default
// (layer_params_fill, --fg-core-clean-sim flips to the exact value) so M4 byte-identity is provable
// first and the clean value measured second, both on record.
// Made with my soul - Swately <3
vec2 pfg_mvcond_mv_guided(vec2 mv_in, in LayerCtx ctx) {
    return guided_mv(ctx.uv, MV_GUIDED_sim);
}
