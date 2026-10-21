// DensifySampling.cu -- quantile / median normalization, indexing, scatter-reduce, and weighted sampling without replacement.
//
// Part of the Densify family -- see DensifyCommon.cuh.

#include "kernels/densify/DensifyInternal.cuh"

// ================
// Quantile / Median Normalization
// ================

template<bool invert_quantile>
int batch_quantile_masked_radix_select(
    const float* d_x,
    int B,
    int N,
    float q,
    float* d_out,
    uint32_t* temp,
    cudaStream_t stream
);

template<bool invert_quantile>
int batch_quantile_positive_radix_select(
    const float* d_x,
    int B,
    int N,
    float q,
    float* d_out,
    uint32_t* temp,
    cudaStream_t stream
);

// Internal helper: pool-allocated quantile computation
void quantile_of_positive_finite_elements_internal(
    const float* inputs_ptr,
    int B,
    int N,
    float q,
    bool return_reciprocal,
    bool abs_input,
    float* outputs_ptr
) {
    if (B == 0)
        return;
    float* temp_ptr = DevicePool::global().acquire<float>(
        PoolSlot::DensifyQuantileTemp, 1024 * B);

    auto fn = abs_input
        ? (return_reciprocal ? batch_quantile_masked_radix_select<true>
                             : batch_quantile_masked_radix_select<false>)
        : (return_reciprocal ? batch_quantile_positive_radix_select<true>
                             : batch_quantile_positive_radix_select<false>);
    fn(inputs_ptr, B, N, q, outputs_ptr, (uint32_t*)temp_ptr,
       (cudaStream_t)0);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void quantile_of_abs_of_finite_elements_tensor(
    DeviceVector<float> inputs,
    float q,
    bool return_reciprocal,
    DeviceVector<float> outputs
) {
    int64_t total = inputs.size();
    int64_t B = outputs.size();
    if (B == 0) return;
    int N = (int)(total / B);

    quantile_of_positive_finite_elements_internal(
        inputs.data_ptr(), B, N, q, return_reciprocal, /*abs_input=*/true,
        outputs.data_ptr()
    );
}

// `!(v <= c)` also catches NaN and +inf, so a spike of either kind lands on
// the cutoff rather than poisoning the row.
__global__ void _normalize_clip_map_kernel(
    int64_t total,
    int N,
    float power,
    float* __restrict__ data,
    const float* __restrict__ inv_median,  // [B], null to skip
    const float* __restrict__ clip,        // [B], null to skip
    const float* __restrict__ fit          // [B, N], null to skip
) {
    int64_t idx = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total)
        return;
    int b = (int)(idx / N);
    float v = data[idx];
    if (clip != nullptr) {
        float c = clip[b];
        if (!(v <= c)) v = c;
    }
    if (inv_median != nullptr)
        v *= inv_median[b];
    // The learned score fits the unpowered map. The flat-region background is
    // tiny negative noise, and a fractional power of it is NaN: hence the floor.
    if (fit != nullptr)
        v = v > 0.0f ? v / fmaxf(fit[idx], v * (1.0f / kLearnedScoreMaxRatio)) : 0.0f;
    else if (power != 1.0f)
        v = powf(fmaxf(v, 0.0f), power);
    data[idx] = v;
}

