// FusedAppearanceOptim.cu -- fused appearance optimizer, including the quantized-SH codecs.
//
// Part of the Optimizer family -- see OptimizerCommon.cuh.

#include "kernels/optim/OptimizerCommon.cuh"

// ================
// Fused appearance optimizer
// ================

template<bool zero_grad>
__global__ void fused_adam_with_steps_kernel(
    float* __restrict__ param,
    float* __restrict__ grad,
    float* __restrict__ exp_avg,
    float* __restrict__ exp_avg_sq,
    const float lr,
    const int32_t scalar_step,
    const int32_t* __restrict__ steps,
    const float decay,
    const float decay_offset,
    const float grad_scale,
    const int64_t numel,
    const int stride
) {
    static constexpr float eps = 1e-15f;
    static constexpr float beta1 = 0.9f;
    static constexpr float beta2 = 0.999f;

    const int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx >= numel) return;

    float step = (float)(steps ? steps[idx / stride] : scalar_step);
    float inv_bias_correction1 = 1.0f / (1.0f - powf(beta1, step));
    float inv_bias_correction2 = 1.0f / (1.0f - powf(beta2, step));

    float x = param[idx];
    float v = grad[idx];
    if (!isfinite(v))
        v = 0.0f;
    if constexpr (zero_grad)
        grad[idx] = 0.0f;
    v *= grad_scale;
    v += decay * (fmaxf(x - decay_offset, 0.0f) + fminf(x + decay_offset, 0.0f));
    float g1 = exp_avg[idx];
    float g2 = exp_avg_sq[idx];
    // Self-heal: a moment that is already non-finite would otherwise pin this
    // parameter for the rest of the run.
    if (!isfinite(g1)) g1 = 0.0f;
    if (!isfinite(g2)) g2 = 0.0f;

    g1 = beta1 * g1 + (1.0f - beta1) * v;
    g2 = beta2 * g2 + (1.0f - beta2) * v * v;

    float x_new = x - lr * inv_bias_correction1 * g1 /
                          (sqrtf(g2 * inv_bias_correction2) + eps);

    // For the per-photo appearance tables (PPISP, bilagrid) one non-finite
    // entry poisons every pixel of that photo for the rest of the run, and
    // nothing restores it. Drop the step instead.
    if (isfinite(x_new)) param[idx] = x_new;
    if (isfinite(g1)) exp_avg[idx] = g1;
    if (isfinite(g2)) exp_avg_sq[idx] = g2;
}

/*[AutoHeaderGeneratorExport]*/
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
    int stride = (int)(param_numel / num_splats);

    auto kfn = zero_grad ? fused_adam_with_steps_kernel<true>
                         : fused_adam_with_steps_kernel<false>;
    kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, 256)>>>(
        param.data_ptr(),
        grad.data_ptr(),
        exp_avg.data_ptr(),
        exp_avg_sq.data_ptr(),
        lr,
        step, per_splat_steps.data_ptr(),
        2.0f*l2_reg/(float)(num_splats*stride),
        l2_reg_offset,
        grad_scale,
        num_splats*stride,
        stride
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// Unscheduled fp32 AdaGrad:
//     accum += g*g
//     x    -= lr * g / (sqrt(accum) + eps)
// Matches torch.optim.Adagrad(lr_decay=0, weight_decay=0,
// initial_accumulator_value=0, eps=1e-15).
__global__ void fused_adagrad_kernel(
    float* __restrict__ param,
    const float* __restrict__ grad,
    float* __restrict__ accum,
    const float lr,
    const int64_t numel
) {
    static constexpr float eps = 1e-15f;
    const int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx >= numel) return;
    float v = grad[idx];
    if (!isfinite(v)) v = 0.0f;
    float a0 = accum[idx];
    if (!isfinite(a0)) a0 = 0.0f;   // self-heal, see fused_adam_with_steps_kernel
    float a = a0 + v * v;
    if (!isfinite(a)) a = a0;
    // See fused_adam_with_steps_kernel: a finite parameter must not become
    // non-finite, or the photo it corrects renders NaN for the rest of the run.
    float x_new = param[idx] - lr * v / (sqrtf(a) + eps);
    if (isfinite(x_new)) param[idx] = x_new;
    accum[idx] = a;
}

