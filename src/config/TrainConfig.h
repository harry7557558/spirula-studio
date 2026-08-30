#pragma once

// The training config: every flag `spirula train` accepts, in one table.
//
// This file is the single source of truth for the flag's BEHAVIOUR. Adding a
// row to SS_CONFIG_FIELDS makes the field appear in the CLI parser, in
// `--help`, in the GUI's "All Options" editor, in the run's config.json and
// in TrainerCore. The struct is expanded from the same table, so a field
// cannot exist in one and not the other.
//
// The one thing a row does NOT carry is what the flag is CALLED and what it
// does in words: that is translated, so it lives in
// i18n/catalog/TrainFields.h, one `SS_MSG(<member>, ...)` name and one
// `SS_MSG(<member>_help, ...)` sentence per row. `--help` and the GUI tooltip
// both read those, and a row with no entry there is a compile error naming
// the flag. This header must not include that catalog -- it is included
// nearly everywhere and should not drag the translations behind it; the
// consumers that draw text include both and paste the two together.
//
// Row: X(type, member, default, section, tier, choices)
//
//   type     one of the scalar types below, or TrainVec3i / TrainVec3f.
//            std::array<T, N> cannot appear here: its comma would split the
//            macro argument.
//   member   the struct member AND, stringified, the CLI flag. The parser
//            treats '-' and '_' alike, so --sh-degree sets sh_degree.
//   default  a constant expression. Vector defaults go through train_v3i() /
//            train_v3f() for the same comma reason.
//   section  the heading the flag is listed under, one of kTrainSections.
//            Rows must stay contiguous per section: --help and the GUI stream
//            headings as they walk the table rather than sorting first.
//            Purely presentational -- config.json is flat, so a flag can be
//            moved to a different heading without breaking anything on disk.
//   tier     how much of a specialist the flag is for, one of kTrainTiers:
//            "basic"    the ~20 flags a first run needs. `--help` and the
//                       GUI's default view show these and nothing else.
//            "advanced" reached for often enough to be worth browsing.
//            "expert"   warmups, schedules and regularizer internals.
//            "stub"     parsed but not implemented; hidden unless asked for.
//   choices  '|'-separated list for string fields; "" is free-form; "none"
//            means the empty string is allowed and displays as `none`.

#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <string>

// Vector field types and their makers, so the table stays comma-free.
using TrainVec3i = std::array<int, 3>;
using TrainVec3f = std::array<float, 3>;
constexpr TrainVec3i train_v3i(int a, int b, int c) { return {a, b, c}; }
constexpr TrainVec3f train_v3f(float a, float b, float c) { return {a, b, c}; }

inline constexpr float kTrainInf = std::numeric_limits<float>::infinity();

// The headings, in the order they are listed. Their labels live in
// i18n/catalog/Train.h (section_*), which this header must not include.
inline constexpr const char* kTrainSections[] = {
    "run", "dataset", "scene", "splats", "detail", "loss",
    "geometry", "shape", "correction", "colorspace", "perf", "rates",
};
inline constexpr int kTrainNumSections =
    (int)(sizeof(kTrainSections) / sizeof(kTrainSections[0]));

inline int train_section_index(const char* section) {
    for (int i = 0; i < kTrainNumSections; i++)
        if (!std::strcmp(kTrainSections[i], section)) return i;
    return 0;
}

// Least specialist first, so "show me everything up to advanced" is a <=.
inline constexpr const char* kTrainTiers[] = {
    "basic", "advanced", "expert", "stub",
};
inline constexpr int kTrainNumTiers =
    (int)(sizeof(kTrainTiers) / sizeof(kTrainTiers[0]));

inline int train_tier_rank(const char* tier) {
    for (int i = 0; i < kTrainNumTiers; i++)
        if (!std::strcmp(kTrainTiers[i], tier)) return i;
    return kTrainNumTiers - 1;
}


// ===========================================================================
// The field table
// ===========================================================================

