#include "nn/vk/Stream.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include "core/Env.h"
#include "core/GpuStall.h"
#include "core/SubmitBudget.h"

namespace nn {
namespace vk {

bool debug_sync_enabled() {
    static const bool v = [] {
        const char* e = spirula::env("NN_DEBUG_SYNC");
        return e && e[0] && std::strcmp(e, "0") != 0;
    }();
    return v;
}

namespace {
constexpr VkDeviceSize kStagingBytes = 32ull << 20;   // upload/download chunk
constexpr VkDeviceSize kParamsRingBytes = 1ull << 20;  // oversized param structs
constexpr uint32_t     kMaxQueries = 8192;
}  // namespace

struct Stream::Impl {
    VkCommandPool  pool = VK_NULL_HANDLE;
    static constexpr int kRing = 4;
    VkCommandBuffer cbs[kRing] = {};
    uint64_t        cb_value[kRing] = {};  // timeline value each ring slot waits on
    int             cur = 0;
    bool            recording = false;
    uint32_t        recorded = 0;  // dispatches in the command buffer being built
    double          work = 0;      // their summed cost

    // Two timestamps per ring slot bracket each submission; read back once the
    // slot retires, they are what the budget learns this device's rate from.
    VkQueryPool     submit_queries = VK_NULL_HANDLE;
    double          slot_work[kRing] = {};  // cost of an unharvested submission
    // Assumed until measured: a 2-CU iGPU's attention rate, so the first
    // submits of a process are safe everywhere and a fast GPU outgrows it at once.
    static constexpr double kPriorFlops = 50e9;
    spirula::SubmitBudget budget{kPriorFlops};

    VkSemaphore timeline = VK_NULL_HANDLE;
    uint64_t    submitted = 0;

    // Cross-queue waits attached to the next submission (see Stream::waitOn).
    std::vector<VkSemaphore> wait_sems;
    std::vector<uint64_t>    wait_values;

    // Staging: upload is write-combined, download asks for HOST_CACHED.
    DevicePtr up_addr = 0;   void* up_map = nullptr;   VkDeviceSize up_head = 0;
    DevicePtr dl_addr = 0;   void* dl_map = nullptr;

    DevicePtr    ring_addr = 0;
    void*        ring_map = nullptr;
    VkDeviceSize ring_head = 0;

    VkQueryPool query_pool = VK_NULL_HANDLE;
    uint32_t    query_next = 0;
    std::vector<std::string> query_names;
    std::map<std::string, std::pair<uint64_t, double>> prof;  // name -> (n, ms)

    bool initialized = false;

