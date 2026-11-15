#include "checkpoint/BackgroundExport.h"
#include "checkpoint/Resume.h"
#include "core/CheckpointIO.h"
#include "core/ColorSpace.h"

#include <chrono>
#include <cmath>
#include <limits>
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
    coeffs[0] = make_float3(0, 0, 0);
    const float matrix[] = {0.5f, 0, 0, 0, 1, 0, 0, 0, 2};
    check(ckpt::render_background_sh_equirectangular(coeffs.data(), 0, 1, 1, rgb8, 2, false, matrix),
          "gamma-coded background with gamut transform renders");
    for (int c = 0; c < 3; ++c) {
        float expected = colorspace::tone_encode(colorspace::srgb_to_linear(.5f) * matrix[c * 4],
                                                 colorspace::Transfer::Aces);
        check(rgb8[c] == uint8_t(std::clamp(expected, 0.0f, 1.0f) * 255 + .5f),
              "gamut transform precedes display transfer");
    }
    coeffs[0] = make_float3(std::numeric_limits<float>::quiet_NaN(), 0, 0);
    check(!ckpt::render_background_sh_equirectangular(coeffs.data(), 0, 1, 1, rgb8),
          "nonfinite coefficients rejected");
}

static void test_state_tar_roundtrip() {
    fs::path tmp_dir = fs::temp_directory_path() / ("spirula_bg_test_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::remove_all(tmp_dir, ec);
    fs::create_directories(tmp_dir, ec);

    fs::path tarpath = tmp_dir / "state.tar";
    {
        std::ofstream tar(tarpath.string(), std::ios::binary);
        check(tar.good(), "open test tar for writing");

        std::string sj = R"({"step": 100, "background": {"mode": "sh", "sh_degree": 2, "splat_transfer" : 2, "splat_is_linear" : true}})";
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
        const float matrix[] = {.5f, 0, 0, 0, 1, 0, 0, 0, 2};
        auto matrix_npy = ckpt::npy_header("<f4", 9);
        matrix_npy.append(reinterpret_cast<const char*>(matrix), sizeof(matrix));
        ckpt::tar_write_bytes(tar, "color_space.splat_matrix.npy", matrix_npy.data(), matrix_npy.size());
        ckpt::tar_finish(tar);
    }

    int deg = -1, transfer = -1;
    bool is_linear = true;
    std::vector<float3> loaded;
    std::vector<float> loaded_matrix;
    bool load_ok = ckpt::load_background_sh(tmp_dir, deg, loaded, transfer, is_linear, &loaded_matrix);
    check(loaded_matrix == std::vector<float>({.5f, 0, 0, 0, 1, 0, 0, 0, 2}), "saved gamut matrix restored");
    check(load_ok, "load_background_sh succeeds from state.tar");
    check(deg == 2, "degree is reconstructed as 2 (9 coeffs)");
    check(loaded.size() == 9, "loaded 9 coefficients");
    check(transfer == 2, "transfer read from state.json");
    check(is_linear, "is_linear read from state.json");

    if (loaded.size() == 9) {
        check(std::abs(loaded[1].y - 0.2f) < 1e-5f, "coefficient values match");
    }

    fs::path old_ckpt = tmp_dir / "run" / "step-000000001.ckpt";
    fs::path latest = tmp_dir / "run" / "step-000000002.ckpt";
    fs::create_directories(old_ckpt);
    fs::create_directories(latest);
    fs::copy_file(tarpath, latest / "state.tar");
    { std::ofstream empty(old_ckpt / "state.tar", std::ios::binary); ckpt::tar_finish(empty); }
    fs::path unfinished = tmp_dir / "run" / "step-000000003.ckpt";
    fs::create_directories(unfinished);
    { std::ofstream partial(unfinished / "state.tar.tmp", std::ios::binary); partial << "x"; }
    check(ckpt::resolve_checkpoint(tmp_dir / "run").ckpt_dir == latest,
          "run directory skips a checkpoint whose save never finished");
    check(ckpt::load_background_sh(tmp_dir / "run", deg, loaded, transfer, is_linear),
          "run directory picks latest checkpoint");
    check(!ckpt::load_background_sh(old_ckpt, deg, loaded, transfer, is_linear),
          "explicit checkpoint never borrows another background");
    check(ckpt::load_background_sh(tarpath, deg, loaded, transfer, is_linear),
          "direct state.tar accepted");
    fs::path output = tmp_dir / "background $name 'quoted'.webp";
    fs::path png = output; png.replace_extension(".png");
    { std::ofstream sentinel(png); sentinel << "keep"; }
    check(ckpt::export_background_from_checkpoint(tarpath, output, 64, 32),
          "native export succeeds without external programs");
    std::ifstream webp(output, std::ios::binary);
    char magic[16]{}; webp.read(magic, sizeof(magic));
    check(std::string(magic, 4) == "RIFF" && std::string(magic + 8, 8) == "WEBPVP8L",
          "export writes real lossless WebP");
    webp.close();
    check(fs::file_size(png) == 4, "neighboring PNG is untouched");
    check(!ckpt::export_background_from_checkpoint(tarpath, latest, 64, 32),
          "unwritable output reports failure");
    check(!ckpt::export_background_from_checkpoint(tarpath, output, 16385, 1),
          "WebP dimension limit checked before rendering");
    auto npy = ckpt::npy_header("<f4", 3);
    auto write_invalid = [&](const std::string& data) {
        std::ofstream tar(tarpath, std::ios::binary | std::ios::trunc);
        ckpt::tar_write_bytes(tar, "eng.bg_sky.sh_coeffs.npy", data.data(), data.size());
        ckpt::tar_finish(tar);
    };
    write_invalid(npy + std::string(13, '\0'));
    check(!ckpt::load_background_sh(tarpath, deg, loaded, transfer, is_linear),
          "partial float3 rejected without a buffer overrun");
    write_invalid(ckpt::npy_header("<i4", 3) + std::string(12, '\0'));
    check(!ckpt::load_background_sh(tarpath, deg, loaded, transfer, is_linear), "wrong dtype rejected");
    write_invalid("bad");
    check(!ckpt::load_background_sh(tarpath, deg, loaded, transfer, is_linear), "short NPY rejected");
    npy[8] = npy[9] = char(255);
    write_invalid(npy);
    check(!ckpt::load_background_sh(tarpath, deg, loaded, transfer, is_linear), "oversized NPY header rejected");
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
