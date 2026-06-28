// framework/render/vulkan/include/phyriad/render/vulkan/OpticalFlowPipeline.hpp
// OpticalFlowPipeline — compute pipeline for motion-vector-guided frame
// interpolation, with a HIERARCHICAL (pyramid, coarse-to-fine) block-match.
//
// Stage 1 (hierarchical block-match): build a ~4-level image pyramid of A and B
//   (2× box downsample each), search a small ±R window at the COARSEST level
//   (a ±R window at scale 2^L reaches ±R·2^L full-res pixels → large motion
//   with a small radius), propagate the winning MV up and refine with a small
//   ±R at each finer level. The finest level's field is the RG16F texture
//   (input/8 in each axis) the warp consumes. This tracks LARGE displacements
//   at O(R²·levels) cost instead of a flat O(D²) search — and is empirically
//   MORE accurate than a full flat search (the pyramid imposes a multi-scale
//   smoothness prior; a huge flat window finds more spurious SAD minima).
//
// Stage 2 (warp): samples A and B at the t-weighted motion-vector offset each,
//   blends to produce the temporal-phase frame C (t∈(0,1), default 0.5).
//
// Compared to a single-tap 50/50 blend, this pass adds ghosting-free
// interpolation on translating motion at the cost of the pyramid images
// (A,B per level + an MV field per level) and the per-level search.
//
// Lifetime + threading: single-instance, not thread-safe; the descriptor-set
// ring rotates per dispatch.
//
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace phyriad::render::vulkan {

class OpticalFlowPipeline {
public:
    OpticalFlowPipeline()  noexcept = default;
    ~OpticalFlowPipeline() noexcept = default;

    OpticalFlowPipeline(OpticalFlowPipeline const&)            = delete;
    OpticalFlowPipeline& operator=(OpticalFlowPipeline const&) = delete;
    OpticalFlowPipeline(OpticalFlowPipeline&&)                 = delete;
    OpticalFlowPipeline& operator=(OpticalFlowPipeline&&)      = delete;

    // ── init ──────────────────────────────────────────────────────────
    // Creates both compute pipelines (block-match + warp), the sampler,
    // descriptor set layouts, descriptor pool sized for max_in_flight,
    // the intermediate motion-vector image at (width/8) × (height/8),
    // and the SAD field image (same size, RG16F — sad_best / sad_zero).
    //
    // search_radius: pixel-radius of the FINEST-level refinement window. The
    //   pyramid handles large displacement by construction (coarse levels), so
    //   this is the fine-detail / small-motion search at full resolution — the
    //   value that guarantees small isolated objects with small motion are
    //   tracked regardless of the coarse predictor. Reasonable values: 1..8;
    //   default 2 (near-lossless at low cost; the ±6×8=±48 px coarse reach
    //   covers D≤48). Large D is the pyramid's job, not this radius. Max 32.
    //
    // residual_ceil: confidence gate — maximum allowed sad_best (per-tile SAD at
    //   the winning MV, float [0,192]) for a tile to be motion-warped. Tiles
    //   whose match residual exceeds this are BLENDED (in-place 50/50) instead
    //   of warped, avoiding ghosting when the block-match failed. Default 32.0
    //   (~0.5 per pixel per channel mean error over an 8×8 tile × 3 ch).
    //
    // improvement_frac: confidence gate — minimum fraction of the in-place
    //   residual (sad_zero) that the match must explain. Gate (b): the difference
    //   (sad_zero − sad_best)/sad_zero > improvement_frac, equivalently
    //   sad_zero*(1 − improvement_frac) > sad_best (stable form, no divide).
    //   Default 0.5: the motion must reduce the per-tile error by at least 50%.
    //   Setting 0.0 passes any moving tile. Together with gate (a), static tiles
    //   (sad_zero ≈ sad_best ≈ noise) fail (b) regardless of the noise floor —
    //   the gate is noise-floor-invariant, no per-capture threshold needed.
    //
    // agreement_thresh: source-agreement gate — maximum allowed mean per-tile
    //   source disagreement d_tile = mean(length(A_samp−B_samp)) for a tile to be
    //   motion-warped. If d_tile ≥ agreement_thresh, the two warp sources straddle
    //   a motion boundary (e.g., a mixed HUD/background tile with a compromise MV)
    //   → BLEND instead. d uses bilinear-MV samples already fetched for the WARP
    //   result; the agreement check is a length() + shared-memory mean-reduction
    //   (8×8 tile). Units: raw length() in [0, √3≈1.733].
    //     0.05 ≈ 7/255  per channel (very tight)
    //     0.10 ≈ 15/255 per channel
    //     0.20 ≈ 29/255 per channel (default)
    //     0.40 ≈ 59/255 per channel (loose)
    //   Set ≥ 1.8 to disable (confidence-only behaviour). Default 0.20. Cost: the
    //   warp samples are fetched unconditionally (the barrier must be in uniform
    //   control flow), a small overhead over confidence-only.
    // emit_second_best: candidate (second-best / runner-up) field — opt-in, ADDITIVE, default OFF (every
    //   existing caller is byte-identical with it off). When OFF: the candidate image is NOT created, the
    //   matcher's binding 5 is bound to a 1×1 placeholder (so the descriptor layout stays valid) and the
    //   shader's emit_second push is 0 at every level → NOTHING is written there; the MV (motion_image)
    //   and SAD (sad_field_image) outputs are byte-identical. When ON: a second RGBA16F image (same size
    //   as the MV field, cand_image()) is created; the FINEST level of record_optical_flow writes, per
    //   tile, the RUNNER-UP motion vector (.xy, pixel units) + its pure SAD (.z, .w reserved 0). Purpose:
    //   AMBIGUITY awareness — periodic textures (striped flags, concentric tunnels) make the matcher
    //   resolve SAD TIES arbitrarily; the runner-up is the other plausible vector ~one texture period
    //   away, so a downstream warp can prefer the candidate nearer its expectation instead of the
    //   matcher's arbitrary pick. Read it via cand_image()/cand_view().
    // mv_affine: per-tile PARAMETRIC (affine) MV refinement — opt-in, ADDITIVE, default OFF (every
    //   existing caller is byte-identical with it off). The block-match emits ONE TRANSLATIONAL MV per
    //   8x8 tile; on NON-translational motion (zoom/rotation/scale) a single MV cannot represent the
    //   intra-tile divergence/curl → the warp samples wrong content (a crossfade artefact). When ON: a
    //   NEW post-pass (optical_flow_affine_fit.comp) runs AFTER the finest match, reads the full-res MV
    //   field, and fits — per tile — the local 2x2 linear part M = (a,b,c,d) over the 3x3 neighbourhood of
    //   best_mv samples (fp32 separable LS), emitting M into a companion RGBA16F field (same size as the
    //   MV field, aff_image()). The full per-pixel flow a consumer reconstructs is
    //   flow(p) = best_mv(tile) + M·(p − tile_centre), p in tile (MV-grid) units. On pure translation M≈0
    //   → flow(p)=best_mv exactly. Ill-conditioned tiles (flat interior / aperture / grid border / outlier
    //   slope) gate M→0 via the SAME confidence test the warp uses (sad_best vs residual_ceil,
    //   sad_zero·(1−improvement_frac)>sad_best). When OFF: aff_image() is VK_NULL_HANDLE, the post-pass is
    //   not recorded → MV/SAD outputs byte-identical. The coarsest-radius 6→8 rider is a separate switch
    //   (coarse_wide, below) — mv_affine does not touch the search radius. The in-pillar warp does not
    //   itself consume M; it is emitted for a downstream consumer.
    // mv_subpel: SUB-PIXEL motion-vector refinement — opt-in, ADDITIVE, default OFF (every existing caller
    //   is byte-identical with it off). The hierarchical search resolves an INTEGER best_mv per 8x8 tile.
    //   When ON, the FINEST match level, after the integer search, fits a 1D parabola to the PURE SAD at
    //   best_mv ±1 along each axis and adds the vertex offset (guarded denominator, clamped to
    //   [-0.5,+0.5]/axis) → the MV field (motion_image()) carries a FRACTIONAL vector. This reduces the
    //   inter-tile rounding error directly AND gives mv_affine a real sub-px ramp instead of a staircase.
    //   The 4 extra pure-SAD blocks fire ONLY at the finest level AND only when armed → ZERO cost on the
    //   OFF path; the subpel==0 store is the EXACT integer best_mv → MV/SAD byte-identical OFF. The
    //   candidate field (cand_image()) stays INTEGER (its OFF byte-identical contract holds).
    // coarse_wide: DECOUPLED coarse-radius rider — opt-in, default OFF. Raises the COARSEST search radius
    //   6→8 (kCoarseRAffine). It is its own switch — mv_affine does not influence the coarse radius. When
    //   OFF the coarsest radius is kCoarseR=6 → byte-identical.
    // mv_candsel: CANDIDATE-SELECTION at the matcher's FINEST level — opt-in, ADDITIVE, default OFF (every
    //   existing caller is byte-identical with it off). Attacks the crossfade/ghosting in textureless /
    //   low-gradient object INTERIORS (the aperture problem): with no local gradient the ±R search finds a
    //   noisy/ambiguous best_mv, and the symmetric warp then blends MISALIGNED samples. The shader ALREADY
    //   upsamples the coarser-pyramid MV into `pred` (the coherent REGION-scale motion). When ON, the
    //   FINEST match level — after the integer search resolves best_mv/best_sad — computes the PURE SAD of
    //   the region-predictor candidate pred_c=round(pred) and ADOPTS it (best_mv=pred_c, best_sad=sad_pred)
    //   UNLESS the local best is confidently better: best_sad < sad_pred·(1−kCandSelMargin)
    //   (kCandSelMargin=0.12 in-shader — the SAD improvement the local match must beat to be trusted over
    //   the region model; same family as improvement_frac, not a tie-break). A jump guard rejects a
    //   degenerate pred_c far from the search centre. The region model OFFERS the MV; the tile ADOPTS it
    //   only when its own match is ambiguous (NOT always — always-adopt hallucinates geometry). It COMPOSES
    //   with mv_subpel: candsel selects the integer MV SOURCE first, then subpel sub-pixel-refines whatever
    //   was chosen (both independently flag-gated). The extra pure_sad + compare fire ONLY at the finest
    //   level AND only when armed → ZERO cost on the OFF path; the OFF store is the EXACT integer best_mv →
    //   MV/SAD byte-identical. The second-best field is unchanged by candsel.
    // fg_variant: APP-LOCAL matcher fork — opt-in, ADDITIVE, default OFF (every existing caller is
    //   byte-identical with it off). The default flow path consumes this matcher with
    //   emit_second_best=false (the candidate/runner-up field is unused), yet the canonical
    //   optical_flow_hier_match.comp tracks the runner-up (second_cost/second_sad/second_mv)
    //   UNCONDITIONALLY on every level. When fg_variant is ON AND emit_second_best is OFF, init()
    //   additionally builds a SECOND matcher pipeline from optical_flow_hier_match_fg.comp — the variant
    //   with that runner-up tracking compiled out — sharing the SAME descriptor-set + push-constant layout
    //   as the canonical (so the descriptor sets are reused unchanged). record_optical_flow then binds the
    //   variant in place of the canonical. The CANONICAL matcher pipeline is STILL BUILT and is the one
    //   selected whenever emit_second_best is ON, so the canonical path stays byte-identical; the variant
    //   only short-circuits the never-consumed runner-up work on the default path. When fg_variant is OFF
    //   (default), or when emit_second_best is ON, NO variant pipeline is created and the canonical is bound
    //   exactly as before → byte-identical. The fork only removes the runner-up compares; on workloads where
    //   the SAD texelFetch loop dominates the saving may be small.
    [[nodiscard]] bool init(VkPhysicalDevice phys_dev,
                            VkDevice         device,
                            uint32_t         width,
                            uint32_t         height,
                            uint32_t         max_in_flight,
                            int              search_radius     = 2,
                            float            residual_ceil     = 32.0f,
                            float            improvement_frac  = 0.5f,
                            float            agreement_thresh  = 0.20f,
                            bool             emit_second_best  = false,
                            bool             mv_affine         = false,
                            bool             mv_subpel         = false,
                            bool             coarse_wide       = false,
                            bool             mv_candsel        = false,
                            bool             fg_variant        = false) noexcept;

    void shutdown(VkDevice device) noexcept;
    [[nodiscard]] bool initialized() const noexcept { return warp_pipeline_ != VK_NULL_HANDLE; }

    // ── set_temporal_prior (temporal MV prior, dual-centre coarsest search) ──
    // Opt-in, ADDITIVE, default OFF (every existing consumer is byte-identical with it off).
    //
    // OFF (default): the coarsest level searches a single ±R window around (0,0) — the
    //   predictor image is zero-cleared every call and pred_scale=0 nullify it, exactly as before.
    //
    // ON: at the START of each record_optical_flow, the PREVIOUS pair's coarsest-level MV
    //   (mvl_img_[n_levels_-1] — about to be overwritten by the new match) is copied into the
    //   coarsest-level predictor image, and the coarsest match is dispatched with dual_centre=1 and
    //   pred_scale=1: it evaluates the ±R window around BOTH (0,0) AND round(prior) and keeps the
    //   lower-cost winner. Self-healing on scene cuts (the zero centre wins a poisoned prior by
    //   construction; no detector); reaches displacements the zero window alone never could on
    //   coherent fast motion. The FIRST pair after arming (no previous MV yet) falls back to the
    //   zero-clear path — identical to OFF — so the prior never seeds from undefined contents.
    //
    // Idempotent; may be called any time after init() (typically once, right after). No GPU work.
    void set_temporal_prior(bool on) noexcept { temporal_prior_ = on; }
    [[nodiscard]] bool temporal_prior() const noexcept { return temporal_prior_; }

    // ── record_optical_flow ───────────────────────────────────────────
    // Records both compute dispatches into `cmd`. The caller is
    // responsible for image-layout transitions of a_view, b_view, c_view.
    //
    // Pre-conditions:
    //   a_view, b_view  → SHADER_READ_ONLY_OPTIMAL or GENERAL
    //   c_view          → GENERAL
    //
    // The intermediate motion-vector image transitions are managed
    // INTERNALLY — the caller doesn't need to touch them.
    //
    // After the call, the caller adds a barrier on c_view if a
    // downstream stage reads it.
    //
    // t: temporal phase in (0,1). Default 0.5 = midpoint.
    //   For Nx present pacing, pass t=k/N for the k-th intermediate.
    [[nodiscard]] bool record_optical_flow(VkCommandBuffer cmd,
                                           VkImageView     a_view,
                                           VkImageView     b_view,
                                           VkImageView     c_view,
                                           float           t = 0.5f) noexcept;

    // ── record_warp_only ─────────────────────────────────────────────
    // Re-dispatches the warp at a different temporal phase t, reusing
    // the MV and SAD from the most recent record_optical_flow call.
    // MUST be called after record_optical_flow (same frame pair); the
    // caller is responsible for the GPU fence between the two command
    // buffer submissions.
    //
    // Pre-conditions (caller must ensure):
    //   MV image, SAD image  → SHADER_READ_ONLY_OPTIMAL (left by record_optical_flow)
    //   a_view, b_view       → SHADER_READ_ONLY_OPTIMAL (left by the frame upload)
    //   c_view               → GENERAL (left by record_optical_flow + copy + barrier)
    [[nodiscard]] bool record_warp_only(VkCommandBuffer cmd,
                                        float           t) noexcept;

    // ── Diagnostics ───────────────────────────────────────────────────
    // Direct access to the motion-vector image for tests / introspection.
    // The image is at (width/8) × (height/8), format RG16F. After a
    // dispatch + fence wait, every texel contains (dx, dy) in pixels.
    [[nodiscard]] VkImage     motion_image()      const noexcept { return mv_image_; }
    [[nodiscard]] VkImageView motion_view()       const noexcept { return mv_view_;  }
    [[nodiscard]] uint32_t    motion_width()      const noexcept { return mv_w_;     }
    [[nodiscard]] uint32_t    motion_height()     const noexcept { return mv_h_;     }

    // SAD field: same size as motion_image(), format RG16F.
    // After a dispatch: R=sad_best (residual at winning MV), G=sad_zero (in-place residual).
    // In SHADER_READ_ONLY_OPTIMAL layout after record_optical_flow returns.
    [[nodiscard]] VkImage     sad_field_image()   const noexcept { return sad_img_;  }
    [[nodiscard]] VkImageView sad_field_view()    const noexcept { return sad_view_; }

    // Candidate (second-best / runner-up) field: same size as motion_image(), format RGBA16F. Valid ONLY
    // when init() was called with emit_second_best=true; VK_NULL_HANDLE otherwise.
    // After record_optical_flow: .xy = runner-up MV (pixel units), .z = runner-up pure SAD, .w = 0.
    // Layout SHADER_READ_ONLY_OPTIMAL post-call (mirrors mv_image_ / sad_img_).
    [[nodiscard]] VkImage     cand_image()         const noexcept { return cand_img_;  }
    [[nodiscard]] VkImageView cand_view()          const noexcept { return cand_view_; }
    [[nodiscard]] bool        emit_second_best()   const noexcept { return emit_second_best_; }

    // Affine companion field: same size as motion_image(), format RGBA16F. Valid ONLY when init() was
    // called with mv_affine=true; VK_NULL_HANDLE otherwise. After record_optical_flow:
    // .xyzw = the per-tile 2x2 linear part M = (a,b,c,d) such that the full per-pixel flow is
    // flow(p) = motion_image()[tile] + M·(p − tile_centre), p in tile (MV-grid) units. M≈0 on pure
    // translation. Layout SHADER_READ_ONLY_OPTIMAL post-call (mirrors mv_image_).
    [[nodiscard]] VkImage     aff_image()          const noexcept { return aff_img_;  }
    [[nodiscard]] VkImageView aff_view()           const noexcept { return aff_view_; }
    [[nodiscard]] bool        mv_affine()          const noexcept { return mv_affine_; }

    // Arm-state accessors. mv_subpel(): the finest-level sub-pixel refinement is armed → motion_image()
    // carries fractional vectors. coarse_wide(): the decoupled coarse-radius rider (6→8) is armed. Both
    // default false (OFF = byte-identical to the integer-MV matcher).
    [[nodiscard]] bool        mv_subpel()          const noexcept { return mv_subpel_; }
    [[nodiscard]] bool        coarse_wide()        const noexcept { return coarse_wide_; }
    // Candidate-selection arm-state. When armed, the finest level may ADOPT the coarse region-predictor MV
    // in ambiguous (textureless-interior / aperture) tiles instead of its noisy local best. Default false
    // (OFF = byte-identical to the matcher without it).
    [[nodiscard]] bool        mv_candsel()         const noexcept { return mv_candsel_; }
    // App-local fork arm-state. fg_variant(): the variant matcher (runner-up tracking compiled out) was
    // requested. match_fg_active(): the variant pipeline was actually built AND is the one bound on the
    // default path (true only when fg_variant requested AND emit_second_best OFF). Default false (OFF =
    // byte-identical to the canonical-only matcher).
    [[nodiscard]] bool        fg_variant()         const noexcept { return fg_variant_; }
    [[nodiscard]] bool        match_fg_active()    const noexcept { return match_pipeline_fg_ != VK_NULL_HANDLE; }

    // ── prebake_fg_descriptors (app-local descriptor-prebake) ────────────────────────────────────────────
    // The canonical record_optical_flow re-writes ALL descriptor sets EVERY record via dozens of per-binding
    // vkUpdateDescriptorSets calls (a host driver-call BURST that becomes a CPU front under GPU saturation).
    // But a frame-generation feed uses only a FIXED, small set of input view-pairs: a PING-PONG over two
    // physical frame views, with EVERY other binding (predictor, MV/SAD out, the pyramid intermediate views,
    // the candidate placeholder at match binding 5) CONSTANT for the run.
    //
    // This opt-in path PRE-BAKES two fully-populated descriptor-set collections at INIT (cold) — one per
    // ping-pong PARITY, keyed on its level-0 input (a_view, b_view) pair — into the existing ring slots 0
    // and 1. After arming, record_optical_flow SELECTS the slot whose baked (a,b) matches the passed views
    // and does ZERO vkUpdateDescriptorSets in steady state (the burst is eliminated; only a bind remains).
    // The two parities cover BOTH orderings the forward/backward records emit.
    //
    // ELIGIBILITY (returns false, leaves the canonical per-record-update path armed, if not met): the
    // pipeline must be initialized, fg_variant must have activated (match_fg_active()), and NEITHER
    // emit_second_best NOR mv_affine may be on — those are the ONLY flags that make a descriptor binding
    // VARY per record (binding 5 = real cand image at the finest level when emit_second; the affine set).
    // On the default path (emit_second_best=false, mv_affine=false) both hold, so binding 5 is ALWAYS the
    // 1×1 placeholder — that is what gets baked (pinned), matching the default path exactly. mv_subpel /
    // candsel / temporal_prior only move PUSH CONSTANTS or image CONTENT (not bindings) → they compose freely.
    //
    // (a0,b0) / (a1,b1) are the level-0 inputs for parity 0 and parity 1; pass the SAME pair twice if the
    // caller only ever feeds one ordering (a degenerate second parity is harmless — it is just never
    // selected). c_view is the warp-output target — CONSTANT across the run; it is baked into both warp
    // sets' binding 4. record_optical_flow ASSERTS the passed c_view matches the baked one and falls back to
    // the per-record path if it ever differs (safety), so a c_view change cannot bind a stale target.
    // Cold call: records descriptor writes only (no GPU work). Idempotent-safe to call once after init().
    // When NOT armed (default, or eligibility fails), record_optical_flow runs the per-record update block
    // EXACTLY as before → byte-identical.
    [[nodiscard]] bool prebake_fg_descriptors(VkImageView a0, VkImageView b0,
                                              VkImageView a1, VkImageView b1,
                                              VkImageView c_view) noexcept;
    [[nodiscard]] bool fg_prebake_active() const noexcept { return fg_prebake_; }

    // ── descriptor_update_calls (host-side measurement instrument) ───────────────────────────────────────
    // Monotonic count of vkUpdateDescriptorSets calls issued by record_optical_flow over this pipeline's
    // lifetime (incremented at each per-binding update site). The prebake fork's saving is a HOST-CPU relief
    // that does NOT change GPU timing; it is read as the DELTA of this counter across a record OFF vs ON.
    // Canonical path: dozens of calls/record (down + match + warp sets at n_levels=4). Prebake ON: 0
    // calls/record in steady state (the 2 cold prebake collections aside). Always counted (no flag — a
    // single relaxed add).
    [[nodiscard]] uint64_t descriptor_update_calls() const noexcept { return desc_update_calls_; }

private:
    // Number of pyramid levels (level 0 = full res; level kLevels-1 = coarsest).
    static constexpr uint32_t kLevels = 4u;

    [[nodiscard]] bool create_mv_image(VkPhysicalDevice phys_dev,
                                       VkDevice device,
                                       uint32_t w, uint32_t h) noexcept;   // level-0 MV (the public one)
    [[nodiscard]] bool create_sad_field_image(VkPhysicalDevice phys_dev,
                                              VkDevice device) noexcept;    // same size as mv, RG16F, STO|CIS
    [[nodiscard]] bool create_cand_image(VkPhysicalDevice phys_dev,
                                         VkDevice device) noexcept;         // candidate field: same size as mv, RGBA16F, STO|CIS
    [[nodiscard]] bool create_cand_placeholder(VkPhysicalDevice phys_dev,
                                               VkDevice device) noexcept;   // 1×1 RGBA16F STORAGE (off-path binding)
    [[nodiscard]] bool create_affine_image(VkPhysicalDevice phys_dev,
                                           VkDevice device) noexcept;       // affine field: same size as mv, RGBA16F, STO|CIS
    [[nodiscard]] bool create_pyramid_resources(VkPhysicalDevice phys_dev,
                                                VkDevice device) noexcept;  // A/B levels 1.., MV 1.., predictor
    [[nodiscard]] bool create_downsample_pipeline(VkDevice device) noexcept;
    [[nodiscard]] bool create_block_match_pipeline(VkDevice device) noexcept;   // builds the HIERARCHICAL matcher
    [[nodiscard]] bool create_block_match_fg_pipeline(VkDevice device) noexcept;// FG-variant matcher (runner-up tracking out), reuses match_pipeline_layout_/match_dsl_
    [[nodiscard]] bool create_warp_pipeline(VkDevice device) noexcept;
    [[nodiscard]] bool create_affine_pipeline(VkDevice device) noexcept;   // affine-fit post-pass
    [[nodiscard]] bool allocate_descriptor_sets(VkDevice device, uint32_t max_in_flight) noexcept;
    // Write the FULL descriptor-set collection for one ring slot given its level-0 (a,b) input views (every
    // other binding is the pipeline's own constant images). Shared by record_optical_flow (per-record path)
    // and prebake_fg_descriptors (cold prebake). emit_cand selects match binding 5's image (real cand at
    // finest level when on, else placeholder). Increments desc_update_calls_ per binding.
    void write_slot_descriptors(uint32_t slot, VkImageView a_view, VkImageView b_view,
                                VkImageView c_view, bool emit_cand) noexcept;

    VkDevice              device_              {VK_NULL_HANDLE};
    int                   search_radius_       {2};      // FINEST-level refinement radius
    float                 residual_ceil_       {32.0f};  // confidence gate (a): max sad_best to WARP
    float                 improvement_frac_    {0.5f};   // confidence gate (b): min (sad_zero-sad_best)/sad_zero
    float                 agreement_thresh_    {0.20f};  // agreement gate: max mean d_tile to WARP
    uint32_t              frame_w_             {0u};
    uint32_t              frame_h_             {0u};
    // Pyramid depth actually used (1..kLevels), chosen at init so the COARSEST level keeps enough
    // resolution that small objects do not vanish in the downsample (see record_optical_flow).
    uint32_t              n_levels_            {0u};

    // Per-level dimensions (image + MV grid). Index 0 = full res.
    uint32_t              lvl_w_[kLevels]      {};
    uint32_t              lvl_h_[kLevels]      {};
    uint32_t              mvl_w_[kLevels]      {};
    uint32_t              mvl_h_[kLevels]      {};

    // Level-0 motion-vector image (frame_w/8 × frame_h/8, RG16F) — the PUBLIC field
    // returned by motion_image()/motion_view(). Unchanged contract.
    VkImage               mv_image_            {VK_NULL_HANDLE};
    VkImageView           mv_view_             {VK_NULL_HANDLE};
    VkDeviceMemory        mv_memory_           {VK_NULL_HANDLE};
    uint32_t              mv_w_                {0u};
    uint32_t              mv_h_                {0u};

    // SAD field image (same size as mv_image_, RG16F) — R=sad_best, G=sad_zero per tile.
    // Written by the block-match; read by the confidence warp. In SHADER_READ_ONLY_OPTIMAL
    // after record_optical_flow returns (same contract as mv_image_ post-call layout).
    VkImage               sad_img_             {VK_NULL_HANDLE};
    VkImageView           sad_view_            {VK_NULL_HANDLE};
    VkDeviceMemory        sad_memory_          {VK_NULL_HANDLE};

    // Candidate (second-best) field — same size as mv_image_, RGBA16F. Created ONLY when emit_second_best_
    // is true; .xy = runner-up MV, .z = runner-up SAD, .w = 0 (reserved). The finest match level writes it
    // (emit_second=1) when armed. SHADER_READ_ONLY_OPTIMAL after record_optical_flow.
    bool                  emit_second_best_    {false};
    VkImage               cand_img_            {VK_NULL_HANDLE};
    VkImageView           cand_view_           {VK_NULL_HANDLE};
    VkDeviceMemory        cand_memory_         {VK_NULL_HANDLE};
    // 1×1 RGBA16F STORAGE placeholder bound at match binding 5 when emit_second_best_ is OFF (and at the
    // NON-finest levels when ON) — keeps the descriptor layout valid with no behavioural effect (the
    // shader's emit_second push is 0 there → no store). Always created (a single 1×1 image).
    VkImage               cand_ph_img_         {VK_NULL_HANDLE};
    VkImageView           cand_ph_view_        {VK_NULL_HANDLE};
    VkDeviceMemory        cand_ph_memory_      {VK_NULL_HANDLE};

    // Affine companion field — same size as mv_image_, RGBA16F. Created ONLY when mv_affine_ is true;
    // .xyzw = the per-tile 2x2 linear part M = (a,b,c,d) of the local affine. The affine-fit post-pass
    // writes it (after the finest match) when armed. SHADER_READ_ONLY_OPTIMAL after record_optical_flow.
    // The translational part stays in mv_image_.
    bool                  mv_affine_           {false};
    VkImage               aff_img_             {VK_NULL_HANDLE};
    VkImageView           aff_view_            {VK_NULL_HANDLE};
    VkDeviceMemory        aff_memory_          {VK_NULL_HANDLE};

    // Arm switches. mv_subpel_: the finest match level adds a guarded parabolic sub-pixel offset to best_mv
    // (the MV field becomes fractional). coarse_wide_: the decoupled coarse-radius rider (kCoarseR 6→
    // kCoarseRAffine 8). Both default OFF — no image/binding change, so OFF is byte-identical; mv_subpel_
    // only flips the finest level's `subpel` push, coarse_wide_ only the coarsest level's radius.
    bool                  mv_subpel_           {false};
    bool                  coarse_wide_         {false};
    // Candidate-selection arm switch. When set, the finest match level flips the `candsel` push to 1 (0
    // everywhere else); the shader then adopts the coarse region-predictor MV in ambiguous tiles. No
    // image/binding change — OFF is byte-identical; only the finest level's `candsel` push moves.
    bool                  mv_candsel_          {false};
    // App-local fork arm switch. When set AND emit_second_best_ is OFF, init() builds match_pipeline_fg_
    // (the runner-up-free matcher) and record_optical_flow binds it instead of match_pipeline_. No
    // image/binding change (same layout) — OFF is byte-identical, and even ON the canonical match_pipeline_
    // is still built (used whenever emit_second_best_ is ON).
    bool                  fg_variant_          {false};

    // Descriptor PREBAKE state (see prebake_fg_descriptors). When armed, the two ring slots 0/1 hold
    // fully-populated descriptor collections, one per ping-pong PARITY keyed on its level-0 (a,b) views
    // below; record_optical_flow then SELECTS the matching slot and skips the per-record
    // vkUpdateDescriptorSets burst entirely. Default OFF → record_optical_flow runs the per-record update
    // block exactly as before (byte-identical). fg_pb_a_/fg_pb_b_ are the baked level-0 inputs per parity.
    bool                  fg_prebake_          {false};
    VkImageView           fg_pb_a_[2]          {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageView           fg_pb_b_[2]          {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageView           fg_pb_c_             {VK_NULL_HANDLE};   // baked warp target — record-time guard
    // Lifetime count of vkUpdateDescriptorSets calls issued by record_optical_flow (and the cold prebake).
    // Read via descriptor_update_calls() as the OFF-vs-ON delta — the host burst eliminated.
    uint64_t              desc_update_calls_   {0u};

    // Pyramid frame images for levels 1..kLevels-1 (level 0 = the caller's A/B views).
    VkImage               pyr_a_img_[kLevels]  {}; VkImageView pyr_a_view_[kLevels]{}; VkDeviceMemory pyr_a_mem_[kLevels]{};
    VkImage               pyr_b_img_[kLevels]  {}; VkImageView pyr_b_view_[kLevels]{}; VkDeviceMemory pyr_b_mem_[kLevels]{};
    // Motion-vector images for levels 1..kLevels-1 (level 0 = mv_image_ above).
    VkImage               mvl_img_[kLevels]    {}; VkImageView mvl_view_[kLevels]{};  VkDeviceMemory mvl_mem_[kLevels]{};
    // Coarsest-level predictor image (serves both the zero-predictor and the temporal-prior roles).
    //   prior OFF, or prior ON + first pair: cleared to (0,0) → pred_scale=0 nullifies it (a zero
    //     predictor).
    //   prior ON + a previous pair exists: the previous pair's coarsest MV (mvl_img_[n_levels_-1]) is
    //     copied in → dual_centre=1, pred_scale=1 search both (0,0) and round(prior).
    VkImage               prior_mv_img_        {VK_NULL_HANDLE};
    VkImageView           prior_mv_view_       {VK_NULL_HANDLE};
    VkDeviceMemory        prior_mv_mem_        {VK_NULL_HANDLE};

    // Temporal MV prior state. temporal_prior_ = the opt-in switch (set_temporal_prior). have_prev_mv_ =
    // a previous record_optical_flow has run since arming, so mvl_img_[n_levels_-1] holds a real coarse MV
    // to seed from (false on the first armed pair → zero-clear fallback).
    bool                  temporal_prior_      {false};
    bool                  have_prev_mv_        {false};

    // Sampler shared across pipelines (linear filter, clamp edge).
    VkSampler             sampler_             {VK_NULL_HANDLE};

    // Pyramid downsample pipeline.
    VkDescriptorSetLayout down_dsl_            {VK_NULL_HANDLE};
    VkPipelineLayout      down_layout_         {VK_NULL_HANDLE};
    VkPipeline            down_pipeline_       {VK_NULL_HANDLE};

    // Stage 1: hierarchical block-match (6 bindings: A, B, predictor, MV out, SAD field out, second-best
    //   out; 24-byte push {int search_radius; float pred_scale; int dual_centre; int emit_second;
    //   int subpel; int candsel}).
    VkDescriptorSetLayout match_dsl_           {VK_NULL_HANDLE};
    VkPipelineLayout      match_pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline            match_pipeline_      {VK_NULL_HANDLE};
    // App-local FG-variant matcher pipeline (runner-up tracking compiled out). Created ONLY when
    // fg_variant_ is armed AND emit_second_best_ is OFF; shares match_pipeline_layout_/match_dsl_/the
    // descriptor sets with the canonical match_pipeline_ (same bindings + push). VK_NULL_HANDLE otherwise.
    VkPipeline            match_pipeline_fg_   {VK_NULL_HANDLE};

    // Stage 2: confidence+agreement warp (5 bindings: A, B, MV, SAD field, C out; 16-byte push).
    VkDescriptorSetLayout warp_dsl_            {VK_NULL_HANDLE};
    VkPipelineLayout      warp_pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline            warp_pipeline_       {VK_NULL_HANDLE};

    // Affine-fit post-pass (3 bindings: MV field sampler, affine-params storage out, SAD field sampler;
    //   8-byte push {float residual_ceil; float improvement_frac}). Created only when armed.
    VkDescriptorSetLayout affine_dsl_            {VK_NULL_HANDLE};
    VkPipelineLayout      affine_pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline            affine_pipeline_       {VK_NULL_HANDLE};

    // Descriptor pool + rotating ring. Each in-flight slot holds the full set
    // collection for one record call: downsample (A,B per level), match (per
    // level), warp. The pyramid/MV images are single-instance (overwritten per
    // call) — the ring guards descriptor-set updates, not image content.
    static constexpr uint32_t kMaxInFlight = 16u;
    VkDescriptorPool      desc_pool_           {VK_NULL_HANDLE};
    VkDescriptorSet       down_a_set_[kMaxInFlight][kLevels]{};   // levels 1..kLevels-1 used
    VkDescriptorSet       down_b_set_[kMaxInFlight][kLevels]{};
    VkDescriptorSet       match_set_[kMaxInFlight][kLevels]{};    // levels 0..kLevels-1
    VkDescriptorSet       warp_sets_[kMaxInFlight]{};
    VkDescriptorSet       affine_set_[kMaxInFlight]{};            // affine post-pass (used only when armed)
    uint32_t              n_sets_              {0u};
    uint32_t              next_set_            {0u};
    uint32_t              last_slot_           {0u};  // slot used by most recent record_optical_flow (for record_warp_only)
};

} // namespace phyriad::render::vulkan
// Made with my soul - Swately <3
