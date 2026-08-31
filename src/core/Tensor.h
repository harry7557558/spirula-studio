#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include "backend/api/BackendTypes.h"
#include "backend/api/BackendRuntime.h"
#include "core/Env.h"

#include <mutex>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "core/PoolSlots.h"


// Scalar dtype tags for .npy serialization of pool buffers. Vector types
// (floatN / intN / ucharN) are tagged by their SCALAR component; a buffer is
// serialized as a flat typed array of (bytes / scalar_size) elements, which
// round-trips exactly. Recorded per-Slot in acquire<T>() so the checkpoint
// writer stays generic (no per-buffer type knowledge).
enum class NpyScalar : uint8_t { u1 = 0, u2, u4, u8, i4, i8, f2, f4, f8, b1 };

template<typename T>
constexpr NpyScalar npy_scalar_tag() {
    if constexpr (std::is_same_v<T, float>  || std::is_same_v<T, float2> ||
                  std::is_same_v<T, float3> || std::is_same_v<T, float4>) return NpyScalar::f4;
    else if constexpr (std::is_same_v<T, double>)   return NpyScalar::f8;
    else if constexpr (std::is_same_v<T, bool>      || std::is_same_v<T, uint8_t> ||
                       std::is_same_v<T, uchar3>    || std::is_same_v<T, uchar4>) return NpyScalar::u1;
    else if constexpr (std::is_same_v<T, uint16_t>) return NpyScalar::u2;
    else if constexpr (std::is_same_v<T, uint32_t>) return NpyScalar::u4;
    else if constexpr (std::is_same_v<T, uint64_t>) return NpyScalar::u8;
    else if constexpr (std::is_same_v<T, int32_t>   || std::is_same_v<T, int2> ||
                       std::is_same_v<T, int3>      || std::is_same_v<T, int4>) return NpyScalar::i4;
    else if constexpr (std::is_same_v<T, int64_t>)  return NpyScalar::i8;
    else return NpyScalar::u1;  // fallback: serialize raw bytes as uint8
}

// (numpy descr string, element size) for a tag. Used by the checkpoint writer.
inline std::pair<const char*, size_t> npy_scalar_descr(NpyScalar t) {
    switch (t) {
        case NpyScalar::u1: return {"|u1", 1};
        case NpyScalar::u2: return {"<u2", 2};
        case NpyScalar::u4: return {"<u4", 4};
        case NpyScalar::u8: return {"<u8", 8};
        case NpyScalar::i4: return {"<i4", 4};
        case NpyScalar::i8: return {"<i8", 8};
        case NpyScalar::f2: return {"<f2", 2};
        case NpyScalar::f4: return {"<f4", 4};
        case NpyScalar::f8: return {"<f8", 8};
        case NpyScalar::b1: return {"|b1", 1};
        default:            return {"|u1", 1};
    }
}


// Use this to communicate with Python, without need to compile/link ATen/LibTorch
typedef std::tuple<
    uint64_t,  // data_ptr
    uint32_t,  // element_size
    std::vector<int64_t>  // shape
> TorchTensorView;


// AsyncReadout<T> -- pinned-host ring buffer for non-blocking D->H scalar readouts.
//
// Usage pattern (one iteration of training):
//     static AsyncReadout<float> R(N);
//     const float* prev = R.read_previous();   // sync on prior issue (cheap)
//     LossValues out = prev ? pack(prev) : LossValues{};
//     R.issue(d_src, stream);                  // async D->H of this iter
//     return out;
//
// `read_previous()` returns the host pointer to the most-recently-issued slot
// AFTER synchronizing on its completion event. Because there's a full
// iteration of GPU work between issue() and the next read_previous(), the
// event is essentially always already complete -- the event sync returns
// immediately, replacing what used to be a blocking device-to-host memcpy.
//
// First call: read_previous() returns nullptr.
template<typename T>
class AsyncReadout {
public:
    explicit AsyncReadout(int n_elems, int n_slots = 2)
        : _n_elems(n_elems), _n_slots(n_slots),
          _h(n_slots, nullptr), _evt(n_slots),
          _valid(n_slots, false), _next_write(0)
    {
        for (int i = 0; i < _n_slots; ++i) {
            _h[i] = (T*)backend::host_malloc_pinned((size_t)_n_elems * sizeof(T));
            _evt[i] = backend::event_create(false);
        }
    }

    ~AsyncReadout() {
        for (int i = 0; i < _n_slots; ++i) {
            if (_h[i]) backend::host_free_pinned(_h[i]);
            backend::event_destroy(_evt[i]);
        }
    }

    AsyncReadout(const AsyncReadout&) = delete;
    AsyncReadout& operator=(const AsyncReadout&) = delete;

    void issue(const T* d_src, backend::Stream stream = backend::kDefaultStream) {
        int i = _next_write;
        backend::memcpy_async(_h[i], d_src, (size_t)_n_elems * sizeof(T),
                              backend::MemcpyKind::DeviceToHost, stream);
        backend::event_record(_evt[i], stream);
        _valid[i] = true;
        _next_write = (_next_write + 1) % _n_slots;
    }

    // Pointer to the slot most recently filled by issue(), after syncing on
    // its completion event. nullptr if no issue() yet.
    const T* read_previous() {
        int i = (_next_write + _n_slots - 1) % _n_slots;
        if (!_valid[i]) return nullptr;
        backend::event_synchronize(_evt[i]);
        return _h[i];
    }

    int n_elems() const { return _n_elems; }

private:
    int _n_elems;
    int _n_slots;
    std::vector<T*> _h;
    std::vector<backend::Event*> _evt;
    std::vector<bool> _valid;
    int _next_write;
};


// Global device memory pool -- enum-keyed buffers with high-water-mark semantics.
// Memory never shrinks on resize; only reallocates when the new size exceeds
// capacity. freeAll() is the only way to release everything; free(slot) drops a
// single logical buffer. All DeviceVector/Tensor2D/3D/5D::resize() and
// Quantized*::resize() calls route through here.
//
// Storage is a dense array indexed by PoolKey = (PoolSlot << kSubBits) | sub
// (see PoolSlots.h). The array is fixed-size (kNumPoolKeys) so there is no
// rehash/insert -- the mutex only serializes the device malloc/free. A small side
// map holds `acquire_dynamic` scratch whose key is built from runtime-variable
// content and therefore has no compile-time PoolSlot.
class DevicePool {
    struct Slot {
        void*     ptr        = nullptr;
        size_t    cap_bytes  = 0;  // allocated (0 when the arena owns it)
        size_t    used_bytes = 0;  // logical (last requested)
        uint8_t   dtype_tag  = (uint8_t)NpyScalar::u1;  // scalar type of last acquire<T>
        bool      owns       = true;  // false: ptr is a slice of _arena
        uint32_t  epoch      = 0;     // phase instance this slice came from
    };
    struct DynSlot {
        Slot         slot;
        VramCategory cat = VramCategory::Other;
        PoolPhase    phase = PoolPhase::None;
    };

    // The one allocation every PoolPhase carves its buffers out of.
    // `want` is the largest cursor any phase has asked for since the last
    // grow; growing happens at a phase switch, when nothing holds a slice.
    struct Arena {
        void*  ptr    = nullptr;
        size_t cap    = 0;
        size_t cursor = 0;
        size_t want   = 0;
    };
    static constexpr size_t kArenaAlign = 256;

    std::vector<Slot> _slots;                          // indexed by PoolKey
    std::unordered_map<std::string, DynSlot> _dyn;     // acquire_dynamic scratch
    Arena     _arena;
    PoolPhase _phase = PoolPhase::None;
    uint32_t  _epoch = 0;   // bumped per begin_phase; 0 never matches a Slot
    // Guards _slots / _dyn / _arena against concurrent acquire. It does NOT
    // make the phase protocol thread-safe: two threads in different phases
    // would fight over the arena, which the engine mutex is what prevents.
    mutable std::mutex _mu;

    DevicePool() : _slots(kNumPoolKeys) {}
    DevicePool(const DevicePool&) = delete;
    DevicePool& operator=(const DevicePool&) = delete;

    static bool _poison_pool() {
        static const bool on = [] {
            const char* e = spirula::env("POISON_POOL");
            return e && e[0] && e[0] != '0';
        }();
        return on;
    }

    // SS_POOL_ALIAS=0 gives every slot its own allocation again; the kill
    // switch for the arena, and the A/B lever for measuring what it saves.
    static bool _alias_enabled() {
        static const bool on = [] {
            const char* e = spirula::env("POOL_ALIAS");
            return !(e && e[0] == '0');
        }();
        return on;
    }

    // SS_POOL_ALIAS_POISON=1 fills the arena on every phase switch, so a read
    // that outlives its phase produces NaNs instead of plausible stale data.
    static bool _poison_arena() {
        static const bool on = [] {
            const char* e = spirula::env("POOL_ALIAS_POISON");
            return e && e[0] && e[0] != '0';
        }();
        return on;
    }

