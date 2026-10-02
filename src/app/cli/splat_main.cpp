#include "app/Tools.h"

#include "app/EvalMetrics.h"
#include "app/TrainerCore.h"
#include "checkpoint/Resume.h"
#include "checkpoint/SplatPly.h"
#include "checkpoint/SplatPrune.h"
#include "engine/Engine.h"
#include "i18n/catalog/Cli.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace cmsg = spirula::i18n::msg::cli;
using spirula::i18n::format;

namespace {

struct EngineCleanup {
    ~EngineCleanup() {
        try {
            engine_reset();
        } catch (...) {
        }
    }
};

void usage(const char* argv0) {
    std::printf("Usage: %s validate <model> [--data <dataset>] [--primitive <p>] [--max-cameras <n>] [--visibility]\n"
                "       %s prune <model> --output <model.ply> [--keep-fraction <f> "
                "--data <dataset> [--primitive <p>] [--max-cameras <n>]] [--min-opacity <a>] "
                "[--sh-degree <n> [--compare-cameras <n>]]\n"
                "       %s compare <reference> <candidate> --data <dataset> "
                "[--primitive <p>] [--max-cameras <n>]\n"
                "       %s evaluate <model> --data <dataset> [--max-cameras <n>] "
                "[--split <train|validation>] [--primitive <p>]\n"
                "       p = 3dgs | mip | 3dgut (overrides config.json)\n",
                argv0, argv0, argv0, argv0);
}

int parse_int(const char* value) {
    try {
        size_t p = 0;
        const int out = std::stoi(value, &p);
        if (value[p] == '\0' && out > 0) return out;
    } catch (...) {
    }
    throw std::runtime_error("--max-cameras: expected a positive integer");
}

double parse_fraction(const char* value) {
    try {
        size_t p = 0;
        const double out = std::stod(value, &p);
        if (value[p] == '\0' && std::isfinite(out) && out > 0.0 && out <= 1.0)
            return out;
    } catch (...) {
    }
    throw std::runtime_error("--keep-fraction: expected a number in (0, 1]");
}

float parse_opacity(const char* value) {
    try {
        size_t p = 0;
        const float out = std::stof(value, &p);
        if (value[p] == '\0' && std::isfinite(out) && out >= 0.0f && out < 1.0f)
            return out;
    } catch (...) {
    }
    throw std::runtime_error("--min-opacity: expected a number in [0, 1)");
}

int parse_sh_degree(const char* value) {
    try {
        size_t p = 0;
        const int out = std::stoi(value, &p);
        if (value[p] == '\0' && out >= 0 && out <= 4) return out;
    } catch (...) {
    }
    throw std::runtime_error("--sh-degree: expected an integer from 0 through 4");
}

std::string parse_primitive(const char* value) {
    const std::string primitive = value;
    if (primitive == "3dgs" || primitive == "mip" || primitive == "3dgut")
        return primitive;
    throw std::runtime_error("--primitive: expected 3dgs, mip, or 3dgut");
}

TrainConfig replay_config(const std::string& ply, const std::string& run,
                         const std::string& data, const std::string& primitive,
                         const spirula::SplatCloud& cloud) {
    const fs::path config_path = fs::path(run) / "config.json";
    TrainConfig cfg;
    if (fs::is_regular_file(config_path)) {
        cfg = ckpt::config_from_json(config_path);
        if (!data.empty()) cfg.data = data;
        else if (fs::path(cfg.data).is_relative())
            cfg.data = (fs::path(run) / cfg.data).string();
    } else {
        if (data.empty())
            throw std::runtime_error("no config.json beside model; supply --data <dataset> for a standalone model");
        cfg.data = data;
    }
    cfg.init_ply = ply;
    cfg.init_ply_add_points = false;
    cfg.resume.clear();
    cfg.cap_max = (int)cloud.num;
    cfg.preallocate_splat_tensors = false;
    cfg.sh_degree = cloud.sh_degree;
    cfg.cache_images = "disk";
    if (!primitive.empty()) cfg.primitive = primitive;
    return cfg;
}

TorchTensorView float_view(const std::vector<float>& value,
                           std::vector<int64_t> shape) {
    return {(uint64_t)(uintptr_t)value.data(), 4, std::move(shape)};
}

void upload_scene(int slot, const spirula::SplatCloud& cloud) {
    const int64_t k = cloud.dim_sh() - 1;
    engine_scene_set_data_3dgs(
        slot, cloud.num,
        float_view(cloud.means, {cloud.num, 3}),
        float_view(cloud.quats, {cloud.num, 4}),
        float_view(cloud.scales, {cloud.num, 3}),
        float_view(cloud.opacities, {cloud.num, 1}),
        float_view(cloud.features_dc, {cloud.num, 3}),
        float_view(cloud.features_sh, {cloud.num, k, 3}));
}

struct ImageMetrics {
    double sum_abs = 0.0, sum_sq = 0.0, sum_ssim = 0.0;
    int64_t pixels = 0, views = 0;

