#include "birefnet/BiRefNet.h"

#include "core/Env.h"
#include "nn/Ops.h"
#include "nn/WeightStore.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/Safetensors.h"
#include "nn/io/StageDump.h"
#include "nn/vk/Stream.h"
#include "swin/Swin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace birefnet {
namespace {

using nn::DType;
using nn::Tensor;
namespace vk = nn::vk;

// SHA-256 is the LFS object id each file is published under. The mirrors are
// ModelScope repositories carrying the same bytes.
const ModelSource kSources[] = {
    {"birefnet-lite",
     {"birefnet-lite.safetensors",
      "https://huggingface.co/ZhengPeng7/BiRefNet_lite/resolve/main/model.safetensors",
      "4417d89795250e698c3cb0ae8df15743810065f646f48a694fdfa7ca052d0815", 177634392ull,
      "https://modelscope.cn/models/1038lab/BiRefNet/resolve/master/BiRefNet_lite.safetensors"}},
    {"birefnet",
     {"birefnet-general.safetensors",
      "https://huggingface.co/ZhengPeng7/BiRefNet/resolve/main/model.safetensors",
      "9ab37426bf4de0567af6b5d21b16151357149139362e6e8992021b8ce356a154", 444473596ull,
      "https://modelscope.cn/models/modelscope/BiRefNet/resolve/master/model.safetensors"}},
};

std::string fmt(const char* f, int a) {
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a);
    return buf;
}

// Matrices go to the device as f16 unless SS_BIREFNET_F32_WEIGHTS=1, which is
// for tools/birefnet/compare_ort.py and nothing else.
bool f32_weights() {
    static const bool on = spirula::env_on("BIREFNET_F32_WEIGHTS");
    return on;
}

// ---------------------------------------------------------------------------
// Weights
// ---------------------------------------------------------------------------

struct Loader {
    const nn::SafetensorsFile& file;
    nn::WeightStore& store;

    nn::OnnxTensor read(const std::string& n) const { return file.read(n); }

    void put(const std::string& name, nn::OnnxTensor t, bool matrix) {
        store.stage(name, t.shape, std::move(t.data), matrix && !f32_weights());
    }

    // A conv followed by an eval-mode BatchNorm, folded into one conv:
    // w' = w * g / sqrt(var + eps), b' = (b - mean) * g / sqrt(var + eps) + beta.
    void conv_bn(const std::string& conv, const std::string& bn, const std::string& dst) {
        nn::OnnxTensor w = read(conv + ".weight");
        const int64_t co = w.shape[0], per = w.numel() / co;
        std::vector<float> b((size_t)co, 0.0f);
        if (file.has(conv + ".bias")) b = read(conv + ".bias").data;
        const std::vector<float> g = read(bn + ".weight").data, be = read(bn + ".bias").data,
                                 mu = read(bn + ".running_mean").data,
                                 var = read(bn + ".running_var").data;
        for (int64_t o = 0; o < co; ++o) {
            const float s = g[(size_t)o] / std::sqrt(var[(size_t)o] + 1e-5f);
            for (int64_t i = 0; i < per; ++i) w.data[(size_t)(o * per + i)] *= s;
            b[(size_t)o] = (b[(size_t)o] - mu[(size_t)o]) * s + be[(size_t)o];
        }
        put(dst + ".w", std::move(w), true);
        store.stage(dst + ".b", {co}, std::move(b), false);
    }

    void conv(const std::string& src, const std::string& dst) {
        put(dst + ".w", read(src + ".weight"), true);
        put(dst + ".b", read(src + ".bias"), false);
    }

