// Relocation.cu -- splat relocation with long-axis split, including the quantized optim-state carry-over.
//
// Part of the Densify family -- see DensifyCommon.cuh.

#include "kernels/densify/DensifyQuantCopy.cuh"
#include "kernels/densify/DensifyInternal.cuh"

// ================
// Relocation
// ================


template<typename g_features_sh_t3>
__global__ void relocate_with_long_axis_split_kernel(
    int64_t cur_num_splats,
    int64_t num_new_splats,
    float split_opacity_k,
    int32_t* __restrict__ src_indices,
    int32_t* __restrict__ dst_indices,
    float3*__restrict__ means, float3*__restrict__ g1_means, float3*__restrict__ g2_means,
    float4*__restrict__ quats, float4*__restrict__ g1_quats, float4*__restrict__ g2_quats,
    float3*__restrict__ scales, float3*__restrict__ g1_scales, float3*__restrict__ g2_scales,
    float*__restrict__ opacs, float*__restrict__ g1_opacs, float*__restrict__ g2_opacs,
    float3*__restrict__ features_dc, float3*__restrict__ g1_features_dc, float3*__restrict__ g2_features_dc,
    int num_sh, float3*__restrict__ features_sh, g_features_sh_t3*__restrict__ g1_features_sh, g_features_sh_t3*__restrict__ g2_features_sh,
    const float4* __restrict__ sh_quant_bounds,  // nullptr in non-quant mode
    bool sh_bounds_per_splat,
    NonShQuantState non_sh,
    float2*__restrict__ densify_accum_buffer,
    int32_t* __restrict__ bias_correction_steps
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_new_splats)
        return;

    int64_t idx_src = src_indices[idx];
    int64_t idx_dst = dst_indices == nullptr ? cur_num_splats + idx : dst_indices[idx];

    // geometry - long axis split
    float3 mean = means[idx_src], mean_delta;
    float3 scale = scales[idx_src], new_scale;
    float4 quat = quats[idx_src];
    float opac = opacs[idx_src], new_opac;
    SlangDensify::long_axis_split_3dgs(
        split_opacity_k,
        scale, opac, quat,
        &new_scale, &new_opac, &mean_delta
    );
    means[idx_src] = mean - mean_delta;
    means[idx_dst] = mean + mean_delta;
    scales[idx_src] = new_scale;
    scales[idx_dst] = new_scale;
    opacs[idx_src] = new_opac;
    opacs[idx_dst] = new_opac;
    quats[idx_dst] = quat;

    // appearance - copy
    features_dc[idx_dst] = features_dc[idx_src];
    // features_sh may be nullptr when SH-value quantization is on: the canonical
    // store lives in the packed buffer (copied below via _copy_quant_sh_value),
    // and the fp32 features_sh allocation is freed. Skip the fp32 copy then.
    if (features_sh) {
        for (int i = 0; i < num_sh; ++i)  // TODO: slow; more cache friendly way to do so?
            features_sh[num_sh*idx_dst+i] = features_sh[num_sh*idx_src+i];
    }

    // optimizer state - zero
