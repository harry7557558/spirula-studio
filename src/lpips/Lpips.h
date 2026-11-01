#pragma once
// LPIPS (Zhang et al. 2018) on the inference layer, computed as torchmetrics'
// LearnedPerceptualImagePatchSimilarity does: torchvision's AlexNet or VGG16
// features and the LPIPS v0.1 linear layers, in fp32. src/lpips/README.md.

#include <cstdint>
#include <string>
#include <vector>

namespace lpips {

enum class Net { Alex, Vgg };
// torchmetrics' net_type: "alex", "vgg".
const char* net_name(Net net);

struct Checkpoints {
    std::string backbone;   // torchvision's ImageNet weights; only features.* is read
    std::string linear;     // LPIPS v0.1's lin0..lin4
};

struct ScoreOptions {
    // Device memory a tile may take; 0: half what the device has free, at most
    // 3 GiB. A view that needs more is scored in tiles, with the same result.
    uint64_t workspace_bytes = 0;
    // Tile side in input pixels, rounded up to 16; 0: the largest that fits.
    int tile = 0;
};

// Owns one net's weights and its workspace. Not thread-safe; scorers share the
// process-wide inference device. Throws nn::Error.
class Scorer {
public:
    Scorer();
    ~Scorer();
    Scorer(const Scorer&) = delete;
    Scorer& operator=(const Scorer&) = delete;

    void load(Net net, const Checkpoints& files);
    void unload();
    bool loaded() const;

    // LPIPS of each of `preds` against `ref`, all [height, width, 3] floats in
    // HWC order. `normalize` is torchmetrics' flag: true takes the images as
    // [0, 1] and maps them to [-1, 1] first.
    std::vector<double> score(const float* ref, const std::vector<const float*>& preds,
                              int width, int height, bool normalize,
                              const ScoreOptions& opts = {});

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace lpips
