// Engine ↔ DataManager glue.
//
// engine_setup_data_manager:
//   Build / replace the engine-owned DataManager from a parsed-out dataset.
//   Idempotent; an existing DataManager is destroyed first (and its disk
//   prefetch threads cleanly joined) before swap-in.
//
// engine_train_step_managed:
//   Pull the next training batch from engine().dm and dispatch the same
//   fused step as engine_train_step. The DataManager owns the per-step
//   host buffers; we hand pointers into those buffers to the engine as
//   TorchTensorViews. They stay alive until the *next* call to
//   next_train_batch().

#include "core/Camera.h"         // camera_model_to_string
#include "core/ColorSpace.h"
#include "data/DataManager.h"
#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"
#include "i18n/catalog/Log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>


void engine_setup_data_manager(
    DataManagerConfig         cfg,
    std::vector<int32_t>      camera_models,
    std::vector<int32_t>      camera_distortions,
    std::vector<std::string>  image_filenames,
    std::vector<std::string>  mask_filenames,
    std::vector<std::string>  depth_filenames,
    std::vector<std::string>  normal_filenames,
    std::vector<int32_t>      widths,
    std::vector<int32_t>      heights,
    std::vector<int32_t>      K_per_camera,
    std::vector<int32_t>      post_offsets,
    std::vector<float>        viewmats,
    std::vector<float>        intrins,
    std::vector<float>        dist_coeffs,
    std::vector<int32_t>      post_widths,
    std::vector<int32_t>      post_heights,
    std::vector<float>        face_axes,
    std::vector<float>        input_intrins,
    std::vector<float>        input_dist_coeffs,
    std::vector<int32_t>      redistort_models,
    std::vector<float>        redistort_params,
    std::vector<int32_t>      train_indices,
    std::vector<int32_t>      val_indices)
{
    // Join the old decode workers before starting their replacement.
    engine().dm.reset();
    engine().eval_batch.reset();
    engine().eval_next_input = 0;
    engine().eval_next_pass  = 0;
    engine().gt_mean_luma.clear();

    engine().dm = std::make_unique<DataManager>(
        std::move(cfg), std::move(camera_models), std::move(camera_distortions),
        std::move(image_filenames),  std::move(mask_filenames),
        std::move(depth_filenames),  std::move(normal_filenames),
        std::move(widths),           std::move(heights),
        std::move(K_per_camera),     std::move(post_offsets),
        std::move(viewmats),         std::move(intrins),
        std::move(dist_coeffs),
        std::move(post_widths),      std::move(post_heights),
        std::move(face_axes),
        std::move(input_intrins),    std::move(input_dist_coeffs),
        std::move(redistort_models), std::move(redistort_params),
        std::move(train_indices),    std::move(val_indices));
}


static EngineStepConfig _resolve_split_vs_fpbo(const EngineStepConfig& cfg,
                                               int64_t max_batch_known,
                                               int max_face_passes) {
    const bool fused = resolve_fused_proj_bwd_optim(
        cfg.optim.use_fused_proj_bwd_optim, cfg.optim.split_batch,
        max_batch_known, max_face_passes);
    if (fused == cfg.optim.use_fused_proj_bwd_optim &&
        !(fused && cfg.optim.split_batch))
        return cfg;
    EngineStepConfig out = cfg;
    out.optim.use_fused_proj_bwd_optim = fused;
    if (fused) out.optim.split_batch = false;
    return out;
}


// Mean Rec.709 luma of one reference image, in display code values.
static float _sampled_mean_luma(uint64_t base, int64_t n_px, int64_t width,
                                uint32_t elem) {
    int64_t stride = std::max<int64_t>(1, n_px / 256);
    // Coprime with the row length, or the walk samples one column of pixels.
    while (std::gcd(stride, width) != 1) stride++;

    const auto& cs = engine().color_space;
    const auto transfer = (colorspace::Transfer)cs.image_transfer;
    const uint64_t pitch = 3 * (uint64_t)elem;

    double sum = 0.0;
    int64_t n = 0;
    for (int64_t i = 0; i < n_px; i += stride, n++) {
        const void* p = (const void*)(base + (uint64_t)i * pitch);
        float c[3];
        if (elem == 1) {
            const uint8_t* q = (const uint8_t*)p;
            for (int k = 0; k < 3; k++) c[k] = q[k] * (1.0f / 255.0f);
        } else if (elem == 2) {
            const uint16_t* q = (const uint16_t*)p;
            for (int k = 0; k < 3; k++) c[k] = q[k] * (1.0f / 65535.0f);
        } else {
            const float* q = (const float*)p;
            for (int k = 0; k < 3; k++) c[k] = q[k];
        }
        // What _engine_color_space_apply_to_gt does to the same pixels on the
        // GPU (working_to_display in shaders/pixel_wise.slang).
        if (cs.image_enabled) {
            if (!cs.image_is_linear)
                for (int k = 0; k < 3; k++) c[k] = colorspace::srgb_to_linear(c[k]);
            colorspace::apply3x3(cs.image_color_matrix_host, c);
            for (int k = 0; k < 3; k++) c[k] = colorspace::tone_encode(c[k], transfer);
        }
        sum += 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
    }
    return (float)(sum / (double)n);
}