#define SS_CONFIG_FIELDS(X) \
                                                                             \
    /* ==== run -- run control: where output goes, how long, checkpoints, viewer ==== */ \
    X(std::string, data, {}, "run", "basic", "")                             \
    X(std::string, resume, "", "run", "advanced", "none")                    \
    X(std::string, output_dir_prefix, "outputs", "run", "basic", "")         \
    X(std::string, output_dir_name, "", "run", "basic", "none")              \
    X(int, num_iterations, 30000, "run", "basic", "")                        \
    X(int, steps_per_save, 2000, "run", "advanced", "")                      \
    X(bool, save_only_latest_checkpoint, true, "run", "advanced", "")        \
    X(bool, save_full_checkpoint, false, "run", "advanced", "")              \
    X(bool, save_eval_images, false, "run", "advanced", "")                  \
    X(int, viewer_port, 7007, "run", "advanced", "")                         \
    X(bool, disable_viewer, false, "run", "advanced", "")                    \
    X(bool, keep_viewer_alive, true, "run", "advanced", "")                  \
                                                                             \
    /* ==== dataset -- which files are read, and which images are held out ==== */ \
    X(std::string, data_format, "", "dataset", "basic", "colmap|nerfstudio|metashape|none") \
    X(std::string, image_dir, "images", "dataset", "basic", "")              \
    X(std::string, mask_dir, "masks", "dataset", "basic", "")                \
    X(bool, load_masks, true, "dataset", "basic", "")                        \
    X(bool, apply_loss_for_mask, false, "dataset", "basic", "")              \
    X(float, mask_boundary_offset, 0.0f, "dataset", "advanced", "")          \
    X(std::string, depth_dir, "depths", "dataset", "basic", "")              \
    X(std::string, normal_dir, "normals", "dataset", "basic", "")            \
    X(bool, load_depths, true, "dataset", "basic", "")                       \
    X(bool, load_normals, true, "dataset", "basic", "")                      \
    X(float, depth_unit_scale_factor, 0.001f, "dataset", "expert", "")       \
    X(std::string, colmap_recon_dir, "", "dataset", "basic", "none")         \
    X(std::string, metashape_xml, "", "dataset", "advanced", "none")         \
    X(std::string, metashape_ply, "", "dataset", "advanced", "none")         \
    X(std::string, metashape_psx, "", "dataset", "advanced", "none")         \
    X(float, rescale_camera_to_fit, 0.0f, "dataset", "advanced", "")         \
    X(std::string, downscale_rounding_mode, "floor", "dataset", "advanced", "floor|ceil|round") \
    X(std::string, eval_mode, "all", "dataset", "advanced", "fraction|filename|interval|all") \
    X(int, eval_interval, 8, "dataset", "advanced", "")                      \
    X(float, train_split_fraction, 0.9f, "dataset", "advanced", "")          \
    X(float, validation_fraction, 0.0f, "dataset", "expert", "")             \
    X(bool, warp_to_pinhole, false, "dataset", "advanced", "")               \
    X(bool, warp_spherical_to_pinhole, true, "dataset", "advanced", "")      \
    X(std::string, warp_face_fit, "uniform", "dataset", "advanced", "uniform|per-face") \
    X(bool, deblur_training_images, false, "dataset", "stub", "")            \
                                                                             \
    /* ==== scene -- how the capture is placed, oriented and scaled ==== */  \
    X(std::string, orientation_method, "up", "scene", "expert", "pca|up|vertical|none|gsplat") \
    X(std::string, center_method, "poses", "scene", "expert", "poses|focus|none|gsplat") \
    X(bool, auto_scale_poses, true, "scene", "expert", "")                   \
    X(float, outlier_threshold, kTrainInf, "scene", "basic", "")             \
    X(std::optional<float>, relative_scale, std::nullopt, "scene", "expert", "") \
    X(std::string, train_frame, "points", "scene", "expert", "normalized|camera|points") \
                                                                             \
    /* ==== splats -- what a splat is and how the splats start out ==== */   \
    X(std::string, primitive, "3dgs", "splats", "basic", "3dgs|mip|3dgut")   \
    X(int, sh_degree, 3, "splats", "basic", "")                              \
    X(int, sh_degree_warmup_every, 1000, "splats", "expert", "")             \
    X(std::string, background_mode, "black", "splats", "basic", "black|noise|sh") \
    X(int, background_sh_degree, 4, "splats", "basic", "")                   \
    X(int, background_noise_warmup, 2000, "splats", "expert", "")            \
    X(float, background_noise_pre_warmup, 0.25f, "splats", "expert", "")     \
    X(std::optional<float>, scale_init, std::nullopt, "splats", "advanced", "") \
    X(std::optional<float>, opacity_init, std::nullopt, "splats", "advanced", "") \
    X(bool, suppress_initial_scales, false, "splats", "expert", "")          \
    X(bool, use_camera_optimizer, false, "splats", "stub", "")               \
                                                                             \
    /* ==== detail -- how many splats there are and where they go ==== */    \
    X(std::string, quality, "medium", "detail", "basic", "low|medium|high|ultra") \
    X(int, cap_max, 1000000, "detail", "basic", "")                          \
    X(std::string, distraction_robustness, "off", "detail", "basic", "off|mild|strong") \
    X(float, min_init_fraction, 0.0f, "detail", "basic", "")                 \
    X(float, growth_factor, 1.05f, "detail", "advanced", "")                 \
    X(float, min_opacity, 0.005f, "detail", "advanced", "")                  \
    X(int, refine_every, 100, "detail", "advanced", "")                      \
    X(int, refine_start_iter, 500, "detail", "expert", "")                   \
    X(int, refine_stop_num_iter, 5000, "detail", "advanced", "")             \
    X(int, refine_stop_iter, 25000, "detail", "advanced", "")                \
    X(float, noise_lr, 80.0f, "detail", "expert", "")                        \
    X(float, noise_lr_final, 0.8f, "detail", "expert", "")                   \
    X(bool, use_revised_densification, true, "detail", "expert", "")         \
    X(std::string, densify_score_mode, "mean", "detail", "basic", "mean|max|median|geom") \
    X(float, densify_score_blend_world_grad, 0.0f, "detail", "advanced", "") \
    X(std::string, densify_loss_map_mode, "ssim_cs", "detail", "basic", "none|loss_full|ssim_full|ssim_cs|ssim_structure|edge_aware|robust_edge_aware|loss_full_nms|ssim_full_nms|ssim_cs_nms|ssim_structure_nms") \
    X(float, densify_robust_edge_aware_quantile, 0.9f, "detail", "basic", "")\
    X(float, densify_nms_falloff, 0.5f, "detail", "advanced", "")            \
    X(bool, densify_loss_map_normalize, false, "detail", "advanced", "")     \
    X(float, densify_loss_map_clip_quantile, 1.0f, "detail", "advanced", "") \
    X(float, densify_loss_map_power, 4.0f, "detail", "advanced", "")         \
    X(std::string, densify_accum_mode, "avg", "detail", "basic", "max|sum|avg") \
    X(float, densify_score_power, 0.4f, "detail", "advanced", "")            \
    X(float, densify_score_clip_quantile, 1.0f, "detail", "advanced", "")    \
    X(float, densify_final_score_power, 1.0f, "detail", "advanced", "")      \
    X(float, densify_oversize_split_fraction, 0.15f, "detail", "advanced", "") \
    X(float, densify_oversize_score_blend, 0.5f, "detail", "advanced", "")   \
    X(bool, use_long_axis_split, true, "detail", "expert", "")               \
    X(TrainVec3f, long_axis_split_opacity_k, train_v3f(0.5f, 0.6f, 8000.0f), "detail", "basic", "") \
    X(float, max_screen_size, 0.3f, "detail", "basic", "")                   \
    X(float, max_screen_size_clip_hardness, 1.5f, "detail", "basic", "")     \
    X(float, max_screen_size_penalty, 1.0f, "detail", "basic", "")           \
    X(float, max_world_size, kTrainInf, "detail", "expert", "")              \
                                                                             \
    /* ==== loss -- how the render is compared against the photo ==== */     \
    X(float, ssim_lambda, 0.2f, "loss", "basic", "")                         \
    X(float, l1_weight, 1.0f, "loss", "basic", "")                           \
    X(float, l2_weight, 0.0f, "loss", "basic", "")                           \
    X(float, l1_weight_y, 0.0f, "loss", "advanced", "")                      \
    X(float, l2_weight_y, 0.0f, "loss", "advanced", "")                      \
    X(float, l2_weight_u, 0.0f, "loss", "advanced", "")                      \
    X(float, l2_weight_v, 0.0f, "loss", "advanced", "")                      \
    X(int, loss_scale_min_pixels, 1920, "loss", "advanced", "")              \
    X(int, num_loss_scales, 0, "loss", "advanced", "")                       \
    X(float, alpha_loss_weight, 0.1f, "loss", "basic", "")                   \
    X(float, alpha_loss_weight_under, 0.0f, "loss", "basic", "")             \
                                                                             \
    /* ==== geometry -- how crisp the surfaces come out, and depth/normal guidance ==== */ \
    X(std::string, floater_suppression, "off", "geometry", "basic", "off|mild|strong") \
    X(float, depth_distortion_reg, 0.0f, "geometry", "basic", "")            \
    X(float, normal_distortion_reg, 0.0f, "geometry", "basic", "")           \
    X(float, rgb_distortion_reg, 0.0f, "geometry", "basic", "")              \
    X(int, distortion_reg_warmup, 6000, "geometry", "basic", "")             \
    X(float, normal_reg_weight, 0.04f, "geometry", "basic", "")              \
    X(int, normal_reg_warmup, 6000, "geometry", "advanced", "")              \
    X(float, alpha_reg_weight, 0.0f, "geometry", "basic", "")                \
    X(int, alpha_reg_warmup, 12000, "geometry", "advanced", "")              \
    X(int, reg_warmup_length, 0, "geometry", "advanced", "")                 \
    X(float, depth_supervision_weight, 0.0f, "geometry", "basic", "")        \
    X(float, normal_supervision_weight, 0.01f, "geometry", "basic", "")      \
    X(std::optional<bool>, input_depth_is_ray_depth, std::nullopt, "geometry", "basic", "") \
    X(int, supervision_warmup, 0, "geometry", "expert", "")                  \
    X(float, mean_median_depth_weight, 0.0f, "geometry", "advanced", "")     \
    X(float, median_depth_normal_reg_weight, 0.0f, "geometry", "expert", "") \
    X(float, median_normal_supervision_weight, 0.0f, "geometry", "expert", "") \
    X(float, median_render_normal_reg_weight, 0.0f, "geometry", "expert", "")\
    X(int, median_warmup, 6000, "geometry", "expert", "")                    \
                                                                             \
    /* ==== shape -- keeping individual splats compact and well behaved ==== */ \
    X(float, opacity_reg, 0.01f, "shape", "basic", "")                       \
    X(float, scale_reg, 0.01f, "shape", "basic", "")                         \
    X(float, opacity_decay, 0.0f, "shape", "basic", "")                      \
    X(float, scale_decay, 0.0f, "shape", "basic", "")                        \
    X(float, erank_reg, 0.001f, "shape", "basic", "")                        \
    X(float, erank_reg_s3, 0.0f, "shape", "advanced", "")                    \
    X(float, scale_regularization_weight, 0.0f, "shape", "advanced", "")     \
    X(float, max_gauss_ratio, 10.0f, "shape", "advanced", "")                \
    X(float, dc_reg, 0.001f, "shape", "basic", "")                           \
    X(float, sh_reg, 0.001f, "shape", "basic", "")                           \
    X(float, overexposure_reg, 0.0f, "shape", "advanced", "")                \
    X(float, quat_norm_reg, 0.01f, "shape", "advanced", "")                  \
                                                                             \
    /* ==== correction -- per-photo exposure, white balance and lens response ==== */ \
    X(bool, use_bilateral_grid, true, "correction", "basic", "")             \
    X(std::string, bilagrid_type, "ppisp", "correction", "basic", "affine|ppisp|loglinear") \
    X(TrainVec3i, bilagrid_shape, train_v3i(16, 16, 8), "correction", "basic", "") \
    X(float, bilagrid_tv_loss_weight, 10.0f, "correction", "advanced", "")   \
    X(float, color_shift_reg_weight, 0.0f, "correction", "advanced", "")     \
    X(int, color_shift_reg_ema_period, 750, "correction", "expert", "")      \
    X(bool, use_bilateral_grid_for_geometry, true, "correction", "advanced", "") \
    X(TrainVec3i, bilagrid_shape_geometry, train_v3i(8, 8, 4), "correction", "advanced", "") \
    X(float, bilagrid_tv_loss_weight_geometry, 10.0f, "correction", "advanced", "") \
    X(bool, use_adagrad_bilagrid_optim, true, "correction", "advanced", "")  \
    X(bool, use_ppisp, true, "correction", "basic", "")                      \
    X(std::string, ppisp_param_type, "no_crf", "correction", "basic", "original|rqs|no_crf") \
    X(bool, ppisp_exposure_from_exif, false, "correction", "basic", "")      \
    X(bool, apply_ppisp_before_bilagrid, true, "correction", "advanced", "") \
    X(bool, use_adagrad_ppisp_optim, true, "correction", "advanced", "")     \
    X(float, ppisp_reg_exposure_mean, 1.0f, "correction", "advanced", "")    \
    X(float, ppisp_reg_color_mean, 1.0f, "correction", "advanced", "")       \
    X(float, ppisp_reg_vig_center, 0.02f, "correction", "advanced", "")      \
    X(float, ppisp_reg_vig_non_pos, 0.01f, "correction", "advanced", "")     \
    X(float, ppisp_reg_vig_channel_var, 0.1f, "correction", "advanced", "")  \
    X(float, ppisp_reg_crf_channel_var, 0.1f, "correction", "advanced", "")  \
    X(float, bilagrid_adagrad_lr, 0.04f, "correction", "advanced", "")       \
    X(float, bilagrid_adagrad_depth_lr, 0.04f, "correction", "expert", "")   \
    X(float, bilagrid_adagrad_normal_lr, 0.01f, "correction", "advanced", "")\
    X(float, ppisp_adagrad_lr, 0.1f, "correction", "advanced", "")           \
    X(float, bilagrid_lr, 0.002f, "correction", "advanced", "")              \
    X(std::optional<float>, bilagrid_lr_final, 0.0001f, "correction", "advanced", "") \
    X(int, bilagrid_lr_warmup, 1000, "correction", "expert", "")             \
    X(float, bilagrid_depth_lr, 0.002f, "correction", "expert", "")          \
    X(std::optional<float>, bilagrid_depth_lr_final, 0.0001f, "correction", "expert", "") \
    X(int, bilagrid_depth_lr_warmup, 2000, "correction", "expert", "")       \
    X(float, bilagrid_normal_lr, 0.0005f, "correction", "advanced", "")      \
    X(std::optional<float>, bilagrid_normal_lr_final, 4e-05f, "correction", "advanced", "") \
    X(int, bilagrid_normal_lr_warmup, 2000, "correction", "advanced", "")    \
    X(float, ppisp_lr, 0.002f, "correction", "advanced", "")                 \
    X(std::optional<float>, ppisp_lr_final, 2e-05f, "correction", "advanced", "") \
    X(int, ppisp_lr_warmup, 500, "correction", "advanced", "")               \
                                                                             \
    /* ==== colorspace -- linear vs display encoding, and which gamut ==== */\
    X(std::optional<bool>, image_color_is_linear, std::nullopt, "colorspace", "basic", "") \
    X(std::string, image_color_gamut, "", "colorspace", "basic", "Rec.709|ACES2065-1|ACEScg|Rec.2020|AdobeRGB|DCI-P3|none") \
    X(std::optional<bool>, splat_color_is_linear, std::nullopt, "colorspace", "basic", "") \
    X(std::string, splat_color_gamut, "", "colorspace", "basic", "Rec.709|ACES2065-1|ACEScg|Rec.2020|AdobeRGB|DCI-P3|none") \
    X(std::optional<bool>, convert_initial_point_cloud_color, std::nullopt, "colorspace", "basic", "") \
                                                                             \
    /* ==== perf -- speed and memory; none of these change the result ==== */\
    X(std::string, cache_images, "disk", "perf", "basic", "cpu|gpu|disk")    \
    X(int, max_batch_per_epoch, 800, "perf", "basic", "")                    \
    X(bool, split_batch, true, "perf", "advanced", "")                       \
    X(bool, use_fused_proj_bwd_optim, true, "perf", "advanced", "")          \
    X(bool, packed, true, "perf", "advanced", "")                            \
    X(int, quantization_level, 1, "perf", "advanced", "")                    \
    X(bool, preallocate_splat_tensors, true, "perf", "expert", "")           \
    X(std::string, optimizer_offload, "", "perf", "stub", "sh|all|none")     \
    X(bool, use_bvh, false, "perf", "stub", "")                              \
                                                                             \
    /* ==== rates -- how fast each splat parameter is allowed to change ==== */ \
    X(float, means_lr, 0.000128f, "rates", "basic", "")                      \
    X(std::optional<float>, means_lr_final, 1.6e-06f, "rates", "basic", "")  \
    X(float, scales_lr, 0.02f, "rates", "basic", "")                         \
    X(std::optional<float>, scales_lr_final, 0.005f, "rates", "basic", "")   \
    X(float, quats_lr, 0.0015f, "rates", "basic", "")                        \
    X(float, opacities_lr, 0.025f, "rates", "basic", "")                     \
    X(float, features_dc_lr, 0.005f, "rates", "basic", "")                   \
    X(float, features_sh_lr, 0.00025f, "rates", "basic", "")                 \
    X(float, background_dc_lr, 0.0025f, "rates", "advanced", "")             \
    X(float, background_sh_lr, 0.0005f, "rates", "advanced", "")             \
    X(std::optional<int>, max_steps, std::nullopt, "rates", "advanced", "")  \
    X(bool, use_scale_agnostic_mean, true, "rates", "expert", "")            \
    X(bool, use_per_splat_bias_correction, true, "rates", "expert", "")      \
    /* end */


