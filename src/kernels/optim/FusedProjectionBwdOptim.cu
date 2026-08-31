#include "kernels/optim/FusedProjectionBwdOptim.cuh"

#include "kernels/projection/CameraVariants.cuh"

#include <core/Common.cuh>

#include <cooperative_groups.h>
namespace cg = cooperative_groups;

#include <cub/cub.cuh>


template<
    typename SplatPrimitive,
    CameraModelType camera_model,
    CameraDistortionType distortion,
    const bool use_scale_agnostic_mean,
    const bool color_trust_linear,
    const int  LEVEL
>
void fused_projection_bwd_optimizer_3dgs_kernel_wrapper(
    cudaStream_t stream,
    // fwd inputs
    const uint32_t C,
    const uint32_t N,
    const uint32_t num_sh_buffer,
    typename SplatPrimitive::WorldBuffer splats_world,
    const float *__restrict__ viewmats, // [C, 4, 4]
    const float4 *__restrict__ intrins,  // [C, 4], fx, fy, cx, cy
    const CameraDistortionCoeffsBuffer dist_coeffs_buffer,
    const uint32_t image_width,
    const uint32_t image_height,
    // fwd outputs
    const int32_t *__restrict__ camera_id_bounds,   // [N+1]
    const int32_t *__restrict__ camera_ids,   // [nnz] -- ORIGINAL (unsorted) order
    const uint2 *__restrict__ aabb,    // [C, N] or [nnz], packed
    // grad outputs from rasterization
    typename SplatPrimitive::WorldBuffer v_splats_world,
    typename SplatPrimitive::ScreenBuffer v_splats_screen,
    // optimizer states
    typename SplatPrimitive::WorldBuffer g1_splats_world,
    typename SplatPrimitive::WorldBuffer g2_splats_world,
    const uint8_t* __restrict__ sh_packed,      // AoS (u, sqrt_g2) packed SH state
    float4* __restrict__ sh_quant_bounds,
    const uint8_t* __restrict__ sh_value_packed,
    float2* __restrict__ sh_value_bounds,
    NonShQuantState non_sh,
    // float *__restrict__ v_viewmats // [C, 4, 4] optional
    // optimizer params
    const float* __restrict__ radii,
    float* __restrict__ densify_score,
    const float lr_means,
    const float lr_quats,
    const float lr_scales,
    const float lr_opacs,
    const float lr_features_dc,
    const float lr_features_sh,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    const float eps_tr,
    const int32_t scalar_step,
    const int32_t* __restrict__ steps
);



// lower_bound over the sorted list, one thread per OUTPUT slot. Filling the
// gaps from the input side makes one thread walk every id a sub-batch never
// saw: 478 ms/call at N=20M, nnz=0.8M against ~2 ms here.
__global__ void camera_id_bounds_kernel(
    int64_t nnz,
    int64_t N,
    const int32_t* __restrict__ gaussian_ids,  // [nnz]
    int32_t* __restrict__ camera_id_bounds  // [N+1]
) {
    int64_t k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k > N)
        return;

    int32_t lo = 0, hi = (int32_t)nnz;
    while (lo < hi) {
        int32_t mid = lo + ((hi - lo) >> 1);
        if (gaussian_ids[mid] < (int32_t)k) lo = mid + 1;
        else hi = mid;
    }
    camera_id_bounds[k] = lo;
}


template<
    typename SplatPrimitive,
    bool use_scale_agnostic_mean,
    bool color_trust_linear,
    int  LEVEL
