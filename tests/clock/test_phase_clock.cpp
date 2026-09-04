// PhyriadFG — tests/clock/test_phase_clock.cpp : the STAGE-4 CLOCK's CPU test (R1 / strategy X14).
// GPU-free, Vulkan-free, deterministic. Two tests:
//
//   1. REPLAY (the bit-parity oracle, risk XR14). `phyriad_fg.exe --arrival-log FILE` records, per
//      present tick, every input the clock reads and every output it produced, in exact hex-float.
//      This test feeds those inputs to PhaseClock and asserts the outputs are BIT-identical
//      (memcmp of the doubles — not "close"): a 1-ulp shift in t_use is a different presented phase.
//      It proves the unit is PURE (its inputs are complete) and is the regression harness for R3+.
//
//   2. SYNTHETIC arrivals (the behaviour the multiplication depends on). No recording needed:
//      a 60 fps source with +-2 ms jitter, a 60->30 fps step, and a dropped frame, against a 240 Hz
//      tick. Asserts the properties the operator's objective rests on — the clock locks, the phase
//      sweeps 0->1 across each pair (that IS the frame multiplication), and content never steps back.
//      The numeric bounds are BASELINES recorded from the first run, not targets asserted a priori.
//
// Build: the `pfg_clock_test` target (CMakeLists.txt). Run: pfg_clock_test [arrival-log]
#include "clock/phase_clock.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

using namespace pfg::clock;

