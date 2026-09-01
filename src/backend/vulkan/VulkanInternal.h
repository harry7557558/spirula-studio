#pragma once
// Internals shared between the Vulkan runtime (VulkanRuntime.cpp) and the
// dispatch layers built on it (VulkanPipelines, SortScan, kernel launchers).
// Not part of the backend API; include only from backend/vulkan/*.cpp.

#include "backend/api/BackendRuntime.h"
#include "backend/vulkan/VulkanContext.h"

#include <cstdint>
#include <vector>

namespace backend {
namespace vk {

// --- allocations -----------------------------------------------------------

struct Allocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint64_t base = 0;        // device: VkDeviceAddress; pinned: mapped addr
    VkDeviceSize size = 0;    // rounded-up capacity
    void* mapped = nullptr;   // non-null for host-pinned allocations
    uint64_t device_addr = 0; // buffer device address (all allocations)
};

// Resolves a pointer that may be a device address (from device_malloc) or a
// pinned-host pointer (from host_malloc_pinned). On success fills `alloc`
// and `offset` (byte offset of `p` inside the allocation) and returns true.
// `is_device` distinguishes the two maps.
bool resolve_pointer(const void* p, Allocation* alloc, VkDeviceSize* offset,
                     bool* is_device);

// --- streams ---------------------------------------------------------------

// A stream is a command-batch context on the single compute queue. Command
// buffers come from a small per-stream ring; submission signals the shared
// queue timeline (Context::submit).
struct StreamImpl {
    VkCommandPool pool = VK_NULL_HANDLE;
    static constexpr int kRing = 4;
    VkCommandBuffer cbs[kRing] = {};
    uint64_t cb_submitted[kRing] = {};  // timeline value of each ring entry
    int cur = 0;
    bool recording = false;
    uint64_t last_value = 0;  // last timeline value signaled by this stream
};

// Maps the public handle (nullptr = default stream) to its impl, creating
// lazily. Returns nullptr only if the context failed to initialize.
StreamImpl* stream_impl(Stream stream);

// Command buffer of `s` in recording state (begins one if needed).
// VK_NULL_HANDLE on failure.
VkCommandBuffer stream_begin(StreamImpl* s);

// Ends and submits the open command buffer, if any. Returns the timeline
// value of the submit (or s->last_value if nothing was pending).
uint64_t stream_flush(StreamImpl* s);

// Records a global memory barrier covering compute+transfer in both scopes.
// Called after every dispatch/copy recorded into a batch; reproduces CUDA
// stream ordering (see README).
void stream_barrier(VkCommandBuffer cb);
void host_read_barrier(VkCommandBuffer cb);

// Flushes every live stream and returns the highest submitted value.
uint64_t flush_all_streams();

// --- staging ring ----------------------------------------------------------

// Persistently mapped host-visible staging ring for pageable-host copies.
// acquire() blocks (on the queue timeline) until `bytes` of the ring are
// reclaimable; the region stays reserved until the timeline passes the value
// later passed to release(). Single-threaded per use site; internally locked.
struct StagingRegion {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    void* mapped = nullptr;
};
// Destroys every device child owned by the runtime layer (stream command
// pools, staging ring, query pool, still-live allocations). Installed as the
// Context shutdown hook; runs inside ~Context after the device-idle wait.
void runtime_shutdown();

// bytes must be <= staging_max_chunk(). Returns false on failure.
bool staging_acquire(VkDeviceSize bytes, StagingRegion* out);
// Marks the last acquired region as in-use until `timeline_value`.
void staging_release(const StagingRegion& region, VkDeviceSize bytes,
                     uint64_t timeline_value);
VkDeviceSize staging_max_chunk();

// --- kernel-params ring ------------------------------------------------------

// Host-visible, device-addressable ring for kernel parameter structs larger
// than the push-constant floor (the launcher writes the struct here and
// pushes only its 8-byte device address). On wrap the ring flushes every
// stream and waits — the ring is sized so that is rare. Returns false on
// failure (sticky error set).
bool params_alloc(uint32_t bytes, uint64_t* device_addr, void** mapped);

// --- GPU-timestamp kernel timing (SS_PROFILE only) ---------------------
// Bracket a dispatch to measure its true on-device execution time via
// timestamp queries. gpu_ts_begin() writes a TOP-of-pipe timestamp into a
// freshly acquired query slot and returns a handle (>= 0), or -1 when
// profiling is off / the device lacks timestamps / the query pool is
// exhausted. gpu_ts_end() writes the matching BOTTOM-of-pipe timestamp.
// gpu_ts_resolve() -- called after a full device drain, so results are ready
// -- reads every completed pair, accumulates GPU time into the profiler's GPU
// bucket, and recycles the slots. All no-ops unless SS_PROFILE is set.
int  gpu_ts_begin(VkCommandBuffer cb, const char* entry);
void gpu_ts_end(VkCommandBuffer cb, int handle);
void gpu_ts_resolve();
// Prints the per-entry-point GPU-time breakdown (called at shutdown when
// SS_PROFILE is set); reveals which kernel dominates device time.
void gpu_ts_report_by_entry();

}  // namespace vk
}  // namespace backend
