#pragma once
// Swin Transformer v1, the backbone BiRefNet and Grounding DINO share, on the
// inference layer. The two checkpoints spell its weights differently -- the
// Microsoft original (BiRefNet) and Hugging Face's port (Grounding DINO) --
// so `stage_weights` maps either onto one set of names, and the forward pass
// reads like the original's swin_transformer.py. src/swin/README.md.

#include "nn/Tensor.h"
#include "nn/WeightStore.h"
#include "nn/io/Onnx.h"
#include "nn/vk/Memory.h"

#include <functional>
#include <map>
#include <string>
#include <tuple>

namespace swin {

struct Config {
    int embed_dim = 96;
    int depths[4] = {2, 2, 6, 2};
    int heads[4] = {3, 6, 12, 24};
    int window = 7;
    // Hugging Face shrinks the window to the map (and drops the shift) once a
    // stage is no bigger than one window; the original pads instead.
    bool clamp_window = false;
    // The stages whose (normed) output the caller wants.
    bool out[4] = {true, true, true, true};

    int dim(int stage) const { return embed_dim << stage; }
};

enum class Naming { Microsoft, HuggingFace };

// Reads a tensor by its checkpoint name; throws when absent.
using Reader = std::function<nn::OnnxTensor(const std::string&)>;
using Has = std::function<bool(const std::string&)>;

// Infers depths, heads and embed_dim from the checkpoint; `window` and
// `clamp_window` are not in the tensors and come from the caller.
Config infer_config(Naming naming, const std::string& prefix, const Has& has,
                    const Reader& read, int window);

// Stages every backbone tensor under "<dst>." in the store, fused and expanded
// for the forward pass: q/k/v into one [3C, C] projection, the relative
// position table into a dense [heads, N, N] bias, matrices as f16.
void stage_weights(nn::WeightStore& store, const std::string& dst, const Config& cfg,
                   Naming naming, const std::string& src_prefix, const Reader& read,
                   bool f16);

struct Features {
    nn::Tensor map[4];   // [h, w, dim(stage)], normed; empty for stages not asked for
    int h[4] = {}, w[4] = {};
};

class Backbone {
public:
    Backbone() = default;
    ~Backbone();
    Backbone(const Backbone&) = delete;
    Backbone& operator=(const Backbone&) = delete;

    void bind(const nn::WeightStore& store, const std::string& prefix, const Config& cfg);
    bool bound() const { return store_ != nullptr; }
    const Config& config() const { return cfg_; }

    // `image` is [H, W, 3] f32, already normalized; H and W need not divide 4.
    // The feature maps are allocated from `arena` and stay live on return.
    Features forward(nn::vk::Arena& arena, const nn::Tensor& image, int H, int W);

    // Bytes of arena the pass needs at this input size, features included.
    uint64_t arenaBytes(int H, int W) const;

    // Drops the cached shifted-window labels and forgets the store.
    void release();

private:
    nn::Tensor labels(int hp, int wp, int ws, int shift);

    const nn::WeightStore* store_ = nullptr;
    std::string prefix_;
    Config cfg_;
    std::map<std::tuple<int, int, int, int>, std::pair<nn::DevicePtr, nn::Tensor>> labels_;
};

}  // namespace swin
