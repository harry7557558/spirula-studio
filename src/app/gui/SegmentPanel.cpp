// SegmentPanel.cpp -- see SegmentPanel.h.

#include "app/gui/SegmentPanel.h"
#include "i18n/catalog/Log.h"

#include "app/gui/Layout.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"

#include "i18n/catalog/Dataset.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

#ifdef SS_BUILD_SAM
#include "nn/io/Image.h"
#include "sam/Masking.h"
#endif
#ifdef SS_HAVE_VIDEO
#include "video/Video.h"
#endif

namespace fs = std::filesystem;

namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

// One colour per object, so a dot on the image and a row in the list are
// obviously the same thing. Red is reserved for negative clicks.
ImU32 object_color(int object) {
    static const ImU32 kColors[] = {
        IM_COL32(80, 220, 110, 255),  IM_COL32(90, 170, 245, 255),
        IM_COL32(245, 200, 70, 255),  IM_COL32(200, 130, 245, 255),
        IM_COL32(80, 225, 220, 255),  IM_COL32(245, 150, 90, 255),
    };
    const int n = (int)(sizeof(kColors) / sizeof(kColors[0]));
    return kColors[((object % n) + n) % n];
}

}  // namespace

// One frame in the panel's own hands. Not nn::Image: the shape editor works
// with no inference layer built, and this is the only picture it needs.
struct SegmentPanel::Rgb {
    int w = 0, h = 0;
    std::vector<uint8_t> px;        // w*h*3, interleaved
    bool empty() const { return px.empty(); }
};

// State kept alive across runs so a prompt edit costs one forward pass rather
// than a 3-second weight upload.
struct SegmentPanel::Job {
    std::string frame_key;          // which frame `frame` holds
    Rgb         frame;
#ifdef SS_BUILD_SAM
    sam::Masker masker;
    std::string loaded_model;       // what masker was initialized with
    std::string loaded_signature;   // and with which prompt settings
#endif
};

SegmentPanel::SegmentPanel() = default;

SegmentPanel::~SegmentPanel() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
}

void SegmentPanel::open(const std::string& input, bool is_video,
                        const std::string& model_path,
                        const std::string& ffmpeg_exe, bool force_ffmpeg) {
    _src = PreviewSource{};
    _src.input = input;
    _src.is_video = is_video;
    _src.ffmpeg_exe = ffmpeg_exe.empty() ? "ffmpeg" : ffmpeg_exe;
    _src.builtin_decode = !force_ffmpeg && backends().builtin_video;
    _model_path = model_path;
    _frame_idx = 0;
    _frame_dirty = true;
    _needs_run = true;
    _kept_fraction = -1.0f;
    _border = app::BorderDetect{};
    _border_camera.clear();
    _detect_asked = false;
    _shape_sel = -1;
    _drag_handle = -1;
    _stencil_key.clear();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _status.clear();
        _error.clear();
    }
    // A dozen spread through the capture is enough to judge a prompt.
    collect_preview_frames(_src, is_video ? 8 : 12, _frames, _all_files, _cancel);
    _open = true;
}

void SegmentPanel::close() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    // Drop the model: the reconstruction that usually follows wants the VRAM,
    // and ~Masker -> Session::unload() hands the weights back for real rather
    // than leaving them in the inference layer's process-wide pool. The device
    // itself stays up -- reopening the panel is common, and rebuilding it
    // would cost a pipeline rebuild for the ~50 MB it holds. A dataset run
    // takes the device down at the end (DatasetPrep::run).
    _job.reset();
    _open = false;
}

void SegmentPanel::destroy_gl() {
    if (_tex) {
        glDeleteTextures(1, &_tex);
        _tex = 0;
    }
    if (_stencil_tex) {
        glDeleteTextures(1, &_stencil_tex);
        _stencil_tex = 0;
        _stencil_key.clear();
    }
}

// ---------------------------------------------------------------------------
// Finding the border
// ---------------------------------------------------------------------------

app::FrameMask SegmentPanel::resolved(const app::FrameStencil& s) const {
    app::FrameMask fm = s.mask;
    if (s.detect_border && _border.found) {
        app::MaskShape e = _border.shape;
        const float k = 1.0f - std::clamp(s.shrink, 0.0f, 0.9f);
        e.rx *= k;
        e.ry *= k;
        // First: it is what the drawn shapes are applied to, in order.
        fm.shapes.insert(fm.shapes.begin(), e);
    }
    return fm;
}

std::string SegmentPanel::camera_of(const std::string& file) const {
    if (file.empty()) return std::string();
    return fs::path(file).lexically_relative(_src.input)
        .parent_path().generic_string();
}

