// DensifySplitFilter.cu -- long-axis split (https://arxiv.org/abs/2508.12313) and the image edge filters used to score densification.
//
// Part of the Densify family -- see DensifyCommon.cuh.

#include "kernels/densify/DensifyQuantCopy.cuh"
#include "kernels/densify/DensifyInternal.cuh"

// ================
// Long axis split (https://arxiv.org/abs/2508.12313)
// ================

__global__ void long_axis_split_3dgs_kernel(
    long num_splats,
    float split_opacity_k,
    const float3* __restrict__ log_scales,
    const float* __restrict__ logit_opacities,
    const float4* __restrict__ quats,
    float3* __restrict__ new_log_scales,
    float* __restrict__ new_logit_opacities,
    float3* __restrict__ mean_deltas
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_splats)
        return;

    SlangDensify::long_axis_split_3dgs(
        split_opacity_k,
        log_scales[idx], logit_opacities[idx], quats[idx],
        &new_log_scales[idx], &new_logit_opacities[idx], &mean_deltas[idx]
    );
}

/*[AutoHeaderGeneratorExport]*/
void long_axis_split_tensor(
    std::string primitive,
    float split_opacity_k,
    DeviceVector<float3> log_scales,
    DeviceVector<float> logit_opacities,
    DeviceVector<float4> quats,
    DeviceVector<float3> new_log_scales,
    DeviceVector<float> new_logit_opacities,
    DeviceVector<float3> mean_deltas
) {
    const size_t num_splats = quats.size();

    if (num_splats == 0)
        return;

    if (primitive == "3dgs" || primitive == "mip" || primitive == "3dgut")
        long_axis_split_3dgs_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
            num_splats,
            split_opacity_k,
            log_scales.data_ptr(),
            logit_opacities.data_ptr(),
            quats.data_ptr(),
            new_log_scales.data_ptr(),
            new_logit_opacities.data_ptr(),
            mean_deltas.data_ptr()
        );
    else throw std::runtime_error("Unsupported primitive: " + primitive);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



// ================
// Image filter (https://arxiv.org/abs/2508.12313, https://arxiv.org/abs/2603.08661)
// ================

__constant__ float blur_filter_5x5[5][5] = {
    { 2.f/159.f,  4.f/159.f,  5.f/159.f,  4.f/159.f, 2.f/159.f },
    { 4.f/159.f,  9.f/159.f, 12.f/159.f,  9.f/159.f, 4.f/159.f },
    { 5.f/159.f, 12.f/159.f, 15.f/159.f, 12.f/159.f, 5.f/159.f },
    { 4.f/159.f,  9.f/159.f, 12.f/159.f,  9.f/159.f, 4.f/159.f },
    { 2.f/159.f,  4.f/159.f,  5.f/159.f,  4.f/159.f, 2.f/159.f },
};

__constant__ float laplacian_filter_3x3[3][3] = {
    { 1.f/6.f,  4.f/6.f, 1.f/6.f },
    { 4.f/6.f,-20.f/6.f, 4.f/6.f },
    { 1.f/6.f,  4.f/6.f, 1.f/6.f },
};

__constant__ float canny_filter_3x3[3][3] = {
    { -1.0f, 0.0f, 1.0f },
    { -2.0f, 0.0f, 2.0f },
    { -1.0f, 0.0f, 1.0f },
};

