// VideoBrowser.cpp -- see VideoBrowser.h.

#include "app/gui/VideoBrowser.h"

#include "app/FfmpegVideo.h"
#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/SourcePreview.h"

#ifdef SS_HAVE_VIDEO
#include "app/FrameExtract.h"
#include "video/Video.h"
#endif

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numeric>

namespace dmsg = spirula::i18n::msg::dataset;
namespace pmsg = spirula::i18n::msg::preview;

namespace gui {

namespace {

// The long edge the exact frame is decoded at, and a thumbnail's. Playing and
// scrubbing ask for what the pane shows instead, within these bounds.
constexpr int kFrameSide = 2048;
constexpr int kThumbSide = 320;
constexpr int kLiveMin = 256, kLiveMax = 1280;
// Frames decoded ahead of a playing playhead.
constexpr size_t kReadAhead = 3;
// A decoder left idle this long gives its memory on the device back.
constexpr double kIdleClose = 10.0;

// The strip fills from coarse to fine, so a slow decoder still shows the
// whole length of the video early.
constexpr int kThumbOrder[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

// "00:01:23.4": the hour is always there, as the duration readouts elsewhere
// in the interface have it.
std::string clock_text(double s, bool tenths = true) {
    if (!(s >= 0.0)) s = 0.0;
    const long long ds = (long long)std::floor(s * 10.0 + 0.5);
    char b[48];
    if (tenths)
        std::snprintf(b, sizeof b, "%02lld:%02lld:%02lld.%lld", ds / 36000, (ds / 600) % 60,
                      (ds / 10) % 60, ds % 10);
    else
        std::snprintf(b, sizeof b, "%02lld:%02lld:%02lld", ds / 36000, (ds / 600) % 60,
                      (ds / 10) % 60);
    return b;
}

// Seconds, or [[h:]m:]s with a fraction: what a person types into a time box.
bool parse_clock(const std::string& text, double& out) {
    double total = 0.0;
    int parts = 0;
    const char* p = text.c_str();
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char* end = nullptr;
        const double v = std::strtod(p, &end);
        if (end == p || !(v >= 0.0)) return false;
        total = total * 60.0 + v;
        parts++;
        p = end;
        while (*p == ' ') p++;
        if (*p == ':') p++;
        else if (*p) return false;
    }
    if (parts == 0 || parts > 3) return false;
    out = total;
    return true;
}

double now_s() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string count_text(long long v) {
    std::string out;
    ui::detail::group_digits(std::to_string(v).c_str(), 0, out);
    return out;
}

void probe_video_summary(const PreviewSource& src, VideoSummary& f,
                         const std::atomic<bool>& cancel) {
#ifdef SS_HAVE_VIDEO
    {
        video::VideoProbe p;
        std::string err;
        if (video::probe_video(src.input, p, err) && p.tracks > 0 && p.fps > 0.0) {
            f.fps = p.fps;
            f.frames = p.frame_count;
            f.seconds = (double)p.frame_count / p.fps;
            f.codec = p.codec;
            f.sizes = app::video_track_sizes(src.input, err);
            if (f.sizes.empty())
                f.sizes.assign((size_t)p.tracks, std::make_pair(p.width, p.height));
            f.done = true;
            return;
        }
    }
#endif
    app::VideoFacts vf;
    if (ffmpeg_probe_video(src.ffmpeg_exe, src.input, vf, cancel) && vf.fps > 0.0) {
        f.fps = vf.fps;
        f.frames = vf.frames;
        f.seconds = vf.duration;
        f.sizes = vf.tracks;
    } else {
        f.error = spirula::i18n::format(pmsg::video_unreadable, {src.ffmpeg_exe});
    }
    f.done = true;
}

}  // namespace


// The decoder is touched only under the gate's lock; `used_at` and `held`
// are the worker's own, so it can tell when to give the memory back.
struct VideoBrowser::Gpu {
#ifdef SS_HAVE_VIDEO
    app::FrameCursor cursor;
#endif
    double used_at = 0.0;
    bool held = false;
    void close() {
#ifdef SS_HAVE_VIDEO
        cursor.close();
#endif
    }
};

VideoBrowser::VideoBrowser() : _gpu(std::make_unique<Gpu>()) {}

VideoBrowser::~VideoBrowser() {
    stop();
    std::lock_guard<std::mutex> hold(_gate->busy);
    _gpu->close();
}

void VideoBrowser::start() {
    if (_worker.joinable() || _key.empty()) return;
    _quit = false;
    _worker = std::thread([this] { work(); });
}

void VideoBrowser::stop() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _quit = true;
    }
    _cancel = true;
    _cv.notify_all();
    if (_worker.joinable()) _worker.join();
    _cancel = false;
}

void VideoBrowser::abort(bool on) {
    _abort = on;
    if (on) {
        _cancel = true;
        _cv.notify_all();
    }
}

void VideoBrowser::drop_decoder() { _gpu->close(); }

void VideoBrowser::set(const std::string& key, const PreviewSource& src,
                       const app::Pano360Layout& pano, const app::Pano360Options& orient) {
    if (key == _key) return;
    clear();
    _key = key;
    _src = src;
    _src.clip_start = _src.clip_end = 0.0;
    _pano = pano;
    _orient = orient;
    _stitched = pano.valid();
    _gpu_failed = false;
}

