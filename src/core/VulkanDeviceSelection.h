#pragma once
// Shared Vulkan device selection and physical-device identity.
// Each runtime resolves a selector in its own VkInstance.
//
// Header-only: core/*.cpp is also compiled into the CUDA engine, so this
// file is included only by Vulkan-native targets.
//
// Explicit selector, then SS_VK_DEVICE, then Auto; resolved UUIDs survive
// reordered enumeration.

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <climits>
#include "core/Env.h"

namespace spirula {
namespace vkselect {

// One physical device as its runtime enumerated it. `index` is that runtime's
// ordinal and means nothing outside it; `uuid` is the identity to carry.
// `usable` is the runtime's own baseline, not a claim about any model.
struct DeviceRecord {
    int                        index = -1;
    std::string                name;
    std::string                type;  // discrete|integrated|virtual|cpu|other
    uint64_t                   vram_bytes = 0;
    bool                       usable = false;
    std::string                unusable_reason;
    VkPhysicalDeviceProperties props{};
    uint8_t                    uuid[VK_UUID_SIZE] = {};
};

inline const char* deviceTypeName(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "cpu";
        default:                                     return "other";
    }
}

// Auto's first key: discrete > integrated > virtual > CPU > other.
inline int deviceTypeRank(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 4;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return 1;
        default:                                     return 0;
    }
}

inline bool uuidIsZero(const uint8_t uuid[VK_UUID_SIZE]) {
    for (int i = 0; i < VK_UUID_SIZE; i++)
        if (uuid[i]) return false;
    return true;
}

inline bool uuidEquals(const uint8_t a[VK_UUID_SIZE], const uint8_t b[VK_UUID_SIZE]) {
    for (int i = 0; i < VK_UUID_SIZE; i++)
        if (a[i] != b[i]) return false;
    return true;
}

// The canonical selector spelling: "uuid:" + 32 lowercase hex digits.
inline std::string uuidSelector(const uint8_t uuid[VK_UUID_SIZE]) {
    static const char* hex = "0123456789abcdef";
    std::string s = "uuid:";
    for (int i = 0; i < VK_UUID_SIZE; i++) {
        s += hex[uuid[i] >> 4];
        s += hex[uuid[i] & 0xf];
    }
    return s;
}

// Empty for a device with no reported UUID -- there is nothing to carry.
inline std::string selectorFor(const DeviceRecord& r) {
    return uuidIsZero(r.uuid) ? std::string() : uuidSelector(r.uuid);
}

