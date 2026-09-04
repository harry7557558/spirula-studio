#pragma once
// A LoMa checkpoint: an .onnx file in, named device tensors and the shape of
// the network out.
//
// Nothing is folded here, unlike aliked/model/Weights.h: these exports have
// BatchNorm already folded into the convolutions before them, so a conv
// carries a bias and there is no `running_var` in the file to look for.
//
// What IS here is the hyperparameter derivation. The detector and the two
// descriptors are the same network at different widths, depths and pyramid
// heights, and every difference is visible in the tensor shapes -- see
// PyramidHparams -- so there is one code path and the variant is only a URL.

#include "loma/Common.h"
#include "nn/Tensor.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace loma {

// One ConvRefiner: a 1x1 in-projection, `blocks` depthwise-5x5 + 1x1 pairs,
// and a 1x1 out-projection that emits `head` channels of result followed by
// `ctx` channels of context for the level below.
struct RefinerSpec {
    int     scale = 0;      // 14, 8, 4, 2 or 1 -- the name in the checkpoint
    int64_t in = 0, hidden = 0, out = 0;
    int     blocks = 0;
    int64_t ctx = 0;        // out - head, 0 at the finest level
};

// Read off the checkpoint's tensor shapes; see the class comment.
struct PyramidHparams {
    // The VGG conv indices, in order, and which of them a feature map is
    // tapped after. A tap sits before every MaxPool, and a MaxPool is what a
    // gap of 4 (rather than 3) between consecutive conv indices means.
    std::vector<int>  conv_index;
    std::vector<int>  tap_at;         // indices into conv_index
    std::vector<int64_t> tap_width;   // channels of each tap, coarse LAST
    std::vector<RefinerSpec> refiners;   // coarse first
    int64_t head = 0;                 // 1 for the detector, 128/256 for a descriptor
    // Where the VGG convolutions live. DeDoDe-B's encoder IS the VGG, so they
    // sit at "<prefix>.encoder.layers."; DeDoDe-G's has a DINOv2 beside it and
    // they move down to "<prefix>.encoder.vgg.layers.".
    std::string vgg_prefix;
    // The frozen DINOv2 that supplies DeDoDe-G's coarsest level, all read off
    // the file. Zero width means the checkpoint has none.
    std::string dino_prefix;
    int64_t dino_width = 0;
    int64_t dino_mlp = 0;
    int     dino_blocks = 0;
    int     dino_heads = 0;
    int     dino_patch = 0;
    int64_t dino_pos_grid = 0;        // side of the positional embedding's grid
    int64_t input_size = 0;           // fixed square the export demands, 0 = any
};

class PyramidWeights {
public:
    PyramidWeights() = default;
    ~PyramidWeights();
    PyramidWeights(const PyramidWeights&) = delete;
    PyramidWeights& operator=(const PyramidWeights&) = delete;

    // `prefix` is "det" or "desc". Throws nn::Error naming the tensor on any
    // missing or unexpectedly shaped weight, so a checkpoint that is not the
    // one asked for fails here with a sentence rather than later with a fault.
    void load(const std::string& onnx_path, const char* prefix);

    bool loaded() const { return loaded_; }
    const PyramidHparams& hparams() const { return hp_; }
    const std::string&    path() const { return path_; }
    uint64_t              deviceBytes() const { return device_bytes_; }

    nn::Tensor get(const std::string& name) const;
    nn::Tensor getf(const char* fmt, ...) const;
    bool       has(const std::string& name) const { return tensors_.count(name) != 0; }
    // The ImageNet normalization the detector applies; {0,0,0}/{1,1,1} when the
    // checkpoint carries none, which is what the descriptors do.
    const float* mean() const { return mean_; }
    const float* std() const { return std_; }
    // DINOv2's positional embedding, kept on the host: it is resampled to the
    // token grid, which the image size decides, so uploading it as-is would be
    // uploading the wrong thing.
    const std::vector<float>& dinoPatchPos() const { return dino_patch_pos_; }
    const std::vector<float>& dinoClsPos() const { return dino_cls_pos_; }

private:
    std::unordered_map<std::string, nn::Tensor> tensors_;
    PyramidHparams hp_;
    std::string    path_;
    nn::DevicePtr  blob_ = 0;
    uint64_t       device_bytes_ = 0;
    float          mean_[3] = {0, 0, 0};
    float          std_[3] = {1, 1, 1};
    std::vector<float> dino_patch_pos_;
    std::vector<float> dino_cls_pos_;
    bool           loaded_ = false;
};

// Nine transformer layers plus one assignment head, and an input projection
// only when the descriptor is narrower than the embedding. Every dimension is
// read off the file, so the five released variants are one code path.
struct MatcherHparams {
    int64_t input_dim = 0;    // what desc0 / desc1 must be
    int64_t embed_dim = 0;
    int     n_layers = 0;
    int     n_heads = 0;      // embed_dim / 64, the one thing not in a shape
    int     assign_layer = 0; // which log_assignment head the export runs
    bool    input_proj = false;
};

class MatcherWeights {
public:
    MatcherWeights() = default;
    ~MatcherWeights();
    MatcherWeights(const MatcherWeights&) = delete;
    MatcherWeights& operator=(const MatcherWeights&) = delete;

    void load(const std::string& onnx_path);
    bool loaded() const { return loaded_; }
    const MatcherHparams& hparams() const { return hp_; }
    const std::string&    path() const { return path_; }
    uint64_t              deviceBytes() const { return device_bytes_; }

    nn::Tensor get(const std::string& name) const;
    nn::Tensor getf(const char* fmt, ...) const;
    bool       has(const std::string& name) const { return tensors_.count(name) != 0; }

private:
    std::unordered_map<std::string, nn::Tensor> tensors_;
    MatcherHparams hp_;
    std::string    path_;
    nn::DevicePtr  blob_ = 0;
    uint64_t       device_bytes_ = 0;
    bool           loaded_ = false;
};

}  // namespace loma
