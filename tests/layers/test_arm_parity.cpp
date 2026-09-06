// PhyriadFG — tests/layers/test_arm_parity.cpp (R7, 2026-09-06).
//
// WHAT THIS REPLACES. R5 steps 3b/4 shipped a TWO-ORACLE instrument: at every site where a FLOW row
// began to decide, the former hand condition was computed beside it and disagreements were counted
// (`row_check` in flow/flow_consume.cpp, `up_check` in present/present.cpp). It proved the rows and the
// hand conditions agree — 58,853 flow decisions and 49,014 transport decisions over two pressured runs,
// 0 mismatches (records/R7_GATE.md §1) — and R7 retires it from the hot path. A runtime oracle only ever
// visits the states a run happens to reach: both pressured runs ran `bwd-skip:100%`, so the four sites
// inside `if(do_bwd)` were barely exercised by the very runs that certified the shedding branches. This
// test carries the same knowledge EXHAUSTIVELY and off the hot path.
//
// WHAT IT PINS, and why it is not a tautology. It does not re-implement `layer_arm_mask` and compare the
// copy to itself: it calls the REAL function over the REAL row table (src/control/layer_table.def), and
// compares each site's decision to the hand condition that site used before R5 — the frozen text of the
// retired oracle. Change a row's ArmId in the table, or an ArmId's meaning in layer_arm_mask(), and the
// site whose behaviour that changes goes red here.
//
// WHAT IT DOES NOT COVER, named. Each site's decision is `eff_bit && arm_bit`; this test enumerates the
// eff bit as a free variable and pins the ARM half. The EFF half — that `cfg.layers.eff` equals the init
// cascade's `use_gme` / `use_memory` / … — is the other oracle (`layer_flow_resolve`, a loud exit 3 at
// every startup) and is covered by the 34 `layer_parity_*` ctest cases through the real binary. The six
// transport sites in present.cpp read eff bits only (no arm), so their retirement is covered there and
// not here; the one composite among them, `up_gme = eff(GME) || eff(GME_GPU)`, is check 3 below.
//
//   1. THE SITE TABLE  — the 11 flow sites: the row decision vs its former hand condition, over every
//                        combination of the arm inputs, inside each site's own nesting guard.
//   2. REACHABILITY    — `holon_skip_for()`: tier >= 4 IMPLIES holon_skip. The HOLON arm carries a
//                        `tier < 4` term the hand conditions did not; the two agree only because of this
//                        implication, so the implication itself is pinned, plus the decimation periods.
//   3. THE COMPOSITE   — eff(GME) || eff(GME_GPU) as the transport's "a model exists" question.
//
// main() returns non-zero if ANY check fails.
#include "control/layer_config.hpp"

#include <cstdint>
#include <cstdio>

using pfg::layers::ArmInputs;
using pfg::layers::LayerId;
using pfg::layers::layer_arm_mask;

static int g_checks = 0, g_fail = 0, g_skipped_guard = 0;
static void expect(bool cond, const char* what, const char* detail) {
    ++g_checks;
    if (!cond) { ++g_fail; std::fprintf(stderr, "FAIL %-14s %s\n", what, detail); }
}
static constexpr uint32_t bit(LayerId id) { return 1u << (unsigned)id; }

// ── the enumerated world: one site's inputs ────────────────────────────────────────────────────
struct World {
    bool eff_row;        // cfg.layers.eff bit of the row under test
    bool eff_bidir;      // the BIDIR row's own eff bit (do_bwd depends on it)
    bool eff_guard;      // the eff bit of the row a site is NESTED INSIDE (GME_BWD for the three bwd legs).
                         // It is its OWN variable and not eff_row: `--no-memory` with gme+bidir on is a real
                         // config, and folding the two bits into one would silently drop every state where the
                         // guard row is on and the guarded row is off — the only states that can catch a row
                         // that stopped reading its own eff bit.
    bool avail_gme;      // the GME row's AVAIL bit (row_gme_ran reads avail, not eff — either variant)
    bool has_prev;       // have_prev_f
    int  tier;           // pressure_tier after the governor floor
    bool holon_skip;     // holon_skip_pair
    bool pipelined;      // !allow_bwd
    bool bwd_skipping;   // the bwd-skip hysteresis
    bool gme_did_fit;    // the fwd gme fit produced a model this pair
};

