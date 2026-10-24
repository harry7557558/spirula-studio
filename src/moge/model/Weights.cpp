#include "moge/model/Weights.h"

#include "moge/Common.h"

#include "core/Env.h"
#include "nn/io/TorchCheckpoint.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace moge {
namespace {

using nn::DType;
using nn::Tensor;

constexpr uint64_t kAlign = 256;
// NVIDIA's Windows driver refuses a single 1 GiB device-local allocation on an
// 8 GiB laptop part with 7 GiB free; vit-large is 1.3 GB of fp32 weights.
constexpr uint64_t kChunkBytes = 256ull << 20;

uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

bool ends_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

// `encoder.backbone.blocks.11.norm1.weight` -> `encoder.blocks.11.norm1.weight`
// `/encoder/blocks.11/attn/qkv`             -> `encoder.blocks.11.attn.qkv`
// The node paths already elide `backbone`; dropping it makes the two agree.
std::string normalize(std::string s) {
    if (!s.empty() && s[0] == '/') s.erase(0, 1);
    for (char& c : s)
        if (c == '/') c = '.';
    const size_t at = s.find("encoder.backbone.");
    if (at == 0) s.erase(std::strlen("encoder."), std::strlen("backbone."));
    return s;
}

// f16 is what the tensor-core GEMM wants; vectors stay f32. SS_MOGE_F32_WEIGHTS=1
// keeps everything f32 -- not a quality knob, it is what makes
// tools/moge/compare_ort.py tight enough that a real bug cannot hide in rounding.
bool wants_f16(const std::string& name, size_t rank) {
    static const bool force_f32 = [] {
        const char* v = spirula::env("MOGE_F32_WEIGHTS");
        return v && v[0] && v[0] != '0';
    }();
    return !force_f32 && rank >= 2 && ends_with(name, ".weight");
}

// [in, out] -> [out, in]. Every Linear the exporter lowered to MatMul arrives
// as the `W` of `x @ W`, which is the transpose of what nn::linear indexes.
template <class Staged>
void transpose2d(Staged& s) {
    const int64_t in = s.shape[0], out = s.shape[1];
    std::vector<float> t((size_t)in * out);
    for (int64_t i = 0; i < in; ++i)
        for (int64_t o = 0; o < out; ++o)
            t[(size_t)(o * in + i)] = s.data[(size_t)(i * out + o)];
    s.data.swap(t);
    s.shape = {out, in};
}

// ConvTranspose2d's [Cin, Cout, 2, 2] -> the [Cout*4, Cin] matrix
// nn::conv_transpose2x2 multiplies by, with the four kernel taps as four
// output-channel groups.
template <class Staged>
void repack_conv_transpose(Staged& s) {
    const int64_t cin = s.shape[0], cout = s.shape[1];
    std::vector<float> dst((size_t)cin * cout * 4);
    for (int64_t ci = 0; ci < cin; ++ci)
        for (int64_t co = 0; co < cout; ++co)
            for (int64_t i = 0; i < 2; ++i)
                for (int64_t j = 0; j < 2; ++j)
                    dst[(size_t)((co * 4 + i * 2 + j) * cin + ci)] =
                        s.data[(size_t)(((ci * cout + co) * 2 + i) * 2 + j)];
    s.data.swap(dst);
    s.shape = {cout * 4, cin};
}

int block_index(const std::string& tensor_name) {
    const size_t at = tensor_name.find("blocks.");
    if (at == std::string::npos) return -1;
    return std::atoi(tensor_name.c_str() + at + std::strlen("blocks."));
}

}  // namespace

Weights::~Weights() {
    for (nn::DevicePtr p : chunks_) vk::device_free(p);
}