void VideoBrowser::clear() {
    stop();
    {
        std::lock_guard<std::mutex> hold(_gate->busy);
        _gpu->close();
    }
    std::lock_guard<std::mutex> lk(_mu);
    _key.clear();
    _generation++;
    _facts = VideoSummary{};
    _ui_facts = VideoSummary{};
    for (int i = 0; i < kThumbs; i++) {
        _thumb_pic[i].clear();
        _thumb_index[i] = 0;
        _thumb_fresh[i] = false;
        _thumb_state[i] = 0;
        for (PreviewTexture& t : _thumb_tex[i]) t.reset();
        _thumb_tex[i].clear();
    }
    _want = Want{};
    _done = Want{};
    _play = false;
    _play_next = -1;
    _ready.clear();
    _frame_error.clear();
    _error_index = -1;
    for (PreviewTexture& t : _frame_tex) t.reset();
    _frame_tex.clear();
    _frame_tex_index = -1;
    _frame_tex_side = 0;
    _playhead = 0.0;
    _playing = false;
    _drag = 0;
    _editing = 0;
}

void VideoBrowser::set_paused(bool paused) {
    if (paused) _playing = false;
    if (_paused.exchange(paused) && !paused) _cv.notify_all();
}

void VideoBrowser::destroy_gl() {
    std::lock_guard<std::mutex> lk(_mu);
    for (int i = 0; i < kThumbs; i++) {
        for (PreviewTexture& t : _thumb_tex[i]) t.reset();
        _thumb_tex[i].clear();
        if (_thumb_state[i] == 2 && _thumb_pic[i].empty()) _thumb_state[i] = 0;
    }
    for (PreviewTexture& t : _frame_tex) t.reset();
    _frame_tex.clear();
    _frame_tex_index = -1;
    _done = Want{};
}

VideoSummary VideoBrowser::facts() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _facts;
}

long long VideoBrowser::index_at(double seconds) const {
    if (!(_ui_facts.fps > 0.0)) return 0;
    const long long last = std::max(0LL, _ui_facts.frames - 1);
    return std::clamp((long long)std::floor(seconds * _ui_facts.fps + 1e-6), 0LL, last);
}

int VideoBrowser::streams(bool stitched) const {
    return stitched && _pano.valid() ? 1 : std::max(1, (int)_ui_facts.pictures.size());
}

void VideoBrowser::set_playhead(double seconds) {
    _playhead = std::clamp(seconds, 0.0, std::max(0.0, _ui_facts.seconds));
    _settle_at = 0.0;
    _jumped = true;
}

bool VideoBrowser::can_play() const {
#ifdef SS_HAVE_VIDEO
    return _src.builtin_decode && _gate->allowed.load() && !_gpu_failed.load() &&
           _ui_facts.done && _ui_facts.error.empty() && _ui_facts.frames > 1;
#else
    return false;
#endif
}

void VideoBrowser::play(bool on, double clip_start, double clip_end) {
    if (on && !can_play()) on = false;
    if (on && !_playing) {
        // To the end of the part kept when starting inside it, else the video's.
        const double dur = _ui_facts.seconds;
        const double kept_end = clip_end > 0.0 ? std::min(clip_end, dur) : dur;
        const double frame = 1.0 / std::max(_ui_facts.fps, 1.0);
        if (_playhead >= kept_end - frame || _playhead < clip_start) {
            if (_playhead >= dur - frame || _playhead < clip_start) _playhead = clip_start;
        }
        _play_end = _playhead < kept_end - frame ? kept_end : dur;
        _jumped = true;
    }
    _playing = on;
}

void VideoBrowser::tick() {
    if (_key.empty()) return;
    // Probed and its strip read whichever tab is open, so another tab can play it.
    set_paused(false);
    start();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _ui_facts = _facts;
    }
    if (!_playing) return;
    if (!can_play() || _drag != 0) {
        _playing = false;
        return;
    }
    // A hitch in the GUI skips ahead rather than slowing the clock.
    _playhead += std::min((double)ImGui::GetIO().DeltaTime, 0.25);
    if (_playhead >= _play_end) {
        _playhead = _play_end;
        _playing = false;
    }
}

