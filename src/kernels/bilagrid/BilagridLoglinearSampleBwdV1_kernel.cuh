#include "kernels/bilagrid/BilagridConfig.cuh"

#ifndef BILAGRID_LOGLINEAR_BWD_V1_SHMEM_CONSTANTS_DEFINED
#define BILAGRID_LOGLINEAR_BWD_V1_SHMEM_CONSTANTS_DEFINED
constexpr int kLoglinearShmemFootX  = (int)kBilagridBwdV1BlockX + 2;
constexpr int kLoglinearShmemFootY  = (int)kBilagridBwdV1BlockY + 2;
constexpr int kLoglinearShmemFootZ  = 3;
constexpr int kLoglinearFullChans   = 9;  // grid storage layout
constexpr int kLoglinearShmemDiags  = 3;  // diagonals actually read
constexpr int kLoglinearShmemCells  = kLoglinearShmemFootX * kLoglinearShmemFootY * kLoglinearShmemFootZ;
constexpr int kLoglinearBlockSize   = (int)kBilagridBwdV1BlockX
                                    * (int)kBilagridBwdV1BlockY
                                    * (int)kBilagridBwdV1BlockZ;
#endif

#ifdef PATCHED
__global__ void bilagrid_loglinear_patched_sample_backward_v1_kernel_bilagrid(
#else
__global__ void bilagrid_loglinear_uniform_sample_backward_v1_kernel_bilagrid(
#endif
    BilagridReader bilagrid,  // [N,L,H,W,9]
#ifdef PATCHED
    const float* __restrict__ rgb,       // [N,m,h,w,3]
    const float* __restrict__ v_output,  // [N,m,h,w,3]
#else
    const float* __restrict__ rgb,       // [N,h,w,3]
    const float* __restrict__ v_output,  // [N,h,w,3]
#endif
    float* __restrict__ v_bilagrid,      // [N,L,H,W,9]
    int N, int L, int H, int W,
#ifdef PATCHED
    int m,
#endif
    int h, int w,
#ifdef PATCHED
    int h0, int w0,
    const int* __restrict__ offsets,     // [N,m,2]
#endif
    int mult_x, int mult_y
#ifdef PATCHED
    , int m_batch_stride
#endif
#ifndef PATCHED
    , const int* __restrict__ grid_indices  // [N], or nullptr -> identity
#endif
) {
    // 1. Block-uniform decode.
#ifdef PATCHED
    const int block_z_total = N * m_batch_stride * L;
#else
    const int block_z_total = N * L;
#endif
    if ((int)blockIdx.z >= block_z_total) return;

    int idx_block = (int)blockIdx.z;
    const int zi = idx_block % L; idx_block /= L;
#ifdef PATCHED
    const int m_batch_i = idx_block % m_batch_stride; idx_block /= m_batch_stride;
#endif
    const int ni = idx_block;

#ifndef PATCHED
    const int g_id = (grid_indices != nullptr) ? grid_indices[ni] : ni;
#else
    const int g_id = ni;
#endif
    const int corner_base = g_id * L * H * W * kLoglinearFullChans;

    // 2. Cooperative preload of the 3 diagonal channels.
    const int base_xi = (int)((blockIdx.x * kBilagridBwdV1BlockX)) / mult_x - 1;
    const int base_yi = (int)((blockIdx.y * kBilagridBwdV1BlockY)) / mult_y - 1;
    const int base_zi = zi - 1;

    __shared__ float shmem_corners
        [kLoglinearShmemFootZ][kLoglinearShmemFootY][kLoglinearShmemFootX][kLoglinearShmemDiags];

    const int linear_tid =
        (threadIdx.z * (int)kBilagridBwdV1BlockY + threadIdx.y)
        * (int)kBilagridBwdV1BlockX + threadIdx.x;

    #pragma unroll
    for (int local = linear_tid; local < kLoglinearShmemCells; local += kLoglinearBlockSize) {
        const int lx = local % kLoglinearShmemFootX;
        const int ly = (local / kLoglinearShmemFootX) % kLoglinearShmemFootY;
        const int lz = local / (kLoglinearShmemFootX * kLoglinearShmemFootY);
        const int gx = min(max(base_xi + lx, 0), W - 1);
        const int gy = min(max(base_yi + ly, 0), H - 1);
        const int gz = min(max(base_zi + lz, 0), L - 1);
        const int g_off = corner_base + ((gz * H + gy) * W + gx) * kLoglinearFullChans;
        // Diagonal full-channel indices: ci_full = 4 * diag_idx (= 3*d + d).
        #pragma unroll
        for (int diag = 0; diag < kLoglinearShmemDiags; diag++) {
            shmem_corners[lz][ly][lx][diag] = bilagrid[g_off + 4 * diag];
        }
    }
    __syncthreads();

    // 3. Per-thread cell coords.
#ifdef PATCHED
    int x_base = 0, y_base = 0;
    int x_idx = blockIdx.x * kBilagridBwdV1BlockX + threadIdx.x;
    int y_idx = blockIdx.y * kBilagridBwdV1BlockY + threadIdx.y;
    int xi = x_idx / mult_x + x_base, xf = x_idx % mult_x;
    int yi = y_idx / mult_y + y_base, yf = y_idx % mult_y;
    bool inside = (xi >= 0 && xi < W && yi >= 0 && yi < H);
#else
    int x_idx = blockIdx.x * kBilagridBwdV1BlockX + threadIdx.x;
    int y_idx = blockIdx.y * kBilagridBwdV1BlockY + threadIdx.y;
    int xi = x_idx / mult_x, xf = x_idx % mult_x;
    int yi = y_idx / mult_y, yf = y_idx % mult_y;
    bool inside = (xi < W && yi < H);
#endif
    if (!inside && (
        mult_x * mult_y == 1 ||
        (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0)
    )) return;

#ifndef PATCHED
    float sw = float(w - 1) / float(W - 1);
    int block_wi0 = max((int)ceil((xi - 1) * sw), 0);
    int block_wi1 = min((int)floor((xi + 1) * sw), w - 1) + 1;
    float sh = float(h - 1) / float(H - 1);
    int block_hi0 = max((int)ceil((yi - 1) * sh), 0);
    int block_hi1 = min((int)floor((yi + 1) * sh), h - 1) + 1;
    int x_step = (block_wi1 - block_wi0 + mult_x - 1) / mult_x;
    int y_step = (block_hi1 - block_hi0 + mult_y - 1) / mult_y;
    int wi0 = block_wi0 + xf * x_step;
    int hi0 = block_hi0 + yf * y_step;
    int wi1 = min(block_wi0 + (xf + 1) * x_step, block_wi1);
    int hi1 = min(block_hi0 + (yf + 1) * y_step, block_hi1);
#endif

    // accum[] is over ALL 9 channels (writeback layout matches grid); only
    // the 3 diagonals receive non-zero contributions.
    float accum[kLoglinearFullChans] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

    // 4. Pixel loop.
    if (inside)
#ifdef PATCHED
    for (int mi = m_batch_i; mi < m; mi += m_batch_stride)
#endif
    {
    #ifdef PATCHED
        int o_off = (ni * m + mi) * 2;
        int2 offset = {offsets[o_off + 0], offsets[o_off + 1]};
        float sw = float(w0 - 1) / float(W - 1);
        int block_wi0 = max((int)ceil((xi - 1) * sw), offset.x);
        int block_wi1 = min((int)floor((xi + 1) * sw), min(offset.x + w, w0) - 1) + 1;
        float sh = float(h0 - 1) / float(H - 1);
        int block_hi0 = max((int)ceil((yi - 1) * sh), offset.y);
        int block_hi1 = min((int)floor((yi + 1) * sh), min(offset.y + h, h0) - 1) + 1;
        int x_step = (block_wi1 - block_wi0 + mult_x - 1) / mult_x;
        int y_step = (block_hi1 - block_hi0 + mult_y - 1) / mult_y;
        if (!(block_wi1 > block_wi0 && block_hi1 > block_hi0) && (
            mult_x * mult_y == 1 ||
            (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0)
        )) continue;
        int wi0 = block_wi0 + xf * x_step;
        int hi0 = block_hi0 + yf * y_step;
        int wi1 = min(block_wi0 + (xf + 1) * x_step, block_wi1);
        int hi1 = min(block_hi0 + (yf + 1) * y_step, block_hi1);
    #endif

        for (int hi = hi0; hi < hi1; hi++)
        for (int wi = wi0; wi < wi1; wi++) {
        #ifdef PATCHED
            int g_off = (((ni * m + mi) * h + (hi - offset.y)) * w + (wi - offset.x)) * 3;
        #else
            int g_off = ((ni * h + hi) * w + wi) * 3;
        #endif
            float sr = rgb[g_off + 0];
            float sg = rgb[g_off + 1];
            float sb = rgb[g_off + 2];

        #ifdef PATCHED
            float x = (float)wi / (float)(w0 - 1) * (float)(W - 1);
            float y = (float)hi / (float)(h0 - 1) * (float)(H - 1);
        #else
            float x = (float)wi / (float)(w - 1) * (float)(W - 1);
            float y = (float)hi / (float)(h - 1) * (float)(H - 1);
        #endif
            float z = (kC2G_r * sr + kC2G_g * sg + kC2G_b * sb);
            z = min(max(z, 0.0f), 1.0f) * (float)(L - 1);

            int x0 = floorf(x), y0 = floorf(y), z0 = floorf(z);
            int x1 = min(x0 + 1, W - 1);
            int y1 = min(y0 + 1, H - 1);
            int z1 = min(z0 + 1, L - 1);
            if (zi != z0 && zi != z1) continue;

            float fx = x - x0, fy = y - y0, fz = z - z0;
            float accum_t = 0.0f;
            if (zi == z0) {
                if (xi == x0 && yi == y0) accum_t += (1 - fx) * (1 - fy) * (1 - fz);
                if (xi == x1 && yi == y0) accum_t += fx * (1 - fy) * (1 - fz);
                if (xi == x0 && yi == y1) accum_t += (1 - fx) * fy * (1 - fz);
                if (xi == x1 && yi == y1) accum_t += fx * fy * (1 - fz);
            }
            if (zi == z1) {
                if (xi == x0 && yi == y0) accum_t += (1 - fx) * (1 - fy) * fz;
                if (xi == x1 && yi == y0) accum_t += fx * (1 - fy) * fz;
                if (xi == x0 && yi == y1) accum_t += (1 - fx) * fy * fz;
                if (xi == x1 && yi == y1) accum_t += fx * fy * fz;
            }

            // Clamp defensively: see BilagridPpispUniformSampleBwdV1_kernel.cuh
            // for the fast-math integer-boundary rationale.
            const int sx0 = max(x0 - base_xi, 0);
            const int sx1 = min(x1 - base_xi, kLoglinearShmemFootX - 1);
            const int sy0 = max(y0 - base_yi, 0);
            const int sy1 = min(y1 - base_yi, kLoglinearShmemFootY - 1);
            const int sz0 = max(z0 - base_zi, 0);
            const int sz1 = min(z1 - base_zi, kLoglinearShmemFootZ - 1);

            // Diagonal trilerp values: `diags[diag_idx]` <-> full ch = 4*diag.
            float diags[kLoglinearShmemDiags];
            #pragma unroll
            for (int diag = 0; diag < kLoglinearShmemDiags; diag++) {
                float v000 = shmem_corners[sz0][sy0][sx0][diag];
                float v001 = shmem_corners[sz0][sy0][sx1][diag];
                float v010 = shmem_corners[sz0][sy1][sx0][diag];
                float v011 = shmem_corners[sz0][sy1][sx1][diag];
                float v100 = shmem_corners[sz1][sy0][sx0][diag];
                float v101 = shmem_corners[sz1][sy0][sx1][diag];
                float v110 = shmem_corners[sz1][sy1][sx0][diag];
                float v111 = shmem_corners[sz1][sy1][sx1][diag];

                float c00 = v000 * (1.0f - fx) + v001 * fx;
                float c01 = v010 * (1.0f - fx) + v011 * fx;
                float c10 = v100 * (1.0f - fx) + v101 * fx;
                float c11 = v110 * (1.0f - fx) + v111 * fx;
                float c0  = c00 * (1.0f - fy) + c01 * fy;
                float c1  = c10 * (1.0f - fy) + c11 * fy;
                diags[diag] = c0 * (1.0f - fz) + c1 * fz;
            }

            float dr = v_output[g_off + 0];
            float dg = v_output[g_off + 1];
            float db = v_output[g_off + 2];

            // Every grid gradient this pixel gives is linear in its incoming
            // one, so a zero one -- a masked pixel, mostly -- gives nothing.
            if (dr == 0.0f && dg == 0.0f && db == 0.0f) continue;

            // Original kernel iterates ci over the full 9 channels but only
            // diagonal entries (si == di) multiply by __expf(diags[si]); the
            // other 6 contribute zero to the writeback path because they're
            // never loaded (and the bilagrid value is implicitly zero for the
            // gradient chain). Match the original's accum layout exactly.
            #pragma unroll
            for (int ci = 0; ci < kLoglinearFullChans; ci++) {
                int si = ci % 3;
                int di = ci / 3;

                float r_coeff = (si == 0 ? sr : si == 1 ? sg : sb);
                float gout = (di == 0 ? dr : di == 1 ? dg : db);
                float grad_weight = r_coeff * gout;
                if (si == di) grad_weight *= __expf(diags[si]);

                accum[ci] += accum_t * grad_weight;
            }
        }
    }

    // 5. Writeback (identical to original, over all 9 channels).
    int out_idx_start = (((ni * L + zi) * H + yi) * W + xi) * kLoglinearFullChans;

    if (mult_x * mult_y == 1) {
        #pragma unroll
        for (int ci = 0; ci < kLoglinearFullChans; ci++) {
            int out_idx = out_idx_start + ci;
            if (isfinite(accum[ci]) && accum[ci] != 0.0f)
                atomicAdd(v_bilagrid + out_idx, accum[ci]);
        }
        return;
    }

    if (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0) {
        #pragma unroll
        for (int ci = 0; ci < kLoglinearFullChans; ci++) {
            int out_idx = out_idx_start + ci;
            if (isfinite(accum[ci]) && accum[ci] != 0.0f)
                atomicAdd(v_bilagrid + out_idx, accum[ci]);
        }
        return;
    }

    #pragma unroll
    for (int ci = 0; ci < kLoglinearFullChans; ci++) {
        bilagrid_bwd_v1_block_atomic_add(v_bilagrid + out_idx_start + ci, accum[ci]);
        __syncthreads();
    }
}


#ifdef PATCHED
__global__ void bilagrid_loglinear_patched_sample_backward_v1_kernel_rgb(
#else
__global__ void bilagrid_loglinear_uniform_sample_backward_v1_kernel_rgb(
#endif
    BilagridReader bilagrid,  // [N,L,H,W,9]
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
    // Clamp gz to [0,1] -- matches forward and the bilagrid-grad bwd branch.
    // Track whether gz was in range so we can zero gz_grad below (dz/dgz = 0
    // outside [0,1] -> the chain through z back to rgb is zero there).
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

    // Channel-last corner offsets: 9 channels of each corner are contiguous.
    int grid_base = g_id * L * H * W * 9;
    int off000 = grid_base + ((z0*H + y0)*W + x0) * 9;
    int off001 = grid_base + ((z0*H + y0)*W + x1) * 9;
    int off010 = grid_base + ((z0*H + y1)*W + x0) * 9;
    int off011 = grid_base + ((z0*H + y1)*W + x1) * 9;
    int off100 = grid_base + ((z1*H + y0)*W + x0) * 9;
    int off101 = grid_base + ((z1*H + y0)*W + x1) * 9;
    int off110 = grid_base + ((z1*H + y1)*W + x0) * 9;
    int off111 = grid_base + ((z1*H + y1)*W + x1) * 9;

    // accumulate bilagrid gradient over 9 channels
    float post_exp[3];
    #pragma unroll
    for (int si = 0; si < 3; si++) {
        #pragma unroll
        for (int di = 0; di < 3; di++) {
            int ci = 3 * di + si;
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
            if (si == di)
                val = __expf(val), post_exp[si] = val;
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
        // gather the corresponding bilagrid value for each of the 9 channels
        #pragma unroll
        for (int ci = 0; ci < 9; ++ci) {
            float v = bilagrid[base + ci];
            int si = ci % 3, di = ci / 3;
            if (si == di)
                v *= post_exp[si];
            float r_coeff = (si==0 ? sr : si==1 ? sg : sb);
            float gout = (di==0 ? dr : di==1 ? dg : db);
            trilerp += v * r_coeff * gout;
        }
        gz_grad += dwdz[corner] * (L-1) * trilerp;
    }
    // Zero gz_grad outside the [0,1] clamp range -- the clamp's vjp is the
    // indicator that gz_raw was strictly interior. Matches the forward's
    // gz clamp.
    if (!gz_in_range) gz_grad = 0.0f;
    vr += kC2G_r * gz_grad;
    vg += kC2G_g * gz_grad;
    vb += kC2G_b * gz_grad;
    v_rgb[g_off+0] = isfinite(vr) ? vr : 0.0f;
    v_rgb[g_off+1] = isfinite(vg) ? vg : 0.0f;
    v_rgb[g_off+2] = isfinite(vb) ? vb : 0.0f;
}

