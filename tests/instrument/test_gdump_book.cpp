// PhyriadFG — tests/instrument/test_gdump_book.cpp : GdumpBook's CPU test (GDUMP_PLAN.md §6 G0, RR1, RR2).
// GPU-free, Vulkan-free (only vulkan.h's TYPE definitions are pulled in via gdump.hpp/device.hpp, never
// called): everything here is the pure-CPU half of the tap (S6). Five tests, each printing [OK]/[FAIL] per
// assertion in the style of the repo's other CPU tests (tests/clock/test_phase_clock.cpp):
//
//   T1 RR1 — the staging-slot indexing: the refuted P3-4 design (slot from a free-running submit counter)
//            seen red against a real slow-writer interleaving, GdumpBook::begin_tick() (slot from
//            ring.write_cursor(), gated by size() < ring_n) seen green on the SAME interleaving.
//   T2 RR2 — the pair-set state machine: an unrecorded pending pair superseded by a re-upload is discarded
//            and counted (pairs_clobbered); claim/release cycles a set back to free; every set
//            pending-then-claimed (never released) makes the next upload fail with pairs_busy.
//   T3      — the reconciliation identity (captured + ring_full == recorded) over a randomized
//             begin_tick/on_submitted/pop run, then deliberately broken to prove identity_holds() actually
//             checks something (seen red under corruption).
//   T4      — the §2 record's exact text: one ticks.tsv index_line, one summary.txt summary_text, both
//             pinned against literal expected strings.
//   T5      — ring_n construction-time rounding (round DOWN to a power of two ≤ kRingMax; 0 stays 0).
//
// Build: the `pfg_gdump_test` target (CMakeLists.txt). Run: pfg_gdump_test
#include "instrument/gdump.hpp"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace pfg::instrument;

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  %s  %s\n", ok ? "[OK]" : "[FAIL]", what);
    if (!ok) ++g_fail;
}

// ── T1 — RR1: the staging slot must come from ring.write_cursor(), never a free-running submit counter ───
// Both sims share one writer model: a "slow" writer that, once it starts streaming a staging slot, holds it
// for W ticks before popping — GDUMP_PLAN §1.4 ("write the frame ... straight to the stream ... then pop —
// the pop is what frees the slot"). A violation is the producer writing into a slot the writer is still
// mid-stream on: the writer would then read bytes the producer overwrote underneath it.
namespace t1 {

// (a) the refuted design (P3-4, GDUMP_PLAN §1.2): slot = cap_seq & (N-1), cap_seq a counter that advances on
// every tick's submit with no regard for whether the writer — and therefore the ring's real occupancy — has
// kept up. GdumpBook never held this logic; there is nothing real to call, so it is modeled locally.
static bool refuted_run(uint32_t N, int W, int ticks) {
    std::vector<bool> streaming(N, false);
    uint64_t cap_seq = 0, writer_next = 0;
    int writer_busy = 0; uint32_t writer_slot = 0;
    bool violation = false;
    for (int t = 0; t < ticks; ++t) {
        const uint32_t slot = (uint32_t)(cap_seq & (N - 1u));
        if (streaming[slot]) violation = true;      // the writer is still reading this slot — corrupted
        ++cap_seq;                                   // advances on EVERY submit, uncaptured ones included
        if (writer_busy > 0) {
            if (--writer_busy == 0) streaming[writer_slot] = false;
        } else if (writer_next < cap_seq) {           // idle and a frame is already waiting: start it
            writer_slot = (uint32_t)(writer_next & (N - 1u));
            streaming[writer_slot] = true;
            writer_busy = W;
            ++writer_next;
        }
    }
    return violation;
}

// (b) the fix: slot from the REAL GdumpBook::begin_tick() (write_cursor()-gated). Deriving the slot from the
// actual class under test is what makes this regress if begin_tick()'s indexing is ever swapped back.
static bool fixed_run(uint32_t N, int W, int ticks) {
    GdumpBook book(N, 1);
    std::vector<bool> streaming(N, false);
    int writer_busy = 0; uint32_t writer_slot = 0;
    uint64_t frame = 0;
    bool violation = false;
    for (int t = 0; t < ticks; ++t) {
        const int slot = book.begin_tick();
        if (slot >= 0) {
            if (streaming[(uint32_t)slot]) violation = true;   // provably impossible for this indexing
            GdumpDesc d{}; d.seq = frame++; d.slot = (uint32_t)slot; d.tick = (uint64_t)t;
            book.on_submitted(d);
        }
        if (writer_busy > 0) {
            if (--writer_busy == 0) { streaming[writer_slot] = false; book.pop(); }
        } else {
            GdumpDesc out;
            if (book.peek(out)) { writer_slot = out.slot; streaming[writer_slot] = true; writer_busy = W; }
        }
    }
    return violation;
}

static void run() {
    std::printf("[T1] RR1 -- staging-slot indexing (seen red on the free-running counter, green on write_cursor)\n");
    const uint32_t N = 2; const int W = 3, ticks = 16;   // W > N: the writer falls behind, forcing reuse
    const bool red   = refuted_run(N, W, ticks);
    const bool green = fixed_run(N, W, ticks);
    check(red,   "the refuted free-running-counter indexing overwrites a slot still being streamed");
    check(!green, "GdumpBook::begin_tick()'s write_cursor indexing never overwrites a streaming slot");
}

}  // namespace t1

