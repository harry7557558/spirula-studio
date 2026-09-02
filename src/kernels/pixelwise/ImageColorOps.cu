// ImageColorOps.cu -- background blending (plain + random noise), the working
// space -> display transfer, overexposure regularization.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/PixelWiseCommon.cuh"

// 2 * weight / N for L = weight * mean(max(-x, x-1, 0)^2) over N = B*H*W*3.
static inline float _overexposure_scale(long b, long h, long w, float weight) {
    if (weight == 0.0f) return 0.0f;
    double n = (double)b * (double)h * (double)w * 3.0;
    return (float)(2.0 * (double)weight / n);
}

// Transfer and linearity are compile-time axes so the curve and its decode
// fold away; the transfer values are colorspace::Transfer (core/ColorSpace.h).
#define _XFER_PICK1(fn, lin, t)                                               \
    ((t) == 1 ? fn<1, lin> : (t) == 2 ? fn<2, lin> : (t) == 3 ? fn<3, lin> :  \
     (t) == 4 ? fn<4, lin> : fn<0, lin>)
#define _XFER_PICK(fn, t, lin)                                                \
    ((lin) ? _XFER_PICK1(fn, true, t) : _XFER_PICK1(fn, false, t))


// ================
// Blend Background
// ================

__global__ void blend_background_forward_kernel(
    const TensorView<float, 4> in_rgb,
    const TensorView<float, 4> in_transmittance,
    const TensorView<float, 4> in_background,
    TensorView<float, 4> out_rgb
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_rgb.shape[0], H = in_rgb.shape[1], W = in_rgb.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 rgb = in_rgb.load3(bid, y, x);
    float transmittance = in_transmittance.load1(bid, y, x);
    float3 background = in_background.load3(bid, y, x);

    rgb = SlangPixelWise::blend_background(rgb, transmittance, background);

    out_rgb.store3(bid, y, x, rgb);
}

__global__ void blend_background_backward_kernel(
    const TensorView<float, 4> in_rgb,
    const TensorView<float, 4> in_transmittance,
    const TensorView<float, 4> in_background,
    const float overexposure_scale,
    const TensorView<float, 4> v_out_rgb,
    TensorView<float, 4> v_in_rgb,
    TensorView<float, 4> v_in_transmittance,
    TensorView<float, 4> v_in_background
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_rgb.shape[0], H = in_rgb.shape[1], W = in_rgb.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 rgb = in_rgb.load3(bid, y, x);
    float transmittance = in_transmittance.load1(bid, y, x);
    float3 background = in_background.load3(bid, y, x);

    float3 v_out = v_out_rgb.load3(bid, y, x);

    float3 v_rgb; float v_transmittance; float3 v_background;
    SlangPixelWise::blend_background_bwd(
        rgb, transmittance, background,
        v_out, overexposure_scale,
        &v_rgb, &v_transmittance, &v_background
    );

    v_in_rgb.store3(bid, y, x, v_rgb);
    v_in_transmittance.store1(bid, y, x, v_transmittance);
    v_in_background.store3(bid, y, x, v_background);

}

