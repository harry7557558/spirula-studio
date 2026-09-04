#include "loma/model/Weights.h"

#include "loma/Common.h"

#include "core/Env.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

namespace loma {
namespace {

constexpr uint64_t kAlign = 256;

// LoMa's head width at every released size (256/4, 512/8, 1024/16). It is not
// written anywhere in the file, so it is the one hyperparameter assumed rather
// than read; an embed dim that is not a multiple of it is refused.
constexpr int64_t kHeadDim = 64;

uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

struct Staged {
    std::string          name;
    std::vector<int64_t> shape;
    const std::vector<float>* src = nullptr;   // borrowed from the OnnxFile
    std::vector<float>   own;                  // used when src is null
    bool                 f16 = false;
    const std::vector<float>& data() const { return src ? *src : own; }
    uint64_t bytes() const { return (uint64_t)data().size() * (f16 ? 2 : 4); }
};

// f16 is what the tensor-core GEMM wants, and DeDoDe-G's frozen DINOv2 is
// 1.2 GB of matrices on its own. SS_LOMA_F32_WEIGHTS=1 keeps everything f32,
// which is what makes tools/loma/compare_ort.py tight enough to trust.
bool f16_allowed() {
    static const bool force_f32 = [] {
        const char* v = spirula::env("LOMA_F32_WEIGHTS");
        return v && v[0] && v[0] != '0';
    }();
    return !force_f32;
}

const OnnxTensor& need(const OnnxFile& f, const std::string& name, const char* path,
                       const char* what) {
    if (const OnnxTensor* t = f.find(name)) return *t;
    nn::fail("'%s' has no initializer '%s'; is this a LoMa %s checkpoint?", path,
             name.c_str(), what);
}

void take(const OnnxFile& f, const char* path, const char* what, const std::string& name,
          std::vector<Staged>& out) {
    const OnnxTensor& t = need(f, name, path, what);
    Staged s;
    s.name = name;
    s.shape = t.shape;
    s.src = &t.data;
    out.push_back(std::move(s));
}

// A [rows, cols] matrix transposed on the host. The matcher's exports store a
// Linear's weight the way MatMul wants it ([in, out]); nn::linear wants
// [out, in], and doing it once here beats a strided read per token.
Staged transposed(const OnnxTensor& t, const std::string& name) {
    Staged s;
    s.name = name;
    const int64_t rows = t.shape[0], cols = t.shape[1];
    s.shape = {cols, rows};
    s.own.resize(t.data.size());
    for (int64_t r = 0; r < rows; ++r)
        for (int64_t c = 0; c < cols; ++c)
            s.own[(size_t)(c * rows + r)] = t.data[(size_t)(r * cols + c)];
    return s;
}

// Wqkv's [head][dim][3] rows repacked to [3][head][dim]. The bias goes the
// same way and is appended to `extra`; the weight is returned.
Staged permute_qkv(const OnnxTensor& wt, const OnnxTensor& bt, int n_heads,
                   int64_t head_dim, const std::string& name,
                   std::vector<Staged>* extra) {
    const int64_t out = wt.shape[0], in = wt.shape[1];
    Staged sw;
    sw.name = name;
    sw.shape = {out, in};
    sw.own.resize(wt.data.size());
    Staged sb;
    sb.name = name.substr(0, name.size() - 7) + ".bias";
    sb.shape = {out};
    sb.own.resize((size_t)out);
    for (int h = 0; h < n_heads; ++h)
        for (int64_t d = 0; d < head_dim; ++d)
            for (int j = 0; j < 3; ++j) {
                const int64_t src = ((int64_t)h * head_dim + d) * 3 + j;
                const int64_t dst = (int64_t)j * n_heads * head_dim + h * head_dim + d;
                std::memcpy(&sw.own[(size_t)(dst * in)], &wt.data[(size_t)(src * in)],
                            (size_t)in * sizeof(float));
                sb.own[(size_t)dst] = bt.data[(size_t)src];
            }
    extra->push_back(std::move(sb));
    return sw;
}

// Allocate one blob, upload every staged tensor into it 256-byte aligned, and
// record the views.
uint64_t upload(const std::vector<Staged>& staged, const char* tag, const char* path,
                nn::DevicePtr& blob,
                std::unordered_map<std::string, nn::Tensor>& tensors) {
    uint64_t total = 0;
    for (const Staged& s : staged) total = align_up(total, kAlign) + s.bytes();
    NN_CHECK(total > 0, "'%s': nothing to upload", path);

    blob = vk::device_alloc(total, tag);
    uint64_t off = 0;
    std::vector<uint16_t> half;
    for (const Staged& s : staged) {
        off = align_up(off, kAlign);
        const nn::DevicePtr ptr = blob + off;
        if (s.f16) {
            half.resize(s.data().size());
            for (size_t i = 0; i < half.size(); ++i)
                half[i] = nn::float_to_half(s.data()[i]);
            vk::Stream::get().upload(ptr, half.data(), s.bytes());
        } else {
            vk::Stream::get().upload(ptr, s.data().data(), s.bytes());
        }
        off += s.bytes();

        NN_CHECK(s.shape.size() <= 4, "'%s': '%s' has rank %zu; nn::Tensor holds 4", path,
                 s.name.c_str(), s.shape.size());
        nn::Tensor t;
        t.ptr = ptr;
        t.dtype = s.f16 ? nn::DType::F16 : nn::DType::F32;
        t.ndim = (int32_t)s.shape.size();
        for (int i = 0; i < t.ndim; ++i) t.shape[i] = s.shape[(size_t)i];
        if (t.ndim == 0) { t.ndim = 1; t.shape[0] = 1; }
        tensors[s.name] = t;
    }
    vk::Stream::get().sync();
    return total;
}

nn::Tensor lookup(const std::unordered_map<std::string, nn::Tensor>& tensors,
                  const std::string& name) {
    auto it = tensors.find(name);
    if (it != tensors.end()) return it->second;

    // Near misses: the failure is almost always a name that drifted, and
    // printing the neighbourhood turns a hunt into a glance.
    std::vector<std::string> near;
    const size_t dot = name.rfind('.');
    for (const auto& kv : tensors)
        if (dot != std::string::npos && kv.first.compare(0, dot, name, 0, dot) == 0)
            near.push_back(kv.first);
    std::sort(near.begin(), near.end());
    std::string hint;
    for (size_t i = 0; i < near.size() && i < 8; ++i) hint += "\n    " + near[i];
    nn::fail("no weight named '%s'%s%s", name.c_str(),
             hint.empty() ? "" : " (did you mean one of these?)", hint.c_str());
}

}  // namespace

PyramidWeights::~PyramidWeights() {
    if (blob_) vk::device_free(blob_);
}

void PyramidWeights::load(const std::string& onnx_path, const char* prefix) {
    NN_CHECK(!loaded_, "PyramidWeights::load called twice");
    const char* path = onnx_path.c_str();
    const char* what = prefix;
    const OnnxFile file = read_onnx(onnx_path);

    PyramidHparams hp;
    std::vector<Staged> staged;

    // Encoder taps. torchvision VGG is [conv, bn, relu] per layer with a
    // MaxPool between blocks, so a gap of 4 between conv indices is a MaxPool
    // and 3 is not; DeDoDe appends before every MaxPool, which makes it a tap.

    // DeDoDe-G puts a frozen DINOv2 beside the VGG, which moves the VGG down
    // one level in the name tree; which shape the file has decides the prefix.
    hp.dino_prefix = std::string(prefix) + ".encoder.frozen_dinov2.dinov2_vitl14.";
    if (!file.find(hp.dino_prefix + "pos_embed")) hp.dino_prefix.clear();
    hp.vgg_prefix = std::string(prefix) +
                    (hp.dino_prefix.empty() ? ".encoder.layers." : ".encoder.vgg.layers.");
    {
        std::map<int, const OnnxTensor*> convs;
        const std::string p = hp.vgg_prefix;
        for (const OnnxTensor& t : file.initializers) {
            if (t.name.compare(0, p.size(), p) != 0) continue;
            const size_t dot = t.name.find('.', p.size());
            if (dot == std::string::npos ||
                t.name.compare(dot, std::string::npos, ".weight") != 0)
                continue;
            convs[std::atoi(t.name.c_str() + p.size())] = &t;
        }
        NN_CHECK(!convs.empty(), "'%s' has no '%s*.weight'; is this a LoMa %s "
                                 "checkpoint?", path, p.c_str(), what);
        for (const auto& kv : convs) hp.conv_index.push_back(kv.first);
        for (size_t i = 0; i < hp.conv_index.size(); ++i) {
            const bool last = i + 1 == hp.conv_index.size();
            if (last || hp.conv_index[i + 1] - hp.conv_index[i] >= 4) {
                hp.tap_at.push_back((int)i);
                hp.tap_width.push_back(convs.at(hp.conv_index[i])->shape[0]);
            }
        }
        for (int idx : hp.conv_index) {
            take(file, path, what, p + std::to_string(idx) + ".weight", staged);
            take(file, path, what, p + std::to_string(idx) + ".bias", staged);
        }
    }

    // ---- decoder: one ConvRefiner per named scale, coarse first ----
    {
        std::set<int> scales;
        const std::string p = std::string(prefix) + ".decoder.layers.";
        for (const OnnxTensor& t : file.initializers)
            if (t.name.compare(0, p.size(), p) == 0)
                scales.insert(std::atoi(t.name.c_str() + p.size()));
        NN_CHECK(!scales.empty(), "'%s' has no '%s*'", path, p.c_str());
        for (auto it = scales.rbegin(); it != scales.rend(); ++it) {
            RefinerSpec r;
            r.scale = *it;
            const std::string q = p + std::to_string(r.scale) + ".";
            const OnnxTensor& b1 = need(file, q + "block1.0.weight", path, what);
            NN_CHECK(b1.shape.size() == 4, "'%s': '%s' is %s, expected a 4-D kernel", path,
                     b1.name.c_str(), b1.shapeString().c_str());
            r.hidden = b1.shape[0];
            r.in = b1.shape[1];
            while (file.find(q + "hidden_blocks." + std::to_string(r.blocks) + ".0.weight"))
                ++r.blocks;
            NN_CHECK(r.blocks > 0, "'%s': '%shidden_blocks.0.0.weight' is missing", path,
                     q.c_str());
            r.out = need(file, q + "out_conv.weight", path, what).shape[0];
            hp.refiners.push_back(r);

            take(file, path, what, q + "block1.0.weight", staged);
            take(file, path, what, q + "block1.0.bias", staged);
            take(file, path, what, q + "block1.3.weight", staged);
            take(file, path, what, q + "block1.3.bias", staged);
            for (int b = 0; b < r.blocks; ++b) {
                const std::string h = q + "hidden_blocks." + std::to_string(b) + ".";
                take(file, path, what, h + "0.weight", staged);
                take(file, path, what, h + "0.bias", staged);
                take(file, path, what, h + "3.weight", staged);
                take(file, path, what, h + "3.bias", staged);
            }
            take(file, path, what, q + "out_conv.weight", staged);
            take(file, path, what, q + "out_conv.bias", staged);
        }
    }

    // The frozen DINOv2, when the checkpoint carries one. Its Linears were
    // exported as bare MatMuls so only the biases kept a name; linearWeights()
    // walks Add -> MatMul, and each [in, out] matrix is transposed here.
    if (!hp.dino_prefix.empty()) {
        const std::string d = hp.dino_prefix;
        const auto linears = file.linearWeights();
        auto take_linear = [&](const std::string& module, bool as_f16) {
            auto it = linears.find(module);
            NN_CHECK(it != linears.end(), "'%s' has no linear named '%s'", path,
                     module.c_str());
            const OnnxTensor& wt = need(file, it->second, path, what);
            NN_CHECK(wt.shape.size() == 2, "'%s': '%s' is %s, expected a matrix", path,
                     module.c_str(), wt.shapeString().c_str());
            Staged sw = transposed(wt, module + ".weight");
            sw.f16 = as_f16 && f16_allowed();
            staged.push_back(std::move(sw));
            take(file, path, what, module + ".bias", staged);
        };

        const OnnxTensor& pe = need(file, d + "patch_embed.proj.weight", path, what);
        NN_CHECK(pe.shape.size() == 4 && pe.shape[2] == pe.shape[3],
                 "'%s': '%s' is %s, expected a square patch kernel", path, pe.name.c_str(),
                 pe.shapeString().c_str());
        hp.dino_width = pe.shape[0];
        hp.dino_patch = (int)pe.shape[2];
        hp.dino_heads = (int)(hp.dino_width / kHeadDim);
        NN_CHECK(hp.dino_heads * kHeadDim == hp.dino_width,
                 "'%s': DINOv2 is %lld wide, not a multiple of the %lld head width", path,
                 (long long)hp.dino_width, (long long)kHeadDim);
        take(file, path, what, d + "patch_embed.proj.weight", staged);
        take(file, path, what, d + "patch_embed.proj.bias", staged);

        while (file.find(d + "blocks." + std::to_string(hp.dino_blocks) + ".norm1.weight"))
            ++hp.dino_blocks;
        NN_CHECK(hp.dino_blocks > 0, "'%s' has no '%sblocks.0.norm1.weight'", path,
                 d.c_str());
        for (int b = 0; b < hp.dino_blocks; ++b) {
            const std::string q = d + "blocks." + std::to_string(b) + ".";
            for (const char* n : {"norm1", "norm2"}) {
                take(file, path, what, q + n + ".weight", staged);
                take(file, path, what, q + n + ".bias", staged);
            }
            take(file, path, what, q + "ls1.gamma", staged);
            take(file, path, what, q + "ls2.gamma", staged);
            take_linear(q + "attn.qkv", true);
            take_linear(q + "attn.proj", true);
            take_linear(q + "mlp.fc1", true);
            take_linear(q + "mlp.fc2", true);
        }
        hp.dino_mlp = need(file, d + "blocks.0.mlp.fc1.bias", path, what).shape[0];
        take(file, path, what, d + "norm.weight", staged);
        take(file, path, what, d + "norm.bias", staged);

        // The positional embedding stays on the host: it is resampled to the
        // token grid the image size implies, so what reaches the device is a
        // derived tensor rather than this one.
        const OnnxTensor& pos = need(file, d + "pos_embed", path, what);
        NN_CHECK(pos.shape.size() == 3 && pos.shape[2] == hp.dino_width,
                 "'%s': pos_embed is %s, expected [1, N, %lld]", path,
                 pos.shapeString().c_str(), (long long)hp.dino_width);
        const int64_t np = pos.shape[1] - 1;
        const int64_t grid = (int64_t)(std::sqrt((double)np) + 0.5);
        NN_CHECK(grid * grid == np, "'%s': %lld patch positions are not a square", path,
                 (long long)np);
        hp.dino_pos_grid = grid;
        dino_cls_pos_.assign(pos.data.begin(), pos.data.begin() + hp.dino_width);
        dino_patch_pos_.assign(pos.data.begin() + hp.dino_width, pos.data.end());

        // The class token is anonymous too -- the export expanded it to the
        // batch, leaving a [1, 1, D] constant no other tensor has the shape of.
        const OnnxTensor* cls = nullptr;
        for (const OnnxTensor& t : file.initializers)
            if (t.shape.size() == 3 && t.shape[0] == 1 && t.shape[1] == 1 &&
                t.shape[2] == hp.dino_width) {
                NN_CHECK(cls == nullptr,
                         "'%s' has more than one [1, 1, %lld] initializer; the class "
                         "token can no longer be identified by shape",
                         path, (long long)hp.dino_width);
                cls = &t;
            }
        NN_CHECK(cls != nullptr, "'%s' has no [1, 1, %lld] class token", path,
                 (long long)hp.dino_width);
        Staged sc;
        sc.name = d + "cls_token";
        sc.shape = {1, hp.dino_width};
        sc.src = &cls->data;
        staged.push_back(std::move(sc));
    }

    // A refiner emits `head` channels then the context the level below eats,
    // and that level's input is its own tap plus that context -- so ctx is the
    // difference, and every level agreeing on `head` is the check.
    {
        const size_t n = hp.refiners.size();
        NN_CHECK(n >= 2, "'%s': %zu decoder levels, expected at least 2", path, n);
        // The finest level's taps are the widest map: tap_width runs fine to
        // coarse, and the refiners run coarse to fine, so they index opposed.
        NN_CHECK(hp.tap_width.size() + (hp.refiners[0].in == hp.tap_width.back() ? 0 : 1) ==
                     n,
                 "'%s': %zu encoder taps but %zu decoder levels", path,
                 hp.tap_width.size(), n);
        for (size_t i = 0; i + 1 < n; ++i) {
            const int64_t tap_next = hp.tap_width[n - 2 - i];
            const int64_t ctx = hp.refiners[i + 1].in - tap_next;
            NN_CHECK(ctx > 0, "'%s': level %d takes %lld channels but its tap is %lld",
                     path, hp.refiners[i + 1].scale, (long long)hp.refiners[i + 1].in,
                     (long long)tap_next);
            hp.refiners[i].ctx = ctx;
            const int64_t head = hp.refiners[i].out - ctx;
            NN_CHECK(head > 0 && (hp.head == 0 || head == hp.head),
                     "'%s': level %d implies a head width of %lld, level %d said %lld",
                     path, hp.refiners[i].scale, (long long)head, hp.refiners[0].scale,
                     (long long)hp.head);
            hp.head = head;
        }
        // The finest level's context channels are sliced off and dropped.
        hp.refiners[n - 1].ctx = hp.refiners[n - 1].out - hp.head;
        NN_CHECK(hp.refiners[n - 1].ctx >= 0, "'%s': finest level emits %lld < head %lld",
                 path, (long long)hp.refiners[n - 1].out, (long long)hp.head);
    }

    // The square the export baked into its "image" input, if it baked one in:
    // the descriptors were exported at a fixed size and nothing in any
    // initializer says which, while the detector left both axes dynamic.
    if (const nn::OnnxValueInfo* in = file.input("image")) {
        NN_CHECK(in->shape.size() == 4, "'%s': 'image' is %zu-D, expected 4", path,
                 in->shape.size());
        if (in->shape[2] > 0 || in->shape[3] > 0) {
            NN_CHECK(in->shape[2] == in->shape[3],
                     "'%s': the input is %lld x %lld; this port runs the fixed square "
                     "the released exports carry",
                     path, (long long)in->shape[2], (long long)in->shape[3]);
            hp.input_size = in->shape[2];
        }
    }

    // ---- the detector's ImageNet normalization, if the file carries one ----
    if (const OnnxTensor* m = file.find(std::string(prefix) + ".normalizer.mean")) {
        const OnnxTensor& sd =
            need(file, std::string(prefix) + ".normalizer.std", path, what);
        NN_CHECK(m->data.size() == 3 && sd.data.size() == 3,
                 "'%s': normalizer is %s / %s, expected 3 channels each", path,
                 m->shapeString().c_str(), sd.shapeString().c_str());
        for (int i = 0; i < 3; ++i) {
            mean_[i] = m->data[(size_t)i];
            std_[i] = sd.data[(size_t)i];
        }
    }

    device_bytes_ = upload(staged, "loma-pyramid", path, blob_, tensors_);
    hp_ = hp;
    path_ = onnx_path;
    loaded_ = true;

    std::string levels;
    for (const RefinerSpec& r : hp.refiners)
        levels += (levels.empty() ? "" : ",") + std::to_string(r.scale);
    NN_LOG_INFO("[loma] %s: %s, %zu conv / %zu taps, levels {%s}, head=%lld%s, %zu "
                "tensors, %.2f MB on device\n",
                onnx_path.c_str(), prefix, hp.conv_index.size(), hp.tap_width.size(),
                levels.c_str(), (long long)hp.head,
                hp.dino_width ? ", dinov2" : "", tensors_.size(),
                (double)device_bytes_ / 1e6);
}

nn::Tensor PyramidWeights::get(const std::string& name) const {
    return lookup(tensors_, name);
}

nn::Tensor PyramidWeights::getf(const char* fmt, ...) const {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return get(buf);
}

// ---------------------------------------------------------------------------

MatcherWeights::~MatcherWeights() {
    if (blob_) vk::device_free(blob_);
}

void MatcherWeights::load(const std::string& onnx_path) {
    NN_CHECK(!loaded_, "MatcherWeights::load called twice");
    const char* path = onnx_path.c_str();
    const char* what = "matcher";
    const OnnxFile file = read_onnx(onnx_path);

    MatcherHparams hp;
    while (file.find("model.transformers." + std::to_string(hp.n_layers) +
                     ".self_attn.Wqkv.weight"))
        ++hp.n_layers;
    NN_CHECK(hp.n_layers > 0,
             "'%s' has no 'model.transformers.0.self_attn.Wqkv.weight'; is this a LoMa "
             "matcher?", path);

    std::vector<Staged> staged;

    const OnnxTensor& qkv0 =
        need(file, "model.transformers.0.self_attn.Wqkv.weight", path, what);
    NN_CHECK(qkv0.shape.size() == 2 && qkv0.shape[0] == 3 * qkv0.shape[1],
             "'%s': Wqkv is %s, expected [3*d, d]", path, qkv0.shapeString().c_str());
    hp.embed_dim = qkv0.shape[1];

    NN_CHECK(hp.embed_dim % kHeadDim == 0,
             "'%s': embed dim %lld is not a multiple of LoMa's head width %lld", path,
             (long long)hp.embed_dim, (long long)kHeadDim);
    hp.n_heads = (int)(hp.embed_dim / kHeadDim);

    if (const OnnxTensor* ip = file.find("model.input_proj.weight")) {
        NN_CHECK(ip->shape.size() == 2 && ip->shape[0] == hp.embed_dim,
                 "'%s': input_proj is %s, expected [%lld, in]", path,
                 ip->shapeString().c_str(), (long long)hp.embed_dim);
        hp.input_dim = ip->shape[1];
        hp.input_proj = true;
        take(file, path, what, "model.input_proj.weight", staged);
        take(file, path, what, "model.input_proj.bias", staged);
    } else {
        hp.input_dim = hp.embed_dim;
    }

    for (int i = 0; i < hp.n_layers; ++i) {
        char q[96];
        std::snprintf(q, sizeof q, "model.transformers.%d.", i);
        const std::string b = q;
        for (const char* n : {"self_attn.out_proj", "self_attn.ffn.0", "self_attn.ffn.1",
                              "self_attn.ffn.3", "cross_attn.to_qk", "cross_attn.to_v",
                              "cross_attn.to_out", "cross_attn.ffn.0", "cross_attn.ffn.1",
                              "cross_attn.ffn.3"}) {
            take(file, path, what, b + n + ".weight", staged);
            take(file, path, what, b + n + ".bias", staged);
        }
        // Wqkv's rows are [head][dim][3] -- q, k and v INTERLEAVED per element,
        // which no stride can express. Permuting them to [3][head][dim] here is
        // exactly the fused layout nn::attention's q/k/v strides address.
        Staged qkv = permute_qkv(need(file, b + "self_attn.Wqkv.weight", path, what),
                                 need(file, b + "self_attn.Wqkv.bias", path, what),
                                 hp.n_heads, kHeadDim, b + "self_attn.Wqkv.weight",
                                 &staged);
        staged.push_back(std::move(qkv));
    }

    // The export runs one assignment head, the last layer's. Take whichever is
    // present rather than assuming the index: n_layers is read off the file,
    // and a shorter export would keep a longer checkpoint's extra heads.
    for (int i = hp.n_layers - 1; i >= 0; --i) {
        char q[96];
        std::snprintf(q, sizeof q, "model.log_assignment.%d.final_proj.weight", i);
        if (!file.find(q)) continue;
        hp.assign_layer = i;
        take(file, path, what, q, staged);
        std::snprintf(q, sizeof q, "model.log_assignment.%d.final_proj.bias", i);
        take(file, path, what, q, staged);
        break;
    }
    NN_CHECK(file.find("model.log_assignment." + std::to_string(hp.assign_layer) +
                       ".final_proj.weight"),
             "'%s' has no assignment head", path);

    // The positional encoding's Wr: [2, F] in the file, because the export
    // lowered nn.Linear(2, F, bias=False) to a MatMul. Transposed here so the
    // forward pass can hand it to nn::linear like every other weight.
    {
        const OnnxTensor* wr = nullptr;
        for (const OnnxTensor& t : file.initializers)
            if (t.shape.size() == 2 && t.shape[0] == 2 && t.shape[1] == kHeadDim / 2) {
                NN_CHECK(wr == nullptr,
                         "'%s' has more than one [2, %lld] initializer; the positional "
                         "encoding can no longer be identified by shape",
                         path, (long long)(kHeadDim / 2));
                wr = &t;
            }
        NN_CHECK(wr != nullptr,
                 "'%s': no [2, %lld] positional-encoding matrix; is this a LoMa matcher?",
                 path, (long long)(kHeadDim / 2));
        staged.push_back(transposed(*wr, "posenc.Wr.weight"));
    }

    device_bytes_ = upload(staged, "loma-matcher", path, blob_, tensors_);
    hp_ = hp;
    path_ = onnx_path;
    loaded_ = true;
    NN_LOG_INFO("[loma] %s: matcher, %d layers x %d heads, embed=%lld, input=%lld, "
                "%zu tensors, %.2f MB on device\n",
                onnx_path.c_str(), hp.n_layers, hp.n_heads, (long long)hp.embed_dim,
                (long long)hp.input_dim, tensors_.size(), (double)device_bytes_ / 1e6);
}

nn::Tensor MatcherWeights::get(const std::string& name) const {
    return lookup(tensors_, name);
}

nn::Tensor MatcherWeights::getf(const char* fmt, ...) const {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return get(buf);
}

}  // namespace loma
