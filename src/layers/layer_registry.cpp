// PhyriadFG — src/layers/layer_registry.cpp : the registry's runtime (stage R0 of
// docs/planning/CONVERGENCE_MASTER_PLAN.md; strategies X1/X2 of CONVERGENCE_IMPLEMENTATION_STRATEGIES.md).
//
// Everything here reads ONLY pfg::layers::kLayers / kParams (layer_table.def expanded). The shadow
// parser mirrors the hand chain in cli/cli.cpp token-for-token (same clamps, same side effects, same
// peek rule for the optional value of --bg-reclaim); layer_config_parity() is the proof that it does
// (risk XR2/XR6: a mismatch is a loud abort in main(), exit code 3 — never a log line).
//
// R0 boundary: the push block is STILL assembled from Config by present.cpp; nothing here drives the
// shader. The registry is the single source for help, the UI model, the dump and the hash.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "cli/cli.hpp"
#include "layers/layer_config.hpp"

namespace pfg::layers {

// ── defaults ────────────────────────────────────────────────────────────────────────────────────
LayerConfig::LayerConfig() {
    for (uint16_t i = 0; i < kLayerCount; ++i) on[i] = kLayers[i].default_on;
    for (size_t i = 0; i < kParamCount; ++i) val[i] = kParams[i].dflt;
    if (kParamCount == 0) val[0] = 0.f;
}

void layer_exec_order(uint16_t out[kLayerCount]) {
    for (uint16_t i = 0; i < kLayerCount; ++i) out[i] = i;
    std::sort(out, out + kLayerCount, [](uint16_t a, uint16_t b) {
        if (kLayers[a].stage != kLayers[b].stage) return (uint8_t)kLayers[a].stage < (uint8_t)kLayers[b].stage;
        return kLayers[a].rank < kLayers[b].rank;
    });
}

// ── the shadow parser ───────────────────────────────────────────────────────────────────────────
static float clampf(float f, float lo, float hi) { return f < lo ? lo : (f > hi ? hi : f); }

// A token that parses COMPLETELY as a number (the --bg-reclaim peek rule, cli.cpp:333).
static bool full_number(const char* s, float& out) {
    if (!s) return false;
    char* end = nullptr;
    const double d = std::strtod(s, &end);
    if (!end || end == s || *end != '\0') return false;
    out = (float)d;
    return true;
}

static void apply_param_value(LayerConfig& lc, size_t p, float raw) {
    const ParamDesc& P = kParams[p];
    float v = raw;
    if (P.type == ParamType::I32) {
        // --mv-edge-snap semantics (cli.cpp:364): n in {1,2} else 0 — an out-of-range integer is OFF,
        // not clamped. Generalised as: an I32 outside [lo,hi] becomes 0 when PF_ZERO_OFF, else clamped.
        const long n = std::lround(v);
        if ((P.pflags & PF_ZERO_OFF) && (n < (long)P.lo || n > (long)P.hi || n == 0)) v = 0.f;
        else v = (float)std::lround(clampf((float)n, P.lo, P.hi));
        if ((P.pflags & PF_ZERO_OFF) && n != 0 && (n < (long)P.lo || n > (long)P.hi)) v = 0.f;
    } else if (P.type == ParamType::BOOL) {
        v = (v != 0.f) ? 1.f : 0.f;
    } else {
        v = clampf(v, P.lo, P.hi);
        if ((P.pflags & PF_ZERO_OFF) && v <= 0.f) v = 0.f;
    }
    lc.val[p] = v;
    if (P.pflags & PF_IMPLIES_ON) lc.on[P.layer] = true;
    if ((P.pflags & PF_ZERO_OFF) && v == 0.f) lc.on[P.layer] = false;
}

bool layer_shadow_parse(int argc, char** argv, int i, LayerConfig& lc) {
    const char* a = argv[i];
    // row on/off tokens
    for (uint16_t L = 0; L < kLayerCount; ++L) {
        if (kLayers[L].on_flag  && !std::strcmp(a, kLayers[L].on_flag))  { lc.on[L] = true;  return true; }
        if (kLayers[L].off_flag && !std::strcmp(a, kLayers[L].off_flag)) {
            lc.on[L] = false;
            // the hand parser also zeroes the strength of a PF_ZERO_OFF param on --no-X (cli.cpp:338-341)
            for (size_t p = kLayers[L].param_first; p < (size_t)kLayers[L].param_first + kLayers[L].param_count; ++p)
                if (kParams[p].pflags & PF_ZERO_OFF) lc.val[p] = 0.f;
            return true;
        }
    }
    // param tokens (flag or alias)
    for (size_t p = 0; p < kParamCount; ++p) {
        const ParamDesc& P = kParams[p];
        const bool is_flag  = P.flag       && !std::strcmp(a, P.flag);
        const bool is_alias = P.flag_alias && !std::strcmp(a, P.flag_alias);
        if (!is_flag && !is_alias) continue;
        const char* next = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (P.type == ParamType::BOOL && !(P.pflags & PF_OPTIONAL_VALUE)) {
            // a bare switch (--vblend-exact, --st-no-stasis)
            apply_param_value(lc, p, 1.f);
            return true;
        }
        if (is_flag && (P.pflags & PF_OPTIONAL_VALUE)) {
            float f = 0.f;
            if (full_number(next, f)) apply_param_value(lc, p, f);   // consumed by the hand parser too
            else                      apply_param_value(lc, p, P.dflt);
            return true;
        }
        if (!next) return true;                       // the hand parser prints "needs arg" and fails
        apply_param_value(lc, p, (float)std::atof(next));   // atof, like the hand parser (cli.cpp:560-564)
        return true;
    }
    return false;
}

// ── the parity oracle ───────────────────────────────────────────────────────────────────────────
LayerOldShadow capture_layer_old(const Config& c) {
    LayerOldShadow s;
    s.valid = true;
    s.mv_guided = c.mv_guided;         s.mv_sim = c.mv_sim;
    s.mv_edge_snap = c.mv_edge_snap;   s.mv_edge_snap_sim = c.mv_edge_snap_sim;
    s.inertia = c.inertia;             s.inertia_thresh = c.inertia_thresh;
    s.bg_reclaim = c.bg_reclaim;
    s.phase_anchor = c.phase_anchor;
    s.ambig = c.ambig;
    s.vblend = c.vblend;               s.vblend_t0 = c.vblend_t0; s.vblend_strength = c.vblend_strength; s.vblend_exact = c.vblend_exact;
    s.stasis = c.stasis;               s.stasis_thresh = c.stasis_thresh;
    s.single_track = c.single_track;   s.st_no_stasis = c.st_no_stasis;
    return s;
}

static size_t pidx(LayerId L, const char* pname) {
    for (size_t p = 0; p < kParamCount; ++p)
        if (kParams[p].layer == (uint16_t)L && !std::strcmp(kParams[p].name, pname)) return p;
    std::printf("[layertab] INTERNAL: param %s not found\n", pname);
    return 0;
}
#define PFG_ON(L)      (lc.on[(uint16_t)LayerId::L])
#define PFG_V(L, name) (lc.val[pidx(LayerId::L, #name)])

bool layer_config_parity(const Config& c) {
    const LayerOldShadow& o = c.layers_old;
    const LayerConfig&   lc = c.layers;
    bool ok = true;
    if (!o.valid) { std::printf("[layertab] PARITY FAIL: the pre-cascade shadow was never captured (parse_args did not call capture_layer_old)\n"); return false; }
    auto chk_b = [&](const char* what, bool a, bool b) { if (a != b) { std::printf("[layertab] PARITY FAIL %-28s old=%d new=%d\n", what, (int)a, (int)b); ok = false; } };
    auto chk_f = [&](const char* what, float a, float b) { if (std::memcmp(&a, &b, sizeof a) != 0) { std::printf("[layertab] PARITY FAIL %-28s old=%.9g new=%.9g\n", what, (double)a, (double)b); ok = false; } };
    // Effective values = what the push block reads (present.cpp:1119-1150): a gated param is 0 when its row is off.
    chk_b("mv_guided.on",          o.mv_guided,                 PFG_ON(MV_GUIDED));
    chk_f("mv_guided.sim (eff)",   o.mv_guided ? o.mv_sim : 0.f, PFG_ON(MV_GUIDED) ? PFG_V(MV_GUIDED, sim) : 0.f);
    chk_b("mv_edge_snap.on",       o.mv_edge_snap != 0,         PFG_ON(MV_EDGE_SNAP));
    chk_f("mv_edge_snap.variant",  (float)o.mv_edge_snap,       PFG_V(MV_EDGE_SNAP, variant));
    chk_f("mv_edge_snap.sim",      o.mv_edge_snap_sim,          PFG_V(MV_EDGE_SNAP, sim));
    chk_b("inertia.on",            o.inertia,                   PFG_ON(INERTIA));
    chk_f("inertia.thresh (eff)",  o.inertia ? o.inertia_thresh : 0.f, PFG_ON(INERTIA) ? PFG_V(INERTIA, thresh) : 0.f);
    chk_b("bg_reclaim.on",         o.bg_reclaim > 0.f,          PFG_ON(BG_RECLAIM));
    chk_f("bg_reclaim.strength",   o.bg_reclaim,                PFG_ON(BG_RECLAIM) ? PFG_V(BG_RECLAIM, strength) : 0.f);
    chk_b("phase_anchor.on",       o.phase_anchor,              PFG_ON(PHASE_ANCHOR));
    chk_b("ambig.on",              o.ambig,                     PFG_ON(AMBIG));
    chk_b("vblend.on",             o.vblend,                    PFG_ON(VBLEND));
    chk_f("vblend.t0 (eff)",       o.vblend ? o.vblend_t0 : 0.f, PFG_ON(VBLEND) ? PFG_V(VBLEND, t0) : 0.f);
    chk_f("vblend.strength (eff)", o.vblend ? o.vblend_strength : 0.f, PFG_ON(VBLEND) ? PFG_V(VBLEND, strength) : 0.f);
    chk_b("vblend.exact",          o.vblend_exact,              PFG_V(VBLEND, exact) != 0.f);
    chk_b("stasis.on",             o.stasis,                    PFG_ON(STASIS));
    chk_f("stasis.thresh (eff)",   o.stasis ? o.stasis_thresh : 0.f, PFG_ON(STASIS) ? PFG_V(STASIS, thresh) : 0.f);
    chk_b("single_track.on",       o.single_track,              PFG_ON(SINGLE_TRACK));
    chk_b("single_track.no_stasis",o.st_no_stasis,              PFG_V(SINGLE_TRACK, no_stasis) != 0.f);
    return ok;
}

// ── the contract hash (FNV-1a 64 over the resolved chain, in execution order) ───────────────────
static void fnv(uint64_t& h, const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
}
uint64_t layer_contract_hash(const Config& c) {
    uint64_t h = 1469598103934665603ull;
    uint16_t order[kLayerCount]; layer_exec_order(order);
    for (uint16_t k = 0; k < kLayerCount; ++k) {
        const LayerDesc& L = kLayers[order[k]];
        const uint8_t hdr[6] = { (uint8_t)(L.id & 0xFF), (uint8_t)(L.id >> 8), (uint8_t)L.stage, (uint8_t)L.kind, (uint8_t)L.overrides, (uint8_t)L.arm };
        fnv(h, hdr, sizeof hdr);
        fnv(h, &L.rank, sizeof L.rank);
        const uint8_t on = c.layers.on[L.id] ? 1 : 0; fnv(h, &on, 1);
        for (size_t p = L.param_first; p < (size_t)L.param_first + L.param_count; ++p) {
            uint32_t bits; std::memcpy(&bits, &c.layers.val[p], 4); fnv(h, &bits, 4);
        }
    }
    return h;
}

// ── --layer-dump ────────────────────────────────────────────────────────────────────────────────
void layer_dump(const Config& c) {
    uint16_t order[kLayerCount]; layer_exec_order(order);
    int enabled = 0, real = 0;
    for (uint16_t i = 0; i < kLayerCount; ++i) if (kLayers[i].kind != Kind::X) { ++real; if (c.layers.on[i]) ++enabled; }
    std::printf("[layertab] contract=0x%016llX  (%d rows enabled of %d; %zu params; R0: registry SHADOW, wap_warp.comp drives the product)\n",
                (unsigned long long)layer_contract_hash(c), enabled, real, (size_t)kParamCount);
    bool core_printed = false;
    for (uint16_t k = 0; k < kLayerCount; ++k) {
        const LayerDesc& L = kLayers[order[k]];
        if (!core_printed && (uint8_t)L.stage >= (uint8_t)Stage::COMPOSE) {
            std::printf("CORE       fg_core          res_ceil=%.3f improv=%.3f agree=%.3f t=<per-tick>\n", (double)c.res_ceil, (double)c.conf_improv, (double)c.agreement);
            core_printed = true;
        }
        if (L.kind == Kind::X) { std::printf("%-7s%4u %-16s (pseudo-row)\n", stage_name(L.stage), (unsigned)L.rank, L.name); continue; }
        std::printf("%-7s%4u %-16s %c  arm=%-7s %s", stage_name(L.stage), (unsigned)L.rank, L.name, kind_char(L.kind), arm_name(L.arm),
                    c.layers.on[L.id] ? "ON " : "OFF");
        for (size_t p = L.param_first; p < (size_t)L.param_first + L.param_count; ++p) {
            const ParamDesc& P = kParams[p];
            if (P.type == ParamType::F32) std::printf("  %s=%.3f", P.name, (double)c.layers.val[p]);
            else                          std::printf("  %s=%d", P.name, (int)c.layers.val[p]);
        }
        if (L.needs) {
            std::printf("  requires={");
            bool first = true;
            for (uint16_t j = 0; j < kLayerCount; ++j) if (L.needs & (1u << j)) { std::printf("%s%s", first ? "" : (L.req_any ? "|" : "&"), kLayers[j].name); first = false; }
            std::printf("}");
        }
        if (L.excludes) {
            std::printf("  unless={");
            bool first = true;
            for (uint16_t j = 0; j < kLayerCount; ++j) if (L.excludes & (1u << j)) { std::printf("%s%s", first ? "" : "|", kLayers[j].name); first = false; }
            std::printf("}");
        }
        if (L.overrides) std::printf("  OVERRIDE");
        if (L.shadows)   std::printf("  shadows=0x%X", L.shadows);
        if (L.dominates_ok) std::printf("\n           dominates_ok: %s", L.dominates_ok);
        std::printf("\n");
    }
    if (!core_printed) std::printf("CORE       fg_core\n");
}

// ── --dump-config (the round-trip corpus record) ────────────────────────────────────────────────
void dump_config(const Config& c) {
    const LayerOldShadow& o = c.layers_old;
    std::printf("[dump-config] sizeof(Config)=%zu rows=%u params=%zu\n", sizeof(Config), (unsigned)kLayerCount, (size_t)kParamCount);
    std::printf("[old] mv_guided=%d mv_sim=%.9g mv_edge_snap=%d mv_edge_snap_sim=%.9g inertia=%d inertia_thresh=%.9g bg_reclaim=%.9g phase_anchor=%d ambig=%d vblend=%d vblend_t0=%.9g vblend_strength=%.9g vblend_exact=%d stasis=%d stasis_thresh=%.9g single_track=%d st_no_stasis=%d\n",
                (int)o.mv_guided, (double)o.mv_sim, o.mv_edge_snap, (double)o.mv_edge_snap_sim, (int)o.inertia, (double)o.inertia_thresh, (double)o.bg_reclaim,
                (int)o.phase_anchor, (int)o.ambig, (int)o.vblend, (double)o.vblend_t0, (double)o.vblend_strength, (int)o.vblend_exact,
                (int)o.stasis, (double)o.stasis_thresh, (int)o.single_track, (int)o.st_no_stasis);
    std::printf("[new]");
    for (uint16_t i = 0; i < kLayerCount; ++i) if (kLayers[i].kind != Kind::X) std::printf(" %s=%d", kLayers[i].name, (int)c.layers.on[i]);
    for (size_t p = 0; p < kParamCount; ++p) std::printf(" %s.%s=%.9g", kLayers[kParams[p].layer].name, kParams[p].name, (double)c.layers.val[p]);
    std::printf("\n[hash] contract=0x%016llX\n", (unsigned long long)layer_contract_hash(c));
}

// ── --help section ──────────────────────────────────────────────────────────────────────────────
void print_layer_help() {
    std::printf("\nLAYERS (the registry: src/layers/layer_table.def is the single declaration site; --layer-dump prints the resolved chain,\n"
                "        --layer-model-json emits the UI model, --dump-config the parsed record):\n");
    uint16_t order[kLayerCount]; layer_exec_order(order);
    for (uint16_t k = 0; k < kLayerCount; ++k) {
        const LayerDesc& L = kLayers[order[k]];
        if (L.kind == Kind::X) continue;
        if (L.off_flag) std::printf("  %-24s %s OFF (%s rank %u; default %s). %s\n", L.off_flag, L.name, stage_name(L.stage), (unsigned)L.rank, L.default_on ? "ON" : "OFF", L.help);
        if (L.on_flag)  std::printf("  %-24s %s ON.\n", L.on_flag, L.name);
        for (size_t p = L.param_first; p < (size_t)L.param_first + L.param_count; ++p) {
            const ParamDesc& P = kParams[p];
            if (!P.flag) continue;
            char tok[64];
            if (P.type == ParamType::BOOL)                 std::snprintf(tok, sizeof tok, "%s", P.flag);
            else if (P.pflags & PF_OPTIONAL_VALUE)         std::snprintf(tok, sizeof tok, "%s [F]", P.flag);
            else if (P.type == ParamType::I32)             std::snprintf(tok, sizeof tok, "%s N", P.flag);
            else                                           std::snprintf(tok, sizeof tok, "%s F", P.flag);
            if (P.type == ParamType::BOOL) std::printf("  %-24s %s.%s (default %s). %s\n", tok, L.name, P.name, P.dflt != 0.f ? "on" : "off", P.help);
            else                           std::printf("  %-24s %s.%s (default %g, [%g,%g]). %s\n", tok, L.name, P.name, (double)P.dflt, (double)P.lo, (double)P.hi, P.help);
            if (P.flag_alias) std::printf("  %-24s alias of %s (value required).\n", P.flag_alias, P.flag);
        }
    }
}

// ── --layer-model-json (the UI renders THIS; ui/src/main.js keeps no layer literal) ─────────────
static void json_str(const char* s) {
    std::putchar('"');
    for (; s && *s; ++s) {
        const unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { std::putchar('\\'); std::putchar(ch); }
        else if (ch < 0x20) std::printf("\\u%04x", ch);
        else std::putchar(ch);
    }
    std::putchar('"');
}
void emit_layer_model_json(const Config& c) {
    std::printf("{\"contract\":\"0x%016llX\",\"controls\":[", (unsigned long long)layer_contract_hash(c));
    bool first = true;
    uint16_t order[kLayerCount]; layer_exec_order(order);
    for (uint16_t k = 0; k < kLayerCount; ++k) {
        const LayerDesc& L = kLayers[order[k]];
        if (L.kind == Kind::X) continue;
        if (L.off_flag) {
            std::printf("%s{\"flag\":", first ? "" : ","); json_str(L.off_flag); first = false;
            std::printf(",\"type\":\"switch-off\",\"default\":true,\"group\":"); json_str(L.group);
            std::printf(",\"name\":"); json_str(L.name);
            std::printf(",\"desc\":"); json_str(L.help);
            std::printf(",\"stage\":\"%s\",\"rank\":%u}", stage_name(L.stage), (unsigned)L.rank);
        }
        for (size_t p = L.param_first; p < (size_t)L.param_first + L.param_count; ++p) {
            const ParamDesc& P = kParams[p];
            if (!P.flag || P.ui == UiKind::HIDDEN) continue;
            const char* flag = (P.pflags & PF_OPTIONAL_VALUE) && P.flag_alias ? P.flag_alias : P.flag;   // the UI always sends a value
            std::printf("%s{\"flag\":", first ? "" : ","); json_str(flag); first = false;
            std::printf(",\"group\":"); json_str(L.group);
            char nm[96]; std::snprintf(nm, sizeof nm, "%s: %s", L.name, P.name);
            std::printf(",\"name\":"); json_str(nm);
            std::printf(",\"desc\":"); json_str(P.help);
            if (P.type == ParamType::BOOL) {
                std::printf(",\"type\":\"switch\",\"default\":%s}", P.dflt != 0.f ? "true" : "false");
            } else {
                const double step = (P.type == ParamType::I32) ? 1.0 : ((P.hi - P.lo) >= 2.f ? 0.5 : ((P.hi - P.lo) >= 0.5f ? 0.05 : 0.01));
                std::printf(",\"type\":\"number\",\"default\":\"%g\",\"min\":%g,\"max\":%g,\"step\":%g}", (double)P.dflt, (double)P.lo, (double)P.hi, step);
            }
        }
    }
    std::printf("]}\n");
}

}  // namespace pfg::layers

// Made with my soul - Swately <3
