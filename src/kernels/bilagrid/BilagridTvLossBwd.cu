#include "kernels/bilagrid/BilagridConfig.cuh"

template<int C, bool inplace>
__global__ void tv_loss_backward_kernel(
    BilagridReader bilagrid,   // [N,L,H,W,C] (channel-last)
    const float v_tv_loss,                   // scalar gradient dL/d(tv_loss)
    float* __restrict__ v_bilagrid,     // [N,L,H,W,C]
    int N, int L, int H, int W, int nl_per_row
) {
    int nl_block, w_block, h_block;
    bilagrid_tv_block_ids(nl_per_row, W, nl_block, w_block, h_block);
    int wi = w_block * blockDim.x + threadIdx.x;
    int hi = h_block * blockDim.y + threadIdx.y;
    int idx = nl_block * blockDim.z + threadIdx.z;
    if (wi >= W || hi >= H || idx >= (N * L)) return;

    int li = idx % L; idx /= L;
    int ni = idx;

    float s = v_tv_loss / (6*N);
    float sx = s / (float)(L * H * (W - 1));
    float sy = s / (float)(L * (H - 1) * W);
    float sz = s / (float)((L - 1) * H * W);

    // Channel-last strides.
    const int sw = C;
    const int sh = W * C;
    const int sl = H * W * C;
    const int sn = L * H * W * C;

    const int64_t cell_base =
        (int64_t)ni * sn + li * sl + hi * sh + wi * sw;

    for (int ci = 0; ci < C; ci++) {

        int64_t cell_idx = cell_base + ci;

        float half_grad = 0.0f;
        float val = bilagrid[cell_idx];

        if (wi > 0) {
            float val0 = bilagrid[cell_idx - sw];
            half_grad += (val - val0) * sx;
        }
        if (wi < W - 1) {
            float val0 = bilagrid[cell_idx + sw];
            half_grad += (val - val0) * sx;
        }
        if (hi > 0) {
            float val0 = bilagrid[cell_idx - sh];
            half_grad += (val - val0) * sy;
        }
        if (hi < H - 1) {
            float val0 = bilagrid[cell_idx + sh];
            half_grad += (val - val0) * sy;
        }
        if (li > 0) {
            float val0 = bilagrid[cell_idx - sl];
            half_grad += (val - val0) * sz;
        }
        if (li < L - 1) {
            float val0 = bilagrid[cell_idx + sl];
            half_grad += (val - val0) * sz;
        }

        if (inplace)
            v_bilagrid[cell_idx] += half_grad;
        else
            v_bilagrid[cell_idx] = half_grad;
    }
}


void tv_loss_backward(
    BilagridReader bilagrid,
    const float v_tv_loss,
    float* v_bilagrid,
    int N, int C, int L, int H, int W, bool inplace,
    cudaStream_t stream
) {
    // TODO: optimize memory access pattern
    dim3 block(4, 4, 4);
    BilagridTvGrid g = bilagrid_tv_grid(N, L, H, W);
    dim3 grid(g.gx, g.gy, g.gz);
    if (C == 12)
        (inplace ? tv_loss_backward_kernel<12, true> : tv_loss_backward_kernel<12, false>)
        <<<grid, block, 0, stream>>>(
            bilagrid, v_tv_loss, v_bilagrid, N, L, H, W, g.nl_per_row
        );
    else if (C == 9)
        (inplace ? tv_loss_backward_kernel<9, true> : tv_loss_backward_kernel<9, false>)
        <<<grid, block, 0, stream>>>(
            bilagrid, v_tv_loss, v_bilagrid, N, L, H, W, g.nl_per_row
        );
    else if (C == 2)
        (inplace ? tv_loss_backward_kernel<2, true> : tv_loss_backward_kernel<2, false>)
        <<<grid, block, 0, stream>>>(
            bilagrid, v_tv_loss, v_bilagrid, N, L, H, W, g.nl_per_row
        );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



template<int C, bool inplace>
__global__ void channel_mean_backward_kernel(
    const float* __restrict__ v_channel_mean,    // [C]
    float* __restrict__ v_bilagrid,     // [N,L,H,W,C] (channel-last)
    int N, int L, int H, int W, int nl_per_row
) {
    int nl_block, w_block, h_block;
    bilagrid_tv_block_ids(nl_per_row, W, nl_block, w_block, h_block);
    int wi = w_block * blockDim.x + threadIdx.x;
    int hi = h_block * blockDim.y + threadIdx.y;
    int idx = nl_block * blockDim.z + threadIdx.z;
    if (wi >= W || hi >= H || idx >= (N * L)) return;

    int li = idx % L; idx /= L;
    int ni = idx;

    const int64_t cell_base =
        ((((int64_t)ni * L + li) * H + hi) * W + wi) * C;

    #pragma unroll
    for (int ci = 0; ci < C; ci++) {

        float grad = v_channel_mean[ci] / (N*L*H*W);

        int64_t cell_idx = cell_base + ci;
        if (inplace)
            v_bilagrid[cell_idx] += grad;
        else
            v_bilagrid[cell_idx] = grad;
    }
}


void channel_mean_backward(
    [[maybe_unused]] BilagridReader bilagrid,  // the Vulkan side reads it
    const float* v_tv_loss,
    float* v_bilagrid,
    int N, int C, int L, int H, int W, bool inplace,
    cudaStream_t stream
) {
    // TODO: optimize memory access pattern
    dim3 block(4, 4, 4);
    BilagridTvGrid g = bilagrid_tv_grid(N, L, H, W);
    dim3 grid(g.gx, g.gy, g.gz);
    if (C == 12)
        (inplace ? channel_mean_backward_kernel<12, true> : channel_mean_backward_kernel<12, false>)
        <<<grid, block, 0, stream>>>(
            v_tv_loss, v_bilagrid, N, L, H, W, g.nl_per_row
        );
    else if (C == 9)
        (inplace ? channel_mean_backward_kernel<9, true> : channel_mean_backward_kernel<9, false>)
        <<<grid, block, 0, stream>>>(
            v_tv_loss, v_bilagrid, N, L, H, W, g.nl_per_row
        );
    else if (C == 2)
        (inplace ? channel_mean_backward_kernel<2, true> : channel_mean_backward_kernel<2, false>)
        <<<grid, block, 0, stream>>>(
            v_tv_loss, v_bilagrid, N, L, H, W, g.nl_per_row
        );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