    void add(const float* reference, const float* candidate, int H, int W, int C) {
        const int64_t n = (int64_t)H * W * C;
        for (int64_t i = 0; i < n; ++i) {
            const double d = (double)reference[i] - (double)candidate[i];
            sum_abs += std::abs(d);
            sum_sq += d * d;
        }
        sum_ssim += spirula::image_ssim(reference, candidate, H, W, C);
        pixels += n;
        ++views;
    }

    void print(const spirula::i18n::Msg& label) const {
        const double n = (double)std::max<int64_t>(pixels, 1);
        const double mse = sum_sq / n;
        const double psnr = mse == 0.0 ? std::numeric_limits<double>::infinity()
                                       : 10.0 * std::log10(1.0 / mse);
        std::printf("%s\n", format(label, {
            (long long)views, sum_abs / n, psnr,
            sum_ssim / (double)std::max<int64_t>(views, 1)}).c_str());
    }

    void print(const char* label) const {
        const double n = (double)std::max<int64_t>(pixels, 1);
        const double mse = sum_sq / n;
        const double psnr = mse == 0.0 ? std::numeric_limits<double>::infinity()
                                       : 10.0 * std::log10(1.0 / mse);
        std::printf("%s: views=%lld, l1=%.6g, psnr=%.6g dB, ssim=%.6g\n",
                    label, (long long)views, sum_abs / n, psnr,
                    sum_ssim / (double)std::max<int64_t>(views, 1));
    }
};

int evaluate_model(int argc, char** argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    std::string data;
    std::string split = "train";
    std::string primitive;
    int max_cameras = -1;
    for (int i = 3; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--data") {
            if (++i == argc) throw std::runtime_error("--data: missing value");
            data = argv[i];
        } else if (key == "--max-cameras") {
            if (++i == argc) throw std::runtime_error("--max-cameras: missing value");
            max_cameras = parse_int(argv[i]);
        } else if (key == "--split") {
            if (++i == argc) throw std::runtime_error("--split: missing value");
            split = argv[i];
        } else if (key == "--primitive") {
            if (++i == argc) throw std::runtime_error("--primitive: missing value");
            primitive = parse_primitive(argv[i]);
        } else {
            throw std::runtime_error("unknown flag: " + key);
        }
    }
    if (split != "train" && split != "validation")
        throw std::runtime_error("--split: expected train or validation");

    auto [ply, run] = spirula::find_splat_ply(argv[2]);
    const spirula::SplatCloud cloud = spirula::read_splat_ply(ply);
    TrainConfig cfg = replay_config(ply, run, data, primitive, cloud);
    spirula::TrainerSession session;
    session.cfg = cfg;
    session.create_output_dir = false;
    session.write_config_json = false;
    session.log_fn = [](const std::string& line) { std::printf("%s\n", line.c_str()); };
    session.check_config();
    session.load_dataset();
    session.cfg.relative_scale.reset();
    session.setup_engine();

    ImageMetrics raw, corrected;
    const auto& indices = split == "train" ? session.ds.train_indices
                                             : session.ds.val_indices;
    if (indices.empty()) throw std::runtime_error(cmsg::splat_selected_split_empty.get());
    const int total = (int)indices.size();
    const int limit = max_cameras < 0 ? total : std::min(total, max_cameras);
    std::vector<float> render, gt, cc;
    for (int i = 0; i < limit; ++i) {
        for (int pass = 0, passes = 1; pass < passes; ++pass) {
            int rendered_passes = 0;
            engine_preview_forward(indices[(size_t)i], cfg.primitive,
                                   cloud.sh_degree, cfg.packed, false, LossConfig{},
                                   pass, &rendered_passes);
            passes = rendered_passes;
            const auto shape = engine_get_render_rgb_shape();
            if (engine_get_gt_rgb_shape() != shape)
                throw std::runtime_error("evaluate: render and ground-truth shapes differ");
            const int64_t B = std::get<0>(shape), H = std::get<1>(shape);
            const int64_t W = std::get<2>(shape), C = std::get<3>(shape);
            const int64_t n = B * H * W * C;
            render.resize((size_t)n);
            gt.resize((size_t)n);
            engine_copy_render_to_host(float_view(render, {B, H, W, C}),
                                       TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}},
                                       TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}});
            engine_copy_gt_rgb_to_host(float_view(gt, {B, H, W, C}));
            const int64_t view_n = H * W * C;
            for (int64_t v = 0; v < B; ++v) {
                float* pred = render.data() + (size_t)(v * view_n);
                const float* truth = gt.data() + (size_t)(v * view_n);
                for (int64_t p = 0; p < view_n; ++p)
                    pred[p] = std::clamp(pred[p], 0.0f, 1.0f);
                raw.add(truth, pred, (int)H, (int)W, (int)C);
                spirula::color_correct_into(pred, truth, H * W, (int)C, cc);
                corrected.add(truth, cc.data(), (int)H, (int)W, (int)C);
            }
        }
        std::printf("%s\n", format(cmsg::splat_evaluated_camera,
                                     {split, i + 1, limit}).c_str());
    }
    raw.print(cmsg::splat_ground_truth_metrics);
    corrected.print(cmsg::splat_ground_truth_metrics_corrected);
    return 0;
}

