// ADOPTED INTO PhyriadFG 2026-09-03 (R2 / S4.0 of docs/planning/CONVERGENCE_MASTER_PLAN.md).
// Origin: F:\Phyriad\apps\minimal_fg\include\minimal_fg\seam_graph.hpp (the June minimal-FG arc).
// THIS COPY IS CANONICAL from the adoption commit: the R2 grafts (G3 dominance warning, G4
// optional_write + liveness, G5 zero-allocation execute) land HERE and not in the container copy,
// which is frozen as the proof of concept (risk XR10). Adoption edits, complete: the namespace
// (minimal_fg -> pfg::seam), this note, and the authorship line. The derivation is untouched.

// apps/minimal_fg/include/minimal_fg/seam_graph.hpp
//
// SG - the "poor-man's render graph" seam for the PhyriadFG minimal-FG core.
//
// PURPOSE (MINIMAL_FG_MASTER_PLAN sec 0.1 / sec 3; MINIMAL_FG_IMPLEMENTATION_STRATEGIES S1;
// MINIMAL_FG_RISK_REGISTER MR-3): a tiny, correct, GPU-FREE-TESTABLE render-graph that
// DERIVES Vulkan barriers from declared per-pass reads/writes, so the minimal core
// (capture -> flow -> warp -> blend -> present) adds/removes a quality pass for the cost
// of ONE declared edge - never a hand-written fence/barrier. This is the layer "the blind
// implementation never let us see".
//
// FIRST-CUT SCOPE (deliberate, per S1 + MR-2/MR-3):
//   * SINGLE universal queue only. Intra-queue ordering. NO async-compute, NO cross-queue
//     ownership transfer, NO timeline semaphores (those are MR-2 / a later phase, S7).
//   * The barrier DERIVATION is pure C++ over declared passes - it needs no VkDevice. It uses
//     the real synchronization2 (Vulkan 1.3) enums/structs (VkImageMemoryBarrier2,
//     VkPipelineStageFlags2, VkAccessFlags2, VkImageLayout). VkImage handles are opaque to the
//     derivation (filled only at execute()); the unit test mocks them.
//   * compile() + dump() are the COMPILE side (testable without a GPU). execute() is a thin
//     method that records the derived barriers into a real VkCommandBuffer (NOT exercised by
//     the GPU-free test).
//
// CORRECTNESS CORE - barrier derivation (get this RIGHT, MR-3):
//   Track, per resource, the LAST write (stage/access/layout) + the read scope already made
//   visible since that write + the reads since that write (for WAR). Walking passes in
//   submission (execution) order:
//     - RAW (write-then-read): a pass READS a resource last WRITTEN by an earlier pass ->
//       ONE VkImageMemoryBarrier2 carrying BOTH scopes (execution AND memory): src = writer
//       stage/access (flush), dst = reader stage/access (invalidate), oldLayout = writer
//       layout, newLayout = reader layout. THE load-bearing case.
//     - WAW (write-after-write): src = prior-writer stage/access (flush), dst = new-writer.
//     - WAR (write-after-read): EXECUTION dependency - src = the prior readers' stage(s),
//       srcAccess = 0 (reads do not dirty memory; no flush needed), dst = new-writer
//       stage/access. (A layout transition, if any, still rides this barrier.)
//     - IMPORTED resources start "externally produced" (last-written by an external stage at
//       kImportLayout); the first internal reader gets a correct ACQUIRE barrier (RAW-class,
//       producer = EXTERNAL, with the layout transition from the import layout).
//   PRECISE masks ONLY - never VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT (the cited 13%-waste
//   anti-pattern). Each barrier carries exactly the producer/consumer stage+access declared.
//   compile() is DETERMINISTIC: same graph -> byte-identical Compiled + dump() (the MR-3
//   golden-determinism gate at graph level - no pointers/addresses leak into dump()).
//
// HONEST FIRST-CUT LIMITATIONS (marked, not hidden):
//   * NO init/undefined->first-write layout transition is emitted: a transient internal image
//     is assumed CREATED in the layout of its first write (i.e. the allocator places it in the
//     producer's layout). Only hazards against a PRIOR access (RAW/WAW/WAR/acquire) emit a
//     barrier. This is what makes "+1 pass == +1 barrier" hold (see the layerability test).
//   * The pass order is the DECLARED (add) order, filtered by culling, then ASSERTED to be a
//     valid topological order (every read's producer precedes it). A general DAG reorder is a
//     later refinement (S1.3 blesses the linear first cut). A read-before-write is recorded in
//     Compiled::errors, loudly, rather than silently mis-synchronised.
//   * A layout transition BETWEEN two reads (two readers of one resource in different layouts)
//     is NOT special-cased to wait on the prior reader - the minimal core never does this (all
//     reads share one layout). Documented so a future layer does not assume it is handled.
//   * imported images are assumed to arrive in kImportLayout (a WGC/DXGI shared texture handed
//     to us as a sampled image). declare_image() takes no per-image layout (the API signature
//     is fixed by the spec); a per-image import layout is a later refinement.
//
// PILLAR FAITHFULNESS: app-local for the first cut (master-plan sec 8) - a POSSIBLE future
// Feature Request into a pillar only if it proves general. It WRAPS, does not replace, the
// existing Vulkan primitives.

