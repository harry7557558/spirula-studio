#include "nn/core/Error.h"
#include "nn/Ops.h"
#include "nn/vk/Stream.h"

#include <algorithm>

namespace nn {

namespace {

struct LayerNormParams {
    uint64_t out, x, weight, bias, residual;
    uint32_t rows, cols;
    float    eps;
    uint32_t x_row_stride, out_row_stride, has_affine, groups_per_row, _pad0;
};

struct GroupNormStatsParams {
    uint64_t stats, x;
    uint32_t n, channels, groups;
    float    eps;
};

struct GroupNormApplyParams {
    uint64_t out, x, stats, weight, bias;
    uint32_t n, channels, groups, act, groups_per_row, _pad0, _pad1;
};

}  // namespace

void layer_norm(const Tensor& out, const Tensor& x, const Tensor& w, const Tensor& b,
                float eps, const Tensor& residual) {
    const int64_t rows = out.rows();
    const int64_t cols = out.cols();
    NN_CHECK(out.dtype == DType::F32, "layer_norm output must be f32");
    NN_CHECK(x.cols() == cols, "layer_norm: x has %lld columns, out has %lld",
               (long long)x.cols(), (long long)cols);
    if (w.valid())
        NN_CHECK(w.numel() >= cols && b.numel() >= cols,
                   "layer_norm: affine parameters are shorter than %lld", (long long)cols);

    check_span("layer_norm", {w, b});

    vk::SpecList spec{(uint32_t)(residual.valid() ? 1 : 0),
                      (uint32_t)(x.dtype == DType::F16)};
    const int64_t xe = dtype_size(x.dtype);
    const int64_t per = span_rows_even("layer_norm", cols * std::max<int64_t>(4, xe));
    for (int64_t r0 = 0; r0 < rows; r0 += per) {
        LayerNormParams p{};
        p.out = out.ptr + (uint64_t)(r0 * cols) * 4;
        p.x = x.ptr + (uint64_t)(r0 * cols) * xe;
        p.weight = vk::or_fallback(w.ptr);
        p.bias = vk::or_fallback(b.ptr);
        p.residual = vk::or_fallback(
            residual.valid() ? residual.ptr + (uint64_t)(r0 * cols) * 4 : 0);
        p.rows = (uint32_t)std::min(per, rows - r0);
        p.cols = (uint32_t)cols;
        p.eps = eps;
        p.x_row_stride = (uint32_t)cols;
        p.out_row_stride = (uint32_t)cols;
        p.has_affine = w.valid() ? 1u : 0u;

        // One workgroup per row; rows can exceed the 65535 grid cap (a 288x288
        // map has 82944), so fold across y.
        vk::Stream::Fold f = vk::Stream::fold1D(p.rows, 1);
        p.groups_per_row = f.per_row;
        vk::Stream::get().dispatch("norm.layer_norm", spec, f.per_row, f.rows, 1, &p,
                                   sizeof(p));
    }
}

void group_norm(vk::Arena& arena, const Tensor& out, const Tensor& x, const Tensor& w,
                const Tensor& b, int groups, float eps, Act act) {
    const int64_t channels = out.cols();
    const int64_t n = out.rows();
    NN_CHECK(channels % groups == 0, "group_norm: %lld channels do not split into %d",
               (long long)channels, groups);

    Tensor stats = arena_tensor(arena, DType::F32, groups, 2);
    // Both passes index the whole map from its base, so it cannot be split.
    const KernelName stats_entry = span_entry("norm.group_norm_stats", {x});
    const KernelName apply_entry = span_entry("norm.group_norm_apply", {out, x});
    check_span("group_norm", {w, b});

    GroupNormStatsParams sp{};
    sp.stats = stats.ptr;
    sp.x = x.ptr;
    sp.n = (uint32_t)n;
    sp.channels = (uint32_t)channels;
    sp.groups = (uint32_t)groups;
    sp.eps = eps;
    vk::SpecList spec{0u, (uint32_t)(x.dtype == DType::F16)};
    vk::Stream::get().dispatch(stats_entry, spec, (uint32_t)groups, 1, 1, &sp,
                               sizeof(sp));

    GroupNormApplyParams ap{};
    ap.out = out.ptr;
    ap.x = x.ptr;
    ap.stats = stats.ptr;
    ap.weight = vk::or_fallback(w.ptr);
    ap.bias = vk::or_fallback(b.ptr);
    ap.n = (uint32_t)n;
    ap.channels = (uint32_t)channels;
    ap.groups = (uint32_t)groups;
    ap.act = (uint32_t)act;
    vk::Stream::get().dispatchFlat(apply_entry, spec, n * channels, 256, &ap, sizeof(ap),
                                   &ap.groups_per_row);
}

}  // namespace nn
