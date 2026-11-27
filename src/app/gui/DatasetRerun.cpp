// What a dataset run will reuse and redo, and starting over.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/Ui.h"
#include "core/LongPath.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Lidar.h"
#include "i18n/catalog/SourcePreview.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
namespace ldmsg = spirula::i18n::msg::lidar;
namespace svmsg = spirula::i18n::msg::preview;
using spirula::i18n::Msg;

namespace gui {

// Re-doing one step of what is already in the output folder, instead of the
// whole run. The rules are probe_workspace's; this only names them.
void GuiApp::draw_dataset_rerun(const WorkspaceState& prior) {
    if (!prior.resumable() && !prior.model && !prior.geometry && !prior.dense) return;
    if (!ui::CollapsingHeader(dmsg::rerun_section)) return;
    ImGui::Indent();
    ui::TextDisabledWrapped(dmsg::rerun_section_help);

    // Each button starts a run of its own, so each is behind exactly the
    // checkpoints its steps read -- a missing geometry model must not stop a
    // rerun of the masks.
    const bool need_mask_model = mask_model_missing();
    const bool need_feat_model = feature_model_missing();

    bool go = false;
    if (prior.frames) {
        ImGui::BeginDisabled(need_mask_model || need_feat_model);
        if (ui::Button(dmsg::rerun_frames)) {
            _redo_frames = _redo_masks = true;   // the masks describe the frames
            _redo_model = go = true;
        }
        ImGui::EndDisabled();
        if (need_mask_model || need_feat_model)
            ui::help_on_hover_disabled(need_mask_model ? dmsg::mask_model_first
                                                       : dmsg::feat_model_first);
        ImGui::SameLine();
    }
    if (prior.masks) {
        ImGui::BeginDisabled(need_mask_model);
        if (ui::Button(dmsg::rerun_masks)) {
            _redo_masks = true;
            _redo_model = go = true;
        }
        ImGui::EndDisabled();
        if (need_mask_model) ui::help_on_hover_disabled(dmsg::mask_model_first);
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(need_feat_model);
    if (ui::Button(dmsg::rerun_model)) {
        _redo_model = go = true;
    }
    ImGui::EndDisabled();
    if (need_feat_model) ui::help_on_hover_disabled(dmsg::feat_model_first);
    // Depth and normals are the one step that reruns on its own: they are read
    // off the finished dataset and nothing downstream of them exists.
    if (prior.geometry) {
        ImGui::SameLine();
        const bool need_geom_model = geometry_model_missing();
        ImGui::BeginDisabled(!_geometry.enable || need_geom_model);
        if (ui::Button(dmsg::rerun_geometry)) {
            _redo_geometry = go = true;
        }
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(need_geom_model ? dmsg::geom_model_first
                                                   : dmsg::rerun_geometry_help);
    }
    if (prior.dense) {
        ImGui::SameLine();
        const bool need_dense_model = dense_model_missing();
        ImGui::BeginDisabled(!_dense.enable || need_dense_model);
        if (ui::Button(spirula::i18n::msg::dense::rerun)) _redo_dense = go = true;
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(need_dense_model ? spirula::i18n::msg::dense::model_first
                                                    : spirula::i18n::msg::dense::rerun_help);
    }
    ImGui::NewLine();
    ImGui::Unindent();
    // Every route but the last re-reconstructs: a model built from frames or
    // masks that have just been replaced describes neither.
    if (go) start_dataset_job();
}

// ---------------------------------------------------------------------------
// Starting over
// ---------------------------------------------------------------------------

void GuiApp::reset_recon_options() {
    // Keep the source color defaults and the application's intermediate-file preference.
    const std::string gamut = _sfm_job.image_gamut;
    const std::optional<bool> linear = _sfm_job.image_is_linear;
    const bool keep = _sfm_job.keep_intermediate;
    _sfm_job = SfmJob{};
    _colmap_job = ColmapJob{};
    _geometry = GeometryJob{};
    _dense = DenseJob{};
    _dense_config_error.clear();
    _sfm_job.image_gamut = gamut;
    _sfm_job.image_is_linear = linear;
    _sfm_job.keep_intermediate = keep;
    for (PrepInput& s : _sources) {
        s.camera_model = default_lens(s);
        s.focal_factor = 0.0f;
        for (SubCamera& sc : s.subcameras) {
            sc.camera_model = s.camera_model;
            sc.focal_factor = 0.0f;
        }
    }
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    normalize_source_lenses(_sources, _sfm_job.camera_model);
    forget_redo_requests();
    _keep_built = false;
    _resume = true;
    log(dmsg::reset_options_done.get());
}

void GuiApp::draw_dataset_reset() {
    if (!ui::CollapsingHeader(dmsg::reset_section)) return;
    ImGui::Indent();
    ui::TextDisabledWrapped(dmsg::reset_section_help);
    ImGui::BeginDisabled(native_work_busy());
    // A transforms.json or a Metashape export is not on the list -- it is the
    // dataset somebody handed over, not something a run here wrote -- so a
    // folder can hold a model and still have nothing to delete.
    ImGui::BeginDisabled(_ws_artifacts.empty());
    if (ui::Button(dmsg::clear_project)) {
        _clear_targets = _ws_artifacts;
        _clear_open = true;
    }
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::clear_project_help);
    ImGui::SameLine();
    if (ui::Button(dmsg::reset_options)) reset_recon_options();
    ui::help_on_hover(dmsg::reset_options_help);
    ImGui::EndDisabled();
    ImGui::NewLine();
    ImGui::Unindent();
}

// Deleting is the one thing here that cannot be undone, so the modal lists the
// paths themselves rather than describing them.
void GuiApp::draw_clear_project_modal() {
    if (_clear_open) {
        ui::OpenPopup(dmsg::clear_project_title);
        _clear_open = false;
        _clear_shown = true;
    }
    if (!_clear_shown) return;
    if (!ui::BeginPopupModal(dmsg::clear_project_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _clear_shown = false;
        return;
    }
    ImGui::PushTextWrapPos(px(kModalWrapW));
    ui::Text(dmsg::clear_project_confirm, {_workspace});
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    for (const std::string& path : _clear_targets) ui::TextDisabledRaw(path);
    ImGui::Spacing();

    if (ui::Button(dmsg::clear_project_button, ImVec2(px(kModalButtonW), 0)) &&
        !native_work_busy()) {
        // The screen is reading several of these; it has to let go first.
        reset_dataset_preview();
        for (const std::string& path : _clear_targets) {
            std::error_code ec;
            fs::remove_all(spirula::long_path(path), ec);
            if (ec) log(i18n::format(dmsg::clear_project_failed,
                                     {path, ec.message()}));
        }
        log(i18n::format(dmsg::clear_project_done, {_workspace}));
        _clear_targets.clear();
        _ws_state_at = -1.0;      // the answer changed; do not wait a second
        _clear_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(kModalButtonW), 0))) {
        _clear_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::draw_drop_intermediate_modal() {
    if (_drop_intermediate_open) {
        ui::OpenPopup(dmsg::drop_intermediate_title);
        _drop_intermediate_open = false;
        _drop_intermediate_shown = true;
    }
    if (!_drop_intermediate_shown) return;
    // Closed by the window's own means rather than by a button: the setting
    // stays as it was, which is the safe half of the question.
    if (!ui::BeginPopupModal(dmsg::drop_intermediate_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _drop_intermediate_shown = false;
        _sfm_job.keep_intermediate = true;
        return;
    }
    ImGui::PushTextWrapPos(px(kModalWrapW));
    ui::Text(dmsg::drop_intermediate_confirm);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ui::Button(dmsg::drop_intermediate_button, ImVec2(px(kModalButtonW), 0))) {
        _sfm_job.keep_intermediate = false;
        save_settings();
        _drop_intermediate_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(kModalButtonW), 0))) {
        _sfm_job.keep_intermediate = true;
        _drop_intermediate_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// What the run will reuse and redo
// ---------------------------------------------------------------------------

namespace {

const Msg& plan_step_name(Step s) {
    switch (s) {
        case Step::Frames:   return dmsg::step_frames;
        case Step::Masks:    return dmsg::step_masks;
        case Step::Model:    return dmsg::plan_step_model;
        case Step::Geometry: return dmsg::view_geometry;
        case Step::Dense: return spirula::i18n::msg::dense::title;
    }
    return dmsg::step_frames;
}

const Msg& plan_state(const StepPlan& s) {
    switch (s.act) {
        case Act::None:  break;
        case Act::Run:
            if (s.why == Why::Resume) return dmsg::plan_finish;
            return s.adds || s.why == Why::Unrecorded ? dmsg::plan_add : dmsg::plan_run;
        case Act::Reuse:
            if (s.sort) return dmsg::plan_sort;
            if (s.why == Why::InDataset) return dmsg::plan_in_dataset;
            if (s.why == Why::Unrecorded) return dmsg::plan_unrecorded;
            return s.masks_changed ? dmsg::plan_reuse_masks_changed : dmsg::plan_reuse;
        case Act::Keep:  return dmsg::plan_keep;
        case Act::Redo:
            switch (s.why) {
                case Why::Requested: return dmsg::plan_redo_requested;
                case Why::Frames:    return dmsg::plan_redo_frames;
                case Why::Model:     return dmsg::plan_redo_model;
                case Why::Features:  return dmsg::plan_redo_features;
                case Why::Matches:   return dmsg::plan_redo_matches;
                case Why::Masks:     return dmsg::plan_redo_masked;
                case Why::Moved:     return dmsg::plan_redo_moved;
                case Why::Stale:     return dmsg::plan_redo_stale;
                case Why::Resume:    return dmsg::plan_redo_interrupted;
                case Why::Unrecorded: return dmsg::plan_redo_unrecorded;
                default:             return dmsg::plan_redo_settings;
            }
    }
    return dmsg::plan_reuse;
}

const Msg& plan_part_name(ModelPart p) {
    switch (p) {
        case ModelPart::Features: return dmsg::plan_part_features;
        case ModelPart::Matching: return dmsg::step_matching;
        case ModelPart::Mapping:  return dmsg::plan_part_mapping;
        case ModelPart::Align:    return dmsg::plan_part_align;
    }
    return dmsg::plan_part_features;
}

// A stage `spirula sfm` decides by its own signature, with no record of ours
// to predict it from.
const Msg& plan_part_state(const StepPlan& s) {
    if (s.act == Act::Reuse && s.why == Why::Unrecorded) return dmsg::plan_reuse_if_same;
    return plan_state(s);
}

const Msg& plan_lock_text(Lock l) {
    switch (l) {
        case Lock::None:
        case Lock::Nothing:  break;
        case Lock::Frames:   return dmsg::plan_lock_frames;
        case Lock::Frontend: return dmsg::plan_lock_frontend;
        case Lock::Before:   return dmsg::plan_lock_before;
        case Lock::Lens:     return dmsg::plan_lock_lens;
        case Lock::Always:   return dmsg::plan_lock_mapping;
    }
    return dmsg::plan_lock_nothing;
}

// Worth a line each only when they do not all just run.
bool plan_parts_shown(const DatasetPlan& plan) {
    for (const StepPlan& s : plan.parts)
        if (s.act != Act::None && !(s.act == Act::Run && s.why == Why::None)) return true;
    return false;
}

ImVec4 plan_color(const StepPlan& s) {
    if (s.act == Act::Redo || s.act == Act::Keep || s.sort) return kWarn;
    if (s.act == Act::Run) return kOk;
    return kDim;
}

// The panel's own label for a setting where it has one; the identifier
// otherwise, which is what the record spells.
std::string plan_key_label(const std::string& key) {
    static const std::pair<const char*, const Msg*> labels[] = {
        {"quality", &dmsg::quality}, {"lens", &dmsg::camera_lens},
        {"stencil", &dmsg::stencil_section}, {"scan_stencil", &dmsg::stencil_section},
        {"stencil_share", &dmsg::stencil_section},
        {"scan_photos", &ldmsg::use_scan_photos},
        {"camera_mode", &dmsg::camera_sharing}, {"pairs", &dmsg::image_matching},
        {"features", &dmsg::features},
        {"mapper", &dmsg::mapper_schedule}, {"loop_closure", &dmsg::loop_closure},
        {"overlap", &dmsg::sequential_overlap}, {"focal_px", &dmsg::initial_focal_px},
        {"distortion_refine", &dmsg::sfm_distortion_refinement},
        {"final_free_rig", &dmsg::sfm_final_free_rig},
        {"max_features", &dmsg::max_features_auto},
        {"aliked_max_features", &dmsg::max_features_auto},
        {"loma_max_features", &dmsg::max_features_auto},
        {"max_image_size", &dmsg::max_image_size_auto},
        {"prefilter_neighbors", &dmsg::prefilter_neighbors},
        {"sensor_gauge", &dmsg::sfm_sensor_gauge}, {"metric_gps", &dmsg::sfm_metric_gps},
        {"exif_attitude", &dmsg::sfm_exif_attitude},
        {"masks_for_features", &dmsg::mask_for_features},
        {"decoder", &dmsg::use_ffmpeg}, {"adaptive_fps", &dmsg::adaptive_fps},
        {"own_motion_budget", &dmsg::adaptive_fps},
        {"adaptive_range", &dmsg::adaptive_range},
        {"sharp_window", &dmsg::sharpness_window}, {"sync_tracks", &dmsg::sync_lenses},
        {"skip_blurry", &dmsg::skip_blurry}, {"blur_px", &dmsg::blur_px},
        {"blur_neighbour", &dmsg::blur_neighbour},
        {"similar_bits", &dmsg::similar_bits},
        {"pano_size", &dmsg::pano360_size}, {"mask_max_size", &dmsg::mask_max_size},
        {"mask_nms", &dmsg::mask_nms}, {"mask_detect_every", &dmsg::mask_detect_every},
        {"mask_memory_frames", &dmsg::mask_memory_frames},
        {"mask_prompt", &dmsg::mask_what_to_remove},
        {"mask_negative_prompt", &dmsg::mask_but_keep},
        {"feature_prompt", &dmsg::mask_features_only},
        {"mask_threshold", &dmsg::mask_threshold}, {"geometry_model", &dmsg::geom_model},
        {"geometry_max_size", &dmsg::geom_max_size},
        {"geometry_tokens", &dmsg::geom_num_tokens},
        {"jpeg_quality", &dmsg::geom_jpeg_quality}, {"clip", &svmsg::plan_clip},
        {"active_lens", &svmsg::plan_active_lens},
    };
    for (const auto& [k, m] : labels)
        if (key == k) return m->get();
    return key;
}

std::string plan_change_text(const FieldChange& c) {
    auto shown = [](const std::string& v) {
        if (v.empty()) return std::string("-");
        return v[0] == '#' ? std::string("#") : v;
    };
    std::string out = plan_key_label(c.key);
    if (!c.scope.empty() && c.scope != ".") out += " (" + c.scope + ")";
    return out + ": " + shown(c.was) + " → " + shown(c.now);
}

void plan_changes_tooltip(const StepPlan& s) {
    if (s.changes.empty() || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) ||
        !ImGui::BeginTooltip())
        return;
    for (const FieldChange& c : s.changes) ui::TextRaw(plan_change_text(c));
    ImGui::EndTooltip();
}

}  // namespace

// The plan as rows, for the panel and the confirmation alike so the two
// cannot list different runs. True when a setting on screen differs from the
// one something on disk was made with.
bool GuiApp::draw_plan_rows(const DatasetPlan& plan, bool interactive) {
    const bool parts = plan_parts_shown(plan);
    const float indent = ImGui::GetStyle().IndentSpacing;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    // A stage's checkbox, frameless so its row keeps the height of the others.
    const float box = ImGui::GetFontSize() + spacing;
    float label_w = 0.0f;
    for (int k = 0; k < kNumSteps; k++)
        label_w = std::max(label_w, ImGui::CalcTextSize(plan_step_name((Step)k).get()).x);
    for (int k = 0; parts && k < kNumModelParts; k++)
        label_w = std::max(label_w, indent + box + ImGui::CalcTextSize(
                                                       plan_part_name((ModelPart)k).get()).x);
    const float state_x = ImGui::GetCursorPosX() + label_w + spacing * 2.0f;
    bool differs = false;
    auto state = [&](const Msg& text, const StepPlan& sp, bool first_change) {
        ImGui::SameLine(state_x);
        ui::TextColored(plan_color(sp), text);
        plan_changes_tooltip(sp);
        differs = differs || !sp.changes.empty();
        if (!first_change || sp.changes.empty()) return;
        std::string first = plan_change_text(sp.changes[0]);
        if (sp.changes.size() > 1)
            first += "  (+" + std::to_string(sp.changes.size() - 1) + ")";
        ImGui::SameLine();
        ui::TextDisabledRaw(first);
        plan_changes_tooltip(sp);
    };
    // Ticked, the stage runs; what the plan would do, until the user says.
    auto stage = [&](ModelPart part, const StepPlan& sp) {
        ImGui::PushID((int)part);
        ImGui::Indent(indent);
        bool run = makes(sp.act);
        ImGui::BeginDisabled(!interactive || sp.lock != Lock::None);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ui::CheckboxRaw("##run", &run)) {
            if (_part_choice_for != _workspace) {
                std::fill(std::begin(_part_choice), std::end(_part_choice), PartChoice::Auto);
                _part_choice_for = _workspace;
            }
            _part_choice[(int)part] = run ? PartChoice::Run : PartChoice::Keep;
        }
        ImGui::PopStyleVar();
        ImGui::EndDisabled();
        if (!interactive) {}
        else if (sp.lock == Lock::None) ui::help_on_hover(dmsg::plan_part_toggle_help);
        else ui::help_on_hover_disabled(plan_lock_text(sp.lock));
        ImGui::SameLine();
        ui::TextDisabled(plan_part_name(part));
        ImGui::Unindent(indent);
        state(plan_part_state(sp), sp, true);
        ImGui::PopID();
    };
    for (int k = 0; k < kNumSteps; k++) {
        const Step step = (Step)k;
        const StepPlan& sp = plan[step];
        if (sp.act == Act::None) continue;
        // Split, each stage names the settings that reach it instead.
        const bool split = step == Step::Model && parts;
        ui::TextDisabled(plan_step_name(step));
        state(plan_state(sp), sp, !split || !makes(sp.act));
        for (int q = 0; split && q < kNumModelParts; q++)
            if (plan[(ModelPart)q].act != Act::None) stage((ModelPart)q, plan[(ModelPart)q]);
    }
    return differs;
}

