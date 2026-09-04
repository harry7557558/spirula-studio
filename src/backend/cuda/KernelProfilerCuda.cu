// Per-kernel GPU timing for the CUDA backend (SS_PROFILE=1) -- the twin of the
// Vulkan timestamp funnel (backend/vulkan/VulkanRuntime.cpp), so both backends
// report the same table and the numbers compare directly.
//
// No launch site is instrumented by hand: the link line carries
// -Wl,--wrap=cudaLaunchKernel (cmake/SsBackendCuda.cmake), which routes every
// `<<<>>>` -- ours and CUB's, which launches through the same entry point --
// through __wrap_cudaLaunchKernel. It brackets the launch with a CUDA event
// pair and names it from cudaFuncGetName.

#include <cuda_runtime.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

#include "backend/api/BackendRuntime.h"
#include "backend/common/Profiler.h"

extern "C" cudaError_t __real_cudaLaunchKernel(const void* func, dim3 grid,
                                               dim3 block, void** args,
                                               size_t shared,
                                               cudaStream_t stream);

namespace {

// A launch's (start, end) event pair, resolved once the events have completed.
struct Pending {
    cudaEvent_t start, end;
    const char* name;
};

// Drained when full; sized so a 1000-step training run drains a handful of
// times, and each drain costs one cudaEventElapsedTime per entry.
constexpr size_t kMaxPending = 8192;

struct Entry {
    uint64_t count = 0;
    double ms = 0.0;
};

// Leaked on purpose: backend::prof::g_state is an inline variable in another
// TU, so its destructor -- which prints this table -- may run after a static
// here would have been destroyed.
struct Store {
    std::mutex mutex;
    std::vector<cudaEvent_t> pool;
    std::vector<Pending> pending;
    std::unordered_map<const void*, const char*> names;
    std::vector<std::string> interned;
    std::map<std::string, Entry> by_kernel;
    uint64_t untimed = 0;
};
Store& store() {
    static Store* s = new Store();
    return *s;
}

// `void ns::foo<T>(float*, int)` -> `ns::foo`. Template arguments go the way
// the Vulkan side folds its spec constants: one row per entry point, not one
// per instantiation.
std::string short_name(const char* mangled) {
    std::string s = mangled ? mangled : "?";
#if defined(__GNUC__) || defined(__clang__)
    int status = 0;
    char* d = abi::__cxa_demangle(s.c_str(), nullptr, nullptr, &status);
    if (status == 0 && d) s = d;
    std::free(d);
#endif
    int depth = 0;
    std::string out;
    for (char c : s) {
        if (c == '<') { depth++; continue; }
        if (c == '>') { depth--; continue; }
        if (depth > 0) continue;
        if (c == '(') break;
        out += c;
    }
    size_t sp = out.find_last_of(' ');
    if (sp != std::string::npos) out = out.substr(sp + 1);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out.empty() ? std::string("?") : out;
}

const char* name_for(const void* func) {
    Store& st = store();
    auto it = st.names.find(func);
    if (it != st.names.end()) return it->second;
    const char* mangled = nullptr;
    if (cudaFuncGetName(&mangled, func) != cudaSuccess) {
        cudaGetLastError();
        mangled = nullptr;
    }
    st.interned.push_back(short_name(mangled));
    const char* interned = st.interned.back().c_str();
    st.names.emplace(func, interned);
    return interned;
}

// Caller holds the mutex. Waits on the newest end event, then folds every
// pair into its kernel's row and returns the events to the pool.
void drain_locked() {
    Store& st = store();
    if (st.pending.empty()) return;
    cudaEventSynchronize(st.pending.back().end);
    for (const Pending& p : st.pending) {
        float ms = 0.0f;
        if (cudaEventElapsedTime(&ms, p.start, p.end) == cudaSuccess) {
            Entry& e = st.by_kernel[p.name];
            e.count++;
            e.ms += ms;
            backend::prof::add(backend::prof::GPU, (uint64_t)(ms * 1e6), 0);
        } else {
            cudaGetLastError();
        }
        st.pool.push_back(p.start);
        st.pool.push_back(p.end);
    }
    st.pending.clear();
}

cudaEvent_t event_acquire(Store& st) {
    if (!st.pool.empty()) {
        cudaEvent_t e = st.pool.back();
        st.pool.pop_back();
        return e;
    }
    cudaEvent_t e = nullptr;
    if (cudaEventCreateWithFlags(&e, cudaEventDefault) != cudaSuccess) {
        cudaGetLastError();
        return nullptr;
    }
    return e;
}

void report() {
    Store& st = store();
    std::lock_guard<std::mutex> lock(st.mutex);
    st.untimed += st.pending.size();  // no sync point followed these
    if (st.by_kernel.empty()) return;
    std::vector<std::pair<std::string, Entry>> rows(st.by_kernel.begin(),
                                                    st.by_kernel.end());
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
    std::fprintf(stderr,
                 "\n[spirula-profile] ---- GPU time by kernel (entry) ----\n");
    std::fprintf(stderr, "%-44s %7s %10s %9s\n", "entry", "count", "gpu_ms",
                 "us/call");
    for (const auto& r : rows) {
        double us = r.second.count ? (r.second.ms * 1e3) / r.second.count : 0.0;
        std::fprintf(stderr, "%-44s %7llu %10.3f %9.2f\n", r.first.c_str(),
                     (unsigned long long)r.second.count, r.second.ms, us);
    }
    if (st.untimed)
        std::fprintf(stderr,
                     "[spirula-profile] note: %llu launch(es) untimed\n",
                     (unsigned long long)st.untimed);
    std::fprintf(stderr,
                 "[spirula-profile] -----------------------------------\n");
}

void resolve() {
    Store& st = store();
    std::lock_guard<std::mutex> lock(st.mutex);
    drain_locked();
}

const bool g_registered = (backend::prof::g_kernel_report = &report,
                           backend::prof::g_kernel_resolve = &resolve, true);

}  // namespace

extern "C" cudaError_t __wrap_cudaLaunchKernel(const void* func, dim3 grid,
                                               dim3 block, void** args,
                                               size_t shared,
                                               cudaStream_t stream) {
    if (!backend::prof::enabled())
        return __real_cudaLaunchKernel(func, grid, block, args, shared, stream);
    (void)g_registered;
    Store& st = store();
    std::lock_guard<std::mutex> lock(st.mutex);
    if (st.pending.size() >= kMaxPending) drain_locked();
    cudaEvent_t start = event_acquire(st);
    cudaEvent_t end = start ? event_acquire(st) : nullptr;
    if (!start || !end) {
        if (start) st.pool.push_back(start);
        st.untimed++;
        return __real_cudaLaunchKernel(func, grid, block, args, shared, stream);
    }
    const char* name = name_for(func);
    cudaEventRecord(start, stream);
    cudaError_t r =
        __real_cudaLaunchKernel(func, grid, block, args, shared, stream);
    cudaEventRecord(end, stream);
    st.pending.push_back({start, end, name});
    return r;
}
