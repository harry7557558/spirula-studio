// The dataset screen's inputs: the source list, its probes and its rows.

#include "app/gui/GuiApp.h"

#include "app/CubeLut.h"
#include "app/LogLuts.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Lidar.h"
#include "i18n/catalog/SourcePreview.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;
namespace ldmsg = spirula::i18n::msg::lidar;
namespace svmsg = spirula::i18n::msg::preview;
using spirula::i18n::Msg;

namespace gui {

// The file-dialog filter list, from DatasetPrep's one list of containers.
static std::vector<std::string> video_dialog_filters() {
    return std::vector<std::string>(kVideoExtensions,
                                    kVideoExtensions + kNumVideoExtensions);
}

// Click sources that name an input the list no longer holds.
static std::vector<std::string> dropped_click_sources(
    const std::vector<PrepInput>& sources, const std::vector<MaskClick>& clicks) {
    std::vector<std::string> gone;
    for (const MaskClick& c : clicks) {
        if (c.source.empty()) continue;
        bool here = false;
        for (const PrepInput& s : sources) here = here || s.path == c.source;
        if (!here && std::find(gone.begin(), gone.end(), c.source) == gone.end())
            gone.push_back(c.source);
    }
    return gone;
}

void GuiApp::refresh_sources() {
    assign_source_subdirs(_sources);
    if (_mask_preview_input >= (int)_sources.size()) _mask_preview_input = 0;

    // Clicks describe one input's frames; when it leaves the list, they go.
    const std::vector<std::string> gone_inputs =
        dropped_click_sources(_sources, _mask.clicks);
    for (const std::string& gone : gone_inputs) {
        log(i18n::format(dmsg::log_clicks_dropped_input_gone, {gone}));
        auto& v = _mask.clicks;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](const MaskClick& c) { return c.source == gone; }),
                v.end());
    }
    if (!gone_inputs.empty() && _mask.clicks.empty()) {
        _mask.object_count = 1;
        _mask.current_object = 0;
    }

    refresh_subcameras(_sources);
    std::string rig_key;
    for (const CameraGroup& g : camera_groups(_sources))
        if (!_sources[g.input].is_video) rig_key += _sources[g.input].path + '\n' + g.rel + '\n';
    if (rig_key != _rig_guess_key) {
        _rig_guess_key = rig_key;
        guess_source_rigs(_sources, /*force=*/false);
    }
    normalize_source_lenses(_sources, _sfm_job.camera_model);
    normalize_source_fps(_sources, _sfm_job.prep.video_fps);
    normalize_source_luts(_sources);

    // The output folder follows the input until the user takes it over --
    // and stays put while the inputs still name it: a run filling it does not
    // make it somebody else's, and a fresh _2 would mean starting over.
    if (_workspace.empty() || _workspace == _workspace_auto) {
        // A scan alone is a dataset of its photographs, written beside it.
        if (_sources.empty() && !_lidar.empty())
            _workspace = fs::path(_lidar[0].path).replace_extension("").string() + "_dataset";
        else if (!workspace_named_by(_sources, _workspace))
            _workspace = default_workspace(_sources);
        _workspace_auto = _workspace;
    }
}
void GuiApp::mark_source_metadata_dirty() {
    _source_probe.invalidate();
    _source_probes_ready = true;
    for (const PrepInput& s : _sources) {
        if (!s.path.empty()) {
            _source_probes_ready = false;
            return;
        }
    }
}
void GuiApp::pump_source_probes() {
    if (native_work_busy()) return;
    bool pending = false;
    bool pano_changed = false, lut_changed = false;
    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        if (s.path.empty()) continue;
        const std::string path = s.path;
        const SourceProbeInfo info = _source_probe.get(path, _ffmpeg_exe, s.is_video);
        if (!info.done) {
            pending = true;
            continue;
        }
        if (!s.is_video) {
            s.detected_color = info.color;
            continue;
        }
        if (info.width > 0 && info.height > 0)
            _input_size[path] = {info.width, info.height};
        s.pano360_unsupported = info.pano360_unsupported;
        if (!s.pano360.valid() && info.pano360.valid()) {
            s.pano360 = info.pano360;
            pano_changed = true;
        }
        // A file with several lenses starts as a rig of its own, once; the row
        // can still say otherwise.
        if (s.video_tracks == 0 && info.video_tracks > 0) {
            s.video_tracks = info.video_tracks;
            if (s.pano360.valid() || s.video_tracks >= 2) s.rig = kRigOwn;
        }
        const int packed = s.packed_lenses;
        set_packed_lenses(s, info.width, info.height, info.unstitched);
        pano_changed = pano_changed || packed != s.packed_lenses;
        if (info.unstitched && packed != s.packed_lenses) {
            // What add_sources does for a fisheye file it knows by its name.
            _border_enable = true;
            if (s.stencil.empty()) s.stencil.detect_border = true;
        }
        if (s.lut_detected != info.lut) {
            s.lut_detected = info.lut;
            lut_changed = true;
        }
    }
    if (lut_changed) {
        normalize_source_luts(_sources);
        close_native_previews();
    }
    if (native_work_busy()) return;
    _source_probes_ready = !pending;
    if (!pano_changed) return;
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    normalize_source_lenses(_sources, _sfm_job.camera_model);
    reapply_dataset_builtin();
}

