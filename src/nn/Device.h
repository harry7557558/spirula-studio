#pragma once
// Device enumeration and process teardown for the inference layer.
//
// Separate from any model's header so an application can present a device
// picker before a checkpoint is chosen -- enumeration creates no device and
// allocates nothing.

#include <cstdint>
#include <string>
#include <vector>

namespace nn {

struct DeviceInfo {
    int         index = -1;
    std::string name;
    std::string type;              // discrete|integrated|virtual|cpu|other
    uint64_t    vram_bytes = 0;
    bool        usable = false;    // meets the Vulkan 1.2 baseline
    std::string unusable_reason;
    std::string uuid;              // canonical "uuid:<32 hex>", "" if unreported
};

std::vector<DeviceInfo> list_devices();

// Freezes the inference device without creating a logical device, so an
// application can commit its GPU choice before loading a model. Throws on an
// unknown/ambiguous/unusable selector, and on a different one once live.
void configure_device(const std::string& selector, bool validation = false,
                      bool profile = false, bool want_video = false);

// Canonical selector of the configured device (empty before configuration).
// The configured identity survives shutdown() for later context generations.
std::string configured_device_selector();
std::string current_device_selector();

// Frees every device allocation and destroys the Vulkan device. Call at exit,
// once every Session, Tracker and VideoReader is gone.
void shutdown();

}  // namespace nn