// ===========================================================================
// The config struct, expanded from the table above
// ===========================================================================

struct TrainConfig {
#define SS_DECLARE_FIELD(type, member, default_, section, tier, choices)      \
    type member = default_;
    SS_CONFIG_FIELDS(SS_DECLARE_FIELD)
#undef SS_DECLARE_FIELD
};

// Fields whose default ({}) is not a usable value. Checked after flag
// parsing, so --help still works without them.
#define SS_CONFIG_REQUIRED_FIELDS(X) \
    X(data) \
    /* end */

// Fields that change what load_dataset() produces: edit one of these in the
// GUI and the parsed dataset it is holding is stale. Spelled out rather than
// derived from `section`, because the two sets are not the same shape (the
// warping and depth/normal flags are listed under other headings) and because
// a heading is free to be reshuffled -- which must not silently change when
// the GUI re-reads a dataset.
#define SS_DATASET_PARSE_FIELDS(X) \
    X(data) X(data_format) X(colmap_recon_dir) X(image_dir) X(mask_dir) \
    X(depth_dir) X(normal_dir) X(metashape_xml) X(metashape_ply) \
    X(metashape_psx) X(rescale_camera_to_fit) X(downscale_rounding_mode) \
    X(orientation_method) X(center_method) X(auto_scale_poses) \
    X(outlier_threshold) X(train_frame) X(eval_mode) X(train_split_fraction) \
    X(eval_interval) X(depth_unit_scale_factor) X(validation_fraction) \
    X(warp_to_pinhole) X(warp_spherical_to_pinhole) X(warp_face_fit) \
    X(load_masks) \
    X(load_depths) X(load_normals) X(relative_scale) \
    /* end */


