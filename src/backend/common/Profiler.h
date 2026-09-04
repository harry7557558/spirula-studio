#pragma once
// Optional, env-gated (SS_PROFILE=1) timing profiler shared by BOTH
// backends. It instruments the handful of functions that exist identically
// on the CUDA and Vulkan runtimes -- memcpy/memset and the synchronization
// points -- so the same methodology attributes wall-clock time on both
// devices and the numbers are directly comparable.
//
// Buckets (CPU wall-clock):
//   H2D / D2H / D2D  -- PCIe transfer time (pure copy: see below)
//   MEMSET           -- device fills
//   DEVSYNC          -- time the CPU is blocked waiting for the GPU. Because
//                       every device-touching sync copy first drains the
//                       queue (CUDA: cudaMemcpy orders after prior work;
//                       Vulkan: memcpy_sync waits flush_all_streams), a
//                       device_synchronize() is issued *before* the timed
//                       copy when profiling, so the kernel-drain lands here
//                       and the copy bucket measures the transfer alone.
//                       This bucket is the device (GPU) execution time proxy.
//
// host time = (wall since process start) - transfer - memset - devsync.
//
// Zero overhead and zero behavior change when SS_PROFILE is unset.

#include "backend/api/BackendRuntime.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include "core/Env.h"

namespace backend {
namespace prof {

// DEVSYNC = CPU wall blocked waiting for the GPU (kernel time + submission /
// queue round-trip latency). GPU = true on-device kernel execution time, from
// device timestamp queries (Vulkan dispatch funnel). GPU is a *component* of
// DEVSYNC, not additional wall, so it is never subtracted from host time;
// submission/queue latency is reported as DEVSYNC - GPU.
enum Cat { H2D = 0, D2H, D2D, MEMSET, DEVSYNC, GPU, NCAT };

struct Bucket {
    std::atomic<uint64_t> count{0}, bytes{0}, ns{0};
};

struct State {
    Bucket b[NCAT];
    std::chrono::steady_clock::time_point t0;
    bool on;
    // Printed after the timing table. Captured by its producer while the
    // source is live -- the VRAM pool is destroyed before ~State runs.
    std::string note;
    State()
        : t0(std::chrono::steady_clock::now()),
          on(spirula::env("PROFILE") != nullptr) {}
    ~State();  // prints the report at process exit
};

// C++17 inline variable: a single instance across all TUs, constructed during
// dynamic init (before main) and destroyed at exit -- so t0 ~ process start
// and the destructor report captures ~full process wall.
inline State g_state;

inline bool enabled() { return g_state.on; }

// Per-kernel GPU timing, set by the backend that owns it (CUDA:
// backend/cuda/KernelProfilerCuda.cu; Vulkan reports from runtime_shutdown()).
// resolve() must run at a device drain: at process exit the context is gone.
inline void (*g_kernel_report)() = nullptr;
inline void (*g_kernel_resolve)() = nullptr;

inline void set_exit_note(std::string text) { g_state.note = std::move(text); }

inline void add(Cat c, uint64_t ns, uint64_t bytes) {
    Bucket& x = g_state.b[c];
    x.count.fetch_add(1, std::memory_order_relaxed);
    x.ns.fetch_add(ns, std::memory_order_relaxed);
    x.bytes.fetch_add(bytes, std::memory_order_relaxed);
}

// RAII wall-clock scope; adds to a bucket on destruction.
struct Scope {
    Cat c;
    uint64_t bytes;
    std::chrono::steady_clock::time_point s;
    explicit Scope(Cat c_, uint64_t bytes_ = 0)
        : c(c_), bytes(bytes_), s(std::chrono::steady_clock::now()) {}
    ~Scope() {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - s)
                      .count();
        add(c, (uint64_t)ns, bytes);
    }
};

inline Cat kind_cat(MemcpyKind k) {
    switch (k) {
        case MemcpyKind::HostToDevice:   return H2D;
        case MemcpyKind::DeviceToHost:   return D2H;
        case MemcpyKind::DeviceToDevice: return D2D;
        default:                         return H2D;  // Auto: caller refines
    }
}

inline const char* cat_name(Cat c) {
    switch (c) {
        case H2D:     return "H2D  (upload)";
        case D2H:     return "D2H  (readback)";
        case D2D:     return "D2D  (dev->dev)";
        case MEMSET:  return "memset";
        case DEVSYNC: return "device (GPU wait)";
        case GPU:     return "  of which GPU-exec";
        default:      return "?";
    }
}

inline State::~State() {
    if (!on) return;
    if (g_kernel_report) g_kernel_report();
    double wall_ms =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            std::chrono::steady_clock::now() - t0)
            .count();
    double transfer = 0, devsync = 0, memset = 0, gpu = 0;
    for (int c = 0; c < NCAT; c++) {
        double ms = b[c].ns.load() * 1e-6;
        if (c == H2D || c == D2H || c == D2D) transfer += ms;
        else if (c == DEVSYNC) devsync += ms;
        else if (c == MEMSET) memset += ms;
        else if (c == GPU) gpu += ms;  // component of devsync, not extra wall
    }
    double host = wall_ms - transfer - devsync - memset;
    std::fprintf(stderr,
                 "\n[spirula-profile] ---- timing breakdown ----\n");
    std::fprintf(stderr, "%-20s %8s %12s %11s %9s\n", "category", "count",
                 "bytes", "wall_ms", "GB/s");
    for (int c = 0; c < NCAT; c++) {
        uint64_t cnt = b[c].count.load(), by = b[c].bytes.load();
        double ms = b[c].ns.load() * 1e-6;
        if (cnt == 0 && by == 0) continue;
        if (c == H2D || c == D2H || c == D2D) {
            double gbs = ms > 0 ? (by / 1e9) / (ms / 1e3) : 0.0;
            std::fprintf(stderr, "%-20s %8llu %12llu %11.3f %9.2f\n",
                         cat_name((Cat)c), (unsigned long long)cnt,
                         (unsigned long long)by, ms, gbs);
        } else {
            std::fprintf(stderr, "%-20s %8llu %12s %11.3f %9s\n",
                         cat_name((Cat)c), (unsigned long long)cnt, "-", ms,
                         "-");
        }
    }
    std::fprintf(stderr, "%-20s %8s %12s %11.3f\n", "  transfer total", "", "",
                 transfer);
    std::fprintf(stderr, "%-20s %8s %12s %11.3f\n", "  device wait total", "",
                 "", devsync);
    if (gpu > 0) {
        std::fprintf(stderr, "%-20s %8s %12s %11.3f\n", "    GPU-exec (stamps)",
                     "", "", gpu);
        std::fprintf(stderr, "%-20s %8s %12s %11.3f\n",
                     "    submit/queue lat.", "", "", devsync - gpu);
    }
    std::fprintf(stderr, "%-20s %8s %12s %11.3f\n", "  host (est.)", "", "",
                 host);
    std::fprintf(stderr, "%-20s %8s %12s %11.3f\n", "  wall (total)", "", "",
                 wall_ms);
    std::fprintf(stderr,
                 "[spirula-profile] --------------------------\n");
    if (!note.empty()) std::fputs(note.c_str(), stderr);
}

// Issue a device drain and attribute its wall to DEVSYNC. Called before a
// device-touching sync copy so the copy bucket measures pure transfer. Uses
// the backend's own (instrumented) device_synchronize so both backends
// record identically and there is no double counting.
inline void drain_before_copy() { backend::device_synchronize(); }

}  // namespace prof
}  // namespace backend
