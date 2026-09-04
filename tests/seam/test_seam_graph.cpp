// PhyriadFG — tests/seam/test_seam_graph.cpp (adopted from apps/minimal_fg/test/ at R2, 2026-09-03).
//
// GPU-FREE unit test for the SG seam engine (minimal_fg/seam_graph.hpp).
// Exercises the COMPILE+DUMP side only - NO VkDevice, NO queue, NO real images.
// VkImage handles are MOCKED (fake uint64 values cast to VkImage); execute() is not called.
//
// The five checks (MINIMAL_FG_IMPLEMENTATION_STRATEGIES S1.5 / MINIMAL_FG_RISK_REGISTER MR-3):
//   1. GOLDEN      - the canonical capture[imported]->flow->warp->blend->present[output]
//                    chain; the derived barrier list + dump() match a hand-computed golden.
//   2. LAYERABILITY- inserting a disocclusion pass adds EXACTLY +1 barrier (one declared edge).
//   3. CULL        - a dead pass (writes a resource nothing reads, not an output) is culled.
//   4. DETERMINISM - compile() twice -> byte-identical dump().
//   5. WAW/WAR     - a write-after-write + write-after-read emit the correct precise deps.
//
// main() returns non-zero if ANY check fails.

#include "seam/seam_graph.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using pfg::seam::Access;
using pfg::seam::Barrier;
using pfg::seam::Compiled;
using pfg::seam::Hazard;
using pfg::seam::ResId;
using pfg::seam::SeamGraph;

// -- declared stage / access / layout shorthands -------------------------------
static constexpr VkPipelineStageFlags2 CS  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
static constexpr VkPipelineStageFlags2 CP  = VK_PIPELINE_STAGE_2_COPY_BIT;
static constexpr VkPipelineStageFlags2 FS  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
static constexpr VkPipelineStageFlags2 ALL = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;  // the anti-pattern
static constexpr VkAccessFlags2 SR   = VK_ACCESS_2_SHADER_READ_BIT;
static constexpr VkAccessFlags2 UR   = VK_ACCESS_2_UNIFORM_READ_BIT;
static constexpr VkAccessFlags2 SW   = VK_ACCESS_2_SHADER_WRITE_BIT;
static constexpr VkAccessFlags2 TR   = VK_ACCESS_2_TRANSFER_READ_BIT;
static constexpr VkAccessFlags2 TW   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
static constexpr VkImageLayout  GEN  = VK_IMAGE_LAYOUT_GENERAL;
static constexpr VkImageLayout  SRO  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
static constexpr VkImageLayout  TSRC = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
static constexpr VkImageLayout  TDST = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