>
inline void launch_fused_projection_bwd_optimizer_3dgs_kernel(
    // fwd inputs
    const int64_t N,
    const uint32_t num_sh_buffer,
    std::vector<DeviceTensorFloatND> splats_world,
    TorchTensorView viewmats,  // [..., C, 4, 4]
    TorchTensorView intrins,  // [..., C, 4], fx, fy, cx, cy
    const uint32_t image_width,
    const uint32_t image_height,
    const CameraModelType camera_model,
    const CameraDistortionType distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    DeviceVector<int32_t> camera_ids,
    DeviceVector<int32_t> gaussian_ids,
    DeviceTensor2D<uint2> aabb,
    // grad outputs
    const std::vector<DeviceTensorFloatND> v_splats_world,
    const std::vector<DeviceTensorFloatND> v_splats_screen,
    // optimizer states
    const std::vector<DeviceTensorFloatND> g1_splats_world,
    const std::vector<DeviceTensorFloatND> g2_splats_world,
    const std::optional<TorchTensorView> sh_packed,         // AoS packed SH state
    const std::optional<TorchTensorView> sh_quant_bounds,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    NonShQuantState non_sh,
    // optimizer params
    DeviceVector<float> radii,
    DeviceVector<float> densify_score,
    const float lr_means,
    const float lr_quats,
    const float lr_scales,
    const float lr_opacs,
    const float lr_features_dc,
    const float lr_features_sh,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    const float eps_tr,
    const int32_t scalar_step,
    const std::optional<TorchTensorView> steps
) {
    uint32_t C = (uint32_t)std::get<2>(viewmats)[0]; // number of cameras (first dim)
    // Note: viewmats shape is [C, 4, 4], so index 0 = C

    if (N == 0)
        return;

    bool packed = camera_ids.data_ptr() && gaussian_ids.data_ptr();

    DeviceVector<int32_t> camera_id_bounds;

    if (packed) {
        long nnz = camera_ids.size();
        // The forward emits the list in (gaussian, camera) order, so
        // gaussian_ids is already non-decreasing and cid_t is the out_idx --
        // no sort, no permutation to carry.
        camera_id_bounds.resize(PoolSlot::FusedProjBwdCamBounds, (int64_t)(N+1));
        camera_id_bounds_kernel<<<_LAUNCH_ARGS_1D(N+1, 256)>>>(
            nnz, N, gaussian_ids.data_ptr(), camera_id_bounds.data_ptr()
        );
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    const float* viewmats_ptr = (const float*)std::get<0>(viewmats);
    const float4* intrins_ptr = (const float4*)std::get<0>(intrins);
    const int32_t* steps_ptr = steps.has_value() ? (const int32_t*)std::get<0>(steps.value()) : nullptr;

    #define _LAUNCH_ARGS ( \
            (cudaStream_t)0, C, N, num_sh_buffer, \
            splats_world, viewmats_ptr, intrins_ptr, dist_coeffs, \
            image_width, image_height, \
            packed ? camera_id_bounds.data_ptr() : nullptr, \
            packed ? camera_ids.data_ptr() : nullptr, \
            aabb.data_ptr(), \
            v_splats_world, \
            v_splats_screen, \
            g1_splats_world, g2_splats_world, \
            sh_packed.has_value() ? (const uint8_t*)std::get<0>(sh_packed.value()) : nullptr, \
            sh_quant_bounds.has_value() ? (float4*)std::get<0>(sh_quant_bounds.value()) : nullptr, \
            sh_value_packed.has_value() ? (const uint8_t*)std::get<0>(sh_value_packed.value()) : nullptr, \
            sh_value_bounds.has_value() ? (float2*)std::get<0>(sh_value_bounds.value()) : nullptr, \
            non_sh, \
            /*v_viewmats.has_value() ? v_viewmats.value().data_ptr<float>() : nullptr */ \
            radii.data_ptr(), \
            densify_score.data_ptr(), \
            lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc, lr_features_sh, \
            max_gauss_ratio, scale_regularization_weight, \
            mcmc_opacity_reg_weight, mcmc_scale_reg_weight, erank_reg_weight, erank_reg_weight_s3, quat_norm_reg_weight, dc_reg_weight, sh_reg_weight, \
            max_screen_size, max_screen_size_penalty, \
            eps_tr, \
            scalar_step, steps_ptr \
        )

    #define _DISPATCH(M, D) \
        if (camera_model == CameraModelType::M && distortion == CameraDistortionType::D) \
            fused_projection_bwd_optimizer_3dgs_kernel_wrapper<SplatPrimitive, \
                CameraModelType::M, CameraDistortionType::D, \
                use_scale_agnostic_mean, color_trust_linear, LEVEL> _LAUNCH_ARGS; else
    SS_FOR_EACH_CAMERA_VARIANT(_DISPATCH)
        throw std::runtime_error("Unsupported camera model / distortion tier");
    #undef _DISPATCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    #undef _LAUNCH_ARGS

}



