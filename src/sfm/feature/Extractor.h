// The extractor seam: what `extract` calls, and the factory that decides which
// one it gets.
//
// GPU SIFT (sfm/feature/Sift.h) was the only implementation and the extract
// stage constructed it directly. That is no longer true -- ALIKED
// (src/aliked/) implements the same contract -- and the indirection buys two
// things beyond the obvious:
//
//   * the SIFT extractor's Vulkan context is not created when it is not the
//     one selected, which matters because ALIKED carries the inference layer's
//     own device and two live devices on one GPU is something this repository
//     sequences deliberately (AGENTS.md);
//   * each extractor states what it needs from the loader (color, working
//     resolution) rather than the CLI knowing per-type rules.
//
// Nothing above this seam assumes 128-D uint8, a scale or an orientation.
#pragma once

#include <memory>
#include <string>

#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/feature/Sift.h"

namespace sfm {

// ALIKED's knobs. Deliberately not aliked::ExtractOptions: src/sfm/ must build
// without the inference layer, so nothing here may include an aliked/ header.
struct AlikedOptions {
    // "aliked-n16rot" or "aliked-n32" (fetched and cached on first use), or a
    // path to an .onnx file. The two released variants differ only in how many
    // sample positions the descriptor head uses.
    std::string model = "aliked-n16rot";
    int   max_num_features = 2048;   // COLMAP's AlikedExtractionOptions default
    double min_score = 0.2;
    bool  verbose = true;
    int   device = -1;
};

// LoMa's knobs. Deliberately not loma::ExtractOptions, for the same reason as
// AlikedOptions: src/sfm/ must build without the inference layer.
struct LomaOptions {
    // Which variant's DESCRIPTOR runs -- "loma-b128" (DeDoDe-B, 128-D) or any
    // of loma-b / loma-r / loma-l / loma-g (DeDoDe-G, 256-D). The DaD detector
    // is shared by all five, so this only picks the descriptor.
    std::string variant = "loma-b128";
    // Paths to .onnx files, overriding what `variant` names. Empty = fetch.
    std::string detector_model;
    std::string descriptor_model;
    int    max_num_features = 2048;   // COLMAP's LomaExtractionOptions default
    double min_score = 0.0;           // DaD's density has no useful floor
    bool   verbose = true;
    int    device = -1;
};

struct IFeatureExtractor {
    virtual ~IFeatureExtractor() = default;

    // One image in, its features out, in the coordinates of `img` (the caller
    // scales them back to the source file's, D46).
    virtual FeatureSet extract(const GrayImage& img) = 0;

    virtual const char* name() const = 0;

    // Whether the loader has to decode color. SIFT works on luma; a learned
    // detector was trained on RGB and must see it.
    virtual bool wantsColor() const { return false; }
};

// Longest edge this extractor should run at when the user did not say, mirroring
// COLMAP's FeatureExtractionOptions::EffMaxImageSize(): 3200 for SIFT and 1600
// for either learned frontend, both of which are full-resolution-map bound.
int defaultMaxImageSize(const std::string& type);

// Whether `type` names one of the learned frontends (as opposed to "sift").
bool isAlikedType(const std::string& type);
bool isLomaType(const std::string& type);

// Descriptor width a LoMa variant works in -- 128 for loma-b128, 256 for the
// other four, 0 for anything else. `auto` refuses a --features / --matcher
// pair that disagrees on it, before spending the extraction.
int lomaDescriptorDim(const std::string& variant);

// Throws std::runtime_error naming the type when it is unknown, or when it is
// learned and this binary was built without the inference layer.
std::unique_ptr<IFeatureExtractor> createFeatureExtractor(const std::string& type,
                                                          const SiftOptions& sift,
                                                          const AlikedOptions& aliked,
                                                          const LomaOptions& loma);

}  // namespace sfm
