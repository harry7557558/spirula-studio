// Self-checking test for images kept in the form they are stored in
// (core/PixelFormat.h). The loss reading a uint8 or float16 ground truth, or a
// float16 render, must give exactly what the same values widened to float give,
// through the pyramid, the SSIM, the mask coverage and every densify map; a
// float16 render's pyramid is float16 too, so those values pool exactly. The
// appearance chain and the bilateral grids reading float16 must do the same,
// and writing it must store their float result rounded once. Either backend:
//
//   ./pixel_format_parity

#include <kernels/bilagrid/BilagridBindings.h>
#include <kernels/loss/PerPixelLoss.cuh>
#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/HalfFloat.h>
#include <core/Tensor.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using backend::MemcpyKind;

namespace {

constexpr int64_t B = 1, H = 75, W = 101, NP = B * H * W;
int g_failures = 0;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

std::vector<float> download(const void* d, int64_t n) {
    std::vector<float> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * sizeof(float),
                         MemcpyKind::DeviceToHost);
    return h;
}

TorchTensorView view(const void* p, uint32_t esize, int64_t C) {
    return std::make_tuple((uint64_t)p, esize, std::vector<int64_t>{B, H, W, C});
}
TorchTensorView null_view() {
    return std::make_tuple((uint64_t)0, (uint32_t)4, std::vector<int64_t>{0});
}

struct Result {
    std::vector<float> v_rgb, v_Ts, map;
};

Result run(TorchTensorView rgb, TorchTensorView ref, const float* Ts,
           const uint8_t* mask, int mode, int scales = 3, int loss = 0) {
    std::array<float, (int)LossWeightIndex::length> weights{};
    weights[(int)LossWeightIndex::RgbSupL1] = 0.8f;
    weights[(int)LossWeightIndex::RgbSupL2] = 0.2f;
    weights[(int)LossWeightIndex::AlphaSup] = 0.5f;
    std::vector<float> v_losses((size_t)LossIndex::length, 1.0f);
    v_losses[(int)LossIndex::RgbPSNR] = 0.0f;
    float* d_v_losses = upload(v_losses);
    std::vector<bool> needs(13, false);
    needs[0] = needs[7] = true;
    PerPixelGrads grads = {};
    float* v_rgb = upload(std::vector<float>((size_t)3 * NP, 0.0f));
    float* v_Ts = upload(std::vector<float>((size_t)NP, 0.0f));
    float* map = upload(std::vector<float>((size_t)NP, 0.0f));
    grads.v_render_rgb = view(v_rgb, 4, 3);
    grads.v_render_Ts = view(v_Ts, 4, 1);
    compute_multi_scale_per_pixel_losses(
        scales, rgb, ref, null_view(), null_view(), null_view(), null_view(),
        null_view(), view(Ts, 4, 1), null_view(), null_view(), null_view(),
        null_view(), null_view(), view(mask, 1, 1), /*has_mask=*/true,
        weights, 0.2f, loss, /*saturation_threshold=*/0.9f,
        std::make_tuple((uint64_t)d_v_losses, (uint32_t)4,
                        std::vector<int64_t>{(int64_t)LossIndex::length}),
        needs, B, null_view(), view(map, 4, 1), mode, 0.75f, 0.5f, grads);
    backend::device_synchronize();
    Result r{download(v_rgb, 3 * NP), download(v_Ts, NP), download(map, NP)};
    for (void* p : {(void*)d_v_losses, (void*)v_rgb, (void*)v_Ts, (void*)map})
        backend::device_free(p);
    return r;
}

void expect_same(const Result& a, const Result& b, const char* what,
                 double rel = 1e-6) {
    double worst = 0.0;
    auto cmp = [&](const std::vector<float>& x, const std::vector<float>& y) {
        for (size_t i = 0; i < x.size(); i++)
            worst = std::max(worst, std::fabs((double)x[i] - y[i]) /
                                        (1e-9 + rel * std::fabs((double)y[i])));
    };
    cmp(a.v_rgb, b.v_rgb);
    cmp(a.v_Ts, b.v_Ts);
    cmp(a.map, b.map);
    const bool ok = worst <= 1.0;
    std::printf("%-58s %s (%.3g of tolerance)\n", what, ok ? "ok" : "FAILED", worst);
    if (!ok) g_failures++;
}