    void init();
    void resolveQueries();
    void harvest(int slot);
};

Stream& Stream::get() {
    static Stream inst;
    return inst;
}

Stream::~Stream() { shutdown(); }

Stream::Impl& Stream::impl() {
    if (!impl_) {
        impl_ = new Impl();
        impl_->init();
    }
    return *impl_;
}

void Stream::Impl::init() {
    if (initialized) return;
    initialized = true;
    Context& ctx = Context::get();

    // Allocator::free calls this before destroying a buffer.
    set_drain_hook([] {
        if (Stream::get().impl_) Stream::get().sync();
    });

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = ctx.queueFamily();
    NN_VK_CHECK(vkCreateCommandPool(ctx.device(), &cpi, nullptr, &pool));

    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = kRing;
    NN_VK_CHECK(vkAllocateCommandBuffers(ctx.device(), &ai, cbs));

    VkSemaphoreTypeCreateInfo sti{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    sti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    sti.initialValue = 0;
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    sci.pNext = &sti;
    NN_VK_CHECK(vkCreateSemaphore(ctx.device(), &sci, nullptr, &timeline));

    up_addr = Allocator::get().allocHost(kStagingBytes, &up_map, "staging.upload");
    dl_addr = Allocator::get().allocHost(kStagingBytes, &dl_map, "staging.download",
                                         /*prefer_cached=*/true);
    ring_addr = Allocator::get().allocHost(kParamsRingBytes, &ring_map, "params.ring");

    uint32_t nqf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical(), &nqf, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(nqf);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical(), &nqf, qfp.data());
    if (ctx.timestampPeriod() > 0.0f && ctx.queueFamily() < nqf &&
        qfp[ctx.queueFamily()].timestampValidBits > 0) {
        VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpi.queryCount = 2 * kRing;
        NN_VK_CHECK(vkCreateQueryPool(ctx.device(), &qpi, nullptr, &submit_queries));
    }

    if (ctx.profiling() && ctx.timestampPeriod() > 0.0f) {
        VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpi.queryCount = kMaxQueries;
        NN_VK_CHECK(vkCreateQueryPool(ctx.device(), &qpi, nullptr, &query_pool));
    }
}

void Stream::shutdown() {
    Stream& s = get();
    if (!s.impl_) return;
    if (Context::initialized()) {
        s.sync();
        // Release the drain hook and the buffers BEFORE the objects it needs:
        // Allocator::free would otherwise call back into a Stream whose
        // semaphore and command pool are already gone.
        set_drain_hook(nullptr);
        Allocator::get().free(s.impl_->up_addr);
        Allocator::get().free(s.impl_->dl_addr);
        Allocator::get().free(s.impl_->ring_addr);
        Context& ctx = Context::get();
        if (s.impl_->query_pool) vkDestroyQueryPool(ctx.device(), s.impl_->query_pool, nullptr);
        if (s.impl_->submit_queries)
            vkDestroyQueryPool(ctx.device(), s.impl_->submit_queries, nullptr);
        if (s.impl_->timeline) vkDestroySemaphore(ctx.device(), s.impl_->timeline, nullptr);
        if (s.impl_->pool) vkDestroyCommandPool(ctx.device(), s.impl_->pool, nullptr);
    }
    delete s.impl_;
    s.impl_ = nullptr;
}

// ================
// Recording
// ================

namespace {

void wait_timeline(const Context& ctx, VkSemaphore timeline, const uint64_t* value) {
    VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wi.semaphoreCount = 1;
    wi.pSemaphores = &timeline;
    wi.pValues = value;
    spirula::GpuStallWatch watch(std::strcmp(ctx.info().type, "cpu") == 0);
    for (;;) {
        const VkResult r = vkWaitSemaphores(ctx.device(), &wi, spirula::GpuStallWatch::kSliceNs);
        if (r == VK_SUCCESS) return;
        if (r != VK_TIMEOUT) NN_VK_CHECK(r);
        uint64_t current = 0;
        NN_VK_CHECK(vkGetSemaphoreCounterValue(ctx.device(), timeline, &current));
        if (watch.stalled(current))
            ::nn::fail("the GPU stopped responding: no work finished for a minute (the driver may have "
                       "reset it, often after running out of GPU memory; SS_GPU_STALL_SECONDS sets the wait)");
    }
}

}  // namespace

VkCommandBuffer Stream::begin() {
    Impl& s = impl();
    if (s.recording) return s.cbs[s.cur];

    Context& ctx = Context::get();
    // Wait until this ring slot's previous submission has retired.
    if (s.cb_value[s.cur] > 0) {
        uint64_t v = 0;
        vkGetSemaphoreCounterValue(ctx.device(), s.timeline, &v);
        if (v < s.cb_value[s.cur]) {
            wait_timeline(ctx, s.timeline, &s.cb_value[s.cur]);
        }
        s.harvest(s.cur);
    }
    NN_VK_CHECK(vkResetCommandBuffer(s.cbs[s.cur], 0));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    NN_VK_CHECK(vkBeginCommandBuffer(s.cbs[s.cur], &bi));
    s.recording = true;
    if (s.submit_queries) {
        vkCmdResetQueryPool(s.cbs[s.cur], s.submit_queries, 2 * s.cur, 2);
        vkCmdWriteTimestamp(s.cbs[s.cur], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            s.submit_queries, 2 * s.cur);
    }
    if (s.query_pool) {
        vkCmdResetQueryPool(s.cbs[s.cur], s.query_pool, 0, kMaxQueries);
        s.query_next = 0;
        s.query_names.clear();
    }
    return s.cbs[s.cur];
}

void Stream::barrier(VkCommandBuffer cb) {
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                       VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 1, &mb, 0, nullptr, 0, nullptr);
}

void Stream::flush() {
    Impl& s = impl();
    if (!s.recording) return;
    Context& ctx = Context::get();
    if (s.submit_queries)
        vkCmdWriteTimestamp(s.cbs[s.cur], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            s.submit_queries, 2 * s.cur + 1);
    NN_VK_CHECK(vkEndCommandBuffer(s.cbs[s.cur]));

    const uint64_t signal = ++s.submitted;
    std::vector<VkPipelineStageFlags> wait_stages(s.wait_sems.size(),
                                                  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkTimelineSemaphoreSubmitInfo tsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.waitSemaphoreValueCount = (uint32_t)s.wait_values.size();
    tsi.pWaitSemaphoreValues = s.wait_values.data();
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &signal;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &tsi;
    si.waitSemaphoreCount = (uint32_t)s.wait_sems.size();
    si.pWaitSemaphores = s.wait_sems.data();
    si.pWaitDstStageMask = wait_stages.data();
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s.cbs[s.cur];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &s.timeline;
    NN_VK_CHECK(vkQueueSubmit(ctx.queue(), 1, &si, VK_NULL_HANDLE));
    s.wait_sems.clear();
    s.wait_values.clear();

    s.cb_value[s.cur] = signal;
    s.slot_work[s.cur] = s.work;
    s.cur = (s.cur + 1) % Impl::kRing;
    s.recording = false;
    s.recorded = 0;
    s.work = 0;
}

// Only for a slot whose submission has retired.
void Stream::Impl::harvest(int slot) {
    if (!submit_queries || slot_work[slot] <= 0) return;
    uint64_t ts[2] = {};
    VkResult r = vkGetQueryPoolResults(Context::get().device(), submit_queries,
                                       2 * slot, 2, sizeof(ts), ts, sizeof(uint64_t),
                                       VK_QUERY_RESULT_64_BIT);
    if (r == VK_SUCCESS && ts[1] > ts[0])
        budget.record(slot_work[slot],
                      (double)(ts[1] - ts[0]) * Context::get().timestampPeriod() * 1e-9);
    slot_work[slot] = 0;
}

double Stream::workCap() { return impl().budget.limit(); }

void Stream::sync() {
    Impl& s = impl();
    flush();
    if (s.submitted == 0) return;
    Context& ctx = Context::get();
    uint64_t v = 0;
    vkGetSemaphoreCounterValue(ctx.device(), s.timeline, &v);
    if (v < s.submitted) {
        wait_timeline(ctx, s.timeline, &s.submitted);
    }
    for (int i = 0; i < Impl::kRing; ++i) s.harvest(i);
    s.resolveQueries();
}

void Stream::waitOn(VkSemaphore sem, uint64_t value) {
    if (!sem) return;
    Impl& s = impl();
    // A wait attaches to the next submission, so anything already recorded has
    // to go out first -- otherwise the wait would also gate work that was
    // supposed to precede it.
    flush();
    s.wait_sems.push_back(sem);
    s.wait_values.push_back(value);
}

VkSemaphore Stream::timeline() { return impl().timeline; }
uint64_t    Stream::lastSubmitted() { return impl().submitted; }

void Stream::Impl::resolveQueries() {
    if (!query_pool || query_names.empty()) return;
    Context& ctx = Context::get();
    std::vector<uint64_t> ts(query_names.size() * 2);
    VkResult r = vkGetQueryPoolResults(
        ctx.device(), query_pool, 0, (uint32_t)ts.size(), ts.size() * sizeof(uint64_t),
        ts.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    if (r == VK_SUCCESS) {
        const double period = ctx.timestampPeriod() * 1e-6;  // ns -> ms
        for (size_t i = 0; i < query_names.size(); ++i) {
            auto& e = prof[query_names[i]];
            e.first += 1;
            e.second += (double)(ts[2 * i + 1] - ts[2 * i]) * period;
        }
    }
    query_names.clear();
    query_next = 0;
}

// ================
// Dispatch
// ================

Stream::Fold Stream::fold1D(int64_t total, uint32_t block) {
    uint32_t wgs = (uint32_t)((total + block - 1) / block);
    uint32_t per_row = std::min(std::max(wgs, 1u), 65535u);
    return {per_row, (wgs + per_row - 1) / per_row};
}

void Stream::dispatch(const char* entry, const SpecList& spec, uint32_t gx, uint32_t gy,
                      uint32_t gz, const void* params, uint32_t params_size, double work) {
    if (gx == 0 || gy == 0 || gz == 0) return;
    Impl& s = impl();
    Context& ctx = Context::get();
    NN_CHECK(params_size <= ctx.maxPushConstantsSize(),
               "kernel %s: params struct is %u bytes but the device push limit is %u; "
               "use dispatchBig()", entry, params_size, ctx.maxPushConstantsSize());
    NN_CHECK(gx <= 65535 && gy <= 65535 && gz <= 65535,
               "kernel %s: grid (%u,%u,%u) exceeds the 65535 per-dimension cap; "
               "fold the range (Stream::fold1D)", entry, gx, gy, gz);

    if (debug_sync_enabled())
        NN_LOG_ERROR("[ssam-sync] %s (%u,%u,%u)...\n", entry, gx, gy, gz);

    VkPipeline pipe = Pipelines::get().acquire(entry, spec);
    const double cap = s.budget.limit();
    if (s.recording && s.recorded > 0 && s.work + work > cap) flush();
    VkCommandBuffer cb = begin();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    if (params_size)
        vkCmdPushConstants(cb, Pipelines::get().layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           params_size, params);

    const bool prof = s.query_pool && s.query_next + 2 <= kMaxQueries;
    if (prof)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s.query_pool,
                            s.query_next);
    vkCmdDispatch(cb, gx, gy, gz);
    if (prof) {
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s.query_pool,
                            s.query_next + 1);
        s.query_next += 2;
        s.query_names.emplace_back(entry);
    }
    barrier(cb);
    s.work += work;

    if (debug_sync_enabled()) {
        sync();
        NN_LOG_ERROR("[ssam-sync] %s: ok\n", entry);
        return;
    }

    // Cap how much goes into one command buffer. A SAM 3 image encode records
    // thousands of dispatches, and left unbounded that is a single batch on
    // Intel/ANV whose results come out wrong AND differ run to run -- the
    // per-dispatch barriers above stop being honoured somewhere inside it.
    // Splitting at a submission boundary fixes it exactly (verified against the
    // NVIDIA result, bit for bit), and there is no reason to want an unbounded
    // batch anyway: submitting sooner lets the GPU start while the host is
    // still recording, and the four-deep ring keeps the host from running away.
    if (++s.recorded >= max_dispatches_per_batch() || s.work >= cap) flush();
}

// Dispatches per command buffer. 64 is comfortably below where ANV starts
// misbehaving (thousands) and high enough that the per-submit cost is noise;
// SS_NN_BATCH_DISPATCHES overrides it, 0 = unbounded (the old behaviour).
uint32_t Stream::max_dispatches_per_batch() {
    static const uint32_t v = [] {
        const char* e = spirula::env("NN_BATCH_DISPATCHES");
        if (!e || !e[0]) return 64u;
        long n = std::strtol(e, nullptr, 10);
        return n > 0 ? (uint32_t)n : UINT32_MAX;
    }();
    return v;
}

void Stream::dispatchBig(const char* entry, const SpecList& spec, uint32_t gx,
                         uint32_t gy, uint32_t gz, const void* params,
                         uint32_t params_size, double work) {
    DevicePtr addr = 0;
    void* mapped = nullptr;
    paramsAlloc(params_size, &addr, &mapped);
    std::memcpy(mapped, params, params_size);
    dispatch(entry, spec, gx, gy, gz, &addr, sizeof(addr), work);
}

void Stream::dispatchFlat(const char* entry, const SpecList& spec, int64_t total,
                          uint32_t block, void* params, uint32_t params_size,
                          uint32_t* groups_per_row_field, double work) {
    if (total <= 0) return;
    Fold f = fold1D(total, block);
    if (groups_per_row_field) *groups_per_row_field = f.per_row;
    dispatch(entry, spec, f.per_row, f.rows, 1, params, params_size,
             work >= 0 ? work : (double)total * kElemWork);
}

void Stream::paramsAlloc(uint32_t bytes, DevicePtr* addr_out, void** mapped_out) {
    Impl& s = impl();
    const VkDeviceSize aligned = (bytes + 255) & ~(VkDeviceSize)255;
    NN_CHECK(aligned <= kParamsRingBytes, "params struct of %u bytes exceeds the ring",
               bytes);
    if (s.ring_head + aligned > kParamsRingBytes) {
        // Wrapping would overwrite params still referenced by in-flight work.
        sync();
        s.ring_head = 0;
    }
    *addr_out = s.ring_addr + s.ring_head;
    *mapped_out = (uint8_t*)s.ring_map + s.ring_head;
    s.ring_head += aligned;
}

// ================
// Memory ops
// ================

void Stream::fill(DevicePtr dst, uint32_t word, VkDeviceSize bytes) {
    if (!dst || bytes == 0) return;
    Allocation a;
    VkDeviceSize off = 0;
    NN_CHECK(Allocator::get().resolve(dst, &a, &off),
               "fill: 0x%llx is not a device address", (unsigned long long)dst);
    // vkCmdFillBuffer needs 4-byte offset and size; allocations are 16-byte
    // rounded, so rounding the size up stays inside the buffer.
    NN_CHECK((off & 3) == 0, "fill: unaligned destination offset");
    VkDeviceSize n = std::min<VkDeviceSize>((bytes + 3) & ~(VkDeviceSize)3, a.size - off);
    VkCommandBuffer cb = begin();
    vkCmdFillBuffer(cb, a.buffer, off, n, word);
    barrier(cb);
}

void Stream::copy(DevicePtr dst, DevicePtr src, VkDeviceSize bytes) {
    if (!dst || !src || bytes == 0) return;
    Allocation da, sa;
    VkDeviceSize doff = 0, soff = 0;
    NN_CHECK(Allocator::get().resolve(dst, &da, &doff), "copy: bad destination");
    NN_CHECK(Allocator::get().resolve(src, &sa, &soff), "copy: bad source");
    VkBufferCopy c{soff, doff, bytes};
    VkCommandBuffer cb = begin();
    vkCmdCopyBuffer(cb, sa.buffer, da.buffer, 1, &c);
    barrier(cb);
}

void Stream::upload(DevicePtr dst, const void* src, VkDeviceSize bytes) {
    if (!dst || bytes == 0) return;
    Impl& s = impl();
    Allocation da;
    VkDeviceSize doff = 0;
    NN_CHECK(Allocator::get().resolve(dst, &da, &doff), "upload: bad destination");
    NN_CHECK(doff + bytes <= da.size,
               "upload: %llu bytes at offset %llu overruns a %llu-byte allocation",
               (unsigned long long)bytes, (unsigned long long)doff,
               (unsigned long long)da.size);

    // The upload staging buffer is a ring: successive uploads take fresh space
    // and only a wrap forces a sync. Weight loading issues ~1500 small uploads;
    // syncing on each of them would dominate model load time.
    Allocation sa;
    VkDeviceSize sbase = 0;
    Allocator::get().resolve(s.up_addr, &sa, &sbase);

    const uint8_t* p = (const uint8_t*)src;
    for (VkDeviceSize off = 0; off < bytes; off += kStagingBytes) {
        VkDeviceSize chunk = std::min(kStagingBytes, bytes - off);
        if (s.up_head + chunk > kStagingBytes) {
            sync();  // every prior copy out of the ring has retired
            s.up_head = 0;
        }
        std::memcpy((uint8_t*)s.up_map + s.up_head, p + off, chunk);
        VkBufferCopy c{sbase + s.up_head, doff + off, chunk};
        VkCommandBuffer cb = begin();
        vkCmdCopyBuffer(cb, sa.buffer, da.buffer, 1, &c);
        barrier(cb);
        s.up_head += (chunk + 255) & ~(VkDeviceSize)255;
    }
}

void Stream::download(void* dst, DevicePtr src, VkDeviceSize bytes) {
    if (!src || bytes == 0) return;
    Impl& s = impl();
    Allocation sa;
    VkDeviceSize soff = 0;
    NN_CHECK(Allocator::get().resolve(src, &sa, &soff), "download: bad source");
    NN_CHECK(soff + bytes <= sa.size,
               "download: %llu bytes at offset %llu overruns a %llu-byte allocation",
               (unsigned long long)bytes, (unsigned long long)soff,
               (unsigned long long)sa.size);

    uint8_t* p = (uint8_t*)dst;
    Allocation da;
    VkDeviceSize doff = 0;
    Allocator::get().resolve(s.dl_addr, &da, &doff);
    for (VkDeviceSize off = 0; off < bytes; off += kStagingBytes) {
        VkDeviceSize chunk = std::min(kStagingBytes, bytes - off);
        VkBufferCopy c{soff + off, doff, chunk};
        VkCommandBuffer cb = begin();
        vkCmdCopyBuffer(cb, sa.buffer, da.buffer, 1, &c);
        barrier(cb);
        sync();
        std::memcpy(p + off, s.dl_map, chunk);
    }
}

// ================
// Profiling
// ================

void Stream::report() {
    Impl& s = impl();
    sync();
    if (s.prof.empty()) return;
    std::vector<std::pair<std::string, std::pair<uint64_t, double>>> v(s.prof.begin(),
                                                                      s.prof.end());
    std::sort(v.begin(), v.end(),
              [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
    double total = 0;
    for (auto& e : v) total += e.second.second;
    NN_LOG_INFO("[profile] %-34s %10s %8s %10s\n", "kernel", "total_ms", "count",
                  "avg_us");
    for (auto& e : v)
        NN_LOG_INFO("[profile] %-34s %10.2f %8llu %10.1f\n", e.first.c_str(),
                      e.second.second, (unsigned long long)e.second.first,
                      1e3 * e.second.second / (double)e.second.first);
    NN_LOG_INFO("[profile] %-34s %10.2f\n", "TOTAL", total);
}

}  // namespace vk
}  // namespace nn
