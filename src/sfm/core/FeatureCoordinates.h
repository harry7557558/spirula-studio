#pragma once

#include "sfm/core/Features.h"

namespace sfm {

// SIFT's first octave samples source x = 0.5*x_oct - 0.25; learned extractors use corners.
inline float extractedCenterOffset(bool sift) { return sift ? -0.25f : -0.5f; }

inline void shiftKeypoints(FeatureSet& features, float x, float y) {
    for (auto& keypoint : features.keypoints) {
        keypoint.x += x;
        keypoint.y += y;
    }
}

inline void centerExtractedKeypoints(FeatureSet& features, bool sift) {
    const float offset = extractedCenterOffset(sift);
    shiftKeypoints(features, offset, offset);
}

inline void correctStoredKeypoints(FeatureSet& features, bool sift) {
    const float sx = features.extract_width > 0 ? float(features.width) / features.extract_width : 1;
    const float sy = features.extract_height > 0 ? float(features.height) / features.extract_height : 1;
    const float offset = extractedCenterOffset(sift);
    shiftKeypoints(features, 0.5f + offset * sx, 0.5f + offset * sy);
}

} // namespace sfm