void SegmentPanel::start_detect() {
    if (_busy.load() || _detecting.load()) return;
    if (_worker.joinable()) _worker.join();
    _detect_asked = true;
    _detecting = true;

    // One camera folder, not the flattened tree. A PortalCam capture is four
    // of them, two not even fisheye: their union leaves no pixel dark in every
    // frame and no ellipse to fit, so a fit the run makes per folder fails here.
    _border_camera = _frames.empty()
                         ? std::string()
                         : camera_of(_frames[(size_t)_frame_idx].path);
    std::vector<std::string> files;
    for (const std::string& f : _all_files)
        if (camera_of(f) == _border_camera) files.push_back(f);
    const std::string input = _src.input;
    const bool is_video = _src.is_video;
    const std::string ffmpeg_exe = _src.ffmpeg_exe;
    const bool builtin = _src.builtin_decode;
    const double video_seconds = _src.video_seconds;

    _worker = std::thread([this, files, input, is_video, ffmpeg_exe, builtin,
                           video_seconds] {
        struct Guard {
            std::atomic<bool>& flag;
            ~Guard() { flag = false; }
        } guard{_detecting};

        // The shrink is applied when the shape is used, so the slider costs no
        // second fit.
        app::BorderDetectOptions o;
        o.shrink = 0.0f;
        app::BorderDetect found;
        if (!is_video) {
            found = app::detect_fisheye_border(files, o);
        } else {
            app::BorderAccumulator acc;
            bool decoded = false;
#ifdef SS_HAVE_VIDEO
            if (builtin) {
                // The decoder cannot seek, so this reads forward from the start.
                video::VideoReader r;
                if (r.open(input)) {
                    const long long total = std::max(1LL, (long long)r.info().frame_count);
                    const long long span = std::min(total, 720LL);
                    const long long stride = std::max(1LL, span / o.samples);
                    for (long long i = 0; i < span; i++) {
                        if (_cancel.load()) return;
                        nn::Image f = r.readFrame();
                        if (f.empty()) break;
                        if (i % stride == 0)
                            acc.add(f.data.data(), f.width, f.height, f.channels);
                    }
                    decoded = acc.frames() >= 2;
                    if (decoded) found = acc.finish(o);
                }
            }
#else
            (void)builtin;
#endif
            if (!decoded) {
                // One subprocess per still, so far fewer of them -- ffmpeg can
                // at least seek, which is what makes the spread worth having.
                constexpr int kStills = 8;
                std::vector<std::string> stills;
                for (int i = 0; i < kStills; i++) {
                    if (_cancel.load()) break;
                    const double at = std::min(
                        video_seconds * (double)i / (double)kStills,
                        std::max(0.0, video_seconds - 0.1));
                    const fs::path tmp =
                        fs::temp_directory_path() /
                        ("spirula-border-" + std::to_string(i) + "-" +
                         std::to_string(std::chrono::steady_clock::now()
                                            .time_since_epoch().count()) + ".jpg");
                    if (ffmpeg_extract_frame(ffmpeg_exe, input, at, tmp.string(),
                                             _cancel))
                        stills.push_back(tmp.string());
                }
                found = app::detect_fisheye_border(stills, o);
                std::error_code rm;
                for (const std::string& p : stills) fs::remove(p, rm);
            }
        }
        if (_cancel.load()) return;
        std::lock_guard<std::mutex> lk(_mu);
        _border_pending = found;
        _border_ready = true;
    });
}

// ---------------------------------------------------------------------------
// The worker
// ---------------------------------------------------------------------------

