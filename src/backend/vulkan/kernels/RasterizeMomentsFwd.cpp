// Vulkan implementation of the occupancy-moment rasterizer
// (kernels/raster/RasterizationMomentsFwd.cuh). Device work:
// shaders/rasterize_moments.slang -- the 3DGUT forward's micro-tile walk with
// the meshing accumulator. Same launch geometry as the other forward
// rasterizers: one workgroup per micro-tile, grid (I, tile_h * MACRO_Y,
// tile_w * MACRO_X).
//
// Used only by the meshing pipeline's render path
// (mesh/MeshingRasterHost.cpp), which is portable and shared with CUDA.

#include <kernels/raster/RasterizationMomentsFwd.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

#include "primitives/Primitive3DGUT.cuh"

namespace {

// Mirrors RasterMomentsParams in shaders/rasterize_moments.slang.
struct RasterMomentsParams {
    uint64_t means, quats, scales, gaussian_ids;
    uint64_t s_screen;
    uint64_t viewmats, intrins, dist_coeffs, aabb;
    uint64_t tile_offsets, flatten_ids;
    uint64_t out_moments, out_rgb;
    uint32_t I, N, n_isects, width, height, tile_width, tile_height, _pad0;
};
static_assert(sizeof(RasterMomentsParams) == 13 * 8 + 8 * 4,
              "params layout must match the slang struct");

}  // namespace

void rasterize_moments_3dgut_fwd(
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const std::string& camera_model,
    const std::string& distortion,
    TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,
    uint32_t image_width,
    uint32_t image_height,
    const DeviceTensor3D<int32_t>& tile_offsets,
    const DeviceVector<int32_t>& flatten_ids,
    float3* render_moments,
    float3* render_rgb
) {
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    if (cd.cam == (uint32_t)CameraModelType::EQUIRECTANGULAR)
        throw std::runtime_error(
            "rasterize_moments_3dgut_fwd: unsupported camera model");

    const uint32_t I = (uint32_t)tile_offsets.size<0>();
    const uint32_t tile_height = (uint32_t)tile_offsets.size<1>();
    const uint32_t tile_width = (uint32_t)tile_offsets.size<2>();

    Vanilla3DGUT<0>::WorldBuffer wb(splats_w);
    Vanilla3DGUT<0>::ScreenBuffer sb(splats_s);

    RasterMomentsParams p{};
    p.means = (uint64_t)wb.raw_data(0);
    p.quats = (uint64_t)wb.raw_data(1);
    p.scales = (uint64_t)wb.raw_data(2);
    p.gaussian_ids = vkk::or_fallback(gaussian_ids.data_ptr());
    p.s_screen = (uint64_t)sb.raw_data();
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.aabb = (uint64_t)aabb.data_ptr();
    p.tile_offsets = (uint64_t)tile_offsets.data_ptr();
    p.flatten_ids = (uint64_t)flatten_ids.data_ptr();
    p.out_moments = (uint64_t)render_moments;
    // The rgb write is spec-folded off when unused, but the address still must
    // not be null (llvmpipe speculates the store's base).
    p.out_rgb = vkk::or_fallback(render_rgb);
    p.I = I;
    p.N = gaussian_ids.data_ptr() ? 0u : (uint32_t)num_splats;
    p.n_isects = (uint32_t)flatten_ids.size();
    p.width = image_width;
    p.height = image_height;
    p.tile_width = tile_width;
    p.tile_height = tile_height;

    // Spec IDs: 0 = camera model, 1 = kRasterPacked, 2 = kOutputRgb,
    // 3 = distortion tier.
    backend::vk::SpecList spec{cd.cam,
                               gaussian_ids.data_ptr() ? 1u : 0u,
                               render_rgb ? 1u : 0u, cd.dist};
    vkk::dispatch_ring("rasterize_moments.rasterize_moments_3dgut", spec, I,
                       tile_height * MACRO_TILE_SIZE_Y,
                       tile_width * MACRO_TILE_SIZE_X, &p, sizeof(p));
}
