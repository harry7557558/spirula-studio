// progressive_resolution_test -- the resolution schedule (data/ResolutionSchedule.h)
// and the batches DataManager builds from it in both cache modes: sizes,
// intrinsics and area-filtered pixels per epoch. Host only.

#include "data/DataManager.h"
#include "data/ResolutionSchedule.h"

#include "external/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

bool throws(const std::function<void()>& f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

void schedule_math() {
    using progressive::Setpoints;
    check(progressive::automatic_setpoints(4, 0.3, 30000) == Setpoints{{0, 4}, {3000, 2}, {9000, 1}},
          "start 4: 1/4 for a third of full_at, then 1/2, then full");
    check(progressive::automatic_setpoints(8, 0.7, 7000) == Setpoints{{0, 8}, {700, 4}, {2100, 2}, {4900, 1}},
          "start 8: each stage twice the last");
    check(throws([] { progressive::automatic_setpoints(3, 0.3, 100); }), "a start that is not a power of two");
    check(throws([] { progressive::automatic_setpoints(4, 0.0, 100); }), "full_at outside (0, 1]");

    check(progressive::parse_setpoints("0:4, 3000:2 9000:1") == Setpoints{{0, 4}, {3000, 2}, {9000, 1}},
          "commas and spaces both separate");
    check(progressive::parse_setpoints("3000:2") == Setpoints{{0, 2}, {3000, 2}},
          "a schedule starting late begins at its first divisor");
    for (const char* bad : {"0:2, 100:4", "x", "5:0", "0:2,0:1", "-1:2", "10:2.5", "10:", ""})
        check(throws([&] { progressive::parse_setpoints(bad); }), std::string("rejects '") + bad + "'");

    check(progressive::to_epochs({{0, 4}, {3000, 2}, {9000, 1}}, 1000) == Setpoints{{0, 4}, {3, 2}, {9, 1}},
          "steps on epoch boundaries");
    check(progressive::to_epochs({{0, 4}, {2600, 2}}, 1000) == Setpoints{{0, 4}, {3, 2}},
          "a switch moves to the nearest boundary");
    check(progressive::to_epochs({{0, 4}, {3000, 2}, {9000, 1}}, 10000) == Setpoints{{0, 2}, {1, 1}},
          "stages that share a boundary keep the finer one");
    check(progressive::divisor_at({{0, 4}, {3, 2}, {9, 1}}, 5) == 2 && progressive::divisor_at({}, 5) == 1,
          "divisor per epoch");
    check(progressive::scaled_extent(7, 4) == 1 && progressive::scaled_extent(3, 4) == 1 &&
          progressive::scaled_extent(9, 2) == 4 && progressive::scaled_extent(8, 1) == 8,
          "floored, never below one pixel");
    using progressive::Budget;
    const Setpoints three{{0, 4}, {3000, 2}, {9000, 1}};
    check(progressive::automatic_budget(three, 1000000, 8000000) ==
              Budget{{0, 2000000}, {3000, 4000000}, {9000, 8000000}},
          "automatic budget: the same factor each stage, from the seed to the cap");
    check(progressive::automatic_budget(three, 9000000, 8000000) == Budget{{0, 8000000}, {3000, 8000000}, {9000, 8000000}} &&
              progressive::automatic_budget(three, 0, 8000000).back().second == 8000000,
          "automatic budget: a seed at or over the cap, or none, holds nothing back");
    check(progressive::stage_budget(three, "500000, 0.5, 100%", 1000000, 8000000) ==
              Budget{{0, 500000}, {3000, 4000000}, {9000, 8000000}},
          "per-stage budget: a count, a fraction of the cap and a percent");
    check(progressive::stage_budget(three, " , 3000000", 1000000, 8000000) ==
              Budget{{0, 2000000}, {3000, 3000000}, {9000, 8000000}},
          "per-stage budget: blank and missing stages take the automatic value");
    check(throws([&] { progressive::stage_budget(three, "4000000, 1000000", 1000000, 8000000); }),
          "per-stage budget that shrinks");
    check(throws([&] { progressive::stage_budget(three, "1, 2, 3, 4", 1000000, 8000000); }),
          "more values than stages");
    check(throws([&] { progressive::stage_budget(three, "lots", 1000000, 8000000); }), "a value that is not a number");
    check(progressive::resolve_budget(three, "", 1000000, 8000000) == progressive::automatic_budget(three, 1000000, 8000000) &&
              progressive::resolve_budget(three, "0:10%", 1000000, 8000000) == Budget{{0, 800000}},
          "resolve: empty is automatic, step:splats is the explicit schedule");
    check(progressive::parse_budget("3000:50%, 9000:1000000 6000:600000", 1000000) ==
              Budget{{0, 500000}, {3000, 500000}, {6000, 600000}, {9000, 1000000}},
          "manual budget: percents, counts, sorted, anchored at step 0");
    check(progressive::parse_budget("0:5000000", 1000000) == Budget{{0, 1000000}}, "manual budget clamps to the cap");
    check(throws([] { progressive::parse_budget("0:50%, 100:20%", 1000); }), "a budget that shrinks");
    check(throws([] { progressive::parse_budget("0:150%", 1000); }), "a percent over 100");
    check(throws([] { progressive::parse_budget("0:abc", 1000); }), "a budget that is not a number");
    check(throws([] { progressive::parse_budget("5:10, 5:20", 1000); }), "a step named twice");
    check(throws([] { progressive::parse_budget(" ", 1000); }), "an empty budget");
}

constexpr int W = 8, H = 8;

std::string write_image(const fs::path& dir) {
    std::vector<uint8_t> px((size_t)W * H * 3);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            uint8_t* p = &px[((size_t)y * W + x) * 3];
            p[0] = (uint8_t)(x * 20); p[1] = (uint8_t)(y * 20); p[2] = 7;
        }
    const auto path = (dir / "gradient.png").string();
    stbi_write_png(path.c_str(), W, H, 3, px.data(), W * 3);
    return path;
}

