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
    std::vector<float> v_render_Ts;
    float delta_e;
    std::vector<float> map;
    float flip;
};

// One loss evaluation. `mask` empty means no mask buffer at all (has_mask=0);
// `zero_depth` supplies an all-sentinel GT depth map, which the alpha
// supervision must not read.
Outcome run(const std::vector<uint8_t>& mask, bool zero_depth, float w_alpha,
            int64_t Hm = H, int64_t Wm = W, float w_ssim = 0.0f,
            const std::vector<float>* rgb_in = nullptr, float w_rgb = 1.0f,
            float sat = -1.0f, const std::vector<float>* ref_in = nullptr,
            int loss = 0, int map_mode = (int)DensifyLossMapMode::None) {
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
    v_losses[(int)LossIndex::AlphaSup] = 1.0f;
    float* d_v_losses = upload(v_losses);

    std::vector<bool> needs(13, false);
    needs[0] = true;
    needs[7] = true;
    PerPixelGrads grads = {};
    grads.v_render_rgb =
        ttv(upload(std::vector<float>((size_t)3 * NP, 0.0f)), {B, H, W, 3});
    grads.v_render_Ts = ttv(upload(std::vector<float>((size_t)NP, 0.0f)), {B, H, W, 1});
    float* d_map = map_mode != (int)DensifyLossMapMode::None
                       ? upload(std::vector<float>((size_t)NP, 0.0f)) : nullptr;

    auto once = [&]() {
        return compute_multi_scale_per_pixel_losses(
            1, ttv(d_rgb, {B, H, W, 3}), ttv(d_ref, {B, H, W, 3}), ttv_null(),
            d_depth ? ttv(d_depth, {B, H, W, 1}) : ttv_null(), ttv_null(),
            ttv_null(), ttv_null(), ttv(d_Ts, {B, H, W, 1}), ttv_null(),
            ttv_null(), ttv_null(), ttv_null(), ttv_null(),
            d_mask ? std::make_tuple((uint64_t)d_mask, (uint32_t)1,
                                     std::vector<int64_t>{B, Hm, Wm, 1})
                   : ttv_null(),
            /*has_mask=*/d_mask != nullptr, weights, w_ssim, loss, sat,
            ttv(d_v_losses, {(int64_t)LossIndex::length}), needs, B,
            ttv_null(), d_map ? ttv(d_map, {B, H, W, 1}) : ttv_null(), map_mode,
            0.75f, 0.0f, grads);
    };
    once();  // the loss scalars are read one iteration behind
    LossValues lv = once();
    backend::device_synchronize();
    return {lv.alpha_sup, lv.ssim, lv.rgb_loss,
            download((const float*)std::get<0>(grads.v_render_rgb), 3 * NP),
            download((const float*)std::get<0>(grads.v_render_Ts), NP),
            lv.delta_e, d_map ? download(d_map, NP) : std::vector<float>{}, lv.flip};
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
    bool transparent_gradient = true;
    for (int64_t y = 0; y < H; ++y) for (int64_t x = 0; x < W; ++x) {
        const float gradient = cut.v_render_Ts[(size_t)(y * W + x)];
        transparent_gradient &= x < W / 2 ? gradient < 0 : gradient == 0;
    }
    check(transparent_gradient, "cut-out: gradient drives masked pixels to transparent");
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

    // S-CIELAB reports mean dE and trains on w * mean dE / 100; its gradient
    // must be the gradient of exactly that, through the border's coverage
    // divide and the mask's.
    {
        constexpr int kSc = (int)PerceptualLoss::SCielab;
        const float w = 0.2f;
        auto fd_check = [&](const std::vector<uint8_t>& mask, int64_t y0,
                            int64_t x0, const char* what) {
            std::vector<float> base = ramp_image(41), ref = ramp_image(43);
            Outcome a = run(mask, false, 0.0f, H, W, w, &base, 0.0f, -1.0f, &ref, kSc);
            double gsum = 0.0;
            const float eps = 1.0f / 256.0f;
            std::vector<float> up = base, dn = base;
            for (int64_t y = y0; y < y0 + 8; y++)
                for (int64_t x = x0; x < x0 + 4; x++)
                    for (int c = 0; c < 3; c++) {
                        size_t i = (size_t)(y * W + x) * 3 + c;
                        gsum += a.v_render_rgb[i];
                        up[i] += eps; dn[i] -= eps;
                    }
            Outcome bu = run(mask, false, 0.0f, H, W, w, &up, 0.0f, -1.0f, &ref, kSc);
            Outcome bd = run(mask, false, 0.0f, H, W, w, &dn, 0.0f, -1.0f, &ref, kSc);
            double fd = (double)(bu.delta_e - bd.delta_e) * w *
                        kScielabLossPerDeltaE / (2.0 * eps);
            std::printf("    (central difference %.6g vs gradient sum %.6g)\n", fd, gsum);
            check(gsum != 0.0 && std::fabs(fd - gsum) <= 0.02 * std::fabs(gsum), what);
        };
        fd_check({}, 12, 14, "s-cielab gradient matches the dE it reports");
        fd_check({}, 0, 0, "s-cielab gradient at the image corner");
        fd_check(half_mask(), 8, W / 2, "s-cielab gradient at a mask edge");

        Outcome ign = run(half_mask(), false, 0.0f, H, W, w, nullptr, 0.0f,
                          -1.0f, nullptr, kSc);
        bool masked_zero = true, kept_nonzero = false;
        for (int64_t y = 0; y < H; y++)
            for (int64_t x = 0; x < W; x++) {
                const float g = ign.v_render_rgb[(size_t)(y * W + x) * 3];
                if (x < W / 2) masked_zero &= (g == 0.0f);
                else kept_nonzero |= (g != 0.0f);
            }
        check(masked_zero && kept_nonzero,
              "s-cielab: masked pixels take no colour gradient");

        // A uniform image is its own blur, so a constant offset shows up at
        // full strength in every pixel, the border included.
        std::vector<float> flat((size_t)3 * NP, 0.5f), flat_ref((size_t)3 * NP, 0.4f);
        Outcome fl = run({}, false, 0.0f, H, W, w, &flat, 0.0f, -1.0f, &flat_ref,
                         kSc, (int)DensifyLossMapMode::SCielab);
        float lo = fl.map[0], hi = fl.map[0];
        for (float v : fl.map) { lo = std::min(lo, v); hi = std::max(hi, v); }
        check(lo > 0.0f && hi - lo <= 1e-4f * hi,
              "s-cielab map: a flat offset scores alike at the border");
    }

    // FLIP trains on w * mean FLIP. Its gradient must be the gradient of the
    // value it reports, past the border's coverage divide and centre fill and
    // the mask's; its map must be FLIP's own closed forms where it has one.
    {
        constexpr int kFl = (int)PerceptualLoss::Flip;
        const float w = 0.2f;
        auto fd_check = [&](const std::vector<uint8_t>& mask, int64_t y0,
                            int64_t x0, const char* what) {
            std::vector<float> base = ramp_image(41), ref = ramp_image(43);
            Outcome a = run(mask, false, 0.0f, H, W, w, &base, 0.0f, -1.0f, &ref, kFl);
            double gsum = 0.0;
            // FLIP's square roots bend too fast for S-CIELAB's 1/256: it put
            // the corner 14% off, against 0.4% at this step.
            const float eps = 1.0f / 1024.0f;
            std::vector<float> up = base, dn = base;
            for (int64_t y = y0; y < y0 + 8; y++)
                for (int64_t x = x0; x < x0 + 4; x++)
                    for (int c = 0; c < 3; c++) {
                        size_t i = (size_t)(y * W + x) * 3 + c;
                        gsum += a.v_render_rgb[i];
                        up[i] += eps; dn[i] -= eps;
                    }
            Outcome bu = run(mask, false, 0.0f, H, W, w, &up, 0.0f, -1.0f, &ref, kFl);
            Outcome bd = run(mask, false, 0.0f, H, W, w, &dn, 0.0f, -1.0f, &ref, kFl);
            double fd = (double)(bu.flip - bd.flip) * w / (2.0 * eps);
            std::printf("    (central difference %.6g vs gradient sum %.6g)\n", fd, gsum);
            check(gsum != 0.0 && std::fabs(fd - gsum) <= 0.02 * std::fabs(gsum), what);
        };
        fd_check({}, 12, 14, "flip gradient matches the flip it reports");
        fd_check({}, 0, 0, "flip gradient at the image corner");
        fd_check(half_mask(), 8, W / 2, "flip gradient at a mask edge");

        std::vector<float> rgb = ramp_image(41), ref = ramp_image(43);
        Outcome ign = run(half_mask(), false, 0.0f, H, W, w, &rgb, 0.0f, -1.0f,
                          &ref, kFl, (int)DensifyLossMapMode::Flip);
        bool masked_zero = true, kept_nonzero = false;
        double map_sum = 0.0;
        for (int64_t y = 0; y < H; y++)
            for (int64_t x = 0; x < W; x++) {
                const float g = ign.v_render_rgb[(size_t)(y * W + x) * 3];
                if (x < W / 2) masked_zero &= (g == 0.0f && ign.map[(size_t)(y * W + x)] == 0.0f);
                else kept_nonzero |= (g != 0.0f);
                map_sum += ign.map[(size_t)(y * W + x)];
            }
        check(masked_zero && kept_nonzero,
              "flip: masked pixels take no gradient and score 0");
        check(std::fabs(map_sum / NP / w - ign.flip) <= 1e-4 * ign.flip,
              "flip: the reported mean is the mean of its map");

        // An all-kept mask takes the coverage kernel's float16 path; it must
        // land where the closed form does, border detectors included.
        Outcome open = run(std::vector<uint8_t>((size_t)NP, 1), false, 0.0f, H, W, w,
                           &rgb, 0.0f, -1.0f, &ref, kFl, (int)DensifyLossMapMode::Flip);
        Outcome none = run({}, false, 0.0f, H, W, w, &rgb, 0.0f, -1.0f, &ref, kFl,
                           (int)DensifyLossMapMode::Flip);
        double gworst = 0.0, gscale = 0.0, mworst = 0.0, mscale = 0.0;
        for (size_t i = 0; i < none.v_render_rgb.size(); i++) {
            gworst = std::max(gworst, (double)std::fabs(open.v_render_rgb[i] -
                                                        none.v_render_rgb[i]));
            gscale = std::max(gscale, (double)std::fabs(none.v_render_rgb[i]));
        }
        for (size_t i = 0; i < none.map.size(); i++) {
            mworst = std::max(mworst, (double)std::fabs(open.map[i] - none.map[i]));
            mscale = std::max(mscale, (double)std::fabs(none.map[i]));
        }
        std::printf("    (all-kept mask vs none: map %.3g, gradient %.3g of max)\n",
                    mworst / mscale, gworst / gscale);
        check(mworst <= 2e-3 * mscale && gworst <= 2e-3 * gscale,
              "flip: an all-kept mask matches no mask");

        // Flat greys have no features, so FLIP is its colour error alone,
        // redistributed |dL*|^0.7, at the border too.
        auto lin = [](double c) {
            return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
        };
        auto lstar = [&](double c) { return 116.0 * std::cbrt(lin(c)) - 16.0; };
        std::vector<float> flat((size_t)3 * NP, 0.5f), flat_ref((size_t)3 * NP, 0.4f);
        Outcome fl = run({}, false, 0.0f, H, W, w, &flat, 0.0f, -1.0f, &flat_ref,
                         kFl, (int)DensifyLossMapMode::Flip);
        const double want = w * 0.0575393531 * std::pow(lstar(0.5) - lstar(0.4), 0.7);
        float lo = fl.map[0], hi = fl.map[0];
        for (float v : fl.map) { lo = std::min(lo, v); hi = std::max(hi, v); }
        std::printf("    (flat grey map %.6g..%.6g, closed form %.6g)\n", lo, hi, want);
        check(std::fabs(lo - want) <= 1e-3 * want && std::fabs(hi - want) <= 1e-3 * want,
              "flip map: flat greys score their colour error everywhere");

        // A vertical step: away from the top and bottom, the edge response
        // beside it is the step itself, so the feature map there is
        // sqrt(|step difference| / sqrt 2) in linear luminance.
        std::vector<float> step((size_t)3 * NP), step_ref((size_t)3 * NP);
        for (int64_t y = 0; y < H; y++)
            for (int64_t x = 0; x < W; x++)
                for (int c = 0; c < 3; c++) {
                    step[(size_t)(y * W + x) * 3 + c] = x < W / 2 ? 0.3f : 0.6f;
                    step_ref[(size_t)(y * W + x) * 3 + c] = x < W / 2 ? 0.3f : 0.7f;
                }
        Outcome st = run({}, false, 0.0f, H, W, w, &step, 0.0f, -1.0f, &step_ref,
                         kFl, (int)DensifyLossMapMode::FlipFeature);
        const double ef = w * std::sqrt(((lin(0.7) - lin(0.3)) - (lin(0.6) - lin(0.3))) /
                                        std::sqrt(2.0));
        double sworst = 0.0;
        for (int64_t y = 5; y < H - 5; y++)
            for (int64_t x = W / 2 - 1; x <= W / 2; x++)
                sworst = std::max(sworst,
                                  std::fabs(st.map[(size_t)(y * W + x)] - ef));
        std::printf("    (step feature map vs closed form %.6g: worst %.3g)\n", ef, sworst);
        check(sworst <= 1e-4 * ef, "flip_feature map: a step scores its edge difference");
    }

    // Blur-tolerant SSIM trains on w * (1 - S). Its gradient runs through the
    // blur's own pass for the blurred branch, so check it where that pass
    // renormalizes (the corner) and where the mask cuts the windows.
    {
        constexpr int kBt = (int)PerceptualLoss::SsimBlurTolerant;
        const float w = 0.2f;
        auto fd_check = [&](const std::vector<uint8_t>& mask, int64_t y0,
                            int64_t x0, const char* what) {
            std::vector<float> base = ramp_image(61), ref = ramp_image(63);
            Outcome a = run(mask, false, 0.0f, H, W, w, &base, 0.0f, -1.0f, &ref, kBt);
            double gsum = 0.0;
            const float eps = 1.0f / 256.0f;
            std::vector<float> up = base, dn = base;
            for (int64_t y = y0; y < y0 + 8; y++)
                for (int64_t x = x0; x < x0 + 4; x++)
                    for (int c = 0; c < 3; c++) {
                        size_t i = (size_t)(y * W + x) * 3 + c;
                        gsum += a.v_render_rgb[i];
                        up[i] += eps; dn[i] -= eps;
                    }
            Outcome bu = run(mask, false, 0.0f, H, W, w, &up, 0.0f, -1.0f, &ref, kBt);
            Outcome bd = run(mask, false, 0.0f, H, W, w, &dn, 0.0f, -1.0f, &ref, kBt);
            double fd = -(double)(bu.ssim - bd.ssim) * w / (2.0 * eps);
            std::printf("    (central difference %.6g vs gradient sum %.6g)\n", fd, gsum);
            check(gsum != 0.0 && std::fabs(fd - gsum) <= 0.02 * std::fabs(gsum), what);
        };
        fd_check({}, 12, 14, "ssim_blur_tolerant gradient matches the ssim it reports");
        fd_check({}, 0, 0, "ssim_blur_tolerant gradient at the image corner");
        fd_check(half_mask(), 8, W / 2, "ssim_blur_tolerant gradient at a mask edge");

        std::vector<float> rgb = ramp_image(61), ref = ramp_image(63);
        Outcome ign = run(half_mask(), false, 0.0f, H, W, w, &rgb, 0.0f, -1.0f, &ref, kBt,
                          (int)DensifyLossMapMode::SsimContrastStruct);
        bool masked_zero = true, kept_nonzero = false;
        for (int64_t y = 0; y < H; y++)
            for (int64_t x = 0; x < W; x++) {
                const float g = ign.v_render_rgb[(size_t)(y * W + x) * 3];
                // The map divides by 3 as a multiply: 1 - 3 * (1/3) is not 0.
                if (x < W / 2)
                    masked_zero &= g == 0.0f && std::fabs(ign.map[(size_t)(y * W + x)]) <= 1e-6f;
                else kept_nonzero |= (g != 0.0f);
            }
        check(masked_zero && kept_nonzero,
              "ssim_blur_tolerant: masked pixels take no gradient and score 0");

        // Box-blur the noise image three times (about sigma 1.4 px), wrapping.
        auto blurred = [&](std::vector<float> v) {
            for (int it = 0; it < 3; it++) {
                std::vector<float> o(v.size());
                for (int64_t y = 0; y < H; y++)
                    for (int64_t x = 0; x < W; x++)
                        for (int c = 0; c < 3; c++) {
                            float s = 0.0f;
                            for (int dy = -1; dy <= 1; dy++)
                                for (int dx = -1; dx <= 1; dx++)
                                    s += v[(size_t)((((y + dy + H) % H) * W + (x + dx + W) % W) * 3 + c)];
                            o[(size_t)((y * W + x) * 3 + c)] = s / 9.0f;
                        }
                v = o;
            }
            return v;
        };
        std::vector<float> soft = blurred(rgb);
        Outcome sharp_bt = run({}, false, 0.0f, H, W, w, &rgb, 0.0f, -1.0f, &soft, kBt);
        Outcome sharp_ss = run({}, false, 0.0f, H, W, w, &rgb, 0.0f, -1.0f, &soft, 0);
        std::printf("    (sharp render, blurred photo: ssim %.4f, tolerant %.4f)\n",
                    sharp_ss.ssim, sharp_bt.ssim);
        check(sharp_bt.ssim > sharp_ss.ssim + 0.3f,
              "ssim_blur_tolerant: a photo blurrier than the render costs little");
        Outcome soft_bt = run({}, false, 0.0f, H, W, w, &soft, 0.0f, -1.0f, &rgb, kBt);
        Outcome soft_ss = run({}, false, 0.0f, H, W, w, &soft, 0.0f, -1.0f, &rgb, 0);
        std::printf("    (blurred render, sharp photo: ssim %.4f, tolerant %.4f)\n",
                    soft_ss.ssim, soft_bt.ssim);
        check(soft_bt.ssim <= soft_ss.ssim + 1e-5f,
              "ssim_blur_tolerant: a render blurrier than the photo gains nothing");
    }

    // A zero ssim_lambda still trains nothing through the perceptual term,
    // but a map that is only that term must not come back blank.
    {
        std::vector<float> rgb = ramp_image(51), ref = ramp_image(53);
        auto max_of = [](const std::vector<float>& v) {
            float m = 0.0f;
            for (float x : v) m = std::max(m, std::fabs(x));
            return m;
        };
        Outcome cs = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref, 0,
                         (int)DensifyLossMapMode::SsimContrastStruct);
        check(max_of(cs.map) > 0.0f, "ssim_lambda 0: the ssim_cs map is not blank");
        Outcome sc = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref,
                         (int)PerceptualLoss::SCielab, (int)DensifyLossMapMode::SCielab);
        check(max_of(sc.map) > 0.0f, "ssim_lambda 0: the s_cielab map is not blank");
        Outcome fm = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref,
                         (int)PerceptualLoss::Flip, (int)DensifyLossMapMode::FlipFeature);
        check(max_of(fm.map) > 0.0f, "ssim_lambda 0: the flip_feature map is not blank");
        Outcome l1 = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref);
        Outcome l1_sc = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref,
                            (int)PerceptualLoss::SCielab);
        double worst = 0.0;
        for (size_t i = 0; i < l1.v_render_rgb.size(); i++)
            worst = std::max(worst, (double)std::fabs(l1.v_render_rgb[i] -
                                                      l1_sc.v_render_rgb[i]));
        check(worst == 0.0, "ssim_lambda 0: s-cielab adds no gradient");
        Outcome l1_fl = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref,
                            (int)PerceptualLoss::Flip);
        worst = 0.0;
        for (size_t i = 0; i < l1.v_render_rgb.size(); i++)
            worst = std::max(worst, (double)std::fabs(l1.v_render_rgb[i] -
                                                      l1_fl.v_render_rgb[i]));
        check(worst == 0.0, "ssim_lambda 0: flip adds no gradient");
        Outcome l1_bt = run({}, false, 0.0f, H, W, 0.0f, &rgb, 1.0f, -1.0f, &ref,
                            (int)PerceptualLoss::SsimBlurTolerant);
        worst = 0.0;
        for (size_t i = 0; i < l1.v_render_rgb.size(); i++)
            worst = std::max(worst, (double)std::fabs(l1.v_render_rgb[i] -
                                                      l1_bt.v_render_rgb[i]));
        check(worst == 0.0, "ssim_lambda 0: ssim_blur_tolerant adds no gradient");
    }

    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
