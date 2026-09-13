#pragma once
// Inline cudart implementation of the backend runtime (the default backend).
// Included by BackendRuntime.h — do not include directly. Calls are
// unchecked, matching the raw cuda* call sites this layer replaced.

#include <cuda_runtime.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#include "backend/common/Profiler.h"

namespace backend {

// --- process VRAM accounting ---
// The CUDA runtime has no per-process memory query (only cudaMemGetInfo, which
// is device-wide). We track the live byte total of device_malloc allocations
// ourselves; device_free takes only a pointer, so a side table remembers each
// allocation's size. Inline-function statics are one-per-program, so the
// counter/table are shared across all translation units. Allocation is pooled
// upstream (Tensor.h), so this map is touched rarely, not per frame.
namespace detail {
inline std::mutex& alloc_mutex() {
    static std::mutex m;
    return m;
}
inline std::unordered_map<void*, size_t>& alloc_sizes() {
    static std::unordered_map<void*, size_t> m;
    return m;
}
inline std::atomic<uint64_t>& device_bytes() {
    static std::atomic<uint64_t> v{0};
    return v;
}
inline std::mutex& budget_mutex() {
    static std::mutex m;
    return m;
}
inline std::mutex& budget_transaction_mutex() {
    static std::mutex m;
    return m;
}
inline std::atomic<bool>& budget_active() {
    static std::atomic<bool> a{false};
    return a;
}
inline std::atomic<uint64_t>& budget_reserve_bytes() {
    static std::atomic<uint64_t> r{0};
    return r;
}
inline std::atomic<uint64_t>& budget_app_limit_bytes() {
    static std::atomic<uint64_t> l{0};
    return l;
}
inline std::atomic<uint64_t>& budget_reserved_bytes() {
    static std::atomic<uint64_t> rb{0};
    return rb;
}

struct ThreadPendingFailure {
    bool has_failure = false;
    BudgetFailure failure{};
};

inline ThreadPendingFailure& thread_pending_failure() {
    thread_local ThreadPendingFailure f;
    return f;
}

inline void set_pending_failure(const BudgetFailure& f) {
    auto& state = thread_pending_failure();
    state.has_failure = true;
    state.failure = f;
}

inline void clear_pending_failure() {
    thread_pending_failure().has_failure = false;
}

inline bool consume_pending_failure(BudgetFailure& out) {
    auto& state = thread_pending_failure();
    if (state.has_failure) {
        out = state.failure;
        state.has_failure = false;
        return true;
    }
    return false;
}
}  // namespace detail

// --- device enumeration / selection ---
inline int device_count() {
    int n = 0;
    if (cudaGetDeviceCount(&n) != cudaSuccess) {
        cudaGetLastError();
        return 0;
    }
    return n;
}
inline DeviceInfo device_info(int index) {
    DeviceInfo info{};
    info.type = "other";
    cudaDeviceProp prop{};
    if (index < 0 || cudaGetDeviceProperties(&prop, index) != cudaSuccess) {
        cudaGetLastError();
        return info;
    }
    std::snprintf(info.name, sizeof(info.name), "%s", prop.name);
    info.type = prop.integrated ? "integrated" : "discrete";
    info.vram_bytes = (uint64_t)prop.totalGlobalMem;
    info.usable = true;
    return info;
}
inline bool device_select(int index) {
    if (index < 0 || index >= device_count()) return false;
    if (cudaSetDevice(index) != cudaSuccess) {
        cudaGetLastError();
        return false;
    }
    return true;
}
inline int device_current() {
    int d = -1;
    if (cudaGetDevice(&d) != cudaSuccess) {
        cudaGetLastError();
        return -1;
    }
    return d;
}
inline bool device_prepare() { return device_current() >= 0; }
inline MemoryUsage memory_usage() {
    MemoryUsage m;
    size_t free_bytes = 0, total_bytes = 0;
    if (cudaMemGetInfo(&free_bytes, &total_bytes) == cudaSuccess &&
        total_bytes > 0) {
        m.total_bytes = (uint64_t)total_bytes;
        m.used_bytes = (uint64_t)(total_bytes - free_bytes);
        m.has_total = true;
        m.has_used = true;
    } else {
        cudaGetLastError();
    }
    m.process_bytes = detail::device_bytes().load(std::memory_order_relaxed);
    m.has_process = true;
    return m;
}

inline BudgetSnapshot budget_snapshot() {
    std::lock_guard<std::mutex> transaction(detail::budget_transaction_mutex());
    BudgetSnapshot s{};
    s.heap_index = -1;
    {
        std::lock_guard<std::mutex> lock(detail::budget_mutex());
        s.reserve_bytes =
            detail::budget_reserve_bytes().load(std::memory_order_relaxed);
        s.app_limit_bytes =
            detail::budget_app_limit_bytes().load(std::memory_order_relaxed);
        s.reserved_bytes =
            detail::budget_reserved_bytes().load(std::memory_order_relaxed);
        s.process_bytes = detail::device_bytes().load(std::memory_order_relaxed);
    }

    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) {
        s.status = err == cudaErrorNoDevice ? BudgetStatus::Unavailable
                                            : BudgetStatus::QueryError;
        return s;
    }
    if (count <= 0) {
        s.status = BudgetStatus::Unavailable;
        return s;
    }

