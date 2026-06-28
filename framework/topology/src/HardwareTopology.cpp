// framework/topology/src/HardwareTopology.cpp
// Hardware topology detection implementation — pillar version.
//
// Covers lines 1-1099 of the original core/HardwareTopology.cpp:
//   - probe_gpus (CUDA + DRM fallback)
//   - HardwareTopology::probe() for Linux and Windows
//   - HardwareTopology member helpers (physical_core_count, allocate_cores, …)
//   - hw:: namespace: enumerate_cores, pin_current_thread, elevate_thread_rt,
//     v_cache_cores, p_cores, e_cores, ccd_cores, numa_cores, numa_node_count,
//     optimal_producer_consumer_pair, detect_system_memory_mb
//
// Excluded (moved to pillars/runtime/):
//   - hw::make_auto_profile()   — depends on PerformanceProfile
//   - hw::apply_profile_hints() — depends on PerformanceProfile
//

#include <phyriad/topology/HardwareTopology.hpp>
#include <phyriad/topology/CpuFeatures.hpp>
#include <phyriad/topology/VCacheDetector.hpp>
#include <phyriad/topology/HybridCoreDetector.hpp>
#include <phyriad/topology/PCIeAffinityProbe.hpp>
#ifdef HAVE_CUDA
#  include <phyriad/topology/CudaDriver.hpp>
#endif
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <sysinfoapi.h>
  #include <intrin.h>          // __cpuid, __cpuidex
  #include <avrt.h>            // THREAD_PROTECTION S2: MMCSS (AvSetMmThreadCharacteristics / AvSetMmThreadPriority / AvRevert)
  #ifdef _MSC_VER
    #pragma comment(lib, "Avrt.lib")  // link the AVRT (MMCSS) import lib for direct-source consumers (render_assistant)
  #endif
#else
  #include <sched.h>
  #include <pthread.h>
  #include <sys/resource.h>   // getpriority / setpriority (FR-9)
  #include <sys/sysinfo.h>
  #include <unistd.h>
  #include <cpuid.h>           // __get_cpuid / __cpuid_count (GCC/Clang)
  // libnuma es opcional — no lo incluimos directamente para no requerir libnuma
#endif

namespace phyriad {

// ────────────────────────────────────────────────────────────────────────────
// GPU detection unificada — cross-platform via CUDA Driver API dinámica
// ────────────────────────────────────────────────────────────────────────────
namespace {

#ifndef _WIN32
// Fallback DRM para GPUs no-CUDA en Linux (AMD, Intel)
int64_t sysfs_read_int(const std::filesystem::path& p) noexcept {
    std::ifstream f(p);
    int64_t v = -1;
    f >> v;
    return v;
}

std::vector<GpuInfo> probe_gpus_drm() {
    std::vector<GpuInfo> result;
    namespace fs = std::filesystem;
    const fs::path drm_base = "/sys/class/drm";
    if (!fs::exists(drm_base)) return result;

    uint32_t idx = 0;
    for (auto& entry : fs::directory_iterator(drm_base)) {
        const auto name = entry.path().filename().string();
        if (name.substr(0, 4) != "card") continue;
        if (name.find('-') != std::string::npos) continue;

        GpuInfo gi{};
        gi.device_index = idx++;

        auto vendor_path = entry.path() / "device" / "product_name";
        if (fs::exists(vendor_path)) {
            std::ifstream vf(vendor_path);
            std::getline(vf, gi.name);
        }
        if (gi.name.empty()) gi.name = "GPU " + std::to_string(gi.device_index);

        auto vram_total = entry.path() / "device" / "mem_info_vram_total";
        auto vram_used  = entry.path() / "device" / "mem_info_vram_used";
        if (fs::exists(vram_total)) {
            gi.vram_total_bytes = static_cast<uint64_t>(
                std::max<int64_t>(0, sysfs_read_int(vram_total)));
        }
        if (fs::exists(vram_used)) {
            uint64_t used = static_cast<uint64_t>(
                std::max<int64_t>(0, sysfs_read_int(vram_used)));
            gi.vram_free_bytes = gi.vram_total_bytes - used;
        }

        gi.cuda_capable    = false;
        gi.display_capable = true;

        result.push_back(std::move(gi));
    }
    return result;
}
#endif // !_WIN32

std::vector<GpuInfo> probe_gpus() {
#ifdef HAVE_CUDA
    auto& drv = cuda::CudaDriver::instance();

    if (drv.available()) {
        auto devices = drv.enumerate_devices();
        if (!devices.empty()) {
            std::vector<GpuInfo> result;
            result.reserve(devices.size());
            for (auto& d : devices) {
                GpuInfo gi{};
                gi.device_index       = static_cast<uint32_t>(d.device_index);
                gi.name               = std::move(d.name);
                gi.vram_total_bytes   = d.vram_total_bytes;
                gi.vram_free_bytes    = d.vram_free_bytes;
                gi.cuda_capable       = true;
                gi.display_capable    = !d.tcc_driver;
                gi.sm_major           = d.sm_major;
                gi.sm_minor           = d.sm_minor;
                gi.sm_count           = d.sm_count;
                gi.max_threads_per_sm = d.max_threads_per_sm;
                gi.clock_rate_khz     = d.clock_rate_khz;
                gi.mem_clock_khz      = d.mem_clock_khz;
                gi.mem_bus_width_bits  = d.mem_bus_width_bits;
                gi.l2_cache_bytes     = d.l2_cache_bytes;
                gi.ecc_enabled        = d.ecc_enabled;
                gi.unified_addressing = d.unified_addressing;
                gi.managed_memory     = d.managed_memory;
                result.push_back(std::move(gi));
            }
            return result;
        }
    }
#endif // HAVE_CUDA

#ifndef _WIN32
    return probe_gpus_drm();
#else
    return {};
#endif
}

} // anon namespace

// ────────────────────────────────────────────────────────────────────────────
// LINUX
// ────────────────────────────────────────────────────────────────────────────
#ifndef _WIN32

namespace {

// (probe_cores_linux removed — core enumeration is unified through
//  hw::enumerate_cores(); the old inline builder also mis-set is_ht_sibling,
//  marking BOTH siblings as siblings.)

std::vector<NumaNode> probe_numa_linux() {
    std::vector<NumaNode> result;
    namespace fs = std::filesystem;
    const fs::path node_base = "/sys/devices/system/node";

    if (!fs::exists(node_base)) {
        NumaNode n{};
        n.id = 0;
        struct sysinfo si{};
        ::sysinfo(&si);
        n.total_memory_bytes = si.totalram  * si.mem_unit;
        n.free_memory_bytes  = si.freeram   * si.mem_unit;
        result.push_back(std::move(n));
        return result;
    }

    for (auto& entry : fs::directory_iterator(node_base)) {
        const auto name = entry.path().filename().string();
        if (name.substr(0, 4) != "node") continue;
        bool all_digits = name.size() > 4 &&
            std::all_of(name.begin() + 4, name.end(), ::isdigit);
        if (!all_digits) continue;

        NumaNode n{};
        n.id = static_cast<uint32_t>(std::stoul(name.substr(4)));

        std::ifstream meminfo(entry.path() / "meminfo");
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal") != std::string::npos) {
                uint64_t kb; std::sscanf(line.c_str(), "%*s %*s %llu", &kb);
                n.total_memory_bytes = kb * 1024;
            }
            if (line.find("MemFree") != std::string::npos) {
                uint64_t kb; std::sscanf(line.c_str(), "%*s %*s %llu", &kb);
                n.free_memory_bytes = kb * 1024;
            }
        }

        std::ifstream cpulist(entry.path() / "cpulist");
        std::string cl; std::getline(cpulist, cl);
        std::stringstream ss(cl);
        std::string token;
        while (std::getline(ss, token, ',')) {
            auto dash = token.find('-');
            if (dash == std::string::npos) {
                n.logical_core_ids.push_back(std::stoul(token));
            } else {
                uint32_t lo = std::stoul(token.substr(0, dash));
                uint32_t hi = std::stoul(token.substr(dash + 1));
                for (uint32_t i = lo; i <= hi; ++i)
                    n.logical_core_ids.push_back(i);
            }
        }

        result.push_back(std::move(n));
    }

    std::sort(result.begin(), result.end(),
        [](const NumaNode& a, const NumaNode& b){ return a.id < b.id; });
    return result;
}

} // anon namespace

