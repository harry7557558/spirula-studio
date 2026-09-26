// Every field a dataset or meshing preset carries must survive a save and a
// load. The tables in DatasetPreset.cpp / MeshPreset.cpp are hand-written, so
// a field added to one of the job structs and forgotten there is silent: the
// preset saves, loads, and quietly runs with the default.
//
// This catches the forgetting by writing a value into every field the table
// names, reading it back and comparing -- which is why it moves each field to
// something that is NOT its default.

#include "app/gui/DatasetPreset.h"
#include "app/gui/MeshPreset.h"
#include "core/SourcePath.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d  %s\n", SS_FILE, __LINE__, #cond);       \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        if (!((a) == (b))) {                                                  \
            std::printf("FAIL %s:%d  %s != %s\n", SS_FILE, __LINE__, #a, #b); \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static fs::path scratch() {
    fs::path d = fs::temp_directory_path() / "spirula_preset_test";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

static void test_dataset_preset() {
    gui::DatasetPreset p;
    p.name = "Round trip";
    p.description = "every field moved off its default";

    gui::DatasetSettings& s = p.s;
    s.colmap_engine = true;
    s.use_found_masks = false;
    s.border_enable = true;
    s.mask_model_id = "sam2.1_hiera_large";
    s.frame_shapes = "selfie stick";

    s.sfm.prep.resume = false;
    s.sfm.prep.photo_import = gui::PhotoImport::Move;
    s.sfm.prep.flip_found_masks = true;
    s.sfm.prep.video_fps = 5.5f;
    s.sfm.prep.adaptive_fps = true;
    s.sfm.prep.adaptive_range = 2.5f;
    s.sfm.prep.sharp_window = 7;
    s.sfm.prep.sync_tracks = false;
    s.sfm.prep.max_frames = 1234;
    s.sfm.prep.auto_rotate = false;
    s.sfm.prep.force_external_decode = true;
    s.sfm.prep.frame_bits = 16;
    s.sfm.prep.pano.mode = app::Pano360Mode::Equirect;
    s.sfm.prep.pano.size = 2048;
    s.sfm.prep.pano.yaw = 10.0f;
    s.sfm.prep.pano.pitch = -20.0f;
    s.sfm.prep.pano.roll = 180.0f;

    s.sfm.image_gamut = "Display P3";
    s.sfm.image_is_linear = true;
    s.sfm.point_color_in_image_space = true;

    s.sfm.prep.mask_enable = true;
    s.sfm.mask_features = false;
    s.mask.prompt = "people; cars";
    s.mask.negative_prompt = "statue";
    s.mask.keep_subject = true;
    s.mask.dilate_ratio = 0.25f;
    s.mask.shrink_ratio = 0.125f;
    s.mask.max_image_size = 1024;
    s.mask.threshold = 0.75f;
    s.mask.nms = 0.4f;
    s.sfm.prep.mask_memory = true;
    s.sfm.prep.mask_detect_every = 4;
    s.sfm.prep.mask_memory_frames = 5;
    s.sfm.prep.force_external_masking = true;

    s.sfm.geometry.enable = true;
    s.sfm.geometry.model = "moge2-vitl";
    s.sfm.geometry.max_size = 800;
    s.sfm.geometry.num_tokens = 2400;
    s.sfm.geometry.want_normal = false;
    s.sfm.geometry.want_depth = true;
    s.sfm.geometry.normal_jpg = true;
    s.sfm.geometry.jpeg_quality = 80;
    s.sfm.geometry.depth_mm = true;
    s.sfm.geometry.ray_depth = 1;
    s.sfm.geometry.split = 2;
    s.sfm.geometry.overwrite = true;

    s.sfm.quality = 1;
    s.sfm.data_type = 2;
    s.sfm.camera_model = "opencv-fisheye";
    s.sfm.camera_mode = 2;
    s.sfm.pairs = 3;
    s.sfm.overlap = 25;
    s.sfm.loop_closure = false;
    s.sfm.use_sequence = false;
    s.sfm.init_focal_px = 1500.0f;
    s.sfm.init_distortion = "0.1,0.02";
    s.sfm.distortion_refine = 2;
    s.sfm.final_per_image_intrinsics = true;
    s.sfm.final_free_rig = true;
    s.sfm.max_features = 8192;
    s.sfm.max_image_size = 2000;
    s.sfm.mapper = 1;
    s.sfm.features = 1;
    s.sfm.matcher = 1;
    s.sfm.metric_gps = 2;
    s.sfm.sensor_gauge = 1;
    s.sfm.exif_attitude = 1;
    s.sfm.keep_intermediate = false;
    s.sfm.ba_cpu = true;
    s.sfm.subprocess = true;
    s.sfm.extra_args = "--some-flag 3";

    s.colmap.camera_model = "FULL_OPENCV";
    s.colmap.camera_mode = 2;
    s.colmap.init_focal_factor = 0.3f;
    s.colmap.camera_params = "1,2,3";
    s.colmap.feature_type = 1;
    s.colmap.lightglue = true;
    s.colmap.quality = 2;
    s.colmap.matcher = 3;
    s.colmap.seq_loop_closure = false;
    s.colmap.max_num_features = 4096;
    s.colmap.max_image_size = 1600;
    s.colmap.seq_overlap = 15;
    s.colmap.seq_quadratic_overlap = false;
    s.colmap.estimate_affine_shape = true;
    s.colmap.ba_use_gpu = false;
    s.colmap.mapper_extra_params = 2;
    s.colmap.min_num_matches = 20;
    s.colmap.match_max_ratio = 0.7f;
    s.colmap.min_inliers_per_pair = 30;
    s.colmap.abs_pose_min_num_inliers = 40;
    s.colmap.abs_pose_min_inlier_ratio = 0.35f;
    s.colmap.abs_pose_max_error = 8.0f;
    s.colmap.merge_models = false;
    s.colmap.final_bundle_adjust = false;
    s.colmap.vocab_tree_path = "C:/tmp/vocab.bin";

    const std::string path = (scratch() / "dataset.json").string();
    gui::save_dataset_preset(p, path);
    const gui::DatasetPreset back = gui::load_dataset_preset(path);

    CHECK_EQ(back.name, p.name);
    CHECK_EQ(back.description, p.description);

    const gui::DatasetSettings& b = back.s;
    CHECK_EQ(b.colmap_engine, s.colmap_engine);
    CHECK_EQ(b.use_found_masks, s.use_found_masks);
    CHECK_EQ(b.border_enable, s.border_enable);
    CHECK_EQ(b.mask_model_id, s.mask_model_id);
    CHECK_EQ(b.frame_shapes, s.frame_shapes);

    CHECK_EQ(b.sfm.prep.resume, s.sfm.prep.resume);
    CHECK(b.sfm.prep.photo_import == s.sfm.prep.photo_import);
    CHECK_EQ(b.sfm.prep.flip_found_masks, s.sfm.prep.flip_found_masks);
    CHECK_EQ(b.sfm.prep.video_fps, s.sfm.prep.video_fps);
    CHECK_EQ(b.sfm.prep.adaptive_fps, s.sfm.prep.adaptive_fps);
    CHECK_EQ(b.sfm.prep.adaptive_range, s.sfm.prep.adaptive_range);
    CHECK_EQ(b.sfm.prep.sharp_window, s.sfm.prep.sharp_window);
    CHECK_EQ(b.sfm.prep.sync_tracks, s.sfm.prep.sync_tracks);
    CHECK_EQ(b.sfm.prep.max_frames, s.sfm.prep.max_frames);
    CHECK_EQ(b.sfm.prep.auto_rotate, s.sfm.prep.auto_rotate);
    CHECK_EQ(b.sfm.prep.force_external_decode, s.sfm.prep.force_external_decode);
    CHECK_EQ(b.sfm.prep.frame_bits, s.sfm.prep.frame_bits);
    CHECK(b.sfm.prep.pano.mode == s.sfm.prep.pano.mode);
    CHECK_EQ(b.sfm.prep.pano.size, s.sfm.prep.pano.size);
    CHECK_EQ(b.sfm.prep.pano.yaw, s.sfm.prep.pano.yaw);
    CHECK_EQ(b.sfm.prep.pano.pitch, s.sfm.prep.pano.pitch);
    CHECK_EQ(b.sfm.prep.pano.roll, s.sfm.prep.pano.roll);

    CHECK_EQ(b.sfm.image_gamut, s.sfm.image_gamut);
    CHECK(b.sfm.image_is_linear == s.sfm.image_is_linear);
    CHECK_EQ(b.sfm.point_color_in_image_space, s.sfm.point_color_in_image_space);

    CHECK_EQ(b.sfm.prep.mask_enable, s.sfm.prep.mask_enable);
    CHECK_EQ(b.sfm.mask_features, s.sfm.mask_features);
    CHECK_EQ(b.mask.prompt, s.mask.prompt);
    CHECK_EQ(b.mask.negative_prompt, s.mask.negative_prompt);
    CHECK_EQ(b.mask.keep_subject, s.mask.keep_subject);
    CHECK_EQ(b.mask.dilate_ratio, s.mask.dilate_ratio);
    CHECK_EQ(b.mask.shrink_ratio, s.mask.shrink_ratio);
    CHECK_EQ(b.mask.max_image_size, s.mask.max_image_size);
    CHECK_EQ(b.mask.threshold, s.mask.threshold);
    CHECK_EQ(b.mask.nms, s.mask.nms);
    CHECK_EQ(b.sfm.prep.mask_memory, s.sfm.prep.mask_memory);
    CHECK_EQ(b.sfm.prep.mask_detect_every, s.sfm.prep.mask_detect_every);
    CHECK_EQ(b.sfm.prep.mask_memory_frames, s.sfm.prep.mask_memory_frames);
    CHECK_EQ(b.sfm.prep.force_external_masking, s.sfm.prep.force_external_masking);

    CHECK_EQ(b.sfm.geometry.enable, s.sfm.geometry.enable);
    CHECK_EQ(b.sfm.geometry.model, s.sfm.geometry.model);
    CHECK_EQ(b.sfm.geometry.max_size, s.sfm.geometry.max_size);
    CHECK_EQ(b.sfm.geometry.num_tokens, s.sfm.geometry.num_tokens);
    CHECK_EQ(b.sfm.geometry.want_normal, s.sfm.geometry.want_normal);
    CHECK_EQ(b.sfm.geometry.want_depth, s.sfm.geometry.want_depth);
    CHECK_EQ(b.sfm.geometry.normal_jpg, s.sfm.geometry.normal_jpg);
    CHECK_EQ(b.sfm.geometry.jpeg_quality, s.sfm.geometry.jpeg_quality);
    CHECK_EQ(b.sfm.geometry.depth_mm, s.sfm.geometry.depth_mm);
    CHECK_EQ(b.sfm.geometry.ray_depth, s.sfm.geometry.ray_depth);
    CHECK_EQ(b.sfm.geometry.split, s.sfm.geometry.split);
    CHECK_EQ(b.sfm.geometry.overwrite, s.sfm.geometry.overwrite);

    CHECK_EQ(b.sfm.quality, s.sfm.quality);
    CHECK_EQ(b.sfm.data_type, s.sfm.data_type);
    CHECK_EQ(b.sfm.camera_model, s.sfm.camera_model);
    CHECK_EQ(b.sfm.camera_mode, s.sfm.camera_mode);
    CHECK_EQ(b.sfm.pairs, s.sfm.pairs);
    CHECK_EQ(b.sfm.overlap, s.sfm.overlap);
    CHECK_EQ(b.sfm.loop_closure, s.sfm.loop_closure);
    CHECK_EQ(b.sfm.use_sequence, s.sfm.use_sequence);
    CHECK_EQ(b.sfm.init_focal_px, s.sfm.init_focal_px);
    CHECK_EQ(b.sfm.init_distortion, s.sfm.init_distortion);
    CHECK_EQ(b.sfm.distortion_refine, s.sfm.distortion_refine);
    CHECK_EQ(b.sfm.final_per_image_intrinsics, s.sfm.final_per_image_intrinsics);
    CHECK_EQ(b.sfm.final_free_rig, s.sfm.final_free_rig);
    CHECK_EQ(b.sfm.max_features, s.sfm.max_features);
    CHECK_EQ(b.sfm.max_image_size, s.sfm.max_image_size);
    CHECK_EQ(b.sfm.mapper, s.sfm.mapper);
    CHECK_EQ(b.sfm.features, s.sfm.features);
    CHECK_EQ(b.sfm.matcher, s.sfm.matcher);
    CHECK_EQ(b.sfm.metric_gps, s.sfm.metric_gps);
    CHECK_EQ(b.sfm.sensor_gauge, s.sfm.sensor_gauge);
    CHECK_EQ(b.sfm.exif_attitude, s.sfm.exif_attitude);
    CHECK_EQ(b.sfm.keep_intermediate, s.sfm.keep_intermediate);
    CHECK_EQ(b.sfm.ba_cpu, s.sfm.ba_cpu);
    CHECK_EQ(b.sfm.subprocess, s.sfm.subprocess);
    CHECK_EQ(b.sfm.extra_args, s.sfm.extra_args);

    CHECK_EQ(b.colmap.camera_model, s.colmap.camera_model);
    CHECK_EQ(b.colmap.camera_mode, s.colmap.camera_mode);
    CHECK_EQ(b.colmap.init_focal_factor, s.colmap.init_focal_factor);
    CHECK_EQ(b.colmap.camera_params, s.colmap.camera_params);
    CHECK_EQ(b.colmap.feature_type, s.colmap.feature_type);
    CHECK_EQ(b.colmap.lightglue, s.colmap.lightglue);
    CHECK_EQ(b.colmap.quality, s.colmap.quality);
    CHECK_EQ(b.colmap.matcher, s.colmap.matcher);
    CHECK_EQ(b.colmap.seq_loop_closure, s.colmap.seq_loop_closure);
    CHECK_EQ(b.colmap.max_num_features, s.colmap.max_num_features);
    CHECK_EQ(b.colmap.max_image_size, s.colmap.max_image_size);
    CHECK_EQ(b.colmap.seq_overlap, s.colmap.seq_overlap);
    CHECK_EQ(b.colmap.seq_quadratic_overlap, s.colmap.seq_quadratic_overlap);
    CHECK_EQ(b.colmap.estimate_affine_shape, s.colmap.estimate_affine_shape);
    CHECK_EQ(b.colmap.ba_use_gpu, s.colmap.ba_use_gpu);
    CHECK_EQ(b.colmap.mapper_extra_params, s.colmap.mapper_extra_params);
    CHECK_EQ(b.colmap.min_num_matches, s.colmap.min_num_matches);
    CHECK_EQ(b.colmap.match_max_ratio, s.colmap.match_max_ratio);
    CHECK_EQ(b.colmap.min_inliers_per_pair, s.colmap.min_inliers_per_pair);
    CHECK_EQ(b.colmap.abs_pose_min_num_inliers, s.colmap.abs_pose_min_num_inliers);
    CHECK_EQ(b.colmap.abs_pose_min_inlier_ratio, s.colmap.abs_pose_min_inlier_ratio);
    CHECK_EQ(b.colmap.abs_pose_max_error, s.colmap.abs_pose_max_error);
    CHECK_EQ(b.colmap.merge_models, s.colmap.merge_models);
    CHECK_EQ(b.colmap.final_bundle_adjust, s.colmap.final_bundle_adjust);
    CHECK_EQ(b.colmap.vocab_tree_path, s.colmap.vocab_tree_path);

    // Clicks are prompts for particular frames and must never travel.
    CHECK(b.mask.clicks.empty());
}

static void test_mesh_preset() {
    gui::MeshPreset p;
    p.name = "Textured";
    p.description = "glb with an atlas";
    p.job.use_data = false;
    p.job.colors[1] = false;
    p.job.colors[2] = true;
    p.job.formats[0] = false;
    p.job.formats[3] = true;
    p.job.max_cameras = 40;
    p.job.texture_size = 4096;
    p.job.iso = 0.4f;
    p.job.bisection_iters = 5;
    p.job.merge_factor = 2.5f;
    p.job.quality_iters = 6;
    p.job.floater_min_faces = 50;
    p.job.cull_unseen = false;
    p.job.carve_k = 3;
    p.job.extra_args = "--verbose";
    // What the preset must NOT carry.
    p.job.checkpoint = "C:/runs/one";
    p.job.data_dir = "C:/datasets/one";
    p.job.output = "C:/runs/one/mesh";

    const std::string path = (scratch() / "mesh.json").string();
    gui::save_mesh_preset(p, path);
    const gui::MeshPreset back = gui::load_mesh_preset(path);

    CHECK_EQ(back.name, p.name);
    CHECK_EQ(back.description, p.description);
    CHECK_EQ(back.job.use_data, p.job.use_data);
    for (int i = 0; i < gui::kNumMeshColorModes; i++)
        CHECK_EQ(back.job.colors[i], p.job.colors[i]);
    for (int i = 0; i < gui::kNumMeshFormats; i++)
        CHECK_EQ(back.job.formats[i], p.job.formats[i]);
    CHECK_EQ(back.job.max_cameras, p.job.max_cameras);
    CHECK_EQ(back.job.texture_size, p.job.texture_size);
    CHECK_EQ(back.job.iso, p.job.iso);
    CHECK_EQ(back.job.bisection_iters, p.job.bisection_iters);
    CHECK_EQ(back.job.merge_factor, p.job.merge_factor);
    CHECK_EQ(back.job.quality_iters, p.job.quality_iters);
    CHECK_EQ(back.job.floater_min_faces, p.job.floater_min_faces);
    CHECK_EQ(back.job.cull_unseen, p.job.cull_unseen);
    CHECK_EQ(back.job.carve_k, p.job.carve_k);
    CHECK_EQ(back.job.extra_args, p.job.extra_args);
    CHECK(back.job.checkpoint.empty());
    CHECK(back.job.data_dir.empty());
    CHECK(back.job.output.empty());
}

// A file a user has edited by hand must not reach a runner with a value
// nothing checked: an hour into an unattended queue is the wrong time.
static void test_sanitize() {
    gui::DatasetSettings s;
    s.sfm.quality = 99;
    s.sfm.camera_model = "not-a-lens";
    s.sfm.features = 0;
    s.sfm.matcher = 1;          // LightGlue without a learned frontend
    s.sfm.prep.sharp_window = -3;
    s.sfm.prep.frame_bits = 12;
    s.sfm.prep.adaptive_range = 0.1f;
    s.mask.threshold = 4.0f;
    s.colmap.matcher = 0;
    s.colmap.camera_model = "NONSENSE";
    gui::sanitize_dataset_settings(s);
    CHECK(s.sfm.quality >= 0 && s.sfm.quality <= 3);
    CHECK_EQ(s.sfm.camera_model, std::string("opencv"));
    CHECK_EQ(s.sfm.matcher, 0);
    CHECK(s.sfm.prep.sharp_window >= 1);
    CHECK_EQ(s.sfm.prep.frame_bits, 0);
    CHECK(s.sfm.prep.adaptive_range >= 1.0f);
    CHECK(s.mask.threshold <= 1.0f);
    CHECK(s.colmap.matcher >= 1);
    CHECK_EQ(s.colmap.camera_model, std::string("OPENCV"));

    // The colours and the formats have to have a pair between them, or the
    // run writes nothing at all.
    gui::MeshJob job;
    for (int i = 0; i < gui::kNumMeshColorModes; i++) job.colors[i] = false;
    job.colors[2] = true;
    for (int i = 0; i < gui::kNumMeshFormats; i++) job.formats[i] = false;
    job.formats[0] = true;      // PLY cannot carry a texture
    gui::sanitize_mesh_job(job);
    CHECK(!gui::mesh_job_writes_nothing(job));

    // A run asking for two colours writes each at its own base path, and one
    // asking for a single colour writes where it always has.
    gui::MeshJob two;
    two.output = "C:/runs/one/mesh";
    two.colors[0] = true;       // none + vertex
    two.formats[0] = true;      // ply
    const std::vector<std::string> outs = gui::mesh_job_outputs(two);
    CHECK_EQ(outs.size(), (size_t)2);
    CHECK_EQ(outs[0], std::string("C:/runs/one/mesh_vertexcolor.ply"));
    CHECK_EQ(outs[1], std::string("C:/runs/one/mesh_nocolor.ply"));
    two.colors[0] = false;
    CHECK_EQ(gui::mesh_job_outputs(two).size(), (size_t)1);
    CHECK_EQ(gui::mesh_job_outputs(two)[0], std::string("C:/runs/one/mesh.ply"));
}

// A preset of one kind must not load as another, whatever its name is.
static void test_kinds_do_not_cross() {
    const std::string ds = (scratch() / "dataset.json").string();
    const std::string mesh = (scratch() / "mesh.json").string();
    bool threw = false;
    try {
        (void)gui::load_mesh_preset(ds);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
    threw = false;
    try {
        (void)gui::load_dataset_preset(mesh);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    test_dataset_preset();
    test_mesh_preset();
    test_sanitize();
    test_kinds_do_not_cross();
    if (failures) {
        std::printf("preset_roundtrip_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("preset_roundtrip_test: OK\n");
    return 0;
}
