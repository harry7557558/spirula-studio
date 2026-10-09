// ImageRedistort.cu -- resample GT images from a camera whose lens model no
// distortion tier represents onto the tier the parser fitted for it.
//
// The fit does not change the pose, so a destination pixel and its source pixel
// are the SAME ray: depth (linear or ray) and camera-frame normals transfer
// unchanged, and only the sampling coordinate moves. That is why these kernels
// carry none of the point-space handling GtDepthNormalWarp.cu needs -- there
// the destination is a rotated cubemap face.
//
// The fused fisheye case does not come through here at all: when
// warp_to_pinhole is on, the wide->pinhole warps take the source projection
// directly (RayToPixel<D, true>), so the fitted camera is never materialized
// and no intermediate image is allocated.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/BilinearSample.cuh"
#include "kernels/pixelwise/RedistortSource.cuh"

// ================
// Re-distort
// ================

// Destination pixel -> source pixel. Both grids are expressed relative to the
// intrinsics reference (the RGB resolution), and they need not match: a depth
// or normal map carries its own size, so `scale_out` and `scale_in` differ.
template<CameraDistortionType distortion>
__forceinline__ __device__ bool _redistort_lookup(
    const RayToPixel<distortion, true>& to_source,
    CameraModelType camera_model, float4 intrin,
    const typename SlangDistortion<distortion>::Coeffs& coeffs,
    float2 scale_out, float2 scale_in,
    uint32_t i, uint32_t j, float2* uv_src
) {
    float2 uv = {
        (((float)i + 0.5f) / scale_out.x - intrin.z) / intrin.x,
        (((float)j + 0.5f) / scale_out.y - intrin.w) / intrin.y
    };
    float3 raydir;
    if (!SlangDistortion<distortion>::unproject_point(
            uv, (int)camera_model, coeffs, &raydir))
        return false;
    if (!to_source(raydir, uv_src))
        return false;
    uv_src->x *= scale_in.x;
    uv_src->y *= scale_in.y;
    return true;
}

template<CameraDistortionType distortion, typename T_in>
__global__ void redistort_image_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,                  // [B, 4] fitted
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,               // [B]
    const float *__restrict__ source_params,             // [B, 16]
    TensorView<T_in, 4>  in_image,                       // [B, H, W, C]
    TensorView<float, 4> out_image,                      // [B, H, W, C]
    int ref_H, int ref_W,                                // intrinsics ref res
    float norm_inv, float invalid
) {
    const int B = in_image.shape[0],
              H = out_image.shape[1],
              W = out_image.shape[2],
              C = in_image.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H) return;

    float4 intrin = intrins[bid];
    auto coeffs = dist_coeffs_buffer.load<distortion>(bid);
    auto to_source = make_ray_to_pixel<distortion, true>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);
    float2 scale_out = { (float)W / (float)ref_W, (float)H / (float)ref_H };
    float2 scale_in  = { (float)in_image.shape[2] / (float)ref_W,
                         (float)in_image.shape[1] / (float)ref_H };

    float2 uv_src;
    bool valid = _redistort_lookup<distortion>(
        to_source, camera_model, intrin, coeffs, scale_out, scale_in, i, j, &uv_src);
    for (int c = 0; c < C; c++) {
        out_image.at(bid, j, i, c) = valid
            ? bilinear_byte_norm<T_in>(in_image, bid, c, uv_src.x, uv_src.y,
                                       norm_inv, invalid)
            : invalid;
    }
}

// Depth keeps its 0 = "no ground truth here" sentinel out of the blend (see
// BilinearSample.cuh), which the plain image kernel above cannot do -- there
// the padding value IS a colour.
template<CameraDistortionType distortion, typename T_in>
__global__ void redistort_depth_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<T_in, 4>  in_depth,                       // [B, H, W, 1]
    TensorView<float, 4> out_depth,                      // [B, H, W, 1]
    int ref_H, int ref_W,
    float norm_inv
) {
    const int B = in_depth.shape[0],
              H = out_depth.shape[1],
              W = out_depth.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H) return;

    float4 intrin = intrins[bid];
    auto coeffs = dist_coeffs_buffer.load<distortion>(bid);
    auto to_source = make_ray_to_pixel<distortion, true>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);
    float2 scale_out = { (float)W / (float)ref_W, (float)H / (float)ref_H };
    float2 scale_in  = { (float)in_depth.shape[2] / (float)ref_W,
                         (float)in_depth.shape[1] / (float)ref_H };

    float d = 0.0f;
    float2 uv_src;
    if (_redistort_lookup<distortion>(
            to_source, camera_model, intrin, coeffs, scale_out, scale_in, i, j, &uv_src))
        bilinear_depth_valid<T_in>(in_depth, bid, uv_src.x, uv_src.y,
                                   norm_inv, /*wrap_u=*/false, &d);
    out_depth.at(bid, j, i, 0) = d;
}

