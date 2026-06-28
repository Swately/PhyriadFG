// framework/topology/include/phyriad/topology/HybridCoreDetector.hpp
// Intel P/E-core and ARM big.LITTLE hybrid topology detection.
// Identifies performance vs. efficiency cores for optimal SPSC placement.
// Standalone — does not depend on HardwareTopology.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace phyriad::topology {

enum class CoreType : uint8_t {
    Performance = 1,   // P-core (Intel ADL+), big core (ARM), or default non-hybrid
    Efficiency  = 0,   // E-core (Intel ADL+), LITTLE core (ARM big.LITTLE)
    Unknown     = 0xFFu,
};

struct CoreTypeEntry {
    uint32_t logical_id{0};
    CoreType type{CoreType::Performance};
};

struct HybridInfo {
    bool                       is_hybrid{false};   // true iff Efficiency cores found
    std::string                detection_method;   // diagnostic: how cores were classified
    std::vector<CoreTypeEntry> core_types;         // one entry per logical core
};

class HybridCoreDetector {
public:
    // Detects core types for all online logical processors.
    // On non-hybrid CPUs all entries carry CoreType::Performance.
    [[nodiscard]] static HybridInfo detect() noexcept;

    // Returns true iff at least one Efficiency core was found.
    [[nodiscard]] static bool is_hybrid() noexcept;
};

} // namespace phyriad::topology
// Made with my soul - Swately <3
