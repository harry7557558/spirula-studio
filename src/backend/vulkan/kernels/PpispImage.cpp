// Vulkan implementations of the PPISP image transform + regularization
// launch APIs (kernels/pixelwise/PixelWise.cuh PPISP subset) and the PpispInit.cu
// helpers (engine/EngineInternal.h). Device work: shaders/
// ppisp_image.slang (canonical shaders/ppisp.slang math) + the init kernels in
// bilagrid_tv.slang.

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Tensor.h>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors PpispImageParams in shaders/ppisp_image.slang.
struct PpispImageParams {
    uint64_t in_image, ppisp_params, intrins, cam_indices, out_image,
        v_in_image, v_ppisp_params;
    int32_t B, H, W;
    float actual_image_width, actual_image_height;
    int32_t has_cam_indices;
    uint32_t wgs_per_row;
    int32_t _pad0;
};
static_assert(sizeof(PpispImageParams) == 7 * 8 + 8 * 4, "layout");

// Mirrors PpispRegParams.
struct PpispRegParams {
    uint64_t ppisp_params, raw_losses, losses, v_ppisp_params;
    float loss_weights[(int)PPISPRegLossIndex::length];
    int32_t B;
    int32_t _pad0, _pad1, _pad2;
};
static_assert(sizeof(PpispRegParams) ==
                  4 * 8 + ((int)PPISPRegLossIndex::length + 4) * 4,
              "layout");

struct PpispInitParams {
    uint64_t params;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(PpispInitParams) == 8 + 2 * 4, "layout");

struct AddIntoGradParams {
    uint64_t src, dst;
    uint32_t total, wgs_per_row;
};
static_assert(sizeof(AddIntoGradParams) == 2 * 8 + 2 * 4, "layout");

// (kParamType, kClampOutput), in ppisp_image.slang declaration order.
backend::vk::SpecList spec_list(const PpispParamSpec& spec) {
    return backend::vk::SpecList{(uint32_t)spec.layout,
                                 spec.clamp_output ? 1u : 0u};
}

// Fold the pixel axis across (gx, gz); gy carries the batch index.
void dispatch_image(const char* entry, const PpispParamSpec& spec,
                    int64_t pixels, uint32_t block, int64_t B,
                    PpispImageParams& p) {
    if (pixels <= 0 || B <= 0) return;
    vkk::Fold f = vkk::fold_1d(pixels, block);
    p.wgs_per_row = f.per_row;
    if (B > 65535 || f.rows > 65535)
        throw std::runtime_error("ppisp: image grid dimension exceeds 65535");
    vkk::dispatch(entry, spec_list(spec), f.per_row, (uint32_t)B, f.rows, &p,
                  sizeof(p));
}

}  // namespace

void ppisp_forward(
    DeviceTensor3D<float3> in_image, TorchTensorView ppisp_params,
    TorchTensorView intrins, const float actual_image_width,
    const float actual_image_height, std::string param_type,
    TorchTensorView cam_indices, DeviceTensor3D<float3> out_image
) {
    const int64_t b = in_image.size<0>();
    const int64_t h = in_image.size<1>();
    const int64_t w = in_image.size<2>();
    const PpispParamSpec spec = ppisp_param_spec(param_type);
    const uint64_t cam_ptr = std::get<0>(cam_indices);

    PpispImageParams p{};
    p.in_image = (uint64_t)in_image.data_ptr();
    p.ppisp_params = std::get<0>(ppisp_params);
    p.intrins = std::get<0>(intrins);
    p.cam_indices = vkk::or_fallback(cam_ptr);
    p.out_image = (uint64_t)out_image.data_ptr();
    p.v_in_image = vkk::or_fallback(nullptr);
    p.v_ppisp_params = vkk::or_fallback(nullptr);
    p.B = (int32_t)b;
    p.H = (int32_t)h;
    p.W = (int32_t)w;
    p.actual_image_width = actual_image_width;
    p.actual_image_height = actual_image_height;
    p.has_cam_indices = cam_ptr != 0 ? 1 : 0;
    dispatch_image("ppisp_image.ppisp_image_fwd", spec, h * w, 256, b, p);
}

void ppisp_backward(
    DeviceTensor3D<float3> in_image, TorchTensorView ppisp_params,
    TorchTensorView intrins, const float actual_image_width,
    const float actual_image_height, DeviceTensor3D<float3> v_out_image,
    std::string param_type, TorchTensorView cam_indices,
    DeviceTensor3D<float3> v_in_image, TorchTensorView v_ppisp_params
) {
    const int64_t b = in_image.size<0>();
    const int64_t h = in_image.size<1>();
    const int64_t w = in_image.size<2>();
    const PpispParamSpec spec = ppisp_param_spec(param_type);
    const uint64_t cam_ptr = std::get<0>(cam_indices);

    PpispImageParams p{};
    p.in_image = (uint64_t)in_image.data_ptr();
    p.ppisp_params = std::get<0>(ppisp_params);
    p.intrins = std::get<0>(intrins);
    p.cam_indices = vkk::or_fallback(cam_ptr);
    p.out_image = (uint64_t)v_out_image.data_ptr();
    p.v_in_image = (uint64_t)v_in_image.data_ptr();
    p.v_ppisp_params = std::get<0>(v_ppisp_params);
    p.B = (int32_t)b;
    p.H = (int32_t)h;
    p.W = (int32_t)w;
    p.actual_image_width = actual_image_width;
    p.actual_image_height = actual_image_height;
    p.has_cam_indices = cam_ptr != 0 ? 1 : 0;
    dispatch_image("ppisp_image.ppisp_image_bwd", spec, h * w, 64, b, p);
}

