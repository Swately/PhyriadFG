// PhyriadFG — src/instrument/gdump_book.cpp : GdumpBook, the `--gdump` tap's pure-CPU bookkeeping (S6).
//
// Contract: docs/planning/GDUMP_PLAN.md §1.2 (staging/slot), §1.3 (per-pair), §1.5 (skip-and-count), §2 (the
// record's exact line formats), RR1 (slot reuse) and RR2 (pair-clobber). gdump.hpp is the fixed interface;
// this file is bodies only. No Vulkan, no threads — everything here runs under `pfg_gdump_test` on the CPU.
//
// Two counter definitions this file commits to (the header leaves them implicit — GDUMP_PLAN §1.2/§4 RR1):
//   `recorded` = ticks on which begin_tick() was called (a tick the present thread ATTEMPTED to record a
//                warp into), incremented unconditionally, success or ring-full.
//   `captured` = ticks on which on_submitted() actually pushed a descriptor (a strict subset of `recorded`,
//                since every begin_tick() that returns a slot is always followed by exactly one on_submitted()
//                — GdumpTap's S5(b)->(d) sequencing guarantees this; the ones that returned -1 never submit).
// identity_holds() (`captured + ring_full == recorded`) is exactly this bookkeeping made checkable.
#include "instrument/gdump.hpp"
#include <cassert>
#include <cstdio>
#include <string>