void SegmentPanel::start_job(const MaskSettings& s, const app::FrameMask& stencil) {
    if (_busy.load() || _detecting.load()) return;
    if (_worker.joinable()) _worker.join();

    const int idx = _frame_idx;
    const PreviewFrame frame =
        (idx >= 0 && idx < (int)_frames.size()) ? _frames[idx] : PreviewFrame{};
    const PreviewSource src = _src;
    const std::string model = _model_path;
    // Only what was drawn on THIS frame: the preview segments one still, with
    // no memory bank, so a click made on another frame has nothing to say
    // about this one. The run is where they all come together.
    std::vector<MaskClick> clicks;
    for (const MaskClick& c : s.clicks)
        if (mine(c) && c.frame == frame.index) clicks.push_back(c);
    const MaskSettings settings = s;
    const bool frame_dirty = _frame_dirty;
    _frame_dirty = false;

    _busy = true;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _status = dmsg::preview_working.get();
    }

    _worker = std::thread([this, settings, model, frame, src, idx, clicks,
                           frame_dirty, stencil] {
        // Every failure below leaves through `return set_error(...)`, so the
        // flag cannot be cleared at the end of the function: one early exit
        // would strand it at true, and start_job() refuses to run while it is
        // -- a panel that never shows anything again, whatever you type.
        struct BusyGuard {
            std::atomic<bool>& flag;
            ~BusyGuard() { flag = false; }
        } busy_guard{_busy};

        auto set_error = [&](const std::string& e) {
            std::lock_guard<std::mutex> lk(_mu);
            _error = e;
            _status.clear();
        };
        auto set_status = [&](const spirula::i18n::Msg& m) {
            std::lock_guard<std::mutex> lk(_mu);
            _status = m.get();
        };
        // The frame on its own, before any mask exists. Called as soon as it is
        // decoded so the panel shows a picture while the checkpoint uploads --
        // and so that a user whose only prompt is a click, or who is dragging a
        // shape and has no model at all, has something to work on.
        auto show_frame = [&](const Rgb& img) {
            std::lock_guard<std::mutex> lk(_mu);
            _preview = img.px;
            _preview_w = img.w;
            _preview_h = img.h;
            _preview_dirty = true;
            _kept_fraction = -1.0f;
        };
        try {
            if (!_job) _job = std::make_unique<Job>();
            Job& j = *_job;

            // ---- the frame ----
            const std::string key = src.is_video ? ("#" + std::to_string(idx))
                                                 : frame.path;
            if (frame_dirty || j.frame_key != key || j.frame.empty()) {
                set_status(dmsg::preview_loading_frame);
                Rgb img;
                std::string err;
                if (!load_preview_frame(src, frame, img.w, img.h, img.px, err,
                                        _cancel)) {
                    if (_cancel.load()) return;
                    return set_error(err);
                }
                j.frame = std::move(img);
                j.frame_key = key;
                show_frame(j.frame);
            }
            if (_cancel.load()) return;

            // ---- the stencil ----
            // Geometry, so it is known here with no model and no forward pass.
            // It is drawn OVER the picture rather than into it (the panel keeps
            // a live overlay while a shape is dragged), so it is counted below
            // and tinted nowhere.
            std::vector<uint8_t> stencil_px;
            if (!stencil.empty()) {
                std::string err;
                app::rasterize_frame_mask(stencil, j.frame.w, j.frame.h,
                                          stencil_px, err);
            }
            auto stencil_keeps = [&](size_t i) {
                return stencil_px.empty() || stencil_px[i] > 127;
            };
            auto stencil_only = [&]() {
                show_frame(j.frame);
                std::lock_guard<std::mutex> lk(_mu);
                if (stencil_px.empty()) {
                    _status = dmsg::preview_say_or_click.get();
                } else {
                    size_t kept = 0;
                    for (uint8_t v : stencil_px) kept += v > 127 ? 1 : 0;
                    _kept_fraction = (float)((double)kept / (double)stencil_px.size());
                    _status.clear();
                }
            };

            const bool wants_model = !settings.prompt.empty() || !clicks.empty();
#ifndef SS_BUILD_SAM
            stencil_only();
            if (wants_model)
                set_error(spirula::i18n::msg::log::err_no_builtin_segmentation.get());
#else
            if (!wants_model) {
                stencil_only();
                return;
            }
            if (model.empty()) {
                stencil_only();
                return set_error(
                    spirula::i18n::msg::log::err_no_model_selected.get());
            }

            // ---- the model ----
            // Every field below changes what the masker computes, so the
            // signature is what decides whether it can be reused. The weights
            // are the expensive part and only the model path moves them.
            std::string sig = settings.prompt + "|" + settings.negative_prompt + "|" +
                              std::to_string((int)settings.keep_subject) + "|" +
                              std::to_string(settings.max_image_size) + "|" +
                              std::to_string(settings.threshold) + "|" +
                              std::to_string(settings.nms);
            for (const MaskClick& c : clicks)
                sig += "|" + std::to_string(c.object) + ":" + std::to_string(c.x) +
                       "," + std::to_string(c.y) + (c.positive ? "+" : "-");
            if (j.loaded_model != model || j.loaded_signature != sig) {
                set_status(j.loaded_model == model ? dmsg::preview_preparing
                                                   : dmsg::preview_loading_model);
                sam::MaskOptions mo;
                mo.model = model;
                mo.text = settings.prompt;
                mo.neg_text = settings.negative_prompt;
                mo.keep_prompted = settings.keep_subject;
                mo.max_size = settings.max_image_size;
                mo.threshold = settings.threshold;
                mo.nms = settings.nms;
                mo.video = false;      // one still frame, no memory bank
                for (const MaskClick& c : clicks) {
                    sam::SeedPrompt seed;
                    seed.object = c.object;
                    seed.frame = frame.index;
                    if (c.positive) seed.prompt.pos_points.push_back({c.x, c.y});
                    else            seed.prompt.neg_points.push_back({c.x, c.y});
                    mo.seeds.push_back(seed);
                }
                std::string err;
                // Re-initializing is cheap when only the prompt moved: the
                // session keeps the weights it already uploaded.
                if (!j.masker.init(mo, err)) {
                    j.loaded_model.clear();
                    return set_error(err);
                }
                j.loaded_model = model;
                j.loaded_signature = sig;
            }
            if (_cancel.load()) return;

            // ---- run ----
            set_status(dmsg::preview_segmenting);
            nn::Image img;
            img.width = j.frame.w;
            img.height = j.frame.h;
            img.channels = 3;
            img.data = j.frame.px;
            sam::Mask mask;
            sam::Result detections;
            if (!j.masker.run(img, mask, &detections, frame.index))
                return set_error(j.masker.lastError());

            // ---- composite ----
            // Kept pixels stay as they are; masked-out pixels are dimmed and
            // tinted red, which reads as "this will be ignored" far better
            // than a separate black-and-white mask image next to the photo.
            std::vector<uint8_t> rgb = j.frame.px;
            size_t kept = 0;
            const size_t n = (size_t)j.frame.w * j.frame.h;
            for (size_t i = 0; i < n && i * 3 + 2 < rgb.size(); i++) {
                if (i < mask.data.size() && mask.data[i] > 127) {
                    kept += stencil_keeps(i) ? 1 : 0;
                    continue;
                }
                rgb[i * 3 + 0] = (uint8_t)(rgb[i * 3 + 0] / 3 + 150);
                rgb[i * 3 + 1] = (uint8_t)(rgb[i * 3 + 1] / 3);
                rgb[i * 3 + 2] = (uint8_t)(rgb[i * 3 + 2] / 3);
            }
            {
                std::lock_guard<std::mutex> lk(_mu);
                _preview.swap(rgb);
                _preview_w = j.frame.w;
                _preview_h = j.frame.h;
                _preview_dirty = true;
                _kept_fraction = n ? (float)((double)kept / (double)n) : -1.0f;
                _status.clear();
                if (detections.detections.empty() && !settings.prompt.empty() &&
                    clicks.empty())
                    _error =
                        spirula::i18n::msg::log::err_prompt_matched_nothing.get();
                else if (detections.detections.empty() && !clicks.empty())
                    _error =
                        spirula::i18n::msg::log::err_clicks_matched_nothing.get();
            }
#endif
        } catch (const std::exception& e) {
            set_error(e.what());
        }
    });
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void SegmentPanel::upload_preview() {
    std::vector<uint8_t> pixels;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_preview_dirty) return;
        _preview_dirty = false;
        pixels = _preview;
        w = _preview_w;
        h = _preview_h;
    }
    if (pixels.empty() || w <= 0 || h <= 0) return;
    if (!_tex) glGenTextures(1, &_tex);
    glBindTexture(GL_TEXTURE_2D, _tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 pixels.data());
    _tex_w = w;
    _tex_h = h;
}

