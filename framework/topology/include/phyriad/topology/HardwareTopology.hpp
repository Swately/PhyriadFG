// framework/topology/include/phyriad/topology/HardwareTopology.hpp
// Hardware topology detection: cores, NUMA, GPU, cache, and ISA features.
//
// Provides:
//   CoreInfo       — full per-logical-core descriptor (SMT, CCX, NUMA, freq)
//   NumaNode       — NUMA node with memory info and logical core IDs
//   GpuInfo        — GPU descriptor (CUDA, display, VRAM)
//   HardwareTopology — aggregate result of hw::probe()
//
// hw:: namespace API (all noexcept, singletons cached on first call):
//   enumerate_cores()              → all logical cores with topology attributes
//   pin_current_thread(id)         → SetThreadAffinityMask / pthread_setaffinity_np
//   elevate_thread_rt(rt)          → SCHED_FIFO / TIME_CRITICAL
//   v_cache_cores()                → AMD 3D V-Cache core IDs
//   p_cores() / e_cores()          → Intel P/E-core pools (or all cores on non-hybrid)
//   ccd_cores(ccd_id)              → cores sharing an L3 domain
//   numa_cores(numa_node)          → cores in a NUMA node
//   numa_node_count()              → number of NUMA nodes
//   optimal_producer_consumer_pair → best SPSC pair (same CCX preferred)
//   detect_system_memory_mb()      → physical RAM in MiB
//
// PerformanceProfile integration (make_auto_profile, apply_profile_hints):
//   Implemented in pillars/runtime — runtime depends on topology AND schema.
//

#pragma once
#include "CacheInfo.hpp"
#include "CpuFeatures.hpp"
#include "VCacheDetector.hpp"
#include "HybridCoreDetector.hpp"
#include "PCIeAffinityProbe.hpp"
#include <phyriad/schema/Error.hpp>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace phyriad {

// ── CoreInfo — description of a single logical core ──────────────────────────
struct CoreInfo {
    uint32_t logical_id{UINT32_MAX};   // OS index (SetThreadAffinityMask)
    uint32_t physical_id{UINT32_MAX};  // physical core (logical / SMT_siblings)
    uint32_t smt_sibling{UINT32_MAX};  // other logical on the same physical;
                                       // UINT32_MAX if no SMT or unknown
    uint32_t ccx_id{0};               // AMD: Core Complex
    uint32_t ccd_id{0};               // AMD: Chiplet Die (Zen2+)
    uint32_t numa_node{0};
    uint32_t l3_cache_kb{0};          // effective L3 visible from this core (KB)
    uint32_t max_freq_mhz{0};         // max boost frequency (MHz)
    bool     has_v_cache{false};      // AMD 3D V-Cache (CCD0 on 7950X3D: 96 MB L3)
    bool     is_ht_sibling{false};    // true → HyperThreading / SMT twin
    // Intel hybrid (Alder Lake+, Meteor Lake, Raptor Lake, etc.)
    // Default = 1 (P-core) on non-hybrid CPUs (Intel pre-ADL, AMD, ARM).
    uint8_t  efficiency_class{1};      // 0=E-core, 1+=P-core
    bool     is_efficiency_core{false}; // true = E-core; always false on non-hybrid
};
// Pure POD layout: must fit in a cacheline for fast topology iteration and
// be copyable via memcpy when published via shared memory (IPC consumers,
// out-of-process telemetry publishers).
static_assert(std::is_trivially_copyable_v<CoreInfo>,
              "CoreInfo must be trivially copyable");
static_assert(std::is_standard_layout_v<CoreInfo>,
              "CoreInfo must be standard layout (POD)");
static_assert(sizeof(CoreInfo) <= 64,
              "CoreInfo should fit in a single cacheline");

// ── NumaNode ─────────────────────────────────────────────────────────────────
struct NumaNode {
    uint32_t              id;
    uint64_t              total_memory_bytes;
    uint64_t              free_memory_bytes;
    std::vector<uint32_t> logical_core_ids;
};

// ── GpuInfo ──────────────────────────────────────────────────────────────────
struct GpuInfo {
    uint32_t    device_index;
    std::string name;
    uint64_t    vram_total_bytes;
    uint64_t    vram_free_bytes;
    bool        cuda_capable;
    bool        display_capable;

