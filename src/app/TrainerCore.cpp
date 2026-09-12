// TrainerCore.cpp -- see TrainerCore.h.

#include "app/TrainerCore.h"
#include "data/SceneTransform.h"
#include "backend/api/BackendRuntime.h"
#include "app/EvalMetrics.h"
#include "checkpoint/Adapt.h"
#include "checkpoint/Resume.h"
#include "config/TrainConfigJson.h"
#include "core/ColorSpace.h"
#include "core/ExrImage.h"
#include "i18n/catalog/Log.h"
#include "data/CameraMath.h"
#include "data/ImageProbe.h"
#include "data/DataManager.h"
#include "data/Knn.h"
#include "sfm/core/Exif.h"
#include "engine/EngineState.h"
#include "external/stb_image.h"

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
#include <limits>
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

// "none" is how both front ends spell unset for a string field, and the GUI
// writes it literally when a preset gave the field a value.
static bool unset(const std::string& v) { return v.empty() || v == "none"; }

// "Rec.709" is the config saying "sRGB, and do not take the file's word for
// it"; resolved it is the identity, same as unset.
static std::string resolved_gamut(const std::string& name) {
    return name == "Rec.709" ? std::string() : name;
}

ColorResolution resolve_color(const TrainConfig& c) {
    ColorResolution r;
    r.image_gamut    = unset(c.image_color_gamut)
                           ? std::string() : resolved_gamut(c.image_color_gamut);
    r.image_linear   = c.image_color_is_linear.value_or(false);
    r.image_transfer = colorspace::transfer_or(c.image_color_transfer,
                                               colorspace::Transfer::Srgb);
    std::optional<bool> convert = c.convert_initial_point_cloud_color;
    auto declared = [&] { if (!convert.has_value()) convert = true; };

    // Each splat-side half falls back to the images, so declaring only the
    // input still renders back into the space the input came from.
    if (unset(c.splat_color_gamut)) {
        r.splat_gamut = r.image_gamut;
    } else {
        r.splat_gamut = resolved_gamut(c.splat_color_gamut);
        declared();
    }

    if (c.splat_color_is_linear.has_value()) {
        r.splat_linear = *c.splat_color_is_linear;
        declared();
    } else {
        r.splat_linear = r.image_linear;
    }

    if (unset(c.splat_color_transfer)) {
        r.splat_transfer = r.image_transfer;
    } else {
        r.splat_transfer = colorspace::transfer_or(c.splat_color_transfer,
                                                   r.image_transfer);
        declared();
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

namespace {

struct SeedRows {
    int64_t min_init = 0;
    int64_t live = 0;
};

SeedRows resolve_seed_rows(int64_t source_count, const TrainConfig& cfg) {
    if (source_count <= 0)
        throw std::runtime_error("seed_splats: empty point cloud");
    const int64_t cap_max = std::max<int64_t>(cfg.cap_max, 1);
    int64_t min_init = std::max<int64_t>(
        (int64_t)(std::min(cfg.min_init_fraction, 1.0f) * (double)cap_max), 1);
    min_init = std::min<int64_t>(min_init, cap_max);
    SeedRows rows;
    rows.min_init = min_init;
    rows.live = source_count > cap_max
        ? cap_max : std::max<int64_t>(source_count, min_init);
    return rows;
}

}  // namespace

int64_t resolve_training_splat_capacity(int64_t source_count,
                                        const TrainConfig& cfg) {
    const SeedRows rows = resolve_seed_rows(source_count, cfg);
    const int64_t cap_max = std::max<int64_t>(cfg.cap_max, 1);
    if (cfg.preallocate_splat_tensors) return cap_max;
    int64_t capacity = rows.live;
    if (!cfg.resume.empty()) {
        const ckpt::ResolvedCheckpoint r = ckpt::resolve_checkpoint(cfg.resume);
        const JsonValue state = ckpt::read_state_json(r.ckpt_dir);
        const int64_t cur = (int64_t)state.get_double("cur_num_splats", 0);
        capacity = std::max(capacity, std::min(cur, cap_max));
    }
    return capacity;
}

SeedSplats seed_splats(const ColmapPoints3D& pts, const TrainConfig& cfg,
                       const ColorResolution& color, int64_t capacity) {
    std::mt19937 rng(42);
    std::normal_distribution<float> gauss(0.f, 1.f);
    std::uniform_real_distribution<float> uni(0.f, 1.f);

    float scale_init   = cfg.scale_init.value_or(0.5f);
    float opacity_init = cfg.opacity_init.value_or(0.1f);

    const int64_t n_src = pts.num();
    const int64_t cap_max = std::max<int64_t>(cfg.cap_max, 1);
    const SeedRows rows = resolve_seed_rows(n_src, cfg);
    if (capacity < rows.live)
        throw std::invalid_argument("seed capacity is below the live point count");
    const int64_t min_init = rows.min_init;

    std::vector<int64_t> pick;
    if (n_src > cap_max) {
        pick.resize((size_t)n_src);
        std::iota(pick.begin(), pick.end(), 0);
        std::shuffle(pick.begin(), pick.end(), rng);
        pick.resize((size_t)cap_max);
    } else {
        // Repeat modulo when under min_init; the repeats are jittered apart
        // below, and pick[0 .. n_src) stay one per source point.
        pick.resize((size_t)rows.live);
        for (int64_t i = 0; i < rows.live; i++) pick[i] = i % n_src;
    }
    const int64_t n_distinct = std::min<int64_t>((int64_t)pick.size(), n_src);
    const int64_t num = (int64_t)pick.size();
    const int64_t cap = capacity;
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
            s.means[i*3 + d] = (float)(pts.xyz[pick[i]*3 + d] * rescale);

    // log(scale_init * sqrt(mean d^2 of 4-NN)) over xyz, over the DISTINCT
    // points: a repeat is its own zero-distance neighbor, and would seed
    // every splat at log(1e-8). TODO: suppress_initial_scales.
    std::vector<float> nn = knn::mean_knn_dist(s.means, n_distinct, 4);
    for (int64_t i = 0; i < num; i++) {
        float d = nn[i % n_distinct];
        // Scatter the repeats through the neighborhood they copy.
        if (i >= n_distinct)
            for (int k = 0; k < 3; k++)
                s.means[i*3 + k] += 0.25f * d * gauss(rng);
        float v = std::log(scale_init * d + 1e-8f);
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
            // The gamut matrix belongs in linear light, so decode through the
            // transfer, rotate, and re-encode only if the splats are stored so.
            for (int d = 0; d < 3; d++)
                col[d] = colorspace::tone_decode(col[d], color.splat_transfer);
            colorspace::apply3x3(to_splat, col);
            if (!color.splat_linear)
                for (int d = 0; d < 3; d++) col[d] = colorspace::linear_to_srgb(col[d]);
        }
        for (int d = 0; d < 3; d++)
            s.features_dc[i*3 + d] = (col[d] - 0.5f) / 0.28209479177387814f;
    }
    return s;
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
    cfg.loss.saturation_threshold = c.loss_saturation_threshold;
    cfg.loss.normalize_by_luminance = c.normalize_loss_by_luminance;
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
    // Front-loaded shape penalty. (p+1)(1-t)^p has unit integral over the run,
    // so p moves when the pressure is spent, not how much of it.
    float reg_t = std::min((float)step / (float)std::max(c.num_iterations, 1), 1.0f);
    auto reg_decay = [reg_t](float p) {
        p = std::max(p, 0.0f);
        return (p + 1.0f) * std::pow(1.0f - reg_t, p);
    };
    cfg.optim.mcmc_opacity_reg_weight     =
        c.opacity_reg * reg_decay(c.opacity_reg_decay_power);
    cfg.optim.mcmc_scale_reg_weight       =
        c.scale_reg * reg_decay(c.scale_reg_decay_power) / alpha;
    cfg.optim.erank_reg_weight            = c.erank_reg;
    cfg.optim.erank_reg_weight_s3         = c.erank_reg_s3;
    cfg.optim.quat_norm_reg_weight        = c.quat_norm_reg;
    cfg.optim.dc_reg_weight               = c.dc_reg;
    cfg.optim.sh_reg_weight               = c.sh_reg;
    cfg.optim.max_screen_size             = c.max_screen_size;
    cfg.optim.max_screen_size_penalty     = c.max_screen_size_penalty;
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
    cfg.densify.clip_screen_size_at_refine    = c.max_screen_size_penalty > 0.0f;
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
    cfg.densify.oversize_split_fraction = c.densify_oversize_split_fraction;
    cfg.densify.oversize_score_blend = c.densify_oversize_score_blend;
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
    cfg.ppisp.run_before_color_space = c.apply_ppisp_before_color_space;

    // ---- background ----------------------------------------------------
    if (c.background_mode == "noise" || c.background_mode == "pseudorandom") {
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

// p_train = relative_scale * (p_dataset - center): the parser's shift and the
// trainer's own rescale, which is every way the splats' frame differs from
// the dataset's.
void save_scene_transform_json(const ParsedDataset& ds, const TrainConfig& c,
                               const fs::path& out_dir) {
    SceneTransform T;
    T.scale = (double)c.relative_scale.value_or(1.0f);
    for (int i = 0; i < 3; i++) T.t[i] = -T.scale * ds.center[i];
    const std::string text =
        scene_transform_json(T, ds.center_mode, ds.center.data());
    const fs::path path = out_dir / "scene_transform.json";
    FILE* f = std::fopen(path.string().c_str(), "w");
    if (!f) throw std::runtime_error("cannot write " + path.string());
    std::fputs(text.c_str(), f);
    std::fclose(f);
}

// Flat, one key per flag: a key that followed the field table's heading
// moved whenever a flag was reshuffled, and readers fell back to the default.
// Macro flags are written beside what they resolved to; config/TrainConfigJson.h.
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
    if (c.train_frame != "points")    return not_impl("--train-frame " + c.train_frame);
    if (c.primitive != "3dgs" && c.primitive != "mip" && c.primitive != "3dgut")
        return not_impl("--primitive " + c.primitive);
    if (c.quantization_level != 0 && c.quantization_level != 1)
        return lmsg::bad_quantization_level.get();
    return {};
}

// Unported-feature guards: fail early rather than ignore a flag.
void TrainerSession::check_config() {
    if (std::string what = train_config_unsupported(cfg); !what.empty())
        throw std::runtime_error(what);
    if (cfg.memory_limit_gib.has_value() &&
        (!std::isfinite(*cfg.memory_limit_gib) || *cfg.memory_limit_gib <= 0.0f))
        throw std::runtime_error(lmsg::bad_memory_limit.get());
    if (cfg.validation_fraction > 0)
        log(lmsg::warn_validation_unported.get());
    if (cfg.orientation_method != "up" || cfg.center_method != "poses")
        log(lfmt(lmsg::warn_pose_normalization_approx,
                 {cfg.orientation_method, cfg.center_method}));
}

namespace {

DatasetParserConfig parser_config(const TrainConfig& cfg, bool eval) {
    DatasetParserConfig pcfg;
    pcfg.recon_dir = cfg.colmap_recon_dir;
    pcfg.image_dir = cfg.image_dir;
    pcfg.mask_dir = cfg.mask_dir;
    pcfg.depth_dir = cfg.depth_dir;
    pcfg.normal_dir = cfg.normal_dir;
    pcfg.validation_fraction = eval ? 0.0f : cfg.validation_fraction;
    pcfg.eval_mode = cfg.eval_mode;
    pcfg.eval_interval = cfg.eval_interval;
    pcfg.train_split_fraction = cfg.train_split_fraction;
    pcfg.outlier_threshold = cfg.outlier_threshold;
    pcfg.center_mode = cfg.scene_center;
    pcfg.exif_orientation = cfg.exif_orientation;
    pcfg.probe_image_size = probe_image_size;
    pcfg.train_resolution_divisor = cfg.train_resolution_divisor;
    pcfg.downscale_rounding_mode = cfg.downscale_rounding_mode;
    pcfg.metashape_xml = cfg.metashape_xml;
    pcfg.metashape_ply = cfg.metashape_ply;
    pcfg.metashape_psx = cfg.metashape_psx;
    if (eval) pcfg.split = "eval";
    return pcfg;
}

RunState resolve_appearance_state(const TrainConfig& cfg, bool depth, bool normal) {
    RunState state;
    state.bilagrid_rgb_init = cfg.use_bilateral_grid &&
        (cfg.use_adagrad_bilagrid_optim ? cfg.bilagrid_adagrad_lr : cfg.bilagrid_lr) > 0;
    state.bilagrid_depth_init = cfg.use_bilateral_grid_for_geometry && depth &&
        cfg.depth_supervision_weight > 0 &&
        (cfg.use_adagrad_bilagrid_optim ? cfg.bilagrid_adagrad_depth_lr : cfg.bilagrid_depth_lr) > 0;
    state.bilagrid_normal_init = cfg.use_bilateral_grid_for_geometry && normal &&
        cfg.normal_supervision_weight > 0 &&
        (cfg.use_adagrad_bilagrid_optim ? cfg.bilagrid_adagrad_normal_lr : cfg.bilagrid_normal_lr) > 0;
    state.ppisp_init = cfg.use_ppisp &&
        (cfg.use_adagrad_ppisp_optim ? cfg.ppisp_adagrad_lr : cfg.ppisp_lr) > 0;
    return state;
}

}  // namespace

void TrainerSession::load_dataset() {
    const DatasetParserConfig pcfg = parser_config(cfg, false);
    ds = parse_dataset(cfg.data, pcfg, cfg.data_format);
    if (ds.center_mode != "none") {
        char xyz[96];
        std::snprintf(xyz, sizeof xyz, "%.12g, %.12g, %.12g",
                      ds.center[0], ds.center[1], ds.center[2]);
        log(lfmt(lmsg::scene_centered, {ds.center_mode, xyz}));
    }

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
        resolve_face_fit(cfg), cfg.warp_back_face);

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

namespace {

uint64_t sat_add(uint64_t a, uint64_t b) {
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}

uint64_t sat_mul(uint64_t a, uint64_t b) {
    return a != 0 && b > UINT64_MAX / a ? UINT64_MAX : a * b;
}

uint64_t ceil_div(uint64_t n, uint64_t d) {
    return n / d + (n % d != 0);
}

struct MemoryAllocations {
    std::map<std::string, uint64_t> retained;
    uint64_t transient = 0;

    void add(const std::string& key, uint64_t bytes) {
        auto& current = retained[key];
        current = std::max(current, bytes);
    }
    void add(PoolSlot slot, uint64_t bytes) { add(slot_name(slot), bytes); }
    void quant(PoolSlot slot, uint64_t cells, uint64_t bytes_per_cell,
               uint64_t bounds, uint64_t bytes_per_bound) {
        const std::string name = slot_name(slot);
        add(name + ".q", sat_mul(cells, bytes_per_cell));
        add(name + ".qb", sat_mul(bounds, bytes_per_bound));
    }
    TrainingMemoryEstimate result(bool fused) const {
        TrainingMemoryEstimate out;
        for (const auto& [key, bytes] : retained) {
            out.accounted_bytes = sat_add(out.accounted_bytes, bytes);
            out.conservative_allowance_bytes =
                std::max(out.conservative_allowance_bytes, bytes);
        }
        out.conservative_allowance_bytes =
            std::max(out.conservative_allowance_bytes, transient);
        out.fused_proj_bwd_optim = fused;
        return out;
    }
};

struct MemoryExtent {
    int64_t w = 0, h = 0;
    uint64_t pixels() const { return sat_mul((uint64_t)w, (uint64_t)h); }
    void merge(MemoryExtent other) {
        if (other.pixels() > pixels()) *this = other;
    }
    MemoryExtent half() const {
        return {std::max<int64_t>(w / 2, 1), std::max<int64_t>(h / 2, 1)};
    }
};

MemoryExtent modality_extent(const std::vector<std::string>& files, size_t i) {
    if (files.empty() || files[i].empty()) return {};
    int w = 0, h = 0;
    if (!probe_image_size(files[i].c_str(), &w, &h))
        throw std::runtime_error("Failed to probe training image '" + files[i] + "'");
    return {w, h};
}

struct MemoryGroup {
    int64_t count = 0, faces = 1;
    MemoryExtent rgb, mask, depth, normal;
    bool rgb_u8 = false, rgb_u16 = false, warped = false, redistort = false;
    std::vector<WarpFacePass> passes;
};

std::map<std::vector<int32_t>, MemoryGroup> memory_groups(
    const ParsedDataset& ds, const PostSplitCameras& post,
    const std::vector<int32_t>& indices, int face_cap,
    bool mask, bool depth, bool normal) {
    std::map<std::vector<int32_t>, MemoryGroup> groups;
    for (int32_t i : indices) {
        const int K = post.K_per_camera.empty() ? 1 : post.K_per_camera[i];
        const int offset = post.post_offsets.empty() ? i : post.post_offsets[i];
        const bool redistort = !post.redistort_models.empty() && post.redistort_models[i] >= 0;
        std::vector<int32_t> widths(K), heights(K);
        std::vector<int32_t> key{
            ds.widths[i], ds.heights[i], ds.camera_models[i],
            ds.camera_distortions[i], (int)redistort, K};
        for (int k = 0; k < K; ++k) {
            widths[k] = K > 1 ? post.post_widths[offset + k] : ds.widths[i];
            heights[k] = K > 1 ? post.post_heights[offset + k] : ds.heights[i];
            key.push_back(widths[k]);
            key.push_back(heights[k]);
        }
        auto& group = groups[key];
        if (group.count++ == 0) {
            group.rgb = {ds.widths[i], ds.heights[i]};
            group.faces = K;
            group.redistort = redistort;
            group.warped = K > 1 || redistort;
            group.passes = build_face_passes(widths.data(), heights.data(), K, face_cap);
        }
        const std::string path = ds.image_filenames.empty() ? std::string() : ds.image_filenames[i];
        if (!exr::is_exr(path)) {
            if (!path.empty() && stbi_is_16_bit(path.c_str())) group.rgb_u16 = true;
            else group.rgb_u8 = true;
        }
        if (mask) {
            auto extent = modality_extent(ds.mask_filenames, i);
            if (extent.pixels() == 0) extent = group.rgb;
            if (extent.pixels() > 1) group.mask.merge(extent);
        }
        if (depth) group.depth.merge(modality_extent(ds.depth_filenames, i));
        if (normal) group.normal.merge(modality_extent(ds.normal_filenames, i));
    }
    for (auto& [key, group] : groups) {
        if (mask && group.mask.pixels() == 0) group.mask = {1, 1};
        else if (mask && group.faces > 1) group.mask = group.rgb;
    }
    return groups;
}

MemoryExtent warped_normal(MemoryExtent face, MemoryExtent normal, MemoryExtent input) {
    if (normal.pixels() == 0) return {};
    return {
        std::max<int64_t>(1, (face.w * normal.w + input.w / 2) / input.w),
        std::max<int64_t>(1, (face.h * normal.h + input.h / 2) / input.h)};
}

void count_image_pass(MemoryAllocations& a, const MemoryGroup& group,
                      const WarpFacePass& pass, uint64_t inputs, bool training,
                      const TrainConfig& cfg, const RunState& state,
                      const EngineStepConfig& step, const ColorResolution& color) {
    const uint64_t cameras = sat_mul(inputs, (uint64_t)(pass.k1 - pass.k0));
    const MemoryExtent face{pass.width, pass.height};
    const MemoryExtent normal = training ? (group.warped ?
        warped_normal(face, group.normal, group.rgb) : group.normal) : MemoryExtent{};
    const MemoryExtent depth = training && group.depth.pixels() != 0 ?
        (group.warped ? face : group.depth) : MemoryExtent{};
    const MemoryExtent mask = group.mask.pixels() != 0 ?
        (group.warped ? face : group.mask) : MemoryExtent{};
    const auto bytes = [cameras](MemoryExtent size, uint64_t per_pixel) {
        return sat_mul(sat_mul(cameras, size.pixels()), per_pixel);
    };
    if (group.rgb_u8) a.add(PoolSlot::GtStagingU8, sat_mul(inputs, sat_mul(group.rgb.pixels(), 3)));
    if (group.rgb_u16) a.add(PoolSlot::GtStagingU16, sat_mul(inputs, sat_mul(group.rgb.pixels(), 6)));
    if (group.warped) {
        a.add(PoolSlot::GtStagingU8, sat_mul(inputs, group.mask.pixels()));
        a.add(PoolSlot::WarpInputIntrins, sat_mul(inputs, 16));
        a.add(PoolSlot::WarpInputDistCoeffs, sat_mul(inputs, 4 * kCameraDistortionParams));
        if (group.redistort) {
            a.add(PoolSlot::WarpSourceModels, sat_mul(inputs, 4));
            a.add(PoolSlot::WarpSourceParams, sat_mul(inputs, 64));
        }
        if (group.faces > 1) a.add(PoolSlot::WarpFaceAxes, sat_mul(cameras, 36));
    }
    if (training) {
        a.add(PoolSlot::GtStagingU8, sat_mul(inputs, sat_mul(group.normal.pixels(), 3)));
        a.add(PoolSlot::GtStagingU16, sat_mul(inputs, sat_mul(group.depth.pixels(), 2)));
    }
    a.add(PoolSlot::CamViewmats, sat_mul(cameras, 64));
    a.add(PoolSlot::CamIntrins, sat_mul(cameras, 16));
    a.add(PoolSlot::CamDistCoeffs, sat_mul(cameras, 4 * kCameraDistortionParams));
    a.add("renders.rgb", bytes(face, 12));
    a.add("renders.depth", bytes(face, 4));
    a.add(PoolSlot::RenderTs, bytes(face, 4));
    a.add(PoolSlot::RenderLastIds, bytes(face, 4));
    a.add(PoolSlot::GtRgb, bytes(face, 12));
    a.add(PoolSlot::GtAlpha, bytes(mask, 1));
    if (color.splat_on()) a.add(PoolSlot::ColorSpaceFwdPost, bytes(face, 12));
    const bool background = cfg.background_mode != "black";
    if (background) a.add(PoolSlot::EngBgSkyRgbPost, bytes(face, 12));
    if (cfg.background_mode == "sh") a.add(PoolSlot::EngBgSkyImage, bytes(face, 12));
    if (!training) return;

    a.add(PoolSlot::GtDepth, bytes(depth, 4));
    a.add(PoolSlot::GtNormal, bytes(normal, 12));
    a.add(PoolSlot::EngVRgb, bytes(face, 12));
    a.add(PoolSlot::EngVDepth, bytes(face, 4));
    a.add(PoolSlot::EngVTs, bytes(face, 4));
    const bool derived_normal = normal.pixels() != 0 ||
        step.loss.weights[(int)LossWeightIndex::MedianDepthNormalReg] > 0;
    if (derived_normal) {
        a.add(PoolSlot::EngDepthNormal, bytes(face, 12));
        a.add(PoolSlot::EngVDepthNormal, bytes(face, 12));
    }
    if (state.bilagrid_rgb_init) a.add(PoolSlot::EngBgRgbPost, bytes(face, 12));
    if (state.ppisp_init) a.add(PoolSlot::EngPpispRgbPost, bytes(face, 12));
    if (state.bilagrid_depth_init) {
        a.add(PoolSlot::EngVRefDepth, bytes(depth, 4));
        a.add(PoolSlot::EngBgDepthPost, bytes(depth, 4));
        a.add(PoolSlot::EngBgDepthTmpScalars, sat_mul(cameras, 4));
        a.add(PoolSlot::BilagridQuantileTemp, sat_mul(cameras, 261 * 4));
    }
    if (state.bilagrid_normal_init) {
        a.add(PoolSlot::EngVRefNormal, bytes(normal, 12));
        a.add(PoolSlot::EngBgNormalPost, bytes(normal, 12));
    }
    if (background) a.add(PoolSlot::EngBgSkyVTsScratch, bytes(face, 4));
    if (cfg.background_mode == "sh") a.add(PoolSlot::EngBgSkyVBg, bytes(face, 12));
    const DistortionType distortion = engine_distortion_type(
        engine_primitive_pixel_type(cfg.primitive),
        step.loss.weights[(int)LossWeightIndex::RgbDistReg],
        step.loss.weights[(int)LossWeightIndex::DepthDistReg],
        step.loss.weights[(int)LossWeightIndex::NormalDistReg]);
    if (dist_has_rgb(distortion)) {
        a.add("distortions.rgb", bytes(face, 12));
        a.add(PoolSlot::EngVRgbDist, bytes(face, 12));
    }
    if (dist_has_depth(distortion)) {
        a.add("distortions.depth", bytes(face, 4));
        a.add(PoolSlot::EngVDepthDist, bytes(face, 4));
    }
    if (step.loss.compute_loss_map) {
        a.add(PoolSlot::EngLossMap, bytes(face, 4));
        a.add(PoolSlot::PplLossMapScale, bytes(face, 4));
        if (step.loss.loss_map_normalize || step.loss.loss_map_clip_quantile < 1 ||
            step.loss.loss_map_power != 1) {
            a.add(PoolSlot::DensifyMapNorm, sat_mul(cameras, 8));
            a.add(PoolSlot::DensifyQuantileTemp, sat_mul(cameras, 4096));
        }
        if (step.loss.loss_map_mode == (int)DensifyLossMapMode::RobustEdgeAware) {
            a.add(PoolSlot::DensifyRobustResid, bytes(face, 4));
            a.add(PoolSlot::DensifyTukeyC, sat_mul(cameras, 4));
            a.add(PoolSlot::DensifyQuantileTemp, sat_mul(cameras, 4096));
        }
    }
    if (mask.pixels() || step.loss.saturation_threshold > 0) {
        a.add(PoolSlot::SsimMaskWeight, bytes(face, 4));
        a.add(PoolSlot::SsimMaskWeightTmp, bytes(face, 4));
    }
    for (PoolSlot slot : {PoolSlot::EngVLosses, PoolSlot::PplLosses, PoolSlot::PplTotalLosses})
        a.add(slot, sizeof(float) * (int)LossIndex::length);
    for (PoolSlot slot : {PoolSlot::PplRawLosses, PoolSlot::PplVRawLosses})
        a.add(slot, sat_mul(sat_add(cameras, 1), sizeof(float) * (int)RawLossIndex::length));
    a.add(PoolSlot::SsimScalar, sizeof(float));

    const int scales = resolve_loss_scales(step.loss.num_loss_scales,
        step.loss.loss_scale_min_pixels, face.w, face.h);
    if (scales < 1 || scales > 4)
        throw std::runtime_error("training memory estimate: loss scale count must be in [1, 4]");
    struct Pyramid {
        const char* input;
        const char* grad;
        MemoryExtent extent;
        uint64_t channels;
    };
    Pyramid pyramids[] = {
        {"rrgb", "vrgb", face, 3}, {"frgb", nullptr, face, 3},
        {"rd", "vrd", face, 1}, {"rT", "vrT", face, 1},
        {"dn", "vdn", derived_normal ? face : MemoryExtent{}, 3},
        {"fn", state.bilagrid_normal_init ? "vfn" : nullptr, normal, 3},
        {"fd", state.bilagrid_depth_init ? "vfd" : nullptr, depth, 1},
        {"rgbd", "vrgbd", dist_has_rgb(distortion) ? face : MemoryExtent{}, 3},
        {"dd", "vdd", dist_has_depth(distortion) ? face : MemoryExtent{}, 1},
    };
    MemoryExtent mask_level = mask, face_level = face;
    for (int scale = 0; scale < scales; ++scale) {
        const std::string suffix = std::to_string(scale);
        if (scale > 0) {
            for (auto& pyramid : pyramids) {
                if (pyramid.extent.pixels() == 0) continue;
                pyramid.extent = pyramid.extent.half();
                const uint64_t n = bytes(pyramid.extent, 4 * pyramid.channels);
                a.add("ppl.s" + suffix + "." + pyramid.input, n);
                if (pyramid.grad) a.add("ppl.g." + std::string(pyramid.grad) + ".s" + suffix, n);
            }
            if (mask_level.pixels()) {
                mask_level = mask_level.half();
                a.add("ppl.s" + suffix + ".ra", sat_mul(ceil_div(bytes(mask_level, 1), 4), 4));
            }
            face_level = face_level.half();
        }
        if (step.loss.compute_loss_map && cfg.densify_loss_map_mode.find("_nms") != std::string::npos &&
            step.loss.nms_falloff < 1)
            a.add("ppl.nms.s" + suffix, sat_mul(ceil_div(bytes(face_level, 1), 4), 4));
    }
}

}  // namespace

TrainingBatchPlan resolve_training_batch_plan(int64_t num_train,
                                              int64_t num_val,
                                              int max_batch_per_epoch) {
    TrainingBatchPlan out;
    double batch = std::max(
        (double)num_train / std::max(max_batch_per_epoch, 1), 1.0);
    out.train_batch_size = std::max(1, (int)(batch + 0.5));
    if (num_val > 0 && num_train > 0)
        out.val_batch_size = std::max(
            1, (int)std::ceil(batch * (double)num_val / (double)num_train));
    return out;
}

TrainingMemoryEstimate estimate_training_memory(
    const ParsedDataset& ds, const PostSplitCameras& post,
    const TrainConfig& cfg, bool has_mask, bool has_depth, bool has_normal,
    int64_t target_splats, int max_faces_per_pass) {
    std::vector<int32_t> train = ds.train_indices;
    if (train.empty()) {
        train.resize((size_t)ds.num_cameras);
        std::iota(train.begin(), train.end(), 0);
    }
    const TrainingBatchPlan batch = resolve_training_batch_plan(
        (int64_t)train.size(), (int64_t)ds.val_indices.size(), cfg.max_batch_per_epoch);
    const auto groups = memory_groups(ds, post, train, max_faces_per_pass,
        has_mask || post.any_fov_mask, has_depth, has_normal);
    const auto val_groups = memory_groups(ds, post, ds.val_indices, max_faces_per_pass,
        has_mask || post.any_fov_mask, has_depth, has_normal);
    int max_passes = 1;
    for (const auto* side : {&groups, &val_groups})
        for (const auto& [key, group] : *side)
            max_passes = std::max(max_passes, (int)group.passes.size());
    const int64_t max_input_batch = std::min<int64_t>(batch.train_batch_size, train.size());
    const bool fused = resolve_fused_proj_bwd_optim(
        cfg.use_fused_proj_bwd_optim, cfg.split_batch, max_input_batch, max_passes);
    const ColorResolution color = resolve_color(cfg);
    const RunState state = resolve_appearance_state(cfg, has_depth, has_normal);
    const EngineStepConfig step = build_step_config(cfg, state, std::max(cfg.num_iterations, 1));
    MemoryAllocations a;
    uint64_t max_step_cameras = 0, nonwarp_inputs = 0;
    bool split = false;
    for (const auto& [key, group] : groups) {
        const uint64_t B = (uint64_t)std::min<int64_t>(group.count, batch.train_batch_size);
        max_step_cameras = std::max(max_step_cameras, sat_mul(B, group.faces));
        if (!group.warped) nonwarp_inputs = sat_add(nonwarp_inputs, group.count);
        const bool split_group = (cfg.split_batch && !fused) || group.passes.size() > 1;
        split = split || (split_group && sat_mul(B, group.passes.size()) > 1);
        for (const auto& pass : group.passes)
            count_image_pass(a, group, pass, split_group ? 1 : B,
                             true, cfg, state, step, color);
    }
    max_step_cameras = std::max(max_step_cameras,
        std::min<uint64_t>(nonwarp_inputs, (uint64_t)batch.train_batch_size));
    split = split || (groups.size() > 1 && nonwarp_inputs > 1 && max_input_batch > 1);
    if (cfg.eval_mode != "all") {
        const ParsedDataset eval = parse_dataset(cfg.data, parser_config(cfg, true), cfg.data_format);
        const PostSplitCameras eval_post = bake_post_split(eval, cfg.warp_to_pinhole,
            cfg.warp_spherical_to_pinhole, WarpFaceFit::Uniform, cfg.warp_back_face);
        std::vector<int32_t> indices((size_t)eval.num_cameras);
        std::iota(indices.begin(), indices.end(), 0);
        const auto eval_groups = memory_groups(eval, eval_post, indices, 1,
            (!eval.mask_filenames.empty() && cfg.load_masks) || eval_post.any_fov_mask, false, false);
        for (const auto& [key, group] : eval_groups)
            for (const auto& pass : group.passes)
                count_image_pass(a, group, pass, 1, false, cfg, state, step, color);
    }

    const uint64_t N = (uint64_t)std::max<int64_t>(target_splats, 0);
    const uint64_t K = (uint64_t)std::max<int64_t>(0,
        ((int64_t)cfg.sh_degree + 1) * ((int64_t)cfg.sh_degree + 1) - 1);
    const uint64_t cells = sat_mul(N, sat_mul(K, 3));
    const uint64_t splat_bounds = ceil_div(N, 256);
    uint64_t sh_cells = cells;
    if (fused && K != 0) {
        const uint64_t padded = sat_mul(sat_mul(splat_bounds, 256),
            sat_mul(ceil_div(sat_mul(K, 3), 2), 2));
        sh_cells = padded <= INT64_MAX ? (uint64_t)sh_fpbo_cells((int64_t)N, (uint32_t)K) : UINT64_MAX;
    }
    const bool quant = step.optim.sh_value_bits != 32;
    const uint64_t startup_fp32_sh_bytes = quant ? sat_mul(cells, sizeof(float)) : 0;
    const bool gut = cfg.primitive == "3dgut";
    struct Attribute {
        PoolSlot world, g1, g2, adam, grad, grad_q;
        uint64_t channels;
    };
    const Attribute attrs[] = {
        {PoolSlot::WorldMeans, PoolSlot::EngG1Means, PoolSlot::EngG2Means,
         PoolSlot::EngMeansQfpbo, PoolSlot::EngVMeans, PoolSlot::EngVMeansQ, 3},
        {PoolSlot::WorldQuats, PoolSlot::EngG1Quats, PoolSlot::EngG2Quats,
         PoolSlot::EngQuatsQfpbo, PoolSlot::EngVQuats, PoolSlot::EngVQuatsQ, 4},
        {PoolSlot::WorldScales, PoolSlot::EngG1Scales, PoolSlot::EngG2Scales,
         PoolSlot::EngScalesQfpbo, PoolSlot::EngVScales, PoolSlot::EngVScalesQ, 3},
        {PoolSlot::WorldOpacities, PoolSlot::EngG1Opacities, PoolSlot::EngG2Opacities,
         PoolSlot::EngOpacitiesQfpbo, PoolSlot::EngVOpacities, PoolSlot::EngVOpacitiesQ, 1},
        {PoolSlot::WorldFeaturesDc, PoolSlot::EngG1FeaturesDc, PoolSlot::EngG2FeaturesDc,
         PoolSlot::EngFeaturesDcQfpbo, PoolSlot::EngVFeaturesDc, PoolSlot::EngVFeaturesDcQ, 3}
    };
    for (size_t i = 0; i < std::size(attrs); ++i) {
        const auto& attr = attrs[i];
        const uint64_t n = sat_mul(N, attr.channels);
        a.add(attr.world, sat_mul(n, sizeof(float)));
        if (quant)
            a.quant(attr.adam, n, QuantizedAdamState<16, 256>::kBytesPerCell, splat_bounds, sizeof(float4));
        else {
            a.add(attr.g1, sat_mul(n, sizeof(float)));
            a.add(attr.g2, sat_mul(n, sizeof(float)));
        }
        if (gut && i < 3) a.add(attr.grad, sat_mul(n, sizeof(float)));
        else if (!fused) {
            if (quant)
                a.quant(attr.grad_q, n, QuantizedTensor<16, 256>::kBytesPerCell, splat_bounds, sizeof(float2));
            else a.add(attr.grad, sat_mul(n, sizeof(float)));
        }
    }
    if (quant) {
        const uint64_t bounds = fused ? splat_bounds : ceil_div(cells, 256);
        a.quant(fused ? PoolSlot::EngWorldShVq16Fpbo : PoolSlot::EngWorldShVq16,
                sh_cells, QuantizedTensor<16, 256>::kBytesPerCell, bounds, sizeof(float2));
        a.quant(fused ? PoolSlot::EngShQuantFpbo : PoolSlot::EngShQuant,
                sh_cells, QuantizedAdamState<8, 256>::kBytesPerCell, bounds, sizeof(float4));
        if (!fused)
            a.quant(PoolSlot::EngVFeaturesShQ, cells, QuantizedTensor<8, 256>::kBytesPerCell,
                    splat_bounds, sizeof(float2));
    } else {
        for (PoolSlot slot : {PoolSlot::WorldFeaturesSh, PoolSlot::EngG1FeaturesSh, PoolSlot::EngG2FeaturesSh})
            a.add(slot, sat_mul(cells, sizeof(float)));
        if (!fused) a.add(PoolSlot::EngVFeaturesSh, sat_mul(cells, sizeof(float)));
    }
    a.add(PoolSlot::EngRadii, sat_mul(N, 4));
    a.add(PoolSlot::EngAccumBuffer, sat_mul(N, 8));
    if (step.optim.use_per_splat_bias_correction) a.add(PoolSlot::EngBiasCorrectionSteps, sat_mul(N, 4));
    if (step.optim.write_densify_world_grad_score) a.add(PoolSlot::EngDensifyWorldGradScore, sat_mul(N, 4));
    if (step.loss.compute_loss_map) {
        const uint64_t lanes = accum_lanes((DensifyAccumMode)step.loss.loss_map_accum_mode);
        a.add(PoolSlot::RasterBwdAccumWeight, sat_mul(N, 4 * lanes));
        if (split) a.add(PoolSlot::EngSubbatchAccumWeightSum, sat_mul(N, 4 * lanes));
    } else if (split) {
        a.add(PoolSlot::EngSubbatchAccumWeightSum, sat_mul(N, 4));
    }
    if (cfg.packed && (fused || quant))
        a.add(PoolSlot::FusedProjBwdCamBounds, sat_mul(sat_add(N, 1), 4));
    if (cfg.use_revised_densification && cfg.refine_start_iter < cfg.num_iterations) {
        const uint64_t n4 = sat_mul(N, 4);
        a.add(PoolSlot::DensifyRelocMask, N);
        a.add(PoolSlot::DensifyRelocCount, 4);
        for (PoolSlot slot : {
                 PoolSlot::DensifyRelocDstIndices,
                 PoolSlot::DensifyWswrSortingValues,
                 PoolSlot::DensifyWswrOutIdx,
                 PoolSlot::DensifyWswrKeysOut,
                 PoolSlot::DensifyWswrIndicesIn,
                 PoolSlot::DensifyWswrIndicesOut})
            a.add(slot, n4);
        if (cfg.densify_score_clip_quantile > 0 &&
            cfg.densify_score_clip_quantile < 1) {
            a.add(PoolSlot::DensifyScoreGather, n4);
            a.add(PoolSlot::DensifyScoreClip, 4);
            a.add(PoolSlot::DensifyQuantileTemp, 4096);
        }
        if (cfg.densify_oversize_split_fraction > 0 && std::isfinite(cfg.max_screen_size)) {
            a.add(PoolSlot::EngDensifyOversize, n4);
            a.add(PoolSlot::EngDensifyOversizeWeight, sat_mul(N, 8));
        }
        if (cfg.densify_final_score_power != 1)
            a.add(PoolSlot::EngDensifySampleScore, sat_mul(N, 8));
    }

    const uint64_t images = (uint64_t)std::max<int64_t>(post.n_post, 0);
    struct Grid {
        bool enabled;
        const TrainVec3i& shape;
        uint64_t channels;
        PoolSlot values, packed, adagrad, accum, adam, g1, g2, image_grad;
    };
    const Grid grids[] = {
        {state.bilagrid_rgb_init, cfg.bilagrid_shape, cfg.bilagrid_type == "affine" ? 12u : 9u,
         PoolSlot::EngBgRgbGrids, PoolSlot::EngBgRgbGridsQ, PoolSlot::EngBgRgbBgAg, PoolSlot::EngBgRgbAccum,
         PoolSlot::EngBgRgbBgQuant, PoolSlot::EngBgRgbG1, PoolSlot::EngBgRgbG2, PoolSlot::EngBgRgbImageGrad},
        {state.bilagrid_depth_init, cfg.bilagrid_shape_geometry, 2,
         PoolSlot::EngBgDepthGrids, PoolSlot::EngBgDepthGridsQ, PoolSlot::EngBgDepthBgAg, PoolSlot::EngBgDepthAccum,
         PoolSlot::EngBgDepthBgQuant, PoolSlot::EngBgDepthG1, PoolSlot::EngBgDepthG2, PoolSlot::EngBgDepthImageGrad},
        {state.bilagrid_normal_init, cfg.bilagrid_shape_geometry, 3,
         PoolSlot::EngBgNormalGrids, PoolSlot::EngBgNormalGridsQ, PoolSlot::EngBgNormalBgAg, PoolSlot::EngBgNormalAccum,
         PoolSlot::EngBgNormalBgQuant, PoolSlot::EngBgNormalG1, PoolSlot::EngBgNormalG2, PoolSlot::EngBgNormalImageGrad}
    };
    for (const auto& grid : grids) {
        if (!grid.enabled) continue;
        uint64_t per_image = grid.channels;
        for (int dim : grid.shape) per_image = sat_mul(per_image, (uint64_t)std::max(dim, 0));
        const uint64_t n = sat_mul(images, per_image), bounds = ceil_div(n, 256);
        if (quant) {
            a.quant(grid.packed, n, QuantizedTensor<16, 256>::kBytesPerCell, bounds, sizeof(float2));
            a.transient = std::max(a.transient, sat_mul(n, 4));
            if (cfg.use_adagrad_bilagrid_optim)
                a.quant(grid.adagrad, n, QuantizedTensorLog<8, 256>::packed_bytes_for(1), bounds, sizeof(float2));
            else a.quant(grid.adam, n, QuantizedAdamState<8, 256>::kBytesPerCell, bounds, sizeof(float4));
        } else {
            a.add(grid.values, sat_mul(n, 4));
            if (cfg.use_adagrad_bilagrid_optim) a.add(grid.accum, sat_mul(n, 4));
            else {
                a.add(grid.g1, sat_mul(n, 4));
                a.add(grid.g2, sat_mul(n, 4));
            }
        }
        a.add(grid.image_grad, sat_mul(sat_mul(max_step_cameras, per_image), 4));
    }
    if (state.bilagrid_rgb_init || state.bilagrid_depth_init || state.bilagrid_normal_init) {
        a.add(PoolSlot::EngBgTvReadout, 12);
        if (split) a.add(PoolSlot::EngBgSplitCamIndices, sat_mul(max_step_cameras, 4));
    }
    if (state.bilagrid_depth_init) a.add(PoolSlot::EngBgDepthScalars, sat_mul(images, 4));
    if (state.ppisp_init) {
        const auto spec = ppisp_param_spec(cfg.ppisp_param_type);
        const uint64_t params = sat_mul(sat_mul(images, spec.num_params), 4);
        a.add(PoolSlot::EngPpispParams, params);
        a.add(PoolSlot::EngPpispGrads, params);
        if (cfg.use_adagrad_ppisp_optim) a.add(PoolSlot::EngPpispAccum, params);
        else {
            a.add(PoolSlot::EngPpispG1, params);
            a.add(PoolSlot::EngPpispG2, params);
        }
        if (std::any_of(step.ppisp.reg_weights.begin(), step.ppisp.reg_weights.end(),
                        [](float weight) { return weight > 0; })) {
            a.add(PoolSlot::EngPpispVRegParams, params);
            a.add(PoolSlot::EngPpispRegRawLosses, sat_mul(sat_add(images, 1), 4 * spec.num_raw_losses));
            a.add(PoolSlot::EngPpispRegLosses, 4 * (int)PPISPRegLossIndex::length);
            a.add(PoolSlot::EngPpispVRegLosses, 4 * (int)PPISPRegLossIndex::length);
            a.add(PoolSlot::PpispVRawLosses, 4 * spec.num_raw_losses);
        }
    }
    if (step.loss.color_shift_reg_weight > 0) {
        a.add(PoolSlot::EngColorShiftRegEma, 12);
        a.add(PoolSlot::EngColorShiftRegBatchSum, 12);
    }
    if (color.splat_on()) a.add(PoolSlot::ColorSpaceSplatMatrix, 36);
    if (color.image_on()) a.add(PoolSlot::ColorSpaceImageMatrix, 36);
    if (cfg.background_mode == "sh") {
        const uint64_t n = sat_mul((uint64_t)cfg.background_sh_degree + 1,
                                  (uint64_t)cfg.background_sh_degree + 1);
        for (PoolSlot slot : {PoolSlot::EngBgSkyShCoeffs, PoolSlot::EngBgSkyG1,
                              PoolSlot::EngBgSkyG2, PoolSlot::EngBgSkyVSh})
            a.add(slot, sat_mul(n, 12));
    }
    a.transient = sat_add(a.transient, startup_fp32_sh_bytes);
    return a.result(fused);
}

namespace {

uint64_t effective_memory_allowance(const backend::BudgetSnapshot& budget,
                                    uint64_t app_limit, uint64_t reserve) {
    if (budget.status != backend::BudgetStatus::Available) return 0;
    uint64_t driver = budget.available_bytes > reserve
        ? budget.available_bytes - reserve : 0;
    if (app_limit == 0) return driver;
    uint64_t used = sat_add(budget.process_bytes, budget.reserved_bytes);
    uint64_t app = app_limit > used ? app_limit - used : 0;
    return std::min(driver, app);
}

}  // namespace

int resolve_eval_worker_count(int64_t view_pixels, unsigned hardware_threads,
                              bool save_images) {
    constexpr uint64_t budget = 4ull << 30;
    constexpr uint64_t active_worker = 139;
    constexpr uint64_t queued_pair = 24;
    constexpr uint64_t producer_pair = 24;
    constexpr uint64_t png_allowance = 24;
    const uint64_t per_pixel = budget / (uint64_t)std::max<int64_t>(view_pixels, 1);
    // A full queue still leaves one just-read pair owned by the producer.
    const uint64_t available = per_pixel > producer_pair ? per_pixel - producer_pair : 0;
    const uint64_t memory_workers = available /
        (active_worker + queued_pair + (save_images ? png_allowance : 0));
    const uint64_t hardware_workers = std::max(1u, hardware_threads / 4u);
    return (int)std::max<uint64_t>(1, std::min<uint64_t>(
        8, std::min(memory_workers, hardware_workers)));
}

std::optional<backend::BudgetFailure> training_memory_refusal(
    const backend::BudgetSnapshot& budget,
    const TrainingMemoryEstimate& estimate, uint64_t app_limit,
    uint64_t reserve) {
    backend::BudgetFailure f{};
    f.requested_bytes = estimate.estimated_bytes();
    f.device_index = budget.device_index;
    f.heap_index = budget.heap_index;
    if (budget.status != backend::BudgetStatus::Available) {
        f.kind = budget.status == backend::BudgetStatus::Unavailable
            ? backend::BudgetFailureKind::TelemetryUnavailable
            : backend::BudgetFailureKind::TelemetryError;
        return f;
    }

    uint64_t available =
        effective_memory_allowance(budget, app_limit, reserve);
    if (estimate.estimated_bytes() <= available) return std::nullopt;
    uint64_t driver = budget.available_bytes > reserve
        ? budget.available_bytes - reserve : 0;
    uint64_t app = UINT64_MAX;
    if (app_limit > 0) {
        uint64_t used = sat_add(budget.process_bytes, budget.reserved_bytes);
        app = app_limit > used ? app_limit - used : 0;
    }
    f.kind = app <= driver
        ? backend::BudgetFailureKind::ApplicationLimit
        : backend::BudgetFailureKind::DriverHeadroom;
    f.available_bytes = available;
    return f;
}

std::string budget_failure_message(const backend::BudgetFailure& failure) {
    switch (failure.kind) {
        case backend::BudgetFailureKind::DriverHeadroom:
            return lfmt(lmsg::driver_memory_refusal,
                        {backend::_fmt_bytes(failure.requested_bytes),
                         backend::_fmt_bytes(failure.available_bytes)});
        case backend::BudgetFailureKind::ApplicationLimit:
            return lfmt(lmsg::app_memory_refusal,
                        {backend::_fmt_bytes(failure.requested_bytes),
                         backend::_fmt_bytes(failure.available_bytes)});
        case backend::BudgetFailureKind::TelemetryUnavailable:
            return lmsg::memory_telemetry_unavailable.get();
        case backend::BudgetFailureKind::TelemetryError:
            return lmsg::memory_telemetry_error.get();
    }
    return lmsg::memory_telemetry_error.get();
}

namespace {

uint64_t configured_memory_limit(const TrainConfig& cfg) {
    if (!cfg.memory_limit_gib.has_value()) return 0;
    double bytes = (double)*cfg.memory_limit_gib * (double)(1ull << 30);
    return bytes >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)std::max(1.0, bytes);
}

void enforce_preflight(const backend::BudgetSnapshot& budget,
                       const TrainingMemoryEstimate& estimate,
                       uint64_t app_limit) {
    if (auto failure = training_memory_refusal(
            budget, estimate, app_limit, kTrainingMemoryReserveBytes))
        throw backend::BudgetError(*failure);
}

}  // namespace

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
    if (!backend::device_prepare()) {
        backend::BudgetFailure failure{};
        failure.kind = backend::BudgetFailureKind::TelemetryError;
        failure.device_index = backend::device_current();
        throw backend::BudgetError(failure);
    }

    uint64_t app_limit = configured_memory_limit(cfg);
    int max_faces_per_pass = cfg.split_batch ? 1 : 0;
    const int64_t cap = resolve_training_splat_capacity(ds.points.num(), cfg);

    memory_budget = backend::budget_snapshot();
    memory_allowance = effective_memory_allowance(
        memory_budget, app_limit, kTrainingMemoryReserveBytes);
    memory_estimate = estimate_training_memory(
        ds, post, cfg, has_mask, has_depth, has_normal, cap,
        max_faces_per_pass);
    enforce_preflight(memory_budget, memory_estimate, app_limit);

    ColorResolution color = resolve_color(cfg);
    SeedSplats seed = seed_splats(ds.points, cfg, color, cap);
    memory_budget = backend::budget_snapshot();
    memory_allowance = effective_memory_allowance(
        memory_budget, app_limit, kTrainingMemoryReserveBytes);
    enforce_preflight(memory_budget, memory_estimate, app_limit);

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
    if (write_config_json) {
        save_config_json(cfg, out_dir, preset);
        save_scene_transform_json(ds, cfg, out_dir);
    }
    log(lfmt(lmsg::output_directory, {fs::absolute(out_dir).string()}));

    backend::training_budget_begin(kTrainingMemoryReserveBytes, app_limit);
    _budget_active = true;
    memory_budget = backend::budget_snapshot();
    try {
    engine_reset();
    _engine_initialized = true;

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

    // Binning granularity for the splat-tile intersection (0 = automatic).
    engine_set_bin_tile_size(cfg.bin_tile_size);

    // Background blending.
    if (cfg.background_mode == "noise")
        engine_init_background_noise((int)color.splat_transfer,
                                     color.splat_linear);
    else if (cfg.background_mode == "pseudorandom")
        engine_init_background_pseudorandom((int)color.splat_transfer,
                                            color.splat_linear);
    else if (cfg.background_mode == "sh")
        engine_init_background_sh(cfg.background_sh_degree,
                                  (int)color.splat_transfer,
                                  color.splat_linear);

    // Output transfer / wide-gamut color space.
    const bool splat_cs_on = color.splat_on();
    const bool image_cs_on = color.image_on();
    // PPISP ahead of the conversion leaves the bilagrid on the display side,
    // where it belongs, so the two order flags cannot disagree.
    if (splat_cs_on && cfg.apply_ppisp_before_color_space &&
        !cfg.apply_ppisp_before_bilagrid)
        throw std::runtime_error(lfmt(lmsg::ppisp_before_color_space_order,
                                      {"--apply-ppisp-before-color-space",
                                       "--apply-ppisp-before-bilagrid"}));
    {
        auto vec = [](const Mat3f& m) { return std::vector<float>(m.begin(), m.end()); };
        engine_init_color_space(
            splat_cs_on, (int)color.splat_transfer, color.splat_linear,
            splat_cs_on ? vec(gamut_to_rec709(color.splat_gamut)) : std::vector<float>{},
            image_cs_on, (int)color.image_transfer, color.image_linear,
            image_cs_on ? vec(gamut_to_rec709(color.image_gamut)) : std::vector<float>{});
    }

    // ---- DataManager ---------------------------------------------------
    const int64_t N = ds.num_cameras;
    int64_t num_val = (int64_t)ds.val_indices.size();
    int64_t num_train = N - num_val;
    TrainingBatchPlan batch = resolve_training_batch_plan(
        num_train, num_val, cfg.max_batch_per_epoch);

    DataManagerConfig dm;
    dm.cache_mode  = (cfg.cache_images == "disk") ? CacheMode::DISK : CacheMode::CPU;
    // A split needs a mask even when none is on disk: the synthesized
    // all-white one becomes the post-split FOV mask (0 past the lens),
    // without which the unseen face regions train as black.
    dm.load_masks  = has_mask || post.any_fov_mask;
    dm.load_depths      = has_depth;
    dm.load_normals     = has_normal;
    dm.train_batch_size = batch.train_batch_size;
    dm.val_batch_size   = batch.val_batch_size;
    dm.max_faces_per_pass = max_faces_per_pass;
    dm.flip_mask = cfg.flip_mask;
    dm.mask_boundary_offset = cfg.mask_boundary_offset;
    dm.exif_quarter_turns = ds.exif_quarter_turns;
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
    st = resolve_appearance_state(cfg, has_depth, has_normal);
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

    if (st.bilagrid_rgb_init) {
        // bilagrid_shape is (X, Y, W) -> engine (L=W, H=Y, W=X).
        engine_init_bilagrid_rgb(n_grids, cfg.bilagrid_type,
                                 cfg.bilagrid_shape[2], cfg.bilagrid_shape[1],
                                 cfg.bilagrid_shape[0],
                                 optim_bits, value_bits,
                                 cfg.use_adagrad_bilagrid_optim);
    }
    if (st.bilagrid_depth_init) {
        engine_init_bilagrid_depth(n_grids,
                                   cfg.bilagrid_shape_geometry[2],
                                   cfg.bilagrid_shape_geometry[1],
                                   cfg.bilagrid_shape_geometry[0],
                                   optim_bits, value_bits,
                                   cfg.use_adagrad_bilagrid_optim);
    }
    if (st.bilagrid_normal_init) {
        engine_init_bilagrid_normal(n_grids,
                                    cfg.bilagrid_shape_geometry[2],
                                    cfg.bilagrid_shape_geometry[1],
                                    cfg.bilagrid_shape_geometry[0],
                                    optim_bits, value_bits,
                                    cfg.use_adagrad_bilagrid_optim);
    }
    if (st.ppisp_init) {
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
    }

    // ---- Resume --------------------------------------------------------
    // Last, because engine_load_checkpoint() overwrites the skeleton just
    // built: the world must already be allocated at max_num_splats and every
    // appearance channel the checkpoint carries must already exist as a
    // restore target, which is what everything above establishes.
    if (!cfg.resume.empty()) restore_checkpoint();
    } catch (...) {
        engine_reset();
        _engine_initialized = false;
        backend::training_budget_end();
        _budget_active = false;
        throw;
    }
}

