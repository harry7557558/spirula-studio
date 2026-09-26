// dlogm_osmo360 -- the D-Log M curve and the Osmo 360 matrix in core/DlogM.h,
// against the curve's own anchors (docs/notes/dlog-m.md). Each check names
// the mistake it exists to catch.
//
//   ./build/dlogm_osmo360

#include "core/ColorSpace.h"
#include "core/DlogM.h"

#include <cmath>
#include <cstdio>

using namespace colorspace;

static int g_failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

static bool near_rel(double got, double want, double rel) {
    return std::fabs(got - want) <= rel * std::fabs(want);
}

// Reference values: f32-rounded constants evaluated in double.
static void test_anchors() {
    check(std::fabs(dlogm_osmo360_to_linear(0.40f) - 0.18000) < 1e-5,
          "anchor: code 0.40 -> 0.18 (mid grey)");
    check(std::fabs(dlogm_osmo360_to_linear(0.714f) - 0.957746) < 1e-5,
          "anchor: code 0.714 -> 0.957746");
    check(std::fabs(dlogm_osmo360_to_linear(1.0f) - 3.764698) < 2e-5,
          "anchor: code 1.0 -> 3.76470");
    // Below the cut (code 0.1252): the toe branch, which grey cannot see.
    check(near_rel(dlogm_osmo360_to_linear(0.05f), 0.003216477, 1e-5),
          "toe: code 0.05 -> 0.003216477");
    check(near_rel(dlogm_osmo360_to_linear(0.10f), 0.007272475, 1e-5),
          "toe: code 0.10 -> 0.007272475");
    check(near_rel(dlogm_osmo360_to_linear(0.20f), 0.037113453, 1e-5),
          "shoulder: code 0.20 -> 0.037113453");
    check(near_rel(dlogm_osmo360_to_linear(0.60f), 0.541291018, 1e-5),
          "shoulder: code 0.60 -> 0.541291018");
    check(near_rel(dlogm_osmo360_to_linear(0.90f), 2.346620316, 1e-5),
          "shoulder: code 0.90 -> 2.346620316");
}

static void test_cut_and_shape() {
    const float cut_code =
        (std::log2(kDlogMCut - kDlogMXShift) - kDlogMYShift) / kDlogMScale;
    check(std::fabs(cut_code - 0.12516f) < 1e-4f, "cut: branches meet at code 0.1252");

    // The slope kinks at the cut by design; the VALUE must not jump. Swapped
    // slopes or a cut off the intersection jump by ~6e-3 there.
    const int n = 4097;
    bool increasing = true;
    double worst_d2 = 0.0;
    float prev = dlogm_osmo360_to_linear(0.0f), prev2 = prev;
    for (int i = 1; i < n; i++) {
        const float y = dlogm_osmo360_to_linear((float)i / (n - 1));
        if (!(y > prev)) increasing = false;
        if (i >= 2) worst_d2 = std::max(worst_d2, std::fabs((double)y - 2.0 * prev + prev2));
        prev2 = prev;
        prev = y;
    }
    check(increasing, "shape: strictly increasing over 4097 samples");
    check(worst_d2 < 1e-4, "shape: continuous at the cut (second difference < 1e-4)");
    check(dlogm_osmo360_to_linear(0.0f) >= -1e-6f, "floor: code 0 is not negative light");
}

static void test_round_trip() {
    float worst = 0.0f;
    for (int i = 0; i <= 1000; i++) {
        const float code = (float)i / 1000.0f;
        const float back = dlogm_osmo360_to_code(dlogm_osmo360_to_linear(code));
        worst = std::max(worst, std::fabs(back - code));
    }
    check(worst < 2e-5f, "round trip: code -> linear -> code within 2e-5");
}

static double det3(const Mat3& m) {
    return (double)m[0] * ((double)m[4] * m[8] - (double)m[5] * m[7]) -
           (double)m[1] * ((double)m[3] * m[8] - (double)m[5] * m[6]) +
           (double)m[2] * ((double)m[3] * m[7] - (double)m[4] * m[6]);
}

