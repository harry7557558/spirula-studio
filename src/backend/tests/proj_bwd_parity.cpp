// Backend parity tool for the plain projection backward
// (projection_{3dgs,mip,3dgut}_backward). Runs the parity-verified
// projection forward (plain or packed) to build real aabb / intersection
// ids, then drives the backward with deterministic screen-space gradients.
// The SAME source builds under both backends:
//
//   CUDA build:   ./proj_bwd_parity dump ref.bin
//   Vulkan build: ./proj_bwd_parity compare ref.bin   (per device)
//
// Per-splat world gradients are accumulated across cameras with atomic adds
// whose order differs between backends, so the comparison is tolerance-based
// with a small violation-fraction cap.

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/projection/ProjectionBwd.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

static constexpr int64_t N = 3000;
static constexpr int64_t C = 2;
static constexpr uint32_t W = 200, H = 150;
static constexpr int NUM_SH = 15;  // degree-3 buffer

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

void readback(std::vector<float>& acc, const float* d, int64_t n) {
    if (n == 0 || d == nullptr) return;
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
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

    std::mt19937 rng(445566u);
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

    // SH value-quant buffers (same recipe as projection_parity): random
    // packed cells + per-block (min, max) bounds in both layouts.
    const int64_t cells = sh_fpbo_cells(N, NUM_SH);
    std::vector<uint8_t> q8(cells);
    std::vector<uint16_t> q16(cells);
    for (auto& v : q8) v = (uint8_t)(rng() & 0xff);
    for (auto& v : q16) v = (uint16_t)(rng() & 0xffff);
    auto gen_bounds = [&](int64_t stride) {
        int64_t nb = (cells + stride - 1) / stride;
        std::vector<float> b(2 * nb);
        for (int64_t i = 0; i < nb; i++) {
            b[2 * i] = uf(-0.5f, 0.1f);
            b[2 * i + 1] = b[2 * i] + uf(0.05f, 0.8f);
        }
        return b;
    };
    std::vector<float> bounds_cell = gen_bounds(256);
    std::vector<float> bounds_fpbo =
        gen_bounds(sh_quant_addr(NUM_SH, 0).bounds_stride);
    uint8_t* d_q8 = upload(q8);
    uint16_t* d_q16 = upload(q16);
    float* d_bcell = upload(bounds_cell);
    float* d_bfpbo = upload(bounds_fpbo);
    std::vector<DeviceTensorFloatND> in_splats_q = in_splats;
    in_splats_q[5] = DeviceTensorFloatND(ttv(nullptr, {N, NUM_SH * 3, 1}));

    std::vector<float> acc;

    const char* cams[4] = {"PINHOLE", "FISHEYE", "EQUISOLID",
                           "EQUIRECTANGULAR"};
    auto dist_tv = [&](int tier) {
        return ttv(d_dist + dist_fixture::row_offset(tier, C),
                   {C, kCameraDistortionParams});
    };

    struct Cfg {
        int prim;      // 0 = 3dgs, 1 = mip, 2 = 3dgut
        bool packed;
        int cam;
        int dist;      // distortion tier, index into dist_fixture::kTierNames
        int max_deg;   // max_sh_degree passed to fwd + bwd
        int qbits;     // 0 = fp32 SH, 8/16 = value-quant
        bool q_fpbo;   // bounds layout when qbits != 0
        bool vmg;      // viewmat grad
    };
    // Rows 1 and 2 are the same config on NONE and on OpenCV, so the tier's
    // fast path is read against a distorted neighbour.
    const Cfg cfgs[] = {
        {0, false, 0, 0, 3, 0, false, true},
        {0, false, 0, 1, 3, 0, false, true},
        {1, false, 1, 2, 2, 0, false, false},
        {0, true, 2, 1, 3, 0, false, false},
        {2, false, 0, 3, 3, 0, false, true},
        {2, true, 1, 0, 1, 0, false, false},
        {0, false, 0, 2, 3, 8, false, false},
        {2, false, 3, 0, 3, 16, true, false},
    };

    for (const Cfg& cfg : cfgs) {
        backend::memset_sync(d_radii, 0, N * sizeof(float));

        const bool quant = cfg.qbits != 0;
        const auto& splats_in = quant ? in_splats_q : in_splats;
        std::optional<TorchTensorView> q_packed, q_bounds;
        int64_t q_stride = 256;
        if (quant) {
            q_packed = ttv(cfg.qbits == 8 ? (const void*)d_q8
                                          : (const void*)d_q16,
                           {cells, 1});
            const auto& b = cfg.q_fpbo ? bounds_fpbo : bounds_cell;
            q_bounds = ttv(cfg.q_fpbo ? d_bfpbo : d_bcell,
                           {(int64_t)b.size() / 2, 2});
            q_stride = cfg.q_fpbo ? 0 : 256;
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
                          (uint32_t)NUM_SH, quant ? cfg.qbits : 32, q_stride);
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
                          (uint32_t)NUM_SH, quant ? cfg.qbits : 32, q_stride);
            aabb_2d = std::get<0>(out);
        }
        backend::device_synchronize();
        if (check_error("fwd")) return 1;

        // --- deterministic screen-space gradients (sized by n_isect) ---
        // The RNG draw count depends on n_isect, which the packed-forward
        // parity test pins as identical across backends.
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

        // --- zeroed world-gradient outputs + optional viewmat grad ---
        const int wch[6] = {3, 4, 3, 1, 3, 3 * NUM_SH};
        std::vector<DeviceTensorFloatND> v_world;
        std::vector<float*> v_world_ptrs;
        for (int c = 0; c < 6; c++) {
            std::vector<float> z(N * wch[c], 0.f);
            float* d = upload(z);
            v_world_ptrs.push_back(d);
            v_world.push_back(DeviceTensorFloatND(ttv(d, {N, wch[c], 1})));
        }
        std::vector<float> zvm(C * 16, 0.f);
        float* d_vvm = upload(zvm);
        DeviceTensor2D<float> v_viewmats(ttv(d_vvm, {C, 16, 1}));

        auto bwd = cfg.prim == 0   ? projection_3dgs_backward
                   : cfg.prim == 1 ? projection_mip_backward
                                   : projection_3dgut_backward;
        bwd(N, cfg.max_deg, splats_in, ttv(d_vm, {C, 16}),
            ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
            dist_fixture::kTierNames[cfg.dist], dist_tv(cfg.dist),
            cam_ids, gauss_ids, aabb_2d, v_screen, v_world,
            cfg.vmg ? &v_viewmats : nullptr, q_packed, q_bounds,
            (uint32_t)NUM_SH, quant ? cfg.qbits : 32, q_stride);
        backend::device_synchronize();
        if (check_error("bwd")) return 1;

        for (int c = 0; c < 6; c++)
            readback(acc, v_world_ptrs[c], N * wch[c]);
        if (cfg.vmg) readback(acc, d_vvm, C * 16);
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t n = (int64_t)acc.size();
        f.write((const char*)&n, sizeof(n));
        f.write((const char*)acc.data(), acc.size() * sizeof(float));
        std::printf("proj_bwd_parity: dumped %lld floats to %s\n",
                    (long long)n, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t n = 0;
    f.read((char*)&n, sizeof(n));
    if (n != (int64_t)acc.size()) {
        std::fprintf(stderr, "float count mismatch: ref %lld vs got %zu\n",
                     (long long)n, acc.size());
        return 1;
    }
    std::vector<float> ref(n);
    f.read((char*)ref.data(), n * 4);

    int64_t viol = 0;
    double max_abs = 0;
    int64_t first_viol = -1, last_viol = -1;
    for (int64_t i = 0; i < n; i++) {
        double d = std::fabs((double)acc[i] - (double)ref[i]);
        double tol = 5e-3 + 1e-3 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) {
            if (first_viol < 0) first_viol = i;
            last_viol = i;
            viol++;
        }
    }
    if (viol)
        std::printf("proj_bwd_parity: violation index range [%lld, %lld]\n",
                    (long long)first_viol, (long long)last_viol);
    double frac = n ? (double)viol / (double)n : 0.0;
    std::printf("proj_bwd_parity: %lld floats, max_abs %.3g, violations %lld "
                "(%.5f%%)\n",
                (long long)n, max_abs, (long long)viol, 100.0 * frac);
    bool pass = frac <= 2e-3;
    std::printf(pass ? "proj_bwd_parity: PASSED\n" : "proj_bwd_parity: FAILED\n");
    return pass ? 0 : 1;
}
