#include "nn/vk/Transfer.h"

#include "nn/core/Error.h"
#include "nn/vk/Stream.h"

#include <map>
#include <utility>

namespace nn {
namespace vk {

Transfer::Transfer() {
    Context& ctx = Context::get();
    if (ctx.transferFamily() == UINT32_MAX) return;
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = ctx.transferFamily();
    NN_VK_CHECK(vkCreateCommandPool(ctx.device(), &cpi, nullptr, &pool_));
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = kRing;
    NN_VK_CHECK(vkAllocateCommandBuffers(ctx.device(), &ai, cbs_));
    VkSemaphoreTypeCreateInfo sti{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    sti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    sci.pNext = &sti;
    NN_VK_CHECK(vkCreateSemaphore(ctx.device(), &sci, nullptr, &timeline_));
    queue_ = ctx.transferQueue();
}

Transfer::~Transfer() {
    if (!queue_) return;
    VkDevice dev = Context::get().device();
    try {
        if (submitted_) wait(submitted_);
    } catch (const std::exception&) {
        vkDeviceWaitIdle(dev);
    }
    vkDestroySemaphore(dev, timeline_, nullptr);
    vkDestroyCommandPool(dev, pool_, nullptr);
}

uint64_t Transfer::copy(const std::vector<Region>& regions, uint64_t after_stream) {
    Stream& stream = Stream::get();
    if (!queue_) {
        for (const Region& r : regions) stream.copy(r.dst, r.src, r.bytes);
        stream.flush();
        return stream.lastSubmitted();
    }

    if (cb_value_[cur_]) wait(cb_value_[cur_]);
    VkCommandBuffer cb = cbs_[cur_];
    NN_VK_CHECK(vkResetCommandBuffer(cb, 0));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    NN_VK_CHECK(vkBeginCommandBuffer(cb, &bi));
    std::map<std::pair<VkBuffer, VkBuffer>, std::vector<VkBufferCopy>> by_pair;
    for (const Region& r : regions) {
        Allocation src, dst;
        VkDeviceSize so = 0, dof = 0;
        NN_CHECK(Allocator::get().resolve(r.src, &src, &so) &&
                     Allocator::get().resolve(r.dst, &dst, &dof),
                 "Transfer::copy: an address that is not an allocation");
        NN_CHECK(so + r.bytes <= src.size && dof + r.bytes <= dst.size,
                 "Transfer::copy: a region runs past its allocation");
        by_pair[{src.buffer, dst.buffer}].push_back({so, dof, r.bytes});
    }
    for (const auto& kv : by_pair)
        vkCmdCopyBuffer(cb, kv.first.first, kv.first.second, (uint32_t)kv.second.size(),
                        kv.second.data());
    NN_VK_CHECK(vkEndCommandBuffer(cb));

    const uint64_t signal = ++submitted_;
    VkSemaphore stream_tl = stream.timeline();
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkTimelineSemaphoreSubmitInfo tsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.waitSemaphoreValueCount = after_stream ? 1 : 0;
    tsi.pWaitSemaphoreValues = &after_stream;
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &signal;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &tsi;
    si.waitSemaphoreCount = after_stream ? 1 : 0;
    si.pWaitSemaphores = &stream_tl;
    si.pWaitDstStageMask = &stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &timeline_;
    NN_VK_CHECK(vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE));
    cb_value_[cur_] = signal;
    cur_ = (cur_ + 1) % kRing;
    return signal;
}

void Transfer::streamWaits(uint64_t ticket) {
    if (queue_) Stream::get().waitOn(timeline_, ticket);
}

void Transfer::wait(uint64_t ticket) {
    wait_timeline(queue_ ? timeline_ : Stream::get().timeline(), ticket);
}

}  // namespace vk
}  // namespace nn
