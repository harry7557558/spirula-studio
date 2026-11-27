// DensifyNoise.cu -- exploration noise on splat means.
//
// Part of the Densify family -- see DensifyCommon.cuh.

#include "kernels/densify/DensifyCommon.cuh"

__global__ void revised_add_noise_3dgs_kernel(
    long num_splats,
    float scaler, uint32_t seed,
    const float* __restrict__ radii,
    float3* __restrict__ means,
    const float3* __restrict__ log_scales,
    const float4* __restrict__ quats,
    const float* __restrict__ logit_opacs,
    const float* __restrict__ peak_weight,  // null: gate on opacity alone
    float peak_weight_scale
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_splats)
        return;

    SlangDensify::revised_add_noise_3dgs(
        scaler, radii[idx],
        &means[idx], log_scales[idx], quats[idx], logit_opacs[idx],
        peak_weight ? peak_weight[idx] * peak_weight_scale : 1.0f,
        make_uint4(hash_uint3(seed ^ 0x9e3779b9u, idx, 0), hash_uint3(seed ^ 0x9e3779b9u, idx, 1),
                   hash_uint3(seed ^ 0x9e3779b9u, idx, 2), hash_uint3(seed ^ 0x9e3779b9u, idx, 3))
    );
}

// peak_weight: the engine's running peak alpha * T (stored values times peak_weight_scale);
// empty gates the noise on opacity alone.
/*[AutoHeaderGeneratorExport]*/
void revised_add_noise_tensor(
    int64_t num_splats,
    float scaler,
    DeviceVector<float> radii,
    DeviceVector<float3> means,
    DeviceVector<float3> log_scales,
    DeviceVector<float4> quats,
    DeviceVector<float> opacs,
    DeviceVector<float> peak_weight,
    float peak_weight_scale,
    uint32_t seed
) {
    revised_add_noise_3dgs_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        num_splats, scaler, seed,
        radii.data_ptr(),
        means.data_ptr(),
        log_scales.data_ptr(),
        quats.data_ptr(),
        opacs.data_ptr(),
        peak_weight.data_ptr(),
        peak_weight_scale
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