void Weights::stageOnnx(const std::string& onnx_path, std::vector<Staged>& staged) {
    const char* path = onnx_path.c_str();

    // ---- pass 1: structure ------------------------------------------------
    // The taps are invisible in the weight list -- every one shares the same
    // final LayerNorm -- so what names them is each `/encoder/norm*` node's input.
    const OnnxFile graph = nn::read_onnx_structure(onnx_path);

    std::unordered_map<std::string, std::string> matmul_module;  // init name -> module
    std::vector<int> taps;
    for (const OnnxNode& n : graph.nodes) {
        if (n.op_type == "MatMul" && n.inputs.size() == 2 && ends_with(n.name, "/MatMul")) {
            const std::string mod = n.name.substr(0, n.name.size() - std::strlen("/MatMul"));
            matmul_module[n.inputs[1]] = normalize(mod);
        }
        if (n.op_type == "ReduceMean" && n.inputs.size() >= 1 &&
            n.name.compare(0, std::strlen("/encoder/norm"), "/encoder/norm") == 0) {
            const int b = block_index(n.inputs[0]);
            if (b >= 0 && std::find(taps.begin(), taps.end(), b) == taps.end())
                taps.push_back(b);
        }
    }
    std::sort(taps.begin(), taps.end());
    NN_CHECK(!taps.empty() && taps.size() <= 4,
             "'%s' taps %zu encoder blocks; MoGe-2 taps 2 or 4", path, taps.size());
    hp_.n_taps = (int)taps.size();
    for (size_t i = 0; i < taps.size(); ++i) hp_.taps[i] = taps[i];

    // ---- pass 2: initializers, one at a time ------------------------------
    // Streamed rather than collected: vit-large is 1.3 GB of fp32 initializers
    // and `read_onnx` would hold all of it at once on top of the staged copy.
    std::vector<float> image_mean, image_std;
    int n_skipped = 0;
    nn::read_onnx_initializers(onnx_path, [&](OnnxTensor&& t) {
        if (t.name == "encoder.image_mean") { image_mean = std::move(t.data); return; }
        if (t.name == "encoder.image_std")  { image_std = std::move(t.data); return; }

        auto mm = matmul_module.find(t.name);
        if (mm == matmul_module.end() && t.name.compare(0, 6, "onnx::") == 0) {
            // The graph's own constants. Two are weights in everything but
            // name -- the positional embedding, split into a [1, grid*grid, D]
            // patch part and a [1, 1, D] class entry; the rest are shapes.
            if (t.shape.size() == 3 && t.shape[0] == 1) {
                if (t.shape[1] == 1 && cls_pos_.empty()) {
                    cls_pos_ = std::move(t.data);
                    return;
                }
                const int64_t g2 = t.shape[1];
                const int grid = (int)std::lround(std::sqrt((double)g2));
                if (g2 > 1 && (int64_t)grid * grid == g2 && patch_pos_.empty()) {
                    hp_.pos_grid = grid;
                    patch_pos_ = std::move(t.data);
                    return;
                }
            }
            ++n_skipped;
            return;
        }

        Staged s;
        s.name = (mm != matmul_module.end()) ? mm->second + ".weight" : normalize(t.name);
        s.shape = std::move(t.shape);
        s.data = std::move(t.data);
        if (mm != matmul_module.end()) {
            NN_CHECK(s.shape.size() == 2, "'%s': linear weight '%s' has rank %zu", path,
                     s.name.c_str(), s.shape.size());
            transpose2d(s);
        } else if (s.shape.size() == 4 && s.shape[2] == 2 && s.shape[3] == 2) {
            NN_CHECK(s.name.find(".resamplers.") != std::string::npos,
                     "'%s': '%s' is a 2x2 kernel outside a resampler", path,
                     s.name.c_str());
            repack_conv_transpose(s);
        }
        // DINOv2 ties the class positional embedding to the class token, and the
        // vit-small export deduplicated the two into this one initializer.
        // vit-base and vit-large keep both, so this is only the fallback.
        if (s.name == "encoder.cls_token" && cls_fallback_.empty()) cls_fallback_ = s.data;
        s.f16 = wants_f16(s.name, s.shape.size());
        staged.push_back(std::move(s));
    });
    NN_CHECK(!patch_pos_.empty(), "'%s' has no positional embedding constant", path);
    if (cls_pos_.empty()) cls_pos_.swap(cls_fallback_);
    if (image_mean.size() == 3 && image_std.size() == 3)
        for (int i = 0; i < 3; ++i) {
            hp_.image_mean[i] = image_mean[(size_t)i];
            hp_.image_std[i] = image_std[(size_t)i];
        }
}

