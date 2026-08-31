#include "kernels/projection/ProjectionBwd.cuh"

#include "kernels/projection/CameraVariants.cuh"

#include <core/Common.cuh>
#include <optional>

#include <cooperative_groups.h>
namespace cg = cooperative_groups;


template<
    typename SplatPrimitive,
    CameraModelType camera_model,
    CameraDistortionType distortion
>
void projection_fused_bwd_kernel_wrapper(
    cudaStream_t stream,
    // fwd inputs
    const uint32_t C,
    const uint32_t N,
    typename SplatPrimitive::WorldBuffer splats_world,
    const float * viewmats, // [C, 4, 4]
    const float4 * intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // fwd outputs
    const int32_t * camera_ids,          // [nnz, 4]
    const int32_t * gaussian_ids,          // [nnz, 4]
    const uint2 * aabb,          // [C, N, 4]
    // grad outputs
    typename SplatPrimitive::ScreenBuffer v_splats_screen,
    // grad inputs
    typename SplatPrimitive::WorldBuffer v_splats_world,
    float * v_viewmats, // [C, 4, 4] optional
    // SH VALUE-quant (active when sh_value_bits != 32). Mirrors fwd kernel
    // args; the bwd uses them to evaluate v_dir against the codec'd SH.
    const uint8_t* sh_value_packed,
    const float2* sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
);


template<typename SplatPrimitive>
inline void launch_projection_projection_fused_bwd_kernel(
    // fwd inputs
    int64_t N,
    const std::vector<DeviceTensorFloatND> &splats_world_tuple,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const CameraModelType camera_model,
    const CameraDistortionType distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> v_splats_screen,
    // returns
    std::vector<DeviceTensorFloatND> v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant args, plumbed through to project_vjp's SH read.
    const uint8_t* sh_value_packed,
    const float2* sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    typename SplatPrimitive::WorldBuffer splats_world(splats_world_tuple);
    uint32_t C = (uint32_t)std::get<2>(viewmats)[0]; // number of cameras
    const float* viewmats_ptr = (const float*)std::get<0>(viewmats);
    const float4* intrins_ptr = (const float4*)std::get<0>(intrins);

    const bool is_packed = (camera_ids.data_ptr() && gaussian_ids.data_ptr());
    if (is_packed) {  // packed
        N = camera_ids.size();
        C = 1;
    }

    if (C*N == 0)
        return;

    // Packed forward that produced ZERO intersections (nnz == 0, e.g. a camera
    // whose frustum contains no splats) leaves camera_ids / gaussian_ids empty
    // and the [nnz, 1] aabb null. That is NOT non-packed mode -- without this
    // guard the null camera_ids would be misread as "non-packed", and the
    // kernel would index the null aabb over the (num_splats) C*N grid (illegal
    // memory access at ProjectionBwd_kernel.cuh:57). Non-packed mode always
    // passes a valid [C, N] aabb, so a null aabb here means "nothing to do".
    if (!is_packed && aabb.data_ptr() == nullptr)
        return;

    #define _LAUNCH_ARGS ( \
            (cudaStream_t)0, C, N, \
            splats_world, viewmats_ptr, intrins_ptr, dist_coeffs, \
            image_width, image_height, \
            camera_ids.data_ptr(), \
            gaussian_ids.data_ptr(), \
            aabb.data_ptr(), \
            v_splats_screen, \
            v_splats_world, \
            v_viewmats != nullptr ? v_viewmats->data_ptr() : nullptr, \
            sh_value_packed, sh_value_bounds, num_sh_buffer, sh_value_bits, \
            sh_bounds_stride \
        )

    #define _DISPATCH(M, D) \
        if (camera_model == CameraModelType::M && distortion == CameraDistortionType::D) \
            projection_fused_bwd_kernel_wrapper<SplatPrimitive, \
                CameraModelType::M, CameraDistortionType::D> _LAUNCH_ARGS; else
    SS_FOR_EACH_CAMERA_VARIANT(_DISPATCH)
        throw std::runtime_error("Unsupported camera model / distortion tier");
    #undef _DISPATCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    #undef _LAUNCH_ARGS

}



// ================
// Vanilla3DGS
// ================


