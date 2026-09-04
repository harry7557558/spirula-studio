// ImageWarp.cu -- the GT image and mask of a wide (fisheye / equirectangular)
// camera resampled into its pinhole faces, byte in and float out in one pass
// so no full-resolution float copy of the frame is ever allocated.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh; the face a pixel
// belongs to is WarpFace.cuh.

#include "kernels/pixelwise/BilinearSample.cuh"
#include "kernels/pixelwise/RedistortSource.cuh"
#include "kernels/pixelwise/WarpFace.cuh"

// ================
// Byte-to-float fused warp kernels
// ================
//
// Output values are normalized to [0, 1] via the `norm_inv` arg (1/255 for
// uint8, 1/65535 for uint16). Output layout is [B, K, H_out, W_out, C].

template<CameraDistortionType distortion, bool from_source, typename T_in>
__global__ void warp_image_wide_to_pinhole_byte_to_float_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,                  // [B, 4]
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<T_in, 4>  wide_image,                     // [B, H, W, C]
    TensorView<float, 5> pinhole_images,                 // [B, K, H_out, W_out, C]
    const float* __restrict__ post_intrins,              // [B*K, 4]
    const float* __restrict__ axes,                      // [B*K, 3, 3]
    float norm_inv                                       // 1/255 or 1/65535
) {
    const int B  = wide_image.shape[0],
              K  = pinhole_images.shape[1],
              Hp = pinhole_images.shape[2],
              Wp = pinhole_images.shape[3],
              C  = wide_image.shape[3];

    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    auto to_pixel = make_ray_to_pixel<distortion, from_source>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);

    for (int ki = 0; ki < K; ++ki) {
        float3 raydir = face_pixel_ray(axes, post_intrins, (long)bid * K + ki, i, j);
        float2 uv;
        bool valid = to_pixel(raydir, &uv);
        if (valid) {
            for (int c = 0; c < C; c++)
                pinhole_images.at(bid, ki, j, i, c) =
                    bilinear_byte_norm<T_in>(wide_image, bid, c, uv.x, uv.y, norm_inv, 0.5f);
        } else {
            for (int c = 0; c < C; c++)
                pinhole_images.at(bid, ki, j, i, c) = 0.5f;
        }
    }
}

template<typename T_in>
__global__ void warp_image_equirectangular_to_pinhole_byte_to_float_kernel(
    TensorView<T_in, 4>  equirectangular_image,           // [B, H, W, C]
    TensorView<float, 5> pinhole_images,                  // [B, K, H_out, W_out, C]
    const float* __restrict__ post_intrins,               // [B*K, 4]
    const float* __restrict__ axes,                       // [B*K, 3, 3]
    float norm_inv
) {
    const int B = equirectangular_image.shape[0],
              h = equirectangular_image.shape[1],
              w = equirectangular_image.shape[2],
              K = pinhole_images.shape[1],
              Hp = pinhole_images.shape[2],
              Wp = pinhole_images.shape[3],
              C  = equirectangular_image.shape[3];

    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    for (int ki = 0; ki < K; ++ki) {
        float3 raydir = face_pixel_ray(axes, post_intrins, (long)bid * K + ki, i, j);
        float2 uv;
        const bool valid = equi_ray_to_uv(raydir, h, w, &uv);
        for (int c = 0; c < C; c++)
            pinhole_images.at(bid, ki, j, i, c) = valid
                ? bilinear_byte_norm_wrap_u<T_in>(
                      equirectangular_image, bid, c, uv.x, uv.y, norm_inv)
                : 0.5f;
    }
}

// Nearest-neighbor mask warps. Mask is bool; we read uint8 (0 / nonzero)
// and emit uint8 0/1 at the post-split resolution. Camera-model dispatch
// matches the RGB kernels.
template<CameraDistortionType distortion, bool from_source>
__global__ void warp_mask_wide_to_pinhole_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<uint8_t, 4> wide_mask,                    // [B, H, W, 1]
    TensorView<uint8_t, 5> pinhole_masks,                // [B, K, H_out, W_out, 1]
    const float* __restrict__ post_intrins,
    const float* __restrict__ axes
) {
    const int B  = wide_mask.shape[0],
              H  = wide_mask.shape[1],
              W  = wide_mask.shape[2],
              K  = pinhole_masks.shape[1],
              Hp = pinhole_masks.shape[2],
              Wp = pinhole_masks.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    auto to_pixel = make_ray_to_pixel<distortion, from_source>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);

    for (int ki = 0; ki < K; ++ki) {
        float3 raydir = face_pixel_ray(axes, post_intrins, (long)bid * K + ki, i, j);
        float2 uv;
        bool valid = to_pixel(raydir, &uv);
        uint8_t out = 0;
        if (valid) {
            int xs = (int)floorf(uv.x);
            int ys = (int)floorf(uv.y);
            if (xs >= 0 && xs < W && ys >= 0 && ys < H) {
                out = (wide_mask.at(bid, ys, xs, 0) != 0) ? 1 : 0;
            }
        }
        pinhole_masks.at(bid, ki, j, i, 0) = out;
    }
}