__global__ void laplacian_edge_filter_kernel(
    const TensorView<float, 4> img_in,
    TensorView<float, 4> img_out
) {
    constexpr int BLOCK = 32;
    constexpr int HALO = 1;

    const int32_t xid = blockIdx.x * BLOCK + threadIdx.x;
    const int32_t yid = blockIdx.y * BLOCK + threadIdx.y;
    const int32_t bid = blockIdx.z;
    const uint32_t H = img_in.shape[1], W = img_in.shape[2];

    // load pixels
    __shared__ float shared_pixels[BLOCK+2*HALO][BLOCK+2*HALO];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO)*(BLOCK+2*HALO); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO);
        int x = tid % (BLOCK+2*HALO);
        if (y < BLOCK+2*HALO) {
            int yi = min(max((int)(blockIdx.y * BLOCK) + y - HALO, 0), H-1);
            int xi = min(max((int)(blockIdx.x * BLOCK) + x - HALO, 0), W-1);
            shared_pixels[y][x] = dot(img_in.load3(bid, yi, xi), float3{0.299f, 0.587f, 0.114f});
        }
    }
    __syncthreads();

    // 3x3 Laplacian
    float total = 0.0f;
    for (int cy = -HALO; cy <= HALO; ++cy)
        for (int cx = -HALO; cx <= HALO; ++cx) {
            float conv_weight = laplacian_filter_3x3[cy+HALO][cx+HALO];
            int yi = threadIdx.y + cy;
            int xi = threadIdx.x + cx;
            total += conv_weight * shared_pixels[yi+HALO][xi+HALO];
        }
    total = fabsf(total);
    if (yid < H && xid < W)
        img_out.store1(bid, yid, xid, total);
}

/*[AutoHeaderGeneratorExport]*/
void laplacian_edge_filter_tensor(
    DeviceTensor3D<float3> img_in,
    DeviceTensor3D<float> img_out
) {
    int B  = img_in.template size<0>();
    int H  = img_in.template size<1>();
    int W  = img_in.template size<2>();

    laplacian_edge_filter_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(
        _dt3d_to_tv4<float>(img_in),
        _dt3d_float_to_tv4_1ch(img_out)
    );
}

__global__ void smoothed_laplacian_edge_filter_kernel(
    const TensorView<float, 4> img_in,
    TensorView<float, 4> img_out
) {
    constexpr int BLOCK = 32;
    constexpr int HALO = 3;
    constexpr int HALO1 = 1;

    const int32_t xid = blockIdx.x * BLOCK + threadIdx.x;
    const int32_t yid = blockIdx.y * BLOCK + threadIdx.y;
    const int32_t bid = blockIdx.z;
    const uint32_t H = img_in.shape[1], W = img_in.shape[2];

    // load pixels
    __shared__ float shared_pixels[BLOCK+2*HALO][BLOCK+2*HALO];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO)*(BLOCK+2*HALO); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO);
        int x = tid % (BLOCK+2*HALO);
        if (y < BLOCK+2*HALO) {
            int yi = min(max((int)(blockIdx.y * BLOCK) + y - HALO, 0), H-1);
            int xi = min(max((int)(blockIdx.x * BLOCK) + x - HALO, 0), W-1);
            shared_pixels[y][x] = dot(img_in.load3(bid, yi, xi), float3{0.299f, 0.587f, 0.114f});
        }
    }
    __syncthreads();

    // TODO: optimize into 7x7 horizontal + vertical convolution?

    // 5x5 blur
    __shared__ float shared_blurred[BLOCK+2*HALO1][BLOCK+2*HALO1];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO1)*(BLOCK+2*HALO1); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO1);
        int x = tid % (BLOCK+2*HALO1);
        if (y >= BLOCK+2*HALO1)
            continue;
        float total = 0.0f;
        for (int cy = -2; cy <= 2; ++cy)
            for (int cx = -2; cx <= 2; ++cx) {
                float conv_weight = blur_filter_5x5[cy+2][cx+2];
                int yi = y - HALO1 + cy;
                int xi = x - HALO1 + cx;
                total += conv_weight * shared_pixels[yi+HALO][xi+HALO];
            }
        shared_blurred[y][x] = total;
    }
    __syncthreads();

    // 3x3 Laplacian
    float total = 0.0f;
    for (int cy = -HALO1; cy <= HALO1; ++cy)
        for (int cx = -HALO1; cx <= HALO1; ++cx) {
            float conv_weight = laplacian_filter_3x3[cy+HALO1][cx+HALO1];
            int yi = threadIdx.y + cy;
            int xi = threadIdx.x + cx;
            total += conv_weight * shared_blurred[yi+HALO1][xi+HALO1];
        }
    total = fabsf(total);
    if (yid < H && xid < W)
        img_out.store1(bid, yid, xid, total);
}

