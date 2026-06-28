// framework/topology/include/phyriad/topology/CpuFeatures.hpp
// Portable CPUID wrapper and x86/ARM ISA feature detection (header-only).
// No external dependencies. Returns zero-initialized structs on non-x86.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

namespace phyriad::topology {

// ── Raw CPUID ──────────────────────────────────────────────────────
struct CpuidResult { uint32_t eax{0}, ebx{0}, ecx{0}, edx{0}; };

[[nodiscard]] inline CpuidResult cpuid(uint32_t leaf, uint32_t subleaf = 0u) noexcept {
    CpuidResult r{};
#if defined(_MSC_VER)
    int regs[4]{};
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
    r.eax = static_cast<uint32_t>(regs[0]);
    r.ebx = static_cast<uint32_t>(regs[1]);
    r.ecx = static_cast<uint32_t>(regs[2]);
    r.edx = static_cast<uint32_t>(regs[3]);
#elif (defined(__GNUC__) || defined(__clang__)) && \
      (defined(__x86_64__) || defined(__i386__))
    __asm__ volatile(
        "cpuid"
        : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
        : "a"(leaf), "c"(subleaf)
    );
#endif
    return r;
}

[[nodiscard]] inline uint32_t cpuid_max_basic() noexcept {
    static const uint32_t s_max = cpuid(0u).eax;
    return s_max;
}

[[nodiscard]] inline uint32_t cpuid_max_extended() noexcept {
    static const uint32_t s_max = cpuid(0x80000000u).eax;
    return s_max;
}

// ── CPU Vendor ────────────────────────────────────────────────────
enum class CpuVendor : uint8_t { Intel, AMD, ARM, Unknown };

[[nodiscard]] inline CpuVendor detect_cpu_vendor() noexcept {
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__)
    return CpuVendor::ARM;
#elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    const auto r = cpuid(0u);
    // "GenuineIntel"
    if (r.ebx == 0x756e6547u && r.edx == 0x49656e69u && r.ecx == 0x6c65746eu)
        return CpuVendor::Intel;
    // "AuthenticAMD"
    if (r.ebx == 0x68747541u && r.edx == 0x69746e65u && r.ecx == 0x444d4163u)
        return CpuVendor::AMD;
#endif
    return CpuVendor::Unknown;
}

// ── ISA Feature Flags ────────────────────────────────────────────
struct X86Features {
    // SSE
    bool sse4_1{false};
    bool sse4_2{false};
    // AVX
    bool avx{false};
    bool avx2{false};
    bool avx512f{false};
    bool avx512bw{false};
    bool avx512vl{false};
    bool avx512dq{false};
    bool avx512cd{false};
    // Bit manipulation
    bool bmi1{false};
    bool bmi2{false};
    bool popcnt{false};
    bool lzcnt{false};
    // Crypto / hash accelerators
    bool aes{false};
    bool sha{false};
    // FP extensions
    bool f16c{false};
    bool fma{false};
    // Cache coherence helpers
    bool clflushopt{false};
    bool clwb{false};
};

[[nodiscard]] inline X86Features detect_x86_features() noexcept {
    X86Features f{};
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    const uint32_t max_b = cpuid_max_basic();
    const uint32_t max_e = cpuid_max_extended();

    if (max_b >= 1u) {
        const auto r1 = cpuid(1u);
        f.sse4_1    = static_cast<bool>((r1.ecx >> 19) & 1u);
        f.sse4_2    = static_cast<bool>((r1.ecx >> 20) & 1u);
        f.avx       = static_cast<bool>((r1.ecx >> 28) & 1u);
        f.aes       = static_cast<bool>((r1.ecx >> 25) & 1u);
        f.f16c      = static_cast<bool>((r1.ecx >> 29) & 1u);
        f.fma       = static_cast<bool>((r1.ecx >> 12) & 1u);
        f.popcnt    = static_cast<bool>((r1.ecx >> 23) & 1u);
    }
    if (max_b >= 7u) {
        const auto r7 = cpuid(7u, 0u);
        f.avx2       = static_cast<bool>((r7.ebx >>  5) & 1u);
        f.bmi1       = static_cast<bool>((r7.ebx >>  3) & 1u);
        f.bmi2       = static_cast<bool>((r7.ebx >>  8) & 1u);
        f.avx512f    = static_cast<bool>((r7.ebx >> 16) & 1u);
        f.avx512dq   = static_cast<bool>((r7.ebx >> 17) & 1u);
        f.avx512bw   = static_cast<bool>((r7.ebx >> 30) & 1u);
        f.avx512vl   = static_cast<bool>((r7.ebx >> 31) & 1u);
        f.avx512cd   = static_cast<bool>((r7.ebx >> 28) & 1u);
        f.sha        = static_cast<bool>((r7.ebx >> 29) & 1u);
        f.clflushopt = static_cast<bool>((r7.ebx >> 23) & 1u);
        f.clwb       = static_cast<bool>((r7.ebx >> 24) & 1u);
    }
    if (max_e >= 0x80000001u) {
        const auto re1 = cpuid(0x80000001u);
        f.lzcnt = static_cast<bool>((re1.ecx >> 5) & 1u);
    }
#endif
    return f;
}

// Returns CPU brand string e.g. "AMD Ryzen 9 7950X3D 16-Core Processor"
[[nodiscard]] inline std::string detect_cpu_brand() noexcept {
    std::string brand;
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    if (cpuid_max_extended() >= 0x80000004u) {
        std::array<uint32_t, 12> regs{};
        for (uint32_t i = 0u; i < 3u; ++i) {
            const auto r = cpuid(0x80000002u + i);
            regs[i*4+0] = r.eax; regs[i*4+1] = r.ebx;
            regs[i*4+2] = r.ecx; regs[i*4+3] = r.edx;
        }
        brand.resize(48u);
        std::memcpy(brand.data(), regs.data(), 48u);
        const auto last = brand.find_last_not_of('\0');
        if (last != std::string::npos) brand.resize(last + 1u);
        const auto first = brand.find_first_not_of(' ');
        if (first != std::string::npos && first > 0u) brand.erase(0u, first);
    }
#elif defined(__aarch64__)
    brand = "ARM64";
#endif
    return brand;
}

} // namespace phyriad::topology
// Made with my soul - Swately <3
