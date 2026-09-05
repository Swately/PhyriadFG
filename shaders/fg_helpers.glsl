// shaders/fg_helpers.glsl — the MV-source helpers of the fused kernel, VERBATIM from shaders/wap_warp.comp
// (guided_mv 179-202, edge_snap_mv 228-265, gme_model_mv 272-277 at the time of the R3 extraction; comments included).
// They read the same samplers by the same names and the gme model from the fg_core push (pc.gme_a..f).
// The M4 veto binds them to those lines: a change here is a product change, never a refactor.
//
// Made with my soul - Swately <3

// Bilateral (color-membership) PRIMARY MV fetch. The MV texture is sampled with LINEAR filtering,
// so a rim pixel straddling the MV-grid boundary between a moving object's cell and the background
// cell gets a MIXED MV -- wrong for both. Under mv-guided we choose a SINGLE corner by color
// membership instead of blending the two.
//
// LINEAR-equivalence: texture(samp, uv) with LINEAR filtering on a texture of size G uses the
// continuous texel coord s = uv*G - 0.5 and blends the four integer corners floor(s)..+(1,1) with
// weights fract(s); each texel i has its centre at uv (i+0.5)/G. We texelFetch exactly those four
// corners (clamped), so the candidate set IS the bilinear footprint; when no corner wins decisively
// we RETURN the bilinear texture() result, so the OFF path is byte-identical.
//
// Selection: c_pix = cur_real at uv; for each corner sample its block-centre color, score
// max-channel |c_pix - c_corner|; the winner is the min-score corner. Fall back to bilinear when
// none is similar (best corner > sim_thresh) or on a near-tie (two best corners within sim_thresh --
// the pixel spans one color object across both cells, picking one would inject a discontinuity).
vec2 guided_mv(vec2 uv, float sim_thresh) {
    const vec2  grid     = vec2(textureSize(u_motion_vectors, 0));
    const vec2  inv_grid = 1.0 / grid;
    const ivec2 mxc      = ivec2(grid) - ivec2(1);
    const vec2  s        = uv * grid - vec2(0.5);          // bilinear continuous texel coord
    const ivec2 i00      = ivec2(floor(s));                // floor corner (bilinear footprint)
    const ivec2 corn[4]  = ivec2[4](i00, i00 + ivec2(1,0), i00 + ivec2(0,1), i00 + ivec2(1,1));
    const vec3  c_pix     = texture(u_cur_real, uv).rgb;   // the pixel's own membership color
    int   best_i = 0; float best_d = 1e9; float second_d = 1e9;
    for (int k = 0; k < 4; ++k) {
        const ivec2 ci  = clamp(corn[k], ivec2(0), mxc);
        const vec3  c_c = texture(u_cur_real, (vec2(ci) + vec2(0.5)) * inv_grid).rgb;
        const vec3  ad  = abs(c_pix - c_c);
        const float dmx = max(ad.r, max(ad.g, ad.b));      // max-channel membership distance
        if (dmx < best_d)      { second_d = best_d; best_d = dmx; best_i = k; }
        else if (dmx < second_d) { second_d = dmx; }
    }
    // none similar (best corner still > band) OR near-tie (two corners within band of each
    // other) → bilinear is the correct answer. Otherwise commit to the winning corner's MV.
    if (best_d > sim_thresh || (second_d - best_d) <= sim_thresh) {
        return texture(u_motion_vectors, uv).xy;           // the standard LINEAR result
    }
    return texelFetch(u_motion_vectors, clamp(corn[best_i], ivec2(0), mxc), 0).xy;
}

