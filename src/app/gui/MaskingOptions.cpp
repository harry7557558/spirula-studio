// Masking on the dataset screen: frame stencils, the segmentation prompt and model.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Ui.h"
#include "core/LicenseFamilies.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Dataset.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
using spirula::i18n::Msg;

namespace gui {

// Inputs no click prompts, named as the picker names them. A clicks-only job
// needs this empty; DatasetPrep::run refuses it otherwise.
static std::string inputs_without_clicks(const std::vector<PrepInput>& sources,
                                         const std::vector<MaskClick>& clicks) {
    std::string out;
    for (const PrepInput& s : sources) {
        bool prompted = false;
        for (const MaskClick& c : clicks)
            prompted = prompted || c.source.empty() || c.source == s.path;
        if (prompted) continue;
        if (!out.empty()) out += ", ";
        out += s.subdir.empty() ? s.path : s.subdir;
    }
    return out;
}

// What this run draws on its inputs, kept in the output folder for next time.
void GuiApp::save_run_stencils() {
    if (!_border_enable || _workspace.empty()) return;
    std::vector<std::pair<std::string, app::MaskSet>> inputs;
    for (const PrepInput& in : _sources) inputs.push_back({in.path, app::mask_set_of(in.stencil)});
    for (const auto& [scan, st] : _sfm_job.prep.scan_stencils)
        inputs.push_back({scan, app::mask_set_of(st)});
    std::string err;
    const std::vector<std::string> written = save_dataset_stencils(_workspace, inputs, err);
    if (!err.empty()) log(i18n::format(dmsg::stencil_save_failed, {err}));
    for (const std::string& f : written) log(i18n::format(dmsg::stencil_autosaved, {f}));
}

void GuiApp::load_stencil_sets() {
    _stencil_sets = list_stencil_presets();
    _stencil_set_shapes.assign(_stencil_sets.size(), app::MaskSet{});
    std::string err;
    for (size_t k = 0; k < _stencil_sets.size(); k++)
        load_stencil_preset(_stencil_sets[k].path, _stencil_set_shapes[k], err);
    _stencil_sets_stale = false;
}

std::vector<StencilRow> GuiApp::stencil_table_rows(const std::vector<PrepInput>& inputs) const {
    std::vector<std::vector<std::string>> keys;
    keys.reserve(inputs.size());
    for (const PrepInput& in : inputs) keys.push_back(stencil_cameras(_sfm_job.prep, in));
    return stencil_rows(keys);
}

void GuiApp::refresh_frame_shapes(const std::vector<PrepInput>& inputs,
                                  std::string& frame_shapes) {
    if (_stencil_sets_stale) load_stencil_sets();
    const int s = common_set(_stencil_set_shapes, inputs, stencil_table_rows(inputs));
    frame_shapes = s < 0 ? std::string() : _stencil_sets[(size_t)s].name;
}

GuiApp::StencilTable GuiApp::stencil_table() {
    StencilTable t;
    std::vector<std::string> setups;
    for (const PrepInput& in : _sources) {
        int w = 0, h = 0;
        if (is_dual_lens(in)) input_pixel_size(in.path, in.is_video, w, h);
        setups.push_back(lens_setup(in, w, h));
    }
    t.owners = stencil_owners(_sources, setups);
    t.inputs = _sources.size();
    for (const PrepInput& in : _sources) {
        fs::path path(in.path);
        if (path.filename().empty()) path = path.parent_path();   // trailing separator
        t.names.push_back(in.subdir.empty() ? path.filename().string() : in.subdir);
        t.paths.push_back(in.path);
    }
    t.rows = stencil_table_rows(_sources);
    const LidarJob lj = lidar_job();
    if (!lj.scan_photos || lj.in_frame) return t;
    std::vector<std::vector<std::string>> keys;
    for (size_t i = 0; i < _lidar.size(); i++) {
        LidarInput& l = _lidar[i];
        if (l.photos <= 0) continue;
        t.owners.push_back({&l.stencil, &l.stencil_follow});
        t.scans.push_back(i);
        t.names.push_back(fs::path(l.path).filename().string());
        t.paths.push_back(l.path);
        std::vector<std::string> k;
        for (const auto& [camera, cs] : l.stencil.cameras) k.push_back(camera);
        if (k.empty()) k.push_back("");
        keys.push_back(std::move(k));
    }
    // Their own chain: a scan's photographs never follow a video's areas.
    append_rows(t.rows, keys, t.inputs);
    return t;
}

int GuiApp::panel_owner(const StencilTable& t) const {
    if (_mask_preview_scan < 0)
        return _mask_preview_input >= 0 && _mask_preview_input < (int)t.inputs
                   ? _mask_preview_input
                   : -1;
    for (size_t k = 0; k < t.scans.size(); k++)
        if (t.scans[k] == (size_t)_mask_preview_scan) return (int)(t.inputs + k);
    return -1;
}

void GuiApp::draw_stencil_rows(const MaskingPanel& p) {
    if (_stencil_sets_stale) load_stencil_sets();
    const StencilTable t = stencil_table();
    const StencilOwners& owners = t.owners;
    const std::vector<StencilRow>& rows = t.rows;
    if (rows.empty()) return;
    settle_follow(owners, rows);

    // "dome" on an input's first row, "dome/cam1" where a row is named.
    std::vector<std::string> names(rows.size());
    float base_w = 0.0f, cam_w = 0.0f;
    for (size_t r = 0; r < rows.size(); r++) {
        const std::string& base = t.names[rows[r].input];
        names[r] = rows[r].camera.empty() ? base : base + "/" + rows[r].camera;
        base_w = std::max(base_w, ImGui::CalcTextSize(base.c_str()).x);
        cam_w = std::max(cam_w, ImGui::CalcTextSize(rows[r].camera.c_str()).x);
    }
    // The border column takes just over half of what the names leave, so the
    // areas combo keeps its arrow on a narrow pane or in a long language.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float name_w = std::min(base_w, px(150.0f));
    const float rest = std::max(px(240.0f), ImGui::GetContentRegionAvail().x - name_w - cam_w -
                                                8.0f * style.CellPadding.x);
    const float border_w = std::min(rest * 0.55f, px(230.0f));
    const float combo_w = std::floor(border_w * 0.56f);
    const float slider_w = border_w - combo_w - style.ItemSpacing.x;
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("##stencil_rows", 4, flags)) return;
    ui::TableSetupColumn(dmsg::stencil_rows_input, ImGuiTableColumnFlags_WidthFixed, name_w);
    ui::TableSetupColumnRaw("##camera", ImGuiTableColumnFlags_WidthFixed, cam_w);
    ui::TableSetupColumn(dmsg::stencil_rows_border, ImGuiTableColumnFlags_WidthFixed, border_w);
    ui::TableSetupColumn(dmsg::stencil_areas_preset, ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int c = 0; c < 4; c++) {
        if (!ImGui::TableSetColumnIndex(c)) continue;
        ImGui::TableHeader(ImGui::TableGetColumnName(c));
        if (c == 0) ui::help_on_hover(dmsg::stencil_rows_help);
        if (c == 2) ui::help_on_hover(dmsg::stencil_border_help);
        if (c == 3) ui::help_on_hover(dmsg::stencil_areas_help);
    }

