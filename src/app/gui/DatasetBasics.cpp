// The dataset screen's basic reconstruction options, lenses and rigs.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/SfmOptions.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
namespace smsg = spirula::i18n::msg::sfmoption;
using spirula::i18n::Msg;

namespace gui {

// ---------------------------------------------------------------------------
// The basics
// ---------------------------------------------------------------------------

namespace {

// Two paragraphs rather than one message, because only the second one changes
// with the selection. `inherited` names the model too, for a row reading "same
// as above" -- the picker is no longer showing which lens that is.
void lens_tooltip(const Msg& model_help, const Msg* inherited = nullptr) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) ||
        !ImGui::BeginTooltip())
        return;
    ImGui::PushTextWrapPos(px(kTooltipWrapW));
    ui::Text(dmsg::camera_lens_help);
    ImGui::Separator();
    if (inherited) ui::Text(*inherited);
    ui::Text(model_help);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// ImGui's Combo has nowhere to hang a description on a row, so the popup is
// built by hand: which physical camera each model is for is the whole of the
// question. `inherit` adds "(same as above)" and makes -1 a selection.
bool sfm_lens_combo(const char* id, int* idx, bool inherit = false) {
    const auto labels = sfm_camera_model_labels();
    const auto helps = sfm_camera_model_helps();
    const char* shown = *idx < 0 ? dmsg::lens_same_as_above.get()
                                 : labels[(size_t)*idx]->get();
    if (!ui::BeginComboRaw(id, shown)) return false;
    bool changed = false;
    if (inherit) {
        if (ui::Selectable(dmsg::lens_same_as_above, *idx < 0)) {
            *idx = -1;
            changed = true;
        }
        ui::help_on_hover(dmsg::lens_same_as_above_help);
        ImGui::Separator();
    }
    for (int i = 0; i < (int)labels.size(); i++) {
        if (ui::Selectable(*labels[(size_t)i], i == *idx)) {
            *idx = i;
            changed = true;
        }
        ui::help_on_hover(*helps[(size_t)i]);
    }
    ImGui::EndCombo();
    return changed;
}

// The same for COLMAP's, whose rows are COLMAP's own model names -- what a
// user who read its documentation is looking for -- with the translated
// sentence beside them rather than instead of them.
bool colmap_lens_combo(const char* id, int* idx) {
    const auto helps = colmap_camera_model_helps();
    if (!ui::BeginComboRaw(id, kColmapCameraModels[*idx])) return false;
    bool changed = false;
    for (int i = 0; i < kNumColmapCameraModels; i++) {
        if (ui::SelectableRaw(kColmapCameraModels[i], i == *idx)) {
            *idx = i;
            changed = true;
        }
        ui::help_on_hover(*helps[(size_t)i]);
    }
    ImGui::EndCombo();
    return changed;
}

}  // namespace