std::expected<HardwareTopology, std::string>
HardwareTopology::probe() noexcept {
    HardwareTopology t;
    t.cores      = hw::enumerate_cores();   // single source of truth (see Windows note)
    t.numa_nodes = probe_numa_linux();
    t.gpus       = probe_gpus();
    t.cache      = CacheInfo::probe();

    struct sysinfo si{};
    ::sysinfo(&si);
    t.total_ram_bytes = si.totalram * si.mem_unit;
    t.free_ram_bytes  = si.freeram  * si.mem_unit;

    if (t.cores.empty())
        return std::unexpected("No se encontraron cores en /sys/devices/system/cpu");

    // ── [BLOQUE A] Topology Extensions ──────────────────────────
    t.cpu_features      = topology::detect_x86_features();
    t.vcache_cores      = topology::VCacheDetector::vcache_logical_ids();
    t.core_types        = topology::HybridCoreDetector::detect().core_types;
    t.pcie_affinity_map = topology::PCIeAffinityProbe::probe();

    // ── FR-2: ccd_count_ ────────────────────────────────────────
    {
        uint32_t max_ccd = 0u;
        for (const auto& c : t.cores)
            if (c.ccd_id > max_ccd) max_ccd = c.ccd_id;
        t.ccd_count_ = t.cores.empty() ? 0u : (max_ccd + 1u);
    }

    return t;
}

#else // ────────────────────── WINDOWS ─────────────────────────────────────

std::expected<HardwareTopology, std::string>
HardwareTopology::probe() noexcept {
    HardwareTopology t;

    // ── Cores: SINGLE SOURCE OF TRUTH ───────────────────────────────
    // Delegate to hw::enumerate_cores() (probe_cores_platform_ + fill_cache_topology),
    // the complete enumeration that correctly detects SMT siblings, physical
    // V-Cache cores, CCD, and hybrid P/E classes. The previous inline builder here
    // mis-computed is_ht_sibling (it compared each bit against the lowest bit of a
    // mask it was MUTATING in the loop → always false, so every logical thread was
    // counted as a physical core) and never assigned CCD — a legacy duplicate from
    // before the topology pillar matured. Converging on one path fixes every
    // probe()/topology() consumer (PlacementPlanner included) at once.
    t.cores = hw::enumerate_cores();
    if (t.cores.empty())
        return std::unexpected("hw::enumerate_cores() returned no cores");

    // ── NUMA nodes via GetLogicalProcessorInformationEx(RelationNumaNode) ──
    DWORD buflen = 0;
    GetLogicalProcessorInformationEx(RelationNumaNode, nullptr, &buflen);
    if (buflen) {
        std::vector<uint8_t> buf(buflen);
        if (GetLogicalProcessorInformationEx(RelationNumaNode,
                reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
                &buflen)) {
            auto* ptr = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data());
            auto* end = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                            buf.data() + buflen);
            while (ptr < end) {
                if (ptr->Relationship == RelationNumaNode) {
                    NumaNode n{};
                    n.id = ptr->NumaNode.NodeNumber;
                    ULONGLONG avail = 0;
                    GetNumaAvailableMemoryNodeEx(static_cast<USHORT>(n.id), &avail);
                    n.free_memory_bytes  = avail;
                    n.total_memory_bytes = avail;
                    t.numa_nodes.push_back(n);
                }
                ptr = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                      reinterpret_cast<uint8_t*>(ptr) + ptr->Size);
            }
        }
    }

    // ── RAM total ────────────────────────────────────────────────
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    t.total_ram_bytes = ms.ullTotalPhys;
    t.free_ram_bytes  = ms.ullAvailPhys;

    // ── GPUs via CUDA Driver API (carga dinámica de nvcuda.dll) ──
    t.gpus = probe_gpus();

    // ── Cache topology (CPUID x86) ───────────────────────────────
    t.cache = CacheInfo::probe();

    // ── [BLOQUE A] Topology Extensions ──────────────────────────
    t.cpu_features      = topology::detect_x86_features();
    t.vcache_cores      = topology::VCacheDetector::vcache_logical_ids();
    t.core_types        = topology::HybridCoreDetector::detect().core_types;
    t.pcie_affinity_map = topology::PCIeAffinityProbe::probe();

    // Back-fill CoreInfo::has_v_cache from the vcache_cores list.
    // The inline core builder above does not call fill_cache_topology(), so
    // has_v_cache defaults to false for all cores.  Sync it here so that
    // Scheduler::score_core() and any other CoreInfo consumer can use the
    // flag directly without consulting topology.vcache_cores separately.
    for (uint32_t vcid : t.vcache_cores) {
        for (auto& c : t.cores)
            if (c.logical_id == vcid) { c.has_v_cache = true; break; }
    }

    // ── FR-2: ccd_count_ ────────────────────────────────────────
    {
        uint32_t max_ccd = 0u;
        for (const auto& c : t.cores)
            if (c.ccd_id > max_ccd) max_ccd = c.ccd_id;
        t.ccd_count_ = t.cores.empty() ? 0u : (max_ccd + 1u);
    }

    return t;
}

#endif // _WIN32

// ── Helpers compartidos ───────────────────────────────────────────────────

uint32_t HardwareTopology::physical_core_count() const noexcept {
    uint32_t count = 0;
    for (auto& c : cores)
        if (!c.is_ht_sibling) ++count;
    return count ? count : static_cast<uint32_t>(cores.size());
}

uint32_t HardwareTopology::logical_core_count() const noexcept {
    return static_cast<uint32_t>(cores.size());
}

std::vector<uint32_t>
HardwareTopology::physical_cores_on_numa(uint32_t node_id) const noexcept {
    std::vector<uint32_t> result;
    for (auto& c : cores)
        if (c.numa_node == node_id && !c.is_ht_sibling)
            result.push_back(c.logical_id);
    return result;
}

std::vector<uint32_t>
HardwareTopology::allocate_cores(uint32_t count, uint32_t prefer_numa) const noexcept {
    std::vector<uint32_t> pool;

    if (prefer_numa != UINT32_MAX)
        pool = physical_cores_on_numa(prefer_numa);

    if (pool.size() < count) {
        for (auto& c : cores) {
            if (c.is_ht_sibling) continue;
            if (std::find(pool.begin(), pool.end(), c.logical_id) == pool.end())
                pool.push_back(c.logical_id);
            if (pool.size() >= count) break;
        }
    }

    if (pool.size() > count) pool.resize(count);
    return pool;
}

// ──────────────────────────────────────────────────────────────────
// hw:: — implementaciones reales (Fase E0)
// ──────────────────────────────────────────────────────────────────
namespace hw {

// ── Helpers internos ──────────────────────────────────────────────
namespace {

#ifdef _WIN32
// fill_cache_topology — rellena L3, CCD, CCX y V-Cache para cualquier CPU x86 en Windows.
//
// Diseño de Graceful Degradation:
//   - L3 size  : GetLogicalProcessorInformationEx(RelationCache) — funciona en Intel, AMD, ARM64
//   - CCD      : cores que comparten la misma mascara de afinidad L3 → mismo CCD
//                (en Intel = "L3 slice domain"; en AMD = CCD real)
//   - CCX      : AMD Zen 4+ → CPUID 0x80000026 (bits de shift del APIC ID)
//                AMD Zen 1-3 → ccx_id = ccd_id (Zen 3 tiene 1 CCX monolítico por CCD)
//                Intel / otro → ccx_id = ccd_id (el L3 es el dominio más pequeño relevante)
//   - V-Cache  : l3_cache_kb > 64 MB (solo AMD 3D V-Cache; false para todo lo demás)
//
// Limitación conocida: sistemas con >64 cores lógicos por grupo de procesador
// (Group > 0) necesitan lógica multi-group. Phyriad actualmente opera en Group 0;
// en sistemas de más de 64 cores se usa solo los primeros 64 lógicos para el mapeo
// de máscara, lo que es correcto para la gran mayoría de workstations/servidores.
void fill_cache_topology(std::vector<CoreInfo>& cores) noexcept {
    if (cores.empty()) return;

    // ── 1. L3 info via GLPIEX — independiente de fabricante ──────────────
    DWORD buf_size = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &buf_size);
    std::vector<uint8_t> buf(buf_size);
    if (!GetLogicalProcessorInformationEx(RelationCache,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
            &buf_size))
        return; // Sin info de caché: ccd_id/ccx_id quedan en 0 (degradación aceptable)

