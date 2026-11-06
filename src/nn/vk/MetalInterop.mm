#define VK_USE_PLATFORM_METAL_EXT 1
#include "nn/vk/MetalInterop.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/vk/Context.h"
#include "nn/vk/Stream.h"

#include <vulkan/vulkan_metal.h>

#include <cstdio>
#include <cstdlib>

namespace nn {
namespace vk {
namespace metal {

namespace {

// FLOPs one section carries before it is submitted: ~0.25 s at the 4-6 TFLOP/s
// MPSGraph reaches, far inside Metal's command-buffer timeout.
constexpr double kSectionWork = 1e12;

struct State {
    uint64_t           generation = 0;
    bool               ok = false;
    std::string        reason = "not probed";
    PFN_vkExportMetalObjectsEXT exportObjects = nullptr;
    id<MTLDevice>       device = nil;
    id<MTLCommandQueue> queue = nil;   // our own: MoltenVK's may hold a waiting buffer
    id<MTLSharedEvent>  event = nil;   // Stream's timeline

    MPSCommandBuffer* open = nil;
    uint64_t          signal = 0;
    double            work = 0;
    Stats             stats;
};

State& state() {
    static State s;
    return s;
}

id<MTLBuffer> export_buffer(State& s, VkDeviceMemory memory) {
    VkExportMetalBufferInfoEXT bi{VK_STRUCTURE_TYPE_EXPORT_METAL_BUFFER_INFO_EXT};
    bi.memory = memory;
    VkExportMetalObjectsInfoEXT oi{VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT};
    oi.pNext = &bi;
    s.exportObjects(Context::get().device(), &oi);
    return bi.mtlBuffer;
}

void probe(State& s) {
    s = State{};
    s.generation = Context::generation();
    Context& ctx = Context::get();
    if (!ctx.hasMetalObjects()) {
        s.reason = "the driver does not expose VK_EXT_metal_objects";
        return;
    }
    if (@available(macOS 15.0, *)) {
    } else {
        s.reason = "MPSGraph attention and offset NDArrays need macOS 15";
        return;
    }
    s.exportObjects = (PFN_vkExportMetalObjectsEXT)ctx.deviceProc("vkExportMetalObjectsEXT");
    if (!s.exportObjects) {
        s.reason = "vkExportMetalObjectsEXT did not resolve";
        return;
    }

    VkExportMetalDeviceInfoEXT di{VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT};
    VkExportMetalCommandQueueInfoEXT qi{VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT};
    qi.queue = ctx.queue();
    VkExportMetalSharedEventInfoEXT ei{VK_STRUCTURE_TYPE_EXPORT_METAL_SHARED_EVENT_INFO_EXT};
    ei.semaphore = Stream::get().timeline();
    di.pNext = &qi;
    qi.pNext = &ei;
    VkExportMetalObjectsInfoEXT oi{VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT};
    oi.pNext = &di;
    s.exportObjects(ctx.device(), &oi);
    if (!di.mtlDevice || !qi.mtlCommandQueue || !ei.mtlSharedEvent) {
        s.reason = "MoltenVK exported no MTLDevice, MTLCommandQueue or timeline MTLSharedEvent";
        return;
    }
    // Compared by registry ID. The event reports a nil device on macOS 26
    // (measured), so only a non-nil one is held to it.
    const uint64_t gpu = di.mtlDevice.registryID;
    if (qi.mtlCommandQueue.device.registryID != gpu ||
        (ei.mtlSharedEvent.device && ei.mtlSharedEvent.device.registryID != gpu)) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "the exported queue or event is on another GPU (%llx, %llx, %llx)",
                      (unsigned long long)gpu, (unsigned long long)qi.mtlCommandQueue.device.registryID,
                      (unsigned long long)ei.mtlSharedEvent.device.registryID);
        s.reason = buf;
        return;
    }
    NN_LOG_DEBUG("[nn] metal: device %p queue.device %p event.device %p\n", (__bridge void*)di.mtlDevice,
                 (__bridge void*)qi.mtlCommandQueue.device, (__bridge void*)ei.mtlSharedEvent.device);
    s.device = di.mtlDevice;
    s.event = ei.mtlSharedEvent;
    s.queue = [s.device newCommandQueue];

