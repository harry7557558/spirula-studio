#pragma once
// MPSGraph attention and GEMM (OpMetal.mm). Each returns false when the op is
// not one it takes, and the caller runs the Vulkan kernel instead.

#include "nn/Ops.h"

namespace nn {

bool metal_attention(const Tensor& out, const Tensor& q, const Tensor& k, const Tensor& v,
                     int64_t nq, int64_t nk, const AttnOpts& o);
bool metal_gemm(const Tensor& out, const Tensor& x, const Tensor& w, const Tensor& bias,
                const Tensor& residual, Act act, float alpha, int64_t x_row_stride,
                float x_scale = 1.0f);

}  // namespace nn
