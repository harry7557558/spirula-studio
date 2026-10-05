// FrameSelect.cpp -- see FrameSelect.h.

#include "app/gui/FrameSelect.h"

#include "app/FrameSharpness.h"
#include "core/ImageFile.h"

#include "external/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

// Candidates loaded ahead of the motion tracker, which has to walk them in
// order. 64 grey frames is a few tens of megabytes whatever the source is.
constexpr size_t kChunk = 64;

struct Analysis {
    double score = -1.0;
    std::vector<uint8_t> grey;   // empty unless the motion pass wants it
};

// `mw`/`mh` <= 0 asks for the sharpness only. `top_rows` limits what the grey
// covers, for an EAC canvas whose bottom row is a different half of the sphere.
void analyze(const std::string& path, int mw, int mh, bool top_rows,
             Analysis& out) {
    int W = 0, H = 0, C = 0;
    std::vector<uint8_t> own_rgb;
    unsigned char* img = nullptr;
    if (imagefile::handles(path)) {
        imagefile::Info info;
        if (!imagefile::decode_srgb8(path, imagefile::Options(), info, own_rgb).empty())
            return;
        W = info.width;
        H = info.height;
        img = own_rgb.data();
    } else {
        img = stbi_load(path.c_str(), &W, &H, &C, 3);
        if (!img) return;
    }

    out.score = app::sharpness_score(img, W, H);

    if (mw > 0 && mh > 0) {
        std::vector<float> f((size_t)mw * mh);
        app::box_grey(img, W, top_rows ? H / 2 : H, mw, mh, f.data());
        out.grey.resize(f.size());
        for (size_t i = 0; i < f.size(); i++)
            out.grey[i] = (uint8_t)std::min(255.0f, std::max(0.0f, f[i] + 0.5f));
    }
    if (own_rgb.empty()) stbi_image_free(img);
}

bool same_bytes(const fs::path& a, const fs::path& b) {
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa || !fb) return false;
    return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(fb), std::istreambuf_iterator<char>());
}

// ffmpeg's fps filter repeats a source frame to reach a rate above the
// source's, and the repeats encode to the same bytes. Each candidate's first
// copy, so a frame is kept once however many groups choose it.
std::vector<size_t> first_copies(const std::vector<fs::path>& files) {
    std::vector<size_t> first(files.size());
    uintmax_t prev = (uintmax_t)-1;
    for (size_t i = 0; i < files.size(); i++) {
        std::error_code ec;
        const uintmax_t size = fs::file_size(files[i], ec);
        first[i] = i;
        if (i > 0 && size != (uintmax_t)-1 && size == prev &&
            same_bytes(files[i - 1], files[i]))
            first[i] = first[i - 1];
        prev = size;
    }
    return first;
}

}  // namespace

