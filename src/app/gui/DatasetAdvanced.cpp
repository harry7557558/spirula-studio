// The dataset screen's advanced options: colour space, built-in SfM, COLMAP.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/ProgressView.h"
#include "app/gui/SfmOptions.h"
#include "app/gui/Ui.h"
#include "core/ColorSpace.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;
using spirula::i18n::Msg;

namespace gui {

namespace {

bool ends_with(const std::string& s, const char* tail) {
    const size_t n = std::strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Advanced: the photographs' colour space
// ---------------------------------------------------------------------------

void GuiApp::draw_color_space_options(bool with_point_color) {
    ui::SeparatorText(dmsg::section_color_space);
    bool source_changed = false;

    // Item 0 of both pickers means "leave it to the file", which is what the
    // child processes do when the flag is absent; anything else is stated on
    // their command line, so an EXR header that lies can be overruled.
    ImGui::SetNextItemWidth(px(kFieldW));
    int gamut = 0;
    for (int i = 0; i < (int)std::size(colorspace::kGamuts); i++)
        if (_sfm_job.image_gamut == colorspace::kGamuts[i]) gamut = i + 1;
    if (ui::Combo(dmsg::input_gamut, &gamut,
                  {&dmsg::space_from_file, &dmsg::gamut_rec709,
                   &dmsg::gamut_aces2065_1, &dmsg::gamut_acescg,
                   &dmsg::gamut_rec2020, &dmsg::gamut_adobergb,
                   &dmsg::gamut_dcip3})) {
        _sfm_job.image_gamut = gamut == 0 ? "" : colorspace::kGamuts[gamut - 1];
        _color_space_touched = true;
        source_changed = true;
    }
    ui::help_on_hover(dmsg::input_gamut_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    int transfer = !_sfm_job.image_is_linear.has_value() ? 0
                                                        : (*_sfm_job.image_is_linear ? 1 : 2);
    if (ui::Combo(dmsg::input_is_linear, &transfer,
                  {&dmsg::space_from_file, &dmsg::transfer_linear,
                   &dmsg::transfer_display})) {
        _sfm_job.image_is_linear =
            transfer == 0 ? std::optional<bool>{} : std::optional<bool>(transfer == 1);
        _color_space_touched = true;
        source_changed = true;
    }
    ui::help_on_hover(dmsg::input_is_linear_help);

    // "" / "auto" / a number of stops: item 2 keeps a number even at zero.
    std::string& ex = _sfm_job.image_exposure;
    int exposure = ex.empty() ? 0 : ex == "auto" ? 1 : 2;
    ImGui::SetNextItemWidth(px(kFieldW));
    if (ui::Combo(dmsg::input_exposure, &exposure,
                  {&dmsg::exposure_as_stored, &dmsg::exposure_auto, &dmsg::exposure_fixed})) {
        ex = exposure == 0 ? "" : exposure == 1 ? "auto" : "+2.0";
        source_changed = true;
    }
    ui::help_on_hover(dmsg::input_exposure_help);
    if (exposure == 2) {
        colorspace::Exposure e;
        colorspace::parse_exposure(ex, e);
        float stops = e.stops;
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::SliderFloat(dmsg::input_exposure_stops, &stops, -4.0f, 10.0f, "%+.1f EV")) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "%+.1f", stops);
            ex = buf;
            source_changed = true;
        }
    }
    if (source_changed) close_native_previews();

