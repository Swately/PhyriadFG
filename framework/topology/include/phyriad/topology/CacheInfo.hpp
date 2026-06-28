// framework/topology/include/phyriad/topology/CacheInfo.hpp
// Detección runtime de cache topology via CPUID (x86) con fallbacks seguros.
//
// Separa dos conceptos:
//   - line_size        : unidad arquitectónica de coherency (CLFLUSH line).
//   - destructive_size : unidad efectiva de sharing considerando prefetchers
//                        espaciales. Es lo que realmente importa para decidir
//                        pad anti-false-sharing.
//
// En CPUs modernos: line_size = 64, destructive_size ∈ {64, 128}.
//
// Uso:
//   - CacheInfo es un POD trivial, copiable.
//   - probe() es noexcept y siempre retorna algo válido (fallbacks conservadores
//     si CPUID no disponible).
//   - validate_padding() emite warning si el pad compile-time es insuficiente.
#pragma once
#include <cstdint>
#include <type_traits>

namespace phyriad {

struct CacheInfo {
    // ── Detección principal ──────────────────────────────────────────────
    uint32_t line_size{64};          // CLFLUSH line (bytes)
    uint32_t destructive_size{128};  // Pad seguro anti-false-sharing
    uint32_t constructive_size{64};  // Para agrupar datos hot (= line_size)

    // ── Topología de caché por core (0 si no detectado) ──────────────────
    uint32_t l1d_bytes{0};           // L1 data cache por core
    uint32_t l2_bytes{0};            // L2 por core (o compartido en algunos CPUs)
    uint32_t l3_bytes{0};            // L3 compartido a nivel socket/chiplet

    // ── Identificación del CPU ───────────────────────────────────────────
    char     vendor[13]{};           // "AuthenticAMD", "GenuineIntel", etc. + NUL
    uint32_t family{0};              // CPU family
    uint32_t model{0};               // CPU model
    bool     cpuid_available{false}; // false → todos los valores son fallback

    // ── API ──────────────────────────────────────────────────────────────
    [[nodiscard]] static CacheInfo probe() noexcept;

    // Retorna true si compile_time_pad >= destructive_size.
    // Loguea warning si es insuficiente (no aborta — portable > estricto).
    bool validate_padding(uint32_t compile_time_pad) const noexcept;

    // Helpers
    [[nodiscard]] bool is_intel() const noexcept;
    [[nodiscard]] bool is_amd()   const noexcept;
};

// Pure POD layout: copyable across thread/process boundaries, ABI-stable.
static_assert(std::is_trivially_copyable_v<CacheInfo>,
              "CacheInfo must be trivially copyable");
static_assert(std::is_standard_layout_v<CacheInfo>,
              "CacheInfo must be standard layout (POD)");
static_assert(sizeof(CacheInfo) <= 64,
              "CacheInfo should fit in a single cacheline");

} // namespace phyriad
// Made with my soul - Swately <3
