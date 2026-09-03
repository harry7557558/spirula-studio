#include "nn/core/Error.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include "core/Env.h"
#include "nn/core/Log.h"
#include "nn/Ops.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

namespace nn {

namespace {

// Mirrors GemmParams in shaders/gemm.slang. Pointers first (8-byte aligned at
// natural C offsets, which is what Slang emits for a uniform block of raw
// pointers), then scalars.
struct GemmParams {
    uint64_t out;
    uint64_t x;
    uint64_t w;
    uint64_t bias;
    uint64_t residual;
    uint32_t M, N, K;
    float    alpha;
    uint32_t x_row_stride;
    uint32_t groups_per_row;
};
static_assert(sizeof(GemmParams) == 64, "GemmParams layout");

// Below this many rows the 64x64 tiling has nothing to chew on and a
// thread-per-K reduction wins. SAM 3 hits this constantly: every MLP head runs
// a single token, the hypernetwork mask heads run one each.
constexpr int64_t kThinRowLimit = 4;

// At or above this on both axes the 128x128 tile wins: it does four times the
// work per shared-memory read, and the edge waste is under 2% at SAM 3's
// shapes. Below it the wasted quarter-tiles cost more than the ratio buys.
constexpr int64_t kBigTileLimit = 128;

// gemm_coop.slang's fixed geometry: 128 output columns per workgroup, and rows
// = 32 * (subgroups per workgroup / 2), which is 128 at a 32-wide subgroup and
// 64 at 64-wide.
constexpr int64_t kCoopTileN = 128;

int64_t coop_tile_m(uint32_t subgroup) { return (int64_t)(256u / subgroup / 2u) * 32; }

// -1 = follow the device probe; 0/1 = forced by set_coop_matrix_enabled().
int g_coop_override = -1;

// Which tiled kernel this device runs fastest, timed once per process: no
// Vulkan property predicts it. On 5184x3072x1024 the wide tile wins 8.6 vs 2.8
// TFLOP/s on an RTX 5070 and loses 0.33 vs 0.82 on an M2, whose register file
// cannot hold its 64 accumulators. Costs one pipeline compile and ~4 ms, once;
// SS_NN_GEMM_KERNEL=wide|narrow pins it.
struct TileKernel {
    const char* entry;
    int64_t     tile;    // output tile edge
    bool        n_on_x;  // gemm_nt_big alone wants N on the x axis
};
struct TileKernels {
    TileKernel f16;  // fp16 weights, M and N past kBigTileLimit
    TileKernel any;  // everything else that reaches the tiled path
};

const TileKernel kWide{"gemm.gemm_nt_big", 128, true};
const TileKernel kNarrow{"gemm.gemm_nt_narrow", 64, false};
const TileKernel kSquare{"gemm.gemm_nt", 64, false};

void dispatch_tile(const TileKernel& k, const vk::SpecList& spec, int64_t M, int64_t N,
                   const GemmParams& p) {
    const uint32_t tm = (uint32_t)((M + k.tile - 1) / k.tile);
    const uint32_t tn = (uint32_t)((N + k.tile - 1) / k.tile);
    NN_CHECK(tm <= 65535 && tn <= 65535,
               "gemm: %lldx%lld output exceeds the dispatch grid cap", (long long)M,
               (long long)N);
    vk::Stream::get().dispatch(k.entry, spec, k.n_on_x ? tn : tm, k.n_on_x ? tm : tn, 1,
                               &p, sizeof(p));
}

double time_tile(const TileKernel& k, const vk::SpecList& spec, int64_t M, int64_t N,
                 const GemmParams& p) {
    using clock = std::chrono::steady_clock;
    dispatch_tile(k, spec, M, N, p);   // pay the pipeline compile outside the timer
    vk::Stream::get().sync();
    const auto t0 = clock::now();
    for (int i = 0; i < 3; ++i) dispatch_tile(k, spec, M, N, p);
    vk::Stream::get().sync();
    return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
}

TileKernels probe_tile_kernels() {
    const TileKernels wide{kWide, kSquare};
    const TileKernels narrow{kNarrow, kNarrow};
    if (const char* forced = spirula::env("NN_GEMM_KERNEL")) {
        if (std::strcmp(forced, "narrow") == 0) return narrow;
        if (std::strcmp(forced, "wide") == 0) return wide;
    }
    // Big enough that both candidates fill the device: at 512x512 the wide tile
    // makes 16 workgroups and loses on a 5070 it beats 3x in real use. This is
    // 256 workgroups, 4.3 GFLOP, 22 MiB.
    const int64_t M = 2048, N = 2048, K = 512;
    const uint64_t bytes = (uint64_t)M * K * 4 + (uint64_t)N * K * 2 + (uint64_t)M * N * 4;
    const vk::DevicePtr base = vk::device_alloc(bytes, "gemm probe");
    if (!base) return wide;

    GemmParams p{};
    p.x = base;
    p.w = p.x + (uint64_t)M * K * 4;
    p.out = p.w + (uint64_t)N * K * 2;
    p.bias = vk::or_fallback(0);
    p.residual = vk::or_fallback(0);
    p.M = (uint32_t)M;
    p.N = (uint32_t)N;
    p.K = (uint32_t)K;
    p.alpha = 1.0f;
    p.x_row_stride = (uint32_t)K;
    vk::Stream::get().fill(p.x, 0x3f000000u, (uint64_t)M * K * 4);
    vk::Stream::get().fill(p.w, 0x38003800u, (uint64_t)N * K * 2);

    const vk::SpecList spec{1u, 0u, 0u, 0u, 0u};   // fp16 weights, bare epilogue
    double t_wide = 0, t_narrow = 0;
    try {
        t_wide = time_tile(kWide, spec, M, N, p);
        t_narrow = time_tile(kNarrow, spec, M, N, p);
    } catch (const std::exception& e) {
        NN_LOG_WARN("gemm tile probe failed (%s); keeping the wide tile\n", e.what());
        vk::device_free(base);
        return wide;
    }
    vk::device_free(base);

    // A margin, not a comparison: a tie should keep the better-travelled path.
    const bool pick_narrow = t_narrow < 0.9 * t_wide;
    NN_LOG_INFO("gemm: %s tile (%.2f ms wide, %.2f ms narrow)\n",
                pick_narrow ? "narrow" : "wide", t_wide, t_narrow);
    return pick_narrow ? narrow : wide;
}

GemmTile g_tile_override = GemmTile::Measured;

const TileKernels& tile_kernels() {
    static const TileKernels wide{kWide, kSquare};
    static const TileKernels narrow{kNarrow, kNarrow};
    if (g_tile_override == GemmTile::Wide) return wide;
    if (g_tile_override == GemmTile::Narrow) return narrow;
    static const TileKernels measured = probe_tile_kernels();
    return measured;
}

void dispatch_gemm(const Tensor& out, const Tensor& x_in, const Tensor& w_in,
                   const Tensor& bias, const Tensor& residual, Act act, float alpha,
                   int64_t x_row_stride) {
    // Weights keep their PyTorch rank in the store; a [Cout, Cin, kh, kw] conv
    // kernel is the same bytes as the [Cout, Cin*kh*kw] matrix this wants.
    const Tensor w = w_in.asMatrix();
    const Tensor x = x_in;
    const int64_t M = out.rows();
    const int64_t N = out.cols();
    const int64_t K = w.cols();

    NN_CHECK(out.dtype == DType::F32, "gemm output must be f32");
    NN_CHECK(w.rows() == N, "gemm: weight is [%lld, %lld] but out has %lld columns",
               (long long)w.rows(), (long long)K, (long long)N);
    NN_CHECK(x.cols() == K || x_row_stride > 0,
               "gemm: x has %lld columns but the weight expects %lld",
               (long long)x.cols(), (long long)K);
    NN_CHECK(x.rows() == M || x_row_stride > 0,
               "gemm: x has %lld rows but out has %lld", (long long)x.rows(),
               (long long)M);
    if (residual.valid())
        NN_CHECK(residual.numel() >= M * N, "gemm: residual is too small");
    if (bias.valid())
        NN_CHECK(bias.numel() >= N, "gemm: bias has %lld entries, need %lld",
                   (long long)bias.numel(), (long long)N);

    GemmParams p{};
    p.out = out.ptr;
    p.x = x.ptr;
    p.w = w.ptr;
    // Never leave an optional pointer null: a CPU Vulkan implementation can
    // speculate the load past its guard and fault (AGENTS.md).
    p.bias = vk::or_fallback(bias.ptr);
    p.residual = vk::or_fallback(residual.ptr);
    p.M = (uint32_t)M;
    p.N = (uint32_t)N;
    p.K = (uint32_t)K;
    p.alpha = alpha;
    p.x_row_stride = (uint32_t)(x_row_stride > 0 ? x_row_stride : K);

    vk::SpecList spec{(uint32_t)(w.dtype == DType::F16),
                      (uint32_t)(x.dtype == DType::F16),
                      (uint32_t)act,
                      (uint32_t)(bias.valid() ? 1 : 0),
                      (uint32_t)(residual.valid() ? 1 : 0)};

    check_span("gemm", {w, bias});
    const int64_t x_elem = x.dtype == DType::F16 ? 2 : 4;

    // Row tiles take one grid axis (65535 cap: a full-resolution 1x1 conv passes
    // 4.2 M rows) and each call must also stay inside one pointer span. Rows are
    // independent and row-contiguous, so a tall GEMM is the same call again.
    auto by_rows = [&](int64_t tile_m, const auto& run) {
        // A row tile is the dispatch granularity, so the span is counted in tiles.
        const int64_t pitch = std::max(N * 4, (int64_t)p.x_row_stride * x_elem);
        const int64_t tiles = span_rows("gemm", pitch * tile_m);
        const int64_t per_call = std::min(tiles, (int64_t)65535) * tile_m;
        for (int64_t m0 = 0; m0 < M; m0 += per_call) {
            const int64_t rows = std::min(per_call, M - m0);
            GemmParams q = p;
            q.M = (uint32_t)rows;
            q.out = p.out + (uint64_t)m0 * N * 4;
            q.x = p.x + (uint64_t)m0 * p.x_row_stride * x_elem;
            if (residual.valid()) q.residual = p.residual + (uint64_t)m0 * N * 4;
            run(q, rows);
        }
    };

    // Tensor cores, when the device has them and the shape is big enough to
    // amortize a 128-wide column tile. Everything below this is the fp32 path,
    // unchanged and still the only path on a device without the extension.
    const vk::Context& ctx = vk::Context::get();
    // N and K multiples of 16: a cooperative-matrix load does no bounds check,
    // so a 16x16 block of W must lie wholly inside it or wholly outside. The
    // fragment load's Aligned 16 decoration is why w.ptr is checked as well.
    if (coop_matrix_enabled() && w.dtype == DType::F16 && N % 16 == 0 && K % 16 == 0 &&
        M >= kBigTileLimit && N >= kBigTileLimit && w.ptr % 16 == 0) {
        const int64_t tile_m = coop_tile_m(ctx.preferredSubgroupSize());
        const uint32_t tn = (uint32_t)((N + kCoopTileN - 1) / kCoopTileN);
        if (tn <= 65535) {
            // NOTE: a different constant list from gemm.slang's -- no
            // kWeightF16 (always fp16 here), plus the subgroup-row count.
            vk::SpecList cspec{(uint32_t)(x.dtype == DType::F16),
                               (uint32_t)act,
                               (uint32_t)(bias.valid() ? 1 : 0),
                               (uint32_t)(residual.valid() ? 1 : 0),
                               (uint32_t)(tile_m / 32)};
            by_rows(tile_m, [&](const GemmParams& q, int64_t rows) {
                vk::Stream::get().dispatch("gemm_coop.gemm_nt_coop", cspec, tn,
                                           (uint32_t)((rows + tile_m - 1) / tile_m), 1,
                                           &q, sizeof(q));
            });
            return;
        }
    }

    if (M <= kThinRowLimit) {
        // One workgroup per output element, so N alone can exceed the 65535
        // grid cap; fold it across x and y and give the row to z.
        check_span("gemm", {out, x, residual});
        const uint32_t per_row = (uint32_t)std::min<int64_t>(N, 65535);
        const uint32_t rows = (uint32_t)((N + per_row - 1) / per_row);
        p.groups_per_row = per_row;
        vk::Stream::get().dispatch("gemm.gemm_nt_thin", spec, per_row, rows, (uint32_t)M, &p,
                                   sizeof(p));
    } else {
        // gemm_nt_big keeps the weight tile packed as fp16 in shared, so it has
        // nothing to offer an fp32 second operand (a matmul against another
        // activation) and does not handle one.
        const bool wide =
            M >= kBigTileLimit && N >= kBigTileLimit && w.dtype == DType::F16;
        const TileKernel& k = wide ? tile_kernels().f16 : tile_kernels().any;
        by_rows(k.tile, [&](const GemmParams& q, int64_t rows) {
            dispatch_tile(k, spec, rows, N, q);
        });
    }
}

}  // namespace

void set_gemm_tile(GemmTile t) { g_tile_override = t; }

bool coop_matrix_enabled() {
    if (g_coop_override >= 0) return g_coop_override != 0;
    return vk::Context::get().hasCoopMat();
}

void set_coop_matrix_enabled(bool on) {
    // Asking for it where the device cannot do it would trip the driver on the
    // module's capabilities, not degrade -- so this only ever narrows.
    g_coop_override = (on && vk::Context::get().hasCoopMat()) ? 1 : 0;
}

void linear(const Tensor& out, const Tensor& x, const Tensor& w, const LinearOpts& o) {
    dispatch_gemm(out, x, w, o.bias, o.residual, o.act, o.alpha, o.x_row_stride);
}

void matmul_nt(const Tensor& out, const Tensor& a, const Tensor& b, float alpha,
               Act act) {
    dispatch_gemm(out, a, b, {}, {}, act, alpha, 0);
}

}  // namespace nn
