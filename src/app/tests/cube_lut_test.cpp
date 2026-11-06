// cube_lut -- .cube parsing and the tetrahedral map (app/CubeLut.h), and the
// built-in log LUTs (app/LogLuts.h). Any interpolation reproduces an affine
// colour map exactly, and an identity LUT must hand back every 8-bit value
// unchanged, so both are asserted per value.

#include "app/CubeLut.h"
#include "app/LogLuts.h"
#include "app/tests/log_lut_checks.h"
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

bool rgb48_matches_rgb8(const app::CubeLut& lut) {
    std::vector<uint8_t> rgb8 = probe_pixels();
    std::vector<uint8_t> rgb48(rgb8.size() * 2), mapped(rgb8.size());
    for (size_t i = 0; i < rgb8.size(); i++) {
        rgb48[i * 2] = rgb8[i];
        rgb48[i * 2 + 1] = rgb8[i];
    }
    app::apply_cube_lut(lut, rgb8.data(), rgb8.size() / 3, 4);
    app::apply_cube_lut_rgb48be(lut, rgb48.data(), mapped.data(), mapped.size() / 3, 4);
    return mapped == rgb8;
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
        expect(rgb48_matches_rgb8(lut),
               "identity " + std::to_string(n) + ": RGB16 and RGB8 exact samples agree");
    }
    expect(app::load_cube_lut(write_cube("affine.cube", 5, affine), lut, err),
           "affine parses " + err);
    expect(max_error(lut, affine) <= 1, "affine is reproduced within rounding");
    expect(rgb48_matches_rgb8(lut), "affine: RGB16 and RGB8 exact samples agree");

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
    expect(rgb48_matches_rgb8(lut), "wide domain: RGB16 and RGB8 exact samples agree");
    expect(app::load_cube_lut(write_cube("range.cube", 5, identity,
                                         "LUT_3D_INPUT_RANGE 0.0 4.0\n"),
                              lut, err) && lut.domain_max[0] == 4.0f,
           "LUT_3D_INPUT_RANGE sets the domain");

    {
        app::CubeLut steep;
        expect(app::load_cube_lut(write_cube("steep.cube", 2, identity,
                                             "DOMAIN_MIN .4 .4 .4\n"
                                             "DOMAIN_MAX .42 .42 .42\n"), steep, err),
               "steep domain parses " + err);
        const uint16_t levels[] = {26432, 26496};
        uint8_t rgb48[12], mapped[6]{}, quantized[6];
        for (int p = 0; p < 2; p++)
            for (int c = 0; c < 3; c++) {
                const int i = p * 3 + c;
                rgb48[i * 2] = (uint8_t)(levels[p] >> 8);
                rgb48[i * 2 + 1] = (uint8_t)levels[p];
                quantized[i] = (uint8_t)((levels[p] + 128) / 257);
            }
        app::apply_cube_lut_rgb48be(steep, rgb48, mapped, 2, 1);
        app::apply_cube_lut(steep, quantized, 2, 1);
        int worst = 0;
        for (int p = 0; p < 2; p++) {
            const int want = (int)std::lround(
                std::clamp((levels[p] / 65535.0 - .4) / .02, 0.0, 1.0) * 255);
            for (int c = 0; c < 3; c++)
                worst = std::max(worst, std::abs(want - (int)mapped[p * 3 + c]));
        }
        expect(worst <= 1 && mapped[3] > mapped[0] + 10 && quantized[0] == quantized[3],
               "steep LUT preserves close 10-bit levels that RGB8 input collapses");
    }

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
        expect(refused("LUT_1D_SIZE 2\nLUT_3D_SIZE 2\n0 0 0\n1 1 1\n") &&
                   err.find("shaper") != std::string::npos,
               "a shaper in front of the 3D LUT is named, not miscounted");
        expect(!app::load_cube_lut((fs::temp_directory_path() / "none.cube").string(), lut, err),
               "a missing file fails");
    }

    expect(app::ffmpeg_lut3d_filter("C:\\LUTs\\a b,c'd.cube") ==
               R"(lut3d=file=C\\:/LUTs/a b\,c\\\'d.cube:interp=tetrahedral)",
           "ffmpeg escaping: " + app::ffmpeg_lut3d_filter("C:\\LUTs\\a b,c'd.cube"));

    // What a .cube written here reads back as, to the digits it was written.
    err.clear();
    {
        app::CubeLut a;
        expect(app::load_cube_lut(write_cube("w.cube", 5, affine), a, err), "affine for writing");
        const std::string p = (fs::temp_directory_path() / "w_out.cube").string();
        app::CubeLut b;
        expect(app::write_cube_lut(a, p, err) && app::load_cube_lut(p, b, err) &&
                   b.size == a.size && b.title == a.title,
               "write_cube_lut reads back " + err);
        float worst = 0;
        for (size_t i = 0; i < a.rgb.size() && b.rgb.size() == a.rgb.size(); i++)
            worst = std::max(worst, std::fabs(a.rgb[i] - b.rgb[i]));
        expect(worst < 1e-6f, "written values survive");
    }

    // A LUT re-exported under the same name is read again, not served stale.
    {
        const std::string p = write_cube("edit.cube", 2, identity);
        const auto first = app::cube_lut_cached(p, err);
        const auto again = app::cube_lut_cached(p, err);
        write_cube("edit.cube", 3, affine);
        const auto edited = app::cube_lut_cached(p, err);
        expect(first && again == first && edited && edited != first && edited->size == 3 &&
                   edited->serial != first->serial,
               "the cache follows an edited file");
    }

    // Every built-in bakes, rises along grey, and is a file ffmpeg can read.
    expect(!app::builtin_luts().empty(), "there are built-in LUTs");
    for (const app::BuiltinLut& b : app::builtin_luts()) {
        const std::string spec = app::builtin_lut_spec(b.id);
        const auto lut = app::cube_lut_cached(spec, err);
        bool ok = lut && lut->size > 1 && app::lut_display_name(spec) == b.name;
        float grey_prev = -1;
        for (int i = 0; ok && i < lut->size; i++) {
            const float g = lut->rgb[(((size_t)i * lut->size + i) * lut->size + i) * 3 + 1];
            ok = g >= grey_prev - 1.0f / 255 && g >= 0 && g <= 1;
            grey_prev = g;
        }
        expect(ok, std::string(b.id) + ": bakes, grey rising " + err);
        std::string file;
        app::CubeLut back;
        expect(app::cube_lut_file(spec, file, err) && app::load_cube_lut(file, back, err) &&
                   back.size == lut->size,
               std::string(b.id) + ": baked to a .cube ffmpeg reads " + err);
    }
    expect(!app::cube_lut_cached(app::builtin_lut_spec("no-such-lut"), err),
           "an unknown built-in fails");
    // A fitted look as tools/fit_log_luts.py evaluates it; GoPro's as GoPro's own
    // LUT and generator give it.
    for (const app::BuiltinLut& b : app::builtin_luts()) {
        const auto lut = app::cube_lut_cached(app::builtin_lut_spec(b.id), err);
        double worst = 0;
        int checked = 0;
        for (const LogLutCheck& c : kLogLutChecks) {
            if (!lut || std::string(c.id) != b.id) continue;
            const size_t at = (((size_t)c.b * lut->size + c.g) * lut->size + c.r) * 3;
            for (int k = 0; k < 3; k++)
                worst = std::max(worst, std::fabs((double)lut->rgb[at + k] - c.rgb[k]));
            checked++;
        }
        expect(checked > 0 && worst < 5e-5,
               std::string(b.id) + ": matches tools/fit_log_luts.py, worst " + std::to_string(worst));
    }

    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures ? 1 : 0;
}
