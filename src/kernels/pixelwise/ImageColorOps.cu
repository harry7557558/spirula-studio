// ImageColorOps.cu -- background blending (plain + random noise), log map
// image (linear <-> sRGB), overexposure regularization.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/PixelWiseCommon.cuh"

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
        v_out,
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
    DeviceTensor3D<float3> rgb,              // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    DeviceTensor3D<float3> background,       // [B, H, W, 3]
    DeviceTensor3D<float3> v_out_rgb,        // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb,            // [B, H, W, 3]
    DeviceTensor3D<float>  v_transmittance,  // [B, H, W, 1]
    DeviceTensor3D<float3> v_background      // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    blend_background_backward_kernel<<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance), _dt3d_to_tv4<float>(background),
        _dt3d_to_tv4<float>(v_out_rgb),
        _dt3d_to_tv4<float>(v_rgb), _dt3d_to_tv4<float>(v_transmittance), _dt3d_to_tv4<float>(v_background)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Blend Background with Random Noise
// ================

// Side, in pixels, of a `blocky` background tile. Per-pixel noise is averaged
// back into flat grey by SSIM's 11x11 window and by the multi-scale pyramid,
// which is where the penalty is supposed to land; a tile this size survives both.
static constexpr unsigned kBgBlockPx = 64u;

// Murmur-style finalizer. Cheap, well-distributed, and stateless -- the whole
// background is computed, never stored.
__device__ __forceinline__ uint32_t _bg_mix(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// The unit sample the background is built from. `blocky` draws one of the 8 RGB
// cube corners per tile, the extreme points of the cube, so residual
// transparency costs the most it can; the plain path keeps its U[0,1).
template<bool blocky>
__device__ __forceinline__ float3 _bg_sample(uint32_t seed, unsigned gid,
                                             unsigned bid, unsigned x, unsigned y) {
    float3 u;
    if constexpr (blocky) {
        // The whole tile grid shifts each step, so no pixel keeps its colour
        // and the pattern cannot be baked into the splats.
        const unsigned ox = _bg_mix(seed * 2u + 1u) % kBgBlockPx;
        const unsigned oy = _bg_mix(seed * 2u + 7u) % kBgBlockPx;
        const uint32_t h  = _bg_mix(((x + ox) / kBgBlockPx) * 2654435761u
                                    ^ ((y + oy) / kBgBlockPx) * 40503u
                                    ^ (seed + bid * 0x9e3779b9u));
        u.x = (h & 1u) ? 1.0f : -1.0f;
        u.y = (h & 2u) ? 1.0f : -1.0f;
        u.z = (h & 4u) ? 1.0f : -1.0f;
    } else {
        u.x = (float)hash_uint3(seed + 0, gid, bid) * exp2f(-32.0f);
        u.y = (float)hash_uint3(seed + 1, gid, bid) * exp2f(-32.0f);
        u.z = (float)hash_uint3(seed + 2, gid, bid) * exp2f(-32.0f);
    }
    return u;
}

template<bool is_linear, bool blocky>
__device__ __forceinline__ float3 _bg_color(uint32_t seed, unsigned gid,
                                            unsigned bid, unsigned x, unsigned y,
                                            float randomize_weight) {
    float3 background = _bg_sample<blocky>(seed, gid, bid, x, y);
    background = 0.5 + 0.5*randomize_weight * background;
    if (is_linear) {
        background.x = SlangPixelWise::srgb_to_linear_rgb(background.x);
        background.y = SlangPixelWise::srgb_to_linear_rgb(background.y);
        background.z = SlangPixelWise::srgb_to_linear_rgb(background.z);
    }
    return background;
}

template<bool is_linear, bool blocky>
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

    float3 background =
        _bg_color<is_linear, blocky>(seed, gid, bid, x, y, randomize_weight);

    rgb = SlangPixelWise::blend_background(rgb, transmittance, background);

    out_rgb.store3(bid, y, x, rgb);
}

template<bool is_linear, bool blocky>
__global__ void blend_background_noise_backward_kernel(
    const TensorView<float, 4> in_rgb,
    const TensorView<float, 4> in_transmittance,
    const float randomize_weight,
    const uint32_t seed,
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

    float3 background =
        _bg_color<is_linear, blocky>(seed, gid, bid, x, y, randomize_weight);

    float3 v_out = v_out_rgb.load3(bid, y, x);

    float3 v_rgb; float v_transmittance; float3 v_background;
    SlangPixelWise::blend_background_bwd(
        rgb, transmittance, background,
        v_out,
        &v_rgb, &v_transmittance, &v_background
    );

    v_in_rgb.store3(bid, y, x, v_rgb);
    v_in_transmittance.store1(bid, y, x, v_transmittance);
}

