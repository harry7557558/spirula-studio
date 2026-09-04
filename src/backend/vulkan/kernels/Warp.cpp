// Vulkan implementation of the dataset GT warp / re-distort + byte->float
// conversion launch APIs (the launch_warp_* and launch_redistort_* sections of
// kernels/pixelwise/PixelWise.cuh and the uint8/16_*_to_float_raw converters of
// EngineInternal.h). Mirrors the CUDA launchers in
// kernels/pixelwise/ImageWarp.cu + ImageRedistort.cu; the device work runs
// shaders/warp.slang.
//
// All launchers take raw device pointers (the engine's DataManager path
// drives them without tensor wrappers). Nullable pointers (dist_coeffs)
// go through vkk::or_fallback; input type tags map to the shader's
// elem_kind field instead of the CUDA template dispatch.

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Common.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors the elem-kind constants in shaders/warp.slang.
constexpr int kElemU8 = 0;
constexpr int kElemU16 = 1;
constexpr int kElemF32 = 2;

// Mirrors WarpParams in shaders/warp.slang.
struct WarpParams {
    uint64_t intrins, dist_coeffs, source_models, source_params;
    uint64_t src, dst, axes, post_intrins;
    int32_t B, Hin, Win, C;
    int32_t K, Hout, Wout;
    int32_t elem_kind, camera_model;
    int32_t ref_H, ref_W;
    int32_t ray_depth;
    float norm_inv, decode_off, invalid;
    int32_t _pad0;
};
static_assert(sizeof(WarpParams) == 8 * 8 + 16 * 4,
              "params layout must match the slang struct");

// Mirrors WarpMaskParams in shaders/warp.slang.
struct WarpMaskParams {
    uint64_t intrins, dist_coeffs, source_models, source_params;
    uint64_t src, dst, axes, post_intrins;
    int32_t B, Hin, Win;
    int32_t K, Hout, Wout;
    int32_t camera_model;
    int32_t ref_H, ref_W;
    uint32_t wgs_per_row;
};
static_assert(sizeof(WarpMaskParams) == 8 * 8 + 10 * 4,
              "params layout must match the slang struct");

// Mirrors BytesToFloatParams in shaders/warp.slang.
struct BytesToFloatParams {
    uint64_t src, dst;
    uint32_t total;
    int32_t elem_kind;
    float scale, offset;
    uint32_t wgs_per_row;
    int32_t _pad0;
};
static_assert(sizeof(BytesToFloatParams) == 2 * 8 + 6 * 4,
              "params layout must match the slang struct");

// Source pointers, which the ordinary (non-re-distorting) path leaves null.
struct SourceCam {
    const int* models = nullptr;
    const float* params = nullptr;
};

// Per post camera: its frame and its face intrinsics (both null on the
// re-distort path, which has no faces).
struct Faces {
    const float* post_intrins = nullptr;
    const float* axes = nullptr;
};

// Common WarpParams assembly for the image / depth / normal warps.
WarpParams make_warp_params(const float* d_intrins, const float* d_dist,
                            SourceCam src_cam, const void* d_src, float* d_dst,
                            Faces faces, int B, int Hin, int Win,
                            int C, int K, int Hout, int Wout, int elem_kind,
                            CameraModelType cm, int ref_H, int ref_W,
                            bool ray_depth, float norm_inv,
                            float decode_off) {
    WarpParams p{};
    p.intrins = vkk::or_fallback(d_intrins);
    p.dist_coeffs = vkk::or_fallback(d_dist);
    p.source_models = vkk::or_fallback(src_cam.models);
    p.source_params = vkk::or_fallback(src_cam.params);
    p.src = vkk::or_fallback(d_src);
    p.dst = (uint64_t)d_dst;
    p.axes = vkk::or_fallback(faces.axes);
    p.post_intrins = vkk::or_fallback(faces.post_intrins);
    p.B = B; p.Hin = Hin; p.Win = Win; p.C = C;
    p.K = K; p.Hout = Hout; p.Wout = Wout;
    p.elem_kind = elem_kind;
    p.camera_model = (int)cm;
    p.ref_H = ref_H; p.ref_W = ref_W;
    p.ray_depth = ray_depth ? 1 : 0;
    p.norm_inv = norm_inv;
    p.decode_off = decode_off;
    return p;
}

// (ceil(Wout/16), ceil(Hout/16), B) grid of flat-256 workgroups (16x16
// logical tiles), mirroring the CUDA _LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1).
void dispatch_warp(const char* entry, const WarpParams& p,
                   const backend::vk::SpecList& spec) {
    if ((int64_t)p.B * p.K * p.Hout * p.Wout <= 0) return;
    if (p.B > 65535)
        throw std::runtime_error("warp: batch exceeds grid limit");
    vkk::dispatch(entry, spec, (uint32_t)((p.Wout + 15) / 16),
                  (uint32_t)((p.Hout + 15) / 16), (uint32_t)p.B, &p,
                  sizeof(p));
}

