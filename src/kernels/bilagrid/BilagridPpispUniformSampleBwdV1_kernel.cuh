#include "kernels/bilagrid/BilagridConfig.cuh"

#include "kernels/bilagrid/BilagridPpispMathBwd.cuh"

using namespace bilagrid_ppisp;

// Compile-time footprint sized for the worst case mult_x = mult_y = 1. When
// mult_x > 1 each block covers fewer cells and only the top-left subregion
// of these slots is used; the rest is dead but cheap to preload.
//
// The file is included twice from the corresponding .cu (once with PATCHED
// and once without), so guard the constexpr block to avoid redefinition.
#ifndef BILAGRID_PPISP_BWD_V1_SHMEM_CONSTANTS_DEFINED
#define BILAGRID_PPISP_BWD_V1_SHMEM_CONSTANTS_DEFINED
constexpr int kPpispShmemFootX  = (int)kBilagridBwdV1BlockX + 2;
constexpr int kPpispShmemFootY  = (int)kBilagridBwdV1BlockY + 2;
constexpr int kPpispShmemFootZ  = 3;
constexpr int kPpispShmemChans  = 9;
constexpr int kPpispShmemCells  = kPpispShmemFootX * kPpispShmemFootY * kPpispShmemFootZ;
constexpr int kPpispBlockSize   = (int)kBilagridBwdV1BlockX
                                * (int)kBilagridBwdV1BlockY
                                * (int)kBilagridBwdV1BlockZ;
#endif