/*[AutoHeaderGeneratorExport]*/
void normalize_clip_map_inplace_tensor(
    TorchTensorView data,  // [B, ...], rows normalized independently
    bool normalize_median,
    float clip_quantile,
    float power,
    // The learned score's rendered map, same shape: the result is then the
    // map, conditioned but not powered, over it -- the ratio the score update
    // reads (docs/notes/learned-densify-score.md). Null skips.
    TorchTensorView fit
) {
    // Quantiles are scale-equivariant and x -> x^p is monotone, so clipping
    // against the un-normalized cutoff and rescaling and powering after is
    // the normalize-then-clip-then-power result in one pass.
    const bool do_clip = (clip_quantile > 0.0f && clip_quantile < 1.0f);
    const bool do_power = (power != 1.0f);
    const float* fit_ptr = (const float*)std::get<0>(fit);
    if (!normalize_median && !do_clip && !do_power && fit_ptr == nullptr)
        return;
    float* ptr = (float*)std::get<0>(data);
    if (ptr == nullptr)
        return;
    const auto& shape = std::get<2>(data);
    if (shape.empty())
        return;
    int64_t total = 1;
    for (size_t i = 0; i < shape.size(); i++) total *= shape[i];
    if (shape[0] <= 0 || total <= 0)
        return;
    int B = (int)shape[0];
    int N = (int)(total / shape[0]);

    float* norm = DevicePool::global().acquire<float>(
        PoolSlot::DensifyMapNorm, 2 * (size_t)B);
    float* inv_median = norm;
    float* clip = norm + B;

    if (normalize_median)
        quantile_of_positive_finite_elements_internal(
            ptr, B, N, 0.5f, /*return_reciprocal=*/true, /*abs_input=*/false,
            inv_median);
    if (do_clip)
        quantile_of_positive_finite_elements_internal(
            ptr, B, N, clip_quantile, /*return_reciprocal=*/false,
            /*abs_input=*/false, clip);

    _normalize_clip_map_kernel<<<_LAUNCH_ARGS_1D(total, 256)>>>(
        total, N, power, ptr,
        normalize_median ? inv_median : nullptr,
        do_clip ? clip : nullptr,
        fit_ptr
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



// ================
// Indexing
// ================

__global__ void index_kernel(
    size_t numel,
    size_t stride,
    int32_t* __restrict__ indices,
    float* __restrict__ src,
    float* __restrict__ dst
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel)
        return;

    size_t bidx = indices[idx / stride] * stride + idx % stride;
    dst[idx] = src[bidx];
}

/*[AutoHeaderGeneratorExport]*/
void inplace_index_tensor(
    DeviceVector<int32_t> indices,
    DeviceVector<float> src,
    DeviceVector<float> dst
) {
    if (indices.size() == 0)
        return;
    int64_t dst_numel = dst.size();
    int64_t stride = dst_numel / indices.size();
    index_kernel<<<_LAUNCH_ARGS_1D(dst_numel, 256)>>>(
        dst_numel,
        stride,
        indices.data_ptr(),
        src.data_ptr(),
        dst.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Scatter Reduce
// ================

__global__ void scatter_add_kernel(
    size_t numel,
    size_t stride,
    int32_t* __restrict__ indices,
    float* __restrict__ src,
    float* __restrict__ dst
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel)
        return;

    size_t bidx = indices[idx / stride] * stride + idx % stride;
    float x = src[idx];
    if (x != 0.0f && isfinite(x))
        atomicAdd(&dst[bidx], src[idx]);
}

__global__ void scatter_max_kernel(
    int numel,
    int stride,
    int32_t* __restrict__ indices,
    float* __restrict__ src,
    float* __restrict__ dst
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel)
        return;

    size_t bidx = indices[idx / stride] * stride + idx % stride;
    float x = src[idx];
    if (isfinite(x))
        atomicMax(&dst[bidx], src[idx]);
}

/*[AutoHeaderGeneratorExport]*/
void inplace_scatter_add_tensor(
    DeviceVector<int32_t> indices,
    DeviceVector<float> src,
    DeviceVector<float> dst
) {
    if (indices.size() == 0)
        return;
    int64_t src_numel = src.size();
    int64_t stride = src_numel / indices.size();
    scatter_add_kernel<<<_LAUNCH_ARGS_1D(src_numel, 256)>>>(
        src_numel,
        stride,
        indices.data_ptr(),
        src.data_ptr(),
        dst.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void inplace_scatter_max_tensor(
    DeviceVector<int32_t> indices,
    DeviceVector<float> src,
    DeviceVector<float> dst
) {
    if (indices.size() == 0)
        return;
    int64_t src_numel = src.size();
    int64_t stride = src_numel / indices.size();
    scatter_max_kernel<<<_LAUNCH_ARGS_1D(src_numel, 256)>>>(
        src_numel,
        stride,
        indices.data_ptr(),
        src.data_ptr(),
        dst.data_ptr()
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Multinomial sample
// ================

__global__ void compute_efraimidis_spirakis_weight_kernel(
    int64_t numel,
    int stride,
    uint32_t seed,
    const float* weights,
    const bool* mask,
    float* out_weights,
    int32_t* num_eligible,  // null to skip counting
    int64_t first_trainable_splat
) {
    int64_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel)
        return;

    float u = (float)hash_uint3(seed, blockIdx.x, threadIdx.x) * exp2f(-32.0f);
    float w = weights[idx * stride];
    w = w / __logf(fmaxf(1.0f - u, 1e-30f));  // larger w -> smaller (more negative) value
    if (mask != nullptr && !mask[idx])
        w = 0.0f;
    if (idx < first_trainable_splat) w = INFINITY;
    out_weights[idx] = w;
    // Strictly negative: a positive weight the mask admits. -0.0 is not one.
    if (num_eligible != nullptr && w < 0.0f)
        atomicAdd(num_eligible, 1);
}

__global__ void iota_kernel(
    int64_t numel,
    int32_t* buffer
) {
    int64_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel)
        return;
    buffer[idx] = idx;
}

// Internal helper for weighted sampling, returns pool-allocated indices
int32_t* weighted_sample_without_replacement_internal(
    int64_t numel,
    float* weights_ptr,
    int64_t weights_numel,
    bool* masks_ptr,
    uint32_t num_sample,
    uint32_t seed,
    uint32_t* num_eligible,
    int64_t first_trainable_splat
) {
    // int stride = (int)(weights_numel / numel);
    int stride = 2;  // above is incorrect during warmup

    float* sorting_values = DevicePool::global().acquire<float>(
        PoolSlot::DensifyWswrSortingValues, numel);
    int32_t* d_eligible = nullptr;
    if (num_eligible) {
        d_eligible = DevicePool::global().acquire<int32_t>(
            PoolSlot::DensifyWswrEligible, 1);
        cudaMemset(d_eligible, 0, sizeof(int32_t));
    }

    compute_efraimidis_spirakis_weight_kernel<<<_LAUNCH_ARGS_1D(numel, 256)>>>(
        numel,
        stride,
        seed,
        weights_ptr,
        masks_ptr,
        sorting_values,
        d_eligible,
        first_trainable_splat
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
    if (num_eligible) {
        int32_t h = 0;
        cudaMemcpy(&h, d_eligible, sizeof(int32_t), cudaMemcpyDeviceToHost);
        *num_eligible = (uint32_t)h;
    }

    int32_t* out_idx = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyWswrOutIdx, num_sample);

    float* d_keys_in = sorting_values;  // reuse
    float* d_keys_out = DevicePool::global().acquire<float>(
        PoolSlot::DensifyWswrKeysOut, numel);

    int32_t* d_indices_in = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyWswrIndicesIn, numel);
    int32_t* d_indices_out = DevicePool::global().acquire<int32_t>(
        PoolSlot::DensifyWswrIndicesOut, numel);

    iota_kernel<<<_LAUNCH_ARGS_1D(numel, 256)>>>(
        numel,
        d_indices_in
    );

    void* d_temp_storage = nullptr;
    size_t temp_storage_bytes = 0;

    cub::DeviceRadixSort::SortPairs(
        d_temp_storage,
        temp_storage_bytes,
        d_keys_in,
        d_keys_out,
        d_indices_in,
        d_indices_out,
        numel
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    d_temp_storage = DeviceScratch::global().acquire(temp_storage_bytes);

    cub::DeviceRadixSort::SortPairs(
        d_temp_storage,
        temp_storage_bytes,
        d_keys_in,
        d_keys_out,
        d_indices_in,
        d_indices_out,
        numel
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    cudaMemcpy(
        out_idx,
        d_indices_out,
        sizeof(int) * num_sample,
        cudaMemcpyDeviceToDevice
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    return out_idx;
}

/*[AutoHeaderGeneratorExport]*/
void weighted_sample_without_replacement_tensor(
    int64_t numel,
    DeviceVector<float> weights,
    bool* masks_ptr,
    uint32_t num_sample,
    uint32_t seed,
    DeviceVector<int32_t> out_idx
) {
    if (numel == -1)
        numel = weights.size();

    int32_t* result = weighted_sample_without_replacement_internal(
        numel,
        weights.data_ptr(),
        weights.size(),
        masks_ptr,
        num_sample,
        seed
    );

    cudaMemcpy(
        out_idx.data_ptr(),
        result,
        sizeof(int32_t) * num_sample,
        cudaMemcpyDeviceToDevice
    );
}


