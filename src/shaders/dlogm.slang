// DJI D-Log M, per camera: code value -> scene-linear Rec.2020 (grey 0.18).
// Device copy of core/DlogM.h; the constants must match it, and the
// gt_decode_dlogm engine test compares the two. See docs/notes/dlog-m.md.
// Based on https://github.com/Kemerd/OpenOSV/blob/3a39776272efb5dfdc1d29711ae746e855383084/include/osv/color/DlogM.h
// and https://github.com/Kemerd/OpenOSV/blob/3a39776272efb5dfdc1d29711ae746e855383084/include/osv/color/Matrices.h
// SPDX-FileCopyrightText: Copyright 2026 The OpenOSV Contributors
// SPDX-License-Identifier: Apache-2.0

static const float kDlogMXShift    = -2.360862594f;
static const float kDlogMYShift    = 0.630835854f;
static const float kDlogMScale     = 6.691455736f;
static const float kDlogMSlope     = 1.011886004f;
static const float kDlogMSlope2    = 3.035658045f;
static const float kDlogMIntercept = 0.822056039f;
static const float kDlogMMidGray   = 0.00786506109f;

float dlogm_curve_to_linear(float code, float x_shift, float y_shift, float scale,
                            float slope, float slope2, float intercept, float mid_gray) {
    float cut = intercept / (slope2 - slope);
    float t = exp2(scale * code + y_shift) + x_shift;
    float pw = t < cut ? t * slope + intercept : t * slope2;
    return pw * mid_gray;
}

float dlogm_osmo360_to_linear(float code) {
    return dlogm_curve_to_linear(code, kDlogMXShift, kDlogMYShift, kDlogMScale, kDlogMSlope,
                                 kDlogMSlope2, kDlogMIntercept, kDlogMMidGray);
}

// Avata 360: our fit, not DJI's (core/DlogM.h, docs/notes/dlog-m.md).
float dlogm_avata360_to_linear(float code) {
    return dlogm_curve_to_linear(code, -2.064248825f, 0.630835854f, 3.622988623f,
                                 1.011886004f, 3.035658012f, 0.521917605f, 0.027401822f);
}

// Row-major, as mul(M, v) reads it: linear camera-native -> linear Rec.2020.
[CudaDeviceExport]
float3 dlogm_osmo360_to_rec2020(float3 code) {
    float3x3 m = float3x3(
        0.807268560f, 0.152663648f, 0.040067792f,
        0.042878162f, 0.990737677f, -0.033615828f,
        -0.009603872f, -0.094263740f, 1.103867650f);
    float3 lin = float3(dlogm_osmo360_to_linear(code.x),
                        dlogm_osmo360_to_linear(code.y),
                        dlogm_osmo360_to_linear(code.z));
    return mul(m, lin);
}

[CudaDeviceExport]
float3 dlogm_avata360_to_rec2020(float3 code) {
    float3x3 m = float3x3(
        0.706524789f, 0.176516764f, 0.116958447f,
        -0.183691555f, 1.116191272f, 0.067500283f,
        -0.302259748f, 0.033496898f, 1.268762850f);
    float3 lin = float3(dlogm_avata360_to_linear(code.x),
                        dlogm_avata360_to_linear(code.y),
                        dlogm_avata360_to_linear(code.z));
    return mul(m, lin);
}

// curve is colorspace::InputCurve: 1 Osmo 360, 2 Avata 360.
[CudaDeviceExport]
float3 input_curve_to_rec2020(int curve, float3 code) {
    return curve == 2 ? dlogm_avata360_to_rec2020(code) : dlogm_osmo360_to_rec2020(code);
}
