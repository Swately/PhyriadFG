#pragma once
// PhyriadFG — src/ingest/ingest.hpp : STAGE 2 (INGEST) — the Vulkan convert + the publish into the FrameRing.
//
// STAGE_CONTRACT §2 stage 2: from a `RawFrame` (whatever the backend deposited) produce the WORKING image the rest
// of the pipeline reads, and publish it as a `RealFrame` in the FrameRing. Stage 1 (acquisition) stays in
// capture/capture.cpp: the WGC callback, the DDA loop, the dedup and the drop-to-newest rule are backend-specific
// and belong to the acquirer; everything below the acquisition is this module.
//
// Two entry points, both moved by anchor from run_capture (R6 step 1; the bodies are verbatim — the measure is in
// records/R6_GATE.md §1):
//   ingest_convert_and_publish(ctx, cap_rot180, s)
//       The SERIAL path's tail: convert on the primary (A.q2 under the queue lock when the flow shares it) or on
//       the iGPU (G) when --convert-gpu igpu is live, the contour-field pass when it is armed, then the publish —
//       `t_pub_ms` stamped BEFORE `c_seq.fetch_add` (the seq_cst store/load pair is what orders the slot's fields
//       for the F thread; the stamp after the fetch_add would be read by F before it was written).
//   run_convert_worker(ctx)
//       The --ingest-async worker thread: the same convert, fed from the RAW ring the acquirer publishes, with its
//       own DROP-TO-NEWEST rule. Already a top-level function; it moves whole.
//
// The publish order and the drop-to-newest rule are the two invariants R6 must not change (CONVERGENCE_MASTER_PLAN
// §R6); the gate measures the second through `in` / `uniq` / `arrived` per second.
// Made with my soul - Swately <3
#include <cstdint>

struct FgContext;

namespace pfg::ingest {

// The serial path's convert + publish, for capture slot `s`. `cap_rot180` is the DDA ROTATE180 correction the
// acquirer computed once (the same value run_convert_worker recomputes for itself).
void convert_and_publish(FgContext& ctx, uint32_t cap_rot180, int s);

}  // namespace pfg::ingest

// The --ingest-async worker thread body (main() spawns it; the name is kept for the thread-spawn site).
void run_convert_worker(FgContext& ctx);