    // Grow-if-needed on a Slot already selected by the caller (mutex held).
    template<typename T>
    static T* _acquire_into(Slot& slot, size_t n) {
        size_t bytes = n * sizeof(T);
        // An arena slice is not capacity this slot may reuse; drop it first.
        if (!slot.owns) { slot.ptr = nullptr; slot.cap_bytes = 0; slot.owns = true; }
        if (bytes > slot.cap_bytes) {
            if (slot.ptr && slot.owns) backend::device_free(slot.ptr);
            // Reset before the (throwing) allocation so an OOM leaves the slot
            // in a consistent empty state rather than {null ptr, stale cap}.
            slot.ptr = nullptr;
            slot.cap_bytes = 0;
            slot.ptr = backend::device_malloc_checked(bytes);
            slot.cap_bytes = bytes;
            // SS_POISON_POOL=1: fresh device memory holds whatever the
            // driver left, so a read-before-write is silent where that is
            // zero and catastrophic where it is not. NaN makes it show.
            if (_poison_pool()) backend::memset_sync(slot.ptr, 0xff, bytes);
        }
        slot.owns = true;
        slot.used_bytes = bytes;
        slot.dtype_tag  = (uint8_t)npy_scalar_tag<T>();
        return static_cast<T*>(slot.ptr);
    }

    // Bump-allocate out of the arena (mutex held). What does not fit yet gets
    // a private allocation for this call and raises `want`, so the next phase
    // switch grows the arena and the call after that is aliased again.
    template<typename T>
    T* _acquire_arena(Slot& slot, size_t n) {
        // Each acquire gets a FRESH slice, so a second one in the same phase
        // would silently move the buffer -- say so rather than hand it back.
        if (slot.epoch == _epoch)
            throw std::runtime_error(
                "DevicePool: arena-backed buffer acquired twice in one phase; "
                "resize it once, or split the phase");
        slot.epoch = _epoch;
        const size_t bytes = n * sizeof(T);
        const size_t off = (_arena.cursor + kArenaAlign - 1) & ~(kArenaAlign - 1);
        // The cursor advances even when the slice does not fit, or `want`
        // would come out one buffer wide instead of one phase wide.
        _arena.cursor = off + bytes;
        if (_arena.want < _arena.cursor) _arena.want = _arena.cursor;
        if (!_arena.ptr || off + bytes > _arena.cap)
            return _acquire_into<T>(slot, n);
        if (slot.ptr && slot.owns) backend::device_free(slot.ptr);
        slot.ptr        = static_cast<char*>(_arena.ptr) + off;
        slot.cap_bytes  = 0;
        slot.used_bytes = bytes;
        slot.dtype_tag  = (uint8_t)npy_scalar_tag<T>();
        slot.owns       = false;
        return static_cast<T*>(slot.ptr);
    }

    // The one place the phase rule is enforced (mutex held). `what` names the
    // buffer for the error; enum slots and acquire_dynamic both come through.
    template<typename T>
    T* _acquire_checked(Slot& slot, size_t n, PoolPhase ph, const char* what) {
        if (ph == PoolPhase::None || !_alias_enabled())
            return _acquire_into<T>(slot, n);
        if (ph != _phase)
            throw std::runtime_error(
                std::string("DevicePool: ") + what + " is arena-backed in phase "
                + to_string(ph) + " but the open phase is " + to_string(_phase)
                + " -- see POOL_ALIAS_TABLE in core/PoolSlots.h");
        return _acquire_arena<T>(slot, n);
    }

public:
    static DevicePool& global() {
        static DevicePool pool;
        return pool;
    }

    // Open `p`. Every slice the previous phase carved out of the arena dies
    // here -- POOL_ALIAS_TABLE's rows are the claim that nothing still reads
    // one. Grows the arena first, which is safe for the same reason.
    void begin_phase(PoolPhase p) {
        std::lock_guard<std::mutex> lock(_mu);
        _phase = p;
        ++_epoch;
        if (!_alias_enabled() || p == PoolPhase::None) return;
        if (_arena.want > _arena.cap) {
            for (Slot& s : _slots)
                if (!s.owns) { s.ptr = nullptr; s.owns = true; }
            for (auto& kv : _dyn) {
                Slot& s = kv.second.slot;
                if (!s.owns) { s.ptr = nullptr; s.owns = true; }
            }
            if (_arena.ptr) backend::device_free(_arena.ptr);
            _arena.ptr = nullptr;
            _arena.cap = 0;
            _arena.ptr = backend::device_malloc_checked(_arena.want);
            _arena.cap = _arena.want;
        }
        _arena.cursor = 0;
        if (_poison_arena() && _arena.ptr)
            backend::memset_sync(_arena.ptr, 0xff, _arena.cap);
    }

    struct ArenaStats { size_t cap_bytes; size_t want_bytes; size_t slots; };
    ArenaStats arenaStats() const {
        std::lock_guard<std::mutex> lock(_mu);
        size_t n = 0;
        for (const Slot& s : _slots) if (!s.owns) ++n;
        for (const auto& kv : _dyn) if (!kv.second.slot.owns) ++n;
        return { _arena.cap, _arena.want, n };
    }

    // Return a pointer to at least `n` elements of type T for the given key.
    // Reallocates only when current capacity is exceeded.
    template<typename T>
    T* acquire(PoolKey key, size_t n) {
        std::lock_guard<std::mutex> lock(_mu);
        const PoolSlot slot = pool_key_slot(key);
        return _acquire_checked<T>(_slots[key], n, slot_phase(slot),
                                   slot_name(slot));
    }

    // Convenience: acquire a (slot, sub) buffer. sub defaults to the main slot.
    template<typename T>
    T* acquire(PoolSlot slot, size_t n, uint32_t sub = kSubMain) {
        return acquire<T>(pool_key(slot, sub), n);
    }

    // Escape hatch for Never-saved scratch whose key is built at runtime and so
    // has no compile-time PoolSlot. `cat` places it in the VRAM report; `name`
    // is the display/debug key. Returns raw bytes (caller casts).
    void* acquire_dynamic(VramCategory cat, const std::string& name,
                          size_t bytes, PoolPhase phase = PoolPhase::None) {
        std::lock_guard<std::mutex> lock(_mu);
        DynSlot& d = _dyn[name];
        d.cat = cat;
        d.phase = phase;
        return _acquire_checked<uint8_t>(d.slot, bytes, phase, name.c_str());
    }

    // Free a single logical buffer (all of its sub-allocations).
    void free(PoolSlot slot) {
        std::lock_guard<std::mutex> lock(_mu);
        for (uint32_t sub = 0; sub <= kSubMask; ++sub) {
            Slot& s = _slots[pool_key(slot, sub)];
            if (s.ptr && s.owns) backend::device_free(s.ptr);
            s = Slot{};
        }
    }

