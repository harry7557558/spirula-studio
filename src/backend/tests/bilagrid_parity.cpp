// Backend parity tool for the bilateral-grid launch APIs: the five sampler
// families (affine / PPISP / log-linear / depth / normal; uniform + patched
// + per-pixel-coords + packed), TV loss + channel mean, the fused
// TV-Adam/AdaGrad optimizers, the q16 one-shot encode and the small init /
// scatter utilities. The SAME source builds under both backends:
//
//   CUDA build:   ./bilagrid_parity dump ref.bin
//   Vulkan build: ./bilagrid_parity compare ref.bin   (per device)
//
// Ref format: [nf tight floats] [nl loose floats] [nc int32 codes].
//
// Channels: per-pixel deterministic outputs (forward images, input-side
// gradients, tv/channel-mean backward, inits) compare tight. Atomically
// accumulated buffers (v_bilagrid, tv-loss scalar, channel means) and the
// PPISP-family gradients (Slang autodiff vs the hand-written CUDA chain in
// BilagridPpispMathBwd.cuh — analytically equal, rounding differs) sit in
// the LOOSE channel. The fused optimizers' outputs are loose everywhere:
// with tv_weight > 0 the cross-block TV neighbor reads race with value
// writeback on BOTH backends (documented partial-update semantic), so exact
// bit equality is not defined even per backend; small lr keeps the race
// contribution far below the loose tolerance. Quantized packed cells
// compare as integer codes with a +-1 allowance.

#include <kernels/bilagrid/BilagridBindings.h>
#include <engine/EngineInternal.h>
#include <core/Tensor.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

using backend::MemcpyKind;

