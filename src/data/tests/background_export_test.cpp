#include "checkpoint/BackgroundExport.h"
#include "core/CheckpointIO.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static void test_eval_sh_constant() {
    float3 dc = make_float3(1.0f, 2.0f, 3.0f);
    float3 d1 = make_float3(1.0f, 0.0f, 0.0f);
    float3 d2 = make_float3(0.0f, 1.0f, 0.0f);
    float3 d3 = make_float3(0.0f, 0.0f, 1.0f);

    float3 c1 = ckpt::eval_sh_direction(0, d1, dc, nullptr);
    float3 c2 = ckpt::eval_sh_direction(0, d2, dc, nullptr);
    float3 c3 = ckpt::eval_sh_direction(0, d3, dc, nullptr);

    float expected_x = 0.2820947917738781f * 1.0f;
    check(std::abs(c1.x - expected_x) < 1e-5f, "DC evaluates correctly for x");
    check(std::abs(c1.x - c2.x) < 1e-6f && std::abs(c1.x - c3.x) < 1e-6f,
          "DC only is direction-invariant");
}

static void test_equirectangular_render() {
    std::vector<float3> coeffs(25, make_float3(0.0f, 0.0f, 0.0f));
    coeffs[0] = make_float3(0.5f, 0.5f, 0.5f);

    std::vector<uint8_t> rgb8;
    bool ok = ckpt::render_background_sh_equirectangular(coeffs.data(), 4, 64, 32, rgb8);
    check(ok, "render equirectangular succeeds");
    check(rgb8.size() == 64 * 32 * 3, "output buffer size matches dimensions");

    bool in_range = true;
    for (uint8_t v : rgb8) {
        if (v == 0) in_range = false;
    }
    check(in_range, "pixels have positive non-zero brightness");
}

static void test_state_tar_roundtrip() {
    fs::path tmp_dir = fs::temp_directory_path() / "spirula_bg_test";
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
    fs::create_directories(tmp_dir, ec);

    fs::path tarpath = tmp_dir / "state.tar";
    {
        std::ofstream tar(tarpath.string(), std::ios::binary);
        check(tar.good(), "open test tar for writing");

        std::string sj = R"({"step": 100, "background": {"mode": "sh", "sh_degree": 2, "splat_transfer": 0, "splat_is_linear": 0}})";
        ckpt::tar_write_bytes(tar, "state.json", sj.data(), sj.size());

        std::vector<float3> test_coeffs(9);
        for (int i = 0; i < 9; ++i) {
            test_coeffs[i] = make_float3((float)i * 0.1f, (float)i * 0.2f, (float)i * 0.3f);
        }
        std::string npy_h = ckpt::npy_header("<f4", 9 * 3);
        size_t npy_size = npy_h.size() + test_coeffs.size() * sizeof(float3);
        ckpt::tar_header(tar, "eng.bg_sky.sh_coeffs.npy", npy_size);
        tar.write(npy_h.data(), (std::streamsize)npy_h.size());
        tar.write(reinterpret_cast<const char*>(test_coeffs.data()),
                  (std::streamsize)(test_coeffs.size() * sizeof(float3)));
        ckpt::tar_pad(tar, npy_size);
        ckpt::tar_finish(tar);
    }

    int deg = -1, transfer = -1;
    bool is_linear = true;
    std::vector<float3> loaded;
    bool load_ok = ckpt::load_background_sh(tmp_dir, deg, loaded, transfer, is_linear);
    check(load_ok, "load_background_sh succeeds from state.tar");
    check(deg == 2, "degree is reconstructed as 2 (9 coeffs)");
    check(loaded.size() == 9, "loaded 9 coefficients");
    check(transfer == 0, "transfer read from state.json");
    check(!is_linear, "is_linear read from state.json");

    if (loaded.size() == 9) {
        check(std::abs(loaded[1].y - 0.2f) < 1e-5f, "coefficient values match");
    }

    fs::remove_all(tmp_dir, ec);
}

int main() {
    test_eval_sh_constant();
    test_equirectangular_render();
    test_state_tar_roundtrip();

    if (failures == 0) {
        std::puts("background_export_test: OK");
        return 0;
    }
    return 1;
}