VideoBrowser::Decoded VideoBrowser::decode(const PreviewSource& base, const Want& w,
                                           bool keyframe, Frame& out, std::string& error) {
    std::unique_lock<std::mutex> hold;
    PreviewSource s = gated(base, *_gate, hold);
    if (!s.device_error.empty()) {
        error = s.device_error;
        return Decoded::Failed;
    }
    const bool stitched = w.stitched && _pano.valid();
    const int n = stitched                    ? 1
                  : s.look.packed_lenses >= 2 ? s.look.packed_lenses
                                              : std::max(1, s.tracks);
    out = Frame{};
    out.stitched = stitched;
    out.side = w.side;
#ifdef SS_HAVE_VIDEO
    if (s.builtin_decode && !_gpu_failed.load()) {
        app::FrameCursor& c = _gpu->cursor;
        std::string err;
        if (!c.is_open() && !c.open(s.input, err, s.device)) {
            // This device cannot read this file: ffmpeg from here on.
            _gpu_failed = true;
            c.close();
        } else {
            _gpu->used_at = now_s();
            _gpu->held = true;
            if (!c.go(w.index, !keyframe, &_cancel, err)) {
                c.close();
                if (_cancel.load()) return Decoded::Aborted;
                error = err;
                return Decoded::Failed;
            }
            out.index = c.index();
            for (int k = 0; k < n; k++) {
                nn::Image img;
                if (!c.image(s.look, k, img, err)) {
                    error = err;
                    return Decoded::Failed;
                }
                preview_to_srgb(s, img.data);
                Picture pic;
                pic.rgb = std::move(img.data);
                pic.w = pic.src_w = img.width;
                pic.h = pic.src_h = img.height;
                pic.made_for = w.side;
                out.pics.push_back(std::move(pic));
            }
            return Decoded::Ok;
        }
    }
#endif
    if (hold.owns_lock()) hold.unlock();
    if (!_have_ffmpeg.load()) return Decoded::NeedsGpu;
    // ffmpeg seeks by time and keeps the first frame at or after it.
    const double fps = std::max(_facts.fps, 1.0);
    const double at = keyframe ? (double)w.index / fps
                               : std::max(0.0, ((double)w.index - 0.5) / fps);
    std::vector<int> folders((size_t)n);
    std::iota(folders.begin(), folders.end(), 0);
    // ffmpeg scales the frame before the lenses are cut from it.
    int side = stitched ? 0 : w.side;
    if (side > 0 && !_facts.sizes.empty() && !_facts.pictures.empty()) {
        const int frame_long = std::max(_facts.sizes[0].first, _facts.sizes[0].second);
        const int pic_long = std::max(_facts.pictures[0].first, _facts.pictures[0].second);
        if (pic_long > 0) side = (int)((long long)side * frame_long / pic_long);
    }
    std::vector<PreviewPixels> px;
    if (!load_ffmpeg_stills(s, at, folders, keyframe, side, px, _cancel)) {
        if (_cancel.load()) return Decoded::Aborted;
        error = dmsg::preview_frame_unreadable.get();
        return Decoded::Failed;
    }
    out.index = w.index;
    for (PreviewPixels& p : px) {
        Picture pic;
        make_picture(p.rgb.data(), p.w, p.h, nullptr, w.side, pic);
        out.pics.push_back(std::move(pic));
    }
    return Decoded::Ok;
}

void VideoBrowser::work() {
    PreviewSource src;
    uint64_t gen = 0;
    {
        std::lock_guard<std::mutex> lk(_mu);
        src = _src;
        gen = _generation;
    }
    VideoSummary f;
    probe_video_summary(src, f, _cancel);
    f.pictures = f.sizes;
    if (src.look.packed_lenses >= 2 && !f.sizes.empty()) {
        int lw = 0, lh = 0;
        app::packed_lens_size(f.sizes[0].first, f.sizes[0].second, src.look.packed_lenses, lw, lh);
        f.pictures.assign((size_t)src.look.packed_lenses, std::make_pair(lw, lh));
    }
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (gen != _generation || _quit) return;
        _facts = f;
    }
    if (!f.error.empty()) return;
    src.video_seconds = f.seconds;
    src.video_frames = f.frames;
    src.tracks = std::max(1, (int)f.sizes.size());
    const int src_long =
        f.pictures.empty() ? 0 : std::max(f.pictures[0].first, f.pictures[0].second);

    // What one request decodes: the panorama for a 360 file, otherwise every
    // track scaled on the GPU to about what is shown.
    auto look_for = [&](const Want& w) {
        PreviewSource s = src;
        s.look.eac = app::Pano360Layout{};
        s.look.views.clear();
        s.look.scale = 1.0f;
        if (w.stitched && _pano.valid()) {
            app::Pano360Options o = _orient;
            o.mode = app::Pano360Mode::Equirect;
            o.size = w.side;
            s.look.eac = _pano;
            s.look.views = app::pano360_views(_pano, o);
        } else if (src_long > w.side) {
            s.look.scale = (float)w.side / (float)src_long;
        }
        return s;
    };
    auto satisfied = [](const Want& done, const Want& w) {
        return done.index == w.index && done.stitched == w.stitched && done.side >= w.side;
    };

    std::unique_lock<std::mutex> lk(_mu);
    while (!_quit) {
        if (_paused.load() || _abort.load()) {
            _cv.wait_for(lk, std::chrono::milliseconds(250));
            continue;
        }
        // What to decode: the next frames of a playing video, the frame the
        // playhead rests on, or a thumbnail for the strip.
        Want w;
        bool thumb = false, playing = false;
        int slot = -1;
        if (_play && _want.index >= 0) {
            const long long next = _play_next < 0 ? _want.index : std::max(_play_next, _want.index);
            if (_ready.size() < kReadAhead && next < f.frames) {
                w = _want;
                w.index = next;
                playing = true;
            }
        } else if (_want.index >= 0 && !satisfied(_done, _want)) {
            w = _want;
        }
        if (w.index < 0 && !_play && f.frames > 0)
            for (int k : kThumbOrder)
                if (_thumb_state[k] == 0) {
                    slot = k;
                    break;
                }
        if (w.index < 0 && slot >= 0) {
            thumb = true;
            w.index = std::min(f.frames - 1, (2LL * slot + 1) * f.frames / (2 * kThumbs));
            w.side = kThumbSide;
            w.stitched = _pano.valid();
            _thumb_state[slot] = 1;
        }
        if (w.index < 0) {
            // Nothing to do: the decoder's memory goes back after a while.
            if (_gpu->held && now_s() - _gpu->used_at > kIdleClose) {
                lk.unlock();
                {
                    std::lock_guard<std::mutex> hold(_gate->busy);
                    _gpu->close();
                }
                _gpu->held = false;
                lk.lock();
                continue;
            }
            _cv.wait_for(lk, std::chrono::seconds(1));
            continue;
        }

        _decoding = true;
        _decoding_thumb = thumb;
        _cancel = false;
        lk.unlock();
        Frame got;
        std::string error;
        const Decoded how = decode(look_for(w), w, thumb, got, error);
        lk.lock();
        _decoding = false;
        _decoding_thumb = false;
        if (_quit) break;
        if (how == Decoded::Aborted || how == Decoded::NeedsGpu) {
            // Paused rather than failed: asked again once the device is back.
            if (thumb) _thumb_state[slot] = 0;
            if (how == Decoded::NeedsGpu) _cv.wait_for(lk, std::chrono::milliseconds(250));
            continue;
        }
        const bool ok = how == Decoded::Ok;
        if (thumb) {
            _thumb_state[slot] = ok ? 2 : 3;
            if (ok) {
                _thumb_pic[slot] = std::move(got.pics);
                _thumb_index[slot] = got.index;
                _thumb_fresh[slot] = true;
            }
            continue;
        }
        if (playing) {
            if (!ok || !_play) {
                if (!ok) _play_next = w.index + 1;
                continue;
            }
            _play_next = got.index + 1;
            _ready.push_back(std::move(got));
            continue;
        }
        _done = w;
        if (ok) {
            _ready.clear();
            _ready.push_back(std::move(got));
            _frame_error.clear();
        } else {
            _frame_error = error;
            _error_index = w.index;
        }
    }
}

