#pragma once

// The one seam between `spirula geometry` and a monocular geometry network.
//
// Three families sit behind it and they answer different questions: Metric3D v2
// predicts depth directly, MoGe-2 predicts an affine point map that needs the
// camera's focal length to become one, and Marigold v2 predicts depth only up to
// an affine map of its logarithm. Everything that difference touches -- which
// input sizes round-trip, what a depth means in metres, whether a mask comes
// back -- is resolved here so the caller only sees depth and normals.

#include "sfm/core/Exif.h"   // ExifTransform

#include <atomic>
#include <cstdint>
#include <functional>
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
    // MoGe-3's refinement passes; every other model ignores it.
    int refine_steps = 3;
};

struct GeometryPrediction {
    int width = 0, height = 0;
    // [H*W] depth and [H*W*3] unit normals, carrying the trainer's two
    // sentinels: 0 and black wherever the model had no answer.
    std::vector<float> depth;
    std::vector<float> normal;
    // [H*W] the probability the model has an answer at all (MoGe's sky mask),
    // empty for a model that answers everywhere.
    std::vector<float> mask;
};

class GeometryWarp;

// Face k of a planned warp: its size and the pinhole it is. Wanting both maps
// by default -- the caller turns off what it is not writing.
GeometryRequest face_request(const GeometryWarp& warp, int k, int num_tokens);

// The same pinhole face after `t` has been applied to its pixels: on an odd
// quarter turn the axes swap, and the principal point travels with them.
GeometryRequest turn_request(const GeometryRequest& r, const sfm::ExifTransform& t);

// One loaded checkpoint. Not thread-safe; both backends share the process-wide
// inference device.
class GeometryModel {
public:
    GeometryModel();
    ~GeometryModel();
    GeometryModel(const GeometryModel&) = delete;
    GeometryModel& operator=(const GeometryModel&) = delete;

    // `selector` is the canonical UUID for the process-wide inference device.
    // Empty lets the caller's SS_VK_DEVICE/Auto precedence apply. A family with
    // one network per map (Marigold v2) loads only those the run writes.
    void load(const std::string& id_or_path, const std::string& selector = {},
              bool want_depth = true, bool want_normal = true);

    // The multiple an input side must land on for the prediction to come back
    // at the size that went in: 28 for Metric3D's decoder, 1 for MoGe, which
    // resamples its own output.
    int sizeGranularity() const;

    // The fewest pixels a split face should hold: MoGe's lowest token budget,
    // 1200 patches. GeometryWarp::plan's `min_face_px`.
    static int64_t minFacePixels();

    // What a returned depth multiplies by to become millimetres. Metric3D's is
    // canonical to a focal of 1000, so it is the face's focal in pixels; MoGe's
    // is already metres.
    double depthToMillimetres(double face_focal_px) const;

    GeometryPrediction predict(const float* rgb, const GeometryRequest& req);

    // The answers predict() gives one at a time, for faces of one frame or of
    // several. Marigold v2 runs those of a size as one batch.
    std::vector<GeometryPrediction> predictBatch(const std::vector<const float*>& rgb,
                                                 const std::vector<GeometryRequest>& reqs);
    // Faces of this request's size one batch should hold; 1 where batching
    // buys nothing.
    int batchCapacity(const GeometryRequest& req);
    // False where one prediction is too dear to spend on warming the pipelines;
    // prepare() then does what setup it can for faces like `req` without one.
    bool cheapWarmup() const;
    void prepare(const GeometryRequest& req);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

// Every model id `--model` accepts, MoGe's first, for a usage message.
std::string geometry_model_ids();

// What has to be fetched before load() can run, beyond the single files the
// families fetch themselves. Only Marigold v2 has any: it is built from ~45 GB
// streamed (src/marigold/README.md), and the disk is checked beforehand.
struct GeometryInstall {
    uint64_t download = 0, disk = 0, free = 0;
    uint64_t spare = 0;    // left free on top of `disk`
    std::string where;     // the directory it is written to
    bool needed() const { return download > 0; }
    bool fits() const { return free >= disk + spare; }
};
GeometryInstall geometry_install_needs(const std::string& id, bool want_depth,
                                       bool want_normal);
// Bytes streamed so far, of the total.
using GeometryInstallProgress = std::function<void(uint64_t done, uint64_t total)>;
void geometry_install(const std::string& id, bool want_depth, bool want_normal,
                      const GeometryInstallProgress& progress,
                      const std::atomic<bool>* cancel);

}  // namespace app
