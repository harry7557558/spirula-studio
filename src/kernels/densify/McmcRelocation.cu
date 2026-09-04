// McmcRelocation.cu -- MCMC relocation (probabilities, index map, update) and MCMC noise injection.
//
// Part of the Densify family -- see DensifyCommon.cuh.

#include "kernels/densify/DensifyQuantCopy.cuh"

// ================
// MCMC Relocation
// ================

// largely from https://github.com/harry7557558/vksplat/blob/main/vksplat/shaders/mcmc.slang

inline __device__ float log_factorial(float x) {
    // least squares for x = [0, 1... 50]
    // absolute error: max 4.6e-5, L1 9.3e-6
    // notebook: https://www.desmos.com/calculator/mik6cz7h5v
    static const float kA = 0.996763591291f;
    static const float kH = 2.73507778369f;
    static const float kB = 0.978531458767f;
    static const float kC2 = -0.0400131099548f;
    static const float kC1 = -0.596969060666f;
    static const float kD3 = 0.00251628691783f;
    static const float kD2 = 0.0959712063178f;
    static const float kD1 = 0.803207449544f;
    return (kA * __logf(x + kH) - kB +
        (kC2 * x + kC1) / (((kD3 * x + kD2) * x + kD1) * x + 1.0f)
    ) * x;
}

inline __device__ float binom(float n, float k) {
    return __expf(log_factorial(n) - log_factorial(k) - log_factorial(n-k));
}

inline __device__ void mcmc_relocation(float& opacity, float3& scale, int n_idx) {
    n_idx = min(n_idx, 50);  // log_factorial only fits to 50

    opacity = sigmoid(opacity);

    float new_opacity = 1.0f - powf(1.0f-opacity, 1.0f / n_idx);

    float denom_sum = 0.0f;
    for (int i = 1; i <= n_idx; ++i) {
        for (int k = 0; k <= (i - 1); ++k) {
            denom_sum += binom(i-1, k) *
                (cosf((float)M_PI*k) / sqrtf(k+1)) *  // (-1)^k / sqrt(k+1)
                powf(new_opacity, k+1);
        }
    }
    // log(coeff * exp(s)) == log(coeff) + s, kept in log space: the round trip
    // out and back flushes a splat near FLT_MIN to -inf, which per_splat_losses
    // then turns into a NaN (shaders/per_splat_losses.slang).
    float log_coeff = __logf(opacity / denom_sum);

    opacity = logit(new_opacity);
    scale = make_float3(scale.x + log_coeff, scale.y + log_coeff,
                        scale.z + log_coeff);
}


__global__ void mcmc_compute_relocation_probabilities_kernel(
    uint32_t num_splats,
    float min_opacity,
    const float* __restrict__ opacs,
    const float3* __restrict__ scales,
    float* __restrict__ probs
) {
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_splats)
        return;

    float opac = sigmoid(opacs[tid]);

    if (opac <= min_opacity || !isfinite(opac) ||
        SlangDensify::splat_scale_is_dead(scales[tid]))
        opac = 0.0f;

    probs[tid] = opac;
}


__global__ void mcmc_compute_relocation_index_map_kernel(
    float* __restrict__ sample_probs,  // [N]
    float* __restrict__ sample_probs_cumsum,  // [N]
    int32_t* __restrict__ index_map,  // [N]
    int32_t* __restrict__ n_idx_buffer,  // [N]
    uint32_t numel,
    uint32_t seed
) {
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= numel)
        return;

    float prob = sample_probs[tid];

    // not relocated, put same ID
    if (prob != 0.0f) {
        index_map[tid] = tid;
        return;
    }

    // binary search for random index
    // find the min idx such that sample_probs_cumsum[idx] >= rand
    float rand_max = sample_probs_cumsum[numel-1];
    float rand = rand_max * hash_uint3(seed, blockIdx.x, threadIdx.x) * exp2f(-32.0f);
    uint32_t lo = 0;
    uint32_t hi = numel - 1;
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        if (sample_probs_cumsum[mid] < rand)
            lo = mid + 1;
        else
            hi = mid;
    }
    index_map[tid] = lo;

    atomicAdd(&n_idx_buffer[lo], 1);
}


