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
        "use_revised_densification": false,
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
              name != "train_frame" && name != "use_revised_densification",
              "retired fields are absent from saved configs");
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

static void mask_mode_resolution() {
    TrainConfig cfg;
    check(cfg.mask_mode == "auto", "mask_mode defaults to auto");
    check(resolve_train_mask_mode(cfg) == ResolvedMaskMode::Ignore,
          "default auto mask_mode resolves to ignore");
    cfg.apply_loss_for_mask = true;
    check(resolve_train_mask_mode(cfg) == ResolvedMaskMode::Segment,
          "auto + apply_loss_for_mask=true resolves to segment");
    cfg.mask_mode = "segment_and_ignore";
    check(train_mask_is_segment_and_ignore(cfg) &&
          train_mask_applies_alpha_loss(cfg) &&
          train_mask_enabled(cfg),
          "explicit segment_and_ignore overrides auto flags");
    cfg.mask_mode = "ignore";
    check(!train_mask_applies_alpha_loss(cfg) &&
          train_mask_enabled(cfg),
          "explicit ignore overrides apply_loss_for_mask preset");
    cfg.mask_mode = "off";
    check(!train_mask_enabled(cfg) &&
          !train_mask_applies_alpha_loss(cfg),
          "explicit off disables masks");

    train_config_from_json(json_parse(R"({"mask_mode": "segment_and_ignore"})"), cfg);
    check(spirula::train_config_error(cfg).empty(), "segment_and_ignore loads cleanly");
    check(cfg.mask_mode == "segment_and_ignore", "mask_mode survives JSON load");
    bool saved = false;
    for (const auto& [key, value] : train_config_json_pairs(cfg))
        if (std::string(key) == "mask_mode") saved = value == "\"segment_and_ignore\"";
    check(saved, "mask_mode is serialized");

    cfg.mask_mode = "bogus";
    check(spirula::train_config_error(cfg).find("--mask-mode") != std::string::npos,
          "invalid mask_mode is rejected");
}

static void background_options() {
    TrainConfig cfg;
    check(cfg.background_mode == "black" && cfg.background_transparency_suppression == "none",
          "background defaults to black, no draw");
    const struct { const char* old_mode; const char* mode; const char* draw; } legacy[] = {
        {"sh", "skybox", "none"},       {"noise", "black", "noise"},
        {"pseudorandom", "black", "pseudorandom"}, {"random", "black", "random"},
        {"color", "color", "none"}};
    for (const auto& l : legacy) {
        TrainConfig c;
        train_config_from_json(
            json_parse(std::string("{\"background_mode\": \"") + l.old_mode + "\"}"), c);
        check(c.background_mode == l.mode && c.background_transparency_suppression == l.draw &&
                  spirula::train_config_error(c).empty(),
              "a saved background_mode from before the split still loads");
    }

    cfg.mask_mode = "ignore";
    cfg.preserve_background = true;
    check(!train_mask_preserves_background(cfg), "preserve_background: no effect in ignore mode");
    cfg.preserve_background.reset();
    cfg.mask_mode = "segment";
    check(!train_mask_preserves_background(cfg), "preserve_background: segment defaults off");
    cfg.background_mode = "skybox";
    cfg.background_transparency_suppression = "noise";
    check(train_mask_preserves_background(cfg),
          "preserve_background: segment defaults on for a skybox under a transparency draw");
    cfg.background_transparency_suppression = "none";
    check(!train_mask_preserves_background(cfg),
          "preserve_background: a skybox without a transparency draw stays off");
    cfg.mask_mode = "segment_and_ignore";
    check(train_mask_preserves_background(cfg),
          "preserve_background: segment_and_ignore defaults on");
    cfg.preserve_background = false;
    check(!train_mask_preserves_background(cfg), "preserve_background: explicit false wins");
    cfg.mask_mode = "segment";
    cfg.preserve_background = true;
    check(train_mask_preserves_background(cfg), "preserve_background: explicit true wins");
}

static void step_config_loss_map_gating() {
    TrainConfig cfg;
    spirula::RunState st;
    // Default stop is max(14000, 30000 - 2500) = 27500.
    const EngineStepConfig early = spirula::build_step_config(cfg, st, 0);
    const EngineStepConfig last = spirula::build_step_config(cfg, st, 27499);
    const EngineStepConfig post = spirula::build_step_config(cfg, st, 27500);
    check(early.loss.loss_map_mode != 0 && early.loss.compute_loss_map,
          "loss map active at step 0");
    check(last.loss.compute_loss_map, "loss map active on final densify step");
    check(post.loss.loss_map_mode != 0 && !post.loss.compute_loss_map,
          "loss map disabled post-densify while preserving configured mode");

    cfg.densify_learned_score = true;
    check(spirula::build_step_config(cfg, st, 27499).loss.learned_score,
          "learned score active before densify stop");
    check(!spirula::build_step_config(cfg, st, 27500).loss.learned_score,
          "learned score disabled after densify stop");
    cfg.densify_learned_score = false;

    cfg.densify_score_blend_world_grad = 0.5f;
    check(spirula::build_step_config(cfg, st, 27499).optim.write_densify_world_grad_score,
          "world-grad score active before densify stop");
    check(!spirula::build_step_config(cfg, st, 27500).optim.write_densify_world_grad_score,
          "world-grad score disabled after densify stop");

    cfg.refine_every = 0;
    check(!spirula::build_step_config(cfg, st, 0).loss.compute_loss_map &&
              !spirula::build_step_config(cfg, st, 0).optim.write_densify_world_grad_score,
          "disabled refinement skips loss map and world-grad score");
}

int main() {
    presets();
    background_options();
    saved_config();
    test_suppress_initial_scales();
    background_export_height();
    mask_mode_resolution();
    step_config_loss_map_gating();
    if (failures) return 1;
    std::puts("train_config_test: OK");
    return 0;
}
