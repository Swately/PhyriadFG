// framework/topology/src/CacheInfo.cpp
// Implementación de CacheInfo::probe() via CPUID x86 con fallbacks.
//
// Referencias:
//   Intel SDM Vol. 2A, CPUID instruction (leaf 1, 4, 0xB)
//   AMD APM Vol. 3, CPUID (leaf 0x80000005, 0x80000006, 0x8000001D)
//
// Portabilidad:
//   - MinGW-GCC / Linux GCC : usa <cpuid.h> (__get_cpuid, __get_cpuid_count)
//   - MSVC                  : usa <intrin.h> (__cpuid, __cpuidex)
//   - Arquitecturas no-x86  : fallback silencioso a valores conservadores
#include <phyriad/topology/CacheInfo.hpp>
#include <cstring>
#include <cstdio>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#  define PHYRIAD_ARCH_X86 1
#  if defined(_MSC_VER)
#    include <intrin.h>
#  else
#    include <cpuid.h>
#  endif
#else
#  define PHYRIAD_ARCH_X86 0
#endif

namespace phyriad {

namespace {

#if PHYRIAD_ARCH_X86

// Wrapper uniforme entre MSVC y GCC
struct CpuidRegs { uint32_t eax, ebx, ecx, edx; };

static bool cpuid(uint32_t leaf, uint32_t subleaf, CpuidRegs& r) noexcept {
#  if defined(_MSC_VER)
    int regs[4]{};
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
    r.eax = static_cast<uint32_t>(regs[0]);
    r.ebx = static_cast<uint32_t>(regs[1]);
    r.ecx = static_cast<uint32_t>(regs[2]);
    r.edx = static_cast<uint32_t>(regs[3]);
    return true;
#  else
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (!__get_cpuid_count(leaf, subleaf, &a, &b, &c, &d))
        return false;
    r.eax = a; r.ebx = b; r.ecx = c; r.edx = d;
    return true;
#  endif
}

// Leaf 0 → vendor string (EBX, EDX, ECX en ese orden forman los 12 bytes)
static void read_vendor(char out[13]) noexcept {
    CpuidRegs r{};
    if (!cpuid(0, 0, r)) { std::memset(out, 0, 13); return; }
    std::memcpy(out + 0, &r.ebx, 4);
    std::memcpy(out + 4, &r.edx, 4);
    std::memcpy(out + 8, &r.ecx, 4);
    out[12] = '\0';
}

// Leaf 1 → family, model, CLFLUSH line size
//   EBX[15:8] * 8 = CLFLUSH line size en bytes
//   EAX bits: [11:8]=family, [7:4]=model, extended fields si family==0xF o 6
static void read_leaf1(uint32_t& family, uint32_t& model,
                       uint32_t& clflush_line) noexcept {
    CpuidRegs r{};
    if (!cpuid(1, 0, r)) {
        family = 0; model = 0; clflush_line = 0;
        return;
    }
    const uint32_t base_family = (r.eax >> 8)  & 0xF;
    const uint32_t base_model  = (r.eax >> 4)  & 0xF;
    const uint32_t ext_family  = (r.eax >> 20) & 0xFF;
    const uint32_t ext_model   = (r.eax >> 16) & 0xF;

    family = (base_family == 0xF) ? base_family + ext_family : base_family;
    model  = (base_family == 0x6 || base_family == 0xF)
             ? ((ext_model << 4) | base_model)
             : base_model;

    clflush_line = ((r.ebx >> 8) & 0xFF) * 8;
}

// Leaf 4 (Intel) — deterministic cache parameters
//   Subleaf i itera niveles. Devuelve L1d, L2 per-core, L3 shared.
//   Campos:
//     EAX[4:0]  = cache type (1=data, 2=instr, 3=unified, 0=end)
//     EAX[7:5]  = cache level (1, 2, 3)
//     EBX[11:0] = line size - 1
//     EBX[21:12]= partitions - 1
//     EBX[31:22]= associativity - 1
//     ECX       = sets - 1
//     size = (ways+1) * (partitions+1) * (line+1) * (sets+1)
static void read_intel_caches(uint32_t& l1d, uint32_t& l2, uint32_t& l3) noexcept {
    l1d = l2 = l3 = 0;
    for (uint32_t sub = 0; sub < 16; ++sub) {
        CpuidRegs r{};
        if (!cpuid(4, sub, r)) break;
        const uint32_t type = r.eax & 0x1F;
        if (type == 0) break;                 // no more caches
        const uint32_t level = (r.eax >> 5) & 0x7;
        const uint32_t ways  = ((r.ebx >> 22) & 0x3FF) + 1;
        const uint32_t parts = ((r.ebx >> 12) & 0x3FF) + 1;
        const uint32_t line  = (r.ebx & 0xFFF) + 1;
        const uint32_t sets  = r.ecx + 1;
        const uint64_t size  = static_cast<uint64_t>(ways) * parts * line * sets;
        // Solo nos interesan data (1) y unified (3); ignoramos instruction (2)
        if (type == 2) continue;
        switch (level) {
            case 1: l1d = static_cast<uint32_t>(size); break;
            case 2: l2  = static_cast<uint32_t>(size); break;
            case 3: l3  = static_cast<uint32_t>(size); break;
            default: break;
        }
    }
}

// Leaf 0x8000001D (AMD Zen+) — topology de caché análoga a Intel leaf 4.
// Formato casi idéntico; mismos campos en EAX/EBX/ECX.
static void read_amd_caches(uint32_t& l1d, uint32_t& l2, uint32_t& l3) noexcept {
    l1d = l2 = l3 = 0;
    for (uint32_t sub = 0; sub < 16; ++sub) {
        CpuidRegs r{};
        if (!cpuid(0x8000001D, sub, r)) break;
        const uint32_t type = r.eax & 0x1F;
        if (type == 0) break;
        const uint32_t level = (r.eax >> 5) & 0x7;
        const uint32_t ways  = ((r.ebx >> 22) & 0x3FF) + 1;
        const uint32_t parts = ((r.ebx >> 12) & 0x3FF) + 1;
        const uint32_t line  = (r.ebx & 0xFFF) + 1;
        const uint32_t sets  = r.ecx + 1;
        const uint64_t size  = static_cast<uint64_t>(ways) * parts * line * sets;
        if (type == 2) continue;
        switch (level) {
            case 1: l1d = static_cast<uint32_t>(size); break;
            case 2: l2  = static_cast<uint32_t>(size); break;
            case 3: l3  = static_cast<uint32_t>(size); break;
            default: break;
        }
    }
}

// Heurística para destructive_size según vendor/family:
//   - Intel family 6 (Core desde Nehalem/Sandy Bridge+): spatial prefetcher
//     agresivo → 128 B efectivos
//   - AMD family >= 0x17 (Zen+): 64 B
//   - Vendor desconocido o family antigua → 128 conservador
static uint32_t derive_destructive_size(const char* vendor,
                                        uint32_t family) noexcept {
    if (std::strcmp(vendor, "GenuineIntel") == 0) {
        return (family == 6 || family >= 6) ? 128 : 64;
    }
    if (std::strcmp(vendor, "AuthenticAMD") == 0) {
        return (family >= 0x17) ? 64 : 128;
    }
    return 128;  // desconocido → conservador
}

#endif // PHYRIAD_ARCH_X86

} // anonymous namespace


// ─────────────────────────────────────────────────────────────────────────
// CacheInfo::probe
// ─────────────────────────────────────────────────────────────────────────
CacheInfo CacheInfo::probe() noexcept {
    CacheInfo info;  // defaults seguros: line=64, destructive=128, caches=0

#if PHYRIAD_ARCH_X86
    read_vendor(info.vendor);
    if (info.vendor[0] == '\0') {
        // CPUID leaf 0 falló — arquitectura exótica o emulador raro
        return info;
    }
    info.cpuid_available = true;

    uint32_t clflush_line = 0;
    read_leaf1(info.family, info.model, clflush_line);
    if (clflush_line != 0) {
        info.line_size         = clflush_line;
        info.constructive_size = clflush_line;
    }

    if (std::strcmp(info.vendor, "GenuineIntel") == 0) {
        read_intel_caches(info.l1d_bytes, info.l2_bytes, info.l3_bytes);
    } else if (std::strcmp(info.vendor, "AuthenticAMD") == 0) {
        read_amd_caches(info.l1d_bytes, info.l2_bytes, info.l3_bytes);
    }

    info.destructive_size = derive_destructive_size(info.vendor, info.family);
#endif

    return info;
}


bool CacheInfo::validate_padding(uint32_t compile_time_pad) const noexcept {
    if (compile_time_pad >= destructive_size) return true;
    std::fprintf(stderr,
        "[phyriad] WARNING: compile-time cacheline pad (%u) < detected "
        "destructive_size (%u) on %s family=%u. False sharing posible. "
        "Recompila con -DPHYRIAD_CACHELINE_PAD=%u.\n",
        compile_time_pad, destructive_size,
        vendor[0] ? vendor : "unknown", family, destructive_size);
    return false;
}


bool CacheInfo::is_intel() const noexcept {
    return std::strcmp(vendor, "GenuineIntel") == 0;
}

bool CacheInfo::is_amd() const noexcept {
    return std::strcmp(vendor, "AuthenticAMD") == 0;
}

} // namespace phyriad
// Made with my soul - Swately <3