// LossConfig::normalize_by_luminance: the photometric weights divided by
// 0.5 / max(mean sRGB luma, 1/255). Leaves them alone when the step carries
// no reference image to measure.
static void _normalize_weights_by_luminance(LossConfig& loss, const TrainStep& stp) {
    std::vector<float>& cache = engine().gt_mean_luma;
    double sum = 0.0, total_px = 0.0;
    for (const auto& sub : stp.subs) {
        const TorchTensorView& rgb = sub->rgb_view;
        const auto& shape = std::get<2>(rgb);
        const uint64_t base = std::get<0>(rgb);
        if (base == 0 || shape.size() != 4 || shape[3] != 3) continue;
        const int64_t B = shape[0], px = shape[1] * shape[2];
        if (px <= 0 || (int64_t)sub->indices.size() != B) continue;
        const uint32_t elem = std::get<1>(rgb);

        for (int64_t j = 0; j < B; j++) {
            const int32_t id = sub->indices[(size_t)j];
            if (id < 0) continue;
            if ((size_t)id >= cache.size())
                cache.resize((size_t)id + 1, std::numeric_limits<float>::quiet_NaN());
            if (std::isnan(cache[(size_t)id]))
                cache[(size_t)id] = _sampled_mean_luma(
                    base + (uint64_t)(j * px) * 3 * elem, px, shape[2], elem);
            sum += (double)cache[(size_t)id] * (double)px;
            total_px += (double)px;
        }
    }
    if (total_px <= 0.0) return;

    const float scale = (float)(0.5 / std::max(sum / total_px, 1.0 / 255.0));
    constexpr LossWeightIndex photometric[] = {
        LossWeightIndex::RgbSupL1, LossWeightIndex::RgbSupL2,
        LossWeightIndex::YSupL1,   LossWeightIndex::YSupL2,
        LossWeightIndex::USupL2,   LossWeightIndex::VSupL2,
    };
    for (LossWeightIndex i : photometric) loss.weights[(int)i] *= scale;
    loss.w_ssim *= scale;
}


