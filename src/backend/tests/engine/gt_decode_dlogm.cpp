// Each D-Log M ground-truth decode on the device, against the host mirror the
// mean-luma features read (_engine_color_space_gt_pixel) and the host curve
// (core/DlogM.h). A decode never dispatched, run after the display encode, or
// with the matrix multiplied from the wrong side trains without erroring.
// Self-checking, either backend:
//
//   ./gt_decode_dlogm

#include <core/ColorSpace.h>
#include <core/DlogM.h>
#include <engine/Engine.h>
#include <engine/EngineInternal.h>
#include <engine/EngineState.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using backend::MemcpyKind;

static int g_failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

static TorchTensorView ttv(const void* p, uint32_t elem, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, elem, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

// Decodes the codes on the device under `curve` and holds the result to
// `host` (the curve's own core/DlogM.h entry point) and to the mirror.
static void run_curve(colorspace::InputCurve curve, void (*host)(float[3]), float want_one,
                      const char* name) {
    // Grey at the anchors, both sides of the cut, then saturated triples: grey
    // cannot tell mul(M, v) from mul(v, M), a saturated code can.
    const float codes[][3] = {
        {0.0f, 0.0f, 0.0f},    {0.05f, 0.05f, 0.05f}, {0.10f, 0.10f, 0.10f},
        {0.40f, 0.40f, 0.40f}, {0.714f, 0.714f, 0.714f}, {1.0f, 1.0f, 1.0f},
        {0.6f, 0.3f, 0.2f},    {0.2f, 0.6f, 0.3f},    {0.3f, 0.2f, 0.6f},
        {0.9f, 0.15f, 0.5f},
    };
    const int W = 10, H = 1, C = 1;
    std::vector<uint16_t> gt((size_t)W * 3);
    for (int i = 0; i < W; i++)
        for (int k = 0; k < 3; k++)
            gt[(size_t)i * 3 + k] = (uint16_t)std::lround(codes[i][k] * 65535.0f);

    const colorspace::Mat3 m = colorspace::gamut_to_rec709("Rec.2020");
    engine_init_color_space(false, 0, false, {}, true,
                            (int)colorspace::Transfer::Srgb, true,
                            std::vector<float>(m.begin(), m.end()));
    engine_init_image_decode((int)curve);
    set_training_data(ttv(gt.data(), 2, {C, H, W, 3}), ttv_null(), ttv_null(),
                      ttv_null(), true);
    backend::device_synchronize();

    std::vector<float> got((size_t)W * 3);
    backend::memcpy_sync(got.data(), engine().gt.rgb.data_ptr(), got.size() * sizeof(float),
                         MemcpyKind::DeviceToHost);
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        std::exit(1);
    }

    // What the device must match: the curve, its matrix, Rec.2020->709, then
    // the open sRGB encode -- written out here, not via the mirror.
    double worst_ref = 0.0, worst_mirror = 0.0;
    for (int i = 0; i < W; i++) {
        float want[3], mirror[3];
        for (int k = 0; k < 3; k++) want[k] = mirror[k] = gt[(size_t)i * 3 + k] / 65535.0f;
        host(want);
        colorspace::apply3x3(m, want);
        for (int k = 0; k < 3; k++)
            want[k] = colorspace::tone_encode(want[k], colorspace::Transfer::Srgb);
        _engine_color_space_gt_pixel(mirror);
        for (int k = 0; k < 3; k++) {
            const float g = got[(size_t)i * 3 + k];
            worst_ref = std::max(worst_ref, (double)std::fabs(g - want[k]));
            worst_mirror = std::max(worst_mirror, (double)std::fabs(g - mirror[k]));
        }
    }
    std::printf("%s: max |device - reference| = %.3g, |device - mirror| = %.3g\n", name,
                worst_ref, worst_mirror);
    char what[96];
    std::snprintf(what, sizeof(what), "%s device: decode -> Rec.709 -> sRGB matches the host", name);
    check(worst_ref < 1e-5, what);
    std::snprintf(what, sizeof(what), "%s mirror: the mean-luma host copy matches the device", name);
    check(worst_mirror < 1e-5, what);
    // Code 0.4 is mid grey on both curves: 0.18 linear, 0.4614 once sRGB-encoded.
    std::snprintf(what, sizeof(what), "%s device: code 0.4 grey -> display 0.4614", name);
    check(std::fabs(got[3 * 3] - 0.461356f) < 1e-4f, what);
    // The open sRGB encode lets decoded highlights through above 1; the two
    // curves part here (Osmo 1.7779, Avata 1.1643).
    std::snprintf(what, sizeof(what), "%s device: code 1.0 -> display %.4f", name, want_one);
    check(std::fabs(got[5 * 3] - want_one) < 1e-4f, what);
    engine_reset();
}

int main() {
    run_curve(colorspace::InputCurve::DlogMOsmo360, colorspace::dlogm_osmo360_to_rec2020,
              1.777905f, "osmo");
    run_curve(colorspace::InputCurve::DlogMAvata360, colorspace::dlogm_avata360_to_rec2020,
              1.164263f, "avata");

    const colorspace::Mat3 m = colorspace::gamut_to_rec709("Rec.2020");
    engine_init_color_space(false, 0, false, {}, true,
                            (int)colorspace::Transfer::Srgb, true,
                            std::vector<float>(m.begin(), m.end()));
    engine_init_image_decode((int)colorspace::InputCurve::DlogMAvata360);
    engine_reset();
    check(engine().color_space.image_curve == 0, "reset: the decode does not outlive the run");

    // The decode hands the conversion linear Rec.2020; a display-encoded image
    // side would push it through the sRGB EOTF a second time.
    engine_init_color_space(false, 0, false, {}, true, 0, false,
                            std::vector<float>(m.begin(), m.end()));
    bool refused = false;
    try {
        engine_init_image_decode((int)colorspace::InputCurve::DlogMOsmo360);
    } catch (const std::exception&) { refused = true; }
    check(refused && engine().color_space.image_curve == 0,
          "guard: the decode is refused on a display-encoded image side");
    engine_reset();
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
