#pragma once

// Densify-internal primitives implemented in DensifySampling.cu and reused by
// the other densify translation units. These are not part of the public
// Densify.cuh surface (no /*[AutoHeaderGeneratorExport]*/) -- they exist only
// so the parts of the densify family can share one implementation instead of
// each carrying a copy.

#include "kernels/densify/DensifyCommon.cuh"

// Quantile across the strictly-positive finite elements of each of B rows of N.
// `return_reciprocal` emits 1/q; `abs_input` folds negatives in as |x| rather
// than dropping them. Uses PoolSlot::DensifyQuantileTemp scratch.
void quantile_of_positive_finite_elements_internal(
    const float* inputs_ptr,
    int B,
    int N,
    float q,
    bool return_reciprocal,
    bool abs_input,
    float* outputs_ptr
);

// Weighted sampling without replacement (Efraimidis-Spirakis) into a pool-owned
// [num_sample] index buffer. Only the first `num_eligible` draws (positive
// weight, admitted by the mask) are real; the rest are zero-weight or masked.
int32_t* weighted_sample_without_replacement_internal(
    int64_t numel,
    float* weights_ptr,
    int64_t weights_numel,
    bool* masks_ptr,
    uint32_t num_sample,
    uint32_t seed,
    uint32_t* num_eligible = nullptr,
    int64_t first_trainable_splat = 0
);
