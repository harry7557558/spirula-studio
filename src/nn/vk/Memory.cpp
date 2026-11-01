#include "nn/vk/Memory.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace nn {
namespace vk {

// ================
// Naming tables
// ================

const char* vram_category_name(VramCategory c) {
    switch (c) {
        case VramCategory::Weights:   return "weights";
        case VramCategory::Features:  return "features";
        case VramCategory::Memory:    return "memory-bank";
        case VramCategory::Workspace: return "workspace";
        case VramCategory::Staging:   return "staging";
        default:                      return "other";
    }
}

const char* pool_slot_name(PoolSlot s) {
    switch (s) {
        case PoolSlot::Weights:        return "weights";
        case PoolSlot::RopeTable:      return "rope";
        case PoolSlot::PosEncTable:    return "posenc";
        case PoolSlot::PromptEncCache: return "prompt_enc_cache";
        case PoolSlot::BackboneFeat:   return "backbone";
        case PoolSlot::NeckDet:        return "neck.det";
        case PoolSlot::NeckTrk:        return "neck.trk";
        case PoolSlot::NeckPe:         return "neck.pe";
        case PoolSlot::TextFeat:       return "text";
        case PoolSlot::PromptFeat:     return "prompt";
        case PoolSlot::FusionFeat:     return "fusion";
        case PoolSlot::MemBankFeat:    return "membank.feat";
        case PoolSlot::MemBankPe:      return "membank.pe";
        case PoolSlot::ObjPtrBank:     return "membank.ptr";
        case PoolSlot::MaskLogits:     return "masks";
        case PoolSlot::ParamsRing:     return "params_ring";
        case PoolSlot::Staging:        return "staging";
        default:                       return "?";
    }
}

VramCategory pool_slot_category(PoolSlot s) {
    switch (s) {
        case PoolSlot::Weights:
            return VramCategory::Weights;
        case PoolSlot::RopeTable:
        case PoolSlot::PosEncTable:
        case PoolSlot::PromptEncCache:
        case PoolSlot::BackboneFeat:
        case PoolSlot::NeckDet:
        case PoolSlot::NeckTrk:
        case PoolSlot::NeckPe:
        case PoolSlot::TextFeat:
        case PoolSlot::PromptFeat:
        case PoolSlot::FusionFeat:
        case PoolSlot::MaskLogits:
            return VramCategory::Features;
        case PoolSlot::MemBankFeat:
        case PoolSlot::MemBankPe:
        case PoolSlot::ObjPtrBank:
            return VramCategory::Memory;
        case PoolSlot::ParamsRing:
        case PoolSlot::Staging:
            return VramCategory::Staging;
        default:
            return VramCategory::Other;
    }
}

// ================
// Allocator
// ================

namespace {
void (*g_drain_hook)() = nullptr;
}  // namespace

void set_drain_hook(void (*fn)()) { g_drain_hook = fn; }

// Submit-and-wait everything before a buffer disappears. See set_drain_hook.
static void drain_before_free() {
    if (g_drain_hook) g_drain_hook();
}

Allocator& Allocator::get() {
    static Allocator inst;
    return inst;
}

Allocator::~Allocator() { freeAll(); }

namespace {

// Human-readable size for the OOM message.
std::string fmtBytes(uint64_t b) {
    char buf[32];
    double mib = (double)b / (1024.0 * 1024.0);
    if (mib >= 1024.0) std::snprintf(buf, sizeof buf, "%.2f GiB", mib / 1024.0);
    else               std::snprintf(buf, sizeof buf, "%.1f MiB", mib);
    return buf;
}

// Windows backs every WDDM allocation with system commit, so a device-local
// allocation can fail with gigabytes of VRAM free purely because the pagefile
// is small and the commit charge is exhausted. The symptom -- OUT_OF_DEVICE_
// MEMORY while the driver still reports a multi-GiB heap budget -- is baffling
// without this hint, so it is checked and reported at the point of failure.
const char* oom_platform_hint() {
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        static char buf[256];
        const double free_gib = (double)ms.ullAvailPageFile / (1024.0 * 1024.0 * 1024.0);
        if (free_gib < 4.0) {
            std::snprintf(buf, sizeof buf,
                          " Windows has only %.1f GiB of commit charge left, and every GPU "
                          "allocation reserves commit -- enlarging the pagefile or closing "
                          "other applications will raise the effective VRAM limit.",
                          free_gib);
            return buf;
        }
    }
#endif
    return "";
}

}  // namespace

