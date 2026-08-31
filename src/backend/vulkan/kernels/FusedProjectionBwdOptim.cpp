// Vulkan implementation of the fused projection-backward + optimizer launch
// API (kernels/optim/FusedProjectionBwdOptim.cuh). Mirrors FusedProjectionBwdOptim.cu's
// launcher (packed preprocessing shared with the quantgrad launcher via
// vkk::build_packed_camera_ranges, regularizer weights pre-divided by N);
// the device work runs shaders/fpbo.slang with the CUDA template axes
// as spec constants.

#include <kernels/optim/FusedProjectionBwdOptim.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors FpboParams in shaders/fpbo.slang.
struct FpboParams {
    uint64_t means, quats, scales, opacities, features_dc, features_sh;
    uint64_t viewmats, intrins, dist_coeffs;
    uint64_t camera_id_bounds, camera_ids, aabb;
    uint64_t vs_screen;
    uint64_t vw_means, vw_quats, vw_scales, vw_opacities, vw_dc;
    uint64_t g1_means, g1_quats, g1_scales, g1_opacities, g1_dc, g1_sh;
    uint64_t g2_means, g2_quats, g2_scales, g2_opacities, g2_dc, g2_sh;
    uint64_t sh_packed, sh_quant_bounds, sh_value_packed, sh_value_bounds;
    uint64_t nq_means_packed, nq_quats_packed, nq_scales_packed,
        nq_opacities_packed, nq_dc_packed;
    uint64_t nq_means_bounds, nq_quats_bounds, nq_scales_bounds,
        nq_opacities_bounds, nq_dc_bounds;
    uint64_t radii, densify_score, steps;
    float lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc,
        lr_features_sh;
    float max_gauss_ratio, scale_regularization_weight;
    float mcmc_opacity_reg_weight, mcmc_scale_reg_weight;
    float erank_reg_weight, erank_reg_weight_s3;
    float quat_norm_reg_weight, dc_reg_weight, sh_reg_weight;
    float max_screen_size, max_screen_size_penalty;
    float eps_tr;
    int32_t scalar_step;
    uint32_t has_steps, has_densify_score;
    uint32_t C, N;
    uint32_t width, height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};
static_assert(sizeof(FpboParams) == 47 * 8 + 18 * 4 + 10 * 4,
              "params layout must match the slang struct");

using vkk::or_fallback;

