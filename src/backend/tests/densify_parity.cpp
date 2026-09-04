// Backend parity tool for the densification launch APIs: densify_clip_scale,
// the MCMC/revised noise kernels, the long-axis-split relocate/add family
// and the MCMC relocate/add family. The SAME source builds under both
// backends:
//
//   CUDA build:   ./densify_parity dump ref.bin
//   Vulkan build: ./densify_parity compare ref.bin   (per device)
//
// Ref format: [nf tight floats] [nl loose floats] [nc int32 codes].
//
// Channels: deterministic outputs (clip_scale) compare in the tight float
// channel. Everything downstream of float-hash randomness sits in the LOOSE
// float channel with a violation-fraction cap: randn3 noise and the
// sampling draws (efraimidis keys, MCMC cumsum binary searches) hash
// integers bit-exactly on both backends, but their float post-processing
// (fract/log/cumsum association) can flip rare boundary cases
// cross-backend, and one flipped sample rewrites one splat's row. Quantized
// packed cells compare as integer codes with a +-1 allowance and the same
// rationale for its violation cap.
//
// The long-axis-split RELOCATE path pairs sampled srcs with atomically
// compacted dst slots, so the pairing is scheduling-dependent even on CUDA
// alone. The relocate configs are therefore constructed with EXACTLY ONE
// relocating splat (one low-opacity config, one non-finite config), making
// the pairing trivial and the comparison elementwise; the ADD path (same
// kernel, dst = cur + i deterministic) covers many concurrent dst splats,
// including neighbor-word masked atomic stores in the quantized layouts.

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

void readback_i(std::vector<int32_t>& acc, const int32_t* d, int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(int32_t),
                         MemcpyKind::DeviceToHost);
}