#pragma once

#include <vulkan/vulkan.h>

#if !defined(VK_VERSION_1_3)
#  error "minimal_fg/seam_graph.hpp requires Vulkan 1.3 (synchronization2 core: VkImageMemoryBarrier2 / VkDependencyInfo / vkCmdPipelineBarrier2)."
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pfg::seam {

// Opaque resource handle (an index minted by the graph).
using ResId = uint32_t;

// A declared access to a resource by a pass: the stage it is touched in, the access type,
// and the image layout it must be in for that access.
struct Access {
    ResId                 res;
    VkPipelineStageFlags2 stage;
    VkAccessFlags2        access;
    VkImageLayout         layout;
    // G4 (R2): an OPTIONAL write — a store the pass can be told to skip. compile() reports the optional
    // writes that NOTHING LIVE reads (Compiled::dead_optional_writes) so the caller can compile the store
    // out (a specialization constant for a fused row, or simply not recording it). Default false =
    // every existing declaration behaves exactly as before.
    bool                  optional = false;
};

// The hazard class a derived barrier resolves.
enum class Hazard { RAW, WAW, WAR };

// A derived barrier at a pass boundary (emitted immediately BEFORE its consumer pass records).
struct Barrier {
    ResId                 res           = 0;
    int                   producer_pass = -1;   // exec index of the pass we wait on; -1 = EXTERNAL (imported)
    int                   consumer_pass = -1;   // exec index of the pass this barrier guards
    Hazard                hazard        = Hazard::RAW;
    VkImageMemoryBarrier2 vk{};                 // the fully-derived barrier (image filled at execute())
};

// One pass in the compiled (culled + ordered) graph, with the barriers that precede it.
// G3 (R2): a compile-time WARNING — not an error. `dominance` fires when a pass overwrites a resource
// it does not read: legal, sometimes intended (a full-frame clear), and exactly the shape of the defect
// the layer contract exists to prevent, so it is NAMED rather than silently derived. A pass silences it
// by declaring why (`dominates_ok`).
struct Warning {
    enum class Kind { Dominance } kind = Kind::Dominance;
    ResId       res = 0;
    int         producer_pass = -1, consumer_pass = -1;
    std::string text;
};

struct CompiledPass {
    std::string          name;
    int                  original_index = -1;   // index into the declared pass list (for execute()'s record fn)
    std::vector<Barrier> barriers;              // emitted BEFORE this pass records
    // G5 (R2): the Vulkan-ready barrier array, built HERE at compile() time. execute() patches only the
    // VkImage handles into it and submits — no per-frame allocation on the recording thread.
    std::vector<VkImageMemoryBarrier2> vk_barriers;
};

// The compiled graph: passes in execution order, each with its derived precede-barriers.
struct Compiled {
    std::vector<CompiledPass> passes;
    std::vector<std::string>  errors;           // non-empty => a declaration hazard (read-before-write, bad id)
    std::vector<Warning>      warnings;         // G3: compile-time notices (dominance). NEVER printed by dump().
    std::vector<std::string>  dead_optional_writes;   // G4: "pass:resource" — an optional write nothing live reads.
    uint64_t                  graph_id = 0;     // G5/XR11: the SeamGraph that produced this Compiled.

    size_t barrier_count() const {
        size_t n = 0;
        for (const CompiledPass& p : passes) n += p.barriers.size();
        return n;
    }
};

namespace detail {

// Deterministic stringifiers (used only by dump(); NEVER print pointers/addresses).
inline std::string stage_str(VkPipelineStageFlags2 s) {
    if (s == VK_PIPELINE_STAGE_2_NONE) return "NONE";
    struct E { VkPipelineStageFlags2 bit; const char* n; };
    static const E tab[] = {
        { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,            "TOP_OF_PIPE" },
        { VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,          "DRAW_INDIRECT" },
        { VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,          "VERTEX_SHADER" },
        { VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,        "FRAGMENT_SHADER" },
        { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,   "EARLY_FRAGMENT_TESTS" },
        { VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,    "LATE_FRAGMENT_TESTS" },
        { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,"COLOR_ATTACHMENT_OUTPUT" },
        { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,         "COMPUTE_SHADER" },
        { VK_PIPELINE_STAGE_2_COPY_BIT,                   "COPY" },
        { VK_PIPELINE_STAGE_2_BLIT_BIT,                   "BLIT" },
        { VK_PIPELINE_STAGE_2_RESOLVE_BIT,                "RESOLVE" },
        { VK_PIPELINE_STAGE_2_CLEAR_BIT,                  "CLEAR" },
        { VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,           "ALL_TRANSFER" },
        { VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,           "ALL_GRAPHICS" },
        { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,           "ALL_COMMANDS" },
        { VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,         "BOTTOM_OF_PIPE" },
    };
    std::string out;
    VkPipelineStageFlags2 rem = s;
    for (const E& e : tab) {
        if ((s & e.bit) == e.bit) {
            if (!out.empty()) out += "|";
            out += e.n;
            rem &= ~e.bit;
        }
    }
    if (rem) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "0x%llx", static_cast<unsigned long long>(rem));
        if (!out.empty()) out += "|";
        out += buf;
    }
    return out.empty() ? std::string("NONE") : out;
}

