// FrameSharpness.cpp -- see FrameSharpness.h.

#include "app/FrameSharpness.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace app {

void box_grey(const uint8_t* img, int W, int rows, int ow, int oh, float* out) {
    for (int y = 0; y < oh; y++) {
        int y0 = (int)((int64_t)y * rows / oh), y1 = (int)((int64_t)(y + 1) * rows / oh);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < ow; x++) {
            int x0 = (int)((int64_t)x * W / ow), x1 = (int)((int64_t)(x + 1) * W / ow);
            if (x1 <= x0) x1 = x0 + 1;
            double acc = 0.0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const uint8_t* p = img + ((size_t)yy * W + xx) * 3;
                    // BT.601 luma like cv2.cvtColor BGR2GRAY (RGB order here).
                    acc += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
                }
            out[(size_t)y * ow + x] = (float)(acc / ((y1 - y0) * (x1 - x0)));
        }
    }
}

double sharpness_score(const uint8_t* rgb, int width, int height) {
    constexpr int S = 512;
    std::vector<float> gray((size_t)S * S);
    box_grey(rgb, width, height, S, S, gray.data());
    double mean = 0.0;
    for (float v : gray) mean += v;
    mean /= (double)gray.size();
    for (float& v : gray) v -= (float)mean;
    double sum = 0.0, sum2 = 0.0;
    int64_t n = 0;
    for (int y = 1; y < S - 1; y++)
        for (int x = 1; x < S - 1; x++) {
            const float* r = &gray[(size_t)y * S + x];
            const double lap = (double)r[-S] + r[S] + r[-1] + r[1] - 4.0 * r[0];
            sum += lap;
            sum2 += lap * lap;
            n++;
        }
    const double mu = sum / n;
    return sum2 / n - mu * mu;
}

// ---------------------------------------------------------------------------
// Edge width
// ---------------------------------------------------------------------------

namespace {

// Half-size luma, smoothed: sensor noise makes false extrema an edge walk
// would stop at. 5 taps of a Gaussian of sigma 0.8.
void half_grey(const uint8_t* rgb, int W, int H, int w, int h, std::vector<float>& out) {
    std::vector<float> raw((size_t)w * h), row((size_t)w * h);
    for (int y = 0; y < h; y++) {
        const uint8_t* a = rgb + (size_t)(2 * y) * W * 3;
        const uint8_t* b = a + (size_t)W * 3;
        float* o = raw.data() + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            const uint8_t* p = a + (size_t)x * 6;
            const uint8_t* q = b + (size_t)x * 6;
            o[x] = 0.25f * (0.299f * (p[0] + p[3] + q[0] + q[3]) +
                            0.587f * (p[1] + p[4] + q[1] + q[4]) +
                            0.114f * (p[2] + p[5] + q[2] + q[5]));
        }
    }
    // The two-pixel rim is left as it came: no edge is walked from there.
    constexpr float k0 = 0.4992f, k1 = 0.2285f, k2 = 0.0219f;
    row = raw;
    for (int y = 0; y < h; y++) {
        const float* r = raw.data() + (size_t)y * w;
        float* o = row.data() + (size_t)y * w;
        for (int x = 2; x < w - 2; x++)
            o[x] = k0 * r[x] + k1 * (r[x - 1] + r[x + 1]) + k2 * (r[x - 2] + r[x + 2]);
    }
    out = row;
    for (int y = 2; y < h - 2; y++) {
        const float* r = row.data() + (size_t)y * w;
        float* o = out.data() + (size_t)y * w;
        for (int x = 0; x < w; x++)
            o[x] = k0 * r[x] + k1 * (r[x - w] + r[x + w]) + k2 * (r[x - 2 * w] + r[x + 2 * w]);
    }
}