void GuiApp::draw_dataset_basics() {
    const bool builtin = effective_engine() == Engine::BuiltIn;

    // A model in the output folder is reused, so these settings reach it only
    // through a rebuild -- which a difference from the record it was built
    // with forces (DatasetPlan.h). One with no record is kept regardless.
    const WorkspaceState& prior = workspace_state();
    const bool reusing = !dataset_busy() && prior.model && !_redo_model;
    const bool inert = reusing && !_ws_record.step(Step::Model).present;
    if (reusing)
        ui::TextColoredWrapped(inert ? kWarn : kDim,
                               inert ? dmsg::recon_reuse_locked
                                     : dmsg::recon_reuse_rebuild);

    // Everything down to the frame-rate control is read by feature extraction
    // and after; the frame settings below it were read the moment the run
    // began. Hence two guards rather than one round the lot.
    ImGui::BeginDisabled(inert || dataset_locked(Stage::Features));

    // Quality means the same thing to both engines even though it moves
    // different knobs, so it is one control.
    ImGui::SetNextItemWidth(px(kFieldW));
    int& quality = builtin ? _sfm_job.quality : _colmap_job.quality;
    const int quality_before = quality;
    if (builtin) {
        ui::ComboWithHelp(dmsg::quality, &quality, dmsg::quality_help_builtin,
            {{dmsg::quality_fast, smsg::quality_fast_help},
             {dmsg::quality_balanced, smsg::quality_balanced_help},
             {dmsg::quality_high_recommended, smsg::quality_high_help},
             {dmsg::quality_maximum, smsg::quality_maximum_help}});
    } else {
        ui::ComboWithHelp(dmsg::quality, &quality, dmsg::quality_help_colmap,
            {{dmsg::quality_fast, smsg::quality_fast_help},
             {dmsg::quality_balanced, smsg::quality_balanced_help},
             {dmsg::quality_high, smsg::quality_high_help}});
    }
    // The masking models go with it while they are the ones it picked.
    if (quality != quality_before)
        follow_mask_quality(quality_before, quality, _model_id, _mask_detector_id);

    // Lens model, spelled per engine under one set of labels. The built-in
    // engine takes one lens per camera folder (draw_source_cameras) when there
    // are several; COLMAP's feature_extractor takes one model for the run.
    bool any_subcameras = false;
    for (const PrepInput& s : _sources)
        any_subcameras = any_subcameras || !s.subcameras.empty();
    const bool per_folder = _sfm_job.camera_mode == 1;
    const bool per_input_lens =
        builtin && per_folder && (_sources.size() > 1 || any_subcameras);
    if (!per_input_lens) {
        ImGui::SetNextItemWidth(px(kFieldW));
        if (builtin) {
            // Kept equal to the first row's by normalize_source_lenses, so
            // this reads the same lens whichever control last wrote it.
            const std::string& model = _sfm_job.camera_model;
            int idx = 0;
            for (int i = 0; i < kNumSfmCameraModels; i++)
                if (model == kSfmCameraModels[i]) idx = i;
            if (sfm_lens_combo(ui::detail::label(dmsg::camera_lens), &idx))
                apply_lens_to_sources(_sources, _sfm_job, kSfmCameraModels[idx]);
            lens_tooltip(*sfm_camera_model_helps()[idx]);
        } else {
            int idx = 0;
            for (int i = 0; i < kNumColmapCameraModels; i++)
                if (_colmap_job.camera_model == kColmapCameraModels[i]) idx = i;
            if (colmap_lens_combo(ui::detail::label(dmsg::camera_lens), &idx))
                _colmap_job.camera_model = kColmapCameraModels[idx];
            lens_tooltip(*colmap_camera_model_helps()[idx]);
        }
        if (!_sources.empty())
            draw_lens_warning(_sources[0].path, _sources[0].is_video,
                              builtin ? _sfm_job.camera_model
                                      : _colmap_job.camera_model,
                              builtin);
        if (!builtin && _sources.size() > 1)
            ui::TextColoredWrapped(kWarn, dmsg::colmap_one_lens_warning);
    } else {
        draw_source_cameras();
    }

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::ComboWithHelp(dmsg::camera_sharing,
        builtin ? &_sfm_job.camera_mode : &_colmap_job.camera_mode, dmsg::camera_sharing_help,
        {{dmsg::camera_sharing_one, smsg::camera_sharing_one_help},
         {dmsg::camera_sharing_folder, smsg::camera_sharing_folder_help},
         {dmsg::camera_sharing_image, smsg::camera_sharing_image_help}});

    ImGui::EndDisabled();

    // Which frames are left out, of photographs and video alike: a switch,
    // and while it is on, its settings whatever Advanced says.
    const auto draw_skip_blurry = [&] {
        PrepJob& prep = _sfm_job.prep;
        ui::Checkbox(dmsg::skip_blurry, &prep.skip_blurry);
        ui::help_on_hover(dmsg::skip_blurry_help);
        // Kept frames are not sorted, whatever this says: without the note
        // "Keep them, run the rest" looks like a run that did nothing.
        const StepPlan& frames = dataset_plan()[Step::Frames];
        bool kept_unsorted = false;
        for (const FieldChange& c : frames.changes)
            kept_unsorted = kept_unsorted || c.key == "skip_blurry" || c.key == "blur_px" ||
                            c.key == "blur_neighbour" || c.key == "similar_bits";
        if (frames.act == Act::Keep && kept_unsorted)
            ui::TextColoredWrapped(kWarn, dmsg::blur_kept_note);
        if (!prep.skip_blurry) return;
        ImGui::Indent();
        // "Every frame" means it: extraction leaves the rule out there.
        if (all_videos_every_frame(_sources, prep.video_fps))
            ui::TextColoredWrapped(kWarn, dmsg::blur_every_frame);
        ImGui::SetNextItemWidth(px(kFieldW));
        // Whole pixels: a finer threshold than the measurement means nothing,
        // and every other value is one more "made with other settings".
        if (ui::SliderFloat(dmsg::blur_px, &prep.blur_px, 1.0f, 40.0f, "%.0f px"))
            prep.blur_px = std::round(prep.blur_px);
        ui::help_on_hover(dmsg::blur_px_help);
        ui::Checkbox(dmsg::blur_neighbour, &prep.blur_neighbour);
        ui::help_on_hover(dmsg::blur_neighbour_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::SliderInt(dmsg::similar_bits, &prep.similar_bits, 0, 32);
        ui::help_on_hover(dmsg::similar_bits_help);
        ui::Checkbox(dmsg::blur_review, &prep.blur_review);
        ui::help_on_hover(dmsg::blur_review_help);
        if (prep.blur_review)
            ui::TextDisabledWrapped(builtin ? dmsg::blur_review_note : dmsg::blur_review_no_pause);
        ui::Checkbox(dmsg::keep_skipped, &prep.keep_skipped);
        ui::help_on_hover(dmsg::keep_skipped_help);
        ImGui::Unindent();
    };

    bool any_video = false;
    for (const PrepInput& s : _sources) any_video = any_video || s.is_video;
    if (any_video) {
        // The rate itself is a column of the input list, beside the video it
        // describes; what is left here is what it means for all of them.
        ImGui::BeginDisabled(dataset_locked(Stage::Frames));
        ui::Checkbox(dmsg::adaptive_fps, &_sfm_job.prep.adaptive_fps);
        ui::help_on_hover(dmsg::adaptive_fps_help);
        if (_sfm_job.prep.adaptive_fps &&
            all_videos_every_frame(_sources, _sfm_job.prep.video_fps))
            ui::TextColoredWrapped(kWarn, dmsg::adaptive_fps_every_frame);
        if (_sfm_job.prep.adaptive_fps) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(px(kFieldW));
            ui::SliderFloat(dmsg::adaptive_range, &_sfm_job.prep.adaptive_range,
                            1.0f, 16.0f, "%.1f");
            ui::help_on_hover(dmsg::adaptive_range_help);
            ImGui::Unindent();
        }
        if (_ds_advanced) {
            ImGui::SetNextItemWidth(px(kFieldW));
            ui::SliderInt(dmsg::sharpness_window, &_sfm_job.prep.sharp_window, 1, 30);
            ui::help_on_hover(dmsg::sharpness_window_help);
        }
        draw_skip_blurry();
        bool any_multi = false;
        for (const PrepInput& s : _sources)
            any_multi = any_multi || (s.is_video && !s.pano360.valid() && s.video_tracks >= 2);
        if (any_multi && _ds_advanced) {
            ui::Checkbox(dmsg::sync_lenses, &_sfm_job.prep.sync_tracks);
            ui::help_on_hover(dmsg::sync_lenses_help);
        }
        if (any_pano360(_sources)) draw_pano360_options();
        if (!backends().builtin_video) {
            // What the note says is a build-configuration diagnostic and
            // stays English; the sentence around it does not.
            ImGui::PushTextWrapPos();
            ui::TextDisabled(dmsg::build_note, {backends().video_note});
            ImGui::PopTextWrapPos();
            ui::help_on_hover_raw(backends().video_reason.c_str());
        }
        ImGui::EndDisabled();
    } else if (!_sources.empty()) {
        ImGui::BeginDisabled(dataset_locked(Stage::Frames));
        draw_skip_blurry();
        ImGui::EndDisabled();
    }
    if (dataset_busy()) ui::help_on_hover_disabled(dmsg::step_locked);
}

