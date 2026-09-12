// EXIF orientation: the tag, the pixel transform it names, and the up
// direction a reconstruction takes from it.
//
// The three have to agree or a portrait capture comes out sideways in one
// place and upright in another, so the marker test below pins them together:
// a pixel placed at `exifUpInCamera` must land at the top once the pixels are
// turned by `exifTransform`.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <filesystem>

#include "core/ImageOrient.h"
#include "sfm/core/Exif.h"
#include "sfm/map/Orient.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        fails++;
    }
}

namespace {

// A little-endian TIFF block: IFD0 with Orientation and an Exif sub-IFD
// pointer, the sub-IFD carrying FocalLengthIn35mmFilm and the pixel size.
std::vector<uint8_t> make_tiff(int orientation, int px_w, int px_h, int f35) {
    std::vector<uint8_t> b;
    auto u16 = [&b](unsigned v) {
        b.push_back((uint8_t)v);
        b.push_back((uint8_t)(v >> 8));
    };
    auto u32 = [&b](uint32_t v) {
        for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i)));
    };
    auto entry = [&](unsigned tag, unsigned type, uint32_t value) {
        u16(tag);
        u16(type);
        u32(1);
        if (type == 3) { u16(value); u16(0); } else { u32(value); }
    };
    b.push_back('I'); b.push_back('I');
    u16(42);
    u32(8);                       // IFD0 at byte 8
    const uint32_t ifd0 = 8, n0 = 2;
    const uint32_t sub = ifd0 + 2 + n0 * 12 + 4;
    u16((unsigned)n0);
    entry(0x0112, 3, (uint32_t)orientation);
    entry(0x8769, 4, sub);
    u32(0);                       // no IFD1
    u16(3);
    entry(0xA405, 3, (uint32_t)f35);       // FocalLengthIn35mmFilm
    entry(0xA002, 4, (uint32_t)px_w);      // PixelXDimension
    entry(0xA003, 4, (uint32_t)px_h);      // PixelYDimension
    u32(0);
    return b;
}

// A file whose marker chain readExifSegment can walk: SOI, a decoy APP1 that
// is XMP rather than Exif, the Exif APP1, then EOI. No picture -- nothing here
// decodes one.
void write_jpeg_with_exif(const std::string& path, const std::vector<uint8_t>& tiff) {
    std::vector<uint8_t> f{0xFF, 0xD8};
    auto app1 = [&f](const std::string& tag, const uint8_t* data, size_t n) {
        const size_t len = 2 + tag.size() + n;
        f.push_back(0xFF);
        f.push_back(0xE1);
        f.push_back((uint8_t)(len >> 8));
        f.push_back((uint8_t)len);
        f.insert(f.end(), tag.begin(), tag.end());
        f.insert(f.end(), data, data + n);
    };
    const std::string xmp = "http://ns.adobe.com/xap/1.0/";
    app1(std::string(xmp.c_str(), xmp.size() + 1), (const uint8_t*)"<x/>", 4);
    app1(std::string("Exif\0\0", 6), tiff.data(), tiff.size());
    f.push_back(0xFF);
    f.push_back(0xD9);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write((const char*)f.data(), (std::streamsize)f.size());
}

}  // namespace

static void check_up_matches_turn();
static void check_turn_is_invertible();
static void check_model_up();

static int cmdExifTest(int, char**) {
    // ---- the tag, through a file ----
    const std::string path = "sfm_exif_test.tmp.jpg";
    write_jpeg_with_exif(path, make_tiff(6, 4000, 3000, 24));
    ExifData e = readExif(path);
    check(e.valid, "an Exif APP1 behind an XMP one is still found");
    check(e.orientation == 6, "Orientation reads back");
    check(e.pixel_width == 4000 && e.pixel_height == 3000, "pixel size reads back");
    check(e.focal_35mm == 24, "the focal prior reads back");
    check(exifOrientation(path) == 6, "exifOrientation agrees");

    // ---- flattening leaves the prior alone ----
    std::vector<uint8_t> seg = readExifSegment(path);
    check(seg.size() > 6, "the segment comes back raw");
    exifFlattenOrientation(seg.data() + 6, seg.size() - 6, 3000, 4000);
    e = parseExifTiff(seg.data() + 6, seg.size() - 6);
    check(e.orientation == 1, "flattened: Orientation is 1");
    check(e.pixel_width == 3000 && e.pixel_height == 4000,
          "flattened: the pixel size is the turned one");
    check(e.focal_35mm == 24, "flattened: the focal prior survives");
    std::remove(path.c_str());

    // ---- an orientation with no tag at all ----
    check(readExif("sfm_exif_test.tmp.missing").orientation == 1,
          "a file that is not there is orientation 1");

    check_up_matches_turn();
    check_turn_is_invertible();
    check_model_up();

    if (fails == 0) std::printf("sfm_exif_test: OK\n");
    return fails == 0 ? 0 : 1;
}