void compute_ppsip_regularization_forward(
    TorchTensorView ppisp_params,
    const std::array<float, (int)PPISPRegLossIndex::length> loss_weights_0,
    std::string param_type, TorchTensorView losses, TorchTensorView raw_losses
) {
    const PpispParamSpec spec = ppisp_param_spec(param_type);
    const int64_t B = std::get<2>(ppisp_params)[0];
    const int nr = spec.num_raw_losses;

    PpispRegParams p{};
    p.ppisp_params = std::get<0>(ppisp_params);
    p.raw_losses = std::get<0>(raw_losses);
    p.losses = vkk::or_fallback(nullptr);
    p.v_ppisp_params = vkk::or_fallback(nullptr);
    for (int i = 0; i < (int)PPISPRegLossIndex::length; i++)
        p.loss_weights[i] = loss_weights_0[i];
    p.B = (int32_t)B;
    vkk::dispatch("ppisp_image.ppisp_reg_raw_fwd", spec_list(spec),
                  (uint32_t)((B + 31) / 32), 1, 1, &p, sizeof(p));

    // Weighted final pass reads the summed tail row.
    PpispRegParams pf{};
    pf.ppisp_params = vkk::or_fallback(nullptr);
    pf.raw_losses =
        std::get<0>(raw_losses) + (uint64_t)B * nr * sizeof(float);
    pf.losses = std::get<0>(losses);
    pf.v_ppisp_params = vkk::or_fallback(nullptr);
    for (int i = 0; i < (int)PPISPRegLossIndex::length; i++)
        pf.loss_weights[i] = loss_weights_0[i];
    pf.B = (int32_t)B;
    vkk::dispatch("ppisp_image.ppisp_reg_final_fwd", spec_list(spec), 1, 1, 1,
                  &pf, sizeof(pf));
}

void compute_ppsip_regularization_backward(
    TorchTensorView ppisp_params,
    const std::array<float, (int)PPISPRegLossIndex::length> loss_weights_0,
    TorchTensorView raw_losses, TorchTensorView v_losses,
    std::string param_type, TorchTensorView v_ppisp_params
) {
    const PpispParamSpec spec = ppisp_param_spec(param_type);
    const int64_t B = std::get<2>(ppisp_params)[0];
    const int nr = spec.num_raw_losses;

    float* v_raw_losses =
        DevicePool::global().acquire<float>(PoolSlot::PpispVRawLosses, nr);
    backend::memset_sync(v_raw_losses, 0, nr * sizeof(float));

    // v_losses -> v_raw_losses (single thread).
    PpispRegParams pf{};
    pf.ppisp_params = vkk::or_fallback(nullptr);
    pf.raw_losses =
        std::get<0>(raw_losses) + (uint64_t)B * nr * sizeof(float);
    pf.losses = std::get<0>(v_losses);
    pf.v_ppisp_params = (uint64_t)v_raw_losses;
    for (int i = 0; i < (int)PPISPRegLossIndex::length; i++)
        pf.loss_weights[i] = loss_weights_0[i];
    pf.B = (int32_t)B;
    vkk::dispatch("ppisp_image.ppisp_reg_final_bwd", spec_list(spec), 1, 1, 1,
                  &pf, sizeof(pf));

    // v_raw_losses -> v_ppisp_params (per image).
    PpispRegParams p{};
    p.ppisp_params = std::get<0>(ppisp_params);
    p.raw_losses = (uint64_t)v_raw_losses;
    p.losses = vkk::or_fallback(nullptr);
    p.v_ppisp_params = std::get<0>(v_ppisp_params);
    for (int i = 0; i < (int)PPISPRegLossIndex::length; i++)
        p.loss_weights[i] = loss_weights_0[i];
    p.B = (int32_t)B;
    vkk::dispatch("ppisp_image.ppisp_reg_raw_bwd", spec_list(spec),
                  (uint32_t)((B + 31) / 32), 1, 1, &p, sizeof(p));
}

void ppisp_original_default_init(
    float* params, int N, backend::Stream stream
) {
    (void)stream;
    if (N <= 0) return;
    PpispInitParams p{};
    p.params = (uint64_t)params;
    int64_t total = (int64_t)N * 36;
    p.total = (uint32_t)total;
    vkk::dispatch_flat("bilagrid_tv.ppisp_original_default_init",
                       backend::vk::SpecList{0u}, total, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}

void ppisp_add_into_grad(
    const float* src, float* dst, size_t n, backend::Stream stream
) {
    (void)stream;
    if (n == 0) return;
    AddIntoGradParams p{};
    p.src = (uint64_t)src;
    p.dst = (uint64_t)dst;
    p.total = (uint32_t)n;
    vkk::dispatch_flat("bilagrid_tv.ppisp_add_into_grad",
                       backend::vk::SpecList{0u}, (int64_t)n, 256, &p,
                       sizeof(p), &p.wgs_per_row);
}