    int dev = -1;
    err = cudaGetDevice(&dev);
    if (err != cudaSuccess) {
        s.status = BudgetStatus::QueryError;
        return s;
    }
    if (dev < 0) {
        s.status = BudgetStatus::Unavailable;
        return s;
    }
    s.device_index = dev;

    size_t free_bytes = 0, total_bytes = 0;
    if (cudaMemGetInfo(&free_bytes, &total_bytes) != cudaSuccess) {
        s.status = BudgetStatus::QueryError;
        return s;
    }

    s.status = BudgetStatus::Available;
    s.total_bytes = (uint64_t)total_bytes;
    s.available_bytes = (uint64_t)free_bytes;
    return s;
}

inline void training_budget_begin(uint64_t reserve_bytes,
                                  uint64_t app_limit_bytes) {
    std::lock_guard<std::mutex> transaction(detail::budget_transaction_mutex());
    std::lock_guard<std::mutex> lock(detail::budget_mutex());
    detail::budget_reserve_bytes().store(reserve_bytes, std::memory_order_relaxed);
    detail::budget_app_limit_bytes().store(app_limit_bytes,
                                           std::memory_order_relaxed);
    detail::budget_reserved_bytes().store(0, std::memory_order_relaxed);
    detail::budget_active().store(true, std::memory_order_release);
}

inline void training_budget_end() {
    std::lock_guard<std::mutex> transaction(detail::budget_transaction_mutex());
    std::lock_guard<std::mutex> lock(detail::budget_mutex());
    detail::budget_active().store(false, std::memory_order_release);
    detail::budget_reserve_bytes().store(0, std::memory_order_relaxed);
    detail::budget_app_limit_bytes().store(0, std::memory_order_relaxed);
    detail::budget_reserved_bytes().store(0, std::memory_order_relaxed);
}

inline bool training_budget_active() {
    return detail::budget_active().load(std::memory_order_acquire);
}

inline bool consume_budget_failure(BudgetFailure& failure) {
    return detail::consume_pending_failure(failure);
}

inline cudaMemcpyKind _to_cuda(MemcpyKind kind) {
    switch (kind) {
        case MemcpyKind::HostToDevice:   return cudaMemcpyHostToDevice;
        case MemcpyKind::DeviceToHost:   return cudaMemcpyDeviceToHost;
        case MemcpyKind::DeviceToDevice: return cudaMemcpyDeviceToDevice;
        default:                         return cudaMemcpyDefault;
    }
}

inline cudaStream_t _to_cuda(Stream stream) {
    return stream;  // backend::Stream IS cudaStream_t under this backend
}

inline const char* last_error() {
    cudaError_t err = cudaGetLastError();
    return err == cudaSuccess ? nullptr : cudaGetErrorString(err);
}

inline bool is_device_pointer(const void* ptr) {
    if (ptr == nullptr) return false;
    cudaPointerAttributes attr{};
    cudaError_t err = cudaPointerGetAttributes(&attr, ptr);
    if (err != cudaSuccess) {
        // Pageable host pointer is unregistered -> reset error, treat as host.
        cudaGetLastError();
        return false;
    }
    return attr.type == cudaMemoryTypeDevice || attr.type == cudaMemoryTypeManaged;
}