/*[AutoHeaderGeneratorExport]*/
void smoothed_laplacian_edge_filter_tensor(
    DeviceTensor3D<float3> img_in,
    DeviceTensor3D<float> img_out
) {
    int B  = img_in.template size<0>();
    int H  = img_in.template size<1>();
    int W  = img_in.template size<2>();

    smoothed_laplacian_edge_filter_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(
        _dt3d_to_tv4<float>(img_in),
        _dt3d_float_to_tv4_1ch(img_out)
    );
}

__global__ void canny_edge_filter_kernel(
    TensorView<float, 4> img_in,
    const uint8_t* __restrict__ mask_in,
    TensorView<float, 4> img_out
) {
    constexpr int BLOCK = 32;
    constexpr int HALO = 4;
    constexpr int HALO1 = 2;
    constexpr int HALO2 = 1;

    const int32_t xid = blockIdx.x * BLOCK + threadIdx.x;
    const int32_t yid = blockIdx.y * BLOCK + threadIdx.y;
    const int32_t bid = blockIdx.z;
    const uint32_t H = img_in.shape[1], W = img_in.shape[2];

    // load pixels
    __shared__ float shared_pixels[BLOCK+2*HALO][BLOCK+2*HALO];
    __shared__ float shared_valid[BLOCK+2*HALO][BLOCK+2*HALO];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO)*(BLOCK+2*HALO); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO);
        int x = tid % (BLOCK+2*HALO);
        if (y < BLOCK+2*HALO) {
            int yi = min(max((int)(blockIdx.y * BLOCK) + y - HALO, 0), H-1);
            int xi = min(max((int)(blockIdx.x * BLOCK) + x - HALO, 0), W-1);
            shared_pixels[y][x] = dot(img_in.load3(bid, yi, xi), float3{0.299f, 0.587f, 0.114f});
            shared_valid[y][x] = (mask_in == nullptr) ? 1.0f :
                (float)(mask_in[(&img_in.at(bid, yi, xi, 0) - img_in.data)/3] == 1);
        }
    }
    __syncthreads();

    // 5x5 blur over the unmasked samples only. Gating canny's output at the
    // end instead lets the mask silhouette -- a step from distractor to scene
    // -- register as the strongest edge in the image.
    __shared__ float shared_blurred[BLOCK+2*HALO1][BLOCK+2*HALO1];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO1)*(BLOCK+2*HALO1); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO1);
        int x = tid % (BLOCK+2*HALO1);
        if (y >= BLOCK+2*HALO1)
            continue;
        float total = 0.0f, weight = 0.0f;
        for (int cy = -2; cy <= 2; ++cy)
            for (int cx = -2; cx <= 2; ++cx) {
                float conv_weight = blur_filter_5x5[cy+2][cx+2];
                int yi = y - HALO1 + cy;
                int xi = x - HALO1 + cx;
                float w = conv_weight * shared_valid[yi+HALO][xi+HALO];
                total += w * shared_pixels[yi+HALO][xi+HALO];
                weight += w;
            }
        // Zero weight is >2px inside the mask, out of reach of any unmasked
        // output pixel: canny reads blurred values at most 2px away.
        shared_blurred[y][x] = weight > 0.0f ? total / weight : 0.0f;
    }
    __syncthreads();

    // 3x3 canny
    __shared__ float2 shared_filtered[BLOCK+2*HALO2][BLOCK+2*HALO2];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO2)*(BLOCK+2*HALO2); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO2);
        int x = tid % (BLOCK+2*HALO2);
        if (y >= BLOCK+2*HALO2)
            continue;
        float total1 = 0.0f, total2 = 0.0f;
        for (int cy = -1; cy <= 1; ++cy)
            for (int cx = -1; cx <= 1; ++cx) {
                float conv_weight_1 = canny_filter_3x3[cy+1][cx+1];
                float conv_weight_2 = canny_filter_3x3[cx+1][cy+1];
                int yi = y - HALO2 + cy;
                int xi = x - HALO2 + cx;
                float value = shared_blurred[yi+HALO1][xi+HALO1];
                total1 += conv_weight_1 * value;
                total2 += conv_weight_2 * value;
            }
        shared_filtered[y][x] = {total1, total2};
    }
    __syncthreads();

    // non-maximum suppression
    float2 total = shared_filtered[threadIdx.y+HALO2][threadIdx.x+HALO2];
    float mag = length(total);
    if (mag > 0.0f) {
        int dx = min(max((int)roundf(total.x / mag), -HALO2), HALO2);
        int dy = min(max((int)roundf(total.y / mag), -HALO2), HALO2);
        float total1 = length(shared_filtered[(int)threadIdx.y+dy+HALO2][(int)threadIdx.x+dx+HALO2]);
        float total2 = length(shared_filtered[(int)threadIdx.y-dy+HALO2][(int)threadIdx.x-dx+HALO2]);
        if (mag < total1 || mag < total2)
            mag = 0.0f;
    }
    if (yid < H && xid < W) {
        if (mask_in && mask_in[(&img_in.at(bid, yid, xid, 0) - img_in.data)/3] != 1)
            mag = 0.0f;
        img_out.store1(bid, yid, xid, mag);
    }
}

