// Backend parity tool for the fused 3DGS geometry optimizer
// (fused_optim_3dgs_geometry) and the trust-region adamtr color kernels
// (fused_adamtr_(linear_)rgb_(sh_)optim). The SAME source builds under both
// backends:
//
//   CUDA build:   ./optimgeo_parity dump ref.bin
//   Vulkan build: ./optimgeo_parity compare ref.bin   (per device)
//
// Ref format: [nf, floats..., nc, int32 codes...]. Floats compare with the
// usual fast-math tolerance; quantized packed cells are decoded host-side
// into integer codes and compare with a +-1 allowance.
//
// Geometry configs cover: fp32 state, scale-agnostic means (radii),
// per-splat steps, densify-score output, non-SH 16-bit quantized Adam state
// (two-step protocol from the zero-state codec fixed point: all-zero bounds
// + all-zero packed decode exactly to (g1=0, g2=0)), and the block-wise
// 16-bit grad-quant decode path. The chaotic scale-agnostic-means x non-SH
// quant combination is excluded, as in fpbo_parity (u = g1/(sqrt_g2 + eps)
// is unbounded for radii == 0 splats — true on CUDA too).

#include <kernels/optim/Optimizer.cuh>

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

template <typename T>
DeviceVector<T> dv(const void* p, int64_t n) {
    return DeviceVector<T>(std::make_tuple((uint64_t)p, (uint32_t)sizeof(T),
                                           std::vector<int64_t>{n, 1}));
}

