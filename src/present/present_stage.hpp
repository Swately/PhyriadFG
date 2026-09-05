#pragma once
// PhyriadFG — src/present/present_stage.hpp : STAGE 6 — PRESENT (R4 of CONVERGENCE_MASTER_PLAN.md, strategy X12).
//
// The stage-6 module of docs/planning/STAGE_CONTRACT.md: "put GenFrame (or a real frame) on the panel at the
// target time; DROP when a warp is still in flight; never block the generation fence; report FlipStats".
// It OWNS the PresentSurface (created ON the present thread — the pillar's threading contract), the submit
// accounting, the --pace-hard pin state, the two bridge slots of --async-present with their front / in-flight
// indices, and the --shallow-queue counters. Everything else it touches is bound by REFERENCE at construction
// (CR1, reference-only binding): the device, the Config, the bridge extent + keyed-mutex flag, the slot-0 NT
// handle, the host mass pointer, the present counter.
//
// The tick DECISION (STAGE_CONTRACT §1 Phase.decision) is an INPUT of begin(): the loop hands in Warp, or Dup on
// an exact-duplicate --fdrop tick (the completed front is re-shown); the stage returns Drop when a warp is still
// in flight under --async-present (the legacy "async re-present drop", counted in rdrop_ticks). Decimated is
// the absence of a call: the loop's decimation gate `continue`s before any stage runs.
//
// Bodies are the legacy present.cpp lines MOVED (records/R4_GATE.md §2 lists every transform, measured by
// scratchpad/verbatim_r4.py); the loop keeps its former local names as ALIASES to the stage's fields, so the
// ~40 read sites (the stats, the CSV row, --rfp-fresh, --motion-fallback) are untouched.
//
// MUST NOT change (the register): the drop decision is a P-local scalar compare (no lock); the slots are
// provisioned at init (present_init.cpp) and only BOUND here; every fence poll is vk_live-wrapped (MR-1);
// own-window PresentSurface only (MR-7); one submitter per queue (MR-2).
// Made with my soul - Swately <3
#include "core/device.hpp"
#include "core/vk_util.hpp"
#include <phyriad/render/present/PresentSurface.hpp>
#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdint>
#include <expected>
#include <vector>

struct Config;
struct ID3D11Texture2D;
struct IDXGIKeyedMutex;

namespace pfg::present {
namespace pp = phyriad::render::present;

// The tick decision (STAGE_CONTRACT §1): what stage 6 does with this vblank slot.
enum class Decision : uint8_t { Warp, Dup, Drop, Decimated };

// One bridge slot: the D3D11 shared texture + its NT handle + keyed mutex (present_init.cpp), the imported
// VK image + memory, and the command buffer + fence the warp records into. Two under --async-present.
struct BridgeSlot { ID3D11Texture2D* tex; void* nt; IDXGIKeyedMutex* km; VkImage img; VkDeviceMemory mem; VkCommandBuffer cmd; VkFence fence; };

// The feedback edge 6 → 4 (STAGE_CONTRACT §0): the own-window flip stats + the submit accounting.
struct FlipStats {
    bool     have_flip = false;                         // last_flip_qpc() had a sample
    uint64_t sync_qpc = 0; uint32_t present_count = 0;  // pp::PresentSurface::FlipStats, forwarded (the pillar's own types)
    uint64_t ps_ok = 0, ps_timeout = 0, ps_err = 0;     // submits so far
    bool     device_lost = false;
};

// What begin() decides for a tick: the slot to record into, whether to record at all, the decision taken.
struct Tick {
    bool ap = false; int back = 0; bool record = false; Decision decision = Decision::Warp;
    VkCommandBuffer cmd = VK_NULL_HANDLE; VkFence fence = VK_NULL_HANDLE; VkImage img = VK_NULL_HANDLE; VkDeviceMemory mem = VK_NULL_HANDLE;
};

class PresentStage {
public:
    PresentStage(VDev& A, const Config& cfg, uint32_t& bridge_w, uint32_t& bridge_h, bool& bridge_use_km,
                 void*& bridge_nt, void*& hostMassPtr, std::atomic<uint64_t>& total_frames, std::atomic<bool>& g_quit_threads);
    ~PresentStage();
    PresentStage(const PresentStage&) = delete; PresentStage& operator=(const PresentStage&) = delete;