    // BasicDecBlk with its ASPPDeformable. conv1 of the ASPP reads the concat
    // of five 256-wide branches; it is split into five [64, 256] matrices so the
    // branches accumulate into one output and the concat never exists.
    void dec_block(const std::string& src, const std::string& dst) {
        conv_bn(src + ".conv_in", src + ".bn_in", dst + ".in");
        conv_bn(src + ".conv_out", src + ".bn_out", dst + ".out");
        const std::string a = src + ".dec_att";
        const char* branches[4] = {".aspp1", ".aspp_deforms.0", ".aspp_deforms.1",
                                   ".aspp_deforms.2"};
        for (int i = 0; i < 4; ++i) {
            const std::string s = a + branches[i], d = dst + fmt(".br%d", i);
            conv(s + ".atrous_conv.offset_conv", d + ".off");
            conv(s + ".atrous_conv.modulator_conv", d + ".mod");
            conv_bn(s + ".atrous_conv.regular_conv", s + ".bn", d + ".reg");
        }
        conv_bn(a + ".global_avg_pool.1", a + ".global_avg_pool.2", dst + ".gp");

        nn::OnnxTensor c1 = read(a + ".conv1.weight");   // [64, 1280, 1, 1]
        const std::vector<float> g = read(a + ".bn1.weight").data,
                                 be = read(a + ".bn1.bias").data,
                                 mu = read(a + ".bn1.running_mean").data,
                                 var = read(a + ".bn1.running_var").data;
        const int64_t co = c1.shape[0], ci = c1.shape[1], part = ci / 5;
        NN_CHECK(part * 5 == ci, "birefnet: %s.conv1 reads %lld channels, not 5 x 256",
                 a.c_str(), (long long)ci);
        std::vector<float> bias((size_t)co);
        for (int k = 0; k < 5; ++k) {
            std::vector<float> w((size_t)(co * part));
            for (int64_t o = 0; o < co; ++o) {
                const float s = g[(size_t)o] / std::sqrt(var[(size_t)o] + 1e-5f);
                for (int64_t i = 0; i < part; ++i)
                    w[(size_t)(o * part + i)] = c1.data[(size_t)(o * ci + k * part + i)] * s;
                bias[(size_t)o] = be[(size_t)o] - mu[(size_t)o] * s;
            }
            store.stage(dst + fmt(".c1_%d", k), {co, part}, std::move(w), !f32_weights());
        }
        store.stage(dst + ".c1.b", {co}, std::move(bias), false);
    }
};

// ---------------------------------------------------------------------------
// The model
// ---------------------------------------------------------------------------

struct Model {
    nn::WeightStore  w;
    swin::Backbone   bb;
    vk::Arena        arena{"birefnet"};
    int              ch[4] = {};   // lateral channels: stage 3 .. stage 0, doubled
    bool             loaded = false;

    Tensor get(const std::string& n) const { return w.get(n); }

    void load(const std::string& path);
    Tensor forward(const Tensor& image, int S);   // [S*S, 1] logits
    uint64_t planArena(int S) const;

    void conv(const Tensor& out, const Tensor& in, const std::string& name, int k,
              nn::Act act = nn::Act::None) {
        nn::ConvOpts o;
        o.pad_y = o.pad_x = k / 2;
        o.bias = get(name + ".b");
        o.act = act;
        nn::conv2d(arena, out, in, get(name + ".w"), k, k, o);
    }
    void dec_block(const Tensor& out, const Tensor& x, int H, int W, const std::string& p);
    Tensor aspp(const Tensor& x, int H, int W, const std::string& p);
    Tensor simple_convs(const Tensor& img, int S, int H, const std::string& p);
    Tensor concat(const Tensor& a, const Tensor& b);
    void gate(const Tensor& p, int H, const std::string& n);
};

