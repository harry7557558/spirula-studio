#include "app/TrainerCore.h"
#include "config/TrainConfigJson.h"

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static void presets() {
    check(spirula::train_config_error(TrainConfig{}).empty(), "default config is valid");
    for (const auto& preset : kTrainPresets) {
        TrainConfig cfg;
        check(train_apply_preset(cfg, preset.name), "built-in preset exists");
        check(spirula::train_config_error(cfg).empty(), preset.name);
    }
}

static void saved_config() {
    TrainConfig cfg;
    train_config_from_json(json_parse(R"({
        "use_bvh": false,
        "use_camera_optimizer": false,
        "deblur_training_images": false,
        "optimizer_offload": "",
        "train_frame": "points",
        "cache_images": "cpu",
        "num_iterations": 1234
    })"), cfg);
    check(spirula::train_config_error(cfg).empty(), "old config still loads");
    check(cfg.cache_images == "cpu" && cfg.num_iterations == 1234,
          "supported settings survive loading an old config");
    for (const auto& [key, value] : train_config_json_pairs(cfg)) {
        const std::string name = key;
        check(name != "use_bvh" && name != "use_camera_optimizer" &&
              name != "deblur_training_images" && name != "optimizer_offload" &&
              name != "train_frame", "retired fields are absent from saved configs");
    }

    train_config_from_json(json_parse(R"({"cache_images": "gpu"})"), cfg);
    const std::string error = spirula::train_config_error(cfg);
    check(error.find("--cache-images") != std::string::npos &&
          error.find("gpu") != std::string::npos,
          "GPU cache in a saved config is rejected with the flag and value");
    cfg.cache_images = "disk";
    check(spirula::train_config_error(cfg).empty(), "disk cache is valid");
    cfg.primitive = "unknown";
    check(!spirula::train_config_error(cfg).empty(), "unknown saved primitive is rejected");
    cfg.primitive = "3dgs";
    cfg.quantization_level = 2;
    check(!spirula::train_config_error(cfg).empty(), "quantization validation is preserved");
}

static void test_suppress_initial_scales() {
    ColmapPoints3D pts;
    pts.xyz = {
        0.0, 0.0, 2.0,
        0.05, 0.0, 2.0,
        -0.05, 0.0, 2.0,
        0.0, 0.05, 2.0,
        0.0, -0.05, 2.0,
        0.0, 0.0, 30.0
    };
    pts.rgb.assign(6 * 3, 200);

    TrainConfig cfg;
    cfg.cap_max = 10;
    cfg.min_init_fraction = 0.1f;
    cfg.preallocate_splat_tensors = false;
    spirula::ColorResolution color;

    cfg.suppress_initial_scales = false;
    spirula::SeedSplats s_nosupp = spirula::seed_splats(pts, cfg, color);

    PostSplitCameras post;
    post.n_post = 1;
    post.post_widths = {800};
    post.post_heights = {600};
    post.intrins = {500.f, 500.f, 400.f, 300.f};
    post.post_models = {0};
    post.viewmats = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    cfg.suppress_initial_scales = true;
    spirula::SeedSplats s_supp = spirula::seed_splats(pts, cfg, color, &post);

    check(s_nosupp.num == 6 && s_supp.num == 6, "seed splats count matches");
    check(s_supp.scales[5 * 3] < s_nosupp.scales[5 * 3],
          "isolated floater scale is suppressed");
}

int main() {
    presets();
    saved_config();
    test_suppress_initial_scales();
    if (failures) return 1;
    std::puts("train_config_test: OK");
    return 0;
}