    struct L3Info { KAFFINITY mask; WORD group; uint32_t size_kb; };
    std::vector<L3Info> l3_infos;

    DWORD offset = 0;
    while (offset < buf_size) {
        auto* entry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buf.data() + offset);
        if (entry->Relationship == RelationCache) {
            const auto& c = entry->Cache;
            if (c.Level == 3 && (c.Type == CacheUnified || c.Type == CacheData)) {
                l3_infos.push_back({c.GroupMask.Mask,
                                    c.GroupMask.Group,
                                    c.CacheSize / 1024u});
            }
        }
        offset += entry->Size;
    }

    // ── 2. Asignar L3 y V-Cache a cada core ─────────────────────────────
    // V-Cache threshold: >64 MB (AMD 3D V-Cache CCD tiene ~96 MB total L3).
    static constexpr uint32_t kVCacheThresholdKB = 65536u;

    for (auto& core : cores) {
        // logical_id % 64 maneja el bit dentro del grupo de 64-core.
        const KAFFINITY bit = KAFFINITY(1ULL << (core.logical_id % 64));
        for (const auto& l3 : l3_infos) {
            if (l3.mask & bit) {
                core.l3_cache_kb = l3.size_kb;
                core.has_v_cache = (l3.size_kb > kVCacheThresholdKB);
                break;
            }
        }
    }

    // ── 3. CCD: agrupar por (group, L3 mask) → mismo L3 = mismo CCD ─────
    uint32_t ccd_id_next = 0;
    struct CcdGroup { KAFFINITY mask; WORD group; uint32_t ccd_id; };
    std::vector<CcdGroup> ccd_groups;

    for (auto& core : cores) {
        const KAFFINITY bit = KAFFINITY(1ULL << (core.logical_id % 64));
        KAFFINITY core_l3_mask = 0;
        WORD      core_l3_grp  = 0;
        for (const auto& l3 : l3_infos) {
            if (l3.mask & bit) { core_l3_mask = l3.mask; core_l3_grp = l3.group; break; }
        }
        bool found = false;
        for (const auto& g : ccd_groups) {
            if (g.mask == core_l3_mask && g.group == core_l3_grp) {
                core.ccd_id = g.ccd_id; found = true; break;
            }
        }
        if (!found) {
            core.ccd_id = ccd_id_next;
            ccd_groups.push_back({core_l3_mask, core_l3_grp, ccd_id_next++});
        }
    }

    // ── 4. CCX: Intel/ARM → ccx_id = ccd_id  |  AMD Zen 4+ → CPUID 0x80000026 ──
    int regs[4]{};
    __cpuid(regs, 0);
    const bool is_amd = (regs[1] == 0x68747541 &&   // "Auth"
                         regs[3] == 0x69746e65 &&   // "enti"
                         regs[2] == 0x444d4163);    // "cAMD"

    bool ccx_assigned = false;
    if (is_amd) {
        __cpuid(regs, static_cast<int>(0x80000000u));
        const uint32_t max_ext = static_cast<uint32_t>(regs[0]);
        if (max_ext >= 0x80000026u) {
            // Zen 4+: subnivel 0 reporta bits de shift para CCX dentro del APIC ID.
            int r[4]{};
            __cpuidex(r, static_cast<int>(0x80000026u), 0);
            const uint32_t bits_shift = static_cast<uint32_t>(r[0]) & 0xFu;
            if (bits_shift > 0) {
                for (auto& core : cores)
                    core.ccx_id = core.logical_id >> bits_shift;
                ccx_assigned = true;
            }
        }
        // Zen 1/2: CPUID 0x8000001E reporta compute_unit_id en EBX[15:8].
        // No podemos leerlo por core sin pinning → fallback a ccd_id (Zen 3 es monolítico;
        // para Zen 1/2 hay 2 CCX/CCD pero el par en mismo CCD sigue siendo óptimo).
    }
    if (!ccx_assigned) {
        // Fallback universal: dominio coherente = CCD (L3 compartido).
        for (auto& core : cores)
            core.ccx_id = core.ccd_id;
    }
}
#endif // _WIN32

// ── probe_cores_platform_ ────────────────────────────────────────
// Implementación real de detección de cores. Llamada UNA SOLA VEZ
// desde enumerate_cores() vía static cache (thread-safe desde C++11).
static std::vector<CoreInfo> probe_cores_platform_() noexcept {
    std::vector<CoreInfo> cores;

#ifdef _WIN32
    // Obtener conteo y relaciones SMT via GetLogicalProcessorInformationEx.
    DWORD buf_size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &buf_size);
    std::vector<uint8_t> buf(buf_size);
    if (!GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
            &buf_size)) {
        // Fallback: SYSTEM_INFO básico
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        cores.resize(si.dwNumberOfProcessors);
        for (uint32_t i = 0; i < si.dwNumberOfProcessors; ++i)
            cores[i].logical_id = i;
        return cores;
    }

    // Parsear: cada entrada RelationProcessorCore = un core físico con sus SMT siblings.
    uint32_t phys_id = 0;
    DWORD offset = 0;
    while (offset < buf_size) {
        auto* entry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buf.data() + offset);
        if (entry->Relationship == RelationProcessorCore) {
            const KAFFINITY mask = entry->Processor.GroupMask[0].Mask;
            // G8: EfficiencyClass — Intel hybrid (Alder Lake+): 0=E-core, 1+=P-core.
            // En CPUs no-hybrid TODOS reportan 0; post-procesamos para distinguir.
            const uint8_t eff_class = entry->Processor.EfficiencyClass;
            // Extraer logical IDs del bitmask.
            std::vector<uint32_t> siblings;
            for (uint32_t bit = 0; bit < 64; ++bit) {
                if (mask & (1ULL << bit)) siblings.push_back(bit);
            }
            for (uint32_t idx = 0; idx < siblings.size(); ++idx) {
                CoreInfo ci{};
                ci.logical_id       = siblings[idx];
                ci.physical_id      = phys_id;
                ci.efficiency_class = eff_class;  // post-procesado abajo
                ci.is_ht_sibling    = (idx > 0);
                // SMT sibling: para 2-way SMT, el otro elemento.
                if (siblings.size() == 2) {
                    ci.smt_sibling = siblings[1 - idx];
                } else {
                    ci.smt_sibling = UINT32_MAX;
                }
                cores.push_back(ci);
            }
            ++phys_id;
        }
        offset += entry->Size;
    }

    // Ordenar por logical_id para acceso O(1) vía índice.
    std::sort(cores.begin(), cores.end(),
        [](const CoreInfo& a, const CoreInfo& b) {
            return a.logical_id < b.logical_id;
        });

    // G8: Post-proceso EfficiencyClass para Intel hybrid.
    // En CPUs no-hybrid (Intel pre-ADL, AMD, ARM64-W) TODOS los cores reportan
    // EfficiencyClass==0 → no marcamos is_efficiency_core (todos son P-cores).
    // Solo si hay al menos un core con EfficiencyClass>0 confirmamos CPU hybrid.
    {
        const bool is_hybrid = std::any_of(cores.begin(), cores.end(),
            [](const CoreInfo& c){ return c.efficiency_class > 0u; });
        if (!is_hybrid) {
            // Non-hybrid: todos son P-cores conceptualmente, resetear al default.
            for (auto& c : cores) {
                c.efficiency_class   = 1u;
                c.is_efficiency_core = false;
            }
        } else {
            for (auto& c : cores)
                c.is_efficiency_core = (c.efficiency_class == 0u);
        }
    }

    // NUMA assignment via GetLogicalProcessorInformationEx(RelationNumaNode)
    DWORD numa_buf_size = 0;
    GetLogicalProcessorInformationEx(RelationNumaNode, nullptr, &numa_buf_size);
    std::vector<uint8_t> numa_buf(numa_buf_size);
    if (GetLogicalProcessorInformationEx(
            RelationNumaNode,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(numa_buf.data()),
            &numa_buf_size)) {
        DWORD noffset = 0;
        while (noffset < numa_buf_size) {
            auto* nentry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                numa_buf.data() + noffset);
            if (nentry->Relationship == RelationNumaNode) {
                const uint32_t node = nentry->NumaNode.NodeNumber;
                const KAFFINITY mask = nentry->NumaNode.GroupMask.Mask;
                for (auto& core : cores) {
                    if (mask & (1ULL << core.logical_id))
                        core.numa_node = node;
                }
            }
            noffset += nentry->Size;
        }
    }

    // L3, CCD, CCX, V-Cache — funciona para AMD, Intel y cualquier CPU x86 en Windows.
    fill_cache_topology(cores);

    // max_freq_mhz via WMI es costoso; usar CPUID 0x80000007/PowerManagement
    // como heurística o dejarlo en 0 (no crítico para pinning).

