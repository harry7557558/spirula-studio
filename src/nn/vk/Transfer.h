#pragma once
// Host-to-device copies on the transfer queue, running while the compute
// stream keeps dispatching -- how a model whose weights do not fit is streamed
// without the GPU waiting on PCIe.
//
// Ordering against the stream is two timeline semaphores: a copy can wait for
// the stream to reach a value (it is done with the memory being overwritten),
// and the stream waits for the copy's ticket before it reads the result.
// Without a transfer-only family (Context::transferFamily) the copies are
// recorded on the stream itself; nothing overlaps, and the caller is the same.
// Every call is from the thread that drives the stream, except wait().

#include "nn/vk/Memory.h"

#include <cstdint>
#include <vector>

namespace nn {
namespace vk {

class Transfer {
public:
    Transfer();
    ~Transfer();
    Transfer(const Transfer&) = delete;
    Transfer& operator=(const Transfer&) = delete;

    // A destination written here must come from Allocator::allocDeviceShared.
    bool overlaps() const { return queue_ != VK_NULL_HANDLE; }

    struct Region {
        DevicePtr dst = 0, src = 0;
        VkDeviceSize bytes = 0;
    };
    // Copies once the stream's timeline has reached `after_stream` (0: no wait).
    // Returns the ticket the other two calls take.
    uint64_t copy(const std::vector<Region>& regions, uint64_t after_stream);
    // The stream's next submission waits for `ticket`.
    void streamWaits(uint64_t ticket);
    // Blocks the calling thread, any thread, until `ticket` has completed.
    void wait(uint64_t ticket);

private:
    static constexpr int kRing = 8;
    VkQueue         queue_ = VK_NULL_HANDLE;
    VkCommandPool   pool_ = VK_NULL_HANDLE;
    VkCommandBuffer cbs_[kRing] = {};
    uint64_t        cb_value_[kRing] = {};
    int             cur_ = 0;
    VkSemaphore     timeline_ = VK_NULL_HANDLE;
    uint64_t        submitted_ = 0;
};

}  // namespace vk
}  // namespace nn
