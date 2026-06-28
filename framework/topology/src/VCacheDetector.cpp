// framework/topology/src/VCacheDetector.cpp
#include <phyriad/topology/VCacheDetector.hpp>
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
#else
#  include <unistd.h>
#endif

namespace phyriad::topology {

namespace {

// V-Cache threshold: AMD X3D SKUs ship with 96 MB L3 on the V-Cache CCD.
// Normal CCDs (non-X3D) top out at 32-64 MB. 96 MB is unique to V-Cache.
static constexpr uint32_t kVCacheThresholdKB = 96u * 1024u;

// ── cpulist parser ────────────────────────────────────────────────
// Handles "0-7", "0,2,4,6", and "0-3,8-11" formats from sysfs.
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

// ── sysfs helper ─────────────────────────────────────────────────
static std::string sysfs_read_line(const char* path) noexcept {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::string s;
    std::getline(f, s);
    return s;
}

// ── LINUX ─────────────────────────────────────────────────────────
#ifndef _WIN32

static std::vector<VCacheCCX> detect_linux() noexcept {
    const long n_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (n_cpus <= 0) return {};

    // For each CPU: find its L3 cache entry and read shared_cpu_list + size.
    // Cores that share the same cpulist string belong to the same L3 domain.
    // shared_cpu_list is used as key — it's identical for all cores on a CCX.
    struct DomainKey { std::string cpulist; uint32_t l3_kb; };
    std::vector<DomainKey> known_domains;
    std::vector<VCacheCCX> result;
    uint32_t ccx_id = 0;
    char path[256];

    for (uint32_t cpu = 0u; cpu < static_cast<uint32_t>(n_cpus); ++cpu) {
        std::string cpulist;
        uint32_t    l3_kb = 0u;

        // Walk cache indices to find the L3 entry
        for (uint32_t idx = 0u; idx < 8u; ++idx) {
            std::snprintf(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%u/cache/index%u/level", cpu, idx);
            const std::string level_str = sysfs_read_line(path);
            if (level_str.empty()) break;             // no more cache levels
            if (level_str != "3") continue;

            std::snprintf(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%u/cache/index%u/shared_cpu_list", cpu, idx);
            cpulist = sysfs_read_line(path);

            std::snprintf(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%u/cache/index%u/size", cpu, idx);
            const std::string sz = sysfs_read_line(path);
            if (!sz.empty()) {
                try { l3_kb = static_cast<uint32_t>(std::stoul(sz)); } catch (...) {}
            }
            break; // found L3
        }

        if (cpulist.empty()) continue; // cpu has no L3 info — skip

        // Is this cpulist already registered as a domain?
        bool found = false;
        for (const auto& dk : known_domains) {
            if (dk.cpulist == cpulist) { found = true; break; }
        }
        if (found) continue;

        // New L3 domain
        VCacheCCX ccx{};
        ccx.ccx_id      = ccx_id++;
        ccx.l3_size_kb  = l3_kb;
        ccx.has_v_cache = (l3_kb >= kVCacheThresholdKB);
        parse_cpulist(cpulist, ccx.logical_ids);
        std::sort(ccx.logical_ids.begin(), ccx.logical_ids.end());

        known_domains.push_back({cpulist, l3_kb});
        result.push_back(std::move(ccx));
    }

    return result;
}

// ── WINDOWS ──────────────────────────────────────────────────────
#else

static std::vector<VCacheCCX> detect_windows() noexcept {
    DWORD buf_size = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &buf_size);
    if (buf_size == 0) return {};

    std::vector<uint8_t> buf(buf_size);
    if (!GetLogicalProcessorInformationEx(
            RelationCache,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
            &buf_size))
        return {};

    std::vector<VCacheCCX> result;
    uint32_t ccx_id = 0;
    DWORD offset = 0;

    while (offset < buf_size) {
        const auto* entry =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                buf.data() + offset);

        if (entry->Relationship == RelationCache) {
            const auto& c = entry->Cache;
            if (c.Level == 3 && (c.Type == CacheUnified || c.Type == CacheData)) {
                const uint32_t l3_kb = c.CacheSize / 1024u;

                VCacheCCX ccx{};
                ccx.ccx_id      = ccx_id++;
                ccx.l3_size_kb  = l3_kb;
                ccx.has_v_cache = (l3_kb >= kVCacheThresholdKB);

                // Extract logical core IDs from the affinity mask.
                // Phyriad operates within processor group 0; systems with >64 logical
                // cores in group 1+ will have those cores reported with offset.
                const KAFFINITY mask  = c.GroupMask.Mask;
                const WORD       group = c.GroupMask.Group;
                for (uint32_t bit = 0u; bit < 64u; ++bit) {
                    if (mask & (KAFFINITY(1u) << bit))
                        ccx.logical_ids.push_back(
                            static_cast<uint32_t>(group) * 64u + bit);
                }

                result.push_back(std::move(ccx));
            }
        }
        offset += entry->Size;
    }

    return result;
}

#endif // _WIN32

} // anonymous namespace

// ── Public API ────────────────────────────────────────────────────

std::vector<VCacheCCX> VCacheDetector::detect() noexcept {
#ifdef _WIN32
    return detect_windows();
#else
    return detect_linux();
#endif
}

std::vector<uint32_t> VCacheDetector::vcache_logical_ids() noexcept {
    const auto ccxs = detect();
    std::vector<uint32_t> result;
    for (const auto& ccx : ccxs) {
        if (ccx.has_v_cache) {
            result.insert(result.end(),
                          ccx.logical_ids.begin(),
                          ccx.logical_ids.end());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace phyriad::topology
// Made with my soul - Swately <3
