#include "swin/Swin.h"

#include "nn/Ops.h"
#include "nn/core/Error.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace swin {
namespace {

using nn::DType;
using nn::Tensor;

std::string fmt(const char* f, int a, int b = 0) {
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

// The checkpoint name of one tensor, in either spelling. `what` is the
// Microsoft suffix; the Hugging Face one is looked up from it.
struct Names {
    Naming naming;
    std::string prefix;

    std::string patch(const char* what) const {
        if (naming == Naming::Microsoft) return prefix + "patch_embed." + what;
        const std::string w = what;
        if (w.compare(0, 5, "proj.") == 0)
            return prefix + "embeddings.patch_embeddings.projection." + w.substr(5);
        return prefix + "embeddings." + w;   // norm.weight / norm.bias
    }
    std::string block(int s, int b, const std::string& what) const {
        if (naming == Naming::Microsoft)
            return prefix + fmt("layers.%d.blocks.%d.", s, b) + what;
        static const std::pair<const char*, const char*> kMap[] = {
            {"norm1.", "layernorm_before."},
            {"norm2.", "layernorm_after."},
            {"attn.relative_position_bias_table", "attention.self.relative_position_bias_table"},
            {"attn.proj.", "attention.output.dense."},
            {"mlp.fc1.", "intermediate.dense."},
            {"mlp.fc2.", "output.dense."},
            {"attn.q.", "attention.self.query."},
            {"attn.k.", "attention.self.key."},
            {"attn.v.", "attention.self.value."},
        };
        for (const auto& kv : kMap) {
            const size_t n = std::strlen(kv.first);
            if (what.compare(0, n, kv.first) == 0)
                return prefix + fmt("encoder.layers.%d.blocks.%d.", s, b) + kv.second +
                       what.substr(n);
        }
        fail_name(what);
    }
    std::string merge(int s, const char* what) const {
        return prefix + fmt(naming == Naming::Microsoft ? "layers.%d.downsample."
                                                        : "encoder.layers.%d.downsample.",
                            s) +
               what;
    }
    std::string out_norm(int s, const char* what) const {
        if (naming == Naming::Microsoft) return prefix + fmt("norm%d.", s) + what;
        return prefix + fmt("hidden_states_norms.stage%d.", s + 1) + what;
    }
    [[noreturn]] static void fail_name(const std::string& what) {
        nn::fail("swin: no checkpoint name for '%s'", what.c_str());
    }
};

// Swin's relative_position_index, expanded: bias[h][i][j] = table[idx(i, j)][h]
// with idx = (dy + ws-1) * (2ws-1) + (dx + ws-1) over the ws*ws window tokens.
std::vector<float> expand_bias_table(const std::vector<float>& table, int ws, int heads) {
    const int n = ws * ws, side = 2 * ws - 1;
    NN_CHECK((int64_t)table.size() == (int64_t)side * side * heads,
             "swin: relative position table holds %zu, not (2*%d-1)^2 x %d", table.size(),
             ws, heads);
    std::vector<float> out((size_t)heads * n * n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const int idx = (i / ws - j / ws + ws - 1) * side + (i % ws - j % ws + ws - 1);
            for (int h = 0; h < heads; ++h)
                out[((size_t)h * n + i) * n + j] = table[(size_t)idx * heads + h];
        }
    return out;
}

int64_t align256(int64_t b) { return (b + 255) / 256 * 256; }

}  // namespace

Config infer_config(Naming naming, const std::string& prefix, const Has& has,
                    const Reader& read, int window) {
    Names nm{naming, prefix};
    Config c;
    c.window = window;
    c.clamp_window = naming == Naming::HuggingFace;
    c.embed_dim = (int)read(nm.patch("proj.bias")).numel();
    for (int s = 0; s < 4; ++s) {
        int d = 0;
        while (has(nm.block(s, d, "norm1.weight"))) ++d;
        NN_CHECK(d > 0, "swin: '%s' has no blocks in stage %d", prefix.c_str(), s);
        c.depths[s] = d;
        const nn::OnnxTensor t = read(nm.block(s, 0, "attn.relative_position_bias_table"));
        NN_CHECK(t.shape.size() == 2 && t.shape[0] == (2 * window - 1) * (2 * window - 1),
                 "swin: stage %d's position table is %s, not for a %d-token window", s,
                 t.shapeString().c_str(), window);
        c.heads[s] = (int)t.shape[1];
    }
    return c;
}

void stage_weights(nn::WeightStore& store, const std::string& dst, const Config& cfg,
                   Naming naming, const std::string& src_prefix, const Reader& read,
                   bool f16) {
    Names nm{naming, src_prefix};
    auto put = [&](const std::string& name, nn::OnnxTensor t, bool matrix) {
        store.stage(dst + "." + name, t.shape, std::move(t.data), matrix && f16);
    };
    put("patch.w", read(nm.patch("proj.weight")), true);
    put("patch.b", read(nm.patch("proj.bias")), false);
    put("patch_norm.w", read(nm.patch("norm.weight")), false);
    put("patch_norm.b", read(nm.patch("norm.bias")), false);

    for (int s = 0; s < 4; ++s) {
        for (int b = 0; b < cfg.depths[s]; ++b) {
            const std::string p = fmt("s%d.b%d.", s, b);
            put(p + "n1.w", read(nm.block(s, b, "norm1.weight")), false);
            put(p + "n1.b", read(nm.block(s, b, "norm1.bias")), false);
            put(p + "n2.w", read(nm.block(s, b, "norm2.weight")), false);
            put(p + "n2.b", read(nm.block(s, b, "norm2.bias")), false);
            put(p + "proj.w", read(nm.block(s, b, "attn.proj.weight")), true);
            put(p + "proj.b", read(nm.block(s, b, "attn.proj.bias")), false);
            put(p + "fc1.w", read(nm.block(s, b, "mlp.fc1.weight")), true);
            put(p + "fc1.b", read(nm.block(s, b, "mlp.fc1.bias")), false);
            put(p + "fc2.w", read(nm.block(s, b, "mlp.fc2.weight")), true);
            put(p + "fc2.b", read(nm.block(s, b, "mlp.fc2.bias")), false);
            if (naming == Naming::Microsoft) {
                put(p + "qkv.w", read(nm.block(s, b, "attn.qkv.weight")), true);
                put(p + "qkv.b", read(nm.block(s, b, "attn.qkv.bias")), false);
            } else {
                // Three [C, C] projections stacked into the [3C, C] one the
                // original computes, so a window's q, k and v are one GEMM.
                nn::OnnxTensor w, bias;
                for (const char* x : {"q", "k", "v"}) {
                    nn::OnnxTensor wx = read(nm.block(s, b, std::string("attn.") + x + ".weight"));
                    nn::OnnxTensor bx = read(nm.block(s, b, std::string("attn.") + x + ".bias"));
                    w.data.insert(w.data.end(), wx.data.begin(), wx.data.end());
                    bias.data.insert(bias.data.end(), bx.data.begin(), bx.data.end());
                    w.shape = {(int64_t)w.data.size() / wx.shape[1], wx.shape[1]};
                    bias.shape = {(int64_t)bias.data.size()};
                }
                put(p + "qkv.w", std::move(w), true);
                put(p + "qkv.b", std::move(bias), false);
            }
            const nn::OnnxTensor table =
                read(nm.block(s, b, "attn.relative_position_bias_table"));
            const int n = cfg.window * cfg.window;
            store.stage(dst + "." + p + "bias", {cfg.heads[s], n, n},
                        expand_bias_table(table.data, cfg.window, cfg.heads[s]), false);
        }
        if (s < 3) {
            put(fmt("s%d.merge_norm.w", s), read(nm.merge(s, "norm.weight")), false);
            put(fmt("s%d.merge_norm.b", s), read(nm.merge(s, "norm.bias")), false);
            put(fmt("s%d.merge.w", s), read(nm.merge(s, "reduction.weight")), true);
        }
        if (cfg.out[s]) {
            put(fmt("out%d.w", s), read(nm.out_norm(s, "weight")), false);
            put(fmt("out%d.b", s), read(nm.out_norm(s, "bias")), false);
        }
    }
}

Backbone::~Backbone() { release(); }

void Backbone::release() {
    for (auto& kv : labels_) nn::vk::device_free(kv.second.first);
    labels_.clear();
    store_ = nullptr;
}

void Backbone::bind(const nn::WeightStore& store, const std::string& prefix,
                    const Config& cfg) {
    release();
    store_ = &store;
    prefix_ = prefix + ".";
    cfg_ = cfg;
}

// Which of the nine cyclic-shift regions each window token came from, in the
// rolled frame: along an axis of padded length L, [0, L-ws) is 0,
// [L-ws, L-shift) is 1 and [L-shift, L) is 2.
Tensor Backbone::labels(int hp, int wp, int ws, int shift) {
    const auto key = std::make_tuple(hp, wp, ws, shift);
    auto it = labels_.find(key);
    if (it != labels_.end()) return it->second.second;
    const int nwh = hp / ws, nww = wp / ws, n = ws * ws;
    std::vector<int32_t> lab((size_t)nwh * nww * n);
    auto region = [&](int v, int len) { return (v >= len - ws) + (v >= len - shift); };
    for (int wi = 0; wi < nwh; ++wi)
        for (int wj = 0; wj < nww; ++wj)
            for (int t = 0; t < n; ++t)
                lab[((size_t)wi * nww + wj) * n + t] =
                    region(wi * ws + t / ws, hp) * 3 + region(wj * ws + t % ws, wp);
    const nn::DevicePtr p = nn::vk::device_alloc(lab.size() * 4, "swin-labels");
    nn::vk::Stream::get().upload(p, lab.data(), lab.size() * 4);
    Tensor t(p, DType::I32, nwh * nww, n);
    labels_[key] = {p, t};
    return t;
}

Features Backbone::forward(nn::vk::Arena& arena, const Tensor& image, int H, int W) {
    NN_CHECK(store_, "swin: forward before bind");
    const nn::WeightStore& w = *store_;
    auto get = [&](const std::string& n) { return w.get(prefix_ + n); };
    Features f;

    // ---- patch embedding: a 4x4 stride-4 conv, zero-padded right/bottom ----
    int h = (H + 3) / 4, wd = (W + 3) / 4;
    int C = cfg_.embed_dim;
    Tensor x = nn::arena_tensor(arena, DType::F32, (int64_t)h * wd, C);
    {
        nn::vk::ArenaScope scope(arena);
        Tensor src = image;
        if (H % 4 || W % 4) {
            src = nn::arena_tensor(arena, DType::F32, (int64_t)h * 4, (int64_t)wd * 4, 3);
            nn::fill(src, 0.0f);
            nn::strided_copy(src, image, H, (int64_t)W * 3, (int64_t)W * 3,
                             (int64_t)wd * 4 * 3);
        }
        Tensor cols = nn::arena_tensor(arena, DType::F32, (int64_t)h * wd, 48);
        nn::patchify(cols, src.view(h * 4, wd * 4, 3), 4);
        nn::LinearOpts lo;
        lo.bias = get("patch.b");
        nn::linear(x, cols, get("patch.w").asMatrix(), lo);
        nn::layer_norm(x, x, get("patch_norm.w"), get("patch_norm.b"));
    }

    for (int s = 0; s < 4; ++s) {
        const int heads = cfg_.heads[s];
        int ws = cfg_.window;
        bool may_shift = true;
        if (cfg_.clamp_window && std::min(h, wd) <= ws) may_shift = false;
        const int nwh = (h + ws - 1) / ws, nww = (wd + ws - 1) / ws;
        const int nwin = nwh * nww, n = ws * ws;
        for (int b = 0; b < cfg_.depths[s]; ++b) {
            nn::vk::ArenaScope scope(arena);
            const std::string p = fmt("s%d.b%d.", s, b);
            const int shift = (b % 2 == 1 && may_shift) ? ws / 2 : 0;

            Tensor xn = nn::arena_tensor(arena, DType::F32, (int64_t)h * wd, C);
            nn::layer_norm(xn, x, get(p + "n1.w"), get(p + "n1.b"));
            Tensor xw = nn::arena_tensor(arena, DType::F32, (int64_t)nwin * n, C);
            nn::window_partition(xw, xn.view(h, wd, C), h, wd, C, ws, shift);
            Tensor qkv = nn::arena_tensor(arena, DType::F32, (int64_t)nwin * n, 3 * C);
            nn::LinearOpts lo;
            lo.bias = get(p + "qkv.b");
            nn::linear(qkv, xw, get(p + "qkv.w"), lo);

            nn::AttnOpts ao;
            ao.n_heads = heads;
            ao.head_dim = C / heads;
            ao.batch = nwin;
            ao.q_stride = ao.k_stride = ao.v_stride = 3 * C;
            ao.bias = get(p + "bias");
            ao.bias_mode = nn::AttnBias::Full;
            if (shift) {
                ao.bias_mode = nn::AttnBias::Window;
                ao.labels = labels(nwh * ws, nww * ws, ws, shift);
            }
            Tensor att = xw;   // the partitioned input is dead once qkv exists
            nn::attention(att, qkv, qkv.offsetElems(C), qkv.offsetElems(2 * C), n, n, ao);
            Tensor proj = nn::arena_tensor(arena, DType::F32, (int64_t)nwin * n, C);
            nn::LinearOpts po;
            po.bias = get(p + "proj.b");
            nn::linear(proj, att, get(p + "proj.w"), po);
            nn::window_unpartition(x.view(h, wd, C), proj, h, wd, C, ws, shift,
                                   /*accumulate=*/true);

            nn::layer_norm(xn, x, get(p + "n2.w"), get(p + "n2.b"));
            Tensor hid = nn::arena_tensor(arena, DType::F32, (int64_t)h * wd, 4 * C);
            nn::LinearOpts f1;
            f1.bias = get(p + "fc1.b");
            f1.act = nn::Act::GeluErf;
            nn::linear(hid, xn, get(p + "fc1.w"), f1);
            nn::LinearOpts f2;
            f2.bias = get(p + "fc2.b");
            f2.residual = x;
            nn::linear(x, hid, get(p + "fc2.w"), f2);
        }

        if (cfg_.out[s]) {
            // Allocated below the merge's scratch so it survives the stage.
            f.map[s] = nn::arena_tensor(arena, DType::F32, h, wd, C);
            f.h[s] = h;
            f.w[s] = wd;
            nn::layer_norm(f.map[s].view((int64_t)h * wd, C), x, get(fmt("out%d.w", s)),
                           get(fmt("out%d.b", s)));
        }
        if (s == 3) break;
        const int h2 = (h + 1) / 2, w2 = (wd + 1) / 2;
        Tensor next = nn::arena_tensor(arena, DType::F32, (int64_t)h2 * w2, 2 * C);
        {
            nn::vk::ArenaScope scope(arena);
            Tensor m = nn::arena_tensor(arena, DType::F32, (int64_t)h2 * w2, 4 * C);
            nn::patch_merge(m, x.view(h, wd, C), h, wd, C);
            nn::layer_norm(m, m, get(fmt("s%d.merge_norm.w", s)), get(fmt("s%d.merge_norm.b", s)));
            nn::linear(next, m, get(fmt("s%d.merge.w", s)));
        }
        x = next;
        h = h2;
        wd = w2;
        C *= 2;
    }
    return f;
}

uint64_t Backbone::arenaBytes(int H, int W) const {
    int64_t h = (H + 3) / 4, wd = (W + 3) / 4, C = cfg_.embed_dim;
    // Live across the pass: every stage's running x and requested output.
    int64_t live = align256((int64_t)h * wd * C * 4);
    int64_t peak = live + align256(h * 4 * wd * 4 * 3 * 4) + align256(h * wd * 48 * 4);
    for (int s = 0; s < 4; ++s) {
        const int64_t ws = cfg_.window;
        const int64_t tok = ((h + ws - 1) / ws) * ((wd + ws - 1) / ws) * ws * ws;
        // xn, xw/att, qkv, proj, hid
        const int64_t block = align256(h * wd * C * 4) + align256(tok * C * 4) +
                              align256(tok * 3 * C * 4) + align256(tok * C * 4) +
                              align256(h * wd * 4 * C * 4);
        peak = std::max(peak, live + block);
        if (cfg_.out[s]) live += align256(h * wd * C * 4);
        if (s == 3) break;
        const int64_t h2 = (h + 1) / 2, w2 = (wd + 1) / 2;
        live += align256(h2 * w2 * 2 * C * 4);
        peak = std::max(peak, live + align256(h2 * w2 * 4 * C * 4));
        h = h2;
        wd = w2;
        C *= 2;
    }
    return (uint64_t)peak + (1u << 20);
}

}  // namespace swin
