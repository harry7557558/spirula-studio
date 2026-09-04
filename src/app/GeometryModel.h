#pragma once

// The one seam between `spirula geometry` and a monocular geometry network.
//
// Two families sit behind it and they answer different questions: Metric3D v2
// predicts depth directly, MoGe-2 predicts an affine point map that needs the
// camera's focal length to become one. Everything that difference touches --
// which input sizes round-trip, what a depth means in metres, whether a mask
// comes back -- is resolved here so the caller only sees depth and normals.

#include <string>
#include <vector>

namespace app {

struct GeometryRequest {
    int  width = 0, height = 0;
    bool want_depth = true;
    bool want_normal = true;
    // The pinhole face the frame was resampled into, in its own pixels.
    // app::GeometryWarp knows all four; a model that predicts depth ignores them.
    double fx = 0.0, fy = 0.0, cx = -1.0, cy = -1.0;
    // MoGe's ViT budget, which is what sets its cost. Per call rather than per
    // load, so changing it does not mean re-uploading the weights.
    int num_tokens = 3600;
};

struct GeometryPrediction {
    int width = 0, height = 0;
    // [H*W] depth and [H*W*3] unit normals, carrying the trainer's two
    // sentinels: 0 and black wherever the model had no answer.
    std::vector<float> depth;
    std::vector<float> normal;
};

class GeometryWarp;

// Face k of a planned warp: its size and the pinhole it is. Wanting both maps
// by default -- the caller turns off what it is not writing.
GeometryRequest face_request(const GeometryWarp& warp, int k, int num_tokens);

// One loaded checkpoint. Not thread-safe; both backends share the process-wide
// inference device.
class GeometryModel {
public:
    GeometryModel();
    ~GeometryModel();
    GeometryModel(const GeometryModel&) = delete;
    GeometryModel& operator=(const GeometryModel&) = delete;

    // `id_or_path` picks the family: a known id by name, a file by what its
    // graph holds.
    void load(const std::string& id_or_path);

    // The multiple an input side must land on for the prediction to come back
    // at the size that went in: 28 for Metric3D's decoder, 1 for MoGe, which
    // resamples its own output.
    int sizeGranularity() const;

    // What a returned depth multiplies by to become millimetres. Metric3D's is
    // canonical to a focal of 1000, so it is the face's focal in pixels; MoGe's
    // is already metres.
    double depthToMillimetres(double face_focal_px) const;

    GeometryPrediction predict(const float* rgb, const GeometryRequest& req);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

// Every model id `--model` accepts, MoGe's first, for a usage message.
std::string geometry_model_ids();

}  // namespace app
