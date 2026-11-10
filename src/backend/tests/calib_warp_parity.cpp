// Backend parity + self-check for the calibration-warp launch APIs
// (kernels/pixelwise/CalibWarp.cuh). `./calib_warp_parity recover` warps a
// synthetic image by a known table and requires the Gauss-Newton loop to find
// it again; `dump ref.bin` / `compare ref.bin` is the usual cross-backend pair.
// The resample is tight; the accumulated sums and the update read atomics.

#include <kernels/pixelwise/CalibWarp.cuh>
#include <core/Tensor.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

namespace {

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T), MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
std::vector<T> download(const T* d, size_t n) {
    std::vector<T> h(n);
    backend::memcpy_sync(h.data(), d, n * sizeof(T), MemcpyKind::DeviceToHost);
    return h;
}

TorchTensorView ttv(const void* p, std::vector<int64_t> shape, uint32_t esize = 4) {
    return std::make_tuple((uint64_t)p, esize, std::move(shape));
}

TorchTensorView ttv_null() { return std::make_tuple((uint64_t)0, (uint32_t)4, std::vector<int64_t>{0}); }

// Smooth texture with detail at several scales, so the warp is observable
// everywhere but the linearization holds over a pixel or two.
std::vector<float> texture(int B, int H, int W, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    struct Wave { float fx, fy, ph, amp; };
    std::vector<Wave> waves;
    for (int i = 0; i < 24; ++i)
        waves.push_back({u(rng) * 0.25f + 0.02f, u(rng) * 0.25f + 0.02f, u(rng) * 6.28f,
                         0.05f + 0.1f * u(rng)});
    std::vector<float> img((size_t)B * H * W * 3);
    for (int b = 0; b < B; ++b)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    float v = 0.5f;
                    for (size_t i = 0; i < waves.size(); ++i) {
                        const Wave& w = waves[i];
                        v += w.amp * std::sin(w.fx * x * (1 + 0.1f * c) + w.fy * y + w.ph + b);
                    }
                    img[(((size_t)b * H + y) * W + x) * 3 + c] = std::min(std::max(v * 0.6f, 0.0f), 1.0f);
                }
    return img;
}

struct Case {
    int B = 2, H = 96, W = 128, n = 5, D = 2, N = 3;
};

}  // namespace

