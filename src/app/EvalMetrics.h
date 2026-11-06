#pragma once

// Image metrics for the eval pass. All host-side, all on [0,1] float RGB in
// HWC order, matching torchmetrics. LPIPS has a model behind it and is
// src/lpips/, which the eval reaches through TrainerCore's LpipsHooks.

#include <cstdint>
#include <vector>

namespace spirula {

// Masks have one nonzero/zero byte per pixel; C maps channels to that pixel.
float image_l1(const float* a, const float* b, int64_t n,
               const uint8_t* mask = nullptr, int C = 3);

// Peak signal-to-noise ratio, data_range = 1 (torchmetrics PeakSignalNoiseRatio).
float image_psnr(const float* a, const float* b, int64_t n,
                 const uint8_t* mask = nullptr, int C = 3);

// SSIM: 11x11 Gaussian (sigma 1.5), reflect padding, full-image average.
// A mask normalizes window statistics by valid support and excludes masked centers.
float image_ssim(const float* a, const float* b, int H, int W, int C,
                 const uint8_t* mask = nullptr);

// Iterated quadratic least-squares color correction, excluding saturated pixels.
// Both inputs are [H*W*C] in [0,1]; the result is clipped to [0,1].
std::vector<float> color_correct(const float* img, const float* ref,
                                 int64_t num_pixels, int C,
                                 int num_iters = 5, float eps = 0.5f / 255.0f);

// Same, writing into a caller-owned buffer. Scoring a 4K view otherwise spends
// more time faulting in fresh scratch than doing the arithmetic, so the eval
// pass reuses one buffer per worker across views.
void color_correct_into(const float* img, const float* ref,
                        int64_t num_pixels, int C, std::vector<float>& out,
                        int num_iters = 5, float eps = 0.5f / 255.0f,
                        const uint8_t* mask = nullptr);

}  // namespace spirula