// What a 360 capture is unwrapped into, drawn among the video settings only
// when one was detected. The lens follows the choice exactly (apply_pano_lens),
// so there is no camera model to pick here.
void GuiApp::draw_pano360_options() {
    app::Pano360Options& p = _sfm_job.prep.pano;
    int mode = p.mode == app::Pano360Mode::Equirect ? 1 : 0;
    ImGui::SetNextItemWidth(px(kFieldW));
    if (ui::ComboWithHelp(dmsg::pano360_output, &mode, dmsg::pano360_output_help,
        {{dmsg::pano360_output_faces, smsg::pano_faces_help},
         {dmsg::pano360_output_equirect, smsg::pano_equirect_help}})) {
        p.mode = mode == 1 ? app::Pano360Mode::Equirect : app::Pano360Mode::Faces;
        // A face side and a panorama width are not the same number, so the one
        // in the box is meaningless the moment the other is chosen.
        reset_pano_size(_sources, _sfm_job.prep.pano);
        apply_pano_lens(_sources, _sfm_job, _colmap_job);
    }
    if (mode == 1 && effective_engine() != Engine::BuiltIn)
        ui::TextColoredWrapped(kWarn, dmsg::pano360_colmap_warning);
}

// The size of those views, which is under Advanced because the default is
// derived from the source and is what almost everyone should use.
void GuiApp::draw_pano360_size() {
    ImGui::SetNextItemWidth(px(kFieldW));
    ui::InputInt(dmsg::pano360_size, &_sfm_job.prep.pano.size, 32, 128);
    if (_sfm_job.prep.pano.size < 0) _sfm_job.prep.pano.size = 0;
    ui::help_on_hover(dmsg::pano360_size_help);
}