// Nearest lookup, and out-of-frame is masked OUT rather than padded: the fitted
// camera can see a little past the source image, exactly as the wide->pinhole
// mask warp does.
template<CameraDistortionType distortion>
__global__ void redistort_mask_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<uint8_t, 4> in_mask,                      // [B, H, W, 1]
    TensorView<uint8_t, 4> out_mask,                     // [B, H, W, 1]
    int ref_H, int ref_W
) {
    const int B = in_mask.shape[0],
              Hin = in_mask.shape[1],
              Win = in_mask.shape[2],
              H = out_mask.shape[1],
              W = out_mask.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H) return;

    float4 intrin = intrins[bid];
    auto coeffs = dist_coeffs_buffer.load<distortion>(bid);
    auto to_source = make_ray_to_pixel<distortion, true>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);
    float2 scale_out = { (float)W / (float)ref_W, (float)H / (float)ref_H };
    float2 scale_in  = { (float)Win / (float)ref_W, (float)Hin / (float)ref_H };

    float2 uv_src;
    uint8_t out = 0;
    if (_redistort_lookup<distortion>(
            to_source, camera_model, intrin, coeffs, scale_out, scale_in, i, j, &uv_src)) {
        int xs = (int)floorf(uv_src.x);
        int ys = (int)floorf(uv_src.y);
        if (xs >= 0 && xs < Win && ys >= 0 && ys < Hin)
            out = in_mask.at(bid, ys, xs, 0);
    }
    out_mask.at(bid, j, i, 0) = out;
}

// Normals are re-normalized after the bilinear blend and keep the (-1,-1,-1)
// "no data" sentinel, matching _warp_one_normal.
template<CameraDistortionType distortion, typename T_in>
__global__ void redistort_normal_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<T_in, 4>  in_normal,                      // [B, H, W, 3]
    TensorView<float, 4> out_normal,                     // [B, H, W, 3]
    int ref_H, int ref_W,
    float norm_inv, float decode_off
) {
    const int B = in_normal.shape[0],
              H = out_normal.shape[1],
              W = out_normal.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H) return;

    float4 intrin = intrins[bid];
    auto coeffs = dist_coeffs_buffer.load<distortion>(bid);
    auto to_source = make_ray_to_pixel<distortion, true>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);
    float2 scale_out = { (float)W / (float)ref_W, (float)H / (float)ref_H };
    float2 scale_in  = { (float)in_normal.shape[2] / (float)ref_W,
                         (float)in_normal.shape[1] / (float)ref_H };

    float3 n = make_float3(-1.0f, -1.0f, -1.0f);
    float2 uv_src;
    if (_redistort_lookup<distortion>(
            to_source, camera_model, intrin, coeffs, scale_out, scale_in, i, j, &uv_src)) {
        float3 s;
        if (bilinear_normal_valid<T_in>(in_normal, bid, uv_src.x, uv_src.y,
                                        norm_inv, decode_off,
                                        /*wrap_u=*/false, &s)) {
            float sl = length(s);
            // The pose is unchanged, so a camera-frame normal needs no rotation.
            if (sl > 1e-8f) n = s / sl;
        }
    }
    out_normal.at(bid, j, i, 0) = n.x;
    out_normal.at(bid, j, i, 1) = n.y;
    out_normal.at(bid, j, i, 2) = n.z;
}


// ---- Host launchers ------------------------------------------------------

namespace {

TensorView<float, 4> _rd_f32(float* p, int B, int H, int W, int C) {
    TensorView<float, 4> v;
    v.data = p;
    v.shape[0] = B; v.shape[1] = H; v.shape[2] = W; v.shape[3] = C;
    long s3 = C, s2 = (long)W * s3, s1 = (long)H * s2;
    v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
    return v;
}

template<typename T>
TensorView<T, 4> _rd_in(const T* p, int B, int H, int W, int C) {
    TensorView<T, 4> v;
    v.data = const_cast<T*>(p);
    v.shape[0] = B; v.shape[1] = H; v.shape[2] = W; v.shape[3] = C;
    long s3 = C, s2 = (long)W * s3, s1 = (long)H * s2;
    v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
    return v;
}

}  // namespace

