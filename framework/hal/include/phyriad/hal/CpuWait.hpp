// framework/hal/include/phyriad/hal/CpuWait.hpp
// Low-power CPU wait primitives with runtime CPUID dispatch.
//
// Two primitives:
//   - spin_hint()    — already in MemoryOrder.hpp; single PAUSE/YIELD instruction
//   - cpu_wait_until(deadline_tsc) — TPAUSE-based wait (Intel Sapphire Rapids+
//                      and Alder Lake P-cores). Releases SMT siblings and
//                      lowers power until deadline OR memory event OR interrupt.
//
// CPUID detection (leaf 7 sub-leaf 0, ECX bit 5 = WAITPKG) at first call.
// Fallback to PAUSE loop when WAITPKG unavailable: keeps the wait semantics
// (sleep ~N µs) without changing observable behavior, just at higher power.
//
// Performance envelope:
//   - TPAUSE wakeup: < 100 ns from "monitor wake event" to thread resume
//   - PAUSE loop wakeup: ~5-20 ns (no kernel transition) but burns 100% CPU
//   - std::this_thread::sleep_for: > 1 µs (syscall + scheduler)
//
// TPAUSE is the sweet spot: shorter than sleep_for, more power-efficient than
// PAUSE loop. Worth using when wait duration ≥ 1 µs.
//
// Use case:
//   Ring consumer that observes empty ring → cpu_wait_until(rdtsc() + N_us)
//   The producer's next send will be visible after wake.
#pragma once
#include <phyriad/hal/Arch.hpp>
#include <phyriad/hal/MemoryOrder.hpp>
#include <phyriad/hal/Timestamp.hpp>
#include <atomic>
#include <cstdint>

#if defined(PHYRIAD_ARCH_X86_64) || defined(_M_X64) || defined(__x86_64__)
#  if defined(_MSC_VER) && !defined(__clang__)
#    include <intrin.h>
#  else
#    include <immintrin.h>
#    include <cpuid.h>
#  endif
#endif

namespace phyriad::hal {

// ── CPUID feature detection (cached) ─────────────────────────────────────────
namespace cpu_wait_detail {

[[nodiscard]] inline bool has_waitpkg() noexcept {
#if defined(PHYRIAD_ARCH_X86_64) || defined(_M_X64) || defined(__x86_64__)
    static const bool cached = []() noexcept -> bool {
        uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
#  if defined(_MSC_VER) && !defined(__clang__)
        int regs[4]{};
        __cpuidex(regs, 7, 0);
        eax = static_cast<uint32_t>(regs[0]);
        ebx = static_cast<uint32_t>(regs[1]);
        ecx = static_cast<uint32_t>(regs[2]);
        edx = static_cast<uint32_t>(regs[3]);
#  else
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
#  endif
        (void)eax; (void)ebx; (void)edx;
        // CPUID.07h:ECX bit 5 = WAITPKG (UMONITOR, UMWAIT, TPAUSE).
        return (ecx & (1u << 5)) != 0u;
    }();
    return cached;
#else
    return false;
#endif
}

} // namespace cpu_wait_detail

// ── waitpkg_available — public capability query ──────────────────────────────
// Use to log at startup or to decide between cpu_wait_until() vs an alternate
// strategy. Cheap after first call (cached).
[[nodiscard]] inline bool waitpkg_available() noexcept {
    return cpu_wait_detail::has_waitpkg();
}

// ── cpu_wait_until(deadline_tsc) ─────────────────────────────────────────────
// Pause the thread until the TSC reaches `deadline_tsc`. On WAITPKG hardware,
// uses TPAUSE which:
//   - Lowers power consumption while waiting
//   - Releases SMT sibling resources (other hyperthread can run)
//   - Wakes < 100 ns on TSC deadline OR on a monitored memory event
//
// On non-WAITPKG hardware, falls back to a PAUSE loop bounded by the deadline.
//
// Power-state selection: state=0 (C0.1, fastest wake, ~100 ns wake latency).
// state=1 (C0.2) saves more power but wakes slower (~500 ns) — not used here.
[[gnu::always_inline]] inline void cpu_wait_until(uint64_t deadline_tsc) noexcept {
#if defined(PHYRIAD_ARCH_X86_64) || defined(_M_X64) || defined(__x86_64__)
    if (cpu_wait_detail::has_waitpkg()) {
#  if defined(__WAITPKG__) || defined(_MSC_VER)
        // _tpause is the intrinsic. State 0 = C0.1 (faster wake).
        // The function takes (state, deadline_tsc). Returns: CF=0 if deadline
        // expired; CF=1 if interrupted by monitor wake. We ignore the result.
        // _tpause(control, counter): TPAUSE with the 64-bit deadline in EDX:EAX and the
        // C-state hint in the control register — the vendor intrinsic places the registers
        // correctly. Reachable only where the intrinsic is declared (MSVC always; GCC/Clang
        // under __WAITPKG__, per the outer guard). state 0 = C0.1 (fast wake).
        //
        // NOTE: the previous GCC/Clang path hand-rolled `.byte … ; tpause eax` and passed
        // the deadline's low 32 bits in ECX — but TPAUSE reads the deadline from EDX:EAX and
        // never reads ECX, so the low word was dropped and the wait retired immediately on
        // WAITPKG silicon. Replaced by the vendor intrinsic (correct EDX:EAX). Dormant on
        // non-WAITPKG CPUs (e.g. AMD Zen → PAUSE fallback). See CPU_SUBSTRATE_PRIOR_ART.md
        // Appendix A.2.
        _tpause(0u, deadline_tsc);
        return;
#  else
        // No intrinsic available at compile time even though CPU supports it.
        // Fall through to PAUSE loop.
#  endif
    }
#endif
    // ── Fallback: bounded PAUSE loop ─────────────────────────────────────────
    // Spin until TSC reaches the deadline; PAUSE keeps SMT pressure low and
    // reduces power vs a tight loop.
    while (rdtsc() < deadline_tsc) {
        spin_hint();
    }
}

// ── cpu_wait_for_ns(ns) ──────────────────────────────────────────────────────
// Convenience: wait for approximately N nanoseconds via TSC arithmetic.
// Uses calibrated TSC frequency; the deadline calculation is one multiply.
[[gnu::always_inline]] inline void cpu_wait_for_ns(uint64_t ns) noexcept {
    if (ns == 0u) return;
    const uint64_t freq = calibrate_tsc_freq();
    if (freq == 0u) [[unlikely]] {
        // TSC calibration failed — fall back to a fixed-count PAUSE loop.
        for (uint64_t i = 0; i < ns; ++i) spin_hint();
        return;
    }
    const uint64_t cycles = (ns * freq) / 1'000'000'000ull;
    cpu_wait_until(rdtsc() + cycles);
}

} // namespace phyriad::hal
// Made with my soul - Swately <3
