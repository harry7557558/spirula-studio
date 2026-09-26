// DatasetPreset.cpp -- see DatasetPreset.h.

#include "app/gui/DatasetPreset.h"

#include "data/JsonField.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace gui {

namespace {

// X(key in the file, member of DatasetSettings). A setting is here or it is
// context, and context is: the input list, the output folder, the mask
// clicks, the fitted borders, the tool paths and the redo-a-step flags.
#define SS_DATASET_PRESET_FIELDS(X)                                           \
    X("engine_colmap",              colmap_engine)                            \
    /* ---- input handling and frame extraction ---- */                       \
    X("resume",                     sfm.prep.resume)                          \
    X("photo_import",               sfm.prep.photo_import)                    \
    X("use_found_masks",            use_found_masks)                          \
    X("flip_found_masks",           sfm.prep.flip_found_masks)                \
    X("video_fps",                  sfm.prep.video_fps)                       \
    X("adaptive_fps",               sfm.prep.adaptive_fps)                    \
    X("adaptive_range",             sfm.prep.adaptive_range)                  \
    X("sharp_window",               sfm.prep.sharp_window)                    \
    X("sync_tracks",                sfm.prep.sync_tracks)                     \
    X("max_frames",                 sfm.prep.max_frames)                      \
    X("auto_rotate",                sfm.prep.auto_rotate)                     \
    X("force_external_decode",      sfm.prep.force_external_decode)           \
    X("frame_bits",                 sfm.prep.frame_bits)                      \
    X("pano_mode",                  sfm.prep.pano.mode)                       \
    X("pano_size",                  sfm.prep.pano.size)                       \
    X("pano_yaw",                   sfm.prep.pano.yaw)                        \
    X("pano_pitch",                 sfm.prep.pano.pitch)                      \
    X("pano_roll",                  sfm.prep.pano.roll)                       \
    /* ---- colour space ---- */                                              \
    X("image_gamut",                sfm.image_gamut)                          \
    X("image_is_linear",            sfm.image_is_linear)                      \
    X("point_color_in_image_space", sfm.point_color_in_image_space)           \
    /* ---- masking ---- */                                                   \
    X("mask_enable",                sfm.prep.mask_enable)                     \
    X("mask_features",              sfm.mask_features)                        \
    X("mask_border",                border_enable)                            \
    X("mask_frame_shapes",          frame_shapes)                             \
    X("mask_model",                 mask_model_id)                            \
    X("mask_prompt",                mask.prompt)                              \
    X("mask_negative_prompt",       mask.negative_prompt)                     \
    X("mask_keep_subject",          mask.keep_subject)                        \
    X("mask_dilate_ratio",          mask.dilate_ratio)                        \
    X("mask_shrink_ratio",          mask.shrink_ratio)                        \
    X("mask_max_image_size",        mask.max_image_size)                      \
    X("mask_threshold",             mask.threshold)                           \
    X("mask_nms",                   mask.nms)                                 \
    X("mask_memory",                sfm.prep.mask_memory)                     \
    X("mask_detect_every",          sfm.prep.mask_detect_every)               \
    X("mask_memory_frames",         sfm.prep.mask_memory_frames)              \
    X("force_external_masking",     sfm.prep.force_external_masking)          \
    /* ---- depth and normals ---- */                                         \
    X("geometry_enable",            sfm.geometry.enable)                      \
    X("geometry_model",             sfm.geometry.model)                       \
    X("geometry_max_size",          sfm.geometry.max_size)                    \
    X("geometry_num_tokens",        sfm.geometry.num_tokens)                  \
    X("geometry_want_normal",       sfm.geometry.want_normal)                 \
    X("geometry_want_depth",        sfm.geometry.want_depth)                  \
    X("geometry_normal_jpg",        sfm.geometry.normal_jpg)                  \
    X("geometry_jpeg_quality",      sfm.geometry.jpeg_quality)                \
    X("geometry_depth_mm",          sfm.geometry.depth_mm)                    \
    X("geometry_ray_depth",         sfm.geometry.ray_depth)                   \
    X("geometry_split",             sfm.geometry.split)                       \
    X("geometry_overwrite",         sfm.geometry.overwrite)                   \
    /* ---- the built-in reconstruction ---- */                               \
    X("sfm_quality",                sfm.quality)                              \
    X("sfm_data_type",              sfm.data_type)                            \
    X("sfm_camera_model",           sfm.camera_model)                         \
    X("sfm_camera_mode",            sfm.camera_mode)                          \
    X("sfm_pairs",                  sfm.pairs)                                \
    X("sfm_overlap",                sfm.overlap)                              \
    X("sfm_loop_closure",           sfm.loop_closure)                         \
    X("sfm_prefilter_sequential",   sfm.prefilter_sequential)                 \
    X("sfm_use_sequence",           sfm.use_sequence)                         \
    X("sfm_init_focal_px",          sfm.init_focal_px)                        \
    X("sfm_init_distortion",        sfm.init_distortion)                      \
    X("sfm_distortion_refine",      sfm.distortion_refine)                    \
    X("sfm_final_per_image_intrinsics", sfm.final_per_image_intrinsics)       \
    X("sfm_final_free_rig",         sfm.final_free_rig)                       \
    X("sfm_max_features",           sfm.max_features)                         \
    X("sfm_max_image_size",         sfm.max_image_size)                       \
    X("sfm_mapper",                 sfm.mapper)                               \
    X("sfm_features",               sfm.features)                            \
    X("sfm_matcher",                sfm.matcher)                              \
    X("sfm_metric_gps",             sfm.metric_gps)                           \
    X("sfm_sensor_gauge",           sfm.sensor_gauge)                         \
    X("sfm_exif_attitude",          sfm.exif_attitude)                        \
    X("sfm_keep_intermediate",      sfm.keep_intermediate)                    \
    X("sfm_ba_cpu",                 sfm.ba_cpu)                               \
    X("sfm_subprocess",             sfm.subprocess)                           \
    X("sfm_extra_args",             sfm.extra_args)                           \
    /* ---- COLMAP ---- */                                                    \
    X("colmap_camera_model",        colmap.camera_model)                      \
    X("colmap_camera_mode",         colmap.camera_mode)                       \
    X("colmap_init_focal_factor",   colmap.init_focal_factor)                 \
    X("colmap_camera_params",       colmap.camera_params)                     \
    X("colmap_feature_type",        colmap.feature_type)                      \
    X("colmap_lightglue",           colmap.lightglue)                         \
    X("colmap_quality",             colmap.quality)                           \
    X("colmap_matcher",             colmap.matcher)                           \
    X("colmap_seq_loop_closure",    colmap.seq_loop_closure)                  \
    X("colmap_max_num_features",    colmap.max_num_features)                  \
    X("colmap_max_image_size",      colmap.max_image_size)                    \
    X("colmap_seq_overlap",         colmap.seq_overlap)                       \
    X("colmap_seq_quadratic_overlap", colmap.seq_quadratic_overlap)           \
    X("colmap_estimate_affine_shape", colmap.estimate_affine_shape)           \
    X("colmap_ba_use_gpu",          colmap.ba_use_gpu)                        \
    X("colmap_mapper_extra_params", colmap.mapper_extra_params)               \
    X("colmap_min_num_matches",     colmap.min_num_matches)                   \
    X("colmap_match_max_ratio",     colmap.match_max_ratio)                   \
    X("colmap_min_inliers_per_pair", colmap.min_inliers_per_pair)             \
    X("colmap_abs_pose_min_num_inliers", colmap.abs_pose_min_num_inliers)     \
    X("colmap_abs_pose_min_inlier_ratio", colmap.abs_pose_min_inlier_ratio)   \
    X("colmap_abs_pose_max_error",  colmap.abs_pose_max_error)                \
    X("colmap_merge_models",        colmap.merge_models)                      \
    X("colmap_final_bundle_adjust", colmap.final_bundle_adjust)               \
    X("colmap_vocab_tree_path",     colmap.vocab_tree_path)                   \
    /* end */

template <typename T>
void clamp_to(T& v, T lo, T hi) { v = std::clamp(v, lo, hi); }

template <typename E>
void clamp_enum(E& v, int lo, int hi) { v = (E)std::clamp((int)v, lo, hi); }

// One of `options`, or the first of them. A model name the runners do not
// know reconstructs into nothing, so a hand-edited file cannot carry one.
void clamp_choice(std::string& v, const char* const* options, int n) {
    for (int i = 0; i < n; i++)
        if (v == options[i]) return;
    v = options[0];
}

}  // namespace


