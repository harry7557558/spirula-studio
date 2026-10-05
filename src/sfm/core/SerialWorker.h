// One thread that runs jobs in submission order, so a stage's host-side tail
// (colors, masks, writing a file) overlaps the device work of the next item.
// Bounded: submit() blocks while `depth` jobs are waiting. A job's exception is
// rethrown by the next submit() or by finish(), and later jobs are dropped.
#pragma once

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace sfm {

class SerialWorker {
public:
    explicit SerialWorker(size_t depth = 2) : depth_(depth > 0 ? depth : 1) {
        thread_ = std::thread([this] { run(); });
    }
    // Waits for the queue; an error nobody collected with finish() is lost.
    ~SerialWorker() {
        try {
            finish();
        } catch (...) {
        }
    }
    SerialWorker(const SerialWorker&) = delete;
    SerialWorker& operator=(const SerialWorker&) = delete;

    void submit(std::function<void()> job) {
        std::unique_lock<std::mutex> lk(mu_);
        space_.wait(lk, [&] { return queue_.size() < depth_ || error_; });
        rethrowLocked();
        queue_.push_back(std::move(job));
        work_.notify_one();
    }

    // Run everything queued, stop the thread, rethrow the first error.
    void finish() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!thread_.joinable()) return;
            stop_ = true;
        }
        work_.notify_one();
        thread_.join();
        std::lock_guard<std::mutex> lk(mu_);
        rethrowLocked();
    }

private:
    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                work_.wait(lk, [&] { return stop_ || !queue_.empty(); });
                if (queue_.empty()) return;
                job = std::move(queue_.front());
                queue_.pop_front();
                if (error_) {
                    queue_.clear();
                    space_.notify_all();
                    continue;
                }
            }
            space_.notify_all();
            try {
                job();
            } catch (...) {
                std::lock_guard<std::mutex> lk(mu_);
                if (!error_) error_ = std::current_exception();
                space_.notify_all();
            }
        }
    }

    void rethrowLocked() {
        if (error_) std::rethrow_exception(error_);
    }

    size_t depth_;
    std::mutex mu_;
    std::condition_variable work_, space_;
    std::deque<std::function<void()>> queue_;
    std::exception_ptr error_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace sfm
