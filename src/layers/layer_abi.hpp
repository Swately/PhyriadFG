#pragma once
// PhyriadFG — src/layers/layer_abi.hpp : the ROW SCHEMA of the layer registry (LAYERTAB).
//
// The layer contract chosen by the architecture search (docs/planning/aap/A3_CHOSEN_DESIGN.md,
// Candidate C §1.4 + the grafts) and bound by docs/planning/STAGE_CONTRACT.md stage 5. One
// X-macro row per layer in layer_table.def is the SOLE declaration site; everything else (the
// LayerConfig fields, the parser, the help, the UI model, the GLSL chains, the UBO layout, the
// contract hash) derives from it. An unknown column is a compile error, not a convention.
//
// Stage R0 of CONVERGENCE_MASTER_PLAN.md: this schema + the registry exist as a SHADOW of the
// hand-written parser (layer_config_parity() proves the two agree, risk XR2). Stage R3: the rows
// have bodies (shaders/layers/<name>.glsl) and shaders/fg_core.comp runs them as the generated chain
// behind --fg-core (opt-in until M4 passes); shaders/wap_warp.comp stays the default product path.
//
// Column families (kept apart on purpose — the column-closure experiment counts them separately):
//   LAYER columns   : what a layer IS (stage, rank, kind, channels, arm, requires/excludes, shadows).
//   CLI-COMPAT cols : what today's CLI surface forces on a faithful port (on/off flags, param flags,
//                     aliases). They exist for parity with the hand parser; R3 may retire them.
#include <cstdint>
#include <cstddef>