void launch_fpbo_vk(
    const bool eval3d, const bool antialiased,
    const int64_t N,
    const int max_sh_degree,
    std::vector<DeviceTensorFloatND>& splats_world,
    TorchTensorView viewmats, TorchTensorView intrins,
    const uint32_t image_width, const uint32_t image_height,
    const std::string& camera_model, const std::string& distortion,
    const TorchTensorView& dist_coeffs,
    const DeviceVector<int32_t>& camera_ids,
    const DeviceVector<int32_t>& gaussian_ids,
    DeviceTensor2D<uint2>& aabb,
    const std::vector<DeviceTensorFloatND>& v_splats_world,
    const std::vector<DeviceTensorFloatND>& v_splats_screen,
    const std::vector<DeviceTensorFloatND>& g1_splats_world,
    const std::vector<DeviceTensorFloatND>& g2_splats_world,
    const std::optional<TorchTensorView>& sh_packed,
    const std::optional<TorchTensorView>& sh_quant_bounds,
    const std::optional<TorchTensorView>& sh_value_packed,
    const std::optional<TorchTensorView>& sh_value_bounds,
    NonShQuantState non_sh,
    DeviceVector<float>& radii,
    DeviceVector<float>& densify_score,
    const float lr_means, const float lr_quats, const float lr_scales,
    const float lr_opacs, const float lr_features_dc,
    const float lr_features_sh, const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight, const float mcmc_scale_reg_weight,
    const float erank_reg_weight, const float erank_reg_weight_s3,
    const float quat_norm_reg_weight, const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size, const float max_screen_size_penalty,
    bool use_scale_agnostic_mean, bool color_trust_linear, float eps_tr,
    std::variant<int32_t, TorchTensorView> step, int quantization_level) {
    if (N == 0)
        return;
    if (quantization_level < 0 || quantization_level > 1)
        throw std::runtime_error(
            "fused_projection_bwd_optimizer: quantization_level must be "
            "0 or 1; got " + std::to_string(quantization_level));

    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);

    Vanilla3DGS<0>::WorldBuffer wb(splats_world);
    Vanilla3DGS<0>::WorldBuffer vwb(
        const_cast<std::vector<DeviceTensorFloatND>&>(v_splats_world));
    Vanilla3DGS<0>::WorldBuffer g1b(
        const_cast<std::vector<DeviceTensorFloatND>&>(g1_splats_world));
    Vanilla3DGS<0>::WorldBuffer g2b(
        const_cast<std::vector<DeviceTensorFloatND>&>(g2_splats_world));

    // Buffer's SH stride is the model max; the kernel template degree is
    // capped at the runtime warmup degree (mirrors the CUDA dispatcher).
    const int buf_sh_deg = wb.sh_degree();
    const uint32_t num_sh_buffer = (uint32_t)(buf_sh_deg * (buf_sh_deg + 2));
    int sh_degree = std::min(buf_sh_deg, max_sh_degree);
    if ((uint64_t)3 * num_sh_buffer * (uint64_t)N > UINT32_MAX)
        throw std::runtime_error(
            "fused_projection_bwd_optimizer: SH cell count exceeds 2^32");

    const uint32_t C = (uint32_t)std::get<2>(viewmats)[0];
    const bool packed = (camera_ids.data_ptr() && gaussian_ids.data_ptr());

    vkk::PackedCameraRanges ranges;
    if (packed)
        ranges = vkk::build_packed_camera_ranges(gaussian_ids, N);

    const int32_t scalar_step =
        std::get_if<int32_t>(&step) ? std::get<int32_t>(step) : -1;
    const uint64_t steps_ptr =
        std::get_if<TorchTensorView>(&step)
            ? std::get<0>(std::get<TorchTensorView>(step))
            : 0;

    const bool level1 = quantization_level == 1;
    if (level1 && sh_degree > 0 &&
        (!sh_packed.has_value() || !sh_quant_bounds.has_value() ||
         !sh_value_packed.has_value() || !sh_value_bounds.has_value()))
        throw std::runtime_error(
            "fused_projection_bwd_optimizer: quantization_level 1 requires "
            "the packed SH state + value buffers");

    uint32_t vw_mask = 0;
    for (int c = 0; c < 5; c++)
        if (vwb.raw_data(c))
            vw_mask |= 1u << c;

    FpboParams p{};
    p.means = (uint64_t)wb.raw_data(0);
    p.quats = (uint64_t)wb.raw_data(1);
    p.scales = (uint64_t)wb.raw_data(2);
    p.opacities = (uint64_t)wb.raw_data(3);
    p.features_dc = (uint64_t)wb.raw_data(4);
    p.features_sh = or_fallback((uint64_t)wb.raw_data(5));
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
    p.vw_opacities = or_fallback((uint64_t)vwb.raw_data(3));
    p.vw_dc = or_fallback((uint64_t)vwb.raw_data(4));
    p.g1_means = or_fallback((uint64_t)g1b.raw_data(0));
    p.g1_quats = or_fallback((uint64_t)g1b.raw_data(1));
    p.g1_scales = or_fallback((uint64_t)g1b.raw_data(2));
    p.g1_opacities = or_fallback((uint64_t)g1b.raw_data(3));
    p.g1_dc = or_fallback((uint64_t)g1b.raw_data(4));
    p.g1_sh = or_fallback((uint64_t)g1b.raw_data(5));
    p.g2_means = or_fallback((uint64_t)g2b.raw_data(0));
    p.g2_quats = or_fallback((uint64_t)g2b.raw_data(1));
    p.g2_scales = or_fallback((uint64_t)g2b.raw_data(2));
    p.g2_opacities = or_fallback((uint64_t)g2b.raw_data(3));
    p.g2_dc = or_fallback((uint64_t)g2b.raw_data(4));
    p.g2_sh = or_fallback((uint64_t)g2b.raw_data(5));
    p.sh_packed = or_fallback(
        sh_packed.has_value() ? std::get<0>(sh_packed.value()) : 0);
    p.sh_quant_bounds = or_fallback(
        sh_quant_bounds.has_value() ? std::get<0>(sh_quant_bounds.value())
                                    : 0);
    p.sh_value_packed = or_fallback(
        sh_value_packed.has_value() ? std::get<0>(sh_value_packed.value())
                                    : 0);
    p.sh_value_bounds = or_fallback(
        sh_value_bounds.has_value() ? std::get<0>(sh_value_bounds.value())
                                    : 0);
    p.nq_means_packed = or_fallback(non_sh.means_packed);
    p.nq_quats_packed = or_fallback(non_sh.quats_packed);
    p.nq_scales_packed = or_fallback(non_sh.scales_packed);
    p.nq_opacities_packed = or_fallback(non_sh.opacities_packed);
    p.nq_dc_packed = or_fallback(non_sh.features_dc_packed);
    p.nq_means_bounds = or_fallback(non_sh.means_bounds);
    p.nq_quats_bounds = or_fallback(non_sh.quats_bounds);
    p.nq_scales_bounds = or_fallback(non_sh.scales_bounds);
    p.nq_opacities_bounds = or_fallback(non_sh.opacities_bounds);
    p.nq_dc_bounds = or_fallback(non_sh.features_dc_bounds);
    p.radii = or_fallback(radii.data_ptr());
    p.densify_score = or_fallback(densify_score.data_ptr());
    p.steps = or_fallback(steps_ptr);
    p.lr_means = lr_means;
    p.lr_quats = lr_quats;
    p.lr_scales = lr_scales;
    p.lr_opacs = lr_opacs;
    p.lr_features_dc = lr_features_dc;
    p.lr_features_sh = lr_features_sh;
    p.max_gauss_ratio = max_gauss_ratio;
    // Per-splat weights are averaged over N (mirrors the CUDA wrapper).
    p.scale_regularization_weight = scale_regularization_weight / (float)N;
    p.mcmc_opacity_reg_weight = mcmc_opacity_reg_weight / (float)N;
    p.mcmc_scale_reg_weight = mcmc_scale_reg_weight / (float)N;
    p.erank_reg_weight = erank_reg_weight / (float)N;
    p.erank_reg_weight_s3 = erank_reg_weight_s3 / (float)N;
    p.quat_norm_reg_weight = quat_norm_reg_weight / (float)N;
    p.dc_reg_weight = 2.0f * dc_reg_weight / 3.0f;
    p.sh_reg_weight = 2.0f * sh_reg_weight / (float)(3 * N);
    p.max_screen_size = max_screen_size;
    p.max_screen_size_penalty =
        radii.data_ptr() ? max_screen_size_penalty : 0.0f;
    p.eps_tr = eps_tr;
    p.scalar_step = scalar_step;
    p.has_steps = steps_ptr ? 1u : 0u;
    p.has_densify_score = densify_score.data_ptr() ? 1u : 0u;
    p.num_sh_buffer = num_sh_buffer;
    p.C = C;
    p.N = (uint32_t)N;
    p.width = image_width;
    p.height = image_height;

    vkk::Fold f = vkk::fold_1d(N, 256);
    p.wgs_per_row = f.per_row;
    backend::vk::SpecList spec{
        cd.cam,
        (uint32_t)sh_degree,
        antialiased ? 1u : 0u,
        level1 ? 16u : 0u,  // kShValueBits: level 1 reads SH via the codec
        packed ? 1u : 0u,
        eval3d ? 1u : 0u,
        use_scale_agnostic_mean ? 1u : 0u,
        color_trust_linear ? 1u : 0u,
        level1 ? 1u : 0u,
        (level1 && non_sh.enabled) ? 1u : 0u,
        vw_mask,
        cd.dist};
    vkk::dispatch_ring("fpbo.fpbo", spec, f.per_row, f.rows, 1, &p,
                       sizeof(p));
}

}  // namespace

