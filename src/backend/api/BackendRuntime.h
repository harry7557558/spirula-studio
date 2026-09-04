#pragma once
// Backend-neutral device runtime, covering exactly the CUDA runtime surface
// the engine layer and Tensor.h use today (audited 2026-07: cudaMemcpy[Async],
// cudaMemset[Async], cudaMalloc/Free, cudaMallocHost/FreeHost, cudaEvent*,
// cudaDeviceSynchronize, streams). Engine .cpp files call these instead of
// cuda* directly; the ~54 existing call sites migrate mechanically.
//
// CUDA backend (the default; anything except -DSS_BACKEND_VULKAN):
// inline wrappers over cudart (zero-cost), included at the bottom of this
// header. Vulkan backend: suballocating pool + staging-buffer copies + fence
// waits (VkSplat model, see backend/README.md).
//
// Device memory is addressed by plain pointers. On Vulkan this presumes
// VK_KHR_buffer_device_address (core 1.2, supported by MoltenVK) so that a
// 64-bit device address behaves pointer-like end to end, including inside
// Slang kernels. The fallback — opaque buffer handle + offset — would ripple
// through Tensor.h and is documented as a rejected-unless-forced alternative.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

struct CUstream_st;

namespace backend {

enum class MemcpyKind {
    HostToDevice,
    DeviceToHost,
    DeviceToDevice,
    Auto,  // cudaMemcpyDefault; Vulkan resolves by address range
};

// Opaque queue/stream token. Under the CUDA backend this IS cudaStream_t
// (forward-declared, so this header stays CUDA-include-free): declarations
// using backend::Stream and .cu definitions using cudaStream_t are the same
// type — no casts anywhere. Vulkan: command-batch handle (single compute
// queue; batches delimit submission like VkSplat's DEVICE_GUARD scopes).
#ifndef SS_BACKEND_VULKAN
using Stream = struct CUstream_st*;  // == cudaStream_t
#else
using Stream = void*;
#endif
inline constexpr Stream kDefaultStream = nullptr;

// --- device enumeration / selection ---
// A process runs on ONE device, picked lazily on the first device operation.
// Enumeration is side-effect-free (it does not initialize the backend), so
// apps can list devices and call device_select before starting work.
struct DeviceInfo {
    char name[256];       // human-readable device name ("" if index invalid)
    const char* type;     // "discrete"|"integrated"|"virtual"|"cpu"|"other"
                          // (static storage)
    uint64_t vram_bytes;  // device-local memory
    bool usable;          // meets the backend's feature requirements
};
// Number of devices visible to the backend (0: none / no driver).
int device_count();
// Info for device `index` in [0, device_count()).
DeviceInfo device_info(int index);
// Selects the device used by all subsequent backend work. Call before the
// first device operation. Returns false if `index` is out of range or not
// usable, or if the backend already initialized on a different device.
bool device_select(int index);
// Index of the device in use — or, before the backend initializes, the one
// it would pick (explicit selection, then backend env override, then
// auto-score). -1 if no usable device.
int device_current();

// --- device memory usage (best-effort, for status/telemetry UIs) ---
// Snapshot of VRAM usage on the current device. Each field is independently
// optional: a backend leaves a value at 0 with its has_* flag false when the
// corresponding query is unavailable (no driver support, extension missing,
// no device selected yet). Callers must fall back on the flags rather than
// treat 0 as "empty". Never throws; cheap enough to poll per frame.
struct MemoryUsage {
    uint64_t process_bytes = 0;  // device memory allocated by THIS process
    uint64_t used_bytes = 0;     // device memory in use system-wide (all apps)
    uint64_t total_bytes = 0;    // device-local memory capacity
    bool has_process = false;
    bool has_used = false;
    bool has_total = false;
};
MemoryUsage memory_usage();

// --- memory ---
void* device_malloc(size_t bytes);
// CONTRACT: device_free must ensure all in-flight device work that may
// reference the allocation completes before the memory is reclaimed
// (cudaFree's implicit sync). DeviceScratch::acquire relies on this when
// growing under a live queue.
void  device_free(void* ptr);
void* host_malloc_pinned(size_t bytes);  // cudaMallocHost / persistently-mapped staging
void  host_free_pinned(void* ptr);

const char* last_error();

// --- out-of-memory reporting ---
// device_malloc returns null on failure (cudaMalloc semantics), and most call
// sites cannot recover -- they need to abort with a message a user can act on
// rather than dereference the null. These helpers build that message from
// memory_usage()/device_info() so it names the size that failed and what the
// device already holds, and works identically on both backends.
inline std::string _fmt_bytes(uint64_t bytes) {
    char buf[32];
    double mib = (double)bytes / (1024.0 * 1024.0);
    if (mib >= 1024.0) std::snprintf(buf, sizeof buf, "%.2f GiB", mib / 1024.0);
    else               std::snprintf(buf, sizeof buf, "%.0f MiB", mib);
    return buf;
}
// `what` optionally names the buffer being allocated (for diagnostics).
[[noreturn]] inline void throw_out_of_memory(size_t bytes,
                                             const char* what = nullptr) {
    const char* err = last_error();
    MemoryUsage m = memory_usage();
    DeviceInfo d = device_info(device_current());
    std::string msg = err ? std::string("GPU allocation failed: ") + err +
                                "; could not allocate " + _fmt_bytes(bytes)
                          : "out of GPU memory: could not allocate " +
                                _fmt_bytes(bytes);
    if (what && what[0]) { msg += " for "; msg += what; }
    if (d.name[0]) { msg += " on "; msg += d.name; }
    msg += ".";
    if (m.has_used && m.has_total)
        msg += " Device memory in use: " + _fmt_bytes(m.used_bytes) + " / " +
               _fmt_bytes(m.total_bytes) + ".";
    if (m.has_process)
        msg += " Used by this process: " + _fmt_bytes(m.process_bytes) + ".";
    msg += err ? " If the device is out of memory, try"
               : " Try";
    msg += " a smaller model (fewer splats / SH), a lower "
           "training image resolution, or a GPU with more memory.";
    throw std::runtime_error(msg);
}
// device_malloc that throws the friendly OOM error above instead of returning
// null. A zero-byte request still returns null (a valid no-op allocation).
inline void* device_malloc_checked(size_t bytes, const char* what = nullptr) {
    void* ptr = device_malloc(bytes);
    if (!ptr && bytes > 0) throw_out_of_memory(bytes, what);
    return ptr;
}

void memcpy_sync(void* dst, const void* src, size_t bytes, MemcpyKind kind);
void memcpy_async(void* dst, const void* src, size_t bytes, MemcpyKind kind,
                  Stream stream = kDefaultStream);
void memset_sync(void* dst, int value, size_t bytes);
void memset_async(void* dst, int value, size_t bytes,
                  Stream stream = kDefaultStream);

// True if `ptr` is a device (or managed) address; false for null and
// pageable-host pointers. Used to auto-route TorchTensorView inputs that may
// be either host or device memory (zero-copy view vs. staging upload).
// Vulkan: address-range check against the pool's buffer-device-address spans.
bool is_device_pointer(const void* ptr);

// --- synchronization ---
void device_synchronize();
void stream_synchronize(Stream stream = kDefaultStream);

// --- events (async-readback fencing and perf timing) ---
struct Event;  // opaque per-backend
Event* event_create(bool enable_timing);
void   event_record(Event* event, Stream stream = kDefaultStream);
void   event_synchronize(Event* event);
void   event_destroy(Event* event);
float  event_elapsed_ms(Event* start, Event* end);  // requires enable_timing

}  // namespace backend

#ifndef SS_BACKEND_VULKAN
#include "backend/cuda/BackendRuntimeCuda.h"
#endif
