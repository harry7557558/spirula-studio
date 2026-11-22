// exr_decode -- the EXR reader against a corpus OpenEXR itself produced.
//
//   python tools/gen_exr_cases.py /tmp/exr_cases && ./build_vulkan/exr_decode /tmp/exr_cases
//
// Non-DWA paths are bit-exact; DWA allows CPU-dependent DCT rounding.
// A case named err_* must be refused with a non-empty message.

#include "core/ExrImage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool read_f32(const std::string& path, std::vector<float>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize n = f.tellg();
    f.seekg(0);
    out.resize((size_t)n / sizeof(float));
    return (bool)f.read((char*)out.data(), n);
}

bool same_pixel(float a, float b, bool dwa) {
    if (std::memcmp(&a, &b, sizeof(float)) == 0) return true;
    if (!dwa || !std::isfinite(a) || !std::isfinite(b)) return false;
    // DCT rounding and the nonlinear LUT each round to half; SSE2 and AVX can differ.
    auto encoded = [](float x) {
        const double v = std::fabs((double)x);
        return std::copysign(v <= 1 ? std::pow(v, 1 / 2.2)
                                  : 1 + std::log(v) / std::log(9.02501329156), x);
    };
    const double x = encoded(a), y = encoded(b);
    return std::fabs(x - y) <= 1e-6 + 2 * std::ldexp(std::max(std::fabs(x), std::fabs(y)), -10);
}

bool same(const std::vector<float>& a, const std::vector<float>& b, bool dwa) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (!same_pixel(a[i], b[i], dwa)) return false;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: exr_decode <case-dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    std::ifstream manifest(dir + "/cases.txt");
    if (!manifest) {
        std::fprintf(stderr, "cannot read %s/cases.txt -- run tools/gen_exr_cases.py\n",
                     dir.c_str());
        return 2;
    }

    int total = 0, failed = 0;
    std::string name;
    int want_w = 0, want_h = 0;
    while (manifest >> name >> want_w >> want_h) {
        total++;
        const std::string path = dir + "/" + name + ".exr";
        const bool want_error = name.rfind("err_", 0) == 0;

        std::vector<float> want;
        if (!want_error && !read_f32(dir + "/" + name + ".f32", want)) {
            std::printf("BAD  %s: no reference\n", name.c_str());
            failed++;
            continue;
        }
        std::vector<float> serial;
        for (int threads : {1, 8}) {
            exr::Info info;
            exr::Options opt;
            opt.threads = threads;
            std::vector<float> got;
            const std::string err = exr::decode(path, opt, info, got);
            const bool dwa = info.compression == "DWAA" || info.compression == "DWAB";
            if (threads == 1) serial = got;
            else if (err.empty() && !same(got, serial, false)) {
                std::printf("BAD  %s: decode changes with thread count\n", name.c_str());
                failed++;
            }
            if (want_error) {
                if (err.empty()) {
                    std::printf("BAD  %s t%d: decoded, but should not have\n",
                                name.c_str(), threads);
                    failed++;
                } else if (threads == 1) {
                    std::printf("ok   %s: refused (%s)\n", name.c_str(), err.c_str());
                }
                continue;
            }
            if (!err.empty()) {
                std::printf("BAD  %s t%d: %s\n", name.c_str(), threads, err.c_str());
                failed++;
            } else if (info.width != want_w || info.height != want_h) {
                std::printf("BAD  %s t%d: %dx%d, expected %dx%d\n", name.c_str(),
                            threads, info.width, info.height, want_w, want_h);
                failed++;
            } else if (!same(got, want, dwa)) {
                size_t at = got.size();
                float a = 0.0f, b = 0.0f;
                for (size_t i = 0; i < got.size() && i < want.size(); i++)
                    if (!same_pixel(got[i], want[i], dwa)) {
                        at = i; a = got[i]; b = want[i];
                        break;
                    }
                std::printf("BAD  %s t%d: differs at %zu (%g vs %g)\n", name.c_str(),
                            threads, at, a, b);
                failed++;
            } else if (threads == 1) {
                std::printf("ok   %s: %dx%d %s\n", name.c_str(), info.width, info.height,
                            info.compression.c_str());
            }
        }
    }
    std::printf("\n%d cases, %d failures\n", total, failed);
    return failed ? 1 : 0;
}