#else // POSIX — leer topología real desde sysfs con Graceful Degradation
    // ── helpers sysfs ──────────────────────────────────────────────────────
    auto sysfs_read_str = [](const char* path) -> std::string {
        std::ifstream f(path);
        if (!f.is_open()) return {};
        std::string s; std::getline(f, s); return s;
    };
    auto sysfs_read_u32 = [&sysfs_read_str](const char* path, uint32_t def) -> uint32_t {
        const auto s = sysfs_read_str(path);
        if (s.empty()) return def;
        try { return static_cast<uint32_t>(std::stoul(s)); } catch (...) { return def; }
    };

    const long n_long = sysconf(_SC_NPROCESSORS_ONLN);
    const uint32_t count = (n_long > 0) ? static_cast<uint32_t>(n_long) : 1u;
    char path[192];

    for (uint32_t i = 0; i < count; ++i) {
        CoreInfo ci{};
        ci.logical_id   = i;
        ci.smt_sibling  = UINT32_MAX;

        // Physical core ID (/topology/core_id)
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/cpu/cpu%u/topology/core_id", i);
        ci.physical_id = sysfs_read_u32(path, i);

        // SMT: thread_siblings_list = "0,4" → 2-way SMT; "0" → no SMT.
        // El core con el ID más bajo en la lista es el primario (is_ht_sibling=false).
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", i);
        {
            const std::string sl = sysfs_read_str(path);
            if (!sl.empty()) {
                const auto comma = sl.find(',');
                if (comma != std::string::npos) {
                    // Formato "A,B" (2-way SMT)
                    try {
                        const uint32_t id0 = static_cast<uint32_t>(std::stoul(sl));
                        const uint32_t id1 = static_cast<uint32_t>(
                            std::stoul(sl.substr(comma + 1)));
                        ci.is_ht_sibling = (i != id0);  // primario = el de menor ID
                        ci.smt_sibling   = (i == id0) ? id1 : id0;
                    } catch (...) {}
                }
                // Rangos "A-B" (posible en configs raras): los ignoramos para SMT sibling
                // pero marcamos is_ht_sibling si el primer ID de la lista != i.
                else if (sl.find('-') != std::string::npos) {
                    try {
                        const uint32_t first = static_cast<uint32_t>(std::stoul(sl));
                        ci.is_ht_sibling = (i != first);
                    } catch (...) {}
                }
            }
        }

        // L3 cache: buscar en cache/indexN/ el nivel 3.
        // sysfs reporta size con sufijo 'K' (KB) — stoul se detiene en 'K' → valor en KB.
        for (uint32_t idx = 0; idx < 8; ++idx) {
            std::snprintf(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%u/cache/index%u/level", i, idx);
            if (sysfs_read_u32(path, 0) != 3) continue;

            std::snprintf(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%u/cache/index%u/size", i, idx);
            const std::string sz = sysfs_read_str(path);
            if (!sz.empty()) {
                try {
                    // stoul para en el primer no-dígito; sysfs da "32768K" → 32768 KB.
                    const uint32_t kb = static_cast<uint32_t>(std::stoul(sz));
                    ci.l3_cache_kb = kb;
                    ci.has_v_cache = (kb > 65536u); // >64 MB = AMD 3D V-Cache
                } catch (...) {}
            }
            break;
        }

        cores.push_back(ci);
    }

    // Ordenar por logical_id
    std::sort(cores.begin(), cores.end(),
        [](const CoreInfo& a, const CoreInfo& b){ return a.logical_id < b.logical_id; });

    // ── NUMA: /sys/devices/system/node/nodeN/cpulist ──────────────────────
    for (uint32_t node = 0; node < 64u; ++node) {
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/node/node%u/cpulist", node);
        const std::string cl = sysfs_read_str(path);
        if (cl.empty()) break;
        // Parsear "0-7,16-23" o "0,1,2,3"
        std::stringstream ss(cl);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            const auto dash = tok.find('-');
            try {
                uint32_t lo, hi;
                if (dash == std::string::npos) {
                    lo = hi = static_cast<uint32_t>(std::stoul(tok));
                } else {
                    lo = static_cast<uint32_t>(std::stoul(tok.substr(0, dash)));
                    hi = static_cast<uint32_t>(std::stoul(tok.substr(dash + 1)));
                }
                for (auto& c : cores)
                    if (c.logical_id >= lo && c.logical_id <= hi)
                        c.numa_node = node;
            } catch (...) {}
        }
    }

    // ── CCD: agrupar por (numa_node, l3_cache_kb) → mismo "cluster L3" ───
    // En CPUs multi-CCD (AMD) los CDs tienen diferente L3 o diferente NUMA.
    // En Intel mono-socket todos tienen el mismo L3 → 1 CCD (correcto).
    {
        uint32_t ccd_id_next = 0;
        struct CcdKey { uint32_t numa_node; uint32_t l3_kb; };
        std::vector<std::pair<CcdKey, uint32_t>> ccd_map;
        for (auto& c : cores) {
            CcdKey key{c.numa_node, c.l3_cache_kb};
            bool found = false;
            for (auto& [k, id] : ccd_map) {
                if (k.numa_node == key.numa_node && k.l3_kb == key.l3_kb) {
                    c.ccd_id = id; found = true; break;
                }
            }
            if (!found) {
                c.ccd_id = ccd_id_next;
                ccd_map.push_back({key, ccd_id_next++});
            }
        }
    }

    // ── CCX: AMD Zen 4+ (CPUID 0x80000026); fallback ccx_id = ccd_id ─────
    {
        bool ccx_set = false;
#if defined(__x86_64__) || defined(__i386__)
        {
            unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
            __get_cpuid(0u, &eax, &ebx, &ecx, &edx);
            const bool is_amd = (ebx == 0x68747541u &&
                                  edx == 0x69746e65u &&
                                  ecx == 0x444d4163u);
            if (is_amd) {
                unsigned max_ext = 0;
                __get_cpuid(0x80000000u, &max_ext, &ebx, &ecx, &edx);
                if (max_ext >= 0x80000026u) {
                    __cpuid_count(static_cast<int>(0x80000026u), 0,
                                  eax, ebx, ecx, edx);
                    const uint32_t bits_shift = eax & 0xFu;
                    if (bits_shift > 0) {
                        for (auto& c : cores)
                            c.ccx_id = c.logical_id >> bits_shift;
                        ccx_set = true;
                    }
                }
            }
        }
#endif
        if (!ccx_set) {
            for (auto& c : cores)
                c.ccx_id = c.ccd_id;
        }
    }

    // G8: P/E-core detection via cpufreq (Intel hybrid: Alder Lake+, Meteor Lake).
    // Heurística: E-cores tienen frecuencia máxima significativamente menor que
    // P-cores. Threshold: < 75% de la freq máxima del sistema → E-core.
    //
    // §F-04 fix: parallel freq_khz[] vector for clarity (previously, kHz values
    // were stored temporarily in the max_freq_mhz field, which was confusing
    // and prone to misreading — comparisons were correct but the unit mismatch
    // between the field NAME and the field CONTENT during the comparison phase
    // led at least one audit pass to mis-classify this as a bug).
    {
        // Pass 1: read max_freq into a parallel kHz vector (units explicit).
        std::vector<uint32_t> freq_khz(cores.size(), 0u);
        uint32_t sys_max_khz = 0;
        for (std::size_t i = 0; i < cores.size(); ++i) {
            std::snprintf(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq",
                cores[i].logical_id);
            freq_khz[i] = sysfs_read_u32(path, 0u);
            if (freq_khz[i] > sys_max_khz) sys_max_khz = freq_khz[i];
        }

        if (sys_max_khz > 0) {
            // Pass 2: classify each core. Comparisons explicitly kHz-vs-kHz.
            const uint32_t threshold_khz = (sys_max_khz * 3u) / 4u;
            const bool has_lower = std::any_of(freq_khz.begin(), freq_khz.end(),
                [&](uint32_t f){
                    return f > 0u && f < threshold_khz;
                });

            for (std::size_t i = 0; i < cores.size(); ++i) {
                auto& c = cores[i];
                if (freq_khz[i] > 0u) {
                    const bool is_e = has_lower && (freq_khz[i] < threshold_khz);
                    c.efficiency_class   = is_e ? 0u : 1u;
                    c.is_efficiency_core = is_e;
                    c.max_freq_mhz       = freq_khz[i] / 1000u;  // final MHz
                } else {
                    // No cpufreq info: assume P-core (default efficiency_class=1).
                    c.efficiency_class   = 1u;
                    c.is_efficiency_core = false;
                    // c.max_freq_mhz stays at its CoreInfo default (0).
                }
            }
        }
    }
#endif // POSIX

    return cores;
}

} // anonymous namespace

