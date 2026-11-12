#include "checkpoint/SplatPly.h"
#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

TorchTensorView view(std::vector<float>& data, std::vector<int64_t> shape) {
    return {(uint64_t)data.data(), 4, std::move(shape)};
}

void run(const fs::path& dir, int64_t n, ColorBasis basis, int bits, bool fused, bool verify) {
    engine_reset();
    const int k = basis.coeffs();
    std::vector<float> means(n * 3), quats(n * 4), scales(n * 3, -2), opacity(n, 0), dc(n * 3), sh(n * k * 3);
    for (int64_t i = 0; i < n; ++i) {
        means[i * 3] = float(i);
        means[i * 3 + 1] = float(i % 73) * .125f;
        quats[i * 4] = 1;
        dc[i * 3] = float(i % 19) * .0625f;
    }
    for (size_t i = 0; i < sh.size(); ++i) sh[i] = float(int(i % 257) - 128) / 128;
    std::vector<uint8_t> keep(n, 1);
    if (verify && n > 0) {
        for (int64_t i = 0; i < n; ++i) {
            if (i % 31 == 0) { opacity[i] = -6; keep[i] = 0; }
            if (i % 37 == 0) {
                for (int a = 0; a < 3; ++a) scales[i * 3 + a] = -40;
                keep[i] = 0;
            }
            if (i % 41 == 0) { means[i * 3 + 2] = std::numeric_limits<float>::infinity(); keep[i] = 0; }
        }
        if (bits == 32 && k > 0) { sh[3 * k] = std::numeric_limits<float>::quiet_NaN(); keep[1] = 0; }
    }
    set_data_3dgs(n, view(means, {n, 3}), view(quats, {n, 4}), view(scales, {n, 3}),
                  view(opacity, {n, 1}), view(dc, {n, 3}), view(sh, {n, k, 3}), basis.sb_lobes);
    if (bits != 32) {
        engine().optim.use_fused_proj_bwd_optim = fused;
        engine_ensure_optim_state(8, bits, 32, false);
    }
    const auto start = std::chrono::steady_clock::now();
    engine_save_checkpoint(dir.string(), false, 123);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("n=%lld degree=%d lobes=%d bits=%d fused=%d: %.3fs, %.1f MiB/s\n",
                (long long)n, basis.sh_degree, basis.sb_lobes, bits, fused, seconds,
                double(fs::file_size(dir / "splat.ply")) / (1 << 20) / seconds);
    require(fs::exists(dir / "state.tar"), "state.tar was not published");
    for (const auto& entry : fs::directory_iterator(dir))
        require(entry.path().extension() != ".tmp", "checkpoint left a temporary file");
    if (verify) {
        const auto saved = spirula::read_splat_ply((dir / "splat.ply").string());
        require(saved.basis() == basis, "checkpoint changed the colour basis");
        int64_t row = 0;
        for (int64_t i = 0; i < n; ++i) {
            if (!keep[i]) continue;
            require(row < saved.num, "checkpoint dropped a live splat");
            for (int a = 0; a < 3; ++a) {
                require(saved.means[row * 3 + a] == means[i * 3 + a], "positions or row order changed");
                require(saved.scales[row * 3 + a] == scales[i * 3 + a], "scales changed");
                require(saved.features_dc[row * 3 + a] == dc[i * 3 + a], "DC changed");
            }
            for (int a = 0; a < 4; ++a)
                require(saved.quats[row * 4 + a] == quats[i * 4 + a], "quaternions changed");
            require(saved.opacities[row] == opacity[i], "opacity changed");
            const float tolerance = bits == 32 ? 0 : bits == 8 ? 1.0f / 255 + 1e-6f : 1.0f / 65535 + 1e-6f;
            for (int a = 0; a < k * 3; ++a)
                require(std::fabs(saved.features_sh[row * k * 3 + a] - sh[i * k * 3 + a]) <= tolerance,
                        "SH or lobe values changed beyond quantization error");
            ++row;
        }
        require(row == saved.num, "checkpoint retained a filtered splat");
        require(fs::file_size(dir / "splat.ply") ==
                spirula::splat_ply_header(row, basis, engine().primitive).size() +
                size_t(row) * spirula::splat_ply_row_floats(basis) * sizeof(float),
                "checkpoint has extra or missing bytes");
    }
    fs::remove_all(dir);
}
}

int main(int argc, char** argv) {
    const fs::path parent = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path();
    const auto dir = parent / ("ss-checkpoint-ply-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        if (argc > 2) {
            const int64_t n = std::stoll(argv[2]);
            const int bits = argc > 3 ? std::stoi(argv[3]) : 8;
            const bool fused = argc > 4 ? std::stoi(argv[4]) != 0 : true;
            for (int repeat = 0; repeat < 3; ++repeat) run(dir, n, {3, 0}, bits, fused, false);
        } else {
            run(dir, 1, {0, 0}, 32, false, true);
            run(dir, 140003, {0, 0}, 32, false, true);
            run(dir, 70003, {3, 0}, 32, false, true);
            for (bool fused : {false, true})
                for (int bits : {8, 16}) run(dir, 70003, {3, 0}, bits, fused, true);
#ifdef SS_BACKEND_VULKAN
            run(dir, 70003, {2, 3}, 32, false, true);
            run(dir, 70003, {2, 3}, 8, true, true);
#endif
        }
        engine_reset();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        fs::remove_all(dir);
        return 1;
    }
}