void Model::load(const std::string& path) {
    const nn::SafetensorsFile file(path);
    NN_CHECK(file.has("bb.patch_embed.proj.weight") &&
                 file.has("decoder.conv_out1.0.weight") &&
                 file.has("squeeze_module.0.conv_in.weight"),
             "'%s' is not a BiRefNet checkpoint with a Swin backbone", path.c_str());

    const nn::OnnxTensor table =
        file.read("bb.layers.0.blocks.0.attn.relative_position_bias_table");
    const int side = (int)std::lround(std::sqrt((double)table.shape[0]));
    const int window = (side + 1) / 2;
    auto has = [&](const std::string& n) { return file.has(n); };
    auto read = [&](const std::string& n) { return file.read(n); };
    swin::Config cfg = swin::infer_config(swin::Naming::Microsoft, "bb.", has, read, window);
    swin::stage_weights(w, "bb", cfg, swin::Naming::Microsoft, "bb.", read, !f32_weights());

    Loader l{file, w};
    // channels = lateral_channels_in_collection, doubled by mul_scl_ipt='cat'.
    for (int s = 0; s < 4; ++s) ch[s] = 2 * cfg.dim(3 - s);
    l.dec_block("squeeze_module.0", "sq");
    for (int i = 1; i <= 4; ++i) l.dec_block(fmt("decoder.decoder_block%d", i), fmt("dec%d", i));
    for (int i = 2; i <= 5; ++i) {
        l.conv(fmt("decoder.ipt_blk%d.conv1", i), fmt("ipt%d.c1", i));
        l.conv(fmt("decoder.ipt_blk%d.conv_out", i), fmt("ipt%d.c2", i));
    }
    for (int i = 2; i <= 4; ++i) {
        l.conv(fmt("decoder.lateral_block%d.conv", i), fmt("lat%d", i));
        l.conv_bn(fmt("decoder.gdt_convs_%d.0", i), fmt("decoder.gdt_convs_%d.1", i),
                  fmt("gdt%d", i));
        l.conv(fmt("decoder.gdt_convs_attn_%d.0", i), fmt("gdt_attn%d", i));
    }

    // conv_out1 (1x1 over [upsample(decoder_block1), ipt_blk1]) is folded into
    // both halves -- linear all the way, ipt_blk1's last conv has no activation
    // -- so the 240-channel 1024x1024 concat (960 MB) never exists.
    {
        const nn::OnnxTensor co1 = l.read("decoder.conv_out1.0.weight");
        const float co1_b = l.read("decoder.conv_out1.0.bias").data[0];
        const int64_t na = ch[3] / 2, nb = ch[3] / 8;
        NN_CHECK(co1.numel() == na + nb, "birefnet: conv_out1 reads %lld channels, not %lld",
                 (long long)co1.numel(), (long long)(na + nb));
        w.stage("out_a.w", {1, na}, std::vector<float>(co1.data.begin(), co1.data.begin() + na),
                false);
        l.conv("decoder.ipt_blk1.conv1", "ipt1.c1");
        const nn::OnnxTensor c2 = l.read("decoder.ipt_blk1.conv_out.weight");   // [nb, 64, 3, 3]
        const std::vector<float> c2b = l.read("decoder.ipt_blk1.conv_out.bias").data;
        NN_CHECK(c2.shape[0] == nb, "birefnet: ipt_blk1 emits %lld channels, not %lld",
                 (long long)c2.shape[0], (long long)nb);
        const int64_t per = c2.numel() / nb;
        std::vector<float> fw((size_t)per, 0.0f);
        float fb = co1_b;
        for (int64_t o = 0; o < nb; ++o) {
            const float a = co1.data[(size_t)(na + o)];
            for (int64_t i = 0; i < per; ++i) fw[(size_t)i] += a * c2.data[(size_t)(o * per + i)];
            fb += a * c2b[(size_t)o];
        }
        w.stage("out_b.w", {1, c2.shape[1], c2.shape[2], c2.shape[3]}, std::move(fw), false);
        w.stage("out_b.b", {1}, {fb}, false);
    }

    w.upload("birefnet-weights");
    bb.bind(w, "bb", cfg);
    loaded = true;
    NN_LOG_INFO("[birefnet] %s: swin dim %d depths %d/%d/%d/%d window %d, %.1f MB on device\n",
                path.c_str(), cfg.embed_dim, cfg.depths[0], cfg.depths[1], cfg.depths[2],
                cfg.depths[3], cfg.window, (double)w.deviceBytes() / 1e6);
}

Tensor Model::concat(const Tensor& a, const Tensor& b) {
    const int64_t n = a.rows(), ca = a.cols(), cb = b.cols();
    Tensor out = nn::arena_tensor(arena, DType::F32, n, ca + cb);
    nn::strided_copy(out, a, n, ca, ca, ca + cb);
    nn::strided_copy(out.offsetElems(ca), b, n, cb, cb, ca + cb);
    return out;
}