    for (size_t r = 0; r < rows.size(); r++) {
        const StencilRow& row = rows[r];
        const bool scan = row.input >= t.inputs;
        const std::string& path = t.paths[row.input];
        // A copy: an edit below rewrites the stencil it would point into.
        const app::CameraStencil cs = row_stencil(owners, row);
        const std::string above = row.above >= 0 ? names[(size_t)row.above] : std::string();
        // A scan's photographs are taken out of it first, into the dataset.
        const bool can_draw =
            !native_work_busy() && (scan ? !_workspace.empty() : p.preview_ready);
        auto open_panel = [&] {
            if (scan) {
                _mask_try_scan = (int)t.scans[row.input - t.inputs];
                open_scan_mask_preview((size_t)_mask_try_scan, row.camera);
            } else {
                _mask_try_scan = -1;
                *p.preview_input = (int)row.input;
                p.open_preview();
                if (_segment.is_open()) _segment.edit_camera(row.camera);
            }
        };
        ImGui::TableNextRow();
        ImGui::PushID(path.c_str());
        ImGui::PushID(row.camera.c_str());

        ImGui::TableNextColumn();
        if (r == 0 || rows[r - 1].input != row.input) {
            ImGui::AlignTextToFramePadding();
            ui::TextRaw(t.names[row.input]);
            if (ImGui::IsItemHovered()) ui::SetTooltipRaw(path);
        }
        ImGui::TableNextColumn();
        if (!row.camera.empty()) {
            ImGui::AlignTextToFramePadding();
            ui::TextDisabledRaw(row.camera);
        }

        ImGui::TableNextColumn();
        const std::vector<size_t> sharers = border_sharers(owners, rows, r);
        std::string sharing;
        for (size_t k : sharers)
            if (k != r) sharing += (sharing.empty() ? "" : ", ") + names[k];
        const char* border_shown = !sharers.empty() ? dmsg::stencil_rows_shared.get()
                                   : cs.detect_border ? dmsg::stencil_rows_cut.get()
                                                      : dmsg::stencil_rows_off.get();
        ImGui::SetNextItemWidth(combo_w);
        if (ui::BeginComboRaw("##border", border_shown)) {
            if (ui::Selectable(dmsg::stencil_rows_cut, sharers.empty() && cs.detect_border))
                set_row_border(owners, rows, r, true, cs.shrink);
            ui::help_on_hover(dmsg::stencil_border_help);
            if (ui::Selectable(dmsg::stencil_rows_off, !cs.detect_border))
                set_row_border(owners, rows, r, false, cs.shrink);
            ui::help_on_hover(dmsg::stencil_rows_off_help);
            // Any input's other row that cuts; a scan's photographs fit their own.
            std::vector<size_t> others;
            for (size_t k = 0; !scan && k < rows.size(); k++)
                if (k != r && rows[k].input < t.inputs &&
                    row_stencil(owners, rows[k]).detect_border)
                    others.push_back(k);
            if (!scan) {
                ImGui::Separator();
                if (ui::BeginMenu(dmsg::stencil_rows_share_with, !others.empty())) {
                    for (size_t k : others) {
                        ImGui::PushID((int)k);
                        const bool joined =
                            std::find(sharers.begin(), sharers.end(), k) != sharers.end();
                        if (ui::MenuItemRaw(names[k].c_str(), joined) && !joined)
                            share_border(owners, rows, r, k);
                        ImGui::PopID();
                    }
                    ImGui::EndMenu();
                } else {
                    // Only while closed: an open submenu is where the tooltip would go.
                    ui::help_on_hover_disabled(dmsg::stencil_rows_share_help);
                }
            }
            ImGui::EndCombo();
        }
        if (!sharers.empty()) ui::help_on_hover(dmsg::stencil_rows_shared_help, {sharing});
        else ui::help_on_hover(cs.detect_border ? dmsg::stencil_border_help
                                                : dmsg::stencil_rows_off_help);
        if (cs.detect_border) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(slider_w);
            float pct = cs.shrink * 100.0f;
            if (ui::SliderFloatRaw("##shrink", &pct, -10.0f, 30.0f, "%.1f%%",
                                   ImGuiSliderFlags_AlwaysClamp))
                set_row_shrink(owners, rows, r, pct / 100.0f);
            ui::help_on_hover(dmsg::stencil_shrink_help);
        }