// ── T2 — RR2: the pair-set state machine (clobber / claim / release / busy) ────────────────────────────────
namespace t2 {
static void run() {
    std::printf("[T2] RR2 -- pair-set state machine\n");
    GdumpBook book(4, 2);   // ring_n is irrelevant to this test; M=2 pair sets

    const int idx1 = book.on_pair_upload(10, 0, -1);
    check(idx1 >= 0, "on_pair_upload(pair=10) finds a free set");
    check(book.pair_pending() && book.pending_pair_id() == 10,
          "pair 10 is pending -- no tick has recorded its GPU planes yet (a Drop)");

    const int idx2 = book.on_pair_upload(11, 1, -1);   // supersedes the still-pending, unrecorded pair 10
    check(book.counters().pairs_clobbered == 1,
          "the unrecorded pair 10 was discarded and counted when 11 superseded it (pairs_clobbered)");
    check(idx2 >= 0, "on_pair_upload(pair=11) finds a set (the clobber freed one)");
    check(book.pair_pending() && book.pending_pair_id() == 11, "the newly pending set holds pair 11");

    const int claimed1 = book.claim_pending_pair();
    check(claimed1 == idx2, "the tick that records claims the set pair 11 is pending in");
    check(!book.pair_pending(), "no pair is pending immediately after the claim");

    book.release_pair_set(claimed1);
    check(book.counters().pairs_written == 1, "the writer's release is counted as pairs_written");

    check(book.claim_pending_pair() == -1, "claim_pending_pair() with nothing pending returns -1");

    // M sets, all pending-then-claimed and never released: the set released above must be reusable, and once
    // every set is busy again the next upload must fail with pairs_busy.
    int filled = 0;
    for (uint64_t p = 20; p < 20u + book.pair_sets(); ++p) {
        const int idx = book.on_pair_upload(p, 0, -1);
        check(idx >= 0, "on_pair_upload finds a free set while any set remains free");
        const int claimed = book.claim_pending_pair();
        check(claimed == idx, "claim_pending_pair returns the set the upload just marked pending");
        ++filled;
    }
    check(filled == (int)book.pair_sets(), "every pair set is now pending-then-claimed (busy, unreleased)");

    check(book.counters().pairs_busy == 0, "pairs_busy is still 0 -- every set found room above");
    const int idx3 = book.on_pair_upload(99, 0, -1);
    check(idx3 == -1, "with every set busy, on_pair_upload returns -1");
    check(book.counters().pairs_busy == 1, "the refusal is counted (pairs_busy == 1)");
}
}  // namespace t2

// ── T3 — the reconciliation identity, seen green then deliberately seen red ────────────────────────────────
namespace t3 {
static void run() {
    std::printf("[T3] the reconciliation identity (captured + ring_full == recorded)\n");
    GdumpBook book(8, 2);
    unsigned seed = 20260909u;
    auto rnd = [&]{ seed = seed * 1664525u + 1013904223u; return (seed >> 8) & 0xFFFFu; };
    uint64_t frame = 0;
    for (int i = 0; i < 500; ++i) {
        const int slot = book.begin_tick();
        if (slot >= 0) {
            GdumpDesc d{}; d.seq = frame++; d.slot = (uint32_t)slot; d.tick = (uint64_t)i;
            book.on_submitted(d);
        }
        if ((rnd() & 3u) == 0u) book.pop();     // the writer, popping at its own (slower, jittery) pace
    }
    GdumpDesc tmp;
    while (book.peek(tmp)) book.pop();          // drain the rest, as the real writer does at stop()

    check(GdumpBook::identity_holds(book.counters()),
          "captured + ring_full == recorded holds after a random begin_tick/on_submitted/pop run");

    book.counters().captured += 1;              // deliberate corruption -- proves identity_holds() checks something
    check(!GdumpBook::identity_holds(book.counters()),
          "a corrupted captured counter breaks the identity (seen red)");
}
}  // namespace t3

