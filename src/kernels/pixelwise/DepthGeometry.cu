// DepthGeometry.cu -- depth -> points, depth -> normal, depth/normal loss,
// ray depth <-> linear depth.
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/PixelWiseCommon.cuh"

// ================
// Depth to Points
// ================


template<CameraDistortionType distortion>
__global__ void depth_to_points_forward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const bool is_ray_depth,
    const TensorView<float, 4> in_depths,  // [B, H, W, 1]
    TensorView<float, 4> out_points  // [B, H, W, 3]
) {
    const int B = in_depths.shape[0],
        H = in_depths.shape[1],
        W = in_depths.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H)
        return;

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
    float in_depth = in_depths.load1(bid, j, i);
    float3 out_point = SlangPixelWiseDist<distortion>::depth_to_point(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model,
        is_ray_depth,
        in_depth
    );
    out_points.store3(bid, j, i, out_point);
}

template<CameraDistortionType distortion>
__global__ void depth_to_points_backward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const bool is_ray_depth,
    const TensorView<float, 4> in_depths,  // [B, H, W, 1]
    const TensorView<float, 4> v_out_points,  // [B, H, W, 3]
    TensorView<float, 4> v_in_depths  // [B, H, W, 1]
) {
    const int B = v_out_points.shape[0],
        H = v_out_points.shape[1],
        W = v_out_points.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H)
        return;

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
    float in_depth = in_depths.load1(bid, j, i);
    float3 v_out_point = v_out_points.load3(bid, j, i);
    float v_in_depth = SlangPixelWiseDist<distortion>::depth_to_point_vjp(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model,
        is_ray_depth,
        in_depth, v_out_point
    );
    v_in_depths.store1(bid, j, i, v_in_depth);
}

/*[AutoHeaderGeneratorExport]*/
void depth_to_points_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> out_points   // [B, H, W, 3]
) {
    int b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();

    #define LAUNCH(D) \
        depth_to_points_forward_kernel<D><<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
            cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, is_ray_depth, \
            _dt3d_to_tv4<float>(depths), _dt3d_to_tv4<float>(out_points))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void depth_to_points_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  in_depths,   // [B, H, W, 1]
    DeviceTensor3D<float3> v_out_points,// [B, H, W, 3]
    DeviceTensor3D<float>  v_in_depths  // [B, H, W, 1]
) {
    int b = in_depths.size<0>(), h = in_depths.size<1>(), w = in_depths.size<2>();

    #define LAUNCH(D) \
        depth_to_points_backward_kernel<D><<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
            cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, is_ray_depth, \
            _dt3d_to_tv4<float>(in_depths), _dt3d_to_tv4<float>(v_out_points), \
            _dt3d_to_tv4<float>(v_in_depths))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



// ================
// Depth to Normal
// ================


template<CameraDistortionType distortion>
__global__ void depth_to_normal_forward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const bool is_ray_depth,
    const TensorView<float, 4> depths,  // [B, H, W, 1]
    TensorView<float, 4> normals  // [B, H, W, 3]
) {
    const int B = depths.shape[0],
        H = depths.shape[1],
        W = depths.shape[2];
    constexpr int TILE = 16;  // blockDim.x and blockDim.y; blockDim.z should be 1
    uint32_t bid = blockIdx.z;
    uint32_t i = blockIdx.x * TILE + threadIdx.x;
    uint32_t j = blockIdx.y * TILE + threadIdx.y;

    bool inside = (bid < B && i < W && j < H);

    // Zero for border pixels (consistent with PyTorch implementation)
    if (inside && (i == 0 || i == W-1 || j == 0 || j == H-1)) {
        normals.store3(bid, j, i, make_float3(0.0f));
        inside = false;
    }

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
#if 0
    if (!inside) return;
    float4 depth = {
        depths.load1(bid, j, i-1),
        depths.load1(bid, j, i+1),
        depths.load1(bid, j-1, i),
        depths.load1(bid, j+1, i),
    };
    float3 normal = depth_to_normal(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model, is_ray_depth,
        depth
    );
#else
    __shared__ float3 shared_points[TILE+2][TILE+2];
    #pragma unroll 2
    for (int k = threadIdx.y * blockDim.x + threadIdx.x;
            k < (TILE+2)*(TILE+2); k += TILE*TILE) {
        int it = k % (TILE+2), jt = k / (TILE+2);
        int ig = int(blockIdx.x * TILE) + it - 1;
        int jg = int(blockIdx.y * TILE) + jt - 1;
        float depth = (ig >= 0 && ig < W && jg >= 0 && jg < H) ?
            depths.load1(bid, jg, ig) : 0.0f;
        float3 ray = SlangPixelWiseDist<distortion>::generate_ray_d2n(
            {(float)ig+0.5f, (float)jg+0.5f},
            {fx, fy, cx, cy}, dist_coeffs,
            (int)camera_model, is_ray_depth
        );
        float3 point = ray * depth;
        if (camera_model == CameraModelType::ORTHOGRAPHIC)
            point += make_float3(((float)ig + 0.5f - cx) / fx,
                                 ((float)jg + 0.5f - cy) / fy, 0.0f);
        shared_points[jt][it] = point;
    }
    __syncthreads();
    if (!inside) return;

    FixedArray<float3, 4> points;
    int it = threadIdx.x+1, jt = threadIdx.y+1;
    points[0] = shared_points[jt][it-1];
    points[1] = shared_points[jt][it+1];
    points[2] = shared_points[jt-1][it];
    points[3] = shared_points[jt+1][it];
    float3 normal = SlangPixelWise::points_to_normal(points);
#endif
    normals.store3(bid, j, i, normal);

}


