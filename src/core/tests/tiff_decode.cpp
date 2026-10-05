// tiff_decode -- the TIFF reader against a corpus tifffile and libtiff wrote.
//
//   python tools/gen_tiff_cases.py /tmp/tiff_cases && ./build_vulkan/tiff_decode /tmp/tiff_cases
//
// Every case must decode to its .ref companion within its tolerance, on one
// thread and on many, and to sRGB8 as those samples quantized; a case named
// err_* must be refused with a non-empty message.

#include "core/ColorSpace.h"
#include "core/TiffImage.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool read_all(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize n = f.tellg();
    f.seekg(0);
    out.resize((size_t)n);
    return (bool)f.read((char*)out.data(), n);
}

// Index of the first sample further apart than `tol`, or SIZE_MAX. Floats
// compare by bit pattern, so a NaN or a signed zero has to survive too.
size_t first_difference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                        tiff::Sample s, double tol) {
    if (a.size() != b.size()) return 0;
    const size_t bytes = s == tiff::Sample::U8 ? 1 : s == tiff::Sample::U16 ? 2 : 4;
    for (size_t i = 0; i < a.size() / bytes; i++) {
        const uint8_t* x = a.data() + i * bytes;
        const uint8_t* y = b.data() + i * bytes;
        if (std::memcmp(x, y, bytes) == 0) continue;
        double u = 0, v = 0;
        if (s == tiff::Sample::U8) { u = *x; v = *y; }
        else if (s == tiff::Sample::U16) {
            uint16_t p, q;
            std::memcpy(&p, x, 2); std::memcpy(&q, y, 2);
            u = p; v = q;
        } else {
            float p, q;
            std::memcpy(&p, x, 4); std::memcpy(&q, y, 4);
            u = p; v = q;
        }
        if (s == tiff::Sample::F32 || std::fabs(u - v) > tol) return i;
    }
    return SIZE_MAX;
}

// decode_srgb8 with nothing declared is the identity transfer, so it has to be
// `native` scaled to 0..1 and rounded.
bool srgb8_matches(const std::vector<uint8_t>& native, tiff::Sample s,
                   const std::vector<uint8_t>& srgb) {
    const size_t bytes = s == tiff::Sample::U8 ? 1 : s == tiff::Sample::U16 ? 2 : 4;
    if (native.size() != srgb.size() * bytes) return false;
    for (size_t i = 0; i < srgb.size(); i++) {
        float v;
        if (s == tiff::Sample::U8) {
            v = native[i] / 255.0f;
        } else if (s == tiff::Sample::U16) {
            uint16_t q;
            std::memcpy(&q, native.data() + 2 * i, 2);
            v = q / 65535.0f;
        } else {
            std::memcpy(&v, native.data() + 4 * i, 4);
        }
        if (srgb[i] != colorspace::quantize_unit8(v)) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: tiff_decode <case-dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    std::ifstream manifest(dir + "/cases.txt");
    if (!manifest) {
        std::fprintf(stderr, "cannot read %s/cases.txt -- run tools/gen_tiff_cases.py\n",
                     dir.c_str());
        return 2;
    }

    int total = 0, failed = 0;
    std::string name, sample;
    int want_w = 0, want_h = 0, channels = 3;
    double tol = 0;
    while (manifest >> name >> want_w >> want_h >> channels >> sample >> tol) {
        total++;
        const std::string path = dir + "/" + name + ".tif";
        const bool want_error = name.rfind("err_", 0) == 0;
        const tiff::Sample want_sample = sample == "u8" ? tiff::Sample::U8
                                       : sample == "u16" ? tiff::Sample::U16
                                       : tiff::Sample::F32;
        std::vector<uint8_t> want;
        if (!want_error && !read_all(dir + "/" + name + ".ref", want)) {
            std::printf("BAD  %s: no reference\n", name.c_str());
            failed++;
            continue;
        }
        for (int threads : {1, 8}) {
            tiff::Info info;
            tiff::Options opt;
            opt.channels = channels;
            opt.threads = threads;
            std::vector<uint8_t> got;
            const std::string err = tiff::decode(path, opt, info, got);
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
            } else if (info.width != want_w || info.height != want_h ||
                       info.sample != want_sample) {
                std::printf("BAD  %s t%d: %dx%d sample %d, expected %dx%d %s\n",
                            name.c_str(), threads, info.width, info.height,
                            (int)info.sample, want_w, want_h, sample.c_str());
                failed++;
            } else if (const size_t at = first_difference(got, want, info.sample, tol);
                       at != SIZE_MAX) {
                std::printf("BAD  %s t%d: differs at sample %zu (%zu vs %zu bytes)\n",
                            name.c_str(), threads, at, got.size(), want.size());
                failed++;
            } else if (std::vector<uint8_t> srgb;
                       !tiff::decode_srgb8(path, opt, info, srgb).empty() ||
                       !srgb8_matches(got, info.sample, srgb)) {
                std::printf("BAD  %s t%d: sRGB8 decode disagrees\n", name.c_str(), threads);
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
