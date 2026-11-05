// Vulkan implementations of the bilateral-grid launch APIs: the sampler
// family from kernels/bilagrid/BilagridBindings.h (affine / PPISP / log-linear / depth /
// normal, uniform + patched + per-pixel-coords + packed), TV loss + channel
// mean, and the fused TV-Adam/AdaGrad optimizers + q16 encode + identity
// init + scatter from engine/EngineInternal.h.
//
// Device work: shaders/bilagrid_{common,affine,ppisp,loglinear,depth,
// normal,tv,optim}.slang. Each module declares its spec constants in one
// place; every dispatch passes ALL of them in declaration order (spec IDs
// are per-module by declaration order — see backend/vulkan/README.md).
//
// BilagridReader maps to three never-null pointer fields (vkk::or_fallback)
// plus the kValueQuant spec constant; grid_indices-null semantics move to a
// has_grid_indices flag field with an or_fallback'd pointer (llvmpipe
// speculation rule).

#include <kernels/bilagrid/BilagridBindings.h>
#include <engine/EngineInternal.h>
#include <core/Tensor.h>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

constexpr uint32_t kBwdV1BlockX = 8;
constexpr uint32_t kBwdV1BlockY = 8;

struct ReaderPtrs {
    uint64_t fp32, q16, vbounds;
    uint32_t vq;
};

ReaderPtrs unpack_reader(const BilagridReader& r) {
    ReaderPtrs o{};
    o.vq = r.fp32 == nullptr ? 1u : 0u;
    o.fp32 = vkk::or_fallback(r.fp32);
    o.q16 = vkk::or_fallback(r.q16);
    o.vbounds = vkk::or_fallback(r.bounds);
    return o;
}

void check_grid_dims(uint32_t gx, uint32_t gy, uint32_t gz, const char* who) {
    if (gx > 65535u || gy > 65535u || gz > 65535u)
        throw std::runtime_error(std::string("bilagrid: ") + who +
                                 " grid dimension exceeds 65535");
}

// mult_x/mult_y tile decomposition of the *_backward_v1 grid-grad launchers
// (identical integer math to the CUDA launchers; w_eff/h_eff are w/h for
// uniform and w0/h0 for patched).
void v1_mults(int W, int H, int w_eff, int h_eff, int target_tile_size,
              int& mult_x, int& mult_y) {
    mult_x = (2 * w_eff + W) / ((int)kBwdV1BlockX * W * target_tile_size);
    mult_y = (2 * h_eff + H) / ((int)kBwdV1BlockY * H * target_tile_size);
    if (mult_x * mult_y < 4) {
        mult_x = mult_y = 1;
    } else {
        mult_x = std::max(mult_x, 1) * (int)kBwdV1BlockX;
        mult_y = std::max(mult_y, 1) * (int)kBwdV1BlockY;
    }
}

// The grid-grad gather in dispatch_budgeted units: each level's slice reads
// every pixel of its cells' footprints. Keyed by tile size, since the arms the
// selector tries differ in speed per pixel.
double v1_grid_work(int N, int L, int m, int h, int w, bool patched) {
    return (double)N * (patched ? m : 1) * h * w * L;
}
std::string v1_grid_key(const char* entry, int target_tile_size) {
    return std::string(entry) + "#" + std::to_string(target_tile_size);
}

// ---- param struct mirrors (see the corresponding slang modules) ----

struct BgSampleParams {
    uint64_t fp32, q16, vbounds, coords, rgb, out_or_vout, v_bilagrid,
        v_coords, v_rgb;
    int32_t N, L, H, W, m, h, w;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BgSampleParams) == 9 * 8 + 10 * 4, "layout");

struct BgUniformParams {
    uint64_t fp32, q16, vbounds, rgb, output, offsets, grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0, has_grid_indices;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BgUniformParams) == 7 * 8 + 12 * 4, "layout");

struct BgV1GridParams {
    uint64_t rgb, v_output, v_bilagrid, offsets, grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0;
    int32_t mult_x, mult_y, m_batch_stride, has_grid_indices, _pad0;
};
static_assert(sizeof(BgV1GridParams) == 5 * 8 + 14 * 4, "layout");

struct BgV1RgbParams {
    uint64_t fp32, q16, vbounds, rgb, v_output, v_rgb, offsets, grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0, has_grid_indices;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BgV1RgbParams) == 8 * 8 + 12 * 4, "layout");

struct BgV2Params {
    uint64_t fp32, q16, vbounds, rgb, v_output, v_bilagrid, v_rgb, offsets,
        grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0, has_grid_indices;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BgV2Params) == 9 * 8 + 12 * 4, "layout");

struct BpSampleParams {
    uint64_t fp32, q16, vbounds, image_indices, coords, rgb, out_or_vout,
        v_bilagrid, v_coords, v_rgb;
    int32_t N, L, H, W, m, h, w;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BpSampleParams) == 10 * 8 + 10 * 4, "layout");

// PPISP / loglinear / normal share the affine uniform + v1-rgb field sets
// (different slang modules, same layout); depth adds scalars.
struct BpV1GridParams {
    uint64_t fp32, q16, vbounds, rgb, v_output, v_bilagrid, offsets,
        grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0;
    int32_t mult_x, mult_y, m_batch_stride, has_grid_indices, _pad0;
};
static_assert(sizeof(BpV1GridParams) == 8 * 8 + 14 * 4, "layout");

// PPISP backward v2 (fused scatter). Uniform only + grid_indices; image-grad
// always on. Mirrors slang bilagrid_ppisp.BpV2Params.
struct BpV2Params {
    uint64_t fp32, q16, vbounds, rgb, v_output, v_bilagrid, v_rgb, grid_indices;
    int32_t N, L, H, W, h, w, has_grid_indices;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BpV2Params) == 8 * 8 + 10 * 4, "layout");

// Depth (2-ch) / normal (3-ch) backward v2 (scatter, grid-grad only). Mirror
// slang bilagrid_depth.BdV2Params / bilagrid_normal.BnV2Params.
struct BdV2Params {
    uint64_t fp32, q16, vbounds, depth, scalars, v_output, v_bilagrid,
        grid_indices;
    int32_t N, L, H, W, h, w, has_grid_indices;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BdV2Params) == 8 * 8 + 10 * 4, "layout");

struct BnV2Params {
    uint64_t fp32, q16, vbounds, normal_in, v_output, v_bilagrid, grid_indices;
    int32_t N, L, H, W, h, w, has_grid_indices;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BnV2Params) == 7 * 8 + 10 * 4, "layout");

// Log-linear backward v2 (fused scatter, image-grad on). Mirror slang
// bilagrid_loglinear.BlV2Params (same field set as PPISP's BpV2Params).
struct BlV2Params {
    uint64_t fp32, q16, vbounds, rgb, v_output, v_bilagrid, v_rgb, grid_indices;
    int32_t N, L, H, W, h, w, has_grid_indices;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BlV2Params) == 8 * 8 + 10 * 4, "layout");

