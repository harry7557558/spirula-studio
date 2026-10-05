// image_color_test -- ICC profiles in TIFFs, and the exposure the analysis
// copies of an image get. Self-contained: the profiles and TIFFs are built
// here, with colorants copied from real profiles (Lightroom Classic's
// "Rec. 2020", sRGB IEC61966-2.1, Adobe RGB (1998), Display P3).

#include "core/ColorSpace.h"
#include "core/IccProfile.h"
#include "core/ImageFile.h"
#include "core/TiffImage.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what);
    if (!ok) failures++;
}

void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) b.push_back((uint8_t)(v >> s));
}
void put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back((uint8_t)(v >> 8));
    b.push_back((uint8_t)v);
}
void puts15(std::vector<uint8_t>& b, double v) { put32(b, (uint32_t)(int32_t)std::lround(v * 65536.0)); }
void putsig(std::vector<uint8_t>& b, const char* s) { b.insert(b.end(), s, s + 4); }

std::vector<uint8_t> xyz_tag(const double v[3]) {
    std::vector<uint8_t> t;
    putsig(t, "XYZ ");
    put32(t, 0);
    for (int i = 0; i < 3; i++) puts15(t, v[i]);
    return t;
}

std::vector<uint8_t> para_tag(int fn, std::vector<double> params) {
    std::vector<uint8_t> t;
    putsig(t, "para");
    put32(t, 0);
    put16(t, (uint16_t)fn);
    put16(t, 0);
    for (double p : params) puts15(t, p);
    return t;
}

std::vector<uint8_t> curv_tag(const std::vector<uint16_t>& entries) {
    std::vector<uint8_t> t;
    putsig(t, "curv");
    put32(t, 0);
    put32(t, (uint32_t)entries.size());
    for (uint16_t e : entries) put16(t, e);
    while (t.size() % 4) t.push_back(0);
    return t;
}

// An RGB matrix/TRC profile with one curve for all three channels, or a grey
// one when `colorants` is null.
std::vector<uint8_t> profile(const double (*colorants)[3], const std::vector<uint8_t>& trc) {
    std::vector<std::pair<const char*, std::vector<uint8_t>>> tags;
    if (colorants) {
        tags = {{"rXYZ", xyz_tag(colorants[0])}, {"gXYZ", xyz_tag(colorants[1])},
                {"bXYZ", xyz_tag(colorants[2])}, {"rTRC", trc}, {"gTRC", trc},
                {"bTRC", trc}};
    } else {
        tags = {{"kTRC", trc}};
    }
    std::vector<uint8_t> h(128, 0);
    std::memcpy(&h[12], "mntr", 4);
    std::memcpy(&h[16], colorants ? "RGB " : "GRAY", 4);
    std::memcpy(&h[20], "XYZ ", 4);
    std::memcpy(&h[36], "acsp", 4);
    h[8] = 4;
    std::vector<uint8_t> table, data;
    put32(table, (uint32_t)tags.size());
    const size_t base = 128 + 4 + 12 * tags.size();
    for (auto& [name, bytes] : tags) {
        putsig(table, name);
        put32(table, (uint32_t)(base + data.size()));
        put32(table, (uint32_t)bytes.size());
        data.insert(data.end(), bytes.begin(), bytes.end());
    }
    std::vector<uint8_t> p = h;
    p.insert(p.end(), table.begin(), table.end());
    p.insert(p.end(), data.begin(), data.end());
    const uint32_t n = (uint32_t)p.size();
    for (int i = 0; i < 4; i++) p[(size_t)i] = (uint8_t)(n >> (24 - 8 * i));
    return p;
}

const double kRec2020[3][3] = {{0.67348, 0.27904, -0.00194},
                               {0.16568, 0.67535, 0.02998},
                               {0.12505, 0.04561, 0.79684}};
const double kSrgb[3][3] = {{0.4361, 0.2225, 0.0139},
                            {0.3851, 0.7169, 0.0971},
                            {0.1431, 0.0606, 0.7141}};
