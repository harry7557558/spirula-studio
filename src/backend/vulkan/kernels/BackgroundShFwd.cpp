// Vulkan implementation of the background spherical-harmonics launch API
// (kernels/background/BackgroundSphericalHarmonics.cuh). Mirrors the host dispatcher in
// BackgroundSphericalHarmonics.cu; the device work runs
// shaders/background_sh.slang with the SH degree as a specialization
// constant. The backward is training-phase work and throws for now.

#include <kernels/background/BackgroundSphericalHarmonics.cuh>
#include <core/Common.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Mirrors BackgroundShFwdParams in shaders/background_sh.slang.
struct BackgroundShFwdParams {
    uint64_t viewmats, intrins, dist_coeffs, sh_coeffs, out_img;
    uint32_t width, height, B;
    int32_t camera_model;
};
static_assert(sizeof(BackgroundShFwdParams) == 5 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors BackgroundShBwdParams in shaders/background_sh.slang.
struct BackgroundShBwdParams {
    uint64_t viewmats, intrins, dist_coeffs, sh_coeffs, out_color,
        v_out_color, scratch;
    uint32_t width, height, B;
    int32_t camera_model;
};
static_assert(sizeof(BackgroundShBwdParams) == 7 * 8 + 4 * 4,
              "params layout must match the slang struct");

// Mirrors BackgroundShBwdReduceParams.
struct BackgroundShBwdReduceParams {
    uint64_t scratch, v_sh_coeffs;
    uint32_t k3;
};
static_assert(sizeof(BackgroundShBwdReduceParams) == 2 * 8 + 4 + 4 /*pad*/,
              "params layout must match the slang struct");

int64_t _batch_count(const TorchTensorView& t, int64_t per_item) {
    int64_t n = 1;
    for (auto s : std::get<2>(t)) n *= s;
    return n / per_item;
}

}  // namespace

/* API definitions matching kernels/background/BackgroundSphericalHarmonics.cuh */

void render_background_sh_forward(
    int w,
    int h,
    std::string camera_model,
    std::string distortion,
    int sh_degree,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    TorchTensorView sh_coeffs,
    TorchTensorView out_color
) {
    if (sh_degree < 0 || sh_degree > 4)
        throw std::runtime_error(
            "render_background_sh_forward: sh_degree must be in [0, 4]");

    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion, true);

    int64_t b = _batch_count(out_color, (int64_t)h * w * 3);
    if (b * h * w == 0) return;
    if (_batch_count(viewmats, 16) != b || _batch_count(intrins, 4) != b)
        throw std::runtime_error(
            "viewmats/intrins must have batch dim matching out_color");

    BackgroundShFwdParams p{};
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.sh_coeffs = std::get<0>(sh_coeffs);
    p.out_img = std::get<0>(out_color);
    p.width = (uint32_t)w;
    p.height = (uint32_t)h;
    p.B = (uint32_t)b;
    p.camera_model = (int32_t)cd.cam;

    backend::vk::SpecList spec{(uint32_t)sh_degree, cd.dist};
    vkk::dispatch("background_sh.background_sh_fwd", spec,
                  (uint32_t)((w + 127) / 128), (uint32_t)h, (uint32_t)b, &p,
                  sizeof(p));
}

// Backward: fixed grid of kBgBwdSlices grid-striding threads accumulate
// per-slice float3 partials (no atomics), then a single-workgroup reduce
// folds the slices into v_sh_coeffs (see background_sh.slang).
void render_background_sh_backward(
    int w,
    int h,
    std::string camera_model,
    std::string distortion,
    int sh_degree,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    TorchTensorView sh_coeffs,
    TorchTensorView out_color,
    TorchTensorView v_out_color,
    TorchTensorView v_sh_coeffs
) {
    if (sh_degree < 0 || sh_degree > 4)
        throw std::runtime_error(
            "render_background_sh_backward: sh_degree must be in [0, 4]");
    const vkk::CamDistSpec cd = vkk::cam_dist_spec(camera_model, distortion);
    int64_t b = _batch_count(out_color, (int64_t)h * w * 3);
    if (b * h * w == 0) return;
    if (_batch_count(viewmats, 16) != b || _batch_count(intrins, 4) != b)
        throw std::runtime_error(
            "viewmats/intrins must have batch dim matching out_color");

    constexpr uint32_t kBgBwdWgs = 64;      // matches background_sh.slang
    constexpr uint32_t kBgBwdSlices = kBgBwdWgs * 256;
    const int K = (sh_degree + 1) * (sh_degree + 1);
    const size_t scratch_bytes = (size_t)kBgBwdSlices * K * 3 * sizeof(float);
    void* scratch = DevicePool::global().acquire_dynamic(
        VramCategory::Appearance, "bg_sh.bwd_scratch", scratch_bytes);
    backend::memset_async(scratch, 0, scratch_bytes, backend::kDefaultStream);

    BackgroundShBwdParams p{};
    p.viewmats = std::get<0>(viewmats);
    p.intrins = std::get<0>(intrins);
    p.dist_coeffs = vkk::or_fallback(std::get<0>(dist_coeffs));
    p.sh_coeffs = std::get<0>(sh_coeffs);
    p.out_color = std::get<0>(out_color);
    p.v_out_color = std::get<0>(v_out_color);
    p.scratch = (uint64_t)scratch;
    p.width = (uint32_t)w;
    p.height = (uint32_t)h;
    p.B = (uint32_t)b;
    p.camera_model = (int32_t)cd.cam;
    vkk::dispatch("background_sh.background_sh_bwd",
                  backend::vk::SpecList{(uint32_t)sh_degree, cd.dist},
                  kBgBwdWgs, 1, 1, &p, sizeof(p));

    BackgroundShBwdReduceParams r{};
    r.scratch = (uint64_t)scratch;
    r.v_sh_coeffs = std::get<0>(v_sh_coeffs);
    r.k3 = (uint32_t)(K * 3);
    // One workgroup per output scalar (the kernel grid-strides + block-reduces
    // the kBgBwdSlices partials for its column). Was a single workgroup that
    // serially folded every slice.
    vkk::dispatch("background_sh.background_sh_bwd_reduce",
                  backend::vk::SpecList{(uint32_t)sh_degree}, (uint32_t)(K * 3),
                  1, 1, &r, sizeof(r));
}
