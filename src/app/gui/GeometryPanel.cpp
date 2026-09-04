// GeometryPanel.cpp -- see GeometryPanel.h.

#include "app/gui/GeometryPanel.h"

#include "app/DepthColor.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/Layout.h"
#include "app/gui/SfmRunner.h"     // sfm_model_is_fisheye
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Log.h"

#include "imgui.h"

#ifdef SS_TOOL_GEOMETRY
#include "app/GeometryWarp.h"
#include "data/CameraMath.h"
#include "data/DatasetParser.h"
#include "data/ImageProbe.h"
#include "app/GeometryModel.h"
#include "nn/core/Log.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dmsg = spirula::i18n::msg::dataset;
namespace lmsg = spirula::i18n::msg::log;

namespace gui {

// One picture the panel can show, already 8-bit RGB.
struct GeometryPanel::Rgb {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    bool empty() const { return px.empty(); }
};

namespace {

std::unique_ptr<GeometryPanel::Rgb> make_rgb(int w, int h) {
    auto r = std::make_unique<GeometryPanel::Rgb>();
    r->w = w;
    r->h = h;
    r->px.assign((size_t)w * h * 3, 0);
    return r;
}

std::string human_time(double ms) {
    char buf[64];
    if (ms < 60000) std::snprintf(buf, sizeof buf, "%.0fs", ms / 1000.0);
    else if (ms < 3600000) std::snprintf(buf, sizeof buf, "%.0fm", ms / 60000.0);
    else std::snprintf(buf, sizeof buf, "%.1fh", ms / 3600000.0);
    return buf;
}

#ifdef SS_TOOL_GEOMETRY

// The name the picker shows for a lens, so the sentence about it points at a
// control the reader is looking at rather than at a flag value.
std::string lens_label(const std::string& lens) {
    const auto labels = sfm_camera_model_labels();
    for (int i = 0; i < kNumSfmCameraModels; i++)
        if (lens == kSfmCameraModels[i]) return labels[(size_t)i]->get();
    return lens;
}

// The lens the screen names, as a camera. Distortion is left at zero -- it is
// not known before the reconstruction, and what the preview needs it for is
// the FIELD OF VIEW, which is what decides the split into faces.
app::GeometryCamera lens_camera(const std::string& lens, float focal_factor,
                                int w, int h) {
    app::GeometryCamera cam;
    cam.width = w;
    cam.height = h;
    cam.cx = 0.5f * (float)w;
    cam.cy = 0.5f * (float)h;
    cam.distortion = (int)CameraDistortionType::None;
    if (lens == "equirectangular") {
        cam.model = (int)CameraModelType::EQUIRECTANGULAR;
        cam.fx = cam.fy = (float)(w / 6.28318530718);
    } else if (sfm_model_is_fisheye(lens)) {
        cam.model = (int)CameraModelType::FISHEYE;
        // An equidistant circle filling the short side over 180 degrees, which
        // is what an uncalibrated fisheye usually is.
        cam.fx = cam.fy = focal_factor > 0.0f
                              ? focal_factor * (float)w
                              : (float)(std::min(w, h) / 3.14159265358979);
    } else {
        cam.model = (int)CameraModelType::PINHOLE;
        cam.fx = cam.fy = focal_factor > 0.0f ? focal_factor * (float)w
                                              : 1.2f * (float)std::max(w, h);
    }
    return cam;
}

// One camera of a parsed dataset, as GeometryWarp takes it. Same fields
// `spirula geometry` reads, including the fitted-lens fallback.
app::GeometryCamera dataset_camera(const ParsedDataset& ds, int64_t i) {
    app::GeometryCamera cam;
    cam.model = ds.camera_models[(size_t)i];
    cam.distortion = ds.camera_distortions[(size_t)i];
    cam.width = ds.widths[(size_t)i];
    cam.height = ds.heights[(size_t)i];
    cam.fx = ds.intrins[(size_t)i * 4 + 0];
    cam.fy = ds.intrins[(size_t)i * 4 + 1];
    cam.cx = ds.intrins[(size_t)i * 4 + 2];
    cam.cy = ds.intrins[(size_t)i * 4 + 3];
    for (int j = 0; j < kCameraDistortionParams; ++j)
        cam.dist[j] = ds.dist_coeffs[(size_t)i * kCameraDistortionParams + j];
    if (!ds.redistort.empty() && ds.redistort[(size_t)i].source_model >= 0) {
        cam.source_model = ds.redistort[(size_t)i].source_model;
        std::copy(std::begin(ds.redistort[(size_t)i].params),
                  std::end(ds.redistort[(size_t)i].params),
                  std::begin(cam.source_params));
    }
    return cam;
}

std::unique_ptr<GeometryPanel::Rgb> depth_picture(const std::vector<float>& d,
                                                  int w, int h) {
    auto out = make_rgb(w, h);
    app::depth_to_rgb(d.data(), std::min(d.size(), out->px.size() / 3),
                      /*skip_zero=*/true, out->px.data());
    return out;
}

std::unique_ptr<GeometryPanel::Rgb> normal_picture(const std::vector<float>& n,
                                                   int w, int h) {
    auto out = make_rgb(w, h);
    app::normal_to_rgb(n.data(), std::min(n.size() / 3, out->px.size() / 3),
                       out->px.data());
    return out;
}

#endif  // SS_TOOL_GEOMETRY

}  // namespace

// What survives between runs: the weights, which are seconds to upload, and
// the frame the last run decoded.
struct GeometryPanel::Job {
    std::string frame_key;
    Rgb frame;
    // The source as collect_preview_frames left it -- it fills in how long a
    // video is, which the ffmpeg path seeks by.
    PreviewSource src;
    bool listed = false;
#ifdef SS_TOOL_GEOMETRY
    app::GeometryModel pred;
    std::string loaded_model;
    // The dataset's own cameras, when the output folder holds one. Parallel to
    // the frame list the panel offers.
    std::vector<app::GeometryCamera> cams;
    int64_t dataset_images = 0;
#endif
};

GeometryPanel::GeometryPanel() = default;

GeometryPanel::~GeometryPanel() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
}

