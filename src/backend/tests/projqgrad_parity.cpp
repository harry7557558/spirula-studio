// Backend parity tool for the grad-quant projection backward
// (projection_{3dgs,mip,3dgut}_backward_quantgrad). Runs the parity-verified
// projection forward (plain or packed) for aabb / ids, then drives the
// quantgrad backward TWICE (two "sub-batches") so the second call exercises
// decode of a real non-zero quantized state. The SAME source builds under
// both backends:
//
//   CUDA build:   ./projqgrad_parity dump ref.bin
//   Vulkan build: ./projqgrad_parity compare ref.bin   (per device)
//
// Output format: [nf floats (bounds + fp32 world grads)]
//                [nc int32 codes (packed cells, decoded to signed ints)].
// The register camera-loop accumulation is deterministic; codes compare with
// a +-1 quantum tolerance (codec rounding + last-ulp bound differences).

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/projection/ProjectionBwdQuantGrad.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

// N is a multiple of the 256-thread block: the CUDA splat-parallel kernel
// reads aabb / screen-grad entries for tail (!inside) threads before
// discarding them -- benign against pooled slack in the engine, but an
// illegal access against this tool's exact-size test buffers.
static constexpr int64_t N = 3072;
static constexpr int64_t C = 2;
static constexpr uint32_t W = 200, H = 150;
static constexpr int NUM_SH = 15;
static constexpr int64_t NB = (N + 255) / 256;  // bounds blocks

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
    if (n == 0 || d == nullptr) return;
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

// Read `cells` packed cells of BITS-wide signed codes into int32s.
void readback_codes(std::vector<int32_t>& acc, const uint8_t* d,
                    int64_t cells, int bits) {
    std::vector<uint8_t> raw(cells * (bits / 8));
    backend::memcpy_sync(raw.data(), d, raw.size(), MemcpyKind::DeviceToHost);
    size_t off = acc.size();
    acc.resize(off + cells);
    for (int64_t i = 0; i < cells; i++) {
        if (bits == 16)
            acc[off + i] = (int32_t) * (int16_t*)&raw[2 * i];
        else
            acc[off + i] = (int32_t) * (int8_t*)&raw[i];
    }
}

int check_error(const char* where) {
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error (%s): %s\n", where, err);
        return 1;
    }
    return 0;
}

