// color_resolution -- what resolve_color() makes of `--image-color-log` and
// `--point-color-log`, and what the seed colours become under it. A wrong
// answer here trains without erroring: grey read as linear seeds 1.15 stops
// bright, and a skipped linear flag decodes the log image through sRGB too.

#include "app/TrainerCore.h"
#include "core/DlogM.h"

#include <cmath>
#include <cstdio>
#include <array>
#include <string>

using namespace spirula;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

bool throws(const TrainConfig& c) {
    try { resolve_color(c); } catch (const std::exception&) { return true; }
    return false;
}

TrainConfig dlogm() {
    TrainConfig c;
    c.image_color_log = "dlogm-osmo360";
    return c;
}

// First seed's display-independent colour, back out of the SH DC.
std::array<float, 3> seed_color(const TrainConfig& c, uint8_t r, uint8_t g, uint8_t b) {
    ColmapPoints3D pts;
    for (int i = 0; i < 8; i++) {
        pts.xyz.insert(pts.xyz.end(), {(double)i, (double)(i % 3), (double)(i % 2)});
        pts.rgb.insert(pts.rgb.end(), {r, g, b});
    }
    // One point a different colour, or seed_splats randomizes a uniform cloud.
    pts.rgb[21] = (uint8_t)(pts.rgb[21] ^ 1);
    TrainConfig cc = c;
    cc.cap_max = 8;
    const SeedSplats s = seed_splats(pts, cc, resolve_color(cc));
    std::array<float, 3> out{};
    for (int d = 0; d < 3; d++) out[d] = s.features_dc[d] * 0.28209479177387814f + 0.5f;
    return out;
}

void test_image_side() {
    const ColorResolution r = resolve_color(dlogm());
    check(r.image_curve == colorspace::InputCurve::DlogMOsmo360,
          "image: dlogm-osmo360 selects the D-Log M decode");
    check(r.image_linear, "image: the decode's output is linear, so the image side is");
    check(r.image_gamut == "Rec.2020", "image: an unset gamut resolves to Rec.2020");
    check(r.image_on(), "image: the GT conversion pass runs");
    check(r.splat_linear && r.splat_gamut == "Rec.2020",
          "splat: follows the decoded image side (linear Rec.2020)");

    TrainConfig c = dlogm();
    c.image_color_is_linear = false;
    c.image_color_gamut = "Rec.2020";
    check(throws(c), "image: dlogm with display-encoded Rec.2020 is refused");
    c = dlogm();
    c.image_color_is_linear = true;
    c.image_color_gamut = "Rec.709";
    check(throws(c), "image: dlogm with linear Rec.709 is refused");
    c = dlogm();
    c.image_color_transfer = "aces";
    const ColorResolution t = resolve_color(c);
    check(t.image_transfer == colorspace::Transfer::Aces &&
              t.splat_transfer == colorspace::Transfer::Aces,
          "image: a log curve leaves the output transfer alone");
    c = dlogm();
    c.image_color_gamut = "ACEScg";
    check(throws(c), "image: dlogm with a gamut other than Rec.2020 is refused");
    c = dlogm();
    c.image_color_gamut = "Rec.2020";
    c.image_color_is_linear = true;
    check(!throws(c), "image: dlogm with Rec.2020 / linear spelled out is accepted");

    TrainConfig none;
    none.image_color_log = "none";
    const ColorResolution n = resolve_color(none);
    check(n.image_curve == colorspace::InputCurve::None && !n.image_on() &&
              n.image_gamut.empty() && !n.image_linear,
          "image: `none` means no decode and changes nothing else");
}

void test_point_side() {
    const ColorResolution r = resolve_color(dlogm());
    check(r.point_curve == colorspace::InputCurve::DlogMOsmo360,
          "point: unset follows the images (SfM sampled the log frames)");
    check(!r.point_is_splat(), "point: a log seed is never already in splat space");

    TrainConfig c = dlogm();
    c.point_color_log = "off";
    const ColorResolution o = resolve_color(c);
    check(o.point_curve == colorspace::InputCurve::None,
          "point: `off` turns the seed decode off");

    check(o.point_gamut.empty() && !o.point_linear,
          "point: `off` reads the cloud as sRGB, not the decoded image side");

    c = dlogm();
    c.point_color_is_linear = false;
    c.point_color_gamut = "Rec.2020";
    check(throws(c), "point: a log seed stated display-encoded Rec.2020 is refused");
}

