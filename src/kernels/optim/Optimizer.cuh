#pragma once

#include <core/Tensor.h>
#include "core/NonShQuantState.h"
#include "kernels/projection/ProjectionBwdQuantGrad.cuh"   // GradQuantBuffers


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void set_zero_tensor(TorchTensorView x);


void fused_adam(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    float lr,
    float beta1,
    float beta2,
    float eps,
    int step
);


void offloaded_adam(
    TorchTensorView param,      // Device
    TorchTensorView grad,       // Device
    TorchTensorView exp_avg,    // Host
    TorchTensorView exp_avg_sq, // Host
    float lr, float beta1, float beta2, float eps, int step
);


void semi_offloaded_adam(
    TorchTensorView param,      // Device
    TorchTensorView grad,       // Device
    TorchTensorView exp_avg,    // Host
    TorchTensorView exp_avg_sq, // Host
    float lr, float beta1, float beta2, float eps, int step
);


void fused_adam_multi(
    std::vector<TorchTensorView> params,
    std::vector<TorchTensorView> grads,
    std::vector<TorchTensorView> exp_avgs,
    std::vector<TorchTensorView> exp_avg_sqs,
    float lr,
    float beta1,
    float beta2,
    float eps,
    int step
);


void fused_adam_riemannian_quat(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    float lr,
    float beta1,
    float beta2,
    float eps,
    int step
);


void fused_newton(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView hess_diag,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    float lr,
    float beta1,
    float beta2,
    float eps,
    int step1,
    int step2
);


void fused_newton_multi(
    std::vector<TorchTensorView> params,
    std::vector<TorchTensorView> grads,
    std::vector<TorchTensorView> hess_diags,
    std::vector<TorchTensorView> exp_avgs,
    std::vector<TorchTensorView> exp_avg_sqs,
    float lr,
    float beta1,
    float beta2,
    float eps,
    int step1,
    int step2
);


void fused_adam_scale_agnostic_mean(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    TorchTensorView scales,
    TorchTensorView quats,
    TorchTensorView opacities,
    TorchTensorView radii,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step
);


void fused_optim_3dgs_geometry(
    int64_t num_splats,
    DeviceVector<float3> means, DeviceVector<float3> v_means, DeviceVector<float3> g1_means, DeviceVector<float3> g2_means,
    DeviceVector<float4> quats, DeviceVector<float4> v_quats, DeviceVector<float4> g1_quats, DeviceVector<float4> g2_quats,
    DeviceVector<float3> scales, DeviceVector<float3> v_scales, DeviceVector<float3> g1_scales, DeviceVector<float3> g2_scales,
    DeviceVector<float> opacities, DeviceVector<float> v_opacities, DeviceVector<float> g1_opacities, DeviceVector<float> g2_opacities,
    // features_dc + grad: consumed only when non_sh has its quant buffers
    // populated (via non_sh.enabled flagged in `non_sh`). The fp32 path
    // ignores them and runs the separate fused_adam_step features_dc call.
    DeviceVector<float3> features_dc, DeviceVector<float3> v_features_dc,
    DeviceVector<float> radii,
    // optional [N] densification world-grad score output; empty disables.
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
);


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
);


void fused_adagrad_step(
    DeviceTensorFloatND param,
    DeviceTensorFloatND grad,
    DeviceTensorFloatND accum,
    float lr
);


void fused_adam_step_quantized(
    int64_t num_splats,
    DeviceTensorFloatND param,
    DeviceTensorFloatND grad,
    uint8_t* packed,                    // AoS (u, sqrt_g2) packed
    float4* quant_bounds,
    float lr,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float l2_reg,
    float l2_reg_offset,
    int bits,                           // 4 or 8 -- selects QuantizedAdamState<BITS, 256>
    float grad_scale, bool zero_grad
);


void fused_adam_step_quantized_value(
    int64_t num_splats,
    int64_t param_numel,                // = num_splats * stride (e.g. 3 * num_sh)
    DeviceTensorFloatND grad,           // fp32 dense [num_splats, stride]; null under grad-quant
    // Block-wise quantized SH grad (non-FPBO grad-quant path); null keeps the
    // fp32 `grad` read.
    const uint8_t* grad_q_packed,
    const float2*  grad_q_bounds,
    uint8_t* optim_packed,
    float4*  optim_bounds,
    uint8_t* value_packed,
    float2*  value_bounds,
    float lr,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float l2_reg,
    float l2_reg_offset,
    int optim_bits,                     // 4 or 8
    int value_bits,                     // 8 or 16
    float grad_scale, bool zero_grad
);


void fused_3dgs2tr_mean_optim(
    TorchTensorView means,
    TorchTensorView vr_means,
    TorchTensorView h_means,
    TorchTensorView scales,
    TorchTensorView quats,
    TorchTensorView opacities,
    TorchTensorView exp_avg_means,
    TorchTensorView exp_avg_sq_means,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step1,
    int step2
);


void fused_3dgs2tr_scale_optim(
    TorchTensorView scales,
    TorchTensorView vr_scales,
    TorchTensorView h_scales,
    TorchTensorView opacities,
    TorchTensorView exp_avg_scales,
    TorchTensorView exp_avg_sq_scales,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step1,
    int step2
);


void fused_3dgs2tr_color_optim(
    TorchTensorView colors,
    TorchTensorView vr_colors,
    TorchTensorView h_colors,
    TorchTensorView opacities,
    TorchTensorView exp_avg_colors,
    TorchTensorView exp_avg_sq_colors,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step1,
    int step2
);


void fused_3dgs2tr_opacity_optim(
    TorchTensorView opacities,
    TorchTensorView vr_opacities,
    TorchTensorView h_opacities,
    TorchTensorView exp_avg_opacities,
    TorchTensorView exp_avg_sq_opacities,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step1,
    int step2
);


void fused_3dgs2tr_quat_optim(
    TorchTensorView quats,
    TorchTensorView vr_quats,
    TorchTensorView h_quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView exp_avg_quats,
    TorchTensorView exp_avg_sq_quats,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step1,
    int step2
);


void fused_adam_linear_rgb_optim(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    float lr,
    float beta1,
    float beta2,
    float eps,
    int step
);


void fused_adamtr_linear_rgb_optim(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    TorchTensorView opacities,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step,
    float grad_scale, bool zero_grad
);


void fused_adamtr_rgb_optim(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    TorchTensorView opacities,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step,
    float grad_scale, bool zero_grad
);


void fused_adamtr_linear_rgb_sh_optim(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    TorchTensorView colors,
    TorchTensorView opacities,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step,
    float grad_scale, bool zero_grad
);


void fused_adamtr_rgb_sh_optim(
    TorchTensorView param,
    TorchTensorView grad,
    TorchTensorView exp_avg,
    TorchTensorView exp_avg_sq,
    TorchTensorView colors,
    TorchTensorView opacities,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float eps_tr,
    int step,
    float grad_scale, bool zero_grad
);


void increment_int32_inplace(DeviceVector<int32_t> data, int64_t n);


void float_add_into(DeviceVector<float> dst, DeviceVector<float> src, int64_t n);


void float_max_into(DeviceVector<float> dst, DeviceVector<float> src, int64_t n);
