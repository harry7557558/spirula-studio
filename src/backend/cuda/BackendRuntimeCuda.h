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
    void* ptr = nullptr;
    cudaMalloc(&ptr, bytes);
    if (ptr) {
        std::lock_guard<std::mutex> lock(detail::alloc_mutex());
        detail::alloc_sizes()[ptr] = bytes;
        detail::device_bytes().fetch_add(bytes, std::memory_order_relaxed);
    }
    return ptr;
}
inline void device_free(void* ptr) {
    if (ptr) {
        std::lock_guard<std::mutex> lock(detail::alloc_mutex());
        auto& sizes = detail::alloc_sizes();
        auto it = sizes.find(ptr);
        if (it != sizes.end()) {
            detail::device_bytes().fetch_sub(it->second, std::memory_order_relaxed);
            sizes.erase(it);
        }
    }
    cudaFree(ptr);
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