DevicePtr Allocator::allocRaw(VkDeviceSize bytes, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags required,
                              VkMemoryPropertyFlags preferred, bool map,
                              void** mapped_out, const char* what, bool shared) {
    if (bytes == 0) return 0;
    // 16-byte rounding: shaders read uint8 buffers as whole u32 words and
    // vector loads may overrun the logical end; the slack keeps them in bounds.
    bytes = (bytes + 15) & ~(VkDeviceSize)15;

    Context& ctx = Context::get();
    Allocation a;
    a.size = bytes;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = bytes;
    bci.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    const uint32_t families[2] = {ctx.queueFamily(), ctx.transferFamily()};
    if (shared && ctx.transferFamily() != UINT32_MAX) {
        bci.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bci.queueFamilyIndexCount = 2;
        bci.pQueueFamilyIndices = families;
    }
    NN_VK_CHECK(vkCreateBuffer(ctx.device(), &bci, nullptr, &a.buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx.device(), a.buffer, &req);

    const auto& mp = ctx.memoryProps();
    uint32_t type_index = UINT32_MAX;
    for (int pass = preferred ? 0 : 1; pass < 2 && type_index == UINT32_MAX; ++pass) {
        VkMemoryPropertyFlags want = required | (pass == 0 ? preferred : 0);
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((req.memoryTypeBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags & want) == want) {
                type_index = i;
                break;
            }
    }
    if (type_index == UINT32_MAX) {
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        fail("no memory type for %s (%s, flags 0x%x)", what, fmtBytes(bytes).c_str(),
             (unsigned)required);
    }

    VkMemoryAllocateFlagsInfo fi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    fi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.pNext = &fi;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type_index;

    VkResult r = vkAllocateMemory(ctx.device(), &mai, nullptr, &a.memory);
    if (r != VK_SUCCESS) {
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        NN_LOG_DEBUG("[vk] vkAllocateMemory(%llu bytes, type %u, heap %u) -> %s\n",
                       (unsigned long long)req.size, type_index,
                       mp.memoryTypes[type_index].heapIndex, Context::resultName(r));
        fail("out of GPU memory: could not allocate %s for %s on %s "
             "(this process already holds %s).%s Try a smaller --img-size, a "
             "quantized model, fewer tracked instances, or a GPU with more memory.",
             fmtBytes(bytes).c_str(), what, ctx.info().name.c_str(),
             fmtBytes(total_).c_str(), oom_platform_hint());
    }
    NN_VK_CHECK(vkBindBufferMemory(ctx.device(), a.buffer, a.memory, 0));

    if (map) NN_VK_CHECK(vkMapMemory(ctx.device(), a.memory, 0, bytes, 0, &a.mapped));

    VkBufferDeviceAddressInfo bda{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bda.buffer = a.buffer;
    a.addr = vkGetBufferDeviceAddress(ctx.device(), &bda);
    NN_CHECK(a.addr != 0, "vkGetBufferDeviceAddress returned 0 for %s", what);

    if (mapped_out) *mapped_out = a.mapped;

    {
        std::lock_guard<std::mutex> lock(mu_);
        allocs_[a.addr] = a;
        bases_.insert(std::lower_bound(bases_.begin(), bases_.end(), a.addr), a.addr);
        total_ += bytes;
        peak_ = std::max(peak_,total_);
    }
    NN_LOG_DEBUG("[vk] alloc %s for %s\n", fmtBytes(bytes).c_str(), what);
    return a.addr;
}

DevicePtr Allocator::allocDevice(VkDeviceSize bytes, const char* what) {
    return allocRaw(bytes,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, false, nullptr, what);
}

DevicePtr Allocator::allocDeviceShared(VkDeviceSize bytes, const char* what) {
    return allocRaw(bytes,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, false, nullptr, what, true);
}

DevicePtr Allocator::allocHost(VkDeviceSize bytes, void** mapped_out, const char* what,
                               bool prefer_cached) {
    // Downloads want HOST_CACHED: the first HOST_VISIBLE|COHERENT type on a
    // discrete GPU is write-combined, which reads at a fraction of RAM speed.
    return allocRaw(bytes,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    prefer_cached ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : 0, true,
                    mapped_out, what);
}

void Allocator::free(DevicePtr ptr) {
    if (!ptr) return;
    Allocation a;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = allocs_.find(ptr);
        if (it == allocs_.end()) return;
        a = it->second;
        allocs_.erase(it);
        auto bit = std::lower_bound(bases_.begin(), bases_.end(), ptr);
        if (bit != bases_.end() && *bit == ptr) bases_.erase(bit);
        total_ -= a.size;
    }
    // Contract (matches cudaFree): all work referencing the allocation has
    // completed before the memory is reclaimed. The drain hook is what makes
    // that true for commands that are recorded but not yet submitted --
    // vkDeviceWaitIdle would happily return with those still pending.
    drain_before_free();
    Context& ctx = Context::get();
    vkDeviceWaitIdle(ctx.device());
    if (a.mapped) vkUnmapMemory(ctx.device(), a.memory);
    vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
    vkFreeMemory(ctx.device(), a.memory, nullptr);
}

void Allocator::freeAll() {
    if (!Context::initialized()) return;
    std::vector<Allocation> doomed;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& kv : allocs_) doomed.push_back(kv.second);
        allocs_.clear();
        bases_.clear();
        total_ = 0;
        null_fallback_ = 0;
    }
    if (doomed.empty()) return;
    drain_before_free();
    Context& ctx = Context::get();
    vkDeviceWaitIdle(ctx.device());
    for (auto& a : doomed) {
        if (a.mapped) vkUnmapMemory(ctx.device(), a.memory);
        vkDestroyBuffer(ctx.device(), a.buffer, nullptr);
        vkFreeMemory(ctx.device(), a.memory, nullptr);
    }
}

