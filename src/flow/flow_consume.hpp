#pragma once
// PhyriadFG — src/flow/flow_consume.hpp : the FLOW stage's per-pair CPU tail and its F→P publish (R5 step 3c).
//
// `consume_wap` was a 444-line lambda inside run_flow (flow.cpp:964–1407 pre-extraction). It is the ORCHESTRATOR of
// stage 3: it waits the forward fence, updates the inertia persistence field, decodes the pressure tier (the
// governor floor included), evaluates the FLOW rows' arms and runs the passes they arm — the gme fit (CPU or the
// device-B variant), the scene-holon (advect/merge/refresh), the object-holon (both anchors), the backward match's
// consume — stamps the pair's scalars into the FlowRing and publishes with `f_seq.fetch_add`. The leaves it
// orchestrates left in step 3a (flow/holons.hpp); this is the tail that sequences them.
//
// The body is MOVED VERBATIM (records/R5_GATE.md §3c has the measure). What the lambda captured is rebound at the
// top of the function: the FgContext-derived names by the same `auto& X = ctx.X;` lines run_flow uses, the
// F-thread's per-pair state through `ConsumeState` (references — run_flow remains the owner, CR1), and the five
// run_flow lambdas it calls through std::function members typed to their signatures, so every call site inside the
// body — the inline lambdas passed to objdump_grid included — is textually unchanged.
//
// `FwdPend` moved here with it: it is the deferred-consume snapshot (--fwd-pipeline) the two call sites pass, and
// the reason it exists is that the deferred consume must NOT read run_flow's live loop locals (they have advanced by
// then) — the torn-read discipline of the pipelined path.
// Made with my soul - Swately <3
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>
#include "flow/holons.hpp"

struct FgContext;

namespace pfg::flow {

// The deferred-consume snapshot (was run_flow-local): what the pipelined path captured at record time.
struct FwdPend{ bool valid=false; int f_gen=0; uint64_t cur_c=0; int s=0; uint64_t span=1; int N_use=1;
                int prv_f=0; int cur_f=0; bool have_prev_f=false; double tp0=0.0; double tf0=0.0; VkFence fwd_fence=VK_NULL_HANDLE; };

// The F thread's per-pair CPU-tail state. Every field is a REFERENCE to a run_flow local: the owner does not move,
// only the code that reads it (CR1 reference-only binding, the R4/R5 pattern).
struct ConsumeState {
    pfg::flow::HolonScratch& hs;
    std::vector<uint8_t>& persist;
    double& gme_fit_ema;
    uint64_t& gme_fits;
    bool& gme_sub2;
    bool& gme_fit_printed;
    std::vector<uint8_t>& gme_vfy_dis;
    uint64_t& gme_vfy_n;
    double& obj_cost_ema;
    uint64_t& obj_pairs;
    bool& obj_settle_printed;
    double& t_pair_ema;
    bool& bwd_skipping;
    double& t_flow_ema;
    double& t_fuse_ema;
    double& t_warp_ema;
    double& span_ema;
    int& pressure_tier;
    uint64_t& holon_pair_ctr;
    uint64_t& tier4_dwell;
    int& live_n_f;
    int& mv_audit_left;
    int& up_streak;
    int& deg_streak;
    int& dwell_sets;
    const uint64_t& kTier4DwellPairs;
    int& objdump_left;
    uint64_t& objdump_idx;
    // the run_flow lambdas the tail calls (Vulkan submits, the flow-input downsample, the --mv-audit tap, the
    // --objdump grid writer). Typed to their signatures so the body's call sites are unchanged.
    std::function<void(VkCommandBuffer,VkFence)> flow_submit_nowait;
    std::function<void(VkCommandBuffer,VkSemaphore,VkPipelineStageFlags,VkSemaphore,VkFence)> flow_submit_q2_chain;
    std::function<void(VkCommandBuffer,int,int,VkImageView&,VkImageView&)> flow_downsample;
    std::function<void(const void*,const uint8_t*,float,double*,float*,uint32_t*)> mv_audit_stat;
    std::function<void(const char*,const std::function<uint8_t(size_t)>&)> objdump_grid;
};

// The tail itself. `pc` is the pair's snapshot; `allow_bwd` is false on the deferred (pipelined) path — the backward
// match shares the single `ofp` and the two `Bframe` slots with the forward one.
void consume_wap(FgContext& ctx, ConsumeState& S, const FwdPend& pc, bool allow_bwd);

}  // namespace pfg::flow