// --- memory ---
inline void* device_malloc(size_t bytes) {
    detail::clear_pending_failure();
    if (bytes == 0) return nullptr;

    std::lock_guard<std::mutex> transaction(
        detail::budget_transaction_mutex());
    if (!training_budget_active()) {
        void* ptr = nullptr;
        cudaError_t alloc_err = cudaMalloc(&ptr, bytes);
        if (ptr && alloc_err == cudaSuccess) {
            std::lock_guard<std::mutex> lock(detail::alloc_mutex());
            detail::alloc_sizes()[ptr] = bytes;
            detail::device_bytes().fetch_add(bytes, std::memory_order_relaxed);
        }
        return ptr;
    }

    auto refuse = [&](BudgetFailureKind kind, uint64_t available,
                      int device = -1) -> void* {
        BudgetFailure f{};
        f.kind = kind;
        f.requested_bytes = bytes;
        f.available_bytes = available;
        f.device_index = device;
        f.heap_index = -1;
        detail::set_pending_failure(f);
        return nullptr;
    };

    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess)
        return refuse(err == cudaErrorNoDevice
                          ? BudgetFailureKind::TelemetryUnavailable
                          : BudgetFailureKind::TelemetryError,
                      0);
    if (count <= 0)
        return refuse(BudgetFailureKind::TelemetryUnavailable, 0);

    int dev = -1;
    err = cudaGetDevice(&dev);
    if (err != cudaSuccess)
        return refuse(BudgetFailureKind::TelemetryError, 0);
    if (dev < 0)
        return refuse(BudgetFailureKind::TelemetryUnavailable, 0);

    size_t free_bytes = 0, total_bytes = 0;
    if (cudaMemGetInfo(&free_bytes, &total_bytes) != cudaSuccess)
        return refuse(BudgetFailureKind::TelemetryError, 0, dev);

    {
        std::lock_guard<std::mutex> lock(detail::budget_mutex());
        uint64_t headroom = (uint64_t)free_bytes;
        uint64_t reserve =
            detail::budget_reserve_bytes().load(std::memory_order_relaxed);
        uint64_t reserved =
            detail::budget_reserved_bytes().load(std::memory_order_relaxed);
        headroom = headroom > reserve ? headroom - reserve : 0;
        headroom = headroom > reserved ? headroom - reserved : 0;

        uint64_t app_limit =
            detail::budget_app_limit_bytes().load(std::memory_order_relaxed);
        uint64_t app_headroom = UINT64_MAX;
        if (app_limit > 0) {
            uint64_t process =
                detail::device_bytes().load(std::memory_order_relaxed);
            app_headroom = app_limit > process ? app_limit - process : 0;
            app_headroom =
                app_headroom > reserved ? app_headroom - reserved : 0;
        }

        if (app_limit > 0 && bytes > app_headroom &&
            app_headroom <= headroom)
            return refuse(BudgetFailureKind::ApplicationLimit, app_headroom,
                          dev);
        if (bytes > headroom)
            return refuse(BudgetFailureKind::DriverHeadroom, headroom, dev);
        if (app_limit > 0 && bytes > app_headroom)
            return refuse(BudgetFailureKind::ApplicationLimit, app_headroom,
                          dev);
        detail::budget_reserved_bytes().fetch_add(bytes,
                                                   std::memory_order_relaxed);
    }

    void* ptr = nullptr;
    cudaError_t alloc_err = cudaMalloc(&ptr, bytes);
    {
        std::lock_guard<std::mutex> budget_lock(detail::budget_mutex());
        detail::budget_reserved_bytes().fetch_sub(bytes,
                                                   std::memory_order_relaxed);
        if (ptr != nullptr && alloc_err == cudaSuccess) {
            std::lock_guard<std::mutex> alloc_lock(detail::alloc_mutex());
            detail::alloc_sizes()[ptr] = bytes;
            detail::device_bytes().fetch_add(bytes, std::memory_order_relaxed);
            return ptr;
        }
    }
    return nullptr;
}

inline void device_free(void* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> transaction(
        detail::budget_transaction_mutex());
    size_t bytes = 0;
    {
        std::lock_guard<std::mutex> lock(detail::alloc_mutex());
        auto it = detail::alloc_sizes().find(ptr);
        if (it != detail::alloc_sizes().end()) bytes = it->second;
    }
    if (cudaFree(ptr) != cudaSuccess || bytes == 0) return;
    {
        std::lock_guard<std::mutex> lock(detail::alloc_mutex());
        detail::alloc_sizes().erase(ptr);
    }
    detail::device_bytes().fetch_sub(bytes, std::memory_order_relaxed);
}
inline void* host_malloc_pinned(size_t bytes) {
    void* ptr = nullptr;
    cudaMallocHost(&ptr, bytes);
    return ptr;
}
inline void host_free_pinned(void* ptr) { cudaFreeHost(ptr); }

