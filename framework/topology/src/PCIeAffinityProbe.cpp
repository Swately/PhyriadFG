// framework/topology/src/PCIeAffinityProbe.cpp
#include <phyriad/topology/PCIeAffinityProbe.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <setupapi.h>   // HDEVINFO, SP_DEVINFO_DATA — type definitions only
// SetupAPI and device property keys — loaded dynamically to avoid hard-linking.
// setupapi.dll is always present on Windows 7+; dynamic load prevents link
// failure on MinGW configurations that don't pass -lsetupapi automatically.
#  include <initguid.h>
#  include <devpropdef.h>
// DEVPKEY_Device_Numa_Node: available since Windows 10 1607 (Build 14393).
// {540b947e-8b40-45bc-a8a2-6a0b894cbfa1}, 3
DEFINE_DEVPROPKEY(PHYRIAD_DEVPKEY_Device_Numa_Node,
    0x540b947eu, 0x8b40u, 0x45bcu,
    0xa8u, 0xa2u, 0x6au, 0x0bu, 0x89u, 0x4cu, 0xbfu, 0xa1u, 3u);
#else
#  include <filesystem>
#  include <sstream>
#endif

namespace phyriad::topology {

namespace {

// ── sysfs helper ─────────────────────────────────────────────────
#ifndef _WIN32

static std::string sysfs_read_line(const char* path) noexcept {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::string s;
    std::getline(f, s);
    return s;
}

static uint16_t parse_hex16(const std::string& s, uint16_t def = 0u) noexcept {
    if (s.empty()) return def;
    try {
        // sysfs vendor/device files start with "0x"
        return static_cast<uint16_t>(std::stoul(s, nullptr, 16));
    } catch (...) { return def; }
}

// ── LINUX implementation ─────────────────────────────────────────
static std::vector<PCIeDevice> probe_linux() noexcept {
    namespace fs = std::filesystem;
    const fs::path pci_base = "/sys/bus/pci/devices";
    if (!fs::exists(pci_base)) return {};

    std::vector<PCIeDevice> result;
    char path[512];

    for (const auto& entry : fs::directory_iterator(pci_base)) {
        if (!entry.is_directory() && !entry.is_symlink()) continue;

        const std::string addr = entry.path().filename().string();
        // PCI address format: "0000:00:1c.0"
        if (addr.size() < 7u) continue;

        PCIeDevice dev;
        dev.address = addr;

        // vendor
        std::snprintf(path, sizeof(path), "%s/%s/vendor",
            pci_base.string().c_str(), addr.c_str());
        dev.vendor_id = parse_hex16(sysfs_read_line(path));

        // device
        std::snprintf(path, sizeof(path), "%s/%s/device",
            pci_base.string().c_str(), addr.c_str());
        dev.device_id = parse_hex16(sysfs_read_line(path));

        // class — 6-digit hex "0x030000"; upper 16 bits = class+subclass
        std::snprintf(path, sizeof(path), "%s/%s/class",
            pci_base.string().c_str(), addr.c_str());
        {
            const std::string cls = sysfs_read_line(path);
            if (!cls.empty()) {
                try {
                    const uint32_t full = static_cast<uint32_t>(
                        std::stoul(cls, nullptr, 16));
                    dev.pci_class = static_cast<uint16_t>(full >> 8u);
                } catch (...) {}
            }
        }

        // numa_node — may be -1 on non-NUMA systems (treat as node 0)
        std::snprintf(path, sizeof(path), "%s/%s/numa_node",
            pci_base.string().c_str(), addr.c_str());
        {
            const std::string nn = sysfs_read_line(path);
            if (!nn.empty()) {
                try {
                    const int64_t val = std::stoll(nn);
                    dev.numa_node = (val >= 0) ? static_cast<uint32_t>(val) : 0u;
                } catch (...) {
                    dev.numa_node = UINT32_MAX;
                }
            }
        }

        result.push_back(std::move(dev));
    }

    return result;
}

// ── WINDOWS implementation ────────────────────────────────────────
#else

// SetupAPI function pointer types (loaded dynamically)
using FnSetupDiGetClassDevsA      = HDEVINFO(WINAPI*)(const GUID*, PCSTR, HWND, DWORD);
using FnSetupDiEnumDeviceInfo     = BOOL(WINAPI*)(HDEVINFO, DWORD, PSP_DEVINFO_DATA);
using FnSetupDiGetDevicePropertyW = BOOL(WINAPI*)(HDEVINFO, PSP_DEVINFO_DATA,
                                                   const DEVPROPKEY*, DEVPROPTYPE*,
                                                   PBYTE, DWORD, PDWORD, DWORD);
using FnSetupDiGetDeviceInstanceIdA = BOOL(WINAPI*)(HDEVINFO, PSP_DEVINFO_DATA,
                                                     PSTR, DWORD, PDWORD);
using FnSetupDiDestroyDeviceInfoList = BOOL(WINAPI*)(HDEVINFO);

struct SetupApiVtable {
    FnSetupDiGetClassDevsA          GetClassDevs{nullptr};
    FnSetupDiEnumDeviceInfo         EnumDeviceInfo{nullptr};
    FnSetupDiGetDevicePropertyW     GetDeviceProperty{nullptr};
    FnSetupDiGetDeviceInstanceIdA   GetDeviceInstanceId{nullptr};
    FnSetupDiDestroyDeviceInfoList  DestroyDeviceInfoList{nullptr};
    bool valid{false};
};

static const SetupApiVtable& get_setupapi() noexcept {
    static SetupApiVtable s_vt = []() noexcept -> SetupApiVtable {
        HMODULE h = LoadLibraryA("setupapi.dll");
        if (!h) return {};
        SetupApiVtable vt;
        vt.GetClassDevs = reinterpret_cast<FnSetupDiGetClassDevsA>(
            GetProcAddress(h, "SetupDiGetClassDevsA"));
        vt.EnumDeviceInfo = reinterpret_cast<FnSetupDiEnumDeviceInfo>(
            GetProcAddress(h, "SetupDiEnumDeviceInfo"));
        vt.GetDeviceProperty = reinterpret_cast<FnSetupDiGetDevicePropertyW>(
            GetProcAddress(h, "SetupDiGetDevicePropertyW"));
        vt.GetDeviceInstanceId = reinterpret_cast<FnSetupDiGetDeviceInstanceIdA>(
            GetProcAddress(h, "SetupDiGetDeviceInstanceIdA"));
        vt.DestroyDeviceInfoList = reinterpret_cast<FnSetupDiDestroyDeviceInfoList>(
            GetProcAddress(h, "SetupDiDestroyDeviceInfoList"));
        vt.valid = vt.GetClassDevs        &&
                   vt.EnumDeviceInfo      &&
                   vt.GetDeviceProperty   &&
                   vt.GetDeviceInstanceId &&
                   vt.DestroyDeviceInfoList;
        return vt;
        // Library intentionally not freed — lifetime == process
    }();
    return s_vt;
}

// Extract PCI address string from a device instance ID.
// Device IDs look like "PCI\VEN_10DE&DEV_2204&SUBSYS_147C1458&REV_A1\4&2C8A..."
// We synthesize a simplified "bus:dev.func" from location info when possible.
// For Block A purposes, we use the instance ID prefix as the "address".
static std::string extract_pci_address(const std::string& instance_id) noexcept {
    // Return the part before the first backslash after "PCI\"
    const auto second_backslash = instance_id.find('\\', 4u);
    if (second_backslash != std::string::npos)
        return instance_id.substr(0u, second_backslash);
    return instance_id;
}

// Parse vendor/device IDs from "PCI\VEN_XXXX&DEV_XXXX..." instance ID.
static void parse_ids_from_instance(const std::string& id,
                                    uint16_t& vendor, uint16_t& device) noexcept {
    auto hex_after = [&](const char* tag) noexcept -> uint16_t {
        const auto pos = id.find(tag);
        if (pos == std::string::npos) return 0u;
        try {
            return static_cast<uint16_t>(
                std::stoul(id.substr(pos + std::strlen(tag), 4u), nullptr, 16));
        } catch (...) { return 0u; }
    };
    vendor = hex_after("VEN_");
    device = hex_after("DEV_");
}

static std::vector<PCIeDevice> probe_windows() noexcept {
    const auto& api = get_setupapi();
    if (!api.valid) return {};

    // Enumerate all PCI devices (GUID_DEVCLASS_SYSTEM covers PCI bus children)
    // Passing nullptr for ClassGuid + DIGCF_ALLCLASSES enumerates everything,
    // which may be slow. We filter by PCI in instance ID.
    HDEVINFO dev_info = api.GetClassDevs(
        nullptr, "PCI", nullptr,
        0x00000004 /* DIGCF_ALLCLASSES */ | 0x00000002 /* DIGCF_PRESENT */);

    if (dev_info == INVALID_HANDLE_VALUE) return {};

    std::vector<PCIeDevice> result;
    SP_DEVINFO_DATA dev_data{};
    dev_data.cbSize = sizeof(dev_data);

    for (DWORD idx = 0u; api.EnumDeviceInfo(dev_info, idx, &dev_data); ++idx) {
        // Get instance ID (e.g. "PCI\VEN_10DE&DEV_2204\4&...")
        char instance_id[512]{};
        if (!api.GetDeviceInstanceId(dev_info, &dev_data,
                                     instance_id, sizeof(instance_id), nullptr))
            continue;

        const std::string inst_str(instance_id);
        if (inst_str.find("PCI\\") != 0u) continue; // not a PCI device

        PCIeDevice dev;
        dev.address = extract_pci_address(inst_str);
        parse_ids_from_instance(inst_str, dev.vendor_id, dev.device_id);

        // Query NUMA node via DEVPKEY_Device_Numa_Node
        DEVPROPTYPE prop_type = 0;
        UINT32 numa_val = UINT32_MAX;
        DWORD prop_size = sizeof(numa_val);
        if (api.GetDeviceProperty(dev_info, &dev_data,
                                   &PHYRIAD_DEVPKEY_Device_Numa_Node,
                                   &prop_type,
                                   reinterpret_cast<PBYTE>(&numa_val),
                                   prop_size, &prop_size, 0u)) {
            dev.numa_node = static_cast<uint32_t>(numa_val);
        }

        result.push_back(std::move(dev));
    }

    api.DestroyDeviceInfoList(dev_info);
    return result;
}

#endif // _WIN32

} // anonymous namespace

// ── Public API ────────────────────────────────────────────────────

std::vector<PCIeDevice> PCIeAffinityProbe::probe() noexcept {
#ifdef _WIN32
    return probe_windows();
#elif defined(__linux__)
    return probe_linux();
#else
    return {}; // ARM macOS, BSD, etc. — no sysfs equivalent
#endif
}

uint32_t PCIeAffinityProbe::numa_for_device(const std::string& pci_addr) noexcept {
    static const auto s_devices = probe();
    for (const auto& d : s_devices) {
        if (d.address == pci_addr) return d.numa_node;
    }
    return UINT32_MAX;
}

} // namespace phyriad::topology
// Made with my soul - Swately <3