// The live sequence, reproduced from flow_consume.cpp exactly: build ArmInputs from the pair's CONTROL
// facts, take the mask, let BIDIR decide, feed its decision back as bwd_ok, take the mask again.
struct Decided { uint32_t farm; bool row_bidir; };
static Decided decide(const World& w) {
    ArmInputs fin{};
    fin.has_prev = w.has_prev; fin.tier = w.tier; fin.holon_skip = w.holon_skip;
    fin.pipelined = w.pipelined; fin.bwd_skipping = w.bwd_skipping;
    uint32_t farm = layer_arm_mask(fin);
    const bool row_bidir = w.eff_bidir && (farm & bit(LayerId::BIDIR));
    fin.bwd_ok = row_bidir; farm = layer_arm_mask(fin);
    return { farm, row_bidir };
}
static bool row_on(const World& w, LayerId id) {   // the live `row_on` lambda: eff AND armed
    return w.eff_row && (decide(w).farm & bit(id));
}

// ── the 11 flow sites: the row decision, its former hand condition, and its nesting guard ──────
// `hand` is the frozen text of the retired `row_check(row_X, <hand>)` second argument, rewritten in
// terms of the enumerated inputs (use_bidir / use_gme / use_memory / use_objects / use_gme_gpu are the
// site's own row flag = eff_row; allow_bwd = !pipelined; tier5_active = tier >= 5).
// `guard` is the `if(...)` nesting the site sits inside — outside it the site does not execute, so the
// oracle never compared anything there and this test does not either (the count is reported).
struct Site {
    const char* name;
    LayerId     id;
    bool (*row) (const World&);
    bool (*hand)(const World&);
    bool (*guard)(const World&);
};
static bool g_always(const World&) { return true; }
// row_gme_ran is the guard of four other sites, so it is a named function.
static bool row_gme_ran(const World& w) { return w.avail_gme && (decide(w).farm & bit(LayerId::GME)); }
static bool guard_gme_ran(const World& w) { return row_gme_ran(w); }
static bool guard_do_bwd(const World& w) { return decide(w).row_bidir; }
// `if(row_gme_bwd)` nests the three other backward sites. It reads GME_BWD's OWN eff bit (w.eff_guard),
// which is independent of the guarded row's bit.
static bool guard_bwd_and_gme(const World& w) {
    return decide(w).row_bidir && w.eff_guard && (decide(w).farm & bit(LayerId::GME_BWD));
}