void GuiApp::draw_dataset_plan(const DatasetPlan& plan) {
    // Its own ground, or it reads as the end of the form scrolled above it.
    ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    bg.w *= 0.5f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(10.0f), px(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, px(4.0f));
    ImGui::BeginChild("##dsplan", ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    const bool differs = draw_plan_rows(plan, /*interactive=*/true);
    const bool keeping = _keep_built && _keep_built_for == _workspace;
    if (plan.ask() || keeping) {
        bool keep = keeping;
        ImGui::BeginDisabled(!plan[Step::Frames].can_keep);
        if (ui::Checkbox(dmsg::plan_keep_built, &keep)) {
            _keep_built = keep;
            _keep_built_for = _workspace;
        }
        ImGui::EndDisabled();
        if (plan[Step::Frames].can_keep) ui::help_on_hover(dmsg::plan_keep_built_help);
        else ui::help_on_hover_disabled(dmsg::rerun_frames);
    }
    if (differs && _ws_record.settings.is_object()) {
        if (ui::SmallButton(dmsg::plan_use_record)) restore_from_record(true);
        ui::help_on_hover(dmsg::plan_use_record_help);
    }
    ImGui::EndChild();
}

// Making again what the user did not ask to: the frames, or the hour the
// reconstruction took. Named step by step, with what changed, before it starts.
void GuiApp::draw_rebuild_confirm_modal() {
    if (_rebuild_open) {
        ui::OpenPopup(dmsg::rebuild_title);
        _rebuild_open = false;
        _rebuild_shown = true;
    }
    if (!_rebuild_shown) return;
    if (!ui::BeginPopupModal(dmsg::rebuild_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _rebuild_shown = false;
        return;
    }
    ImGui::PushTextWrapPos(px(520.0f));
    ui::Text(dmsg::rebuild_confirm, {_workspace});
    ImGui::Spacing();
    for (int k = 0; k < kNumSteps; k++) {
        const StepPlan& sp = _rebuild_plan[(Step)k];
        if (!sp.ask) continue;
        ui::TextColored(kWarn, plan_step_name((Step)k));
        ImGui::Indent();
        ui::Text(plan_state(sp));
        for (size_t c = 0; c < sp.changes.size() && c < 12; c++)
            ui::TextDisabledRaw(plan_change_text(sp.changes[c]));
        if (sp.changes.size() > 12) ui::TextDisabledRaw("...");
        ImGui::Unindent();
    }
    ImGui::PopTextWrapPos();
    // Every step, not only the ones asked about: what follows from them runs too.
    ImGui::Spacing();
    ui::TextDisabled(dmsg::rebuild_whole_run);
    draw_plan_rows(_rebuild_plan, /*interactive=*/false);
    ImGui::Spacing();

    bool go = false, consent = false;
    if (ui::Button(dmsg::rebuild_go, ImVec2(px(kModalButtonW), 0))) {
        _keep_built = false;
        _keep_built_for.clear();
        consent = !_rebuild_plan[Step::Frames].can_keep && !_redo_frames;
        if (consent) _redo_frames = true;
        go = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!_rebuild_plan[Step::Frames].can_keep);
    if (ui::Button(dmsg::rebuild_keep, ImVec2(px(240.0f), 0))) {
        _keep_built = true;
        _keep_built_for = _workspace;
        go = true;
    }
    ImGui::EndDisabled();
    if (_rebuild_plan[Step::Frames].can_keep) ui::help_on_hover(dmsg::plan_keep_built_help);
    else ui::help_on_hover_disabled(dmsg::rerun_frames);
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(kModalButtonW), 0))) {
        _rebuild_shown = false;
        ImGui::CloseCurrentPopup();
    }
    if (go) {
        _rebuild_shown = false;
        ImGui::CloseCurrentPopup();
        // A launch that did not start must not leave the consent armed.
        if (!launch_dataset_job() && consent) _redo_frames = false;
    }
    ImGui::EndPopup();
}