/*[AutoHeaderGeneratorExport]*/
void canny_edge_filter_tensor(
    DeviceTensor3D<float3> img_in,
    uint8_t* mask_in_ptr,
    DeviceTensor3D<float> img_out
) {
    int B  = img_in.template size<0>();
    int H  = img_in.template size<1>();
    int W  = img_in.template size<2>();

    canny_edge_filter_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(
        _dt3d_to_tv4<float>(img_in),
        mask_in_ptr,
        _dt3d_float_to_tv4_1ch(img_out)
    );
}


// ============================================================================
// Robust residual canny: RobustNeRF-style Tukey biweight on |render - GT| in
// BT.601 luma, followed by canny edge detection. Densification guidance that
// is simultaneously (a) zero where the render already matches GT, (b) tolerant
// to global luminance drift (DC residuals contribute no spatial gradient to
// canny), (c) suppressing distractor pixels (residuals above the per-image
// q-quantile get downweighted to zero).
//
// VRAM footprint per call: one [B,H,W] float scratch (pool-keyed
// "densify_robust_resid") + one [B] float Tukey threshold (pool-keyed
// "densify_tukey_c"). Both stable across calls/scales -- the pool's
// high-water-mark sizing handles the largest scale's allocation and reuses it
// for the smaller scales.
// ============================================================================

// Stage 1: per-pixel |render - GT| projected onto BT.601 luma. Luma matches
// canny's internal RGB->luma weighting, so the downstream edge detector sees
// the same scalar field a color-blind canny would have computed on the
// post-difference image.
__global__ void _robust_residual_luma_kernel(
    int B, int H, int W,
    const float3* __restrict__ render,
    const float3* __restrict__ ref,
    const uint8_t* __restrict__ mask_in,
    float* __restrict__ out  // [B, H, W]
) {
    int xid = blockIdx.x * blockDim.x + threadIdx.x;
    int yid = blockIdx.y * blockDim.y + threadIdx.y;
    int bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (xid >= W || yid >= H || bid >= B) return;
    size_t idx = ((size_t)bid * H + yid) * W + xid;
    // Infinity, not zero: the quantile below counts only strictly positive
    // finite values, so a masked residual is dropped instead of dragging the
    // Tukey cutoff around. The Tukey pass turns it into a real zero.
    if (mask_in != nullptr && mask_in[idx] != 1) {
        out[idx] = INFINITY;
        return;
    }
    float3 d;
    d.x = render[idx].x - ref[idx].x;
    d.y = render[idx].y - ref[idx].y;
    d.z = render[idx].z - ref[idx].z;
    out[idx] = fabsf(0.299f * d.x + 0.587f * d.y + 0.114f * d.z);
}

