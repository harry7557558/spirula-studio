// Vulkan implementation of kernels/raster/RasterizationStochastic.cuh: the
// device work is shaders/rasterize_stochastic.slang, one workgroup per micro
// tile as the regular forward (RasterizeFwd.cpp).

#include <kernels/raster/RasterizationStochastic.cuh>

#include "primitives/Primitive3DGS.cuh"
#include "primitives/Primitive3DGUT.cuh"

#include "backend/vulkan/kernels/KernelCommon.h"

#include <algorithm>

namespace {

// Mirrors StochasticParams in shaders/rasterize_stochastic.slang.
struct StochasticParams {
    uint64_t means, quats, scales, gaussian_ids, s_screen, depths;
    uint64_t viewmats, intrins, dist_coeffs, aabb, tile_offsets, flatten_ids;
    uint64_t sum, out_rgb, out_depth, out_normal, out_T;
    uint32_t I, N, n_isects, width, height, tile_width, tile_height, macro_log2;
    uint32_t frame, spp, winner, out_depth_on;
    float kernel_cut;
    uint32_t depth_sorted, stratified, _pad0;
};
static_assert(sizeof(StochasticParams) == 17 * 8 + 16 * 4,
              "params layout must match the slang struct");

}  // namespace

std::tuple<
    RenderOutput::TensorTuple,
    DeviceTensor3D<float>
> rasterize_stochastic_fwd(
    int primitive,
    bool output_normal,
    bool output_depth,
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    DeviceTensorFloatND depths,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    DeviceTensor2D<uint2> aabb,
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,
    StochasticFrame frame,
    float kernel_cut
) {
    const int64_t batch = tile_offsets.size<0>();
    const uint32_t tile_height = (uint32_t)tile_offsets.size<1>();
    const uint32_t tile_width = (uint32_t)tile_offsets.size<2>();
    const bool normal = primitive == 2 || (primitive == 1 && output_normal);
    RenderOutput::TensorTuple renders;
    if (normal)
        RenderOutput::resize<RenderOutputType::RGB_DN>(renders, batch, image_height, image_width,
                                                      PoolSlot::Renders, output_depth);
    else
        RenderOutput::resize<RenderOutputType::RGB_D>(renders, batch, image_height, image_width,
                                                     PoolSlot::Renders, output_depth);
    DeviceTensor3D<float> render_Ts;
    render_Ts.resize(PoolSlot::RenderTs, batch, image_height, image_width);

    const vkk::CamDistSpec cd = primitive == 0
        ? vkk::CamDistSpec{}
        : vkk::cam_dist_spec(camera_model, distortion, true);
    StochasticParams p{};
    if (primitive != 0) {
        Vanilla3DGUT<0>::WorldBuffer wb(splats_w);
        p.means = (uint64_t)wb.raw_data(0);
        p.quats = (uint64_t)wb.raw_data(1);
        p.scales = (uint64_t)wb.raw_data(2);
        p.s_screen = (uint64_t)Vanilla3DGUT<0>::ScreenBuffer(splats_s).raw_data();
        p.viewmats = std::get<0>(viewmats);
        p.intrins = std::get<0>(intrins);
        p.aabb = (uint64_t)aabb.data_ptr();
    } else {
        p.means = p.quats = p.scales = p.viewmats = p.intrins = p.aabb = vkk::null_fallback();
        p.s_screen = (uint64_t)Vanilla3DGS<0>::ScreenBuffer(splats_s).raw_data();
    }
    p.gaussian_ids = vkk::or_fallback(gaussian_ids.data_ptr());
    p.depths = (uint64_t)depths.data_ptr();
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.tile_offsets = (uint64_t)tile_offsets.data_ptr();
    p.flatten_ids = vkk::or_fallback(flatten_ids.data_ptr());
    p.sum = (uint64_t)frame.sum;
    p.out_rgb = (uint64_t)std::get<0>(renders).data_ptr();
    p.out_depth = vkk::or_fallback(std::get<1>(renders).data_ptr());
    p.out_normal = vkk::or_fallback(std::get<2>(renders).data_ptr());
    p.out_T = (uint64_t)render_Ts.data_ptr();
    p.I = (uint32_t)batch;
    p.N = gaussian_ids.data_ptr() ? 0u : (uint32_t)num_splats;
    p.n_isects = (uint32_t)flatten_ids.size();
    p.width = image_width;
    p.height = image_height;
    p.tile_width = tile_width;
    p.tile_height = tile_height;
    p.macro_log2 = (uint32_t)macro_log2;
    p.frame = frame.frame;
    p.spp = std::clamp<uint32_t>(frame.spp, 1u, kStochasticMaxSpp);
    p.winner = frame.winner ? 1u : 0u;
    p.out_depth_on = output_depth ? 1u : 0u;
    p.kernel_cut = kernel_cut;
    p.depth_sorted = frame.depth_sorted ? 1u : 0u;
    p.stratified = frame.stratified ? 1u : 0u;

    // Spec IDs: 0 = camera model, 1 = kStPrim, 2 = kStNormal, 3 = kRasterPacked,
    // 4 = distortion tier.
    backend::vk::SpecList spec{cd.cam, (uint32_t)primitive, normal && primitive == 1 ? 1u : 0u,
                               gaussian_ids.data_ptr() ? 1u : 0u, cd.dist};
    // Two walks of every tile's list, the first one culled as it fills.
    vkk::dispatch_ring_budgeted("rasterize_stochastic.rasterize_stochastic", spec,
                                (uint32_t)batch, tile_height << macro_log2,
                                tile_width << macro_log2, &p, sizeof(p),
                                2.0 * vkk::raster_work(p.n_isects, macro_log2, batch,
                                                       image_width, image_height));
    return std::make_tuple(renders, render_Ts);
}
