// TrainerCore.cpp -- see TrainerCore.h.

#include "app/TrainerCore.h"
#include "app/EvalMetrics.h"
#include "checkpoint/Adapt.h"
#include "checkpoint/Resume.h"
#include "checkpoint/SplatPly.h"
#include "config/TrainConfigJson.h"
#include "core/ColorSpace.h"
#include "core/ExrImage.h"
#include "i18n/catalog/Log.h"
#include "data/CameraMath.h"
#include "data/Knn.h"
#include "sfm/core/Exif.h"

#ifndef _WIN32
#include <ftw.h>
#endif

#include "external/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <ctime>
#include <deque>
#include <numeric>
#include <random>
#include <stdexcept>
#include <thread>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef SS_BACKEND_VULKAN
#include <cuda_runtime.h>  // check_cuda_runtime() driver/runtime preflight
#endif

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

// Progress lines carrying a path or a count. See i18n/Message.h: whole
// sentences with {0} placeholders, never concatenated fragments.
static std::string lfmt(const spirula::i18n::Msg& m,
                        std::initializer_list<spirula::i18n::Arg> a) {
    return spirula::i18n::format(m, a);
}

namespace spirula {

// Recursive delete. Was nftw rather than std::filesystem::remove_all because
// libtorch interposed its own std::filesystem symbols; torch is gone, so this
// can become remove_all whenever someone wants to.
static void remove_tree(const std::filesystem::path& p) {
#ifndef _WIN32
    nftw(p.string().c_str(),
         [](const char* f, const struct stat*, int, struct FTW*) {
             return ::remove(f);
         }, 16, FTW_DEPTH | FTW_PHYS);
#else
    std::filesystem::remove_all(p);
#endif
}

// ===========================================================================
// Color-space handling
// ===========================================================================

Mat3f gamut_to_rec709(const std::string& name) {
    return colorspace::gamut_to_rec709(name);
}

Mat3f invert3x3(const Mat3f& m) { return colorspace::invert3x3(m); }

ColorResolution resolve_color(const TrainConfig& c) {
    ColorResolution r;
    // "Rec.709" is how the config says "sRGB, and do not take the file's word
    // for it"; resolved it is the identity, same as the unset "".
    r.image_gamut  = c.image_color_gamut == "Rec.709" ? "" : c.image_color_gamut;
    r.image_linear = c.image_color_is_linear.value_or(false);
    std::optional<bool> convert = c.convert_initial_point_cloud_color;

    r.splat_gamut = c.splat_color_gamut;
    if (r.splat_gamut.empty()) r.splat_gamut = r.image_gamut;
    else if (!convert.has_value()) convert = true;
    if (r.splat_gamut == "Rec.709") r.splat_gamut = "";

    if (c.splat_color_is_linear.has_value()) {
        r.splat_linear = *c.splat_color_is_linear;
        if (!convert.has_value()) convert = true;
    } else {
        r.splat_linear = r.image_linear;
    }
    r.convert_seed = convert.value_or(false);
    return r;
}

static WarpFaceFit resolve_face_fit(const TrainConfig& c) {
    if (c.warp_face_fit == "uniform")  return WarpFaceFit::Uniform;
    if (c.warp_face_fit == "per-face") return WarpFaceFit::PerFace;
    throw std::runtime_error("unknown warp_face_fit: " + c.warp_face_fit);
}

// What `depths/` measures when the flag does not say. `spirula geometry`
// writes ray depth exactly when it had to split the frame, so this asks the
// same question of the same lens. Wrong is silent: the loss still trains, on
// a depth field bent by a secant.
bool resolve_ray_depth(const TrainConfig& c, const ParsedDataset& ds) {
    if (c.input_depth_is_ray_depth.has_value()) return *c.input_depth_is_ray_depth;
    int64_t wide = 0, voters = 0;
    for (int64_t i = 0; i < ds.num_cameras; i++) {
        // A frame with no depth map has no opinion on what the depth maps are.
        if (!ds.depth_filenames.empty() && ds.depth_filenames[(size_t)i].empty())
            continue;
        voters++;
        if (camhost::splits_to_pinhole_faces(ds.camera_models[(size_t)i],
                                             ds.widths[(size_t)i],
                                             ds.heights[(size_t)i],
                                             ds.intrins[(size_t)i * 4 + 0],
                                             ds.intrins[(size_t)i * 4 + 1]))
            wide++;
    }
    // A dataset that mixes the two has no right answer; the majority is the
    // one that leaves fewer frames misread.
    return wide * 2 > voters;
}


// ===========================================================================
// LR schedule. std::nullopt for `lr_final` means "constant at `lr`".
// ===========================================================================

float scheduled_lr(int step, int max_steps, float lr,
                   std::optional<float> lr_final,
                   std::optional<int> warmup) {
    float s = lr;
    if (lr_final.has_value() && lr != 0.0f && *lr_final != 0.0f)
        s = lr * std::pow(*lr_final / lr,
                          std::min((float)step / (float)std::max(max_steps, 1), 1.0f));
    if (warmup.has_value())
        s = std::min(s, lr * std::min((float)step / (float)std::max(*warmup, 1), 1.0f));
    return s;
}


// ===========================================================================
// Splat seeding (3dgs branch)
// ===========================================================================

SeedSplats seed_splats(const ColmapPoints3D& pts, const TrainConfig& cfg,
                       const ColorResolution& color) {
    std::mt19937 rng(42);
    std::normal_distribution<float> gauss(0.f, 1.f);
    std::uniform_real_distribution<float> uni(0.f, 1.f);

    float scale_init   = cfg.scale_init.value_or(0.5f);
    float opacity_init = cfg.opacity_init.value_or(0.1f);

    // Resolve seed count into [min_init, cap_max].
    int64_t n_src = pts.num();
    if (n_src == 0) throw std::runtime_error("seed_splats: empty point cloud");
    int64_t min_init = std::max<int64_t>(
        (int64_t)(std::min(cfg.min_init_fraction, 1.0f) * cfg.cap_max), 1);
    min_init = std::min<int64_t>(min_init, cfg.cap_max);

    std::vector<int64_t> pick;
    if (n_src > cfg.cap_max) {
        pick.resize(n_src);
        std::iota(pick.begin(), pick.end(), 0);
        std::shuffle(pick.begin(), pick.end(), rng);
        pick.resize(cfg.cap_max);
    } else {
        // Repeat modulo when under min_init. TODO: jitter the repeats toward
        // a nearest neighbor instead of duplicating exactly.
        int64_t n = std::max(n_src, min_init);
        pick.resize(n);
        for (int64_t i = 0; i < n; i++) pick[i] = i % n_src;
    }
    const int64_t num = (int64_t)pick.size();
    const int64_t cap = cfg.preallocate_splat_tensors
        ? std::max<int64_t>(num, cfg.cap_max) : num;
    const int64_t dim_sh = (int64_t)(cfg.sh_degree + 1) * (cfg.sh_degree + 1);

    SeedSplats s;
    s.num = num;
    s.means.assign(cap * 3, 0.f);
    s.quats.assign(cap * 4, 0.f);
    s.scales.assign(cap * 3, 0.f);
    s.opacities.assign(cap * 1, 0.f);
    s.features_dc.assign(cap * 3, 0.f);
    s.features_sh.assign(cap * (dim_sh - 1) * 3, 0.f);

    // means, scaled into the training frame
    float rescale = cfg.relative_scale.value_or(1.0f);
    for (int64_t i = 0; i < num; i++)
        for (int d = 0; d < 3; d++)
            s.means[i*3 + d] = pts.xyz[pick[i]*3 + d] * rescale;

    // log(scale_init * sqrt(mean d^2 of 4-NN)) over xyz.
    // TODO: suppress_initial_scales.
    std::vector<float> nn = knn::mean_knn_dist(s.means, num, 4);
    for (int64_t i = 0; i < num; i++) {
        float v = std::log(scale_init * nn[i] + 1e-8f);
        s.scales[i*3+0] = s.scales[i*3+1] = s.scales[i*3+2] = v;
    }

    for (int64_t i = 0; i < num; i++) {
        float q[4] = {gauss(rng), gauss(rng), gauss(rng), gauss(rng)};
        float qn = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
        for (int d = 0; d < 4; d++) s.quats[i*4 + d] = q[d] / std::max(qn, 1e-12f);
        s.opacities[i] = std::log(opacity_init / (1.f - opacity_init));
    }

    // Seed colors. Uniform-color clouds are randomized.
    bool all_same = true;
    for (int64_t i = 0; i < num && all_same; i++)
        for (int d = 0; d < 3; d++)
            if (pts.rgb[pick[i]*3 + d] != pts.rgb[pick[0]*3]) { all_same = false; break; }
    // Rec.709 -> the SPLAT gamut: the seeds feed the renderer, which works in
    // the splat space. The image gamut only matches it by default.
    Mat3f to_splat = invert3x3(gamut_to_rec709(color.splat_gamut));
    for (int64_t i = 0; i < num; i++) {
        float col[3];
        for (int d = 0; d < 3; d++)
            col[d] = all_same ? uni(rng) : pts.rgb[pick[i]*3 + d] / 255.f;
        if (color.convert_seed) {
            for (int d = 0; d < 3; d++) col[d] = colorspace::srgb_to_linear(col[d]);
            colorspace::apply3x3(to_splat, col);
            if (!color.splat_linear)
                for (int d = 0; d < 3; d++) col[d] = colorspace::linear_to_srgb(col[d]);
        }
        for (int d = 0; d < 3; d++)
            s.features_dc[i*3 + d] = (col[d] - 0.5f) / 0.28209479177387814f;
    }
    return s;
}

SeedSplats seed_splats_from_ply(const std::string& path, const TrainConfig& cfg,
                                const ColorResolution& color) {
    constexpr float kSHC0 = 0.28209479177387814f;  // band-0 SH, colour <-> DC
    if (!is_splat_ply(path))
        throw std::runtime_error(
            path + ": not a 3D Gaussian Splatting PLY (no opacity / scale / rot "
            "properties). A plain point cloud is a dataset's seed cloud, not an "
            "--init-ply.");
    SplatCloud src = read_splat_ply(path, cfg.sh_degree > 0);
    if (src.num == 0) throw std::runtime_error(path + ": no splats in file");

    std::vector<int64_t> pick((size_t)src.num);
    std::iota(pick.begin(), pick.end(), (int64_t)0);
    if (src.num > cfg.cap_max) {
        // Faintest out first, as checkpoint adaptation does (checkpoint/Adapt.h).
        std::partial_sort(pick.begin(), pick.begin() + cfg.cap_max, pick.end(),
                          [&](int64_t a, int64_t b) {
                              return src.opacities[(size_t)a] >
                                     src.opacities[(size_t)b];
                          });
        pick.resize((size_t)cfg.cap_max);
    }

    const int64_t num = (int64_t)pick.size();
    const int64_t cap = cfg.preallocate_splat_tensors
        ? std::max<int64_t>(num, cfg.cap_max) : num;
    const int64_t K = (int64_t)(cfg.sh_degree + 1) * (cfg.sh_degree + 1) - 1;
    const int64_t K_src = src.dim_sh() - 1;
    const int64_t K_copy = std::min(K, K_src);

    SeedSplats s;
    s.num = num;
    s.means.assign((size_t)cap * 3, 0.f);
    s.quats.assign((size_t)cap * 4, 0.f);
    s.scales.assign((size_t)cap * 3, 0.f);
    s.opacities.assign((size_t)cap, 0.f);
    s.features_dc.assign((size_t)cap * 3, 0.f);
    s.features_sh.assign((size_t)cap * K * 3, 0.f);

    const float rescale = cfg.relative_scale.value_or(1.0f);
    const float log_rescale = std::log(std::max(rescale, 1e-12f));
    Mat3f to_splat = invert3x3(gamut_to_rec709(color.splat_gamut));

    for (int64_t i = 0; i < num; i++) {
        const int64_t j = pick[(size_t)i];
        for (int d = 0; d < 3; d++) {
            s.means[i*3 + d]  = src.means[j*3 + d] * rescale;
            s.scales[i*3 + d] = src.scales[j*3 + d] + log_rescale;
        }
        float qn = 0.f;
        for (int d = 0; d < 4; d++) qn += src.quats[j*4 + d] * src.quats[j*4 + d];
        qn = std::sqrt(qn);
        for (int d = 0; d < 4; d++)
            s.quats[i*4 + d] = src.quats[j*4 + d] / std::max(qn, 1e-12f);
        s.opacities[i] = src.opacities[j];

        if (color.convert_seed) {
            float col[3];
            for (int d = 0; d < 3; d++)
                col[d] = src.features_dc[j*3 + d] * kSHC0 + 0.5f;
            for (int d = 0; d < 3; d++) col[d] = colorspace::srgb_to_linear(col[d]);
            colorspace::apply3x3(to_splat, col);
            if (!color.splat_linear)
                for (int d = 0; d < 3; d++) col[d] = colorspace::linear_to_srgb(col[d]);
            for (int d = 0; d < 3; d++)
                s.features_dc[i*3 + d] = (col[d] - 0.5f) / kSHC0;
        } else {
            for (int d = 0; d < 3; d++)
                s.features_dc[i*3 + d] = src.features_dc[j*3 + d];
        }

        // A gamut change is linear and the DC carries it; the view-dependent
        // terms ride an encoded curve, so they come across as they are and the
        // first refinements re-fit them.
        for (int64_t k = 0; k < K_copy; k++)
            for (int d = 0; d < 3; d++)
                s.features_sh[(i*K + k)*3 + d] =
                    src.features_sh[(j*K_src + k)*3 + d];
    }
    return s;
}

void append_point_seeds(SeedSplats& s, const ColmapPoints3D& pts,
                        const TrainConfig& cfg, const ColorResolution& color) {
    const int64_t room = (int64_t)cfg.cap_max - s.num;
    if (room <= 0 || pts.num() == 0) return;

    // seed_splats() budgets against cap_max, so the room left over IS the cap
    // for this half; compact, because it is copied into `s` row by row.
    TrainConfig sub = cfg;
    sub.cap_max = (int)room;
    sub.preallocate_splat_tensors = false;
    SeedSplats p = seed_splats(pts, sub, color);

    const int64_t K = (int64_t)(cfg.sh_degree + 1) * (cfg.sh_degree + 1) - 1;
    const int64_t at = s.num, total = s.num + p.num;
    if ((int64_t)s.opacities.size() < total) {
        s.means.resize((size_t)total * 3, 0.f);
        s.quats.resize((size_t)total * 4, 0.f);
        s.scales.resize((size_t)total * 3, 0.f);
        s.opacities.resize((size_t)total, 0.f);
        s.features_dc.resize((size_t)total * 3, 0.f);
        s.features_sh.resize((size_t)total * K * 3, 0.f);
    }
    for (int64_t i = 0; i < p.num; i++) {
        for (int d = 0; d < 3; d++) {
            s.means[(at + i)*3 + d]        = p.means[i*3 + d];
            s.scales[(at + i)*3 + d]       = p.scales[i*3 + d];
            s.features_dc[(at + i)*3 + d]  = p.features_dc[i*3 + d];
        }
        for (int d = 0; d < 4; d++)
            s.quats[(at + i)*4 + d] = p.quats[i*4 + d];
        s.opacities[at + i] = p.opacities[i];
        for (int64_t k = 0; k < K; k++)
            for (int d = 0; d < 3; d++)
                s.features_sh[((at + i)*K + k)*3 + d] = p.features_sh[(i*K + k)*3 + d];
    }
    s.num = total;
}


// ===========================================================================
// Per-step EngineStepConfig
// ===========================================================================

namespace {

int densify_loss_map_mode_int(const std::string& mode) {
    if (mode == "none")              return 0;
    if (mode == "loss_full")         return 1;
    if (mode == "ssim_full")         return 2;
    if (mode == "ssim_cs")           return 3;
    if (mode == "ssim_structure")    return 4;
    if (mode == "edge_aware")        return 5;
    if (mode == "robust_edge_aware") return 6;
    if (mode == "loss_full_nms")       return 7;
    if (mode == "ssim_full_nms")       return 8;
    if (mode == "ssim_cs_nms")         return 9;
    if (mode == "ssim_structure_nms")  return 10;
    throw std::runtime_error("unknown densify_loss_map_mode: " + mode);
}

int densify_accum_mode_int(const std::string& mode) {
    if (mode == "max") return (int)DensifyAccumMode::Max;
    if (mode == "sum") return (int)DensifyAccumMode::Sum;
    if (mode == "avg") return (int)DensifyAccumMode::Avg;
    throw std::runtime_error("unknown densify_accum_mode: " + mode);
}

}  // namespace

std::array<float, (int)LossWeightIndex::length>
build_loss_weights(const TrainConfig& c, int step) {
    float dist_factor = std::min((float)step / std::max(c.distortion_reg_warmup, 1), 1.0f);
    float reg_active  = step >= c.reg_warmup_length ? 1.0f : 0.0f;
    float sup_active  = step > c.supervision_warmup ? 1.0f : 0.0f;
    float median_factor = std::min((float)step / std::max(c.median_warmup, 1), 1.0f);
    float alpha_reg_factor = c.alpha_reg_weight *
        std::min((float)step / std::max(c.alpha_reg_warmup, 1), 1.0f);
    float mask = c.apply_loss_for_mask ? 1.0f : 0.0f;

    float w_rgb_l1 = std::max(0.0f, c.l1_weight);
    float w_rgb_l2 = std::max(0.0f, c.l2_weight);
    float w_y_l1   = std::max(0.0f, c.l1_weight_y);
    float w_y_l2   = std::max(0.0f, c.l2_weight_y);
    float w_u_l2   = std::max(0.0f, c.l2_weight_u);
    float w_v_l2   = std::max(0.0f, c.l2_weight_v);
    float total = w_rgb_l1 + w_rgb_l2 + w_y_l1 + w_y_l2 + w_u_l2 + w_v_l2;
    float scale = total > 0.0f ? (1.0f - c.ssim_lambda) / total : 0.0f;

    std::array<float, (int)LossWeightIndex::length> w{};
    w[(int)LossWeightIndex::RgbSupL1]      = w_rgb_l1 * scale;
    w[(int)LossWeightIndex::RgbSupL2]      = w_rgb_l2 * scale;
    w[(int)LossWeightIndex::YSupL1]        = w_y_l1 * scale;
    w[(int)LossWeightIndex::YSupL2]        = w_y_l2 * scale;
    w[(int)LossWeightIndex::USupL2]        = w_u_l2 * scale;
    w[(int)LossWeightIndex::VSupL2]        = w_v_l2 * scale;
    w[(int)LossWeightIndex::DepthSup]      = sup_active * c.depth_supervision_weight;
    w[(int)LossWeightIndex::NormalSup]     = sup_active * c.normal_supervision_weight;
    w[(int)LossWeightIndex::AlphaSup]      = mask * c.alpha_loss_weight;
    w[(int)LossWeightIndex::AlphaSupUnder] = mask * c.alpha_loss_weight_under;
    w[(int)LossWeightIndex::NormalReg]     = reg_active * c.normal_reg_weight * dist_factor;
    w[(int)LossWeightIndex::AlphaReg]      = reg_active * alpha_reg_factor;
    w[(int)LossWeightIndex::RgbDistReg]    = reg_active * c.rgb_distortion_reg * dist_factor;
    w[(int)LossWeightIndex::DepthDistReg]  = reg_active * c.depth_distortion_reg * dist_factor;
    w[(int)LossWeightIndex::NormalDistReg] = reg_active * c.normal_distortion_reg * dist_factor;
    w[(int)LossWeightIndex::MeanMedianDepthSup]    = median_factor * c.mean_median_depth_weight;
    w[(int)LossWeightIndex::MedianDepthNormalReg]  = median_factor * c.median_depth_normal_reg_weight;
    w[(int)LossWeightIndex::MedianNormalSup]       = median_factor * c.median_normal_supervision_weight;
    w[(int)LossWeightIndex::MedianRenderNormalReg] = median_factor * c.median_render_normal_reg_weight;
    return w;
}

EngineStepConfig build_step_config(const TrainConfig& c, const RunState& st, int step) {
    int max_steps_lr = c.max_steps.value_or(c.num_iterations);
    float alpha = st.train_frame_scale;
    EngineStepConfig cfg;

    // ---- loss ----------------------------------------------------------
    cfg.loss.weights = build_loss_weights(c, step);
    cfg.loss.w_ssim = c.ssim_lambda;
    cfg.loss.num_loss_scales = c.num_loss_scales + 1;
    cfg.loss.loss_scale_min_pixels = c.loss_scale_min_pixels;
    int loss_map_mode = densify_loss_map_mode_int(c.densify_loss_map_mode);
    // blend >= 1: world-grad score only, loss map has no consumer.
    if (c.densify_score_blend_world_grad >= 1.0f) loss_map_mode = 0;
    cfg.loss.loss_map_mode = loss_map_mode;
    cfg.loss.compute_loss_map = (loss_map_mode != 0);
    cfg.loss.robust_edge_aware_quantile = c.densify_robust_edge_aware_quantile;
    cfg.loss.nms_falloff = c.densify_nms_falloff;
    cfg.loss.loss_map_normalize = c.densify_loss_map_normalize;
    cfg.loss.loss_map_clip_quantile = c.densify_loss_map_clip_quantile;
    cfg.loss.loss_map_power = c.densify_loss_map_power;
    cfg.loss.loss_map_accum_mode = densify_accum_mode_int(c.densify_accum_mode);
    cfg.loss.overexposure_reg_weight = c.overexposure_reg;
    if (st.bilagrid_rgb_init || st.ppisp_init) {
        cfg.loss.color_shift_reg_weight = c.color_shift_reg_weight;
        cfg.loss.color_shift_reg_beta =
            std::max(0.0f, 1.0f - 1.0f / std::max(c.color_shift_reg_ema_period, 1));
    }
    cfg.loss.input_depth_is_ray_depth = st.input_depth_is_ray_depth;

    // ---- optim ---------------------------------------------------------
    float means_lr = scheduled_lr(step, max_steps_lr, c.means_lr, c.means_lr_final);
    if (!c.use_scale_agnostic_mean) means_lr *= alpha;
    cfg.optim.lr_means       = means_lr;
    cfg.optim.lr_quats       = scheduled_lr(step, max_steps_lr, c.quats_lr);
    cfg.optim.lr_scales      = scheduled_lr(step, max_steps_lr, c.scales_lr, c.scales_lr_final);
    cfg.optim.lr_opacities   = scheduled_lr(step, max_steps_lr, c.opacities_lr);
    cfg.optim.lr_features_dc = scheduled_lr(step, max_steps_lr, c.features_dc_lr);
    cfg.optim.lr_features_sh = scheduled_lr(step, max_steps_lr, c.features_sh_lr);
    cfg.optim.max_gauss_ratio             = c.max_gauss_ratio;
    cfg.optim.scale_regularization_weight = c.scale_regularization_weight;
    cfg.optim.mcmc_opacity_reg_weight     = c.opacity_reg;
    cfg.optim.mcmc_scale_reg_weight       = c.scale_reg / alpha;
    cfg.optim.erank_reg_weight            = c.erank_reg;
    cfg.optim.erank_reg_weight_s3         = c.erank_reg_s3;
    cfg.optim.quat_norm_reg_weight        = c.quat_norm_reg;
    cfg.optim.sh_reg_weight               = c.sh_reg;
    cfg.optim.use_scale_agnostic_mean     = c.use_scale_agnostic_mean;
    // quantization level -> bit depths
    cfg.optim.quantization_level = c.quantization_level;
    cfg.optim.sh_optim_bits      = c.quantization_level == 0 ? 32 : 8;
    cfg.optim.sh_value_bits      = c.quantization_level == 0 ? 32 : 16;
    cfg.optim.non_sh_optim_bits  = c.quantization_level == 0 ? 32 : 16;
    cfg.optim.use_per_splat_bias_correction = c.use_per_splat_bias_correction;
    cfg.optim.use_fused_proj_bwd_optim      = c.use_fused_proj_bwd_optim;
    cfg.optim.write_densify_world_grad_score =
        c.densify_score_blend_world_grad > 0.0f && c.use_revised_densification;
    cfg.optim.split_batch     = c.split_batch;
    cfg.optim.color_is_linear = st.splat_linear;
    cfg.optim.use_color_trust_region = st.splat_linear;
    cfg.optim.eps_tr = 1e-6f * std::pow(0.01f, (float)step / std::max(max_steps_lr, 1));

    // ---- densify -------------------------------------------------------
    float noise_lr_scalar = c.use_revised_densification ? 1.0f : alpha;
    cfg.densify.refine_start_iter             = c.refine_start_iter;
    cfg.densify.refine_stop_num_iter          = c.refine_stop_num_iter;
    cfg.densify.refine_stop_iter              = c.refine_stop_iter;
    cfg.densify.refine_every                  = c.refine_every;
    cfg.densify.growth_factor                 = c.growth_factor;
    cfg.densify.min_opacity                   = c.min_opacity;
    cfg.densify.max_screen_size               = c.max_screen_size;
    cfg.densify.max_screen_size_clip_hardness = c.max_screen_size_clip_hardness;
    cfg.densify.max_world_size                = c.max_world_size * alpha;
    cfg.densify.noise_lr                      = c.noise_lr * noise_lr_scalar;
    cfg.densify.noise_lr_final                = c.noise_lr_final * noise_lr_scalar;
    cfg.densify.use_revised_densification     = c.use_revised_densification;
    cfg.densify.score_mode = c.densify_score_mode == "mean" ? 0
        : c.densify_score_mode == "max" ? 1
        : c.densify_score_mode == "median" ? 2 : 3;
    cfg.densify.score_blend_world_grad = c.densify_score_blend_world_grad;
    cfg.densify.score_power = c.densify_score_power;
    cfg.densify.score_clip_quantile = c.densify_score_clip_quantile;
    cfg.densify.final_score_power = c.densify_final_score_power;
    cfg.densify.las_split_opacity_k_init   = c.long_axis_split_opacity_k[0];
    cfg.densify.las_split_opacity_k_final  = c.long_axis_split_opacity_k[1];
    cfg.densify.las_split_opacity_k_warmup = (int)c.long_axis_split_opacity_k[2];

    // ---- bilagrid LRs + TV ---------------------------------------------
    if (st.bilagrid_rgb_init) {
        cfg.bilagrid.lr_rgb = c.use_adagrad_bilagrid_optim
            ? c.bilagrid_adagrad_lr
            : scheduled_lr(step, max_steps_lr, c.bilagrid_lr, c.bilagrid_lr_final,
                           c.bilagrid_lr_warmup);
        cfg.bilagrid.tv_weight_rgb = c.bilagrid_tv_loss_weight;
    }
    if (st.bilagrid_depth_init) {
        cfg.bilagrid.lr_depth = c.use_adagrad_bilagrid_optim
            ? c.bilagrid_adagrad_depth_lr
            : scheduled_lr(step, max_steps_lr, c.bilagrid_depth_lr,
                           c.bilagrid_depth_lr_final, c.bilagrid_depth_lr_warmup);
        cfg.bilagrid.tv_weight_depth = c.bilagrid_tv_loss_weight_geometry;
    }
    if (st.bilagrid_normal_init) {
        cfg.bilagrid.lr_normal = c.use_adagrad_bilagrid_optim
            ? c.bilagrid_adagrad_normal_lr
            : scheduled_lr(step, max_steps_lr, c.bilagrid_normal_lr,
                           c.bilagrid_normal_lr_final, c.bilagrid_normal_lr_warmup);
        cfg.bilagrid.tv_weight_normal = c.bilagrid_tv_loss_weight_geometry;
    }

    // ---- PPISP ---------------------------------------------------------
    if (st.ppisp_init) {
        cfg.ppisp.lr = c.use_adagrad_ppisp_optim
            ? c.ppisp_adagrad_lr
            : scheduled_lr(step, max_steps_lr, c.ppisp_lr, c.ppisp_lr_final,
                           c.ppisp_lr_warmup);
        // PPISPRegLossIndex order
        cfg.ppisp.reg_weights = {
            c.ppisp_reg_exposure_mean, c.ppisp_reg_vig_center,
            c.ppisp_reg_vig_non_pos,   c.ppisp_reg_vig_channel_var,
            c.ppisp_reg_color_mean,    c.ppisp_reg_crf_channel_var,
        };
    }
    // Outside the guard: it is an ordering flag, not a rate, so it reflects
    // the config whether or not PPISP is live.
    cfg.ppisp.run_before_bilagrid = c.apply_ppisp_before_bilagrid;

    // ---- background ----------------------------------------------------
    if (c.background_mode == "noise") {
        float rw = std::min((float)step / std::max(c.background_noise_warmup, 1), 1.0f);
        cfg.background.randomize_weight =
            1.0f - (1.0f - c.background_noise_pre_warmup) * (1.0f - rw);
    } else if (c.background_mode == "sh") {
        cfg.background.lr_dc = scheduled_lr(step, max_steps_lr, c.background_dc_lr);
        cfg.background.lr_sh = scheduled_lr(step, max_steps_lr, c.background_sh_lr);
    }
    cfg.background.seed = (uint32_t)(step & 0x7FFFFFFF);

    return cfg;
}


// ===========================================================================
// config.json dump
// ===========================================================================

// Flat dump: one key per flag, spelled exactly as the flag is. It used to be
// nested under the field table's group column, which quietly made a
// presentational choice part of an on-disk format -- moving a flag to another
// heading moved its key, and a reader that could not find it silently fell
// back to the default. Flat, a heading can be renamed or reshuffled without
// touching anything that reads this file (`spirula mesh`, --resume).
//
// Macro flags (--quality and friends) are written alongside the values they
// resolved to, so a reader takes the values and never re-resolves them.
//
// The encoding itself is config/TrainConfigJson.h, shared with --resume's
// reader and the GUI's saved presets so the three cannot disagree.
void save_config_json(const TrainConfig& c, const fs::path& out_dir,
                      const std::string& preset) {
    FILE* f = std::fopen((out_dir / "config.json").string().c_str(), "w");
    if (!f) throw std::runtime_error("cannot write config.json");
    std::fprintf(f, "{\n    \"preset\": \"%s\"", preset.c_str());
    for (const auto& [key, value] : train_config_json_pairs(c))
        std::fprintf(f, ",\n    \"%s\": %s", key, value.c_str());
    std::fprintf(f, "\n}\n");
    std::fclose(f);
}


// ===========================================================================
// TrainerSession
// ===========================================================================

void TrainerSession::log(const std::string& msg) {
    if (log_fn) { log_fn(msg); return; }
    std::printf("%s\n", msg.c_str());
    std::fflush(stdout);
}

// The unported-feature guards, as a pure check. Split out of check_config()
// so a front-end can ask the question without the answer arriving as an
// exception: the GUI's batch pre-flight reports every row's problems at once,
// before anything starts, which is the whole point of a pre-flight.
std::string train_config_unsupported(const TrainConfig& c) {
    // The flag name is an IDENTIFIER and goes in as {0}: `--use-bvh` reads the
    // same in every language, and it is what the reader would type or search
    // for. Only the sentence around it is translated.
    auto not_impl = [](const std::string& what) {
        return lfmt(lmsg::not_supported_yet, {what});
    };
    if (c.use_bvh)                    return not_impl("--use-bvh");
    if (c.use_camera_optimizer)       return not_impl("--use-camera-optimizer");
    if (c.deblur_training_images)     return not_impl("--deblur-training-images");
    if (!c.optimizer_offload.empty()) return not_impl("--optimizer-offload");
    if (c.cache_images == "gpu")      return not_impl("--cache-images gpu");
    if (c.rescale_camera_to_fit < 0)  return not_impl("--rescale-camera-to-fit auto-detect");
    if (c.train_frame != "points")    return not_impl("--train-frame " + c.train_frame);
    if (c.primitive != "3dgs" && c.primitive != "mip" && c.primitive != "3dgut")
        return not_impl("--primitive " + c.primitive);
    if (c.quantization_level != 0 && c.quantization_level != 1)
        return lmsg::bad_quantization_level.get();
    return {};
}

std::string format_duration(double seconds) {
    if (seconds < 0) return "--:--";
    int t = (int)(seconds + 0.5);
    char buf[32];
    if (t >= 3600)
        std::snprintf(buf, sizeof buf, "%d:%02d:%02d", t / 3600, (t / 60) % 60,
                      t % 60);
    else
        std::snprintf(buf, sizeof buf, "%d:%02d", t / 60, t % 60);
    return buf;
}

// Unported-feature guards: fail early rather than ignore a flag.
void TrainerSession::check_config() {
    if (std::string what = train_config_unsupported(cfg); !what.empty())
        throw std::runtime_error(what);
    if (!cfg.init_ply.empty() && !cfg.resume.empty())
        log(lmsg::warn_init_ply_ignored.get());
    else if (!cfg.init_ply.empty())
        find_splat_ply(cfg.init_ply);  // before the dataset is parsed, not after
    if (cfg.validation_fraction > 0)
        log(lmsg::warn_validation_unported.get());
    if (cfg.orientation_method != "up" || cfg.center_method != "poses")
        log(lfmt(lmsg::warn_pose_normalization_approx,
                 {cfg.orientation_method, cfg.center_method}));
}

void TrainerSession::load_dataset() {
    DatasetParserConfig pcfg;
    pcfg.recon_dir            = cfg.colmap_recon_dir;
    pcfg.image_dir            = cfg.image_dir;
    pcfg.mask_dir             = cfg.mask_dir;
    pcfg.depth_dir            = cfg.depth_dir;
    pcfg.normal_dir           = cfg.normal_dir;
    pcfg.validation_fraction  = cfg.validation_fraction;
    pcfg.eval_mode            = cfg.eval_mode;
    pcfg.eval_interval        = cfg.eval_interval;
    pcfg.train_split_fraction = cfg.train_split_fraction;
    pcfg.outlier_threshold    = cfg.outlier_threshold;
    pcfg.rescale_camera_to_fit   = cfg.rescale_camera_to_fit;
    pcfg.downscale_rounding_mode = cfg.downscale_rounding_mode;
    pcfg.metashape_xml           = cfg.metashape_xml;
    pcfg.metashape_ply           = cfg.metashape_ply;
    pcfg.metashape_psx           = cfg.metashape_psx;
    ds = parse_dataset(cfg.data, pcfg, cfg.data_format);

    // An EXR carries its own colour space, and nothing downstream can recover
    // it: DataManager hands the engine the file's raw scene-linear floats. The
    // two halves are adopted independently, so declaring one keeps the other.
    exr::Info exr_info;
    if (!ds.image_filenames.empty() &&
        exr::declared_color_space(ds.image_filenames.front(), exr_info)) {
        const bool take_gamut = cfg.image_color_gamut.empty();
        const bool take_linear = !cfg.image_color_is_linear.has_value();
        if (take_gamut) cfg.image_color_gamut = exr_info.gamut;
        if (take_linear) cfg.image_color_is_linear = exr_info.is_linear;
        const std::string name =
            cfg.image_color_gamut.empty() ? "Rec.709" : cfg.image_color_gamut;
        if (take_linear)     log(lfmt(lmsg::exr_color_space, {name}));
        else if (take_gamut) log(lfmt(lmsg::exr_gamut_from_file, {name}));
        if (take_gamut && !exr_info.gamut_known) log(lmsg::exr_gamut_unknown.get());
    }

    // relative_scale scales the world: point means here, and the c2w
    // translations pre-bake so the baked viewmats follow.
    // auto_scale_poses=false forces the normalized-frame scale to 1.
    if (cfg.relative_scale.has_value()) {
        float rs = *cfg.relative_scale;
        for (auto& v : ds.points.xyz) v *= rs;
        for (int64_t i = 0; i < ds.num_cameras; i++)
            for (int r = 0; r < 3; r++)
                ds.c2w[i*12 + r*4 + 3] *= rs;
    }
    if (!cfg.auto_scale_poses) ds.train_frame_scale = 1.0f;

    // POST-split camera bake (identity when no warp flag applies).
    post = bake_post_split(
        ds, cfg.warp_to_pinhole, cfg.warp_spherical_to_pinhole,
        resolve_face_fit(cfg));

    // Warp-path guards, plus: a modality no weight reads is not loaded at all.
    has_mask   = !ds.mask_filenames.empty()   && cfg.load_masks;
    has_depth  = !ds.depth_filenames.empty()  && cfg.load_depths &&
                 cfg.depth_supervision_weight > 0.0f;
    has_normal = !ds.normal_filenames.empty() && cfg.load_normals &&
                 (cfg.normal_supervision_weight > 0.0f ||
                  cfg.median_normal_supervision_weight > 0.0f);
    if (post.direct_equirect && (has_depth || has_normal))
        throw std::runtime_error(
            "Direct equirectangular training (warp_spherical_to_pinhole=0) "
            "does not support depth/normal supervision yet.");

    char scale[32];
    std::snprintf(scale, sizeof scale, "%.4g", ds.train_frame_scale);
    log(lfmt(lmsg::parsed_dataset,
             {(long long)ds.num_cameras, (long long)post.n_post,
              (long long)ds.points.num(), scale}));
}

// Pre-flight GPU check. A binary compiled by a newer CUDA toolkit than the
// installed driver supports links and loads fine, but every kernel launch then
// fails -- and cudaGetErrorString can't name the resulting error, so it shows
// up as the cryptic "CUDA Error ...: (null)". Detect the mismatch up front and
// report it with the numbers and the fix, instead of dying deep in a kernel.
// CUDA-backend-specific diagnostics by design; the Vulkan backend replaces
// this with instance/physical-device enumeration at the same call site.
#ifndef SS_BACKEND_VULKAN
static void check_cuda_runtime() {
    auto fmt = [](int v) {
        return std::to_string(v / 1000) + "." + std::to_string((v % 1000) / 10);
    };
    // cudaRuntimeGetVersion is answered by the statically-linked runtime and
    // does not touch the driver -- it tells us which CUDA toolkit built this
    // binary even when the driver is unusable.
    int runtime_ver = 0;
    cudaRuntimeGetVersion(&runtime_ver);
    std::string built_with = runtime_ver ? " (this binary was built with CUDA "
                                           + fmt(runtime_ver) + ")" : "";

    // The first driver-backed call triggers lazy CUDA init; if the driver is
    // too old for the runtime it fails here with cudaErrorInsufficientDriver
    // instead of much later inside a kernel launch (where cudaGetErrorString
    // returns null -> the cryptic "CUDA Error ...: (null)").
    int dev_count = 0;
    cudaError_t cerr = cudaGetDeviceCount(&dev_count);
    if (cerr == cudaErrorInsufficientDriver) {
        int driver_ver = 0;
        cudaDriverGetVersion(&driver_ver);  // 0 if the driver is far too old
        std::string drv = driver_ver
            ? "The installed NVIDIA driver supports only up to CUDA "
              + fmt(driver_ver) + "."
            : "The installed NVIDIA driver is too old.";
        throw std::runtime_error(
            "NVIDIA driver too old for this build. " + drv + built_with +
            " Update the GPU driver, or rebuild against an older CUDA toolkit that matches the driver.");
    }
    if (cerr != cudaSuccess) {
        const char* s = cudaGetErrorString(cerr);
        throw std::runtime_error(
            std::string("CUDA initialization failed: ") +
            (s ? s : "unknown error") + built_with +
            ". Check the NVIDIA driver / GPU installation.");
    }
    if (dev_count == 0)
        throw std::runtime_error("No CUDA-capable GPU detected.");
}
#endif  // SS_BACKEND_VULKAN

// PPISP exposure seeds: mean-relative EXIF EV x 0.5 per POST-split slot; empty
// when no image has the tags. The 0.5: PPISP multiplies the sRGB-encoded
// render, where a bracketed +1 EV measures x2^0.49 (0.34-0.76 by tone curve).
static std::vector<float> exif_exposure_evs(const ParsedDataset& ds,
                                            const PostSplitCameras& post,
                                            int& n_found) {
    int64_t n = ds.num_cameras;
    std::vector<double> ev(n, 0.0);
    std::vector<char> has(n, 0);
    double sum = 0.0;
    n_found = 0;
    for (int64_t i = 0; i < n; i++) {
        double v;
        if (sfm::exifExposureEv(sfm::readExif(ds.image_filenames[i]), v)) {
            ev[i] = v;
            has[i] = 1;
            sum += v;
            // sum += std::exp2(v);
            n_found++;
        }
    }
    if (n_found == 0) return {};
    double mean = sum / n_found;
    // double mean = std::log2(sum / n_found);
    std::vector<float> out((size_t)post.n_post, 0.0f);
    for (int64_t i = 0; i < n; i++) {
        if (!has[i]) continue;
        float v = 0.5f * (float)(ev[i] - mean);
        if (post.K_per_camera.empty()) {
            out[i] = v;
        } else {
            for (int k = 0; k < post.K_per_camera[i]; k++)
                out[post.post_offsets[i] + k] = v;
        }
    }
    return out;
}

void TrainerSession::setup_engine() {
#ifndef SS_BACKEND_VULKAN
    check_cuda_runtime();
#endif

    // ---- Output dir ----------------------------------------------------
    if (!out_dir_override.empty()) {
        out_dir = fs::path(out_dir_override);
    } else if (!cfg.output_dir_name.empty()) {
        out_dir = fs::path(cfg.output_dir_prefix) / cfg.output_dir_name;
    } else {
        std::time_t t = std::time(nullptr);
        char stamp[32];
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
        out_dir = fs::path(cfg.output_dir_prefix) /
                  (fs::path(cfg.data).stem().string() + "_" + stamp);
    }
    fs::create_directories(out_dir);
    if (write_config_json)
        save_config_json(cfg, out_dir, preset);
    log(lfmt(lmsg::output_directory, {fs::absolute(out_dir).string()}));

    // ---- Engine setup -------------------------------------------------
    engine_reset();

    ColorResolution color = resolve_color(cfg);
    SeedSplats seed = seed_world(color);
    int64_t cap = (int64_t)seed.opacities.size();
    int64_t dim_sh = (int64_t)(cfg.sh_degree + 1) * (cfg.sh_degree + 1);
    auto tv = [](std::vector<float>& v, std::vector<int64_t> shape) -> TorchTensorView {
        return {(uint64_t)(uintptr_t)v.data(), (uint32_t)sizeof(float), std::move(shape)};
    };
    set_data_3dgs(seed.num,
                  tv(seed.means,       {cap, 3}),
                  tv(seed.quats,       {cap, 4}),
                  tv(seed.scales,      {cap, 3}),
                  tv(seed.opacities,   {cap, 1}),
                  tv(seed.features_dc, {cap, 3}),
                  tv(seed.features_sh, {cap, dim_sh - 1, 3}));

    // Background blending.
    if (cfg.background_mode == "noise")
        engine_init_background_noise(color.splat_linear);
    else if (cfg.background_mode == "sh")
        engine_init_background_sh(cfg.background_sh_degree, color.splat_linear);

    // Linear / wide-gamut color space.
    bool splat_cs_on = color.splat_linear || !color.splat_gamut.empty();
    bool image_cs_on = color.image_linear || !color.image_gamut.empty();
    {
        auto vec = [](const Mat3f& m) { return std::vector<float>(m.begin(), m.end()); };
        engine_init_color_space(
            splat_cs_on, color.splat_linear,
            splat_cs_on ? vec(gamut_to_rec709(color.splat_gamut)) : std::vector<float>{},
            image_cs_on, color.image_linear,
            image_cs_on ? vec(gamut_to_rec709(color.image_gamut)) : std::vector<float>{});
    }

    // ---- DataManager ---------------------------------------------------
    const int64_t N = ds.num_cameras;
    int64_t num_val = (int64_t)ds.val_indices.size();
    int64_t num_train = N - num_val;
    // Batch-size policy.
    double n_batch = std::max((double)num_train / std::max(cfg.max_batch_per_epoch, 1), 1.0);
    int train_bs = std::max(1, (int)(n_batch + 0.5));
    int val_bs = 1;
    if (num_val > 0)
        val_bs = std::max(1, (int)std::ceil(n_batch * (double)num_val / (double)num_train));

    DataManagerConfig dm;
    dm.cache_mode  = (cfg.cache_images == "disk") ? CacheMode::DISK : CacheMode::CPU;
    // A split needs a mask even when none is on disk: the synthesized
    // all-white one becomes the post-split FOV mask (0 past the lens),
    // without which the unseen face regions train as black.
    dm.load_masks  = has_mask || post.any_fov_mask;
    dm.load_depths      = has_depth;
    dm.load_normals     = has_normal;
    dm.train_batch_size = train_bs;
    dm.val_batch_size   = val_bs;
    dm.mask_boundary_offset = cfg.mask_boundary_offset;
    engine_setup_data_manager(
        dm, ds.camera_models, ds.camera_distortions,
        ds.image_filenames,
        has_mask ? ds.mask_filenames : std::vector<std::string>{},
        has_depth ? ds.depth_filenames : std::vector<std::string>{},
        has_normal ? ds.normal_filenames : std::vector<std::string>{},
        ds.widths, ds.heights,
        post.any_warp ? post.K_per_camera : std::vector<int32_t>{},
        post.any_warp ? post.post_offsets : std::vector<int32_t>{},
        post.viewmats, post.intrins, post.dist_coeffs,
        post.any_warp ? post.post_widths : std::vector<int32_t>{},
        post.any_warp ? post.post_heights : std::vector<int32_t>{},
        post.any_warp ? post.face_axes : std::vector<float>{},
        post.input_intrins, post.input_dist_coeffs,
        post.redistort_models, post.redistort_params,
        ds.train_indices, ds.val_indices);

    // ---- Bilagrid / PPISP init -----------------------------------------
    // Enablement conditions are static here (dataset modalities known up
    // front), so the init happens once at setup rather than per step.
    st = RunState{};
    st.train_frame_scale = ds.train_frame_scale;
    st.splat_linear      = color.splat_linear;
    st.input_depth_is_ray_depth = resolve_ray_depth(cfg, ds);
    if (has_depth && !cfg.input_depth_is_ray_depth.has_value())
        log(lfmt(lmsg::ray_depth_resolved,
                 {(st.input_depth_is_ray_depth ? lmsg::ray_depth_along_ray
                                               : lmsg::ray_depth_straight_ahead)
                      .get()}));
    int optim_bits = cfg.quantization_level == 0 ? 32 : 8;
    int value_bits = cfg.quantization_level == 0 ? 32 : 16;
    // num_train_data resolves to the POST-split camera count -- the
    // bilagrid / PPISP tables have one slot per post camera and the
    // TV-loss normalization depends on it.
    int n_grids = (int)post.n_post;

    if (cfg.use_bilateral_grid &&
        (cfg.use_adagrad_bilagrid_optim ? cfg.bilagrid_adagrad_lr
                                        : cfg.bilagrid_lr) > 0.0f) {
        // bilagrid_shape is (X, Y, W) -> engine (L=W, H=Y, W=X).
        engine_init_bilagrid_rgb(n_grids, cfg.bilagrid_type,
                                 cfg.bilagrid_shape[2], cfg.bilagrid_shape[1],
                                 cfg.bilagrid_shape[0],
                                 optim_bits, value_bits,
                                 cfg.use_adagrad_bilagrid_optim);
        st.bilagrid_rgb_init = true;
    }
    if (cfg.use_bilateral_grid_for_geometry && has_depth &&
        cfg.depth_supervision_weight > 0.0f &&
        (cfg.use_adagrad_bilagrid_optim ? cfg.bilagrid_adagrad_depth_lr
                                        : cfg.bilagrid_depth_lr) > 0.0f) {
        engine_init_bilagrid_depth(n_grids,
                                   cfg.bilagrid_shape_geometry[2],
                                   cfg.bilagrid_shape_geometry[1],
                                   cfg.bilagrid_shape_geometry[0],
                                   optim_bits, value_bits,
                                   cfg.use_adagrad_bilagrid_optim);
        st.bilagrid_depth_init = true;
    }
    if (cfg.use_bilateral_grid_for_geometry && has_normal &&
        cfg.normal_supervision_weight > 0.0f &&
        (cfg.use_adagrad_bilagrid_optim ? cfg.bilagrid_adagrad_normal_lr
                                        : cfg.bilagrid_normal_lr) > 0.0f) {
        engine_init_bilagrid_normal(n_grids,
                                    cfg.bilagrid_shape_geometry[2],
                                    cfg.bilagrid_shape_geometry[1],
                                    cfg.bilagrid_shape_geometry[0],
                                    optim_bits, value_bits,
                                    cfg.use_adagrad_bilagrid_optim);
        st.bilagrid_normal_init = true;
    }
    if (cfg.use_ppisp &&
        (cfg.use_adagrad_ppisp_optim ? cfg.ppisp_adagrad_lr : cfg.ppisp_lr) > 0.0f) {
        std::vector<float> exif_ev;
        if (cfg.ppisp_exposure_from_exif) {
            int n_exif = 0;
            exif_ev = exif_exposure_evs(ds, post, n_exif);
            if (n_exif > 0)
                log(lfmt(lmsg::ppisp_exif_exposure,
                         {(long long)n_exif, (long long)ds.num_cameras}));
        }
        engine_init_ppisp(n_grids, cfg.ppisp_param_type,
                          cfg.use_adagrad_ppisp_optim, exif_ev);
        st.ppisp_init = true;
    }

    // ---- Resume --------------------------------------------------------
    // Last, because engine_load_checkpoint() overwrites the skeleton just
    // built: the world must already be allocated at max_num_splats and every
    // appearance channel the checkpoint carries must already exist as a
    // restore target, which is what everything above establishes.
    if (!cfg.resume.empty()) restore_checkpoint();
}

// Where the run's first splats come from: the dataset's point cloud, an
// --init-ply warm start, or both. A resume overwrites them either way.
SeedSplats TrainerSession::seed_world(const ColorResolution& color) {
    if (cfg.init_ply.empty() || !cfg.resume.empty())
        return seed_splats(ds.points, cfg, color);

    // A run or checkpoint directory resolves to its splat.ply, as --resume does.
    const std::string ply = find_splat_ply(cfg.init_ply).first;
    SeedSplats s = seed_splats_from_ply(ply, cfg, color);
    const int64_t from_ply = s.num;
    if (cfg.init_ply_add_points) append_point_seeds(s, ds.points, cfg, color);
    log(lfmt(lmsg::seeded_from_ply,
             {ply, (long long)from_ply, (long long)(s.num - from_ply)}));
    return s;
}

// Restore engine state from cfg.resume, adapting the checkpoint's buffers on
// the host first when its layout differs from the one just built (fewer
// splats, different SH degree, bilagrid/PPISP added or dropped).
void TrainerSession::restore_checkpoint() {
    ckpt::ResolvedCheckpoint r = ckpt::resolve_checkpoint(cfg.resume);
    ckpt::check_resumable(r.ckpt_dir);

    // The target is what the engine ACTUALLY holds, not what the config asks
    // for: a depth/normal grid also needs the dataset to carry those maps,
    // which setup_engine() resolved into `st`.
    ckpt::TargetLayout target;
    target.max_num_splats = engine_get_max_num_splats();
    target.num_sh         = (cfg.sh_degree + 1) * (cfg.sh_degree + 1);
    target.num_images     = (int)post.n_post;
    auto lhw = [](const std::array<int, 3>& xyw) {
        return std::array<int, 3>{xyw[2], xyw[1], xyw[0]};   // (X,Y,W)->(L,H,W)
    };
    if (st.bilagrid_rgb_init)    target.bilagrid_rgb    = lhw(cfg.bilagrid_shape);
    if (st.bilagrid_depth_init)  target.bilagrid_depth  = lhw(cfg.bilagrid_shape_geometry);
    if (st.bilagrid_normal_init) target.bilagrid_normal = lhw(cfg.bilagrid_shape_geometry);
    target.ppisp = st.ppisp_init;

    fs::path load_from = r.ckpt_dir;
    fs::path tmp;
    bool adapted = false;
    {
        JsonValue state = ckpt::read_state_json(r.ckpt_dir);
        if (ckpt::needs_adapt(state, target)) {
            tmp = out_dir / ".resume_adapt";
            log(lmsg::ckpt_adapting.get());
            adapted = ckpt::adapt_checkpoint(r.ckpt_dir, target, tmp);
            if (adapted) load_from = tmp;
        }
    }

    try {
        start_step = engine_load_checkpoint(load_from.string());
    } catch (const std::exception& e) {
        if (adapted) remove_tree(tmp);
        throw std::runtime_error(
            std::string("cannot resume from ") + r.ckpt_dir.string() + ": " +
            e.what());
    }
    if (adapted) remove_tree(tmp);
    log(lfmt(lmsg::resumed_from, {r.ckpt_dir.string(), (long long)start_step}));
}

void TrainerSession::save_checkpoint(int step) {
    char name[32];
    std::snprintf(name, sizeof name, "step-%09d.ckpt", step);
    fs::path ckpt = out_dir / name;
    fs::create_directories(ckpt);
    engine_save_checkpoint(ckpt.string(), cfg.save_full_checkpoint, step);
    if (cfg.save_only_latest_checkpoint) {
        std::vector<fs::path> stale;
        for (const auto& e : fs::directory_iterator(out_dir)) {
            std::string b = e.path().filename().string();
            if (b.rfind("step-", 0) == 0 &&
                b.find(".ckpt") != std::string::npos && e.path() != ckpt)
                stale.push_back(e.path());
        }
        for (const auto& p : stale) remove_tree(p);
    }
}

// One step. Split out of train() so a front-end that keeps its own loop
// shares this per-step config rather than rebuilding it.
std::map<std::string, float> TrainerSession::train_step(int step) {
    int sh_degree_to_use = step / std::max(cfg.sh_degree_warmup_every, 1);
    EngineStepConfig sc = build_step_config(cfg, st, step);
    return engine_train_step_managed(
        step, cfg.num_iterations, cfg.primitive, sh_degree_to_use,
        cfg.packed || cfg.use_bvh, sc);
}

void TrainerSession::pause_clock_start() {
    std::lock_guard<std::mutex> lk(_time_mutex);
    _pause_start = std::chrono::steady_clock::now();
}

void TrainerSession::pause_clock_stop() {
    std::lock_guard<std::mutex> lk(_time_mutex);
    if (_pause_start == std::chrono::steady_clock::time_point{}) return;
    _paused_s += std::chrono::duration<double>(
        std::chrono::steady_clock::now() - _pause_start).count();
    _pause_start = {};
}

double TrainerSession::elapsed_seconds() const {
    using Clock = std::chrono::steady_clock;
    std::lock_guard<std::mutex> lk(_time_mutex);
    if (_start_time == Clock::time_point{}) return 0.0;
    const Clock::time_point now =
        _end_time == Clock::time_point{} ? Clock::now() : _end_time;
    double s = std::chrono::duration<double>(now - _start_time).count() -
               _paused_s;
    if (_pause_start != Clock::time_point{})
        s -= std::chrono::duration<double>(now - _pause_start).count();
    return std::max(0.0, s);
}

double TrainerSession::avg_step_latency() const {
    std::lock_guard<std::mutex> lk(_progress_mutex);
    if (_step_latencies.empty()) return -1.0;
    double sum = 0.0;
    for (double v : _step_latencies) sum += v;
    return sum / (double)_step_latencies.size();
}

double TrainerSession::eta_seconds() const {
    const double avg = avg_step_latency();
    const int step = cur_step.load();
    if (avg < 0.0 || step <= 0) return -1.0;
    return std::max(0, cfg.num_iterations - step) * avg;
}

void TrainerSession::train(const TrainerCallbacks& cb) {
    {
        std::lock_guard<std::mutex> lk(_time_mutex);
        _start_time = std::chrono::steady_clock::now();
        _end_time = _pause_start = {};
        _paused_s = 0.0;
    }

    int step = start_step;
    for (; step < cfg.num_iterations; step++) {
        // Pause gate + render-fairness yield: give viewer render workers an
        // uncontended window to take the engine mutex.
        if (paused.load() && !stop_requested.load()) {
            pause_clock_start();
            while (paused.load() && !stop_requested.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            pause_clock_stop();
        }
        if (stop_requested.load()) break;
        while (render_pending.load())
            std::this_thread::sleep_for(std::chrono::microseconds(500));

        auto step_start = std::chrono::steady_clock::now();
        std::map<std::string, float> losses;
        std::string data_error;
        {
            std::lock_guard<std::mutex> lk(engine_mutex);
            if (step > 0 && cfg.steps_per_save > 0 && step % cfg.steps_per_save == 0)
                save_checkpoint(step);
            try {
                losses = train_step(step);
            } catch (const DataDecodeError& e) {
                data_error = e.what();
            }
        }
        // Asking outside the lock: the front end may sit on this for minutes
        // while the user puts the dataset back, and the viewport still wants
        // to render.
        if (!data_error.empty()) {
            if (!cb.on_data_error) {
                engine_resolve_data_error(false);
                throw std::runtime_error(data_error);
            }
            pause_clock_start();          // waiting on a human is not run time
            const bool retry = cb.on_data_error(data_error);
            pause_clock_stop();
            engine_resolve_data_error(retry);
            if (!retry) break;
            --step;                      // this step never ran
            continue;
        }
        cur_step = step + 1;
        double latency = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - step_start).count();
        {
            std::lock_guard<std::mutex> lk(_progress_mutex);
            _step_latencies.push_back(latency);
            if (_step_latencies.size() > 100) _step_latencies.pop_front();
        }

        if (cb.on_step) {
            TrainerProgress p;
            p.step = step;
            p.total_steps = cfg.num_iterations;
            p.step_latency = latency;
            p.num_splats = engine_get_cur_num_splats();
            p.losses = std::move(losses);
            cb.on_step(p);
        }
    }

    {
        std::lock_guard<std::mutex> lk(_time_mutex);
        _end_time = std::chrono::steady_clock::now();
    }
    training_time_s = elapsed_seconds();
    // Pool capacities are a monotonic high-water mark, so reading them after
    // the loop gives the training-time peak.
    {
        size_t cap = 0;
        for (const auto& e : engine_get_pool_breakdown()) cap += std::get<2>(e);
        engine_vram_mb = (double)(cap + engine_get_scratch_bytes()) / (1024.0 * 1024.0);
    }

    if (cfg.steps_per_save != 0 && save_on_stop.load()) {
        std::lock_guard<std::mutex> lk(engine_mutex);
        save_checkpoint(step);
        log(lfmt(lmsg::checkpoint_saved, {fs::absolute(out_dir).string()}));
    }

    // Steps THIS run, not cur_step: a resumed run's clock starts here too, and
    // a count that included the checkpoint's steps would not match the time.
    log(lfmt(lmsg::train_finished, {cur_step.load() - start_step,
                                    format_duration(training_time_s)}));
}

std::string TrainerSession::progress_json() {
    int step = cur_step.load();
    double elapsed = elapsed_seconds();
    double avg = avg_step_latency();
    double eta = eta_seconds();
    char buf[256];
    if (eta >= 0.0) {
        std::snprintf(buf, sizeof buf,
            "{\"step\": %d, \"total_steps\": %d, \"elapsed_time\": %.3f, "
            "\"eta\": %.3f, \"latency_ms\": %.3f, \"paused\": %s}",
            step, cfg.num_iterations, elapsed, eta, avg * 1000.0,
            paused.load() ? "true" : "false");
    } else {
        std::snprintf(buf, sizeof buf,
            "{\"step\": %d, \"total_steps\": %d, \"elapsed_time\": %.3f, "
            "\"eta\": null, \"latency_ms\": null, \"paused\": %s}",
            step, cfg.num_iterations, elapsed,
            paused.load() ? "true" : "false");
    }
    return buf;
}

ViewerRenderConfig TrainerSession::make_viewer_config() const {
    ViewerRenderConfig vc;
    vc.primitive = cfg.primitive;
    vc.packed = cfg.packed || cfg.use_bvh;
    vc.sh_degree_warmup_every = cfg.sh_degree_warmup_every;
    vc.relative_scale = cfg.relative_scale;
    vc.output_median = cfg.mean_median_depth_weight > 0.0f ||
                       cfg.median_depth_normal_reg_weight > 0.0f ||
                       cfg.median_normal_supervision_weight > 0.0f ||
                       cfg.median_render_normal_reg_weight > 0.0f;
    vc.distortion_reg_on = cfg.rgb_distortion_reg != 0.0f ||
                           cfg.depth_distortion_reg != 0.0f ||
                           cfg.normal_distortion_reg != 0.0f;
    const auto color = resolve_color(cfg);
    vc.color_space_on = color.splat_linear || !color.splat_gamut.empty();
    vc.train_frame_scale = ds.train_frame_scale;
    vc.train_to_normalized = ds.train_to_normalized;
    vc.base_camera_size = viewer_base_camera_size;
    return vc;
}

ViewerHooks TrainerSession::make_viewer_hooks() {
    ViewerHooks hooks;
    hooks.engine_mutex = &engine_mutex;
    hooks.current_step = [this] { return cur_step.load(); };
    hooks.set_render_pending = [this](bool v) { render_pending = v; };
    hooks.pause_toggle = [this] {
        bool now = !paused.load();
        paused = now;
        return now;
    };
    hooks.progress_json = [this] { return progress_json(); };
    return hooks;
}



// ===========================================================================
// Eval
// ===========================================================================

namespace {

// Clip to [0,1] and quantize to 8-bit, which is what the saved PNGs hold and
// therefore what the Python LPIPS tool sees.
std::vector<uint8_t> to_png_bytes(const std::vector<float>& rgb) {
    std::vector<uint8_t> out(rgb.size());
    for (size_t i = 0; i < rgb.size(); i++) {
        float v = std::min(std::max(rgb[i], 0.0f), 1.0f);
        out[i] = (uint8_t)std::lround(v * 255.0f);
    }
    return out;
}

}  // namespace

void TrainerSession::eval() {
    // eval_mode "all" trains on every frame, so nothing is held out.
    if (cfg.eval_mode == "all") return;

    // Re-parse for the eval side of the split. The parser computes the split
    // over all frames, so this is the exact complement of what training saw.
    DatasetParserConfig pcfg;
    pcfg.recon_dir            = cfg.colmap_recon_dir;
    pcfg.image_dir            = cfg.image_dir;
    pcfg.mask_dir             = cfg.mask_dir;
    pcfg.depth_dir            = cfg.depth_dir;
    pcfg.normal_dir           = cfg.normal_dir;
    pcfg.validation_fraction  = 0.0f;      // no early-stop holdout inside eval
    pcfg.eval_mode            = cfg.eval_mode;
    pcfg.eval_interval        = cfg.eval_interval;
    pcfg.train_split_fraction = cfg.train_split_fraction;
    pcfg.outlier_threshold    = cfg.outlier_threshold;
    pcfg.rescale_camera_to_fit   = cfg.rescale_camera_to_fit;
    pcfg.downscale_rounding_mode = cfg.downscale_rounding_mode;
    pcfg.metashape_xml           = cfg.metashape_xml;
    pcfg.metashape_ply           = cfg.metashape_ply;
    pcfg.metashape_psx           = cfg.metashape_psx;
    pcfg.split                   = "eval";

    ParsedDataset eds = parse_dataset(cfg.data, pcfg, cfg.data_format);
    if (eds.num_cameras == 0) {
        log(lmsg::eval_split_empty.get());
        return;
    }
    // Same world scaling training applied, so the cameras line up with the
    // trained splats.
    if (cfg.relative_scale.has_value()) {
        float rs = *cfg.relative_scale;
        for (int64_t i = 0; i < eds.num_cameras; i++)
            for (int r = 0; r < 3; r++) eds.c2w[i*12 + r*4 + 3] *= rs;
    }
    // Uniform whatever the run trains with: eval renders one pass per image,
    // and its metrics stay comparable between the two fits.
    PostSplitCameras epost = bake_post_split(
        eds, cfg.warp_to_pinhole, cfg.warp_spherical_to_pinhole,
        WarpFaceFit::Uniform);

    // One image per step: metrics are per-image, and the batch scheduler would
    // otherwise pack several resolutions into one step.
    DataManagerConfig dm;
    dm.cache_mode  = (cfg.cache_images == "disk") ? CacheMode::DISK : CacheMode::CPU;
    const bool eval_masks = !eds.mask_filenames.empty() && cfg.load_masks;
    dm.load_masks  = eval_masks || epost.any_fov_mask;
    dm.load_depths = false;
    dm.load_normals = false;
    dm.train_batch_size = 1;
    dm.val_batch_size   = 1;
    dm.mask_boundary_offset = cfg.mask_boundary_offset;
    std::vector<int32_t> all_idx((size_t)eds.num_cameras);
    std::iota(all_idx.begin(), all_idx.end(), 0);

    {
        std::lock_guard<std::mutex> lk(engine_mutex);
        engine_setup_data_manager(
            dm, eds.camera_models, eds.camera_distortions,
            eds.image_filenames,
            eval_masks ? eds.mask_filenames : std::vector<std::string>{}, {}, {},
            eds.widths, eds.heights,
            epost.any_warp ? epost.K_per_camera : std::vector<int32_t>{},
            epost.any_warp ? epost.post_offsets : std::vector<int32_t>{},
            epost.viewmats, epost.intrins, epost.dist_coeffs,
            epost.any_warp ? epost.post_widths : std::vector<int32_t>{},
            epost.any_warp ? epost.post_heights : std::vector<int32_t>{},
            epost.any_warp ? epost.face_axes : std::vector<float>{},
            epost.input_intrins, epost.input_dist_coeffs,
            epost.redistort_models, epost.redistort_params,
            all_idx, {});
    }

    log(lfmt(lmsg::eval_views, {(long long)epost.n_post}));

    // Rendering is serial (one process-global engine), but scoring a view --
    // colour correction, the metrics, and the PNG encode -- is pure host work
    // that depends on nothing else, so it runs on a pool while the GPU gets on
    // with the next view. Results are written into a slot indexed by view, so
    // the per-image lists in metrics.json do not depend on who finished first.
    struct ViewJob {
        int64_t index = 0;
        int H = 0, W = 0, C = 0;
        std::vector<float> gt, pred;
    };
    struct ViewScore {
        bool  filled = false;
        float l1 = 0, psnr = 0, ssim = 0, cc_l1 = 0, cc_psnr = 0, cc_ssim = 0;
    };

    // Per worker: a GT + a render + the colour-corrected copy (3 x H*W*C
    // floats) plus SSIM's eleven H*W double buffers. ~725 MB at 4K, so the
    // pool is capped by a memory budget as well as by cores -- an eval run
    // must not be the thing that OOMs the machine.
    const int64_t view_pixels = eds.widths.empty()
        ? (int64_t)1 << 20
        : (int64_t)eds.widths[0] * eds.heights[0];
    const int64_t bytes_per_view = view_pixels * (3 * 3 * 4 + 11 * 8);
    const int64_t kBudget = (int64_t)4 << 30;
    int n_workers = (int)std::max(1u, std::thread::hardware_concurrency() / 4u);
    n_workers = (int)std::min<int64_t>(n_workers,
                                       std::max<int64_t>(1, kBudget / std::max<int64_t>(bytes_per_view, 1)));
    n_workers = std::min<int>(n_workers, 8);

    std::deque<ViewJob> queue;
    std::vector<ViewScore> scores;
    std::mutex qmu, smu;
    std::condition_variable qcv, spacecv;
    bool producing = true;
    std::exception_ptr worker_error;

    auto score_view = [&](ViewJob& j) {
        const int64_t view_px = (int64_t)j.H * j.W * j.C;
        const float* g = j.gt.data();
        thread_local std::vector<float> cc;   // reused across this worker's views
        color_correct_into(j.pred.data(), g, (int64_t)j.H * j.W, j.C, cc);
        ViewScore s;
        s.filled  = true;
        s.l1      = image_l1(g, j.pred.data(), view_px);
        s.psnr    = image_psnr(g, j.pred.data(), view_px);
        s.ssim    = image_ssim(g, j.pred.data(), j.H, j.W, j.C);
        s.cc_l1   = image_l1(g, cc.data(), view_px);
        s.cc_psnr = image_psnr(g, cc.data(), view_px);
        s.cc_ssim = image_ssim(g, cc.data(), j.H, j.W, j.C);
        {
            std::lock_guard<std::mutex> lk(smu);
            if ((size_t)j.index >= scores.size()) scores.resize((size_t)j.index + 1);
            scores[(size_t)j.index] = s;
        }
        if (cfg.save_eval_images) {
            char nm[64];
            auto png = [&](const char* kind, const std::vector<float>& img) {
                std::snprintf(nm, sizeof nm, "eval-%s-%05d.png", kind,
                              (int)j.index);
                std::vector<uint8_t> bytes = to_png_bytes(img);
                stbi_write_png((out_dir / nm).string().c_str(), j.W, j.H, j.C,
                               bytes.data(), j.W * j.C);
            };
            // Both sides, because the LPIPS tool needs the pair -- and the
            // 8-bit clip here is what it will score, so its numbers are
            // reproducible from the files alone.
            png("gt", j.gt);
            png("render", j.pred);
        }
    };

    std::vector<std::thread> workers;
    for (int t = 0; t < n_workers; t++) {
        workers.emplace_back([&] {
            // Threads inside the metrics would oversubscribe against the pool;
            // the per-view work is already the coarser and cheaper split.
#ifdef _OPENMP
            omp_set_num_threads(std::max(1, (int)std::thread::hardware_concurrency() / n_workers));
#endif
            for (;;) {
                ViewJob job;
                {
                    std::unique_lock<std::mutex> lk(qmu);
                    qcv.wait(lk, [&] { return !queue.empty() || !producing; });
                    if (queue.empty()) return;
                    job = std::move(queue.front());
                    queue.pop_front();
                }
                spacecv.notify_one();
                try {
                    score_view(job);
                } catch (...) {
                    std::lock_guard<std::mutex> lk(smu);
                    if (!worker_error) worker_error = std::current_exception();
                    return;
                }
            }
        });
    }

    const int sh_deg = cfg.sh_degree;
    int64_t next_slot = 0;

    for (int64_t i = 0; i < eds.num_cameras; i++) {
        int64_t H = 0, W = 0, B = 0, Cc = 0;
        std::vector<float> gt, render;
        {
            std::lock_guard<std::mutex> lk(engine_mutex);
            int n_view = engine_eval_forward(cfg.primitive, sh_deg, cfg.packed);
            if (n_view == 0) break;
            auto shape = engine_get_render_rgb_shape();
            B = std::get<0>(shape); H = std::get<1>(shape);
            W = std::get<2>(shape); Cc = std::get<3>(shape);
            const int64_t npx = B * H * W * Cc;
            render.resize((size_t)npx);
            gt.resize((size_t)npx);
            engine_copy_render_to_host(
                TorchTensorView{(uint64_t)(uintptr_t)render.data(), 4, {B, H, W, Cc}},
                TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}},
                TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}});
            engine_copy_gt_rgb_to_host(
                TorchTensorView{(uint64_t)(uintptr_t)gt.data(), 4, {B, H, W, Cc}});
        }

        // Per POST-split view inside the batch (K faces of one input image).
        const int64_t view_px = H * W * Cc;
        for (int64_t v = 0; v < B; v++) {
            ViewJob job;
            job.index = next_slot++;
            job.H = (int)H; job.W = (int)W; job.C = (int)Cc;
            job.gt.assign(gt.begin() + (size_t)(v * view_px),
                          gt.begin() + (size_t)((v + 1) * view_px));
            job.pred.assign(render.begin() + (size_t)(v * view_px),
                            render.begin() + (size_t)((v + 1) * view_px));
            for (float& x : job.pred) x = std::min(std::max(x, 0.0f), 1.0f);
            {
                std::unique_lock<std::mutex> lk(qmu);
                spacecv.wait(lk, [&] { return (int)queue.size() < n_workers; });
                queue.push_back(std::move(job));
            }
            qcv.notify_one();
        }
    }

    {
        std::lock_guard<std::mutex> lk(qmu);
        producing = false;
    }
    qcv.notify_all();
    for (auto& t : workers) t.join();
    if (worker_error) std::rethrow_exception(worker_error);

    std::map<std::string, std::vector<float>> per_image;
    for (const ViewScore& s : scores) {
        if (!s.filled) continue;
        per_image["l1"].push_back(s.l1);
        per_image["psnr"].push_back(s.psnr);
        per_image["ssim"].push_back(s.ssim);
        per_image["cc_l1"].push_back(s.cc_l1);
        per_image["cc_psnr"].push_back(s.cc_psnr);
        per_image["cc_ssim"].push_back(s.cc_ssim);
    }

    if (per_image.empty()) {
        log(lmsg::eval_no_views.get());
        return;
    }

    std::map<std::string, float> avg;
    for (const auto& [k, v] : per_image) {
        double s = 0.0;
        for (float x : v) s += x;
        avg[k] = (float)(s / (double)v.size());
        log("  " + k + ": " + std::to_string(avg[k]));
    }

    // metrics.json: per-image lists plus avg_* scalars, the shape
    // reference/python/benchmark.py reads back.
    std::ofstream mf((out_dir / "metrics.json").string());
    if (!mf) throw std::runtime_error("cannot write metrics.json");
    mf << "{\n";
    bool first = true;
    for (const auto& [k, v] : per_image) {
        mf << (first ? "" : ",\n") << "    \"" << k << "\": [";
        for (size_t i = 0; i < v.size(); i++)
            mf << (i ? ", " : "") << v[i];
        mf << "]";
        first = false;
    }
    for (const auto& [k, v] : avg)
        mf << ",\n    \"avg_" << k << "\": " << v;
    mf << ",\n    \"num_eval_images\": " << per_image.begin()->second.size();
    // Per-run training stats: a benchmark launches each scene as its own
    // process, so metrics.json is the only place it can read these.
    mf << ",\n    \"training_time\": " << training_time_s;
    mf << ",\n    \"engine_vram\": " << engine_vram_mb;
    mf << "\n}\n";
    log(lfmt(lmsg::eval_metrics_written, {(out_dir / "metrics.json").string()}));
}

}  // namespace spirula
