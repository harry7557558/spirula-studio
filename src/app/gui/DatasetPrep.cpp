// DatasetPrep.cpp -- see DatasetPrep.h.

#include "app/gui/DatasetPrep.h"

#include "app/gui/ReconStamp.h"
#include "app/gui/mask/MaskLayer.h"
#include "sfm/core/Resume.h"

#include "i18n/catalog/Log.h"
#include "i18n/catalog/MaskEdit.h"

#include "app/gui/FrameSelect.h"
#include "app/gui/Subprocess.h"

#include "app_generated/mask_py.h"   // kMaskPy[], from reference/scripts/mask.py

#include "core/ExrImage.h"
#include "data/DatasetColor.h"
#include "core/ImageOrient.h"
#include "sfm/core/Exif.h"
#include "sfm/core/Telemetry.h"
#include "external/stb_image.h"      // stbi_info (image size probe), stbi_load
#include "external/stb_image_write.h"  // stbi_write_jpg (the photo re-encode)

#ifdef SS_BUILD_SAM
#include "app/WriterPool.h"
#include "nn/Device.h"
#include "nn/io/Image.h"
#include "sam/Masking.h"
#endif
#ifdef SS_HAVE_VIDEO
#include "app/FrameExtract.h"
#include "video/Video.h"
#endif

#ifndef _WIN32
#include <ftw.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;
namespace mmsg = spirula::i18n::msg::maskedit;

// Shorthand: most log lines here carry a path or a count, so they are
// format() calls. See i18n/Message.h on why they are whole sentences with
// {0} placeholders rather than concatenated pieces.
inline std::string fmt(const spirula::i18n::Msg& m,
                       std::initializer_list<spirula::i18n::Arg> a) {
    return spirula::i18n::format(m, a);
}

