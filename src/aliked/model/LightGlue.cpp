// LightGlue's forward pass, next to its PyTorch reference (cvg/LightGlue,
// lightglue.py). Nine layers of self- then cross-attention, one assignment.
//
// Three things about the checkpoint decide how this file is written:
//
//   * torch exports nn.Linear as MatMul + Add, and only the Add's bias keeps
//     its qualified name -- the weight arrives as "onnx::MatMul_2537", and its
//     matrix is [in, out] rather than the [out, in] every op here wants. Both
//     are fixed once, at load: OnnxFile::linearWeights() recovers the pairing
//     by walking Add -> MatMul, and the matrices are transposed on the host.
//   * the fused qkv projection is laid out [head][dim][3] -- q, k and v
//     INTERLEAVED per element, which no stride can express. Permuting the
//     projection's output rows at load makes it [3][head][dim] instead, which
//     is exactly the fused layout nn::attention's q/k/v strides address.
//   * the export runs the assignment head on the last layer only. LightGlue's
//     early exit and token pruning are not in the graph, so they are not here
//     either; both are speedups, not behaviour.

#include "aliked/model/LightGlue.h"

#include "aliked/Common.h"
#include "aliked/model/Fetch.h"
#include "nn/Blocks.h"
#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

NN_DECLARE_EMBEDDED_MODULES(aliked)

namespace aliked {
namespace {

using nn::Act;
using nn::AttnOpts;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

constexpr int64_t kAlign = 256;
int64_t align_up(int64_t v, int64_t a) { return (v + a - 1) / a * a; }

struct AssignParams {
    uint64_t out_val, out_idx, sim, bias;
    uint32_t R, C, stride, step;
    float    scale;
    uint32_t groups_per_row;
};

}  // namespace

struct Matcher::Impl {
    // ---- hyperparameters, read off the checkpoint ----
    int n_layers = 0;
    int dim = 0;         // 256
    int input_dim = 0;   // 128, ALIKED's descriptor width
    int n_heads = 4;     // not stated in the file; LightGlue's every release
    int head_dim = 0;

    std::unordered_map<std::string, Tensor> w;
    nn::DevicePtr blob = 0;
    vk::Arena     arena{"lightglue"};
    std::string   path;
    bool          loaded = false;

    ~Impl() {
        if (blob) vk::device_free(blob);
    }

    Tensor get(const std::string& name) const {
        auto it = w.find(name);
        NN_CHECK(it != w.end(), "LightGlue checkpoint has no '%s'", name.c_str());
        return it->second;
    }
    Tensor getf(const char* fmt, int i, const char* suffix) const {
        char buf[128];
        std::snprintf(buf, sizeof buf, fmt, i, suffix);
        return get(buf);
    }

    void load(const std::string& onnx_path);
    std::vector<Match> match(const MatchInput& a, const MatchInput& b,
                             const MatchOptions& opts);