// `bench W H`: device time of each launch at one image of that size.
int bench(int W, int H) {
    const int B = 1, n = 9, D = 2, P = n * n * D;
    const int64_t px = (int64_t)W * H;
    std::vector<uint8_t> g8((size_t)px * 3);
    for (size_t i = 0; i < g8.size(); ++i) g8[i] = (uint8_t)((i * 2654435761u) >> 24);
    uint8_t* gt = upload(g8);
    float* render = upload(std::vector<float>((size_t)px * 3, 0.5f));
    float* v = upload(std::vector<float>((size_t)px * 3, 1e-7f));
    uint16_t* out = upload(std::vector<uint16_t>((size_t)px * 3 + 3, 0));
    std::vector<float> tab((size_t)P);
    for (int i = 0; i < P; ++i) tab[(size_t)i] = 0.3f * std::sin((float)i);
    float* params = upload(tab);
    float* grad = upload(std::vector<float>((size_t)P, 0.0f));
    float* hess = upload(std::vector<float>((size_t)n * n * 3, 0.0f));
    float* intr = upload(std::vector<float>{1000.0f, 1000.0f, W * 0.5f, H * 0.5f});
    const TorchTensorView g = ttv(gt, {B, H, W, 3}, 1), r = ttv(render, {B, H, W, 3});
    const TorchTensorView o = ttv(out, {B, H, W, 3}, 2), vv = ttv(v, {B, H, W, 3});
    const TorchTensorView t = ttv(params, {B, P}), it = ttv(intr, {B, 4});
    auto time = [&](const char* what, auto&& fn) {
        for (int i = 0; i < 5; ++i) fn();
        backend::device_synchronize();
        const auto t0 = std::chrono::steady_clock::now();
        constexpr int kReps = 100;
        for (int i = 0; i < kReps; ++i) fn();
        backend::device_synchronize();
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count() / kReps;
        std::printf("calib_warp_parity: %-28s %8.3f ms at %dx%d\n", what, ms, W, H);
    };
    time("forward (f16 out)", [&] {
        calib_warp_gt_forward(g, r, ttv_null(), ttv_null(), t, it, ttv_null(), n, D, o);
    });
    time("accumulate (registration)", [&] {
        calib_warp_accumulate(g, r, ttv_null(), ttv_null(), ttv_null(), 0.0f, t, it, ttv_null(),
                              ttv_null(), n, D, 0.03f, 1.0f, false, ttv(grad, {B, P}),
                              ttv(hess, {B, n * n * 3}));
    });
    time("accumulate (loss gradient)", [&] {
        calib_warp_accumulate(g, r, ttv_null(), ttv_null(), vv, 1.0f, t, it, ttv_null(), ttv_null(), n,
                              D, 0.03f, 1.0f, false, ttv(grad, {B, P}), ttv(hess, {B, n * n * 3}));
    });
    time("accumulate (loss, pre-warped)", [&] {
        calib_warp_accumulate(o, r, ttv_null(), ttv_null(), vv, 1.0f, t, it, ttv_null(), ttv_null(), n,
                              D, 0.03f, 1.0f, true, ttv(grad, {B, P}), ttv(hess, {B, n * n * 3}));
    });
    time("update", [&] {
        calib_warp_update(t, ttv(grad, {B, P}), ttv(hess, {B, n * n * 3}), ttv_null(), it, 1.0f, B, n,
                          D, 0.0f, 0.1f, 0.05f, 0.01f, 1.5f);
    });
    for (void* p : std::initializer_list<void*>{gt, render, v, out, params, grad, hess, intr})
        backend::device_free(p);
    return 0;
}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "recover";
    if (mode == "bench")
        return bench(argc > 2 ? std::atoi(argv[2]) : 1920, argc > 3 ? std::atoi(argv[3]) : 1080);
    if ((mode == "dump" || mode == "compare") && argc < 3) {
        std::fprintf(stderr,
                     "usage: calib_warp_parity recover | dump ref.bin | compare ref.bin | bench [W H]\n");
        return 2;
    }
    std::vector<float> tight, loose;
    int failures = 0;

    for (int D : {2, 4}) {
        Case k;
        k.D = D;
        const int P = k.n * k.n * k.D, DH = calib_warp_hess_floats(k.D);
        const int64_t px = (int64_t)k.B * k.H * k.W;
        std::mt19937 rng(7 + D);
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);

        // A known table in mrad; with fx = fy = 1000 one mrad is one pixel.
        std::vector<float> truth((size_t)k.N * P, 0.0f);
        for (auto& v : truth) v = 1.2f * u(rng);
        std::vector<float> intr;
        for (int b = 0; b < k.B; ++b) intr.insert(intr.end(), {1000.0f, 1000.0f, k.W * 0.5f, k.H * 0.5f});
        const std::vector<int32_t> cams = {2, 0};
        std::vector<float> depth((size_t)px);
        for (int64_t i = 0; i < px; ++i) depth[(size_t)i] = 2.0f + 0.8f * std::sin(0.05f * (float)(i % k.W)) +
                                                          0.5f * (float)((i / k.W) % k.H) / k.H;

        const std::vector<float> gt_h = texture(k.B, k.H, k.W, 11);
        std::vector<uint8_t> gt_u8(gt_h.size());
        for (size_t i = 0; i < gt_h.size(); ++i) gt_u8[i] = (uint8_t)std::lround(gt_h[i] * 255.0f);
        std::vector<uint8_t> mask((size_t)px, 1);
        for (int64_t i = 0; i < px; i += 7) mask[(size_t)i] = 0;

        float* gt = upload(gt_h);
        uint8_t* gt8 = upload(gt_u8);
        float* d_intr = upload(intr);
        int32_t* d_cams = upload(cams);
        // The render holds sum(w d) = d (1 - T); the warp reads back d.
        std::vector<float> Ts((size_t)px), depth_sum((size_t)px);
        for (int64_t i = 0; i < px; ++i) {
            Ts[(size_t)i] = 0.4f * (0.5f + 0.5f * std::sin(0.11f * (float)i));
            depth_sum[(size_t)i] = depth[(size_t)i] * (1.0f - Ts[(size_t)i]);
        }
        float* d_depth = upload(depth_sum);
        float* d_ts = upload(Ts);
        uint8_t* d_mask = upload(mask);
        float* d_truth = upload(truth);
        float* render = upload(std::vector<float>((size_t)px * 3, 0.0f));
        float* out = upload(std::vector<float>((size_t)px * 3, 0.0f));
        float* params = upload(std::vector<float>((size_t)k.N * P, 0.0f));
        float* grad = upload(std::vector<float>((size_t)k.B * P, 0.0f));
        float* hess = upload(std::vector<float>((size_t)k.B * k.n * k.n * DH, 0.0f));
        uint16_t* out16 = upload(std::vector<uint16_t>((size_t)px * 3 + 3, 0));

        const auto img = [&](const void* p, uint32_t es = 4) { return ttv(p, {k.B, k.H, k.W, 3}, es); };
        const auto tab = [&](const void* p, int cols) { return ttv(p, {k.N, cols}); };
        const auto btab = [&](const void* p, int cols) { return ttv(p, {k.B, cols}); };
        const TorchTensorView t_depth = k.D == 4 ? ttv(d_depth, {k.B, k.H, k.W, 1}) : ttv_null();
        const TorchTensorView t_ts = k.D == 4 ? ttv(d_ts, {k.B, k.H, k.W, 1}) : ttv_null();

        // render := gt warped by the truth, so the fit should land on the truth.
        calib_warp_gt_forward(img(gt), img(gt), t_depth, t_ts, tab(d_truth, P), ttv(d_intr, {k.B, 4}),
                              ttv(d_cams, {k.B}), k.n, k.D, img(render));
        auto fwd = download(render, (size_t)px * 3);
        tight.insert(tight.end(), fwd.begin(), fwd.end());
        // The uint8 path reads the same values to within the quantization.
        calib_warp_gt_forward(img(gt8, 1), img(gt), t_depth, t_ts, tab(d_truth, P), ttv(d_intr, {k.B, 4}),
                              ttv(d_cams, {k.B}), k.n, k.D, img(out));
        auto fwd8 = download(out, (size_t)px * 3);
        double worst8 = 0.0;
        for (size_t i = 0; i < fwd8.size(); ++i) worst8 = std::max(worst8, (double)std::fabs(fwd8[i] - fwd[i]));
        if (worst8 > 1.0 / 255.0) {
            std::fprintf(stderr, "D=%d: uint8 resample off by %g\n", D, worst8);
            ++failures;
        }
        // A zero table lands on the pixel centres, where Catmull-Rom is exact.
        calib_warp_gt_forward(img(gt), img(gt), t_depth, t_ts, tab(params, P), ttv(d_intr, {k.B, 4}),
                              ttv(d_cams, {k.B}), k.n, k.D, img(out));
        auto ident = download(out, (size_t)px * 3);
        double worst_id = 0.0;
        for (size_t i = 0; i < ident.size(); ++i)
            worst_id = std::max(worst_id, (double)std::fabs(ident[i] - gt_h[i]));
        if (worst_id > 1e-6) {
            std::fprintf(stderr, "D=%d: identity resample off by %g\n", D, worst_id);
            ++failures;
        }
        // The pyramid step of the eval-time alignment, odd edge included.
        {
            const int H2 = (k.H + 1) / 2, W2 = (k.W + 1) / 2 + 1;
            float* half = upload(std::vector<float>((size_t)k.B * H2 * W2 * 3, 0.0f));
            calib_warp_downsample2(img(gt8, 1), ttv(half, {k.B, H2, W2, 3}));
            auto got = download(half, (size_t)k.B * H2 * W2 * 3);
            tight.insert(tight.end(), got.begin(), got.end());
            double worst = 0.0;
            for (int b = 0; b < k.B; ++b)
                for (int y = 0; y < H2; ++y)
                    for (int x = 0; x < W2; ++x)
                        for (int c = 0; c < 3; ++c) {
                            double want = 0.0;
                            for (int d = 0; d < 4; ++d) {
                                const int sx = std::min(2 * x + (d & 1), k.W - 1);
                                const int sy = std::min(2 * y + (d >> 1), k.H - 1);
                                want += 0.25 * gt_u8[(((size_t)b * k.H + sy) * k.W + sx) * 3 + c] / 255.0;
                            }
                            worst = std::max(worst, std::fabs(want - got[(((size_t)b * H2 + y) * W2 + x) * 3 + c]));
                        }
            if (worst > 1e-5) {
                std::fprintf(stderr, "D=%d: downsample off by %g\n", D, worst);
                ++failures;
            }
            backend::device_free(half);
        }
        // A float16 output, read back through the engine's own codec.
        calib_warp_gt_forward(img(gt), img(gt), t_depth, t_ts, tab(d_truth, P), ttv(d_intr, {k.B, 4}),
                              ttv(d_cams, {k.B}), k.n, k.D, img(out16, 2));
        auto h16 = download(out16, (size_t)px * 3);
        double worst16 = 0.0;
        for (size_t i = 0; i < h16.size(); ++i) {
            const uint32_t hb = h16[i], e = (hb >> 10) & 31, m = hb & 1023;
            const float v = e == 0 ? std::ldexp((float)m, -24) : std::ldexp(1.0f + m / 1024.0f, (int)e - 15);
            worst16 = std::max(worst16, (double)std::fabs(v - fwd[i]));
        }
        if (worst16 > 1e-3) {
            std::fprintf(stderr, "D=%d: float16 resample off by %g\n", D, worst16);
            ++failures;
        }

        calib_warp_accumulate(img(gt), img(render), t_depth, t_ts, ttv_null(), 0.0f, tab(params, P),
                              ttv(d_intr, {k.B, 4}), ttv(d_cams, {k.B}),
                              ttv(d_mask, {k.B, k.H, k.W, 1}, 1), k.n, k.D, 0.03f, 1.0f, false,
                              btab(grad, P), btab(hess, k.n * k.n * DH));
        auto g = download(grad, (size_t)k.B * P);
        auto h = download(hess, (size_t)k.B * k.n * k.n * DH);
        loose.insert(loose.end(), g.begin(), g.end());
        loose.insert(loose.end(), h.begin(), h.end());

        // A pyramid level: the half-size pair with the focal scaled by its width
        // ratio lands on the same table, to within what half the pixels resolve.
        if (k.D == 2) {
            const int H2 = (k.H + 1) / 2, W2 = (k.W + 1) / 2;
            float* g2 = upload(std::vector<float>((size_t)k.B * H2 * W2 * 3, 0.0f));
            float* r2 = upload(std::vector<float>((size_t)k.B * H2 * W2 * 3, 0.0f));
            const auto half = [&](const void* p) { return ttv(p, {k.B, H2, W2, 3}); };
            calib_warp_downsample2(img(gt), half(g2));
            calib_warp_downsample2(img(render), half(r2));
            for (int it = 0; it < 60; ++it) {
                calib_warp_accumulate(half(g2), half(r2), ttv_null(), ttv_null(), ttv_null(), 0.0f,
                                      tab(params, P), ttv(d_intr, {k.B, 4}), ttv(d_cams, {k.B}),
                                      ttv_null(), k.n, k.D, 0.03f, (float)W2 / k.W, false, btab(grad, P),
                                      btab(hess, k.n * k.n * DH));
                calib_warp_update(tab(params, P), btab(grad, P), btab(hess, k.n * k.n * DH),
                                  ttv(d_cams, {k.B}), ttv(d_intr, {k.B, 4}), (float)W2 / k.W, k.B,
                                  k.n, k.D, 1.0f, 1e-3f, 1e-4f, 0.0f, 2.0f);
            }
            auto coarse = download(params, (size_t)k.N * P);
            double e2 = 0.0, r2s = 0.0;
            for (int c : cams)
                for (int y = 1; y < k.n - 1; ++y)
                    for (int x = 1; x < k.n - 1; ++x)
                        for (int i = 0; i < 2; ++i) {
                            const size_t o = (size_t)c * P + (y * k.n + x) * k.D + i;
                            e2 += (coarse[o] - truth[o]) * (coarse[o] - truth[o]);
                            r2s += truth[o] * truth[o];
                        }
            std::printf("calib_warp_parity: half-size level: table error %.3f of the planted rms\n",
                        std::sqrt(e2 / r2s));
            if (!(std::sqrt(e2 / r2s) < 0.2)) ++failures;
            backend::memset_sync(params, 0, (size_t)k.N * P * sizeof(float));
            backend::device_free(g2);
            backend::device_free(r2);
        }
        // The loss-gradient pass on the copy already warped by the table agrees
        // with resampling the source through it.
        {
            std::vector<float> vh((size_t)px * 3);
            // What an L2 loss sends back when the table is half way there.
            for (size_t i = 0; i < vh.size(); ++i) vh[i] = 1e-6f * (gt_h[i] - fwd[i]);
            float* d_v = upload(vh);
            float* g_direct = upload(std::vector<float>((size_t)k.B * P, 0.0f));
            float* g_warped = upload(std::vector<float>((size_t)k.B * P, 0.0f));
            float* h2 = upload(std::vector<float>((size_t)k.B * k.n * k.n * DH, 0.0f));
            for (int pass = 0; pass < 2; ++pass) {
                backend::memset_sync(h2, 0, (size_t)k.B * k.n * k.n * DH * sizeof(float));
                calib_warp_accumulate(pass ? img(render) : img(gt), img(gt), t_depth, t_ts,
                                      img(d_v), 1e5f, tab(d_truth, P), ttv(d_intr, {k.B, 4}),
                                      ttv(d_cams, {k.B}), ttv_null(), k.n, k.D, 0.03f, 1.0f,
                                      pass == 1, btab(pass ? g_warped : g_direct, P),
                                      btab(h2, k.n * k.n * DH));
            }
            auto a1 = download(g_direct, (size_t)k.B * P), a2 = download(g_warped, (size_t)k.B * P);
            tight.insert(tight.end(), a2.begin(), a2.end());
            double dot = 0, n1 = 0, n2 = 0;
            for (size_t i = 0; i < a1.size(); ++i) {
                dot += (double)a1[i] * a2[i];
                n1 += (double)a1[i] * a1[i];
                n2 += (double)a2[i] * a2[i];
            }
            const double cosv = dot / std::sqrt(n1 * n2 + 1e-30), ratio = std::sqrt(n2 / (n1 + 1e-30));
            std::printf("calib_warp_parity: D=%d pre-warped gradient: cosine %.4f, norm ratio %.3f\n", D,
                        cosv, ratio);
            if (!(cosv > 0.98 && ratio > 0.9 && ratio < 1.1)) ++failures;
            for (void* q : std::initializer_list<void*>{d_v, g_direct, g_warped, h2}) backend::device_free(q);
        }
        // The Gauss-Newton loop from zero; damping and priors near zero.
        for (int it = 0; it < 100; ++it) {
            if (it > 0)
                calib_warp_accumulate(img(gt), img(render), t_depth, t_ts, ttv_null(), 0.0f, tab(params, P),
                                      ttv(d_intr, {k.B, 4}), ttv(d_cams, {k.B}),
                                      ttv(d_mask, {k.B, k.H, k.W, 1}, 1), k.n, k.D, 0.03f, 1.0f, false,
                                      btab(grad, P), btab(hess, k.n * k.n * DH));
            calib_warp_update(tab(params, P), btab(grad, P), btab(hess, k.n * k.n * DH), ttv(d_cams, {k.B}),
                              ttv(d_intr, {k.B, 4}), 1.0f, k.B, k.n, k.D, 1.0f, 1e-3f, 1e-4f, 0.0f, 2.0f);
        }
        auto fit = download(params, (size_t)k.N * P);
        loose.insert(loose.end(), fit.begin(), fit.end());

        bool untouched_ok = true;
        for (int c = 0; c < k.N; ++c)
            if (c != cams[0] && c != cams[1])
                for (int i = 0; i < P; ++i) untouched_ok &= fit[(size_t)c * P + i] == 0.0f;
        // Judged on the pixel flow rather than the table: where depth varies
        // little the parallax term trades against the displacement. A border
        // cell sees part of its support, so the inner half of the image only.
        auto flow_at = [&](const std::vector<float>& t, int b, int x, int y, float out2[2]) {
            const float* row = t.data() + (size_t)cams[b] * P;
            const float uu = (x + 0.5f) * (k.n - 1) / k.W, vv = (y + 0.5f) * (k.n - 1) / k.H;
            const int i0 = std::min(std::max((int)std::floor(uu), 0), k.n - 2);
            const int j0 = std::min(std::max((int)std::floor(vv), 0), k.n - 2);
            const float fu = uu - i0, fv = vv - j0;
            const float w[4] = {(1 - fu) * (1 - fv), fu * (1 - fv), (1 - fu) * fv, fu * fv};
            const float e = 1.0f / depth[((size_t)b * k.H + y) * k.W + x];
            out2[0] = out2[1] = 0.0f;
            for (int q = 0; q < 4; ++q) {
                const float* c = row + ((j0 + (q >> 1)) * k.n + i0 + (q & 1)) * k.D;
                out2[0] += w[q] * (c[0] + (k.D == 4 ? e * c[2] : 0.0f));
                out2[1] += w[q] * (c[1] + (k.D == 4 ? e * c[3] : 0.0f));
            }
        };
        double err = 0.0, ref = 0.0;
        int cnt = 0;
        for (int b = 0; b < k.B; ++b)
            for (int y = k.H / 4; y < 3 * k.H / 4; ++y)
                for (int x = k.W / 4; x < 3 * k.W / 4; ++x) {
                    float a[2], t[2];
                    flow_at(fit, b, x, y, a);
                    flow_at(truth, b, x, y, t);
                    err += (a[0] - t[0]) * (a[0] - t[0]) + (a[1] - t[1]) * (a[1] - t[1]);
                    ref += t[0] * t[0] + t[1] * t[1];
                    ++cnt;
                }
        const double rms = std::sqrt(err / std::max(cnt, 1)), rms_ref = std::sqrt(ref / std::max(cnt, 1));
        std::printf("calib_warp_parity: D=%d recovered the planted flow to %.4f px rms (planted %.3f)\n",
                    D, rms, rms_ref);
        if (!untouched_ok) {
            std::fprintf(stderr, "D=%d: an image outside the batch moved\n", D);
            ++failures;
        }
        if (!(rms < 0.05)) ++failures;

        for (void* p : std::initializer_list<void*>{gt, gt8, d_intr, d_cams, d_depth, d_mask, d_truth,
                                                     render, out, out16, params, grad, hess, d_ts})
            backend::device_free(p);
    }

    if (mode == "recover") {
        std::printf(failures ? "calib_warp_parity: FAILED\n" : "calib_warp_parity: PASSED\n");
        return failures ? 1 : 0;
    }
    if (mode == "dump") {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)tight.size(), nl = (int64_t)loose.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)tight.data(), nf * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)loose.data(), nl * 4);
        std::printf("calib_warp_parity: dumped %lld + %lld floats\n", (long long)nf, (long long)nl);
        return failures ? 1 : 0;
    }
    std::ifstream f(argv[2], std::ios::binary);
    int64_t nf = 0, nl = 0;
    f.read((char*)&nf, 8);
    std::vector<float> rt((size_t)std::max<int64_t>(nf, 0));
    f.read((char*)rt.data(), nf * 4);
    f.read((char*)&nl, 8);
    std::vector<float> rl((size_t)std::max<int64_t>(nl, 0));
    f.read((char*)rl.data(), nl * 4);
    if (!f || nf != (int64_t)tight.size() || nl != (int64_t)loose.size()) {
        std::fprintf(stderr, "calib_warp_parity: reference size mismatch\n");
        return 1;
    }
    auto viol = [](const std::vector<float>& got, const std::vector<float>& want, double abs_tol,
                   double rel_tol, double& worst) {
        int64_t v = 0;
        worst = 0.0;
        for (size_t i = 0; i < got.size(); ++i) {
            const double d = std::fabs((double)got[i] - (double)want[i]);
            worst = std::max(worst, d);
            if (!(d <= abs_tol + rel_tol * std::fabs((double)want[i]))) ++v;
        }
        return v;
    };
    double wt = 0, wl = 0;
    const int64_t vt = viol(tight, rt, 1e-5, 1e-5, wt);
    const int64_t vl = viol(loose, rl, 1e-3, 1e-3, wl);
    std::printf("calib_warp_parity: tight %lld floats (max %.3g, %lld over), loose %lld (max %.3g, %lld over)\n",
                (long long)nf, wt, (long long)vt, (long long)nl, wl, (long long)vl);
    const bool pass = failures == 0 && vt == 0 && vl <= nl / 1000;
    std::printf(pass ? "calib_warp_parity: PASSED\n" : "calib_warp_parity: FAILED\n");
    return pass ? 0 : 1;
}
