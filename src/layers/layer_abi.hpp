#pragma once
// PhyriadFG — src/layers/layer_abi.hpp : the ROW SCHEMA of the layer registry (LAYERTAB).
//
// The layer contract chosen by the architecture search (docs/planning/aap/A3_CHOSEN_DESIGN.md,
// Candidate C §1.4 + the grafts) and bound by docs/planning/STAGE_CONTRACT.md stage 5. One
// X-macro row per layer in layer_table.def is the SOLE declaration site; everything else (the
// LayerConfig fields, the parser, the help, the UI model, the GLSL chains, the UBO layout, the
// contract hash) derives from it. An unknown column is a compile error, not a convention.
//
// Stage R0 of CONVERGENCE_MASTER_PLAN.md: this schema + the registry exist; shaders/wap_warp.comp
// is UNCHANGED and the push block is still assembled from Config. The registry runs as a SHADOW
// of the hand-written parser and layer_config_parity() proves the two agree (risk XR2).
//
// Column families (kept apart on purpose — the column-closure experiment counts them separately):
//   LAYER columns   : what a layer IS (stage, rank, kind, channels, arm, requires/excludes, shadows).
//   CLI-COMPAT cols : what today's CLI surface forces on a faithful port (on/off flags, param flags,
//                     aliases). They exist for parity with the hand parser; R3 may retire them.
#include <cstdint>
#include <cstddef>

namespace pfg::layers {

enum class Stage : uint8_t { MVCOND, SAMPLE, COMPOSE, FLOW, HOST };
// F = fused into the stage kernel (spec-constant gated); P = own SG pass; X = pseudo-row (no body,
// no enable, no params — pins an ordering fact: ":snapshot mv_raw_fwd" at rank 25, etc.).
enum class Kind  : uint8_t { F, P, X };
// Per-GENERATION validity (the arm_mask bit derived from FlowSet[gen] fields — STAGE_CONTRACT §1
// ArmInputs). ALWAYS = the spec constant alone decides.
enum class ArmId : uint8_t { ALWAYS, GME, BWD, GME_AND_BWD, COMMIT };

enum class ParamType : uint8_t { F32, I32, BOOL };
enum class UiKind    : uint8_t { NUMBER, SWITCH, HIDDEN };

// CLI-COMPAT param flags (how the hand parser treats the token today):
enum : uint32_t {
    PF_NONE           = 0,
    PF_IMPLIES_ON     = 1u << 0,   // setting the param turns the row ON (--vblend-exact, --mv-edge-snap N)
    PF_OPTIONAL_VALUE = 1u << 1,   // the value token is optional; bare flag = the param's dflt (--bg-reclaim [F])
    PF_ZERO_OFF       = 1u << 2,   // value 0 (or <= 0) means the row is OFF (--bg-reclaim 0, --mv-edge-snap 0)
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
};

// The single_track backward reach into the core (Candidate C §4.7 — the declared, printed wart).
enum : uint32_t {
    SH_NONE        = 0,
    SH_WA_EFF_ZERO = 1u << 0,  // wap_warp.comp:678  wa_eff collapse
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

// The core push block (STAGE_CONTRACT stage 5; Candidate C §2). R0: declared, NOT yet used by the
// present path (the 58-float push stays until R3).
struct CorePush {
    float    residual_ceil;
    float    improvement_frac;
    float    agreement_threshold;
    float    t;
    uint32_t arm_mask;
};
static_assert(sizeof(CorePush) == 20, "CorePush is the 20-byte core contract (A0 M2c)");

}  // namespace pfg::layers

// Made with my soul - Swately <3
