// Backend parity tool for the PixelWise training kernels (blend/srgb/noise
// backwards, overexposure grad, depth->normal backward, linear->ray depth,
// color-shift regularizer) and the background-SH backward. The SAME source
// builds under both backends:
//
//   CUDA build:   ./pwtrain_parity dump ref.bin
//   Vulkan build: ./pwtrain_parity compare ref.bin   (per device)
//
// depth_to_normal_backward scatters with atomic adds (accumulation order
// differs) and the background-SH backward reduces millions of pixel
// contributions in a different order than CUDA's block-atomic scheme, so
// comparison uses the usual tolerance + small violation-fraction cap.

#include <backend/tests/DistortionFixture.h>
#include <kernels/pixelwise/PixelWise.cuh>
#include <kernels/background/BackgroundSphericalHarmonics.cuh>
#include <engine/EngineInternal.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

using backend::MemcpyKind;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

void readback_f(std::vector<float>& acc, const float* d, int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

static constexpr int64_t B = 2, H = 48, W = 64;
static constexpr int64_t PIX = B * H * W;

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    std::mt19937 rng(24601u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    auto fill = [&](std::vector<float>& v, float lo, float hi) {
        for (auto& x : v) x = uf(lo, hi);
    };

    std::vector<float> acc;

    std::vector<float> rgb(PIX * 3), T(PIX), bg(PIX * 3), v_out(PIX * 3);
    fill(rgb, -0.2f, 1.2f);
    fill(T, 0.f, 1.f);
    fill(bg, 0.f, 1.f);
    fill(v_out, -1.f, 1.f);
    float* d_rgb = upload(rgb);
    float* d_T = upload(T);
    float* d_bg = upload(bg);
    float* d_vout = upload(v_out);

    auto t3 = [&](float* p) {
        return DeviceTensor3D<float3>(ttv(p, {B, H, W, 3}));
    };
    auto t1 = [&](float* p) {
        return DeviceTensor3D<float>(ttv(p, {B, H, W, 1}));
    };
    auto fresh3 = [&]() { return upload(std::vector<float>(PIX * 3, 0.f)); };
    auto fresh1 = [&]() { return upload(std::vector<float>(PIX, 0.f)); };

    // ---- blend_background_backward ----
    {
        float* d_vr = fresh3();
        float* d_vt = fresh1();
        float* d_vb = fresh3();
        blend_background_backward(t3(d_rgb), t1(d_T), t3(d_bg), t3(d_vout),
                                  t3(d_vr), t1(d_vt), t3(d_vb));
        backend::device_synchronize();
        readback_f(acc, d_vr, PIX * 3);
        readback_f(acc, d_vt, PIX);
        readback_f(acc, d_vb, PIX * 3);
    }

    // ---- blend_background_noise_backward (both linear modes, both draws) ----
    for (int lin = 0; lin < 2; lin++)
    for (int blocky = 0; blocky < 2; blocky++) {
        float* d_vr = fresh3();
        float* d_vt = fresh1();
        blend_background_noise_backward(lin != 0, blocky != 0,
                                        t3(d_rgb), t1(d_T), 0.7f,
                                        1234u + lin, t3(d_vout), t3(d_vr),
                                        t1(d_vt));
        backend::device_synchronize();
        readback_f(acc, d_vr, PIX * 3);
        readback_f(acc, d_vt, PIX);
    }

    // ---- rgb_to_srgb_backward (both modes) ----
    {
        std::vector<float> cm = {0.9f, 0.08f, 0.02f, 0.05f, 0.9f,
                                 0.05f, 0.02f, 0.08f, 0.9f};
        float* d_cm = upload(cm);
        for (int lin = 0; lin < 2; lin++) {
            float* d_vr = fresh3();
            rgb_to_srgb_backward(lin != 0, t3(d_rgb),
                                 DeviceTensor2D<float3>(ttv(d_cm, {3, 1, 3})),
                                 t3(d_vout), t3(d_vr));
            backend::device_synchronize();
            readback_f(acc, d_vr, PIX * 3);
        }
    }

    // ---- overexposure_grad_add (in-place) ----
    {
        std::vector<float> v0(PIX * 3);
        fill(v0, -0.5f, 0.5f);
        float* d_v = upload(v0);
        overexposure_grad_add(t3(d_rgb), 0.25f, t3(d_v));
        backend::device_synchronize();
        readback_f(acc, d_v, PIX * 3);
    }

    // ---- cameras for depth kernels ----
    std::vector<float> intrins = {50.f, 50.f, 32.f, 24.f,
                                  55.f, 52.f, 30.f, 25.f};  // [B,4]
    std::vector<float> dist = dist_fixture::distortion_rows(B);
    float* d_intr = upload(intrins);
    float* d_dist = upload(dist);
    auto dist_tv = [&](int tier) {
        return ttv(d_dist + dist_fixture::row_offset(tier, B),
                   {B, kCameraDistortionParams});
    };
    std::vector<float> depths(PIX);
    fill(depths, 0.5f, 6.f);
    float* d_depths = upload(depths);

    // ---- depth_to_normal_backward (pinhole; every tier x ray/linear) ----
    for (int tier = 0; tier < 4; tier++)
        for (int rd = 0; rd < 2; rd++) {
            std::vector<float> vn(PIX * 3);
            fill(vn, -1.f, 1.f);
            float* d_vn = upload(vn);
            float* d_vd = fresh1();
            depth_to_normal_backward("PINHOLE",
                                     dist_fixture::kTierNames[tier],
                                     ttv(d_intr, {B, 4}), dist_tv(tier),
                                     rd != 0, t1(d_depths), t3(d_vn),
                                     t1(d_vd));
            backend::device_synchronize();
            readback_f(acc, d_vd, PIX);
        }

    // ---- linear_depth_to_ray_depth_inplace (fisheye, thin prism) ----
    {
        std::vector<float> dcopy = depths;
        float* d_d = upload(dcopy);
        linear_depth_to_ray_depth_inplace("FISHEYE", "THIN_PRISM",
                                          ttv(d_intr, {B, 4}), dist_tv(2),
                                          (int)W * 2, (int)H * 2, t1(d_d));
        backend::device_synchronize();
        readback_f(acc, d_d, PIX);
    }

    // ---- color_shift_reg_step (two steps to exercise the EMA) ----
    {
        std::vector<float> post(PIX * 3), pre(PIX * 3), vr(PIX * 3);
        fill(post, 0.f, 1.f);
        fill(pre, 0.f, 1.f);
        fill(vr, -0.1f, 0.1f);
        float* d_post = upload(post);
        float* d_pre = upload(pre);
        float* d_vr = upload(vr);
        float* d_ema = upload(std::vector<float>(3, 0.f));
        float* d_bs = upload(std::vector<float>(3, 0.f));
        color_shift_reg_step(d_vr, d_post, d_pre, d_ema, d_bs, (int)PIX,
                             0.01f, 0.9f, 0, backend::kDefaultStream);
        color_shift_reg_step(d_vr, d_post, d_pre, d_ema, d_bs, (int)PIX,
                             0.01f, 0.9f, 1, backend::kDefaultStream);
        backend::device_synchronize();
        readback_f(acc, d_vr, PIX * 3);
        readback_f(acc, d_ema, 3);
    }

    // ---- background SH forward + backward (degrees 1 and 3) ----
    // viewmats: identity and a mild y-rotation, row-major [R|t].
    {
        const float cy_ = std::cos(0.3f), sy_ = std::sin(0.3f);
        std::vector<float> vm = {
            1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0.5f,  0, 0, 0, 1,
            cy_, 0, sy_, 0,  0, 1, 0, 0,  -sy_, 0, cy_, 1.f,  0, 0, 0, 1};
        float* d_vm = upload(vm);
        for (int deg : {1, 3}) {
            const int tier = deg == 1 ? 1 : 2;   // OpenCV, then thin prism
            const int K = (deg + 1) * (deg + 1);
            std::vector<float> sh(K * 3);
            fill(sh, -0.4f, 0.4f);
            sh[0] = 0.6f; sh[1] = 0.4f; sh[2] = 0.3f;
            float* d_sh = upload(sh);
            float* d_out = fresh3();
            render_background_sh_forward(
                (int)W, (int)H, "PINHOLE", dist_fixture::kTierNames[tier],
                deg, ttv(d_vm, {B, 4, 4}),
                ttv(d_intr, {B, 4}), dist_tv(tier),
                ttv(d_sh, {K, 3}), ttv(d_out, {B, H, W, 3}));
            std::vector<float> vout2(PIX * 3);
            fill(vout2, -1.f, 1.f);
            float* d_vout2 = upload(vout2);
            float* d_vsh = upload(std::vector<float>(K * 3, 0.f));
            render_background_sh_backward(
                (int)W, (int)H, "PINHOLE", dist_fixture::kTierNames[tier],
                deg, ttv(d_vm, {B, 4, 4}),
                ttv(d_intr, {B, 4}), dist_tv(tier),
                ttv(d_sh, {K, 3}), ttv(d_out, {B, H, W, 3}),
                ttv(d_vout2, {B, H, W, 3}), ttv(d_vsh, {K, 3}));
            backend::device_synchronize();
            readback_f(acc, d_out, PIX * 3);
            readback_f(acc, d_vsh, K * 3);
        }
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)acc.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        std::printf("pwtrain_parity: dumped %lld floats to %s\n",
                    (long long)nf, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)acc.size()) {
        std::fprintf(stderr, "float count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, acc.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);

    if (const char* gp = std::getenv("PWTRAIN_DUMP_GOT")) {
        std::ofstream g(gp, std::ios::binary);
        int64_t n2 = (int64_t)acc.size();
        g.write((const char*)&n2, 8);
        g.write((const char*)acc.data(), n2 * 4);
    }
    int64_t viol = 0;
    double max_abs = 0;
    int64_t first_viol = -1, last_viol = -1;
    for (int64_t i = 0; i < nf; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 5e-4 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) {
            if (first_viol < 0) first_viol = i;
            last_viol = i;
            viol++;
        }
    }
    if (viol)
        std::printf("pwtrain_parity: violation index range [%lld, %lld]\n",
                    (long long)first_viol, (long long)last_viol);
    double frac = nf ? (double)viol / (double)nf : 0.0;
    std::printf("pwtrain_parity: %lld floats, max_abs %.3g, violations %lld "
                "(%.5f%%)\n",
                (long long)nf, max_abs, (long long)viol, 100.0 * frac);
    bool pass = frac <= 1e-3;
    std::printf(pass ? "pwtrain_parity: PASSED\n" : "pwtrain_parity: FAILED\n");
    return pass ? 0 : 1;
}
