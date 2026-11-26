#include "aliked/model/Weights.h"

#include "aliked/Common.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace aliked {
namespace {

constexpr uint64_t kAlign = 256;

uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

// A tensor staged on the host on its way to the device.
struct Staged {
    std::string          name;
    std::vector<int64_t> shape;
    std::vector<float>   data;
};

// string_view: a std::string& parameter makes GCC's -Wdangling-reference
// flag every `need(f, prefix + ".weight", ...)` bound to a reference.
const OnnxTensor& need(const OnnxFile& f, std::string_view name, const char* path) {
    const OnnxTensor* t = f.find(std::string(name));
    if (t) return *t;
    // Name every miss with the file, because the usual cause is pointing
    // --aliked-model at the wrong .onnx (the LightGlue one, say).
    nn::fail("'%s' has no initializer '%s'; is this an ALIKED checkpoint?", path,
             std::string(name).c_str());
}

void check_shape(const OnnxTensor& t, std::initializer_list<int64_t> want,
                 const char* path) {
    bool ok = t.shape.size() == want.size();
    if (ok) {
        size_t i = 0;
        for (int64_t w : want) {
            if (w >= 0 && t.shape[i] != w) ok = false;
            ++i;
        }
    }
    if (ok) return;
    std::string ws = "[";
    size_t i = 0;
    for (int64_t w : want) {
        if (i++) ws += ", ";
        ws += (w < 0) ? std::string("*") : std::to_string(w);
    }
    ws += "]";
    nn::fail("'%s': initializer '%s' is %s, expected %s", path, t.name.c_str(),
             t.shapeString().c_str(), ws.c_str());
}

// Fold `conv` (shape [Co, Ci, kh, kw], no bias -- ALIKED's convs never have
// one) together with the BatchNorm that follows it into one conv plus a bias:
//
//   s      = gamma / sqrt(var + eps)
//   w'[o]  = w[o] * s[o]
//   b'[o]  = beta[o] - mean[o] * s[o]
//
// Appends both to `out`, named "<conv>.weight" and "<conv>.bias".
void fold_bn(const OnnxFile& f, const char* path, const std::string& conv_name,
             const std::string& bn_prefix, std::vector<Staged>& out) {
    const OnnxTensor& w = need(f, conv_name + ".weight", path);
    const OnnxTensor& gamma = need(f, bn_prefix + ".weight", path);
    const OnnxTensor& beta = need(f, bn_prefix + ".bias", path);
    const OnnxTensor& mean = need(f, bn_prefix + ".running_mean", path);
    const OnnxTensor& var = need(f, bn_prefix + ".running_var", path);

    NN_CHECK(w.shape.size() == 4, "'%s': '%s' is %s, expected a 4-D conv kernel", path,
             w.name.c_str(), w.shapeString().c_str());
    const int64_t Co = w.shape[0];
    const int64_t per_out = w.shape[1] * w.shape[2] * w.shape[3];
    for (const OnnxTensor* t : {&gamma, &beta, &mean, &var})
        NN_CHECK((int64_t)t->data.size() == Co,
                 "'%s': '%s' has %zu entries, expected %lld to match '%s'", path,
                 t->name.c_str(), t->data.size(), (long long)Co, w.name.c_str());

    // The epsilon in the file, not PyTorch's default. ONNX's own default is
    // 1e-5 and applies when the node did not spell the attribute out.
    float eps = 1e-5f;
    auto it = f.bn_epsilon.find(gamma.name);
    if (it != f.bn_epsilon.end()) eps = it->second;

    Staged sw{conv_name + ".weight", w.shape, w.data};
    Staged sb{conv_name + ".bias", {Co}, std::vector<float>((size_t)Co, 0.0f)};
    for (int64_t o = 0; o < Co; ++o) {
        const float denom = std::sqrt(var.data[(size_t)o] + eps);
        NN_CHECK(denom > 0.0f && std::isfinite(denom),
                 "'%s': '%s'[%lld] is %g; cannot fold BatchNorm", path, var.name.c_str(),
                 (long long)o, var.data[(size_t)o]);
        const float s = gamma.data[(size_t)o] / denom;
        float* row = sw.data.data() + (size_t)(o * per_out);
        for (int64_t i = 0; i < per_out; ++i) row[i] *= s;
        sb.data[(size_t)o] = beta.data[(size_t)o] - mean.data[(size_t)o] * s;
    }
    out.push_back(std::move(sw));
    out.push_back(std::move(sb));
}

// Copy an initializer through unchanged.
void take(const OnnxFile& f, const char* path, const std::string& name,
          std::vector<Staged>& out) {
    const OnnxTensor& t = need(f, name, path);
    out.push_back(Staged{name, t.shape, t.data});
}

void take_optional(const OnnxFile& f, const std::string& name, std::vector<Staged>& out) {
    if (const OnnxTensor* t = f.find(name)) out.push_back(Staged{name, t->shape, t->data});
}

}  // namespace