void data_manager(const std::string& image, CacheMode mode, const char* name) {
    DataManagerConfig cfg;
    cfg.cache_mode = mode;
    cfg.load_masks = cfg.load_depths = cfg.load_normals = false;
    cfg.resolution_stages = {{0, 2}, {1, 1}};
    std::vector<float> viewmat(16, 0.0f);
    viewmat[0] = viewmat[5] = viewmat[10] = viewmat[15] = 1.0f;
    auto make = [&](int64_t first_step) {
        cfg.first_step = first_step;
        return std::make_unique<DataManager>(cfg, std::vector<int32_t>{0}, std::vector<int32_t>{0},
            std::vector<std::string>{image}, std::vector<std::string>{}, std::vector<std::string>{},
            std::vector<std::string>{}, std::vector<int32_t>{W}, std::vector<int32_t>{H},
            std::vector<int32_t>{}, std::vector<int32_t>{}, viewmat, std::vector<float>{10.0f, 12.0f, 4.0f, 3.0f},
            std::vector<float>(8, 0.0f), std::vector<int32_t>{}, std::vector<int32_t>{}, std::vector<float>{},
            std::vector<float>{}, std::vector<float>{}, std::vector<int32_t>{}, std::vector<float>{},
            std::vector<int32_t>{0}, std::vector<int32_t>{});
    };
    const std::string tag = std::string(name) + ": ";
    if (mode == CacheMode::DISK) {
        // Decode workers publish steps as they finish, so steps arrive out of order
        // by up to the pipeline's depth (ready, queued and decoding: ~30 here).
        auto dm = make(0);
        int halves = 0, fulls = 0;
        bool shapes = true;
        for (int k = 0; k < 64; ++k) {
            const DecodedBatch& b = *dm->next_train_step().subs[0];
            if (b.resolution_divisor == 2) {
                ++halves;
                shapes &= b.width == 4 && b.intrins == std::vector<float>{5.0f, 6.0f, 2.0f, 1.5f} &&
                          b.rgb_buffer.size() == 4 * 4 * 3 && std::abs(b.rgb_buffer[3] - 50) <= 1;
            } else {
                ++fulls;
                shapes &= b.width == W && b.intrins[0] == 10.0f && b.rgb_buffer[3] == 20;
            }
        }
        check(halves == 1 && fulls == 63, tag + "one pass at half size, the rest at the loaded size");
        check(shapes, tag + "each batch's size, intrinsics and pixels match its divisor");
        auto resumed = make(1);
        bool all_full = true;
        for (int k = 0; k < 4; ++k) all_full &= resumed->next_train_step().subs[0]->width == W;
        check(all_full, tag + "a resume past the stage starts at full size");
        return;
    }
    {
        auto dm = make(0);
        const DecodedBatch& half = *dm->next_train_step().subs[0];
        bool pixels = half.rgb_buffer.size() == 4 * 4 * 3;
        for (int y = 0; y < 4 && pixels; ++y)
            for (int x = 0; x < 4; ++x) {
                const uint8_t* p = &half.rgb_buffer[((size_t)y * 4 + x) * 3];
                pixels &= std::abs(p[0] - (40 * x + 10)) <= 1 && std::abs(p[1] - (40 * y + 10)) <= 1 && p[2] == 7;
            }
        check(half.width == 4 && half.height == 4 && half.input_width == 4 && half.resolution_divisor == 2,
              tag + "epoch 0 trains at half size");
        check(half.intrins == std::vector<float>{5.0f, 6.0f, 2.0f, 1.5f}, tag + "intrinsics follow the image");
        check(pixels, tag + "pixels are 2x2 area averages");
        check(dm->last_train_divisor() == 2, tag + "the divisor handed out is reported");
        const DecodedBatch& full = *dm->next_train_step().subs[0];
        check(full.width == W && full.resolution_divisor == 1 && full.intrins[0] == 10.0f &&
              full.rgb_buffer.size() == (size_t)W * H * 3 && full.rgb_buffer[3] == 20,
              tag + "epoch 1 trains at the loaded size");
        check(dm->next_train_step().subs[0]->width == W, tag + "and stays there");
    }
    {
        auto dm = make(1);
        check(dm->next_train_step().subs[0]->width == W, tag + "a resume past the stage starts at full size");
    }
}

