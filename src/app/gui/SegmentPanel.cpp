// SegmentPanel.cpp -- see SegmentPanel.h.

#include "app/gui/SegmentPanel.h"
#include "i18n/catalog/Log.h"

#include "app/gui/Layout.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/MaskTint.h"
#include "app/gui/Ui.h"
#include "core/PolygonFill.h"

#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/MaskEdit.h"
#include "app/gui/mask/PathOverlay.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>

#ifdef SS_BUILD_SAM
#include "nn/io/Image.h"
#include "sam/Masking.h"
#endif

namespace fs = std::filesystem;

namespace dmsg = spirula::i18n::msg::dataset;
namespace mmsg = spirula::i18n::msg::maskedit;
namespace gmsg = spirula::i18n::msg::gui;

namespace gui {

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

void SegmentPanel::open(const PreviewSource& src, const MaskModelFiles& model) {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    if (_job) {
        _job->frame_key.clear();
        _job->frame = Rgb{};
    }
    _src = src;
    if (_src.ffmpeg_exe.empty()) _src.ffmpeg_exe = "ffmpeg";
    _model = model;
    _folder_idx = 0;
    _frame_idx = 0;
    _frame_dirty = true;
    _needs_run = true;
    _border = app::BorderDetect{};
    _border_camera.clear();
    _detect_asked = false;
    _shape_sel = -1;
    _drag_handle = -1;
    _stencil_key.clear();
    _draw = DrawTool::Select;
    _tool.cancel();
    _history.clear();
    _saved_msg.clear();
    _path.cancel();
    _pen.cancel();
    _pt_part = PenPart::None;
    _pt_anchor = -1;
    _path.set_livewire(nullptr);
    _livewire.reset();
    _livewire_key.clear();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _kept_fraction = -1.0f;
        _feature_fraction = -1.0f;
        _frames.clear();
        _all_files.clear();
        _folders.clear();
        _cameras.clear();
        _frames_pending.clear();
        _all_files_pending.clear();
        _folders_pending.clear();
        _frames_ready = false;
        _listed_src = PreviewSource{};
        _border_ready = false;
        _border_pending = app::BorderDetect{};
        _preview.clear();
        _preview_w = _preview_h = 0;
        _preview_dirty = false;
        _status = dmsg::preview_working.get();
        _error.clear();
        _livewire_pending.reset();
        _livewire_pending_key.clear();
        _livewire_ready = false;
    }
    _tex_w = _tex_h = 0;
    _view_w = _view_h = 0;
    _panning = false;
    _listing = true;
    _open = true;
    const PreviewSource requested = _src;
    _worker = std::thread([this, requested] {
        struct ListingGuard {
            std::atomic<bool>& flag;
            ~ListingGuard() { flag = false; }
        } listing_guard{_listing};
        try {
            PreviewSource listed = requested;
            std::vector<PreviewFrame> frames;
            std::vector<std::string> all_files;
            collect_preview_frames(listed, listed.is_video ? 8 : 12, frames,
                                   all_files, _cancel);
            if (_cancel.load()) return;
            std::vector<std::string> folders = preview_folders(listed);
            std::lock_guard<std::mutex> lk(_mu);
            _listed_src = std::move(listed);
            _frames_pending = std::move(frames);
            _all_files_pending = std::move(all_files);
            _folders_pending = std::move(folders);
            _frames_ready = true;
            _status.clear();
        } catch (const std::exception& e) {
            if (!_cancel.load()) {
                std::lock_guard<std::mutex> lk(_mu);
                _error = e.what();
                _status.clear();
            }
        }
    });
}

void SegmentPanel::close() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    _listing = false;
    _livewiring = false;
    _draw = DrawTool::Select;
    _tool.cancel();
    _path.cancel();
    _pen.cancel();
    _pt_part = PenPart::None;
    _path.set_livewire(nullptr);
    _livewire.reset();
    _livewire_key.clear();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _frames.clear();
        _all_files.clear();
        _folders.clear();
        _cameras.clear();
        _frames_pending.clear();
        _all_files_pending.clear();
        _folders_pending.clear();
        _frames_ready = false;
        _listed_src = PreviewSource{};
        _border_ready = false;
        _border_pending = app::BorderDetect{};
        _preview.clear();
        _preview_w = _preview_h = 0;
        _preview_dirty = false;
        _livewire_pending.reset();
        _livewire_pending_key.clear();
        _livewire_ready = false;
    }
    _tex_w = _tex_h = 0;
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
    const app::CameraStencil& cs = s.for_camera(shown_camera());
    app::FrameMask fm = cs.mask;
    if (cs.detect_border && _border.found) {
        fm.shapes.insert(fm.shapes.begin(), app::shrink_border(_border.shape, cs.shrink));
    }
    return fm;
}

std::string SegmentPanel::camera_of(const std::string& file) const {
    if (file.empty()) return std::string();
    return fs::path(file).lexically_relative(_src.input)
        .parent_path().generic_string();
}

std::string SegmentPanel::file_camera(const std::string& file) const {
    const std::string sub = camera_of(file);
    if (_src.is_video || _src.look.packed_lenses < 2 || _folder_idx < 0 ||
        _folder_idx >= (int)_folders.size())
        return sub;
    const std::string& lens = _folders[(size_t)_folder_idx];
    return sub.empty() ? lens : lens + "/" + sub;
}

std::string SegmentPanel::shown_camera() const {
    if (_src.is_video)
        return _folder_idx >= 0 && _folder_idx < (int)_folders.size()
                   ? _folders[(size_t)_folder_idx]
                   : std::string();
    return _frames.empty() ? std::string()
                           : file_camera(_frames[(size_t)_frame_idx].path);
}

std::vector<std::string> SegmentPanel::camera_names() const {
    std::set<std::string> out;
    if (_src.is_video) {
        out.insert(_folders.begin(), _folders.end());
    } else {
        for (const std::string& f : _all_files) {
            const std::string sub = camera_of(f);
            if (_src.look.packed_lenses < 2) out.insert(sub);
            else
                for (const std::string& lens : _folders)
                    out.insert(sub.empty() ? lens : lens + "/" + sub);
        }
    }
    return {out.begin(), out.end()};
}

app::CameraStencil& SegmentPanel::scope(app::FrameStencil& s) {
    if (!s.per_camera()) return s;
    const std::string cam = shown_camera();
    auto it = s.cameras.find(cam);
    if (it == s.cameras.end())
        it = s.cameras.emplace(cam, static_cast<const app::CameraStencil&>(s)).first;
    return it->second;
}

// On: every camera starts as a copy of the input's, so nothing changes until
// one is drawn on. Off: the shown camera's becomes the input's.
void SegmentPanel::set_per_camera(app::FrameStencil& s, bool on) {
    if (on) {
        for (const std::string& cam : _cameras)
            s.cameras.emplace(cam, static_cast<const app::CameraStencil&>(s));
    } else if (s.per_camera()) {
        const app::CameraStencil shown = s.for_camera(shown_camera());
        s.cameras.clear();
        static_cast<app::CameraStencil&>(s) = shown;
    }
    _shape_sel = -1;
    _drag_handle = -1;
    _pt_part = PenPart::None;
    _stencil_key.clear();
}

void SegmentPanel::start_detect() {
    if (_busy.load() || _detecting.load() || _listing.load() || _livewiring.load()) return;
    if (_worker.joinable()) _worker.join();
    _detect_asked = true;
    _detecting = true;

    // One camera folder, not the flattened tree. A PortalCam capture is four
    // of them, two not even fisheye: their union leaves no pixel dark in every
    // frame and no ellipse to fit, so a fit the run makes per folder fails here.
    _border_camera = shown_camera();
    std::vector<std::string> files;
    for (const std::string& f : _all_files)
        if (file_camera(f) == _border_camera) files.push_back(f);
    // The shrink is applied when the shape is used, so the slider costs no
    // second fit.
    app::BorderDetectOptions o;
    o.shrink = 0.0f;
    const std::vector<PreviewFrame> frames =
        spread_preview_frames(_src, _frames, files, o.samples);
    const PreviewSource src = _src;
    const int folder = _folder_idx;

    _worker = std::thread([this, frames, src, folder, o] {
        struct Guard {
            std::atomic<bool>& flag;
            ~Guard() { flag = false; }
        } guard{_detecting};

        // The same pixels the panel draws the ellipse over, and the same ones
        // the run fits on: a turned or unwrapped frame has its border
        // somewhere else than the file it came out of.
        app::BorderAccumulator acc;
        scan_preview_frames(src, frames, folder,
                            [&](const uint8_t* rgb, int w, int h) {
                                acc.add(rgb, w, h, 3);
                            },
                            _cancel);
        if (_cancel.load()) return;
        const app::BorderDetect found = acc.frames() >= 2 ? acc.finish(o)
                                                          : app::BorderDetect{};
        std::lock_guard<std::mutex> lk(_mu);
        _border_pending = found;
        _border_ready = true;
    });
}

