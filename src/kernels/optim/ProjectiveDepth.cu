#include "kernels/optim/ProjectiveDepth.cuh"
#include "core/Common.cuh"
#include "generated/slang.cuh"
namespace SlangProjectiveDepth {
#include "generated/set_namespace.cuh"
#include "generated/projective_depth.cuh"
}

__global__ void projective_observe_kernel(
    int64_t count, int64_t n, bool packed, const int32_t* camera_ids,
    const int32_t* gaussian_ids, const float* gradients, int stride, int offset,
    const float3* means, const float3* scales, const float4* quats,
    ProjectiveDepthOptim depth
) {
    int64_t i = (int64_t)blockIdx.x*blockDim.x+threadIdx.x;
    if (i >= count || gradients[i*stride+offset] == 0) return;
    int camera = packed ? camera_ids[i] : i/n;
    int gaussian = packed ? gaussian_ids[i] : i%n;
    uint32_t* anchor = reinterpret_cast<uint32_t*>(depth.states)+4*gaussian;
    uint32_t id;
    if (depth.single_observation) {
        id = __float_as_uint(depth.states[gaussian].x);
        if (id == 0) depth.states[gaussian].x = float(depth.camera_indices[camera]+1);
    } else id = atomicCAS(anchor,0u,__float_as_uint(float(depth.camera_indices[camera]+1)));
    if (id == 0) return;
    float4 a = depth.centers[int(__uint_as_float(id))-1];
    float4 c = depth.centers[depth.camera_indices[camera]];
    float information = SlangProjectiveDepth::projective_depth_information(
        {a.x,a.y,a.z},{c.x,c.y,c.z},means[gaussian],scales[gaussian],quats[gaussian]);
    if (!(information > 0) || !isfinite(information)) return;
    if (depth.single_observation) {
        float previous = depth.information[gaussian];
        depth.information[gaussian] = previous > 0 ? .9f*previous+.1f*information : information;
        return;
    }
    uint32_t* target = reinterpret_cast<uint32_t*>(depth.information)+gaussian;
    uint32_t old = atomicCAS(target,0u,0u);
    for (;;) {
        float previous = __uint_as_float(old);
        float next = previous > 0 ? .9f*previous+.1f*information : information;
        uint32_t observed = atomicCAS(target,old,__float_as_uint(next));
        if (observed == old) break;
        old = observed;
    }
}

/*[AutoHeaderGeneratorExport]*/
void projective_depth_observe_tensor(
    int64_t count, int64_t n, bool packed,
    DeviceVector<int32_t> camera_ids, DeviceVector<int32_t> gaussian_ids,
    DeviceTensorFloatND gradients, int stride, int offset,
    DeviceVector<float3> means, DeviceVector<float3> scales, DeviceVector<float4> quats,
    ProjectiveDepthOptim depth
) {
    if (count <= 0 || !depth.states) return;
    projective_observe_kernel<<<_LAUNCH_ARGS_1D(count,256)>>>(count,n,packed,
        camera_ids.data_ptr(),gaussian_ids.data_ptr(),gradients.data_ptr(),stride,offset,
        means.data_ptr(),scales.data_ptr(),quats.data_ptr(),depth);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
