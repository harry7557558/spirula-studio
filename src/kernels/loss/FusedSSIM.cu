// Based on https://github.com/MrNeRF/optimized-fused-ssim/ by Rahul Goel and MrNeRF
// MIT License: https://github.com/MrNeRF/optimized-fused-ssim/blob/main/LICENSE

// Changes:
//  - Output scalar loss only, instead of full SSIM map and reduce on Python side
//  - Hard-core float3 with channel-last memory layout
//  - Hard-core padding=same

#include "kernels/loss/FusedSSIM.cuh"
#include "kernels/loss/PerPixelLoss.cuh"  // SsimLossMapMode
#include "core/PixelFormat.h"


#include <cooperative_groups.h>
#include <algorithm>
#include <type_traits>
#include <iostream>

namespace cg = cooperative_groups;

__forceinline__ __device__ float3 fmaxf(float3 v, float k) {
    return {
        fmaxf(v.x, k),
        fmaxf(v.y, k),
        fmaxf(v.z, k)
    };
}
__forceinline__ __device__ float3 sqrtf(float3 v) {
    return {
        sqrtf(fmaxf(v.x, 0.0f)),
        sqrtf(fmaxf(v.y, 0.0f)),
        sqrtf(fmaxf(v.z, 0.0f))
    };
}
__forceinline__ __device__ float3 fabsf(float3 v) {
    return {
        fabsf(v.x), fabsf(v.y), fabsf(v.z)
    };
}
__forceinline__ __device__ float3 fsignf(float3 v) {
    return {
        copysignf(1.0f, v.x),
        copysignf(1.0f, v.y),
        copysignf(1.0f, v.z)
    };
}

// ------------------------------------------
// Block and Shared Memory Dimensions
// ------------------------------------------
#define BLOCK_X 16
#define BLOCK_Y 16
#define HALO    5

#define SHARED_X (BLOCK_X + 2 * HALO)
#define SHARED_Y (BLOCK_Y + 2 * HALO)

// For partial results after horizontal pass
#define CONV_X BLOCK_X
#define CONV_Y SHARED_Y

// ------------------------------------------
// Constant Memory for Gaussian Coefficients
// ------------------------------------------
__constant__ float cGauss[HALO+1] = {
    0.001028380123898387f,
    0.0075987582094967365f,
    0.036000773310661316f,
    0.10936068743467331f,
    0.21300552785396576f,
    0.26601171493530273f,
    // 0.21300552785396576f,
    // 0.10936068743467331f,
    // 0.036000773310661316f,
    // 0.0075987582094967365f,
    // 0.001028380123898387f
};

// ------------------------------------------
// Utility: Safe pixel fetch w/ zero padding
// ------------------------------------------
__device__ __forceinline__ float3 get_pix_value(
    const float3* img,
    int b, int y, int x,
    int H, int W
) {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) {
        return make_float3(0);
    }
    return img[b * H * W + y * W + x];
}

__device__ __forceinline__ float get_pix_value(
    const float3* img,
    int b, int y, int x, int c,
    int H, int W
) {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) {
        return 0.0f;
    }
    return ((float*)img)[(b * H * W + y * W + x) * 3 + c];
}

// A mask is stored at the size of the file it came from, which is not the
// render's whenever the run is downscaled, so it is nearest-sampled exactly as
// core/Interpolation.cuh's nearest_sample_b. Out-of-image taps read unmasked.
__device__ __forceinline__ bool get_pix_value(
    const bool* img,
    int b, int y, int x,
    int B_mask, int H_mask, int W_mask,
    int H, int W
) {
    if (img == nullptr)
        return true;  // not masked
    if ((unsigned)b >= (unsigned)B_mask ||
        (unsigned)x >= (unsigned)W ||
        (unsigned)y >= (unsigned)H) {
        return true;  // to not mess up metrics
    }
    int xs = x, ys = y;
    if (W_mask != W || H_mask != H) {
        float u = ((float)x + 0.5f) * (float)W_mask / (float)W - 0.5f;
        float v = ((float)y + 0.5f) * (float)H_mask / (float)H - 0.5f;
        xs = max(0, min(W_mask - 1, (int)floorf(u + 0.5f)));
        ys = max(0, min(H_mask - 1, (int)floorf(v + 0.5f)));
    }
    return img[b * H_mask * W_mask + ys * W_mask + xs];
}

// An image the memory-efficient kernel reads, in the form it is stored in.
struct SsimImg {
    const void* p;
    PixelFormat f;
};

__device__ __forceinline__ float get_pix_value(
    SsimImg img,
    int b, int y, int x, int c,
    int H, int W
) {
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) {
        return 0.0f;
    }
    return pixel_load1(img.p, img.f, ((size_t)(b * H + y) * W + x) * 3 + c);
}

// The mask every stage below reads: the image mask, plus (when `sat` is
// positive) a pixel both images clip above, which carries no recoverable
// error. Out-of-image taps read unmasked, as the mask fetch does.
__device__ __forceinline__ bool get_pix_mask(
    SsimImg img1, SsimImg img2, float sat,
    const bool* masks,
    int b, int y, int x,
    int B_mask, int H_mask, int W_mask,
    int H, int W
) {
    if (!get_pix_value(masks, b, y, x, B_mask, H_mask, W_mask, H, W))
        return false;
    if (sat <= 0.0f)
        return true;
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H)
        return true;
    float3 a = pixel_load3(img1.p, img1.f, (size_t)(b * H + y) * W + x);
    float3 c = pixel_load3(img2.p, img2.f, (size_t)(b * H + y) * W + x);
    return fminf(fminf(a.x, a.y), a.z) <= sat ||
           fminf(fminf(c.x, c.y), c.z) <= sat;
}

// ------------------------------------------
// Window coverage of the mask
// ------------------------------------------

// Gaussian weight each 11x11 SSIM window puts on an UNMASKED pixel. Dividing
// the window sums by it makes every moment a mean over the observed pixels;
// taps read out-of-range as unmasked, so an unmasked image gets coverage 1.

__constant__ float cGaussSum = 0.9999999f;

// One pass: the mask tile and its halo go through shared memory, so the
// horizontal sums never reach global memory.
#define COV_T 16
#define COV_S (COV_T + 2 * HALO)

__global__ void _ssim_mask_coverage_kernel(
    int B, int H, int W,
    const SsimImg img1,
    const SsimImg img2,
    float sat,
    const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask,
    float* __restrict__ out  // [B, H, W]
) {
    __shared__ float sm[COV_S][COV_S];
    __shared__ float sx[COV_S][COV_T];
    const int b = blockIdx.z;
    const int x0 = blockIdx.x * COV_T - HALO;
    const int y0 = blockIdx.y * COV_T - HALO;
    const int tid = threadIdx.y * COV_T + threadIdx.x;
    for (int i = tid; i < COV_S * COV_S; i += COV_T * COV_T) {
        int ly = i / COV_S, lx = i % COV_S;
        sm[ly][lx] = (float)get_pix_mask(img1, img2, sat, masks, b, y0 + ly,
                                         x0 + lx, B_mask, H_mask, W_mask, H, W);
    }
    __syncthreads();
    for (int i = tid; i < COV_S * COV_T; i += COV_T * COV_T) {
        int ly = i / COV_T, lx = i % COV_T;
        float acc = 0.0f;
        #pragma unroll
        for (int d = -HALO; d <= HALO; ++d)
            acc += cGauss[HALO - abs(d)] * sm[ly][lx + HALO + d];
        sx[ly][lx] = acc;
    }
    __syncthreads();
    const int x = blockIdx.x * COV_T + threadIdx.x;
    const int y = blockIdx.y * COV_T + threadIdx.y;
    if (x >= W || y >= H) return;
    float acc = 0.0f;
    #pragma unroll
    for (int d = -HALO; d <= HALO; ++d) {
        int yy = y + d;
        // A row off the top or bottom of the image is entirely outside the mask
        // buffer, so its horizontal pass would have summed all-unmasked taps.
        float row = ((unsigned)yy < (unsigned)H)
            ? sx[threadIdx.y + HALO + d][threadIdx.x] : cGaussSum;
        acc += cGauss[HALO - abs(d)] * row;
    }
    out[((size_t)b * H + y) * W + x] = acc;
}

// Returns null when nothing is masked out -- the caller then skips the divide
// and reproduces the unmasked result bit for bit.
static float* _ssim_mask_coverage(
    int B, int H, int W,
    SsimImg img1, SsimImg img2, float sat,
    const bool* masks, int B_mask, int H_mask, int W_mask
) {
    if (masks == nullptr && sat <= 0.0f) return nullptr;
    const size_t n = (size_t)B * H * W;
    float* out = DevicePool::global().acquire_reused<float>(PoolSlot::SsimMaskWeight, n);
    _ssim_mask_coverage_kernel<<<_LAUNCH_ARGS_3D(W, H, B, COV_T, COV_T, 1)>>>(
        B, H, W, img1, img2, sat, masks, B_mask, H_mask, W_mask, out);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    return out;
}



// ------------------------------------------
// Forward Kernel: Fused SSIM
//  - Two-pass convolution to get mu1, mu2,
//    sigma1_sq, sigma2_sq, sigma12, etc.
//  - Writes final SSIM map to ssim_loss_map
//  - Optionally writes partial derivatives
//    to dm_dmu1, dm_dsigma1_sq, dm_dsigma12
// ------------------------------------------
template<bool is_l1, bool inplace>
__global__ void ssim_forward_kernel(
    int B, int H, int W,
    const float3* __restrict__ img1,
    const float3* __restrict__ img2,
    float* __restrict__ ssim,
    float ssim_loss_map_weight,
    float* __restrict__ ssim_loss_map,
    float3* __restrict__ dm_dmu1,
    float3* __restrict__ dm_dsigma1_sq,
    float3* __restrict__ dm_dsigma12
) {
    auto block = cg::this_thread_block();
    const int bIdx   = block.group_index().z;  // batch index
    const int pix_y  = block.group_index().y * BLOCK_Y + block.thread_index().y;
    const int pix_x  = block.group_index().x * BLOCK_X + block.thread_index().x;
    const int pix_id = pix_y * W + pix_x;
    const int num_pix = H * W;

    // Shared memory for the tile (img1, img2)
    __shared__ float3 sTile[SHARED_Y][SHARED_X][2];
    // After horizontal pass, store partial sums here
    // xconv[y][x] -> (sumX, sumX^2, sumY, sumY^2, sumXY)
    __shared__ float3 xconv[CONV_Y][CONV_X][5];

    // Each block processes B x C sub-batches. We loop over channels:

    // ------------------------------------------------------------
    // 1) Load (img1, img2) tile + halo into shared memory
    // ------------------------------------------------------------
    {
        const int tileSize = SHARED_Y * SHARED_X;
        const int threads = BLOCK_X * BLOCK_Y;
        const int steps = (tileSize + threads - 1) / threads;

        const int tileStartY = block.group_index().y * BLOCK_Y;
        const int tileStartX = block.group_index().x * BLOCK_X;

        for (int s = 0; s < steps; ++s) {
            int tid = s * threads + block.thread_rank();
            if (tid < tileSize) {
                int local_y = tid / SHARED_X;
                int local_x = tid % SHARED_X;
                int gy = tileStartY + local_y - HALO;
                int gx = tileStartX + local_x - HALO;

                float3 X = get_pix_value(img1, bIdx, gy, gx, H, W);
                float3 Y = get_pix_value(img2, bIdx, gy, gx, H, W);

                sTile[local_y][local_x][0] = X;
                sTile[local_y][local_x][1] = Y;
            }
        }
    }
    block.sync();

    // ------------------------------------------------------------
    // 2) Horizontal convolution (11x1) in shared memory
    //    We'll accumulate symmetrical pairs around center.
    // ------------------------------------------------------------
    {
        int ly = threadIdx.y;
        int lx = threadIdx.x + HALO;  // skip left halo

        float3 sumX   = make_float3(0);
        float3 sumX2  = make_float3(0);
        float3 sumY   = make_float3(0);
        float3 sumY2  = make_float3(0);
        float3 sumXY  = make_float3(0);

        // #pragma unroll for those 5 pairs
        #pragma unroll
        for (int d = 1; d <= HALO; ++d) {
            float w = cGauss[HALO - d];
            float3 Xleft  = sTile[ly][lx - d][0];
            float3 Yleft  = sTile[ly][lx - d][1];
            float3 Xright = sTile[ly][lx + d][0];
            float3 Yright = sTile[ly][lx + d][1];

            sumX  += (Xleft + Xright) * w;
            sumX2 += ((Xleft * Xleft) + (Xright * Xright)) * w;
            sumY  += (Yleft + Yright) * w;
            sumY2 += ((Yleft * Yleft) + (Yright * Yright)) * w;
            sumXY += ((Xleft * Yleft) + (Xright * Yright)) * w;
        }
        // center
        {
            float3 centerX = sTile[ly][lx][0];
            float3 centerY = sTile[ly][lx][1];
            float wc = cGauss[HALO];
            sumX  += centerX * wc;
            sumX2 += (centerX * centerX) * wc;
            sumY  += centerY * wc;
            sumY2 += (centerY * centerY) * wc;
            sumXY += (centerX * centerY) * wc;
        }

        // Write out partial sums
        xconv[ly][threadIdx.x][0] = sumX;
        xconv[ly][threadIdx.x][1] = sumX2;
        xconv[ly][threadIdx.x][2] = sumY;
        xconv[ly][threadIdx.x][3] = sumY2;
        xconv[ly][threadIdx.x][4] = sumXY;

        // Possibly handle second row in same warp
        int ly2 = ly + BLOCK_Y;
        if (ly2 < CONV_Y) {
            sumX   = make_float3(0); sumX2  = make_float3(0);
            sumY   = make_float3(0); sumY2  = make_float3(0);
            sumXY  = make_float3(0);

            #pragma unroll
            for (int d = 1; d <= HALO; ++d) {
                float w = cGauss[HALO - d];
                float3 Xleft  = sTile[ly2][lx - d][0];
                float3 Yleft  = sTile[ly2][lx - d][1];
                float3 Xright = sTile[ly2][lx + d][0];
                float3 Yright = sTile[ly2][lx + d][1];

                sumX  += (Xleft + Xright) * w;
                sumX2 += ((Xleft * Xleft) + (Xright * Xright)) * w;
                sumY  += (Yleft + Yright) * w;
                sumY2 += ((Yleft * Yleft) + (Yright * Yright)) * w;
                sumXY += ((Xleft * Yleft) + (Xright * Yright)) * w;
            }
            // center
            {
                float3 cx = sTile[ly2][lx][0];
                float3 cy = sTile[ly2][lx][1];
                float wc = cGauss[HALO];
                sumX  += cx * wc;
                sumX2 += (cx * cx) * wc;
                sumY  += cy * wc;
                sumY2 += (cy * cy) * wc;
                sumXY += (cx * cy) * wc;
            }
            xconv[ly2][threadIdx.x][0] = sumX;
            xconv[ly2][threadIdx.x][1] = sumX2;
            xconv[ly2][threadIdx.x][2] = sumY;
            xconv[ly2][threadIdx.x][3] = sumY2;
            xconv[ly2][threadIdx.x][4] = sumXY;
        }
    }
    block.sync();

    // ------------------------------------------------------------
    // 3) Vertical convolution (1x11) + final SSIM
    // ------------------------------------------------------------
    {
        int ly = threadIdx.y + HALO;
        int lx = threadIdx.x;

        float3 out0 = make_float3(0), out1 = make_float3(0), out2 = make_float3(0), out3 = make_float3(0), out4 = make_float3(0);

        #pragma unroll
        for (int d = 1; d <= HALO; ++d) {
            float w = cGauss[HALO - d];
            float3* top = xconv[ly - d][lx];
            float3* bot = xconv[ly + d][lx];

            out0 += (top[0] + bot[0]) * w;
            out1 += (top[1] + bot[1]) * w;
            out2 += (top[2] + bot[2]) * w;
            out3 += (top[3] + bot[3]) * w;
            out4 += (top[4] + bot[4]) * w;
        }
        // center
        {
            float wC = cGauss[HALO];
            float3* ctr = xconv[ly][lx];
            out0 += ctr[0] * wC;
            out1 += ctr[1] * wC;
            out2 += ctr[2] * wC;
            out3 += ctr[3] * wC;
            out4 += ctr[4] * wC;
        }

        float val = 0.0;
        int global_idx = bIdx * num_pix + pix_id;
        if (pix_x < W && pix_y < H) {
            static constexpr float kC1 = 0.01f * 0.01f;
            static constexpr float kC2 = 0.03f * 0.03f;

            float3 mu1 = out0;
            float3 mu2 = out2;
            float3 mu1_sq = mu1 * mu1;
            float3 mu2_sq = mu2 * mu2;

            float3 sigma1_sq = out1 - mu1_sq;
            float3 sigma2_sq = out3 - mu2_sq;
            float3 sigma12   = out4 - mu1 * mu2;

        if (!is_l1) {
            // standard SSIM

            float3 A = mu1_sq + mu2_sq + kC1;
            float3 B = sigma1_sq + sigma2_sq + kC2;
            float3 C_ = 2.f * mu1 * mu2 + kC1;
            float3 D_ = 2.f * sigma12 + kC2;

            float3 val3 = (C_ * D_) / (A * B);
            val = (val3.x + val3.y + val3.z) * (1.0f/3.0f);

            if (dm_dmu1) {
                // partial derivatives
                float3 d_m_dmu1 = (
                    (mu2 * 2.f * D_) / (A * B)
                    - (mu2 * 2.f * C_) / (A * B)
                    - (mu1 * 2.f * C_ * D_) / (A * A * B)
                    + (mu1 * 2.f * C_ * D_) / (A * B * B)
                );
                float3 d_m_dsigma1_sq = (-C_ * D_) / (A * B * B);
                float3 d_m_dsigma12   = (2.f * C_) / (A * B);

                dm_dmu1[global_idx]       = d_m_dmu1;
                dm_dsigma1_sq[global_idx] = d_m_dsigma1_sq;
                dm_dsigma12[global_idx]   = d_m_dsigma12;
                if (ssim_loss_map != nullptr) {
                    if (inplace)
                        ssim_loss_map[global_idx] += ssim_loss_map_weight * (1.0f - val);
                    else
                        ssim_loss_map[global_idx] = 1.0f - val;
                }
            }

        } else {
            // L1 version of SSIM
            float3 A = mu1_sq + mu2_sq + kC1;
            float3 B = sigma1_sq + sigma2_sq + kC2;
            float3 C_ = 2.f * mu1 * mu2 + kC1;
            float3 D_ = 2.f * sigma12 + kC2;

            float3 val3 = (C_ * D_) / (A * B);
            val3 = 1.0f - sqrtf(1.0f - val3);
            val = (val3.x + val3.y + val3.z) * (1.0f/3.0f);

            if (dm_dmu1) {
                // partial derivatives
                float3 d_m_dmu1 = (
                    (mu2 * 2.f * D_) / (A * B)
                    - (mu2 * 2.f * C_) / (A * B)
                    - (mu1 * 2.f * C_ * D_) / (A * A * B)
                    + (mu1 * 2.f * C_ * D_) / (A * B * B)
                );
                float3 d_m_dsigma1_sq = (-C_ * D_) / (A * B * B);
                float3 d_m_dsigma12   = (2.f * C_) / (A * B);

                float3 grad_val3 = 0.5f / fmaxf(1.0f - val3, 1e-5f);

                dm_dmu1[global_idx]       = d_m_dmu1 * grad_val3;
                dm_dsigma1_sq[global_idx] = d_m_dsigma1_sq * grad_val3;
                dm_dsigma12[global_idx]   = d_m_dsigma12 * grad_val3;
                if (ssim_loss_map != nullptr) {
                    if (inplace)
                        ssim_loss_map[global_idx] += ssim_loss_map_weight * (1.0f - val);
                    else
                        ssim_loss_map[global_idx] = 1.0f - val;
                }
            }
            }
        }

        // TODO: block reduce
        // Note that this value does not affect gradient; for display only
        cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
        val = cg::reduce(warp, val, cg::plus<float>());
        if (warp.thread_rank() == 0)
            atomicAdd(ssim, val/(B*W*H));
    }

}

