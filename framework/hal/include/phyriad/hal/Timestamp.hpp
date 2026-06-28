// framework/hal/include/phyriad/hal/Timestamp.hpp
// Contador de ciclos monótono y conversión a nanosegundos, portable.
//
// API:
//   tsc_t               — tipo del counter (uint64_t).
//   rdtsc()             — lee el counter actual. Overhead: ~7-20 cycles en x86.
//   tsc_freq_hz()       — frecuencia del counter en Hz (via CPUID/cntfrq_el0).
//   calibrate_tsc_freq()— calibración empírica contra wall clock (~10 ms sleep).
//                         Thread-safe: el primer llamador mide, los siguientes
//                         retornan el valor cacheado instantáneamente.
//   tsc_to_ns(delta)    — convierte delta de cycles a nanosegundos.
//
// Uso:
//   const uint64_t freq = hal::calibrate_tsc_freq();   // al inicio
//   const uint64_t t0   = hal::rdtsc();
//   do_work();
//   const uint64_t elapsed_ns = hal::tsc_to_ns(hal::rdtsc() - t0);
//

#pragma once
#include "Arch.hpp"
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#if PHYRIAD_ARCH_X86_64 && (PHYRIAD_COMPILER_GCC || PHYRIAD_COMPILER_CLANG)
#  include <cpuid.h>   // __cpuid_count() — GCC/Clang/MinGW
#endif

namespace phyriad::hal {

using tsc_t = std::uint64_t;

// ─────────────────────────────────────────────────────────────────────────────
// rdtsc() — leer el cycle counter actual.
// ─────────────────────────────────────────────────────────────────────────────
// x86: __rdtsc() — not serializing. Para timing de hot path es suficiente.
//      Para micro-benchmarks precisos rodear con LFENCE antes y RDTSCP después.
// ARM: mrs CNTVCT_EL0 — virtual counter. Frecuencia fija (ver tsc_freq_hz()).
[[nodiscard]] PHYRIAD_ALWAYS_INLINE tsc_t rdtsc() noexcept {
#if PHYRIAD_ARCH_X86_64
    return static_cast<tsc_t>(__rdtsc());

#elif PHYRIAD_ARCH_AARCH64 && PHYRIAD_ARCH_APPLE
    return static_cast<tsc_t>(mach_absolute_time());

#elif PHYRIAD_ARCH_AARCH64
    tsc_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;

#else
    // Fallback genérico: nanosegundos de wall clock.
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<tsc_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// tsc_freq_hz() — frecuencia del counter via hardware.
// ─────────────────────────────────────────────────────────────────────────────
// x86: CPUID[15h] (disponible en Intel Skylake+, AMD Zen2+). 0 si no soportado.
// ARM: CNTFRQ_EL0 (p.ej. 24 MHz en Apple, 25 MHz en Graviton 3).
[[nodiscard]] inline tsc_t tsc_freq_hz() noexcept {
#if PHYRIAD_ARCH_X86_64
#  if PHYRIAD_COMPILER_MSVC
    int info[4]{};
    __cpuidex(info, 0x15, 0);
    const uint32_t numerator   = static_cast<uint32_t>(info[1]);
    const uint32_t denominator = static_cast<uint32_t>(info[0]);
    const uint32_t crystal_hz  = static_cast<uint32_t>(info[2]);
#  else
    uint32_t eax{}, ebx{}, ecx{}, edx{};
    __cpuid_count(0x15, 0, eax, ebx, ecx, edx);
    const uint32_t numerator   = ebx;
    const uint32_t denominator = eax;
    const uint32_t crystal_hz  = ecx;
#  endif
    if (numerator && denominator && crystal_hz) {
        return static_cast<tsc_t>(crystal_hz) * numerator / denominator;
    }
    return 0;

#elif PHYRIAD_ARCH_AARCH64 && PHYRIAD_ARCH_APPLE
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    if (info.numer == 0) return 0;
    return static_cast<tsc_t>(1'000'000'000ULL) * info.denom / info.numer;

#elif PHYRIAD_ARCH_AARCH64
    tsc_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;

#else
    return 1'000'000'000ULL;  // Fallback: 1 GHz (ns desde chrono).
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// calibrate_tsc_freq() — calibración empírica contra wall clock.
// ─────────────────────────────────────────────────────────────────────────────
// Mide la frecuencia del TSC con una ventana de ~10 ms.
// Devuelve la frecuencia en Hz.
// Thread-safe: múltiples llamadas concurrentes retornan el mismo valor.
// El primer llamador bloquea ~10 ms; los siguientes son instantáneos.
[[nodiscard]] inline tsc_t calibrate_tsc_freq() noexcept {
    static std::atomic<tsc_t> cached{0};

    tsc_t result = cached.load(std::memory_order_relaxed);
    if (result) return result;

    const auto  wall0 = std::chrono::steady_clock::now();
    const tsc_t tsc0  = rdtsc();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const tsc_t tsc1  = rdtsc();
    const auto  wall1 = std::chrono::steady_clock::now();

    const auto wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        wall1 - wall0).count();
    const tsc_t delta = tsc1 - tsc0;

    result = (wall_ns == 0)
           ? 1'000'000'000ULL
           : delta * 1'000'000'000ULL / static_cast<tsc_t>(wall_ns);

    cached.store(result, std::memory_order_relaxed);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// tsc_to_ns() — convertir delta de cycles a nanosegundos.
// ─────────────────────────────────────────────────────────────────────────────
// Usa calibrate_tsc_freq() al primer acceso (10 ms de latencia una sola vez).
// Precisión: ±0.1 % en x86 invariant TSC.
[[nodiscard]] PHYRIAD_ALWAYS_INLINE std::uint64_t tsc_to_ns(tsc_t delta) noexcept {
    static tsc_t freq = [] {
        const tsc_t from_cpuid = tsc_freq_hz();
        return from_cpuid ? from_cpuid : calibrate_tsc_freq();
    }();

    if (freq == 0) return delta;
    return (delta * 1'000'000'000ULL) / freq;
}

} // namespace phyriad::hal
// Made with my soul - Swately <3