/*[AutoHeaderGeneratorExport]*/
void projection_3dgs_backward(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &splats_world,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> &v_splats_screen,
    // returns
    const std::vector<DeviceTensorFloatND> &v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant. Nulls + sh_value_bits=32 leaves the bwd on the fp32
    // SH path (callers without value-quant active pass std::nullopt + 32).
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    int sh_degree = Vanilla3DGS<0>::WorldBuffer(splats_world).sh_degree();
    sh_degree = min(sh_degree, max_sh_degree);
    const uint8_t* vp = sh_value_packed.has_value()
        ? (const uint8_t*)std::get<0>(sh_value_packed.value()) : nullptr;
    const float2* vb = sh_value_bounds.has_value()
        ? (const float2*)std::get<0>(sh_value_bounds.value()) : nullptr;
    #define LAUNCH(n) if (sh_degree == (n)) \
        return launch_projection_projection_fused_bwd_kernel<Vanilla3DGS<n>>( \
            num_splats, splats_world, viewmats, intrins, image_width, image_height, cmt(camera_model), cdt(distortion), dist_coeffs, \
            camera_ids, gaussian_ids, aabb, v_splats_screen, \
            v_splats_world, v_viewmats, \
            vp, vb, num_sh_buffer, sh_value_bits, sh_bounds_stride);
    LAUNCH(3) LAUNCH(2) LAUNCH(1) LAUNCH(0) LAUNCH(4)
    #undef LAUNCH
}


// ================
// MipSplatting
// ================

/*[AutoHeaderGeneratorExport]*/
void projection_mip_backward(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &splats_world,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> &v_splats_screen,
    // returns
    const std::vector<DeviceTensorFloatND> &v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant. Nulls + sh_value_bits=32 leaves the bwd on the fp32
    // SH path (callers without value-quant active pass std::nullopt + 32).
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    int sh_degree = MipSplatting<0>::WorldBuffer(splats_world).sh_degree();
    sh_degree = min(sh_degree, max_sh_degree);
    const uint8_t* vp = sh_value_packed.has_value()
        ? (const uint8_t*)std::get<0>(sh_value_packed.value()) : nullptr;
    const float2* vb = sh_value_bounds.has_value()
        ? (const float2*)std::get<0>(sh_value_bounds.value()) : nullptr;
    #define LAUNCH(n) if (sh_degree == (n)) \
        return launch_projection_projection_fused_bwd_kernel<MipSplatting<n>>( \
            num_splats, splats_world, viewmats, intrins, image_width, image_height, cmt(camera_model), cdt(distortion), dist_coeffs, \
            camera_ids, gaussian_ids, aabb, v_splats_screen, \
            v_splats_world, v_viewmats, \
            vp, vb, num_sh_buffer, sh_value_bits, sh_bounds_stride);
    LAUNCH(3) LAUNCH(2) LAUNCH(1) LAUNCH(0) LAUNCH(4)
    #undef LAUNCH
}



// ================
// Vanilla3DGUT
// ================

/*[AutoHeaderGeneratorExport]*/
void projection_3dgut_backward(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &splats_world,
    TorchTensorView viewmats,  // [C, 4, 4]
    TorchTensorView intrins,   // [C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,  // [nnz] or null
    const DeviceVector<int32_t> gaussian_ids,  // [nnz] or null
    const DeviceTensor2D<uint2> aabb,  // [C, N]
    // grad outputs
    const std::vector<DeviceTensorFloatND> &v_splats_screen,
    // returns
    const std::vector<DeviceTensorFloatND> &v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    // SH VALUE-quant. Nulls + sh_value_bits=32 leaves the bwd on the fp32
    // SH path (callers without value-quant active pass std::nullopt + 32).
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    int sh_degree = Vanilla3DGUT<0>::WorldBuffer(splats_world).sh_degree();
    sh_degree = min(sh_degree, max_sh_degree);
    const uint8_t* vp = sh_value_packed.has_value()
        ? (const uint8_t*)std::get<0>(sh_value_packed.value()) : nullptr;
    const float2* vb = sh_value_bounds.has_value()
        ? (const float2*)std::get<0>(sh_value_bounds.value()) : nullptr;
    #define LAUNCH(n) if (sh_degree == (n)) \
        return launch_projection_projection_fused_bwd_kernel<Vanilla3DGUT<n>>( \
            num_splats, splats_world, viewmats, intrins, image_width, image_height, cmt(camera_model), cdt(distortion), dist_coeffs, \
            camera_ids, gaussian_ids, aabb, v_splats_screen, \
            v_splats_world, v_viewmats, \
            vp, vb, num_sh_buffer, sh_value_bits, sh_bounds_stride);
    LAUNCH(3) LAUNCH(2) LAUNCH(1) LAUNCH(0) LAUNCH(4)
    #undef LAUNCH
}