__global__ void warp_mask_equirectangular_to_pinhole_kernel(
    TensorView<uint8_t, 4> equi_mask,                    // [B, H, W, 1]
    TensorView<uint8_t, 5> pinhole_masks,                // [B, K, H_out, W_out, 1]
    const float* __restrict__ post_intrins,
    const float* __restrict__ axes
) {
    const int B  = equi_mask.shape[0],
              h  = equi_mask.shape[1],
              w  = equi_mask.shape[2],
              K  = pinhole_masks.shape[1],
              Hp = pinhole_masks.shape[2],
              Wp = pinhole_masks.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    for (int ki = 0; ki < K; ++ki) {
        float3 raydir = face_pixel_ray(axes, post_intrins, (long)bid * K + ki, i, j);
        float2 uv;
        uint8_t out = 0;
        if (equi_ray_to_uv(raydir, h, w, &uv)) {
            int xs = (int)floorf(uv.x);
            int ys = (int)floorf(uv.y);
            // wrap u for equirectangular (panoramic continuity); clamp v.
            if (xs < 0) xs += w; else if (xs >= w) xs -= w;
            if (ys < 0) ys = 0; if (ys >= h) ys = h - 1;
            out = (equi_mask.at(bid, ys, xs, 0) != 0) ? 1 : 0;
        }
        pinhole_masks.at(bid, ki, j, i, 0) = out;
    }
}


// ---- Host launchers (raw-pointer, since the engine drives these without
// going through DeviceTensor wrappers). Picks the right kernel by type tag.