template<CameraDistortionType distortion>
__global__ void depth_to_normal_backward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const bool is_ray_depth,
    const TensorView<float, 4> depths,  // [B, H, W, 1]
    const TensorView<float, 4> v_normals,  // [B, H, W, 3]
    TensorView<float, 4> v_depths  // [B, H, W, 1]
) {
    const int B = depths.shape[0],
        H = depths.shape[1],
        W = depths.shape[2];
    constexpr int TILE = 16;  // blockDim.x and blockDim.y; blockDim.z should be 1
    uint32_t bid = blockIdx.z;
    uint32_t i = blockIdx.x * TILE + threadIdx.x;
    uint32_t j = blockIdx.y * TILE + threadIdx.y;

    bool inside = (bid < B && i < W && j < H);

    // Zero for border pixels (consistent with PyTorch implementation)
    if (i == 0 || i == W-1 || j == 0 || j == H-1) {
        inside = false;
    }

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
#if 0
    if (!inside) return;
    float4 depth = {
        depths.load1(bid, j, i-1),
        depths.load1(bid, j, i+1),
        depths.load1(bid, j-1, i),
        depths.load1(bid, j+1, i),
    };
    float3 v_normal = v_normals.load3(bid, j, i);
    float4 v_depth;
    depth_to_normal_vjp(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model, is_ray_depth,
        depth, v_normal, &v_depth
    );
    v_depths.atomicStore1(bid, j, i-1, v_depth.x);
    v_depths.atomicStore1(bid, j, i+1, v_depth.y);
    v_depths.atomicStore1(bid, j-1, i, v_depth.z);
    v_depths.atomicStore1(bid, j+1, i, v_depth.w);
#else
    __shared__ float4 shared_points[TILE+2][TILE+2];
    #pragma unroll 2
    for (int k = threadIdx.y * blockDim.x + threadIdx.x;
            k < (TILE+2)*(TILE+2); k += TILE*TILE) {
        int it = k % (TILE+2), jt = k / (TILE+2);
        int ig = int(blockIdx.x * TILE) + it - 1;
        int jg = int(blockIdx.y * TILE) + jt - 1;
        float depth = (ig >= 0 && ig < W && jg >= 0 && jg < H) ?
            depths.load1(bid, jg, ig) : 0.0f;
        float3 ray = SlangPixelWiseDist<distortion>::generate_ray_d2n(
            {(float)ig+0.5f, (float)jg+0.5f},
            {fx, fy, cx, cy}, dist_coeffs,
            (int)camera_model, is_ray_depth
        );
        shared_points[jt][it] = make_float4(ray.x, ray.y, ray.z, depth);
    }
    __syncthreads();
    if (!inside) return;

    float3 v_normal = v_normals.load3(bid, j, i);

    FixedArray<float3, 4> rays;
    FixedArray<float3, 4> points;
    int it = threadIdx.x+1, jt = threadIdx.y+1;
    float4 t;
    t = shared_points[jt][it-1]; rays[0] = {t.x, t.y, t.z}; points[0] = rays[0] * t.w;
    t = shared_points[jt][it+1]; rays[1] = {t.x, t.y, t.z}; points[1] = rays[1] * t.w;
    t = shared_points[jt-1][it]; rays[2] = {t.x, t.y, t.z}; points[2] = rays[2] * t.w;
    t = shared_points[jt+1][it]; rays[3] = {t.x, t.y, t.z}; points[3] = rays[3] * t.w;
    FixedArray<float3, 4> v_points;
    SlangPixelWise::points_to_normal_vjp(points, v_normal, &v_points);

    v_depths.atomicStore1(bid, j, i-1, dot(v_points[0], rays[0]));
    v_depths.atomicStore1(bid, j, i+1, dot(v_points[1], rays[1]));
    v_depths.atomicStore1(bid, j-1, i, dot(v_points[2], rays[2]));
    v_depths.atomicStore1(bid, j+1, i, dot(v_points[3], rays[3]));
#endif
}