/*[AutoHeaderGeneratorExport]*/
void blend_background_noise_forward(
    bool is_linear,
    bool blocky,                          // tiled RGB corners instead of U[0,1)
    DeviceTensor3D<float3> rgb,           // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance, // [B, H, W, 1]
    float randomize_weight,
    uint32_t seed,
    DeviceTensor3D<float3> out_rgb        // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    auto k = is_linear
        ? (blocky ? blend_background_noise_forward_kernel<true, true>
                  : blend_background_noise_forward_kernel<true, false>)
        : (blocky ? blend_background_noise_forward_kernel<false, true>
                  : blend_background_noise_forward_kernel<false, false>);
    k<<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance),
        randomize_weight, seed,
        _dt3d_to_tv4<float>(out_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void blend_background_noise_backward(
    bool is_linear,
    bool blocky,                          // tiled RGB corners instead of U[0,1)
    DeviceTensor3D<float3> rgb,              // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    float randomize_weight,
    uint32_t seed,
    DeviceTensor3D<float3> v_out_rgb,        // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb,            // [B, H, W, 3]
    DeviceTensor3D<float>  v_transmittance   // [B, H, W, 1]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    auto k = is_linear
        ? (blocky ? blend_background_noise_backward_kernel<true, true>
                  : blend_background_noise_backward_kernel<true, false>)
        : (blocky ? blend_background_noise_backward_kernel<false, true>
                  : blend_background_noise_backward_kernel<false, false>);
    k<<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb), _dt3d_to_tv4<float>(transmittance),
        randomize_weight, seed,
        _dt3d_to_tv4<float>(v_out_rgb),
        _dt3d_to_tv4<float>(v_rgb), _dt3d_to_tv4<float>(v_transmittance)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Log Map Image
// ================

template<bool is_input_linear>
__global__ void rgb_to_srgb_forward_kernel(
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

    if (is_input_linear)
        rgb = SlangPixelWise::linear_rgb_to_srgb(rgb, color_matrix);
    else
        rgb = SlangPixelWise::rgb_to_srgb(rgb, color_matrix);

    out_rgb.store3(bid, y, x, rgb);
}

template<bool is_input_linear>
__global__ void rgb_to_srgb_backward_kernel(
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

    float3 v_rgb;
    if (is_input_linear)
        v_rgb = SlangPixelWise::linear_rgb_to_srgb_bwd(rgb, color_matrix, v_out);
    else
        v_rgb = SlangPixelWise::rgb_to_srgb_bwd(rgb, color_matrix, v_out);

    v_in_rgb.store3(bid, y, x, v_rgb);
}

/*[AutoHeaderGeneratorExport]*/
void rgb_to_srgb_forward(
    bool is_input_linear,
    DeviceTensor3D<float3> rgb,          // [B, H, W, 3]
    DeviceTensor2D<float3> color_matrix, // [3, 3] stored as 3 float3
    DeviceTensor3D<float3> out_rgb       // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    (is_input_linear ? rgb_to_srgb_forward_kernel<true> : rgb_to_srgb_forward_kernel<false>)
    <<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb),
        (float*)color_matrix.data_ptr(),
        _dt3d_to_tv4<float>(out_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void rgb_to_srgb_backward(
    bool is_input_linear,
    DeviceTensor3D<float3> rgb,          // [B, H, W, 3]
    DeviceTensor2D<float3> color_matrix, // [3, 3] stored as 3 float3
    DeviceTensor3D<float3> v_out_rgb,    // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb         // [B, H, W, 3]
) {
    long b = rgb.size<0>(), h = rgb.size<1>(), w = rgb.size<2>();

    (is_input_linear ? rgb_to_srgb_backward_kernel<true> : rgb_to_srgb_backward_kernel<false>)
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

// Penalizes rgb values outside [0,1] with an L2 loss
//   L = weight * mean_{b,y,x,c}( max(-x, x-1, 0)^2 )
// The actual scalar loss is never materialized; this kernel only adds the
// per-pixel gradient dL/dx into v_rgb in-place. With N = B*H*W*3 the
// per-channel grad is
//   x < 0  : 2 * weight * x / N
//   x > 1  : 2 * weight * (x - 1) / N
//   else   : 0
__global__ void overexposure_grad_add_kernel(
    const TensorView<float, 4> rgb,
    const float scale,                // 2 * weight / (B * H * W * 3)
    TensorView<float, 4> v_rgb
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned bid = blockIdx.y * blockDim.y + threadIdx.y;
    unsigned B = rgb.shape[0], H = rgb.shape[1], W = rgb.shape[2];
    if (bid >= B || gid >= H * W) return;
    unsigned y = gid / W;
    unsigned x = gid % W;

    float3 c   = rgb.load3(bid, y, x);
    float3 v   = v_rgb.load3(bid, y, x);

    auto over = [scale](float v_in, float v_pix) -> float {
        float g = (v_pix < 0.0f) ? v_pix
                : (v_pix > 1.0f) ? (v_pix - 1.0f)
                : 0.0f;
        return v_in + scale * g;
    };
    v.x = over(v.x, c.x);
    v.y = over(v.y, c.y);
    v.z = over(v.z, c.z);

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
    double N = (double)b * (double)h * (double)w * 3.0;
    float scale = (float)(2.0 * (double)weight / N);

    overexposure_grad_add_kernel<<<_LAUNCH_ARGS_2D(h * w, b, 256, 1)>>>(
        _dt3d_to_tv4<float>(rgb),
        scale,
        _dt3d_to_tv4<float>(v_rgb)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


