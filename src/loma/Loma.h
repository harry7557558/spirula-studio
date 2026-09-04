#pragma once
// LoMa: DaD keypoints, DeDoDe descriptors and the LoMa matcher, on the
// inference layer (src/nn/). https://github.com/davnords/LoMa
//
// The public surface of src/loma/. Nothing here exposes Vulkan, nn::Tensor or
// the checkpoints' layout, so src/sfm/ can hold one without depending on the
// inference layer's headers. The conventions below are COLMAP's LomaFeature*,
// so a reconstruction on our features and one on COLMAP's are comparable:
// keypoints in ORIGINAL-image pixels with the top-left pixel's CORNER at
// (0, 0), descriptors NOT normalized (LoMa's matcher does not want them to
// be), and the descriptor run at the fixed square size it was exported at.

#include <cstdint>
#include <string>
#include <vector>

namespace loma {

struct ExtractOptions {
    // Top-scoring keypoints kept. COLMAP's LomaExtractionOptions default; the
    // paper recommends 2048 or 4096.
    int max_num_features = 2048;
    // DaD's score is a normalized density over the whole map, not a per-pixel
    // confidence, so it has no ALIKED-like threshold. COLMAP defaults to 0 for
    // the same reason and says so.
    float min_score = 0.0f;
    // Peak list capacity. Every local maximum of the density survives NMS and
    // a 2 MP image has tens of thousands; saturating warns rather than fails.
    uint32_t max_candidates = 262144;
};

struct Keypoint {
    float x = 0, y = 0;   // original-image pixels, corner origin
    float score = 0;
};

struct Features {
    int width = 0, height = 0;      // the image the coordinates refer to
    int desc_dim = 0;
    std::vector<Keypoint> keypoints;
    std::vector<float>    descriptors;   // keypoints.size() * desc_dim
};

// Owns the two checkpoints (detector and descriptor) and the working buffers
// for one image at a time. Not thread-safe: one Extractor per thread, and they
// share the process-wide inference device.
class Extractor {
public:
    Extractor();
    ~Extractor();
    Extractor(const Extractor&) = delete;
    Extractor& operator=(const Extractor&) = delete;

    // Each is a known id ("loma-dad", "loma-dedode-b", "loma-dedode-g"),
    // fetched and cached, or a path to an .onnx file. Throws nn::Error on
    // anything that is not the LoMa checkpoint it was asked for.
    void load(const std::string& detector, const std::string& descriptor);
    bool loaded() const;

    // Descriptor width of the loaded checkpoint: 128 for DeDoDe-B, 256 for -G.
    int descriptorDim() const;

    // `rgb` is width*height*3 interleaved bytes.
    Features extract(const uint8_t* rgb, int width, int height,
                     const ExtractOptions& opts = {});

    // The largest arena extract() has planned, and the most it has used. Both
    // are high-water marks. The arena refuses to grow mid-pass, so a plan that
    // under-counts is a hard failure; loma_test asserts the second fits.
    uint64_t plannedBytes() const;
    uint64_t peakBytes() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

struct MatchOptions {
    // Assignment confidence a pair must reach. COLMAP's LomaMatchingOptions
    // default, and the threshold LoMa's own filter_matches uses.
    float min_score = 0.1f;
};

struct Match {
    uint32_t i = 0, j = 0;   // indices into set 0 and set 1
    float    score = 0;
};

// One image's input. Keypoints are in pixels with the top-left pixel's CORNER
// at (0, 0); the conversion back to LoMa's normalized [-1, 1] happens inside.
struct MatchInput {
    const float* keypoints = nullptr;    // [n, 2] xy
    const float* descriptors = nullptr;  // [n, dim]
    uint32_t     n = 0;
    int          width = 0, height = 0;  // the image the keypoints refer to
};

// Nine transformer layers -- self-attention within each image, cross-attention
// between them -- and one dual-softmax assignment. Tens of milliseconds for a
// 2048 x 2048 pair, so it belongs behind pair selection and nowhere else.
class Matcher {
public:
    Matcher();
    ~Matcher();
    Matcher(const Matcher&) = delete;
    Matcher& operator=(const Matcher&) = delete;

    // "loma-b", "loma-b128", "loma-r", "loma-l", "loma-g" (fetched and cached)
    // or a path to an .onnx file.
    void load(const std::string& model);
    bool loaded() const;
    int  descriptorDim() const;   // what the matcher's input_proj expects

    std::vector<Match> match(const MatchInput& a, const MatchInput& b,
                             const MatchOptions& opts = {});

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

// Which descriptor a matcher variant was trained against: "loma-dedode-b" for
// loma-b128, "loma-dedode-g" for the rest. Empty when `matcher` is not one of
// the known ids (a path, say), where the caller has to say for itself.
std::string descriptor_for_matcher(const std::string& matcher);

}  // namespace loma