const double kAdobe[3][3] = {{0.6097, 0.3111, 0.0195},
                             {0.2053, 0.6257, 0.0609},
                             {0.1492, 0.0632, 0.7446}};
const double kDisplayP3[3][3] = {{0.5151, 0.2412, -0.0011},
                                 {0.2919, 0.6922, 0.0419},
                                 {0.1571, 0.0666, 0.7841}};

// Lightroom's own bytes: g 2.2222, a 0.9099, b 0.0901, c 0.2222, d 0.0812.
std::vector<uint8_t> bt709_trc() {
    std::vector<uint8_t> t;
    putsig(t, "para");
    put32(t, 0);
    put16(t, 3);
    put16(t, 0);
    for (uint32_t v : {0x000238e4u, 0x0000e8e0u, 0x00001720u, 0x000038e4u, 0x000014ccu})
        put32(t, v);
    return t;
}

// A little-endian, uncompressed, single-strip TIFF of 16-bit samples.
void write_tiff(const std::string& path, int w, int h, int spp,
                const std::vector<uint16_t>& px, const std::vector<uint8_t>& icc) {
    struct Entry { uint16_t tag, type; uint32_t count, value; };
    std::vector<Entry> e;
    const uint32_t pixels_at = 8;
    const uint32_t pixel_bytes = (uint32_t)(px.size() * 2);
    const uint32_t bits_at = pixels_at + pixel_bytes;
    const uint32_t icc_at = bits_at + 8;
    const uint32_t ifd_at = icc_at + (uint32_t)((icc.size() + 1) & ~size_t(1));
    e.push_back({256, 4, 1, (uint32_t)w});
    e.push_back({257, 4, 1, (uint32_t)h});
    e.push_back({258, 3, (uint32_t)spp, spp == 1 ? 16u : bits_at});
    e.push_back({259, 3, 1, 1});
    e.push_back({262, 3, 1, spp == 1 ? 1u : 2u});
    e.push_back({273, 4, 1, pixels_at});
    e.push_back({277, 3, 1, (uint32_t)spp});
    e.push_back({278, 4, 1, (uint32_t)h});
    e.push_back({279, 4, 1, pixel_bytes});
    if (!icc.empty()) e.push_back({34675, 7, (uint32_t)icc.size(), icc_at});

    std::vector<uint8_t> f = {'I', 'I', 42, 0};
    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) f.push_back((uint8_t)(v >> (8 * i))); };
    auto le16 = [&](uint16_t v) { f.push_back((uint8_t)v); f.push_back((uint8_t)(v >> 8)); };
    le32(ifd_at);
    for (uint16_t v : px) le16(v);
    for (int i = 0; i < 4; i++) le16(16);
    f.insert(f.end(), icc.begin(), icc.end());
    if (icc.size() % 2) f.push_back(0);
    le16((uint16_t)e.size());
    for (const Entry& x : e) {
        le16(x.tag);
        le16(x.type);
        le32(x.count);
        if (x.type == 3 && x.count == 1) { le16((uint16_t)x.value); le16(0); }
        else le32(x.value);
    }
    le32(0);
    std::ofstream(path, std::ios::binary).write((const char*)f.data(), (std::streamsize)f.size());
}