// ===========================================================================
// Presets -- named bundles of default overrides, selected as
// `spirula train <preset>`. "3dgs" is the base config and applies nothing.
// ===========================================================================

// Just the names, which are what the command line and the checkpoints spell.
// The label and the sentence explaining each one are in
// i18n/catalog/Train.h: this header is included nearly everywhere and has no
// business dragging a translation catalog behind it. Both consumers
// (app/cli/main.cpp, app/gui/GuiApp.cpp) static_assert that the two lists are
// the same length.
struct TrainPresetInfo { const char* name; };
inline constexpr TrainPresetInfo kTrainPresets[] = {
    {"3dgs"},
    {"360-camera"},
    {"in-the-wild"},
    {"linear-color"},
    {"synthetic"},
    {"meshing"},
    // {"academic-baseline"},  // hidden by default, uncomment to enable
};

// Returns false for an unknown preset name.
inline bool train_apply_preset(TrainConfig& c, const std::string& name) {
    if (name == "3dgs") {
        return true;
    }
    if (name == "360-camera") {
        c.warp_to_pinhole = true;
        c.mask_boundary_offset = -0.025f;
        // c.primitive = "mip";
        c.erank_reg = 0.01f;
        c.long_axis_split_opacity_k = {0.5f, 0.6f, 15000.0f};
        return true;
    }
    if (name == "in-the-wild") {
        c.center_method = "focus";
        c.outlier_threshold = 10.0f;
        c.load_depths = true;
        c.load_normals = true;
        c.mask_boundary_offset = -0.025f;
        // c.floater_suppression= "strong";
        c.distraction_robustness = "strong";
        c.sh_degree_warmup_every = 0;
        c.long_axis_split_opacity_k = {0.5f, 0.6f, 30000.0f};
        c.noise_lr = 10.0f;
        c.noise_lr_final = 0.1f;
        c.erank_reg = 0.1f;
        c.means_lr = 5e-05f;
        c.means_lr_final = 1e-07f;
        return true;
    }
    if (name == "linear-color") {
        c.splat_color_gamut = "ACEScg";
        c.splat_color_is_linear = true;
        c.image_color_gamut = "Rec.2020";
        c.image_color_is_linear = false;
        c.background_mode = "noise";
        c.dc_reg = 0.0f;
        c.features_dc_lr = 0.0015f;
        c.features_sh_lr = 0.000075f;
        return true;
    }
    if (name == "synthetic") {
        c.min_init_fraction = 0.02f;
        c.use_bilateral_grid = false;
        c.use_ppisp = false;
        c.use_bilateral_grid_for_geometry = false;
        c.long_axis_split_opacity_k = {0.5f, 0.6f, 25000.0f};
        return true;
    }
    if (name == "meshing") {
        c.primitive = "3dgut";
        c.sh_degree = 0;
        c.dc_reg = 10.0f;
        c.sh_reg = 10.0f;
        c.overexposure_reg = 10.0f;
        c.background_mode = "noise";
        c.depth_distortion_reg = 0.01f;
        c.normal_distortion_reg = 0.01f;
        c.mean_median_depth_weight = 0.01f;
        c.median_depth_normal_reg_weight = 0.01f;
        c.normal_supervision_weight = 0.01f;
        c.median_normal_supervision_weight = 0.01f;
        c.median_render_normal_reg_weight = 0.01f;
        c.erank_reg = 0.01f;
        c.erank_reg_s3 = 0.01f;
        return true;
    }
    if (name == "academic-baseline") {
        c.eval_mode = "interval";
        c.eval_interval = 8;
        c.center_method = "gsplat";
        c.orientation_method = "gsplat";
        c.max_batch_per_epoch = 387420489;
        c.load_depths = false;
        c.load_normals = false;
        c.primitive = "3dgs";
        c.relative_scale = 1.0f;
        c.use_bilateral_grid = false;
        c.use_bilateral_grid_for_geometry = false;
        c.use_ppisp = false;
        c.use_revised_densification = false;
        c.densify_loss_map_mode = "none";
        c.use_long_axis_split = false;
        c.use_fused_proj_bwd_optim = false;
        c.quantization_level = 0;
        c.max_screen_size = kTrainInf;
        c.max_world_size = kTrainInf;
        c.suppress_initial_scales = false;
        c.scale_init = 0.1f;
        c.opacity_init = 0.5f;
        c.depth_distortion_reg = 0.0f;
        c.normal_reg_weight = 0.0f;
        c.alpha_reg_weight = 0.0f;
        c.alpha_loss_weight = 0.0f;
        c.alpha_loss_weight_under = 0.0f;
        c.erank_reg = 0.0f;
        c.erank_reg_s3 = 0.0f;
        c.quat_norm_reg = 0.0f;
        c.dc_reg = 0.0f;
        c.sh_reg = 0.0f;
        c.normal_supervision_weight = 0.0f;
        c.opacity_reg = 0.01f;
        c.scale_reg = 0.01f;
        c.max_steps = 30000;
        c.use_scale_agnostic_mean = false;
        c.use_per_splat_bias_correction = false;
        c.means_lr = 0.00016f;
        c.means_lr_final = 1.6e-06f;
        c.scales_lr = 0.005f;
        c.scales_lr_final = std::nullopt;
        c.quats_lr = 0.001f;
        c.opacities_lr = 0.05f;
        c.features_dc_lr = 0.0025f;
        c.features_sh_lr = 0.000125f;
        return true;
    }
    (void)c;
    return false;
}


