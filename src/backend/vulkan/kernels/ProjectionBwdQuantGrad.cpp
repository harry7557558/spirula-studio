// Vulkan implementation of the grad-quant projection-backward launch API
// (kernels/projection/ProjectionBwdQuantGrad.cuh). Mirrors ProjectionBwdQuantGrad.cu's
// launcher (per-splat camera ranges when packed); the device work runs
// shaders/projection_qgrad.slang. Which attributes take the quantized
// path is a spec-constant bitmask (kGqMask) so inactive attributes' loads
// are dead at pipeline creation (llvmpipe speculated-load rule).

#include <kernels/projection/ProjectionBwdQuantGrad.cuh>

#include <primitives/Primitive3DGS.cuh>
#include <primitives/Primitive3DGUT.cuh>

#include "backend/common/SortScan.h"
#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors ProjectionQgradParams in shaders/projection_qgrad.slang.
struct ProjectionQgradParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t camera_id_bounds, camera_ids;
    uint64_t aabb;
    uint64_t vs_screen;
    uint64_t vw_means, vw_quats, vw_scales;
    uint64_t gq_means_packed, gq_means_bounds;
    uint64_t gq_quats_packed, gq_quats_bounds;
    uint64_t gq_scales_packed, gq_scales_bounds;
    uint64_t gq_opac_packed, gq_opac_bounds;
    uint64_t gq_dc_packed, gq_dc_bounds;
    uint64_t gq_sh_packed, gq_sh_bounds;
    uint64_t sh_value_packed, sh_value_bounds;
    int64_t sh_value_bounds_stride;
    uint32_t sh_stride_src;
    uint32_t C, N;
    uint32_t width, height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};
static_assert(sizeof(ProjectionQgradParams) == 30 * 8 + 8 + 8 * 4,
              "params layout must match the slang struct");

using vkk::or_fallback;
using vkk::resolve_sh_quant;