void GeometryPanel::open(const std::string& input, bool is_video,
                         const std::string& dataset,
                         const std::string& image_dir, const std::string& lens,
                         float focal_factor, const std::string& ffmpeg_exe,
                         bool force_ffmpeg) {
    // A job still running belongs to the old input; only the weights survive.
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    if (_job) {
        _job->listed = false;
        _job->frame_key.clear();
        _job->frame = Rgb{};
#ifdef SS_TOOL_GEOMETRY
        _job->cams.clear();
        _job->dataset_images = 0;
#endif
    }
    _src = PreviewSource{};
    _src.input = input;
    _src.is_video = is_video;
    _src.ffmpeg_exe = ffmpeg_exe.empty() ? "ffmpeg" : ffmpeg_exe;
    _src.builtin_decode = !force_ffmpeg && backends().builtin_video;
    _dataset = dataset;
    _image_dir = image_dir;
    _lens = lens;
    _focal_factor = focal_factor;
    _frame_idx = 0;
    _frame_dirty = true;
    _needs_run = true;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _frames.clear();
        _frames_ready = false;
        _status.clear();
        _error.clear();
        _source_note.clear();
        _cost_note.clear();
        for (auto& s : _shot) s.reset();
        _shot_dirty = true;
    }
    _open = true;
}

void GeometryPanel::close() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    // Drop the weights for real: a vit-large is 784 MB on the device and a
    // reconstruction or a training run is usually what comes next.
    _job.reset();
    _open = false;
}

void GeometryPanel::destroy_gl() {
    for (int i = 0; i < 3; i++)
        if (_tex[i]) {
            glDeleteTextures(1, &_tex[i]);
            _tex[i] = 0;
        }
}

// ---------------------------------------------------------------------------
// The worker
// ---------------------------------------------------------------------------

