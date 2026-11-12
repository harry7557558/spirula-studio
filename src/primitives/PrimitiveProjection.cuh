#pragma once

// (camera model, distortion tier) -> the matching projection_3dgs / 3dgut
// export in shaders/primitive_3dgs.slang.
//
// Only the pairs the CUDA build instantiates exist here; the set matches
// camera_distortion_is_compiled() in core/CameraModel.h, the export list in
// shaders/primitive_3dgs.slang and kCameraVariants in
// tools/codegen/generate_kernel_instantiation.py. A combination outside it is a
// compile error rather than a silent fallback, so the host promotion in
// bake_post_split cannot drift from what was built.
//
// INCLUDE ORDER: names Slang3DGS::* without opening it -- include after the
// TU's own `namespace Slang3DGS { ... }` block.

#include "core/Common.cuh"
#include "primitives/Primitive.cuh"

#ifdef __CUDACC__

template<CameraModelType M, CameraDistortionType D>
struct Slang3DGSProj {
    static_assert(sizeof(CameraDistortionCoeffsT<D>) == 0,
                  "this (camera model, distortion tier) pair is not compiled -- "
                  "see camera_distortion_is_compiled() in core/CameraModel.h");
};

#define _SS_DEF_3DGS_PROJ(MODEL, TIER, MODEL_SUFFIX, TIER_SUFFIX)                 \
template<>                                                                        \
struct Slang3DGSProj<CameraModelType::MODEL, CameraDistortionType::TIER> {         \
    using Coeffs = CameraDistortionCoeffsT<CameraDistortionType::TIER>;            \
                                                                                   \
    template<typename... Args>                                                     \
    static __device__ __forceinline__ void fwd_2d(                                 \
        bool antialiased, Args... args                                             \
    ) {                                                                            \
        Slang3DGS::projection_3dgs_##MODEL_SUFFIX##TIER_SUFFIX(antialiased, args...); \
    }                                                                              \
    template<typename... Args>                                                     \
    static __device__ __forceinline__ void fwd_3d(                                 \
        bool antialiased, Args... args                                             \
    ) {                                                                            \
        Slang3DGS::projection_3dgut_##MODEL_SUFFIX##TIER_SUFFIX(antialiased, args...); \
    }                                                                              \
    template<typename... Args>                                                     \
    static __device__ __forceinline__ void vjp_2d(                                 \
        bool antialiased, Args... args                                             \
    ) {                                                                            \
        Slang3DGS::projection_3dgs_##MODEL_SUFFIX##TIER_SUFFIX##_vjp(antialiased, args...); \
    }                                                                              \
    template<typename... Args>                                                     \
    static __device__ __forceinline__ void vjp_3d(                                 \
        bool antialiased, Args... args                                             \
    ) {                                                                            \
        Slang3DGS::projection_3dgut_##MODEL_SUFFIX##TIER_SUFFIX##_vjp(antialiased, args...); \
    }                                                                              \
};

#define _SS_DEF_3DGS_PROJ_TIERS(MODEL, MODEL_SUFFIX)          \
    _SS_DEF_3DGS_PROJ(MODEL, None,      MODEL_SUFFIX, _none)  \
    _SS_DEF_3DGS_PROJ(MODEL, OpenCV,    MODEL_SUFFIX, _opencv)\
    _SS_DEF_3DGS_PROJ(MODEL, ThinPrism, MODEL_SUFFIX, _prism)

_SS_DEF_3DGS_PROJ_TIERS(PINHOLE, persp)
_SS_DEF_3DGS_PROJ_TIERS(FISHEYE, fisheye)
_SS_DEF_3DGS_PROJ_TIERS(EQUISOLID, equisolid)
_SS_DEF_3DGS_PROJ(EQUIRECTANGULAR, None, equirect, _none)

template<>
struct Slang3DGSProj<CameraModelType::ORTHOGRAPHIC, CameraDistortionType::None> {
    template<typename... Args>
    static __device__ __forceinline__ void fwd_2d(bool antialiased, Args... args) {
        Slang3DGS::projection_3dgs_ortho_none(antialiased, args...);
    }
    template<typename... Args>
    static __device__ __forceinline__ void fwd_3d(bool antialiased, Args... args) {
        Slang3DGS::projection_3dgut_ortho_none(antialiased, args...);
    }
};

#undef _SS_DEF_3DGS_PROJ_TIERS
#undef _SS_DEF_3DGS_PROJ

#endif  // __CUDACC__