// ASPPDeformable over [H, W, 64] -> [H*W, 64]. BN is folded everywhere and
// the global-pool branch is constant per pixel, so it becomes conv1's bias.
Tensor Model::aspp(const Tensor& x, int H, int W, const std::string& p) {
    const int64_t n = (int64_t)H * W;
    const int64_t ci = x.cols();
    const int co = (int)get(p + ".c1.b").numel();
    Tensor out = nn::arena_tensor(arena, DType::F32, n, co);
    Tensor bias = nn::arena_tensor(arena, DType::F32, co);
    {
        vk::ArenaScope scope(arena);
        NN_CHECK(H == W, "birefnet: ASPP pooling wants a square map, got %dx%d", H, W);
        Tensor mean = nn::arena_tensor(arena, DType::F32, 1, 1, ci, 1, 3);
        nn::avgpool(mean, x.view(H, W, ci), H);
        Tensor g = nn::arena_tensor(arena, DType::F32, 1, get(p + ".gp.b").numel());
        nn::LinearOpts lo;
        lo.bias = get(p + ".gp.b");
        lo.act = nn::Act::Relu;
        nn::linear(g, mean.view(1, ci), get(p + ".gp.w").asMatrix(), lo);
        nn::LinearOpts lb;
        lb.bias = get(p + ".c1.b");
        nn::linear(bias.view(1, co), g, get(p + ".c1_4"), lb);
    }
    static const int kSizes[4] = {1, 1, 3, 7};
    for (int i = 0; i < 4; ++i) {
        vk::ArenaScope scope(arena);
        const int k = kSizes[i];
        const std::string b = p + fmt(".br%d", i);
        Tensor off = nn::arena_tensor(arena, DType::F32, H, W, 2 * k * k, 1, 3);
        conv(off, x.view(H, W, ci), b + ".off", k);
        Tensor mod = nn::arena_tensor(arena, DType::F32, H, W, k * k, 1, 3);
        conv(mod, x.view(H, W, ci), b + ".mod", k);
        nn::unary(mod, mod, nn::Act::Sigmoid, 1.0f, 0.0f, 2.0f, 0.0f);
        const Tensor reg_b = get(b + ".reg.b");
        Tensor y = nn::arena_tensor(arena, DType::F32, H, W, reg_b.numel(), 1, 3);
        nn::ConvOpts o;
        o.pad_y = o.pad_x = k / 2;
        o.bias = reg_b;
        o.act = nn::Act::Relu;
        nn::modulated_deform_conv2d(arena, y, x.view(H, W, ci), off, mod, get(b + ".reg.w"), k,
                                    k, o);
        nn::LinearOpts lo;
        if (i == 0) lo.bias = bias;
        else lo.residual = out;
        if (i == 3) lo.act = nn::Act::Relu;
        nn::linear(out, y.view(n, reg_b.numel()), get(p + fmt(".c1_%d", i)), lo);
    }
    return out;
}

// BasicDecBlk: conv3x3 + BN + ReLU, ASPPDeformable, conv3x3 + BN.
void Model::dec_block(const Tensor& out, const Tensor& x, int H, int W, const std::string& p) {
    vk::ArenaScope scope(arena);
    Tensor a = nn::arena_tensor(arena, DType::F32, H, W, get(p + ".in.b").numel(), 1, 3);
    conv(a, x.view(H, W, x.cols()), p + ".in", 3, nn::Act::Relu);
    Tensor b = aspp(a.view((int64_t)H * W, a.shape[2]), H, W, p);
    conv(out.view(H, W, out.cols()), b.view(H, W, b.cols()), p + ".out", 3);
}

// The image as an H x H grid of patches, then SimpleConvs (two 3x3 convs with
// nothing between them). Column-major: see src/birefnet/README.md before
// "fixing" that to the einops order the current reference code spells.
Tensor Model::simple_convs(const Tensor& img, int S, int H, const std::string& p) {
    const Tensor c2b = get(p + ".c2.b");
    Tensor out = nn::arena_tensor(arena, DType::F32, (int64_t)H * H, c2b.numel());
    vk::ArenaScope scope(arena);
    const int g = S / H;
    Tensor patches = nn::arena_tensor(arena, DType::F32, H, H, 3 * g * g, 1, 3);
    nn::blocks_to_channels(patches, img, S, S, 3, g, g, nn::BlockOrder::ColumnsThenRows);
    Tensor mid = nn::arena_tensor(arena, DType::F32, H, H, get(p + ".c1.b").numel(), 1, 3);
    conv(mid, patches, p + ".c1", 3);
    conv(out.view(H, H, c2b.numel()), mid, p + ".c2", 3);
    return out;
}

