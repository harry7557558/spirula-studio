#pragma once

// DJI D-Log M: code value -> scene-linear Rec.2020 (grey 0.18), per camera.
// Host copy; shaders/dlogm.slang is the device copy and must hold the same
// constants (gt_decode_dlogm compares the two). See docs/notes/dlog-m.md.
// Based on https://github.com/Kemerd/OpenOSV/blob/3a39776272efb5dfdc1d29711ae746e855383084/include/osv/color/DlogM.h
// and https://github.com/Kemerd/OpenOSV/blob/3a39776272efb5dfdc1d29711ae746e855383084/include/osv/color/Matrices.h
// SPDX-FileCopyrightText: Copyright 2026 The OpenOSV Contributors
// SPDX-License-Identifier: Apache-2.0

#include "core/ColorSpace.h"

#include <cmath>

namespace colorspace {

// lin = mid_gray * (t < cut ? t*slope + intercept : t*slope2),
// t = 2^(scale*code + y_shift) + x_shift, cut = intercept / (slope2 - slope).
struct DlogMCurve {
    float x_shift, y_shift, scale, slope, slope2, intercept, mid_gray;
};

// kDlogMOsmo360: a fit to the neutral axis of DJI's Osmo 360 D-Log M LUT.
inline constexpr float kDlogMXShift    = -2.360862594f;
inline constexpr float kDlogMYShift    = 0.630835854f;
inline constexpr float kDlogMScale     = 6.691455736f;
inline constexpr float kDlogMSlope     = 1.011886004f;
inline constexpr float kDlogMSlope2    = 3.035658045f;
inline constexpr float kDlogMIntercept = 0.822056039f;
inline constexpr float kDlogMMidGray   = 0.00786506109f;
// Where the two linear branches meet, in exp2 space: reached at code 0.1252.
inline constexpr float kDlogMCut = kDlogMIntercept / (kDlogMSlope2 - kDlogMSlope);
inline constexpr DlogMCurve kDlogMOsmo360 = {kDlogMXShift, kDlogMYShift, kDlogMScale,
                                             kDlogMSlope, kDlogMSlope2, kDlogMIntercept,
                                             kDlogMMidGray};

// kNativeToRec2020_Osmo360, row-major, linear camera-native -> linear Rec.2020.
// Rows sum to 1, so white stays white.
inline constexpr Mat3 kOsmo360ToRec2020 = {
    0.807268560f, 0.152663648f, 0.040067792f,
    0.042878162f, 0.990737677f, -0.033615828f,
    -0.009603872f, -0.094263740f, 1.103867650f};

// Avata 360: our fit to DJI Studio's D-Log M export of paired footage through
// Rec.709 and the BT.709 OETF; DJI publishes no Avata LUT. Cut at code 0.1614.
inline constexpr DlogMCurve kDlogMAvata360 = {-2.064248825f, 0.630835854f, 3.622988623f,
                                              1.011886004f, 3.035658012f, 0.521917605f,
                                              0.027401822f};
inline constexpr Mat3 kAvata360ToRec2020 = {
    0.706524789f, 0.176516764f, 0.116958447f,
    -0.183691555f, 1.116191272f, 0.067500283f,
    -0.302259748f, 0.033496898f, 1.268762850f};

// Not clamped: codes above 1 keep extrapolating, as the camera's do.
inline float dlogm_to_linear(const DlogMCurve& k, float code) {
    const float cut = k.intercept / (k.slope2 - k.slope);
    const float t = std::exp2(k.scale * code + k.y_shift) + k.x_shift;
    const float pw = t < cut ? t * k.slope + k.intercept : t * k.slope2;
    return pw * k.mid_gray;
}

// The exact inverse; the log argument is floored so it never sees <= 0.
inline float dlogm_to_code(const DlogMCurve& k, float lin) {
    const float pw = lin / k.mid_gray;
    float t = (pw - k.intercept) / k.slope;
    if (!(t < k.intercept / (k.slope2 - k.slope))) t = pw / k.slope2;
    return (std::log2(std::max(t - k.x_shift, 1e-30f)) - k.y_shift) / k.scale;
}

inline float dlogm_osmo360_to_linear(float code) { return dlogm_to_linear(kDlogMOsmo360, code); }
inline float dlogm_osmo360_to_code(float lin) { return dlogm_to_code(kDlogMOsmo360, lin); }

inline void dlogm_osmo360_to_rec2020(float v[3]) {
    for (int c = 0; c < 3; c++) v[c] = dlogm_osmo360_to_linear(v[c]);
    apply3x3(kOsmo360ToRec2020, v);
}

inline void dlogm_avata360_to_rec2020(float v[3]) {
    for (int c = 0; c < 3; c++) v[c] = dlogm_to_linear(kDlogMAvata360, v[c]);
    apply3x3(kAvata360ToRec2020, v);
}

// Leaves v alone for InputCurve::None.
inline void input_curve_to_rec2020(InputCurve curve, float v[3]) {
    if (curve == InputCurve::DlogMOsmo360) dlogm_osmo360_to_rec2020(v);
    else if (curve == InputCurve::DlogMAvata360) dlogm_avata360_to_rec2020(v);
}

}  // namespace colorspace