void readback_f(std::vector<float>& acc, const float* d, int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

// 16-bit qadam cells -> two codes per cell.
void readback_qadam16(std::vector<int32_t>& codes, const uint8_t* d,
                      int64_t n_cells) {
    std::vector<uint16_t> h(n_cells * 2);
    backend::memcpy_sync(h.data(), d, n_cells * 4, MemcpyKind::DeviceToHost);
    for (int64_t i = 0; i < n_cells * 2; i++) codes.push_back(h[i]);
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    std::mt19937 rng(240717u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    static constexpr int64_t N = 3072;  // multiple of 256 (block reduces)
    const int64_t n_blocks = (N + 255) / 256;

    std::vector<float> acc;
    std::vector<int32_t> codes;

    // ---- fused_optim_3dgs_geometry ----
    // (sam, zero_grad, non_sh_quant, grad_quant, per_splat_steps, densify)
    struct GeoCfg {
        bool sam, zero_grad, nq, gq, steps, densify;
    };
    const GeoCfg geo_cfgs[4] = {
        {false, true, false, false, false, true},
        {true, false, false, false, true, false},
        {false, true, true, false, false, true},
        {true, true, false, true, true, true},
    };

    for (const GeoCfg& c : geo_cfgs) {
        std::vector<float> means(3 * N), quats(4 * N), scales(3 * N),
            opacs(N), dc(3 * N), radii(N);
        for (auto& v : means) v = uf(-2.f, 2.f);
        for (auto& v : quats) v = uf(-1.f, 1.f);
        for (auto& v : scales) v = uf(-4.f, 0.5f);
        for (auto& v : opacs) v = uf(-4.f, 4.f);
        for (auto& v : dc) v = uf(-1.5f, 1.5f);
        for (auto& v : radii) v = uf(0.5f, 40.f);

        std::vector<float> v_means(3 * N), v_quats(4 * N), v_scales(3 * N),
            v_opacs(N), v_dc(3 * N);
        for (auto& v : v_means) v = uf(-0.05f, 0.05f);
        for (auto& v : v_quats) v = uf(-0.05f, 0.05f);
        for (auto& v : v_scales) v = uf(-0.05f, 0.05f);
        for (auto& v : v_opacs) v = uf(-0.05f, 0.05f);
        for (auto& v : v_dc) v = uf(-0.05f, 0.05f);

        float* d_means = upload(means);
        float* d_quats = upload(quats);
        float* d_scales = upload(scales);
        float* d_opacs = upload(opacs);
        float* d_dc = upload(dc);
        float* d_radii = upload(radii);
        float* d_score = nullptr;
        if (c.densify) d_score = upload(std::vector<float>(N, 0.f));

        // fp32 v_* buffers (per-attribute grad-quant replaces means/quats/
        // scales/opac; dc grad stays fp32 in the gq config)
        float* d_v_means = c.gq ? nullptr : upload(v_means);
        float* d_v_quats = c.gq ? nullptr : upload(v_quats);
        float* d_v_scales = c.gq ? nullptr : upload(v_scales);
        float* d_v_opacs = c.gq ? nullptr : upload(v_opacs);
        float* d_v_dc = upload(v_dc);

        GradQuantBuffers gq{};
        if (c.gq) {
            auto gq_attr = [&](int prims, uint8_t*& packed, float2*& bounds) {
                std::vector<uint8_t> pk(prims * N * 2);
                for (auto& v : pk) v = (uint8_t)(rng() & 0xff);
                std::vector<float> bd(n_blocks * 2);
                for (int64_t b = 0; b < n_blocks; b++) {
                    bd[2 * b] = uf(-0.1f, 0.f);
                    bd[2 * b + 1] = uf(0.f, 0.1f);
                }
                packed = upload(pk);
                bounds = (float2*)upload(bd);
            };
            gq_attr(3, gq.means_packed, gq.means_bounds);
            gq_attr(4, gq.quats_packed, gq.quats_bounds);
            gq_attr(3, gq.scales_packed, gq.scales_bounds);
            gq_attr(1, gq.opac_packed, gq.opac_bounds);
        }

        // Adam state: fp32 buffers or zero-initialized 16-bit qadam
        NonShQuantState nq{};
        float* d_g1[5] = {}, * d_g2[5] = {};
        const int prims[5] = {3, 4, 3, 1, 3};
        uint8_t* nq_packed[5] = {};
        float* nq_bounds[5] = {};
        if (c.nq) {
            for (int a = 0; a < 5; a++) {
                nq_packed[a] = upload(
                    std::vector<uint8_t>(prims[a] * N * 4, 0));
                nq_bounds[a] = upload(std::vector<float>(n_blocks * 4, 0.f));
            }
            nq.enabled = true;
            nq.means_packed = nq_packed[0];
            nq.quats_packed = nq_packed[1];
            nq.scales_packed = nq_packed[2];
            nq.opacities_packed = nq_packed[3];
            nq.features_dc_packed = nq_packed[4];
            nq.means_bounds = (float4*)nq_bounds[0];
            nq.quats_bounds = (float4*)nq_bounds[1];
            nq.scales_bounds = (float4*)nq_bounds[2];
            nq.opacities_bounds = (float4*)nq_bounds[3];
            nq.features_dc_bounds = (float4*)nq_bounds[4];
        } else {
            for (int a = 0; a < 5; a++) {
                std::vector<float> g1(prims[a] * N), g2(prims[a] * N);
                for (auto& v : g1) v = uf(-0.02f, 0.02f);
                for (auto& v : g2) v = uf(0.f, 4e-4f);
                d_g1[a] = upload(g1);
                d_g2[a] = upload(g2);
            }
        }

        DeviceVector<int32_t> steps;
        if (c.steps) {
            std::vector<int32_t> st(N);
            for (auto& v : st) v = 1 + (int32_t)(rng() % 40);
            steps = dv<int32_t>(upload(st), N);
        }

        // Two steps so the quantized configs decode self-written state.
        for (int step_i = 0; step_i < (c.nq ? 2 : 1); step_i++) {
            fused_optim_3dgs_geometry(
                N,
                dv<float3>(d_means, N), dv<float3>(d_v_means, N),
                dv<float3>(d_g1[0], N), dv<float3>(d_g2[0], N),
                dv<float4>(d_quats, N), dv<float4>(d_v_quats, N),
                dv<float4>(d_g1[1], N), dv<float4>(d_g2[1], N),
                dv<float3>(d_scales, N), dv<float3>(d_v_scales, N),
                dv<float3>(d_g1[2], N), dv<float3>(d_g2[2], N),
                dv<float>(d_opacs, N), dv<float>(d_v_opacs, N),
                dv<float>(d_g1[3], N), dv<float>(d_g2[3], N),
                dv<float3>(d_dc, N), dv<float3>(d_v_dc, N),
                dv<float>(d_radii, N), dv<float>(d_score, c.densify ? N : 0),
                1.6e-4f, 1e-3f, 5e-3f, 5e-2f, 2.5e-3f,
                /*max_gauss_ratio=*/4.f, /*scale_reg=*/0.1f,
                /*mcmc_op=*/0.05f, /*mcmc_scale=*/0.02f,
                /*erank=*/0.1f, /*erank_s3=*/0.05f, /*quat_norm=*/0.1f,
                /*dc_reg=*/0.01f, /*sh_reg=*/0.01f,
                /*max_screen_size=*/0.3f, /*max_screen_size_penalty=*/1.5f,
                c.sam, nq, gq,
                /*step=*/7 + step_i, steps, /*grad_scale=*/0.5f,
                c.zero_grad);
            backend::device_synchronize();
        }

        readback_f(acc, d_means, 3 * N);
        readback_f(acc, d_quats, 4 * N);
        readback_f(acc, d_scales, 3 * N);
        readback_f(acc, d_opacs, N);
        if (c.densify) readback_f(acc, d_score, N);
        if (c.nq) {
            readback_f(acc, d_dc, 3 * N);  // DC updated in-kernel under nq
            for (int a = 0; a < 5; a++) {
                readback_f(acc, nq_bounds[a], n_blocks * 4);
                readback_qadam16(codes, nq_packed[a], prims[a] * N);
            }
        } else {
            for (int a = 0; a < 5; a++) {
                readback_f(acc, d_g1[a], prims[a] * N);
                readback_f(acc, d_g2[a], prims[a] * N);
            }
        }
        if (c.zero_grad && !c.gq) readback_f(acc, d_v_means, 3 * N);
    }

    // ---- fused_adamtr_(linear_)rgb_(sh_)optim ----
    {
        const int64_t K = 15;  // num_sh
        std::vector<float> dc(3 * N), opacs(N), sh(3 * K * N);
        for (auto& v : dc) v = uf(-1.5f, 1.5f);
        for (auto& v : opacs) v = uf(-4.f, 4.f);
        for (auto& v : sh) v = uf(-0.5f, 0.5f);

        for (int is_linear = 0; is_linear < 2; is_linear++) {
            // rgb (features_dc) variant
            std::vector<float> g1(3 * N), g2(3 * N), grad(3 * N);
            for (auto& v : g1) v = uf(-0.02f, 0.02f);
            for (auto& v : g2) v = uf(0.f, 4e-4f);
            for (auto& v : grad) v = uf(-0.1f, 0.1f);
            float* d_p = upload(dc);
            float* d_g = upload(grad);
            float* d_g1 = upload(g1);
            float* d_g2 = upload(g2);
            float* d_o = upload(opacs);
            auto fn = is_linear ? fused_adamtr_linear_rgb_optim
                                : fused_adamtr_rgb_optim;
            fn(ttv(d_p, {N, 3}), ttv(d_g, {N, 3}), ttv(d_g1, {N, 3}),
               ttv(d_g2, {N, 3}), ttv(d_o, {N, 1}), 2.5e-3f, 0.9f, 0.999f,
               1e-15f, /*eps_tr=*/1e-4f, /*step=*/7, /*grad_scale=*/0.5f,
               /*zero_grad=*/is_linear == 1);
            backend::device_synchronize();
            readback_f(acc, d_p, 3 * N);
            readback_f(acc, d_g, 3 * N);   // zero_grad check
            readback_f(acc, d_g1, 3 * N);
            readback_f(acc, d_g2, 3 * N);

            // sh variant
            std::vector<float> sg1(3 * K * N), sg2(3 * K * N),
                sgrad(3 * K * N);
            for (auto& v : sg1) v = uf(-0.02f, 0.02f);
            for (auto& v : sg2) v = uf(0.f, 4e-4f);
            for (auto& v : sgrad) v = uf(-0.1f, 0.1f);
            float* d_sp = upload(sh);
            float* d_sg = upload(sgrad);
            float* d_sg1 = upload(sg1);
            float* d_sg2 = upload(sg2);
            auto sfn = is_linear ? fused_adamtr_linear_rgb_sh_optim
                                 : fused_adamtr_rgb_sh_optim;
            sfn(ttv(d_sp, {N, K, 3}), ttv(d_sg, {N, K, 3}),
                ttv(d_sg1, {N, K, 3}), ttv(d_sg2, {N, K, 3}),
                ttv(d_p, {N, 3}), ttv(d_o, {N, 1}), 1.25e-4f, 0.9f, 0.999f,
                1e-15f, 1e-4f, 7, 0.5f, is_linear == 1);
            backend::device_synchronize();
            readback_f(acc, d_sp, 3 * K * N);
            readback_f(acc, d_sg, 3 * K * N);
            readback_f(acc, d_sg1, 3 * K * N);
            readback_f(acc, d_sg2, 3 * K * N);
        }
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)acc.size(), nc = (int64_t)codes.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)codes.data(), nc * 4);
        std::printf("optimgeo_parity: dumped %lld floats + %lld codes to "
                    "%s\n",
                    (long long)nf, (long long)nc, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nc = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)acc.size()) {
        std::fprintf(stderr, "float count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, acc.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);
    f.read((char*)&nc, 8);
    if (nc != (int64_t)codes.size()) {
        std::fprintf(stderr, "code count mismatch: ref %lld vs got %zu\n",
                     (long long)nc, codes.size());
        return 1;
    }
    std::vector<int32_t> refc(nc);
    f.read((char*)refc.data(), nc * 4);

    int64_t fviol = 0;
    double max_abs = 0;
    for (int64_t i = 0; i < nf; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 1e-3 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) fviol++;
    }
    int64_t cviol = 0;
    int64_t max_c = 0;
    for (int64_t i = 0; i < nc; i++) {
        int64_t d = std::llabs((int64_t)codes[i] - (int64_t)refc[i]);
        max_c = std::max(max_c, d);
        if (d > 1) cviol++;
    }
    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double cfrac = nc ? (double)cviol / (double)nc : 0.0;
    std::printf(
        "optimgeo_parity: %lld floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld codes (max |d| %lld, violations %lld = %.5f%%)\n",
        (long long)nf, max_abs, (long long)fviol, 100.0 * ffrac,
        (long long)nc, (long long)max_c, (long long)cviol, 100.0 * cfrac);
    bool pass = ffrac <= 2e-3 && cfrac <= 5e-3;
    std::printf(pass ? "optimgeo_parity: PASSED\n"
                     : "optimgeo_parity: FAILED\n");
    return pass ? 0 : 1;
}