inline std::string access_str(VkAccessFlags2 a) {
    if (a == VK_ACCESS_2_NONE) return "NONE";
    struct E { VkAccessFlags2 bit; const char* n; };
    static const E tab[] = {
        { VK_ACCESS_2_SHADER_READ_BIT,            "SHADER_READ" },
        { VK_ACCESS_2_SHADER_WRITE_BIT,           "SHADER_WRITE" },
        { VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,    "SHADER_SAMPLED_READ" },
        { VK_ACCESS_2_SHADER_STORAGE_READ_BIT,    "SHADER_STORAGE_READ" },
        { VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,   "SHADER_STORAGE_WRITE" },
        { VK_ACCESS_2_UNIFORM_READ_BIT,           "UNIFORM_READ" },
        { VK_ACCESS_2_TRANSFER_READ_BIT,          "TRANSFER_READ" },
        { VK_ACCESS_2_TRANSFER_WRITE_BIT,         "TRANSFER_WRITE" },
        { VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,  "COLOR_ATTACHMENT_READ" },
        { VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, "COLOR_ATTACHMENT_WRITE" },
        { VK_ACCESS_2_MEMORY_READ_BIT,            "MEMORY_READ" },
        { VK_ACCESS_2_MEMORY_WRITE_BIT,           "MEMORY_WRITE" },
    };
    std::string out;
    VkAccessFlags2 rem = a;
    for (const E& e : tab) {
        if ((a & e.bit) == e.bit) {
            if (!out.empty()) out += "|";
            out += e.n;
            rem &= ~e.bit;
        }
    }
    if (rem) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "0x%llx", static_cast<unsigned long long>(rem));
        if (!out.empty()) out += "|";
        out += buf;
    }
    return out.empty() ? std::string("NONE") : out;
}

