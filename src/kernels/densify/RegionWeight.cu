// The region test at every splat, on the densification cadence: a weight of
// `inside` or `outside` per splat, multiplied into the relocation draw so a
// model grows only where it is wanted (shaders/region.slang; docs/notes/
// scene-partition.md). Cameras for the normal's sign come from `toward`, the
// nearest camera's position per splat, or none for an orientation-free test.

#include "kernels/densify/DensifyCommon.cuh"

namespace SlangRegion {
#include "generated/set_namespace.cuh"
#include "generated/region.cuh"
}

namespace {

__global__ void region_weight_kernel(
    int64_t num_splats,
    const float3* __restrict__ means,
    const float4* __restrict__ quats,      // null: no normal
    const float3* __restrict__ scales,
    const float4* __restrict__ camera_bvh, // null: no normal
    const float4* __restrict__ camera_seeds,
    uint32_t num_camera_nodes,
    const float4* __restrict__ program,
    uint32_t num_prog,
    const float4* __restrict__ field_bvh,
    const float4* __restrict__ field_seeds,
    uint32_t num_field_nodes,
    float inside,
    float outside,
    float* __restrict__ weight
) {
    int64_t idx = blockIdx.x * (int64_t)blockDim.x + threadIdx.x;
    if (idx >= num_splats) return;
    const float3 p = means[idx];
    float3 n = make_float3(0.f, 0.f, 0.f);
    bool has_n = false;
    if (quats != nullptr && camera_bvh != nullptr && num_camera_nodes > 0) {
        // The nearest camera, found through the same field query with the
        // cameras as seeds: its index is the seed's label slot.
        const uint32_t cam = SlangRegion::label_field_nearest(
            p, n, false, (float4*)camera_bvh, (float4*)camera_seeds, num_camera_nodes);
        if (cam != 255u) {
            const float4 c = camera_seeds[cam];
            n = SlangRegion::splat_normal(quats[idx], scales[idx], p, make_float3(c.x, c.y, c.z));
            has_n = true;
        }
    }
    const bool in = SlangRegion::region_contains(p, n, has_n, (float4*)program, num_prog,
                                                 (float4*)field_bvh, (float4*)field_seeds,
                                                 num_field_nodes);
    weight[idx] = in ? inside : outside;
}

__global__ void scale_score_kernel(int64_t num_splats, const float* __restrict__ weight,
                                   float2* __restrict__ score) {
    int64_t idx = blockIdx.x * (int64_t)blockDim.x + threadIdx.x;
    if (idx >= num_splats) return;
    score[idx].x *= weight[idx];
}

// Opacity is a logit: sigmoid(o) * factor, back to a logit.
__global__ void decay_opacity_kernel(int64_t num_splats, const float* __restrict__ weight,
                                     float* __restrict__ opac, float factor) {
    int64_t idx = blockIdx.x * (int64_t)blockDim.x + threadIdx.x;
    if (idx >= num_splats || weight[idx] >= 1.0f) return;
    const float q = factor / (1.0f + expf(-opac[idx]));
    opac[idx] = logf(q / (1.0f - q));
}

}  // namespace

/*[AutoHeaderGeneratorExport]*/
void region_weight_tensor(
    int64_t num_splats,
    DeviceVector<float3> means,
    DeviceVector<float4> quats,          // optional with scales + cameras: orients the test
    DeviceVector<float3> scales,
    DeviceVector<float4> camera_bvh,     // a label field over the cameras, index as label
    DeviceVector<float4> camera_seeds,
    DeviceVector<float4> program,        // [num_prog * 6]
    DeviceVector<float4> field_bvh,      // the program's label field, or empty
    DeviceVector<float4> field_seeds,
    float inside,
    float outside,
    DeviceVector<float> weight           // [N] out
) {
    if (num_splats <= 0 || program.data_ptr() == nullptr) return;
    const bool orient = quats.data_ptr() && scales.data_ptr() && camera_bvh.data_ptr() &&
                        camera_seeds.data_ptr();
    // The camera field's seeds label slot holds only 8 bits, so a camera
    // index above 254 cannot be returned; the launcher stores indices modulo
    // nothing -- callers pass a field whose labels ARE the seed indices.
    region_weight_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        num_splats, means.data_ptr(), orient ? quats.data_ptr() : nullptr, scales.data_ptr(),
        orient ? camera_bvh.data_ptr() : nullptr, camera_seeds.data_ptr(),
        (uint32_t)(camera_bvh.size() / 2), program.data_ptr(), (uint32_t)(program.size() / 6),
        field_bvh.data_ptr(), field_seeds.data_ptr(), (uint32_t)(field_bvh.size() / 2),
        inside, outside, weight.data_ptr());
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void densify_scale_score_tensor(
    int64_t num_splats,
    DeviceVector<float> weight,   // [N]
    DeviceVector<float2> score    // [N, 2]; lane 0 is multiplied in place
) {
    if (num_splats <= 0 || weight.data_ptr() == nullptr || score.data_ptr() == nullptr) return;
    scale_score_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(num_splats, weight.data_ptr(),
                                                             score.data_ptr());
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void region_decay_opacity_tensor(
    int64_t num_splats,
    DeviceVector<float> weight,      // [N]; below 1 is outside the region
    DeviceVector<float> opacities,   // [N] logits, scaled in place outside
    float factor                     // in (0, 1]
) {
    if (num_splats <= 0 || weight.data_ptr() == nullptr || !(factor < 1.0f)) return;
    decay_opacity_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(num_splats, weight.data_ptr(),
                                                               opacities.data_ptr(), factor);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