// Stage 3: in-place Tukey biweight. Input `data[i]` holds |r_i| (luma resid);
// overwritten with |r_i| * w(r_i) where w(r) = (1 - (r/c)^2)^2 for |r| <= c
// else 0. The product r * w (the psi-style influence magnitude) is what the
// densification map should peak at: small for well-reconstructed regions
// (|r| ~ 0), large in the middle range, and back to zero past the cutoff
// (likely-distractor pixels). c is the per-image q-quantile of |r|.
__global__ void _robust_tukey_inplace_kernel(
    int B, int N,
    float* __restrict__ data,        // [B, N]
    const float* __restrict__ c_buf  // [B]
) {
    size_t idx = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = (size_t)B * (size_t)N;
    if (idx >= total) return;
    int b = (int)(idx / (size_t)N);
    float c = c_buf[b];
    float r = data[idx];
    if (!(c > 0.0f) || !(r < c)) {  // !(r < c) also catches the masked infinity
        data[idx] = 0.0f;
        return;
    }
    float u = r / c;
    float w = 1.0f - u * u;
    data[idx] = r * w * w;
}

// Scalar-input canny: identical body to canny_edge_filter_kernel, only the
// input load differs (load1 vs RGB->luma dot product). Kept as a separate
// kernel rather than templatized because the body uses shared memory and
// the indirection through a __device__ functor would inhibit some loop /
// unroll heuristics.
__global__ void canny_edge_filter_kernel_scalar(
    TensorView<float, 4> img_in,    // [B, H, W, 1]
    const uint8_t* __restrict__ mask_in,
    TensorView<float, 4> img_out    // [B, H, W, 1]
) {
    constexpr int BLOCK = 32;
    constexpr int HALO = 4;
    constexpr int HALO1 = 2;
    constexpr int HALO2 = 1;

    const int32_t xid = blockIdx.x * BLOCK + threadIdx.x;
    const int32_t yid = blockIdx.y * BLOCK + threadIdx.y;
    const int32_t bid = blockIdx.z;
    const uint32_t H = img_in.shape[1], W = img_in.shape[2];

    __shared__ float shared_pixels[BLOCK+2*HALO][BLOCK+2*HALO];
    __shared__ float shared_valid[BLOCK+2*HALO][BLOCK+2*HALO];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO)*(BLOCK+2*HALO); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO);
        int x = tid % (BLOCK+2*HALO);
        if (y < BLOCK+2*HALO) {
            int yi = min(max((int)(blockIdx.y * BLOCK) + y - HALO, 0), H-1);
            int xi = min(max((int)(blockIdx.x * BLOCK) + x - HALO, 0), W-1);
            shared_pixels[y][x] = img_in.load1(bid, yi, xi);
            shared_valid[y][x] = (mask_in == nullptr) ? 1.0f :
                (float)(mask_in[&img_in.at(bid, yi, xi, 0) - img_in.data] == 1);
        }
    }
    __syncthreads();

    __shared__ float shared_blurred[BLOCK+2*HALO1][BLOCK+2*HALO1];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO1)*(BLOCK+2*HALO1); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO1);
        int x = tid % (BLOCK+2*HALO1);
        if (y >= BLOCK+2*HALO1)
            continue;
        float total = 0.0f, weight = 0.0f;
        for (int cy = -2; cy <= 2; ++cy)
            for (int cx = -2; cx <= 2; ++cx) {
                float conv_weight = blur_filter_5x5[cy+2][cx+2];
                int yi = y - HALO1 + cy;
                int xi = x - HALO1 + cx;
                float w = conv_weight * shared_valid[yi+HALO][xi+HALO];
                total += w * shared_pixels[yi+HALO][xi+HALO];
                weight += w;
            }
        // Zero weight is >2px inside the mask, out of reach of any unmasked
        // output pixel: canny reads blurred values at most 2px away.
        shared_blurred[y][x] = weight > 0.0f ? total / weight : 0.0f;
    }
    __syncthreads();

    __shared__ float2 shared_filtered[BLOCK+2*HALO2][BLOCK+2*HALO2];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO2)*(BLOCK+2*HALO2); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK+2*HALO2);
        int x = tid % (BLOCK+2*HALO2);
        if (y >= BLOCK+2*HALO2)
            continue;
        float total1 = 0.0f, total2 = 0.0f;
        for (int cy = -1; cy <= 1; ++cy)
            for (int cx = -1; cx <= 1; ++cx) {
                float conv_weight_1 = canny_filter_3x3[cy+1][cx+1];
                float conv_weight_2 = canny_filter_3x3[cx+1][cy+1];
                int yi = y - HALO2 + cy;
                int xi = x - HALO2 + cx;
                float value = shared_blurred[yi+HALO1][xi+HALO1];
                total1 += conv_weight_1 * value;
                total2 += conv_weight_2 * value;
            }
        shared_filtered[y][x] = {total1, total2};
    }
    __syncthreads();

    float2 total = shared_filtered[threadIdx.y+HALO2][threadIdx.x+HALO2];
    float mag = length(total);
    if (mag > 0.0f) {
        int dx = min(max((int)roundf(total.x / mag), -HALO2), HALO2);
        int dy = min(max((int)roundf(total.y / mag), -HALO2), HALO2);
        float total1 = length(shared_filtered[(int)threadIdx.y+dy+HALO2][(int)threadIdx.x+dx+HALO2]);
        float total2 = length(shared_filtered[(int)threadIdx.y-dy+HALO2][(int)threadIdx.x-dx+HALO2]);
        if (mag < total1 || mag < total2)
            mag = 0.0f;
    }
    if (yid < H && xid < W) {
        if (mask_in && mask_in[(&img_in.at(bid, yid, xid, 0) - img_in.data)] != 1)
            mag = 0.0f;
        img_out.store1(bid, yid, xid, mag);
    }
}