bool Allocator::resolve(DevicePtr ptr, Allocation* alloc, VkDeviceSize* offset) const {
    if (!ptr) return false;
    std::lock_guard<std::mutex> lock(mu_);
    // Greatest base <= ptr.
    auto it = std::upper_bound(bases_.begin(), bases_.end(), ptr);
    if (it == bases_.begin()) return false;
    --it;
    auto ait = allocs_.find(*it);
    if (ait == allocs_.end()) return false;
    const Allocation& a = ait->second;
    if (ptr >= a.addr + a.size) return false;
    if (alloc) *alloc = a;
    if (offset) *offset = ptr - a.addr;
    return true;
}

bool Allocator::isDevicePointer(DevicePtr ptr) const {
    return resolve(ptr, nullptr, nullptr);
}

VkDeviceSize Allocator::totalBytes() const {
    std::lock_guard<std::mutex> lock(mu_);
    return total_;
}

VkDeviceSize Allocator::peakBytes() const {
    std::lock_guard<std::mutex> lock(mu_);
    return peak_;
}

size_t Allocator::allocationCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    return allocs_.size();
}

DevicePtr Allocator::nullFallback() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (null_fallback_) return null_fallback_;
    }
    // 4 KiB covers every constant-offset load a JIT could hoist past a guard.
    DevicePtr p = allocDevice(4096, "null fallback");
    std::lock_guard<std::mutex> lock(mu_);
    null_fallback_ = p;
    return p;
}

DevicePtr null_fallback() { return Allocator::get().nullFallback(); }

// ================
// VramPool
// ================

VramPool& VramPool::get() {
    static VramPool inst;
    return inst;
}

DevicePtr VramPool::acquire(PoolSlot slot, uint32_t sub, VkDeviceSize bytes) {
    std::lock_guard<std::mutex> lock(mu_);
    Slot& s = slots_[key(slot, sub)];
    if (bytes > s.cap) {
        if (s.ptr) Allocator::get().free(s.ptr);
        // Reset before the (throwing) allocation so an OOM leaves the slot
        // empty rather than {null ptr, stale capacity}.
        s.ptr = 0;
        s.cap = 0;
        char what[96];
        std::snprintf(what, sizeof what, "%s[%u]", pool_slot_name(slot), sub);
        s.ptr = Allocator::get().allocDevice(bytes, what);
        s.cap = bytes;
    }
    s.used = bytes;
    return s.ptr;
}

DevicePtr VramPool::lookup(PoolSlot slot, uint32_t sub) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = slots_.find(key(slot, sub));
    return it == slots_.end() ? 0 : it->second.ptr;
}