    // CUDA fields (valid only when cuda_capable == true)
    int         sm_major{0};
    int         sm_minor{0};
    int         sm_count{0};
    int         max_threads_per_sm{0};
    int         clock_rate_khz{0};
    int         mem_clock_khz{0};
    int         mem_bus_width_bits{0};
    int         l2_cache_bytes{0};
    bool        ecc_enabled{false};
    bool        unified_addressing{false};
    bool        managed_memory{false};

    [[nodiscard]] std::string arch_flag() const noexcept {
        if (!cuda_capable || sm_major == 0) return {};
        return "sm_" + std::to_string(sm_major * 10 + sm_minor);
    }

    [[nodiscard]] double mem_bandwidth_gbps() const noexcept {
        if (mem_clock_khz == 0 || mem_bus_width_bits == 0) return 0.0;
        return 2.0 * (mem_clock_khz / 1.0e6) * (mem_bus_width_bits / 8.0);
    }
};

// ── HardwareTopology ─────────────────────────────────────────────────────────
struct HardwareTopology {
    std::vector<CoreInfo>  cores;
    std::vector<NumaNode>  numa_nodes;
    std::vector<GpuInfo>   gpus;
    CacheInfo              cache{};
    uint64_t               total_ram_bytes{0};
    uint64_t               free_ram_bytes{0};

    // ISA feature flags detected via CPUID. Zero on non-x86.
    topology::X86Features                  cpu_features{};
    // Sorted logical IDs of cores that carry AMD 3D V-Cache (L3 >= 96 MB).
    std::vector<uint32_t>                  vcache_cores;
    // One entry per online logical core: Performance vs Efficiency classification.
    std::vector<topology::CoreTypeEntry>   core_types;
    // All enumerated PCIe devices with their NUMA node affinity.
    std::vector<topology::PCIeDevice>      pcie_affinity_map;

    [[nodiscard]] static std::expected<HardwareTopology, std::string> probe() noexcept;

    [[nodiscard]] uint32_t physical_core_count() const noexcept;
    [[nodiscard]] uint32_t logical_core_count()  const noexcept;

    /// Number of distinct CCD/CCX IDs present in `cores`.
    /// Returns 0 if cores is empty, 1 for single-CCD CPUs.
    /// Computed once during probe() — O(1) thereafter.
    [[nodiscard]] uint32_t ccd_count() const noexcept { return ccd_count_; }

    [[nodiscard]] std::vector<uint32_t>
    physical_cores_on_numa(uint32_t node_id) const noexcept;

    [[nodiscard]] std::vector<uint32_t>
    allocate_cores(uint32_t count, uint32_t prefer_numa = UINT32_MAX) const noexcept;

private:
    /// Populated by probe(). Equals max(cores[].ccd_id) + 1, or 0 if cores empty.
    uint32_t ccd_count_{0u};
};