// The lens and starting focal an input's images are fitted with: its first
// camera group's, once "same as above" has been followed down the list.
void GuiApp::source_lens(size_t input, std::string& model, float& focal) const {
    const std::vector<CameraGroup> groups = camera_groups(_sources);
    const std::vector<std::string> models =
        camera_group_models(_sources, groups, _sfm_job.camera_model);
    for (size_t i = 0; i < groups.size(); i++) {
        if (groups[i].input != input) continue;
        model = models[i];
        focal = group_focal(_sources, groups[i]);
        return;
    }
}

// One lens per camera group, in place of the single "Camera / lens" control,
// once there is more than one camera to tell apart -- which lets a 360 clip and
// a phone clip reconstruct as one scene, neither fitted with the other's model.
void GuiApp::draw_source_cameras() {
    ui::Text(dmsg::camera_lens_per_input);
    ui::help_on_hover(dmsg::camera_lens_per_input_help);
    ImGui::Indent();
    const std::vector<CameraGroup> groups = camera_groups(_sources);
    const std::vector<std::string> models =
        camera_group_models(_sources, groups, _sfm_job.camera_model);
    const auto labels = sfm_camera_model_labels();
    const auto helps = sfm_camera_model_helps();

    // A row names the folder it measures, which is also the prefix the
    // override matches on -- so an input with several camera folders repeats
    // its own name, and that is what says which rows belong together.
    std::vector<std::string> names(groups.size());
    float col = px(200.0f);
    for (size_t i = 0; i < groups.size(); i++) {
        fs::path p(_sources[groups[i].input].path);
        if (p.filename().empty()) p = p.parent_path();   // trailing separator
        names[i] = groups[i].rel.empty() ? p.filename().string() : groups[i].rel;
        col = std::max(col, ImGui::CalcTextSize(names[i].c_str()).x + px(24.0f));
    }
    col = std::min(col, px(420.0f));

    // One letter per two rows, and never fewer than a row already holds.
    int letters = std::max<int>(1, (int)groups.size() / 2);
    for (const CameraGroup& g : groups)
        letters = std::max(letters, group_rig(_sources, g) - kRigFirstShared + 1);
    letters = std::min(letters, kRigShared);
    std::vector<std::string> letter_names;
    for (int l = 0; l < letters; l++) letter_names.push_back(rig_letter(l));

    bool edited = false;
    for (size_t i = 0; i < groups.size(); i++) {
        const CameraGroup& g = groups[i];
        const PrepInput& in = _sources[g.input];
        const std::string dir =
            g.sub < 0 ? in.path
                      : (fs::path(in.path) /
                         in.subcameras[(size_t)g.sub].rel).string();
        // The row's own prefix as its ID, so adding or dropping an input does
        // not carry an open picker or a half-typed focal onto another row.
        ImGui::PushID(g.rel.c_str());
        ui::TextRaw(names[i]);
        if (ImGui::IsItemHovered()) ui::SetTooltipRaw(dir);
        // A name too long for the column takes the row and leaves the controls
        // on the next one, rather than being drawn through them.
        if (ImGui::CalcTextSize(names[i].c_str()).x + px(8.0f) < col)
            ImGui::SameLine(col);
        ImGui::SetNextItemWidth(px(kFieldW));
        std::string& stored = group_model(_sources, g);
        int idx = stored.empty() ? -1 : 0;
        for (int m = 0; m < kNumSfmCameraModels; m++)
            if (stored == kSfmCameraModels[m]) idx = m;
        if (sfm_lens_combo("##lens", &idx, /*inherit=*/i > 0)) {
            stored = idx < 0 ? std::string() : kSfmCameraModels[idx];
            edited = true;
        }
        int shown = 0;
        for (int m = 0; m < kNumSfmCameraModels; m++)
            if (models[i] == kSfmCameraModels[m]) shown = m;
        lens_tooltip(*helps[(size_t)shown],
                     idx < 0 ? labels[(size_t)shown] : nullptr);
        // "This input's lenses" only means something for an input with
        // several: a lone photo folder or a one-lens video has none to rig.
        const bool multi = g.sub >= 0 || lens_dirs(_sfm_job.prep, in).size() >= 2;
        int& rig = group_rig(_sources, g);
        // A lens switched off can come back, and the rig with it.
        if (!multi && rig == kRigOwn && in.active_lens < 0) rig = kRigNone;
        if (_ds_advanced) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(px(90.0f));
            ui::InputFloat(dmsg::focal_x_width, &group_focal(_sources, g), 0, 0,
                           "%.4g");
            ui::help_on_hover(dmsg::focal_x_width_help);
        }
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(px(150.0f));
            std::vector<const char*> items = {ui::detail::label(dmsg::rig_none)};
            std::vector<int> ids = {kRigNone};
            if (multi) {
                items.push_back(ui::detail::label(dmsg::rig_own));
                ids.push_back(kRigOwn);
            }
            for (int l = 0; l < letters; l++) {
                items.push_back(letter_names[(size_t)l].c_str());
                ids.push_back(kRigFirstShared + l);
            }
            int idx = (int)(std::find(ids.begin(), ids.end(), rig) - ids.begin());
            if (idx >= (int)ids.size()) idx = 0;
            if (ui::BeginComboRaw("##rig", items[(size_t)idx])) {
                for (size_t j = 0; j < items.size(); ++j) {
                    if (ui::SelectableRaw(items[j], (int)j == idx)) rig = ids[j];
                    ui::help_on_hover(ids[j] == kRigNone ? smsg::rig_none_help
                                      : ids[j] == kRigOwn ? smsg::rig_own_help
                                                          : smsg::rig_shared_help);
                    if ((int)j == idx) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            } else {
                ui::help_on_hover(dmsg::rig_help);
            }
        }
        draw_lens_warning(dir, in.is_video, models[i], /*builtin=*/true);
        ImGui::PopID();
    }
    draw_rig_kinds(groups, names);
    if (groups.size() >= 2) {
        if (ui::SmallButton(dmsg::rig_guess)) guess_source_rigs(_sources, /*force=*/true);
        ui::help_on_hover(dmsg::rig_guess_help);
    }
    ImGui::Unindent();
    // Only now: the edit is the row's own, and collapsing it into the row above
    // rewrites what the loop was holding references into.
    if (edited) {
        normalize_source_lenses(_sources, _sfm_job.camera_model);
        normalize_source_fps(_sources, _sfm_job.prep.video_fps);
    }
}