void icc_profiles() {
    icc::ColorSpace cs;
    std::vector<uint8_t> p = profile(kRec2020, bt709_trc());
    check(icc::read(p.data(), p.size(), cs) && cs.gamut == "Rec.2020" && cs.gamut_known &&
              !cs.is_linear,
          "icc: Lightroom Rec. 2020 is display-encoded Rec.2020");

    const std::vector<uint8_t> srgb_trc =
        para_tag(3, {2.4, 1 / 1.055, 0.055 / 1.055, 1 / 12.92, 0.04045});
    p = profile(kSrgb, srgb_trc);
    check(icc::read(p.data(), p.size(), cs) && cs.gamut.empty() && cs.gamut_known &&
              !cs.is_linear,
          "icc: sRGB is display-encoded Rec.709");

    p = profile(kAdobe, curv_tag({563}));
    check(icc::read(p.data(), p.size(), cs) && cs.gamut == "AdobeRGB" && !cs.is_linear,
          "icc: Adobe RGB (1998), gamma 2.2 as one curv entry");

    p = profile(kRec2020, curv_tag({}));
    check(icc::read(p.data(), p.size(), cs) && cs.gamut == "Rec.2020" && cs.is_linear,
          "icc: an empty curv is linear");

    p = profile(kSrgb, para_tag(0, {1.0}));
    check(icc::read(p.data(), p.size(), cs) && cs.is_linear, "icc: para gamma 1 is linear");

    std::vector<uint16_t> ramp(256);
    for (int i = 0; i < 256; i++) ramp[(size_t)i] = (uint16_t)std::lround(i * 65535.0 / 255.0);
    p = profile(kSrgb, curv_tag(ramp));
    check(icc::read(p.data(), p.size(), cs) && cs.is_linear, "icc: an identity table is linear");

    p = profile(kDisplayP3, srgb_trc);
    check(icc::read(p.data(), p.size(), cs) && !cs.gamut_known,
          "icc: Display P3 (D65 white) is not the theatrical DCI-P3");

    p = profile(nullptr, para_tag(0, {1.0}));
    check(icc::read(p.data(), p.size(), cs) && cs.grey && cs.is_linear, "icc: grey, linear");

    p = profile(kSrgb, srgb_trc);
    check(!icc::read(p.data(), 100, cs), "icc: truncated is refused");
    std::vector<uint8_t> lut = profile(nullptr, srgb_trc);
    std::memcpy(&lut[16], "RGB ", 4);
    check(!icc::read(lut.data(), lut.size(), cs), "icc: RGB without colorants is refused");
}

void exposure_maths() {
    colorspace::Exposure e;
    check(colorspace::parse_exposure("", e) && !e.active(), "exposure: empty is none");
    check(colorspace::parse_exposure("auto", e) && e.automatic, "exposure: auto");
    check(colorspace::parse_exposure("+1.5", e) && e.stops == 1.5f, "exposure: +1.5");
    check(colorspace::parse_exposure("-2", e) && e.stops == -2.0f, "exposure: -2");
    check(!colorspace::parse_exposure("bright", e) && !colorspace::parse_exposure("40", e) &&
              !colorspace::parse_exposure("2x", e),
          "exposure: nonsense is refused");

    colorspace::Exposure fixed;
    fixed.stops = 3;
    std::vector<float> none;
    check(colorspace::exposure_gain(fixed, none) == 8.0f, "exposure: 3 stops is 8x");

    colorspace::Exposure a;
    a.automatic = true;
    auto gain = [&](std::vector<float> luma) { return colorspace::exposure_gain(a, luma); };
    check(std::fabs(gain({0.005f, 0.01f, 0.02f, 0.5f, 0.001f}) - 10.0f) < 1e-4f,
          "exposure: auto lifts the median to 0.1");
    check(gain({0.2f, 0.5f, 0.9f}) == 1.0f, "exposure: auto never darkens an LDR image");
    check(std::fabs(gain({1.0f, 2.0f, 50.0f}) - 0.05f) < 1e-6f,
          "exposure: auto darkens an overexposed HDR image to the median");
    check(std::fabs(gain({0.3f, 0.5f, 4.0f}) - 0.25f) < 1e-6f,
          "exposure: but not past the peak reaching white");
    check(gain({0.0f, 0.0f, 0.0f}) == colorspace::kMaxExposureGain, "exposure: black is capped");

    const colorspace::Srgb8Encoder enc("", true, 4.0f);
    const float px[3] = {0.05f, 0.05f, 0.05f};
    uint8_t o[3];
    enc(px, o, 1, 3);
    check(o[0] == colorspace::quantize_srgb8(colorspace::srgb8_thresholds(), 0.2f),
          "encoder: gain applies in linear light");
    uint8_t c[3] = {50, 50, 50};
    colorspace::expose_srgb8_inplace(c, 3, 4.0f);
    const float lin = colorspace::srgb_to_linear(50.0f / 255.0f) * 4.0f;
    check(c[0] == colorspace::quantize_srgb8(colorspace::srgb8_thresholds(), lin),
          "8-bit: gain applies in linear light");
}