static int g_fail = 0;
static bool g_trace = false;
static void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL  %s\n", what); ++g_fail; }
}
static bool bit_eq(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

// ── 1. the replay ───────────────────────────────────────────────────────────────────────────────
struct Rec {
    double now_b, now_d, sc_delta_ms, T_src;
    unsigned long long cur_c, fs;
    int async_ready;
    std::vector<double>             r_tcap;
    std::vector<unsigned long long> r_cseq, r_span;
    std::vector<int>                r_n, r_slot;
    double D, t_display, content_clock, T_robust, tcap_r, phase_global, extrap, t_use;
    int f_gen, gen_back, found, N_set, rs, cand_k, backwards, backstep, committed, commit_k;
    unsigned long long pair_c, span;
};

static bool parse_cfg(const std::string& line, Cfg& k, int& NS) {
    auto grab = [&](const char* key, double& out) {
        const size_t p = line.find(std::string(key) + "=");
        if (p == std::string::npos) return false;
        out = std::strtod(line.c_str() + p + std::strlen(key) + 1, nullptr);
        return true;
    };
    double v;
    if (!grab("tick_period_ms", v)) return false; k.tick_period_ms = v;
    grab("NS", v); k.NS = NS = (int)v;
    grab("cap_slots", v);      k.cap_slots = (int)v;
    grab("fg_factor", v);      k.fg_factor = (int)v;
    grab("sync_clock", v);     k.sync_clock = v != 0;
    grab("sc_select", v);      k.sc_select = v != 0;
    grab("phasefix", v);       k.phasefix = v != 0;
    grab("low_d", v);          k.low_d = v != 0;
    grab("vblend_exact", v);   k.vblend_exact = v != 0;
    grab("predict", v);        k.predict = v != 0;
    grab("predict_e", v);      k.predict_e = v;
    grab("asw", v);            k.asw = v != 0;
    grab("asw_max", v);        k.asw_max = v;
    grab("lowd_span_frac", v); k.lowd_span_frac = v;
    grab("lowd_span_cap", v);  k.lowd_span_cap = v;
    grab("freq_alpha", v);     k.sc_freq_alpha = v;
    grab("phase_gain", v);     k.sc_phase_gain = v;
    grab("reseat_err", v);     k.sc_reseat_err = v;
    return true;
}

static int replay(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::printf("  SKIP  replay: cannot open %s\n", path); return 0; }
    Cfg k{}; int NS = 3; bool have_cfg = false;
    std::vector<Rec> recs;
    char buf[4096];
    while (std::fgets(buf, sizeof buf, f)) {
        if (buf[0] == '#') {
            std::string l(buf);
            if (l.find("tick_period_ms=") != std::string::npos) have_cfg = parse_cfg(l, k, NS);
            continue;
        }
        Rec r; r.r_tcap.resize(NS); r.r_cseq.resize(NS); r.r_span.resize(NS); r.r_n.resize(NS); r.r_slot.resize(NS);
        const char* p = buf; char* e = nullptr;
        auto d = [&]{ const double v = std::strtod(p, &e); p = e; return v; };
        auto u = [&]{ const unsigned long long v = std::strtoull(p, &e, 10); p = e; return v; };
        auto i = [&]{ const int v = (int)std::strtol(p, &e, 10); p = e; return v; };
        u();                                        // the tick index
        r.now_b = d(); r.now_d = d(); r.cur_c = u(); r.fs = u(); r.sc_delta_ms = d(); r.T_src = d(); r.async_ready = i();
        for (int g = 0; g < NS; ++g) { r.r_tcap[g] = d(); r.r_cseq[g] = u(); r.r_span[g] = u(); r.r_n[g] = i(); r.r_slot[g] = i(); }
        r.D = d(); r.t_display = d(); r.content_clock = d(); r.T_robust = d();
        r.f_gen = i(); r.gen_back = i(); r.found = i(); r.pair_c = u(); r.span = u(); r.N_set = i(); r.rs = i();
        r.tcap_r = d(); r.phase_global = d(); r.extrap = d(); r.t_use = d();
        r.cand_k = i(); r.backwards = i(); r.backstep = i(); r.committed = i(); r.commit_k = i();
        recs.push_back(std::move(r));
    }
    std::fclose(f);
    if (!have_cfg || recs.empty()) { std::printf("  SKIP  replay: no cfg header or no ticks in %s\n", path); return 0; }

    PhaseClock clk(k);
    size_t bad_D = 0, bad_disp = 0, bad_cc = 0, bad_tr = 0, bad_sel = 0, bad_phase = 0, bad_tuse = 0, bad_k = 0;
    size_t first_bad = (size_t)-1;
    for (size_t t = 0; t < recs.size(); ++t) {
        const Rec& r = recs[t];
        PairRing ring{ r.r_tcap.data(), r.r_cseq.data(), r.r_span.data(), r.r_n.data(), r.r_slot.data() };
        const AdvanceOut a = clk.advance({ r.now_b, r.T_src, r.sc_delta_ms, r.cur_c, r.fs, ring });
        const Selection  s = clk.select({ r.now_d, a.D, r.T_src, r.cur_c, a.have_interp, a.f_gen_new, ring,
                                          [](const void*, int) -> double { return 0.0; }, nullptr });
        const Order      o = clk.order(s.phase_global, s.extrap_amt, s.span_ms, s.pair_c, r.async_ready != 0);
        bool ok = true;
        if (!bit_eq(a.D, r.D))                       { ++bad_D; ok = false; }
        if (!bit_eq(s.t_display, r.t_display))       { ++bad_disp; ok = false; }
        if (!bit_eq(clk.content_clock(), r.content_clock)) { ++bad_cc; ok = false; }
        if (!bit_eq(clk.T_robust_ms(), r.T_robust))  { ++bad_tr; ok = false; }
        if (s.f_gen != r.f_gen || s.gen_back != r.gen_back || (int)s.found != r.found
            || s.pair_c != r.pair_c || s.span != r.span || s.N_set != r.N_set || s.rs != r.rs) { ++bad_sel; ok = false; }
        if (!bit_eq(s.phase_global, r.phase_global) || !bit_eq(s.extrap_amt, r.extrap)) { ++bad_phase; ok = false; }
        if (!bit_eq(o.t_use, r.t_use))               { ++bad_tuse; ok = false; }
        if (o.cand_k != r.cand_k || (int)o.backwards != r.backwards || (int)o.backstep_freeze != r.backstep) { ++bad_k; ok = false; }
        if (!ok && first_bad == (size_t)-1) first_bad = t;
        if (r.committed) clk.commit(r.pair_c, r.commit_k);   // the live run's own commit decision
    }
    std::printf("  replay: %zu ticks | mismatches D=%zu t_display=%zu content_clock=%zu T_robust=%zu selection=%zu phase=%zu t_use=%zu order=%zu\n",
                recs.size(), bad_D, bad_disp, bad_cc, bad_tr, bad_sel, bad_phase, bad_tuse, bad_k);
    if (first_bad != (size_t)-1) {
        const Rec& r = recs[first_bad];
        std::printf("  first mismatch at tick %zu: logged t_use=%a phase=%a D=%a pair_c=%llu\n",
                    first_bad, r.t_use, r.phase_global, r.D, r.pair_c);
    }
    check(bad_D + bad_disp + bad_cc + bad_tr + bad_sel + bad_phase + bad_tuse + bad_k == 0,
          "replay is bit-identical on every tick");
    return (int)recs.size();
}