    // COLMAP writes its own point cloud, so the choice is the built-in SfM's.
    if (!with_point_color) return;
    ImGui::BeginDisabled(colorspace::is_identity(
        _sfm_job.image_gamut, _sfm_job.image_is_linear.value_or(false)));
    ui::Checkbox(dmsg::point_color_image_space,
                 &_sfm_job.point_color_in_image_space);
    ui::help_on_hover(dmsg::point_color_image_space_help);
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Advanced: built-in SfM
// ---------------------------------------------------------------------------

// Would the reconstruction reach the learned frontend and find no checkpoint?
// The same question mask_model_missing() asks, of the other download.
bool GuiApp::feature_model_missing() const {
    if (effective_engine() != Engine::BuiltIn) return false;
    return !sfm_features_cached(_sfm_job.features, _sfm_job.matcher);
}

void GuiApp::request_feature_download() {
    start_downloads_with_consent(
        _feat_download, sfm_feature_downloads(_sfm_job.features, _sfm_job.matcher));
}

// The detector and, with LightGlue, the matcher: what they cost and a button
// that gets them. Under the two combos that chose them.
void GuiApp::draw_feature_download() {
    FileDownload& dl = _feat_download.current();
    if (_feat_download.running()) {
        download_bar(dl, dl.status());
        ImGui::SameLine();
        ImGui::PushID("featdl");
        if (ui::Button(dmsg::stop)) _feat_download.cancel();
        ImGui::PopID();
        return;
    }
    if (!feature_model_missing()) {
        ui::TextColored(kOk, dmsg::feat_model_ready);
        return;
    }
    uint64_t bytes = 0;
    for (const PendingDownload& d :
         sfm_feature_downloads(_sfm_job.features, _sfm_job.matcher))
        bytes += d.bytes;
    if (ui::Button(dmsg::feat_get_model)) request_feature_download();
    ImGui::SameLine();
    ui::TextDisabledRaw(human_bytes(bytes));
    if (dl.state() == FileDownload::State::Failed)
        ui::TextColoredWrappedRaw(kErr, dl.status());
}

namespace {

// One `spirula sfm` option (SfmOptions.h). The box shows what a run would use:
// the default the quality and the frontend imply, or the value typed over it.
void sfm_option_box(SfmReconSettings& job, const std::string& flag, const char* label,
                    const Msg* help = nullptr) {
    SfmOption row;
    if (!find_sfm_option(flag, row)) return;
    std::string value = sfm_option_value(job, flag);
    const bool bounded = row.lo < row.hi;
    bool changed = false;
    ImGui::PushID(flag.c_str());
    ImGui::SetNextItemWidth(px(kFieldW));
    switch (row.type) {
        case SfmOptionType::Switch: {
            bool on = value == "on";
            changed = ui::CheckboxRaw(label, &on);
            value = on ? "on" : "off";
            break;
        }
        case SfmOptionType::Integer: {
            int v = std::atoi(value.c_str());
            changed = ui::InputIntRaw(label, &v);
            if (bounded) v = (int)std::clamp((double)v, row.lo, row.hi);
            value = std::to_string(v);
            break;
        }
        case SfmOptionType::Real: {
            double v = std::strtod(value.c_str(), nullptr);
            changed = ui::InputDoubleRaw(label, &v, 0.0, 0.0, "%g");
            if (bounded) v = std::clamp(v, row.lo, row.hi);
            char text[32];
            std::snprintf(text, sizeof text, "%g", v);
            value = text;
            break;
        }
        case SfmOptionType::Text:
            if (row.choices.empty()) {
                changed = ui::InputTextRaw(label, &value);
            } else if (ui::BeginComboRaw(label, value.c_str())) {
                for (const std::string& c : row.choices)
                    if (ui::SelectableRaw(c, c == value)) {
                        value = c;
                        changed = true;
                    }
                ImGui::EndCombo();
            }
            break;
    }
    if (help) ui::help_on_hover(*help);
    else if (row.help) ui::help_on_hover(*row.help);
    if (changed) set_sfm_option(job, flag, value);
    if (job.options.count(flag)) {
        ImGui::SameLine();
        if (ui::SmallButton(dmsg::option_default)) job.options.erase(flag);
    }
    ImGui::PopID();
}

// The selected detector's or matcher's own options, folded. `drawn` is one
// the panel already has a box for.
void sfm_owner_options(SfmReconSettings& job, const Msg& title, const std::string& owner,
                       const std::string& drawn) {
    std::vector<SfmOption> rows = sfm_owned_options(owner);
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [&](const SfmOption& r) { return r.flag == drawn || ends_with(r.flag, "model"); }),
               rows.end());
    if (rows.empty() || !ui::TreeNode(title)) return;
    for (const SfmOption& r : rows) sfm_option_box(job, r.flag, r.flag.c_str());
    ImGui::TreePop();
}

}  // namespace

