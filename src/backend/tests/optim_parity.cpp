// Backend parity tool for the optimizer launch APIs (fused Adam fp32 /
// quantized-state / doubly-quantized, AdaGrad, float_add_into,
// increment_int32_inplace, densify_update_weight). The SAME source builds
// under both backends:
//
//   CUDA build:   ./optim_parity dump ref.bin
//   Vulkan build: ./optim_parity compare ref.bin   (per device)
//
// Ref format: [nf, floats..., nc, int32 codes...]. Floats compare with the
// usual fast-math tolerance. Quantized packed cells are decoded host-side
// into their integer codes and compare as the codes channel with a +-1
// allowance (the Vulkan backend emulates log1pf/expm1f, which can move a
// borderline cell by one quantization step).
//
// densify_update_weight's Median mode draws hash14 randomness whose float
// arithmetic is contraction-sensitive (can differ CUDA vs Vulkan), so for
// that mode only the deterministic count channel (accum.y) is compared.

#include <kernels/optim/Optimizer.cuh>
#include <kernels/densify/Densify.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
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

std::vector<uint8_t> readback_b(const uint8_t* d, int64_t n) {
    std::vector<uint8_t> h(n);
    backend::memcpy_sync(h.data(), d, n, MemcpyKind::DeviceToHost);
    return h;
}

// Host-side code extraction mirroring the QuantizedAdamState AoS layouts.
void append_qadam_codes(std::vector<int32_t>& codes,
                        const std::vector<uint8_t>& packed, int64_t n_cells,
                        int bits) {
    for (int64_t i = 0; i < n_cells; i++) {
        if (bits == 16) {
            const uint16_t* p16 = (const uint16_t*)packed.data();
            codes.push_back(p16[2 * i]);
            codes.push_back(p16[2 * i + 1]);
        } else if (bits == 8) {
            codes.push_back(packed[2 * i]);
            codes.push_back(packed[2 * i + 1]);
        } else {  // 4
            codes.push_back(packed[i] & 0xf);
            codes.push_back((packed[i] >> 4) & 0xf);
        }
    }
}