inline const char* layout_str(VkImageLayout l) {
    switch (l) {
        case VK_IMAGE_LAYOUT_UNDEFINED:                return "UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL:                  return "GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:     return "TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:     return "TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:          return "PRESENT_SRC_KHR";
        case VK_IMAGE_LAYOUT_PREINITIALIZED:           return "PREINITIALIZED";
        default:                                       return "LAYOUT_OTHER";
    }
}

inline const char* hazard_str(Hazard h) {
    switch (h) {
        case Hazard::RAW: return "RAW";
        case Hazard::WAW: return "WAW";
        case Hazard::WAR: return "WAR";
    }
    return "?";
}

}  // namespace detail

// The seam graph. Declare resources + passes, mark outputs, compile() -> derived barriers.
class SeamGraph {
public:
    // imported images are assumed to arrive externally produced in this layout (see header note).
    static constexpr VkImageLayout kImportLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Register an image resource. imported = an external producer wrote it (e.g. a WGC capture).
    ResId declare_image(const char* name, bool imported = false) {
        const ResId id = static_cast<ResId>(res_.size());
        res_.push_back(ResDecl{ name ? name : "", imported, VK_NULL_HANDLE });
        return id;
    }

    // Register a pass with the resources it READS and WRITES (each carries its stage/access/layout)
    // and an optional command-recording callback (only used by execute()).
    void add_pass(const char* name,
                  std::vector<Access> reads,
                  std::vector<Access> writes,
                  std::function<void(VkCommandBuffer)> record = {}) {
        passes_.push_back(PassDecl{ name ? name : "",
                                    std::move(reads), std::move(writes),
                                    std::move(record), std::string() });
    }

    // G3: the same, declaring WHY this pass may overwrite what it does not read. A non-empty reason
    // silences the dominance warning for this pass and is printed by dump_warnings().
    void add_pass_dominating(const char* name,
                             std::vector<Access> reads,
                             std::vector<Access> writes,
                             const char* dominates_ok,
                             std::function<void(VkCommandBuffer)> record = {}) {
        passes_.push_back(PassDecl{ name ? name : "",
                                    std::move(reads), std::move(writes),
                                    std::move(record), dominates_ok ? dominates_ok : "" });
    }

    // Mark a resource as a graph output (a present source / root). Culling = backward
    // reachability from the set of outputs.
    void mark_output(ResId r) { outputs_.push_back(r); }

    // Optionally bind a (real or mock) VkImage to a resource - consumed by execute() only.
    void bind_image(ResId r, VkImage img) {
        if (r < res_.size()) res_[r].image = img;
    }

