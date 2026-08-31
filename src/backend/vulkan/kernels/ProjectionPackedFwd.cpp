// Vulkan implementation of the packed projection-forward launch API
// (kernels/projection/ProjectionPackedFwd.cuh). Two-pass compaction mirroring
// ProjectionPackedFwd.cu: a visibility bitmask plus per-workgroup popcounts,
// then backend::inclusive_sum<int32> (SortScan) over the workgroup counts
// instead of CUB, with a 4-byte nnz readback.

#include <kernels/projection/ProjectionPackedFwd.cuh>

#include "backend/common/SortScan.h"
#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors PackedMaskParams in shaders/projection_fwd.slang.
struct PackedMaskParams {
    uint64_t means, quats, scales, opacities;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t out_bits, out_counts;
    uint32_t C, N, width, height, wgs_per_row;
};
static_assert(sizeof(PackedMaskParams) == 9 * 8 + 5 * 4 + 4 /*pad*/,
              "params layout must match the slang struct");

// Mirrors PackedFwdParams in shaders/projection_fwd.slang.
struct PackedFwdParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t mask_bits, block_scan;
    uint64_t out_camera_ids, out_gaussian_ids, out_aabb, out_depths,
        out_radii;
    uint64_t s_screen;
    uint64_t sh_packed, sh_bounds;
    int64_t sh_bounds_stride;
    uint32_t sh_stride;
    uint32_t C, N, width, height, wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};
static_assert(sizeof(PackedFwdParams) == 20 * 8 + 8 * 4,
              "params layout must match the slang struct");

using vkk::resolve_sh_quant;

std::tuple<DeviceVector<int32_t>, DeviceVector<int32_t>, DeviceVector<float4>,
           DeviceVector<float>, std::vector<DeviceTensorFloatND>>
launch_projection_packed_vk(
    const bool eval3d, const bool antialiased,
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

    packed_check_pair_count(C, (uint32_t)N);
    const uint64_t total_threads = (uint64_t)C * (uint64_t)N;
    const uint32_t wgs =
        (uint32_t)((total_threads + SS_PMASK_BLOCK - 1) / SS_PMASK_BLOCK);
    const uint32_t per_row = std::min(std::max(wgs, 1u), 65535u);
    const uint32_t rows = (wgs + per_row - 1) / per_row;

    // --- pass 1: visibility bitmask + per-workgroup popcount, scanned ---
    DeviceVector<uint32_t> mask_bits;
    mask_bits.resize(PoolSlot::ProjMask, (int64_t)wgs * SS_PMASK_WORDS);
    DeviceVector<int32_t> block_counts;
    block_counts.resize(PoolSlot::ProjBlockCount, wgs);
    DeviceVector<int32_t> block_scan;
    block_scan.resize(PoolSlot::ProjScan, wgs);

    PackedMaskParams mp{};
    mp.means = (uint64_t)wb.raw_data(0);
    mp.quats = (uint64_t)wb.raw_data(1);
    mp.scales = (uint64_t)wb.raw_data(2);
    mp.opacities = (uint64_t)wb.raw_data(3);
    mp.viewmats = std::get<0>(viewmats);
    mp.intrins = std::get<0>(intrins);
    mp.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    mp.out_bits = (uint64_t)mask_bits.data_ptr();
    mp.out_counts = (uint64_t)block_counts.data_ptr();
    mp.C = C;
    mp.N = (uint32_t)N;
    mp.width = image_width;
    mp.height = image_height;
    mp.wgs_per_row = per_row;

    // Spec IDs: 0 = camera model, 1 = SH degree (unused by the mask),
    // 2 = antialiased, 3 = SH value bits (unused by the mask), 4 = eval3d,
    // 5 = distortion tier.
    backend::vk::SpecList spec{cd.cam, (uint32_t)sh_degree,
                               antialiased ? 1u : 0u, spec_bits,
                               eval3d ? 1u : 0u, cd.dist};
    vkk::dispatch("projection_fwd.projection_packed_mask", spec, per_row,
                  rows, 1, &mp, sizeof(mp));

    backend::inclusive_sum<int32_t>(block_counts.data_ptr(),
                                    block_scan.data_ptr(), wgs);
    int32_t nnz = 0;
    if (wgs > 0)
        backend::memcpy_sync(&nnz, block_scan.data_ptr() + (wgs - 1),
                             sizeof(int32_t),
                             backend::MemcpyKind::DeviceToHost);

    // --- pass 2: compacted projection ---
    DeviceVector<int32_t> camera_ids;
    camera_ids.resize(PoolSlot::ProjCameraIds, nnz);
    DeviceVector<int32_t> gaussian_ids;
    gaussian_ids.resize(PoolSlot::ProjGaussianIds, nnz);
    DeviceVector<float4> aabb;
    aabb.resize(PoolSlot::ProjAabb, nnz);
    DeviceVector<float> sorting_depths;
    sorting_depths.resize(PoolSlot::ProjDepths, nnz);
    std::vector<DeviceTensorFloatND> splats_screen =
        eval3d ? Vanilla3DGUT<0>::ScreenBuffer::empty_pool(
                     nnz, PoolSlot::ProjScreen)
               : Vanilla3DGS<0>::ScreenBuffer::empty_pool(
                     nnz, PoolSlot::ProjScreen);

    if (nnz > 0) {
        PackedFwdParams p{};
        p.means = mp.means;
        p.quats = mp.quats;
        p.scales = mp.scales;
        p.opacities = mp.opacities;
        p.features_dc = (uint64_t)wb.raw_data(4);
        p.features_sh = (uint64_t)wb.raw_data(5);
        p.sh_stride = (uint32_t)wb.raw_stride(5);
        p.viewmats = mp.viewmats;
        p.intrins = mp.intrins;
        p.dist_coeffs = mp.dist_coeffs;
        p.mask_bits = (uint64_t)mask_bits.data_ptr();
        p.block_scan = (uint64_t)block_scan.data_ptr();
        p.out_camera_ids = (uint64_t)camera_ids.data_ptr();
        p.out_gaussian_ids = (uint64_t)gaussian_ids.data_ptr();
        p.out_aabb = (uint64_t)aabb.data_ptr();
        p.out_depths = (uint64_t)sorting_depths.data_ptr();
        p.out_radii = (uint64_t)radii.data_ptr();
        if (eval3d) {
            Vanilla3DGUT<0>::ScreenBuffer sb(splats_screen);
            p.s_screen = (uint64_t)sb.raw_data();
        } else {
            Vanilla3DGS<0>::ScreenBuffer sb(splats_screen);
            p.s_screen = (uint64_t)sb.raw_data();
        }
        p.sh_packed = q_packed;
        p.sh_bounds = q_bounds;
        p.sh_bounds_stride = q_stride;
        p.num_sh_buffer = num_sh_buffer;
        p.C = C;
        p.N = (uint32_t)N;
        p.width = image_width;
        p.height = image_height;
        p.wgs_per_row = per_row;

        vkk::dispatch_ring("projection_fwd.projection_packed_fwd", spec,
                           per_row, rows, 1, &p, sizeof(p));
    }

    return std::make_tuple(camera_ids, gaussian_ids, aabb, sorting_depths,
                           splats_screen);
}

}  // namespace