int compare_models(int argc, char** argv) {
    if (argc < 4) {
        usage(argv[0]);
        return 2;
    }
    std::string data;
    std::string primitive;
    int max_cameras = -1;
    for (int i = 4; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--data") {
            if (++i == argc) throw std::runtime_error("--data: missing value");
            data = argv[i];
        } else if (key == "--max-cameras") {
            if (++i == argc) throw std::runtime_error("--max-cameras: missing value");
            max_cameras = parse_int(argv[i]);
        } else if (key == "--primitive") {
            if (++i == argc) throw std::runtime_error("--primitive: missing value");
            primitive = parse_primitive(argv[i]);
        } else {
            throw std::runtime_error("unknown flag: " + key);
        }
    }

    auto [reference_ply, reference_run] = spirula::find_splat_ply(argv[2]);
    auto [candidate_ply, candidate_run] = spirula::find_splat_ply(argv[3]);
    (void)candidate_run;
    const spirula::SplatCloud reference = spirula::read_splat_ply(reference_ply);
    const spirula::SplatCloud candidate = spirula::read_splat_ply(candidate_ply);
    TrainConfig cfg = replay_config(reference_ply, reference_run, data, primitive, reference);

    spirula::TrainerSession session;
    session.cfg = cfg;
    session.create_output_dir = false;
    session.write_config_json = false;
    session.log_fn = [](const std::string& line) { std::printf("%s\n", line.c_str()); };
    session.check_config();
    session.load_dataset();
    session.cfg.relative_scale.reset();
    session.setup_engine();

    upload_scene(0, reference);
    upload_scene(1, candidate);
    const spirula::ColorResolution color = spirula::resolve_color(cfg);
    auto matrix = [](const spirula::Mat3f& m) {
        return std::vector<float>(m.begin(), m.end());
    };
    const std::vector<float> splat_matrix = color.splat_on()
        ? matrix(spirula::gamut_to_rec709(color.splat_gamut)) : std::vector<float>{};
    for (int slot : {0, 1})
        engine_scene_set_color_space(slot, color.splat_on(), (int)color.splat_transfer,
                                     color.splat_linear, splat_matrix);

    ImageMetrics metrics;
    const int total = (int)session.ds.train_indices.size();
    const int limit = max_cameras < 0 ? total : std::min(total, max_cameras);
    std::vector<float> reference_rgb, candidate_rgb;
    for (int i = 0; i < limit; ++i) {
        for (int pass = 0, passes = 1; pass < passes; ++pass) {
            engine_scene_activate(0);
            int reference_passes = 0;
            engine_preview_forward(session.ds.train_indices[(size_t)i], cfg.primitive,
                                   reference.sh_degree, cfg.packed, false, LossConfig{},
                                   pass, &reference_passes);
            passes = reference_passes;
            const auto shape = engine_get_render_rgb_shape();
            const int64_t B = std::get<0>(shape), H = std::get<1>(shape);
            const int64_t W = std::get<2>(shape), C = std::get<3>(shape);
            const int64_t n = B * H * W * C;
            reference_rgb.resize((size_t)n);
            engine_copy_render_to_host(float_view(reference_rgb, {B, H, W, C}),
                                       TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}},
                                       TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}});

            engine_scene_activate(1);
            int candidate_passes = 0;
            engine_preview_forward(session.ds.train_indices[(size_t)i], cfg.primitive,
                                   candidate.sh_degree, cfg.packed, false, LossConfig{},
                                   pass, &candidate_passes);
            if (candidate_passes != reference_passes)
                throw std::runtime_error("compare: models produced different camera passes");
            const auto candidate_shape = engine_get_render_rgb_shape();
            if (candidate_shape != shape)
                throw std::runtime_error("compare: models produced different render shapes");
            candidate_rgb.resize((size_t)n);
            engine_copy_render_to_host(float_view(candidate_rgb, {B, H, W, C}),
                                       TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}},
                                       TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}});

            const int64_t view_n = H * W * C;
            for (int64_t v = 0; v < B; ++v) {
                float* a = reference_rgb.data() + (size_t)(v * view_n);
                float* b = candidate_rgb.data() + (size_t)(v * view_n);
                for (int64_t p = 0; p < view_n; ++p) {
                    a[p] = std::clamp(a[p], 0.0f, 1.0f);
                    b[p] = std::clamp(b[p], 0.0f, 1.0f);
                }
                metrics.add(a, b, (int)H, (int)W, (int)C);
            }
        }
        std::printf("compared camera %d/%d\n", i + 1, limit);
    }
    metrics.print("comparison");
    engine_scene_free(0);
    engine_scene_free(1);
    return 0;
}

}  // namespace

