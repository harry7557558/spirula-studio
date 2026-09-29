// Vulkan implementation of kernels/densify/RegionWeight.cu. Device work:
// shaders/region.slang (entries) over the shared src/shaders/region.slang.

#include <kernels/densify/Densify.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors RegionWeightParams in shaders/region.slang.
struct RegionWeightParams {
    uint64_t means, quats, scales, camera_bvh, camera_seeds, program, field_bvh, field_seeds, weight;
    float inside, outside;
    uint32_t num_camera_nodes, num_prog, num_field_nodes, use_normal;
    uint32_t num_splats, wgs_per_row;
};
static_assert(sizeof(RegionWeightParams) == 9 * 8 + 8 * 4, "params layout must match the slang struct");

// Mirrors ScaleScoreParams.
struct ScaleScoreParams {
    uint64_t weight, score;
    uint32_t num_splats, wgs_per_row, _pad0, _pad1;
};
static_assert(sizeof(ScaleScoreParams) == 2 * 8 + 4 * 4, "params layout must match the slang struct");

// Mirrors DecayOpacityParams.
struct DecayOpacityParams {
    uint64_t weight, opac;
    uint32_t num_splats, wgs_per_row;
    float factor;
    uint32_t _pad0;
};
static_assert(sizeof(DecayOpacityParams) == 2 * 8 + 4 * 4, "params layout must match the slang struct");

}  // namespace

void region_weight_tensor(int64_t num_splats, DeviceVector<float3> means, DeviceVector<float4> quats,
                          DeviceVector<float3> scales, DeviceVector<float4> camera_bvh,
                          DeviceVector<float4> camera_seeds, DeviceVector<float4> program,
                          DeviceVector<float4> field_bvh, DeviceVector<float4> field_seeds,
                          float inside, float outside, DeviceVector<float> weight) {
    if (num_splats <= 0 || program.data_ptr() == nullptr) return;
    const bool orient = quats.data_ptr() && scales.data_ptr() && camera_bvh.data_ptr() &&
                        camera_seeds.data_ptr();
    RegionWeightParams p{};
    p.means = (uint64_t)means.data_ptr();
    p.quats = vkk::or_fallback(quats.data_ptr());
    p.scales = vkk::or_fallback(scales.data_ptr());
    p.camera_bvh = vkk::or_fallback(camera_bvh.data_ptr());
    p.camera_seeds = vkk::or_fallback(camera_seeds.data_ptr());
    p.program = (uint64_t)program.data_ptr();
    p.field_bvh = vkk::or_fallback(field_bvh.data_ptr());
    p.field_seeds = vkk::or_fallback(field_seeds.data_ptr());
    p.weight = (uint64_t)weight.data_ptr();
    p.inside = inside;
    p.outside = outside;
    p.num_camera_nodes = orient ? (uint32_t)(camera_bvh.size() / 2) : 0u;
    p.num_prog = (uint32_t)(program.size() / 6);
    p.num_field_nodes = field_bvh.data_ptr() ? (uint32_t)(field_bvh.size() / 2) : 0u;
    p.use_normal = orient ? 1u : 0u;
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("region.region_weight", {}, num_splats, 256, &p, sizeof(p), &p.wgs_per_row);
}

void densify_scale_score_tensor(int64_t num_splats, DeviceVector<float> weight,
                                DeviceVector<float2> score) {
    if (num_splats <= 0 || weight.data_ptr() == nullptr || score.data_ptr() == nullptr) return;
    ScaleScoreParams p{};
    p.weight = (uint64_t)weight.data_ptr();
    p.score = (uint64_t)score.data_ptr();
    p.num_splats = (uint32_t)num_splats;
    vkk::dispatch_flat("region.densify_scale_score", {}, num_splats, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void region_decay_opacity_tensor(int64_t num_splats, DeviceVector<float> weight,
                                 DeviceVector<float> opacities, float factor) {
    if (num_splats <= 0 || weight.data_ptr() == nullptr || !(factor < 1.0f)) return;
    DecayOpacityParams p{};
    p.weight = (uint64_t)weight.data_ptr();
    p.opac = (uint64_t)opacities.data_ptr();
    p.num_splats = (uint32_t)num_splats;
    p.factor = factor;
    vkk::dispatch_flat("region.decay_opacity", {}, num_splats, 256, &p, sizeof(p), &p.wgs_per_row);
}