// ===========================================================================
// Macro options -- one basic flag that moves a handful of specialist ones
// ===========================================================================

// Resolution order is: base defaults -> preset -> macros -> whatever the user
// set. A macro never overwrites a field the user set by hand; that is what
// `explicit_flags` is for (the CLI's `seen` set, the GUI's edited-field set),
// and it is why the flags a macro moves stay ordinary editable flags rather
// than becoming hidden.
//
// A macro sitting at its default writes nothing at all, so a preset that
// already tuned those fields keeps its own values. Naming the macro -- typing
// `--quality medium` -- does write, which is how a preset's tuning is put back
// to stock.
//
// `written`, when given, collects the flags the macros set so that a later
// stage can treat them as explicit as well (--resume does, so that a macro
// passed on the command line still beats the checkpoint's config.json).
//
// Call this once, in the front end, after the preset and the user's flags.
inline void train_resolve_macros(TrainConfig& c,
                                 const std::set<std::string>& explicit_flags,
                                 std::set<std::string>* written = nullptr) {
    auto put = [&](const char* key, auto& field, auto value) {
        if (explicit_flags.count(key)) return;
        field = value;
        if (written) written->insert(key);
    };

    // Splat budget and run length together: raising one without the other
    // just spends longer on splats that never get refined.
    if (c.quality != "medium" || explicit_flags.count("quality")) {
        if (c.quality == "low") {
            put("cap_max", c.cap_max, 200000);
            put("num_iterations", c.num_iterations, 15000);
        } else if (c.quality == "medium") {
            put("cap_max", c.cap_max, 1000000);
            put("num_iterations", c.num_iterations, 30000);
        } else if (c.quality == "high") {
            put("cap_max", c.cap_max, 3000000);
            put("num_iterations", c.num_iterations, 50000);
        } else if (c.quality == "ultra") {
            put("cap_max", c.cap_max, 10000000);
            put("num_iterations", c.num_iterations, 80000);
        }
    }

    // Score the splat-placement error in a way that a person walking through
    // half the photos cannot dominate.
    if (c.distraction_robustness != "off") {
        put("densify_loss_map_power", c.densify_loss_map_power, 1.0f);
        put("densify_score_power", c.densify_score_power, 1.0f);
        put("ssim_lambda", c.ssim_lambda, 0.1f);
        if (c.distraction_robustness == "strong") {
            put("densify_loss_map_mode", c.densify_loss_map_mode,
                "robust_edge_aware");
            put("densify_robust_edge_aware_quantile",
                c.densify_robust_edge_aware_quantile, 0.75f);
            put("densify_score_mode", c.densify_score_mode, "median");
        }
    }

    // Make each pixel commit to one surface, and stop view-dependent colour
    // from papering over what is left.
    if (c.floater_suppression != "off") {
        const bool strong = c.floater_suppression == "strong";
        put("depth_distortion_reg", c.depth_distortion_reg,
            strong ? 0.03f : 0.01f);
        put("rgb_distortion_reg", c.rgb_distortion_reg,
            strong ? 0.05f : 0.01f);
        put("sh_reg", c.sh_reg, strong ? 0.05f : 0.01f);
        put("max_screen_size", c.max_screen_size, strong ? 0.1f : 0.2f);
        put("max_screen_size_clip_hardness", c.max_screen_size_clip_hardness, strong ? 1.1f : 1.25f);
    }
}