/*[AutoHeaderGeneratorExport]*/
void fused_adagrad_step(
    DeviceTensorFloatND param,
    DeviceTensorFloatND grad,
    DeviceTensorFloatND accum,
    float lr
) {
    int64_t numel = param.numel();
    if (numel == 0) return;
    fused_adagrad_kernel<<<_LAUNCH_ARGS_1D(numel, 256)>>>(
        param.data_ptr(),
        grad.data_ptr(),
        accum.data_ptr(),
        lr,
        numel
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


template<int BLOCK_SIZE, int QUANT_BITS, bool zero_grad>
__global__ void fused_adam_with_steps_8bit_kernel(
    float* __restrict__ param,
    float* __restrict__ grad,
    uint8_t* __restrict__ packed,       // AoS (u, sqrt_g2) packed cells
    float4* __restrict__ quant_bounds,  // (u_min, u_max, sqrt_g2_min, sqrt_g2_max)
    const float lr,
    const int32_t scalar_step,
    const int32_t* __restrict__ steps,
    const float decay,
    const float decay_offset,
    const float grad_scale,
    const int64_t numel,
    const int stride
) {
    using QState = QuantizedAdamState<QUANT_BITS, BLOCK_SIZE>;
    static constexpr float eps = 1e-15f;
    static constexpr float beta1 = 0.9f;
    static constexpr float beta2 = 0.999f;

    const int64_t idx = (int64_t)blockIdx.x * BLOCK_SIZE + (int64_t)threadIdx.x;
    bool inside = (idx < numel);

    float step = (float)(steps ? steps[idx / stride] : scalar_step);
    float inv_bias_correction1 = 1.0f / (1.0f - powf(beta1, step));
    float inv_bias_correction2 = 1.0f / (1.0f - powf(beta2, step));

    float x = inside ? param[idx] : 0.0f;
    float v = inside ? grad[idx] : 0.0f;
    if (!isfinite(v))
        v = 0.0f;
    if constexpr (zero_grad) {
        if (inside) grad[idx] = 0.0f;
    }
    v *= grad_scale;
    v += decay * (fmaxf(x - decay_offset, 0.0f) + fminf(x + decay_offset, 0.0f));

    // Decode joint (u, sqrt(g2)) via QuantizedAdamState codec.
    float4 mm = quant_bounds[blockIdx.x];
    float g1, g2;
    if (inside) {
        float2 g1g2 = QState::decode_g1g2(packed, idx, mm);
        g1 = g1g2.x;
        g2 = g1g2.y;
    } else {
        g1 = 0.0f;
        g2 = 0.0f;
    }

    g1 = beta1 * g1 + (1.0f - beta1) * v;
    g2 = beta2 * g2 + (1.0f - beta2) * v*v;

    x -= lr * inv_bias_correction1 * g1 / (sqrtf(g2 * inv_bias_correction2) + eps);
    param[idx] = x;

    // Re-encode the new Adam state in the (u, sqrt(g2)) basis.
    float2 us_new = QState::g1g2_to_us(g1, g2);
    float u_new       = us_new.x;
    float sqrt_g2_new = us_new.y;

    cg::thread_block block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
    mm = inside ? float4{u_new, u_new, sqrt_g2_new, sqrt_g2_new}
                : float4{1e30f, -1e30f, 1e30f, -1e30f};
    mm.x = cg::reduce(warp, mm.x, cg::less<float>());
    mm.y = cg::reduce(warp, mm.y, cg::greater<float>());
    mm.z = cg::reduce(warp, mm.z, cg::less<float>());
    mm.w = cg::reduce(warp, mm.w, cg::greater<float>());
    __shared__ float4 shared_reduce[BLOCK_SIZE / WARP_SIZE];
    if (threadIdx.x % WARP_SIZE == 0)
        shared_reduce[threadIdx.x / WARP_SIZE] = mm;
    __syncthreads();
    mm = (threadIdx.x < BLOCK_SIZE / WARP_SIZE) ?
        shared_reduce[threadIdx.x] : float4{1e30f, -1e30f, 1e30f, -1e30f};
    mm.x = cg::reduce(warp, mm.x, cg::less<float>());
    mm.y = cg::reduce(warp, mm.y, cg::greater<float>());
    mm.z = cg::reduce(warp, mm.z, cg::less<float>());
    mm.w = cg::reduce(warp, mm.w, cg::greater<float>());
    __syncthreads();
    if (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        shared_reduce[threadIdx.x] = mm;
    __syncthreads();
    mm = shared_reduce[threadIdx.x / WARP_SIZE];

    if (inside) {
        QState::encode_us(packed, idx, u_new, sqrt_g2_new, mm);
    }

    if (threadIdx.x == 0)
        quant_bounds[blockIdx.x] = mm;
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    int64_t param_numel = param.numel();
    if (param_numel == 0 || num_splats == 0)
        return;
    int stride = (int)(param_numel / num_splats);
    constexpr int BLOCK_SIZE = 256;

    #define _ARGS_TAIL \
        param.data_ptr(), grad.data_ptr(), packed, quant_bounds, \
        lr, step, per_splat_steps.data_ptr(), \
        2.0f*l2_reg/(float)(num_splats*stride), l2_reg_offset, \
        grad_scale, \
        num_splats*stride, stride

    if (bits == 4) {
        auto kfn = zero_grad ? fused_adam_with_steps_8bit_kernel<BLOCK_SIZE, 4, true>
                             : fused_adam_with_steps_8bit_kernel<BLOCK_SIZE, 4, false>;
        kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, BLOCK_SIZE)>>>(_ARGS_TAIL);
    } else if (bits == 8) {
        auto kfn = zero_grad ? fused_adam_with_steps_8bit_kernel<BLOCK_SIZE, 8, true>
                             : fused_adam_with_steps_8bit_kernel<BLOCK_SIZE, 8, false>;
        kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, BLOCK_SIZE)>>>(_ARGS_TAIL);
    } else {
        throw std::runtime_error(
            "fused_adam_step_quantized: bits must be 4 or 8, got " +
            std::to_string(bits));
    }
    #undef _ARGS_TAIL
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Doubly-quantized fused Adam for SH features (non-FPBO).
// Quantizes BOTH the optimizer state (g1, g2) AND the parameter value (sh
// coeffs) against per-CELL-block (256 cells per block) min/max bounds:
//
//   * Optimizer state -- QuantizedAdamState<OPTIM_BITS, 256> in (u, sqrt_g2)
//     basis, one float4 bound per block (u_min, u_max, sqrt_g2_min, sqrt_g2_max).
//   * Parameter value -- QuantizedTensor<VALUE_BITS, 256>, one float2 bound
//     per block (v_min, v_max).
//
// FPBO already handles this combo per-splat-block; this kernel mirrors the
// math but indexes per-cell-block, matching the existing non-FPBO Adam
// kernel's launch geometry. Used by engine_optim_step when sh_value_bits != 32
// and use_fused_proj_bwd_optim is off (no fp32 features_sh exists then;
// the packed buffer IS the canonical storage).
// ================

template<int BLOCK_SIZE, int OPTIM_BITS, int VALUE_BITS, bool zero_grad>
__global__ void fused_adam_with_steps_qq_kernel(
    float* __restrict__ grad,           // fp32 grad (dense); null when grad-quant on
    // Block-wise QUANTIZED SH grad (non-FPBO grad-quant path). When
    // grad_q_packed != null the grad is decoded from the 8-bit codec (cell idx,
    // per-splat-block bound at (idx/stride)/256) instead of grad[idx].
    const uint8_t* __restrict__ grad_q_packed,
    const float2*  __restrict__ grad_q_bounds,
    uint8_t* __restrict__ optim_packed, // optim-state packed (u, sqrt_g2)
    float4*  __restrict__ optim_bounds, // per-cell-block float4 (u_mm, sqrt_g2_mm)
    uint8_t* __restrict__ value_packed, // parameter-value packed
    float2*  __restrict__ value_bounds, // per-cell-block float2 (v_min, v_max)
    const float lr,
    const int32_t scalar_step,
    const int32_t* __restrict__ steps,
    const float decay,
    const float decay_offset,
    const float grad_scale,
    const int64_t numel,
    const int stride
) {
    using QState     = QuantizedAdamState<OPTIM_BITS, BLOCK_SIZE>;
    using QValue     = QuantizedTensor<VALUE_BITS, BLOCK_SIZE>;
    static constexpr float eps   = 1e-15f;
    static constexpr float beta1 = 0.9f;
    static constexpr float beta2 = 0.999f;

    const int64_t idx = (int64_t)blockIdx.x * BLOCK_SIZE + (int64_t)threadIdx.x;
    bool inside = (idx < numel);

    float step = (float)(steps ? steps[idx / stride] : scalar_step);
    float inv_bias_correction1 = 1.0f / (1.0f - powf(beta1, step));
    float inv_bias_correction2 = 1.0f / (1.0f - powf(beta2, step));

    // Decode old value from packed bytes against the block's old bound. Skip
    // the load for out-of-range threads (they pass sentinels through the
    // reduction so the bound only reflects inside threads' values).
    float2 value_mm_old = inside ? value_bounds[blockIdx.x] : float2{0.f, 0.f};
    float x = inside ? QValue::decode_v(value_packed, idx, value_mm_old) : 0.0f;

    float v;
    if (grad_q_packed) {
        float2 gmm = inside ? grad_q_bounds[(idx / stride) / 256] : float2{0.f, 0.f};
        v = inside ? gradq::Codec<8>::decode1(grad_q_packed, idx, gmm) : 0.0f;
    } else {
        v = inside ? grad[idx] : 0.0f;
    }
    if (!isfinite(v))
        v = 0.0f;
    if constexpr (zero_grad) {
        if (inside && grad) grad[idx] = 0.0f;   // quantized grad is zeroed via _alloc_grad_buffers
    }
    v *= grad_scale;
    v += decay * (fmaxf(x - decay_offset, 0.0f) + fminf(x + decay_offset, 0.0f));

    // Decode joint (u, sqrt_g2) for Adam state.
    float4 optim_mm = optim_bounds[blockIdx.x];
    float g1 = 0.0f, g2 = 0.0f;
    if (inside) {
        float2 g1g2 = QState::decode_g1g2(optim_packed, idx, optim_mm);
        g1 = g1g2.x;
        g2 = g1g2.y;
    }

    g1 = beta1 * g1 + (1.0f - beta1) * v;
    g2 = beta2 * g2 + (1.0f - beta2) * v * v;

    x -= lr * inv_bias_correction1 * g1 / (sqrtf(g2 * inv_bias_correction2) + eps);

    // ---- value bounds block-reduce (float2 min/max) ----
    float2 value_mm_new = inside ? float2{x, x} : float2{1e30f, -1e30f};

    cg::thread_block       block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
    value_mm_new.x = cg::reduce(warp, value_mm_new.x, cg::less<float>());
    value_mm_new.y = cg::reduce(warp, value_mm_new.y, cg::greater<float>());
    __shared__ float2 shared_reduce_v[BLOCK_SIZE / WARP_SIZE];
    if (threadIdx.x % WARP_SIZE == 0)
        shared_reduce_v[threadIdx.x / WARP_SIZE] = value_mm_new;
    __syncthreads();
    value_mm_new = (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        ? shared_reduce_v[threadIdx.x] : float2{1e30f, -1e30f};
    value_mm_new.x = cg::reduce(warp, value_mm_new.x, cg::less<float>());
    value_mm_new.y = cg::reduce(warp, value_mm_new.y, cg::greater<float>());
    __syncthreads();
    if (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        shared_reduce_v[threadIdx.x] = value_mm_new;
    __syncthreads();
    value_mm_new = shared_reduce_v[threadIdx.x / WARP_SIZE];

    // ---- optim state bounds block-reduce ((u, sqrt_g2) -> float4) ----
    float2 us_new = QState::g1g2_to_us(g1, g2);
    float u_new = us_new.x, sqrt_g2_new = us_new.y;
    float4 optim_mm_new = inside
        ? float4{u_new, u_new, sqrt_g2_new, sqrt_g2_new}
        : float4{1e30f, -1e30f, 1e30f, -1e30f};
    optim_mm_new.x = cg::reduce(warp, optim_mm_new.x, cg::less<float>());
    optim_mm_new.y = cg::reduce(warp, optim_mm_new.y, cg::greater<float>());
    optim_mm_new.z = cg::reduce(warp, optim_mm_new.z, cg::less<float>());
    optim_mm_new.w = cg::reduce(warp, optim_mm_new.w, cg::greater<float>());
    __shared__ float4 shared_reduce_o[BLOCK_SIZE / WARP_SIZE];
    if (threadIdx.x % WARP_SIZE == 0)
        shared_reduce_o[threadIdx.x / WARP_SIZE] = optim_mm_new;
    __syncthreads();
    optim_mm_new = (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        ? shared_reduce_o[threadIdx.x]
        : float4{1e30f, -1e30f, 1e30f, -1e30f};
    optim_mm_new.x = cg::reduce(warp, optim_mm_new.x, cg::less<float>());
    optim_mm_new.y = cg::reduce(warp, optim_mm_new.y, cg::greater<float>());
    optim_mm_new.z = cg::reduce(warp, optim_mm_new.z, cg::less<float>());
    optim_mm_new.w = cg::reduce(warp, optim_mm_new.w, cg::greater<float>());
    __syncthreads();
    if (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        shared_reduce_o[threadIdx.x] = optim_mm_new;
    __syncthreads();
    optim_mm_new = shared_reduce_o[threadIdx.x / WARP_SIZE];

    // Encode + write back. Each inside thread encodes its cell; thread 0 writes
    // the per-block bounds.
    if (inside) {
        QValue::encode_v(value_packed, idx, x, value_mm_new);
        QState::encode_us(optim_packed, idx, u_new, sqrt_g2_new, optim_mm_new);
    }
    if (threadIdx.x == 0) {
        value_bounds[blockIdx.x] = value_mm_new;
        optim_bounds[blockIdx.x] = optim_mm_new;
    }
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    if (param_numel == 0 || num_splats == 0)
        return;
    int stride = (int)(param_numel / num_splats);
    constexpr int BLOCK_SIZE = 256;

    #define _ARGS_TAIL_QQ \
        grad.data_ptr(), grad_q_packed, grad_q_bounds, optim_packed, optim_bounds, value_packed, value_bounds, \
        lr, step, per_splat_steps.data_ptr(), \
        2.0f*l2_reg/(float)(num_splats*stride), l2_reg_offset, \
        grad_scale, \
        num_splats*stride, stride

    // 4 instantiations: (optim_bits in {4, 8}) x (value_bits in {8, 16}) x
    // (zero_grad in {false, true}). Throw on any other combination.
    if (optim_bits == 4 && value_bits == 8) {
        auto kfn = zero_grad ? fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 4, 8,  true>
                             : fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 4, 8,  false>;
        kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, BLOCK_SIZE)>>>(_ARGS_TAIL_QQ);
    } else if (optim_bits == 4 && value_bits == 16) {
        auto kfn = zero_grad ? fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 4, 16, true>
                             : fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 4, 16, false>;
        kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, BLOCK_SIZE)>>>(_ARGS_TAIL_QQ);
    } else if (optim_bits == 8 && value_bits == 8) {
        auto kfn = zero_grad ? fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 8, 8,  true>
                             : fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 8, 8,  false>;
        kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, BLOCK_SIZE)>>>(_ARGS_TAIL_QQ);
    } else if (optim_bits == 8 && value_bits == 16) {
        auto kfn = zero_grad ? fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 8, 16, true>
                             : fused_adam_with_steps_qq_kernel<BLOCK_SIZE, 8, 16, false>;
        kfn<<<_LAUNCH_ARGS_1D(num_splats*stride, BLOCK_SIZE)>>>(_ARGS_TAIL_QQ);
    } else {
        throw std::runtime_error(
            "fused_adam_step_quantized_value: optim_bits in {4, 8} and "
            "value_bits in {8, 16}; got optim_bits=" + std::to_string(optim_bits) +
            ", value_bits=" + std::to_string(value_bits));
    }
    #undef _ARGS_TAIL_QQ
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



