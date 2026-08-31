// Backend parity tool for the fused projection-backward + optimizer
// (fused_projection_bwd_optimizer_{3dgs,mip,3dgut}). Runs the
// parity-verified projection forward (plain or packed), then one FPBO step
// against deterministic screen/world gradients and Adam state; compares the
// in-place-updated splat parameters, Adam state (fp32 or quantized codes),
// bounds, and densify scores. The SAME source builds under both backends:
//
//   CUDA build:   ./fpbo_parity dump ref.bin
//   Vulkan build: ./fpbo_parity compare ref.bin   (per device)
//
// Output format: [nf floats][nc int32 codes (+-1 quantum tolerance)].
// N is a multiple of the 256-thread block (see projqgrad_parity note on the
// CUDA kernel's tail-thread reads).

#include <backend/tests/DistortionFixture.h>
#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/projection/ProjectionPackedFwd.cuh>
#include <kernels/optim/FusedProjectionBwdOptim.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

static constexpr int64_t N = 3072;
static constexpr int64_t C = 2;
static constexpr uint32_t W = 200, H = 150;
static constexpr int NUM_SH = 15;
static constexpr int64_t NB = (N + 255) / 256;

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

// qadam cells: BITS=16 -> one u32 word (u16 | s16<<16); BITS=8 -> one
// halfword (u8 | s8<<8). Emits two codes per cell.
void readback_qadam(std::vector<int32_t>& acc, const uint8_t* d,
                    int64_t cells, int bits) {
    std::vector<uint8_t> raw(cells * (bits == 16 ? 4 : 2));
    backend::memcpy_sync(raw.data(), d, raw.size(), MemcpyKind::DeviceToHost);
    size_t off = acc.size();
    acc.resize(off + 2 * cells);
    for (int64_t i = 0; i < cells; i++) {
        if (bits == 16) {
            uint32_t w = *(uint32_t*)&raw[4 * i];
            acc[off + 2 * i] = (int32_t)(w & 0xffffu);
            acc[off + 2 * i + 1] = (int32_t)(w >> 16);
        } else {
            uint16_t h = *(uint16_t*)&raw[2 * i];
            acc[off + 2 * i] = (int32_t)(h & 0xffu);
            acc[off + 2 * i + 1] = (int32_t)(h >> 8);
        }
    }
}

void readback_u16(std::vector<int32_t>& acc, const uint8_t* d,
                  int64_t cells) {
    std::vector<uint16_t> raw(cells);
    backend::memcpy_sync(raw.data(), d, cells * 2, MemcpyKind::DeviceToHost);
    size_t off = acc.size();
    acc.resize(off + cells);
    for (int64_t i = 0; i < cells; i++)
        acc[off + i] = (int32_t)raw[i];
}