// The decoded frames due on screen, as textures. GUI thread, under _mu.
void VideoBrowser::take_frames() {
    for (int i = 0; i < kThumbs; i++)
        if (_thumb_fresh[i]) {
            _thumb_tex[i].resize(_thumb_pic[i].size());
            for (size_t k = 0; k < _thumb_pic[i].size(); k++) _thumb_tex[i][k].upload(_thumb_pic[i][k]);
            _thumb_pic[i].clear();
            _thumb_fresh[i] = false;
        }
    if (_ready.empty()) return;
    size_t pick = _ready.size() - 1;
    if (_play) {
        // The newest frame the playhead has reached; later ones wait their turn.
        const long long cur = index_at(_playhead);
        long long best = -1;
        for (size_t i = 0; i < _ready.size(); i++)
            if (_ready[i].index <= cur) {
                pick = i;
                best = (long long)i;
            }
        if (best < 0) return;
    }
    Frame& fr = _ready[pick];
    if (_frame_tex.size() > fr.pics.size())
        for (size_t k = fr.pics.size(); k < _frame_tex.size(); k++) _frame_tex[k].reset();
    _frame_tex.resize(fr.pics.size());
    for (size_t k = 0; k < fr.pics.size(); k++) _frame_tex[k].upload(fr.pics[k]);
    _frame_tex_index = fr.index;
    _frame_tex_side = fr.side;
    _frame_tex_stitched = fr.stitched;
    _ready.erase(_ready.begin(), _ready.begin() + (std::ptrdiff_t)pick + 1);
    _cv.notify_all();
}