// Re-distort of one modality, whose input and output grids are independent of
// each other and of the (ref_H, ref_W) intrinsics reference.
WarpParams make_redistort_params(const float* d_intrins, const float* d_dist,
                                 SourceCam src_cam, const void* d_src,
                                 float* d_dst, int B, int in_H, int in_W,
                                 int C, int out_H, int out_W, int elem_kind,
                                 CameraModelType cm, int ref_H, int ref_W,
                                 float norm_inv, float decode_off,
                                 float invalid) {
    WarpParams p = make_warp_params(d_intrins, d_dist, src_cam, d_src, d_dst,
                                    {}, B, in_H, in_W, C, 1, out_H, out_W,
                                    elem_kind, cm, ref_H, ref_W, false,
                                    norm_inv, decode_off);
    p.invalid = invalid;
    return p;
}

void run_bytes_to_float(const void* d_in, float* d_out, int64_t total,
                        int elem_kind, float scale, float offset) {
    BytesToFloatParams p{};
    p.src = vkk::or_fallback(d_in);
    p.dst = (uint64_t)d_out;
    p.total = (uint32_t)total;
    p.elem_kind = elem_kind;
    p.scale = scale;
    p.offset = offset;
    vkk::dispatch_flat("warp.bytes_to_float", {}, total, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

// kFromSource: the CUDA _SS_DISPATCH_SOURCE predicate.
uint32_t from_source_spec(const int* d_source_models) {
    return d_source_models != nullptr ? 1u : 0u;
}

int resolve_depth_kind(uint32_t elem_size, const char* who) {
    if (elem_size == 2) return kElemU16;
    if (elem_size == 4) return kElemF32;
    throw std::runtime_error(std::string(who) +
                             ": depth must be uint16 or float32");
}

}  // namespace


// ---- Raw byte -> float converters (EngineInternal.h) -----------------------

void uint8_image_to_float_raw(const uint8_t* d_in, float* d_out, int B, int H,
                              int W, int C) {
    run_bytes_to_float(d_in, d_out, (int64_t)B * H * W * C, kElemU8,
                       1.0f / 255.0f, 0.0f);
}

void uint16_image_to_float_raw(const uint16_t* d_in, float* d_out, int B,
                               int H, int W, int C) {
    run_bytes_to_float(d_in, d_out, (int64_t)B * H * W * C, kElemU16,
                       1.0f / 65535.0f, 0.0f);
}

void uint8_normal_to_float_raw(const uint8_t* d_in, float* d_out, int B,
                               int H, int W, int C) {
    run_bytes_to_float(d_in, d_out, (int64_t)B * H * W * C, kElemU8,
                       1.0f / 127.5f, -1.0f);
}

void uint16_depth_to_float_raw(const uint16_t* d_in, float* d_out, int B,
                               int H, int W, int C) {
    run_bytes_to_float(d_in, d_out, (int64_t)B * H * W * C, kElemU16, 1.0f,
                       0.0f);
}

void uint8_image_to_float_tensor(DeviceTensor3D<uint8_t> img_in,
                                 DeviceTensor3D<float> img_out) {
    run_bytes_to_float(img_in.data_ptr(), img_out.data_ptr(),
                       img_in.size<0>() * img_in.size<1>() * img_in.size<2>(),
                       kElemU8, 1.0f / 255.0f, 0.0f);
}

void uint16_image_to_float_tensor(DeviceTensor3D<uint16_t> img_in,
                                  DeviceTensor3D<float> img_out) {
    run_bytes_to_float(img_in.data_ptr(), img_out.data_ptr(),
                       img_in.size<0>() * img_in.size<1>() * img_in.size<2>(),
                       kElemU16, 1.0f / 65535.0f, 0.0f);
}


// ---- Fused byte -> float GT warps (PixelWise.cuh) ---------------------------

void launch_warp_byte_to_float_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_byte, bool input_is_u16,
    int B, int Hin, int Win, int C,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    dispatch_warp("warp.warp_img_wide",
                  make_warp_params(
                      d_intrins, d_dist_coeffs,
                      {d_source_models, d_source_params},
                      d_byte, d_float_out, {d_post_intrins, d_axes},
                      B, Hin, Win, C, K, Hout, Wout,
                      input_is_u16 ? kElemU16 : kElemU8,
                      (CameraModelType)cd.cam, Hin, Win, false,
                      input_is_u16 ? 1.0f / 65535.0f : 1.0f / 255.0f, 0.0f),
                  {cd.dist, from_source_spec(d_source_models)});
}

