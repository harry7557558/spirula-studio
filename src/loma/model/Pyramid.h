#pragma once
// The half of DaD and DeDoDe that is the same network: a VGG encoder tapped
// before every MaxPool, then a cascade of ConvRefiners from the coarsest tap
// to the finest, each adding to a running head and handing the next level a
// context. The detector's head is one channel of logits, a descriptor's is 128
// or 256 channels of description, and the other difference is which filter the
// head is upsampled with -- bicubic for the detector, bilinear for the rest.
//
// Read against `ConvRefiner` and `Decoder` in the LoMa repository's
// dedode.py and dad.py; the two are copies of each other there too.

#include "loma/model/Dump.h"
#include "loma/model/Weights.h"
#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/vk/Memory.h"

#include <cstdio>
#include <string>
#include <vector>

namespace loma {

// A feature map the cascade consumes, with the size the level runs at.
struct Level {
    nn::Tensor map;
    int64_t    h = 0, w = 0, c = 0;
};

struct Pyramid {
    PyramidWeights w;
    vk::Arena*     arena = nullptr;
    std::string    prefix;   // "det" or "desc"

    const PyramidHparams& hp() const { return w.hparams(); }

    void convRelu(const nn::Tensor& out, const nn::Tensor& in, int idx) const {
        nn::ConvOpts o;
        o.pad_y = o.pad_x = 1;
        o.bias = w.getf("%s%d.bias", hp().vgg_prefix.c_str(), idx);
        o.act = nn::Act::Relu;
        nn::conv2d(*arena, out, in, w.getf("%s%d.weight", hp().vgg_prefix.c_str(), idx), 3,
                   3, o);
    }

    // The VGG encoder, appending one tap per MaxPool, finest first. Each tap
    // is allocated outside the scope that frees its block's intermediates,
    // because the cascade reads all of them.
    void encode(std::vector<Level>& taps, const nn::Tensor& img, int64_t H,
                int64_t W) const {
        const PyramidHparams& p = hp();
        nn::Tensor cur = img;
        int64_t h = H, ww = W;
        size_t conv = 0;
        for (size_t t = 0; t < p.tap_at.size(); ++t) {
            Level lv;
            lv.h = h;
            lv.w = ww;
            lv.c = p.tap_width[t];
            lv.map = nn::arena_tensor(*arena, nn::DType::F32, h, ww, lv.c);
            {
                vk::ArenaScope scope(*arena);
                nn::Tensor x = cur;
                for (; conv < (size_t)p.tap_at[t]; ++conv) {
                    const int64_t co = w.getf("%s%d.bias", p.vgg_prefix.c_str(),
                                              p.conv_index[conv]).shape[0];
                    nn::Tensor y = nn::arena_tensor(*arena, nn::DType::F32, h, ww, co);
                    convRelu(y, x, p.conv_index[conv]);
                    x = y;
                }
                convRelu(lv.map, x, p.conv_index[conv++]);
            }
            if (dump_enabled()) {
                char nm[64];
                std::snprintf(nm, sizeof nm, "%s_tap%zu", prefix.c_str(), t);
                dump_tensor(nm, lv.map, {h, ww, lv.c});
            }
            taps.push_back(lv);
            if (t + 1 == p.tap_at.size()) break;
            nn::Tensor pooled =
                nn::arena_tensor(*arena, nn::DType::F32, h / 2, ww / 2, lv.c);
            nn::maxpool2x2(pooled, lv.map);
            cur = pooled;
            h /= 2;
            ww /= 2;
        }
    }

    void conv1x1(const nn::Tensor& out, const nn::Tensor& in, const std::string& name,
                 nn::Act act) const {
        nn::ConvOpts o;
        o.bias = w.get(name + ".bias");
        o.act = act;
        nn::conv2d(*arena, out, in, w.get(name + ".weight"), 1, 1, o);
    }

