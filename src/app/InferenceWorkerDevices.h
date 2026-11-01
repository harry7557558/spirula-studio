#pragma once

#include "nn/Device.h"
#include "core/HostMemory.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace app {

inline bool worker_device_supported(const nn::DeviceInfo& device) {
    return device.usable && !device.uuid.empty() &&
        (device.type == "discrete" || device.type == "integrated");
}

inline std::vector<std::string> worker_devices_for(
    const std::string& additional, const std::string& primary,
    const std::vector<nn::DeviceInfo>& devices) {
    const auto first = std::find_if(devices.begin(), devices.end(), [&](const auto& device) {
        return device.uuid == primary && worker_device_supported(device);
    });
    if (primary.empty() || first == devices.end() || additional == "none")
        return {};
    const bool discrete = std::any_of(devices.begin(), devices.end(), [](const auto& device) {
        return device.type == "discrete" && worker_device_supported(device);
    });
    std::vector<std::string> result{primary};
    for (const auto& device : devices) {
        if (!worker_device_supported(device) ||
            std::find(result.begin(), result.end(), device.uuid) != result.end())
            continue;
        const bool selected = additional == "auto"
            ? device.type == (discrete ? "discrete" : "integrated")
            : ("," + additional + ",").find("," + device.uuid + ",") != std::string::npos;
        if (selected) result.push_back(device.uuid);
    }
    if (result.size() == 1) result.clear();
    return result;
}

inline uint64_t default_worker_memory_bytes() {
    return std::max(uint64_t(4) * 1024 * 1024 * 1024,
        uint64_t(spirula::physicalRamBytes()) / 2);
}

inline std::vector<std::string> parse_worker_devices(const std::string& csv) {
    std::vector<std::string> result;
    if (csv.empty()) return result;
    const auto available = nn::list_devices();
    std::set<std::string> seen;
    size_t start = 0;
    while (start <= csv.size()) {
        const size_t comma = csv.find(',', start);
        std::string uuid = csv.substr(start, comma == std::string::npos ? comma : comma - start);
        std::transform(uuid.begin(), uuid.end(), uuid.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        if (uuid.size() != 37 || uuid.compare(0, 5, "uuid:") != 0 ||
            !std::all_of(uuid.begin() + 5, uuid.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            }) || !seen.insert(uuid).second)
            throw std::runtime_error("worker devices require unique canonical UUID selectors");
        const auto found = std::find_if(available.begin(), available.end(), [&](const auto& d) {
            return d.uuid == uuid;
        });
        if (found == available.end())
            throw std::runtime_error("unavailable worker device: " + uuid);
        if (!worker_device_supported(*found))
            throw std::runtime_error("unsupported worker device: " + found->name);
        result.push_back(std::move(uuid));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return result;
}

}  // namespace app
