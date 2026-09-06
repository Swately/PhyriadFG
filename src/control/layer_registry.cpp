// PhyriadFG — src/control/layer_registry.cpp : the registry's runtime (stage R0 of
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
#include "control/cli.hpp"
#include "control/layer_config.hpp"

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
    // row on/off tokens. R5 (FLOW_ROW_MAP §2.2): a token may drive SEVERAL rows (--no-inertia: the MVCOND consumer
    // and the FLOW producer; --no-memory: the three memory rows; --no-ambig / --no-objects likewise) — every row
    // that carries the token takes it, exactly as the hand parser's single field cascades to all of them.
    bool matched = false;
    for (uint16_t L = 0; L < kLayerCount; ++L) {
        if (kLayers[L].on_flag  && !std::strcmp(a, kLayers[L].on_flag))  { lc.on[L] = true;  matched = true; }
        if (kLayers[L].off_flag && !std::strcmp(a, kLayers[L].off_flag)) {
            lc.on[L] = false; matched = true;
            // the hand parser also zeroes the strength of a PF_ZERO_OFF param on --no-X (cli.cpp:338-341)
            for (size_t p = kLayers[L].param_first; p < (size_t)kLayers[L].param_first + kLayers[L].param_count; ++p)
                if (kParams[p].pflags & PF_ZERO_OFF) lc.val[p] = 0.f;
        }
    }
    if (matched) return true;
    // R5: the derived negative form of a PF_NO_FORM BOOL ("--no-" + flag[2:]) sets it to 0
    for (size_t p = 0; p < kParamCount; ++p) {
        const ParamDesc& P = kParams[p];
        if (!(P.pflags & PF_NO_FORM) || !P.flag || std::strncmp(P.flag, "--", 2) != 0) continue;
        char noform[80]; std::snprintf(noform, sizeof noform, "--no-%s", P.flag + 2);
        if (!std::strcmp(a, noform)) { apply_param_value(lc, p, 0.f); return true; }
    }
    // param tokens (flag or alias)
    for (size_t p = 0; p < kParamCount; ++p) {
        const ParamDesc& P = kParams[p];
        const bool is_flag  = P.flag       && !std::strcmp(a, P.flag);
        const bool is_alias = P.flag_alias && !std::strcmp(a, P.flag_alias);
        if (!is_flag && !is_alias) continue;
        const char* next = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (P.type == ParamType::BOOL && !(P.pflags & PF_OPTIONAL_VALUE)) {
            // a bare switch (--vblend-exact, --st-no-stasis); R5: a PF_NEGATED flag is the negative token (--no-shapefield → 0)
            apply_param_value(lc, p, (P.pflags & PF_NEGATED) ? 0.f : 1.f);
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
    // R5: the FLOW rows' hand fields (pre-cascade)
    s.gme = c.gme; s.gme_gpu = c.gme_gpu; s.gme_gpu_verify = c.gme_gpu_verify; s.gme_irls2 = c.gme_irls2;
    s.objects = c.objects; s.shapefield = c.shapefield; s.obj_fill_rim = c.obj_fill_rim; s.expire = c.expire; s.persist_reset = c.persist_reset;
    s.scene_memory = c.scene_memory; s.bidir = c.bidir; s.mv_median = c.mv_median; s.mv_smooth = c.mv_smooth; s.nvofa = c.nvofa;
    s.mv_consensus = c.mv_consensus;
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
    // R5: the FLOW rows. Raw pre-cascade values on both sides; "(eff)" where the hand field is meaningless alone.
    chk_b("flow_source.nvofa",     o.nvofa,                     PFG_V(FLOW_SOURCE, nvofa) != 0.f);
    chk_b("mv_smooth.on",          o.mv_smooth > 0.f,           PFG_ON(MV_SMOOTH));
    chk_f("mv_smooth.alpha",       o.mv_smooth,                 PFG_V(MV_SMOOTH, alpha));
    chk_b("candidates.on",         o.ambig,                     PFG_ON(CANDIDATES));
    chk_b("bidir.on",              o.bidir,                     PFG_ON(BIDIR));
    chk_b("persistence.on",        o.inertia,                   PFG_ON(PERSISTENCE));
    chk_b("gme.on",                o.gme,                       PFG_ON(GME));
    chk_b("gme.irls2",             o.gme_irls2,                 PFG_V(GME, irls2) != 0.f);
    chk_b("gme_gpu.on",            o.gme_gpu,                   PFG_ON(GME_GPU));
    chk_b("gme_gpu.verify (eff)",  o.gme_gpu && o.gme_gpu_verify, PFG_ON(GME_GPU) && PFG_V(GME_GPU, verify) != 0.f);
    chk_b("mem_fwd.on",            o.scene_memory,              PFG_ON(MEM_FWD));
    chk_b("mem_bwd.on",            o.scene_memory,              PFG_ON(MEM_BWD));
    chk_b("mem_refresh.on",        o.scene_memory,              PFG_ON(MEM_REFRESH));
    chk_b("objects.on",            o.objects,                   PFG_ON(OBJECTS));
    chk_b("objects_bwd.on",        o.objects,                   PFG_ON(OBJECTS_BWD));
    chk_b("objects.shapefield",    o.shapefield,                PFG_V(OBJECTS, shapefield) != 0.f);
    chk_b("objects.fill_rim",      o.obj_fill_rim,              PFG_V(OBJECTS, fill_rim) != 0.f);
    chk_b("objects.expire",        o.expire,                    PFG_V(OBJECTS, expire) != 0.f);
    chk_b("objects.persist_reset", o.persist_reset,             PFG_V(OBJECTS, persist_reset) != 0.f);
    chk_b("mv_consensus.on (eff)", o.mv_consensus && (o.mv_guided || o.mv_median), PFG_ON(MV_CONSENSUS) && (PFG_ON(MV_GUIDED) || PFG_V(MV_CONSENSUS, blind) != 0.f));
    chk_b("mv_consensus.blind",    o.mv_median,                 PFG_V(MV_CONSENSUS, blind) != 0.f);
    return ok;
}

// ── R5 step 3b: the resolved state ─────────────────────────────────────────────────────────────
void layer_resolve_effective(LayerConfig& lc, uint32_t unavailable) {
    uint32_t avail = 0;
    for (int pass = 0; pass < 16; ++pass) {   // a monotone fixpoint over `needs` (depth ≤ the longest needs chain)
        uint32_t next = 0;
        for (uint16_t i = 0; i < kLayerCount; ++i) {
            const LayerDesc& L = kLayers[i];
            if (L.kind == Kind::X || !lc.on[i] || (unavailable & (1u << i))) continue;
            bool ok = true;
            if (L.needs) { const uint32_t have = L.needs & avail; ok = L.req_any ? (have != 0) : (have == L.needs); }
            if (ok) next |= (1u << i);
        }
        if (next == avail) break;
        avail = next;
    }
    uint32_t eff = avail;
    for (uint16_t i = 0; i < kLayerCount; ++i)
        if ((avail & (1u << i)) && (kLayers[i].excludes & avail)) eff &= ~(1u << i);
    lc.avail = avail; lc.eff = eff;
}
bool layer_flow_resolve(Config& c, bool use_wap, bool use_gme, bool use_gme_gpu, bool use_objects, bool use_memory,
                        bool use_bidir, bool use_ambig, bool use_inertia, bool use_mv_smooth) {
    uint32_t unavailable = 0;
    if (!use_wap) for (uint16_t i = 0; i < kLayerCount; ++i) if (kLayers[i].stage == Stage::FLOW) unavailable |= (1u << i);   // the stage exists only under WAP
    if (!use_gme_gpu) unavailable |= (1u << (unsigned)LayerId::GME_GPU);      // forced off on single-GPU or when gme_create failed (flow_init.cpp)
    if (!use_mv_smooth) unavailable |= (1u << (unsigned)LayerId::MV_SMOOTH);  // the pipe was not created (mvsm_create)
    if (!use_ambig && c.ambig && use_gme) unavailable |= (1u << (unsigned)LayerId::CANDIDATES);   // the host bridge failed (init_host_bridge)
    layer_resolve_effective(c.layers, unavailable);
    const LayerConfig& lc = c.layers;
    auto E = [&](LayerId id) { return (lc.eff & (1u << (unsigned)id)) != 0u; };
    auto A = [&](LayerId id) { return (lc.avail & (1u << (unsigned)id)) != 0u; };
    bool ok = true;
    auto chk = [&](const char* what, bool hand, bool row) { if (hand != row) { std::printf("[layertab] FLOW PARITY FAIL %-16s init=%d rows=%d\n", what, (int)hand, (int)row); ok = false; } };
    chk("gme (avail)",   use_gme,       A(LayerId::GME));
    chk("gme_gpu (eff)", use_gme_gpu,   E(LayerId::GME_GPU));
    chk("gme cpu (eff)", use_gme && !use_gme_gpu, E(LayerId::GME));
    chk("objects",       use_objects,   E(LayerId::OBJECTS));
    chk("objects_bwd",   use_objects && use_bidir, E(LayerId::OBJECTS_BWD));
    chk("mem_fwd",       use_memory,    E(LayerId::MEM_FWD));
    chk("mem_bwd",       use_memory && use_bidir, E(LayerId::MEM_BWD));
    chk("mem_refresh",   use_memory,    E(LayerId::MEM_REFRESH));
    chk("gme_bwd",       use_gme && use_bidir, E(LayerId::GME_BWD));
    chk("bidir",         use_bidir,     E(LayerId::BIDIR));
    chk("candidates",    use_ambig,     E(LayerId::CANDIDATES));
    chk("persistence",   use_inertia,   E(LayerId::PERSISTENCE));
    chk("mv_smooth",     use_mv_smooth, E(LayerId::MV_SMOOTH));
    std::printf("[layertab] flow rows resolved: avail=0x%08X eff=0x%08X (gme=%d gme_gpu=%d objects=%d memory=%d bidir=%d candidates=%d persistence=%d mv_smooth=%d consensus=%d) %s\n",
                lc.avail, lc.eff, (int)A(LayerId::GME), (int)E(LayerId::GME_GPU), (int)E(LayerId::OBJECTS), (int)E(LayerId::MEM_FWD), (int)E(LayerId::BIDIR),
                (int)E(LayerId::CANDIDATES), (int)E(LayerId::PERSISTENCE), (int)E(LayerId::MV_SMOOTH), (int)E(LayerId::MV_CONSENSUS), ok ? "== the init cascades" : "!= the init cascades");
    return ok;
}

// ── R5 step 4: the transport question, answered from the table ─────────────────────────────────
uint32_t layer_host_written_channels(const LayerConfig& lc) {
    uint32_t ch = 0;
    for (uint16_t i = 0; i < kLayerCount; ++i)
        if (kLayers[i].kind == Kind::H && (lc.eff & (1u << i))) ch |= kLayers[i].writes_ch;
    return ch;
}
static void print_channels(uint32_t m) {
    static const struct { uint32_t bit; const char* name; } kNames[] = {
        {CH_MV,"mv"},{CH_MV_RAW_FWD,"mv_raw_fwd"},{CH_MV_SAMPLE,"mv_sample"},{CH_SAD,"sad"},{CH_PREV,"prev"},{CH_CUR,"cur"},
        {CH_D_PIXEL,"d_pixel"},{CH_STASIS,"stasis"},{CH_A_SAMP,"a_samp"},{CH_B_SAMP,"b_samp"},{CH_WARP_OK,"warp_ok"},
        {CH_PERSIST,"persist"},{CH_MV_BWD,"mv_bwd"},{CH_CANDIDATES,"candidates"},{CH_DISSIDENCE,"dissidence"},
        {CH_MV_TARGET,"mv_target"},{CH_GME,"gme"},{CH_BLEND,"blend"},{CH_MV_PREV_GEN,"mv_prev_gen"},{CH_MEM_PRIOR,"mem_prior"},
        {CH_MEM_ADV,"mem_adv"},{CH_OBJ_STATE,"obj_state"},{CH_WAKE,"wake"},{CH_GME_BWD,"gme_bwd"},{CH_PAIR_STATS,"pair_stats"} };
    bool first = true;
    for (const auto& n : kNames) if (m & n.bit) { std::printf("%s%s", first ? "" : ",", n.name); first = false; }
    if (first) std::printf("-");
}
void layer_transport_report(const LayerConfig& lc, uint32_t transported_ch) {
    const uint32_t host = layer_host_written_channels(lc);
    const uint32_t dirty = transported_ch & host;          // the CPU is the author: the upload is inherent
    const uint32_t gpu_only = transported_ch & ~host;      // GPU-produced and GPU-consumed: a device-resident FlowSet could skip it
    std::printf("[layertab] 3->5 transport: CPU-authored ["); print_channels(dirty);
    std::printf("] -- a stage-3 host row writes them, so the host copy is authoritative and its upload is inherent. "
                "Not CPU-authored ["); print_channels(gpu_only);
    std::printf("] -- of these, the FLOW fields (sad, candidates, mv_target) are GPU-produced and GPU-consumed and a "
                "device-resident FlowSet could share them without the round trip; prev/cur are stage-2 frames on the "
                "ingest path, a separate question. Host rows on: ");
    bool first = true;
    for (uint16_t i = 0; i < kLayerCount; ++i)
        if (kLayers[i].kind == Kind::H && (lc.eff & (1u << i))) { std::printf("%s%s", first ? "" : " ", kLayers[i].name); first = false; }
    if (first) std::printf("none");
    std::printf("\n");
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
// ── R3: the fg_core.comp host halves ─────────────────────────────────────────────────────────────
size_t layer_params_bytes() { return kParamCount * 4; }
void layer_params_fill(const Config& c, void* out, bool clean_sim) {
    unsigned char* b = (unsigned char*)out;
    const size_t p_sim = pidx(LayerId::MV_GUIDED, "sim");
    const size_t p_mes = pidx(LayerId::MV_EDGE_SNAP, "sim");
    const float  sim_eff = c.layers.on[(uint16_t)LayerId::MV_GUIDED] ? c.layers.val[p_sim] : 0.f;   // the effective value (a gated param is 0 when its row is off)
    for (size_t p = 0; p < kParamCount; ++p) {
        const ParamDesc& P = kParams[p];
        float v = c.layers.val[p];
        if (p == p_sim && !clean_sim) { volatile float one = 1.0f; const float packed = one + v; v = packed - one; }   // XR1: wap_warp.comp:325 bit-for-bit
        if (p == p_mes && v <= 0.f) v = sim_eff;   // the "0 = use --mv-sim" CLI-COMPAT cascade (present.cpp mes_push), resolved host-side
        if (P.type == ParamType::F32) std::memcpy(b + p * 4, &v, 4);
        else { const int32_t iv = (int32_t)std::lround(v); std::memcpy(b + p * 4, &iv, 4); }
    }
}
uint32_t layer_arm_mask(const ArmInputs& in) {
    uint32_t m = 0;
    for (uint16_t i = 0; i < kLayerCount; ++i) {
        bool ok = false;
        switch (kLayers[i].arm) {   // every ArmId has a case: a new value is a compile-time warning here, not a silent disarm
            case ArmId::ALWAYS:      ok = true; break;
            case ArmId::GME:         ok = in.gme_ok; break;
            case ArmId::BWD:         ok = in.bwd_ok; break;
            case ArmId::GME_AND_BWD: ok = in.gme_ok && in.bwd_ok; break;
            case ArmId::COMMIT:      ok = in.commit_ok; break;
            case ArmId::PRIOR:       ok = in.has_prev; break;
            case ArmId::HOLON:       ok = in.has_prev && in.tier < 4 && !in.holon_skip; break;
            case ArmId::BIDIR_OK:    ok = in.has_prev && in.tier < 5 && !in.pipelined && !in.bwd_skipping; break;
        }
        if (ok) m |= (1u << i);
    }
    return m;
}
void layer_dump(const Config& c) {
    uint16_t order[kLayerCount]; layer_exec_order(order);
    int enabled = 0, real = 0;
    for (uint16_t i = 0; i < kLayerCount; ++i) if (kLayers[i].kind != Kind::X) { ++real; if (c.layers.on[i]) ++enabled; }
    std::printf("[layertab] contract=0x%016llX  (%d rows enabled of %d; %zu params; R7a 2026-09-06: fg_core.comp IS the default product path; --legacy-warp selects wap_warp.comp)\n",
                (unsigned long long)layer_contract_hash(c), enabled, real, (size_t)kParamCount);
    bool sample_printed = false, blend_printed = false;
    for (uint16_t k = 0; k < kLayerCount; ++k) {
        const LayerDesc& L = kLayers[order[k]];
        if (!sample_printed && (uint8_t)L.stage >= (uint8_t)Stage::WEIGHT) {   // the core split (COLUMN_CLOSURE §2.1): fg_sample before WEIGHT, fg_blend before COMPOSE
            std::printf("CORE       fg_sample        res_ceil=%.3f improv=%.3f agree=%.3f t=<per-tick>  push=CorePush 20 B + gen-scalars 24 B\n", (double)c.res_ceil, (double)c.conf_improv, (double)c.agreement);
            sample_printed = true;
        }
        if (!blend_printed && (uint8_t)L.stage >= (uint8_t)Stage::COMPOSE) {
            std::printf("CORE       fg_blend         wa * A + (1 - wa) * B ; blend = (1-t) prev + t cur (CH_BLEND)\n");
            blend_printed = true;
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
    if (!sample_printed) std::printf("CORE       fg_sample\n");
    if (!blend_printed)  std::printf("CORE       fg_blend\n");
}

// ── --dump-config (the round-trip corpus record) ────────────────────────────────────────────────
void dump_config(const Config& c) {
    const LayerOldShadow& o = c.layers_old;
    std::printf("[dump-config] sizeof(Config)=%zu rows=%u params=%zu\n", sizeof(Config), (unsigned)kLayerCount, (size_t)kParamCount);
    std::printf("[old] mv_guided=%d mv_sim=%.9g mv_edge_snap=%d mv_edge_snap_sim=%.9g inertia=%d inertia_thresh=%.9g bg_reclaim=%.9g phase_anchor=%d ambig=%d vblend=%d vblend_t0=%.9g vblend_strength=%.9g vblend_exact=%d stasis=%d stasis_thresh=%.9g single_track=%d st_no_stasis=%d\n",
                (int)o.mv_guided, (double)o.mv_sim, o.mv_edge_snap, (double)o.mv_edge_snap_sim, (int)o.inertia, (double)o.inertia_thresh, (double)o.bg_reclaim,
                (int)o.phase_anchor, (int)o.ambig, (int)o.vblend, (double)o.vblend_t0, (double)o.vblend_strength, (int)o.vblend_exact,
                (int)o.stasis, (double)o.stasis_thresh, (int)o.single_track, (int)o.st_no_stasis);
    std::printf("[old2] gme=%d gme_gpu=%d gme_gpu_verify=%d gme_irls2=%d objects=%d shapefield=%d obj_fill_rim=%d expire=%d persist_reset=%d scene_memory=%d bidir=%d mv_median=%d mv_smooth=%.9g nvofa=%d mv_consensus=%d\n",
                (int)o.gme, (int)o.gme_gpu, (int)o.gme_gpu_verify, (int)o.gme_irls2, (int)o.objects, (int)o.shapefield, (int)o.obj_fill_rim, (int)o.expire, (int)o.persist_reset,
                (int)o.scene_memory, (int)o.bidir, (int)o.mv_median, (double)o.mv_smooth, (int)o.nvofa, (int)o.mv_consensus);
    std::printf("[new]");
    for (uint16_t i = 0; i < kLayerCount; ++i) if (kLayers[i].kind != Kind::X) std::printf(" %s=%d", kLayers[i].name, (int)c.layers.on[i]);
    for (size_t p = 0; p < kParamCount; ++p) std::printf(" %s.%s=%.9g", kLayers[kParams[p].layer].name, kParams[p].name, (double)c.layers.val[p]);
    std::printf("\n[hash] contract=0x%016llX\n", (unsigned long long)layer_contract_hash(c));
}

// R5: a token may drive several rows (FLOW_ROW_MAP §2.2); the help and the UI model show it once — on the first row
// in execution order that carries it (the dump still lists every row).
static bool first_carrier(const uint16_t* order, uint16_t k, const char* tok) {
    if (!tok) return false;
    for (uint16_t j = 0; j < k; ++j) { const LayerDesc& O = kLayers[order[j]]; if (O.off_flag && !std::strcmp(O.off_flag, tok)) return false; }
    return true;
}
// ── --help section ──────────────────────────────────────────────────────────────────────────────
void print_layer_help() {
    std::printf("\nLAYERS (the registry: src/control/layer_table.def is the single declaration site; --layer-dump prints the resolved chain,\n"
                "        --layer-model-json emits the UI model, --dump-config the parsed record):\n");
    uint16_t order[kLayerCount]; layer_exec_order(order);
    for (uint16_t k = 0; k < kLayerCount; ++k) {
        const LayerDesc& L = kLayers[order[k]];
        if (L.kind == Kind::X) continue;
        if (L.off_flag && first_carrier(order, k, L.off_flag)) {
            std::printf("  %-24s %s OFF (%s rank %u; default %s). %s\n", L.off_flag, L.name, stage_name(L.stage), (unsigned)L.rank, L.default_on ? "ON" : "OFF", L.help);
            for (uint16_t j = (uint16_t)(k + 1); j < kLayerCount; ++j) { const LayerDesc& O = kLayers[order[j]]; if (O.off_flag && !std::strcmp(O.off_flag, L.off_flag)) std::printf("  %-24s (also turns %s OFF)\n", "", O.name); }
        }
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
    std::printf("  R3 (stage 5, shaders/fg_core.comp — the rows above as ONE generated kernel; opt-in until its M4 gate passes):\n"
                "    --fg-core            pin fg_core.comp as the product path -- THE DEFAULT since 2026-09-06 (R7a); this flag now only makes it explicit\n"
                "    --fg-core-ab         run BOTH kernels every tick from the same inputs and count differing pixels (the M4 instrument)\n"
                "    --fg-core-clean-sim  mv_guided.sim = the exact --mv-sim (default: the legacy's packed (1+sim)-1, XR1)\n"
                "    --legacy-warp        select shaders/wap_warp.comp instead -- THE REVERT for the R7a default; the legacy shader stays in the tree\n");
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
        if (L.off_flag && first_carrier(order, k, L.off_flag)) {
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
            char noform[80] = {0};   // R5: a PF_NO_FORM BOOL is rendered as its negative token (a "switch-off"), like a row's off_flag
            if ((P.pflags & PF_NO_FORM) && P.flag && !std::strncmp(P.flag, "--", 2)) { std::snprintf(noform, sizeof noform, "--no-%s", P.flag + 2); flag = noform; }
            std::printf("%s{\"flag\":", first ? "" : ","); json_str(flag); first = false;
            std::printf(",\"group\":"); json_str(L.group);
            char nm[96]; std::snprintf(nm, sizeof nm, "%s: %s", L.name, P.name);
            std::printf(",\"name\":"); json_str(nm);
            std::printf(",\"desc\":"); json_str(P.help);
            if (P.type == ParamType::BOOL && (P.pflags & (PF_NEGATED | PF_NO_FORM))) {
                std::printf(",\"type\":\"switch-off\",\"default\":true}");   // R5: the token turns the BOOL OFF; the UI's off-switch semantics
            } else if (P.type == ParamType::BOOL) {
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