// The gradient-attention gate: p *= sigmoid(conv1x1(relu(bn(conv3x3(p))))).
void Model::gate(const Tensor& p, int H, const std::string& n) {
    vk::ArenaScope scope(arena);
    Tensor g = nn::arena_tensor(arena, DType::F32, H, H, get("gdt" + n + ".b").numel(), 1, 3);
    conv(g, p.view(H, H, p.cols()), "gdt" + n, 3, nn::Act::Relu);
    Tensor a = nn::arena_tensor(arena, DType::F32, (int64_t)H * H, 1, 1, 1, 2);
    nn::LinearOpts lo;
    lo.bias = get("gdt_attn" + n + ".b");
    lo.act = nn::Act::Sigmoid;
    nn::linear(a, g.view((int64_t)H * H, g.shape[2]), get("gdt_attn" + n + ".w").asMatrix(), lo);
    nn::mul_rows(p, p, a);
}

Tensor Model::forward(const Tensor& image, int S) {
    static const nn::StageDump dump("BIREFNET_DUMP");
    Tensor logits = nn::arena_tensor(arena, DType::F32, (int64_t)S * S, 1, 1, 1, 2);
    vk::ArenaScope scope(arena);

    // ---- encoder: the backbone at 1x and 0.5x, concatenated per stage ------
    swin::Features f = bb.forward(arena, image, S, S);
    Tensor x[4];
    int hs[4];
    {
        Tensor half = nn::arena_tensor(arena, DType::F32, S / 2, S / 2, 3, 1, 3);
        nn::resize_bilinear(half, image, /*align_corners=*/true);
        swin::Features g = bb.forward(arena, half, S / 2, S / 2);
        for (int s = 0; s < 4; ++s) {
            hs[s] = f.h[s];
            const int64_t c = f.map[s].shape[2];
            Tensor up = nn::arena_tensor(arena, DType::F32, f.h[s], f.w[s], c, 1, 3);
            nn::resize_bilinear(up, g.map[s], true);
            x[s] = concat(f.map[s].view((int64_t)f.h[s] * f.w[s], c), up.view(up.rows(), c));
            if (dump.on()) dump.tensor(fmt("x%d", s + 1).c_str(), x[s], {hs[s], hs[s], 2 * c});
        }
    }
    const int h4 = hs[3];

    // ---- squeeze: the three finer stages resized onto the coarsest -------
    Tensor p = nn::arena_tensor(arena, DType::F32, (int64_t)h4 * h4, ch[0]);
    {
        vk::ArenaScope sc(arena);
        Tensor cat = x[3];
        for (int s = 2; s >= 0; --s) {
            Tensor r = nn::arena_tensor(arena, DType::F32, h4, h4, x[s].cols(), 1, 3);
            nn::resize_bilinear(r, x[s].view(hs[s], hs[s], x[s].cols()), true);
            cat = concat(r.view((int64_t)h4 * h4, x[s].cols()), cat);
        }
        dec_block(p, cat, h4, h4, "sq");
    }
    if (dump.on()) dump.tensor("squeezed", p, {h4, h4, ch[0]});

    // ---- decoder: three gated levels, each fed the image as patches -------
    const int outc[3] = {ch[1], ch[2], ch[3]};
    for (int lvl = 0; lvl < 3; ++lvl) {
        const int H = hs[3 - lvl], H2 = hs[2 - lvl];
        const int blk = 4 - lvl;   // decoder_block4, 3, 2
        Tensor next = nn::arena_tensor(arena, DType::F32, (int64_t)H2 * H2, outc[lvl]);
        vk::ArenaScope sc(arena);
        Tensor ipt = simple_convs(image, S, H, fmt("ipt%d", blk + 1));
        if (dump.on()) dump.tensor(fmt("ipt%d", blk + 1).c_str(), ipt, {H, H, ipt.cols()});
        Tensor in = concat(p, ipt);
        Tensor d = nn::arena_tensor(arena, DType::F32, (int64_t)H * H, outc[lvl]);
        dec_block(d, in, H, H, fmt("dec%d", blk));
        if (dump.on()) dump.tensor(fmt("dec%d", blk).c_str(), d, {H, H, outc[lvl]});
        gate(d, H, fmt("%d", blk));
        if (dump.on()) dump.tensor(fmt("p%d", blk).c_str(), d, {H, H, outc[lvl]});

        Tensor up = nn::arena_tensor(arena, DType::F32, H2, H2, outc[lvl], 1, 3);
        nn::resize_bilinear(up, d.view(H, H, outc[lvl]), true);
        nn::LinearOpts lo;
        lo.bias = get(fmt("lat%d", blk) + ".b");
        lo.residual = up.view((int64_t)H2 * H2, outc[lvl]);
        nn::linear(next, x[2 - lvl], get(fmt("lat%d", blk) + ".w").asMatrix(), lo);
        p = next;
    }
    {
        const int H = hs[0];
        Tensor t = nn::arena_tensor(arena, DType::F32, H, H, 1, 1, 3);
        {
            vk::ArenaScope sc(arena);
            Tensor ipt = simple_convs(image, S, H, "ipt2");
            Tensor in = concat(p, ipt);
            Tensor d1 = nn::arena_tensor(arena, DType::F32, (int64_t)H * H, ch[3] / 2);
            dec_block(d1, in, H, H, "dec1");
            if (dump.on()) dump.tensor("d1", d1, {H, H, ch[3] / 2});
            // conv_out1 over [upsample(d1), ipt_blk1(image)], folded at load.
            nn::linear(t.view((int64_t)H * H, 1), d1, get("out_a.w"));
        }
        Tensor up = nn::arena_tensor(arena, DType::F32, S, S, 1, 1, 3);
        nn::resize_bilinear(up, t, true);
        Tensor mid = nn::arena_tensor(arena, DType::F32, S, S, get("ipt1.c1.b").numel(), 1, 3);
        conv(mid, image, "ipt1.c1", 3);
        conv(logits.view(S, S, 1), mid, "out_b", 3);
        nn::add(logits, logits, up.view((int64_t)S * S, 1));
    }
    return logits;
}

