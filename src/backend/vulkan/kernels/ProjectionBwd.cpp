// Vulkan implementation of the plain projection-backward launch API
// (kernels/projection/ProjectionBwd.cuh). Mirrors ProjectionBwd.cu's launcher logic; the
// device work runs shaders/projection_bwd.slang with camera model / SH
// degree / antialiased / value-quant bits / packed / viewmat-grad as
// specialization constants.
//
// All splat/screen/world-grad tensors are required (the CUDA kernel loads
// them unconditionally through Screen::load / World::load); only
// dist_coeffs, the packed id arrays, v_viewmats and the SH value-quant
// buffers are optional, and those are spec-gated or small enough for the
// null fallback.

#include <kernels/projection/ProjectionBwd.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors ProjectionBwd3dgsParams in shaders/projection_bwd.slang.
struct ProjectionBwd3dgsParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t camera_ids, gaussian_ids, aabb;
    uint64_t vs_screen;
    uint64_t vw_means, vw_quats, vw_scales, vw_opacities, vw_dc, vw_sh;
    uint64_t v_viewmats;
    uint64_t sh_packed, sh_bounds;
    int64_t sh_bounds_stride;
    uint32_t sh_stride_src, sh_stride_v;
    uint32_t C, N;
    uint32_t width, height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
};
static_assert(sizeof(ProjectionBwd3dgsParams) == 22 * 8 + 8 + 8 * 4,
              "params layout must match the slang struct");

// Mirrors ProjectionBwd3dgutParams in shaders/projection_bwd.slang.
struct ProjectionBwd3dgutParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t camera_ids, gaussian_ids, aabb;
    uint64_t vs_screen;
    uint64_t vw_means, vw_quats, vw_scales, vw_opacities, vw_dc, vw_sh;
    uint64_t v_viewmats;
    uint64_t sh_packed, sh_bounds;
    int64_t sh_bounds_stride;
    uint32_t sh_stride_src, sh_stride_v;
    uint32_t C, N;
    uint32_t width, height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
};
static_assert(sizeof(ProjectionBwd3dgutParams) == 22 * 8 + 8 + 8 * 4,
              "params layout must match the slang struct");

using vkk::resolve_sh_quant;

