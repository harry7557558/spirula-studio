// LoMa's matcher: LightGlue's block structure with a different assignment.
//
// Four things separate it from src/aliked/model/LightGlue.cpp, and each is a
// place where copying that file would be wrong: keypoints are normalized
// PER AXIS (2*x/W - 1), not by the longer side, so the aspect ratio is not
// preserved; the input projection exists only when the descriptor is narrower
// than the embedding; the assignment is a plain dual softmax with no
// matchability term; and the threshold applies to the probability, not its
// log. Read against the LoMa repository's loma.py.

#include "loma/Loma.h"

#include "loma/Common.h"
#include "loma/model/Fetch.h"
#include "loma/model/Weights.h"
#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

NN_DECLARE_EMBEDDED_MODULES(loma)

namespace loma {
namespace {

using nn::Act;
using nn::AttnOpts;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

struct AssignParams {
    uint64_t out_idx, out_score, sim, lse1, argmax1;
    uint32_t n0, n1;
    float    min_score;
    uint32_t groups_per_row;
};

}  // namespace

struct Matcher::Impl {
    MatcherWeights w;
    vk::Arena      arena{"loma-matcher"};

    int64_t dim() const { return w.hparams().embed_dim; }
    int64_t headDim() const { return dim() / w.hparams().n_heads; }

