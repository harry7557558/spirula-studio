#include "marigold/model/HostMath.h"

#include "marigold/Common.h"

#include <cmath>

namespace marigold {
namespace {

// The midpoints between adjacent NF4 levels, bitsandbytes' dQuantizeNF4 tree
// flattened: a value's code is how many of these it exceeds.
const float kNf4Thresholds[15] = {
    -0.8480964004993439f, -0.6106329262256622f, -0.4599952697753906f,
    -0.33967943489551544f, -0.23460740596055984f, -0.13791173323988914f,
    -0.045525018125772476f, 0.03979014977812767f, 0.1202552504837513f,
    0.2035212516784668f, 0.2920137718319893f, 0.3893125355243683f,
    0.5016634166240692f, 0.6427869200706482f, 0.8614784181118011f};

const float kNf4Levels[16] = {
    -1.0f, -0.6961928009986877f, -0.5250730514526367f, -0.39491748809814453f,
    -0.28444138169288635f, -0.18477343022823334f, -0.09105003625154495f, 0.0f,
    0.07958029955625534f, 0.16093020141124725f, 0.24611230194568634f,
    0.33791524171829224f, 0.44070982933044434f, 0.5626170039176941f,
    0.7229568362236023f, 1.0f};

uint8_t nf4_code(float x) {
    uint8_t c = 0;
    // NaN (an all-zero block, 0 * inf) exceeds nothing: code 0, as on the GPU.
    for (float t : kNf4Thresholds) c += x > t;
    return c;
}

}  // namespace

void nf4_quantize(const uint16_t* bf16, int64_t n, uint8_t* codes, uint16_t* absmax) {
    parallel_for(n / 64, [&](int64_t b0, int64_t b1) {
        for (int64_t b = b0; b < b1; ++b) {
            const uint16_t* src = bf16 + b * 64;
            float am = 0.0f;
            float v[64];
            for (int i = 0; i < 64; ++i) {
                v[i] = bf16_to_f32(src[i]);
                am = std::fmax(am, std::fabs(v[i]));
            }
            absmax[b] = f32_to_bf16(am);
            const float rcp = 1.0f / am;
            for (int i = 0; i < 64; i += 2)
                codes[b * 32 + i / 2] =
                    (uint8_t)((nf4_code(v[i] * rcp) << 4) | nf4_code(v[i + 1] * rcp));
        }
    }, 256);
}

void nf4_dequantize(const uint8_t* codes, const uint16_t* absmax, int64_t n, float* out,
                    bool gemv) {
    float levels[16];
    for (int i = 0; i < 16; ++i) levels[i] = gemv ? bf16r(kNf4Levels[i]) : kNf4Levels[i];
    parallel_for(n / 64, [&](int64_t b0, int64_t b1) {
        for (int64_t b = b0; b < b1; ++b) {
            const float am = bf16_to_f32(absmax[b]);
            for (int i = 0; i < 32; ++i) {
                const uint8_t c = codes[b * 32 + i];
                out[b * 64 + 2 * i] = bf16r(levels[c >> 4] * am);
                out[b * 64 + 2 * i + 1] = bf16r(levels[c & 15] * am);
            }
        }
    }, 256);
}

std::vector<float> linear_bf16(const std::vector<float>& x, int64_t rows,
                               const std::vector<float>& w, int64_t N, int64_t K,
                               const float* bias, Rounding r) {
    std::vector<float> y((size_t)(rows * N));
    parallel_for(rows * N, [&](int64_t i0, int64_t i1) {
        for (int64_t i = i0; i < i1; ++i) {
            const int64_t m = i / N, n = i % N;
            const float* xr = &x[(size_t)(m * K)];
            const float* wr = &w[(size_t)(n * K)];
            double acc = 0.0;
            if (r == Rounding::Gemv4bit)
                for (int64_t k = 0; k < K; ++k) acc += bf16r(xr[k] * wr[k]);
            else
                for (int64_t k = 0; k < K; ++k) acc += (double)xr[k] * wr[k];
            const float b = bias ? bias[n] : 0.0f;
            y[(size_t)i] = r == Rounding::Gemv4bit ? bf16r(bf16r((float)acc) + bf16r(b))
                                                   : bf16r((float)(acc + b));
        }
    }, 64);
    return y;
}

}  // namespace marigold
