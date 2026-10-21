// Engine densification step (MCMC + revised relocate/add splits).

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"
#include "core/Env.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>


// Helper: build DeviceVector<T> from TorchTensorView (non-owning view).
template<typename T>
static inline DeviceVector<T> _dv(const TorchTensorView& tv) {
    return DeviceVector<T>(tv);
}
// For null-able TorchTensorViews, return default (null) DeviceVector.
template<typename T>
static inline DeviceVector<T> _dv_or_null(const TorchTensorView& tv) {
    if (std::get<0>(tv) == 0) return DeviceVector<T>();
    return DeviceVector<T>(tv);
}
// For [N, K, C] tensors like features_sh: flatten to [N*K, C] then build DeviceVector.
template<typename T>
static inline DeviceVector<T> _dv_flat(const TorchTensorView& tv) {
    if (std::get<0>(tv) == 0) return DeviceVector<T>();
    const auto& shape = std::get<2>(tv);
    int64_t flat_count = 1;
    for (size_t i = 0; i + 1 < shape.size(); i++) flat_count *= shape[i];
    TorchTensorView flat_tv(std::get<0>(tv), std::get<1>(tv),
        {flat_count, (int64_t)(sizeof(T) / std::get<1>(tv))});
    return DeviceVector<T>(flat_tv);
}


// One damped Richardson-Lucy step of the learned score, when this step's
// raster bwd accumulated the ratio map. The prior follows the stats it leaves
// one step late, through a readout that never waits on the device.
static void _learned_score_step(int64_t num_splats, const DensifyConfig& cfg,
                                DeviceVector<float>& radii,
                                DeviceVector<float2>& accum_buf,
                                DeviceVector<float>& oversize) {
    constexpr float kPriorRate = 0.1f;
    auto& ls = engine().learned_score;
    static AsyncReadout<float> stats_readout(2);
    float mean_log = NAN;
    if (ls.stats_pending) {
        const float* h = stats_readout.read_previous();
        if (h && h[1] > 0.0f) mean_log = h[0] / h[1];
        ls.stats_pending = false;
    }
    const bool ratio_map = ls.ratio_map;
    ls.ratio_map = false;
    if (ratio_map && engine().fwd.accum_mode == DensifyAccumMode::Avg &&
        engine().fwd.accum_weight.data_ptr() != nullptr) {
        // With the prior this step's forward rendered; the new one is next step's.
        ls.stats.resize(PoolSlot::EngLearnedScoreStats, 2);
        ls.stats.zero();
        densify_learned_score_tensor(num_splats, ls.power, cfg.learned_score_rate,
                                     ls.log_prior, cfg.learned_score_pull, radii,
                                     engine().fwd.accum_weight,
                                     accum_buf, cfg.max_screen_size, oversize, ls.stats,
                                     engine().num_sky_splats);
        stats_readout.issue(ls.stats.data_ptr());
        ls.stats_pending = true;
    }
    if (std::isfinite(mean_log)) {
        ls.log_prior = ls.has_prior ? ls.log_prior + kPriorRate * (mean_log - ls.log_prior)
                                    : mean_log;
        ls.has_prior = true;
    }
}