// The widths of the sharpest of the strong edges across one axis of `g`
// (`along_x`: edges a row crosses), averaged; negative for too few.
float axis_width(const std::vector<float>& g, int w, int h, bool along_x) {
    const float* d = g.data();
    auto grad = [&](int x, int y, float& across, float& other) {
        const float* p = d + (size_t)y * w + x;
        const float gx = (p[1 - w] + 2 * p[1] + p[1 + w]) - (p[-1 - w] + 2 * p[-1] + p[-1 + w]);
        const float gy = (p[w - 1] + 2 * p[w] + p[w + 1]) - (p[-w - 1] + 2 * p[-w] + p[-w + 1]);
        across = along_x ? gx : gy;
        other = along_x ? gy : gx;
    };
    // The 99th percentile of the gradient, off every fourth pixel each way.
    std::vector<float> mag;
    mag.reserve((size_t)(w / 4 + 1) * (h / 4 + 1));
    for (int y = 1; y < h - 1; y += 4)
        for (int x = 1; x < w - 1; x += 4) {
            float a, o;
            grad(x, y, a, o);
            mag.push_back(std::fabs(a));
        }
    if (mag.size() < 100) return -1.0f;
    const size_t at = mag.size() * 99 / 100;
    std::nth_element(mag.begin(), mag.begin() + at, mag.end());
    const float strong = mag[at];

    // Edges that really run across this axis and stand clear of the noise, off
    // every other pixel each way: a quarter of them is still tens of thousands.
    std::vector<uint32_t> edge;
    for (int y = 3; y < h - 3; y += 2)
        for (int x = 3; x < w - 3; x += 2) {
            float a, o;
            grad(x, y, a, o);
            if (std::fabs(a) > strong && std::fabs(a) > 2.0f * std::fabs(o))
                edge.push_back((uint32_t)((size_t)y * w + x));
        }
    constexpr size_t kSample = 1500;
    constexpr int kReach = 60;
    constexpr float kStep = 12.0f;    // a real step, in 8-bit levels, not a ripple
    const size_t stride = std::max<size_t>(1, edge.size() / kSample);
    const int step = along_x ? 1 : w, len = along_x ? w : h;
    std::vector<float> widths;
    for (size_t e = 0; e < edge.size(); e += stride) {
        const int x = (int)(edge[e] % (uint32_t)w), y = (int)(edge[e] / (uint32_t)w);
        float a, o;
        grad(x, y, a, o);
        const bool up = a > 0.0f;
        const float* p = d + edge[e];
        const int pos = along_x ? x : y;
        int lo = 0, hi = 0;
        while (pos - lo > 0 && lo < kReach &&
               (up ? p[-(lo + 1) * step] < p[-lo * step] : p[-(lo + 1) * step] > p[-lo * step]))
            lo++;
        while (pos + hi < len - 1 && hi < kReach &&
               (up ? p[(hi + 1) * step] > p[hi * step] : p[(hi + 1) * step] < p[hi * step]))
            hi++;
        if (std::fabs(p[hi * step] - p[-lo * step]) > kStep) widths.push_back((float)(lo + hi));
    }
    if (widths.size() < 30) return -1.0f;
    std::sort(widths.begin(), widths.end());
    double sum = 0.0;
    // The sharpest fifth, less the very narrowest: blur widens every edge,
    // while a sharp frame of softly shaded things has wide edges among
    // narrow ones (the middle half ranked blinds close up as most blurred).
    const size_t a = widths.size() / 20, b = widths.size() / 4;
    for (size_t i = a; i < b; i++) sum += widths[i];
    return (float)(sum / (double)(b - a));
}

}  // namespace

float edge_width_px(const uint8_t* rgb, int width, int height) {
    const int w = width / 2, h = height / 2;
    if (!rgb || w < 64 || h < 64) return -1.0f;
    std::vector<float> g;
    half_grey(rgb, width, height, w, h, g);
    const float x = axis_width(g, w, h, true), y = axis_width(g, w, h, false);
    if (x < 0.0f || y < 0.0f) return -1.0f;
    return 2.0f * std::max(x, y);
}

}  // namespace app
