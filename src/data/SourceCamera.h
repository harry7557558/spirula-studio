#pragma once

// Forward projection of the camera models no (CameraModelType,
// CameraDistortionType) pair represents, on the CPU.
//
// Host mirror of shaders/camera_source.slang. The fitter measures against this
// one and the re-distort kernel resamples through that one, so the two must
// agree exactly or the images land in a different camera than the one fitted.

namespace srccam {

// Model ids. 0..17 are COLMAP's own CameraModelId values (models.h), and their
// parameters are COLMAP's array verbatim. Ours are numbered from 1000, leaving
// COLMAP room to keep appending to its enum.
constexpr int kColmapFOV                    = 7;
constexpr int kColmapRadTanThinPrismFisheye = 11;
constexpr int kColmapSimpleDivision         = 12;
constexpr int kColmapDivision               = 13;
constexpr int kColmapEUCM                   = 16;

// Any supported camera plus a sensor skew, which no tier carries: Agisoft
// Metashape's b2. Parameter layout in camera_source.slang.
constexpr int kSkewed = 1000;

// kSkewed's base camera, params[13].
constexpr float kSkewBasePerspective = 0.0f;
constexpr float kSkewBaseFisheye     = 1.0f;
constexpr float kSkewBaseEquisolid   = 2.0f;

// kSkewed's radial form, params[14].
constexpr float kSkewRadialPolynomial = 0.0f;   // DistThinPrism slot order
constexpr float kSkewRadialRational   = 1.0f;   // DistRational slot order

constexpr int kMaxParams = 16;

// View-space direction (need not be normalized) -> source pixel. false where
// the source camera has no image for that direction, including past the angle
// where the model folds back over itself.
bool project(int model_id, const float* params,
             double X, double Y, double Z, double* u, double* v);

// Multiply the pixel-valued parameters by the per-axis resolution scale, for a
// parser that resizes the camera it fitted: the re-distort kernels read these
// in the same pixel space as the fitted intrinsics.
void rescale(int model_id, float* params, double sx, double sy);

}  // namespace srccam
