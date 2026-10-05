// FrameSharpness.cpp -- see FrameSharpness.h.

#include "app/FrameSharpness.h"

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

}  // namespace app