/*[AutoHeaderGeneratorExport]*/
void launch_warp_byte_to_float_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,                // [B, 4]
    const float* d_dist_coeffs,            // [B, 8] (nullable -> all-zeros)
    const int*   d_source_models,          // [B] (null unless re-distorting)
    const float* d_source_params,          // [B, 16]
    const void* d_byte, bool input_is_u16,
    int B, int Hin, int Win, int C,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,           // [B*K, 4]
    const float* d_axes                    // [B*K, 3, 3]
) {
    auto make_5 = [&](float* p) {
        TensorView<float, 5> v;
        v.data = p;
        v.shape[0] = B; v.shape[1] = K; v.shape[2] = Hout; v.shape[3] = Wout; v.shape[4] = C;
        long s4 = C, s3 = (long)Wout * s4, s2 = (long)Hout * s3, s1 = (long)K * s2;
        v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = s4; v.strides[4] = 1;
        return v;
    };
    auto make_4_u8 = [&](const uint8_t* p) {
        TensorView<uint8_t, 4> v;
        v.data = const_cast<uint8_t*>(p);
        v.shape[0] = B; v.shape[1] = Hin; v.shape[2] = Win; v.shape[3] = C;
        long s3 = C, s2 = (long)Win * s3, s1 = (long)Hin * s2;
        v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
        return v;
    };
    auto make_4_u16 = [&](const uint16_t* p) {
        TensorView<uint16_t, 4> v;
        v.data = const_cast<uint16_t*>(p);
        v.shape[0] = B; v.shape[1] = Hin; v.shape[2] = Win; v.shape[3] = C;
        long s3 = C, s2 = (long)Win * s3, s1 = (long)Hin * s2;
        v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
        return v;
    };
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intrins_f4 = (const float4*)d_intrins;
    #define LAUNCH(D, FROM)                                                    \
        if (input_is_u16) {                                                    \
            warp_image_wide_to_pinhole_byte_to_float_kernel<D, FROM, uint16_t> \
                <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(               \
                    cm, intrins_f4, dcb, d_source_models, d_source_params,     \
                    make_4_u16((const uint16_t*)d_byte), make_5(d_float_out),  \
                    d_post_intrins, d_axes, 1.0f / 65535.0f);                  \
        } else {                                                               \
            warp_image_wide_to_pinhole_byte_to_float_kernel<D, FROM, uint8_t>  \
                <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(               \
                    cm, intrins_f4, dcb, d_source_models, d_source_params,     \
                    make_4_u8((const uint8_t*)d_byte), make_5(d_float_out),    \
                    d_post_intrins, d_axes, 1.0f / 255.0f);                    \
        }
    _SS_DISPATCH_SOURCE(distortion, d_source_models != nullptr, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void launch_warp_byte_to_float_equi(
    const void* d_byte, bool input_is_u16,
    int B, int Hin, int Win, int C,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    auto make_5 = [&](float* p) {
        TensorView<float, 5> v;
        v.data = p;
        v.shape[0] = B; v.shape[1] = K; v.shape[2] = Hout; v.shape[3] = Wout; v.shape[4] = C;
        long s4 = C, s3 = (long)Wout * s4, s2 = (long)Hout * s3, s1 = (long)K * s2;
        v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = s4; v.strides[4] = 1;
        return v;
    };
    auto make_4_u8 = [&](const uint8_t* p) {
        TensorView<uint8_t, 4> v;
        v.data = const_cast<uint8_t*>(p);
        v.shape[0] = B; v.shape[1] = Hin; v.shape[2] = Win; v.shape[3] = C;
        long s3 = C, s2 = (long)Win * s3, s1 = (long)Hin * s2;
        v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
        return v;
    };
    auto make_4_u16 = [&](const uint16_t* p) {
        TensorView<uint16_t, 4> v;
        v.data = const_cast<uint16_t*>(p);
        v.shape[0] = B; v.shape[1] = Hin; v.shape[2] = Win; v.shape[3] = C;
        long s3 = C, s2 = (long)Win * s3, s1 = (long)Hin * s2;
        v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
        return v;
    };
    if (input_is_u16) {
        warp_image_equirectangular_to_pinhole_byte_to_float_kernel<uint16_t>
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
                make_4_u16((const uint16_t*)d_byte), make_5(d_float_out),
                d_post_intrins, d_axes, 1.0f / 65535.0f);
    } else {
        warp_image_equirectangular_to_pinhole_byte_to_float_kernel<uint8_t>
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
                make_4_u8((const uint8_t*)d_byte), make_5(d_float_out),
                d_post_intrins, d_axes, 1.0f / 255.0f);
    }
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void launch_warp_mask_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,                // [B, 4]
    const float* d_dist_coeffs,            // [B, 8] (nullable)
    const int*   d_source_models,          // [B] (null unless re-distorting)
    const float* d_source_params,          // [B, 16]
    const uint8_t* d_byte_mask,
    int B, int Hin, int Win,
    uint8_t* d_byte_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    TensorView<uint8_t, 4> in_v;
    in_v.data = const_cast<uint8_t*>(d_byte_mask);
    in_v.shape[0] = B; in_v.shape[1] = Hin; in_v.shape[2] = Win; in_v.shape[3] = 1;
    in_v.strides[0] = (long)Hin*Win; in_v.strides[1] = Win; in_v.strides[2] = 1; in_v.strides[3] = 1;

    TensorView<uint8_t, 5> out_v;
    out_v.data = d_byte_out;
    out_v.shape[0] = B; out_v.shape[1] = K; out_v.shape[2] = Hout; out_v.shape[3] = Wout; out_v.shape[4] = 1;
    long s4 = 1, s3 = Wout * s4, s2 = Hout * s3, s1 = K * s2;
    out_v.strides[0] = s1; out_v.strides[1] = s2; out_v.strides[2] = s3; out_v.strides[3] = s4; out_v.strides[4] = 1;

    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intrins_f4 = (const float4*)d_intrins;
    #define LAUNCH(D, FROM) \
        warp_mask_wide_to_pinhole_kernel<D, FROM> \
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>( \
                cm, intrins_f4, dcb, d_source_models, d_source_params, \
                in_v, out_v, d_post_intrins, d_axes)
    _SS_DISPATCH_SOURCE(distortion, d_source_models != nullptr, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void launch_warp_mask_equi(
    const uint8_t* d_byte_mask,
    int B, int Hin, int Win,
    uint8_t* d_byte_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    TensorView<uint8_t, 4> in_v;
    in_v.data = const_cast<uint8_t*>(d_byte_mask);
    in_v.shape[0] = B; in_v.shape[1] = Hin; in_v.shape[2] = Win; in_v.shape[3] = 1;
    in_v.strides[0] = (long)Hin*Win; in_v.strides[1] = Win; in_v.strides[2] = 1; in_v.strides[3] = 1;

    TensorView<uint8_t, 5> out_v;
    out_v.data = d_byte_out;
    out_v.shape[0] = B; out_v.shape[1] = K; out_v.shape[2] = Hout; out_v.shape[3] = Wout; out_v.shape[4] = 1;
    long s4 = 1, s3 = Wout * s4, s2 = Hout * s3, s1 = K * s2;
    out_v.strides[0] = s1; out_v.strides[1] = s2; out_v.strides[2] = s3; out_v.strides[3] = s4; out_v.strides[4] = 1;

    warp_mask_equirectangular_to_pinhole_kernel<<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
        in_v, out_v, d_post_intrins, d_axes);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
