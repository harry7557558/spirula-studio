// FlexGlue's forward pass next to its PyTorch reference (cvg/FlexGlue,
// descriptor.py, flexglue.py, utils/modules.py): the DINOv3 ConvNeXt-B
// descriptor, then the sparse matcher of onnx/export.py's SparseMatcherONNX
// with COLMAP's early-stop checks. Weights come from flexglue.pth by name.

#include "flexglue/FlexGlue.h"

#include "flexglue/model/Fetch.h"
#include "nn/Blocks.h"
#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/WeightStore.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/StageDump.h"
#include "nn/io/TorchCheckpoint.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include "core/Env.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

NN_DECLARE_EMBEDDED_MODULES(flexglue)

namespace flexglue {

namespace {

// x after layer 1's self block, per live image, least recently used last.
// Entries are only freed by trim() and drop(), both called with the GPU idle.
struct SelfCache {
    std::mutex mu;
    int capacity = 64;
    std::list<std::pair<uint64_t, nn::DevicePtr>> lru;
    std::unordered_map<uint64_t, std::list<std::pair<uint64_t, nn::DevicePtr>>::iterator> at;
    Model::SelfCacheStats stats;

    nn::DevicePtr find(uint64_t id) {
        std::lock_guard<std::mutex> l(mu);
        if (capacity <= 0) return 0;
        auto it = at.find(id);
        if (it == at.end()) {
            ++stats.misses;
            return 0;
        }
        ++stats.hits;
        lru.splice(lru.begin(), lru, it->second);
        return it->second->second;
    }
    // Never evicts, so a pointer find() returned this pair stays valid.
    nn::DevicePtr insert(uint64_t id, uint64_t bytes) {
        std::lock_guard<std::mutex> l(mu);
        if (auto it = at.find(id); it != at.end()) return it->second->second;
        lru.emplace_front(id, nn::vk::device_alloc(bytes, "flexglue-self1"));
        at[id] = lru.begin();
        return lru.front().second;
    }
    bool overfull() {
        std::lock_guard<std::mutex> l(mu);
        return (int)lru.size() > std::max(capacity, 0);
    }
    void trim() {
        std::lock_guard<std::mutex> l(mu);
        while ((int)lru.size() > std::max(capacity, 0)) {
            nn::vk::device_free(lru.back().second);
            at.erase(lru.back().first);
            lru.pop_back();
            ++stats.evictions;
        }
    }
    void drop(uint64_t id) {
        std::lock_guard<std::mutex> l(mu);
        auto it = at.find(id);
        if (it == at.end()) return;
        nn::vk::device_free(it->second->second);
        lru.erase(it->second);
        at.erase(it);
    }
    ~SelfCache() {
        for (auto& e : lru) nn::vk::device_free(e.second);
    }
};

std::atomic<uint64_t> g_next_id{1};

}  // namespace

struct DenseFeatures {
    nn::DevicePtr ptr = 0;
    uint64_t id = 0;   // never reused, so a dead image cannot alias a live one's entry
    std::weak_ptr<SelfCache> self;
    ~DenseFeatures() {
        if (auto c = self.lock()) c->drop(id);
        if (ptr) nn::vk::device_free(ptr);
    }
};

namespace {

namespace vk = ::nn::vk;
using nn::Act;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

constexpr int kSize = 512;          // descriptor input, squashed
constexpr int kGrid = kSize / 8;    // 64x64 stride-8 feature map
constexpr int kTokens = kGrid * kGrid;
constexpr int kFeat = 256;          // features channels
constexpr int kDim = 512;           // matcher width
constexpr int kHeads = 8;
constexpr int kHeadDim = kDim / kHeads;
constexpr int kSearch = 64;
constexpr int kLayers = 9;
constexpr int kStageDims[4] = {128, 256, 512, 1024};
constexpr int kStageDepths[4] = {3, 3, 27, 3};
constexpr float kBackboneEps = 1e-6f;   // DINOv3's LayerNorm2d(d, 1e-6)
constexpr float kEps = 1e-5f;           // every other LayerNorm
constexpr float kMean[3] = {0.485f, 0.456f, 0.406f};
constexpr float kStd[3] = {0.229f, 0.224f, 0.225f};
constexpr int64_t kQueryChunk = 4096;   // the reference's query_chunk_size

struct ReadoutParams {
    uint64_t logits, pos, mass, cell;
    uint32_t R, stride, h, w, groups_per_row;
};
struct OffsetParams {
    uint64_t head, z, mass, pos, conf;
    uint32_t R, h, w, groups_per_row;
};
struct NearestParams {
    uint64_t x, y, out;
    uint32_t M, N, groups_per_row;
};

std::string fmt(const char* f, int a, int b = 0) {
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

}  // namespace

struct Model::Impl {
    nn::WeightStore w;
    vk::Arena arena{"flexglue"};
    nn::StageDump dump{"FLEXGLUE_DUMP"};
    std::string prefix;       // stage dumps
    std::string out_prefix;   // the final outputs: "colmap." or "nostop."
    bool stages = true;
    std::vector<float> rope_proj;   // [4096, 32], Wr . cell centre
    // SS_FLEXGLUE_PROFILE=1: syncs around attention and the readout and
    // prints where a pair's time went.
    bool profile = spirula::env_on("FLEXGLUE_PROFILE");
    double t_attn = 0, t_head = 0;
    float temperature = 0.0f;
    std::vector<float> rope;   // [4096, 32, 2] (cos, sin)
    bool loaded = false;
    std::shared_ptr<SelfCache> self_cache = std::make_shared<SelfCache>();

    Tensor g(const std::string& n) const { return w.get(n); }

    void load(const std::string& path);
    Tensor encode(const Tensor& img);   // [512, 512, 3] normalized -> [64, 64, 256]
    MatchResult match(const DenseFeatures& f0, const Keypoints& k0, const DenseFeatures& f1,
                      const Keypoints& k1, const MatchOptions& o);

    void dumpT(const std::string& name, const Tensor& t) const {
        if (!dump.on() || !stages) return;
        std::vector<int64_t> shape(t.shape, t.shape + t.ndim);
        dump.tensor((prefix + name).c_str(), t, shape);
    }

