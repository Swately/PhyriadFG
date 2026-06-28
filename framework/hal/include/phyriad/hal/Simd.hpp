// framework/hal/include/phyriad/hal/Simd.hpp
// Detección de capacidades SIMD runtime y kernels de copia de tamaño fijo.
//
// Este header provee:
//   1. SimdCaps — bitfield de features detectados en runtime.
//   2. detect_simd_caps() — detección completa (llamar una vez al init).
//   3. simd_caps() — singleton cacheado de SimdCaps.
//   4. slot_copy_fixed64()  — copia inline de exactamente 64 B (1 slot compact).
//   5. payload_copy_fixed32() — copia inline de exactamente 32 B (1 payload).
//
// NOTA: pick_slot_copy() y slot_copy_fn pertenecen al pilar transport
// (pillars/transport/include/phyriad/transport/SlotCopy.hpp) porque dependen
// de PerformanceProfile y del dispatch table de transport. No están aquí.
//
// Uso:
//   const auto& caps = hal::simd_caps();
//   if (caps.avx2) { /* use AVX2 path */ }
//   hal::slot_copy_fixed64(dst, src);   // 1 slot = 64 B, inlineado
//

#pragma once
#include "Arch.hpp"
#include "Cacheline.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

// x86 SIMD intrinsics (AVX2 copy kernels + the FR-HAL-1 F16C batch decode). Header is
// safe to include without -mavx2/-mf16c; the intrinsic USES are guarded on __AVX2__/__F16C__.
#if PHYRIAD_ARCH_X86_64
#  include <immintrin.h>
#endif

// Linux aarch64 runtime feature detection via getauxval(AT_HWCAP).
// Wrapped in an arch guard so x86_64 builds don't pull in <sys/auxv.h>.
#if defined(__aarch64__) && defined(__linux__) && !defined(__APPLE__)
#  include <sys/auxv.h>
#  ifndef HWCAP_SVE
#    define HWCAP_SVE   (1UL << 22)   // glibc ≥ 2.28; fall back to ABI value
#  endif
#  ifndef HWCAP_CRC32
#    define HWCAP_CRC32 (1UL << 7)
#  endif
#endif

namespace phyriad::hal {

#if PHYRIAD_ARCH_X86_64
// Read XCR0 (extended control register 0) via XGETBV. The caller MUST first confirm
// OSXSAVE (CPUID.1:ECX[27]) — XGETBV #UDs otherwise. XCR0[2:1]==0b11 means the OS has
// XSAVE-enabled the SSE+AVX (YMM) state; without it a VEX-encoded op (AVX2 / F16C)
// #UDs REGARDLESS of the CPUID feature bit — the textbook AVX-detection requirement
// (Intel SDM Vol.1 §14.3 / §15.2). XCR0[7:5]==0b111 additionally enables AVX-512 state.
[[nodiscard]] inline uint64_t read_xcr0() noexcept {
#  if PHYRIAD_COMPILER_MSVC
    return _xgetbv(0);
#  else
    uint32_t lo = 0u, hi = 0u;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0u));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#  endif
}
#endif

// ── Capacidades SIMD detectadas en runtime ────────────────────────────────────
struct SimdCaps {
    bool avx2    : 1;   // Intel Haswell+ / AMD Zen
    bool avx512f : 1;   // Intel Skylake-X+ / AMD Zen4
    bool neon    : 1;   // ARM NEON (obligatorio en aarch64)
    bool sve     : 1;   // ARM SVE (Graviton 3+, no Apple)
    bool has_crc : 1;   // SSE4.2 CRC32C (x86) / ARM CRC32 extension
    bool f16c    : 1;   // FR-HAL-1: x86 half-precision convert (vcvtph2ps/vcvtps2ph); Ivy Bridge+/Zen
};