// ── enumerate_cores ───────────────────────────────────────────────
// Retorna copia del resultado cacheado. Los OS calls (GLPIEX + CPUID)
// corren exactamente UNA VEZ por proceso. Thread-safe: static local
// initialization es atómica desde C++11 (no necesita mutex externo).
std::vector<CoreInfo> enumerate_cores() noexcept {
    static const std::vector<CoreInfo> s_cache = probe_cores_platform_();
    return s_cache;
}

// ── pin_current_thread ────────────────────────────────────────────
bool pin_current_thread(uint32_t logical_id) noexcept {
#ifdef _WIN32
    // R6 (THREAD_PROTECTION_RISK_REGISTER): on > 64-logical-CPU Windows (multi-processor-group
    // systems), `1ULL << logical_id` with logical_id >= 64 is undefined behaviour and a silent
    // mis-pin. Guard it — skip the pin (run unpinned, the caller logs + continues) rather than
    // shift out of range. Single-group consumer CPUs (logical_id < 64) are byte-identical; a
    // SetThreadSelectedCpuSetMasks route for > 64 groups is left as a future hardening.
    if (logical_id >= 64u) return false;
    const DWORD_PTR mask = static_cast<DWORD_PTR>(1ULL << logical_id);
    return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
#else
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(logical_id, &cs);
    return pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs) == 0;
#endif
}

// ── current_core_id ──────────────────────────────────────────────
// Used by SubmitterRegistry + TaskClassifier for CCD-local
// scheduling. Hot path: must be cheap (< 50 ns).
uint32_t current_core_id() noexcept {
#ifdef _WIN32
    // GetCurrentProcessorNumber is a CPUID + syscall on older Windows but
    // since Windows 10 it's an RDTSCP-equivalent (~20 ns). Returns DWORD.
    return static_cast<uint32_t>(::GetCurrentProcessorNumber());
#else
    const int id = ::sched_getcpu();
    return (id < 0) ? UINT32_MAX : static_cast<uint32_t>(id);
#endif
}

// ── core_ccd_id ──────────────────────────────────────────────────
// Maps logical core id → CCD id via the cached topology singleton. UINT32_MAX
// if the id is out of range or topology probing failed. O(1) — direct vector
// index after a single-shot table build on first call.
uint32_t core_ccd_id(uint32_t logical_id) noexcept {
    // Build a static lookup table on first call; subsequent calls are a single
    // bounds-check + array load. Thread-safe via C++11 local-static init.
    static const std::vector<uint32_t> table = []() noexcept {
        std::vector<uint32_t> t;
        auto const& topo = topology();
        if (topo.cores.empty()) return t;
        uint32_t max_lid = 0;
        for (auto const& c : topo.cores)
            if (c.logical_id > max_lid) max_lid = c.logical_id;
        t.assign(max_lid + 1u, UINT32_MAX);
        for (auto const& c : topo.cores) t[c.logical_id] = c.ccd_id;
        return t;
    }();
    if (logical_id >= table.size()) [[unlikely]] return UINT32_MAX;
    return table[logical_id];
}

uint32_t current_core_ccd_id() noexcept {
    const uint32_t lid = current_core_id();
    if (lid == UINT32_MAX) [[unlikely]] return UINT32_MAX;
    return core_ccd_id(lid);
}

// ── elevate_thread_rt ────────────────────────────────────────────
bool elevate_thread_rt(bool time_critical) noexcept {
#ifdef _WIN32
    const int prio = time_critical
        ? THREAD_PRIORITY_TIME_CRITICAL
        : THREAD_PRIORITY_HIGHEST;
    return SetThreadPriority(GetCurrentThread(), prio) != 0;
#else
    struct sched_param sp{};
    sp.sched_priority = time_critical ? 80 : 50;
    // SCHED_FIFO requiere CAP_SYS_NICE o root. Fallback a SCHED_OTHER si falla.
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0)
        return true;
    // Intentar nice(-10) como fallback parcial
    nice(-10);
    return false;
#endif
}

// ── v_cache_cores ────────────────────────────────────────────────
std::vector<uint32_t> v_cache_cores() noexcept {
    auto cores = enumerate_cores();
    std::vector<uint32_t> result;
    for (const auto& c : cores) {
        if (c.has_v_cache) result.push_back(c.logical_id);
    }
    return result;
}

// ── p_cores (G8) ─────────────────────────────────────────────────
// P-cores físicos (efficiency_class >= 1, alto rendimiento).
// En CPUs no-hybrid (Intel pre-ADL, AMD, ARM) retorna todos los cores
// físicos porque el default es efficiency_class=1 (P-core).
std::vector<uint32_t> p_cores() noexcept {
    auto cores = enumerate_cores();
    std::vector<uint32_t> result;
    for (const auto& c : cores) {
        if (!c.is_efficiency_core && !c.is_ht_sibling)
            result.push_back(c.logical_id);
    }
    return result;
}

// ── e_cores (G8) ─────────────────────────────────────────────────
// E-cores físicos (is_efficiency_core == true, bajo consumo/latencia).
// Vector vacío en CPUs no-hybrid (Intel pre-ADL, AMD, ARM, Apple Silicon).
std::vector<uint32_t> e_cores() noexcept {
    auto cores = enumerate_cores();
    std::vector<uint32_t> result;
    for (const auto& c : cores) {
        if (c.is_efficiency_core && !c.is_ht_sibling)
            result.push_back(c.logical_id);
    }
    return result;
}

// ── ccd_cores ────────────────────────────────────────────────────
std::vector<uint32_t> ccd_cores(uint32_t ccd_id) noexcept {
    auto cores = enumerate_cores();
    std::vector<uint32_t> result;
    for (const auto& c : cores) {
        if (c.ccd_id == ccd_id) result.push_back(c.logical_id);
    }
    return result;
}

// ── numa_cores ───────────────────────────────────────────────────
std::vector<uint32_t> numa_cores(uint32_t numa_node) noexcept {
    auto cores = enumerate_cores();
    std::vector<uint32_t> result;
    for (const auto& c : cores) {
        if (c.numa_node == numa_node) result.push_back(c.logical_id);
    }
    return result;
}

// ── numa_node_count ──────────────────────────────────────────────
uint32_t numa_node_count() noexcept {
#ifdef _WIN32
    ULONG highest = 0;
    GetNumaHighestNodeNumber(&highest);
    return static_cast<uint32_t>(highest) + 1u;
#else
    // /sys/devices/system/node/ — contar directorios nodeN
    uint32_t count = 1;
    for (uint32_t n = 1; n < 64; ++n) {
        char path[64];
        std::snprintf(path, sizeof(path), "/sys/devices/system/node/node%u", n);
        if (access(path, F_OK) != 0) break;
        count = n + 1;
    }
    return count;
#endif
}

