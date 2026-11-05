#include "nn/core/Error.h"
#include "nn/Ops.h"
#include "nn/vk/Stream.h"

namespace nn {

namespace {

struct DequantNf4Params {
    uint64_t out, codes, absmax, residual, row_scale;
    uint32_t words, K, groups_per_row, _pad0;
};

}  // namespace

void dequant_nf4(const Tensor& out, const Tensor& codes, const Tensor& absmax,
                 const Nf4Opts& o) {
    const int64_t N = out.rows(), K = out.cols();
    NN_CHECK(out.dtype == DType::F16, "dequant_nf4 writes f16");
    NN_CHECK(K % 64 == 0, "dequant_nf4: K = %lld is not a whole number of 64-blocks",
             (long long)K);
    NN_CHECK(codes.dtype == DType::U8 && codes.numel() == N * K / 2,
             "dequant_nf4: %lld code bytes for a %lldx%lld matrix", (long long)codes.numel(),
             (long long)N, (long long)K);
    NN_CHECK(absmax.numel() == N * K / 64, "dequant_nf4: %lld scales for %lld blocks",
             (long long)absmax.numel(), (long long)(N * K / 64));
    if (o.residual.valid())
        NN_CHECK(o.residual.dtype == DType::F32 && o.residual.numel() == N * K,
                 "dequant_nf4: the residual must be an f32 [N, K]");
    if (o.row_scale.valid())
        NN_CHECK(o.row_scale.numel() >= N, "dequant_nf4: row_scale is shorter than N");
    check_span("dequant_nf4", {out, codes, absmax, o.residual, o.row_scale});

    DequantNf4Params p{};
    p.out = out.ptr;
    p.codes = codes.ptr;
    p.absmax = absmax.ptr;
    p.residual = vk::or_fallback(o.residual.ptr);
    p.row_scale = vk::or_fallback(o.row_scale.ptr);
    p.words = (uint32_t)(N * K / 8);
    p.K = (uint32_t)K;
    const vk::SpecList spec{(uint32_t)o.residual.valid(), (uint32_t)o.row_scale.valid()};
    vk::Stream::get().dispatchFlat("quant.dequant_nf4", spec, p.words, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

}  // namespace nn
