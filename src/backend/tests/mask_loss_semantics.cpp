// Self-checking test for what an image mask -- and the blown-highlight cutoff
// that drops pixels the same way -- means in the loss. Contracts that are all
// silent when broken: the run trains, it just trains the wrong thing. Runs
// under either backend:
//
//   ./mask_loss_semantics

#include <kernels/loss/PerPixelLoss.cuh>
#include <core/Tensor.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

using backend::MemcpyKind;

static int g_failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%-62s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) g_failures++;
}

template <typename T>
static T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

static std::vector<float> download(const float* d, int64_t n) {
    std::vector<float> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * sizeof(float),
                         MemcpyKind::DeviceToHost);
    return h;
}

static TorchTensorView ttv(void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, (uint32_t)4, std::vector<int64_t>{0});
}

namespace {

constexpr int64_t B = 1, H = 32, W = 32, NP = B * H * W;

struct Outcome {
    float alpha_sup;
    float ssim;
    float rgb_loss;
    std::vector<float> v_render_rgb;
};

// One loss evaluation. `mask` empty means no mask buffer at all (has_mask=0);
// `zero_depth` supplies an all-sentinel GT depth map, which the alpha
// supervision must not read.
Outcome run(const std::vector<uint8_t>& mask, bool zero_depth, float w_alpha,
            int64_t Hm = H, int64_t Wm = W, float w_ssim = 0.0f,
            const std::vector<float>* rgb_in = nullptr, float w_rgb = 1.0f,
            float sat = -1.0f, const std::vector<float>* ref_in = nullptr) {
    std::vector<float> rgb((size_t)3 * NP, 0.5f), ref((size_t)3 * NP, 0.25f);
    if (rgb_in) rgb = *rgb_in;
    for (size_t i = 0; i < ref.size(); i++)
        ref[i] = 0.25f + 0.5f * (float)((i * 37) % 17) / 17.0f;
    if (ref_in) ref = *ref_in;
    std::vector<float> Ts((size_t)NP, 0.5f);
    float* d_rgb = upload(rgb);
    float* d_ref = upload(ref);
    float* d_Ts = upload(Ts);
    float* d_depth = zero_depth ? upload(std::vector<float>((size_t)NP, 0.0f))
                                : nullptr;
    uint8_t* d_mask = mask.empty() ? nullptr : upload(mask);

    std::array<float, (int)LossWeightIndex::length> weights{};
    weights[(int)LossWeightIndex::RgbSupL1] = w_rgb;
    weights[(int)LossWeightIndex::AlphaSup] = w_alpha;

    std::vector<float> v_losses((size_t)LossIndex::length, 0.0f);
    v_losses[(int)LossIndex::RgbLoss] = 1.0f;
    float* d_v_losses = upload(v_losses);

    std::vector<bool> needs(13, false);
    needs[0] = true;
    PerPixelGrads grads = {};
    grads.v_render_rgb =
        ttv(upload(std::vector<float>((size_t)3 * NP, 0.0f)), {B, H, W, 3});

    auto once = [&]() {
        return compute_multi_scale_per_pixel_losses(
            1, ttv(d_rgb, {B, H, W, 3}), ttv(d_ref, {B, H, W, 3}), ttv_null(),
            d_depth ? ttv(d_depth, {B, H, W, 1}) : ttv_null(), ttv_null(),
            ttv_null(), ttv_null(), ttv(d_Ts, {B, H, W, 1}), ttv_null(),
            ttv_null(), ttv_null(), ttv_null(), ttv_null(),
            d_mask ? std::make_tuple((uint64_t)d_mask, (uint32_t)1,
                                     std::vector<int64_t>{B, Hm, Wm, 1})
                   : ttv_null(),
            /*has_mask=*/d_mask != nullptr, weights, w_ssim, 1.0f, sat,
            ttv(d_v_losses, {(int64_t)LossIndex::length}), needs, B,
            ttv_null(), ttv_null(), (int)DensifyLossMapMode::None, 0.75f,
            0.0f, grads);
    };
    once();  // the loss scalars are read one iteration behind
    LossValues lv = once();
    backend::device_synchronize();
    return {lv.alpha_sup, lv.ssim, lv.rgb_loss,
            download((const float*)std::get<0>(grads.v_render_rgb), 3 * NP)};
}

// Left half masked out (0 = "not the subject"), right half kept.
std::vector<uint8_t> half_mask(int64_t h = H, int64_t w = W) {
    std::vector<uint8_t> m((size_t)(h * w));
    for (int64_t y = 0; y < h; y++)
        for (int64_t x = 0; x < w; x++) m[(size_t)(y * w + x)] = x < w / 2 ? 0 : 1;
    return m;
}

std::vector<float> ramp_image(uint32_t seed) {
    std::vector<float> v((size_t)3 * NP);
    uint32_t s = seed;
    for (size_t i = 0; i < v.size(); i++) {
        s = s * 1664525u + 1013904223u;
        v[i] = (float)((s >> 8) & 0xffffu) / 65535.0f;
    }
    return v;
}

}  // namespace