void launch_projection_qgrad_vk(
    const bool eval3d, const bool antialiased,
    const int64_t N,
    const int max_sh_degree,
    const std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string& camera_model, const std::string& distortion,
    const TorchTensorView& dist_coeffs,
    const DeviceVector<int32_t>& camera_ids,
    const DeviceVector<int32_t>& gaussian_ids,
    const DeviceTensor2D<uint2>& aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    GradQuantBuffers gq,
    const std::optional<TorchTensorView>& sh_value_packed,
    const std::optional<TorchTensorView>& sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride) {
    uint64_t q_packed, q_bounds;
    int64_t q_stride;
    const uint32_t spec_bits =
        resolve_sh_quant(sh_value_packed, sh_value_bounds, num_sh_buffer,
                         sh_value_bits, sh_value_bounds_stride, &q_packed,
                         &q_bounds, &q_stride);

    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);

    Vanilla3DGS<0>::WorldBuffer wb(
        const_cast<std::vector<DeviceTensorFloatND>&>(splats_world));
    Vanilla3DGS<0>::WorldBuffer vwb(
        const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_world));
    int sh_degree = wb.sh_degree();
    sh_degree = std::min(sh_degree, max_sh_degree);

    const uint32_t C = (uint32_t)std::get<2>(viewmats)[0];
    const bool packed = (camera_ids.data_ptr() && gaussian_ids.data_ptr());
    if (N == 0)
        return;
    if (!packed && aabb.data_ptr() == nullptr)
        return;

    // Packed: per-splat camera ranges (exactly the CUDA launcher's steps).
    vkk::PackedCameraRanges ranges;
    if (packed)
        ranges = vkk::build_packed_camera_ranges(gaussian_ids, N);

    // Active-attribute bitmask from the GradQuantBuffers null pattern.
    uint32_t gq_mask = 0;
    if (gq.means_packed) gq_mask |= 1;
    if (gq.quats_packed) gq_mask |= 2;
    if (gq.scales_packed) gq_mask |= 4;
    if (gq.opac_packed) gq_mask |= 8;
    if (gq.dc_packed) gq_mask |= 16;
    if (gq.sh_packed) gq_mask |= 32;
    const bool world_grad_add = vwb.raw_data(0) != nullptr;

    ProjectionQgradParams p{};
    p.means = (uint64_t)wb.raw_data(0);
    p.quats = (uint64_t)wb.raw_data(1);
    p.scales = (uint64_t)wb.raw_data(2);
    p.opacities = (uint64_t)wb.raw_data(3);
    p.features_dc = (uint64_t)wb.raw_data(4);
    p.features_sh = or_fallback((uint64_t)wb.raw_data(5));
    p.sh_stride_src = (uint32_t)wb.raw_stride(5);
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = or_fallback(std::get<0>(dist_coeffs));
    p.camera_id_bounds = or_fallback(
        (uint64_t)(packed ? ranges.camera_id_bounds.data_ptr() : nullptr));
    p.camera_ids = or_fallback((uint64_t)(packed ? camera_ids.data_ptr()
                                                 : nullptr));
    p.aabb = (uint64_t)aabb.data_ptr();
    if (eval3d) {
        Vanilla3DGUT<0>::ScreenBuffer vsb(
            const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_screen));
        p.vs_screen = (uint64_t)vsb.raw_data();
    } else {
        Vanilla3DGS<0>::ScreenBuffer vsb(
            const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_screen));
        p.vs_screen = (uint64_t)vsb.raw_data();
    }
    p.vw_means = or_fallback((uint64_t)vwb.raw_data(0));
    p.vw_quats = or_fallback((uint64_t)vwb.raw_data(1));
    p.vw_scales = or_fallback((uint64_t)vwb.raw_data(2));
    p.gq_means_packed = or_fallback(gq.means_packed);
    p.gq_means_bounds = or_fallback(gq.means_bounds);
    p.gq_quats_packed = or_fallback(gq.quats_packed);
    p.gq_quats_bounds = or_fallback(gq.quats_bounds);
    p.gq_scales_packed = or_fallback(gq.scales_packed);
    p.gq_scales_bounds = or_fallback(gq.scales_bounds);
    p.gq_opac_packed = or_fallback(gq.opac_packed);
    p.gq_opac_bounds = or_fallback(gq.opac_bounds);
    p.gq_dc_packed = or_fallback(gq.dc_packed);
    p.gq_dc_bounds = or_fallback(gq.dc_bounds);
    p.gq_sh_packed = or_fallback(gq.sh_packed);
    p.gq_sh_bounds = or_fallback(gq.sh_bounds);
    p.sh_value_packed = q_packed;
    p.sh_value_bounds = q_bounds;
    p.sh_value_bounds_stride = q_stride;
    p.num_sh_buffer = num_sh_buffer;
    p.C = C;
    p.N = (uint32_t)N;
    p.width = image_width;
    p.height = image_height;

    vkk::Fold f = vkk::fold_1d(N, 256);
    p.wgs_per_row = f.per_row;
    backend::vk::SpecList spec{cd.cam,              (uint32_t)sh_degree,
                               antialiased ? 1u : 0u, spec_bits,
                               packed ? 1u : 0u,    eval3d ? 1u : 0u,
                               gq_mask,             world_grad_add ? 1u : 0u,
                               cd.dist};
    vkk::dispatch_ring("projection_qgrad.projection_qgrad", spec, f.per_row,
                       f.rows, 1, &p, sizeof(p));
}

}  // namespace

/* API definitions matching kernels/projection/ProjectionBwdQuantGrad.cuh */

void projection_3dgs_backward_quantgrad(
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
    GradQuantBuffers gq,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride
) {
    launch_projection_qgrad_vk(
        /*eval3d=*/false, /*antialiased=*/false, num_splats, max_sh_degree,
        splats_world, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_screen, v_splats_world, gq, sh_value_packed,
        sh_value_bounds, num_sh_buffer, sh_value_bits,
        sh_value_bounds_stride);
}

void projection_mip_backward_quantgrad(
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
    GradQuantBuffers gq,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride
) {
    launch_projection_qgrad_vk(
        /*eval3d=*/false, /*antialiased=*/true, num_splats, max_sh_degree,
        splats_world, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_screen, v_splats_world, gq, sh_value_packed,
        sh_value_bounds, num_sh_buffer, sh_value_bits,
        sh_value_bounds_stride);
}

void projection_3dgut_backward_quantgrad(
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
    GradQuantBuffers gq,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    const uint32_t num_sh_buffer,
    const int sh_value_bits,
    const int64_t sh_value_bounds_stride
) {
    launch_projection_qgrad_vk(
        /*eval3d=*/true, /*antialiased=*/false, num_splats, max_sh_degree,
        splats_world, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_screen, v_splats_world, gq, sh_value_packed,
        sh_value_bounds, num_sh_buffer, sh_value_bits,
        sh_value_bounds_stride);
}
