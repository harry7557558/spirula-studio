#include "marigold/model/Dit.h"

#include "marigold/Common.h"
#include "marigold/model/Files.h"
#include "marigold/model/Layout.h"
#include "nn/Ops.h"

#include <algorithm>
#include <cmath>

namespace marigold {
namespace {

using nn::DType;
using nn::Tensor;
using nn::arena_tensor;

// The tensor-core GEMM rounds its activations to fp16, and Qwen-Image's run to
// 5.3e4 at the text MLP's output projection on an ordinary photo (bf16 in
// training never had a ceiling). 2^-8 leaves a 300x margin.
constexpr float kXScale = 1.0f / 256.0f;

// The modulation rows a block keeps per stream, in model/Install.cpp's
// [shift1, scale1, gate1, shift2, scale2, gate2] turned into what is used.
enum { kLn1W, kLn1B, kGate1, kLn2W, kLn2B, kGate2, kModRows };

int64_t largest_projection() {
    int64_t n = 0;
    for (const BlockLinear& L : kBlockLinears) n = std::max(n, L.N * L.K);
    return n;
}

// QwenEmbedRope(theta=10000, axes_dim=(16, 56, 56)): 64 rotation pairs, the
// frame axis first. Float32 throughout, as torch computes them.
void rope_row(float pf, float ph, float pw, float* out) {
    const int dims[3] = {16, 56, 56};
    const float pos[3] = {pf, ph, pw};
    int k = 0;
    for (int a = 0; a < 3; ++a)
        for (int i = 0; i < dims[a] / 2; ++i, ++k) {
            const float inv = 1.0f / std::pow(10000.0f, (float)(2 * i) / (float)dims[a]);
            const float ang = pos[a] * inv;
            out[2 * k] = std::cos(ang);
            out[2 * k + 1] = std::sin(ang);
        }
}

}  // namespace

void Dit::load(const nn::SafetensorsFile& base) {
    // Each block's two streams: LayerNorm(x) * (1 + scale) + shift is a
    // LayerNorm with that affine, and the gate scales an output projection.
    std::vector<float> mods((size_t)kLayers * 2 * kModRows * kDim);
    for (int i = 0; i < kLayers; ++i)
        for (int s = 0; s < 2; ++s) {
            const nn::OnnxTensor m = base.read(block_tensor(i, s ? "mod_txt" : "mod_img"));
            float* dst = &mods[(size_t)((i * 2 + s) * kModRows) * kDim];
            for (int c = 0; c < kDim; ++c) {
                const float* r = m.data.data();
                dst[kLn1W * kDim + c] = 1.0f + r[1 * kDim + c];
                dst[kLn1B * kDim + c] = r[0 * kDim + c];
                dst[kGate1 * kDim + c] = r[2 * kDim + c];
                dst[kLn2W * kDim + c] = 1.0f + r[4 * kDim + c];
                dst[kLn2B * kDim + c] = r[3 * kDim + c];
                dst[kGate2 * kDim + c] = r[5 * kDim + c];
            }
        }
    w_.stage("mods", {kLayers * 2 * kModRows, kDim}, std::move(mods), false);

    // proj_out, then the unpacking to latents: rows are (c, dy, dx), which is
    // conv_transpose_patch's packed order, and z = mu - std * pred folds the
    // latent scale in. The per-row bias goes on as a 2x2 tile.
    const nn::OnnxTensor b = base.read("proj_out.bias");
    std::vector<float> w = read_nf4(base, "proj_out");
    std::vector<float> tile((size_t)4 * kLatent);
    for (int r = 0; r < 4 * kLatent; ++r) {
        const float s = -kLatentStd[r / 4];
        for (int k = 0; k < kDim; ++k) w[(size_t)r * kDim + k] *= s;
        tile[(size_t)((r % 4) * kLatent + r / 4)] = s * b.data[(size_t)r];
    }
    w_.stage("proj_out.weight", {4 * kLatent, kDim}, std::move(w), false);
    w_.stage("proj_out.tile", {2, 2, kLatent}, std::move(tile), false);
    w_.upload("marigold.dit");

    const int64_t big = largest_projection();
    w16_ = vk::Allocator::get().allocDevice((uint64_t)big * 2, "marigold.w16");
    ba_ = vk::Allocator::get().allocDevice((uint64_t)big * 4, "marigold.lora");
    bias_ = vk::Allocator::get().allocDevice((uint64_t)kMlp * 4, "marigold.bias");
}

void Dit::loadTask(Task t, const nn::SafetensorsFile& task) {
    nn::WeightStore& w = task_w_[(int)t];
    w.release();
    const nn::OnnxTensor txt = task.read("txt");
    text_tokens_[(int)t] = (int)txt.shape[0];
    w.stage("txt", txt.shape, txt.data, false);

    const nn::OnnxTensor no = task.read("norm_out");
    std::vector<float> ln((size_t)2 * kDim);
    for (int c = 0; c < kDim; ++c) {
        ln[(size_t)c] = 1.0f + no.data[(size_t)c];
        ln[(size_t)(kDim + c)] = no.data[(size_t)(kDim + c)];
    }
    w.stage("norm_out", {2, kDim}, std::move(ln), false);

    // img_in reads the packed latents (mu - mean) / std; that affine is folded
    // into it, so the posterior mean goes in as it is.
    nn::OnnxTensor wi = task.read("img_in.weight");
    nn::OnnxTensor bi = task.read("img_in.bias");
    for (int n = 0; n < kDim; ++n)
        for (int j = 0; j < 4 * kLatent; ++j) {
            float& v = wi.data[(size_t)(n * 4 * kLatent + j)];
            bi.data[(size_t)n] -= v * kLatentMean[j / 4] / kLatentStd[j / 4];
            v /= kLatentStd[j / 4];
        }
    w.stage("img_in.weight", wi.shape, std::move(wi.data), false);
    w.stage("img_in.bias", bi.shape, std::move(bi.data), false);
    w.upload("marigold.task");
}

void Dit::release() {
    w_.release();
    for (nn::WeightStore& w : task_w_) w.release();
    rope_w_.release();
    rope_.clear();
    for (nn::DevicePtr* p : {&w16_, &ba_, &bias_}) {
        vk::Allocator::get().free(*p);
        *p = 0;
    }
}

uint64_t Dit::scratchBytes() { return (uint64_t)largest_projection() * 6 + kMlp * 4; }

uint64_t Dit::deviceBytes() const {
    uint64_t n = w_.deviceBytes() + rope_w_.deviceBytes() + (w16_ ? scratchBytes() : 0);
    for (const nn::WeightStore& w : task_w_) n += w.deviceBytes();
    return n;
}

uint64_t Dit::arenaBytes(int64_t tokens, int batch) const {
    const int64_t L = std::max(text_tokens_[0], text_tokens_[1]);
    const int64_t all = (int64_t)batch * (tokens + L), img = (int64_t)batch * tokens;
    // The two streams, and the larger of the attention's live set (LayerNorm,
    // fused qkv, attention out) and the MLP's (LayerNorm, hidden).
    const int64_t streams = all * kDim;
    const int64_t attn = all * (kDim + 3 * kDim + kDim);
    const int64_t mlp = img * (kDim + kMlp);
    return (uint64_t)(streams + std::max(attn, mlp) + img * 4 * kLatent) * 4 + (64ull << 20);
}

Tensor Dit::mod(int block, int stream, int which) const {
    return w_.get("mods").offsetElems((int64_t)((block * 2 + stream) * kModRows + which) * kDim)
        .view(kDim);
}

// One projection's fp16 weight: its NF4 base, plus the LoRA's B A, times a
// per-output gate where the block multiplies the result by one.
Tensor Dit::expand(const BlockLin& l, const BlockLinear& L, const Tensor& row_scale) {
    const int64_t rows = L.N / L.parts;
    const Tensor ba(ba_, DType::F32, L.N, L.K);
    for (int p = 0; p < L.parts; ++p)
        nn::matmul_nt(ba.slice0(p * rows, rows), l.lora_b.slice0(p * rows, rows),
                      l.lora_a.slice0(p * L.K, L.K));
    const Tensor w(w16_, DType::F16, L.N, L.K);
    nn::Nf4Opts o;
    o.residual = ba;
    o.row_scale = row_scale;
    nn::dequant_nf4(w, l.q4, l.absmax, o);
    return w;
}

const std::pair<Tensor, Tensor>& Dit::rope(int h, int w, Task t) {
    const auto key = std::make_tuple(h, w, (int)t);
    auto it = rope_.find(key);
    if (it != rope_.end()) return it->second;
    // scale_rope: rows run -(gh - gh/2) .. gh/2 - 1, and the text continues
    // from max(gh/2, gw/2) on every axis.
    const int gh = h / 2, gw = w / 2, L = text_tokens_[(int)t];
    std::vector<float> img((size_t)gh * gw * 128), txt((size_t)L * 128);
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
            rope_row(0.0f, (float)(r - (gh - gh / 2)), (float)(c - (gw - gw / 2)),
                     &img[(size_t)(r * gw + c) * 128]);
    const int start = std::max(gh / 2, gw / 2);
    for (int j = 0; j < L; ++j) {
        const float p = (float)(start + j);
        rope_row(p, p, p, &txt[(size_t)j * 128]);
    }
    const std::string tag = std::to_string(h) + "x" + std::to_string(w) + task_name(t);
    rope_w_.stage("img" + tag, {(int64_t)gh * gw, 64, 2}, std::move(img), false);
    rope_w_.stage("txt" + tag, {L, 64, 2}, std::move(txt), false);
    rope_w_.upload("marigold.rope");
    return rope_[key] = {rope_w_.get("img" + tag), rope_w_.get("txt" + tag)};
}

void Dit::forward(vk::Arena& a, Streamer& s, Task t, const std::vector<Tensor>& mu,
                  const std::vector<Tensor>& z, const nn::StageDump* dump) {
    const nn::WeightStore& tw = task_w_[(int)t];
    const int B = (int)mu.size();
    const int64_t h = mu[0].shape[0], w = mu[0].shape[1];
    const int64_t T = (h / 2) * (w / 2), L = text_tokens_[(int)t], J = L + T;
    const auto& freqs = rope((int)h, (int)w, t);
    vk::ArenaScope scope(a);

    Tensor x = arena_tensor(a, DType::F32, B * T, kDim);
    Tensor c = arena_tensor(a, DType::F32, B * L, kDim);
    {
        vk::ArenaScope tokens_scope(a);
        Tensor tok = arena_tensor(a, DType::F32, B * T, 4 * kLatent);
        for (int b = 0; b < B; ++b) nn::patchify(tok.slice0(b * T, T), mu[(size_t)b], 2);
        nn::LinearOpts lo;
        lo.bias = tw.get("img_in.bias");
        nn::linear(x, tok, tw.get("img_in.weight"), lo);
    }
    for (int b = 0; b < B; ++b) nn::copy(c.slice0(b * L, L), tw.get("txt"));
    if (dump) dump->tensor("img_in", x.slice0(0, T), {T, kDim});

    const Tensor bias_scratch(bias_, DType::F32, kMlp);
    auto gated = [&](const Tensor& bias, const Tensor& gate) {
        const Tensor out = bias_scratch.view(bias.numel());
        nn::mul(out, bias, gate);
        return out;
    };
    nn::LinearOpts proj;
    proj.x_scale = kXScale;

    for (int i = 0; i < kLayers; ++i) {
        const BlockView v = s.acquire(i);
        const bool last = i == kLayers - 1;
        {
            vk::ArenaScope attn_scope(a);
            Tensor qkv = arena_tensor(a, DType::F32, B * J, 3 * kDim);
            Tensor att = arena_tensor(a, DType::F32, B * J, kDim);
            Tensor ln = arena_tensor(a, DType::F32, B * std::max(T, L), kDim);
            for (int st = 0; st < 2; ++st) {
                const bool img = st == 0;
                const int64_t n = img ? T : L, off = img ? L : 0;
                nn::layer_norm(ln.view(B * n, kDim), img ? x : c, mod(i, st, kLn1W),
                               mod(i, st, kLn1B), 1e-6f);
                const BlockLin& l = v.lin[img ? kImgQkv : kTxtQkv];
                const Tensor wq = expand(l, kBlockLinears[img ? kImgQkv : kTxtQkv], {});
                nn::LinearOpts lo = proj;
                lo.bias = l.bias;
                for (int b = 0; b < B; ++b) {
                    const Tensor rows = qkv.slice0(b * J + off, n);
                    nn::linear(rows, ln.slice0(b * n, n), wq, lo);
                    for (int qk = 0; qk < 2; ++qk) {
                        const Tensor head = rows.offsetElems(qk * kDim);
                        nn::rms_norm_heads(head, v.norm[st * 2 + qk], n, kHeads, kHeadDim,
                                           3 * kDim, 1e-6f);
                        nn::rope(head, img ? freqs.first : freqs.second, kHeads, kHeadDim, n, 1,
                                 3 * kDim);
                    }
                }
            }
            nn::AttnOpts ao;
            ao.n_heads = kHeads;
            ao.head_dim = kHeadDim;
            ao.batch = B;
            ao.q_stride = ao.k_stride = ao.v_stride = 3 * kDim;
            ao.out_stride = kDim;
            ao.arena = &a;
            // Q K^T in fp16 moved block 0's output by 3e-2 (rel L2 vs fp32) and
            // depth by 1.3e-2; the fp32 kernel holds both near 1e-4 for 12% more time.
            ao.allow_coop = false;
            nn::attention(att, qkv, qkv.offsetElems(kDim), qkv.offsetElems(2 * kDim), J, J, ao);

            for (int st = 0; st < (last ? 1 : 2); ++st) {
                const bool img = st == 0;
                const int64_t n = img ? T : L, off = img ? L : 0;
                const Tensor& xs = img ? x : c;
                const BlockLin& l = v.lin[img ? kImgOut : kTxtOut];
                const Tensor wo = expand(l, kBlockLinears[img ? kImgOut : kTxtOut], mod(i, st, kGate1));
                nn::LinearOpts lo = proj;
                lo.bias = gated(l.bias, mod(i, st, kGate1));
                for (int b = 0; b < B; ++b) {
                    lo.residual = xs.slice0(b * n, n);
                    nn::linear(xs.slice0(b * n, n), att.slice0(b * J + off, n), wo, lo);
                }
            }
        }
        for (int st = 0; st < (last ? 1 : 2); ++st) {
            vk::ArenaScope mlp_scope(a);
            const bool img = st == 0;
            const Tensor& xs = img ? x : c;
            const int64_t n = (int64_t)B * (img ? T : L);
            Tensor ln = arena_tensor(a, DType::F32, n, kDim);
            nn::layer_norm(ln, xs, mod(i, st, kLn2W), mod(i, st, kLn2B), 1e-6f);
            Tensor hid = arena_tensor(a, DType::F32, n, kMlp);
            const BlockLin& l1 = v.lin[img ? kImgFc1 : kTxtFc1];
            nn::LinearOpts lo = proj;
            lo.bias = l1.bias;
            lo.act = nn::Act::GeluTanh;
            nn::linear(hid, ln, expand(l1, kBlockLinears[img ? kImgFc1 : kTxtFc1], {}), lo);
            const BlockLin& l2 = v.lin[img ? kImgFc2 : kTxtFc2];
            const Tensor w2 = expand(l2, kBlockLinears[img ? kImgFc2 : kTxtFc2], mod(i, st, kGate2));
            lo = proj;
            lo.bias = gated(l2.bias, mod(i, st, kGate2));
            lo.residual = xs;
            nn::linear(xs, hid, w2, lo);
        }
        s.release(i);
        if (dump && (i == 0 || i == 1 || i == 29 || i == 59)) {
            char name[32];
            std::snprintf(name, sizeof name, "block%02d", i);
            dump->tensor(name, x.slice0(0, T), {T, kDim});
            std::snprintf(name, sizeof name, "block%02d_txt", i);
            dump->tensor(name, c.slice0(0, L), {L, kDim});
        }
    }

    Tensor ln = arena_tensor(a, DType::F32, B * T, kDim);
    nn::layer_norm(ln, x, tw.get("norm_out").view(kDim),
                   tw.get("norm_out").offsetElems(kDim).view(kDim), 1e-6f);
    for (int b = 0; b < B; ++b) {
        const Tensor& zb = z[(size_t)b];
        nn::conv_transpose_patch(a, zb, ln.slice0(b * T, T).view(h / 2, w / 2, kDim),
                                 w_.get("proj_out.weight"), {}, 2);
        nn::add_tiled(zb, zb, w_.get("proj_out.tile"), (int)h, (int)w, kLatent, 2, 2);
        nn::add(zb, zb, mu[(size_t)b]);
    }
}

}  // namespace marigold
