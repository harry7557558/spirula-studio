// Vulkan implementation of the rasterization-forward launch API
// (kernels/raster/RasterizationFwd.cuh + RasterizationEval3DFwd.cuh). Mirrors the
// rasterize_to_pixels_*_fwd_tensor host wrappers; the device work runs
// shaders/rasterize_fwd.slang with distortion type / median output /
// (eval3d) camera model as specialization constants. One workgroup per
// micro-tile, grid (I, tile_h * MACRO_Y, tile_w * MACRO_X).

#include <kernels/raster/RasterizationFwd.cuh>
#include <kernels/raster/RasterizationEval3DFwd.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors RasterFwd2dParams in shaders/rasterize_fwd.slang.
struct RasterFwd2dParams {
    uint64_t s_screen;
    uint64_t tile_offsets, flatten_ids;
    uint64_t out_rgb, out_depth, out_T, out_last_ids;
    uint64_t dist_rgb, dist_depth, out_median;
    uint32_t I, n_isects, width, height, tile_width, tile_height;
};
static_assert(sizeof(RasterFwd2dParams) == 10 * 8 + 6 * 4,
              "params layout must match the slang struct");

// Mirrors RasterFwd3dgutParams in shaders/rasterize_fwd.slang.
struct RasterFwd3dgutParams {
    uint64_t means, quats, scales, gaussian_ids;
    uint64_t s_screen;
    uint64_t viewmats, intrins, dist_coeffs, aabb;
    uint64_t tile_offsets, flatten_ids;
    uint64_t out_rgb, out_depth, out_T, out_last_ids;
    uint64_t dist_rgb, dist_depth, out_median;
    uint32_t I, N, n_isects, width, height, tile_width, tile_height;
};
static_assert(sizeof(RasterFwd3dgutParams) == 18 * 8 + 7 * 4 + 4 /*pad*/,
              "params layout must match the slang struct");

struct RasterOutputs {
    RenderOutput::TensorTuple renders, distortions;
    DeviceTensor3D<float> render_Ts, render_median;
    DeviceTensor3D<int32_t> last_ids;
};

uint32_t dist_spec(DistortionType dist_type) {
    switch (dist_type) {
        case DistortionType::None: return 0;
        case DistortionType::D: return 1;
        case DistortionType::RGB_D: return 2;
        default:
            throw std::runtime_error(
                "rasterize_to_pixels_fwd: distortion type not instantiated "
                "(normal needs a normal-rendering primitive)");
    }
}

RasterOutputs alloc_raster_outputs(int64_t batch, uint32_t image_height,
                                   uint32_t image_width,
                                   DistortionType dist_type,
                                   bool output_median) {
    RasterOutputs o;
    RenderOutput::resize<RenderOutputType::RGB_D>(
        o.renders, batch, image_height, image_width, PoolSlot::Renders);
    if (dist_type == DistortionType::D)
        RenderOutput::resizeDistortion<DistortionType::D>(
            o.distortions, batch, image_height, image_width,
            PoolSlot::Distortions);
    else if (dist_type == DistortionType::RGB_D)
        RenderOutput::resizeDistortion<DistortionType::RGB_D>(
            o.distortions, batch, image_height, image_width,
            PoolSlot::Distortions);
    o.render_Ts.resize(PoolSlot::RenderTs, batch, image_height, image_width);
    o.last_ids.resize(PoolSlot::RenderLastIds, batch, image_height,
                      image_width);
    if (output_median)
        o.render_median.resize(PoolSlot::RenderMedian, batch, image_height,
                               image_width);
    return o;
}

void dispatch_raster(const char* entry, const backend::vk::SpecList& spec,
                     uint32_t I, uint32_t tile_width, uint32_t tile_height,
                     const void* params, uint32_t params_size) {
    vkk::dispatch_ring(entry, spec, I, tile_height * MACRO_TILE_SIZE_Y,
                       tile_width * MACRO_TILE_SIZE_X, params, params_size);
}

// Shared implementation of the 2D forward (Vanilla3DGS + MipSplatting: the
// antialiasing difference lives entirely in projection).
std::tuple<RenderOutput::TensorTuple, DeviceTensor3D<float>,
           DeviceTensor3D<int32_t>, RenderOutput::TensorTuple,
           DeviceTensor3D<float>>
