#pragma once

// Compile-time selection of the per-tier Slang exports.
//
// slangc cannot export a generic, so shaders/projection_utils.slang emits one
// entry per distortion tier (`_none` / `_opencv` / `_prism`) and these traits
// pick one from a `CameraDistortionType` template argument.
//
// INCLUDE ORDER: this header names SlangProjectionUtils::* but does not open
// that namespace, because generated/projection_utils.cuh is `#pragma once` and
// would land in whichever namespace included it first. Include it after the
// TU's own `namespace SlangProjectionUtils { ... }` block.

#include "core/Common.cuh"

#ifdef __CUDACC__

template<CameraDistortionType D>
struct SlangDistortion;

#define _SS_DEF_SLANG_DISTORTION(TIER, SUFFIX)                                    \
template<>                                                                        \
struct SlangDistortion<CameraDistortionType::TIER> {                               \
    using Coeffs = CameraDistortionCoeffsT<CameraDistortionType::TIER>;            \
                                                                                   \
    static __device__ __forceinline__                                              \
    bool is_valid_distortion(float2 uv, const Coeffs& c) {                          \
        return SlangProjectionUtils::is_valid_distortion##SUFFIX(uv, c.v);          \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool persp_proj_nav(float3 p_view, float4 intrins, const Coeffs& c, float2* uv) { \
        return SlangProjectionUtils::persp_proj_nav##SUFFIX(p_view, intrins, c.v, uv); \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool fisheye_proj_nav(float3 p_view, float4 intrins, const Coeffs& c, float2* uv) { \
        return SlangProjectionUtils::fisheye_proj_nav##SUFFIX(p_view, intrins, c.v, uv); \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool equisolid_proj_nav(float3 p_view, float4 intrins, const Coeffs& c, float2* uv) { \
        return SlangProjectionUtils::equisolid_proj_nav##SUFFIX(p_view, intrins, c.v, uv); \
    }                                                                              \
    static __device__ __forceinline__                                              \
    float2 distort_point(float2 uv, int camera_model, const Coeffs& c) {            \
        return SlangProjectionUtils::distort_point##SUFFIX(uv, camera_model, c.v);  \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool undistort_point(float2 uv, int camera_model, const Coeffs& c, float2* out) { \
        return SlangProjectionUtils::undistort_point##SUFFIX(uv, camera_model, c.v, out); \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool unproject_point(float2 uv, int camera_model, const Coeffs& c, float3* rd) { \
        return SlangProjectionUtils::unproject_point##SUFFIX(uv, camera_model, c.v, rd); \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool generate_ray(float2 uv, int camera_model, const Coeffs& c, float3* rd) {   \
        return SlangProjectionUtils::generate_ray##SUFFIX(uv, camera_model, c.v, rd); \
    }                                                                              \
    static __device__ __forceinline__                                              \
    bool pixel_rect_quad(int camera_model, float4 intrins, const Coeffs& c,         \
                         float2 lo, float2 hi, float3* q) {                         \
        return SlangProjectionUtils::pixel_rect_quad##SUFFIX(                       \
            camera_model, intrins, c.v, lo, hi, q, q + 1, q + 2, q + 3);            \
    }                                                                              \
};

_SS_DEF_SLANG_DISTORTION(None,      _none)
_SS_DEF_SLANG_DISTORTION(OpenCV,    _opencv)
_SS_DEF_SLANG_DISTORTION(ThinPrism, _prism)

#undef _SS_DEF_SLANG_DISTORTION


// Camera-model-aware `*_proj_nav`, for the kernels that take the model at run
// time (image warps, meshing raster). EQUIRECTANGULAR ignores the coefficients.
template<CameraDistortionType D>
__device__ __forceinline__ bool camera_proj_nav(
    CameraModelType model, float3 p_view, float4 intrins,
    const typename SlangDistortion<D>::Coeffs& c, float2* uv
) {
    switch (model) {
        case CameraModelType::FISHEYE:
            return SlangDistortion<D>::fisheye_proj_nav(p_view, intrins, c, uv);
        case CameraModelType::EQUISOLID:
            return SlangDistortion<D>::equisolid_proj_nav(p_view, intrins, c, uv);
        case CameraModelType::EQUIRECTANGULAR:
            return SlangProjectionUtils::equirect_proj_nav(p_view, intrins, uv);
        default:
            return SlangDistortion<D>::persp_proj_nav(p_view, intrins, c, uv);
    }
}

#endif  // __CUDACC__
