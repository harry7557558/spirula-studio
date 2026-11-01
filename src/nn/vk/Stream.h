#pragma once
// Command recording and submission on the single compute queue.
//
// Dispatches and copies accumulate into one open command buffer; a flush ends
// it, submits, and signals a queue-level timeline semaphore. Batching matters:
// a SAM 3 image encode issues a few thousand dispatches, and submitting each
// one separately costs more than the kernels do.
//
// A global COMPUTE|TRANSFER memory barrier sits between every recorded command,
// which reproduces straight-line data dependence exactly.
//
// Flush points: sync(), download(), a params-ring wrap, the command-buffer
// ring running dry, and a batch reaching its dispatch or work cap.

#include "nn/vk/Context.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Pipelines.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nn {
namespace vk {

class Stream {
public:
    static Stream& get();
    static void    shutdown();

    // --- dispatch ---------------------------------------------------------
    // `params` is the entry point's `uniform` struct (dispatchBig(): through the
    // params ring); `work` is its cost in FLOPs, what the submit budget counts.
    void dispatch(const char* entry, const SpecList& spec, uint32_t gx, uint32_t gy,
                  uint32_t gz, const void* params, uint32_t params_size, double work = 0);
    void dispatchBig(const char* entry, const SpecList& spec, uint32_t gx, uint32_t gy,
                     uint32_t gz, const void* params, uint32_t params_size,
                     double work = 0);

    // Flat 1-D helper: folds `total` threads into `block`-wide workgroups.
    // Vulkan caps EVERY dispatch dimension at 65535, so long ranges fold into a
    // second grid row and the shader recovers the flat index as
    // ((gy * groups_per_row + gx) * block + tid). `groups_per_row_field`, when
    // given, is written into the params struct before the dispatch.
    struct Fold { uint32_t per_row, rows; };
    static Fold fold1D(int64_t total, uint32_t block);
    // Its work defaults to kElemWork per thread.
    void dispatchFlat(const char* entry, const SpecList& spec, int64_t total,
                      uint32_t block, void* params, uint32_t params_size,
                      uint32_t* groups_per_row_field = nullptr, double work = -1);
    // A memory-bound thread in FLOP-equivalents: ~12 bytes against a GPU's
    // FLOP:byte ratio of 10-50.
    static constexpr double kElemWork = 32;

    // --- memory ops -------------------------------------------------------
    void fill(DevicePtr dst, uint32_t word, VkDeviceSize bytes);
    void zero(DevicePtr dst, VkDeviceSize bytes) { fill(dst, 0u, bytes); }
    void copy(DevicePtr dst, DevicePtr src, VkDeviceSize bytes);

    // Host <-> device. Both are synchronous (they flush); `upload` copies
    // through a persistently mapped staging ring in chunks.
    void upload(DevicePtr dst, const void* src, VkDeviceSize bytes);
    void download(void* dst, DevicePtr src, VkDeviceSize bytes);

    // --- synchronization --------------------------------------------------
    void flush();  // submit what is recorded (does not wait)
    void sync();   // flush + wait for the queue to drain
    // sync() for teardown, which must not throw: destructors free. Once it has
    // failed the device is lost or hung, and every later call returns at once.
    void drain() noexcept;
    // Dispatches recorded before flush() happens on its own. See the definition
    // for why an unbounded command buffer is not an option.
    static uint32_t max_dispatches_per_batch();
    // FLOPs of this kernel one submission may carry before the driver's watchdog
    // is at risk (core/SubmitBudget.h). An op whose single dispatch would exceed it
    // splits.
    double workCap(const char* entry, const SpecList& spec);

    // Makes the NEXT submission wait on an external timeline. Used by the video
    // decoder, whose work runs on a different queue family: the decode signals
    // its own timeline and the copy recorded here waits on it.
    void waitOn(VkSemaphore sem, uint64_t value);
    // This stream's timeline and the value the last flush signalled, so another
    // queue can wait for work recorded here.
    VkSemaphore timeline();
    uint64_t    lastSubmitted();

    // --- escape hatch -----------------------------------------------------
    // Raw access to the open command buffer, for the handful of commands this
    // class does not wrap (image layout transitions and image<->buffer copies
    // in src/video). Anything recorded here is subject to the same batching, so
    // follow it with barrierNow() exactly as dispatch() does internally.
    VkCommandBuffer commandBuffer() { return begin(); }
    void            barrierNow() { barrier(begin()); }

    // --- params ring ------------------------------------------------------
    // Host-visible, device-addressable scratch for oversized param structs.
    // Returns the device address and a mapped pointer to `bytes` of space.
    void paramsAlloc(uint32_t bytes, DevicePtr* addr_out, void** mapped_out);

    // --- profiling --------------------------------------------------------
    // SS_PROFILE=1: per-entry GPU time, printed by report().
    void report();

private:
    Stream() = default;
    ~Stream();
    struct Impl;
    Impl& impl();
    Impl* impl_ = nullptr;

    VkCommandBuffer begin();
    void            barrier(VkCommandBuffer cb);
};

// Blocks the calling thread until `timeline` reaches `value`; a GPU that stops
// making progress throws rather than hanging. Any thread.
void wait_timeline(VkSemaphore timeline, uint64_t value);

// SS_NN_DEBUG_SYNC=1: sync + report after every dispatch, printing the entry
// name and grid BEFORE the dispatch, so a hang bisects to the exact kernel
// even when the sync never returns.
bool debug_sync_enabled();

}  // namespace vk
}  // namespace nn