void GeometryPanel::start_job(const GeometryJob& settings) {
    if (_busy.load()) return;
    if (_worker.joinable()) _worker.join();

    const int idx = _frame_idx;
    const PreviewSource src = _src;
    const std::string dataset = _dataset;
    const std::string image_dir = _image_dir;
    const std::string lens = _lens;
    const float focal_factor = _focal_factor;
    const bool frame_dirty = _frame_dirty;
    _frame_dirty = false;

    _busy = true;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _status = dmsg::preview_working.get();
    }

    _worker = std::thread([this, settings, src, dataset, image_dir, lens,
                           focal_factor, idx, frame_dirty] {
        // Every failure below leaves through `return set_error(...)`, so the
        // flag cannot be cleared at the end of the function.
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

#ifndef SS_TOOL_GEOMETRY
        (void)settings; (void)src; (void)dataset; (void)image_dir; (void)lens;
        (void)focal_factor; (void)idx; (void)frame_dirty;
        set_error(lmsg::err_no_geometry_module.get());
#else
        try {
            if (!_job) _job = std::make_unique<Job>();
            Job& j = *_job;

            // ---- what there is to look at ----
            if (!j.listed) {
                set_status(dmsg::geom_preview_reading);
                j.src = src;
                std::vector<PreviewFrame> frames;
                std::string note;
                // A dataset in the output folder is the exact answer, and the
                // only one: the input's frames have no intrinsics until
                // something has reconstructed them.
                if (!dataset.empty() && folder_looks_like_dataset(dataset)) {
                    try {
                        DatasetParserConfig cfg;
                        cfg.require_image_files = false;
                        cfg.probe_image_size = probe_image_size;
                        if (!image_dir.empty()) cfg.image_dir = image_dir;
                        const ParsedDataset ds = parse_dataset(dataset, cfg, "");
                        const int64_t total = ds.num_cameras;
                        const int64_t offers = std::min<int64_t>(total, 12);
                        for (int64_t k = 0; k < offers; k++) {
                            const int64_t i =
                                offers > 1 ? k * (total - 1) / (offers - 1) : 0;
                            PreviewFrame f;
                            f.path = ds.image_filenames[(size_t)i];
                            f.index = i;
                            f.position = total > 1 ? (float)i / (float)(total - 1) : 0.0f;
                            frames.push_back(f);
                            j.cams.push_back(dataset_camera(ds, i));
                        }
                        j.dataset_images = total;
                        note = spirula::i18n::format(dmsg::geom_preview_from_dataset,
                                                     {(long long)total});
                    } catch (const std::exception&) {
                        frames.clear();
                        j.cams.clear();
                        j.dataset_images = 0;
                    }
                }
                if (frames.empty()) {
                    std::vector<std::string> all;
                    collect_preview_frames(j.src, src.is_video ? 8 : 12, frames,
                                           all, _cancel);
                    note = spirula::i18n::format(dmsg::geom_preview_assumed_lens,
                                                 {lens_label(lens)});
                }
                if (_cancel.load()) return;
                j.listed = true;
                std::lock_guard<std::mutex> lk(_mu);
                _frames = std::move(frames);
                _frames_ready = true;
                _source_note = note;
            }

            PreviewFrame frame;
            app::GeometryCamera cam;
            bool have_cam = false;
            {
                std::lock_guard<std::mutex> lk(_mu);
                if (_frames.empty()) {
                    _status.clear();
                    _error = dmsg::geom_preview_nothing.get();
                    return;
                }
                const size_t pick = std::min((size_t)std::max(idx, 0),
                                             _frames.size() - 1);
                frame = _frames[pick];
                if (pick < j.cams.size()) {
                    cam = j.cams[pick];
                    have_cam = true;
                }
            }

            // ---- the frame ----
            const std::string key = frame.path.empty()
                                        ? ("#" + std::to_string(idx))
                                        : frame.path;
            if (frame_dirty || j.frame_key != key || j.frame.empty()) {
                set_status(dmsg::preview_loading_frame);
                Rgb img;
                std::string err;
                PreviewSource load_src = j.src;
                // A dataset frame is a file on disk whatever the input was.
                if (!frame.path.empty()) load_src.is_video = false;
                if (!load_preview_frame(load_src, frame, img.w, img.h, img.px, err,
                                        _cancel)) {
                    if (_cancel.load()) return;
                    return set_error(err);
                }
                j.frame = std::move(img);
                j.frame_key = key;
                std::lock_guard<std::mutex> lk(_mu);
                _shot[0] = make_rgb(j.frame.w, j.frame.h);
                _shot[0]->px = j.frame.px;
                _shot[1].reset();
                _shot[2].reset();
                _shot_dirty = true;
            }
            if (_cancel.load()) return;
            if (!have_cam)
                cam = lens_camera(lens, focal_factor, j.frame.w, j.frame.h);

            // ---- the model ----
            // Before the warp: which input sizes round-trip is a property of
            // the network, and the run loads it in the same order.
            const bool just_loaded = j.loaded_model != settings.model;
            if (just_loaded && !geometry_model_cached(settings.model))
                return set_error(lmsg::err_geometry_model_not_downloaded.get());
            if (just_loaded) {
                set_status(dmsg::preview_loading_model);
                j.loaded_model.clear();
                j.pred.load(settings.model);
                j.loaded_model = settings.model;
            }
            if (_cancel.load()) return;

            // Planned exactly as the run plans it.
            const int patch = j.pred.sizeGranularity();
            double s = 1.0;
            if (settings.max_size > 0 &&
                std::max(cam.width, cam.height) > settings.max_size)
                s = (double)settings.max_size / std::max(cam.width, cam.height);
            const int ow = std::max(patch, (int)(cam.width * s) / patch * patch);
            const int oh = std::max(patch, (int)(cam.height * s) / patch * patch);
            const bool split =
                settings.split == 0
                    ? camhost::splits_to_pinhole_faces(cam.model, cam.width,
                                                       cam.height, cam.fx, cam.fy)
                    : settings.split == 1;
            app::GeometryWarp warp;
            warp.plan(cam, ow, oh, split, patch, settings.max_size);

            // ---- run ----
            set_status(dmsg::geom_preview_running);
            const std::vector<float> rgb =
                app::resize_area(j.frame.px.data(), j.frame.w, j.frame.h, 3,
                                 warp.sampleWidth(), warp.sampleHeight());
            std::vector<std::vector<float>> face_depth, face_normal;
            std::vector<float> face_rgb;
            // The first call after a load builds the pipelines and measures the
            // GEMM tiling, so timing it reports about 3x the steady-state cost
            // -- and that number is what the whole-capture estimate is made of.
            if (just_loaded) {
                warp.sampleFace(0, rgb.data(), face_rgb);
                app::GeometryRequest warm =
                    face_request(warp, 0, settings.num_tokens);
                warm.want_normal = false;
                j.pred.predict(face_rgb.data(), warm);
                if (_cancel.load()) return;
            }
            const double t0 = nn::now_ms();
            for (int k = 0; k < warp.faces(); k++) {
                if (_cancel.load()) return;
                warp.sampleFace(k, rgb.data(), face_rgb);
                // Both, whatever the checkboxes say: they decide what the
                // RUN writes, and a pane that appears without another forward
                // pass is worth the decoder head.
                app::GeometryPrediction p = j.pred.predict(
                    face_rgb.data(), face_request(warp, k, settings.num_tokens));
                // One unit across faces before they are blended: Metric3D's
                // depth is canonical to the face's focal.
                const float mm = (float)j.pred.depthToMillimetres(warp.faceFocal(k));
                for (float& d : p.depth) d *= mm;
                face_depth.push_back(std::move(p.depth));
                face_normal.push_back(std::move(p.normal));
            }
            const double each = nn::now_ms() - t0;
            std::vector<float> depth, normal;
            const bool ray = settings.ray_depth == 0 ? warp.defaultRayDepth()
                                                     : settings.ray_depth == 1;
            warp.gather(face_depth, face_normal, ray, &depth, &normal);

            auto normals = normal_picture(normal, warp.outWidth(), warp.outHeight());
            auto depths = depth_picture(depth, warp.outWidth(), warp.outHeight());

            std::string cost = spirula::i18n::format(
                dmsg::geom_preview_cost,
                {(long long)warp.faces(), (long long)warp.outWidth(),
                 (long long)warp.outHeight(), (long long)std::lround(each)});
            if (j.dataset_images > 0)
                cost += "  " + spirula::i18n::format(
                                   dmsg::geom_preview_total,
                                   {(long long)j.dataset_images,
                                    human_time(each * (double)j.dataset_images)});
            std::lock_guard<std::mutex> lk(_mu);
            _shot[1] = std::move(normals);
            _shot[2] = std::move(depths);
            _shot_dirty = true;
            _cost_note = std::move(cost);
            _status.clear();
        } catch (const std::exception& e) {
            set_error(e.what());
        }
#endif
    });
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void GeometryPanel::upload_preview() {
    std::vector<uint8_t> px[3];
    int w[3] = {0, 0, 0}, h[3] = {0, 0, 0};
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_shot_dirty) return;
        _shot_dirty = false;
        for (int i = 0; i < 3; i++) {
            const Rgb* r = _shot[i].get();
            if (!r || r->empty()) continue;
            px[i] = r->px;
            w[i] = r->w;
            h[i] = r->h;
        }
    }
    for (int i = 0; i < 3; i++) {
        if (px[i].empty()) continue;
        if (!_tex[i]) glGenTextures(1, &_tex[i]);
        glBindTexture(GL_TEXTURE_2D, _tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w[i], h[i], 0, GL_RGB,
                     GL_UNSIGNED_BYTE, px[i].data());
        _tex_w[i] = w[i];
        _tex_h[i] = h[i];
    }
}

