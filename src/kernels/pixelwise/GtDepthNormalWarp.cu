// GtDepthNormalWarp.cu -- ground-truth depth / normal wide -> pinhole warps
// for split-mode supervision.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh; the face a pixel
// belongs to is WarpFace.cuh.

#include "kernels/pixelwise/BilinearSample.cuh"
#include "kernels/pixelwise/RedistortSource.cuh"
#include "kernels/pixelwise/WarpFace.cuh"

// ================
// GT depth / normal wide -> pinhole warps (for split-mode supervision)
// ================
//
// Output matches gt.depth / gt.normal: [B*K, out_H, out_W, 1|3] float at the
// POST-split (render) resolution, for the per-pixel supervision loss.

// Wide-frame depth -> the per-face RAY depth the rasterizer renders: |P| from
// the centre, so one value serves every face. Linear (z) divides by cos(theta)
// off the INPUT axis, and 85 degrees is where that secant reaches 11x.
__forceinline__ __device__ float _wide_depth_to_face_ray_depth(
    float d, float3 raydir, bool input_is_ray_depth
) {
    if (d <= 0.0f) return 0.0f;
    if (input_is_ray_depth) return d;
    float rl = length(raydir);
    float rz = raydir.z;
    if (rz <= 0.08715574f * rl) return 0.0f;   // cos(85 deg)
    return d * rl / rz;
}

template<CameraDistortionType distortion, bool from_source, typename T_in>
__global__ void warp_depth_wide_to_pinhole_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,                  // [B, 4]
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<T_in, 4>  wide_depth,                     // [B, Hd, Wd, 1]
    TensorView<float, 5> pinhole_depth,                  // [B, K, H_out, W_out, 1]
    const float* __restrict__ post_intrins,              // [B*K, 4]
    const float* __restrict__ axes,                      // [B*K, 3, 3]
    int in_H, int in_W,                                  // intrinsics ref res
    float norm_inv,
    bool input_is_ray_depth
) {
    const int B  = wide_depth.shape[0],
              Hd = wide_depth.shape[1],
              Wd = wide_depth.shape[2],
              K  = pinhole_depth.shape[1],
              Hp = pinhole_depth.shape[2],
              Wp = pinhole_depth.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    auto to_pixel = make_ray_to_pixel<distortion, from_source>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);
    // Projected pixel is in intrinsics-reference (RGB input) pixel space;
    // rescale to the GT depth map's own resolution.
    float sx = (float)Wd / (float)in_W, sy = (float)Hd / (float)in_H;

    for (int ki = 0; ki < K; ++ki) {
        float3 raydir = face_pixel_ray(axes, post_intrins, (long)bid * K + ki, i, j);
        float2 uv;
        bool valid = to_pixel(raydir, &uv);
        float out = 0.0f;
        float d;
        if (valid && bilinear_depth_valid<T_in>(
                wide_depth, bid, uv.x * sx, uv.y * sy, norm_inv, false, &d))
            out = _wide_depth_to_face_ray_depth(d, raydir, input_is_ray_depth);
        pinhole_depth.at(bid, ki, j, i, 0) = out;
    }
}

template<typename T_in>
__global__ void warp_depth_equirectangular_to_pinhole_kernel(
    TensorView<T_in, 4>  wide_depth,                      // [B, Hd, Wd, 1]
    TensorView<float, 5> pinhole_depth,                   // [B, K, H_out, W_out, 1]
    const float* __restrict__ post_intrins,
    const float* __restrict__ axes,
    float norm_inv,
    bool input_is_ray_depth
) {
    const int B  = wide_depth.shape[0],
              h  = wide_depth.shape[1],
              w  = wide_depth.shape[2],
              K  = pinhole_depth.shape[1],
              Hp = pinhole_depth.shape[2],
              Wp = pinhole_depth.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    for (int ki = 0; ki < K; ++ki) {
        float3 raydir = face_pixel_ray(axes, post_intrins, (long)bid * K + ki, i, j);
        float2 uv;
        float d;
        pinhole_depth.at(bid, ki, j, i, 0) =
            equi_ray_to_uv(raydir, h, w, &uv) &&
            bilinear_depth_valid<T_in>(wide_depth, bid, uv.x, uv.y, norm_inv,
                                       /*wrap_u=*/true, &d)
                ? _wide_depth_to_face_ray_depth(d, raydir, input_is_ray_depth)
                : 0.0f;
    }
}

// Sample a wide-frame normal (in the INPUT camera frame) and rotate it into a
// pinhole face's camera frame: the face-frame components are the dot products
// with its unit axes. (-1,-1,-1) is the "no data" / degenerate sentinel.
template<typename T_in>
__forceinline__ __device__ float3 _warp_one_normal(
    const TensorView<T_in, 4>& wide_normal, uint32_t bid,
    float x, float y, float norm_inv, float decode_off, bool wrap_u,
    const float* __restrict__ a
) {
    float3 n;
    if (!bilinear_normal_valid<T_in>(wide_normal, bid, x, y, norm_inv,
                                     decode_off, wrap_u, &n))
        return make_float3(-1.0f, -1.0f, -1.0f);
    float3 r = make_float3(a[0]*n.x + a[1]*n.y + a[2]*n.z,
                           a[3]*n.x + a[4]*n.y + a[5]*n.z,
                           a[6]*n.x + a[7]*n.y + a[8]*n.z);
    float rl = length(r);
    if (rl <= 1e-8f) return make_float3(-1.0f, -1.0f, -1.0f);
    return r / rl;
}