// static: the engine library these tools link defines the same helper
// inline (engine/EngineCommon.h), and MSVC refuses the duplicate.
static DeviceTensor2D<uint2> vec_to_2d_aabb(const DeviceVector<uint2>& vec) {
    TorchTensorView tv{(uint64_t)vec.data_ptr(), (uint32_t)sizeof(unsigned),
                       {vec.size(), 1LL, 2LL}};
    return DeviceTensor2D<uint2>(tv);
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    std::mt19937 rng(778899u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    std::vector<float> means(N * 3), quats(N * 4), scales(N * 3), opac(N),
        dc(N * 3), sh(N * NUM_SH * 3);
    for (int64_t i = 0; i < N; i++) {
        for (int k = 0; k < 3; k++) means[3 * i + k] = uf(-4.f, 4.f);
        means[3 * i + 2] = uf(-2.f, 8.f);
        float qn = 0.f;
        for (int k = 0; k < 4; k++) {
            quats[4 * i + k] = uf(-1.f, 1.f);
            qn += quats[4 * i + k] * quats[4 * i + k];
        }
        if (qn < 1e-6f) quats[4 * i] = 1.f;
        for (int k = 0; k < 3; k++) scales[3 * i + k] = uf(-5.f, -1.5f);
        opac[i] = uf(-3.f, 5.f);
        for (int k = 0; k < 3; k++) dc[3 * i + k] = uf(-0.5f, 1.5f);
    }
    for (auto& v : sh) v = uf(-0.3f, 0.3f);

    const float cy_ = std::cos(0.2f), sy_ = std::sin(0.2f);
    std::vector<float> vm = {
        1, 0, 0, 0,   0, 1, 0, 0,    0, 0, 1, 4,     0, 0, 0, 1,
        cy_, 0, sy_, 0.3f,  0, 1, 0, -0.2f,  -sy_, 0, cy_, 5.f,  0, 0, 0, 1,
    };
    std::vector<float> intr = {150, 152, 100, 75, 145, 146, 97, 78};
    std::vector<float> dist = dist_fixture::distortion_rows(C);

    float* d_means = upload(means);
    float* d_quats = upload(quats);
    float* d_scales = upload(scales);
    float* d_opac = upload(opac);
    float* d_dc = upload(dc);
    float* d_sh = upload(sh);
    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));

    std::vector<DeviceTensorFloatND> in_splats = {
        DeviceTensorFloatND(ttv(d_means, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_quats, {N, 4, 1})),
        DeviceTensorFloatND(ttv(d_scales, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_opac, {N, 1, 1})),
        DeviceTensorFloatND(ttv(d_dc, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_sh, {N, NUM_SH * 3, 1})),
    };
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));

    // SH value-quant source (per-cell-block layout, q8).
    const int64_t cells_v = N * 3 * NUM_SH;
    std::vector<uint8_t> q8(cells_v);
    for (auto& v : q8) v = (uint8_t)(rng() & 0xff);
    std::vector<float> bounds_cell(2 * ((cells_v + 255) / 256));
    for (size_t i = 0; i < bounds_cell.size() / 2; i++) {
        bounds_cell[2 * i] = uf(-0.5f, 0.1f);
        bounds_cell[2 * i + 1] = bounds_cell[2 * i] + uf(0.05f, 0.8f);
    }
    uint8_t* d_q8 = upload(q8);
    float* d_bcell = upload(bounds_cell);
    std::vector<DeviceTensorFloatND> in_splats_q = in_splats;
    in_splats_q[5] = DeviceTensorFloatND(ttv(nullptr, {N, NUM_SH * 3, 1}));

    std::vector<float> accf;
    std::vector<int32_t> accc;

    const char* cams[4] = {"PINHOLE", "FISHEYE", "EQUISOLID",
                           "EQUIRECTANGULAR"};
    auto dist_tv = [&](int tier) {
        return ttv(d_dist + dist_fixture::row_offset(tier, C),
                   {C, kCameraDistortionParams});
    };

    struct Cfg {
        int prim;    // 0 = 3dgs, 1 = mip, 2 = 3dgut
        bool packed;
        int cam;
        int dist;    // distortion tier, index into dist_fixture::kTierNames
        int max_deg;
        bool geom_quant;  // quantize means/quats/scales (off for 3dgut)
        bool qsrc;        // q8 SH value source
    };
    const Cfg cfgs[] = {
        {0, false, 0, 1, 3, true, false},
        {2, false, 1, 2, 3, false, false},
        {0, true, 2, 1, 2, true, false},
        {2, true, 0, 3, 1, false, true},
        {1, false, 0, 0, 3, true, false},
    };

    for (const Cfg& cfg : cfgs) {
        backend::memset_sync(d_radii, 0, N * sizeof(float));

        const bool quant_src = cfg.qsrc;
        const auto& splats_in = quant_src ? in_splats_q : in_splats;
        std::optional<TorchTensorView> q_packed, q_bounds;
        if (quant_src) {
            q_packed = ttv(d_q8, {cells_v, 1});
            q_bounds = ttv(d_bcell, {(int64_t)bounds_cell.size() / 2, 2});
        }

        // --- forward: aabb (+ ids when packed) ---
        DeviceVector<int32_t> cam_ids, gauss_ids;
        DeviceTensor2D<uint2> aabb_2d;
        DeviceVector<uint2> aabb_vec;
        int64_t n_isect = C * N;
        if (cfg.packed) {
            auto fn = cfg.prim == 0   ? projection_3dgs_packed_forward
                      : cfg.prim == 1 ? projection_mip_packed_forward
                                      : projection_3dgut_packed_forward;
            auto out = fn(N, cfg.max_deg, splats_in, ttv(d_vm, {C, 16}),
                          ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
                          dist_fixture::kTierNames[cfg.dist],
                          dist_tv(cfg.dist), radii, q_packed, q_bounds,
                          (uint32_t)NUM_SH, quant_src ? 8 : 32, 256);
            cam_ids = std::get<0>(out);
            gauss_ids = std::get<1>(out);
            aabb_vec = std::get<2>(out);
            aabb_2d = vec_to_2d_aabb(aabb_vec);
            n_isect = cam_ids.size();
        } else {
            auto fn = cfg.prim == 0   ? projection_3dgs_forward
                      : cfg.prim == 1 ? projection_mip_forward
                                      : projection_3dgut_forward;
            auto out = fn(N, cfg.max_deg, splats_in, ttv(d_vm, {C, 16}),
                          ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
                          dist_fixture::kTierNames[cfg.dist],
                          dist_tv(cfg.dist), radii, q_packed, q_bounds,
                          (uint32_t)NUM_SH, quant_src ? 8 : 32, 256);
            aabb_2d = std::get<0>(out);
        }
        backend::device_synchronize();
        if (check_error("fwd")) return 1;

        // NOTE: the sort in the quantgrad launcher may clobber gaussian_ids
        // (radix ping-pong, as with CUB). Keep a pristine copy for call 2.
        std::vector<int32_t> gauss_host;
        if (cfg.packed) {
            gauss_host.resize(n_isect);
            backend::memcpy_sync(gauss_host.data(), gauss_ids.data_ptr(),
                                 n_isect * 4, MemcpyKind::DeviceToHost);
        }

        // --- screen-space gradients ---
        std::vector<DeviceTensorFloatND> v_screen;
        if (cfg.prim == 2) {
            std::vector<float> vs(n_isect * 3), vo(n_isect), vr(n_isect * 3);
            for (auto& v : vs) v = uf(-0.1f, 0.1f);
            for (auto& v : vo) v = uf(-0.5f, 0.5f);
            for (auto& v : vr) v = uf(-1.f, 1.f);
            auto rows = interleave_screen(n_isect, SCRG_STRIDE,
                {{SCRG_SCALE, &vs}, {SCRG_OPAC, &vo}, {SCRG_RGB, &vr}});
            v_screen = {DeviceTensorFloatND(
                ttv(upload(rows), {n_isect, SCRG_STRIDE, 1}))};
        } else {
            std::vector<float> vxy(n_isect * 2), vd(n_isect),
                vc(n_isect * 3), vo(n_isect), vr(n_isect * 3);
            for (auto& v : vxy) v = uf(-0.5f, 0.5f);
            for (auto& v : vd) v = uf(-0.2f, 0.2f);
            for (auto& v : vc) v = uf(-0.02f, 0.02f);
            for (auto& v : vo) v = uf(-0.5f, 0.5f);
            for (auto& v : vr) v = uf(-1.f, 1.f);
            auto rows = interleave_screen(n_isect, SCR2_STRIDE,
                {{SCR2_XY, &vxy}, {SCR2_DEPTH, &vd}, {SCR2_CONIC, &vc},
                 {SCR2_OPAC, &vo}, {SCR2_RGB, &vr}});
            v_screen = {DeviceTensorFloatND(
                ttv(upload(rows), {n_isect, SCR2_STRIDE, 1}))};
        }

        // --- quantized grad accumulators, zero-initialized ---
        auto zero_packed = [&](int64_t cells, int bits) {
            std::vector<uint8_t> z(cells * (bits / 8), 0);
            return upload(z);
        };
        auto zero_bounds = [&](int64_t blocks) {
            std::vector<float> z(2 * blocks, 0.f);
            return (float2*)upload(z);
        };
        GradQuantBuffers gq{};
        uint8_t *p_means = nullptr, *p_quats = nullptr, *p_scales = nullptr;
        float2 *b_means = nullptr, *b_quats = nullptr, *b_scales = nullptr;
        if (cfg.geom_quant) {
            p_means = zero_packed(N * 3, 16);
            p_quats = zero_packed(N * 4, 16);
            p_scales = zero_packed(N * 3, 16);
            b_means = zero_bounds(NB);
            b_quats = zero_bounds(NB);
            b_scales = zero_bounds(NB);
            gq.means_packed = p_means;
            gq.means_bounds = b_means;
            gq.quats_packed = p_quats;
            gq.quats_bounds = b_quats;
            gq.scales_packed = p_scales;
            gq.scales_bounds = b_scales;
        }
        uint8_t* p_opac = zero_packed(N, 16);
        uint8_t* p_dc = zero_packed(N * 3, 16);
        uint8_t* p_gsh = zero_packed(N * 3 * NUM_SH, 8);
        float2* b_opac = zero_bounds(NB);
        float2* b_dc = zero_bounds(NB);
        float2* b_gsh = zero_bounds(NB);
        gq.opac_packed = p_opac;
        gq.opac_bounds = b_opac;
        gq.dc_packed = p_dc;
        gq.dc_bounds = b_dc;
        gq.sh_packed = p_gsh;
        gq.sh_bounds = b_gsh;

        // fp32 world grads: real buffers for 3dgut (raster-bwd style), null
        // views for 3dgs/mip (adds skipped).
        std::vector<DeviceTensorFloatND> v_world;
        float* vw_ptrs[3] = {nullptr, nullptr, nullptr};
        if (cfg.prim == 2) {
            const int wch[3] = {3, 4, 3};
            for (int c = 0; c < 3; c++) {
                std::vector<float> z(N * wch[c]);
                for (auto& v : z) v = uf(-0.1f, 0.1f);
                vw_ptrs[c] = upload(z);
                v_world.push_back(
                    DeviceTensorFloatND(ttv(vw_ptrs[c], {N, wch[c], 1})));
            }
        } else {
            v_world.push_back(DeviceTensorFloatND(ttv(nullptr, {N, 3, 1})));
            v_world.push_back(DeviceTensorFloatND(ttv(nullptr, {N, 4, 1})));
            v_world.push_back(DeviceTensorFloatND(ttv(nullptr, {N, 3, 1})));
        }
        v_world.push_back(DeviceTensorFloatND(ttv(nullptr, {N, 1, 1})));
        v_world.push_back(DeviceTensorFloatND(ttv(nullptr, {N, 3, 1})));
        v_world.push_back(
            DeviceTensorFloatND(ttv(nullptr, {N, NUM_SH * 3, 1})));

        auto bwd = cfg.prim == 0   ? projection_3dgs_backward_quantgrad
                   : cfg.prim == 1 ? projection_mip_backward_quantgrad
                                   : projection_3dgut_backward_quantgrad;
        for (int call = 0; call < 2; call++) {
            if (cfg.packed && call == 1)  // restore possibly-clobbered keys
                backend::memcpy_sync(gauss_ids.data_ptr(), gauss_host.data(),
                                     n_isect * 4, MemcpyKind::HostToDevice);
            bwd(N, cfg.max_deg, splats_in, ttv(d_vm, {C, 16}),
                ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
                dist_fixture::kTierNames[cfg.dist], dist_tv(cfg.dist),
                cam_ids, gauss_ids, aabb_2d, v_screen,
                v_world, gq, q_packed, q_bounds, (uint32_t)NUM_SH,
                quant_src ? 8 : 32, 256);
            backend::device_synchronize();
            if (check_error("bwd")) return 1;
        }

        // --- readback: bounds + fp32 grads as floats, cells as codes ---
        if (cfg.geom_quant) {
            readback_f(accf, (float*)b_means, 2 * NB);
            readback_f(accf, (float*)b_quats, 2 * NB);
            readback_f(accf, (float*)b_scales, 2 * NB);
        }
        readback_f(accf, (float*)b_opac, 2 * NB);
        readback_f(accf, (float*)b_dc, 2 * NB);
        readback_f(accf, (float*)b_gsh, 2 * NB);
        if (cfg.prim == 2) {
            readback_f(accf, vw_ptrs[0], N * 3);
            readback_f(accf, vw_ptrs[1], N * 4);
            readback_f(accf, vw_ptrs[2], N * 3);
        }
        if (cfg.geom_quant) {
            readback_codes(accc, p_means, N * 3, 16);
            readback_codes(accc, p_quats, N * 4, 16);
            readback_codes(accc, p_scales, N * 3, 16);
        }
        readback_codes(accc, p_opac, N, 16);
        readback_codes(accc, p_dc, N * 3, 16);
        readback_codes(accc, p_gsh, N * 3 * NUM_SH, 8);
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)accf.size(), nc = (int64_t)accc.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)accf.data(), nf * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)accc.data(), nc * 4);
        std::printf("projqgrad_parity: dumped %lld floats + %lld codes\n",
                    (long long)nf, (long long)nc);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nc = 0;
    f.read((char*)&nf, 8);
    std::vector<float> reff(nf);
    f.read((char*)reff.data(), nf * 4);
    f.read((char*)&nc, 8);
    std::vector<int32_t> refc(nc);
    f.read((char*)refc.data(), nc * 4);
    if (nf != (int64_t)accf.size() || nc != (int64_t)accc.size()) {
        std::fprintf(stderr, "count mismatch: ref %lld/%lld vs %zu/%zu\n",
                     (long long)nf, (long long)nc, accf.size(), accc.size());
        return 1;
    }

    int64_t violf = 0, violc = 0;
    double max_abs = 0;
    int max_code_d = 0;
    for (int64_t i = 0; i < nf; i++) {
        double d = std::fabs((double)accf[i] - (double)reff[i]);
        double tol = 5e-3 + 1e-3 * std::fabs((double)reff[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) violf++;
    }
    for (int64_t i = 0; i < nc; i++) {
        int d = std::abs(accc[i] - refc[i]);
        max_code_d = std::max(max_code_d, d);
        if (d > 1) violc++;
    }
    double fracf = nf ? (double)violf / (double)nf : 0.0;
    double fracc = nc ? (double)violc / (double)nc : 0.0;
    std::printf(
        "projqgrad_parity: %lld floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld codes (max |d| %d, violations %lld = %.5f%%)\n",
        (long long)nf, max_abs, (long long)violf, 100.0 * fracf,
        (long long)nc, max_code_d, (long long)violc, 100.0 * fracc);
    bool pass = fracf <= 2e-3 && fracc <= 2e-3;
    std::printf(pass ? "projqgrad_parity: PASSED\n"
                     : "projqgrad_parity: FAILED\n");
    return pass ? 0 : 1;
}