    // Compile: cull unreachable passes -> order -> derive precise barriers. Deterministic.
    Compiled compile() const {
        Compiled out;
        const size_t NP = passes_.size();

        // ---- 1. Cull: backward reachability from marked outputs ---------------------------
        std::vector<char> is_output(res_.size(), 0);
        bool any_output = false;
        for (ResId r : outputs_) {
            if (r < res_.size()) { is_output[r] = 1; any_output = true; }
        }

        std::vector<char> live(NP, 0);
        if (!any_output) {
            // No root declared => cannot cull; keep everything (and record a note).
            std::fill(live.begin(), live.end(), 1);
            out.errors.push_back("no output marked: culling disabled (all passes kept)");
        } else {
            // Seed: a pass that writes a marked-output resource is live.
            for (size_t p = 0; p < NP; ++p) {
                for (const Access& w : passes_[p].writes) {
                    if (w.res < res_.size() && is_output[w.res]) { live[p] = 1; break; }
                }
            }
            // Propagate: a producer of any resource read by a live pass is live.
            bool changed = true;
            while (changed) {
                changed = false;
                for (size_t p = 0; p < NP; ++p) {
                    if (!live[p]) continue;
                    for (const Access& rd : passes_[p].reads) {
                        for (size_t q = 0; q < NP; ++q) {
                            if (live[q]) continue;
                            for (const Access& w : passes_[q].writes) {
                                if (w.res == rd.res) { live[q] = 1; changed = true; break; }
                            }
                        }
                    }
                }
            }
        }

        // Execution order = declared order filtered to live passes (the linear first cut, S1.3).
        std::vector<int> exec;
        exec.reserve(NP);
        for (size_t p = 0; p < NP; ++p) if (live[p]) exec.push_back(static_cast<int>(p));

        // ---- 2. Per-resource hazard-tracking state ---------------------------------------
        // A scope the producer's write was actually MADE VISIBLE to (one emitted RAW barrier's dst).
        struct VisScope {
            VkPipelineStageFlags2 stage;
            VkAccessFlags2        access;
            VkImageLayout         layout;
        };
        struct RState {
            bool                  has_producer = false;  // written before, or imported
            int                   prod_pass    = -1;     // exec index of last writer; -1 = EXTERNAL
            VkPipelineStageFlags2 prod_stage   = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        prod_access  = VK_ACCESS_2_NONE;
            VkImageLayout         layout       = VK_IMAGE_LAYOUT_UNDEFINED;
            // FIX (MR-3, adversarial finding): visibility is a SET of (stage,access,layout) scopes the
            // producer's write was actually made visible to. A read is covered ONLY if its (stage,access)
            // is a subset of a SINGLE such scope at the same layout. Two DECOUPLED stage/access unions
            // would skip a barrier for a cross-pair scope never actually made visible (a silent RAW).
            std::vector<VisScope> vis;                    // scopes made visible since last write
            VkPipelineStageFlags2 rsw_stage    = 0;       // union of read stages since last write (WAR)
            int                   last_read_pass = -1;    // exec index of latest reader since last write
            bool                  read_since_write = false; // any reader since the last write (layout-after-read guard)
        };
        std::vector<RState> st(res_.size());
        for (size_t r = 0; r < res_.size(); ++r) {
            if (res_[r].imported) {
                st[r].has_producer = true;
                st[r].prod_pass    = -1;                  // EXTERNAL
                st[r].prod_stage   = VK_PIPELINE_STAGE_2_NONE;
                st[r].prod_access  = VK_ACCESS_2_NONE;
                st[r].layout       = kImportLayout;
            }
        }

        out.passes.reserve(exec.size());

        for (size_t e = 0; e < exec.size(); ++e) {
            const PassDecl& P = passes_[exec[e]];
            CompiledPass cp;
            cp.name           = P.name;
            cp.original_index = exec[e];

            // ---- READS -> RAW (incl. the imported acquire). Decide before committing state. --
            for (const Access& rd : P.reads) {
                if (rd.res >= res_.size()) {
                    out.errors.push_back("pass '" + P.name + "' reads invalid resource id");
                    continue;
                }
                const RState& s = st[rd.res];
                if (!s.has_producer) {
                    out.errors.push_back("read-before-write: pass '" + P.name + "' reads uninitialised '"
                                         + res_[rd.res].name + "'");
                    continue;
                }
                const bool layout_change = (s.layout != rd.layout);
                // Covered iff a SINGLE prior visibility scope (same layout) already includes BOTH this
                // read's stage AND its access. NOT two decoupled unions (that misses a cross-pair the
                // producer was never made visible to -> a silent RAW). This is the critical-finding fix.
                bool covered = false;
                for (const VisScope& v : s.vis) {
                    if (v.layout == rd.layout
                        && (rd.stage  & ~v.stage)  == 0
                        && (rd.access & ~v.access) == 0) { covered = true; break; }
                }
                if (!covered) {
                    VkPipelineStageFlags2 src_stage  = s.prod_stage;
                    VkAccessFlags2        src_access = s.prod_access;
                    if (layout_change && s.read_since_write) {
                        // First-cut SG does NOT fully support a layout transition AFTER a prior reader
                        // (the transition must also wait on that reader, not just the producer). Flag it
                        // LOUD (never a silent wrong barrier) + emit a best-effort barrier that also
                        // orders the prior readers. The minimal core never hits this (all reads one layout).
                        out.errors.push_back("layout-transition-after-read unsupported (first-cut SG): pass '"
                            + P.name + "' reads '" + res_[rd.res].name + "' in a new layout after a prior reader");
                        src_stage |= s.rsw_stage;
                    }
                    cp.barriers.push_back(make_barrier(
                        rd.res, Hazard::RAW, s.prod_pass, static_cast<int>(e),
                        src_stage, src_access, rd.stage, rd.access,
                        s.layout, rd.layout));
                }
                // else: read already covered by an earlier reader's barrier (same scope, same
                // layout) -> NO new barrier. THIS is what keeps "+1 pass == +1 barrier".
            }

            // ---- WRITES -> WAW / WAR. Decided against PRE-pass state (no intra-pass RMW self-hazard). --
            for (const Access& wr : P.writes) {
                if (wr.res >= res_.size()) {
                    out.errors.push_back("pass '" + P.name + "' writes invalid resource id");
                    continue;
                }
                const RState& s = st[wr.res];
                // RMW: if THIS pass also READS the resource, its read already emitted a RAW that orders
                // this pass after the producer -> a WAW against that same producer is redundant (the
                // over-sync finding). Skip it. (Prior readers from OTHER passes still need the WAR below.)
                bool read_here = false;
                for (const Access& rd : P.reads) { if (rd.res == wr.res) { read_here = true; break; } }
                if (s.rsw_stage != 0) {
                    // WAR: the write must wait for prior reads (from OTHER passes; this pass's own read is
                    // not yet committed). Execution dependency only - srcAccess = 0 (reads do not dirty).
                    cp.barriers.push_back(make_barrier(
                        wr.res, Hazard::WAR, s.last_read_pass, static_cast<int>(e),
                        s.rsw_stage, /*srcAccess*/ 0, wr.stage, wr.access,
                        s.layout, wr.layout));
                } else if (s.has_producer && !read_here) {
                    // G3 (R2): this pass overwrites a resource it does not read — the dominance the layer
                    // contract exists to name. Legal, but silent today; warn unless the pass declared why.
                    if (P.dominates_ok.empty()) {
                        Warning w; w.kind = Warning::Kind::Dominance; w.res = wr.res;
                        w.producer_pass = s.prod_pass; w.consumer_pass = static_cast<int>(e);
                        w.text = "pass '" + P.name + "' overwrites resource '" +
                                 (wr.res < res_.size() ? res_[wr.res].name : std::string("?")) +
                                 "' without reading it (declare add_pass_dominating(..., reason) if intended)";
                        out.warnings.push_back(std::move(w));
                    }
                    // WAW: flush the prior write before the new write (producer may be EXTERNAL).
                    cp.barriers.push_back(make_barrier(
                        wr.res, Hazard::WAW, s.prod_pass, static_cast<int>(e),
                        s.prod_stage, s.prod_access, wr.stage, wr.access,
                        s.layout, wr.layout));
                }
                // else: first write of a fresh internal image -> no prior access -> no barrier
                // (assumed created in its first-write layout; see header limitation note).
            }

            // ---- Commit this pass's state changes (reads then writes; RMW ends 'written'). ---
            for (const Access& rd : P.reads) {
                if (rd.res >= res_.size()) continue;
                RState& s = st[rd.res];
                if (!s.has_producer) continue;            // error already recorded
                s.vis.push_back(VisScope{ rd.stage, rd.access, rd.layout });  // this scope now made visible
                s.rsw_stage     |= rd.stage;
                s.last_read_pass = static_cast<int>(e);
                s.read_since_write = true;
                s.layout         = rd.layout;             // image now in the reader's layout
            }
            for (const Access& wr : P.writes) {
                if (wr.res >= res_.size()) continue;
                RState& s = st[wr.res];
                s.has_producer   = true;
                s.prod_pass      = static_cast<int>(e);
                s.prod_stage     = wr.stage;
                s.prod_access    = wr.access;
                s.layout         = wr.layout;
                s.vis.clear();
                s.rsw_stage      = 0;
                s.last_read_pass = -1;
                s.read_since_write = false;
            }

            out.passes.push_back(std::move(cp));
        }

        // ── G5 (R2): build the Vulkan barrier arrays ONCE, here. execute() then patches only the
        //    VkImage handles — no allocation on the recording thread. ──────────────────────────────
        for (CompiledPass& cp : out.passes) {
            cp.vk_barriers.reserve(cp.barriers.size());
            for (const Barrier& b : cp.barriers) cp.vk_barriers.push_back(b.vk);
        }
        // ── G4 (R2): an OPTIONAL write that nothing LIVE reads is dead — report it so the caller can
        //    compile the store out. (A write read by a later live pass, or marked as an output, is live.)
        for (const CompiledPass& cp : out.passes) {
            if (cp.original_index < 0 || (size_t)cp.original_index >= passes_.size()) continue;
            const PassDecl& PD = passes_[cp.original_index];
            for (const Access& wr : PD.writes) {
                if (!wr.optional) continue;
                bool live = false;
                for (ResId r : outputs_) if (r == wr.res) { live = true; break; }
                if (!live) {
                    for (const CompiledPass& later : out.passes) {
                        if (later.original_index <= cp.original_index) continue;
                        const PassDecl& LD = passes_[later.original_index];
                        for (const Access& rd : LD.reads) if (rd.res == wr.res) { live = true; break; }
                        if (live) break;
                    }
                }
                if (!live)
                    out.dead_optional_writes.push_back(
                        PD.name + ":" + (wr.res < res_.size() ? res_[wr.res].name : std::string("?")));
            }
        }
        out.graph_id = graph_id_;
        return out;
    }

