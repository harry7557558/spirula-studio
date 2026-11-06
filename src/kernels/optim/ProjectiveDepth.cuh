#pragma once
#include "core/Tensor.h"
#include "core/ProjectiveDepthState.h"


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void projective_depth_observe_tensor(
    int64_t count, int64_t n, bool packed,
    DeviceVector<int32_t> camera_ids, DeviceVector<int32_t> gaussian_ids,
    DeviceTensorFloatND gradients, int stride, int offset,
    DeviceVector<float3> means, DeviceVector<float3> scales, DeviceVector<float4> quats,
    ProjectiveDepthOptim depth
);
