// framework/topology/include/phyriad/topology/VCacheDetector.hpp
// AMD 3D V-Cache detection and L3 domain grouping.
// Identifies which L3 cache domains carry V-Cache (≥96 MB) and which cores
// belong to each domain. Standalone — does not depend on HardwareTopology.
#pragma once
#include <cstdint>
#include <vector>

namespace phyriad::topology {

struct VCacheCCX {
    uint32_t              ccx_id{0};
    uint32_t              l3_size_kb{0};     // effective L3 visible to this domain
    bool                  has_v_cache{false}; // true when l3_size_kb >= 96 MB
    std::vector<uint32_t> logical_ids;        // all logical cores in this L3 domain
};

class VCacheDetector {
public:
    // Probes platform topology and returns one entry per distinct L3 domain.
    // Empty on non-AMD, non-x86, or irrecoverable detection failure.
    [[nodiscard]] static std::vector<VCacheCCX> detect() noexcept;

    // Convenience: returns sorted logical IDs of all V-Cache cores.
    [[nodiscard]] static std::vector<uint32_t> vcache_logical_ids() noexcept;
};

} // namespace phyriad::topology
// Made with my soul - Swately <3