/*[AutoHeaderGeneratorExport]*/
void depth_to_normal_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> normals      // [B, H, W, 3]
) {
    int b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();

    #define LAUNCH(D) \
        depth_to_normal_forward_kernel<D><<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
            cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
            is_ray_depth, _dt3d_to_tv4<float>(depths), _dt3d_to_tv4<float>(normals))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void depth_to_normal_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> v_normals,   // [B, H, W, 3]
    DeviceTensor3D<float>  v_depths     // [B, H, W, 1] (accumulated in-place)
) {
    int b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();

    #define LAUNCH(D) \
        depth_to_normal_backward_kernel<D><<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
            cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
            is_ray_depth, _dt3d_to_tv4<float>(depths), \
            _dt3d_to_tv4<float>(v_normals), \
            _dt3d_to_tv4<float>(v_depths))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



/*[AutoHeaderGeneratorExport]*/
void depth_to_normal_forward_tv(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    TorchTensorView depths,    // [B, H, W, 1] float32, CUDA
    TorchTensorView normals    // [B, H, W, 3] float32, CUDA (pre-allocated output)
) {
    depth_to_normal_forward(
        camera_model, distortion, intrins, dist_coeffs, is_ray_depth,
        DeviceTensor3D<float>(depths),
        DeviceTensor3D<float3>(normals)
    );
}

/*[AutoHeaderGeneratorExport]*/
void depth_to_normal_backward_tv(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    TorchTensorView depths,    // [B, H, W, 1]
    TorchTensorView v_normals, // [B, H, W, 3]
    TorchTensorView v_depths   // [B, H, W, 1] accumulated in-place
) {
    depth_to_normal_backward(
        camera_model, distortion, intrins, dist_coeffs, is_ray_depth,
        DeviceTensor3D<float>(depths),
        DeviceTensor3D<float3>(v_normals),
        DeviceTensor3D<float>(v_depths)
    );
}


// ================
// Loss between Depth and Normal
// ================


template<CameraDistortionType distortion>
__global__ void depth_normal_loss_forward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const bool is_ray_depth,
    const TensorView<float, 4> depths,  // [B, H, W, 1]
    const TensorView<float, 4> gt_normals,  // [B, H, W, 3]
    TensorView<float, 4> losses  // [B, H, W, 1]
) {
    const int B = depths.shape[0],
        H = depths.shape[1],
        W = depths.shape[2];
    constexpr int TILE = 16;  // blockDim.x and blockDim.y; blockDim.z should be 1
    uint32_t bid = blockIdx.z;
    uint32_t i = blockIdx.x * TILE + threadIdx.x;
    uint32_t j = blockIdx.y * TILE + threadIdx.y;

    bool inside = (bid < B && i < W && j < H);

    // Zero for border pixels (consistent with PyTorch implementation)
    if (inside && (i == 0 || i == W-1 || j == 0 || j == H-1)) {
        losses.store1(bid, j, i, 0.0f);
        inside = false;
    }

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
#if 1
    if (!inside) return;
    float4 depth = {
        depths.load1(bid, j, i-1),
        depths.load1(bid, j, i+1),
        depths.load1(bid, j-1, i),
        depths.load1(bid, j+1, i),
    };
    float3 gt_normal = gt_normals.load3(bid, j, i);
    float loss = SlangPixelWiseDist<distortion>::depth_normal_loss(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model, is_ray_depth,
        depth, gt_normal
    );
#else
    // TODO: pre-undistort and store in shared memory
#endif
    losses.store1(bid, j, i, loss);
}


