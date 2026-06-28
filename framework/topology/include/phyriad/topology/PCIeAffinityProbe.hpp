// framework/topology/include/phyriad/topology/PCIeAffinityProbe.hpp
// PCIe device → NUMA node affinity probing.
// Used by the Scheduler (Block F) to co-locate workers with their PCIe devices
// (GPUs, NICs, NVMe) on the same NUMA node to avoid cross-NUMA DMA traffic.
// Standalone — does not depend on HardwareTopology.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace phyriad::topology {

struct PCIeDevice {
    std::string address;          // "0000:03:00.0" (domain:bus:dev.func)
    uint16_t    vendor_id{0};     // e.g. 0x10DE = NVIDIA, 0x1002 = AMD, 0x8086 = Intel
    uint16_t    device_id{0};
    uint16_t    pci_class{0};     // upper 16 bits of class code:
                                  //   0x0300 = display/GPU
                                  //   0x0200 = network controller
                                  //   0x0108 = NVMe storage
    uint32_t    numa_node{UINT32_MAX};  // UINT32_MAX = NUMA affinity unknown
};

class PCIeAffinityProbe {
public:
    // Enumerates all PCIe devices with their NUMA affinity.
    // Linux:   /sys/bus/pci/devices/ (no privileges required)
    // Windows: SetupAPI DEVPKEY_Device_Numa_Node (dynamic, no admin required)
    // Returns empty on unsupported platforms or irrecoverable failure.
    [[nodiscard]] static std::vector<PCIeDevice> probe() noexcept;

    // Returns the NUMA node for a specific PCI address string.
    // Returns UINT32_MAX if the address is not in the probe results.
    [[nodiscard]] static uint32_t numa_for_device(const std::string& pci_addr) noexcept;
};

} // namespace phyriad::topology
// Made with my soul - Swately <3
