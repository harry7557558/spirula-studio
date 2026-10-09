// mask_weight_test -- split_mask_weight and scale_by_mask_weight against a host
// reference, under whichever backend this build has. Exit code 0 on a match.

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using backend::MemcpyKind;

namespace {

TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T), MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
std::vector<T> download(const T* d, size_t n) {
    std::vector<T> h(n);
    backend::memcpy_sync(h.data(), d, n * sizeof(T), MemcpyKind::DeviceToHost);
    return h;
}

int check_scale(int B, int H, int W, int C, int Hm, int Wm, std::mt19937& rng) {
    std::vector<uint8_t> w((size_t)B * Hm * Wm);
    for (auto& v : w) v = (uint8_t)(rng() & 0xff);
    std::vector<float> x((size_t)B * H * W * C);
    for (auto& v : x) v = (float)(rng() % 2001) / 1000.0f - 1.0f;
    float* d_x = upload(x);
    uint8_t* d_w = upload(w);
    std::vector<int64_t> shape = {B, H, W};
    if (C > 1) shape.push_back(C);
    scale_by_mask_weight(ttv(d_x, shape), ttv(d_w, {B, Hm, Wm, 1}));
    const std::vector<float> got = download(d_x, x.size());
    int bad = 0;
    for (int b = 0; b < B; ++b)
        for (int y = 0; y < H; ++y)
            for (int xx = 0; xx < W; ++xx) {
                int ys = y, xs = xx;
                if (Hm != H || Wm != W) {
                    const float u = ((float)xx + 0.5f) * (float)Wm / (float)W - 0.5f;
                    const float v = ((float)y + 0.5f) * (float)Hm / (float)H - 0.5f;
                    xs = std::min(Wm - 1, std::max(0, (int)std::floor(u + 0.5f)));
                    ys = std::min(Hm - 1, std::max(0, (int)std::floor(v + 0.5f)));
                }
                const float s = (float)w[((size_t)b * Hm + ys) * Wm + xs] / 255.0f;
                for (int c = 0; c < C; ++c) {
                    const size_t i = (((size_t)b * H + y) * W + xx) * C + c;
                    if (std::fabs(got[i] - x[i] * s) > 1e-6f) ++bad;
                }
            }
    std::printf("scale %dx%dx%dx%d by %dx%d: %d mismatches\n", B, H, W, C, Hm, Wm, bad);
    return bad;
}

}  // namespace

int main() {
    std::mt19937 rng(7u);
    int bad = 0;
    for (int n : {1, 3, 4, 5, 4099}) {
        std::vector<uint8_t> w((size_t)n);
        for (auto& v : w) v = (rng() & 1) ? (uint8_t)(rng() & 0xff) : 0;
        uint8_t* d_w = upload(w);
        uint8_t* d_m = (uint8_t*)backend::device_malloc((size_t)n);
        split_mask_weight(ttv(d_w, {1, 1, n, 1}), ttv(d_m, {1, 1, n, 1}));
        const std::vector<uint8_t> m = download(d_m, (size_t)n);
        int b = 0;
        for (int i = 0; i < n; ++i) b += m[(size_t)i] != (w[(size_t)i] != 0 ? 1 : 0);
        std::printf("split n=%d: %d mismatches\n", n, b);
        bad += b;
    }
    bad += check_scale(2, 37, 53, 3, 37, 53, rng);
    bad += check_scale(2, 37, 53, 1, 19, 27, rng);
    bad += check_scale(1, 64, 48, 3, 128, 96, rng);
    std::printf("%s\n", bad ? "FAILED" : "ok");
    return bad ? 1 : 0;
}
