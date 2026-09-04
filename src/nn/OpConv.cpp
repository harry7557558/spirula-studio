// Convolution launchers. See shaders/conv.slang for why the k x k case goes
// through im2col + GEMM and what the chunking is for.

#include "nn/core/Error.h"
#include "nn/Ops.h"
#include "nn/vk/Stream.h"

#include <algorithm>

namespace nn {

namespace {

struct Im2ColParams {
    uint64_t out, x;
    uint32_t Hi, Wi, Ci, Ho, Wo, kh, kw;
    uint32_t stride_y, stride_x, pad_y, pad_x;
    uint32_t p0, P, groups_per_row;
};

struct DeformIm2ColParams {
    uint64_t out, x, offset;
    uint32_t Hi, Wi, Ci, Ho, Wo, kh, kw;
    uint32_t stride_y, stride_x, pad_y, pad_x;
    uint32_t p0, P;
    float    max_offset;
    uint32_t groups_per_row;
};

struct PatchGatherParams {
    uint64_t out, x, centers;
    uint32_t Hi, Wi, C, N, k, groups_per_row;
};

struct DepthwiseParams {
    uint64_t out, x, w, bias;
    uint32_t Hi, Wi, Ho, Wo, C, kh, kw;
    uint32_t stride_y, stride_x, pad_y, pad_x, groups_per_row;
};

struct PatchifyParams {
    uint64_t out, x;
    uint32_t Hi, Wi, Ci, patch, groups_per_row, _pad0;
};

struct ConvTScatterParams {
    uint64_t out, packed, bias;
    uint32_t Hi, Wi, Co, groups_per_row, _pad0;
};

// Column-buffer budget. A full im2col of the seg head's 288x288x256 3x3 conv is
// 764 MiB; chunking the output positions bounds it here while keeping the GEMM
// tiles large enough to stay compute bound.
constexpr int64_t kMaxColBytes = 32ll << 20;

}  // namespace

void conv2d(vk::Arena& arena, const Tensor& out, const Tensor& in, const Tensor& w_in,
            int kh, int kw, const ConvOpts& o) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3, "conv2d expects [H, W, C] tensors");
    const Tensor w = w_in.asMatrix();  // [Cout, Cin*kh*kw]
    const int64_t Hi = in.shape[0], Wi = in.shape[1], Ci = in.shape[2];
    const int64_t Ho = out.shape[0], Wo = out.shape[1], Co = out.shape[2];
    const int64_t K = Ci * kh * kw;
    NN_CHECK(w.rows() == Co && w.cols() == K,
               "conv2d: weight is [%lld, %lld] but %lldx%lldx%lld -> %lld needs [%lld, %lld]",
               (long long)w.rows(), (long long)w.cols(), (long long)kh, (long long)kw,
               (long long)Ci, (long long)Co, (long long)Co, (long long)K);

    // 1x1 stride-1 is a plain Linear over the flattened positions -- no
    // column buffer, no extra pass.
    if (kh == 1 && kw == 1 && o.stride_y == 1 && o.stride_x == 1 && o.pad_y == 0 &&
        o.pad_x == 0) {
        LinearOpts lo;
        lo.bias = o.bias;
        lo.act = o.act;
        linear(out.view(Ho * Wo, Co), in.view(Hi * Wi, Ci), w, lo);
        return;
    }

    // Chunking bounds the column buffer and the output slice, but im2col reads
    // the source at arbitrary positions, so that one has to fit whole.
    const KernelName entry = span_entry("conv.im2col", {in, w});
    const int64_t positions = Ho * Wo;
    int64_t chunk = std::max<int64_t>(64, kMaxColBytes / (K * 4));
    chunk = std::min(chunk, positions);

    vk::ArenaScope scope(arena);
    Tensor cols = arena_tensor(arena, DType::F32, chunk, K);

    for (int64_t p0 = 0; p0 < positions; p0 += chunk) {
        const int64_t P = std::min(chunk, positions - p0);

        Im2ColParams ip{};
        ip.out = cols.ptr;
        ip.x = in.ptr;
        ip.Hi = (uint32_t)Hi;
        ip.Wi = (uint32_t)Wi;
        ip.Ci = (uint32_t)Ci;
        ip.Ho = (uint32_t)Ho;
        ip.Wo = (uint32_t)Wo;
        ip.kh = (uint32_t)kh;
        ip.kw = (uint32_t)kw;
        ip.stride_y = (uint32_t)o.stride_y;
        ip.stride_x = (uint32_t)o.stride_x;
        ip.pad_y = (uint32_t)o.pad_y;
        ip.pad_x = (uint32_t)o.pad_x;
        ip.p0 = (uint32_t)p0;
        ip.P = (uint32_t)P;
        vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u,
                          (uint32_t)o.pad_replicate};
        vk::Stream::get().dispatchFlat(entry, spec, P * K, 256, &ip, sizeof(ip),
                                       &ip.groups_per_row);

        LinearOpts lo;
        lo.bias = o.bias;
        lo.act = o.act;
        Tensor out_chunk(out.ptr + (uint64_t)(p0 * Co) * 4, DType::F32, P, Co);
        linear(out_chunk, cols.view(P, K), w, lo);
    }
}

