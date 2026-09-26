// dlogm_session -- `--image-color-log` through the trainer's real setup and
// one real step: TrainerSession arms the engine's GT decode, the step's GT
// comes back decoded, and the brightness match reads decoded light. Each of
// those is one line of wiring whose loss trains on flat log frames silently.

#include "app/TrainerCore.h"
#include "engine/EngineState.h"
#include "external/stb_image_write.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <random>

namespace fs = std::filesystem;
using namespace spirula;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

constexpr int kW = 32, kH = 24, kImages = 3;
// 102/255 is D-Log M code 0.4 exactly: mid grey, 0.18 linear, 0.4614 displayed.
constexpr uint8_t kCode = 102;
constexpr float kDisplay = 0.461356f;

// Three views of a small cloud, every pixel the same code, in COLMAP text.
fs::path write_dataset(const fs::path& root) {
    fs::create_directories(root / "images");
    fs::create_directories(root / "sparse" / "0");
    std::vector<uint8_t> px((size_t)kW * kH * 3, kCode);
    std::ofstream im(root / "sparse" / "0" / "images.txt");
    for (int i = 0; i < kImages; i++) {
        const std::string name = "v" + std::to_string(i) + ".png";
        stbi_write_png((root / "images" / name).string().c_str(), kW, kH, 3,
                       px.data(), kW * 3);
        im << i + 1 << " 1 0 0 0 " << 0.2 * i << " 0 0 1 " << name << "\n\n";
    }
    std::ofstream(root / "sparse" / "0" / "cameras.txt")
        << "1 PINHOLE " << kW << " " << kH << " 30 30 16 12\n";
    std::ofstream pts(root / "sparse" / "0" / "points3D.txt");
    for (int k = 0; k < 64; k++)
        pts << k + 1 << " " << 0.1 * (k % 8) - 0.35 << " " << 0.1 * (k / 8) - 0.35
            << " 4 " << (int)kCode << " " << (int)kCode + (k % 2) << " " << (int)kCode
            << " 0.5\n";
    return root;
}

// One step at `exposure` stops; the GT and the brightness match should both
// read `display`.
void run(float exposure, float display, const char* gt_what, const char* luma_what) {
    const fs::path tmp = fs::temp_directory_path() /
                         ("dlogm_session_test_" + std::to_string(std::random_device{}()));
    const fs::path data = write_dataset(tmp / "data");

    TrainerSession s;
    s.cfg.data = data.string();
    s.cfg.output_dir_prefix = (tmp / "out").string();
    s.cfg.image_color_log = "dlogm-osmo360";
    s.cfg.image_color_log_exposure = exposure;
    s.cfg.num_iterations = 1;
    s.cfg.steps_per_save = 0;
    s.cfg.eval_mode = "all";
    s.cfg.disable_viewer = true;
    s.cfg.keep_viewer_alive = false;
    s.cfg.cache_images = "cpu";
    s.cfg.background_mode = "noise";
    s.cfg.background_match_luminance = true;
    s.log_fn = [](const std::string&) {};

    s.check_config();
    s.load_dataset();
    s.setup_engine();
    check(engine().color_space.image_curve == (int)colorspace::InputCurve::DlogMOsmo360,
          "setup_engine: arms the GT decode");

    s.train();

    const auto& rgb = engine().gt.rgb;
    const int64_t n = rgb.size<0>() * rgb.size<1>() * rgb.size<2>() * 3;
    std::vector<float> gt((size_t)std::max<int64_t>(n, 0));
    if (n > 0)
        backend::memcpy_sync(gt.data(), rgb.data_ptr(), gt.size() * sizeof(float),
                             backend::MemcpyKind::DeviceToHost);
    double worst = n > 0 ? 0.0 : 1.0;
    for (float v : gt) worst = std::max(worst, (double)std::fabs(v - display));
    std::printf("GT values: %lld, max |GT - %.4f| = %.3g\n", (long long)n, display, worst);
    check(n > 0 && worst < 1e-4, gt_what);

    const auto& luma = engine().background.luma_by_cam_host;
    int seen = 0;
    double luma_err = 0.0;
    for (float l : luma)
        if (!std::isnan(l)) { seen++; luma_err = std::max(luma_err, (double)std::fabs(l - display)); }
    std::printf("brightness match: %d view(s) measured, max |luma - %.4f| = %.3g\n",
                seen, display, luma_err);
    check(seen > 0 && luma_err < 1e-4, luma_what);

    engine_reset();
    std::error_code ec;
    fs::remove_all(tmp, ec);
}

}  // namespace

int main() {
    run(0.0f, kDisplay, "step: the uploaded GT is decoded (code 0.4 -> 0.4614)",
        "step: the brightness match measures decoded light, not codes");
    // +1 stop doubles the decoded 0.18 before the display encode.
    run(1.0f, colorspace::linear_to_srgb(0.36f),
        "exposure +1: the uploaded GT is 0.36 linear, displayed",
        "exposure +1: the brightness match reads the brightened GT");
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
