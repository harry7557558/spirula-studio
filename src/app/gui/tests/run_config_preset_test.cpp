// run_config_preset_test -- a run's config.json opened as a preset or a batch
// row (app/gui/TrainPreset.h) keeps `--image-color-log auto`: the curve one
// dataset settled on must not follow it to the next dataset.

#include "app/TrainerCore.h"
#include "app/gui/TrainPreset.h"
#include "external/stb_image_write.h"

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

void check(bool ok, const std::string& what) {
    std::printf("%-72s %s\n", what.c_str(), ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

void write_colmap(const fs::path& root) {
    constexpr int kW = 16, kH = 12;
    fs::create_directories(root / "images");
    fs::create_directories(root / "sparse" / "0");
    std::vector<uint8_t> px((size_t)kW * kH * 3, 102);
    std::ofstream im(root / "sparse" / "0" / "images.txt");
    for (int i = 0; i < 3; i++) {
        const std::string name = "v" + std::to_string(i) + ".png";
        stbi_write_png((root / "images" / name).string().c_str(), kW, kH, 3, px.data(), kW * 3);
        im << i + 1 << " 1 0 0 0 " << 0.2 * i << " 0 0 1 " << name << "\n\n";
    }
    std::ofstream(root / "sparse" / "0" / "cameras.txt") << "1 PINHOLE 16 12 15 15 8 6\n";
    std::ofstream pts(root / "sparse" / "0" / "points3D.txt");
    for (int k = 0; k < 16; k++)
        pts << k + 1 << " " << 0.1 * (k % 4) << " " << 0.1 * (k / 4) << " 4 102 102 102 0.5\n";
}

// A run on `data` with the given record, and the preset its config.json makes.
gui::TrainPreset run_then_preset(const fs::path& tmp, const char* name, ClipColor mode) {
    const fs::path data = tmp / name, run = tmp / (std::string(name) + "_run");
    write_colmap(data);
    DatasetColor d;
    d.clips.push_back({mode, mode == ClipColor::DlogM ? 19 : 0, "dvtm_oq101.proto", "a.OSV"});
    write_dataset_color(data.string(), d);
    TrainerSession s;
    s.cfg.data = data.string();
    s.log_fn = [](const std::string&) {};
    s.load_dataset();
    fs::create_directories(run);
    save_config_json(s.cfg, run, "3dgs");
    return gui::load_preset((run / "config.json").string());
}

}  // namespace

int main() {
    const fs::path tmp =
        fs::temp_directory_path() / ("run_config_preset_test_" + std::to_string(std::random_device{}()));
    fs::create_directories(tmp);

    const gui::TrainPreset log = run_then_preset(tmp, "dlogm", ClipColor::DlogM);
    check(log.cfg.image_color_log == "auto" && !log.touched.count("image_color_log"),
          "preset: a D-Log M run's config.json loads as auto");
    check(resolve_color(log.cfg).image_curve == colorspace::InputCurve::None,
          "preset: the curve that run settled on is not carried to the next dataset");

    const gui::TrainPreset plain = run_then_preset(tmp, "normal", ClipColor::Normal);
    check(plain.cfg.image_color_log == "auto" && !plain.touched.count("image_color_log"),
          "preset: a Normal run's config.json does not switch auto-detection off");

    std::error_code ec;
    fs::remove_all(tmp, ec);
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
