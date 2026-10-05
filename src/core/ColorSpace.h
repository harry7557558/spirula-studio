#pragma once

// Gamut matrices and the sRGB transfer, shared by everything that has to move
// pixels between a capture colour space and sRGB: the trainer's seed colours,
// the SfM front end, and the AI models. Header-only so `src/sfm/` and `src/nn/`
// can use it without depending on the app layer.
//
// Matrices are row-major 3x3, source primaries -> Rec.709, chromatically
// adapted (white maps to white).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace colorspace {

using Mat3 = std::array<float, 9>;

inline constexpr const char* kGamuts[] = {
    "Rec.709", "ACES2065-1", "ACEScg", "Rec.2020", "AdobeRGB", "DCI-P3",
};

// The same gamuts as CIE xy: Rx Ry Gx Gy Bx By Wx Wy. A file's primaries match
// one within 0.002 on all eight, the white included: "DCI-P3" is the theatrical
// white, and passing P3-D65 off as it would be a visible green shift.
struct GamutPrimaries {
    const char* name;
    float xy[8];
};

inline constexpr GamutPrimaries kGamutPrimaries[] = {
    {"Rec.709",    {0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f, 0.3127f, 0.3290f}},
    {"ACES2065-1", {0.7347f, 0.2653f, 0.0f, 1.0f, 0.0001f, -0.0770f, 0.32168f, 0.33767f}},
    {"ACEScg",     {0.713f, 0.293f, 0.165f, 0.830f, 0.128f, 0.044f, 0.32168f, 0.33767f}},
    {"Rec.2020",   {0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f}},
    {"AdobeRGB",   {0.640f, 0.330f, 0.210f, 0.710f, 0.150f, 0.060f, 0.3127f, 0.3290f}},
    {"DCI-P3",     {0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.314f, 0.351f}},
};

// "" and "none" are Rec.709, i.e. the identity.
inline Mat3 gamut_to_rec709(const std::string& name) {
    if (name.empty() || name == "none" || name == "Rec.709")
        return {1,0,0, 0,1,0, 0,0,1};
    if (name == "ACES2065-1") return {
        2.5247180476f, -1.1325619434f, -0.3921561044f,
        -0.2776344819f, 1.3709123773f, -0.0932778953f,
        -0.0165202369f, -0.1479259606f, 1.1644461975f};
    if (name == "ACEScg") return {
        1.7072552160f, -0.6200352595f, -0.0872199564f,
        -0.1311566587f, 1.1391010566f, -0.0079443978f,
        -0.0245499075f, -0.1248045805f, 1.1493544880f};
    if (name == "Rec.2020") return {
        1.6604910021f, -0.5876411388f, -0.0728498633f,
        -0.1245504745f, 1.1328998971f, -0.0083494226f,
        -0.0181507634f, -0.1005788980f, 1.1187296614f};
    if (name == "AdobeRGB") return {
        1.3983671735f, -0.3983451225f, 0.0000054016f,
        -0.0000103176f, 0.9999916496f, -0.0000039459f,
        -0.0000003709f, -0.0429269510f, 1.0429319656f};
    if (name == "DCI-P3") return {
        1.1548337042f, -0.1451763523f, -0.0096573518f,
        -0.0393300117f, 1.0378282998f, 0.0015017119f,
        -0.0184786235f, -0.0689101110f, 1.0873887345f};
    throw std::runtime_error("unsupported color gamut: " + name);
}

inline Mat3 invert3x3(const Mat3& m) {
    const float a = m[0], b = m[1], c = m[2],
                d = m[3], e = m[4], g = m[5],
                h = m[6], i = m[7], j = m[8];
    const float det = a*(e*j - g*i) - b*(d*j - g*h) + c*(d*i - e*h);
    if (std::abs(det) < 1e-20f) throw std::runtime_error("singular color matrix");
    const float s = 1.0f / det;
    return {(e*j - g*i)*s, (c*i - b*j)*s, (b*g - c*e)*s,
            (g*h - d*j)*s, (a*j - c*h)*s, (c*d - a*g)*s,
            (d*i - e*h)*s, (b*h - a*i)*s, (a*e - b*d)*s};
}

// 0.04045 is the branch point (= 12.92 * 0.0031308); 0.055 is the offset.
inline float srgb_to_linear(float x) {
    return x < 0.04045f ? x * (1.0f / 12.92f)
                        : std::pow((x + 0.055f) * (1.0f / 1.055f), 2.4f);
}

