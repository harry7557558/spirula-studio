// Vulkan implementation of the optimizer launch APIs the portable engine
// references (kernels/optim/Optimizer.cuh): fused_adam_step (fp32 / quantized-state /
// doubly-quantized), fused_adagrad_step, the trust-region adamtr color
// variants, the fused 3DGS geometry step, float_add_into,
// increment_int32_inplace. Device work: shaders/optimizer.slang,
// optim_color.slang and optim_geometry.slang.

#include <kernels/optim/Optimizer.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

#include <core/Env.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// Mirrors FusedAdamParams in shaders/optimizer.slang.
struct FusedAdamParams {
    uint64_t param, grad, exp_avg, exp_avg_sq, steps;
    float lr, decay, decay_offset, grad_scale;
    int32_t scalar_step;
    uint32_t has_steps, numel, stride, zero_grad, wgs_per_row;
};
static_assert(sizeof(FusedAdamParams) == 5 * 8 + 10 * 4,
              "params layout must match the slang struct");

// Mirrors AdagradParams.
struct AdagradParams {
    uint64_t param, grad, accum;
    float lr;
    uint32_t numel, wgs_per_row;
};
static_assert(sizeof(AdagradParams) == 3 * 8 + 3 * 4 + 4 /*pad*/,
              "params layout must match the slang struct");

// Mirrors QAdamParams.
struct QAdamParams {
    uint64_t param, grad, packed, quant_bounds, steps;
    float lr, decay, decay_offset, grad_scale;
    int32_t scalar_step;
    uint32_t has_steps, numel, stride, zero_grad, num_blocks, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(QAdamParams) == 5 * 8 + 12 * 4,
              "params layout must match the slang struct");

// Mirrors QQAdamParams.
struct QQAdamParams {
    uint64_t grad, grad_q_packed, grad_q_bounds, optim_packed, optim_bounds,
        value_packed, value_bounds, steps;
    float lr, decay, decay_offset, grad_scale;
    int32_t scalar_step;
    uint32_t has_steps, grad_quant, numel, stride, zero_grad, num_blocks,
        wgs_per_row;
};
static_assert(sizeof(QQAdamParams) == 8 * 8 + 12 * 4,
              "params layout must match the slang struct");

// Mirrors FloatAddParams.
struct FloatAddParams {
    uint64_t dst, src;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(FloatAddParams) == 2 * 8 + 2 * 4, "layout");

// Mirrors IncI32Params.
struct IncI32Params {
    uint64_t data;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(IncI32Params) == 8 + 2 * 4, "layout");

uint32_t checked_u32_numel(int64_t numel, const char* what) {
    if (numel < 0 || numel > (int64_t)UINT32_MAX)
        throw std::runtime_error(std::string(what) +
                                 ": numel exceeds the u32 cell-index range");
    return (uint32_t)numel;
}

// Cells per launch for the per-cell optimizer entries, whose cell index is
// u32: whole 256-cell blocks of whole splats, so a later slice only advances
// pointers. Only SH reaches two; SS_OPTIM_SLICE_CELLS forces more, for tests.
int64_t optim_slice_cells(int64_t stride) {
    const int64_t grain = 256 * (stride > 0 ? stride : 1);
    static const int64_t cap = [] {
        const char* v = spirula::env("OPTIM_SLICE_CELLS");
        const int64_t n = v ? std::atoll(v) : 0;
        return n > 0 ? n : ((int64_t)1 << 30);
    }();
    const int64_t n = cap / grain * grain;
    return n > 0 ? n : grain;
}

}  // namespace

/* API definitions matching kernels/optim/Optimizer.cuh (engine-referenced subset) */