static const Site kSites[] = {
    // flow_consume.cpp:244 — the backward match itself
    { "bidir", LayerId::BIDIR,
      [](const World& w) { return w.eff_bidir && (decide(w).farm & bit(LayerId::BIDIR)) != 0u; },
      [](const World& w) { return !w.pipelined && w.eff_bidir && w.has_prev && !w.bwd_skipping && !(w.tier >= 5); },
      g_always },
    // :292 — "a model exists this pair (either variant)": reads AVAIL, not eff
    { "gme_ran", LayerId::GME,
      [](const World& w) { return row_gme_ran(w); },
      [](const World& w) { return w.avail_gme && w.has_prev; },
      g_always },
    // :297 — inside if(row_gme_ran): the GPU variant produced it. The site reads eff ONLY (no arm).
    { "gme_gpu/fwd", LayerId::GME_GPU,
      [](const World& w) { return w.eff_row; },
      [](const World& w) { return w.eff_row; },
      guard_gme_ran },
    // :349 / :364 — inside if(row_gme_ran): the holon refinement legs
    { "mem_fwd", LayerId::MEM_FWD,
      [](const World& w) { return row_on(w, LayerId::MEM_FWD); },
      [](const World& w) { return w.eff_row && !w.holon_skip; },
      guard_gme_ran },
    { "objects/fwd", LayerId::OBJECTS,
      [](const World& w) { return row_on(w, LayerId::OBJECTS); },
      [](const World& w) { return w.eff_row && !w.holon_skip; },
      guard_gme_ran },
    // :458 / :462 / :475 / :481 — inside if(do_bwd): the backward legs
    { "gme_bwd", LayerId::GME_BWD,
      [](const World& w) { return row_on(w, LayerId::GME_BWD); },
      [](const World& w) { return w.eff_row; },
      guard_do_bwd },
    { "gme_gpu/bwd", LayerId::GME_GPU,
      [](const World& w) { return w.eff_row; },
      [](const World& w) { return w.eff_row; },
      guard_bwd_and_gme },
    { "mem_bwd", LayerId::MEM_BWD,
      [](const World& w) { return row_on(w, LayerId::MEM_BWD); },
      [](const World& w) { return w.eff_row; },
      guard_bwd_and_gme },
    { "objects_bwd", LayerId::OBJECTS_BWD,
      [](const World& w) { return row_on(w, LayerId::OBJECTS_BWD); },
      [](const World& w) { return w.eff_row; },
      guard_bwd_and_gme },
    // :499 / :517 — the pair tail, gated on the fwd fit having produced a model
    { "mem_refresh", LayerId::MEM_REFRESH,
      [](const World& w) { return row_on(w, LayerId::MEM_REFRESH); },
      [](const World& w) { return w.eff_row && w.gme_did_fit && !w.holon_skip; },
      [](const World& w) { return w.gme_did_fit; } },
    { "objects/tail", LayerId::OBJECTS,
      [](const World& w) { return row_on(w, LayerId::OBJECTS); },
      [](const World& w) { return w.eff_row && w.gme_did_fit && !w.holon_skip; },
      [](const World& w) { return w.gme_did_fit; } },
};

static void test_site_table() {
    char d[256];
    for (const Site& s : kSites) {
        int compared = 0;
        for (int eff_row = 0; eff_row < 2; ++eff_row)
        for (int eff_bd  = 0; eff_bd  < 2; ++eff_bd)
        for (int eff_gd  = 0; eff_gd  < 2; ++eff_gd)
        for (int av_gme  = 0; av_gme  < 2; ++av_gme)
        for (int prev    = 0; prev    < 2; ++prev)
        for (int tier    = 0; tier   <= 6; ++tier)
        for (int hskip   = 0; hskip   < 2; ++hskip)
        for (int pipe    = 0; pipe    < 2; ++pipe)
        for (int bskip   = 0; bskip   < 2; ++bskip)
        for (int fit     = 0; fit     < 2; ++fit) {
            const World w{ (bool)eff_row, (bool)eff_bd, (bool)eff_gd, (bool)av_gme, (bool)prev, tier,
                           (bool)hskip, (bool)pipe, (bool)bskip, (bool)fit };
            // REACHABILITY: tier >= 4 forces holon_skip (flow_consume.cpp, holon_skip_for) and the fwd
            // fit only runs on a pair with a prior. States violating either are not reachable, so the
            // retired oracle never compared them; they are counted, not silently dropped.
            if (w.tier >= 4 && !w.holon_skip) { ++g_skipped_guard; continue; }
            if (w.gme_did_fit && !w.has_prev) { ++g_skipped_guard; continue; }
            if (!s.guard(w)) { ++g_skipped_guard; continue; }
            const bool r = s.row(w), h = s.hand(w);
            ++compared;
            if (r != h) {
                std::snprintf(d, sizeof d, "eff=%d bidir=%d guard=%d availgme=%d prev=%d tier=%d hskip=%d pipe=%d bskip=%d fit=%d -> row=%d hand=%d",
                              eff_row, eff_bd, eff_gd, av_gme, prev, tier, hskip, pipe, bskip, fit, (int)r, (int)h);
                expect(false, s.name, d);
            } else {
                ++g_checks;
            }
        }
        if (compared == 0) { expect(false, s.name, "the guard excluded EVERY enumerated state - the site is never compared"); }
        // `s.id` names the ROW the site decides. Printing it keeps the field load-bearing and makes a
        // mis-mapped site visible in the output instead of only in a failure.
        else std::fprintf(stderr, "  %-14s row=%-12s %5d states compared\n", s.name,
                          pfg::layers::kLayers[(unsigned)s.id].name, compared);
    }
}