inline float linear_to_srgb(float x) {
    return x < 0.0031308f ? 12.92f * x
                          : 1.055f * std::pow(std::max(x, 0.0f), 1.0f / 2.4f) - 0.055f;
}

// Output transfer: the curve from linear light to a display code value, and
// orthogonal to whether the buffer is stored linear (the caller's is_linear).
// Mirrors the kXfer* block in shaders/pixel_wise.slang; the two must agree.
enum class Transfer : int {
    Srgb = 0, SrgbClamped = 1, Aces = 2, Filmic = 3, Uncharted2 = 4,
};

inline constexpr const char* kTransfers[] = {
    "srgb", "srgb-clamped", "aces", "filmic", "uncharted2",
};
inline constexpr int kNumTransfers = 5;

// "" and "none" mean "not declared" -- the config's spelling for unset, which
// the GUI and the CLI can both write. The caller supplies what that infers to.
inline Transfer transfer_or(const std::string& name, Transfer fallback) {
    if (name.empty() || name == "none") return fallback;
    for (int i = 0; i < kNumTransfers; i++)
        if (name == kTransfers[i]) return (Transfer)i;
    throw std::runtime_error("unsupported color transfer: " + name);
}

inline const char* transfer_name(Transfer t) { return kTransfers[(int)t]; }

inline constexpr float kTransferWhite = 11.2f;

