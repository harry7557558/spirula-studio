// The neck and the three heads, read against ConvStack in MoGe's modules.py:
// one shape used four times, five levels at 1x..16x the patch grid.
//
// Three conventions here are load-bearing and none is in the weight list:
// every 3x3 pads by REPLICATION; the residual blocks carry no normalization at
// all, so a block is x + conv(relu(conv(relu(x)))); and the level-3 resampler
// is a bilinear 2x upsample where the other three are transposed convolutions,
// which is visible only as whether `resamplers.<i>.0.weight` exists.

#include "moge/model/Model.h"

#include "moge/Common.h"
#include "nn/Ops.h"
#include "nn/vk/Stream.h"

#include <string>

namespace moge {
namespace {

using nn::Act;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

std::string fmt(const char* mod, const char* suffix, int a, int b = -1) {
    char buf[192];
    if (b < 0) std::snprintf(buf, sizeof buf, "%s.%s.%d", mod, suffix, a);
    else std::snprintf(buf, sizeof buf, "%s.%s.%d.%d", mod, suffix, a, b);
    return buf;
}

// x += conv3x3(relu(conv3x3(relu(x)))), replicate-padded, no normalization.
void res_block(Model& m, const Tensor& x, const char* mod, int level, int idx) {
    const std::string p = fmt(mod, "res_blocks", level, idx) + ".layers.";
    const int64_t H = x.shape[0], W = x.shape[1], C = x.shape[2];

    vk::ArenaScope scope(m.arena);
    Tensor t = nn::arena_tensor(m.arena, DType::F32, H, W, C);
    nn::unary(t, x, Act::Relu);

    nn::ConvOpts co;
    co.pad_y = co.pad_x = 1;
    co.pad_replicate = true;
    // layers.3 is an Identity, so the ReLU that follows the first convolution
    // is this epilogue rather than a pass of its own.
    co.act = Act::Relu;
    co.bias = m.weights.get(p + "2.bias");
    Tensor u = nn::arena_tensor(m.arena, DType::F32, H, W, C);
    nn::conv2d(m.arena, u, t, m.weights.get(p + "2.weight"), 3, 3, co);

    co.act = Act::None;
    co.bias = m.weights.get(p + "5.bias");
    nn::conv2d(m.arena, t, u, m.weights.get(p + "5.weight"), 3, 3, co);
    nn::add(x, x, t);
}

// 2x up then a replicate-padded 3x3 convolution. `out` is [2*Hi, 2*Wi, Cout].
void resampler(Model& m, const Tensor& out, const Tensor& in, const char* mod, int i) {
    const std::string p = fmt(mod, "resamplers", i) + ".";
    const int64_t Ho = out.shape[0], Wo = out.shape[1];
    // The transposed convolution changes the channel count before the 3x3; the
    // bilinear upsample does not. Either way the 3x3's input width says which.
    const Tensor w3 = m.weights.get(p + "1.weight");
    const int64_t mid = w3.shape[1];

    vk::ArenaScope scope(m.arena);
    Tensor up = nn::arena_tensor(m.arena, DType::F32, Ho, Wo, mid);
    if (m.weights.has(p + "0.weight"))
        nn::conv_transpose2x2(m.arena, up, in, m.weights.get(p + "0.weight"),
                              m.weights.get(p + "0.bias"));
    else
        nn::resize_bilinear(up, in);

    nn::ConvOpts co;
    co.pad_y = co.pad_x = 1;
    co.pad_replicate = true;
    co.bias = m.weights.get(p + "1.bias");
    nn::conv2d(m.arena, out, up, w3, 3, 3, co);
}

// out += conv1x1(in), which nn::linear does in place.
void add_projected(Model& m, const Tensor& out, const Tensor& in, const std::string& p) {
    const int64_t rows = out.rows(), cols = out.cols();
    LinearOpts lo;
    lo.bias = m.weights.get(p + ".bias");
    lo.residual = out.view(rows, cols);
    nn::linear(out.view(rows, cols), in.view(rows, in.cols()),
               m.weights.get(p + ".weight").asMatrix(), lo);
}

}  // namespace

void Model::decode(const Features& f, int64_t H, int64_t W, bool want_points,
                   bool want_normal, bool want_mask, Outputs* out, int refine_steps) {
    const Hparams& h = hp();
    const int64_t D = h.embed_dim;
    const int64_t np = f.gh * f.gw;

    // ---- the shared neck ---------------------------------------------------
    Tensor neck[Hparams::kLevels];
    for (int l = 0; l < Hparams::kLevels; ++l)
        neck[l] = nn::arena_tensor(arena, DType::F32, f.gh << l, f.gw << l, h.ch[l]);
    {
        vk::ArenaScope scope(arena);
        Tensor cat = nn::arena_tensor(arena, DType::F32, np, D + 2);
        nn::strided_copy(cat, f.map.view(np, D), np, D, D, D + 2);
        nn::strided_copy(cat.offsetElems(D), uv[0].view(np, 2), np, 2, 2, D + 2);

        nn::ConvOpts co;
        co.bias = weights.get("neck.input_blocks.0.bias");
        nn::conv2d(arena, neck[0].view(f.gh, f.gw, h.ch[0]),
                   cat.view(f.gh, f.gw, D + 2), weights.get("neck.input_blocks.0.weight"),
                   1, 1, co);
    }
    for (int l = 1; l < Hparams::kLevels; ++l) {
        vk::ArenaScope scope(arena);
        resampler(*this, neck[l], neck[l - 1], "neck", l - 1);
        add_projected(*this, neck[l], uv[l], fmt("neck", "input_blocks", l));
        const int nb = weights.resBlocks("neck", l);
        for (int j = 0; j < nb; ++j) res_block(*this, neck[l], "neck", l, j);
    }
    if (dump_enabled())
        for (int l = 0; l < Hparams::kLevels; ++l)
            dump_tensor(("neck" + std::to_string(l)).c_str(), neck[l],
                        {f.gh << l, f.gw << l, h.ch[l]});

    // ---- one head at a time, each rewound before the next -------------------
    const int64_t Hf = f.gh << (Hparams::kLevels - 1), Wf = f.gw << (Hparams::kLevels - 1);
    auto run_head = [&](const char* mod, const Tensor& dst) {
        vk::ArenaScope scope(arena);
        Tensor x = nn::arena_tensor(arena, DType::F32, f.gh, f.gw, h.ch[0]);
        {
            nn::ConvOpts co;
            co.bias = weights.get(std::string(mod) + ".input_blocks.0.bias");
            nn::conv2d(arena, x, neck[0],
                       weights.get(std::string(mod) + ".input_blocks.0.weight"), 1, 1, co);
        }
        for (int l = 1; l < Hparams::kLevels; ++l) {
            Tensor y = nn::arena_tensor(arena, DType::F32, f.gh << l, f.gw << l, h.ch[l]);
            resampler(*this, y, x, mod, l - 1);
            add_projected(*this, y, neck[l], fmt(mod, "input_blocks", l));
            const int nb = weights.resBlocks(mod, l);
            for (int j = 0; j < nb; ++j) res_block(*this, y, mod, l, j);
            x = y;
        }
        // ndim spelled out: the mask head's single channel would otherwise
        // infer a 2-D tensor and every [H, W, C] op would refuse it.
        Tensor raw = nn::arena_tensor(arena, DType::F32, Hf, Wf, dst.shape[2], 1, 3);
        nn::ConvOpts co;
        co.bias = weights.get(std::string(mod) + ".output_blocks.4.bias");
        nn::conv2d(arena, raw, x,
                   weights.get(std::string(mod) + ".output_blocks.4.weight"), 1, 1, co);
        if (dump_enabled())
            dump_tensor((std::string(mod) + "_raw").c_str(), raw, {Hf, Wf, dst.shape[2]});
        // MoGe-3 refines at the head's own resolution and resizes after.
        if (refine_steps > 0 && h.has_refiner && std::string(mod) == "points_head") {
            refine(f, raw, refine_steps);
            if (dump_enabled()) dump_tensor("points_head_refined", raw, {Hf, Wf, 3});
        }
        nn::resize_bilinear(dst, raw);
    };

    if (want_points) {
        out->points = nn::arena_tensor(arena, DType::F32, H, W, 3, 1, 3);
        run_head("points_head", out->points);
    }
    if (want_normal) {
        NN_CHECK(h.has_normal, "this checkpoint has no normal head");
        out->normal = nn::arena_tensor(arena, DType::F32, H, W, 3, 1, 3);
        run_head("normal_head", out->normal);
    }
    if (want_mask) {
        NN_CHECK(h.has_mask, "this checkpoint has no mask head");
        out->mask = nn::arena_tensor(arena, DType::F32, H, W, 1, 1, 3);
        run_head("mask_head", out->mask);
    }

    // ---- the metric scale, off the class token alone ------------------------
    if (h.has_scale) {
        out->scale = nn::arena_tensor(arena, DType::F32, 1, 1, 1, 1, 2);
        vk::ArenaScope scope(arena);
        Tensor t = nn::arena_tensor(arena, DType::F32, 1, D);
        LinearOpts lo;
        lo.bias = weights.get("scale_head.0.bias");
        lo.act = Act::Relu;
        nn::linear(t, f.cls, weights.get("scale_head.0.weight"), lo);
        lo.bias = weights.get("scale_head.2.bias");
        Tensor t2 = nn::arena_tensor(arena, DType::F32, 1, D);
        nn::linear(t2, t, weights.get("scale_head.2.weight"), lo);

        LinearOpts so;
        so.bias = weights.get("scale_head.4.bias");
        so.act = Act::Exp;
        nn::linear(out->scale, t2, weights.get("scale_head.4.weight"), so);
    }
}

}  // namespace moge