uint64_t Model::planArena(int S) const {
    // Measured at S = 1024: a 680 MB peak for Swin-T (embed 96), 1102 MB for
    // Swin-L (192). Linear in the width and quadratic in S, plus 15%.
    const double mb = (258.0 + 4.4 * bb.config().embed_dim) * ((double)S * S / (1024.0 * 1024.0));
    return (uint64_t)(mb * 1.15 * 1048576.0);
}

}  // namespace

// ---------------------------------------------------------------------------
// Public surface
// ---------------------------------------------------------------------------

const ModelSource* find_model_source(const std::string& id) {
    for (const ModelSource& s : kSources)
        if (id == s.id) return &s;
    return nullptr;
}

std::string model_id_list() {
    std::string s;
    for (const ModelSource& m : kSources) s += (s.empty() ? "" : ", ") + std::string(m.id);
    return s;
}

std::string resolve_model(const std::string& id_or_path) {
    if (const ModelSource* src = find_model_source(id_or_path))
        return nn::ensure_file(src->file, "birefnet");
    std::error_code ec;
    NN_CHECK(std::filesystem::exists(id_or_path, ec),
             "'%s' is neither a known BiRefNet id (%s) nor a file that exists",
             id_or_path.c_str(), model_id_list().c_str());
    return id_or_path;
}

bool is_checkpoint(const std::string& path) {
    try {
        const nn::SafetensorsFile f(path);
        return f.has("decoder.conv_out1.0.weight") && f.has("bb.patch_embed.proj.weight");
    } catch (const std::exception&) {
        return false;
    }
}