        ImGui::TableNextColumn();
        const bool areas_follow = row_follows(owners, rows, r);
        const int set = row_set(_stencil_set_shapes, row, cs);
        const std::string areas_shown =
            areas_follow           ? std::string(dmsg::lens_same_as_above.get())
            : cs.mask.shapes.empty() ? std::string(dmsg::stencil_rows_none.get())
            : set >= 0             ? stencil_preset_label(_stencil_sets[(size_t)set])
                                   : std::string(dmsg::stencil_rows_drawn.get());
        std::optional<StencilPreset> edit;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ui::BeginComboRaw("##areas", areas_shown.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (row.above >= 0) {
                if (ui::Selectable(dmsg::lens_same_as_above, areas_follow))
                    follow_above(owners, rows, r);
                ui::help_on_hover(dmsg::stencil_rows_same_help, {above});
                ImGui::Separator();
            }
            if (ui::Selectable(dmsg::stencil_rows_none, !areas_follow && cs.mask.shapes.empty()))
                set_row_areas(owners, rows, r, app::MaskSet{});
            if (!areas_follow && !cs.mask.shapes.empty() && set < 0)
                ui::Selectable(dmsg::stencil_rows_drawn, true);
            // A per-camera set only on an input with one of its cameras.
            for (size_t k = 0; k < _stencil_sets.size(); k++) {
                const app::MaskSet& ms = _stencil_set_shapes[k];
                if (ms.empty()) continue;
                bool fits = !ms.per_camera();
                for (const StencilRow& other : rows)
                    fits = fits || (other.input == row.input && ms.cameras.count(other.camera));
                if (!fits) continue;
                if (ui::SelectableRaw(stencil_preset_label(_stencil_sets[k]),
                                      !areas_follow && set == (int)k))
                    set_row_areas(owners, rows, r, ms);
                if (_stencil_set_menu.item(_stencil_sets[k], can_draw)) edit = _stencil_sets[k];
            }
            ImGui::Separator();
            ImGui::BeginDisabled(!can_draw);
            if (ui::Selectable(dmsg::stencil_rows_draw, false)) open_panel();
            ImGui::EndDisabled();
            ui::help_on_hover_disabled(dmsg::stencil_rows_draw_help);
            if (edit) ImGui::CloseCurrentPopup();
            ImGui::EndCombo();
        }
        if (areas_follow) ui::help_on_hover(dmsg::stencil_rows_same_help, {above});
        else ui::help_on_hover(dmsg::stencil_areas_help);
        if (edit) {
            const SegmentPanel::EditRequest request{*edit, row.camera};
            if (scan) _scan_edit = request;
            open_panel();
            if (!scan && _segment.is_open()) _segment.edit_saved(request);
        }

