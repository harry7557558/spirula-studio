#pragma once

#include "nn/Device.h"
#include "core/HostMemory.h"
#include "core/Env.h"

#include <algorithm>
#include <charconv>
#include <cstring>
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
inline unsigned parse_workers_per_gpu(const char* value) {
    if (!value) return 1;
    unsigned count = 0;
    const char* end = value + std::strlen(value);
    const auto parsed = std::from_chars(value, end, count);
    if (parsed.ec != std::errc{} || parsed.ptr != end || !count)
        throw std::runtime_error("SS_WORKERS_PER_GPU must be an integer >= 1");
    return count;
}

inline unsigned workers_per_gpu() {
    return parse_workers_per_gpu(spirula::env("WORKERS_PER_GPU"));
}

inline std::vector<std::string> expand_worker_devices(
    const std::vector<std::string>& devices, unsigned count) {
    if (count == 1 || devices.empty()) return devices;
    std::vector<std::string> result;
    if (!count || count > result.max_size() / devices.size())
        throw std::runtime_error("SS_WORKERS_PER_GPU exceeds the worker-list capacity");
    result.reserve(devices.size() * count);
    for (unsigned i = 0; i < count; ++i)
        result.insert(result.end(), devices.begin(), devices.end());
    return result;
}

inline std::vector<std::string> worker_devices_for(
    const std::string& additional, const std::string& primary,
    const std::vector<nn::DeviceInfo>& devices, unsigned copies = 1) {
    const auto first = std::find_if(devices.begin(), devices.end(), [&](const auto& device) {
        return device.uuid == primary && worker_device_supported(device);
    });
    if (primary.empty() || first == devices.end()) return {};
    if (additional == "none")
        return copies > 1 ? std::vector<std::string>{primary} : std::vector<std::string>{};
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
    if (result.size() == 1 && copies == 1) result.clear();
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