    // x[n, d] <- x + ffn(cat(x, LN(message))).
    void ffn(const Tensor& x, const Tensor& message, int64_t n, const std::string& b);
    void attend(const Tensor& ctx, const Tensor& q, const Tensor& k, const Tensor& v);
    void selfBlock(const Tensor& x, const Tensor& freqs, const std::string& b);
    void crossBlock(const Tensor& x0, const Tensor& x1, const std::string& b);
    float covisibility(const Tensor& x0, const Tensor& x1, int layer);
    // One direction i -> j of heads[layer]: normalized predictions and confidence.
    void correspond(const Tensor& cf_i, const Tensor& cf_j, const std::vector<float>& kn,
                    int layer, std::vector<float>& p2d, std::vector<float>& conf, int dir);
};

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

void Model::Impl::load(const std::string& path) {
    const nn::TorchCheckpoint f(path);
    // f32 weights: without tensor cores f16 measured slower here (attention
    // dominates) and cost the descriptor 4e-4 relative L2.
    const bool half = false;
    auto take = [&](const std::string& name, bool matrix = false,
                    std::vector<int64_t> shape = {}) {
        if (!f.has(name)) nn::fail("'%s' has no tensor '%s'; is this flexglue.pth?",
                                   path.c_str(), name.c_str());
        nn::OnnxTensor t = f.read(name);
        if (shape.empty()) shape = t.shape;
        int64_t want = 1, got = 1;
        for (int64_t d : shape) want *= d;
        for (int64_t d : t.shape) got *= d;
        NN_CHECK(want == got, "'%s': '%s' has %lld values, expected %lld", path.c_str(),
                 name.c_str(), (long long)got, (long long)want);
        w.stage(name, shape, std::move(t.data), half && matrix);
    };
    auto linear = [&](const std::string& p, bool bias = true) {
        const std::vector<int64_t> s = f.entry(p + ".weight").shape;
        take(p + ".weight", true, {s[0], s.size() == 4 ? s[1] * s[2] * s[3] : s[1]});
        if (bias) take(p + ".bias");
    };
    auto norm = [&](const std::string& p) {
        take(p + ".weight");
        take(p + ".bias");
    };

    const std::string bb = "extractor.backbone.";
    linear(bb + "downsample_layers.0.0");
    norm(bb + "downsample_layers.0.1");
    for (int s = 1; s < 4; ++s) {
        norm(bb + fmt("downsample_layers.%d.0", s));
        linear(bb + fmt("downsample_layers.%d.1", s));
    }
    for (int s = 0; s < 4; ++s)
        for (int b = 0; b < kStageDepths[s]; ++b) {
            const std::string p = bb + fmt("stages.%d.%d.", s, b);
            take(p + "dwconv.weight", false, {kStageDims[s], 7, 7});
            take(p + "dwconv.bias");
            norm(p + "norm");
            linear(p + "pwconv1");
            linear(p + "pwconv2");
            take(p + "gamma");
        }
    const std::string hd = "extractor.head.";
    norm(hd + "stem.0");
    take(hd + "stem.1.weight", true);
    take(hd + "stem.1.bias");
    norm(hd + "stem.2");
    linear(hd + "fine_proj");
    for (int i = 0; i < 3; ++i) {
        norm(hd + fmt("norms.%d", i));
        linear(hd + fmt("projs.%d", i), false);
    }
    norm(hd + "mlp.0");
    linear(hd + "mlp.1");
    linear(hd + "mlp.3");
    linear(hd + "down");
    norm(hd + "out_norm");

    linear("matcher.input_proj");
    for (int l = 0; l < kLayers; ++l) {
        const std::string sa = fmt("matcher.transformers.%d.self_attn.", l);
        // Wqkv's rows are (head, dim, 3): q, k and v interleaved per element.
        nn::OnnxTensor wq = f.read(sa + "Wqkv.weight"), bq = f.read(sa + "Wqkv.bias");
        NN_CHECK(wq.data.size() == (size_t)3 * kDim * kDim, "'%s': %sWqkv is not [1536, 512]",
                 path.c_str(), sa.c_str());
        for (int j = 0; j < 3; ++j) {
            std::vector<float> m((size_t)kDim * kDim), b(kDim);
            for (int r = 0; r < kDim; ++r) {
                const size_t src = (size_t)r * 3 + j;
                std::copy_n(wq.data.begin() + src * kDim, kDim, m.begin() + (size_t)r * kDim);
                b[r] = bq.data[src];
            }
            const char* n = j == 0 ? "q" : j == 1 ? "k" : "v";
            w.stage(sa + n + ".weight", {kDim, kDim}, std::move(m), half);
            w.stage(sa + n + ".bias", {kDim}, std::move(b), false);
        }
        norm(sa + "q_norm");
        norm(sa + "k_norm");
        linear(sa + "out_proj");
        norm(sa + "message_norm");
        linear(sa + "ffn.0");
        norm(sa + "ffn.1");
        linear(sa + "ffn.3");
        const std::string ca = fmt("matcher.transformers.%d.cross_attn.", l);
        linear(ca + "to_qk");
        linear(ca + "to_v");
        linear(ca + "to_out");
        norm(ca + "qk_norm");
        norm(ca + "message_norm");
        linear(ca + "ffn.0");
        norm(ca + "ffn.1");
        linear(ca + "ffn.3");
    }
    for (int k = 1; k <= kLayers; ++k) linear(fmt("matcher.heads.%d.disambiguate", k));
    {
        const std::string h = fmt("matcher.heads.%d.", kLayers);
        linear(h + "to_search");
        linear(h + "matchability");
        for (const char* e : {"query_encoder", "match_encoder", "head"}) {
            const std::string o = h + "offset_head." + e + ".";
            linear(o + "0");
            norm(o + "1");
            linear(o + "3");
        }
        temperature = f.read(h + "temperature").data.at(0);
    }

    // Rotary table: cos/sin of Wr . cell centre, the centre scaled by the
    // longer side (64 both ways).
    const nn::OnnxTensor wr = f.read("matcher.posenc.Wr.weight");   // [32, 2]
    NN_CHECK(wr.data.size() == (size_t)kHeadDim, "'%s': posenc.Wr is not [32, 2]", path.c_str());
    rope.assign((size_t)kTokens * kHeadDim, 0.0f);
    for (int y = 0; y < kGrid; ++y)
        for (int x = 0; x < kGrid; ++x) {
            const float px = (x + 0.5f) / kGrid * 2 - 1, py = (y + 0.5f) / kGrid * 2 - 1;
            for (int i = 0; i < kHeadDim / 2; ++i) {
                const float a = wr.data[2 * i] * px + wr.data[2 * i + 1] * py;
                rope_proj.push_back(a);
                const size_t o = ((size_t)(y * kGrid + x) * (kHeadDim / 2) + i) * 2;
                rope[o] = std::cos(a);
                rope[o + 1] = std::sin(a);
            }
        }

    w.upload("flexglue-weights");
    arena.reserve(512ull << 20);
    loaded = true;
    NN_LOG_INFO("[flexglue] %s: %.1f MB on device, temperature %.4f\n", path.c_str(),
                (double)w.deviceBytes() / 1e6, temperature);
}

// ---------------------------------------------------------------------------
// Descriptor
// ---------------------------------------------------------------------------

Tensor Model::Impl::encode(const Tensor& img) {
    const std::string bb = "extractor.backbone.";
    Tensor feats[4];
    int hw = kSize / 4;

    // Stem: 4x4 stride-4 conv as patchify + GEMM, then LN.
    Tensor x;
    {
        Tensor cols = nn::arena_tensor(arena, DType::F32, hw * hw, 48);
        nn::patchify(cols, img, 4);
        x = nn::arena_tensor(arena, DType::F32, hw, hw, kStageDims[0]);
        LinearOpts lo;
        lo.bias = g(bb + "downsample_layers.0.0.bias");
        nn::linear(x.view(hw * hw, kStageDims[0]), cols,
                   g(bb + "downsample_layers.0.0.weight"), lo);
        nn::layer_norm(x, x, g(bb + "downsample_layers.0.1.weight"),
                       g(bb + "downsample_layers.0.1.bias"), kBackboneEps);
    }
    for (int s = 0; s < 4; ++s) {
        const int C = kStageDims[s];
        if (s > 0) {
            const int cin = kStageDims[s - 1];
            Tensor n = nn::arena_tensor(arena, DType::F32, hw, hw, cin);
            nn::layer_norm(n, x, g(bb + fmt("downsample_layers.%d.0.weight", s)),
                           g(bb + fmt("downsample_layers.%d.0.bias", s)), kBackboneEps);
            const int ho = hw / 2;
            Tensor cols = nn::arena_tensor(arena, DType::F32, ho * ho, 4 * cin);
            nn::patchify(cols, n, 2);
            Tensor y = nn::arena_tensor(arena, DType::F32, ho, ho, C);
            LinearOpts lo;
            lo.bias = g(bb + fmt("downsample_layers.%d.1.bias", s));
            nn::linear(y.view(ho * ho, C), cols, g(bb + fmt("downsample_layers.%d.1.weight", s)),
                       lo);
            x = y;
            hw = ho;
        }
        const int64_t n = (int64_t)hw * hw;
        for (int b = 0; b < kStageDepths[s]; ++b) {
            const std::string p = bb + fmt("stages.%d.%d.", s, b);
            vk::ArenaScope scope(arena);
            Tensor dw = nn::arena_tensor(arena, DType::F32, hw, hw, C);
            nn::ConvOpts co;
            co.pad_y = co.pad_x = 3;
            co.bias = g(p + "dwconv.bias");
            nn::conv2d_depthwise(dw, x, g(p + "dwconv.weight"), 7, 7, co);
            nn::layer_norm(dw, dw, g(p + "norm.weight"), g(p + "norm.bias"), kBackboneEps);
            Tensor h = nn::arena_tensor(arena, DType::F32, n, 4 * C);
            LinearOpts l1;
            l1.bias = g(p + "pwconv1.bias");
            l1.act = Act::GeluErf;
            nn::linear(h, dw.view(n, C), g(p + "pwconv1.weight"), l1);
            Tensor y = dw.view(n, C);
            LinearOpts l2;
            l2.bias = g(p + "pwconv2.bias");
            nn::linear(y, h, g(p + "pwconv2.weight"), l2);
            nn::mul(y, y, g(p + "gamma"));
            nn::add(x.view(n, C), x.view(n, C), y);
        }
        feats[s] = x;
        dumpT(fmt("f%d", 4 << s), x);
    }

    // LinearConvHead.
    const std::string hd = "extractor.head.";
    const int fh = kSize / 4;
    Tensor out = nn::arena_tensor(arena, DType::F32, kGrid, kGrid, kDim);
    {
        vk::ArenaScope scope(arena);
        Tensor n0 = nn::arena_tensor(arena, DType::F32, fh, fh, 128);
        nn::layer_norm(n0, feats[0], g(hd + "stem.0.weight"), g(hd + "stem.0.bias"), kEps);
        Tensor st = nn::arena_tensor(arena, DType::F32, fh, fh, 128);
        nn::ConvOpts co;
        co.pad_y = co.pad_x = 1;
        co.bias = g(hd + "stem.1.bias");
        nn::conv2d(arena, st, n0, g(hd + "stem.1.weight"), 3, 3, co);
        nn::layer_norm(st, st, g(hd + "stem.2.weight"), g(hd + "stem.2.bias"), kEps);
        nn::unary(st, st, Act::GeluErf);
        dumpT("head_stem", st);
        // pixel_unshuffle(2)'s channel c*4 + dy*2 + dx is patchify's column order.
        Tensor cols = nn::arena_tensor(arena, DType::F32, kTokens, 512);
        nn::patchify(cols, st, 2);
        LinearOpts lf;
        lf.bias = g(hd + "fine_proj.bias");
        nn::linear(out.view(kTokens, kDim), cols, g(hd + "fine_proj.weight"), lf);
    }
    dumpT("head_fine", out);
    for (int i = 0; i < 3; ++i) {
        vk::ArenaScope scope(arena);
        const Tensor& c = feats[i + 1];
        const int64_t ch = c.shape[0], cc = c.shape[2];
        Tensor nrm = nn::arena_tensor(arena, DType::F32, ch, ch, cc);
        nn::layer_norm(nrm, c, g(hd + fmt("norms.%d.weight", i)), g(hd + fmt("norms.%d.bias", i)),
                       kEps);
        Tensor pr = nn::arena_tensor(arena, DType::F32, ch, ch, kDim);
        nn::linear(pr.view(ch * ch, kDim), nrm.view(ch * ch, cc), g(hd + fmt("projs.%d.weight", i)));
        if (ch != kGrid) {
            Tensor up = nn::arena_tensor(arena, DType::F32, kGrid, kGrid, kDim);
            nn::resize_bilinear(up, pr, false);
            pr = up;
        }
        nn::add(out, out, pr);
    }
    dumpT("head_x", out);
    Tensor feat = nn::arena_tensor(arena, DType::F32, kGrid, kGrid, kFeat);
    {
        vk::ArenaScope scope(arena);
        Tensor n = nn::arena_tensor(arena, DType::F32, kTokens, kDim);
        nn::layer_norm(n, out.view(kTokens, kDim), g(hd + "mlp.0.weight"), g(hd + "mlp.0.bias"),
                       kEps);
        Tensor h = nn::arena_tensor(arena, DType::F32, kTokens, kDim);
        LinearOpts l1;
        l1.bias = g(hd + "mlp.1.bias");
        l1.act = Act::GeluErf;
        nn::linear(h, n, g(hd + "mlp.1.weight"), l1);
        LinearOpts l3;
        l3.bias = g(hd + "mlp.3.bias");
        l3.residual = out.view(kTokens, kDim);
        nn::linear(out.view(kTokens, kDim), h, g(hd + "mlp.3.weight"), l3);
        LinearOpts ld;
        ld.bias = g(hd + "down.bias");
        nn::linear(feat.view(kTokens, kFeat), out.view(kTokens, kDim), g(hd + "down.weight"), ld);
        nn::layer_norm(feat, feat, g(hd + "out_norm.weight"), g(hd + "out_norm.bias"), kEps);
    }
    dumpT("features", feat);
    return feat;
}

// ---------------------------------------------------------------------------
// Matcher
// ---------------------------------------------------------------------------

void Model::Impl::ffn(const Tensor& x, const Tensor& message, int64_t n, const std::string& b) {
    nn::ConcatFfn f;
    f.w0 = g(b + "ffn.0.weight");
    f.b0 = g(b + "ffn.0.bias");
    f.ln_w = g(b + "ffn.1.weight");
    f.ln_b = g(b + "ffn.1.bias");
    f.w3 = g(b + "ffn.3.weight");
    f.b3 = g(b + "ffn.3.bias");
    f.msg_ln_w = g(b + "message_norm.weight");
    f.msg_ln_b = g(b + "message_norm.bias");
    f.eps = kEps;
    nn::concat_ffn(arena, x, message, n, kDim, f);
}

void Model::Impl::attend(const Tensor& ctx, const Tensor& q, const Tensor& k,
                         const Tensor& v) {
    nn::AttnOpts ao;
    ao.n_heads = kHeads;
    ao.head_dim = kHeadDim;
    ao.arena = &arena;
    if (profile) vk::Stream::get().sync();
    const double t0 = nn::now_ms();
    nn::attention(ctx, q, k, v, kTokens, kTokens, ao);
    if (profile) vk::Stream::get().sync();
    t_attn += nn::now_ms() - t0;
}

void Model::Impl::selfBlock(const Tensor& x, const Tensor& freqs, const std::string& b) {
    vk::ArenaScope scope(arena);
    const int64_t n = kTokens;
    Tensor qkv[3];
    for (int j = 0; j < 3; ++j) {
        const char* nm = j == 0 ? "q" : j == 1 ? "k" : "v";
        qkv[j] = nn::arena_tensor(arena, DType::F32, n, kDim);
        LinearOpts lo;
        lo.bias = g(b + nm + ".bias");
        nn::linear(qkv[j], x, g(b + nm + ".weight"), lo);
    }
    // Per-head LayerNorm: [n, 8 * 64] is [n * 8, 64] rows.
    nn::layer_norm(qkv[0].view(n * kHeads, kHeadDim), qkv[0].view(n * kHeads, kHeadDim),
                   g(b + "q_norm.weight"), g(b + "q_norm.bias"), kEps);
    nn::layer_norm(qkv[1].view(n * kHeads, kHeadDim), qkv[1].view(n * kHeads, kHeadDim),
                   g(b + "k_norm.weight"), g(b + "k_norm.bias"), kEps);
    nn::rope(qkv[0], freqs, kHeads, kHeadDim, n);
    nn::rope(qkv[1], freqs, kHeads, kHeadDim, n);
    Tensor ctx = nn::arena_tensor(arena, DType::F32, n, kDim);
    attend(ctx, qkv[0], qkv[1], qkv[2]);
    Tensor msg = qkv[0];
    LinearOpts lo;
    lo.bias = g(b + "out_proj.bias");
    nn::linear(msg, ctx, g(b + "out_proj.weight"), lo);
    ffn(x, msg, n, b);
}

void Model::Impl::crossBlock(const Tensor& x0, const Tensor& x1, const std::string& b) {
    vk::ArenaScope scope(arena);
    const int64_t n = kTokens;
    const Tensor x[2] = {x0, x1};
    Tensor qk[2], v[2], msg[2];
    for (int s = 0; s < 2; ++s) {
        qk[s] = nn::arena_tensor(arena, DType::F32, n, kDim);
        v[s] = nn::arena_tensor(arena, DType::F32, n, kDim);
        LinearOpts lq, lv;
        lq.bias = g(b + "to_qk.bias");
        lv.bias = g(b + "to_v.bias");
        nn::linear(qk[s], x[s], g(b + "to_qk.weight"), lq);
        nn::linear(v[s], x[s], g(b + "to_v.weight"), lv);
        nn::layer_norm(qk[s].view(n * kHeads, kHeadDim), qk[s].view(n * kHeads, kHeadDim),
                       g(b + "qk_norm.weight"), g(b + "qk_norm.bias"), kEps);
    }
    for (int s = 0; s < 2; ++s) {
        Tensor ctx = nn::arena_tensor(arena, DType::F32, n, kDim);
        attend(ctx, qk[s], qk[1 - s], v[1 - s]);
        msg[s] = nn::arena_tensor(arena, DType::F32, n, kDim);
        LinearOpts lo;
        lo.bias = g(b + "to_out.bias");
        nn::linear(msg[s], ctx, g(b + "to_out.weight"), lo);
    }
    for (int s = 0; s < 2; ++s) ffn(x[s], msg[s], n, b);
}

float Model::Impl::covisibility(const Tensor& x0, const Tensor& x1, int layer) {
    vk::ArenaScope scope(arena);
    const std::string b = fmt("matcher.heads.%d.disambiguate.", layer);
    Tensor z = nn::arena_tensor(arena, DType::F32, 2 * kTokens, 1);
    LinearOpts lo;
    lo.bias = g(b + "bias");
    nn::linear(z.view(kTokens, 1), x0, g(b + "weight"), lo);
    nn::linear(z.offsetElems(kTokens).view(kTokens, 1), x1, g(b + "weight"), lo);
    std::vector<float> h(2 * kTokens);
    nn::tensor_to_host(z, h.data(), (int64_t)h.size());
    double s = 0;
    for (float v : h) s += 1.0 / (1.0 + std::exp(-(double)v));
    return (float)(s / (double)h.size());
}

void Model::Impl::correspond(const Tensor& cf_i, const Tensor& cf_j,
                             const std::vector<float>& kn, int layer, std::vector<float>& p2d,
                             std::vector<float>& conf, int dir) {
    const std::string h = fmt("matcher.heads.%d.", layer);
    const int64_t M = (int64_t)kn.size() / 2;
    p2d.assign((size_t)M * 2, 0.0f);
    conf.assign((size_t)M, 0.0f);
    vk::ArenaScope root(arena);
    const Tensor map_i = cf_i.view(kGrid, kGrid, kDim), map_j = cf_j.view(kGrid, kGrid, kDim);

    Tensor search = nn::arena_tensor(arena, DType::F32, kTokens, kSearch);
    LinearOpts ls;
    ls.bias = g(h + "to_search.bias");
    nn::linear(search, cf_j, g(h + "to_search.weight"), ls);
    nn::l2_normalize_rows(search, search);
    const std::string d = std::to_string(dir);
    dumpT("search" + d, search);

    auto mlp = [&](const Tensor& out, const Tensor& in, int64_t n, const std::string& p,
                   int64_t hidden) {
        vk::ArenaScope s(arena);
        Tensor t = nn::arena_tensor(arena, DType::F32, n, hidden);
        LinearOpts a;
        a.bias = g(p + "0.bias");
        nn::linear(t, in, g(p + "0.weight"), a);
        nn::layer_norm(t, t, g(p + "1.weight"), g(p + "1.bias"), kEps);
        nn::unary(t, t, Act::Relu);
        LinearOpts c;
        c.bias = g(p + "3.bias");
        nn::linear(out, t, g(p + "3.weight"), c);
    };

    for (int64_t c0 = 0; c0 < M; c0 += kQueryChunk) {
        const int64_t m = std::min(kQueryChunk, M - c0);
        vk::ArenaScope scope(arena);
        Tensor pts = nn::arena_tensor(arena, DType::F32, m, 2);
        nn::tensor_from_host(pts, kn.data() + c0 * 2, m * 2);
        Tensor fq = nn::arena_tensor(arena, DType::F32, m, kDim);
        nn::grid_sample_points(fq, map_i, pts, false, nn::GridPad::Border);
        Tensor q = nn::arena_tensor(arena, DType::F32, m, kSearch);
        nn::linear(q, fq, g(h + "to_search.weight"), ls);
        nn::l2_normalize_rows(q, q);
        Tensor logits = nn::arena_tensor(arena, DType::F32, m, kTokens);
        nn::matmul_nt(logits, q, search, temperature);

        Tensor pos = nn::arena_tensor(arena, DType::F32, m, 2);
        Tensor mass = nn::arena_tensor(arena, DType::F32, m);
        Tensor cell = nn::arena_tensor(arena, DType::F32, m, 2);
        {
            ReadoutParams p{};
            p.logits = logits.ptr;
            p.pos = pos.ptr;
            p.mass = mass.ptr;
            p.cell = cell.ptr;
            p.R = (uint32_t)m;
            p.stride = kTokens;
            p.h = p.w = kGrid;
            const vk::Stream::Fold fold = vk::Stream::fold1D(m, 1);
            p.groups_per_row = fold.per_row;
            vk::Stream::get().dispatch("flexglue.readout", vk::SpecList{0u, 0u}, fold.per_row,
                                       fold.rows, 1, &p, sizeof p,
                                       (double)m * kTokens * 8 * vk::Stream::kElemWork);
        }
        Tensor coarse;
        if (dump.on() && stages && M <= kQueryChunk) {
            dumpT("logits" + d, logits);
            dumpT("coarse_p2d" + d, pos);
            dumpT("mass" + d, mass);
            std::vector<float> c((size_t)m * 2), am((size_t)m), tau((size_t)m);
            nn::tensor_to_host(cell, c.data(), m * 2);
            for (int64_t r = 0; r < m; ++r) {
                am[(size_t)r] = c[2 * r];
                tau[(size_t)r] = c[2 * r + 1];
            }
            dump.host((prefix + "argmax" + d).c_str(), am.data(), {m});
            dump.host((prefix + "tau" + d).c_str(), tau.data(), {m});
            coarse = nn::arena_tensor(arena, DType::F32, m, 2);
            nn::copy(coarse, pos);
        }
        Tensor fm = nn::arena_tensor(arena, DType::F32, m, kDim);
        nn::grid_sample_points(fm, map_j, pos, false, nn::GridPad::Border);

        Tensor cat = nn::arena_tensor(arena, DType::F32, m, 512);
        Tensor qe = nn::arena_tensor(arena, DType::F32, m, 256);
        mlp(qe, fq, m, h + "offset_head.query_encoder.", 256);
        nn::strided_copy(cat, qe, m, 256, 256, 512);
        mlp(qe, fm, m, h + "offset_head.match_encoder.", 256);
        nn::strided_copy(cat.offsetElems(256), qe, m, 256, 256, 512);
        Tensor head = nn::arena_tensor(arena, DType::F32, m, 36);
        mlp(head, cat, m, h + "offset_head.head.", 512);
        Tensor z = nn::arena_tensor(arena, DType::F32, m, 1, 1, 1, 2);
        LinearOpts lz;
        lz.bias = g(h + "matchability.bias");
        nn::linear(z, fq, g(h + "matchability.weight"), lz);
        Tensor cf = nn::arena_tensor(arena, DType::F32, m);
        {
            OffsetParams p{};
            p.head = head.ptr;
            p.z = z.ptr;
            p.mass = mass.ptr;
            p.pos = pos.ptr;
            p.conf = cf.ptr;
            p.R = (uint32_t)m;
            p.h = p.w = kGrid;
            vk::Stream::get().dispatchFlat("flexglue.offset", vk::SpecList{0u, 0u}, m, 256, &p,
                                           sizeof p, &p.groups_per_row);
        }
        if (coarse.valid()) {
            Tensor off = nn::arena_tensor(arena, DType::F32, m, 2);
            nn::add(off, pos, coarse, 1.0f, -1.0f);
            dumpT("offset" + d, off);
        }
        if (dump.on() && M <= kQueryChunk) {
            dumpT("fquery" + d, fq);
            dumpT("fmatch" + d, fm);
            dumpT("p2d" + d, pos);
            dumpT("conf" + d, cf);
        }
        nn::tensor_to_host(pos, p2d.data() + c0 * 2, m * 2);
        nn::tensor_to_host(cf, conf.data() + c0, m);
    }
}

MatchResult Model::Impl::match(const DenseFeatures& d0, const Keypoints& k0,
                               const DenseFeatures& d1, const Keypoints& k1,
                               const MatchOptions& o) {
    MatchResult r;
    const int64_t M = k0.n, N = k1.n;
    r.matches0.assign((size_t)M, -1);
    r.scores0.assign((size_t)M, 0.0f);
    if (M == 0 || N == 0) return r;

    vk::ArenaScope root(arena);
    const double t_start = nn::now_ms();
    t_attn = t_head = 0;
    struct Report {
        Impl* m;
        double t0;
        ~Report() {
            if (m->profile)
                std::fprintf(stderr, "[flexglue] pair %.1f ms: attention %.1f, readout %.1f\n",
                             nn::now_ms() - t0, m->t_attn, m->t_head);
        }
    } report{this, t_start};
    struct Trim {
        SelfCache& c;
        ~Trim() {
            if (!c.overfull()) return;
            vk::Stream::get().sync();
            c.trim();
        }
    } trim{*self_cache};
    Tensor freqs = nn::arena_tensor(arena, DType::F32, (int64_t)kTokens * (kHeadDim / 2), 2);
    nn::tensor_from_host(freqs, rope.data(), (int64_t)rope.size());
    Tensor x[2];
    const DenseFeatures* d[2] = {&d0, &d1};
    nn::DevicePtr self1[2];
    for (int s = 0; s < 2; ++s) self1[s] = self_cache->find(d[s]->id);
    const uint64_t x_bytes = (uint64_t)kTokens * kDim * 4;
    for (int s = 0; s < 2; ++s) {
        x[s] = nn::arena_tensor(arena, DType::F32, kTokens, kDim);
        if (self1[s] && !(dump.on() && stages)) continue;
        LinearOpts lo;
        lo.bias = g("matcher.input_proj.bias");
        nn::linear(x[s], Tensor(d[s]->ptr, DType::F32, kTokens, kFeat),
                   g("matcher.input_proj.weight"), lo);
        dumpT("tokens" + std::to_string(s), x[s]);
    }
    if (dump.on() && stages)
        dump.host((prefix + "posenc_proj").c_str(), rope_proj.data(), {kTokens, kHeadDim / 2});
    auto dump_outputs = [&]() {
        if (!dump.on()) return;
        std::vector<float> mm(r.matches0.begin(), r.matches0.end());
        const float stop = (float)r.stop_layer;
        dump.host((out_prefix + "matches0").c_str(), mm.data(), {M});
        dump.host((out_prefix + "mscores0").c_str(), r.scores0.data(), {M});
        dump.host((out_prefix + "covisibility_score").c_str(), &r.covisibility, {1});
        dump.host((out_prefix + "stop_layer").c_str(), &stop, {1});
    };

    const bool stopping = o.min_covisibility > 0 && o.early_stop_decay < 1;
    for (int l = 0; l < kLayers; ++l) {
        const std::string sa = fmt("matcher.transformers.%d.self_attn.", l);
        for (int s = 0; s < 2; ++s) {
            if (l == 0 && self1[s]) {
                vk::Stream::get().copy(x[s].ptr, self1[s], x_bytes);
                continue;
            }
            selfBlock(x[s], freqs, sa);
            if (l == 0 && self_cache->capacity > 0)
                vk::Stream::get().copy(self_cache->insert(d[s]->id, x_bytes), x[s].ptr, x_bytes);
        }
        crossBlock(x[0], x[1], fmt("matcher.transformers.%d.cross_attn.", l));
        const int k = l + 1;
        if (dump.on()) {
            dumpT(fmt("x0_layer%d", k), x[0]);
            dumpT(fmt("x1_layer%d", k), x[1]);
        }
        const bool check = std::find(o.check_layers.begin(), o.check_layers.end(), k) !=
                           o.check_layers.end();
        if (k == kLayers || (stopping && check) || dump.on()) {
            const float score = covisibility(x[0], x[1], k);
            if (dump.on() && stages) {
                const std::string nm = prefix + fmt("cov_layer%d", k);
                dump.host(nm.c_str(), &score, {1});
            }
            r.covisibility = score;
            r.stop_layer = k;
            if (k < kLayers && stopping && check &&
                score < o.min_covisibility * std::pow(1.0f - o.early_stop_decay,
                                                      (float)(kLayers - k))) {
                dump_outputs();
                return r;
            }
        }
    }
    const bool covisible = r.covisibility >= o.min_covisibility;
    r.covisible_pair = covisible;

    // Keypoints into the 512 frame, as COLMAP passes them.
    const Keypoints* kp[2] = {&k0, &k1};
    std::vector<float> k512[2], kn[2], p2dn[2], conf[2];
    for (int s = 0; s < 2; ++s) {
        const Keypoints& K = *kp[s];
        const float sx = (float)kSize / (float)K.width, sy = (float)kSize / (float)K.height;
        k512[s].resize((size_t)K.n * 2);
        kn[s].resize((size_t)K.n * 2);
        for (uint32_t i = 0; i < K.n; ++i) {
            k512[s][2 * i] = K.xy[2 * i] * sx;
            k512[s][2 * i + 1] = K.xy[2 * i + 1] * sy;
            kn[s][2 * i] = k512[s][2 * i] / (float)kSize * 2 - 1;
            kn[s][2 * i + 1] = k512[s][2 * i + 1] / (float)kSize * 2 - 1;
        }
    }
    if (profile) vk::Stream::get().sync();
    const double th = nn::now_ms();
    correspond(x[0], x[1], kn[0], kLayers, p2dn[0], conf[0], 0);
    correspond(x[1], x[0], kn[1], kLayers, p2dn[1], conf[1], 1);
    t_head += nn::now_ms() - th;
    if (o.keep_correspondences)
        for (int s = 0; s < 2; ++s) {
            r.p2d[s] = p2dn[s];
            r.conf[s] = conf[s];
        }

    // sparse_matches with exact_nn, in 512-frame pixels.
    std::vector<float> p2d[2], valid[2];
    auto inside = [](float a, float b) { return a > 0 && a < kSize && b > 0 && b < kSize; };
    for (int s = 0; s < 2; ++s) {
        const size_t n = k512[s].size() / 2;
        p2d[s].resize(n * 2);
        valid[s].resize(n);
        for (size_t i = 0; i < n; ++i) {
            p2d[s][2 * i] = (p2dn[s][2 * i] + 1) / 2 * kSize;
            p2d[s][2 * i + 1] = (p2dn[s][2 * i + 1] + 1) / 2 * kSize;
            valid[s][i] = covisible && inside(k512[s][2 * i], k512[s][2 * i + 1]) &&
                          inside(p2d[s][2 * i], p2d[s][2 * i + 1]);
        }
    }
    const float c = kSize / 2.0f;
    std::vector<float> X((size_t)M * 4), Y((size_t)N * 4);
    for (int64_t m = 0; m < M; ++m) {
        X[4 * m] = p2d[0][2 * m] - c;
        X[4 * m + 1] = p2d[0][2 * m + 1] - c;
        X[4 * m + 2] = k512[0][2 * m] - c;
        X[4 * m + 3] = k512[0][2 * m + 1] - c;
    }
    for (int64_t n = 0; n < N; ++n) {
        Y[4 * n] = k512[1][2 * n] - c;
        Y[4 * n + 1] = k512[1][2 * n + 1] - c;
        Y[4 * n + 2] = p2d[1][2 * n] - c;
        Y[4 * n + 3] = p2d[1][2 * n + 1] - c;
    }
    std::vector<int32_t> m0((size_t)M), m1((size_t)N);
    {
        Tensor tx = nn::arena_tensor(arena, DType::F32, M, 4);
        Tensor ty = nn::arena_tensor(arena, DType::F32, N, 4);
        nn::tensor_from_host(tx, X.data(), M * 4);
        nn::tensor_from_host(ty, Y.data(), N * 4);
        Tensor i0 = nn::arena_tensor(arena, DType::I32, M);
        Tensor i1 = nn::arena_tensor(arena, DType::I32, N);
        auto run = [&](const Tensor& a, const Tensor& b, const Tensor& out, int64_t na,
                       int64_t nb) {
            NearestParams p{};
            p.x = a.ptr;
            p.y = b.ptr;
            p.out = out.ptr;
            p.M = (uint32_t)na;
            p.N = (uint32_t)nb;
            vk::Stream::get().dispatchFlat("flexglue.nearest", vk::SpecList{0u, 0u}, na, 256, &p,
                                           sizeof p, &p.groups_per_row,
                                           (double)na * nb * 12);
        };
        run(tx, ty, i0, M, N);
        run(ty, tx, i1, N, M);
        vk::Stream::get().download(m0.data(), i0.ptr, (uint64_t)M * 4);
        vk::Stream::get().download(m1.data(), i1.ptr, (uint64_t)N * 4);
    }
    auto norm2 = [](float dx, float dy) { return std::sqrt(dx * dx + dy * dy); };
    for (int64_t m = 0; m < M; ++m) {
        const int32_t j = m0[(size_t)m];
        if (j < 0 || j >= N || m1[(size_t)j] != (int32_t)m) continue;
        const float score = std::sqrt(conf[0][(size_t)m] * conf[1][(size_t)j]);
        if (!(score >= o.filter_threshold) || !valid[0][(size_t)m] || !valid[1][(size_t)j])
            continue;
        if (o.max_distance_px > 0) {
            const float dist =
                0.5f * (norm2(p2d[0][2 * m] - k512[1][2 * j], p2d[0][2 * m + 1] - k512[1][2 * j + 1]) +
                        norm2(k512[0][2 * m] - p2d[1][2 * j], k512[0][2 * m + 1] - p2d[1][2 * j + 1]));
            if (!(dist < o.max_distance_px)) continue;
        }
        r.matches0[(size_t)m] = j;
        r.scores0[(size_t)m] = score;
    }
    if (dump.on() && stages) {
        // nn: the raw argmins; m: mutual, -1 elsewhere, before any gate.
        std::vector<float> a(m0.begin(), m0.end()), b(m1.begin(), m1.end()), ma(a), mb(b),
            v0(valid[0].begin(), valid[0].end()), v1(valid[1].begin(), valid[1].end());
        for (int64_t m = 0; m < M; ++m)
            if (m0[(size_t)m] < 0 || m1[(size_t)m0[(size_t)m]] != (int32_t)m) ma[(size_t)m] = -1;
        for (int64_t n = 0; n < N; ++n)
            if (m1[(size_t)n] < 0 || m0[(size_t)m1[(size_t)n]] != (int32_t)n) mb[(size_t)n] = -1;
        dump.host((prefix + "nn0").c_str(), a.data(), {M});
        dump.host((prefix + "nn1").c_str(), b.data(), {N});
        dump.host((prefix + "m0").c_str(), ma.data(), {M});
        dump.host((prefix + "m1").c_str(), mb.data(), {N});
        dump.host((prefix + "valid0").c_str(), v0.data(), {M});
        dump.host((prefix + "valid1").c_str(), v1.data(), {N});
        dump.host((prefix + "kpts0").c_str(), k512[0].data(), {M, 2});
        dump.host((prefix + "kpts1").c_str(), k512[1].data(), {N, 2});
    }
    dump_outputs();
    return r;
}

// ---------------------------------------------------------------------------

std::vector<float> squash_rgb(const uint8_t* rgb, int width, int height, int out_w, int out_h) {
    struct Tap {
        int i0, i1;
        float w1;
    };
    auto taps = [](int in, int out) {
        std::vector<Tap> t((size_t)out);
        const float scale = (float)in / (float)out;
        for (int i = 0; i < out; ++i) {
            const float src = std::max(0.f, scale * (i + 0.5f) - 0.5f);
            const int i0 = std::min((int)src, in - 1);
            t[(size_t)i] = {i0, i0 < in - 1 ? i0 + 1 : i0, src - (float)i0};
        }
        return t;
    };
    const std::vector<Tap> tx = taps(width, out_w), ty = taps(height, out_h);
    std::vector<float> out((size_t)out_w * out_h * 3);
    const size_t pitch = (size_t)width * 3;
    for (int y = 0; y < out_h; ++y) {
        const uint8_t* r0 = rgb + (size_t)ty[(size_t)y].i0 * pitch;
        const uint8_t* r1 = rgb + (size_t)ty[(size_t)y].i1 * pitch;
        const float wy = ty[(size_t)y].w1;
        for (int x = 0; x < out_w; ++x) {
            const Tap& t = tx[(size_t)x];
            for (int c = 0; c < 3; ++c) {
                const float top = (1.f - t.w1) * r0[3 * t.i0 + c] + t.w1 * r0[3 * t.i1 + c];
                const float bot = (1.f - t.w1) * r1[3 * t.i0 + c] + t.w1 * r1[3 * t.i1 + c];
                out[((size_t)y * out_w + x) * 3 + c] = ((1.f - wy) * top + wy * bot) * (1.f / 255.f);
            }
        }
    }
    return out;
}

Model::Model() : impl_(new Impl) {
    if (const char* e = spirula::env("FLEXGLUE_SELF_CACHE"); e && *e)
        impl_->self_cache->capacity = std::max(std::atoi(e), 0);
}
Model::~Model() = default;
bool Model::loaded() const { return impl_->loaded; }
void Model::setDump(const std::string& prefix, const std::string& outputs, bool stages) {
    impl_->prefix = prefix;
    impl_->out_prefix = outputs;
    impl_->stages = stages;
}

void Model::load(const std::string& model) {
    NN_ENSURE_EMBEDDED_MODULES(flexglue);
    NN_CHECK(!impl_->loaded, "flexglue::Model::load called twice");
    impl_->load(resolve_checkpoint(model));
}

std::shared_ptr<DenseFeatures> Model::describe(const uint8_t* rgb, int width, int height) {
    const std::vector<float> s = squash_rgb(rgb, width, height, kSize, kSize);
    return describeResized(s.data());
}

std::shared_ptr<DenseFeatures> Model::describeResized(const float* rgb512) {
    NN_CHECK(impl_->loaded, "flexglue::Model::describe before load()");
    std::vector<float> norm((size_t)kSize * kSize * 3);
    for (size_t i = 0; i < norm.size(); ++i)
        norm[i] = (rgb512[i] - kMean[i % 3]) / kStd[i % 3];
    auto out = std::make_shared<DenseFeatures>();
    out->id = g_next_id++;
    out->self = impl_->self_cache;
    out->ptr = vk::device_alloc((uint64_t)kTokens * kFeat * 4, "flexglue-features");
    vk::ArenaScope scope(impl_->arena);
    Tensor img = nn::arena_tensor(impl_->arena, DType::F32, kSize, kSize, 3);
    nn::tensor_from_host(img, norm.data(), (int64_t)norm.size());
    impl_->dumpT("desc_norm", img);
    const Tensor feat = impl_->encode(img);
    vk::Stream::get().copy(out->ptr, feat.ptr, (uint64_t)kTokens * kFeat * 4);
    vk::Stream::get().sync();
    return out;
}

MatchResult Model::match(const DenseFeatures& f0, const Keypoints& k0, const DenseFeatures& f1,
                         const Keypoints& k1, const MatchOptions& opts) {
    NN_CHECK(impl_->loaded, "flexglue::Model::match before load()");
    return impl_->match(f0, k0, f1, k1, opts);
}

void Model::setSelfCache(int entries) {
    SelfCache& c = *impl_->self_cache;
    {
        std::lock_guard<std::mutex> l(c.mu);
        c.capacity = std::max(entries, 0);
    }
    if (!c.overfull()) return;
    vk::Stream::get().sync();
    c.trim();
}

Model::SelfCacheStats Model::selfCacheStats() const {
    SelfCache& c = *impl_->self_cache;
    std::lock_guard<std::mutex> l(c.mu);
    SelfCacheStats s = c.stats;
    s.entries = (int)c.lru.size();
    s.capacity = c.capacity;
    return s;
}

std::vector<float> Model::download(const DenseFeatures& f) const {
    std::vector<float> h((size_t)kTokens * kFeat);
    nn::tensor_to_host(Tensor(f.ptr, DType::F32, kTokens, kFeat), h.data(), (int64_t)h.size());
    return h;
}

}  // namespace flexglue
