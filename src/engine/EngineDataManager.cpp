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
#include "data/DataManager.h"
#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"
#include "i18n/catalog/Log.h"

#include <cstdio>
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
    // Drop the existing manager first so its scheduler / worker threads are
    // joined before we begin spinning up new ones.
    engine().dm.reset();

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


// Resolve the `split_batch` + `use_fused_proj_bwd_optim` conflict. Both
// turned on at once is an inconsistency: split_batch loops over single
// cameras and accumulates atomicAdd into world-grad buffers, whereas FPBO
// folds projection-bwd into the optim kernel and never materializes those
// buffers. If we know (from the DataManager) that the post-split batch
// size will always be 1, split_batch is a no-op and FPBO is the correct
// choice. Otherwise the user wants the memory win of split_batch and we
// must turn FPBO off. Prints a one-shot warning describing the choice.
static EngineStepConfig _resolve_split_vs_fpbo(const EngineStepConfig& cfg,
                                               int64_t max_batch_known,
                                               int max_face_passes) {
    // Faces of unequal size are rendered one pass per size, which accumulates
    // grad across passes -- the very thing FPBO cannot do.
    if (max_face_passes > 1) {
        EngineStepConfig out = cfg;
        out.optim.use_fused_proj_bwd_optim = false;
        return out;
    }
    if (!cfg.optim.split_batch || !cfg.optim.use_fused_proj_bwd_optim)
        return cfg;
    EngineStepConfig out = cfg;
    static bool warned = false;
    // max_batch_known == -1 -> caller couldn't determine ahead of time;
    // treat as "may exceed 1" so the user gets the memory-safe path.
    bool batch_le_one = (max_batch_known > 0 && max_batch_known <= 1);
    if (batch_le_one) {
        out.optim.split_batch = false;
        if (!warned) {
            #if 0
            fprintf(stderr, "[spirula] %s\n",
                    spirula::i18n::msg::log::warn_split_batch_noop.get());
            #endif
            warned = true;
        }
    } else {
        out.optim.use_fused_proj_bwd_optim = false;
        if (!warned) {
            #if 0
            fprintf(stderr, "[spirula] %s\n",
                    spirula::i18n::format(
                        spirula::i18n::msg::log::warn_fpbo_incompatible,
                        {(long long)max_batch_known}).c_str());
            #endif
            warned = true;
        }
    }
    return out;
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
    const EngineStepConfig cfg = _resolve_split_vs_fpbo(
        cfg_in, engine().dm->max_input_batch_size(),
        engine().dm->max_face_passes());

    const TrainStep& stp = engine().dm->next_train_step();
    if (stp.subs.empty())
        throw std::runtime_error("engine_train_step_managed: empty training step");

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
    if (!engine().dm)
        throw std::runtime_error(
            "engine_eval_forward: DataManager not configured — call "
            "engine_setup_data_manager(...) with the eval split first.");

    const TrainStep& stp = engine().dm->next_train_step();
    if (stp.subs.empty()) return 0;
    if (stp.subs.size() != 1)
        throw std::runtime_error(
            "engine_eval_forward: expected one sub-batch per eval step "
            "(set train_batch_size = 1)");

    const DecodedBatch& b = *stp.subs[0];
    if (b.face_passes.size() > 1)
        throw std::runtime_error(
            "engine_eval_forward: the eval split must be baked with uniform "
            "faces, which is what TrainerCore does");
    return _install_and_forward(b, std::move(primitive), sh_degree, packed);
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
