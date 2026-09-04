// FrameSelect.cpp -- see FrameSelect.h.

#include "app/gui/FrameSelect.h"

#include "core/ExrImage.h"

#include "external/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

// Sharpness metric, matching extract_frames.py add_frame(): resize to
// 512x512 (area average), grayscale, subtract mean, variance of the 3x3
// Laplacian. Returns -1 on decode failure.
double sharpness_score(const std::string& path) {
    int W = 0, H = 0, C = 0;
    std::vector<uint8_t> exr_rgb;
    unsigned char* img = nullptr;
    if (exr::is_exr(path)) {
        exr::Info info;
        if (!exr::decode_srgb8(path, exr::Options(), info, exr_rgb).empty()) return -1.0;
        W = info.width;
        H = info.height;
        img = exr_rgb.data();
    } else {
        img = stbi_load(path.c_str(), &W, &H, &C, 3);
        if (!img) return -1.0;
    }

    constexpr int S = 512;
    std::vector<float> gray((size_t)S * S);
    double mean = 0.0;
    for (int y = 0; y < S; y++) {
        // Box-average the source rows/cols covered by this output cell.
        int y0 = (int)((int64_t)y * H / S), y1 = (int)((int64_t)(y + 1) * H / S);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < S; x++) {
            int x0 = (int)((int64_t)x * W / S), x1 = (int)((int64_t)(x + 1) * W / S);
            if (x1 <= x0) x1 = x0 + 1;
            double acc = 0.0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const unsigned char* p = img + ((size_t)yy * W + xx) * 3;
                    // BT.601 luma like cv2.cvtColor BGR2GRAY (RGB order here).
                    acc += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
                }
            float v = (float)(acc / ((y1 - y0) * (x1 - x0)));
            gray[(size_t)y * S + x] = v;
            mean += v;
        }
    }
    if (exr_rgb.empty()) stbi_image_free(img);
    mean /= (double)S * S;
    for (auto& v : gray) v -= (float)mean;

    // Variance of the 3x3 Laplacian (cv2.Laplacian default kernel).
    double sum = 0.0, sum2 = 0.0;
    int64_t n = 0;
    for (int y = 1; y < S - 1; y++)
        for (int x = 1; x < S - 1; x++) {
            const float* r = &gray[(size_t)y * S + x];
            double lap = (double)r[-S] + r[S] + r[-1] + r[1] - 4.0 * r[0];
            sum += lap;
            sum2 += lap * lap;
            n++;
        }
    double mu = sum / n;
    return sum2 / n - mu * mu;
}

}  // namespace

int select_sharpest_frames(const std::string& cand_dir,
                           const std::string& out_dir,
                           const std::string& prefix,
                           int group, int max_frames,
                           const std::function<void(const std::string&)>& log,
                           const std::atomic<bool>& cancel) {
    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::directory_iterator it(cand_dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) files.push_back(it->path());
    std::sort(files.begin(), files.end());
    if (files.empty()) return -1;

    group = std::max(group, 1);
    if (max_frames > 0)
        group = std::max<int>(group, (int)((files.size() + max_frames - 1) / max_frames));

    // Score all candidates in parallel.
    std::vector<double> scores(files.size(), -1.0);
    if (group > 1) {
        unsigned n_threads = std::max(1u, std::thread::hardware_concurrency());
        std::atomic<size_t> next{0};
        std::atomic<size_t> done{0};
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < n_threads; t++)
            pool.emplace_back([&] {
                for (;;) {
                    size_t i = next.fetch_add(1);
                    if (i >= files.size() || cancel.load()) return;
                    // A decode that throws would be std::terminate off a
                    // worker thread. An unscored frame just loses its group.
                    try {
                        scores[i] = sharpness_score(files[i].string());
                    } catch (...) {}
                    done.fetch_add(1);
                }
            });
        // Progress line roughly once a second.
        while (done.load() < files.size() && !cancel.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (log) log("scored " + std::to_string(done.load()) + "/" +
                         std::to_string(files.size()) + " candidate frames");
        }
        for (auto& th : pool) th.join();
        if (cancel.load()) return -1;
    }

    // Keep the sharpest per group; move to out_dir, delete the rest.
    fs::create_directories(out_dir, ec);
    int kept = 0;
    for (size_t g0 = 0; g0 < files.size(); g0 += group) {
        size_t g1 = std::min(g0 + (size_t)group, files.size());
        size_t best = g0;
        for (size_t i = g0 + 1; i < g1; i++)
            if (scores[i] > scores[best]) best = i;
        std::string ext = files[best].extension().string();
        char name[64];
        std::snprintf(name, sizeof name, "%s%05d%s", prefix.c_str(), kept,
                      ext.empty() ? ".jpg" : ext.c_str());
        fs::rename(files[best], fs::path(out_dir) / name, ec);
        if (ec) {   // cross-device fallback
            fs::copy_file(files[best], fs::path(out_dir) / name,
                          fs::copy_options::overwrite_existing, ec);
            fs::remove(files[best], ec);
        }
        kept++;
        for (size_t i = g0; i < g1; i++)
            if (i != best) fs::remove(files[i], ec);
        if (cancel.load()) return -1;
    }
    return kept;
}

}  // namespace gui
