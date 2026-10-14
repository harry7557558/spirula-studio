// FrameSelect.cpp -- see FrameSelect.h.

#include "app/gui/FrameSelect.h"

#include "app/FrameSharpness.h"
#include "app/gui/PickSheet.h"
#include "core/ImageFile.h"
#include "i18n/catalog/Log.h"

#include "external/stb_image.h"

#include <algorithm>
#include <cctype>
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
    app::FramePick pick;         // hash and edge width, only where judged
    std::vector<uint8_t> thumb;  // a JPEG, only where judged
};

// What analyze() reads beside the score.
enum class Judge { No, Similar, Blur };

// `mw`/`mh` <= 0 asks for the sharpness only. `top_rows` limits what the grey
// covers, for an EAC canvas whose bottom row is a different half of the sphere.
void analyze(const std::string& path, int mw, int mh, bool top_rows,
             Judge judge, Analysis& out) {
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
    if (judge != Judge::No) {
        app::measure_pick(out.pick, img, W, H, judge == Judge::Blur);
        out.thumb = pick_thumb_jpeg(img, W, H);
    }

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
    app::PickRule rule{options.blur_reject, options.blur_neighbour, options.similar_bits};
    const Judge judge = !rule.on() && !options.judged ? Judge::No
                        : rule.max_blur > 0.0f        ? Judge::Blur
                                                      : Judge::Similar;
    // A few kilobytes a candidate, held until the picks are made: which ones
    // are shown is not known before then, and decoding them again costs more.
    std::vector<app::FramePick> measured(judge != Judge::No ? files.size() : 0);
    std::vector<std::vector<uint8_t>> thumbs(measured.size());
    const bool want_scores = group > 1 || options.adaptive || judge != Judge::No;
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
                                    judge, got[i - base]);
                        } catch (...) {}
                    }
                });
            for (auto& th : pool) th.join();
            if (cancel.load()) return -1;
            for (size_t i = base; i < end; i++) {
                scores[i] = got[i - base].score;
                if (judge != Judge::No) {
                    measured[i] = got[i - base].pick;
                    thumbs[i] = std::move(got[i - base].thumb);
                }
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
    std::vector<size_t> keep, skipped;
    int blurry = 0, similar = 0;
    // Numbered by the candidate it is, not by how many were kept: the stem is
    // what times a frame against the video's IMU, and an adaptive plan leaves
    // nothing evenly spaced for a rate to recover it from.
    auto name_of = [&](size_t i) {
        const std::string ext = files[i].extension().string();
        char name[64];
        std::snprintf(name, sizeof name, "%s%05d%s", prefix.c_str(), (int)i,
                      ext.empty() ? ".jpg" : ext.c_str());
        return std::string(name);
    };
    auto take = [&](size_t g0, size_t g1) {
        size_t best = g1;
        for (size_t i = g0; i < g1; i++)
            if (!taken[first[i]] && (best == g1 || scores[i] > scores[best])) best = i;
        if (best == g1) return;
        taken[first[best]] = true;
        if (judge != Judge::No) {
            app::FramePick p = measured[best];
            p.index = (int64_t)best;
            const bool kept = rule.judge(p) == app::PickVerdict::Kept;
            if (options.judged) {
                const std::string dir = kept ? out_dir : options.skipped_dir;
                options.judged(p, std::move(thumbs[best]),
                               dir.empty() ? std::string()
                                           : (fs::path(dir) / name_of(best)).string());
            }
            if (!kept) {
                ++(p.verdict == app::PickVerdict::Blurry ? blurry : similar);
                skipped.push_back(best);
                return;
            }
        }
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
    auto move_to = [&](size_t i, const std::string& dir) {
        const fs::path to = fs::path(dir) / name_of(i);
        fs::rename(files[i], to, ec);
        if (ec) {   // cross-device fallback
            fs::copy_file(files[i], to, fs::copy_options::overwrite_existing, ec);
            fs::remove(files[i], ec);
        }
        kept_flag[i] = true;
    };
    for (size_t best : keep) {
        move_to(best, out_dir);
        kept++;
        if (cancel.load()) return -1;
    }
    if (!options.skipped_dir.empty() && !skipped.empty()) {
        fs::create_directories(options.skipped_dir, ec);
        for (size_t i : skipped) move_to(i, options.skipped_dir);
    }
    for (size_t i = 0; i < files.size(); i++)
        if (!kept_flag[i]) fs::remove(files[i], ec);
    namespace lmsg = spirula::i18n::msg::log;
    if (blurry > 0 && log)
        log(spirula::i18n::format(lmsg::blurry_skipped, {(long long)blurry}));
    if (similar > 0 && log)
        log(spirula::i18n::format(lmsg::similar_skipped, {(long long)similar}));
    return kept;
}

int judge_image_folder(const std::string& dir, const FrameSelectOptions& options,
                       const std::function<void(const std::string&)>& log,
                       const std::atomic<bool>& cancel) {
    std::error_code ec;
    const fs::path root(dir);
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" ||
            ext == ".tga" || imagefile::handles(it->path().string()))
            files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    app::PickRule rule{options.blur_reject, options.blur_neighbour, options.similar_bits};
    if (files.empty() || (!rule.on() && !options.judged)) return (int)files.size();
    const Judge judge = rule.max_blur > 0.0f ? Judge::Blur : Judge::Similar;

    // Ruled on in file order as each one's turn comes, so a watcher sees them
    // arrive. On half the cores: 30 workers on 33 MP photographs froze the
    // window for 9 s to finish 287 in 12.1 s; 16 took 13.9 s and froze nothing.
    std::vector<Analysis> got(files.size());
    std::unique_ptr<std::atomic<bool>[]> ready(new std::atomic<bool>[files.size()]);
    for (size_t i = 0; i < files.size(); i++) ready[i] = false;
    std::atomic<size_t> next{0};
    const unsigned cores = std::thread::hardware_concurrency();
    const unsigned n_threads = std::max(1u, cores / 2);
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < n_threads; t++)
        pool.emplace_back([&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= files.size() || cancel.load()) return;
                try {
                    analyze(files[i].string(), 0, 0, false, judge, got[i]);
                } catch (...) {}
                ready[i] = true;
            }
        });
    int kept = 0, blurry = 0, similar = 0;
    for (size_t i = 0; i < files.size() && !cancel.load();) {
        if (!ready[i].load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        app::FramePick p = got[i].pick;
        p.index = (int64_t)i;
        const bool keep = rule.judge(p) == app::PickVerdict::Kept;
        fs::path at = files[i];
        if (!keep && options.skipped_dir.empty()) {
            fs::remove(files[i], ec);
        } else if (!keep) {
            at = fs::path(options.skipped_dir) / fs::relative(files[i], root, ec);
            fs::create_directories(at.parent_path(), ec);
            fs::rename(files[i], at, ec);
            if (ec) {   // cross-device fallback
                fs::copy_file(files[i], at, fs::copy_options::overwrite_existing, ec);
                fs::remove(files[i], ec);
            }
        }
        if (options.judged) options.judged(p, std::move(got[i].thumb), at.string());
        if (keep) kept++;
        else ++(p.verdict == app::PickVerdict::Blurry ? blurry : similar);
        got[i] = Analysis{};
        i++;
        if (options.scanning) options.scanning((int64_t)i, (int64_t)files.size());
    }
    for (auto& th : pool) th.join();
    if (cancel.load()) return -1;
    namespace lmsg = spirula::i18n::msg::log;
    if (blurry > 0 && log)
        log(spirula::i18n::format(lmsg::blurry_skipped, {(long long)blurry}));
    if (similar > 0 && log)
        log(spirula::i18n::format(lmsg::similar_skipped, {(long long)similar}));
    return kept;
}

