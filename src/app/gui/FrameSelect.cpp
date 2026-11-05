// FrameSelect.cpp -- see FrameSelect.h.

#include "app/gui/FrameSelect.h"

#include "app/FrameSharpness.h"
#include "app/gui/PickSheet.h"
#include "core/HardwareThreads.h"
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
void analyze(const std::string& path, int mw, int mh, bool top_rows, Judge judge,
             app::EdgeMeter* meter, Analysis& out) {
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

    if (judge == Judge::No) {
        out.score = app::sharpness_score(img, W, H);
    } else {
        app::measure_pick(out.pick, img, W, H, judge == Judge::Blur ? meter : nullptr);
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

// What the rule reads off each of `paths`, on `threads` workers. `done` hears
// (finished, all) on the calling thread. Empty when cancelled.
std::vector<Analysis> measure_all(const std::vector<std::string>& paths, Judge judge,
                                  unsigned threads,
                                  const std::function<void(int64_t, int64_t)>& done,
                                  const std::atomic<bool>& cancel) {
    std::vector<Analysis> got(paths.size());
    std::unique_ptr<app::EdgeMeter> meter;
    if (judge == Judge::Blur) meter = std::make_unique<app::EdgeMeter>();
    std::atomic<size_t> next{0}, finished{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, threads); t++)
        pool.emplace_back([&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= paths.size() || cancel.load()) return;
                // A decode that throws would be std::terminate off a worker
                // thread. A frame that cannot be read is kept, unmeasured.
                try {
                    analyze(paths[i], 0, 0, false, judge, meter.get(), got[i]);
                } catch (...) {}
                finished.fetch_add(1);
            }
        });
    while (finished.load() < paths.size() && !cancel.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (done) done((int64_t)finished.load(), (int64_t)paths.size());
    }
    for (auto& th : pool) th.join();
    if (cancel.load()) return {};
    return got;
}

void log_left_out(int blurry, int similar, const std::function<void(const std::string&)>& log) {
    namespace lmsg = spirula::i18n::msg::log;
    if (blurry > 0 && log) log(spirula::i18n::format(lmsg::blurry_skipped, {(long long)blurry}));
    if (similar > 0 && log)
        log(spirula::i18n::format(lmsg::similar_skipped, {(long long)similar}));
}

// `from` to `to`, by copy where a rename cannot cross devices.
void move_file(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::rename(from, to, ec);
    if (ec) {
        ec.clear();
        fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
        if (!ec) fs::remove(from, ec);
    }
}

bool is_picture(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tga" ||
           imagefile::handles(p.string());
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

    // Score in chunks: the tracker has to see the frames in order, and
    // everything before it parallelizes.
    std::vector<double> scores(files.size(), -1.0);
    size_t reported = 0;
    app::PickRule rule{options.blur_reject, options.blur_neighbour, options.similar_bits};
    const Judge judge = !rule.on() && !options.judged ? Judge::No
                        : rule.max_blur > 0.0f        ? Judge::Blur
                                                      : Judge::Similar;
    const unsigned n_threads = std::max(1u, spirula::hardware_threads());
    if (group > 1 || options.adaptive) {
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
                                    options.eac.valid() && !options.eac.sphere(), Judge::No,
                                    nullptr, got[i - base]);
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
    std::vector<size_t> picks;
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
        picks.push_back(best);
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
    if (picks.empty())
        for (size_t g0 = 0; g0 < files.size(); g0 += (size_t)group)
            take(g0, std::min(g0 + (size_t)group, files.size()));

    // The picks alone are measured for the rule, and all of them before the
    // first is ruled on: the capture's own sharpness is what blur is read
    // against (app::whole_capture).
    std::vector<size_t> keep, skipped;
    int blurry = 0, similar = 0;
    if (judge == Judge::No) {
        keep = picks;
    } else {
        std::vector<std::string> paths;
        for (size_t i : picks) paths.push_back(files[i].string());
        std::vector<Analysis> got = measure_all(paths, judge, n_threads, {}, cancel);
        if (cancel.load()) return -1;
        std::vector<app::FramePick> measured;
        for (const Analysis& a : got) measured.push_back(a.pick);
        app::PickRule whole = app::whole_capture(rule, measured);
        for (size_t k = 0; k < picks.size(); k++) {
            app::FramePick p = measured[k];
            p.index = (int64_t)picks[k];
            const bool kept = whole.judge(p) == app::PickVerdict::Kept;
            if (options.judged) {
                const std::string dir = kept ? out_dir : options.skipped_dir;
                options.judged(p, std::move(got[k].thumb),
                               dir.empty() ? std::string()
                                           : (fs::path(dir) / name_of(picks[k])).string());
            }
            if (kept) {
                keep.push_back(picks[k]);
            } else {
                ++(p.verdict == app::PickVerdict::Blurry ? blurry : similar);
                skipped.push_back(picks[k]);
            }
        }
    }

    fs::create_directories(out_dir, ec);
    std::vector<bool> kept_flag(files.size(), false);
    int kept = 0;
    for (size_t best : keep) {
        move_file(files[best], fs::path(out_dir) / name_of(best));
        kept_flag[best] = true;
        kept++;
        if (cancel.load()) return -1;
    }
    for (size_t i : skipped)
        if (!options.skipped_dir.empty()) {
            move_file(files[i], fs::path(options.skipped_dir) / name_of(i));
            kept_flag[i] = true;
        }
    for (size_t i = 0; i < files.size(); i++)
        if (!kept_flag[i]) fs::remove(files[i], ec);
    log_left_out(blurry, similar, log);
    return kept;
}