void fused_adam_step(
    int64_t num_splats,
    DeviceTensorFloatND param,
    DeviceTensorFloatND grad,
    DeviceTensorFloatND exp_avg,
    DeviceTensorFloatND exp_avg_sq,
    float lr,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float l2_reg,
    float l2_reg_offset,
    float grad_scale, bool zero_grad
) {
    int64_t param_numel = param.numel();
    if (param_numel == 0 || num_splats == 0)
        return;
    int64_t stride = param_numel / num_splats;
    int64_t numel = num_splats * stride;

    FusedAdamParams p{};
    p.param = (uint64_t)param.data_ptr();
    p.grad = (uint64_t)grad.data_ptr();
    p.exp_avg = (uint64_t)exp_avg.data_ptr();
    p.exp_avg_sq = (uint64_t)exp_avg_sq.data_ptr();
    p.steps = vkk::or_fallback(per_splat_steps.data_ptr());
    p.has_steps = per_splat_steps.data_ptr() ? 1u : 0u;
    p.lr = lr;
    p.decay = 2.0f * l2_reg / (float)numel;
    p.decay_offset = l2_reg_offset;
    p.grad_scale = grad_scale;
    p.scalar_step = step;
    p.stride = (uint32_t)stride;
    p.zero_grad = zero_grad ? 1u : 0u;
    const int64_t slice = optim_slice_cells(stride);
    const uint64_t b_param = p.param, b_grad = p.grad;
    const uint64_t b_m = p.exp_avg, b_v = p.exp_avg_sq, b_steps = p.steps;
    for (int64_t base = 0; base < numel; base += slice) {
        const int64_t n = std::min(slice, numel - base);
        p.param = b_param + 4u * (uint64_t)base;
        p.grad = b_grad + 4u * (uint64_t)base;
        p.exp_avg = b_m + 4u * (uint64_t)base;
        p.exp_avg_sq = b_v + 4u * (uint64_t)base;
        if (p.has_steps) p.steps = b_steps + 4u * (uint64_t)(base / stride);
        p.numel = checked_u32_numel(n, "fused_adam_step");
        vkk::dispatch_flat("optimizer.fused_adam_fwd", backend::vk::SpecList{},
                           n, 256, &p, sizeof(p), &p.wgs_per_row);
    }
}

void fused_adagrad_step(
    DeviceTensorFloatND param,
    DeviceTensorFloatND grad,
    DeviceTensorFloatND accum,
    float lr
) {
    int64_t numel = param.numel();
    if (numel == 0) return;
    AdagradParams p{};
    p.param = (uint64_t)param.data_ptr();
    p.grad = (uint64_t)grad.data_ptr();
    p.accum = (uint64_t)accum.data_ptr();
    p.lr = lr;
    p.numel = checked_u32_numel(numel, "fused_adagrad_step");
    vkk::dispatch_flat("optimizer.fused_adagrad_fwd", backend::vk::SpecList{},
                       numel, 256, &p, sizeof(p), &p.wgs_per_row);
}

void fused_adam_step_quantized(
    int64_t num_splats,
    DeviceTensorFloatND param,
    DeviceTensorFloatND grad,
    uint8_t* packed,
    float4* quant_bounds,
    float lr,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float l2_reg,
    float l2_reg_offset,
    int bits,
    float grad_scale, bool zero_grad
) {
    int64_t param_numel = param.numel();
    if (param_numel == 0 || num_splats == 0)
        return;
    if (bits != 4 && bits != 8)
        throw std::runtime_error(
            "fused_adam_step_quantized: bits must be 4 or 8, got " +
            std::to_string(bits));
    int64_t stride = param_numel / num_splats;
    int64_t numel = num_splats * stride;

    QAdamParams p{};
    p.param = (uint64_t)param.data_ptr();
    p.grad = (uint64_t)grad.data_ptr();
    p.packed = (uint64_t)packed;
    p.quant_bounds = (uint64_t)quant_bounds;
    p.steps = vkk::or_fallback(per_splat_steps.data_ptr());
    p.has_steps = per_splat_steps.data_ptr() ? 1u : 0u;
    p.lr = lr;
    p.decay = 2.0f * l2_reg / (float)numel;
    p.decay_offset = l2_reg_offset;
    p.grad_scale = grad_scale;
    p.scalar_step = step;
    p.stride = (uint32_t)stride;
    p.zero_grad = zero_grad ? 1u : 0u;
    const int64_t slice = optim_slice_cells(stride);
    const uint64_t cell_bytes = bits == 8 ? 2u : 1u;
    const uint64_t b_param = p.param, b_grad = p.grad;
    const uint64_t b_packed = p.packed, b_bounds = p.quant_bounds;
    const uint64_t b_steps = p.steps;
    for (int64_t base = 0; base < numel; base += slice) {
        const int64_t n = std::min(slice, numel - base);
        p.param = b_param + 4u * (uint64_t)base;
        p.grad = b_grad + 4u * (uint64_t)base;
        p.packed = b_packed + cell_bytes * (uint64_t)base;
        p.quant_bounds = b_bounds + 16u * (uint64_t)(base / 256);
        if (p.has_steps) p.steps = b_steps + 4u * (uint64_t)(base / stride);
        p.numel = checked_u32_numel(n, "fused_adam_step_quantized");
        p.num_blocks = (uint32_t)((n + 255) / 256);
        // Spec IDs: 0 = kOptimBits (kValueBits unused by this entry).
        vkk::dispatch_flat("optimizer.fused_adam_q",
                           backend::vk::SpecList{(uint32_t)bits}, n, 256, &p,
                           sizeof(p), &p.wgs_per_row);
    }
}

