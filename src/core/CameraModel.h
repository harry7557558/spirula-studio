#pragma once

// CUDA-free camera-model enum + name helpers.
//
// CameraModelType and camera_model_from_name were historically defined in
// Common.cuh / Camera.h, which under the CUDA backend transitively pull in
// <cuda_runtime.h> (via backend/api/BackendTypes.h). The
// standalone dataset parsers (data/parsers/) only need these two trivial
// symbols, and are also compiled for WebAssembly (viewer/), where CUDA headers
// are unavailable. They include THIS header instead of Camera.h.
//
// The CUDA engine build is unaffected: it never includes this file (it keeps
// using Common.cuh's CameraModelType and Camera.h's helpers), and no single
// translation unit includes both this header and Common.cuh/Camera.h.
//
// The enum values MUST stay in sync with Common.cuh:130 and
// projection_utils.slang.

#include <string>

enum class CameraModelType {
    PINHOLE = 0,
    FISHEYE = 1,
    EQUISOLID = 2,
    EQUIRECTANGULAR = 3,
    ORTHOGRAPHIC = 4,
};

// Lens distortion, orthogonal to CameraModelType and chosen per camera group at
// load time: the parsers pick the cheapest tier that represents the source
// camera exactly, and fit + re-distort when none does. Values must match
// CameraDistortionType in shaders/projection_utils.slang and Common.cuh.
//
// Coefficient order is each tier's own -- a slot index does NOT mean the same
// thing across tiers:
//   OpenCV      k1 k2 p1 p2                 (COLMAP OPENCV)
//   ThinPrism   k1 k2 k3 k4 p1 p2 sx1 sy1   (COLMAP THIN_PRISM_FISHEYE)
enum class CameraDistortionType {
    None = 0,
    OpenCV = 1,
    ThinPrism = 2,
};

// Storage width of the per-camera coefficient row. Every tier reads a prefix of
// it; the tail is zero. MUST match kCameraDistortionParams in Common.cuh.
inline constexpr int kCameraDistortionParams = 8;

inline constexpr int camera_distortion_num_params(CameraDistortionType d) {
    return d == CameraDistortionType::None      ? 0 :
           d == CameraDistortionType::OpenCV    ? 4 : 8;
}

inline const char* camera_distortion_to_string(CameraDistortionType d) {
    switch (d) {
        case CameraDistortionType::None:      return "NONE";
        case CameraDistortionType::OpenCV:    return "OPENCV";
        case CameraDistortionType::ThinPrism: return "THIN_PRISM";
        default:                              return "UNKNOWN";
    }
}

inline CameraDistortionType camera_distortion_from_name(const std::string& name) {
    if (name == "NONE")       return CameraDistortionType::None;
    if (name == "OPENCV")     return CameraDistortionType::OpenCV;
    if (name == "THIN_PRISM") return CameraDistortionType::ThinPrism;
    return (CameraDistortionType)-1;
}

// Cheapest tier that still represents `coeffs` exactly, given the tier it was
// written in. Zero-extension OpenCV -> ThinPrism is exact, so a ThinPrism
// camera whose k3/k4/sx1/sy1 vanish demotes.
inline CameraDistortionType camera_distortion_demote(
    CameraDistortionType tier, const float* coeffs, float* out)
{
    auto zero = [](float v) { return v == 0.0f; };
    if (tier == CameraDistortionType::ThinPrism) {
        if (zero(coeffs[2]) && zero(coeffs[3]) && zero(coeffs[6]) && zero(coeffs[7])) {
            float k1 = coeffs[0], k2 = coeffs[1], p1 = coeffs[4], p2 = coeffs[5];
            out[0] = k1; out[1] = k2; out[2] = p1; out[3] = p2;
            for (int i = 4; i < kCameraDistortionParams; i++) out[i] = 0.0f;
            return camera_distortion_demote(CameraDistortionType::OpenCV, out, out);
        }
    } else if (tier == CameraDistortionType::OpenCV) {
        if (zero(coeffs[0]) && zero(coeffs[1]) && zero(coeffs[2]) && zero(coeffs[3])) {
            for (int i = 0; i < kCameraDistortionParams; i++) out[i] = 0.0f;
            return CameraDistortionType::None;
        }
    } else {
        for (int i = 0; i < kCameraDistortionParams; i++) out[i] = 0.0f;
        return CameraDistortionType::None;
    }
    if (out != coeffs)
        for (int i = 0; i < kCameraDistortionParams; i++) out[i] = coeffs[i];
    return tier;
}

