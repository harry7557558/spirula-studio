// Engine setup entrypoints: copy splat/camera/GT inputs to device-resident pool.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <stdexcept>


void set_data_3dgs(
    int64_t num_splats,
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
) {
    int64_t max_num_splats = std::get<2>(means)[0];
    if (std::get<2>(quats)[0] != max_num_splats ||
        std::get<2>(scales)[0] != max_num_splats ||
        std::get<2>(opacities)[0] != max_num_splats ||
        std::get<2>(features_dc)[0] != max_num_splats ||
        std::get<2>(features_sh)[0] != max_num_splats)
        throw std::runtime_error("setData3DGS: max_num_splats mismatch across splat tensors");
    if (max_num_splats < num_splats)
        throw std::runtime_error("setData3DGS: tensor size smaller than num_splats");

    engine().cur_num_splats = num_splats;
    engine().max_num_splats = max_num_splats;
    DevicePool::global().set_splat_counts(engine().cur_num_splats, engine().max_num_splats);

    auto sh_shape = std::get<2>(features_sh);
    engine().num_sh = (sh_shape.size() >= 2) ? (int)sh_shape[1] : 0;

    // Splats are set once and persist on device; subsequent calls are no-ops.
    if (engine().world.initialized) return;

    engine().world.means       = _hv_to_dv<float3>(PoolSlot::WorldMeans, means);
    engine().world.quats       = _hv_to_dv<float4>(PoolSlot::WorldQuats, quats);
    engine().world.scales      = _hv_to_dv<float3>(PoolSlot::WorldScales, scales);
    engine().world.opacities   = _hv_to_dv<float>(PoolSlot::WorldOpacities, opacities);
    engine().world.features_dc = _hv_to_dv<float3>(PoolSlot::WorldFeaturesDc, features_dc);
    engine().world.features_sh = _hv_to_dt2d<float3>(PoolSlot::WorldFeaturesSh, features_sh);

    engine().world.initialized = true;
}


void engine_set_region(TorchTensorView program, TorchTensorView field_bvh,
                       TorchTensorView field_seeds, TorchTensorView camera_bvh,
                       TorchTensorView camera_seeds, float outside_weight,
                       float outside_opacity_decay) {
    auto& r = engine().region;
    r = EngineState::Region{};
    if (std::get<0>(program) == 0) return;
    // [n, 6, 4] floats arrive as float4 rows.
    auto as_float4 = [](TorchTensorView tv) {
        if (std::get<0>(tv) == 0) return tv;
        int64_t n = 1;
        for (int64_t d : std::get<2>(tv)) n *= d;
        return TorchTensorView(std::get<0>(tv), 4, {n / 4, 4});
    };
    r.program = _hv_to_dv<float4>(PoolSlot::EngRegionProgram, as_float4(program));
    r.field_bvh = _hv_to_dv<float4>(PoolSlot::EngRegionFieldBvh, as_float4(field_bvh));
    r.field_seeds = _hv_to_dv<float4>(PoolSlot::EngRegionFieldSeeds, as_float4(field_seeds));
    r.camera_bvh = _hv_to_dv<float4>(PoolSlot::EngRegionCameraBvh, as_float4(camera_bvh));
    r.camera_seeds = _hv_to_dv<float4>(PoolSlot::EngRegionCameraSeeds, as_float4(camera_seeds));
    r.outside = outside_weight;
    r.opacity_decay = outside_opacity_decay;
}