// ---------------------------------------------------------------------------
// The built-in presets
// ---------------------------------------------------------------------------

bool is_dataset_preset_name(const std::string& name) {
    for (const DatasetPresetInfo& p : kDatasetPresets)
        if (name == p.name) return true;
    return false;
}


bool dataset_apply_preset(DatasetSettings& s, const std::string& name) {
    if (name == "general") {
        // The base settings, so that picking it is a reset rather than a
        // change: what a freshly opened capture would have had.
        return true;
    }
    if (name == "360-camera") {
        // Two fisheye circles per frame is what a consumer 360 camera writes;
        // dataset_adapt_preset() takes it back to the panorama model for a
        // capture whose own frames measure 2:1.
        s.sfm.camera_model = "thin-prism-fisheye";
        s.colmap.camera_model = "THIN_PRISM_FISHEYE";
        // Whoever holds a 360 camera is in every frame of it, and so are
        // whatever they carry and their shadow.
        s.sfm.prep.mask_enable = true;
        s.mask.prompt = "person; hand; backpack; shadow of person";
        s.sfm.mask_features = true;
        s.border_enable = true;
        return true;
    }
    if (name == "internet-photos") {
        // Photographs from everywhere: no two share a lens, so no two share a
        // camera, and the wide baselines are what the learned frontend is for.
        s.sfm.data_type = 2;
        s.sfm.camera_mode = 2;
        s.colmap.camera_mode = 2;
        s.sfm.features = 2;              // ALIKED-n32
        s.sfm.matcher = 1;               // LightGlue
        s.colmap.feature_type = 1;
        s.colmap.lightglue = true;
        // Distortion fitted per image over a handful of photographs each is
        // free to drift; hold it until the one pass that has every camera.
        s.sfm.distortion_refine = 1;
        s.colmap.mapper_extra_params = 2;
        // Priors are what carries a scene the photographs only half cover.
        s.sfm.geometry.enable = true;
        s.sfm.geometry.want_normal = true;
        s.sfm.geometry.want_depth = true;
        return true;
    }
    return false;
}


