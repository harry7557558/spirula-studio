#pragma once
// Metal work on the inference stream's timeline (macOS, SS_NN_METAL). MoltenVK
// exports the MTLBuffer behind each allocation and the MTLSharedEvent behind
// Stream's timeline (VK_EXT_metal_objects): a Metal section waits for every
// Vulkan submission before it, and the next submission waits for the section.

#include "nn/vk/Memory.h"

#include <cstdint>
#include <string>

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#endif

namespace nn {
namespace vk {
namespace metal {

// False, with reason(), when the device cannot share its memory with Metal.
bool               available();
const std::string& reason();

// Submits the open section, if any. Stream calls it before recording again.
void close();

struct Stats {
    uint64_t sections = 0;
    // A section MPSGraph moved to a second command buffer, which nothing orders
    // after the Vulkan work. close() aborts on the first one.
    uint64_t rotations = 0;
};
Stats stats();

// Fails unless [p, p + bytes) lies inside one allocation and its MTLBuffer.
void check_range(DevicePtr p, uint64_t bytes, const char* what);

#ifdef __OBJC__
id<MTLDevice> device();

struct Range {
    id<MTLBuffer> buffer;
    NSUInteger    offset;
};
// The MTLBuffer holding [p, p + bytes); fails if a byte of it lies outside.
Range resolve(DevicePtr p, uint64_t bytes, const char* what);

// The open section's command buffer, opening one on first use. `work` is the
// caller's FLOPs; a section it would push past kSectionWork is closed first.
MPSCommandBuffer* section(double work);
#endif

}  // namespace metal
}  // namespace vk
}  // namespace nn