inline float tone_aces(float x) {
    return (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
}

inline float tone_hable(float x) {
    return (x * (0.15f * x + 0.05f) + 0.004f)
         / (x * (0.15f * x + 0.50f) + 0.06f) - 0.02f / 0.30f;
}

inline float tone_uncharted2(float x) {
    return tone_hable(std::max(x, 0.0f)) / tone_hable(kTransferWhite);
}

inline float tone_filmic(float x) {
    const float t = std::max(x - 0.004f, 0.0f);
    return (t * (6.2f * t + 0.5f)) / (t * (6.2f * t + 1.7f) + 0.06f);
}

// Positive root of a x^2 + b x + c, the branch every curve inverse takes.
inline float tone_quad_root(float a, float b, float c) {
    return (-b + std::sqrt(std::max(b * b - 4.0f * a * c, 0.0f))) / (2.0f * a);
}

// Scene-linear (Rec.709) -> display code value.
inline float tone_encode(float x, Transfer t) {
    switch (t) {
    case Transfer::Filmic:      return tone_filmic(x);
    case Transfer::Aces:
        return linear_to_srgb(std::min(tone_aces(std::max(x, 0.0f)), 1.0f));
    case Transfer::Uncharted2:
        return linear_to_srgb(std::min(tone_uncharted2(x), 1.0f));
    case Transfer::SrgbClamped:
        return linear_to_srgb(std::min(std::max(x, 0.0f), 1.0f));
    default:                    return linear_to_srgb(std::max(x, 0.0f));
    }
}

inline float tone_decode(float d, Transfer t) {
    switch (t) {
    case Transfer::Filmic: {
        // The curve only reaches 1.0 at infinity; cap the decode at the white
        // point so display white lands on a finite scene value.
        const float y = std::min(d, tone_filmic(kTransferWhite));
        return tone_quad_root(6.2f * (1.0f - y), 0.5f - 1.7f * y, -0.06f * y)
             + 0.004f;
    }
    case Transfer::Aces: {
        const float y = srgb_to_linear(d);
        return tone_quad_root(2.51f - 2.43f * y, 0.03f - 0.59f * y, -0.14f * y);
    }
    case Transfer::Uncharted2: {
        const float r = srgb_to_linear(d) * tone_hable(kTransferWhite) + 0.02f / 0.30f;
        return tone_quad_root(0.15f * (1.0f - r), 0.50f * (0.10f - r),
                              0.20f * (0.02f - r * 0.30f));
    }
    default:               return srgb_to_linear(d);
    }
}

// One channel of a working-space value -> its display code value, and back.
// The gamut matrix is the caller's; these are the transfer alone.
inline float working_to_display(float w, Transfer t, bool is_linear) {
    return tone_encode(is_linear ? w : srgb_to_linear(w), t);
}

inline float display_to_working(float d, Transfer t, bool is_linear) {
    const float lin = tone_decode(d, t);
    return is_linear ? lin : linear_to_srgb(lin);
}

inline void apply3x3(const Mat3& m, float v[3]) {
    const float t0 = v[0], t1 = v[1], t2 = v[2];
    for (int r = 0; r < 3; r++)
        v[r] = m[r*3+0]*t0 + m[r*3+1]*t1 + m[r*3+2]*t2;
}

// True when (gamut, is_linear) is already plain sRGB, so callers can skip the
// per-pixel work entirely.
inline bool is_identity(const std::string& gamut, bool is_linear) {
    return !is_linear && (gamut.empty() || gamut == "none" || gamut == "Rec.709");
}

// Interleaved 8-bit RGB, in place. `n` is the pixel count.
//
// Values outside the destination gamut clamp -- these buffers feed feature
// extraction and 8-bit point colours, both of which are 0..255 by definition.
inline void to_srgb_inplace(uint8_t* rgb, size_t n,
                            const std::string& gamut, bool is_linear) {
    if (is_identity(gamut, is_linear)) return;
    const Mat3 m = gamut_to_rec709(gamut);
    for (size_t i = 0; i < n; i++) {
        float v[3];
        for (int c = 0; c < 3; c++) {
            const float x = rgb[3*i + c] * (1.0f / 255.0f);
            v[c] = is_linear ? x : srgb_to_linear(x);
        }
        apply3x3(m, v);
        for (int c = 0; c < 3; c++) {
            const float y = linear_to_srgb(v[c]) * 255.0f + 0.5f;
            rgb[3*i + c] = (uint8_t)(y < 0.0f ? 0.0f : (y > 255.0f ? 255.0f : y));
        }
    }
}

// Exact 8-bit quantization of linear_to_srgb: thresh[c] is the linear value at
// which the code steps to c+1, so the search cannot disagree with the curve.
inline const float* srgb8_thresholds() {
    static const std::vector<float> t = [] {
        std::vector<float> v(255);
        for (int c = 0; c < 255; c++)
            v[(size_t)c] = srgb_to_linear((c + 0.5f) / 255.0f);
        return v;
    }();
    return t.data();
}

inline uint8_t quantize_srgb8(const float* thresh, float x) {
    int lo = 0, hi = 255;
    while (lo < hi) {
        const int m = (lo + hi + 1) >> 1;
        if (x >= thresh[m - 1]) lo = m;
        else                    hi = m - 1;
    }
    return (uint8_t)lo;
}

inline uint8_t quantize_unit8(float x) {
    return (uint8_t)std::lround(std::min(std::max(x, 0.0f), 1.0f) * 255.0f);
}

inline float luma709(const float v[3]) {
    return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
}

// Float pixels in (gamut, is_linear) times `gain` to 8-bit sRGB, `nc` = 1, 3
// or 4 per pixel. Every gamut maps white to white, so one channel takes only
// the transfer; a fourth is alpha.
struct Srgb8Encoder {
    Mat3 m;
    bool linear;
    bool identity;
    float gain;
    const float* thresh;

    Srgb8Encoder(const std::string& gamut, bool is_linear, float gain = 1.0f)
        : m(gamut_to_rec709(gamut)), linear(is_linear),
          identity(is_identity(gamut, is_linear) && gain == 1.0f), gain(gain),
          thresh(srgb8_thresholds()) {}

    // Linear Rec.709 light before the gain, which is what auto exposure meters.
    void to_linear(const float* px, int nc, float v[3]) const {
        if (nc == 1) {
            v[0] = v[1] = v[2] = linear ? px[0] : srgb_to_linear(px[0]);
            return;
        }
        for (int c = 0; c < 3; c++) v[c] = linear ? px[c] : srgb_to_linear(px[c]);
        apply3x3(m, v);
    }

    void operator()(const float* px, uint8_t* o, size_t n, int nc) const {
        for (size_t i = 0; i < n; i++, px += nc, o += nc) {
            if (nc == 1) {
                o[0] = linear || gain != 1.0f
                           ? quantize_srgb8(thresh, (linear ? px[0] : srgb_to_linear(px[0])) * gain)
                           : quantize_unit8(px[0]);
                continue;
            }
            if (identity) {
                for (int c = 0; c < 3; c++) o[c] = quantize_unit8(px[c]);
            } else {
                float v[3];
                to_linear(px, nc, v);
                for (int c = 0; c < 3; c++) o[c] = quantize_srgb8(thresh, v[c] * gain);
            }
            if (nc == 4) o[3] = quantize_unit8(px[3]);
        }
    }
};

// ================
// Exposure for analysis
// ================

// A gain in linear light for what the detectors and the AI models look at;
// training pixels never pass through it. docs/notes/analysis-exposure.md.
struct Exposure {
    bool automatic = false;
    float stops = 0.0f;
    bool active() const { return automatic || stops != 0.0f; }
};

// "auto", a number of stops ("2", "+1.5", "-1"), or "" / "0" for none.
inline bool parse_exposure(const std::string& s, Exposure& out) {
    out = Exposure();
    if (s.empty() || s == "none" || s == "off") return true;
    if (s == "auto") {
        out.automatic = true;
        return true;
    }
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v) || std::fabs(v) > 16.0f)
        return false;
    out.stops = v;
    return true;
}