VideoBrowser::Result VideoBrowser::draw(float height, double& clip_start, double& clip_end,
                                        bool locked, float kept_fps, int* active_track) {
    Result r;
    if (_key.empty()) return r;
    set_paused(false);
    start();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _ui_facts = _facts;
        take_frames();
    }
    const VideoSummary& f = _ui_facts;
    if (!f.done) {
        ui::TextDisabled(pmsg::reading_video);
        return r;
    }
    if (!f.error.empty()) {
        ui::TextColoredWrappedRaw(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), f.error);
        return r;
    }

    // Which picture of a 360 file, and the decoder's state when it is waiting.
    if (_pano.valid()) {
        if (ui::RadioButton(pmsg::view_stitched, _stitched)) _stitched = true;
        ui::help_on_hover(pmsg::view_stitched_help);
        ImGui::SameLine();
        if (ui::RadioButton(pmsg::view_recorded, !_stitched)) _stitched = false;
        ui::help_on_hover(pmsg::view_recorded_help);
    } else if (f.sizes.size() > 1) {
        ui::TextDisabled(pmsg::streams_side_by_side, {(long long)f.sizes.size()});
        if (active_track && f.sizes.size() == 2) {
            ImGui::SameLine();
            ui::TextDisabledRaw("·");
            ImGui::SameLine();
            bool cam0 = (*active_track < 0 || *active_track == 0);
            ImGui::BeginDisabled(locked || (cam0 && *active_track == 0));
            if (ui::Checkbox(pmsg::camera_1, &cam0)) {
                *active_track = cam0 ? -1 : 1;
                r.tracks_changed = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover(pmsg::camera_toggle_help);
            ImGui::SameLine();
            bool cam1 = (*active_track < 0 || *active_track == 1);
            ImGui::BeginDisabled(locked || (cam1 && *active_track == 1));
            if (ui::Checkbox(pmsg::camera_2, &cam1)) {
                *active_track = cam1 ? -1 : 0;
                r.tracks_changed = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover(pmsg::camera_toggle_help);
        }
    } else if (f.pictures.size() > 1) {
        ui::TextDisabled(pmsg::lenses_side_by_side, {(long long)f.pictures.size()});
    }
    if (!_gate->allowed.load() && !_have_ffmpeg.load() && _src.builtin_decode) {
        ui::TextColoredWrapped(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), pmsg::video_needs_gpu);
        if (ui::Button(pmsg::decode_on_gpu)) r.want_gpu = true;
        ui::help_on_hover(pmsg::decode_on_gpu_help);
    }

    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float strip_h = px(56.0f) + line;
    const float controls_h = ImGui::GetFrameHeightWithSpacing() * 2.0f + line;
    const float frame_h = std::max(px(80.0f), height - strip_h - controls_h -
                                                  (ImGui::GetCursorPosY() - ImGui::GetCursorStartPos().y));
    const ImVec2 box(ImGui::GetContentRegionAvail().x, frame_h);

    // Keys while the pointer is over the video or it has the focus: play, a
    // frame, a second, and the in and out points as an editor's I and O.
    const bool hovered = (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) ||
                          ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) &&
                         !ImGui::GetIO().WantTextInput;
    if (hovered && f.fps > 0.0) {
        // Shortcut() claims the key: imgui's nav would also walk the focus on it.
        const ImGuiInputFlags route = ImGuiInputFlags_RouteFocused |
                                      ImGuiInputFlags_RouteFromRootWindow;
        const ImGuiInputFlags rep = route | ImGuiInputFlags_Repeat;
        const double frame = 1.0 / f.fps;
        auto step = [&](double by) {
            _playing = false;
            set_playhead(_playhead + by);
        };
        if (ImGui::Shortcut(ImGuiKey_Space, route) && can_play())
            play(!_playing, clip_start, clip_end);
        if (ImGui::Shortcut(ImGuiKey_LeftArrow, rep)) step(-frame);
        if (ImGui::Shortcut(ImGuiKey_RightArrow, rep)) step(frame);
        if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_LeftArrow, rep)) step(-1.0);
        if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_RightArrow, rep)) step(1.0);
        if (!locked && ImGui::Shortcut(ImGuiKey_I, route)) {
            clip_start = _playhead;
            if (clip_end > 0.0 && clip_end <= clip_start) clip_end = 0.0;
            r.clip_changed = true;
        }
        if (!locked && ImGui::Shortcut(ImGuiKey_O, route)) {
            clip_end = _playhead > clip_start ? _playhead : 0.0;
            r.clip_changed = true;
        }
    }

    draw_frame(box, active_track);
    if (draw_timeline(box.x, clip_start, clip_end, locked)) r.clip_changed = true;

    // What to decode next. On the GPU every move of the playhead is a frame
    // at about the pane's size, and the full size once it rests; ffmpeg takes
    // over a second a frame, so it is asked only once the playhead rests.
    const bool gpu = can_play();
    const bool live = _playing || _drag != 0;
    const int n_streams = streams(_stitched);
    const float fb = ImGui::GetIO().DisplayFramebufferScale.x;
    const float slot_w = (box.x / (float)n_streams) * std::max(fb, 1.0f);
    const float slot_h = box.y * std::max(fb, 1.0f);
    const int shown = (int)std::ceil(std::max(slot_w, _stitched && _pano.valid()
                                                          ? std::min(slot_w, slot_h * 2.0f)
                                                          : std::min(slot_w, slot_h)) /
                                     128.0f) *
                      128;
    Want want;
    want.index = index_at(_playhead);
    want.stitched = _stitched && _pano.valid();
    want.side = live && gpu ? std::clamp(shown, kLiveMin, kLiveMax) : kFrameSide;
    if (gpu || !live || ImGui::GetTime() - _settle_at > 0.2) {
        std::lock_guard<std::mutex> lk(_mu);
        // Frames read ahead are only good for the stretch they were read on.
        if ((_jumped && (_play || _playing)) || _play != _playing) {
            _ready.clear();
            _play_next = -1;
        }
        _jumped = false;
        _play = _playing;
        const bool changed = _want.index != want.index || _want.side != want.side ||
                             _want.stitched != want.stitched;
        if (changed) {
            // ffmpeg's still for a frame no longer looked at is not worth finishing.
            if (_decoding && !gpu && !_play) _cancel = true;
            _want = want;
            _cv.notify_all();
        } else if (_play) {
            _cv.notify_all();
        }
    }

    // Play, then the clip as buttons, as two boxes to type into, and as what it keeps.
    ImGui::BeginDisabled(!gpu);
    if (ui::Button(_playing ? pmsg::pause : pmsg::play)) play(!_playing, clip_start, clip_end);
    ImGui::EndDisabled();
    if (gpu) ui::help_on_hover(pmsg::play_help);
    else if (!_src.builtin_decode) ui::help_on_hover_disabled(pmsg::play_needs_gpu);
    else if (_gpu_failed.load()) ui::help_on_hover_disabled(pmsg::play_gpu_cannot);
    else ui::help_on_hover_disabled(pmsg::play_gpu_busy);
    ImGui::SameLine();
    ImGui::BeginDisabled(locked);
    if (ui::Button(pmsg::clip_start_here)) {
        clip_start = _playhead;
        if (clip_end > 0.0 && clip_end <= clip_start) clip_end = 0.0;
        r.clip_changed = true;
    }
    ui::help_on_hover(pmsg::clip_start_here_help);
    ImGui::SameLine();
    if (ui::Button(pmsg::clip_end_here)) {
        clip_end = _playhead > clip_start ? _playhead : 0.0;
        r.clip_changed = true;
    }
    ui::help_on_hover(pmsg::clip_end_here_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(clip_start <= 0.0 && clip_end <= 0.0);
    if (ui::Button(pmsg::clip_whole)) {
        clip_start = clip_end = 0.0;
        r.clip_changed = true;
    }
    ImGui::EndDisabled();
    ui::help_on_hover(pmsg::clip_whole_help);

    const double end_shown = clip_end > 0.0 ? clip_end : f.seconds;
    auto time_box = [&](const char* id, int which, double value, double& target) {
        std::string& text = which == 1 ? _start_text : _end_text;
        if (_editing != which) text = clock_text(value);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("00:00:00.00").x +
                                ImGui::GetStyle().FramePadding.x * 2.0f);
        ui::InputTextRaw(id, &text, ImGuiInputTextFlags_AutoSelectAll);
        if (ImGui::IsItemActive()) _editing = which;
        else if (_editing == which) _editing = 0;
        double v = 0.0;
        if (ImGui::IsItemDeactivatedAfterEdit() && parse_clock(text, v)) {
            target = std::clamp(v, 0.0, f.seconds);
            r.clip_changed = true;
        }
    };
    ui::Text(pmsg::clip_from);
    ImGui::SameLine();
    double new_start = clip_start, new_end = end_shown;
    time_box("##clipfrom", 1, clip_start, new_start);
    ImGui::SameLine();
    ui::Text(pmsg::clip_to);
    ImGui::SameLine();
    time_box("##clipto", 2, end_shown, new_end);
    ImGui::EndDisabled();
    if (r.clip_changed && (new_start != clip_start || new_end != end_shown)) {
        clip_start = new_start >= f.seconds ? 0.0 : new_start;
        clip_end = new_end >= f.seconds - 0.5 / std::max(f.fps, 1.0) ? 0.0 : new_end;
        if (clip_end > 0.0 && clip_end <= clip_start) clip_end = 0.0;
    }
    ImGui::SameLine();
    const double kept = std::max(0.0, end_shown - clip_start);
    ui::TextDisabled(pmsg::clip_length, {clock_text(kept), clock_text(f.seconds)});
    if (locked) ui::help_on_hover(pmsg::clip_locked);

    const long long frames = kept_fps > 0.0f ? (long long)std::llround(kept * kept_fps)
                                             : (long long)std::llround(kept * f.fps);
    const bool single_active = active_track && *active_track >= 0;
    if (f.sizes.size() > 1 && !_pano.valid() && !single_active)
        ui::TextDisabled(pmsg::clip_frames_per_camera, {count_text(frames)});
    else if (f.pictures.size() > 1 && !_pano.valid() && !single_active)
        ui::TextDisabled(pmsg::clip_frames_per_lens, {count_text(frames)});
    else
        ui::TextDisabled(pmsg::clip_frames, {count_text(frames)});
    ui::help_on_hover(pmsg::clip_frames_help);
    return r;
}