// Two photo folders on one rig may be the two lenses of one dual-fisheye
// camera, which a video's own file says for itself (SfmRunner::build_rigs).
// Offered per rig of exactly two folders; any row of it holding the flag counts.
void GuiApp::draw_rig_kinds(const std::vector<CameraGroup>& groups,
                            const std::vector<std::string>& names) {
    for (int id = kRigOwn; id < kRigFirstShared + kRigShared; id++) {
        // "own" is a rig per input, so it is asked per input; a letter once.
        const size_t n_inputs = id == kRigOwn ? _sources.size() : 1;
        for (size_t input = 0; input < n_inputs; input++) {
            std::vector<size_t> rows;
            bool video = false;
            for (size_t i = 0; i < groups.size(); i++) {
                const CameraGroup& g = groups[i];
                if (group_rig(_sources, g) != id) continue;
                if (id == kRigOwn && (g.input != input || g.sub < 0)) continue;
                video = video || _sources[g.input].is_video;
                rows.push_back(i);
            }
            if (video || rows.size() != 2) continue;
            bool dual = false;
            for (size_t i : rows) dual = dual || group_rig_dual_fisheye(_sources, groups[i]);
            fs::path own(_sources[input].path);
            if (own.filename().empty()) own = own.parent_path();
            const std::string rig_name =
                id == kRigOwn ? own.filename().string()
                              : rig_letter(id - kRigFirstShared);
            const std::string label =
                i18n::format(dmsg::rig_dual_fisheye, {rig_name, names[rows[0]], names[rows[1]]}) +
                "###rig_dual_fisheye_" + std::to_string(id) + "_" + std::to_string(input);
            if (ui::CheckboxRaw(label.c_str(), &dual))
                for (size_t i : rows) group_rig_dual_fisheye(_sources, groups[i]) = dual;
            ui::help_on_hover(dmsg::rig_dual_fisheye_help);
        }
    }
}