// ── optimal_producer_consumer_pair ───────────────────────────────
// Selecciona par {prod, cons} optimo para SPSC de baja latencia:
//   Prioridad 1 — mismo CCX, dos cores fisicos distintos (L3 compartido)
//   Prioridad 2 — mismo CCD, distintos cores fisicos
//   Prioridad 3 — cualquier par de cores fisicos distintos (cross-CCD)
//   Prioridad 4 — SMT siblings (ultimo recurso: unico core fisico)
//
// NOTA: los candidatos excluyen HT siblings en las prioridades 1-3
// para garantizar que prod y cons no compartan puertos de ejecucion.
std::pair<uint32_t, uint32_t>
optimal_producer_consumer_pair(bool prefer_v_cache_ccd) noexcept {
    auto cores = enumerate_cores();
    if (cores.empty()) return {0u, 1u};

    // ── Paso 0 (G8): P-core priority pool — Intel hybrid (Alder Lake+) ──
    // Si hay E-cores detectados, restringir el pool de búsqueda a P-cores
    // físicos. Los E-cores tienen ~50% IPC de los P-cores y mayor latencia
    // de coherencia inter-core → son subóptimos para SPSC de baja latencia.
    {
        const bool has_ecores = std::any_of(cores.begin(), cores.end(),
            [](const CoreInfo& c){ return c.is_efficiency_core; });

        if (has_ecores) {
            std::vector<const CoreInfo*> p_phys;
            for (const auto& c : cores)
                if (!c.is_efficiency_core && !c.is_ht_sibling)
                    p_phys.push_back(&c);

            if (p_phys.size() >= 2) {
                // Preferir mismo CCX entre P-cores.
                for (const auto* a : p_phys)
                    for (const auto* b : p_phys)
                        if (a != b && a->ccx_id == b->ccx_id &&
                            a->physical_id != b->physical_id)
                            return {a->logical_id, b->logical_id};
                // Fallback: cualquier par de P-cores físicos distintos.
                for (const auto* a : p_phys)
                    for (const auto* b : p_phys)
                        if (a != b && a->physical_id != b->physical_id)
                            return {a->logical_id, b->logical_id};
                // Último recurso: dos P-cores cualesquiera.
                return {p_phys[0]->logical_id, p_phys[1]->logical_id};
            }
            // Si hay <2 P-cores (sistema muy restringido), caer al flujo normal.
        }
    }

    // ── Paso 1: pool por preferencia de CCD ──────────────────────
    // Solo cores fisicos (no HT siblings) para garantizar aislamiento.
    std::vector<const CoreInfo*> phys;
    if (prefer_v_cache_ccd) {
        for (const auto& c : cores)
            if (c.has_v_cache && !c.is_ht_sibling) phys.push_back(&c);
    }
    if (phys.empty()) {
        // Sin preferencia o sin V-Cache: todos los cores fisicos.
        for (const auto& c : cores)
            if (!c.is_ht_sibling) phys.push_back(&c);
    }
    // Ultimo recurso: incluir HT siblings si no hay cores fisicos detectados.
    if (phys.size() < 2) {
        phys.clear();
        for (const auto& c : cores) phys.push_back(&c);
    }
    if (phys.size() < 2) {
        // Sistema single-core — mejor que podemos hacer.
        return {phys[0]->logical_id, phys[0]->logical_id};
    }

    // ── Paso 2: buscar mismo CCX, cores fisicos distintos ─────────
    for (const auto* a : phys) {
        for (const auto* b : phys) {
            if (a == b) continue;
            if (a->ccx_id == b->ccx_id && a->physical_id != b->physical_id)
                return {a->logical_id, b->logical_id};
        }
    }

    // ── Paso 3: mismo CCD, distintos cores fisicos ────────────────
    for (const auto* a : phys) {
        for (const auto* b : phys) {
            if (a == b) continue;
            if (a->ccd_id == b->ccd_id && a->physical_id != b->physical_id)
                return {a->logical_id, b->logical_id};
        }
    }

    // ── Paso 4: cualquier par de cores fisicos distintos ──────────
    for (const auto* a : phys) {
        for (const auto* b : phys) {
            if (a == b) continue;
            if (a->physical_id != b->physical_id)
                return {a->logical_id, b->logical_id};
        }
    }

    // ── Paso 5: SMT siblings (fallback absoluto) ──────────────────
    return {phys[0]->logical_id, phys[1]->logical_id};
}

// ── detect_system_memory_mb ──────────────────────────────────────
// RAM física total en MiB. Siempre retorna algo válido.
uint32_t detect_system_memory_mb() noexcept {
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return static_cast<uint32_t>(ms.ullTotalPhys / (1024ULL * 1024ULL));
#else
    // /proc/meminfo es la fuente más portable (sysinfo.mem_unit puede ser 1 o 4096).
    FILE* f = std::fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            unsigned long long kb = 0;
            if (std::sscanf(line, "MemTotal: %llu kB", &kb) == 1) {
                std::fclose(f);
                return static_cast<uint32_t>(kb / 1024ULL);
            }
        }
        std::fclose(f);
    }
    // Fallback vía sysinfo(2)
    struct sysinfo si{};
    if (::sysinfo(&si) == 0)
        return static_cast<uint32_t>(
            (static_cast<uint64_t>(si.totalram) * si.mem_unit) / (1024ULL * 1024ULL));
#endif
    return 8192u; // fallback conservador
}

// ── system_busy_fraction ─────────────────────────────────────────────────────
// System-wide CPU busy fraction in [0,1] over a short sampling window. Used by
// the pool's Intent::Auto policy. Blocks ~window_ms. Returns 0.0 on failure.
double system_busy_fraction(uint32_t window_ms) noexcept {
#ifdef _WIN32
    auto totals = [](uint64_t& idle, uint64_t& busy) noexcept -> bool {
        FILETIME fi, fk, fu;
        if (!GetSystemTimes(&fi, &fk, &fu)) return false;
        auto u64 = [](FILETIME f) noexcept {
            return (static_cast<uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
        };
        const uint64_t i = u64(fi), k = u64(fk), us = u64(fu);
        idle = i;
        busy = (k + us) - i;   // kernel time INCLUDES idle → subtract it out
        return true;
    };
    uint64_t i0 = 0, b0 = 0, i1 = 0, b1 = 0;
    if (!totals(i0, b0)) return 0.0;
    std::this_thread::sleep_for(std::chrono::milliseconds(window_ms));
    if (!totals(i1, b1)) return 0.0;
    const uint64_t d_idle = (i1 > i0) ? (i1 - i0) : 0;
    const uint64_t d_busy = (b1 > b0) ? (b1 - b0) : 0;
    const uint64_t d_tot  = d_idle + d_busy;
    return d_tot ? static_cast<double>(d_busy) / static_cast<double>(d_tot) : 0.0;
#else
    auto sample = [](uint64_t& idle, uint64_t& total) noexcept -> bool {
        std::ifstream f("/proc/stat");
        std::string cpu; if (!(f >> cpu) || cpu != "cpu") return false;
        uint64_t v[10] = {0}; int n = 0;
        for (; n < 10 && (f >> v[n]); ++n) {}
        if (n < 5) return false;
        idle  = v[3] + v[4];           // idle + iowait
        total = 0; for (int j = 0; j < n; ++j) total += v[j];
        return true;
    };
    uint64_t i0 = 0, t0 = 0, i1 = 0, t1 = 0;
    if (!sample(i0, t0)) return 0.0;
    std::this_thread::sleep_for(std::chrono::milliseconds(window_ms));
    if (!sample(i1, t1)) return 0.0;
    const uint64_t d_tot  = (t1 > t0) ? (t1 - t0) : 0;
    const uint64_t d_idle = (i1 > i0) ? (i1 - i0) : 0;
    return d_tot ? static_cast<double>(d_tot - d_idle) / static_cast<double>(d_tot) : 0.0;
#endif
}

// ── FR-1: topology() singleton ──────────────────────────────────────────────
// TopologyCache holds the result of a single probe() attempt.
// Constructed once via C++11 function-local static (thread-safe init).
namespace {
struct TopologyCache {
    HardwareTopology topo;
    std::string      err;
};

const TopologyCache& topo_cache_() noexcept {
    static const TopologyCache s_cache = []() noexcept -> TopologyCache {
        TopologyCache tmp;
        auto r = HardwareTopology::probe();
        if (r)  tmp.topo = std::move(*r);
        else    tmp.err  = std::move(r.error());
        return tmp;
    }();
    return s_cache;
}
} // anonymous

const HardwareTopology& topology() noexcept {
    return topo_cache_().topo;
}

std::string_view last_probe_error() noexcept {
    return topo_cache_().err;
}

// ── FR-3: set_process_affinity / get_process_affinity ───────────────────────
#ifdef _WIN32

std::expected<uint64_t, phyriad::Error>
get_process_affinity(uint32_t pid) noexcept {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                           static_cast<DWORD>(pid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }
    DWORD_PTR proc_mask = 0, sys_mask = 0;
    const bool ok = GetProcessAffinityMask(h, &proc_mask, &sys_mask) != 0;
    CloseHandle(h);
    if (!ok) return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    return static_cast<uint64_t>(proc_mask);
}

std::expected<uint64_t, phyriad::Error>
set_process_affinity(uint32_t pid, uint64_t mask) noexcept {
    if (mask == 0ull)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::InvalidArgument});

    HANDLE h = OpenProcess(
        PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, static_cast<DWORD>(pid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }

    DWORD_PTR prev_mask = 0, sys_mask = 0;
    if (!GetProcessAffinityMask(h, &prev_mask, &sys_mask)) {
        CloseHandle(h);
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    }

    const bool ok = SetProcessAffinityMask(
        h, static_cast<DWORD_PTR>(mask)) != 0;
    CloseHandle(h);
    if (!ok) return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    return static_cast<uint64_t>(prev_mask);
}

