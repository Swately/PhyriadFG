// framework/topology/src/HybridCoreDetector.cpp
#include <phyriad/topology/HybridCoreDetector.hpp>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <intrin.h>
#else
#  include <filesystem>
#  include <unistd.h>
#endif

namespace phyriad::topology {

namespace {

// ── sysfs helper ─────────────────────────────────────────────────
static std::string sysfs_read_line(const char* path) noexcept {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::string s;
    std::getline(f, s);
    return s;
}

static uint32_t sysfs_read_u32(const char* path, uint32_t def) noexcept {
    const auto s = sysfs_read_line(path);
    if (s.empty()) return def;
    try { return static_cast<uint32_t>(std::stoul(s)); } catch (...) { return def; }
}

// ── cpulist parser ────────────────────────────────────────────────
static void parse_cpulist(const std::string& s, std::vector<uint32_t>& out) noexcept {
    std::string tok;
    auto flush = [&]() noexcept {
        if (tok.empty()) return;
        const auto dash = tok.find('-');
        try {
            if (dash == std::string::npos) {
                out.push_back(static_cast<uint32_t>(std::stoul(tok)));
            } else {
                uint32_t lo = static_cast<uint32_t>(std::stoul(tok.substr(0, dash)));
                uint32_t hi = static_cast<uint32_t>(std::stoul(tok.substr(dash + 1)));
                for (uint32_t i = lo; i <= hi; ++i) out.push_back(i);
            }
        } catch (...) {}
        tok.clear();
    };
    for (char c : s) {
        if (c == ',' || c == '\n' || c == '\r') flush();
        else tok += c;
    }
    flush();
}

// ─────────────────────────────────────────────────────────────────
// LINUX
// ─────────────────────────────────────────────────────────────────
#ifndef _WIN32

// Strategy 1: /sys/devices/cpu_atom/cpus + /sys/devices/cpu_core/cpus
// Present since Linux 5.18 on Intel hybrid systems (Alder Lake+, Raptor Lake,
// Meteor Lake). The kernel exposes separate policy directories for atom/core.
static bool try_detect_linux_sysfs_atom(HybridInfo& out) noexcept {
    namespace fs = std::filesystem;
    const fs::path atom_cpus_path = "/sys/devices/cpu_atom/cpus";
    const fs::path core_cpus_path = "/sys/devices/cpu_core/cpus";

    if (!fs::exists(atom_cpus_path)) return false;

    std::vector<uint32_t> e_cores, p_cores;

    std::string atom_list = sysfs_read_line(atom_cpus_path.string().c_str());
    if (!atom_list.empty()) parse_cpulist(atom_list, e_cores);

    if (fs::exists(core_cpus_path)) {
        std::string core_list = sysfs_read_line(core_cpus_path.string().c_str());
        if (!core_list.empty()) parse_cpulist(core_list, p_cores);
    }

    if (e_cores.empty()) return false;

    // Build CoreTypeEntry for all known cores
    for (uint32_t id : e_cores)
        out.core_types.push_back({id, CoreType::Efficiency});
    for (uint32_t id : p_cores)
        out.core_types.push_back({id, CoreType::Performance});

    std::sort(out.core_types.begin(), out.core_types.end(),
        [](const CoreTypeEntry& a, const CoreTypeEntry& b){
            return a.logical_id < b.logical_id;
        });

    out.is_hybrid         = true;
    out.detection_method  = "sysfs_cpu_atom";
    return true;
}

// Strategy 2: cpufreq heuristic — cores with freq < 75% of system max
// are classified as efficiency cores. Works for Intel hybrid CPUs and ARM
// big.LITTLE even on older kernels without cpu_atom directory.
static bool try_detect_linux_cpufreq(HybridInfo& out) noexcept {
    const long n_long = sysconf(_SC_NPROCESSORS_ONLN);
    if (n_long <= 0) return false;
    const uint32_t n_cpus = static_cast<uint32_t>(n_long);

    std::vector<uint32_t> freq_khz(n_cpus, 0u);
    uint32_t sys_max_khz = 0u;
    char path[256];

    for (uint32_t i = 0u; i < n_cpus; ++i) {
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", i);
        const uint32_t f = sysfs_read_u32(path, 0u);
        freq_khz[i] = f;
        if (f > sys_max_khz) sys_max_khz = f;
    }

    if (sys_max_khz == 0u) return false;

    // Check if any core has freq noticeably below max
    const uint32_t threshold_khz = (sys_max_khz * 3u) / 4u;
    const bool has_lower = std::any_of(freq_khz.begin(), freq_khz.end(),
        [&](uint32_t f){ return f > 0u && f < threshold_khz; });

    for (uint32_t i = 0u; i < n_cpus; ++i) {
        const bool is_e = has_lower && (freq_khz[i] > 0u) && (freq_khz[i] < threshold_khz);
        out.core_types.push_back({i, is_e ? CoreType::Efficiency : CoreType::Performance});
    }

    out.is_hybrid        = has_lower;
    out.detection_method = has_lower ? "cpufreq_heuristic" : "cpufreq_uniform";
    return true;
}

// Strategy 3: sysfs topology/core_type (Linux 6.3+ on some ARM SoCs)
// /sys/devices/system/cpu/cpu{N}/topology/core_type returns "core" or "cluster"
static bool try_detect_linux_core_type(HybridInfo& out) noexcept {
    const long n_long = sysconf(_SC_NPROCESSORS_ONLN);
    if (n_long <= 0) return false;
    const uint32_t n_cpus = static_cast<uint32_t>(n_long);
    char path[256];

    bool any_found = false;
    for (uint32_t i = 0u; i < n_cpus; ++i) {
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/cpu/cpu%u/topology/core_type", i);
        const std::string val = sysfs_read_line(path);
        if (val.empty()) continue;
        any_found = true;
        // "core" or "1" → Performance; "cluster" or "0" → Efficiency
        const bool is_e = (val == "0" || val == "cluster");
        out.core_types.push_back({i, is_e ? CoreType::Efficiency : CoreType::Performance});
        if (is_e) out.is_hybrid = true;
    }

    if (!any_found) return false;
    std::sort(out.core_types.begin(), out.core_types.end(),
        [](const CoreTypeEntry& a, const CoreTypeEntry& b){
            return a.logical_id < b.logical_id;
        });
    out.detection_method = "sysfs_core_type";
    return true;
}

static HybridInfo detect_linux() noexcept {
    HybridInfo info;

    if (try_detect_linux_sysfs_atom(info))  return info;
    if (try_detect_linux_core_type(info))   return info;
    if (try_detect_linux_cpufreq(info))     return info;

    // Final fallback: all cores are Performance (non-hybrid default)
    const long n = sysconf(_SC_NPROCESSORS_ONLN);
    const uint32_t n_cpus = (n > 0) ? static_cast<uint32_t>(n) : 1u;
    info.core_types.reserve(n_cpus);
    for (uint32_t i = 0u; i < n_cpus; ++i)
        info.core_types.push_back({i, CoreType::Performance});
    info.is_hybrid        = false;
    info.detection_method = "fallback_all_performance";
    return info;
}

// ─────────────────────────────────────────────────────────────────
// WINDOWS
// ─────────────────────────────────────────────────────────────────
#else

static HybridInfo detect_windows() noexcept {
    HybridInfo info;

    DWORD buf_size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &buf_size);
    if (buf_size == 0) {
        info.detection_method = "fallback_glpiex_unavailable";
        return info;
    }