AlikedWeights::~AlikedWeights() {
    if (blob_) vk::device_free(blob_);
}

void AlikedWeights::load(const std::string& onnx_path) {
    NN_CHECK(!loaded_, "AlikedWeights::load called twice");
    const char* path = onnx_path.c_str();
    const OnnxFile file = read_onnx(onnx_path);

    // ---- hyperparameters, from shapes ----
    AlikedHparams hp;
    {
        const OnnxTensor& b1 = need(file, "block1.conv1.weight", path);
        check_shape(b1, {-1, 3, 3, 3}, path);
        hp.c1 = (int)b1.shape[0];
        hp.c2 = (int)need(file, "block2.conv1.weight", path).shape[0];
        hp.c3 = (int)need(file, "block3.conv1.regular_conv.weight", path).shape[0];
        hp.c4 = (int)need(file, "block4.conv1.regular_conv.weight", path).shape[0];

        const OnnxTensor& p1 = need(file, "conv1.weight", path);
        check_shape(p1, {-1, -1, 1, 1}, path);
        hp.dim4 = (int)p1.shape[0];
        hp.dim = hp.dim4 * 4;

        const OnnxTensor& agg = need(file, "desc_head.agg_weights", path);
        NN_CHECK(agg.shape.size() == 3, "'%s': desc_head.agg_weights is %s, expected [M, C, C]",
                 path, agg.shapeString().c_str());
        hp.M = (int)agg.shape[0];
        hp.desc_dim = (int)agg.shape[2];
        NN_CHECK(agg.shape[1] == hp.dim,
                 "'%s': desc_head.agg_weights is %s but the aggregated width is %d", path,
                 agg.shapeString().c_str(), hp.dim);

        const OnnxTensor& oc = need(file, "desc_head.offset_conv.0.weight", path);
        check_shape(oc, {2 * hp.M, hp.dim, -1, -1}, path);
        NN_CHECK(oc.shape[2] == oc.shape[3], "'%s': desc_head.offset_conv.0 is not square",
                 path);
        hp.K = (int)oc.shape[2];

        // The deformable convs carry offsets only -- 2 per tap, no modulation
        // mask. A 3*k*k export would need a different kernel, so refuse it
        // rather than silently sampling with the wrong channel stride.
        const OnnxTensor& off = need(file, "block3.conv1.offset_conv.weight", path);
        NN_CHECK(off.shape.size() == 4 && off.shape[0] == 2 * off.shape[2] * off.shape[3],
                 "'%s': block3.conv1.offset_conv is %s; this port implements the "
                 "unmodulated (2*k*k offset channels) form only",
                 path, off.shapeString().c_str());
    }
    hp_ = hp;

    // ---- stage every tensor the forward pass asks for ----
    std::vector<Staged> staged;
    staged.reserve(48);

    // block1 is a ConvBlock, block2..4 are ResBlocks; blocks 3 and 4 use
    // deformable convs, whose *offset* conv has its own bias and no BN.
    fold_bn(file, path, "block1.conv1", "block1.bn1", staged);
    fold_bn(file, path, "block1.conv2", "block1.bn2", staged);
    fold_bn(file, path, "block2.conv1", "block2.bn1", staged);
    fold_bn(file, path, "block2.conv2", "block2.bn2", staged);
    take(file, path, "block2.downsample.weight", staged);
    take(file, path, "block2.downsample.bias", staged);
    for (int b = 3; b <= 4; ++b) {
        char buf[64];
        for (int c = 1; c <= 2; ++c) {
            std::snprintf(buf, sizeof buf, "block%d.conv%d.regular_conv", b, c);
            char bn[64];
            std::snprintf(bn, sizeof bn, "block%d.bn%d", b, c);
            fold_bn(file, path, buf, bn, staged);
            std::snprintf(buf, sizeof buf, "block%d.conv%d.offset_conv.weight", b, c);
            take(file, path, buf, staged);
            std::snprintf(buf, sizeof buf, "block%d.conv%d.offset_conv.bias", b, c);
            take(file, path, buf, staged);
        }
        std::snprintf(buf, sizeof buf, "block%d.downsample.weight", b);
        take(file, path, buf, staged);
        std::snprintf(buf, sizeof buf, "block%d.downsample.bias", b);
        take(file, path, buf, staged);
    }

    for (int i = 1; i <= 4; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "conv%d.weight", i);
        take(file, path, buf, staged);
    }
    for (int i : {0, 2, 4, 6}) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "score_head.%d.weight", i);
        take(file, path, buf, staged);
        // The score head's convs have no bias in the released checkpoints;
        // read one if some future export grows it rather than assuming.
        std::snprintf(buf, sizeof buf, "score_head.%d.bias", i);
        take_optional(file, buf, staged);
    }
    take(file, path, "desc_head.offset_conv.0.weight", staged);
    take(file, path, "desc_head.offset_conv.0.bias", staged);
    take(file, path, "desc_head.offset_conv.2.weight", staged);
    take(file, path, "desc_head.offset_conv.2.bias", staged);
    take(file, path, "desc_head.sf_conv.weight", staged);
    take_optional(file, "desc_head.sf_conv.bias", staged);
    // The descriptor aggregation is einsum('ncp,pcd->nd'): for each of the M
    // sample positions, a [C, D] matrix applied to that position's features
    // and summed. That is M ordinary matmuls -- which is how the forward pass
    // runs it, on the tuned GEMM -- except that `linear` wants its weight as
    // [out_features, in_features] and the checkpoint stores [in, out]. So the
    // transpose happens once, here, rather than M times per image in a kernel.
    {
        const OnnxTensor& agg = need(file, "desc_head.agg_weights", path);
        const int64_t M = agg.shape[0], C = agg.shape[1], D = agg.shape[2];
        Staged t{"desc_head.agg_weights_t", {M, D, C}, std::vector<float>(agg.data.size())};
        for (int64_t p = 0; p < M; ++p)
            for (int64_t c = 0; c < C; ++c)
                for (int64_t d = 0; d < D; ++d)
                    t.data[(size_t)((p * D + d) * C + c)] =
                        agg.data[(size_t)((p * C + c) * D + d)];
        staged.push_back(std::move(t));
    }

    // ---- one allocation, 256-byte sub-alignment ----
    uint64_t total = 0;
    for (const Staged& s : staged) total = align_up(total, kAlign) + s.data.size() * 4;
    NN_CHECK(total > 0, "'%s': nothing to upload", path);

    blob_ = vk::device_alloc(total, "aliked-weights");
    device_bytes_ = total;

    uint64_t off = 0;
    for (const Staged& s : staged) {
        off = align_up(off, kAlign);
        const nn::DevicePtr ptr = blob_ + off;
        vk::Stream::get().upload(ptr, s.data.data(), s.data.size() * 4);
        off += s.data.size() * 4;

        nn::Tensor t;
        t.ptr = ptr;
        t.dtype = nn::DType::F32;
        t.ndim = (int32_t)std::min<size_t>(s.shape.size(), 4);
        NN_CHECK(s.shape.size() <= 4, "'%s': '%s' has rank %zu; nn::Tensor holds 4", path,
                 s.name.c_str(), s.shape.size());
        for (int i = 0; i < t.ndim; ++i) t.shape[i] = s.shape[(size_t)i];
        if (t.ndim == 0) { t.ndim = 1; t.shape[0] = 1; }
        tensors_[s.name] = t;
    }
    vk::Stream::get().sync();

    path_ = onnx_path;
    loaded_ = true;
    NN_LOG_INFO("[aliked] %s: c=(%d,%d,%d,%d) dim=%d K=%d M=%d desc=%d, %zu tensors, "
                "%.2f MB on device\n",
                onnx_path.c_str(), hp.c1, hp.c2, hp.c3, hp.c4, hp.dim, hp.K, hp.M,
                hp.desc_dim, tensors_.size(), (double)total / 1e6);
}

nn::Tensor AlikedWeights::get(const std::string& name) const {
    auto it = tensors_.find(name);
    if (it != tensors_.end()) return it->second;

    // Near misses: the failure is almost always a name that drifted, and
    // printing the neighbourhood turns a five-minute hunt into a glance.
    std::vector<std::string> misses;
    for (const auto& kv : tensors_) {
        const std::string& k = kv.first;
        const size_t dot = name.find('.');
        if (dot != std::string::npos && k.compare(0, dot, name, 0, dot) == 0)
            misses.push_back(k);
    }
    std::sort(misses.begin(), misses.end());
    std::string hint;
    for (size_t i = 0; i < misses.size() && i < 8; ++i) hint += "\n    " + misses[i];
    nn::fail("no weight named '%s'%s%s", name.c_str(),
             hint.empty() ? "" : " (did you mean one of these?)", hint.c_str());
}

nn::Tensor AlikedWeights::getf(const char* fmt, ...) const {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return get(buf);
}

}  // namespace aliked
