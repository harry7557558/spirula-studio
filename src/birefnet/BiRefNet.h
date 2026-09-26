#pragma once
// BiRefNet (Zheng et al., "Bilateral Reference for High-Resolution Dichotomous
// Image Segmentation") on the inference layer: one image in, the mask of its
// main subject out, no prompt. src/birefnet/README.md.

#include "nn/io/Fetch.h"
#include "nn/io/Image.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace birefnet {

struct ModelSource {
    const char*   id;
    nn::FetchFile file;
};

const ModelSource* find_model_source(const std::string& id);
std::string model_id_list();
// A known id is fetched into the model cache; anything else must be a file.
// Throws nn::Error.
std::string resolve_model(const std::string& id_or_path);

// Whether `path` is a BiRefNet checkpoint, by its tensor names. Cheap: reads
// the safetensors header only.
bool is_checkpoint(const std::string& path);

class Predictor {
public:
    Predictor();
    ~Predictor();
    Predictor(const Predictor&) = delete;
    Predictor& operator=(const Predictor&) = delete;

    // An id from model_id_list() or a path. Throws nn::Error.
    void load(const std::string& id_or_path);
    bool loaded() const;
    // Returns the weights and the workspace to the device.
    void unload();
    uint64_t deviceBytes() const;

    // The subject's mask at the image's own size, 255 where the network's
    // probability exceeds `threshold`.
    std::vector<uint8_t> segment(const nn::Image& image, float threshold = 0.5f);

    // The network on an input that is already resized and normalized: `hwc` is
    // [S, S, 3] f32. Returns the [S, S] logits. For parity checks.
    std::vector<float> forwardNormalized(const std::vector<float>& hwc, int S);

    // The square side the network was trained at.
    static int inputSize() { return 1024; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace birefnet