/*[AutoHeaderGeneratorExport]*/
void blend_background_forward(
    DeviceTensor3D<float3> rgb,           // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance, // [B, H, W, 1]
    DeviceTensor3D<float3> background,    // [B, H, W, 3]
    DeviceTensor3D<float3> out_rgb        // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    blend_background_forward_kernel<<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance), _dt3d_to_tv4<float>(background),
        _dt3d_to_tv4<float>(out_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void blend_background_backward(
    DeviceTensor3D<float3> rgb,              // [B, H, W, 3] PRE-blend
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    DeviceTensor3D<float3> background,       // [B, H, W, 3]
    float overexposure_weight,               // fused image-space reg, 0 = off
    DeviceTensor3D<float3> v_out_rgb,        // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb,            // [B, H, W, 3]
    DeviceTensor3D<float>  v_transmittance,  // [B, H, W, 1]
    DeviceTensor3D<float3> v_background      // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    blend_background_backward_kernel<<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance), _dt3d_to_tv4<float>(background),
        _overexposure_scale(b, h, w, overexposure_weight),
        _dt3d_to_tv4<float>(v_out_rgb),
        _dt3d_to_tv4<float>(v_rgb), _dt3d_to_tv4<float>(v_transmittance), _dt3d_to_tv4<float>(v_background)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Blend Background with Random Noise
// ================

template<int Transfer, bool IsLinear>
__global__ void blend_background_noise_forward_kernel(
    const TensorView<float, 4> in_rgb,
    const TensorView<float, 4> in_transmittance,
    const float randomize_weight,
    const uint32_t seed,
    TensorView<float, 4> out_rgb
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_rgb.shape[0], H = in_rgb.shape[1], W = in_rgb.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 rgb = in_rgb.load3(bid, y, x);
    float transmittance = in_transmittance.load1(bid, y, x);

    float3 background;
    background.x = (float)hash_uint3(seed + 0, gid, bid) * exp2f(-32.0f);
    background.y = (float)hash_uint3(seed + 1, gid, bid) * exp2f(-32.0f);
    background.z = (float)hash_uint3(seed + 2, gid, bid) * exp2f(-32.0f);
    background = 0.5 + 0.5*randomize_weight * (2.0f * background - 1.0f);
    background = SlangPixelWise::display_to_working3(background, Transfer, IsLinear);

    rgb = SlangPixelWise::blend_background(rgb, transmittance, background);

    out_rgb.store3(bid, y, x, rgb);
}

template<int Transfer, bool IsLinear>
__global__ void blend_background_noise_backward_kernel(
    const TensorView<float, 4> in_rgb,
    const TensorView<float, 4> in_transmittance,
    const float randomize_weight,
    const uint32_t seed,
    const float overexposure_scale,
    const TensorView<float, 4> v_out_rgb,
    TensorView<float, 4> v_in_rgb,
    TensorView<float, 4> v_in_transmittance
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_rgb.shape[0], H = in_rgb.shape[1], W = in_rgb.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 rgb = in_rgb.load3(bid, y, x);
    float transmittance = in_transmittance.load1(bid, y, x);

    float3 background;
    background.x = (float)hash_uint3(seed + 0, gid, bid) * exp2f(-32.0f);
    background.y = (float)hash_uint3(seed + 1, gid, bid) * exp2f(-32.0f);
    background.z = (float)hash_uint3(seed + 2, gid, bid) * exp2f(-32.0f);
    background = 0.5 + 0.5*randomize_weight * (2.0f * background - 1.0f);
    background = SlangPixelWise::display_to_working3(background, Transfer, IsLinear);

    float3 v_out = v_out_rgb.load3(bid, y, x);

    float3 v_rgb; float v_transmittance; float3 v_background;
    SlangPixelWise::blend_background_bwd(
        rgb, transmittance, background,
        v_out, overexposure_scale,
        &v_rgb, &v_transmittance, &v_background
    );

    v_in_rgb.store3(bid, y, x, v_rgb);
    v_in_transmittance.store1(bid, y, x, v_transmittance);
}

/*[AutoHeaderGeneratorExport]*/
void blend_background_noise_forward(
    int transfer,
    bool is_linear,
    DeviceTensor3D<float3> rgb,           // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance, // [B, H, W, 1]
    float randomize_weight,
    uint32_t seed,
    DeviceTensor3D<float3> out_rgb        // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    _XFER_PICK(blend_background_noise_forward_kernel, transfer, is_linear)
    <<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance),
        randomize_weight, seed,
        _dt3d_to_tv4<float>(out_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void blend_background_noise_backward(
    int transfer,
    bool is_linear,
    DeviceTensor3D<float3> rgb,              // [B, H, W, 3] PRE-blend
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    float randomize_weight,
    uint32_t seed,
    float overexposure_weight,               // fused image-space reg, 0 = off
    DeviceTensor3D<float3> v_out_rgb,        // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb,            // [B, H, W, 3]
    DeviceTensor3D<float>  v_transmittance   // [B, H, W, 1]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    _XFER_PICK(blend_background_noise_backward_kernel, transfer, is_linear)
    <<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance),
        randomize_weight, seed,
        _overexposure_scale(b, h, w, overexposure_weight),
        _dt3d_to_tv4<float>(v_out_rgb),
        _dt3d_to_tv4<float>(v_rgb), _dt3d_to_tv4<float>(v_transmittance)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Working Space -> Display
// ================

template<int Transfer, bool IsLinear>
__global__ void working_to_display_forward_kernel(
    const TensorView<float, 4> in_rgb,
    const float* __restrict__ color_matrix_buffer,
    TensorView<float, 4> out_rgb
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_rgb.shape[0], H = in_rgb.shape[1], W = in_rgb.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 rgb = in_rgb.load3(bid, y, x);

    float3x3 color_matrix;
    color_matrix[0].x = color_matrix_buffer[0];
    color_matrix[0].y = color_matrix_buffer[1];
    color_matrix[0].z = color_matrix_buffer[2];
    color_matrix[1].x = color_matrix_buffer[3];
    color_matrix[1].y = color_matrix_buffer[4];
    color_matrix[1].z = color_matrix_buffer[5];
    color_matrix[2].x = color_matrix_buffer[6];
    color_matrix[2].y = color_matrix_buffer[7];
    color_matrix[2].z = color_matrix_buffer[8];

    rgb = SlangPixelWise::working_to_display(rgb, color_matrix, Transfer, IsLinear);

    out_rgb.store3(bid, y, x, rgb);
}

template<int Transfer, bool IsLinear>
__global__ void working_to_display_backward_kernel(
    const TensorView<float, 4> in_rgb,
    const float* __restrict__ color_matrix_buffer,
    const TensorView<float, 4> v_out_rgb,
    TensorView<float, 4> v_in_rgb
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = in_rgb.shape[0], H = in_rgb.shape[1], W = in_rgb.shape[2];
    if (bid >= B || gid >= H*W)
        return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 rgb = in_rgb.load3(bid, y, x);

    float3x3 color_matrix;
    color_matrix[0].x = color_matrix_buffer[0];
    color_matrix[0].y = color_matrix_buffer[1];
    color_matrix[0].z = color_matrix_buffer[2];
    color_matrix[1].x = color_matrix_buffer[3];
    color_matrix[1].y = color_matrix_buffer[4];
    color_matrix[1].z = color_matrix_buffer[5];
    color_matrix[2].x = color_matrix_buffer[6];
    color_matrix[2].y = color_matrix_buffer[7];
    color_matrix[2].z = color_matrix_buffer[8];

    float3 v_out = v_out_rgb.load3(bid, y, x);

    float3 v_rgb = SlangPixelWise::working_to_display_bwd(
        rgb, color_matrix, Transfer, IsLinear, v_out);

    v_in_rgb.store3(bid, y, x, v_rgb);
}

/*[AutoHeaderGeneratorExport]*/
void working_to_display_forward(
    int transfer,                        // colorspace::Transfer
    bool is_linear,                      // does the source store linear light
    DeviceTensor3D<float3> rgb,          // [B, H, W, 3]
    DeviceTensor2D<float3> color_matrix, // [3, 3] stored as 3 float3
    DeviceTensor3D<float3> out_rgb       // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    _XFER_PICK(working_to_display_forward_kernel, transfer, is_linear)
    <<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb),
        (float*)color_matrix.data_ptr(),
        _dt3d_to_tv4<float>(out_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void working_to_display_backward(
    int transfer,
    bool is_linear,
    DeviceTensor3D<float3> rgb,          // [B, H, W, 3]
    DeviceTensor2D<float3> color_matrix, // [3, 3] stored as 3 float3
    DeviceTensor3D<float3> v_out_rgb,    // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb         // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    _XFER_PICK(working_to_display_backward_kernel, transfer, is_linear)
    <<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb),
        (float*)color_matrix.data_ptr(),
        _dt3d_to_tv4<float>(v_out_rgb),
        _dt3d_to_tv4<float>(v_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Overexposure Regularization
// ================

// Gradient of L = weight * mean(max(-x, x-1, 0)^2), added into v_rgb in place;
// the scalar loss is never materialized. Only for the no-background path: with
// a blend enabled the same term is fused there, onto the UNCLAMPED composite.
__global__ void overexposure_grad_add_kernel(
    const TensorView<float, 4> rgb,
    const float scale,
    TensorView<float, 4> v_rgb
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = rgb.shape[0], H = rgb.shape[1], W = rgb.shape[2];
    if (bid >= B || gid >= H * W) return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 c = rgb.load3(bid, y, x);
    float3 v = v_rgb.load3(bid, y, x);
    float3 g = SlangPixelWise::overexposure_grad(c, scale);
    v.x += g.x;
    v.y += g.y;
    v.z += g.z;

    v_rgb.store3(bid, y, x, v);
}

/*[AutoHeaderGeneratorExport]*/
void overexposure_grad_add(
    DeviceTensor3D<float3> rgb,    // [B, H, W, 3]
    float weight,                  // L = weight * mean(max(-x, x-1, 0)^2)
    DeviceTensor3D<float3> v_rgb   // [B, H, W, 3], in/out
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();
    if (b <= 0 || h <= 0 || w <= 0 || weight == 0.0f) return;

    overexposure_grad_add_kernel<<<_LAUNCH_ARGS_2D(h * w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb),
        _overexposure_scale(b, h, w, weight),
        _dt3d_to_tv4<float>(v_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