// Fills name/type/props/uuid from one enumerated physical device; the caller
// adds its own vram figure and usability verdict.
inline void probeIdentity(VkPhysicalDevice pd, DeviceRecord* r) {
    VkPhysicalDeviceIDProperties id{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &id;
    vkGetPhysicalDeviceProperties2(pd, &p2);
    r->props = p2.properties;
    r->name = p2.properties.deviceName;
    r->type = deviceTypeName(p2.properties.deviceType);
    std::copy(id.deviceUUID, id.deviceUUID + VK_UUID_SIZE, r->uuid);
}

// What the caller asked for. `text` is the raw value; `error` explains a
// Malformed one, so a CLI can report the bad flag before any GPU work.
struct Request {
    enum class Kind { Auto, Ordinal, Name, Uuid, Malformed };
    Kind        kind = Kind::Auto;
    int         ordinal = -1;
    std::string text;
    std::string error;
    bool        explicit_request = false;  // from the caller, not the environment
};

// Whole values only: overflow, every negative other than SfM's -1, and any
// other malformed spelling is Malformed rather than a guess.
inline Request parseRequest(const std::string& value) {
    Request r;
    r.text = value;
    size_t b = 0, e = value.size();
    while (b < e && std::isspace((unsigned char)value[b])) b++;
    while (e > b && std::isspace((unsigned char)value[e - 1])) e--;
    const std::string v = value.substr(b, e - b);
    r.text = v;

    if (v.empty()) {
        r.kind = Request::Kind::Malformed;
        r.error = "empty device selector";
        return r;
    }
    std::string lower = v;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);

    if (lower == "auto" || v == "-1") {
        r.kind = Request::Kind::Auto;
        return r;
    }
    if (lower.rfind("uuid:", 0) == 0) {
        const std::string hex = v.substr(5);
        if (hex.size() != 2 * VK_UUID_SIZE) {
            r.kind = Request::Kind::Malformed;
            r.error = "uuid selector needs " +
                      std::to_string(2 * VK_UUID_SIZE) + " hex digits, got " +
                      std::to_string(hex.size());
            return r;
        }
        for (char c : hex)
            if (!std::isxdigit((unsigned char)c)) {
                r.kind = Request::Kind::Malformed;
                r.error = "uuid selector is not hex";
                return r;
            }
        r.kind = Request::Kind::Uuid;
        return r;
    }
    if (std::isdigit((unsigned char)v[0])) {
        long long n = 0;
        for (char c : v) {
            if (!std::isdigit((unsigned char)c)) {
                r.kind = Request::Kind::Malformed;
                r.error = "device index '" + v + "' is not a whole number";
                return r;
            }
            const int digit = c - '0';
            if (n > (INT32_MAX - digit) / 10) {
                r.kind = Request::Kind::Malformed;
                r.error = "device index '" + v + "' is out of range";
                return r;
            }
            n = n * 10 + digit;
        }
        r.kind = Request::Kind::Ordinal;
        r.ordinal = (int)n;
        return r;
    }
    if (v[0] == '-') {
        r.kind = Request::Kind::Malformed;
        r.error = "negative device value '" + v + "' (auto is -1 or auto)";
        return r;
    }
    r.kind = Request::Kind::Name;
    return r;
}

// The shared precedence: an explicit CLI/GUI value wins, else a nonempty
// SS_VK_DEVICE, else Auto. `explicit_set` matters because an explicit Auto
// (empty selector) must not fall through to the environment.
inline Request requestFrom(const std::string& explicit_selector, bool explicit_set) {
    if (explicit_set) {
        Request r = parseRequest(explicit_selector.empty() ? "auto"
                                                           : explicit_selector);
        r.explicit_request = true;
        return r;
    }
    const char* env = spirula::env("VK_DEVICE");
    if (env && env[0]) return parseRequest(env);
    return parseRequest("auto");
}

enum class ResolveStatus {
    Ok,
    Malformed,
    OutOfRange,
    Missing,
    Ambiguous,
    Unusable,
    NoDevice,
};

struct Resolution {
    ResolveStatus status = ResolveStatus::Ok;
    std::string   error;     // user-facing reason, empty when Ok
    DeviceRecord  device;    // the resolved record; for Unusable, the match
    std::string   selector;  // canonical uuid:<hex>, empty if none reported
    bool          ok() const { return status == ResolveStatus::Ok; }
};

inline int findByUuid(const std::vector<DeviceRecord>& devices,
                      const uint8_t uuid[VK_UUID_SIZE]) {
    int found = -1;
    for (size_t i = 0; i < devices.size(); i++) {
        if (uuidIsZero(devices[i].uuid) || !uuidEquals(devices[i].uuid, uuid))
            continue;
        if (found >= 0) return -2;  // The runtime reported a duplicate identity.
        found = (int)i;
    }
    return found;
}

// Auto's total order: type rank first, VRAM second. A record with no reported
// deviceUUID is not a candidate at all: an identity is what the request carries
// forward, and Vulkan permits an all-zero one.
inline bool autoCandidate(const DeviceRecord& r) {
    return r.usable && !uuidIsZero(r.uuid);
}

inline bool outranks(const DeviceRecord& a, const DeviceRecord& b) {
    const int ra = deviceTypeRank(a.props.deviceType);
    const int rb = deviceTypeRank(b.props.deviceType);
    if (ra != rb) return ra > rb;
    return a.vram_bytes > b.vram_bytes;
}

// Best record with a reported identity, or -1.
inline int autoPick(const std::vector<DeviceRecord>& devices) {
    int best = -1;
    for (size_t i = 0; i < devices.size(); i++) {
        if (!autoCandidate(devices[i])) continue;
        if (best < 0 || outranks(devices[i], devices[best])) best = (int)i;
    }
    return best;
}