void launch_warp_byte_to_float_equi(
    const void* d_byte, bool input_is_u16,
    int B, int Hin, int Win, int C,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    dispatch_warp("warp.warp_img_equi",
                  make_warp_params(
                      nullptr, nullptr, {}, d_byte, d_float_out,
                      {d_post_intrins, d_axes},
                      B, Hin, Win, C, K, Hout, Wout,
                      input_is_u16 ? kElemU16 : kElemU8,
                      CameraModelType::EQUIRECTANGULAR, Hin, Win, false,
                      input_is_u16 ? 1.0f / 65535.0f : 1.0f / 255.0f, 0.0f),
                  {});
}

void launch_warp_mask_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const uint8_t* d_byte_mask,
    int B, int Hin, int Win,
    uint8_t* d_byte_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    WarpMaskParams p{};
    p.intrins = vkk::or_fallback(d_intrins);
    p.dist_coeffs = vkk::or_fallback(d_dist_coeffs);
    p.source_models = vkk::or_fallback(d_source_models);
    p.source_params = vkk::or_fallback(d_source_params);
    p.src = vkk::or_fallback(d_byte_mask);
    p.dst = (uint64_t)d_byte_out;
    p.axes = vkk::or_fallback(d_axes);
    p.post_intrins = vkk::or_fallback(d_post_intrins);
    p.B = B; p.Hin = Hin; p.Win = Win;
    p.K = K; p.Hout = Hout; p.Wout = Wout;
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    p.camera_model = (int)cd.cam;
    int64_t words = ((int64_t)B * K * Hout * Wout + 3) / 4;
    vkk::dispatch_flat("warp.warp_mask_wide",
                       {cd.dist, from_source_spec(d_source_models)}, words,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

void launch_warp_mask_equi(
    const uint8_t* d_byte_mask,
    int B, int Hin, int Win,
    uint8_t* d_byte_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    WarpMaskParams p{};
    p.intrins = vkk::or_fallback(nullptr);
    p.dist_coeffs = vkk::or_fallback(nullptr);
    p.source_models = vkk::or_fallback(nullptr);
    p.source_params = vkk::or_fallback(nullptr);
    p.src = vkk::or_fallback(d_byte_mask);
    p.dst = (uint64_t)d_byte_out;
    p.axes = vkk::or_fallback(d_axes);
    p.post_intrins = vkk::or_fallback(d_post_intrins);
    p.B = B; p.Hin = Hin; p.Win = Win;
    p.K = K; p.Hout = Hout; p.Wout = Wout;
    p.camera_model = (int)CameraModelType::EQUIRECTANGULAR;
    int64_t words = ((int64_t)B * K * Hout * Wout + 3) / 4;
    vkk::dispatch_flat("warp.warp_mask_equi", {}, words, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_warp_depth_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_depth, uint32_t elem_size,
    int B, int Hin, int Win,
    int in_H, int in_W,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes, bool input_is_ray_depth)
{
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    dispatch_warp("warp.warp_depth_wide",
                  make_warp_params(
                      d_intrins, d_dist_coeffs,
                      {d_source_models, d_source_params},
                      d_depth, d_float_out, {d_post_intrins, d_axes},
                      B, Hin, Win, 1, K, Hout, Wout,
                      resolve_depth_kind(elem_size, "launch_warp_depth_wide"),
                      (CameraModelType)cd.cam, in_H, in_W, input_is_ray_depth,
                      1.0f, 0.0f),
                  {cd.dist, from_source_spec(d_source_models)});
}

void launch_warp_depth_equi(
    const void* d_depth, uint32_t elem_size,
    int B, int Hin, int Win,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes, bool input_is_ray_depth)
{
    dispatch_warp("warp.warp_depth_equi",
                  make_warp_params(
                      nullptr, nullptr, {}, d_depth, d_float_out,
                      {d_post_intrins, d_axes},
                      B, Hin, Win, 1, K, Hout, Wout,
                      resolve_depth_kind(elem_size, "launch_warp_depth_equi"),
                      CameraModelType::EQUIRECTANGULAR, Hin, Win,
                      input_is_ray_depth, 1.0f, 0.0f),
                  {});
}

void launch_warp_normal_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_normal, uint32_t elem_size,
    int B, int Hin, int Win,
    int in_H, int in_W,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    if (elem_size != 1 && elem_size != 4)
        throw std::runtime_error(
            "launch_warp_normal_wide: normal must be uint8 or float32");
    bool u8 = elem_size == 1;
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    dispatch_warp("warp.warp_normal_wide",
                  make_warp_params(
                      d_intrins, d_dist_coeffs,
                      {d_source_models, d_source_params},
                      d_normal, d_float_out, {d_post_intrins, d_axes},
                      B, Hin, Win, 3, K, Hout, Wout,
                      u8 ? kElemU8 : kElemF32, (CameraModelType)cd.cam, in_H,
                      in_W, false, u8 ? 1.0f / 127.5f : 1.0f,
                      u8 ? -1.0f : 0.0f),
                  {cd.dist, from_source_spec(d_source_models)});
}

void launch_warp_normal_equi(
    const void* d_normal, uint32_t elem_size,
    int B, int Hin, int Win,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    if (elem_size != 1 && elem_size != 4)
        throw std::runtime_error(
            "launch_warp_normal_equi: normal must be uint8 or float32");
    bool u8 = elem_size == 1;
    dispatch_warp("warp.warp_normal_equi",
                  make_warp_params(
                      nullptr, nullptr, {}, d_normal, d_float_out,
                      {d_post_intrins, d_axes},
                      B, Hin, Win, 3, K, Hout, Wout,
                      u8 ? kElemU8 : kElemF32,
                      CameraModelType::EQUIRECTANGULAR, Hin, Win, false,
                      u8 ? 1.0f / 127.5f : 1.0f, u8 ? -1.0f : 0.0f),
                  {});
}


// ---- Re-distort onto the fitted tier (PixelWise.cuh) ------------------------

void launch_redistort_byte_to_float(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_byte, bool input_is_u16,
    int B, int in_H, int in_W, int C,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W,
    float invalid)
{
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    dispatch_warp("warp.redistort_img",
                  make_redistort_params(
                      d_intrins, d_dist_coeffs,
                      {d_source_models, d_source_params},
                      d_byte, d_float_out, B, in_H, in_W, C, out_H, out_W,
                      input_is_u16 ? kElemU16 : kElemU8,
                      (CameraModelType)cd.cam, ref_H, ref_W,
                      input_is_u16 ? 1.0f / 65535.0f : 1.0f / 255.0f, 0.0f,
                      invalid),
                  {cd.dist});
}

// Depth is raw counts either way, so norm_inv stays 1 -- same convention as
// the wide warp.
void launch_redistort_depth(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_in, uint32_t elem_size,
    int B, int in_H, int in_W,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W)
{
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    dispatch_warp("warp.redistort_depth",
                  make_redistort_params(
                      d_intrins, d_dist_coeffs,
                      {d_source_models, d_source_params},
                      d_in, d_float_out, B, in_H, in_W, 1, out_H, out_W,
                      elem_size == 2 ? kElemU16 : kElemF32,
                      (CameraModelType)cd.cam, ref_H, ref_W, 1.0f, 0.0f,
                      0.0f),
                  {cd.dist});
}

void launch_redistort_mask(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const uint8_t* d_byte_mask,
    int B, int in_H, int in_W,
    uint8_t* d_byte_out, int out_H, int out_W,
    int ref_H, int ref_W)
{
    WarpMaskParams p{};
    p.intrins = vkk::or_fallback(d_intrins);
    p.dist_coeffs = vkk::or_fallback(d_dist_coeffs);
    p.source_models = vkk::or_fallback(d_source_models);
    p.source_params = vkk::or_fallback(d_source_params);
    p.src = vkk::or_fallback(d_byte_mask);
    p.dst = (uint64_t)d_byte_out;
    p.axes = vkk::or_fallback(nullptr);
    p.B = B; p.Hin = in_H; p.Win = in_W;
    p.K = 1; p.Hout = out_H; p.Wout = out_W;
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    p.camera_model = (int)cd.cam;
    p.ref_H = ref_H; p.ref_W = ref_W;
    int64_t words = ((int64_t)B * out_H * out_W + 3) / 4;
    vkk::dispatch_flat("warp.redistort_mask", {cd.dist}, words, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}

void launch_redistort_normal(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_in, bool input_is_float,
    int B, int in_H, int in_W,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W)
{
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    dispatch_warp("warp.redistort_normal",
                  make_redistort_params(
                      d_intrins, d_dist_coeffs,
                      {d_source_models, d_source_params},
                      d_in, d_float_out, B, in_H, in_W, 3, out_H, out_W,
                      input_is_float ? kElemF32 : kElemU8,
                      (CameraModelType)cd.cam, ref_H, ref_W,
                      input_is_float ? 1.0f : 1.0f / 127.5f,
                      input_is_float ? 0.0f : -1.0f, 0.0f),
                  {cd.dist});
}