        ImGui::PopID();
        ImGui::PopID();
    }
    ImGui::EndTable();
    // Every frame, so removing an input or a saved set is seen too.
    refresh_frame_shapes(*p.inputs, *p.frame_shapes);
}

void GuiApp::draw_stencil_set_dialogs() {
    std::string message;
    bool failed = false;
    if (_stencil_set_menu.draw_dialogs(message, failed)) {
        _stencil_sets_stale = true;
        // Rows naming the set as it was would fail at launch.
        _batch_checked = false;
    }
    if (!message.empty()) log(message);
}

MaskModelFiles GuiApp::selected_mask_model() const {
    return cached_mask_model(_model_id, _mask_detector_id);
}

// Fetch the selected checkpoint, from wherever the screen offers it. Consent
// first, every time the family has not been agreed to (ModelCache.h says why):
// the checkpoint's, then the detector's, one modal each, and then the files.
void GuiApp::request_model_download(const std::string& id, const std::string& detector_id) {
    const ModelEntry* e = find_model(id);
    if (!e || _download.state() == ModelDownload::State::Running) return;
    _license_notice.clear();
    const TextDetector* d = detector_for(*e, detector_id);
    const std::string families[] = {model_is_cached(*e) ? "" : e->family,
                                    d && !detector_is_cached(*d) ? spirula::license::family::kGdino : ""};
    for (const std::string& family : families)
        if (!family.empty() && !license_accepted(family)) {
            _license_prompt = family;
            _license_model_id = id;
            _license_detector_id = detector_id;
            _license_tick = false;
            return;
        }
    if (_download.start(*e, d)) {
        _download_model_id = id;
        _download_detector_id = detector_id;
    }
}

// Would the run reach the built-in masker and find no checkpoint? That is a
// download, not something the run can do anything about, so it is asked before
// starting rather than reported twenty minutes in.
bool GuiApp::mask_model_missing() const {
    return mask_model_missing(_mask_enable, _sources);
}

bool GuiApp::mask_model_missing(bool enable, const std::vector<PrepInput>& inputs) const {
    if (!enable) return false;
    // Inputs that arrived with their own masks are never segmented, so a job
    // made only of those needs no model at all.
    bool all_bring_masks = !inputs.empty();
    for (const PrepInput& s : inputs)
        all_bring_masks = all_bring_masks && !s.mask_dir.empty();
    if (all_bring_masks) return false;
    return selected_mask_model().empty();
}

void GuiApp::draw_model_fetch(FileDownload& dl, const Msg& missing, const Msg& get,
                              const std::function<void()>& request) {
    ImGui::SameLine();
    ui::TextDisabled(missing);
    ImGui::SameLine();
    if (dl.state() == FileDownload::State::Running)
        download_bar(dl, dl.status());
    else if (ui::Button(get))
        request();
    if (!_license_notice.empty()) {
        ImGui::SameLine();
        ui::TextDisabledRaw(_license_notice.c_str());
    }
}