void GuiApp::draw_sfm_advanced() {
    _ds_advanced = ui::CollapsingHeader(dmsg::section_advanced);
    if (!_ds_advanced) return;

    if (any_pano360(_sources)) draw_pano360_size();

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::Combo(dmsg::features, &_sfm_job.features,
              {&dmsg::features_sift, &dmsg::features_aliked_n16,
               &dmsg::features_aliked_n32, &dmsg::features_loma_b128,
               &dmsg::features_loma_b});
    ui::help_on_hover(dmsg::features_help);
    sfm_owner_options(_sfm_job, dmsg::sfm_detector_options,
                      sfm_detector_owner(_sfm_job.features),
                      sfm_feature_count_flag(_sfm_job.features));

    {
        // The middle entry is whichever learned matcher goes with the frontend
        // above, since each reads only the descriptors it was trained on; SIFT
        // has none, so its list skips it. FlexGlue reads pixels and takes any.
        const bool learned = _sfm_job.features != 0;
        const bool is_loma = _sfm_job.features >= 3;
        ImGui::SetNextItemWidth(px(kFieldW));
        if (learned) {
            ui::Combo(dmsg::matcher, &_sfm_job.matcher,
                      {&dmsg::matcher_brute_force,
                       is_loma ? &dmsg::matcher_loma : &dmsg::matcher_lightglue,
                       &dmsg::matcher_flexglue});
        } else {
            int shown = _sfm_job.matcher == 2 ? 1 : 0;
            if (ui::Combo(dmsg::matcher, &shown,
                          {&dmsg::matcher_brute_force, &dmsg::matcher_flexglue}))
                _sfm_job.matcher = shown == 1 ? 2 : 0;
        }
        ui::help_on_hover(_sfm_job.matcher == 2 ? dmsg::matcher_flexglue_help
                          : learned            ? dmsg::matcher_help
                                               : dmsg::matcher_needs_learned);
        sfm_owner_options(_sfm_job, dmsg::sfm_matcher_options,
                          sfm_matcher_owner(_sfm_job.features, _sfm_job.matcher), "");
        if (_sfm_job.matcher == 2 &&
            any_fisheye_or_pano_lens(_sources, _sfm_job.camera_model))
            ui::TextColoredWrapped(kWarn, dmsg::matcher_flexglue_wide_lens);
        if (learned || _sfm_job.matcher == 2) draw_feature_download();
    }

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::Combo(dmsg::mapper_schedule, &_sfm_job.mapper,
              {&dmsg::mapper_flat, &dmsg::mapper_bottom_up, &dmsg::mapper_global});
    ui::help_on_hover(dmsg::mapper_schedule_help);

    const bool builtin = effective_engine() == Engine::BuiltIn;
    int matcher_idx = _colmap_job.matcher - 1;
    if (!builtin && (matcher_idx < 0 || matcher_idx > 2)) {
        matcher_idx = (!_sources.empty() && _sources[0].is_video) ? 1 : 0;
        _colmap_job.matcher = matcher_idx + 1;
    }

    ImGui::SetNextItemWidth(px(kFieldW));
    if (builtin) {
        // In the order worth trying; SfmJob::pairs keeps its stored numbering.
        static const int kOrder[] = {kPairsPrefilter, kPairsExhaustive, kPairsSequential};
        int shown = 0;
        for (int i = 0; i < 3; i++)
            if (sfm_pairs(_sfm_job.pairs) == kOrder[i]) shown = i;
        if (ui::Combo(dmsg::image_matching, &shown,
                      {&dmsg::matching_gpu_preselect, &dmsg::matching_every_pair,
                       &dmsg::matching_neighbours}))
            _sfm_job.pairs = kOrder[shown];
        ui::help_on_hover(dmsg::matching_help_builtin);
    } else {
        if (ui::Combo(dmsg::image_matching, &matcher_idx,
                        {&dmsg::matching_exhaustive, &dmsg::matching_sequential,
                        &dmsg::matching_vocab_tree}))
            _colmap_job.matcher = matcher_idx + 1;
        ui::help_on_hover(dmsg::matching_help_colmap);
        if (_colmap_job.matcher == 2) {
            ImGui::Indent();
            ui::Checkbox(dmsg::loop_closure, &_colmap_job.seq_loop_closure);
            ui::help_on_hover(dmsg::loop_closure_help_colmap);
            ImGui::Unindent();
        }
    }

    // Each belongs to one pair mode, and is offered while that one is chosen.
    {
        ImGui::Indent();
        const int pairs = sfm_pairs(_sfm_job.pairs);
        if (pairs == kPairsSequential) {
            ui::Checkbox(dmsg::loop_closure, &_sfm_job.loop_closure);
            ui::help_on_hover(dmsg::loop_closure_help_builtin);
        }
        if (pairs == kPairsPrefilter) {
            ui::Checkbox(dmsg::prefilter_sequential, &_sfm_job.prefilter_sequential);
            ui::help_on_hover(dmsg::prefilter_sequential_help);
        }
        if (sequential_window_applies(_sfm_job)) {
            ImGui::SetNextItemWidth(px(kFieldW));
            ui::InputInt(dmsg::sequential_overlap, &_sfm_job.overlap);
            ui::help_on_hover(dmsg::sequential_overlap_help);
        }
        if (pairs == kPairsPrefilter || (pairs == kPairsSequential && _sfm_job.loop_closure))
            sfm_option_box(_sfm_job, "prefilter-neighbors",
                        ui::detail::label(dmsg::prefilter_neighbors),
                        &dmsg::prefilter_neighbors_help);
        ImGui::Unindent();
    }

    // What Quality picked, until typed over.
    sfm_option_box(_sfm_job, sfm_feature_count_flag(_sfm_job.features),
                   ui::detail::label(dmsg::max_features_auto), &dmsg::max_features_auto_help);
    sfm_option_box(_sfm_job, "max-image-size", ui::detail::label(dmsg::max_image_size_auto),
                   &dmsg::max_image_size_auto_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::InputFloat(dmsg::initial_focal_px, &_sfm_job.init_focal_px, 0, 0, "%.4g");
    ui::help_on_hover(dmsg::initial_focal_px_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::InputTextWithHint(dmsg::initial_distortion, dmsg::initial_distortion_hint,
                          &_sfm_job.init_distortion);
    ui::help_on_hover(dmsg::initial_distortion_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::Combo(dmsg::sfm_distortion_refinement, &_sfm_job.distortion_refine,
              {&dmsg::sfm_distortion_during, &dmsg::sfm_distortion_final,
               &dmsg::sfm_distortion_never});
    ui::help_on_hover(dmsg::sfm_distortion_refinement_help);

    // Per-image cameras already give every image its own intrinsics, so the
    // pass would have nothing to split.
    ImGui::BeginDisabled(_sfm_job.camera_mode == 2);
    ui::Checkbox(dmsg::sfm_per_image_intrinsics,
                 &_sfm_job.final_per_image_intrinsics);
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::sfm_per_image_intrinsics_help);
    ui::Checkbox(dmsg::sfm_final_free_rig, &_sfm_job.final_free_rig);
    ui::help_on_hover(dmsg::sfm_final_free_rig_help);

    // ---- what the sensors are allowed to settle ----
    // GPS from a video's track or a photo's EXIF, a video's IMU, and a
    // drone's recorded attitude. Any one can be all an input has.
    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_sensors);
    ui::TextDisabledWrapped(dmsg::sensors_section_caption);
    ImGui::SetNextItemWidth(px(kFieldW));
    ui::Combo(dmsg::sfm_metric_gps, &_sfm_job.metric_gps,
              {&dmsg::sfm_metric_gps_off, &dmsg::sfm_metric_gps_horizontal,
               &dmsg::sfm_metric_gps_full, &dmsg::sfm_metric_gps_auto});
    ui::help_on_hover(dmsg::sfm_metric_gps_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::Combo(dmsg::sfm_sensor_gauge, &_sfm_job.sensor_gauge,
              {&dmsg::sfm_sensor_gauge_off, &dmsg::sfm_sensor_gauge_up,
               &dmsg::sfm_sensor_gauge_auto});
    ui::help_on_hover(dmsg::sfm_sensor_gauge_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::Combo(dmsg::sfm_exif_attitude, &_sfm_job.exif_attitude,
              {&dmsg::sfm_sensor_gauge_off, &dmsg::sfm_sensor_gauge_up,
               &dmsg::sfm_exif_attitude_auto});
    ui::help_on_hover(dmsg::sfm_exif_attitude_help);
    ImGui::Spacing();

    draw_color_space_options(/*with_point_color=*/true);

    ui::SeparatorText(dmsg::section_fallbacks);
    ImGui::BeginDisabled(!backends().builtin_video);
    ui::Checkbox(dmsg::use_ffmpeg, &_sfm_job.prep.force_external_decode);
    ImGui::EndDisabled();
    ui::help_on_hover(backends().builtin_video ? dmsg::use_ffmpeg_help
                                               : dmsg::use_ffmpeg_always);

    ui::Checkbox(dmsg::sfm_ba_cpu, &_sfm_job.ba_cpu);
    ui::help_on_hover(dmsg::sfm_ba_cpu_help);

    ui::Checkbox(dmsg::sfm_subprocess, &_sfm_job.subprocess);
    ui::help_on_hover(dmsg::sfm_subprocess_help);

    ImGui::Spacing();
    // Unticking it is what throws the resumable state away, so it asks first
    // and the answer is remembered; ticking it back needs no ceremony.
    if (ui::Checkbox(dmsg::keep_intermediate, &_sfm_job.keep_intermediate)) {
        if (_sfm_job.keep_intermediate) save_settings();
        else _drop_intermediate_open = true;
    }
    ui::help_on_hover(dmsg::keep_intermediate_help);

    ImGui::SetNextItemWidth(-1);
    ui::InputTextWithHintRaw("##sfmextra", dmsg::extra_sfm_flags_hint,
                             &_sfm_job.extra_args);
    ui::help_on_hover(dmsg::extra_sfm_flags_help);
}

// ---------------------------------------------------------------------------
// Advanced: external COLMAP
// ---------------------------------------------------------------------------

void GuiApp::draw_colmap_options() {
    _ds_advanced = ui::CollapsingHeader(dmsg::section_advanced);
    if (_ds_advanced) {
        bool fisheye = _colmap_job.camera_model.find("FISHEYE") != std::string::npos;
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputFloat(dmsg::colmap_initial_focal,
                       &_colmap_job.init_focal_factor, 0, 0, "%.4g");
        ui::help_on_hover(dmsg::colmap_initial_focal_help);
        ImGui::SetNextItemWidth(px(280.0f));
        ui::InputTextWithHint(dmsg::colmap_camera_params,
                              dmsg::colmap_camera_params_hint,
                              &_colmap_job.camera_params);
        ui::help_on_hover(dmsg::colmap_camera_params_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::colmap_max_features, &_colmap_job.max_num_features);
        ui::help_on_hover(dmsg::colmap_max_features_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::colmap_max_image_size, &_colmap_job.max_image_size);
        ui::help_on_hover(dmsg::colmap_max_image_size_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::sequential_overlap, &_colmap_job.seq_overlap);
        ui::help_on_hover(dmsg::colmap_seq_overlap_help);
        ui::Checkbox(dmsg::colmap_quadratic_overlap,
                     &_colmap_job.seq_quadratic_overlap);
        ui::help_on_hover(dmsg::colmap_quadratic_overlap_help);
        ui::Checkbox(dmsg::colmap_lightglue, &_colmap_job.lightglue);
        ui::help_on_hover(dmsg::colmap_lightglue_help);
        if (_colmap_job.feature_type == 0) {
            ui::Checkbox(dmsg::colmap_affine_sift,
                         &_colmap_job.estimate_affine_shape);
            ui::help_on_hover(dmsg::colmap_affine_sift_help);
        }
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::Combo(dmsg::colmap_distortion_refinement,
                  &_colmap_job.mapper_extra_params,
                  {&dmsg::colmap_extra_auto, &dmsg::colmap_extra_during,
                   &dmsg::colmap_extra_final});
        ui::help_on_hover(dmsg::colmap_distortion_refinement_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::colmap_min_matches, &_colmap_job.min_num_matches);
        ui::help_on_hover(dmsg::colmap_min_matches_help);

        ui::SeparatorText(dmsg::colmap_repetitive);
        ui::help_on_hover(dmsg::colmap_repetitive_help);
        // Preset levels filling the five fields below (editing any field
        // afterwards shows "Custom"). Stricter = fewer wrong welds but
        // fewer registered images on genuinely weak overlap.
        struct RepLevel {
            const Msg* name;
            float ratio; int pair_in; int reg_in; float reg_ratio; float err;
        };
        static const RepLevel kRepLevels[] = {
            {&dmsg::colmap_rep_off,    0.0f,    0,   0, 0.0f,  0.0f},
            {&dmsg::colmap_rep_low,    0.75f,  30,  40, 0.30f, 10.0f},
            {&dmsg::colmap_rep_medium, 0.70f,  60,  60, 0.40f,  8.0f},
            {&dmsg::colmap_rep_high,   0.62f, 100, 100, 0.50f,  6.0f},
        };
        int rep_idx = -1;
        for (int i = 0; i < 4; i++)
            if (_colmap_job.match_max_ratio == kRepLevels[i].ratio &&
                _colmap_job.min_inliers_per_pair == kRepLevels[i].pair_in &&
                _colmap_job.abs_pose_min_num_inliers == kRepLevels[i].reg_in &&
                _colmap_job.abs_pose_min_inlier_ratio == kRepLevels[i].reg_ratio &&
                _colmap_job.abs_pose_max_error == kRepLevels[i].err) {
                rep_idx = i;
                break;
            }
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::BeginCombo(dmsg::colmap_repetitive_level,
                           rep_idx < 0 ? dmsg::colmap_rep_custom.get()
                                       : kRepLevels[rep_idx].name->get())) {
            for (int i = 0; i < 4; i++)
                if (ui::Selectable(*kRepLevels[i].name, rep_idx == i)) {
                    _colmap_job.match_max_ratio = kRepLevels[i].ratio;
                    _colmap_job.min_inliers_per_pair = kRepLevels[i].pair_in;
                    _colmap_job.abs_pose_min_num_inliers = kRepLevels[i].reg_in;
                    _colmap_job.abs_pose_min_inlier_ratio = kRepLevels[i].reg_ratio;
                    _colmap_job.abs_pose_max_error = kRepLevels[i].err;
                }
            ImGui::EndCombo();
        }
        ui::help_on_hover(dmsg::colmap_repetitive_level_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputFloat(dmsg::colmap_match_ratio, &_colmap_job.match_max_ratio,
                       0, 0, "%.3g");
        ui::help_on_hover(dmsg::colmap_match_ratio_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::colmap_min_inliers_pair,
                     &_colmap_job.min_inliers_per_pair);
        ui::help_on_hover(dmsg::colmap_min_inliers_pair_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::colmap_min_inliers_reg,
                     &_colmap_job.abs_pose_min_num_inliers);
        ui::help_on_hover(dmsg::colmap_min_inliers_reg_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputFloat(dmsg::colmap_min_inlier_ratio,
                       &_colmap_job.abs_pose_min_inlier_ratio, 0, 0, "%.3g");
        ui::help_on_hover(dmsg::colmap_min_inlier_ratio_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputFloat(dmsg::colmap_max_reg_error,
                       &_colmap_job.abs_pose_max_error, 0, 0, "%.3g");
        ui::help_on_hover(dmsg::colmap_max_reg_error_help);
        ImGui::Separator();
        if (fisheye) ImGui::BeginDisabled();
        ui::Checkbox(dmsg::colmap_gpu_ba, &_colmap_job.ba_use_gpu);
        if (fisheye) ImGui::EndDisabled();
        ui::help_on_hover(fisheye ? dmsg::colmap_gpu_ba_fisheye
                                  : dmsg::colmap_gpu_ba_help);
        ui::Checkbox(dmsg::colmap_merge_models, &_colmap_job.merge_models);
        ui::help_on_hover(dmsg::colmap_merge_models_help);
        ui::Checkbox(dmsg::colmap_final_ba, &_colmap_job.final_bundle_adjust);
        ui::help_on_hover(dmsg::colmap_final_ba_help);
        ImGui::SetNextItemWidth(px(-160.0f));
        ui::InputTextWithHintRaw("##vocab", dmsg::colmap_vocab_tree_hint,
                                 &_colmap_job.vocab_tree_path);
        ImGui::SameLine();
        ImGui::PushID("vt");
        if (ui::Button(dmsg::browse)) {
            open_pick(PickAction::VocabTree, msg::pick_vocab_tree.get(),
                      FileDialog::Mode::File, {".bin"});
        }
        ImGui::PopID();
        ImGui::SameLine();
        ui::Text(dmsg::colmap_vocab_tree);

        draw_color_space_options(/*with_point_color=*/false);
    }
}

void GuiApp::draw_tool_locations() {
    if (ui::CollapsingHeader(dmsg::section_tool_locations)) {
        bool ch = false;
        // A folder is resolved once the field is left, not under the cursor.
        auto resolve = [](std::string& exe, const char* name) {
            if (!ImGui::IsItemDeactivatedAfterEdit()) return false;
            const std::string found = tool_in_folder(exe, name);
            if (found == exe) return false;
            exe = found;
            return true;
        };
        if (effective_engine() == Engine::Colmap) {
            ImGui::SetNextItemWidth(px(kPathFieldW));
            ch |= ui::InputText(dmsg::colmap_executable, &_colmap_exe);
            ch |= resolve(_colmap_exe, "colmap");
        }
        ImGui::SetNextItemWidth(px(kPathFieldW));
        bool ffmpeg_changed = ui::InputText(dmsg::ffmpeg_executable, &_ffmpeg_exe);
        ffmpeg_changed |= resolve(_ffmpeg_exe, "ffmpeg");
        ch |= ffmpeg_changed;
        ui::help_on_hover(backends().builtin_video
                              ? dmsg::ffmpeg_executable_help_fallback
                              : dmsg::ffmpeg_executable_help_always);
        if (ffmpeg_changed) mark_source_metadata_dirty();
        if (ch) save_settings();
    }
}

}  // namespace gui
