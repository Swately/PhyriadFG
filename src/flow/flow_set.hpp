#pragma once
// PhyriadFG — src/flow/flow_set.hpp : STAGE_CONTRACT §1's `FlowSet[gen]` and the `FlowRing` (R5 step 2).
//
// The FlowRing is the F→P ring of NS = kGenRing generations. It OWNS the per-pair scalars that used to be
// twelve `f_pair_*` locals of main() and the ring's two counters, and BINDS by reference the per-generation host
// bridges (the buffers) whose allocation stays with the host-bridge layer (core/app_init.hpp `HostBridgeInit`,
// core_init.cpp: they exist only when their producer is armed, aligned for EMH import). `FlowSet` is the declared
// per-generation VIEW — a plain struct of pointers and references into the ring — the type the qdump/CSV
// instruments and the FLOW rows (step 3) read; nothing crosses the 3→5 boundary by any other name.
//
// The publish discipline is UNCHANGED (it is the contract): F writes every field of generation `gen = f_seq % NS`
// BEFORE `f_seq.fetch_add(1)` (seq_cst), P reads them AFTER observing the new f_seq (the seq_cst fetch_add/load
// pair orders them; no other synchronization). P publishes `p_presenting` = the f_seq value it is presenting so F
// can detect the ring-overwrite hazard (F must not build into the generation P still holds; with kGenRing=3 it
// only fires during span >= kGenRing stalls). The loop keeps its former names as ALIASES to the ring's fields
// (main.cpp), so flow.cpp / present.cpp read and write exactly the memory they did.
//
// Field notes (moved here from main.cpp, 2026-09-06 — the reasoning is the record):
//   cseq / slot / tcap  — WHICH capture the set's pair-cur is, so P presents THE MATCHING real after the interps
//                         (interp(N-1→N) must be followed by real N, never by the newest capture — displayed
//                         content would go NON-MONOTONIC).
//   span / n            — span = how many source frames the pair covers (drops → > 1): the set's interps
//                         represent span×T of motion and P paces them over span×T (kills the walking 2× pulse);
//                         n = the interps actually generated (auto may pick < cap).
//   gme[6] / gme_valid  — the per-pair global affine model {a,b,c,d,e,f} + a validity flag (0 until the first
//                         fit: P pushes gme_on=0 for the rare unfitted generation).
//   gme_bwd[6]          — the BACKWARD (cur-anchored) model; NOT pushed to the shader (the bwd DISSIDENCE MASK
//                         carries the leading-edge information); its validity tracks the fwd model's.
//   mfwd / mbwd         — the anchored MATTE MASSES (block counts whose dissidence byte exceeds the quantized
//                         matte_thresh — the byte test mirrors the shader's classification EXACTLY); P forms the
//                         phase-t expected mass lerp(mfwd, mbwd, t) against the GPU-measured presented mass; a
//                         RATIO comparison (block space vs pixel space).
//   disp                — --motion-fallback: the per-pair gme dispersion (dis% in [0,100]); P gates the
//                         fast-motion real-fallback on it.
//   bwd_valid           — 1 when this pair recorded + fit the bwd flow; 0 when F skipped it under pressure or
//                         no bwd ran (P then pushes occl_thresh=0 AND matte_on=0 for THAT generation only).
//                         Initialized 1 so the off-bidir build behaves as before; P consults it under use_bidir.
// Made with my soul - Swately <3
#include <atomic>
#include <cstdint>
#include "flow/flow.hpp"   // kGenRing, kMaxInterp

namespace pfg::flow {

// The per-generation VIEW (the contract type). Pointers to the host bridges may be null when the producer is
// off (the validity bits and the arm mask say so, never the pointer). Scalars are references into the ring.
struct FlowSet {
    int    gen;
    void*  mv;        // MV fwd, W/8 RG16F (hostMV[gen])          — CH_MV_RAW_FWD
    void*  sad;       // SAD (sad_best, sad_zero) RG16F            — CH_SAD
    void*  mvb;       // MV bwd (hostMVB[gen])                     — CH_MV_BWD
    void*  c2;        // the runner-up candidate RGBA16F           — CH_CANDIDATES
    void*  dis;       // dissidence fwd, R8                        — CH_DISSIDENCE
    void*  disb;      // dissidence bwd, R8                        — CH_DISSIDENCE (bwd half)
    void*  gme_m;     // the gme-gpu model readback (6 floats)     — CH_GME (device variant)
    void*  gme_mb;    // the gme-gpu bwd model readback            — CH_GME_BWD (device variant)
    void*  per;       // persistence, R8                           — CH_PERSIST
    void* const* interp;   // hostI[gen][0..kMaxInterp) — the grid path's interp frames
    uint64_t& cseq; int& slot; double& tcap; uint64_t& span; int& n;
    float (&gme)[6]; int& gme_valid; float (&gme_bwd)[6];
    float& mfwd; float& mbwd; float& disp; int& bwd_valid;
};

struct FlowRing {
    static constexpr int NS = kGenRing;
    // ── the two counters of the ring contract ──
    std::atomic<uint64_t> f_seq{0};          // F publishes: fetch_add AFTER the generation's fields are written
    std::atomic<uint64_t> p_presenting{0};   // P publishes: the f_seq value it is presenting (the lap guard)
    // ── the per-pair scalars, owned (formerly main()'s f_pair_* locals) ──
    uint64_t cseq[NS]{}; int slot[NS]{}; double tcap[NS]{};
    uint64_t span[NS]{}; int n[NS]{};
    float gme[NS][6]{}; int gme_valid[NS]{};
    float gme_bwd[NS][6]{};
    float mfwd[NS]{}; float mbwd[NS]{};
    float disp[NS]{};
    int bwd_valid[NS];
    // ── the host bridges, bound by reference (allocated by the host-bridge layer when their producer is armed) ──
    void* (&mv)[NS]; void* (&sad)[NS]; void* (&mvb)[NS]; void* (&c2)[NS]; void* (&dis)[NS]; void* (&disb)[NS];
    void* (&gme_m)[NS]; void* (&gme_mb)[NS]; void* (&per)[NS];
    void* (&interp)[NS][kMaxInterp];

    FlowRing(void* (&mv_)[NS], void* (&sad_)[NS], void* (&mvb_)[NS], void* (&c2_)[NS], void* (&dis_)[NS], void* (&disb_)[NS],
             void* (&gme_m_)[NS], void* (&gme_mb_)[NS], void* (&per_)[NS], void* (&interp_)[NS][kMaxInterp])
        : mv(mv_), sad(sad_), mvb(mvb_), c2(c2_), dis(dis_), disb(disb_), gme_m(gme_m_), gme_mb(gme_mb_), per(per_), interp(interp_) {
        for (int g = 0; g < NS; ++g) bwd_valid[g] = 1;   // the off-bidir build behaves as before (see the field note)
    }
    FlowRing(const FlowRing&) = delete; FlowRing& operator=(const FlowRing&) = delete;

    static int gen_of(uint64_t seq) { return (int)(seq % (uint64_t)NS); }
    FlowSet at(int g) {
        return FlowSet{ g, mv[g], sad[g], mvb[g], c2[g], dis[g], disb[g], gme_m[g], gme_mb[g], per[g], interp[g],
                        cseq[g], slot[g], tcap[g], span[g], n[g], gme[g], gme_valid[g], gme_bwd[g], mfwd[g], mbwd[g], disp[g], bwd_valid[g] };
    }
};

}  // namespace pfg::flow