// ---------------------------------------------------------------------------
// Masking
// ---------------------------------------------------------------------------

PreviewSource GuiApp::preview_source(size_t input) const {
    if (input >= _sources.size()) return PreviewSource{};
    const PrepInput& in = _sources[input];
    return preview_source_for(in, in.is_video ? input_lut(_sources, input) : std::string());
}

PreviewSource GuiApp::preview_source_for(const PrepInput& in, const std::string& lut) const {
    PreviewSource src;
    src.input = in.path;
    src.is_video = in.is_video;
    src.ffmpeg_exe = _ffmpeg_exe;
    // In process where the driver can, ffmpeg where it cannot or where the job
    // said to -- the choice preparation itself makes.
    src.builtin_decode =
        !_sfm_job.prep.force_external_decode && backends().builtin_video;
    src.tracks = std::max(in.video_tracks, 1);
    src.lens = in.active_lens;
    src.look.packed_lenses = in.packed_lenses;
    src.look.lut = lut;
    // The one frozen choice, so a preview decodes and segments on the GPU the
    // run will use; empty leaves the panel's own precedence in charge.
    src.device = _native_device_uuid;
    const spirula::ImageColor color = in.color.value_or(
        spirula::ImageColor{_sfm_job.image_gamut, _sfm_job.image_is_linear});
    src.image_gamut = color.gamut;
    src.image_is_linear = color.linear;
    src.image_exposure = _sfm_job.image_exposure;
    src.look.auto_rotate = _sfm_job.prep.auto_rotate;
    src.clip_start = in.clip_start;
    src.clip_end = in.clip_end;
    if (in.pano360.valid() && _sfm_job.prep.pano.mode != app::Pano360Mode::Off) {
        src.look.eac = in.pano360;
        src.look.views = app::pano360_views(in.pano360, _sfm_job.prep.pano);
    }
    return src;
}

void GuiApp::open_mask_preview() {
    if (native_work_busy()) return;
    _mask_preview_scan = -1;
    if (_sources.empty()) {
        log(dmsg::mask_pick_input_first.get());
        return;
    }
    pump_source_probes();
    if (!_source_probes_ready) {
        log(dmsg::sensors_reading.get());
        return;
    }
    // A preview decodes and segments on the GPU, so it is a GPU-consuming
    // operation like the run itself: it freezes the same one choice first.
    // The other preview owns the same process-wide inference pool.
    stop_inference_users();
    close_splat();
    if (!freeze_native_device()) return;
    std::optional<PreviewFrame> initial;
    if (selected_source() == _mask_preview_input) initial = _source_view.selected_frame();
    open_segment(preview_source((size_t)_mask_preview_input), std::move(initial));
}

void GuiApp::draw_dataset_model_picker() {
    const ModelEntry* before = find_model(_model_id);
    draw_mask_model_picker(_model_id, &_mask_detector_id, _download, [this] {
        request_model_download(_model_id, _mask_detector_id);
    });
    const ModelEntry* entry = find_model(_model_id);
    const bool was_subject = before && before->kind == MaskModelKind::Subject;
    const bool is_subject = entry && entry->kind == MaskModelKind::Subject;
    // BiRefNet is for object captures, so picking it means keeping what it
    // finds; leaving it for an empty SAM prompt goes back to removing.
    if (is_subject && !was_subject) _mask.keep_subject = true;
    if (was_subject && !is_subject && _mask.prompt.empty()) _mask.keep_subject = false;
}

void GuiApp::open_segment(const PreviewSource& src, std::optional<PreviewFrame> initial) {
    // A click or a prompt with no model asks for one in the panel; taking it
    // ticks masking, so the run does what the panel shows.
    _segment.set_model_picker([this] {
        draw_dataset_model_picker();
        const MaskModelFiles m = selected_mask_model();
        ImGui::BeginDisabled(m.empty());
        const bool use = ui::Button(dmsg::preview_use_model);
        ImGui::EndDisabled();
        if (!use) return MaskModelFiles{};
        _mask_enable = true;
        return m;
    });
    _segment.open(src, _mask_enable ? selected_mask_model() : MaskModelFiles{},
                  std::move(initial));
}

