// ColorOptim.cu -- linear-RGB Adam and the trust-region Adam variants for RGB and SH coefficients.
//
// Part of the Optimizer family -- see OptimizerCommon.cuh.

#include "kernels/optim/OptimizerCommon.cuh"

// ================
// Adam for linear RGB
// ================

__global__ void fused_adam_linear_rgb_optim_kernel(
    const int numel,
    float* __restrict__ rgbs,  // [N]
    const float* __restrict__ grad,  // [N]
    float* __restrict__ exp_avg,  // [N]
    float* __restrict__ exp_avg_sq,  // [N]
    const float lr,
    const float beta1,
    const float beta2,
    const float bias_correction1,
    const float bias_correction2,
    const float eps
) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx < numel) {
        // Bias correction terms
        // TODO: proper bias correction after densification
        const float step_size = lr / bias_correction1;
        
        // Load values
        float x = rgbs[idx];
        float v = grad[idx];
        float m_val = exp_avg[idx];
        float v_val = exp_avg_sq[idx];

        // Convert gradient to linear color space
        v /= SlangPixelWise::linear_rgb_to_srgb_grad(kSh0 * x + 0.5f);
        
        // Update biased first moment estimate: m_t = beta1 * m_{t-1} + (1 - beta1) * g_t
        m_val = beta1 * m_val + (1.0f - beta1) * v;
        
        // Update biased second raw moment estimate: v_t = beta2 * v_{t-1} + (1 - beta2) * g_t^2
        v_val = beta2 * v_val + (1.0f - beta2) * v * v;
        
        // Compute update: theta_t = theta_{t-1} - step_size * m_t / (sqrt(v_t / bias_correction2) + eps)
        float denom = sqrtf(v_val / bias_correction2) + eps;
        x = x - step_size * (m_val / denom);
        
        // Write back
        rgbs[idx] = x;
        exp_avg[idx] = m_val;
        exp_avg_sq[idx] = v_val;
    }
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    const int numel = (int)_tv_numel(param);
    if (numel == 0)
        return;

    fused_adam_linear_rgb_optim_kernel<<<_LAUNCH_ARGS_1D(numel, 256)>>>(
        numel,
        _tv_f(param),
        _tv_f(grad),
        _tv_f(exp_avg),
        _tv_f(exp_avg_sq),
        lr,
        beta1,
        beta2,
        1.0f - powf(beta1, step),
        1.0f - powf(beta2, step),
        eps
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Trust Region Adam for RGB
// ================

template<bool is_linear, bool zero_grad>
__global__ void fused_adamtr_rgb_optim_kernel(
    const int num_gs,
    float3* __restrict__ rgbs,  // [N, 3]
    float3* __restrict__ grad,  // [N, 3]
    float3* __restrict__ exp_avg,  // [N, 3]
    float3* __restrict__ exp_avg_sq,  // [N, 3]
    float* __restrict__ opacities,  // [N]
    const float lr,
    const float beta1,
    const float beta2,
    const float bias_correction1,
    const float bias_correction2,
    const float eps,
    const float eps_tr,
    const float grad_scale,
    const int step
) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < num_gs) {
        // Bias correction terms
        // TODO: proper bias correction after densification
        const float step_size = lr / bias_correction1;

        // Load values
        float3 x = rgbs[idx];
        float3 v = grad[idx];
        if constexpr (zero_grad) grad[idx] = make_float3(0.0f);
        v = grad_scale * v;
        float3 m_val = exp_avg[idx];
        float3 v_val = exp_avg_sq[idx];

        // Carry the gradient to x = splat_dc_encode(dc), the space Adam and
        // its moments live in when the working colour space is linear.
        if (is_linear) {
            v.x /= SlangPixelWise::linear_rgb_to_srgb_grad(kSh0 * x.x + 0.5f);
            v.y /= SlangPixelWise::linear_rgb_to_srgb_grad(kSh0 * x.y + 0.5f);
            v.z /= SlangPixelWise::linear_rgb_to_srgb_grad(kSh0 * x.z + 0.5f);
        }

        // Update momentum
        m_val = beta1 * m_val + (1.0f - beta1) * v;
        v_val = beta2 * v_val + (1.0f - beta2) * v * v;

        // Compute delta
        float3 denom = sqrtf(v_val / bias_correction2) + eps;
        float3 delta = -step_size * (m_val / denom);

        // Trust region. splat_dc_encode already scales the step by brightness,
        // so the linear path clips a bare radius (the 2 makes it identical to
        // the colour-proportional one wherever that binds).
        float opac = opacities[idx];
        opac = sigmoid(opac);
        float3 clip;
        if (is_linear) {
            clip = make_float3(kSh0 * sqrtf(2.0f * eps_tr / fmaxf(opac, 1e-12f)));
        } else {
            float3 rgb = fmaxf(kSh0 * x + 0.5f, (1.0f/255.0f)*(1.0f/255.0f));
            clip = kSh0 * sqrtf(4.0f * eps_tr * rgb / fmaxf(opac, 1e-12f));
        }

        // clip and update
        delta.x = fminf(fmaxf(delta.x, -clip.x), clip.x);
        delta.y = fminf(fmaxf(delta.y, -clip.y), clip.y);
        delta.z = fminf(fmaxf(delta.z, -clip.z), clip.z);
        delta.x = isfinite(delta.x) ? delta.x : 0.0f;
        delta.y = isfinite(delta.y) ? delta.y : 0.0f;
        delta.z = isfinite(delta.z) ? delta.z : 0.0f;
        if (is_linear) {
            rgbs[idx] = make_float3(
                SlangPixelWise::splat_dc_decode(SlangPixelWise::splat_dc_encode(x.x) + delta.x),
                SlangPixelWise::splat_dc_decode(SlangPixelWise::splat_dc_encode(x.y) + delta.y),
                SlangPixelWise::splat_dc_decode(SlangPixelWise::splat_dc_encode(x.z) + delta.z));
        } else {
            rgbs[idx] = x + delta;
        }
        exp_avg[idx] = m_val;
        exp_avg_sq[idx] = v_val;
    }
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    const int num_gs = (int)(_tv_numel(param) / 3);
    if (num_gs == 0)
        return;

    auto kfn = zero_grad ? fused_adamtr_rgb_optim_kernel<true,  true>
                         : fused_adamtr_rgb_optim_kernel<true,  false>;
    kfn<<<_LAUNCH_ARGS_1D(num_gs, 256)>>>(
        num_gs,
        (float3*)_tv_f(param),
        (float3*)_tv_f(grad),
        (float3*)_tv_f(exp_avg),
        (float3*)_tv_f(exp_avg_sq),
        _tv_f(opacities),
        lr,
        beta1,
        beta2,
        1.0f - powf(beta1, step),
        1.0f - powf(beta2, step),
        eps,
        eps_tr,
        grad_scale,
        step
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    const int num_gs = (int)(_tv_numel(param) / 3);
    if (num_gs == 0)
        return;

    auto kfn = zero_grad ? fused_adamtr_rgb_optim_kernel<false, true>
                         : fused_adamtr_rgb_optim_kernel<false, false>;
    kfn<<<_LAUNCH_ARGS_1D(num_gs, 256)>>>(
        num_gs,
        (float3*)_tv_f(param),
        (float3*)_tv_f(grad),
        (float3*)_tv_f(exp_avg),
        (float3*)_tv_f(exp_avg_sq),
        _tv_f(opacities),
        lr,
        beta1,
        beta2,
        1.0f - powf(beta1, step),
        1.0f - powf(beta2, step),
        eps,
        eps_tr,
        grad_scale,
        step
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Trust Region Adam for linear RGB, SH coefficients
// ================

template<bool is_linear, bool zero_grad>
__global__ void fused_adamtr_rgb_sh_optim_kernel(
    const int64_t num_params,
    const int num_sh,
    float* __restrict__ param,  // [N, K, 3]
    float* __restrict__ grad,  // [N, K, 3]
    float* __restrict__ exp_avg,  // [N, K, 3]
    float* __restrict__ exp_avg_sq,  // [N, K, 3]
    float* __restrict__ rgbs,  // [N, 3]
    float* __restrict__ opacities,  // [N]
    const float lr,
    const float beta1,
    const float beta2,
    const float bias_correction1,
    const float bias_correction2,
    const float eps,
    const float eps_tr,
    const float grad_scale,
    const int step
) {
    const int64_t idx = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < num_params) {
        // Bias correction terms
        // TODO: proper bias correction after densification
        const float step_size = lr / bias_correction1;

        // Load values
        // float x = param[idx];
        float v = grad[idx];
        if constexpr (zero_grad) grad[idx] = 0.0f;
        v = grad_scale * v;
        float m_val = exp_avg[idx];
        float v_val = exp_avg_sq[idx];

        // SH stays in coefficient space -- the DC reparameterization does not
        // extend here, because linearizing it about the DC colour needs
        // |sh| << c, which 3DGS does not satisfy.
        float c = rgbs[(idx / (3*num_sh)) * 3 + (idx % 3)] * kSh0 + 0.5f;
        if (is_linear)
            v /= SlangPixelWise::linear_rgb_to_srgb_grad(c);

        // Update momentum
        m_val = beta1 * m_val + (1.0f - beta1) * v;
        v_val = beta2 * v_val + (1.0f - beta2) * v * v;

        // Compute delta
        float denom = sqrtf(v_val / bias_correction2) + eps;
        float delta = -step_size * (m_val / denom);

        // Compute trust region
        float opac = opacities[idx / (3*num_sh)];
        opac = sigmoid(opac);
        c = fmaxf(c, (1.0f/255.0f)*(1.0f/255.0f));
        float clip = kSh0 * sqrtf(4.0f * eps_tr * c / fmaxf(opac, 1e-12f));

        // clip and update
        delta = fminf(fmaxf(delta, -clip), clip);
        param[idx] += delta;
        exp_avg[idx] = m_val;
        exp_avg_sq[idx] = v_val;
    }
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    int64_t colors_numel = _tv_numel(colors);
    const int64_t num_gs = colors_numel / 3;
    if (num_gs == 0)
        return;
    const int num_sh = (int)(_tv_numel(param) / colors_numel);
    if (num_sh == 0)
        return;
    const int64_t num_params = num_gs * num_sh * 3;

    auto kfn = zero_grad ? fused_adamtr_rgb_sh_optim_kernel<true,  true>
                         : fused_adamtr_rgb_sh_optim_kernel<true,  false>;
    kfn<<<_LAUNCH_ARGS_1D(num_params, 256)>>>(
        num_params,
        num_sh,
        _tv_f(param),
        _tv_f(grad),
        _tv_f(exp_avg),
        _tv_f(exp_avg_sq),
        _tv_f(colors),
        _tv_f(opacities),
        lr,
        beta1,
        beta2,
        1.0f - powf(beta1, step),
        1.0f - powf(beta2, step),
        eps,
        eps_tr,
        grad_scale,
        step
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    int64_t colors_numel = _tv_numel(colors);
    const int64_t num_gs = colors_numel / 3;
    if (num_gs == 0)
        return;
    const int num_sh = (int)(_tv_numel(param) / colors_numel);
    if (num_sh == 0)
        return;
    const int64_t num_params = num_gs * num_sh * 3;

    auto kfn = zero_grad ? fused_adamtr_rgb_sh_optim_kernel<false, true>
                         : fused_adamtr_rgb_sh_optim_kernel<false, false>;
    kfn<<<_LAUNCH_ARGS_1D(num_params, 256)>>>(
        num_params,
        num_sh,
        _tv_f(param),
        _tv_f(grad),
        _tv_f(exp_avg),
        _tv_f(exp_avg_sq),
        _tv_f(colors),
        _tv_f(opacities),
        lr,
        beta1,
        beta2,
        1.0f - powf(beta1, step),
        1.0f - powf(beta2, step),
        eps,
        eps_tr,
        grad_scale,
        step
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



__global__ void increment_int32_kernel(int32_t* __restrict__ data, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) data[idx] += 1;
}

/*[AutoHeaderGeneratorExport]*/
void increment_int32_inplace(DeviceVector<int32_t> data, int64_t n) {
    if (n == 0 || data.data_ptr() == nullptr) return;
    increment_int32_kernel<<<(n + 255) / 256, 256>>>(data.data_ptr(), n);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// dst[i] += src[i] for i in [0, n).
__global__ void float_add_into_kernel(float* __restrict__ dst,
                                      const float* __restrict__ src,
                                      int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) dst[idx] += src[idx];
}

/*[AutoHeaderGeneratorExport]*/
void float_add_into(DeviceVector<float> dst, DeviceVector<float> src, int64_t n) {
    if (n == 0 || dst.data_ptr() == nullptr || src.data_ptr() == nullptr) return;
    float_add_into_kernel<<<(n + 255) / 256, 256>>>(dst.data_ptr(), src.data_ptr(), n);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

// dst[i] = max(dst[i], src[i]). Folds per-sub-batch accum_weight the way
// raster_bwd folds a batch's cameras (atomicMax): a sum would score a splat
// by how many cameras saw it rather than by its error.
__global__ void float_max_into_kernel(float* __restrict__ dst,
                                      const float* __restrict__ src,
                                      int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) dst[idx] = fmaxf(dst[idx], src[idx]);
}

/*[AutoHeaderGeneratorExport]*/
void float_max_into(DeviceVector<float> dst, DeviceVector<float> src, int64_t n) {
    if (n == 0 || dst.data_ptr() == nullptr || src.data_ptr() == nullptr) return;
    float_max_into_kernel<<<(n + 255) / 256, 256>>>(dst.data_ptr(), src.data_ptr(), n);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