void TrainerSession::reset_engine() {
    std::lock_guard<std::mutex> lock(engine_mutex);
    if (_engine_initialized) {
        engine_reset();
        _engine_initialized = false;
    }
    if (_budget_active) {
        backend::training_budget_end();
        _budget_active = false;
    }
}

void TrainerSession::release_engine_budget() {
    std::lock_guard<std::mutex> lock(engine_mutex);
    if (_budget_active) {
        backend::training_budget_end();
        _budget_active = false;
    }
}

// Restore engine state from cfg.resume, adapting the checkpoint's buffers on
// the host first when its layout differs from the one just built (fewer
// splats, different SH degree, bilagrid/PPISP added or dropped).
void TrainerSession::restore_checkpoint() {
    ckpt::ResolvedCheckpoint r = ckpt::resolve_checkpoint(cfg.resume);
    ckpt::check_resumable(r.ckpt_dir);

    // Channel presence is resolved during setup, not from config flags alone.
    ckpt::TargetLayout target;
    target.max_num_splats = engine_get_max_num_splats();
    target.num_sh         = (cfg.sh_degree + 1) * (cfg.sh_degree + 1) - 1;
    target.num_images     = (int)post.n_post;
    // The live engine flag is unset until a step; restore must use the resolved layout.
    target.fused_proj_bwd_optim = memory_estimate.fused_proj_bwd_optim;
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
    auto losses = engine_train_step_managed(
        step, cfg.num_iterations, cfg.primitive, sh_degree_to_use,
        cfg.packed || cfg.use_bvh, sc);

    // Sticky and returns-and-clears, and nothing else on the training thread
    // reads it: a failed dispatch or copy would otherwise leave a buffer
    // unwritten and training would carry on over whatever was in it.
    if (const char* err = backend::last_error())
        throw std::runtime_error("GPU backend error at step " +
                                 std::to_string(step) + ": " + err);

    // Divergence never recovers, and the run otherwise continues in silence to
    // a black render and a checkpoint with zero splats. Reported values sit
    // well under 10, so 1e3 is clear of anything legitimate.
    if (!_diverged_loss_reported) {
        for (const auto& [name, value] : losses) {
            if (name == "cur_num_splats" || name == "max_num_splats" ||
                name == "num_added")
                continue;
            // Magnitude only for rgb_loss: the others carry scene-dependent
            // units (depth, TV) with no comparable ceiling.
            const bool huge = name == "rgb_loss" && std::fabs(value) > 1e3f;
            if (std::isfinite(value) && !huge) continue;
            _diverged_loss_reported = true;
            log(lfmt(lmsg::warn_diverged_loss,
                     {(long long)step, name, (double)value}));
            break;
        }
    }
    return losses;
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
        // Clock starts before the yield: a render the trainer stood aside for
        // is time this step took. Timing only the work below reported 6 ms on
        // a step the run was actually spending 24 ms on.
        auto step_start = std::chrono::steady_clock::now();
        while (render_pending.load())
            std::this_thread::sleep_for(std::chrono::microseconds(500));

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
    engine_profile_capture_vram();

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
    vc.color_space_on = color.splat_on();
    vc.centers = dsparse::scene_centers(ds);
    vc.center_cameras = ds.num_cameras > 0;
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
    const DatasetParserConfig pcfg = parser_config(cfg, true);

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
    // Uniform faces keep metrics comparable across training fits.
    PostSplitCameras epost = bake_post_split(
        eds, cfg.warp_to_pinhole, cfg.warp_spherical_to_pinhole,
        WarpFaceFit::Uniform, cfg.warp_back_face);

    DataManagerConfig dm;
    dm.cache_mode  = (cfg.cache_images == "disk") ? CacheMode::DISK : CacheMode::CPU;
    const bool eval_masks = !eds.mask_filenames.empty() && cfg.load_masks;
    dm.load_masks  = eval_masks || epost.any_fov_mask;
    dm.load_depths = false;
    dm.load_normals = false;
    dm.train_batch_size = 1;
    dm.val_batch_size   = 1;
    dm.max_faces_per_pass = 1;
    dm.flip_mask = cfg.flip_mask;
    dm.mask_boundary_offset = cfg.mask_boundary_offset;
    dm.exif_quarter_turns = eds.exif_quarter_turns;
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

    // Only host scoring is concurrent; every forward uses the singleton engine.
    struct ViewJob {
        int64_t index = 0;
        int H = 0, W = 0, C = 0;
        std::vector<float> gt, pred;
    };
    struct ViewScore {
        bool  filled = false;
        float l1 = 0, psnr = 0, ssim = 0, cc_l1 = 0, cc_psnr = 0, cc_ssim = 0;
    };

    const auto& eval_widths = epost.post_widths.empty() ? eds.widths : epost.post_widths;
    const auto& eval_heights = epost.post_heights.empty() ? eds.heights : epost.post_heights;
    int64_t view_pixels = eval_widths.empty() ? (int64_t)1 << 20 : 0;
    for (size_t i = 0; i < eval_widths.size(); ++i)
        view_pixels = std::max(view_pixels, (int64_t)eval_widths[i] * eval_heights[i]);
    const int n_workers = resolve_eval_worker_count(
        view_pixels, std::thread::hardware_concurrency(), cfg.save_eval_images);

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
                const std::string path = (out_dir / nm).string();
                if (!stbi_write_png(path.c_str(), j.W, j.H, j.C,
                                    bytes.data(), j.W * j.C))
                    throw std::runtime_error("cannot write eval image: " + path);
            };
            // Both sides, because the LPIPS tool needs the pair -- and the
            // 8-bit clip here is what it will score, so its numbers are
            // reproducible from the files alone.
            png("gt", j.gt);
            png("render", j.pred);
        }
    };

    std::vector<std::thread> workers;
    std::exception_ptr producer_error;
    auto worker_failed = [&] {
        std::lock_guard<std::mutex> lk(qmu);
        return worker_error != nullptr;
    };

    try {
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
                        qcv.wait(lk, [&] { return worker_error || !queue.empty() || !producing; });
                        if (worker_error || queue.empty()) return;
                        job = std::move(queue.front());
                        queue.pop_front();
                    }
                    spacecv.notify_one();
                    try {
                        score_view(job);
                    } catch (...) {
                        {
                            std::lock_guard<std::mutex> lk(qmu);
                            if (!worker_error) worker_error = std::current_exception();
                        }
                        spacecv.notify_all();
                        qcv.notify_all();
                        return;
                    }
                }
            });
        }

        const int sh_deg = cfg.sh_degree;
        int64_t next_slot = 0;

        while (!worker_failed()) {
            ViewJob job;
            job.index = next_slot;
            {
                std::lock_guard<std::mutex> lk(engine_mutex);
                const int n_view = engine_eval_forward(cfg.primitive, sh_deg, cfg.packed);
                if (n_view == 0) break;
                const auto [B, H, W, C] = engine_get_render_rgb_shape();
                if (n_view != 1 || B != 1 || next_slot >= epost.n_post)
                    throw std::runtime_error("TrainerSession::eval: unexpected face count");
                job.H = (int)H; job.W = (int)W; job.C = (int)C;
                const int64_t npx = H * W * C;
                job.pred.resize((size_t)npx);
                job.gt.resize((size_t)npx);
                engine_copy_render_to_host(
                    TorchTensorView{(uint64_t)(uintptr_t)job.pred.data(), 4, {B, H, W, C}},
                    TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}},
                    TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}});
                engine_copy_gt_rgb_to_host(
                    TorchTensorView{(uint64_t)(uintptr_t)job.gt.data(), 4, {B, H, W, C}});
            }
            for (float& x : job.pred) x = std::min(std::max(x, 0.0f), 1.0f);
            {
                std::unique_lock<std::mutex> lk(qmu);
                spacecv.wait(lk, [&] {
                    return worker_error || (int)queue.size() < n_workers;
                });
                if (worker_error) break;
                queue.push_back(std::move(job));
                ++next_slot;
            }
            qcv.notify_one();
        }

        if (next_slot != (int64_t)epost.n_post && !worker_failed())
            throw std::runtime_error(
                "TrainerSession::eval: rendered " + std::to_string(next_slot) +
                " of " + std::to_string(epost.n_post) + " eval views");
    } catch (...) {
        producer_error = std::current_exception();
    }

    {
        std::lock_guard<std::mutex> lk(qmu);
        producing = false;
    }
    qcv.notify_all();
    for (auto& t : workers) t.join();
    if (producer_error) std::rethrow_exception(producer_error);
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
    // Eval renders at full resolution and can push the pool past its
    // training-time mark, so re-capture over train()'s snapshot.
    engine_profile_capture_vram();
}

}  // namespace spirula