std::map<std::string, float> engine_train_step_managed(
    int step, int max_steps,
    std::string primitive,
    int sh_degree,
    bool packed,
    const EngineStepConfig& cfg_in)
{
    if (!engine().dm) {
        throw std::runtime_error(
            "engine_train_step_managed: DataManager not configured — "
            "call engine_setup_data_manager(...) first.");
    }

    // Resolve split_batch vs FPBO using the DataManager's view of the
    // dataset (max train batch size × max K). Done once per session via
    // the static warned flag inside the helper.
    EngineStepConfig cfg = _resolve_split_vs_fpbo(
        cfg_in, engine().dm->max_input_batch_size(),
        engine().dm->max_face_passes());

    const TrainStep& stp = engine().dm->next_train_step();
    if (stp.subs.empty())
        throw std::runtime_error("engine_train_step_managed: empty training step");

    if (cfg.loss.normalize_by_luminance)
        _normalize_weights_by_luminance(cfg.loss, stp);

    // Build a POST-split bilagrid cam-index buffer for one sub-batch.
    // bilagrid_cam_indices must be the POST-split camera id, not the input
    // dataset id: for mixed datasets (e.g. K=5 fisheye + K=1 pinhole) those
    // diverge after the first K>1 input, so reading b.indices[j] for K==1
    // batches would point the thumbnail kernel + bilagrid + PPISP at fisheye
    // slots. `b.post_offsets[j]` is the starting post-split slot for input
    // image j (populated by DataManager::allocate_batch).
    auto build_bg_idx = [](const DecodedBatch& b, std::vector<int32_t>& buf) {
        buf.resize((size_t)b.num);
        int K = b.K;
        for (int j = 0; j < b.input_num; ++j) {
            int32_t off = b.post_offsets[j];
            for (int k = 0; k < K; ++k) buf[(size_t)j * K + k] = off + k;
        }
    };

    // ---- Fast path: single homogeneous sub-batch (unchanged behavior) -----
    if (stp.subs.size() == 1) {
        const DecodedBatch& b = *stp.subs[0];
        static std::vector<int32_t> _bg_idx_buf;   // reused across steps
        build_bg_idx(b, _bg_idx_buf);
        TorchTensorView bilagrid_cam_indices(
            (uint64_t)_bg_idx_buf.data(), 4, {(int64_t)b.num, 1LL});

        if (b.K <= 1 && b.input_source_models.empty()) {
            // Standard path: GT is already at engine shape, no warp needed.
            return engine_train_step(
                step, max_steps,
                std::move(primitive), sh_degree, packed,
                (int)b.width, (int)b.height,
                camera_model_to_string(b.model),
                camera_distortion_to_string(b.distortion),
                b.viewmats_view, b.intrins_view, b.dist_coeffs_view,
                b.rgb_view, b.depth_view, b.normal_view, b.mask_view,
                bilagrid_cam_indices,
                cfg);
        }

        // ---- Warp path: fisheye / equisolid + warp_to_pinhole OR equirect --
        // Depth is warped to per-face ray depth, normal is rotated into each
        // face's camera frame (see set_training_data_warped).
        return engine_train_step_warped(
            step, max_steps,
            std::move(primitive), sh_degree, packed,
            (int)b.width, (int)b.height,
            b.viewmats_view, b.intrins_view, b.dist_coeffs_view,
            camera_model_to_string(b.input_model),
            camera_distortion_to_string(b.input_distortion),
            b.input_num, b.input_height, b.input_width, b.K,
            b.input_intrins_view, b.input_dist_coeffs_view,
            b.input_source_models_view, b.input_source_params_view,
            b.rgb_view, b.mask_view,
            b.mask_height, b.mask_width,
            b.depth_view, b.depth_height, b.depth_width,
            b.normal_view, b.normal_height, b.normal_width,
            b.face_axes_view, b.face_passes,
            bilagrid_cam_indices,
            cfg);
    }

    // ---- Heterogeneous step: multiple non-warp sub-batches, accumulated ----
    // The DataManager only packs K == 1 (non-warp) groups across resolutions,
    // so every sub-batch here is a plain pinhole/model batch that goes through
    // the standard (non-warp) fwd/bwd. Per-sub bilagrid index buffers must
    // stay alive across the whole engine_train_step_hetero call.
    std::vector<std::vector<int32_t>> bg_bufs(stp.subs.size());
    std::vector<HeteroSubBatch> hsubs;
    hsubs.reserve(stp.subs.size());
    for (size_t i = 0; i < stp.subs.size(); ++i) {
        const DecodedBatch& b = *stp.subs[i];
        if (b.K > 1 || !b.input_source_models.empty()) {
            throw std::runtime_error(
                "engine_train_step_managed: warp sub-batch (K>1 or re-distort) "
                "inside a multi-sub-batch step — the scheduler must never pack "
                "these.");
        }
        build_bg_idx(b, bg_bufs[i]);

        HeteroSubBatch hs;
        hs.width        = (int)b.width;
        hs.height       = (int)b.height;
        hs.num          = (int)b.num;
        hs.camera_model = camera_model_to_string(b.model);
        hs.distortion   = camera_distortion_to_string(b.distortion);
        hs.viewmats     = b.viewmats_view;
        hs.intrins      = b.intrins_view;
        hs.dist_coeffs  = b.dist_coeffs_view;
        hs.gt_rgb       = b.rgb_view;
        hs.gt_depth     = b.depth_view;
        hs.gt_normal    = b.normal_view;
        hs.gt_alpha     = b.mask_view;
        hs.bilagrid_cam_indices = TorchTensorView(
            (uint64_t)bg_bufs[i].data(), 4, {(int64_t)b.num, 1LL});
        hsubs.push_back(std::move(hs));
    }

    return engine_train_step_hetero(
        step, max_steps,
        std::move(primitive), sh_degree, packed,
        hsubs, cfg);
}


void engine_resolve_data_error(bool retry) {
    if (engine().dm) engine().dm->resolve_data_error(retry);
}


// ---------------------------------------------------------------------------
// Forward-only paths: take a batch, install it, render it. See Engine.h.
// ---------------------------------------------------------------------------
static TorchTensorView _tv_null() { return {0, 0, {}}; }