void deform_conv2d(vk::Arena& arena, const Tensor& out, const Tensor& in,
                   const Tensor& offset, const Tensor& w_in, int kh, int kw,
                   float max_offset, const ConvOpts& o) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3 && offset.ndim == 3,
             "deform_conv2d expects [H, W, C] tensors");
    const Tensor w = w_in.asMatrix();
    const int64_t Hi = in.shape[0], Wi = in.shape[1], Ci = in.shape[2];
    const int64_t Ho = out.shape[0], Wo = out.shape[1], Co = out.shape[2];
    const int64_t K = Ci * kh * kw;
    NN_CHECK(w.rows() == Co && w.cols() == K,
             "deform_conv2d: weight is [%lld, %lld] but %lldx%lldx%lld -> %lld needs "
             "[%lld, %lld]",
             (long long)w.rows(), (long long)w.cols(), (long long)kh, (long long)kw,
             (long long)Ci, (long long)Co, (long long)Co, (long long)K);
    NN_CHECK(offset.shape[0] == Ho && offset.shape[1] == Wo &&
                 offset.shape[2] == 2 * kh * kw,
             "deform_conv2d: offset is [%lld, %lld, %lld], expected [%lld, %lld, %lld]",
             (long long)offset.shape[0], (long long)offset.shape[1],
             (long long)offset.shape[2], (long long)Ho, (long long)Wo,
             (long long)(2 * kh * kw));
    // The offset map is read as raw f32 by the kernel; an f16 one would need a
    // second load path for two values per tap and buys nothing at this size.
    NN_CHECK(offset.dtype == DType::F32, "deform_conv2d: offset must be f32");
    const KernelName entry = span_entry("conv.deform_im2col", {in, offset, w});

    const int64_t positions = Ho * Wo;
    int64_t chunk = std::max<int64_t>(64, kMaxColBytes / (K * 4));
    chunk = std::min(chunk, positions);

    vk::ArenaScope scope(arena);
    Tensor cols = arena_tensor(arena, DType::F32, chunk, K);

    for (int64_t p0 = 0; p0 < positions; p0 += chunk) {
        const int64_t P = std::min(chunk, positions - p0);

        DeformIm2ColParams ip{};
        ip.out = cols.ptr;
        ip.x = in.ptr;
        ip.offset = offset.ptr;
        ip.Hi = (uint32_t)Hi;
        ip.Wi = (uint32_t)Wi;
        ip.Ci = (uint32_t)Ci;
        ip.Ho = (uint32_t)Ho;
        ip.Wo = (uint32_t)Wo;
        ip.kh = (uint32_t)kh;
        ip.kw = (uint32_t)kw;
        ip.stride_y = (uint32_t)o.stride_y;
        ip.stride_x = (uint32_t)o.stride_x;
        ip.pad_y = (uint32_t)o.pad_y;
        ip.pad_x = (uint32_t)o.pad_x;
        ip.p0 = (uint32_t)p0;
        ip.P = (uint32_t)P;
        ip.max_offset = max_offset;
        vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u};
        vk::Stream::get().dispatchFlat(entry, spec, P * K, 256, &ip, sizeof(ip),
                                       &ip.groups_per_row);

        LinearOpts lo;
        lo.bias = o.bias;
        lo.act = o.act;
        Tensor out_chunk(out.ptr + (uint64_t)(p0 * Co) * 4, DType::F32, P, Co);
        linear(out_chunk, cols.view(P, K), w, lo);
    }
}

