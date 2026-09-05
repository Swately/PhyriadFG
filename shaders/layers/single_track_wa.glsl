// shaders/layers/single_track_wa.glsl — WEIGHT rank 190: single_track's re-weight of the core blend
// (wap_warp.comp:678 `wa_eff = single_track ? 0.0 : wa`). requires single_track. This was the first of
// single_track's four `shadows` (SH_WA_EFF_ZERO); with the core split it is an ordinary WEIGHT row and
// the wart dissolves into the schema (COLUMN_CLOSURE_EXPERIMENT.md §2.1).
// Made with my soul - Swately <3
float pfg_weight_single_track_wa(float wa, in LayerCtx ctx) {
    return 0.0;   // :678 — the blend collapses to the B-track
}