// Attach a picked `masks/` folder to the input it sits beside (`<root>/images`
// + `<root>/masks`) or under (photos at `<root>`). False when no input owns it,
// the only case worth a message.
static bool attach_mask_folder(std::vector<PrepInput>& sources,
                               const std::string& masks) {
    std::error_code ec;
    const fs::path parent = fs::absolute(masks, ec).parent_path();
    for (PrepInput& s : sources) {
        if (s.is_video) continue;
        const fs::path images = fs::absolute(s.path, ec);
        if (fs::equivalent(images.parent_path(), parent, ec) ||
            fs::equivalent(images, parent, ec)) {
            s.mask_dir = fs::absolute(masks, ec).string();
            return true;
        }
    }
    return false;
}

// Re-answer "are there masks beside these photos?" after the switch moves.
// Turning it off forgets them; turning it back on has to look again, because
// the answer was thrown away rather than remembered.
bool GuiApp::any_found_masks() const {
    for (const PrepInput& s : _sources)
        if (!s.is_video && !s.mask_dir.empty()) return true;
    return false;
}

void GuiApp::rescan_found_masks() {
    for (PrepInput& s : _sources) {
        if (s.is_video) continue;
        if (!_use_found_masks) {
            s.mask_dir.clear();
        } else {
            std::string images = s.path;
            resolve_photo_folder(s.path, images, s.mask_dir);
        }
    }
    refresh_sources();
}