// The edge map for the shown frame, built on the worker because the decoded
// frame lives in the Job, which only the worker touches. One build per frame
// shown; the panel asks for it when the pen tool is armed.
void SegmentPanel::start_livewire() {
    if (_busy.load() || _detecting.load() || _listing.load() || _livewiring.load()) return;
    if (_worker.joinable()) _worker.join();
    if (!_job || _job->frame.empty()) return;
    _livewiring = true;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _status = mmsg::path_building.get();
    }
    _worker = std::thread([this] {
        struct Guard {
            std::atomic<bool>& flag;
            ~Guard() { flag = false; }
        } guard{_livewiring};
        const auto t0 = std::chrono::steady_clock::now();
        auto lw = std::make_unique<mask::Livewire>();
        lw->build(_job->frame.px.data(), _job->frame.w, _job->frame.h,
                  mask::kLivewireMaxEdge, mask::LivewireWeights{}, &_cancel);
        if (_cancel.load() || !lw->ready()) return;
        std::lock_guard<std::mutex> lk(_mu);
        _livewire_ms = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - t0).count();
        _livewire_pending = std::move(lw);
        _livewire_pending_key = _job->frame_key;
        _livewire_ready = true;
        _status.clear();
    });
}

std::string SegmentPanel::shown_frame_key() const {
    if (_src.is_video)
        return "#" + std::to_string(_folder_idx) + ":" + std::to_string(_frame_idx);
    return _frames.empty() ? std::string() : _frames[(size_t)_frame_idx].path;
}

// ---------------------------------------------------------------------------
// The worker
// ---------------------------------------------------------------------------