// The stencil as a translucent red layer over the frame. Rasterizing it into a
// small texture beats drawing it: shapes that keep and shapes that remove can
// compose into a region no sequence of ImDrawList calls describes, and the
// rasterizer that answers it here is the one that writes the masks.
void SegmentPanel::upload_stencil(const app::FrameMask& stencil) {
    const std::string key = app::format_mask_shapes(stencil.shapes) + "|" +
                            stencil.image;
    if (key == _stencil_key) return;
    _stencil_key = key;
    if (stencil.empty()) {
        if (_stencil_tex) {
            glDeleteTextures(1, &_stencil_tex);
            _stencil_tex = 0;
        }
        return;
    }
    const float aspect = _tex_h > 0 ? (float)_tex_w / (float)_tex_h : 1.0f;
    const int w = aspect >= 1.0f ? 512 : std::max(8, (int)(512 * aspect));
    const int h = aspect >= 1.0f ? std::max(8, (int)(512 / aspect)) : 512;
    std::vector<uint8_t> px;
    std::string err;
    if (!app::rasterize_frame_mask(stencil, w, h, px, err)) return;
    std::vector<uint8_t> rgba((size_t)w * h * 4, 0);
    for (size_t i = 0; i < px.size(); i++) {
        if (px[i] > 127) continue;
        rgba[i * 4 + 0] = 235;
        rgba[i * 4 + 1] = 45;
        rgba[i * 4 + 2] = 45;
        rgba[i * 4 + 3] = 150;
    }
    if (!_stencil_tex) glGenTextures(1, &_stencil_tex);
    glBindTexture(GL_TEXTURE_2D, _stencil_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba.data());
}