// Slice [k_start, k_start + k_count) off the leading dim; null passes through.
static TorchTensorView _slice_rows(const TorchTensorView& tv, int64_t k_start,
                                   int64_t k_count) {
    uint64_t base = std::get<0>(tv);
    if (base == 0) return tv;
    const auto& shape = std::get<2>(tv);
    if (shape.empty()) return tv;
    int64_t inner = 1;
    for (size_t i = 1; i < shape.size(); ++i) inner *= shape[i];
    std::vector<int64_t> out = shape;
    out[0] = k_count;
    return TorchTensorView(base + (uint64_t)k_start * (uint64_t)inner *
                                      (uint64_t)std::get<1>(tv),
                           std::get<1>(tv), std::move(out));
}

// How many post-split views one pass of a batch renders -- what
// _install_and_forward returns, needed by its caller before the call.
static int _batch_views(const DecodedBatch& b, int pass) {
    if (b.K <= 1 && b.input_source_models.empty()) return (int)b.num;
    const int C = b.face_passes.empty() ? 1 : (int)b.face_passes.size();
    const int p = std::min(std::max(pass, 0), C - 1);
    const int k0 = C == 1 ? 0 : b.face_passes[(size_t)p].k0;
    return (C == 1 ? (int)b.K : b.face_passes[(size_t)p].k1) - k0;
}

// Install a decoded batch as GT + camera params and run the forward pass;
// neither caller wants loss, backward or optim. `with_geometry` installs the
// depth and normal GT too, which costs the linear->ray conversion.
static int _install_and_forward(const DecodedBatch& b, std::string primitive,
                                int sh_degree, bool packed,
                                bool with_geometry = false,
                                bool input_depth_is_ray_depth = true,
                                int dist_type = 0, int pass = 0) {
    const bool geom = with_geometry;
    if (b.K <= 1 && b.input_source_models.empty()) {
        set_camera_params((int)b.width, (int)b.height,
                          camera_model_to_string(b.model),
                          camera_distortion_to_string(b.distortion),
                          b.viewmats_view, b.intrins_view, b.dist_coeffs_view);
        set_training_data(b.rgb_view,
                          geom ? b.depth_view : _tv_null(),
                          geom ? b.normal_view : _tv_null(),
                          b.mask_view, input_depth_is_ray_depth);
    } else {
        // Faces of unequal size render one pass at a time; the rows of a pass
        // are contiguous, and a fetched batch is one input image.
        const int C = b.face_passes.empty() ? 1 : (int)b.face_passes.size();
        const int p = std::min(std::max(pass, 0), C - 1);
        const int k0 = C == 1 ? 0 : b.face_passes[(size_t)p].k0;
        const int Kc = (C == 1 ? b.K : b.face_passes[(size_t)p].k1) - k0;
        const int Wc = C == 1 ? (int)b.width  : b.face_passes[(size_t)p].width;
        const int Hc = C == 1 ? (int)b.height : b.face_passes[(size_t)p].height;
        if (C > 1 && b.input_num != 1)
            throw std::runtime_error(
                "engine forward: a multi-pass batch must hold one input image");
        // b.model / b.distortion are already PINHOLE / NONE when K > 1; at
        // K == 1 (re-distort) they are the camera the parser fitted.
        set_camera_params(Wc, Hc,
                          camera_model_to_string(b.model),
                          camera_distortion_to_string(b.distortion),
                          _slice_rows(b.viewmats_view, k0, Kc),
                          _slice_rows(b.intrins_view, k0, Kc),
                          _slice_rows(b.dist_coeffs_view, k0, Kc));
        set_training_data_warped(
            camera_model_to_string(b.input_model),
            camera_distortion_to_string(b.input_distortion),
            b.input_num, (int)b.input_height, (int)b.input_width,
            Kc, Hc, Wc,
            b.rgb_view, b.mask_view, (int)b.mask_height, (int)b.mask_width,
            geom ? b.depth_view : _tv_null(),
            geom ? (int)b.depth_height : 0, geom ? (int)b.depth_width : 0,
            geom ? b.normal_view : _tv_null(),
            geom ? (int)b.normal_height : 0, geom ? (int)b.normal_width : 0,
            input_depth_is_ray_depth,
            b.input_intrins_view, b.input_dist_coeffs_view,
            b.input_source_models_view, b.input_source_params_view,
            _slice_rows(b.face_axes_view, k0, Kc));
        forward_3dgs(std::move(primitive), sh_degree, packed,
                     /*output_median=*/false, dist_type);
        return _batch_views(b, pass);
    }

    forward_3dgs(std::move(primitive), sh_degree, packed,
                 /*output_median=*/false, dist_type);
    return _batch_views(b, pass);
}

