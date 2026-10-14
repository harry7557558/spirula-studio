// packed_lens -- how a 360 camera's one-image frame is read (app/Pano360.h):
// 2:1 and 1:2 are two fisheye circles, 1:1 is one, and anything else takes the
// nearer of 2:1 and 1:1 rather than failing -- so the shape test, the cut and
// the Kandao box that names an unstitched .mp4 are asserted.

#include "app/Pano360.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

void shape(int w, int h, int lenses, bool exact) {
    bool e = !exact;
    const int got = app::packed_lens_count(w, h, e);
    expect(got == lenses && e == exact,
           std::to_string(w) + "x" + std::to_string(h) + " is " +
               std::to_string(lenses) + (exact ? " lens(es), exactly" : " lens(es), nearest"));
}

// The lenses of a w x h frame whose byte is x*10+y plus the channel index.
void cut(int w, int h, bool stacked) {
    const int ch = 2;
    std::vector<uint8_t> px(w * h * ch);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            for (int c = 0; c < ch; c++) px[(y * w + x) * ch + c] = (uint8_t)(x * 10 + y + c);
    for (int lens = 0; lens < 2; lens++) {
        std::vector<uint8_t> out;
        int ow = 0, oh = 0;
        app::packed_lens_crop(px.data(), w, h, ch, 2, lens, out, ow, oh);
        const int x0 = stacked ? 0 : lens * ow, y0 = stacked ? lens * oh : 0;
        bool same = ow == 2 && oh == 2 && out.size() == (size_t)(ow * oh * ch);
        for (int y = 0; same && y < oh; y++)
            for (int x = 0; x < ow; x++)
                for (int c = 0; c < ch; c++)
                    same = same && out[(y * ow + x) * ch + c] ==
                                       (uint8_t)((x + x0) * 10 + y + y0 + c);
        expect(same, std::string(stacked ? "stacked" : "side by side") + " lens " +
                         std::to_string(lens) + " is its own half");
    }
}

void entry(std::string& box, const char* name, const char* type, uint32_t count,
           const std::string& data) {
    char head[40] = {};
    std::strncpy(head, name, 32);
    std::strncpy(head + 32, type, 8);
    box.append(head, 40);
    box.append((const char*)&count, 4);
    box += data;
}

// An MP4 of an `mdat` and a `kfix` saying `stitched`, as a QooCam 3 Ultra lays
// it out. The counts are written in host order, which is little endian here.
bool unstitched(int stitched, bool with_box) {
    std::string body("\x03\0\0\0", 4);
    entry(body, "PRODUCT", "CHAR", 16, std::string("QooCam 3 Ultra\0\0", 16));
    entry(body, "FOV", "U32", 1, std::string("\x68\x01\0\0", 4));
    entry(body, "STITCHED", "U8", 1, std::string(1, (char)stitched));
    auto box = [](const char* type, const std::string& b) {
        const uint32_t n = (uint32_t)b.size() + 8;
        const char size[4] = {(char)(n >> 24), (char)(n >> 16), (char)(n >> 8), (char)n};
        return std::string(size, 4) + type + b;
    };
    const std::filesystem::path f =
        std::filesystem::temp_directory_path() / "packed_lens_test.mp4";
    std::ofstream(f, std::ios::binary)
        << box("mdat", std::string(100, 'x')) << (with_box ? box("kfix", body) : "");
    const bool got = app::kandao_unstitched(f.string());
    std::error_code ec;
    std::filesystem::remove(f, ec);
    return got;
}

}  // namespace

int main() {
    shape(11904, 5952, 2, true);   // an X5 .insp
    shape(1664, 832, 2, true);     // its .lrv
    shape(2880, 2880, 1, true);
    shape(1920, 1080, 2, false);   // 1.78: nearer 2:1
    shape(1440, 1080, 1, false);   // 1.33: nearer 1:1
    shape(1080, 1920, 1, false);

    shape(3840, 7680, 2, true);    // a QooCam 3 Ultra .mp4, one lens above the other

    cut(4, 2, /*stacked=*/false);
    cut(2, 4, /*stacked=*/true);

    expect(unstitched(0, true), "kfix STITCHED=0 is an unstitched recording");
    expect(!unstitched(1, true), "kfix STITCHED=1 is not");
    expect(!unstitched(0, false), "nor is a file with no kfix");

    if (g_failures == 0) std::printf("packed_lens_test: OK\n");
    return g_failures == 0 ? 0 : 1;
}