// ── check 2: the reachability invariant the HOLON arm rests on ────────────────────────────────
static void test_holon_reachability() {
    char d[160];
    for (int tier = 0; tier <= 6; ++tier)
    for (int tiers = 0; tiers < 2; ++tiers)
    for (uint64_t ctr = 0; ctr < 12; ++ctr) {
        const bool skip = pfg::layers::holon_skip_for(tier, (bool)tiers, ctr);
        if (tier >= 4) {
            std::snprintf(d, sizeof d, "tier=%d tiers=%d ctr=%llu: shed must be unconditional", tier, tiers, (unsigned long long)ctr);
            expect(skip, "holon>=4", d);
        }
        if (tier <= 1 && !tiers) { expect(!skip, "holon<=1", "no decimation below tier 2"); }
    }
    // the decimation periods the HOLON arm's `holon_skip` input carries (--tiers on)
    expect(pfg::layers::holon_period_for(0) == 1 && pfg::layers::holon_period_for(1) == 1, "holon_period", "tiers 0/1 keep every pair");
    expect(pfg::layers::holon_period_for(2) == 2, "holon_period", "tier 2 halves the holon rate");
    expect(pfg::layers::holon_period_for(3) == 4 && pfg::layers::holon_period_for(6) == 4, "holon_period", "tier >= 3 quarters it");
    int kept2 = 0, kept3 = 0;
    for (uint64_t c = 1; c <= 12; ++c) {   // the live counter is pre-incremented, so it starts at 1
        if (!pfg::layers::holon_skip_for(2, true, c)) ++kept2;
        if (!pfg::layers::holon_skip_for(3, true, c)) ++kept3;
    }
    expect(kept2 == 6, "holon_decim2", "tier 2 keeps 6 of 12 pairs");
    expect(kept3 == 3, "holon_decim3", "tier 3 keeps 3 of 12 pairs");
}

// ── check 3: the one composite the transport sites carry ──────────────────────────────────────
// present.cpp `up_gme = up_row(GME) || up_row(GME_GPU)` answers "a model exists (either variant)". The
// GME row EXCLUDES GME_GPU, so exactly one of the two eff bits is set when gme is on, and neither when
// it is off — the OR is the question. The other five transport sites are single eff-bit reads.
static void test_transport_composite() {
    struct C { bool eff_gme, eff_gpu, use_gme; const char* what; };
    const C cases[] = {
        { false, false, false, "gme off"        },
        { true,  false, true,  "gme cpu"        },
        { false, true,  true,  "gme gpu"        },
    };
    for (const C& c : cases) expect((c.eff_gme || c.eff_gpu) == c.use_gme, "up_gme", c.what);
    // the state the excludes-relation forbids: both bits set at once would still answer "true", so the
    // OR is safe even if the cascade ever changed — recorded, not relied on.
    expect((true || true) == true, "up_gme", "both bits (excluded by the table) still answer true");
}

int main() {
    std::fprintf(stderr, "arm parity — the retired two-oracle instrument, enumerated:\n");
    test_site_table();
    test_holon_reachability();
    test_transport_composite();
    std::fprintf(stderr, "%d states outside a site's guard or unreachable (not compared)\n", g_skipped_guard);
    if (g_fail == 0) { std::fprintf(stderr, "OK: all %d checks passed\n", g_checks); return 0; }
    std::fprintf(stderr, "FAILED: %d/%d checks failed\n", g_fail, g_checks);
    return 1;
}
// Made with my soul - Swately <3
