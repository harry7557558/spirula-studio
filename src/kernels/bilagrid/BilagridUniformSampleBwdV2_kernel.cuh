#include "kernels/bilagrid/BilagridConfig.cuh"

#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>

namespace cg = cooperative_groups;


#ifndef PATCHED
__device__ inline void warp_reduce_and_atomicAdd(int addr, float val, float *basePtr) {
    const unsigned mask = __activemask();
    // const unsigned mask = ~0u;
    const int W = 8;
    int lane = threadIdx.x & (W - 1);

    int leader_idx = lane;
    float val_total = 0.0f;
    #pragma unroll
    for (int k = 0; k < W; ++k) {
        int addr_k = __shfl_sync(mask, addr, k, W);
        float val_k = __shfl_sync(mask, val, k, W);
        if (addr_k == addr) {
            if (k < lane)
                leader_idx = k;
            val_total += val_k;
        }
    }
    if (leader_idx == lane)
        atomicAdd(basePtr + addr, val_total);
}
#endif


#ifdef PATCHED
__global__ void bilagrid_patched_sample_backward_v2_kernel(
#else
__global__ void bilagrid_uniform_sample_backward_v2_kernel(
#endif
    BilagridReader bilagrid,  // [N,L,H,W,12]
#ifdef PATCHED
    const float* __restrict__ rgb,  // [N,m,h,w,3]
    const float* __restrict__ v_output,  // [N,m,h,w,3]
#else
    const float* __restrict__ rgb,  // [N,h,w,3]
    const float* __restrict__ v_output,  // [N,h,w,3]
#endif
    float* __restrict__ v_bilagrid,  // [N,L,H,W,12]
#ifdef PATCHED
    float* __restrict__ v_rgb,  // [N,m,h,w,3]
#else
    float* __restrict__ v_rgb,  // [N,h,w,3]
#endif
    int N, int L, int H, int W,
#ifdef PATCHED
    int m,
#endif
    int h, int w
#ifdef PATCHED
    , int h0, int w0,
    const int* __restrict__ offsets  // [N,m,2]
#else
    , const int* __restrict__ grid_indices  // [N], or nullptr -> identity
#endif
) {
#ifdef PATCHED
    // potentially higher cache hit rate
  #if 0
    int wi = blockIdx.x * blockDim.x + threadIdx.x;
    int hi = blockIdx.y * blockDim.y + threadIdx.y;
    int idx = blockIdx.z * blockDim.z + threadIdx.z;
  #else
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int wi = idx % w; idx /= w;
    int hi = idx % h; idx /= h;
  #endif
#else
    // potentially lower atomic add conflicts
    int wi = threadIdx.x * ((w+blockDim.x-1) / blockDim.x) + blockIdx.x;
    int hi = threadIdx.y * ((h+blockDim.y-1) / blockDim.y) + blockIdx.y;
    int idx = blockIdx.z * blockDim.z + threadIdx.z;
#endif

#ifdef PATCHED
    bool inside = (wi < w && hi < h && idx < (N*m));
    // if (!inside) return;
    int mi = idx % m;
    int ni = idx / m;
#else
    bool inside = (wi < w && hi < h && idx < N);
    // if (!inside) return;
    int ni = idx;
    // Reads use g_id (camera grid slot); writes use ni (per-batch grad buffer).
    int g_id = (grid_indices != nullptr) ? grid_indices[ni] : ni;
#endif

    // load RGB colors
#ifdef PATCHED
    int g_off = (((ni*m + mi)*h + hi)*w + wi) * 3;
#else
    int g_off = ((ni*h + hi)*w + wi) * 3;
#endif
    float sr = inside ? rgb[g_off+0] : 0.5f,
        sg = inside ? rgb[g_off+1] : 0.5f,
        sb = inside ? rgb[g_off+2] : 0.5f;
    sr = isfinite(sr) ? sr : 0.5f;
    sg = isfinite(sg) ? sg : 0.5f;
    sb = isfinite(sb) ? sb : 0.5f;

    // grid coords
#ifdef PATCHED
    offsets += (ni * m + mi) * 2;
    float x = (float)(wi + offsets[0]) / (float)(w0-1) * (float)(W-1);
    float y = (float)(hi + offsets[1]) / (float)(h0-1) * (float)(H-1);
#else
    float x = (float)wi / (float)(w-1) * (float)(W-1);
    float y = (float)hi / (float)(h-1) * (float)(H-1);
#endif
    // Clamp gz to [0,1] -- matches forward and V1 bilagrid-grad bwd branch.
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = fminf(fmaxf(gz_raw, 0.0f), 1.0f);
    bool  gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * (L-1);

    // floor + ceil, clamped
    int x0 = floorf(x), y0 = floorf(y), z0 = floorf(z);
    int x1 = min(x0+1, W-1);
    int y1 = min(y0+1, H-1);
    int z1 = min(z0+1, L-1);

    // fractional parts
    float fx = x - x0, fy = y - y0, fz = z - z0;

    // read rgb coeffs and upstream gradient
    float dr = inside ? v_output[g_off+0] : 0.0f;
    float dg = inside ? v_output[g_off+1] : 0.0f;
    float db = inside ? v_output[g_off+2] : 0.0f;
    dr = isfinite(dr) ? dr : 0.0f;
    dg = isfinite(dg) ? dg : 0.0f;
    db = isfinite(db) ? db : 0.0f;

    // Both gradients are linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- contributes nothing anywhere.
    if (dr == 0.0f && dg == 0.0f && db == 0.0f) {
        if (v_rgb != nullptr && inside) {
            v_rgb[g_off+0] = 0.0f;
            v_rgb[g_off+1] = 0.0f;
            v_rgb[g_off+2] = 0.0f;
        }
        return;
    }

    float vr = 0.0, vg = 0.0, vb = 0.0;

    // spatial derivatives for coords

    float gz_grad = 0.f;

#ifndef PATCHED
    if (!inside) return;
    #pragma unroll
    for (int corner = 0; corner < 8; ++corner) {
        int xi = (corner & 1) ? x1 : x0;
        int yi = (corner & 2) ? y1 : y0;
        int zi = (corner & 4) ? z1 : z0;

        float dfdz = ((corner & 1) ? fx : (1-fx)) *
            ((corner & 2) ? fy : (1-fy)) * ((corner & 4) ? 1 : -1);
        float f = dfdz * ((corner & 4) ? fz : (fz-1));

        // Channel-last: 12 channels of this corner are contiguous; `+ ci` is
        // innermost. READ grid values from the g_id slot, SCATTER grad into the
        // per-batch buffer indexed by ni (see the ni/g_id split note above).
        int cellidx = ((zi*H + yi)*W + xi);
        int rbase = (g_id*(L*H*W) + cellidx) * 12;
        int wbase = (ni *(L*H*W) + cellidx) * 12;

        float trilerp = 0.f;
        #pragma unroll
        for (int ci = 0; ci < 12; ++ci) {
            int si = ci % 4, di = ci / 4;

            float r_coeff = (si==0 ? sr : si==1 ? sg : si==2 ? sb : 1.f);
            float gout = (di==0 ? dr : di==1 ? dg : db);

            float v = bilagrid[rbase + ci];

            if (si < 3)
                (si == 0 ? vr : si == 1 ? vg : vb) += v * f * gout;

            float grad_weight = r_coeff * gout;
            trilerp += v * grad_weight;
            atomicAdd(v_bilagrid + wbase + ci, f * grad_weight);
        }
        gz_grad += dfdz * (L-1) * trilerp;
    }
#else
    if (!inside) return;
    // #pragma unroll
    for (int corner = 0; corner < 8; ++corner) {
        int xi = (corner & 1) ? x1 : x0;
        int yi = (corner & 2) ? y1 : y0;
        int zi = (corner & 4) ? z1 : z0;

        float dfdz = ((corner & 1) ? fx : (1-fx)) *
            ((corner & 2) ? fy : (1-fy)) * ((corner & 4) ? 1 : -1);
        float f = dfdz * ((corner & 4) ? fz : (fz-1));

        // Channel-last: 12 channels of this corner are contiguous; `+ ci` is innermost.
        int corner_base = (((ni*L + zi)*H + yi)*W + xi) * 12;

        #pragma unroll
        for (int ci = 0; ci < 12; ++ci) {
            int bidx = corner_base + ci;
            int si = ci % 4, di = ci / 4;

            float r_coeff = (si==0 ? sr : si==1 ? sg : si==2 ? sb : 1.f);
            float gout = (di==0 ? dr : di==1 ? dg : db);

            // BilagridReader returns by value; can't take address for __ldg.
            // The reader dispatches to fp32 passthrough or 16-bit decode.
            float v = bilagrid[bidx];

            if (si < 3)
                (si == 0 ? vr : si == 1 ? vg : vb) += v * f * gout;

            float grad_weight = r_coeff * gout;
            gz_grad += dfdz * (L-1) * v * grad_weight;

            float aa = f * grad_weight;
        #ifdef PATCHED
            warp_reduce_and_atomicAdd(bidx, aa, v_bilagrid);
        #else
            if (aa != 0.0f)
                atomicAdd(v_bilagrid+bidx, aa);
        #endif
        }
    }
#endif

    // Zero gz_grad outside the [0,1] clamp range -- the clamp's vjp.
    // (Replaces the prior z-discontinuity heuristic; same intent.)
    if (!gz_in_range) gz_grad = 0.0f;
    if (inside) {
        v_rgb[g_off+0] = vr + kC2G_r * gz_grad;
        v_rgb[g_off+1] = vg + kC2G_g * gz_grad;
        v_rgb[g_off+2] = vb + kC2G_b * gz_grad;
    }
}
