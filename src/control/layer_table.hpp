#pragma once
// PhyriadFG — src/control/layer_table.hpp : expands layer_table.def into the constexpr tables.
//   enum class LayerId          — dense ids, in .def order
//   kLayers[] / kParams[]       — the rows and their params
//   the static_asserts          — unique rank per stage; every requires/excludes target exists;
//                                 param ranges sane; ids dense.
// Everything downstream (LayerConfig, parser, help, JSON model, dump, hash, GLSL generation) reads
// ONLY these tables. See layer_abi.hpp for the column semantics.
#include "control/layer_abi.hpp"

namespace pfg::layers {

// ── pass 1: the ids ─────────────────────────────────────────────────────────────────────────────
enum class LayerId : uint16_t {
#define PFG_LAYER(ID, ...)            ID,
#define PFG_PARAM(...)
#define PFG_LAYER_END(ID)
#define PFG_PSEUDO(ID, ...)           ID,
#define PFG_BIT(ID)                   0u
#include "control/layer_table.def"
#undef PFG_LAYER
#undef PFG_PARAM
#undef PFG_LAYER_END
#undef PFG_PSEUDO
#undef PFG_BIT
    COUNT
};
constexpr uint16_t kLayerCount = static_cast<uint16_t>(LayerId::COUNT);

// ── pass 2: the params (flat, in .def order; each carries its layer index) ─────────────────────
struct ParamTableBuilder {
    // counted at compile time by expanding PFG_PARAM as 1 and everything else as 0
};
constexpr size_t kParamCount =
#define PFG_LAYER(...)
#define PFG_PARAM(...)                1 +
#define PFG_LAYER_END(ID)
#define PFG_PSEUDO(...)
#define PFG_BIT(ID)                   0u
#include "control/layer_table.def"
#undef PFG_LAYER
#undef PFG_PARAM
#undef PFG_LAYER_END
#undef PFG_PSEUDO
#undef PFG_BIT
    0;

#define PFG_BIT(ID) (1u << static_cast<unsigned>(::pfg::layers::LayerId::ID))

constexpr ParamDesc kParams[kParamCount == 0 ? 1 : kParamCount] = {
#define PFG_LAYER(...)
#define PFG_PARAM(ID, pname, type, dflt, lo, hi, flag, alias, pflags, ui, help) \
    ParamDesc{ static_cast<uint16_t>(LayerId::ID), #pname, ParamType::type, (float)(dflt), (float)(lo), (float)(hi), flag, alias, (uint32_t)(pflags), UiKind::ui, help },
#define PFG_LAYER_END(ID)
#define PFG_PSEUDO(...)
#include "control/layer_table.def"
#undef PFG_LAYER
#undef PFG_PARAM
#undef PFG_LAYER_END
#undef PFG_PSEUDO
};

// param_first / param_count per layer: computed from kParams (the .def lists params right after
// their layer, so they are contiguous).
constexpr uint16_t param_first_of(uint16_t layer) {
    for (size_t i = 0; i < kParamCount; ++i) if (kParams[i].layer == layer) return (uint16_t)i;
    return 0;
}
constexpr uint16_t param_count_of(uint16_t layer) {
    uint16_t n = 0;
    for (size_t i = 0; i < kParamCount; ++i) if (kParams[i].layer == layer) ++n;
    return n;
}

// ── pass 3: the rows ────────────────────────────────────────────────────────────────────────────
constexpr LayerDesc kLayers[kLayerCount] = {
#define PFG_LAYER(ID, name, stage, rank, kind, default_on, overrides, needs, req_any, excludes, reads, writes, shadows, arm, group, on_flag, off_flag, dominates_ok, help) \
    LayerDesc{ static_cast<uint16_t>(LayerId::ID), name, Stage::stage, (uint16_t)(rank), Kind::kind, default_on, overrides, \
               (uint32_t)(needs), req_any, (uint32_t)(excludes), (uint32_t)(reads), (uint32_t)(writes), (uint32_t)(shadows), \
               ArmId::arm, group, on_flag, off_flag, dominates_ok, help, \
               param_first_of(static_cast<uint16_t>(LayerId::ID)), param_count_of(static_cast<uint16_t>(LayerId::ID)) },
#define PFG_PARAM(...)
#define PFG_LAYER_END(ID)
#define PFG_PSEUDO(ID, name, stage, rank, text) \
    LayerDesc{ static_cast<uint16_t>(LayerId::ID), name, Stage::stage, (uint16_t)(rank), Kind::X, true, false, \
               0u, false, 0u, 0u, 0u, 0u, ArmId::ALWAYS, "", nullptr, nullptr, nullptr, text, 0, 0 },
#include "control/layer_table.def"
#undef PFG_LAYER
#undef PFG_PARAM
#undef PFG_LAYER_END
#undef PFG_PSEUDO
};

// ── the static checks (the schema's own invariants) ─────────────────────────────────────────────
constexpr bool ids_dense() {
    for (uint16_t i = 0; i < kLayerCount; ++i) if (kLayers[i].id != i) return false;
    return true;
}
constexpr bool ranks_unique_per_stage() {
    for (uint16_t i = 0; i < kLayerCount; ++i)
        for (uint16_t j = i + 1; j < kLayerCount; ++j)
            if (kLayers[i].stage == kLayers[j].stage && kLayers[i].rank == kLayers[j].rank) return false;
    return true;
}
constexpr bool targets_exist() {
    const uint32_t all = (kLayerCount >= 32) ? 0xFFFFFFFFu : ((1u << kLayerCount) - 1u);
    for (uint16_t i = 0; i < kLayerCount; ++i)
        if ((kLayers[i].needs & ~all) || (kLayers[i].excludes & ~all)) return false;
    return true;
}
constexpr bool params_sane() {
    for (size_t i = 0; i < kParamCount; ++i) {
        if (kParams[i].layer >= kLayerCount) return false;
        if (kParams[i].lo > kParams[i].hi) return false;
        if (kParams[i].dflt < kParams[i].lo || kParams[i].dflt > kParams[i].hi) {
            // exception: a default of exactly 0 outside [lo,hi] is the "unset / off" sentinel the hand
            // parser uses (--mes-sim 0 = use --mv-sim; --bg-reclaim 0 = off); nothing else is allowed
            if (kParams[i].dflt != 0.f) return false;
        }
    }
    return true;
}
constexpr bool stage_writes_legal() {
    for (uint16_t i = 0; i < kLayerCount; ++i) {
        const LayerDesc& L = kLayers[i];
        if (L.kind == Kind::X) continue;
        if (L.stage == Stage::MVCOND  && (L.writes_ch & ~CH_MV)) return false;
        if (L.stage == Stage::SAMPLE  && (L.writes_ch & ~CH_MV_SAMPLE)) return false;
        if (L.stage == Stage::WEIGHT  && L.writes_ch != CH_NONE) return false;   // its return value (wa) only
        if (L.stage == Stage::COMPOSE && (L.writes_ch & ~CH_BLEND)) return false;   // its return value, plus CH_BLEND (COLUMN_CLOSURE §2.2)
        // R5: a FLOW row produces flow-class fields only — never stage 5's conditioned MV, its samples, its
        // accumulator or its per-pixel evidence (STAGE_CONTRACT §2: "stage 5 never computes a flow-class field", and the converse)
        if (L.stage == Stage::FLOW && (L.writes_ch & (CH_MV | CH_MV_SAMPLE | CH_BLEND | CH_A_SAMP | CH_B_SAMP | CH_WARP_OK | CH_D_PIXEL | CH_STASIS))) return false;
    }
    return true;
}
constexpr bool pseudo_rows_clean() {
    for (uint16_t i = 0; i < kLayerCount; ++i)
        if (kLayers[i].kind == Kind::X && (kLayers[i].param_count || kLayers[i].on_flag || kLayers[i].off_flag)) return false;
    return true;
}
static_assert(kLayerCount <= 32, "requires/excludes masks are 32-bit; widen before the 33rd row");
static_assert(ids_dense(), "LayerId must be dense and in .def order");
static_assert(ranks_unique_per_stage(), "two rows share a rank inside one stage — the declared order is ambiguous");
static_assert(targets_exist(), "a requires/excludes mask names a layer that does not exist");
static_assert(params_sane(), "a param default is outside [lo,hi] (or lo > hi)");
static_assert(stage_writes_legal(), "a row writes a channel its stage may not write (MVCOND->CH_MV only; SAMPLE->CH_MV_SAMPLE only; WEIGHT returns; COMPOSE returns + CH_BLEND)");
static_assert(pseudo_rows_clean(), "a pseudo-row carries params or flags");

constexpr const char* stage_name(Stage s) {
    switch (s) { case Stage::MVCOND: return "MVCOND"; case Stage::SAMPLE: return "SAMPLE"; case Stage::WEIGHT: return "WEIGHT";
                 case Stage::COMPOSE: return "COMPOSE"; case Stage::FLOW: return "FLOW"; case Stage::HOST: return "HOST"; }
    return "?";
}
constexpr const char* arm_name(ArmId a) {
    switch (a) { case ArmId::ALWAYS: return "ALWAYS"; case ArmId::GME: return "GME"; case ArmId::BWD: return "BWD";
                 case ArmId::GME_AND_BWD: return "GME+BWD"; case ArmId::COMMIT: return "COMMIT";
                 case ArmId::PRIOR: return "PRIOR"; case ArmId::HOLON: return "HOLON"; case ArmId::BIDIR_OK: return "BIDIR"; }
    return "?";
}
constexpr char kind_char(Kind k) { return k == Kind::F ? 'F' : (k == Kind::P ? 'P' : (k == Kind::H ? 'H' : 'X')); }

}  // namespace pfg::layers

// Made with my soul - Swately <3