#ifdef PATCHED
__global__ void bilagrid_ppisp_patched_sample_backward_v1_kernel_bilagrid(
#else
__global__ void bilagrid_ppisp_uniform_sample_backward_v1_kernel_bilagrid(
#endif
    BilagridReader bilagrid,  // [N,L,H,W,9]
#ifdef PATCHED
    const float* __restrict__ rgb_in,     // [N,m,h,w,3]
    const float* __restrict__ v_rgb_out,  // [N,m,h,w,3]
#else
    const float* __restrict__ rgb_in,     // [N,h,w,3]
    const float* __restrict__ v_rgb_out,  // [N,h,w,3]
#endif
    float* __restrict__ v_bilagrid,       // [N,L,H,W,9]
    int N, int L, int H, int W,
#ifdef PATCHED
    int m,
#endif
    int h, int w,
#ifdef PATCHED
    int h0, int w0,
    const int* __restrict__ offsets,      // [N,m,2]
#endif
    int mult_x, int mult_y
#ifdef PATCHED
    , int m_batch_stride
#endif
#ifndef PATCHED
    , const int* __restrict__ grid_indices  // [N], or nullptr -> identity
#endif
) {
    // ---- 1. Decode block-uniform (ni, zi, g_id, corner_base). ----
    //
    // blockIdx.z encodes (ni, m_batch_i, zi) in PATCHED mode or (ni, zi)
    // otherwise. All threads in a block share these values, so we decode
    // ONCE per block, before any per-thread work / early returns.
#ifdef PATCHED
    const int block_z_total = N * m_batch_stride * L;
#else
    const int block_z_total = N * L;
#endif
    if ((int)blockIdx.z >= block_z_total) return;  // block-uniform early exit

    int idx_block = (int)blockIdx.z;
    const int zi = idx_block % L;  idx_block /= L;
#ifdef PATCHED
    const int m_batch_i = idx_block % m_batch_stride;  idx_block /= m_batch_stride;
#endif
    const int ni = idx_block;

#ifndef PATCHED
    const int g_id = (grid_indices != nullptr) ? grid_indices[ni] : ni;
#else
    const int g_id = ni;
#endif
    const int corner_base = g_id * L * H * W * kPpispShmemChans;

    // ---- 2. Cooperative preload of the block's corner footprint. ----
    //
    // The block covers a (xi, yi) range of cells; the pixel loop only ever
    // reads bilagrid cells in
    //     [xi_min - 1, xi_max + 1] x [yi_min - 1, yi_max + 1] x [zi - 1, zi + 1]
    // Out-of-grid cells are clamped to the boundary, matching the original
    // kernel's `x1 = min(x0+1, W-1)` etc.
    const int base_xi = (int)((blockIdx.x * kBilagridBwdV1BlockX)) / mult_x - 1;
    const int base_yi = (int)((blockIdx.y * kBilagridBwdV1BlockY)) / mult_y - 1;
    const int base_zi = zi - 1;

    __shared__ float shmem_corners
        [kPpispShmemFootZ][kPpispShmemFootY][kPpispShmemFootX][kPpispShmemChans];

    const int linear_tid =
        (threadIdx.z * (int)kBilagridBwdV1BlockY + threadIdx.y)
        * (int)kBilagridBwdV1BlockX + threadIdx.x;

    // 300 cells / 64 threads ~= 5 cells per thread, each load is 9 contiguous
    // floats so the channel-last layout coalesces inside the operator[] path.
    #pragma unroll
    for (int local = linear_tid; local < kPpispShmemCells; local += kPpispBlockSize) {
        const int lx = local % kPpispShmemFootX;
        const int ly = (local / kPpispShmemFootX) % kPpispShmemFootY;
        const int lz = local / (kPpispShmemFootX * kPpispShmemFootY);
        const int gx = min(max(base_xi + lx, 0), W - 1);
        const int gy = min(max(base_yi + ly, 0), H - 1);
        const int gz = min(max(base_zi + lz, 0), L - 1);
        const int g_off = corner_base + ((gz * H + gy) * W + gx) * kPpispShmemChans;
        #pragma unroll
        for (int ci = 0; ci < kPpispShmemChans; ci++) {
            shmem_corners[lz][ly][lx][ci] = bilagrid[g_off + ci];
        }
    }
    __syncthreads();

    // ---- 3. Per-thread cell coords + early-out for slow path. ----
    //
    // From here on, mirrors the original kernel exactly. The "fast atomicAdd"
    // path at the bottom uses __syncthreads, so when that path is taken the
    // early-return below is guaranteed inactive (mult_x*mult_y > 1 AND
    // mult_x % BlockX == 0 imply mult_x >= BlockX so the inside-only branch
    // doesn't fire). See the original kernel's comments for the invariant.
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

    // Loop bounds (non-PATCHED hoisted; PATCHED recomputes per mi inside).
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

    float accum[kPpispShmemChans] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

    // ---- 4. Pixel loop; corner reads now go through shmem. ----
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
            float sr = rgb_in[g_off + 0];
            float sg = rgb_in[g_off + 1];
            float sb = rgb_in[g_off + 2];

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

            // Every grid gradient this pixel contributes is linear in its
            // incoming gradient, so a zero one -- a masked pixel, mostly --
            // buys nothing and skips the 9-channel interpolation below.
            float dr = v_rgb_out[g_off + 0];
            float dg = v_rgb_out[g_off + 1];
            float db = v_rgb_out[g_off + 2];
            if (dr == 0.0f && dg == 0.0f && db == 0.0f) continue;

            float fx = x - x0, fy = y - y0, fz = z - z0;

            // Shmem indices for the 8 corners. Clamp defensively: with
            // --use_fast_math, the pixel-loop's x = wi*(W-1)/(w-1) can round
            // just below the integer it equals exactly under exact arithmetic
            // (happens when wi falls on a `(xi-1)*(w-1)/(W-1)` integer
            // boundary), producing x0 = xi-2 even though the loop bound
            // assumed x0 >= xi-1. At such a boundary fx ~ 1 so the v0**
            // weights are ~0 -- the clamp introduces sub-ULP error in accum.
            const int sx0 = max(x0 - base_xi, 0);
            const int sx1 = min(x1 - base_xi, kPpispShmemFootX - 1);
            const int sy0 = max(y0 - base_yi, 0);
            const int sy1 = min(y1 - base_yi, kPpispShmemFootY - 1);
            const int sz0 = max(z0 - base_zi, 0);
            const int sz1 = min(z1 - base_zi, kPpispShmemFootZ - 1);

            float exposure_param;
            ColorPPISPParams color_params;
            #pragma unroll
            for (int ci = 0; ci < kPpispShmemChans; ci++) {
                // 8 corners from shmem (~5 ns / load vs. dequant + L1 miss).
                float v000 = shmem_corners[sz0][sy0][sx0][ci];
                float v001 = shmem_corners[sz0][sy0][sx1][ci];
                float v010 = shmem_corners[sz0][sy1][sx0][ci];
                float v011 = shmem_corners[sz0][sy1][sx1][ci];
                float v100 = shmem_corners[sz1][sy0][sx0][ci];
                float v101 = shmem_corners[sz1][sy0][sx1][ci];
                float v110 = shmem_corners[sz1][sy1][sx0][ci];
                float v111 = shmem_corners[sz1][sy1][sx1][ci];

                float c00 = v000 * (1.0f - fx) + v001 * fx;
                float c01 = v010 * (1.0f - fx) + v011 * fx;
                float c10 = v100 * (1.0f - fx) + v101 * fx;
                float c11 = v110 * (1.0f - fx) + v111 * fx;
                float c0  = c00 * (1.0f - fy) + c01 * fy;
                float c1  = c10 * (1.0f - fy) + c11 * fy;
                float val = c0 * (1.0f - fz) + c1 * fz;

                (ci == 0 ? exposure_param :
                    ci == 1 ? color_params.b.x :
                    ci == 2 ? color_params.b.y :
                    ci == 3 ? color_params.r.x :
                    ci == 4 ? color_params.r.y :
                    ci == 5 ? color_params.g.x :
                    ci == 6 ? color_params.g.y :
                    ci == 7 ? color_params.n.x :
                    color_params.n.y) = val;
            }

            float3 rgb = {sr, sg, sb};
            float3 grad_rgb = {dr, dg, db};
            float grad_exposure_param = 0.0f;
            ColorPPISPParams grad_color_params = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
            float3 rgb_after_exp;
            apply_exposure(rgb, exposure_param, rgb_after_exp);
            apply_color_correction_ppisp_bwd(rgb_after_exp, &color_params, grad_rgb, grad_rgb, &grad_color_params);
            apply_exposure_bwd(rgb, exposure_param, grad_rgb, grad_rgb, grad_exposure_param);

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

            #pragma unroll
            for (int ci = 0; ci < kPpispShmemChans; ci++) {
                float grad_weight = (ci == 0 ? grad_exposure_param :
                    ci == 1 ? grad_color_params.b.x :
                    ci == 2 ? grad_color_params.b.y :
                    ci == 3 ? grad_color_params.r.x :
                    ci == 4 ? grad_color_params.r.y :
                    ci == 5 ? grad_color_params.g.x :
                    ci == 6 ? grad_color_params.g.y :
                    ci == 7 ? grad_color_params.n.x :
                    grad_color_params.n.y);
                accum[ci] += accum_t * grad_weight;
            }
        }
    }

    // ---- 5. Writeback: identical to the original kernel. ----
    int out_idx_start = (((ni * L + zi) * H + yi) * W + xi) * kPpispShmemChans;

    if (mult_x * mult_y == 1) {
        #pragma unroll
        for (int ci = 0; ci < kPpispShmemChans; ci++) {
            int out_idx = out_idx_start + ci;
            if (isfinite(accum[ci]) && accum[ci] != 0.0f)
                atomicAdd(v_bilagrid + out_idx, accum[ci]);
        }
        return;
    }

    if (mult_x % kBilagridBwdV1BlockX != 0 || mult_y % kBilagridBwdV1BlockY != 0) {
        #pragma unroll
        for (int ci = 0; ci < kPpispShmemChans; ci++) {
            int out_idx = out_idx_start + ci;
            if (isfinite(accum[ci]) && accum[ci] != 0.0f)
                atomicAdd(v_bilagrid + out_idx, accum[ci]);
        }
        return;
    }

    // fast atomicAdd: shared helper from BilagridBwdV1BlockReduce.cuh (now
    // safe to call -- the preload's __syncthreads above is the only earlier
    // sync, and all 64 threads are guaranteed to be live in this branch).
    #pragma unroll
    for (int ci = 0; ci < kPpispShmemChans; ci++) {
        bilagrid_bwd_v1_block_atomic_add(v_bilagrid + out_idx_start + ci, accum[ci]);
        __syncthreads();
    }
}


