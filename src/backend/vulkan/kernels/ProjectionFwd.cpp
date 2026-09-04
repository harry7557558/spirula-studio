// Vulkan implementation of the projection-forward launch API
// (kernels/projection/ProjectionFwd.cuh). Mirrors ProjectionFwd.cu's launcher logic; the
// device work runs shaders/projection_fwd.slang with camera model /
// distortion tier / SH degree / antialiased as specialization constants.

#include <kernels/projection/ProjectionFwd.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors ProjectionFwdParams in shaders/projection_fwd.slang.
struct ProjectionFwdParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t out_aabb, out_depths, out_radii;
    uint64_t s_screen;
    uint64_t sh_packed, sh_bounds;
    int64_t sh_bounds_stride;
    uint32_t sh_stride;
    uint32_t C, N;
    uint32_t width, height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};
static_assert(sizeof(ProjectionFwdParams) == 16 * 8 + 8 * 4,
              "params layout must match the slang struct");

// Mirrors Projection3dgutParams in shaders/projection_fwd.slang.
struct Projection3dgutParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t out_aabb, out_depths, out_radii;
    uint64_t s_screen;
    uint64_t sh_packed, sh_bounds;
    int64_t sh_bounds_stride;
    uint32_t sh_stride;
    uint32_t C, N;
    uint32_t width, height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};
static_assert(sizeof(Projection3dgutParams) == 16 * 8 + 8 * 4,
              "params layout must match the slang struct");

using vkk::resolve_sh_quant;

std::tuple<DeviceTensor2D<uint2>, DeviceTensor2D<float>,
           std::vector<DeviceTensorFloatND>>
launch_projection_fwd_vk(
    const bool antialiased,
    const int64_t N,
    const std::vector<DeviceTensorFloatND>& in_splats,
    const int max_sh_degree,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string& camera_model, const std::string& distortion,
    const TorchTensorView& dist_coeffs,
    DeviceVector<float>& radii,
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

    Vanilla3DGS<0>::WorldBuffer wb(in_splats);
    int sh_degree = wb.sh_degree();
    sh_degree = std::min(sh_degree, max_sh_degree);

    const uint32_t C = (uint32_t)std::get<2>(viewmats)[0];

    DeviceTensor2D<uint2> aabb;
    aabb.resize(PoolSlot::ProjAabb, C, N);
    DeviceTensor2D<float> sorting_depths;
    sorting_depths.resize(PoolSlot::ProjDepths, C, N);
    std::vector<DeviceTensorFloatND> splats_screen =
        Vanilla3DGS<0>::ScreenBuffer::empty_pool(C * N,
                                                 PoolSlot::ProjScreen);
    Vanilla3DGS<0>::ScreenBuffer sb(splats_screen);

    const uint64_t total_threads = (uint64_t)C * (uint64_t)N;
    const uint32_t wgs = (uint32_t)((total_threads + 127) / 128);
    const uint32_t per_row = std::min(wgs, 65535u);
    const uint32_t rows = (wgs + per_row - 1) / per_row;

    ProjectionFwdParams p{};
    p.means = (uint64_t)wb.raw_data(0);
    p.quats = (uint64_t)wb.raw_data(1);
    p.scales = (uint64_t)wb.raw_data(2);
    p.opacities = (uint64_t)wb.raw_data(3);
    p.features_dc = (uint64_t)wb.raw_data(4);
    p.features_sh = (uint64_t)wb.raw_data(5);
    p.sh_stride = (uint32_t)wb.raw_stride(5);
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.out_aabb = (uint64_t)aabb.data_ptr();
    p.out_depths = (uint64_t)sorting_depths.data_ptr();
    p.out_radii = (uint64_t)radii.data_ptr();
    p.s_screen = (uint64_t)sb.raw_data();
    p.sh_packed = q_packed;
    p.sh_bounds = q_bounds;
    p.sh_bounds_stride = q_stride;
    p.num_sh_buffer = num_sh_buffer;
    p.C = C;
    p.N = (uint32_t)N;
    p.width = image_width;
    p.height = image_height;
    p.wgs_per_row = per_row;

    backend::vk::SpecList spec{cd.cam, (uint32_t)sh_degree,
                               antialiased ? 1u : 0u, spec_bits, 0u, cd.dist};
    vkk::dispatch_ring("projection_fwd.projection_fwd_3dgs", spec, per_row,
                       rows, 1, &p, sizeof(p));

    return std::make_tuple(aabb, sorting_depths, splats_screen);
}

}  // namespace