/* API definitions matching kernels/optim/FusedProjectionBwdOptim.cuh */

#define _FPBO_ARGS                                                          \
    num_splats, max_sh_degree, splats_world, viewmats, intrins,             \
        image_width, image_height, camera_model, distortion, dist_coeffs,   \
        camera_ids,                                                         \
        gaussian_ids, aabb, v_splats_world, v_splats_screen,                \
        g1_splats_world, g2_splats_world, sh_packed, sh_quant_bounds,       \
        sh_value_packed, sh_value_bounds, non_sh, radii, densify_score,     \
        lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc,            \
        lr_features_sh, max_gauss_ratio, scale_regularization_weight,       \
        mcmc_opacity_reg_weight, mcmc_scale_reg_weight, erank_reg_weight,   \
        erank_reg_weight_s3, quat_norm_reg_weight, dc_reg_weight,           \
        sh_reg_weight, max_screen_size, max_screen_size_penalty,            \
        use_scale_agnostic_mean, color_trust_linear,                        \
        eps_tr, step, quantization_level

#define _FPBO_PARAMS                                                        \
    const int64_t num_splats, const int max_sh_degree,                      \
        std::vector<DeviceTensorFloatND> splats_world,                      \
        TorchTensorView viewmats, TorchTensorView intrins,                  \
        const uint32_t image_width, const uint32_t image_height,            \
        const std::string camera_model, const std::string distortion,       \
        const TorchTensorView dist_coeffs,                                  \
        const DeviceVector<int32_t> camera_ids,                             \
        const DeviceVector<int32_t> gaussian_ids, DeviceTensor2D<uint2> aabb, \
        const std::vector<DeviceTensorFloatND> v_splats_world,              \
        const std::vector<DeviceTensorFloatND> v_splats_screen,             \
        const std::vector<DeviceTensorFloatND> g1_splats_world,             \
        const std::vector<DeviceTensorFloatND> g2_splats_world,             \
        const std::optional<TorchTensorView> sh_packed,                     \
        const std::optional<TorchTensorView> sh_quant_bounds,               \
        const std::optional<TorchTensorView> sh_value_packed,               \
        const std::optional<TorchTensorView> sh_value_bounds,               \
        NonShQuantState non_sh, DeviceVector<float> radii,                  \
        DeviceVector<float> densify_score, const float lr_means,            \
        const float lr_quats, const float lr_scales, const float lr_opacs,  \
        const float lr_features_dc, const float lr_features_sh,             \
        const float max_gauss_ratio,                                        \
        const float scale_regularization_weight,                            \
        const float mcmc_opacity_reg_weight,                                \
        const float mcmc_scale_reg_weight, const float erank_reg_weight,    \
        const float erank_reg_weight_s3, const float quat_norm_reg_weight,  \
        const float dc_reg_weight, const float sh_reg_weight,               \
        const float max_screen_size, const float max_screen_size_penalty,   \
        bool use_scale_agnostic_mean, bool color_trust_linear,              \
        float eps_tr,                                                       \
        std::variant<int32_t, TorchTensorView> step, int quantization_level

