// The log pane, and the run log each training run writes beside its output.

#include "app/gui/GuiApp.h"

#include "app/RunLog.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/SfmOptions.h"
#include "app/gui/Ui.h"
#include "dense/ConfigFields.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <fstream>

namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace runlog = spirula::runlog;
using spirula::runlog::cfg_str;

namespace gui {

static const char* photo_import_name(PhotoImport m) {
    switch (m) {
        case PhotoImport::ConvertJpeg: return "convert-jpeg";
        case PhotoImport::Copy:        return "copy";
        case PhotoImport::Move:        return "move";
        case PhotoImport::InPlace:     return "in-place";
    }
    return "?";
}

// One entry per SCREEN line: the panel clips with a list clipper and
// compensates its scroll for trimmed lines, and both want a uniform height.
void GuiApp::log(const std::string& s, bool detail) {
    const std::string stamp = runlog::line_stamp();
    size_t at = 0;
    do {
        const size_t nl = s.find('\n', at);
        _log.push_back({stamp + s.substr(at, nl == std::string::npos ? nl : nl - at),
                        detail});
        at = nl == std::string::npos ? nl : nl + 1;
    } while (at != std::string::npos);
    while (_log.size() > 4000) {
        if (!_log.front().detail || _log_details) _log_dropped++;
        _log.pop_front();
    }
    _log_shown_dirty = true;
}

void GuiApp::clear_log() {
    _log.clear();
    _log_shown.clear();
    _log_dropped = 0;
    _log_shown_dirty = true;
}

// The panel snapshot at the top of every run log: plain key=value lines in
// sections, no timestamps -- the timestamps start below the separator.
void GuiApp::write_run_settings(std::ofstream& f) {
    if (!f.is_open()) return;
    std::string out;
    auto section = [&out](const std::string& name) { out += runlog::section(name); };
    auto line = [&out](const std::string& k, const std::string& v) {
        out += runlog::entry(k, v);
    };

    out += runlog::run_heading();
    line("run_started", runlog::file_stamp());
    line("spirula_version", SS_VERSION);
    line("engine", effective_engine() == Engine::BuiltIn ? "builtin" : "colmap");
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
    line("native_device", _native_device_uuid.empty() ? "auto" : _native_device_uuid);
    if (!_native_device_name.empty()) line("native_device_name", _native_device_name);
#endif
#ifdef SS_BUILD_SAM
    line("additional_gpus", _additional_gpus);
    line("worker_devices", resolved_worker_devices());
#endif
#ifndef SS_BACKEND_VULKAN
    if (_cuda_device_index >= 0)
        line("cuda_device", std::to_string(_cuda_device_index));
#endif
    line("preset", _preset);
    if (!_train_presets.file.empty()) line("preset_file", _train_presets.file);
    if (!_ds_presets.file.empty()) line("dataset_preset_file", _ds_presets.file);

    section(msg::runlog_section_prep.get());
    line("workspace", _workspace);
    line("photo_import", photo_import_name(_photo_import));
    for (const PrepInput& s : _sources) {
        line(s.is_video ? "video_source" : "photo_source", s.path);
        line("sequential", cfg_str(s.sequential));
        if (!s.use_imu) line(s.is_video ? "imu" : "attitude", cfg_str(false));
        if (!s.use_gps) line("gps", cfg_str(false));
    }
    line("use_found_masks", cfg_str(_use_found_masks));
    line("flip_found_masks", cfg_str(_flip_found_masks));
    line("masking_enabled", cfg_str(_mask_enable));
    line("mask_features", cfg_str(_mask_features));
    line("mask_detect_every", std::to_string(_mask_detect_every));

    section(i18n::format(msg::runlog_section_recon, {"SfM"}));
    const SfmJob& j = _sfm_job;
    line("quality", std::to_string(j.quality));
    line("camera_model", j.camera_model);
    line("camera_mode", std::to_string(j.camera_mode));
    line("pairs", sfm_pick(kSfmPairs, sfm_pairs(j.pairs)));
    line("overlap", std::to_string(j.overlap));
    line("loop_closure", cfg_str(j.loop_closure));
    line("features", std::to_string(j.features));
    line("matcher", std::to_string(j.matcher));
    line("mapper", std::to_string(j.mapper));
    line("init_focal_px", cfg_str(j.init_focal_px));
    if (!j.init_distortion.empty()) line("init_distortion", j.init_distortion);
    line("distortion_refine", std::to_string(j.distortion_refine));
    line("final_per_image_intrinsics", cfg_str(j.final_per_image_intrinsics));
    line("final_free_rig", cfg_str(j.final_free_rig));
    for (const auto& [flag, value] : active_sfm_options(j)) line(sfm_option_field(flag), value);
    line("metric_gps", std::to_string(j.metric_gps));
    line("sensor_gauge", std::to_string(j.sensor_gauge));
    line("exif_attitude", std::to_string(j.exif_attitude));
    line("keep_intermediate", cfg_str(j.keep_intermediate));
    line("ba_cpu", cfg_str(j.ba_cpu));
    line("subprocess", cfg_str(j.subprocess));
    line("force_external_decode", cfg_str(j.prep.force_external_decode));
    line("video_fps", cfg_str(j.prep.video_fps));
    line("adaptive_fps", cfg_str(j.prep.adaptive_fps));
    if (j.prep.adaptive_fps) line("adaptive_range", cfg_str(j.prep.adaptive_range));
    for (const PrepInput& s : _sources)
        if (s.is_video && s.fps != 0.0f)
            line("video_fps:" + (s.subdir.empty() ? s.path : s.subdir),
                 cfg_str(std::max(s.fps, 0.0f)));
    for (size_t i = 0; i < _sources.size(); i++)
        if (const std::string lut = input_lut(_sources, i); _sources[i].is_video && !lut.empty())
            line("video_lut:" + (_sources[i].subdir.empty() ? _sources[i].path : _sources[i].subdir),
                 lut);
    for (const PrepInput& s : _sources)
        if (s.clipped())
            line("video_clip:" + (s.subdir.empty() ? s.path : s.subdir),
                 cfg_str((float)s.clip_start) + "-" +
                     (s.clip_end > 0.0 ? cfg_str((float)s.clip_end) : std::string("end")));
    line("sharp_window", std::to_string(j.prep.sharp_window));
    line("skip_blurry", cfg_str(j.prep.skip_blurry));
    if (j.prep.skip_blurry) {
        line("blur_px", cfg_str(j.prep.blur_px));
        line("blur_neighbour", cfg_str(j.prep.blur_neighbour));
        if (j.prep.similar_bits > 0) line("similar_bits", std::to_string(j.prep.similar_bits));
    }
    line("sync_tracks", cfg_str(j.prep.sync_tracks));
    line("max_frames", std::to_string(j.prep.max_frames));
    if (!j.extra_args.empty()) line("extra_args", j.extra_args);

    if (effective_engine() == Engine::Colmap) {
        section(i18n::format(msg::runlog_section_recon, {"COLMAP"}));
        line("colmap_exe", _colmap_job.colmap_exe);
        line("camera_model", _colmap_job.camera_model);
        line("camera_mode", std::to_string(_colmap_job.camera_mode));
    }

    section(spirula::i18n::msg::dense::title.get());
    line("enabled", cfg_str(_dense.enable));
    line("use_for_training", cfg_str(_dense.use_for_training));
    line("log_performance", cfg_str(_dense.log_performance));
    line("settings", spirula::dense::config_json(_dense.config));

    section(msg::runlog_section_geometry.get());
    line("enabled", cfg_str(_geometry.enable));
    line("model", _geometry.model);
    line("want_depth", cfg_str(_geometry.want_depth));
    line("want_normal", cfg_str(_geometry.want_normal));
    line("max_size", std::to_string(_geometry.max_size));
    line("face_res", _geometry.face_res == 1 ? "source" : "output");

    out += runlog::train_sections(_cfg);
    out += runlog::settings_end();
    f << out;
    f.flush();
}

void GuiApp::append_logs() {
    for (auto& s : _runner.drain_log()) {
        runlog::write_line(_train_log, s);
        log(s);
    }
    for (auto& l : _colmap.steps().drain()) {
        runlog::write_line(_prep_log, l.text);
        log(l.text, l.detail);
    }
    for (auto& l : _sfm.steps().drain()) {
        runlog::write_line(_prep_log, l.text);
        log(l.text, l.detail);
    }
    for (auto& s : _compare.drain_log()) log(s);
    for (auto& s : _mesh.drain_log()) log(s);
    poll_finish_command();
    for (auto& s : _download.drain_log()) log(s);
    for (auto& s : _font_download.drain_log()) log(s);
    // A finished font download is the one thing besides a language switch
    // that changes what the atlas should hold; GuiMain rebuilds it between
    // frames.
    if (_font_fetching &&
        _font_download.state() == FileDownload::State::Done) {
        _font_fetching = nullptr;
        _fonts.invalidate();
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

float GuiApp::log_height(float avail) const {
    if (!_show_log) return 0.0f;
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float pad = ImGui::GetStyle().WindowPadding.y * 2.0f;
    float h = std::max(_log_h * ui_scale(), 4.0f * line + pad);
    // The cap wins over the floor: on a window too short for both, a log that
    // took the four lines it wants would leave the screen above it with none.
    return std::max(0.0f, std::min(h, avail * 0.60f));
}

float GuiApp::body_height(float log_h) {
    // ItemSpacing twice: once above the splitter's grab and once below it.
    // Counting it once left the body a spacing too tall, which the host window
    // answered by becoming scrollable by exactly that much.
    if (log_h <= 0.0f) return 0.0f;
    return -(log_h + splitter_extent() + ImGui::GetStyle().ItemSpacing.y);
}

void GuiApp::draw_log_panel(float height) {
    if (height <= 0.0f) return;

    {
        const float line = ImGui::GetTextLineHeightWithSpacing();
        float h = height;
        if (splitter_h("##logsplit", &h, 3.0f * line,
                       std::max(3.0f * line, ImGui::GetWindowHeight() * 0.8f),
                       ImGui::GetContentRegionAvail().x)) {
            _log_h = h / ui_scale();
            _layout_dirty = true;
        }
    }

    ImGui::BeginChild("##log", ImVec2(0, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Reading back where the view sits is the whole state: pinned to the
    // bottom means "follow", and a wheel-up on the previous frame has already
    // moved it by the time this runs.
    _log_follow = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
    if (_log_scroll_end) {
        _log_scroll_end = false;
        _log_follow = true;
    }
    if (_log_dropped) {
        if (!_log_follow)
            ImGui::SetScrollY(std::max(0.0f,
                ImGui::GetScrollY() -
                    (float)_log_dropped * ImGui::GetTextLineHeightWithSpacing()));
        _log_dropped = 0;
    }

    // A dataset run says far more than it means. The default view is the lines
    // that answer "what happened"; everything else is behind Details, which is
    // where a bug report is copied from.
    if (_log_shown_dirty) {
        _log_shown.clear();
        for (size_t i = 0; i < _log.size(); i++)
            if (_log_details || !_log[i].detail) _log_shown.push_back(i);
        _log_shown_dirty = false;
    }

    // Log lines arrive already formatted, from here and from the engine, one
    // entry per screen line -- which is what lets the clipper skip the 4000
    // that are not visible.
    ImGuiListClipper clip;
    clip.Begin((int)_log_shown.size());
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++)
            ui::TextRaw(_log[_log_shown[(size_t)i]].text);
    clip.End();
    if (_log_follow) ImGui::SetScrollHereY(1.0f);

    if (ImGui::BeginPopupContextWindow()) {
        if (ui::MenuItem(msg::log_follow, nullptr, _log_follow))
            _log_scroll_end = true;
        if (ui::MenuItem(msg::log_details, nullptr, _log_details)) {
            _log_details = !_log_details;
            _log_shown_dirty = true;
            _log_scroll_end = true;
            save_settings();
        }
        if (ui::MenuItem(msg::log_copy)) {
            // Everything, whichever view is up: a log copied for somebody else
            // to read is not the summary.
            std::string all;
            for (const auto& e : _log) { all += e.text; all += '\n'; }
            ImGui::SetClipboardText(all.c_str());
        }
        if (ui::MenuItem(msg::log_clear)) clear_log();
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // The way back, offered only when it is needed: scrolled off the bottom
    // with the run still writing. Drawn over the panel's bottom-right corner
    // so it costs no height.
    if (!_log_follow) {
        const ImVec2 corner = ImGui::GetItemRectMax();
        const float bw = ImGui::CalcTextSize(msg::log_jump.get()).x +
                         ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextWindowPos(ImVec2(corner.x - bw - px(24.0f),
                                       corner.y - ImGui::GetFrameHeight() - px(8.0f)));
        if (ImGui::Begin("##logjump", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (ui::Button(msg::log_jump)) _log_scroll_end = true;
        }
        ImGui::End();
    }
}

}  // namespace gui
