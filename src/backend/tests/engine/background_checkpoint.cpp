#include "engine/Engine.h"
#include "engine/EngineState.h"
#include "checkpoint/BackgroundExport.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void check_dimensions(const std::filesystem::path& path, int width, int height) {
    std::ifstream in(path, std::ios::binary);
    in.seekg(21);
    unsigned char bytes[4]{};
    in.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    uint32_t bits = uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
                    (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
    require(in.good() && int(bits & 16383) + 1 == width && int((bits >> 14) & 16383) + 1 == height,
            "saved background dimensions do not match export height");
}

TorchTensorView view(std::vector<float>& data, std::vector<int64_t> shape) {
    return {(uint64_t)data.data(), 4, std::move(shape)};
}

void seed(int slot = -1) {
    std::vector<float> means{0, 0, 3}, quats{1, 0, 0, 0}, scales(3, -2), opacity(1), color(3, .5f), sh(9);
    if (slot < 0)
        set_data_3dgs(1, view(means, {1, 3}), view(quats, {1, 4}), view(scales, {1, 3}),
                      view(opacity, {1, 1}), view(color, {1, 3}), view(sh, {1, 3, 3}), 0);
    else
        engine_scene_set_data_3dgs(slot, 1, view(means, {1, 3}), view(quats, {1, 4}), view(scales, {1, 3}),
                                   view(opacity, {1, 1}), view(color, {1, 3}), view(sh, {1, 3, 3}), 0);
    engine_init_color_space(true, 2, true, {.5f, 0, 0, 0, 1, 0, 0, 0, 2}, false, 0, false, {});
}
}

int main() {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / ("ss-background-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::vector<float> coefficients(27);
        for (size_t i = 0; i < coefficients.size(); ++i) coefficients[i] = float(i) * .01f;
        engine_reset(); seed();
        engine_init_background_sh(2, 2, true);
        backend::memcpy_sync(engine().background.sh_coeffs.data_ptr(), coefficients.data(),
                             coefficients.size() * sizeof(float), backend::MemcpyKind::HostToDevice);
        engine_save_checkpoint(root.string(), true, 123);
        require(fs::file_size(root / "background.webp") > 25, "checkpoint did not export background");
        for (const auto& e : fs::directory_iterator(root))
            require(e.path().extension() != ".tmp", "checkpoint left a staged .tmp file");
        check_dimensions(root / "background.webp", 1024, 512);
        engine_save_checkpoint((root / "custom").string(), true, 123, 64);
        check_dimensions(root / "custom" / "background.webp", 128, 64);
        require(ckpt::export_background_from_checkpoint(root, root / "standalone.webp"),
                "standalone checkpoint export failed");
        {
            std::ifstream automatic(root / "background.webp", std::ios::binary);
            std::ifstream standalone(root / "standalone.webp", std::ios::binary);
            require(std::vector<char>(std::istreambuf_iterator<char>(automatic), {}) ==
                    std::vector<char>(std::istreambuf_iterator<char>(standalone), {}),
                    "checkpoint and standalone exports disagree on gamut or transfer");
        }
        engine_reset(); seed();
        engine_init_background_sh(2, 0, false);
        require(engine_load_checkpoint(root.string()) == 123, "checkpoint step changed");
        int degree = -1;
        require(engine_has_background_sh(&degree) && degree == 2, "background not restored");
        require(engine().background.splat_transfer == 2 && engine().background.splat_is_linear,
                "same-degree resume ignored saved color metadata");
        std::vector<float> restored(27);
        backend::memcpy_sync(restored.data(), engine().background.sh_coeffs.data_ptr(),
                             restored.size() * sizeof(float), backend::MemcpyKind::DeviceToHost);
        require(restored == coefficients, "background coefficients changed");
        engine_reset(); seed();
        require(engine_load_checkpoint(root.string()) == 123 && engine_has_background_sh(),
                "resume did not initialize absent background");
        engine_reset(); seed(0); seed(1);
        engine_scene_set_background_sh(0, 2, view(coefficients, {9, 3}), 2, true);
        engine_scene_activate(0);
        require(engine_has_background_sh() && engine().background.splat_transfer == 2,
                "scene background not activated");
        engine_scene_activate(1);
        require(!engine_has_background_sh() && !engine().background.sh_coeffs.data_ptr(),
                "scene without background retained previous background");
        engine_scene_activate(0);
        engine_scene_free(0);
        require(!engine_has_background_sh() && !engine().background.sh_coeffs.data_ptr(),
                "freeing scene left dangling background pointer");
        engine_reset();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        fs::remove_all(root);
        return 1;
    }
    fs::remove_all(root);
    std::puts("background_checkpoint: OK");
}
