#include "kernels/optim/ProjectiveDepth.cuh"
#include "backend/vulkan/kernels/KernelCommon.h"

namespace {
struct ProjectiveObserveParams {
    uint64_t camera_ids, gaussian_ids, gradients, camera_map, centers;
    uint64_t means, scales, quats, states, information, angular_bound;
    uint32_t count, n, packed, stride, offset, wgs_per_row, single_camera, relative, has_bound, pad;
};
static_assert(sizeof(ProjectiveObserveParams) == 128);
}

void projective_depth_observe_tensor(
    int64_t count, int64_t n, bool packed,
    DeviceVector<int32_t> camera_ids, DeviceVector<int32_t> gaussian_ids,
    DeviceTensorFloatND gradients, int stride, int offset,
    DeviceVector<float3> means, DeviceVector<float3> scales, DeviceVector<float4> quats,
    ProjectiveDepthOptim depth
) {
    if (count <= 0 || !depth.states) return;
    ProjectiveObserveParams p{vkk::or_fallback(camera_ids.data_ptr()),vkk::or_fallback(gaussian_ids.data_ptr()),
        (uint64_t)gradients.data_ptr(),(uint64_t)depth.camera_indices,(uint64_t)depth.centers,
        (uint64_t)means.data_ptr(),(uint64_t)scales.data_ptr(),(uint64_t)quats.data_ptr(),
        (uint64_t)depth.states,(uint64_t)depth.information,vkk::or_fallback(depth.angular_bound),(uint32_t)count,(uint32_t)n,
        (uint32_t)packed,(uint32_t)stride,(uint32_t)offset,0,(uint32_t)depth.single_observation,(uint32_t)depth.angular_relative,depth.angular_bound ? 1u : 0u,0};
    vkk::dispatch_flat("projective_depth.observe",{},count,256,&p,sizeof(p),&p.wgs_per_row);
}
