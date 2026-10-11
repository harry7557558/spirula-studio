// Worker pool for the CPU bundle adjustment: one for the process, not one per
// solver. The bottom-up phase runs a mapper (and its solves) per atom worker,
// and a pool inside each would oversubscribe the machine by that factor. So
// parallel regions serialize against each other, which is what keeps them at
// full width, and small solves stay inline instead of entering the pool.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "core/Env.h"

namespace bacpu {

class Pool {
public:
    // Machine-wide (SS_SFM_BA_THREADS overrides); a solve that wants fewer
    // threads caps its task count instead. Sizing this from the first caller
    // would leave every later solve as narrow as an atom worker's threads=1.
    static Pool& get() {
        static Pool p;
        return p;
    }

    int size() const { return (int)workers_.size() + 1; }

    // fn(task, tid) for task in [0, ntasks), handed out dynamically over at
    // most `maxWorkers` threads. Anything reduced afterwards must be keyed on
    // `task` rather than `tid`, or the result depends on the scheduling.
    template <class F>
    void run(int ntasks, int maxWorkers, F&& fn) {
        if (ntasks <= 0) return;
        if (workers_.empty() || ntasks == 1 || maxWorkers <= 1) {
            for (int t = 0; t < ntasks; t++) fn(t, 0);
            return;
        }
        Job<F> job{std::forward<F>(fn)};
        dispatch(ntasks, maxWorkers, job);
    }
    template <class F>
    void run(int ntasks, F&& fn) {
        run(ntasks, size(), std::forward<F>(fn));
    }
    template <class F>
    void runChecked(int ntasks, int maxWorkers, F&& fn) {
        std::mutex failure_mutex; std::exception_ptr failure; std::atomic<bool> failed{false};
        run(ntasks, maxWorkers, [&](int task, int tid) {
            if (failed.load(std::memory_order_relaxed)) return;
            try { fn(task, tid); }
            catch (...) {
                std::lock_guard<std::mutex> lock(failure_mutex);
                if (!failure) failure = std::current_exception();
                failed.store(true, std::memory_order_relaxed);
            }
        });
        if (failure) std::rethrow_exception(failure);
    }

private:
    struct AnyJob {
        virtual void call(int task, int tid) const = 0;
        virtual ~AnyJob() = default;
    };
    template <class F>
    struct Job : AnyJob {
        F f;
        explicit Job(F&& fn) : f(std::forward<F>(fn)) {}
        void call(int task, int tid) const override { f(task, tid); }
    };

    Pool() {
        int threads = 0;
        if (const char* e = spirula::env("SFM_BA_THREADS")) threads = atoi(e);
        unsigned hc = std::thread::hardware_concurrency();
        int n = threads > 0 ? threads : (int)(hc ? hc : 1u);
        workers_.reserve((size_t)n - 1);
        for (int i = 1; i < n; i++) workers_.emplace_back([this, i] { loop(i); });
    }

    ~Pool() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (std::thread& t : workers_) t.join();
    }

    void dispatch(int ntasks, int maxWorkers, const AnyJob& job) {
        std::lock_guard<std::mutex> region(region_);  // one parallel region at a time
        {
            std::lock_guard<std::mutex> lk(mu_);
            job_ = &job;
            ntasks_ = ntasks;
            limit_ = maxWorkers;
            next_.store(0, std::memory_order_relaxed);
            done_ = 0;
            gen_++;
        }
        cv_.notify_all();
        drain(0);
        std::unique_lock<std::mutex> lk(mu_);
        cv_done_.wait(lk, [&] { return done_ == (int)workers_.size(); });
        job_ = nullptr;
    }

    void drain(int tid) {
        if (tid >= limit_) return;
        for (int t = next_.fetch_add(1, std::memory_order_relaxed); t < ntasks_;
             t = next_.fetch_add(1, std::memory_order_relaxed))
            job_->call(t, tid);
    }

    void loop(int tid) {
        uint64_t seen = 0;
        for (;;) {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return stop_ || gen_ != seen; });
            if (stop_) return;
            seen = gen_;
            lk.unlock();
            drain(tid);
            lk.lock();
            if (++done_ == (int)workers_.size()) cv_done_.notify_one();
        }
    }

    std::vector<std::thread> workers_;
    std::mutex region_, mu_;
    std::condition_variable cv_, cv_done_;
    const AnyJob* job_ = nullptr;
    std::atomic<int> next_{0};
    int ntasks_ = 0, done_ = 0, limit_ = 0;
    uint64_t gen_ = 0;
    bool stop_ = false;
};

// Split [0, n) into `ntasks` contiguous pieces; piece `t` is [lo, hi).
inline void taskRange(int64_t n, int ntasks, int t, int64_t& lo, int64_t& hi) {
    int64_t q = n / ntasks, r = n % ntasks;
    lo = q * t + (t < r ? t : r);
    hi = lo + q + (t < r ? 1 : 0);
}

// `n` items at `grain` apiece, capped by the pool. One task runs inline, which
// is what keeps a forty-image solve off the pool entirely.
inline int taskCount(int64_t n, int64_t grain, int nthreads) {
    if (n <= grain || nthreads <= 1) return 1;
    int64_t k = (n + grain - 1) / grain;
    return (int)(k < nthreads ? k : nthreads);
}

}  // namespace bacpu