// SS_DENSIFY_SCORE_LOG=1: the share of the draw's weight held by each fifth
// of the splats ranked by world size, smallest first, in English like the
// other deep diagnostics. Large splats are where the background lives.
static void _log_score_by_size(int step, int64_t n) {
    static const bool on = [] {
        const char* v = spirula::env("DENSIFY_SCORE_LOG");
        return v && *v && v[0] != '0';
    }();
    if (!on || n <= 0) return;
    const auto& opt = engine().optim;
    const DeviceVector<float2>& score =
        opt.densify_sample_score.data_ptr() ? opt.densify_sample_score : opt.accum_buffer;
    std::vector<float2> h_score((size_t)n);
    std::vector<float3> h_scale((size_t)n);
    backend::memcpy_sync(h_score.data(), score.data_ptr(), (size_t)n * sizeof(float2),
                         backend::MemcpyKind::DeviceToHost);
    backend::memcpy_sync(h_scale.data(), engine().world.scales.data_ptr(),
                         (size_t)n * sizeof(float3), backend::MemcpyKind::DeviceToHost);
    std::vector<std::pair<float, float>> size_score((size_t)n);
    double total = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        const float3 l = h_scale[(size_t)i];
        const float w = std::isfinite(h_score[(size_t)i].x) ? std::max(h_score[(size_t)i].x, 0.0f) : 0.0f;
        size_score[(size_t)i] = {std::max(l.x, std::max(l.y, l.z)), w};
        total += w;
    }
    std::sort(size_score.begin(), size_score.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    double part[5] = {0, 0, 0, 0, 0}, top1 = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        part[std::min<int64_t>(4, i * 5 / n)] += size_score[(size_t)i].second;
        if (i >= n - std::max<int64_t>(1, n / 100)) top1 += size_score[(size_t)i].second;
    }
    const double t = std::max(total, 1e-30);
    std::vector<float> pos;
    for (const auto& ss : size_score)
        if (ss.second > 0.0f) pos.push_back(ss.second);
    std::sort(pos.begin(), pos.end());
    auto q = [&](double f) {
        return pos.empty() ? 0.0f : pos[std::min(pos.size() - 1, (size_t)(f * (double)pos.size()))];
    };
    std::fprintf(stderr, "[score] step %d: draw weight by size fifth %.1f%% %.1f%% %.1f%% %.1f%% "
                         "%.1f%%, largest 1%% %.1f%%; score p10 %.3g p50 %.3g p90 %.3g "
                         "p99 %.3g, positive %.1f%%\n",
                 step, 100 * part[0] / t, 100 * part[1] / t, 100 * part[2] / t,
                 100 * part[3] / t, 100 * part[4] / t, 100 * top1 / t, q(0.1), q(0.5),
                 q(0.9), q(0.99), 100.0 * (double)pos.size() / (double)n);
}


void engine_max_weight_begin_step(int step, int max_steps, const DensifyConfig& cfg) {
    auto& mw = engine().max_weight;
    const bool track = cfg.use_revised_densification
        && cfg.dead_criterion != DeadCriterion::Opacity && cfg.refine_every > 0
        && step < std::max(cfg.refine_stop_iter, max_steps - cfg.refine_stop_num_iter);
    if (!track) {
        if (mw.value.data_ptr() != nullptr)
            DevicePool::global().free(PoolSlot::EngMaxWeight);
        mw = EngineState::MaxWeight{};
        return;
    }
    if (mw.value.data_ptr() == nullptr || mw.value.size() != engine().max_num_splats) {
        mw.value.resize(PoolSlot::EngMaxWeight, engine().max_num_splats);
        densify_max_weight_init_tensor(engine().cur_num_splats, engine().world.opacities,
                                       mw.value);
        mw.base_step = step;
    }
    mw.half_life = std::max(cfg.max_weight_half_life, 1.0f);
    // Keeps step_scale, and so the stored values, within 2^32.
    if ((float)(step - mw.base_step) > 32.0f * mw.half_life) {
        densify_max_weight_scale_tensor(
            engine().cur_num_splats,
            std::exp2(-(float)(step - mw.base_step) / mw.half_life), mw.value);
        mw.base_step = step;
    }
    mw.step_scale = std::exp2((float)(step - mw.base_step) / mw.half_life);
}


