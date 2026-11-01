#include "marigold/model/Vae.h"

#include "marigold/Common.h"
#include "nn/Ops.h"

#include <algorithm>
#include <cmath>

namespace marigold {
namespace {

using nn::DType;
using nn::Tensor;
using nn::arena_tensor;

// Upsampled rows held at once by Vae::up: its full-size input would be the
// largest buffer in the decoder (192 channels at full resolution).
constexpr uint64_t kBandBytes = 64ull << 20;

}  // namespace

void Vae::load(const nn::SafetensorsFile& f, bool encoder, const char* tag) {
    const std::string keep[2] = {encoder ? "vae.encoder." : "vae.decoder.",
                                 encoder ? "vae.quant_conv." : "vae.post_quant_conv."};
    for (const std::string& name : f.names()) {
        if (name.rfind(keep[0], 0) != 0 && name.rfind(keep[1], 0) != 0) continue;
        nn::OnnxTensor t = f.read(name);
        const bool gamma = name.size() > 6 && name.compare(name.size() - 6, 6, ".gamma") == 0;
        if (gamma) {
            // QwenImageRMS_norm: F.normalize(x) * sqrt(C) * gamma.
            const float s = std::sqrt((float)t.data.size());
            for (float& v : t.data) v *= s;
        }
        w_.stage(name, t.shape, std::move(t.data), t.was_f16);
    }
    w_.upload(tag);
}

void Vae::release() { w_.release(); }

namespace {

void rms(const Tensor& out, const Tensor& x, const Tensor& gamma, bool silu) {
    const int64_t C = x.shape[x.ndim - 1];
    nn::l2_normalize_rows(out.view(out.numel() / C, C), x.view(x.numel() / C, C));
    nn::mul(out, out, gamma, silu ? nn::Act::Silu : nn::Act::None);
}

void conv(vk::Arena& a, const nn::WeightStore& w, const Tensor& out, const Tensor& in,
          const std::string& p, int k, int stride, int pad_y, int pad_x,
          const Tensor& residual = {}) {
    nn::ConvOpts o;
    o.stride_y = o.stride_x = stride;
    o.pad_y = pad_y;
    o.pad_x = pad_x;
    o.bias = w.get(p + ".bias");
    o.residual = residual;
    nn::conv2d(a, out, in, w.get(p + ".weight"), k, k, o);
}

}  // namespace

// x + conv2(silu(rms2(conv1(silu(rms1(x)))))), the sum written over the
// shortcut, so a block whose width does not change runs in place.
Tensor Vae::res_block(vk::Arena& a, const Tensor& x, int cout, const std::string& p) const {
    const int64_t H = x.shape[0], W = x.shape[1], C = x.shape[2];
    Tensor out = x;
    if (C != cout) {
        out = arena_tensor(a, DType::F32, H, W, cout);
        conv(a, w_, out, x, p + ".conv_shortcut", 1, 1, 0, 0);
    }
    vk::ArenaScope scope(a);
    Tensor t = arena_tensor(a, DType::F32, H, W, C);
    rms(t, x, w_.get(p + ".norm1.gamma"), true);
    Tensor u = arena_tensor(a, DType::F32, H, W, cout);
    conv(a, w_, u, t, p + ".conv1", 3, 1, 1, 1);
    rms(u, u, w_.get(p + ".norm2.gamma"), true);
    conv(a, w_, out, u, p + ".conv2", 3, 1, 1, 1, out);
    return out;
}

// One head as wide as the map (384), past the fused kernel's 256: chunked
// GEMMs and a row softmax instead. softmax(qk) (v + b) = softmax(qk) v + b,
// so v's bias joins at the end and v^T comes straight out of a GEMM.
void Vae::attention(vk::Arena& a, const Tensor& x, const std::string& p) const {
    vk::ArenaScope scope(a);
    const int64_t N = x.shape[0] * x.shape[1], C = x.shape[2];
    const Tensor wqkv = w_.get(p + ".to_qkv.weight").asMatrix();
    const Tensor bqkv = w_.get(p + ".to_qkv.bias");

    Tensor xn = arena_tensor(a, DType::F32, N, C);
    rms(xn, x.view(N, C), w_.get(p + ".norm.gamma"), false);
    Tensor q = arena_tensor(a, DType::F32, N, C);
    nn::LinearOpts lo;
    lo.bias = bqkv.view(C);
    nn::linear(q, xn, wqkv.slice0(0, C), lo);
    Tensor k = arena_tensor(a, DType::F32, N, C);
    lo.bias = bqkv.offsetElems(C).view(C);
    nn::linear(k, xn, wqkv.slice0(C, C), lo);
    Tensor k16 = arena_tensor(a, DType::F16, N, C);
    nn::copy(k16, k);
    Tensor vt = arena_tensor(a, DType::F32, C, N);
    nn::matmul_nt(vt, wqkv.slice0(2 * C, C), xn);
    Tensor vt16 = arena_tensor(a, DType::F16, C, N);
    nn::copy(vt16, vt);

    Tensor o = arena_tensor(a, DType::F32, N, C);
    const int64_t chunk = std::max<int64_t>(64, (int64_t)((kBandBytes / 4) / (uint64_t)N));
    Tensor s = arena_tensor(a, DType::F32, std::min(chunk, N), N);
    const float scale = 1.0f / std::sqrt((float)C);
    nn::LinearOpts po;
    po.bias = bqkv.offsetElems(2 * C).view(C);
    for (int64_t q0 = 0; q0 < N; q0 += chunk) {
        const int64_t n = std::min(chunk, N - q0);
        Tensor sc = s.view(n, N);
        nn::matmul_nt(sc, q.slice0(q0, n), k16, scale);
        nn::softmax_rows(sc, sc);
        nn::linear(o.slice0(q0, n), sc, vt16, po);
    }
    nn::LinearOpts out;
    out.bias = w_.get(p + ".proj.bias");
    out.residual = x.view(N, C);
    nn::linear(x.view(N, C), o, w_.get(p + ".proj.weight").asMatrix(), out);
}

// Nearest 2x and a 3x3 conv to half the channels, a band of rows at a time:
// the upsampled map is never whole. Zero rows above and below the image stand
// in for the conv's padding.
Tensor Vae::up(vk::Arena& a, const Tensor& x, const std::string& p) const {
    const int64_t H = x.shape[0], W = x.shape[1], C = x.shape[2];
    const Tensor wt = w_.get(p + ".resample.1.weight");
    const int64_t Co = wt.shape[0];
    Tensor y = arena_tensor(a, DType::F32, 2 * H, 2 * W, Co);
    vk::ArenaScope scope(a);
    const uint64_t row = (uint64_t)(2 * W * C * 4);
    const int64_t sb = std::clamp<int64_t>((int64_t)(kBandBytes / (2 * row)) - 2, 1, H);
    Tensor src = arena_tensor(a, DType::F32, sb + 2, W, C);
    Tensor upb = arena_tensor(a, DType::F32, 2 * (sb + 2), 2 * W, C);
    for (int64_t y0 = 0; y0 < H; y0 += sb) {
        const int64_t n = std::min(sb, H - y0);
        const int64_t first = std::max<int64_t>(0, y0 - 1), last = std::min(H, y0 + n + 1);
        const Tensor band = src.view(n + 2, W, C);
        if (y0 == 0 || y0 + n == H) nn::fill(band, 0.0f);
        nn::copy(band.slice0(first - (y0 - 1), last - first), x.slice0(first, last - first));
        const Tensor ub = upb.view(2 * (n + 2), 2 * W, C);
        nn::upsample_nearest2x(ub, band);
        conv(a, w_, y.slice0(2 * y0, 2 * n), ub.slice0(1, 2 * n + 2), p + ".resample.1", 3, 1,
             0, 1);
    }
    return y;
}

void Vae::encode(vk::Arena& a, const Tensor& rgb, const Tensor& mu) const {
    vk::ArenaScope scope(a);
    int64_t H = rgb.shape[0], W = rgb.shape[1];
    const std::string e = "vae.encoder.";
    Tensor x = arena_tensor(a, DType::F32, H, W, w_.get(e + "conv_in.weight").shape[0]);
    conv(a, w_, x, rgb, e + "conv_in", 3, 1, 1, 1);
    for (int i = 0;; ++i) {
        const std::string p = e + "down_blocks." + std::to_string(i);
        if (w_.has(p + ".resample.1.weight")) {
            // ZeroPad2d((0, 1, 0, 1)) then a stride-2 conv: pad 0, the far taps read zero.
            Tensor y = arena_tensor(a, DType::F32, H / 2, W / 2, x.shape[2]);
            conv(a, w_, y, x, p + ".resample.1", 3, 2, 0, 0);
            x = y;
            H /= 2;
            W /= 2;
        } else if (w_.has(p + ".conv1.weight")) {
            x = res_block(a, x, (int)w_.get(p + ".conv1.weight").shape[0], p);
        } else {
            break;
        }
    }
    const std::string m = e + "mid_block.";
    x = res_block(a, x, (int)x.shape[2], m + "resnets.0");
    attention(a, x, m + "attentions.0");
    x = res_block(a, x, (int)x.shape[2], m + "resnets.1");
    rms(x, x, w_.get(e + "norm_out.gamma"), true);
    Tensor h = arena_tensor(a, DType::F32, H, W, w_.get(e + "conv_out.weight").shape[0]);
    conv(a, w_, h, x, e + "conv_out", 3, 1, 1, 1);
    // quant_conv's first half is the posterior mean; the rest is its log variance.
    nn::LinearOpts lo;
    lo.bias = w_.get("vae.quant_conv.bias").view(mu.shape[2]);
    nn::linear(mu.view(H * W, mu.shape[2]), h.view(H * W, h.shape[2]),
               w_.get("vae.quant_conv.weight").asMatrix().slice0(0, mu.shape[2]), lo);
}

void Vae::decode(vk::Arena& a, const Tensor& z, const Tensor& rgb) const {
    vk::ArenaScope scope(a);
    const int64_t h = z.shape[0], w = z.shape[1];
    const std::string d = "vae.decoder.";
    Tensor zq = arena_tensor(a, DType::F32, h, w, z.shape[2]);
    nn::LinearOpts lo;
    lo.bias = w_.get("vae.post_quant_conv.bias");
    nn::linear(zq.view(h * w, z.shape[2]), z.view(h * w, z.shape[2]),
               w_.get("vae.post_quant_conv.weight").asMatrix(), lo);
    Tensor x = arena_tensor(a, DType::F32, h, w, w_.get(d + "conv_in.weight").shape[0]);
    conv(a, w_, x, zq, d + "conv_in", 3, 1, 1, 1);
    const std::string m = d + "mid_block.";
    x = res_block(a, x, (int)x.shape[2], m + "resnets.0");
    attention(a, x, m + "attentions.0");
    x = res_block(a, x, (int)x.shape[2], m + "resnets.1");
    for (int u = 0;; ++u) {
        const std::string p = d + "up_blocks." + std::to_string(u);
        if (!w_.has(p + ".resnets.0.conv1.weight")) break;
        for (int r = 0; w_.has(p + ".resnets." + std::to_string(r) + ".conv1.weight"); ++r) {
            const std::string rp = p + ".resnets." + std::to_string(r);
            x = res_block(a, x, (int)w_.get(rp + ".conv1.weight").shape[0], rp);
        }
        if (w_.has(p + ".upsamplers.0.resample.1.weight")) x = up(a, x, p + ".upsamplers.0");
    }
    rms(x, x, w_.get(d + "norm_out.gamma"), true);
    conv(a, w_, rgb, x, d + "conv_out", 3, 1, 1, 1);
}

uint64_t Vae::arenaBytes(int H, int W) {
    // Three full-resolution 96-channel maps in either pass, the levels below
    // them still on the stack, and the bands: 1.45 KB a pixel plus the bands.
    return (uint64_t)((double)H * W * 1480.0) + 3 * kBandBytes + (64ull << 20);
}

}  // namespace marigold
