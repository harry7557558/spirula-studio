// DatasetPrep.cpp -- see DatasetPrep.h.

#include "app/gui/DatasetPrep.h"

#include "i18n/catalog/Log.h"

#include "app/gui/FrameSelect.h"
#include "app/gui/Subprocess.h"

#include "app_generated/mask_py.h"   // kMaskPy[], from reference/scripts/mask.py

#include "core/ExrImage.h"
#include "external/stb_image.h"      // stbi_info (image size probe)

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
#include <thread>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

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
    ".mts", ".m2ts", ".360", ".ts", ".wmv",
};

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

bool is_image_file(const fs::path& p) {
    std::string e = p.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".webp" ||
           e == ".tif" || e == ".tiff" || e == ".bmp" || e == ".exr";
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
// Preview clicks -> the seeds the masker takes. Clicks of one object made on
// one frame become ONE prompt (several positive points describe one thing);
// clicks of the same object on another frame become a second prompt, which the
// masker applies as a correction when it gets there.
//
// `exact` says the click's own frame number means the same thing to the run
// that is about to happen; otherwise only the fraction through the capture
// survives and the frame is looked up in `ids` -- see MaskClick.
std::vector<sam::SeedPrompt> seeds_from_clicks(const std::vector<MaskClick>& clicks,
                                               const std::vector<int64_t>& ids,
                                               bool exact) {
    std::vector<sam::SeedPrompt> seeds;
    for (const MaskClick& c : clicks) {
        int64_t frame = c.frame;
        if (!exact && !ids.empty()) {
            const double at = std::min(1.0, std::max(0.0, (double)c.position)) *
                              (double)(ids.size() - 1);
            frame = ids[(size_t)std::llround(at)];
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

    // The next file in the order given; empty when it could not be read or the
    // reader stopped early. What the reader threw is rethrown here instead,
    // this being the thread with somewhere to report it.
    nn::Image take() {
        std::unique_lock<std::mutex> lk(_mu);
        _ready.wait(lk, [this] { return !_queue.empty() || _done; });
        if (_queue.empty()) {
            if (_err) std::rethrow_exception(_err);
            return nn::Image();
        }
        nn::Image img = std::move(_queue.front());
        _queue.pop_front();
        _space.notify_one();
        return img;
    }

private:
    // Two in hand plus the one being masked: a 4K frame is ~25 MB, and the
    // depth buys nothing beyond covering one decode.
    static constexpr size_t kDepth = 2;

    void run() {
        try {
            for (const fs::path& f : _files) {
                if (_cancel.load()) break;
                nn::Image img = nn::load_image(f.string(), _gamut, _is_linear);
                std::unique_lock<std::mutex> lk(_mu);
                _space.wait(lk, [this] { return _queue.size() < kDepth || _stop; });
                if (_stop) break;
                _queue.push_back(std::move(img));
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
    std::deque<nn::Image> _queue;
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

// One frame is written every this many source frames, from the run's kept
// frame rate and what the container says it holds.
int frame_skip(const PrepJob& job, double src_fps) {
    return std::max(1, (int)std::lround(src_fps / std::max(job.video_fps, 0.01f)));
}

// What a video is expected to yield, for the step's bar. The extraction loop
// stops on the real end of stream either way.
int64_t expected_frames(const PrepJob& job, double src_fps, int64_t src_frames,
                        int tracks) {
    int64_t expect = src_frames > 0
                         ? (src_frames / frame_skip(job, src_fps)) * (int64_t)tracks
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
                    const size_t f = line.find(" fps");
                    if (f == std::string::npos ||
                        line.find("Video:") == std::string::npos)
                        return;
                    // The frame size off the same line. The 16-pixel floor is
                    // what rejects the fourcc ("0x31637661"), which is also
                    // digits on both sides of an x.
                    for (size_t i = 1; i + 1 < line.size() && !out.width; i++) {
                        if (line[i] != 'x') continue;
                        size_t b = i, e = i + 1;
                        while (b > 0 && std::isdigit((unsigned char)line[b - 1])) b--;
                        while (e < line.size() && std::isdigit((unsigned char)line[e])) e++;
                        if (b == i || e == i + 1) continue;
                        const int w = std::atoi(line.c_str() + b);
                        const int h = std::atoi(line.c_str() + i + 1);
                        if (w >= 16 && h >= 16) { out.width = w; out.height = h; }
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

bool ffmpeg_extract_frame(const std::string& ffmpeg_exe, const std::string& video,
                          double seconds, const std::string& out_path,
                          const std::atomic<bool>& cancel) {
    if (!command_exists(ffmpeg_exe)) return false;
    std::error_code ec;
    fs::remove(out_path, ec);
    char ts[32];
    std::snprintf(ts, sizeof ts, "%.3f", seconds > 0.0 ? seconds : 0.0);
    // -ss before -i: seek first, decode one frame, stop. The other order
    // decodes the whole file up to that point, which on a ten-minute capture
    // is the difference between a preview and a coffee break.
    const int rc = run_process({ffmpeg_exe, "-nostdin", "-y", "-ss", ts, "-i",
                                video, "-frames:v", "1", "-q:v", "2", out_path},
                               "", [](const std::string&) {}, cancel);
    if (rc != 0) {
        fs::remove(out_path, ec);
        return false;
    }
    return fs::exists(out_path, ec) && fs::file_size(out_path, ec) > 0;
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

std::vector<std::string> camera_subfolders(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    // One level only, and any_image stops at the first hit, so this stays cheap
    // enough for the draw that asks it.
    for (fs::directory_iterator it(dir, kWalk, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        if (is_mask_folder(it->path().string())) continue;
        if (any_image(it->path())) out.push_back(it->path().filename().string());
    }
    std::sort(out.begin(), out.end());
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

bool folder_looks_like_dataset(const std::string& dir) {
    std::error_code ec;
    const fs::path p(dir);
    if (fs::exists(p / "transforms.json", ec) ||
        fs::is_directory(p / "sparse", ec) || fs::is_directory(p / "colmap", ec))
        return true;
    return metashape_export_here(p);
}

WorkspaceState probe_workspace(const std::string& workspace,
                               const std::vector<PrepInput>& inputs) {
    WorkspaceState st;
    std::error_code ec;
    const fs::path ws(workspace);
    if (workspace.empty() || !fs::is_directory(ws, ec)) return st;

    // A folder is the run's leftover only if the run would write it. When
    // images/ or masks/ under the output IS an input, it is the capture.
    auto is_input = [&](const fs::path& dir, bool masks) {
        for (const PrepInput& in : inputs) {
            const std::string& p = masks ? in.mask_dir : in.path;
            if (!p.empty() && fs::equivalent(dir, p, ec)) return true;
        }
        return false;
    };
    auto has_content = [&](const fs::path& p) {
        return fs::is_directory(p, ec) && !fs::is_empty(p, ec);
    };

    st.frames = has_content(ws / "images") && !is_input(ws / "images", false);
    st.masks = has_content(ws / "masks") && !is_input(ws / "masks", true);
    st.features = has_content(ws / "features") || fs::exists(ws / "matches.bin", ec) ||
                  fs::exists(ws / "database.db", ec);
    // Stricter than folder_looks_like_dataset, which answers "where should a
    // dropped folder go" and takes an empty sparse/ for a model. An empty one
    // is a directory somebody made, and reconstructing into it is right.
    st.model = has_content(ws / "sparse") || has_content(ws / "colmap") ||
               fs::exists(ws / "transforms.json", ec) ||
               metashape_export_here(ws);
    st.geometry = has_content(ws / "normals") || has_content(ws / "depths");
    return st;
}

bool is_mask_folder(const std::string& path) {
    return named(fs::path(path), "masks");
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
                              const std::string& workspace) {
    std::error_code ec;
    if (reads_photos_in_place(inputs))
        return fs::absolute(inputs[0].path, ec).string();
    return workspace.empty() ? std::string()
                             : (fs::path(workspace) / "images").string();
}

const Backends& backends() {
    static const Backends probed = [] {
        Backends b;
#ifdef SS_HAVE_VIDEO
        b.video_reason = app::video_decode_availability();
        b.builtin_video = b.video_reason.empty();
        if (!b.builtin_video)
            b.video_note = "this graphics driver cannot decode video, so "
                           "frames are extracted with ffmpeg";
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

int DatasetPrep::count_images(const std::string& dir, const std::string& skip) {
    return (int)walk_images(dir, skip).size();
}

// Pixel size from the header alone, no decode.
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

int DatasetPrep::exec(const std::vector<std::string>& argv) {
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);
    return run_process(argv, "", [this](const std::string& l) { log(l); },
                       _cancel);
}

int64_t DatasetPrep::estimate_frames(const PrepJob& job, const PrepInput& in,
                                     const std::string& images) {
    // What a resumed run keeps is exactly what is there already.
    if (job.resume && !job.redo_frames) {
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
#ifdef SS_HAVE_VIDEO
    if (!job.force_external_decode && backends().builtin_video) {
        std::string err;
        const int tracks = app::video_track_count(in.path, err);
        video::VideoReader r;
        if (tracks > 0 && r.open(in.path))
            return expected_frames(job, r.info().fps > 1.0 ? r.info().fps : 30.0,
                                   r.info().frame_count, tracks);
    }
#endif
    VideoFacts facts;
    if (ffmpeg_probe_video(job.ffmpeg_exe, in.path, facts, _cancel) &&
        facts.fps > 1.0)
        return expected_frames(job, facts.fps, facts.frames,
                               is_dual_fisheye_path(in.path) ? 2 : 1);
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

bool DatasetPrep::run(const PrepJob& job_in, PrepResult& out, std::string& error,
                      const RefreshFn& refresh_masks) {
    PrepJob job = job_in;
#ifdef SS_BUILD_SAM
    // Hand the GPU back on the way out, by whichever of the dozen exits is
    // taken. A SAM 3 checkpoint is about 2 GB of VRAM and the inference layer's
    // pool is process-wide and grow-only, so without this it stays resident
    // for the life of the GUI -- through the reconstruction and the training
    // run that follow, which are exactly what wants the memory back.
    //
    // Safe because the mask preview owns the only other Session, and the
    // dataset screen closes it before starting a job.
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

    // Where each input's images and masks ended up, so the masking pass below
    // can run per input (see generate_masks) instead of over one flat tree.
    struct Prepared {
        std::string images, masks;      // absolute
        std::string images_rel, masks_rel;  // relative to the workspace
        // This input's masks already exist: brought along by the input, or
        // kept by a resumed run.
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

    if (reads_photos_in_place(job.inputs)) {
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
        for (size_t i = 0; i < job.inputs.size(); i++) {
            const PrepInput& in = job.inputs[i];
            Prepared& p = per[i];
            if (in.is_video) {
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
            // Masks an input brought with it are in the dataset now, so they
            // count even when nothing asked for masking.
            if (p.have_masks && !in.mask_dir.empty()) want_masks = true;
        }
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
            // Already masked by whoever made the masks this input arrived
            // with. Segmenting over those would replace an answer the user
            // already has.
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
    return true;
}

// ---------------------------------------------------------------------------
// Video -> frames
// ---------------------------------------------------------------------------

bool DatasetPrep::extract_video(const PrepJob& job, const PrepInput& in,
                                const std::string& images,
                                const std::string& masks, PrepResult& out,
                                bool& masked, std::string& error) {
    // Resume: frames are moved into place in one batch after selection, so a
    // non-empty folder means a previous extraction of THIS input finished.
    if (job.redo_frames) clear_generated(images, job.workspace);
    if (job.resume && !job.redo_frames) {
        const int have = count_images(images);
        if (have > 0) {
            log(fmt(lmsg::resume_keep_frames, {(long long)have, images}),
                /*detail=*/false);
            std::error_code ec;
            if (fs::is_directory(fs::path(images) / "cam1", ec))
                out.per_folder_cameras = true;
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

    const bool want_builtin = !job.force_external_decode && backends().builtin_video;
    if (want_builtin) {
        if (extract_video_builtin(job, in, images, out, error)) return true;
        if (_cancel.load()) return false;
        // A container or profile the driver cannot decode is exactly what the
        // fallback is for, and the user should not have to know which is which.
        log(fmt(lmsg::decode_fallback_ffmpeg, {error}), /*detail=*/false);
    }
    return extract_video_ffmpeg(job, in, images, out, error);
}

// Extraction writes frames and nothing else. Masking used to ride along on the
// decode, which was cheaper by one JPEG decode per frame and cost the user the
// ability to see the frames before deciding what to mask, to re-mask without
// re-extracting, and to have photos and video take the same path. The decode it
// saved is hidden behind the model anyway (generate_masks_builtin reads ahead).
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
    const int tracks = app::video_track_count(in.path, probe_err);
    if (tracks <= 0) {
        error = probe_err.empty() ? "no video track" : probe_err;
        return false;
    }
    if (tracks > 1) out.per_folder_cameras = true;

    double src_fps = 30.0;
    {
        video::VideoReader r;
        if (r.open(in.path) && r.info().fps > 1.0) src_fps = r.info().fps;
    }
    const int window = std::max(job.sharp_window, 1);
    const int skip = frame_skip(job, src_fps);

    app::FrameExtractJob fx;
    fx.input = in.path;
    fx.image_dir = images;
    fx.skip = skip;
    fx.keep = window > 1 ? window : 0;
    fx.max_frames = job.max_frames;
    fx.quality = 95;

    app::FrameExtractSinks sinks;
    sinks.log = [this](const std::string& l) { log(l); };
    sinks.cancel = &_cancel;
    RateLimitedProgress progress(_prog, Stage::Frames, lmsg::noun_frames_written,
                                 _frames_tally);
    sinks.progress = [&](int64_t written, int64_t decoded) {
        (void)decoded;
        progress.update(written);
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
    const fs::path ws = job.workspace;
    if (!command_exists(job.ffmpeg_exe)) {
        error = fmt(lmsg::err_ffmpeg_missing, {job.ffmpeg_exe});
        return false;
    }
    log(fmt(lmsg::video_input, {in.path}), /*detail=*/false);

    // Multi-track videos (Insta360 .insv): one folder per track, one camera
    // per folder, as in reference/scripts/extract_frames.py.
    std::vector<int> streams = {0};
    if (is_dual_fisheye_path(in.path)) {
        std::vector<int> found;
        run_process({"ffprobe", "-v", "error", "-select_streams", "v",
                     "-show_entries", "stream=index", "-of", "csv=p=0",
                     in.path}, "",
                    [&](const std::string& l) {
                        try { found.push_back(std::stoi(l)); } catch (...) {}
                    }, _cancel);
        if (found.size() > 1) streams = found;
    }
    if (streams.size() > 1) out.per_folder_cameras = true;

    const int window = std::max(job.sharp_window, 1);
    for (size_t tr = 0; tr < streams.size(); tr++) {
        std::string track_path = in.path;
        const fs::path out_dir = streams.size() > 1
            ? fs::path(images) / ("cam" + std::to_string(tr))
            : fs::path(images);
        if (job.resume && !job.redo_frames &&
            count_images(out_dir.string()) > 0) {
            log(fmt(lmsg::resume_keep_frames_dir, {out_dir.string()}),
                /*detail=*/false);
            continue;
        }
        if (streams.size() > 1) {
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
        const fs::path cand = ws / "frames_tmp";
        remove_tree(cand);
        std::error_code ec;
        fs::create_directories(cand, ec);
        char vf[64];
        std::snprintf(vf, sizeof vf, "fps=%g", (double)job.video_fps * window);
        int rc = exec({job.ffmpeg_exe, "-nostdin", "-y", "-i", track_path,
                       "-vf", vf, "-qscale:v", "2",
                       (cand / "c_%06d.jpg").string()});
        if (rc == kCancelled) { error = lmsg::err_cancelled.get(); return false; }
        if (rc != 0) {
            error = lmsg::err_ffmpeg_extract_failed.get();
            return false;
        }

        if (window > 1) enter(Stage::Frames, lmsg::stage_select_sharpest.get());
        fs::create_directories(out_dir, ec);
        const int kept = select_sharpest_frames(
            cand.string(), out_dir.string(), "", window, job.max_frames,
            [this](const std::string& l) { log(l); }, _cancel);
        remove_tree(cand);
        if (streams.size() > 1) fs::remove(track_path, ec);
        if (kept < 0) {
            error = _cancel.load() ? "cancelled" : "frame selection failed";
            return false;
        }
        log(fmt(lmsg::kept_frames, {(long long)kept, out_dir.string()}),
            /*detail=*/false);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Photos -> the dataset's own images/
// ---------------------------------------------------------------------------

// Only when the photos cannot be read where they are: a job with more than one
// input reconstructs from ONE image tree, and the folders under it are what
// make its inputs separate cameras.
//
// Hard links where the filesystem gives one, a copy otherwise. A link costs a
// directory entry, which matters when the alternative is a second copy of a
// folder of raw captures; falling back is what makes it work across devices
// (and on a filesystem that has no links at all).
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
        const spirula::i18n::Msg* moving;   // "<kind>: <from> -> <to>"
        const spirula::i18n::Msg* empty;    // "there are no <kind> in <from>"
        const spirula::i18n::Msg* counted;  // what the progress line counts
    };
    std::vector<Tree> trees{{src, fs::path(images), &lmsg::copying_photos,
                             &lmsg::err_no_photos_in,
                             &lmsg::noun_photos_collected}};
    if (with_masks)
        trees.push_back({fs::absolute(in.mask_dir, ec), fs::path(masks),
                         &lmsg::copying_masks, &lmsg::err_no_masks_in,
                         &lmsg::noun_masks_collected});

    for (const Tree& t : trees) {
        log(fmt(*t.moving, {t.from.string(), t.to.string()}), /*detail=*/false);
        const std::vector<fs::path> files = walk_images(t.from);
        if (files.empty()) {
            error = fmt(*t.empty, {t.from.string()});
            return false;
        }
        int linked = 0, copied = 0, kept = 0;
        RateLimitedProgress progress(_prog, Stage::Frames, *t.counted,
                                     _frames_tally);
        for (const fs::path& f : files) {
            if (_cancel.load()) { error = lmsg::err_cancelled.get(); return false; }
            fs::path rel = f.lexically_relative(t.from);
            if (rel.empty() || *rel.begin() == "..") rel = f.filename();
            const fs::path out_path = t.to / rel;
            if (fs::exists(out_path, ec)) { kept++; continue; }
            fs::create_directories(out_path.parent_path(), ec);
            std::error_code link_ec;
            fs::create_hard_link(f, out_path, link_ec);
            if (!link_ec) {
                linked++;
            } else {
                std::error_code copy_ec;
                fs::copy_file(f, out_path, copy_ec);
                if (copy_ec) {
                    error = fmt(lmsg::err_copy_failed,
                                {f.string(), t.to.string(), copy_ec.message()});
                    return false;
                }
                copied++;
            }
            progress.update(linked + copied + kept);
        }
        log(fmt(lmsg::linked_copied_kept,
                 {(long long)linked, (long long)copied, (long long)kept}));
    }
    if (with_masks) have_masks = true;
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
    const bool by_stem = in.is_video && !job.force_external_decode &&
                         backends().builtin_video &&
                         frame_ids_from_stems(files, ids);
    if (!by_stem) {
        ids.resize(files.size());
        for (size_t i = 0; i < ids.size(); i++) ids[i] = (int64_t)i;
    }

    sam::MaskOptions mo;
    mo.model = job.mask_model_path;
    mo.text = job.mask_prompt;
    mo.neg_text = job.mask_negative_prompt;
    mo.keep_prompted = job.mask_keep_subject;
    mo.max_size = job.mask_max_image_size;
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
    // recorded against did; ffmpeg resampled the video, so there only the
    // fraction through the capture is meaningful.
    mo.seeds = seeds_from_clicks(clicks, ids, /*exact=*/!in.is_video || by_stem);

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
        nn::Image img = reader.take();
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
        if (!stencil.apply(todo_files[k], image_root, mask, error)) return false;
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