int engine_densify_step(int step, int max_steps, const DensifyConfig& cfg) {
    int64_t cur_num_splats = engine().cur_num_splats;
    int64_t max_num_splats = engine().max_num_splats;
    const int64_t first_trainable = engine().num_sky_splats;
    bool quantize_sh = engine().optim.sh_quantize_enabled();
    const int sh_optim_bits = engine().optim.sh_optim_bits;
    // The 4-bit (joint-nibble) path is wired through both _zero_quant_sh_for_splat
    // and the relocate/MCMC kernels via uchar3 stride; 32 / 8 / 4 are all
    // valid here. Anything else (e.g. 16) would silently mis-stride.
    if (quantize_sh && sh_optim_bits != 8 && sh_optim_bits != 4) {
        throw std::runtime_error(
            "engine_densify_step: SH-optim quantization at "
            + std::to_string(sh_optim_bits)
            + " bits is not supported alongside densification "
              "(only 4-bit and 8-bit are wired through the dst zero-encoding).");
    }
    // SH VALUE-quant: pick the right buffer (cell-block or FPBO layout) for
    // the codec-aware src->dst copy (`_copy_quant_sh_value_for_splat`).
    int  sh_value_bits = engine().world.sh_value_bits;
    DeviceVector<uint8_t> dv_sh_value_packed;
    DeviceVector<float2>  dv_sh_value_bounds;
    bool sh_value_bounds_per_splat = false;
    if (sh_value_bits == 8) {
        if (engine().world.features_sh_quant8_fpbo.initialized()) {
            dv_sh_value_packed = engine().world.features_sh_quant8_fpbo.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant8_fpbo.bounds;
            sh_value_bounds_per_splat = true;
        } else if (engine().world.features_sh_quant8.initialized()) {
            dv_sh_value_packed = engine().world.features_sh_quant8.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant8.bounds;
            sh_value_bounds_per_splat = false;
        }
    } else if (sh_value_bits == 16) {
        if (engine().world.features_sh_quant16_fpbo.initialized()) {
            // QuantizedTensor<16>::packed is uint8_t storage with kBytesPerCell=2,
            // so the DeviceVector<uint8_t> view passed to the densify kernel
            // covers the raw byte stream; the kernel template parameter
            // BITS=16 inside the codec re-interprets each pair of bytes as a
            // uint16_t cell.
            dv_sh_value_packed = engine().world.features_sh_quant16_fpbo.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant16_fpbo.bounds;
            sh_value_bounds_per_splat = true;
        } else if (engine().world.features_sh_quant16.initialized()) {
            dv_sh_value_packed = engine().world.features_sh_quant16.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant16.bounds;
            sh_value_bounds_per_splat = false;
        }
    }
    int num_sh = engine().num_sh;
    int num_sh_buffer = num_sh;  // buffer stride = engine().num_sh (model max)

    bool use_revised = (cfg.use_revised_densification);
    bool densify_ongoing =
        (step < std::max(cfg.refine_stop_iter,
                         max_steps - cfg.refine_stop_num_iter));
    bool do_densify = densify_grows_at(cfg, step, max_steps);
    float progress = ((float)step + 0.5f) / (float)max_steps;

    // Use pool-backed DeviceVector/DeviceTensor from Buffers directly
    auto& dv_means = engine().world.means;
    auto& dv_quats = engine().world.quats;
    auto& dv_scales = engine().world.scales;
    auto& dv_opacs = engine().world.opacities;
    auto& dv_features_dc = engine().world.features_dc;
    // features_sh: DeviceTensor2D<float3> [N, K] -> flatten to DeviceVector<float3> [N*K]
    DeviceVector<float3> dv_features_sh;
    {
        auto& t = engine().world.features_sh;
        TorchTensorView tv((uint64_t)t.data_ptr(), 4,
            {t.size<0>() * t.size<1>(), 3LL});
        dv_features_sh = DeviceVector<float3>(tv);
    }
    auto& dv_g1_means = engine().optim.g1_means;
    auto& dv_g1_quats = engine().optim.g1_quats;
    auto& dv_g1_scales = engine().optim.g1_scales;
    auto& dv_g1_opacs = engine().optim.g1_opacities;
    auto& dv_g1_features_dc = engine().optim.g1_features_dc;
    auto& dv_g2_means = engine().optim.g2_means;
    auto& dv_g2_quats = engine().optim.g2_quats;
    auto& dv_g2_scales = engine().optim.g2_scales;
    auto& dv_g2_opacs = engine().optim.g2_opacities;
    auto& dv_g2_features_dc = engine().optim.g2_features_dc;
    // g1/g2 features_sh as DeviceVector<float3> with numel=N*K. The densify kernel
    // reinterprets the data as uchar3 when quantize_sh, so the element_size carrier
    // type is just for the constructor's shape check.
    DeviceVector<float3> dv_g1_features_sh, dv_g2_features_sh;
    int64_t sh_flat = (int64_t)engine().max_num_splats * (int64_t)engine().num_sh;
    if (quantize_sh) {
        // Joint (u, sqrt(g2)) AoS layout: bytes for both halves of every cell
        // sit in a single packed buffer. Densify only zeros bytes on dst
        // splats, so we pass the same packed pointer for both the g1 and g2
        // wrappers; the kernel template is instantiated with `short3` (6 bytes
        // per SH coef = 3 channels x 2 bytes), so each write zeros the joint
        // (u, sqrt_g2) record for one (splat, coef). The second write
        // (g2_features_sh[i] = 0) targets the same memory and is a no-op.
        //
        // FPBO mode uses a separate `sh_quant_state_fpbo` buffer because its
        // per-block bounds granularity differs (per-splat-block vs per-cell-
        // block). The PACKED bytes have identical layout in both modes
        // (N*K*3*2 bytes, indexed by (splat, coef, channel)), so densify only
        // needs to pick the right buffer; bounds get rewritten by the next
        // optim-kernel pass and don't need touching here.
        uint8_t* packed = engine().optim.use_fused_proj_bwd_optim
            ? engine().optim.sh_quant_state_fpbo.packed_ptr()
            : engine().optim.sh_quant_state.packed_ptr();
        TorchTensorView tv((uint64_t)packed, 1, {sh_flat, 12LL});
        dv_g1_features_sh = DeviceVector<float3>(tv);
        dv_g2_features_sh = DeviceVector<float3>(tv);
    } else {
        // Non-quantized: storage is float3 N*K elements (12 bytes each), shape [N*K, 3], element_size=4
        TorchTensorView tv1((uint64_t)engine().optim.g1_features_sh.data_ptr(), 4, {sh_flat, 3LL});
        TorchTensorView tv2((uint64_t)engine().optim.g2_features_sh.data_ptr(), 4, {sh_flat, 3LL});
        dv_g1_features_sh = DeviceVector<float3>(tv1);
        dv_g2_features_sh = DeviceVector<float3>(tv2);
    }
    auto& dv_radii = engine().optim.radii;
    auto& dv_accum_buf = engine().optim.accum_buffer;
    auto& dv_bias_steps = engine().optim.bias_correction_steps;
    auto& dv_visit = engine().optim.visit_counters;

    // SH quant bounds (one float4 per block) -- needed by the densify kernels
    // to encode (g1=0, g2=0) into the dst splats' packed bytes against the
    // current bounds slot. FPBO mode uses a per-splat-block layout, regular
    // FusedAppearanceOptim.cu uses a per-cell-block layout. Empty vector when SH quant
    // is disabled; the kernels treat data_ptr() == nullptr as a sentinel and
    // fall back to plain-zero writes (which don't apply to non-quant g1/g2
    // anyway).
    DeviceVector<float4> dv_sh_quant_bounds;
    bool sh_bounds_per_splat = false;
    if (quantize_sh) {
        if (engine().optim.use_fused_proj_bwd_optim) {
            dv_sh_quant_bounds = engine().optim.sh_quant_state_fpbo.bounds;
            sh_bounds_per_splat = true;
        } else {
            dv_sh_quant_bounds = engine().optim.sh_quant_state.bounds;
            sh_bounds_per_splat = false;
        }
    }

    // Non-SH Adam-state quant bundle. Same struct as FPBO consumes; densify
    // uses it only to encode (g1=0, g2=0) into each relocated dst splat's
    // packed bytes against the current per-splat-block bound (FPBO layout).
    NonShQuantState non_sh;
    if (engine().optim.non_sh_optim_bits == 16
        && engine().optim.means_quant_state_fpbo.initialized()) {
        non_sh.enabled            = true;
        non_sh.means_packed       = engine().optim.means_quant_state_fpbo.packed_ptr();
        non_sh.quats_packed       = engine().optim.quats_quant_state_fpbo.packed_ptr();
        non_sh.scales_packed      = engine().optim.scales_quant_state_fpbo.packed_ptr();
        non_sh.opacities_packed   = engine().optim.opacities_quant_state_fpbo.packed_ptr();
        non_sh.features_dc_packed = engine().optim.features_dc_quant_state_fpbo.packed_ptr();
        non_sh.means_bounds       = engine().optim.means_quant_state_fpbo.bounds_ptr();
        non_sh.quats_bounds       = engine().optim.quats_quant_state_fpbo.bounds_ptr();
        non_sh.scales_bounds      = engine().optim.scales_quant_state_fpbo.bounds_ptr();
        non_sh.opacities_bounds   = engine().optim.opacities_quant_state_fpbo.bounds_ptr();
        non_sh.features_dc_bounds = engine().optim.features_dc_quant_state_fpbo.bounds_ptr();
    }

    // The oversize split channel only feeds draws, so the accumulator is
    // allocated for exactly as long as densification runs.
    const bool use_oversize_channel =
        densify_ongoing && use_revised && cfg.oversize_split_fraction > 0.0f
        && std::isfinite(cfg.max_screen_size);
    auto& dv_oversize = engine().optim.densify_oversize;
    if (use_oversize_channel) {
        if (dv_oversize.data_ptr() == nullptr) {
            dv_oversize.resize(PoolSlot::EngDensifyOversize, max_num_splats);
            dv_oversize.zero();
        }
    } else {
        dv_oversize = DeviceVector<float>();
    }

    // Clip large splats. With the soft penalty on, the hard clip is a
    // backstop the split draw below can pay for in the same step; running it
    // every step instead leaves the penalty no room to balance against.
    const bool clip_now = !cfg.clip_screen_size_at_refine
        || cfg.refine_every <= 0 || (step % cfg.refine_every) == 0;
    if (clip_now &&
        (std::isfinite(cfg.max_screen_size) || std::isfinite(cfg.max_world_size))) {
        densify_clip_scale_tensor(
            cur_num_splats,
            dv_radii, dv_scales,
            nullptr,  // don't clip opacities
            cfg.max_screen_size,
            cfg.max_screen_size_clip_hardness,
            cfg.max_world_size,
            first_trainable
        );
    }

    // weight = accum_weight^(1-w) * world_grad_score^w, where w == 0 leaves
    // the world-grad buffer unallocated and w == 1 passes that score alone
    // (TrainerCore then also skips producing the loss map).
    if (densify_ongoing && use_revised && dv_accum_buf.data_ptr() != nullptr) {
        const float blend_w = cfg.score_blend_world_grad;
        DeviceVector<float> wg_score = engine().fwd.world_grad_score;

        // Avg's raster-bwd buffer is [sum(w*a*T), sum(a*T)] planar; divide it
        // down to a plain [N] score now that every camera has landed.
        if (engine().fwd.accum_mode == DensifyAccumMode::Avg &&
            engine().fwd.accum_weight.data_ptr() != nullptr) {
            densify_accum_finalize_tensor(cur_num_splats,
                                          engine().fwd.accum_weight);
        }

        if (cfg.learned_score) {
            _learned_score_step(cur_num_splats, cfg, dv_radii, dv_accum_buf, dv_oversize);
        } else {
            DeviceVector<float> score;
            DeviceVector<float> score2;  // stays null unless 0 < w < 1
            // Only the opacity-gradient fallback wants the opacity weighting:
            // accum_weight (max of loss_map * alpha * T) and the world-grad score
            // already carry it, and re-applying would square the factor.
            bool weight_by_opacity = false;
            if (blend_w >= 1.0f && wg_score.data_ptr() != nullptr) {
                score = wg_score;
            } else {
                if (engine().fwd.accum_weight.data_ptr() != nullptr) {
                    score = engine().fwd.accum_weight;
                } else {
                    score = engine().grad.opacities.data_ptr() ? engine().grad.opacities : dv_opacs;
                    weight_by_opacity = true;
                }
                if (blend_w > 0.0f && wg_score.data_ptr() != nullptr)
                    score2 = wg_score;
            }

            densify_update_weight(
                cur_num_splats,
                dv_radii,
                nullptr,
                weight_by_opacity ? (float*)dv_opacs.data_ptr() : nullptr,
                score,
                score2,
                blend_w,
                cfg.score_power,
                dv_accum_buf,
                cfg.score_mode,
                cfg.max_screen_size,
                dv_oversize,
                first_trainable
            );
        }

        // Clipping above the q-quantile in place does not ratchet; a power
        // would, so it goes to the side buffer the draw and preview read, as
        // does any clip of a learned score (the state the forward renders).
        auto& dv_sample_score = engine().optim.densify_sample_score;
        const bool clip = cfg.score_clip_quantile > 0.0f && cfg.score_clip_quantile < 1.0f;
        // Sized for the whole pool, not cur_num_splats: the oversize draw
        // adds splats that the second draw then has to rank.
        if (cfg.final_score_power != 1.0f || (cfg.learned_score && clip))
            dv_sample_score.resize(PoolSlot::EngDensifySampleScore,
                                   max_num_splats);
        else
            dv_sample_score = DeviceVector<float2>();
        if (!cfg.learned_score) {
            densify_clip_score_tensor(cur_num_splats, dv_accum_buf,
                                      cfg.score_clip_quantile,
                                      cfg.final_score_power, dv_sample_score);
        } else if (dv_sample_score.data_ptr()) {
            // The quantile of x^p is the quantile of x, raised to p.
            densify_clip_score_tensor(cur_num_splats, dv_accum_buf, 1.0f,
                                      cfg.final_score_power, dv_sample_score);
            densify_clip_score_tensor(cur_num_splats, dv_sample_score,
                                      cfg.score_clip_quantile, 1.0f,
                                      DeviceVector<float2>());
        }
    }

    if (densify_ongoing && use_revised && cfg.split_weight_by_renders &&
        dv_accum_buf.data_ptr() != nullptr && dv_visit.data_ptr() != nullptr) {
        auto& score = engine().optim.densify_sample_score;
        if (score.data_ptr() == nullptr) {
            score.resize(PoolSlot::EngDensifySampleScore, max_num_splats);
            densify_clip_score_tensor(cur_num_splats, dv_accum_buf, 1.0f, 1.0f, score);
        }
        densify_scale_score_by_renders_tensor(cur_num_splats, dv_visit, score);
    }

    // The region test, on the refine cadence only: a weight per splat that
    // scales its draw in both relocation paths.
    auto& region = engine().region;
    if (do_densify && region.active()) {
        region.weight.resize(PoolSlot::EngRegionWeight, max_num_splats);
        region_weight_tensor(cur_num_splats, dv_means, dv_quats, dv_scales, region.camera_bvh,
                             region.camera_seeds, region.pattern, (uint32_t)step,
                             region.program, region.field_bvh, region.field_seeds,
                             region.inside, region.outside, region.weight,
                             first_trainable);
        // Before the relocation below, so what fades past dead moves inside.
        region_decay_opacity_tensor(cur_num_splats, region.weight, dv_opacs, region.opacity_decay,
                                    first_trainable);
        if (use_revised) {
            auto& score = engine().optim.densify_sample_score;
            if (score.data_ptr() == nullptr) {
                score.resize(PoolSlot::EngDensifySampleScore, max_num_splats);
                densify_clip_score_tensor(cur_num_splats, dv_accum_buf, 1.0f, 1.0f, score);
            }
            densify_scale_score_tensor(cur_num_splats, region.weight, score);
        }
    } else if (!region.active()) {
        region.weight = DeviceVector<float>();
    }

    int num_added = 0;
    const int64_t split_budget = cfg.max_split_fraction < 1.0f
        ? std::max<int64_t>(1, (int64_t)(cfg.max_split_fraction * (float)cur_num_splats))
        : 0;
    int64_t num_dead = 0, num_relocated = 0;

    // Rebased to the true value: the relocation test reads it, and the split
    // kernels write the new halves' in the same units.
    DeviceVector<float> dv_max_weight;
    auto& mw = engine().max_weight;
    if (do_densify && use_revised && mw.value.data_ptr() != nullptr) {
        if (step != mw.base_step)
            densify_max_weight_scale_tensor(
                cur_num_splats, std::exp2(-(float)(step - mw.base_step) / mw.half_life),
                mw.value);
        mw.base_step = step;
        dv_max_weight = mw.value;
    }
    const bool dead_by_opacity = cfg.dead_criterion != DeadCriterion::MaxWeight
        || dv_max_weight.data_ptr() == nullptr;

    // Long-axis-split opacity split factor `k`, linearly scheduled over the
    // first `las_split_opacity_k_warmup` steps (clamped to [init, final]).
    float split_opacity_k = cfg.las_split_opacity_k_final;
    if (cfg.las_split_opacity_k_warmup > 0) {
        float t = (float)step / (float)cfg.las_split_opacity_k_warmup;
        t = std::max(0.0f, std::min(1.0f, t));
        split_opacity_k = cfg.las_split_opacity_k_init
            + t * (cfg.las_split_opacity_k_final - cfg.las_split_opacity_k_init);
    }

    if (do_densify && use_revised)
        _log_score_by_size(step, cur_num_splats);

    if (do_densify && use_revised) {
        // Revised relocation (long axis split)
        num_relocated = relocate_splats_with_long_axis_split_tensor(
            cur_num_splats, cfg.min_opacity, split_opacity_k,
            dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
            dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
            dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
            dv_accum_buf, engine().optim.densify_sample_score, dv_bias_steps,
            dv_visit, (uint32_t)std::max(cfg.dead_after_steps, 0),
            dv_max_weight, dead_by_opacity,
            sh_optim_bits, num_sh,
            dv_sh_quant_bounds, sh_bounds_per_splat,
            dv_sh_value_packed, dv_sh_value_bounds,
            sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
            non_sh,
            2 * step + 0,
            split_budget, &num_dead,
            first_trainable
        );

        // Add more splats
        int64_t n_target = densify_target(cfg, cur_num_splats, densify_cap_at(cfg, step, max_num_splats));
        num_added = (int)std::max((int64_t)0, n_target - cur_num_splats);
        if (first_trainable > 0)
            num_added = (int)std::min<int64_t>(num_added, cur_num_splats - first_trainable);
        if (split_budget > 0)
            num_added = (int)std::max<int64_t>(0, std::min<int64_t>(num_added, split_budget - num_relocated));
        if (num_added > 0) {
            // The oversize channel draws first, so its candidate set is the
            // splats the accumulator actually covers.
            int n_oversize = 0;
            if (use_oversize_channel) {
                n_oversize = (int)std::lround(
                    cfg.oversize_split_fraction * (float)num_added);
                n_oversize = std::max(0, std::min(n_oversize, num_added));
            }
            auto add_split = [&](int64_t n_cur, int n_new,
                                 DeviceVector<float2> weights, uint32_t seed) {
                add_splats_with_long_axis_split_tensor(
                    n_cur, n_new, split_opacity_k,
                    dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
                    dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
                    dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
                    dv_accum_buf, weights,
                    dv_bias_steps, dv_visit, dv_max_weight,
                    sh_optim_bits, num_sh,
                    dv_sh_quant_bounds, sh_bounds_per_splat,
                    dv_sh_value_packed, dv_sh_value_bounds,
                    sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
                    non_sh,
                    seed,
                    first_trainable
                );
            };
            auto& dv_score = engine().optim.densify_sample_score;
            if (n_oversize > 0) {
                DeviceVector<float2> dv_over_w;
                dv_over_w.resize(PoolSlot::EngDensifyOversizeWeight,
                                 cur_num_splats);
                densify_oversize_weight_tensor(
                    cur_num_splats, cfg.oversize_score_blend, dv_oversize,
                    dv_score.data_ptr() ? dv_score : dv_accum_buf, dv_over_w,
                    first_trainable);
                add_split(cur_num_splats, n_oversize, dv_over_w,
                          2 * step + 1 + 0x5bf03635u);
                // The split kernel carries accum_buf src -> dst but not the
                // transformed score, so the second draw would rank the new
                // splats on whatever those slots held in a past life.
                if (dv_score.data_ptr())
                    backend::memset_sync(dv_score.data_ptr() + cur_num_splats,
                                         0, (size_t)n_oversize * sizeof(float2));
            }
            if (num_added - n_oversize > 0)
                add_split(cur_num_splats + n_oversize,
                          num_added - n_oversize, dv_score, 2 * step + 1);
        }
    } else if (do_densify) {
        // MCMC relocation
        relocate_splats_mcmc_tensor(
            cur_num_splats, cfg.min_opacity,
            dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
            dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
            dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
            dv_bias_steps,
            sh_optim_bits, num_sh,
            dv_sh_quant_bounds, sh_bounds_per_splat,
            dv_sh_value_packed, dv_sh_value_bounds,
            sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
            non_sh,
            2 * step + 0
        , engine().region.weight, first_trainable);

        // MCMC sample add
        int64_t n_target = densify_target(cfg, cur_num_splats, densify_cap_at(cfg, step, max_num_splats));
        num_added = (int)std::max((int64_t)0, n_target - cur_num_splats);
        if (num_added > 0) {
            add_splats_mcmc_tensor(
                cur_num_splats, num_added, cfg.min_opacity,
                dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
                dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
                dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
                dv_bias_steps,
                sh_optim_bits, num_sh,
                dv_sh_quant_bounds, sh_bounds_per_splat,
                dv_sh_value_packed, dv_sh_value_bounds,
                sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
                non_sh,
                2 * step + 1
            , engine().region.weight, first_trainable);
        }
    }

    // Reset accum buffer after densification step; a learned score carries on.
    if (do_densify && dv_accum_buf.data_ptr() != nullptr && !cfg.learned_score) {
        backend::memset_sync(dv_accum_buf.data_ptr(), 0, dv_accum_buf.size() * sizeof(float2));
    }
    if (do_densify && dv_oversize.data_ptr() != nullptr) {
        backend::memset_sync(dv_oversize.data_ptr(), 0,
                             dv_oversize.size() * sizeof(float));
    }

    // Add MCMC noise
    if (cfg.noise_lr > 0.0f && cfg.noise_lr_final > 0.0f) {
        float noise_scalar = cfg.noise_lr * powf(cfg.noise_lr_final / cfg.noise_lr, progress);
        if (use_revised) {
            revised_add_noise_tensor(
                cur_num_splats + num_added, noise_scalar,
                dv_radii, dv_means, dv_scales, dv_quats, dv_opacs,
                first_trainable);
        } else {
            mcmc_add_noise_tensor(
                cur_num_splats + num_added, noise_scalar,
                dv_means, dv_scales, dv_quats, dv_opacs,
                first_trainable);
        }
    }

    if (do_densify) {
        engine().optim.last_num_dead      = num_dead;
        engine().optim.last_num_relocated = num_relocated;
    }
    // SS_VISIT_LOG=1: what the render counters hold at each refine step, in
    // English like the other deep diagnostics.
    static const bool log_visit = [] {
        const char* v = spirula::env("VISIT_LOG");
        return v && *v && v[0] != '0';
    }();
    if (do_densify && log_visit && dv_visit.data_ptr() != nullptr) {
        std::vector<uint32_t> h((size_t)cur_num_splats);
        backend::memcpy_sync(h.data(), dv_visit.data_ptr(),
                             h.size() * sizeof(uint32_t), backend::MemcpyKind::DeviceToHost);
        std::vector<uint32_t> r(h.size()), u(h.size());
        int64_t never = 0;
        for (size_t i = 0; i < h.size(); ++i) {
            r[i] = visit_renders(h[i]);
            u[i] = visit_unrendered_steps(h[i]);
            never += r[i] == 0u;
        }
        std::sort(r.begin(), r.end());
        std::sort(u.begin(), u.end());
        const size_t n = h.size();
        std::fprintf(stderr,
                     "[visit] step %d: never rendered %.1f%%, renders p10 %u median %u p90 %u, "
                     "unrendered streak median %u p90 %u max %u\n",
                     step, 100.0 * (double)never / (double)std::max<size_t>(n, 1),
                     r[n / 10], r[n / 2], r[(9 * n) / 10], u[n / 2], u[(9 * n) / 10], u[n - 1]);
    }
    engine().cur_num_splats = cur_num_splats + num_added;
    DevicePool::global().set_splat_counts(engine().cur_num_splats, engine().max_num_splats);
    return num_added;
}