    // One ConvRefiner: a 1x1 in-projection, `blocks` depthwise-5x5 + 1x1 pairs,
    // the residual halved by 1.4, then the 1x1 out-projection.
    void refine(const nn::Tensor& out, const nn::Tensor& in, const RefinerSpec& r,
                int64_t h, int64_t ww) const {
        vk::ArenaScope scope(*arena);
        const std::string q = prefix + ".decoder.layers." + std::to_string(r.scale);
        const int64_t rows = h * ww;
        auto buf = [&] { return nn::arena_tensor(*arena, nn::DType::F32, h, ww, r.hidden); };

        // Three buffers. Once the depthwise has read `x` the buffer it came
        // from is dead, so the 1x1 writes back into it -- except on the first
        // block, where `x` is the residual source and has to survive.
        nn::Tensor tmp = buf(), x0 = buf(), roll = buf();
        conv1x1(tmp, in, q + ".block1.0", nn::Act::Relu);
        conv1x1(x0, tmp, q + ".block1.3", nn::Act::None);

        nn::Tensor x = x0;
        for (int i = 0; i < r.blocks; ++i) {
            const std::string hb = q + ".hidden_blocks." + std::to_string(i);
            nn::ConvOpts d;
            d.pad_y = d.pad_x = 2;
            d.bias = w.get(hb + ".0.bias");
            d.act = nn::Act::Relu;
            nn::conv2d_depthwise(tmp, x, w.get(hb + ".0.weight"), 5, 5, d);
            nn::Tensor dst = (x.ptr == x0.ptr) ? roll : x;
            conv1x1(dst, tmp, hb + ".3", nn::Act::None);
            x = dst;
        }
        nn::add(x.view(rows, r.hidden), x.view(rows, r.hidden), x0.view(rows, r.hidden),
                1.0f / 1.4f, 1.0f / 1.4f);
        conv1x1(out, x, q + ".out_conv", nn::Act::None);
    }

    // The (h, w, c) of every cascade level, coarse first. planBytes has to size
    // the arena before anything runs, so it cannot ask encode(); deriving the
    // sequence twice in two files is what let the plan drift from the cascade.
    static std::vector<Level> planLevels(const PyramidHparams& p, int64_t H, int64_t W) {
        std::vector<Level> taps;
        int64_t h = H, ww = W;
        for (size_t t = 0; t < p.tap_at.size(); ++t) {
            Level lv;
            lv.h = h;
            lv.w = ww;
            lv.c = p.tap_width[t];
            taps.push_back(lv);
            h /= 2;
            ww /= 2;
        }
        std::vector<Level> out;
        if (p.dino_width) {
            Level lv;
            lv.h = H / p.dino_patch;
            lv.w = W / p.dino_patch;
            lv.c = p.dino_width;
            out.push_back(lv);
        }
        out.insert(out.end(), taps.rbegin(), taps.rend());
        return out;
    }

    // Elements the context buffers need: `w` where one is written, `r` where
    // the other is read after the resize to the next level.
    static void planContext(const PyramidHparams& p, const std::vector<Level>& levels,
                            int64_t& ctx_w, int64_t& ctx_r) {
        for (size_t i = 0; i + 1 < levels.size(); ++i) {
            ctx_w = std::max(ctx_w, p.refiners[i].ctx * levels[i].h * levels[i].w);
            ctx_r = std::max(ctx_r,
                             p.refiners[i].ctx * levels[i + 1].h * levels[i + 1].w);
        }
    }

    // Floats encode() and cascade() need: `live` is what survives to the end,
    // `transient` the largest scoped peak sitting on top of it. Adds to both,
    // so the caller starts them at whatever it allocates itself.
    static void planPyramid(const PyramidHparams& p, const std::vector<Level>& levels,
                            int64_t& live, int64_t& transient) {
        const size_t n = levels.size(), nt = p.tap_at.size();

        // Every tap survives, and so does the pooled map feeding the next
        // block. A block's other convolutions are scoped, and all of them emit
        // the tap's width.
        int prev = -1;
        for (size_t t = 0; t < nt; ++t) {
            const Level& lv = levels[n - 1 - t];
            const int64_t lp = lv.h * lv.w;
            live += lp * lv.c;
            if (t + 1 < nt) live += lp / 4 * lv.c;
            transient = std::max(transient, (int64_t)(p.tap_at[t] - prev - 1) * lp * lv.c);
            prev = p.tap_at[t];
        }

        int64_t ctx_w = 0, ctx_r = 0;
        planContext(p, levels, ctx_w, ctx_r);
        live += levels[n - 1].h * levels[n - 1].w * p.head + ctx_w + ctx_r;

        // One refiner: its concatenated input, its output, the head slice cut
        // out of that, and the three buffers refine() runs on.
        for (size_t i = 0; i < n; ++i) {
            const RefinerSpec& r = p.refiners[i];
            const int64_t lp = levels[i].h * levels[i].w;
            transient = std::max(transient, lp * (r.in + r.out + p.head + 3 * r.hidden));
        }
    }