void VideoBrowser::draw_frame(const ImVec2& box, int* active_track, bool locked, Result* r) {
    (void)locked; (void)r;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ui::InvisibleButtonRaw("##videoframe", ImVec2(std::max(box.x, 1.0f), std::max(box.y, 1.0f)));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(at, ImVec2(at.x + box.x, at.y + box.y), IM_COL32(12, 12, 14, 255));

    const long long want = index_at(_playhead);
    const bool stitched = _stitched && _pano.valid();
    const int n = streams(_stitched);
    const bool same_view = _frame_tex_stitched == stitched && (int)_frame_tex.size() == n;
    const bool exact = same_view && _frame_tex_index == want;
    bool busy = false;
    std::string error;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_error_index == want) error = _frame_error;
        busy = (_decoding && !_decoding_thumb) || (_want.index >= 0 && _done.index != _want.index);
    }
    // Waiting for a decoder is not decoding; the line above the frame says why.
    if (!_gate->allowed.load() && !_have_ffmpeg.load() && _src.builtin_decode) busy = false;

    // The pictures to draw: the frame in hand, or, where ffmpeg is slow to
    // bring the exact one, the strip's nearest picture when that is nearer.
    std::vector<const PreviewTexture*> tex((size_t)n, nullptr);
    if (same_view)
        for (int k = 0; k < n; k++) tex[(size_t)k] = _frame_tex[(size_t)k].ready() ? &_frame_tex[(size_t)k] : nullptr;
    if (!exact && !can_play() && stitched == _pano.valid() && _ui_facts.frames > 0) {
        const int k = std::clamp((int)(_playhead / std::max(_ui_facts.seconds, 1e-9) * kThumbs), 0,
                                 kThumbs - 1);
        const std::vector<PreviewTexture>& t = _thumb_tex[k];
        const bool whole = (int)t.size() == n &&
                           std::all_of(t.begin(), t.end(), [](const PreviewTexture& x) { return x.ready(); });
        if (whole && (!same_view || std::llabs(_thumb_index[k] - want) < std::llabs(_frame_tex_index - want)))
            for (int s = 0; s < n; s++) tex[(size_t)s] = &t[(size_t)s];
    }

    // Every stream in its own slot at one height, as a rig's frames are one
    // instant -- sized from the file before any of them arrives, so a stream
    // that is late leaves its place empty instead of moving the others.
    std::vector<float> aspect((size_t)n, 1.0f);
    for (int k = 0; k < n; k++) {
        if (tex[(size_t)k]) aspect[(size_t)k] = (float)tex[(size_t)k]->w / (float)tex[(size_t)k]->h;
        else if (stitched) aspect[(size_t)k] = 2.0f;
        else if ((size_t)k < _ui_facts.pictures.size() && _ui_facts.pictures[(size_t)k].second > 0)
            aspect[(size_t)k] = (float)_ui_facts.pictures[(size_t)k].first /
                                (float)_ui_facts.pictures[(size_t)k].second;
    }
    const float total = std::accumulate(aspect.begin(), aspect.end(), 0.0f);
    const float gap = px(4.0f);
    const float s = std::max(1.0f, std::min((box.x - gap * (float)(n - 1)) / total, box.y));
    float x = at.x + (box.x - (s * total + gap * (float)(n - 1))) * 0.5f;
    const float y = at.y + (box.y - s) * 0.5f;
    for (int k = 0; k < n; k++) {
        const float w = s * aspect[(size_t)k];
        const ImVec2 lo(x, y), hi(x + w, y + s);
        if (tex[(size_t)k]) dl->AddImage(tex[(size_t)k]->id(), lo, hi);
        else dl->AddRectFilled(lo, hi, IM_COL32(32, 33, 38, 255));
        if (active_track && n == 2 && !_pano.valid()) {
            const bool enabled = (*active_track < 0 || *active_track == k);
            if (!enabled) {
                dl->AddRectFilled(lo, hi, IM_COL32(8, 8, 12, 180));
                const char* dis_text = pmsg::camera_disabled.get();
                const ImVec2 dts = ImGui::CalcTextSize(dis_text);
                dl->AddText(ImVec2(lo.x + (w - dts.x) * 0.5f, lo.y + (s - dts.y) * 0.5f),
                            IM_COL32(220, 220, 220, 180), dis_text);
            }
            const std::string cam_lbl =
                spirula::i18n::format(pmsg::camera_slot, {(long long)(k + 1)});
            const char* clabel = cam_lbl.c_str();
            const ImVec2 cts = ImGui::CalcTextSize(clabel);
            const ImVec2 cpad(px(5.0f), px(2.0f));
            const ImVec2 clo(hi.x - cts.x - 2 * cpad.x - px(6.0f), lo.y + px(6.0f));
            const ImVec2 chi(hi.x - px(6.0f), lo.y + px(6.0f) + cts.y + 2 * cpad.y);
            dl->AddRectFilled(clo, chi, enabled ? IM_COL32(0, 0, 0, 150) : IM_COL32(40, 20, 20, 180), px(3.0f));
            dl->AddText(ImVec2(clo.x + cpad.x, clo.y + cpad.y),
                        enabled ? IM_COL32(235, 235, 235, 255) : IM_COL32(180, 180, 180, 180), clabel);
        }
        x += w + gap;
    }

    const std::string text =
        spirula::i18n::format(pmsg::frame_at, {count_text(want), clock_text(_playhead)});
    const char* label = text.c_str();
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const ImVec2 pad(px(6.0f), px(3.0f));
    dl->AddRectFilled(ImVec2(at.x + px(6.0f), at.y + px(6.0f)),
                      ImVec2(at.x + px(6.0f) + ts.x + 2 * pad.x, at.y + px(6.0f) + ts.y + 2 * pad.y),
                      IM_COL32(0, 0, 0, 150), px(3.0f));
    dl->AddText(ImVec2(at.x + px(6.0f) + pad.x, at.y + px(6.0f) + pad.y),
                IM_COL32(235, 235, 235, 255), label);
    if (busy && !exact && !_playing) {
        const char* note = pmsg::decoding.get();
        const ImVec2 ns = ImGui::CalcTextSize(note);
        dl->AddText(ImVec2(at.x + box.x - ns.x - px(10.0f), at.y + px(9.0f)),
                    IM_COL32(220, 220, 220, 200), note);
    }
    if (!error.empty()) {
        const ImVec2 save = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(at.x + px(10.0f), at.y + px(34.0f)));
        ImGui::PushTextWrapPos(at.x + box.x - px(10.0f) - ImGui::GetWindowPos().x);
        ui::TextColoredRaw(ImVec4(0.95f, 0.55f, 0.45f, 1.0f), error);
        ImGui::PopTextWrapPos();
        ImGui::SetCursorScreenPos(save);
    }
}