// ── hw:: — hardware affinity API ─────────────────────────────────────────────
// All functions are noexcept. Singleton results are cached on first call.
// Graceful degradation: works on any x86 CPU (Intel, AMD) and ARM64-Windows.
namespace hw {

// ── FR-1: topology() singleton accessor ──────────────────────────────────────
// Returns the process-wide cached HardwareTopology.
//   First call: invokes HardwareTopology::probe() and caches the result.
//   On probe failure: returns a sentinel empty topology (cores.empty() == true).
//   Subsequent calls: O(1) — no OS calls.
// Thread-safe via C++11 function-local static guarantee.
[[nodiscard]] const HardwareTopology& topology() noexcept;

// Returns the error string from the last probe attempt.
// Empty string_view means the probe succeeded.
[[nodiscard]] std::string_view last_probe_error() noexcept;

// Enumerate all logical cores with their topology attributes.
// Singleton cached: OS calls execute ONCE per process. Thread-safe (C++11 static).
[[nodiscard]] std::vector<CoreInfo> enumerate_cores() noexcept;

// Pin the current thread to the given logical core.
// Windows: SetThreadAffinityMask. Linux: pthread_setaffinity_np.
[[nodiscard]] bool pin_current_thread(uint32_t logical_id) noexcept;

// Logical id of the core currently running the calling thread.
// Returns UINT32_MAX if the platform query fails. Cheap on Linux
// (sched_getcpu, ~30 ns) and on Windows (GetCurrentProcessorNumber, ~20 ns).
// Used by per-CCD scheduling paths (Phase O1.4) to route work locally.
[[nodiscard]] uint32_t current_core_id() noexcept;

// CCD (Chiplet Die / L3 domain) id of the core currently running the calling
// thread. Returns UINT32_MAX if topology probing failed. Cached lookup table
// built on first call. Used by SubmitterRegistry / TaskClassifier for
// CCD-local task affinity.
[[nodiscard]] uint32_t current_core_ccd_id() noexcept;

// CCD id of a specific logical core. UINT32_MAX if topology lookup fails.
[[nodiscard]] uint32_t core_ccd_id(uint32_t logical_id) noexcept;

// Elevate the current thread to real-time priority.
// Windows: SetThreadPriority(TIME_CRITICAL). Linux: SCHED_FIFO prio=80 (needs CAP_SYS_NICE).
[[nodiscard]] bool elevate_thread_rt(bool time_critical = true) noexcept;

// Logical IDs of cores with AMD 3D V-Cache. Empty on non-X3D / non-AMD.
[[nodiscard]] std::vector<uint32_t> v_cache_cores() noexcept;

// Intel hybrid core pools.
// p_cores(): physical P-cores (efficiency_class >= 1). All cores on non-hybrid.
// e_cores(): physical E-cores (is_efficiency_core == true). Empty on non-hybrid.
[[nodiscard]] std::vector<uint32_t> p_cores() noexcept;
[[nodiscard]] std::vector<uint32_t> e_cores() noexcept;

// Logical IDs of cores sharing the given L3 cache domain (CCD).
[[nodiscard]] std::vector<uint32_t> ccd_cores(uint32_t ccd_id) noexcept;

// Logical IDs of cores in the given NUMA node.
[[nodiscard]] std::vector<uint32_t> numa_cores(uint32_t numa_node) noexcept;

// Number of NUMA nodes in the system (minimum 1).
[[nodiscard]] uint32_t numa_node_count() noexcept;

// Optimal {producer_core, consumer_core} pair for low-latency SPSC.
// Priority: same CCX → same CCD → any distinct physical cores → SMT siblings.
// If prefer_v_cache_ccd=true and V-Cache cores exist, restrict to that CCD.
[[nodiscard]] std::pair<uint32_t, uint32_t>
optimal_producer_consumer_pair(bool prefer_v_cache_ccd) noexcept;

// Physical RAM in MiB. Always returns something (fallback: 8192 MiB).
[[nodiscard]] uint32_t detect_system_memory_mb() noexcept;

// System-wide CPU busy fraction in [0.0, 1.0], sampled over `window_ms`. Used by
// the pool's Intent::Auto policy to tell whether the machine is already loaded by
// OTHER work (→ assist mode, oversubscribe via SMT to fill the gaps) or mostly
// idle (→ owner mode, one worker per physical core). Blocks for ~window_ms while
// sampling. Windows: GetSystemTimes; Linux: /proc/stat. Returns 0.0 on query
// failure (conservative → owner mode).
[[nodiscard]] double system_busy_fraction(uint32_t window_ms = 15u) noexcept;

// ── FR-3: process-level affinity ─────────────────────────────────────────────
// Set/get the CPU affinity mask of an arbitrary process (cross-platform).
//
// Windows: requires PROCESS_SET_INFORMATION — usually needs admin for other users.
// Linux:   requires CAP_SYS_NICE (or root) for processes owned by other users.
//
// set_process_affinity: returns the previous mask on success so the caller can
//   trivially revert the change.  mask == 0 is rejected (InvalidArgument).
// get_process_affinity: reads the current mask without modifying it.
//
// Both functions cover up to 64 logical cores (uint64_t bitmask).
[[nodiscard]] std::expected<uint64_t, phyriad::Error>
set_process_affinity(uint32_t pid, uint64_t mask) noexcept;

[[nodiscard]] std::expected<uint64_t, phyriad::Error>
get_process_affinity(uint32_t pid) noexcept;

// ── FR-9: process-level priority ─────────────────────────────────────────────
// Set/get the scheduling priority of an arbitrary process (cross-platform).
//
// Windows: priority_class is a Win32 priority-class constant:
//   IDLE_PRIORITY_CLASS (0x40), BELOW_NORMAL_PRIORITY_CLASS (0x4000),
//   NORMAL_PRIORITY_CLASS (0x20), ABOVE_NORMAL_PRIORITY_CLASS (0x8000),
//   HIGH_PRIORITY_CLASS (0x80), REALTIME_PRIORITY_CLASS (0x100).
//   Requires PROCESS_SET_INFORMATION — usually needs admin for elevated targets.
// POSIX:   priority_class is treated as a raw nice value (−20..19 cast to int32_t).
//
// set_process_priority: returns the previous priority_class on success.
//   Returns InvalidArgument if priority_class == 0 (Win32 sentinel for failure).
// get_process_priority: reads the current priority without modifying it.
[[nodiscard]] std::expected<uint32_t, phyriad::Error>
set_process_priority(uint32_t pid, uint32_t priority_class) noexcept;

[[nodiscard]] std::expected<uint32_t, phyriad::Error>
get_process_priority(uint32_t pid) noexcept;

// ── GFR-Ayama-2: thread-level affinity ──────────────────────────────────────
// Set/get the CPU affinity mask of an arbitrary thread by TID (cross-platform).
//
// Symmetric counterpart to FR-3 `set_process_affinity` but operates at the
// thread granularity. Use case: "differential thread pinning" — keeping the
// hot critical-path thread on a high-throughput core cluster (e.g. AMD
// V-Cache CCD, Intel P-cores) while letting worker threads stay on the
// secondary cluster. Requires per-thread control, not process-wide.
// See apps/ayama/ for a concrete consumer implementing this pattern.
//
// Windows: requires THREAD_SET_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION
//          via OpenThread. Cross-process typically needs admin.
// Linux:   sched_setaffinity(tid, ...) — the `tid` here is the kernel TID
//          (gettid() result), not pthread_t. Requires CAP_SYS_NICE for
//          threads owned by other processes.
//
// set_thread_affinity: returns the previous mask on success so the caller can
//   trivially revert.  mask == 0 is rejected (InvalidArgument) — would
//   suspend the thread permanently, never a legitimate use case.
// get_thread_affinity: reads the current mask without modifying it.
//
// Both functions cover up to 64 logical cores (uint64_t bitmask). For
// systems with > 64 logical CPUs (Windows processor groups), the mask
// applies to the thread's current group only.
[[nodiscard]] std::expected<uint64_t, phyriad::Error>
set_thread_affinity(uint32_t tid, uint64_t mask) noexcept;

[[nodiscard]] std::expected<uint64_t, phyriad::Error>
get_thread_affinity(uint32_t tid) noexcept;

// ── GFR-Ayama-4: thread-level ideal processor (soft scheduling hint) ────────
// Set/get the IDEAL processor for a thread. Unlike `set_thread_affinity`
// (hard constraint — thread CAN ONLY run on the masked cores), the ideal
// processor is a soft hint: the scheduler PREFERS this core but may move
// the thread temporarily if it's saturated.
//
// Use case: combine `set_thread_affinity` (pin to CCD mask) with
// `set_thread_ideal_processor` (prefer specific core within CCD) for both
// isolation AND deterministic placement, while keeping scheduler flexibility
// if the preferred core is busy. See apps/ayama/ for a worked example
// applying this pattern to game-thread pinning on asymmetric CPUs.
//
// `logical_id` is a flat OS index (0..logical_core_count()-1). On Windows
// systems with > 64 logical CPUs, the implementation maps to PROCESSOR_NUMBER
// (group + within-group index). Out-of-range values return InvalidArgument.
//
// To "revert" to a previous ideal processor, the caller stores the returned
// prev value from a prior set_thread_ideal_processor call and passes it back
// (symmetric with set_thread_affinity revert pattern).
//
// Windows: SetThreadIdealProcessorEx (Vista+). The non-Ex variant is
//          deprecated and limited to 64 CPUs single-group.
// Linux:   no exact equivalent. Returns Unavailable.
[[nodiscard]] std::expected<uint32_t, phyriad::Error>
set_thread_ideal_processor(uint32_t tid, uint32_t logical_id) noexcept;

[[nodiscard]] std::expected<uint32_t, phyriad::Error>
get_thread_ideal_processor(uint32_t tid) noexcept;

// ── THREAD_PROTECTION S2: MMCSS (Multimedia Class Scheduler Service) ──────────
// Per-thread, OS-sanctioned BOUNDED priority boost via the AVRT API. Unlike a
// raw SetThreadPriority(TIME_CRITICAL), MMCSS gives the two properties raw
// priority lacks under contention (both Microsoft-documented): (1) the
// SystemResponsiveness reserve — a fraction of CPU guaranteed to non-MMCSS /
// low-priority work, and (2) consumption-based SELF-DEMOTION of an over-consuming
// thread into the demoted band. That makes it a boost that CANNOT starve the box.
// Windows-only; on non-Windows the token is always inactive (caller falls back to
// elevate_thread_rt). See THREAD_PROTECTION_MASTER_PLAN.md §2.1.
//
// HARD RULES (master-plan §2.1): use ONLY the per-thread AVRT API; NEVER
// read/write the global SystemResponsiveness registry value; NEVER the "Games"
// task (Windows keeps it LOW); NEVER REALTIME_PRIORITY_CLASS.

// MMCSS task category. Maps to the Win32 task-name string the service ships with.
enum class MmcssTask : uint8_t {
    ProAudio = 0,   // "Pro Audio" — strongest (priority band ~23-26)
    Capture  = 1,   // "Capture"   — medium    (priority band ~16-22)
    Playback = 2,   // "Playback"  — medium    (priority band ~16-22)
};

// Relative priority WITHIN the joined MMCSS task (AvSetMmThreadPriority).
// None = leave the task's default priority (do NOT call AvSetMmThreadPriority).
enum class AvrtPriority : uint8_t {
    None     = 0,   // do not set a relative priority
    Normal   = 1,   // AVRT_PRIORITY_NORMAL
    High     = 2,   // AVRT_PRIORITY_HIGH
    Critical = 3,   // AVRT_PRIORITY_CRITICAL
};

// RAII token for a joined MMCSS task. Move-only. Holds the AVRT handle + the
// task-index out-param. The destructor (or an explicit leave()) calls
// AvRevertMmThreadCharacteristics — which is per-thread, so the token MUST live
// on, and be destroyed by, the SAME thread that joined (store it thread-local in
// the thread's loop body). active()==false means the join failed / the platform
// has no MMCSS → the caller falls back to elevate_thread_rt().
//
// The handle is held as void* (and the task index as unsigned long) so this
// public header pulls in NEITHER <windows.h> NOR <avrt.h>; leave() is defined in
// HardwareTopology.cpp where those headers are available.
class MmcssToken {
public:
    MmcssToken() noexcept = default;
    MmcssToken(void* handle, unsigned long task_index) noexcept
        : handle_(handle), task_index_(task_index) {}
    ~MmcssToken() noexcept { leave(); }

