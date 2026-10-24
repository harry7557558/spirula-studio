#pragma once
// A MoGe checkpoint: an .onnx export (MoGe-2) or a torch.save .pt (MoGe-3, which
// has no export) in, named device tensors out.
//
// Torch lowers nn.Linear to MatMul + Add, so in the ONNX file those matrices
// arrive anonymous ("onnx::MatMul_3480") and transposed, and it is the MatMul
// node's name that carries the module path. The scale head is the exception:
// it exports as Gemm with transB, which keeps both name and layout. The .pt
// holds module paths and [out, in] layouts as they are.
//
// Names are normalized on the way in -- `encoder.backbone.` loses its middle
// term, which is what the graph's node paths already spell -- so the forward
// pass asks for `encoder.blocks.11.norm1.weight` and reads like the source.

#include "moge/model/Hparams.h"
#include "nn/Tensor.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace moge {

class Weights {
public:
    Weights() = default;
    ~Weights();
    Weights(const Weights&) = delete;
    Weights& operator=(const Weights&) = delete;

    // A .pt is read as a torch checkpoint. Throws nn::Error naming the tensor
    // on any missing or misshapen weight -- a checkpoint that is not MoGe must
    // fail here with a sentence, not later with a fault.
    void load(const std::string& path);

    bool loaded() const { return loaded_; }
    const Hparams& hparams() const { return hp_; }
    const std::string& path() const { return path_; }
    uint64_t deviceBytes() const { return device_bytes_; }

    // Throws when absent, listing near misses: a typo in a weight name is
    // otherwise a null tensor that faults somewhere else entirely.
    nn::Tensor get(const std::string& name) const;
    // printf-style, for the per-block names ("encoder.blocks.%d.norm1.weight").
    nn::Tensor getf(const char* fmt, ...) const;
    bool has(const std::string& name) const { return tensors_.count(name) != 0; }

    // Residual blocks in one stack at one level. Per stack because vit-large's
    // neck runs two where its heads run one, and reading a shared count off the
    // neck asks the heads for weights they do not have.
    int resBlocks(const char* stack, int level) const;

    // The stored positional embedding, on the host: `patchPos` is
    // [pos_grid*pos_grid, embed_dim] and `clsPos` is [embed_dim]. The latter IS
    // `encoder.cls_token` -- see src/moge/README.md before calling that a bug.
    const std::vector<float>& patchPos() const { return patch_pos_; }
    const std::vector<float>& clsPos() const { return cls_pos_; }

    // Residual blocks in one stage of the refiner ("down_stages.2",
    // "bottleneck_stage"), counted off the file like resBlocks().
    int refinerBlocks(const std::string& stage) const;

private:
    struct Staged {
        std::string          name;
        std::vector<int64_t> shape;
        std::vector<float>   data;
        bool                 f16 = false;
    };
    void stageOnnx(const std::string& path, std::vector<Staged>& staged);
    void stageTorch(const std::string& path, std::vector<Staged>& staged);

    std::unordered_map<std::string, nn::Tensor> tensors_;
    Hparams                    hp_;
    std::string                path_;
    std::vector<nn::DevicePtr> chunks_;
    uint64_t                   device_bytes_ = 0;
    std::vector<float>         patch_pos_, cls_pos_, cls_fallback_;
    bool                       loaded_ = false;
};

}  // namespace moge