// QuantizedTensor codes (value channel of the qq kernel).
void append_qtensor_codes(std::vector<int32_t>& codes,
                          const std::vector<uint8_t>& packed, int64_t n_cells,
                          int bits) {
    for (int64_t i = 0; i < n_cells; i++) {
        if (bits == 16)
            codes.push_back(((const uint16_t*)packed.data())[i]);
        else
            codes.push_back(packed[i]);
    }
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    std::mt19937 rng(987654u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    static constexpr int64_t N = 3000;   // splats
    static constexpr int64_t S = 3;      // stride
    static constexpr int64_t NUMEL = N * S;  // 9000 -> 36 blocks, partial tail

    std::vector<float> acc;        // float outputs
    std::vector<int32_t> codes;    // quantized-cell codes (tol +-1)

    auto make_grad = [&](int64_t n) {
        std::vector<float> g(n);
        for (auto& v : g) v = uf(-1.f, 1.f);
        for (int64_t i = 7; i < n; i += 997)
            g[i] = std::numeric_limits<float>::quiet_NaN();  // isfinite path
        return g;
    };

    // ---- fused_adam_step (fp32) ----
    for (int case_i = 0; case_i < 2; case_i++) {
        std::vector<float> param(NUMEL), g1(NUMEL), g2(NUMEL);
        for (auto& v : param) v = uf(-2.f, 2.f);
        for (auto& v : g1) v = uf(-0.1f, 0.1f);
        for (auto& v : g2) v = uf(0.f, 0.01f);
        std::vector<float> grad = make_grad(NUMEL);
        float* d_param = upload(param);
        float* d_grad = upload(grad);
        float* d_g1 = upload(g1);
        float* d_g2 = upload(g2);
        DeviceVector<int32_t> steps;
        if (case_i == 1) {
            std::vector<int32_t> st(N);
            for (auto& v : st) v = 1 + (int32_t)(rng() % 50);
            steps = DeviceVector<int32_t>(
                std::make_tuple((uint64_t)upload(st), (uint32_t)4,
                                std::vector<int64_t>{N, 1}));
        }
        fused_adam_step(N, ttv(d_param, {N, S, 1}), ttv(d_grad, {N, S, 1}),
                        ttv(d_g1, {N, S, 1}), ttv(d_g2, {N, S, 1}),
                        case_i ? 3e-3f : 1e-2f, 7, steps,
                        case_i ? 0.01f : 0.f, case_i ? 0.1f : 0.f,
                        case_i ? 0.5f : 1.f, case_i == 1);
        backend::device_synchronize();
        readback_f(acc, d_param, NUMEL);
        readback_f(acc, d_g1, NUMEL);
        readback_f(acc, d_g2, NUMEL);
        readback_f(acc, d_grad, NUMEL);  // zero_grad check (case 1)
    }

    // ---- fused_adagrad_step ----
    {
        const int64_t n = 10007;
        std::vector<float> param(n), accum(n);
        for (auto& v : param) v = uf(-2.f, 2.f);
        for (auto& v : accum) v = uf(0.f, 0.1f);
        std::vector<float> grad = make_grad(n);
        float* d_param = upload(param);
        float* d_grad = upload(grad);
        float* d_accum = upload(accum);
        fused_adagrad_step(ttv(d_param, {n, 1}), ttv(d_grad, {n, 1}),
                           ttv(d_accum, {n, 1}), 5e-3f);
        backend::device_synchronize();
        readback_f(acc, d_param, n);
        readback_f(acc, d_accum, n);
    }

    // ---- fused_adam_step_quantized (bits 4, 8) ----
    const int64_t n_blocks = (NUMEL + 255) / 256;
    for (int bits : {4, 8}) {
        const int64_t packed_bytes = NUMEL * (bits == 8 ? 2 : 1);
        std::vector<uint8_t> packed(packed_bytes);
        for (auto& v : packed) v = (uint8_t)(rng() & 0xff);
        std::vector<float> bounds(n_blocks * 4);
        for (int64_t b = 0; b < n_blocks; b++) {
            bounds[4 * b] = uf(-2.f, -0.5f);      // u_min
            bounds[4 * b + 1] = uf(0.5f, 2.f);    // u_max
            bounds[4 * b + 2] = uf(0.f, 5.f);     // log_s_min
            bounds[4 * b + 3] = uf(20.f, 30.f);   // log_s_max
        }
        std::vector<float> param(NUMEL);
        for (auto& v : param) v = uf(-2.f, 2.f);
        std::vector<float> grad = make_grad(NUMEL);
        float* d_param = upload(param);
        float* d_grad = upload(grad);
        uint8_t* d_packed = upload(packed);
        float* d_bounds = upload(bounds);
        fused_adam_step_quantized(N, ttv(d_param, {N, S, 1}),
                                  ttv(d_grad, {N, S, 1}), d_packed,
                                  (float4*)d_bounds, 1e-2f, 12,
                                  DeviceVector<int32_t>{}, 0.f, 0.f, bits,
                                  1.f, false);
        backend::device_synchronize();
        readback_f(acc, d_param, NUMEL);
        readback_f(acc, d_bounds, n_blocks * 4);
        append_qadam_codes(codes, readback_b(d_packed, packed_bytes), NUMEL,
                           bits);
    }

    // ---- fused_adam_step_quantized_value ----
    // (optim_bits, value_bits, use quantized grad)
    const int qq_cases[4][3] = {
        {4, 8, 0}, {8, 8, 1}, {8, 16, 0}, {4, 16, 1}};
    const int64_t n_splat_blocks = (N + 255) / 256;
    for (auto& c : qq_cases) {
        const int obits = c[0], vbits = c[1];
        const bool gq = c[2] != 0;
        const int64_t opt_bytes = NUMEL * (obits == 8 ? 2 : 1);
        const int64_t val_bytes = NUMEL * (vbits == 16 ? 2 : 1);
        std::vector<uint8_t> opt_packed(opt_bytes), val_packed(val_bytes),
            gq_packed(NUMEL);
        for (auto& v : opt_packed) v = (uint8_t)(rng() & 0xff);
        for (auto& v : val_packed) v = (uint8_t)(rng() & 0xff);
        for (auto& v : gq_packed) v = (uint8_t)(rng() & 0xff);  // int8 codes
        std::vector<float> obounds(n_blocks * 4), vbounds(n_blocks * 2),
            gbounds(n_splat_blocks * 2);
        for (int64_t b = 0; b < n_blocks; b++) {
            obounds[4 * b] = uf(-2.f, -0.5f);
            obounds[4 * b + 1] = uf(0.5f, 2.f);
            obounds[4 * b + 2] = uf(0.f, 5.f);
            obounds[4 * b + 3] = uf(20.f, 30.f);
            vbounds[2 * b] = uf(-1.f, -0.1f);
            vbounds[2 * b + 1] = uf(0.1f, 1.f);
        }
        for (int64_t b = 0; b < n_splat_blocks; b++) {
            gbounds[2 * b] = uf(-0.5f, 0.f);
            gbounds[2 * b + 1] = uf(0.f, 0.5f);
        }
        std::vector<float> grad = make_grad(NUMEL);
        float* d_grad = gq ? nullptr : upload(grad);
        uint8_t* d_gq = gq ? upload(gq_packed) : nullptr;
        float* d_gb = gq ? upload(gbounds) : nullptr;
        uint8_t* d_opt = upload(opt_packed);
        float* d_ob = upload(obounds);
        uint8_t* d_val = upload(val_packed);
        float* d_vb = upload(vbounds);
        fused_adam_step_quantized_value(
            N, NUMEL, ttv(d_grad, {N, S, 1}), d_gq, (const float2*)d_gb, d_opt,
            (float4*)d_ob, d_val, (float2*)d_vb, 1e-2f, 9,
            DeviceVector<int32_t>{}, 0.f, 0.f, obits, vbits, 1.f, !gq);
        backend::device_synchronize();
        readback_f(acc, d_ob, n_blocks * 4);
        readback_f(acc, d_vb, n_blocks * 2);
        append_qadam_codes(codes, readback_b(d_opt, opt_bytes), NUMEL, obits);
        append_qtensor_codes(codes, readback_b(d_val, val_bytes), NUMEL,
                             vbits);
        if (!gq) readback_f(acc, d_grad, NUMEL);  // zero_grad untouched here
    }

    // ---- float_add_into / float_max_into / increment_int32_inplace ----
    {
        const int64_t n = 5000;
        std::vector<float> dst(n), src(n);
        for (auto& v : dst) v = uf(-1.f, 1.f);
        for (auto& v : src) v = uf(-1.f, 1.f);
        float* d_dst = upload(dst);
        float* d_src = upload(src);
        float_add_into(DeviceVector<float>(ttv(d_dst, {n, 1})),
                       DeviceVector<float>(ttv(d_src, {n, 1})), n);
        backend::device_synchronize();
        readback_f(acc, d_dst, n);

        float* d_dst_m = upload(dst);
        float_max_into(DeviceVector<float>(ttv(d_dst_m, {n, 1})),
                       DeviceVector<float>(ttv(d_src, {n, 1})), n);
        backend::device_synchronize();
        readback_f(acc, d_dst_m, n);

        const int64_t ni = 4097;
        std::vector<int32_t> data(ni);
        for (auto& v : data) v = (int32_t)(rng() & 0xffff);
        int32_t* d_data = upload(data);
        increment_int32_inplace(
            DeviceVector<int32_t>(std::make_tuple(
                (uint64_t)d_data, (uint32_t)4, std::vector<int64_t>{ni, 1})),
            ni);
        backend::device_synchronize();
        std::vector<int32_t> got(ni);
        backend::memcpy_sync(got.data(), d_data, ni * 4,
                             MemcpyKind::DeviceToHost);
        for (auto v : got) codes.push_back(v);
    }

    // ---- densify_update_weight ----
    {
        std::vector<float> radii(N), opacs(N), w1(N), w2(N), accum(N * 2);
        for (int64_t i = 0; i < N; i++) {
            radii[i] = (i % 5 == 0) ? 0.f : uf(0.5f, 30.f);
            opacs[i] = uf(-3.f, 3.f);
            w1[i] = uf(-1.f, 2.f);
            w2[i] = uf(0.f, 2.f);
            accum[2 * i] = uf(0.01f, 2.f);
            accum[2 * i + 1] = (float)(int)(uf(0.f, 3.f));
        }
        float* d_radii = upload(radii);
        float* d_opacs = upload(opacs);
        float* d_w1 = upload(w1);
        float* d_w2 = upload(w2);
        auto dv_f = [&](float* p, int64_t n) {
            return DeviceVector<float>(ttv(p, {n, 1}));
        };
        auto dv_f2 = [&](float* p, int64_t n) {
            return DeviceVector<float2>(ttv(p, {n, 2}));
        };
        // (score_mode, use opacs, use w2, score_power x10): Mean, Max, Geom
        // fully compared.
        const int dcases[4][4] = {
            {0, 0, 0, 10}, {1, 1, 0, 10}, {3, 1, 1, 10}, {0, 0, 0, 5}};
        std::vector<float> over0(N, 0.f);
        for (auto& c : dcases) {
            float* d_accum = upload(accum);
            float* d_over = upload(over0);
            densify_update_weight(
                N, dv_f(d_radii, N), nullptr, c[1] ? d_opacs : nullptr,
                dv_f(d_w1, N),
                c[2] ? dv_f(d_w2, N) : DeviceVector<float>{},
                c[2] ? 0.3f : 0.f, 0.1f * (float)c[3], dv_f2(d_accum, N),
                c[0], 5.0f, dv_f(d_over, N));
            backend::device_synchronize();
            readback_f(acc, d_accum, N * 2);
            readback_f(acc, d_over, N);
        }
        // Median: hash-dependent random walk; compare only the count.
        {
            float* d_accum = upload(accum);
            densify_update_weight(N, dv_f(d_radii, N), nullptr, d_opacs,
                                  dv_f(d_w1, N), DeviceVector<float>{}, 0.f,
                                  1.0f, dv_f2(d_accum, N), 2, 5.0f,
                                  DeviceVector<float>{});
            backend::device_synchronize();
            std::vector<float> got(N * 2);
            backend::memcpy_sync(got.data(), d_accum, N * 2 * 4,
                                 MemcpyKind::DeviceToHost);
            for (int64_t i = 0; i < N; i++) got[2 * i] = 0.f;
            acc.insert(acc.end(), got.begin(), got.end());
        }

        // ---- densify_oversize_weight ----
        {
            std::vector<float> over(N), sc(N * 2);
            for (int64_t i = 0; i < N; i++) {
                over[i] = (i % 3 == 0) ? 0.f : uf(0.f, 4.f);
                sc[2 * i] = (i % 7 == 0) ? 0.f : uf(0.f, 5.f);
                sc[2 * i + 1] = uf(0.f, 3.f);
            }
            float* d_over = upload(over);
            float* d_sc = upload(sc);
            float* d_out = upload(std::vector<float>(N * 2, 0.f));
            for (float blend : {0.0f, 0.5f, 1.0f}) {
                densify_oversize_weight_tensor(N, blend, dv_f(d_over, N),
                                               dv_f2(d_sc, N),
                                               dv_f2(d_out, N));
                backend::device_synchronize();
                readback_f(acc, d_out, N * 2);
            }
        }

        // ---- densify_clip_score ----
        // (quantile, power): quantile 1 must still run the power pass, and a
        // non-null score_out must leave the accumulator's own .x un-powered.
        std::vector<float> score(N * 2);
        for (int64_t i = 0; i < N; i++) {
            score[2 * i] = uf(0.f, 5.f);
            score[2 * i + 1] = (float)(int)uf(0.f, 4.f);
        }
        score[6] = -1e-9f;
        score[8] = std::numeric_limits<float>::quiet_NaN();
        const float ccases[4][2] = {
            {0.95f, 1.0f}, {1.0f, 0.5f}, {0.95f, 2.0f}, {1.0f, 1.0f}};
        for (auto& c : ccases) {
            float* d_score = upload(score);
            float* d_out = upload(std::vector<float>(N * 2, 0.f));
            densify_clip_score_tensor(N, dv_f2(d_score, N), c[0], c[1],
                                      dv_f2(d_out, N));
            backend::device_synchronize();
            readback_f(acc, d_score, N * 2);
            readback_f(acc, d_out, N * 2);
        }
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)acc.size(), nc = (int64_t)codes.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)codes.data(), nc * 4);
        std::printf("optim_parity: dumped %lld floats + %lld codes to %s\n",
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
        double tol = 5e-3 + 5e-4 * std::fabs((double)ref[i]);
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
    std::printf("optim_parity: %lld floats (max_abs %.3g, violations %lld = "
                "%.5f%%), %lld codes (max |d| %lld, violations %lld = "
                "%.5f%%)\n",
                (long long)nf, max_abs, (long long)fviol, 100.0 * ffrac,
                (long long)nc, (long long)max_c, (long long)cviol,
                100.0 * cfrac);
    bool pass = ffrac <= 1e-3 && cfrac <= 2e-3;
    std::printf(pass ? "optim_parity: PASSED\n" : "optim_parity: FAILED\n");
    return pass ? 0 : 1;
}