template<CameraDistortionType distortion>
__global__ void depth_normal_loss_backward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const bool is_ray_depth,
    const TensorView<float, 4> depths,  // [B, H, W, 1]
    const TensorView<float, 4> gt_normals,  // [B, H, W, 3]
    const TensorView<float, 4> v_losses,  // [B, H, W, 1]
    TensorView<float, 4> v_depths,  // [B, H, W, 1]
    TensorView<float, 4> v_gt_normals  // [B, H, W, 3]
) {
    const int B = depths.shape[0],
        H = depths.shape[1],
        W = depths.shape[2];
    constexpr int TILE = 16;  // blockDim.x and blockDim.y; blockDim.z should be 1
    uint32_t bid = blockIdx.z;
    uint32_t i = blockIdx.x * TILE + threadIdx.x;
    uint32_t j = blockIdx.y * TILE + threadIdx.y;

    bool inside = (bid < B && i < W && j < H);

    // Zero for border pixels (consistent with PyTorch implementation)
    if (i == 0 || i == W-1 || j == 0 || j == H-1) {
        inside = false;
    }

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
#if 1
    if (!inside) {
        v_gt_normals.store3(bid, j, i, float3{0.0f, 0.0f, 0.0f});
        return;
    }
    float4 depth = {
        depths.load1(bid, j, i-1),
        depths.load1(bid, j, i+1),
        depths.load1(bid, j-1, i),
        depths.load1(bid, j+1, i),
    };
    float3 gt_normal = gt_normals.load3(bid, j, i);
    float v_loss = v_losses.load1(bid, j, i);
    float4 v_depth = {0.0f, 0.0f, 0.0f, 0.0f};
    float3 v_gt_normal = float3{0.0f, 0.0f, 0.0f};
    SlangPixelWiseDist<distortion>::depth_normal_loss_vjp(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model, is_ray_depth,
        depth, gt_normal, v_loss, &v_depth, &v_gt_normal
    );
    v_depths.atomicStore1(bid, j, i-1, v_depth.x);
    v_depths.atomicStore1(bid, j, i+1, v_depth.y);
    v_depths.atomicStore1(bid, j-1, i, v_depth.z);
    v_depths.atomicStore1(bid, j+1, i, v_depth.w);
    v_gt_normals.store3(bid, j, i, v_gt_normal);
#else
    // TODO: pre-undistort and store in shared memory
#endif
}


/*[AutoHeaderGeneratorExport]*/
void depth_normal_loss_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> gt_normals,  // [B, H, W, 3]
    DeviceTensor3D<float>  losses       // [B, H, W, 1]
) {
    int b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();

    #define LAUNCH(D) \
        depth_normal_loss_forward_kernel<D><<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
            cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
            is_ray_depth, _dt3d_to_tv4<float>(depths), _dt3d_to_tv4<float>(gt_normals), \
            _dt3d_to_tv4<float>(losses))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void depth_normal_loss_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> gt_normals,  // [B, H, W, 3]
    DeviceTensor3D<float>  v_losses,    // [B, H, W, 1]
    DeviceTensor3D<float>  v_depths,    // [B, H, W, 1] (must be pre-zeroed)
    DeviceTensor3D<float3> v_gt_normals // [B, H, W, 3]
) {
    int b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();

    #define LAUNCH(D) \
        depth_normal_loss_backward_kernel<D><<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
            cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
            is_ray_depth, _dt3d_to_tv4<float>(depths), _dt3d_to_tv4<float>(gt_normals), \
            _dt3d_to_tv4<float>(v_losses), \
            _dt3d_to_tv4<float>(v_depths), _dt3d_to_tv4<float>(v_gt_normals))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ================
// Ray Depth To Linear Depth
// ================


template<CameraDistortionType distortion>
__global__ void ray_depth_to_linear_depth_forward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const TensorView<float, 4> in_depths,  // [B, H, W, 1]
    TensorView<float, 4> out_depths  // [B, H, W, 1]
) {
    const int B = in_depths.shape[0],
        H = in_depths.shape[1],
        W = in_depths.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H)
        return;

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
    float in_depth = in_depths.load1(bid, j, i);
    float out_depth = in_depth * SlangPixelWiseDist<distortion>::ray_depth_to_linear_depth_factor(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model
    );
    out_depths.store1(bid, j, i, out_depth);
}