    // Free every acquire_dynamic slot whose name starts with `prefix`.
    //
    // The one caller is a primitive switch in the viewer: the 3DGS/Mip and
    // 3DGUT screen buffers share slot NAMES but not shapes (10 floats per
    // splat against 7, and the first slot is float2 against float3), so a
    // pool that only ever grows ends up holding the union of the two rather
    // than the larger. Handing them back on the switch costs one
    // reallocation on the next render and keeps the high-water mark at the
    // larger of the two, which is what a user comparing primitives expects.
    void free_dynamic_prefix(const std::string& prefix) {
        std::lock_guard<std::mutex> lock(_mu);
        for (auto it = _dyn.begin(); it != _dyn.end();) {
            if (it->first.rfind(prefix, 0) == 0) {
                if (it->second.slot.ptr && it->second.slot.owns)
                    backend::device_free(it->second.slot.ptr);
                it = _dyn.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Free all managed device memory.
    void freeAll() {
        std::lock_guard<std::mutex> lock(_mu);
        for (auto& s : _slots) {
            if (s.ptr && s.owns) backend::device_free(s.ptr);
            s = Slot{};
        }
        for (auto& kv : _dyn)
            if (kv.second.slot.ptr && kv.second.slot.owns)
                backend::device_free(kv.second.slot.ptr);
        _dyn.clear();
        if (_arena.ptr) backend::device_free(_arena.ptr);
        _arena = Arena{};
        _phase = PoolPhase::None;
    }

    // Total bytes allocated (capacity, not logical size).
    size_t totalAllocBytes() const {
        std::lock_guard<std::mutex> lock(_mu);
        size_t total = _arena.cap;
        for (auto& s : _slots) total += s.cap_bytes;
        for (auto& kv : _dyn)  total += kv.second.slot.cap_bytes;
        return total;
    }

    // Per-slot breakdown: [(name, used_bytes, cap_bytes), ...]. Names are
    // reconstructed byte-identically to the old string keys (slot_name + sub
    // suffix), so existing consumers keep working.
    std::vector<std::tuple<std::string, size_t, size_t>> getBreakdown() const {
        std::lock_guard<std::mutex> lock(_mu);
        std::vector<std::tuple<std::string, size_t, size_t>> result;
        for (uint32_t i = 0; i < _slots.size(); ++i) {
            const Slot& s = _slots[i];
            if (!s.ptr && s.cap_bytes == 0 && s.used_bytes == 0) continue;
            std::string name = std::string(slot_name(pool_key_slot(i)))
                             + sub_suffix(pool_key_sub(i));
            result.emplace_back(std::move(name), s.used_bytes, s.cap_bytes);
        }
        for (auto& kv : _dyn)
            result.emplace_back(kv.first, kv.second.slot.used_bytes,
                                kv.second.slot.cap_bytes);
        return result;
    }

    // One row per live buffer. `arena` means the bytes are a slice of the
    // shared arena, so `cap` is 0 and the allocation is counted once in
    // arenaStats() instead.
    struct BreakdownRow {
        std::string  name;
        VramCategory cat;
        size_t       used_bytes;
        size_t       cap_bytes;
        bool         arena;
    };

    std::vector<BreakdownRow> breakdown() const {
        std::lock_guard<std::mutex> lock(_mu);
        std::vector<BreakdownRow> result;
        for (uint32_t i = 0; i < _slots.size(); ++i) {
            const Slot& s = _slots[i];
            if (!s.ptr && s.cap_bytes == 0 && s.used_bytes == 0) continue;
            PoolSlot slot = pool_key_slot(i);
            result.push_back({ std::string(slot_name(slot)) +
                                   sub_suffix(pool_key_sub(i)),
                               slot_category(slot), s.used_bytes, s.cap_bytes,
                               !s.owns });
        }
        for (auto& kv : _dyn)
            result.push_back({ kv.first, kv.second.cat,
                               kv.second.slot.used_bytes,
                               kv.second.slot.cap_bytes,
                               !kv.second.slot.owns });
        return result;
    }

    // Categorized breakdown: [(name, category, used_bytes, cap_bytes), ...].
    // category is (int)VramCategory. Used by the C++-driven VRAM report.
    std::vector<std::tuple<std::string, int, size_t, size_t>>
    getBreakdownCategorized() const {
        std::vector<std::tuple<std::string, int, size_t, size_t>> result;
        for (auto& r : breakdown())
            result.emplace_back(r.name, (int)r.cat, r.used_bytes, r.cap_bytes);
        return result;
    }

    // A live pool buffer selected for checkpointing. `name` is the display key
    // (slot name + sub suffix, e.g. "eng.sh_quant.q"); `ptr`/`nbytes` are the
    // device source for a D->H dump. Only enum slots participate -- dynamic
    // scratch is always SaveClass::Never and never appears here.
    struct SavedSlot {
        std::string name;
        const void* ptr;
        size_t      nbytes;     // logical (used_bytes) to serialize
        uint8_t     dtype_tag;
        SaveClass   save;
    };

    // Enumerate every live slot whose SaveClass is >= `min`, in enum order.
    //   min = SaveClass::Always -> appearance/inference params only
    //   min = SaveClass::Resume -> also optimizer/world state (full resume)
    // (Never < Resume < Always, so ">= min" gives the intended supersets.)
    std::vector<SavedSlot> saved(SaveClass min) const {
        std::lock_guard<std::mutex> lock(_mu);
        std::vector<SavedSlot> out;
        for (uint32_t i = 0; i < _slots.size(); ++i) {
            const Slot& s = _slots[i];
            if (!s.ptr || s.used_bytes == 0) continue;
            PoolSlot slot = pool_key_slot(i);
            SaveClass sc  = slot_save(slot);
            if ((int)sc < (int)min) continue;
            out.push_back({ std::string(slot_name(slot)) + sub_suffix(pool_key_sub(i)),
                            s.ptr, s.used_bytes, s.dtype_tag, sc });
        }
        return out;
    }

    // Look up a live slot's device pointer + logical byte size, for restoring
    // checkpoint bytes into an already-allocated buffer. False if not live.
    bool lookup(PoolSlot slot, uint32_t sub, void*& ptr, size_t& nbytes) const {
        std::lock_guard<std::mutex> lock(_mu);
        const Slot& s = _slots[pool_key(slot, sub)];
        if (!s.ptr || s.used_bytes == 0) return false;
        ptr    = s.ptr;
        nbytes = s.used_bytes;
        return true;
    }

    ~DevicePool() { freeAll(); }
};


// Reverse map: slot name string -> PoolSlot, built once from the metadata table.
inline bool pool_slot_from_name(const std::string& name, PoolSlot& out) {
    static const std::unordered_map<std::string, PoolSlot> kMap = [] {
        std::unordered_map<std::string, PoolSlot> m;
        for (size_t i = 0; i < (size_t)PoolSlot::Count; ++i)
            m.emplace(kSlotMeta[i].name, (PoolSlot)i);
        return m;
    }();
    auto it = kMap.find(name);
    if (it == kMap.end()) return false;
    out = it->second;
    return true;
}

// Parse a saved-buffer name back into (slot, sub):
//   "world.means"       -> (WorldMeans,  kSubMain)
//   "eng.sh_quant.q"    -> (EngShQuant,  kSubPacked)
//   "eng.sh_quant.qb"   -> (EngShQuant,  kSubBounds)
// False if the base name is not a known slot.
inline bool parse_saved_name(const std::string& name, PoolSlot& slot, uint32_t& sub) {
    if (pool_slot_from_name(name, slot)) { sub = kSubMain; return true; }
    auto ends_with = [&](const std::string& suf) {
        return name.size() > suf.size() &&
               name.compare(name.size() - suf.size(), suf.size(), suf) == 0;
    };
    if (ends_with(".qb") && pool_slot_from_name(name.substr(0, name.size() - 3), slot)) {
        sub = kSubBounds; return true;
    }
    if (ends_with(".q") && pool_slot_from_name(name.substr(0, name.size() - 2), slot)) {
        sub = kSubPacked; return true;
    }
    return false;
}


// Single reusable scratch buffer for CUB-style temporary allocations.
// Grows monotonically; never fragmented. Intended for single-stream use.
class DeviceScratch {
    void*  _ptr = nullptr;
    size_t _cap = 0;
    mutable std::mutex _mu;  // protects _ptr/_cap from concurrent threads

    DeviceScratch() = default;
    DeviceScratch(const DeviceScratch&) = delete;
    DeviceScratch& operator=(const DeviceScratch&) = delete;

public:
    static DeviceScratch& global() {
        static DeviceScratch s;
        return s;
    }

    // Returns a pointer to at least `bytes` bytes of device scratch space.
    // Note: callers using the returned pointer asynchronously across threads must
    // synchronize externally; device_free's implicit device sync (a documented
    // part of the BackendRuntime contract, matching cudaFree) ensures any
    // in-flight kernel using the previous buffer completes before the new
    // allocation when growing.
    void* acquire(size_t bytes) {
        std::lock_guard<std::mutex> lock(_mu);
        if (bytes > _cap) {
            if (_ptr) backend::device_free(_ptr);
            _ptr = nullptr;
            _cap = 0;
            _ptr = backend::device_malloc_checked(bytes, "scratch buffer");
            _cap = bytes;
        }
        return _ptr;
    }

    size_t capBytes() const {
        std::lock_guard<std::mutex> lock(_mu);
        return _cap;
    }

    void freeAll() {
        std::lock_guard<std::mutex> lock(_mu);
        if (_ptr) { backend::device_free(_ptr); _ptr = nullptr; _cap = 0; }
    }

    ~DeviceScratch() { freeAll(); }
};


// Open a pool phase; the arena's slices from the previous one die here.
// Called by the launcher that owns a phase, not by the engine layer -- see
// POOL_ALIAS_TABLE in core/PoolSlots.h.
inline void pool_begin_phase(PoolPhase p) {
    DevicePool::global().begin_phase(p);
}


// Free all device memory managed by the pool and scratch buffer.
// Safe to call at VRAM pressure or before process exit.
inline void freeAllDeviceMemory() {
    DevicePool::global().freeAll();
    DeviceScratch::global().freeAll();
}


// 1D device vector -- non-owning view; memory owned by DevicePool or PyTorch.
template<typename T>
class DeviceVector {
protected:

    int64_t _numel;
    T* __restrict__ _data_ptr;

public:

    int64_t size() const {
        return _numel;
    }

    T* data_ptr() const {
        return _data_ptr;
    }

    DeviceVector() : _data_ptr(nullptr), _numel(0) {}

    DeviceVector(const TorchTensorView& view) {
        _data_ptr = (T*)std::get<0>(view);
        uint32_t element_size = std::get<1>(view);
        std::vector<int64_t> shape = std::get<2>(view);
        if (shape.size() == 1)
            shape.push_back(1);
        if (shape.size() != 2)
            throw std::runtime_error("Expected 2D tensor view with channel-last layout");
        if (shape[1] * element_size != sizeof(T))
            throw std::runtime_error("Element size mismatch");
        _numel = shape[0];
    }

    // shallow copy
    DeviceVector& operator=(const DeviceVector&) = default;

    // Pool-backed resize: memory owned by DevicePool, this is a non-owning view.
    // `sub` selects a sub-allocation within the slot (default = main); the
    // QuantizedTensor* codecs pass kSubPacked / kSubBounds.
    void resize(PoolSlot slot, int64_t numel, uint32_t sub = kSubMain) {
        _data_ptr = DevicePool::global().acquire<T>(slot, (size_t)numel, sub);
        _numel = numel;
    }

    // Dynamic-keyed resize for Never-class scratch whose name is built at
    // runtime (prefix + index / suffix). Routes to the pool's side map with an
    // explicit category. See DevicePool::acquire_dynamic.
    void resize_dynamic(VramCategory cat, const std::string& name, int64_t numel,
                        PoolPhase phase = PoolPhase::None) {
        _data_ptr = (T*)DevicePool::global().acquire_dynamic(
            cat, name, (size_t)numel * sizeof(T), phase);
        _numel = numel;
    }

    // fill zero
    void zero() {
        if (_data_ptr)
            backend::memset_sync(_data_ptr, 0, _numel * sizeof(T));
    }

#ifdef __CUDACC__
    __device__ T load(int64_t idx) const {
        return _data_ptr[idx];
    }
    __device__ void store(int64_t idx, T value) {
        _data_ptr[idx] = value;
    }
    __device__ T atomicAdd(int64_t idx, T value) {
        return ::atomicAdd(&_data_ptr[idx], value);
    }
    __device__ T atomicMax(int64_t idx, T value) {
        return ::atomicMax(&_data_ptr[idx], value);
    }
    __device__ T atomicMin(int64_t idx, T value) {
        return ::atomicMin(&_data_ptr[idx], value);
    }
#endif

};


// 2D device tensor -- non-owning view; memory owned by DevicePool or PyTorch.
template<typename T>
class DeviceTensor2D {
protected:

    int64_t _numel_0;
    int64_t _numel_1;
    T* __restrict__ _data_ptr;

public:

    template<int dim>
    int64_t size() const {
        static_assert(dim == 0 || dim == 1, "Invalid dimension");
        if constexpr (dim == 0) return _numel_0;
        else if constexpr (dim == 1) return _numel_1;
    }

    int64_t numel() const {
        return _numel_0 * _numel_1;
    }

    T* data_ptr() const {
        return _data_ptr;
    }

    DeviceTensor2D() : _data_ptr(nullptr), _numel_0(0), _numel_1(0) {}

    DeviceTensor2D(const TorchTensorView& view) {
        _data_ptr = (T*)std::get<0>(view);
        uint32_t element_size = std::get<1>(view);
        std::vector<int64_t> shape = std::get<2>(view);
        if (shape.size() == 2)
            shape.push_back(1);
        if (shape.size() != 3)
            throw std::runtime_error("Expected 3D tensor view with channel-last layout");
        if (shape[2] * element_size != sizeof(T))
            throw std::runtime_error("Element size mismatch");
        _numel_0 = shape[0];
        _numel_1 = shape[1];
    }

    // shallow copy
    DeviceTensor2D& operator=(const DeviceTensor2D&) = default;

    // Pool-backed resize: memory owned by DevicePool, this is a non-owning view.
    void resize(PoolSlot slot, int64_t numel_0, int64_t numel_1, uint32_t sub = kSubMain) {
        _data_ptr = DevicePool::global().acquire<T>(slot, (size_t)(numel_0 * numel_1), sub);
        _numel_0 = numel_0;
        _numel_1 = numel_1;
    }

    // Dynamic-keyed resize (Never-class scratch, runtime-built name).
    void resize_dynamic(VramCategory cat, const std::string& name,
                        int64_t numel_0, int64_t numel_1,
                        PoolPhase phase = PoolPhase::None) {
        _data_ptr = (T*)DevicePool::global().acquire_dynamic(
            cat, name, (size_t)(numel_0 * numel_1) * sizeof(T), phase);
        _numel_0 = numel_0;
        _numel_1 = numel_1;
    }

    // Shape-only descriptor: keeps the [n0, n1] dims but holds no device
    // memory. Used when the canonical storage moves to a packed/quantized
    // buffer elsewhere but downstream code still expects shape info (e.g.,
    // TensorArray's per-axis stride inference).
    void set_shape_no_alloc(int64_t numel_0, int64_t numel_1) {
        _data_ptr = nullptr;
        _numel_0 = numel_0;
        _numel_1 = numel_1;
    }

    // fill zero
    void zero() {
        if (_data_ptr)
            backend::memset_sync(_data_ptr, 0, _numel_0 * _numel_1 * sizeof(T));
    }

#ifdef __CUDACC__
    __device__ T load(int64_t idx0, int64_t idx1) const {
        return _data_ptr[idx0 * _numel_1 + idx1];
    }
    __device__ void store(int64_t idx0, int64_t idx1, T value) {
        _data_ptr[idx0 * _numel_1 + idx1] = value;
    }
    __device__ T atomicAdd(int64_t idx0, int64_t idx1, T value) {
        return ::atomicAdd(&_data_ptr[idx0 * _numel_1 + idx1], value);
    }
    __device__ T atomicMax(int64_t idx0, int64_t idx1, T value) {
        return ::atomicMax(&_data_ptr[idx0 * _numel_1 + idx1], value);
    }
    __device__ T atomicMin(int64_t idx0, int64_t idx1, T value) {
        return ::atomicMin(&_data_ptr[idx0 * _numel_1 + idx1], value);
    }
#endif

};


// 3D device tensor -- non-owning view; memory owned by DevicePool or PyTorch.
template<typename T>
class DeviceTensor3D {
protected:

    int64_t _numel_0;
    int64_t _numel_1;
    int64_t _numel_2;
    T* __restrict__ _data_ptr;

public:

    template<int dim>
    int64_t size() const {
        static_assert(dim == 0 || dim == 1 || dim == 2, "Invalid dimension");
        if constexpr (dim == 0) return _numel_0;
        else if constexpr (dim == 1) return _numel_1;
        else if constexpr (dim == 2) return _numel_2;
    }

    int64_t numel() const {
        return _numel_0 * _numel_1 * _numel_2;
    }

    T* data_ptr() const {
        return _data_ptr;
    }

    DeviceTensor3D() : _data_ptr(nullptr), _numel_0(0), _numel_1(0), _numel_2(0) {}

    DeviceTensor3D(const TorchTensorView& view) {
        _data_ptr = (T*)std::get<0>(view);
        uint32_t element_size = std::get<1>(view);
        std::vector<int64_t> shape = std::get<2>(view);
        if (shape.size() == 3)
            shape.push_back(1);
        if (shape.size() != 4)
            throw std::runtime_error("Expected 4D tensor view with channel-last layout");
        if (shape[3] * element_size != sizeof(T))
            throw std::runtime_error("Element size mismatch");
        _numel_0 = shape[0];
        _numel_1 = shape[1];
        _numel_2 = shape[2];
    }

    // shallow copy
    DeviceTensor3D& operator=(const DeviceTensor3D&) = default;

    // Pool-backed resize: memory owned by DevicePool, this is a non-owning view.
    void resize(PoolSlot slot, int64_t numel_0, int64_t numel_1, int64_t numel_2,
                uint32_t sub = kSubMain) {
        _data_ptr = DevicePool::global().acquire<T>(slot, (size_t)(numel_0 * numel_1 * numel_2), sub);
        _numel_0 = numel_0;
        _numel_1 = numel_1;
        _numel_2 = numel_2;
    }

    // Dynamic-keyed resize (Never-class scratch, runtime-built name).
    void resize_dynamic(VramCategory cat, const std::string& name,
                        int64_t numel_0, int64_t numel_1, int64_t numel_2,
                        PoolPhase phase = PoolPhase::None) {
        _data_ptr = (T*)DevicePool::global().acquire_dynamic(
            cat, name, (size_t)(numel_0 * numel_1 * numel_2) * sizeof(T), phase);
        _numel_0 = numel_0;
        _numel_1 = numel_1;
        _numel_2 = numel_2;
    }

    // fill zero
    void zero() {
        if (_data_ptr)
            backend::memset_sync(_data_ptr, 0, _numel_0 * _numel_1 * _numel_2 * sizeof(T));
    }

#ifdef __CUDACC__
    __device__ T load(int64_t idx0, int64_t idx1, int64_t idx2) const {
        return _data_ptr[idx0 * _numel_1 * _numel_2 + idx1 * _numel_2 + idx2];
    }
    __device__ void store(int64_t idx0, int64_t idx1, int64_t idx2, T value) {
        _data_ptr[idx0 * _numel_1 * _numel_2 + idx1 * _numel_2 + idx2] = value;
    }
    __device__ T atomicAdd(int64_t idx0, int64_t idx1, int64_t idx2, T value) {
        return ::atomicAdd(&_data_ptr[idx0 * _numel_1 * _numel_2 + idx1 * _numel_2 + idx2], value);
    }
    __device__ T atomicMax(int64_t idx0, int64_t idx1, int64_t idx2, T value) {
        return ::atomicMax(&_data_ptr[idx0 * _numel_1 * _numel_2 + idx1 * _numel_2 + idx2], value);
    }
    __device__ T atomicMin(int64_t idx0, int64_t idx1, int64_t idx2, T value) {
        return ::atomicMin(&_data_ptr[idx0 * _numel_1 * _numel_2 + idx1 * _numel_2 + idx2], value);
    }
#endif

};


// 5D device tensor -- non-owning view; memory owned by DevicePool or PyTorch.
// Channel-last expected (last dim packs into sizeof(T)); fits bilagrid [N,C,L,H,W]
// when each cell is a single float.
template<typename T>
class DeviceTensor5D {
protected:

    int64_t _numel_0;
    int64_t _numel_1;
    int64_t _numel_2;
    int64_t _numel_3;
    int64_t _numel_4;
    T* __restrict__ _data_ptr;

public:

    template<int dim>
    int64_t size() const {
        static_assert(dim >= 0 && dim < 5, "Invalid dimension");
        if constexpr (dim == 0) return _numel_0;
        else if constexpr (dim == 1) return _numel_1;
        else if constexpr (dim == 2) return _numel_2;
        else if constexpr (dim == 3) return _numel_3;
        else if constexpr (dim == 4) return _numel_4;
    }

    int64_t numel() const {
        return _numel_0 * _numel_1 * _numel_2 * _numel_3 * _numel_4;
    }

    T* data_ptr() const {
        return _data_ptr;
    }

    DeviceTensor5D() : _data_ptr(nullptr),
        _numel_0(0), _numel_1(0), _numel_2(0), _numel_3(0), _numel_4(0) {}

    DeviceTensor5D(const TorchTensorView& view) {
        _data_ptr = (T*)std::get<0>(view);
        uint32_t element_size = std::get<1>(view);
        std::vector<int64_t> shape = std::get<2>(view);
        if (shape.size() == 5)
            shape.push_back(1);
        if (shape.size() != 6)
            throw std::runtime_error("Expected 6D tensor view with channel-last layout");
        if (shape[5] * element_size != sizeof(T))
            throw std::runtime_error("Element size mismatch");
        _numel_0 = shape[0];
        _numel_1 = shape[1];
        _numel_2 = shape[2];
        _numel_3 = shape[3];
        _numel_4 = shape[4];
    }

    // shallow copy
    DeviceTensor5D& operator=(const DeviceTensor5D&) = default;

    // Pool-backed resize: memory owned by DevicePool, this is a non-owning view.
    void resize(PoolSlot slot, int64_t n0, int64_t n1, int64_t n2, int64_t n3, int64_t n4,
                uint32_t sub = kSubMain) {
        _data_ptr = DevicePool::global().acquire<T>(slot, (size_t)(n0 * n1 * n2 * n3 * n4), sub);
        _numel_0 = n0; _numel_1 = n1; _numel_2 = n2; _numel_3 = n3; _numel_4 = n4;
    }

    // Shape-only descriptor: keep dims, drop the pool slot. Used when a
    // packed/quantized buffer takes over as the canonical store but downstream
    // code still queries size<>() for layout.
    void set_shape_no_alloc(int64_t n0, int64_t n1, int64_t n2, int64_t n3, int64_t n4) {
        _data_ptr = nullptr;
        _numel_0 = n0; _numel_1 = n1; _numel_2 = n2; _numel_3 = n3; _numel_4 = n4;
    }

    // fill zero
    void zero() {
        if (_data_ptr)
            backend::memset_sync(_data_ptr, 0, _numel_0 * _numel_1 * _numel_2 * _numel_3 * _numel_4 * sizeof(T));
    }

#ifdef __CUDACC__
    __device__ T load(int64_t i0, int64_t i1, int64_t i2, int64_t i3, int64_t i4) const {
        int64_t stride_3 = _numel_4;
        int64_t stride_2 = _numel_3 * stride_3;
        int64_t stride_1 = _numel_2 * stride_2;
        int64_t stride_0 = _numel_1 * stride_1;
        return _data_ptr[i0 * stride_0 + i1 * stride_1 + i2 * stride_2 + i3 * stride_3 + i4];
    }
    __device__ void store(int64_t i0, int64_t i1, int64_t i2, int64_t i3, int64_t i4, T value) {
        int64_t stride_3 = _numel_4;
        int64_t stride_2 = _numel_3 * stride_3;
        int64_t stride_1 = _numel_2 * stride_2;
        int64_t stride_0 = _numel_1 * stride_1;
        _data_ptr[i0 * stride_0 + i1 * stride_1 + i2 * stride_2 + i3 * stride_3 + i4] = value;
    }
#endif

};


// ND device tensor -- non-owning view.
// Intended for heterogeneous containers (e.g. std::vector of tensors with different ranks).
// For fixed-rank device-friendly code, prefer DeviceVector/Tensor2D/Tensor3D.
template<typename T>
class DeviceTensorND {
protected:

    int64_t ndim;
    std::vector<int64_t> shape;
    T* __restrict__ _data_ptr;

public:

    int64_t size(int dim) const {
        if (dim < 0 || dim >= ndim)
            throw std::runtime_error("Invalid dimension");
        return shape[dim];
    }

    int64_t get_ndim() const {
        return ndim;
    }

    int64_t numel() const {
        int64_t n = 1;
        for (int i = 0; i < ndim; i++) n *= shape[i];
        return n;
    }

    T* data_ptr() const {
        return _data_ptr;
    }

    DeviceTensorND() : _data_ptr(nullptr), ndim(0) {}

    DeviceTensorND(const TorchTensorView& view) {
        _data_ptr = (T*)std::get<0>(view);
        uint32_t element_size = std::get<1>(view);
        std::vector<int64_t> s = std::get<2>(view);
        if (s.empty())
            throw std::runtime_error("Expected non-empty tensor view");
        if (s.back() * element_size != sizeof(T))
            throw std::runtime_error("Element size mismatch");
        ndim = (int64_t)s.size() - 1;
        shape.assign(s.begin(), s.begin() + ndim);
    }

    DeviceTensorND(const DeviceVector<T>& vec) {
        _data_ptr = vec.data_ptr();
        ndim = 1;
        shape = {vec.size()};
    }

    DeviceTensorND(const DeviceTensor2D<T>& tensor) {
        _data_ptr = tensor.data_ptr();
        ndim = 2;
        shape = {tensor.template size<0>(), tensor.template size<1>()};
    }

    DeviceTensorND(const DeviceTensor3D<T>& tensor) {
        _data_ptr = tensor.data_ptr();
        ndim = 3;
        shape = {tensor.template size<0>(), tensor.template size<1>(), tensor.template size<2>()};
    }
};


class DeviceTensorFloatND : public DeviceTensorND<float> {
public:
    using DeviceTensorND::DeviceTensorND;

    DeviceTensorFloatND(const DeviceVector<float2>& vec) {
        _data_ptr = (float*)vec.data_ptr();
        ndim = 2;
        shape = {vec.size(), 2};
    }
    DeviceTensorFloatND(const DeviceTensor2D<float2>& tensor) {
        _data_ptr = (float*)tensor.data_ptr();
        ndim = 3;
        shape = {tensor.template size<0>(), tensor.template size<1>(), 2};
    }
    DeviceTensorFloatND(const DeviceTensor3D<float2>& tensor) {
        _data_ptr = (float*)tensor.data_ptr();
        ndim = 4;
        shape = {tensor.template size<0>(), tensor.template size<1>(), tensor.template size<2>(), 2};
    }

    DeviceTensorFloatND(const DeviceVector<float3>& vec) {
        _data_ptr = (float*)vec.data_ptr();
        ndim = 2;
        shape = {vec.size(), 3};
    }
    DeviceTensorFloatND(const DeviceTensor2D<float3>& tensor) {
        _data_ptr = (float*)tensor.data_ptr();
        ndim = 3;
        shape = {tensor.template size<0>(), tensor.template size<1>(), 3};
    }
    DeviceTensorFloatND(const DeviceTensor3D<float3>& tensor) {
        _data_ptr = (float*)tensor.data_ptr();
        ndim = 4;
        shape = {tensor.template size<0>(), tensor.template size<1>(), tensor.template size<2>(), 3};
    }

    DeviceTensorFloatND(const DeviceVector<float4>& vec) {
        _data_ptr = (float*)vec.data_ptr();
        ndim = 2;
        shape = {vec.size(), 4};
    }
    DeviceTensorFloatND(const DeviceTensor2D<float4>& tensor) {
        _data_ptr = (float*)tensor.data_ptr();
        ndim = 3;
        shape = {tensor.template size<0>(), tensor.template size<1>(), 4};
    }
    DeviceTensorFloatND(const DeviceTensor3D<float4>& tensor) {
        _data_ptr = (float*)tensor.data_ptr();
        ndim = 4;
        shape = {tensor.template size<0>(), tensor.template size<1>(), tensor.template size<2>(), 4};
    }
};


// ============================================================================
// QuantizedAdamState -- joint (u, log_s) Adam optimizer-state quantization.
//
// Per cell the codec stores TWO linearly-quantized "primitive" values:
//     primitives.x = u     = g1 / (sqrt(g2) + eps)
//     primitives.y = log_s = log1p(sqrt(g2) / eps)
// log1p(x/eps) is non-negative, monotone, sends 0 -> 0 (so the all-zero
// initial state of `packed` + `bounds` correctly decodes to sqrt_g2 = 0,
// matching the float-path zero-init invariant), and is approximately
// log(sqrt_g2 / eps) for sqrt_g2 >> eps. Its inverse is eps · expm1(log_s).
//
// Decode path:
//     u, log_s = linear-dequantize the two bytes within the per-block bounds
//     sqrt_g2  = eps · expm1(log_s)
//     g1       = u · (sqrt_g2 + eps)
//     g2       = sqrt_g2 · sqrt_g2
// Encode path is the same transformation reversed; the block-level min/max
// reduction inside the kernels operates on the linearly-quantized primitives
// (`u`, `log_s`), so the per-block bounds float4 means:
//     bounds.x, bounds.y = (u_min, u_max)             -- linear u domain
//     bounds.z, bounds.w = (log_s_min, log_s_max)     -- log-mapped sqrt(g2) domain
//
// Why log_s instead of plain sqrt(g2)? Empirically, the next-step
// Adam update u' = β1·g1 / sqrt(β2·g2 + ...) is dominated by errors in
// sqrt(g2) at small magnitudes -- exactly the region where linear quantization
// has the worst RELATIVE precision and where Adam's normalizer is most
// sensitive. Storing log1p(sqrt_g2 / eps) gives uniform relative resolution
// across all magnitudes within a block and consistently buys 20--30 dB on the
// post-step SNR metric at no storage cost.
//
// Storage is array-of-structure (AoS) interleaved over the two primitives:
//   BITS=16: 4 bytes/cell -- (uint16_t u_q) || (uint16_t log_s_q)
//   BITS=8:  2 bytes/cell -- byte[idx*2 + 0] = u_q, byte[idx*2 + 1] = log_s_q
//   BITS=4:  1 byte/cell  -- low nibble = u_q,    high nibble = log_s_q
// Quantization is endpoint-exact: q=0 -> lo, q=(kLevels-1) -> hi exactly.
//
// Hyperparameters (bit depth, block size, eps) are compile-time template
// arguments; all the math constants are constexpr so the codec compiles to
// straight-line arithmetic plus log1pf / expm1f (single CUDA math-lib calls).
// ============================================================================

// The finite test that survives every compiler this header sees; neither
// spelling of isfinite does (AGENTS.md gotchas). Mirrors _oq_finite in
// backend/vulkan/shaders/optim_quant.slang.
__device__ inline bool q_finite(float v) {
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    return (u & 0x7f800000u) != 0x7f800000u;
}

// Packed-SH cell addressing, shared by every reader of the two SH quant
// buffers and by the host decoders. Layouts: docs/notes/sh-quant-layout.md.
struct ShQuantAddr {
    int64_t bounds_stride;    // cells per bound
    int64_t pair_pitch;       // 0 = per-cell-block AoS, else cells per PAIR
    int64_t cells_per_splat;

    __device__ int64_t base(int64_t gid) const {
        return pair_pitch > 0
            ? (gid >> 8) * ((cells_per_splat + 1) >> 1) * 512 + (gid & 255) * 2
            : gid * cells_per_splat;
    }
    // Branch-free (this runs per cell): AoS folds to (c >> 0) * 1 + (c & 0).
    __device__ int64_t cell(int64_t base_cell, int c) const {
        int pair = (int)(pair_pitch > 0 ? 1 : 0);
        int64_t pitch = pair_pitch + (1 - pair);
        return base_cell + (int64_t)(c >> pair) * pitch + (int64_t)(c & pair);
    }
};

// `arg_stride` is the kernels' sh_bounds_stride argument: > 0 picks the
// per-cell-block AoS layout with that stride, 0 the FPBO transposed one.
__device__ inline ShQuantAddr sh_quant_addr(uint32_t num_sh_buffer,
                                            int64_t arg_stride) {
    const int64_t R = 3 * (int64_t)num_sh_buffer;
    if (arg_stride > 0) return ShQuantAddr{arg_stride, 0, R};
    return ShQuantAddr{((R + 1) >> 1) * 512, 512, R};
}

// Cells an FPBO-layout buffer needs for `n` splats: whole 256-splat blocks of
// whole words, so an odd cell count per splat leaves one cell unused.
inline int64_t sh_fpbo_cells(int64_t n, uint32_t num_sh_buffer) {
    const int64_t R = 3 * (int64_t)num_sh_buffer;
    return ((n + 255) / 256) * 256 * (((R + 1) >> 1) * 2);
}

template<int BITS, int BLOCK_SIZE = 256>
class QuantizedAdamState {
public:
    static_assert(BITS == 4 || BITS == 8 || BITS == 16,
                  "QuantizedAdamState: only 4-bit, 8-bit, and 16-bit are supported");
    static_assert(BLOCK_SIZE > 0 && (BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0,
                  "QuantizedAdamState: BLOCK_SIZE must be a positive power of 2");

    static constexpr int   kBits         = BITS;
    static constexpr int   kBlockSize    = BLOCK_SIZE;
    static constexpr int   kLevels       = 1 << BITS;
    static constexpr float kQMax         = (float)(kLevels - 1);
    static constexpr float kInvQMax      = 1.0f / kQMax;
    static constexpr int   kBytesPerCell = (BITS == 16) ? 4 : (BITS == 8) ? 2 : 1;
    static constexpr float kEps          = 1e-15f;   // matches Adam denominator

    // Storage (non-owning views backed by the global DevicePool).
    DeviceVector<uint8_t> packed;     // size = n_cells * kBytesPerCell
    DeviceVector<float4>  bounds;     // size = n_bounds (caller-chosen)
    int64_t n_cells  = 0;
    int64_t n_bounds = 0;

    // ---- Host: storage management ------------------------------------------

    void resize(PoolSlot slot, int64_t cells, int64_t bnds) {
        n_cells  = cells;
        n_bounds = bnds;
        packed.resize(slot, (size_t)(cells * kBytesPerCell), kSubPacked);
        bounds.resize(slot, (size_t)bnds,                    kSubBounds);
    }

    void zero() {
        if (packed.data_ptr()) packed.zero();
        if (bounds.data_ptr()) bounds.zero();
    }

    bool initialized() const { return packed.data_ptr() != nullptr; }

    uint8_t* packed_ptr() const { return packed.data_ptr(); }
    float4*  bounds_ptr() const { return bounds.data_ptr(); }

    int64_t packed_bytes() const { return n_cells * kBytesPerCell; }
    int64_t bounds_bytes() const { return n_bounds * (int64_t)sizeof(float4); }
    int64_t total_bytes()  const { return packed_bytes() + bounds_bytes(); }

    // ---- Codec primitives ---------------------------------------------------
    // Host-callable too: `__device__` is an empty macro in host translation
    // units, and this is pure <cmath> math. Host-side checkpoint adaptation
    // (checkpoint/Adapt.cpp) decodes and re-encodes through these very
    // functions, so there is no second implementation to drift.
    // Forward / inverse map for the sqrt_g2 half:
    //   forward:  sqrt_g2 -> log1pf(sqrt_g2 / eps)
    //   inverse:  log_s   -> eps * expm1f(log_s)
    // Both single CUDA math-lib calls; preserve the 0 <-> 0 fixed point so
    // the all-zero initial state of `packed` + `bounds` correctly decodes to
    // sqrt_g2 = 0 (no special init needed).
    __device__ static inline float forward_sqrt_g2(float sqrt_g2) {
        return log1pf(fmaxf(sqrt_g2, 0.0f) * (1.0f / kEps));
    }
    __device__ static inline float inverse_sqrt_g2(float log_s) {
        return kEps * expm1f(log_s);
    }

    // Decode the linearly-quantized primitives (u, log_s) at cell `idx`.
    // Note: returns the INTERNAL representation, not (u, sqrt_g2). Use
    // decode_g1g2 for the reconstructed Adam state.
    __device__ static inline float2 decode_us(
        const uint8_t* __restrict__ packed_ptr, int64_t idx, float4 mm
    ) {
        float u_q, s_q;
        if constexpr (BITS == 16) {
            const uint16_t* p = reinterpret_cast<const uint16_t*>(packed_ptr);
            u_q = (float)p[idx * 2 + 0];
            s_q = (float)p[idx * 2 + 1];
        } else if constexpr (BITS == 8) {
            u_q = (float)packed_ptr[idx * 2 + 0];
            s_q = (float)packed_ptr[idx * 2 + 1];
        } else {  // BITS == 4
            uint8_t b = packed_ptr[idx];
            u_q = (float)(b & 0x0Fu);
            s_q = (float)((b >> 4) & 0x0Fu);
        }
        // A non-finite bound would decode every one of the block's 256 cells
        // to NaN, which no later step can undo. Decode 0 instead: the block's
        // Adam state restarts rather than staying poisoned.
        if (!q_finite(mm.x) || !q_finite(mm.y)) { mm.x = 0.0f; mm.y = 0.0f; }
        if (!q_finite(mm.z) || !q_finite(mm.w)) { mm.z = 0.0f; mm.w = 0.0f; }
        return float2{
            mm.x + (mm.y - mm.x) * (u_q * kInvQMax),
            mm.z + (mm.w - mm.z) * (s_q * kInvQMax)
        };
    }

    // Decode + reconstruct Adam state (g1, g2) from the cell at `idx`.
    __device__ static inline float2 decode_g1g2(
        const uint8_t* __restrict__ packed_ptr, int64_t idx, float4 mm
    ) {
        float2 prim = decode_us(packed_ptr, idx, mm);
        float sqrt_g2 = inverse_sqrt_g2(prim.y);
        return float2{
            prim.x * (sqrt_g2 + kEps),   // g1 = u * (sqrt_g2 + eps)
            sqrt_g2 * sqrt_g2             // g2 = sqrt_g2 * sqrt_g2
        };
    }

    // Encode the linearly-quantized primitives (u, log_s) into the packed
    // byte stream at cell `idx`. Endpoint-exact 8-bit (or 4-bit) within mm.
    // Kernels typically obtain (u_val, log_s_val) by calling g1g2_to_us and
    // pass mm = the block-reduced bounds.
    __device__ static inline void encode_us(
        uint8_t* __restrict__ packed_ptr, int64_t idx,
        float u_val, float log_s_val, float4 mm
    ) {
        float u_range = fmaxf(mm.y - mm.x, kEps);
        float s_range = fmaxf(mm.w - mm.z, kEps);
        float u_qf = fminf(fmaxf(roundf(kQMax * (u_val     - mm.x) / u_range), 0.0f), kQMax);
        float s_qf = fminf(fmaxf(roundf(kQMax * (log_s_val - mm.z) / s_range), 0.0f), kQMax);
        if constexpr (BITS == 16) {
            uint16_t* p = reinterpret_cast<uint16_t*>(packed_ptr);
            p[idx * 2 + 0] = (uint16_t)u_qf;
            p[idx * 2 + 1] = (uint16_t)s_qf;
        } else if constexpr (BITS == 8) {
            packed_ptr[idx * 2 + 0] = (uint8_t)u_qf;
            packed_ptr[idx * 2 + 1] = (uint8_t)s_qf;
        } else {  // BITS == 4
            uint8_t u_q = (uint8_t)u_qf;
            uint8_t s_q = (uint8_t)s_qf;
            packed_ptr[idx] = (uint8_t)((u_q & 0x0Fu) | ((s_q & 0x0Fu) << 4));
        }
    }

    // Encode (g1, g2) -- internally derives (u, log_s) via g1g2_to_us.
    __device__ static inline void encode_g1g2(
        uint8_t* __restrict__ packed_ptr, int64_t idx,
        float g1, float g2, float4 mm
    ) {
        float2 prim = g1g2_to_us(g1, g2);
        encode_us(packed_ptr, idx, prim.x, prim.y, mm);
    }

    // BITS == 8 only: one cell's packed byte pair (u_q | s_q << 8) as a
    // value, for callers that assemble whole words themselves (the FPBO
    // kernel's staged coalesced writeback). Same rounding as encode_us.
    __device__ static inline uint32_t encode_us_code(
        float u_val, float log_s_val, float4 mm
    ) {
        static_assert(BITS == 8, "cell-code encode is the 2-byte/cell AoS");
        float u_range = fmaxf(mm.y - mm.x, kEps);
        float s_range = fmaxf(mm.w - mm.z, kEps);
        float u_qf = fminf(fmaxf(roundf(kQMax * (u_val - mm.x) / u_range), 0.0f), kQMax);
        float s_qf = fminf(fmaxf(roundf(kQMax * (log_s_val - mm.z) / s_range), 0.0f), kQMax);
        return (uint32_t)(uint8_t)u_qf | ((uint32_t)(uint8_t)s_qf << 8);
    }

    __device__ static inline uint32_t encode_g1g2_code(
        float g1, float g2, float4 mm
    ) {
        float2 prim = g1g2_to_us(g1, g2);
        return encode_us_code(prim.x, prim.y, mm);
    }

    // (g1, g2) -> linearly-quantized primitives (u, log_s). Block reduction
    // inside host kernels operates on these -- both halves are min/max-reduced
    // in their own (linear) domain to produce the per-block float4 bounds.
    __device__ static inline float2 g1g2_to_us(float g1, float g2) {
        float sqrt_g2 = sqrtf(fmaxf(g2, 0.0f));
        float u       = g1 / (sqrt_g2 + kEps);
        return float2{u, forward_sqrt_g2(sqrt_g2)};
    }
};


// ============================================================================
// QuantizedTensorLog -- generic block-wise log-encoded quantized tensor.
//
// Stores a single non-negative scalar per cell as q ~ log1pf(v / eps), with
// per-block (min, max) float2 bounds in the log domain. Endpoint-exact within
// each block: q=0 -> decoded=lo, q=(kLevels-1) -> decoded=hi. Preserves the
// 0 <-> 0 fixed point so an all-zero (`packed` + `bounds`) initial state
// decodes to v = 0 exactly -- matches the "initial_accumulator_value=0" we use
// for AdaGrad accumulators and saves a separate init pass.
//
// Not Adam-specific. Anything that wants relative-precision storage for one
// non-negative value per cell (AdaGrad accum, running variance, occupancy
// likelihood, ...) can reuse it. The bounds float2 always means
// (log-domain min, log-domain max); the codec lives entirely in this class
// and a caller need only:
//     decode_v / encode_v       -- per-cell read / write
//     forward_v / inverse_v     -- to do their own block-reduction in the
//                                  log domain before writing bounds back
//
// Storage (AoS -- same per-cell stride conventions as QuantizedAdamState):
//   BITS=8: 1 byte/cell -- packed[idx] = q
//   BITS=4: 1 byte/2-cells -- low nibble = idx2k, high nibble = idx2k+1
// ============================================================================

template<int BITS, int BLOCK_SIZE = 256>
class QuantizedTensorLog {
public:
    static_assert(BITS == 4 || BITS == 8,
                  "QuantizedTensorLog: only 4-bit and 8-bit are supported");
    static_assert(BLOCK_SIZE > 0 && (BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0,
                  "QuantizedTensorLog: BLOCK_SIZE must be a positive power of 2");

    static constexpr int   kBits         = BITS;
    static constexpr int   kBlockSize    = BLOCK_SIZE;
    static constexpr int   kLevels       = 1 << BITS;
    static constexpr float kQMax         = (float)(kLevels - 1);
    static constexpr float kInvQMax      = 1.0f / kQMax;
    // 8-bit: 1 byte per cell. 4-bit: 0.5 byte per cell (2 cells share a byte).
    // packed_bytes() rounds up to the nearest byte for 4-bit.
    static constexpr float kEps          = 1e-15f;

    // Storage (non-owning views backed by the global DevicePool).
    DeviceVector<uint8_t> packed;     // size = packed_bytes()
    DeviceVector<float2>  bounds;     // size = n_bounds (caller-chosen)
    int64_t n_cells  = 0;
    int64_t n_bounds = 0;

    // ---- Host: storage management ------------------------------------------

    void resize(PoolSlot slot, int64_t cells, int64_t bnds) {
        n_cells  = cells;
        n_bounds = bnds;
        packed.resize(slot, (size_t)packed_bytes_for(cells), kSubPacked);
        bounds.resize(slot, (size_t)bnds,                    kSubBounds);
    }

    void zero() {
        if (packed.data_ptr()) packed.zero();
        if (bounds.data_ptr()) bounds.zero();
    }

    bool initialized() const { return packed.data_ptr() != nullptr; }

    uint8_t* packed_ptr() const { return packed.data_ptr(); }
    float2*  bounds_ptr() const { return bounds.data_ptr(); }

    static int64_t packed_bytes_for(int64_t cells) {
        if constexpr (BITS == 8) return cells;
        else                     return (cells + 1) / 2;   // 4-bit, AoS
    }
    int64_t packed_bytes() const { return packed_bytes_for(n_cells); }
    int64_t bounds_bytes() const { return n_bounds * (int64_t)sizeof(float2); }
    int64_t total_bytes()  const { return packed_bytes() + bounds_bytes(); }

    // ---- Codec primitives ---------------------------------------------------
    // Host-callable too: `__device__` is an empty macro in host translation
    // units, and this is pure <cmath> math. Host-side checkpoint adaptation
    // (checkpoint/Adapt.cpp) decodes and re-encodes through these very
    // functions, so there is no second implementation to drift.
    // Forward / inverse log map. 0 <-> 0 fixed point.
    __device__ static inline float forward_v(float v) {
        return log1pf(fmaxf(v, 0.0f) * (1.0f / kEps));
    }
    __device__ static inline float inverse_v(float log_v) {
        return kEps * expm1f(log_v);
    }

    // Decode the linearly-quantized log-primitive at cell `idx`. Returns
    // the value in the LOG domain -- use inverse_v(...) to get back to v.
    __device__ static inline float decode_log(
        const uint8_t* __restrict__ packed_ptr, int64_t idx, float2 mm
    ) {
        float q;
        if constexpr (BITS == 8) {
            q = (float)packed_ptr[idx];
        } else {  // BITS == 4
            uint8_t b = packed_ptr[idx >> 1];
            q = (idx & 1) ? (float)((b >> 4) & 0x0Fu) : (float)(b & 0x0Fu);
        }
        return mm.x + (mm.y - mm.x) * (q * kInvQMax);
    }

    // Decode + reconstruct v from the cell at `idx`.
    __device__ static inline float decode_v(
        const uint8_t* __restrict__ packed_ptr, int64_t idx, float2 mm
    ) {
        return inverse_v(decode_log(packed_ptr, idx, mm));
    }

    // Encode a log-domain value into the packed byte stream at cell `idx`.
    // Endpoint-exact within mm. Callers typically obtain `log_v` via
    // forward_v(v) and pass mm = the block-reduced log-domain bounds.
    //
    // NOTE: BITS==4 stores two cells per byte. Concurrent threads writing
    // odd/even partners of the same byte will race. The bilagrid kernels
    // serialize this with a __syncthreads() before the bytewise store --
    // see fused_bilagrid_tv_adagrad_kernel for the pattern.
    __device__ static inline void encode_log(
        uint8_t* __restrict__ packed_ptr, int64_t idx,
        float log_v, float2 mm
    ) {
        float range = fmaxf(mm.y - mm.x, kEps);
        uint8_t q = (uint8_t)fminf(
            fmaxf(roundf(kQMax * (log_v - mm.x) / range), 0.0f), kQMax);
        if constexpr (BITS == 8) {
            packed_ptr[idx] = q;
        } else {  // BITS == 4
            // 4-bit AoS: two cells per byte (idx&1 picks low/high nibble).
            // Caller-synchronized -- see note above.
            int64_t byte_idx = idx >> 1;
            uint8_t old = packed_ptr[byte_idx];
            uint8_t merged = (idx & 1)
                ? (uint8_t)((old & 0x0Fu) | ((q & 0x0Fu) << 4))
                : (uint8_t)((old & 0xF0u) | (q & 0x0Fu));
            packed_ptr[byte_idx] = merged;
        }
    }

    // Encode v directly -- internally calls forward_v.
    __device__ static inline void encode_v(
        uint8_t* __restrict__ packed_ptr, int64_t idx,
        float v, float2 mm
    ) {
        encode_log(packed_ptr, idx, forward_v(v), mm);
    }
};


// ============================================================================
// QuantizedTensor -- generic block-wise UNIFORM (linear) quantized tensor for a
// single scalar value per cell. Sibling of QuantizedTensorLog, but with linear
// (not log) quantization in the original value domain.
//
// Block-wise (min, max) float2 bounds per BLOCK_SIZE cells. Endpoint-exact
// within each block: q=0 -> decoded=lo, q=(kLevels-1) -> decoded=hi.
//
// Targets value (parameter) quantization where the value range is naturally
// bounded and small, so a linear codec captures it well without log-distortion
// of the precision-versus-magnitude trade-off. The codec is the building block
// for future SH-feature / bilagrid-grid value quantization (see "dequantize on
// load" plan): every consumer kernel takes BITS as a template parameter and
// calls decode_v on each read; the encoder writes once after the optimizer
// step alongside the per-block bound reduction.
//
// Storage (AoS):
//   BITS=8:  1 byte/cell  -- packed[idx]      = q
//   BITS=16: 2 bytes/cell -- packed16[idx]    = q   (reinterpret to uint16_t*)
//
// 16-bit is the high-precision option; 8-bit is the compact option. We do not
// support 4-bit here -- linear quantization at 4-bit is too coarse for the
// dynamic range of typical parameter values (use the log codec for AdaGrad-
// style accumulators where 4-bit pays off with log-encoding).
// ============================================================================

template<int BITS, int BLOCK_SIZE = 256>
class QuantizedTensor {
public:
    static_assert(BITS == 8 || BITS == 16,
                  "QuantizedTensor: only 8-bit and 16-bit are supported "
                  "(use QuantizedTensorLog for 4-bit log-domain storage)");
    static_assert(BLOCK_SIZE > 0 && (BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0,
                  "QuantizedTensor: BLOCK_SIZE must be a positive power of 2");

    static constexpr int   kBits         = BITS;
    static constexpr int   kBlockSize    = BLOCK_SIZE;
    static constexpr int   kLevels       = 1 << BITS;
    static constexpr float kQMax         = (float)(kLevels - 1);
    static constexpr float kInvQMax      = 1.0f / kQMax;
    static constexpr int   kBytesPerCell = (BITS == 16) ? 2 : 1;

    // Storage (non-owning views backed by the global DevicePool).
    DeviceVector<uint8_t> packed;     // size = n_cells * kBytesPerCell
    DeviceVector<float2>  bounds;     // size = n_bounds (caller-chosen)
    int64_t n_cells  = 0;
    int64_t n_bounds = 0;

    // ---- Host: storage management ------------------------------------------

    void resize(PoolSlot slot, int64_t cells, int64_t bnds) {
        n_cells  = cells;
        n_bounds = bnds;
        packed.resize(slot, (size_t)(cells * kBytesPerCell), kSubPacked);
        bounds.resize(slot, (size_t)bnds,                    kSubBounds);
    }

    void zero() {
        if (packed.data_ptr()) packed.zero();
        if (bounds.data_ptr()) bounds.zero();
    }

    bool initialized() const { return packed.data_ptr() != nullptr; }

    uint8_t* packed_ptr() const { return packed.data_ptr(); }
    float2*  bounds_ptr() const { return bounds.data_ptr(); }

    int64_t packed_bytes() const { return n_cells * (int64_t)kBytesPerCell; }
    int64_t bounds_bytes() const { return n_bounds * (int64_t)sizeof(float2); }
    int64_t total_bytes()  const { return packed_bytes() + bounds_bytes(); }

    // ---- Codec primitives ---------------------------------------------------
    // Host-callable too: `__device__` is an empty macro in host translation
    // units, and this is pure <cmath> math. Host-side checkpoint adaptation
    // (checkpoint/Adapt.cpp) decodes and re-encodes through these very
    // functions, so there is no second implementation to drift.
    // Decode the linearly-quantized value at cell `idx` directly (no log).
    __device__ static inline float decode_v(
        const uint8_t* __restrict__ packed_ptr, int64_t idx, float2 mm
    ) {
        float q;
        if constexpr (BITS == 8) {
            q = (float)packed_ptr[idx];
        } else {  // BITS == 16
            const uint16_t* p16 = reinterpret_cast<const uint16_t*>(packed_ptr);
            q = (float)p16[idx];
        }
        return mm.x + (mm.y - mm.x) * (q * kInvQMax);
    }

    // Encode v into the packed byte stream at cell `idx`. Endpoint-exact
    // within (mm.x, mm.y). Callers pass mm = the block-reduced bounds.
    __device__ static inline void encode_v(
        uint8_t* __restrict__ packed_ptr, int64_t idx,
        float v, float2 mm
    ) {
        float range = fmaxf(mm.y - mm.x, 1e-30f);
        float qf = roundf(kQMax * (v - mm.x) / range);
        qf = fminf(fmaxf(qf, 0.0f), kQMax);
        if constexpr (BITS == 8) {
            packed_ptr[idx] = (uint8_t)qf;
        } else {  // BITS == 16
            uint16_t* p16 = reinterpret_cast<uint16_t*>(packed_ptr);
            p16[idx] = (uint16_t)qf;
        }
    }

    // One cell's quantized code as a value (low BITS bits), for callers
    // that assemble whole words themselves (the FPBO kernel's staged
    // coalesced writeback). Same rounding as encode_v.
    __device__ static inline uint32_t encode_v_code(float v, float2 mm) {
        float range = fmaxf(mm.y - mm.x, 1e-30f);
        float qf = roundf(kQMax * (v - mm.x) / range);
        qf = fminf(fmaxf(qf, 0.0f), kQMax);
        if constexpr (BITS == 8) return (uint32_t)(uint8_t)qf;
        else                     return (uint32_t)(uint16_t)qf;
    }
};