// ── 2. synthetic arrivals ───────────────────────────────────────────────────────────────────────
// A source publishing pairs at T_src ms, a 240 Hz tick, and the F->P ring of NS=3 generations. This
// is the multiplication in miniature: between two real frames the clock must hand the warp a SWEEP
// of distinct phases, one per tick, monotone, and re-lock when the source rate changes.
struct Sim {
    static constexpr int NS = 3;
    double tcap[NS]{}; unsigned long long cseq[NS]{}, span[NS]{}; int n[NS]{}, slot[NS]{};
    unsigned long long fs = 0, cur_c = 0;
    // A pair is CAPTURED at tcap and becomes VISIBLE to P only pub_lag later (F's pipeline delay) —
    // that gap is exactly what the clock calibrates as D, so the sim must model it or the clock never
    // locks (found: my first version wrote tcap in the future and D stayed 0, phase pinned at 1).
    struct Pending { double tcap; unsigned long long c; double due; };
    Pending q[8]{}; int qn = 0;
    void capture(double now, unsigned long long c, double pub_lag) {
        if (qn < 8) q[qn++] = Pending{ now, c, now + pub_lag };
    }
    void pump(double now) {
        int w = 0;
        for (int i = 0; i < qn; ++i) {
            if (q[i].due <= now) {
                const int g = (int)(fs % NS);
                tcap[g] = q[i].tcap; cseq[g] = q[i].c; span[g] = 1; n[g] = 2; slot[g] = (int)(q[i].c % 28);
                ++fs;
            } else q[w++] = q[i];
        }
        qn = w;
    }
    PairRing ring() const { return PairRing{ tcap, cseq, span, n, slot }; }
};