    // G3: the warnings, as a deterministic listing. SEPARATE from dump() on purpose — dump() is the
    // golden-test artifact and must stay byte-identical.
    std::string dump_warnings(const Compiled& c) const {
        std::ostringstream os;
        os << "SG-WARN: " << c.warnings.size() << " warning(s), "
           << c.dead_optional_writes.size() << " dead optional write(s)\n";
        for (const Warning& w : c.warnings) os << "  dominance: " << w.text << "\n";
        for (const std::string& d : c.dead_optional_writes) os << "  dead-optional-write: " << d << "\n";
        return os.str();
    }

    // Record the derived barriers (vkCmdPipelineBarrier2) + invoke each pass's record fn, in
    // execution order. NOT exercised by the GPU-free unit test; image handles come from bind_image().
    // G5 (R2): ZERO-ALLOCATION execute. The barrier array was built at compile(); here we patch the
    // VkImage handles (bind_image() may have changed them since) and submit. Takes Compiled& by
    // NON-const reference because the patch is in place — that is the whole point.
    // XR11: the handles are re-read from res_ on EVERY call (never cached across calls), and the
    // Compiled must have come from THIS graph (asserted; a mismatched pair would barrier wrong images).
    void execute(VkCommandBuffer cmd, Compiled& c) const {
        if (c.graph_id != graph_id_) {   // wrong graph: refuse rather than barrier the wrong images
            std::fprintf(stderr, "SG: execute() got a Compiled from another graph (%llu vs %llu)\n",
                         (unsigned long long)c.graph_id, (unsigned long long)graph_id_);
            return;
        }
        for (CompiledPass& p : c.passes) {
            if (!p.vk_barriers.empty()) {
                for (size_t i = 0; i < p.vk_barriers.size(); ++i) {
                    const ResId r = p.barriers[i].res;
                    p.vk_barriers[i].image = (r < res_.size()) ? res_[r].image : VK_NULL_HANDLE;
                }
                VkDependencyInfo dep{};
                dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dep.imageMemoryBarrierCount  = static_cast<uint32_t>(p.vk_barriers.size());
                dep.pImageMemoryBarriers     = p.vk_barriers.data();
                vkCmdPipelineBarrier2(cmd, &dep);
            }
            if (p.original_index >= 0 && static_cast<size_t>(p.original_index) < passes_.size()) {
                const std::function<void(VkCommandBuffer)>& rec = passes_[p.original_index].record;
                if (rec) rec(cmd);
            }
        }
    }