GuiApp::MaskingPanel GuiApp::dataset_masking_panel() {
    MaskingPanel p;
    p.inputs = &_sources;
    p.enable = &_mask_enable;
    p.border = &_border_enable;
    p.features = &_mask_features;
    p.preview_input = &_mask_preview_input;
    p.frame_shapes = &_frame_shapes;
    p.preview_ready = _source_probes_ready;
    p.preview_wait = &dmsg::sensors_reading;
    p.open_preview = [this] { open_mask_preview(); };
    return p;
}

void GuiApp::draw_masking_options(const MaskingPanel& p) {
    std::vector<PrepInput>& inputs = *p.inputs;
    // Masks that came with an input need no checkbox and no model. Say so here,
    // or a user turns masking on to "make sure" and waits twenty minutes for
    // masks they already had.
    int with_masks = 0;
    for (const PrepInput& s : inputs)
        if (!s.mask_dir.empty()) with_masks++;
    if (with_masks > 0) {
        if (with_masks == (int)inputs.size())
            ui::TextColoredWrapped(kOk, dmsg::masks_found_all);
        else
            ui::TextColoredWrapped(kOk, dmsg::masks_found_some,
                                   {with_masks, (int)inputs.size()});
    }

    ImGui::BeginDisabled(!backends().builtin_masking);
    ui::Checkbox(dmsg::mask_enable, p.enable);
    ImGui::EndDisabled();
    if (backends().builtin_masking) {
        ui::help_on_hover(dmsg::mask_enable_help);
    } else {
        ui::TextDisabledWrapped(dmsg::mask_objects_need_segmentation);
        ui::help_on_hover_raw(backends().masking_reason.c_str());
    }

    // A sibling, not a child: the stencil is geometry, so it works with no
    // model downloaded and on a build with no segmentation in it at all.
    if (ui::Checkbox(dmsg::mask_border_enable, p.border) && *p.border && p.fit_lens_border) {
        // Ticking it has to do something on its own. The fisheye border is
        // what it is for, so an input with nothing drawn on it yet gets the
        // fit, unless it is a GoPro's sphere, whose views have no border.
        for (PrepInput& in : inputs)
            if (in.stencil.empty() && !in.pano360.valid() && !is_pano360_path(in.path))
                in.stencil.detect_border = true;
    }
    ui::help_on_hover(dmsg::mask_border_enable_help);
    if (*p.border) {
        ImGui::Indent();
        draw_stencil_rows(p);
        ImGui::Unindent();
    }

    // Asked wherever the dataset ends up with masks at all, including ones
    // that arrived with the photographs: what it decides is the reconstruction,
    // not whether they are written.
    if (p.features && (*p.enable || *p.border || with_masks > 0)) {
        ui::Checkbox(dmsg::mask_for_features, p.features);
        ui::help_on_hover(dmsg::mask_for_features_help);
    }
    if (!*p.enable && !*p.border) return;

    ImGui::Indent();

    const ModelEntry* entry = find_model(_model_id);

    if (*p.enable) {
        draw_dataset_model_picker();
        entry = find_model(_model_id);
        const bool is_subject = entry && entry->kind == MaskModelKind::Subject;
        if (is_subject) {
            ImGui::PushTextWrapPos(px(560.0f));
            ui::TextDisabled(dmsg::mask_subject_note);
            ImGui::PopTextWrapPos();
        } else if (!selected_mask_model().text && _mask.clicks.empty()) {
            ui::TextColored(kWarn, dmsg::mask_no_text_prompts);
        }
        if (!is_subject && !_mask.clicks.empty()) {
            int objects = 0;
            for (const MaskClick& c : _mask.clicks)
                objects = std::max(objects, c.object + 1);
            ui::Text(dmsg::mask_clicked_objects, {objects});
            ImGui::SameLine();
            if (ui::SmallButton(dmsg::mask_forget_clicks)) {
                _mask.clicks.clear();
                _mask.object_count = 1;
                _mask.current_object = 0;
            }
            ui::help_on_hover(dmsg::mask_forget_clicks_help);
            const std::string unprompted =
                inputs_without_clicks(inputs, _mask.clicks);
            if (!unprompted.empty() && (_mask.prompt.empty() || !selected_mask_model().text))
                ui::TextColoredWrapped(kWarn, dmsg::mask_inputs_need_clicks,
                                       {unprompted});
        }
    }

    // A scan whose photographs join the inputs is one more to try it on.
    const StencilTable tried = stencil_table();
    int tried_scan = -1;
    for (size_t k = 0; k < tried.scans.size(); k++)
        if ((int)tried.scans[k] == _mask_try_scan) tried_scan = (int)k;
    const bool mask_preview_busy = native_work_busy();
    const bool mask_preview_blocked =
        mask_preview_busy || (tried_scan >= 0 ? _workspace.empty() : !p.preview_ready);
    ImGui::BeginDisabled(mask_preview_blocked);
    if (ui::Button(dmsg::mask_try)) {
        if (tried_scan >= 0) open_scan_mask_preview((size_t)_mask_try_scan);
        else p.open_preview();
    }
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(
        mask_preview_busy ? dmsg::step_locked
        : tried_scan >= 0 && _workspace.empty() ? dmsg::pick_input_first
        : tried_scan < 0 && !p.preview_ready && p.preview_wait ? *p.preview_wait
                                                               : dmsg::mask_try_help);
    // Which input it opens, and so which input a NEW click prompts; the ones
    // already made stay with their own (MaskClick::source).
    if (tried.names.size() > 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(kFieldW));
        std::vector<const char*> names;
        for (const std::string& n : tried.names) names.push_back(n.c_str());
        const int shown = tried_scan >= 0 ? (int)(tried.inputs + (size_t)tried_scan)
                                          : *p.preview_input;
        int pick = shown;
        if (ui::ComboRaw(ui::detail::label(dmsg::mask_on_input), &pick,
                         names.data(), (int)names.size()) &&
            pick != shown) {
            // An open panel is showing the old input's frames and editing its
            // stencil; point both at the new one.
            if ((size_t)pick < tried.inputs) {
                _mask_try_scan = -1;
                *p.preview_input = pick;
                if (_segment.is_open()) p.open_preview();
            } else {
                _mask_try_scan = (int)tried.scans[(size_t)pick - tried.inputs];
                if (_segment.is_open()) open_scan_mask_preview((size_t)_mask_try_scan);
            }
        }
        ui::help_on_hover(dmsg::mask_on_input_help);
    }

    if (!*p.enable) {
        ImGui::Unindent();
        return;
    }

    // Polarity above the two prompts, which are labelled by it -- the same
    // box means "take this out" or "this is the subject" depending on the
    // radio. Same order and same wording as the preview panel.
    const MaskModelFiles picked = selected_mask_model();
    const MaskModelKind kind = picked.kind;
    const bool subject = kind == MaskModelKind::Subject;
    int polarity = _mask.keep_subject ? 1 : 0;
    if (subject) {
        // No prompt to label: the choice is about the one thing it finds.
        if (ui::RadioButton(dmsg::mask_subject_keep, polarity == 1)) polarity = 1;
        ImGui::SameLine();
        if (ui::RadioButton(dmsg::mask_subject_remove, polarity == 0)) polarity = 0;
        _mask.keep_subject = polarity == 1;
        ui::help_on_hover(dmsg::mask_subject_polarity_help);
    } else {
        if (ui::RadioButton(dmsg::mask_remove_named, polarity == 0)) polarity = 0;
        ImGui::SameLine();
        if (ui::RadioButton(dmsg::mask_keep_named, polarity == 1)) polarity = 1;
        _mask.keep_subject = polarity == 1;
        ui::help_on_hover(dmsg::mask_polarity_help);
    }
    const bool keep_subject = _mask.keep_subject;
    if (subject) {
        if (ui::CollapsingHeader(dmsg::mask_advanced)) {
            ImGui::SetNextItemWidth(px(kFieldW));
            if (ui::InputInt(dmsg::mask_max_size, &_mask.max_image_size))
                _mask.max_image_size = std::max(0, _mask.max_image_size);
            ui::help_on_hover(dmsg::mask_max_size_help);
            draw_margin_slider(_mask.dilate_ratio, _mask.shrink_ratio, keep_subject,
                               px(220.0f), /*inline_label=*/true);
        }
        ImGui::Unindent();
        return;
    }
    const bool grounded = kind == MaskModelKind::Grounded;
    // Clicks only: no boxes to type into, and no scores to threshold.
    const bool text = picked.text;

    // English, whatever the interface language is -- see MaskPrompt.h. The
    // placeholder examples are English for the same reason.
    if (text) {
        ImGui::SetNextItemWidth(px(kTextFieldW));
        ui::InputTextEnglish(
            keep_subject ? dmsg::mask_what_to_keep : dmsg::mask_what_to_remove,
            keep_subject ? "the statue; its pedestal" : "tourist; pedestrian; child; person; car; shadow of a person",
            &_mask.prompt);
        ui::help_on_hover(keep_subject ? dmsg::mask_prompt_help_keep
                                       : dmsg::mask_prompt_help_remove);
        ImGui::SetNextItemWidth(px(kTextFieldW));
        ui::InputTextEnglish(
            keep_subject ? dmsg::mask_but_remove : dmsg::mask_but_keep,
            keep_subject ? "the hand holding it" : "person in a painting",
            &_mask.negative_prompt);
        ui::help_on_hover(keep_subject ? dmsg::mask_negative_help_keep
                                       : dmsg::mask_negative_help_remove);

        // Only worth saying when the interface is not already English: for an
        // English user the box being English is not news.
        if (i18n::current() != i18n::Lang::en) {
            ImGui::PushTextWrapPos(px(560.0f));
            ui::TextDisabled(dmsg::mask_english_only);
            ImGui::PopTextWrapPos();
        }
        draw_subject_palette(_mask.prompt, _mask.negative_prompt, keep_subject);
    }

    if (ui::CollapsingHeader(dmsg::mask_advanced)) {
        if (text) {
            ImGui::SetNextItemWidth(px(kTextFieldW));
            ui::InputTextEnglish(dmsg::mask_features_only, "sky; cloud",
                                 &_mask.feature_prompt);
            ui::help_on_hover(dmsg::mask_features_only_help);
        }
        // The preview reads these off the same fields. Grounding DINO scores
        // on its own scale, so the slider is its threshold there, and it keeps
        // every box, so there is no NMS to set.
        if (text) {
            ImGui::SetNextItemWidth(px(kFieldW));
            ui::SliderFloat(dmsg::mask_threshold,
                            grounded ? &_mask.box_threshold : &_mask.threshold, 0.05f, 0.95f,
                            "%.2f");
            ui::help_on_hover(dmsg::mask_threshold_help);
        }
        if (text && !grounded) {
            ImGui::SetNextItemWidth(px(kFieldW));
            ui::SliderFloat(dmsg::mask_nms, &_mask.nms, 0.05f, 0.95f, "%.2f");
            ui::help_on_hover(dmsg::mask_nms_help);
        }
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::InputInt(dmsg::mask_max_size, &_mask.max_image_size))
            _mask.max_image_size = std::max(0, _mask.max_image_size);
        ui::help_on_hover(dmsg::mask_max_size_help);
        draw_margin_slider(_mask.dilate_ratio, _mask.shrink_ratio, keep_subject, px(220.0f),
                           /*inline_label=*/true);

        // The rest is the memory bank, which photos never get.
        bool any_video = false;
        for (const PrepInput& s : inputs) any_video = any_video || s.is_video;
        if (any_video) {
            // A clicked object means nothing on any frame but its own without
            // the bank, so it forces the option on and the box shows what will
            // run.
            const bool forced = !_mask.clicks.empty();
            bool memory = _mask_memory || forced;
            ImGui::BeginDisabled(forced);
            if (ui::Checkbox(dmsg::mask_memory, &memory)) _mask_memory = memory;
            ImGui::EndDisabled();
            ui::help_on_hover_disabled(dmsg::mask_memory_help);

            ImGui::Indent();
            ImGui::BeginDisabled(!memory);
            // Grounding DINO searches every frame; only clicks are tracked.
            if (!grounded) {
                ImGui::SetNextItemWidth(px(200.0f));
                if (ui::InputInt(dmsg::mask_detect_every, &_mask_detect_every))
                    _mask_detect_every = std::clamp(_mask_detect_every, 1, 1000);
                ui::help_on_hover_disabled(dmsg::mask_detect_every_help);
            }
            ImGui::SetNextItemWidth(px(200.0f));
            if (ui::InputInt(dmsg::mask_memory_frames, &_mask_memory_frames))
                _mask_memory_frames = std::clamp(_mask_memory_frames, 0, 7);
            ui::help_on_hover_disabled(dmsg::mask_memory_frames_help);
            ImGui::EndDisabled();
            ImGui::Unindent();
        }
    }

    ImGui::Unindent();
}

}  // namespace gui