namespace pfg::layers {

// WEIGHT (R3, COLUMN_CLOSURE_EXPERIMENT.md §2.1): between the core's samples and its blend —
// `float pfg_weight_<n>(float wa, in LayerCtx ctx)`, ranks 100–199; identity wa = 1 − t.
enum class Stage : uint8_t { MVCOND, SAMPLE, WEIGHT, COMPOSE, FLOW, HOST };
// F = fused into the stage kernel (spec-constant gated); P = own SG pass; X = pseudo-row (no body,
// no enable, no params — pins an ordering fact: ":snapshot mv_raw_fwd" at rank 25, etc.).
enum class Kind  : uint8_t { F, P, X, H };   // H (R5, FLOW_ROW_MAP §2.1) = a HOST pass: a CPU function over FlowSet fields, ordered by rank among the stage's GPU passes; no GLSL body, no SG barrier
// Per-GENERATION validity (the arm_mask bit derived from FlowSet[gen] fields — STAGE_CONTRACT §1
// ArmInputs). ALWAYS = the spec constant alone decides.
enum class ArmId : uint8_t { ALWAYS, GME, BWD, GME_AND_BWD, COMMIT,
                             PRIOR,       // R5 (FLOW): the pair has a prior (has_prev) — the first pair after start runs only the source
                             HOLON,       // R5: has_prev && tier < 4 && !holon_skip — the holon refinement (memory/objects), shed at tier 4, decimated at 2/3
                             BWD_HOLON,   // R5: bwd_ok && tier < 4 && !holon_skip — the same for the backward legs
                             BIDIR_OK };  // R5: has_prev && tier < 5 && !pipelined && !bwd_skipping — the backward match itself
// The per-generation validity inputs (STAGE_CONTRACT §1 ArmInputs) the host derives arm_mask from — one
// struct, so every ArmId has its input and layer_arm_mask() switches over all of them (R3).
struct ArmInputs { bool gme_ok, bwd_ok, matte_ok, appear_ok, commit_ok;
                   // R5 (FLOW_ROW_MAP §2.4): the CONTROL-plane inputs the FLOW rows' arms read. Defaults keep every
                   // pre-R5 call site (`ArmInputs{a,b,c,d,e}`) meaning what it meant.
                   bool has_prev = false;      // the pair has a prior (flow.cpp have_prev_f)
                   int  tier = 0;              // the pressure tier after the governor floor (flow.cpp:1467)
                   bool holon_skip = false;    // the tier-2/3 decimation decided by CONTROL (flow.cpp:1468-1474)
                   bool pipelined = false;     // --fwd-pipeline (forbids the backward legs, flow.cpp:1386-1389)
                   bool bwd_skipping = false;  // the bwd-skip hysteresis on t_pair_ema (flow.cpp:1428-1431)
                 };

enum class ParamType : uint8_t { F32, I32, BOOL };
enum class UiKind    : uint8_t { NUMBER, SWITCH, HIDDEN };

// CLI-COMPAT param flags (how the hand parser treats the token today):
enum : uint32_t {
    PF_NONE           = 0,
    PF_IMPLIES_ON     = 1u << 0,   // setting the param turns the row ON (--vblend-exact, --mv-edge-snap N)
    PF_OPTIONAL_VALUE = 1u << 1,   // the value token is optional; bare flag = the param's dflt (--bg-reclaim [F])
    PF_ZERO_OFF       = 1u << 2,   // value 0 (or <= 0) means the row is OFF (--bg-reclaim 0, --mv-edge-snap 0)
    PF_NEGATED        = 1u << 3,   // R5: the flag itself sets this BOOL to 0 (--no-shapefield: the hand parser's positive token is a no-op print)
    PF_NO_FORM        = 1u << 4,   // R5: the derived token "--no-" + flag[2:] sets this BOOL to 0 (--obj-fill-rim / --no-obj-fill-rim both act)
};

// Channel bits (declared reads/writes — LAYER columns).
enum : uint32_t {
    CH_NONE       = 0,
    CH_MV         = 1u << 0,   // the conditioned primary MV (MVCOND rows may write ONLY this)
    CH_MV_RAW_FWD = 1u << 1,   // the rank-25 snapshot (wap_warp.comp:344)
    CH_MV_SAMPLE  = 1u << 2,   // the SAMPLE-stage effective MV (vblend) — flows only into fg_core()
    CH_SAD        = 1u << 3,
    CH_PREV       = 1u << 4,   // the A anchor image
    CH_CUR        = 1u << 5,   // the B anchor image
    CH_D_PIXEL    = 1u << 6,
    CH_STASIS     = 1u << 7,   // the block-stasis boolean published at rank 45, read by single_track
    CH_A_SAMP     = 1u << 8,
    CH_B_SAMP     = 1u << 9,
    CH_WARP_OK    = 1u << 10,
    CH_PERSIST    = 1u << 11,  // u_persistence
    CH_MV_BWD     = 1u << 12,  // the bwd field
    CH_CANDIDATES = 1u << 13,  // u_candidates (runner-up)
    CH_DISSIDENCE = 1u << 14,  // u_dissidence / u_dissidence_bwd
    CH_MV_TARGET  = 1u << 15,  // u_mv_target (prev-pair MV / real next-pair MV)
    CH_GME        = 1u << 16,  // the affine model
    CH_BLEND      = 1u << 17,  // the core's SECOND accumulator (the un-warped (1-t)/t crossfade, wap_warp.comp:694-695):
                               // produced by the core, read by the select row; a COMPOSE row MAY write it (COLUMN_CLOSURE §2.2)
    // R5 — the FLOW stage's fields (FLOW_ROW_MAP §2.5). The first four are TEMPORAL: the row reads its own output of
    // gen−1 (the CH_PERSIST pattern); declared as the row's state, read at entry, never a cross-row temporal read.
    CH_MV_PREV_GEN = 1u << 18,  // mv_smooth's previous-generation store
    CH_MEM_PRIOR   = 1u << 19,  // the scene-holon's persistent silhouette prior (mem_prior)
    CH_MEM_ADV     = 1u << 20,  // the prior advected to the current anchor (mem_adv)
    CH_OBJ_STATE   = 1u << 21,  // the object-holon's 16-slot identity table + label scratch
    CH_WAKE        = 1u << 22,  // the wake records the backward repair leaves for mem_refresh
    CH_GME_BWD     = 1u << 23,  // the backward affine model
    CH_PAIR_STATS  = 1u << 24,  // the FlowSet scalars: matte masses fwd/bwd, dispersion, bwd_valid
};

// The single_track backward reach into the core (Candidate C §4.7 — the declared, printed wart).
enum : uint32_t {
    SH_NONE        = 0,
    SH_WA_EFF_ZERO = 1u << 0,  // wap_warp.comp:678  wa_eff collapse — R3: dissolved into the WEIGHT row single_track_wa (rank 190); no longer declared
    SH_BLEND_BASE  = 1u << 1,  // :692–695 blend_result base = cur
    SH_FORCE_WARP  = 1u << 2,  // the forced warp selection
    SH_COMMIT_INERT= 1u << 3,  // commit/ts-smooth inertness
};

struct ParamDesc {
    uint16_t    layer;        // index into kLayers
    const char* name;         // "sim"
    ParamType   type;
    float       dflt, lo, hi; // for I32/BOOL: integral values in float
    const char* flag;         // "--mv-sim"; nullptr => internal (in the dump + hash, not on the CLI)
    const char* flag_alias;   // CLI-COMPAT: a second token for the same param (--bg-reclaim-strength); nullptr = none
    uint32_t    pflags;       // PF_* (CLI-COMPAT)
    UiKind      ui;
    const char* help;
};

struct LayerDesc {
    uint16_t    id;           // == its index in kLayers (dense)
    const char* name;         // "mv_guided"
    Stage       stage;
    uint16_t    rank;         // THE declared order inside the stage; unique per stage (static_assert)
    Kind        kind;
    bool        default_on;
    bool        overrides;    // a COMPOSE row that may DISCARD c_in — printed as OVERRIDE
    uint32_t    needs;        // "requires" in Candidate C (a C++20 keyword): bitmask of layer ids that must also be on
    bool        req_any;      // requires semantics: true = ANY of the mask, false = ALL
    uint32_t    excludes;     // bitmask of layer ids whose presence disables this row's call (mv_guided vs mv_edge_snap)
    uint32_t    reads_ch;
    uint32_t    writes_ch;
    uint32_t    shadows;      // SH_* (single_track only)
    ArmId       arm;
    const char* group;        // UI group
    const char* on_flag;      // CLI-COMPAT: token that turns the row ON (nullptr = none / legacy no-op stays in cli.cpp)
    const char* off_flag;     // CLI-COMPAT: token that turns the row OFF (nullptr = not user-toggleable)
    const char* dominates_ok; // the printed justification when the row knowingly overwrites what it does not read
    const char* help;
    uint16_t    param_first, param_count;
};

// The core push block (STAGE_CONTRACT stage 5; Candidate C §2): the six contract parameters + t + the
// per-generation arm mask. R3: pushed to shaders/fg_core.comp as the head of FgPush below.
struct CorePush {
    float    residual_ceil;
    float    improvement_frac;
    float    agreement_threshold;
    float    t;
    uint32_t arm_mask;
};
static_assert(sizeof(CorePush) == 20, "CorePush is the 20-byte core contract (A0 M2c)");

// The per-generation SCALAR block of FlowSet[gen] that rides with the core push: the gme affine model
// (mv(gx,gy) = (a + b·gx + c·gy, d + e·gx + f·gy), gx,gy = mv-grid coords), read by the bg_reclaim /
// ambig rows through gme_model_mv(). It is FlowSet data — like t and arm_mask, which also ride in the
// push — not a layer parameter, which is why it is not in the config-time LayerParams UBO. A declared
// deviation from Candidate C §2's "20 B of fields" (push constants are race-free under the async
// present; a host-written per-gen UBO would need a ring). Recorded in R3's stage record.
struct GenScalars { float gme_a, gme_b, gme_c, gme_d, gme_e, gme_f; };
struct FgPush { CorePush core; GenScalars gen; };
static_assert(sizeof(FgPush) == 44, "FgPush = CorePush (20) + the gme gen-scalars (24); shaders/fg_core.comp declares the same block");

}  // namespace pfg::layers

// Made with my soul - Swately <3
