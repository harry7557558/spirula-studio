#pragma once
// The matcher seam: LightGlue behind IFeatureMatcher, and the factory that
// picks between it and the GPU brute-force matcher.
//
// sfm/feature/Matcher.h always said a learned matcher would implement that
// interface; this is it. What the interface does NOT carry, and what callers
// therefore have to know, is cost: brute force is ~3.5 ms for a pair, LightGlue
// is tens of milliseconds, because it runs nine transformer layers over both
// images' keypoints. It belongs behind pair selection and nowhere else --
// `--pairs exhaustive` with `--matcher lightglue` on a thousand images is half
// a million pairs and several hours.
//
// Like Extractor.h, nothing here may include an aliked/ header: src/sfm/ builds
// without the inference layer, and the factory says so at run time.

#include <memory>
#include <string>

#include "sfm/feature/Matcher.h"

namespace sfm {

struct LightGlueOptions {
    // "aliked-lightglue" (fetched and cached on first use) or a path to an
    // .onnx file.
    std::string model = "aliked-lightglue";
    // COLMAP's LightGlueONNXMatchingOptions default.
    double min_score = 0.1;
    int    device = -1;
    bool   verbose = true;
};

// LoMa's matcher. Five released variants, all nine layers; they differ in the
// embedding width and in which descriptor they were trained against, and both
// are read off the checkpoint rather than spelled here.
struct LomaMatchOptions {
    // "loma-b", "loma-b128", "loma-r", "loma-l", "loma-g" (fetched and cached)
    // or a path to an .onnx file. Empty means "whatever --matcher named".
    std::string model;
    // COLMAP's LomaMatchingOptions default, and LoMa's own filter threshold.
    double min_score = 0.1;
    int    device = -1;
    bool   verbose = true;
};

bool isLearnedMatcher(const std::string& type);

// Throws std::runtime_error naming the type when it is unknown, or when it is
// learned and this binary has no inference layer.
std::unique_ptr<IFeatureMatcher> createFeatureMatcher(const std::string& type,
                                                      const MatchOptions& match,
                                                      const LightGlueOptions& lightglue,
                                                      const LomaMatchOptions& loma);

}  // namespace sfm