    MmcssToken(const MmcssToken&)            = delete;
    MmcssToken& operator=(const MmcssToken&) = delete;
    MmcssToken(MmcssToken&& o) noexcept
        : handle_(o.handle_), task_index_(o.task_index_) {
        o.handle_ = nullptr; o.task_index_ = 0;
    }
    MmcssToken& operator=(MmcssToken&& o) noexcept {
        if (this != &o) {
            leave();
            handle_ = o.handle_; task_index_ = o.task_index_;
            o.handle_ = nullptr; o.task_index_ = 0;
        }
        return *this;
    }

    // True iff a real MMCSS task is held (non-NULL AVRT handle).
    [[nodiscard]] bool          active()     const noexcept { return handle_ != nullptr; }
    [[nodiscard]] unsigned long task_index() const noexcept { return task_index_; }

    // Revert the MMCSS characteristics NOW (idempotent). Called by the destructor.
    void leave() noexcept;

private:
    void*         handle_{nullptr};
    unsigned long task_index_{0};
};

// Join the CURRENT thread to an MMCSS task and (optionally) set its relative
// priority. Returns an RAII MmcssToken; token.active()==false on failure (NULL
// handle — MMCSS disabled / SystemResponsiveness=100 / a future SKU) so the caller
// falls back to elevate_thread_rt(). Windows: AvSetMmThreadCharacteristics +
// (prio != None) AvSetMmThreadPriority. Non-Windows: always inactive.
[[nodiscard]] MmcssToken join_mmcss_task(MmcssTask    task,
                                         AvrtPriority prio = AvrtPriority::None) noexcept;

} // namespace hw

} // namespace phyriad
// Made with my soul - Swately <3