void fused_adam_step_quantized_value(
    int64_t num_splats,
    int64_t param_numel,
    DeviceTensorFloatND grad,
    const uint8_t* grad_q_packed,
    const float2* grad_q_bounds,
    uint8_t* optim_packed,
    float4* optim_bounds,
    uint8_t* value_packed,
    float2* value_bounds,
    float lr,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float l2_reg,
    float l2_reg_offset,
    int optim_bits,
    int value_bits,
    float grad_scale, bool zero_grad
) {
    if (param_numel == 0 || num_splats == 0)
        return;
    if ((optim_bits != 4 && optim_bits != 8) ||
        (value_bits != 8 && value_bits != 16))
        throw std::runtime_error(
            "fused_adam_step_quantized_value: optim_bits in {4, 8} and "
            "value_bits in {8, 16}; got optim_bits=" +
            std::to_string(optim_bits) +
            ", value_bits=" + std::to_string(value_bits));
    int64_t stride = param_numel / num_splats;
    int64_t numel = num_splats * stride;

    QQAdamParams p{};
    p.grad = vkk::or_fallback(grad.data_ptr());
    p.grad_q_packed = vkk::or_fallback(grad_q_packed);
    p.grad_q_bounds = vkk::or_fallback(grad_q_bounds);
    p.grad_quant = grad_q_packed ? 1u : 0u;
    p.optim_packed = (uint64_t)optim_packed;
    p.optim_bounds = (uint64_t)optim_bounds;
    p.value_packed = (uint64_t)value_packed;
    p.value_bounds = (uint64_t)value_bounds;
    p.steps = vkk::or_fallback(per_splat_steps.data_ptr());
    p.has_steps = per_splat_steps.data_ptr() ? 1u : 0u;
    p.lr = lr;
    p.decay = 2.0f * l2_reg / (float)numel;
    p.decay_offset = l2_reg_offset;
    p.grad_scale = grad_scale;
    p.scalar_step = step;
    p.stride = (uint32_t)stride;
    p.zero_grad = zero_grad ? 1u : 0u;
    const int64_t slice = optim_slice_cells(stride);
    const uint64_t optim_bytes = optim_bits == 8 ? 2u : 1u;
    const uint64_t value_bytes = value_bits == 16 ? 2u : 1u;
    const uint64_t b_grad = p.grad, b_gq = p.grad_q_packed;
    const uint64_t b_gqb = p.grad_q_bounds;
    const uint64_t b_op = p.optim_packed, b_ob = p.optim_bounds;
    const uint64_t b_vp = p.value_packed, b_vb = p.value_bounds;
    const uint64_t b_steps = p.steps;
    for (int64_t base = 0; base < numel; base += slice) {
        const int64_t n = std::min(slice, numel - base);
        const uint64_t splat = (uint64_t)(base / stride);
        if (grad.data_ptr()) p.grad = b_grad + 4u * (uint64_t)base;
        if (p.grad_quant) {
            p.grad_q_packed = b_gq + (uint64_t)base;
            p.grad_q_bounds = b_gqb + 8u * (splat / 256);
        }
        p.optim_packed = b_op + optim_bytes * (uint64_t)base;
        p.optim_bounds = b_ob + 16u * (uint64_t)(base / 256);
        p.value_packed = b_vp + value_bytes * (uint64_t)base;
        p.value_bounds = b_vb + 8u * (uint64_t)(base / 256);
        if (p.has_steps) p.steps = b_steps + 4u * splat;
        p.numel = checked_u32_numel(n, "fused_adam_step_quantized_value");
        p.num_blocks = (uint32_t)((n + 255) / 256);
        // Spec IDs: 0 = kOptimBits, 1 = kValueBits.
        vkk::dispatch_flat(
            "optimizer.fused_adam_qq",
            backend::vk::SpecList{(uint32_t)optim_bits, (uint32_t)value_bits},
            n, 256, &p, sizeof(p), &p.wgs_per_row);
    }
}