void Weights::stageTorch(const std::string& pt_path, std::vector<Staged>& staged) {
    const char* path = pt_path.c_str();
    const nn::TorchCheckpoint f(pt_path);

    // The taps are in the config, as the block indices DINOv2's
    // get_intermediate_layers takes; nothing in the weights says which.
    const std::vector<double> taps = f.config("model_config.encoder.intermediate_layers");
    NN_CHECK(taps.size() >= 1 && taps.size() <= 4,
             "'%s' has no model_config.encoder.intermediate_layers list", path);
    hp_.n_taps = (int)taps.size();
    for (size_t i = 0; i < taps.size(); ++i) hp_.taps[i] = (int)taps[i];

    const std::vector<double> pool = f.config("model_config.refiner.downsample_factors");
    for (size_t i = 0; i < pool.size() && i < (size_t)Hparams::kMaxRefinerLevels; ++i)
        hp_.refiner_pool[i] = (int)pool[i];
    const std::vector<double> bins = f.config("model_config.refiner_depth_resolution");
    if (bins.size() == 1) hp_.refiner_depth_resolution = (float)bins[0];

    for (const std::string& raw : f.names()) {
        const std::string name = normalize(raw);
        if (name == "encoder.mask_token") continue;   // training only
        nn::OnnxTensor t = f.read(raw);
        if (name == "encoder.image_mean" || name == "encoder.image_std") {
            NN_CHECK(t.data.size() == 3, "'%s': '%s' holds %zu values", path, name.c_str(),
                     t.data.size());
            float* dst = name == "encoder.image_mean" ? hp_.image_mean : hp_.image_std;
            for (int i = 0; i < 3; ++i) dst[i] = t.data[(size_t)i];
            continue;
        }
        // [1, 1 + grid^2, D]: the class entry, then the patch grid.
        if (name == "encoder.pos_embed") {
            NN_CHECK(t.shape.size() == 3 && t.shape[0] == 1 && t.shape[1] > 1,
                     "'%s': unexpected positional embedding shape", path);
            const int64_t D = t.shape[2], g2 = t.shape[1] - 1;
            hp_.pos_grid = (int)std::lround(std::sqrt((double)g2));
            NN_CHECK((int64_t)hp_.pos_grid * hp_.pos_grid == g2,
                     "'%s': %lld positional entries are not a square grid", path,
                     (long long)g2);
            cls_pos_.assign(t.data.begin(), t.data.begin() + D);
            patch_pos_.assign(t.data.begin() + D, t.data.end());
            continue;
        }

        Staged s;
        s.name = name;
        s.shape = std::move(t.shape);
        s.data = std::move(t.data);
        if (s.shape.size() == 4 && s.shape[2] == 2 && s.shape[3] == 2) {
            NN_CHECK(s.name.find(".resamplers.") != std::string::npos,
                     "'%s': '%s' is a 2x2 kernel outside a resampler", path, s.name.c_str());
            repack_conv_transpose(s);
        } else if (s.shape.size() == 4 && s.shape[2] == 1 && s.shape[3] == 1) {
            // A 1x1 convolution is a Linear; the ONNX export already had it 2-D.
            s.shape = {s.shape[0], s.shape[1]};
        } else if (s.shape.size() == 5) {
            // FlexGEMM's [Cout, 3, 3, 3, Cin] is already the [Cout, 27*Cin]
            // matrix a sparse convolution multiplies its im2col by.
            s.shape = {s.shape[0], s.shape[1] * s.shape[2] * s.shape[3] * s.shape[4]};
        }
        // An odd row would split the GEMM's f16 word pairs across rows.
        s.f16 = wants_f16(s.name, s.shape.size()) && s.shape.back() % 2 == 0;
        staged.push_back(std::move(s));
    }
    NN_CHECK(!patch_pos_.empty(), "'%s' has no encoder.backbone.pos_embed", path);
}

