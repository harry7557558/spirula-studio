// CPU-only allocation-layout and refusal checks; profile reads metadata only.
#include "app/TrainerCore.h"
#include "config/TrainConfigJson.h"
#include "core/CheckpointIO.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace spirula;
namespace fs = std::filesystem;

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void equal(uint64_t actual, uint64_t expected, const char* message) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s: got %llu, expected %llu\n", message,
                 (unsigned long long)actual, (unsigned long long)expected);
    throw std::runtime_error(message);
}
struct TempDir {
    fs::path path;
    TempDir() {
        for (uint64_t attempt = 0;; ++attempt) {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path = fs::temp_directory_path() / ("spirula_preflight_" +
                std::to_string(stamp) + "_" + std::to_string(attempt));
            if (fs::create_directory(path)) return;
        }
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};
std::string png(const fs::path& dir, const char* name, int w, int h) {
    const auto path = dir / name;
    std::vector<uint8_t> pixels((size_t)w * h * 3, 128);
    check(stbi_write_png(path.string().c_str(), w, h, 3, pixels.data(), w * 3) != 0,
          "cannot write fixture PNG");
    return path.string();
}
ParsedDataset dataset(std::initializer_list<std::array<int32_t, 2>> sizes,
                      std::vector<std::string> normals = {}) {
    ParsedDataset ds;
    for (auto size : sizes) {
        ds.train_indices.push_back((int32_t)ds.num_cameras++);
        ds.widths.push_back(size[0]);
        ds.heights.push_back(size[1]);
        ds.camera_models.push_back(0);
        ds.camera_distortions.push_back(0);
        ds.image_filenames.emplace_back();
    }
    ds.normal_filenames = std::move(normals);
    return ds;
}
PostSplitCameras faces(int count, int side) {
    PostSplitCameras post;
    post.n_post = count;
    post.any_warp = true;
    post.K_per_camera = {count};
    post.post_offsets = {0};
    post.post_widths.assign(count, side);
    post.post_heights.assign(count, side);
    return post;
}
TrainConfig config() {
    TrainConfig cfg;
    cfg.eval_mode = "all";
    cfg.num_loss_scales = 0;
    cfg.loss_scale_min_pixels = 0;
    cfg.quantization_level = 0;
    cfg.sh_degree = 0;
    cfg.split_batch = false;
    cfg.use_fused_proj_bwd_optim = false;
    cfg.use_bilateral_grid = false;
    cfg.use_bilateral_grid_for_geometry = false;
    cfg.use_ppisp = false;
    cfg.background_mode = "black";
    cfg.densify_loss_map_mode = "none";
    return cfg;
}
TrainingMemoryEstimate size(const ParsedDataset& ds, const TrainConfig& cfg,
                            int64_t capacity = 1000) {
    PostSplitCameras post;
    post.n_post = ds.num_cameras;
    return estimate_training_memory(ds, post, cfg, false, false,
                                    !ds.normal_filenames.empty(), capacity, 0);
}
void resolvers(const TempDir& tmp) {
    for (auto row : {std::array<int, 2>{1919, 1}, {1920, 2}, {3839, 2},
                     {3840, 3}, {7679, 3}, {7680, 4}})
        equal(resolve_loss_scales(1, 1920, row[0], 10000), row[1], "adaptive threshold");
    equal(resolve_loss_scales(3, 0, 20, 20), 3, "fixed scales changed");
    equal(resolve_loss_scales(0, 0, 4482, 4482), 0, "invalid fixed count must reach validation");
    check(resolve_fused_proj_bwd_optim(true, true, 1, 1), "single input can stay fused");
    check(!resolve_fused_proj_bwd_optim(true, true, 2, 1), "split input batch cannot be fused");
    check(!resolve_fused_proj_bwd_optim(true, false, 1, 2), "multiple face passes cannot be fused");

    TrainConfig cfg = config();
    cfg.cap_max = 1000;
    cfg.min_init_fraction = 0;
    cfg.preallocate_splat_tensors = true;
    equal(resolve_training_splat_capacity(10, cfg), 1000, "preallocation must use cap");
    cfg.preallocate_splat_tensors = false;
    equal(resolve_training_splat_capacity(10, cfg), 10, "fresh live capacity");
    cfg.min_init_fraction = 0.5f;
    equal(resolve_training_splat_capacity(10, cfg), 500, "minimum seed fraction");
    {
        std::ofstream out(tmp.path / "state.tar", std::ios::binary);
        const std::string state = "{\"cur_num_splats\":900,\"max_num_splats\":1024}";
        ckpt::tar_write_bytes(out, "state.json", state.data(), state.size());
        ckpt::tar_finish(out);
    }
    cfg.resume = tmp.path.string();
    equal(resolve_training_splat_capacity(10, cfg), 900, "resume must retain live rows above seed count");
    cfg.cap_max = 700;
    equal(resolve_training_splat_capacity(10, cfg), 700, "resume respects target cap");
}
void refusal_and_workers() {
    const auto ds = dataset({{512, 512}});
    auto cfg = config();
    const auto estimate = size(ds, cfg);
    check(estimate.dynamic_unknown, "projection and densification demand is not a bound");
    const uint64_t request = estimate.estimated_bytes();
    backend::BudgetSnapshot budget;
    budget.status = backend::BudgetStatus::Available;
    budget.available_bytes = request + kTrainingMemoryReserveBytes;
    check(!training_memory_refusal(budget, estimate, 0), "exact driver headroom refused");
    --budget.available_bytes;
    auto failure = training_memory_refusal(budget, estimate, 0);
    check(failure && failure->kind == backend::BudgetFailureKind::DriverHeadroom,
          "one-byte driver shortage was not typed");
    equal(failure->requested_bytes, request, "allowance missing from request");
    budget.available_bytes = UINT64_MAX;
    budget.process_bytes = 1024;
    budget.reserved_bytes = 512;
    check(!training_memory_refusal(budget, estimate, request + 1536), "exact app headroom refused");
    failure = training_memory_refusal(budget, estimate, request + 1535);
    check(failure && failure->kind == backend::BudgetFailureKind::ApplicationLimit,
          "reserved memory missing from app refusal");
    budget.status = backend::BudgetStatus::Unavailable;
    failure = training_memory_refusal(budget, estimate, 0);
    check(failure && failure->kind == backend::BudgetFailureKind::TelemetryUnavailable,
          "unavailable telemetry kind lost");
    budget.status = backend::BudgetStatus::QueryError;
    failure = training_memory_refusal(budget, estimate, 0);
    check(failure && failure->kind == backend::BudgetFailureKind::TelemetryError,
          "telemetry error kind lost");
    equal(size(ds, cfg, INT64_MAX).estimated_bytes(), UINT64_MAX, "overflow must saturate");
    cfg.quantization_level = 1;
    cfg.sh_degree = 3;
    cfg.use_fused_proj_bwd_optim = true;
    equal(size(ds, cfg, INT64_MAX).estimated_bytes(), UINT64_MAX, "FPBO padding must not wrap");

    constexpr uint64_t pixels = 2540ull * 2540;
    const int workers = resolve_eval_worker_count(pixels, 16, false);
    check(workers > 1 && pixels * (163ull * workers + 24) <= (4ull << 30),
          "full eval queue plus producer exceeds soft concurrency budget");
    const int64_t png_pixels = 3400ll * 3369;
    check(resolve_eval_worker_count(png_pixels, 16, true) <
          resolve_eval_worker_count(png_pixels, 16, false), "PNG allowance must reduce concurrency");
    equal(resolve_eval_worker_count(INT64_MAX, 16, true), 1, "huge eval extent wrapped");
}
void model_layouts() {
    const auto ds = dataset({{512, 512}});
    auto delta = [&](TrainConfig cfg, int64_t n) {
        cfg.sh_degree = 3;
        const uint64_t high = size(ds, cfg, n).accounted_bytes;
        cfg.sh_degree = 1;
        return high - size(ds, cfg, n).accounted_bytes;
    };
    auto cfg = config();
    constexpr uint64_t n = 256;
    equal(delta(cfg, n), 4 * n * 12 * sizeof(float3), "fp32 SH requires value, gradient and two moments");
    cfg.quantization_level = 1;
    const uint64_t high_cells = n * 15 * 3, low_cells = n * 3 * 3;
    equal(delta(cfg, n), (high_cells - low_cells) * 5 +
          (high_cells / 256 - low_cells / 256) * 24, "cell-block SH packing and bounds");
    const auto tiny = dataset({{1, 1}});
    constexpr uint64_t startup_n = 65536;
    cfg.sh_degree = 3;
    const uint64_t startup_high = size(tiny, cfg, startup_n).conservative_allowance_bytes;
    cfg.sh_degree = 1;
    const uint64_t startup_low = size(tiny, cfg, startup_n).conservative_allowance_bytes;
    equal(startup_high - startup_low, 36 * startup_n * sizeof(float),
          "quantized setup must allow overlapping fp32 SH storage");
    cfg.use_fused_proj_bwd_optim = true;
    const uint64_t block_delta = 4ull * (sh_fpbo_cells(256, 15) - sh_fpbo_cells(256, 3));
    equal(delta(cfg, 255), block_delta, "FPBO partial block allocation");
    equal(delta(cfg, 256), block_delta, "FPBO full block allocation");
    equal(delta(cfg, 257), block_delta * 2, "FPBO next block allocation");
    cfg.split_batch = true;
    cfg.max_batch_per_epoch = 1;
    const auto four = dataset({{640, 480}, {640, 480}, {640, 480}, {640, 480}});
    const auto requested = size(four, cfg);
    cfg.use_fused_proj_bwd_optim = false;
    check(!requested.fused_proj_bwd_optim, "requested fused layout was not resolved");
    equal(requested.accounted_bytes, size(four, cfg).accounted_bytes, "resolved layout bytes disagree");

    cfg = config();
    const uint64_t densify_base = size(ds, cfg, n).accounted_bytes;
    cfg.densify_oversize_split_fraction = 0;
    equal(densify_base - size(ds, cfg, n).accounted_bytes, 12 * n,
          "oversize densification workspace missing");
    cfg = config();
    const uint64_t unclipped = size(ds, cfg, n).accounted_bytes;
    cfg.densify_score_clip_quantile = 0.5f;
    equal(size(ds, cfg, n).accounted_bytes - unclipped, 4 * n + 4100,
          "clipped-score densification workspace missing");
}
void image_layouts(const TempDir& tmp) {
    auto cfg = config();
    const std::string small = png(tmp.path, "small.png", 32, 32);
    const std::string large = png(tmp.path, "large.png", 900, 600);
    const auto wide = dataset({{2000, 1000}}, {small});
    const auto narrow = dataset({{64, 64}}, {large});
    const auto mixed = dataset({{2000, 1000}, {64, 64}}, {small, large});
    const uint64_t a = size(wide, cfg).accounted_bytes, b = size(narrow, cfg).accounted_bytes;
    const uint64_t both = size(mixed, cfg).accounted_bytes;
    check(both > std::max(a, b) && both < a + b, "per-allocation maxima lost a modality peak");

    auto hetero = dataset({{1, 1}, {1, 1}, {1, 1}});
    hetero.camera_models = {0, 1, 1};
    auto bilagrid_bytes = [&](TrainConfig c) {
        c.use_bilateral_grid = true;
        const uint64_t enabled = size(hetero, c).accounted_bytes;
        c.use_bilateral_grid = false;
        return enabled - size(hetero, c).accounted_bytes;
    };
    cfg = config();
    cfg.max_batch_per_epoch = 1;
    const uint64_t packed = bilagrid_bytes(cfg);
    cfg.max_batch_per_epoch = 800;
    const uint64_t unpacked = bilagrid_bytes(cfg);
    constexpr uint64_t rgb_grid_cells = 9 * 16 * 16 * 8;
    equal(packed - unpacked, 24 + 2 * rgb_grid_cells * sizeof(float),
          "heterogeneous step camera total missing from bilagrid gradient");
    cfg = config();

    auto missing_mask = dataset({{64, 64}, {64, 64}});
    missing_mask.mask_filenames = {png(tmp.path, "scalar-mask.png", 1, 1), ""};
    auto full_mask = missing_mask;
    full_mask.mask_filenames[1] = png(tmp.path, "full-mask.png", 64, 64);
    PostSplitCameras mask_post;
    mask_post.n_post = 2;
    auto masked_size = [&](const ParsedDataset& ds) {
        return estimate_training_memory(ds, mask_post, cfg, true, false, false,
                                        1000, 0).accounted_bytes;
    };
    equal(masked_size(missing_mask), masked_size(full_mask),
          "synthesized mask must use RGB extent");

    const auto native = dataset({{4482, 4482}}, {png(tmp.path, "normal.png", 307, 307)});
    cfg.loss_scale_min_pixels = 1920;
    const uint64_t adaptive = size(native, cfg).accounted_bytes;
    cfg.loss_scale_min_pixels = 0;
    cfg.num_loss_scales = 2;
    equal(size(native, cfg).accounted_bytes, adaptive, "normal GT must not drive adaptive scales");
    auto pyramid_delta = [&](int side) {
        const auto ds = dataset({{side, side}});
        auto c = config();
        const uint64_t base = size(ds, c).accounted_bytes;
        c.loss_scale_min_pixels = 1920;
        return size(ds, c).accounted_bytes - base;
    };
    equal(pyramid_delta(960), 0, "single-scale pyramid grew");
    const uint64_t one = pyramid_delta(1920);
    check(one > 0, "second scale missing");
    equal(pyramid_delta(1921), one, "pyramid must floor each axis");
    equal(pyramid_delta(3840), 5 * one, "three-scale area sum");
    equal(pyramid_delta(7680), 21 * one, "four-scale area sum");

    const auto pano = dataset({{1024, 512}});
    cfg = config();
    auto split = [&](int count) {
        return estimate_training_memory(pano, faces(count, 256), cfg,
                                        false, false, false, 1000, 1).accounted_bytes;
    };
    equal(split(6), split(2), "extra passes must reuse one face's retained buffers");
    check(estimate_training_memory(pano, faces(6, 256), cfg,
          false, false, false, 1000, 0).accounted_bytes > split(6), "unsplit faces missing from working set");

    const auto mapped = dataset({{15520, 7760}}, {png(tmp.path, "panonormal.png", 1064, 532)});
    const auto mapped_large = dataset({{15520, 7760}}, {png(tmp.path, "panonormal2.png", 2128, 1064)});
    const auto post = faces(6, 4482);
    auto mapped_bytes = [&](const ParsedDataset& ds) {
        return estimate_training_memory(ds, post, cfg, false, false, true, 1000, 1).accounted_bytes;
    };
    equal(mapped_bytes(mapped_large) - mapped_bytes(mapped), 12ull * (615 * 615 - 307 * 307),
          "normal warp must use original RGB dimensions and nearest per-axis rounding");
}
int profile(const char* path) {
    const auto root = json_parse_file(path);
    check(train_config_json_has_fields(root), "not a training configuration");
    TrainerSession session;
    train_config_from_json(root, session.cfg);
    session.load_dataset();
    const auto& cfg = session.cfg;
    const int64_t capacity = resolve_training_splat_capacity(session.ds.points.num(), cfg);
    const auto estimate = estimate_training_memory(session.ds, session.post, cfg,
        session.has_mask, session.has_depth, session.has_normal, capacity, cfg.split_batch ? 1 : 0);
    auto peak = [](const auto& widths, const auto& heights) {
        std::array<int64_t, 2> result{};
        for (size_t i = 0; i < widths.size(); ++i)
            if ((int64_t)widths[i] * heights[i] > result[0] * result[1]) result = {widths[i], heights[i]};
        return result;
    };
    const auto input = peak(session.ds.widths, session.ds.heights);
    const auto& widths = session.post.post_widths.empty() ? session.ds.widths : session.post.post_widths;
    const auto& heights = session.post.post_heights.empty() ? session.ds.heights : session.post.post_heights;
    const auto face = peak(widths, heights);
    int low = 4, high = 0;
    for (size_t i = 0; i < widths.size(); ++i) {
        const int scales = resolve_loss_scales(cfg.num_loss_scales + 1, cfg.loss_scale_min_pixels, widths[i], heights[i]);
        low = std::min(low, scales); high = std::max(high, scales);
    }
    std::printf("input=%lldx%lld face=%lldx%lld cameras=%lld post=%lld scales=%d..%d\n",
        (long long)input[0], (long long)input[1], (long long)face[0], (long long)face[1],
        (long long)session.ds.num_cameras, (long long)session.post.n_post, low, high);
    std::printf("source=%lld capacity=%lld quantization=%d fused=%d eval=%s interval=%d\n",
        (long long)session.ds.points.num(), (long long)capacity, cfg.quantization_level,
        (int)estimate.fused_proj_bwd_optim, cfg.eval_mode.c_str(), cfg.eval_interval);
    std::printf("accounted_bytes=%llu allowance_bytes=%llu estimated_bytes=%llu dynamic_unknown=%d\n",
        (unsigned long long)estimate.accounted_bytes, (unsigned long long)estimate.conservative_allowance_bytes,
        (unsigned long long)estimate.estimated_bytes(), (int)estimate.dynamic_unknown);
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "profile") return profile(argv[2]);
        check(argc == 1, "usage: training_memory_preflight [profile <config.json>]");
        TempDir tmp;
        resolvers(tmp);
        refusal_and_workers();
        model_layouts();
        image_layouts(tmp);
        std::puts("training_memory_preflight: ALL PASSED");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "training_memory_preflight: %s\n", error.what());
        return 1;
    }
}