    // x <- x + ffn(cat(x, message)); the FFN is Linear, LayerNorm, GELU, Linear.
    void ffn(const Tensor& x, const Tensor& message, int64_t n, const char* prefix,
             int layer);
};

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

void Matcher::Impl::load(const std::string& onnx_path) {
    NN_CHECK(!loaded, "LightGlue::load called twice");
    const OnnxFile file = read_onnx(onnx_path);
    const auto linears = file.linearWeights();
    const char* path = onnx_path.c_str();

    // How many layers the file has, from the highest transformers.N present.
    n_layers = 0;
    for (const auto& kv : linears) {
        int idx = -1;
        if (std::sscanf(kv.first.c_str(), "transformers.%d.", &idx) == 1 && idx >= 0)
            n_layers = std::max(n_layers, idx + 1);
    }
    NN_CHECK(n_layers > 0, "'%s' has no transformer layers; is this a LightGlue "
                           "checkpoint?", path);

    // Staged host copies, transposed / permuted as described at the top.
    struct Staged {
        std::string          name;
        std::vector<int64_t> shape;
        std::vector<float>   data;
    };
    std::vector<Staged> staged;

    auto init_of = [&](const std::string& n) -> const OnnxTensor& {
        const OnnxTensor* t = file.find(n);
        NN_CHECK(t != nullptr, "'%s' has no initializer '%s'", path, n.c_str());
        return *t;
    };

    // A Linear: weight [in, out] in the file -> [out, in] here, plus its bias.
    auto take_linear = [&](const std::string& module) {
        auto it = linears.find(module);
        NN_CHECK(it != linears.end(), "'%s' has no linear named '%s'", path,
                 module.c_str());
        const OnnxTensor& wt = init_of(it->second);
        NN_CHECK(wt.shape.size() == 2, "'%s': %s is not a matrix", path, module.c_str());
        const int64_t in = wt.shape[0], out = wt.shape[1];
        Staged s{module + ".weight", {out, in}, std::vector<float>(wt.data.size())};
        for (int64_t i = 0; i < in; ++i)
            for (int64_t o = 0; o < out; ++o)
                s.data[(size_t)(o * in + i)] = wt.data[(size_t)(i * out + o)];
        staged.push_back(std::move(s));
        const OnnxTensor& b = init_of(module + ".bias");
        staged.push_back(Staged{module + ".bias", b.shape, b.data});
    };
    auto take_raw = [&](const std::string& name) {
        const OnnxTensor& t = init_of(name);
        staged.push_back(Staged{name, t.shape, t.data});
    };

    take_linear("input_proj");
    input_dim = (int)staged[staged.size() - 2].shape[1];
    dim = (int)staged[staged.size() - 2].shape[0];
    head_dim = dim / n_heads;
    NN_CHECK(head_dim * n_heads == dim && head_dim % 2 == 0,
             "'%s': %d channels do not split into %d rotary heads", path, dim, n_heads);

    for (int l = 0; l < n_layers; ++l) {
        char p[96];
        std::snprintf(p, sizeof p, "transformers.%d.self_attn", l);
        const std::string sa = p;
        // Wqkv, with its output rows permuted from [head][dim][3] to
        // [3][head][dim] so q, k and v become contiguous blocks.
        {
            auto it = linears.find(sa + ".Wqkv");
            NN_CHECK(it != linears.end(), "'%s' has no %s.Wqkv", path, sa.c_str());
            const OnnxTensor& wt = init_of(it->second);
            const OnnxTensor& bt = init_of(sa + ".Wqkv.bias");
            const int64_t in = wt.shape[0], out = wt.shape[1];
            NN_CHECK(out == 3 * dim, "'%s': %s.Wqkv is %lld wide, expected %lld", path,
                     sa.c_str(), (long long)out, (long long)(3 * dim));
            Staged sw{sa + ".Wqkv.weight", {out, in}, std::vector<float>(wt.data.size())};
            Staged sb{sa + ".Wqkv.bias", {out}, std::vector<float>((size_t)out)};
            for (int h = 0; h < n_heads; ++h)
                for (int d = 0; d < head_dim; ++d)
                    for (int j = 0; j < 3; ++j) {
                        const int64_t src = (int64_t)(h * head_dim + d) * 3 + j;
                        const int64_t dst = (int64_t)j * dim + h * head_dim + d;
                        for (int64_t i = 0; i < in; ++i)
                            sw.data[(size_t)(dst * in + i)] =
                                wt.data[(size_t)(i * out + src)];
                        sb.data[(size_t)dst] = bt.data[(size_t)src];
                    }
            staged.push_back(std::move(sw));
            staged.push_back(std::move(sb));
        }
        take_linear(sa + ".out_proj");
        take_linear(sa + ".ffn.0");
        take_raw(sa + ".ffn.1.weight");
        take_raw(sa + ".ffn.1.bias");
        take_linear(sa + ".ffn.3");

        std::snprintf(p, sizeof p, "transformers.%d.cross_attn", l);
        const std::string ca = p;
        take_linear(ca + ".to_qk");
        take_linear(ca + ".to_v");
        take_linear(ca + ".to_out");
        take_linear(ca + ".ffn.0");
        take_raw(ca + ".ffn.1.weight");
        take_raw(ca + ".ffn.1.bias");
        take_linear(ca + ".ffn.3");
    }

    {
        char p[64];
        std::snprintf(p, sizeof p, "log_assignment.%d", n_layers - 1);
        take_linear(std::string(p) + ".final_proj");
        take_linear(std::string(p) + ".matchability");
    }

    // The Fourier positional encoding's Wr is a bias-free Linear(2, dim/2), so
    // the Add-walk above cannot find it. It is the only [2, N] initializer in
    // the file, which is enough to name it.
    {
        const OnnxTensor* wr = nullptr;
        for (const OnnxTensor& t : file.initializers)
            if (t.shape.size() == 2 && t.shape[0] == 2) {
                NN_CHECK(wr == nullptr,
                         "'%s' has more than one [2, N] initializer; the positional "
                         "encoding can no longer be identified by shape",
                         path);
                wr = &t;
            }
        NN_CHECK(wr != nullptr, "'%s' has no positional-encoding matrix", path);
        NN_CHECK(wr->shape[1] * 2 == head_dim,
                 "'%s': positional encoding is [2, %lld], expected [2, %d]", path,
                 (long long)wr->shape[1], head_dim / 2);
        Staged s{"posenc.weight", {wr->shape[1], 2}, std::vector<float>(wr->data.size())};
        for (int64_t i = 0; i < 2; ++i)
            for (int64_t o = 0; o < wr->shape[1]; ++o)
                s.data[(size_t)(o * 2 + i)] = wr->data[(size_t)(i * wr->shape[1] + o)];
        staged.push_back(std::move(s));
    }

    int64_t total = 0;
    for (const Staged& s : staged) total = align_up(total, kAlign) + (int64_t)s.data.size() * 4;
    blob = vk::device_alloc((uint64_t)total, "lightglue-weights");

    int64_t off = 0;
    for (const Staged& s : staged) {
        off = align_up(off, kAlign);
        const nn::DevicePtr ptr = blob + (uint64_t)off;
        vk::Stream::get().upload(ptr, s.data.data(), s.data.size() * 4);
        off += (int64_t)s.data.size() * 4;
        Tensor t;
        t.ptr = ptr;
        t.dtype = DType::F32;
        t.ndim = (int32_t)s.shape.size();
        for (size_t i = 0; i < s.shape.size() && i < 4; ++i) t.shape[i] = s.shape[i];
        w[s.name] = t;
    }
    vk::Stream::get().sync();

    this->path = onnx_path;
    loaded = true;
    NN_LOG_INFO("[lightglue] %s: %d layers, dim %d (%d heads), input %d, %zu tensors, "
                "%.1f MB on device\n",
                onnx_path.c_str(), n_layers, dim, n_heads, input_dim, w.size(),
                (double)total / 1e6);
}

// ---------------------------------------------------------------------------
// Forward
// ---------------------------------------------------------------------------

void Matcher::Impl::ffn(const Tensor& x, const Tensor& message, int64_t n,
                        const char* prefix, int layer) {
    char p[128];
    std::snprintf(p, sizeof p, "transformers.%d.%s", layer, prefix);
    const std::string b = p;
    nn::ConcatFfn f;
    f.w0 = get(b + ".ffn.0.weight");
    f.b0 = get(b + ".ffn.0.bias");
    f.ln_w = get(b + ".ffn.1.weight");
    f.ln_b = get(b + ".ffn.1.bias");
    f.w3 = get(b + ".ffn.3.weight");
    f.b3 = get(b + ".ffn.3.bias");
    nn::concat_ffn(arena, x, message, n, dim, f);
}

std::vector<Match> Matcher::Impl::match(const MatchInput& A, const MatchInput& B,
                                        const MatchOptions& opts) {
    std::vector<Match> out;
    const int64_t n0 = A.n, n1 = B.n;
    if (n0 == 0 || n1 == 0) return out;

    // Peak live set: two [n, dim] states, one [n, 3*dim] projection, the
    // similarity matrix, and the FFN's [n, 2*dim] scratch.
    const int64_t nmax = std::max(n0, n1);
    arena.reserve((uint64_t)((n0 * n1 + nmax * (10 * dim + 8)) * 4 + (32 << 20)));
    vk::ArenaScope root(arena);

    Tensor x[2];
    Tensor freqs[2];
    const MatchInput* in[2] = {&A, &B};
    const int64_t nn_[2] = {n0, n1};

    for (int s = 0; s < 2; ++s) {
        const MatchInput& I = *in[s];
        const int64_t n = nn_[s];

        // Keypoints: our corner origin -> LightGlue's centre origin, then its
        // own normalization -- shift by the image centre, scale by half the
        // LONGER side (not per-axis, so the aspect ratio survives).
        std::vector<float> kn((size_t)n * 2);
        const float sx = 0.5f * (float)I.width, sy = 0.5f * (float)I.height;
        const float scale = 0.5f * (float)std::max(I.width, I.height);
        for (int64_t i = 0; i < n; ++i) {
            kn[(size_t)i * 2] = (I.keypoints[i * 2] - 0.5f - sx) / scale;
            kn[(size_t)i * 2 + 1] = (I.keypoints[i * 2 + 1] - 0.5f - sy) / scale;
        }
        Tensor tk = nn::arena_tensor(arena, DType::F32, n, 2);
        nn::tensor_from_host(tk, kn.data(), (int64_t)kn.size());

        // Rotary table: proj = kn @ Wr^T, then (cos, sin) per pair, shared by
        // every head -- which is exactly nn::rope's [n, head_dim/2, 2] layout.
        Tensor proj = nn::arena_tensor(arena, DType::F32, n, head_dim / 2);
        nn::linear(proj, tk, get("posenc.weight"));
        freqs[s] = nn::arena_tensor(arena, DType::F32, n * (head_dim / 2), 2);
        {
            std::vector<float> host((size_t)n * (head_dim / 2));
            nn::tensor_to_host(proj, host.data(), (int64_t)host.size());
            std::vector<float> cs(host.size() * 2);
            for (size_t i = 0; i < host.size(); ++i) {
                cs[i * 2] = std::cos(host[i]);
                cs[i * 2 + 1] = std::sin(host[i]);
            }
            nn::tensor_from_host(freqs[s], cs.data(), (int64_t)cs.size());
        }

        Tensor desc = nn::arena_tensor(arena, DType::F32, n, input_dim);
        nn::tensor_from_host(desc, I.descriptors, n * input_dim);
        x[s] = nn::arena_tensor(arena, DType::F32, n, dim);
        LinearOpts lp;
        lp.bias = get("input_proj.bias");
        nn::linear(x[s], desc, get("input_proj.weight"), lp);
        nn::tensor_debug_dump(s ? "lg input_proj[1]" : "lg input_proj[0]", x[s]);
    }

    for (int l = 0; l < n_layers; ++l) {
        // ---- self-attention, each image independently ----
        for (int s = 0; s < 2; ++s) {
            const int64_t n = nn_[s];
            vk::ArenaScope scope(arena);
            char p[96];
            std::snprintf(p, sizeof p, "transformers.%d.self_attn", l);
            const std::string sa = p;

            Tensor qkv = nn::arena_tensor(arena, DType::F32, n, 3 * dim);
            LinearOpts lq;
            lq.bias = get(sa + ".Wqkv.bias");
            nn::linear(qkv, x[s], get(sa + ".Wqkv.weight"), lq);

            // q and k rotate in place inside the fused buffer; v does not.
            nn::rope(qkv, freqs[s], n_heads, head_dim, n, 1, 3 * dim);
            nn::rope(qkv.offsetElems(dim), freqs[s], n_heads, head_dim, n, 1, 3 * dim);

            Tensor ctx = nn::arena_tensor(arena, DType::F32, n, dim);
            AttnOpts ao;
            ao.n_heads = n_heads;
            ao.head_dim = head_dim;
            ao.q_stride = ao.k_stride = ao.v_stride = 3 * dim;
            ao.arena = &arena;
            nn::attention(ctx, qkv, qkv.offsetElems(dim), qkv.offsetElems(2 * dim), n, n,
                          ao);

            Tensor msg = nn::arena_tensor(arena, DType::F32, n, dim);
            LinearOpts lo;
            lo.bias = get(sa + ".out_proj.bias");
            nn::linear(msg, ctx, get(sa + ".out_proj.weight"), lo);
            ffn(x[s], msg, n, "self_attn", l);
        }

        // ---- cross-attention, both directions off the same projections ----
        {
            vk::ArenaScope scope(arena);
            char p[96];
            std::snprintf(p, sizeof p, "transformers.%d.cross_attn", l);
            const std::string ca = p;

            Tensor qk[2], v[2], msg[2];
            for (int s = 0; s < 2; ++s) {
                qk[s] = nn::arena_tensor(arena, DType::F32, nn_[s], dim);
                v[s] = nn::arena_tensor(arena, DType::F32, nn_[s], dim);
                LinearOpts lqk, lv;
                lqk.bias = get(ca + ".to_qk.bias");
                lv.bias = get(ca + ".to_v.bias");
                nn::linear(qk[s], x[s], get(ca + ".to_qk.weight"), lqk);
                nn::linear(v[s], x[s], get(ca + ".to_v.weight"), lv);
            }
            for (int s = 0; s < 2; ++s) {
                const int o = 1 - s;
                Tensor ctx = nn::arena_tensor(arena, DType::F32, nn_[s], dim);
                AttnOpts ao;
                ao.n_heads = n_heads;
                ao.head_dim = head_dim;
                ao.arena = &arena;
                // No rotary here: the cross block attends between images, where
                // a within-image position has no meaning.
                nn::attention(ctx, qk[s], qk[o], v[o], nn_[s], nn_[o], ao);
                msg[s] = nn::arena_tensor(arena, DType::F32, nn_[s], dim);
                LinearOpts lo;
                lo.bias = get(ca + ".to_out.bias");
                nn::linear(msg[s], ctx, get(ca + ".to_out.weight"), lo);
            }
            for (int s = 0; s < 2; ++s) ffn(x[s], msg[s], nn_[s], "cross_attn", l);
        }
        if (l == 0 || l == n_layers - 1) {
            char lbl[48];
            std::snprintf(lbl, sizeof lbl, "lg layer%d[0]", l);
            nn::tensor_debug_dump(lbl, x[0]);
        }
    }

    // ---- assignment ----
    char pfx[64];
    std::snprintf(pfx, sizeof pfx, "log_assignment.%d", n_layers - 1);
    const std::string la = pfx;

    Tensor md[2], z[2];
    for (int s = 0; s < 2; ++s) {
        md[s] = nn::arena_tensor(arena, DType::F32, nn_[s], dim);
        LinearOpts lf;
        lf.bias = get(la + ".final_proj.bias");
        nn::linear(md[s], x[s], get(la + ".final_proj.weight"), lf);
        z[s] = nn::arena_tensor(arena, DType::F32, nn_[s], 1, 1, 1, 2);
        LinearOpts lm;
        lm.bias = get(la + ".matchability.bias");
        lm.act = Act::LogSigmoid;   // logsigmoid(z), the certainty term
        nn::linear(z[s], x[s], get(la + ".matchability.weight"), lm);
    }

    // sim = (md0 / d^0.25) . (md1 / d^0.25), i.e. one 1/sqrt(dim) on the product.
    Tensor sim = nn::arena_tensor(arena, DType::F32, n0, n1);
    nn::matmul_nt(sim, md[0], md[1], 1.0f / std::sqrt((float)dim));
    nn::tensor_debug_dump("lg mdesc0", md[0]);
    nn::tensor_debug_dump("lg z0", z[0].view(n0));
    nn::tensor_debug_dump("lg sim", sim);

    // The row and column log-sum-exps, and then the two arg-maxes with every
    // column-dependent term folded into a bias. See shaders/aliked.slang for
    // why the score matrix is never written out.
    Tensor lse0 = nn::arena_tensor(arena, DType::F32, n0);
    Tensor lse1 = nn::arena_tensor(arena, DType::F32, n1);
    Tensor bias0 = nn::arena_tensor(arena, DType::F32, n1);
    Tensor bias1 = nn::arena_tensor(arena, DType::F32, n0);
    Tensor val0 = nn::arena_tensor(arena, DType::F32, n0);
    Tensor val1 = nn::arena_tensor(arena, DType::F32, n1);
    Tensor idx0 = nn::arena_tensor(arena, DType::I32, n0);
    Tensor idx1 = nn::arena_tensor(arena, DType::I32, n1);

    auto run = [&](const char* entry, const Tensor& ov, const Tensor& oi,
                   const Tensor& bias, int64_t R, int64_t C, int64_t stride,
                   int64_t step, float scale = 1.0f) {
        AssignParams p{};
        p.out_val = ov.ptr;
        p.out_idx = oi.valid() ? oi.ptr : vk::or_fallback(0);
        p.sim = sim.ptr;
        p.bias = bias.valid() ? bias.ptr : vk::or_fallback(0);
        p.R = (uint32_t)R;
        p.C = (uint32_t)C;
        p.stride = (uint32_t)stride;
        p.step = (uint32_t)step;
        p.scale = scale;
        const vk::Stream::Fold fold = vk::Stream::fold1D(R, 1);
        p.groups_per_row = fold.per_row;
        vk::Stream::get().dispatch(entry, vk::SpecList{0u, 0u}, fold.per_row, fold.rows, 1,
                                   &p, sizeof(p), (double)R * C * vk::Stream::kElemWork);
    };

    run("aliked.assign_logsumexp", lse0, {}, {}, n0, n1, n1, 1);
    run("aliked.assign_logsumexp", lse1, {}, {}, n1, n0, 1, n1);
    // bias for a row scan is (logsigmoid(z1) - lse1) per column, and vice versa.
    nn::add(bias0, z[1].view(n1), lse1, 1.0f, -1.0f);
    nn::add(bias1, z[0].view(n0), lse0, 1.0f, -1.0f);
    // 2 * sim, because the score sums a row log-softmax and a column one and
    // each contains sim. See the kernel.
    run("aliked.assign_argmax", val0, idx0, bias0, n0, n1, n1, 1, 2.0f);
    run("aliked.assign_argmax", val1, idx1, bias1, n1, n0, 1, n1, 2.0f);
    nn::tensor_debug_dump("lg lse0", lse0);
    nn::tensor_debug_dump("lg lse1", lse1);
    nn::tensor_debug_dump("lg val0", val0);

    std::vector<float> h_val0((size_t)n0), h_lse0((size_t)n0), h_z0((size_t)n0);
    std::vector<int32_t> h_idx0((size_t)n0), h_idx1((size_t)n1);
    nn::tensor_to_host(val0, h_val0.data(), n0);
    nn::tensor_to_host(lse0, h_lse0.data(), n0);
    nn::tensor_to_host(z[0].view(n0), h_z0.data(), n0);
    vk::Stream::get().download(h_idx0.data(), idx0.ptr, (uint64_t)n0 * 4);
    vk::Stream::get().download(h_idx1.data(), idx1.ptr, (uint64_t)n1 * 4);

    out.reserve((size_t)std::min(n0, n1));
    for (int64_t i = 0; i < n0; ++i) {
        const int32_t j = h_idx0[(size_t)i];
        if (j < 0 || j >= n1) continue;
        if (h_idx1[(size_t)j] != (int32_t)i) continue;   // mutual nearest
        // The row-constant terms, which the arg-max did not need.
        const float score = std::exp(h_val0[(size_t)i] - h_lse0[(size_t)i] +
                                     h_z0[(size_t)i]);
        if (score < opts.min_score) continue;
        out.push_back({(uint32_t)i, (uint32_t)j, score});
    }
    return out;
}

// ---------------------------------------------------------------------------

Matcher::Matcher() : impl_(new Impl) {}
Matcher::~Matcher() { delete impl_; }
bool Matcher::loaded() const { return impl_->loaded; }
int  Matcher::descriptorDim() const { return impl_->input_dim; }

void Matcher::load(const std::string& model) {
    NN_ENSURE_EMBEDDED_MODULES(aliked);
    impl_->load(resolve_model(model));
}

std::vector<Match> Matcher::match(const MatchInput& a, const MatchInput& b,
                                  const MatchOptions& opts) {
    NN_CHECK(impl_->loaded, "LightGlue::match before load()");
    NN_CHECK((int)a.n == 0 || impl_->input_dim > 0, "LightGlue: no descriptor width");
    return impl_->match(a, b, opts);
}

}  // namespace aliked