// ================
// Per-primitive host wrappers
// ================
//
// All three primitives (3DGS, MipSplatting, 3DGUT) share the same kernel and
// host-launcher body — they only differ in which `PrimT<sh_degree>` template
// alias is plugged into the templated `launch_*` call. Factor the dispatch
// into one helper templated on the primitive class template, then expose a
// thin wrapper per primitive.

template<template<int> class PrimT>
static inline void _fused_projection_bwd_optimizer_dispatch(
    // fwd inputs
    const int64_t num_splats,
    const int max_sh_degree,
    std::vector<DeviceTensorFloatND> splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    // fwd outputs
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    DeviceTensor2D<uint2> aabb,
    // grad outputs
    const std::vector<DeviceTensorFloatND> v_splats_world,
    const std::vector<DeviceTensorFloatND> v_splats_screen,
    // optimizer states
    const std::vector<DeviceTensorFloatND> g1_splats_world,
    const std::vector<DeviceTensorFloatND> g2_splats_world,
    const std::optional<TorchTensorView> sh_packed,
    const std::optional<TorchTensorView> sh_quant_bounds,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    NonShQuantState non_sh,
    // optimizer params
    DeviceVector<float> radii,
    DeviceVector<float> densify_score,
    const float lr_means,
    const float lr_quats,
    const float lr_scales,
    const float lr_opacs,
    const float lr_features_dc,
    const float lr_features_sh,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    bool use_scale_agnostic_mean,
    bool color_trust_linear,
    float eps_tr,
    std::variant<int32_t, TorchTensorView> step,
    int quantization_level
) {
    int32_t scalar_step = std::get_if<int32_t>(&step) ? std::get<int32_t>(step) : -1;
    std::optional<TorchTensorView> steps_view = std::get_if<TorchTensorView>(&step) ?
        (std::optional<TorchTensorView>)std::get<TorchTensorView>(step) : std::nullopt;

    // Buffer's actual SH coef count (3 * (buffer_sh_degree*(buffer_sh_degree+2))).
    // The launch dispatch below caps the kernel's *template* sh degree at the
    // runtime sh_degree_to_use; the packed SH-Adam buffer is sized for the
    // model's max degree (engine().num_sh) and must be indexed with that
    // stride regardless of warmup.
    const int _buf_sh_deg = typename PrimT<0>::WorldBuffer(splats_world).sh_degree();
    const uint32_t num_sh_buffer = (uint32_t)(_buf_sh_deg * (_buf_sh_deg + 2));
    #define _ARGS ( \
        num_splats, num_sh_buffer, \
        splats_world, \
        viewmats, \
        intrins, \
        image_width, \
        image_height, \
        cmt(camera_model), \
        cdt(distortion), \
        dist_coeffs, \
        camera_ids, \
        gaussian_ids, \
        aabb, \
        v_splats_world, \
        v_splats_screen, \
        g1_splats_world, \
        g2_splats_world, \
        sh_packed, \
        sh_quant_bounds, \
        sh_value_packed, \
        sh_value_bounds, \
        non_sh, \
        radii, \
        densify_score, \
        lr_means, \
        lr_quats, \
        lr_scales, \
        lr_opacs, \
        lr_features_dc, \
        lr_features_sh, \
        max_gauss_ratio, \
        scale_regularization_weight, \
        mcmc_opacity_reg_weight, \
        mcmc_scale_reg_weight, \
        erank_reg_weight, \
        erank_reg_weight_s3, \
        quat_norm_reg_weight, \
        dc_reg_weight, \
        sh_reg_weight, \
        max_screen_size, \
        max_screen_size_penalty, \
        eps_tr, \
        scalar_step, \
        steps_view \
    )
    // Match projection_*_backward: cap kernel-template SH degree at the
    // runtime sh_degree_to_use so forward/backward agree on which bands
    // contribute (otherwise SH backward computes a spurious gradient on
    // unused bands, which propagates through `v_viewdir` into `v_mean`).
    int sh_degree = typename PrimT<0>::WorldBuffer(splats_world).sh_degree();
    sh_degree = std::min(sh_degree, max_sh_degree);
    // Pack (use_scale_agnostic_mean, color_trust_linear) into a 2-bit key.
    // LEVEL also becomes a template arg so each kernel.cu compiles ONE
    // specialization. LEVEL collapses the prior (QUANT_BITS, VALUE_BITS) axes
    // down to 2 valid combos:
    //   0 = off (32-bit value, fp32 optim)
    //   1 = light (16-bit value, 8-bit packed optim)
    const int dispatch_key =
        (int)use_scale_agnostic_mean
        | ((int)color_trust_linear << 1);
    // Validate up front: bits == 32 / packed-not-allocated -> LEVEL must be 0;
    // any non-zero level requires the optim+value packed buffers to be live.
    if (quantization_level < 0 || quantization_level > 1)
        throw std::runtime_error(
            "fused_projection_bwd_optimizer: quantization_level must be "
            "0 or 1; got " + std::to_string(quantization_level));
    #define LAUNCH_LEVEL(n, sam, ctl, level) \
        if (sh_degree == (n) && dispatch_key == ((int)(sam) | ((int)(ctl) << 1)) \
            && quantization_level == (level)) \
            return (void)launch_fused_projection_bwd_optimizer_3dgs_kernel< \
                PrimT<n>, sam, ctl, level> _ARGS;
    #define LAUNCH_LEVELS(n, sam, ctl) \
        LAUNCH_LEVEL(n, sam, ctl, 0) \
        LAUNCH_LEVEL(n, sam, ctl, 1)
    #define LAUNCH(n) \
        LAUNCH_LEVELS(n, false, false) \
        LAUNCH_LEVELS(n, true,  false) \
        LAUNCH_LEVELS(n, false, true) \
        LAUNCH_LEVELS(n, true,  true)
    LAUNCH(3) LAUNCH(2) LAUNCH(1) LAUNCH(0) LAUNCH(4)
    #undef LAUNCH
    #undef LAUNCH_LEVELS
    #undef LAUNCH_LEVEL
    #undef _ARGS
}


