#include "nn/Blocks.h"

namespace nn {

void concat_ffn(vk::Arena& arena, const Tensor& x, const Tensor& message, int64_t n, int64_t d,
                const ConcatFfn& w) {
    vk::ArenaScope scope(arena);
    Tensor cat = arena_tensor(arena, DType::F32, n, 2 * d);
    strided_copy(cat, x, n, d, d, 2 * d);
    Tensor msg = message;
    if (w.msg_ln_w.valid()) {
        msg = arena_tensor(arena, DType::F32, n, d);
        layer_norm(msg, message, w.msg_ln_w, w.msg_ln_b, w.eps);
    }
    strided_copy(cat.offsetElems(d), msg, n, d, d, 2 * d);
    Tensor h = arena_tensor(arena, DType::F32, n, 2 * d);
    LinearOpts l0;
    l0.bias = w.b0;
    linear(h, cat, w.w0, l0);
    layer_norm(h, h, w.ln_w, w.ln_b, w.eps);
    unary(h, h, Act::GeluErf);
    LinearOpts l1;
    l1.bias = w.b3;
    l1.residual = x;
    linear(x, h, w.w3, l1);
}

}  // namespace nn
