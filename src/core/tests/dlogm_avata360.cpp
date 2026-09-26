// dlogm_avata360 -- the Avata 360 D-Log M curve and matrix in core/DlogM.h,
// against the fit's own anchors (docs/notes/dlog-m.md). Each check names the
// mistake it exists to catch.
//
//   ./build/dlogm_avata360

#include "core/ColorSpace.h"
#include "core/DlogM.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace colorspace;

static int g_failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%-64s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

static bool near_rel(double got, double want, double rel) {
    return std::fabs(got - want) <= rel * std::fabs(want);
}

static float lin(float code) { return dlogm_to_linear(kDlogMAvata360, code); }

// Reference values: f32-rounded constants evaluated in double. The Osmo curve
// gives 0.957746 at 0.714 and 3.764698 at 1.0, so reusing it fails every
// anchor above grey.
static void test_anchors() {
    check(std::fabs(lin(0.40f) - 0.18) < 1e-5, "anchor: code 0.40 -> 0.18 (mid grey)");
    check(near_rel(lin(0.714f), 0.602113034, 1e-5), "anchor: code 0.714 -> 0.602113");
    check(near_rel(lin(1.0f), 1.41523257, 1e-5), "anchor: code 1.0 -> 1.415233");
    check(near_rel(lin(0.05f), 0.00574413853, 1e-5), "toe: code 0.05 -> 0.005744139");
    check(near_rel(lin(0.10f), 0.0122567679, 1e-5), "toe: code 0.10 -> 0.012256768");
    check(near_rel(lin(0.20f), 0.0411330486, 1e-5), "shoulder: code 0.20 -> 0.041133049");
    check(near_rel(lin(0.60f), 0.409469333, 1e-5), "shoulder: code 0.60 -> 0.409469333");
    check(near_rel(lin(0.90f), 1.06281076, 1e-5), "shoulder: code 0.90 -> 1.062810760");
}

static void test_cut_and_shape() {
    const DlogMCurve& k = kDlogMAvata360;
    const float cut = k.intercept / (k.slope2 - k.slope);
    const float cut_code = (std::log2(cut - k.x_shift) - k.y_shift) / k.scale;
    check(std::fabs(cut_code - 0.161364f) < 1e-4f, "cut: branches meet at code 0.1614");
    // Same bound as OpenOSV's Osmo fit: the slope may kink
    // by at most 3x at the cut.
    check(k.slope2 / k.slope <= 3.0f * (1.0f + 1e-6f), "shape: slope2 / slope <= 3");

    const int n = 4097;
    bool increasing = true;
    double worst_d2 = 0.0;
    float prev = lin(0.0f), prev2 = prev;
    for (int i = 1; i < n; i++) {
        const float y = lin((float)i / (n - 1));
        if (!(y > prev)) increasing = false;
        if (i >= 2) worst_d2 = std::max(worst_d2, std::fabs((double)y - 2.0 * prev + prev2));
        prev2 = prev;
        prev = y;
    }
    check(increasing, "shape: strictly increasing over 4097 samples");
    check(worst_d2 < 1e-4, "shape: continuous at the cut (second difference < 1e-4)");
    check(lin(0.0f) >= -1e-6f, "floor: code 0 is not negative light");

    float worst = 0.0f;
    for (int i = 0; i <= 1000; i++) {
        const float code = (float)i / 1000.0f;
        worst = std::max(worst, std::fabs(dlogm_to_code(kDlogMAvata360, lin(code)) - code));
    }
    check(worst < 2e-5f, "round trip: code -> linear -> code within 2e-5");
}

static double det3(const Mat3& m) {
    return (double)m[0] * ((double)m[4] * m[8] - (double)m[5] * m[7]) -
           (double)m[1] * ((double)m[3] * m[8] - (double)m[5] * m[6]) +
           (double)m[2] * ((double)m[3] * m[7] - (double)m[4] * m[6]);
}

static void test_matrix_and_chain() {
    const Mat3& m = kAvata360ToRec2020;
    bool rows = true;
    for (int r = 0; r < 3; r++)
        rows &= std::fabs(m[r*3] + m[r*3+1] + m[r*3+2] - 1.0f) < 1e-6f;
    check(rows, "matrix: rows sum to 1 (a transpose does not)");
    // The Osmo matrix has det 0.873348.
    check(std::fabs(det3(m) - 1.075248) < 1e-5, "matrix: determinant 1.075248");

    const Mat3 to709 = gamut_to_rec709("Rec.2020");
    float white[3] = {1.0f, 1.0f, 1.0f};
    apply3x3(m, white);
    apply3x3(to709, white);
    check(std::fabs(white[0] - 1) < 1e-6f && std::fabs(white[1] - 1) < 1e-6f &&
              std::fabs(white[2] - 1) < 1e-6f,
          "chain: native white -> Rec.709 white");

    float grey[3] = {0.4f, 0.4f, 0.4f};
    dlogm_avata360_to_rec2020(grey);
    apply3x3(to709, grey);
    check(std::fabs(grey[0] - 0.18f) < 1e-5f && std::fabs(grey[1] - 0.18f) < 1e-5f &&
              std::fabs(grey[2] - 0.18f) < 1e-5f,
          "chain: code 0.4 grey -> linear 0.18 grey");

    // Grey is blind to any white-preserving matrix; a saturated code is not.
    float sat[3] = {0.6f, 0.3f, 0.2f};
    dlogm_avata360_to_rec2020(sat);
    check(std::fabs(sat[0] - 0.312097f) < 1e-5f && std::fabs(sat[1] - 0.041293f) < 1e-5f &&
              std::fabs(sat[2] + 0.068165f) < 1e-5f,
          "chain: code (0.6,0.3,0.2) -> Rec.2020 (0.3121,0.0413,-0.0682)");
}

static void test_names_and_dispatch() {
    check(input_curve_or("dlogm-avata360", InputCurve::None) == InputCurve::DlogMAvata360,
          "names: dlogm-avata360 is the Avata curve");
    check(std::string(input_curve_name(InputCurve::DlogMAvata360)) == "dlogm-avata360" &&
              std::string(input_curve_name(InputCurve::DlogMOsmo360)) == "dlogm-osmo360",
          "names: each curve prints the name it parses from");

    float a[3] = {0.6f, 0.3f, 0.2f}, want[3] = {0.6f, 0.3f, 0.2f};
    input_curve_to_rec2020(InputCurve::DlogMAvata360, a);
    dlogm_avata360_to_rec2020(want);
    check(a[0] == want[0] && a[1] == want[1] && a[2] == want[2],
          "dispatch: the Avata curve decodes with the Avata constants");
    float o[3] = {0.6f, 0.3f, 0.2f}, osmo[3] = {0.6f, 0.3f, 0.2f};
    input_curve_to_rec2020(InputCurve::DlogMOsmo360, o);
    dlogm_osmo360_to_rec2020(osmo);
    check(o[0] == osmo[0] && o[1] == osmo[1] && o[2] == osmo[2],
          "dispatch: the Osmo curve still decodes with the Osmo constants");
    float n[3] = {0.6f, 0.3f, 0.2f};
    input_curve_to_rec2020(InputCurve::None, n);
    check(n[0] == 0.6f && n[1] == 0.3f && n[2] == 0.2f, "dispatch: no curve leaves the value alone");
}

int main() {
    test_anchors();
    test_cut_and_shape();
    test_matrix_and_chain();
    test_names_and_dispatch();
    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