static void test_matrix() {
    const Mat3& m = kOsmo360ToRec2020;
    bool rows = true;
    for (int r = 0; r < 3; r++)
        rows &= std::fabs(m[r*3] + m[r*3+1] + m[r*3+2] - 1.0f) < 1e-6f;
    check(rows, "matrix: rows sum to 1 (a transpose does not)");
    // Pocket 3's matrix has det 0.9465; swapping two entries in a row keeps
    // the row sum and moves the determinant.
    check(std::fabs(det3(m) - 0.873348) < 1e-5, "matrix: determinant 0.873348");
    float red[3] = {1.0f, 0.0f, 0.0f};
    apply3x3(m, red);
    check(std::fabs(red[0] - 0.807269f) < 1e-6f && std::fabs(red[1] - 0.042878f) < 1e-6f &&
              std::fabs(red[2] + 0.009604f) < 1e-6f,
          "matrix: native red -> first column (row-major)");
}

// Through spirula's own Rec.2020 -> Rec.709, as the trainer applies it.
static void test_end_to_end() {
    const Mat3 to709 = gamut_to_rec709("Rec.2020");
    float white[3] = {1.0f, 1.0f, 1.0f};
    apply3x3(kOsmo360ToRec2020, white);
    apply3x3(to709, white);
    check(std::fabs(white[0] - 1) < 1e-6f && std::fabs(white[1] - 1) < 1e-6f &&
              std::fabs(white[2] - 1) < 1e-6f,
          "chain: native white -> Rec.709 white");

    float grey[3] = {0.4f, 0.4f, 0.4f};
    dlogm_osmo360_to_rec2020(grey);
    apply3x3(to709, grey);
    check(std::fabs(grey[0] - 0.18f) < 1e-6f && std::fabs(grey[1] - 0.18f) < 1e-6f &&
              std::fabs(grey[2] - 0.18f) < 1e-6f,
          "chain: code 0.4 grey -> linear 0.18 grey");

    // Grey is blind to any white-preserving matrix; a saturated code is not.
    float sat[3] = {0.6f, 0.3f, 0.2f};
    dlogm_osmo360_to_rec2020(sat);
    check(std::fabs(sat[0] - 0.452542f) < 1e-5f && std::fabs(sat[1] - 0.113386f) < 1e-5f &&
              std::fabs(sat[2] - 0.027071f) < 1e-5f,
          "chain: code (0.6,0.3,0.2) -> Rec.2020 (0.4525,0.1134,0.0271)");
    apply3x3(to709, sat);
    check(std::fabs(sat[0] - 0.682839f) < 1e-5f && std::fabs(sat[1] - 0.071865f) < 1e-5f &&
              std::fabs(sat[2] - 0.010667f) < 1e-5f,
          "chain: code (0.6,0.3,0.2) -> Rec.709 (0.6828,0.0719,0.0107)");
}

static void test_names() {
    check(input_curve_or("", InputCurve::None) == InputCurve::None &&
              input_curve_or("none", InputCurve::DlogMOsmo360) == InputCurve::DlogMOsmo360,
          "names: unset and `none` take the caller's fallback");
    check(input_curve_or("dlogm-osmo360", InputCurve::None) == InputCurve::DlogMOsmo360,
          "names: dlogm-osmo360 is the D-Log M curve");
    check(input_curve_or("off", InputCurve::DlogMOsmo360) == InputCurve::None,
          "names: `off` is explicitly no curve");
    bool threw = false;
    try { input_curve_or("dlog", InputCurve::None); } catch (...) { threw = true; }
    check(threw, "names: an unknown curve is refused");
}

int main() {
    test_anchors();
    test_cut_and_shape();
    test_round_trip();
    test_matrix();
    test_end_to_end();
    test_names();
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