static void _fpbo_call(bool eval3d, bool antialiased, _FPBO_PARAMS) {
    launch_fpbo_vk(eval3d, antialiased, num_splats, max_sh_degree,
                   splats_world, viewmats, intrins, image_width, image_height,
                   camera_model, distortion, dist_coeffs, camera_ids,
                   gaussian_ids, aabb,
                   v_splats_world, v_splats_screen, g1_splats_world,
                   g2_splats_world, sh_packed, sh_quant_bounds,
                   sh_value_packed, sh_value_bounds, non_sh, radii,
                   densify_score, lr_means, lr_quats, lr_scales, lr_opacs,
                   lr_features_dc, lr_features_sh, max_gauss_ratio,
                   scale_regularization_weight, mcmc_opacity_reg_weight,
                   mcmc_scale_reg_weight, erank_reg_weight,
                   erank_reg_weight_s3, quat_norm_reg_weight, dc_reg_weight,
                   sh_reg_weight, max_screen_size, max_screen_size_penalty,
                   use_scale_agnostic_mean, color_trust_linear, eps_tr, step,
                   quantization_level);
}

void fused_projection_bwd_optimizer_3dgs(_FPBO_PARAMS) {
    _fpbo_call(/*eval3d=*/false, /*antialiased=*/false, _FPBO_ARGS);
}

void fused_projection_bwd_optimizer_mip(_FPBO_PARAMS) {
    _fpbo_call(/*eval3d=*/false, /*antialiased=*/true, _FPBO_ARGS);
}

void fused_projection_bwd_optimizer_3dgut(_FPBO_PARAMS) {
    _fpbo_call(/*eval3d=*/true, /*antialiased=*/false, _FPBO_ARGS);
}
