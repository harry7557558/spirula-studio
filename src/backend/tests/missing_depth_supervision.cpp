// Missing reference depth must match an all-zero reference map on both backends.

#include <kernels/loss/PerPixelLoss.cuh>
#include <core/Tensor.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

enum class ReferenceDepth { Missing, Zero, Valid };

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
};

Outcome run(int64_t h, int64_t w, int scales, bool masked,
            ReferenceDepth reference, float weight = 0.01f) {
    const int64_t count = h * w;
    DeviceInputs inputs;
    std::vector<float> rendered(count), target(count, 0.0f);
    std::vector<uint8_t> mask(count);
    uint32_t seed = 42;
    for (int64_t i = 0; i < count; ++i) {
        seed = 1664525u * seed + 1013904223u;
        rendered[i] = 0.3f + 9.7f * float((seed >> 8) & 0xffffu) / 65535.0f;
        if (reference == ReferenceDepth::Valid && i % 7 != 0)
            target[i] = 0.4f + float((i * 37) % 113) / 17.0f;
        mask[i] = i % w >= w / 2;
    }
    auto rgb = view(inputs.upload(std::vector<float>(3 * count, 0.4f)), {1, h, w, 3});
    auto depth = view(inputs.upload(rendered), {1, h, w, 1});
    auto gt = reference == ReferenceDepth::Missing ? absent()
        : view(inputs.upload(target), {1, h, w, 1});
    auto transmittance = view(inputs.upload(std::vector<float>(count, 0.5f)), {1, h, w, 1});
    auto alpha = masked ? view(inputs.upload(mask), {1, h, w, 1}, 1) : absent();
    std::array<float, static_cast<int>(LossWeightIndex::length)> weights{};
    weights[static_cast<int>(LossWeightIndex::DepthSup)] = weight;
    std::vector<float> upstream(static_cast<int>(LossIndex::length), 0.0f);
    upstream[static_cast<int>(LossIndex::DepthSup)] = 1.0f;
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
            absent(), absent(), alpha, masked, weights, 0.0f, -1.0f, v_losses, needs,
            1, absent(), absent(), static_cast<int>(DensifyLossMapMode::None), 0.75f, 0.0f, grads);
        backend::device_synchronize();
    }
    Outcome result{losses.depth_sup, std::vector<float>(count)};
    backend::memcpy_sync(result.gradient.data(), reinterpret_cast<void*>(std::get<0>(grads.v_render_depth)),
        count * sizeof(float), backend::MemcpyKind::DeviceToHost);
    return result;
}

bool no_supervision(const Outcome& result) {
    return result.loss == 0.0f && std::all_of(result.gradient.begin(), result.gradient.end(),
        [](float value) { return value == 0.0f; });
}

bool valid_supervision(const Outcome& result, int64_t w, bool masked) {
    bool nonzero = false;
    for (size_t i = 0; i < result.gradient.size(); ++i) {
        const float value = result.gradient[i];
        if (!std::isfinite(value) || (masked && i % w < w / 2 && value != 0.0f)) return false;
        nonzero |= value != 0.0f;
    }
    return std::isfinite(result.loss) && result.loss > 0.0f && nonzero;
}

}  // namespace

int main() {
    int failures = 0;
    for (bool large : {false, true}) for (int scales : {1, 3}) for (bool masked : {false, true}) {
        const int64_t h = large ? 1500 : 32, w = large ? 2000 : 48;
        Outcome missing = run(h, w, scales, masked, ReferenceDepth::Missing);
        Outcome zero = run(h, w, scales, masked, ReferenceDepth::Zero);
        Outcome valid = run(h, w, scales, masked, ReferenceDepth::Valid);
        const bool missing_ok = no_supervision(missing), zero_ok = no_supervision(zero);
        const bool valid_ok = valid_supervision(valid, w, masked);
        std::printf("pixels=%lld scales=%d mask=%d: missing=%s (loss=%.9g), zero=%s, valid=%s\n",
            static_cast<long long>(h * w), scales, masked,
            missing_ok ? "ok" : "FAILED", missing.loss, zero_ok ? "ok" : "FAILED",
            valid_ok ? "ok" : "FAILED");
        failures += !missing_ok || !zero_ok || !valid_ok;
    }
    const bool disabled_ok = no_supervision(run(32, 48, 3, true, ReferenceDepth::Missing, 0.0f));
    std::printf("zero weight: %s\n", disabled_ok ? "ok" : "FAILED");
    failures += !disabled_ok;
    std::printf("%d failing cases\n", failures);
    return failures ? 1 : 0;
}