void SegmentPanel::start_job(const MaskSettings& s,
                             const app::FrameMask& stencil) {
    if (_busy.load() || _detecting.load() || _listing.load() || _livewiring.load()) return;
    if (_worker.joinable()) _worker.join();

    const int idx = _frame_idx;
    const PreviewFrame frame =
        (idx >= 0 && idx < (int)_frames.size()) ? _frames[idx] : PreviewFrame{};
    const PreviewSource src = _src;
    const int folder = _folder_idx;
    const std::string camera = shown_camera();
    const MaskModelFiles model = _model;
    // Only what was drawn on THIS frame: the preview segments one still, with
    // no memory bank, so a click made on another frame has nothing to say
    // about this one. The run is where they all come together.
    std::vector<MaskClick> clicks;
    for (const MaskClick& c : s.clicks)
        if (mine(c) && c.frame == frame.index && c.camera == camera)
            clicks.push_back(c);
    const MaskSettings settings = s;
    const bool frame_dirty = _frame_dirty;
    _frame_dirty = false;

    _busy = true;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _status = dmsg::preview_working.get();
    }

    _worker = std::thread([this, settings, model, frame, src, idx, folder,
                           clicks, frame_dirty, stencil] {
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
            _feature_fraction = -1.0f;
        };
        try {
            if (!_job) _job = std::make_unique<Job>();
            Job& j = *_job;

            // ---- the frame ----
            const std::string key = src.is_video
                                        ? ("#" + std::to_string(folder) + ":" +
                                           std::to_string(idx))
                                        : frame.path;
            if (frame_dirty || j.frame_key != key || j.frame.empty()) {
                set_status(dmsg::preview_loading_frame);
                Rgb img;
                std::string err;
                if (!load_preview_frame(src, frame, folder, img.w, img.h,
                                        img.px, err, _cancel)) {
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

            const bool subject = model.kind == MaskModelKind::Subject;
            const std::string prompt = model.text ? settings.prompt : "";
            const std::string negative = model.text ? settings.negative_prompt : "";
            const std::string feature = model.text ? settings.feature_prompt : "";
            const bool wants_model =
                subject || !prompt.empty() || !clicks.empty() || !feature.empty();
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
            std::string sig = prompt + "|" + negative + "|" + feature + "|" +
                              std::to_string((int)settings.keep_subject) + "|" +
                              std::to_string(settings.max_image_size) + "|" +
                              std::to_string(settings.threshold) + "|" +
                              std::to_string(settings.nms) + "|" +
                              std::to_string(settings.box_threshold) + "|" +
                              std::to_string(settings.boundary_ratio());
            const std::string model_key = model.model + "|" + model.detector;
            for (const MaskClick& c : clicks)
                sig += "|" + std::to_string(c.object) + ":" + std::to_string(c.x) +
                       "," + std::to_string(c.y) + (c.positive ? "+" : "-");
            if (j.loaded_model != model_key || j.loaded_signature != sig) {
                set_status(j.loaded_model == model_key ? dmsg::preview_preparing
                                                       : dmsg::preview_loading_model);
                sam::MaskOptions mo;
                mo.model = model.model;
                mo.detector = model.detector;
                mo.detector_threshold = settings.box_threshold;
                mo.device = src.device;
                mo.text = prompt;
                mo.neg_text = negative;
                mo.feature_text = feature;
                mo.keep_prompted = settings.keep_subject;
                mo.max_size = settings.max_image_size;
                mo.threshold = settings.threshold;
                mo.nms = settings.nms;
                mo.dilate_ratio = settings.boundary_ratio();
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
                j.loaded_model = model_key;
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
            sam::Mask mask, fmask;
            sam::Result detections;
            if (!j.masker.run(img, mask, &detections, frame.index, &fmask))
                return set_error(j.masker.lastError());

            // ---- composite ----
            // Tinted over the photo, which reads as "ignored" far better than a
            // mask beside it: red is dropped, amber hatching features-only.
            std::vector<uint8_t> rgb = j.frame.px;
            size_t kept = 0, for_features = 0;
            const size_t n = (size_t)j.frame.w * j.frame.h;
            const bool features = fmask.data.size() == n;
            const int period = hatch_period(j.frame.w, j.frame.h);
            for (size_t i = 0; i < n && i * 3 + 2 < rgb.size(); i++) {
                if (i >= mask.data.size() || mask.data[i] <= 127) {
                    tint_removed(&rgb[i * 3]);
                } else if (features && fmask.data[i] <= 127) {
                    tint_features_only(&rgb[i * 3], (int)(i % j.frame.w),
                                       (int)(i / j.frame.w), period);
                    kept += stencil_keeps(i) ? 1 : 0;
                } else if (stencil_keeps(i)) {
                    kept++;
                    for_features++;
                }
            }
            {
                std::lock_guard<std::mutex> lk(_mu);
                _preview.swap(rgb);
                _preview_w = j.frame.w;
                _preview_h = j.frame.h;
                _preview_dirty = true;
                _kept_fraction = n ? (float)((double)kept / (double)n) : -1.0f;
                _feature_fraction =
                    n && features ? (float)((double)for_features / (double)n) : -1.0f;
                _status.clear();
                if (detections.detections.empty() && !prompt.empty() &&
                    clicks.empty() && !subject)
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

// The stencil as a translucent red layer over the frame's on-screen part. A
// raster, not draw calls: keep and remove shapes compose into regions no path
// describes, and this rasterizer is the one that writes the masks.
void SegmentPanel::upload_stencil(const app::FrameMask& stencil, const float win[4], int w,
                                  int h) {
    char view[96];
    std::snprintf(view, sizeof view, "|%g,%g,%g,%g|%dx%d", win[0], win[1], win[2], win[3], w, h);
    const std::string key = app::format_mask_shapes(stencil.shapes) + view;
    if (key == _stencil_key) return;
    _stencil_key = key;
    // The GUI never sets FrameMask::image, so the shapes are the whole overlay.
    if (stencil.shapes.empty()) {
        if (_stencil_tex) {
            glDeleteTextures(1, &_stencil_tex);
            _stencil_tex = 0;
        }
        return;
    }
    app::FrameMask part;
    for (const app::MaskShape& s : stencil.shapes)
        part.shapes.push_back(stencil_crop(s, win[0], win[1], win[2], win[3]));
    std::vector<uint8_t> px;
    std::string err;
    if (!app::rasterize_frame_mask(part, w, h, px, err)) return;
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
    std::copy(win, win + 4, _stencil_win);
}

void SegmentPanel::draw_image(MaskSettings& settings, app::FrameStencil& stencil,
                              bool& edited) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 pane(std::max(avail.x, 64.0f), std::max(avail.y - 8.0f, 120.0f));
    if (!_tex || _tex_w <= 0) {
        ImGui::Dummy(pane);
        return;
    }
    // An InvisibleButton rather than an Image: a plain Image is not an item the
    // mouse can hold, so a drag across it moves the WINDOW, which is exactly
    // what happens while pulling a shape's handle around.
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ui::InvisibleButtonRaw("##canvas", pane,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);   // the wheel zooms; nothing scrolls under it
    const ImVec2 pane_end(corner.x + pane.x, corner.y + pane.y);
    const ImGuiIO& io = ImGui::GetIO();
    const bool space = ImGui::IsKeyDown(ImGuiKey_Space);

    if (_view_w != _tex_w || _view_h != _tex_h) {
        _view = mask::View{1.0f, 0.5f * (float)_tex_w, 0.5f * (float)_tex_h};
        _view_w = _tex_w;
        _view_h = _tex_h;
    }
    const bool brush = _draw == DrawTool::Eraser ||
                       (_draw == DrawTool::Shape && _tool.id() == ToolId::Brush);
    // Not mid-stroke or mid-drag: those hold canvas pixels, which a new view would move.
    if (!_tool.in_progress() && _drag_handle == -1 && _pt_part == PenPart::None) {
        const mask::Mapping m0 = mask::mapping(_view, _tex_w, _tex_h, pane.x, pane.y);
        if (hovered && io.MouseWheel != 0.0f) {
            if (!io.KeyAlt)
                mask::zoom_about(_view, std::pow(1.2f, io.MouseWheel), io.MousePos.x - corner.x,
                                 io.MousePos.y - corner.y, _tex_w, _tex_h, pane.x, pane.y);
            else if (brush)
                _brush_pct = std::clamp(_brush_pct * std::pow(1.25f, io.MouseWheel), 0.2f, 25.0f);
        }
        const bool pan_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                              (space && ImGui::IsMouseDown(ImGuiMouseButton_Left));
        if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                        (space && ImGui::IsMouseClicked(ImGuiMouseButton_Left))))
            _panning = true;
        if (_panning) {
            if (!pan_down) _panning = false;
            else mask::pan(_view, io.MouseDelta.x, io.MouseDelta.y, m0, _tex_w, _tex_h);
        }
    }
    // From here on `origin` and `size` are the picture's rectangle on screen,
    // which may overhang the pane or sit inside it with room around.
    const mask::Mapping m = mask::mapping(_view, _tex_w, _tex_h, pane.x, pane.y);
    const ImVec2 origin(corner.x + m.to_screen_x(0.0f), corner.y + m.to_screen_y(0.0f));
    const ImVec2 size((float)_tex_w * m.scale, (float)_tex_h * m.scale);
    const ImVec2 far_corner(origin.x + size.x, origin.y + size.y);
    // The tools take the pointer off the picture too, anywhere on the pane.
    const bool pointer = hovered && !space && !_panning;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(corner, pane_end, true);
    dl->AddRectFilled(corner, pane_end, IM_COL32(24, 24, 24, 255));
    dl->AddImage((ImTextureID)(intptr_t)_tex, origin, far_corner);
    const app::FrameMask shown = resolved(stencil);
    const float win[4] = {std::max(0.0f, (corner.x - origin.x) / size.x),
                          std::max(0.0f, (corner.y - origin.y) / size.y),
                          std::min(1.0f, (pane_end.x - origin.x) / size.x),
                          std::min(1.0f, (pane_end.y - origin.y) / size.y)};
    if (win[2] > win[0] && win[3] > win[1]) {
        const float vw = (win[2] - win[0]) * size.x, vh = (win[3] - win[1]) * size.y;
        // 1024 bounds the re-raster a pan or a handle drag does every frame.
        const float k = std::min(1.0f, 1024.0f / std::max(vw, vh));
        upload_stencil(shown, win, std::max(8, (int)std::ceil(vw * k)),
                       std::max(8, (int)std::ceil(vh * k)));
    }
    auto to_screen = [&](float u, float v) {
        return ImVec2(origin.x + u * size.x, origin.y + v * size.y);
    };
    if (_stencil_tex)
        dl->AddImage((ImTextureID)(intptr_t)_stencil_tex,
                     to_screen(_stencil_win[0], _stencil_win[1]),
                     to_screen(_stencil_win[2], _stencil_win[3]));

    const PreviewFrame frame =
        _frames.empty() ? PreviewFrame{} : _frames[(size_t)_frame_idx];
    std::vector<app::MaskShape>& shapes = scope(stencil).mask.shapes;
    if (_shape_sel >= (int)shapes.size()) _shape_sel = -1;

    // The edge map belongs to one frame; a frame change drops it, and the
    // armed livewire path asks for this frame's.
    const std::string frame_key = shown_frame_key();
    if (_livewire && _livewire_key != frame_key) {
        _livewire.reset();
        _livewire_key.clear();
        _path.set_livewire(nullptr);
        _path.cancel();
    }
    const bool path_mode = _draw == DrawTool::Path;
    const bool pen_mode = _draw == DrawTool::Pen;
    if (path_mode && !_livewire && !_livewiring.load()) start_livewire();

    // The selected shape owns the mouse over its handles and body (Points: a pen
    // shape's anchors and handles too); under Select every other click prompts
    // the model, so an unselected shape (the fisheye circle, usually) eats none.
    const ImVec2 mouse = ImGui::GetMousePos();
    const float mu = (mouse.x - origin.x) / size.x;
    const float mv = (mouse.y - origin.y) / size.y;
    app::MaskShape* sel = _shape_sel >= 0 ? &shapes[(size_t)_shape_sel] : nullptr;
    bool pen_sel = sel && sel->kind == app::MaskShape::Kind::Bezier;
    // Whatever changed the list under a point drag (undo, the list) ended it.
    if (!pen_sel || _pt_anchor >= (int)(sel->pts.size() / 6)) {
        _pt_part = PenPart::None;
        _pt_pull = false;
        _pt_anchor = -1;
    }
    const bool selecting = _draw == DrawTool::Select || _draw == DrawTool::Points;
    // Ctrl is the vector editors' "direct selection for now" under the pen.
    const bool points_now =
        pen_sel && (_draw == DrawTool::Points ||
                    (pen_mode && !_pen.in_progress() && (io.KeyCtrl || _pt_part != PenPart::None)));
    bool on_shape = false;
    auto grab = [&](int handle) {
        _drag_handle = handle;
        _drag_before = stencil;
        _drag_before_key = app::format_mask_shapes(shapes);
    };
    if (points_now) on_shape = edit_points(stencil, *sel, mu, mv, size.x, size.y, pointer, edited);
    else if (pen_mode && pen_sel && !_pen.in_progress())
        on_shape = pen_on_shape(stencil, *sel, mu, mv, size.x, size.y, pointer, edited);
    if (selecting && sel && _pt_part == PenPart::None) {
        app::MaskShape& s = *sel;
        float hu[3], hv[3];
        const int n = stencil_handles(s, hu, hv);
        for (int i = 0; i < n; i++) {
            const ImVec2 p = to_screen(hu[i], hv[i]);
            // `near` is a macro in the Windows headers; do not name it that.
            const bool hit = std::fabs(mouse.x - p.x) < 9.0f &&
                             std::fabs(mouse.y - p.y) < 9.0f;
            on_shape |= hit && pointer;
            dl->AddCircleFilled(p, 6.0f, IM_COL32(255, 255, 255, 230));
            dl->AddCircle(p, 6.0f, IM_COL32(30, 30, 30, 220), 0, 1.5f);
            if (hit && pointer && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) grab(i);
        }
        if (pointer && !on_shape && stencil_contains(s, mu, mv)) {
            on_shape = true;
            if (_drag_handle == -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                grab(kDragBody);
                _drag_from_u = mu;
                _drag_from_v = mv;
            }
        }
    }
    // Points picks what it clicks, the topmost shape first, and lets go on
    // empty picture; the press that picks also starts moving it.
    if (_draw == DrawTool::Points && pointer && !on_shape && _drag_handle == -1 &&
        _pt_part == PenPart::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        _shape_sel = -1;
        _pt_anchor = -1;
        for (int k = (int)shapes.size() - 1; k >= 0; k--)
            if (stencil_contains(shapes[(size_t)k], mu, mv)) {
                _shape_sel = k;
                grab(kDragBody);
                _drag_from_u = mu;
                _drag_from_v = mv;
                on_shape = true;
                break;
            }
    }
    if (_drag_handle != -1 && _shape_sel >= 0) {
        app::MaskShape& s = shapes[(size_t)_shape_sel];
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (_drag_handle == kDragBody) {
                stencil_move(s, mu - _drag_from_u, mv - _drag_from_v);
                _drag_from_u = mu;
                _drag_from_v = mv;
            } else {
                stencil_move_handle(s, _drag_handle, mu, mv);
            }
            on_shape = true;
        } else {
            _drag_handle = -1;
            // A click that moved nothing is not a step to undo.
            if (_drag_before_key != app::format_mask_shapes(shapes)) {
                _history.push(_drag_before);
                _shapes_edited = true;
                edited = true;      // rerun once, on release
            }
        }
    }

    // What any tool finishes becomes one shape at the end of the list.
    auto add_shape = [&](const app::MaskShape& n) {
        change_shapes(stencil, edited);
        shapes.push_back(n);
        _shape_sel = -1;
        _drag_handle = -1;
    };
    ViewportInput in;
    in.hovered = pointer && !on_shape && _drag_handle == -1;
    in.x = mouse.x - origin.x;
    in.y = mouse.y - origin.y;
    in.W = (int)size.x;
    in.H = (int)size.y;
    in.down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    in.clicked = in.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    in.released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    in.right_clicked = pointer && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    in.double_clicked = in.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    in.shift = io.KeyShift;
    in.ctrl = io.KeyCtrl;
    in.alt = io.KeyAlt;
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                         !io.WantTextInput;
    const bool enter = focused && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                   ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));

    // Canvas pixels <-> frame pixels, for the two tools that keep frame pixels.
    mask::PathSpace sp;
    {
        const float tw = (float)_tex_w, th = (float)_tex_h;
        sp.to_frame = [size, tw, th](float x, float y, float& fx, float& fy) {
            fx = x / size.x * tw;
            fy = y / size.y * th;
        };
        sp.from_frame = [size, tw, th](float fx, float fy, float& x, float& y) {
            x = fx / tw * size.x;
            y = fy / th * size.y;
        };
    }
    bool tool_consumed = false;
    if (path_mode) {
        _path.set_space(sp);
        _path.note_modifiers(io.KeyShift, io.KeyCtrl);
        std::vector<float> poly;
        bool closed = _path.update(in, poly, tool_consumed);
        if (enter && !closed) closed = _path.commit_pending(poly);
        if (closed) {
            ShapeStroke st;
            st.kind = ShapeKind::Polygon;
            st.pts = std::move(poly);
            app::MaskShape n;
            if (stencil_shape_from_stroke(st, size.x, size.y, stroke_removes(_path.mode_ctrl()), n))
                add_shape(n);
        }
        mask::draw_path_overlay(dl, origin, _path);
    } else if (pen_mode) {
        _pen.set_space(sp);
        _pen.note_modifiers(io.KeyShift, io.KeyCtrl);
        _pen.note_space(ImGui::IsKeyDown(ImGuiKey_Space));
        std::vector<float> anchors;
        bool closed = _pen.update(in, anchors, tool_consumed);
        if (enter && !closed) closed = _pen.commit_pending(anchors);
        app::MaskShape n;
        if (closed && stencil_shape_from_pen(anchors, size.x, size.y,
                                             stroke_removes(_pen.mode_ctrl()), n)) {
            add_shape(n);
            // Kept picked, as a vector editor keeps the path it just drew.
            _shape_sel = (int)shapes.size() - 1;
            _pt_anchor = -1;
        }
        mask::draw_pen_overlay(dl, origin, _pen);
    } else if (_draw == DrawTool::Shape || _draw == DrawTool::Eraser) {
        _tool.set_brush_radius(_brush_pct * 0.01f * std::min(size.x, size.y));
        ShapeStroke st;
        bool done = _tool.update(in, st, tool_consumed);
        if (enter && !done) done = _tool.commit_pending(st);
        app::MaskShape n;
        if (done && stencil_shape_from_stroke(st, size.x, size.y, stroke_removes(io.KeyCtrl), n))
            add_shape(n);
        _tool.draw_overlay(dl, origin);
    }

    // Undo replaces the stencil, and `shapes` with it: it runs after drawing.
    // A new shape may have moved the list, so the pick is looked up again.
    int undo_request = 0;   // 1 undo, 2 redo
    sel = _shape_sel >= 0 && _shape_sel < (int)shapes.size() ? &shapes[(size_t)_shape_sel] : nullptr;
    pen_sel = sel && sel->kind == app::MaskShape::Kind::Bezier;
    if (focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            if (_pen.in_progress()) _pen.cancel();
            else if (_path.in_progress()) _path.cancel();
            else if (_tool.in_progress()) _tool.cancel();
            else if ((pen_mode || _draw == DrawTool::Points) && _shape_sel >= 0) _shape_sel = -1;
            else _draw = DrawTool::Select;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            // A half-drawn polygon or path gives back its last point first.
            if (!io.KeyShift && pen_mode && _pen.in_progress()) _pen.pop_anchor();
            else if (!io.KeyShift && path_mode && _path.in_progress()) _path.pop_anchor();
            else if (!io.KeyShift && _tool.in_progress() && _tool.id() == ToolId::Polygon)
                _tool.pop_point();
            else undo_request = io.KeyShift ? 2 : 1;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) undo_request = 2;
        const bool erase_key = ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
                               ImGui::IsKeyPressed(ImGuiKey_Delete, false);
        if (erase_key && pen_mode && _pen.in_progress()) {
            _pen.pop_anchor();
        } else if (erase_key && _draw == DrawTool::Points && pen_sel && _pt_anchor >= 0 &&
                   _pt_part == PenPart::None) {
            const app::FrameStencil before = stencil;
            if (pen_delete(*sel, _pt_anchor)) {
                _history.push(before);
                _stencil_key.clear();
                _shapes_edited = true;
                edited = true;
                _pt_anchor = -1;
            }
        }
    }

    // Clicks land in source-image pixels, which is what the model wants, so
    // only on the picture. Only with no drawing tool: every other one owns the
    // left button.
    const bool on_picture = mu >= 0.0f && mu < 1.0f && mv >= 0.0f && mv < 1.0f;
    const bool canvas_free = pointer && on_picture && !on_shape && _drag_handle == -1 &&
                             _draw == DrawTool::Select && !tool_consumed &&
                             _model.kind != MaskModelKind::Subject;
    if (!_listing.load() && !_frames.empty() && canvas_free &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        MaskClick c;
        c.x = mu * (float)_tex_w;
        c.y = mv * (float)_tex_h;
        c.positive = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        c.object = settings.current_object;
        c.frame = frame.index;
        c.position = frame.position;
        c.source = _src.input;
        c.camera = shown_camera();
        settings.clicks.push_back(c);
        start_job(settings, shown);
    }
    if (canvas_free)
        ui::SetTooltip(dmsg::click_tooltip, {settings.current_object + 1});

    // Outlines: the exact boundary, over an overlay that is only 512 px wide.
    std::vector<float> fed;
    for (size_t i = 0; i < shapes.size(); i++) {
        const app::MaskShape& s = shapes[i];
        const ImU32 col = (int)i == _shape_sel ? IM_COL32(255, 255, 255, 235)
                                               : IM_COL32(255, 255, 255, 120);
        const float thick = (int)i == _shape_sel ? 2.0f : 1.5f;
        switch (s.kind) {
            case app::MaskShape::Kind::Path:
                for (size_t k = 0; k + 1 < s.pts.size(); k += 2)
                    dl->PathLineTo(to_screen(s.pts[k], s.pts[k + 1]));
                dl->PathStroke(col, ImDrawFlags_Closed, thick);
                break;
            case app::MaskShape::Kind::Stroke:
                // The centre line: the fill under it already shows the width.
                for (size_t k = 0; k + 1 < s.pts.size(); k += 2)
                    dl->PathLineTo(to_screen(s.pts[k], s.pts[k + 1]));
                if (s.pts.size() == 2) dl->AddCircleFilled(to_screen(s.pts[0], s.pts[1]), thick, col);
                else dl->PathStroke(col, 0, thick);
                break;
            case app::MaskShape::Kind::Ellipse:
                dl->AddEllipse(to_screen(s.cx, s.cy),
                               ImVec2(s.rx * size.x, s.ry * size.y), col, 0.0f, 64, thick);
                break;
            case app::MaskShape::Kind::Rect:
                dl->AddRect(to_screen(s.cx, s.cy), to_screen(s.rx, s.ry), col, 0.0f, 0, thick);
                break;
            case app::MaskShape::Kind::Bezier: {
                fed = s.pts;
                for (size_t k = 0; k + 1 < fed.size(); k += 2) {
                    fed[k] *= size.x;
                    fed[k + 1] *= size.y;
                }
                const size_t n = fed.size() / 6;
                mask::draw_bezier(dl, origin, fed.data(), n, /*closed=*/true, col, thick);
                // Its points wherever they can be edited: Points, and the pen.
                if ((int)i == _shape_sel && (_draw == DrawTool::Points || pen_mode))
                    mask::draw_bezier_points(dl, origin, fed.data(), n, _pt_anchor,
                                             /*handles=*/_draw == DrawTool::Points || io.KeyCtrl);
                break;
            }
        }
    }

    const std::string camera = shown_camera();
    for (const MaskClick& c : settings.clicks) {
        if (!mine(c) || c.frame != frame.index || c.camera != camera) continue;
        const ImVec2 p(origin.x + c.x / (float)_tex_w * size.x,
                       origin.y + c.y / (float)_tex_h * size.y);
        const ImU32 col = c.positive ? (ImU32)mask_object_color(c.object)
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
    dl->PopClipRect();
    if (undo_request) undo_shapes(stencil, undo_request == 2, edited);
}

