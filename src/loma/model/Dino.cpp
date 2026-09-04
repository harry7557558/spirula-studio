// DeDoDe-G's frozen DINOv2 ViT-L/14, read against DINOv2's own
// vision_transformer.py: a pre-norm ViT with LayerScale and no register
// tokens. What LoMa takes from it is one thing -- x_norm_patchtokens, the
// final LayerNorm's patch rows -- so there are no taps and no projections
// here, unlike src/moge/model/Encoder.cpp's copy of the same network.
//
// LayerScale is a separate multiply rather than folded into the projection it
// follows: the fold is exact in fp32, but these matrices go to the device in
// f16 and a trained gamma reaches 1e-4, where the product is subnormal.

#include "loma/model/Model.h"

#include "loma/Common.h"
#include "loma/model/Dump.h"
#include "nn/Ops.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <cmath>
#include <vector>

namespace loma {
namespace {

using nn::Act;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

constexpr float kNormEps = 1e-6f;

float cubic_weight(float t, float a) {
    t = std::fabs(t);
    if (t <= 1.0f) return ((a + 2.0f) * t - (a + 3.0f)) * t * t + 1.0f;
    if (t < 2.0f) return ((a * t - 5.0f * a) * t + 8.0f * a) * t - 4.0f * a;
    return 0.0f;
}

// [gi, gi, C] -> [ho, wo, C], torch's bicubic with align_corners=False. The
// scale is NOT ho/gi: interpolate_pos_encoding passes (ho + 0.1)/gi, which
// sizes the output the same but moves every sample by ~0.2% of a cell.
std::vector<float> bicubic_resize(const std::vector<float>& src, int64_t gi, int64_t C,
                                  int64_t ho, int64_t wo) {
    std::vector<float> dst((size_t)ho * wo * C);
    const float a = -0.75f;
    const double inv_y = (double)gi / ((double)ho + 0.1);
    const double inv_x = (double)gi / ((double)wo + 0.1);
    for (int64_t oy = 0; oy < ho; ++oy) {
        const double sy = ((double)oy + 0.5) * inv_y - 0.5;
        const int64_t y0 = (int64_t)std::floor(sy);
        float wy[4];
        for (int k = 0; k < 4; ++k) wy[k] = cubic_weight((float)(sy - (y0 - 1 + k)), a);
        for (int64_t ox = 0; ox < wo; ++ox) {
            const double sx = ((double)ox + 0.5) * inv_x - 0.5;
            const int64_t x0 = (int64_t)std::floor(sx);
            float wx[4];
            for (int k = 0; k < 4; ++k) wx[k] = cubic_weight((float)(sx - (x0 - 1 + k)), a);
            float* out = &dst[(size_t)(oy * wo + ox) * C];
            for (int ky = 0; ky < 4; ++ky) {
                const int64_t yy = std::min<int64_t>(std::max<int64_t>(y0 - 1 + ky, 0), gi - 1);
                for (int kx = 0; kx < 4; ++kx) {
                    const int64_t xx =
                        std::min<int64_t>(std::max<int64_t>(x0 - 1 + kx, 0), gi - 1);
                    const float ww = wy[ky] * wx[kx];
                    if (ww == 0.0f) continue;
                    const float* sp = &src[(size_t)(yy * gi + xx) * C];
                    for (int64_t c = 0; c < C; ++c) out[c] += ww * sp[c];
                }
            }
        }
    }
    return dst;
}

}  // namespace

void Dino::release() {
    if (pos_blob_) vk::device_free(pos_blob_);
    pos_blob_ = 0;
    pos_gh_ = pos_gw_ = 0;
}

void Dino::ensurePosEmbed(const PyramidWeights& w, int64_t gh, int64_t gw) {
    if (pos_gh_ == gh && pos_gw_ == gw && pos_blob_) return;
    const PyramidHparams& hp = w.hparams();
    const int64_t D = hp.dino_width;

    const std::vector<float> patch =
        bicubic_resize(w.dinoPatchPos(), hp.dino_pos_grid, D, gh, gw);

    if (pos_blob_) vk::device_free(pos_blob_);
    pos_blob_ = vk::device_alloc((uint64_t)(patch.size() + (size_t)D) * 4, "loma-dino-pos");
    pos_patch_ = Tensor(pos_blob_, DType::F32, gh * gw, D);
    pos_cls_ = Tensor(pos_blob_ + (uint64_t)patch.size() * 4, DType::F32, 1, D);
    nn::tensor_from_host(pos_patch_, patch.data(), (int64_t)patch.size());
    nn::tensor_from_host(pos_cls_, w.dinoClsPos().data(), D);
    vk::Stream::get().sync();
    pos_gh_ = gh;
    pos_gw_ = gw;
}

int64_t Dino::planBytes(const PyramidHparams& hp, int64_t S) {
    const int64_t D = hp.dino_width;
    const int64_t N = 1 + (S / hp.dino_patch) * (S / hp.dino_patch);
    // The token sequence, the normed copy, one block's scratch, and the fused
    // qkv / MLP hidden, which is the widest thing in the pass.
    return (3 * N * D + N * 3 * D + N * hp.dino_mlp) * 4;
}

// The whole encoder. `map` comes back as [gh, gw, D] -- the patch rows of the
// final LayerNorm, which is what DeDoDe-G's coarsest refiner reads.
void Dino::run(const PyramidWeights& w, vk::Arena& arena, const Tensor& image,
               const Tensor& map, int64_t gh, int64_t gw) {
    const PyramidHparams& hp = w.hparams();
    const int64_t D = hp.dino_width;
    const int64_t P = hp.dino_patch;
    const int64_t np = gh * gw, N = 1 + np;
    const std::string d = hp.dino_prefix;
    ensurePosEmbed(w, gh, gw);

    vk::ArenaScope tokens(arena);
    Tensor x = nn::arena_tensor(arena, DType::F32, N, D);
    {
        vk::ArenaScope scope(arena);
        // patch_embed is a stride-P convolution, so its [gh, gw, D] output is
        // already the patch token sequence in memory.
        Tensor patches = nn::arena_tensor(arena, DType::F32, gh, gw, D);
        nn::ConvOpts co;
        co.stride_y = co.stride_x = (int)P;
        co.bias = w.get(d + "patch_embed.proj.bias");
        nn::conv2d(arena, patches, image, w.get(d + "patch_embed.proj.weight"), (int)P,
                   (int)P, co);

        Tensor patch_rows(patches.ptr, DType::F32, np, D);
        nn::add(patch_rows, patch_rows, pos_patch_);
        nn::add(Tensor(x.ptr, DType::F32, 1, D), w.get(d + "cls_token"), pos_cls_);
        nn::strided_copy(Tensor(x.ptr + (uint64_t)D * 4, DType::F32, np, D), patch_rows,
                         np, D, D, D);
    }

    for (int b = 0; b < hp.dino_blocks; ++b) {
        vk::ArenaScope scope(arena);
        const std::string q = d + "blocks." + std::to_string(b) + ".";
        Tensor t = nn::arena_tensor(arena, DType::F32, N, D);

        // x = x + ls1 * proj(attn(norm1(x)))
        nn::layer_norm(t, x, w.get(q + "norm1.weight"), w.get(q + "norm1.bias"), kNormEps);
        {
            vk::ArenaScope inner(arena);
            Tensor qkv = nn::arena_tensor(arena, DType::F32, N, 3 * D);
            LinearOpts lo;
            lo.bias = w.get(q + "attn.qkv.bias");
            nn::linear(qkv, t, w.get(q + "attn.qkv.weight"), lo);

            Tensor attn = nn::arena_tensor(arena, DType::F32, N, D);
            nn::AttnOpts ao;
            ao.n_heads = hp.dino_heads;
            ao.head_dim = (int)(D / hp.dino_heads);
            ao.arena = &arena;
            ao.q_stride = ao.k_stride = ao.v_stride = 3 * D;
            nn::attention(attn, qkv, qkv.offsetElems(D), qkv.offsetElems(2 * D), N, N, ao);

            LinearOpts po;
            po.bias = w.get(q + "attn.proj.bias");
            nn::linear(t, attn, w.get(q + "attn.proj.weight"), po);
        }
        nn::mul(t, t, w.get(q + "ls1.gamma"));
        nn::add(x, x, t);

        // x = x + ls2 * mlp(norm2(x))
        nn::layer_norm(t, x, w.get(q + "norm2.weight"), w.get(q + "norm2.bias"), kNormEps);
        {
            vk::ArenaScope inner(arena);
            Tensor hid = nn::arena_tensor(arena, DType::F32, N, hp.dino_mlp);
            LinearOpts lo;
            lo.bias = w.get(q + "mlp.fc1.bias");
            lo.act = Act::GeluErf;
            nn::linear(hid, t, w.get(q + "mlp.fc1.weight"), lo);
            LinearOpts o2;
            o2.bias = w.get(q + "mlp.fc2.bias");
            nn::linear(t, hid, w.get(q + "mlp.fc2.weight"), o2);
        }
        nn::mul(t, t, w.get(q + "ls2.gamma"));
        nn::add(x, x, t);
    }

    // x_norm_patchtokens: the final LayerNorm, class row dropped.
    nn::layer_norm(map.view(np, D), Tensor(x.ptr + (uint64_t)D * 4, DType::F32, np, D),
                   w.get(d + "norm.weight"), w.get(d + "norm.bias"), kNormEps);
    dump_tensor("dino_map", map, {gh, gw, D});
}

}  // namespace loma