inline Resolution resolveRequest(const Request& r,
                                 const std::vector<DeviceRecord>& devices) {
    Resolution out;
    auto fail = [&](ResolveStatus s, const std::string& why) {
        out.status = s;
        out.error = why;
        return out;
    };

    if (r.kind == Request::Kind::Malformed)
        return fail(ResolveStatus::Malformed, r.error);

    int picked = -1;
    if (r.kind == Request::Kind::Auto) {
        picked = autoPick(devices);
        if (picked < 0) {
            for (const DeviceRecord& d : devices)
                if (d.usable && uuidIsZero(d.uuid))
                    return fail(ResolveStatus::Missing,
                                "device '" + d.name +
                                    "' reports no physical-device UUID, so it "
                                    "cannot be selected by identity");
            return fail(ResolveStatus::NoDevice,
                        "no usable Vulkan device (need Vulkan 1.2 + "
                        "bufferDeviceAddress + timelineSemaphore)");
        }
    } else if (r.kind == Request::Kind::Ordinal) {
        if (r.ordinal >= (int)devices.size())
            return fail(ResolveStatus::OutOfRange,
                        "device index " + std::to_string(r.ordinal) +
                        " does not exist (" + std::to_string(devices.size()) +
                        " device(s))");
        picked = r.ordinal;
    } else if (r.kind == Request::Kind::Uuid) {
        uint8_t want[VK_UUID_SIZE] = {};
        const std::string hex = r.text.substr(5);
        for (int i = 0; i < VK_UUID_SIZE; i++) {
            auto nibble = [](char c) {
                return (uint8_t)(c <= '9' ? c - '0'
                                          : std::tolower((unsigned char)c) - 'a' + 10);
            };
            want[i] = (uint8_t)(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
        }
        picked = findByUuid(devices, want);
        if (picked == -2)
            return fail(ResolveStatus::Ambiguous,
                        "multiple Vulkan devices report " + r.text);
        if (picked < 0)
            return fail(ResolveStatus::Missing,
                        "no Vulkan device has " + r.text);
    } else {
        int found = 0;
        for (size_t i = 0; i < devices.size(); i++) {
            std::string name = devices[i].name, needle = r.text;
            for (auto& c : name) c = (char)std::tolower((unsigned char)c);
            for (auto& c : needle) c = (char)std::tolower((unsigned char)c);
            if (name.find(needle) == std::string::npos) continue;
            if (found == 0) picked = (int)i;
            found++;
        }
        if (found == 0)
            return fail(ResolveStatus::Missing,
                        "no Vulkan device name contains '" + r.text + "'");
        if (found > 1)
            return fail(ResolveStatus::Ambiguous,
                        "device name '" + r.text + "' matches " +
                        std::to_string(found) +
                        " devices; use a longer name or a uuid: selector");
    }

    const DeviceRecord& d = devices[picked];
    // An ordinal or a name only looks a record up; the identity is what the
    // caller keeps, so a record that has none cannot answer a request.
    if (uuidIsZero(d.uuid))
        return fail(ResolveStatus::Missing,
                    "device " + std::to_string(picked) + " (" + d.name +
                        ") reports no physical-device UUID, so it cannot be "
                        "selected by identity");
    if (findByUuid(devices, d.uuid) == -2)
        return fail(ResolveStatus::Ambiguous,
                    "multiple Vulkan devices report " + uuidSelector(d.uuid));
    if (!d.usable) {
        std::string why = "device " + std::to_string(picked) + " (" + d.name +
                          ") is not usable";
        if (!d.unusable_reason.empty()) why += ": " + d.unusable_reason;
        out.device = d;
        out.status = ResolveStatus::Unusable;
        out.error = why;
        return out;
    }
    out.device = d;
    out.selector = uuidSelector(d.uuid);
    return out;
}

inline Resolution resolveSelector(const std::string& value,
                                  const std::vector<DeviceRecord>& devices) {
    return resolveRequest(parseRequest(value), devices);
}

}  // namespace vkselect
}  // namespace spirula
