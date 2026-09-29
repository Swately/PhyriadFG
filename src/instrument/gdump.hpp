#pragma once
// PhyriadFG — src/instrument/gdump.hpp : `--gdump <dir>`, the every-tick capture tap (INSTRUMENT plane).
//
// Contract: docs/planning/GDUMP_PLAN.md (§1 mechanism, §2 the record, §4 the risk register). Read it before
// touching either side of this header. The present thread ("P") calls the `GdumpTap` methods below at the five
// sites listed in the plan's S5; a dedicated writer thread (owned by the tap) streams the captured frames to
// disk. P NEVER waits on the tap: a full ring or a busy pair set is skipped and counted (§1.5).
//
// Two classes, one file, so the bookkeeping can be tested without a GPU (S6):
//   GdumpBook — pure CPU: the SPSC descriptor ring, the staging-slot cursor, the pair-set state machine, every
//               counter, the index/summary formatting, the reconciliation identity. No Vulkan, no threads.
//   GdumpTap  — the Vulkan + thread glue around a GdumpBook: staging buffers (hbuf_import), the timeline
//               semaphore, the copy commands, the writer thread, the files.
//
// Off = byte-identical: nothing in this header is instantiated unless cfg.gdump_on (S7).
// Made with my soul - Swately <3
#include "core/device.hpp"
#include "core/vk_util.hpp"
#include <phyriad/ipc/Ring.hpp>
#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace pfg::instrument {

// The warp push block (`pcw`, present.cpp ~1121) is 58 floats = 232 bytes; the record's push.bin stride. A static_assert
// at the shadow-copy site pins sizeof(pcw) to this, so the two cannot drift apart silently.
inline constexpr uint32_t kGdumpPushBytes = 232u;

// ── The descriptor P pushes once per captured tick (64 bytes; trivially copyable — ipc::Ring's static_assert) ──
struct GdumpDesc {
    uint64_t seq;        // cap_seq of the tick = the semCapTL value the writer waits for (advances on EVERY recorded submit)
    uint64_t pair;       // the monotone pair id (pair_c: the capture sequence of the pair's B frame) — NEVER f_gen
    uint64_t tick;       // the present loop's tick_k
    int64_t  qpc;        // QueryPerformanceCounter at submit
    float    t;          // the phase the warp used (push t)
    uint32_t slot;       // staging slot = write_cursor & (N-1) at record time
    int32_t  gen;        // f_gen (the FlowRing slot, informational — the pair id is `pair`)
    int32_t  tgen;       // target_gen (u_mv_target's slot), -1 if none
    uint32_t flags;      // GdumpFlag bits
    int32_t  pairset;    // the pair set whose GPU planes this tick's cmd copied (flags & kPair), else -1
    uint32_t reserved[2];   // 8+8+8+8 + 4+4 + 4+4 + 4+4 + 8 = 64
};
static_assert(sizeof(GdumpDesc) == 64, "GdumpDesc must stay 64 bytes (the ring slot and the index contract)");

enum GdumpFlag : uint32_t {
    kFlagPair  = 1u << 0,   // this tick's command buffer carried the pair copies of `pair` into `pairset`
    kFlagXfer  = 1u << 1,   // --upload-xfer was on (the semWarpTL WAR edge now includes the copy — CR4/P1-4)
    kFlagSg    = 1u << 2,   // the copy was recorded inside the seam-graph blit pass (--sg-barriers)
    kFlagQdump = 1u << 3,   // --qdump dumped this tick too (see qdump_xref.tsv)
};

// The tick decisions P counts on ticks that do NOT record (STAGE_CONTRACT §1 + the real-present paths).
enum class GdumpSkip : uint8_t { Drop = 0, Dup, Decimated, Real, RingFull, kCount };

