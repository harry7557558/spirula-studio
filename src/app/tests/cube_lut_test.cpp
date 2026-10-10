// cube_lut -- .cube parsing and the tetrahedral map (app/CubeLut.h). Any
// interpolation reproduces an affine colour map exactly, and an identity LUT
// must hand back every 8-bit value unchanged, so both are asserted per value.

#include "app/CubeLut.h"
#include "core/SourcePath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

using Map = std::function<void(float r, float g, float b, float out[3])>;

std::string write_cube(const std::string& name, int n, const Map& f,
                       const std::string& header = "") {
    const fs::path p = fs::temp_directory_path() / name;
    std::ofstream o(p, std::ios::binary);
    o << "# made by cube_lut_test\r\nTITLE \"test\"\r\n" << header
      << "LUT_3D_SIZE\t" << n << "\r\n\r\n";
    for (int b = 0; b < n; b++)
        for (int g = 0; g < n; g++)
            for (int r = 0; r < n; r++) {
                float v[3];
                f(r / (float)(n - 1), g / (float)(n - 1), b / (float)(n - 1), v);
                o << v[0] << ' ' << v[1] << "\t" << v[2] << "\n";
            }
    return p.string();
}

// Every value of each channel, and a spread of mixed colours.
std::vector<uint8_t> probe_pixels() {
    std::vector<uint8_t> px;
    for (int v = 0; v < 256; v++) px.insert(px.end(), {(uint8_t)v, (uint8_t)v, (uint8_t)v});
    for (int i = 0; i < 4096; i++)
        px.insert(px.end(), {(uint8_t)(i * 37 % 256), (uint8_t)(i * 101 % 256),
                             (uint8_t)(i * 173 % 256)});
    return px;
}

int max_error(const app::CubeLut& lut, const Map& f) {
    std::vector<uint8_t> px = probe_pixels(), in = px;
    app::apply_cube_lut(lut, px.data(), px.size() / 3, 4);
    int worst = 0;
    for (size_t i = 0; i < px.size(); i += 3) {
        float v[3];
        f(in[i] / 255.0f, in[i + 1] / 255.0f, in[i + 2] / 255.0f, v);
        for (int c = 0; c < 3; c++) {
            const int want = (int)std::lround(std::min(std::max(v[c], 0.0f), 1.0f) * 255.0f);
            worst = std::max(worst, std::abs(want - (int)px[i + c]));
        }
    }
    return worst;
}

}  // namespace

int main() {
    const Map identity = [](float r, float g, float b, float o[3]) { o[0] = r; o[1] = g; o[2] = b; };
    const Map affine = [](float r, float g, float b, float o[3]) {
        o[0] = 0.6f * r + 0.3f * g + 0.1f * b;
        o[1] = 0.1f + 0.8f * g;
        o[2] = 1.0f - b;
    };

    app::CubeLut lut;
    std::string err;
    for (int n : {2, 17, 33}) {
        expect(app::load_cube_lut(write_cube("id.cube", n, identity), lut, err) &&
                   lut.size == n && lut.title == "test",
               "identity " + std::to_string(n) + " parses " + err);
        expect(max_error(lut, identity) == 0, "identity " + std::to_string(n) + " is exact");
    }
    expect(app::load_cube_lut(write_cube("affine.cube", 5, affine), lut, err),
           "affine parses " + err);
    expect(max_error(lut, affine) <= 1, "affine is reproduced within rounding");

    // A domain wider than [0, 1] reads the input as a fraction of it.
    const Map half = [](float r, float g, float b, float o[3]) { o[0] = 2 * r; o[1] = 2 * g; o[2] = 2 * b; };
    expect(app::load_cube_lut(write_cube("dom.cube", 9, identity,
                                         "DOMAIN_MIN 0 0 0\nDOMAIN_MAX 2.0 2.0 2.0\n"),
                              lut, err) && lut.domain_max[1] == 2.0f,
           "DOMAIN_MAX is read");
    expect(max_error(lut, [&](float r, float g, float b, float o[3]) {
               half(r / 4, g / 4, b / 4, o);
           }) <= 1,
           "a wider domain scales the input");
    expect(app::load_cube_lut(write_cube("range.cube", 5, identity,
                                         "LUT_3D_INPUT_RANGE 0.0 4.0\n"),
                              lut, err) && lut.domain_max[0] == 4.0f,
           "LUT_3D_INPUT_RANGE sets the domain");

    // What must be refused rather than half-read.
    {
        const fs::path p = fs::temp_directory_path() / "short.cube";
        auto refused = [&](const char* text) {
            std::ofstream(p) << text;
            return !app::load_cube_lut(p.string(), lut, err);
        };
        expect(refused("LUT_3D_SIZE 2\n0 0 0\n1 1 1\n") &&
                   err.find("expected 8") != std::string::npos,
               "too few samples fail");
        expect(refused("LUT_1D_SIZE 4\n0 0 0\n") && err.find("1D") != std::string::npos,
               "a 1D LUT fails as one");
        expect(!app::load_cube_lut((fs::temp_directory_path() / "none.cube").string(), lut, err),
               "a missing file fails");
    }

    expect(app::ffmpeg_lut3d_filter("C:\\LUTs\\a b,c'd.cube") ==
               R"(lut3d=file=C\\:/LUTs/a b\,c\\\'d.cube:interp=tetrahedral)",
           "ffmpeg escaping: " + app::ffmpeg_lut3d_filter("C:\\LUTs\\a b,c'd.cube"));

    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures ? 1 : 0;
}