static std::vector<float> g_tight, g_loose;
static std::vector<int32_t> g_codes;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(
        ((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
T* alloc_zero(int64_t n) {
    int64_t bytes = ((n * (int64_t)sizeof(T) + 3) / 4) * 4;
    T* d = (T*)backend::device_malloc(bytes);
    backend::memset_sync(d, 0, bytes);
    return d;
}

void readback_f(std::vector<float>& acc, const float* d, int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

void readback_codes(std::vector<int32_t>& codes, const void* d,
                    int64_t n_bytes, int code_bits) {
    std::vector<uint8_t> h(n_bytes);
    backend::memcpy_sync(h.data(), d, n_bytes, MemcpyKind::DeviceToHost);
    if (code_bits == 16) {
        for (int64_t i = 0; i < n_bytes / 2; i++)
            codes.push_back(((const uint16_t*)h.data())[i]);
    } else if (code_bits == 8) {
        for (auto v : h) codes.push_back(v);
    } else {
        for (auto v : h) {
            codes.push_back(v & 0xf);
            codes.push_back((v >> 4) & 0xf);
        }
    }
}

struct Rng {
    std::mt19937 rng;
    explicit Rng(uint32_t seed) : rng(seed) {}
    float uf(float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    }
    int ui(int lo, int hi) {  // inclusive
        return lo + (int)(rng() % (uint32_t)(hi - lo + 1));
    }
    std::vector<float> vec(int64_t n, float lo, float hi) {
        std::vector<float> v(n);
        for (auto& x : v) x = uf(lo, hi);
        return v;
    }

    // An upstream gradient with the first half of the pixels at EXACTLY zero,
    // as a loss mask leaves them -- which is the only thing the backward's
    // skip path keys on, and random values never are.
    std::vector<float> vgrad(int64_t n_pix, int c, float lo, float hi) {
        std::vector<float> v = vec(n_pix * c, lo, hi);
        for (int64_t i = 0; i < n_pix / 2 * c; i++) v[i] = 0.0f;
        return v;
    }
};

// A grid + optional q16 encoding (built on-device via
// bilagrid_encode_q16_launch, which is itself under test).
struct Grid {
    int N, C, L, H, W;
    int64_t cells;
    float* fp32 = nullptr;
    uint16_t* q16 = nullptr;
    float2* vbounds = nullptr;

    BilagridReader reader(bool vq) const {
        return vq ? BilagridReader::from_q16(q16, vbounds)
                  : BilagridReader(fp32);
    }
};

Grid make_grid(Rng& r, int N, int C, int L, int H, int W, float lo, float hi,
               bool with_q16) {
    Grid g;
    g.N = N; g.C = C; g.L = L; g.H = H; g.W = W;
    g.cells = (int64_t)N * C * L * H * W;
    g.fp32 = upload(r.vec(g.cells, lo, hi));
    if (with_q16) {
        g.q16 = alloc_zero<uint16_t>(g.cells);
        int64_t n_blocks = (g.cells + 255) / 256;
        g.vbounds = alloc_zero<float2>(n_blocks);
        bilagrid_encode_q16_launch(g.fp32, g.q16, g.vbounds, g.cells,
                                   backend::kDefaultStream);
    }
    return g;
}

/* ========================================================================
 * Affine family
 * ======================================================================== */

void test_affine(Rng& r) {
    const int L = 6, Hg = 7, Wg = 9;

    // uniform fwd + bwd v1 + bwd v2 (fp32 and q16, with/without grid_indices)
    for (int vq = 0; vq < 2; vq++) {
        const int N_img = 3, N_grids = vq ? 5 : 3, h = 33, w = 41;
        Grid g = make_grid(r, N_grids, 12, L, Hg, Wg, -0.8f, 1.2f, vq != 0);
        std::vector<int> gi_h = {4, 0, 2};
        int* gi = vq ? upload(gi_h) : nullptr;

        int64_t np = (int64_t)N_img * h * w;
        float* rgb = upload(r.vec(3 * np, -0.05f, 1.05f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_uniform_sample_forward(g.reader(vq != 0), rgb, out, N_img, L,
                                        Hg, Wg, h, w,
                                        backend::kDefaultStream, gi);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.5f, 0.5f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 12 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_uniform_sample_backward_v1(g.reader(vq != 0), rgb, v_out,
                                            v_grid, v_rgb, N_img, L, Hg, Wg,
                                            h, w, 1, backend::kDefaultStream,
                                            gi);
        readback_f(g_tight, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 12 * L * Hg * Wg);

        float* v_grid2 = alloc_zero<float>((int64_t)N_img * 12 * L * Hg * Wg);
        float* v_rgb2 = alloc_zero<float>(3 * np);
        bilagrid_uniform_sample_backward_v2(g.reader(vq != 0), rgb, v_out,
                                            v_grid2, v_rgb2, N_img, L, Hg, Wg,
                                            h, w, backend::kDefaultStream);
        readback_f(g_tight, v_rgb2, 3 * np);
        readback_f(g_loose, v_grid2, (int64_t)N_img * 12 * L * Hg * Wg);
    }

    // mult > 1 fast path (block-reduce writeback): tiny grid, big image.
    {
        const int N_img = 1, Ls = 4, Hs = 4, Ws = 4, h = 200, w = 224;
        Grid g = make_grid(r, N_img, 12, Ls, Hs, Ws, -0.6f, 1.0f, false);
        int64_t np = (int64_t)N_img * h * w;
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* v_out = upload(r.vgrad(np, 3, -0.2f, 0.2f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 12 * Ls * Hs * Ws);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_uniform_sample_backward_v1(g.reader(false), rgb, v_out,
                                            v_grid, v_rgb, N_img, Ls, Hs, Ws,
                                            h, w, 1, backend::kDefaultStream,
                                            nullptr);
        readback_f(g_tight, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 12 * Ls * Hs * Ws);
    }

    // patched fwd + bwd v1 (mi batching) + bwd v2
    {
        const int N_img = 2, m = 3, h = 20, w = 24, h0 = 45, w0 = 55;
        Grid g = make_grid(r, N_img, 12, L, Hg, Wg, -0.8f, 1.2f, false);
        std::vector<int> off_h;
        Rng ro(777);
        for (int i = 0; i < N_img * m; i++) {
            off_h.push_back(ro.ui(0, w0 - w));
            off_h.push_back(ro.ui(0, h0 - h));
        }
        int* offsets = upload(off_h);
        int64_t np = (int64_t)N_img * m * h * w;
        float* rgb = upload(r.vec(3 * np, -0.05f, 1.05f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_patched_sample_forward(g.reader(false), rgb, offsets, out,
                                        N_img, L, Hg, Wg, m, h, w, h0, w0,
                                        backend::kDefaultStream);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.5f, 0.5f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 12 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_patched_sample_backward_v1(
            g.reader(false), rgb, offsets, v_out, v_grid, v_rgb, N_img, L, Hg,
            Wg, m, h, w, h0, w0, 1, 2, backend::kDefaultStream);
        readback_f(g_tight, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 12 * L * Hg * Wg);

        float* v_grid2 = alloc_zero<float>((int64_t)N_img * 12 * L * Hg * Wg);
        float* v_rgb2 = alloc_zero<float>(3 * np);
        bilagrid_patched_sample_backward_v2(
            g.reader(false), rgb, offsets, v_out, v_grid2, v_rgb2, N_img, L,
            Hg, Wg, m, h, w, h0, w0, backend::kDefaultStream);
        readback_f(g_tight, v_rgb2, 3 * np);
        readback_f(g_loose, v_grid2, (int64_t)N_img * 12 * L * Hg * Wg);
    }

    // per-pixel-coords sample fwd + bwd (with and without v_coords)
    {
        const int N_img = 2, m = 2, h = 18, w = 22;
        Grid g = make_grid(r, N_img, 12, L, Hg, Wg, -0.8f, 1.2f, false);
        int64_t np = (int64_t)N_img * m * h * w;
        float* coords = upload(r.vec(2 * np, 0.0f, 1.0f));
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_sample_forward(g.reader(false), coords, rgb, out, N_img, L,
                                Hg, Wg, m, h, w, backend::kDefaultStream);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.5f, 0.5f));
        for (int with_coords = 0; with_coords < 2; with_coords++) {
            float* v_grid =
                alloc_zero<float>((int64_t)N_img * 12 * L * Hg * Wg);
            float* v_coords =
                with_coords ? alloc_zero<float>(2 * np) : nullptr;
            float* v_rgb = alloc_zero<float>(3 * np);
            bilagrid_sample_backward(g.reader(false), coords, rgb, v_out,
                                     v_grid, v_coords, v_rgb, N_img, L, Hg,
                                     Wg, m, h, w, backend::kDefaultStream);
            readback_f(g_tight, v_rgb, 3 * np);
            if (with_coords) readback_f(g_tight, v_coords, 2 * np);
            readback_f(g_loose, v_grid, (int64_t)N_img * 12 * L * Hg * Wg);
        }
    }
}

/* ========================================================================
 * PPISP family (autodiff-vs-handwritten bwd -> loose gradients)
 * ======================================================================== */

void test_ppisp(Rng& r) {
    const int L = 5, Hg = 6, Wg = 8;

    for (int vq = 0; vq < 2; vq++) {
        const int N_img = 2, N_grids = 2, h = 30, w = 36;
        Grid g = make_grid(r, N_grids, 9, L, Hg, Wg, -0.7f, 0.7f, vq != 0);
        int64_t np = (int64_t)N_img * h * w;
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_ppisp_uniform_sample_forward(g.reader(vq != 0), rgb, out,
                                              N_img, L, Hg, Wg, h, w,
                                              backend::kDefaultStream,
                                              nullptr);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.4f, 0.4f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_ppisp_uniform_sample_backward_v1(
            g.reader(vq != 0), rgb, v_out, v_grid, v_rgb, N_img, L, Hg, Wg, h,
            w, 1, backend::kDefaultStream, nullptr);
        readback_f(g_loose, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 9 * L * Hg * Wg);
    }

    // Negative-intensity pixels: ~half push rgi_out.z below the denominator
    // floor, the one branch where the hand-written CUDA backward and the Slang
    // autodiff one have to be made to agree.
    {
        const int N_img = 2, N_grids = 2, h = 30, w = 36;
        Grid g = make_grid(r, N_grids, 9, L, Hg, Wg, -0.7f, 0.7f, false);
        int64_t np = (int64_t)N_img * h * w;
        float* rgb = upload(r.vec(3 * np, -1.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_ppisp_uniform_sample_forward(g.reader(false), rgb, out,
                                              N_img, L, Hg, Wg, h, w,
                                              backend::kDefaultStream,
                                              nullptr);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.4f, 0.4f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_ppisp_uniform_sample_backward_v1(
            g.reader(false), rgb, v_out, v_grid, v_rgb, N_img, L, Hg, Wg, h,
            w, 1, backend::kDefaultStream, nullptr);
        readback_f(g_loose, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 9 * L * Hg * Wg);
    }

    // patched fwd + bwd v1
    {
        const int N_img = 2, m = 2, h = 16, w = 20, h0 = 40, w0 = 48;
        Grid g = make_grid(r, N_img, 9, L, Hg, Wg, -0.7f, 0.7f, false);
        std::vector<int> off_h;
        Rng ro(888);
        for (int i = 0; i < N_img * m; i++) {
            off_h.push_back(ro.ui(0, w0 - w));
            off_h.push_back(ro.ui(0, h0 - h));
        }
        int* offsets = upload(off_h);
        int64_t np = (int64_t)N_img * m * h * w;
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_ppisp_patched_sample_forward(g.reader(false), rgb, offsets,
                                              out, N_img, L, Hg, Wg, m, h, w,
                                              h0, w0, backend::kDefaultStream);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.4f, 0.4f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_ppisp_patched_sample_backward_v1(
            g.reader(false), rgb, offsets, v_out, v_grid, v_rgb, N_img, L, Hg,
            Wg, m, h, w, h0, w0, 1, 2, backend::kDefaultStream);
        readback_f(g_loose, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 9 * L * Hg * Wg);
    }

    // per-pixel coords sample + packed
    {
        const int N_img = 2, m = 2, h = 14, w = 18;
        Grid g = make_grid(r, N_img, 9, L, Hg, Wg, -0.7f, 0.7f, false);
        int64_t np = (int64_t)N_img * m * h * w;
        float* coords = upload(r.vec(2 * np, 0.0f, 1.0f));
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_ppisp_sample_forward(g.reader(false), coords, rgb, out,
                                      N_img, L, Hg, Wg, m, h, w,
                                      backend::kDefaultStream);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.4f, 0.4f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* v_coords = alloc_zero<float>(2 * np);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_ppisp_sample_backward(g.reader(false), coords, rgb, v_out,
                                       v_grid, v_coords, v_rgb, N_img, L, Hg,
                                       Wg, m, h, w, backend::kDefaultStream);
        readback_f(g_loose, v_rgb, 3 * np);
        readback_f(g_loose, v_coords, 2 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 9 * L * Hg * Wg);

        // packed
        const int nnz = 2500;
        std::vector<int64_t> idx_h(nnz);
        Rng ri(999);
        for (auto& v : idx_h) v = ri.ui(0, N_img - 1);
        int64_t* image_indices = upload(idx_h);
        float* pcoords = upload(r.vec(2 * nnz, 0.0f, 1.0f));
        float* prgb = upload(r.vec(3 * nnz, 0.0f, 1.0f));
        float* pout = alloc_zero<float>(3 * nnz);
        bilagrid_ppisp_packed_sample_forward(g.reader(false), image_indices,
                                             pcoords, prgb, pout, N_img, L,
                                             Hg, Wg, nnz,
                                             backend::kDefaultStream);
        readback_f(g_tight, pout, 3 * nnz);

        float* pv_out = upload(r.vec(3 * nnz, -0.4f, 0.4f));
        float* pv_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* pv_coords = alloc_zero<float>(2 * nnz);
        float* pv_rgb = alloc_zero<float>(3 * nnz);
        bilagrid_ppisp_packed_sample_backward(
            g.reader(false), image_indices, pcoords, prgb, pv_out, pv_grid,
            pv_coords, pv_rgb, N_img, L, Hg, Wg, nnz,
            backend::kDefaultStream);
        readback_f(g_loose, pv_rgb, 3 * nnz);
        readback_f(g_loose, pv_coords, 2 * nnz);
        readback_f(g_loose, pv_grid, (int64_t)N_img * 9 * L * Hg * Wg);
    }
}

/* ========================================================================
 * Log-linear / depth / normal families
 * ======================================================================== */

void test_loglinear(Rng& r) {
    const int L = 5, Hg = 6, Wg = 8, N_img = 2, h = 28, w = 34;
    for (int vq = 0; vq < 2; vq++) {
        Grid g = make_grid(r, N_img, 9, L, Hg, Wg, -0.5f, 0.5f, vq != 0);
        int64_t np = (int64_t)N_img * h * w;
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_loglinear_uniform_sample_forward(
            g.reader(vq != 0), rgb, out, N_img, L, Hg, Wg, h, w,
            backend::kDefaultStream, nullptr);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.4f, 0.4f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_loglinear_uniform_sample_backward_v1(
            g.reader(vq != 0), rgb, v_out, v_grid, v_rgb, N_img, L, Hg, Wg, h,
            w, 1, backend::kDefaultStream, nullptr);
        readback_f(g_tight, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 9 * L * Hg * Wg);
    }
    // patched
    {
        const int m = 2, hp = 15, wp = 17, h0 = 36, w0 = 44;
        Grid g = make_grid(r, N_img, 9, L, Hg, Wg, -0.5f, 0.5f, false);
        std::vector<int> off_h;
        Rng ro(1234);
        for (int i = 0; i < N_img * m; i++) {
            off_h.push_back(ro.ui(0, w0 - wp));
            off_h.push_back(ro.ui(0, h0 - hp));
        }
        int* offsets = upload(off_h);
        int64_t np = (int64_t)N_img * m * hp * wp;
        float* rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_loglinear_patched_sample_forward(
            g.reader(false), rgb, offsets, out, N_img, L, Hg, Wg, m, hp, wp,
            h0, w0, backend::kDefaultStream);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vgrad(np, 3, -0.4f, 0.4f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 9 * L * Hg * Wg);
        float* v_rgb = alloc_zero<float>(3 * np);
        bilagrid_loglinear_patched_sample_backward_v1(
            g.reader(false), rgb, offsets, v_out, v_grid, v_rgb, N_img, L, Hg,
            Wg, m, hp, wp, h0, w0, 1, 2, backend::kDefaultStream);
        readback_f(g_tight, v_rgb, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 9 * L * Hg * Wg);
    }
}

void test_depth(Rng& r) {
    const int L = 5, Hg = 6, Wg = 8, N_img = 2, h = 26, w = 30;
    for (int vq = 0; vq < 2; vq++) {
        Grid g = make_grid(r, N_img, 2, L, Hg, Wg, -0.4f, 0.4f, vq != 0);
        int64_t np = (int64_t)N_img * h * w;
        float* depth = upload(r.vec(np, 0.05f, 20.0f));
        float* scalars = upload(r.vec(N_img, 0.5f, 2.0f));
        float* out = alloc_zero<float>(np);
        bilagrid_depth_uniform_sample_forward(g.reader(vq != 0), depth,
                                              scalars, out, N_img, L, Hg, Wg,
                                              h, w, backend::kDefaultStream,
                                              nullptr);
        readback_f(g_tight, out, np);

        float* v_out = upload(r.vec(np, -0.3f, 0.3f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 2 * L * Hg * Wg);
        float* v_depth = alloc_zero<float>(np);
        bilagrid_depth_uniform_sample_backward_v1(
            g.reader(vq != 0), depth, scalars, v_out, v_grid, v_depth, N_img,
            L, Hg, Wg, h, w, 1, backend::kDefaultStream, nullptr);
        readback_f(g_tight, v_depth, np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 2 * L * Hg * Wg);
    }
    // patched (scalars[0] semantics)
    {
        const int m = 2, hp = 14, wp = 16, h0 = 32, w0 = 36;
        Grid g = make_grid(r, N_img, 2, L, Hg, Wg, -0.4f, 0.4f, false);
        std::vector<int> off_h;
        Rng ro(555);
        for (int i = 0; i < N_img * m; i++) {
            off_h.push_back(ro.ui(0, w0 - wp));
            off_h.push_back(ro.ui(0, h0 - hp));
        }
        int* offsets = upload(off_h);
        int64_t np = (int64_t)N_img * m * hp * wp;
        float* depth = upload(r.vec(np, 0.05f, 20.0f));
        float* scalars = upload(r.vec(1, 0.5f, 2.0f));
        float* out = alloc_zero<float>(np);
        bilagrid_depth_patched_sample_forward(
            g.reader(false), depth, scalars, offsets, out, N_img, L, Hg, Wg,
            m, hp, wp, h0, w0, backend::kDefaultStream);
        readback_f(g_tight, out, np);

        float* v_out = upload(r.vec(np, -0.3f, 0.3f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 2 * L * Hg * Wg);
        float* v_depth = alloc_zero<float>(np);
        bilagrid_depth_patched_sample_backward_v1(
            g.reader(false), depth, scalars, offsets, v_out, v_grid, v_depth,
            N_img, L, Hg, Wg, m, hp, wp, h0, w0, 1, 2,
            backend::kDefaultStream);
        readback_f(g_tight, v_depth, np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 2 * L * Hg * Wg);
    }
}

void test_normal(Rng& r) {
    const int L = 5, Hg = 6, Wg = 8, N_img = 2, h = 26, w = 30;
    for (int vq = 0; vq < 2; vq++) {
        Grid g = make_grid(r, N_img, 3, L, Hg, Wg, -0.6f, 0.6f, vq != 0);
        int64_t np = (int64_t)N_img * h * w;
        float* normal_in = upload(r.vec(3 * np, -1.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_normal_uniform_sample_forward(
            g.reader(vq != 0), normal_in, out, N_img, L, Hg, Wg, h, w,
            backend::kDefaultStream, nullptr);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vec(3 * np, -0.3f, 0.3f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 3 * L * Hg * Wg);
        float* v_normal = alloc_zero<float>(3 * np);
        bilagrid_normal_uniform_sample_backward_v1(
            g.reader(vq != 0), normal_in, v_out, v_grid, v_normal, N_img, L,
            Hg, Wg, h, w, 1, backend::kDefaultStream, nullptr);
        readback_f(g_tight, v_normal, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 3 * L * Hg * Wg);

        // null input-grad path: the engine's depth/normal hooks discard the
        // GT-side grad by passing v_rgb = nullptr, which must SKIP the
        // input-grad kernel (dispatching it would write through null; the
        // Vulkan port once faulted the device here). Grid grads must match
        // the non-null call on freshly zeroed accumulators.
        float* v_grid2 = alloc_zero<float>((int64_t)N_img * 3 * L * Hg * Wg);
        bilagrid_normal_uniform_sample_backward_v1(
            g.reader(vq != 0), normal_in, v_out, v_grid2, nullptr, N_img, L,
            Hg, Wg, h, w, 1, backend::kDefaultStream, nullptr);
        readback_f(g_loose, v_grid2, (int64_t)N_img * 3 * L * Hg * Wg);
    }
    // patched
    {
        const int m = 2, hp = 14, wp = 16, h0 = 32, w0 = 36;
        Grid g = make_grid(r, N_img, 3, L, Hg, Wg, -0.6f, 0.6f, false);
        std::vector<int> off_h;
        Rng ro(666);
        for (int i = 0; i < N_img * m; i++) {
            off_h.push_back(ro.ui(0, w0 - wp));
            off_h.push_back(ro.ui(0, h0 - hp));
        }
        int* offsets = upload(off_h);
        int64_t np = (int64_t)N_img * m * hp * wp;
        float* normal_in = upload(r.vec(3 * np, -1.0f, 1.0f));
        float* out = alloc_zero<float>(3 * np);
        bilagrid_normal_patched_sample_forward(
            g.reader(false), normal_in, offsets, out, N_img, L, Hg, Wg, m, hp,
            wp, h0, w0, backend::kDefaultStream);
        readback_f(g_tight, out, 3 * np);

        float* v_out = upload(r.vec(3 * np, -0.3f, 0.3f));
        float* v_grid = alloc_zero<float>((int64_t)N_img * 3 * L * Hg * Wg);
        float* v_normal = alloc_zero<float>(3 * np);
        bilagrid_normal_patched_sample_backward_v1(
            g.reader(false), normal_in, offsets, v_out, v_grid, v_normal,
            N_img, L, Hg, Wg, m, hp, wp, h0, w0, 1, 2,
            backend::kDefaultStream);
        readback_f(g_tight, v_normal, 3 * np);
        readback_f(g_loose, v_grid, (int64_t)N_img * 3 * L * Hg * Wg);
    }
}

/* ========================================================================
 * TV loss + channel mean
 * ======================================================================== */

void test_tv(Rng& r) {
    const int N = 2, L = 5, Hg = 6, Wg = 7;
    const int channels[3] = {12, 9, 2};
    for (int k = 0; k < 3; k++) {
        int C = channels[k];
        bool vq = (k == 1);  // one q16 reader config
        Grid g = make_grid(r, N, C, L, Hg, Wg, -0.7f, 0.9f, vq);
        int64_t cells = g.cells;

        float* tv = alloc_zero<float>(1);
        tv_loss_forward(g.reader(vq), tv, N, C, L, Hg, Wg,
                        backend::kDefaultStream);
        readback_f(g_loose, tv, 1);

        for (int inplace = 0; inplace < 2; inplace++) {
            float* v_grid = inplace ? upload(r.vec(cells, -0.1f, 0.1f))
                                    : alloc_zero<float>(cells);
            tv_loss_backward(g.reader(vq), 1.7f, v_grid, N, C, L, Hg, Wg,
                             inplace != 0, backend::kDefaultStream);
            readback_f(g_tight, v_grid, cells);
        }

        float* cm = alloc_zero<float>(C);
        channel_mean_forward(g.reader(vq), cm, N, C, L, Hg, Wg,
                             backend::kDefaultStream);
        readback_f(g_loose, cm, C);

        float* v_cm = upload(r.vec(C, -1.0f, 1.0f));
        float* v_grid = alloc_zero<float>(cells);
        channel_mean_backward(g.reader(vq), v_cm, v_grid, N, C, L, Hg, Wg,
                              false, backend::kDefaultStream);
        readback_f(g_tight, v_grid, cells);
    }

    // Many-grid case: the (ni, li) block count N*L/4 lands past the 65535
    // per-dimension dispatch cap, so that axis has to fold across two grid
    // dimensions. L/H/W/C are minimal to keep the table small -- N stands in
    // for a >2^16-image dataset, which at the real 16x16x8 shape would not
    // fit in memory here. Only the head and tail of each grad buffer go into
    // the reference (a fold that drops or aliases blocks shows up at both
    // ends); dumping 6M floats per call would bloat ref.bin for no gain.
    {
        const int N = 400000, L = 2, Hg = 2, Wg = 2, C = 2;
        const int64_t kWindow = 2048;
        // Own Rng: drawing from `r` would shift every later case's golden
        // values, so adding a case here would rewrite the whole reference.
        Rng rb(90210);
        Grid g = make_grid(rb, N, C, L, Hg, Wg, -0.7f, 0.9f, false);
        int64_t cells = g.cells;
        auto ends = [&](const float* d) {
            readback_f(g_tight, d, kWindow);
            readback_f(g_tight, d + (cells - kWindow), kWindow);
        };

        float* tv = alloc_zero<float>(1);
        tv_loss_forward(g.reader(false), tv, N, C, L, Hg, Wg,
                        backend::kDefaultStream);
        readback_f(g_loose, tv, 1);

        float* v_grid = alloc_zero<float>(cells);
        tv_loss_backward(g.reader(false), 1.7f, v_grid, N, C, L, Hg, Wg,
                         false, backend::kDefaultStream);
        ends(v_grid);

        float* cm = alloc_zero<float>(C);
        channel_mean_forward(g.reader(false), cm, N, C, L, Hg, Wg,
                             backend::kDefaultStream);
        readback_f(g_loose, cm, C);

        float* v_cm = upload(rb.vec(C, -1.0f, 1.0f));
        float* v_grid2 = alloc_zero<float>(cells);
        channel_mean_backward(g.reader(false), v_cm, v_grid2, N, C, L, Hg, Wg,
                              false, backend::kDefaultStream);
        ends(v_grid2);
    }
}

/* ========================================================================
 * Fused TV-Adam / TV-AdaGrad
 * ======================================================================== */

void test_fused_optim(Rng& r) {
    // (C, quant_bits, value_quant, use_cam_indices, tv_weight)
    struct Cfg {
        int C, qbits;
        bool vq, cams;
        float tv_w;
    };
    // tv_weight > 0 only in the fp32-state configs: the cross-block TV
    // neighbor race (see file header) shifts values by O(lr * tv) on every
    // backend, which the loose tolerance absorbs — but through the QUANTIZED
    // states' log-domain block bounds (log1p(sqrt(g2)/eps) of near-zero
    // cells) the same shift rescales entire blocks of codes. llvmpipe runs
    // blocks sequentially (systematically post-update neighbor reads), so
    // quantized x tv_weight>0 is not comparable cross-backend at all.
    const Cfg cfgs[4] = {
        {12, 0, false, false, 0.5f},
        {9, 0, false, true, 0.25f},
        {2, 8, false, true, 0.0f},
        {9, 4, true, false, 0.0f},
    };
    const int N_grids = 3, L = 4, Hg = 5, Wg = 6, C_batch = 2;
    const float lr = 1e-2f;

    for (const Cfg& c : cfgs) {
        for (int variant = 0; variant < 2; variant++) {  // 0 adam, 1 adagrad
            Grid g =
                make_grid(r, N_grids, c.C, L, Hg, Wg, -0.6f, 0.8f, c.vq);
            int64_t cells = g.cells;
            int64_t n_blocks = (cells + 255) / 256;

            std::vector<int> cam_h = {2, 2};  // duplicate camera on purpose
            int* cams = c.cams ? upload(cam_h) : nullptr;
            float* image_grad = upload(
                r.vec((int64_t)C_batch * c.C * L * Hg * Wg, -0.3f, 0.3f));

            float* g1 = nullptr;
            float* g2 = nullptr;
            uint8_t* packed = nullptr;
            float4* qb4 = nullptr;
            float2* qb2 = nullptr;
            int64_t packed_bytes = 0;
            if (c.qbits == 0) {
                if (variant == 0) {
                    g1 = upload(r.vec(cells, -0.05f, 0.05f));
                    g2 = upload(r.vec(cells, 0.0f, 0.01f));
                } else {
                    g1 = alloc_zero<float>(cells);  // accum
                }
            } else if (variant == 0) {
                // Adam cells: BITS=8 -> halfword, BITS=4 -> byte. Zero
                // state = codec fixed point.
                packed_bytes = c.qbits == 8 ? cells * 2 : cells;
                packed = alloc_zero<uint8_t>(packed_bytes);
                qb4 = alloc_zero<float4>(n_blocks);
            } else {
                // AdaGrad log codec: BITS=8 -> byte, BITS=4 -> nibble.
                packed_bytes = c.qbits == 8 ? cells : (cells + 1) / 2;
                packed = alloc_zero<uint8_t>(packed_bytes);
                qb2 = alloc_zero<float2>(n_blocks);
            }

            // Two steps (quantized state needs a second step over encoded
            // state to be meaningful).
            for (int step = 7; step <= 8; step++) {
                if (variant == 0) {
                    fused_bilagrid_tv_adam(
                        c.vq ? nullptr : g.fp32, g.q16, g.vbounds, g1, g2,
                        packed, qb4, image_grad, cams, N_grids, C_batch, c.C,
                        L, Hg, Wg, lr, c.tv_w, step, c.qbits != 0, c.qbits,
                        c.vq, backend::kDefaultStream);
                } else {
                    fused_bilagrid_tv_adagrad(
                        c.vq ? nullptr : g.fp32, g.q16, g.vbounds, g1, packed,
                        qb2, image_grad, cams, N_grids, C_batch, c.C, L, Hg,
                        Wg, lr, c.tv_w, c.qbits != 0, c.qbits, c.vq,
                        backend::kDefaultStream);
                }
            }

            // Values
            if (c.vq) {
                readback_codes(g_codes, g.q16, cells * 2, 16);
                readback_f(g_loose, (const float*)g.vbounds, 2 * n_blocks);
            } else {
                readback_f(g_loose, g.fp32, cells);
            }
            // State
            if (c.qbits == 0) {
                readback_f(g_loose, g1, cells);
                if (variant == 0) readback_f(g_loose, g2, cells);
            } else {
                int code_bits = variant == 0 ? (c.qbits == 8 ? 8 : 4)
                                             : (c.qbits == 8 ? 8 : 4);
                readback_codes(g_codes, packed, packed_bytes, code_bits);
                if (variant == 0)
                    readback_f(g_loose, (const float*)qb4, 4 * n_blocks);
                else
                    readback_f(g_loose, (const float*)qb2, 2 * n_blocks);
            }
        }
    }
}

/* ========================================================================
 * Fused-optimizer grid-fold tail
 * ======================================================================== */

// >65535 workgroups folds the flat dispatch into two grid rows
// (KernelCommon.h fold_1d) and the second row ends in padding blocks. Those
// blocks must not touch the per-block quant-bounds arrays past their end —
// an out-of-bounds device write faults the device into a wait that never
// returns. Smallest crossing size, quantized state (the bounds-indexed path),
// tv_weight 0 (see test_fused_optim). Values/codes are sliced (head + a
// tail window spanning both fold rows' real blocks) to keep the ref small;
// the bounds arrays — what the bug clobbered — read back in full.
void test_fold_tail(Rng& r) {
    const int N_grids = 262145, C = 2, L = 2, Hg = 4, Wg = 4, C_batch = 1;
    const int64_t cells = (int64_t)N_grids * C * L * Hg * Wg;  // 16,777,280
    const int64_t n_blocks = (cells + 255) / 256;  // 65,537: 2 fold rows
    const int64_t head = 4096, tail = 8192;
    const float lr = 1e-2f;

    float* image_grad = upload(
        r.vec((int64_t)C_batch * C * L * Hg * Wg, -0.3f, 0.3f));
    for (int variant = 0; variant < 2; variant++) {  // 0 adam, 1 adagrad
        float* grids = upload(r.vec(cells, -0.6f, 0.8f));
        int64_t packed_bytes = variant == 0 ? cells * 2 : cells;
        uint8_t* packed = alloc_zero<uint8_t>(packed_bytes);
        float4* qb4 = variant == 0 ? alloc_zero<float4>(n_blocks) : nullptr;
        float2* qb2 = variant == 0 ? nullptr : alloc_zero<float2>(n_blocks);

        if (variant == 0) {
            fused_bilagrid_tv_adam(grids, nullptr, nullptr, nullptr, nullptr,
                                   packed, qb4, image_grad, nullptr, N_grids,
                                   C_batch, C, L, Hg, Wg, lr, 0.0f, 7, true,
                                   8, false, backend::kDefaultStream);
        } else {
            fused_bilagrid_tv_adagrad(grids, nullptr, nullptr, nullptr,
                                      packed, qb2, image_grad, nullptr,
                                      N_grids, C_batch, C, L, Hg, Wg, lr,
                                      0.0f, true, 8, false,
                                      backend::kDefaultStream);
        }

        if (variant == 0)
            readback_f(g_loose, (const float*)qb4, 4 * n_blocks);
        else
            readback_f(g_loose, (const float*)qb2, 2 * n_blocks);
        readback_f(g_loose, grids, head);
        readback_f(g_loose, grids + (cells - tail), tail);
        readback_codes(g_codes, packed + (packed_bytes - tail), tail, 8);

        backend::device_free(grids);
        backend::device_free(packed);
        if (qb4) backend::device_free(qb4);
        if (qb2) backend::device_free(qb2);
    }
    backend::device_free(image_grad);
}

/* ========================================================================
 * Utilities: q16 encode, identity init, scatter, ppisp init / add
 * ======================================================================== */

void test_utils(Rng& r) {
    // encode_q16 codes + bounds (tail block: cells not a multiple of 256)
    {
        const int64_t cells = 2000;
        float* grids = upload(r.vec(cells, -3.0f, 5.0f));
        uint16_t* q16 = alloc_zero<uint16_t>(cells);
        int64_t n_blocks = (cells + 255) / 256;
        float2* bounds = alloc_zero<float2>(n_blocks);
        bilagrid_encode_q16_launch(grids, q16, bounds, cells,
                                   backend::kDefaultStream);
        readback_codes(g_codes, q16, cells * 2, 16);
        readback_f(g_tight, (const float*)bounds, 2 * n_blocks);
    }
    // affine identity init
    {
        const int N = 2, L = 3, Hg = 4, Wg = 5;
        int64_t n = (int64_t)N * L * Hg * Wg * 12;
        float* grids = upload(r.vec(n, -9.0f, 9.0f));
        bilagrid_affine_identity_init(grids, N, L, Hg, Wg,
                                      backend::kDefaultStream);
        readback_f(g_tight, grids, n);
    }
    // affine identity init, many grids: N*L past the 65535 grid-dimension cap
    // (the old 3D grid over (wi, hi, ni*li) capped this at ~8k images at
    // L=8). Head + tail only, as in the many-grid TV case above.
    {
        const int N = 100000, L = 2, Hg = 2, Wg = 2;
        const int64_t kWindow = 2048;
        Rng ri2(24680);   // own Rng -- see the many-grid TV case in test_tv
        int64_t n = (int64_t)N * L * Hg * Wg * 12;
        float* grids = upload(ri2.vec(n, -9.0f, 9.0f));
        bilagrid_affine_identity_init(grids, N, L, Hg, Wg,
                                      backend::kDefaultStream);
        readback_f(g_tight, grids, kWindow);
        readback_f(g_tight, grids + (n - kWindow), kWindow);
    }
    // scatter floats
    {
        const int n = 500, N_full = 700;
        float* src = upload(r.vec(n, -5.0f, 5.0f));
        std::vector<int> idx_h(n);
        Rng ri(31337);
        // Unique indices so last-writer-wins never triggers (that order is
        // scheduling-dependent on both backends).
        std::vector<int> perm(N_full);
        for (int i = 0; i < N_full; i++) perm[i] = i;
        for (int i = N_full - 1; i > 0; i--)
            std::swap(perm[i], perm[ri.ui(0, i)]);
        for (int i = 0; i < n; i++) idx_h[i] = perm[i];
        int* indices = upload(idx_h);
        float* dst = alloc_zero<float>(N_full);
        bilagrid_scatter_floats(src, indices, n, dst,
                                backend::kDefaultStream);
        readback_f(g_tight, dst, N_full);
    }
    // ppisp default init + add-into-grad
    {
        const int N = 7;
        float* params = upload(r.vec((int64_t)N * 36, -1.0f, 1.0f));
        ppisp_original_default_init(params, N, backend::kDefaultStream);
        readback_f(g_tight, params, (int64_t)N * 36);

        const size_t n = 1000;
        float* src = upload(r.vec(n, -2.0f, 2.0f));
        float* dst = upload(r.vec(n, -2.0f, 2.0f));
        ppisp_add_into_grad(src, dst, n, backend::kDefaultStream);
        readback_f(g_tight, dst, n);
    }
}

/* ======================================================================== */

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    Rng r(260718u);
    test_affine(r);
    test_ppisp(r);
    test_loglinear(r);
    test_depth(r);
    test_normal(r);
    test_tv(r);
    test_fused_optim(r);
    test_fold_tail(r);
    test_utils(r);

    auto write_all = [&](const char* path) {
        std::ofstream f(path, std::ios::binary);
        int64_t nf = (int64_t)g_tight.size(), nl = (int64_t)g_loose.size(),
                nc = (int64_t)g_codes.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)g_tight.data(), nf * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)g_loose.data(), nl * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)g_codes.data(), nc * 4);
    };

    if (dumping) {
        write_all(argv[2]);
        std::printf(
            "bilagrid_parity: dumped %zu + %zu floats + %zu codes to %s\n",
            g_tight.size(), g_loose.size(), g_codes.size(), argv[2]);
        return 0;
    }

    if (const char* dump_got = std::getenv("BILAGRID_DUMP_GOT"))
        write_all(dump_got);

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nl = 0, nc = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)g_tight.size()) {
        std::fprintf(stderr, "tight count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, g_tight.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);
    f.read((char*)&nl, 8);
    if (nl != (int64_t)g_loose.size()) {
        std::fprintf(stderr, "loose count mismatch: ref %lld vs got %zu\n",
                     (long long)nl, g_loose.size());
        return 1;
    }
    std::vector<float> lref(nl);
    f.read((char*)lref.data(), nl * 4);
    f.read((char*)&nc, 8);
    if (nc != (int64_t)g_codes.size()) {
        std::fprintf(stderr, "code count mismatch: ref %lld vs got %zu\n",
                     (long long)nc, g_codes.size());
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
    cmp_f(g_tight, ref, fviol, fmax);
    cmp_f(g_loose, lref, lviol, lmax);
    int64_t cviol = 0, max_c = 0;
    for (int64_t i = 0; i < nc; i++) {
        int64_t d = std::llabs((int64_t)g_codes[i] - (int64_t)refc[i]);
        max_c = std::max(max_c, d);
        if (d > 1) cviol++;
    }
    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double lfrac = nl ? (double)lviol / (double)nl : 0.0;
    double cfrac = nc ? (double)cviol / (double)nc : 0.0;
    std::printf(
        "bilagrid_parity: %lld tight floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld loose floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld codes (max |d| %lld, violations %lld = %.5f%%)\n",
        (long long)nf, fmax, (long long)fviol, 100.0 * ffrac, (long long)nl,
        lmax, (long long)lviol, 100.0 * lfrac, (long long)nc,
        (long long)max_c, (long long)cviol, 100.0 * cfrac);
    bool pass = ffrac <= 2e-3 && lfrac <= 2e-2 && cfrac <= 2e-2;
    std::printf(pass ? "bilagrid_parity: PASSED\n"
                     : "bilagrid_parity: FAILED\n");
    return pass ? 0 : 1;
}