// The hdr preset states plain sRGB input; the log flag has to win over that,
// not trip on a flag the user never passed.
void test_hdr_preset() {
    TrainConfig c;
    const bool known = train_apply_preset(c, "hdr");
    c.image_color_log = "dlogm-osmo360";
    bool ok = known;
    ColorResolution r;
    try { r = resolve_color(c); } catch (const std::exception&) { ok = false; }
    check(ok, "hdr preset: composes with --image-color-log");
    check(ok && r.image_linear && r.image_gamut == "Rec.2020" &&
              r.splat_gamut == "ACEScg" && r.splat_linear,
          "hdr preset: images decode to Rec.2020, splats stay the preset's ACEScg");
}

// What a colour is on screen, from a splat-space value.
float display_of(const ColorResolution& r, std::array<float, 3> col, int k) {
    float v[3] = {col[0], col[1], col[2]};
    for (int d = 0; d < 3; d++) if (!r.splat_linear) v[d] = colorspace::srgb_to_linear(v[d]);
    colorspace::apply3x3(colorspace::gamut_to_rec709(r.splat_gamut), v);
    return colorspace::tone_encode(v[k], r.splat_transfer);
}

void test_compare_source() {
    const ColorResolution r = resolve_color(dlogm());
    float raw[3] = {0.4f, 0.4f, 0.4f};
    source_pixel_for_compare(r, true, raw);
    check(std::fabs(raw[0] - 0.18f) < 1e-5f && std::fabs(raw[2] - 0.18f) < 1e-5f,
          "compare: a log source is decoded into the raw splat space");
    TrainConfig c = dlogm();
    c.splat_color_gamut = "Rec.709";
    c.splat_color_is_linear = false;
    float disp[3] = {0.4f, 0.4f, 0.4f};
    source_pixel_for_compare(resolve_color(c), false, disp);
    check(std::fabs(disp[1] - 0.461356f) < 1e-4f,
          "compare: a log source is decoded to display values");
    float plain[3] = {0.4f, 0.3f, 0.2f};
    source_pixel_for_compare(resolve_color(TrainConfig{}), true, plain);
    check(plain[0] == 0.4f && plain[1] == 0.3f && plain[2] == 0.2f,
          "compare: without a log curve the file is shown as stored");
}

void test_seeds() {
    // 102/255 is code 0.4 exactly: D-Log M mid grey.
    const auto grey = seed_color(dlogm(), 102, 102, 102);
    check(std::fabs(grey[0] - 0.18f) < 1e-4f && std::fabs(grey[1] - 0.18f) < 1e-4f &&
              std::fabs(grey[2] - 0.18f) < 1e-4f,
          "seed: code 0.4 grey lands at linear 0.18 in the splats");

    float want[3] = {153 / 255.0f, 51 / 255.0f, 102 / 255.0f};
    colorspace::dlogm_osmo360_to_rec2020(want);
    const auto sat = seed_color(dlogm(), 153, 51, 102);
    check(std::fabs(sat[0] - want[0]) < 1e-4f && std::fabs(sat[1] - want[1]) < 1e-4f &&
              std::fabs(sat[2] - want[2]) < 1e-4f,
          "seed: a saturated code goes through the Osmo matrix");

    // `off` on a D-Log M run must show an sRGB cloud exactly as a plain run
    // would, even though the two runs store splats in different spaces.
    TrainConfig c = dlogm();
    c.point_color_log = "off";
    const TrainConfig plain;
    const auto off = seed_color(c, 153, 51, 102);
    const auto ref = seed_color(plain, 153, 51, 102);
    bool same = true;
    for (int k = 0; k < 3; k++)
        same &= std::fabs(display_of(resolve_color(c), off, k) -
                          display_of(resolve_color(plain), ref, k)) < 1e-4f;
    check(same, "seed: `off` shows an sRGB cloud as a run without the flag does");
}