    // The memory's MTLBuffer and the VkBuffer's own are distinct objects over
    // one heap range; only equal GPU addresses prove they alias.
    const DevicePtr p = device_alloc(4096, "metal probe");
    Allocation a;
    VkDeviceSize off = 0;
    id<MTLBuffer> b = nil;
    if (Allocator::get().resolve(p, &a, &off)) b = export_buffer(s, a.memory);
    const bool same = b && b.device.registryID == gpu && b.gpuAddress == a.addr;
    device_free(p);
    if (!same) {
        s.reason = b ? "an exported MTLBuffer's GPU address is not the Vulkan device address"
                     : "MoltenVK exported no MTLBuffer for an allocation";
        return;
    }
    s.ok = true;
    s.reason.clear();
}

State& live() {
    State& s = state();
    if (s.generation != Context::generation()) probe(s);
    return s;
}

}  // namespace

bool available() { return live().ok; }

const std::string& reason() { return live().reason; }

Stats stats() { return state().stats; }

id<MTLDevice> device() { return live().device; }

Range resolve(DevicePtr p, uint64_t bytes, const char* what) {
    State& s = live();
    Allocation a;
    VkDeviceSize off = 0;
    NN_CHECK(Allocator::get().resolve(p, &a, &off), "metal: %s at 0x%llx is not a device address",
             what, (unsigned long long)p);
    NN_CHECK(off + bytes <= a.size, "metal: %s, %llu bytes at offset %llu, overruns a %llu-byte allocation",
             what, (unsigned long long)bytes, (unsigned long long)off, (unsigned long long)a.size);
    id<MTLBuffer> b = export_buffer(s, a.memory);
    NN_CHECK(b && b.device.registryID == s.device.registryID, "metal: no MTLBuffer on this device for %s", what);
    NN_CHECK(b.gpuAddress == a.addr && off + bytes <= b.length,
             "metal: %s does not lie inside its MTLBuffer (gpu 0x%llx vs 0x%llx, %llu + %llu of %llu)",
             what, (unsigned long long)b.gpuAddress, (unsigned long long)a.addr,
             (unsigned long long)off, (unsigned long long)bytes, (unsigned long long)b.length);
    return {b, (NSUInteger)off};
}

void check_range(DevicePtr p, uint64_t bytes, const char* what) { resolve(p, bytes, what); }

// A failed section never signals, and Vulkan would wait on it forever.
void check_completed(id<MTLCommandBuffer> done) {
    if (done.status == MTLCommandBufferStatusCompleted) return;
    std::fprintf(stderr, "[nn] metal command buffer failed (status %d): %s\n", (int)done.status,
                 done.error.localizedDescription.UTF8String);
    std::abort();
}

void close() {
    State& s = state();
    if (!s.open) return;
    MPSCommandBuffer* cb = s.open;
    s.open = nil;
    s.work = 0;
    // MPSGraph committed the waiting buffer and went on in a fresh one, which
    // nothing orders after the Vulkan work. It is not committed yet, so stop
    // here before it can read early. Never seen (0 in every measured run).
    if (cb.commandBuffer != cb.rootCommandBuffer) {
        ++s.stats.rotations;
        std::fprintf(stderr, "[nn] metal: MPSGraph split a section across command buffers; "
                             "its reads would be unordered. Rerun with SS_NN_METAL=0.\n");
        std::abort();
    }
    [cb encodeSignalEvent:s.event value:s.signal];
    [cb commit];
}

MPSCommandBuffer* section(double work) {
    State& s = live();
    NN_CHECK(s.ok, "metal: %s", s.reason.c_str());
    Stream& st = Stream::get();
    if (s.open && s.work > 0 && s.work + work > kSectionWork) close();
    if (!s.open) {
        st.setExternalCloser(&close);
        const Stream::Handoff h = st.handOff();
        id<MTLCommandBuffer> cb = [s.queue commandBuffer];
        [cb addCompletedHandler:^(id<MTLCommandBuffer> d) { check_completed(d); }];
        [cb encodeWaitForEvent:s.event value:h.wait];
        s.open = [MPSCommandBuffer commandBufferWithCommandBuffer:cb];
        s.signal = h.signal;
        ++s.stats.sections;
    }
    s.work += work;
    return s.open;
}

}  // namespace metal
}  // namespace vk
}  // namespace nn
