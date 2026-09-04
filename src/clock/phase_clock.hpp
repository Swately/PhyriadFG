#pragma once
// PhyriadFG — src/clock/phase_clock.hpp : STAGE 4 (CLOCK) of docs/planning/STAGE_CONTRACT.md.
//
// THE decision that makes frame MULTIPLICATION real: for every present tick, WHICH pair the generated
// frame depicts and at WHAT phase t. Everything else in the FG is sampling and compositing; this is the
// part that says "the frame you are about to make belongs at t = 0.37 of pair 812". Extract it and the
// multiplication becomes a testable function instead of 1,500 lines of loop.
//
// R1 of docs/planning/CONVERGENCE_MASTER_PLAN.md (strategy X14). The bodies are MOVED VERBATIM from
// src/present/present.cpp's OUTPUT-CLOCK loop (the four blocks named below); the only edits are the
// documented transforms listed in phase_clock.cpp's header. No constant is re-tuned, no expression is
// re-associated (risk XR14: a 1-ulp shift in t_use is a different presented phase, invisible to the eye).
//
// WHAT IS HERE (the pure clock):
//   advance() — the PLL frequency loop + NCO advance, the D calibration, the per-arrival phase lock.
//               Runs EVERY tick (the panel-unit timebase; before the decimation gate).
//   select()  — t_display, the published-set selection (content-clock or wall-time), the phase within
//               the selected window + the sync-clock phase override + the ASW overshoot.
//   order()   — the content-order key (pair, cand_k), the backwards guard, the base t_use.
//   commit()  — the monotonicity bookkeeping (what was actually presented).
//
// WHAT IS NOT HERE (deliberately — the phase LAYERS stay in present.cpp behind their flags, S5):
//   --phase-norm's even-grid ladder, the s2 realized-mult governor, --cphase's opening ease,
//   --fdrop / the over-production drop, the laser mass feedback. They READ this clock's output and
//   reshape it; they are not the clock. Nor is the arrival-delta extraction (stage 1/2 data: the
//   caller reads the capture backend's atomics and hands the outlier-rejected delta in).
#include <cstdint>

namespace pfg::clock {

// The resolved knobs the clock reads. Captured ONCE at construction (they are resolve_config output,
// constant for the run) so the per-tick inputs stay small and the value type is testable.
struct Cfg {
    double tick_period_ms = 0.0;   // 1000 / refresh_hz
    int    NS = 3;                 // the F->P generation ring size (kGenRing)
    int    cap_slots = 0;          // the capture ring depth
    int    fg_factor = 2;          // the no-interp fallback N
    bool   sync_clock = true, sc_select = true, phasefix = true, low_d = true;
    bool   vblend_exact = false, predict = false, asw = true;
    double lowd_span_frac = 0.5, lowd_span_cap = 1.5, predict_e = 0.5, asw_max = 1.0;
    double sc_freq_alpha = 0.05, sc_phase_gain = 0.10, sc_reseat_err = 4.0;
};

// The F->P publish arrays (read-only views of what FLOW published, indexed by generation).
struct PairRing {
    const double*   tcap = nullptr;   // f_pair_tcap_a
    const uint64_t* cseq = nullptr;   // f_pair_cseq_a
    const uint64_t* span = nullptr;   // f_pair_span_a
    const int*      n    = nullptr;   // f_pair_n_a
    const int*      slot = nullptr;   // f_pair_slot_a
};

// The capture ring's per-slot capture time, read ONLY on the startup (no-interp) path. A function
// pointer + context so the clock needs no Vulkan/FgContext type (the CPU test passes its own).
using RealTcapFn = double (*)(const void* ctx, int slot);

struct AdvanceIn {
    double   now_b = 0.0;         // the tick's set-detect clock read (hoisted; see the .cpp header)
    double   T_src = 0.0;         // src_interval_ema_ms
    double   sc_delta_ms = 0.0;   // the OUTLIER-REJECTED arrival delta this tick (0 = none)
    uint64_t cur_c = 0;           // c_seq snapshot
    uint64_t fs = 0;              // f_seq snapshot
    PairRing ring{};
};
struct AdvanceOut {
    double   D = 0.0;             // the calibrated lag the tick reads behind "now"
    bool     have_interp = false;
    int      f_gen_new = 0;
    uint64_t fs = 0;
};

struct SelectIn {
    double     now_d = 0.0;       // the t_display clock read
    double     D = 0.0;
    double     T_src = 0.0;
    uint64_t   cur_c = 0;
    bool       have_interp = false;
    int        f_gen_new = 0;
    PairRing   ring{};
    RealTcapFn real_tcap = nullptr;
    const void* real_ctx = nullptr;
};
struct Selection {
    double   t_display = 0.0;
    int      f_gen = 0, gen_back = 0;
    bool     found = false;
    int      N_set = 1;
    uint64_t span = 1, pair_c = 0;
    int      rs = 0;
    double   tcap_r = 0.0;
    bool     real_valid = true;
    double   span_ms = 0.0, pair_t0 = 0.0;
    double   phase_global = 0.0, extrap_amt = 0.0;
};

struct Order {
    double t_use = 0.0;
    int    cand_k = 0;
    bool   backwards = false;
    bool   backstep_freeze = false;
};

class PhaseClock {
public:
    explicit PhaseClock(const Cfg& k) : k_(k) {}

    AdvanceOut advance(const AdvanceIn& in);
    Selection  select(const SelectIn& in);
    Order      order(double phase_global, double extrap_amt, double span_ms, uint64_t pair_c,
                     bool async_front_ready);
    // What was actually presented (the monotonicity anchor the guards read next tick).
    void commit(uint64_t pair_c, int cand_k) { last_pres_cseq = pair_c; last_pres_k = cand_k; have_last_pres = true; }

    // Reads for the layers + the instruments (the phase layers need T_robust; the log needs both).
    bool     has_last()      const { return have_last_pres; }
    bool     delay_init_ok() const { return delay_init; }
    double   freshage_ema()  const { return freshage_ema_ms; }
    uint64_t last_cseq()     const { return last_pres_cseq; }
    int      last_k()        const { return last_pres_k; }
    double   content_clock() const { return content_clock_; }
    double   T_robust_ms()   const { return T_robust_ms_; }
    const Cfg& cfg()         const { return k_; }

private:
    Cfg k_;
    // ── the state, verbatim from the OUTPUT-CLOCK loop's P-locals ──────────────────────────────────
    double   delay_ema_ms = 0.0;      bool delay_init = false;
    double   freshage_ema_ms = 0.0;
    uint64_t cur_pair_seq = 0;
    uint64_t last_pres_cseq = 0;      int  last_pres_k = -1;   bool have_last_pres = false;
    double   last_disp_t = 0.0;       bool disp_init = false;
    double   content_clock_ = 0.0;    // the NCO accumulator (SOURCE-FRAME units)
    double   T_robust_ms_ = 0.0;      // the PLL frequency estimate (ms/source-frame)
    bool     sc_init = false;
    uint64_t sc_last_c = 0;
};

}  // namespace pfg::clock

// Made with my soul - Swately <3