void patch_gather(const Tensor& out, const Tensor& in, const Tensor& centers, int k) {
    NN_CHECK(in.ndim == 3, "patch_gather expects an [H, W, C] map");
    NN_CHECK(centers.dtype == DType::I32 && centers.cols() == 2,
             "patch_gather: centers must be an i32 [N, 2] tensor");
    const int64_t N = centers.rows();
    const int64_t C = in.shape[2];
    NN_CHECK(out.rows() == N && out.cols() == C * k * k,
             "patch_gather: out is [%lld, %lld], expected [%lld, %lld]",
             (long long)out.rows(), (long long)out.cols(), (long long)N,
             (long long)(C * k * k));
    if (N == 0) return;
    const KernelName entry = span_entry("conv.patch_gather", {out, in, centers});

    PatchGatherParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.centers = centers.ptr;
    p.Hi = (uint32_t)in.shape[0];
    p.Wi = (uint32_t)in.shape[1];
    p.C = (uint32_t)C;
    p.N = (uint32_t)N;
    p.k = (uint32_t)k;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(entry, spec, N * C * k * k, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void conv2d_depthwise(const Tensor& out, const Tensor& in, const Tensor& w_in, int kh,
                      int kw, const ConvOpts& o) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3, "conv2d_depthwise expects [H, W, C]");
    const Tensor w = w_in.asMatrix();  // [C, kh*kw] from the checkpoint's [C,1,kh,kw]
    const KernelName entry = span_entry("conv.conv2d_depthwise", {out, in, w});
    DepthwiseParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.w = w.ptr;
    p.bias = vk::or_fallback(o.bias.ptr);
    p.Hi = (uint32_t)in.shape[0];
    p.Wi = (uint32_t)in.shape[1];
    p.Ho = (uint32_t)out.shape[0];
    p.Wo = (uint32_t)out.shape[1];
    p.C = (uint32_t)out.shape[2];
    p.kh = (uint32_t)kh;
    p.kw = (uint32_t)kw;
    p.stride_y = (uint32_t)o.stride_y;
    p.stride_x = (uint32_t)o.stride_x;
    p.pad_y = (uint32_t)o.pad_y;
    p.pad_x = (uint32_t)o.pad_x;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), (uint32_t)o.act};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void conv_transpose2x2(vk::Arena& arena, const Tensor& out, const Tensor& in,
                       const Tensor& w_packed, const Tensor& bias, Act act) {
    NN_CHECK(out.ndim == 3 && in.ndim == 3, "conv_transpose2x2 expects [H, W, C]");
    const int64_t Hi = in.shape[0], Wi = in.shape[1], Ci = in.shape[2];
    const int64_t Co = out.shape[2];
    NN_CHECK(out.shape[0] == Hi * 2 && out.shape[1] == Wi * 2,
               "conv_transpose2x2: output must be exactly 2x the input");
    NN_CHECK(w_packed.rows() == Co * 4 && w_packed.cols() == Ci,
               "conv_transpose2x2: packed weight must be [Cout*4, Cin]");

    vk::ArenaScope scope(arena);
    Tensor packed = arena_tensor(arena, DType::F32, Hi * Wi, Co * 4);
    // Bias and activation belong to the scatter, not the GEMM: one bias entry
    // serves all four taps of a channel.
    linear(packed, in.view(Hi * Wi, Ci), w_packed);

    const KernelName entry = span_entry("misc.convt_scatter", {out, packed});
    ConvTScatterParams p{};
    p.out = out.ptr;
    p.packed = packed.ptr;
    p.bias = vk::or_fallback(bias.ptr);
    p.Hi = (uint32_t)Hi;
    p.Wi = (uint32_t)Wi;
    p.Co = (uint32_t)Co;
    vk::SpecList spec{0u, (uint32_t)act};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

void patchify(const Tensor& out, const Tensor& in, int patch) {
    NN_CHECK(in.ndim == 3, "patchify expects [H, W, C]");
    const KernelName entry = span_entry("conv.patchify", {out, in});
    PatchifyParams p{};
    p.out = out.ptr;
    p.x = in.ptr;
    p.Hi = (uint32_t)in.shape[0];
    p.Wi = (uint32_t)in.shape[1];
    p.Ci = (uint32_t)in.shape[2];
    p.patch = (uint32_t)patch;
    vk::SpecList spec{(uint32_t)(in.dtype == DType::F16), 0u};
    vk::Stream::get().dispatchFlat(entry, spec, out.numel(), 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

}  // namespace nn