int engine_eval_forward(std::string primitive, int sh_degree, bool packed) {
    EngineState& st = engine();
    if (!st.dm)
        throw std::runtime_error(
            "engine_eval_forward: DataManager not configured — call "
            "engine_setup_data_manager(...) with the eval split first.");

    const int64_t num_inputs = st.dm->num_train();
    if (st.eval_next_input >= num_inputs) {
        st.eval_batch.reset();
        return 0;
    }

    // Evaluation follows dataset order, not the randomized training schedule.
    if (st.eval_next_pass == 0) {
        if (!st.eval_batch) st.eval_batch = std::make_unique<DecodedBatch>();
        st.dm->fetch_one((int32_t)st.eval_next_input, *st.eval_batch);
    }
    DecodedBatch& b = *st.eval_batch;

    const int passes = std::max<int>(1, (int)b.face_passes.size());
    const int views = _install_and_forward(b, std::move(primitive), sh_degree, packed,
                                           /*with_geometry=*/false,
                                           /*input_depth_is_ray_depth=*/true,
                                           /*dist_type=*/0, st.eval_next_pass);
    if (++st.eval_next_pass >= passes) {
        st.eval_next_pass = 0;
        st.eval_next_input++;
    }
    return views;
}

int engine_preview_forward(int index, std::string primitive, int sh_degree,
                           bool packed, bool apply_color_correction,
                           const LossConfig& loss, int pass, int* out_passes) {
    if (!engine().dm)
        throw std::runtime_error(
            "engine_preview_forward: DataManager not configured — call "
            "engine_setup_data_manager(...) first.");

    // Static: the buffers the views point into must outlive the forward, and
    // a preview is one call at a time under the engine mutex. Later passes of
    // one image re-render what pass 0 fetched.
    static DecodedBatch b;
    static int fetched = -1;
    if (pass <= 0 || fetched != index) {
        engine().dm->fetch_one((int32_t)index, b);
        fetched = index;
    }
    const int passes = std::max<int>(1, (int)b.face_passes.size());
    if (out_passes) *out_passes = passes;
    pass = std::min(std::max(pass, 0), passes - 1);
    const int k0 = b.face_passes.empty() ? 0 : b.face_passes[(size_t)pass].k0;
    // With the geometry GT and the training step's distortion channels: this
    // renders what the loss compares, so a loss map read off this forward
    // carries the same terms the trainer's does.
    const DistortionType dist_type = engine_distortion_type(
        engine_primitive_pixel_type(primitive),
        loss.weights[(int)LossWeightIndex::RgbDistReg],
        loss.weights[(int)LossWeightIndex::DepthDistReg],
        loss.weights[(int)LossWeightIndex::NormalDistReg]);
    // POST-split camera ids for this pass's faces, the same ones the training
    // step hands the per-image tables. Built before the forward because the
    // before-color-space order applies PPISP inside it.
    const int views = _batch_views(b, pass);
    std::vector<int32_t> cam_idx((size_t)views);
    for (int v = 0; v < views; ++v)
        cam_idx[(size_t)v] = b.post_offsets[0] + k0 + v;
    TorchTensorView cam_view((uint64_t)cam_idx.data(), 4,
                             {(int64_t)views, 1LL});
    const bool ppisp_in_forward = apply_color_correction &&
                                  engine().ppisp.enabled &&
                                  engine().ppisp.cur_run_before_color_space;
    if (ppisp_in_forward) {
        _set_cur_cam_indices(cam_view);
        engine().ppisp.forward_pending = true;
    }

    _install_and_forward(
        b, std::move(primitive), sh_degree, packed,
        /*with_geometry=*/true, loss.input_depth_is_ray_depth, (int)dist_type,
        pass);

    if (apply_color_correction) {
        const bool ppisp_after = engine().ppisp.enabled && !ppisp_in_forward;
        const bool bg_enabled = engine().bilagrid_rgb.enabled ||
                                engine().bilagrid_depth.enabled ||
                                engine().bilagrid_normal.enabled;
        if (engine().ppisp.cur_run_before_bilagrid) {
            if (ppisp_after) engine_ppisp_forward(cam_view);
            if (bg_enabled)  engine_bilagrid_forward(cam_view);
        } else {
            if (bg_enabled)  engine_bilagrid_forward(cam_view);
            if (ppisp_after) engine_ppisp_forward(cam_view);
        }
    }
    return views;
}
