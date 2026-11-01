#include "kernels/raster/RasterizationFwd.cuh"

#include <core/Common.cuh>


#include <core/Tensor.h>


template <typename SplatPrimitive, DistortionType dist_type, bool output_median,
          bool output_score>
void rasterize_to_pixels_fwd_kernel_wrapper(
    cudaStream_t stream,
    const uint32_t I,
    const uint32_t N,
    const uint32_t n_isects,
    const uint32_t *__restrict__ gaussian_ids,  // [nnz] optional, for packed mode
    const typename SplatPrimitive::WorldBuffer splat_wbuffer,
    const typename SplatPrimitive::ScreenBuffer splat_sbuffer,
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const int macro_log2,
    const int32_t *__restrict__ tile_offsets, // [I, tile_height, tile_width]
    const int32_t *__restrict__ flatten_ids,  // [n_isects]
    RenderOutput::Buffer render_colors, // [I, image_height, image_width, 3]
    float *__restrict__ render_Ts, // [I, image_height, image_width, 1]
    int32_t *__restrict__ last_ids,        // [I, image_height, image_width]
    RenderOutput::Buffer render_distortions, // [I, image_height, image_width, ...]
    float *__restrict__ render_median, // [I, image_height, image_width, 1], optional
    const SplatScoreRender score,
    const float kernel_cut
);

template <typename SplatPrimitive, DistortionType dist_type, bool output_median,
          bool output_score>
