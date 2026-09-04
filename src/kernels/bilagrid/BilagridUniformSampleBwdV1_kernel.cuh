#include "kernels/bilagrid/BilagridConfig.cuh"

#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>

namespace cg = cooperative_groups;

#ifdef PATCHED
__global__ void bilagrid_patched_sample_backward_v1_kernel_bilagrid(
#else
__global__ void bilagrid_uniform_sample_backward_v1_kernel_bilagrid(
#endif
#ifdef PATCHED
    const float* __restrict__ rgb,  // [N,m,h,w,3]
    const float* __restrict__ v_output,  // [N,m,h,w,3]
#else
    const float* __restrict__ rgb,  // [N,h,w,3]
    const float* __restrict__ v_output,  // [N,h,w,3]
#endif
    float* __restrict__ v_bilagrid,  // [N_grids,L,H,W,12] or [N,L,H,W,12]
    int N, int L, int H, int W,
#ifdef PATCHED
    int m,
#endif
    int h, int w,
#ifdef PATCHED
    int h0, int w0,
    const int* __restrict__ offsets,  // [N,m,2]
#endif
    int mult_x, int mult_y
#ifdef PATCHED
    , int m_batch_stride
#endif
#ifndef PATCHED
    , const int* __restrict__ grid_indices  // [N], or nullptr -> identity
#endif
) {
#ifdef PATCHED
    int idx = blockIdx.z * kBilagridBwdV1BlockZ + threadIdx.z;
    bool inside = (idx < (N*m*L));
    int zi = idx % L; idx /= L;
    int m_batch_i = idx % m_batch_stride; idx /= m_batch_stride;
    int ni = idx;

    // offsets += (ni * m + mi) * 2;
    // int2 offset = {offsets[0], offsets[1]};
    // int x_base = max((offset.x * W) / w0 - 1, 0);
    // int y_base = max((offset.y * H) / h0 - 1, 0);
    int x_base = 0, y_base = 0;

    int x_idx = blockIdx.x * kBilagridBwdV1BlockX + threadIdx.x;
    int y_idx = blockIdx.y * kBilagridBwdV1BlockY + threadIdx.y;
    int xi = x_idx / mult_x + x_base, xf = x_idx % mult_x;
    int yi = y_idx / mult_y + y_base, yf = y_idx % mult_y;
    // printf("x_idx=%d y_idx=%d  xi=%d xf=%d yi=%d yf=%d\n", x_idx, y_idx, xi, xf, yi, yf);

    inside &= (xi >= 0 && xi < W && yi >= 0 && yi < H);
#else
    int x_idx = blockIdx.x * kBilagridBwdV1BlockX + threadIdx.x;
    int y_idx = blockIdx.y * kBilagridBwdV1BlockY + threadIdx.y;
    int idx = blockIdx.z * kBilagridBwdV1BlockZ + threadIdx.z;
    int xi = x_idx / mult_x, xf = x_idx % mult_x;
    int yi = y_idx / mult_y, yf = y_idx % mult_y;
    bool inside = (xi < W && yi < H && idx < (N*L));
    int zi = idx % L; idx /= L;
    int ni = idx;
#endif
    if (!inside && (
        mult_x*mult_y == 1 ||
        (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0)
    )) return;

    // Loop bounds
#ifndef PATCHED
    float sw = float(w-1)/float(W-1);
    int block_wi0 = max((int)ceil((xi-1)*sw), 0);  // same for all threads in the block
    int block_wi1 = min((int)floor((xi+1)*sw), w-1) + 1;
    float sh = float(h-1)/float(H-1);
    int block_hi0 = max((int)ceil((yi-1)*sh), 0);
    int block_hi1 = min((int)floor((yi+1)*sh), h-1) + 1;
    int x_step = (block_wi1-block_wi0+mult_x-1)/mult_x;
    int y_step = (block_hi1-block_hi0+mult_y-1)/mult_y;

    int wi0 = block_wi0+xf*x_step;
    int hi0 = block_hi0+yf*y_step;
    // int wi1 = min(block_wi0+(xf+1)*x_step, w);
    // int hi1 = min(block_hi0+(yf+1)*y_step, h);
    int wi1 = min(block_wi0+(xf+1)*x_step, block_wi1);
    int hi1 = min(block_hi0+(yf+1)*y_step, block_hi1);
#endif

    // Result for each affine mat channel
    float accum[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Loop over all samples for this batch
    if (inside)
#ifdef PATCHED
    for (int mi = m_batch_i; mi < m; mi += m_batch_stride)
#endif
    {
    #ifdef PATCHED
        int o_off = (ni * m + mi) * 2;
        int2 offset = {offsets[o_off+0], offsets[o_off+1]};
        float sw = float(w0-1)/float(W-1);
        int block_wi0 = max((int)ceil((xi-1)*sw), offset.x);  // same for all threads in the block
        int block_wi1 = min((int)floor((xi+1)*sw), min(offset.x+w,w0)-1) + 1;
        float sh = float(h0-1)/float(H-1);
        int block_hi0 = max((int)ceil((yi-1)*sh), offset.y);
        int block_hi1 = min((int)floor((yi+1)*sh), min(offset.y+h,h0)-1) + 1;
        int x_step = (block_wi1-block_wi0+mult_x-1)/mult_x;
        int y_step = (block_hi1-block_hi0+mult_y-1)/mult_y;
        // block_wi1 = max(block_wi1, block_wi0);
        // block_hi1 = max(block_hi1, block_hi0);
        if (!(block_wi1 > block_wi0 && block_hi1 > block_hi0) && (
            mult_x*mult_y == 1 ||
            (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0)
        )) continue;
        int wi0 = block_wi0+xf*x_step;
        int hi0 = block_hi0+yf*y_step;
        int wi1 = min(block_wi0+(xf+1)*x_step, block_wi1);
        int hi1 = min(block_hi0+(yf+1)*y_step, block_hi1);
    #endif

        for (int hi = hi0; hi < hi1; hi++)
        for (int wi = wi0; wi < wi1; wi++) {

        #ifdef PATCHED
            int g_off = (((ni*m + mi)*h + (hi-offset.y))*w + (wi-offset.x))*3;
        #else
            int g_off = ((ni*h + hi)*w + wi)*3;
        #endif
            float sr = rgb[g_off+0];
            float sg = rgb[g_off+1];
            float sb = rgb[g_off+2];

        #ifdef PATCHED
            float x = (float)wi / (float)(w0-1) * (float)(W-1);
            float y = (float)hi / (float)(h0-1) * (float)(H-1);
        #else
            float x = (float)wi / (float)(w-1) * (float)(W-1);
            float y = (float)hi / (float)(h-1) * (float)(H-1);
        #endif
            float z = (kC2G_r * sr + kC2G_g * sg + kC2G_b * sb);
            z = min(max(z, 0.0f), 1.0f) * (float)(L-1);

            int x0 = floorf(x), y0 = floorf(y), z0 = floorf(z);
            int x1 = min(x0+1, W-1);
            int y1 = min(y0+1, H-1);
            int z1 = min(z0+1, L-1);
            if (zi != z0 && zi != z1) continue;

            float fx = x-x0, fy = y-y0, fz = z-z0;
            float accum_t = 0.0;
            if (zi == z0) {
                if (xi == x0 && yi == y0) accum_t += (1-fx)*(1-fy)*(1-fz);
                if (xi == x1 && yi == y0) accum_t += fx*(1-fy)*(1-fz);
                if (xi == x0 && yi == y1) accum_t += (1-fx)*fy*(1-fz);
                if (xi == x1 && yi == y1) accum_t += fx*fy*(1-fz);
            }
            if (zi == z1) {
                if (xi == x0 && yi == y0) accum_t += (1-fx)*(1-fy)*fz;
                if (xi == x1 && yi == y0) accum_t += fx*(1-fy)*fz;
                if (xi == x0 && yi == y1) accum_t += (1-fx)*fy*fz;
                if (xi == x1 && yi == y1) accum_t += fx*fy*fz;
            }

            float dr = v_output[g_off+0];
            float dg = v_output[g_off+1];
            float db = v_output[g_off+2];

            // Every grid gradient this pixel gives is linear in its incoming
            // one, so a zero one -- a masked pixel, mostly -- gives nothing.
            if (dr == 0.0f && dg == 0.0f && db == 0.0f) continue;

            #pragma unroll
            for (int ci = 0; ci < 12; ci++) {
                int si = ci % 4;
                int di = ci / 4;

                float r_coeff = (si==0 ? sr : si==1 ? sg : si==2 ? sb : 1.f);
                float gout = (di==0 ? dr : di==1 ? dg : db);
                float grad_weight = r_coeff * gout;

                accum[ci] += accum_t * grad_weight;
            }

        }
    }

    // Write result.
    //
    // Output index uses `ni` (batch slot), NOT `g_id` (camera index). The
    // output buffer is sized [C_batch, 12, L, H, W] — sparse over the camera
    // axis. Reads of `bilagrid` above still use `g_id` (true camera index)
    // because that's the full [N_grids, ...] parameter table.
    //
    // Within a single ni, multiple threads can contribute to the same cell
    // when mult_x*mult_y > 1 (tile decomposition); use atomicAdd. Across ni
    // values, no contention (each ni has its own slice).
    // Channel-last write: 12 channels for the cell are contiguous in memory.
    int out_idx_start = (((ni*L + zi)*H + yi)*W + xi) * 12;

    if (mult_x*mult_y == 1) {
        #pragma unroll
        for (int ci = 0; ci < 12; ci++) {
            int out_idx = out_idx_start + ci;
            if (isfinite(accum[ci]) && accum[ci] != 0.0f)
                atomicAdd(v_bilagrid + out_idx, accum[ci]);
        }
        return;
    }

    // out_idx can be different for each thread, fall back to global atomicAdd
    if (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0) {
        #pragma unroll
        for (int ci = 0; ci < 12; ci++) {
            int out_idx = out_idx_start + ci;
            if (isfinite(accum[ci]) && accum[ci] != 0.0f)
                atomicAdd(v_bilagrid + out_idx, accum[ci]);
        }
        return;
    }

    #pragma unroll
    for (int ci = 0; ci < 12; ci++) {
        bilagrid_bwd_v1_block_atomic_add(v_bilagrid + out_idx_start + ci, accum[ci]);
        __syncthreads();
    }

}


#ifdef PATCHED
__global__ void bilagrid_patched_sample_backward_v1_kernel_rgb(
#else
__global__ void bilagrid_uniform_sample_backward_v1_kernel_rgb(
#endif
    BilagridReader bilagrid,  // [N_grids,L,H,W,12] or [N,L,H,W,12]
#ifdef PATCHED
    const float* __restrict__ rgb,  // [N,m,h,w,3]
    const float* __restrict__ v_output,  // [N,m,h,w,3]
    float* __restrict__ v_rgb,  // [N,m,h,w,3]
#else
    const float* __restrict__ rgb,  // [N,h,w,3]
    const float* __restrict__ v_output,  // [N,h,w,3]
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
#endif
#ifndef PATCHED
    , const int* __restrict__ grid_indices  // [N], or nullptr -> identity
#endif
) {
    int idx = blockIdx.x * kBilagridBwdV1RgbThreads + threadIdx.x;
#ifdef PATCHED
    int total = N * m * h * w;
#else
    int total = N * h * w;
#endif
    if (idx >= total) return;

    int tmp = idx;
    int wi = tmp % w; tmp /= w;
    int hi = tmp % h; tmp /= h;
#ifdef PATCHED
    int mi = tmp % m; tmp /= m;
#endif
    int ni = tmp;
#ifndef PATCHED
    int g_id = grid_indices ? grid_indices[ni] : ni;
#else
    int g_id = ni;
#endif

    // input and output colors
#ifdef PATCHED
    int g_off = (((ni * m + mi) * h + hi) * w + wi) * 3;
#else
    int g_off = ((ni * h + hi) * w + wi) * 3;
#endif
    float sr = rgb[g_off+0];
    float sg = rgb[g_off+1];
    float sb = rgb[g_off+2];
    float dr = v_output[g_off+0];
    float dg = v_output[g_off+1];
    float db = v_output[g_off+2];

    // The image gradient is linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- is zero out and skips the corner loads.
    if (dr == 0.0f && dg == 0.0f && db == 0.0f) {
        v_rgb[g_off+0] = 0.0f;
        v_rgb[g_off+1] = 0.0f;
        v_rgb[g_off+2] = 0.0f;
        return;
    }

    float vr = 0.0, vg = 0.0, vb = 0.0;

    // grid coords
#ifdef PATCHED
    offsets += (ni * m + mi) * 2;
    float x = (float)(wi + offsets[0]) / (float)(w0-1) * (float)(W-1);
    float y = (float)(hi + offsets[1]) / (float)(h0-1) * (float)(H-1);
#else
    float x = (float)wi / (float)(w-1) * (float)(W-1);
    float y = (float)hi / (float)(h-1) * (float)(H-1);
#endif
    // Clamp gz to [0,1] -- matches forward and bilagrid-grad bwd branch.
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = fminf(fmaxf(gz_raw, 0.0f), 1.0f);
    bool  gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * (L-1);
    int x0 = floorf(x), y0 = floorf(y), z0 = floorf(z);
    int x1 = min(x0+1, W-1);
    int y1 = min(y0+1, H-1);
    int z1 = min(z0+1, L-1);

    // fractional parts
    float fx = x - x0, fy = y - y0, fz = z - z0;
    float w000 = (1-fx)*(1-fy)*(1-fz);
    float w001 = fx*(1-fy)*(1-fz);
    float w010 = (1-fx)*fy*(1-fz);
    float w011 = fx*fy*(1-fz);
    float w100 = (1-fx)*(1-fy)*fz;
    float w101 = fx*(1-fy)*fz;
    float w110 = (1-fx)*fy*fz;
    float w111 = fx*fy*fz;

    // Channel-last corner offsets: 12 channels of each corner are contiguous.
    int grid_base = g_id * L * H * W * 12;
    int off000 = grid_base + ((z0*H + y0)*W + x0) * 12;
    int off001 = grid_base + ((z0*H + y0)*W + x1) * 12;
    int off010 = grid_base + ((z0*H + y1)*W + x0) * 12;
    int off011 = grid_base + ((z0*H + y1)*W + x1) * 12;
    int off100 = grid_base + ((z1*H + y0)*W + x0) * 12;
    int off101 = grid_base + ((z1*H + y0)*W + x1) * 12;
    int off110 = grid_base + ((z1*H + y1)*W + x0) * 12;
    int off111 = grid_base + ((z1*H + y1)*W + x1) * 12;

    // accumulate bilagrid gradient over 12 channels
    #pragma unroll
    for (int si = 0; si < 3; si++) {
        #pragma unroll
        for (int di = 0; di < 3; di++) {
            int ci = 4 * di + si;
            float gout = (di==0 ? dr : di==1 ? dg : db);

            float val =
                bilagrid[off000 + ci] * w000 +
                bilagrid[off001 + ci] * w001 +
                bilagrid[off010 + ci] * w010 +
                bilagrid[off011 + ci] * w011 +
                bilagrid[off100 + ci] * w100 +
                bilagrid[off101 + ci] * w101 +
                bilagrid[off110 + ci] * w110 +
                bilagrid[off111 + ci] * w111;
            (si == 0 ? vr : si == 1 ? vg : vb) += val * gout;
        }
    }

    // spatial derivatives for coords
    float dwdz[8] = {
        -(1-fx)*(1-fy), -fx*(1-fy),
        -(1-fx)*fy,     -fx*fy,
         (1-fx)*(1-fy),  fx*(1-fy),
         (1-fx)*fy,      fx*fy
    };
    const int corner_offs[8] = {
        off000, off001, off010, off011,
        off100, off101, off110, off111
    };

    // accumulate gradient into coords (chain through bilagrid values and rgb)
    float gz_grad = 0.f;
    #pragma unroll
    for (int corner = 0; corner < 8; ++corner) {
        int base = corner_offs[corner];
        float trilerp = 0.f;
        // gather the corresponding bilagrid value for each of the 12 channels
        #pragma unroll
        for (int ci = 0; ci < 12; ++ci) {
            float v = bilagrid[base + ci];
            int si = ci % 4, di = ci / 4;
            float r_coeff = (si==0 ? sr : si==1 ? sg : si==2 ? sb : 1.f);
            float gout = (di==0 ? dr : di==1 ? dg : db);
            trilerp += v * r_coeff * gout;
        }
        gz_grad += dwdz[corner] * (L-1) * trilerp;
    }
    // Zero gz_grad outside the [0,1] clamp range -- the clamp's vjp.
    if (!gz_in_range) gz_grad = 0.0f;
    vr += kC2G_r * gz_grad;
    vg += kC2G_g * gz_grad;
    vb += kC2G_b * gz_grad;
    v_rgb[g_off+0] = isfinite(vr) ? vr : 0.0f;
    v_rgb[g_off+1] = isfinite(vg) ? vg : 0.0f;
    v_rgb[g_off+2] = isfinite(vb) ? vb : 0.0f;
}