void float_add_into(DeviceVector<float> dst, DeviceVector<float> src,
                    int64_t n) {
    if (n == 0 || dst.data_ptr() == nullptr || src.data_ptr() == nullptr)
        return;
    FloatAddParams p{};
    p.dst = (uint64_t)dst.data_ptr();
    p.src = (uint64_t)src.data_ptr();
    p.n = checked_u32_numel(n, "float_add_into");
    vkk::dispatch_flat("optimizer.float_add", backend::vk::SpecList{}, n, 256,
                       &p, sizeof(p), &p.wgs_per_row);
}

void float_max_into(DeviceVector<float> dst, DeviceVector<float> src,
                    int64_t n) {
    if (n == 0 || dst.data_ptr() == nullptr || src.data_ptr() == nullptr)
        return;
    FloatAddParams p{};
    p.dst = (uint64_t)dst.data_ptr();
    p.src = (uint64_t)src.data_ptr();
    p.n = checked_u32_numel(n, "float_max_into");
    vkk::dispatch_flat("optimizer.float_max", backend::vk::SpecList{}, n, 256,
                       &p, sizeof(p), &p.wgs_per_row);
}

void increment_int32_inplace(DeviceVector<int32_t> data, int64_t n) {
    if (n == 0 || data.data_ptr() == nullptr) return;
    IncI32Params p{};
    p.data = (uint64_t)data.data_ptr();
    p.n = checked_u32_numel(n, "increment_int32_inplace");
    vkk::dispatch_flat("optimizer.increment_i32", backend::vk::SpecList{}, n,
                       256, &p, sizeof(p), &p.wgs_per_row);
}

