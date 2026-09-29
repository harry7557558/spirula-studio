#include "gdino/Model.h"

#include "core/Env.h"
#include "nn/Ops.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/Safetensors.h"
#include "nn/io/StageDump.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>

namespace gdino {
namespace {

using nn::DType;
using nn::Tensor;
namespace vk = nn::vk;

constexpr double kPi = 3.14159265358979323846;

std::string fmt(const char* f, int a) {
    char buf[200];
    std::snprintf(buf, sizeof buf, f, a);
    return buf;
}

bool ends_with(const std::string& s, const char* p) {
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}

// SS_GDINO_F32_WEIGHTS=1 keeps every matrix f32: for tools/gdino/compare_ort.py.
bool f32_weights() {
    static const bool on = spirula::env_on("GDINO_F32_WEIGHTS");
    return on;
}

// encode_sinusoidal_position_embedding for one coordinate: `n` features,
// interleaved sin/cos of coord * 2pi / temperature^(2*floor(i/2)/n).
void sine(float coord, int n, double temperature, float* out) {
    for (int i = 0; i < n; ++i) {
        const double dim_t = std::pow(temperature, 2.0 * (i / 2) / n);
        const double v = (double)coord * 2.0 * kPi / dim_t;
        out[i] = (float)((i % 2 == 0) ? std::sin(v) : std::cos(v));
    }
}

float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

float logit(float x, float eps) {
    x = std::min(std::max(x, eps), 1.0f - eps);
    return std::log(x / (1.0f - x));
}

// Grounding DINO's generate_masks_with_special_tokens_and_transfer_map, as the
// checkpoints were trained: each phrase and its closing separator are one block
// that sees itself, numbered 0.. through the separator; [CLS] / [SEP] stand alone.
void phrase_structure(const std::vector<int32_t>& ids, std::vector<uint8_t>& allow,
                      std::vector<int32_t>& pos) {
    const int T = (int)ids.size();
    allow.assign((size_t)T * T, 0);
    pos.assign((size_t)T, 0);
    for (int i = 0; i < T; ++i) allow[(size_t)i * T + i] = 1;
    int prev = 0;
    for (int col = 0; col < T; ++col) {
        const int32_t id = ids[(size_t)col];
        if (id != Tokenizer::kCls && id != Tokenizer::kSep && id != Tokenizer::kPeriod &&
            id != Tokenizer::kQuestion)
            continue;
        if (col != 0 && col != T - 1) {
            for (int i = prev + 1; i <= col; ++i) {
                pos[(size_t)i] = i - prev - 1;
                for (int j = prev + 1; j <= col; ++j) allow[(size_t)i * T + j] = 1;
            }
        }
        prev = col;
    }
}

}  // namespace

Model::~Model() { release(); }

void Model::release() {
    vk::Stream::get().sync();
    for (auto& kv : geo_) vk::device_free(kv.second.blob);
    geo_.clear();
    if (text_.blob) vk::device_free(text_.blob);
    text_ = Text{};
    bb_.release();
    w_.release();
    arena_.release();
    loaded_ = false;
}

// ---------------------------------------------------------------------------
// Weights
// ---------------------------------------------------------------------------

void Model::load(const std::string& weights, const std::string& vocab) {
    release();
    tok_.load(vocab);
    const nn::SafetensorsFile file(weights);
    NN_CHECK(file.has("model.text_projection.weight") &&
                 file.has("model.backbone.conv_encoder.model.embeddings.norm.weight"),
             "'%s' is not a Grounding DINO checkpoint with a Swin backbone", weights.c_str());
    const bool f16 = !f32_weights();

    // ---- backbone ----
    const std::string bbp = "model.backbone.conv_encoder.model.";
    const nn::OnnxTensor table =
        file.read(bbp + "encoder.layers.0.blocks.0.attention.self.relative_position_bias_table");
    const int window = ((int)std::lround(std::sqrt((double)table.shape[0])) + 1) / 2;
    auto has = [&](const std::string& n) { return file.has(n); };
    auto read = [&](const std::string& n) { return file.read(n); };
    swin::Config cfg = swin::infer_config(swin::Naming::HuggingFace, bbp, has, read, window);
    cfg.out[0] = false;
    swin::stage_weights(w_, "bb", cfg, swin::Naming::HuggingFace, bbp, read, f16);

    // ---- hyperparameters, from shapes ----
    hp_.enc_layers = 0;
    while (file.has(fmt("model.encoder.layers.%d.fusion_layer.text_param", hp_.enc_layers)))
        ++hp_.enc_layers;
    hp_.dec_layers = 0;
    while (file.has(fmt("model.decoder.layers.%d.fc1.weight", hp_.dec_layers))) ++hp_.dec_layers;
    hp_.text_layers = 0;
    while (file.has(fmt("model.text_backbone.encoder.layer.%d.output.dense.weight", hp_.text_layers)))
        ++hp_.text_layers;
    hp_.d_model = (int)file.entry("model.level_embed").shape[1];
    hp_.levels = (int)file.entry("model.level_embed").shape[0];
    hp_.queries = (int)file.entry("model.query_position_embeddings.weight").shape[0];
    hp_.text_dim = (int)file.entry("model.text_projection.weight").shape[1];
    NN_CHECK(hp_.d_model == 256 && hp_.levels == 4 && hp_.enc_layers > 0 && hp_.dec_layers > 0,
             "'%s': an unexpected Grounding DINO configuration", weights.c_str());
    const int64_t lp = (int64_t)hp_.levels * hp_.enc_points;
    hp_.enc_heads = (int)(file.entry("model.encoder.layers.0.deformable_layer.self_attn.attention_weights.bias").shape[0] / lp);
    hp_.dec_heads = (int)(file.entry("model.decoder.layers.0.encoder_attn.attention_weights.bias").shape[0] / lp);
    hp_.text_heads = hp_.text_dim / 64;

    // ---- everything else, fused where one GEMM can do two ----
    auto put = [&](const std::string& name, nn::OnnxTensor t) {
        const bool matrix = t.shape.size() >= 2 && ends_with(name, ".weight");
        std::string dst = name.compare(0, 6, "model.") == 0 ? name.substr(6) : name;
        w_.stage(dst, t.shape, std::move(t.data), matrix && f16);
    };
    auto stack = [&](const std::string& dst, std::initializer_list<std::string> srcs) {
        for (const char* suffix : {".weight", ".bias"}) {
            nn::OnnxTensor all;
            for (const std::string& s : srcs) {
                nn::OnnxTensor t = file.read(s + suffix);
                all.data.insert(all.data.end(), t.data.begin(), t.data.end());
                all.shape = t.shape;
            }
            all.shape[0] = (int64_t)all.data.size() / (all.shape.size() > 1 ? all.shape[1] : 1);
            put(dst + suffix, std::move(all));
        }
    };
    std::vector<std::string> fused;
    auto skip = [&](const std::string& prefix) { fused.push_back(prefix); };
    for (int i = 0; i < hp_.text_layers; ++i) {
        const std::string p = fmt("model.text_backbone.encoder.layer.%d.attention.self.", i);
        stack(p + "qkv", {p + "query", p + "key", p + "value"});
        skip(p + "query."), skip(p + "key."), skip(p + "value.");
    }
    for (int i = 0; i < hp_.enc_layers; ++i) {
        const std::string p = fmt("model.encoder.layers.%d.text_enhancer_layer.self_attn.", i);
        stack(p + "qk", {p + "query", p + "key"});
        skip(p + "query."), skip(p + "key.");
        // vision_param / text_param scale the fusion's output projections.
        for (const char* side : {"vision", "text"}) {
            const std::string f = fmt("model.encoder.layers.%d.fusion_layer.", i);
            const std::vector<float> g = file.read(f + side + "_param").data;
            nn::OnnxTensor wt = file.read(f + "attn.out_" + side + "_proj.weight");
            nn::OnnxTensor bt = file.read(f + "attn.out_" + side + "_proj.bias");
            const int64_t in = wt.shape[1];
            for (size_t o = 0; o < g.size(); ++o) {
                for (int64_t k = 0; k < in; ++k) wt.data[o * in + k] *= g[o];
                bt.data[o] *= g[o];
            }
            put(f + "attn.out_" + side + "_proj.weight", std::move(wt));
            put(f + "attn.out_" + side + "_proj.bias", std::move(bt));
            skip(f + "attn.out_" + side + "_proj."), skip(f + side + "_param");
        }
    }
    for (int i = 0; i < hp_.dec_layers; ++i) {
        const std::string p = fmt("model.decoder.layers.%d.self_attn.", i);
        stack(p + "qk", {p + "query", p + "key"});
        skip(p + "query."), skip(p + "key.");
    }
    for (const std::string& n : file.names()) {
        if (n.compare(0, bbp.size(), bbp) == 0 || file.entry(n).dtype == "I64") continue;
        bool is_fused = false;
        for (const std::string& f : fused) is_fused |= n.compare(0, f.size(), f) == 0;
        if (is_fused) continue;
        // decoder_bbox_embed_share ties every layer's head to one, which the
        // checkpoints store as bbox_embed.0 or model.decoder.bbox_embed.0.
        std::string name = n;
        if (name.compare(0, 25, "model.decoder.bbox_embed.") == 0) name = name.substr(14);
        if (name.compare(0, 11, "bbox_embed.") == 0 && name.compare(0, 13, "bbox_embed.0.") != 0)
            continue;
        if (name != n && file.has(name)) continue;
        put(name, file.read(n));
    }
    w_.upload("gdino-weights");
    bb_.bind(w_, "bb", cfg);
    loaded_ = true;
    NN_LOG_INFO("[gdino] %s: swin dim %d depths %d/%d/%d/%d window %d, %d+%d layers, "
                "%.1f MB on device\n",
                weights.c_str(), cfg.embed_dim, cfg.depths[0], cfg.depths[1], cfg.depths[2],
                cfg.depths[3], cfg.window, hp_.enc_layers, hp_.dec_layers,
                (double)w_.deviceBytes() / 1e6);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void Model::linear(const Tensor& out, const Tensor& x, const std::string& name, nn::Act act,
                   const Tensor& residual) {
    nn::LinearOpts o;
    if (w_.has(name + ".bias")) o.bias = get(name + ".bias");
    o.act = act;
    o.residual = residual;
    nn::linear(out, x, get(name + ".weight").asMatrix(), o);
}

void Model::layer_norm(const Tensor& out, const Tensor& x, const std::string& name,
                       const Tensor& residual, float eps) {
    nn::layer_norm(out, x, get(name + ".weight"), get(name + ".bias"), eps, residual);
}

// GroundingDinoMLPPredictionHead: ReLU between the layers, none after the last.
void Model::mlp(const Tensor& out, const Tensor& x, const std::string& name, int layers) {
    vk::ArenaScope scope(arena_);
    Tensor cur = x;
    for (int i = 0; i < layers; ++i) {
        const bool last = i == layers - 1;
        const Tensor wt = get(fmt((name + ".layers.%d.weight").c_str(), i));
        Tensor next = last ? out : nn::arena_tensor(arena_, DType::F32, x.rows(), wt.shape[0]);
        linear(next, cur, fmt((name + ".layers.%d").c_str(), i),
               last ? nn::Act::None : nn::Act::Relu);
        cur = next;
    }
}

const Model::Geometry& Model::geometry(const Levels& lv) {
    const auto key = std::make_pair(lv.h[0], lv.w[0]);
    auto it = geo_.find(key);
    if (it != geo_.end()) return it->second;
    const int D = hp_.d_model;
    const int64_t N = lv.tokens();
    std::vector<float> level_embed((size_t)hp_.levels * D);
    nn::tensor_to_host(get("level_embed"), level_embed.data(), (int64_t)level_embed.size());

    std::vector<float> pos((size_t)N * D), refs((size_t)N * 2), valid((size_t)N);
    Geometry g;
    g.proposals.resize((size_t)N * 4);
    int64_t row = 0;
    const int half = D / 2;
    std::vector<float> sy(half), sx(half);
    for (int l = 0; l < lv.n; ++l) {
        const int H = lv.h[l], W = lv.w[l];
        for (int y = 0; y < H; ++y) {
            // GroundingDinoSinePositionEmbedding: the cumulative mask sum,
            // normalized by the last row, times 2pi, at temperature 20.
            const float ye = (float)((y + 1) / (H + 1e-6) * 2.0 * kPi);
            for (int i = 0; i < half; ++i) {
                const double dim_t = std::pow(20.0, 2.0 * (i / 2) / half);
                sy[(size_t)i] = (float)((i % 2 == 0) ? std::sin(ye / dim_t) : std::cos(ye / dim_t));
            }
            for (int x = 0; x < W; ++x, ++row) {
                const float xe = (float)((x + 1) / (W + 1e-6) * 2.0 * kPi);
                float* p = &pos[(size_t)row * D];
                for (int i = 0; i < half; ++i) {
                    const double dim_t = std::pow(20.0, 2.0 * (i / 2) / half);
                    p[i] = sy[(size_t)i] + level_embed[(size_t)l * D + i];
                    p[half + i] = (float)((i % 2 == 0) ? std::sin(xe / dim_t) : std::cos(xe / dim_t)) +
                                  level_embed[(size_t)l * D + half + i];
                }
                refs[(size_t)row * 2] = (x + 0.5f) / W;
                refs[(size_t)row * 2 + 1] = (y + 0.5f) / H;
                const float c[4] = {(x + 0.5f) / W, (y + 0.5f) / H, 0.05f * (float)(1 << l),
                                    0.05f * (float)(1 << l)};
                bool ok = true;
                for (float v : c) ok &= v > 0.01f && v < 0.99f;
                valid[(size_t)row] = ok ? 1.0f : 0.0f;
                for (int k = 0; k < 4; ++k)
                    g.proposals[(size_t)row * 4 + k] =
                        ok ? std::log(c[k] / (1.0f - c[k])) : INFINITY;
            }
        }
    }
    const uint64_t bytes = (uint64_t)N * (D + 2 + 1) * 4 + 1024;
    g.blob = vk::device_alloc(bytes, "gdino-geometry");
    g.pos = Tensor(g.blob, DType::F32, N, D);
    g.refs = Tensor(g.blob + (uint64_t)N * D * 4, DType::F32, N, 2);
    const uint64_t voff = ((uint64_t)N * (D + 2) * 4 + 255) / 256 * 256;
    g.valid = Tensor(g.blob + voff, DType::F32, N);
    NN_CHECK(voff + (uint64_t)N * 4 <= bytes + 1024, "gdino: geometry blob overflow");
    nn::tensor_from_host(g.pos, pos.data(), (int64_t)pos.size());
    nn::tensor_from_host(g.refs, refs.data(), (int64_t)refs.size());
    nn::tensor_from_host(g.valid, valid.data(), (int64_t)valid.size());
    return geo_.emplace(key, std::move(g)).first->second;
}

// ---------------------------------------------------------------------------
// Text tower
// ---------------------------------------------------------------------------

const Model::Text& Model::text(const std::vector<int32_t>& ids) {
    if (text_.blob && text_.ids == ids) return text_;
    if (text_.blob) vk::device_free(text_.blob);
    text_ = Text{};
    text_.ids = ids;
    encode_text(text_);
    return text_;
}

void Model::encode_text(Text& t) {
    const int T = (int)t.ids.size();
    const int D = hp_.d_model, E = hp_.text_dim, heads = hp_.text_heads;
    NN_CHECK(T >= 2 && T <= hp_.max_text, "gdino: %d tokens; the text tower takes 2..%d", T,
             hp_.max_text);
    std::vector<uint8_t> allow;
    std::vector<int32_t> pos_ids;
    phrase_structure(t.ids, allow, pos_ids);

    const int eh = hp_.enc_heads / 2;
    const uint64_t n_feat = (uint64_t)T * D, n_mask4 = (uint64_t)eh * T * T, n_pos = (uint64_t)T * D;
    t.blob = vk::device_alloc((n_feat + n_mask4 + n_pos) * 4 + 1024, "gdino-text");
    t.feat = Tensor(t.blob, DType::F32, T, D);
    t.mask4 = Tensor(t.blob + n_feat * 4, DType::F32, eh, T, T);
    t.pos = Tensor(t.blob + (n_feat + n_mask4) * 4, DType::F32, T, D);

    std::vector<float> mask((size_t)heads * T * T), mask4((size_t)eh * T * T);
    for (int h = 0; h < std::max(heads, eh); ++h)
        for (int k = 0; k < T * T; ++k) {
            const float v = allow[(size_t)k] ? 0.0f : -1e9f;
            if (h < heads) mask[(size_t)h * T * T + k] = v;
            if (h < eh) mask4[(size_t)h * T * T + k] = v;
        }
    nn::tensor_from_host(t.mask4, mask4.data(), (int64_t)mask4.size());
    std::vector<float> tpos((size_t)T * D);
    for (int i = 0; i < T; ++i) sine((float)pos_ids[(size_t)i], D, 10000.0, &tpos[(size_t)i * D]);
    nn::tensor_from_host(t.pos, tpos.data(), (int64_t)tpos.size());

    arena_.reserve(planArena(64, 64, T));
    vk::ArenaScope scope(arena_);
    Tensor ids = nn::arena_tensor(arena_, DType::I32, T);
    vk::Stream::get().upload(ids.ptr, t.ids.data(), (size_t)T * 4);
    Tensor pids = nn::arena_tensor(arena_, DType::I32, T);
    vk::Stream::get().upload(pids.ptr, pos_ids.data(), (size_t)T * 4);
    Tensor tmask = nn::arena_tensor(arena_, DType::F32, (int64_t)heads * T * T);
    nn::tensor_from_host(tmask, mask.data(), (int64_t)mask.size());

    // BertEmbeddings: word + position + token type 0, then LayerNorm.
    Tensor x = nn::arena_tensor(arena_, DType::F32, T, E);
    Tensor tmp = nn::arena_tensor(arena_, DType::F32, T, E);
    nn::gather_rows(x, get("text_backbone.embeddings.word_embeddings.weight"), ids);
    nn::gather_rows(tmp, get("text_backbone.embeddings.position_embeddings.weight"), pids);
    nn::add(x, x, tmp);
    Tensor type0 = nn::arena_tensor(arena_, DType::F32, E);
    nn::copy(type0, get("text_backbone.embeddings.token_type_embeddings.weight").view(E));
    nn::add(x, x, type0);
    layer_norm(x, x, "text_backbone.embeddings.LayerNorm", {}, 1e-12f);
    static const nn::StageDump dump("GDINO_DUMP");
    if (dump.on()) dump.tensor("text_emb", x, {1, T, E});

    Tensor qkv = nn::arena_tensor(arena_, DType::F32, T, 3 * E);
    Tensor att = nn::arena_tensor(arena_, DType::F32, T, E);
    Tensor hid = nn::arena_tensor(arena_, DType::F32, T,
                                  get("text_backbone.encoder.layer.0.intermediate.dense.weight").shape[0]);
    for (int i = 0; i < hp_.text_layers; ++i) {
        const std::string p = fmt("text_backbone.encoder.layer.%d.", i);
        linear(qkv, x, p + "attention.self.qkv");
        nn::AttnOpts ao;
        ao.n_heads = heads;
        ao.head_dim = E / heads;
        ao.q_stride = ao.k_stride = ao.v_stride = 3 * E;
        ao.bias_mode = nn::AttnBias::Full;
        ao.bias = tmask;
        nn::attention(att, qkv, qkv.offsetElems(E), qkv.offsetElems(2 * E), T, T, ao);
        linear(tmp, att, p + "attention.output.dense", nn::Act::None, x);
        layer_norm(x, tmp, p + "attention.output.LayerNorm", {}, 1e-12f);
        linear(hid, x, p + "intermediate.dense", nn::Act::GeluErf);
        linear(tmp, hid, p + "output.dense", nn::Act::None, x);
        layer_norm(x, tmp, p + "output.LayerNorm", {}, 1e-12f);
        if (dump.on() && i == 0) dump.tensor("text_l0", x, {1, T, E});
    }
    linear(t.feat, x, "text_projection");
    vk::Stream::get().sync();
}

// ---------------------------------------------------------------------------
// The image
// ---------------------------------------------------------------------------

uint64_t Model::planArena(int H, int W, int T) const {
    const int64_t h1 = (H + 7) / 8, w1 = (W + 7) / 8;
    int64_t N = 0;
    for (int l = 0; l < 4; ++l) N += ((h1 + (1 << l) - 1) >> l) * ((w1 + (1 << l) - 1) >> l);
    const int64_t D = hp_.d_model;
    // vis, pos-added query, value, offsets, weights, deform out, ffn hidden
    // (2048), the fusion's three 1024-wide maps, and the decoder's value map.
    const int64_t enc = N * 4 * (D * 6 + 2048 + 1024 * 3 + D) + (int64_t)T * 1024 * 16;
    const int64_t text = (int64_t)T * 4 * (hp_.text_dim * 8 + 3072) + (int64_t)T * T * 4 * 16;
    const uint64_t bb = bb_.bound() ? bb_.arenaBytes(H, W) : 0;
    return bb + (uint64_t)(enc + text) + (64ull << 20);
}

HeadOutput Model::run(const std::vector<float>& image, int H, int W,
                      const std::vector<int32_t>& ids) {
    NN_CHECK(loaded_, "gdino: no model loaded");
    static const nn::StageDump dump("GDINO_DUMP");
    const Text& tx = text(ids);
    const int T = (int)ids.size();
    const int D = hp_.d_model;

    arena_.reserve(planArena(H, W, T));
    const uint64_t cap = arena_.capacity();
    HeadOutput out;
    {
        vk::ArenaScope scope(arena_);
        Tensor img = nn::arena_tensor(arena_, DType::F32, H, W, 3, 1, 3);
        nn::tensor_from_host(img, image.data(), (int64_t)image.size());

        // ---- backbone and input projections ------------------------------
        swin::Features f = bb_.forward(arena_, img, H, W);
        Levels lv;
        lv.n = hp_.levels;
        for (int l = 0; l < 3; ++l) { lv.h[l] = f.h[l + 1]; lv.w[l] = f.w[l + 1]; }
        lv.h[3] = (lv.h[2] - 1) / 2 + 1;
        lv.w[3] = (lv.w[2] - 1) / 2 + 1;
        const int64_t N = lv.tokens();
        nn::MsDeformLevels ml;
        ml.n = lv.n;
        for (int l = 0; l < lv.n; ++l) { ml.h[l] = lv.h[l]; ml.w[l] = lv.w[l]; }
        const Geometry& geo = geometry(lv);

        Tensor vis = nn::arena_tensor(arena_, DType::F32, N, D);
        {
            int64_t row = 0;
            for (int l = 0; l < lv.n; ++l) {
                const int64_t n = (int64_t)lv.h[l] * lv.w[l];
                Tensor dst = vis.offsetElems(row * D).view(lv.h[l], lv.w[l], D);
                const std::string p = fmt("input_proj_vision.%d", l);
                nn::ConvOpts o;
                o.bias = get(p + ".0.bias");
                if (l < 3) {
                    nn::conv2d(arena_, dst, f.map[l + 1], get(p + ".0.weight"), 1, 1, o);
                } else {
                    o.stride_y = o.stride_x = 2;
                    o.pad_y = o.pad_x = 1;
                    nn::conv2d(arena_, dst, f.map[3], get(p + ".0.weight"), 3, 3, o);
                }
                nn::group_norm(arena_, dst.view(n, D), dst.view(n, D), get(p + ".1.weight"),
                               get(p + ".1.bias"), 32, 1e-5f);
                row += n;
            }
        }
        if (dump.on()) dump.tensor("sources", vis, {N, D});

        // ---- encoder ------------------------------------------------------
        Tensor text = nn::arena_tensor(arena_, DType::F32, T, D);
        nn::copy(text, tx.feat);
        if (dump.on()) dump.tensor("text_feat", text, {1, T, D});
        {
            vk::ArenaScope sc(arena_);
            const int fh = 4, fd = 1024 / fh;   // the fusion's 4 heads of 256
            Tensor vln = nn::arena_tensor(arena_, DType::F32, N, D);
            Tensor tln = nn::arena_tensor(arena_, DType::F32, T, D);
            Tensor vq = nn::arena_tensor(arena_, DType::F32, N, 1024);
            Tensor vv = nn::arena_tensor(arena_, DType::F32, N, 1024);
            Tensor vo = nn::arena_tensor(arena_, DType::F32, N, 1024);
            Tensor tk = nn::arena_tensor(arena_, DType::F32, T, 1024);
            Tensor tv = nn::arena_tensor(arena_, DType::F32, T, 1024);
            Tensor to = nn::arena_tensor(arena_, DType::F32, T, 1024);
            Tensor tq = nn::arena_tensor(arena_, DType::F32, T, D);
            Tensor tqk = nn::arena_tensor(arena_, DType::F32, T, 2 * D);
            Tensor tval = nn::arena_tensor(arena_, DType::F32, T, D);
            Tensor tatt = nn::arena_tensor(arena_, DType::F32, T, D);
            Tensor thid = nn::arena_tensor(arena_, DType::F32, T, 1024);
            Tensor q = nn::arena_tensor(arena_, DType::F32, N, D);
            Tensor val = nn::arena_tensor(arena_, DType::F32, N, D);
            const int lp = hp_.levels * hp_.enc_points;
            Tensor off = nn::arena_tensor(arena_, DType::F32, N, (int64_t)hp_.enc_heads * lp * 2);
            Tensor aw = nn::arena_tensor(arena_, DType::F32, N, (int64_t)hp_.enc_heads * lp);
            Tensor hid = nn::arena_tensor(arena_, DType::F32, N,
                                          get("encoder.layers.0.deformable_layer.fc1.weight").shape[0]);
            for (int i = 0; i < hp_.enc_layers; ++i) {
                const std::string p = fmt("encoder.layers.%d.", i);
                // Fusion: bi-directional cross attention between the two.
                const std::string fu = p + "fusion_layer.";
                layer_norm(vln, vis, fu + "layer_norm_vision");
                layer_norm(tln, text, fu + "layer_norm_text");
                linear(vq, vln, fu + "attn.vision_proj");
                linear(vv, vln, fu + "attn.values_vision_proj");
                linear(tk, tln, fu + "attn.text_proj");
                linear(tv, tln, fu + "attn.values_text_proj");
                nn::AttnOpts ao;
                ao.n_heads = fh;
                ao.head_dim = fd;
                ao.scale = 1.0f / std::sqrt((float)fd);
                nn::attention(vo, vq, tk, tv, N, T, ao);
                ao.arena = &arena_;
                nn::attention(to, tk, vq, vv, T, N, ao);
                linear(vis, vo, fu + "attn.out_vision_proj", nn::Act::None, vln);
                linear(text, to, fu + "attn.out_text_proj", nn::Act::None, tln);

                // Text enhancer: self attention inside each phrase.
                const std::string te = p + "text_enhancer_layer.";
                nn::add(tq, text, tx.pos);
                linear(tqk, tq, te + "self_attn.qk");
                linear(tval, text, te + "self_attn.value");
                nn::AttnOpts to_;
                to_.n_heads = hp_.enc_heads / 2;
                to_.head_dim = D / to_.n_heads;
                to_.q_stride = to_.k_stride = 2 * D;
                to_.bias_mode = nn::AttnBias::Full;
                to_.bias = tx.mask4;
                nn::attention(tatt, tqk, tqk.offsetElems(D), tval, T, T, to_);
                linear(tq, tatt, te + "self_attn.out_proj", nn::Act::None, text);
                layer_norm(text, tq, te + "layer_norm_before");
                linear(thid, text, te + "fc1", nn::Act::Relu);
                linear(tq, thid, te + "fc2", nn::Act::None, text);
                layer_norm(text, tq, te + "layer_norm_after");

                // Deformable self attention over the four levels.
                const std::string de = p + "deformable_layer.";
                nn::add(q, vis, geo.pos);
                linear(val, vis, de + "self_attn.value_proj");
                linear(off, q, de + "self_attn.sampling_offsets");
                linear(aw, q, de + "self_attn.attention_weights");
                nn::ms_deform_attn(vln, val, off, aw, geo.refs, ml, hp_.enc_heads, hp_.enc_points);
                linear(q, vln, de + "self_attn.output_proj", nn::Act::None, vis);
                layer_norm(vis, q, de + "self_attn_layer_norm");
                linear(hid, vis, de + "fc1", nn::Act::Relu);
                linear(q, hid, de + "fc2", nn::Act::None, vis);
                layer_norm(vis, q, de + "final_layer_norm");
                if (dump.on() && i == 0) {
                    dump.tensor("enc0_vision", vis, {1, N, D});
                    dump.tensor("enc0_text", text, {1, T, D});
                }
            }
        }
        if (dump.on()) {
            dump.tensor("enc_vision", vis, {N, D});
            dump.tensor("enc_text", text, {T, D});
        }

        // ---- two-stage query selection -----------------------------------
        std::vector<float> refs((size_t)hp_.queries * 4);
        {
            vk::ArenaScope sc(arena_);
            Tensor oq = nn::arena_tensor(arena_, DType::F32, N, D);
            nn::mul_rows(oq, vis, geo.valid);
            Tensor tmp = nn::arena_tensor(arena_, DType::F32, N, D);
            linear(tmp, oq, "enc_output");
            layer_norm(oq, tmp, "enc_output_norm");
            Tensor cls = nn::arena_tensor(arena_, DType::F32, N, T);
            nn::matmul_nt(cls, oq, text);
            Tensor delta = nn::arena_tensor(arena_, DType::F32, N, 4);
            mlp(delta, oq, "encoder_output_bbox_embed", 3);
            std::vector<float> hc((size_t)N * T), hd((size_t)N * 4);
            nn::tensor_to_host(cls, hc.data(), (int64_t)hc.size());
            nn::tensor_to_host(delta, hd.data(), (int64_t)hd.size());
            std::vector<float> best((size_t)N);
            for (int64_t i = 0; i < N; ++i)
                best[(size_t)i] = *std::max_element(&hc[(size_t)i * T], &hc[(size_t)i * T] + T);
            std::vector<int64_t> order((size_t)N);
            std::iota(order.begin(), order.end(), 0);
            const int K = (int)std::min<int64_t>(hp_.queries, N);
            std::partial_sort(order.begin(), order.begin() + K, order.end(),
                              [&](int64_t a, int64_t b) {
                                  return best[(size_t)a] > best[(size_t)b] ||
                                         (best[(size_t)a] == best[(size_t)b] && a < b);
                              });
            for (int k = 0; k < hp_.queries; ++k) {
                const int64_t i = order[(size_t)std::min(k, K - 1)];
                for (int c = 0; c < 4; ++c)
                    refs[(size_t)k * 4 + c] =
                        sigmoid(hd[(size_t)i * 4 + c] + geo.proposals[(size_t)i * 4 + c]);
            }
        }

        // ---- decoder ------------------------------------------------------
        const int Q = hp_.queries;
        Tensor h = nn::arena_tensor(arena_, DType::F32, Q, D);
        nn::copy(h, get("query_position_embeddings.weight"));
        {
            vk::ArenaScope sc(arena_);
            Tensor qpos_in = nn::arena_tensor(arena_, DType::F32, Q, 2 * D);
            Tensor qpos = nn::arena_tensor(arena_, DType::F32, Q, D);
            Tensor rt = nn::arena_tensor(arena_, DType::F32, Q, 4);
            Tensor q = nn::arena_tensor(arena_, DType::F32, Q, D);
            Tensor qk = nn::arena_tensor(arena_, DType::F32, Q, 2 * D);
            Tensor v = nn::arena_tensor(arena_, DType::F32, Q, D);
            Tensor a = nn::arena_tensor(arena_, DType::F32, Q, D);
            Tensor tk = nn::arena_tensor(arena_, DType::F32, T, D);
            Tensor tv = nn::arena_tensor(arena_, DType::F32, T, D);
            Tensor val = nn::arena_tensor(arena_, DType::F32, N, D);
            const int lp = hp_.levels * hp_.dec_points;
            Tensor off = nn::arena_tensor(arena_, DType::F32, Q, (int64_t)hp_.dec_heads * lp * 2);
            Tensor aw = nn::arena_tensor(arena_, DType::F32, Q, (int64_t)hp_.dec_heads * lp);
            Tensor hid = nn::arena_tensor(arena_, DType::F32, Q,
                                          get("decoder.layers.0.fc1.weight").shape[0]);
            Tensor delta = nn::arena_tensor(arena_, DType::F32, Q, 4);
            std::vector<float> sine_in((size_t)Q * 2 * D), hdelta((size_t)Q * 4);
            const int hd = D / 2;
            for (int i = 0; i < hp_.dec_layers; ++i) {
                const std::string p = fmt("decoder.layers.%d.", i);
                // The query position is the sine of the current box, [y, x, w, h].
                for (int k = 0; k < Q; ++k) {
                    const float* r = &refs[(size_t)k * 4];
                    float* s = &sine_in[(size_t)k * 2 * D];
                    sine(r[1], hd, 10000.0, s);
                    sine(r[0], hd, 10000.0, s + hd);
                    sine(r[2], hd, 10000.0, s + 2 * hd);
                    sine(r[3], hd, 10000.0, s + 3 * hd);
                }
                nn::tensor_from_host(qpos_in, sine_in.data(), (int64_t)sine_in.size());
                nn::tensor_from_host(rt, refs.data(), (int64_t)refs.size());
                mlp(qpos, qpos_in, "decoder.reference_points_head", 2);

                nn::add(q, h, qpos);
                linear(qk, q, p + "self_attn.qk");
                linear(v, h, p + "self_attn.value");
                nn::AttnOpts ao;
                ao.n_heads = hp_.dec_heads;
                ao.head_dim = D / hp_.dec_heads;
                ao.q_stride = ao.k_stride = 2 * D;
                nn::attention(a, qk, qk.offsetElems(D), v, Q, Q, ao);
                linear(q, a, p + "self_attn.out_proj", nn::Act::None, h);
                layer_norm(h, q, p + "self_attn_layer_norm");

                nn::add(q, h, qpos);
                linear(v, q, p + "encoder_attn_text.query");
                linear(tk, text, p + "encoder_attn_text.key");
                linear(tv, text, p + "encoder_attn_text.value");
                nn::AttnOpts ct;
                ct.n_heads = hp_.dec_heads;
                ct.head_dim = D / hp_.dec_heads;
                nn::attention(a, v, tk, tv, Q, T, ct);
                linear(q, a, p + "encoder_attn_text.out_proj", nn::Act::None, h);
                layer_norm(h, q, p + "encoder_attn_text_layer_norm");

                nn::add(q, h, qpos);
                linear(val, vis, p + "encoder_attn.value_proj");
                linear(off, q, p + "encoder_attn.sampling_offsets");
                linear(aw, q, p + "encoder_attn.attention_weights");
                nn::ms_deform_attn(a, val, off, aw, rt, ml, hp_.dec_heads, hp_.dec_points);
                linear(q, a, p + "encoder_attn.output_proj", nn::Act::None, h);
                layer_norm(h, q, p + "encoder_attn_layer_norm");

                linear(hid, h, p + "fc1", nn::Act::Relu);
                linear(q, hid, p + "fc2", nn::Act::None, h);
                layer_norm(h, q, p + "final_layer_norm");

                if (i + 1 < hp_.dec_layers) {
                    // Iterative refinement, on the un-normed state.
                    mlp(delta, h, "bbox_embed.0", 3);
                    nn::tensor_to_host(delta, hdelta.data(), (int64_t)hdelta.size());
                    for (size_t k = 0; k < refs.size(); ++k)
                        refs[k] = sigmoid(hdelta[k] + logit(refs[k], 1e-5f));
                }
            }

            // ---- head: on the normed state, against the encoder's text ----
            layer_norm(q, h, "decoder.layer_norm");
            Tensor lg = nn::arena_tensor(arena_, DType::F32, Q, T);
            nn::matmul_nt(lg, q, text);
            mlp(delta, q, "bbox_embed.0", 3);
            out.queries = Q;
            out.tokens = T;
            out.logits.resize((size_t)Q * T);
            nn::tensor_to_host(lg, out.logits.data(), (int64_t)out.logits.size());
            nn::tensor_to_host(delta, hdelta.data(), (int64_t)hdelta.size());
            out.boxes.resize((size_t)Q * 4);
            for (size_t k = 0; k < out.boxes.size(); ++k)
                out.boxes[k] = sigmoid(hdelta[k] + logit(refs[k], 1e-5f));
        }
    }
    NN_CHECK(arena_.capacity() == cap,
             "gdino: the arena grew from %.0f to %.0f MB mid-pass; planArena is wrong",
             (double)cap / 1e6, (double)arena_.capacity() / 1e6);
    NN_LOG_INFO("[gdino] arena peak %.0f of %.0f MB\n", (double)arena_.highWater() / 1e6,
                (double)cap / 1e6);
    return out;
}

}  // namespace gdino
