// Fixed-input timing for SSIM and the bilagrid PPISP image kernels.
// image_loss_bench [mode] [width] [height] [iters] [dump]

#include <kernels/loss/PerPixelLoss.cuh>
#include <kernels/bilagrid/BilagridBindings.h>
#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>

#include <chrono>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

template <typename T>
T* upload(const std::vector<T>& v) {
    auto* p = static_cast<T*>(backend::device_malloc(v.size() * sizeof(T)));
    backend::memcpy_sync(p, v.data(), v.size() * sizeof(T), MemcpyKind::HostToDevice);
    return p;
}

TorchTensorView view(const void* p, int h, int w, int c) {
    return {(uint64_t)p, 4u, {1, h, w, c}};
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "ssim_bt";
    const int w = argc > 2 ? std::atoi(argv[2]) : 4946;
    const int h = argc > 3 ? std::atoi(argv[3]) : 3286;
    const int iters = argc > 4 ? std::atoi(argv[4]) : 20;
    const std::string name(mode);
    const bool affine = name.rfind("affine", 0) == 0;
    const bool grid = name.rfind("bilagrid", 0) == 0 || affine;
    const bool chain = name.rfind("appearance:", 0) == 0;
    const bool ppisp = name.rfind("ppisp:", 0) == 0;
    const bool bt = name.rfind("ssim_bt", 0) == 0;
    const bool quant = name.find("q16") != std::string::npos;
    if ((!grid && !chain && !ppisp && name.rfind("ssim", 0) != 0) || w < 2 || h < 2 || iters < 1)
        return 2;

    const size_t pixels = size_t(w) * h;
    std::mt19937 rng(20261029u);
    auto values = [&](size_t n, float lo, float hi) {
        std::vector<float> v(n);
        for (float& x : v) x = lo + (hi - lo) * float(rng() & 0xffffffu) / 16777215.f;
        return v;
    };
    float* a = upload(values(3 * pixels, 0.f, 1.f));
    float* b = upload(values(3 * pixels, 0.f, 1.f));
    float* grad = upload(values(3 * pixels, -0.001f, 0.001f));
    float* out = upload(std::vector<float>(3 * pixels));
    float* map = upload(std::vector<float>(pixels));
    const int cells = 8 * 16 * 16 * (affine ? 12 : 9);
    auto gh = values(cells, -0.02f, 0.02f);
    if (affine) for (int i = 0; i < cells; ++i)
        if (i % 12 == 0 || i % 12 == 5 || i % 12 == 10) gh[i] += 1.f;
    float* g = upload(gh);
    float* vg = upload(std::vector<float>(cells));
    BilagridReader reader(g);
    if (quant) {
        auto* q = upload(std::vector<uint16_t>(cells));
        auto* bounds = upload(std::vector<float2>((cells + 255) / 256));
        bilagrid_encode_q16_launch(g, q, bounds, cells, backend::kDefaultStream);
        reader = BilagridReader::from_q16(q, bounds);
    }
    float* transmittance = upload(std::vector<float>(pixels, 0.5f));
    float* losses = upload(std::vector<float>((int)LossIndex::length));
    auto av = view(a, h, w, 3), bv = view(b, h, w, 3);
    if (name.find("f16") != std::string::npos) {
        std::vector<uint16_t> half(3 * pixels);
        for (auto& x : half) x = (uint16_t)(((10 + rng() % 5) << 10) | (rng() & 0x3ff));
        av = {(uint64_t)upload(half), 2u, {1, h, w, 3}};
    }
    if (name.find("u8") != std::string::npos) {
        std::vector<uint8_t> bytes(3 * pixels);
        for (auto& x : bytes) x = (uint8_t)(rng() & 255u);
        bv = {(uint64_t)upload(bytes), 1u, {1, h, w, 3}};
    }
    const std::string type = chain || ppisp ? name.substr(name.find(':') + 1) : "no_crf_no_vig";
    const bool pp_on = (chain || ppisp) && type != "off";
    const auto spec = ppisp_param_spec(pp_on ? type : "no_crf_no_vig");
    float* params = upload(values(spec.num_params, -0.02f, 0.02f));
    float* vp = upload(std::vector<float>(spec.num_params));
    float* vt = upload(std::vector<float>(pixels));
    float* intr = upload(std::vector<float>{900.f, 900.f, w * .5f, h * .5f});
    float* matrix = upload(std::vector<float>{1,0,0,0,1,0,0,0,1});
    AppearanceChainParams app;
    app.bg = AppearanceBg::Color;
    app.bg_color[0] = .2f; app.bg_color[1] = .3f; app.bg_color[2] = .4f;
    app.cs_enabled = 1; app.cs_is_linear = 1; app.cs_matrix = matrix;
    app.ppisp = pp_on ? AppearancePpisp::BeforeEncode : AppearancePpisp::Off;
    app.ppisp_layout = spec.layout; app.ppisp_clamp = spec.clamp_output;
    app.ppisp_params = params; app.v_ppisp_params = vp; app.intrins = intr;
    auto pview = TorchTensorView{(uint64_t)params, 4u, {1, spec.num_params}};
    auto vpview = TorchTensorView{(uint64_t)vp, 4u, {1, spec.num_params}};
    std::array<float, (int)LossWeightIndex::length> weights{};
    weights[(int)LossWeightIndex::RgbSupL1] = 0.8f;
    std::vector<bool> needs(13, false);
    needs[0] = true;
    PerPixelGrads grads{};
    grads.v_render_rgb = view(out, h, w, 3);
    auto one = [&]() {
        if (grid) {
            backend::memset_async(vg, 0, cells * sizeof(float));
            auto forward = affine ? bilagrid_uniform_sample_forward : bilagrid_ppisp_uniform_sample_forward;
            auto backward = affine ? bilagrid_uniform_sample_backward_v1 : bilagrid_ppisp_uniform_sample_backward_v1;
            forward(reader, PixelPtr(a), PixelOut(out),
                1, 8, 16, 16, h, w, backend::kDefaultStream, nullptr);
            backward(reader, PixelPtr(a), grad, vg, out,
                1, 8, 16, 16, h, w, 4, backend::kDefaultStream, nullptr);
        } else if (chain || ppisp) {
            backend::memset_async(vp, 0, spec.num_params * sizeof(float));
            backend::memset_async(vt, 0, pixels * sizeof(float));
            if (chain) {
                appearance_chain_forward(app, view(a,h,w,3), DeviceTensor3D<float>(view(transmittance,h,w,1)), view(out,h,w,3), {});
                backend::memcpy_async(out, grad, 3*pixels*sizeof(float), MemcpyKind::DeviceToDevice);
                appearance_chain_backward(app, view(a,h,w,3), DeviceTensor3D<float>(view(transmittance,h,w,1)),
                    DeviceTensor3D<float3>(view(out,h,w,3)), DeviceTensor3D<float>(view(vt,h,w,1)));
            } else {
                ppisp_forward(DeviceTensor3D<float3>(view(a,h,w,3)),pview,
                    TorchTensorView{(uint64_t)intr,4u,{1,4}},float(w),float(h),type,{},DeviceTensor3D<float3>(view(out,h,w,3)));
                ppisp_backward(DeviceTensor3D<float3>(view(a,h,w,3)),pview,
                    TorchTensorView{(uint64_t)intr,4u,{1,4}},float(w),float(h),DeviceTensor3D<float3>(view(grad,h,w,3)),
                    type,{},DeviceTensor3D<float3>(view(out,h,w,3)),vpview);
            }
        } else {
            backend::memset_async(out, 0, 3 * pixels * sizeof(float));
            backend::memset_async(map, 0, pixels * sizeof(float));
            compute_multi_scale_per_pixel_losses(1,
                av, bv, {}, {}, {}, {}, {},
                view(transmittance, h, w, 1), {}, {}, {}, {}, {}, {},
                false, weights, 0.2f,
                (int)(bt ? PerceptualLoss::SsimBlurTolerant : PerceptualLoss::Ssim),
                0.f, view(losses, 1, 1, (int)LossIndex::length), needs, 1, {},
                view(map, h, w, 1), (int)DensifyLossMapMode::SsimContrastStruct,
                0.9f, 0.f, grads);
        }
    };
    for (int i = 0; i < 3; ++i) one();
    backend::device_synchronize();
    const auto warm = std::chrono::steady_clock::now();
    do {
        for (int i = 0; i < 8; ++i) one();
        backend::device_synchronize();
    } while (std::chrono::steady_clock::now() - warm < std::chrono::milliseconds(100));
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) one();
    backend::device_synchronize();
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count() / iters;
    std::printf("%s: %.3f ms/call (%dx%d)\n", mode, ms, w, h);
    if (argc > 5) {
        std::ofstream f(argv[5], std::ios::binary);
        auto dump = [&](const float* p, size_t n) {
            std::vector<float> v(n);
            backend::memcpy_sync(v.data(), p, n * sizeof(float), MemcpyKind::DeviceToHost);
            f.write(reinterpret_cast<const char*>(v.data()), n * sizeof(float));
        };
        dump(out, 3 * pixels);
        if (grid) dump(vg, cells);
        else if (chain || ppisp) { dump(vp, spec.num_params); if (chain) dump(vt, pixels); }
        else dump(map, pixels);
        if (!f) return 1;
    }
    return 0;
}