inline void memcpy_sync(void* dst, const void* src, size_t bytes, MemcpyKind kind) {
    if (prof::enabled() && bytes) {
        prof::Cat c = prof::kind_cat(kind);
        if (kind == MemcpyKind::Auto) {
            bool dd = is_device_pointer(dst), sd = is_device_pointer(src);
            c = dd && sd ? prof::D2D : dd ? prof::H2D : prof::D2H;
        }
        prof::drain_before_copy();  // pending GPU work -> DEVSYNC bucket
        prof::Scope s(c, bytes);    // copy bucket then measures pure transfer
        cudaMemcpy(dst, src, bytes, _to_cuda(kind));
        return;
    }
    cudaMemcpy(dst, src, bytes, _to_cuda(kind));
}
inline void memcpy_async(void* dst, const void* src, size_t bytes, MemcpyKind kind,
                         Stream stream) {
    if (prof::enabled() && bytes) {
        prof::Cat c = prof::kind_cat(kind);
        if (kind == MemcpyKind::Auto) {
            bool dd = is_device_pointer(dst), sd = is_device_pointer(src);
            c = dd && sd ? prof::D2D : dd ? prof::H2D : prof::D2H;
        }
        prof::Scope s(c, bytes);  // enqueue cost; drain lands in a later sync
        cudaMemcpyAsync(dst, src, bytes, _to_cuda(kind), _to_cuda(stream));
        return;
    }
    cudaMemcpyAsync(dst, src, bytes, _to_cuda(kind), _to_cuda(stream));
}
inline void memset_sync(void* dst, int value, size_t bytes) {
    if (prof::enabled() && bytes) {
        prof::Scope s(prof::MEMSET, bytes);
        cudaMemset(dst, value, bytes);
        return;
    }
    cudaMemset(dst, value, bytes);
}
inline void memset_async(void* dst, int value, size_t bytes, Stream stream) {
    if (prof::enabled() && bytes) {
        prof::Scope s(prof::MEMSET, bytes);
        cudaMemsetAsync(dst, value, bytes, _to_cuda(stream));
        return;
    }
    cudaMemsetAsync(dst, value, bytes, _to_cuda(stream));
}

// --- synchronization ---
inline void device_synchronize() {
    if (prof::enabled()) {
        { prof::Scope s(prof::DEVSYNC); cudaDeviceSynchronize(); }
        if (prof::g_kernel_resolve) prof::g_kernel_resolve();
        return;
    }
    cudaDeviceSynchronize();
}
inline void stream_synchronize(Stream stream) {
    if (prof::enabled()) {
        { prof::Scope s(prof::DEVSYNC); cudaStreamSynchronize(_to_cuda(stream)); }
        if (prof::g_kernel_resolve) prof::g_kernel_resolve();
        return;
    }
    cudaStreamSynchronize(_to_cuda(stream));
}

// --- events ---
inline Event* event_create(bool enable_timing) {
    cudaEvent_t event;
    cudaEventCreateWithFlags(
        &event, enable_timing ? cudaEventDefault : cudaEventDisableTiming);
    return reinterpret_cast<Event*>(event);
}
inline void event_record(Event* event, Stream stream) {
    cudaEventRecord(reinterpret_cast<cudaEvent_t>(event), _to_cuda(stream));
}
inline void event_synchronize(Event* event) {
    if (prof::enabled()) {
        prof::Scope s(prof::DEVSYNC);
        cudaEventSynchronize(reinterpret_cast<cudaEvent_t>(event));
        return;
    }
    cudaEventSynchronize(reinterpret_cast<cudaEvent_t>(event));
}
inline void event_destroy(Event* event) {
    cudaEventDestroy(reinterpret_cast<cudaEvent_t>(event));
}
inline float event_elapsed_ms(Event* start, Event* end) {
    float ms = 0.0f;
    cudaEventElapsedTime(&ms, reinterpret_cast<cudaEvent_t>(start),
                         reinterpret_cast<cudaEvent_t>(end));
    return ms;
}

}  // namespace backend
