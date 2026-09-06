#pragma once
// PhyriadFG — src/ingest/frames.hpp : the two ingest-side RINGS of STAGE_CONTRACT §1 (R6 step 2).
//
//   FrameRing (2 → 3, 2 → 4)  — the WORKING frames the rest of the pipeline reads (`hR_a[s]` on A, `Bframe[s]` on
//                               the flow device), addressed by `s = c_seq % cap_slots`, with the timestamp pair
//                               stage 4 needs: `t_cap_ms` (when the frame was captured — the freshage anchor) and
//                               `t_pub_ms` (when stage 2 published it).
//   RawRing   (1 → 2)         — what the ACQUIRER deposited before any Vulkan work, under `--ingest-async` only
//                               (default OFF; on the serial path the acquirer converts in place and no raw frame is
//                               ever published): the host-staged copy, its import on A (TRANSFER_SRC) and, when the
//                               iGPU convert is live, on G (STORAGE), the capture timestamp, and the two WGC
//                               latency stamps that ride with it.
//
// Both follow the discipline the FlowRing states (flow/flow_set.hpp): ONE seq_cst counter publishes, every field of
// the slot is written BEFORE the `fetch_add`, and the consumer reads the counter before the fields. Drop-to-newest
// lives in the CONSUMERS (the convert worker takes `raw_seq - 1` and abandons what it held; the flow takes the
// newest published real) — the rings never queue. The rings OWN their counters and BIND the slot storage by
// reference: the buffers are allocated by the host-bridge layer only when their producer is armed, and `main()`
// keeps the former names as aliases, so every consumer reads and writes exactly the memory it did.
//
// WHAT IS DELIBERATELY NOT HERE (R6 step 2's honest scope): a per-frame `RealFrame` / `RawFrame` VIEW.
// STAGE_CONTRACT §1 names those two types, and the plan says the structs "replace the loose locals" — but the eight
// readers of `c_slots[]` (capture, flow, flow_consume, present ×2) address the ring by an ARBITRARY SLOT, not by
// the publish sequence: `c_slots[rfp_slot]`, `c_slots[mf_slot]`, `c_slots[s]` for a slot the caller already chose.
// A view keyed on `seq` would not fit them, and a view keyed on `slot` would be a struct with one member. Inventing
// either would be a wrapper with no consumer — the container's rule 1. The contract's names are honoured by the
// rings and by `publish()` below; the per-frame view arrives when a reader needs one, not before.
// Made with my soul - Swately <3
#include <atomic>
#include <cstdint>
#include "core/fg_context.hpp"   // RealSlot, kRawSlots, HBuf

namespace pfg::ingest {

// ── FrameRing (2 → 3, 2 → 4) ───────────────────────────────────────────────────────────────────
// `cap_slots` is the ACTIVE depth (auto-sized at init from the source/flow/resolution gap); `kCapSlots` is only the
// compile-time maximum of the handle arrays, which is why the modulus reads the active value, not the array bound.
struct FrameRing {
    std::atomic<uint64_t>& seq;        // c_seq: the publish counter (seq_cst)
    RealSlot* slots;                   // the c_slots array; kCapSlots is only its compile-time bound and lives in
                                       // core/app_init.hpp, so the ring takes the pointer and indexes with cap_slots
    const int& cap_slots;

    int slot_of(uint64_t s) const { return (int)(s % (uint64_t)cap_slots); }

    // THE publish. The order below is the contract, not a preference: `t_pub_ms` must be written BEFORE the
    // seq_cst `fetch_add`, because that store/load pair is the only thing that orders the slot's fields for the
    // reader that observes the new sequence. Stamping after the bump would let F read a value not yet written.
    // Both publish sites (the serial tail and the --ingest-async worker) go through here, so the invariant lives
    // in one place instead of being repeated as a convention at each — R6's "MUST NOT change" made structural.
    // `stamp_ms` = 0.0 means "no latency trace this run": the field is left alone, exactly as before.
    uint64_t publish(int slot, double stamp_ms) {
        if (stamp_ms != 0.0) slots[slot].t_pub_ms = stamp_ms;
        return seq.fetch_add(1);
    }
};

// ── RawRing (1 → 2) — armed only under --ingest-async ──────────────────────────────────────────
struct RawRing {
    static constexpr int N = kRawSlots;
    std::atomic<uint64_t>& seq;        // "newest published raw frame index + 1"; the worker reads seq-1
    void*   (&host)[N];
    HBuf    (&import_a)[N];
    HBuf    (&import_g)[N];
    double  (&t_cap)[N];
    double  (&lt_submit)[N];
    double  (&lt_compose)[N];
    static int slot_of(uint64_t s) { return (int)(s % (uint64_t)N); }
};

}  // namespace pfg::ingest