void set_camera_params(
    int width,
    int height,
    std::string camera_model,
    std::string distortion,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs
) {
    engine().camera.width = width;
    engine().camera.height = height;
    engine().camera.model = cmt(camera_model);
    engine().camera.model_str = camera_model;
    engine().camera.distortion = cdt(distortion);
    engine().camera.distortion_str = distortion;
    engine().camera.num = std::get<2>(viewmats)[0];

    if (std::get<2>(intrins)[0] != engine().camera.num ||
        std::get<2>(dist_coeffs)[0] != engine().camera.num)
        throw std::runtime_error("setCameraParams: num_cameras mismatch");

    // viewmats: PyTorch shape [C, 4, 4] -> treat as DeviceTensor2D<float4> [C, 4]
    int64_t C = std::get<2>(viewmats)[0];
    {
        uint64_t src_ptr = std::get<0>(viewmats);
        float4* ptr;
        if (_is_device_ptr((void*)src_ptr)) {
            ptr = (float4*)src_ptr;
        } else {
            ptr = DevicePool::global().acquire<float4>(PoolSlot::CamViewmats, (size_t)(C * 4));
            backend::memcpy_sync(ptr, (void*)src_ptr, C * 4 * sizeof(float4), backend::MemcpyKind::HostToDevice);
        }
        TorchTensorView dv_tv((uint64_t)ptr, 4, {C, 4LL, 4LL});
        engine().camera.viewmats = DeviceTensor2D<float4>(dv_tv);
    }
    engine().camera.intrins     = _hv_to_dv<float4>(PoolSlot::CamIntrins, intrins);
    engine().camera.dist_coeffs = _hv_to_dt2d<float>(PoolSlot::CamDistCoeffs, dist_coeffs);
}


// An 8-bit image the loss can read as it is stays 8-bit on the device, a
// quarter of the float copy. A device source is copied too: the data
// manager may reuse its buffer for the next batch while this one trains.
static TorchTensorView _upload_gt_rgb(const TorchTensorView& src) {
    const uint64_t ptr = std::get<0>(src);
    if (ptr == 0) return TorchTensorView();
    if (std::get<1>(src) != 1 || engine().color_space.image_enabled)
        return _dt3d_tv(_hv_to_dt3d_gt<float3>(src, PoolSlot::GtRgb, "rgb"));
    size_t n = 1;
    for (int64_t d : std::get<2>(src)) n *= (size_t)d;
    uint8_t* dst = DevicePool::global().acquire<uint8_t>(PoolSlot::GtRgb, n);
    backend::memcpy_sync(dst, (const void*)ptr, n, backend::MemcpyKind::Auto);
    return TorchTensorView((uint64_t)dst, 1, std::get<2>(src));
}

void set_training_data(
    TorchTensorView gt_rgb,
    TorchTensorView gt_depth,
    TorchTensorView gt_normal,
    TorchTensorView gt_alpha,
    bool input_depth_is_ray_depth
) {
    engine().gt.rgb    = _upload_gt_rgb(gt_rgb);
    engine().gt.depth  = _hv_to_dt3d_gt<float>(gt_depth,   PoolSlot::GtDepth,  "depth");
    engine().gt.normal = _hv_to_dt3d_gt<float3>(gt_normal, PoolSlot::GtNormal, "normal");
    // Mask labels use the byte ABI in core/TrainingMask.h.
    engine().gt.alpha  = _hv_to_dt3d<uint8_t>(PoolSlot::GtAlpha, gt_alpha);
    engine().gt.has_gt    = (std::get<0>(gt_rgb) != 0);
    engine().gt.has_mask  = (std::get<0>(gt_alpha) != 0);

    // Linear (z) depth -> ray depth, in place on the GPU buffer, before the
    // depth bilateral grid / loss consume it. The rasterizer renders ray
    // depth, so linear supervision depth must be converted to match. Uses the
    // current camera intrinsics (set by set_camera_params just before this),
    // rescaled inside the kernel to the depth map resolution.
    if (!input_depth_is_ray_depth && engine().gt.depth.data_ptr() != nullptr) {
        linear_depth_to_ray_depth_inplace(
            engine().camera.model_str,
        engine().camera.distortion_str,
            _dv_tv(engine().camera.intrins),
            _dt2d_tv(engine().camera.dist_coeffs),
            engine().camera.width, engine().camera.height,
            engine().gt.depth
        );
    }

    // Apply image-side color space conversion (linear / wide-gamut -> sRGB)
    // in place on the freshly uploaded GT. No-op when no image color space
    // was configured.
    _engine_color_space_apply_to_gt();
}