// ── T4 — the §2 record's exact text ─────────────────────────────────────────────────────────────────────
namespace t4 {
static void run() {
    std::printf("[T4] the SS2 record's formatting: ticks.tsv's index_line, summary.txt's summary_text\n");

    GdumpDesc d{};
    d.seq = 42; d.slot = 3; d.tick = 1000; d.t = 0.5f; d.gen = 1; d.tgen = 2; d.pair = 7;
    d.flags = kFlagPair | kFlagXfer; d.qpc = 123456789; d.pairset = 5;
    const std::string line = GdumpBook::index_line(d, /*live_off=*/8192, /*push_off=*/256);
    const std::string want_line = "42 3 1000 0.500000 1 2 7 warp 3 123456789 8192 256 5\n";
    check(line == want_line, "index_line matches the SS2 ticks.tsv line, token for token");
    if (line != want_line) std::printf("    got:  %s    want: %s", line.c_str(), want_line.c_str());

    GdumpBook::Counters c{};
    c.ticks = 100; c.recorded = 90; c.captured = 85; c.ring_full = 5;
    c.skipped[(size_t)GdumpSkip::Drop] = 3; c.skipped[(size_t)GdumpSkip::Dup] = 2;
    c.skipped[(size_t)GdumpSkip::Decimated] = 1; c.skipped[(size_t)GdumpSkip::Real] = 4;
    c.pairs_uploaded = 10; c.pairs_written = 8; c.pairs_busy = 1; c.pairs_clobbered = 1;
    c.writer_timeouts = 0; c.bytes = 123456;
    const std::string sum = GdumpBook::summary_text(c, /*total_presents=*/200);
    const std::string want_sum =
        "ticks 100\n"
        "recorded 90\n"
        "captured 85\n"
        "ring_full 5\n"
        "drop 3\n"
        "dup 2\n"
        "decimated 1\n"
        "real 4\n"
        "pairs_uploaded 10\n"
        "pairs_written 8\n"
        "pairs_busy 1\n"
        "pairs_clobbered 1\n"
        "writer_timeouts 0\n"
        "bytes 123456\n"
        "total_presents 200\n"
        "identity captured+ring_full==recorded : ok\n"
        "note interp= in the exit line is presents - ingested reals, not a generated-frame count\n";
    check(sum == want_sum, "summary_text matches the SS2 summary.txt text, line for line");
    if (sum != want_sum) std::printf("    got:\n%s    want:\n%s", sum.c_str(), want_sum.c_str());
}
}  // namespace t4

// ── T5 — ring_n construction-time rounding ──────────────────────────────────────────────────────────────
namespace t5 {
static void run() {
    std::printf("[T5] ring_n rounding (round DOWN to a power of two <= kRingMax; 0 stays 0)\n");
    GdumpBook rounded(100, 4);
    check(rounded.ring_n() == 64, "GdumpBook(100,4).ring_n() == 64 (100 rounds down to the pow2 below it)");

    GdumpBook zero(0, 4);
    const int slot = zero.begin_tick();
    check(slot == -1, "GdumpBook(0,4).begin_tick() == -1 (a zero-size ring is always full)");
    check(zero.counters().ring_full == 1, "ring_full counts the refusal");
}
}  // namespace t5

// T6 (--gdump-live, LEVER1_RECORD s4.12): with the live frames off, a captured tick writes no live bytes and advances
// live_off by 0; on, it is the logical frame size, as before.
namespace t6 {
static void run() {
    std::printf("[T6] --gdump-live: the live-frame bytes a captured tick writes\n");
    const uint64_t fb = 1920ull * 1061ull * 4ull;
    check(GdumpBook::live_frame_bytes(true, fb) == fb, "live on: the logical frame size (today's tap)");
    check(GdumpBook::live_frame_bytes(false, fb) == 0, "live off: 0 bytes written, live_off does not advance");
    check(GdumpBook::live_frame_bytes(false, 0) == 0 && GdumpBook::live_frame_bytes(true, 0) == 0, "a zero-size frame is 0 either way");
}
}  // namespace t6

int main() {
    std::printf("pfg_gdump_test -- GdumpBook, GDUMP_PLAN.md S6 / RR1 / RR2\n");
    t1::run();
    t2::run();
    t3::run();
    t4::run();
    t5::run();
    t6::run();
    if (g_fail) std::printf("RESULT: %d CHECK(S) FAILED\n", g_fail);
    else        std::printf("RESULT: PASS -- all checks green\n");
    return g_fail ? 1 : 0;
}

// Made with my soul - Swately <3
