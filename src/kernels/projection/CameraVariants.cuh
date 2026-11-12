#pragma once

// The (camera model, distortion tier) pairs the CUDA kernels are instantiated
// for, as an X-macro so every launcher's dispatch chain stays in step. Must
// match camera_distortion_is_compiled() in core/CameraModel.h, kCameraVariants
// in tools/codegen/generate_kernel_instantiation.py and the export list in
// shaders/primitive_3dgs.slang.
//
// F takes the unqualified enumerator names, e.g.
//   #define _DISPATCH(M, D) if (m == CameraModelType::M && d == CameraDistortionType::D) ...
#define SS_FOR_EACH_CAMERA_VARIANT(F) \
    F(PINHOLE,         None)          \
    F(PINHOLE,         OpenCV)        \
    F(PINHOLE,         ThinPrism)     \
    F(FISHEYE,         None)          \
    F(FISHEYE,         OpenCV)        \
    F(FISHEYE,         ThinPrism)     \
    F(EQUISOLID,       None)          \
    F(EQUISOLID,       OpenCV)        \
    F(EQUISOLID,       ThinPrism)     \
    F(EQUIRECTANGULAR, None)

// Viewer-only lenses never expand the backward or fused-training kernels.
#define SS_FOR_EACH_FORWARD_CAMERA_VARIANT(F) \
    SS_FOR_EACH_CAMERA_VARIANT(F)             \
    F(ORTHOGRAPHIC, None)
