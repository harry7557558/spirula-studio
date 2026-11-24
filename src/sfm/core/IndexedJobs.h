#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <thread>

#include "core/HardwareThreads.h"
#include "sfm/core/Cancel.h"

namespace sfm {

// Jobs write only to their own slots. Reduce on the caller after all jobs join.
template <typename Job>
void runIndexedJobs(size_t count, int threads, const Job& job) {
    cancel::check();
    if (!count) return;
    constexpr size_t kMaxWorkers = 4;
    const size_t requested = threads > 0 ? (size_t)threads : spirula::hardware_threads();
    const size_t workers = std::min({count, kMaxWorkers, std::max<size_t>(1, requested)});
    if (workers == 1) {
        for (size_t i = 0; i < count; ++i) {
            cancel::check();
            job(i);
        }
        cancel::check();
        return;
    }

    struct Failure {
        std::exception_ptr error;
        size_t index = 0;
        bool cancelled = false;
    };
    std::array<Failure, kMaxWorkers> failures;
    std::atomic<size_t> next{0};
    auto work = [&](size_t worker) noexcept {
        for (size_t i = next.fetch_add(1, std::memory_order_relaxed); i < count;
             i = next.fetch_add(1, std::memory_order_relaxed)) {
            try {
                cancel::check();
                job(i);
            } catch (const Cancelled&) {
                failures[worker] = {std::current_exception(), i, true};
                break;
            } catch (...) {
                failures[worker] = {std::current_exception(), i, false};
                break;
            }
        }
    };

    std::array<std::thread, kMaxWorkers - 1> pool;
    std::exception_ptr launch_error;
    size_t started = 0;
    try {
        for (; started < workers - 1; ++started) pool[started] = std::thread(work, started);
    } catch (...) {
        launch_error = std::current_exception();
        next.store(count, std::memory_order_relaxed);
    }
    if (!launch_error) work(workers - 1);
    for (size_t t = 0; t < started; ++t) pool[t].join();

    const Failure* first = nullptr;
    for (const Failure& f : failures) {
        if (f.cancelled) std::rethrow_exception(f.error);
        if (f.error && (!first || f.index < first->index)) first = &f;
    }
    cancel::check();
    if (first) std::rethrow_exception(first->error);
    if (launch_error) std::rethrow_exception(launch_error);
}

}  // namespace sfm
