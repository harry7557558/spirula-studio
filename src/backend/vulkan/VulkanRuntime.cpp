// Vulkan implementation of backend/api/BackendRuntime.h (see README.md).
//
// Memory: one VkBuffer + dedicated VkDeviceMemory per device_malloc, plain
// device addresses (VK_KHR_buffer_device_address) as the pointer type.
// Pinned host memory: persistently-mapped host-visible buffers (returns the
// mapped pointer). Streams: per-stream command batches on one compute queue,
// ordered by a shared timeline semaphore. Sync transfers are conservatively
// device-ordered (flush every stream, wait, transfer, wait) — the CUDA-side
// semantics of cudaMemcpy, traded for simplicity first.

#include "backend/vulkan/VulkanInternal.h"

#include "backend/common/Profiler.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "core/Env.h"

namespace backend {
namespace vk {

extern std::mutex g_error_mutex;   // VulkanContext.cpp
extern std::string g_error;

// Live device_malloc byte total (process VRAM estimate). Defined here, read
// by backend::memory_usage() in VulkanContext.cpp. Host-pinned allocations
// are excluded — they are not device-local VRAM.
std::atomic<uint64_t> g_device_bytes{0};

namespace {

// ---------------------------------------------------------------------------
// allocation maps
// ---------------------------------------------------------------------------

std::mutex g_alloc_mutex;
std::map<uint64_t, Allocation> g_device_allocs;  // key: device address
std::map<uint64_t, Allocation> g_host_allocs;    // key: mapped host address

bool lookup(const std::map<uint64_t, Allocation>& m, uint64_t addr,
            Allocation* alloc, VkDeviceSize* offset) {
    auto it = m.upper_bound(addr);
    if (it == m.begin()) return false;
    --it;
    if (addr >= it->first + it->second.size) return false;
    *alloc = it->second;
    *offset = addr - it->first;
    return true;
}

// Creates buffer + dedicated memory. `host_visible` selects the pinned pool
// path (persistently mapped, no device address).
bool create_allocation(VkDeviceSize bytes, bool host_visible,
                       Allocation* out) {
    Context& ctx = Context::get();
    if (!ctx.ok()) return false;
    ctx.set_shutdown_hook(&runtime_shutdown);

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = bytes;
    // Every allocation gets a device address: pinned host memory doubles as
    // the kernel-params ring, and kernels may read from it directly.
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (!host_visible)
        bci.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Allocation a{};
    VkResult r = vkCreateBuffer(ctx.device(), &bci, nullptr, &a.buffer);
    if (r != VK_SUCCESS) {
        set_error("vkCreateBuffer failed", r);
        return false;
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx.device(), a.buffer, &req);

    uint32_t type;
    if (host_visible) {
        type = ctx.find_memory_type(req.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        if (type == UINT32_MAX)
            type = ctx.find_memory_type(req.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    } else {
        type = ctx.find_memory_type(req.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (type == UINT32_MAX)
            type = ctx.find_memory_type(req.memoryTypeBits, 0);
    }
    if (type == UINT32_MAX) {
        set_error("no suitable Vulkan memory type", VK_SUCCESS);
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        return false;
    }

    VkMemoryAllocateFlagsInfo mfi{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    mfi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.pNext = &mfi;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;

    r = vkAllocateMemory(ctx.device(), &mai, nullptr, &a.memory);
    if (r != VK_SUCCESS) {
        set_error("vkAllocateMemory failed", r);
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        return false;
    }
    r = vkBindBufferMemory(ctx.device(), a.buffer, a.memory, 0);
    if (r != VK_SUCCESS) {
        set_error("vkBindBufferMemory failed", r);
        vkFreeMemory(ctx.device(), a.memory, nullptr);
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        return false;
    }
    a.size = bytes;

    VkBufferDeviceAddressInfo bdai{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bdai.buffer = a.buffer;
    a.device_addr = vkGetBufferDeviceAddress(ctx.device(), &bdai);
    if (a.device_addr == 0) {
        set_error("vkGetBufferDeviceAddress returned 0", VK_SUCCESS);
        vkFreeMemory(ctx.device(), a.memory, nullptr);
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        return false;
    }

    if (host_visible) {
        r = vkMapMemory(ctx.device(), a.memory, 0, VK_WHOLE_SIZE, 0,
                        &a.mapped);
        if (r != VK_SUCCESS) {
            set_error("vkMapMemory failed", r);
            vkFreeMemory(ctx.device(), a.memory, nullptr);
            vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
            return false;
        }
        a.base = (uint64_t)a.mapped;
    } else {
        a.base = a.device_addr;
    }
    *out = a;
    return true;
}

void destroy_allocation(const Allocation& a) {
    Context& ctx = Context::get();
    if (a.mapped) vkUnmapMemory(ctx.device(), a.memory);
    vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
    vkFreeMemory(ctx.device(), a.memory, nullptr);
}

// ---------------------------------------------------------------------------
// streams
// ---------------------------------------------------------------------------

std::mutex g_stream_mutex;
std::vector<StreamImpl*> g_streams;  // all live stream contexts
StreamImpl* g_default_stream = nullptr;

StreamImpl* create_stream_impl() {
    Context& ctx = Context::get();
    if (!ctx.ok()) return nullptr;
    ctx.set_shutdown_hook(&runtime_shutdown);
    auto* s = new StreamImpl();
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = ctx.queue_family();
    VkResult r = vkCreateCommandPool(ctx.device(), &pci, nullptr, &s->pool);
    if (r != VK_SUCCESS) {
        set_error("vkCreateCommandPool failed", r);
        delete s;
        return nullptr;
    }
    VkCommandBufferAllocateInfo cai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = s->pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = StreamImpl::kRing;
    r = vkAllocateCommandBuffers(ctx.device(), &cai, s->cbs);
    if (r != VK_SUCCESS) {
        set_error("vkAllocateCommandBuffers failed", r);
        vkDestroyCommandPool(ctx.device(), s->pool, nullptr);
        delete s;
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_stream_mutex);
    g_streams.push_back(s);
    return s;
}

// ---------------------------------------------------------------------------
// staging ring
// ---------------------------------------------------------------------------

std::mutex g_transfer_mutex;  // serializes every staged transfer end-to-end
Allocation g_staging{};
VkDeviceSize g_staging_cursor = 0;
struct StagingReservation {
    VkDeviceSize begin, end;
    uint64_t value;  // queue timeline value that releases it
};
std::deque<StagingReservation> g_staging_reservations;

VkDeviceSize staging_capacity() {
    static VkDeviceSize cap = [] {
        long mb = 64;
        if (const char* env = spirula::env("VK_STAGING_MB"))
            mb = std::max(1L, std::strtol(env, nullptr, 10));
        return (VkDeviceSize)mb << 20;
    }();
    return cap;
}

bool staging_init() {
    if (g_staging.buffer != VK_NULL_HANDLE) return true;
    return create_allocation(staging_capacity(), /*host_visible=*/true,
                             &g_staging);
}

}  // namespace

// --- internal API (VulkanInternal.h) ---------------------------------------

bool resolve_pointer(const void* p, Allocation* alloc, VkDeviceSize* offset,
                     bool* is_device) {
    std::lock_guard<std::mutex> lock(g_alloc_mutex);
    if (lookup(g_device_allocs, (uint64_t)p, alloc, offset)) {
        *is_device = true;
        return true;
    }
    if (lookup(g_host_allocs, (uint64_t)p, alloc, offset)) {
        *is_device = false;
        return true;
    }
    return false;
}

StreamImpl* stream_impl(Stream stream) {
    if (stream != kDefaultStream) return (StreamImpl*)stream;
    static std::once_flag once;
    std::call_once(once, [] { g_default_stream = create_stream_impl(); });
    return g_default_stream;
}

VkCommandBuffer stream_begin(StreamImpl* s) {
    if (!s) return VK_NULL_HANDLE;
    Context& ctx = Context::get();
    if (s->recording) return s->cbs[s->cur];
    // Reuse the next ring entry once its previous submission completed.
    int next = (s->cur + 1) % StreamImpl::kRing;
    if (!ctx.wait(s->cb_submitted[next])) return VK_NULL_HANDLE;
    s->cur = next;
    VkCommandBuffer cb = s->cbs[s->cur];
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult r = vkBeginCommandBuffer(cb, &bi);
    if (r != VK_SUCCESS) {
        set_error("vkBeginCommandBuffer failed", r);
        return VK_NULL_HANDLE;
    }
    s->recording = true;
    return cb;
}

uint64_t stream_flush(StreamImpl* s) {
    if (!s) return 0;
    if (!s->recording) return s->last_value;
    Context& ctx = Context::get();
    VkCommandBuffer cb = s->cbs[s->cur];
    s->recording = false;
    VkResult r = vkEndCommandBuffer(cb);
    if (r != VK_SUCCESS) {
        set_error("vkEndCommandBuffer failed", r);
        return s->last_value;
    }
    uint64_t value = ctx.submit(cb);
    if (value == 0) return s->last_value;  // submit failed (error set)
    s->cb_submitted[s->cur] = value;
    s->last_value = value;
    return value;
}

void stream_barrier(VkCommandBuffer cb) {
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT |
                       VK_ACCESS_TRANSFER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                       VK_ACCESS_SHADER_WRITE_BIT |
                       VK_ACCESS_TRANSFER_READ_BIT |
                       VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &mb, 0, nullptr, 0, nullptr);
}

// Device writes reach a mapped pointer only through a barrier naming the host
// stage. Desktop drivers tolerate its absence; GB10 unified memory hands back
// stale lines without it.
void host_read_barrier(VkCommandBuffer cb) {
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT |
                       VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

uint64_t flush_all_streams() {
    std::vector<StreamImpl*> streams;
    {
        std::lock_guard<std::mutex> lock(g_stream_mutex);
        streams = g_streams;
    }
    uint64_t max_value = 0;
    for (StreamImpl* s : streams)
        max_value = std::max(max_value, stream_flush(s));
    return std::max(max_value, Context::get().last_submitted());
}

bool staging_acquire(VkDeviceSize bytes, StagingRegion* out) {
    // Caller holds g_transfer_mutex (all transfer paths in this file do).
    if (!staging_init()) return false;
    if (bytes > staging_capacity()) {
        set_error("staging_acquire: chunk exceeds ring capacity", VK_SUCCESS);
        return false;
    }
    Context& ctx = Context::get();
    VkDeviceSize place = g_staging_cursor;
    if (place + bytes > staging_capacity()) place = 0;
    // Retire (in FIFO order) every reservation overlapping the target range.
    // All reservations carry real timeline values (release happens before
    // the transfer mutex is dropped), so this always terminates.
    while (!g_staging_reservations.empty()) {
        bool any_overlap = false;
        for (const auto& res : g_staging_reservations)
            if (res.begin < place + bytes && res.end > place) {
                any_overlap = true;
                break;
            }
        if (!any_overlap) break;
        if (!ctx.wait(g_staging_reservations.front().value)) return false;
        g_staging_reservations.pop_front();
    }
    out->buffer = g_staging.buffer;
    out->offset = place;
    out->mapped = (char*)g_staging.mapped + place;
    g_staging_cursor = place + bytes;
    return true;
}

void staging_release(const StagingRegion& region, VkDeviceSize bytes,
                     uint64_t timeline_value) {
    g_staging_reservations.push_back(
        {region.offset, region.offset + bytes, timeline_value});
}

VkDeviceSize staging_max_chunk() { return staging_capacity(); }

namespace {
std::mutex g_params_mutex;
Allocation g_params_ring{};
VkDeviceSize g_params_cursor = 0;
constexpr VkDeviceSize kParamsRingSize = 1 << 20;  // ~5k dispatches per wrap
}  // namespace

bool params_alloc(uint32_t bytes, uint64_t* device_addr, void** mapped) {
    std::lock_guard<std::mutex> lock(g_params_mutex);
    if (g_params_ring.buffer == VK_NULL_HANDLE) {
        if (!create_allocation(kParamsRingSize, /*host_visible=*/true,
                               &g_params_ring))
            return false;
    }
    VkDeviceSize rounded = (bytes + 63u) / 64u * 64u;
    if (rounded > kParamsRingSize) {
        set_error("params_alloc: params struct exceeds ring size", VK_SUCCESS);
        return false;
    }
    if (g_params_cursor + rounded > kParamsRingSize) {
        // Wrap: everything recorded so far may still reference the ring.
        if (!Context::get().wait(flush_all_streams())) return false;
        g_params_cursor = 0;
    }
    *device_addr = g_params_ring.device_addr + g_params_cursor;
    *mapped = (char*)g_params_ring.mapped + g_params_cursor;
    g_params_cursor += rounded;
    return true;
}

namespace {

// One-shot transfer recording on the default stream: begin, record via `fn`,
// submit, wait. Used by all _sync transfer paths (which have already
// device-synchronized).
template <typename Fn>
bool record_and_wait(Fn&& fn) {
    StreamImpl* s = stream_impl(kDefaultStream);
    VkCommandBuffer cb = stream_begin(s);
    if (cb == VK_NULL_HANDLE) return false;
    fn(cb);
    uint64_t value = stream_flush(s);
    return Context::get().wait(value);
}

}  // namespace

}  // namespace vk

// ===========================================================================
// backend:: public API
// ===========================================================================

namespace {

// Round allocation sizes up so whole-buffer vkCmdFillBuffer never needs a
// byte tail (see README "Memory model") and device addresses of distinct
// allocations never touch.
constexpr size_t kAllocRound = 16;

}  // namespace

void* device_malloc(size_t bytes) {
    if (bytes == 0) return nullptr;  // cudaMalloc(0) analog
    size_t rounded = (bytes + kAllocRound - 1) / kAllocRound * kAllocRound;
    vk::Allocation a;
    if (!vk::create_allocation(rounded, /*host_visible=*/false, &a))
        return nullptr;
    {
        std::lock_guard<std::mutex> lock(vk::g_alloc_mutex);
        vk::g_device_allocs[a.base] = a;
    }
    vk::g_device_bytes.fetch_add(a.size, std::memory_order_relaxed);
    return (void*)a.base;
}

void device_free(void* ptr) {
    if (!ptr) return;
    vk::Allocation a;
    {
        std::lock_guard<std::mutex> lock(vk::g_alloc_mutex);
        auto it = vk::g_device_allocs.find((uint64_t)ptr);
        if (it == vk::g_device_allocs.end()) {
            vk::set_error("device_free: not an allocation base pointer",
                          VK_SUCCESS);
            return;
        }
        a = it->second;
        vk::g_device_allocs.erase(it);
    }
    vk::g_device_bytes.fetch_sub(a.size, std::memory_order_relaxed);
    // Contract (BackendRuntime.h): all in-flight work completes first.
    vk::Context::get().wait(vk::flush_all_streams());
    vk::destroy_allocation(a);
}

void* host_malloc_pinned(size_t bytes) {
    if (bytes == 0) return nullptr;
    size_t rounded = (bytes + kAllocRound - 1) / kAllocRound * kAllocRound;
    vk::Allocation a;
    if (!vk::create_allocation(rounded, /*host_visible=*/true, &a))
        return nullptr;
    {
        std::lock_guard<std::mutex> lock(vk::g_alloc_mutex);
        vk::g_host_allocs[a.base] = a;
    }
    return a.mapped;
}

void host_free_pinned(void* ptr) {
    if (!ptr) return;
    vk::Allocation a;
    {
        std::lock_guard<std::mutex> lock(vk::g_alloc_mutex);
        auto it = vk::g_host_allocs.find((uint64_t)ptr);
        if (it == vk::g_host_allocs.end()) {
            vk::set_error("host_free_pinned: not an allocation base pointer",
                          VK_SUCCESS);
            return;
        }
        a = it->second;
        vk::g_host_allocs.erase(it);
    }
    vk::Context::get().wait(vk::flush_all_streams());
    vk::destroy_allocation(a);
}

namespace {

struct ResolvedPtr {
    bool known = false;
    bool is_device = false;
    vk::Allocation alloc;
    VkDeviceSize offset = 0;
};

ResolvedPtr resolve(const void* p) {
    ResolvedPtr r;
    r.known = vk::resolve_pointer(p, &r.alloc, &r.offset, &r.is_device);
    return r;
}

bool range_ok(const ResolvedPtr& r, size_t bytes, const char* what) {
    if (r.offset + bytes > r.alloc.size) {
        vk::set_error(what, VK_SUCCESS);
        return false;
    }
    return true;
}

// Sync H2D from pageable host memory via the staging ring.
void staged_upload_sync(const ResolvedPtr& dst, const void* src,
                        size_t bytes) {
    std::lock_guard<std::mutex> lock(vk::g_transfer_mutex);
    size_t done = 0;
    while (done < bytes) {
        size_t n = std::min((size_t)vk::staging_max_chunk(), bytes - done);
        vk::StagingRegion reg;
        if (!vk::staging_acquire(n, &reg)) return;
        std::memcpy(reg.mapped, (const char*)src + done, n);
        vk::record_and_wait([&](VkCommandBuffer cb) {
            VkBufferCopy c{reg.offset, dst.offset + done, n};
            vkCmdCopyBuffer(cb, reg.buffer, dst.alloc.buffer, 1, &c);
        });
        done += n;
    }
}

// Sync D2H to pageable host memory via the staging ring.
void staged_download_sync(void* dst, const ResolvedPtr& src, size_t bytes) {
    std::lock_guard<std::mutex> lock(vk::g_transfer_mutex);
    size_t done = 0;
    while (done < bytes) {
        size_t n = std::min((size_t)vk::staging_max_chunk(), bytes - done);
        vk::StagingRegion reg;
        if (!vk::staging_acquire(n, &reg)) return;
        vk::record_and_wait([&](VkCommandBuffer cb) {
            VkBufferCopy c{src.offset + done, reg.offset, n};
            vkCmdCopyBuffer(cb, src.alloc.buffer, reg.buffer, 1, &c);
            vk::host_read_barrier(cb);
        });
        std::memcpy((char*)dst + done, reg.mapped, n);
        done += n;
    }
}

}  // namespace

void memcpy_sync(void* dst, const void* src, size_t bytes, MemcpyKind kind) {
    if (bytes == 0) return;
    ResolvedPtr d = resolve(dst), s = resolve(src);

    bool dst_dev = kind == MemcpyKind::HostToDevice ||
                   kind == MemcpyKind::DeviceToDevice ||
                   (kind == MemcpyKind::Auto && d.known && d.is_device);
    bool src_dev = kind == MemcpyKind::DeviceToHost ||
                   kind == MemcpyKind::DeviceToDevice ||
                   (kind == MemcpyKind::Auto && s.known && s.is_device);

    if (dst_dev && (!d.known || !d.is_device)) {
        vk::set_error("memcpy_sync: dst is not a device pointer", VK_SUCCESS);
        return;
    }
    if (src_dev && (!s.known || !s.is_device)) {
        vk::set_error("memcpy_sync: src is not a device pointer", VK_SUCCESS);
        return;
    }

    if (!dst_dev && !src_dev) {  // host to host
        std::memcpy(dst, src, bytes);
        return;
    }
    if (dst_dev && !range_ok(d, bytes, "memcpy_sync: dst range overflow"))
        return;
    if (src_dev && !range_ok(s, bytes, "memcpy_sync: src range overflow"))
        return;

    // Profiling: attribute the pending-work drain to DEVSYNC so the copy
    // scope below measures pure transfer (the wait() a few lines down is then
    // a no-op). Same scheme as the CUDA backend for comparable numbers.
    std::optional<prof::Scope> _psc;
    if (prof::enabled()) {
        prof::Cat c = (dst_dev && src_dev) ? prof::D2D
                      : dst_dev            ? prof::H2D
                                           : prof::D2H;
        prof::drain_before_copy();
        _psc.emplace(c, bytes);
    }

    // cudaMemcpy is ordered after previously issued device work.
    vk::Context::get().wait(vk::flush_all_streams());

    if (dst_dev && src_dev) {
        vk::record_and_wait([&](VkCommandBuffer cb) {
            VkBufferCopy c{s.offset, d.offset, bytes};
            vkCmdCopyBuffer(cb, s.alloc.buffer, d.alloc.buffer, 1, &c);
        });
    } else if (dst_dev) {
        if (s.known) {  // pinned source: direct buffer-to-buffer copy
            if (!range_ok(s, bytes, "memcpy_sync: pinned src overflow"))
                return;
            vk::record_and_wait([&](VkCommandBuffer cb) {
                VkBufferCopy c{s.offset, d.offset, bytes};
                vkCmdCopyBuffer(cb, s.alloc.buffer, d.alloc.buffer, 1, &c);
            });
        } else {
            staged_upload_sync(d, src, bytes);
        }
    } else {
        if (d.known) {  // pinned destination
            if (!range_ok(d, bytes, "memcpy_sync: pinned dst overflow"))
                return;
            vk::record_and_wait([&](VkCommandBuffer cb) {
                VkBufferCopy c{s.offset, d.offset, bytes};
                vkCmdCopyBuffer(cb, s.alloc.buffer, d.alloc.buffer, 1, &c);
                vk::host_read_barrier(cb);
            });
        } else {
            staged_download_sync(dst, s, bytes);
        }
    }
}

void memcpy_async(void* dst, const void* src, size_t bytes, MemcpyKind kind,
                  Stream stream) {
    if (bytes == 0) return;
    vk::StreamImpl* st = vk::stream_impl(stream);
    if (!st) return;
    ResolvedPtr d = resolve(dst), s = resolve(src);

    bool dst_dev = kind == MemcpyKind::HostToDevice ||
                   kind == MemcpyKind::DeviceToDevice ||
                   (kind == MemcpyKind::Auto && d.known && d.is_device);
    bool src_dev = kind == MemcpyKind::DeviceToHost ||
                   kind == MemcpyKind::DeviceToDevice ||
                   (kind == MemcpyKind::Auto && s.known && s.is_device);

    if ((dst_dev && (!d.known || !d.is_device)) ||
        (src_dev && (!s.known || !s.is_device))) {
        vk::set_error("memcpy_async: pointer kind mismatch", VK_SUCCESS);
        return;
    }

    if (!dst_dev && !src_dev) {  // host to host, ordered on the stream
        stream_synchronize(stream);
        std::memcpy(dst, src, bytes);
        return;
    }

    // Profiling: async copies only record here (the transfer completes later);
    // the drain lands in a subsequent sync/event bucket, as on CUDA.
    std::optional<prof::Scope> _psc;
    if (prof::enabled()) {
        prof::Cat c = (dst_dev && src_dev) ? prof::D2D
                      : dst_dev            ? prof::H2D
                                           : prof::D2H;
        _psc.emplace(c, bytes);
    }

    if (dst_dev && src_dev) {
        if (!range_ok(s, bytes, "memcpy_async: src range overflow") ||
            !range_ok(d, bytes, "memcpy_async: dst range overflow"))
            return;
        VkCommandBuffer cb = vk::stream_begin(st);
        if (cb == VK_NULL_HANDLE) return;
        VkBufferCopy c{s.offset, d.offset, bytes};
        vkCmdCopyBuffer(cb, s.alloc.buffer, d.alloc.buffer, 1, &c);
        vk::stream_barrier(cb);
        return;
    }

    if (dst_dev) {  // upload
        if (!range_ok(d, bytes, "memcpy_async: dst range overflow")) return;
        if (s.known) {  // pinned source: record directly, stays async
            if (!range_ok(s, bytes, "memcpy_async: pinned src overflow"))
                return;
            VkCommandBuffer cb = vk::stream_begin(st);
            if (cb == VK_NULL_HANDLE) return;
            VkBufferCopy c{s.offset, d.offset, bytes};
            vkCmdCopyBuffer(cb, s.alloc.buffer, d.alloc.buffer, 1, &c);
            vk::stream_barrier(cb);
            return;
        }
        // Pageable source: stage chunks; each chunk is submitted (not
        // waited), so the call returns once the data is staged — the CUDA
        // pageable-async contract.
        std::lock_guard<std::mutex> lock(vk::g_transfer_mutex);
        size_t done = 0;
        while (done < bytes) {
            size_t n =
                std::min((size_t)vk::staging_max_chunk(), bytes - done);
            vk::StagingRegion reg;
            if (!vk::staging_acquire(n, &reg)) return;
            std::memcpy(reg.mapped, (const char*)src + done, n);
            VkCommandBuffer cb = vk::stream_begin(st);
            if (cb == VK_NULL_HANDLE) return;
            VkBufferCopy c{reg.offset, d.offset + done, n};
            vkCmdCopyBuffer(cb, reg.buffer, d.alloc.buffer, 1, &c);
            vk::stream_barrier(cb);
            uint64_t value = vk::stream_flush(st);
            vk::staging_release(reg, n, value);
            done += n;
        }
        return;
    }

    // download
    if (!range_ok(s, bytes, "memcpy_async: src range overflow")) return;
    if (d.known) {  // pinned destination: record directly, stays async
        if (!range_ok(d, bytes, "memcpy_async: pinned dst overflow")) return;
        VkCommandBuffer cb = vk::stream_begin(st);
        if (cb == VK_NULL_HANDLE) return;
        VkBufferCopy c{s.offset, d.offset, bytes};
        vkCmdCopyBuffer(cb, s.alloc.buffer, d.alloc.buffer, 1, &c);
        vk::stream_barrier(cb);
        vk::host_read_barrier(cb);
        return;
    }
    // Pageable destination degrades to sync (as CUDA does).
    vk::Context::get().wait(vk::stream_flush(st));
    staged_download_sync(dst, s, bytes);
}

namespace {

bool fill_params(const ResolvedPtr& d, size_t bytes, int value,
                 VkDeviceSize* fill_bytes, uint32_t* word,
                 const char* what) {
    if (d.offset % 4 != 0) {
        vk::set_error(what, VK_SUCCESS);  // non-4-aligned fill
        return false;
    }
    VkDeviceSize rounded = (bytes + 3) / 4 * 4;
    if (d.offset + rounded > d.alloc.size) {
        vk::set_error(what, VK_SUCCESS);  // tail would leave the allocation
        return false;
    }
    uint32_t b = (uint32_t)(value & 0xff);
    *word = b * 0x01010101u;
    *fill_bytes = rounded;
    return true;
}

// Stream-ordered device fill, shared by both memset entry points.
void record_fill(const ResolvedPtr& d, size_t bytes, VkDeviceSize fill,
                 uint32_t word, Stream stream) {
    vk::StreamImpl* st = vk::stream_impl(stream);
    if (!st) return;
    // Opening a fresh command buffer waits for the ring entry's previous
    // submission, so it is a GPU wait and not fill cost: charging it to
    // MEMSET made a free 450 MB/step of zeroing read as 12 s of a 56 s run.
    std::optional<prof::Scope> _pwait;
    if (prof::enabled() && !st->recording) _pwait.emplace(prof::DEVSYNC);
    VkCommandBuffer cb = vk::stream_begin(st);
    _pwait.reset();
    if (cb == VK_NULL_HANDLE) return;
    std::optional<prof::Scope> _pms;
    if (prof::enabled()) _pms.emplace(prof::MEMSET, bytes);
    vkCmdFillBuffer(cb, d.alloc.buffer, d.offset, fill, word);
    vk::stream_barrier(cb);
}

}  // namespace

// Ordered against other work on the default stream but not a host wait --
// cudaMemset's semantics, which the CUDA backend gets for free. Tensor::zero()
// issues ~22 of these per training step, so draining the queue and submitting
// alone instead cost 18% of a run on Apple silicon and 10% on an RTX 5070.
void memset_sync(void* dst, int value, size_t bytes) {
    if (bytes == 0) return;
    ResolvedPtr d = resolve(dst);
    if (!d.known || !d.is_device) {
        if (d.known) {  // pinned host
            std::memset(dst, value, bytes);
            return;
        }
        std::memset(dst, value, bytes);  // pageable host (cudaMemset would
        return;                          // fail; engine never does this)
    }
    VkDeviceSize fill;
    uint32_t word;
    if (!fill_params(d, bytes, value, &fill, &word,
                     "memset_sync: unaligned or out-of-range fill"))
        return;
    record_fill(d, bytes, fill, word, kDefaultStream);
}

void memset_async(void* dst, int value, size_t bytes, Stream stream) {
    if (bytes == 0) return;
    ResolvedPtr d = resolve(dst);
    if (!d.known || !d.is_device) {
        stream_synchronize(stream);
        std::memset(dst, value, bytes);
        return;
    }
    VkDeviceSize fill;
    uint32_t word;
    if (!fill_params(d, bytes, value, &fill, &word,
                     "memset_async: unaligned or out-of-range fill"))
        return;
    record_fill(d, bytes, fill, word, stream);
}

bool is_device_pointer(const void* ptr) {
    if (!ptr) return false;
    vk::Allocation a;
    VkDeviceSize off;
    bool is_dev;
    return vk::resolve_pointer(ptr, &a, &off, &is_dev) && is_dev;
}

const char* last_error() {
    static std::string holder;  // static storage, as documented
    std::lock_guard<std::mutex> lock(vk::g_error_mutex);
    if (vk::g_error.empty()) return nullptr;
    holder = vk::g_error;
    vk::g_error.clear();
    return holder.c_str();
}

void device_synchronize() {
    if (prof::enabled()) {
        {  // scope ends (records DEVSYNC) before we read timestamps
            prof::Scope _p(prof::DEVSYNC);
            vk::Context::get().wait(vk::flush_all_streams());
        }
        vk::gpu_ts_resolve();  // work is drained -> timestamps are ready
        return;
    }
    vk::Context::get().wait(vk::flush_all_streams());
}

void stream_synchronize(Stream stream) {
    vk::StreamImpl* st = vk::stream_impl(stream);
    if (!st) return;
    std::optional<prof::Scope> _p;
    if (prof::enabled()) _p.emplace(prof::DEVSYNC);
    vk::Context::get().wait(vk::stream_flush(st));
}

// --- events ----------------------------------------------------------------

struct Event {
    uint64_t value = 0;
    bool timing = false;
    int slot = -1;  // timestamp query slot, when timing
};

namespace {

constexpr uint32_t kMaxTimestamps = 4096;
std::mutex g_query_mutex;
VkQueryPool g_query_pool = VK_NULL_HANDLE;
std::vector<int> g_query_free;

int query_slot_acquire() {
    vk::Context& ctx = vk::Context::get();
    if (!ctx.ok() || !ctx.caps().timestamps) return -1;
    std::lock_guard<std::mutex> lock(g_query_mutex);
    if (g_query_pool == VK_NULL_HANDLE) {
        VkQueryPoolCreateInfo qci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qci.queryCount = kMaxTimestamps;
        VkResult r =
            vkCreateQueryPool(ctx.device(), &qci, nullptr, &g_query_pool);
        if (r != VK_SUCCESS) {
            vk::set_error("vkCreateQueryPool failed", r);
            return -1;
        }
        g_query_free.reserve(kMaxTimestamps);
        for (int i = kMaxTimestamps - 1; i >= 0; i--)
            g_query_free.push_back(i);
    }
    if (g_query_free.empty()) return -1;
    int slot = g_query_free.back();
    g_query_free.pop_back();
    return slot;
}

void query_slot_release(int slot) {
    if (slot < 0) return;
    std::lock_guard<std::mutex> lock(g_query_mutex);
    g_query_free.push_back(slot);
}

// GPU-timestamp kernel timing (SS_PROFILE): outstanding (start,end) query
// slot pairs, one per timed dispatch, resolved after a device drain.
struct TsPair { int qs, qe; const char* entry; };
std::mutex g_ts_mutex;
std::vector<TsPair> g_ts_pending;
uint64_t g_ts_untimed = 0;  // dispatches skipped when the pool was exhausted
bool g_ts_warned = false;
// Per-entry-point GPU time accumulation: name -> {dispatch count, ns}.
struct TsEntry { uint64_t count = 0, ns = 0; };
std::map<std::string, TsEntry> g_ts_by_entry;

}  // namespace

Event* event_create(bool enable_timing) {
    Event* e = new Event();
    e->timing = enable_timing;
    if (enable_timing) e->slot = query_slot_acquire();
    return e;
}

void event_record(Event* event, Stream stream) {
    if (!event) return;
    vk::StreamImpl* st = vk::stream_impl(stream);
    if (!st) return;
    if (event->timing && event->slot >= 0) {
        VkCommandBuffer cb = vk::stream_begin(st);
        if (cb != VK_NULL_HANDLE) {
            vkCmdResetQueryPool(cb, g_query_pool, event->slot, 1);
            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                g_query_pool, event->slot);
        }
    }
    event->value = vk::stream_flush(st);
}

void event_synchronize(Event* event) {
    if (!event) return;
    std::optional<prof::Scope> _p;
    if (prof::enabled()) _p.emplace(prof::DEVSYNC);
    vk::Context::get().wait(event->value);
}

void event_destroy(Event* event) {
    if (!event) return;
    query_slot_release(event->slot);
    delete event;
}

float event_elapsed_ms(Event* start, Event* end) {
    vk::Context& ctx = vk::Context::get();
    if (!start || !end || start->slot < 0 || end->slot < 0) {
        vk::set_error("event_elapsed_ms: events not created with timing "
                      "or timestamps unsupported", VK_SUCCESS);
        return 0.0f;
    }
    uint64_t ts[2] = {0, 0};
    VkResult r1 = vkGetQueryPoolResults(
        ctx.device(), g_query_pool, start->slot, 1, sizeof(uint64_t), &ts[0],
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    VkResult r2 = vkGetQueryPoolResults(
        ctx.device(), g_query_pool, end->slot, 1, sizeof(uint64_t), &ts[1],
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    if (r1 != VK_SUCCESS || r2 != VK_SUCCESS) {
        vk::set_error("vkGetQueryPoolResults failed",
                      r1 != VK_SUCCESS ? r1 : r2);
        return 0.0f;
    }
    double dt_ns =
        (double)(ts[1] - ts[0]) * ctx.caps().timestamp_period_ns;
    return (float)(dt_ns / 1.0e6);
}

// --- shutdown (installed as the Context hook; see VulkanInternal.h) --------

namespace vk {

// --- GPU-timestamp kernel timing (see VulkanInternal.h) --------------------

int gpu_ts_begin(VkCommandBuffer cb, const char* entry) {
    if (!prof::enabled()) return -1;
    Context& ctx = Context::get();
    if (!ctx.ok() || !ctx.caps().timestamps) return -1;
    std::lock_guard<std::mutex> lock(g_ts_mutex);
    int qs = query_slot_acquire();
    int qe = query_slot_acquire();
    if (qs < 0 || qe < 0) {  // pool exhausted: dispatch runs untimed
        query_slot_release(qs);
        query_slot_release(qe);
        g_ts_untimed++;
        return -1;
    }
    vkCmdResetQueryPool(cb, g_query_pool, qs, 1);
    // BOTTOM (not TOP): the start stamp must wait for prior work to drain, so
    // that back-to-back dispatches (serialized by stream_barrier) produce
    // non-overlapping [start,end] intervals. A TOP-of-pipe start fires early
    // and makes consecutive intervals overlap -> the sum over-counts GPU time
    // (can exceed the CPU wait, yielding a nonsensical negative latency).
    vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g_query_pool,
                        qs);
    g_ts_pending.push_back({qs, qe, entry ? entry : "?"});
    return qe;  // handle == the end slot
}

void gpu_ts_end(VkCommandBuffer cb, int handle) {
    if (handle < 0) return;
    vkCmdResetQueryPool(cb, g_query_pool, handle, 1);
    vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g_query_pool,
                        handle);
}

void gpu_ts_resolve() {
    if (!prof::enabled()) return;
    std::lock_guard<std::mutex> lock(g_ts_mutex);
    if (g_ts_pending.empty()) {
        if (g_ts_untimed && !g_ts_warned) {
            g_ts_warned = true;
            std::fprintf(stderr,
                         "[spirula-profile] note: %llu dispatch(es) untimed "
                         "(timestamp pool exhausted)\n",
                         (unsigned long long)g_ts_untimed);
        }
        return;
    }
    Context& ctx = Context::get();
    double period = ctx.caps().timestamp_period_ns;
    for (const TsPair& p : g_ts_pending) {
        uint64_t t0 = 0, t1 = 0;
        VkResult r0 = vkGetQueryPoolResults(
            ctx.device(), g_query_pool, p.qs, 1, sizeof(uint64_t), &t0,
            sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        VkResult r1 = vkGetQueryPoolResults(
            ctx.device(), g_query_pool, p.qe, 1, sizeof(uint64_t), &t1,
            sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        if (r0 == VK_SUCCESS && r1 == VK_SUCCESS && t1 >= t0) {
            uint64_t dt = (uint64_t)((double)(t1 - t0) * period);
            prof::add(prof::GPU, dt, 0);
            TsEntry& e = g_ts_by_entry[p.entry];
            e.count++;
            e.ns += dt;
        }
        query_slot_release(p.qs);
        query_slot_release(p.qe);
    }
    g_ts_pending.clear();
    if (g_ts_untimed && !g_ts_warned) {
        g_ts_warned = true;
        std::fprintf(stderr,
                     "[spirula-profile] note: %llu dispatch(es) untimed "
                     "(timestamp pool exhausted)\n",
                     (unsigned long long)g_ts_untimed);
    }
}

void gpu_ts_report_by_entry() {
    if (!prof::enabled()) return;
    std::lock_guard<std::mutex> lock(g_ts_mutex);
    if (g_ts_by_entry.empty()) return;
    // Sort entries by total GPU time, descending.
    std::vector<std::pair<std::string, TsEntry>> rows(g_ts_by_entry.begin(),
                                                      g_ts_by_entry.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.second.ns > b.second.ns;
    });
    std::fprintf(stderr,
                 "\n[spirula-profile] ---- GPU time by kernel (entry) ----\n");
    std::fprintf(stderr, "%-44s %7s %10s %9s\n", "entry", "count", "gpu_ms",
                 "us/call");
    for (const auto& r : rows) {
        double ms = r.second.ns * 1e-6;
        double us = r.second.count ? (r.second.ns * 1e-3) / r.second.count : 0;
        std::fprintf(stderr, "%-44s %7llu %10.3f %9.2f\n", r.first.c_str(),
                     (unsigned long long)r.second.count, ms, us);
    }
    std::fprintf(stderr, "[spirula-profile] -----------------------------------\n");
}

void pipelines_shutdown();  // VulkanPipelines.cpp

void runtime_shutdown() {
    Context& ctx = Context::get();  // called from within ~Context's body;
    VkDevice dev = ctx.device();    // the device is still alive here

    gpu_ts_report_by_entry();  // per-kernel GPU breakdown (SS_PROFILE)
    pipelines_shutdown();
    {
        std::lock_guard<std::mutex> lock(g_stream_mutex);
        for (StreamImpl* s : g_streams) {
            vkDestroyCommandPool(dev, s->pool, nullptr);
            delete s;
        }
        g_streams.clear();
        g_default_stream = nullptr;
    }
    if (g_staging.buffer != VK_NULL_HANDLE) {
        destroy_allocation(g_staging);
        g_staging = Allocation{};
        g_staging_reservations.clear();
    }
    if (g_params_ring.buffer != VK_NULL_HANDLE) {
        destroy_allocation(g_params_ring);
        g_params_ring = Allocation{};
        g_params_cursor = 0;
    }
    {
        std::lock_guard<std::mutex> lock(g_alloc_mutex);
        for (auto& kv : g_device_allocs) destroy_allocation(kv.second);
        for (auto& kv : g_host_allocs) destroy_allocation(kv.second);
        g_device_allocs.clear();
        g_host_allocs.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_query_mutex);
        if (g_query_pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(dev, g_query_pool, nullptr);
            g_query_pool = VK_NULL_HANDLE;
            g_query_free.clear();
        }
    }
}

}  // namespace vk

}  // namespace backend