void sanitize_dataset_settings(DatasetSettings& s) {
    PrepJob& p = s.sfm.prep;
    clamp_enum(p.photo_import, 0, kNumPhotoImports - 1);
    clamp_enum(p.pano.mode, 0, (int)app::Pano360Mode::Equirect);
    clamp_to(p.pano.size, 0, 16384);
    p.video_fps = std::clamp(p.video_fps, 0.0f, 240.0f);
    p.adaptive_range = std::clamp(p.adaptive_range, 1.0f, 64.0f);
    clamp_to(p.sharp_window, 1, 1000);
    clamp_to(p.max_frames, 1, 1000000);
    if (p.frame_bits != 8 && p.frame_bits != 16) p.frame_bits = 0;
    clamp_to(p.mask_detect_every, 1, 1000);
    clamp_to(p.mask_memory_frames, 0, 64);

    MaskSettings& m = s.mask;
    clamp_to(m.max_image_size, 64, 8192);
    m.dilate_ratio = std::clamp(m.dilate_ratio, 0.0f, 1.0f);
    m.shrink_ratio = std::clamp(m.shrink_ratio, 0.0f, 1.0f);
    m.threshold = std::clamp(m.threshold, 0.0f, 1.0f);
    m.nms = std::clamp(m.nms, 0.0f, 1.0f);

    GeometryJob& g = s.sfm.geometry;
    clamp_to(g.max_size, 64, 8192);
    clamp_to(g.num_tokens, 0, 100000);
    clamp_to(g.jpeg_quality, 1, 100);
    clamp_to(g.ray_depth, 0, 2);
    clamp_to(g.split, 0, 2);

    SfmJob& j = s.sfm;
    clamp_to(j.quality, 0, 3);
    clamp_to(j.data_type, 0, 2);
    clamp_to(j.camera_mode, 0, 2);
    clamp_to(j.pairs, 0, 3);
    clamp_to(j.overlap, 1, 1000);
    clamp_to(j.distortion_refine, 0, 2);
    clamp_to(j.mapper, 0, 1);
    clamp_to(j.features, 0, 2);
    clamp_to(j.matcher, 0, 1);
    clamp_to(j.metric_gps, 0, 2);
    clamp_to(j.sensor_gauge, 0, 2);
    clamp_to(j.exif_attitude, 0, 2);
    clamp_to(j.max_features, 0, 1000000);
    clamp_to(j.max_image_size, 0, 32768);
    j.init_focal_px = std::max(0.0f, j.init_focal_px);
    clamp_choice(j.camera_model, kSfmCameraModels, kNumSfmCameraModels);
    // LightGlue only means something with a learned frontend, and the CLI
    // refuses the combination rather than quietly ignoring it.
    if (j.features == 0) j.matcher = 0;

    ColmapJob& c = s.colmap;
    clamp_choice(c.camera_model, kColmapCameraModels, kNumColmapCameraModels);
    clamp_to(c.camera_mode, 0, 2);
    clamp_to(c.feature_type, 0, 1);
    clamp_to(c.quality, 0, 2);
    clamp_to(c.matcher, 1, 3);
    clamp_to(c.mapper_extra_params, 0, 2);
    clamp_to(c.seq_overlap, 1, 1000);
    clamp_to(c.max_num_features, 0, 1000000);
    clamp_to(c.max_image_size, 0, 32768);
    clamp_to(c.min_num_matches, 0, 100000);
    clamp_to(c.abs_pose_min_num_inliers, 0, 100000);
    clamp_to(c.min_inliers_per_pair, 0, 100000);
    c.init_focal_factor = std::max(0.0f, c.init_focal_factor);
    c.match_max_ratio = std::clamp(c.match_max_ratio, 0.0f, 1.0f);
    c.abs_pose_min_inlier_ratio = std::clamp(c.abs_pose_min_inlier_ratio, 0.0f, 1.0f);
    c.abs_pose_max_error = std::max(0.0f, c.abs_pose_max_error);
}