void GuiApp::replace_source(size_t input, const std::string& path) {
    if (input >= _sources.size() || native_work_busy()) return;
    close_native_previews();
    close_splat();
    _sources[input] = path.empty() ? PrepInput{}
                                   : make_source(path, _use_found_masks);
    if (input >= _source_path_edits.size())
        _source_path_edits.resize(input + 1);
    _source_path_edits[input] = _sources[input].path;
    _restored_ws.clear();
    mark_source_metadata_dirty();
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    reapply_dataset_builtin();
    refresh_sources();
}
bool GuiApp::add_sources(const std::vector<std::string>& paths, bool replace) {
    if (paths.empty() || native_work_busy()) return false;
    // A folder of masks is not an input of its own: it belongs to one, and the
    // images half may be in the same pick, in either order. Split before
    // anything else so that picking masks alone cannot clear the list.
    std::vector<std::string> inputs, mask_folders;
    for (const std::string& path : paths) {
        std::error_code ec;
        if (fs::is_directory(path, ec) && is_mask_folder(path))
            mask_folders.push_back(path);
        else
            inputs.push_back(path);
    }
    if (inputs.empty() && _sources.empty()) {
        for (const std::string& masks : mask_folders)
            log(i18n::format(dmsg::log_masks_orphaned, {masks}));
        return false;
    }
    close_native_previews();
    close_splat();
    if (replace && !inputs.empty()) {
        clear_sources();
        // Started from elsewhere, it is a new dataset, scans and all.
        if (_screen != Screen::NewDataset) clear_lidar_sources();
    }
    const size_t first_new = _sources.size();
    for (const std::string& path : inputs) {
        _sources.push_back(make_source(path, _use_found_masks));
        _source_path_edits.push_back(_sources.back().path);
    }
    // A 360 camera's own files always carry the lens border, so they tick the
    // box themselves -- only on arrival, so unticking it sticks.
    for (size_t i = first_new; i < _sources.size(); i++) {
        PrepInput& in = _sources[i];
        if (!has_fisheye_lens(in)) continue;
        _border_enable = true;
        if (in.stencil.empty()) in.stencil.detect_border = true;
    }
    std::vector<bool> fisheye;
    for (const PrepInput& in : _sources) fisheye.push_back(has_fisheye_lens(in));
    adopt_rows_above(_sources, stencil_table_rows(_sources), first_new, fisheye);
    apply_frame_shapes(_sources, _frame_shapes, first_new);
    for (const std::string& masks : mask_folders) {
        if (attach_mask_folder(_sources, masks))
            log(i18n::format(dmsg::log_masks_attached, {masks}));
        else
            log(i18n::format(dmsg::log_masks_orphaned, {masks}));
    }
    if (!inputs.empty()) mark_source_metadata_dirty();
    if (_sources.empty()) return false;
    // What was just added is what the user wants to look at.
    if (first_new < _sources.size()) select_source(_sources[first_new].path, false);
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    reapply_dataset_builtin();
    if (_mask_preview_input >= (int)_sources.size()) _mask_preview_input = 0;
    refresh_sources();
    return true;
}

void GuiApp::clear_sources() {
    _sources.clear();
    _source_path_edits.clear();
    _mask_preview_input = 0;
    // A capture dropped again is restored from again (restore_from_record).
    _restored_ws.clear();
    // A folder typed for the last capture is not this one's.
    _workspace.clear();
    _workspace_auto.clear();
    // A different capture is a different job, and the geometry options
    // are remembered nowhere: a run must never quietly cost an hour of
    // inference nobody asked for. (Not mid-run: that would disable it.)
    if (!dataset_busy()) _geometry = GeometryJob{};
    // ... and a different capture is a different colour space.
    _sfm_job.image_gamut.clear();
    _sfm_job.image_is_linear.reset();
}

void GuiApp::add_existing_dataset(const std::string& dir) {
    if (dir.empty() || add_xgrids_export(dir)) return;
    add_dataset_folder(dir);
}

void GuiApp::add_dataset_folder(const std::string& dir) {
    // The ordinary route already lands on the right pair for the usual
    // layout: resolve_photo_folder picks images/ out of the folder and
    // refresh_sources makes its parent -- this folder -- the output.
    if (!add_sources({dir}, /*replace=*/true)) return;
    std::error_code ec;
    if (!folder_looks_like_dataset(dir) ||
        !fs::equivalent(_sources[0].path, dir, ec))
        return;
    // A dataset whose photographs sit at its root rather than under images/.
    // They are read where they are: gathering them into an images/ beside the
    // model would leave the folder holding the capture twice.
    _photo_import = PhotoImport::InPlace;
    _workspace = _workspace_auto = fs::absolute(dir, ec).string();
}