// Anchors and handles of the selected pen shape, all in normalized
// coordinates; Shift's 45 degrees are taken in canvas pixels.
bool SegmentPanel::edit_points(app::FrameStencil& stencil, app::MaskShape& s, float mu,
                               float mv, float cw, float ch, bool hovered, bool& edited) {
    const ImGuiIO& io = ImGui::GetIO();
    if (_pt_part == PenPart::None) {
        const PenHit h = pen_hit(s, mu, mv, cw, ch, 6.0f, /*handles=*/true);
        if (!hovered || h.part == PenPart::None || h.part == PenPart::Segment) return false;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const float* a = &s.pts[6 * (size_t)h.index];
            const int j = h.part == PenPart::In ? 0 : h.part == PenPart::Out ? 4 : 2;
            _pt_part = h.part;
            _pt_anchor = h.index;
            _pt_from[0] = a[j];
            _pt_from[1] = a[j + 1];
            _pt_press[0] = mu;
            _pt_press[1] = mv;
            _pt_moved = false;
            _pt_pull = false;
            _drag_before = stencil;
            _drag_before_key = app::format_mask_shapes(scope(stencil).mask.shapes);
        }
        return true;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float* a = &s.pts[6 * (size_t)_pt_anchor];
        float x = (_pt_from[0] + mu - _pt_press[0]) * cw;
        float y = (_pt_from[1] + mv - _pt_press[1]) * ch;
        _pt_moved = _pt_moved || std::hypot((mu - _pt_press[0]) * cw,
                                            (mv - _pt_press[1]) * ch) >= mask::kPenDragStart;
        if (io.KeyShift) {
            const bool handle = _pt_part != PenPart::Anchor;
            mask::snap_45((handle ? a[2] : _pt_from[0]) * cw, (handle ? a[3] : _pt_from[1]) * ch,
                          x, y);
        }
        if (_pt_pull) {
            // A corner pulled open: symmetric handles, as dragging a new anchor.
            if (_pt_moved) {
                float* w = &s.pts[6 * (size_t)_pt_anchor];
                w[4] = x / cw;
                w[5] = y / ch;
                w[0] = 2.0f * w[2] - w[4];
                w[1] = 2.0f * w[3] - w[5];
            }
        } else if (_pt_moved) {
            pen_move_point(s, _pt_anchor, _pt_part, x / cw, y / ch, cw, ch, io.KeyAlt);
        }
        return true;
    }
    if (_pt_pull && !_pt_moved) pen_make_corner(s, _pt_anchor);
    _pt_part = PenPart::None;
    _pt_pull = false;
    if (_drag_before_key != app::format_mask_shapes(scope(stencil).mask.shapes)) {
        _history.push(_drag_before);
        _stencil_key.clear();
        _shapes_edited = true;
        edited = true;
    }
    return true;
}