// ── The per-pair HOST planes P memcpy's at the upload site (RR3). Sizes are in bytes; a null src = absent. ──
struct GdumpPairHost {
    const void* sad;   const void* c2;   const void* dis;  const void* disb; const void* per;
    const void* mvt;   const void* mv;   const void* mvb;
    const float* gme6; int gme_valid;
};

// ── GdumpBook — the CPU bookkeeping (testable) ───────────────────────────────────────────────────────────────
class GdumpBook {
public:
    static constexpr uint32_t kRingMax = 1024;  // the descriptor ring's compile-time capacity (N ≤ kRingMax; N = ring_n at runtime).
                                                // Measured 2026-09-09 (observer r2, 640×360 @ 240 fps on the 980 PRO): the writer stalls in
                                                // BURSTS of 0.26–0.73 s as the OS write cache flushes a growing stream — a 64-slot ring
                                                // (0.27 s) lost 6 % of the ticks; the default is now 256 (1.07 s) and the stream is unbuffered.
    using Ring = phyriad::ipc::Ring<GdumpDesc, kRingMax>;

    explicit GdumpBook(uint32_t ring_n, uint32_t pair_sets) noexcept;

    // P side (single producer) ──────────────────────────────────────────────
    // A staging slot is free iff fewer than ring_n descriptors are outstanding (size() < ring_n — the ring's
    // capacity is kRingMax so the runtime N is enforced here, RR1). Returns the slot index (write_cursor & (N-1))
    // or -1 (and counts RingFull). Call BEFORE recording the copy.
    int      begin_tick() noexcept;
    // After the submit: push the descriptor (cannot fail after begin_tick() returned a slot — P is the only producer).
    void     on_submitted(const GdumpDesc& d) noexcept;
    // A tick that did not record (or recorded but was not captured): count it by reason.
    void     skip(GdumpSkip why) noexcept;
    // Pair state machine (RR2): the upload site marks a pair pending in a free set (or counts pairs_busy);
    // a pending pair that is superseded before any tick records it is discarded and counted (pairs_clobbered).
    // Returns the pair set index to fill (host planes memcpy'd by the caller into pair_host(idx)), or -1.
    int      on_pair_upload(uint64_t pair, int gen, int tgen) noexcept;
    bool     pair_pending() const noexcept { return pending_set_ >= 0; }
    // The tick that records the pair's GPU copies claims the pending set: returns its index and clears pending.
    int      claim_pending_pair() noexcept;
    uint64_t pending_pair_id() const noexcept { return pending_pair_; }

    // Writer side (single consumer) ────────────────────────────────────────
    bool     peek(GdumpDesc& out) noexcept { return ring_.peek_at(read_cursor_, out); }
    void     pop() noexcept { GdumpDesc d; if (ring_.try_pop(d)) ++read_cursor_; }
    // The writer releases a pair set after handling its planes (frees it for on_pair_upload); `written` = the
    // planes reached the disk (counted as pairs_written) — false when write_pair refused it (no SAD, RR4).
    void     release_pair_set(int idx, bool written = true) noexcept;

    // Counters (P-only writes except `written`, which the writer bumps; read at stop after the join) ─────
    struct Counters {
        uint64_t ticks = 0, recorded = 0, captured = 0, ring_full = 0;
        uint64_t skipped[(size_t)GdumpSkip::kCount] = {};
        uint64_t pairs_uploaded = 0, pairs_written = 0, pairs_busy = 0, pairs_clobbered = 0;
        uint64_t writer_timeouts = 0, bytes = 0, written = 0;
    };
    Counters&       counters() noexcept { return c_; }
    const Counters& counters() const noexcept { return c_; }
    uint32_t        ring_n() const noexcept { return ring_n_; }
    uint32_t        pair_sets() const noexcept { return pair_sets_; }
    uint64_t        write_cursor() const noexcept { return ring_.write_cursor(); }