void GuiApp::apply_frame_shapes(std::vector<PrepInput>& inputs, std::string& preset,
                                size_t first_input) {
    if (preset.empty()) return;
    app::MaskSet set;
    std::string err;
    if (!load_stencil_preset(preset, set, err)) {
        log(i18n::format(dmsg::stencil_load_failed, {err}));
        preset.clear();
        return;
    }
    for (size_t i = first_input; i < inputs.size(); i++)
        app::apply_mask_set(inputs[i].stencil, set);
}

// ---------------------------------------------------------------------------
// Source, destination, resume
// ---------------------------------------------------------------------------

namespace {

// Built by hand rather than with ui::Combo for the same reason as the lens
// pickers: what each row costs is the whole of the question, and one tooltip
// on the closed combo cannot answer it row by row.
void photo_import_combo(PhotoImport* mode, bool no_in_place) {
    const std::vector<const Msg*> labels{
        &dmsg::photo_import_convert, &dmsg::photo_import_copy,
        &dmsg::photo_import_move, &dmsg::photo_import_inplace};
    const std::vector<const Msg*> helps{
        &dmsg::photo_import_convert_help, &dmsg::photo_import_copy_help,
        &dmsg::photo_import_move_help, &dmsg::photo_import_inplace_help};
    int idx = (int)*mode;
    ImGui::SetNextItemWidth(px(kFieldW));
    if (ui::BeginCombo(dmsg::photo_import, labels[(size_t)idx]->get())) {
        for (int i = 0; i < kNumPhotoImports; i++) {
            const bool blocked =
                no_in_place && (PhotoImport)i == PhotoImport::InPlace;
            ImGui::BeginDisabled(blocked);
            if (ui::Selectable(*labels[(size_t)i], i == idx))
                *mode = (PhotoImport)i;
            ImGui::EndDisabled();
            ui::help_on_hover(*helps[(size_t)i]);
        }
        ImGui::EndCombo();
    }
    ui::help_on_hover(dmsg::photo_import_help);
}

}  // namespace

// One switch per kind of sensor this input was found to carry, beside the row
// that chose it: nothing found is nothing to switch. Greyed, with the reason,
// where the engine or Advanced > Sensors reads none of it anyway.
void GuiApp::draw_sensor_toggles(PrepInput& s) {
    std::error_code ec;
    if (s.path.empty() ||
        (s.is_video ? !fs::is_regular_file(s.path, ec)
                    : !fs::is_directory(s.path, ec)))
        return;
    const TelemetryInfo t = _telemetry.get(s.path, s.is_video);
    if (!t.done) {
        ImGui::SameLine();
        ui::TextDisabled(dmsg::sensors_reading);
        return;
    }
    const bool imu = s.is_video ? t.gyro || t.accel || t.attitude : t.with_attitude > 0;
    const bool gps = s.is_video ? t.gps : t.with_gps > 0;
    if (!imu && !gps) {
        if (s.is_video || t.photos > 0) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::sensors_none);
        }
        return;
    }
    const bool colmap = effective_engine() != Engine::BuiltIn;
    // Nothing under Advanced quiets photo GPS entirely: --metric-gps none still
    // lets it propose pairs and admit an equirect capture's level prior. A
    // video's GPS has no such use unless --sensor-pairs asks.
    const Msg* imu_off = s.is_video ? (_sfm_job.sensor_gauge == 0 ? &dmsg::sfm_sensor_gauge : nullptr)
                                    : (_sfm_job.exif_attitude == 0 ? &dmsg::sfm_exif_attitude : nullptr);
    const Msg* gps_off =
        s.is_video && _sfm_job.metric_gps == 0 ? &dmsg::sfm_metric_gps : nullptr;
    auto explain = [&](const Msg& help, const Msg* off) {
        if (colmap) {
            ui::help_on_hover_disabled(dmsg::sensors_unused_colmap);
        } else if (off) {
            ui::help_on_hover_raw(i18n::format(dmsg::sensors_off_in_advanced, {off->get()}).c_str(),
                                  ImGuiHoveredFlags_AllowWhenDisabled);
        } else if (t.carrier.empty()) {
            ui::help_on_hover(help);
        } else {
            const std::string text = std::string(help.get()) + "\n\n" +
                                     i18n::format(dmsg::sensors_carrier_tooltip, {t.carrier});
            ui::help_on_hover_raw(text.c_str());
        }
    };
    const long long n = t.photos;
    if (imu) {
        ImGui::SameLine();
        ImGui::BeginDisabled(colmap || imu_off);
        if (s.is_video) ui::ToggleButton(dmsg::sensors_imu, &s.use_imu);
        else ui::ToggleButton(dmsg::sensors_photo_attitude, {(long long)t.with_attitude, n}, &s.use_imu);
        ImGui::EndDisabled();
        explain(s.is_video ? dmsg::sensors_video_imu_help : dmsg::sensors_photo_attitude_help, imu_off);
    }
    if (gps) {
        ImGui::SameLine();
        ImGui::BeginDisabled(colmap || gps_off);
        if (s.is_video) ui::ToggleButton(dmsg::sensors_gps, &s.use_gps);
        else ui::ToggleButton(dmsg::sensors_photo_gps, {(long long)t.with_gps, n}, &s.use_gps);
        ImGui::EndDisabled();
        explain(s.is_video ? dmsg::sensors_video_gps_help : dmsg::sensors_photo_gps_help, gps_off);
    }
}