// CROSS-BILATERAL (edge-aware) PRIMARY MV fetch — the coherent-blur frontier fix. guided_mv above
// makes a HARD single-corner pick (or falls fully back to bilinear on a near-tie); it fixes TILE-
// level selection but cannot take a WEIGHTED combination biased toward the object side. The convicted
// defect lives at SUB-TILE sample time: a boundary output pixel's plain bilinear over the 4 MV texels
// mixes object-MV with background-MV(~0), so the effective MV is DILUTED -> the rendered edge advances
// short/unevenly (measured: ~constant px error, fatal at 120fps where the whole per-frame step is
// small). The cure is a joint-bilateral upsample of the MV field: keep the 4 bilinear SPATIAL weights
// (LINEAR-equivalence in the flat case) but MULTIPLY each corner by a GUIDANCE similarity term so an
// object-edge pixel draws its MV from the object-side texels continuously (no hard flip, no seam).
//
//   variant G1 (dissidence-class, PREFERRED): the R8 dissidence byte (binding 6, the prev-anchored
//     object silhouette) is the direct object/background discriminant. guide_k = gaussian(|dis_pix -
//     dis_corner| / sig): a corner whose silhouette class matches this pixel's keeps full weight; a
//     cross-class corner is suppressed. Needs the mask valid (matte/gme on) — the host passes G1 only
//     then, else it sends G2.
//   variant G2 (color, joint-bilateral): guide_k = gaussian(|c_pix - c_corner| / sig) over cur_real,
//     the classic joint-bilateral upsampling guide (same membership signal guided_mv scores).
//
// DEGENERATE GUARD: if every guidance weight collapses (all corners cross-class / w_sum ~ 0) the
// bilateral is undefined -> RETURN plain bilinear (never NaN, never a 0/0). ENDPOINTS: this only
// selects the vector; the A/B taps still present the reals byte-exact at t=0/t=1 (mv scales by t /
// (1-t) at the taps), so endpoint guarantees are untouched. sig = sim_thresh (the color/dissidence
// band already carried in the flag) sets the guidance falloff; a wider band -> softer (closer to
// plain bilinear), a tighter band -> harder edge snap.
vec2 edge_snap_mv(vec2 uv, float sim_thresh, bool use_dis) {
    const vec2  grid     = vec2(textureSize(u_motion_vectors, 0));
    const vec2  inv_grid = 1.0 / grid;
    const ivec2 mxc      = ivec2(grid) - ivec2(1);
    const vec2  s        = uv * grid - vec2(0.5);          // bilinear continuous texel coord
    const vec2  f        = fract(s);                       // bilinear spatial fractions
    const ivec2 i00      = ivec2(floor(s));               // floor corner (bilinear footprint)
    const ivec2 corn[4]  = ivec2[4](i00, i00 + ivec2(1,0), i00 + ivec2(0,1), i00 + ivec2(1,1));
    // the 4 bilinear SPATIAL weights (sum to 1) — the LINEAR footprint we re-weight by guidance
    const float sp[4]    = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y),
                                    (1.0 - f.x) *        f.y,  f.x *        f.y);
    // this pixel's own guidance signal: the dissidence class (G1) or the membership color (G2)
    const float dis_pix  = use_dis ? texture(u_dissidence, uv).r : 0.0;
    const vec3  c_pix    = use_dis ? vec3(0.0)                    : texture(u_cur_real, uv).rgb;
    // gaussian falloff: guide = exp(-d^2 / (2*sig^2)); sig = the sim band (floored so it never blows up)
    const float sig      = max(sim_thresh, 1e-3);
    const float inv2s2   = 1.0 / (2.0 * sig * sig);
    vec2  mv_acc = vec2(0.0);
    float w_sum  = 0.0;
    for (int k = 0; k < 4; ++k) {
        const ivec2 ci = clamp(corn[k], ivec2(0), mxc);
        const vec2  cc = (vec2(ci) + vec2(0.5)) * inv_grid;   // corner block-centre uv
        float d;
        if (use_dis) {
            d = abs(dis_pix - texture(u_dissidence, cc).r);            // G1: dissidence-class distance
        } else {
            const vec3 ad = abs(c_pix - texture(u_cur_real, cc).rgb);
            d = max(ad.r, max(ad.g, ad.b));                           // G2: max-channel color distance
        }
        const float guide = exp(-d * d * inv2s2);                     // 1 = same class → 0 = far class
        const float w      = sp[k] * guide;                          // spatial × guidance
        mv_acc += w * texelFetch(u_motion_vectors, ci, 0).xy;
        w_sum  += w;
    }
    // DEGENERATE: guidance annihilated every corner (all cross-class) → bilateral undefined → plain bilinear.
    if (w_sum < 1e-5) return texture(u_motion_vectors, uv).xy;
    return mv_acc / w_sum;                                            // the edge-aware sub-block MV
}

// Evaluate the global affine motion model at this pixel's mv-grid position. The CPU fits
// mv_model(gx,gy) = (a + b*gx + c*gy, d + e*gx + f*gy) over integer grid indices (gx,gy) in
// [0,G.x)x[0,G.y), G = MV-grid size. texture() maps uv to the continuous texel coord s = uv*G - 0.5,
// which equals the integer index at a texel centre, so evaluating the model at gx = uv*G - 0.5
// reproduces the grid position the CPU fit used. Result in PIXEL units, used like a sampled MV.
vec2 gme_model_mv(vec2 uv) {
    const vec2 grid = vec2(textureSize(u_motion_vectors, 0));
    const vec2 g    = uv * grid - vec2(0.5);          // bilinear texel coord = grid index at centres
    return vec2(pc.gme_a + pc.gme_b * g.x + pc.gme_c * g.y,
                pc.gme_d + pc.gme_e * g.x + pc.gme_f * g.y);
}