bool VideoBrowser::draw_timeline(float width, double& clip_start, double& clip_end,
                                 bool locked) {
    const double dur = std::max(_ui_facts.seconds, 1e-6);
    const float strip = px(56.0f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float x0 = at.x, x1 = at.x + width;
    auto x_of = [&](double t) { return x0 + (float)(std::clamp(t / dur, 0.0, 1.0)) * (x1 - x0); };
    auto t_of = [&](float x) { return std::clamp((double)(x - x0) / (double)(x1 - x0), 0.0, 1.0) * dur; };

    ui::InvisibleButtonRaw("##timeline", ImVec2(std::max(width, 1.0f), strip));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const float mx = ImGui::GetIO().MousePos.x;
    const double end_t = clip_end > 0.0 ? std::min(clip_end, dur) : dur;
    const float grab = px(7.0f);
    const float xs = x_of(clip_start), xe = x_of(end_t);
    const int grabbed = locked ? 0 : std::fabs(mx - xs) <= grab ? 2 : std::fabs(mx - xe) <= grab ? 3 : 0;
    if (ImGui::IsItemActivated()) _drag = grabbed ? grabbed : 1;
    if (!active) _drag = 0;
    if (hovered && grabbed) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    bool changed = false;
    if (active) {
        const double t = t_of(mx);
        const double min_gap = 1.0 / std::max(_ui_facts.fps, 1.0);
        double before = _playhead;
        if (_drag == 1) {
            _playhead = t;
        } else if (_drag == 2) {
            const double v = std::clamp(t, 0.0, end_t - min_gap);
            changed = v != clip_start;
            clip_start = v < 0.5 * min_gap ? 0.0 : v;
            _playhead = clip_start;
        } else if (_drag == 3) {
            const double v = std::clamp(t, clip_start + min_gap, dur);
            const double was = clip_end;
            clip_end = v >= dur - 0.5 * min_gap ? 0.0 : v;
            changed = clip_end != was;
            _playhead = std::min(v, dur);
        }
        if (_playhead != before) {
            _settle_at = ImGui::GetTime();
            _jumped = true;
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float y0 = at.y, y1 = at.y + strip;
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), ImGui::GetColorU32(ImGuiCol_FrameBg));
    // The strip: each picture cropped to fill its slot, so the strip has no
    // gaps whatever shape the video is.
    const float slot = (x1 - x0) / (float)kThumbs;
    for (int k = 0; k < kThumbs; k++) {
        if (_thumb_tex[k].empty() || !_thumb_tex[k][0].ready()) continue;
        const PreviewTexture& t = _thumb_tex[k][0];
        const float sa = slot / strip, ta = (float)t.w / (float)t.h;
        ImVec2 uv0(0, 0), uv1(1, 1);
        if (ta > sa) {
            const float u = sa / ta;
            uv0.x = 0.5f - u * 0.5f;
            uv1.x = 0.5f + u * 0.5f;
        } else {
            const float v = ta / sa;
            uv0.y = 0.5f - v * 0.5f;
            uv1.y = 0.5f + v * 0.5f;
        }
        dl->AddImage(t.id(), ImVec2(x0 + slot * k, y0), ImVec2(x0 + slot * (k + 1), y1), uv0, uv1);
    }
    // What is left out, dimmed; what is kept, framed; the two ends, handles.
    const ImU32 veil = IM_COL32(0, 0, 0, 165);
    if (xs > x0) dl->AddRectFilled(ImVec2(x0, y0), ImVec2(xs, y1), veil);
    if (xe < x1) dl->AddRectFilled(ImVec2(xe, y0), ImVec2(x1, y1), veil);
    const ImU32 keep = IM_COL32(80, 160, 255, 255);
    dl->AddRect(ImVec2(xs, y0), ImVec2(xe, y1), keep, 0.0f, 0, px(2.0f));
    auto handle = [&](float x, ImU32 col, bool left, bool hot) {
        const float w = px(hot ? 9.0f : 7.0f);
        const float a = left ? x : x - w, b = left ? x + w : x;
        dl->AddRectFilled(ImVec2(a, y0), ImVec2(b, y1), col, px(2.0f));
        for (int i = -1; i <= 1; i++) {
            const float gx = (a + b) * 0.5f + (float)i * px(2.0f);
            dl->AddLine(ImVec2(gx, y0 + strip * 0.35f), ImVec2(gx, y1 - strip * 0.35f),
                        IM_COL32(255, 255, 255, 170), 1.0f);
        }
    };
    if (!locked || clip_start > 0.0 || clip_end > 0.0) {
        handle(xs, IM_COL32(70, 185, 90, 255), true, _drag == 2 || (hovered && grabbed == 2));
        handle(xe, IM_COL32(220, 70, 60, 255), false, _drag == 3 || (hovered && grabbed == 3));
    }
    const float xp = x_of(_playhead);
    dl->AddLine(ImVec2(xp, y0 - px(2.0f)), ImVec2(xp, y1 + px(2.0f)), IM_COL32(255, 255, 255, 255),
                px(2.0f));
    dl->AddTriangleFilled(ImVec2(xp - px(5.0f), y0 - px(4.0f)), ImVec2(xp + px(5.0f), y0 - px(4.0f)),
                          ImVec2(xp, y0 + px(3.0f)), IM_COL32(255, 255, 255, 255));
    if (hovered && !active)
        ui::SetTooltipRaw(clock_text(t_of(mx)) + "\n" + pmsg::timeline_help.get());

    // The ruler: a tick every so many seconds, as many labels as fit.
    const float label_w = ImGui::CalcTextSize("00:00:00").x + px(16.0f);
    const int max_labels = std::max(2, (int)((x1 - x0) / label_w));
    static const double steps[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600};
    double step = steps[sizeof steps / sizeof *steps - 1];
    for (double s : steps)
        if (dur / s <= max_labels) {
            step = s;
            break;
        }
    const ImU32 tick = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    for (double t = 0.0; t <= dur + 1e-9; t += step) {
        const float x = x_of(t);
        dl->AddLine(ImVec2(x, y1), ImVec2(x, y1 + px(4.0f)), tick);
        const std::string s = clock_text(t, false);
        const float tw = ImGui::CalcTextSize(s.c_str()).x;
        const float lx = std::clamp(x - tw * 0.5f, x0, x1 - tw);
        dl->AddText(ImVec2(lx, y1 + px(4.0f)), tick, s.c_str());
    }
    ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
    return changed;
}

}  // namespace gui