struct BdUniformParams {
    uint64_t fp32, q16, vbounds, depth, scalars, output, offsets, grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0, has_grid_indices;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(BdUniformParams) == 8 * 8 + 12 * 4, "layout");

struct BdV1GridParams {
    uint64_t fp32, q16, vbounds, depth, scalars, v_output, v_bilagrid, offsets,
        grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0;
    int32_t mult_x, mult_y, m_batch_stride, has_grid_indices, _pad0;
};
static_assert(sizeof(BdV1GridParams) == 9 * 8 + 14 * 4, "layout");

struct BdV1DepthParams {
    uint64_t fp32, q16, vbounds, depth, scalars, v_output, v_depth, offsets,
        grid_indices;
    int32_t N, L, H, W, m, h, w, h0, w0, has_grid_indices;
    uint32_t total, wgs_per_row, _pad0;
};
static_assert(sizeof(BdV1DepthParams) == 9 * 8 + 13 * 4 + 4 /*pad*/, "layout");

struct TvParams {
    uint64_t fp32, q16, vbounds, out_buf;
    int32_t N, C, L, H, W;
    float v_tv_loss;
    int32_t inplace, nl_per_row;
};
static_assert(sizeof(TvParams) == 4 * 8 + 8 * 4, "layout");

struct ChannelMeanParams {
    uint64_t fp32, q16, vbounds, channel_mean, v_bilagrid;
    int32_t N, C, L, H, W, inplace, nl_per_row, _pad0;
};
static_assert(sizeof(ChannelMeanParams) == 5 * 8 + 8 * 4, "layout");

struct EncodeQ16Params {
    uint64_t grids, grids_q16, value_bounds;
    uint32_t total_cells, wgs_per_row;
};
static_assert(sizeof(EncodeQ16Params) == 3 * 8 + 2 * 4, "layout");

struct AffineInitParams {
    uint64_t grids;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(AffineInitParams) == 8 + 2 * 4, "layout");

struct ScatterFloatsParams {
    uint64_t src, indices, dst;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(ScatterFloatsParams) == 3 * 8 + 2 * 4, "layout");

struct BgOptimParams {
    uint64_t grids, grids_q16, value_bounds, g1_f, g2_f, packed, quant_bounds4,
        quant_bounds2, image_grad, cam_indices;
    int32_t N_grids, C_batch, C, L, H, W;
    float lr, tv_weight;
    int32_t adam_step, has_cam_indices;
    uint32_t total_cells, wgs_per_row;
};
static_assert(sizeof(BgOptimParams) == 10 * 8 + 12 * 4, "layout");

// Fill the shared (uniform + patched) sampler param fields. `patched`
// selects the layout; unused pointers route through or_fallback.
template <typename P>
void fill_uniform(P& p, const BilagridReader& br, const void* in_buf,
                  const void* out_buf, const int* offsets,
                  const int* grid_indices, int N, int L, int H, int W, int m,
                  int h, int w, int h0, int w0, bool patched, int64_t total) {
    ReaderPtrs r = unpack_reader(br);
    p.fp32 = r.fp32;
    p.q16 = r.q16;
    p.vbounds = r.vbounds;
    p.N = N; p.L = L; p.H = H; p.W = W;
    p.m = patched ? m : 1;
    p.h = h; p.w = w;
    p.h0 = patched ? h0 : 1;
    p.w0 = patched ? w0 : 1;
    p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    (void)in_buf; (void)out_buf; (void)offsets;
}

backend::vk::SpecList spec2(uint32_t vq, bool patched) {
    return backend::vk::SpecList{vq, patched ? 1u : 0u};
}

}  // namespace

/* ========================================================================
 * Affine (12-channel) family
 * ======================================================================== */

void bilagrid_sample_forward(
    BilagridReader bilagrid, const float* coords, const float* rgb,
    float* output, int N, int L, int H, int W, int m, int h, int w,
    backend::Stream stream
) {
    (void)stream;
    ReaderPtrs r = unpack_reader(bilagrid);
    BgSampleParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.coords = (uint64_t)coords;
    p.rgb = (uint64_t)rgb;
    p.out_or_vout = (uint64_t)output;
    p.v_bilagrid = vkk::or_fallback(nullptr);
    p.v_coords = vkk::or_fallback(nullptr);
    p.v_rgb = vkk::or_fallback(nullptr);
    p.N = N; p.L = L; p.H = H; p.W = W; p.m = m; p.h = h; p.w = w;
    int64_t total = (int64_t)N * m * h * w;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted("bilagrid_affine.bilagrid_sample_fwd",
                                backend::vk::SpecList{r.vq, 0u, 0u}, total, 256,
                                &p, sizeof(p), &p.wgs_per_row);
}