// ------------------------------------------
// Backward Kernel: Apply chain rule to get
//    dL/d(img1) from partial derivatives
//    (dm_dmu1, dm_dsigma1_sq, dm_dsigma12)
//    and dL/dmap (the gradient from above).
// ------------------------------------------
template<bool inplace>
__global__ void ssim_backward_kernel(
    int B, int H, int W,
    const float3* __restrict__ img1,
    const float3* __restrict__ img2,
    const float dL_dmap,
    float3* __restrict__ dL_dimg1,
    const float3* __restrict__ dm_dmu1,
    const float3* __restrict__ dm_dsigma1_sq,
    const float3* __restrict__ dm_dsigma12
) {
    auto block = cg::this_thread_block();

    float grad = dL_dmap / (3.0f*B*W*H);

    const int pix_y  = block.group_index().y * BLOCK_Y + block.thread_index().y;
    const int pix_x  = block.group_index().x * BLOCK_X + block.thread_index().x;
    const int pix_id = pix_y * W + pix_x;
    const int num_pix = H * W;
    const int bIdx   = block.group_index().z;

    // Shared memory for the fused data:
    // [0]: dm_dmu1*dL, [1]: dm_dsigma1_sq*dL, [2]: dm_dsigma12*dL
    __shared__ float3 sData[3][SHARED_Y][SHARED_X];
    __shared__ float3 sScratch[CONV_Y][CONV_X][3];

    float3 p1 = make_float3(0), p2 = make_float3(0);
    if (pix_x < W && pix_y < H) {
        p1 = get_pix_value(img1, bIdx, pix_y, pix_x, H, W);
        p2 = get_pix_value(img2, bIdx, pix_y, pix_x, H, W);
    }

    // (1) Load + fuse multiplication
    {
        const int start_y = block.group_index().y * BLOCK_Y;
        const int start_x = block.group_index().x * BLOCK_X;

        int tid = threadIdx.y * blockDim.x + threadIdx.x;
        int warp_id = tid / 32;
        int lane_id = tid % 32;
        int totalThreads = BLOCK_X * BLOCK_Y;
        int num_warps = (totalThreads + 31) / 32;

        for (int row = warp_id; row < SHARED_Y; row += num_warps) {
            int gy = start_y + row - HALO;
            for (int col = lane_id; col < SHARED_X; col += 32) {
                int gx = start_x + col - HALO;

                float3 vmu   = get_pix_value(dm_dmu1,      bIdx, gy, gx, H, W);
                float3 vs1   = get_pix_value(dm_dsigma1_sq,bIdx, gy, gx, H, W);
                float3 vs12  = get_pix_value(dm_dsigma12,  bIdx, gy, gx, H, W);

                sData[0][row][col] = vmu  * grad;
                sData[1][row][col] = vs1  * grad;
                sData[2][row][col] = vs12 * grad;
            }
        }
    }
    block.sync();

    // (2) Horizontal pass
    {
        int ly = threadIdx.y;
        int lx = threadIdx.x + HALO;

        for (int pass = 0; pass < 2; ++pass) {
            int yy = ly + pass * BLOCK_Y;
            if (yy < CONV_Y) {
                float3 accum0 = make_float3(0), accum1 = make_float3(0), accum2 = make_float3(0);

                #pragma unroll
                for (int d = 1; d <= HALO; ++d) {
                    float w = cGauss[HALO - d];
                    float3 left0  = sData[0][yy][lx - d];
                    float3 left1  = sData[1][yy][lx - d];
                    float3 left2  = sData[2][yy][lx - d];

                    float3 right0 = sData[0][yy][lx + d];
                    float3 right1 = sData[1][yy][lx + d];
                    float3 right2 = sData[2][yy][lx + d];

                    accum0 += (left0 + right0) * w;
                    accum1 += (left1 + right1) * w;
                    accum2 += (left2 + right2) * w;
                }
                // center
                {
                    float wc = cGauss[HALO];
                    float3 c0 = sData[0][yy][lx];
                    float3 c1 = sData[1][yy][lx];
                    float3 c2 = sData[2][yy][lx];
                    accum0 += c0 * wc;
                    accum1 += c1 * wc;
                    accum2 += c2 * wc;
                }

                sScratch[yy][threadIdx.x][0] = accum0;
                sScratch[yy][threadIdx.x][1] = accum1;
                sScratch[yy][threadIdx.x][2] = accum2;
            }
        }
    }
    block.sync();

    // (3) Vertical pass -> finalize dL/d(img1)
    if (pix_x < W && pix_y < H) {
        int ly = threadIdx.y + HALO;
        int lx = threadIdx.x;

        float3 sum0 = make_float3(0), sum1 = make_float3(0), sum2 = make_float3(0);

        #pragma unroll
        for (int d = 1; d <= HALO; ++d) {
            float w = cGauss[HALO - d];
            float3* top = sScratch[ly - d][lx];
            float3* bot = sScratch[ly + d][lx];

            sum0 += (top[0] + bot[0]) * w;
            sum1 += (top[1] + bot[1]) * w;
            sum2 += (top[2] + bot[2]) * w;
        }
        // center
        {
            float wc = cGauss[HALO];
            float3* ctr = sScratch[ly][lx];
            sum0 += ctr[0] * wc;
            sum1 += ctr[1] * wc;
            sum2 += ctr[2] * wc;
        }

        // final accumulation
        float3 dL_dpix = sum0 + (2.f * p1) * sum1 + (p2) * sum2;

        int out_idx = bIdx * num_pix + pix_id;
        if (inplace)
            dL_dimg1[out_idx] += dL_dpix;
        else
            dL_dimg1[out_idx] = dL_dpix;
    }

}

#define BLOCK_X_ME 24
#define BLOCK_Y_ME 24

#define SHARED_X_ME (BLOCK_X_ME + 2 * HALO)
#define SHARED_Y_ME (BLOCK_Y_ME + 2 * HALO)

#define CONV_X_ME BLOCK_X_ME
#define CONV_Y_ME SHARED_Y_ME

#define LOAD_X_ME (BLOCK_X_ME + 4 * HALO)
#define LOAD_Y_ME (BLOCK_Y_ME + 4 * HALO)