/*[AutoHeaderGeneratorExport]*/
void robust_canny_residual_tensor(
    DeviceTensor3D<float3> render,   // [B, H, W, 3]
    DeviceTensor3D<float3> ref,      // [B, H, W, 3]
    uint8_t* mask_in_ptr,               // optional [B*H*W] mask; nullptr for none
    float quantile,                  // Tukey cutoff = per-image q-quantile of |r|
    DeviceTensor3D<float> img_out    // [B, H, W, 1] -- written (not added)
) {
    int B = render.template size<0>();
    int H = render.template size<1>();
    int W = render.template size<2>();
    if (B <= 0 || H <= 0 || W <= 0) return;
    size_t N = (size_t)H * (size_t)W;

    // Pool scratch buffers. Same keys across scales/calls -- pool's high-water
    // mark holds the largest, smaller scales reuse the allocation.
    float* resid = DevicePool::global().acquire<float>(
        PoolSlot::DensifyRobustResid, (size_t)B * N);
    float* c_buf = DevicePool::global().acquire<float>(
        PoolSlot::DensifyTukeyC, (size_t)B);

    // 1) per-pixel BT.601-luma |render - ref|.
    _robust_residual_luma_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(
        B, H, W,
        (const float3*)render.data_ptr(),
        (const float3*)ref.data_ptr(),
        mask_in_ptr,
        resid
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // 2) per-image q-quantile of |r| -> Tukey cutoff c.
    quantile_of_positive_finite_elements_internal(
        resid, B, (int)N, quantile, /*return_reciprocal=*/false,
        /*abs_input=*/true, c_buf);

    // 3) in-place Tukey biweight: resid <- |r| * (1 - (r/c)^2)^2, zero past c.
    _robust_tukey_inplace_kernel<<<_LAUNCH_ARGS_1D((size_t)B * N, 256)>>>(
        B, (int)N, resid, c_buf
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // 4) canny on the robustified scalar -> img_out (overwrites).
    TensorView<float, 4> resid_tv;
    resid_tv.data = resid;
    resid_tv.shape[0] = B;
    resid_tv.shape[1] = H;
    resid_tv.shape[2] = W;
    resid_tv.shape[3] = 1;
    resid_tv.strides[0] = (long)N;
    resid_tv.strides[1] = W;
    resid_tv.strides[2] = 1;
    resid_tv.strides[3] = 1;
    canny_edge_filter_kernel_scalar<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(
        resid_tv,
        mask_in_ptr,
        _dt3d_float_to_tv4_1ch(img_out)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