struct Predictor::Impl {
    Model m;
};

Predictor::Predictor() : impl_(new Impl) {}
Predictor::~Predictor() { unload(); }

void Predictor::load(const std::string& id_or_path) {
    unload();
    impl_.reset(new Impl);
    impl_->m.load(resolve_model(id_or_path));
}

bool Predictor::loaded() const { return impl_ && impl_->m.loaded; }

void Predictor::unload() {
    if (!impl_) return;
    vk::Stream::get().sync();
    impl_->m.bb.release();
    impl_->m.w.release();
    impl_->m.arena.release();
    impl_->m.loaded = false;
}

uint64_t Predictor::deviceBytes() const { return impl_->m.w.deviceBytes(); }

std::vector<float> Predictor::forwardNormalized(const std::vector<float>& hwc, int S) {
    NN_CHECK(loaded(), "birefnet: no model loaded");
    NN_CHECK((int64_t)hwc.size() == (int64_t)S * S * 3, "birefnet: input is not [%d, %d, 3]",
             S, S);
    NN_CHECK(S % 64 == 0, "birefnet: input side %d is not a multiple of 64", S);
    Model& m = impl_->m;
    m.arena.reserve(m.planArena(S));
    const uint64_t cap = m.arena.capacity();
    std::vector<float> out((size_t)S * S);
    {
        vk::ArenaScope scope(m.arena);
        Tensor img = nn::arena_tensor(m.arena, DType::F32, S, S, 3, 1, 3);
        nn::tensor_from_host(img, hwc.data(), (int64_t)hwc.size());
        Tensor logits = m.forward(img, S);
        nn::tensor_to_host(logits, out.data(), (int64_t)out.size());
    }
    NN_CHECK(m.arena.capacity() == cap,
             "birefnet: the arena grew from %.0f to %.0f MB mid-pass; planArena is wrong",
             (double)cap / 1e6, (double)m.arena.capacity() / 1e6);
    NN_LOG_INFO("[birefnet] arena peak %.0f of %.0f MB\n", (double)m.arena.highWater() / 1e6,
                (double)cap / 1e6);
    return out;
}

std::vector<uint8_t> Predictor::segment(const nn::Image& image, float threshold) {
    NN_CHECK(loaded(), "birefnet: no model loaded");
    const int S = inputSize();
    const nn::Image resized = nn::resize_image(image, S, S);
    static const float kMean[3] = {0.485f, 0.456f, 0.406f}, kStd[3] = {0.229f, 0.224f, 0.225f};
    std::vector<float> hwc((size_t)S * S * 3);
    for (size_t i = 0; i < (size_t)S * S; ++i)
        for (int c = 0; c < 3; ++c)
            hwc[i * 3 + c] = (resized.data[i * 3 + c] / 255.0f - kMean[c]) / kStd[c];

    Model& m = impl_->m;
    m.arena.reserve(m.planArena(S));
    const uint64_t cap = m.arena.capacity();
    std::vector<uint8_t> mask((size_t)image.width * image.height);
    {
        vk::ArenaScope scope(m.arena);
        Tensor img = nn::arena_tensor(m.arena, DType::F32, S, S, 3, 1, 3);
        nn::tensor_from_host(img, hwc.data(), (int64_t)hwc.size());
        Tensor logits = m.forward(img, S);
        // Thresholding the logit at logit(p) is thresholding the sigmoid at p.
        const float t = std::min(0.999f, std::max(0.001f, threshold));
        Tensor bytes = nn::arena_tensor(m.arena, DType::U8,
                                        ((int64_t)image.width * image.height + 3) / 4 * 4);
        nn::resize_binarize(bytes, logits.view(S, S, 1), image.height, image.width,
                            std::log(t / (1.0f - t)));
        vk::Stream::get().download(mask.data(), bytes.ptr, mask.size());
    }
    NN_CHECK(m.arena.capacity() == cap,
             "birefnet: the arena grew from %.0f to %.0f MB mid-pass; planArena is wrong",
             (double)cap / 1e6, (double)m.arena.capacity() / 1e6);
    return mask;
}

}  // namespace birefnet
