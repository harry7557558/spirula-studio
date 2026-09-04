#pragma once
// A one-function host thread helper.
//
// The GPU does the model; the host still resizes a frame, thresholds a mask and
// fills holes, and at 1008x1008x3 in double precision that resize alone is a
// tenth of a frame's budget. Those loops are embarrassingly parallel over rows,
// which is all this covers -- there is no task graph here and no need for one.
//
// Threads are spawned per call. At a handful of calls per frame the ~50 us that
// costs is far below the work being split; a persistent pool would buy nothing
// and would have to be shut down cleanly across the library boundary.

#include <algorithm>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace nn {

// Calls body(begin, end) on disjoint sub-ranges of [0, n) covering the whole
// range. Runs inline when the range is small or the machine is single-core, so
// the caller never has to special-case it.
template <typename F>
void parallel_for(int64_t n, const F& body, int64_t min_chunk = 1) {
    if (n <= 0) return;
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;
    int64_t workers = std::min<int64_t>((int64_t)hw, (n + min_chunk - 1) / min_chunk);
    if (workers <= 1) {
        body((int64_t)0, n);
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve((size_t)workers - 1);

    // Both ends need this: a throw off a worker is std::terminate, and one out
    // of the inline chunk would destroy a still-joinable pool, which is too.
    std::mutex err_mu;
    std::exception_ptr err;
    auto run = [&](int64_t lo, int64_t hi) {
        try {
            body(lo, hi);
        } catch (...) {
            std::lock_guard<std::mutex> lk(err_mu);
            if (!err) err = std::current_exception();
        }
    };

    const int64_t chunk = (n + workers - 1) / workers;
    for (int64_t w = 1; w < workers; ++w) {
        const int64_t lo = w * chunk;
        const int64_t hi = std::min(n, lo + chunk);
        if (lo >= hi) break;
        pool.emplace_back([&run, lo, hi] { run(lo, hi); });
    }
    run((int64_t)0, std::min(n, chunk));
    for (auto& t : pool) t.join();
    if (err) std::rethrow_exception(err);
}

}  // namespace nn
