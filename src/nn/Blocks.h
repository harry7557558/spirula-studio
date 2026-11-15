#pragma once
// Composites more than one model is built from, made only of the ops in Ops.h.
// Weights are arguments: nothing here knows a checkpoint or a width.

#include "nn/Ops.h"

namespace nn {

// The LightGlue-family message FFN (LightGlue, LoMa, FlexGlue), in place:
// x[n, d] += W3 . GELU(LN(W0 . cat(x, LN_m(message)) + b0)) + b3, with
// W0 [2d, 2d] and W3 [d, 2d]; LN_m is skipped when its weight is empty.
struct ConcatFfn {
    Tensor w0, b0, ln_w, ln_b, w3, b3;
    Tensor msg_ln_w, msg_ln_b;
    float  eps = 1e-5f;   // both LayerNorms
};
void concat_ffn(vk::Arena& arena, const Tensor& x, const Tensor& message, int64_t n, int64_t d,
                const ConcatFfn& w);

}  // namespace nn