    std::vector<uint8_t> buf(buf_size);
    if (!GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
            &buf_size)) {
        info.detection_method = "fallback_glpiex_failed";
        return info;
    }

    DWORD offset = 0;
    uint32_t logical_idx = 0;
    bool any_efficiency = false;

    while (offset < buf_size) {
        const auto* entry =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                buf.data() + offset);

        if (entry->Relationship == RelationProcessorCore) {
            const auto& pc = entry->Processor;
            // EfficiencyClass: 0 = E-core, 1+ = P-core.
            // On non-hybrid CPUs all entries report 0 — post-processed below.
            const uint8_t eff = pc.EfficiencyClass;

            const KAFFINITY mask = pc.GroupMask[0].Mask;
            for (uint32_t bit = 0u; bit < 64u; ++bit) {
                if (!(mask & (KAFFINITY(1u) << bit))) continue;
                const bool is_e = (eff == 0u);
                info.core_types.push_back({logical_idx++,
                    is_e ? CoreType::Efficiency : CoreType::Performance});
                if (is_e) any_efficiency = true;
            }
        }
        offset += entry->Size;
    }

    // On non-hybrid CPUs all EfficiencyClass values are 0.
    // If no core has eff > 0, the CPU is not hybrid — treat all as Performance.
    if (!any_efficiency) {
        // Verify: is any EfficiencyClass actually > 0?
        DWORD offset2 = 0;
        bool has_nonzero_eff = false;
        while (offset2 < buf_size) {
            const auto* e2 =
                reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                    buf.data() + offset2);
            if (e2->Relationship == RelationProcessorCore &&
                e2->Processor.EfficiencyClass > 0u)
            { has_nonzero_eff = true; break; }
            offset2 += e2->Size;
        }

        if (!has_nonzero_eff) {
            for (auto& entry : info.core_types)
                entry.type = CoreType::Performance;
            info.is_hybrid        = false;
            info.detection_method = "winapi_non_hybrid";
            return info;
        }
    }

    info.is_hybrid        = any_efficiency;
    info.detection_method = any_efficiency ? "winapi_efficiency_class" : "winapi_non_hybrid";
    return info;
}

#endif // _WIN32

} // anonymous namespace

// ── Public API ────────────────────────────────────────────────────

HybridInfo HybridCoreDetector::detect() noexcept {
#ifdef _WIN32
    return detect_windows();
#else
    return detect_linux();
#endif
}

bool HybridCoreDetector::is_hybrid() noexcept {
    static const bool s_hybrid = detect().is_hybrid;
    return s_hybrid;
}

} // namespace phyriad::topology
// Made with my soul - Swately <3
