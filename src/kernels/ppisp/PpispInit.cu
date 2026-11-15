// PPISP parameter default initialization (per-camera tables).
//
// "rqs"      -> all 39 zeros (the loglinear-style parameterization's identity).
// "original" -> 24 zeros, then 3x (a, a, b, 0) with a = 0.013658988289535046,
//              b = 0.37816452980041504 (the reference implementation's
//              DEFAULT_PPISP_PARAMS).

#include <cuda_runtime.h>
#include <cstdint>

#include "kernels/bilagrid/BilagridConfig.cuh"


__global__ void ppisp_original_default_init_kernel(
    float* __restrict__ params,   // [N, 36]
    int N
) {
    int bid = blockIdx.x * blockDim.x + threadIdx.x;
    int pid = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= N || pid >= 36) return;

    constexpr float a = 0.013658988289535046f;
    constexpr float b = 0.37816452980041504f;
    float v;
    if (pid < 24) {
        v = 0.0f;
    } else {
        // 24..35: three groups of (a, a, b, 0)
        int j = (pid - 24) % 4;
        v = (j == 0 || j == 1) ? a : (j == 2 ? b : 0.0f);
    }
    params[bid * 36 + pid] = v;
}


void ppisp_original_default_init(
    float* params, int N, cudaStream_t stream
) {
    if (N <= 0) return;
    dim3 block(16, 16, 1);
    dim3 bounds((N + 15) / 16, (36 + 15) / 16, 1);
    ppisp_original_default_init_kernel<<<bounds, block, 0, stream>>>(params, N);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


__global__ void ppisp_add_into_grad_kernel(
    const float* __restrict__ src,
    float* __restrict__ dst,
    float dst_scale,
    size_t n
) {
    size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    dst[i] = dst[i] * dst_scale + (src ? src[i] : 0.0f);
}


// Elementwise dst = dst * dst_scale + src over n floats; a null src adds nothing.
void ppisp_add_into_grad(
    const float* src, float* dst, float dst_scale, size_t n, cudaStream_t stream
) {
    if (n == 0) return;
    const int BLK = 256;
    int blocks = (int)((n + BLK - 1) / BLK);
    ppisp_add_into_grad_kernel<<<blocks, BLK, 0, stream>>>(src, dst, dst_scale, n);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


__global__ void bilagrid_scatter_floats_kernel(
    const float* __restrict__ src,    // [n]
    const int* __restrict__ indices,  // [n]
    int n,
    float* __restrict__ dst           // [N_full]
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    dst[indices[i]] = src[i];
}


// Scatter src[i] into dst[indices[i]] for i in [0, n). When n is large enough
// for two i,j to have the same index (collision), the last writer wins, which
// is the desired behavior for "set the per-camera scalar for the cameras seen
// in this batch."
void bilagrid_scatter_floats(
    const float* src, const int* indices, int n,
    float* dst, cudaStream_t stream
) {
    if (n <= 0) return;
    const int BLK = 128;
    int blocks = (n + BLK - 1) / BLK;
    bilagrid_scatter_floats_kernel<<<blocks, BLK, 0, stream>>>(src, indices, n, dst);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