// Within one float16 rounding of `ref`: half an ulp to nearest, a whole one
// for a device that truncates.
void expect_rounded(const std::vector<float>& got, const std::vector<float>& ref,
                    const char* what) {
    double worst = 0.0;
    for (size_t i = 0; i < ref.size(); i++)
        worst = std::max(worst, std::fabs((double)got[i] - ref[i]) /
                                    (std::ldexp(1.0, -24) +
                                     std::ldexp(std::fabs((double)ref[i]), -10)));
    const bool ok = worst <= 1.0;
    std::printf("%-58s %s (%.3g of tolerance)\n", what, ok ? "ok" : "FAILED", worst);
    if (!ok) g_failures++;
}

// `rel` is of the largest |ref|: a sum in no fixed order can cancel to nothing.
void expect_equal(const std::vector<float>& got, const std::vector<float>& ref,
                  double rel, const char* what) {
    double scale = 0.0, worst = 0.0;
    for (float r : ref) scale = std::max(scale, std::fabs((double)r));
    for (size_t i = 0; i < ref.size(); i++)
        worst = std::max(worst, std::fabs((double)got[i] - ref[i]) /
                                    (1e-9 + rel * scale));
    const bool ok = worst <= 1.0;
    std::printf("%-58s %s (%.3g of tolerance)\n", what, ok ? "ok" : "FAILED", worst);
    if (!ok) g_failures++;
}

std::vector<float> download_half(const void* d, int64_t n) {
    std::vector<uint16_t> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * 2, MemcpyKind::DeviceToHost);
    const float* half_to_float = spirula::half_to_float_table();
    std::vector<float> f((size_t)n);
    for (size_t i = 0; i < h.size(); i++) f[i] = half_to_float[h[i]];
    return f;
}

// Even pixel counts for the float16 buffers, whose writers store pixel pairs.
void* alloc_image(int64_t pixels, PixelFormat f) {
    const size_t bytes = pixel_buffer_bytes(f, pixels, 3);
    void* d = backend::device_malloc((bytes + 3) / 4 * 4);
    backend::memset_sync(d, 0, (bytes + 3) / 4 * 4);
    return d;
}

TorchTensorView image_view(const void* p, PixelFormat f, int64_t b, int64_t h,
                           int64_t w) {
    return std::make_tuple((uint64_t)p, pixel_format_bytes(f),
                           std::vector<int64_t>{b, h, w, 3});
}

