// Device enumeration and teardown -- see nn/Device.h.

#include "nn/Device.h"

#include "nn/core/Log.h"
#include "nn/vk/Context.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Pipelines.h"
#include "nn/vk/Stream.h"

namespace nn {

std::vector<DeviceInfo> list_devices() {
    std::vector<DeviceInfo> out;
    try {
        for (const auto& d : vk::enumerate_devices()) {
            DeviceInfo o;
            o.index = d.index;
            o.name = d.name;
            o.type = d.type;
            o.vram_bytes = d.vram_bytes;
            o.usable = d.usable;
            o.unusable_reason = d.unusable_reason;
            o.uuid = d.uuid;
            out.push_back(std::move(o));
        }
    } catch (const std::exception& e) {
        NN_LOG_ERROR("list_devices: %s\n", e.what());
    }
    return out;
}

void configure_device(const std::string& selector, bool validation,
                      bool profile, bool want_video) {
    vk::ContextOptions opts;
    opts.selector_set = true;
    opts.device_selector = selector;
    opts.validation = validation;
    opts.profile = profile;
    opts.want_video = want_video;
    vk::Context::configure(opts);
}

std::string configured_device_selector() {
    return vk::Context::configured_selector();
}

std::string current_device_selector() {
    return vk::Context::current_selector();
}

void shutdown() {
    try {
        vk::Stream::shutdown();
        vk::Pipelines::get().shutdown();
        vk::VramPool::get().releaseAll();
        vk::Allocator::get().freeAll();
        vk::Context::shutdown();
    } catch (const std::exception& e) {
        NN_LOG_ERROR("shutdown: %s\n", e.what());
    }
}

}  // namespace nn