    // Formatting (the §2 contract; pure functions of their inputs so the test can pin them) ───────────────
    static std::string index_line(const GdumpDesc& d, uint64_t live_off, uint64_t push_off);
    static std::string summary_text(const Counters& c, uint64_t total_presents);
    // The reconciliation identity of §2: captured + ring_full == recorded. False = a bookkeeping defect.
    static bool identity_holds(const Counters& c) noexcept { return c.captured + c.ring_full == c.recorded; }
    // --gdump-live: the live-frame bytes one captured tick writes (and advances live_off by). 0 when the tap records no
    // live frames: the writer then spends the disk on the push/index and the pair sets only.
    static uint64_t live_frame_bytes(bool live, uint64_t frame_bytes) noexcept { return live ? frame_bytes : 0u; }

private:
    Ring     ring_;
    uint64_t read_cursor_ = 0;          // writer-local (the multi-reader pattern of Ring.hpp; one reader here)
    uint32_t ring_n_, pair_sets_;
    // pair sets: set_pair_ = the pair id a set holds (P-only); set_busy_ is the ONE field both threads touch —
    // P stores 1 (pending) / 2 (recorded) and scans for 0; the writer stores 0 on release — so it is atomic
    // (release/acquire), never a plain byte (a data race would be UB, and the ordering is what frees the set).
public:
    static constexpr uint32_t kMaxPairSets = 16;   // pair_sets is clamped to this at construction
private:
    std::vector<uint64_t> set_pair_;
    std::atomic<uint8_t>  set_busy_[kMaxPairSets]{};   // 0 free · 1 pending (host planes in, GPU planes not yet recorded) · 2 recorded (awaiting the writer)
    int      pending_set_ = -1; uint64_t pending_pair_ = 0;
    Counters c_;
};

// ── GdumpTap — the Vulkan + thread glue ─────────────────────────────────────────────────────────────────────
struct GdumpDims { uint32_t WW, WH, WW_warp, WH_warp, warp_div, mvw, mvh; uint32_t push_bytes; };
struct GdumpInfo { const char* kernel; uint64_t contract; bool bidir, xfer, async_present, sg; double qpc_hz;
                   bool live = true;      // live: --gdump-live (false = no live warp frames; LEVER1_RECORD s4.12)
                   int  eco_anchor = 0;   // 2026-09-29: the PRODUCT's eco_anchor mode (0 = off; the header line is absent then)
                   float eco_hyst = 0.f; };   // its hyst, so ref_warp replays the value the GPU was given

class GdumpTap {
public:
    // Allocates the staging (N frame slots + M pair sets, hbuf_import on A), creates semCapTL (TIMELINE), opens
    // the files, writes gdump.hdr, starts the writer. On any failure: prints one line, armed() stays false, and
    // everything allocated so far is released — the run continues without the tap (a diagnostic never kills WAP).
    // `dir` null or empty = the flag is OFF: the constructor returns IMMEDIATELY, silently, allocating nothing and
    // printing nothing (S7 byte-identical-off — the object always exists in run_present; its calls are inline no-ops).
    // ring_n == 0 with a dir = ARMED WITHOUT COPYING (the semaphore signal, the feature, the usage bits, the writer,
    // but begin_tick() always returns -1): the observer gate's third arm (CR5).
    GdumpTap(VDev& A, const char* dir, uint32_t ring_n, uint32_t pair_sets, GdumpDims dims, GdumpInfo info);
    ~GdumpTap();   // joins the writer FIRST (CR3), then destroys the semaphore and the buffers
    GdumpTap(const GdumpTap&) = delete; GdumpTap& operator=(const GdumpTap&) = delete;

    bool armed() const noexcept { return armed_; }