// Batches whose pixel count is odd, so a float16 pair straddles two images,
// and an odd total, so the last pixel has no partner.
void check_half_stages(std::mt19937& rng) {
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    const float* half_to_float = spirula::half_to_float_table();
    for (int nb : {2, 1}) {
        const int h = 33, w = 41, L = 6, Hg = 7, Wg = 9;
        const int64_t np = (int64_t)nb * h * w;
        std::vector<uint16_t> rgb16((size_t)3 * np);
        std::vector<float> rgb16f(rgb16.size()), Ts((size_t)np);
        for (size_t i = 0; i < rgb16.size(); i++) {
            rgb16[i] = (uint16_t)(((9 + rng() % 7) << 10) | (rng() & 0x3ff));
            rgb16f[i] = half_to_float[rgb16[i]];
        }
        for (auto& t : Ts) t = uf(0.0f, 1.0f);
        uint16_t* d_rgb16 = upload(rgb16);
        float* d_rgbf = upload(rgb16f);
        float* d_Ts = upload(Ts);
        const DeviceTensor3D<float> Ts_t(std::make_tuple(
            (uint64_t)d_Ts, (uint32_t)4, std::vector<int64_t>{nb, h, w, 1}));
        char what[128];

        // The appearance chain: a background blend over the render.
        AppearanceChainParams ap;
        ap.bg = AppearanceBg::Color;
        ap.bg_color[0] = 0.3f; ap.bg_color[1] = 0.55f; ap.bg_color[2] = 0.8f;
        void* out_f = alloc_image(np, PixelFormat::F32);
        void* out_h = alloc_image(np, PixelFormat::F16);
        void* out_hh = alloc_image(np, PixelFormat::F16);
        void* raw16 = alloc_image(np, PixelFormat::F16);
        appearance_chain_forward(ap, image_view(d_rgbf, PixelFormat::F32, nb, h, w),
                                 Ts_t, image_view(out_f, PixelFormat::F32, nb, h, w),
                                 image_view(raw16, PixelFormat::F16, nb, h, w));
        appearance_chain_forward(ap, image_view(d_rgbf, PixelFormat::F32, nb, h, w),
                                 Ts_t, image_view(out_h, PixelFormat::F16, nb, h, w),
                                 null_view());
        appearance_chain_forward(ap, image_view(d_rgb16, PixelFormat::F16, nb, h, w),
                                 Ts_t, image_view(out_hh, PixelFormat::F16, nb, h, w),
                                 null_view());
        backend::device_synchronize();
        const std::vector<float> ref = download(out_f, 3 * np);
        std::snprintf(what, sizeof(what), "appearance chain, float16 out, %d image(s)", nb);
        expect_rounded(download_half(out_h, 3 * np), ref, what);
        std::snprintf(what, sizeof(what), "appearance chain, float16 in and out, %d image(s)", nb);
        expect_equal(download_half(out_hh, 3 * np), download_half(out_h, 3 * np), 0.0, what);
        std::snprintf(what, sizeof(what), "appearance chain, float16 raw copy, %d image(s)", nb);
        expect_equal(download_half(raw16, 3 * np), rgb16f, 0.0, what);

        // The three colour grids: float16 in gives what float in gives, and
        // float16 out is float out rounded.
        for (int fam = 0; fam < 3; fam++) {
            const char* name = fam == 0 ? "affine" : fam == 1 ? "ppisp" : "loglinear";
            const int C = fam == 0 ? 12 : 9;
            const float lo = fam == 0 ? -0.8f : fam == 1 ? -0.7f : -0.5f;
            std::vector<float> grid((size_t)nb * L * Hg * Wg * C);
            for (auto& g : grid) g = uf(lo, -lo + (fam == 0 ? 0.4f : 0.0f));
            float* d_grid = upload(grid);
            const BilagridReader br(d_grid);
            auto fwd = [&](PixelPtr in, PixelOut out) {
                if (fam == 0)
                    bilagrid_uniform_sample_forward(br, in, out, nb, L, Hg, Wg, h, w,
                                                    backend::kDefaultStream);
                else if (fam == 1)
                    bilagrid_ppisp_uniform_sample_forward(br, in, out, nb, L, Hg, Wg, h,
                                                          w, backend::kDefaultStream);
                else
                    bilagrid_loglinear_uniform_sample_forward(br, in, out, nb, L, Hg, Wg,
                                                              h, w, backend::kDefaultStream);
            };
            const PixelPtr in_f(d_rgbf), in_h(d_rgb16, PixelFormat::F16);
            void* gf = alloc_image(np, PixelFormat::F32);
            void* gff = alloc_image(np, PixelFormat::F32);
            void* gh = alloc_image(np, PixelFormat::F16);
            fwd(in_f, PixelOut((float*)gf));
            fwd(in_h, PixelOut((float*)gff));
            fwd(in_f, PixelOut(gh, PixelFormat::F16));
            backend::device_synchronize();
            const std::vector<float> gref = download(gf, 3 * np);
            std::snprintf(what, sizeof(what), "%s grid, float16 in, %d image(s)", name, nb);
            expect_equal(download(gff, 3 * np), gref, 0.0, what);
            std::snprintf(what, sizeof(what), "%s grid, float16 out, %d image(s)", name, nb);
            expect_rounded(download_half(gh, 3 * np), gref, what);

            std::vector<float> v_out((size_t)3 * np);
            for (auto& v : v_out) v = uf(-0.5f, 0.5f);
            float* d_vout = upload(v_out);
            for (int version = 1; version <= 2; version++) {
                std::vector<float> v_grid[2], v_rgb[2];
                for (int k = 0; k < 2; k++) {
                    const PixelPtr in = k == 0 ? in_f : in_h;
                    float* vg = upload(std::vector<float>(grid.size(), 0.0f));
                    float* vr = upload(v_out);
                    if (fam == 0 && version == 1)
                        bilagrid_uniform_sample_backward_v1(br, in, vr, vg, vr, nb, L, Hg, Wg,
                                                            h, w, 5, backend::kDefaultStream);
                    else if (fam == 0)
                        bilagrid_uniform_sample_backward_v2(br, in, vr, vg, vr, nb, L, Hg, Wg,
                                                            h, w, backend::kDefaultStream);
                    else if (fam == 1 && version == 1)
                        bilagrid_ppisp_uniform_sample_backward_v1(
                            br, in, vr, vg, vr, nb, L, Hg, Wg, h, w, 5, backend::kDefaultStream);
                    else if (fam == 1)
                        bilagrid_ppisp_uniform_sample_backward_v2(
                            br, in, vr, vg, vr, nb, L, Hg, Wg, h, w, backend::kDefaultStream);
                    else if (version == 1)
                        bilagrid_loglinear_uniform_sample_backward_v1(
                            br, in, vr, vg, vr, nb, L, Hg, Wg, h, w, 5, backend::kDefaultStream);
                    else
                        bilagrid_loglinear_uniform_sample_backward_v2(
                            br, in, vr, vg, vr, nb, L, Hg, Wg, h, w, backend::kDefaultStream);
                    backend::device_synchronize();
                    v_grid[k] = download(vg, (int64_t)grid.size());
                    v_rgb[k] = download(vr, 3 * np);
                    backend::device_free(vg);
                    backend::device_free(vr);
                }
                // The grid gradient is summed with atomics.
                std::snprintf(what, sizeof(what), "%s grid backward v%d, float16 in, %d image(s)",
                              name, version, nb);
                expect_equal(v_rgb[1], v_rgb[0], 0.0, what);
                expect_equal(v_grid[1], v_grid[0], 1e-5, what);
            }
            for (void* q : {(void*)d_grid, gf, gff, gh, (void*)d_vout}) backend::device_free(q);
        }

        // The overexposure term reading a float16 render.
        std::vector<float> v[2];
        for (int k = 0; k < 2; k++) {
            float* vr = upload(std::vector<float>((size_t)3 * np, 0.0f));
            overexposure_grad_add(k == 0 ? image_view(d_rgbf, PixelFormat::F32, nb, h, w)
                                         : image_view(d_rgb16, PixelFormat::F16, nb, h, w),
                                  0.5f,
                                  DeviceTensor3D<float3>(image_view(vr, PixelFormat::F32,
                                                                    nb, h, w)));
            backend::device_synchronize();
            v[k] = download(vr, 3 * np);
            backend::device_free(vr);
        }
        std::snprintf(what, sizeof(what), "overexposure, float16 in, %d image(s)", nb);
        expect_equal(v[1], v[0], 0.0, what);

        for (void* q : {(void*)d_rgb16, (void*)d_rgbf, (void*)d_Ts, out_f, out_h, out_hh,
                        raw16})
            backend::device_free(q);
    }
}

}  // namespace