#else // POSIX ──────────────────────────────────────────────────────────────

std::expected<uint64_t, phyriad::Error>
get_process_affinity(uint32_t pid) noexcept {
    cpu_set_t cs;
    CPU_ZERO(&cs);
    if (sched_getaffinity(static_cast<pid_t>(pid), sizeof(cs), &cs) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM ? phyriad::ErrorCode::PermissionDenied
                           : phyriad::ErrorCode::InvalidArgument});
    }
    uint64_t mask = 0ull;
    for (uint32_t i = 0u; i < 64u; ++i)
        if (CPU_ISSET(i, &cs)) mask |= (1ull << i);
    return mask;
}

std::expected<uint64_t, phyriad::Error>
set_process_affinity(uint32_t pid, uint64_t mask) noexcept {
    if (mask == 0ull)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::InvalidArgument});

    // Read previous mask first.
    cpu_set_t prev_cs;
    CPU_ZERO(&prev_cs);
    if (sched_getaffinity(static_cast<pid_t>(pid), sizeof(prev_cs), &prev_cs) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM ? phyriad::ErrorCode::PermissionDenied
                           : phyriad::ErrorCode::InvalidArgument});
    }

    cpu_set_t new_cs;
    CPU_ZERO(&new_cs);
    for (uint32_t i = 0u; i < 64u; ++i)
        if (mask & (1ull << i)) CPU_SET(i, &new_cs);

    if (sched_setaffinity(static_cast<pid_t>(pid), sizeof(new_cs), &new_cs) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM ? phyriad::ErrorCode::PermissionDenied
                           : phyriad::ErrorCode::IoError});
    }

    uint64_t prev_mask = 0ull;
    for (uint32_t i = 0u; i < 64u; ++i)
        if (CPU_ISSET(i, &prev_cs)) prev_mask |= (1ull << i);
    return prev_mask;
}

#endif // _WIN32 / POSIX

// ── FR-9: set_process_priority / get_process_priority ──────────────────────
#ifdef _WIN32

std::expected<uint32_t, phyriad::Error>
get_process_priority(uint32_t pid) noexcept {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                           static_cast<DWORD>(pid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }
    const DWORD cls = GetPriorityClass(h);
    CloseHandle(h);
    if (cls == 0)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    return static_cast<uint32_t>(cls);
}

std::expected<uint32_t, phyriad::Error>
set_process_priority(uint32_t pid, uint32_t priority_class) noexcept {
    // GetPriorityClass returns 0 on failure; reject 0 as sentinel per FR-9 spec.
    if (priority_class == 0u)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::InvalidArgument});

    HANDLE h = OpenProcess(
        PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, static_cast<DWORD>(pid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }

    // Read previous priority class before modifying.
    const DWORD prev = GetPriorityClass(h);
    if (prev == 0) {
        CloseHandle(h);
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    }

    const bool ok = SetPriorityClass(h, static_cast<DWORD>(priority_class)) != 0;
    CloseHandle(h);
    if (!ok) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::IoError});
    }
    return static_cast<uint32_t>(prev);
}

#else // POSIX ──────────────────────────────────────────────────────────────

// POSIX: priority_class is treated as a raw nice value (cast to int32_t).
// getpriority/setpriority are declared in <sys/resource.h> (included at top).
std::expected<uint32_t, phyriad::Error>
get_process_priority(uint32_t pid) noexcept {
    errno = 0;
    const int prio = getpriority(PRIO_PROCESS, static_cast<id_t>(pid));
    if (prio == -1 && errno != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM || errno == EACCES ? phyriad::ErrorCode::PermissionDenied
                                              : phyriad::ErrorCode::InvalidArgument});
    }
    // Nice values range -20..19; store as uint32_t by bit-casting int32_t.
    return static_cast<uint32_t>(static_cast<int32_t>(prio));
}

std::expected<uint32_t, phyriad::Error>
set_process_priority(uint32_t pid, uint32_t priority_class) noexcept {
    // Read previous nice value first.
    errno = 0;
    const int prev = getpriority(PRIO_PROCESS, static_cast<id_t>(pid));
    if (prev == -1 && errno != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM || errno == EACCES ? phyriad::ErrorCode::PermissionDenied
                                              : phyriad::ErrorCode::InvalidArgument});
    }

    const int new_nice = static_cast<int>(static_cast<int32_t>(priority_class));
    if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), new_nice) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM || errno == EACCES ? phyriad::ErrorCode::PermissionDenied
                                              : phyriad::ErrorCode::IoError});
    }
    return static_cast<uint32_t>(static_cast<int32_t>(prev));
}

#endif // _WIN32 / POSIX

// ── GFR-Ayama-2: set_thread_affinity / get_thread_affinity ─────────────────
// Thread-level affinity (per-TID). Symmetric to FR-3 process-level.
// Enables differential thread pinning: place the critical-path thread on
// one core cluster (e.g. V-Cache CCD, P-cores) while leaving the worker
// threads on the secondary cluster. See apps/ayama/ for a concrete
// consumer of this primitive.
#ifdef _WIN32

std::expected<uint64_t, phyriad::Error>
get_thread_affinity(uint32_t tid) noexcept {
    // Windows quirk: there's no direct GetThreadAffinityMask. We must
    // SET the same affinity twice to read it (SetThreadAffinityMask returns
    // the previous mask). The "no-op set" trick is the documented way.
    HANDLE h = OpenThread(THREAD_SET_INFORMATION
                          | THREAD_QUERY_LIMITED_INFORMATION,
                          FALSE, static_cast<DWORD>(tid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }

    // Snapshot the system affinity to use as a "harmless write" target.
    // The thread is guaranteed allowed to run on the union of system-
    // accessible cores, so setting to that mask doesn't change behaviour.
    DWORD_PTR proc_mask = 0, sys_mask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask)) {
        CloseHandle(h);
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    }
    if (sys_mask == 0u) {
        CloseHandle(h);
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::SystemError});
    }

    // First call: set to system mask, get prev (the actual current value).
    const DWORD_PTR prev_for_read = SetThreadAffinityMask(h, sys_mask);
    if (prev_for_read == 0u) {
        CloseHandle(h);
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    }
    // Second call: restore the actual current value.
    (void)SetThreadAffinityMask(h, prev_for_read);
    CloseHandle(h);
    return static_cast<uint64_t>(prev_for_read);
}

std::expected<uint64_t, phyriad::Error>
set_thread_affinity(uint32_t tid, uint64_t mask) noexcept {
    if (mask == 0ull)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::InvalidArgument});

    HANDLE h = OpenThread(THREAD_SET_INFORMATION
                          | THREAD_QUERY_LIMITED_INFORMATION,
                          FALSE, static_cast<DWORD>(tid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }

    const DWORD_PTR prev = SetThreadAffinityMask(
        h, static_cast<DWORD_PTR>(mask));
    CloseHandle(h);
    if (prev == 0u)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    return static_cast<uint64_t>(prev);
}

#else // POSIX ──────────────────────────────────────────────────────────────

std::expected<uint64_t, phyriad::Error>
get_thread_affinity(uint32_t tid) noexcept {
    cpu_set_t cs;
    CPU_ZERO(&cs);
    // Linux: sched_getaffinity accepts a TID directly (kernel-level TID,
    // not pthread_t). On other POSIX systems this may be portable subset.
    if (sched_getaffinity(static_cast<pid_t>(tid), sizeof(cs), &cs) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM ? phyriad::ErrorCode::PermissionDenied
                           : phyriad::ErrorCode::InvalidArgument});
    }
    uint64_t mask = 0ull;
    for (uint32_t i = 0u; i < 64u; ++i)
        if (CPU_ISSET(i, &cs)) mask |= (1ull << i);
    return mask;
}