void save_dataset_preset(const DatasetPreset& p, const std::string& path) {
    PresetHeader head{p.name, p.description, path};
    JsonWriter w = preset_writer(PresetKind::Dataset, head);
    w.key("settings").object();
#define SS_DS_EMIT(key, member) w.field_raw(key, json_field::emit(p.s.member));
    SS_DATASET_PRESET_FIELDS(SS_DS_EMIT)
#undef SS_DS_EMIT
    w.end();
    w.end();
    write_preset_file(path, w.str());
}


DatasetPreset load_dataset_preset(const std::string& path) {
    PresetHeader head;
    const JsonValue root = read_preset_file(path, PresetKind::Dataset, head);
    const JsonValue* fields = root.find("settings");
    if (!fields || !fields->is_object())
        throw std::runtime_error(path + " holds no dataset settings");

    DatasetPreset p;
    p.path = path;
    p.name = head.name;
    p.description = head.description;
#define SS_DS_LOAD(key, member)                                               \
    if (const JsonValue* v = fields->find(key))                               \
        json_field::assign(p.s.member, *v);
    SS_DATASET_PRESET_FIELDS(SS_DS_LOAD)
#undef SS_DS_LOAD
    sanitize_dataset_settings(p.s);
    if (p.name.empty()) p.name = std::filesystem::path(path).stem().string();
    return p;
}


void delete_dataset_preset(const std::string& path) {
    delete_preset_file(path, PresetKind::Dataset);
}


std::vector<DatasetPreset> list_dataset_presets() {
    return list_preset_files(PresetKind::Dataset, load_dataset_preset);
}

}  // namespace gui