int main() {
    std::mt19937 rng(912u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    const float* half_to_float = spirula::half_to_float_table();
    auto half_of_k64 = [](uint32_t k) -> uint16_t {  // k / 64, k < 2048
        if (k == 0) return 0;
        uint32_t e = 0;
        while ((k >> (e + 1)) != 0) e++;
        return (uint16_t)(((e + 9) << 10) | ((k - (1u << e)) << (10 - e)));
    };

    // uint8 and float16 images and the floats they widen to, reaching past the
    // 0.9 saturation cutoff. The pooled ones are k / 64 under a mask of whole
    // 4x4 blocks, so two levels of 2x2 means stay exact in float16.
    std::vector<uint8_t> ref8((size_t)3 * NP);
    std::vector<float> ref8f(ref8.size());
    std::vector<uint16_t> ref16(ref8.size()), rgb16(ref8.size());
    std::vector<uint16_t> refp(ref8.size()), rgbp(ref8.size());
    std::vector<float> ref16f(ref8.size()), rgb16f(ref8.size()), rgbf(ref8.size());
    std::vector<float> refpf(ref8.size()), rgbpf(ref8.size());
    for (size_t i = 0; i < ref8.size(); i++) {
        ref8[i] = (uint8_t)(rng() & 0xff);
        ref8f[i] = (float)ref8[i] / 255.0f;
        // exponent 10..15: values from 2^-5 up to just under 2
        ref16[i] = (uint16_t)(((10 + rng() % 6) << 10) | (rng() & 0x3ff));
        rgb16[i] = (uint16_t)(((10 + rng() % 6) << 10) | (rng() & 0x3ff));
        ref16f[i] = half_to_float[ref16[i]];
        rgb16f[i] = half_to_float[rgb16[i]];
        rgbf[i] = uf(-0.05f, 1.1f);
        refp[i] = half_of_k64(rng() % 128);
        rgbp[i] = half_of_k64(rng() % 128);
        refpf[i] = half_to_float[refp[i]];
        rgbpf[i] = half_to_float[rgbp[i]];
    }
    std::vector<float> Ts((size_t)NP);
    std::vector<uint8_t> mask((size_t)NP), mask4((size_t)NP);
    for (int64_t i = 0; i < NP; i++) {
        Ts[(size_t)i] = uf(0.0f, 1.0f);
        mask[(size_t)i] = (i % W) < W / 3 && (i / W) % 7 != 0 ? 0 : 1;
        mask4[(size_t)i] = ((i % W) / 4 + (i / W) / 4) % 3 != 0 ? 1 : 0;
    }

    float* d_rgbf = upload(rgbf);
    float* d_rgb16f = upload(rgb16f);
    uint16_t* d_rgb16 = upload(rgb16);
    // Widened on the device by the engine's own upload kernel, so both sides
    // see one rounding of x / 255.
    uint8_t* d_ref8 = upload(ref8);
    float* d_ref8f = upload(ref8f);
    uint8_image_to_float_raw(d_ref8, d_ref8f, (int)B, (int)H, (int)W, 3);
    float* d_ref16f = upload(ref16f);
    uint16_t* d_ref16 = upload(ref16);
    float* d_rgbpf = upload(rgbpf);
    float* d_refpf = upload(refpf);
    uint16_t* d_rgbp = upload(rgbp);
    uint16_t* d_refp = upload(refp);
    float* d_Ts = upload(Ts);
    uint8_t* d_mask = upload(mask);
    uint8_t* d_mask4 = upload(mask4);

    // S-CIELAB's sRGB decode and cube root go through driver transcendentals:
    // NVIDIA Vulkan's uint8 pipeline lands its map 1.7e-6 off (CUDA: exact).
    constexpr int kScielab = (int)PerceptualLoss::SCielab;
    constexpr int kFlip = (int)PerceptualLoss::Flip;
    const struct { int mode; const char* name; int loss = 0; } modes[] = {
        {(int)DensifyLossMapMode::SsimContrastStruct, "ssim_cs"},
        {(int)DensifyLossMapMode::LossFull, "loss_full"},
        {(int)DensifyLossMapMode::EdgeAware, "edge_aware"},
        {(int)DensifyLossMapMode::RobustEdgeAware, "robust_edge_aware"},
        {(int)DensifyLossMapMode::SsimFullNms, "ssim_full_nms"},
        {(int)DensifyLossMapMode::SCielab, "s_cielab", kScielab},
        {(int)DensifyLossMapMode::LossFull, "loss_full on s_cielab", kScielab},
        {(int)DensifyLossMapMode::FlipFeature, "flip_feature", kFlip},
        {(int)DensifyLossMapMode::LossFull, "loss_full on flip", kFlip},
        {(int)DensifyLossMapMode::SsimContrastStruct, "ssim_cs on ssim_blur_tolerant",
         (int)PerceptualLoss::SsimBlurTolerant},
    };
    for (const auto& m : modes) {
        char what[128];
        const Result base8 = run(view(d_rgbf, 4, 3), view(d_ref8f, 4, 3), d_Ts,
                                 d_mask, m.mode, 3, m.loss);
        std::snprintf(what, sizeof(what), "uint8 ground truth, %s", m.name);
        expect_same(run(view(d_rgbf, 4, 3), view(d_ref8, 1, 3), d_Ts, d_mask,
                        m.mode, 3, m.loss), base8, what, m.loss ? 1e-5 : 1e-6);
        const Result base16 = run(view(d_rgb16f, 4, 3), view(d_ref16f, 4, 3),
                                  d_Ts, d_mask, m.mode, 1, m.loss);
        std::snprintf(what, sizeof(what), "float16 render and ground truth, %s",
                      m.name);
        expect_same(run(view(d_rgb16, 2, 3), view(d_ref16, 2, 3), d_Ts, d_mask,
                        m.mode, 1, m.loss), base16, what, m.loss ? 1e-5 : 1e-6);
        const Result basep = run(view(d_rgbpf, 4, 3), view(d_refpf, 4, 3), d_Ts,
                                 d_mask4, m.mode, 3, m.loss);
        std::snprintf(what, sizeof(what), "float16 pyramid, %s", m.name);
        expect_same(run(view(d_rgbp, 2, 3), view(d_refp, 2, 3), d_Ts, d_mask4,
                        m.mode, 3, m.loss), basep, what, m.loss ? 1e-5 : 1e-6);
    }
    check_half_stages(rng);
    std::printf("pixel_format_parity: %s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