/* API definitions matching kernels/projection/ProjectionPackedFwd.cuh */

#define _SS_DEF_PACKED(name, eval3d, antialiased)                        \
std::tuple<                                                                  \
    DeviceVector<int32_t>, DeviceVector<int32_t>, DeviceVector<float4>,      \
    DeviceVector<float>, std::vector<DeviceTensorFloatND>                    \
> name(                                                                      \
    const int64_t num_splats,                                                \
    const int max_sh_degree,                                                 \
    const std::vector<DeviceTensorFloatND> &in_splats,                       \
    TorchTensorView viewmats,                                                \
    TorchTensorView intrins,                                                 \
    const uint32_t image_width,                                              \
    const uint32_t image_height,                                             \
    const std::string camera_model,                                          \
    const std::string distortion,                                            \
    const TorchTensorView dist_coeffs,                                       \
    DeviceVector<float> radii,                                               \
    const std::optional<TorchTensorView> sh_value_packed,                    \
    const std::optional<TorchTensorView> sh_value_bounds,                    \
    const uint32_t num_sh_buffer,                                            \
    const int sh_value_bits,                                                 \
    const int64_t sh_bounds_stride                                           \
) {                                                                          \
    return launch_projection_packed_vk(                                      \
        eval3d, antialiased, num_splats, in_splats, max_sh_degree,           \
        viewmats, intrins, image_width, image_height, camera_model,          \
        distortion, dist_coeffs, radii, sh_value_packed, sh_value_bounds,    \
        num_sh_buffer, sh_value_bits, sh_bounds_stride);                     \
}

_SS_DEF_PACKED(projection_3dgs_packed_forward, false, false)
_SS_DEF_PACKED(projection_mip_packed_forward, false, true)
_SS_DEF_PACKED(projection_3dgut_packed_forward, true, false)

#undef _SS_DEF_PACKED