void VramPool::release(PoolSlot slot, uint32_t sub) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = slots_.find(key(slot, sub));
    if (it == slots_.end()) return;
    if (it->second.ptr) Allocator::get().free(it->second.ptr);
    slots_.erase(it);
}

void VramPool::releaseSlot(PoolSlot slot) {
    std::vector<DevicePtr> doomed;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto it = slots_.begin(); it != slots_.end();) {
            if ((PoolSlot)(it->first >> 32) == slot) {
                if (it->second.ptr) doomed.push_back(it->second.ptr);
                it = slots_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (DevicePtr p : doomed) Allocator::get().free(p);
}

void VramPool::releaseAll() {
    std::vector<DevicePtr> doomed;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& kv : slots_)
            if (kv.second.ptr) doomed.push_back(kv.second.ptr);
        slots_.clear();
    }
    for (DevicePtr p : doomed) Allocator::get().free(p);
}

std::vector<VramPool::Entry> VramPool::breakdown() const {
    std::lock_guard<std::mutex> lock(mu_);
    // Aggregate the per-sub allocations of one slot: 1465 weight tensors as
    // 1465 rows helps nobody.
    std::unordered_map<uint32_t, Entry> agg;
    for (auto& kv : slots_) {
        PoolSlot slot = (PoolSlot)(kv.first >> 32);
        Entry& e = agg[(uint32_t)slot];
        if (e.name.empty()) {
            e.name = pool_slot_name(slot);
            e.category = pool_slot_category(slot);
        }
        e.used += kv.second.used;
        e.capacity += kv.second.cap;
    }
    std::vector<Entry> out;
    out.reserve(agg.size());
    for (auto& kv : agg) out.push_back(kv.second);
    std::sort(out.begin(), out.end(),
              [](const Entry& a, const Entry& b) { return a.capacity > b.capacity; });
    return out;
}

VkDeviceSize VramPool::totalCapacity() const {
    std::lock_guard<std::mutex> lock(mu_);
    VkDeviceSize t = 0;
    for (auto& kv : slots_) t += kv.second.cap;
    return t;
}

// ================
// Arena
// ================

Arena::Arena(const char* name) : name_(name) {}

Arena::~Arena() {
    if (base_) Allocator::get().free(base_);
}

void Arena::grow(VkDeviceSize need) {
    // Growing invalidates every outstanding pointer, so this is only legal at a
    // rewind point. Round up generously (x1.5, 4 MiB granularity) so a forward
    // pass reaches steady state in two or three frames and never grows again.
    VkDeviceSize want = std::max<VkDeviceSize>(need, cap_ + cap_ / 2);
    want = (want + (4u << 20) - 1) & ~(VkDeviceSize)((4u << 20) - 1);
    NN_LOG_DEBUG("[vk] arena '%s' grows %.1f -> %.1f MiB\n", name_.c_str(),
                   cap_ / 1048576.0, want / 1048576.0);
    if (base_) Allocator::get().free(base_);
    base_ = Allocator::get().allocDevice(want, name_.c_str());
    cap_ = want;
}

void Arena::reserve(VkDeviceSize bytes) {
    NN_CHECK(head_ == 0, "Arena::reserve must be called at an empty arena");
    if (bytes > cap_) grow(bytes);
}

void Arena::release() {
    NN_CHECK(head_ == 0, "Arena::release must be called at an empty arena");
    if (base_) Allocator::get().free(base_);
    base_ = 0;
    cap_ = 0;
    // high_water_ deliberately survives: it is a measurement of this arena's
    // peak, and a VRAM report after a release should still show what it took.
}

DevicePtr Arena::alloc(VkDeviceSize bytes) {
    if (bytes == 0) return null_fallback();
    constexpr VkDeviceSize kAlign = 256;
    VkDeviceSize off = (head_ + kAlign - 1) & ~(kAlign - 1);
    if (off + bytes > cap_) {
        NN_CHECK(head_ == 0,
                   "arena '%s' overflow: needs %llu bytes at offset %llu but has "
                   "%llu. Growing mid-scope would invalidate live pointers; "
                   "reserve() more up front.",
                   name_.c_str(), (unsigned long long)bytes,
                   (unsigned long long)off, (unsigned long long)cap_);
        grow(off + bytes);
        off = 0;
    }
    head_ = off + bytes;
    high_water_ = std::max(high_water_, head_);
    return base_ + off;
}

}  // namespace vk
}  // namespace nn