// kF1 / kF2 >= 0 fix the images' formats at compile time: a format switch
// on every tap made this kernel ~65% slower.
template<bool inplace, int kF1 = -1, int kF2 = -1>
__global__ void memory_efficient_ssim_backward_kernel(
    int B, int H, int W,
    SsimImg img1,                      // [B, H, W, 3]
    SsimImg img2,                      // [B, H, W, 3]
    const bool* __restrict__ masks,  // [B_mask, H_mask, W_mask, 1]
    int B_mask, int H_mask, int W_mask,
    float sat,                       // clip threshold, or <= 0 to disable
    const float* __restrict__ mask_w,  // [B, H, W] gaussian-blurred mask, or null
    const float dL_dmap, // [1]
    float3* __restrict__ dL_dimg1,      // [B, H, W, 3]
    float* __restrict__ out_ssim_val,
    float ssim_loss_map_weight,
    float* __restrict__ ssim_loss_map,
    int ssim_loss_map_mode  // SsimLossMapMode: 0=skip, 1=LCS, 2=CS, 3=structure
) {
    if constexpr (kF1 >= 0) img1.f = (PixelFormat)kF1;
    if constexpr (kF2 >= 0) img2.f = (PixelFormat)kF2;
    auto block = cg::this_thread_block();
    // Materialize the loss-map write target up front -- when the mode is
    // SsimNone (0) we treat ssim_loss_map as null even if the caller passed
    // a buffer (lets callers reuse one allocation across multiple SSIM calls
    // with different modes).
    float* lm_out = (ssim_loss_map_mode != 0) ? ssim_loss_map : nullptr;

    const int bIdx   = block.group_index().z;
    const int pix_y  = block.group_index().y * BLOCK_Y_ME + block.thread_index().y;
    const int pix_x  = block.group_index().x * BLOCK_X_ME + block.thread_index().x;
    const int pix_id = pix_y * W + pix_x;
    const int num_pix = H * W;

    const float grad = dL_dmap / (3.0f * B * W * H);

    // __shared__ float sTile[LOAD_Y][LOAD_X][2];
    // __shared__ float xconv[LOAD_Y][SHARED_X][5];
    // __shared__ float sData[3][SHARED_Y][SHARED_X];
    // __shared__ float sScratch[CONV_Y][CONV_X][3];

    static constexpr int sTile_size = LOAD_Y_ME*LOAD_X_ME*2;
    static constexpr int xconv_size = LOAD_Y_ME*SHARED_X_ME*5;
    static constexpr int sData_size = 3*SHARED_Y_ME*SHARED_X_ME;
    static constexpr int sScratch_size = CONV_Y_ME*CONV_X_ME*3;
    // static constexpr int sSsim_size = BLOCK_X_ME*BLOCK_Y_ME;
    __shared__ float _shared_buffer1[sTile_size>sData_size?sTile_size:sData_size];
    __shared__ float _shared_buffer2[xconv_size>sScratch_size?xconv_size:sScratch_size];
    __shared__ float sSsim[BLOCK_Y_ME][BLOCK_X_ME];
    #define sTile(i, j, k) _shared_buffer1[((i)*LOAD_X_ME+(j))*2+(k)]
    #define xconv(i, j, k) _shared_buffer2[((i)*SHARED_X_ME+(j))*5+(k)]
    #define sData(i, j, k) _shared_buffer1[((i)*SHARED_Y_ME+(j))*SHARED_X_ME+(k)]
    #define sScratch(i, j, k) _shared_buffer2[((i)*CONV_X_ME+(j))*3+(k)]

    // TODO: some shared memory can be reused to improve occupancy

    float ssim_val = 0.0f;

    #pragma unroll
    for (int ci = 0; ci < 3; ci++) {
        if (ci != 0)
            block.sync();

    // ------------------------------------------------------------------
    // 1) Load (img1, img2) tile + halo into shared memory
    // ------------------------------------------------------------------
    {
        const int tileSize = LOAD_X_ME * LOAD_Y_ME;
        const int threads = BLOCK_X_ME * BLOCK_Y_ME;
        const int steps = (tileSize + threads - 1) / threads;

        const int tileStartY = block.group_index().y * BLOCK_Y_ME;
        const int tileStartX = block.group_index().x * BLOCK_X_ME;

        for (int s = 0; s < steps; ++s) {
            int tid = s * threads + block.thread_rank();
            if (tid < tileSize) {
                int local_y = tid / LOAD_X_ME;
                int local_x = tid % LOAD_X_ME;
                int gy = tileStartY + local_y - 2 * HALO;
                int gx = tileStartX + local_x - 2 * HALO;

                float X = get_pix_value(img1, bIdx, gy, gx, ci, H, W);
                float Y = get_pix_value(img2, bIdx, gy, gx, ci, H, W);
                bool mask = get_pix_mask(img1, img2, sat, masks, bIdx, gy, gx,
                                         B_mask, H_mask, W_mask, H, W);
                // Drop the sample instead of substituting for it: every window
                // sum below is over w*mask, and `mask_w` divides the coverage
                // back out, so the statistics are conditional on the mask.
                if (!mask)
                    X = Y = 0.0f;

                sTile(local_y, local_x, 0) = X;
                sTile(local_y, local_x, 1) = Y;
            }
        }
    }
    block.sync();

    // ------------------------------------------------------------------
    // 2) Horizontal convolution (11x1) in shared memory
    //    We'll accumulate symmetrical pairs around center.
    // ------------------------------------------------------------------
    #if 0
    for (int ly = threadIdx.y; ly < LOAD_Y_ME; ly += BLOCK_Y_ME)
    for (int lx = threadIdx.x + HALO; lx + HALO < LOAD_X_ME; lx += BLOCK_X_ME) {
    #else
    for (int l = block.thread_rank(); l < SHARED_X_ME*LOAD_Y_ME; l += BLOCK_X_ME*BLOCK_Y_ME) {
        int lx = l % SHARED_X_ME + HALO;
        int ly = l / SHARED_Y_ME;
    #endif

        float sumX   = 0.0f;
        float sumX2  = 0.0f;
        float sumY   = 0.0f;
        float sumY2  = 0.0f;
        float sumXY  = 0.0f;

        // #pragma unroll for those 5 pairs
        #pragma unroll
        for (int d = 1; d <= HALO; ++d) {
            float w = cGauss[HALO - d];
            float Xleft  = sTile(ly, lx - d, 0);
            float Yleft  = sTile(ly, lx - d, 1);
            float Xright = sTile(ly, lx + d, 0);
            float Yright = sTile(ly, lx + d, 1);

            sumX  += (Xleft + Xright) * w;
            sumX2 += ((Xleft * Xleft) + (Xright * Xright)) * w;
            sumY  += (Yleft + Yright) * w;
            sumY2 += ((Yleft * Yleft) + (Yright * Yright)) * w;
            sumXY += ((Xleft * Yleft) + (Xright * Yright)) * w;
        }
        // center
        {
            float centerX = sTile(ly, lx, 0);
            float centerY = sTile(ly, lx, 1);
            float wc = cGauss[HALO];
            sumX  += centerX * wc;
            sumX2 += (centerX * centerX) * wc;
            sumY  += centerY * wc;
            sumY2 += (centerY * centerY) * wc;
            sumXY += (centerX * centerY) * wc;
        }

        // Write out partial sums
        xconv(ly, lx-HALO, 0) = sumX;
        xconv(ly, lx-HALO, 1) = sumX2;
        xconv(ly, lx-HALO, 2) = sumY;
        xconv(ly, lx-HALO, 3) = sumY2;
        xconv(ly, lx-HALO, 4) = sumXY;

    }
    block.sync();

    // ------------------------------------------------------------
    // 3) Vertical convolution (1x11) + final SSIM
    // ------------------------------------------------------------
    #if 0
    for (int ly = threadIdx.y + HALO; ly + HALO < LOAD_Y_ME; ly += BLOCK_Y_ME)
    for (int lx = threadIdx.x; lx < SHARED_X_ME; lx += BLOCK_X_ME) {
    #else
    for (int l = block.thread_rank(); l < SHARED_X_ME*SHARED_Y_ME; l += BLOCK_X_ME*BLOCK_Y_ME) {
        int lx = l % SHARED_X_ME;
        int ly = l / SHARED_Y_ME + HALO;
    #endif

        float out0 = 0.0f, out1 = 0.0f, out2 = 0.0f, out3 = 0.0f, out4 = 0.0f;

        #pragma unroll
        for (int d = 1; d <= HALO; ++d) {
            float w = cGauss[HALO - d];

            out0 += (xconv(ly-d, lx, 0) + xconv(ly+d, lx, 0)) * w;
            out1 += (xconv(ly-d, lx, 1) + xconv(ly+d, lx, 1)) * w;
            out2 += (xconv(ly-d, lx, 2) + xconv(ly+d, lx, 2)) * w;
            out3 += (xconv(ly-d, lx, 3) + xconv(ly+d, lx, 3)) * w;
            out4 += (xconv(ly-d, lx, 4) + xconv(ly+d, lx, 4)) * w;
        }
        // center
        {
            float wC = cGauss[HALO];
            out0 += xconv(ly, lx, 0) * wC;
            out1 += xconv(ly, lx, 1) * wC;
            out2 += xconv(ly, lx, 2) * wC;
            out3 += xconv(ly, lx, 3) * wC;
            out4 += xconv(ly, lx, 4) * wC;
        }

        static constexpr float kC1 = 0.01f * 0.01f;
        static constexpr float kC2 = 0.03f * 0.03f;

        // This entry's window is centred on (cy, cx) -- the shared-memory tile
        // is offset by 2*HALO in y and HALO in x from the block's first pixel.
        const int cy = block.group_index().y * BLOCK_Y_ME + ly - 2 * HALO;
        const int cx = block.group_index().x * BLOCK_X_ME + lx - HALO;
        // Turns the sums above into means over the observed pixels. Unmasked
        // runs pass null and skip the divide, staying bit-identical.
        float inv_cov = 1.0f;
        if (mask_w != nullptr && (unsigned)cx < (unsigned)W &&
                (unsigned)cy < (unsigned)H)
            inv_cov = 1.0f / fmaxf(mask_w[((size_t)bIdx * H + cy) * W + cx], 1e-6f);

        float mu1 = out0 * inv_cov;
        float mu2 = out2 * inv_cov;
        float mu1_sq = mu1 * mu1;
        float mu2_sq = mu2 * mu2;

        float sigma1_sq = out1 * inv_cov - mu1_sq;
        float sigma2_sq = out3 * inv_cov - mu2_sq;
        float sigma12   = out4 * inv_cov - mu1 * mu2;

        float A = mu1_sq + mu2_sq + kC1;
        float B = sigma1_sq + sigma2_sq + kC2;
        float C_ = 2.f * mu1 * mu2 + kC1;
        float D_ = 2.f * sigma12 + kC2;

        // sSsim is keyed by the window's OWN centre (cy, cx), which is what the
        // write-out at the end reads back. Indexing it by (ly-HALO, lx) put
        // every value HALO pixels down and right of the pixel it describes.
        if ((out_ssim_val || lm_out)
                && lx >= HALO && lx < HALO + BLOCK_X_ME
                && ly >= 2 * HALO && ly < 2 * HALO + BLOCK_Y_ME
                && cx < W && cy < H
        ) {
            const bool cmask =
                get_pix_mask(img1, img2, sat, masks, bIdx, cy, cx,
                             B_mask, H_mask, W_mask, H, W);
            float ssim_v = (C_ * D_) / (A * B);
            if (lm_out) {
                // ssim_loss_map_mode selects which SSIM variant gets folded
                // into the densification loss map:
                //   1 = LCS (full SSIM, same as ssim_v).
                //   2 = CS (contrast*structure) -- D_ / B isolates spatial
                //       pattern/contrast from luminance.
                //   3 = structure only -- D_ / (2*sqrt(sigma1*sigma2) + C2),
                //       biases densification toward pattern/edge mismatches.
                float lm_v;
                if (ssim_loss_map_mode == (int)SsimLossMapMode::SsimCs) {
                    lm_v = D_ / B;
                } else if (ssim_loss_map_mode == (int)SsimLossMapMode::SsimStr) {
                    float s1s2 = sqrtf(fmaxf(sigma1_sq, 0.0f) * fmaxf(sigma2_sq, 0.0f));
                    lm_v = D_ / (2.f * s1s2 + kC2);
                } else {  // SsimFull (1) and any other non-zero value
                    lm_v = ssim_v;
                }
                // A masked pixel is not being reconstructed, so it carries no
                // densification error.
                if (!cmask)
                    lm_v = 1.0f;
                if (ci == 0)
                    sSsim[ly-2*HALO][lx-HALO] = lm_v;
                else
                    sSsim[ly-2*HALO][lx-HALO] += lm_v;
            }
            ssim_val += cmask ? ssim_v : 1.0f;
        }

        // partial derivatives
        float d_m_dmu1 = (
            (mu2 * 2.f * D_) / (A * B)
            - (mu2 * 2.f * C_) / (A * B)
            - (mu1 * 2.f * C_ * D_) / (A * A * B)
            + (mu1 * 2.f * C_ * D_) / (A * B * B)
        );
        float d_m_dsigma1_sq = (-C_ * D_) / (A * B * B);
        float d_m_dsigma12   = (2.f * C_) / (A * B);

        const int pix_y  = block.group_index().y * BLOCK_Y_ME + ly-HALO;
        const int pix_x  = block.group_index().x * BLOCK_X_ME + lx;
        // A window centred on a masked pixel scores the constant 1 in the
        // scalar above, so it must push no gradient onto the unmasked
        // neighbours it overlaps -- its moments are a divide by ~nothing.
        float masked_grad = grad * float(
            pix_x >= HALO && pix_y >= HALO && pix_x < W+HALO && pix_y < H+HALO
            && get_pix_mask(img1, img2, sat, masks, bIdx, pix_y - HALO,
                            pix_x - HALO, B_mask, H_mask, W_mask, H, W)
        );

        // Each moment was divided by the coverage, so its vjp carries the same
        // factor; the per-pixel `mask` gate below supplies the rest.
        sData(0, ly-HALO, lx) = d_m_dmu1 * masked_grad * inv_cov;
        sData(1, ly-HALO, lx) = d_m_dsigma1_sq * masked_grad * inv_cov;
        sData(2, ly-HALO, lx) = d_m_dsigma12 * masked_grad * inv_cov;
    }
    block.sync();

    // (2) Horizontal pass
    {
        int ly = threadIdx.y;
        int lx = threadIdx.x + HALO;

        for (int pass = 0; pass < 2; ++pass) {
            int yy = ly + pass * BLOCK_Y_ME;
            if (yy < CONV_Y_ME) {
                float accum0 = 0.0f, accum1 = 0.0f, accum2 = 0.0f;

                #pragma unroll
                for (int d = 1; d <= HALO; ++d) {
                    float w = cGauss[HALO - d];
                    float left0  = sData(0, yy, lx - d);
                    float left1  = sData(1, yy, lx - d);
                    float left2  = sData(2, yy, lx - d);

                    float right0 = sData(0, yy, lx + d);
                    float right1 = sData(1, yy, lx + d);
                    float right2 = sData(2, yy, lx + d);

                    accum0 += (left0 + right0) * w;
                    accum1 += (left1 + right1) * w;
                    accum2 += (left2 + right2) * w;
                }
                // center
                {
                    float wc = cGauss[HALO];
                    float c0 = sData(0, yy, lx);
                    float c1 = sData(1, yy, lx);
                    float c2 = sData(2, yy, lx);
                    accum0 += c0 * wc;
                    accum1 += c1 * wc;
                    accum2 += c2 * wc;
                }

                sScratch(yy, threadIdx.x, 0) = accum0;
                sScratch(yy, threadIdx.x, 1) = accum1;
                sScratch(yy, threadIdx.x, 2) = accum2;
            }
        }
    }
    block.sync();

    // (3) Vertical pass -> finalize dL/d(img1); null when only the map is wanted
    if (dL_dimg1 != nullptr && pix_x < W && pix_y < H) {
        int ly = threadIdx.y + HALO;
        int lx = threadIdx.x;

        float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f;

        #pragma unroll
        for (int d = 1; d <= HALO; ++d) {
            float w = cGauss[HALO - d];
            sum0 += (sScratch(ly - d, lx, 0) + sScratch(ly + d, lx, 0)) * w;
            sum1 += (sScratch(ly - d, lx, 1) + sScratch(ly + d, lx, 1)) * w;
            sum2 += (sScratch(ly - d, lx, 2) + sScratch(ly + d, lx, 2)) * w;
        }
        // center
        {
            float wc = cGauss[HALO];
            sum0 += sScratch(ly, lx, 0) * wc;
            sum1 += sScratch(ly, lx, 1) * wc;
            sum2 += sScratch(ly, lx, 2) * wc;
        }

        // final accumulation
        float p1 = get_pix_value(img1, bIdx, pix_y, pix_x, ci, H, W);
        float p2 = get_pix_value(img2, bIdx, pix_y, pix_x, ci, H, W);
        bool mask = get_pix_mask(img1, img2, sat, masks, bIdx, pix_y, pix_x,
                                 B_mask, H_mask, W_mask, H, W);
        float dL_dpix = sum0 + (2.f * p1) * sum1 + (p2) * sum2;
        // Masked pixels get no gradient; the ssim scalar already counted them
        // as a perfect match, up in the aligned window loop.
        if (!mask)
            dL_dpix = 0.0f;

        int out_idx = bIdx * num_pix + pix_id;
        if constexpr (inplace)
            ((float*)dL_dimg1)[out_idx*3+ci] += dL_dpix;
        else
            ((float*)dL_dimg1)[out_idx*3+ci] = dL_dpix;
    }

    }  // for (int ci = 0; ci < 3; ci++)

    #undef sTile
    #undef xconv
    #undef sData
    #undef sScratch

    if (lm_out && pix_x < W && pix_y < H) {
        float ssim_v = sSsim[threadIdx.y][threadIdx.x] / 3.0f;
        int out_idx = bIdx * num_pix + pix_id;
        if constexpr (inplace)
            lm_out[out_idx] += ssim_loss_map_weight * (1.0f - ssim_v);
        else
            lm_out[out_idx] = 1.0f - ssim_v;
    }
    if (out_ssim_val) {
        ssim_val /= 3*B*H*W;
        atomicAddFVec<BLOCK_X_ME*BLOCK_Y_ME>(out_ssim_val, ssim_val);
    }
}

// ------------------------------------------
// Blur-tolerant SSIM
// ------------------------------------------

// Each window scores a soft max of SSIM(render, ref) and SSIM(B render, ref),
// B a small Gaussian blur (docs/notes/ssim-blur-tolerance.md). Mirrored by
// fused_ssim.slang's ssim_bt_* entries.

#define BT_T 16      // the blur passes' tile
#define BT_R 6
#define BT_TEMP 50.0f

// The blur's taps by |offset|, summing to 1 over -r..r.
struct BtTaps {
    float w[BT_R + 1];
    int r;
};

// Forward: out = B(m img1) / B(m) and norm = B(m), m the kept pixels (in the
// image, unmasked, unclipped). Adjoint: out += m B^T g, g already over norm.
// The adjoint is its own pass because it needs g beyond a tile.
template<bool adjoint, int kF1 = -1, int kF2 = -1>
__global__ void _ssim_bt_blur_kernel(
    int B, int H, int W, SsimImg img1, SsimImg img2, const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask, float sat, const float* __restrict__ g,
    BtTaps t, float* __restrict__ out, float* __restrict__ norm
) {
    if constexpr (kF1 >= 0) img1.f = (PixelFormat)kF1;
    if constexpr (kF2 >= 0) img2.f = (PixelFormat)kF2;
    constexpr int kMax = BT_T + 2 * BT_R;
    constexpr int kC = adjoint ? 3 : 4;
    __shared__ float st[kMax * kMax * kC];
    __shared__ float sh[kMax * BT_T * kC];
    const int S = BT_T + 2 * t.r;
    const int b = blockIdx.z;
    const int x0 = blockIdx.x * BT_T - t.r, y0 = blockIdx.y * BT_T - t.r;
    const int tid = threadIdx.x;
    for (int i = tid; i < S * S; i += BT_T * BT_T) {
        int x = x0 + i % S, y = y0 + i / S;
        float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
            size_t p = ((size_t)b * H + y) * W + x;
            if constexpr (adjoint) {
                for (int c = 0; c < 3; ++c) v[c] = g[p * 3 + c];
            } else if (get_pix_mask(img1, img2, sat, masks, b, y, x, B_mask, H_mask,
                                    W_mask, H, W)) {
                for (int c = 0; c < 3; ++c) v[c] = pixel_load1(img1.p, img1.f, p * 3 + c);
                v[3] = 1.0f;
            }
        }
        for (int c = 0; c < kC; ++c) st[i * kC + c] = v[c];
    }
    __syncthreads();
    for (int i = tid; i < S * BT_T; i += BT_T * BT_T) {
        int ly = i / BT_T, lx = i % BT_T;
        float a[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        for (int d = -t.r; d <= t.r; ++d) {
            float w = t.w[abs(d)];
            for (int c = 0; c < kC; ++c) a[c] += w * st[(ly * S + lx + t.r + d) * kC + c];
        }
        for (int c = 0; c < kC; ++c) sh[i * kC + c] = a[c];
    }
    __syncthreads();
    const int tx = tid % BT_T, ty = tid / BT_T;
    const int x = blockIdx.x * BT_T + tx, y = blockIdx.y * BT_T + ty;
    if (x >= W || y >= H) return;
    float a[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int d = -t.r; d <= t.r; ++d) {
        float w = t.w[abs(d)];
        for (int c = 0; c < kC; ++c) a[c] += w * sh[((ty + t.r + d) * BT_T + tx) * kC + c];
    }
    size_t p = ((size_t)b * H + y) * W + x;
    if constexpr (adjoint) {
        if (get_pix_mask(img1, img2, sat, masks, b, y, x, B_mask, H_mask, W_mask, H, W))
            for (int c = 0; c < 3; ++c) out[p * 3 + c] += a[c];
    } else {
        float n = a[3] > 0.0f ? 1.0f / a[3] : 0.0f;
        for (int c = 0; c < 3; ++c) out[p * 3 + c] = a[c] * n;
        norm[p] = a[3];
    }
}

// SSIM of one window and its adjoints w.r.t. the raw window sums (mean,
// E[x^2], E[xy]) of the first image, as the plain kernel takes them.
__device__ __forceinline__ float _bt_ssim(float mu1, float mu2, float s11, float s22,
                                          float s12, float& a_mu, float& a_11,
                                          float& a_12, float& cs) {
    constexpr float kC1 = 0.01f * 0.01f, kC2 = 0.03f * 0.03f;
    float A = mu1 * mu1 + mu2 * mu2 + kC1, Bv = s11 + s22 + kC2;
    float C = 2.0f * mu1 * mu2 + kC1, D = 2.0f * s12 + kC2;
    a_mu = (mu2 * 2.0f * D) / (A * Bv) - (mu2 * 2.0f * C) / (A * Bv) -
           (mu1 * 2.0f * C * D) / (A * A * Bv) + (mu1 * 2.0f * C * D) / (A * Bv * Bv);
    a_11 = (-C * D) / (A * Bv * Bv);
    a_12 = (2.0f * C) / (A * Bv);
    cs = D / Bv;
    return (C * D) / (A * Bv);
}

// The render's window sums wait in registers while the blurred render's take
// the same shared buffer: 30 KB at T = 16. T = 24 (47.7 KB) needs 198
// registers here, one block per SM, and ran 10% slower; Vulkan takes it.
template<int T, int kF1 = -1, int kF2 = -1>
__global__ void __launch_bounds__(256) _ssim_bt_kernel(
    int B, int H, int W,
    SsimImg img1, SsimImg img2,
    const float* __restrict__ blurred,   // [B, H, W, 3] B img1
    const float* __restrict__ norm,      // [B, H, W] the blur's kept weight
    const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask,
    float sat,
    const float* __restrict__ mask_w,
    float dL_dmap,
    float* __restrict__ dL_dimg1,        // [B, H, W, 3], accumulated
    float* __restrict__ g_blur,          // [B, H, W, 3], written
    float* __restrict__ out_val,
    float map_weight,
    float* __restrict__ loss_map,
    int map_mode
) {
    if constexpr (kF1 >= 0) img1.f = (PixelFormat)kF1;
    if constexpr (kF2 >= 0) img2.f = (PixelFormat)kF2;
    constexpr int kThreads = 256;
    constexpr int S = T + 2 * HALO, L = T + 4 * HALO;
    constexpr int NW = (S * S + kThreads - 1) / kThreads;
    __shared__ float sm[2 * L * L + 5 * L * S];
    __shared__ float smap[T * T];
    const int tid = threadIdx.x;
    const int b = blockIdx.z;
    const int sx0 = blockIdx.x * T, sy0 = blockIdx.y * T;
    float* lm_out = map_mode != 0 ? loss_map : nullptr;
    const float grad = dL_dmap / (3.0f * B * W * H);
    float* tile = sm;                       // [L][L][2]
    float* xc = sm + 2 * L * L;             // [L][S][5]
    // Unblurred adjoints overwrite the tile in pass 1 (its window loop reads
    // only the sums); the blurred ones follow once the sums are done.
    float* adj = sm;                        // [3][S][S]
    float* adjb = sm + 2 * L * L;           // [3][S][S]
    float* scr = adjb + 3 * S * S;          // [S][T][3]
    float val = 0.0f;

    for (int ci = 0; ci < 3; ++ci) {
        float m[NW][5];   // pass 0's window sums
        float gb[NW][3];  // the blurred branch's adjoints
        // Pass 0: the render and the reference. Pass 1: the blurred render.
        for (int pass = 0; pass < 2; ++pass) {
            __syncthreads();
            for (int i = tid; i < L * L; i += kThreads) {
                int gy = sy0 + i / L - 2 * HALO, gx = sx0 + i % L - 2 * HALO;
                bool keep = get_pix_mask(img1, img2, sat, masks, b, gy, gx,
                                         B_mask, H_mask, W_mask, H, W);
                float X = 0.0f;
                if (keep && (unsigned)gx < (unsigned)W && (unsigned)gy < (unsigned)H)
                    X = pass == 0 ? get_pix_value(img1, b, gy, gx, ci, H, W)
                                  : blurred[(((size_t)b * H + gy) * W + gx) * 3 + ci];
                tile[i * 2] = X;
                if (pass == 0)
                    tile[i * 2 + 1] = keep ? get_pix_value(img2, b, gy, gx, ci, H, W) : 0.0f;
            }
            __syncthreads();
            // Pass 1 needs only the blurred render's three sums; y's are kept.
            const int nk = pass == 0 ? 5 : 3;
            for (int l = tid; l < S * L; l += kThreads) {
                int lx = l % S + HALO, ly = l / S;
                float s[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                #pragma unroll
                for (int d = -HALO; d <= HALO; ++d) {
                    float w = cGauss[HALO - abs(d)];
                    float X = tile[(ly * L + lx + d) * 2];
                    float Y = tile[(ly * L + lx + d) * 2 + 1];
                    s[0] += X * w;
                    s[1] += X * X * w;
                    s[2] += X * Y * w;
                    if (pass == 0) {
                        s[3] += Y * w;
                        s[4] += Y * Y * w;
                    }
                }
                #pragma unroll
                for (int k = 0; k < 5; ++k)
                    if (k < nk) xc[(ly * S + lx - HALO) * 5 + k] = s[k];
            }
            __syncthreads();
            #pragma unroll
            for (int k = 0; k < NW; ++k) {
                int l = tid + k * kThreads;
                if (l >= S * S) break;
                int lx = l % S, ly = l / S + HALO;
                float s[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                #pragma unroll
                for (int d = -HALO; d <= HALO; ++d) {
                    float w = cGauss[HALO - abs(d)];
                    #pragma unroll
                    for (int j = 0; j < 5; ++j)
                        if (j < nk) s[j] += xc[((ly + d) * S + lx) * 5 + j] * w;
                }
                if (pass == 0) {
                    #pragma unroll
                    for (int j = 0; j < 5; ++j) m[k][j] = s[j];
                    continue;
                }
                int cy = sy0 + ly - 2 * HALO, cx = sx0 + lx - HALO;
                bool in = (unsigned)cx < (unsigned)W && (unsigned)cy < (unsigned)H;
                float inv_cov = 1.0f;
                if (mask_w != nullptr && in)
                    inv_cov = 1.0f / fmaxf(mask_w[((size_t)b * H + cy) * W + cx], 1e-6f);
                float mx = m[k][0] * inv_cov, my = m[k][3] * inv_cov;
                float syy = m[k][4] * inv_cov - my * my;
                float sxx = m[k][1] * inv_cov - mx * mx, sxy = m[k][2] * inv_cov - mx * my;
                float mb = s[0] * inv_cov;
                float sbb = s[1] * inv_cov - mb * mb, sby = s[2] * inv_cov - mb * my;
                float cs0, cs1, a0, a1, a2;
                float S0 = _bt_ssim(mx, my, sxx, syy, sxy, a0, a1, a2, cs0);
                float S1 = _bt_ssim(mb, my, sbb, syy, sby, gb[k][0], gb[k][1], gb[k][2], cs1);
                float top = fmaxf(S0, S1);
                float e0 = __expf(BT_TEMP * (S0 - top)), e1 = __expf(BT_TEMP * (S1 - top));
                float w0 = e0 / (e0 + e1), w1 = 1.0f - w0;
                // ln 2 off, so two equal branches score what either does.
                float Sv = top + (__logf(e0 + e1) - 0.69314718f) / BT_TEMP;
                bool cmask = !in || get_pix_mask(img1, img2, sat, masks, b, cy, cx,
                                                  B_mask, H_mask, W_mask, H, W);
                if (lx >= HALO && lx < HALO + T && ly >= 2 * HALO && ly < 2 * HALO + T && in) {
                    if (lm_out) {
                        float lm = Sv;
                        if (map_mode == (int)SsimLossMapMode::SsimCs) {
                            lm = w0 * cs0 + w1 * cs1;
                        } else if (map_mode == (int)SsimLossMapMode::SsimStr) {
                            constexpr float kC2 = 0.03f * 0.03f;
                            float vy = fmaxf(syy, 0.0f);
                            lm = w0 * (2.0f * sxy + kC2) / (2.0f * sqrtf(fmaxf(sxx, 0.0f) * vy) + kC2) +
                                 w1 * (2.0f * sby + kC2) / (2.0f * sqrtf(fmaxf(sbb, 0.0f) * vy) + kC2);
                        }
                        if (!cmask) lm = 1.0f;
                        int r = (ly - 2 * HALO) * T + lx - HALO;
                        smap[r] = ci == 0 ? lm : smap[r] + lm;
                    }
                    val += cmask ? Sv : 1.0f;
                }
                // As in the plain kernel: no gradient from a dropped window.
                float gw = (in && cmask) ? grad * inv_cov : 0.0f;
                adj[l] = a0 * w0 * gw;
                adj[S * S + l] = a1 * w0 * gw;
                adj[2 * S * S + l] = a2 * w0 * gw;
                gb[k][0] *= w1 * gw; gb[k][1] *= w1 * gw; gb[k][2] *= w1 * gw;
            }
        }
        __syncthreads();
        #pragma unroll
        for (int k = 0; k < NW; ++k) {
            int l = tid + k * kThreads;
            if (l >= S * S) break;
            #pragma unroll
            for (int j = 0; j < 3; ++j) adjb[j * S * S + l] = gb[k][j];
        }
        // One branch at a time, so the 24-pixel tile fits 48 KB.
        for (int br = 0; br < 2; ++br) {
            __syncthreads();
            const float* g = br == 0 ? adj : adjb;
            for (int l = tid; l < S * T; l += kThreads) {
                int yy = l / T, cx = l % T;
                float a[3] = {0.0f, 0.0f, 0.0f};
                #pragma unroll
                for (int d = -HALO; d <= HALO; ++d) {
                    float w = cGauss[HALO - abs(d)];
                    const int o = yy * S + cx + HALO + d;
                    #pragma unroll
                    for (int j = 0; j < 3; ++j) a[j] += g[j * S * S + o] * w;
                }
                #pragma unroll
                for (int j = 0; j < 3; ++j) scr[l * 3 + j] = a[j];
            }
            __syncthreads();
            if (dL_dimg1 == nullptr) continue;
            for (int l = tid; l < T * T; l += kThreads) {
                int tx = l % T, ty = l / T;
                int px = sx0 + tx, py = sy0 + ty;
                if (px >= W || py >= H) continue;
                float a[3] = {0.0f, 0.0f, 0.0f};
                #pragma unroll
                for (int d = -HALO; d <= HALO; ++d) {
                    float w = cGauss[HALO - abs(d)];
                    #pragma unroll
                    for (int j = 0; j < 3; ++j) a[j] += scr[((ty + HALO + d) * T + tx) * 3 + j] * w;
                }
                size_t pix = ((size_t)b * H + py) * W + px, o = pix * 3 + ci;
                bool keep = get_pix_mask(img1, img2, sat, masks, b, py, px,
                                         B_mask, H_mask, W_mask, H, W);
                float Y = get_pix_value(img2, b, py, px, ci, H, W);
                if (br == 0) {
                    float X = get_pix_value(img1, b, py, px, ci, H, W);
                    dL_dimg1[o] += keep ? a[0] + 2.0f * X * a[1] + Y * a[2] : 0.0f;
                } else {
                    g_blur[o] = keep ? (a[0] + 2.0f * blurred[o] * a[1] + Y * a[2]) / norm[pix]
                                     : 0.0f;
                }
            }
        }
    }

    if (lm_out) {
        for (int l = tid; l < T * T; l += kThreads) {
            int px = sx0 + l % T, py = sy0 + l / T;
            if (px < W && py < H)
                lm_out[((size_t)b * H + py) * W + px] += map_weight * (1.0f - smap[l] / 3.0f);
        }
    }
    if (out_val)
        atomicAddFVec<kThreads>(out_val, val / (3.0f * B * H * W));
}


// ------------------------------------------
// Fused S-CIELAB (Zhang & Wandell 1997)
// ------------------------------------------

// sRGB -> linear -> XYZ (D65) -> Poirson-Wandell opponent channels, one blur
// per channel, back to XYZ, then CIELAB dE*ab against the reference taken
// through the same path. Mirrored by fused_ssim.slang's scielab_* entries.

__constant__ float cScielabRgbToOpp[9] = {
     0.266131096f,  0.601919772f,  0.000625524f,
    -0.125006493f,  0.037664685f, -0.133259103f,
    -0.080319477f, -0.331473061f,  0.449036729f,
};
// The opponent -> XYZ inverse with each row divided by the D65 white, so it
// lands directly on X/Xn, Y/Yn, Z/Zn.
__constant__ float cScielabOppToXyzN[9] = {
    0.659204924f, -1.964478203f, -0.161137515f,
    1.369855450f,  0.934755824f,  0.436229005f,
    1.382815274f,  1.305367938f,  2.329124914f,
};
// Per-channel blur, indexed like cGauss: sigma 0.5 / 1.5 / 2.0 px for
// luminance / red-green / blue-yellow. S-CIELAB's wide chroma lobes do not
// fit 11x11; the coarser loss scales stand in for them.
__constant__ float cScielabGauss[3][HALO+1] = {
#if 0
    // 0.5 / 1.5 / 2.0
    {1.5170981316200282e-22f, 9.961261650047284e-15f, 1.1979455936033156e-08f, 0.0002638650764154286f, 0.10645076942314473f, 0.7865707070419479f},
    {0.0010283800844791092f, 0.007598758135239184f, 0.03600077212843082f, 0.1093606895097f, 0.2130055377112537f, 0.2660117248617943f},
    {0.008812229292562283f, 0.027143577143479366f, 0.06511405659938266f, 0.12164907301380957f, 0.17699835683135567f, 0.2005654142388208f},
#else
    // 1.5 / 4.5 / 6.0
    {0.0010283800844791092f, 0.007598758135239184f, 0.03600077212843082f, 0.1093606895097f, 0.2130055377112537f, 0.2660117248617943f},
    {0.06136116169339518f, 0.07663081738142594f, 0.09108916096839265f, 0.10305837835540532f, 0.11098218377453624f, 0.11375659565368933f},
    {0.07327261223290525f, 0.083028747203882f, 0.09150641891586254f, 0.0980868750764954f, 0.10226016805075756f, 0.10369035704019461f},
#endif
};
// Charbonnier floor on dE for the gradient only, in dE units.
#define SCIELAB_EPS2 1e-4f

__device__ __forceinline__ float scielab_lin(float c) {
    return c <= 0.04045f ? c * (1.0f / 12.92f)
                         : powf((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}
__device__ __forceinline__ float scielab_dlin(float c) {
    return c <= 0.04045f ? 1.0f / 12.92f
                         : (2.4f / 1.055f) * powf((c + 0.055f) * (1.0f / 1.055f), 1.4f);
}
__device__ __forceinline__ float scielab_opp(float3 c, int k) {
    const float* m = cScielabRgbToOpp + 3 * k;
    return m[0] * scielab_lin(c.x) + m[1] * scielab_lin(c.y) + m[2] * scielab_lin(c.z);
}
// CIELAB's f and f'; the linear toe takes negative XYZ from a negative render.
__device__ __forceinline__ float scielab_f(float t) {
    return t > 216.0f / 24389.0f ? cbrtf(t) : t * (841.0f / 108.0f) + 4.0f / 29.0f;
}
__device__ __forceinline__ float scielab_df(float t) {
    if (t <= 216.0f / 24389.0f) return 841.0f / 108.0f;
    float r = cbrtf(t);
    return 1.0f / (3.0f * r * r);
}
__device__ __forceinline__ float3 scielab_xyzn(float a, float b, float c) {
    const float* m = cScielabOppToXyzN;
    return {m[0] * a + m[1] * b + m[2] * c,
            m[3] * a + m[4] * b + m[5] * c,
            m[6] * a + m[7] * b + m[8] * c};
}
// Kernel weight inside the image along one axis: 1 away from the border.
__device__ __forceinline__ float scielab_cov1d(int k, int x, int W) {
    if (x >= HALO && x < W - HALO) return 1.0f;
    float c = 0.0f;
    for (int d = -HALO; d <= HALO; ++d)
        if ((unsigned)(x + d) < (unsigned)W) c += cScielabGauss[k][HALO - abs(d)];
    return c;
}

// Per-channel kernel weight on kept pixels. Unlike SSIM's, taps off the image
// count as dropped, so the blur does not darken the border.
__global__ void _scielab_mask_coverage_kernel(
    int B, int H, int W,
    const SsimImg img1,
    const SsimImg img2,
    float sat,
    const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask,
    float3* __restrict__ out  // [B, H, W]
) {
    __shared__ float sm[COV_S][COV_S];
    __shared__ float sx[3][COV_S][COV_T];
    const int b = blockIdx.z;
    const int x0 = blockIdx.x * COV_T - HALO;
    const int y0 = blockIdx.y * COV_T - HALO;
    const int tid = threadIdx.y * COV_T + threadIdx.x;
    for (int i = tid; i < COV_S * COV_S; i += COV_T * COV_T) {
        int ly = i / COV_S, lx = i % COV_S;
        int gy = y0 + ly, gx = x0 + lx;
        sm[ly][lx] = (float)((unsigned)gx < (unsigned)W && (unsigned)gy < (unsigned)H &&
            get_pix_mask(img1, img2, sat, masks, b, gy, gx, B_mask, H_mask, W_mask, H, W));
    }
    __syncthreads();
    for (int i = tid; i < COV_S * COV_T; i += COV_T * COV_T) {
        int ly = i / COV_T, lx = i % COV_T;
        #pragma unroll
        for (int k = 0; k < 3; ++k) {
            float acc = 0.0f;
            #pragma unroll
            for (int d = -HALO; d <= HALO; ++d)
                acc += cScielabGauss[k][HALO - abs(d)] * sm[ly][lx + HALO + d];
            sx[k][ly][lx] = acc;
        }
    }
    __syncthreads();
    const int x = blockIdx.x * COV_T + threadIdx.x;
    const int y = blockIdx.y * COV_T + threadIdx.y;
    if (x >= W || y >= H) return;
    float c[3];
    #pragma unroll
    for (int k = 0; k < 3; ++k) {
        float acc = 0.0f;
        #pragma unroll
        for (int d = -HALO; d <= HALO; ++d)
            acc += cScielabGauss[k][HALO - abs(d)] * sx[k][threadIdx.y + HALO + d][threadIdx.x];
        c[k] = acc;
    }
    out[((size_t)b * H + y) * W + x] = {c[0], c[1], c[2]};
}

// Shared memory, in floats: two images' filtered channels 0 and 1, then the
// load tile, then the horizontal pass. Channel 2's filtered values overwrite
// the tile once its horizontal pass is done, which keeps the total at 28 KB.
#define SC_T 16
#define SC_LX (SC_T + 4 * HALO)
#define SC_SX (SC_T + 2 * HALO)
#define SC_FILT (SC_SX * SC_SX * 2)
#define SC_TILE (SC_LX * SC_LX * 2)
#define SC_XCONV (SC_LX * SC_SX * 2)

// The loss is dL_dmap * mean(dE); loss_map gets map_weight * dE added. Masked
// window centres score 0 and push no gradient. kF1 / kF2: as in the SSIM kernel.
template<int kF1 = -1, int kF2 = -1>
__global__ void fused_scielab_kernel(
    int B, int H, int W,
    SsimImg img1,
    SsimImg img2,
    const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask,
    float sat,
    const float3* __restrict__ cov,  // [B, H, W] from the coverage kernel, or null
    float dL_dmap,
    float3* __restrict__ dL_dimg1,   // [B, H, W] accumulated, or null
    float* __restrict__ out_val,     // mean dE accumulated, or null
    float map_weight,
    float* __restrict__ loss_map     // [B, H, W] accumulated, or null
) {
    if constexpr (kF1 >= 0) img1.f = (PixelFormat)kF1;
    if constexpr (kF2 >= 0) img2.f = (PixelFormat)kF2;
    __shared__ float sm[2 * SC_FILT + SC_TILE + SC_XCONV];
    #define scFilt(k, sy, sx, i) sm[(k) * SC_FILT + ((sy) * SC_SX + (sx)) * 2 + (i)]
    #define scTile(ly, lx, i) sm[2 * SC_FILT + ((ly) * SC_LX + (lx)) * 2 + (i)]
    #define scXconv(ly, sx, i) sm[2 * SC_FILT + SC_TILE + ((ly) * SC_SX + (sx)) * 2 + (i)]
    #define scGrad(sy, t) sm[2 * SC_FILT + SC_TILE + (sy) * SC_T + (t)]

    const int b = blockIdx.z;
    const int x0 = blockIdx.x * SC_T, y0 = blockIdx.y * SC_T;
    const int tid = threadIdx.y * SC_T + threadIdx.x;
    constexpr int kThreads = SC_T * SC_T;

    // Forward: filter each opponent channel of both images over the block
    // plus one halo -- the window centres the block's gradient reads.
    for (int k = 0; k < 3; ++k) {
        if (k) __syncthreads();
        for (int i = tid; i < SC_LX * SC_LX; i += kThreads) {
            int ly = i / SC_LX, lx = i % SC_LX;
            int gy = y0 + ly - 2 * HALO, gx = x0 + lx - 2 * HALO;
            float a = 0.0f, c = 0.0f;
            if ((unsigned)gx < (unsigned)W && (unsigned)gy < (unsigned)H &&
                    get_pix_mask(img1, img2, sat, masks, b, gy, gx,
                                 B_mask, H_mask, W_mask, H, W)) {
                size_t p = ((size_t)b * H + gy) * W + gx;
                a = scielab_opp(pixel_load3(img1.p, img1.f, p), k);
                c = scielab_opp(pixel_load3(img2.p, img2.f, p), k);
            }
            scTile(ly, lx, 0) = a;
            scTile(ly, lx, 1) = c;
        }
        __syncthreads();
        for (int i = tid; i < SC_LX * SC_SX; i += kThreads) {
            int ly = i / SC_SX, sx = i % SC_SX, lx = sx + HALO;
            float w = cScielabGauss[k][HALO];
            float s0 = scTile(ly, lx, 0) * w, s1 = scTile(ly, lx, 1) * w;
            #pragma unroll
            for (int d = 1; d <= HALO; ++d) {
                w = cScielabGauss[k][HALO - d];
                s0 += (scTile(ly, lx - d, 0) + scTile(ly, lx + d, 0)) * w;
                s1 += (scTile(ly, lx - d, 1) + scTile(ly, lx + d, 1)) * w;
            }
            scXconv(ly, sx, 0) = s0;
            scXconv(ly, sx, 1) = s1;
        }
        __syncthreads();
        for (int i = tid; i < SC_SX * SC_SX; i += kThreads) {
            int sy = i / SC_SX, sx = i % SC_SX, ly = sy + HALO;
            float w = cScielabGauss[k][HALO];
            float s0 = scXconv(ly, sx, 0) * w, s1 = scXconv(ly, sx, 1) * w;
            #pragma unroll
            for (int d = 1; d <= HALO; ++d) {
                w = cScielabGauss[k][HALO - d];
                s0 += (scXconv(ly - d, sx, 0) + scXconv(ly + d, sx, 0)) * w;
                s1 += (scXconv(ly - d, sx, 1) + scXconv(ly + d, sx, 1)) * w;
            }
            scFilt(k, sy, sx, 0) = s0;
            scFilt(k, sy, sx, 1) = s1;
        }
    }
    __syncthreads();

    // dE per window centre, and in img1's slot its adjoint w.r.t. the three
    // unnormalized window sums.
    const float gscale = dL_dmap / ((float)B * H * W);
    float val = 0.0f;
    for (int i = tid; i < SC_SX * SC_SX; i += kThreads) {
        int sy = i / SC_SX, sx = i % SC_SX;
        int cy = y0 + sy - HALO, cx = x0 + sx - HALO;
        float g[3] = {0.0f, 0.0f, 0.0f};
        if ((unsigned)cx < (unsigned)W && (unsigned)cy < (unsigned)H &&
                get_pix_mask(img1, img2, sat, masks, b, cy, cx,
                             B_mask, H_mask, W_mask, H, W)) {
            float ic[3];
            if (cov != nullptr) {
                float3 cv = cov[((size_t)b * H + cy) * W + cx];
                ic[0] = 1.0f / fmaxf(cv.x, 1e-6f);
                ic[1] = 1.0f / fmaxf(cv.y, 1e-6f);
                ic[2] = 1.0f / fmaxf(cv.z, 1e-6f);
            } else {
                #pragma unroll
                for (int k = 0; k < 3; ++k)
                    ic[k] = 1.0f / (scielab_cov1d(k, cx, W) * scielab_cov1d(k, cy, H));
            }
            float3 tx = scielab_xyzn(scFilt(0, sy, sx, 0) * ic[0],
                                     scFilt(1, sy, sx, 0) * ic[1],
                                     scFilt(2, sy, sx, 0) * ic[2]);
            float3 ty = scielab_xyzn(scFilt(0, sy, sx, 1) * ic[0],
                                     scFilt(1, sy, sx, 1) * ic[1],
                                     scFilt(2, sy, sx, 1) * ic[2]);
            float fxX = scielab_f(tx.x), fxY = scielab_f(tx.y), fxZ = scielab_f(tx.z);
            float fyX = scielab_f(ty.x), fyY = scielab_f(ty.y), fyZ = scielab_f(ty.z);
            float dL = 116.0f * (fxY - fyY);
            float da = 500.0f * ((fxX - fxY) - (fyX - fyY));
            float db = 200.0f * ((fxY - fxZ) - (fyY - fyZ));
            float de2 = dL * dL + da * da + db * db;
            float de = sqrtf(de2);
            if (sy >= HALO && sy < HALO + SC_T && sx >= HALO && sx < HALO + SC_T) {
                val += de;
                if (loss_map)
                    loss_map[((size_t)b * H + cy) * W + cx] += map_weight * de;
            }
            if (dL_dimg1) {
                float s = gscale * rsqrtf(de2 + SCIELAB_EPS2);
                float gX = 500.0f * s * da * scielab_df(tx.x);
                float gY = (116.0f * dL - 500.0f * da + 200.0f * db) * s * scielab_df(tx.y);
                float gZ = -200.0f * s * db * scielab_df(tx.z);
                const float* m = cScielabOppToXyzN;
                g[0] = (m[0] * gX + m[3] * gY + m[6] * gZ) * ic[0];
                g[1] = (m[1] * gX + m[4] * gY + m[7] * gZ) * ic[1];
                g[2] = (m[2] * gX + m[5] * gY + m[8] * gZ) * ic[2];
            }
        }
        #pragma unroll
        for (int k = 0; k < 3; ++k) scFilt(k, sy, sx, 0) = g[k];
    }

    if (dL_dimg1) {
        // Backward: the blur is symmetric, so its adjoint is the same blur.
        float gl[3] = {0.0f, 0.0f, 0.0f};
        for (int k = 0; k < 3; ++k) {
            __syncthreads();
            for (int i = tid; i < SC_SX * SC_T; i += kThreads) {
                int sy = i / SC_T, t = i % SC_T, sx = t + HALO;
                float w = cScielabGauss[k][HALO];
                float s = scFilt(k, sy, sx, 0) * w;
                #pragma unroll
                for (int d = 1; d <= HALO; ++d) {
                    w = cScielabGauss[k][HALO - d];
                    s += (scFilt(k, sy, sx - d, 0) + scFilt(k, sy, sx + d, 0)) * w;
                }
                scGrad(sy, t) = s;
            }
            __syncthreads();
            int sy = threadIdx.y + HALO;
            float w = cScielabGauss[k][HALO];
            float go = scGrad(sy, threadIdx.x) * w;
            #pragma unroll
            for (int d = 1; d <= HALO; ++d) {
                w = cScielabGauss[k][HALO - d];
                go += (scGrad(sy - d, threadIdx.x) + scGrad(sy + d, threadIdx.x)) * w;
            }
            const float* m = cScielabRgbToOpp + 3 * k;
            gl[0] += m[0] * go;
            gl[1] += m[1] * go;
            gl[2] += m[2] * go;
        }
        const int px = x0 + threadIdx.x, py = y0 + threadIdx.y;
        if (px < W && py < H &&
                get_pix_mask(img1, img2, sat, masks, b, py, px,
                             B_mask, H_mask, W_mask, H, W)) {
            size_t p = ((size_t)b * H + py) * W + px;
            float3 c = pixel_load3(img1.p, img1.f, p);
            float3& o = dL_dimg1[p];
            o.x += gl[0] * scielab_dlin(c.x);
            o.y += gl[1] * scielab_dlin(c.y);
            o.z += gl[2] * scielab_dlin(c.z);
        }
    }
    #undef scFilt
    #undef scTile
    #undef scXconv
    #undef scGrad

    if (out_val)
        atomicAddFVec<kThreads>(out_val, val / ((float)B * H * W));
}

// ------------------------------------------
// Fused FLIP (Andersson et al. 2020, LDR)
// ------------------------------------------

// FLIP at 36.6 px/degree, where its CSF and feature filters come out 11x11;
// docs/notes/flip.md has where this departs from the reference. Mirrored by
// fused_ssim.slang's flip_* entries.

// Linear sRGB -> Y, X - Y, Y - Z of D65-normalized XYZ: FLIP's YCxCz less its
// offset and scales, which the blurs commute with.
__constant__ float cFlipRgbToOpp[9] = {
    0.212636822f,  0.715182982f,  0.072180196f,
    0.221258567f, -0.338940609f,  0.117682042f,
    0.194884399f,  0.605717353f, -0.800601752f,
};
enum FlipFilter { kFlA, kFlRG, kFlBY, kFlG, kFlD, kFlP };
// Taps by |offset|: the channels' CSF blurs (BY's two Gaussians as their 1D
// marginal), then the detectors' Gaussian, its derivative (odd; the positive
// side) and its second derivative, each signed lobe summing to 1.
__constant__ float cFlipTaps[6][HALO + 1] = {
    {0.704064511f, 0.146642777f, 0.00132496784f, 0.0f, 0.0f, 0.0f},
    {0.664371978f, 0.165269872f, 0.00254413906f, 0.0f, 0.0f, 0.0f},
    {0.257677777f, 0.207930974f, 0.11076056f, 0.0402669907f, 0.0103173304f, 0.00188525584f},
    {0.266011725f, 0.213005538f, 0.10936069f, 0.0360007721f, 0.00759875814f, 0.00102838008f},
    {0.0f, 0.370273015f, 0.380209009f, 0.18774321f, 0.0528364683f, 0.008938298f},
    {-0.529182223f, -0.235408888f, 0.170187732f, 0.216094916f, 0.092912529f, 0.0208048228f},
};
// pc * cmax (cmax: HyAB^0.7 from Hunt-adjusted green to blue) and the two
// slopes of FLIP's error redistribution, with pt = 0.95.
#define FLIP_PCCMAX 16.510439366f
#define FLIP_SLOPE_LO 0.0575393531f
#define FLIP_SLOPE_HI 0.00201892467f
// Gradient-only floors where FLIP's power laws turn infinite: HyAB in L* (about
// one JND; FLIP_EC_FLOOR is its colour error) and the feature difference.
#define FLIP_HYAB_FLOOR 1.0f
#define FLIP_EC_FLOOR 0.0575393531f
#define FLIP_FEAT_FLOOR 2e-3f

// Filter kF around in[i] at stride s; the odd one takes the two sides'
// difference. A and RG reach 2 taps, the rest HALO.
template<int kF>
__device__ __forceinline__ float flip_tap(const float* in, int i, int s) {
    constexpr int kR = kF <= kFlRG ? 2 : HALO;
    float acc = kF == kFlD ? 0.0f : cFlipTaps[kF][0] * in[i];
    #pragma unroll
    for (int d = 1; d <= kR; ++d)
        acc += cFlipTaps[kF][d] * (kF == kFlD ? in[i + d * s] - in[i - d * s]
                                              : in[i + d * s] + in[i - d * s]);
    return acc;
}

__device__ __forceinline__ float flip_cov1d(int f, int x, int n) {
    float c = 0.0f;
    for (int d = -HALO; d <= HALO; ++d)
        if ((unsigned)(x + d) < (unsigned)n)
            c += (f == kFlD && d < 0 ? -1.0f : 1.0f) * cFlipTaps[f][abs(d)];
    return c;
}

// The weight each of a centre's filters puts on kept pixels: a CSF blur
// divides its window by it, and a detector's dropped taps read as the
// centre's value, i.e. it subtracts this times that value.
struct FlipCov { float a, rg, by, ex, ey, px, py; };

__device__ __forceinline__ FlipCov flip_cov(const uint4* cov, int b, int y, int x,
                                            int H, int W) {
    if (cov != nullptr) {
        const uint4 q = cov[((size_t)b * H + y) * W + x];
        const float2 v0 = __half22float2(*(const __half2*)&q.x);
        const float2 v1 = __half22float2(*(const __half2*)&q.y);
        const float2 v2 = __half22float2(*(const __half2*)&q.z);
        const float2 v3 = __half22float2(*(const __half2*)&q.w);
        return {v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, v3.x};
    }
    if (x >= HALO && x < W - HALO && y >= HALO && y < H - HALO)
        return {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const float gx = flip_cov1d(kFlG, x, W), gy = flip_cov1d(kFlG, y, H);
    return {flip_cov1d(kFlA, x, W) * flip_cov1d(kFlA, y, H),
            flip_cov1d(kFlRG, x, W) * flip_cov1d(kFlRG, y, H),
            flip_cov1d(kFlBY, x, W) * flip_cov1d(kFlBY, y, H),
            flip_cov1d(kFlD, x, W) * gy, gx * flip_cov1d(kFlD, y, H),
            flip_cov1d(kFlP, x, W) * gy, gx * flip_cov1d(kFlP, y, H)};
}

// flip_cov under a mask or the clip test, as half pairs (a rg | by ex | ey px |
// py -). Taps off the image count as dropped, as in the closed form.
__global__ void _flip_mask_coverage_kernel(
    int B, int H, int W,
    const SsimImg img1,
    const SsimImg img2,
    float sat,
    const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask,
    uint4* __restrict__ out  // [B, H, W]
) {
    __shared__ float sm[COV_S * COV_S];
    __shared__ float sx[6][COV_S * COV_T];
    const int b = blockIdx.z;
    const int x0 = blockIdx.x * COV_T - HALO;
    const int y0 = blockIdx.y * COV_T - HALO;
    const int tid = threadIdx.y * COV_T + threadIdx.x;
    for (int i = tid; i < COV_S * COV_S; i += COV_T * COV_T) {
        int gy = y0 + i / COV_S, gx = x0 + i % COV_S;
        sm[i] = (float)((unsigned)gx < (unsigned)W && (unsigned)gy < (unsigned)H &&
            get_pix_mask(img1, img2, sat, masks, b, gy, gx, B_mask, H_mask, W_mask, H, W));
    }
    __syncthreads();
    for (int i = tid; i < COV_S * COV_T; i += COV_T * COV_T) {
        const int j = (i / COV_T) * COV_S + i % COV_T + HALO;
        sx[kFlA][i] = flip_tap<kFlA>(sm, j, 1);
        sx[kFlRG][i] = flip_tap<kFlRG>(sm, j, 1);
        sx[kFlBY][i] = flip_tap<kFlBY>(sm, j, 1);
        sx[kFlG][i] = flip_tap<kFlG>(sm, j, 1);
        sx[kFlD][i] = flip_tap<kFlD>(sm, j, 1);
        sx[kFlP][i] = flip_tap<kFlP>(sm, j, 1);
    }
    __syncthreads();
    const int x = blockIdx.x * COV_T + threadIdx.x;
    const int y = blockIdx.y * COV_T + threadIdx.y;
    if (x >= W || y >= H) return;
    const int j = (threadIdx.y + HALO) * COV_T + threadIdx.x;
    auto pack = [](float a, float c) {
        __half2 h = __floats2half2_rn(a, c);
        return *(unsigned*)&h;
    };
    out[((size_t)b * H + y) * W + x] = {
        pack(flip_tap<kFlA>(sx[kFlA], j, COV_T), flip_tap<kFlRG>(sx[kFlRG], j, COV_T)),
        pack(flip_tap<kFlBY>(sx[kFlBY], j, COV_T), flip_tap<kFlG>(sx[kFlD], j, COV_T)),
        pack(flip_tap<kFlD>(sx[kFlG], j, COV_T), flip_tap<kFlG>(sx[kFlP], j, COV_T)),
        pack(flip_tap<kFlP>(sx[kFlG], j, COV_T), 0.0f)};
}

// The window-centre grid is the block plus one halo, as in the S-CIELAB
// kernel. A and RG reach 2 taps, so their tile and passes skip 3 rows a side.
#define FL_T 16
#define FL_LX (FL_T + 4 * HALO)
#define FL_SX (FL_T + 2 * HALO)
#define FL_RX (FL_SX + 4)
#define FL_NC (FL_SX * FL_SX)
#define FL_NT (FL_LX * FL_LX)
#define FL_NX (FL_LX * FL_SX)
#define FL_NG (FL_SX * FL_T)
#define FL_CPT ((FL_NC + FL_T * FL_T - 1) / (FL_T * FL_T))
// Forward: the three channels' tiles and five horizontal passes; BY's pass
// then reuses xg. Backward: seven adjoint grids and six passes, in the front.
#define FL_SMEM (2 * FL_NT + FL_RX * FL_RX + 3 * FL_NX + 2 * FL_RX * FL_SX)

// The loss is dL_dmap * mean(FLIP); loss_map gets map_weight times FLIP, or
// with map_features its feature error alone. Each thread keeps the centres it
// owns in registers across passes, so shared memory holds tiles, then adjoints.
template<int kF1 = -1, int kF2 = -1>
__global__ void fused_flip_kernel(
    int B, int H, int W,
    SsimImg img1,
    SsimImg img2,
    const bool* __restrict__ masks,
    int B_mask, int H_mask, int W_mask,
    float sat,
    const uint4* __restrict__ cov,   // [B, H, W] from the coverage kernel, or null
    float dL_dmap,
    float3* __restrict__ dL_dimg1,   // [B, H, W] accumulated, or null
    float* __restrict__ out_val,     // mean FLIP accumulated, or null
    float map_weight,
    bool map_features,
    float* __restrict__ loss_map     // [B, H, W] accumulated, or null
) {
    if constexpr (kF1 >= 0) img1.f = (PixelFormat)kF1;
    if constexpr (kF2 >= 0) img2.f = (PixelFormat)kF2;
    static_assert(7 * FL_NC + 6 * FL_NG <= FL_SMEM, "adjoints overlay the forward");
    __shared__ float sm[FL_SMEM];
    float* ty = sm;
    float* tby = ty + FL_NT;
    float* trg = tby + FL_NT;           // tile rows and columns [3, 33)
    float* xg = trg + FL_RX * FL_RX;
    float* xd = xg + FL_NX;
    float* xp = xd + FL_NX;
    float* xa = xp + FL_NX;             // tile rows [3, 33)
    float* xrg = xa + FL_RX * FL_SX;
    float* xby = xg;

    const int b = blockIdx.z;
    const int x0 = blockIdx.x * FL_T, y0 = blockIdx.y * FL_T;
    const int tid = threadIdx.y * FL_T + threadIdx.x;
    constexpr int kThreads = FL_T * FL_T;
    constexpr int kR0 = HALO - 2;

    // Per owned centre: the reference's feature magnitudes and Hunt-adjusted
    // Lab; the render's feature error, its winner's direction, filtered channels.
    float er[FL_CPT], pr[FL_CPT], rL[FL_CPT], ra[FL_CPT], rb[FL_CPT];
    float cY[FL_CPT], cD1[FL_CPT], cD2[FL_CPT];
    float df[FL_CPT], ux[FL_CPT], uy[FL_CPT];
    bool pt[FL_CPT];

    #pragma unroll
    for (int img = 0; img < 2; ++img) {
        const SsimImg& src = img ? img1 : img2;
        for (int i = tid; i < FL_NT; i += kThreads) {
            const int ly = i / FL_LX, lx = i % FL_LX;
            const int gy = y0 + ly - 2 * HALO, gx = x0 + lx - 2 * HALO;
            float o[3] = {0.0f, 0.0f, 0.0f};
            if ((unsigned)gx < (unsigned)W && (unsigned)gy < (unsigned)H &&
                    get_pix_mask(img1, img2, sat, masks, b, gy, gx,
                                 B_mask, H_mask, W_mask, H, W)) {
                const float3 c = pixel_load3(src.p, src.f, ((size_t)b * H + gy) * W + gx);
                const float l0 = scielab_lin(c.x), l1 = scielab_lin(c.y), l2 = scielab_lin(c.z);
                #pragma unroll
                for (int k = 0; k < 3; ++k)
                    o[k] = cFlipRgbToOpp[3 * k] * l0 + cFlipRgbToOpp[3 * k + 1] * l1 +
                           cFlipRgbToOpp[3 * k + 2] * l2;
            }
            ty[i] = o[0];
            tby[i] = o[2];
            if ((unsigned)(ly - kR0) < FL_RX && (unsigned)(lx - kR0) < FL_RX)
                trg[(ly - kR0) * FL_RX + lx - kR0] = o[1];
        }
        __syncthreads();
        for (int i = tid; i < FL_NX; i += kThreads) {
            const int ly = i / FL_SX, sx = i % FL_SX;
            const int j = ly * FL_LX + sx + HALO;
            xg[i] = flip_tap<kFlG>(ty, j, 1);
            xd[i] = flip_tap<kFlD>(ty, j, 1);
            xp[i] = flip_tap<kFlP>(ty, j, 1);
            if ((unsigned)(ly - kR0) < FL_RX) {
                const int r = (ly - kR0) * FL_SX + sx;
                xa[r] = flip_tap<kFlA>(ty, j, 1);
                xrg[r] = flip_tap<kFlRG>(trg, (ly - kR0) * FL_RX + sx + 2, 1);
            }
        }
        __syncthreads();
        // The four detector responses, A and RG. A detector's dropped taps
        // read as the centre: subtract its kept weight times the centre value.
        #pragma unroll
        for (int k = 0; k < FL_CPT; ++k) {
            const int i = tid + k * kThreads;
            if (i >= FL_NC) continue;
            const int sy = i / FL_SX, sx = i % FL_SX;
            const int cy = y0 + sy - HALO, cx = x0 + sx - HALO;
            if ((unsigned)cx >= (unsigned)W || (unsigned)cy >= (unsigned)H) continue;
            const FlipCov c = flip_cov(cov, b, cy, cx, H, W);
            const int j = (sy + HALO) * FL_SX + sx, jr = (sy + 2) * FL_SX + sx;
            const float yc = ty[(sy + HALO) * FL_LX + sx + HALO];
            const float ex = flip_tap<kFlG>(xd, j, FL_SX) - c.ex * yc;
            const float ey = flip_tap<kFlD>(xg, j, FL_SX) - c.ey * yc;
            const float px = flip_tap<kFlG>(xp, j, FL_SX) - c.px * yc;
            const float py = flip_tap<kFlP>(xg, j, FL_SX) - c.py * yc;
            cY[k] = flip_tap<kFlA>(xa, jr, FL_SX) * (1.0f / fmaxf(c.a, 1e-6f));
            cD1[k] = flip_tap<kFlRG>(xrg, jr, FL_SX) * (1.0f / fmaxf(c.rg, 1e-6f));
            const float en = sqrtf(ex * ex + ey * ey);
            const float pn = sqrtf(px * px + py * py);
            if (img == 0) {
                er[k] = en;
                pr[k] = pn;
            } else {
                // The larger of the two magnitude errors, and its gradient
                // w.r.t. the render's responses: a signed unit vector.
                const float de = en - er[k], dp = pn - pr[k];
                pt[k] = fabsf(dp) > fabsf(de);
                const float n = pt[k] ? pn : en;
                const float s = n > 0.0f ? ((pt[k] ? dp : de) < 0.0f ? -1.0f : 1.0f) / n : 0.0f;
                df[k] = fmaxf(fabsf(de), fabsf(dp));
                ux[k] = s * (pt[k] ? px : ex);
                uy[k] = s * (pt[k] ? py : ey);
            }
        }
        __syncthreads();
        for (int i = tid; i < FL_NX; i += kThreads)
            xby[i] = flip_tap<kFlBY>(tby, (i / FL_SX) * FL_LX + i % FL_SX + HALO, 1);
        __syncthreads();
        // The next image's tiles do not overlap xby, which this pass reads.
        #pragma unroll
        for (int k = 0; k < FL_CPT; ++k) {
            const int i = tid + k * kThreads;
            if (i >= FL_NC) continue;
            const int sy = i / FL_SX, sx = i % FL_SX;
            const int cy = y0 + sy - HALO, cx = x0 + sx - HALO;
            if ((unsigned)cx >= (unsigned)W || (unsigned)cy >= (unsigned)H) continue;
            const FlipCov c = flip_cov(cov, b, cy, cx, H, W);
            const float d2 = flip_tap<kFlBY>(xby, (sy + HALO) * FL_SX + sx, FL_SX) *
                             (1.0f / fmaxf(c.by, 1e-6f));
            if (img == 0) {
                const float fx = scielab_f(cD1[k] + cY[k]), fy = scielab_f(cY[k]),
                            fz = scielab_f(cY[k] - d2);
                rL[k] = 116.0f * fy - 16.0f;
                ra[k] = 5.0f * rL[k] * (fx - fy);
                rb[k] = 2.0f * rL[k] * (fy - fz);
            } else {
                cD2[k] = d2;
            }
        }
    }
    __syncthreads();

    // FLIP per centre, and its adjoints w.r.t. the render's window sums: one
    // grid per filter, read by the horizontal pass below.
    float* gA = sm;
    float* gRG = gA + FL_NC;
    float* gBY = gRG + FL_NC;
    float* gEx = gBY + FL_NC;
    float* gEy = gEx + FL_NC;
    float* gPx = gEy + FL_NC;
    float* gPy = gPx + FL_NC;
    const float gscale = dL_dmap / ((float)B * H * W);
    float val = 0.0f;
    #pragma unroll
    for (int k = 0; k < FL_CPT; ++k) {
        const int i = tid + k * kThreads;
        if (i >= FL_NC) continue;
        const int sy = i / FL_SX, sx = i % FL_SX;
        const int cy = y0 + sy - HALO, cx = x0 + sx - HALO;
        float g[7] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        if ((unsigned)cx < (unsigned)W && (unsigned)cy < (unsigned)H &&
                get_pix_mask(img1, img2, sat, masks, b, cy, cx,
                             B_mask, H_mask, W_mask, H, W)) {
            const float X = cD1[k] + cY[k], Z = cY[k] - cD2[k];
            const float fx = scielab_f(X), fy = scielab_f(cY[k]), fz = scielab_f(Z);
            const float L = 116.0f * fy - 16.0f;
            const float dL = L - rL[k];
            const float da = 5.0f * L * (fx - fy) - ra[k];
            const float db = 2.0f * L * (fy - fz) - rb[k];
            const float nab = sqrtf(da * da + db * db);
            const float hy = fabsf(dL) + nab;
            const float pw = powf(hy, 0.7f);
            const bool lo = pw < FLIP_PCCMAX;
            const float ec = lo ? FLIP_SLOPE_LO * pw
                                : 0.95f + (pw - FLIP_PCCMAX) * FLIP_SLOPE_HI;
            const float ef = fminf(sqrtf(df[k] * 0.70710678f), 1.0f);
            const float fl = ec > 0.0f ? powf(ec, 1.0f - ef) : 0.0f;
            if (sy >= HALO && sy < HALO + FL_T && sx >= HALO && sx < HALO + FL_T) {
                val += fl;
                if (loss_map)
                    loss_map[((size_t)b * H + cy) * W + cx] +=
                        map_weight * (map_features ? ef : fl);
            }
            if (dL_dimg1) {
                const FlipCov c = flip_cov(cov, b, cy, cx, H, W);
                const float g_hy = gscale * (1.0f - ef) * powf(fmaxf(ec, FLIP_EC_FLOOR), -ef) *
                                   (lo ? FLIP_SLOPE_LO : FLIP_SLOPE_HI) * 0.7f *
                                   powf(fmaxf(hy, FLIP_HYAB_FLOOR), -0.3f);
                const float gL = dL < 0.0f ? -g_hy : g_hy;
                const float inv = nab > 0.0f ? g_hy / nab : 0.0f;
                const float gah = da * inv, gbh = db * inv;
                const float gLt = gL + 5.0f * (fx - fy) * gah + 2.0f * (fy - fz) * gbh;
                const float gX = 5.0f * L * gah * scielab_df(X);
                const float gZ = -2.0f * L * gbh * scielab_df(Z);
                const float gY = (116.0f * gLt - 5.0f * L * gah + 2.0f * L * gbh) *
                                 scielab_df(cY[k]);
                g[0] = (gX + gY + gZ) * (1.0f / fmaxf(c.a, 1e-6f));
                g[1] = gX * (1.0f / fmaxf(c.rg, 1e-6f));
                g[2] = -gZ * (1.0f / fmaxf(c.by, 1e-6f));
                const float g_ef = ec > 0.0f ? -gscale * logf(ec) * fl : 0.0f;
                const float g_df = ef < 1.0f
                    ? g_ef * 0.35355339f * rsqrtf(fmaxf(df[k], FLIP_FEAT_FLOOR) * 0.70710678f)
                    : 0.0f;
                if (pt[k]) {
                    g[5] = g_df * ux[k];
                    g[6] = g_df * uy[k];
                } else {
                    g[3] = g_df * ux[k];
                    g[4] = g_df * uy[k];
                }
            }
        }
        #pragma unroll
        for (int f = 0; f < 7; ++f) sm[f * FL_NC + i] = g[f];
    }

    if (dL_dimg1) {
        // Backward: the adjoint of an odd filter is its negation, the rest are
        // symmetric. h1 goes on to G vertically, h2 to D, h3 to P.
        float* hA = sm + 7 * FL_NC;
        float* hRG = hA + FL_NG;
        float* hBY = hRG + FL_NG;
        float* h1 = hBY + FL_NG;
        float* h2 = h1 + FL_NG;
        float* h3 = h2 + FL_NG;
        __syncthreads();
        for (int i = tid; i < FL_NG; i += kThreads) {
            const int j = (i / FL_T) * FL_SX + i % FL_T + HALO;
            hA[i] = flip_tap<kFlA>(gA, j, 1);
            hRG[i] = flip_tap<kFlRG>(gRG, j, 1);
            hBY[i] = flip_tap<kFlBY>(gBY, j, 1);
            h1[i] = flip_tap<kFlP>(gPx, j, 1) - flip_tap<kFlD>(gEx, j, 1);
            h2[i] = flip_tap<kFlG>(gEy, j, 1);
            h3[i] = flip_tap<kFlG>(gPy, j, 1);
        }
        __syncthreads();
        const int px = x0 + threadIdx.x, py_ = y0 + threadIdx.y;
        if (px < W && py_ < H &&
                get_pix_mask(img1, img2, sat, masks, b, py_, px,
                             B_mask, H_mask, W_mask, H, W)) {
            const int j = (threadIdx.y + HALO) * FL_T + threadIdx.x;
            const int c = (threadIdx.y + HALO) * FL_SX + threadIdx.x + HALO;
            const FlipCov cv = flip_cov(cov, b, py_, px, H, W);
            const float gy = flip_tap<kFlA>(hA, j, FL_T) + flip_tap<kFlG>(h1, j, FL_T) -
                             flip_tap<kFlD>(h2, j, FL_T) + flip_tap<kFlP>(h3, j, FL_T) -
                             cv.ex * gEx[c] - cv.ey * gEy[c] - cv.px * gPx[c] - cv.py * gPy[c];
            const float g1 = flip_tap<kFlRG>(hRG, j, FL_T);
            const float g2 = flip_tap<kFlBY>(hBY, j, FL_T);
            const float* m = cFlipRgbToOpp;
            const size_t p = ((size_t)b * H + py_) * W + px;
            const float3 cc = pixel_load3(img1.p, img1.f, p);
            float3& o = dL_dimg1[p];
            o.x += (m[0] * gy + m[3] * g1 + m[6] * g2) * scielab_dlin(cc.x);
            o.y += (m[1] * gy + m[4] * g1 + m[7] * g2) * scielab_dlin(cc.y);
            o.z += (m[2] * gy + m[5] * g1 + m[8] * g2) * scielab_dlin(cc.z);
        }
    }

    if (out_val)
        atomicAddFVec<kThreads>(out_val, val / ((float)B * H * W));
}

// ------------------------------------------
// Interface functions using TorchTensorView
// ------------------------------------------

#include <core/Tensor.h>

static inline float3* _nullable_f3(const TorchTensorView& tv) {
    uint64_t ptr = std::get<0>(tv);
    return ptr ? (float3*)ptr : nullptr;
}
static inline float* _nullable_f(const TorchTensorView& tv) {
    uint64_t ptr = std::get<0>(tv);
    return ptr ? (float*)ptr : nullptr;
}
static inline bool* _nullable_b(const TorchTensorView& tv) {
    uint64_t ptr = std::get<0>(tv);
    return ptr ? (bool*)ptr : nullptr;
}

/*[AutoHeaderGeneratorExport]*/
float fused_ssim_forward(
    TorchTensorView img1,           // [B, H, W, 3]
    TorchTensorView img2,           // [B, H, W, 3]
    TorchTensorView dm_dmu1,        // [B, H, W, 3] output, or null
    TorchTensorView dm_dsigma1_sq,  // [B, H, W, 3] output, or null
    TorchTensorView dm_dsigma12,    // [B, H, W, 3] output, or null
    TorchTensorView ssim_loss_map,  // [B, H, W, 1] output, or null
    float ssim_loss_map_weight,
    bool inplace,
    bool is_l1
) {
    const auto& s = std::get<2>(img1);
    int B = s[0], H = s[1], W = s[2];

    float* ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
    cudaMemset(ssim_buf, 0, sizeof(float));

    auto fwd = inplace ?
        (is_l1 ? ssim_forward_kernel<true, true> : ssim_forward_kernel<false, true>) :
        (is_l1 ? ssim_forward_kernel<true, false> : ssim_forward_kernel<false, false>);

    fwd<<<_LAUNCH_ARGS_3D(W, H, B, BLOCK_X, BLOCK_Y, 1)>>>(
        B, H, W,
        (float3*)std::get<0>(img1),
        (float3*)std::get<0>(img2),
        ssim_buf,
        ssim_loss_map_weight,
        _nullable_f(ssim_loss_map),
        _nullable_f3(dm_dmu1),
        _nullable_f3(dm_dsigma1_sq),
        _nullable_f3(dm_dsigma12)
    );

    float ssim_val;
    cudaMemcpy(&ssim_val, ssim_buf, sizeof(float), cudaMemcpyDeviceToHost);
    return ssim_val;
}

/*[AutoHeaderGeneratorExport]*/
void fused_ssim_backward(
    TorchTensorView img1,           // [B, H, W, 3]
    TorchTensorView img2,           // [B, H, W, 3]
    float dL_dmap,
    TorchTensorView dm_dmu1,        // [B, H, W, 3] input, or null
    TorchTensorView dm_dsigma1_sq,  // [B, H, W, 3] input, or null
    TorchTensorView dm_dsigma12,    // [B, H, W, 3] input, or null
    TorchTensorView dL_dimg1,       // [B, H, W, 3] output
    bool inplace
) {
    const auto& s = std::get<2>(img1);
    int B = s[0], H = s[1], W = s[2];

    float3* p_dm_dmu1 = _nullable_f3(dm_dmu1);
    float3* p_dm_dsigma1_sq = _nullable_f3(dm_dsigma1_sq);
    float3* p_dm_dsigma12 = _nullable_f3(dm_dsigma12);

    if (p_dm_dmu1 && p_dm_dsigma1_sq && p_dm_dsigma12) {
        (inplace ? ssim_backward_kernel<true> : ssim_backward_kernel<false>)
        <<<_LAUNCH_ARGS_3D(W, H, B, BLOCK_X, BLOCK_Y, 1)>>>(
            B, H, W,
            (float3*)std::get<0>(img1),
            (float3*)std::get<0>(img2),
            dL_dmap,
            (float3*)std::get<0>(dL_dimg1),
            p_dm_dmu1,
            p_dm_dsigma1_sq,
            p_dm_dsigma12
        );
    }
    else {
        (inplace ? memory_efficient_ssim_backward_kernel<true> : memory_efficient_ssim_backward_kernel<false>)
        <<<_LAUNCH_ARGS_3D(W, H, B, BLOCK_X_ME, BLOCK_Y_ME, 1)>>>(
            B, H, W,
            SsimImg{(const void*)std::get<0>(img1), PixelFormat::F32},
            SsimImg{(const void*)std::get<0>(img2), PixelFormat::F32},
            nullptr,
            /*B_mask=*/0, /*H_mask=*/0, /*W_mask=*/0,
            /*sat=*/-1.0f,
            /*mask_w=*/nullptr,
            dL_dmap,
            (float3*)std::get<0>(dL_dimg1),
            nullptr, 1.0f, nullptr, false
        );
    }
}

static void _launch_ssim_bt(
    int B, int H, int W, SsimImg img1, SsimImg img2, const bool* masks,
    int B_mask, int H_mask, int W_mask, float sat, const float* mask_w,
    float dL_dmap, float* dL_dimg1, float* val, float map_weight, float* map,
    int map_mode, float sigma
) {
    BtTaps t{};
    t.r = std::min(BT_R, (int)std::ceil(3.0f * sigma));
    float sum = 0.0f;
    for (int d = 0; d <= t.r; ++d) {
        t.w[d] = std::exp(-0.5f * d * d / (sigma * sigma));
        sum += (d ? 2.0f : 1.0f) * t.w[d];
    }
    for (int d = 0; d <= t.r; ++d) t.w[d] /= sum;
    const size_t n = (size_t)B * H * W * 3;
    auto& pool = DevicePool::global();
    float* blurred = pool.acquire_reused<float>(PoolSlot::SsimBlurred, n);
    float* norm = pool.acquire_reused<float>(PoolSlot::SsimBlurNorm, n / 3);
    float* g_blur = dL_dimg1 != nullptr ? pool.acquire_reused<float>(PoolSlot::SsimBlurGrad, n)
                                        : nullptr;
    const dim3 grid((W + BT_T - 1) / BT_T, (H + BT_T - 1) / BT_T, B);
    auto with_f1 = [&](auto&& go) {
        if (img1.f == PixelFormat::U8) go(std::integral_constant<int, (int)PixelFormat::U8>{});
        else if (img1.f == PixelFormat::F16) go(std::integral_constant<int, (int)PixelFormat::F16>{});
        else go(std::integral_constant<int, (int)PixelFormat::F32>{});
    };
    auto with_f12 = [&](auto&& go) {
        with_f1([&](auto f1) {
            if (img2.f == PixelFormat::U8) go(f1, std::integral_constant<int, (int)PixelFormat::U8>{});
            else if (img2.f == PixelFormat::F16) go(f1, std::integral_constant<int, (int)PixelFormat::F16>{});
            else go(f1, std::integral_constant<int, (int)PixelFormat::F32>{});
        });
    };
    with_f12([&](auto f1, auto f2) {
        _ssim_bt_blur_kernel<false, decltype(f1)::value, decltype(f2)::value>
            <<<grid, BT_T * BT_T>>>(B, H, W, img1, img2, masks, B_mask, H_mask, W_mask,
                                    sat, nullptr, t, blurred, norm);
    });
    CHECK_DEVICE_ERROR(cudaGetLastError());
    with_f12([&](auto f1, auto f2) {
        _ssim_bt_kernel<16, decltype(f1)::value, decltype(f2)::value><<<grid, 256>>>(
            B, H, W, img1, img2, blurred, norm, masks, B_mask, H_mask, W_mask, sat, mask_w,
            dL_dmap, dL_dimg1, g_blur, val, map_weight, map, map_mode);
    });
    CHECK_DEVICE_ERROR(cudaGetLastError());
    if (g_blur != nullptr) {
        with_f12([&](auto f1, auto f2) {
            _ssim_bt_blur_kernel<true, decltype(f1)::value, decltype(f2)::value>
                <<<grid, BT_T * BT_T>>>(B, H, W, img1, img2, masks, B_mask, H_mask, W_mask,
                                        sat, g_blur, t, dL_dimg1, nullptr);
        });
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }
}

// Common kernel-launch helper: write SSIM scalar to ssim_buf if non-null,
// accumulate gradient into dL_dimg1, optionally write loss map.
// Internal: not exported through the auto-header generator.
static inline void _launch_fused_ssim_inplace(
    TorchTensorView img1,
    TorchTensorView img2,
    TorchTensorView mask,
    float dL_dmap,
    TorchTensorView dL_dimg1,
    float* ssim_buf,                // non-null to receive SSIM scalar
    TorchTensorView ssim_loss_map,
    float ssim_loss_map_weight,
    int ssim_loss_map_mode,
    float saturation_threshold,
    float blur_tolerance
) {
    const auto& s = std::get<2>(img1);
    int B = s[0], H = s[1], W = s[2];
    // Mask's [B, H, W] may differ from img1's (e.g. broadcast mask or
    // GT-resolution mask paired with renders at a different resolution).
    // Pass the mask's own dims so the kernel's bounds check stays inside
    // the mask buffer regardless of the per-pixel-loss image dims.
    bool* mask_ptr = _nullable_b(mask);
    int B_mask = 0, H_mask = 0, W_mask = 0;
    if (mask_ptr != nullptr) {
        const auto& ms = std::get<2>(mask);
        if (ms.size() >= 3) {
            B_mask = (int)ms[0];
            H_mask = (int)ms[1];
            W_mask = (int)ms[2];
        }
    }

    const SsimImg p_img1{(const void*)std::get<0>(img1), pixel_format(img1)};
    const SsimImg p_img2{(const void*)std::get<0>(img2), pixel_format(img2)};
    const float* mask_w =
        _ssim_mask_coverage(B, H, W, p_img1, p_img2, saturation_threshold,
                            mask_ptr, B_mask, H_mask, W_mask);

    if (blur_tolerance > 0.0f) {
        _launch_ssim_bt(B, H, W, p_img1, p_img2, mask_ptr, B_mask, H_mask, W_mask,
                        saturation_threshold, mask_w, dL_dmap, _nullable_f(dL_dimg1),
                        ssim_buf, ssim_loss_map_weight, _nullable_f(ssim_loss_map),
                        ssim_loss_map_mode, blur_tolerance);
        return;
    }
    auto launch = [&](auto f1, auto f2) {
        memory_efficient_ssim_backward_kernel<true, decltype(f1)::value, decltype(f2)::value>
            <<<_LAUNCH_ARGS_3D(W, H, B, BLOCK_X_ME, BLOCK_Y_ME, 1)>>>(
            B, H, W,
            p_img1,
            p_img2,
            mask_ptr,
            B_mask, H_mask, W_mask,
            saturation_threshold,
            mask_w,
            dL_dmap,
            (float3*)std::get<0>(dL_dimg1),
            ssim_buf,
            ssim_loss_map_weight,
            _nullable_f(ssim_loss_map),
            ssim_loss_map_mode
        );
    };
    using F32 = std::integral_constant<int, (int)PixelFormat::F32>;
    using U8 = std::integral_constant<int, (int)PixelFormat::U8>;
    using F16 = std::integral_constant<int, (int)PixelFormat::F16>;
    auto with_f2 = [&](auto f1) {
        if (p_img2.f == PixelFormat::U8) launch(f1, U8{});
        else if (p_img2.f == PixelFormat::F16) launch(f1, F16{});
        else launch(f1, F32{});
    };
    if (p_img1.f == PixelFormat::U8) with_f2(U8{});
    else if (p_img1.f == PixelFormat::F16) with_f2(F16{});
    else with_f2(F32{});
}

/*[AutoHeaderGeneratorExport]*/
float fused_ssim_inplace(
    TorchTensorView img1,           // [B, H, W, 3]
    TorchTensorView img2,           // [B, H, W, 3]
    TorchTensorView mask,           // [B, H, W] bool, or null
    float dL_dmap,
    TorchTensorView dL_dimg1,       // [B, H, W, 3] output (accumulated)
    bool return_ssim_val,
    TorchTensorView ssim_loss_map,  // [B, H, W, 1] output, or null
    float ssim_loss_map_weight,
    int ssim_loss_map_mode,
    // Positive: a pixel above it in every channel of BOTH images drops out,
    // exactly as a masked one does.
    float saturation_threshold,
    // Positive: the blur sigma, in pixels, of blur-tolerant SSIM.
    float blur_tolerance
) {
    float* ssim_buf = nullptr;
    if (return_ssim_val) {
        ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
        cudaMemset(ssim_buf, 0, sizeof(float));
    }

    _launch_fused_ssim_inplace(
        img1, img2, mask, dL_dmap, dL_dimg1,
        ssim_buf, ssim_loss_map, ssim_loss_map_weight,
        ssim_loss_map_mode, saturation_threshold, blur_tolerance);

    if (return_ssim_val) {
        float val;
        cudaMemcpy(&val, ssim_buf, sizeof(float), cudaMemcpyDeviceToHost);
        return val;
    }
    return -1.0f;
}

/*[AutoHeaderGeneratorExport]*/
float fused_ssim_inplace_async(
    TorchTensorView img1,
    TorchTensorView img2,
    TorchTensorView mask,
    float dL_dmap,
    TorchTensorView dL_dimg1,
    TorchTensorView ssim_loss_map,
    float ssim_loss_map_weight,
    int ssim_loss_map_mode,
    float saturation_threshold,
    float blur_tolerance,
    AsyncReadout<float>& readout
) {
    float* ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
    cudaMemset(ssim_buf, 0, sizeof(float));

    _launch_fused_ssim_inplace(
        img1, img2, mask, dL_dmap, dL_dimg1,
        ssim_buf, ssim_loss_map, ssim_loss_map_weight,
        ssim_loss_map_mode, saturation_threshold, blur_tolerance);

    const float* prev = readout.read_previous();
    float val = prev ? prev[0] : 0.0f;
    readout.issue(ssim_buf);
    return val;
}

static inline void _launch_fused_scielab_inplace(
    TorchTensorView img1,
    TorchTensorView img2,
    TorchTensorView mask,
    float dL_dmap,
    TorchTensorView dL_dimg1,
    float* val_buf,
    TorchTensorView loss_map,
    float loss_map_weight,
    float saturation_threshold
) {
    const auto& s = std::get<2>(img1);
    int B = s[0], H = s[1], W = s[2];
    bool* mask_ptr = _nullable_b(mask);
    int B_mask = 0, H_mask = 0, W_mask = 0;
    if (mask_ptr != nullptr) {
        const auto& ms = std::get<2>(mask);
        if (ms.size() >= 3) {
            B_mask = (int)ms[0];
            H_mask = (int)ms[1];
            W_mask = (int)ms[2];
        }
    }
    const SsimImg p_img1{(const void*)std::get<0>(img1), pixel_format(img1)};
    const SsimImg p_img2{(const void*)std::get<0>(img2), pixel_format(img2)};

    float3* cov = nullptr;
    if (mask_ptr != nullptr || saturation_threshold > 0.0f) {
        cov = DevicePool::global().acquire_reused<float3>(PoolSlot::ScielabMaskWeight,
                                                          (size_t)B * H * W);
        _scielab_mask_coverage_kernel<<<_LAUNCH_ARGS_3D(W, H, B, COV_T, COV_T, 1)>>>(
            B, H, W, p_img1, p_img2, saturation_threshold, mask_ptr,
            B_mask, H_mask, W_mask, cov);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    auto launch = [&](auto f1, auto f2) {
        fused_scielab_kernel<decltype(f1)::value, decltype(f2)::value>
            <<<_LAUNCH_ARGS_3D(W, H, B, SC_T, SC_T, 1)>>>(
            B, H, W, p_img1, p_img2, mask_ptr, B_mask, H_mask, W_mask,
            saturation_threshold, cov, dL_dmap, _nullable_f3(dL_dimg1),
            val_buf, loss_map_weight, _nullable_f(loss_map));
        CHECK_DEVICE_ERROR(cudaGetLastError());
    };
    using F32 = std::integral_constant<int, (int)PixelFormat::F32>;
    using U8 = std::integral_constant<int, (int)PixelFormat::U8>;
    using F16 = std::integral_constant<int, (int)PixelFormat::F16>;
    auto with_f2 = [&](auto f1) {
        if (p_img2.f == PixelFormat::U8) launch(f1, U8{});
        else if (p_img2.f == PixelFormat::F16) launch(f1, F16{});
        else launch(f1, F32{});
    };
    if (p_img1.f == PixelFormat::U8) with_f2(U8{});
    else if (p_img1.f == PixelFormat::F16) with_f2(F16{});
    else with_f2(F32{});
}

// Returns mean S-CIELAB dE*ab over [B, H, W] (masked pixels count 0), or -1
// when not asked for. dL_dimg1 and loss_map may each be null.
/*[AutoHeaderGeneratorExport]*/
float fused_scielab_inplace(
    TorchTensorView img1,           // [B, H, W, 3], sRGB-encoded
    TorchTensorView img2,           // [B, H, W, 3]
    TorchTensorView mask,           // [B, H, W] bool, or null
    float dL_dmap,
    TorchTensorView dL_dimg1,       // [B, H, W, 3] output (accumulated)
    bool return_val,
    TorchTensorView loss_map,       // [B, H, W, 1] output (accumulated)
    float loss_map_weight,
    float saturation_threshold
) {
    float* val_buf = nullptr;
    if (return_val) {
        val_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
        cudaMemset(val_buf, 0, sizeof(float));
    }
    _launch_fused_scielab_inplace(img1, img2, mask, dL_dmap, dL_dimg1, val_buf,
                                  loss_map, loss_map_weight, saturation_threshold);
    if (return_val) {
        float val;
        cudaMemcpy(&val, val_buf, sizeof(float), cudaMemcpyDeviceToHost);
        return val;
    }
    return -1.0f;
}

/*[AutoHeaderGeneratorExport]*/
float fused_scielab_inplace_async(
    TorchTensorView img1,
    TorchTensorView img2,
    TorchTensorView mask,
    float dL_dmap,
    TorchTensorView dL_dimg1,
    TorchTensorView loss_map,
    float loss_map_weight,
    float saturation_threshold,
    AsyncReadout<float>& readout
) {
    float* val_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
    cudaMemset(val_buf, 0, sizeof(float));
    _launch_fused_scielab_inplace(img1, img2, mask, dL_dmap, dL_dimg1, val_buf,
                                  loss_map, loss_map_weight, saturation_threshold);
    const float* prev = readout.read_previous();
    float val = prev ? prev[0] : 0.0f;
    readout.issue(val_buf);
    return val;
}

static inline void _launch_fused_flip_inplace(
    TorchTensorView img1,
    TorchTensorView img2,
    TorchTensorView mask,
    float dL_dmap,
    TorchTensorView dL_dimg1,
    float* val_buf,
    TorchTensorView loss_map,
    float loss_map_weight,
    bool map_features,
    float saturation_threshold
) {
    const auto& s = std::get<2>(img1);
    int B = s[0], H = s[1], W = s[2];
    bool* mask_ptr = _nullable_b(mask);
    int B_mask = 0, H_mask = 0, W_mask = 0;
    if (mask_ptr != nullptr) {
        const auto& ms = std::get<2>(mask);
        if (ms.size() >= 3) {
            B_mask = (int)ms[0];
            H_mask = (int)ms[1];
            W_mask = (int)ms[2];
        }
    }
    const SsimImg p_img1{(const void*)std::get<0>(img1), pixel_format(img1)};
    const SsimImg p_img2{(const void*)std::get<0>(img2), pixel_format(img2)};

    uint4* cov = nullptr;
    if (mask_ptr != nullptr || saturation_threshold > 0.0f) {
        cov = DevicePool::global().acquire_reused<uint4>(PoolSlot::FlipMaskWeight,
                                                         (size_t)B * H * W);
        _flip_mask_coverage_kernel<<<_LAUNCH_ARGS_3D(W, H, B, COV_T, COV_T, 1)>>>(
            B, H, W, p_img1, p_img2, saturation_threshold, mask_ptr,
            B_mask, H_mask, W_mask, cov);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    auto launch = [&](auto f1, auto f2) {
        fused_flip_kernel<decltype(f1)::value, decltype(f2)::value>
            <<<_LAUNCH_ARGS_3D(W, H, B, FL_T, FL_T, 1)>>>(
            B, H, W, p_img1, p_img2, mask_ptr, B_mask, H_mask, W_mask,
            saturation_threshold, cov, dL_dmap, _nullable_f3(dL_dimg1),
            val_buf, loss_map_weight, map_features, _nullable_f(loss_map));
        CHECK_DEVICE_ERROR(cudaGetLastError());
    };
    using F32 = std::integral_constant<int, (int)PixelFormat::F32>;
    using U8 = std::integral_constant<int, (int)PixelFormat::U8>;
    using F16 = std::integral_constant<int, (int)PixelFormat::F16>;
    auto with_f2 = [&](auto f1) {
        if (p_img2.f == PixelFormat::U8) launch(f1, U8{});
        else if (p_img2.f == PixelFormat::F16) launch(f1, F16{});
        else launch(f1, F32{});
    };
    if (p_img1.f == PixelFormat::U8) with_f2(U8{});
    else if (p_img1.f == PixelFormat::F16) with_f2(F16{});
    else with_f2(F32{});
}

// Mean FLIP over [B, H, W] (masked pixels count 0), or -1 when not asked for;
// map_features maps the feature error alone. dL_dimg1, loss_map may be null.
/*[AutoHeaderGeneratorExport]*/
float fused_flip_inplace(
    TorchTensorView img1,           // [B, H, W, 3], sRGB-encoded
    TorchTensorView img2,           // [B, H, W, 3]
    TorchTensorView mask,           // [B, H, W] bool, or null
    float dL_dmap,
    TorchTensorView dL_dimg1,       // [B, H, W, 3] output (accumulated)
    bool return_val,
    TorchTensorView loss_map,       // [B, H, W, 1] output (accumulated)
    float loss_map_weight,
    bool map_features,
    float saturation_threshold
) {
    float* val_buf = nullptr;
    if (return_val) {
        val_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
        cudaMemset(val_buf, 0, sizeof(float));
    }
    _launch_fused_flip_inplace(img1, img2, mask, dL_dmap, dL_dimg1, val_buf,
                               loss_map, loss_map_weight, map_features,
                               saturation_threshold);
    if (return_val) {
        float val;
        cudaMemcpy(&val, val_buf, sizeof(float), cudaMemcpyDeviceToHost);
        return val;
    }
    return -1.0f;
}

/*[AutoHeaderGeneratorExport]*/
float fused_flip_inplace_async(
    TorchTensorView img1,
    TorchTensorView img2,
    TorchTensorView mask,
    float dL_dmap,
    TorchTensorView dL_dimg1,
    TorchTensorView loss_map,
    float loss_map_weight,
    bool map_features,
    float saturation_threshold,
    AsyncReadout<float>& readout
) {
    float* val_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
    cudaMemset(val_buf, 0, sizeof(float));
    _launch_fused_flip_inplace(img1, img2, mask, dL_dmap, dL_dimg1, val_buf,
                               loss_map, loss_map_weight, map_features,
                               saturation_threshold);
    const float* prev = readout.read_previous();
    float val = prev ? prev[0] : 0.0f;
    readout.issue(val_buf);
    return val;
}