// The pen over the picked pen shape, as vector editors have it: on its
// outline it adds an anchor, on an anchor it deletes it, and with Alt it
// turns an anchor into a corner, or pulls new handles out of it.
bool SegmentPanel::pen_on_shape(app::FrameStencil& stencil, app::MaskShape& s, float mu,
                                float mv, float cw, float ch, bool hovered, bool& edited) {
    const ImGuiIO& io = ImGui::GetIO();
    if (!hovered) return false;
    const PenHit h = pen_hit(s, mu, mv, cw, ch, 6.0f, /*handles=*/false);
    if (h.part == PenPart::None) return false;
    const bool alt = io.KeyAlt && h.part == PenPart::Anchor;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 at(ImGui::GetMousePos().x + 10.0f, ImGui::GetMousePos().y + 10.0f);
    const ImU32 badge = IM_COL32(255, 255, 255, 230);
    dl->AddLine(ImVec2(at.x - 4.0f, at.y), ImVec2(at.x + 4.0f, at.y), badge, 1.5f);
    if (h.part == PenPart::Segment)
        dl->AddLine(ImVec2(at.x, at.y - 4.0f), ImVec2(at.x, at.y + 4.0f), badge, 1.5f);
    if (alt) {
        dl->AddLine(ImVec2(at.x - 4.0f, at.y + 3.0f), ImVec2(at.x, at.y - 3.0f), badge, 1.5f);
        dl->AddLine(ImVec2(at.x, at.y - 3.0f), ImVec2(at.x + 4.0f, at.y + 3.0f), badge, 1.5f);
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return true;
    if (alt) {
        // Settled on release by edit_points: a click makes a corner, a drag
        // pulls symmetric handles.
        const float* a = &s.pts[6 * (size_t)h.index];
        _pt_part = PenPart::Out;
        _pt_anchor = h.index;
        _pt_from[0] = a[2];
        _pt_from[1] = a[3];
        _pt_press[0] = mu;
        _pt_press[1] = mv;
        _pt_moved = false;
        _pt_pull = true;
        _drag_before = stencil;
        _drag_before_key = app::format_mask_shapes(scope(stencil).mask.shapes);
        return true;
    }
    const app::FrameStencil before = stencil;
    const bool changed = h.part == PenPart::Segment
                             ? (_pt_anchor = pen_insert(s, h.index, h.t)) >= 0
                             : pen_delete(s, h.index);
    if (h.part == PenPart::Anchor) _pt_anchor = -1;
    if (changed) {
        _history.push(before);
        _stencil_key.clear();
        _shapes_edited = true;
        edited = true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The stencil
// ---------------------------------------------------------------------------

bool SegmentPanel::stroke_removes(bool ctrl) const {
    return !(_subtract != (_draw == DrawTool::Eraser) != ctrl);
}

void SegmentPanel::change_shapes(app::FrameStencil& stencil, bool& edited) {
    _history.push(stencil);
    _stencil_key.clear();
    _shapes_edited = true;
    edited = true;
}

void SegmentPanel::undo_shapes(app::FrameStencil& stencil, bool redo, bool& edited) {
    if (!(redo ? _history.redo(stencil) : _history.undo(stencil))) return;
    _shape_sel = -1;
    _drag_handle = -1;
    _pt_part = PenPart::None;
    _pt_anchor = -1;
    _stencil_key.clear();
    _shapes_edited = true;
    edited = true;
}

// The mask editor's tool row, over the picture: the same tools, keys and
// Add / Subtract, drawing shapes instead of pixels.
void SegmentPanel::draw_tools(app::FrameStencil& stencil, bool& edited) {
    const float w = px(78.0f);
    auto pick = [&](DrawTool t, ToolId id) {
        if (_draw != t || _tool.id() != id) {
            _tool.cancel();
            _path.cancel();
            _pen.cancel();
        }
        _draw = t;
        if (t == DrawTool::Shape || t == DrawTool::Eraser) _tool.set_id(id);
        // The pick carries over between the tools that edit it.
        if (t != DrawTool::Select && t != DrawTool::Points && t != DrawTool::Pen)
            _shape_sel = -1;
        _drag_handle = -1;
        _pt_part = PenPart::None;
    };
    if (ui::KeyButton(dmsg::stencil_tool_select, w, "V", _draw == DrawTool::Select))
        pick(DrawTool::Select, ToolId::Navigate);
    ui::help_on_hover(dmsg::stencil_tool_select_help);
    ImGui::SameLine();
    if (ui::KeyButton(dmsg::stencil_tool_points, w, "A", _draw == DrawTool::Points))
        pick(DrawTool::Points, ToolId::Navigate);
    ui::help_on_hover(dmsg::stencil_tool_points_help);
    // The pen takes the polygon's place and its P: clicked, it draws one.
    for (int i = (int)ToolId::Box; i <= (int)ToolId::Brush; i++) {
        const ToolRow& row = tool_table()[i];
        ImGui::SameLine();
        if (row.id == ToolId::Polygon) {
            if (ui::KeyButton(mmsg::tool_pen, w, "P", _draw == DrawTool::Pen))
                pick(DrawTool::Pen, ToolId::Navigate);
            ui::help_on_hover(dmsg::stencil_tool_pen_help);
            continue;
        }
        if (ui::KeyButton(tool_label(row.id), w, row.key,
                          _draw == DrawTool::Shape && _tool.id() == row.id))
            pick(DrawTool::Shape, row.id);
    }
    ImGui::SameLine();
    if (ui::KeyButton(mmsg::tool_eraser, w, "X", _draw == DrawTool::Eraser))
        pick(DrawTool::Eraser, ToolId::Brush);
    ImGui::SameLine();
    if (ui::KeyButton(mmsg::tool_path, w, "I", _draw == DrawTool::Path))
        pick(DrawTool::Path, ToolId::Navigate);
    ui::help_on_hover(dmsg::stencil_add_path_help);

    if (ui::RadioButton(mmsg::mode_add, !_subtract)) _subtract = false;
    ui::help_on_hover(mmsg::mode_help);
    ImGui::SameLine();
    if (ui::RadioButton(mmsg::mode_subtract, _subtract)) _subtract = true;
    ui::help_on_hover(mmsg::mode_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(!_history.can_undo());
    if (ui::Button(mmsg::undo)) undo_shapes(stencil, false, edited);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!_history.can_redo());
    if (ui::Button(mmsg::redo)) undo_shapes(stencil, true, edited);
    ImGui::EndDisabled();
    const bool brush = _draw == DrawTool::Eraser ||
                       (_draw == DrawTool::Shape && _tool.id() == ToolId::Brush);
    if (brush) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(180.0f));
        // The message's own '%' is literal to ImGui's printf, so it is doubled.
        std::string fmt;
        for (char c : spirula::i18n::format(dmsg::stencil_brush_size, {"\x01"}))
            fmt += c == '%' ? "%%" : c == '\x01' ? "%.1f" : std::string(1, c);
        ui::SliderFloatRaw("##stencilbrush", &_brush_pct, 0.2f, 25.0f, fmt.c_str(),
                           ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        ui::help_on_hover(dmsg::stencil_brush_size_help);
    }
    if (_draw == DrawTool::Pen && _pen.in_progress()) {
        ImGui::SameLine();
        ui::TextDisabled(mmsg::pen_anchors, {_pen.anchor_count()});
    }
    if (_draw == DrawTool::Path) {
        ImGui::SameLine();
        ui::TextDisabled(dmsg::stencil_path_anchors, {_path.anchor_count()});
        if (_livewiring.load()) {
            ImGui::SameLine();
            ui::TextDisabled(mmsg::path_building);
        } else if (!_path.snapping()) {
            ImGui::SameLine();
            ui::TextDisabled(mmsg::path_straight);
        }
    }
    ui::TextDisabledWrapped(mmsg::hint_view);

    // The keys, while nothing is being typed and no stroke is half drawn.
    const ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || io.WantTextInput ||
        io.KeyCtrl || _tool.in_progress() || _path.in_progress() || _pen.in_progress() ||
        _pt_part != PenPart::None)
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) pick(DrawTool::Select, ToolId::Navigate);
    if (ImGui::IsKeyPressed(ImGuiKey_A, false)) pick(DrawTool::Points, ToolId::Navigate);
    if (ImGui::IsKeyPressed(ImGuiKey_P, false)) pick(DrawTool::Pen, ToolId::Navigate);
    for (int i = (int)ToolId::Box; i <= (int)ToolId::Brush; i++) {
        const ToolRow& row = tool_table()[i];
        if (row.id == ToolId::Polygon) continue;
        if (ImGui::IsKeyPressed((ImGuiKey)row.imgui_key, false)) pick(DrawTool::Shape, row.id);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) pick(DrawTool::Eraser, ToolId::Brush);
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) pick(DrawTool::Path, ToolId::Navigate);
    if (brush && ImGui::IsKeyPressed(ImGuiKey_LeftBracket, true))
        _brush_pct = std::max(0.2f, _brush_pct / 1.25f);
    if (brush && ImGui::IsKeyPressed(ImGuiKey_RightBracket, true))
        _brush_pct = std::min(25.0f, _brush_pct * 1.25f);
}