[[nodiscard]] inline SimdCaps detect_simd_caps() noexcept {
    SimdCaps caps{};

#if PHYRIAD_ARCH_X86_64
    // CPUID leaf 1 (ECX) → OSXSAVE, SSE4.2, F16C; leaf 7 (EBX) → AVX2, AVX-512F.
    uint32_t l1_ecx = 0u, l7_ebx = 0u;
#  if PHYRIAD_COMPILER_MSVC
    int info1[4]{}; __cpuidex(info1, 1, 0); l1_ecx = static_cast<uint32_t>(info1[2]);
    int info7[4]{}; __cpuidex(info7, 7, 0); l7_ebx = static_cast<uint32_t>(info7[1]);
#  else
    uint32_t a{}, b{}, c{}, d{};
    __cpuid_count(1, 0, a, b, c, d); l1_ecx = c;
    __cpuid_count(7, 0, a, b, c, d); l7_ebx = b;
#  endif
    // OS-enablement gate (Intel SDM Vol.1 §14.3): a VEX-encoded op #UDs unless the OS
    // has XSAVE-enabled the relevant state, regardless of the CPUID feature bit. Gate
    // every VEX-requiring feature (AVX2 / F16C / AVX-512) on OSXSAVE + XCR0; SSE4.2
    // CRC32 is legacy-encoded → no gate. Cold one-time init → zero hot-path cost.
    const bool     osxsave   = (l1_ecx & (1u << 27)) != 0u;
    const uint64_t xcr0      = osxsave ? read_xcr0() : 0ull;
    const bool     avx_os    = osxsave && ((xcr0 & 0x6ull)  == 0x6ull);    // XMM + YMM state
    const bool     avx512_os = avx_os  && ((xcr0 & 0xE0ull) == 0xE0ull);   // + opmask/ZMM state

    caps.has_crc = (l1_ecx & (1u << 20)) != 0u;                      // SSE4.2 CRC32 (no VEX → ungated)
    caps.avx2    = ((l7_ebx & (1u << 5))  != 0u) && avx_os;          // EBX[5]  = AVX2
    caps.avx512f = ((l7_ebx & (1u << 16)) != 0u) && avx512_os;       // EBX[16] = AVX-512F
    caps.f16c    = ((l1_ecx & (1u << 29)) != 0u) && avx_os;          // ECX[29] = F16C (FR-HAL-1)

#elif PHYRIAD_ARCH_AARCH64
    caps.neon = true;   // NEON es obligatorio en aarch64 (parte del ISA base).
#  if PHYRIAD_ARCH_APPLE
    // Apple Silicon (M1+): SVE not supported on any current part; CRC32 always on.
    caps.sve     = false;
    caps.has_crc = true;
#  elif defined(__linux__)
    // Linux aarch64 (Graviton, Ampere Altra, Cortex-A78, RPi5, etc.):
    // probe via the kernel-published hardware capability bitmap.
    const unsigned long hwcap = getauxval(AT_HWCAP);
    caps.sve     = (hwcap & HWCAP_SVE)   != 0ul;
    caps.has_crc = (hwcap & HWCAP_CRC32) != 0ul;
#  else
    // Windows-on-ARM and other aarch64 OSes: conservative — opt-in only.
    // (IsProcessorFeaturePresent has no SVE flag as of Windows 11 24H2.)
    caps.sve     = false;
    caps.has_crc = false;
#  endif
#endif

    return caps;
}

// Singleton cacheado — evaluado una sola vez al primer acceso.
[[nodiscard]] inline const SimdCaps& simd_caps() noexcept {
    static const SimdCaps caps = detect_simd_caps();
    return caps;
}

// ── Copia de tamaño fijo: 64 B (1 slot compact) ──────────────────────────────
// Inlineado en el caller: cero overhead de dispatch.
// Compile-time dispatch según capabilities del translation unit.
// Para dispatch dinámico, usar pillars/transport/SlotCopy.hpp.
PHYRIAD_ALWAYS_INLINE void slot_copy_fixed64(void* dst, const void* src) noexcept {
#if PHYRIAD_ARCH_X86_64 && defined(__AVX2__)
    // 2 × vmovdqu de 32 B = 64 B en una sola pasada.
    const __m256i lo = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(src));
    const __m256i hi = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(reinterpret_cast<const char*>(src) + 32));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), lo);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(
        reinterpret_cast<char*>(dst) + 32), hi);
    _mm256_zeroupper();
#elif PHYRIAD_ARCH_AARCH64
    // 4 × vld1q_u8 (4 × 16 B = 64 B).
    const uint8_t* s = static_cast<const uint8_t*>(src);
    uint8_t*       d = static_cast<      uint8_t*>(dst);
    const uint8x16_t v0 = vld1q_u8(s);
    const uint8x16_t v1 = vld1q_u8(s + 16);
    const uint8x16_t v2 = vld1q_u8(s + 32);
    const uint8x16_t v3 = vld1q_u8(s + 48);
    vst1q_u8(d,      v0);
    vst1q_u8(d + 16, v1);
    vst1q_u8(d + 32, v2);
    vst1q_u8(d + 48, v3);
#else
    std::memcpy(dst, src, 64);
#endif
}

// ── Copia de tamaño fijo: 32 B (1 payload compact) ───────────────────────────
// 1 instrucción SIMD: ymm (AVX2) o 2×q (NEON).
PHYRIAD_ALWAYS_INLINE void payload_copy_fixed32(void* dst, const void* src) noexcept {
#if PHYRIAD_ARCH_X86_64 && defined(__AVX2__)
    _mm256_storeu_si256(
        static_cast<__m256i*>(dst),
        _mm256_loadu_si256(static_cast<const __m256i*>(src)));
    _mm256_zeroupper();
#elif PHYRIAD_ARCH_AARCH64
    const uint8_t* s = static_cast<const uint8_t*>(src);
    uint8_t*       d = static_cast<      uint8_t*>(dst);
    const uint8x16_t v0 = vld1q_u8(s);
    const uint8x16_t v1 = vld1q_u8(s + 16);
    vst1q_u8(d,      v0);
    vst1q_u8(d + 16, v1);
#else
    std::memcpy(dst, src, 32);
#endif
}