int spirula_splat_main(int argc, char** argv) {
    EngineCleanup cleanup;
    try {
        if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            usage(argv[0]);
            return 0;
        }
        if (argc < 3) {
            usage(argv[0]);
            return 2;
        }
        const std::string command = argv[1];
        if (command == "evaluate") return evaluate_model(argc, argv);
        if (command == "compare") return compare_models(argc, argv);
        if (command != "validate" && command != "prune")
            throw std::runtime_error("expected subcommand: validate, prune, compare, or evaluate");

        std::string data;
        std::string output;
        std::string primitive;
        int max_cameras = -1;
        int compare_cameras = 0;
        bool visibility = false;
        double keep_fraction = 0.0;
        float min_opacity = -1.0f;
        int sh_degree = -1;
        for (int i = 3; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--data") {
                if (++i == argc) throw std::runtime_error("--data: missing value");
                data = argv[i];
            } else if (key == "--output") {
                if (++i == argc) throw std::runtime_error("--output: missing value");
                output = argv[i];
            } else if (key == "--keep-fraction") {
                if (++i == argc) throw std::runtime_error("--keep-fraction: missing value");
                keep_fraction = parse_fraction(argv[i]);
            } else if (key == "--min-opacity") {
                if (++i == argc) throw std::runtime_error("--min-opacity: missing value");
                min_opacity = parse_opacity(argv[i]);
            } else if (key == "--sh-degree") {
                if (++i == argc) throw std::runtime_error("--sh-degree: missing value");
                sh_degree = parse_sh_degree(argv[i]);
            } else if (key == "--max-cameras") {
                if (++i == argc) throw std::runtime_error("--max-cameras: missing value");
                max_cameras = parse_int(argv[i]);
            } else if (key == "--primitive") {
                if (++i == argc) throw std::runtime_error("--primitive: missing value");
                primitive = parse_primitive(argv[i]);
            } else if (key == "--compare-cameras") {
                if (++i == argc) throw std::runtime_error("--compare-cameras: missing value");
                compare_cameras = parse_int(argv[i]);
            } else if (key == "--visibility") {
                visibility = true;
            } else {
                throw std::runtime_error("unknown flag: " + key);
            }
        }
        if (command == "prune" && output.empty())
            throw std::runtime_error("prune: --output is required");
        if (command == "prune" && keep_fraction == 0.0 && min_opacity < 0.0f && sh_degree < 0)
            throw std::runtime_error("prune: specify --keep-fraction, --min-opacity, or --sh-degree");
        if (compare_cameras > 0 && sh_degree < 0)
            throw std::runtime_error("--compare-cameras requires --sh-degree");

        auto [ply, run] = spirula::find_splat_ply(argv[2]);
        if (command == "prune" && fs::exists(output))
            throw std::runtime_error("prune: output already exists: " + output);

        spirula::SplatCloud cloud = spirula::read_splat_ply(ply);
        if (sh_degree > cloud.sh_degree)
            throw std::runtime_error("--sh-degree exceeds the model's SH degree");
        const bool replay = command == "validate" || keep_fraction > 0.0 ||
                            visibility || compare_cameras > 0;
        const bool collect_contribution = visibility || keep_fraction > 0.0;
        std::vector<uint8_t> keep((size_t)cloud.num, 1);
        int limit = 0;
        if (replay) {
            TrainConfig cfg = replay_config(ply, run, data, primitive, cloud);

            spirula::TrainerSession session;
            session.cfg = cfg;
            session.create_output_dir = false;
            session.write_config_json = false;
            session.log_fn = [](const std::string& line) { std::printf("%s\n", line.c_str()); };
            session.check_config();
            session.load_dataset();
            session.cfg.relative_scale.reset();
            session.setup_engine();

            if (collect_contribution) engine_begin_splat_contribution();
            std::vector<float> unit_weight_map;
            std::vector<float> comparison_original, comparison_reduced;
            const int total = (int)session.ds.train_indices.size();
            limit = max_cameras < 0 ? total : std::min(total, max_cameras);
            for (int i = 0; i < limit; ++i) {
                int passes = 0;
                for (int pass = 0; pass == 0 || pass < passes; ++pass) {
                    engine_preview_forward(session.ds.train_indices[(size_t)i], cfg.primitive,
                                           cloud.sh_degree, cfg.packed, false, LossConfig{},
                                           pass, &passes);
                    if (collect_contribution) {
                        const auto shape = engine_get_render_rgb_shape();
                        const int64_t n = std::get<0>(shape) * std::get<1>(shape) *
                                          std::get<2>(shape);
                        unit_weight_map.assign((size_t)n, 1.0f);
                        engine_accumulate_splat_contribution(TorchTensorView{
                            (uint64_t)(uintptr_t)unit_weight_map.data(), 4,
                            {std::get<0>(shape), std::get<1>(shape), std::get<2>(shape), 1}});
                    }
                    if (compare_cameras > 0 && i < compare_cameras) {
                        const auto shape = engine_get_render_rgb_shape();
                        const int64_t n = std::get<0>(shape) * std::get<1>(shape) *
                                          std::get<2>(shape) * std::get<3>(shape);
                        comparison_original.resize((size_t)n);
                        comparison_reduced.resize((size_t)n);
                        const TorchTensorView tv_original{
                            (uint64_t)(uintptr_t)comparison_original.data(), 4,
                            {std::get<0>(shape), std::get<1>(shape), std::get<2>(shape), 3}};
                        const TorchTensorView tv_reduced{
                            (uint64_t)(uintptr_t)comparison_reduced.data(), 4,
                            {std::get<0>(shape), std::get<1>(shape), std::get<2>(shape), 3}};
                        engine_debug_forward(TorchTensorView{0, 0, {}}, -1, tv_original);
                        engine_debug_forward(TorchTensorView{0, 0, {}}, sh_degree, tv_reduced);
                        std::printf("SH comparison: camera %d pass %d PSNR=%.6g dB\n",
                                    i + 1, pass + 1,
                                    spirula::image_psnr(comparison_original.data(),
                                                        comparison_reduced.data(), n));
                    }
                }
                std::printf("validated camera %d/%d\n", i + 1, limit);
            }
        }
        if (collect_contribution) {
            std::vector<float> contribution((size_t)cloud.num);
            engine_copy_splat_contribution_to_host(TorchTensorView{
                (uint64_t)(uintptr_t)contribution.data(), 4, {(int64_t)contribution.size(), 1}});
            const auto [mn, mx] = std::minmax_element(contribution.begin(), contribution.end());
            const double sum = std::accumulate(contribution.begin(), contribution.end(), 0.0);
            const auto nonzero = std::count_if(contribution.begin(), contribution.end(),
                                               [](float v) { return v > 0.0f; });
            std::printf("visibility: sum=%.6f, nonzero=%zu/%zu, min=%.6g, max=%.6g\n",
                        sum, nonzero, contribution.size(), *mn, *mx);
            if (keep_fraction > 0.0)
                keep = spirula::select_splats_by_contribution(contribution, keep_fraction);
        }
        if (min_opacity >= 0.0f) {
            const std::vector<uint8_t> opaque =
                spirula::select_splats_by_opacity(cloud.opacities, min_opacity);
            for (size_t i = 0; i < keep.size(); ++i) keep[i] &= opaque[i];
        }
        if (sh_degree >= 0) spirula::reduce_splat_sh_degree(cloud, sh_degree);
        if (command == "prune") {
            const int64_t kept = std::count(keep.begin(), keep.end(), (uint8_t)1);
            spirula::write_splat_ply(cloud, output, keep.data());
            std::printf("wrote %s: kept %lld/%lld splats\n",
                        output.c_str(), (long long)kept, (long long)cloud.num);
        }
        if (command == "validate")
            std::printf("validated %d training cameras; no model was written\n", limit);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "splat: %s\n", e.what());
        return 1;
    }
    return 0;
}