int main() {
    // Cut-out mode: masked-out pixels are trained toward empty, and whether a
    // GT depth map exists has nothing to do with it.
    Outcome cut = run(half_mask(), /*zero_depth=*/false, 1.0f);
    check(cut.alpha_sup > 0.0f, "cut-out: masked pixels get alpha supervision");
    Outcome cut_zd = run(half_mask(), /*zero_depth=*/true, 1.0f);
    check(std::fabs(cut_zd.alpha_sup - cut.alpha_sup) < 1e-5f,
          "cut-out: an all-sentinel GT depth map changes nothing");

    // No mask means no opinion about opacity: ref_alpha reads false for every
    // pixel, which must not train the whole image to empty.
    Outcome none = run({}, /*zero_depth=*/false, 1.0f);
    check(none.alpha_sup == 0.0f, "no mask: no alpha supervision anywhere");

    // Ignore-distractors mode: zero alpha weight, and masked pixels take no
    // colour gradient.
    Outcome ign = run(half_mask(), /*zero_depth=*/false, 0.0f);
    check(ign.alpha_sup == 0.0f, "ignore mode: no alpha supervision");
    bool masked_zero = true, kept_nonzero = false;
    for (int64_t y = 0; y < H; y++)
        for (int64_t x = 0; x < W; x++) {
            const float g = ign.v_render_rgb[(size_t)(y * W + x) * 3];
            if (x < W / 2) masked_zero &= (g == 0.0f);
            else kept_nonzero |= (g != 0.0f);
        }
    check(masked_zero && kept_nonzero,
          "ignore mode: masked pixels take no colour gradient");

    // A mask is stored at the size of the file it came from, so a downscaled
    // run hands the kernels a mask larger than the render. Every consumer must
    // resample it rather than index it with render coordinates.
    Outcome big = run(half_mask(H * 2, W * 2), /*zero_depth=*/false, 0.0f,
                      H * 2, W * 2, /*w_ssim=*/0.2f);
    bool big_masked_zero = true, big_kept_nonzero = false;
    for (int64_t y = 0; y < H; y++)
        for (int64_t x = 0; x < W; x++) {
            const float g = big.v_render_rgb[(size_t)(y * W + x) * 3];
            if (x < W / 2) big_masked_zero &= (g == 0.0f);
            else big_kept_nonzero |= (g != 0.0f);
        }
    check(big_masked_zero && big_kept_nonzero,
          "double-resolution mask still masks the colour gradient");

    // The ssim scalar counts a masked window centre as a perfect match, so the
    // gradient it reports must be the gradient of exactly that: perturb a patch
    // of kept pixels beside the mask edge and compare against its own sum.
    {
        const float w_ssim = 0.2f;
        std::vector<float> base = ramp_image(11);
        Outcome a = run(half_mask(), false, 0.0f, H, W, w_ssim, &base, 0.0f);
        double gsum = 0.0;
        for (int64_t y = 8; y < 24; y++)
            for (int64_t x = W / 2; x < W / 2 + 4; x++)
                for (int c = 0; c < 3; c++)
                    gsum += a.v_render_rgb[(size_t)(y * W + x) * 3 + c];
        const float eps = 1.0f / 64.0f;
        std::vector<float> up = base, dn = base;
        for (int64_t y = 8; y < 24; y++)
            for (int64_t x = W / 2; x < W / 2 + 4; x++)
                for (int c = 0; c < 3; c++) {
                    size_t i = (size_t)(y * W + x) * 3 + c;
                    up[i] += eps; dn[i] -= eps;
                }
        Outcome bu = run(half_mask(), false, 0.0f, H, W, w_ssim, &up, 0.0f);
        Outcome bd = run(half_mask(), false, 0.0f, H, W, w_ssim, &dn, 0.0f);
        // v_render_rgb carries d[w_ssim * (1 - ssim)] / dx.
        double fd = -(double)(bu.ssim - bd.ssim) * w_ssim / (2.0 * eps);
        std::printf("    (central difference %.6g vs gradient sum %.6g)\n", fd, gsum);
        check(std::fabs(fd - gsum) <= 0.05 * std::fabs(gsum),
              "ssim gradient at a mask edge matches the ssim it reports");
    }

    // Blown highlights: where the render and the photo are both over the cutoff
    // in every channel, both are clipped and no error there is real, so the
    // pixel leaves the loss exactly as a masked one does.
    {
        const float kSat = 0.98f;
        std::vector<float> rgb = ramp_image(23), ref = ramp_image(29);
        for (auto& v : rgb) v *= 0.9f;
        for (auto& v : ref) v *= 0.9f;
        // (4..12)^2 is over the cutoff in both; (20..28)^2 only in the render.
        auto fill = [](std::vector<float>& v, int64_t y0, int64_t x0, float a) {
            for (int64_t y = y0; y < y0 + 8; y++)
                for (int64_t x = x0; x < x0 + 8; x++)
                    for (int c = 0; c < 3; c++)
                        v[(size_t)(y * W + x) * 3 + c] = a;
        };
        fill(rgb, 4, 4, 0.999f);
        fill(ref, 4, 4, 0.985f);
        fill(rgb, 20, 20, 0.999f);

        auto block_grad = [](const Outcome& o, int64_t y0, int64_t x0) {
            double sum = 0.0;
            for (int64_t y = y0; y < y0 + 8; y++)
                for (int64_t x = x0; x < x0 + 8; x++)
                    for (int c = 0; c < 3; c++)
                        sum += std::fabs(o.v_render_rgb[(size_t)(y * W + x) * 3 + c]);
            return sum;
        };

        Outcome off = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref);
        Outcome on = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, kSat, &ref);
        check(block_grad(off, 4, 4) > 0.0 && block_grad(on, 4, 4) == 0.0,
              "blown block: colour gradient goes to zero");
        check(block_grad(on, 20, 20) > 0.0,
              "bright in the render only: still supervised");
        check(on.rgb_loss != off.rgb_loss,
              "blown block: the reported rgb loss changes");

        // The SSIM half of the fused loss takes the same gate.
        Outcome ssim_on =
            run({}, false, 0.0f, H, W, 0.2f, &rgb, 0.0f, kSat, &ref);
        Outcome ssim_off =
            run({}, false, 0.0f, H, W, 0.2f, &rgb, 0.0f, -1.0f, &ref);
        check(block_grad(ssim_off, 4, 4) > 0.0 &&
                  block_grad(ssim_on, 4, 4) == 0.0,
              "blown block: ssim gradient goes to zero");

        // Nothing over the cutoff: the coverage divide the threshold turns on
        // must reproduce the untouched path (to the gaussian's own rounding).
        std::vector<float> dim_rgb = ramp_image(31), dim_ref = ramp_image(37);
        for (auto& v : dim_rgb) v *= 0.9f;
        for (auto& v : dim_ref) v *= 0.9f;
        Outcome inert_on =
            run({}, false, 0.0f, H, W, 0.2f, &dim_rgb, 1.0f, kSat, &dim_ref);
        Outcome inert_off =
            run({}, false, 0.0f, H, W, 0.2f, &dim_rgb, 1.0f, -1.0f, &dim_ref);
        double worst = 0.0, scale = 0.0;
        for (size_t i = 0; i < inert_on.v_render_rgb.size(); i++) {
            worst = std::max(worst, (double)std::fabs(inert_on.v_render_rgb[i] -
                                                      inert_off.v_render_rgb[i]));
            scale = std::max(scale, (double)std::fabs(inert_off.v_render_rgb[i]));
        }
        check(scale > 0.0 && worst <= 1e-4 * scale,
              "no pixel over the cutoff: the threshold changes nothing");
    }

    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
