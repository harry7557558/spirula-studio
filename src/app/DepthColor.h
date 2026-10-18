#pragma once

// The depth colour ramp the GUI's still-image panels draw with.
//
// It is the training viewport's, deliberately: a depth map read here and one
// read there have to be the same picture. The coefficients MUST match
// `COEFFS_turbo` in kernels/visualize/Visualizer.cu and in
// backend/vulkan/shaders/visualizer.slang, and so must the normalization --
// linear min to max over the image, not a percentile and not a log.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace app {

inline void turbo(float t, uint8_t out[3]) {
    static const float kCoeffs[3][8] = {
        { 0.14637796f, 2.94711014f, -10.15061040f, -90.46877071f,
          550.44382083f, -1061.31232675f, 874.85901369f, -266.03287948f },
        { 0.08594198f, 2.06520532f, 9.64581379f, -55.09180383f,
          149.40813531f, -245.26823636f, 202.98596195f, -63.82163439f },
        { 0.23431523f, 6.33792818f, 9.26380342f, -203.76964931f,
          604.07329733f, -766.82678386f, 447.99287080f, -97.24397473f },
    };
    for (int c = 0; c < 3; c++) {
        float v = 0.0f;
        for (int k = 7; k >= 0; --k) v = v * t + kCoeffs[c][k];
        out[c] = (uint8_t)std::lround(std::fmin(255.0f, std::fmax(0.0f, v * 255.0f)));
    }
}

// [n] depths -> [n*3] bytes. `skip_zero` leaves the trainer's "no ground
// truth here" sentinel black instead of colouring it and dragging the range
// down to it; the render's own depth has no such sentinel.
inline void depth_to_rgb(const float* depth, size_t n, bool skip_zero,
                         uint8_t* out) {
    float lo = 1e38f, hi = -1e38f;
    for (size_t i = 0; i < n; i++) {
        const float d = depth[i];
        if (!std::isfinite(d) || (skip_zero && d <= 0.0f)) continue;
        lo = std::fmin(lo, d);
        hi = std::fmax(hi, d);
    }
    const float span = hi > lo ? hi - lo : 1.0f;
    for (size_t i = 0; i < n; i++) {
        const float d = depth[i];
        if (!std::isfinite(d) || (skip_zero && d <= 0.0f)) {
            out[i * 3] = out[i * 3 + 1] = out[i * 3 + 2] = 0;
            continue;
        }
        turbo(std::fmin(1.0f, std::fmax(0.0f, (d - lo) / span)), out + i * 3);
    }
}

// The 99.5th percentile of the strictly-positive finite values (1 if there
// are none). EVERY error-like view on the turbo ramp scales by this, so the
// pictures stay comparable; a mean would sit far down these tails.
inline float error_scale(const float* v, size_t n) {
    float mx = 0.0f;
    for (size_t i = 0; i < n; i++)
        if (std::isfinite(v[i]) && v[i] > mx) mx = v[i];
    // 1024 linear bins: the ramp resolves 256 steps, so a finer quantile
    // than this buys nothing.
    constexpr int kBins = 1024;
    size_t hist[kBins] = {};
    size_t total = 0;
    if (mx > 0.0f) {
        for (size_t i = 0; i < n; i++) {
            const float x = v[i];
            if (!std::isfinite(x) || x <= 0.0f) continue;
            int b = (int)(x / mx * (float)(kBins - 1));
            hist[b < 0 ? 0 : (b >= kBins ? kBins - 1 : b)]++;
            total++;
        }
    }
    size_t acc = 0;
    int b = 0;
    for (; b + 1 < kBins; b++) {
        acc += hist[b];
        if ((double)acc >= 0.995 * (double)total) break;
    }
    const float hi = mx * (float)(b + 1) / (float)kBins;
    return hi > 0.0f ? hi : 1.0f;
}

// The 0.5th and 99.5th percentiles of the finite values: error_scale for
// values of either sign. Equal when the values are, both 0 when none is finite.
inline void value_range(const float* v, size_t n, float& lo, float& hi) {
    double mn = INFINITY, mx = -INFINITY;
    for (size_t i = 0; i < n; i++)
        if (std::isfinite(v[i])) {
            mn = std::min(mn, (double)v[i]);
            mx = std::max(mx, (double)v[i]);
        }
    lo = hi = std::isfinite(mn) ? (float)mn : 0.0f;
    if (!(mx > mn)) return;
    constexpr int kBins = 1024;
    size_t hist[kBins] = {};
    size_t total = 0;
    const double scale = (kBins - 1) / (mx - mn);
    for (size_t i = 0; i < n; i++)
        if (std::isfinite(v[i])) {
            hist[(int)((v[i] - mn) * scale)]++;
            total++;
        }
    const double cut = 0.005 * (double)total;
    size_t acc = 0;
    int b = 0;
    for (; b + 1 < kBins && (double)(acc + hist[b]) <= cut; b++) acc += hist[b];
    acc = 0;
    int t = kBins - 1;
    for (; t > b && (double)(acc + hist[t]) <= cut; t--) acc += hist[t];
    lo = (float)(mn + b / scale);
    hi = (float)std::min(mx, mn + (t + 1) / scale);
}

// [n] non-negative error values -> [n*3] bytes on the turbo ramp. `hi`
// saturates it; 0 asks for error_scale(). Returns the normalizer used.
inline float error_to_rgb(const float* err, size_t n, float hi, uint8_t* out) {
    if (hi <= 0.0f && n > 0) hi = error_scale(err, n);
    if (!(hi > 0.0f)) hi = 1.0f;
    for (size_t i = 0; i < n; i++) {
        const float v = err[i];
        turbo(std::isfinite(v) ? std::fmin(1.0f, std::fmax(0.0f, v / hi)) : 0.0f,
              out + i * 3);
    }
    return hi;
}

// [n*3] unit normals -> [n*3] bytes, the encoding `spirula geometry` writes
// and the loss decodes: byte/127.5 - 1, and BLACK where there is none.
inline void normal_to_rgb(const float* normal, size_t n, uint8_t* out) {
    for (size_t i = 0; i < n; i++) {
        const float* v = normal + i * 3;
        if (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < 0.25f) {
            out[i * 3] = out[i * 3 + 1] = out[i * 3 + 2] = 0;
            continue;
        }
        for (int c = 0; c < 3; c++)
            out[i * 3 + c] = (uint8_t)std::lround(
                std::fmin(255.0f, std::fmax(0.0f, 127.5f + 127.5f * v[c])));
    }
}

}  // namespace app