namespace {

// Mirrors AdamTrRgbParams in shaders/optim_color.slang.
struct AdamTrRgbParams {
    uint64_t rgbs, grad, exp_avg, exp_avg_sq, opacities;
    float lr, bias_correction1, bias_correction2, eps, eps_tr, grad_scale;
    uint32_t is_linear, zero_grad, num_gs, wgs_per_row;
};
static_assert(sizeof(AdamTrRgbParams) == 5 * 8 + 10 * 4,
              "params layout must match the slang struct");

// Mirrors AdamTrRgbShParams.
struct AdamTrRgbShParams {
    uint64_t param, grad, exp_avg, exp_avg_sq, rgbs, opacities;
    float lr, bias_correction1, bias_correction2, eps, eps_tr, grad_scale;
    uint32_t is_linear, zero_grad, num_params, num_sh, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(AdamTrRgbShParams) == 6 * 8 + 12 * 4,
              "params layout must match the slang struct");

// Mirrors OptimGeoParams in shaders/optim_geometry.slang.
struct OptimGeoParams {
    uint64_t means, v_means, g1_means, g2_means;
    uint64_t quats, v_quats, g1_quats, g2_quats;
    uint64_t scales, v_scales, g1_scales, g2_scales;
    uint64_t opacities, v_opacities, g1_opacities, g2_opacities;
    uint64_t features_dc, v_features_dc;
    uint64_t radii, densify_score, steps;
    uint64_t gq_means_packed, gq_means_bounds, gq_quats_packed,
        gq_quats_bounds, gq_scales_packed, gq_scales_bounds, gq_opac_packed,
        gq_opac_bounds, gq_dc_packed, gq_dc_bounds;
    uint64_t nq_means_packed, nq_quats_packed, nq_scales_packed,
        nq_opacities_packed, nq_dc_packed;
    uint64_t nq_means_bounds, nq_quats_bounds, nq_scales_bounds,
        nq_opacities_bounds, nq_dc_bounds;
    float lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc;
    float max_gauss_ratio, scale_regularization_weight,
        mcmc_opacity_reg_weight, mcmc_scale_reg_weight, erank_reg_weight,
        erank_reg_weight_s3, quat_norm_reg_weight,
        dc_reg_weight, sh_reg_weight, grad_scale, max_screen_size,
        max_screen_size_penalty, _pad;
    int32_t scalar_step;
    uint32_t has_steps, has_densify_score, numel, wgs_per_row;
    uint32_t _pad0;
};
static_assert(sizeof(OptimGeoParams) == 41 * 8 + 24 * 4,
              "params layout must match the slang struct");

int64_t tv_numel(const TorchTensorView& tv) {
    int64_t n = 1;
    for (auto s : std::get<2>(tv)) n *= s;
    return n;
}

void launch_adamtr_rgb(bool is_linear, TorchTensorView param,
                       TorchTensorView grad, TorchTensorView exp_avg,
                       TorchTensorView exp_avg_sq, TorchTensorView opacities,
                       float lr, float beta1, float beta2, float eps,
                       float eps_tr, int step, float grad_scale,
                       bool zero_grad) {
    int64_t num_gs = tv_numel(param) / 3;
    if (num_gs == 0) return;
    AdamTrRgbParams p{};
    p.rgbs = std::get<0>(param);
    p.grad = std::get<0>(grad);
    p.exp_avg = std::get<0>(exp_avg);
    p.exp_avg_sq = std::get<0>(exp_avg_sq);
    p.opacities = std::get<0>(opacities);
    p.lr = lr;
    p.bias_correction1 = 1.0f - std::pow(beta1, (float)step);
    p.bias_correction2 = 1.0f - std::pow(beta2, (float)step);
    p.eps = eps;
    p.eps_tr = eps_tr;
    p.grad_scale = grad_scale;
    p.is_linear = is_linear ? 1u : 0u;
    p.zero_grad = zero_grad ? 1u : 0u;
    p.num_gs = checked_u32_numel(num_gs, "fused_adamtr_rgb_optim");
    vkk::dispatch_flat("optim_color.fused_adamtr_rgb",
                       backend::vk::SpecList{}, num_gs, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void launch_adamtr_rgb_sh(bool is_linear, TorchTensorView param,
                          TorchTensorView grad, TorchTensorView exp_avg,
                          TorchTensorView exp_avg_sq, TorchTensorView colors,
                          TorchTensorView opacities, float lr, float beta1,
                          float beta2, float eps, float eps_tr, int step,
                          float grad_scale, bool zero_grad) {
    int64_t colors_numel = tv_numel(colors);
    int64_t num_gs = colors_numel / 3;
    if (num_gs == 0) return;
    int64_t num_sh = tv_numel(param) / colors_numel;
    if (num_sh == 0) return;
    int64_t num_params = num_gs * num_sh * 3;
    AdamTrRgbShParams p{};
    p.param = std::get<0>(param);
    p.grad = std::get<0>(grad);
    p.exp_avg = std::get<0>(exp_avg);
    p.exp_avg_sq = std::get<0>(exp_avg_sq);
    p.rgbs = std::get<0>(colors);
    p.opacities = std::get<0>(opacities);
    p.lr = lr;
    p.bias_correction1 = 1.0f - std::pow(beta1, (float)step);
    p.bias_correction2 = 1.0f - std::pow(beta2, (float)step);
    p.eps = eps;
    p.eps_tr = eps_tr;
    p.grad_scale = grad_scale;
    p.is_linear = is_linear ? 1u : 0u;
    p.zero_grad = zero_grad ? 1u : 0u;
    p.num_sh = (uint32_t)num_sh;
    const int64_t stride = num_sh * 3;
    const int64_t slice = optim_slice_cells(stride);
    const uint64_t b_param = p.param, b_grad = p.grad;
    const uint64_t b_m = p.exp_avg, b_v = p.exp_avg_sq;
    const uint64_t b_rgbs = p.rgbs, b_opac = p.opacities;
    for (int64_t base = 0; base < num_params; base += slice) {
        const int64_t n = std::min(slice, num_params - base);
        const uint64_t splat = (uint64_t)(base / stride);
        p.param = b_param + 4u * (uint64_t)base;
        p.grad = b_grad + 4u * (uint64_t)base;
        p.exp_avg = b_m + 4u * (uint64_t)base;
        p.exp_avg_sq = b_v + 4u * (uint64_t)base;
        p.rgbs = b_rgbs + 12u * splat;
        p.opacities = b_opac + 4u * splat;
        p.num_params = checked_u32_numel(n, "fused_adamtr_rgb_sh_optim");
        vkk::dispatch_flat("optim_color.fused_adamtr_rgb_sh",
                           backend::vk::SpecList{}, n, 256, &p, sizeof(p),
                           &p.wgs_per_row);
    }
}

}  // namespace

void fused_adamtr_linear_rgb_optim(
    TorchTensorView param, TorchTensorView grad, TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq, TorchTensorView opacities, float lr,
    float beta1, float beta2, float eps, float eps_tr, int step,
    float grad_scale, bool zero_grad
) {
    launch_adamtr_rgb(true, param, grad, exp_avg, exp_avg_sq, opacities, lr,
                      beta1, beta2, eps, eps_tr, step, grad_scale, zero_grad);
}

void fused_adamtr_rgb_optim(
    TorchTensorView param, TorchTensorView grad, TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq, TorchTensorView opacities, float lr,
    float beta1, float beta2, float eps, float eps_tr, int step,
    float grad_scale, bool zero_grad
) {
    launch_adamtr_rgb(false, param, grad, exp_avg, exp_avg_sq, opacities, lr,
                      beta1, beta2, eps, eps_tr, step, grad_scale, zero_grad);
}

void fused_adamtr_linear_rgb_sh_optim(
    TorchTensorView param, TorchTensorView grad, TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq, TorchTensorView colors,
    TorchTensorView opacities, float lr, float beta1, float beta2, float eps,
    float eps_tr, int step, float grad_scale, bool zero_grad
) {
    launch_adamtr_rgb_sh(true, param, grad, exp_avg, exp_avg_sq, colors,
                         opacities, lr, beta1, beta2, eps, eps_tr, step,
                         grad_scale, zero_grad);
}

void fused_adamtr_rgb_sh_optim(
    TorchTensorView param, TorchTensorView grad, TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq, TorchTensorView colors,
    TorchTensorView opacities, float lr, float beta1, float beta2, float eps,
    float eps_tr, int step, float grad_scale, bool zero_grad
) {
    launch_adamtr_rgb_sh(false, param, grad, exp_avg, exp_avg_sq, colors,
                         opacities, lr, beta1, beta2, eps, eps_tr, step,
                         grad_scale, zero_grad);
}

void fused_optim_3dgs_geometry(
    int64_t num_splats,
    DeviceVector<float3> means, DeviceVector<float3> v_means, DeviceVector<float3> g1_means, DeviceVector<float3> g2_means,
    DeviceVector<float4> quats, DeviceVector<float4> v_quats, DeviceVector<float4> g1_quats, DeviceVector<float4> g2_quats,
    DeviceVector<float3> scales, DeviceVector<float3> v_scales, DeviceVector<float3> g1_scales, DeviceVector<float3> g2_scales,
    DeviceVector<float> opacities, DeviceVector<float> v_opacities, DeviceVector<float> g1_opacities, DeviceVector<float> g2_opacities,
    DeviceVector<float3> features_dc, DeviceVector<float3> v_features_dc,
    DeviceVector<float> radii,
    DeviceVector<float> densify_score,
    const float lr_means, const float lr_quats, const float lr_scales, const float lr_opacs,
    const float lr_features_dc,
    const float max_gauss_ratio, const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight, const float mcmc_scale_reg_weight,
    const float erank_reg_weight, const float erank_reg_weight_s3, const float quat_norm_reg_weight,
    const float dc_reg_weight, const float sh_reg_weight,
    const float max_screen_size, const float max_screen_size_penalty,
    bool use_scale_agnostic_mean,
    NonShQuantState non_sh,
    GradQuantBuffers gq,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float grad_scale, bool zero_grad
) {
    if (num_splats == 0) return;

    OptimGeoParams p{};
    p.means = (uint64_t)means.data_ptr();
    p.v_means = vkk::or_fallback(v_means.data_ptr());
    p.g1_means = vkk::or_fallback(g1_means.data_ptr());
    p.g2_means = vkk::or_fallback(g2_means.data_ptr());
    p.quats = (uint64_t)quats.data_ptr();
    p.v_quats = vkk::or_fallback(v_quats.data_ptr());
    p.g1_quats = vkk::or_fallback(g1_quats.data_ptr());
    p.g2_quats = vkk::or_fallback(g2_quats.data_ptr());
    p.scales = (uint64_t)scales.data_ptr();
    p.v_scales = vkk::or_fallback(v_scales.data_ptr());
    p.g1_scales = vkk::or_fallback(g1_scales.data_ptr());
    p.g2_scales = vkk::or_fallback(g2_scales.data_ptr());
    p.opacities = (uint64_t)opacities.data_ptr();
    p.v_opacities = vkk::or_fallback(v_opacities.data_ptr());
    p.g1_opacities = vkk::or_fallback(g1_opacities.data_ptr());
    p.g2_opacities = vkk::or_fallback(g2_opacities.data_ptr());
    p.features_dc = vkk::or_fallback(features_dc.data_ptr());
    p.v_features_dc = vkk::or_fallback(v_features_dc.data_ptr());
    p.radii = vkk::or_fallback(radii.data_ptr());
    p.densify_score = vkk::or_fallback(densify_score.data_ptr());
    p.steps = vkk::or_fallback(per_splat_steps.data_ptr());

    uint32_t gq_mask = 0;
    p.gq_means_packed = vkk::or_fallback(gq.means_packed);
    p.gq_means_bounds = vkk::or_fallback(gq.means_bounds);
    if (gq.means_packed) gq_mask |= 1;
    p.gq_quats_packed = vkk::or_fallback(gq.quats_packed);
    p.gq_quats_bounds = vkk::or_fallback(gq.quats_bounds);
    if (gq.quats_packed) gq_mask |= 2;
    p.gq_scales_packed = vkk::or_fallback(gq.scales_packed);
    p.gq_scales_bounds = vkk::or_fallback(gq.scales_bounds);
    if (gq.scales_packed) gq_mask |= 4;
    p.gq_opac_packed = vkk::or_fallback(gq.opac_packed);
    p.gq_opac_bounds = vkk::or_fallback(gq.opac_bounds);
    if (gq.opac_packed) gq_mask |= 8;
    p.gq_dc_packed = vkk::or_fallback(gq.dc_packed);
    p.gq_dc_bounds = vkk::or_fallback(gq.dc_bounds);
    if (gq.dc_packed) gq_mask |= 16;

    p.nq_means_packed = vkk::or_fallback(non_sh.means_packed);
    p.nq_quats_packed = vkk::or_fallback(non_sh.quats_packed);
    p.nq_scales_packed = vkk::or_fallback(non_sh.scales_packed);
    p.nq_opacities_packed = vkk::or_fallback(non_sh.opacities_packed);
    p.nq_dc_packed = vkk::or_fallback(non_sh.features_dc_packed);
    p.nq_means_bounds = vkk::or_fallback(non_sh.means_bounds);
    p.nq_quats_bounds = vkk::or_fallback(non_sh.quats_bounds);
    p.nq_scales_bounds = vkk::or_fallback(non_sh.scales_bounds);
    p.nq_opacities_bounds = vkk::or_fallback(non_sh.opacities_bounds);
    p.nq_dc_bounds = vkk::or_fallback(non_sh.features_dc_bounds);

    p.lr_means = lr_means;
    p.lr_quats = lr_quats;
    p.lr_scales = lr_scales;
    p.lr_opacs = lr_opacs;
    p.lr_features_dc = lr_features_dc;
    p.max_gauss_ratio = max_gauss_ratio;
    p.scale_regularization_weight =
        scale_regularization_weight / (float)num_splats;
    p.mcmc_opacity_reg_weight = mcmc_opacity_reg_weight / (float)num_splats;
    p.mcmc_scale_reg_weight = mcmc_scale_reg_weight / (float)num_splats;
    p.erank_reg_weight = erank_reg_weight / (float)num_splats;
    p.erank_reg_weight_s3 = erank_reg_weight_s3 / (float)num_splats;
    p.quat_norm_reg_weight = quat_norm_reg_weight / (float)num_splats;
    p.dc_reg_weight = dc_reg_weight;
    p.sh_reg_weight = sh_reg_weight;
    p.grad_scale = grad_scale;
    p.max_screen_size = max_screen_size;
    p.max_screen_size_penalty =
        radii.data_ptr() ? max_screen_size_penalty : 0.0f;
    p.scalar_step = step;
    p.has_steps = per_splat_steps.data_ptr() ? 1u : 0u;
    p.has_densify_score = densify_score.data_ptr() ? 1u : 0u;
    p.numel = checked_u32_numel(num_splats, "fused_optim_3dgs_geometry");

    backend::vk::SpecList spec{
        use_scale_agnostic_mean ? 1u : 0u,
        zero_grad ? 1u : 0u,
        non_sh.enabled ? 1u : 0u,
        gq_mask,
    };
    vkk::Fold f = vkk::fold_1d(num_splats, 256);
    p.wgs_per_row = f.per_row;
    vkk::dispatch_ring("optim_geometry.fused_optim_3dgs_geometry", spec,
                       f.per_row, f.rows, 1, &p, sizeof(p));
}