template<typename g_features_sh_t3>
__global__ void mcmc_compute_relocation_kernel(
    uint32_t num_splats,
    float min_opacity,
    int32_t* __restrict__ n_idx_buffer,  // [N]
    float3*__restrict__ means, float3*__restrict__ g1_means, float3*__restrict__ g2_means,
    float4*__restrict__ quats, float4*__restrict__ g1_quats, float4*__restrict__ g2_quats,
    float3*__restrict__ scales, float3*__restrict__ g1_scales, float3*__restrict__ g2_scales,
    float*__restrict__ opacs, float*__restrict__ g1_opacs, float*__restrict__ g2_opacs,
    float3*__restrict__ features_dc, float3*__restrict__ g1_features_dc, float3*__restrict__ g2_features_dc,
    int num_sh, float3*__restrict__ features_sh, g_features_sh_t3*__restrict__ g1_features_sh, g_features_sh_t3*__restrict__ g2_features_sh,
    const float4* __restrict__ sh_quant_bounds,
    bool sh_bounds_per_splat,
    NonShQuantState non_sh,
    int32_t* __restrict__ bias_correction_steps
) {
    uint32_t cur_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (cur_idx >= num_splats)
        return;

    // only compute on indices being relocated to
    uint n_idx = n_idx_buffer[cur_idx] + 1;
    if (n_idx == 1)
        return;

    // compute relocation
    float3 scale = scales[cur_idx];
    float opac = opacs[cur_idx];
    mcmc_relocation(opac, scale, int(n_idx));
    scales[cur_idx] = scale;
    opacs[cur_idx] = opac;

    // set grad to zero (skip non-SH fp32 writes when those buffers are
    // freed by FPBO non-SH Adam-state quantization; see relocate kernel).
    if (g1_means) {
        g1_means[cur_idx] = make_float3(0.0f);
        g2_means[cur_idx] = make_float3(0.0f);
        g1_quats[cur_idx] = make_float4(0.0f);
        g2_quats[cur_idx] = make_float4(0.0f);
        g1_scales[cur_idx] = make_float3(0.0f);
        g2_scales[cur_idx] = make_float3(0.0f);
        g1_opacs[cur_idx] = 0.0f;
        g2_opacs[cur_idx] = 0.0f;
        g1_features_dc[cur_idx] = make_float3(0.0f);
        g2_features_dc[cur_idx] = make_float3(0.0f);
    }
    if constexpr (sizeof(g_features_sh_t3) == sizeof(short3)) {
        _zero_quant_sh_for_splat<8>(
            (uint8_t*)g1_features_sh, cur_idx, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else if constexpr (sizeof(g_features_sh_t3) == sizeof(uchar3)) {
        _zero_quant_sh_for_splat<4>(
            (uint8_t*)g1_features_sh, cur_idx, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else {
        for (int i = 0; i < num_sh; ++i) {  // TODO: slow; more cache friendly way to do so?
            g1_features_sh[num_sh*cur_idx+i] = g_features_sh_t3{0,0,0};
            g2_features_sh[num_sh*cur_idx+i] = g_features_sh_t3{0,0,0};
        }
    }
    _zero_quant_non_sh_all(non_sh, cur_idx);
    if (bias_correction_steps)
        bias_correction_steps[cur_idx] = 0;
}


__global__ void mcmc_update_relocation_kernel(
    uint32_t num_splats,
    int32_t* __restrict__ index_map,  // [N]
    float3*__restrict__ means,
    float4*__restrict__ quats,
    float3*__restrict__ scales,
    float*__restrict__ opacs,
    float3*__restrict__ features_dc,
    int num_sh, float3*__restrict__ features_sh
) {
    uint32_t id_dst = blockIdx.x * blockDim.x + threadIdx.x;
    if (id_dst >= num_splats)
        return;

    // only update on those relocated from
    uint32_t id_src = index_map[id_dst];
    if (id_src == id_dst)
        return;

    // copy components
    means[id_dst] = means[id_src];
    quats[id_dst] = quats[id_src];
    scales[id_dst] = scales[id_src];
    opacs[id_dst] = opacs[id_src];
    features_dc[id_dst] = features_dc[id_src];
    // features_sh may be null under SH-value quantization (canonical store is
    // the packed buffer; fp32 features_sh allocation is freed).
    if (features_sh) {
        for (int i = 0; i < num_sh; ++i)  // TODO: slow; more cache friendly way to do so?
            features_sh[num_sh*id_dst+i] = features_sh[num_sh*id_src+i];
    }
}


/*[AutoHeaderGeneratorExport]*/
void relocate_splats_mcmc_tensor(
    int64_t cur_num_splats,
    float min_opacity,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,
    NonShQuantState non_sh,
    uint32_t seed
) {
    int32_t* bias_correction_steps_ptr = bias_correction_steps.data_ptr();
    float* sample_probs = DevicePool::global().acquire<float>(
        PoolSlot::DensifyMcmcSampleProbs, cur_num_splats);
    mcmc_compute_relocation_probabilities_kernel<<<_LAUNCH_ARGS_1D(cur_num_splats, 256)>>>(
        cur_num_splats,
        min_opacity,
        opacs.data_ptr(),
        scales.data_ptr(),
        sample_probs
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // CUB inclusive sum for cumsum
    float* sample_probs_cumsum = DevicePool::global().acquire<float>(
        PoolSlot::DensifyMcmcSampleProbsCumsum, cur_num_splats);
    {
        size_t temp_bytes = 0;
        cub::DeviceScan::InclusiveSum(
            nullptr, temp_bytes,
            sample_probs, sample_probs_cumsum, (int)cur_num_splats);
        void* temp = DeviceScratch::global().acquire(temp_bytes);
        cub::DeviceScan::InclusiveSum(
            temp, temp_bytes,
            sample_probs, sample_probs_cumsum, (int)cur_num_splats);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    int32_t* index_map = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyMcmcIndexMap, cur_num_splats);
    int32_t* n_idx_buffer = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyMcmcNIdxBuffer, cur_num_splats);
    cudaMemset(n_idx_buffer, 0, cur_num_splats * sizeof(int32_t));

    mcmc_compute_relocation_index_map_kernel<<<_LAUNCH_ARGS_1D(cur_num_splats, 256)>>>(
        sample_probs,
        sample_probs_cumsum,
        index_map,
        n_idx_buffer,
        cur_num_splats,
        seed
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    #define _DENSIFY_MCMC_RELOC_LAUNCH(T, bnd_ptr, bps) \
        mcmc_compute_relocation_kernel<T><<<_LAUNCH_ARGS_1D(cur_num_splats, 64)>>>( \
            cur_num_splats, \
            min_opacity, \
            n_idx_buffer, \
            means.data_ptr(), g1_means.data_ptr(), g2_means.data_ptr(), \
            quats.data_ptr(), g1_quats.data_ptr(), g2_quats.data_ptr(), \
            scales.data_ptr(), g1_scales.data_ptr(), g2_scales.data_ptr(), \
            opacs.data_ptr(), g1_opacs.data_ptr(), g2_opacs.data_ptr(), \
            features_dc.data_ptr(), g1_features_dc.data_ptr(), g2_features_dc.data_ptr(), \
            num_sh, features_sh.data_ptr(), (T*)g1_features_sh.data_ptr(), (T*)g2_features_sh.data_ptr(), \
            bnd_ptr, bps, \
            non_sh, \
            bias_correction_steps_ptr)
    if      (sh_optim_bits == 32) _DENSIFY_MCMC_RELOC_LAUNCH(float3, nullptr, false);
    else if (sh_optim_bits == 8)  _DENSIFY_MCMC_RELOC_LAUNCH(short3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
    else if (sh_optim_bits == 4)  _DENSIFY_MCMC_RELOC_LAUNCH(uchar3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
    #undef _DENSIFY_MCMC_RELOC_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    mcmc_update_relocation_kernel<<<_LAUNCH_ARGS_1D(cur_num_splats, 64)>>>(
        cur_num_splats,
        index_map,
        means.data_ptr(),
        quats.data_ptr(),
        scales.data_ptr(),
        opacs.data_ptr(),
        features_dc.data_ptr(),
        num_sh, features_sh.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    _launch_densify_copy_quant_sh_value_index_map(
        cur_num_splats, index_map,
        sh_value_packed.data_ptr(), sh_value_bounds.data_ptr(),
        num_sh, num_sh_buffer, sh_value_bits, sh_value_bounds_per_splat);
}



__global__ void mcmc_compute_add_index_map_kernel(
    float* __restrict__ sample_probs_cumsum,  // [N]
    int32_t* __restrict__ index_map,  // [dN]
    int32_t* __restrict__ n_idx_buffer,  // [N]
    uint32_t num_splats,
    uint32_t num_add,
    uint32_t seed
) {
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_add)
        return;

    // binary search for random index
    // find the min idx such that sample_probs_cumsum[idx] >= rand
    float rand_max = sample_probs_cumsum[num_splats-1];
    float rand = rand_max * hash_uint3(seed, blockIdx.x, threadIdx.x) * exp2f(-32.0f);
    uint32_t lo = 0;
    uint32_t hi = num_splats - 1;
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        if (sample_probs_cumsum[mid] <= rand)
            lo = mid + 1;
        else
            hi = mid;
    }
    index_map[tid] = lo;

    atomicAdd(&n_idx_buffer[lo], 1);
}


__global__ void mcmc_compute_add_kernel(
    uint32_t num_splats,
    float min_opacity,
    int32_t* __restrict__ n_idx_buffer,
    float3*__restrict__ scales,
    float*__restrict__ opacs
) {
    uint32_t cur_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (cur_idx >= num_splats)
        return;

    // only compute on indices being relocated to
    uint n_idx = n_idx_buffer[cur_idx] + 1;
    if (n_idx == 1)
        return;

    // compute relocation
    float3 scale = scales[cur_idx];
    float opac = opacs[cur_idx];
    mcmc_relocation(opac, scale, int(n_idx));
    scales[cur_idx] = scale;
    opacs[cur_idx] = opac;

    // don't change grad
}


template<typename g_features_sh_t3>
__global__ void mcmc_update_add_kernel(
    uint32_t num_splats,
    uint32_t num_add,
    int32_t* __restrict__ index_map,  // [dN]
    float3*__restrict__ means, float3*__restrict__ g1_means, float3*__restrict__ g2_means,
    float4*__restrict__ quats, float4*__restrict__ g1_quats, float4*__restrict__ g2_quats,
    float3*__restrict__ scales, float3*__restrict__ g1_scales, float3*__restrict__ g2_scales,
    float*__restrict__ opacs, float*__restrict__ g1_opacs, float*__restrict__ g2_opacs,
    float3*__restrict__ features_dc, float3*__restrict__ g1_features_dc, float3*__restrict__ g2_features_dc,
    int num_sh, float3*__restrict__ features_sh, g_features_sh_t3*__restrict__ g1_features_sh, g_features_sh_t3*__restrict__ g2_features_sh,
    const float4* __restrict__ sh_quant_bounds,
    bool sh_bounds_per_splat,
    NonShQuantState non_sh,
    int32_t* __restrict__ bias_correction_steps
) {
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_add)
        return;

    uint32_t id_dst = tid + num_splats;
    uint32_t id_src = index_map[tid];

    // copy components
    means[id_dst] = means[id_src];
    quats[id_dst] = quats[id_src];
    scales[id_dst] = scales[id_src];
    opacs[id_dst] = opacs[id_src];
    features_dc[id_dst] = features_dc[id_src];
    // features_sh may be null under SH-value quantization (canonical store is
    // the packed buffer; fp32 features_sh allocation is freed).
    if (features_sh) {
        for (int i = 0; i < num_sh; ++i)  // TODO: slow; more cache friendly way to do so?
            features_sh[num_sh*id_dst+i] = features_sh[num_sh*id_src+i];
    }

    // set grad to zero (skip non-SH fp32 writes when freed by FPBO non-SH
    // Adam-state quantization).
    if (g1_means) {
        g1_means[id_dst] = make_float3(0.0f);
        g2_means[id_dst] = make_float3(0.0f);
        g1_quats[id_dst] = make_float4(0.0f);
        g2_quats[id_dst] = make_float4(0.0f);
        g1_scales[id_dst] = make_float3(0.0f);
        g2_scales[id_dst] = make_float3(0.0f);
        g1_opacs[id_dst] = 0.0f;
        g2_opacs[id_dst] = 0.0f;
        g1_features_dc[id_dst] = make_float3(0.0f);
        g2_features_dc[id_dst] = make_float3(0.0f);
    }
    if constexpr (sizeof(g_features_sh_t3) == sizeof(short3)) {
        _zero_quant_sh_for_splat<8>(
            (uint8_t*)g1_features_sh, id_dst, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else if constexpr (sizeof(g_features_sh_t3) == sizeof(uchar3)) {
        _zero_quant_sh_for_splat<4>(
            (uint8_t*)g1_features_sh, id_dst, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else {
        for (int i = 0; i < num_sh; ++i) {  // TODO: slow; more cache friendly way to do so?
            g1_features_sh[num_sh*id_dst+i] = g_features_sh_t3{0,0,0};
            g2_features_sh[num_sh*id_dst+i] = g_features_sh_t3{0,0,0};
        }
    }
    _zero_quant_non_sh_all(non_sh, id_dst);
    if (bias_correction_steps)
        bias_correction_steps[id_dst] = 0;
}


/*[AutoHeaderGeneratorExport]*/
void add_splats_mcmc_tensor(
    int64_t cur_num_splats,
    int64_t num_add,
    float min_opacity,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,
    NonShQuantState non_sh,
    uint32_t seed
) {
    if (num_add == 0)
        return;
    int32_t* bias_correction_steps_ptr = bias_correction_steps.data_ptr();

    float* sample_probs = DevicePool::global().acquire<float>(
        PoolSlot::DensifyMcmcAddSampleProbs, cur_num_splats);
    mcmc_compute_relocation_probabilities_kernel<<<_LAUNCH_ARGS_1D(cur_num_splats, 256)>>>(
        cur_num_splats,
        min_opacity,
        opacs.data_ptr(),
        scales.data_ptr(),
        sample_probs
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // CUB inclusive sum for cumsum
    float* sample_probs_cumsum = DevicePool::global().acquire<float>(
        PoolSlot::DensifyMcmcAddSampleProbsCumsum, cur_num_splats);
    {
        size_t temp_bytes = 0;
        cub::DeviceScan::InclusiveSum(
            nullptr, temp_bytes,
            sample_probs, sample_probs_cumsum, (int)cur_num_splats);
        void* temp = DeviceScratch::global().acquire(temp_bytes);
        cub::DeviceScan::InclusiveSum(
            temp, temp_bytes,
            sample_probs, sample_probs_cumsum, (int)cur_num_splats);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    int32_t* index_map = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyMcmcAddIndexMap, num_add);
    int32_t* n_idx_buffer = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyMcmcAddNIdxBuffer, cur_num_splats);
    cudaMemset(n_idx_buffer, 0, cur_num_splats * sizeof(int32_t));

    mcmc_compute_add_index_map_kernel<<<_LAUNCH_ARGS_1D(num_add, 256)>>>(
        sample_probs_cumsum,
        index_map,
        n_idx_buffer,
        cur_num_splats,
        num_add,
        seed
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    mcmc_compute_add_kernel<<<_LAUNCH_ARGS_1D(cur_num_splats, 64)>>>(
        cur_num_splats,
        min_opacity,
        n_idx_buffer,
        scales.data_ptr(),
        opacs.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    #define _DENSIFY_MCMC_ADD_LAUNCH(T, bnd_ptr, bps) \
        mcmc_update_add_kernel<T><<<_LAUNCH_ARGS_1D(num_add, 64)>>>( \
            cur_num_splats, \
            num_add, \
            index_map, \
            means.data_ptr(), g1_means.data_ptr(), g2_means.data_ptr(), \
            quats.data_ptr(), g1_quats.data_ptr(), g2_quats.data_ptr(), \
            scales.data_ptr(), g1_scales.data_ptr(), g2_scales.data_ptr(), \
            opacs.data_ptr(), g1_opacs.data_ptr(), g2_opacs.data_ptr(), \
            features_dc.data_ptr(), g1_features_dc.data_ptr(), g2_features_dc.data_ptr(), \
            num_sh, features_sh.data_ptr(), (T*)g1_features_sh.data_ptr(), (T*)g2_features_sh.data_ptr(), \
            bnd_ptr, bps, \
            non_sh, \
            bias_correction_steps_ptr)
    if      (sh_optim_bits == 32) _DENSIFY_MCMC_ADD_LAUNCH(float3, nullptr, false);
    else if (sh_optim_bits == 8)  _DENSIFY_MCMC_ADD_LAUNCH(short3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
    else if (sh_optim_bits == 4)  _DENSIFY_MCMC_ADD_LAUNCH(uchar3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
    #undef _DENSIFY_MCMC_ADD_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // mcmc_update_add: dst[i] = cur_num_splats + i, src[i] = index_map[i]
    _launch_densify_copy_quant_sh_value(
        cur_num_splats, num_add, index_map, /*dst_indices=*/nullptr,
        sh_value_packed.data_ptr(), sh_value_bounds.data_ptr(),
        num_sh, num_sh_buffer, sh_value_bits, sh_value_bounds_per_splat);
}


// ================
// MCMC Add Noise
// ================


__global__ void mcmc_add_noise_3dgs_kernel(
    long num_splats,
    float scaler,
    float3* __restrict__ means,
    const float3* __restrict__ log_scales,
    const float4* __restrict__ quats,
    const float* __restrict__ logit_opacs
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_splats)
        return;

    SlangDensify::mcmc_add_noise_3dgs(
        scaler,
        &means[idx], log_scales[idx], quats[idx], logit_opacs[idx]
    );
}

__global__ void revised_add_noise_3dgs_kernel(
    long num_splats,
    float scaler,
    const float* __restrict__ radii,
    float3* __restrict__ means,
    const float3* __restrict__ log_scales,
    const float4* __restrict__ quats,
    const float* __restrict__ logit_opacs
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_splats)
        return;

    SlangDensify::revised_add_noise_3dgs(
        scaler, radii[idx],
        &means[idx], log_scales[idx], quats[idx], logit_opacs[idx]
    );
}

// __global__ void mcmc_add_noise_triangle_kernel(
//     long num_splats,
//     float scaler, float min_opacity,
//     float3* __restrict__ means,
//     const float3* __restrict__ log_scales,
//     const float4* __restrict__ quats,
//     const float* __restrict__ opacs
// ) {
//     size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
//     if (idx >= num_splats)
//         return;

//     SlangDensify::mcmc_add_noise_triangle(
//         scaler, min_opacity,
//         &means[idx], log_scales[idx], quats[idx], opacs[idx]
//     );
// }


/*[AutoHeaderGeneratorExport]*/
void mcmc_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs
) {
    mcmc_add_noise_3dgs_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        num_splats, scaler,
        means.data_ptr(),
        log_scales.data_ptr(),
        quats.data_ptr(),
        opacs.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


/*[AutoHeaderGeneratorExport]*/
void revised_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float> radii,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs
) {
    revised_add_noise_3dgs_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        num_splats, scaler,
        radii.data_ptr(),
        means.data_ptr(),
        log_scales.data_ptr(),
        quats.data_ptr(),
        opacs.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



