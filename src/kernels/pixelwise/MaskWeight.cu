// MaskWeight.cu -- a mask carrying per-pixel weights (0..255, focus/ times
// masks/): the strict 0/1 mask every other consumer reads, and the weight
// applied to a per-pixel buffer. docs/notes/focus-weights.md.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/PixelWiseCommon.cuh"


__global__ void split_mask_weight_kernel(const uint8_t* weight, uint8_t* mask, int64_t n) {
    const int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        mask[i] = weight[i] != 0 ? 1 : 0;
}

// Nearest lookup for a weight stored at its own size, as FusedSSIM's mask fetch.
__global__ void scale_by_mask_weight_kernel(
    float* data, const uint8_t* weight,
    int H, int W, int C, int Hm, int Wm
) {
    const unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned b = blockIdx.y;
    if (gid >= (unsigned)(H * W))
        return;
    const int y = (int)(gid / W), x = (int)(gid % W);
    int ys = y, xs = x;
    if (Hm != H || Wm != W) {
        const float u = ((float)x + 0.5f) * (float)Wm / (float)W - 0.5f;
        const float v = ((float)y + 0.5f) * (float)Hm / (float)H - 0.5f;
        xs = max(0, min(Wm - 1, (int)floorf(u + 0.5f)));
        ys = max(0, min(Hm - 1, (int)floorf(v + 0.5f)));
    }
    const float w = (float)weight[((size_t)b * Hm + ys) * Wm + xs] * (1.0f / 255.0f);
    float* p = data + (((size_t)b * H + y) * W + x) * C;
    for (int c = 0; c < C; ++c)
        p[c] *= w;
}


/*[AutoHeaderGeneratorExport]*/
void split_mask_weight(
    TorchTensorView weight,  // [B, H, W, 1] uint8, 0..255
    TorchTensorView mask     // [B, H, W, 1] uint8, out: weight != 0
) {
    const auto& s = std::get<2>(weight);
    const int64_t n = s[0] * s[1] * s[2];
    if (n <= 0)
        return;
    split_mask_weight_kernel<<<(unsigned)((n + 255) / 256), 256>>>(
        (const uint8_t*)std::get<0>(weight), (uint8_t*)std::get<0>(mask), n);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void scale_by_mask_weight(
    TorchTensorView data,    // [B, H, W] or [B, H, W, C] float, scaled in place
    TorchTensorView weight   // [B, Hm, Wm, 1] uint8, 0..255
) {
    const auto& s = std::get<2>(data);
    const auto& m = std::get<2>(weight);
    const int B = (int)s[0], H = (int)s[1], W = (int)s[2];
    const int C = s.size() > 3 ? (int)s[3] : 1;
    if (B <= 0 || H <= 0 || W <= 0)
        return;
    scale_by_mask_weight_kernel<<<_LAUNCH_ARGS_2D(H * W, B, 256, 1)>>>(
        (float*)std::get<0>(data), (const uint8_t*)std::get<0>(weight),
        H, W, C, (int)m[1], (int)m[2]);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
