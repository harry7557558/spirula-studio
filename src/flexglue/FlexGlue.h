#pragma once
// FlexGlue (cvg/FlexGlue, MIT code; the checkpoint bundles Meta's DINOv3
// ConvNeXt-B under the DINOv3 License): dense stride-8 features per image, then
// a sparse matcher over two images' keypoints that also scores whether the pair
// is covisible at all. This is the configuration COLMAP runs: the 512x512
// squash, fp32, mutual nearest neighbours, early-stop checks after layers
// 2, 4, 6 and 8. README.md has the numbers.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace flexglue {

// COLMAP's FlexGlueMatchingOptions defaults.
struct MatchOptions {
    float filter_threshold = 0.1f;
    // Mean of the two directions' distances, in pixels of the 512x512 input.
    // <= 0 disables it.
    float max_distance_px = 3.0f;
    // 0 disables the pair rejection and every early stop.
    float min_covisibility = 0.5f;
    // Stop after check layer k when score_k < min_covisibility * (1 - decay)^(9 - k).
    // 1 never stops.
    float early_stop_decay = 0.5f;
    std::vector<int> check_layers = {2, 4, 6, 8};
    // Also return each keypoint's predicted point and confidence (tests).
    bool keep_correspondences = false;
};

struct MatchResult {
    std::vector<int32_t> matches0;   // [n0]: index into set 1, or -1
    std::vector<float>   scores0;    // [n0]: 0 where unmatched
    float covisibility = 0.0f;       // the score of the last layer run
    int   stop_layer = 0;            // 9 when no early stop fired
    bool  covisible_pair = false;    // ran to the end and scored >= min_covisibility
    // keep_correspondences: per direction i -> j, normalized [-1, 1] points in
    // j ([n_i, 2]) and their confidence ([n_i]), before any gate.
    std::vector<float> p2d[2], conf[2];
};

struct DenseFeatures;   // one image's [64, 64, 256] map, resident on the device

// One image's keypoints, in pixels of the image the dense features were made
// from, corner origin (the top-left pixel's centre is at (0.5, 0.5)).
struct Keypoints {
    const float* xy = nullptr;   // [n, 2]
    uint32_t     n = 0;
    int          width = 0, height = 0;
};

class Model {
public:
    Model();
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // "flexglue" (fetched and cached, licence-gated) or a path to flexglue.pth.
    void load(const std::string& model = "flexglue");
    bool loaded() const;

    // An 8-bit RGB image, any size; squashed to 512x512 as COLMAP does.
    std::shared_ptr<DenseFeatures> describe(const uint8_t* rgb, int width, int height);
    // The same from an already squashed, [0, 1] [512, 512, 3] image.
    std::shared_ptr<DenseFeatures> describeResized(const float* rgb512);

    MatchResult match(const DenseFeatures& f0, const Keypoints& k0, const DenseFeatures& f1,
                      const Keypoints& k1, const MatchOptions& opts = {});

    // SS_FLEXGLUE_DUMP=<dir>: <prefix><stage>.npy per stage when `stages`, and
    // match()'s outputs as <outputs><name>.npy (tools/flexglue_ref/README.md).
    void setDump(const std::string& prefix, const std::string& outputs = "",
                 bool stages = true);

    // Host copy of a feature map, [64, 64, 256] channel-last.
    std::vector<float> download(const DenseFeatures& f) const;

    // Layer 1's self block depends on one image only, so match() keeps its
    // output (8 MB) for the `entries` most recently used live images, 0 = off.
    // Default 64, or SS_FLEXGLUE_SELF_CACHE. Entries die with their features.
    void setSelfCache(int entries);
    struct SelfCacheStats {
        uint64_t hits = 0, misses = 0, evictions = 0;
        int entries = 0, capacity = 0;
    };
    SelfCacheStats selfCacheStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// COLMAP's ResizeBilinearToCHW, laid out [h, w, 3] in [0, 1]: half-pixel
// bilinear, no antialias, which is F.interpolate(align_corners=False).
std::vector<float> squash_rgb(const uint8_t* rgb, int width, int height, int out_w,
                              int out_h);

}  // namespace flexglue