int select_sharpest_frames(const std::string& cand_dir,
                           const std::string& out_dir,
                           const std::string& prefix,
                           const FrameSelectOptions& options,
                           const std::function<void(const std::string&)>& log,
                           const std::atomic<bool>& cancel) {
    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::directory_iterator it(cand_dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) files.push_back(it->path());
    std::sort(files.begin(), files.end());
    if (files.empty()) return -1;

    int group = std::max(options.group, 1);
    if (!options.adaptive && options.max_frames > 0)
        group = std::max<int>(
            group, (int)((files.size() + options.max_frames - 1) / options.max_frames));

    // The motion tracker's frames. ffmpeg has already cut the canvas, so what
    // is left of an EAC packing is its top row and of a panorama the whole of it.
    app::MotionOptions mo;
    std::unique_ptr<app::MotionTracker> tracker;
    if (options.adaptive) {
        mo.view = options.view;
        mo.out_fov = options.out_fov;
        int src_w = 0, src_h = 0;
        if (options.eac.valid()) {
            mo.eac = options.eac;
            mo.eac.margin = 0;
            if (options.eac.sphere()) {
                mo.eac.track_w = options.eac.canvasW();
            } else {
                mo.eac.strip = 0;
                mo.eac.track_w = 3 * options.eac.face;
                mo.eac.track_h = options.eac.face;
            }
            src_w = mo.eac.track_w;
            src_h = mo.eac.track_h;
        } else {
            int w = 0, h = 0, c = 0;
            if (stbi_info(files[0].string().c_str(), &w, &h, &c)) {
                src_w = w;
                src_h = h;
            }
        }
        app::motion_frame_size(mo.view, src_w, src_h, mo.width, mo.height);
        if (mo.width > 0) tracker = std::make_unique<app::MotionTracker>(mo);
    }

    // Score, and measure, in chunks: the tracker has to see the frames in
    // order, and everything before it parallelizes.
    std::vector<double> scores(files.size(), -1.0);
    size_t reported = 0;
    const bool want_scores = group > 1 || options.adaptive;
    if (want_scores) {
        const unsigned n_threads = std::max(1u, std::thread::hardware_concurrency());
        auto last_log = std::chrono::steady_clock::now();
        for (size_t base = 0; base < files.size(); base += kChunk) {
            if (cancel.load()) return -1;
            const size_t end = std::min(base + kChunk, files.size());
            std::vector<Analysis> got(end - base);
            std::atomic<size_t> next{base};
            std::vector<std::thread> pool;
            for (unsigned t = 0; t < n_threads; t++)
                pool.emplace_back([&] {
                    for (;;) {
                        const size_t i = next.fetch_add(1);
                        if (i >= end || cancel.load()) return;
                        // A decode that throws would be std::terminate off a
                        // worker thread. An unscored frame loses its group.
                        try {
                            analyze(files[i].string(), tracker ? mo.width : 0,
                                    tracker ? mo.height : 0,
                                    options.eac.valid() && !options.eac.sphere(),
                                    got[i - base]);
                        } catch (...) {}
                    }
                });
            for (auto& th : pool) th.join();
            if (cancel.load()) return -1;
            for (size_t i = base; i < end; i++) {
                scores[i] = got[i - base].score;
                if (!tracker || got[i - base].grey.empty()) continue;
                tracker->track(got[i - base].grey.data(), (int64_t)i);
                if (options.measured)
                    for (; reported < tracker->costs().size(); reported++)
                        options.measured(tracker->ends()[reported],
                                         (int64_t)files.size(),
                                         tracker->costs()[reported]);
            }
            if (options.scanning)
                options.scanning((int64_t)end, (int64_t)files.size());
            const auto now = std::chrono::steady_clock::now();
            if (log && (end == files.size() ||
                        now - last_log > std::chrono::seconds(1))) {
                last_log = now;
                log("scored " + std::to_string(end) + "/" +
                    std::to_string(files.size()) + " candidate frames");
            }
        }
    }

    // Which candidates survive: one per group, or one per equal share of view
    // change with the sharpest of the window around it. What an earlier pick
    // already took, itself or a repeat of it, is not a candidate again.
    const std::vector<size_t> first = first_copies(files);
    std::vector<bool> taken(files.size(), false);
    std::vector<size_t> keep;
    auto take = [&](size_t g0, size_t g1) {
        size_t best = g1;
        for (size_t i = g0; i < g1; i++)
            if (!taken[first[i]] && (best == g1 || scores[i] > scores[best])) best = i;
        if (best == g1) return;
        taken[first[best]] = true;
        keep.push_back(best);
    };
    if (tracker) {
        tracker->finish();
        const int window = std::max(options.window, 1);
        app::MotionPlanInput mi;
        mi.cost = tracker->costs();
        mi.step = tracker->steps();
        mi.ends = tracker->ends();
        mi.frames = (int64_t)files.size();
        mi.skip = group;
        mi.window = window;
        mi.max_frames = options.max_frames;
        const std::vector<std::vector<int64_t>> planned =
            app::plan_by_motion({mi}, options.range);
        const std::vector<int64_t>& plan = planned[0];
        for (int64_t at : plan) {
            const size_t g1 = (size_t)at + 1;
            take(g1 > (size_t)window ? g1 - (size_t)window : 0, g1);
        }
        if (options.planned) options.planned(plan, (int64_t)files.size());
    }
    if (keep.empty())
        for (size_t g0 = 0; g0 < files.size(); g0 += (size_t)group)
            take(g0, std::min(g0 + (size_t)group, files.size()));

    fs::create_directories(out_dir, ec);
    std::vector<bool> kept_flag(files.size(), false);
    int kept = 0;
    for (size_t best : keep) {
        const std::string ext = files[best].extension().string();
        char name[64];
        // Numbered by the candidate it is, not by how many were kept: the stem
        // is what times a frame against the video's IMU, and an adaptive plan
        // leaves nothing evenly spaced for a rate to recover it from.
        std::snprintf(name, sizeof name, "%s%05d%s", prefix.c_str(), (int)best,
                      ext.empty() ? ".jpg" : ext.c_str());
        fs::rename(files[best], fs::path(out_dir) / name, ec);
        if (ec) {   // cross-device fallback
            fs::copy_file(files[best], fs::path(out_dir) / name,
                          fs::copy_options::overwrite_existing, ec);
            fs::remove(files[best], ec);
        }
        kept_flag[best] = true;
        kept++;
        if (cancel.load()) return -1;
    }
    for (size_t i = 0; i < files.size(); i++)
        if (!kept_flag[i]) fs::remove(files[i], ec);
    return kept;
}

}  // namespace gui
