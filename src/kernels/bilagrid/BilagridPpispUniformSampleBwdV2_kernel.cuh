#include "kernels/bilagrid/BilagridConfig.cuh"

#include "kernels/bilagrid/BilagridPpispMathBwd.cuh"

using namespace bilagrid_ppisp;

// PPISP bilagrid backward, v2 = SCATTER (thread per image pixel), the
// alternative to the v1 GATHER (thread per grid cell). See
// BilagridBackwardSelection.md.
//
// v2 fuses the grid-gradient and image-gradient passes that v1 splits into two
// kernels: it reuses the exact per-pixel transform-backward of the v1 `_rgb`
// kernel (interpolate the 9 PPISP params from the 8 corners -> apply_exposure /
// apply_color_correction_ppisp_bwd -> grad params + grad rgb), and additionally
// SCATTERS the per-channel param gradient to the 8 trilinear corners with an
// atomicAdd (v1's `_grid` kernel instead GATHERS the same sum per cell). The two
// are mathematically identical up to atomic summation order.
//
// In-place aliasing note: the engine passes v_rgb_out and v_rgb_in as the SAME
// buffer. Safe here -- each thread reads its own pixel's upstream grad at the
// top and writes its own pixel's input grad at the end; no cross-thread v_rgb
// dependency, and the grid values (bilagrid) and grid grad (v_bilagrid) are
// distinct buffers.
//
// NEEDS_IMAGE_GRAD folds out the image-gradient write for GT-side grids
// (depth/normal pass v_rgb_in == nullptr); for RGB PPISP it is always true.
template <bool NEEDS_IMAGE_GRAD>
__global__ void bilagrid_ppisp_uniform_sample_backward_v2_kernel(
    BilagridReader bilagrid,               // [N,L,H,W,9]
    const float* __restrict__ rgb_in,      // [N,h,w,3]
    const float* __restrict__ v_rgb_out,   // [N,h,w,3]
    float* __restrict__ v_bilagrid,        // [N,L,H,W,9]  (accumulated)
    float* __restrict__ v_rgb_in,          // [N,h,w,3] or null
    int N, int L, int H, int W,
    int h, int w,
    const int* __restrict__ grid_indices   // [N], or nullptr -> identity
) {
    int idx = blockIdx.x * kBilagridBwdV1RgbThreads + threadIdx.x;
    int total = N * h * w;
    if (idx >= total) return;

    int tmp = idx;
    int wi = tmp % w; tmp /= w;
    int hi = tmp % h; tmp /= h;
    int ni = tmp;
    int g_id = grid_indices ? grid_indices[ni] : ni;

    int g_off = ((ni * h + hi) * w + wi) * 3;
    float sr = rgb_in[g_off + 0];
    float sg = rgb_in[g_off + 1];
    float sb = rgb_in[g_off + 2];
    float dr = v_rgb_out[g_off + 0];
    float dg = v_rgb_out[g_off + 1];
    float db = v_rgb_out[g_off + 2];

    // Both gradients this thread produces are linear in the incoming one, so
    // a zero one -- a masked pixel, mostly -- contributes nothing anywhere.
    if (dr == 0.0f && dg == 0.0f && db == 0.0f) {
        if (NEEDS_IMAGE_GRAD && v_rgb_in != nullptr) {
            v_rgb_in[g_off + 0] = 0.0f;
            v_rgb_in[g_off + 1] = 0.0f;
            v_rgb_in[g_off + 2] = 0.0f;
        }
        return;
    }

    // grid coords (identical to v1)
    float x = (float)wi / (float)(w - 1) * (float)(W - 1);
    float y = (float)hi / (float)(h - 1) * (float)(H - 1);
    // Clamp gz to [0,1] -- matches forward and v1.
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = fminf(fmaxf(gz_raw, 0.0f), 1.0f);
    bool  gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * (L - 1);
    int x0 = floorf(x), y0 = floorf(y), z0 = floorf(z);
    int x1 = min(x0 + 1, W - 1);
    int y1 = min(y0 + 1, H - 1);
    int z1 = min(z0 + 1, L - 1);

    float fx = x - x0, fy = y - y0, fz = z - z0;

    // Channel-last CELL offsets (9 channels per corner contiguous), WITHOUT a
    // grid base. Grid VALUES are read from the g_id slot; the grid GRADIENT is
    // scattered into the per-batch buffer indexed by ni (matches v1's split:
    // reads use g_id, writes use ni). With grid_indices these differ, and the
    // grad buffer is only [C_batch, ...] -- scattering to g_id would be OOB.
    const int grid_base = g_id * L * H * W * 9;
    const int out_base  = ni  * L * H * W * 9;
    const int corner_cell[8] = {
        ((z0 * H + y0) * W + x0) * 9,
        ((z0 * H + y0) * W + x1) * 9,
        ((z0 * H + y1) * W + x0) * 9,
        ((z0 * H + y1) * W + x1) * 9,
        ((z1 * H + y0) * W + x0) * 9,
        ((z1 * H + y0) * W + x1) * 9,
        ((z1 * H + y1) * W + x0) * 9,
        ((z1 * H + y1) * W + x1) * 9,
    };

    // load PPISP params (trilinear interpolation of the 9 channels)
    float exposure_param;
    ColorPPISPParams color_params;
    #pragma unroll
    for (int ci = 0; ci < 9; ci++) {
        auto v000 = bilagrid[grid_base + corner_cell[0] + ci];
        auto v001 = bilagrid[grid_base + corner_cell[1] + ci];
        auto v010 = bilagrid[grid_base + corner_cell[2] + ci];
        auto v011 = bilagrid[grid_base + corner_cell[3] + ci];
        auto v100 = bilagrid[grid_base + corner_cell[4] + ci];
        auto v101 = bilagrid[grid_base + corner_cell[5] + ci];
        auto v110 = bilagrid[grid_base + corner_cell[6] + ci];
        auto v111 = bilagrid[grid_base + corner_cell[7] + ci];

        float c00 = v000 * (1.0f - fx) + v001 * fx;
        float c01 = v010 * (1.0f - fx) + v011 * fx;
        float c10 = v100 * (1.0f - fx) + v101 * fx;
        float c11 = v110 * (1.0f - fx) + v111 * fx;
        float c0 = c00 * (1.0f - fy) + c01 * fy;
        float c1 = c10 * (1.0f - fy) + c11 * fy;
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

    // apply PPISP backward (identical to v1 `_rgb`)
    float3 rgb = {sr, sg, sb};
    float3 grad_rgb = {dr, dg, db};
    float grad_exposure_param = 0.0f;
    ColorPPISPParams grad_color_params = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
    float3 rgb_after_exp;
    apply_exposure(rgb, exposure_param, rgb_after_exp);
    apply_color_correction_ppisp_bwd(rgb_after_exp, &color_params, grad_rgb, grad_rgb, &grad_color_params);
    apply_exposure_bwd(rgb, exposure_param, grad_rgb, grad_rgb, grad_exposure_param);

    // param gradient per grid channel (== v1 `_grid`'s grad_weight)
    const float grad_weight[9] = {
        grad_exposure_param,
        grad_color_params.b.x, grad_color_params.b.y,
        grad_color_params.r.x, grad_color_params.r.y,
        grad_color_params.g.x, grad_color_params.g.y,
        grad_color_params.n.x, grad_color_params.n.y,
    };

    // trilinear corner weights (array index bits: x=c&1, y=(c>>1)&1, z=(c>>2)&1)
    const float wcx[2] = {1.0f - fx, fx};
    const float wcy[2] = {1.0f - fy, fy};
    const float wcz[2] = {1.0f - fz, fz};

    // SCATTER param grad to the 8 corners + accumulate the gz (color->z) chain.
    float gz_grad = 0.0f;
    #pragma unroll
    for (int corner = 0; corner < 8; ++corner) {
        int cx = corner & 1, cy = (corner >> 1) & 1, cz = (corner >> 2) & 1;
        float wc = wcx[cx] * wcy[cy] * wcz[cz];
        // d(weight)/dz: swap the z factor for +-1 (see v1 `_rgb` dwdz).
        float dwdz = wcx[cx] * wcy[cy] * (cz ? 1.0f : -1.0f);
        int rbase = grid_base + corner_cell[corner];  // read grid values
        int wbase = out_base + corner_cell[corner];   // scatter grad (per-batch)
        float trilerp = 0.0f;
        #pragma unroll
        for (int ci = 0; ci < 9; ++ci) {
            float gw = grad_weight[ci];
            if (NEEDS_IMAGE_GRAD)
                trilerp += bilagrid[rbase + ci] * gw;
            float aa = wc * gw;
            if (aa != 0.0f && isfinite(aa))
                atomicAdd(v_bilagrid + wbase + ci, aa);
        }
        if (NEEDS_IMAGE_GRAD)
            gz_grad += dwdz * (L - 1) * trilerp;
    }

    if (NEEDS_IMAGE_GRAD) {
        // Zero gz_grad outside the [0,1] clamp range -- the clamp's vjp.
        if (!gz_in_range) gz_grad = 0.0f;
        grad_rgb.x += kC2G_r * gz_grad;
        grad_rgb.y += kC2G_g * gz_grad;
        grad_rgb.z += kC2G_b * gz_grad;

        v_rgb_in[g_off + 0] = isfinite(grad_rgb.x) ? grad_rgb.x : 0.0f;
        v_rgb_in[g_off + 1] = isfinite(grad_rgb.y) ? grad_rgb.y : 0.0f;
        v_rgb_in[g_off + 2] = isfinite(grad_rgb.z) ? grad_rgb.z : 0.0f;
    }
}