template<CameraDistortionType distortion, bool from_source, typename T_in>
__global__ void warp_normal_wide_to_pinhole_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,                  // [B, 4]
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const int *__restrict__ source_models,
    const float *__restrict__ source_params,
    TensorView<T_in, 4>  wide_normal,                    // [B, Hn, Wn, 3]
    TensorView<float, 5> pinhole_normal,                 // [B, K, H_out, W_out, 3]
    const float* __restrict__ post_intrins,              // [B*K, 4]
    const float* __restrict__ axes,                      // [B*K, 3, 3]
    int in_H, int in_W,
    float norm_inv, float decode_off
) {
    const int B  = wide_normal.shape[0],
              Hn = wide_normal.shape[1],
              Wn = wide_normal.shape[2],
              K  = pinhole_normal.shape[1],
              Hp = pinhole_normal.shape[2],
              Wp = pinhole_normal.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    auto to_pixel = make_ray_to_pixel<distortion, from_source>(
        bid, camera_model, intrins, dist_coeffs_buffer, source_models, source_params);
    float sx = (float)Wn / (float)in_W, sy = (float)Hn / (float)in_H;

    for (int ki = 0; ki < K; ++ki) {
        const long p = (long)bid * K + ki;
        float3 raydir = face_pixel_ray(axes, post_intrins, p, i, j);
        float2 uv;
        bool valid = to_pixel(raydir, &uv);
        float3 nf = make_float3(-1.0f, -1.0f, -1.0f);
        if (valid) {
            nf = _warp_one_normal<T_in>(
                wide_normal, bid, uv.x * sx, uv.y * sy, norm_inv, decode_off,
                /*wrap_u=*/false, axes + 9 * p);
        }
        pinhole_normal.at(bid, ki, j, i, 0) = nf.x;
        pinhole_normal.at(bid, ki, j, i, 1) = nf.y;
        pinhole_normal.at(bid, ki, j, i, 2) = nf.z;
    }
}

template<typename T_in>
__global__ void warp_normal_equirectangular_to_pinhole_kernel(
    TensorView<T_in, 4>  wide_normal,                     // [B, Hn, Wn, 3]
    TensorView<float, 5> pinhole_normal,                  // [B, K, H_out, W_out, 3]
    const float* __restrict__ post_intrins,
    const float* __restrict__ axes,
    float norm_inv, float decode_off
) {
    const int B  = wide_normal.shape[0],
              h  = wide_normal.shape[1],
              w  = wide_normal.shape[2],
              K  = pinhole_normal.shape[1],
              Hp = pinhole_normal.shape[2],
              Wp = pinhole_normal.shape[3];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i   = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j   = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= Wp || j >= Hp) return;

    for (int ki = 0; ki < K; ++ki) {
        const long p = (long)bid * K + ki;
        float3 raydir = face_pixel_ray(axes, post_intrins, p, i, j);
        float2 uv;
        float3 nf = make_float3(-1.0f, -1.0f, -1.0f);
        if (equi_ray_to_uv(raydir, h, w, &uv))
            nf = _warp_one_normal<T_in>(
                wide_normal, bid, uv.x, uv.y, norm_inv, decode_off,
                /*wrap_u=*/true, axes + 9 * p);
        pinhole_normal.at(bid, ki, j, i, 0) = nf.x;
        pinhole_normal.at(bid, ki, j, i, 1) = nf.y;
        pinhole_normal.at(bid, ki, j, i, 2) = nf.z;
    }
}


// ---- Host launchers for the GT depth / normal warps -------------------------

// Build a [B, Hin, Win, C] input TensorView over a raw device pointer.
template<typename T>
static inline TensorView<T, 4> _make_tv4_in(const T* p, int B, int Hin, int Win, int C) {
    TensorView<T, 4> v;
    v.data = const_cast<T*>(p);
    v.shape[0] = B; v.shape[1] = Hin; v.shape[2] = Win; v.shape[3] = C;
    long s3 = C, s2 = (long)Win * s3, s1 = (long)Hin * s2;
    v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = 1;
    return v;
}

static inline TensorView<float, 5> _make_tv5_out(
    float* p, int B, int K, int Hout, int Wout, int C) {
    TensorView<float, 5> v;
    v.data = p;
    v.shape[0] = B; v.shape[1] = K; v.shape[2] = Hout; v.shape[3] = Wout; v.shape[4] = C;
    long s4 = C, s3 = (long)Wout * s4, s2 = (long)Hout * s3, s1 = (long)K * s2;
    v.strides[0] = s1; v.strides[1] = s2; v.strides[2] = s3; v.strides[3] = s4; v.strides[4] = 1;
    return v;
}