bool GuiApp::input_pixel_size(const std::string& path, bool is_video,
                              int& w, int& h) {
    w = h = 0;
    if (path.empty()) return false;
    auto it = _input_size.find(path);
    if (it == _input_size.end()) {
        if (is_video) return false;
        std::pair<int, int> size{0, 0};
        std::error_code ec;
        if (fs::is_directory(path, ec)) {
            int iw = 0, ih = 0;
            if (DatasetPrep::first_image_dims(path, iw, ih)) size = {iw, ih};
        }
        it = _input_size.emplace(path, size).first;
    }
    w = it->second.first;
    h = it->second.second;
    return w > 0 && h > 0;
}

void GuiApp::draw_lens_warning(const std::string& path, bool is_video,
                               const std::string& model, bool builtin) {
    const bool fisheye = builtin ? sfm_model_is_fisheye(model)
                                 : colmap_model_is_fisheye(model);
    // COLMAP has no panorama model, so the question only arises for the
    // built-in engine.
    const bool pano = builtin && model == "equirectangular";
    // A 360 capture's lens describes the WARPED views, which are 2:1 (or square
    // faces) by construction; the frame this would measure is the packing.
    for (const PrepInput& s : _sources)
        if (s.pano360.valid() && s.path == path) return;
    // A dual-lens file is two fisheye circles per frame whatever its pixel
    // dimensions are, so this one needs no measurement.
    bool known_fisheye = is_dual_fisheye_path(path);
    for (const PrepInput& s : _sources)
        if (s.path == path) known_fisheye = known_fisheye || has_fisheye_lens(s);
    if (known_fisheye) {
        if (pano) ui::TextColoredWrapped(kWarn, dmsg::lens_warn_dual_fisheye);
        else if (!fisheye)
            ui::TextColoredWrapped(kWarn, dmsg::lens_warn_needs_fisheye);
        return;
    }
    if (!pano) return;
    int w = 0, h = 0;
    if (!input_pixel_size(path, is_video, w, h)) return;
    if (std::fabs((double)w / (double)h - 2.0) <= 0.02) return;
    ui::TextColoredWrapped(kWarn, dmsg::lens_warn_not_2to1, {w, h});
}

}  // namespace gui
