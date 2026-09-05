// shaders/layers/fetch_mv.glsl — MVCOND rank 0: the primary MV source (wap_warp.comp:326).
// The mv-source parameter of the core contract (A0 M2c): a LINEAR (bilinear) fetch of the W/8 forward
// field. Swapping the source is a change to THIS body, never to the core. mv_in is unused (rank 0).
// Made with my soul - Swately <3
vec2 pfg_mvcond_fetch_mv(vec2 mv_in, in LayerCtx ctx) {
    return texture(u_motion_vectors, ctx.uv).xy;   // pixel units
}