/* API definitions matching kernels/projection/ProjectionFwd.cuh */

std::tuple<
    DeviceTensor2D<uint2>, DeviceTensor2D<float>, std::vector<DeviceTensorFloatND>
> projection_3dgs_forward(
    const int64_t num_splats, const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &in_splats,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string camera_model, const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceVector<float> radii,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    return launch_projection_fwd_vk(
        /*antialiased=*/false, num_splats, in_splats, max_sh_degree,
        viewmats, intrins, image_width, image_height, camera_model,
        distortion, dist_coeffs, radii, sh_value_packed, sh_value_bounds,
        num_sh_buffer, sh_value_bits, sh_bounds_stride);
}

std::tuple<
    DeviceTensor2D<uint2>, DeviceTensor2D<float>, std::vector<DeviceTensorFloatND>
> projection_mip_forward(
    const int64_t num_splats, const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &in_splats,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string camera_model, const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceVector<float> radii,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    return launch_projection_fwd_vk(
        /*antialiased=*/true, num_splats, in_splats, max_sh_degree,
        viewmats, intrins, image_width, image_height, camera_model,
        distortion, dist_coeffs, radii, sh_value_packed, sh_value_bounds,
        num_sh_buffer, sh_value_bits, sh_bounds_stride);
}

std::tuple<
    DeviceTensor2D<uint2>, DeviceTensor2D<float>, std::vector<DeviceTensorFloatND>
> projection_3dgut_forward(
    const int64_t num_splats, const int max_sh_degree,
    const std::vector<DeviceTensorFloatND> &in_splats,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string camera_model, const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceVector<float> radii,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_bounds_stride
) {
    uint64_t q_packed, q_bounds;
    int64_t q_stride;
    const uint32_t spec_bits =
        resolve_sh_quant(sh_value_packed, sh_value_bounds, num_sh_buffer,
                         sh_value_bits, sh_bounds_stride, &q_packed,
                         &q_bounds, &q_stride);

    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);

    const int64_t N = num_splats;
    Vanilla3DGUT<0>::WorldBuffer wb(in_splats);
    int sh_degree = wb.sh_degree();
    sh_degree = std::min(sh_degree, max_sh_degree);

    const uint32_t C = (uint32_t)std::get<2>(viewmats)[0];

    DeviceTensor2D<uint2> aabb;
    aabb.resize(PoolSlot::ProjAabb, C, N);
    DeviceTensor2D<float> sorting_depths;
    sorting_depths.resize(PoolSlot::ProjDepths, C, N);
    std::vector<DeviceTensorFloatND> splats_screen =
        Vanilla3DGUT<0>::ScreenBuffer::empty_pool(C * N,
                                                  PoolSlot::ProjScreen);
    Vanilla3DGUT<0>::ScreenBuffer sb(splats_screen);

    const uint64_t total_threads = (uint64_t)C * (uint64_t)N;
    const uint32_t wgs = (uint32_t)((total_threads + 127) / 128);
    const uint32_t per_row = std::min(wgs, 65535u);
    const uint32_t rows = (wgs + per_row - 1) / per_row;

    Projection3dgutParams p{};
    p.means = (uint64_t)wb.raw_data(0);
    p.quats = (uint64_t)wb.raw_data(1);
    p.scales = (uint64_t)wb.raw_data(2);
    p.opacities = (uint64_t)wb.raw_data(3);
    p.features_dc = (uint64_t)wb.raw_data(4);
    p.features_sh = (uint64_t)wb.raw_data(5);
    p.sh_stride = (uint32_t)wb.raw_stride(5);
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.out_aabb = (uint64_t)aabb.data_ptr();
    p.out_depths = (uint64_t)sorting_depths.data_ptr();
    p.out_radii = (uint64_t)radii.data_ptr();
    p.s_screen = (uint64_t)sb.raw_data();
    p.sh_packed = q_packed;
    p.sh_bounds = q_bounds;
    p.sh_bounds_stride = q_stride;
    p.num_sh_buffer = num_sh_buffer;
    p.C = C;
    p.N = (uint32_t)N;
    p.width = image_width;
    p.height = image_height;
    p.wgs_per_row = per_row;

    backend::vk::SpecList spec{cd.cam, (uint32_t)sh_degree, 0u, spec_bits, 0u,
                               cd.dist};
    vkk::dispatch_ring("projection_fwd.projection_fwd_3dgut", spec, per_row,
                       rows, 1, &p, sizeof(p));

    return std::make_tuple(aabb, sorting_depths, splats_screen);
}