inline void launch_rasterize_to_pixels_fwd_kernel(
    // Gaussian parameters
    int64_t num_splats,  // = cur_num_splats; non-packed projection layout stride
    typename SplatPrimitive::WorldBuffer splats_w,
    typename SplatPrimitive::ScreenBuffer splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets, // [I, tile_height, tile_width]
    const DeviceVector<int32_t> flatten_ids,    // [n_isects]
    int macro_log2,               // binning granularity
    // outputs
    RenderOutput::Tensor renders,
    DeviceTensor3D<float> transmittances,
    DeviceTensor3D<int32_t> last_ids,
    RenderOutput::Tensor distortions,
    DeviceTensor3D<float> render_median,
    const SplatScoreRender& score,
    float kernel_cut
) {
    // splats_w.size() returns max_num_splats (pre-allocated); the projection
    // layout uses cur_num_splats per camera stride. See RasterizationBwd.cu
    // for the matching fix on the backward path.
    uint32_t N = gaussian_ids.data_ptr() ? 0 : (uint32_t)num_splats;
    uint32_t I = transmittances.size<0>();  // number of images
    uint32_t tile_height = tile_offsets.size<1>();
    uint32_t tile_width = tile_offsets.size<2>();
    uint32_t n_isects = flatten_ids.size();

    rasterize_to_pixels_fwd_kernel_wrapper<SplatPrimitive, dist_type, output_median,
                                           output_score>(
        (cudaStream_t)0,
        I, N, n_isects,
        (uint32_t*)gaussian_ids.data_ptr(),
        splats_w, splats_s,
        image_width,
        image_height,
        tile_width,
        tile_height,
        macro_log2,
        tile_offsets.data_ptr(),
        flatten_ids.data_ptr(),
        renders,
        transmittances.data_ptr(),
        last_ids.data_ptr(),
        dist_any(dist_type) ? distortions.buffer() : RenderOutput::Buffer(),
        output_median ? render_median.data_ptr() : nullptr,
        score, kernel_cut
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


template <typename SplatPrimitive, DistortionType dist_type, bool output_median,
          bool output_score>
inline std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_fwd_tensor(
    // Gaussian parameters
    int64_t num_splats,  // = cur_num_splats
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets, // [I, tile_height, tile_width]
    const DeviceVector<int32_t> flatten_ids,    // [n_isects]
    int macro_log2,                             // binning granularity
    bool output_depth,
    const SplatScoreRender& score,
    float kernel_cut
) {
    int64_t batch = tile_offsets.size<0>();

    RenderOutput::TensorTuple renders, distortions;
    RenderOutput::resize<SplatPrimitive::pixelType>(
        renders, batch, image_height, image_width, PoolSlot::Renders,
        output_depth);
    // Allocate only the distortion channels in dist_type (no-op when None).
    RenderOutput::resizeDistortion<dist_type>(
        distortions, batch, image_height, image_width, PoolSlot::Distortions);

    DeviceTensor3D<float> render_Ts;
    DeviceTensor3D<int32_t> render_last_ids;
    render_Ts.resize(PoolSlot::RenderTs, batch, image_height, image_width);
    render_last_ids.resize(PoolSlot::RenderLastIds, batch, image_height, image_width);

    DeviceTensor3D<float> render_median;
    if (output_median)
        render_median.resize(PoolSlot::RenderMedian, batch, image_height, image_width);

    launch_rasterize_to_pixels_fwd_kernel<SplatPrimitive, dist_type, output_median,
                                          output_score>(
        num_splats,
        splats_w, splats_s, gaussian_ids,
        image_width,
        image_height,
        tile_offsets,
        flatten_ids,
        macro_log2,
        renders,
        render_Ts,
        render_last_ids,
        distortions,
        render_median,
        score, kernel_cut
    );

    return std::make_tuple(
        renders, render_Ts, render_last_ids,
        distortions, render_median
    );
}




// 3DGS and Mip-Splatting share the 2D fragment; only None/D/RGB_D are
// instantiated (RGB_D primitive) until a normal-rendering primitive exists.
static std::tuple<
    RenderOutput::TensorTuple, DeviceTensor3D<float>, DeviceTensor3D<int32_t>,
    RenderOutput::TensorTuple, DeviceTensor3D<float>
> rasterize_to_pixels_2d_fwd(
    int64_t num_splats,
    const std::vector<DeviceTensorFloatND>& splats_w,
    const std::vector<DeviceTensorFloatND>& splats_s,
    const DeviceVector<int32_t>& gaussian_ids,
    const uint32_t image_width,
    const uint32_t image_height,
    const DeviceTensor3D<int32_t>& tile_offsets,
    const DeviceVector<int32_t>& flatten_ids,
    int macro_log2,
    DistortionType dist_type,
    bool output_median,
    bool output_depth,
    const SplatScoreRender& score,
    float kernel_cut
) {
#define _F(DT, M, S) rasterize_to_pixels_fwd_tensor<Vanilla3DGS<0>, DT, M, S>
#define _DISPATCH_FWD(DT) (output_median \
        ? (score.enabled() ? _F(DT, true, true) : _F(DT, true, false)) \
        : (score.enabled() ? _F(DT, false, true) : _F(DT, false, false)))
    auto dispatch =
        dist_type == DistortionType::None  ? _DISPATCH_FWD(DistortionType::None) :
        dist_type == DistortionType::D     ? _DISPATCH_FWD(DistortionType::D) :
        dist_type == DistortionType::RGB_D ? _DISPATCH_FWD(DistortionType::RGB_D) :
        throw std::runtime_error("rasterize_to_pixels_fwd: distortion type not instantiated (normal needs a normal-rendering primitive)");
#undef _DISPATCH_FWD
#undef _F
    return dispatch(
        num_splats,
        splats_w, splats_s, gaussian_ids,
        image_width, image_height,
        tile_offsets, flatten_ids, macro_log2, output_depth, score, kernel_cut
    );
}


// ================
// Vanilla3DGS
// ================

/*[AutoHeaderGeneratorExport]*/
std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_3dgs_fwd(
    // Gaussian parameters
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,               // binning granularity
    DistortionType dist_type,
    bool output_median,
    bool output_depth,            // false: nothing reads the depth, leave it out
    SplatScoreRender score,
    // Primitive3DGS::kernel_vis: > 0 cut and renormalized, 0 the full gaussian
    float kernel_cut
) {
    return rasterize_to_pixels_2d_fwd(
        num_splats, splats_w, splats_s, gaussian_ids, image_width, image_height,
        tile_offsets, flatten_ids, macro_log2, dist_type, output_median,
        output_depth, score, kernel_cut);
}



// ================
// MipSplatting
// ================

/*[AutoHeaderGeneratorExport]*/
std::tuple<
    RenderOutput::TensorTuple,  // renders
    DeviceTensor3D<float>,  // transmittances
    DeviceTensor3D<int32_t>,  // last_ids
    RenderOutput::TensorTuple,  // distortions, optional
    DeviceTensor3D<float>  // median depth, optional
> rasterize_to_pixels_mip_fwd(
    // Gaussian parameters
    int64_t num_splats,
    std::vector<DeviceTensorFloatND> splats_w,
    std::vector<DeviceTensorFloatND> splats_s,
    DeviceVector<int32_t> gaussian_ids,
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    // intersections
    const DeviceTensor3D<int32_t> tile_offsets,
    const DeviceVector<int32_t> flatten_ids,
    int macro_log2,               // binning granularity
    DistortionType dist_type,
    bool output_median,
    bool output_depth,            // false: nothing reads the depth, leave it out
    SplatScoreRender score,
    // Primitive3DGS::kernel_vis: > 0 cut and renormalized, 0 the full gaussian
    float kernel_cut
) {
    return rasterize_to_pixels_2d_fwd(
        num_splats, splats_w, splats_s, gaussian_ids, image_width, image_height,
        tile_offsets, flatten_ids, macro_log2, dist_type, output_median,
        output_depth, score, kernel_cut);
}
