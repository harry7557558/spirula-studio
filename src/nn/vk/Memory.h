#pragma once
// Vulkan allocation, persistent pools and transient arenas; nn/README.md.

#include "nn/vk/Context.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nn {
namespace vk {

using DevicePtr = uint64_t;  // VkDeviceAddress; 0 == null

// Installed by Stream at startup. Allocator::free MUST run this before it
// destroys a VkBuffer: vkDeviceWaitIdle alone is not enough, because the open
// command buffer holds recorded-but-unsubmitted commands that reference the
// allocation, and destroying it under them costs a VK_ERROR_DEVICE_LOST on the
// next submit -- with no hint as to which allocation was at fault.
void set_drain_hook(void (*fn)());

// ================
// Raw allocator
// ================

struct Allocation {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    DevicePtr      addr = 0;
    VkDeviceSize   size = 0;      // rounded-up capacity
    void*          mapped = nullptr;  // non-null for host-visible allocations
};

// What a buffer is for, for the VRAM report. Kept coarse on purpose: finer
// granularity belongs in the per-slot names.
enum class VramCategory : uint8_t {
    Weights = 0,
    Features,     // backbone / neck outputs held across prompts
    Memory,       // video memory bank (spatial features + object pointers)
    Workspace,    // arenas
    Staging,
    Other,
};
const char* vram_category_name(VramCategory c);

class Allocator {
public:
    static Allocator& get();

    // Device-local storage buffer, usable as a shader storage buffer and as a
    // transfer source/destination, with a device address.
    // Returns 0 only for a zero-byte request; a real failure throws with the
    // size, the device name and current usage in the message.
    DevicePtr allocDevice(VkDeviceSize bytes, const char* what);
    // The same, also written by the transfer queue (vk/Transfer.h): sharing
    // is concurrent across the two families, so no ownership transfer is due.
    DevicePtr allocDeviceShared(VkDeviceSize bytes, const char* what);

    // Host-visible, coherent, persistently mapped. Used for staging and for
    // the kernel-params ring. `mapped_out` receives the host pointer.
    DevicePtr allocHost(VkDeviceSize bytes, void** mapped_out, const char* what,
                        bool prefer_cached = false);

    void free(DevicePtr ptr);
    void freeAll();

    // Resolve an address (possibly interior) to its allocation and byte offset.
    // False when the address is not one of ours.
    bool resolve(DevicePtr ptr, Allocation* alloc, VkDeviceSize* offset) const;
    bool isDevicePointer(DevicePtr ptr) const;

    VkDeviceSize totalBytes() const;
    VkDeviceSize peakBytes() const;
    size_t allocationCount() const;

    // A never-null stand-in for optional pointer params. CPU Vulkan
    // implementations (llvmpipe) speculate loads across branches, so a null
    // device address behind an untaken `if` still faults; launchers substitute
    // this 4 KiB zeroed block instead.
    DevicePtr nullFallback();

private:
    Allocator() = default;
    ~Allocator();

    DevicePtr allocRaw(VkDeviceSize bytes, VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
                       bool map, void** mapped_out, const char* what, bool shared = false);

    mutable std::mutex mu_;
    std::unordered_map<DevicePtr, Allocation> allocs_;  // keyed by base address
    // Sorted base addresses for interior-pointer resolution.
    std::vector<DevicePtr> bases_;
    VkDeviceSize total_ = 0, peak_ = 0;
    DevicePtr    null_fallback_ = 0;
};

// Convenience wrapper for the pool/arena and for one-off buffers.
inline DevicePtr device_alloc(VkDeviceSize bytes, const char* what) {
    return Allocator::get().allocDevice(bytes, what);
}
inline void device_free(DevicePtr p) { Allocator::get().free(p); }
DevicePtr null_fallback();
inline DevicePtr or_fallback(DevicePtr p) { return p ? p : null_fallback(); }

// ================
// Persistent pool
// ================

// Enum-rooted keys keep the VRAM report readable and make a slot's lifetime
// obvious at the call site. `sub` distinguishes repeated instances of the same
// logical buffer (FPN level 2, memory-bank slot 3 of instance 7, ...).
enum class PoolSlot : uint16_t {
    Weights = 0,        // one blob per weight tensor (sub = tensor index)
    RopeTable,
    PosEncTable,
    PromptEncCache,
    BackboneFeat,       // ViT output
    NeckDet,            // detector FPN levels    (sub = level)
    NeckTrk,            // tracker FPN levels     (sub = level)
    NeckPe,             // sinusoidal PE per level(sub = level)
    TextFeat,
    PromptFeat,
    FusionFeat,
    MemBankFeat,        // sub = instance*kMaxMemSlots + slot
    MemBankPe,
    ObjPtrBank,
    MaskLogits,
    ParamsRing,
    Staging,
    Count
};

const char*  pool_slot_name(PoolSlot s);
VramCategory pool_slot_category(PoolSlot s);

class VramPool {
public:
    static VramPool& get();