// Per-primitive thin wrappers. Identical signatures — the auto-header
// generator extracts each declaration via regex (no preprocessor), so the
// argument list is spelled out verbatim rather than macro-folded.

/*[AutoHeaderGeneratorExport]*/
void fused_projection_bwd_optimizer_3dgs(
    const int64_t num_splats,
    const int max_sh_degree,
    std::vector<DeviceTensorFloatND> splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND> v_splats_world,
    const std::vector<DeviceTensorFloatND> v_splats_screen,
    const std::vector<DeviceTensorFloatND> g1_splats_world,
    const std::vector<DeviceTensorFloatND> g2_splats_world,
    const std::optional<TorchTensorView> sh_packed,
    const std::optional<TorchTensorView> sh_quant_bounds,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    NonShQuantState non_sh,
    DeviceVector<float> radii,
    DeviceVector<float> densify_score,
    const float lr_means,
    const float lr_quats,
    const float lr_scales,
    const float lr_opacs,
    const float lr_features_dc,
    const float lr_features_sh,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    bool use_scale_agnostic_mean,
    bool color_trust_linear,
    float eps_tr,
    std::variant<int32_t, TorchTensorView> step,
    int quantization_level
) {
    _fused_projection_bwd_optimizer_dispatch<Vanilla3DGS>(
        num_splats, max_sh_degree, splats_world, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_world, v_splats_screen,
        g1_splats_world, g2_splats_world, sh_packed, sh_quant_bounds,
        sh_value_packed, sh_value_bounds,
        non_sh,
        radii, densify_score, lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc,
        lr_features_sh, max_gauss_ratio, scale_regularization_weight,
        mcmc_opacity_reg_weight, mcmc_scale_reg_weight,
        erank_reg_weight, erank_reg_weight_s3, quat_norm_reg_weight,
        dc_reg_weight, sh_reg_weight,
        max_screen_size, max_screen_size_penalty, use_scale_agnostic_mean,
        color_trust_linear, eps_tr, step,
        quantization_level);
}

