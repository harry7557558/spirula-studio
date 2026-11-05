#pragma once
// Marigold v2 (huawei-bayerlab): monocular depth and surface normals from
// Qwen-Image-Edit-2509, a 20B-parameter DiT fine-tuned to predict them in one
// step. The public surface of src/marigold/; README.md beside it has the design.
//
// Two conventions a caller has to know:
//   * Depth is affine in LOG depth: log z = a + b * depth_log for unknown a and
//     b > 0, -1 near and +1 far. app/GeometryModel.cpp resolves the two.
//   * Normals are unit vectors in OpenCV's camera frame (x right, y down, z
//     forward), facing the camera -- what the trainer reads.

#include "marigold/model/Fetch.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace marigold {

struct Prediction {
    int width = 0, height = 0;
    std::vector<float> depth_log;   // [H*W], empty unless asked for
    std::vector<float> normal;      // [H*W*3], empty unless asked for
};

struct PredictOptions {
    bool want_depth = true;
    bool want_normal = true;
    // DiT tokens per image, 16x16 pixels each, which is what sets the cost.
    // An image holding more is downscaled to this; one holding fewer runs at
    // its own size, rounded to a multiple of 16.
    int num_tokens = 3600;
};

class Predictor {
public:
    Predictor();
    ~Predictor();
    Predictor(const Predictor&) = delete;
    Predictor& operator=(const Predictor&) = delete;

    // Throws unless what is asked for is installed (model/Install.h): the
    // install is tens of GB, which the caller asks about first.
    void load(bool want_depth, bool want_normal);
    bool loaded() const;

    // width*height*3 floats in 0..1 (sRGB). Images that the network sees at
    // one size share passes (batchCapacity()); any mix of sizes is fine.
    struct Image {
        const float* rgb = nullptr;
        int width = 0, height = 0;
    };
    std::vector<Prediction> predict(const std::vector<Image>& images, const PredictOptions& opts);

    // Plans VRAM for images of this size and uploads the resident weights, so
    // the first predict() measures the network and not the upload.
    void prepare(int width, int height, int num_tokens);

    // How many images of this size one pass takes: more amortize the weights'
    // trip over PCIe, and the VRAM plan bounds it.
    int batchCapacity(int width, int height, int num_tokens);

    // The size the network sees an image of `width` x `height` at.
    static void processingSize(int width, int height, int num_tokens, int* pw, int* ph);
    uint64_t deviceBytes() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace marigold
