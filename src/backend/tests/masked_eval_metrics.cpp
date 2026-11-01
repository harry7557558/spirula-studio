#include "app/EvalMetrics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void ignored_pixels() {
    constexpr int H = 24, W = 32, C = 3;
    std::vector<float> reference(H*W*C, .5f), prediction = reference;
    std::vector<uint8_t> mask(H*W, 1);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W/2; ++x) {
        mask[y*W+x] = 0;
        for (int c = 0; c < C; ++c) prediction[(y*W+x)*C+c] = 1;
    }
    require(spirula::image_l1(reference.data(), prediction.data(), reference.size(), mask.data(), C) == 0,
            "masked L1 includes invalid pixels");
    require(std::isinf(spirula::image_psnr(reference.data(), prediction.data(), reference.size(), mask.data(), C)),
            "masked PSNR includes invalid pixels");
    require(std::abs(spirula::image_ssim(reference.data(), prediction.data(), H, W, C, mask.data())-1) < 1e-6f,
            "SSIM windows include invalid pixels");
    std::fill(prediction.begin(), prediction.end(), .25f);
    require(std::abs(spirula::image_l1(reference.data(), prediction.data(), reference.size(), mask.data(), C)-.25f) < 1e-6f,
            "masked L1 is not normalized by valid support");
    require(std::abs(spirula::image_psnr(reference.data(), prediction.data(), reference.size(), mask.data(), C)
            -20*std::log10(4.0f)) < 1e-5f, "masked PSNR is not normalized by valid support");
    std::fill(mask.begin(), mask.end(), 1);
    require(spirula::image_ssim(reference.data(), prediction.data(), H, W, C)
            == spirula::image_ssim(reference.data(), prediction.data(), H, W, C, mask.data()),
            "full mask changes SSIM");
}

void masked_color_fit() {
    constexpr int N = 256;
    std::vector<float> image(N*3), reference(N*3), altered(N*3), a, b;
    std::vector<uint8_t> mask(N, 1);
    for (int p = 0; p < N; ++p) {
        mask[p] = p%3 != 0;
        for (int c = 0; c < 3; ++c) {
            float v = .1f + .8f*((p*(17+2*c)+37*c)%251)/250.0f;
            image[3*p+c] = v;
            reference[3*p+c] = .8f*v + .05f;
            altered[3*p+c] = mask[p] ? reference[3*p+c] : 1.0f-v;
        }
    }
    spirula::color_correct_into(image.data(), reference.data(), N, 3, a, 2, .01f, mask.data());
    spirula::color_correct_into(image.data(), altered.data(), N, 3, b, 2, .01f, mask.data());
    for (int i = 0; i < N*3; ++i)
        require(a[i] == b[i], "invalid pixels affect the color fit");
    require(spirula::image_l1(a.data(), reference.data(), N*3, mask.data(), 3) < 1e-5f,
            "masked color correction did not recover an affine color transform");
}
}

int main() {
    try { ignored_pixels(); masked_color_fit(); }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
    std::puts("masked_eval_metrics passed");
}
