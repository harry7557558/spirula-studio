// Launchers for the elementwise, resample, gather and shuffle kernels.

#include "nn/core/Error.h"
#include "nn/Ops.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cstring>

namespace nn {

namespace {

struct BinaryParams {
    uint64_t out, a, b;
    uint32_t n, cols;
    float    alpha, beta;
    uint32_t groups_per_row;
};

struct UnaryParams {
    uint64_t out, x;
    uint32_t n;
    float    pre_scale, pre_bias, post_scale, post_bias;
    uint32_t groups_per_row;
};

struct ConvertParams {
    uint64_t out, x;
    uint32_t n, groups_per_row;
};

struct GatherParams {
    uint64_t out, table, ids;
    uint32_t n, cols, vocab, groups_per_row, _pad0;
};

struct StridedCopyParams {
    uint64_t out, x;
    uint32_t rows, cols, in_stride, out_stride, groups_per_row, _pad0;
};

struct WindowParams {
    uint64_t out, x;
    uint32_t H, W, C, ws, nwh, nww, groups_per_row, _pad0;
};

struct TiledAddParams {
    uint64_t out, x, tile;
    uint32_t H, W, C, th, tw, groups_per_row;
};

struct RoiAlignParams {
    uint64_t out, feat, boxes;
    uint32_t nboxes, H, W, C, S, groups_per_row, _pad0;
};

struct ResizeParams {
    uint64_t out, x;
    uint32_t Ho, Wo, Hi, Wi, C, groups_per_row;
};

struct MaskExportParams {
    uint64_t out, x;
    uint32_t Ho, Wo, Hi, Wi;
    float    threshold;
    uint32_t groups_per_row;
};

struct PoolParams {
    uint64_t out, x;
    uint32_t Ho, Wo, Hi, Wi, C, kh, kw, stride_y, stride_x, pad_y, pad_x, groups_per_row;
};

struct ScaledResizeParams {
    uint64_t out, x;
    uint32_t Ho, Wo, Hi, Wi, C, groups_per_row;
    float    inv_scale_y, inv_scale_x;
};

struct GridSampleParams {
    uint64_t out, x, pos;
    uint32_t H, W, C, N, groups_per_row;
};

struct NormalizeParams {
    uint64_t out, x;
    uint32_t rows, cols;
    float    eps;
    uint32_t groups_per_row;
};

}  // namespace

// ================
// Elementwise
// ================

void fill(const Tensor& t, float value) {
    NN_CHECK(t.dtype == DType::F32 || value == 0.0f,
               "fill: only zero-fill is defined for %s tensors", dtype_name(t.dtype));
    uint32_t word;
    std::memcpy(&word, &value, 4);
    vk::Stream::get().fill(t.ptr, word, (VkDeviceSize)t.bytes());
}

void copy(const Tensor& dst, const Tensor& src) {
    NN_CHECK(dst.numel() == src.numel(), "copy: %lld vs %lld elements",
               (long long)dst.numel(), (long long)src.numel());
    if (dst.dtype == src.dtype) {
        vk::Stream::get().copy(dst.ptr, src.ptr, (VkDeviceSize)dst.bytes());
        return;
    }
    // kOp selects the direction: 0 -> f32 out, 1 -> packed f16 out.
    const uint32_t to_f16 = (dst.dtype == DType::F16) ? 1u : 0u;
    NN_CHECK(to_f16 || dst.dtype == DType::F32, "copy: unsupported target dtype %s",
               dtype_name(dst.dtype));
    vk::SpecList spec{to_f16, 0u, 0u, (uint32_t)(src.dtype == DType::F16)};
    const int64_t de = dtype_size(dst.dtype), se = dtype_size(src.dtype);
    const int64_t per = span_rows_even("copy", std::max(de, se));
    for (int64_t i0 = 0; i0 < src.numel(); i0 += per) {
        const int64_t n = std::min(per, src.numel() - i0);
        ConvertParams p{};
        p.out = dst.ptr + (uint64_t)i0 * de;
        p.x = src.ptr + (uint64_t)i0 * se;
        p.n = (uint32_t)n;
        const int64_t threads = to_f16 ? (n + 1) / 2 : n;
        vk::Stream::get().dispatchFlat("elementwise.convert", spec, threads, 256, &p,
                                       sizeof(p), &p.groups_per_row);
    }
}

static void binary_op(const Tensor& out, const Tensor& a, const Tensor& b, uint32_t op,
                      float alpha, float beta, Act act) {
    // Broadcast mode is inferred from the shape, which is unambiguous here:
    // matching element count is elementwise, a single row is per-column, a
    // single element is scalar.
    uint32_t bcast = 0;
    if (b.valid()) {
        if (b.numel() == out.numel())      bcast = 0;
        else if (b.numel() == out.cols())  bcast = 1;
        else if (b.numel() == 1)           bcast = 2;
        else fail("binary op: b has %lld elements, which broadcasts against neither "
                  "%lld (elementwise) nor %lld (per-column)",
                  (long long)b.numel(), (long long)out.numel(), (long long)out.cols());
    } else {
        bcast = 2;  // zeroed fallback
    }
    vk::SpecList spec{op, bcast, (uint32_t)act,
                      (uint32_t)(a.dtype == DType::F16)};
    if (bcast == 1) check_span("binary op", {b});

    // A per-column `b` is indexed by `i % cols`, so those chunks carry whole
    // rows; the other modes chunk on any element.
    const int64_t oe = dtype_size(out.dtype), ae = dtype_size(a.dtype);
    const int64_t be = b.valid() ? dtype_size(b.dtype) : 0;
    const int64_t width = (bcast == 1) ? std::max<int64_t>(out.cols(), 1) : 1;
    const int64_t pitch = width * std::max(oe, std::max(ae, bcast == 0 ? be : 0));
    const int64_t per = span_rows_even("binary op", pitch) * width;
    for (int64_t i0 = 0; i0 < out.numel(); i0 += per) {
        const int64_t n = std::min(per, out.numel() - i0);
        BinaryParams p{};
        p.out = out.ptr + (uint64_t)i0 * oe;
        p.a = a.ptr + (uint64_t)i0 * ae;
        p.b = vk::or_fallback(bcast == 0 ? b.ptr + (uint64_t)i0 * be : b.ptr);
        p.n = (uint32_t)n;
        p.cols = (uint32_t)out.cols();
        p.alpha = alpha;
        p.beta = beta;
        vk::Stream::get().dispatchFlat("elementwise.binary", spec, n, 256, &p, sizeof(p),
                                       &p.groups_per_row);
    }
}

void add(const Tensor& out, const Tensor& a, const Tensor& b, float alpha, float beta,
         Act act) {
    binary_op(out, a, b, 0, alpha, beta, act);
}

void mul(const Tensor& out, const Tensor& a, const Tensor& b, Act act) {
    binary_op(out, a, b, 1, 1.0f, 1.0f, act);
}

void unary(const Tensor& out, const Tensor& x, Act act, float pre_scale, float pre_bias,
           float post_scale, float post_bias) {
    vk::SpecList spec{0u, 0u, (uint32_t)act, (uint32_t)(x.dtype == DType::F16)};
    const int64_t oe = dtype_size(out.dtype), xe = dtype_size(x.dtype);
    const int64_t per = span_rows_even("unary", std::max(oe, xe));
    for (int64_t i0 = 0; i0 < out.numel(); i0 += per) {
        const int64_t n = std::min(per, out.numel() - i0);
        UnaryParams p{};
        p.out = out.ptr + (uint64_t)i0 * oe;
        p.x = x.ptr + (uint64_t)i0 * xe;
        p.n = (uint32_t)n;
        p.pre_scale = pre_scale;
        p.pre_bias = pre_bias;
        p.post_scale = post_scale;
        p.post_bias = post_bias;
        vk::Stream::get().dispatchFlat("elementwise.unary", spec, n, 256, &p, sizeof(p),
                                       &p.groups_per_row);
    }
}

// ================
// Gather / shuffle
// ================

void gather_rows(const Tensor& out, const Tensor& table, const Tensor& ids) {
    const KernelName entry = span_entry("misc.gather_rows", {out, table, ids});
    GatherParams p{};
    p.out = out.ptr;
    p.table = table.ptr;
    p.ids = ids.ptr;
    p.n = (uint32_t)out.rows();
    p.cols = (uint32_t)out.cols();
    p.vocab = (uint32_t)table.rows();
    vk::SpecList spec{(uint32_t)(table.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void strided_copy(const Tensor& out, const Tensor& in, int64_t rows, int64_t cols,
                  int64_t in_stride, int64_t out_stride) {
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u};
    const int64_t oe = dtype_size(out.dtype), ie = dtype_size(in.dtype);
    const int64_t per =
        span_rows_even("strided_copy", std::max(out_stride * oe, in_stride * ie));
    for (int64_t r0 = 0; r0 < rows; r0 += per) {
        StridedCopyParams p{};
        p.out = out.ptr + (uint64_t)(r0 * out_stride) * oe;
        p.x = in.ptr + (uint64_t)(r0 * in_stride) * ie;
        p.rows = (uint32_t)std::min(per, rows - r0);
        p.cols = (uint32_t)cols;
        p.in_stride = (uint32_t)in_stride;
        p.out_stride = (uint32_t)out_stride;
        vk::Stream::get().dispatchFlat("misc.strided_copy", spec, (int64_t)p.rows * cols,
                                       256, &p, sizeof(p), &p.groups_per_row);
    }
}

static void window_op(const char* entry, const Tensor& out, const Tensor& in, int H,
                      int W, int C, int ws, int64_t total) {
    const KernelName e = span_entry(entry, {out, in});
    WindowParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.H = (uint32_t)H;
    p.W = (uint32_t)W;
    p.C = (uint32_t)C;
    p.ws = (uint32_t)ws;
    p.nwh = (uint32_t)((H + ws - 1) / ws);
    p.nww = (uint32_t)((W + ws - 1) / ws);
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(e, spec, total, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void window_partition(const Tensor& out, const Tensor& in, int H, int W, int C, int ws) {
    const int64_t nwh = (H + ws - 1) / ws, nww = (W + ws - 1) / ws;
    window_op("misc.window_partition", out, in, H, W, C, ws,
              nwh * nww * ws * ws * (int64_t)C);
}

void window_unpartition(const Tensor& out, const Tensor& in, int H, int W, int C,
                        int ws) {
    window_op("misc.window_unpartition", out, in, H, W, C, ws, (int64_t)H * W * C);
}

void add_tiled(const Tensor& out, const Tensor& in, const Tensor& tile, int H, int W,
               int C, int th, int tw) {
    const KernelName entry = span_entry("misc.add_tiled", {out, in, tile});
    TiledAddParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.tile = tile.ptr;
    p.H = (uint32_t)H;
    p.W = (uint32_t)W;
    p.C = (uint32_t)C;
    p.th = (uint32_t)th;
    p.tw = (uint32_t)tw;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(entry, spec, (int64_t)H * W * C, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void roi_align(const Tensor& out, const Tensor& feat, const Tensor& boxes, int H, int W,
               int C, int S) {
    const KernelName entry = span_entry("misc.roi_align", {out, feat, boxes});
    RoiAlignParams p{};
    p.out = out.ptr;
    p.feat = feat.ptr;
    p.boxes = boxes.ptr;
    p.nboxes = (uint32_t)boxes.rows();
    p.H = (uint32_t)H;
    p.W = (uint32_t)W;
    p.C = (uint32_t)C;
    p.S = (uint32_t)S;
    vk::SpecList spec{(uint32_t)(feat.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

// ================
// Resample
// ================

static void resize_op(const char* entry, const Tensor& out, const Tensor& in,
                      bool align_corners = false) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3,
               "%s expects [H, W, C] tensors (got %dD and %dD)", entry, out.ndim,
               in.ndim);
    NN_CHECK(out.shape[2] == in.shape[2], "%s: channel counts differ (%lld vs %lld)",
               entry, (long long)out.shape[2], (long long)in.shape[2]);
    const KernelName e = span_entry(entry, {out, in});
    ResizeParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.Ho = (uint32_t)out.shape[0];
    p.Wo = (uint32_t)out.shape[1];
    p.Hi = (uint32_t)in.shape[0];
    p.Wi = (uint32_t)in.shape[1];
    p.C = (uint32_t)out.shape[2];
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u, (uint32_t)align_corners};
    vk::Stream::get().dispatchFlat(e, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void resize_bilinear(const Tensor& out, const Tensor& in, bool align_corners) {
    resize_op("resample.resize_bilinear", out, in, align_corners);
}
void upsample_nearest2x(const Tensor& out, const Tensor& in) {
    resize_op("resample.upsample_nearest2x", out, in);
}
void maxpool2x2(const Tensor& out, const Tensor& in) {
    resize_op("resample.maxpool2x2", out, in);
}

void resize_nearest(const Tensor& out, const Tensor& in, float scale_y, float scale_x) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3, "resize_nearest expects [H, W, C] tensors");
    NN_CHECK(out.shape[2] == in.shape[2], "resize_nearest: channel counts differ");
    NN_CHECK(scale_y > 0.0f && scale_x > 0.0f, "resize_nearest: scale must be positive");
    const KernelName entry = span_entry("resample.resize_nearest", {out, in});
    ScaledResizeParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.Ho = (uint32_t)out.shape[0];
    p.Wo = (uint32_t)out.shape[1];
    p.Hi = (uint32_t)in.shape[0];
    p.Wi = (uint32_t)in.shape[1];
    p.C = (uint32_t)out.shape[2];
    p.inv_scale_y = 1.0f / scale_y;
    p.inv_scale_x = 1.0f / scale_x;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u, 0u};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void avgpool(const Tensor& out, const Tensor& in, int kernel, int stride, int pad) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3, "avgpool expects [H, W, C] tensors");
    NN_CHECK(out.shape[2] == in.shape[2], "avgpool: channel counts differ");
    NN_CHECK(kernel > 0, "avgpool: kernel must be positive");
    if (stride <= 0) stride = kernel;
    // torch with ceil_mode=False; say so here rather than let a mis-sized
    // output silently read past the last complete window.
    const int64_t want_h = (in.shape[0] + 2 * pad - kernel) / stride + 1;
    const int64_t want_w = (in.shape[1] + 2 * pad - kernel) / stride + 1;
    NN_CHECK(out.shape[0] == want_h && out.shape[1] == want_w,
             "avgpool(%d, %d, pad %d) of %lldx%lld is %lldx%lld, but out is %lldx%lld",
             kernel, stride, pad, (long long)in.shape[0], (long long)in.shape[1],
             (long long)want_h, (long long)want_w, (long long)out.shape[0],
             (long long)out.shape[1]);
    const KernelName entry = span_entry("resample.avgpool", {out, in});

    PoolParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.Ho = (uint32_t)out.shape[0];
    p.Wo = (uint32_t)out.shape[1];
    p.Hi = (uint32_t)in.shape[0];
    p.Wi = (uint32_t)in.shape[1];
    p.C = (uint32_t)out.shape[2];
    p.kh = p.kw = (uint32_t)kernel;
    p.stride_y = p.stride_x = (uint32_t)stride;
    p.pad_y = p.pad_x = (uint32_t)pad;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u, 0u};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void grid_sample_points(const Tensor& out, const Tensor& in, const Tensor& pos,
                        bool align_corners) {
    NN_CHECK(in.ndim == 3, "grid_sample_points expects an [H, W, C] map");
    NN_CHECK(pos.dtype == DType::F32 && pos.cols() == 2,
             "grid_sample_points: pos must be an f32 [N, 2] tensor");
    const int64_t N = pos.rows(), C = in.shape[2];
    NN_CHECK(out.rows() == N && out.cols() == C,
             "grid_sample_points: out is [%lld, %lld], expected [%lld, %lld]",
             (long long)out.rows(), (long long)out.cols(), (long long)N, (long long)C);
    if (N == 0) return;
    const KernelName entry = span_entry("misc.grid_sample_points", {out, in, pos});

    GridSampleParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.pos = pos.ptr;
    p.H = (uint32_t)in.shape[0];
    p.W = (uint32_t)in.shape[1];
    p.C = (uint32_t)C;
    p.N = (uint32_t)N;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u, (uint32_t)align_corners};
    vk::Stream::get().dispatchFlat(entry, spec, N * C, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

// One workgroup per row, so rows are independent: the grid folds over them and
// a tall tensor splits onto shifted pointers.
static void rowwise(const char* entry, const Tensor& out, const Tensor& x, float eps) {
    NN_CHECK(out.rows() == x.rows() && out.cols() == x.cols(), "%s: shapes differ",
             entry);
    const int64_t rows = x.rows(), cols = x.cols();
    if (rows == 0) return;

    const int64_t oe = dtype_size(out.dtype), xe = dtype_size(x.dtype);
    const int64_t per = span_rows_even(entry, cols * std::max(oe, xe));
    vk::SpecList spec{(uint32_t)(x.dtype == DType::F16), 0u, 0u};
    for (int64_t r0 = 0; r0 < rows; r0 += per) {
        NormalizeParams p{};
        p.out = out.ptr + (uint64_t)(r0 * cols) * oe;
        p.x = x.ptr + (uint64_t)(r0 * cols) * xe;
        p.rows = (uint32_t)std::min(per, rows - r0);
        p.cols = (uint32_t)cols;
        p.eps = eps;
        const vk::Stream::Fold fold = vk::Stream::fold1D(p.rows, 1);
        p.groups_per_row = fold.per_row;
        vk::Stream::get().dispatch(entry, spec, fold.per_row, fold.rows, 1, &p,
                                   sizeof(p));
    }
}

void l2_normalize_rows(const Tensor& out, const Tensor& x, float eps) {
    rowwise("misc.l2_normalize_rows", out, x, eps);
}

void softmax_rows(const Tensor& out, const Tensor& x) {
    rowwise("misc.softmax_rows", out, x, 0.0f);
}

void resize_binarize(const Tensor& out_u8, const Tensor& logits, int64_t Ho, int64_t Wo,
                     float threshold) {
    NN_CHECK(out_u8.dtype == DType::U8, "resize_binarize writes a u8 tensor");
    const KernelName entry = span_entry("resample.resize_binarize", {out_u8, logits});
    MaskExportParams p{};
    p.out = out_u8.ptr;
    p.x = logits.ptr;
    p.Ho = (uint32_t)Ho;
    p.Wo = (uint32_t)Wo;
    p.Hi = (uint32_t)logits.dim(0);
    p.Wi = (uint32_t)logits.dim(1);
    p.threshold = threshold;
    vk::SpecList spec{(uint32_t)(logits.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(entry, spec, (Ho * Wo + 3) / 4, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

}  // namespace nn