    // A deterministic, human-readable barrier+pass listing (the golden-test + hand-audit artifact).
    std::string dump(const Compiled& c) const {
        std::ostringstream os;
        os << "SG: " << c.passes.size() << " passes, " << c.barrier_count() << " barriers\n";
        for (size_t i = 0; i < c.passes.size(); ++i) {
            const CompiledPass& p = c.passes[i];
            os << "pass " << i << " \"" << p.name << "\"\n";
            for (const Barrier& b : p.barriers) {
                const std::string prod = (b.producer_pass < 0)
                                       ? std::string("EXTERNAL")
                                       : c.passes[static_cast<size_t>(b.producer_pass)].name;
                const std::string cons = (b.consumer_pass >= 0 &&
                                          static_cast<size_t>(b.consumer_pass) < c.passes.size())
                                       ? c.passes[static_cast<size_t>(b.consumer_pass)].name
                                       : std::string("?");
                const std::string rname = (b.res < res_.size()) ? res_[b.res].name : std::string("?");
                os << "  " << detail::hazard_str(b.hazard)
                   << " res=" << b.res << ":\"" << rname << "\" "
                   << prod << "->" << cons
                   << " src=" << detail::stage_str(b.vk.srcStageMask)
                   << ":"     << detail::access_str(b.vk.srcAccessMask)
                   << " dst=" << detail::stage_str(b.vk.dstStageMask)
                   << ":"     << detail::access_str(b.vk.dstAccessMask)
                   << " layout=" << detail::layout_str(b.vk.oldLayout)
                   << "->"       << detail::layout_str(b.vk.newLayout)
                   << "\n";
            }
        }
        return os.str();
    }

private:
    struct ResDecl  { std::string name; bool imported; VkImage image; };
    struct PassDecl {
        std::string                          name;
        std::vector<Access>                  reads;
        std::vector<Access>                  writes;
        std::function<void(VkCommandBuffer)> record;
        std::string                          dominates_ok;   // G3: the declared reason a dominance is intended
    };