// --image-color-log-exposure: a gain in linear after the decode, on the
// images and the seeds alike.
void test_exposure() {
    TrainConfig zero = dlogm();
    zero.image_color_log_exposure = 0.0f;
    const auto g0 = seed_color(zero, 102, 102, 102);
    check(resolve_color(zero).image_gain == 1.0f && std::fabs(g0[1] - 0.18f) < 1e-4f,
          "exposure: 0 leaves the decode unchanged");

    TrainConfig one = dlogm();
    one.image_color_log_exposure = 1.0f;
    const ColorResolution r1 = resolve_color(one);
    check(r1.image_gain == 2.0f, "exposure: +1 doubles the image side");
    const auto g1 = seed_color(one, 102, 102, 102);
    check(std::fabs(g1[0] - 2.0f * g0[0]) < 1e-5f && std::fabs(g1[2] - 2.0f * g0[2]) < 1e-5f,
          "exposure: +1 doubles the seeds' linear colour");

    TrainConfig part = dlogm();
    part.image_color_log_exposure = 0.45f;
    check(std::fabs(resolve_color(part).image_gain - std::exp2(0.45f)) < 1e-6f &&
              std::fabs(resolve_color(part).point_gain - std::exp2(0.45f)) < 1e-6f,
          "exposure: +0.45 is 2^0.45 on both sides, not 1 + 0.45");

    float raw[3] = {0.4f, 0.4f, 0.4f};
    source_pixel_for_compare(r1, true, raw);
    check(std::fabs(raw[1] - 0.36f) < 1e-5f, "exposure: the compare source gets the gain");

    TrainConfig plain;
    plain.image_color_log = "none";
    plain.image_color_log_exposure = 1.0f;
    const ColorResolution rp = resolve_color(plain);
    check(rp.image_gain == 1.0f && rp.point_gain == 1.0f,
          "exposure: no log curve, no gain");

    TrainConfig off = one;
    off.point_color_log = "off";
    check(resolve_color(off).point_gain == 1.0f && resolve_color(off).image_gain == 2.0f,
          "exposure: an sRGB seed cloud is not brightened");
}

// The Avata curve through every host site that decodes: resolve, compare,
// seeds. Each would silently fall back to the Osmo constants if it tested
// for DlogMOsmo360 by name.
void test_avata() {
    TrainConfig av;
    av.image_color_log = "dlogm-avata360";
    const ColorResolution r = resolve_color(av);
    check(r.image_curve == colorspace::InputCurve::DlogMAvata360 &&
              r.point_curve == colorspace::InputCurve::DlogMAvata360,
          "avata: dlogm-avata360 selects the Avata decode, seeds follow");
    check(r.image_linear && r.image_gamut == "Rec.2020",
          "avata: the Avata decode pins linear Rec.2020 too");
    av.image_color_gamut = "ACEScg";
    std::string why;
    try { resolve_color(av); } catch (const std::exception& e) { why = e.what(); }
    check(why.find("dlogm-avata360") != std::string::npos,
          "avata: a contradicting gamut is refused naming the Avata flag");

    float want[3] = {0.6f, 0.3f, 0.2f}, osmo[3] = {0.6f, 0.3f, 0.2f};
    colorspace::dlogm_avata360_to_rec2020(want);
    colorspace::dlogm_osmo360_to_rec2020(osmo);
    float raw[3] = {0.6f, 0.3f, 0.2f};
    source_pixel_for_compare(r, true, raw);
    check(std::fabs(raw[0] - want[0]) < 1e-5f && std::fabs(raw[1] - want[1]) < 1e-5f &&
              std::fabs(raw[2] - want[2]) < 1e-5f && std::fabs(raw[0] - osmo[0]) > 0.05f,
          "avata: the compare source decodes with the Avata constants");

    TrainConfig av2;
    av2.image_color_log = "dlogm-avata360";
    float seed_want[3] = {140 / 255.0f, 110 / 255.0f, 100 / 255.0f};
    colorspace::dlogm_avata360_to_rec2020(seed_want);
    const auto sat = seed_color(av2, 140, 110, 100);
    check(std::fabs(sat[0] - seed_want[0]) < 1e-4f && std::fabs(sat[1] - seed_want[1]) < 1e-4f &&
              std::fabs(sat[2] - seed_want[2]) < 1e-4f,
          "avata: a coloured seed goes through the Avata curve and matrix");
}

}  // namespace

int main() {
    test_avata();
    test_exposure();
    test_image_side();
    test_point_side();
    test_seeds();
    test_hdr_preset();
    test_compare_source();
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
