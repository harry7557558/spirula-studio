// Memory-bounded parallel image decode for batch stages.
//
// `loadGrayImage` (stb_image, single-threaded) is the largest single cost in
// `spirula-sfm extract` on real captures -- ~66% of the stage on 17 MP JPEGs against
// 47 ms of GPU SIFT per image (src/sfm/README.md). The GPU side owns
// one VkContext and must stay on one thread, so decode is the part that gets a
// pool: workers decode ahead while the caller drives the GPU.
//
// The hard constraint is that this has to survive a few thousand 20 MP images,
// where naive "decode everything on all cores" needs tens of GB. Two things
// bound it:
//
//   * Delivery is strictly in input order, so the caller keeps the largest-first
//     ordering that lets SiftExtractor allocate device buffers exactly once.
//     Workers claim indices monotonically and block until the consumer is
//     within `window` of them, so the in-flight set is a bounded sliding window
//     rather than the whole dataset.
//   * `window` and the thread count are *derived from a byte budget* and the
//     largest image's dimensions, not fixed. Peak RSS is therefore roughly
//     constant in the number of images.
//
// Both halves of that second point are load-bearing and were both wrong once:
// a fixed 1 GiB budget against a 7 B/source-pixel decode gave 3 threads on 21
// MP JPEGs, and extraction ran at 0.18 s/image with neither the CPU nor the
// GPU busy. The budget now comes from the machine (defaultDecodeBudget) and
// the per-decode peak from what is actually held (see below), which on a
// 32-core box is 32 threads and a GPU-bound stage.
//
// Peak accounting per concurrent decode is stb's 3-byte RGB buffer at the
// *source* resolution plus the downscaled outputs it is resampled into; a
// decoded image waiting in the window costs the downscaled float image
// (4 B/px). loadGrayImage() resamples straight out of the RGB buffer
// (sfm/core/Image.cpp), so no full-resolution float image is ever held.
#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "sfm/core/HostMemory.h"
#include "sfm/core/Image.h"

