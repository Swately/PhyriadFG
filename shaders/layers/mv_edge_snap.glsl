// shaders/layers/mv_edge_snap.glsl — MVCOND rank 5: the cross-bilateral (edge-aware sub-block) primary
// MV fetch (wap_warp.comp:319-323). DEFAULT OFF. Its presence disables mv_guided's call (the registry's
// `excludes` on rank 10 = today's `else`).
// The guidance band: the legacy packs `variant + sim` into one float and recovers sim = fract(v), which
// shifts the band by up to 1 ulp; here the band is the exact UBO value. The "0 = use --mv-sim" rule is a
// CONTROL-plane cascade resolved host-side into this row's own `sim` slot (layer_params_fill), so the
// body reads only its own parameters — no cross-row read.
// Made with my soul - Swately <3
vec2 pfg_mvcond_mv_edge_snap(vec2 mv_in, in LayerCtx ctx) {
    const bool es_dis = (MV_EDGE_SNAP_variant == 1);   // variant 1 (dissidence class) vs 2 (color class)
    return edge_snap_mv(ctx.uv, MV_EDGE_SNAP_sim, es_dis);
}