static void synthetic() {
    Cfg k{}; k.tick_period_ms = 1000.0 / 240.0; k.NS = Sim::NS; k.cap_slots = 28; k.fg_factor = 2;
    PhaseClock clk(k);
    Sim sim;
    const double T = 1000.0 / 60.0;          // a 60 fps source
    const double pub_lag = 8.0;              // F publishes ~8 ms after capture
    double next_cap = 0.0, T_src = T;
    unsigned long long c = 0;
    double prev_phase = -1.0; unsigned long long prev_pair = 0;
    int lock_tick = -1, reseats = 0, backsteps = 0, sweeps = 0, distinct_in_pair = 0, max_distinct = 0;
    double jitter = 0.0; int traced = 0; double last_cap_now = -1.0; int pair_changes = 0;
    auto rnd = [&]{ static unsigned s = 12345u; s = s * 1664525u + 1013904223u; return (double)((s >> 8) & 0xFFFF) / 65535.0; };

    // WARM-UP, as in the real system: the capture and flow threads run during the present thread's
    // init, so P's FIRST tick already sees a published ring (the recorded live log opens at cur_c=19,
    // fs=13). Without it the clock seeds with D=0 and needs ~1.5 s to acquire — a real but different
    // scenario (it does recover), not the steady state this test is about.
    for (int w = 0; w < 10; ++w) {
        const double now = -(10 - w) * T;
        ++c; sim.cur_c = c; sim.capture(now, c, pub_lag); sim.pump(now + T);
    }
    next_cap = 0.0;

    for (int tick = 0; tick < 240 * 6; ++tick) {   // 6 seconds
        const double now = tick * k.tick_period_ms;
        const double T_now = (tick < 240 * 3) ? T : 2.0 * T;      // a 60 -> 30 fps step at t = 3 s
        double sc_delta = 0.0;
        if (now >= next_cap) {                                     // a real frame is CAPTURED
            jitter = (rnd() - 0.5) * 4.0;                          // +-2 ms of arrival jitter
            ++c; sim.cur_c = c;
            // the REALIZED inter-arrival, which is what the capture thread measures — not the intended
            // period. The tick grid quantizes arrivals, so the two differ by up to one tick; feeding the
            // intended value made the PLL learn a period the source never had (found while writing this).
            sc_delta = (last_cap_now < 0.0) ? T_now : (now - last_cap_now);
            last_cap_now = now;
            T_src = T_src * 0.9 + sc_delta * 0.1;
            if (c != 7) sim.capture(now, c, pub_lag);              // frame 7's pair is DROPPED
            next_cap = now + T_now + jitter;
        }
        sim.pump(now);                                             // F publishes what is due
        const PairRing ring = sim.ring();
        const AdvanceOut a = clk.advance({ now, T_src, sc_delta, sim.cur_c, sim.fs, ring });
        const Selection  s = clk.select({ now, a.D, T_src, sim.cur_c, a.have_interp, a.f_gen_new, ring,
                                          [](const void*, int) -> double { return 0.0; }, nullptr });
        const Order      o = clk.order(s.phase_global, s.extrap_amt, s.span_ms, s.pair_c, true);
        if (!a.have_interp) continue;
        if (g_trace && (tick < 8 || tick % 120 == 0) && traced < 24) {
            std::printf("    t=%4d cur_c=%2llu fs=%2llu D=%6.2f cc=%7.3f Tr=%5.2f gen=%d back=%d found=%d pair_c=%2llu ph=%.3f t_use=%.3f k=%d\n",
                        tick, sim.cur_c, sim.fs, a.D, clk.content_clock(), clk.T_robust_ms(),
                        s.f_gen, s.gen_back, (int)s.found, s.pair_c, s.phase_global, o.t_use, o.cand_k);
            ++traced;
        }
        if (lock_tick < 0 && s.found && s.phase_global > 0.0 && s.phase_global < 1.0) lock_tick = tick;
        if (s.pair_c == prev_pair) {
            if (o.t_use < prev_phase) ++backsteps;
            if (o.t_use != prev_phase) ++distinct_in_pair;
        } else {
            if (prev_pair) ++pair_changes;
            if (prev_pair && distinct_in_pair >= 2) ++sweeps;
            if (distinct_in_pair > max_distinct) max_distinct = distinct_in_pair;
            distinct_in_pair = 0;
        }
        prev_phase = o.t_use; prev_pair = s.pair_c;
        clk.commit(s.pair_c, o.cand_k);
    }
    std::printf("  synthetic: lock_tick=%d pairs=%d sweeps=%d max_distinct_phases_per_pair=%d backsteps=%d T_robust=%.3fms\n",
                lock_tick, pair_changes, sweeps, max_distinct, backsteps, clk.T_robust_ms());
    // The properties the operator's objective rests on. Bounds are wide on purpose: this test asserts
    // BEHAVIOUR (it locks, it sweeps, it never rewinds), not a tuning.
    check(lock_tick >= 0 && lock_tick < 240, "the clock locks within one second");
    check(backsteps == 0, "content never steps backwards inside a pair");
    check(sweeps >= 100, "every pair gets a multi-phase sweep (the frame multiplication)");
    check(max_distinct >= 3, "a 60 fps source on a 240 Hz panel yields >= 3 distinct phases per pair");
    check(clk.T_robust_ms() > 30.0 && clk.T_robust_ms() < 40.0, "the PLL tracked the 60->30 fps step (T_robust ~ 33 ms)");
}

int main(int argc, char** argv) {
    std::printf("pfg_clock_test — STAGE 4 (CLOCK), R1\n");
    std::printf("[1] replay (bit-parity oracle)\n");
    const int n = (argc > 1) ? replay(argv[1]) : (std::printf("  SKIP  no arrival-log given\n"), 0);
    g_trace = (argc > 2);
    std::printf("[2] synthetic arrivals\n");
    synthetic();
    std::printf(g_fail ? "RESULT: %d CHECK(S) FAILED\n" : "RESULT: all checks passed (%d)\n", g_fail ? g_fail : n);
    return g_fail ? 1 : 0;
}

// Made with my soul - Swately <3