template<CameraDistortionType distortion>
__global__ void ray_depth_to_linear_depth_backward_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const TensorView<float, 4> v_out_depths,  // [B, H, W, 1]
    TensorView<float, 4> v_in_depths  // [B, H, W, 1]
) {
    const int B = v_out_depths.shape[0],
        H = v_out_depths.shape[1],
        W = v_out_depths.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H)
        return;

    // Load camera
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    // Process
    float v_out_depth = v_out_depths.load1(bid, j, i);
    float factor = SlangPixelWiseDist<distortion>::ray_depth_to_linear_depth_factor(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model
    );
    float v_in_depth = factor * v_out_depth;
    v_in_depths.store1(bid, j, i, v_in_depth);
}

/*[AutoHeaderGeneratorExport]*/
void ray_depth_to_linear_depth_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,  // [B, 4]
    TorchTensorView dist_coeffs,  // [B, 8]
    TorchTensorView depths,  // [B, H, W, 1]
    TorchTensorView out_depths  // [B, H, W, 1]
) {
    const auto& s = std::get<2>(depths);
    int64_t b = s[0], h = s[1], w = s[2];

    #define LAUNCH(D) \
        ray_depth_to_linear_depth_forward_kernel<D> \
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
                cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
                _bhw1_view(depths), _bhw1_view(out_depths))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void ray_depth_to_linear_depth_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,  // [B, 4]
    TorchTensorView dist_coeffs,  // [B, 8]
    TorchTensorView v_out_depths,  // [B, H, W, 1]
    TorchTensorView v_in_depths  // [B, H, W, 1]
) {
    const auto& s = std::get<2>(v_out_depths);
    int64_t b = s[0], h = s[1], w = s[2];

    #define LAUNCH(D) \
        ray_depth_to_linear_depth_backward_kernel<D> \
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
                cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
                _bhw1_view(v_out_depths), _bhw1_view(v_in_depths))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// LINEAR (z) -> RAY depth in place. Intrinsics come in at the IMAGE
// resolution; (sx, sy) rescale them to the depth map's own. The zero sentinel,
// a failed undistort and anything past 85 degrees off axis all leave 0.
template<CameraDistortionType distortion>
__global__ void linear_depth_to_ray_depth_inplace_kernel(
    CameraModelType camera_model,
    const float4 *__restrict__ intrins,  // image-res fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    float sx, float sy,                  // depth_res / image_res scale
    TensorView<float, 4> depths          // [B, Hd, Wd, 1] in/out
) {
    const int B = depths.shape[0],
        H = depths.shape[1],
        W = depths.shape[2];
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t j = blockIdx.y * blockDim.y + threadIdx.y;
    if (bid >= B || i >= W || j >= H)
        return;

    // Intrinsics rescaled from image to depth-map resolution. Distortion
    // coeffs operate on normalized image coordinates and are resolution
    // independent, so they need no scaling.
    float4 intrin = intrins[bid];
    float fx = intrin.x * sx, fy = intrin.y * sy;
    float cx = intrin.z * sx, cy = intrin.w * sy;
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);

    float in_depth = depths.load1(bid, j, i);
    float factor = SlangPixelWiseDist<distortion>::ray_depth_to_linear_depth_factor(
        {(float)i+0.5f, (float)j+0.5f},
        {fx, fy, cx, cy}, dist_coeffs,
        (int)camera_model
    );
    // factor is cos(theta); 0.08715574 is cos(85 deg).
    float out_depth = (factor > 0.08715574f) ? (in_depth / factor) : 0.0f;
    depths.store1(bid, j, i, out_depth);
}

/*[AutoHeaderGeneratorExport]*/
void linear_depth_to_ray_depth_inplace(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,        // [B, 4] at image resolution
    TorchTensorView dist_coeffs,    // [B, 8]
    int image_width, int image_height,
    DeviceTensor3D<float> depths    // [B, Hd, Wd, 1] in/out
) {
    int b = depths.size<0>(), h = depths.size<1>(), w = depths.size<2>();
    if (b <= 0 || h <= 0 || w <= 0) return;
    float sx = (image_width  > 0) ? (float)w / (float)image_width  : 1.0f;
    float sy = (image_height > 0) ? (float)h / (float)image_height : 1.0f;

    #define LAUNCH(D) \
        linear_depth_to_ray_depth_inplace_kernel<D> \
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>( \
                cmt(camera_model), (float4*)std::get<0>(intrins), dist_coeffs, \
                sx, sy, _dt3d_to_tv4<float>(depths))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