    // The cascade over `levels`, coarse first: refine, split the result into
    // head and context, add the head into `head`, and upsample both to the
    // next level. `head` must be sized for the finest level.
    void cascade(const nn::Tensor& head, const std::vector<Level>& levels,
                 bool bicubic_head) const {
        const PyramidHparams& p = hp();
        const size_t n = p.refiners.size();
        NN_CHECK(levels.size() == n, "loma: %zu levels for %zu refiners", levels.size(), n);

        // Both grow as the cascade descends and a resize is not in place, so
        // each needs a second buffer. The context's two differ in size: one is
        // written at its own level, the other read at the finer one below.
        int64_t ctx_w = 0, ctx_r = 0;
        planContext(p, levels, ctx_w, ctx_r);
        nn::Tensor head_a = head;
        nn::Tensor head_b =
            nn::arena_tensor(*arena, nn::DType::F32, head.numel() / p.head, p.head);
        nn::Tensor ctx_a = nn::arena_tensor(*arena, nn::DType::F32, std::max<int64_t>(ctx_r, 1));
        nn::Tensor ctx_b = nn::arena_tensor(*arena, nn::DType::F32, std::max<int64_t>(ctx_w, 1));

        int64_t cctx = 0;
        for (size_t i = 0; i < n; ++i) {
            const RefinerSpec& r = p.refiners[i];
            const Level& lv = levels[i];
            vk::ArenaScope scope(*arena);

            const nn::Tensor* in = &lv.map;
            nn::Tensor cat;
            if (i > 0) {
                cat = nn::arena_tensor(*arena, nn::DType::F32, lv.h, lv.w, lv.c + cctx);
                nn::strided_copy(cat, lv.map.view(lv.h * lv.w, lv.c), lv.h * lv.w, lv.c,
                                 lv.c, lv.c + cctx);
                nn::strided_copy(cat.offsetElems(lv.c),
                                 ctx_a.view(lv.h * lv.w, cctx), lv.h * lv.w, cctx, cctx,
                                 lv.c + cctx);
                in = &cat;
            }
            NN_CHECK(in->shape[2] == r.in, "loma: level %d wants %lld channels, got %lld",
                     r.scale, (long long)r.in, (long long)in->shape[2]);

            nn::Tensor y = nn::arena_tensor(*arena, nn::DType::F32, lv.h, lv.w, r.out);
            refine(y, *in, r, lv.h, lv.w);

            // y[:head] adds into the running head, y[head:] is the context.
            const int64_t rows = lv.h * lv.w;
            nn::Tensor cur_head = head_a.view(lv.h, lv.w, p.head);
            if (i == 0) {
                nn::strided_copy(cur_head, y.view(rows, r.out), rows, p.head, r.out,
                                 p.head);
            } else {
                nn::Tensor dh = nn::arena_tensor(*arena, nn::DType::F32, lv.h, lv.w, p.head);
                nn::strided_copy(dh, y.view(rows, r.out), rows, p.head, r.out, p.head);
                nn::add(cur_head.view(rows, p.head), cur_head.view(rows, p.head),
                        dh.view(rows, p.head));
            }
            if (dump_enabled()) {
                char nm[64];
                std::snprintf(nm, sizeof nm, "%s_head%d", prefix.c_str(), r.scale);
                dump_tensor(nm, cur_head, {lv.h, lv.w, p.head});
            }
            if (i + 1 == n) break;

            const Level& nx = levels[i + 1];
            nn::Tensor next_head = head_b.view(nx.h, nx.w, p.head);
            if (bicubic_head)
                nn::resize_bicubic(next_head, cur_head);
            else
                nn::resize_bilinear(next_head, cur_head);
            std::swap(head_a, head_b);

            nn::Tensor cur_ctx = ctx_b.view(lv.h, lv.w, r.ctx);
            nn::strided_copy(cur_ctx, y.offsetElems(p.head).view(rows, r.ctx), rows,
                             r.ctx, r.out, r.ctx);
            nn::resize_bilinear(ctx_a.view(nx.h, nx.w, r.ctx), cur_ctx);
            cctx = r.ctx;
        }
        // The caller passed `head`; if the cascade ended on the other buffer,
        // the result has to be moved back into it.
        if (head_a.ptr != head.ptr)
            nn::copy(head.view(levels[n - 1].h, levels[n - 1].w, p.head),
                     head_a.view(levels[n - 1].h, levels[n - 1].w, p.head));
    }
};

}  // namespace loma
