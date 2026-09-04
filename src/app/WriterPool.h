#pragma once

// A few worker threads that encode and write images while the GPU gets on with
// the next frame.
//
// PNG and JPEG encoding is pure CPU and it is not cheap: a 1080p binary mask
// costs ~75 ms through stb's deflate and a 4K JPEG ~25 ms, against ~200 ms of
// model time for the frame that produced it. Decoding, the sharpness metric
// and SAM all run on the device, so a caller that writes inline spends a third
// of every frame with the GPU idle, waiting on zlib.
//
// The queue is bounded, so a slow disk still applies back-pressure instead of
// growing until memory runs out; `submit` blocks when it is full.

#include "app/DepthPng.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"
#include "sam/Sam.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace app {

// An image, a mask or a depth job: whichever member is non-empty is written.
struct WriteJob {
    nn::Image             image;    // RGB
    sam::Mask             mask;
    std::vector<uint16_t> depth;    // 16-bit grayscale, `depth_w` x `depth_h`
    int                   depth_w = 0, depth_h = 0;
    std::string           path;
    int                   quality = 95;
};

class WriterPool {
public:
    // 0 threads = cores - 1, which is what leaves the calling thread free to
    // keep feeding the GPU.
    explicit WriterPool(int threads = 0) {
        const int n = threads > 0
                          ? threads
                          : std::max(1, (int)std::thread::hardware_concurrency() - 1);
        limit_ = (size_t)n * 3;
        for (int i = 0; i < n; ++i) workers_.emplace_back([this] { run(); });
    }
    ~WriterPool() { finish(); }

    WriterPool(const WriterPool&) = delete;
    WriterPool& operator=(const WriterPool&) = delete;

    void submit(WriteJob&& job) {
        std::unique_lock<std::mutex> lk(mu_);
        space_.wait(lk, [this] { return queue_.size() < limit_; });
        queue_.push_back(std::move(job));
        work_.notify_one();
    }

    // Waits for everything queued to reach the disk. Called by the destructor,
    // so it is only needed when the caller wants the files to exist before it
    // goes on to the next stage.
    void finish() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (done_) return;
            done_ = true;
        }
        work_.notify_all();
        for (auto& t : workers_) t.join();
        workers_.clear();
    }

    int    threads() const { return (int)std::max<size_t>(limit_ / 3, 1); }
    double busyMs() const { return busy_ms_.load(); }
    int    failures() const { return failures_.load(); }

private:
    void run() {
        while (true) {
            WriteJob job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                work_.wait(lk, [this] { return !queue_.empty() || done_; });
                // done_ with work still queued means "drain, then stop".
                if (queue_.empty()) return;
                job = std::move(queue_.front());
                queue_.pop_front();
                space_.notify_one();
            }
            const double t0 = nn::now_ms();
            // An encoder that throws here would take the process with it: an
            // exception off a worker thread is std::terminate.
            bool ok = false;
            std::string why;
            try {
                ok = !job.depth.empty()
                         ? save_depth_png16(job.path, job.depth.data(),
                                            job.depth_w, job.depth_h)
                         : (job.image.empty()
                                ? sam::save_mask_png(job.mask, job.path)
                                : nn::save_image(job.image, job.path,
                                                 job.quality));
            } catch (const std::exception& e) {
                why = e.what();
            } catch (...) {
                why = "unknown error";
            }
            if (!ok) {
                ++failures_;
                NN_LOG_ERROR("could not write %s%s%s\n", job.path.c_str(),
                             why.empty() ? "" : ": ", why.c_str());
            }
            double expected = busy_ms_.load();
            const double add = nn::now_ms() - t0;
            while (!busy_ms_.compare_exchange_weak(expected, expected + add)) {}
        }
    }

    std::vector<std::thread> workers_;
    std::deque<WriteJob>     queue_;
    std::mutex               mu_;
    std::condition_variable  work_, space_;
    size_t                   limit_ = 3;
    bool                     done_ = false;
    std::atomic<double>      busy_ms_{0.0};
    std::atomic<int>         failures_{0};
};

}  // namespace app