void GuiApp::restore_from_record(bool announce) {
    if (_workspace.empty() || native_work_busy()) return;
    DatasetRecord rec = read_dataset_record(_workspace);
    DatasetSettings s = capture_dataset_settings();
    if (rec.settings.is_object()) {
        // A record is written by whichever build ran last; one it cannot
        // read is reported, never a reason to close the application.
        try {
            read_dataset_settings_json(rec.settings, s);
        } catch (const std::exception& e) {
            log(i18n::format(dmsg::log_settings_unreadable, {_workspace, e.what()}));
            return;
        }
    } else {
        // A folder from before the record: what its stamps still say.
        rec = read_legacy_settings(_workspace, s.sfm, s.colmap_engine);
        if (!rec.present) return;
    }
    apply_dataset_settings(s);
    restore_record_rows(rec);
    // What the picker last applied no longer describes the panel, and a
    // built-in left armed would be put back over it by the next probe.
    _ds_presets.file.clear();
    _ds_presets.builtin.clear();
    _ds_presets.display.clear();
    _ds_presets.desc.clear();
    _ds_presets.msg_err = false;
    _ds_presets.msg = i18n::format(dmsg::log_settings_restored, {_workspace});
    _keep_built = false;
    _ws_state_at = -1.0;
    if (announce) log(_ds_presets.msg);
}

void GuiApp::forget_redo_requests() {
    _redo_frames = _redo_masks = _redo_model = _redo_geometry = _redo_dense = false;
    std::fill(std::begin(_part_choice), std::end(_part_choice), PartChoice::Auto);
}

void GuiApp::restore_record_rows(const DatasetRecord& rec) {
    PrepJob job = _sfm_job.prep;
    job.inputs = _sources;
    job.workspace = _workspace;
    job.photo_import = _photo_import;
    job.mask_clicks = _mask.clicks;
    restore_record_inputs(rec, job, _sfm_job.camera_model);
    _sources = job.inputs;
    if (!job.mask_clicks.empty()) {
        _mask.clicks = job.mask_clicks;
        int objects = 1;
        for (const MaskClick& c : _mask.clicks) objects = std::max(objects, c.object + 1);
        _mask.object_count = objects;
        _mask.current_object = 0;
    }
    normalize_source_lenses(_sources, _sfm_job.camera_model);
    normalize_source_fps(_sources, _sfm_job.prep.video_fps);
}

}  // namespace gui