namespace {

// A shape's draggable points, in normalized image coordinates: the first is
// what moves the whole thing, the rest resize it.
int shape_handles(const app::MaskShape& s, ImVec2 out[3]) {
    if (s.kind == app::MaskShape::Kind::Ellipse) {
        out[0] = ImVec2(s.cx, s.cy);
        out[1] = ImVec2(s.cx + s.rx, s.cy);
        out[2] = ImVec2(s.cx, s.cy + s.ry);
        return 3;
    }
    out[0] = ImVec2((s.cx + s.rx) * 0.5f, (s.cy + s.ry) * 0.5f);
    out[1] = ImVec2(s.cx, s.cy);
    out[2] = ImVec2(s.rx, s.ry);
    return 3;
}

bool shape_contains(const app::MaskShape& s, float u, float v) {
    if (s.kind == app::MaskShape::Kind::Ellipse) {
        if (s.rx <= 0.0f || s.ry <= 0.0f) return false;
        const float du = (u - s.cx) / s.rx, dv = (v - s.cy) / s.ry;
        return du * du + dv * dv <= 1.0f;
    }
    return u >= std::min(s.cx, s.rx) && u <= std::max(s.cx, s.rx) &&
           v >= std::min(s.cy, s.ry) && v <= std::max(s.cy, s.ry);
}

void move_shape(app::MaskShape& s, float du, float dv) {
    s.cx += du;
    s.cy += dv;
    if (s.kind == app::MaskShape::Kind::Rect) {
        s.rx += du;
        s.ry += dv;
    }
}

void move_handle(app::MaskShape& s, int handle, float u, float v) {
    if (s.kind == app::MaskShape::Kind::Ellipse) {
        if (handle == 0) { s.cx = u; s.cy = v; }
        else if (handle == 1) s.rx = std::max(0.005f, std::fabs(u - s.cx));
        else s.ry = std::max(0.005f, std::fabs(v - s.cy));
        return;
    }
    if (handle == 0) {
        const float w = s.rx - s.cx, h = s.ry - s.cy;
        s.cx = u - w * 0.5f;
        s.cy = v - h * 0.5f;
        s.rx = s.cx + w;
        s.ry = s.cy + h;
    } else if (handle == 1) { s.cx = u; s.cy = v; }
    else { s.rx = u; s.ry = v; }
}

}  // namespace

void SegmentPanel::draw_image(MaskSettings& settings, app::FrameStencil& stencil,
                              bool& edited) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float box_h = std::max(avail.y - 8.0f, 120.0f);
    if (!_tex || _tex_w <= 0) {
        ImGui::Dummy(ImVec2(avail.x, box_h));
        return;
    }
    const float aspect = (float)_tex_w / (float)_tex_h;
    ImVec2 size(avail.x, avail.x / aspect);
    if (size.y > box_h) {
        size.y = box_h;
        size.x = box_h * aspect;
    }
    // An InvisibleButton rather than an Image: a plain Image is not an item the
    // mouse can hold, so a drag across it moves the WINDOW, which is exactly
    // what happens while pulling a shape's handle around.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ui::InvisibleButtonRaw("##canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);
    const ImVec2 far_corner(origin.x + size.x, origin.y + size.y);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddImage((ImTextureID)(intptr_t)_tex, origin, far_corner);
    const app::FrameMask shown = resolved(stencil);
    upload_stencil(shown);
    if (_stencil_tex)
        dl->AddImage((ImTextureID)(intptr_t)_stencil_tex, origin, far_corner);

    auto to_screen = [&](float u, float v) {
        return ImVec2(origin.x + u * size.x, origin.y + v * size.y);
    };
    const PreviewFrame frame =
        _frames.empty() ? PreviewFrame{} : _frames[(size_t)_frame_idx];
    const bool hovered = ImGui::IsItemHovered();

    // The selected shape owns the mouse over its handles and over its body:
    // dragging a circle and pointing at an object share this canvas, and the
    // shape being SELECTED is what tells the two apart -- no mode switch, and
    // an unselected shape (the fisheye circle, usually) never eats a click.
    const ImVec2 mouse = ImGui::GetMousePos();
    const float mu = (mouse.x - origin.x) / size.x;
    const float mv = (mouse.y - origin.y) / size.y;
    bool on_shape = false;
    if (_shape_sel >= 0 && _shape_sel < (int)stencil.mask.shapes.size()) {
        app::MaskShape& s = stencil.mask.shapes[(size_t)_shape_sel];
        ImVec2 hs[3];
        const int n = shape_handles(s, hs);
        for (int i = 0; i < n; i++) {
            const ImVec2 p = to_screen(hs[i].x, hs[i].y);
            // `near` is a macro in the Windows headers; do not name it that.
            const bool hit = std::fabs(mouse.x - p.x) < 9.0f &&
                             std::fabs(mouse.y - p.y) < 9.0f;
            on_shape |= hit && hovered;
            dl->AddCircleFilled(p, 6.0f, IM_COL32(255, 255, 255, 230));
            dl->AddCircle(p, 6.0f, IM_COL32(30, 30, 30, 220), 0, 1.5f);
            if (hit && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                _drag_handle = i;
        }
        if (hovered && shape_contains(s, mu, mv)) {
            on_shape = true;
            if (_drag_handle == -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                _drag_handle = kDragBody;
                _drag_from_u = mu;
                _drag_from_v = mv;
            }
        }
        if (_drag_handle != -1) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if (_drag_handle == kDragBody) {
                    move_shape(s, mu - _drag_from_u, mv - _drag_from_v);
                    _drag_from_u = mu;
                    _drag_from_v = mv;
                } else {
                    move_handle(s, _drag_handle, mu, mv);
                }
                on_shape = true;
            } else {
                _drag_handle = -1;
                edited = true;      // rerun once, on release
            }
        }
    }

    // Clicks land in source-image pixels, which is what the model wants.
    const bool canvas_free = hovered && !on_shape && _drag_handle == -1;
    if (canvas_free && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        MaskClick c;
        c.x = mu * (float)_tex_w;
        c.y = mv * (float)_tex_h;
        c.positive = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        c.object = settings.current_object;
        c.frame = frame.index;
        c.position = frame.position;
        c.source = _src.input;
        settings.clicks.push_back(c);
        start_job(settings, shown);
    }
    if (canvas_free)
        ui::SetTooltip(dmsg::click_tooltip, {settings.current_object + 1});

    // Outlines: the exact boundary, over an overlay that is only 512 px wide.
    for (size_t i = 0; i < stencil.mask.shapes.size(); i++) {
        const app::MaskShape& s = stencil.mask.shapes[i];
        const ImU32 col = (int)i == _shape_sel ? IM_COL32(255, 255, 255, 235)
                                               : IM_COL32(255, 255, 255, 120);
        if (s.kind == app::MaskShape::Kind::Ellipse)
            dl->AddEllipse(to_screen(s.cx, s.cy),
                           ImVec2(s.rx * size.x, s.ry * size.y), col, 0.0f, 64,
                           (int)i == _shape_sel ? 2.0f : 1.5f);
        else
            dl->AddRect(to_screen(s.cx, s.cy), to_screen(s.rx, s.ry), col, 0.0f,
                        0, (int)i == _shape_sel ? 2.0f : 1.5f);
    }

    for (const MaskClick& c : settings.clicks) {
        if (!mine(c) || c.frame != frame.index) continue;
        const ImVec2 p(origin.x + c.x / (float)_tex_w * size.x,
                       origin.y + c.y / (float)_tex_h * size.y);
        const ImU32 col = c.positive ? object_color(c.object)
                                     : IM_COL32(240, 90, 90, 255);
        dl->AddCircleFilled(p, 6.0f, col);
        dl->AddCircle(p, 6.0f, IM_COL32(20, 20, 20, 200), 0, 1.5f);
        // A negative click is a cross, so the two are told apart without
        // relying on colour alone.
        if (!c.positive) {
            dl->AddLine(ImVec2(p.x - 3, p.y - 3), ImVec2(p.x + 3, p.y + 3),
                        IM_COL32(255, 255, 255, 255), 1.5f);
            dl->AddLine(ImVec2(p.x - 3, p.y + 3), ImVec2(p.x + 3, p.y - 3),
                        IM_COL32(255, 255, 255, 255), 1.5f);
        }
    }
}