    // ── P side, in tick order (GDUMP_PLAN §3 S5) ──
    int  begin_tick() noexcept { return armed_ ? book_.begin_tick() : -1; }                   // (b) before recording
    // (b) the frame copy: `out` must be TRANSFER_SRC_OPTIMAL at this point (inside the blit pass / between :1299 and :1300)
    void record_frame_copy(VkCommandBuffer c, VkImage out, int slot) noexcept;
    // (b) the pair copies, only when pair_pending(): prev/cur (WW×WH) and mv1/mvb1 (mvw×mvh; mvb may be VK_NULL_HANDLE).
    //     Records the RO->TRANSFER_SRC->RO barrier pairs itself (present.cpp:1418-1437's pattern). Returns the pair set.
    int  record_pair_copies(VkCommandBuffer c, VkImage prev, VkImage cur, VkImage mv, VkImage mvb) noexcept;
    bool pair_pending() const noexcept { return armed_ && book_.pair_pending(); }
    // (c) the second timeline signal: the semaphore and the value THIS submit signals (++cap_seq, every recorded submit)
    VkSemaphore semaphore() const noexcept { return sem_; }
    uint64_t    next_signal() noexcept { return ++cap_seq_; }
    // (d) after pres.submit: push the descriptor (seq = the value next_signal() returned this tick) + shadow the push block
    void on_submitted(GdumpDesc d, const void* push, uint32_t push_bytes) noexcept;
    // (e) at the pair-advance site, right after wap_upload returns: memcpy the host planes (RR3) + mark pending
    void on_pair_upload(uint64_t pair, int gen, int tgen, const GdumpPairHost& h) noexcept;
    // (f) a tick that does not record / capture
    void skip(GdumpSkip why) noexcept { if (armed_) book_.skip(why); }
    void tick() noexcept { if (armed_) ++book_.counters().ticks; }
    // (g) --qdump dumped on the tick whose descriptor was last pushed (sync path only)
    void note_qdump(int qdump_idx) noexcept;
    // stop: join the writer, write summary.txt (with the FG's own total_presents), close the files. Idempotent.
    void stop(uint64_t total_presents) noexcept;

    const GdumpBook& book() const noexcept { return book_; }

private:
    struct Slot { void* ptr = nullptr; HBuf buf{}; std::vector<uint8_t> push; };
    struct PairSet {
        void* prev = nullptr; void* cur = nullptr; void* mv1 = nullptr; void* mvb1 = nullptr;   // hbuf_import'd (GPU-written)
        HBuf  bprev{}, bcur{}, bmv1{}, bmvb1{};
        std::vector<uint8_t> sad, c2, dis, disb, per, mvt, mv, mvb;                              // host memcpy'd (P)
        float gme6[6] = {}; int gme_valid = 0; uint64_t pair = 0; int gen = -1, tgen = -1; bool has_mvb1 = false;
        bool has_sad = false, has_c2 = false, has_dis = false, has_disb = false, has_per = false, has_mvt = false, has_mv = false, has_mvb = false;
    };
    void writer_main() noexcept;
    bool write_pair(const PairSet& s, uint64_t seq_recorded);

    VDev&       A_;
    GdumpBook   book_;
    GdumpDims   dims_;
    GdumpInfo   info_;
    std::string dir_;
    std::vector<Slot>    slots_;
    std::vector<PairSet> sets_;
    VkSemaphore sem_ = VK_NULL_HANDLE;
    uint64_t    cap_seq_ = 0;          // P-only
    uint64_t    last_seq_ = 0;         // the seq of the last pushed descriptor (for note_qdump)
    std::FILE*  f_live_ = nullptr; std::FILE* f_push_ = nullptr; std::FILE* f_ticks_ = nullptr; std::FILE* f_pairs_ = nullptr; std::FILE* f_xref_ = nullptr;
    void*       live_raw_ = nullptr;   // HANDLE: live.rgba opened FILE_FLAG_NO_BUFFERING when the frame is sector-aligned (PR2); INVALID_HANDLE_VALUE = the stdio stream is in use
    uint64_t    live_off_ = 0, push_off_ = 0;   // writer-only
    std::thread writer_;
    std::atomic<bool> stop_{false};
    bool armed_ = false, stopped_ = false;
};

}  // namespace pfg::instrument