/*[AutoHeaderGeneratorExport]*/
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
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intrins_f4 = (const float4*)d_intrins;
    auto out_v = _make_tv5_out(d_float_out, B, K, Hout, Wout, 1);
    if (elem_size != 2 && elem_size != 4)
        throw std::runtime_error("launch_warp_depth_wide: depth must be uint16 or float32");
    #define LAUNCH(D, FROM)                                                       \
        if (elem_size == 2) {                                                     \
            warp_depth_wide_to_pinhole_kernel<D, FROM, uint16_t>                  \
                <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(                  \
                    cm, intrins_f4, dcb, d_source_models, d_source_params,        \
                    _make_tv4_in<uint16_t>((const uint16_t*)d_depth, B, Hin, Win, 1), \
                    out_v, d_post_intrins, d_axes, in_H, in_W, 1.0f,              \
                    input_is_ray_depth);                                          \
        } else {                                                                  \
            warp_depth_wide_to_pinhole_kernel<D, FROM, float>                     \
                <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(                  \
                    cm, intrins_f4, dcb, d_source_models, d_source_params,        \
                    _make_tv4_in<float>((const float*)d_depth, B, Hin, Win, 1),   \
                    out_v, d_post_intrins, d_axes, in_H, in_W, 1.0f,              \
                    input_is_ray_depth);                                          \
        }
    _SS_DISPATCH_SOURCE(distortion, d_source_models != nullptr, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void launch_warp_depth_equi(
    const void* d_depth, uint32_t elem_size,
    int B, int Hin, int Win,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes, bool input_is_ray_depth)
{
    auto out_v = _make_tv5_out(d_float_out, B, K, Hout, Wout, 1);
    if (elem_size == 2) {
        warp_depth_equirectangular_to_pinhole_kernel<uint16_t>
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
                _make_tv4_in<uint16_t>((const uint16_t*)d_depth, B, Hin, Win, 1),
                out_v, d_post_intrins, d_axes, 1.0f, input_is_ray_depth);
    } else if (elem_size == 4) {
        warp_depth_equirectangular_to_pinhole_kernel<float>
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
                _make_tv4_in<float>((const float*)d_depth, B, Hin, Win, 1),
                out_v, d_post_intrins, d_axes, 1.0f, input_is_ray_depth);
    } else {
        throw std::runtime_error("launch_warp_depth_equi: depth must be uint16 or float32");
    }
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
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
    CameraModelType cm = cmt(camera_model);
    CameraDistortionCoeffsBuffer dcb(const_cast<float*>(d_dist_coeffs));
    const float4* intrins_f4 = (const float4*)d_intrins;
    auto out_v = _make_tv5_out(d_float_out, B, K, Hout, Wout, 3);
    if (elem_size != 1 && elem_size != 4)
        throw std::runtime_error("launch_warp_normal_wide: normal must be uint8 or float32");
    #define LAUNCH(D, FROM)                                                       \
        if (elem_size == 1) {                                                     \
            warp_normal_wide_to_pinhole_kernel<D, FROM, uint8_t>                  \
                <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(                  \
                    cm, intrins_f4, dcb, d_source_models, d_source_params,        \
                    _make_tv4_in<uint8_t>((const uint8_t*)d_normal, B, Hin, Win, 3), \
                    out_v, d_post_intrins, d_axes, in_H, in_W, 1.0f / 127.5f,     \
                    -1.0f);                                                       \
        } else {                                                                  \
            warp_normal_wide_to_pinhole_kernel<D, FROM, float>                    \
                <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(                  \
                    cm, intrins_f4, dcb, d_source_models, d_source_params,        \
                    _make_tv4_in<float>((const float*)d_normal, B, Hin, Win, 3),  \
                    out_v, d_post_intrins, d_axes, in_H, in_W, 1.0f, 0.0f);       \
        }
    _SS_DISPATCH_SOURCE(distortion, d_source_models != nullptr, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void launch_warp_normal_equi(
    const void* d_normal, uint32_t elem_size,
    int B, int Hin, int Win,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_post_intrins,
    const float* d_axes)
{
    auto out_v = _make_tv5_out(d_float_out, B, K, Hout, Wout, 3);
    if (elem_size == 1) {
        warp_normal_equirectangular_to_pinhole_kernel<uint8_t>
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
                _make_tv4_in<uint8_t>((const uint8_t*)d_normal, B, Hin, Win, 3),
                out_v, d_post_intrins, d_axes, 1.0f / 127.5f, -1.0f);
    } else if (elem_size == 4) {
        warp_normal_equirectangular_to_pinhole_kernel<float>
            <<<_LAUNCH_ARGS_3D(Wout, Hout, B, 16, 16, 1)>>>(
                _make_tv4_in<float>((const float*)d_normal, B, Hin, Win, 3),
                out_v, d_post_intrins, d_axes, 1.0f, 0.0f);
    } else {
        throw std::runtime_error("launch_warp_normal_equi: normal must be uint8 or float32");
    }
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