// Camera JPEGs of a 25-frame Fujifilm capture (issue #127) have median linear
// luma 0.11-0.21, so auto lifts a darker image to 0.1 and leaves those alone.
inline constexpr float kAutoExposureMedian = 0.1f;
inline constexpr float kMaxExposureGain = 1024.0f;   // 10 stops; beyond is noise

// Pixels apart, in x and in y, that keep an auto-exposure meter near 64k samples.
inline size_t exposure_sample_step(size_t w, size_t h) {
    return std::max<size_t>(1, (size_t)std::sqrt((double)w * (double)h / 65536.0));
}

// Auto never darkens past the point where the brightest sample reaches white,
// so an image with nothing above 1.0 is only ever brightened. `luma` (linear
// Rec.709) is reordered.
inline float exposure_gain(const Exposure& e, std::vector<float>& luma) {
    if (!e.automatic) return std::exp2(e.stops);
    if (luma.empty()) return 1.0f;
    const auto mid = luma.begin() + (std::ptrdiff_t)(luma.size() / 2);
    std::nth_element(luma.begin(), mid, luma.end());
    const float peak = *std::max_element(luma.begin(), luma.end());
    const float lo = peak > 1.0f ? 1.0f / peak : 1.0f;
    const float g = *mid > 0.0f ? kAutoExposureMedian / *mid : kMaxExposureGain;
    return std::min(std::max(g, lo), kMaxExposureGain);
}

// The same for 8-bit sRGB pixels, which is all an stb_image format leaves.
inline float exposure_gain_srgb8(const Exposure& e, const uint8_t* rgb, size_t w, size_t h) {
    if (!e.automatic) return std::exp2(e.stops);
    float lin[256];
    for (int c = 0; c < 256; c++) lin[c] = srgb_to_linear(c * (1.0f / 255.0f));
    const size_t step = exposure_sample_step(w, h);
    std::vector<float> luma;
    luma.reserve((w / step + 1) * (h / step + 1));
    for (size_t y = 0; y < h; y += step)
        for (size_t x = 0; x < w; x += step) {
            const uint8_t* p = rgb + 3 * (y * w + x);
            const float v[3] = {lin[p[0]], lin[p[1]], lin[p[2]]};
            luma.push_back(luma709(v));
        }
    return exposure_gain(e, luma);
}

// Interleaved 8-bit sRGB times `gain` in linear light, in place.
inline void expose_srgb8_inplace(uint8_t* px, size_t n_values, float gain) {
    if (gain == 1.0f) return;
    const float* thresh = srgb8_thresholds();
    uint8_t lut[256];
    for (int c = 0; c < 256; c++)
        lut[c] = quantize_srgb8(thresh, srgb_to_linear(c * (1.0f / 255.0f)) * gain);
    for (size_t i = 0; i < n_values; i++) px[i] = lut[px[i]];
}

// The inverse of to_srgb_inplace.
inline void from_srgb_inplace(uint8_t* rgb, size_t n,
                              const std::string& gamut, bool is_linear) {
    if (is_identity(gamut, is_linear)) return;
    const Mat3 m = invert3x3(gamut_to_rec709(gamut));
    for (size_t i = 0; i < n; i++) {
        float v[3];
        for (int c = 0; c < 3; c++)
            v[c] = srgb_to_linear(rgb[3*i + c] * (1.0f / 255.0f));
        apply3x3(m, v);
        for (int c = 0; c < 3; c++) {
            const float y = (is_linear ? v[c] : linear_to_srgb(v[c])) * 255.0f + 0.5f;
            rgb[3*i + c] = (uint8_t)(y < 0.0f ? 0.0f : (y > 255.0f ? 255.0f : y));
        }
    }
}

}  // namespace colorspace