namespace sfm {

struct ImageLoadOptions {
    int max_image_size = 3200;    // long-edge clamp, COLMAP's default
    int num_threads = 0;          // 0 = hardware_concurrency, 1 = decode inline
    bool want_color = false;      // also decode a downscaled RGB buffer (for point colors)
    // The files' colour space; decoded pixels are converted to sRGB.
    std::string gamut;
    std::optional<bool> is_linear;
    // Keypoint masks (sfm/core/Mask.h), one per entry of the `paths` passed to
    // loadImagesInOrder and in the same order; "" means "no mask for this
    // image". Empty (the default) skips mask decoding entirely. Masks are
    // decoded on the pool with their image because they are PNGs of the same
    // order of magnitude -- doing them serially on the consumer thread would
    // hand the GPU stage back the decode cost the pool exists to hide.
    std::vector<std::string> mask_paths;
    // Swap keep and ignore in every decoded mask (sfm/core/Mask.h). On the pool
    // rather than the consumer thread, which the mask decode is already on.
    bool flip_mask = false;
    // Host memory the decoder may use for in-flight images. Half is charged to
    // concurrent decodes, half to the ready window; both are then at least 1,
    // so a single image larger than the budget still loads (it just runs alone).
    //
    // 0 = derive it from the machine (defaultDecodeBudget below). A fixed 1 GiB
    // was the old default; it is the right answer on a 17 MP capture with four
    // cores and badly wrong on a 21 MP one with thirty-two, where it held the
    // pool to 3 decode threads while the GPU waited. The budget is what bounds
    // peak RSS, so it belongs to the machine, not to the code.
    size_t memory_budget_bytes = 0;
};

// A quarter of physical RAM, clamped to [1 GiB, 8 GiB]. The decoder's peak
// in-flight set is bounded by this, so it is peak *anonymous* RSS for the
// stage; a quarter leaves the page cache the room it needs to keep serving the
// very files being decoded. Falls back to the historical 1 GiB when the
// platform will not say how much memory it has.
inline size_t defaultDecodeBudget() {
    const size_t total = physicalRamBytes();
    if (total == 0) return 1ull << 30;
    return std::min<size_t>(std::max<size_t>(total / 4, 1ull << 30), 8ull << 30);
}

// What the pool decided, for logging.
struct ImageLoadPlan {
    int num_threads = 1;
    int window = 1;
    size_t decode_peak_bytes = 0;  // per concurrent decode, largest image
    size_t held_bytes = 0;         // per image waiting in the window
};

namespace detail {

// Downscaled size after the long-edge clamp.
inline void clampedSize(int w, int h, int max_image_size, int& dw, int& dh) {
    dw = w;
    dh = h;
    int longEdge = std::max(w, h);
    if (max_image_size > 0 && longEdge > max_image_size) {
        double scale = (double)max_image_size / longEdge;
        dw = std::max(1, (int)std::lround(w * scale));
        dh = std::max(1, (int)std::lround(h * scale));
    }
}

}  // namespace detail

// Derive thread count and window from the budget and the largest input.
// `dims` are the probed (width, height) of every image, in any order.
inline ImageLoadPlan planImageLoad(const std::vector<std::pair<int, int>>& dims,
                                   const ImageLoadOptions& opt) {
    ImageLoadPlan plan;
    size_t maxPix = 1, maxOutPix = 1;
    for (const auto& d : dims) {
        maxPix = std::max(maxPix, (size_t)d.first * (size_t)d.second);
        int dw = 0, dh = 0;
        detail::clampedSize(d.first, d.second, opt.max_image_size, dw, dh);
        maxOutPix = std::max(maxOutPix, (size_t)dw * (size_t)dh);
    }
    // stb RGB at the source resolution (3 B/src px), plus the downscaled gray
    // float (4 B/out px) it is resampled into and, with want_color, the
    // downscaled color buffer built before the RGB is freed (3 B/out px). A
    // mask, when one is requested, is charged 2 B/px of the *decoded* size: 1 B
    // for stb's gray read plus 1 B for the binarized copy. Masks are not
    // header-probed, so this is an estimate -- they are segmentation output at
    // or below the image resolution in every convention we have seen, and
    // 1 B/px against the image's 3 B/px leaves the budget dominated by the
    // image either way.
    const size_t mask_bytes = opt.mask_paths.empty() ? 0 : maxOutPix * 2;
    plan.decode_peak_bytes =
        maxPix * 3 + maxOutPix * (opt.want_color ? 7 : 4) + mask_bytes;
    plan.held_bytes = maxOutPix * (opt.want_color ? 7 : 4)  // gray float (+ RGB u8)
                    + (opt.mask_paths.empty() ? 0 : maxOutPix);

    unsigned hc = std::thread::hardware_concurrency();
    int want = opt.num_threads > 0 ? opt.num_threads : (hc > 0 ? (int)hc : 1);
    const size_t budget =
        opt.memory_budget_bytes ? opt.memory_budget_bytes : defaultDecodeBudget();
    const size_t half = budget / 2;

    int by_mem = (int)std::max<size_t>(1, half / plan.decode_peak_bytes);
    plan.num_threads = std::max(1, std::min(want, by_mem));
    // The window must cover every worker or a worker could block on an index
    // the consumer is waiting for. Beyond that it is pure decode-ahead slack.
    int win_by_mem = (int)std::max<size_t>(1, half / plan.held_bytes);
    plan.window = std::max(plan.num_threads, std::min(2 * plan.num_threads, win_by_mem));
    return plan;
}

// Decode `paths` on the pool and hand each image to `consume(index, image)`
// **on the calling thread, in input order**. A file that fails to decode is
// reported through `on_error(index, message)` (if given) and skipped rather
// than aborting the batch -- one bad file in a thousand should not lose the run.
//
// `plan` comes from planImageLoad(); pass the one you logged so the numbers the
// user sees are the numbers in force.
inline void loadImagesInOrder(const std::vector<std::string>& paths, const ImageLoadPlan& plan,
                              const ImageLoadOptions& opt,
                              const std::function<void(size_t, GrayImage&)>& consume,
                              const std::function<void(size_t, const std::string&)>& on_error
                                  = nullptr) {
    const size_t n = paths.size();
    if (n == 0) return;

    auto decodeOne = [&](size_t i, GrayImage& out, std::string& err) {
        static const std::string kNoMask;
        const std::string& mp = i < opt.mask_paths.size() ? opt.mask_paths[i] : kNoMask;
        try {
            out = loadGrayImage(paths[i], opt.max_image_size, opt.want_color, mp,
                                opt.gamut, opt.is_linear, opt.flip_mask);
        } catch (const std::exception& e) {
            err = e.what();
        }
    };

    if (plan.num_threads <= 1) {
        for (size_t i = 0; i < n; i++) {
            GrayImage img;
            std::string err;
            decodeOne(i, img, err);
            if (!err.empty()) {
                if (on_error) on_error(i, err);
                continue;
            }
            consume(i, img);
        }
        return;
    }

    std::vector<GrayImage> slots(n);
    std::vector<std::string> errors(n);
    std::vector<char> ready(n, 0);
    std::atomic<size_t> next_claim{0};
    size_t next_consume = 0;  // guarded by mtx
    std::mutex mtx;
    std::condition_variable cv_ready, cv_window;

    std::vector<std::thread> workers;
    workers.reserve(plan.num_threads);
    for (int t = 0; t < plan.num_threads; t++) {
        workers.emplace_back([&] {
            for (;;) {
                size_t i = next_claim.fetch_add(1);
                if (i >= n) return;
                {   // decode-ahead gate: keep the in-flight set bounded
                    std::unique_lock<std::mutex> lk(mtx);
                    cv_window.wait(lk, [&] { return i < next_consume + (size_t)plan.window; });
                }
                GrayImage img;
                std::string err;
                decodeOne(i, img, err);
                {
                    std::lock_guard<std::mutex> lk(mtx);
                    slots[i] = std::move(img);
                    errors[i] = std::move(err);
                    ready[i] = 1;
                }
                cv_ready.notify_all();
            }
        });
    }

    for (size_t i = 0; i < n; i++) {
        GrayImage img;
        std::string err;
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv_ready.wait(lk, [&] { return ready[i] != 0; });
            img = std::move(slots[i]);
            slots[i] = GrayImage();  // release the window slot immediately
            err = std::move(errors[i]);
            next_consume = i + 1;
        }
        cv_window.notify_all();
        if (!err.empty()) {
            if (on_error) on_error(i, err);
            continue;
        }
        consume(i, img);
    }
    for (std::thread& w : workers) w.join();
}

}  // namespace sfm