// One list goes where the tools draw; a per-camera set goes on the cameras it
// names, with separate areas turned on, and refuses an input that has none.
void SegmentPanel::load_file(app::FrameStencil& s, const std::string& path) {
    app::MaskSet set;
    std::string err;
    if (!load_stencil_preset(path, set, err)) {
        _saved_msg = spirula::i18n::format(dmsg::stencil_load_failed, {err});
        _saved_msg_err = true;
        return;
    }
    if (set.per_camera() && !_cameras.empty()) {
        std::string names;
        bool any = false;
        for (const auto& [camera, shapes] : set.cameras) {
            names += (names.empty() ? "" : ", ") + camera;
            any = any || std::find(_cameras.begin(), _cameras.end(), camera) != _cameras.end();
        }
        if (!any) {
            _saved_msg = spirula::i18n::format(dmsg::stencil_set_no_camera, {names});
            _saved_msg_err = true;
            return;
        }
    }
    bool edited = false;
    change_shapes(s, edited);
    if (set.per_camera()) {
        if (!s.per_camera() && _cameras.size() > 1) set_per_camera(s, true);
        app::apply_mask_set(s, set);
    } else {
        scope(s).mask.shapes = std::move(set.shapes);
    }
    _shape_sel = -1;
    _drag_handle = -1;
    _saved_msg.clear();
    _needs_run = true;
}

