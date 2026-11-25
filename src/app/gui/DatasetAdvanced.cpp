// The dataset screen's advanced options: colour space, built-in SfM, COLMAP.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/OptionSearch.h"
#include "app/gui/ProgressView.h"
#include "app/gui/SfmOptions.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "core/ColorSpace.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/SfmOptions.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;
namespace smsg = spirula::i18n::msg::sfmoption;
using spirula::i18n::Msg;

namespace gui {

// ---------------------------------------------------------------------------
// Advanced: the photographs' colour space
// ---------------------------------------------------------------------------

void GuiApp::draw_color_space_options(bool with_point_color) {
    ui::SeparatorText(dmsg::section_color_space);
    ui::TextDisabledWrapped(dmsg::color_space_section_caption);
    bool source_changed = false;

    // Item 0 of both pickers means "leave it to the file", which is what the
    // child processes do when the flag is absent; anything else is stated on
    // their command line, so an EXR header that lies can be overruled.
    ImGui::SetNextItemWidth(px(kFieldW));
    int gamut = 0;
    for (int i = 0; i < (int)std::size(colorspace::kGamuts); i++)
        if (_sfm_job.image_gamut == colorspace::kGamuts[i]) gamut = i + 1;
    if (ui::ComboWithHelp(dmsg::input_gamut, &gamut, dmsg::input_gamut_help,
        {{dmsg::space_from_file, smsg::gamut_file_help},
         {dmsg::gamut_rec709, smsg::gamut_rec709_help},
         {dmsg::gamut_aces2065_1, smsg::gamut_aces2065_1_help},
         {dmsg::gamut_acescg, smsg::gamut_acescg_help},
         {dmsg::gamut_rec2020, smsg::gamut_rec2020_help},
         {dmsg::gamut_adobergb, smsg::gamut_adobergb_help},
         {dmsg::gamut_dcip3, smsg::gamut_dcip3_help}})) {
        _sfm_job.image_gamut = gamut == 0 ? "" : colorspace::kGamuts[gamut - 1];
        _color_space_touched = true;
        source_changed = true;
    }

    ImGui::SetNextItemWidth(px(kFieldW));
    int transfer = !_sfm_job.image_is_linear.has_value() ? 0
                                                        : (*_sfm_job.image_is_linear ? 1 : 2);
    if (ui::ComboWithHelp(dmsg::input_is_linear, &transfer, dmsg::input_is_linear_help,
        {{dmsg::space_from_file, smsg::transfer_file_help},
         {dmsg::transfer_linear, smsg::transfer_linear_help},
         {dmsg::transfer_display, smsg::transfer_display_help}})) {
        _sfm_job.image_is_linear =
            transfer == 0 ? std::optional<bool>{} : std::optional<bool>(transfer == 1);
        _color_space_touched = true;
        source_changed = true;
    }

    // "" / "auto" / a number of stops: item 2 keeps a number even at zero.
    std::string& ex = _sfm_job.image_exposure;
    int exposure = ex.empty() ? 0 : ex == "auto" ? 1 : 2;
    ImGui::SetNextItemWidth(px(kFieldW));
    if (ui::ComboWithHelp(dmsg::input_exposure, &exposure, dmsg::input_exposure_help,
        {{dmsg::exposure_as_stored, smsg::exposure_stored_help},
         {dmsg::exposure_auto, smsg::exposure_auto_help},
         {dmsg::exposure_fixed, smsg::exposure_fixed_help}})) {
        ex = exposure == 0 ? "" : exposure == 1 ? "auto" : "+2.0";
        source_changed = true;
    }
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

void sfm_option_box(SfmReconSettings& job, const SfmOption& row, const Msg& label,
                    const Msg& help) {
    const std::string& flag = row.flag;
    std::string value = sfm_option_value(job, flag);
    const bool bounded = row.lo < row.hi;
    bool changed = false;
    bool popup_open = false;
    ImGui::PushID(flag.c_str());
    const float width = px(row.label && row.type != SfmOptionType::Text
                              ? kCompactFieldW : kFieldW);
    const float widget_width = row.type == SfmOptionType::Switch ? ImGui::GetFrameHeight()
                                                                 : width;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float reset_width = ImGui::CalcTextSize(dmsg::option_default.get()).x +
                              2 * style.FramePadding.x + style.ItemSpacing.x;
    const bool wrap_label = widget_width + style.ItemInnerSpacing.x + reset_width +
                            ImGui::CalcTextSize(label.get()).x > ImGui::GetContentRegionAvail().x;
    if (wrap_label) {
        ui::TextWrapped(label);
        ui::help_on_hover(help);
    }
    const char* widget = ui::detail::label(wrap_label ? "" : label.get(), label.id);
    ImGui::SetNextItemWidth(width);
    switch (row.type) {
        case SfmOptionType::Switch: {
            bool on = value == "on";
            changed = ui::CheckboxRaw(widget, &on);
            value = on ? "on" : "off";
            break;
        }
        case SfmOptionType::Integer: {
            int v = std::atoi(value.c_str());
            changed = ui::InputIntRaw(widget, &v);
            if (bounded) v = (int)std::clamp((double)v, row.lo, row.hi);
            value = std::to_string(v);
            break;
        }
        case SfmOptionType::Real: {
            double v = std::strtod(value.c_str(), nullptr);
            changed = ui::InputDoubleRaw(widget, &v, 0.0, 0.0, "%g");
            if (bounded) v = std::clamp(v, row.lo, row.hi);
            char text[32];
            std::snprintf(text, sizeof text, "%g", v);
            value = text;
            break;
        }
        case SfmOptionType::Text: {
            if (row.choices.empty()) {
                changed = ui::InputTextRaw(widget, &value);
            } else {
                const auto selected = std::find_if(row.choices.begin(), row.choices.end(),
                    [&](const SfmOptionChoice& c) { return c.value == value; });
                const char* preview = selected != row.choices.end() && selected->label
                                          ? selected->label->get() : value.c_str();
                popup_open = ui::detail::named_combo(widget, [&] {
                    return ui::BeginComboRaw(widget, preview);
                });
                if (!popup_open) break;
                for (const SfmOptionChoice& c : row.choices) {
                    const bool current = c.value == value;
                    const bool clicked = c.label ? ui::Selectable(*c.label, current)
                                                 : ui::SelectableRaw(c.value, current);
                    if (clicked && !current) {
                        value = c.value;
                        changed = true;
                    }
                    if (c.help) ui::help_on_hover(*c.help);
                    if (current) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            break;
        }
    }
    if (!popup_open) ui::help_on_hover(help);
    if (changed) set_sfm_option(job, flag, value);
    if (job.options.count(flag)) {
        ImGui::SameLine();
        if (ui::SmallButton(dmsg::option_default)) job.options.erase(flag);
    }
    ImGui::PopID();
}

void sfm_option_box(SfmReconSettings& job, const std::string& flag, const Msg& label,
                    const Msg& help) {
    SfmOption row;
    if (find_sfm_option(flag, row)) sfm_option_box(job, row, label, help);
}

void sfm_options_fold(SfmReconSettings& job, const Msg& title,
                      const std::vector<SfmOption>& rows, SfmOptionFilter& filter) {
    if (rows.empty() || !ui::TreeNode(title)) return;

    constexpr size_t kFilterThreshold = 10;
    std::vector<std::string> tokens;
    bool show_expert = true;
    if (rows.size() > kFilterThreshold) {
        const float tier_width = px(130.0f);
        const float search_width = ImGui::GetContentRegionAvail().x - tier_width -
                                   ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(std::clamp(search_width, 1.0f, px(320.0f)));
        ui::InputTextWithHintRaw("##search", smsg::filter_search_hint, &filter.search);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(tier_width);
        const Msg* tiers[] = {&msg::cfg_tier_advanced, &msg::cfg_tier_all};
        if (ui::detail::named_combo("##tier", [&] {
                return ui::BeginComboRaw("##tier", tiers[filter.tier]->get());
            })) {
            for (int i = 0; i < (int)std::size(tiers); ++i) {
                const bool selected = filter.tier == i;
                if (ui::Selectable(*tiers[i], selected)) filter.tier = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        } else {
            ui::help_on_hover(msg::cfg_tier_help);
        }
        tokens = option_search::query_tokens(filter.search.c_str());
        show_expert = filter.tier == 1 || !tokens.empty();
    }

    bool found = false;
    for (const SfmOption& row : rows) {
        if (row.expert && !show_expert) continue;
        if (!tokens.empty()) {
            std::string searchable = row.flag;
            option_search::add_text(searchable, *row.label);
            option_search::add_text(searchable, *row.help);
            for (const SfmOptionChoice& choice : row.choices) {
                searchable += ' ' + choice.value;
                if (choice.label) option_search::add_text(searchable, *choice.label);
            }
            if (!option_search::matches(option_search::lower_ascii(searchable), tokens)) continue;
        }
        found = true;
        sfm_option_box(job, row, *row.label, *row.help);
    }
    if (!found) ui::TextDisabled(smsg::filter_no_results);
    ImGui::TreePop();
}

}  // namespace

void GuiApp::draw_sfm_advanced() {
    _ds_advanced = ui::CollapsingHeader(dmsg::section_advanced);
    if (!_ds_advanced) return;

    if (any_pano360(_sources)) {
        draw_pano360_size();
        ImGui::Spacing();
    }

    ui::SeparatorText(dmsg::plan_part_features);
    ui::TextDisabledWrapped(dmsg::feature_extraction_section_caption);
    ImGui::SetNextItemWidth(px(kFieldW));
    ui::ComboWithHelp(dmsg::features, &_sfm_job.features, dmsg::features_help,
        {{dmsg::features_sift, smsg::detector_sift_help},
         {dmsg::features_aliked_n16, smsg::detector_aliked_n16_help},
         {dmsg::features_aliked_n32, smsg::detector_aliked_n32_help},
         {dmsg::features_loma_b128, smsg::detector_loma_b128_help},
         {dmsg::features_loma_b, smsg::detector_loma_b_help}});
    sfm_options_fold(_sfm_job, dmsg::sfm_detector_options,
                     sfm_owned_options(sfm_detector_owner(_sfm_job.features)), _detector_filter);

    // What Quality picked, until typed over.
    sfm_option_box(_sfm_job, sfm_feature_count_flag(_sfm_job.features),
                   dmsg::max_features_auto, dmsg::max_features_auto_help);
    sfm_option_box(_sfm_job, "max-image-size", dmsg::max_image_size_auto,
                   dmsg::max_image_size_auto_help);

    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_feature_matching);
    ui::TextDisabledWrapped(dmsg::feature_matching_section_caption);
    {
        const bool learned = _sfm_job.features != 0;
        const bool is_loma = sfm_detector_owner(_sfm_job.features) == "loma";
        const ui::ComboChoice brute_force{dmsg::matcher_brute_force, smsg::matcher_brute_force_help};
        const ui::ComboChoice flexglue{dmsg::matcher_flexglue, dmsg::matcher_flexglue_help};
        ImGui::SetNextItemWidth(px(kFieldW));
        if (learned) {
            const ui::ComboChoice companion = is_loma
                ? ui::ComboChoice{dmsg::matcher_loma, smsg::matcher_loma_help}
                : ui::ComboChoice{dmsg::matcher_lightglue, smsg::matcher_lightglue_help};
            ui::ComboWithHelp(dmsg::matcher, &_sfm_job.matcher, dmsg::matcher_help,
                              {brute_force, companion, flexglue});
        } else {
            int shown = _sfm_job.matcher == 2 ? 1 : 0;
            if (ui::ComboWithHelp(dmsg::matcher, &shown, dmsg::matcher_help, {brute_force, flexglue}))
                _sfm_job.matcher = shown == 1 ? 2 : 0;
        }
        sfm_options_fold(_sfm_job, dmsg::sfm_matcher_options,
                         sfm_owned_options(sfm_matcher_owner(_sfm_job.features, _sfm_job.matcher)),
                         _matcher_filter);
        if (_sfm_job.matcher == 2 &&
            any_fisheye_or_pano_lens(_sources, _sfm_job.camera_model))
            ui::TextColoredWrapped(kWarn, dmsg::matcher_flexglue_wide_lens);
        if (learned || _sfm_job.matcher == 2) draw_feature_download();
    }

    const bool builtin = effective_engine() == Engine::BuiltIn;
    int matcher_idx = _colmap_job.matcher - 1;
    if (!builtin && (matcher_idx < 0 || matcher_idx > 2)) {
        matcher_idx = (!_sources.empty() && _sources[0].is_video) ? 1 : 0;
        _colmap_job.matcher = matcher_idx + 1;
    }

    ImGui::SetNextItemWidth(px(kFieldW));
    if (builtin) {
        // Cheapest first; SfmJob::pairs keeps its stored numbering.
        static const int kOrder[] = {kPairsPrefilter, kPairsSequential, kPairsExhaustive};
        int shown = 0;
        for (int i = 0; i < 3; i++)
            if (sfm_pairs(_sfm_job.pairs) == kOrder[i]) shown = i;
        if (ui::ComboWithHelp(dmsg::image_matching, &shown, dmsg::matching_help_builtin,
            {{dmsg::matching_gpu_preselect, smsg::pairs_prefilter_help},
             {dmsg::matching_neighbours, smsg::pairs_sequential_help},
             {dmsg::matching_every_pair, smsg::pairs_exhaustive_help}}))
            _sfm_job.pairs = kOrder[shown];
    } else {
        if (ui::ComboWithHelp(dmsg::image_matching, &matcher_idx, dmsg::matching_help_colmap,
            {{dmsg::matching_exhaustive, smsg::pairs_exhaustive_help},
             {dmsg::matching_sequential, smsg::pairs_sequential_help},
             {dmsg::matching_vocab_tree, smsg::pairs_vocab_help}}))
            _colmap_job.matcher = matcher_idx + 1;
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
        if (sequential_pairing(_sfm_job)) {
            ui::Checkbox(dmsg::loop_closure, &_sfm_job.loop_closure);
            ui::help_on_hover(dmsg::loop_closure_help_builtin);
            ImGui::SetNextItemWidth(px(kFieldW));
            ui::InputInt(dmsg::sequential_overlap, &_sfm_job.overlap);
            ui::help_on_hover(dmsg::sequential_overlap_help);
        }
        if (pairs == kPairsPrefilter || (pairs == kPairsSequential && _sfm_job.loop_closure))
            sfm_option_box(_sfm_job, "prefilter-neighbors",
                        dmsg::prefilter_neighbors, dmsg::prefilter_neighbors_help);
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ui::SeparatorText(dmsg::step_mapping);
    ui::TextDisabledWrapped(dmsg::mapping_section_caption);
    ImGui::SetNextItemWidth(px(kFieldW));
    ui::ComboWithHelp(dmsg::mapper_schedule, &_sfm_job.mapper, dmsg::mapper_schedule_help,
        {{dmsg::mapper_flat, smsg::mapper_flat_help},
         {dmsg::mapper_bottom_up, smsg::mapper_bottom_up_help},
         {dmsg::mapper_global, smsg::mapper_global_help}});
    sfm_options_fold(_sfm_job, dmsg::sfm_mapper_options, sfm_mapper_options(_sfm_job.mapper),
                     _mapper_filter);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::InputFloat(dmsg::initial_focal_px, &_sfm_job.init_focal_px, 0, 0, "%.4g");
    ui::help_on_hover(dmsg::initial_focal_px_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::InputTextWithHint(dmsg::initial_distortion, dmsg::initial_distortion_hint,
                          &_sfm_job.init_distortion);
    ui::help_on_hover(dmsg::initial_distortion_help);

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::ComboWithHelp(dmsg::sfm_distortion_refinement, &_sfm_job.distortion_refine,
                      dmsg::sfm_distortion_refinement_help,
        {{dmsg::sfm_distortion_during, smsg::distortion_during_help},
         {dmsg::sfm_distortion_final, smsg::distortion_final_help},
         {dmsg::sfm_distortion_never, smsg::distortion_never_help}});

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
    ui::ComboWithHelp(dmsg::sfm_metric_gps, &_sfm_job.metric_gps,
                      dmsg::sfm_metric_gps_help,
        {{dmsg::sfm_metric_gps_off, smsg::gps_off_help},
         {dmsg::sfm_metric_gps_horizontal, smsg::gps_horizontal_help},
         {dmsg::sfm_metric_gps_full, smsg::gps_full_help},
         {dmsg::sfm_metric_gps_auto, smsg::gps_auto_help}});

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::ComboWithHelp(dmsg::sfm_sensor_gauge, &_sfm_job.sensor_gauge,
                      dmsg::sfm_sensor_gauge_help,
        {{dmsg::sfm_sensor_gauge_off, smsg::imu_off_help},
         {dmsg::sfm_sensor_gauge_up, smsg::imu_up_help},
         {dmsg::sfm_sensor_gauge_auto, smsg::imu_scale_help}});

    ImGui::SetNextItemWidth(px(kFieldW));
    ui::ComboWithHelp(dmsg::sfm_exif_attitude, &_sfm_job.exif_attitude,
                      dmsg::sfm_exif_attitude_help,
        {{dmsg::sfm_sensor_gauge_off, smsg::attitude_off_help},
         {dmsg::sfm_sensor_gauge_up, smsg::attitude_up_help},
         {dmsg::sfm_exif_attitude_auto, smsg::attitude_north_help}});
    ImGui::Spacing();

    draw_color_space_options(/*with_point_color=*/true);

    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_fallbacks);
    ui::TextDisabledWrapped(dmsg::fallbacks_section_caption);
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
    ui::SeparatorText(dmsg::section_additional);
    // Unticking it is what throws the resumable state away, so it asks first
    // and the answer is remembered; ticking it back needs no ceremony.
    if (ui::Checkbox(dmsg::keep_intermediate, &_sfm_job.keep_intermediate)) {
        if (_sfm_job.keep_intermediate) save_settings();
        else _drop_intermediate_open = true;
    }
    ui::help_on_hover(dmsg::keep_intermediate_help);

    draw_dataset_finish_command();

    ui::Text(dmsg::extra_sfm_flags);
    ImGui::SetNextItemWidth(-1);
    ui::InputTextWithHintRaw("##sfmextra", dmsg::extra_sfm_flags_hint,
                             &_sfm_job.extra_args);
    ui::help_on_hover(dmsg::extra_sfm_flags_help);
}

void GuiApp::draw_dataset_finish_command() {
    ImGui::AlignTextToFramePadding();
    ui::Text(dmsg::finish_command);
    ImGui::SameLine();
    ImGui::BeginDisabled(_finish_cmd_run.busy() || split_args(_dataset_finish_command).empty());
    if (ui::Button(msg::batch_cmd_test))
        run_finish_command(_dataset_finish_command, msg::batch_cmd_test_message.get());
    ui::help_on_hover_disabled(msg::batch_cmd_test_help);
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(-1);
    ui::InputTextEnglishRaw("##dataset_finish_command",
                            "python notify_me.py --message \"{message}\"",
                            &_dataset_finish_command);
    if (ImGui::IsItemDeactivatedAfterEdit()) save_settings();
    ui::help_on_hover(dmsg::finish_command_help);
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
        ui::ComboWithHelp(dmsg::colmap_distortion_refinement, &_colmap_job.mapper_extra_params,
                          dmsg::colmap_distortion_refinement_help,
            {{dmsg::colmap_extra_auto, smsg::colmap_distortion_auto_help},
             {dmsg::colmap_extra_during, smsg::distortion_during_help},
             {dmsg::colmap_extra_final, smsg::colmap_distortion_final_help}});
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(dmsg::colmap_min_matches, &_colmap_job.min_num_matches);
        ui::help_on_hover(dmsg::colmap_min_matches_help);

        ui::SeparatorText(dmsg::colmap_repetitive);
        ui::help_on_hover(dmsg::colmap_repetitive_help);
        // Preset levels filling the five fields below (editing any field
        // afterwards shows "Custom"). Stricter = fewer wrong welds but
        // fewer registered images on genuinely weak overlap.
        struct RepLevel {
            const Msg& name;
            const Msg& help;
            float ratio; int pair_in; int reg_in; float reg_ratio; float err;
        };
        static const RepLevel kRepLevels[] = {
            {dmsg::colmap_rep_off,    smsg::rep_off_help,    0.0f,    0,   0, 0.0f,  0.0f},
            {dmsg::colmap_rep_low,    smsg::rep_low_help,    0.75f,  30,  40, 0.30f, 10.0f},
            {dmsg::colmap_rep_medium, smsg::rep_medium_help, 0.70f,  60,  60, 0.40f,  8.0f},
            {dmsg::colmap_rep_high,   smsg::rep_high_help,   0.62f, 100, 100, 0.50f,  6.0f},
        };
        int rep_idx = -1;
        for (int i = 0; i < (int)std::size(kRepLevels); i++)
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
                                       : kRepLevels[rep_idx].name.get())) {
            for (int i = 0; i < (int)std::size(kRepLevels); i++) {
                const RepLevel& level = kRepLevels[i];
                if (ui::Selectable(level.name, rep_idx == i)) {
                    _colmap_job.match_max_ratio = level.ratio;
                    _colmap_job.min_inliers_per_pair = level.pair_in;
                    _colmap_job.abs_pose_min_num_inliers = level.reg_in;
                    _colmap_job.abs_pose_min_inlier_ratio = level.reg_ratio;
                    _colmap_job.abs_pose_max_error = level.err;
                }
                ui::help_on_hover(level.help);
                if (rep_idx == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        } else {
            ui::help_on_hover(dmsg::colmap_repetitive_level_help);
        }
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
        ImGui::Spacing();
        ui::SeparatorText(dmsg::section_additional);
        draw_dataset_finish_command();
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
