#pragma once
// The loaded model and the two halves of its forward pass.
//
// Split across Encoder.cpp and Head.cpp because they are two different networks
// bolted together -- a DINOv2 ViT and a five-level convolutional stack -- and
// each is meant to be read against its own PyTorch source (dinov2's
// vision_transformer.py, and ConvStack in MoGe's modules.py).
//
// There is no computation graph: an op runs when it is called and writes into
// an arena tensor. The arena is a bump allocator that refuses to grow while
// anything is live, so planArenaBytes() has to bound the whole pass up front.

#include "moge/model/Weights.h"
#include "nn/Tensor.h"
#include "nn/vk/Memory.h"

#include <string>
#include <vector>

namespace moge {

// What the encoder hands the neck: the summed projected features on the patch
// grid, and the class token of the last tap, which is all the scale head reads.
struct Features {
    nn::Tensor map;      // [gh, gw, embed_dim]
    nn::Tensor cls;      // [1, embed_dim]
    int64_t    gh = 0, gw = 0;
};

// The head outputs at the caller's resolution, before the host applies the
// exp remap, the normalization and the sigmoid. A null request is not computed.
struct Outputs {
    nn::Tensor points;   // [H, W, 3]
    nn::Tensor normal;   // [H, W, 3]
    nn::Tensor mask;     // [H, W, 1] logits
    nn::Tensor scale;    // [1, 1] metres per unit; empty without a scale head
};

struct Model {
    Weights       weights;
    nn::vk::Arena arena{"moge"};
    // The refiner's own, sized per step once the voxels are counted: how many
    // there are depends on the depth map, so planArenaBytes cannot bound it.
    nn::vk::Arena refine_arena{"moge-refine"};

    // The positional embedding resampled for one patch grid, and the five
    // levels' view-plane uv grids. Both are host work done once per input size
    // rather than per image, and a dataset is usually one size.
    nn::DevicePtr pos_blob = 0;
    nn::Tensor    pos_patch;                     // [gh*gw, D]
    nn::Tensor    pos_cls;                       // [D]
    int64_t       pos_gh = 0, pos_gw = 0;
    nn::DevicePtr uv_blob = 0;
    nn::Tensor    uv[Hparams::kLevels];          // [gh<<l, gw<<l, 2]
    int64_t       uv_gh = 0, uv_gw = 0;
    double        uv_aspect = 0.0;

    ~Model();

    const Hparams& hp() const { return weights.hparams(); }

    // `image` is [ih, iw, 3] f32 on the device, already ImageNet-normalized,
    // with ih/iw exact multiples of the patch stride.
    Features encode(const nn::Tensor& image);

    // Runs the neck and the requested heads and resizes each to [H, W]. With a
    // refiner, the points head's raw map is refined `refine_steps` times first.
    void decode(const Features& f, int64_t H, int64_t W, bool want_points,
                bool want_normal, bool want_mask, Outputs* out, int refine_steps = 0);

    // MoGe-3's sparse 3D refinement of `raw`, the points head's [Hf, Wf, 3]
    // (u, v, log z) map at 16x the patch grid, in place. Each step moves log z
    // only. Refiner.cpp.
    void refine(const Features& f, const nn::Tensor& raw, int steps);

    void ensurePosEmbed(int64_t gh, int64_t gw);
    void ensureUv(int64_t gh, int64_t gw, double aspect);

    uint64_t planArenaBytes(int64_t gh, int64_t gw, int64_t H, int64_t W) const;
};

// SS_MOGE_DUMP=<dir> writes one .npy per stage for tools/moge/compare_ort.py.
// Off unless the variable is set; the check is a getenv on the first call and a
// null pointer test after that.
bool dump_enabled();
void dump_tensor(const char* name, const nn::Tensor& t,
                 const std::vector<int64_t>& shape);

}  // namespace moge
