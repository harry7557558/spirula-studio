#include "nn/core/Parallel.h"
#include "nn/io/Image.h"

#include <algorithm>
#include <cmath>

namespace nn {
namespace {

// One output coordinate's taps: PIL's precompute_coeffs for the bilinear
// filter, normalized so the weights sum to one.
struct Taps {
    std::vector<int>   first, count;
    std::vector<float> w;   // count.max() per output
    int                stride = 0;
};

Taps taps(int in, int out) {
    Taps t;
    const double scale = (double)in / out;
    const double support = std::max(scale, 1.0);   // bilinear's radius is 1
    t.stride = (int)std::ceil(support) * 2 + 1;
    t.first.resize((size_t)out);
    t.count.resize((size_t)out);
    t.w.assign((size_t)out * t.stride, 0.0f);
    const double ss = 1.0 / std::max(scale, 1.0);
    for (int o = 0; o < out; ++o) {
        const double center = (o + 0.5) * scale;
        const int x0 = std::max(0, (int)(center - support + 0.5));
        const int x1 = std::min(in, (int)(center + support + 0.5));
        double sum = 0.0;
        for (int x = x0; x < x1; ++x) {
            const double d = std::fabs((x - center + 0.5) * ss);
            const double v = d < 1.0 ? 1.0 - d : 0.0;
            t.w[(size_t)o * t.stride + (x - x0)] = (float)v;
            sum += v;
        }
        if (sum > 0)
            for (int x = x0; x < x1; ++x) t.w[(size_t)o * t.stride + (x - x0)] /= (float)sum;
        t.first[(size_t)o] = x0;
        t.count[(size_t)o] = x1 - x0;
    }
    return t;
}

}  // namespace

Image resize_image(const Image& src, int width, int height) {
    if (src.empty() || (src.width == width && src.height == height)) return src;
    const int C = src.channels;
    const Taps tx = taps(src.width, width), ty = taps(src.height, height);

    // Horizontal pass into float rows, then vertical.
    std::vector<float> mid((size_t)src.height * width * C);
    parallel_for(src.height, [&](int64_t lo, int64_t hi) {
        for (int64_t y = lo; y < hi; ++y) {
            const uint8_t* row = src.data.data() + (size_t)y * src.width * C;
            float* dst = mid.data() + (size_t)y * width * C;
            for (int x = 0; x < width; ++x) {
                const float* w = &tx.w[(size_t)x * tx.stride];
                for (int c = 0; c < C; ++c) {
                    float acc = 0.0f;
                    for (int k = 0; k < tx.count[(size_t)x]; ++k)
                        acc += w[k] * row[(size_t)(tx.first[(size_t)x] + k) * C + c];
                    dst[(size_t)x * C + c] = acc;
                }
            }
        }
    }, 8);

    Image out;
    out.width = width;
    out.height = height;
    out.channels = C;
    out.data.resize((size_t)width * height * C);
    parallel_for(height, [&](int64_t lo, int64_t hi) {
        for (int64_t y = lo; y < hi; ++y) {
            const float* w = &ty.w[(size_t)y * ty.stride];
            uint8_t* dst = out.data.data() + (size_t)y * width * C;
            for (int i = 0; i < width * C; ++i) {
                float acc = 0.0f;
                for (int k = 0; k < ty.count[(size_t)y]; ++k)
                    acc += w[k] * mid[(size_t)(ty.first[(size_t)y] + k) * width * C + i];
                dst[i] = (uint8_t)std::min(255.0f, std::max(0.0f, std::round(acc)));
            }
        }
    }, 8);
    return out;
}

}  // namespace nn
