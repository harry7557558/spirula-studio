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

// The mask keeps its file resolution; out-of-image taps read unmasked.
__device__ __forceinline__ bool get_pix_value(
    const uint8_t* img,
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
    return img[b * H_mask * W_mask + ys * W_mask + xs] == 1;
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
    const uint8_t* masks,
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
    const uint8_t* __restrict__ masks,
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
    const uint8_t* masks, int B_mask, int H_mask, int W_mask
) {
    if (masks == nullptr && sat <= 0.0f) return nullptr;
    const size_t n = (size_t)B * H * W;
    float* out = DevicePool::global().acquire<float>(PoolSlot::SsimMaskWeight, n);
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
    const uint8_t* __restrict__ masks,  // [B_mask, H_mask, W_mask, 1]
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

    // (3) Vertical pass -> finalize dL/d(img1)
    if (pix_x < W && pix_y < H) {
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
static inline uint8_t* _nullable_b(const TorchTensorView& tv) {
    uint64_t ptr = std::get<0>(tv);
    return ptr ? (uint8_t*)ptr : nullptr;
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
    float saturation_threshold
) {
    const auto& s = std::get<2>(img1);
    int B = s[0], H = s[1], W = s[2];
    // Mask's [B, H, W] may differ from img1's (e.g. broadcast mask or
    // GT-resolution mask paired with renders at a different resolution).
    // Pass the mask's own dims so the kernel's bounds check stays inside
    // the mask buffer regardless of the per-pixel-loss image dims.
    uint8_t* mask_ptr = _nullable_b(mask);
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
    float saturation_threshold
) {
    float* ssim_buf = nullptr;
    if (return_ssim_val) {
        ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
        cudaMemset(ssim_buf, 0, sizeof(float));
    }

    _launch_fused_ssim_inplace(
        img1, img2, mask, dL_dmap, dL_dimg1,
        ssim_buf, ssim_loss_map, ssim_loss_map_weight,
        ssim_loss_map_mode, saturation_threshold);

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
    AsyncReadout<float>& readout
) {
    float* ssim_buf = DevicePool::global().acquire<float>(PoolSlot::SsimScalar, 1);
    cudaMemset(ssim_buf, 0, sizeof(float));

    _launch_fused_ssim_inplace(
        img1, img2, mask, dL_dmap, dL_dimg1,
        ssim_buf, ssim_loss_map, ssim_loss_map_weight,
        ssim_loss_map_mode, saturation_threshold);

    const float* prev = readout.read_previous();
    float val = prev ? prev[0] : 0.0f;
    readout.issue(ssim_buf);
    return val;
}