namespace {

// A clip's ends on an input row: "00:01:23.4", the hour always there.
std::string clip_clock(double s) {
    const long long ds = (long long)std::floor(std::max(s, 0.0) * 10.0 + 0.5);
    char b[48];
    std::snprintf(b, sizeof b, "%02lld:%02lld:%02lld.%lld", ds / 36000, (ds / 600) % 60,
                  (ds / 10) % 60, ds % 10);
    return b;
}

// The rate box's text, with the unit; "0 fps" is every frame. A row following
// the one above shows the caret the lens column uses for the same idea.
std::string fps_label(float fps) {
    char b[32];
    std::snprintf(b, sizeof b, "%g fps", (double)std::max(fps, 0.0f));
    return b;
}

// And back: a number is a rate, with or without the unit typed back, and 0 is
// every frame. Nothing readable is nullopt.
std::optional<float> parse_fps(const std::string& text) {
    const char* first = text.c_str();
    char* end = nullptr;
    const double v = std::strtod(first, &end);
    if (end == first || !(v >= 0.0)) return std::nullopt;
    return (float)v;
}

// The row whose box holds the dataset's own rate: it has nothing above it to
// follow, so it is never a caret.
size_t first_video_row(const std::vector<PrepInput>& sources) {
    for (size_t i = 0; i < sources.size(); i++)
        if (sources[i].is_video) return i;
    return sources.size();
}

}  // namespace