namespace pfg::instrument {

// Round `n` down to the nearest power of two, clamped to kRingMax (the ring's compile-time Capacity — a
// runtime N above it would let begin_tick() hand out a slot try_push() cannot accept). 0 stays 0 (an
// explicitly disarmed ring: every begin_tick() is then a permanent ring_full, per T5).
static uint32_t round_down_pow2(uint32_t n) noexcept {
    if (n == 0u) return 0u;
    if (n > GdumpBook::kRingMax) n = GdumpBook::kRingMax;
    uint32_t p = 1u;
    while (p * 2u <= n) p *= 2u;
    return p;
}

GdumpBook::GdumpBook(uint32_t ring_n, uint32_t pair_sets) noexcept
    : ring_n_(round_down_pow2(ring_n)), pair_sets_(pair_sets > kMaxPairSets ? kMaxPairSets : pair_sets) {
    set_pair_.assign(pair_sets_, 0u);
    for (uint32_t i = 0; i < kMaxPairSets; ++i) set_busy_[i].store(0u, std::memory_order_relaxed);
}

// §1.2: the slot is `write_cursor() & (N-1)` — from the ring, NEVER a submit counter (P4-2, the refuted D1
// indexed by cap_seq; T1 is the regression for exactly this swap). Gate is `size() >= ring_n_` (the RUNTIME
// N; the ring's own Capacity is kRingMax and is never itself the gate — RR1's producer-side check).
int GdumpBook::begin_tick() noexcept {
    ++c_.recorded;
    if (ring_n_ == 0u || ring_.size() >= ring_n_) { ++c_.ring_full; return -1; }
    return (int)(ring_.write_cursor() & (ring_n_ - 1u));
}

// P is the sole producer and begin_tick() already gated capacity against ring_n_ <= kRingMax == the ring's
// Capacity, so try_push() cannot fail here; the assert is the canary if that discipline is ever broken.
void GdumpBook::on_submitted(const GdumpDesc& d) noexcept {
    [[maybe_unused]] const bool pushed = ring_.try_push(d);
    assert(pushed && "GdumpBook::on_submitted: try_push failed after a granted begin_tick() slot (RR1 broken)");
    ++c_.captured;
}

void GdumpBook::skip(GdumpSkip why) noexcept { ++c_.skipped[(size_t)why]; }

// §1.3 / RR2: at most one pair is ever pending. A new upload while one is still pending means no tick
// recorded that pair's GPU planes before the source was overwritten — it is unrecoverable, so discard-and-
// count rather than silently keep stale data. Then claim the first free set, or count pairs_busy.
int GdumpBook::on_pair_upload(uint64_t pair, int /*gen*/, int /*tgen*/) noexcept {
    ++c_.pairs_uploaded;
    if (pending_set_ >= 0) {
        set_busy_[(size_t)pending_set_].store(0u, std::memory_order_release);   // free — its GPU images no longer hold that pair (§1.3)
        ++c_.pairs_clobbered;
        pending_set_ = -1;
    }
    for (uint32_t i = 0; i < pair_sets_; ++i) {
        if (set_busy_[i].load(std::memory_order_acquire) == 0u) {   // acquire: pairs with the writer's release-store on free
            set_busy_[i].store(1u, std::memory_order_release);      // pending: host planes in, GPU planes not yet recorded
            set_pair_[i] = pair;
            pending_set_ = (int)i;
            pending_pair_ = pair;
            return (int)i;
        }
    }
    ++c_.pairs_busy;
    return -1;
}

int GdumpBook::claim_pending_pair() noexcept {
    if (pending_set_ < 0) return -1;
    const int idx = pending_set_;
    set_busy_[(size_t)idx].store(2u, std::memory_order_release);   // recorded — awaiting the writer
    pending_set_ = -1;
    return idx;
}

// The writer's side: the set is free again (release-store, so P's acquire-scan sees every plane write the
// writer finished before it), and pairs_written counts only what reached the disk (`written`, RR4).
void GdumpBook::release_pair_set(int idx, bool written) noexcept {
    if (idx < 0 || (uint32_t)idx >= pair_sets_) return;
    if (written) ++c_.pairs_written;
    set_busy_[(size_t)idx].store(0u, std::memory_order_release);   // free for the next on_pair_upload
}

// §2's exact ticks.tsv line: "seq slot tick t gen tgen pair decision flags qpc live_off push_off pairset\n".
// `decision` is the literal word "warp" (ticks.tsv only ever records captured warps — non-recording ticks
// are counted in summary.txt's skipped[], never given a line here). `t` is printed %.6f (P6-5: the push
// phase, not a byte-exact hex float — this file is a human/np.loadtxt-readable index, not the replay oracle).
std::string GdumpBook::index_line(const GdumpDesc& d, uint64_t live_off, uint64_t push_off) {
    char buf[256];
    const int n = std::snprintf(buf, sizeof buf,
        "%llu %u %llu %.6f %d %d %llu warp %u %lld %llu %llu %d\n",
        (unsigned long long)d.seq, d.slot, (unsigned long long)d.tick, (double)d.t, d.gen, d.tgen,
        (unsigned long long)d.pair, d.flags, (long long)d.qpc,
        (unsigned long long)live_off, (unsigned long long)push_off, d.pairset);
    return std::string(buf, buf + (n > 0 ? (size_t)n : 0));
}

// §2's summary.txt: one "key value" line per field, in the table's order, then the reconciliation identity
// and the P7-1 note about `interp=` in the FG's own exit line (not this file's business to compute — just to
// disclaim, since an operator reading both side by side would otherwise read `interp=` as a frame count).
std::string GdumpBook::summary_text(const Counters& c, uint64_t total_presents) {
    char buf[1024];
    const int n = std::snprintf(buf, sizeof buf,
        "ticks %llu\n"
        "recorded %llu\n"
        "captured %llu\n"
        "ring_full %llu\n"
        "drop %llu\n"
        "dup %llu\n"
        "decimated %llu\n"
        "real %llu\n"
        "pairs_uploaded %llu\n"
        "pairs_written %llu\n"
        "pairs_busy %llu\n"
        "pairs_clobbered %llu\n"
        "writer_timeouts %llu\n"
        "bytes %llu\n"
        "total_presents %llu\n"
        "identity captured+ring_full==recorded : %s\n"
        "note interp= in the exit line is presents - ingested reals, not a generated-frame count\n",
        (unsigned long long)c.ticks, (unsigned long long)c.recorded, (unsigned long long)c.captured,
        (unsigned long long)c.ring_full,
        (unsigned long long)c.skipped[(size_t)GdumpSkip::Drop], (unsigned long long)c.skipped[(size_t)GdumpSkip::Dup],
        (unsigned long long)c.skipped[(size_t)GdumpSkip::Decimated], (unsigned long long)c.skipped[(size_t)GdumpSkip::Real],
        (unsigned long long)c.pairs_uploaded, (unsigned long long)c.pairs_written,
        (unsigned long long)c.pairs_busy, (unsigned long long)c.pairs_clobbered,
        (unsigned long long)c.writer_timeouts, (unsigned long long)c.bytes, (unsigned long long)total_presents,
        identity_holds(c) ? "ok" : "BROKEN");
    return std::string(buf, buf + (n > 0 ? (size_t)n : 0));
}

}  // namespace pfg::instrument
// Made with my soul - Swately <3