launch_raster_2d_fwd(
    std::vector<DeviceTensorFloatND>& splats_s,
    const uint32_t image_width, const uint32_t image_height,
    const DeviceTensor3D<int32_t>& tile_offsets,
    const DeviceVector<int32_t>& flatten_ids,
    DistortionType dist_type, bool output_median) {
    const int64_t batch = tile_offsets.size<0>();
    const uint32_t tile_height = (uint32_t)tile_offsets.size<1>();
    const uint32_t tile_width = (uint32_t)tile_offsets.size<2>();

    RasterOutputs o = alloc_raster_outputs(batch, image_height, image_width,
                                           dist_type, output_median);

    Vanilla3DGS<0>::ScreenBuffer sb(splats_s);

    RasterFwd2dParams p{};
    p.s_screen = (uint64_t)sb.raw_data();
    p.tile_offsets = (uint64_t)tile_offsets.data_ptr();
    p.flatten_ids = (uint64_t)flatten_ids.data_ptr();
    p.out_rgb = (uint64_t)std::get<0>(o.renders).data_ptr();
    p.out_depth = (uint64_t)std::get<1>(o.renders).data_ptr();
    p.out_T = (uint64_t)o.render_Ts.data_ptr();
    p.out_last_ids = (uint64_t)o.last_ids.data_ptr();
    p.dist_rgb = (uint64_t)std::get<0>(o.distortions).data_ptr();
    p.dist_depth = (uint64_t)std::get<1>(o.distortions).data_ptr();
    p.out_median = (uint64_t)o.render_median.data_ptr();
    p.I = (uint32_t)batch;
    p.n_isects = (uint32_t)flatten_ids.size();
    p.width = image_width;
    p.height = image_height;
    p.tile_width = tile_width;
    p.tile_height = tile_height;

    backend::vk::SpecList spec{0u, dist_spec(dist_type),
                               output_median ? 1u : 0u};
    dispatch_raster("rasterize_fwd.rasterize_fwd_2d", spec, (uint32_t)batch,
                    tile_width, tile_height, &p, sizeof(p));

    return std::make_tuple(o.renders, o.render_Ts, o.last_ids, o.distortions,
                           o.render_median);
}

}  // namespace

/* API definitions matching kernels/raster/RasterizationFwd.cuh */

std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_3dgs_fwd(
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    DistortionType dist_type,
    bool output_median
) {
    // The 2D fragment reads only the screen buffer; num_splats /
    // gaussian_ids / splats_w never reach the kernel (as in the CUDA path).
    (void)num_splats; (void)splats_w; (void)gaussian_ids;
    return launch_raster_2d_fwd(splats_s, image_width, image_height,
                                tile_offsets, flatten_ids, dist_type,
                                output_median);
}

std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_mip_fwd(
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    DistortionType dist_type,
    bool output_median
) {
    (void)num_splats; (void)splats_w; (void)gaussian_ids;
    return launch_raster_2d_fwd(splats_s, image_width, image_height,
                                tile_offsets, flatten_ids, dist_type,
                                output_median);
}

/* API definition matching kernels/raster/RasterizationEval3DFwd.cuh */

std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_3dgut_fwd(
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    TorchTensorView viewmats,  // [..., C, 4, 4]
    TorchTensorView intrins,  // [..., C, 4], fx, fy, cx, cy
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,  // [..., N] projected 2D AABB
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    DistortionType dist_type,
    bool output_median
) {
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);

    const int64_t batch = tile_offsets.size<0>();
    const uint32_t tile_height = (uint32_t)tile_offsets.size<1>();
    const uint32_t tile_width = (uint32_t)tile_offsets.size<2>();

    RasterOutputs o = alloc_raster_outputs(batch, image_height, image_width,
                                           dist_type, output_median);

    Vanilla3DGUT<0>::WorldBuffer wb(splats_w);
    Vanilla3DGUT<0>::ScreenBuffer sb(splats_s);

    RasterFwd3dgutParams p{};
    p.means = (uint64_t)wb.raw_data(0);
    p.quats = (uint64_t)wb.raw_data(1);
    p.scales = (uint64_t)wb.raw_data(2);
    p.gaussian_ids = (uint64_t)gaussian_ids.data_ptr();
    p.s_screen = (uint64_t)sb.raw_data();
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.aabb = (uint64_t)aabb.data_ptr();
    p.tile_offsets = (uint64_t)tile_offsets.data_ptr();
    p.flatten_ids = (uint64_t)flatten_ids.data_ptr();
    p.out_rgb = (uint64_t)std::get<0>(o.renders).data_ptr();
    p.out_depth = (uint64_t)std::get<1>(o.renders).data_ptr();
    p.out_T = (uint64_t)o.render_Ts.data_ptr();
    p.out_last_ids = (uint64_t)o.last_ids.data_ptr();
    p.dist_rgb = (uint64_t)std::get<0>(o.distortions).data_ptr();
    p.dist_depth = (uint64_t)std::get<1>(o.distortions).data_ptr();
    p.out_median = (uint64_t)o.render_median.data_ptr();
    p.I = (uint32_t)batch;
    // packed mode passes gaussian_ids and N stays 0 (never read); the
    // non-packed projection layout uses cur_num_splats per camera stride
    p.N = gaussian_ids.data_ptr() ? 0u : (uint32_t)num_splats;
    p.n_isects = (uint32_t)flatten_ids.size();
    p.width = image_width;
    p.height = image_height;
    p.tile_width = tile_width;
    p.tile_height = tile_height;

    // Spec IDs: 0 = camera model, 1 = dist type, 2 = median,
    // 3 = kRasterPacked (gaussian_ids present), 4 = distortion tier.
    backend::vk::SpecList spec{cd.cam, dist_spec(dist_type),
                               output_median ? 1u : 0u,
                               gaussian_ids.data_ptr() ? 1u : 0u, cd.dist};
    dispatch_raster("rasterize_fwd.rasterize_fwd_3dgut", spec,
                    (uint32_t)batch, tile_width, tile_height, &p, sizeof(p));

    return std::make_tuple(o.renders, o.render_Ts, o.last_ids, o.distortions,
                           o.render_median);
}