void SegmentPanel::draw_saved_areas(app::FrameStencil& s) {
    if (ui::SmallButton(dmsg::stencil_load)) {
        _saved = list_stencil_presets();
        _in_dataset = list_dataset_stencils(_workspace);
        ImGui::OpenPopup("##stencilload");
    }
    if (ImGui::BeginPopup("##stencilload")) {
        ui::SeparatorText(dmsg::stencil_saved_areas);
        for (const StencilPreset& p : _saved)
            if (ui::SelectableRaw(stencil_preset_label(p) + "##" + p.path, false))
                load_file(s, p.path);
        // Kept by earlier runs into this output folder, saved or not.
        if (!_in_dataset.empty()) {
            ui::SeparatorText(dmsg::stencil_in_dataset);
            for (const StencilPreset& p : _in_dataset)
                if (ui::SelectableRaw(stencil_preset_label(p) + "##ds" + p.path, false))
                    load_file(s, p.path);
        }
        ImGui::Separator();
        if (ui::Selectable(dmsg::stencil_other_file)) _browse_requested = true;
        ImGui::EndPopup();
    }
    // With separate areas on, the set is the whole input's: a file per camera
    // when the cameras differ.
    const app::MaskSet to_save =
        s.per_camera() ? app::mask_set_of(s) : app::MaskSet{scope(s).mask.shapes, {}};
    ImGui::SameLine();
    ImGui::BeginDisabled(to_save.empty());
    if (ui::SmallButton(dmsg::stencil_save)) ImGui::OpenPopup("##stencilsave");
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(dmsg::stencil_save_help);
    if (ImGui::BeginPopup("##stencilsave")) {
        ImGui::SetNextItemWidth(px(260.0f));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ui::InputTextWithHint(gmsg::preset_name, gmsg::preset_name_hint,
                                                 &_save_name,
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::BeginDisabled(_save_name.empty());
        if ((ui::Button(mmsg::save) || enter) && !_save_name.empty()) {
            std::string path, err;
            if (save_stencil_preset(_save_name, to_save, path, err)) {
                _saved_msg = spirula::i18n::format(dmsg::stencil_saved_as, {path});
                _saved_msg_err = false;
            } else {
                _saved_msg = spirula::i18n::format(dmsg::stencil_save_failed, {err});
                _saved_msg_err = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ui::Button(dmsg::cancel)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ui::help_on_hover(dmsg::stencil_saved_areas);
    if (!_saved_msg.empty()) {
        if (_saved_msg_err)
            ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _saved_msg);
        else
            ui::TextColoredWrappedRaw(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], _saved_msg);
    }
}

void SegmentPanel::draw_stencil(app::FrameStencil& s, bool& edited) {
    ui::Text(dmsg::stencil_section);
    ui::help_on_hover(dmsg::stencil_section_help);

    // What is edited below: the input's, or the shown camera's own.
    app::CameraStencil* cs = &scope(s);
    const bool several = _cameras.size() > 1;
    if (several || s.per_camera()) {
        bool per = s.per_camera();
        if (ui::Checkbox(dmsg::stencil_per_camera, &per)) {
            // Undoable, but no drawn shape changes: a named set stays named.
            _history.push(s);
            edited = true;
            set_per_camera(s, per);
            cs = &scope(s);
        }
        ui::help_on_hover(dmsg::stencil_per_camera_help);
        if (s.per_camera()) {
            const std::string cam = shown_camera();
            ui::TextDisabled(dmsg::stencil_per_camera_editing, {cam.empty() ? std::string("/") : cam});
        }
    }

    bool detect = cs->detect_border;
    if (ui::Checkbox(dmsg::stencil_border, &detect)) {
        _history.push(s);
        _stencil_key.clear();
        edited = true;
        cs->detect_border = detect;
    }
    ui::help_on_hover(dmsg::stencil_border_help);
    if (cs->detect_border) {
        // Lazily, here rather than off the checkbox: the option is on already
        // when the panel opens on an input that was set up before.
        if (!_listing.load() && !_detect_asked) start_detect();
        // The slider walks into other camera folders, whose circle is not this
        // one's; refit rather than draw the wrong circle over the frame.
        else if (!_detecting.load() && !_frames.empty() &&
                 shown_camera() != _border_camera)
            start_detect();
        ImGui::Indent();
        // One undo step per drag, taken from the stencil as the drag found it.
        const app::FrameStencil before = s;
        float pct = cs->shrink * 100.0f;
        ImGui::SetNextItemWidth(-1);
        if (ui::SliderFloatRaw("##shrink", &pct, -10.0f, 30.0f, "%.1f%%",
                               ImGuiSliderFlags_AlwaysClamp)) {
            cs->shrink = pct / 100.0f;
            _stencil_key.clear();
            edited = true;
        }
        if (ImGui::IsItemActivated()) _history.push(before);
        ui::help_on_hover(dmsg::stencil_shrink_help);
        if (_detecting.load())
            ui::TextDisabled(dmsg::stencil_looking);
        else if (_detect_asked && !_border.found)
            ui::TextColoredWrapped(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                                   dmsg::stencil_border_none);
        if (!_detecting.load() && !_listing.load() &&
            ui::SmallButton(dmsg::stencil_look_again))
            start_detect();
        if (!_busy.load() && !_detecting.load() && !_listing.load() &&
            _border.found && shown_camera() == _border_camera) {
            if (ui::SmallButton(dmsg::stencil_edit_border)) {
                // The ellipse fits one lens: on a many-camera input it is that
                // camera's, and the others keep their own fits.
                change_shapes(s, edited);
                if (several && !s.per_camera()) set_per_camera(s, true);
                cs = &scope(s);
                if (app::edit_detected_border(*cs, _border)) {
                    _shape_sel = 0;
                    _drag_handle = -1;
                    _draw = DrawTool::Select;
                    _tool.cancel();
                    _path.cancel();
                    _pen.cancel();
                }
            }
            ui::help_on_hover(dmsg::stencil_edit_border_help);
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ui::Text(dmsg::stencil_shapes);
    ui::help_on_hover(dmsg::stencil_shapes_help);
    draw_saved_areas(s);

    std::vector<app::MaskShape>& shapes = scope(s).mask.shapes;
    for (size_t i = 0; i < shapes.size(); i++) {
        app::MaskShape& sh = shapes[i];
        ImGui::PushID((int)i);
        const spirula::i18n::Msg& kind_label =
            sh.kind == app::MaskShape::Kind::Rect      ? dmsg::stencil_shape_box
            : sh.kind == app::MaskShape::Kind::Path    ? dmsg::stencil_shape_path
            : sh.kind == app::MaskShape::Kind::Stroke  ? dmsg::stencil_shape_stroke
            : sh.kind == app::MaskShape::Kind::Bezier  ? dmsg::stencil_shape_curve
                                                       : dmsg::stencil_shape_circle;
        const std::string label = spirula::i18n::format(kind_label, {(int)i + 1});
        // Toggling, not a plain radio: the selected shape swallows clicks on
        // the picture, so there has to be a way to let go of it again.
        const bool sel = _shape_sel == (int)i;
        if (ui::RadioButtonRaw(label.c_str(), sel)) {
            _shape_sel = sel ? -1 : (int)i;
            _drag_handle = -1;
            _pt_part = PenPart::None;
            _pt_anchor = -1;
            // Moving it is the Select tool's; picking it is asking for that.
            // The pen and Points edit it where they are.
            if (_draw != DrawTool::Points && _draw != DrawTool::Pen) _draw = DrawTool::Select;
            _tool.cancel();
            _path.cancel();
            _pen.cancel();
        }
        ImGui::SameLine();
        if (ui::SmallButton(sh.remove ? dmsg::stencil_removes_inside
                                      : dmsg::stencil_keeps_inside)) {
            change_shapes(s, edited);
            shapes[i].remove = !shapes[i].remove;
        }
        ui::help_on_hover(dmsg::stencil_flip_help);
        ImGui::SameLine();
        if (ui::SmallButton(dmsg::remove)) {
            change_shapes(s, edited);
            shapes.erase(shapes.begin() + (long)i);
            _shape_sel = -1;
            _drag_handle = -1;
            _pt_part = PenPart::None;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (_shape_sel >= 0 && _shape_sel < (int)shapes.size()) {
        const bool pen_shape = shapes[(size_t)_shape_sel].kind == app::MaskShape::Kind::Bezier;
        if (pen_shape && _draw == DrawTool::Points) ui::TextDisabledWrapped(dmsg::stencil_points_hint);
        else if (pen_shape) ui::TextDisabledWrapped(dmsg::stencil_drag_hint_pen);
        else ui::TextDisabledWrapped(dmsg::stencil_drag_hint);
    }
}

// ---------------------------------------------------------------------------
// The object list
// ---------------------------------------------------------------------------

void SegmentPanel::draw_objects(MaskSettings& settings, bool& edited) {
    const long long cur = _frames.empty() ? 0 : _frames[(size_t)_frame_idx].index;
    draw_mask_objects(settings, cur, shown_camera(), _src.input, edited);
}

void SegmentPanel::draw(MaskSettings& settings, app::FrameStencil& stencil) {
    if (!_open) return;

    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_frames_ready) {
            _src = std::move(_listed_src);
            _frames = std::move(_frames_pending);
            _all_files = std::move(_all_files_pending);
            _folders = std::move(_folders_pending);
            _frames_ready = false;
            _cameras = camera_names();
        }
    }
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
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_livewire_ready) {
            _livewire_ready = false;
            _livewire = std::move(_livewire_pending);
            _livewire_key = _livewire_pending_key;
            _path.set_livewire(_livewire.get());
            char ms[32];
            std::snprintf(ms, sizeof ms, "%.0f", _livewire_ms);
            _status = spirula::i18n::format(mmsg::path_edge_map,
                                            {_livewire->width(), _livewire->height(),
                                             _livewire->step(), std::string(ms)});
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
    if (_model.text && _model.kind != MaskModelKind::Subject && !_model.empty() &&
        !settings.feature_prompt.empty())
        ui::TextDisabled(dmsg::preview_legend_features);
    ImGui::Separator();

    // ---- controls ----
    const float panel_w = 360.0f;
    ImGui::BeginChild("##segctl", ImVec2(panel_w, 0), ImGuiChildFlags_Borders);

    bool edited = false;

#ifndef SS_BUILD_SAM
    // No segmentation in this build, so the half of the panel that prompts a
    // model would be a row of dead controls. The shapes below need none.
    ui::TextWrapped(dmsg::mask_objects_need_segmentation);
    ui::help_on_hover_raw(backends().masking_reason.c_str());
#else
    // Polarity first, and the two fields below it are labelled by it: the same
    // box means "take this out" or "this is the subject" depending on the
    // radio, and a label that does not follow the switch reads as a bug.
    // Stacked, not side by side: at this panel width the second label clips.
    int polarity = settings.keep_subject ? 1 : 0;
    const bool subject = _model.kind == MaskModelKind::Subject;
    if (subject) {
        if (ui::RadioButton(dmsg::mask_subject_keep, polarity == 1)) polarity = 1;
        if (ui::RadioButton(dmsg::mask_subject_remove, polarity == 0)) polarity = 0;
    } else {
        if (ui::RadioButton(dmsg::mask_remove_named, polarity == 0)) polarity = 0;
        if (ui::RadioButton(dmsg::mask_keep_named, polarity == 1)) polarity = 1;
    }
    if ((polarity == 1) != settings.keep_subject) {
        settings.keep_subject = polarity == 1;
        edited = true;
    }
    ui::help_on_hover(subject ? dmsg::mask_subject_polarity_help : dmsg::preview_polarity_help);
    const bool keep = settings.keep_subject;

    // BiRefNet reads no prompt and takes no click: what is left of this half
    // of the panel is the margin.
    if (subject) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ui::TextDisabled(dmsg::mask_subject_note);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        edited |= draw_margin_slider(settings.dilate_ratio, settings.shrink_ratio, keep, -1.0f,
                                     /*inline_label=*/false);
    } else {

    // English, whatever the interface language is -- see MaskPrompt.h. A pick
    // that reads no words gets the clicks alone.
    if (!_model.text) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ui::TextDisabled(dmsg::preview_clicks_only);
        ImGui::PopTextWrapPos();
    } else {
    ImGui::Spacing();
    ui::Text(keep ? dmsg::preview_what_kept : dmsg::preview_what_removed);
    ImGui::SetNextItemWidth(-1);
    edited |= ui::InputTextEnglishRaw(
        "##prompt",
        keep ? "the statue; its pedestal" : "tourist; visitor; person; car; shadow of a person",
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
    }

    // Under the prompts because it is a property of what they matched, and it
    // moves the red on the picture: the outlines come back tight against the
    // object and leave a rim of its colour that would be reconstructed.
    ImGui::Spacing();
    edited |= draw_margin_slider(settings.dilate_ratio, settings.shrink_ratio, keep, -1.0f,
                                 /*inline_label=*/false);

    // Below the margin, which does not apply to it.
    if (_model.text) {
        ImGui::Spacing();
        ui::Text(dmsg::mask_features_only);
        ImGui::SetNextItemWidth(-1);
        edited |= ui::InputTextEnglishRaw("##featprompt", "sky; cloud",
                                          &settings.feature_prompt);
        ui::help_on_hover(dmsg::preview_features_only_help);
    }

    ImGui::Spacing();
    ImGui::Separator();
    draw_objects(settings, edited);
    }
#endif

    ImGui::Spacing();
    ImGui::Separator();
    draw_stencil(stencil, edited);

    // ---- which camera, then which frame ----
    if (_folders.size() > 1) {
        ImGui::Spacing();
        ui::Text(dmsg::preview_camera);
        ImGui::SetNextItemWidth(-1);
        if (ui::BeginComboRaw("##camera",
                              _folders[(size_t)_folder_idx].c_str())) {
            for (size_t i = 0; i < _folders.size(); i++)
                if (ui::SelectableRaw(_folders[i], (int)i == _folder_idx)) {
                    _folder_idx = (int)i;
                    _frame_dirty = true;
                    _needs_run = true;
                }
            ImGui::EndCombo();
        }
        ui::help_on_hover(dmsg::preview_camera_help);
    }
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
    if (!_listing.load() && _needs_run && !_busy.load() && !_detecting.load() &&
        _drag_handle == -1 && !ImGui::IsAnyItemActive()) {
        _needs_run = false;
        start_job(settings, resolved(stencil));
    }

    ImGui::Spacing();
    float kept_fraction = -1.0f, feature_fraction = -1.0f;
    {
        std::lock_guard<std::mutex> lk(_mu);
        // Segmentation errors and progress come from the inference layer.
        if (!_error.empty())
            ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _error);
        else if (!_status.empty())
            ui::TextDisabledRaw(_status);
        kept_fraction = _kept_fraction;
        feature_fraction = _feature_fraction;
    }
    if (kept_fraction >= 0.0f) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f", 100.0f * kept_fraction);
        ui::Text(dmsg::preview_kept_fraction, {pct});
        if (feature_fraction >= 0.0f) {
            std::snprintf(pct, sizeof pct, "%.0f", 100.0f * feature_fraction);
            ui::Text(dmsg::preview_features_kept_fraction, {pct});
        }
        if (kept_fraction < 0.05f)
            ui::TextColoredWrapped(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                                   settings.keep_subject
                                       ? dmsg::preview_almost_nothing_kept
                                       : dmsg::preview_almost_all_masked);
    }

    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##segimg", ImVec2(0, 0));
    bool canvas_edited = false;
    draw_tools(stencil, canvas_edited);
    draw_image(settings, stencil, canvas_edited);
    if (canvas_edited) _needs_run = true;
    ImGui::EndChild();

    ImGui::End();
    if (!open) close();
}

}  // namespace gui