void Weights::load(const std::string& path_in) {
    NN_CHECK(!loaded_, "moge::Weights::load called twice");
    const char* path = path_in.c_str();
    std::vector<Staged> staged;
    if (ends_with(path_in, ".pt")) stageTorch(path_in, staged);
    else stageOnnx(path_in, staged);
    NN_CHECK(!staged.empty(), "'%s' holds no MoGe weights", path);
    NN_CHECK(!cls_pos_.empty(), "'%s' has no class positional embedding", path);

    // ---- upload, chunked --------------------------------------------------
    uint64_t cursor = 0, chunk_used = 0;
    std::vector<uint64_t> chunk_sizes;
    std::vector<std::pair<uint32_t, uint64_t>> placement(staged.size());
    for (size_t i = 0; i < staged.size(); ++i) {
        const uint64_t bytes = (uint64_t)staged[i].data.size() * (staged[i].f16 ? 2u : 4u);
        chunk_used = align_up(chunk_used, kAlign);
        if (chunk_used != 0 && chunk_used + bytes > kChunkBytes) {
            chunk_sizes.push_back(chunk_used);
            chunk_used = 0;
        }
        placement[i] = {(uint32_t)chunk_sizes.size(), chunk_used};
        chunk_used += bytes;
        cursor += bytes;
    }
    if (chunk_used) chunk_sizes.push_back(chunk_used);
    for (size_t i = 0; i < chunk_sizes.size(); ++i)
        chunks_.push_back(vk::device_alloc(chunk_sizes[i], "moge-weights"));
    device_bytes_ = cursor;

    for (size_t i = 0; i < staged.size(); ++i) {
        Staged& s = staged[i];
        NN_CHECK(s.shape.size() <= 4, "'%s': '%s' has rank %zu; nn::Tensor holds 4", path,
                 s.name.c_str(), s.shape.size());
        Tensor t;
        t.ptr = chunks_[placement[i].first] + placement[i].second;
        t.dtype = s.f16 ? DType::F16 : DType::F32;
        t.ndim = (int32_t)std::max<size_t>(s.shape.size(), 1);
        for (size_t d = 0; d < s.shape.size(); ++d) t.shape[d] = s.shape[d];
        if (s.shape.empty()) t.shape[0] = 1;
        nn::tensor_from_host(t, s.data.data(), (int64_t)s.data.size());
        tensors_[s.name] = t;
        std::vector<float>().swap(s.data);
    }
    vk::Stream::get().sync();

    // ---- hyperparameters, from shapes -------------------------------------
    {
        const Tensor pe = get("encoder.patch_embed.proj.weight");
        hp_.embed_dim = (int)pe.shape[0];
        hp_.patch = (int)pe.shape[2];
        NN_CHECK(pe.shape[2] == pe.shape[3], "'%s': non-square patch embedding", path);
        // DINOv2 keeps head_dim at 64 for every width (384/6, 768/12, 1024/16)
        // and the export records the head count nowhere.
        NN_CHECK(hp_.embed_dim % 64 == 0, "'%s': embed_dim %d is not a multiple of 64",
                 path, hp_.embed_dim);
        hp_.num_heads = hp_.embed_dim / 64;
        hp_.mlp_hidden = (int)get("encoder.blocks.0.mlp.fc1.weight").shape[0];

        hp_.depth = 0;
        while (has("encoder.blocks." + std::to_string(hp_.depth) + ".norm1.weight"))
            ++hp_.depth;
        NN_CHECK(hp_.depth > 0, "'%s' has no encoder blocks", path);
        NN_CHECK(hp_.taps[hp_.n_taps - 1] == hp_.depth - 1,
                 "'%s': the last tapped block is %d of %d", path, hp_.taps[hp_.n_taps - 1],
                 hp_.depth);
        for (int i = 0; i < hp_.n_taps; ++i)
            NN_CHECK(has("encoder.output_projections." + std::to_string(i) + ".weight"),
                     "'%s' taps %d blocks but has no output_projections.%d", path,
                     hp_.n_taps, i);
        NN_CHECK(!has("encoder.output_projections." + std::to_string(hp_.n_taps) + ".weight"),
                 "'%s' has more output projections than tapped blocks", path);
        NN_CHECK(!has("encoder.register_tokens"),
                 "'%s' carries register tokens; MoGe-2 uses a DINOv2 without them", path);

        NN_CHECK((int64_t)patch_pos_.size() ==
                     (int64_t)hp_.pos_grid * hp_.pos_grid * hp_.embed_dim,
                 "'%s': positional embedding is %zu for a %dx%d grid at width %d", path,
                 patch_pos_.size(), hp_.pos_grid, hp_.pos_grid, hp_.embed_dim);
        NN_CHECK((int64_t)cls_pos_.size() == hp_.embed_dim,
                 "'%s': class token is %zu wide, not %d", path, cls_pos_.size(),
                 hp_.embed_dim);

        for (int l = 0; l < Hparams::kLevels; ++l)
            hp_.ch[l] = (int)getf("neck.input_blocks.%d.weight", l).shape[0];
        NN_CHECK(!has("neck.input_blocks." + std::to_string(Hparams::kLevels) + ".weight"),
                 "'%s' has more than %d feature levels", path, Hparams::kLevels);
        NN_CHECK(hp_.ch[0] == hp_.embed_dim,
                 "'%s': the neck's first level is %d wide, not the encoder's %d", path,
                 hp_.ch[0], hp_.embed_dim);
        // The level-0 input is the encoder feature with the view-plane uv
        // concatenated; every finer level is the uv alone.
        NN_CHECK(get("neck.input_blocks.0.weight").shape[1] == hp_.embed_dim + 2,
                 "'%s': the neck's first level reads %lld channels, not %d", path,
                 (long long)get("neck.input_blocks.0.weight").shape[1], hp_.embed_dim + 2);
        for (int l = 1; l < Hparams::kLevels; ++l)
            NN_CHECK(getf("neck.input_blocks.%d.weight", l).shape[1] == 2,
                     "'%s': neck level %d reads %lld channels, not the 2 uv ones", path, l,
                     (long long)getf("neck.input_blocks.%d.weight", l).shape[1]);

        hp_.has_normal = has("normal_head.output_blocks.4.weight");
        hp_.has_mask = has("mask_head.output_blocks.4.weight");
        hp_.has_scale = has("scale_head.4.weight");
        NN_CHECK(has("points_head.output_blocks.4.weight"), "'%s' has no points head", path);
        NN_CHECK(get("points_head.output_blocks.4.weight").shape[0] == 3,
                 "'%s': the points head emits %lld channels, not 3", path,
                 (long long)get("points_head.output_blocks.4.weight").shape[0]);
        NN_CHECK(!hp_.has_normal || get("normal_head.output_blocks.4.weight").shape[0] == 3,
                 "'%s': the normal head emits %lld channels, not 3", path,
                 (long long)get("normal_head.output_blocks.4.weight").shape[0]);
        NN_CHECK(!hp_.has_mask || get("mask_head.output_blocks.4.weight").shape[0] == 1,
                 "'%s': the mask head emits %lld channels, not 1", path,
                 (long long)get("mask_head.output_blocks.4.weight").shape[0]);
    }

    // ---- the refiner, when there is one -----------------------------------
    hp_.has_refiner = has("refiner.input_proj.weight");
    if (hp_.has_refiner) {
        int levels = 1;
        while (has("refiner.downsample_blocks." + std::to_string(levels - 1) + ".linear.weight"))
            ++levels;
        NN_CHECK(levels >= 2 && levels <= Hparams::kMaxRefinerLevels,
                 "'%s': the refiner has %d levels", path, levels);
        hp_.refiner_levels = levels;
        hp_.refiner_ch[0] = (int)get("refiner.input_proj.weight").shape[0];
        int span = 1;
        for (int i = 0; i + 1 < levels; ++i) {
            const Tensor w = getf("refiner.downsample_blocks.%d.linear.weight", i);
            NN_CHECK(w.shape[1] == hp_.refiner_ch[i],
                     "'%s': refiner level %d is %d wide but its pooling reads %lld", path, i,
                     hp_.refiner_ch[i], (long long)w.shape[1]);
            hp_.refiner_ch[i + 1] = (int)w.shape[0];
            if (hp_.refiner_pool[i] == 0) hp_.refiner_pool[i] = 2;
            span *= hp_.refiner_pool[i];
        }
        // The bottleneck samples the patch-grid feature map at its own coords,
        // so the pooling has to come out at the patch grid exactly.
        NN_CHECK(span == 1 << (Hparams::kLevels - 1),
                 "'%s': the refiner pools by %d, not the head's %d", path, span,
                 1 << (Hparams::kLevels - 1));
        NN_CHECK(get("refiner.encoder_fuse.weight").shape[1] == hp_.embed_dim + 2,
                 "'%s': the refiner fuses %lld encoder channels, not %d", path,
                 (long long)get("refiner.encoder_fuse.weight").shape[1], hp_.embed_dim + 2);
        NN_CHECK(get("refiner.input_proj.weight").shape[1] == 3 &&
                     get("refiner.out_proj.weight").shape[0] == 1,
                 "'%s': the refiner does not map (u, v, log z) to one log-depth delta", path);
    }

    path_ = path_in;
    loaded_ = true;
    std::string tap_list;
    for (int i = 0; i < hp_.n_taps; ++i)
        tap_list += (i ? "," : "") + std::to_string(hp_.taps[i]);
    NN_LOG_INFO("[moge] %s: dim=%d depth=%d heads=%d hidden=%d, taps %s, levels "
                "%d/%d/%d/%d/%d%s%s%s%s, %zu tensors, %.2f MB on device in %zu chunks\n",
                path, hp_.embed_dim, hp_.depth, hp_.num_heads, hp_.mlp_hidden,
                tap_list.c_str(), hp_.ch[0], hp_.ch[1], hp_.ch[2], hp_.ch[3], hp_.ch[4],
                hp_.has_normal ? " +normal" : "", hp_.has_mask ? " +mask" : "",
                hp_.has_scale ? " +scale" : "", hp_.has_refiner ? " +refiner" : "",
                tensors_.size(), (double)device_bytes_ / 1e6, chunks_.size());
}