// Three images, so one pass is three steps: a switch lands on its step whether
// the run resumed mid-pass or the stage starts inside one.
void stage_steps(const std::string& image, CacheMode mode, const char* name) {
    DataManagerConfig cfg;
    cfg.cache_mode = mode;
    cfg.load_masks = cfg.load_depths = cfg.load_normals = false;
    std::vector<float> viewmats(48, 0.0f);
    for (int k = 0; k < 3; ++k)
        for (int d = 0; d < 4; ++d) viewmats[k * 16 + d * 5] = 1.0f;
    auto widths = [&](int64_t first_step, std::vector<std::pair<int64_t, int>> stages, int steps) {
        cfg.first_step = first_step;
        cfg.resolution_stages = std::move(stages);
        DataManager dm(cfg, std::vector<int32_t>(3, 0), std::vector<int32_t>(3, 0),
            std::vector<std::string>(3, image), std::vector<std::string>{}, std::vector<std::string>{},
            std::vector<std::string>{}, std::vector<int32_t>(3, W), std::vector<int32_t>(3, H),
            std::vector<int32_t>{}, std::vector<int32_t>{}, viewmats,
            std::vector<float>{10.0f, 12.0f, 4.0f, 3.0f, 10.0f, 12.0f, 4.0f, 3.0f, 10.0f, 12.0f, 4.0f, 3.0f},
            std::vector<float>(24, 0.0f), std::vector<int32_t>{}, std::vector<int32_t>{}, std::vector<float>{},
            std::vector<float>{}, std::vector<float>{}, std::vector<int32_t>{}, std::vector<float>{},
            std::vector<int32_t>{0, 1, 2}, std::vector<int32_t>{});
        std::vector<int> out;
        for (int k = 0; k < steps; ++k) out.push_back(dm.next_train_step().subs[0]->width);
        return out;
    };
    const std::string tag = std::string(name) + ": ";
    check(widths(1, {{0, 2}, {3, 1}}, 4) == std::vector<int>{4, 4, W, W},
          tag + "resumed at step 1, the switch still lands on step 3");
    check(widths(0, {{0, 2}, {2, 1}}, 5) == std::vector<int>{4, 4, W, W, W},
          tag + "a switch inside a pass ends that pass early");
}

// A fisheye split into two 6x6 faces: faces render at 1/divisor, the input it
// is warped from is resampled, so each takes its own scale.
void split_camera(const std::string& image) {
    DataManagerConfig cfg;
    cfg.cache_mode = CacheMode::CPU;
    cfg.load_masks = cfg.load_depths = cfg.load_normals = false;
    cfg.resolution_stages = {{0, 2}};
    std::vector<float> viewmats(32, 0.0f), axes(18, 0.0f);
    for (int k = 0; k < 2; ++k) {
        for (int d = 0; d < 4; ++d) viewmats[k * 16 + d * 5] = 1.0f;
        for (int d = 0; d < 3; ++d) axes[k * 9 + d * 4] = 1.0f;
    }
    DataManager dm(cfg, std::vector<int32_t>{(int32_t)CameraModelType::FISHEYE}, std::vector<int32_t>{0},
                   std::vector<std::string>{image}, std::vector<std::string>{}, std::vector<std::string>{},
                   std::vector<std::string>{}, std::vector<int32_t>{W}, std::vector<int32_t>{H},
                   std::vector<int32_t>{2}, std::vector<int32_t>{0}, viewmats,
                   std::vector<float>{3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f}, std::vector<float>(16, 0.0f),
                   std::vector<int32_t>{6, 6}, std::vector<int32_t>{6, 6}, axes,
                   std::vector<float>{2.5f, 2.5f, 4.0f, 4.0f}, std::vector<float>(8, 0.0f),
                   std::vector<int32_t>{}, std::vector<float>{}, std::vector<int32_t>{0}, std::vector<int32_t>{});
    const DecodedBatch& b = *dm.next_train_step().subs[0];
    bool passes = !b.face_passes.empty();
    for (const auto& p : b.face_passes) passes &= p.width == 3 && p.height == 3;
    check(b.K == 2 && b.width == 3 && b.height == 3 && passes, "split faces render at half size");
    check(b.intrins == std::vector<float>(8, 1.5f), "face intrinsics halve with them");
    check(b.input_width == 4 && b.input_intrins == std::vector<float>{1.25f, 1.25f, 2.0f, 2.0f} &&
          b.rgb_buffer.size() == 4 * 4 * 3, "the input image and its camera are resampled together");
    check(b.face_axes == axes, "face frames are untouched");
}

}  // namespace

int main() {
    schedule_math();
    const fs::path dir = fs::temp_directory_path() / "spirula-progressive-resolution-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string image = write_image(dir);
    data_manager(image, CacheMode::CPU, "cpu cache");
    data_manager(image, CacheMode::DISK, "disk stream");
    stage_steps(image, CacheMode::CPU, "cpu cache");
    split_camera(image);
    fs::remove_all(dir);
    std::printf(g_failures ? "FAIL %d\n" : "PASS progressive resolution schedule and batches\n", g_failures);
    return g_failures ? 1 : 0;
}
