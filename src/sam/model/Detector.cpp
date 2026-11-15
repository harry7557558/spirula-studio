// The PCS (promptable concept segmentation) path: exemplar encoder, fusion
// encoder, DETR decoder and the MaskFormer-style segmentation head.
//
// Everything stays on device. The ggml original moves each stage's output to
// host vectors between sub-graphs -- for the fusion encoder alone that is
// 5184 x 256 floats out and back on every prompt.

#include "sam/Common.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "sam/model/Modules.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sam {
namespace model {

using nn::Act;
using nn::AttnBias;
using nn::Tensor;

namespace {

struct SineEmbedParams {
    uint64_t out, boxes, dim_t;
    uint32_t nq, groups_per_row;
};
struct RpbDeltaParams {
    uint64_t out, boxes, coords;
    uint32_t nq, S, axis, groups_per_row, _pad0;
};
struct RpbCombineParams {
    uint64_t out, rpb_x, rpb_y;
    uint32_t nq, S, n_heads, groups_per_row, _pad0;
};
struct PoolParams {
    uint64_t out, x, weights;
    uint32_t T, C, groups_per_row;
};
struct BoxRefineParams {
    uint64_t boxes, delta;
    uint32_t n, groups_per_row;
};

}  // namespace

// ---------------------------------------------------------------------------
// Geometry (exemplar) encoder
// ---------------------------------------------------------------------------

void encode_geometry(const SamModel& m, vk::Arena& arena,
                     const std::vector<Exemplar>& boxes, const Tensor& img_feats,
                     const Tensor& img_pe, const Tensor& out) {
    const Hparams& h = m.hp();
    const WeightStore& W = m.w();
    const int64_t D = h.neck_dim;
    const int64_t G = m.grid();
    const int64_t nb = (int64_t)boxes.size();
    const int64_t N = nb + 1;  // + CLS
    const int n_heads = 8;
    const int roi = 7;
    NN_CHECK(out.rows() == N && out.cols() == D,
               "encode_geometry: output must be [%lld, %lld]", (long long)N,
               (long long)D);

    vk::ArenaScope scope(arena);

    // ---- pre-transformer token construction ----
    // Four additive terms per exemplar: a direct coordinate projection, a
    // pooled visual feature, a sinusoidal box encoding and a positive/negative
    // label embedding. The first, third and fourth are tiny and assembled on
    // the host (four boxes at most in practice); the pooled term needs the
    // feature map and stays on device.
    Tensor tokens = nn::arena_tensor(arena, nn::DType::F32, N, D);
    {
        std::vector<float> host((size_t)N * D, 0.0f);
        std::vector<float> box_proj_w((size_t)4 * D), box_proj_b((size_t)D);
        std::vector<float> pos_proj_w((size_t)(2 * (D / 2) + 2) * D), pos_proj_b((size_t)D);
        std::vector<float> label_w((size_t)2 * D), cls((size_t)D);
        nn::tensor_to_host(W.get("geom.boxes_direct_project.weight"), box_proj_w.data(),
                           4 * D);
        nn::tensor_to_host(W.get("geom.boxes_direct_project.bias"), box_proj_b.data(), D);
        nn::tensor_to_host(W.get("geom.boxes_pos_enc_project.weight"), pos_proj_w.data(),
                           (int64_t)pos_proj_w.size());
        nn::tensor_to_host(W.get("geom.boxes_pos_enc_project.bias"), pos_proj_b.data(), D);
        nn::tensor_to_host(W.get("geom.label_embed.weight"), label_w.data(), 2 * D);
        nn::tensor_to_host(W.get("geom.cls_embed.weight"), cls.data(), D);

        const int num_pos_feats = (int)(D / 2);
        const int pos_len = 2 * num_pos_feats + 2;
        std::vector<float> pe((size_t)pos_len);
        for (int64_t b = 0; b < nb; ++b) {
            const Exemplar& e = boxes[(size_t)b];
            float* dst = host.data() + (size_t)b * D;
            const float coords[4] = {e.cx, e.cy, e.w, e.h};
            for (int64_t d = 0; d < D; ++d) {
                float s = box_proj_b[(size_t)d];
                for (int j = 0; j < 4; ++j)
                    s += coords[j] * box_proj_w[(size_t)(d * 4 + j)];
                dst[d] = s;
            }
            sine_encode_box(pe.data(), e.cx, e.cy, e.w, e.h, num_pos_feats);
            for (int64_t d = 0; d < D; ++d) {
                float s = pos_proj_b[(size_t)d];
                for (int j = 0; j < pos_len; ++j)
                    s += pe[(size_t)j] * pos_proj_w[(size_t)(d * pos_len + j)];
                dst[d] += s;
            }
            const int label = (e.label != 0) ? 1 : 0;
            for (int64_t d = 0; d < D; ++d) dst[d] += label_w[(size_t)(label * D + d)];
        }
        std::copy(cls.begin(), cls.end(), host.begin() + (size_t)nb * D);
        nn::tensor_from_host(tokens, host.data(), (int64_t)host.size());
    }

    // ---- pooled visual term ----
    if (nb > 0) {
        vk::ArenaScope pool(arena);
        // The reference LayerNorms the feature map before pooling.
        Tensor normed = nn::arena_tensor(arena, nn::DType::F32, G * G, D);
        nn::layer_norm(normed, img_feats.view(G * G, D), W.get("geom.img_pre_norm.weight"),
                       W.get("geom.img_pre_norm.bias"));

        std::vector<float> xyxy((size_t)nb * 4);
        for (int64_t b = 0; b < nb; ++b) {
            const Exemplar& e = boxes[(size_t)b];
            xyxy[(size_t)(b * 4 + 0)] = (e.cx - e.w * 0.5f) * (float)G;
            xyxy[(size_t)(b * 4 + 1)] = (e.cy - e.h * 0.5f) * (float)G;
            xyxy[(size_t)(b * 4 + 2)] = (e.cx + e.w * 0.5f) * (float)G;
            xyxy[(size_t)(b * 4 + 3)] = (e.cy + e.h * 0.5f) * (float)G;
        }
        Tensor tbox = nn::arena_tensor(arena, nn::DType::F32, nb, 4);
        nn::tensor_from_host(tbox, xyxy.data(), (int64_t)xyxy.size());

        Tensor pooled = nn::arena_tensor(arena, nn::DType::F32, nb, (int64_t)roi * roi * D);
        nn::roi_align(pooled.view(nb, (int64_t)roi * roi, D), normed.view(G, G, D), tbox,
                      (int)G, (int)G, (int)D, roi);

        // Conv2d(D, D, 7) on a 7x7 patch is one matmul once the kernel is
        // reordered to (ky, kx, ci) -- see repack_conv_to_hwc.
        nn::LinearOpts lo;
        lo.bias = W.get("geom.boxes_pool_project.bias");
        lo.residual = tokens.slice0(0, nb);
        nn::linear(tokens.slice0(0, nb), pooled, m.geomBoxPoolWeight(), lo);
    }

    // ---- final projection + norm, then the transformer ----
    {
        Tensor projected = nn::arena_tensor(arena, nn::DType::F32, N, D);
        nn::LinearOpts lo;
        lo.bias = W.get("geom.final_proj.bias");
        nn::linear(projected, tokens, W.get("geom.final_proj.weight"), lo);
        nn::layer_norm(out, projected, W.get("geom.norm.weight"), W.get("geom.norm.bias"));
    }

    for (int i = 0; i < h.geom_layers; ++i) {
        vk::ArenaScope blk(arena);
        char p[48];
        std::snprintf(p, sizeof p, "geom.layers.%d", i);
        auto wt = [&](const char* s) { return W.getf("%s.%s", p, s); };

        Tensor xn = nn::arena_tensor(arena, nn::DType::F32, N, D);
        // Self-attention: pos_enc_at_attn is False here, so Q/K/V all read the
        // normalized tokens with no positional term.
        nn::layer_norm(xn, out, wt("norm1.weight"), wt("norm1.bias"));
        mha_fused(arena, out, xn, xn, xn, N, N, wt("sa.in_proj_weight"),
                  wt("sa.in_proj_bias"), wt("sa.out_proj.weight"), wt("sa.out_proj.bias"),
                  n_heads, AttnBias::None, {}, out);

        // Cross-attention to the image: keys carry the positional encoding,
        // values do not.
        nn::layer_norm(xn, out, wt("norm2.weight"), wt("norm2.bias"));
        Tensor kin = nn::arena_tensor(arena, nn::DType::F32, G * G, D);
        nn::add(kin, img_feats.view(G * G, D), img_pe.view(G * G, D));
        mha_fused(arena, out, xn, kin, img_feats.view(G * G, D), N, G * G,
                  wt("ca.in_proj_weight"), wt("ca.in_proj_bias"), wt("ca.out_proj.weight"),
                  wt("ca.out_proj.bias"), n_heads, AttnBias::None, {}, out);

        nn::layer_norm(xn, out, wt("norm3.weight"), wt("norm3.bias"));
        Tensor hid = nn::arena_tensor(arena, nn::DType::F32, N,
                                      wt("linear1.weight").rows());
        nn::LinearOpts l1;
        l1.bias = wt("linear1.bias");
        l1.act = Act::Relu;
        nn::linear(hid, xn, wt("linear1.weight"), l1);
        nn::LinearOpts l2;
        l2.bias = wt("linear2.bias");
        l2.residual = out;
        nn::linear(out, hid, wt("linear2.weight"), l2);
    }

    nn::layer_norm(out, out, W.get("geom.encode_norm.weight"),
                   W.get("geom.encode_norm.bias"));
}

// ---------------------------------------------------------------------------
// Fusion encoder
// ---------------------------------------------------------------------------

void fusion_encoder_prefix(const SamModel& m, vk::Arena& arena,
                           const Tensor& feats, const Tensor& pos) {
    if (m.hp().fenc_layers == 0) return;
    vk::ArenaScope scope(arena);
    const int64_t N = feats.rows(), D = m.hp().neck_dim;
    const auto& W = m.w();
    Tensor xn = nn::arena_tensor(arena, nn::DType::F32, N, D);
    Tensor xnp = nn::arena_tensor(arena, nn::DType::F32, N, D);
    nn::layer_norm(xn, feats, W.get("fenc.layers.0.norm1.weight"),
                   W.get("fenc.layers.0.norm1.bias"));
    nn::add(xnp, xn, pos);
    mha_fused(arena, feats, xnp, xnp, xn, N, N,
              W.get("fenc.layers.0.sa.in_proj_weight"),
              W.get("fenc.layers.0.sa.in_proj_bias"),
              W.get("fenc.layers.0.sa.out_proj.weight"),
              W.get("fenc.layers.0.sa.out_proj.bias"), m.hp().fenc_heads,
              AttnBias::None, {}, feats);
}

void fusion_encoder(const SamModel& m, vk::Arena& arena, const Tensor& feats,
                    const Tensor& pos, const Tensor& prompt, int64_t n_prompt,
                    const Tensor& prompt_bias, bool prefix_done) {
    NN_SCOPED_TIMER("fusion_encoder");
    const Hparams& h = m.hp();
    const WeightStore& W = m.w();
    const int64_t D = h.neck_dim;
    const int64_t N = feats.rows();

    for (int i = 0; i < h.fenc_layers; ++i) {
        vk::ArenaScope blk(arena);
        char p[48];
        std::snprintf(p, sizeof p, "fenc.layers.%d", i);
        auto wt = [&](const char* s) { return W.getf("%s.%s", p, s); };

        Tensor xn = nn::arena_tensor(arena, nn::DType::F32, N, D);
        if (i != 0 || !prefix_done) {
            Tensor xnp = nn::arena_tensor(arena, nn::DType::F32, N, D);
            nn::layer_norm(xn, feats, wt("norm1.weight"), wt("norm1.bias"));
            nn::add(xnp, xn, pos);
            mha_fused(arena, feats, xnp, xnp, xn, N, N, wt("sa.in_proj_weight"),
                      wt("sa.in_proj_bias"), wt("sa.out_proj.weight"), wt("sa.out_proj.bias"),
                      h.fenc_heads, AttnBias::None, {}, feats);
        }

        // Cross-attention from image tokens to the prompt (text + exemplars).
        nn::layer_norm(xn, feats, wt("norm2.weight"), wt("norm2.bias"));
        mha_fused(arena, feats, xn, prompt, prompt, N, n_prompt, wt("ca.in_proj_weight"),
                  wt("ca.in_proj_bias"), wt("ca.out_proj.weight"), wt("ca.out_proj.bias"),
                  h.fenc_heads, AttnBias::PerKey, prompt_bias, feats);

        nn::layer_norm(xn, feats, wt("norm3.weight"), wt("norm3.bias"));
        Tensor hid = nn::arena_tensor(arena, nn::DType::F32, N,
                                      wt("linear1.weight").rows());
        nn::LinearOpts l1;
        l1.bias = wt("linear1.bias");
        l1.act = Act::Relu;
        nn::linear(hid, xn, wt("linear1.weight"), l1);
        nn::LinearOpts l2;
        l2.bias = wt("linear2.bias");
        l2.residual = feats;
        nn::linear(feats, hid, wt("linear2.weight"), l2);
    }
}

// ---------------------------------------------------------------------------
// DETR decoder
// ---------------------------------------------------------------------------

namespace {

// query_pos = ref_point_head(gen_sineembed_for_position(boxes)), with a zero
// row prepended for the presence token. Recomputed every layer, as in the
// reference -- the reference boxes move after each one.
void build_query_pos(const SamModel& m, vk::Arena& arena, const Tensor& boxes,
                     int64_t nq, const Tensor& out /* [nq+1, D] */) {
    const WeightStore& W = m.w();
    const int64_t D = m.hp().neck_dim;
    vk::ArenaScope scope(arena);

    Tensor sine = nn::arena_tensor(arena, nn::DType::F32, nq, 512);
    SineEmbedParams sp{};
    sp.out = sine.ptr;
    sp.boxes = boxes.ptr;
    sp.dim_t = m.sineDimT().ptr;
    sp.nq = (uint32_t)nq;
    vk::Stream::get().dispatchFlat("detr.sine_embed_boxes", {0u}, nq * 512, 256, &sp,
                                   sizeof(sp), &sp.groups_per_row);

    Tensor hid = nn::arena_tensor(arena, nn::DType::F32, nq, D);
    nn::LinearOpts l0;
    l0.bias = W.get("ddec.ref_point_head.layers.0.bias");
    l0.act = Act::Relu;
    nn::linear(hid, sine, W.get("ddec.ref_point_head.layers.0.weight"), l0);

    nn::fill(out.slice0(0, 1), 0.0f);  // the presence token has no reference box
    nn::LinearOpts l1;
    l1.bias = W.get("ddec.ref_point_head.layers.1.bias");
    nn::linear(out.slice0(1, nq), hid, W.get("ddec.ref_point_head.layers.1.weight"), l1);
}

// The box-relative positional bias for image cross-attention:
// [n_heads, nq + 1, S*S], row 0 (presence) zeroed.
void build_box_rpb(const SamModel& m, vk::Arena& arena, const Tensor& boxes, int64_t nq,
                   int64_t S, const Tensor& out) {
    const WeightStore& W = m.w();
    const int n_heads = m.hp().ddec_heads;
    const int64_t D = m.hp().neck_dim;
    vk::ArenaScope scope(arena);

    Tensor coords = nn::arena_tensor(arena, nn::DType::F32, S);
    nn::tensor_from_host(coords, m.rpbCoords().data(), S);

    // Both axis results must outlive the per-axis scratch, so they are
    // allocated before the loop -- an ArenaScope inside it would rewind over
    // the x result while computing y.
    Tensor rpb[2] = {nn::arena_tensor(arena, nn::DType::F32, nq * S, n_heads),
                     nn::arena_tensor(arena, nn::DType::F32, nq * S, n_heads)};
    for (int axis = 0; axis < 2; ++axis) {
        vk::ArenaScope inner(arena);
        Tensor deltas = nn::arena_tensor(arena, nn::DType::F32, nq * S, 2);
        RpbDeltaParams dp{};
        dp.out = deltas.ptr;
        dp.boxes = boxes.ptr;
        dp.coords = coords.ptr;
        dp.nq = (uint32_t)nq;
        dp.S = (uint32_t)S;
        dp.axis = (uint32_t)axis;
        vk::Stream::get().dispatchFlat("detr.rpb_deltas", {0u}, nq * S * 2, 256, &dp,
                                       sizeof(dp), &dp.groups_per_row);

        const char* ax = axis == 0 ? "x" : "y";
        Tensor hid = nn::arena_tensor(arena, nn::DType::F32, nq * S, D);
        nn::LinearOpts l0;
        l0.bias = W.getf("ddec.boxRPB_embed_%s.layers.0.bias", ax);
        l0.act = Act::Relu;
        nn::linear(hid, deltas, W.getf("ddec.boxRPB_embed_%s.layers.0.weight", ax), l0);

        nn::LinearOpts l1;
        l1.bias = W.getf("ddec.boxRPB_embed_%s.layers.1.bias", ax);
        nn::linear(rpb[axis], hid, W.getf("ddec.boxRPB_embed_%s.layers.1.weight", ax), l1);
    }

    RpbCombineParams cp{};
    cp.out = out.ptr;
    cp.rpb_x = rpb[0].ptr;
    cp.rpb_y = rpb[1].ptr;
    cp.nq = (uint32_t)nq;
    cp.S = (uint32_t)S;
    cp.n_heads = (uint32_t)n_heads;
    vk::Stream::get().dispatchFlat("detr.rpb_combine", {0u},
                                   (int64_t)n_heads * (nq + 1) * S * S, 256, &cp,
                                   sizeof(cp), &cp.groups_per_row);
}

// DotProductScoring: a residual MLP over the prompt, a masked mean, two
// projections and a scaled dot product.
void dot_product_scoring(const SamModel& m, vk::Arena& arena, const Tensor& queries,
                         int64_t nq, const Tensor& prompt, int64_t n_prompt,
                         const Tensor& prompt_valid, const Tensor& out_scores) {
    const WeightStore& W = m.w();
    const int64_t D = m.hp().neck_dim;
    vk::ArenaScope scope(arena);

    Tensor mlp = nn::arena_tensor(arena, nn::DType::F32, n_prompt, D);
    {
        Tensor hid = nn::arena_tensor(arena, nn::DType::F32, n_prompt,
                                      W.get("scoring.prompt_mlp.layers.0.weight").rows());
        nn::LinearOpts l0;
        l0.bias = W.get("scoring.prompt_mlp.layers.0.bias");
        l0.act = Act::Relu;
        nn::linear(hid, prompt, W.get("scoring.prompt_mlp.layers.0.weight"), l0);
        nn::LinearOpts l1;
        l1.bias = W.get("scoring.prompt_mlp.layers.1.bias");
        l1.residual = prompt;  // the MLP is residual around the prompt features
        nn::linear(mlp, hid, W.get("scoring.prompt_mlp.layers.1.weight"), l1);
        nn::layer_norm(mlp, mlp, W.get("scoring.prompt_mlp.out_norm.weight"),
                       W.get("scoring.prompt_mlp.out_norm.bias"));
    }

    Tensor pooled = nn::arena_tensor(arena, nn::DType::F32, 1, D);
    PoolParams pp{};
    pp.out = pooled.ptr;
    pp.x = mlp.ptr;
    pp.weights = prompt_valid.ptr;
    pp.T = (uint32_t)n_prompt;
    pp.C = (uint32_t)D;
    vk::Stream::get().dispatchFlat("detr.pool_weighted", {0u}, D, 256, &pp, sizeof(pp),
                                   &pp.groups_per_row);

    Tensor proj_pooled = nn::arena_tensor(arena, nn::DType::F32, 1, D);
    nn::LinearOpts lp;
    lp.bias = W.get("scoring.prompt_proj.bias");
    nn::linear(proj_pooled, pooled, W.get("scoring.prompt_proj.weight"), lp);

    Tensor proj_hs = nn::arena_tensor(arena, nn::DType::F32, nq, D);
    nn::LinearOpts lh;
    lh.bias = W.get("scoring.hs_proj.bias");
    nn::linear(proj_hs, queries, W.get("scoring.hs_proj.weight"), lh);

    // The clamp to [-12, 12] the reference applies is monotone and the scores
    // are read back for thresholding anyway, so it is done on the host.
    nn::matmul_nt(out_scores.view(nq, 1), proj_hs, proj_pooled,
                  1.0f / std::sqrt((float)D));
}

}  // namespace

void detr_decoder(const SamModel& m, vk::Arena& arena, const Tensor& enc_feats,
                  const Tensor& enc_pos, const Tensor& prompt, int64_t n_prompt,
                  const Tensor& prompt_bias, const Tensor& prompt_valid,
                  DetrOutputs& out) {
    NN_SCOPED_TIMER("detr_decoder");
    const Hparams& h = m.hp();
    const WeightStore& W = m.w();
    const int64_t D = h.neck_dim;
    const int64_t NQ = h.ddec_num_queries;
    const int64_t S = m.grid();
    const int64_t NKV = S * S;
    const int NH = h.ddec_heads;

    // Queries = [presence token, 200 content queries]; the presence token rides
    // along through every layer and only its own head reads it at the end.
    Tensor queries = nn::arena_tensor(arena, nn::DType::F32, NQ + 1, D);
    nn::strided_copy(queries.slice0(0, 1), W.get("ddec.presence_token.weight"), 1, D, D, D);
    nn::strided_copy(queries.slice0(1, NQ), W.get("ddec.query_embed.weight"), NQ, D, D, D);

    Tensor boxes = nn::arena_tensor(arena, nn::DType::F32, NQ, 4);
    nn::unary(boxes, W.get("ddec.reference_points.weight"), Act::Sigmoid);

    Tensor query_pos = nn::arena_tensor(arena, nn::DType::F32, NQ + 1, D);
    Tensor qin = nn::arena_tensor(arena, nn::DType::F32, NQ + 1, D);
    Tensor kin = nn::arena_tensor(arena, nn::DType::F32, NKV, D);
    nn::add(kin, enc_feats, enc_pos);

    for (int i = 0; i < h.ddec_layers; ++i) {
        vk::ArenaScope blk(arena);
        char p[48];
        std::snprintf(p, sizeof p, "ddec.layers.%d", i);
        auto wt = [&](const char* s) { return W.getf("%s.%s", p, s); };

        build_query_pos(m, arena, boxes, NQ, query_pos);
        Tensor rpb = nn::arena_tensor(arena, nn::DType::F32, (int64_t)NH * (NQ + 1) * NKV);
        build_box_rpb(m, arena, boxes, NQ, S, rpb);

        // The reference's decoder layer is POST-norm and runs
        // self-attention -> text cross-attention -> image cross-attention -> FFN.
        // The checkpoint's norm names do not follow that order: norm2 is the
        // post-self-attention norm, norm_ca_text follows the text attention,
        // norm1 follows the image attention and norm3 follows the FFN.
        nn::add(qin, queries, query_pos);
        mha_fused(arena, queries, qin, qin, queries, NQ + 1, NQ + 1,
                  wt("sa.in_proj_weight"), wt("sa.in_proj_bias"), wt("sa.out_proj.weight"),
                  wt("sa.out_proj.bias"), NH, AttnBias::None, {}, queries);
        nn::layer_norm(queries, queries, wt("norm2.weight"), wt("norm2.bias"));

        nn::add(qin, queries, query_pos);
        mha_fused(arena, queries, qin, prompt, prompt, NQ + 1, n_prompt,
                  wt("ca_text.in_proj_weight"), wt("ca_text.in_proj_bias"),
                  wt("ca_text.out_proj.weight"), wt("ca_text.out_proj.bias"), NH,
                  AttnBias::PerKey, prompt_bias, queries);
        nn::layer_norm(queries, queries, wt("norm_ca_text.weight"),
                       wt("norm_ca_text.bias"));

        nn::add(qin, queries, query_pos);
        mha_fused(arena, queries, qin, kin, enc_feats, NQ + 1, NKV, wt("ca.in_proj_weight"),
                  wt("ca.in_proj_bias"), wt("ca.out_proj.weight"), wt("ca.out_proj.bias"),
                  NH, AttnBias::Full, rpb, queries);
        nn::layer_norm(queries, queries, wt("norm1.weight"), wt("norm1.bias"));

        {
            Tensor hid = nn::arena_tensor(arena, nn::DType::F32, NQ + 1,
                                          wt("linear1.weight").rows());
            nn::LinearOpts l1;
            l1.bias = wt("linear1.bias");
            l1.act = Act::Relu;
            nn::linear(hid, queries, wt("linear1.weight"), l1);
            nn::LinearOpts l2;
            l2.bias = wt("linear2.bias");
            l2.residual = queries;
            nn::linear(queries, hid, wt("linear2.weight"), l2);
            nn::layer_norm(queries, queries, wt("norm3.weight"), wt("norm3.bias"));
        }

        // Iterative box refinement, on the object queries only. The decoder's
        // final norm is applied before the box head every layer
        // (use_normed_output_consistently in the reference).
        {
            Tensor normed = nn::arena_tensor(arena, nn::DType::F32, NQ, D);
            nn::layer_norm(normed, queries.slice0(1, NQ), W.get("ddec.norm.weight"),
                           W.get("ddec.norm.bias"));
            Tensor delta = nn::arena_tensor(arena, nn::DType::F32, NQ, 4);
            mlp_relu(arena, delta, normed, m, "ddec.bbox_embed.layers", 3);

            BoxRefineParams bp{};
            bp.boxes = boxes.ptr;
            bp.delta = delta.ptr;
            bp.n = (uint32_t)(NQ * 4);
            vk::Stream::get().dispatchFlat("detr.box_refine", {0u}, NQ * 4, 256, &bp,
                                           sizeof(bp), &bp.groups_per_row);
        }
    }

    out.boxes = boxes;
    out.queries = nn::arena_tensor(arena, nn::DType::F32, NQ, D);
    nn::layer_norm(out.queries, queries.slice0(1, NQ), W.get("ddec.norm.weight"),
                   W.get("ddec.norm.bias"));

    out.class_scores = nn::arena_tensor(arena, nn::DType::F32, NQ);
    dot_product_scoring(m, arena, out.queries, NQ, prompt, n_prompt, prompt_valid,
                        out.class_scores);

    // Presence head: LayerNorm then a 3-layer MLP down to a single logit.
    out.presence = nn::arena_tensor(arena, nn::DType::F32, 1);
    {
        vk::ArenaScope scope(arena);
        Tensor pres = nn::arena_tensor(arena, nn::DType::F32, 1, D);
        nn::layer_norm(pres, queries.slice0(0, 1),
                       W.get("ddec.presence_token_out_norm.weight"),
                       W.get("ddec.presence_token_out_norm.bias"));
        mlp_relu(arena, out.presence.view(1, 1), pres, m,
                 "ddec.presence_token_head.layers", 3);
    }
}

// ---------------------------------------------------------------------------
// Segmentation head
// ---------------------------------------------------------------------------

void segmentation_head(const SamModel& m, vk::Arena& arena, const Tensor& enc_feats,
                       const ImageFeatures& feats, const Tensor& queries,
                       int64_t n_queries, const Tensor& prompt, int64_t n_prompt,
                       const Tensor& prompt_bias, const Tensor& out_masks) {
    NN_SCOPED_TIMER("segmentation_head");
    const WeightStore& W = m.w();
    const int64_t D = m.hp().neck_dim;
    const int64_t G = m.grid();
    const int64_t S = G * 4;
    if (n_queries == 0) return;

    vk::ArenaScope scope(arena);

    // Cross-attend the fusion output to the prompt once more, then use it in
    // place of the lowest-resolution FPN level (_embed_pixels in the reference).
    Tensor enc = nn::arena_tensor(arena, nn::DType::F32, G * G, D);
    nn::strided_copy(enc, enc_feats, G * G, D, D, D);
    {
        Tensor xn = nn::arena_tensor(arena, nn::DType::F32, G * G, D);
        nn::layer_norm(xn, enc, W.get("seg.cross_attn_norm.weight"),
                       W.get("seg.cross_attn_norm.bias"));
        mha_fused(arena, enc, xn, prompt, prompt, G * G, n_prompt,
                  W.get("seg.cross_attend_prompt.in_proj_weight"),
                  W.get("seg.cross_attend_prompt.in_proj_bias"),
                  W.get("seg.cross_attend_prompt.out_proj.weight"),
                  W.get("seg.cross_attend_prompt.out_proj.bias"), 8, AttnBias::PerKey,
                  prompt_bias, enc);
    }

    // Pixel decoder: two rounds of (nearest upsample, add the finer FPN level,
    // 3x3 conv, GroupNorm(8), ReLU). The convolution runs on the MERGED map,
    // not on the FPN level alone.
    Tensor prev = nn::arena_tensor(arena, nn::DType::F32, G * 2, G * 2, D);
    {
        vk::ArenaScope inner(arena);
        Tensor up = nn::arena_tensor(arena, nn::DType::F32, G * 2, G * 2, D);
        nn::upsample_nearest2x(up, enc.view(G, G, D));
        nn::add(up, up, feats.det[1].view(G * 2 * G * 2, D));
        Tensor conv = nn::arena_tensor(arena, nn::DType::F32, G * 2, G * 2, D);
        nn::ConvOpts co;
        co.pad_y = co.pad_x = 1;
        co.bias = W.get("seg.pixel_decoder.conv_layers.0.bias");
        nn::conv2d(arena, conv, up, W.get("seg.pixel_decoder.conv_layers.0.weight"), 3, 3,
                   co);
        nn::group_norm(arena, prev.view(G * 2 * G * 2, D), conv.view(G * 2 * G * 2, D),
                       W.get("seg.pixel_decoder.norms.0.weight"),
                       W.get("seg.pixel_decoder.norms.0.bias"), 8, 1e-5f, Act::Relu);
    }
    Tensor pixels = nn::arena_tensor(arena, nn::DType::F32, S, S, D);
    {
        vk::ArenaScope inner(arena);
        Tensor up = nn::arena_tensor(arena, nn::DType::F32, S, S, D);
        nn::upsample_nearest2x(up, prev);
        nn::add(up, up, feats.det[0].view(S * S, D));
        Tensor conv = nn::arena_tensor(arena, nn::DType::F32, S, S, D);
        nn::ConvOpts co;
        co.pad_y = co.pad_x = 1;
        co.bias = W.get("seg.pixel_decoder.conv_layers.1.bias");
        nn::conv2d(arena, conv, up, W.get("seg.pixel_decoder.conv_layers.1.weight"), 3, 3,
                   co);
        nn::group_norm(arena, pixels.view(S * S, D), conv.view(S * S, D),
                       W.get("seg.pixel_decoder.norms.1.weight"),
                       W.get("seg.pixel_decoder.norms.1.bias"), 8, 1e-5f, Act::Relu);
    }
    // (The reference allocates a third conv layer here and never uses it.)

    Tensor pixel_embed = nn::arena_tensor(arena, nn::DType::F32, S * S, D);
    {
        nn::LinearOpts lo;
        lo.bias = W.get("seg.instance_seg_head.bias");
        nn::linear(pixel_embed, pixels.view(S * S, D),
                   W.get("seg.instance_seg_head.weight"), lo);
    }

    Tensor mask_embed = nn::arena_tensor(arena, nn::DType::F32, n_queries, D);
    mlp_relu(arena, mask_embed, queries, m, "seg.mask_predictor.mask_embed.layers", 3);

    // masks[q][p] = <mask_embed[q], pixel_embed[p]> -- einsum('qc,pc->qp').
    nn::matmul_nt(out_masks.view(n_queries, S * S), mask_embed, pixel_embed);
}

}  // namespace model
}  // namespace sam