void GuiApp::draw_dataset_source() {
    // One row per input. Several videos reconstruct as one scene -- each gets
    // its own folder of frames under images/, and so its own camera.
    int remove = -1;
    bool edited = false;
    if (_source_path_edits.size() != _sources.size()) {
        const size_t old = _source_path_edits.size();
        _source_path_edits.resize(_sources.size());
        for (size_t i = old; i < _sources.size(); i++)
            _source_path_edits[i] = _sources[i].path;
    }
    // The path box takes what the rows leave, measured on the frame before: a
    // translated label or a badge still being read has no width until drawn.
    // Below a dozen characters, the label and badge get a line of their own.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float min_path = ImGui::GetFontSize() * 12.0f;
    const float controls_w = _source_controls_w > 0.0f ? _source_controls_w : px(250.0f);
    const float info_w = _source_info_w > 0.0f ? _source_info_w : px(340.0f);
    const bool one_line = avail - controls_w - info_w >= min_path;
    const float path_w =
        std::max(min_path, avail - controls_w - (one_line ? info_w : 0.0f));
    float row_controls = 0.0f, row_info = 0.0f;
    _source_row_h.resize(_sources.size(), 0.0f);
    const int selected = _source_sel_scan ? -1 : selected_source();
    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        ImGui::PushID((int)i);
        const float row_y = ImGui::GetCursorScreenPos().y;
        if (row_select_begin(_source_row_h[i], (int)i == selected)) select_source(s.path, false);
        ImGui::SetNextItemWidth(path_w);
        std::string& path_edit = _source_path_edits[i];
        ui::InputTextRaw("##in", &path_edit);
        if (ImGui::IsItemActivated()) select_source(s.path, false);
        const float path_right = ImGui::GetItemRectMax().x;
        // A typed path is only worth resolving once it is finished -- rewriting
        // `<folder>` to `<folder>/images` under the cursor is not helpful.
        if (ImGui::IsItemDeactivatedAfterEdit() && path_edit != s.path)
            replace_source(i, path_edit);
        ImGui::SameLine();
        if (ui::Button(dmsg::browse)) {
            _pick_source = (int)i;
            if (s.is_video)
                open_pick(PickAction::SourceReplace, msg::pick_video_file.get(),
                          FileDialog::Mode::File, video_dialog_filters());
            else
                open_pick(PickAction::SourceReplace,
                          msg::pick_photo_folder.get(),
                          FileDialog::Mode::Folder);
        }
        ImGui::SameLine();
        if (ui::Button(dmsg::remove)) remove = (int)i;
        {
            // The rate lives beside the video it describes rather than in the
            // settings below, so a list of clips reads as the one decision it
            // usually is.
            ImGui::SameLine();
            if (s.is_video) {
                ImGui::BeginDisabled(dataset_locked(Stage::Frames));
                ImGui::SetNextItemWidth(px(84.0f));
                // The first video's box IS the dataset's rate -- that is what a
                // preset carries and what every item of a batch starts from --
                // so its own `fps` stays 0 and the rest follow it.
                const bool head = i == first_video_row(_sources);
                if (_fps_text.size() != _sources.size()) {
                    _fps_text.assign(_sources.size(), std::string());
                    _fps_editing = -1;
                }
                std::string& text = _fps_text[i];
                if (_fps_editing != (int)i)
                    text = head ? fps_label(_sfm_job.prep.video_fps)
                                : s.fps == 0.0f ? std::string("^") : fps_label(s.fps);
                ui::InputTextRaw("##fps", &text);
                if (ImGui::IsItemActive()) _fps_editing = (int)i;
                else if (_fps_editing == (int)i) _fps_editing = -1;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    const std::optional<float> v = parse_fps(text);
                    // An unreadable answer leaves a head row's rate alone and
                    // is the caret below it. A number, even the rate above,
                    // is the row's own rate and its own adaptive group.
                    if (head) {
                        if (v) _sfm_job.prep.video_fps = *v;
                    } else {
                        s.fps = !v ? 0.0f : *v > 0.0f ? *v : kFpsEveryFrame;
                    }
                    edited = true;
                }
                ui::help_on_hover(head ? (_sfm_job.prep.adaptive_fps
                                              ? dmsg::frames_per_second_help_adaptive
                                              : dmsg::frames_per_second_help)
                                       : dmsg::video_fps_this_one_help);
                ImGui::EndDisabled();
                ImGui::SameLine();
            }
            ImGui::BeginDisabled(dataset_locked(Stage::Features));
            if (ui::ToggleButton(dmsg::source_sequential, &s.sequential))
                apply_capture_order(_sources, _colmap_job);
            ImGui::EndDisabled();
            ui::help_on_hover(dmsg::source_sequential_help);
        }
        row_controls = std::max(row_controls, ImGui::GetItemRectMax().x - path_right +
                                                  ImGui::GetStyle().ItemSpacing.x);
        if (one_line) ImGui::SameLine();
        const float info_left = ImGui::GetCursorScreenPos().x;
        // What this input is and whether masks were found for it. Four whole
        // messages, not a kind + "+ masks" tail: they do not compose in that
        // order in every language.
        const bool masked = !s.mask_dir.empty();
        if (_sources.size() > 1) {
            const Msg& row = s.is_video
                ? (masked ? dmsg::row_video_masks_to : dmsg::row_video_to)
                : (masked ? dmsg::row_photos_masks_to : dmsg::row_photos_to);
            ui::TextDisabled(row, {s.subdir});
        } else {
            ui::Text(s.is_video
                         ? (masked ? dmsg::kind_video_file_masks
                                   : dmsg::kind_video_file)
                         : (masked ? dmsg::kind_photo_folder_masks
                                   : dmsg::kind_photo_folder));
        }
        if (masked && ImGui::IsItemHovered())
            ui::SetTooltip(dmsg::existing_masks_tooltip, {s.mask_dir});
        if (s.pano360_unsupported) {
            ImGui::SameLine();
            ui::TextColored(kWarn, dmsg::pano360_unsupported);
            ui::help_on_hover(dmsg::pano360_unsupported_help);
        }
        ImGui::BeginDisabled(dataset_locked(Stage::Features));
        draw_sensor_toggles(s);
        ImGui::EndDisabled();
        // Folded away under Advanced, a LUT still changes every frame.
        if (const std::string lut = s.is_video && !_ds_advanced ? input_lut(_sources, i) : "";
            !lut.empty()) {
            ImGui::SameLine();
            ui::TextDisabledRaw(app::lut_display_name(lut));
            ui::help_on_hover(dmsg::video_lut_help, {app::is_builtin_lut(lut)
                                                         ? app::lut_display_name(lut)
                                                         : lut});
        }
        // Set on the preview's timeline, which is not always on screen.
        if (s.clipped()) {
            ImGui::SameLine();
            ui::TextDisabled(svmsg::row_clip,
                             {clip_clock(s.clip_start),
                              s.clip_end > 0.0 ? clip_clock(s.clip_end)
                                               : std::string(svmsg::row_clip_end.get())});
            ui::help_on_hover(svmsg::row_clip_help);
        }
        row_info = std::max(row_info, ImGui::GetItemRectMax().x - info_left);
        _source_row_h[i] = ImGui::GetCursorScreenPos().y - row_y - ImGui::GetStyle().ItemSpacing.y;
        ImGui::PopID();
    }
    if (!_sources.empty()) {
        _source_controls_w = row_controls;
        _source_info_w = row_info;
    }
    if (remove >= 0 && !native_work_busy()) {
        close_native_previews();
        close_splat();
        _sources.erase(_sources.begin() + remove);
        _source_path_edits.erase(_source_path_edits.begin() + remove);
        mark_source_metadata_dirty();
        edited = true;
    }
    if (edited) refresh_sources();
    draw_lidar_rows(path_w, one_line);

    // The ways in, on one line while they fit: four labels run off a narrow
    // panel in several languages.
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    auto same_line_if_fits = [&](const Msg& next, bool button) {
        const ImGuiStyle& st = ImGui::GetStyle();
        const float w = ImGui::CalcTextSize(next.get()).x + (button ? st.FramePadding.x * 2 : 0);
        if (ImGui::GetItemRectMax().x + st.ItemSpacing.x + w <= right) ImGui::SameLine();
    };
    if (ui::Button(dmsg::add_video)) {
        open_pick(PickAction::SourceVideo, msg::pick_videos.get(),
                  FileDialog::Mode::File, video_dialog_filters(), "",
                  /*multi_select=*/true);
    }
    ui::help_on_hover(dmsg::add_video_help);
    same_line_if_fits(dmsg::add_photos, true);
    if (ui::Button(dmsg::add_photos)) {
        open_pick(PickAction::SourceImages, msg::pick_photo_folder.get(),
                  FileDialog::Mode::Folder);
    }
    ui::help_on_hover(dmsg::add_photos_help);
    same_line_if_fits(dmsg::add_dataset, true);
    if (ui::Button(dmsg::add_dataset)) {
        open_pick(PickAction::SourceDataset, msg::pick_existing_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ui::help_on_hover(dmsg::add_dataset_help);
    same_line_if_fits(ldmsg::add_scan, true);
    if (ui::Button(ldmsg::add_scan)) {
        _pick_lidar = -1;
        open_pick(PickAction::LidarSource, ldmsg::pick_scan.get(), FileDialog::Mode::File,
                  {".e57", ".las", ".ply"}, "", /*multi_select=*/true);
    }
    ui::help_on_hover(ldmsg::add_scan_help);
    if (_sources.empty() && _lidar.empty()) {
        same_line_if_fits(dmsg::no_input_yet, false);
        ui::TextDisabled(dmsg::no_input_yet);
    }
    draw_lidar_options();

    // Masks that came WITH the photos are adopted automatically: right for a
    // prepared capture, wrong for a masks/ that describes something else.
    // Offered only for photo inputs; a video has none.
    bool any_photos = false;
    for (const PrepInput& s : _sources) any_photos = any_photos || !s.is_video;
    if (any_photos) {
        if (ui::Checkbox(dmsg::use_found_masks, &_use_found_masks))
            rescan_found_masks();
        ui::help_on_hover(dmsg::use_found_masks_help);
        // Which way round they are is a property of those files, so it is asked
        // here rather than per stage: everything the run writes comes out in
        // the one convention, whatever the folder arrived in.
        if (_use_found_masks && any_found_masks()) {
            ImGui::Indent();
            ui::Checkbox(dmsg::flip_found_masks, &_flip_found_masks);
            ui::help_on_hover(dmsg::flip_found_masks_help);
            ImGui::Unindent();
        }
        // A scan's photographs or views join the images, so they cannot be
        // read where they are.
        photo_import_combo(&_photo_import,
                           _sources.size() > 1 || (!_sources.empty() && _sources[0].heif) ||
                               (!_lidar.empty() && !_lidar_in_frame));
    }

    ImGui::SetNextItemWidth(px(-220.0f));
    ui::InputTextRaw("##ws", &_workspace);
    ImGui::SameLine();
    // Two "Browse..." buttons in one scope: the message supplies the ID, so
    // they need distinguishing exactly as two identical literals would.
    ImGui::PushID("ws");
    if (ui::Button(dmsg::browse)) {
        open_pick(PickAction::Workspace, msg::pick_output_folder.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::PopID();
    ImGui::SameLine();
    ui::Text(dmsg::output_folder);

    // What is there already: what a run can pick up and what it would write
    // over. The output is often the input folder (images/ + masks/), so the
    // input's own images count as neither (probe_workspace).
    const WorkspaceState& prior = workspace_state();
    if (prior.resumable()) {
        ui::Checkbox(dmsg::resume_previous, &_resume);
        ui::help_on_hover(dmsg::resume_previous_help);
        // A finished run keeps features/ too; only its record can say the
        // folder holds a step that never finished.
        bool unfinished = !_ws_record.present;
        for (const StepRecord& r : _ws_record.steps)
            unfinished = unfinished || (r.present && !r.complete);
        if (unfinished) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::unfinished_run_detected);
        }
    }
    // A finished dataset in the output folder is reused unless its settings
    // differ, so a capture somebody else reconstructed can be given masks,
    // depth and normals. draw_dataset_plan lists what a run will do with it.
    if (prior.model) {
        ui::Checkbox(dmsg::reconstruct_again, &_redo_model);
        ui::help_on_hover(dmsg::reconstruct_again_help);
    }
}

}  // namespace gui