void tiff_files(const std::filesystem::path& dir) {
    const int w = 64, h = 48;
    std::vector<uint16_t> dark((size_t)w * h * 3, 655);   // 0.01
    dark[0] = 65535;                                      // one clipped sample
    const std::string lr = (dir / "lightroom_rec2020.tif").string();
    const std::string lin = (dir / "linear_rec2020.tif").string();
    const std::string bare = (dir / "no_profile.tif").string();
    const std::string grey = (dir / "grey_linear.tif").string();
    write_tiff(lr, w, h, 3, dark, profile(kRec2020, bt709_trc()));
    write_tiff(lin, w, h, 3, dark, profile(kRec2020, curv_tag({})));
    write_tiff(bare, w, h, 3, dark, {});
    write_tiff(grey, w, h, 1, std::vector<uint16_t>((size_t)w * h, 655),
               profile(nullptr, curv_tag({})));

    tiff::Info t;
    check(tiff::probe(lr, t).empty() && t.icc && t.gamut == "Rec.2020" && !t.is_linear,
          "tiff: the ICC tag is read");
    check(tiff::probe(bare, t).empty() && !t.icc && !tiff::declared_color_space(bare, t),
          "tiff: no profile declares nothing");
    check(tiff::probe(grey, t).empty() && t.icc && t.is_linear, "tiff: grey profile on grey");

    imagefile::DeclaredColor d;
    check(imagefile::declared_color_space(lin, d) && d.format == "TIFF" &&
              d.gamut == "Rec.2020" && d.is_linear,
          "imagefile: a TIFF's declaration");

    imagefile::Options opt;
    imagefile::Info info;
    std::vector<uint8_t> own, explicit_, plain;
    imagefile::decode_srgb8(lr, opt, info, own);
    imagefile::decode_srgb8(lr, opt, info, explicit_, "Rec.2020", false);
    check(own == explicit_, "decode: unset means the profile's colour space");
    check(info.peak == 1.0f && info.gain == 1.0f, "decode: peak 1.0 without exposure");

    opt.exposure.automatic = true;
    std::vector<uint8_t> lifted;
    imagefile::decode_srgb8(lin, opt, info, lifted, "", std::nullopt, &plain);
    const float want = 0.1f / (655.0f / 65535.0f);
    check(std::fabs(info.gain - want) < 1e-3f * want, "decode: auto meters the median");
    std::vector<uint8_t> unlit;
    imagefile::Options off;
    imagefile::decode_srgb8(lin, off, info, unlit);
    check(plain == unlit && lifted != unlit, "decode: the unexposed copy is the plain decode");
    check(lifted[100] == colorspace::quantize_srgb8(colorspace::srgb8_thresholds(), 0.1f) ||
              lifted[100] + 1 == colorspace::quantize_srgb8(colorspace::srgb8_thresholds(), 0.1f),
          "decode: the median lands on 0.1");

    opt.exposure = colorspace::Exposure();
    opt.exposure.stops = 1;
    plain.assign(5, 7);
    imagefile::decode_srgb8(lin, opt, info, lifted, "", std::nullopt, &plain);
    check(info.gain == 2.0f && !plain.empty(), "decode: one stop doubles");
    opt.exposure = colorspace::Exposure();
    imagefile::decode_srgb8(lin, opt, info, lifted, "", std::nullopt, &plain);
    check(plain.empty(), "decode: no exposure leaves the unexposed copy empty");
}

}  // namespace

int main() {
    icc_profiles();
    exposure_maths();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "spirula_image_color_test";
    std::filesystem::create_directories(dir);
    tiff_files(dir);
    std::filesystem::remove_all(dir);
    std::printf("\n%d failures\n", failures);
    return failures ? 1 : 0;
}