int judge_image_folder(const std::string& dir, const FrameSelectOptions& options,
                       const std::function<void(const std::string&)>& log,
                       const std::atomic<bool>& cancel) {
    std::error_code ec;
    const fs::path root(dir);
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && is_picture(it->path())) files.push_back(it->path());
    std::sort(files.begin(), files.end());
    app::PickRule rule{options.blur_reject, options.blur_neighbour, options.similar_bits};
    if (files.empty() || (!rule.on() && !options.judged)) return (int)files.size();
    const Judge judge = rule.max_blur > 0.0f ? Judge::Blur : Judge::Similar;

    // On half the cores: 30 workers on 33 MP photographs froze the window for
    // 9 s to finish 287 in 12.1 s; 16 took 13.9 s and froze nothing.
    std::vector<std::string> paths;
    for (const fs::path& f : files) paths.push_back(f.string());
    std::vector<Analysis> got =
        measure_all(paths, judge, spirula::hardware_threads() / 2, options.scanning, cancel);
    if (cancel.load()) return -1;
    std::vector<app::FramePick> measured;
    for (const Analysis& a : got) measured.push_back(a.pick);
    rule = app::whole_capture(rule, measured);

    int kept = 0, blurry = 0, similar = 0;
    for (size_t i = 0; i < files.size(); i++) {
        app::FramePick p = measured[i];
        p.index = (int64_t)i;
        const bool keep = rule.judge(p) == app::PickVerdict::Kept;
        fs::path at = files[i];
        if (!keep && options.skipped_dir.empty()) {
            fs::remove(files[i], ec);
        } else if (!keep) {
            at = fs::path(options.skipped_dir) / fs::relative(files[i], root, ec);
            move_file(files[i], at);
        }
        if (options.judged) options.judged(p, std::move(got[i].thumb), at.string());
        if (keep) kept++;
        else ++(p.verdict == app::PickVerdict::Blurry ? blurry : similar);
    }
    log_left_out(blurry, similar, log);
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
    for (fs::directory_iterator it(root / folders[0], ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && is_picture(it->path()))
            names.push_back(it->path().filename().string());
    std::sort(names.begin(), names.end());
    app::PickRule rule{options.blur_reject, options.blur_neighbour, options.similar_bits};
    if (names.empty() || (!rule.on() && !options.judged)) return (int)names.size();
    const Judge judge = rule.max_blur > 0.0f ? Judge::Blur : Judge::Similar;

    // As judge_image_folder: half the cores. A frame is as blurred as its
    // worst lens and is hashed and shown by its first.
    const size_t lenses = folders.size();
    std::vector<std::string> paths;
    for (const std::string& n : names)
        for (const std::string& f : folders) paths.push_back((root / f / n).string());
    const std::function<void(int64_t, int64_t)> scanning = options.scanning;
    std::vector<Analysis> got = measure_all(
        paths, judge, spirula::hardware_threads() / 2,
        [&](int64_t done, int64_t) {
            if (scanning) scanning(done / (int64_t)lenses, (int64_t)names.size());
        },
        cancel);
    if (cancel.load()) return -1;
    std::vector<app::FramePick> measured(names.size());
    for (size_t n = 0; n < names.size(); n++) {
        measured[n] = got[n * lenses].pick;
        for (size_t k = 1; k < lenses; k++)
            measured[n].width = std::max(measured[n].width, got[n * lenses + k].pick.width);
    }
    rule = app::whole_capture(rule, measured);

    int kept = 0, blurry = 0, similar = 0;
    for (size_t n = 0; n < names.size(); n++) {
        app::FramePick p = measured[n];
        p.index = (int64_t)n;
        const bool keep = rule.judge(p) == app::PickVerdict::Kept;
        fs::path first = root / folders[0] / names[n];
        for (size_t k = 0; k < lenses && !keep; k++) {
            const fs::path from = root / folders[k] / names[n];
            if (options.skipped_dir.empty()) {
                fs::remove(from, ec);
                continue;
            }
            const fs::path to = fs::path(options.skipped_dir) / folders[k] / names[n];
            move_file(from, to);
            if (k == 0) first = to;
        }
        if (options.judged) options.judged(p, std::move(got[n * lenses].thumb), first.string());
        if (keep) kept++;
        else ++(p.verdict == app::PickVerdict::Blurry ? blurry : similar);
    }
    log_left_out(blurry, similar, log);
    return kept;
}

}  // namespace gui
