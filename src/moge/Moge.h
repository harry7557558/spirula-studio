#pragma once
// MoGe-2 / MoGe-3 monocular geometry: one image in, metric depth, surface
// normals and a validity mask out.
//
// The public surface of src/moge/. Two conventions are worth stating because
// getting either wrong is silent:
//
//   * The input must be a PINHOLE image, as for src/metric3d/. app/GeometryWarp.h
//     is how a distorted camera reaches this.
//   * The network predicts an AFFINE point map -- correct up to one unknown
//     z shift -- plus a metric scale. `depth` is what comes out after both are
//     resolved, in metres. Resolving the shift needs the camera's focal length;
//     without one it is recovered from the point map and is a guess.

#include <cstdint>
#include <string>
#include <vector>

namespace moge {

struct PredictOptions {
    bool want_depth = true;
    bool want_normal = true;
    bool want_mask = false;

    // ViT tokens, which is what the cost is set by rather than the image size.
    // MoGe's published range is 1200..3600; clamped down when the image holds
    // fewer patches than this asks for.
    int num_tokens = 3600;

    // The pinhole camera, in the image's own pixels; cx/cy default to centre.
    // With fx set the z shift is solved against it; left at 0 the focal is
    // recovered from the point map too, which is a guess.
    float fx = 0.0f, fy = 0.0f;
    float cx = -1.0f, cy = -1.0f;

    // Below this the pixel is written as the trainer's two "no ground truth
    // here" sentinels -- depth 0, black normal. MoGe's own threshold, and what
    // rejects sky.
    float mask_threshold = 0.5f;

    // MoGe-3's sparse 3D refinement passes; ignored by a checkpoint without a
    // refiner. 3 is what it was trained with and MoGe's default; 0 is the
    // unrefined base prediction.
    int refine_steps = 3;
};

struct Prediction {
    int width = 0, height = 0;
    // [H*W] z-depth in metres, 0 where the mask rejected the pixel.
    std::vector<float> depth;
    // [H*W*3] unit normals in the camera frame, 0 where rejected.
    std::vector<float> normal;
    // [H*W] the mask's probability, filled only when it was asked for.
    std::vector<float> mask;

    // What resolved the point map, kept for diagnostics: the scale head's
    // metres-per-unit, the z shift in the network's own units, and the focal
    // used, relative to half the image diagonal.
    float metric_scale = 1.0f;
    float shift = 0.0f;
    float focal = 0.0f;
};

// Owns a checkpoint and the working buffers for one image at a time. Not
// thread-safe: one Predictor per thread, and they share the process-wide
// inference device.
class Predictor {
public:
    Predictor();
    ~Predictor();
    Predictor(const Predictor&) = delete;
    Predictor& operator=(const Predictor&) = delete;

    // `model` is a known id ("moge2-vitb", "moge3-vitl"), which is fetched and
    // cached, or a path to an .onnx or .pt file. Throws nn::Error on anything
    // that is not a MoGe-2 or MoGe-3 checkpoint.
    void load(const std::string& model);
    bool loaded() const;

    // The ViT's patch stride (14). Unlike Metric3D there is no output
    // granularity: the prediction comes back at exactly the size that went in.
    static int patchSize();

    // `rgb` is width*height*3 floats in 0..1, row-major, interleaved.
    Prediction predict(const float* rgb, int width, int height,
                       const PredictOptions& opts = {});

    // Bytes this Predictor is holding on the device.
    uint64_t deviceBytes() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace moge