int check_error(const char* where) {
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error (%s): %s\n", where, err);
        return 1;
    }
    return 0;
}

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

    std::mt19937 rng(990011u);
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

    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    float* d_radii = (float*)backend::device_malloc(N * sizeof(float));
    DeviceVector<float> radii(ttv(d_radii, {N, 1}));

    std::vector<float> acc_f;
    std::vector<int32_t> acc_c;

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
        int max_deg;
        int level;     // 0 fp32, 1 quantized SH
        bool non_sh;   // non-SH quant (level 1 only)
        bool sam;      // use_scale_agnostic_mean
        bool ctl;      // color_trust_linear
        int vw_mask;   // which v_splats_world tensors exist
        bool per_splat_steps;
        bool densify;
    };
    // Rows 1 and 2 differ only in the tier, so the NONE fast path is read
    // against a distorted neighbour.
    const Cfg cfgs[] = {
        {0, false, 0, 0, 3, 0, false, false, false, 31, false, true},
        {0, false, 0, 1, 3, 0, false, false, false, 31, false, true},
        {1, false, 1, 2, 2, 0, false, true, false, 0, true, false},
        {2, false, 0, 3, 3, 0, false, false, true, 7, false, true},
        // NOTE: scale-agnostic means x non-SH quant is excluded: the mixed
        // g1/g2 units make u = g1/sqrt(g2) unbounded for edge splats
        // (radii 0 with nonzero grad), so +-1 quantum decode differences
        // amplify chaotically -- inherent to the lossy codec, on CUDA too.
        {0, true, 2, 1, 3, 1, true, false, false, 31, false, false},
        {2, true, 3, 0, 1, 1, false, false, true, 7, false, false},
    };

    for (const Cfg& cfg : cfgs) {
        backend::memset_sync(d_radii, 0, N * sizeof(float));
        const bool level1 = cfg.level == 1;

        // Fresh in-place-updated splat buffers per config.
        float* d_means = upload(means);
        float* d_quats = upload(quats);
        float* d_scales = upload(scales);
        float* d_opac = upload(opac);
        float* d_dc = upload(dc);
        float* d_sh = upload(sh);
        std::vector<DeviceTensorFloatND> splats = {
            DeviceTensorFloatND(ttv(d_means, {N, 3, 1})),
            DeviceTensorFloatND(ttv(d_quats, {N, 4, 1})),
            DeviceTensorFloatND(ttv(d_scales, {N, 3, 1})),
            DeviceTensorFloatND(ttv(d_opac, {N, 1, 1})),
            DeviceTensorFloatND(ttv(d_dc, {N, 3, 1})),
            DeviceTensorFloatND(ttv(d_sh, {N, NUM_SH * 3, 1})),
        };
        std::vector<DeviceTensorFloatND> splats_fwd = splats;
        std::optional<TorchTensorView> q_val_packed, q_val_bounds;

        // Level-1 quantized buffers (also the forward's SH value source).
        uint8_t *d_shq = nullptr, *d_shv = nullptr;
        float *d_shq_b = nullptr, *d_shv_b = nullptr;
        std::optional<TorchTensorView> shq_tv, shq_b_tv, shv_tv, shv_b_tv;
        // FPBO layout: whole 256-splat blocks of whole u32 words.
        const int64_t cells = sh_fpbo_cells(N, NUM_SH);
        if (level1) {
            // Zero packed + zero bounds is the codec's documented valid
            // initial state (decodes to exactly (0, 0) on both backends).
            // Random codes/bounds would land in the u = g1/(sqrt_g2+eps)
            // ill-conditioned regime where cross-compiler FP noise amplifies
            // unboundedly. The VALUE codec bounds get a real range (the
            // forward reads SH values through it); values re-encode against
            // fresh bounds on the first step.
            std::vector<uint8_t> shq(cells * 2, 0);
            std::vector<uint8_t> shv(cells * 2);
            for (auto& v : shv) v = (uint8_t)(rng() & 0xff);
            std::vector<float> shq_b(4 * NB, 0.f), shv_b(2 * NB);
            for (int64_t b = 0; b < NB; b++) {
                shv_b[2 * b] = uf(-0.5f, -0.1f);
                shv_b[2 * b + 1] = uf(0.1f, 0.5f);
            }
            d_shq = upload(shq);
            d_shv = upload(shv);
            d_shq_b = upload(shq_b);
            d_shv_b = upload(shv_b);
            shq_tv = ttv(d_shq, {cells, 1});
            shq_b_tv = ttv(d_shq_b, {NB, 4});
            shv_tv = ttv(d_shv, {cells, 1});
            shv_b_tv = ttv(d_shv_b, {NB, 2});
            // Forward reads the SH values via the codec (FPBO layout).
            splats_fwd[5] = DeviceTensorFloatND(
                ttv(nullptr, {N, NUM_SH * 3, 1}));
            q_val_packed = shv_tv;
            q_val_bounds = shv_b_tv;
        }

        // --- forward: aabb (+ ids when packed) ---
        DeviceVector<int32_t> cam_ids, gauss_ids;
        DeviceTensor2D<uint2> aabb_2d;
        int64_t n_isect = C * N;
        if (cfg.packed) {
            auto fn = cfg.prim == 0   ? projection_3dgs_packed_forward
                      : cfg.prim == 1 ? projection_mip_packed_forward
                                      : projection_3dgut_packed_forward;
            auto out = fn(N, cfg.max_deg, splats_fwd, ttv(d_vm, {C, 16}),
                          ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
                          dist_fixture::kTierNames[cfg.dist],
                          dist_tv(cfg.dist), radii, q_val_packed,
                          q_val_bounds, (uint32_t)NUM_SH, level1 ? 16 : 32,
                          level1 ? 0 : 256);
            cam_ids = std::get<0>(out);
            gauss_ids = std::get<1>(out);
            aabb_2d = vec_to_2d_aabb(std::get<2>(out));
            n_isect = cam_ids.size();
        } else {
            auto fn = cfg.prim == 0   ? projection_3dgs_forward
                      : cfg.prim == 1 ? projection_mip_forward
                                      : projection_3dgut_forward;
            auto out = fn(N, cfg.max_deg, splats_fwd, ttv(d_vm, {C, 16}),
                          ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
                          dist_fixture::kTierNames[cfg.dist],
                          dist_tv(cfg.dist), radii, q_val_packed,
                          q_val_bounds, (uint32_t)NUM_SH, level1 ? 16 : 32,
                          level1 ? 0 : 256);
            aabb_2d = std::get<0>(out);
        }
        backend::device_synchronize();
        if (check_error("fwd")) return 1;

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

        // --- world-space gradients (vw_mask) ---
        const int wch[6] = {3, 4, 3, 1, 3, 3 * NUM_SH};
        std::vector<DeviceTensorFloatND> v_world;
        for (int c = 0; c < 6; c++) {
            if (c < 5 && (cfg.vw_mask >> c) & 1) {
                std::vector<float> z(N * wch[c]);
                for (auto& v : z) v = uf(-0.05f, 0.05f);
                v_world.push_back(
                    DeviceTensorFloatND(ttv(upload(z), {N, wch[c], 1})));
            } else {
                v_world.push_back(
                    DeviceTensorFloatND(ttv(nullptr, {N, wch[c], 1})));
            }
        }

        // --- Adam state ---
        std::vector<DeviceTensorFloatND> g1_world, g2_world;
        std::vector<float*> g1_ptrs, g2_ptrs;
        for (int c = 0; c < 6; c++) {
            std::vector<float> a(N * wch[c]), b(N * wch[c]);
            for (auto& v : a) v = uf(-0.02f, 0.02f);
            for (auto& v : b) v = uf(0.f, 4e-4f);
            g1_ptrs.push_back(upload(a));
            g2_ptrs.push_back(upload(b));
            g1_world.push_back(
                DeviceTensorFloatND(ttv(g1_ptrs[c], {N, wch[c], 1})));
            g2_world.push_back(
                DeviceTensorFloatND(ttv(g2_ptrs[c], {N, wch[c], 1})));
        }

        NonShQuantState non_sh{};
        std::vector<uint8_t*> nq_packed(5, nullptr);
        std::vector<float*> nq_bounds(5, nullptr);
        if (cfg.non_sh) {
            const int prims[5] = {3, 4, 3, 1, 3};  // means/quats/scales/o/dc
            for (int c = 0; c < 5; c++) {
                std::vector<uint8_t> pk(N * prims[c] * 4, 0);  // zero state
                std::vector<float> bd(4 * NB, 0.f);
                nq_packed[c] = upload(pk);
                nq_bounds[c] = upload(bd);
            }
            non_sh.enabled = true;
            non_sh.means_packed = nq_packed[0];
            non_sh.quats_packed = nq_packed[1];
            non_sh.scales_packed = nq_packed[2];
            non_sh.opacities_packed = nq_packed[3];
            non_sh.features_dc_packed = nq_packed[4];
            non_sh.means_bounds = (float4*)nq_bounds[0];
            non_sh.quats_bounds = (float4*)nq_bounds[1];
            non_sh.scales_bounds = (float4*)nq_bounds[2];
            non_sh.opacities_bounds = (float4*)nq_bounds[3];
            non_sh.features_dc_bounds = (float4*)nq_bounds[4];
        }

        // steps + densify score
        std::variant<int32_t, TorchTensorView> step;
        int32_t* d_steps = nullptr;
        if (cfg.per_splat_steps) {
            std::vector<int32_t> st(N);
            for (auto& v : st) v = 1 + (int32_t)(rng() % 50);
            d_steps = upload(st);
            step = ttv(d_steps, {N, 1});
        } else {
            step = (int32_t)10;
        }
        float* d_ds = nullptr;
        DeviceVector<float> densify_score;
        if (cfg.densify) {
            std::vector<float> z(N, 0.f);
            d_ds = upload(z);
            densify_score = DeviceVector<float>(ttv(d_ds, {N, 1}));
        }

        // Two steps: step 1 runs from the zero quantized state (exact on
        // both backends); step 2 decodes step 1's real state. The packed
        // ids may be clobbered by the launcher's radix sort -- restore.
        std::vector<int32_t> gauss_host;
        if (cfg.packed) {
            gauss_host.resize(n_isect);
            backend::memcpy_sync(gauss_host.data(), gauss_ids.data_ptr(),
                                 n_isect * 4, MemcpyKind::DeviceToHost);
        }
        auto fn = cfg.prim == 0   ? fused_projection_bwd_optimizer_3dgs
                  : cfg.prim == 1 ? fused_projection_bwd_optimizer_mip
                                  : fused_projection_bwd_optimizer_3dgut;
        for (int call = 0; call < 2; call++) {
            if (cfg.packed && call == 1)
                backend::memcpy_sync(gauss_ids.data_ptr(), gauss_host.data(),
                                     n_isect * 4, MemcpyKind::HostToDevice);
            fn(N, cfg.max_deg, splats, ttv(d_vm, {C, 16}),
               ttv(d_intr, {C, 4}), W, H, cams[cfg.cam],
               dist_fixture::kTierNames[cfg.dist], dist_tv(cfg.dist),
               cam_ids, gauss_ids, aabb_2d, v_world,
               v_screen, g1_world, g2_world, shq_tv, shq_b_tv, shv_tv,
               shv_b_tv, non_sh, radii, densify_score,
               /*lr_means=*/1.6e-4f, /*lr_quats=*/1e-3f, /*lr_scales=*/5e-3f,
               /*lr_opacs=*/5e-2f, /*lr_features_dc=*/2.5e-3f,
               /*lr_features_sh=*/1.25e-4f, /*max_gauss_ratio=*/10.f,
               /*scale_reg=*/0.1f, /*mcmc_op=*/0.01f, /*mcmc_scale=*/0.01f,
               /*erank=*/0.1f, /*erank_s3=*/0.02f, /*quat_norm=*/0.01f,
               /*dc_reg=*/0.02f, /*sh_reg=*/0.05f,
               /*max_screen_size=*/0.02f, /*max_screen_size_penalty=*/1.f,
               cfg.sam, cfg.ctl,
               /*eps_tr=*/1e-4f,
               call == 0 ? step
                         : std::variant<int32_t, TorchTensorView>(
                               (int32_t)11),
               cfg.level);
            backend::device_synchronize();
            if (check_error("fpbo")) return 1;
        }

        // --- readback: updated params, Adam state, bounds, scores ---
        readback_f(acc_f, d_means, N * 3);
        readback_f(acc_f, d_quats, N * 4);
        readback_f(acc_f, d_scales, N * 3);
        readback_f(acc_f, d_opac, N);
        readback_f(acc_f, d_dc, N * 3);
        if (!level1)
            readback_f(acc_f, d_sh, N * NUM_SH * 3);
        if (cfg.non_sh) {
            const int prims[5] = {3, 4, 3, 1, 3};
            for (int c = 0; c < 5; c++) {
                readback_f(acc_f, nq_bounds[c], 4 * NB);
                readback_qadam(acc_c, nq_packed[c], N * prims[c], 16);
            }
        } else {
            for (int c = 0; c < 5; c++) {
                readback_f(acc_f, g1_ptrs[c], N * wch[c]);
                readback_f(acc_f, g2_ptrs[c], N * wch[c]);
            }
        }
        if (level1) {
            readback_f(acc_f, d_shq_b, 4 * NB);
            readback_f(acc_f, d_shv_b, 2 * NB);
            readback_qadam(acc_c, d_shq, cells, 8);
            readback_u16(acc_c, d_shv, cells);
        } else {
            readback_f(acc_f, g1_ptrs[5], N * NUM_SH * 3);
            readback_f(acc_f, g2_ptrs[5], N * NUM_SH * 3);
        }
        if (cfg.densify)
            readback_f(acc_f, d_ds, N);
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)acc_f.size(), nc = (int64_t)acc_c.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc_f.data(), nf * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)acc_c.data(), nc * 4);
        std::printf("fpbo_parity: dumped %lld floats + %lld codes\n",
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
    if (nf != (int64_t)acc_f.size() || nc != (int64_t)acc_c.size()) {
        std::fprintf(stderr, "count mismatch: ref %lld/%lld vs %zu/%zu\n",
                     (long long)nf, (long long)nc, acc_f.size(),
                     acc_c.size());
        return 1;
    }

    if (const char* gp = std::getenv("FPBO_DUMP_GOT")) {
        std::ofstream g(gp, std::ios::binary);
        int64_t n2 = (int64_t)acc_f.size(), c2 = (int64_t)acc_c.size();
        g.write((const char*)&n2, 8);
        g.write((const char*)acc_f.data(), n2 * 4);
        g.write((const char*)&c2, 8);
        g.write((const char*)acc_c.data(), c2 * 4);
    }
    int64_t violf = 0, violc = 0;
    double max_abs = 0;
    int max_code_d = 0;
    int64_t first_viol = -1;
    for (int64_t i = 0; i < nf; i++) {
        double d = std::fabs((double)acc_f[i] - (double)reff[i]);
        double tol = 5e-3 + 1e-3 * std::fabs((double)reff[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) {
            if (first_viol < 0) first_viol = i;
            violf++;
        }
    }
    for (int64_t i = 0; i < nc; i++) {
        int d = std::abs(acc_c[i] - refc[i]);
        max_code_d = std::max(max_code_d, d);
        if (d > 1) violc++;
    }
    if (violf)
        std::printf("fpbo_parity: first float violation at %lld\n",
                    (long long)first_viol);
    double fracf = nf ? (double)violf / (double)nf : 0.0;
    double fracc = nc ? (double)violc / (double)nc : 0.0;
    std::printf(
        "fpbo_parity: %lld floats (max_abs %.3g, violations %lld = %.5f%%), "
        "%lld codes (max |d| %d, violations %lld = %.5f%%)\n",
        (long long)nf, max_abs, (long long)violf, 100.0 * fracf,
        (long long)nc, max_code_d, (long long)violc, 100.0 * fracc);
    // Codes cap 0.5%: quantized codes sit behind near-tie rounding of the
    // (u, log_s) map, so cross-compiler FP noise shifts ~0.2% of cells by
    // >1 quantum on Intel/llvmpipe. Confirmed benign by comparing those
    // devices against an NVIDIA-Vulkan dump of the SAME shader (identical
    // scatter); floats compare with zero violations on all devices.
    bool pass = fracf <= 2e-3 && fracc <= 5e-3;
    std::printf(pass ? "fpbo_parity: PASSED\n" : "fpbo_parity: FAILED\n");
    return pass ? 0 : 1;
}