    // x <- x + ffn(cat(x, message)); the FFN is Linear, LayerNorm, GELU, Linear.
    void ffn(const Tensor& x, const Tensor& message, int64_t n, const char* which,
             int layer) {
        char p[128];
        std::snprintf(p, sizeof p, "model.transformers.%d.%s", layer, which);
        const std::string b = p;
        const int64_t d = dim();
        vk::ArenaScope scope(arena);

        Tensor cat = nn::arena_tensor(arena, DType::F32, n, 2 * d);
        nn::strided_copy(cat, x, n, d, d, 2 * d);
        nn::strided_copy(cat.offsetElems(d), message, n, d, d, 2 * d);

        Tensor h = nn::arena_tensor(arena, DType::F32, n, 2 * d);
        LinearOpts l0;
        l0.bias = w.get(b + ".ffn.0.bias");
        nn::linear(h, cat, w.get(b + ".ffn.0.weight"), l0);
        nn::layer_norm(h, h, w.get(b + ".ffn.1.weight"), w.get(b + ".ffn.1.bias"), 1e-5f);
        nn::unary(h, h, Act::GeluErf);

        LinearOpts l1;
        l1.bias = w.get(b + ".ffn.3.bias");
        l1.residual = x;
        nn::linear(x, h, w.get(b + ".ffn.3.weight"), l1);
    }
};

Matcher::Matcher() : impl_(new Impl) {}
Matcher::~Matcher() { delete impl_; }
bool Matcher::loaded() const { return impl_->w.loaded(); }
int  Matcher::descriptorDim() const { return (int)impl_->w.hparams().input_dim; }

void Matcher::load(const std::string& model) {
    NN_ENSURE_EMBEDDED_MODULES(loma);
    impl_->w.load(resolve_model(model, "matcher"));
}

std::vector<Match> Matcher::match(const MatchInput& A, const MatchInput& B,
                                  const MatchOptions& opts) {
    NN_CHECK(loaded(), "Matcher::match before load()");
    Impl& im = *impl_;
    const MatcherHparams& hp = im.w.hparams();
    vk::Arena& arena = im.arena;

    std::vector<Match> out;
    const int64_t n0 = A.n, n1 = B.n;
    if (n0 == 0 || n1 == 0) return out;
    const int64_t d = hp.embed_dim, hd = im.headDim();

    // Peak live set: two [n, d] states, one [n, 3*d] projection, the similarity
    // matrix, and the FFN's [n, 2*d] scratch.
    const int64_t nmax = std::max(n0, n1);
    arena.reserve((uint64_t)((n0 * n1 + nmax * (10 * d + 8)) * 4 + (32 << 20)));
    vk::ArenaScope root(arena);

    Tensor x[2], freqs[2];
    const MatchInput* in[2] = {&A, &B};
    const int64_t ns[2] = {n0, n1};

    for (int s = 0; s < 2; ++s) {
        const MatchInput& I = *in[s];
        const int64_t n = ns[s];
        NN_CHECK(I.width > 0 && I.height > 0, "LoMa matcher: image %d has no size", s);

        // Corner-origin pixels -> LoMa's normalized [-1, 1], per axis. This is
        // NOT LightGlue's aspect-preserving normalization, and swapping them
        // rotates every position encoding on a non-square image.
        std::vector<float> kn((size_t)n * 2);
        for (int64_t i = 0; i < n; ++i) {
            kn[(size_t)i * 2] = 2.0f * I.keypoints[i * 2] / (float)I.width - 1.0f;
            kn[(size_t)i * 2 + 1] = 2.0f * I.keypoints[i * 2 + 1] / (float)I.height - 1.0f;
        }
        Tensor tk = nn::arena_tensor(arena, DType::F32, n, 2);
        nn::tensor_from_host(tk, kn.data(), (int64_t)kn.size());

        // Rotary table: proj = kn @ Wr^T, then (cos, sin) per pair, shared by
        // every head -- which is exactly nn::rope's [n, head_dim/2, 2] layout.
        Tensor proj = nn::arena_tensor(arena, DType::F32, n, hd / 2);
        nn::linear(proj, tk, im.w.get("posenc.Wr.weight"));
        freqs[s] = nn::arena_tensor(arena, DType::F32, n * (hd / 2), 2);
        {
            std::vector<float> host((size_t)n * (hd / 2));
            nn::tensor_to_host(proj, host.data(), (int64_t)host.size());
            std::vector<float> cs(host.size() * 2);
            for (size_t i = 0; i < host.size(); ++i) {
                cs[i * 2] = std::cos(host[i]);
                cs[i * 2 + 1] = std::sin(host[i]);
            }
            nn::tensor_from_host(freqs[s], cs.data(), (int64_t)cs.size());
        }

        x[s] = nn::arena_tensor(arena, DType::F32, n, d);
        if (hp.input_proj) {
            Tensor desc = nn::arena_tensor(arena, DType::F32, n, hp.input_dim);
            nn::tensor_from_host(desc, I.descriptors, n * hp.input_dim);
            LinearOpts lp;
            lp.bias = im.w.get("model.input_proj.bias");
            nn::linear(x[s], desc, im.w.get("model.input_proj.weight"), lp);
        } else {
            nn::tensor_from_host(x[s], I.descriptors, n * d);
        }
    }

    for (int l = 0; l < hp.n_layers; ++l) {
        for (int s = 0; s < 2; ++s) {
            const int64_t n = ns[s];
            vk::ArenaScope scope(arena);
            char p[96];
            std::snprintf(p, sizeof p, "model.transformers.%d.self_attn", l);
            const std::string sa = p;

            Tensor qkv = nn::arena_tensor(arena, DType::F32, n, 3 * d);
            LinearOpts lq;
            lq.bias = im.w.get(sa + ".Wqkv.bias");
            nn::linear(qkv, x[s], im.w.get(sa + ".Wqkv.weight"), lq);

            // q and k rotate in place inside the fused buffer; v does not.
            nn::rope(qkv, freqs[s], hp.n_heads, (int)hd, n, 1, 3 * d);
            nn::rope(qkv.offsetElems(d), freqs[s], hp.n_heads, (int)hd, n, 1, 3 * d);

            Tensor ctx = nn::arena_tensor(arena, DType::F32, n, d);
            AttnOpts ao;
            ao.n_heads = hp.n_heads;
            ao.head_dim = (int)hd;
            ao.q_stride = ao.k_stride = ao.v_stride = 3 * d;
            ao.arena = &arena;
            nn::attention(ctx, qkv, qkv.offsetElems(d), qkv.offsetElems(2 * d), n, n, ao);

            Tensor msg = nn::arena_tensor(arena, DType::F32, n, d);
            LinearOpts lo;
            lo.bias = im.w.get(sa + ".out_proj.bias");
            nn::linear(msg, ctx, im.w.get(sa + ".out_proj.weight"), lo);
            im.ffn(x[s], msg, n, "self_attn", l);
        }

        {
            vk::ArenaScope scope(arena);
            char p[96];
            std::snprintf(p, sizeof p, "model.transformers.%d.cross_attn", l);
            const std::string ca = p;

            Tensor qk[2], v[2], msg[2];
            for (int s = 0; s < 2; ++s) {
                qk[s] = nn::arena_tensor(arena, DType::F32, ns[s], d);
                v[s] = nn::arena_tensor(arena, DType::F32, ns[s], d);
                LinearOpts lqk, lv;
                lqk.bias = im.w.get(ca + ".to_qk.bias");
                lv.bias = im.w.get(ca + ".to_v.bias");
                nn::linear(qk[s], x[s], im.w.get(ca + ".to_qk.weight"), lqk);
                nn::linear(v[s], x[s], im.w.get(ca + ".to_v.weight"), lv);
            }
            for (int s = 0; s < 2; ++s) {
                const int o = 1 - s;
                Tensor ctx = nn::arena_tensor(arena, DType::F32, ns[s], d);
                AttnOpts ao;
                ao.n_heads = hp.n_heads;
                ao.head_dim = (int)hd;
                ao.arena = &arena;
                // No rotary here: the cross block attends between images, where
                // a within-image position has no meaning.
                nn::attention(ctx, qk[s], qk[o], v[o], ns[s], ns[o], ao);
                msg[s] = nn::arena_tensor(arena, DType::F32, ns[s], d);
                LinearOpts lo;
                lo.bias = im.w.get(ca + ".to_out.bias");
                nn::linear(msg[s], ctx, im.w.get(ca + ".to_out.weight"), lo);
            }
            for (int s = 0; s < 2; ++s) im.ffn(x[s], msg[s], ns[s], "cross_attn", l);
        }
    }

    // The assignment: sim = final_proj(x0) . final_proj(x1) / sqrt(d), then
    // softmax along the row times softmax along the column. No matchability
    // term -- LoMa supervises one in training and drops it at inference.
    std::vector<int32_t> idx((size_t)n0);
    std::vector<float>   sc((size_t)n0);
    {
        vk::ArenaScope scope(arena);
        char p[96];
        std::snprintf(p, sizeof p, "model.log_assignment.%d.final_proj", hp.assign_layer);
        const std::string fp = p;

        Tensor m[2];
        for (int s = 0; s < 2; ++s) {
            m[s] = nn::arena_tensor(arena, DType::F32, ns[s], d);
            LinearOpts lf;
            lf.bias = im.w.get(fp + ".bias");
            nn::linear(m[s], x[s], im.w.get(fp + ".weight"), lf);
        }
        Tensor sim = nn::arena_tensor(arena, DType::F32, n0, n1);
        nn::matmul_nt(sim, m[0], m[1], 1.0f / std::sqrt((float)d));

        Tensor lse1 = nn::arena_tensor(arena, DType::F32, n1);
        Tensor am1 = nn::arena_tensor(arena, DType::I32, n1);
        Tensor oi = nn::arena_tensor(arena, DType::I32, n0);
        Tensor os = nn::arena_tensor(arena, DType::F32, n0);

        AssignParams ap{};
        ap.out_idx = oi.ptr;
        ap.out_score = os.ptr;
        ap.sim = sim.ptr;
        ap.lse1 = lse1.ptr;
        ap.argmax1 = am1.ptr;
        ap.n0 = (uint32_t)n0;
        ap.n1 = (uint32_t)n1;
        ap.min_score = opts.min_score;
        // Two passes over the similarity, both coalesced: the column pass is
        // one thread per column (fold over elements), the row pass one
        // workgroup per row (fold over lines).
        const vk::SpecList spec{0u, 0u};
        vk::Stream& st = vk::Stream::get();
        st.dispatchFlat("loma.lse_cols", spec, n1, 256, &ap, sizeof(ap),
                        &ap.groups_per_row);
        const vk::Stream::Fold f = vk::Stream::fold1D(n0, 1);
        ap.groups_per_row = f.per_row;
        st.dispatch("loma.assign_rows", spec, f.per_row, f.rows, 1, &ap, sizeof(ap));
        st.download(idx.data(), oi.ptr, (uint64_t)n0 * 4);
        st.download(sc.data(), os.ptr, (uint64_t)n0 * 4);
    }

    out.reserve((size_t)n0 / 4);
    for (int64_t i = 0; i < n0; ++i)
        if (idx[(size_t)i] >= 0)
            out.push_back({(uint32_t)i, (uint32_t)idx[(size_t)i], sc[(size_t)i]});
    return out;
}

}  // namespace loma