// Rewrite `coeffs` from `from` into `to`, which must be a tier that contains it
// (see camera_distortion_kernel_tier). Returns false when it does not.
inline bool camera_distortion_promote(CameraDistortionType from,
                                      CameraDistortionType to,
                                      const float* coeffs, float* out) {
    if (from == to) {
        if (out != coeffs)
            for (int i = 0; i < kCameraDistortionParams; i++) out[i] = coeffs[i];
        return true;
    }
    float in[kCameraDistortionParams];
    for (int i = 0; i < kCameraDistortionParams; i++) in[i] = coeffs[i];
    for (int i = 0; i < kCameraDistortionParams; i++) out[i] = 0.0f;
    if (from == CameraDistortionType::None)
        return true;   // all-zero coefficients are the identity in every tier
    if (from == CameraDistortionType::OpenCV && to == CameraDistortionType::ThinPrism) {
        out[0] = in[0]; out[1] = in[1];          // k1 k2
        out[4] = in[2]; out[5] = in[3];          // p1 p2
        return true;
    }
    return false;
}

// Which (camera model, distortion tier) pairs the CUDA kernels are instantiated
// for. Every tier is available on Vulkan (a specialization constant costs no
// SPIR-V), so this bounds the CUDA binary only, and it must match
// kCameraVariants in tools/codegen/generate_kernel_instantiation.py and the
// export lists in shaders/primitive_3dgs.slang.
inline bool camera_distortion_is_compiled(CameraModelType m, CameraDistortionType d) {
    switch (m) {
        case CameraModelType::PINHOLE:
        case CameraModelType::FISHEYE:
        case CameraModelType::EQUISOLID:       return true;
        case CameraModelType::EQUIRECTANGULAR: return d == CameraDistortionType::None;
        default:                               return false;
    }
}

// COLMAP / NerfStudio camera-model string -> CameraModelType.
// Returns CameraModelType(-1) for unknown / unsupported models; callers
// should validate and raise. Mirrors Camera.h:90-105.
inline CameraModelType camera_model_from_name(const std::string& name) {
    if (name == "PINHOLE" ||
        name == "SIMPLE_PINHOLE" ||
        name == "SIMPLE_RADIAL" ||
        name == "RADIAL" ||
        name == "OPENCV")             return CameraModelType::PINHOLE;
    if (name == "FISHEYE" ||
        name == "SIMPLE_FISHEYE" ||
        name == "SIMPLE_RADIAL_FISHEYE" ||
        name == "RADIAL_FISHEYE" ||
        name == "OPENCV_FISHEYE" ||
        name == "THIN_PRISM_FISHEYE") return CameraModelType::FISHEYE;
    if (name == "ORTHOGRAPHIC") return CameraModelType::ORTHOGRAPHIC;
    if (name == "EQUISOLID")          return CameraModelType::EQUISOLID;
    if (name == "EQUIRECTANGULAR")    return CameraModelType::EQUIRECTANGULAR;
    return (CameraModelType)-1;
}

inline const char* camera_model_to_string(CameraModelType m) {
    switch (m) {
        case CameraModelType::ORTHOGRAPHIC:    return "ORTHOGRAPHIC";
        case CameraModelType::PINHOLE:         return "PINHOLE";
        case CameraModelType::FISHEYE:         return "FISHEYE";
        case CameraModelType::EQUISOLID:       return "EQUISOLID";
        case CameraModelType::EQUIRECTANGULAR: return "EQUIRECTANGULAR";
        default:                               return "UNKNOWN";
    }
}