    // At least `bytes` for (slot, sub); reallocates only when capacity is
    // exceeded. The returned address is stable until the next growing acquire
    // of the same key.
    DevicePtr acquire(PoolSlot slot, uint32_t sub, VkDeviceSize bytes);
    DevicePtr acquire(PoolSlot slot, VkDeviceSize bytes) { return acquire(slot, 0, bytes); }

    // Live address for (slot, sub), or 0.
    DevicePtr lookup(PoolSlot slot, uint32_t sub) const;

    void release(PoolSlot slot, uint32_t sub);
    void releaseSlot(PoolSlot slot);   // every sub of one slot
    void releaseAll();

    struct Entry {
        std::string  name;
        VramCategory category;
        VkDeviceSize used;
        VkDeviceSize capacity;
    };
    std::vector<Entry> breakdown() const;
    VkDeviceSize       totalCapacity() const;

private:
    VramPool() = default;
    struct Slot {
        DevicePtr    ptr = 0;
        VkDeviceSize cap = 0;
        VkDeviceSize used = 0;
    };
    static uint64_t key(PoolSlot s, uint32_t sub) {
        return ((uint64_t)s << 32) | sub;
    }
    mutable std::mutex mu_;
    std::unordered_map<uint64_t, Slot> slots_;
};

// ================
// Transient arena
// ================

// Bump allocator over one grow-only device buffer. Not thread-safe by design:
// a forward pass is single-threaded, and sharing an arena across threads would
// make the rewind points meaningless.
class Arena {
public:
    explicit Arena(const char* name = "arena");
    ~Arena();
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    using Mark = VkDeviceSize;

    // 256-byte aligned (covers every std430 vector alignment plus a healthy
    // margin for coalesced loads). Grows the backing buffer on overflow, which
    // invalidates every previously handed out pointer -- so growth is only
    // legal at a rewind point. `reserve()` up front avoids it entirely.
    DevicePtr alloc(VkDeviceSize bytes);

    Mark mark() const { return head_; }
    void rewind(Mark m) { head_ = m; }
    void reset() { head_ = 0; }
    void reserve(VkDeviceSize bytes);

    // Hands the backing buffer back to the allocator. The next alloc() builds
    // a new one, so this is legal exactly where growth is -- at a rewind point,
    // with no live pointers. reset() only moves the head; this is what actually
    // returns the memory.
    void release();

    VkDeviceSize capacity()  const { return cap_; }
    VkDeviceSize highWater() const { return high_water_; }

private:
    void grow(VkDeviceSize need);

    std::string  name_;
    DevicePtr    base_ = 0;
    VkDeviceSize cap_ = 0;
    VkDeviceSize head_ = 0;
    VkDeviceSize high_water_ = 0;
};

// RAII rewind. Every module forward pass opens one.
class ArenaScope {
public:
    explicit ArenaScope(Arena& a) : arena_(a), mark_(a.mark()) {}
    ~ArenaScope() { arena_.rewind(mark_); }
    ArenaScope(const ArenaScope&) = delete;
    ArenaScope& operator=(const ArenaScope&) = delete;

private:
    Arena&      arena_;
    Arena::Mark mark_;
};

}  // namespace vk
}  // namespace nn