// Shared launcher: eval3d selects the 3DGUT entry / screen layout.
void launch_projection_bwd_vk(
    const bool eval3d, const bool antialiased,
    int64_t N,
    const std::vector<DeviceTensorFloatND>& splats_world,
    const int max_sh_degree,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string& camera_model, const std::string& distortion,
    const TorchTensorView& dist_coeffs,
    const DeviceVector<int32_t>& camera_ids,
    const DeviceVector<int32_t>& gaussian_ids,
    const DeviceTensor2D<uint2>& aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    const std::optional<TorchTensorView>& sh_value_packed,
    const std::optional<TorchTensorView>& sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride) {
    uint64_t q_packed, q_bounds;
    int64_t q_stride;
    const uint32_t spec_bits =
        resolve_sh_quant(sh_value_packed, sh_value_bounds, num_sh_buffer,
                         sh_value_bits, sh_bounds_stride, &q_packed,
                         &q_bounds, &q_stride);

    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);

    // WorldBuffer layout is identical for 3DGS/Mip/3DGUT (TensorArray<6>).
    Vanilla3DGS<0>::WorldBuffer wb(
        const_cast<std::vector<DeviceTensorFloatND>&>(splats_world));
    Vanilla3DGS<0>::WorldBuffer vwb(
        const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_world));
    int sh_degree = wb.sh_degree();
    sh_degree = std::min(sh_degree, max_sh_degree);

    uint32_t C = (uint32_t)std::get<2>(viewmats)[0];
    const bool packed = (camera_ids.data_ptr() && gaussian_ids.data_ptr());
    if (packed) {
        N = camera_ids.size();
        C = 1;
    }
    if ((int64_t)C * N == 0)
        return;
    // Packed forward with zero intersections leaves null ids AND a null
    // aabb; treat as "nothing to do" (see ProjectionBwd.cu's guard).
    if (!packed && aabb.data_ptr() == nullptr)
        return;

    const bool viewmat_grad = (v_viewmats != nullptr);

    const uint64_t total = (uint64_t)C * (uint64_t)N;
    vkk::Fold f = vkk::fold_1d(total, 128);

    backend::vk::SpecList spec{cd.cam,           (uint32_t)sh_degree,
                               antialiased ? 1u : 0u, spec_bits,
                               packed ? 1u : 0u, viewmat_grad ? 1u : 0u,
                               cd.dist};

    if (!eval3d) {
        Vanilla3DGS<0>::ScreenBuffer vsb(
            const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_screen));
        ProjectionBwd3dgsParams p{};
        p.means = (uint64_t)wb.raw_data(0);
        p.quats = (uint64_t)wb.raw_data(1);
        p.scales = (uint64_t)wb.raw_data(2);
        p.opacities = (uint64_t)wb.raw_data(3);
        p.features_dc = (uint64_t)wb.raw_data(4);
        p.features_sh = vkk::or_fallback((uint64_t)wb.raw_data(5));
        p.sh_stride_src = (uint32_t)wb.raw_stride(5);
        p.viewmats = std::get<0>(viewmats);
        p.intrins = std::get<0>(intrins);
        p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
        p.camera_ids = vkk::or_fallback((uint64_t)camera_ids.data_ptr());
        p.gaussian_ids = vkk::or_fallback((uint64_t)gaussian_ids.data_ptr());
        p.aabb = vkk::or_fallback((uint64_t)aabb.data_ptr());
        p.vs_screen = (uint64_t)vsb.raw_data();
        p.vw_means = (uint64_t)vwb.raw_data(0);
        p.vw_quats = (uint64_t)vwb.raw_data(1);
        p.vw_scales = (uint64_t)vwb.raw_data(2);
        p.vw_opacities = (uint64_t)vwb.raw_data(3);
        p.vw_dc = (uint64_t)vwb.raw_data(4);
        p.vw_sh = (uint64_t)vwb.raw_data(5);
        p.sh_stride_v = (uint32_t)vwb.raw_stride(5);
        p.v_viewmats = vkk::or_fallback(
            (uint64_t)(viewmat_grad ? v_viewmats->data_ptr() : nullptr));
        p.sh_packed = q_packed;
        p.sh_bounds = q_bounds;
        p.sh_bounds_stride = q_stride;
        p.num_sh_buffer = num_sh_buffer;
        p.C = C;
        p.N = (uint32_t)N;
        p.width = image_width;
        p.height = image_height;
        p.wgs_per_row = f.per_row;
        vkk::dispatch_ring("projection_bwd.projection_bwd_3dgs", spec,
                           f.per_row, f.rows, 1, &p, sizeof(p));
    } else {
        Vanilla3DGUT<0>::ScreenBuffer vsb(
            const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_screen));
        ProjectionBwd3dgutParams p{};
        p.means = (uint64_t)wb.raw_data(0);
        p.quats = (uint64_t)wb.raw_data(1);
        p.scales = (uint64_t)wb.raw_data(2);
        p.opacities = (uint64_t)wb.raw_data(3);
        p.features_dc = (uint64_t)wb.raw_data(4);
        p.features_sh = vkk::or_fallback((uint64_t)wb.raw_data(5));
        p.sh_stride_src = (uint32_t)wb.raw_stride(5);
        p.viewmats = std::get<0>(viewmats);
        p.intrins = std::get<0>(intrins);
        p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
        p.camera_ids = vkk::or_fallback((uint64_t)camera_ids.data_ptr());
        p.gaussian_ids = vkk::or_fallback((uint64_t)gaussian_ids.data_ptr());
        p.aabb = vkk::or_fallback((uint64_t)aabb.data_ptr());
        p.vs_screen = (uint64_t)vsb.raw_data();
        p.vw_means = (uint64_t)vwb.raw_data(0);
        p.vw_quats = (uint64_t)vwb.raw_data(1);
        p.vw_scales = (uint64_t)vwb.raw_data(2);
        p.vw_opacities = (uint64_t)vwb.raw_data(3);
        p.vw_dc = (uint64_t)vwb.raw_data(4);
        p.vw_sh = (uint64_t)vwb.raw_data(5);
        p.sh_stride_v = (uint32_t)vwb.raw_stride(5);
        p.v_viewmats = vkk::or_fallback(
            (uint64_t)(viewmat_grad ? v_viewmats->data_ptr() : nullptr));
        p.sh_packed = q_packed;
        p.sh_bounds = q_bounds;
        p.sh_bounds_stride = q_stride;
        p.num_sh_buffer = num_sh_buffer;
        p.C = C;
        p.N = (uint32_t)N;
        p.width = image_width;
        p.height = image_height;
        p.wgs_per_row = f.per_row;
        vkk::dispatch_ring("projection_bwd.projection_bwd_3dgut", spec,
                           f.per_row, f.rows, 1, &p, sizeof(p));
    }
}

}  // namespace

/* API definitions matching kernels/projection/ProjectionBwd.cuh */

void projection_3dgs_backward(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    const DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    launch_projection_bwd_vk(
        /*eval3d=*/false, /*antialiased=*/false, num_splats, splats_world,
        max_sh_degree, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_screen, v_splats_world, v_viewmats, sh_value_packed,
        sh_value_bounds, num_sh_buffer, sh_value_bits, sh_bounds_stride);
}

void projection_mip_backward(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    const DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    launch_projection_bwd_vk(
        /*eval3d=*/false, /*antialiased=*/true, num_splats, splats_world,
        max_sh_degree, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_screen, v_splats_world, v_viewmats, sh_value_packed,
        sh_value_bounds, num_sh_buffer, sh_value_bits, sh_bounds_stride);
}

void projection_3dgut_backward(
    const int64_t num_splats,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    const DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    DeviceTensor2D<float>* v_viewmats,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    launch_projection_bwd_vk(
        /*eval3d=*/true, /*antialiased=*/false, num_splats, splats_world,
        max_sh_degree, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_screen, v_splats_world, v_viewmats, sh_value_packed,
        sh_value_bounds, num_sh_buffer, sh_value_bits, sh_bounds_stride);
}