namespace gui {

const char* const kVideoExtensions[kNumVideoExtensions] = {
    ".mp4", ".mov", ".mkv", ".webm", ".m4v", ".insv", ".osv", ".avi",
    ".mts", ".m2ts", ".360", ".ts", ".wmv", ".lrv",
};

bool is_image_file(const fs::path& p) {
    std::string e = p.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".webp" ||
           e == ".tif" || e == ".tiff" || e == ".bmp" || e == ".exr" ||
           e == ".insp";
}

namespace {

// NOT std::filesystem::remove_all -- on the torch build libtorch.so interposes
// an ABI-incompatible copy (see AGENTS.md gotchas).
void remove_tree(const fs::path& p) {
#ifndef _WIN32
    nftw(p.string().c_str(),
         [](const char* f, const struct stat*, int, struct FTW*) {
             return ::remove(f);
         }, 16, FTW_DEPTH | FTW_PHYS);
#else
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
#endif
}

// Candidates ffmpeg resamples per frame kept. Adaptive selection picks from
// them, so there have to be enough for the fastest rate it may ask for.
int candidate_group(const PrepJob& job, const PrepInput& in) {
    if (every_frame(job, in)) return 1;
    const int window = std::max(job.sharp_window, 1);
    return job.adaptive_fps ? std::max(window, (int)std::ceil(job.adaptive_range))
                            : window;
}

// How ffmpeg is told to write every decoded frame exactly once: the image2
// muxer is constant-rate by default, and pads or drops a variable-rate file to fit.
void append_every_frame_args(std::vector<std::string>& argv,
                             const std::string& ffmpeg_exe, int max_frames) {
    const std::vector<std::string> pass = passthrough_args(ffmpeg_exe);
    argv.insert(argv.end(), pass.begin(), pass.end());
    if (max_frames > 0) argv.insert(argv.end(), {"-frames:v", std::to_string(max_frames)});
}

// The count on one of ffmpeg's `frame=  123 fps=...` progress lines; -1 otherwise.
long long progress_frame(const std::string& line) {
    const size_t at = line.find_first_not_of(" \t");
    if (at == std::string::npos || line.compare(at, 6, "frame=") != 0) return -1;
    const char* first = line.c_str() + at + 6;
    char* end = nullptr;
    const long long frame = std::strtoll(first, &end, 10);
    return end == first ? -1 : frame;
}

// One argument's ceiling: ~17 bytes a keeper, so about 60k keepers on macOS.
#if defined(_WIN32)
constexpr size_t kMaxArgBytes = 30000;    // CreateProcess: 32767 for the whole line
#elif defined(__APPLE__)
constexpr size_t kMaxArgBytes = 900000;   // ARG_MAX 1 MiB, shared with the environment
#else
constexpr size_t kMaxArgBytes = 130000;   // MAX_ARG_STRLEN: 128 KiB for one argument
#endif

// `select` true on frames keep[lo, hi), as a balanced sum: ffmpeg's expression
// parser fails ("Cannot allocate memory") on a flat sum of more than 100 terms.
std::string select_expr(const std::vector<long long>& keep, size_t lo, size_t hi) {
    if (hi - lo == 1) return "eq(n\\," + std::to_string(keep[lo]) + ")";
    const size_t mid = lo + (hi - lo) / 2;
    return "(" + select_expr(keep, lo, mid) + "+" + select_expr(keep, mid, hi) + ")";
}

// Throw away what a previous run generated, for a step being re-done. Only
// under the workspace: photos read where they are belong to the user, and a
// re-run must not be able to delete the capture.
void clear_generated(const fs::path& dir, const fs::path& workspace) {
    std::error_code ec;
    const fs::path d = fs::weakly_canonical(dir, ec);
    const fs::path w = fs::weakly_canonical(workspace, ec);
    if (d == w || w.empty()) return;
    for (fs::path up = d.parent_path(); !up.empty() && up != up.root_path();
         up = up.parent_path())
        if (up == w) { remove_tree(d); return; }
}

// Every walk over an image tree follows directory symlinks. A prepared capture
// whose images/ and masks/ are links into the raw one is an ordinary layout,
// and the default iterator returns nothing at all for it -- which reads as "no
// images here" rather than as "not looked".
constexpr fs::directory_options kWalk =
    fs::directory_options::skip_permission_denied |
    fs::directory_options::follow_directory_symlink;

// Images under `dir`, sorted, never descending into `skip` (see count_images).
std::vector<fs::path> walk_images(const fs::path& dir, const fs::path& skip = {}) {
    std::vector<fs::path> out;
    std::error_code ec;
    const bool guard = !skip.empty() && fs::is_directory(skip, ec);
    for (fs::recursive_directory_iterator it(dir, kWalk, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (guard && it->is_directory(ec) &&
            fs::equivalent(it->path(), skip, ec)) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && is_image_file(it->path()))
            out.push_back(it->path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Where a file sits under the tree it was walked from. Lexical on purpose:
// fs::relative resolves symlinks, and an images/ of links into a raw capture
// then relativizes to "../../<capture>/..." -- which sent every generated mask
// into the folder the run was only asked to read.
fs::path under_root(const fs::path& file, const fs::path& root) {
    fs::path rel = file.lexically_relative(root);
    if (rel.empty() || *rel.begin() == "..") rel = file.filename();
    return rel;
}

// Lexical for the same reason: does this folder sit inside that one?
bool inside(const fs::path& p, const fs::path& root) {
    const fs::path rel = p.lexically_relative(root);
    return !rel.empty() && *rel.begin() != "..";
}

// Frames an input the job no longer names left in the dataset's images/: they
// would be trained on beside the new ones, and the colour record says nothing
// of them. Never a folder an input is read from.
struct Pruned {
    std::vector<std::string> removed, stuck;
    std::vector<std::pair<std::string, std::string>> links;   // name, target
};

// `own_frames_kept`: the frames are not being re-extracted, so a lone input's
// own root frames and camera folders stay; only other folders go.
Pruned prune_removed_inputs(const fs::path& images, const fs::path& ws,
                            const std::vector<PrepInput>& inputs, bool own_frames_kept) {
    Pruned out;
    std::set<std::string> kept;
    const PrepInput* lone = nullptr;
    std::error_code ec;
    for (const PrepInput& in : inputs) {
        const fs::path src = fs::absolute(in.path, ec).lexically_normal();
        if (inside(src, images) || inside(images, src)) return out;
        // A lone input owns images/ itself.
        if (in.subdir.empty()) lone = &in;
        else kept.insert(fs::path(in.subdir).begin()->string());
    }
    if (lone && own_frames_kept && !lone->is_video)
        for (const std::string& cam : camera_subfolders(lone->path))
            kept.insert(fs::path(cam).begin()->string());
    auto camera_dir = [](const std::string& n) {
        return n.size() > 3 && n.compare(0, 3, "cam") == 0 &&
               n.find_first_not_of("0123456789", 3) == std::string::npos;
    };
    std::vector<fs::path> gone;
    for (fs::directory_iterator it(images, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (kept.count(name)) continue;
        if (lone && own_frames_kept) {
            const fs::file_status st = it->symlink_status(ec);
            if (!fs::is_directory(st) && !fs::is_symlink(st)) continue;
            if (lone->is_video && camera_dir(name)) continue;
        }
        gone.push_back(it->path());
    }
    for (const fs::path& p : gone) {
        const std::string name = p.filename().string();
        if (fs::is_symlink(fs::symlink_status(p, ec))) {
            // The link, never what it points at.
            const std::string target = fs::read_symlink(p, ec).string();
            fs::remove(p, ec);
            if (!fs::exists(fs::symlink_status(p, ec))) out.links.push_back({name, target});
            else out.stuck.push_back(name);
            continue;
        }
        if (fs::is_directory(p, ec)) clear_generated(p, ws);
        else fs::remove(p, ec);
        if (!fs::exists(fs::symlink_status(p, ec))) out.removed.push_back(name);
        else out.stuck.push_back(name);
    }
    return out;
}

// The frame number the masker should be given for each file, taken from the
// digits the extractors end a frame's name with. Only when the whole list is
// one camera: cam0/ and cam1/ restart the numbering, and a frame id that goes
// backwards re-triggers seeds that have already been applied.
bool frame_ids_from_stems(const std::vector<fs::path>& files,
                          std::vector<int64_t>& ids) {
    ids.clear();
    if (files.empty()) return false;
    const fs::path dir = files.front().parent_path();
    for (const fs::path& f : files) {
        if (f.parent_path() != dir) return false;
        const std::string stem = f.stem().string();
        size_t b = stem.size();
        while (b > 0 && std::isdigit((unsigned char)stem[b - 1])) b--;
        if (b == stem.size()) return false;
        ids.push_back(std::strtoll(stem.c_str() + b, nullptr, 10));
    }
    return true;
}

#ifdef SS_BUILD_SAM
// Preview clicks -> the masker's seeds, against the list it will walk
// (`cameras` per file, `ids` its frame number). A click resolves within its
// OWN camera: the same fraction through a 360 plan lands in another view.
std::vector<sam::SeedPrompt> seeds_from_clicks(const std::vector<MaskClick>& clicks,
                                               const std::vector<std::string>& cameras,
                                               const std::vector<int64_t>& ids,
                                               bool exact) {
    std::vector<sam::SeedPrompt> seeds;
    for (const MaskClick& c : clicks) {
        std::vector<size_t> mine;
        for (size_t i = 0; i < ids.size(); i++)
            if (i >= cameras.size() || cameras[i] == c.camera) mine.push_back(i);
        // A capture the run split differently from what the panel offered:
        // the whole list beats dropping the prompt on the floor.
        if (mine.empty())
            for (size_t i = 0; i < ids.size(); i++) mine.push_back(i);

        int64_t frame = c.frame;
        if (!exact && !mine.empty()) {
            const double at = std::min(1.0, std::max(0.0, (double)c.position)) *
                              (double)(mine.size() - 1);
            frame = ids[mine[(size_t)std::llround(at)]];
        }
        sam::SeedPrompt* seed = nullptr;
        for (sam::SeedPrompt& s : seeds)
            if (s.object == c.object && s.frame == frame) seed = &s;
        if (!seed) {
            seeds.push_back({});
            seed = &seeds.back();
            seed->object = c.object;
            seed->frame = frame;
        }
        if (c.positive) seed->prompt.pos_points.push_back({c.x, c.y});
        else            seed->prompt.neg_points.push_back({c.x, c.y});
    }
    return seeds;
}

// Reading a frame is ~10-20 ms of CPU that the model it feeds does not need
// the CPU for. One thread ahead of the masking loop hides all of it, which is
// what pays for masking the written frames instead of the decoded ones.
class ImagePrefetch {
public:
    ImagePrefetch(std::vector<fs::path> files, const std::atomic<bool>& cancel,
                  std::string gamut, std::optional<bool> is_linear)
        : _files(std::move(files)), _cancel(cancel), _gamut(std::move(gamut)),
          _is_linear(is_linear), _worker([this] { run(); }) {}
    ~ImagePrefetch() {
        {
            std::lock_guard<std::mutex> lk(_mu);
            _stop = true;
        }
        _space.notify_all();
        _worker.join();
    }
    ImagePrefetch(const ImagePrefetch&) = delete;
    ImagePrefetch& operator=(const ImagePrefetch&) = delete;

    // The next file, the way up the model wants it, `turn` saying what to
    // undo on the mask. Empty when it could not be read or the reader stopped.
    // What the reader threw is rethrown here, the thread that can report it.
    nn::Image take(sfm::ExifTransform& turn) {
        std::unique_lock<std::mutex> lk(_mu);
        _ready.wait(lk, [this] { return !_queue.empty() || _done; });
        if (_queue.empty()) {
            if (_err) std::rethrow_exception(_err);
            return nn::Image();
        }
        Item item = std::move(_queue.front());
        _queue.pop_front();
        _space.notify_one();
        turn = item.turn;
        return std::move(item.img);
    }

private:
    // Two in hand plus the one being masked: a 4K frame is ~25 MB, and the
    // depth buys nothing beyond covering one decode.
    static constexpr size_t kDepth = 2;

    struct Item {
        nn::Image img;
        sfm::ExifTransform turn;
    };

    void run() {
        try {
            for (const fs::path& f : _files) {
                if (_cancel.load()) break;
                Item item;
                item.img = app::load_upright(f.string(), _gamut, _is_linear,
                                             item.turn);
                std::unique_lock<std::mutex> lk(_mu);
                _space.wait(lk, [this] { return _queue.size() < kDepth || _stop; });
                if (_stop) break;
                _queue.push_back(std::move(item));
                _ready.notify_one();
            }
        } catch (...) {
            std::lock_guard<std::mutex> lk(_mu);
            _err = std::current_exception();
        }
        {
            std::lock_guard<std::mutex> lk(_mu);
            _done = true;
        }
        _ready.notify_all();
    }

    std::vector<fs::path> _files;
    const std::atomic<bool>& _cancel;
    std::string _gamut;
    std::optional<bool> _is_linear;
    std::deque<Item> _queue;
    std::mutex _mu;
    std::condition_variable _ready, _space;
    bool _stop = false;
    // The reader is gone. Without it a take() the reader cancelled out from
    // under waits on a queue nothing will ever fill again.
    bool _done = false;
    std::exception_ptr _err;
    std::thread _worker;
};

// One input's stencil, resolved per camera folder and rasterized once per
// frame size, so a generated mask can be intersected with it in memory.
class StencilRaster {
public:
    using ReportFn = std::function<void(const std::string& camera,
                                        const app::BorderDetect&)>;

    void build(const app::FrameStencil& st, const fs::path& root,
               const std::vector<fs::path>& files, const ReportFn& report) {
        std::map<std::string, std::vector<std::string>> groups;
        std::error_code ec;
        for (const fs::path& f : files) {
            groups[under_root(f, root).parent_path().generic_string()]
                .push_back(f.string());
        }
        for (auto& [camera, group] : groups) {
            app::FrameMask fm = st.mask;
            app::BorderDetect border;
            if (st.detect_border) {
                app::BorderDetectOptions o;
                o.shrink = st.shrink;
                border = app::detect_fisheye_border(group, o);
                // First, so the shapes drawn on top are applied to it in order.
                if (border.found) fm.shapes.insert(fm.shapes.begin(), border.shape);
            }
            if (report) report(camera, border);
            if (!fm.empty()) _fm[camera] = std::move(fm);
        }
    }

    bool apply(const fs::path& file, const fs::path& root, sam::Mask& mask,
               std::string& error) {
        if (_fm.empty() || mask.data.empty()) return true;
        std::error_code ec;
        const auto it =
            _fm.find(under_root(file, root).parent_path().generic_string());
        if (it == _fm.end()) return true;

        Key key{it->first, mask.width, mask.height};
        auto cached = _cache.find(key);
        if (cached == _cache.end()) {
            std::vector<uint8_t> px;
            if (!app::rasterize_frame_mask(it->second, mask.width, mask.height,
                                           px, error))
                return false;
            cached = _cache.emplace(key, std::move(px)).first;
        }
        const std::vector<uint8_t>& px = cached->second;
        for (size_t i = 0; i < mask.data.size() && i < px.size(); i++)
            if (!px[i]) mask.data[i] = 0;
        return true;
    }

private:
    struct Key {
        std::string camera;
        int w, h;
        bool operator<(const Key& o) const {
            if (camera != o.camera) return camera < o.camera;
            if (w != o.w) return w < o.w;
            return h < o.h;
        }
    };
    std::map<std::string, app::FrameMask> _fm;
    std::map<Key, std::vector<uint8_t>> _cache;
};
#endif

std::string lower_ext(const std::string& path) {
    std::string e = fs::path(path).extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

// base / sub, where an empty `sub` means base itself (and not "base/").
fs::path under(const std::string& base, const std::string& sub) {
    return sub.empty() ? fs::path(base) : fs::path(base) / sub;
}

// The clicks that prompt this input; an unnamed source is the single-input case.
std::vector<MaskClick> clicks_for(const PrepJob& job, const PrepInput& in) {
    std::vector<MaskClick> out;
    for (const MaskClick& c : job.mask_clicks)
        if (c.source.empty() || c.source == in.path) out.push_back(c);
    return out;
}

std::string round_to(double v, int decimals) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", decimals, v);
    return b;
}

std::string human_duration(double seconds) {
    if (seconds < 1.0) return lmsg::dur_moment.get();
    if (seconds < 90.0) return fmt(lmsg::dur_seconds, {round_to(seconds, 0)});
    if (seconds < 5400.0)
        return fmt(lmsg::dur_minutes, {round_to(seconds / 60.0, 0)});
    return fmt(lmsg::dur_hours, {round_to(seconds / 3600.0, 1)});
}

// The adaptive pass writes nothing, so the step's tally cannot move and a long
// video would sit on an unchanged bar. It shows its own share of `frames`
// instead -- the whole group, which is measured before any of it is written.
class ScanProgress {
public:
    using Clock = std::chrono::steady_clock;

    ScanProgress(RunProgress* prog, int64_t frames)
        : _prog(prog), _total(frames), _last(Clock::now()) {}

    // The step has to be RUNNING or the panel draws no bar at all: nothing has
    // entered Frames yet when a group is being measured. The same line goes to
    // the log, which is the other place a user looks for signs of life.
    void begin(std::string name) {
        _name = std::move(name);
        _prog->enter(Stage::Frames, line(_base));
        _prog->fraction(Stage::Frames, share(_base));
        // Only once the length is known: the first update() brings it where the
        // caller could not, and "0 of 0" is not a sign of life.
        if (_total > 0) _prog->note(Stage::Frames, line(_base), /*detail=*/false);
    }

    void update(int64_t done, int64_t total) {
        const int64_t at = _total > 0 ? _base + done : done;
        if (_total <= 0) _total = total;
        _prog->fraction(Stage::Frames, share(at));
        const auto now = Clock::now();
        if (now - _last < std::chrono::milliseconds(150)) return;
        _last = now;
        _prog->detail(Stage::Frames, line(at));
    }

    void finish(int64_t frames) { _base += frames; }

private:
    float share(int64_t at) const {
        return _total > 0 ? (float)std::min(1.0, (double)at / (double)_total)
                          : -1.0f;
    }
    std::string line(int64_t at) const {
        return fmt(lmsg::scanning_motion,
                   {(long long)at, (long long)_total, _name});
    }

    RunProgress* _prog;
    int64_t _total = 0, _base = 0;
    std::string _name;
    Clock::time_point _last;
};

// A kept-frame plan as a strip: the local rate at each slice of the capture,
// against the fastest. Read off the gaps rather than counted per slice, so a
// plan of forty frames is a curve and not forty ticks.
std::vector<float> spacing_bars(const std::vector<int64_t>& plan, int64_t frames) {
    // Sliced exactly as the measured curve is, since the panel draws the two
    // against one another: a strip of its own width would end short of it.
    if (plan.size() < 2 || frames <= 0) return {};
    std::vector<float> bars((size_t)kScanSlices, 0.0f);
    size_t k = 0;
    for (int b = 0; b < kScanSlices; b++) {
        const int64_t at = (int64_t)(((double)b + 0.5) * frames / kScanSlices);
        while (k + 2 < plan.size() && plan[k + 1] < at) k++;
        bars[(size_t)b] = 1.0f / (float)std::max<int64_t>(1, plan[k + 1] - plan[k]);
    }
    const float top = *std::max_element(bars.begin(), bars.end());
    if (top > 0.0f)
        for (float& v : bars) v /= top;
    return bars;
}

// Progress that answers "how long is this going to take", which is the only
// question a user has during a twenty-minute masking pass, and the one a
// counter that ticks every tenth frame does not answer.
//
// Rate-limited by wall clock rather than by a frame count: the same call site
// serves a decode running at a thousand frames a second and a segmentation
// running at one every two seconds.
//
// Counts are reported against the whole step's tally, so what is on screen is
// the run's position through every input rather than through this one.
class RateLimitedProgress {
public:
    using Clock = std::chrono::steady_clock;

    RateLimitedProgress(RunProgress* prog, Stage stage,
                        const spirula::i18n::Msg& noun, StageTally& tally)
        : _prog(prog), _stage(stage), _noun(&noun), _tally(&tally),
          _base(tally.done), _start(Clock::now()), _last(_start) {}

    void update(int64_t in_segment, bool force = false) {
        const auto now = Clock::now();
        // Never backwards: an input that reports nothing still leaves the bar
        // where the last one left it.
        _tally->done = std::max(_tally->done, _base + in_segment);
        const int64_t done = _tally->done;
        _total = _tally->total;
        // The counter itself is cheap and drives the bar, so it is not
        // rate-limited; only the sentence built from it is.
        _prog->count(_stage, done, _total);
        const double since =
            std::chrono::duration<double>(now - _last).count();
        if (!force && (done == _reported || since < 2.0)) return;
        _reported = done;
        _last = now;
        // The rate is measured from the first item, not from the start: the
        // several seconds a checkpoint takes to reach the GPU would otherwise
        // be spread over every frame and put the first estimate out by 3x.
        if (_anchor_done < 0) {
            _anchor_done = done;
            _start = now;
        }
        const double elapsed = std::chrono::duration<double>(now - _start).count();
        const int64_t measured = done - _anchor_done;
        // One whole sentence per shape, never a stem with clauses appended:
        // the rate and the estimate sit in different places in a verb-final
        // language, and "about ... left" cannot be glued onto a Japanese noun
        // phrase and still parse.
        const std::string noun = _noun->get();
        if (measured <= 0 || elapsed <= 0.5) {
            say(_total > 0 ? fmt(lmsg::prog_count_total, {noun, done, _total})
                           : fmt(lmsg::prog_count, {noun, done}));
            return;
        }
        const double per = elapsed / (double)measured;
        const std::string rate =
            per >= 0.5 ? fmt(lmsg::rate_each, {round_to(per, 1)})
                       : fmt(lmsg::rate_per_second, {round_to(1.0 / per, 0)});
        if (_total <= 0)
            say(fmt(lmsg::prog_count_rate, {noun, done, rate}));
        else if (_total > done)
            say(fmt(lmsg::prog_count_total_rate_eta,
                    {noun, done, _total, rate,
                     human_duration(per * (double)(_total - done))}));
        else
            say(fmt(lmsg::prog_count_total_rate, {noun, done, _total, rate}));
    }

private:
    // The same sentence in both places: under the step's bar, where it is the
    // answer to "how long", and in the log, which keeps the history.
    void say(const std::string& s) {
        _prog->detail(_stage, s);
        _prog->note(_stage, s, /*detail=*/true);
    }

    RunProgress* _prog;
    Stage _stage;
    const spirula::i18n::Msg* _noun;
    StageTally* _tally;
    int64_t     _base = 0;           // the tally when this input started
    int64_t     _total = 0;
    int64_t     _reported = -1;
    int64_t     _anchor_done = -1;   // count at the first report; see update()
    Clock::time_point _start, _last;
};

// One frame is written every this many source frames, from the kept frame rate
// this input asked for and what the container says it holds.
int frame_skip(float fps, double src_fps) {
    if (!(fps > 0.0f)) return 1;
    return std::max(1, (int)std::lround(src_fps / std::max(fps, 0.01f)));
}

// What a video is expected to yield, for the step's bar. The extraction loop
// stops on the real end of stream either way.
int64_t expected_frames(const PrepJob& job, float fps, double src_fps,
                        int64_t src_frames, int tracks) {
    int64_t expect = src_frames > 0
                         ? (src_frames / frame_skip(fps, src_fps)) * (int64_t)tracks
                         : 0;
    if (job.max_frames > 0 &&
        (expect == 0 || expect > (int64_t)job.max_frames * tracks))
        expect = (int64_t)job.max_frames * tracks;
    return expect;
}

}  // namespace

bool is_video_path(const std::string& path) {
    const std::string e = lower_ext(path);
    for (const char* v : kVideoExtensions)
        if (e == v) return true;
    return false;
}

bool is_dual_fisheye_path(const std::string& path) {
    return lower_ext(path) == ".insv" || lower_ext(path) == ".osv";
}

bool is_pano360_path(const std::string& path) {
    return lower_ext(path) == ".360";
}

bool is_packed_lens_path(const std::string& path) {
    return lower_ext(path) == ".insp" || lower_ext(path) == ".lrv";
}

int probe_packed_lenses(const std::string& dir) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, kWalk, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || lower_ext(it->path().string()) != ".insp")
            continue;
        int w = 0, h = 0, ch = 0;
        if (!stbi_info(it->path().string().c_str(), &w, &h, &ch)) continue;
        bool exact = false;
        return app::packed_lens_count(w, h, exact);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// The ffmpeg fallback, on its own
// ---------------------------------------------------------------------------

bool ffmpeg_probe_video(const std::string& ffmpeg_exe, const std::string& path,
                        VideoFacts& out, const std::atomic<bool>& cancel) {
    out = VideoFacts{};
    if (!command_exists(ffmpeg_exe)) return false;
    // No output file, so ffmpeg prints the container and stream table and then
    // exits non-zero saying it was given nothing to write -- which is why the
    // return code is not the answer here, the two lines below are. ffprobe
    // would be tidier and is not assumed to be installed: only the ffmpeg path
    // is a setting (Tool locations), and a user who set one did not promise
    // the other is beside it.
    run_process({ffmpeg_exe, "-nostdin", "-hide_banner", "-i", path}, "",
                [&](const std::string& line) {
                    const size_t d = line.find("Duration:");
                    if (d != std::string::npos) {
                        int hh = 0, mm = 0;
                        double ss = 0.0;
                        if (std::sscanf(line.c_str() + d, "Duration: %d:%d:%lf",
                                        &hh, &mm, &ss) == 3)
                            out.duration = hh * 3600.0 + mm * 60.0 + ss;
                    }
                    // "... 1920x1080, 19938 kb/s, 30.01 fps, 30 tbr, ..."
                    // A cover picture is a video stream to ffmpeg and is not a
                    // lens; counting it gives a DJI .osv three of them.
                    const size_t f = line.find(" fps");
                    if (f == std::string::npos ||
                        line.find("Video:") == std::string::npos ||
                        line.find("(attached pic)") != std::string::npos)
                        return;
                    // The frame size off the same line. The 16-pixel floor is
                    // what rejects the fourcc ("0x31637661"), which is also
                    // digits on both sides of an x.
                    int lw = 0, lh = 0;
                    for (size_t i = 1; i + 1 < line.size() && !lw; i++) {
                        if (line[i] != 'x') continue;
                        size_t b = i, e = i + 1;
                        while (b > 0 && std::isdigit((unsigned char)line[b - 1])) b--;
                        while (e < line.size() && std::isdigit((unsigned char)line[e])) e++;
                        if (b == i || e == i + 1) continue;
                        const int w = std::atoi(line.c_str() + b);
                        const int h = std::atoi(line.c_str() + i + 1);
                        if (w >= 16 && h >= 16) { lw = w; lh = h; }
                    }
                    if (lw > 0) {
                        out.tracks.emplace_back(lw, lh);
                        if (out.width == 0) { out.width = lw; out.height = lh; }
                    }
                    size_t b = f;
                    while (b > 0 && (std::isdigit((unsigned char)line[b - 1]) ||
                                     line[b - 1] == '.'))
                        b--;
                    if (b < f) {
                        try {
                            out.fps = std::stod(line.substr(b, f - b));
                        } catch (...) {}
                    }
                },
                cancel);
    if (out.duration > 0.0 && out.fps > 0.0)
        out.frames = (long long)(out.duration * out.fps);
    return out.duration > 0.0;
}

std::vector<std::string> passthrough_args_for(const std::string& version_line) {
    int major = -1, minor = 0;
    const size_t at = version_line.find("version ");
    if (at != std::string::npos) {
        const char* p = version_line.c_str() + at + 8;
        if (*p == 'n') p++;   // a release tag, "n7.0"; "N-1234-g..." is a git build
        if (std::sscanf(p, "%d.%d", &major, &minor) < 1) major = -1;
    }
    const bool old = major >= 0 && (major < 5 || (major == 5 && minor < 1));
    return {old ? "-vsync" : "-fps_mode", "passthrough"};
}

std::vector<std::string> passthrough_args(const std::string& ffmpeg_exe) {
    static std::mutex m;
    static std::map<std::string, std::vector<std::string>> known;
    {
        std::lock_guard<std::mutex> lock(m);
        const auto it = known.find(ffmpeg_exe);
        if (it != known.end()) return it->second;
    }
    std::string first;
    const std::atomic<bool> never{false};
    run_process({ffmpeg_exe, "-hide_banner", "-version"}, "",
                [&first](const std::string& line) {
                    if (first.empty()) first = line;
                }, never);
    std::vector<std::string> args = passthrough_args_for(first);
    std::lock_guard<std::mutex> lock(m);
    known[ffmpeg_exe] = args;
    return args;
}

bool ffmpeg_extract_frame(const std::string& ffmpeg_exe, const std::string& video,
                          double seconds, const std::string& out_path,
                          const std::atomic<bool>& cancel,
                          const FfmpegStillOpts& opts) {
    if (!command_exists(ffmpeg_exe)) return false;
    std::error_code ec;
    fs::remove(out_path, ec);
    char ts[32];
    std::snprintf(ts, sizeof ts, "%.3f", seconds > 0.0 ? seconds : 0.0);
    // -ss before -i: seek first, decode one frame, stop. The other order
    // decodes the whole file up to that point, which on a ten-minute capture
    // is the difference between a preview and a coffee break.
    std::vector<std::string> argv{ffmpeg_exe, "-nostdin", "-y"};
    // A 360 capture's geometry is the EAC layout, not the display matrix.
    if (!opts.auto_rotate || opts.eac.valid()) argv.push_back("-noautorotate");
    argv.insert(argv.end(), {"-ss", ts, "-i", video});
    if (opts.eac.valid())
        argv.insert(argv.end(), {"-filter_complex",
                                 app::pano360_graph(opts.eac, ""), "-map",
                                 std::string("[") + app::pano360_canvas_pad() + "]"});
    else if (opts.track > 0)
        argv.insert(argv.end(), {"-map", "0:v:" + std::to_string(opts.track)});
    argv.insert(argv.end(), {"-frames:v", "1", "-q:v", "2", out_path});
    const int rc = run_process(argv, "", [](const std::string&) {}, cancel);
    if (rc != 0) {
        fs::remove(out_path, ec);
        return false;
    }
    return fs::exists(out_path, ec) && fs::file_size(out_path, ec) > 0;
}

int probe_video_tracks(const std::string& ffmpeg_exe, const std::string& path,
                       const std::atomic<bool>& cancel) {
#ifdef SS_HAVE_VIDEO
    {
        std::string err;
        const std::vector<std::pair<int, int>> tracks = app::video_track_sizes(path, err);
        if (!tracks.empty()) return (int)tracks.size();
    }
#endif
    VideoFacts facts;
    if (ffmpeg_probe_video(ffmpeg_exe, path, facts, cancel)) return (int)facts.tracks.size();
    return 0;
}

std::vector<std::string> lens_dirs(const PrepJob& job, const PrepInput& in) {
    std::vector<std::string> out;
    for (int k = 0; in.packed_lenses >= 2 && k < in.packed_lenses; k++)
        out.push_back("cam" + std::to_string(k));
    if (!out.empty() || !in.is_video) return out;
    if (in.pano360.valid()) {
        for (const app::Pano360View& v : app::pano360_views(in.pano360, job.pano))
            if (!v.dir.empty()) out.push_back(v.dir);
        if (out.size() < 2) out.clear();
        return out;
    }
    for (int t = 0; in.video_tracks >= 2 && t < in.video_tracks; t++)
        out.push_back("cam" + std::to_string(t));
    return out;
}

Pano360Probe probe_pano360(const std::string& ffmpeg_exe,
                           const std::string& path,
                           const std::atomic<bool>& cancel) {
    Pano360Probe out;
    std::vector<std::pair<int, int>> tracks;
#ifdef SS_HAVE_VIDEO
    {
        std::string err;
        tracks = app::video_track_sizes(path, err);
    }
#endif
    if (tracks.empty()) {
        VideoFacts facts;
        if (ffmpeg_probe_video(ffmpeg_exe, path, facts, cancel))
            tracks = facts.tracks;
    }
    if (tracks.size() != 2 || tracks[0] != tracks[1]) return out;
    app::Pano360Meta meta;
#ifdef SS_TOOL_SFM
    // What the camera says it wrote beats what the frame size suggests: two
    // GoPro generations pack 5952x1920 differently and only the tag tells them
    // apart.
    const sfm::VideoProjection pr = sfm::video_projection(path);
    meta = app::Pano360Meta{pr.name, pr.mode};
#endif
    const int w = tracks[0].first, h = tracks[0].second;
    if (!app::pano360_detect(2, w, h, meta, out.layout))
        out.unsupported = app::pano360_unsupported(2, w, h, meta);

    return out;
}

namespace {

// The last component, without a trailing separator ("a/b/" -> "b").
std::string leaf_name(const fs::path& p) {
    return p.filename().empty() ? p.parent_path().filename().string()
                                : p.filename().string();
}

bool named(const fs::path& p, const char* what) {
    std::string n = leaf_name(p);
    for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    return n == what;
}

// A folder that exists and has at least one image in it, at any depth. Stops at
// the first one: this runs on the UI thread, and the tree can hold thousands.
bool any_image(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_directory(p, ec)) return false;
    for (fs::recursive_directory_iterator it(p, kWalk, ec), end; !ec && it != end;
         it.increment(ec))
        if (it->is_regular_file(ec) && is_image_file(it->path())) return true;
    return false;
}

}  // namespace

bool folder_has_images(const std::string& dir) { return any_image(dir); }

namespace {

// Recursion for camera_subfolders. Collects the folder itself when it holds an
// image directly, then descends -- one readdir per folder, which is what
// answering both questions at once costs.
void collect_image_folders(const fs::path& dir, const std::string& rel, int depth,
                           std::vector<std::string>& out) {
    if (out.size() >= kMaxCameraFolders) return;
    std::vector<fs::path> sub;
    bool here = false;
    std::error_code ec;
    for (fs::directory_iterator it(dir, kWalk, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (it->is_directory(ec)) {
            if (!is_mask_folder(it->path().string()) &&
                !is_mask_edits_folder(it->path().string()))
                sub.push_back(it->path());
        } else if (!here && it->is_regular_file(ec) && is_image_file(it->path())) {
            here = true;
        }
    }
    if (here) out.push_back(rel);
    if (depth >= kMaxCameraFolderDepth) return;
    std::sort(sub.begin(), sub.end());
    for (const fs::path& s : sub)
        collect_image_folders(s, rel.empty() ? s.filename().string()
                                             : rel + "/" + s.filename().string(),
                              depth + 1, out);
}

}  // namespace

std::vector<std::string> camera_subfolders(const std::string& dir) {
    std::vector<std::string> out;
    collect_image_folders(dir, "", 0, out);
    return out;
}

std::vector<CameraGroup> camera_groups(const std::vector<PrepInput>& inputs) {
    std::vector<CameraGroup> out;
    for (size_t i = 0; i < inputs.size(); i++) {
        const PrepInput& in = inputs[i];
        if (in.subcameras.empty()) {
            out.push_back({i, -1, in.subdir});
            continue;
        }
        for (size_t k = 0; k < in.subcameras.size(); k++) {
            const std::string& rel = in.subcameras[k].rel;
            std::string full = in.subdir;
            if (!rel.empty()) full = full.empty() ? rel : full + "/" + rel;
            out.push_back({i, (int)k, full});
        }
    }
    return out;
}

std::vector<std::string> camera_group_models(const std::vector<PrepInput>& inputs,
                                             const std::vector<CameraGroup>& groups,
                                             const std::string& fallback) {
    std::vector<std::string> out;
    out.reserve(groups.size());
    std::string above = fallback;
    for (const CameraGroup& g : groups) {
        const std::string& m = group_model(inputs, g);
        if (!m.empty()) above = m;
        out.push_back(above);
    }
    return out;
}

// A camera .xml next to a point-cloud .ply, which is what MetashapeParser
// probes for.
static bool metashape_export_here(const fs::path& p) {
    std::error_code ec;
    bool has_xml = false, has_ply = false;
    for (fs::directory_iterator it(p, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string e = it->path().extension().string();
        for (auto& c : e) c = (char)std::tolower((unsigned char)c);
        has_xml = has_xml || e == ".xml";
        has_ply = has_ply || e == ".ply";
    }
    return has_xml && has_ply;
}

// A COLMAP model written straight into the folder rather than under sparse/,
// the last place ColmapParser looks. points3D is optional, as it is there.
static bool colmap_model_here(const fs::path& p) {
    std::error_code ec;
    for (const char* base : {"cameras", "images"})
        if (!fs::exists(p / (std::string(base) + ".bin"), ec) &&
            !fs::exists(p / (std::string(base) + ".txt"), ec))
            return false;
    return true;
}

bool folder_looks_like_dataset(const std::string& dir) {
    std::error_code ec;
    const fs::path p(dir);
    if (fs::exists(p / "transforms.json", ec) ||
        fs::is_directory(p / "sparse", ec) || fs::is_directory(p / "colmap", ec))
        return true;
    return colmap_model_here(p) || metashape_export_here(p);
}

// A folder is the run's leftover only if the run would write it. When images/
// or masks/ under the output IS an input, it is the capture.
static bool is_input_folder(const fs::path& dir,
                            const std::vector<PrepInput>& inputs, bool masks) {
    std::error_code ec;
    for (const PrepInput& in : inputs) {
        const std::string& p = masks ? in.mask_dir : in.path;
        if (!p.empty() && fs::equivalent(dir, p, ec)) return true;
    }
    return false;
}

WorkspaceState probe_workspace(const std::string& workspace,
                               const std::vector<PrepInput>& inputs) {
    WorkspaceState st;
    std::error_code ec;
    const fs::path ws(workspace);
    if (workspace.empty() || !fs::is_directory(ws, ec)) return st;

    auto is_input = [&](const fs::path& dir, bool masks) {
        return is_input_folder(dir, inputs, masks);
    };
    auto has_content = [&](const fs::path& p) {
        return fs::is_directory(p, ec) && !fs::is_empty(p, ec);
    };

    st.frames = has_content(ws / "images") && !is_input(ws / "images", false);
    st.masks = has_content(ws / "masks") && !is_input(ws / "masks", true);
    for (const PrepInput& in : inputs)
        st.input_masks = st.input_masks || (!in.mask_dir.empty() && has_content(in.mask_dir));
    st.features = has_content(ws / "features") || fs::exists(ws / "matches.bin", ec) ||
                  fs::exists(ws / "database.db", ec);
    // Stricter than folder_looks_like_dataset, which answers "where should a
    // dropped folder go" and takes an empty sparse/ for a model. An empty one
    // is a directory somebody made, and reconstructing into it is right.
    st.model = has_content(ws / "sparse") || has_content(ws / "colmap") ||
               fs::exists(ws / "transforms.json", ec) ||
               colmap_model_here(ws) || metashape_export_here(ws);
    st.geometry = has_content(ws / "normals") || has_content(ws / "depths");
    st.recon_stamp = fs::exists(ws / kReconStampFile, ec);
    return st;
}

int resolved_frame_bits(const PrepJob& job, const PrepInput& in, VideoColorRead read) {
    if (!read) read = job.read_color;
#ifdef SS_TOOL_SFM
    if (!read) read = sfm::video_color;
#endif
    const bool asks = in.is_video && job.frame_bits == 0 && read;
    return frame_bits_for(job, in, asks ? read(in.path).mode : sfm::VideoColorMode::NotRecorded);
}

spirula::DatasetColor clip_colors(const std::vector<PrepInput>& inputs,
                                  std::vector<std::string>* notes, VideoColorRead read) {
#ifdef SS_TOOL_SFM
    if (!read) read = sfm::video_color;
#endif
    spirula::DatasetColor d;
    for (const PrepInput& in : inputs) {
        spirula::ClipColorEntry e;
        e.source = leaf_name(fs::path(in.path));
        if (in.is_video && read) {
            const sfm::VideoColor c = read(in.path);
            e.code = c.code;
            e.proto = c.proto;
            switch (c.mode) {
                case sfm::VideoColorMode::Normal:   e.mode = spirula::ClipColor::Normal; break;
                case sfm::VideoColorMode::DlogM:    e.mode = spirula::ClipColor::DlogM; break;
                case sfm::VideoColorMode::OtherLog: e.mode = spirula::ClipColor::OtherLog; break;
                case sfm::VideoColorMode::Unknown:  e.mode = spirula::ClipColor::Unknown; break;
                default: break;
            }
            if (notes && e.mode == spirula::ClipColor::Unknown)
                notes->push_back(fmt(lmsg::clip_color_unreadable,
                                     {e.source, e.proto.empty() ? std::string("djmd") : e.proto}));
        }
        d.clips.push_back(e);
    }
    return d;
}

std::vector<std::string> workspace_artifacts(const std::string& workspace,
                                             const std::vector<PrepInput>& inputs) {
    std::vector<std::string> out;
    std::error_code ec;
    const fs::path ws(workspace);
    if (workspace.empty() || !fs::is_directory(ws, ec)) return out;

    auto add = [&](const char* name) {
        const fs::path p = ws / name;
        if (fs::is_directory(p, ec) ? !fs::is_empty(p, ec) : fs::exists(p, ec))
            out.push_back(p.string());
    };
    if (!is_input_folder(ws / "images", inputs, false)) add("images");
    if (!is_input_folder(ws / "masks", inputs, true)) add("masks");
    for (const char* name : {"features", "sparse", "colmap", "normals", "depths",
                             ".progress", sfm::resume::kDir, "matches.bin",
                             "database.db", kReconStampFile,
                             spirula::kDatasetColorFile})
        add(name);
    return out;
}

bool is_mask_folder(const std::string& path) {
    return named(fs::path(path), "masks");
}

bool is_mask_edits_folder(const std::string& path) {
    return named(fs::path(path), gui::mask::kLayerDirName);
}

void resolve_photo_folder(const std::string& picked, std::string& images,
                          std::string& masks) {
    std::error_code ec;
    const fs::path p = fs::absolute(picked, ec);
    images = p.string();
    masks.clear();
    // A dataset folder: index images/, not the folder holding it (which also
    // holds the masks, the point cloud, and whatever else was left there).
    // This is the same probe `spirula sfm auto` prints as "<dir> contains
    // images/, using <dir>/images as the image directory".
    if (any_image(p / "images")) images = (p / "images").string();
    // The masks belong beside the images: under the folder that holds them, or
    // -- when the folder IS the images/ of a dataset -- next to it.
    std::vector<fs::path> candidates{fs::path(images) / "masks"};
    if (named(images, "images"))
        candidates.push_back(fs::path(images).parent_path() / "masks");
    for (const fs::path& cand : candidates) {
        if (fs::is_directory(cand, ec) && any_image(cand)) {
            masks = cand.string();
            break;
        }
    }
}

std::string planned_image_dir(const std::vector<PrepInput>& inputs,
                              const std::string& workspace, PhotoImport mode) {
    std::error_code ec;
    if (reads_photos_in_place(inputs, mode))
        return fs::absolute(inputs[0].path, ec).string();
    return workspace.empty() ? std::string()
                             : (fs::path(workspace) / "images").string();
}

const Backends& backends() {
    static const Backends probed = [] {
        Backends b;
#ifdef SS_HAVE_VIDEO
        b.builtin_video = true;
#else
        b.video_reason = "built without the video decoder "
                         "(-DSS_ENABLE_PATENTED=OFF)";
        b.video_note = "frames are extracted with ffmpeg";
#endif
#ifdef SS_BUILD_SAM
        b.builtin_masking = true;
#else
        b.masking_reason = "built without the segmentation module "
                           "(-DSS_BUILD_SAM=OFF)";
        b.masking_note =
            "Masks are made by an external Python script "
            "(reference/scripts/mask.py with lang-segment-anything, which "
            "needs a CUDA PyTorch). Set the Python path under Tool "
            "locations if it is not on PATH.";
#endif
        return b;
    }();
    return probed;
}

// The decoder's own capability answer for THIS device; empty means yes. It
// creates the inference context, so it is only asked after a job's device
// request is frozen, at that job's first extraction.
static const std::string& native_decode_reason() {
#ifdef SS_HAVE_VIDEO
    static const std::string reason = app::video_decode_availability();
    return reason;
#else
    return backends().video_reason;
#endif
}

int DatasetPrep::count_images(const std::string& dir, const std::string& skip) {
    return (int)walk_images(dir, skip).size();
}

// Pixel size from the header alone, no decode. STORED, not displayed: an
// import that does not re-encode leaves the EXIF turn on the file, and the
// focal prior this feeds describes the pixels every reader then sees.
static bool probe_dims(const fs::path& f, int& W, int& H) {
    if (exr::is_exr(f.string())) {
        exr::Info info;
        if (!exr::probe(f.string(), info).empty()) return false;
        W = info.width;
        H = info.height;
        return true;
    }
    int c = 0;
    return stbi_info(f.string().c_str(), &W, &H, &c) != 0;
}

bool DatasetPrep::first_image_dims(const std::string& dir, int& W, int& H) {
    for (const fs::path& f : walk_images(dir))
        if (probe_dims(f, W, H)) return true;
    return false;
}

std::vector<DatasetPrep::ImageSize> DatasetPrep::image_sizes(
        const std::string& dir, const std::string& skip) {
    std::vector<ImageSize> out;
    for (const fs::path& f : walk_images(dir, skip)) {
        ImageSize s;
        s.name = under_root(f, dir).generic_string();
        if (!probe_dims(f, s.w, s.h)) s.w = s.h = 0;
        out.push_back(std::move(s));
    }
    return out;
}

void DatasetPrep::log(const std::string& s, bool detail) {
    _prog->note(s, detail);
}

void DatasetPrep::enter(Stage s, const std::string& text) {
    _prog->enter(s, text);
}

int DatasetPrep::exec(
        const std::vector<std::string>& argv,
        const std::function<void(const std::string&)>& on_line) {
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);
    return run_process(argv, "", [this, &on_line](const std::string& line) {
        log(line);
        if (on_line) on_line(line);
    }, _cancel);
}

int64_t DatasetPrep::estimate_frames(const PrepJob& job, const PrepInput& in,
                                     const std::string& images) {
    // What a resumed run keeps is exactly what is there already.
    if (job.resume && !frames_stale(job)) {
        const int have = count_images(images);
        if (have > 0) return have;
    }
    if (!in.is_video) {
        int64_t n = (int64_t)walk_images(in.path).size();
        // The masks an input brings move in the same pass (gather_photos).
        if (!in.mask_dir.empty()) n += (int64_t)walk_images(in.mask_dir).size();
        return n;
    }
    // The probe has to match the path the extraction will take: the two count
    // tracks differently, and ffmpeg resamples the video rather than stepping
    // through the frames the container holds.

    // A 360 capture writes one image per view; its two tracks are one frame.
    const int per_frame =
        in.pano360.valid()
            ? (int)app::pano360_views(in.pano360, job.pano).size()
            : std::max(in.packed_lenses, 0);
#ifdef SS_HAVE_VIDEO
    if (builtin_decode(job, in)) {
        std::string err;
        video::VideoProbe probe;
        if (video::probe_video(in.path, probe, err) && probe.tracks > 0)
            return expected_frames(job, input_fps(job, in),
                                   probe.fps > 1.0 ? probe.fps : 30.0,
                                   probe.frame_count,
                                   per_frame > 0 ? per_frame : probe.tracks);
    }
#endif
    VideoFacts facts;
    if (ffmpeg_probe_video(job.ffmpeg_exe, in.path, facts, _cancel) &&
        facts.fps > 1.0)
        return expected_frames(job, input_fps(job, in), facts.fps, facts.frames,
                               per_frame > 0 ? per_frame
                                             : (is_dual_fisheye_path(in.path) ? 2 : 1));
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

bool DatasetPrep::run(const PrepJob& job_in, PrepResult& out, std::string& error,
                      const RefreshFn& refresh_masks) {
    PrepJob job = job_in;
#ifdef SS_BUILD_SAM
    // Hand the GPU back on any exit: a ~2 GB SAM 3 pool would outlive the run.
    // Safe: stop_inference_users() freed every other Session before launch --
    // both previews' and the mask editor's -- and the editor refuses during a run.
    struct ReleaseDevice {
        ~ReleaseDevice() { nn::shutdown(); }
    } release_device;
#endif
    const fs::path ws = job.workspace;
    std::error_code ec;
    fs::create_directories(ws, ec);
    if (job.inputs.empty()) {
        error = lmsg::err_nothing_to_prepare.get();
        return false;
    }

    // Before the built-in decoder can pick one. Only a job that will reach the
    // decoder pays: asking whether THIS device decodes is what the freeze makes
    // safe, and a photo-only job that masks freezes inside Masker::init.
#ifdef SS_HAVE_VIDEO
    bool wants_native_decode = false;
    if (!job.force_external_decode)
        for (const PrepInput& in : job.inputs)
            if (in.is_video) {
                wants_native_decode = true;
                break;
            }
    if (wants_native_decode && !sam::freeze_device(job.device, error))
        return false;
#endif

    _bits.assign(job.inputs.size(), 8);
    for (size_t i = 0; i < job.inputs.size(); i++) {
        _bits[i] = resolved_frame_bits(job, job.inputs[i]);
        if (_bits[i] == 16 && job.frame_bits == 0)
            log(fmt(lmsg::frames_16bit_auto, {leaf_name(job.inputs[i].path)}),
                /*detail=*/false);
    }

    // Frames already there were extracted with settings the workspace records;
    // a run asking for others has to go back to the video (ReconStamp.h).
    const ReconStamp frames_now = frames_stamp(job, _bits);
    // No stamp over frames that are there: an earlier run stopped before
    // writing it, so what the frames came from is unknown (see the prune below).
    bool frames_unstamped = false;
    {
        const ReconStamp prior = read_recon_stamp(ws.string(), kFramesStampFile);
        std::error_code ec;
        frames_unstamped = !prior.present && fs::is_directory(ws / "images", ec) &&
                           !fs::is_empty(ws / "images", ec);
        const std::string moved = recon_stamp_change(prior, frames_now);
        if (!moved.empty()) {
            _frames_stale = true;
            out.frames_rebuilt = true;
            log(fmt(lmsg::frames_settings_changed, {moved}), /*detail=*/false);
        }
    }

    // Where each input's images and masks ended up, so the masking pass below
    // can run per input (see generate_masks) instead of over one flat tree.
    struct Prepared {
        std::string images, masks;      // absolute
        std::string images_rel, masks_rel;  // relative to the workspace
        // This input's masks already exist: brought along by the input, taken
        // from its alpha channel, or kept by a resumed run.
        bool have_masks = false;
        // Segmentation already intersected this input's stencil into them.
        bool stencil_folded = false;
        // The tree the stencil pass folds in, when it is not `masks` itself:
        // the masks the photos arrived with, which stay as they are.
        std::string merge_from;
    };
    std::vector<Prepared> per(job.inputs.size());
    // A masks/ nested under the images is not a folder of views (see
    // count_images); only the in-place case can have one, since everything else
    // writes into the dataset's own images/.
    std::string skip_dir;
    // Is the dataset's own masks/ this run's, rather than an earlier one's? A
    // stale masks/ from a previous run with masking on must not be picked up by
    // a run that turned it off -- that is what --no-masks exists to say.
    bool want_masks = job.mask_enable;

    if (reads_photos_in_place(job.inputs, job.photo_import)) {
        // Photos are referenced where they are, not copied: a 40 GB folder of
        // raw captures does not want a second copy, and the parsers accept an
        // absolute image_dir.
        const PrepInput& in = job.inputs[0];
        out.image_dir = fs::absolute(in.path).string();
        out.image_dir_cfg = out.image_dir;
        // A capture handed over already split into cam/, cam0/, cam1/ is
        // several cameras, exactly as a multi-track video is -- and the folders
        // are what says so, since this path copies nothing.
        if (camera_subfolders(out.image_dir).size() > 1)
            out.per_folder_cameras = true;
        per[0].images = per[0].images_rel = out.image_dir;
        if (in.mask_dir.empty()) {
            // Nothing came with them, so anything generated goes in the
            // dataset, next to the reconstruction rather than next to the
            // photos -- a folder we were only asked to read.
            per[0].masks = (ws / "masks").string();
            per[0].masks_rel = "masks";
            skip_dir = per[0].masks;
        } else {
            const std::string bundled = fs::absolute(in.mask_dir, ec).string();
            per[0].have_masks = true;
            log(fmt(lmsg::using_bundled_masks, {bundled}), /*detail=*/false);
            if (in.stencil.empty()) {
                // Nothing to fold in, so they are read where they lie and keep
                // whichever convention they arrived in.
                per[0].masks = per[0].masks_rel = bundled;
                out.mask_dir_flipped = job.flip_found_masks;
            } else {
                // The stencil has to go somewhere, and a masks/ under the
                // images is the one folder every reader already skips -- so
                // when theirs is there, the combination replaces it.
                per[0].merge_from = bundled;
                const bool under_images =
                    inside(fs::path(bundled), fs::path(out.image_dir));
                per[0].masks = under_images ? bundled : (ws / "masks").string();
                per[0].masks_rel = under_images ? bundled : std::string("masks");
            }
            out.mask_dir = per[0].masks;
            out.mask_dir_cfg = per[0].masks_rel;
            skip_dir = per[0].masks;
        }
    } else {
        out.image_dir = (ws / "images").string();
        out.image_dir_cfg = "images";
        // Unstamped frames are pruned without being called stale, so a resume
        // still keeps the inputs an interrupted run finished.
        if (frames_stale(job) || frames_unstamped) {
            const Pruned pr = prune_removed_inputs(fs::path(out.image_dir), ws, job.inputs,
                                                   /*own_frames_kept=*/!frames_stale(job));
            auto join = [](const std::vector<std::string>& v) {
                std::string j;
                for (const std::string& x : v) j += (j.empty() ? "" : ", ") + x;
                return j;
            };
            if (!pr.removed.empty())
                log(fmt(lmsg::frames_removed_inputs, {join(pr.removed)}), /*detail=*/false);
            for (const auto& [name, target] : pr.links)
                log(fmt(lmsg::frames_link_removed, {name, target}), /*detail=*/false);
            if (!pr.stuck.empty())
                log(fmt(lmsg::frames_not_removed, {join(pr.stuck)}), /*detail=*/false);
        }
        // Every input is measured before any of them is extracted, so the bar
        // covers the whole step from the first frame rather than restarting on
        // each input (StageTally).
        std::vector<int64_t> planned(job.inputs.size(), 0);
        for (size_t i = 0; i < job.inputs.size(); i++) {
            const PrepInput& in = job.inputs[i];
            Prepared& p = per[i];
            p.images = under(out.image_dir, in.subdir).string();
            p.masks = under((ws / "masks").string(), in.subdir).string();
            p.images_rel = under("images", in.subdir).generic_string();
            p.masks_rel = under("masks", in.subdir).generic_string();
            // Folded in from where they lie, not from the copies gathered next
            // to the images: a second run would otherwise fold the fold.
            if (!in.mask_dir.empty() &&
                (!in.stencil.empty() || job.flip_found_masks))
                p.merge_from = fs::absolute(in.mask_dir, ec).string();
            // Several inputs share one image tree only by living in their own
            // folders, and a folder is what makes them separate cameras.
            if (!in.subdir.empty()) out.per_folder_cameras = true;
            planned[i] = estimate_frames(job, in, p.images);
            _frames_tally.plan(planned[i]);
        }
        _plans.assign(job.inputs.size(), std::vector<int64_t>());
        _planned.assign(job.inputs.size(), false);
        // The panel that watches the measuring pass wants the whole list, so
        // a folder of photographs among the clips is a row that says so rather
        // than a gap. A video's length arrives with its first measured step.
        if (job.adaptive_fps) {
            std::vector<ScanRow> rows(job.inputs.size());
            for (size_t i = 0; i < job.inputs.size(); i++) {
                rows[i].name = leaf_name(job.inputs[i].path);
                rows[i].video = job.inputs[i].is_video &&
                                !every_frame(job, job.inputs[i]);
                if (!rows[i].video) rows[i].frames = planned[i];
            }
            _prog->scan_reset(std::move(rows));
        }
        for (size_t i = 0; i < job.inputs.size(); i++) {
            const PrepInput& in = job.inputs[i];
            Prepared& p = per[i];
            if (in.is_video) {
                const bool keeping =
                    job.resume && !frames_stale(job) && count_images(p.images) > 0;
                if (!keeping && builtin_decode(job, in) && !plan_group(job, i, error))
                    return false;
                if (!extract_video(job, in, p.images, p.masks, out, p.have_masks,
                                   error))
                    return false;
            } else if (!gather_photos(job, in, p.images, p.masks, p.have_masks,
                                      error)) {
                return false;
            }
            int64_t produced = count_images(p.images);
            if (!in.is_video && !in.mask_dir.empty())
                produced += count_images(p.masks);
            _frames_tally.settle(produced, planned[i]);
            // Masks an input brought with it -- or carried in its alpha --
            // are in the dataset now, so they count even when nothing asked
            // for masking.
            if (p.have_masks) want_masks = true;
        }
        // A capture that arrived split into cam/, cam0/, cam1/ keeps those
        // folders on the way in, and they are what make it several cameras --
        // not only a job whose inputs each got one.
        if (camera_subfolders(out.image_dir).size() > 1)
            out.per_folder_cameras = true;
    }

    // Written once the images are there, so an interrupted extraction is not
    // recorded as having produced what it was asked for.
    write_recon_stamp(ws.string(), frames_now, kFramesStampFile);
    {
        std::vector<std::string> notes;
        const spirula::DatasetColor colors = clip_colors(job.inputs, &notes, job.read_color);
        for (const std::string& n : notes) log(n, /*detail=*/false);
        const spirula::ColorRecordResult rec = spirula::write_dataset_color(ws.string(), colors);
        const std::string rec_path = (ws / spirula::kDatasetColorFile).string();
        // A record left stale would decode every frame the wrong way, silently.
        if (rec.status == spirula::ColorRecordWrite::FailedStale) {
            error = fmt(lmsg::color_record_stale, {rec_path, rec.error});
            return false;
        }
        if (rec.status == spirula::ColorRecordWrite::Failed)
            log(fmt(lmsg::color_record_failed, {rec_path, rec.error}), /*detail=*/false);
    }

    out.n_images = count_images(out.image_dir, skip_dir);
    log(fmt(lmsg::found_images, {(long long)out.n_images, out.image_dir}),
        /*detail=*/false);
    if (out.n_images < 3) {
        error = fmt(lmsg::err_too_few_images, {(long long)out.n_images});
        return false;
    }

    // The frames exist now, so what masking is asked to do can still be the
    // answer the user gave while watching them go by.
    if (refresh_masks) refresh_masks(job);

    std::vector<int64_t> mask_planned(job.inputs.size(), 0);
    if (job.mask_enable) {
        if (job.mask_prompt.empty() && job.mask_clicks.empty()) {
            error = lmsg::err_mask_no_target.get();
            return false;
        }
        // Half-masking reads as a masking run that worked, so refuse instead:
        // without a text prompt, every input needs clicks of its own.
        if (job.mask_prompt.empty()) {
            std::string unprompted;
            for (size_t i = 0; i < job.inputs.size(); i++) {
                if (per[i].have_masks || !clicks_for(job, job.inputs[i]).empty())
                    continue;
                if (!unprompted.empty()) unprompted += ", ";
                unprompted += job.inputs[i].path;
            }
            if (!unprompted.empty()) {
                error = fmt(lmsg::err_inputs_without_prompt, {unprompted});
                return false;
            }
        }
        for (size_t i = 0; i < job.inputs.size(); i++)
            if (!per[i].have_masks) {
                mask_planned[i] = count_images(per[i].images, per[i].masks);
                _masks_tally.plan(mask_planned[i]);
            }
    }
    // The step's bar covers the stencil pass as well as segmentation, so both
    // are planned before either runs.
    std::vector<int64_t> stencil_planned(job.inputs.size(), 0);
    for (size_t i = 0; i < job.inputs.size(); i++)
        if (!job.inputs[i].stencil.empty() || !per[i].merge_from.empty()) {
            stencil_planned[i] = count_images(per[i].images, per[i].masks);
            _masks_tally.plan(stencil_planned[i]);
        }

    if (job.mask_enable) {
        for (size_t i = 0; i < job.inputs.size(); i++) {
            // A re-done masking pass writes one mask per frame that exists
            // now; anything else in there described a frame set that no longer
            // does. clear_generated refuses a folder outside the workspace, so
            // masks an input brought with it are safe.
            if (job.redo_masks && job.inputs[i].mask_dir.empty())
                clear_generated(per[i].masks, ws);
            // Already masked by whoever drew the masks -- or the alpha -- this
            // input arrived with. Segmenting over those would replace an
            // answer the user already has.
            if (per[i].have_masks) continue;
            if (!generate_masks(job, job.inputs[i], per[i].images,
                                per[i].images_rel, per[i].masks, per[i].masks_rel,
                                per[i].stencil_folded, error))
                return false;
            _masks_tally.settle(mask_planned[i], mask_planned[i]);
        }
    }

    for (size_t i = 0; i < job.inputs.size(); i++) {
        if (job.inputs[i].stencil.empty() && per[i].merge_from.empty()) continue;
        want_masks = true;
        // Segmentation intersected it as it went, so the pass planned for it
        // is not going to run.
        if (per[i].stencil_folded) {
            _masks_tally.drop(stencil_planned[i]);
            continue;
        }
        if (!apply_stencil(job, job.inputs[i], per[i].images, per[i].masks,
                           per[i].merge_from, error))
            return false;
        _masks_tally.settle(stencil_planned[i], stencil_planned[i]);
    }
    // Masks are whatever ended up in the dataset's own masks/ -- generated
    // here, written by the decoder, or linked in beside gathered photos. The
    // in-place case has already named the folder it reads them from.
    std::error_code mec;
    if (out.mask_dir.empty() && want_masks && fs::is_directory(ws / "masks", mec) &&
        !fs::is_empty(ws / "masks", mec)) {
        out.mask_dir = (ws / "masks").string();
        out.mask_dir_cfg = "masks";
    }
    // Hand corrections outlive a re-run: whatever wrote masks/ this time, the
    // editor's layers are re-applied over every mask whose bytes changed.
    const fs::path layer_root = ws / gui::mask::kLayerDirName;
    if (fs::exists(layer_root / gui::mask::kIndexFileName, mec)) {
        std::string rerr;
        const int n = gui::mask::recomposite_all(layer_root.string(), rerr);
        // Success and failure are independent: a partial batch, or every frame
        // failing (n == 0 with rerr non-empty), must still say so.
        if (n > 0) log(fmt(mmsg::log_recomposited, {(long long)n}), /*detail=*/false);
        if (!rerr.empty()) log(fmt(mmsg::log_recomposite_failed, {rerr}), /*detail=*/false);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Video -> frames
// ---------------------------------------------------------------------------

// Whether one stem's frames were taken at one instant: a 360 packing's views
// are cut from one decoded canvas; separate tracks only under `sync_tracks`,
// which only the built-in decoder does.
static bool lockstep_extraction(const PrepJob& job, const PrepInput& in, bool builtin,
                                bool bits16) {
    if (in.pano360.valid()) return job.pano.mode != app::Pano360Mode::Off;
    if (in.packed_lenses >= 2) return true;
    return bits16 || (builtin && job.sync_tracks);
}

int DatasetPrep::bits_of(const PrepJob& job, const PrepInput& in) const {
    const size_t row = input_index(job, in);
    return row < _bits.size() ? _bits[row] : 8;
}

bool DatasetPrep::builtin_decode(const PrepJob& job, const PrepInput& in) const {
    return !job.force_external_decode && bits_of(job, in) != 16 &&
           native_decode_reason().empty();
}

bool DatasetPrep::extract_video(const PrepJob& job, const PrepInput& in,
                                const std::string& images,
                                const std::string& masks, PrepResult& out,
                                bool& masked, std::string& error) {
    // Resume: frames are moved into place in one batch after selection, so a
    // non-empty folder means a previous extraction of THIS input finished.
    if (frames_stale(job)) clear_generated(images, job.workspace);
    if (job.resume && !frames_stale(job)) {
        const int have = count_images(images);
        if (have > 0) {
            log(fmt(lmsg::resume_keep_frames, {(long long)have, images}),
                /*detail=*/false);
            std::error_code ec;
            if (fs::is_directory(fs::path(images) / "cam1", ec))
                out.per_folder_cameras = true;
            // Kept frames of unknown provenance: the file's own rate is the
            // built-in extractor's convention, and a wrong one is refused
            // downstream by the gyro-against-poses check, not misused.
            out.captures.push_back(
                {in.subdir, in.path, 0.0,
                 lockstep_extraction(job, in, builtin_decode(job, in),
                                     bits_of(job, in) == 16 && !every_frame(job, in))});
            if (!split_packed_frames(in, images, out, error)) return false;
            // Masks a previous run left. Not when this one is re-doing them:
            // `masked` is what makes run() skip the masking pass entirely.
            if (job.mask_enable && !job.redo_masks) {
                std::error_code mec;
                if (fs::is_directory(masks, mec) && !fs::is_empty(masks, mec)) {
                    log(fmt(lmsg::resume_keep_masks, {masks}), /*detail=*/false);
                    masked = true;
                }
            }
            return true;
        }
    }

    const bool bits16 = bits_of(job, in) == 16;
    if (bits16 && !job.force_external_decode && native_decode_reason().empty())
        log(lmsg::frames_16bit_ffmpeg.get(), /*detail=*/false);
    if (builtin_decode(job, in)) {
        if (extract_video_builtin(job, in, images, out, error)) {
            out.captures.push_back(
                {in.subdir, in.path, 0.0, lockstep_extraction(job, in, true, false)});
            return split_packed_frames(in, images, out, error);
        }
        if (_cancel.load()) return false;
        // A container or profile the driver cannot decode is exactly what the
        // fallback is for, and the user should not have to know which is which.
        log(fmt(lmsg::decode_fallback_ffmpeg, {error}), /*detail=*/false);
    }
    const bool ok = in.pano360.valid() && job.pano.mode != app::Pano360Mode::Off
                        ? extract_360_ffmpeg(job, in, images, out, error)
                        : extract_video_ffmpeg(job, in, images, out, error);
    // The stems are candidate numbers, and the candidates were resampled at
    // the kept rate times the group -- which is the rate that times them. Every
    // frame is not resampled, so its stems are source indices as above.
    if (ok)
        out.captures.push_back({in.subdir, in.path,
                                (double)input_fps(job, in) * candidate_group(job, in),
                                lockstep_extraction(job, in, false,
                                                    bits16 && !every_frame(job, in))});
    return ok && split_packed_frames(in, images, out, error);
}

#ifdef SS_HAVE_VIDEO
// The job's frame-selection half, which the scan and the extraction have to
// read the same way or a plan would be spaced against the wrong schedule.
static bool builtin_job(const PrepJob& job, const PrepInput& in,
                        app::FrameExtractJob& fx, int64_t* frames,
                        std::string& error) {
    video::VideoProbe probe;
    std::string probe_err;
    if (!video::probe_video(in.path, probe, probe_err) || probe.tracks <= 0) {
        error = probe_err.empty() ? "no video track" : probe_err;
        return false;
    }
    if (frames) *frames = probe.frame_count;
    const double src_fps = probe.fps > 1.0 ? probe.fps : 30.0;
    const bool every = every_frame(job, in);
    const int window = every ? 1 : std::max(job.sharp_window, 1);
    fx.input = in.path;
    fx.device = job.device;
    fx.skip = frame_skip(input_fps(job, in), src_fps);
    fx.keep = window > 1 ? window : 0;
    fx.max_frames = job.max_frames;
    fx.sync_tracks = job.sync_tracks;
    fx.adaptive = job.adaptive_fps && !every;
    fx.adaptive_range = job.adaptive_range;
    fx.auto_rotate = job.auto_rotate;
    fx.quality = 95;
    if (in.pano360.valid()) {
        const std::vector<app::Pano360View> views =
            app::pano360_views(in.pano360, job.pano);
        if (!views.empty()) {
            fx.eac = in.pano360;
            fx.views = views;
        }
    }
    return true;
}
#endif

// Every video extracted at one rate, planned together: a brisk clip takes
// frames off one shot from a bench, as far as the two were measured the same
// way. Scanned here so the group is measured before any of it is written.
bool DatasetPrep::plan_group(const PrepJob& job, size_t at, std::string& error) {
#ifndef SS_HAVE_VIDEO
    (void)job; (void)at; (void)error;
    return true;
#else
    if (!job.adaptive_fps || _plans.size() != job.inputs.size()) return true;
    if (_planned[at] || every_frame(job, job.inputs[at])) return true;
    const size_t g = fps_group(job.inputs, at);
    std::vector<size_t> rows;
    for (size_t i = 0; i < job.inputs.size(); i++)
        if (job.inputs[i].is_video && fps_group(job.inputs, i) == g)
            rows.push_back(i);

    // Probed first, so the bar covers the group from its first frame instead of
    // restarting at each video of it.
    std::vector<app::FrameExtractJob> jobs(rows.size());
    std::vector<int64_t> lengths(rows.size(), 0);
    int64_t group_frames = 0;
    for (size_t k = 0; k < rows.size(); k++) {
        if (!builtin_job(job, job.inputs[rows[k]], jobs[k], &lengths[k], error))
            return false;
        group_frames += std::max<int64_t>(0, lengths[k]);
    }

    app::FrameExtractSinks sinks;
    sinks.log = [this](const std::string& l) { log(l); };
    sinks.cancel = &_cancel;
    ScanProgress scan(_prog, group_frames);
    sinks.scanning = [&](int64_t done, int64_t total) { scan.update(done, total); };
    std::vector<app::MotionPlanInput> measured(rows.size());
    std::vector<double> fps(rows.size(), 0.0);
    for (size_t k = 0; k < rows.size(); k++) {
        scan.begin(leaf_name(job.inputs[rows[k]].path));
        _prog->scan_open(rows[k]);
        sinks.measured = [this, row = rows[k]](int64_t at, int64_t of, float c) {
            _prog->scan_step(row, at, of, c);
        };
        app::FrameExtractStats stats;
        if (!app::scan_motion(jobs[k], sinks, measured[k], stats, error))
            return false;
        fps[k] = measured[k].fps;
        scan.finish(lengths[k] > 0 ? lengths[k] : measured[k].frames);
    }
    const std::vector<std::vector<int64_t>> plans =
        app::plan_by_motion(measured, job.adaptive_range);
    for (size_t k = 0; k < rows.size() && k < plans.size(); k++) {
        if (plans[k].empty()) {
            error = lmsg::err_capture_too_short.get();
            return false;
        }
        _plans[rows[k]] = plans[k];
        _planned[rows[k]] = true;
        _prog->scan_kept(rows[k], spacing_bars(plans[k], measured[k].frames),
                         (int64_t)plans[k].size());
        sinks.planned = nullptr;
        app::report_plan(sinks, plans[k], measured[k].frames, fps[k]);
    }
    return true;
#endif
}

// Extraction writes frames and nothing else: masking is a separate pass
// (generate_masks_builtin), so frames are visible before the user chooses
// masks and re-masking costs no re-extraction.
bool DatasetPrep::extract_video_builtin(const PrepJob& job, const PrepInput& in,
                                        const std::string& images,
                                        PrepResult& out, std::string& error) {
#ifndef SS_HAVE_VIDEO
    (void)job; (void)in; (void)images; (void)out;
    error = backends().video_reason;
    return false;
#else
    enter(Stage::Frames, lmsg::stage_extract_gpu.get());
    log(fmt(lmsg::video_input, {in.path}), /*detail=*/false);

    // fps -> "one frame every N source frames". The source rate is what the
    // container states; a variable-rate file is close enough for this.
    std::string probe_err;
    video::VideoProbe probe;
    if (!video::probe_video(in.path, probe, probe_err) || probe.tracks <= 0) {
        error = probe_err.empty() ? "no video track" : probe_err;
        return false;
    }
    const std::vector<app::Pano360View> views =
        in.pano360.valid() ? app::pano360_views(in.pano360, job.pano)
                          : std::vector<app::Pano360View>();
    if (!views.empty()) out.per_folder_cameras = views.size() > 1;
    else if (probe.tracks > 1) out.per_folder_cameras = true;

    app::FrameExtractJob fx;
    if (!builtin_job(job, in, fx, nullptr, error)) return false;
    fx.image_dir = images;
    const size_t row = input_index(job, in);
    if (row < _plans.size()) fx.plan = _plans[row];
    if (!views.empty())
        log(fmt(lmsg::pano360_plan, {(long long)views.size(), views[0].width,
                                     views[0].height}), /*detail=*/false);

    app::FrameExtractSinks sinks;
    sinks.log = [this](const std::string& l) { log(l); };
    sinks.cancel = &_cancel;
    RateLimitedProgress progress(_prog, Stage::Frames, lmsg::noun_frames_written,
                                 _frames_tally);
    sinks.progress = [&](int64_t written, int64_t decoded) {
        (void)decoded;
        progress.update(written);
    };
    // Only where this run is going to measure for itself: a plan handed in was
    // made by plan_group, which reported its own pass.
    ScanProgress scan(_prog, probe.frame_count);
    if (fx.adaptive && fx.plan.empty()) {
        scan.begin(leaf_name(in.path));
        sinks.scanning = [&](int64_t done, int64_t total) {
            scan.update(done, total);
        };
    }
    sinks.measured = [this, row](int64_t at, int64_t of, float c) {
        _prog->scan_step(row, at, of, c);
    };
    sinks.planned = [this, row](const std::vector<int64_t>& plan, int64_t frames) {
        _prog->scan_kept(row, spacing_bars(plan, frames), (int64_t)plan.size());
    };
    if (_films.frames) {
        const fs::path root(images);
        sinks.preview = [this, root](const uint8_t* rgb, int w, int h,
                                     const std::string& path) {
            FilmFrame f;
            f.name = under_root(fs::path(path), root).generic_string();
            f.image_path = path;
            // Registered whether or not its picture is wanted: the reel's
            // slider covers every frame, and the ones nobody watched go by are
            // read back from disk.
            _films.frames->add(f, _films.frames->wants() ? rgb : nullptr, w, h);
        };
    }

    app::FrameExtractStats stats;
    if (!app::extract_frames(fx, sinks, stats, error)) return false;
    log(app::format_extract_stats(stats, images, /*masked=*/false));
    if (stats.written == 0) {
        error = lmsg::err_no_frames_extracted.get();
        return false;
    }
    return true;
#endif
}

bool DatasetPrep::extract_video_ffmpeg(const PrepJob& job, const PrepInput& in,
                                       const std::string& images,
                                       PrepResult& out, std::string& error) {
    const bool bits16 = bits_of(job, in) == 16;
    if (job.sync_tracks && !bits16) log(lmsg::sync_needs_builtin.get(), /*detail=*/false);
    const fs::path ws = job.workspace;
    if (!command_exists(job.ffmpeg_exe)) {
        error = fmt(lmsg::err_ffmpeg_missing, {job.ffmpeg_exe});
        return false;
    }
    log(fmt(lmsg::video_input, {in.path}), /*detail=*/false);

    // Multi-track videos (an Insta360 .insv, a DJI .osv): one folder per track,
    // one camera per folder. Counted off the stream table rather than ffprobe's
    // stream list, which calls an attached cover picture a video track.
    VideoFacts facts;
    ffmpeg_probe_video(job.ffmpeg_exe, in.path, facts, _cancel);
    size_t streams = 1;
    if (is_dual_fisheye_path(in.path) && facts.tracks.size() > 1)
        streams = facts.tracks.size();
    if (streams > 1) out.per_folder_cameras = true;

    const bool every = every_frame(job, in);
    const int window = every ? 1 : std::max(job.sharp_window, 1);
    const int group = candidate_group(job, in);
    const bool fisheye = streams > 1 && !facts.tracks.empty() &&
                         facts.tracks[0].first == facts.tracks[0].second;
    if (bits16)
        return extract_video_ffmpeg16(job, in, images, streams, fisheye, facts.width,
                                      facts.height, facts.frames, error);
    for (size_t tr = 0; tr < streams; tr++) {
        std::string track_path = in.path;
        const fs::path out_dir = streams > 1
            ? fs::path(images) / ("cam" + std::to_string(tr))
            : fs::path(images);
        if (job.resume && !frames_stale(job) &&
            count_images(out_dir.string()) > 0) {
            log(fmt(lmsg::resume_keep_frames_dir, {out_dir.string()}),
                /*detail=*/false);
            continue;
        }
        if (streams > 1) {
            enter(Stage::Frames, fmt(lmsg::stage_split_track, {(long long)tr}));
            const fs::path tmp_track =
                ws / ("track_cam" + std::to_string(tr) + ".mp4");
            int rc = exec({job.ffmpeg_exe, "-nostdin", "-y", "-i", in.path,
                           "-map", "0:v:" + std::to_string(tr), "-c", "copy",
                           tmp_track.string()});
            if (rc == kCancelled) { error = lmsg::err_cancelled.get(); return false; }
            if (rc != 0) {
                error = lmsg::err_ffmpeg_split_failed.get();
                return false;
            }
            track_path = tmp_track.string();
        }
        enter(Stage::Frames, window > 1 ? lmsg::stage_extract_candidates.get()
                                       : lmsg::stage_extract_ffmpeg.get());
        RateLimitedProgress progress(_prog, Stage::Frames,
                                     lmsg::noun_frames_written, _frames_tally);
        progress.update(0, /*force=*/true);
        const fs::path cand = ws / "frames_tmp";
        remove_tree(cand);
        std::error_code ec;
        fs::create_directories(cand, ec);
        char vf[64];
        std::snprintf(vf, sizeof vf, "fps=%g", (double)input_fps(job, in) * group);
        // ffmpeg turns the picture by the container's matrix unless told not
        // to, which is what the built-in decoder's auto_rotate matches.
        std::vector<std::string> argv{job.ffmpeg_exe, "-nostdin", "-y"};
        if (!job.auto_rotate) argv.push_back("-noautorotate");
        argv.insert(argv.end(), {"-i", track_path});
        if (every) append_every_frame_args(argv, job.ffmpeg_exe, job.max_frames);
        else argv.insert(argv.end(), {"-vf", vf});
        argv.insert(argv.end(), {"-qscale:v", "2", (cand / "c_%06d.jpg").string()});
        const int max_frames = job.max_frames;
        int rc = exec(argv, [&progress, window, max_frames](const std::string& line) {
            const long long frame = progress_frame(line);
            if (frame < 0) return;
            int64_t projected = (frame + window - 1) / window;
            if (max_frames > 0) projected = std::min<int64_t>(projected, max_frames);
            progress.update(projected);
        });
        if (rc == kCancelled) { error = lmsg::err_cancelled.get(); return false; }
        if (rc != 0) {
            error = lmsg::err_ffmpeg_extract_failed.get();
            return false;
        }

        if (window > 1) enter(Stage::Frames, lmsg::stage_select_sharpest.get());
        fs::create_directories(out_dir, ec);
        FrameSelectOptions so;
        so.group = group;
        // ffmpeg already stopped at the cap, which is where the built-in
        // decoder stops too; spreading the cap would skip frames.
        so.max_frames = every ? 0 : job.max_frames;
        so.adaptive = job.adaptive_fps && !every;
        so.range = job.adaptive_range;
        so.window = window;
        if (fisheye) so.view = app::MotionView::Fisheye;
        so.out_fov = fisheye ? 3.4034f : 1.5708f;
        ScanProgress scan(_prog, 0);
        scan.begin(leaf_name(in.path));
        const size_t row = input_index(job, in);
        _prog->scan_open(row);
        so.scanning = [&](int64_t done, int64_t total) { scan.update(done, total); };
        so.measured = [this, row](int64_t at, int64_t of, float c) {
            _prog->scan_step(row, at, of, c);
        };
        so.planned = [this, row](const std::vector<int64_t>& plan, int64_t frames) {
            _prog->scan_kept(row, spacing_bars(plan, frames), (int64_t)plan.size());
        };
        const int kept = select_sharpest_frames(
            cand.string(), out_dir.string(), "", so,
            [this](const std::string& l) { log(l); }, _cancel);
        progress.update(std::max<int64_t>(kept, 0), /*force=*/true);
        remove_tree(cand);
        if (streams > 1) fs::remove(track_path, ec);
        if (kept < 0) {
            error = _cancel.load() ? "cancelled" : "frame selection failed";
            return false;
        }
        log(fmt(lmsg::kept_frames, {(long long)kept, out_dir.string()}),
            /*detail=*/false);
    }
    return true;
}

bool DatasetPrep::extract_video_ffmpeg16(const PrepJob& job, const PrepInput& in,
                                         const std::string& images, size_t streams,
                                         bool fisheye, int width, int height,
                                         long long frames, std::string& error) {
    const fs::path ws = job.workspace;
    const bool every = every_frame(job, in);
    const int window = every ? 1 : std::max(job.sharp_window, 1);
    const int group = candidate_group(job, in);
    const fs::path cand = ws / "frames_tmp", kept = ws / "kept_tmp",
                   stage = ws / "frames16_tmp";
    std::error_code ec;
    std::vector<fs::path> tracks;
    auto clean = [&] {
        remove_tree(cand);
        remove_tree(kept);
        remove_tree(stage);
        if (streams > 1)
            for (const fs::path& t : tracks) fs::remove(t, ec);
    };
    auto failed = [&](const std::string& why) {
        clean();
        error = why;
        return false;
    };
    auto run = [&](const std::vector<std::string>& argv,
                   const std::function<void(const std::string&)>& on_line) {
        const int rc = exec(argv, on_line);
        if (rc == kCancelled) return failed(lmsg::err_cancelled.get());
        return rc == 0 || failed(lmsg::err_ffmpeg_extract_failed.get());
    };
    auto head = [&](const fs::path& track) {
        std::vector<std::string> argv{job.ffmpeg_exe, "-nostdin", "-y"};
        if (!job.auto_rotate) argv.push_back("-noautorotate");
        argv.insert(argv.end(), {"-i", track.string()});
        return argv;
    };
    clean();

    for (size_t tr = 0; tr < streams; tr++) {
        if (streams == 1) {
            tracks.push_back(in.path);
            break;
        }
        enter(Stage::Frames, fmt(lmsg::stage_split_track, {(long long)tr}));
        tracks.push_back(ws / ("track_cam" + std::to_string(tr) + ".mp4"));
        const int rc = exec({job.ffmpeg_exe, "-nostdin", "-y", "-i", in.path, "-map",
                             "0:v:" + std::to_string(tr), "-c", "copy", tracks.back().string()});
        if (rc == kCancelled) return failed(lmsg::err_cancelled.get());
        if (rc != 0) return failed(lmsg::err_ffmpeg_split_failed.get());
    }

    char rate[64];
    std::snprintf(rate, sizeof rate, "fps=%g", (double)input_fps(job, in) * group);
    RateLimitedProgress progress(_prog, Stage::Frames, lmsg::noun_frames_written,
                                 _frames_tally);
    progress.update(0, /*force=*/true);

    // The candidates' 0-based numbers, which are the stems FrameSelect would give.
    std::vector<long long> keep;
    if (!every) {
        enter(Stage::Frames, window > 1 ? lmsg::stage_extract_candidates.get()
                                        : lmsg::stage_extract_ffmpeg.get());
        fs::create_directories(cand, ec);
        std::vector<std::string> argv = head(tracks[0]);
        argv.insert(argv.end(), {"-vf", rate, "-qscale:v", "2",
                                 (cand / "c_%06d.jpg").string()});
        const int max_frames = job.max_frames;
        if (!run(argv, [&progress, window, max_frames](const std::string& line) {
                const long long frame = progress_frame(line);
                if (frame < 0) return;
                int64_t projected = (frame + window - 1) / window;
                if (max_frames > 0) projected = std::min<int64_t>(projected, max_frames);
                progress.update(projected);
            }))
            return false;

        if (window > 1) enter(Stage::Frames, lmsg::stage_select_sharpest.get());
        FrameSelectOptions so;
        so.group = group;
        so.max_frames = job.max_frames;
        so.adaptive = job.adaptive_fps;
        so.range = job.adaptive_range;
        so.window = window;
        if (fisheye) so.view = app::MotionView::Fisheye;
        so.out_fov = fisheye ? 3.4034f : 1.5708f;
        ScanProgress scan(_prog, 0);
        scan.begin(leaf_name(in.path));
        const size_t row = input_index(job, in);
        _prog->scan_open(row);
        so.scanning = [&](int64_t done, int64_t total) { scan.update(done, total); };
        so.measured = [this, row](int64_t at, int64_t of, float c) {
            _prog->scan_step(row, at, of, c);
        };
        so.planned = [this, row](const std::vector<int64_t>& plan, int64_t frames) {
            _prog->scan_kept(row, spacing_bars(plan, frames), (int64_t)plan.size());
        };
        const int n = select_sharpest_frames(
            cand.string(), kept.string(), "", so,
            [this](const std::string& l) { log(l); }, _cancel);
        if (n < 0) return failed(_cancel.load() ? "cancelled" : "frame selection failed");
        for (const auto& e : fs::directory_iterator(kept, ec))
            keep.push_back(std::atoll(e.path().stem().string().c_str()));
        std::sort(keep.begin(), keep.end());
        remove_tree(cand);
        remove_tree(kept);
        if (keep.empty()) return failed(lmsg::err_no_frames_extracted.get());
    }

    long long per_track = (long long)keep.size();
    if (every) per_track = job.max_frames > 0 ? std::min<long long>(frames, job.max_frames) : frames;
    // 40 MB per 3840² PNG16: the mean over 374 frames of a 187 s D-Log M clip.
    const double need = 40.0e6 * (double)std::max<long long>(per_track, 1) * (double)streams *
                        ((double)width * height / (3840.0 * 3840.0));
    auto gb = [](double bytes) {
        char s[32];
        std::snprintf(s, sizeof s, "%.1f", bytes / 1e9);
        return std::string(s);
    };
    log(fmt(lmsg::frames_16bit_estimate, {gb(need), per_track * (long long)streams}),
        /*detail=*/false);
    std::error_code sec;
    const double free = job.free_space ? (double)job.free_space(ws.string())
                                       : (double)fs::space(ws, sec).available;
    if (!sec) {
        const DiskVerdict v = disk_verdict(need, free);
        if (v == DiskVerdict::TooBig)
            return failed(fmt(lmsg::err_frames_16bit_disk, {gb(need), gb(free), ws.string()}));
        if (v == DiskVerdict::Tight)
            log(fmt(lmsg::frames_16bit_disk_tight, {gb(need), gb(free), ws.string()}),
                /*detail=*/false);
    }

    // Full range at 12 bits before the RGB conversion: swscale's own 10-bit to
    // rgb48 misses BT.709 by up to 8.2% of full scale on saturated chroma and
    // lands Y=940 on 65283 (docs/notes/dlog-m.md, "Bit depth").
    std::string vf = "colorspace=iall=bt709:irange=tv:all=bt709:range=pc:format=yuv444p12";
    if (!every) {
        vf = std::string(rate) + "," + vf + ",select='" + select_expr(keep, 0, keep.size()) + "'";
        if (vf.size() > kMaxArgBytes)
            return failed(fmt(lmsg::err_frames_16bit_argv,
                              {(long long)keep.size(),
                               (long long)(keep.size() * kMaxArgBytes / vf.size())}));
    }
    const std::vector<std::string> pass = passthrough_args(job.ffmpeg_exe);
    enter(Stage::Frames, lmsg::stage_extract_ffmpeg.get());
    int64_t written = 0;
    for (size_t tr = 0; tr < streams; tr++) {
        const fs::path dir = streams > 1 ? stage / ("cam" + std::to_string(tr)) : stage;
        fs::create_directories(dir, ec);
        std::vector<std::string> argv = head(tracks[tr]);
        argv.insert(argv.end(), {"-vf", vf});
        argv.insert(argv.end(), pass.begin(), pass.end());
        if (every) {
            if (job.max_frames > 0)
                argv.insert(argv.end(), {"-frames:v", std::to_string(job.max_frames)});
            fs::create_directories(cand, ec);
        } else {
            argv.insert(argv.end(), {"-frame_pts", "1"});
        }
        argv.insert(argv.end(), {"-pix_fmt", "rgb48be", "-f", "image2",
                                 (every ? cand / "c_%06d.png" : dir / "%05d.png").string()});
        const int64_t base = written;
        if (!run(argv, [&progress, base](const std::string& line) {
                const long long frame = progress_frame(line);
                if (frame >= 0) progress.update(base + frame);
            }))
            return false;
        // Renamed by their place in the file, dropping the repeats of a frame
        // exactly as the 8-bit path does.
        if (every) {
            FrameSelectOptions so;
            so.window = 1;
            if (select_sharpest_frames(cand.string(), dir.string(), "", so,
                                       [this](const std::string& l) { log(l); }, _cancel) < 0)
                return failed(_cancel.load() ? "cancelled" : "frame selection failed");
            remove_tree(cand);
        }

        std::vector<long long> got;
        for (const auto& e : fs::directory_iterator(dir, ec))
            if (e.is_regular_file(ec))
                got.push_back(std::atoll(e.path().stem().string().c_str()));
        std::sort(got.begin(), got.end());
        if (got.empty() || (!every && got != keep))
            return failed(fmt(lmsg::err_frames_16bit_count,
                              {in.path, (long long)got.size(),
                               (long long)(every ? got.size() : keep.size())}));
        written += (int64_t)got.size();
        progress.update(written, /*force=*/true);
    }

    // Moved in one batch, so a folder with frames in it is a finished input
    // (extract_video's resume rule).
    for (size_t tr = 0; tr < streams; tr++) {
        const fs::path from = streams > 1 ? stage / ("cam" + std::to_string(tr)) : stage;
        const fs::path to = streams > 1 ? fs::path(images) / ("cam" + std::to_string(tr))
                                        : fs::path(images);
        fs::create_directories(to, ec);
        for (const auto& e : fs::directory_iterator(to, ec))
            if (e.path().extension() == ".jpg")
                return failed(fmt(lmsg::err_frames_16bit_mixed, {to.string()}));
        std::vector<fs::path> files;
        for (const auto& e : fs::directory_iterator(from, ec)) files.push_back(e.path());
        for (const fs::path& f : files) {
            fs::rename(f, to / f.filename(), ec);
            if (ec) return failed(ec.message());
        }
        log(fmt(lmsg::kept_frames, {(long long)files.size(), to.string()}), /*detail=*/false);
    }
    clean();
    return true;
}

// ---------------------------------------------------------------------------
// 360 -> views
// ---------------------------------------------------------------------------

namespace {

// The canvas frames selection kept, resampled into the plan's views. One
// thread per view, each with a share of the cores: the resampler threads and
// the JPEG encode does not, so overlapping them is what fills the machine.
bool warp_canvases(const fs::path& from, const fs::path& to,
                   const app::Pano360Layout& layout,
                   const std::vector<app::Pano360View>& views,
                   const std::atomic<bool>& cancel,
                   const std::function<void(int64_t)>& progress,
                   std::string& error) {
    std::error_code ec;
    std::vector<app::Pano360Remap> maps(views.size());
    for (size_t i = 0; i < views.size(); i++) {
        app::pano360_remap(layout, views[i], maps[i]);
        fs::create_directories(to / views[i].dir, ec);
    }
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(from, ec))
        if (e.is_regular_file(ec)) files.push_back(e.path());
    std::sort(files.begin(), files.end());

    const int cores = std::max(1, (int)std::thread::hardware_concurrency());
    const int per_view = std::max(1, cores / (int)views.size());
    int64_t done = 0;
    for (const fs::path& f : files) {
        if (cancel.load()) { error = lmsg::err_cancelled.get(); return false; }
        int w = 0, h = 0, ch = 0;
        stbi_uc* px = stbi_load(f.string().c_str(), &w, &h, &ch, 3);
        if (!px || w != layout.canvasW() || h != layout.canvasH()) {
            if (px) stbi_image_free(px);
            error = fmt(lmsg::err_360_frame_read, {f.string()});
            return false;
        }
        std::atomic<bool> ok{true};
        std::vector<std::thread> pool;
        pool.reserve(views.size());
        for (size_t i = 0; i < views.size(); i++) {
            pool.emplace_back([&, i] {
                std::vector<uint8_t> out((size_t)maps[i].width * maps[i].height * 3);
                app::pano360_apply(maps[i], px, w, h, per_view, out.data());
                const std::string path =
                    (to / views[i].dir / (f.stem().string() + ".jpg")).string();
                if (!stbi_write_jpg(path.c_str(), maps[i].width, maps[i].height, 3,
                                    out.data(), kPhotoJpegQuality))
                    ok = false;
            });
        }
        for (std::thread& t : pool) t.join();
        stbi_image_free(px);
        if (!ok) {
            error = fmt(lmsg::err_360_frame_write, {(to / f.stem()).string()});
            return false;
        }
        if (progress) progress(++done);
    }
    return true;
}

}  // namespace

bool DatasetPrep::extract_360_ffmpeg(const PrepJob& job, const PrepInput& in,
                                     const std::string& images, PrepResult& out,
                                     std::string& error) {
    if (!command_exists(job.ffmpeg_exe)) {
        error = fmt(lmsg::err_ffmpeg_missing, {job.ffmpeg_exe});
        return false;
    }
    const std::vector<app::Pano360View> views =
        app::pano360_views(in.pano360, job.pano);
    if (views.empty()) {
        error = lmsg::err_ffmpeg_extract_failed.get();
        return false;
    }
    if (views.size() > 1) out.per_folder_cameras = true;
    log(fmt(lmsg::video_input, {in.path}), /*detail=*/false);
    log(fmt(lmsg::pano360_plan, {(long long)views.size(), views[0].width,
                                 views[0].height}), /*detail=*/false);

    const fs::path ws = job.workspace;
    const bool every = every_frame(job, in);
    const int window = every ? 1 : std::max(job.sharp_window, 1);
    const int group = candidate_group(job, in);
    std::error_code ec;

    // ffmpeg decodes both tracks and cuts the overlap strips out; the warp is
    // ours, in both decode paths, because ffmpeg's own EAC sampler insets every
    // face (app/Pano360.h).
    enter(Stage::Frames, window > 1 ? lmsg::stage_extract_candidates.get()
                                    : lmsg::stage_extract_ffmpeg.get());
    const fs::path cand = ws / "frames_tmp";
    remove_tree(cand);
    fs::create_directories(cand, ec);
    char pre[64] = "";
    if (!every)
        std::snprintf(pre, sizeof pre, "fps=%g", (double)input_fps(job, in) * group);
    const std::string graph = app::pano360_graph(in.pano360, pre);
    // A 360 capture's geometry is the EAC layout, not the display matrix: the
    // built-in path leaves it alone and so must this one.
    std::vector<std::string> argv{job.ffmpeg_exe, "-nostdin", "-y", "-noautorotate",
                                  "-i", in.path, "-filter_complex", graph, "-map",
                                  std::string("[") + app::pano360_canvas_pad() + "]"};
    if (every) append_every_frame_args(argv, job.ffmpeg_exe, job.max_frames);
    argv.insert(argv.end(), {"-qscale:v", "2", (cand / "c_%06d.jpg").string()});
    int rc = exec(argv);
    if (rc == kCancelled) { error = lmsg::err_cancelled.get(); return false; }
    if (rc != 0) {
        error = lmsg::err_ffmpeg_extract_failed.get();
        return false;
    }

    // Selection scores the canvas, so a frame is ranked on the whole sphere
    // rather than on whichever view happens to be pointing at texture.
    if (window > 1) enter(Stage::Frames, lmsg::stage_select_sharpest.get());
    const fs::path kept = ws / "canvas_tmp";
    remove_tree(kept);
    fs::create_directories(kept, ec);
    FrameSelectOptions so;
    so.group = group;
    so.max_frames = every ? 0 : job.max_frames;
    so.adaptive = job.adaptive_fps && !every;
    so.range = job.adaptive_range;
    so.window = window;
    so.view = app::MotionView::Packed360;
    so.eac = in.pano360;
    so.out_fov = app::motion_out_fov(views);
    ScanProgress scan(_prog, 0);
    scan.begin(leaf_name(in.path));
    const size_t row = input_index(job, in);
    _prog->scan_open(row);
    so.scanning = [&](int64_t done, int64_t total) { scan.update(done, total); };
    so.measured = [this, row](int64_t at, int64_t of, float c) {
        _prog->scan_step(row, at, of, c);
    };
    so.planned = [this, row](const std::vector<int64_t>& plan, int64_t frames) {
        _prog->scan_kept(row, spacing_bars(plan, frames), (int64_t)plan.size());
    };
    const int n = select_sharpest_frames(
        cand.string(), kept.string(), "", so,
        [this](const std::string& l) { log(l); }, _cancel);
    remove_tree(cand);
    if (n < 0) {
        remove_tree(kept);
        error = _cancel.load() ? lmsg::err_cancelled.get()
                               : lmsg::err_ffmpeg_extract_failed.get();
        return false;
    }

    enter(Stage::Frames, lmsg::stage_warp_360.get());
    RateLimitedProgress progress(_prog, Stage::Frames, lmsg::noun_frames_written,
                                 _frames_tally);
    const bool ok = warp_canvases(
        kept, fs::path(images), in.pano360, views, _cancel,
        [&](int64_t done) { progress.update(done * (int64_t)views.size()); },
        error);
    remove_tree(kept);
    if (!ok) return false;
    log(fmt(lmsg::kept_frames, {(long long)n * (long long)views.size(), images}),
        /*detail=*/false);
    return true;
}

// ---------------------------------------------------------------------------
// Photos -> the dataset's own images/
// ---------------------------------------------------------------------------

namespace {

// Extensions worth handing to the re-encoder. EXR is HDR, and the two stb
// cannot decode (TIFF, WebP) would only reach the fallback. A JPEG is already
// JPEG and is re-encoded only to bake in an orientation (photo_exif_turn).
bool jpeg_candidate_ext(const fs::path& f) {
    std::string e = f.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".png" || e == ".bmp";
}

bool is_jpeg_ext(const fs::path& f) {
    std::string e = f.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg";
}

// A photo whose EXIF asks to be turned before it is shown. Baking that in is
// what keeps the dataset readable by everything downstream: the trainer, the
// viewer and COLMAP all take the stored pixels at face value.
int photo_exif_turn(const fs::path& f) {
    return is_jpeg_ext(f) ? sfm::exifOrientation(f.string()) : 1;
}

// JPEG bytes plus the source's own EXIF, with the orientation flattened out of
// it. stb writes no metadata, so the segment is spliced in behind the SOI --
// without it a re-encode would drop the focal-length prior and the GPS.
bool write_jpeg_with_exif(const fs::path& to, int w, int h, int channels,
                          const stbi_uc* px, std::vector<uint8_t> exif,
                          bool drop_focal = false) {
    std::vector<uint8_t> jpeg;
    auto sink = [](void* ctx, void* data, int size) {
        auto* out = (std::vector<uint8_t>*)ctx;
        out->insert(out->end(), (uint8_t*)data, (uint8_t*)data + size);
    };
    if (!stbi_write_jpg_to_func(sink, &jpeg, w, h, channels, px, kPhotoJpegQuality))
        return false;
    if (jpeg.size() < 2) return false;
    // 65533 is all a segment's 16-bit length can address; a maker note that
    // long is dropped rather than written back malformed.
    if (exif.size() > 6 && exif.size() + 2 <= 65535) {
        sfm::exifFlattenOrientation(exif.data() + 6, exif.size() - 6, w, h);
        if (drop_focal) sfm::exifClearFocal(exif.data() + 6, exif.size() - 6);
        const size_t len = exif.size() + 2;
        const uint8_t head[4] = {0xFF, 0xE1, (uint8_t)(len >> 8), (uint8_t)len};
        jpeg.insert(jpeg.begin() + 2, exif.begin(), exif.end());
        jpeg.insert(jpeg.begin() + 2, head, head + 4);
    }
    std::ofstream f(to, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((const char*)jpeg.data(), (std::streamsize)jpeg.size());
    return (bool)f;
}

// Decode, turn by `orientation`, then JPEG. Alpha is a cut-out, not decoration,
// so it becomes `mask_to` gated at 128 (opaque = keep). The mask is written
// FIRST: a resumed run reads the photo's existence as proof the pair is complete.
bool convert_to_jpeg(const fs::path& from, const fs::path& to,
                     const fs::path& mask_to, int orientation, bool& wrote_mask) {
    wrote_mask = false;
    const std::string src = from.string();
    int w = 0, h = 0, ch = 0;
    if (!stbi_info(src.c_str(), &w, &h, &ch)) return false;
    if (ch < 1 || ch > 4 || stbi_is_16_bit(src.c_str())) return false;
    const bool alpha = ch == 2 || ch == 4;
    // Nowhere to put the cut-out: copying keeps it, re-encoding would lose it.
    if (alpha && mask_to.empty()) return false;
    stbi_uc* px = stbi_load(src.c_str(), &w, &h, &ch, 0);
    if (!px) return false;

    const int color = alpha ? ch - 1 : ch;
    const size_t n = (size_t)w * (size_t)h;
    std::vector<stbi_uc> opaque, mask;
    std::error_code ec;
    bool ok = true;
    if (alpha) {
        opaque.resize(n * (size_t)color);
        mask.resize(n);
        for (size_t i = 0; i < n; i++) {
            for (int c = 0; c < color; c++)
                opaque[i * (size_t)color + (size_t)c] = px[i * (size_t)ch + (size_t)c];
            mask[i] = px[i * (size_t)ch + (size_t)color] >= 128 ? 255 : 0;
        }
    }
    const stbi_uc* rgb = alpha ? opaque.data() : px;

    // The mirror is applied here as freely as the turn: these are pixels, and
    // the reconstruction is fitted to what this writes.
    const sfm::ExifTransform xf = sfm::exifTransform(orientation);
    std::vector<stbi_uc> turned_rgb, turned_mask;
    int dw = w, dh = h;
    if (!xf.identity()) {
        spirula::oriented_size(xf.turns_cw, dw, dh);
        turned_rgb.resize(n * (size_t)color);
        spirula::orient_pixels(rgb, w, h, color, xf.turns_cw, xf.mirror,
                               turned_rgb.data());
        rgb = turned_rgb.data();
        if (alpha) {
            turned_mask.resize(n);
            spirula::orient_pixels(mask.data(), w, h, 1, xf.turns_cw, xf.mirror,
                                   turned_mask.data());
            mask.swap(turned_mask);
        }
    }

    if (alpha) {
        fs::create_directories(mask_to.parent_path(), ec);
        ok = stbi_write_png(mask_to.string().c_str(), dw, dh, 1, mask.data(), dw) != 0;
    }
    if (ok)
        ok = write_jpeg_with_exif(to, dw, dh, color, rgb,
                                  sfm::readExifSegment(src));
    stbi_image_free(px);
    // A half-written pair is worse than none: the next run would keep it.
    if (!ok) {
        fs::remove(to, ec);
        if (alpha) fs::remove(mask_to, ec);
        return false;
    }
    wrote_mask = alpha;
    return true;
}

// `from`'s lenses left to right, one file each in `to`, each turned by
// `orientation` after the cut: the lenses sit side by side in the STORED
// pixels. The last is written last, so its existence means the set is whole.
bool split_packed_image(const fs::path& from, const std::vector<fs::path>& to,
                        int orientation) {
    const std::string src = from.string();
    int w = 0, h = 0, ch = 0;
    stbi_uc* px = stbi_load(src.c_str(), &w, &h, &ch, 3);
    if (!px) return false;
    const std::vector<uint8_t> exif = sfm::readExifSegment(src);
    const sfm::ExifTransform xf = sfm::exifTransform(orientation);
    std::error_code ec;
    bool ok = true;
    for (size_t k = 0; k < to.size() && ok; k++) {
        std::vector<uint8_t> lens;
        int lw = 0, lh = h;
        app::packed_lens_crop(px, w, h, 3, (int)to.size(), (int)k, lens, lw);
        app::turn_pixels(xf, 3, lens, lw, lh);
        fs::create_directories(to[k].parent_path(), ec);
        ok = is_jpeg_ext(to[k])
                 ? write_jpeg_with_exif(to[k], lw, lh, 3, lens.data(), exif,
                                        /*drop_focal=*/true)
                 : stbi_write_png(to[k].string().c_str(), lw, lh, 3, lens.data(),
                                  lw * 3) != 0;
    }
    stbi_image_free(px);
    if (!ok)
        for (const fs::path& f : to) fs::remove(f, ec);
    return ok;
}

// The warning for a packed frame of neither shape, once per size.
void note_packed_shape(const fs::path& f, int w, int h, int lenses,
                       std::set<std::pair<int, int>>& warned,
                       std::vector<std::string>& notes) {
    if (!warned.insert({w, h}).second) return;
    notes.push_back(fmt(lenses >= 2 ? lmsg::packed_shape_as_dual
                                    : lmsg::packed_shape_as_single,
                        {f.string(), w, h}));
}

// One photo's journey. `fallback` is where it goes when the re-encode cannot
// happen after all -- its own name, which nothing else can have claimed.
// `mask_to` is empty for a photo that gets no mask of its own.
struct PhotoMove {
    fs::path from, to, fallback, mask_to;
    bool convert = false;
    int orientation = 1;   // EXIF, baked into the pixels by the re-encode
    // A packed .insp: one JPEG per lens, `to` being the last of them.
    std::vector<fs::path> split;
};

// A re-encoded photo takes the .jpg its bytes now are; the parsers match a
// mask by stem (find_aux_file), so that is free. A name already spoken for --
// a.jpg beside a.png, or two stems meeting in `mask_root` -- is not taken twice.
std::vector<PhotoMove> plan_photo_moves(const std::vector<fs::path>& files,
                                        const fs::path& from, const fs::path& to,
                                        const fs::path& mask_root, bool convert,
                                        std::vector<std::string>& notes) {
    std::vector<PhotoMove> plan;
    plan.reserve(files.size());
    std::set<fs::path> taken, mask_taken;
    std::set<std::pair<int, int>> warned;
    for (const fs::path& f : files)
        taken.insert(to / under_root(f, from));
    for (const fs::path& f : files) {
        PhotoMove m;
        m.from = f;
        const fs::path rel = under_root(f, from);
        m.to = m.fallback = to / rel;
        if (is_packed_lens_path(f.string())) {
            // Always cut and re-encoded, whatever the import mode: nothing
            // downstream reads an .insp by that name.
            int w = 0, h = 0, ch = 0;
            bool exact = true;
            const int lenses = stbi_info(f.string().c_str(), &w, &h, &ch)
                                   ? app::packed_lens_count(w, h, exact)
                                   : 1;
            if (!exact) note_packed_shape(f, w, h, lenses, warned, notes);
            std::string stem = rel.stem().string();
            for (int k = 0; k < lenses; k++) {
                const fs::path dir = lenses > 1 ? to / ("cam" + std::to_string(k)) : to;
                fs::path cand = dir / rel.parent_path() / (stem + ".jpg");
                if (k == 0 && taken.count(cand)) {
                    stem += "_insp";
                    cand = dir / rel.parent_path() / (stem + ".jpg");
                }
                taken.insert(cand);
                m.split.push_back(cand);
            }
            m.to = m.split.back();
            m.orientation = sfm::exifOrientation(f.string());
            plan.push_back(std::move(m));
            continue;
        }
        if (convert && jpeg_candidate_ext(f)) {
            const fs::path cand =
                m.to.parent_path() / (m.to.stem().string() + ".jpg");
            if (taken.insert(cand).second) {
                m.to = cand;
                m.convert = true;
            }
        } else if (convert) {
            // A JPEG keeps its name, so nothing has to be claimed: it is
            // re-encoded only when its EXIF asks for the picture to be turned.
            m.orientation = photo_exif_turn(f);
            m.convert = m.orientation != 1;
        }
        // Only a format that can carry alpha needs a name held for its mask.
        if (m.convert && !mask_root.empty() && !is_jpeg_ext(f)) {
            const fs::path cand =
                mask_root / rel.parent_path() / (rel.stem().string() + ".png");
            if (mask_taken.insert(cand).second) m.mask_to = cand;
        }
        plan.push_back(std::move(m));
    }
    return plan;
}

// What a run did with one tree, for the line it logs afterwards.
struct GatherTally {
    std::atomic<int> converted{0}, linked{0}, copied{0}, moved{0}, kept{0};
    std::atomic<int> masked{0};
    std::atomic<int64_t> done{0};
};

}  // namespace

// Copying hard-links where the filesystem gives one, which costs a directory
// entry rather than a second copy of a folder of raw captures; falling back to
// a real copy is what makes it work across devices and where links do not.
bool DatasetPrep::gather_photos(const PrepJob& job, const PrepInput& in,
                                const std::string& images,
                                const std::string& masks, bool& have_masks,
                                std::string& error) {
    std::error_code ec;
    const fs::path src = fs::absolute(in.path, ec);
    if (!fs::is_directory(src, ec)) {
        error = fmt(lmsg::err_not_a_folder, {in.path});
        return false;
    }
    enter(Stage::Frames, lmsg::stage_collecting_photos.get());

    // The masks come across too, into the folder that mirrors the images -- a
    // mask is found by its image's relative name, so the two trees have to move
    // together or every mask stops matching.
    const bool with_masks = !in.mask_dir.empty();
    // Photos and masks move the same way but are named differently at every
    // step, so each carries its three messages rather than a noun that gets
    // pasted into an English sentence.
    struct Tree {
        fs::path from, to;
        bool photos;
        const spirula::i18n::Msg* moving;   // "<kind>: <from> -> <to>"
        const spirula::i18n::Msg* empty;    // "there are no <kind> in <from>"
        const spirula::i18n::Msg* counted;  // what the progress line counts
    };
    std::vector<Tree> trees{{src, fs::path(images), true, &lmsg::copying_photos,
                             &lmsg::err_no_photos_in,
                             &lmsg::noun_photos_collected}};
    if (with_masks)
        trees.push_back({fs::absolute(in.mask_dir, ec), fs::path(masks), false,
                         &lmsg::copying_masks, &lmsg::err_no_masks_in,
                         &lmsg::noun_masks_collected});

    // Masks this pass made out of the photos' alpha, which is what tells the
    // run there are masks in the dataset even though nothing asked for any.
    bool derived_any_masks = false;
    for (const Tree& t : trees) {
        // The output folder is often the input folder -- the point of the
        // images/ + masks/ layout -- and then there is nothing to do. A
        // re-encode there would be rewriting the capture.
        std::error_code same_ec;
        if (fs::exists(t.to, same_ec) &&
            fs::equivalent(t.from, t.to, same_ec) && !same_ec) {
            log(fmt(lmsg::photos_already_in_dataset, {t.to.string()}),
                /*detail=*/false);
            continue;
        }
        log(fmt(*t.moving, {t.from.string(), t.to.string()}), /*detail=*/false);
        // A destination nested inside the source would otherwise be walked as
        // input, so a resumed run would gather its own output.
        const std::vector<fs::path> files =
            walk_images(t.from, inside(t.to, t.from) ? t.to : fs::path());
        if (files.empty()) {
            error = fmt(*t.empty, {t.from.string()});
            return false;
        }
        // Masks are never re-encoded: they are binary, and JPEG's ringing
        // around every edge is exactly what a mask cannot survive.
        const bool convert =
            t.photos && job.photo_import == PhotoImport::ConvertJpeg;
        const bool move = job.photo_import == PhotoImport::Move;
        // An alpha channel becomes a mask -- but not over a masks/ the input
        // brought, which is the answer its owner already gave and which this
        // loop fills after the photos.
        const fs::path derived_masks =
            convert && !with_masks ? fs::path(masks) : fs::path();
        std::vector<std::string> shape_notes;
        const std::vector<PhotoMove> plan = plan_photo_moves(
            files, t.from, t.to, derived_masks, convert, shape_notes);
        for (const std::string& n : shape_notes) log(n, /*detail=*/false);
        bool any_split = false;
        for (const PhotoMove& m : plan) any_split = any_split || !m.split.empty();

        GatherTally tally;
        std::mutex notes_mu;
        std::vector<std::string> notes;   // capped in step()
        std::atomic<size_t> next{0};
        std::atomic<bool> stop{false};
        std::string failure;              // guarded by notes_mu

        auto step = [&](const PhotoMove& m) {
            std::error_code probe_ec;
            if (fs::exists(m.to, probe_ec)) { tally.kept++; return true; }
            std::error_code dir_ec;
            fs::create_directories(m.to.parent_path(), dir_ec);
            if (dir_ec && !fs::is_directory(m.to.parent_path(), dir_ec)) {
                std::lock_guard<std::mutex> lk(notes_mu);
                if (failure.empty())
                    failure = fmt(lmsg::err_copy_failed,
                                  {m.from.string(), t.to.string(),
                                   dir_ec.message()});
                return false;
            }
            if (!m.split.empty()) {
                if (split_packed_image(m.from, m.split, m.orientation)) {
                    tally.converted++;
                    return true;
                }
                std::lock_guard<std::mutex> lk(notes_mu);
                if (failure.empty())
                    failure = fmt(lmsg::err_packed_split_failed, {m.from.string()});
                return false;
            }
            if (m.convert) {
                bool wrote_mask = false;
                if (convert_to_jpeg(m.from, m.to, m.mask_to, m.orientation,
                                    wrote_mask)) {
                    tally.converted++;
                    if (wrote_mask) tally.masked++;
                    return true;
                }
                // 16-bit, or a format stb cannot read. Its own name is free --
                // plan_photo_moves reserved it.
                std::lock_guard<std::mutex> lk(notes_mu);
                if (notes.size() < 20)
                    notes.push_back(fmt(lmsg::photo_kept_unconverted,
                                        {m.from.string()}));
            }
            const fs::path& dst = m.convert ? m.fallback : m.to;
            if (m.convert && fs::exists(dst, probe_ec)) { tally.kept++; return true; }
            std::error_code op_ec;
            if (move) {
                fs::rename(m.from, dst, op_ec);
                if (!op_ec) { tally.moved++; return true; }
                // Another filesystem: copy, then drop the original only once
                // the copy is on disk.
                op_ec.clear();
                fs::copy_file(m.from, dst, op_ec);
                if (!op_ec) {
                    std::error_code rm_ec;
                    fs::remove(m.from, rm_ec);
                    tally.moved++;
                    return true;
                }
            } else {
                fs::create_hard_link(m.from, dst, op_ec);
                if (!op_ec) { tally.linked++; return true; }
                op_ec.clear();
                fs::copy_file(m.from, dst, op_ec);
                if (!op_ec) { tally.copied++; return true; }
            }
            std::lock_guard<std::mutex> lk(notes_mu);
            if (failure.empty())
                failure = fmt(lmsg::err_copy_failed,
                              {m.from.string(), t.to.string(), op_ec.message()});
            return false;
        };

        // Copying and moving are the disk's work, so one thread; the re-encode
        // is the CPU's, ~60 ms a photo. Each worker holds a decoded frame that
        // glibc faults in on every call: 24 MB at 4K, ~320 MB for a 12K .insp.
        const unsigned cores = std::thread::hardware_concurrency();
        const int threads =
            convert || any_split
                ? (int)std::clamp<unsigned>(cores ? cores : 1u, 1u, any_split ? 4u : 8u)
                : 1;
        std::atomic<int> live{0};
        auto worker = [&] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= plan.size() || stop.load() || _cancel.load()) break;
                if (!step(plan[i])) { stop.store(true); break; }
                tally.done.fetch_add(1);
            }
            live.fetch_sub(1);
        };
        RateLimitedProgress progress(_prog, Stage::Frames, *t.counted,
                                     _frames_tally);
        std::vector<std::thread> pool;
        pool.reserve((size_t)threads);
        for (int i = 0; i < threads; i++) {
            live.fetch_add(1);
            try {
                pool.emplace_back(worker);
            } catch (const std::system_error&) {
                live.fetch_sub(1);
                break;
            }
        }
        if (pool.empty()) {
            // No thread could be started, so this one does the plan itself.
            for (size_t i = 0; i < plan.size(); i++) {
                if (_cancel.load()) break;
                if (!step(plan[i])) { stop.store(true); break; }
                tally.done.fetch_add(1);
                progress.update(tally.done.load());
            }
        } else {
            while (live.load() > 0) {
                progress.update(tally.done.load());
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            for (std::thread& th : pool) th.join();
        }
        progress.update(tally.done.load());

        if (!failure.empty()) { error = failure; return false; }
        if (_cancel.load()) { error = lmsg::err_cancelled.get(); return false; }
        for (const std::string& n : notes) log(n);
        if (move)
            log(fmt(lmsg::moved_kept,
                    {(long long)tally.moved.load(), (long long)tally.kept.load()}));
        else if (convert)
            log(fmt(lmsg::converted_copied_kept,
                    {(long long)tally.converted.load(),
                     (long long)(tally.linked.load() + tally.copied.load()),
                     (long long)tally.kept.load()}));
        else
            log(fmt(lmsg::linked_copied_kept,
                    {(long long)tally.linked.load(),
                     (long long)tally.copied.load(),
                     (long long)tally.kept.load()}));
        if (tally.masked.load() > 0) {
            log(fmt(lmsg::masks_from_alpha,
                    {(long long)tally.masked.load(), masks}),
                /*detail=*/false);
            derived_any_masks = true;
        }
    }
    if (with_masks || derived_any_masks) have_masks = true;
    return true;
}

bool DatasetPrep::split_packed_frames(const PrepInput& in,
                                      const std::string& images,
                                      PrepResult& out, std::string& error) {
    if (in.packed_lenses < 2) return true;
    out.per_folder_cameras = true;
    std::error_code ec;
    std::vector<fs::path> frames;
    for (fs::directory_iterator it(images, ec), end; !ec && it != end;
         it.increment(ec))
        if (it->is_regular_file(ec) && is_image_file(it->path()))
            frames.push_back(it->path());
    if (frames.empty()) return true;
    std::sort(frames.begin(), frames.end());

    int w = 0, h = 0, ch = 0;
    bool exact = true;
    if (stbi_info(frames[0].string().c_str(), &w, &h, &ch))
        app::packed_lens_count(w, h, exact);
    if (!exact) {
        std::set<std::pair<int, int>> warned;
        std::vector<std::string> notes;
        note_packed_shape(fs::path(in.path), w, h, in.packed_lenses, warned, notes);
        for (const std::string& n : notes) log(n, /*detail=*/false);
    }

    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::mutex mu;
    auto worker = [&] {
        for (;;) {
            const size_t i = next.fetch_add(1);
            if (i >= frames.size() || failed.load() || _cancel.load()) return;
            const fs::path& f = frames[i];
            std::vector<fs::path> to;
            for (int k = 0; k < in.packed_lenses; k++)
                to.push_back(fs::path(images) / ("cam" + std::to_string(k)) /
                             f.filename());
            std::error_code fec;
            if (fs::exists(to.back(), fec) || split_packed_image(f, to, 1)) {
                fs::remove(f, fec);
                continue;
            }
            std::lock_guard<std::mutex> lk(mu);
            if (!failed.exchange(true))
                error = fmt(lmsg::err_packed_split_failed, {f.string()});
        }
    };
    const unsigned cores = std::thread::hardware_concurrency();
    const int threads = (int)std::clamp<unsigned>(cores ? cores : 1u, 1u, 8u);
    std::vector<std::thread> pool;
    for (int t = 1; t < threads; t++) pool.emplace_back(worker);
    worker();
    for (std::thread& t : pool) t.join();
    if (failed.load()) return false;
    if (_cancel.load()) {
        error = lmsg::err_cancelled.get();
        return false;
    }
    log(fmt(lmsg::packed_frames_split, {images}), /*detail=*/false);
    return true;
}

// ---------------------------------------------------------------------------
// Masks
// ---------------------------------------------------------------------------

bool DatasetPrep::generate_masks(const PrepJob& job, const PrepInput& in,
                                 const std::string& images,
                                 const std::string& images_rel,
                                 const std::string& masks,
                                 const std::string& masks_rel,
                                 bool& folded, std::string& error) {
    if (!job.force_external_masking && backends().builtin_masking) {
        // A missing checkpoint is a download, not an install. Falling through
        // to the Python masker answered "the model is not here" with "pip
        // install lang-segment-anything", which is advice for another problem.
        if (job.mask_model_path.empty()) {
            error = lmsg::err_mask_model_not_downloaded.get();
            return false;
        }
        return generate_masks_builtin(job, in, images, masks, folded, error);
    }
    if (!job.mask_clicks.empty()) {
        // The Python fallback is lang-segment-anything: text in, masks out. It
        // has no way to take a click, so saying so beats writing masks that
        // quietly ignore half of what the user asked for.
        error = lmsg::err_clicks_need_builtin.get();
        return false;
    }
    return generate_masks_python(job, images_rel, masks_rel, error);
}

bool DatasetPrep::generate_masks_builtin(const PrepJob& job, const PrepInput& in,
                                         const std::string& images,
                                         const std::string& masks,
                                         bool& folded, std::string& error) {
#ifndef SS_BUILD_SAM
    (void)job; (void)in; (void)images; (void)masks; (void)folded;
    error = backends().masking_reason;
    return false;
#else
    enter(Stage::Masks, lmsg::stage_masks_builtin.get());

    std::error_code ec;
    // Never the masks themselves: a nested masks/ is only possible for photos
    // read where they are, and those already have masks -- but the guard costs
    // nothing and the alternative is masking a folder of masks.
    const std::vector<fs::path> files = walk_images(images, masks);
    if (files.empty()) {
        error = lmsg::err_no_images_to_mask.get();
        return false;
    }
    const fs::path image_root(images);
    // Frames the built-in decoder wrote are still named after the source frame
    // they were chosen from; ffmpeg's were renumbered by frame selection, and a
    // photo folder's clicks were recorded against its own sorted order.
    std::vector<int64_t> ids;
    const bool by_stem = in.is_video && builtin_decode(job, in) &&
                         frame_ids_from_stems(files, ids);
    if (!by_stem) {
        ids.resize(files.size());
        for (size_t i = 0; i < ids.size(); i++) ids[i] = (int64_t)i;
    }
    // Which camera folder each file belongs to, keyed as the preview keyed a
    // click (SegmentPanel::shown_camera).
    std::vector<std::string> cameras;
    for (const fs::path& f : files)
        cameras.push_back(under_root(f, image_root).parent_path().generic_string());

    sam::MaskOptions mo;
    mo.model = job.mask_model_path;
    mo.device = job.device;
    mo.text = job.mask_prompt;
    mo.neg_text = job.mask_negative_prompt;
    mo.keep_prompted = job.mask_keep_subject;
    mo.max_size = job.mask_max_image_size;
    mo.dilate_ratio = job.mask_dilate_ratio;
    mo.threshold = job.mask_threshold;
    mo.nms = job.mask_nms;
    mo.detect_every = job.mask_detect_every;
    mo.memory_frames = job.mask_memory_frames;
    // Photos never get a memory bank -- tracking across unrelated frames would
    // carry one that does not apply -- and a video only when it was asked for.
    // A click needs one either way, since it says nothing about any frame but
    // its own.
    const std::vector<MaskClick> clicks = clicks_for(job, in);
    mo.video = (in.is_video && job.mask_memory) || !clicks.empty();
    // A click's own frame number survives whenever the numbering it was
    // recorded against did. ffmpeg resampled the video, and a packed photo's
    // preview counted the files before the cut: there only the fraction holds.
    mo.seeds = seeds_from_clicks(
        clicks, cameras, ids,
        /*exact=*/(!in.is_video && in.packed_lenses < 2) || by_stem);

    // The stencil goes in here rather than in a pass of its own: it is one AND
    // over a mask that is already in memory, against a decode and a re-encode
    // of every PNG on disk (~131 ms per 2880-square fisheye frame).
    StencilRaster stencil;
    if (!in.stencil.empty()) {
        stencil.build(in.stencil, image_root, files,
                      [&](const std::string& camera, const app::BorderDetect& d) {
                          const std::string name =
                              camera.empty() ? in.path : camera;
                          if (!in.stencil.detect_border) return;
                          if (!d.found) {
                              log(fmt(lmsg::frame_mask_no_border, {name}), false);
                              return;
                          }
                          char pct[16];
                          std::snprintf(pct, sizeof pct, "%.0f",
                                        100.0f * d.kept_fraction);
                          log(fmt(lmsg::frame_mask_border, {name, pct}), false);
                      });
        folded = true;
    }

    sam::Masker masker;
    if (!masker.init(mo, error)) return false;

    const fs::path mask_root(masks);
    fs::create_directories(mask_root, ec);

    // What a resumed run still has to do, decided before anything is read: the
    // prefetcher below reads ahead, and reading a frame whose mask is already
    // on disk is the one decode that buys nothing.
    std::vector<size_t> todo;
    std::vector<fs::path> todo_files, todo_dst;
    int done = 0;
    for (size_t i = 0; i < files.size(); i++) {
        // Masks mirror the image tree, so cam0/ and cam1/ keep their names.
        const fs::path rel = under_root(files[i], image_root);
        const fs::path dst = mask_root / rel.parent_path() /
                             (rel.stem().string() + ".png");
        if (job.resume && !job.redo_masks && fs::exists(dst, ec)) {
            ++done;
            continue;
        }
        fs::create_directories(dst.parent_path(), ec);
        todo.push_back(i);
        todo_files.push_back(files[i]);
        todo_dst.push_back(dst);
    }

    // Encoding a 1080p mask costs about a third of what the model costs to
    // produce it, and none of it needs the GPU.
    app::WriterPool writers;
    ImagePrefetch reader(todo_files, _cancel, job.image_gamut, job.image_is_linear);
    RateLimitedProgress progress(_prog, Stage::Masks, lmsg::noun_images_masked,
                                 _masks_tally);
    for (size_t k = 0; k < todo.size(); k++) {
        if (_cancel.load()) { error = lmsg::err_cancelled.get(); return false; }
        sfm::ExifTransform turn;
        nn::Image img = reader.take(turn);
        if (img.empty()) {
            log(fmt(lmsg::warn_unreadable_skipped, {todo_files[k].string()}), false);
            continue;
        }
        sam::Mask mask;
        if (!masker.run(img, mask, nullptr, ids[todo[k]])) {
            error = fmt(lmsg::err_masking_failed_on,
                        {todo_files[k].filename().string(), masker.lastError()});
            return false;
        }
        // Still the way up the model saw it, which is the frame the stencil's
        // shapes were drawn in.
        if (!stencil.apply(todo_files[k], image_root, mask, error)) return false;
        // And back into the frame the file stores, which is the one the
        // trainer reads it against (docs/datasets.md, "EXIF orientation") and
        // the one the reel re-reads a frame nobody watched go by in.
        const sfm::ExifTransform back = app::inverse_turn(turn);
        app::turn_pixels(back, 1, mask.data, mask.width, mask.height);
        app::turn_pixels(back, img.channels, img.data, img.width, img.height);
        if (_films.masks) {
            FilmFrame f;
            f.name = under_root(todo_files[k], image_root).generic_string();
            f.image_path = todo_files[k].string();
            f.mask_path = todo_dst[k].string();
            _films.masks->add(f, _films.masks->wants() ? img.data.data() : nullptr,
                              img.width, img.height, mask.data.data());
        }
        app::WriteJob wj;
        wj.mask = std::move(mask);
        wj.path = todo_dst[k].string();
        writers.submit(std::move(wj));
        // Sequenced deliberately: `update(++done, done == n)` leaves the
        // argument's read of `done` unsequenced against the increment, so
        // whether the last image is reported as the last one came down to the
        // compiler's evaluation order.
        ++done;
        progress.update(done, done == (int)files.size());
    }
    // The last few masks are still in the queue; the caller goes straight on to
    // structure from motion, which will look for them.
    writers.finish();
    if (writers.failures() > 0) {
        error = std::to_string(writers.failures()) + " mask(s) could not be "
                "written to " + mask_root.string();
        return false;
    }
    return true;
#endif
}

// Written into the destination when the fold flipped what it read, so a second
// run over the same folder does not flip an already-flipped mask.
static const char* kFlippedMarker = ".spirula-mask-convention";

bool DatasetPrep::apply_stencil(const PrepJob& job, const PrepInput& in,
                                const std::string& images,
                                const std::string& masks,
                                const std::string& merge_from,
                                std::string& error) {
    std::error_code ec;
    enter(Stage::Masks, lmsg::stage_frame_mask.get());

    app::FrameStencilRun run;
    run.image_dir = images;
    run.mask_dir = masks;
    run.skip_dir = masks;
    run.stencil = in.stencil;
    run.merge_dir = merge_from;
    run.flip_merge = job.flip_found_masks && !merge_from.empty();
    // Reading and writing one folder cannot be repeated under a flip: the
    // marker records that it has been done, and travels with the files.
    const bool in_place =
        !merge_from.empty() &&
        fs::weakly_canonical(masks, ec) == fs::weakly_canonical(merge_from, ec);
    const fs::path marker = fs::path(masks) / kFlippedMarker;
    if (in_place) {
        if (run.flip_merge && fs::exists(marker, ec)) run.flip_merge = false;
        log(fmt(lmsg::masks_combined_in_place, {masks}), /*detail=*/false);
    }

    RateLimitedProgress progress(_prog, Stage::Masks, lmsg::noun_images_masked,
                                 _masks_tally);
    app::FrameStencilSinks sinks;
    sinks.cancel = &_cancel;
    sinks.progress = [&](int64_t done, int64_t total) {
        progress.update(done, done == total);
    };
    sinks.resolved = [&](const std::string& camera, const app::FrameMask&,
                         const app::BorderDetect& d) {
        if (!in.stencil.detect_border) return;
        const std::string name = camera.empty() ? in.path : camera;
        if (!d.found) {
            log(fmt(lmsg::frame_mask_no_border, {name}), /*detail=*/false);
            return;
        }
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f", 100.0f * d.kept_fraction);
        log(fmt(lmsg::frame_mask_border, {name, pct}), /*detail=*/false);
    };

    if (app::apply_frame_stencil(run, sinks, error) < 0) return false;
    if (_cancel.load()) {
        error = lmsg::err_cancelled.get();
        return false;
    }
    if (in_place && job.flip_found_masks) std::ofstream(marker.string());
    return true;
}

// The Python fallback: the embedded reference/scripts/mask.py run through an
// external interpreter with lang-segment-anything. It prints an install hint
// and exits 0 when the packages are missing, so that is detected from its
// output rather than its exit code.
bool DatasetPrep::generate_masks_python(const PrepJob& job,
                                        const std::string& images_rel,
                                        const std::string& masks_rel,
                                        std::string& error) {
    enter(Stage::Masks, lmsg::stage_masks_python.get());
    if (!command_exists(job.python_exe)) {
        error = fmt(lmsg::err_python_missing, {job.python_exe});
        return false;
    }
    const fs::path ws = job.workspace;
    const fs::path script = ws / ".spirula_mask.py";
    {
        FILE* f = std::fopen(script.string().c_str(), "wb");
        if (!f) {
            error = fmt(lmsg::err_cannot_write, {script.string()});
            return false;
        }
        std::fwrite(kMaskPy, 1, kMaskPySize, f);
        std::fclose(f);
    }
    std::vector<std::string> argv = {
        job.python_exe, script.string(), ws.string(),
        "--prompt", job.mask_prompt,
        "--images", images_rel,
        "--masks", masks_rel,
        "--max_image_size", std::to_string(job.mask_max_image_size),
        "--model", job.mask_model_name,
    };
    if (!job.mask_negative_prompt.empty()) {
        argv.push_back("--negative_prompt");
        argv.push_back(job.mask_negative_prompt);
    }
    // Without this the external path silently ignores the polarity and always
    // removes what the prompt named -- the exact opposite of what an object
    // capture asked for.
    if (job.mask_keep_subject) argv.push_back("--keep_prompted");
    std::string install_hint;
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);
    const int rc = run_process(argv, "", [&](const std::string& l) {
        log(l);
        if (l.find("not found or not installed properly") != std::string::npos ||
            l.find("ModuleNotFoundError") != std::string::npos)
            install_hint = l;
    }, _cancel);
    if (rc == kCancelled) { error = lmsg::err_cancelled.get(); return false; }

    std::error_code ec;
    const bool have_masks = fs::is_directory(ws / masks_rel, ec) &&
                            !fs::is_empty(ws / masks_rel, ec);
    if (!install_hint.empty() || rc != 0 || !have_masks) {
        // Three whole sentences rather than one with tails appended: the
        // install advice is the message when there is any, and which advice
        // depends on the model.
        if (install_hint.empty())
            error = lmsg::err_mask_generation_failed.get();
        else if (job.mask_model_name == "sam3")
            error = lmsg::err_mask_missing_packages_sam3.get();
        else
            error = lmsg::err_mask_missing_packages.get();
        return false;
    }
    return true;
}

}  // namespace gui