// ---------------------------------------------------------------------------
// The stencil
// ---------------------------------------------------------------------------

void SegmentPanel::draw_stencil(app::FrameStencil& s, bool& edited) {
    ui::Text(dmsg::stencil_section);
    ui::help_on_hover(dmsg::stencil_section_help);

    bool detect = s.detect_border;
    if (ui::Checkbox(dmsg::stencil_border, &detect)) {
        s.detect_border = detect;
        edited = true;
    }
    ui::help_on_hover(dmsg::stencil_border_help);
    if (s.detect_border) {
        // Lazily, here rather than off the checkbox: the option is on already
        // when the panel opens on an input that was set up before.
        if (!_detect_asked) start_detect();
        // The slider walks into other camera folders, whose circle is not this
        // one's; refit rather than draw the wrong ellipse over the frame.
        else if (!_detecting.load() && !_frames.empty() &&
                 camera_of(_frames[(size_t)_frame_idx].path) != _border_camera)
            start_detect();
        ImGui::Indent();
        float pct = s.shrink * 100.0f;
        ImGui::SetNextItemWidth(-1);
        if (ui::SliderFloatRaw("##shrink", &pct, 0.0f, 5.0f, "%.1f%%")) {
            s.shrink = pct / 100.0f;
            _stencil_key.clear();
            edited = true;
        }
        ui::help_on_hover(dmsg::stencil_shrink_help);
        if (_detecting.load())
            ui::TextDisabled(dmsg::stencil_looking);
        else if (_detect_asked && !_border.found)
            ui::TextColoredWrapped(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                                   dmsg::stencil_border_none);
        if (!_detecting.load() && ui::SmallButton(dmsg::stencil_look_again))
            start_detect();
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ui::Text(dmsg::stencil_shapes);
    ui::help_on_hover(dmsg::stencil_shapes_help);
    auto add = [&](app::MaskShape::Kind kind, bool remove) {
        app::MaskShape n;
        n.kind = kind;
        n.remove = remove;
        // Half the frame, in the middle: big enough to grab, small enough that
        // what it does is visible at a glance.
        n.cx = kind == app::MaskShape::Kind::Ellipse ? 0.5f : 0.25f;
        n.cy = kind == app::MaskShape::Kind::Ellipse ? 0.5f : 0.25f;
        n.rx = kind == app::MaskShape::Kind::Ellipse ? 0.25f : 0.75f;
        n.ry = kind == app::MaskShape::Kind::Ellipse ? 0.25f : 0.75f;
        s.mask.shapes.push_back(n);
        _shape_sel = (int)s.mask.shapes.size() - 1;
        _stencil_key.clear();
        edited = true;
    };
    if (ui::SmallButton(dmsg::stencil_add_box)) add(app::MaskShape::Kind::Rect, true);
    ImGui::SameLine();
    if (ui::SmallButton(dmsg::stencil_add_circle))
        add(app::MaskShape::Kind::Ellipse, true);

    for (size_t i = 0; i < s.mask.shapes.size(); i++) {
        app::MaskShape& sh = s.mask.shapes[i];
        ImGui::PushID((int)i);
        const std::string label = spirula::i18n::format(
            sh.kind == app::MaskShape::Kind::Rect ? dmsg::stencil_shape_box
                                                  : dmsg::stencil_shape_circle,
            {(int)i + 1});
        // Toggling, not a plain radio: the selected shape swallows clicks on
        // the picture, so there has to be a way to let go of it again.
        const bool sel = _shape_sel == (int)i;
        if (ui::RadioButtonRaw(label.c_str(), sel)) {
            _shape_sel = sel ? -1 : (int)i;
            _drag_handle = -1;
        }
        ImGui::SameLine();
        if (ui::SmallButton(sh.remove ? dmsg::stencil_removes_inside
                                      : dmsg::stencil_keeps_inside)) {
            sh.remove = !sh.remove;
            _stencil_key.clear();
            edited = true;
        }
        ui::help_on_hover(dmsg::stencil_flip_help);
        ImGui::SameLine();
        if (ui::SmallButton(dmsg::remove)) {
            s.mask.shapes.erase(s.mask.shapes.begin() + (long)i);
            _shape_sel = -1;
            _drag_handle = -1;
            _stencil_key.clear();
            edited = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (_shape_sel >= 0) ui::TextDisabled(dmsg::stencil_drag_hint);
}

// ---------------------------------------------------------------------------
// The object list
// ---------------------------------------------------------------------------

void SegmentPanel::draw_objects(MaskSettings& settings, bool& edited) {
    ui::Text(dmsg::objects_to_click);
    ui::help_on_hover(dmsg::objects_to_click_help);

    for (int o = 0; o < settings.object_count; ++o) {
        ImGui::PushID(o);
        int here = 0, elsewhere = 0;
        const long long cur = _frames.empty() ? 0 : _frames[(size_t)_frame_idx].index;
        for (const MaskClick& c : settings.clicks)
            if (mine(c) && c.object == o) (c.frame == cur ? here : elsewhere)++;

        const ImU32 col = object_color(o);
        ImGui::ColorButton("##col", ImGui::ColorConvertU32ToFloat4(col),
                           ImGuiColorEditFlags_NoTooltip |
                               ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(12, 12));
        ImGui::SameLine();
        const std::string label =
            (here || elsewhere)
                ? spirula::i18n::format(dmsg::object_with_clicks,
                                        {o + 1, here, elsewhere})
                : spirula::i18n::format(dmsg::object_no_clicks, {o + 1});
        // PushID(o) above already separates the rows, so the label carries no
        // ID of its own.
        if (ui::RadioButtonRaw(label.c_str(), settings.current_object == o))
            settings.current_object = o;
        if (here || elsewhere) {
            ImGui::SameLine();
            if (ui::SmallButton(dmsg::object_clear)) {
                auto& v = settings.clicks;
                v.erase(std::remove_if(v.begin(), v.end(),
                                       [&](const MaskClick& c) {
                                           return mine(c) && c.object == o;
                                       }),
                        v.end());
                edited = true;
            }
        }
        ImGui::PopID();
    }

    if (ui::SmallButton(dmsg::object_another)) {
        settings.current_object = settings.object_count++;
    }
    ui::help_on_hover(dmsg::object_another_help);
    if (settings.object_count > 1) {
        ImGui::SameLine();
        if (ui::SmallButton(dmsg::object_clear_all)) {
            auto& v = settings.clicks;
            v.erase(std::remove_if(v.begin(), v.end(),
                                   [&](const MaskClick& c) { return mine(c); }),
                    v.end());
            settings.object_count = 1;
            settings.current_object = 0;
            edited = true;
        }
    }
}

void SegmentPanel::draw(MaskSettings& settings, app::FrameStencil& stencil) {
    if (!_open) return;

    upload_preview();
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_border_ready) {
            _border_ready = false;
            _border = _border_pending;
            _stencil_key.clear();
            _needs_run = true;
        }
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.8f, vp->WorkSize.y * 0.8f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (!ImGui::Begin(ui::detail::label(dmsg::preview_title), &open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) close();
        return;
    }

    ui::TextDisabled(dmsg::preview_legend);
    ImGui::Separator();

    // ---- controls ----
    const float panel_w = 360.0f;
    ImGui::BeginChild("##segctl", ImVec2(panel_w, 0), ImGuiChildFlags_Borders);

    bool edited = false;

#ifndef SS_BUILD_SAM
    // No segmentation in this build, so the half of the panel that prompts a
    // model would be a row of dead controls. The shapes below need none.
    ui::TextWrappedRaw(backends().masking_note);
    ui::help_on_hover_raw(backends().masking_reason.c_str());
#else
    // Polarity first, and the two fields below it are labelled by it: the same
    // box means "take this out" or "this is the subject" depending on the
    // radio, and a label that does not follow the switch reads as a bug.
    // Stacked, not side by side: at this panel width the second label clips.
    int polarity = settings.keep_subject ? 1 : 0;
    if (ui::RadioButton(dmsg::mask_remove_named, polarity == 0)) polarity = 0;
    if (ui::RadioButton(dmsg::mask_keep_named, polarity == 1)) polarity = 1;
    if ((polarity == 1) != settings.keep_subject) {
        settings.keep_subject = polarity == 1;
        edited = true;
    }
    ui::help_on_hover(dmsg::preview_polarity_help);
    const bool keep = settings.keep_subject;

    // English, whatever the interface language is -- see MaskPrompt.h.
    ImGui::Spacing();
    ui::Text(keep ? dmsg::preview_what_kept : dmsg::preview_what_removed);
    ImGui::SetNextItemWidth(-1);
    edited |= ui::InputTextEnglishRaw(
        "##prompt",
        keep ? "the statue; its pedestal" : "person; car; shadow of a person",
        &settings.prompt);
    ui::help_on_hover(keep ? dmsg::preview_prompt_help_keep
                           : dmsg::preview_prompt_help_remove);

    ImGui::Spacing();
    ui::Text(keep ? dmsg::preview_but_remove_these
                  : dmsg::preview_but_keep_these);
    ImGui::SetNextItemWidth(-1);
    edited |= ui::InputTextEnglishRaw(
        "##negprompt",
        keep ? "the hand holding it" : "person in a painting",
        &settings.negative_prompt);
    ui::help_on_hover(keep ? dmsg::preview_negative_help_keep
                           : dmsg::preview_negative_help_remove);

    // The palette matters more here than on the dataset screen: this panel is
    // where a prompt is actually iterated on, one try at a time.
    if (spirula::i18n::current() != spirula::i18n::Lang::en)
        ui::TextDisabled(dmsg::mask_english_only);
    edited |= draw_subject_palette(settings.prompt, settings.negative_prompt,
                                   keep);

    ImGui::Spacing();
    ImGui::Separator();
    draw_objects(settings, edited);
#endif

    ImGui::Spacing();
    ImGui::Separator();
    draw_stencil(stencil, edited);

    // ---- frame chooser ----
    if (_frames.size() > 1) {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        int idx = _frame_idx;
        // The slider's own overlay format string: the index is substituted by
        // ImGui, so this is not a place a placeholder can be used.
        const std::string fmt = spirula::i18n::format(
            dmsg::preview_frame, {(long long)_frames[(size_t)_frame_idx].index});
        if (ui::SliderIntRaw("##frame", &idx, 0, (int)_frames.size() - 1,
                             fmt.c_str())) {
            _frame_idx = idx;
            _frame_dirty = true;
            _needs_run = true;
        }
        ui::help_on_hover(dmsg::preview_frame_help);
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(_busy.load());
    if (ui::Button(dmsg::preview_try_it, ImVec2(-1, 30))) _needs_run = true;
    ImGui::EndDisabled();

    // Rerun once the user stops typing, so every keystroke does not queue a
    // forward pass.
    if (edited) _needs_run = true;
    if (_needs_run && !_busy.load() && !_detecting.load() && _drag_handle == -1 &&
        !ImGui::IsAnyItemActive()) {
        _needs_run = false;
        start_job(settings, resolved(stencil));
    }

    ImGui::Spacing();
    {
        std::lock_guard<std::mutex> lk(_mu);
        // Segmentation errors and progress come from the inference layer.
        if (!_error.empty())
            ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _error);
        else if (!_status.empty())
            ui::TextDisabledRaw(_status);
    }
    if (_kept_fraction >= 0.0f) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f", 100.0f * _kept_fraction);
        ui::Text(dmsg::preview_kept_fraction, {pct});
        if (_kept_fraction < 0.05f)
            ui::TextColoredWrapped(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                                   settings.keep_subject
                                       ? dmsg::preview_almost_nothing_kept
                                       : dmsg::preview_almost_all_masked);
    }

    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##segimg", ImVec2(0, 0));
    bool canvas_edited = false;
    draw_image(settings, stencil, canvas_edited);
    if (canvas_edited) _needs_run = true;
    ImGui::EndChild();

    ImGui::End();
    if (!open) close();
}

}  // namespace gui
