// framework/hal/include/phyriad/hal/Cacheline.hpp
// Constantes de cacheline portables para alineación correcta por arquitectura.
//
// Dos constantes:
//   kCachelineSize  — unidad arquitectónica de coherency (CLFLUSH line).
//                     Usar para alinear datos en acceso secuencial.
//   kDestructivePad — unidad efectiva de false-sharing considerando prefetchers
//                     espaciales. Usar en atomics con contención cross-core.
//                     = max(2 × kCachelineSize, 128) para cubrir prefetchers
//                     de Intel/AMD que agrupan dos líneas contiguas.
//
// Uso:
//   struct alignas(hal::kDestructivePad) HotProducer {
//       std::atomic<uint32_t> write_idx{0};
//   };
//   static_assert(sizeof(HotProducer) % hal::kDestructivePad == 0);
//

#pragma once
#include "Arch.hpp"
#include <cstddef>

namespace phyriad::hal {

// ── Tamaño de línea de caché por arquitectura ─────────────────────────────────
#if PHYRIAD_ARCH_X86_64
// x86-64: todos los procesadores modernos (Intel Core, AMD Zen) = 64 B.
// El spatial prefetcher de Intel agrupa dos líneas (ver kDestructivePad),
// pero la línea arquitectónica sigue siendo 64 B.
inline constexpr std::size_t kCachelineSize = 64;

#elif PHYRIAD_ARCH_AARCH64 && PHYRIAD_ARCH_APPLE
// Apple Silicon (M1, M2, M3, M4): línea de 128 B.
inline constexpr std::size_t kCachelineSize = 128;

#elif PHYRIAD_ARCH_AARCH64
// ARM genérico (AWS Graviton 3/4, Ampere Altra, Snapdragon server): 64 B.
inline constexpr std::size_t kCachelineSize = 64;

#else
// Arquitectura desconocida: valor conservador.
inline constexpr std::size_t kCachelineSize = 64;
#endif

// ── Pad anti-false-sharing ────────────────────────────────────────────────────
// = max(2 × kCachelineSize, 128).
// El factor 2 cubre spatial prefetchers que precargan el par de líneas
// adyacentes (Intel Sandy Bridge+ en modo interleaved).
// El mínimo de 128 garantiza que en x86 (64 B) seguimos usando 128 B.
inline constexpr std::size_t kDestructivePad =
    (2 * kCachelineSize > 128) ? 2 * kCachelineSize : 128;

// NOTE — std::hardware_destructive_interference_size is INTENTIONALLY not used.
// (1) It is a compile-time constant the toolchain bakes in (typically 64 on x86),
//     which UNDER-pads against the Intel/AMD spatial prefetcher's adjacent-line pair
//     — the very thing kDestructivePad's ×2 factor exists to cover.
// (2) It is mixed (via this value) into schema::schema_hash, so it is ABI-pinned:
//     swapping it would silently invalidate every persistent / cross-process artifact.
// Treat the constant as load-bearing — do NOT "modernize" it to the std symbol.

// ── Tamaño del slot compacto (SlotHeader 2.0) ────────────────────────────────
// 64 B: encaja en un cacheline x86, en media línea de Apple Silicon.
inline constexpr std::size_t kMessageSlotSize = 64;

// ── Verificaciones estáticas portabilidad ─────────────────────────────────────
static_assert((kDestructivePad & (kDestructivePad - 1)) == 0,
    "kDestructivePad must be a power of 2 for use with alignas()");
static_assert(kDestructivePad >= 64,
    "kDestructivePad too small — minimum 64 B required");

} // namespace phyriad::hal
// Made with my soul - Swately <3
