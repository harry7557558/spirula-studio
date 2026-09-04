#pragma once
// The two frozen halves of every LoMa variant, behind the public Extractor.
//
// DaD detects on the image at its own resolution; DeDoDe describes on a fixed
// square the export baked in, and the keypoints that travel between them are
// normalized, so neither size reaches a coordinate.

#include "loma/model/Pyramid.h"

#include <string>
#include <vector>

namespace loma {

struct DetectorOut {
    std::vector<float> xy;      // [n, 2] normalized (x, y) in [-1, 1]
    std::vector<float> score;   // [n]
};

class Detector {
public:
    ~Detector();
    void load(const std::string& path, vk::Arena* arena);
    bool loaded() const { return pyr_.w.loaded(); }
    const float* mean() const { return pyr_.w.mean(); }
    const float* std() const { return pyr_.w.std(); }
    int64_t planBytes(int64_t H, int64_t W) const;

    // `img` is the normalized [H, W, 3] device tensor.
    DetectorOut run(const nn::Tensor& img, int64_t H, int64_t W, int max_features,
                    float min_score, uint32_t cap);

private:
    Pyramid       pyr_;
    nn::DevicePtr offsets_ = 0;   // the [2, 9] soft-argmax offset matrix
};

// DeDoDe-G's frozen DINOv2 ViT-L/14. Holds only the resampled positional
// embedding: everything else it needs is in the PyramidWeights beside it.
class Dino {
public:
    ~Dino() { release(); }
    void release();
    void run(const PyramidWeights& w, vk::Arena& arena, const nn::Tensor& image,
             const nn::Tensor& map, int64_t gh, int64_t gw);
    static int64_t planBytes(const PyramidHparams& hp, int64_t S);

private:
    void ensurePosEmbed(const PyramidWeights& w, int64_t gh, int64_t gw);

    nn::DevicePtr pos_blob_ = 0;
    nn::Tensor    pos_patch_, pos_cls_;
    int64_t       pos_gh_ = 0, pos_gw_ = 0;
};

class Descriptor {
public:
    void load(const std::string& path, vk::Arena* arena);
    bool loaded() const { return pyr_.w.loaded(); }
    int64_t inputSize() const { return pyr_.hp().input_size; }
    int     dim() const { return (int)pyr_.hp().head; }
    int64_t planBytes() const;

    // `img` is [S, S, 3] in [0, 1]; `xy` is [n, 2] normalized. Writes n * dim.
    void run(const nn::Tensor& img, const float* xy, int n, float* out);

private:
    Pyramid pyr_;
    Dino    dino_;
};

}  // namespace loma