int judge_image_sets(const std::string& dir, const std::vector<std::string>& folders,
                     const FrameSelectOptions& options,
                     const std::function<void(const std::string&)>& log,
                     const std::atomic<bool>& cancel) {
    std::error_code ec;
    const fs::path root(dir);
    if (folders.empty()) return 0;
    std::vector<std::string> names;
    for (fs::directory_iterator it(root / folders[0], ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" ||
            ext == ".tga" || imagefile::handles(it->path().string()))
            names.push_back(it->path().filename().string());
    }
    std::sort(names.begin(), names.end());
    app::PickRule rule{options.blur_reject, options.blur_neighbour, options.similar_bits};
    if (names.empty() || (!rule.on() && !options.judged)) return (int)names.size();
    const Judge judge = rule.max_blur > 0.0f ? Judge::Blur : Judge::Similar;

    // As judge_image_folder: half the cores, ruled on in order as they come.
    const size_t lenses = folders.size(), total = names.size() * lenses;
    std::vector<Analysis> got(total);
    std::unique_ptr<std::atomic<bool>[]> ready(new std::atomic<bool>[total]);
    for (size_t i = 0; i < total; i++) ready[i] = false;
    std::atomic<size_t> next{0};
    const unsigned n_threads = std::max(1u, std::thread::hardware_concurrency() / 2);
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < n_threads; t++)
        pool.emplace_back([&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= total || cancel.load()) return;
                try {
                    analyze((root / folders[i % lenses] / names[i / lenses]).string(), 0, 0,
                            false, judge, got[i]);
                } catch (...) {}
                ready[i] = true;
            }
        });
    int kept = 0, blurry = 0, similar = 0;
    for (size_t n = 0; n < names.size() && !cancel.load();) {
        bool all = true;
        for (size_t k = 0; k < lenses; k++) all = all && ready[n * lenses + k].load();
        if (!all) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        app::FramePick p = got[n * lenses].pick;
        p.index = (int64_t)n;
        for (size_t k = 1; k < lenses; k++)
            p.width = std::max(p.width, got[n * lenses + k].pick.width);
        const bool keep = rule.judge(p) == app::PickVerdict::Kept;
        fs::path first = root / folders[0] / names[n];
        for (size_t k = 0; k < lenses && !keep && !options.skipped_dir.empty(); k++) {
            const fs::path from = root / folders[k] / names[n];
            const fs::path to = fs::path(options.skipped_dir) / folders[k] / names[n];
            fs::create_directories(to.parent_path(), ec);
            fs::rename(from, to, ec);
            if (ec) {   // cross-device fallback
                fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
                fs::remove(from, ec);
            }
            if (k == 0) first = to;
        }
        if (options.judged) options.judged(p, std::move(got[n * lenses].thumb), first.string());
        if (keep) kept++;
        else ++(p.verdict == app::PickVerdict::Blurry ? blurry : similar);
        for (size_t k = 0; k < lenses; k++) got[n * lenses + k] = Analysis{};
        n++;
        if (options.scanning) options.scanning((int64_t)n, (int64_t)names.size());
    }
    for (auto& th : pool) th.join();
    if (cancel.load()) return -1;
    namespace lmsg = spirula::i18n::msg::log;
    if (blurry > 0 && log)
        log(spirula::i18n::format(lmsg::blurry_skipped, {(long long)blurry}));
    if (similar > 0 && log)
        log(spirula::i18n::format(lmsg::similar_skipped, {(long long)similar}));
    return kept;
}

}  // namespace gui