std::expected<uint64_t, phyriad::Error>
set_thread_affinity(uint32_t tid, uint64_t mask) noexcept {
    if (mask == 0ull)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::InvalidArgument});

    cpu_set_t prev_cs;
    CPU_ZERO(&prev_cs);
    if (sched_getaffinity(static_cast<pid_t>(tid), sizeof(prev_cs), &prev_cs) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM ? phyriad::ErrorCode::PermissionDenied
                           : phyriad::ErrorCode::InvalidArgument});
    }

    cpu_set_t new_cs;
    CPU_ZERO(&new_cs);
    for (uint32_t i = 0u; i < 64u; ++i)
        if (mask & (1ull << i)) CPU_SET(i, &new_cs);

    if (sched_setaffinity(static_cast<pid_t>(tid), sizeof(new_cs), &new_cs) != 0) {
        return std::unexpected(phyriad::Error{
            errno == EPERM ? phyriad::ErrorCode::PermissionDenied
                           : phyriad::ErrorCode::IoError});
    }

    uint64_t prev_mask = 0ull;
    for (uint32_t i = 0u; i < 64u; ++i)
        if (CPU_ISSET(i, &prev_cs)) prev_mask |= (1ull << i);
    return prev_mask;
}

#endif // _WIN32 / POSIX

// ── GFR-Ayama-4: set_thread_ideal_processor / get_thread_ideal_processor ───
// Soft scheduling hint (preferred core) — complements set_thread_affinity
// for "pin to CCD + prefer specific core within CCD" patterns.
#ifdef _WIN32

std::expected<uint32_t, phyriad::Error>
get_thread_ideal_processor(uint32_t tid) noexcept {
    // Same trick as get_thread_affinity: SetThreadIdealProcessorEx returns
    // the previous, so a no-op set reveals the current value.
    HANDLE h = OpenThread(THREAD_SET_INFORMATION
                          | THREAD_QUERY_LIMITED_INFORMATION,
                          FALSE, static_cast<DWORD>(tid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }

    // To read without side effects, query the thread's current group and
    // ideal processor. There's no GetThreadIdealProcessor — we use the
    // "set to a known value and immediately set back" trick.
    PROCESSOR_NUMBER current{};
    current.Group = 0;
    current.Number = 0;  // CPU 0, group 0 — safe default for "read"
    PROCESSOR_NUMBER prev{};
    BOOL ok = SetThreadIdealProcessorEx(h, &current, &prev);
    if (!ok) {
        CloseHandle(h);
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::IoError});
    }
    // Restore.
    (void)SetThreadIdealProcessorEx(h, &prev, nullptr);
    CloseHandle(h);

    const uint32_t prev_id =
        static_cast<uint32_t>(prev.Group) * 64u +
        static_cast<uint32_t>(prev.Number);
    return prev_id;
}

std::expected<uint32_t, phyriad::Error>
set_thread_ideal_processor(uint32_t tid, uint32_t logical_id) noexcept {
    // Validate range against topology. Number field is BYTE — must fit in [0,63]
    // per group. The hardcoded 64-per-group covers all current Windows configs.
    if (logical_id >= 64u * 64u)  // 4096 logical CPUs max (huge headroom)
        return std::unexpected(phyriad::Error{phyriad::ErrorCode::InvalidArgument});

    HANDLE h = OpenThread(THREAD_SET_INFORMATION
                          | THREAD_QUERY_LIMITED_INFORMATION,
                          FALSE, static_cast<DWORD>(tid));
    if (!h) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_ACCESS_DENIED ? phyriad::ErrorCode::PermissionDenied
                                     : phyriad::ErrorCode::InvalidArgument});
    }

    PROCESSOR_NUMBER desired{};
    desired.Group  = static_cast<WORD>(logical_id / 64u);
    desired.Number = static_cast<BYTE>(logical_id % 64u);

    PROCESSOR_NUMBER prev{};
    const BOOL ok = SetThreadIdealProcessorEx(h, &desired, &prev);
    CloseHandle(h);
    if (!ok) {
        const DWORD e = GetLastError();
        return std::unexpected(phyriad::Error{
            e == ERROR_INVALID_PARAMETER ? phyriad::ErrorCode::InvalidArgument
                                         : phyriad::ErrorCode::IoError});
    }

    const uint32_t prev_id =
        static_cast<uint32_t>(prev.Group) * 64u +
        static_cast<uint32_t>(prev.Number);
    return prev_id;
}

#else // POSIX ──────────────────────────────────────────────────────────────

std::expected<uint32_t, phyriad::Error>
get_thread_ideal_processor(uint32_t /*tid*/) noexcept {
    // No exact equivalent on POSIX. sched_getaffinity returns a SET, not
    // a preferred core. Return Unavailable to be honest.
    return std::unexpected(phyriad::Error{phyriad::ErrorCode::Unavailable});
}

std::expected<uint32_t, phyriad::Error>
set_thread_ideal_processor(uint32_t /*tid*/, uint32_t /*logical_id*/) noexcept {
    return std::unexpected(phyriad::Error{phyriad::ErrorCode::Unavailable});
}

#endif // _WIN32 / POSIX

// ── THREAD_PROTECTION S2: MMCSS (Multimedia Class Scheduler Service) ──────────
// Per-thread, OS-sanctioned bounded priority boost via the AVRT API. ONLY the
// per-thread API is used — NEVER the global SystemResponsiveness registry value,
// NEVER REALTIME_PRIORITY_CLASS, NEVER the "Games" task (master-plan §2.1).
#ifdef _WIN32

void MmcssToken::leave() noexcept {
    if (handle_) {
        // AvRevertMmThreadCharacteristics is per-thread: this runs on the same
        // thread that joined (the token is held thread-local by the caller).
        (void)AvRevertMmThreadCharacteristics(static_cast<HANDLE>(handle_));
        handle_     = nullptr;
        task_index_ = 0;
    }
}

MmcssToken join_mmcss_task(MmcssTask task, AvrtPriority prio) noexcept {
    // Map the task enum → the Win32 task-name string (use the W variant explicitly
    // so the call is deterministic regardless of the UNICODE macro state).
    const wchar_t* name = L"Pro Audio";
    switch (task) {
        case MmcssTask::ProAudio: name = L"Pro Audio"; break;
        case MmcssTask::Capture:  name = L"Capture";   break;
        case MmcssTask::Playback: name = L"Playback";  break;
    }

    DWORD  idx = 0;  // MUST be 0 on the call; receives a task identifier on success.
    HANDLE h   = AvSetMmThreadCharacteristicsW(name, &idx);
    if (!h) {
        // MMCSS disabled (SystemResponsiveness=100), service stopped, or a future
        // SKU → inactive token; the caller falls back to elevate_thread_rt().
        return MmcssToken{};
    }

    if (prio != AvrtPriority::None) {
        AVRT_PRIORITY ap = AVRT_PRIORITY_NORMAL;
        switch (prio) {
            case AvrtPriority::Normal:   ap = AVRT_PRIORITY_NORMAL;   break;
            case AvrtPriority::High:     ap = AVRT_PRIORITY_HIGH;     break;
            case AvrtPriority::Critical: ap = AVRT_PRIORITY_CRITICAL; break;
            case AvrtPriority::None:     break;  // unreachable (guarded above)
        }
        // Best-effort: on failure we keep the task membership (still a boost) and
        // do not tear down — the join itself succeeded.
        (void)AvSetMmThreadPriority(h, ap);
    }
    return MmcssToken{ static_cast<void*>(h), static_cast<unsigned long>(idx) };
}

#else // POSIX / non-Windows ─────────────────────────────────────────────────

void MmcssToken::leave() noexcept {
    // No MMCSS off Windows; the token is always inactive (handle_ stays null).
    handle_     = nullptr;
    task_index_ = 0;
}

MmcssToken join_mmcss_task(MmcssTask /*task*/, AvrtPriority /*prio*/) noexcept {
    // Inactive token — caller falls back to elevate_thread_rt() (SCHED_FIFO/nice).
    return MmcssToken{};
}

#endif // _WIN32

} // namespace hw

} // namespace phyriad
// Made with my soul - Swately <3
