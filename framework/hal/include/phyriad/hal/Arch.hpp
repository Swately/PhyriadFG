// framework/hal/include/phyriad/hal/Arch.hpp
// Detección de arquitectura y compilador — fundación de toda la HAL.
//
// Define macros PHYRIAD_ARCH_* y PHYRIAD_COMPILER_* que todos los demás
// headers de phyriad/hal/ incluyen. NO incluir directamente fuera de hal/.
//
// Macros definidos:
//   PHYRIAD_ARCH_X86_64       — 1 si x86-64 (AMD64 / Intel 64)
//   PHYRIAD_ARCH_AARCH64      — 1 si ARM64 (aarch64 / Apple Silicon / Graviton)
//   PHYRIAD_ARCH_APPLE        — 1 si Apple Silicon (subset de AARCH64)
//   PHYRIAD_COMPILER_GCC      — 1 si GCC (incluye MinGW)
//   PHYRIAD_COMPILER_CLANG    — 1 si Clang / Apple Clang
//   PHYRIAD_COMPILER_MSVC     — 1 si MSVC
//
// Atributos de inlining portables:
//   PHYRIAD_ALWAYS_INLINE     — fuerza inlining (nunca omitir en hot path)
//   PHYRIAD_FLATTEN           — inlinea recursivamente las llamadas internas
//
// Garantía: exactamente uno de PHYRIAD_ARCH_X86_64 / PHYRIAD_ARCH_AARCH64
// es 1 en las plataformas soportadas. En plataformas desconocidas ambos
// son 0 y el código cae al path genérico (std:: puro).
//

#pragma once

// ── Detección de arquitectura ─────────────────────────────────────────────────
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
#  define PHYRIAD_ARCH_X86_64   1
#  define PHYRIAD_ARCH_AARCH64  0
#  define PHYRIAD_ARCH_APPLE    0
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define PHYRIAD_ARCH_X86_64   0
#  define PHYRIAD_ARCH_AARCH64  1
#  if defined(__APPLE__)
#    define PHYRIAD_ARCH_APPLE  1
#  else
#    define PHYRIAD_ARCH_APPLE  0
#  endif
#else
// Arquitectura desconocida — stubs genéricos en todos los demás headers.
#  define PHYRIAD_ARCH_X86_64   0
#  define PHYRIAD_ARCH_AARCH64  0
#  define PHYRIAD_ARCH_APPLE    0
#endif

// ── Detección de compilador ───────────────────────────────────────────────────
#if defined(_MSC_VER) && !defined(__clang__)
#  define PHYRIAD_COMPILER_MSVC  1
#  define PHYRIAD_COMPILER_GCC   0
#  define PHYRIAD_COMPILER_CLANG 0
#elif defined(__clang__)
#  define PHYRIAD_COMPILER_MSVC  0
#  define PHYRIAD_COMPILER_GCC   0
#  define PHYRIAD_COMPILER_CLANG 1
#elif defined(__GNUC__)
#  define PHYRIAD_COMPILER_MSVC  0
#  define PHYRIAD_COMPILER_GCC   1
#  define PHYRIAD_COMPILER_CLANG 0
#else
#  define PHYRIAD_COMPILER_MSVC  0
#  define PHYRIAD_COMPILER_GCC   0
#  define PHYRIAD_COMPILER_CLANG 0
#endif

// ── Atributos de inlining ─────────────────────────────────────────────────────
// PHYRIAD_ALWAYS_INLINE: el compilador DEBE inlinear esta función.
// En release, toda la HAL se colapsa a instrucciones directas.
#if PHYRIAD_COMPILER_MSVC
#  define PHYRIAD_ALWAYS_INLINE __forceinline
#  define PHYRIAD_FLATTEN       /* MSVC no tiene flatten; usar PGO */
#elif PHYRIAD_COMPILER_GCC || PHYRIAD_COMPILER_CLANG
#  define PHYRIAD_ALWAYS_INLINE __attribute__((always_inline)) inline
#  define PHYRIAD_FLATTEN       __attribute__((flatten))
#else
#  define PHYRIAD_ALWAYS_INLINE inline
#  define PHYRIAD_FLATTEN
#endif

// ── Atributos de calor / frío de funciones ────────────────────────────────────
// PHYRIAD_HOT: la función está en el steady-state hot path. GCC/Clang la
// optimizan más agresivamente y el linker la coloca en .text.hot para que
// las funciones calientes se agrupen en icache.
// PHYRIAD_COLD: la función es rara (errores, shutdown, init). Va a .text.cold,
// alejada del hot path, dejando icache más limpia para lo que sí importa.
//
// MSVC no tiene equivalentes nativos; usa PGO para la misma clasificación
// automáticamente (ver docs/PGO.md cuando esté).
//
// Performance reinforcement / icache layout.
#if PHYRIAD_COMPILER_GCC || PHYRIAD_COMPILER_CLANG
#  define PHYRIAD_HOT  __attribute__((hot))
#  define PHYRIAD_COLD __attribute__((cold))
#else
#  define PHYRIAD_HOT
#  define PHYRIAD_COLD
#endif

// ── Intrinsics x86: inclusión centralizada ────────────────────────────────────
// Todos los headers de hal/ que necesiten intrinsics x86 incluyen Arch.hpp
// en lugar de incluir <immintrin.h> o <intrin.h> directamente.
#if PHYRIAD_ARCH_X86_64
#  if PHYRIAD_COMPILER_MSVC
#    include <intrin.h>      // MSVC: __rdtsc, _mm_pause, _mm_sfence, CPUID
#  else
#    include <immintrin.h>   // GCC/Clang MinGW: __rdtsc, _mm_pause, AVX*
#    include <x86intrin.h>   // GCC/Clang: __builtin_ia32_pause, extras
#    include <cpuid.h>       // GCC/Clang/MinGW: __cpuid_count (CPUID leaf intrinsic)
#  endif
#endif

// ── Intrinsics aarch64: inclusión centralizada ────────────────────────────────
#if PHYRIAD_ARCH_AARCH64
#  include <arm_neon.h>      // NEON: uint8x16_t, vld1q_u8, etc.
#  if PHYRIAD_ARCH_APPLE
#    include <mach/mach_time.h>  // mach_absolute_time() fallback Apple
#  endif
#endif
// Made with my soul - Swately <3
