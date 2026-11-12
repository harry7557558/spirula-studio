#pragma once

// Camera-distortion inputs shared by the backend parity tools: the compiled
// (camera model, tier) set and one coefficient row per (tier, camera).

#include <core/Common.cuh>

#include <cstdint>
#include <vector>

namespace dist_fixture {

inline const char* const kTierNames[3] = {"NONE", "OPENCV", "THIN_PRISM"};

// Tiers compiled for each camera model, in CameraModelType order; see
// camera_distortion_is_compiled in core/CameraModel.h. Rows are padded with
// tier 0 so a fixed-width table indexes cleanly -- read only kNumTiers[m].
inline constexpr int kNumTiers[5] = {3, 3, 3, 1, 1};
inline constexpr int kTiers[5][3] = {
    {0, 1, 2},   // PINHOLE
    {0, 1, 2},   // FISHEYE
    {0, 1, 2},   // EQUISOLID
    {0, 0, 0},   // EQUIRECTANGULAR
    {0, 0, 0},   // ORTHOGRAPHIC
};

// [tier][camera][kCameraDistortionParams], each row in ITS OWN tier's slot
// order and filling the terms the cheaper tiers cannot express. Magnitudes stay
// mild: a strong lens flips borderline culls and rewrites whole comparison rows.
inline std::vector<float> distortion_rows(int64_t C) {
    static const float row[3][kCameraDistortionParams] = {
        {},
        {0.05f, -0.01f, 0.0012f, -0.0020f},
        {0.05f, -0.01f, 0.0020f, -0.0005f,
         0.0012f, -0.0020f, 0.0015f, -0.0008f},
    };
    std::vector<float> out((size_t)3 * C * kCameraDistortionParams, 0.0f);
    for (int t = 0; t < 3; t++)
        for (int64_t c = 0; c < C; c++) {
            const float s = 1.0f - 0.3f * (float)(c % 3);
            for (int k = 0; k < kCameraDistortionParams; k++)
                out[((size_t)t * C + c) * kCameraDistortionParams + k] =
                    s * row[t][k];
        }
    return out;
}

// Offset in floats of camera `c`'s row under `tier`, into distortion_rows(C).
inline int64_t row_offset(int tier, int64_t C, int64_t c = 0) {
    return ((int64_t)tier * C + c) * kCameraDistortionParams;
}

}  // namespace dist_fixture