#ifdef PATCHED
__global__ void bilagrid_ppisp_patched_sample_backward_v1_kernel_rgb(
#else
__global__ void bilagrid_ppisp_uniform_sample_backward_v1_kernel_rgb(
#endif
    BilagridReader bilagrid,  // [N,L,H,W,9]
#ifdef PATCHED
    const float* __restrict__ rgb_in,  // [N,m,h,w,3]
    const float* __restrict__ v_rgb_out,  // [N,m,h,w,3]
    float* __restrict__ v_rgb_in,  // [N,m,h,w,3]
#else
    const float* __restrict__ rgb_in,  // [N,h,w,3]
    const float* __restrict__ v_rgb_out,  // [N,h,w,3]
    float* __restrict__ v_rgb_in,  // [N,h,w,3]
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
    float sr = rgb_in[g_off+0];
    float sg = rgb_in[g_off+1];
    float sb = rgb_in[g_off+2];
    float dr = v_rgb_out[g_off+0];
    float dg = v_rgb_out[g_off+1];
    float db = v_rgb_out[g_off+2];

    // The image gradient is linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- is zero out and skips 72 corner loads.
    if (dr == 0.0f && dg == 0.0f && db == 0.0f) {
        v_rgb_in[g_off+0] = 0.0f;
        v_rgb_in[g_off+1] = 0.0f;
        v_rgb_in[g_off+2] = 0.0f;
        return;
    }

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

    float fx = x-x0, fy = y-y0, fz = z-z0;

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

    // load PPISP params
    float exposure_param;
    ColorPPISPParams color_params;
    #pragma unroll
    for (int ci = 0; ci < 9; ci++) {
        // fetch 8 corners
        auto v000 = bilagrid[off000 + ci];
        auto v001 = bilagrid[off001 + ci];
        auto v010 = bilagrid[off010 + ci];
        auto v011 = bilagrid[off011 + ci];
        auto v100 = bilagrid[off100 + ci];
        auto v101 = bilagrid[off101 + ci];
        auto v110 = bilagrid[off110 + ci];
        auto v111 = bilagrid[off111 + ci];

        // trilinear interp
        float c00 = v000*(1.0f-fx) + v001*fx;
        float c01 = v010*(1.0f-fx) + v011*fx;
        float c10 = v100*(1.0f-fx) + v101*fx;
        float c11 = v110*(1.0f-fx) + v111*fx;
        float c0 = c00*(1.0f-fy) + c01*fy;
        float c1 = c10*(1.0f-fy) + c11*fy;
        float val = c0*(1.0f-fz) + c1*fz;

        (ci == 0 ? exposure_param :
            ci == 1 ? color_params.b.x :
            ci == 2 ? color_params.b.y :
            ci == 3 ? color_params.r.x :
            ci == 4 ? color_params.r.y :
            ci == 5 ? color_params.g.x :
            ci == 6 ? color_params.g.y :
            ci == 7 ? color_params.n.x :
            color_params.n.y) = val;
    }

    // apply PPISP
    float3 rgb = {sr, sg, sb};
    float3 grad_rgb = {dr, dg, db};
    float grad_exposure_param = 0.0f;
    ColorPPISPParams grad_color_params = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
    float3 rgb_after_exp;
    apply_exposure(rgb, exposure_param, rgb_after_exp);
    apply_color_correction_ppisp_bwd(rgb_after_exp, &color_params, grad_rgb, grad_rgb, &grad_color_params);
    apply_exposure_bwd(rgb, exposure_param, grad_rgb, grad_rgb, grad_exposure_param);

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
            trilerp += v * (ci == 0 ? grad_exposure_param :
                ci == 1 ? grad_color_params.b.x :
                ci == 2 ? grad_color_params.b.y :
                ci == 3 ? grad_color_params.r.x :
                ci == 4 ? grad_color_params.r.y :
                ci == 5 ? grad_color_params.g.x :
                ci == 6 ? grad_color_params.g.y :
                ci == 7 ? grad_color_params.n.x :
                grad_color_params.n.y);
        }
        gz_grad += dwdz[corner] * (L-1) * trilerp;
    }
    // Zero gz_grad outside the [0,1] clamp range -- the clamp's vjp.
    if (!gz_in_range) gz_grad = 0.0f;
    grad_rgb.x += kC2G_r * gz_grad;
    grad_rgb.y += kC2G_g * gz_grad;
    grad_rgb.z += kC2G_b * gz_grad;

    v_rgb_in[g_off+0] = isfinite(grad_rgb.x) ? grad_rgb.x : 0.0f;
    v_rgb_in[g_off+1] = isfinite(grad_rgb.y) ? grad_rgb.y : 0.0f;
    v_rgb_in[g_off+2] = isfinite(grad_rgb.z) ? grad_rgb.z : 0.0f;
}

