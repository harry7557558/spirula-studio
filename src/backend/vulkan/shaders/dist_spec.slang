#pragma once

// Distortion tier as a specialization constant: the coefficient loader and the
// dispatch macros that fold `kDistortion` (and, where the camera model is also
// a spec constant, the pair) into the Slang generics of
// shaders/projection_utils.slang.
//
// `kDistortion` itself is NOT declared here. Specialization-constant IDs are
// assigned by declaration order within a module and the host passes SpecList by
// index, so a constant declared mid-module renumbers everything after it: every
// module declares `kDistortion` LAST and appends its value last in SpecList.
//
// Include after shaders/projection_utils.slang (directly or transitively) and
// after int64_compat.slang.

// Storage width of one camera's coefficient row; must match
// kCameraDistortionParams in core/CameraModel.h. Each tier reads a prefix.
static const int kDistCoeffRow = 8;

// The identity tier reads nothing: its one slot is unused (Slang has no
// zero-length array), and an undistorted launch passes the 4 KB zeroed
// fallback, which a large `cid` would index past.
[ForceInline]
void load_dist_coeffs<D : ICameraDistortion>(
    float* dist_coeffs, uint cid, out float dist[D.kNumParams]
) {
    [ForceUnroll]
    for (int i = 0; i < D.kNumParams; i++)
        dist[i] = (D.kIsIdentity || is_null(dist_coeffs))
                      ? 0.0f
                      : dist_coeffs[kDistCoeffRow * cid + i];
}

// Ray through a pixel for the kernels that take the camera model as a runtime
// value (rasterization, background, pixel-wise): loads the tier's row and calls
// generate_ray, so callers dispatch on kDistortion alone.
[ForceInline]
bool pixel_ray<D : ICameraDistortion>(float* dist_coeffs, uint cid,
                                      int camera_model, float2 uv,
                                      out float3 raydir) {
    float dist[D.kNumParams];
    load_dist_coeffs<D>(dist_coeffs, cid, dist);
    return generate_ray<D>(uv, camera_model, dist, raydir);
}

// CALL(D) once per tier, folded on kDistortion.
#define SS_DISPATCH_DIST(CALL)                  \
    switch (kDistortion) {                      \
    case 0:  CALL(DistNone); break;             \
    case 1:  CALL(DistOpenCV); break;           \
    default: CALL(DistThinPrism); break;        \
    }

// CALL(camera model, D) over the compiled combos (camera_distortion_is_compiled
// in core/CameraModel.h). Combos outside that set fall through to the nearest
// compiled tier rather than duplicating an arm -- no launcher dispatches them.
#define SS_DISPATCH_CAM_DIST(CALL)                                      \
    switch (kCameraModel) {                                             \
    case 1:                                                             \
        switch (kDistortion) {                                          \
        case 0:  CALL(CameraModelType::FISHEYE, DistNone); break;       \
        case 1:  CALL(CameraModelType::FISHEYE, DistOpenCV); break;     \
        default: CALL(CameraModelType::FISHEYE, DistThinPrism); break;  \
        }                                                               \
        break;                                                          \
    case 2:                                                             \
        switch (kDistortion) {                                          \
        case 0:  CALL(CameraModelType::EQUISOLID, DistNone); break;     \
        case 1:  CALL(CameraModelType::EQUISOLID, DistOpenCV); break;   \
        default: CALL(CameraModelType::EQUISOLID, DistThinPrism); break;\
        }                                                               \
        break;                                                          \
    case 3:                                                             \
        CALL(CameraModelType::EQUIRECTANGULAR, DistNone);               \
        break;                                                          \
    default:                                                            \
        switch (kDistortion) {                                          \
        case 0:  CALL(CameraModelType::PINHOLE, DistNone); break;       \
        case 1:  CALL(CameraModelType::PINHOLE, DistOpenCV); break;     \
        default: CALL(CameraModelType::PINHOLE, DistThinPrism); break;  \
        }                                                               \
        break;                                                          \
    }

// Orthographic is a viewer-only forward path.
#define SS_DISPATCH_CAM_DIST_FWD(CALL) \
    if (kCameraModel == 4) { CALL(CameraModelType::ORTHOGRAPHIC, DistNone); } \
    else { SS_DISPATCH_CAM_DIST(CALL) }