// Packed cells -> integer codes at CODE granularity (so the +-1 allowance
// applies per quantization step, not per raw byte): code_bits selects the
// element width (16 -> u16 halfwords, 8 -> bytes, 4 -> nibbles).
void readback_codes(std::vector<int32_t>& codes, const uint8_t* d,
                    int64_t n_bytes, int code_bits) {
    std::vector<uint8_t> h(n_bytes);
    backend::memcpy_sync(h.data(), d, n_bytes, MemcpyKind::DeviceToHost);
    if (code_bits == 16) {
        for (int64_t i = 0; i < n_bytes / 2; i++)
            codes.push_back(((const uint16_t*)h.data())[i]);
    } else if (code_bits == 8) {
        for (auto v : h) codes.push_back(v);
    } else {  // 4
        for (auto v : h) {
            codes.push_back(v & 0xf);
            codes.push_back((v >> 4) & 0xf);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    std::mt19937 rng(260716u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    static constexpr int64_t N = 3000;      // current splats
    static constexpr int64_t MAX_ADD = 600; // append headroom
    static constexpr int64_t CAP = N + MAX_ADD;

    std::vector<float> acc;    // tight floats
    std::vector<float> lacc;   // loose floats (sampling/noise downstream)
    std::vector<int32_t> codes;

    // ---- densify_clip_scale (tight) ----
    {
        std::vector<float> radii(N), scales(3 * N), opacs(N);
        for (auto& v : radii) v = uf(0.f, 60.f);
        for (auto& v : scales) v = uf(-4.f, 1.f);
        for (auto& v : opacs) v = uf(-4.f, 4.f);
        const float inf = std::numeric_limits<float>::infinity();
        // (max2d, hardness, max3d, clip opacs)
        const float ccfg[2][4] = {{25.f, 8.f, inf, 1.f},
                                  {inf, inf, 0.8f, 0.f}};
        for (auto& c : ccfg) {
            float* d_radii = upload(radii);
            float* d_scales = upload(scales);
            float* d_opacs = upload(opacs);
            densify_clip_scale_tensor(N, dv<float>(d_radii, N),
                                      dv<float3>(d_scales, N),
                                      c[3] != 0.f ? d_opacs : nullptr, c[0],
                                      c[1], c[2]);
            backend::device_synchronize();
            readback_f(acc, d_scales, 3 * N);
            readback_f(acc, d_opacs, N);
        }
    }

    // ---- mcmc / revised noise (loose: randn3 float hashing) ----
    {
        std::vector<float> means(3 * N), scales(3 * N), quats(4 * N),
            opacs(N), radii(N);
        for (auto& v : means) v = uf(-2.f, 2.f);
        for (auto& v : scales) v = uf(-5.f, -1.f);
        for (auto& v : quats) v = uf(-1.f, 1.f);
        for (auto& v : opacs) v = uf(-4.f, 4.f);
        for (int64_t i = 0; i < N; i++)
            radii[i] = (i % 7 == 0) ? 0.f : uf(0.5f, 40.f);

        float* d_means = upload(means);
        float* d_scales = upload(scales);
        float* d_quats = upload(quats);
        float* d_opacs = upload(opacs);
        mcmc_add_noise_tensor(N, 2e-2f, dv<float3>(d_means, N),
                              dv<float3>(d_scales, N), dv<float4>(d_quats, N),
                              dv<float>(d_opacs, N));
        backend::device_synchronize();
        readback_f(lacc, d_means, 3 * N);

        float* d_means2 = upload(means);
        float* d_radii = upload(radii);
        revised_add_noise_tensor(N, 5e-2f, dv<float>(d_radii, N),
                                 dv<float3>(d_means2, N),
                                 dv<float3>(d_scales, N),
                                 dv<float4>(d_quats, N),
                                 dv<float>(d_opacs, N));
        backend::device_synchronize();
        readback_f(lacc, d_means2, 3 * N);
    }

    // ---- long-axis-split relocate/add + MCMC relocate/add ----
    // Shared per-config splat-model construction. sh_optim_bits in
    // {32, 8, 4}; sh_value_bits in {32, 8, 16}.
    struct Model {
        int64_t num_sh, num_sh_buffer;
        int sh_optim_bits, sh_value_bits;
        bool bounds_per_splat, non_sh;
        // device buffers
        float *means, *quats, *scales, *opacs, *dc, *sh;
        float *g1[5], *g2[5];           // means/quats/scales/opacs/dc
        float *g1_sh, *g2_sh;           // fp32 SH state
        uint8_t* sh_state_packed;       // quantized SH state (aliased g1/g2)
        float* sh_state_bounds;         // float4 blocks
        uint8_t* sh_value_packed;
        float* sh_value_bounds;         // float2 blocks
        uint8_t* nq_packed[5];
        float* nq_bounds[5];
        float* accum;                   // float2 [CAP]
        int32_t* bias_steps;
        int64_t sh_state_bytes, sh_value_bytes;
        int64_t sh_state_bound_f, sh_value_bound_f;
        NonShQuantState nq{};
    };
    const int prims[5] = {3, 4, 3, 1, 3};

    auto build_model = [&](int sh_optim_bits, int sh_value_bits,
                           bool bounds_per_splat, bool non_sh,
                           int64_t num_sh, int64_t num_sh_buffer,
                           int64_t special_lowopac,
                           int64_t special_nan,
                           int64_t special_dead_scale) -> Model {
        Model m{};
        m.num_sh = num_sh;
        m.num_sh_buffer = num_sh_buffer;
        m.sh_optim_bits = sh_optim_bits;
        m.sh_value_bits = sh_value_bits;
        m.bounds_per_splat = bounds_per_splat;
        m.non_sh = non_sh;

        std::vector<float> means(3 * CAP), quats(4 * CAP), scales(3 * CAP),
            opacs(CAP), dc(3 * CAP);
        for (auto& v : means) v = uf(-2.f, 2.f);
        for (auto& v : quats) v = uf(-1.f, 1.f);
        for (auto& v : scales) v = uf(-4.f, 0.f);
        for (auto& v : dc) v = uf(-1.5f, 1.5f);
        // opacities: all clearly above the relocation threshold except the
        // designated low-opacity splat
        for (auto& v : opacs) v = uf(0.5f, 4.f);
        if (special_lowopac >= 0) opacs[special_lowopac] = -8.f;
        if (special_nan >= 0)
            means[3 * special_nan] = std::numeric_limits<float>::quiet_NaN();
        // exp() of this is not a positive normal float: the splat is gone
        // for good, and relocation has to recycle it.
        if (special_dead_scale >= 0)
            for (int k = 0; k < 3; k++)
                scales[3 * special_dead_scale + k] = -90.f;
        m.means = upload(means);
        m.quats = upload(quats);
        m.scales = upload(scales);
        m.opacs = upload(opacs);
        m.dc = upload(dc);

        if (sh_value_bits == 32) {
            std::vector<float> sh(3 * num_sh * CAP);
            for (auto& v : sh) v = uf(-0.5f, 0.5f);
            m.sh = upload(sh);
        } else {
            // The FPBO layout (bounds_per_splat) addresses whole 256-splat
            // blocks of whole words -- see docs/notes/sh-quant-layout.md.
            int64_t cells = bounds_per_splat
                ? sh_fpbo_cells(CAP, (uint32_t)num_sh_buffer)
                : 3 * num_sh_buffer * CAP;
            m.sh_value_bytes = cells * (sh_value_bits == 16 ? 2 : 1);
            std::vector<uint8_t> pk(m.sh_value_bytes);
            for (auto& v : pk) v = (uint8_t)(rng() & 0xff);
            m.sh_value_packed = upload(pk);
            m.sh_value_bound_f =
                2 * (bounds_per_splat ? (CAP + 255) / 256
                                      : (cells + 255) / 256);
            std::vector<float> bd(m.sh_value_bound_f);
            for (int64_t b = 0; b < m.sh_value_bound_f / 2; b++) {
                bd[2 * b] = uf(-1.f, -0.1f);
                bd[2 * b + 1] = uf(0.1f, 1.f);
            }
            m.sh_value_bounds = upload(bd);
        }

        if (sh_optim_bits == 32) {
            std::vector<float> g(3 * num_sh * CAP);
            for (auto& v : g) v = uf(-0.02f, 0.02f);
            m.g1_sh = upload(g);
            for (auto& v : g) v = uf(0.f, 4e-4f);
            m.g2_sh = upload(g);
        } else {
            // state stride is 3 * num_sh cells per splat (see FusedAppearanceOptim.cu)
            int64_t cells = bounds_per_splat
                ? sh_fpbo_cells(CAP, (uint32_t)num_sh)
                : 3 * num_sh * CAP;
            m.sh_state_bytes = cells * (sh_optim_bits == 8 ? 2 : 1);
            std::vector<uint8_t> pk(m.sh_state_bytes);
            for (auto& v : pk) v = (uint8_t)(rng() & 0xff);
            m.sh_state_packed = upload(pk);
            m.sh_state_bound_f =
                4 * (bounds_per_splat ? (CAP + 255) / 256
                                      : (cells + 255) / 256);
            std::vector<float> bd(m.sh_state_bound_f);
            for (int64_t b = 0; b < m.sh_state_bound_f / 4; b++) {
                bd[4 * b] = uf(-2.f, -0.5f);
                bd[4 * b + 1] = uf(0.5f, 2.f);
                bd[4 * b + 2] = uf(0.f, 5.f);
                bd[4 * b + 3] = uf(20.f, 30.f);
            }
            m.sh_state_bounds = upload(bd);
        }

        if (non_sh) {
            const int64_t nb = (CAP + 255) / 256;
            for (int a = 0; a < 5; a++) {
                std::vector<uint8_t> pk(prims[a] * CAP * 4);
                for (auto& v : pk) v = (uint8_t)(rng() & 0xff);
                m.nq_packed[a] = upload(pk);
                std::vector<float> bd(nb * 4);
                for (int64_t b = 0; b < nb; b++) {
                    bd[4 * b] = uf(-2.f, -0.5f);
                    bd[4 * b + 1] = uf(0.5f, 2.f);
                    bd[4 * b + 2] = uf(0.f, 5.f);
                    bd[4 * b + 3] = uf(20.f, 30.f);
                }
                m.nq_bounds[a] = upload(bd);
            }
            m.nq.enabled = true;
            m.nq.means_packed = m.nq_packed[0];
            m.nq.quats_packed = m.nq_packed[1];
            m.nq.scales_packed = m.nq_packed[2];
            m.nq.opacities_packed = m.nq_packed[3];
            m.nq.features_dc_packed = m.nq_packed[4];
            m.nq.means_bounds = (float4*)m.nq_bounds[0];
            m.nq.quats_bounds = (float4*)m.nq_bounds[1];
            m.nq.scales_bounds = (float4*)m.nq_bounds[2];
            m.nq.opacities_bounds = (float4*)m.nq_bounds[3];
            m.nq.features_dc_bounds = (float4*)m.nq_bounds[4];
        } else {
            for (int a = 0; a < 5; a++) {
                std::vector<float> g(prims[a] * CAP);
                for (auto& v : g) v = uf(-0.02f, 0.02f);
                m.g1[a] = upload(g);
                for (auto& v : g) v = uf(0.f, 4e-4f);
                m.g2[a] = upload(g);
            }
        }

        // accum weights: wide multiplicative spread so sort-key near-ties
        // (the cross-backend flip risk) are rare
        std::vector<float> accum(2 * CAP);
        for (int64_t i = 0; i < CAP; i++) {
            accum[2 * i] = std::exp(uf(-3.f, 3.f));
            accum[2 * i + 1] = (float)(int)uf(0.f, 3.f);
        }
        m.accum = upload(accum);

        std::vector<int32_t> steps(CAP);
        for (auto& v : steps) v = 1 + (int32_t)(rng() % 40);
        m.bias_steps = upload(steps);
        return m;
    };

    auto readback_model = [&](const Model& m) {
        readback_f(lacc, m.means, 3 * CAP);
        readback_f(lacc, m.quats, 4 * CAP);
        readback_f(lacc, m.scales, 3 * CAP);
        readback_f(lacc, m.opacs, CAP);
        readback_f(lacc, m.dc, 3 * CAP);
        readback_f(lacc, (const float*)m.accum, 2 * CAP);
        readback_i(codes, m.bias_steps, CAP);
        if (m.sh_value_bits == 32)
            readback_f(lacc, m.sh, 3 * m.num_sh * CAP);
        else
            readback_codes(codes, m.sh_value_packed, m.sh_value_bytes,
                           m.sh_value_bits);
        if (m.sh_optim_bits == 32) {
            readback_f(lacc, m.g1_sh, 3 * m.num_sh * CAP);
            readback_f(lacc, m.g2_sh, 3 * m.num_sh * CAP);
        } else {
            // 8-bit cells hold (u_q, log_s_q) byte pairs; 4-bit cells hold
            // the joint nibbles — both split into per-code elements.
            readback_codes(codes, m.sh_state_packed, m.sh_state_bytes,
                           m.sh_optim_bits);
        }
        for (int a = 0; a < 5; a++) {
            if (m.non_sh) {
                readback_codes(codes, m.nq_packed[a], prims[a] * CAP * 4,
                               16);
            } else {
                readback_f(lacc, m.g1[a], prims[a] * CAP);
                readback_f(lacc, m.g2[a], prims[a] * CAP);
            }
        }
    };

    // Argument pack shared by the four API calls.
    auto call_relocate_las = [&](Model& m, float min_opacity, uint32_t seed) {
        relocate_splats_with_long_axis_split_tensor(
            N, min_opacity, /*split_opacity_k=*/0.6f,
            dv<float3>(m.means, CAP), dv<float4>(m.quats, CAP),
            dv<float3>(m.scales, CAP), dv<float>(m.opacs, CAP),
            dv<float3>(m.dc, CAP),
            dv<float3>(m.sh, m.sh ? m.num_sh * CAP : 0),
            dv<float3>(m.g1[0], m.g1[0] ? CAP : 0),
            dv<float4>(m.g1[1], m.g1[1] ? CAP : 0),
            dv<float3>(m.g1[2], m.g1[2] ? CAP : 0),
            dv<float>(m.g1[3], m.g1[3] ? CAP : 0),
            dv<float3>(m.g1[4], m.g1[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g1_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<float3>(m.g2[0], m.g2[0] ? CAP : 0),
            dv<float4>(m.g2[1], m.g2[1] ? CAP : 0),
            dv<float3>(m.g2[2], m.g2[2] ? CAP : 0),
            dv<float>(m.g2[3], m.g2[3] ? CAP : 0),
            dv<float3>(m.g2[4], m.g2[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g2_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<float2>(m.accum, CAP), DeviceVector<float2>{},
            dv<int32_t>(m.bias_steps, CAP),
            m.sh_optim_bits, (int)m.num_sh,
            dv<float4>(m.sh_state_bounds, m.sh_state_bound_f / 4),
            m.bounds_per_splat, dv<uint8_t>(m.sh_value_packed,
                                            m.sh_value_bytes),
            dv<float2>(m.sh_value_bounds, m.sh_value_bound_f / 2),
            m.sh_value_bits, m.bounds_per_splat, (int)m.num_sh_buffer, m.nq,
            seed);
        backend::device_synchronize();
    };

    auto call_add_las = [&](Model& m, int64_t num_add, uint32_t seed) {
        add_splats_with_long_axis_split_tensor(
            N, num_add, /*split_opacity_k=*/0.6f,
            dv<float3>(m.means, CAP), dv<float4>(m.quats, CAP),
            dv<float3>(m.scales, CAP), dv<float>(m.opacs, CAP),
            dv<float3>(m.dc, CAP),
            dv<float3>(m.sh, m.sh ? m.num_sh * CAP : 0),
            dv<float3>(m.g1[0], m.g1[0] ? CAP : 0),
            dv<float4>(m.g1[1], m.g1[1] ? CAP : 0),
            dv<float3>(m.g1[2], m.g1[2] ? CAP : 0),
            dv<float>(m.g1[3], m.g1[3] ? CAP : 0),
            dv<float3>(m.g1[4], m.g1[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g1_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<float3>(m.g2[0], m.g2[0] ? CAP : 0),
            dv<float4>(m.g2[1], m.g2[1] ? CAP : 0),
            dv<float3>(m.g2[2], m.g2[2] ? CAP : 0),
            dv<float>(m.g2[3], m.g2[3] ? CAP : 0),
            dv<float3>(m.g2[4], m.g2[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g2_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<float2>(m.accum, CAP), DeviceVector<float2>{},
            dv<int32_t>(m.bias_steps, CAP),
            m.sh_optim_bits, (int)m.num_sh,
            dv<float4>(m.sh_state_bounds, m.sh_state_bound_f / 4),
            m.bounds_per_splat, dv<uint8_t>(m.sh_value_packed,
                                            m.sh_value_bytes),
            dv<float2>(m.sh_value_bounds, m.sh_value_bound_f / 2),
            m.sh_value_bits, m.bounds_per_splat, (int)m.num_sh_buffer, m.nq,
            seed);
        backend::device_synchronize();
    };

    auto call_relocate_mcmc = [&](Model& m, float min_opacity,
                                  uint32_t seed) {
        relocate_splats_mcmc_tensor(
            N, min_opacity,
            dv<float3>(m.means, CAP), dv<float4>(m.quats, CAP),
            dv<float3>(m.scales, CAP), dv<float>(m.opacs, CAP),
            dv<float3>(m.dc, CAP),
            dv<float3>(m.sh, m.sh ? m.num_sh * CAP : 0),
            dv<float3>(m.g1[0], m.g1[0] ? CAP : 0),
            dv<float4>(m.g1[1], m.g1[1] ? CAP : 0),
            dv<float3>(m.g1[2], m.g1[2] ? CAP : 0),
            dv<float>(m.g1[3], m.g1[3] ? CAP : 0),
            dv<float3>(m.g1[4], m.g1[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g1_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<float3>(m.g2[0], m.g2[0] ? CAP : 0),
            dv<float4>(m.g2[1], m.g2[1] ? CAP : 0),
            dv<float3>(m.g2[2], m.g2[2] ? CAP : 0),
            dv<float>(m.g2[3], m.g2[3] ? CAP : 0),
            dv<float3>(m.g2[4], m.g2[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g2_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<int32_t>(m.bias_steps, CAP), m.sh_optim_bits, (int)m.num_sh,
            dv<float4>(m.sh_state_bounds, m.sh_state_bound_f / 4),
            m.bounds_per_splat, dv<uint8_t>(m.sh_value_packed,
                                            m.sh_value_bytes),
            dv<float2>(m.sh_value_bounds, m.sh_value_bound_f / 2),
            m.sh_value_bits, m.bounds_per_splat, (int)m.num_sh_buffer, m.nq,
            seed);
        backend::device_synchronize();
    };

    auto call_add_mcmc = [&](Model& m, int64_t num_add, float min_opacity,
                             uint32_t seed) {
        add_splats_mcmc_tensor(
            N, num_add, min_opacity,
            dv<float3>(m.means, CAP), dv<float4>(m.quats, CAP),
            dv<float3>(m.scales, CAP), dv<float>(m.opacs, CAP),
            dv<float3>(m.dc, CAP),
            dv<float3>(m.sh, m.sh ? m.num_sh * CAP : 0),
            dv<float3>(m.g1[0], m.g1[0] ? CAP : 0),
            dv<float4>(m.g1[1], m.g1[1] ? CAP : 0),
            dv<float3>(m.g1[2], m.g1[2] ? CAP : 0),
            dv<float>(m.g1[3], m.g1[3] ? CAP : 0),
            dv<float3>(m.g1[4], m.g1[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g1_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<float3>(m.g2[0], m.g2[0] ? CAP : 0),
            dv<float4>(m.g2[1], m.g2[1] ? CAP : 0),
            dv<float3>(m.g2[2], m.g2[2] ? CAP : 0),
            dv<float>(m.g2[3], m.g2[3] ? CAP : 0),
            dv<float3>(m.g2[4], m.g2[4] ? CAP : 0),
            dv<float3>(m.sh_optim_bits == 32 ? (void*)m.g2_sh
                                             : (void*)m.sh_state_packed,
                       0),
            dv<int32_t>(m.bias_steps, CAP), m.sh_optim_bits, (int)m.num_sh,
            dv<float4>(m.sh_state_bounds, m.sh_state_bound_f / 4),
            m.bounds_per_splat, dv<uint8_t>(m.sh_value_packed,
                                            m.sh_value_bytes),
            dv<float2>(m.sh_value_bounds, m.sh_value_bound_f / 2),
            m.sh_value_bits, m.bounds_per_splat, (int)m.num_sh_buffer, m.nq,
            seed);
        backend::device_synchronize();
    };

    // A recycled splat must come back alive. Parity cannot see this on its
    // own: both backends leaving the dead splat where it is compares equal.
    bool revived_ok = true;
    auto check_revived = [&](const Model& m, int64_t idx, const char* what) {
        float sc[3];
        backend::memcpy_sync(sc, m.scales + 3 * idx, sizeof sc,
                             MemcpyKind::DeviceToHost);
        float hi = std::max(std::max(sc[0], sc[1]), sc[2]);
        if (!(hi > -87.336548f)) {
            std::printf("densify_parity: %s left splat %lld dead "
                        "(max log scale %g)\n", what, (long long)idx, hi);
            revived_ok = false;
        }
    };

    // relocate (long-axis split): exactly one relocating splat per config
    {
        // fp32, low-opacity trigger
        Model m = build_model(32, 32, false, false, 15, 15, /*lowopac=*/517,
                              /*nan=*/-1, -1);
        call_relocate_las(m, 0.005f, 11u);
        readback_model(m);
    }
    {
        // 8-bit SH state + 16-bit value codec (FPBO layouts) + non-SH
        // quant, non-finite trigger
        Model m = build_model(8, 16, true, true, 15, 15, -1, /*nan=*/1234, -1);
        call_relocate_las(m, 0.005f, 22u);
        readback_model(m);
    }
    {
        // 4-bit SH state + 8-bit value codec (per-cell layouts), degree
        // warmup (num_sh < num_sh_buffer)
        Model m = build_model(4, 8, false, false, 8, 15, /*lowopac=*/2718,
                              -1, -1);
        call_relocate_las(m, 0.005f, 33u);
        readback_model(m);
    }

    {
        // dead-scale trigger: too small for exp() to stay normal
        Model m = build_model(32, 32, false, false, 15, 15, -1, -1,
                              /*dead_scale=*/1729);
        call_relocate_las(m, 0.005f, 121u);
        check_revived(m, 1729, "relocate_las");
        readback_model(m);
    }

    // add (long-axis split): many deterministic dst splats
    {
        Model m = build_model(32, 32, false, false, 15, 15, -1, -1, -1);
        call_add_las(m, 555, 44u);
        readback_model(m);
    }
    {
        Model m = build_model(8, 16, true, true, 15, 15, -1, -1, -1);
        call_add_las(m, 600, 55u);
        readback_model(m);
    }
    {
        Model m = build_model(4, 8, false, false, 8, 15, -1, -1, -1);
        call_add_las(m, 333, 66u);
        readback_model(m);
    }

    // MCMC relocate: ~10% of splats below the opacity threshold
    auto lower_some_opacs = [&](Model& m) {
        std::vector<float> opacs(CAP);
        backend::memcpy_sync(opacs.data(), m.opacs, CAP * 4,
                             MemcpyKind::DeviceToHost);
        for (int64_t i = 0; i < N; i++)
            if (i % 11 == 0) opacs[i] = uf(-9.f, -7.f);
        backend::memcpy_sync(m.opacs, opacs.data(), CAP * 4,
                             MemcpyKind::HostToDevice);
    };
    {
        Model m = build_model(32, 32, false, false, 15, 15, -1, -1, -1);
        lower_some_opacs(m);
        call_relocate_mcmc(m, 0.005f, 77u);
        readback_model(m);
    }
    {
        Model m = build_model(8, 16, true, true, 15, 15, -1, -1, -1);
        lower_some_opacs(m);
        call_relocate_mcmc(m, 0.005f, 88u);
        readback_model(m);
    }

    {
        // dead-scale trigger on the MCMC path
        Model m = build_model(32, 32, false, false, 15, 15, -1, -1,
                              /*dead_scale=*/1729);
        call_relocate_mcmc(m, 0.005f, 131u);
        check_revived(m, 1729, "relocate_mcmc");
        readback_model(m);
    }

    // MCMC add
    {
        Model m = build_model(32, 32, false, false, 15, 15, -1, -1, -1);
        call_add_mcmc(m, 444, 0.005f, 99u);
        readback_model(m);
    }
    {
        Model m = build_model(4, 8, false, false, 8, 15, -1, -1, -1);
        call_add_mcmc(m, 512, 0.005f, 111u);
        readback_model(m);
    }

    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nf = (int64_t)acc.size(), nl = (int64_t)lacc.size(),
                nc = (int64_t)codes.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)lacc.data(), nl * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)codes.data(), nc * 4);
        std::printf("densify_parity: dumped %lld + %lld floats + %lld codes "
                    "to %s\n",
                    (long long)nf, (long long)nl, (long long)nc, argv[2]);
        return revived_ok ? 0 : 1;
    }

    // Diagnostics: DENSIFY_DUMP_GOT=<path> writes this run's outputs in the
    // ref format before comparing.
    if (const char* dump_got = std::getenv("DENSIFY_DUMP_GOT")) {
        std::ofstream f(dump_got, std::ios::binary);
        int64_t nf = (int64_t)acc.size(), nl = (int64_t)lacc.size(),
                nc = (int64_t)codes.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)lacc.data(), nl * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)codes.data(), nc * 4);
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nl = 0, nc = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)acc.size()) {
        std::fprintf(stderr, "tight count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, acc.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);
    f.read((char*)&nl, 8);
    if (nl != (int64_t)lacc.size()) {
        std::fprintf(stderr, "loose count mismatch: ref %lld vs got %zu\n",
                     (long long)nl, lacc.size());
        return 1;
    }
    std::vector<float> lref(nl);
    f.read((char*)lref.data(), nl * 4);
    f.read((char*)&nc, 8);
    if (nc != (int64_t)codes.size()) {
        std::fprintf(stderr, "code count mismatch: ref %lld vs got %zu\n",
                     (long long)nc, codes.size());
        return 1;
    }
    std::vector<int32_t> refc(nc);
    f.read((char*)refc.data(), nc * 4);

    auto cmp_f = [](const std::vector<float>& got,
                    const std::vector<float>& want, int64_t& viol,
                    double& max_abs) {
        viol = 0;
        max_abs = 0;
        for (size_t i = 0; i < got.size(); i++) {
            bool gfin = std::isfinite(got[i]), wfin = std::isfinite(want[i]);
            if (!gfin || !wfin) {
                if (gfin != wfin) viol++;
                continue;
            }
            double d = std::fabs((double)got[i] - (double)want[i]);
            double tol = 5e-3 + 1e-3 * std::fabs((double)want[i]);
            max_abs = std::max(max_abs, d);
            if (d > tol) viol++;
        }
    };
    int64_t fviol = 0, lviol = 0;
    double fmax = 0, lmax = 0;
    cmp_f(acc, ref, fviol, fmax);
    cmp_f(lacc, lref, lviol, lmax);
    int64_t cviol = 0, max_c = 0;
    for (int64_t i = 0; i < nc; i++) {
        int64_t d = std::llabs((int64_t)codes[i] - (int64_t)refc[i]);
        max_c = std::max(max_c, d);
        if (d > 1) cviol++;
    }
    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double lfrac = nl ? (double)lviol / (double)nl : 0.0;
    double cfrac = nc ? (double)cviol / (double)nc : 0.0;
    std::printf(
        "densify_parity: %lld tight floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld loose floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld codes (max |d| %lld, violations %lld = %.5f%%)\n",
        (long long)nf, fmax, (long long)fviol, 100.0 * ffrac, (long long)nl,
        lmax, (long long)lviol, 100.0 * lfrac, (long long)nc,
        (long long)max_c, (long long)cviol, 100.0 * cfrac);
    bool pass = ffrac <= 1e-3 && lfrac <= 2e-2 && cfrac <= 2e-2 && revived_ok;
    std::printf(pass ? "densify_parity: PASSED\n"
                     : "densify_parity: FAILED\n");
    return pass ? 0 : 1;
}