// -- tiny test harness ---------------------------------------------------------
static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                \
    do {                                                                           \
        ++g_checks;                                                                \
        if (!(cond)) {                                                             \
            ++g_fail;                                                              \
            std::fprintf(stderr, "  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                          \
    } while (0)

static void check_str_eq(const std::string& got, const std::string& want, const char* what) {
    ++g_checks;
    if (got == want) return;
    ++g_fail;
    std::fprintf(stderr, "  FAIL: %s string mismatch\n--- got (%zu) ---\n%s--- want (%zu) ---\n%s---\n",
                 what, got.size(), got.c_str(), want.size(), want.c_str());
}

// Flatten a compiled graph's barriers (in execution order) for field-by-field assertions.
static std::vector<Barrier> flat(const Compiled& c) {
    std::vector<Barrier> v;
    for (const auto& p : c.passes)
        for (const auto& b : p.barriers) v.push_back(b);
    return v;
}

static bool no_all_commands(const Compiled& c) {
    for (const Barrier& b : flat(c))
        if (b.vk.srcStageMask == ALL || b.vk.dstStageMask == ALL) return false;
    return true;
}

// Build the canonical MC-1..MC-5 chain. Returns the resource ids via out-params.
struct ChainIds { ResId cap, flow_out, warp_out, blend_out, present_out; };
static ChainIds build_chain(SeamGraph& g) {
    ChainIds r;
    r.cap         = g.declare_image("capture", /*imported=*/true);
    r.flow_out    = g.declare_image("flow_out");
    r.warp_out    = g.declare_image("warp_out");
    r.blend_out   = g.declare_image("blend_out");
    r.present_out = g.declare_image("present_out");
    g.add_pass("flow",    {{r.cap,       CS, SR, GEN}},  {{r.flow_out,    CS, SW, GEN}});
    g.add_pass("warp",    {{r.flow_out,  CS, SR, GEN}},  {{r.warp_out,    CS, SW, GEN}});
    g.add_pass("blend",   {{r.warp_out,  CS, SR, GEN}},  {{r.blend_out,   CS, SW, GEN}});
    g.add_pass("present", {{r.blend_out, CP, TR, TSRC}}, {{r.present_out, CP, TW, TDST}});
    g.mark_output(r.present_out);
    return r;
}

// The hand-computed golden dump for the canonical chain (must match dump() byte-for-byte).
static const char* kGoldenChainDump =
    "SG: 4 passes, 4 barriers\n"
    "pass 0 \"flow\"\n"
    "  RAW res=0:\"capture\" EXTERNAL->flow src=NONE:NONE dst=COMPUTE_SHADER:SHADER_READ layout=SHADER_READ_ONLY_OPTIMAL->GENERAL\n"
    "pass 1 \"warp\"\n"
    "  RAW res=1:\"flow_out\" flow->warp src=COMPUTE_SHADER:SHADER_WRITE dst=COMPUTE_SHADER:SHADER_READ layout=GENERAL->GENERAL\n"
    "pass 2 \"blend\"\n"
    "  RAW res=2:\"warp_out\" warp->blend src=COMPUTE_SHADER:SHADER_WRITE dst=COMPUTE_SHADER:SHADER_READ layout=GENERAL->GENERAL\n"
    "pass 3 \"present\"\n"
    "  RAW res=3:\"blend_out\" blend->present src=COMPUTE_SHADER:SHADER_WRITE dst=COPY:TRANSFER_READ layout=GENERAL->TRANSFER_SRC_OPTIMAL\n";

// -- 1. GOLDEN -----------------------------------------------------------------
static void test_golden() {
    std::fprintf(stderr, "[1] GOLDEN\n");
    SeamGraph g;
    const ChainIds id = build_chain(g);

    // Exercise the mock-handle API surface (handles never dereferenced; not asserted).
    g.bind_image(id.cap,       (VkImage)(uintptr_t)0xA0000001ULL);
    g.bind_image(id.present_out,(VkImage)(uintptr_t)0xA0000005ULL);

    const Compiled c = g.compile();

    CHECK(c.errors.empty());
    CHECK(c.passes.size() == 4);
    CHECK(c.barrier_count() == 4);
    CHECK(no_all_commands(c));

    if (c.passes.size() == 4) {
        CHECK(c.passes[0].name == "flow");
        CHECK(c.passes[1].name == "warp");
        CHECK(c.passes[2].name == "blend");
        CHECK(c.passes[3].name == "present");
        // exactly one barrier precedes each pass
        CHECK(c.passes[0].barriers.size() == 1);
        CHECK(c.passes[1].barriers.size() == 1);
        CHECK(c.passes[2].barriers.size() == 1);
        CHECK(c.passes[3].barriers.size() == 1);
    }

    const std::vector<Barrier> b = flat(c);
    CHECK(b.size() == 4);
    if (b.size() == 4) {
        // B0: imported acquire (capture) - producer EXTERNAL, real layout transition.
        CHECK(b[0].res == id.cap);
        CHECK(b[0].hazard == Hazard::RAW);
        CHECK(b[0].producer_pass == -1);                 // EXTERNAL
        CHECK(b[0].vk.srcStageMask  == VK_PIPELINE_STAGE_2_NONE);
        CHECK(b[0].vk.srcAccessMask == VK_ACCESS_2_NONE);
        CHECK(b[0].vk.dstStageMask  == CS);
        CHECK(b[0].vk.dstAccessMask == SR);
        CHECK(b[0].vk.oldLayout == SRO);
        CHECK(b[0].vk.newLayout == GEN);

        // B1: flow_out RAW (flow -> warp), precise compute->compute.
        CHECK(b[1].res == id.flow_out);
        CHECK(b[1].hazard == Hazard::RAW);
        CHECK(b[1].vk.srcStageMask  == CS);
        CHECK(b[1].vk.srcAccessMask == SW);
        CHECK(b[1].vk.dstStageMask  == CS);
        CHECK(b[1].vk.dstAccessMask == SR);
        CHECK(b[1].vk.oldLayout == GEN);
        CHECK(b[1].vk.newLayout == GEN);

        // B2: warp_out RAW (warp -> blend).
        CHECK(b[2].res == id.warp_out);
        CHECK(b[2].hazard == Hazard::RAW);
        CHECK(b[2].vk.srcStageMask  == CS);
        CHECK(b[2].vk.srcAccessMask == SW);
        CHECK(b[2].vk.dstStageMask  == CS);
        CHECK(b[2].vk.dstAccessMask == SR);

        // B3: blend_out RAW (blend -> present): cross-stage compute->transfer + layout transition.
        CHECK(b[3].res == id.blend_out);
        CHECK(b[3].hazard == Hazard::RAW);
        CHECK(b[3].vk.srcStageMask  == CS);
        CHECK(b[3].vk.srcAccessMask == SW);
        CHECK(b[3].vk.dstStageMask  == CP);
        CHECK(b[3].vk.dstAccessMask == TR);
        CHECK(b[3].vk.oldLayout == GEN);
        CHECK(b[3].vk.newLayout == TSRC);
    }

    // dump() equals the hand-computed golden, byte-for-byte.
    check_str_eq(g.dump(c), kGoldenChainDump, "golden chain");
}

// -- 2. LAYERABILITY: +1 pass == +1 barrier ------------------------------------
static void test_layerability() {
    std::fprintf(stderr, "[2] LAYERABILITY\n");

    // Base = the canonical chain.
    SeamGraph base;
    build_chain(base);
    const Compiled cbase = base.compile();
    CHECK(cbase.barrier_count() == 4);

    // Layered: insert a disocclusion pass between warp and blend.
    //   disoccl: reads warp_out, writes mask.   blend: now also reads mask.
    SeamGraph g;
    const ResId cap     = g.declare_image("capture", /*imported=*/true);
    const ResId flowo   = g.declare_image("flow_out");
    const ResId warpo   = g.declare_image("warp_out");
    const ResId mask    = g.declare_image("disoccl_mask");
    const ResId blendo  = g.declare_image("blend_out");
    const ResId preso   = g.declare_image("present_out");
    g.add_pass("flow",    {{cap,    CS, SR, GEN}},               {{flowo,  CS, SW, GEN}});
    g.add_pass("warp",    {{flowo,  CS, SR, GEN}},               {{warpo,  CS, SW, GEN}});
    g.add_pass("disoccl", {{warpo,  CS, SR, GEN}},               {{mask,   CS, SW, GEN}});
    g.add_pass("blend",   {{warpo,  CS, SR, GEN}, {mask, CS, SR, GEN}}, {{blendo, CS, SW, GEN}});
    g.add_pass("present", {{blendo, CP, TR, TSRC}},              {{preso,  CP, TW, TDST}});
    g.mark_output(preso);

    const Compiled c = g.compile();
    CHECK(c.errors.empty());
    CHECK(c.passes.size() == 5);

    // THE key property: exactly +1 barrier vs the base graph.
    CHECK(c.barrier_count() == cbase.barrier_count() + 1);

    // Pass order is correct (disoccl strictly between warp and blend).
    if (c.passes.size() == 5) {
        CHECK(c.passes[0].name == "flow");
        CHECK(c.passes[1].name == "warp");
        CHECK(c.passes[2].name == "disoccl");
        CHECK(c.passes[3].name == "blend");
        CHECK(c.passes[4].name == "present");

        // The +1 is the mask edge (disoccl -> blend); blend's SECOND read of warp_out is COVERED
        // by the warp->disoccl barrier (same scope, same layout) and emits NO extra barrier.
        CHECK(c.passes[3].barriers.size() == 1);            // blend has exactly one (mask), not two
        if (c.passes[3].barriers.size() == 1) {
            const Barrier& mb = c.passes[3].barriers[0];
            CHECK(mb.res == mask);
            CHECK(mb.hazard == Hazard::RAW);
            CHECK(c.passes[(size_t)mb.producer_pass].name == "disoccl");
            CHECK(mb.vk.srcStageMask == CS && mb.vk.srcAccessMask == SW);
            CHECK(mb.vk.dstStageMask == CS && mb.vk.dstAccessMask == SR);
        }
        // warp_out's barrier moved to its FIRST reader (disoccl), not blend.
        CHECK(c.passes[2].barriers.size() == 1);
        if (c.passes[2].barriers.size() == 1)
            CHECK(c.passes[2].barriers[0].res == warpo);
    }
    CHECK(no_all_commands(c));
}

// -- 3. CULL: a dead pass is removed entirely ----------------------------------
static void test_cull() {
    std::fprintf(stderr, "[3] CULL\n");
    SeamGraph g;
    const ChainIds id = build_chain(g);
    // A dead pass: writes a resource nothing reads, not a marked output.
    const ResId dead_res = g.declare_image("dead_res");
    g.add_pass("dead", /*reads=*/{}, /*writes=*/{{dead_res, CS, SW, GEN}});

    const Compiled c = g.compile();
    CHECK(c.errors.empty());
    CHECK(c.passes.size() == 4);            // dead pass culled
    CHECK(c.barrier_count() == 4);          // and contributes no barrier

    for (const auto& p : c.passes) CHECK(p.name != "dead");
    for (const Barrier& b : flat(c)) {
        CHECK(b.res != dead_res);
        CHECK(b.res != id.present_out);     // the (unconsumed) present output emits nothing either
    }
}

// -- 4. DETERMINISM: compile() twice -> identical dump() -----------------------
static void test_determinism() {
    std::fprintf(stderr, "[4] DETERMINISM\n");
    SeamGraph g;
    build_chain(g);
    const Compiled c1 = g.compile();
    const Compiled c2 = g.compile();
    check_str_eq(g.dump(c1), g.dump(c2), "determinism (chain)");
    CHECK(g.dump(c1) == kGoldenChainDump);  // and still the golden across recompiles

    // Independent graph instance with the same declaration -> identical dump (no hidden state).
    SeamGraph g2;
    build_chain(g2);
    check_str_eq(g2.dump(g2.compile()), kGoldenChainDump, "determinism (fresh instance)");
}

// -- 5. WAW / WAR: correct precise dependency barriers -------------------------
static void test_waw_war() {
    std::fprintf(stderr, "[5] WAW/WAR\n");
    SeamGraph g;
    const ResId X = g.declare_image("X");
    const ResId Y = g.declare_image("Y");
    g.add_pass("A", /*r*/{},            /*w*/{{X, CS, SW, GEN}});   // first write of X (no barrier)
    g.add_pass("B", /*r*/{},            /*w*/{{X, CS, SW, GEN}});   // WAW vs A
    g.add_pass("C", /*r*/{{X, CS, SR, GEN}}, /*w*/{{Y, CS, SW, GEN}}); // RAW (reads X), produces Y
    g.add_pass("D", /*r*/{},            /*w*/{{X, CS, SW, GEN}});   // WAR vs C's read of X
    g.mark_output(X);                                              // keep all writers of X live
    g.mark_output(Y);                                              // keep C live

    const Compiled c = g.compile();
    CHECK(c.errors.empty());
    CHECK(c.passes.size() == 4);
    CHECK(c.barrier_count() == 3);          // WAW + RAW + WAR ; A's first write emits nothing
    CHECK(no_all_commands(c));

    const std::vector<Barrier> b = flat(c);
    CHECK(b.size() == 3);
    if (b.size() == 3) {
        // [0] WAW (A -> B): flush prior write before the new write.
        CHECK(b[0].hazard == Hazard::WAW);
        CHECK(b[0].res == X);
        CHECK(b[0].vk.srcStageMask  == CS);
        CHECK(b[0].vk.srcAccessMask == SW);   // prior writer's write flushed
        CHECK(b[0].vk.dstStageMask  == CS);
        CHECK(b[0].vk.dstAccessMask == SW);

        // [1] RAW (B -> C): the read sees B's write.
        CHECK(b[1].hazard == Hazard::RAW);
        CHECK(b[1].res == X);
        CHECK(b[1].vk.srcStageMask  == CS);
        CHECK(b[1].vk.srcAccessMask == SW);
        CHECK(b[1].vk.dstStageMask  == CS);
        CHECK(b[1].vk.dstAccessMask == SR);

        // [2] WAR (C -> D): EXECUTION dependency only - the new write waits for the prior read.
        //     srcAccessMask MUST be 0 (reads do not dirty memory; no flush) and NEVER over-broad.
        CHECK(b[2].hazard == Hazard::WAR);
        CHECK(b[2].res == X);
        CHECK(b[2].vk.srcStageMask  == CS);   // precise: the reader's stage, not ALL_COMMANDS
        CHECK(b[2].vk.srcAccessMask == 0u);   // WAR is execution-only
        CHECK(b[2].vk.dstStageMask  == CS);
        CHECK(b[2].vk.dstAccessMask == SW);
        CHECK(b[2].vk.srcStageMask != ALL);
    }
}

// -- 6. CROSS-SCOPE READS (adversarial CRITICAL fix): a reader whose (stage,access) pair was never
//      made visible MUST get its own barrier. The buggy two-decoupled-unions model skipped it. ------
static void test_cross_scope_reads() {
    std::fprintf(stderr, "[6] CROSS-SCOPE READS (silent-RAW guard)\n");
    SeamGraph g;
    const ResId X  = g.declare_image("X");
    const ResId O1 = g.declare_image("O1");
    const ResId O2 = g.declare_image("O2");
    const ResId O3 = g.declare_image("O3");
    const ResId O4 = g.declare_image("O4");
    g.add_pass("A",  /*r*/{},                 /*w*/{{X, CS, SW, GEN}});   // producer
    g.add_pass("R1", {{X, FS, UR, GEN}},      {{O1, CS, SW, GEN}});       // visible: (FS,UR)
    g.add_pass("R2", {{X, CS, SR, GEN}},      {{O2, CS, SW, GEN}});       // visible: (CS,SR)
    g.add_pass("R3", {{X, FS, SR, GEN}},      {{O3, CS, SW, GEN}});       // (FS,SR) cross-pair: NEVER visible
    g.add_pass("R4", {{X, CS, SR, GEN}},      {{O4, CS, SW, GEN}});       // == R2 scope -> COVERED
    g.mark_output(O1); g.mark_output(O2); g.mark_output(O3); g.mark_output(O4);

    const Compiled c = g.compile();
    CHECK(c.errors.empty());
    CHECK(c.passes.size() == 5);

    // X must carry 3 barriers (R1, R2, R3); R4 is covered. The decoupled-union bug yields 2 (misses R3).
    int xbar = 0;
    for (const Barrier& b : flat(c)) if (b.res == X) ++xbar;
    CHECK(xbar == 3);

    if (c.passes.size() == 5) {
        CHECK(c.passes[1].name == "R1");
        CHECK(c.passes[3].name == "R3");
        CHECK(c.passes[4].name == "R4");
        // R3 (the cross-pair) MUST have its own X barrier - the regression guard for the critical fix.
        bool r3_has_x = false;
        for (const Barrier& b : c.passes[3].barriers) if (b.res == X) r3_has_x = true;
        CHECK(r3_has_x);
        // R4 (same scope as R2) is COVERED -> no X barrier.
        bool r4_has_x = false;
        for (const Barrier& b : c.passes[4].barriers) if (b.res == X) r4_has_x = true;
        CHECK(!r4_has_x);
    }
    CHECK(no_all_commands(c));
}

// -- 7. RMW emits NO redundant WAW (adversarial over-sync fix): a pass that reads-then-writes a
//      resource is already ordered after the producer by its read's RAW; a second WAW is redundant. --
static void test_rmw_no_redundant_waw() {
    std::fprintf(stderr, "[7] RMW no-redundant-WAW (over-sync guard)\n");
    SeamGraph g;
    const ResId X = g.declare_image("X");
    g.add_pass("A", /*r*/{},                 /*w*/{{X, CS, SW, GEN}});   // first write (no barrier)
    g.add_pass("B", {{X, CS, SR, GEN}},      {{X, CS, SW, GEN}});        // RMW: read X then write X
    g.mark_output(X);

    const Compiled c = g.compile();
    CHECK(c.errors.empty());
    CHECK(c.passes.size() == 2);
    // ONLY the RAW (A->B for the read). The redundant WAW against the same producer is skipped.
    CHECK(c.barrier_count() == 1);
    const std::vector<Barrier> b = flat(c);
    if (b.size() >= 1) {
        CHECK(b[0].hazard == Hazard::RAW);
        CHECK(b[0].res == X);
    }
    for (const Barrier& bb : flat(c)) CHECK(bb.hazard != Hazard::WAW);  // no WAW at all on the RMW
    CHECK(no_all_commands(c));
}

// -- 8. LAYOUT-AFTER-READ is LOUD (adversarial fix): a layout transition after a prior reader is not
//      fully supported in the first cut -> it MUST be flagged in errors, never a silent wrong barrier. --
static void test_layout_after_read_is_loud() {
    std::fprintf(stderr, "[8] LAYOUT-AFTER-READ is loud (no silent wrong barrier)\n");
    SeamGraph g;
    const ResId X  = g.declare_image("X");
    const ResId O1 = g.declare_image("O1");
    const ResId O2 = g.declare_image("O2");
    g.add_pass("A",  /*r*/{},                 /*w*/{{X, CS, SW, GEN}});
    g.add_pass("R1", {{X, CS, SR, GEN}},      {{O1, CS, SW, GEN}});      // reads X@GENERAL
    g.add_pass("R2", {{X, FS, SR, SRO}},      {{O2, CS, SW, GEN}});      // reads X@SRO AFTER R1: unsupported
    g.mark_output(O1); g.mark_output(O2);

    const Compiled c = g.compile();
    bool flagged = false;
    for (const std::string& e : c.errors)
        if (e.find("layout-transition-after-read") != std::string::npos) flagged = true;
    CHECK(flagged);  // never silent: the caller is told this graph is not first-cut-supported
}

// ── [9] R2 GRAFTS (G3 dominance warning · G4 optional_write liveness · G5 zero-alloc execute) ──
// Added when the engine was adopted into PhyriadFG (docs/planning/records/R2_GATE.md). Each graft gets
// its own regression check; none of them may move dump() — checks 1..8 above are the guard for that.
static void test_grafts_r2() {
    std::fprintf(stderr, "[9] R2 GRAFTS (dominance / optional-write / zero-alloc execute)\n");

    // G3 — a pass that OVERWRITES what it does not read is named at compile time.
    {
        SeamGraph g;
        const ResId a = g.declare_image("a");
        Compiled c;
        g.add_pass("producer", {}, { Access{ a, CS, SW, GEN } });
        g.add_pass("clobber",  {}, { Access{ a, CS, SW, GEN } });   // writes a, never reads it
        g.mark_output(a);
        c = g.compile();
        CHECK(c.errors.empty());
        CHECK(c.warnings.size() == 1);
        CHECK(!c.warnings.empty() && c.warnings[0].kind == pfg::seam::Warning::Kind::Dominance);
        CHECK(!c.warnings.empty() && c.warnings[0].res == a);
        // the DECLARED form is silent, and the derived barriers are identical either way
        SeamGraph g2;
        const ResId a2 = g2.declare_image("a");
        g2.add_pass("producer", {}, { Access{ a2, CS, SW, GEN } });
        g2.add_pass_dominating("clobber", {}, { Access{ a2, CS, SW, GEN } }, "the full-frame clear owns it");
        g2.mark_output(a2);
        Compiled c2 = g2.compile();
        CHECK(c2.warnings.empty());
        check_str_eq(g2.dump(c2), g.dump(c), "dominance: dump() is unchanged by the declaration");
    }

    // G4 — an optional write nothing live reads is reported; one a later pass reads is not.
    {
        SeamGraph g;
        const ResId out = g.declare_image("out");
        const ResId aux = g.declare_image("aux");
        g.add_pass("gen", {}, { Access{ out, CS, SW, GEN }, Access{ aux, CS, SW, GEN, /*optional*/ true } });
        g.mark_output(out);
        Compiled c = g.compile();
        CHECK(c.dead_optional_writes.size() == 1);
        CHECK(!c.dead_optional_writes.empty() && c.dead_optional_writes[0] == std::string("gen:aux"));

        SeamGraph g2;
        const ResId out2 = g2.declare_image("out");
        const ResId aux2 = g2.declare_image("aux");
        g2.add_pass("gen",      {}, { Access{ out2, CS, SW, GEN }, Access{ aux2, CS, SW, GEN, true } });
        g2.add_pass("consumer", { Access{ aux2, CS, SR, GEN } }, { Access{ out2, CS, SW, GEN } });
        g2.mark_output(out2);
        Compiled c2 = g2.compile();
        CHECK(c2.dead_optional_writes.empty());   // aux is live: a later pass reads it
    }

    // G5 — the Vulkan barrier array is built at compile(); execute() only patches handles. The array
    // must exist and match the derived barriers 1:1 for every pass, and a Compiled from ANOTHER graph
    // must be refused (XR11) rather than barrier the wrong images.
    {
        SeamGraph g;
        const ResId a = g.declare_image("a", /*imported*/ true);
        const ResId b = g.declare_image("b");
        g.add_pass("read_a_write_b", { Access{ a, CS, SR, SRO } }, { Access{ b, CS, SW, GEN } });
        g.add_pass("blit",           { Access{ b, CP, TR, TSRC } }, { Access{ a, CP, TW, TDST } });
        g.mark_output(a);
        Compiled c = g.compile();
        size_t vk_total = 0, der_total = 0;
        for (const auto& p : c.passes) { vk_total += p.vk_barriers.size(); der_total += p.barriers.size(); }
        CHECK(vk_total == der_total);
        CHECK(der_total == c.barrier_count());
        CHECK(c.graph_id != 0);
        SeamGraph other;
        const ResId z = other.declare_image("z");
        other.add_pass("p", {}, { Access{ z, CS, SW, GEN } });
        other.mark_output(z);
        Compiled co = other.compile();
        CHECK(co.graph_id != c.graph_id);   // two graphs never share an id -> execute() can refuse
    }
}

// ── [10] THE STAGE-5 SHAPE (R2): what the engine derives for PhyriadFG's real generation output ──
// The shipping default records, per tick, in src/present/present.cpp:
//     warp dispatch                        writes wapOutA   (COMPUTE / SHADER_WRITE / GENERAL)
//   [ overlay dispatch, --fps-overlay      RMW    wapOutA ]
//     img_barrier wapOutA  GENERAL -> TRANSFER_SRC   (SHADER_WRITE -> TRANSFER_READ)
//     img_barrier bridge   UNDEFINED -> TRANSFER_DST (0 -> TRANSFER_WRITE)
//     vkCmdBlitImage wapOutA -> bridge
//     img_barrier wapOutA  TRANSFER_SRC -> GENERAL   (TRANSFER_READ -> SHADER_WRITE)   <- the RESTORE
// This check declares that graph with the R2b capabilities (a per-image IMPORT layout and a declared
// RESTING layout) and asserts the engine derives EXACTLY the three hand-written barriers, field by
// field. That equality is the evidence the flip needs: replacing the hand-written barriers with
// sg.execute() then changes which code emits them, not what the GPU is told.
static void test_stage5_shape() {
    std::fprintf(stderr, "[10] STAGE-5 SHAPE (the real generation output path)\n");
    SeamGraph g;
    // wapOutA is internal and LIVES in GENERAL: the next frame's warp writes it there. That per-frame
    // invariant is declared, not left to a hand-written trailing barrier (R2b, GAP B).
    const ResId wapOut = g.declare_image("wapOutA");
    // the present bridge is imported and its contents are DISCARDABLE — the blit overwrites the whole
    // image, which is why the hand-written barrier uses UNDEFINED as the old layout (R2b, GAP A).
    const ResId bridge = g.declare_image("bridge_img", /*imported*/ true, VK_IMAGE_LAYOUT_UNDEFINED);
    g.set_resting(wapOut, GEN, CS, SW);
    g.add_pass("warp", {}, { Access{ wapOut, CS, SW, GEN } });
    g.add_pass("blit", { Access{ wapOut, CP, TR, TSRC } }, { Access{ bridge, CP, TW, TDST } });
    g.mark_output(bridge);
    Compiled c = g.compile();
    CHECK(c.errors.empty());
    const std::vector<Barrier> b = flat(c);

    // The hand-written sequence in src/present/present.cpp, barrier for barrier:
    //   #1 wapOutA GENERAL -> TRANSFER_SRC   (SHADER_WRITE -> TRANSFER_READ)
    //   #2 bridge  UNDEFINED -> TRANSFER_DST (0 -> TRANSFER_WRITE)
    //   #3 wapOutA TRANSFER_SRC -> GENERAL   (TRANSFER_READ -> SHADER_WRITE)   [the restore]
    CHECK(b.size() == 2);              // #1 and #2 are per-pass barriers
    CHECK(c.epilogue.size() == 1);     // #3 is the epilogue

    bool one = false, two = false;
    for (const Barrier& x : b) {
        if (x.res == wapOut && x.vk.srcStageMask == CS && x.vk.srcAccessMask == SW
            && x.vk.dstStageMask == CP && x.vk.dstAccessMask == TR
            && x.vk.oldLayout == GEN && x.vk.newLayout == TSRC) one = true;
        if (x.res == bridge && x.vk.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && x.vk.newLayout == TDST && x.vk.dstStageMask == CP && x.vk.dstAccessMask == TW) two = true;
    }
    CHECK(one);   // == hand-written barrier #1, derived
    CHECK(two);   // == hand-written barrier #2, derived — UNDEFINED, not SHADER_READ_ONLY (GAP A closed)
    if (c.epilogue.size() == 1) {
        const Barrier& e = c.epilogue[0];
        CHECK(e.res == wapOut);
        CHECK(e.vk.oldLayout == TSRC && e.vk.newLayout == GEN);     // == hand-written barrier #3
        CHECK(e.vk.srcStageMask == CP);                             // the blit's read is what we wait on
        CHECK(e.vk.srcAccessMask == VK_ACCESS_2_NONE);              // reads do not dirty memory
        CHECK(e.vk.dstStageMask == CS && e.vk.dstAccessMask == SW);  // the next frame's warp
    }
    // no ALL_COMMANDS anywhere — the anti-pattern the whole seam exists to remove
    CHECK(no_all_commands(c));
    for (const Barrier& e : c.epilogue) CHECK(e.vk.srcStageMask != ALL && e.vk.dstStageMask != ALL);

    // A resource with NO declared resting layout gets NO epilogue (the default is inert).
    {
        SeamGraph g2;
        const ResId a = g2.declare_image("a");
        const ResId out = g2.declare_image("out", true, VK_IMAGE_LAYOUT_UNDEFINED);
        g2.add_pass("w", {}, { Access{ a, CS, SW, GEN } });
        g2.add_pass("b", { Access{ a, CP, TR, TSRC } }, { Access{ out, CP, TW, TDST } });
        g2.mark_output(out);
        Compiled c2 = g2.compile();
        CHECK(c2.epilogue.empty());
    }
}

int main() {
    std::fprintf(stderr, "test_seam_graph - SG seam engine (GPU-free)\n");
    test_golden();
    test_layerability();
    test_cull();
    test_determinism();
    test_waw_war();
    test_cross_scope_reads();
    test_rmw_no_redundant_waw();
    test_layout_after_read_is_loud();
    test_grafts_r2();
    test_stage5_shape();

    if (g_fail == 0) {
        std::fprintf(stderr, "OK: all %d checks passed\n", g_checks);
        return 0;
    }
    std::fprintf(stderr, "FAILED: %d/%d checks failed\n", g_fail, g_checks);
    return 1;
}

// Made with my soul - Swately <3