/*[AutoHeaderGeneratorExport]*/
void fused_projection_bwd_optimizer_mip(
    const int64_t num_splats,
    const int max_sh_degree,
    std::vector<DeviceTensorFloatND> splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND> v_splats_world,
    const std::vector<DeviceTensorFloatND> v_splats_screen,
    const std::vector<DeviceTensorFloatND> g1_splats_world,
    const std::vector<DeviceTensorFloatND> g2_splats_world,
    const std::optional<TorchTensorView> sh_packed,
    const std::optional<TorchTensorView> sh_quant_bounds,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    NonShQuantState non_sh,
    DeviceVector<float> radii,
    DeviceVector<float> densify_score,
    const float lr_means,
    const float lr_quats,
    const float lr_scales,
    const float lr_opacs,
    const float lr_features_dc,
    const float lr_features_sh,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    bool use_scale_agnostic_mean,
    bool color_trust_linear,
    float eps_tr,
    std::variant<int32_t, TorchTensorView> step,
    int quantization_level
) {
    _fused_projection_bwd_optimizer_dispatch<MipSplatting>(
        num_splats, max_sh_degree, splats_world, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_world, v_splats_screen,
        g1_splats_world, g2_splats_world, sh_packed, sh_quant_bounds,
        sh_value_packed, sh_value_bounds,
        non_sh,
        radii, densify_score, lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc,
        lr_features_sh, max_gauss_ratio, scale_regularization_weight,
        mcmc_opacity_reg_weight, mcmc_scale_reg_weight,
        erank_reg_weight, erank_reg_weight_s3, quat_norm_reg_weight,
        dc_reg_weight, sh_reg_weight,
        max_screen_size, max_screen_size_penalty, use_scale_agnostic_mean,
        color_trust_linear, eps_tr, step,
        quantization_level);
}

/*[AutoHeaderGeneratorExport]*/
void fused_projection_bwd_optimizer_3dgut(
    const int64_t num_splats,
    const int max_sh_degree,
    std::vector<DeviceTensorFloatND> splats_world,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    const uint32_t image_width,
    const uint32_t image_height,
    const std::string camera_model,
    const std::string distortion,
    const TorchTensorView dist_coeffs,
    const DeviceVector<int32_t> camera_ids,
    const DeviceVector<int32_t> gaussian_ids,
    DeviceTensor2D<uint2> aabb,
    const std::vector<DeviceTensorFloatND> v_splats_world,
    const std::vector<DeviceTensorFloatND> v_splats_screen,
    const std::vector<DeviceTensorFloatND> g1_splats_world,
    const std::vector<DeviceTensorFloatND> g2_splats_world,
    const std::optional<TorchTensorView> sh_packed,
    const std::optional<TorchTensorView> sh_quant_bounds,
    const std::optional<TorchTensorView> sh_value_packed,
    const std::optional<TorchTensorView> sh_value_bounds,
    NonShQuantState non_sh,
    DeviceVector<float> radii,
    DeviceVector<float> densify_score,
    const float lr_means,
    const float lr_quats,
    const float lr_scales,
    const float lr_opacs,
    const float lr_features_dc,
    const float lr_features_sh,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    bool use_scale_agnostic_mean,
    bool color_trust_linear,
    float eps_tr,
    std::variant<int32_t, TorchTensorView> step,
    int quantization_level
) {
    _fused_projection_bwd_optimizer_dispatch<Vanilla3DGUT>(
        num_splats, max_sh_degree, splats_world, viewmats, intrins, image_width, image_height,
        camera_model, distortion, dist_coeffs, camera_ids, gaussian_ids, aabb,
        v_splats_world, v_splats_screen,
        g1_splats_world, g2_splats_world, sh_packed, sh_quant_bounds,
        sh_value_packed, sh_value_bounds,
        non_sh,
        radii, densify_score, lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc,
        lr_features_sh, max_gauss_ratio, scale_regularization_weight,
        mcmc_opacity_reg_weight, mcmc_scale_reg_weight,
        erank_reg_weight, erank_reg_weight_s3, quat_norm_reg_weight,
        dc_reg_weight, sh_reg_weight,
        max_screen_size, max_screen_size_penalty, use_scale_agnostic_mean,
        color_trust_linear, eps_tr, step,
        quantization_level);
}