void bilagrid_sample_backward(
    BilagridReader bilagrid, const float* coords, const float* rgb,
    const float* v_output, float* v_bilagrid, float* v_coords, float* v_rgb,
    int N, int L, int H, int W, int m, int h, int w, backend::Stream stream
) {
    (void)stream;
    ReaderPtrs r = unpack_reader(bilagrid);
    BgSampleParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.coords = (uint64_t)coords;
    p.rgb = (uint64_t)rgb;
    p.out_or_vout = (uint64_t)v_output;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.v_coords = vkk::or_fallback(v_coords);
    p.v_rgb = (uint64_t)v_rgb;
    p.N = N; p.L = L; p.H = H; p.W = W; p.m = m; p.h = h; p.w = w;
    int64_t total = (int64_t)N * m * h * w;
    p.total = (uint32_t)total;
    uint32_t coords_grad = v_coords != nullptr ? 1u : 0u;
    vkk::dispatch_flat("bilagrid_affine.bilagrid_sample_bwd",
                       backend::vk::SpecList{r.vq, 0u, coords_grad}, total,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

namespace {

void launch_affine_fwd(
    BilagridReader bilagrid, PixelPtr rgb, PixelOut output,
    const int* offsets, const int* grid_indices, int N, int L, int H, int W,
    int m, int h, int w, int h0, int w0, bool patched
) {
    int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
    BgUniformParams p{};
    fill_uniform(p, bilagrid, rgb.p, output.p, offsets, grid_indices, N, L, H,
                 W, m, h, w, h0, w0, patched, total);
    p.rgb = (uint64_t)rgb.p;
    p.output = (uint64_t)output.p;
    p.offsets = vkk::or_fallback(offsets);
    p.grid_indices = vkk::or_fallback(grid_indices);
    ReaderPtrs r = unpack_reader(bilagrid);
    vkk::dispatch_flat_budgeted(
        "bilagrid_affine.bilagrid_affine_fwd",
        backend::vk::SpecList{r.vq, patched ? 1u : 0u, 0u, (uint32_t)rgb.f,
                              (uint32_t)output.f},
        total, 256, &p, sizeof(p), &p.wgs_per_row);
}

void launch_affine_bwd_v1(
    BilagridReader bilagrid, PixelPtr rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0, int target_tile_size,
    int mi_batch_size, const int* grid_indices, bool patched
) {
    ReaderPtrs r = unpack_reader(bilagrid);
    // grid-grad kernel
    {
        int mult_x, mult_y;
        v1_mults(W, H, patched ? w0 : w, patched ? h0 : h, target_tile_size,
                 mult_x, mult_y);
        int num_m_batches =
            patched ? (m + mi_batch_size - 1) / mi_batch_size : 1;
        uint32_t gx = (uint32_t)((W * mult_x + kBwdV1BlockX - 1) / kBwdV1BlockX);
        uint32_t gy = (uint32_t)((H * mult_y + kBwdV1BlockY - 1) / kBwdV1BlockY);
        uint32_t gz = (uint32_t)(patched ? N * num_m_batches * L : N * L);
        check_grid_dims(gx, gy, gz, "affine_bwd_v1_grid");

        BgV1GridParams p{};
        p.rgb = (uint64_t)rgb.p;
        p.v_output = (uint64_t)v_output;
        p.v_bilagrid = (uint64_t)v_bilagrid;
        p.offsets = vkk::or_fallback(offsets);
        p.grid_indices = vkk::or_fallback(grid_indices);
        p.N = N; p.L = L; p.H = H; p.W = W;
        p.m = patched ? m : 1;
        p.h = h; p.w = w;
        p.h0 = patched ? h0 : 1;
        p.w0 = patched ? w0 : 1;
        p.mult_x = mult_x;
        p.mult_y = mult_y;
        p.m_batch_stride = num_m_batches;
        p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
        const std::string key = v1_grid_key(
            "bilagrid_affine.bilagrid_affine_bwd_v1_grid", target_tile_size);
        vkk::dispatch_budgeted("bilagrid_affine.bilagrid_affine_bwd_v1_grid",
                               backend::vk::SpecList{r.vq, patched ? 1u : 0u,
                                                     0u, (uint32_t)rgb.f},
                               gx, gy, gz, &p, sizeof(p),
                               v1_grid_work(N, L, m, h, w, patched),
                               key.c_str());
    }
    // rgb-grad kernel
    {
        int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
        BgV1RgbParams p{};
        p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
        p.rgb = (uint64_t)rgb.p;
        p.v_output = (uint64_t)v_output;
        p.v_rgb = (uint64_t)v_rgb;
        p.offsets = vkk::or_fallback(offsets);
        p.grid_indices = vkk::or_fallback(grid_indices);
        p.N = N; p.L = L; p.H = H; p.W = W;
        p.m = patched ? m : 1;
        p.h = h; p.w = w;
        p.h0 = patched ? h0 : 1;
        p.w0 = patched ? w0 : 1;
        p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
        p.total = (uint32_t)total;
        vkk::dispatch_flat_budgeted(
            "bilagrid_affine.bilagrid_affine_bwd_v1_rgb",
            backend::vk::SpecList{r.vq, patched ? 1u : 0u, 0u,
                                  (uint32_t)rgb.f},
            total, 256, &p, sizeof(p), &p.wgs_per_row);
    }
}

void launch_affine_bwd_v2(
    BilagridReader bilagrid, PixelPtr rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0, bool patched,
    const int* grid_indices
) {
    ReaderPtrs r = unpack_reader(bilagrid);
    int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
    BgV2Params p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.rgb = (uint64_t)rgb.p;
    p.v_output = (uint64_t)v_output;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.v_rgb = (uint64_t)v_rgb;
    p.offsets = vkk::or_fallback(offsets);
    p.grid_indices = vkk::or_fallback(grid_indices);
    p.N = N; p.L = L; p.H = H; p.W = W;
    p.m = patched ? m : 1;
    p.h = h; p.w = w;
    p.h0 = patched ? h0 : 1;
    p.w0 = patched ? w0 : 1;
    p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted(
        "bilagrid_affine.bilagrid_affine_bwd_v2",
        backend::vk::SpecList{r.vq, patched ? 1u : 0u, 0u, (uint32_t)rgb.f},
        total, 256, &p, sizeof(p), &p.wgs_per_row);
}

}  // namespace

void bilagrid_uniform_sample_forward(
    BilagridReader bilagrid, PixelPtr rgb, PixelOut output, int N, int L,
    int H, int W, int h, int w, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_affine_fwd(bilagrid, rgb, output, nullptr, grid_indices, N, L, H,
                      W, 1, h, w, 1, 1, false);
}

void bilagrid_patched_sample_forward(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    float* output, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, backend::Stream stream
) {
    (void)stream;
    launch_affine_fwd(bilagrid, rgb, output, offsets, nullptr, N, L, H, W, m,
                      h, w, h0, w0, true);
}

void bilagrid_uniform_sample_backward_v1(
    BilagridReader bilagrid, PixelPtr rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    const int target_tile_size, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_affine_bwd_v1(bilagrid, rgb, nullptr, v_output, v_bilagrid, v_rgb,
                         N, L, H, W, 1, h, w, 1, 1, target_tile_size, 1,
                         grid_indices, false);
}

void bilagrid_patched_sample_backward_v1(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0,
    const int target_tile_size, const int mi_batch_size,
    backend::Stream stream
) {
    (void)stream;
    launch_affine_bwd_v1(bilagrid, rgb, offsets, v_output, v_bilagrid, v_rgb,
                         N, L, H, W, m, h, w, h0, w0, target_tile_size,
                         mi_batch_size, nullptr, true);
}

void bilagrid_uniform_sample_backward_v2(
    BilagridReader bilagrid, PixelPtr rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    launch_affine_bwd_v2(bilagrid, rgb, nullptr, v_output, v_bilagrid, v_rgb,
                         N, L, H, W, 1, h, w, 1, 1, false, grid_indices);
}

void bilagrid_patched_sample_backward_v2(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0, backend::Stream stream
) {
    (void)stream;
    launch_affine_bwd_v2(bilagrid, rgb, offsets, v_output, v_bilagrid, v_rgb,
                         N, L, H, W, m, h, w, h0, w0, true, nullptr);
}

/* ========================================================================
 * PPISP (9-channel) family
 * ======================================================================== */

namespace {

void launch_ppisp_sample_fwd_bwd(
    BilagridReader bilagrid, const int64_t* image_indices, const float* coords,
    const float* rgb, const float* out_or_vout, float* v_bilagrid,
    float* v_coords, float* v_rgb, int N, int L, int H, int W, int m, int h,
    int w, int64_t total, bool packed, bool bwd
) {
    ReaderPtrs r = unpack_reader(bilagrid);
    BpSampleParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.image_indices = vkk::or_fallback(image_indices);
    p.coords = (uint64_t)coords;
    p.rgb = (uint64_t)rgb;
    p.out_or_vout = (uint64_t)out_or_vout;
    p.v_bilagrid = vkk::or_fallback(v_bilagrid);
    p.v_coords = vkk::or_fallback(v_coords);
    p.v_rgb = vkk::or_fallback(v_rgb);
    p.N = N; p.L = L; p.H = H; p.W = W; p.m = m; p.h = h; p.w = w;
    p.total = (uint32_t)total;
    uint32_t coords_grad = (bwd && v_coords != nullptr) ? 1u : 0u;
    backend::vk::SpecList spec{r.vq, 0u, coords_grad, packed ? 1u : 0u};
    vkk::dispatch_flat_budgeted(
        bwd ? "bilagrid_ppisp.bilagrid_ppisp_sample_bwd"
            : "bilagrid_ppisp.bilagrid_ppisp_sample_fwd",
        spec, total, 256, &p, sizeof(p), &p.wgs_per_row);
}

}  // namespace

void bilagrid_ppisp_sample_forward(
    BilagridReader bilagrid, const float* coords, const float* rgb,
    float* output, int N, int L, int H, int W, int m, int h, int w,
    backend::Stream stream
) {
    (void)stream;
    launch_ppisp_sample_fwd_bwd(bilagrid, nullptr, coords, rgb, output,
                                nullptr, nullptr, nullptr, N, L, H, W, m, h,
                                w, (int64_t)N * m * h * w, false, false);
}

void bilagrid_ppisp_sample_backward(
    BilagridReader bilagrid, const float* coords, const float* rgb,
    const float* v_output, float* v_bilagrid, float* v_coords, float* v_rgb,
    int N, int L, int H, int W, int m, int h, int w, backend::Stream stream
) {
    (void)stream;
    launch_ppisp_sample_fwd_bwd(bilagrid, nullptr, coords, rgb, v_output,
                                v_bilagrid, v_coords, v_rgb, N, L, H, W, m, h,
                                w, (int64_t)N * m * h * w, false, true);
}

void bilagrid_ppisp_packed_sample_forward(
    BilagridReader bilagrid, const int64_t* image_indices, const float* coords,
    const float* rgb, float* output, int N, int L, int H, int W, int nnz,
    backend::Stream stream
) {
    (void)stream;
    launch_ppisp_sample_fwd_bwd(bilagrid, image_indices, coords, rgb, output,
                                nullptr, nullptr, nullptr, N, L, H, W, 1, 1,
                                1, nnz, true, false);
}

void bilagrid_ppisp_packed_sample_backward(
    BilagridReader bilagrid, const int64_t* image_indices, const float* coords,
    const float* rgb, const float* v_output, float* v_bilagrid,
    float* v_coords, float* v_rgb, int N, int L, int H, int W, int nnz,
    backend::Stream stream
) {
    (void)stream;
    launch_ppisp_sample_fwd_bwd(bilagrid, image_indices, coords, rgb,
                                v_output, v_bilagrid, v_coords, v_rgb, N, L,
                                H, W, 1, 1, 1, nnz, true, true);
}

namespace {

// Shared launcher for the PPISP / log-linear / normal uniform+patched
// forward and backward_v1 pairs (same param layouts, per-family modules).
struct FamilyEntries {
    const char* fwd;
    const char* v1_grid;
    const char* v1_rgb;
};

void launch_family_fwd(
    const FamilyEntries& e, BilagridReader bilagrid, PixelPtr in_buf,
    PixelOut output, const int* offsets, const int* grid_indices, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0, bool patched
) {
    int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
    BgUniformParams p{};
    fill_uniform(p, bilagrid, in_buf.p, output.p, offsets, grid_indices, N, L,
                 H, W, m, h, w, h0, w0, patched, total);
    p.rgb = (uint64_t)in_buf.p;
    p.output = (uint64_t)output.p;
    p.offsets = vkk::or_fallback(offsets);
    p.grid_indices = vkk::or_fallback(grid_indices);
    ReaderPtrs r = unpack_reader(bilagrid);
    const uint32_t pt = patched ? 1u : 0u;
    const uint32_t fi = (uint32_t)in_buf.f, fo = (uint32_t)output.f;
    const std::string entry(e.fwd);
    backend::vk::SpecList spec =
        entry.rfind("bilagrid_ppisp.", 0) == 0
            ? backend::vk::SpecList{r.vq, pt, 0u, 0u, fi, fo}
        : entry.rfind("bilagrid_loglinear.", 0) == 0
            ? backend::vk::SpecList{r.vq, pt, fi, fo}
            : backend::vk::SpecList{r.vq, pt};
    vkk::dispatch_flat_budgeted(e.fwd, spec, total, 256, &p, sizeof(p),
                                &p.wgs_per_row);
}

void launch_family_bwd_v1(
    const FamilyEntries& e, BilagridReader bilagrid, PixelPtr in_buf,
    const int* offsets, const float* v_output, float* v_bilagrid,
    float* v_in, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, int target_tile_size, int mi_batch_size, const int* grid_indices,
    bool patched
) {
    ReaderPtrs r = unpack_reader(bilagrid);
    const uint32_t pt = patched ? 1u : 0u, fi = (uint32_t)in_buf.f;
    const std::string entry(e.v1_grid);
    backend::vk::SpecList spec =
        entry.rfind("bilagrid_ppisp.", 0) == 0
            ? backend::vk::SpecList{r.vq, pt, 0u, 0u, fi}
        : entry.rfind("bilagrid_loglinear.", 0) == 0
            ? backend::vk::SpecList{r.vq, pt, fi}
            : backend::vk::SpecList{r.vq, pt};
    // grid-grad kernel
    {
        int mult_x, mult_y;
        v1_mults(W, H, patched ? w0 : w, patched ? h0 : h, target_tile_size,
                 mult_x, mult_y);
        int num_m_batches =
            patched ? (m + mi_batch_size - 1) / mi_batch_size : 1;
        uint32_t gx = (uint32_t)((W * mult_x + kBwdV1BlockX - 1) / kBwdV1BlockX);
        uint32_t gy = (uint32_t)((H * mult_y + kBwdV1BlockY - 1) / kBwdV1BlockY);
        const int layers = entry.rfind("bilagrid_ppisp.", 0) == 0 ? (L + 1) / 2 : L;
        uint32_t gz = (uint32_t)(patched ? N * num_m_batches * layers : N * layers);
        check_grid_dims(gx, gy, gz, e.v1_grid);

        BpV1GridParams p{};
        p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
        p.rgb = (uint64_t)in_buf.p;
        p.v_output = (uint64_t)v_output;
        p.v_bilagrid = (uint64_t)v_bilagrid;
        p.offsets = vkk::or_fallback(offsets);
        p.grid_indices = vkk::or_fallback(grid_indices);
        p.N = N; p.L = L; p.H = H; p.W = W;
        p.m = patched ? m : 1;
        p.h = h; p.w = w;
        p.h0 = patched ? h0 : 1;
        p.w0 = patched ? w0 : 1;
        p.mult_x = mult_x;
        p.mult_y = mult_y;
        p.m_batch_stride = num_m_batches;
        p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
        const std::string key = v1_grid_key(e.v1_grid, target_tile_size);
        vkk::dispatch_budgeted(e.v1_grid, spec, gx, gy, gz, &p, sizeof(p),
                               v1_grid_work(N, L, m, h, w, patched),
                               key.c_str());
    }
    // input-grad kernel. null v_in = skip (the engine's depth/normal hooks
    // discard the GT-side grad; the CUDA launchers guard the same way) --
    // dispatching anyway would write C*h*w*12 bytes through a null device
    // address, which faults the device into a wait that never returns.
    if (v_in != nullptr) {
        int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
        BgV1RgbParams p{};
        p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
        p.rgb = (uint64_t)in_buf.p;
        p.v_output = (uint64_t)v_output;
        p.v_rgb = (uint64_t)v_in;
        p.offsets = vkk::or_fallback(offsets);
        p.grid_indices = vkk::or_fallback(grid_indices);
        p.N = N; p.L = L; p.H = H; p.W = W;
        p.m = patched ? m : 1;
        p.h = h; p.w = w;
        p.h0 = patched ? h0 : 1;
        p.w0 = patched ? w0 : 1;
        p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
        p.total = (uint32_t)total;
        vkk::dispatch_flat_budgeted(e.v1_rgb, spec, total, 256, &p, sizeof(p),
                                    &p.wgs_per_row);
    }
}

const FamilyEntries kPpispEntries = {
    "bilagrid_ppisp.bilagrid_ppisp_fwd",
    "bilagrid_ppisp.bilagrid_ppisp_bwd_v1_grid",
    "bilagrid_ppisp.bilagrid_ppisp_bwd_v1_rgb",
};
const FamilyEntries kLoglinearEntries = {
    "bilagrid_loglinear.bilagrid_loglinear_fwd",
    "bilagrid_loglinear.bilagrid_loglinear_bwd_v1_grid",
    "bilagrid_loglinear.bilagrid_loglinear_bwd_v1_rgb",
};
const FamilyEntries kNormalEntries = {
    "bilagrid_normal.bilagrid_normal_fwd",
    "bilagrid_normal.bilagrid_normal_bwd_v1_grid",
    "bilagrid_normal.bilagrid_normal_bwd_v1_normal",
};

}  // namespace

void bilagrid_ppisp_uniform_sample_forward(
    BilagridReader bilagrid, PixelPtr rgb, PixelOut output, int N, int L,
    int H, int W, int h, int w, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_family_fwd(kPpispEntries, bilagrid, rgb, output, nullptr,
                      grid_indices, N, L, H, W, 1, h, w, 1, 1, false);
}

void bilagrid_ppisp_patched_sample_forward(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    float* output, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, backend::Stream stream
) {
    (void)stream;
    launch_family_fwd(kPpispEntries, bilagrid, rgb, output, offsets, nullptr,
                      N, L, H, W, m, h, w, h0, w0, true);
}

void bilagrid_ppisp_uniform_sample_backward_v1(
    BilagridReader bilagrid, PixelPtr rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    const int target_tile_size, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_family_bwd_v1(kPpispEntries, bilagrid, rgb, nullptr, v_output,
                         v_bilagrid, v_rgb, N, L, H, W, 1, h, w, 1, 1,
                         target_tile_size, 1, grid_indices, false);
}

void bilagrid_ppisp_uniform_sample_backward_v2(
    BilagridReader bilagrid, PixelPtr rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    ReaderPtrs r = unpack_reader(bilagrid);
    int64_t total = (int64_t)N * h * w;
    BpV2Params p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.rgb = (uint64_t)rgb.p;
    p.v_output = (uint64_t)v_output;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.v_rgb = (uint64_t)v_rgb;
    p.grid_indices = vkk::or_fallback(grid_indices);
    p.N = N; p.L = L; p.H = H; p.W = W; p.h = h; p.w = w;
    p.has_grid_indices = (grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted(
        "bilagrid_ppisp.bilagrid_ppisp_bwd_v2",
        backend::vk::SpecList{r.vq, 0u, 0u, 0u, (uint32_t)rgb.f}, total, 256,
        &p, sizeof(p), &p.wgs_per_row);
}

void bilagrid_ppisp_patched_sample_backward_v1(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0,
    const int target_tile_size, const int mi_batch_size,
    backend::Stream stream
) {
    (void)stream;
    launch_family_bwd_v1(kPpispEntries, bilagrid, rgb, offsets, v_output,
                         v_bilagrid, v_rgb, N, L, H, W, m, h, w, h0, w0,
                         target_tile_size, mi_batch_size, nullptr, true);
}

/* ========================================================================
 * Log-linear (9-channel) family
 * ======================================================================== */

void bilagrid_loglinear_uniform_sample_forward(
    BilagridReader bilagrid, PixelPtr rgb, PixelOut output, int N, int L,
    int H, int W, int h, int w, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_family_fwd(kLoglinearEntries, bilagrid, rgb, output, nullptr,
                      grid_indices, N, L, H, W, 1, h, w, 1, 1, false);
}

void bilagrid_loglinear_patched_sample_forward(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    float* output, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, backend::Stream stream
) {
    (void)stream;
    launch_family_fwd(kLoglinearEntries, bilagrid, rgb, output, offsets,
                      nullptr, N, L, H, W, m, h, w, h0, w0, true);
}

void bilagrid_loglinear_uniform_sample_backward_v1(
    BilagridReader bilagrid, PixelPtr rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    const int target_tile_size, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_family_bwd_v1(kLoglinearEntries, bilagrid, rgb, nullptr, v_output,
                         v_bilagrid, v_rgb, N, L, H, W, 1, h, w, 1, 1,
                         target_tile_size, 1, grid_indices, false);
}

void bilagrid_loglinear_uniform_sample_backward_v2(
    BilagridReader bilagrid, PixelPtr rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    ReaderPtrs r = unpack_reader(bilagrid);
    int64_t total = (int64_t)N * h * w;
    BlV2Params p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.rgb = (uint64_t)rgb.p;
    p.v_output = (uint64_t)v_output;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.v_rgb = (uint64_t)v_rgb;
    p.grid_indices = vkk::or_fallback(grid_indices);
    p.N = N; p.L = L; p.H = H; p.W = W; p.h = h; p.w = w;
    p.has_grid_indices = (grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted(
        "bilagrid_loglinear.bilagrid_loglinear_bwd_v2",
        backend::vk::SpecList{r.vq, 0u, (uint32_t)rgb.f}, total, 256, &p,
        sizeof(p), &p.wgs_per_row);
}

void bilagrid_loglinear_patched_sample_backward_v1(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0,
    const int target_tile_size, const int mi_batch_size,
    backend::Stream stream
) {
    (void)stream;
    launch_family_bwd_v1(kLoglinearEntries, bilagrid, rgb, offsets, v_output,
                         v_bilagrid, v_rgb, N, L, H, W, m, h, w, h0, w0,
                         target_tile_size, mi_batch_size, nullptr, true);
}

/* ========================================================================
 * Depth (2-channel) family
 * ======================================================================== */

namespace {

void launch_depth_fwd(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    float* output, const int* offsets, const int* grid_indices, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0, bool patched
) {
    int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
    ReaderPtrs r = unpack_reader(bilagrid);
    BdUniformParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.depth = (uint64_t)depth;
    p.scalars = (uint64_t)scalars;
    p.output = (uint64_t)output;
    p.offsets = vkk::or_fallback(offsets);
    p.grid_indices = vkk::or_fallback(grid_indices);
    p.N = N; p.L = L; p.H = H; p.W = W;
    p.m = patched ? m : 1;
    p.h = h; p.w = w;
    p.h0 = patched ? h0 : 1;
    p.w0 = patched ? w0 : 1;
    p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted("bilagrid_depth.bilagrid_depth_fwd",
                                spec2(r.vq, patched), total, 256, &p, sizeof(p),
                                &p.wgs_per_row);
}

void launch_depth_bwd_v1(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    const int* offsets, const float* v_output, float* v_bilagrid,
    float* v_depth, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, int target_tile_size, int mi_batch_size, const int* grid_indices,
    bool patched
) {
    ReaderPtrs r = unpack_reader(bilagrid);
    // grid-grad kernel
    {
        int mult_x, mult_y;
        v1_mults(W, H, patched ? w0 : w, patched ? h0 : h, target_tile_size,
                 mult_x, mult_y);
        int num_m_batches =
            patched ? (m + mi_batch_size - 1) / mi_batch_size : 1;
        uint32_t gx = (uint32_t)((W * mult_x + kBwdV1BlockX - 1) / kBwdV1BlockX);
        uint32_t gy = (uint32_t)((H * mult_y + kBwdV1BlockY - 1) / kBwdV1BlockY);
        uint32_t gz = (uint32_t)(patched ? N * num_m_batches * L : N * L);
        check_grid_dims(gx, gy, gz, "depth_bwd_v1_grid");

        BdV1GridParams p{};
        p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
        p.depth = (uint64_t)depth;
        p.scalars = (uint64_t)scalars;
        p.v_output = (uint64_t)v_output;
        p.v_bilagrid = (uint64_t)v_bilagrid;
        p.offsets = vkk::or_fallback(offsets);
        p.grid_indices = vkk::or_fallback(grid_indices);
        p.N = N; p.L = L; p.H = H; p.W = W;
        p.m = patched ? m : 1;
        p.h = h; p.w = w;
        p.h0 = patched ? h0 : 1;
        p.w0 = patched ? w0 : 1;
        p.mult_x = mult_x;
        p.mult_y = mult_y;
        p.m_batch_stride = num_m_batches;
        p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
        const std::string key = v1_grid_key(
            "bilagrid_depth.bilagrid_depth_bwd_v1_grid", target_tile_size);
        vkk::dispatch_budgeted("bilagrid_depth.bilagrid_depth_bwd_v1_grid",
                               spec2(r.vq, patched), gx, gy, gz, &p,
                               sizeof(p), v1_grid_work(N, L, m, h, w, patched),
                               key.c_str());
    }
    // depth-grad (input-grad) kernel. null v_depth = skip: depth grids are
    // GT-side, so the engine discards this gradient (v_depth = nullptr).
    // Dispatching anyway writes N*h*w floats through a null device address ->
    // device fault -> a semaphore wait that never returns. (The shared
    // launch_family_bwd_v1 guards the same way for ppisp/loglinear/normal;
    // this separate depth launcher was missing the guard -- latent until
    // depth bilagrid first ran on Vulkan.)
    if (v_depth != nullptr) {
        int64_t total = patched ? (int64_t)N * m * h * w : (int64_t)N * h * w;
        BdV1DepthParams p{};
        p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
        p.depth = (uint64_t)depth;
        p.scalars = (uint64_t)scalars;
        p.v_output = (uint64_t)v_output;
        p.v_depth = (uint64_t)v_depth;
        p.offsets = vkk::or_fallback(offsets);
        p.grid_indices = vkk::or_fallback(grid_indices);
        p.N = N; p.L = L; p.H = H; p.W = W;
        p.m = patched ? m : 1;
        p.h = h; p.w = w;
        p.h0 = patched ? h0 : 1;
        p.w0 = patched ? w0 : 1;
        p.has_grid_indices = (!patched && grid_indices != nullptr) ? 1 : 0;
        p.total = (uint32_t)total;
        vkk::dispatch_flat_budgeted(
            "bilagrid_depth.bilagrid_depth_bwd_v1_depth", spec2(r.vq, patched),
            total, 256, &p, sizeof(p), &p.wgs_per_row);
    }
}

}  // namespace

void bilagrid_depth_uniform_sample_forward(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    float* output, int N, int L, int H, int W, int h, int w,
    backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    launch_depth_fwd(bilagrid, depth, scalars, output, nullptr, grid_indices,
                     N, L, H, W, 1, h, w, 1, 1, false);
}

void bilagrid_depth_patched_sample_forward(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    const int* offsets, float* output, int N, int L, int H, int W, int m,
    int h, int w, int h0, int w0, backend::Stream stream
) {
    (void)stream;
    launch_depth_fwd(bilagrid, depth, scalars, output, offsets, nullptr, N, L,
                     H, W, m, h, w, h0, w0, true);
}

void bilagrid_depth_uniform_sample_backward_v1(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    const float* v_output, float* v_bilagrid, float* v_depth, int N, int L,
    int H, int W, int h, int w, const int target_tile_size,
    backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    launch_depth_bwd_v1(bilagrid, depth, scalars, nullptr, v_output,
                        v_bilagrid, v_depth, N, L, H, W, 1, h, w, 1, 1,
                        target_tile_size, 1, grid_indices, false);
}

void bilagrid_depth_patched_sample_backward_v1(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    const int* offsets, const float* v_output, float* v_bilagrid,
    float* v_depth, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, const int target_tile_size, const int mi_batch_size,
    backend::Stream stream
) {
    (void)stream;
    launch_depth_bwd_v1(bilagrid, depth, scalars, offsets, v_output,
                        v_bilagrid, v_depth, N, L, H, W, m, h, w, h0, w0,
                        target_tile_size, mi_batch_size, nullptr, true);
}

void bilagrid_depth_uniform_sample_backward_v2(
    BilagridReader bilagrid, const float* depth, const float* scalars,
    const float* v_output, float* v_bilagrid, int N, int L, int H, int W,
    int h, int w, backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    ReaderPtrs r = unpack_reader(bilagrid);
    int64_t total = (int64_t)N * h * w;
    BdV2Params p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.depth = (uint64_t)depth;
    p.scalars = (uint64_t)scalars;
    p.v_output = (uint64_t)v_output;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.grid_indices = vkk::or_fallback(grid_indices);
    p.N = N; p.L = L; p.H = H; p.W = W; p.h = h; p.w = w;
    p.has_grid_indices = (grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted("bilagrid_depth.bilagrid_depth_bwd_v2",
                                backend::vk::SpecList{r.vq}, total, 256, &p,
                                sizeof(p), &p.wgs_per_row);
}

/* ========================================================================
 * Normal (3-channel) family
 * ======================================================================== */

void bilagrid_normal_uniform_sample_forward(
    BilagridReader bilagrid, const float* rgb, float* output, int N, int L,
    int H, int W, int h, int w, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_family_fwd(kNormalEntries, bilagrid, rgb, output, nullptr,
                      grid_indices, N, L, H, W, 1, h, w, 1, 1, false);
}

void bilagrid_normal_patched_sample_forward(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    float* output, int N, int L, int H, int W, int m, int h, int w, int h0,
    int w0, backend::Stream stream
) {
    (void)stream;
    launch_family_fwd(kNormalEntries, bilagrid, rgb, output, offsets, nullptr,
                      N, L, H, W, m, h, w, h0, w0, true);
}

void bilagrid_normal_uniform_sample_backward_v1(
    BilagridReader bilagrid, const float* rgb, const float* v_output,
    float* v_bilagrid, float* v_rgb, int N, int L, int H, int W, int h, int w,
    const int target_tile_size, backend::Stream stream,
    const int* grid_indices
) {
    (void)stream;
    launch_family_bwd_v1(kNormalEntries, bilagrid, rgb, nullptr, v_output,
                         v_bilagrid, v_rgb, N, L, H, W, 1, h, w, 1, 1,
                         target_tile_size, 1, grid_indices, false);
}

void bilagrid_normal_patched_sample_backward_v1(
    BilagridReader bilagrid, const float* rgb, const int* offsets,
    const float* v_output, float* v_bilagrid, float* v_rgb, int N, int L,
    int H, int W, int m, int h, int w, int h0, int w0,
    const int target_tile_size, const int mi_batch_size,
    backend::Stream stream
) {
    (void)stream;
    launch_family_bwd_v1(kNormalEntries, bilagrid, rgb, offsets, v_output,
                         v_bilagrid, v_rgb, N, L, H, W, m, h, w, h0, w0,
                         target_tile_size, mi_batch_size, nullptr, true);
}

void bilagrid_normal_uniform_sample_backward_v2(
    BilagridReader bilagrid, const float* rgb, const float* v_output,
    float* v_bilagrid, int N, int L, int H, int W, int h, int w,
    backend::Stream stream, const int* grid_indices
) {
    (void)stream;
    ReaderPtrs r = unpack_reader(bilagrid);
    int64_t total = (int64_t)N * h * w;
    BnV2Params p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.normal_in = (uint64_t)rgb;
    p.v_output = (uint64_t)v_output;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.grid_indices = vkk::or_fallback(grid_indices);
    p.N = N; p.L = L; p.H = H; p.W = W; p.h = h; p.w = w;
    p.has_grid_indices = (grid_indices != nullptr) ? 1 : 0;
    p.total = (uint32_t)total;
    vkk::dispatch_flat_budgeted("bilagrid_normal.bilagrid_normal_bwd_v2",
                                backend::vk::SpecList{r.vq}, total, 256, &p,
                                sizeof(p), &p.wgs_per_row);
}

/* ========================================================================
 * TV loss + channel mean
 * ======================================================================== */

namespace {

// The CUDA launchers only instantiate C in {12, 9, 2} (+3 for the forward /
// channel-mean forward); other channel counts silently launch nothing.
// Mirror that exactly.
bool tv_c_supported(int C, bool fwd) {
    return C == 12 || C == 9 || C == 2 || (fwd && C == 3);
}

// Grid for the four whole-table kernels (TV loss + channel mean, fwd/bwd).
// The fused (ni, li) axis has N*L/4 blocks -- 80000 for a 40k-image dataset
// at L=8, past the 65535 per-dimension dispatch cap -- so it folds across
// x/y at `nl_per_row` blocks per row, and the two small spatial block counts
// pack into z. Identical decomposition to bilagrid_tv_grid() in
// kernels/bilagrid/BilagridConfig.cuh; bg_tv_blocks() in bilagrid_tv.slang
// undoes it.
void tv_grid(int N, int L, int H, int W, uint32_t& gx, uint32_t& gy,
             uint32_t& gz, int& nl_per_row) {
    int64_t nl_blocks = ((int64_t)N * L + 3) / 4;
    int64_t per_row = nl_blocks < 1 ? 1 : nl_blocks;
    if (per_row > 65535) per_row = 65535;
    int64_t rows = (nl_blocks + per_row - 1) / per_row;
    uint32_t wb = (uint32_t)((W + 3) / 4);
    uint32_t hb = (uint32_t)((H + 3) / 4);
    gx = (uint32_t)per_row;
    gy = (uint32_t)rows;
    gz = wb * hb;
    nl_per_row = (int)per_row;
    check_grid_dims(gx, gy, gz, "tv/channel_mean");
}

}  // namespace

void tv_loss_forward(
    BilagridReader bilagrid, float* tv_loss, int N, int C, int L, int H, int W,
    backend::Stream stream
) {
    (void)stream;
    if (!tv_c_supported(C, true)) return;
    ReaderPtrs r = unpack_reader(bilagrid);
    TvParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.out_buf = (uint64_t)tv_loss;
    p.N = N; p.C = C; p.L = L; p.H = H; p.W = W;
    uint32_t gx, gy, gz;
    tv_grid(N, L, H, W, gx, gy, gz, p.nl_per_row);
    vkk::dispatch("bilagrid_tv.bilagrid_tv_loss_fwd",
                  backend::vk::SpecList{r.vq}, gx, gy, gz, &p, sizeof(p));
}

void tv_loss_backward(
    BilagridReader bilagrid, const float v_tv_loss, float* v_bilagrid, int N,
    int C, int L, int H, int W, bool inplace, backend::Stream stream
) {
    (void)stream;
    if (!tv_c_supported(C, false)) return;
    ReaderPtrs r = unpack_reader(bilagrid);
    TvParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.out_buf = (uint64_t)v_bilagrid;
    p.N = N; p.C = C; p.L = L; p.H = H; p.W = W;
    p.v_tv_loss = v_tv_loss;
    p.inplace = inplace ? 1 : 0;
    uint32_t gx, gy, gz;
    tv_grid(N, L, H, W, gx, gy, gz, p.nl_per_row);
    vkk::dispatch("bilagrid_tv.bilagrid_tv_loss_bwd",
                  backend::vk::SpecList{r.vq}, gx, gy, gz, &p, sizeof(p));
}

void channel_mean_forward(
    BilagridReader bilagrid, float* channel_mean, int N, int C, int L, int H,
    int W, backend::Stream stream
) {
    (void)stream;
    if (!tv_c_supported(C, true)) return;
    ReaderPtrs r = unpack_reader(bilagrid);
    ChannelMeanParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.channel_mean = (uint64_t)channel_mean;
    p.v_bilagrid = vkk::or_fallback(nullptr);
    p.N = N; p.C = C; p.L = L; p.H = H; p.W = W;
    uint32_t gx, gy, gz;
    tv_grid(N, L, H, W, gx, gy, gz, p.nl_per_row);
    vkk::dispatch("bilagrid_tv.bilagrid_channel_mean_fwd",
                  backend::vk::SpecList{r.vq}, gx, gy, gz, &p, sizeof(p));
}

void channel_mean_backward(
    BilagridReader bilagrid, const float* v_channel_mean, float* v_bilagrid,
    int N, int C, int L, int H, int W, bool inplace, backend::Stream stream
) {
    (void)stream;
    if (!tv_c_supported(C, false)) return;
    ReaderPtrs r = unpack_reader(bilagrid);
    ChannelMeanParams p{};
    p.fp32 = r.fp32; p.q16 = r.q16; p.vbounds = r.vbounds;
    p.channel_mean = (uint64_t)v_channel_mean;
    p.v_bilagrid = (uint64_t)v_bilagrid;
    p.N = N; p.C = C; p.L = L; p.H = H; p.W = W;
    p.inplace = inplace ? 1 : 0;
    uint32_t gx, gy, gz;
    tv_grid(N, L, H, W, gx, gy, gz, p.nl_per_row);
    vkk::dispatch("bilagrid_tv.bilagrid_channel_mean_bwd",
                  backend::vk::SpecList{r.vq}, gx, gy, gz, &p, sizeof(p));
}

/* ========================================================================
 * Fused TV-Adam / TV-AdaGrad + q16 encode + inits (EngineInternal.h)
 * ======================================================================== */

namespace {

void fill_optim(BgOptimParams& p, float* grids, uint16_t* grids_q16,
                float2* value_bounds, float* g1, float* g2, uint8_t* packed,
                const void* qb4, const void* qb2, const float* image_grad,
                const int* cam_indices, int N_grids, int C_batch, int C,
                int L, int H, int W, float lr, float tv_weight,
                int32_t adam_step, int64_t total_cells) {
    p.grids = vkk::or_fallback(grids);
    p.grids_q16 = vkk::or_fallback(grids_q16);
    p.value_bounds = vkk::or_fallback(value_bounds);
    p.g1_f = vkk::or_fallback(g1);
    p.g2_f = vkk::or_fallback(g2);
    p.packed = vkk::or_fallback(packed);
    p.quant_bounds4 = vkk::or_fallback(qb4);
    p.quant_bounds2 = vkk::or_fallback(qb2);
    p.image_grad = (uint64_t)image_grad;
    p.cam_indices = vkk::or_fallback(cam_indices);
    p.N_grids = N_grids;
    p.C_batch = C_batch;
    p.C = C;
    p.L = L; p.H = H; p.W = W;
    p.lr = lr;
    p.tv_weight = tv_weight;
    p.adam_step = adam_step;
    p.has_cam_indices = cam_indices != nullptr ? 1 : 0;
    p.total_cells = (uint32_t)total_cells;
}

}  // namespace

void fused_bilagrid_tv_adam(
    float* grids, uint16_t* grids_q16, float2* value_bounds, float* g1_f,
    float* g2_f, uint8_t* packed, float4* quant_bounds,
    const float* image_grad, const int* cam_indices, int N_grids, int C_batch,
    int C, int L, int H, int W, float lr, float tv_weight, int32_t adam_step,
    bool quantize, int quant_bits, bool value_quantize, backend::Stream stream
) {
    (void)stream;
    if (C != 12 && C != 9 && C != 3 && C != 2) return;
    const int64_t total_cells = (int64_t)N_grids * C * L * H * W;
    if (total_cells <= 0) return;
    BgOptimParams p{};
    fill_optim(p, value_quantize ? nullptr : grids, grids_q16, value_bounds,
               g1_f, g2_f, packed, quant_bounds, nullptr, image_grad,
               cam_indices, N_grids, C_batch, C, L, H, W, lr, tv_weight,
               adam_step, total_cells);
    const uint32_t qq = quantize ? (quant_bits == 4 ? 4u : 8u) : 0u;
    vkk::dispatch_flat("bilagrid_optim.bilagrid_tv_adam",
                       backend::vk::SpecList{qq, value_quantize ? 1u : 0u},
                       total_cells, 256, &p, sizeof(p), &p.wgs_per_row);
}

void fused_bilagrid_tv_adagrad(
    float* grids, uint16_t* grids_q16, float2* value_bounds, float* accum_f,
    uint8_t* packed, float2* quant_bounds, const float* image_grad,
    const int* cam_indices, int N_grids, int C_batch, int C, int L, int H,
    int W, float lr, float tv_weight, bool quantize, int quant_bits,
    bool value_quantize, backend::Stream stream
) {
    (void)stream;
    if (C != 12 && C != 9 && C != 3 && C != 2) return;
    const int64_t total_cells = (int64_t)N_grids * C * L * H * W;
    if (total_cells <= 0) return;
    BgOptimParams p{};
    fill_optim(p, value_quantize ? nullptr : grids, grids_q16, value_bounds,
               accum_f, nullptr, packed, nullptr, quant_bounds, image_grad,
               cam_indices, N_grids, C_batch, C, L, H, W, lr, tv_weight, 0,
               total_cells);
    const uint32_t qq = quantize ? (quant_bits == 4 ? 4u : 8u) : 0u;
    vkk::dispatch_flat("bilagrid_optim.bilagrid_tv_adagrad",
                       backend::vk::SpecList{qq, value_quantize ? 1u : 0u},
                       total_cells, 256, &p, sizeof(p), &p.wgs_per_row);
}

void bilagrid_encode_q16_launch(
    const float* grids, uint16_t* grids_q16, float2* value_bounds,
    int64_t total_cells, backend::Stream stream
) {
    (void)stream;
    if (total_cells <= 0) return;
    EncodeQ16Params p{};
    p.grids = (uint64_t)grids;
    p.grids_q16 = (uint64_t)grids_q16;
    p.value_bounds = (uint64_t)value_bounds;
    p.total_cells = (uint32_t)total_cells;
    vkk::dispatch_flat("bilagrid_tv.bilagrid_encode_q16",
                       backend::vk::SpecList{0u}, total_cells, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void bilagrid_affine_identity_init(
    float* grids, int N, int L, int H, int W, backend::Stream stream
) {
    (void)stream;
    if (N <= 0 || L <= 0 || H <= 0 || W <= 0) return;
    AffineInitParams p{};
    p.grids = (uint64_t)grids;
    int64_t total = (int64_t)N * L * H * W;
    p.total = (uint32_t)total;
    vkk::dispatch_flat("bilagrid_tv.bilagrid_affine_identity_init",
                       backend::vk::SpecList{0u}, total, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void bilagrid_scatter_floats(
    const float* src, const int* indices, int n, float* dst,
    backend::Stream stream
) {
    (void)stream;
    if (n <= 0) return;
    ScatterFloatsParams p{};
    p.src = (uint64_t)src;
    p.indices = (uint64_t)indices;
    p.dst = (uint64_t)dst;
    p.total = (uint32_t)n;
    vkk::dispatch_flat("bilagrid_tv.bilagrid_scatter_floats",
                       backend::vk::SpecList{0u}, n, 128, &p, sizeof(p),
                       &p.wgs_per_row);
}