// The transform and the up direction name the same turn: a pixel placed at
// `exifUpInCamera` must land above the centre once the pixels are turned.
static void check_up_matches_turn() {
    for (int o = 1; o <= 8; o++) {
        const int w = 5, h = 3;
        double up[3];
        exifUpInCamera(o, up);
        check(up[2] == 0 && std::abs(up[0]) + std::abs(up[1]) == 1,
              "up is one of the four image axes");
        uint8_t src[w * h] = {0}, dst[w * h] = {0};
        const int cx = w / 2, cy = h / 2;
        src[(cy + (int)up[1]) * w + (cx + (int)up[0])] = 255;

        const ExifTransform xf = exifTransform(o);
        int dw = w, dh = h;
        spirula::oriented_size(xf.turns_cw, dw, dh);
        spirula::orient_pixels(src, w, h, 1, xf.turns_cw, xf.mirror, dst);
        const std::string tag = "orientation " + std::to_string(o);
        check(dst[(dh / 2 - 1) * dw + dw / 2] == 255,
              tag + ": the marker at `up` lands above the centre");
        check(dw == (xf.turns_cw & 1 ? h : w), tag + ": the turned size");
    }

}

static void check_turn_is_invertible() {
    const int w = 4, h = 3, n = w * h * 3;
    uint8_t a[n], b[n], un[n], c[n];
    for (int i = 0; i < n; i++) a[i] = (uint8_t)(i * 7 + 1);
    for (int t = 0; t < 4; t++) {
        for (int m = 0; m < 2; m++) {
            spirula::orient_pixels(a, w, h, 3, t, m != 0, b);
            int dw = w, dh = h;
            spirula::oriented_size(t, dw, dh);
            // Undo: mirror first (it was applied last), then turn back.
            spirula::orient_pixels(b, dw, dh, 3, 0, m != 0, un);
            spirula::orient_pixels(un, dw, dh, 3, (4 - t) & 3, false, c);
            check(std::memcmp(a, c, (size_t)n) == 0,
                  "turn " + std::to_string(t) + " mirror " + std::to_string(m) +
                      " is invertible");
        }
    }
}

// The gauge fix: an image whose tag says it was shot in portrait must level
// the model by the photographer's up, not by the frame's.
static void check_model_up() {
    // A portrait capture of a world whose up is +Z: the phone is turned, so the
    // frame's own up (camera -y) lies along +Y and the photographer's up (-x
    // for orientation 6) along +Z. World -> camera rows, right-handed.
    Reconstruction rec;
    for (uint32_t i = 0; i < 2; i++) {
        Image im;
        im.id = i;
        im.registered = true;
        im.name = "p" + std::to_string(i) + ".jpg";
        im.pose.R = Mat3{0, 0, -1, 0, -1, 0, -1, 0, 0};
        im.pose.t = Vec3{0, 0, (double)i};
        im.exif_orientation = 6;   // stored sideways, turn 90 CW to show
        rec.images[i] = im;
    }
    const Vec3 plain = meanCameraUp(rec, false);
    check(std::abs(plain.y - 2.0) < 1e-9 && std::abs(plain.z) < 1e-9,
          "the frame's own up is +Y, which is not up");
    const Vec3 exif = meanCameraUp(rec, true);
    check(std::abs(exif.z - 2.0) < 1e-9 && std::abs(exif.y) < 1e-9,
          "the tag's up is +Z, which is");

    // The tags a model read off disk does not carry, taken from the files.
    const std::string dir = "sfm_exif_test.tmp.d";
    std::filesystem::create_directories(dir);
    for (uint32_t i = 0; i < 2; i++)
        write_jpeg_with_exif(dir + "/p" + std::to_string(i) + ".jpg",
                             make_tiff(8, 400, 300, 24));
    std::vector<Reconstruction> bare{rec};
    for (auto& kv : bare[0].images) kv.second.exif_orientation = 1;
    check(fillExifOrientations(bare, dir) == 2, "the files are read");
    check(bare[0].images[0].exif_orientation == 8, "and their tags land on the model");
    // A model that already carries one is the authority; nothing is read.
    std::vector<Reconstruction> kept{rec};
    check(fillExifOrientations(kept, dir) == 0, "features beat the files");
    check(kept[0].images[0].exif_orientation == 6, "and are left alone");
    std::filesystem::remove_all(dir);
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdExifTest); }
