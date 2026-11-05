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

static void background_export_height() {
    TrainConfig cfg;
    check(cfg.background_export_height == 512, "background export defaults to 512 pixels");
    train_config_from_json(json_parse(R"({"background_export_height": 768})"), cfg);
    check(cfg.background_export_height == 768, "background export height loads from saved config");
    bool saved = false;
    for (const auto& [key, value] : train_config_json_pairs(cfg))
        if (std::string(key) == "background_export_height") saved = value == "768";
    check(saved, "background export height is serialized");
    for (int height : {1, 512, 768, 8192}) {
        cfg.background_export_height = height;
        check(spirula::train_config_error(cfg).empty(), "valid background export height accepted");
    }
    for (int height : {-1, 0, 8193, 2147483647}) {
        cfg.background_export_height = height;
        check(spirula::train_config_error(cfg).find("--background-export-height") != std::string::npos,
              "invalid background export height rejected before training");
    }
}

int main() {
    presets();
    saved_config();
    background_export_height();
    if (failures) return 1;
    std::puts("train_config_test: OK");
    return 0;
}