int Weights::refinerBlocks(const std::string& stage) const {
    int n = 0;
    while (has("refiner." + stage + "." + std::to_string(n) + ".norm1.weight")) ++n;
    return n;
}

int Weights::resBlocks(const char* stack, int level) const {
    int n = 0;
    while (has(std::string(stack) + ".res_blocks." + std::to_string(level) + "." +
               std::to_string(n) + ".layers.2.weight"))
        ++n;
    return n;
}

nn::Tensor Weights::get(const std::string& name) const {
    auto it = tensors_.find(name);
    if (it != tensors_.end()) return it->second;

    // Near misses: the failure is almost always a name that drifted, and
    // printing the neighbourhood turns a five-minute hunt into a glance.
    std::vector<std::string> misses;
    const size_t dot = name.rfind('.');
    for (const auto& kv : tensors_)
        if (dot != std::string::npos && kv.first.compare(0, dot, name, 0, dot) == 0)
            misses.push_back(kv.first);
    std::sort(misses.begin(), misses.end());
    std::string hint;
    for (size_t i = 0; i < misses.size() && i < 8; ++i) hint += "\n    " + misses[i];
    nn::fail("no weight named '%s'%s%s", name.c_str(),
             hint.empty() ? "" : " (did you mean one of these?)", hint.c_str());
}

nn::Tensor Weights::getf(const char* fmt, ...) const {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return get(buf);
}

}  // namespace moge