// ── FR-HAL-1: half-precision (binary16) ⇄ float, scalar + F16C-batch ──────────
// The canonical fp16 codec for the substrate. Consumers (render_assistant's optical-flow MV
// grids, any fp16 sensor/telemetry stream) were hand-rolling this per-element bit-math; it now
// lives here once. f16_to_f32 / f32_to_f16 are PORTABLE bit math (no -mf16c needed, no UB —
// pure integer reinterpretation per the binary16 spec, full subnormal + Inf/NaN handling).
// f16_to_f32_batch is the perf primitive: F16C 8-wide (vcvtph2ps) when the running CPU has it
// AND the calling TU was compiled with -mf16c/-march (so the intrinsic is emitted), else the
// scalar path — runtime-dispatched on simd_caps().f16c, BYTE-IDENTICAL to the scalar result.
// Ease-of-consumption note: to get the F16C path, compile the ONE TU that calls this batch with
// the SIMD flag (set_source_files_properties(... "/arch:AVX2"|"-mavx2 -mf16c")) — the same
// pattern transport/SlotCopy.cpp uses; the rest of the consumer needs no flags.
[[nodiscard]] PHYRIAD_ALWAYS_INLINE float f16_to_f32(uint16_t h) noexcept {
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    const uint32_t exp  = (h >> 10) & 0x1Fu;
    const uint32_t mant = h & 0x3FFu;
    uint32_t bits;
    if (exp == 0u) {
        if (mant == 0u) { bits = sign; }                 // ±0
        else {                                            // subnormal → normalise
            uint32_t m = mant; int e = -1;
            do { m <<= 1; ++e; } while ((m & 0x400u) == 0u);
            m &= 0x3FFu;
            bits = sign | (uint32_t)((127 - 15 - e) << 23) | (m << 13);
        }
    } else if (exp == 0x1Fu) {                            // Inf / NaN
        bits = sign | 0x7F800000u | (mant << 13);
    } else {                                              // normal
        bits = sign | (uint32_t)((int)exp - 15 + 127) << 23 | (mant << 13);
    }
    float out; std::memcpy(&out, &bits, sizeof(out)); return out;
}

[[nodiscard]] PHYRIAD_ALWAYS_INLINE uint16_t f32_to_f16(float f) noexcept {
    uint32_t x; std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    int32_t  exp  = (int32_t)((x >> 23) & 0xFFu) - 127 + 15;   // rebias 127 → 15
    uint32_t mant = x & 0x7FFFFFu;
    if (((x >> 23) & 0xFFu) == 0xFFu) return (uint16_t)(sign | 0x7C00u | (mant ? 0x200u : 0u));  // Inf/NaN
    if (exp >= 0x1F) return (uint16_t)(sign | 0x7C00u);                                          // overflow → Inf
    if (exp <= 0) {                                            // subnormal / underflow
        if (exp < -10) return (uint16_t)sign;
        mant |= 0x800000u;
        const int shift = 14 - exp;
        const uint32_t halfmant = mant >> shift;
        const uint32_t rem = mant & ((1u << shift) - 1u);
        const uint32_t halfway = 1u << (shift - 1);
        uint32_t r = halfmant;
        if (rem > halfway || (rem == halfway && (halfmant & 1u))) ++r;   // round half to even
        return (uint16_t)(sign | r);
    }
    const uint32_t halfmant = mant >> 13;                     // normal: round 23→10 bits, half-to-even
    const uint32_t rem = mant & 0x1FFFu;
    uint32_t out = (uint32_t)(exp << 10) | halfmant;
    if (rem > 0x1000u || (rem == 0x1000u && (halfmant & 1u))) ++out;
    return (uint16_t)(sign | out);
}

// Batch decode n binary16 → n float. Runtime-dispatched: F16C 8-wide when available (and the TU
// has -mf16c), scalar otherwise; the two paths are bit-identical (F16C vcvtph2ps is IEEE-correct,
// matching the scalar bit-math on every normal/subnormal/Inf/NaN — verified 0/64800 mismatches in
// apps/render_assistant/bench/gme_fit_bench). dst/src may not alias.
PHYRIAD_ALWAYS_INLINE void f16_to_f32_batch(const uint16_t* src, float* dst, std::size_t n) noexcept {
    std::size_t i = 0;
#if PHYRIAD_ARCH_X86_64 && defined(__F16C__)
    if (simd_caps().f16c) {
        for (; i + 8 <= n; i += 8) {
            const __m128i h8 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
            _mm256_storeu_ps(dst + i, _mm256_cvtph_ps(h8));   // 8 halves → 8 floats, IEEE-correct
        }
        _mm256_zeroupper();
    }
#endif
    for (; i < n; ++i) dst[i] = f16_to_f32(src[i]);           // scalar tail / full fallback
}

} // namespace phyriad::hal
// Made with my soul - Swately <3
