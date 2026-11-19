// The metric LiDAR depth term: zero at the measured distance, the closed-form
// Cauchy value off it, and no gradient where the map has no depth.

#include <kernels/loss/PerPixelLoss.cuh>
#include <core/Tensor.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

struct DeviceInputs {
    std::vector<void*> allocations;
    ~DeviceInputs() {
        for (void* data : allocations) backend::device_free(data);
    }

    template <typename T>
    T* upload(const std::vector<T>& host) {
        const size_t bytes = host.size() * sizeof(T);
        T* data = static_cast<T*>(backend::device_malloc((bytes + 3) & ~size_t(3)));
        backend::memcpy_sync(data, host.data(), bytes, backend::MemcpyKind::HostToDevice);
        allocations.push_back(data);
        return data;
    }
};

TorchTensorView view(void* data, std::vector<int64_t> shape, uint32_t bytes = 4) {
    return std::make_tuple(reinterpret_cast<uint64_t>(data), bytes, std::move(shape));
}

TorchTensorView absent() { return view(nullptr, {0}); }

struct Outcome {
    float loss;
    std::vector<float> gradient;
    std::vector<float> target;
};

// Target in millimetre counts, rendered depth in metres times `ratio`, or times
// `hole_ratio` where the map has no depth.
Outcome run(int64_t h, int64_t w, float ratio, float weight, int scales = 1,
            float hole_ratio = 0.0f) {
    const int64_t count = h * w;
    DeviceInputs inputs;
    std::vector<float> rendered(count), target(count, 0.0f);
    for (int64_t i = 0; i < count; ++i) {
        const float mm = 500.0f + float((i * 37) % 113) * 170.0f;
        const bool hole = i % 7 == 0;
        if (!hole) target[i] = mm;
        rendered[i] = (hole && hole_ratio > 0.0f ? hole_ratio : ratio) * mm * 0.001f;
    }
    auto rgb = view(inputs.upload(std::vector<float>(3 * count, 0.4f)), {1, h, w, 3});
    auto depth = view(inputs.upload(rendered), {1, h, w, 1});
    auto gt = view(inputs.upload(target), {1, h, w, 1});
    auto transmittance = view(inputs.upload(std::vector<float>(count, 0.5f)), {1, h, w, 1});
    std::array<float, static_cast<int>(LossWeightIndex::length)> weights{};
    weights[static_cast<int>(LossWeightIndex::DepthSupMetric)] = weight;
    weights[static_cast<int>(LossWeightIndex::DepthMetricLogScale)] = std::log(0.001f);
    std::vector<float> upstream(static_cast<int>(LossIndex::length), 0.0f);
    upstream[static_cast<int>(LossIndex::DepthSupMetric)] = 1.0f;
    auto v_losses = view(inputs.upload(upstream), {static_cast<int>(LossIndex::length)});
    std::vector<bool> needs(13, false);
    // SSIM requires its RGB gradient buffer even when its weight is zero.
    needs[0] = needs[2] = true;
    PerPixelGrads grads{};
    grads.v_render_rgb = view(inputs.upload(std::vector<float>(3 * count, 0.0f)), {1, h, w, 3});
    grads.v_render_depth = view(inputs.upload(std::vector<float>(count, 0.0f)), {1, h, w, 1});

    // Loss scalars arrive one evaluation behind.
    LossValues losses{};
    for (int replay = 0; replay < 3; ++replay) {
        losses = compute_multi_scale_per_pixel_losses(scales, rgb, rgb, depth, gt,
            absent(), absent(), absent(), transmittance, absent(), absent(), absent(),
            absent(), absent(), absent(), false, weights, 0.0f, 0, -1.0f, v_losses, needs,
            1, absent(), absent(), static_cast<int>(DensifyLossMapMode::None), 0.75f, 0.0f, grads);
        backend::device_synchronize();
    }
    Outcome result{losses.depth_sup_metric, std::vector<float>(count), target};
    backend::memcpy_sync(result.gradient.data(), reinterpret_cast<void*>(std::get<0>(grads.v_render_depth)),
        count * sizeof(float), backend::MemcpyKind::DeviceToHost);
    return result;
}

}  // namespace

int main() {
    const int64_t h = 32, w = 48;
    const float weight = 0.1f;
    int failures = 0;

    Outcome exact = run(h, w, 1.0f, weight);
    bool exact_ok = std::fabs(exact.loss) < 1e-6f;
    for (float g : exact.gradient) exact_ok &= std::fabs(g) < 1e-6f;
    std::printf("at the scan: loss=%.3g %s\n", exact.loss, exact_ok ? "ok" : "FAILED");
    failures += !exact_ok;

    // Every valid pixel is 10 % too far: the mean equals one pixel's value.
    Outcome beyond = run(h, w, 1.1f, weight);
    const float e = std::log(1.1f);
    const float expect = weight * 0.015f * std::log(1.0f + (e / 0.03f) * (e / 0.03f));
    bool beyond_ok = std::fabs(beyond.loss - expect) < 1e-4f * expect;
    for (size_t i = 0; i < beyond.gradient.size(); ++i) {
        const float g = beyond.gradient[i];
        beyond_ok &= std::isfinite(g) && (beyond.target[i] == 0.0f ? g == 0.0f : g > 0.0f);
    }
    std::printf("10 %% too far: loss=%.6g expect=%.6g, gradients %s\n", beyond.loss, expect,
                beyond_ok ? "ok" : "FAILED");
    failures += !beyond_ok;

    Outcome off = run(h, w, 1.1f, 0.0f);
    bool off_ok = off.loss == 0.0f;
    for (float g : off.gradient) off_ok &= g == 0.0f;
    std::printf("zero weight: %s\n", off_ok ? "ok" : "FAILED");
    failures += !off_ok;

    // A coarse render pixel averages the holes that the pooled map skips.
    Outcome holes = run(h, w, 1.0f, weight, 2, 5.0f);
    bool holes_ok = std::fabs(holes.loss) < 1e-6f;
    for (float g : holes.gradient) holes_ok &= std::fabs(g) < 1e-6f;
    std::printf("two scales, far holes: loss=%.3g %s\n", holes.loss, holes_ok ? "ok" : "FAILED");
    failures += !holes_ok;

    Outcome two = run(h, w, 1.1f, weight, 2);
    bool two_ok = std::fabs(two.loss - expect) < 1e-3f * expect;
    for (size_t i = 0; i < two.gradient.size(); ++i)
        two_ok &= std::fabs(two.gradient[i] - beyond.gradient[i]) <=
                  1e-3f * std::fabs(beyond.gradient[i]);
    std::printf("two scales keep the weight: loss=%.6g expect=%.6g %s\n", two.loss, expect,
                two_ok ? "ok" : "FAILED");
    failures += !two_ok;

    std::printf("%d failing cases\n", failures);
    return failures ? 1 : 0;
}