    static Barrier make_barrier(ResId res, Hazard hz, int prod, int cons,
                                VkPipelineStageFlags2 ss, VkAccessFlags2 sa,
                                VkPipelineStageFlags2 ds, VkAccessFlags2 da,
                                VkImageLayout oldL, VkImageLayout newL) {
        Barrier b{};
        b.res           = res;
        b.hazard        = hz;
        b.producer_pass = prod;
        b.consumer_pass = cons;
        VkImageMemoryBarrier2& v = b.vk;
        v.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        v.pNext               = nullptr;
        v.srcStageMask        = ss;
        v.srcAccessMask       = sa;
        v.dstStageMask        = ds;
        v.dstAccessMask       = da;
        v.oldLayout           = oldL;
        v.newLayout           = newL;
        v.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        v.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        v.image               = VK_NULL_HANDLE;  // filled from bind_image() at execute()
        v.subresourceRange    = VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
        return b;
    }

    std::vector<ResDecl>  res_;
    std::vector<PassDecl> passes_;
    std::vector<ResId>    outputs_;
    // G5/XR11: a per-instance id stamped into every Compiled, so execute() can refuse a Compiled that
    // came from a different graph (it would patch the wrong images into the right barriers).
    uint64_t              graph_id_ = next_graph_id();
    static uint64_t next_graph_id() { static uint64_t n = 0; return ++n; }
};

}  // namespace pfg::seam

// Made with my soul - Swately <3