void GeometryPanel::draw(const GeometryJob& job) {
    if (!_open) return;
    upload_preview();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.75f, vp->WorkSize.y * 0.75f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (!ImGui::Begin(ui::detail::label(dmsg::geom_preview_title), &open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) close();
        return;
    }

    // ---- the picture ----
    int n_frames = 0;
    {
        std::lock_guard<std::mutex> lk(_mu);
        n_frames = (int)_frames.size();
        if (!_source_note.empty()) ui::TextDisabledRaw(_source_note);
        if (!_error.empty())
            ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _error);
        else if (!_status.empty())
            ui::TextDisabledRaw(_status);
        else if (!_cost_note.empty())
            ui::TextRaw(_cost_note);
        else
            ui::TextDisabled(dmsg::geom_preview_legend);
    }

    ImGui::BeginDisabled(_busy.load());
    if (ui::Button(dmsg::preview_try_it)) _needs_run = true;
    ImGui::EndDisabled();

    if (n_frames > 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(320.0f));
        int idx = _frame_idx;
        std::string fmt;
        {
            std::lock_guard<std::mutex> lk(_mu);
            fmt = spirula::i18n::format(
                dmsg::preview_frame,
                {(long long)_frames[(size_t)std::min(idx, n_frames - 1)].index});
        }
        if (ui::SliderIntRaw("##geomframe", &idx, 0, n_frames - 1, fmt.c_str())) {
            _frame_idx = idx;
            _frame_dirty = true;
            _needs_run = true;
        }
        ui::help_on_hover(dmsg::preview_frame_help);
    }

    // Side by side, so the photograph and what was made of it are read
    // together instead of by flipping between them. Which maps appear follows
    // the checkboxes -- what the run would write is what is worth judging.
    const spirula::i18n::Msg* captions[3] = {&dmsg::geom_view_photo, &dmsg::geom_view_normals,
                              &dmsg::geom_view_depth};
    int shown[3] = {0, -1, -1};
    int n_shown = 1;
    if (job.want_normal) shown[n_shown++] = 1;
    if (job.want_depth)  shown[n_shown++] = 2;

    ImGui::BeginChild("##geomimg", ImVec2(0, 0));
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float cell =
        std::max(64.0f, (avail.x - gap * (n_shown - 1)) / (float)n_shown);
    for (int s = 0; s < n_shown; s++) {
        const int i = shown[s];
        if (s) ImGui::SameLine();
        ImGui::BeginChild(captions[i]->id, ImVec2(cell, avail.y));
        ui::TextDisabled(*captions[i]);
        if (_tex[i] && _tex_w[i] > 0) {
            const ImVec2 box = ImGui::GetContentRegionAvail();
            const float aspect = (float)_tex_w[i] / (float)_tex_h[i];
            ImVec2 size(box.x, box.x / aspect);
            if (size.y > box.y) {
                size.y = std::max(box.y, 60.0f);
                size.x = size.y * aspect;
            }
            ImGui::Image((ImTextureID)(intptr_t)_tex[i], size);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    if (_needs_run && !_busy.load()) {
        _needs_run = false;
        start_job(job);
    }

    ImGui::End();
    if (!open) close();
}

}  // namespace gui