/*[AutoHeaderGeneratorExport]*/
void launch_redistort_byte_to_float(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,                // [B, 4] fitted
    const float* d_dist_coeffs,            // [B, 8] (nullable -> zeros)
    const int*   d_source_models,          // [B]
    const float* d_source_params,          // [B, 16]
    const void* d_byte, bool input_is_u16,
    int B, int in_H, int in_W, int C,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W,
    float invalid)
{
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intr = (const float4*)d_intrins;
    auto out = _rd_f32(d_float_out, B, out_H, out_W, C);

    #define _RD_LAUNCH(D)                                                        \
        if (input_is_u16)                                                        \
            redistort_image_kernel<D, uint16_t>                                  \
                <<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(                       \
                    cm, intr, dcb, d_source_models, d_source_params,             \
                    _rd_in((const uint16_t*)d_byte, B, in_H, in_W, C), out,            \
                    ref_H, ref_W, 1.0f / 65535.0f, invalid);                     \
        else                                                                     \
            redistort_image_kernel<D, uint8_t>                                   \
                <<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(                       \
                    cm, intr, dcb, d_source_models, d_source_params,             \
                    _rd_in((const uint8_t*)d_byte, B, in_H, in_W, C), out,             \
                    ref_H, ref_W, 1.0f / 255.0f, invalid);
    _SS_DISPATCH_DISTORTION(distortion, _RD_LAUNCH);
    #undef _RD_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void launch_redistort_depth(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_in, uint32_t elem_size,   // 2 = uint16 raw counts, 4 = float
    int B, int in_H, int in_W,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W)
{
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intr = (const float4*)d_intrins;
    auto out = _rd_f32(d_float_out, B, out_H, out_W, 1);

    // Depth is raw counts either way, so norm_inv stays 1 -- same convention as
    // the wide warp.
    #define _RD_LAUNCH(D)                                                        \
        if (elem_size == 2)                                                      \
            redistort_depth_kernel<D, uint16_t>                                  \
                <<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(               \
                    cm, intr, dcb, d_source_models, d_source_params,             \
                    _rd_in((const uint16_t*)d_in, B, in_H, in_W, 1), out,        \
                    ref_H, ref_W, 1.0f);                                         \
        else                                                                     \
            redistort_depth_kernel<D, float>                                     \
                <<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(               \
                    cm, intr, dcb, d_source_models, d_source_params,             \
                    _rd_in((const float*)d_in, B, in_H, in_W, 1), out,           \
                    ref_H, ref_W, 1.0f);
    _SS_DISPATCH_DISTORTION(distortion, _RD_LAUNCH);
    #undef _RD_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
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
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intr = (const float4*)d_intrins;
    auto in_v  = _rd_in(d_byte_mask, B, in_H, in_W, 1);
    auto out_v = _rd_in((const uint8_t*)d_byte_out, B, out_H, out_W, 1);

    #define _RD_LAUNCH(D)                                                        \
        redistort_mask_kernel<D><<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(       \
            cm, intr, dcb, d_source_models, d_source_params,                     \
            in_v, out_v, ref_H, ref_W);
    _SS_DISPATCH_DISTORTION(distortion, _RD_LAUNCH);
    #undef _RD_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
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
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intr = (const float4*)d_intrins;
    auto out = _rd_f32(d_float_out, B, out_H, out_W, 3);

    #define _RD_LAUNCH(D)                                                        \
        if (input_is_float)                                                      \
            redistort_normal_kernel<D, float>                                    \
                <<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(                       \
                    cm, intr, dcb, d_source_models, d_source_params,             \
                    _rd_in((const float*)d_in, B, in_H, in_W, 3), out,                 \
                    ref_H, ref_W, 1.0f, 0.0f);                                   \
        else                                                                     \
            redistort_normal_kernel<D, uint8_t>                                  \
                <<<_LAUNCH_ARGS_3D(out_W, out_H, B, 16, 16, 1)>>>(                       \
                    cm, intr, dcb, d_source_models, d_source_params,             \
                    _rd_in((const uint8_t*)d_in, B, in_H, in_W, 3), out,               \
                    ref_H, ref_W, 1.0f / 127.5f, -1.0f);
    _SS_DISPATCH_DISTORTION(distortion, _RD_LAUNCH);
    #undef _RD_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