#if 1
    // Non-SH Adam state: fp32 path zeros the g1_/g2_ buffers directly;
    // 16-bit quant path encodes (g1=0, g2=0) into the packed bytes against
    // the current per-splat-block bound (which may not include 0 -- the
    // encode clamps to the nearest endpoint and the next FPBO step's reduce
    // expands the bound).
    if (g1_means) {
        g1_means[idx_dst] = make_float3(0.0f);
        g2_means[idx_dst] = make_float3(0.0f);
        g1_quats[idx_dst] = make_float4(0.0f);
        g2_quats[idx_dst] = make_float4(0.0f);
        g1_scales[idx_dst] = make_float3(0.0f);
        g2_scales[idx_dst] = make_float3(0.0f);
        g1_opacs[idx_dst] = 0.0f;
        g2_opacs[idx_dst] = 0.0f;
        g1_features_dc[idx_dst] = make_float3(0.0f);
        g2_features_dc[idx_dst] = make_float3(0.0f);
    }
    _zero_quant_non_sh_all(non_sh, idx_dst);
    if constexpr (sizeof(g_features_sh_t3) == sizeof(short3)) {
        // 8-bit quant path: write encoded (u=0, log_s=0) bytes; g1 and g2
        // alias the same packed buffer, so one pass covers both.
        _zero_quant_sh_for_splat<8>(
            (uint8_t*)g1_features_sh, idx_dst, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else if constexpr (sizeof(g_features_sh_t3) == sizeof(uchar3)) {
        // 4-bit quant path: joint nibbles, 1 byte/cell. Same aliasing as 8-bit.
        _zero_quant_sh_for_splat<4>(
            (uint8_t*)g1_features_sh, idx_dst, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else {
        for (int i = 0; i < num_sh; ++i) {  // TODO: slow; more cache friendly way to do so?
            g1_features_sh[num_sh*idx_dst+i] = g_features_sh_t3{0,0,0};
            g2_features_sh[num_sh*idx_dst+i] = g_features_sh_t3{0,0,0};
        }
    }
    if (bias_correction_steps)
        bias_correction_steps[idx_dst] = 0;
    #if 0
    g1_means[idx_src] = make_float3(0.0f);
    g2_means[idx_src] = make_float3(0.0f);
    g1_quats[idx_src] = make_float4(0.0f);
    g2_quats[idx_src] = make_float4(0.0f);
    g1_scales[idx_src] = make_float3(0.0f);
    g2_scales[idx_src] = make_float3(0.0f);
    g1_opacs[idx_src] = 0.0f;
    g2_opacs[idx_src] = 0.0f;
    g1_features_dc[idx_src] = make_float3(0.0f);
    g2_features_dc[idx_src] = make_float3(0.0f);
    if constexpr (sizeof(g_features_sh_t3) == sizeof(short3)) {
        _zero_quant_sh_for_splat<8>(
            (uint8_t*)g1_features_sh, idx_src, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else if constexpr (sizeof(g_features_sh_t3) == sizeof(uchar3)) {
        _zero_quant_sh_for_splat<4>(
            (uint8_t*)g1_features_sh, idx_src, num_sh,
            sh_quant_bounds, sh_bounds_per_splat);
    } else {
        for (int i = 0; i < num_sh; ++i) {  // TODO: slow; more cache friendly way to do so?
            g1_features_sh[num_sh*idx_src+i] = g_features_sh_t3{0,0,0};
            g2_features_sh[num_sh*idx_src+i] = g_features_sh_t3{0,0,0};
        }
    }
    if (bias_correction_steps)
        bias_correction_steps[idx_src] = 0;
    #endif
#else
    // to avoid messing up Adam bias correction
    constexpr float k = 1.0f;
    g1_means[idx_dst] = g1_means[idx_src] = k*g1_means[idx_src];
    g2_means[idx_dst] = g2_means[idx_src] = k*g2_means[idx_src];
    g1_quats[idx_dst] = g1_quats[idx_src] = k*g1_quats[idx_src];
    g2_quats[idx_dst] = g2_quats[idx_src] = k*g2_quats[idx_src];
    g1_scales[idx_dst] = g1_scales[idx_src] = k*g1_scales[idx_src];
    g2_scales[idx_dst] = g2_scales[idx_src] = k*g2_scales[idx_src];
    g1_opacs[idx_dst] = g1_opacs[idx_src] = k*g1_opacs[idx_src];
    g2_opacs[idx_dst] = g2_opacs[idx_src] = k*g2_opacs[idx_src];
    g1_features_dc[idx_dst] = g1_features_dc[idx_src] = k*g1_features_dc[idx_src];
    g2_features_dc[idx_dst] = g2_features_dc[idx_src] = k*g2_features_dc[idx_src];
    for (int i = 0; i < num_sh; ++i) {  // TODO: slow; more cache friendly way to do so?
        g1_features_sh[num_sh*idx_dst+i] = g1_features_sh[num_sh*idx_src+i] = k*g1_features_sh[num_sh*idx_src+i];
        g2_features_sh[num_sh*idx_dst+i] = g2_features_sh[num_sh*idx_src+i] = k*g2_features_sh[num_sh*idx_src+i];
    }
    if (bias_correction_steps)
        bias_correction_steps[idx_dst] = bias_correction_steps[idx_src];
#endif

    // will be cleared after densification anyway but doesn't hurt to do so
    if (densify_accum_buffer)
        densify_accum_buffer[idx_dst] = densify_accum_buffer[idx_src];
}


__global__ void compute_relocation_mask_kernel(
    int64_t num_splats,
    float min_opacity,
    const float3* __restrict__ means,
    const float4* __restrict__ quats,
    const float3* __restrict__ scales,
    const float* __restrict__ opacities,
    const float3* __restrict__ features_dc,
    bool* __restrict__ masks,
    int32_t* __restrict__ num_relocate,
    int32_t* __restrict__ relocate_indices
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    int num_relocate_delta = 0;

    if (idx < num_splats) {

        float3 mean = means[idx];
        float4 quat = quats[idx];
        float3 scale = scales[idx];
        float opac = opacities[idx];
        float3 feature_dc = features_dc[idx];

        bool is_low_opac = sigmoid(opac) <= min_opacity;

        bool is_finite = isfinite(
            dot(mean, mean) / dot(quat, quat) + dot(scale, feature_dc) * opac
        );
        bool relocate = is_low_opac || !is_finite
            || SlangDensify::splat_scale_is_dead(scale);

        masks[idx] = !relocate;

        num_relocate_delta = (int)relocate;
    }

    // warpAtomicAdd(num_relocate, num_relocate_delta);

    // TODO: faster way to do so
    if (num_relocate_delta)
        relocate_indices[atomicAdd(num_relocate, num_relocate_delta)] = (int32_t)idx;
}


/*[AutoHeaderGeneratorExport]*/
void relocate_splats_with_long_axis_split_tensor(
    int64_t cur_num_splats,
    float min_opacity,
    float split_opacity_k,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<float2> densify_accum_buffer,
    // Sampling weights, [N, 2] with the score in lane 0; empty falls back to
    // densify_accum_buffer. Separate so a transformed score can drive the
    // draw while the kernel still propagates the raw accumulator src -> dst.
    DeviceVector<float2> sample_weights,
    DeviceVector<int32_t> bias_correction_steps,
    int sh_optim_bits,
    int num_sh,
    // SH-quant bounds buffer + layout flag used to encode (g1=0, g2=0) into
    // the dst splats' packed bytes. Null when sh_optim_bits == 32 (no quant).
    DeviceVector<float4> sh_quant_bounds,
    bool sh_bounds_per_splat,
    // SH VALUE-quant codec copy params. When sh_value_bits != 32 we also do
    // a codec-aware src->dst copy of the SH coefficient bytes (decode src,
    // encode dst against current dst bounds; clipping is acceptable -- see
    // _copy_quant_sh_value_for_splat for the rationale).
    DeviceVector<uint8_t> sh_value_packed,
    DeviceVector<float2>  sh_value_bounds,
    int  sh_value_bits,             // 32 / 8 / 16
    bool sh_value_bounds_per_splat,
    int  num_sh_buffer,             // buffer stride for cell indexing
    // Non-SH Adam-state quant: when enabled, each relocated dst splat gets
    // its packed bytes set to codec-encoded zero against the current bound.
    NonShQuantState non_sh,
    uint32_t seed
) {
    int32_t* bias_correction_steps_ptr = bias_correction_steps.data_ptr();
    bool* mask = DevicePool::global().acquire<bool>(
        PoolSlot::DensifyRelocMask, cur_num_splats);
    int32_t* num_relocate_ptr = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyRelocCount, 1);
    cudaMemset(num_relocate_ptr, 0, sizeof(int32_t));

    int32_t* dst_indices = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyRelocDstIndices, cur_num_splats);

    compute_relocation_mask_kernel<<<_LAUNCH_ARGS_1D(cur_num_splats, 256)>>>(
        cur_num_splats,
        min_opacity,
        means.data_ptr(),
        quats.data_ptr(),
        scales.data_ptr(),
        opacs.data_ptr(),
        features_dc.data_ptr(),
        mask,
        num_relocate_ptr,
        dst_indices
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    int32_t num_relocate_host = 0;
    cudaMemcpy(&num_relocate_host, num_relocate_ptr, sizeof(int32_t), cudaMemcpyDeviceToHost);
    int64_t num_relocate = (int64_t)num_relocate_host;
    if (num_relocate == 0)
        return;

    DeviceVector<float2> weights = sample_weights.data_ptr()
        ? sample_weights : densify_accum_buffer;
    int32_t* src_indices = weighted_sample_without_replacement_internal(
        cur_num_splats, (float*)weights.data_ptr(),
        weights.size() * 2, mask, num_relocate, seed);

    #define _DENSIFY_LAS_LAUNCH(T, bnd_ptr, bps) \
            relocate_with_long_axis_split_kernel<T><<<_LAUNCH_ARGS_1D(num_relocate, 256)>>>( \
                cur_num_splats, \
                num_relocate, \
                split_opacity_k, \
                src_indices, \
                dst_indices, \
                means.data_ptr(), g1_means.data_ptr(), g2_means.data_ptr(), \
                quats.data_ptr(), g1_quats.data_ptr(), g2_quats.data_ptr(), \
                scales.data_ptr(), g1_scales.data_ptr(), g2_scales.data_ptr(), \
                opacs.data_ptr(), g1_opacs.data_ptr(), g2_opacs.data_ptr(), \
                features_dc.data_ptr(), g1_features_dc.data_ptr(), g2_features_dc.data_ptr(), \
                num_sh, features_sh.data_ptr(), (T*)g1_features_sh.data_ptr(), (T*)g2_features_sh.data_ptr(), \
                bnd_ptr, bps, \
                non_sh, \
                densify_accum_buffer.data_ptr(), \
                bias_correction_steps_ptr)
        if      (sh_optim_bits == 32) _DENSIFY_LAS_LAUNCH(float3, nullptr, false);
        else if (sh_optim_bits == 8)  _DENSIFY_LAS_LAUNCH(short3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
        else if (sh_optim_bits == 4)  _DENSIFY_LAS_LAUNCH(uchar3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
        #undef _DENSIFY_LAS_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    _launch_densify_copy_quant_sh_value(
        cur_num_splats, num_relocate, src_indices, dst_indices,
        sh_value_packed.data_ptr(), sh_value_bounds.data_ptr(),
        num_sh, num_sh_buffer, sh_value_bits, sh_value_bounds_per_splat);
}

/*[AutoHeaderGeneratorExport]*/
void add_splats_with_long_axis_split_tensor(
    int64_t cur_num_splats,
    int64_t num_new_splats,
    float split_opacity_k,
    DeviceVector<float3> means, DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacs, DeviceVector<float3> features_dc, DeviceVector<float3> features_sh,
    DeviceVector<float3> g1_means, DeviceVector<float4> g1_quats, DeviceVector<float3> g1_scales, DeviceVector<float> g1_opacs, DeviceVector<float3> g1_features_dc, DeviceVector<float3> g1_features_sh,
    DeviceVector<float3> g2_means, DeviceVector<float4> g2_quats, DeviceVector<float3> g2_scales, DeviceVector<float> g2_opacs, DeviceVector<float3> g2_features_dc, DeviceVector<float3> g2_features_sh,
    DeviceVector<float2> densify_accum_buffer,
    // Sampling weights, [N, 2] with the score in lane 0; empty falls back to
    // densify_accum_buffer. Separate so a transformed score can drive the
    // draw while the kernel still propagates the raw accumulator src -> dst.
    DeviceVector<float2> sample_weights,
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
    if (num_new_splats == 0)
        return;
    int32_t* bias_correction_steps_ptr = bias_correction_steps.data_ptr();

    DeviceVector<float2> weights = sample_weights.data_ptr()
        ? sample_weights : densify_accum_buffer;
    int32_t* split_indices = weighted_sample_without_replacement_internal(
        cur_num_splats, (float*)weights.data_ptr(),
        weights.size() * 2, nullptr, num_new_splats, seed);

    #define _DENSIFY_LAS_ADD_LAUNCH(T, bnd_ptr, bps) \
        relocate_with_long_axis_split_kernel<T><<<_LAUNCH_ARGS_1D(num_new_splats, 256)>>>( \
            cur_num_splats, \
            num_new_splats, \
            split_opacity_k, \
            split_indices, \
            nullptr, \
            means.data_ptr(), g1_means.data_ptr(), g2_means.data_ptr(), \
            quats.data_ptr(), g1_quats.data_ptr(), g2_quats.data_ptr(), \
            scales.data_ptr(), g1_scales.data_ptr(), g2_scales.data_ptr(), \
            opacs.data_ptr(), g1_opacs.data_ptr(), g2_opacs.data_ptr(), \
            features_dc.data_ptr(), g1_features_dc.data_ptr(), g2_features_dc.data_ptr(), \
            num_sh, features_sh.data_ptr(), (T*)g1_features_sh.data_ptr(), (T*)g2_features_sh.data_ptr(), \
            bnd_ptr, bps, \
            non_sh, \
            densify_accum_buffer.data_ptr(), \
            bias_correction_steps_ptr)
    if      (sh_optim_bits == 32) _DENSIFY_LAS_ADD_LAUNCH(float3, nullptr, false);
    else if (sh_optim_bits == 8)  _DENSIFY_LAS_ADD_LAUNCH(short3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
    else if (sh_optim_bits == 4)  _DENSIFY_LAS_ADD_LAUNCH(uchar3, sh_quant_bounds.data_ptr(), sh_bounds_per_splat);
    #undef _DENSIFY_LAS_ADD_LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    _launch_densify_copy_quant_sh_value(
        cur_num_splats, num_new_splats, split_indices, /*dst_indices=*/nullptr,
        sh_value_packed.data_ptr(), sh_value_bounds.data_ptr(),
        num_sh, num_sh_buffer, sh_value_bits, sh_value_bounds_per_splat);
}


