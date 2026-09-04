#include "kernels/bilagrid/BilagridConfig.cuh"

// Log-linear bilagrid backward v2 = thread-per-pixel SCATTER (fuses grid- and
// image-gradient). 9 channels = 3 out x 3 in (ci = 3*di + si); the diagonal
// (si == di) channels pass through exp(). Reuses the v1 `_rgb` transform-
// backward and additionally scatters w_corner * grad_weight to the 8 corners.
// Reads grid values from the g_id slot; scatters grad into the per-batch buffer
// indexed by ni. See BilagridBackwardSelection.md and
// BilagridLoglinearSampleBwdV1_kernel.cuh.
__global__ void bilagrid_loglinear_uniform_sample_backward_v2_kernel(
    BilagridReader bilagrid,               // [N_grids,L,H,W,9]
    const float* __restrict__ rgb_in,      // [N,h,w,3]
    const float* __restrict__ v_rgb_out,   // [N,h,w,3]
    float* __restrict__ v_bilagrid,        // [N,L,H,W,9] (per-batch, accumulated)
    float* __restrict__ v_rgb_in,          // [N,h,w,3]
    int N, int L, int H, int W, int h, int w,
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

    float sr = rgb_in[g_off + 0], sg = rgb_in[g_off + 1], sb = rgb_in[g_off + 2];
    float dr = v_rgb_out[g_off + 0], dg = v_rgb_out[g_off + 1], db = v_rgb_out[g_off + 2];

    // Both gradients are linear in the incoming one, so a zero one -- a
    // masked pixel, mostly -- contributes nothing anywhere.
    if (dr == 0.0f && dg == 0.0f && db == 0.0f) {
        if (v_rgb_in != nullptr) {
            v_rgb_in[g_off + 0] = 0.0f;
            v_rgb_in[g_off + 1] = 0.0f;
            v_rgb_in[g_off + 2] = 0.0f;
        }
        return;
    }

    float x = (float)wi / (float)(w - 1) * (float)(W - 1);
    float y = (float)hi / (float)(h - 1) * (float)(H - 1);
    float gz_raw = kC2G_r * sr + kC2G_g * sg + kC2G_b * sb;
    float gz = fminf(fmaxf(gz_raw, 0.0f), 1.0f);
    bool gz_in_range = (gz_raw >= 0.0f && gz_raw <= 1.0f);
    float z = gz * (L - 1);
    int x0 = floorf(x), y0 = floorf(y), z0 = floorf(z);
    int x1 = min(x0 + 1, W - 1);
    int y1 = min(y0 + 1, H - 1);
    int z1 = min(z0 + 1, L - 1);
    float fx = x - x0, fy = y - y0, fz = z - z0;

    const float ww[8] = {
        (1 - fx) * (1 - fy) * (1 - fz), fx * (1 - fy) * (1 - fz),
        (1 - fx) * fy * (1 - fz),       fx * fy * (1 - fz),
        (1 - fx) * (1 - fy) * fz,       fx * (1 - fy) * fz,
        (1 - fx) * fy * fz,             fx * fy * fz,
    };
    const float dwdz8[8] = {
        -(1 - fx) * (1 - fy), -fx * (1 - fy), -(1 - fx) * fy, -fx * fy,
         (1 - fx) * (1 - fy),  fx * (1 - fy),  (1 - fx) * fy,  fx * fy,
    };
    const int cc[8] = {
        ((z0 * H + y0) * W + x0) * 9, ((z0 * H + y0) * W + x1) * 9,
        ((z0 * H + y1) * W + x0) * 9, ((z0 * H + y1) * W + x1) * 9,
        ((z1 * H + y0) * W + x0) * 9, ((z1 * H + y0) * W + x1) * 9,
        ((z1 * H + y1) * W + x0) * 9, ((z1 * H + y1) * W + x1) * 9,
    };
    int grid_base = g_id * L * H * W * 9;
    int out_base  = ni  * L * H * W * 9;

    const float coeff[3] = {sr, sg, sb};
    const float dcol[3] = {dr, dg, db};

    // interpolate params (read g_id slot), apply exp on the diagonal, and build
    // the image gradient v[si] + the per-channel param gradient grad_weight[ci].
    float v[3] = {0.0f, 0.0f, 0.0f};
    float post_exp[3];
    float grad_weight[9];
    #pragma unroll
    for (int ci = 0; ci < 9; ci++) {
        int si = ci % 3, di = ci / 3;
        float val = 0.0f;
        #pragma unroll
        for (int corner = 0; corner < 8; corner++)
            val += bilagrid[grid_base + cc[corner] + ci] * ww[corner];
        if (si == di) { val = __expf(val); post_exp[si] = val; }
        v[si] += val * dcol[di];
        grad_weight[ci] = coeff[si] * dcol[di];
        if (si == di) grad_weight[ci] *= post_exp[si];
    }

    // scatter param grad to the 8 corners (per-batch ni) + gz (color->z) chain.
    float gz_grad = 0.0f;
    #pragma unroll
    for (int corner = 0; corner < 8; corner++) {
        int rbase = grid_base + cc[corner];
        int wbase = out_base + cc[corner];
        float trilerp = 0.0f;
        #pragma unroll
        for (int ci = 0; ci < 9; ci++) {
            float gw = grad_weight[ci];
            trilerp += bilagrid[rbase + ci] * gw;  // gw already carries post_exp
            float aa = ww[corner] * gw;
            if (aa != 0.0f && isfinite(aa)) atomicAdd(v_bilagrid + wbase + ci, aa);
        }
        gz_grad += dwdz8[corner] * (L - 1) * trilerp;
    }

    if (!gz_in_range) gz_grad = 0.0f;
    float vr = v[0] + kC2G_r * gz_grad;
    float vg = v[1] + kC2G_g * gz_grad;
    float vb = v[2] + kC2G_b * gz_grad;
    v_rgb_in[g_off + 0] = isfinite(vr) ? vr : 0.0f;
    v_rgb_in[g_off + 1] = isfinite(vg) ? vg : 0.0f;
    v_rgb_in[g_off + 2] = isfinite(vb) ? vb : 0.0f;
}