    // ── owned state (public: the loop's aliases read them; the per-second stats print their deltas) ──
    pp::PresentSurface surface; bool surface_ready = false;
    uint64_t ps_ok = 0, ps_timeout = 0, ps_err = 0;     // submit ok / timeout / err
    uint64_t rdrop_ticks = 0;                           // async re-present drops (a warp in flight → the stale front re-shown)
    double   ph_tgt = 0.0, ph_w_ema = 0.0;              // --pace-hard: this tick's grid target (set by the loop), EMA(work)
    uint64_t ph_held = 0, ph_overshoot = 0;             // --pace-hard: pinned / overshot presents
    BridgeSlot bslot[2]{};                              // BOUND by the loop from present_init.cpp's objects
    int async_front = -1, async_inflight = -1;          // the last COMPLETED slot (presentable) / the slot with a warp in flight
    uint64_t sq_hits = 0, sq_misses = 0;                // --shallow-queue early-promote hits / misses

    // ── the stage's operations (each = a legacy present.cpp body, cited in present_stage.cpp) ──
    bool init(void* wgc_target_hwnd);                                   // the surface, created on this thread; false = quit
    void account(const std::expected<void, phyriad::Error>& r);         // the submit result accounting (ps_account)
    void present_front();                                              // the --pace-hard pin + submit slot 0 (bridge_present)
    void poll_inflight(uint32_t* presented_out);                        // the async preamble: promote a completed in-flight slot to the front
    Tick begin(Decision d, bool count_drop);                            // the slot decision for this tick; Drop returned when a warp is in flight
    void submit(const Tick& tk, VkSubmitInfo& si);                      // sync: submit + TDR-catching wait; async: submit + mark in flight
    void submit_sync(VkFence fence, VkSubmitInfo& si);                  // the grid path's shared cmd/fence submit (+ wait)
    void shallow_queue(const Tick& tk, uint32_t* presented_out);        // --shallow-queue: bounded early promote of this tick's warp
    void present_tick(const Tick& tk);                                  // sync → present_front(); async → the completed front slot
    FlipStats flip_stats() const;                                       // the feedback edge (consumed by the CSV row today)
    Decision last_decision() const { return last_decision_; }

    // ── --tdr-test N (G-R4): armed at init; fired ONCE into a tick's command buffer after N s. L3 (SAFETY §5):
    //    never dispatched unless the flag is on the command line; the operator's explicit word is required to run it.
    bool tdr_arm(const std::vector<uint32_t>& spv, int seconds);
    void tdr_maybe(VkCommandBuffer cmd);
    bool tdr_fired() const { return tdr_fired_; }

private:
    VDev& A_; const Config& cfg_;
    uint32_t& bridge_w_; uint32_t& bridge_h_; bool& bridge_use_km_; void*& bridge_nt_; void*& hostMassPtr_;
    std::atomic<uint64_t>& total_frames_;
    std::atomic<bool>& g_quit_threads_;   // main()'s clean-exit latch (a main() local despite the g_ name — bound, like FgContext does)
    Decision last_decision_ = Decision::Warp;
    int tdr_seconds_ = 0; double tdr_t0_ = 0.0; bool tdr_fired_ = false;
    VkDescriptorSetLayout tdr_dsl_ = VK_NULL_HANDLE; VkPipelineLayout tdr_pl_ = VK_NULL_HANDLE; VkPipeline tdr_pipe_ = VK_NULL_HANDLE;
    VkDescriptorPool tdr_pool_ = VK_NULL_HANDLE; VkDescriptorSet tdr_set_ = VK_NULL_HANDLE; HBuf tdr_sink_{};
    void tdr_destroy();
};

}  // namespace pfg::present
