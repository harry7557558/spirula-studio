// frame_file_test -- what frames are written as (app/FrameFile.h): a JPEG XL
// or AVIF decodes to what was encoded, alpha exactly, EXIF intact. The AVIF
// part skips when the build has no AV1 encoder.

#include "core/AvifImage.h"
#include "core/JxlImage.h"
#include "sfm/core/Exif.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failed = 0;

void check(bool ok, const std::string& what) {
    std::printf("%-4s %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) g_failed++;
}

// Smooth colour with fine texture, and an alpha that is a disc.
std::vector<uint8_t> picture(int w, int h, int ch) {
    std::vector<uint8_t> px((size_t)w * h * ch);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t* p = px.data() + ((size_t)y * w + x) * ch;
            const int grain = (x * 7 + y * 13) % 5;
            p[0] = (uint8_t)(x * 255 / w + grain);
            if (ch >= 3) {
                p[1] = (uint8_t)(y * 255 / h);
                p[2] = (uint8_t)((x + y) * 127 / (w + h) + 64 + grain);
            }
            if (ch == 2 || ch == 4) {
                const double r = std::hypot(x - w / 2.0, y - h / 2.0);
                p[ch - 1] = r < h / 3.0 ? 255 : r < h / 3.0 + 2 ? 128 : 0;
            }
        }
    return px;
}

double psnr_colour(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int ch) {
    const int colour = ch >= 3 ? 3 : 1;
    double se = 0;
    size_t n = 0;
    for (size_t i = 0; i < a.size(); i += ch)
        for (int c = 0; c < colour; c++, n++) se += std::pow((double)a[i + c] - b[i + c], 2);
    return se == 0 ? 99.0 : 10 * std::log10(255.0 * 255.0 * n / se);
}

bool alpha_equal(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int ch) {
    for (size_t i = ch - 1; i < a.size(); i += ch)
        if (a[i] != b[i]) return false;
    return true;
}

void check_jxl(const fs::path& dir) {
    const int w = 320, h = 200;
    const std::vector<uint8_t> exif = sfm::exifExposureSegment(1.0 / 250, 2.8, 400);
    for (int ch : {1, 2, 3, 4}) {
        const std::vector<uint8_t> px = picture(w, h, ch);
        std::vector<uint8_t> bytes;
        const std::string why = jxl::encode_srgb8(px.data(), w, h, ch, 95, 3, exif, 1, bytes);
        check(why.empty() && !bytes.empty(), "JPEG XL encodes " + std::to_string(ch) + " channels");
        const fs::path f = dir / ("c" + std::to_string(ch) + ".jxl");
        {
            std::FILE* o = std::fopen(f.string().c_str(), "wb");
            std::fwrite(bytes.data(), 1, bytes.size(), o);
            std::fclose(o);
        }
        jxl::Info info;
        std::vector<uint8_t> back;
        const int want = ch == 2 ? 4 : ch == 1 ? 1 : ch;
        check(jxl::decode_srgb8(f.string(), want, 1, info, back).empty() && info.width == w &&
                  info.height == h,
              "...and decodes at its size");
        if (ch == 2) continue;   // decoded as RGBA, compared below through 4
        const double db = psnr_colour(px, back, ch);
        std::printf("      %d channels: %zu bytes, %.1f dB\n", ch, bytes.size(), db);
        check(db > 38.0, "...close to what went in");
        if (ch == 4) check(alpha_equal(px, back, 4), "...with the alpha exact");
        check(jxl::read_exif(f.string()) == exif, "...and the EXIF as written");
    }
}

void check_avif(const fs::path& dir) {
    if (!avif::can_encode()) {
        std::printf("skip AVIF: this libavif has no AV1 encoder\n");
        return;
    }
    const int w = 320, h = 200;
    const std::vector<uint8_t> exif = sfm::exifExposureSegment(1.0 / 250, 2.8, 400);
    for (int ch : {3, 4}) {
        const std::vector<uint8_t> px = picture(w, h, ch);
        std::vector<uint8_t> bytes;
        const std::string why = avif::encode_srgb8(px.data(), w, h, ch, 90, 8, exif, 1, bytes);
        check(why.empty() && !bytes.empty(), "AVIF encodes " + std::to_string(ch) + " channels");
        const fs::path f = dir / ("c" + std::to_string(ch) + ".avif");
        {
            std::FILE* o = std::fopen(f.string().c_str(), "wb");
            std::fwrite(bytes.data(), 1, bytes.size(), o);
            std::fclose(o);
        }
        avif::Info info;
        std::vector<uint8_t> back;
        check(avif::decode_srgb8(f.string(), ch, 1, info, back).empty() && info.width == w &&
                  info.channels == ch,
              "...and decodes at its size and channels");
        const double db = psnr_colour(px, back, ch);
        std::printf("      %d channels: %zu bytes, %.1f dB\n", ch, bytes.size(), db);
        check(db > 38.0, "...close to what went in");
        if (ch == 4) check(alpha_equal(px, back, 4), "...with the alpha exact");
        check(avif::read_exif(f.string()) == exif, "...and the EXIF as written");
    }
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "ss_frame_file_test";
    fs::create_directories(dir);
    check_jxl(dir);
    check_avif(dir);
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::printf("%s\n", g_failed ? "FAILED" : "all passed");
    return g_failed ? 1 : 0;
}
