// Engine forward pass + debug forward.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <stdexcept>
#include <string>


extern void _engine_background_forward();
extern void _engine_color_space_forward();


void engine_release_screen_buffers() {
    // Drop the views FIRST: they point into the pool slots below, and
    // anything that read them after the free would be reading freed memory.
    engine().fwd.splats_s.clear();
    DevicePool::global().free_dynamic_prefix(
        std::string(slot_name(PoolSlot::ProjScreen)) + ".");
}

void forward_3dgs(
    std::string primitive,   // "3dgs", "mip", "3dgut"
    int sh_degree,
    bool packed,
    bool output_median,      // also emit per-pixel median depth
    int dist_type_int        // distortion channel set (DistortionType); emits D = W*S - C^2
) {
    const DistortionType dist_type = (DistortionType)dist_type_int;
    engine().primitive = primitive;
    engine().sh_degree = sh_degree;
    engine().packed = packed;

    // The stashed screen gradients live in the arena the intersect below
    // reuses, so this view is already dead. Dropping it turns a stale
    // fused-optim read into that step's "v_splats_s not stashed" error.
    engine().fwd.v_splats_s.clear();

    // Build splats as DeviceTensorFloatND from typed device buffers.
    // For DeviceVector<float> (opacities), build a [N, 1] shape FND manually.
    DeviceTensorFloatND fnd_opac;
    {
        TorchTensorView opac_tv((uint64_t)engine().world.opacities.data_ptr(), 4,
            {engine().world.opacities.size(), 1LL, 1LL});
        fnd_opac = DeviceTensorFloatND(opac_tv);  // ndim=2, shape=[N, 1]
    }
    std::vector<DeviceTensorFloatND> in_splats = {
        DeviceTensorFloatND(engine().world.means),         // [N, 3]
        DeviceTensorFloatND(engine().world.quats),         // [N, 4]
        DeviceTensorFloatND(engine().world.scales),        // [N, 3]
        fnd_opac,                                          // [N, 1]
        DeviceTensorFloatND(engine().world.features_dc),   // [N, 3]
        DeviceTensorFloatND(engine().world.features_sh),   // [N, K, 3]
    };
    engine().fwd.splats_w = in_splats;

    DeviceTensor2D<uint2> aabb_q;
    DeviceTensorFloatND depths_nd;

    // Allocate and zero radii buffer before projection (kernel uses atomicMax).
    // In sub-batched mode, projection runs once per sub-batch; the optim step's
    // scale-agnostic-mean term uses radii as the dimensionless screen-size
    // denominator (lr * grad / (radii*0.6f)) -- if radii is 0 (splat absent
    // from the *last* sub-batch's camera), the means update divides by
    // ~eps and sends splats to infinity, so we must accumulate the
    // atomicMax across sub-batches by zeroing ONLY on the first one.
    // engine().optim.skip_grad_zero mirrors that same "is this the first
    // sub-batch of the train step?" flag and is set by the sub-batch
    // dispatcher in EngineTrainStep.cpp.
    engine().optim.radii.resize(PoolSlot::EngRadii, engine().max_num_splats);
    if (!engine().optim.skip_grad_zero) {
        engine().optim.radii.zero();
    }

    // SH VALUE-quant pointers. When sh_value_bits != 32, the canonical SH
    // storage lives in engine().world.features_sh_quant{8,16}{,_fpbo} (the
    // FPBO-layout variant when use_fused_proj_bwd_optim is on; cell-block
    // otherwise). The projection forward kernels read from these via the
    // codec when their VALUE_BITS template is non-32.
    std::optional<TorchTensorView> sh_value_packed_opt = std::nullopt;
    std::optional<TorchTensorView> sh_value_bounds_opt = std::nullopt;
    const int sh_value_bits = engine().world.sh_value_bits;
    const uint32_t num_sh_buffer = (uint32_t)engine().num_sh;
    // sh_bounds_stride selects the packed-SH layout the kernels address with:
    // 256 = per-cell-block AoS, 0 = FPBO (docs/notes/sh-quant-layout.md).
    int64_t sh_bounds_stride = 0;
    if (sh_value_bits == 8) {
        auto pick = [&](auto& vq) {
            if (!vq.initialized()) return;
            sh_value_packed_opt = TorchTensorView(
                (uint64_t)vq.packed_ptr(), 1, {vq.packed_bytes()});
            sh_value_bounds_opt = TorchTensorView(
                (uint64_t)vq.bounds_ptr(), 4, {vq.n_bounds, 2LL});
        };
        if (engine().world.features_sh_quant8_fpbo.initialized()) {
            pick(engine().world.features_sh_quant8_fpbo);
            sh_bounds_stride = 0;  // FPBO layout
        } else {
            pick(engine().world.features_sh_quant8);
            sh_bounds_stride = 256;
        }
    } else if (sh_value_bits == 16) {
        auto pick = [&](auto& vq) {
            if (!vq.initialized()) return;
            sh_value_packed_opt = TorchTensorView(
                (uint64_t)vq.packed_ptr(), 1, {vq.packed_bytes()});
            sh_value_bounds_opt = TorchTensorView(
                (uint64_t)vq.bounds_ptr(), 4, {vq.n_bounds, 2LL});
        };
        if (engine().world.features_sh_quant16_fpbo.initialized()) {
            pick(engine().world.features_sh_quant16_fpbo);
            sh_bounds_stride = 0;  // FPBO layout
        } else {
            pick(engine().world.features_sh_quant16);
            sh_bounds_stride = 256;
        }
    }

    // --- Projection ---
    if (packed) {
        DeviceVector<int32_t> cam_ids, gauss_ids;
        DeviceVector<uint2> aabb_vec;
        DeviceVector<float> depths_vec;

        if (primitive == "3dgs") {
            auto [a, b, c, d, e] = projection_3dgs_packed_forward(
                engine().cur_num_splats, sh_degree, in_splats,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().optim.radii,
                sh_value_packed_opt, sh_value_bounds_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            cam_ids = a; gauss_ids = b; aabb_vec = c; depths_vec = d;
            engine().fwd.splats_s = e;
        } else if (primitive == "mip") {
            auto [a, b, c, d, e] = projection_mip_packed_forward(
                engine().cur_num_splats, sh_degree, in_splats,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().optim.radii,
                sh_value_packed_opt, sh_value_bounds_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            cam_ids = a; gauss_ids = b; aabb_vec = c; depths_vec = d;
            engine().fwd.splats_s = e;
        } else if (primitive == "3dgut") {
            auto [a, b, c, d, e] = projection_3dgut_packed_forward(
                engine().cur_num_splats, sh_degree, in_splats,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().optim.radii,
                sh_value_packed_opt, sh_value_bounds_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            cam_ids = a; gauss_ids = b; aabb_vec = c; depths_vec = d;
            engine().fwd.splats_s = e;
        } else {
            throw std::runtime_error("engine_forward: unknown primitive: " + primitive);
        }

        engine().fwd.camera_ids = cam_ids;
        engine().fwd.gaussian_ids = gauss_ids;
        engine().fwd.aabb = vec_to_2d_aabb(aabb_vec);  // [nnz, 1] view for backward
        aabb_q = engine().fwd.aabb;
        depths_nd = DeviceTensorFloatND(depths_vec);      // [nnz]   for intersect
    } else {
        DeviceTensor2D<uint2> aabb_2d;
        DeviceTensor2D<float> depths_2d;  // [C, N] — sorting depths per camera

        if (primitive == "3dgs") {
            auto [a, b, c] = projection_3dgs_forward(
                engine().cur_num_splats, sh_degree, in_splats,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().optim.radii,
                sh_value_packed_opt, sh_value_bounds_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            aabb_2d = a; depths_2d = b;
            engine().fwd.splats_s = c;
        } else if (primitive == "mip") {
            auto [a, b, c] = projection_mip_forward(
                engine().cur_num_splats, sh_degree, in_splats,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().optim.radii,
                sh_value_packed_opt, sh_value_bounds_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            aabb_2d = a; depths_2d = b;
            engine().fwd.splats_s = c;
        } else if (primitive == "3dgut") {
            auto [a, b, c] = projection_3dgut_forward(
                engine().cur_num_splats, sh_degree, in_splats,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().optim.radii,
                sh_value_packed_opt, sh_value_bounds_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            aabb_2d = a; depths_2d = b;
            engine().fwd.splats_s = c;
        } else {
            throw std::runtime_error("engine_forward: unknown primitive: " + primitive);
        }

        engine().fwd.camera_ids = DeviceVector<int32_t>();
        engine().fwd.gaussian_ids = DeviceVector<int32_t>();
        engine().fwd.aabb = aabb_2d;                           // [C, N] for backward
        aabb_q = aabb_2d;
        depths_nd = DeviceTensorFloatND(depths_2d);            // [C, N] -> numel=C*N for intersect
    }

    // --- Tile intersection (ellipse mode) ---
    DeviceVector<int32_t>* image_ids_ptr = nullptr;
    if (packed && engine().fwd.camera_ids.data_ptr() != nullptr)
        image_ids_ptr = &engine().fwd.camera_ids;

    // Handing the projected conic to the intersector buys the tighter
    // ellipse-vs-tile test instead of the conservative AABB one.
    ProjEllipseView ellipse = proj_ellipse_view(
        engine().fwd.splats_s[0].data_ptr(), primitive == "3dgut");

    auto [isect_ids, flatten_ids, tile_offsets] = do_intersect_tile_generic(
        aabb_q, depths_nd, ellipse,
        (uint32_t)engine().camera.num,
        _dv_tv(engine().camera.intrins),
        (uint32_t)engine().camera.width,
        (uint32_t)engine().camera.height,
        image_ids_ptr,
        engine().fwd.tile_active.data_ptr()
    );

    engine().fwd.tile_offsets = tile_offsets;
    engine().fwd.flatten_ids = flatten_ids;
    // One shot: whoever wants tiles skipped sets the map immediately before
    // the forward, so a later preview or viewer render cannot inherit it.
    engine().fwd.tile_active = DeviceVector<int32_t>();

    // --- Rasterization forward ---
    RenderOutput::TensorTuple renders;
    RenderOutput::TensorTuple distortions;
    DeviceTensor3D<float> render_Ts;
    DeviceTensor3D<int32_t> last_ids;
    DeviceTensor3D<float> render_median;

    if (primitive == "3dgs") {
        auto [r, rTs, lids, dist, med] = rasterize_to_pixels_3dgs_fwd(
            engine().cur_num_splats,
            in_splats, engine().fwd.splats_s, engine().fwd.gaussian_ids,
            (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
            tile_offsets, flatten_ids, dist_type, output_median);
        renders = r; render_Ts = rTs; last_ids = lids; render_median = med; distortions = dist;
    } else if (primitive == "mip") {
        auto [r, rTs, lids, dist, med] = rasterize_to_pixels_mip_fwd(
            engine().cur_num_splats,
            in_splats, engine().fwd.splats_s, engine().fwd.gaussian_ids,
            (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
            tile_offsets, flatten_ids, dist_type, output_median);
        renders = r; render_Ts = rTs; last_ids = lids; render_median = med; distortions = dist;
    } else if (primitive == "3dgut") {
        auto [r, rTs, lids, dist, med] = rasterize_to_pixels_3dgut_fwd(
            engine().cur_num_splats,
            in_splats, engine().fwd.splats_s, engine().fwd.gaussian_ids,
            _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
            engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
            engine().fwd.aabb,
            (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
            tile_offsets, flatten_ids, dist_type, output_median);
        renders = r; render_Ts = rTs; last_ids = lids; render_median = med; distortions = dist;
    }

    engine().fwd.renders = renders;
    engine().fwd.distortions = distortions;
    engine().fwd.dist_type = dist_type;
    engine().fwd.render_Ts = render_Ts;
    engine().fwd.render_median = render_median;
    engine().fwd.last_ids = last_ids;

    // Background blend (in-place on fwd.renders.rgb). No-op when no
    // engine_init_background_* was called. Folded into the forward path so
    // both training (via engine_train_step) and eval/viewer renders (via
    // direct calls to engine_forward_3dgs) see the blend. The blend reads
    // its per-iter (seed, randomize_weight) from engine state — training
    // sets them via engine_set_background_step_params before each call.
    if (engine().background.enabled) {
        _engine_background_forward();
    }

    // Linear / wide-gamut -> sRGB. Done AFTER the background blend so the
    // blend operates in the splat working color space (matches Python
    // forward order: render -> bg -> rgb_to_srgb -> bilagrid -> PPISP -> loss).
    _engine_color_space_forward();

    // Results stay in pool — use engine_copy_render_to_host to fetch
}


void engine_debug_forward(
    TorchTensorView override_features_dc,
    int override_sh_degree,
    TorchTensorView out_rgb
) {
    if (std::get<0>(engine().fwd.renders).data_ptr() == nullptr)
        throw std::runtime_error("engine_debug_forward: forward_3dgs must be called first");

    // Swap in overrides (H->D copy for host tensor). Also temporarily disable
    // the background blend — debug renders should show the raw rendered RGB
    // (black where Ts > 0) rather than compositing over the trained skybox /
    // randomized noise.
    DeviceVector<float3> saved_dc = engine().world.features_dc;
    int  saved_sh = engine().sh_degree;
    bool saved_bg_enabled = engine().background.enabled;
    if (std::get<0>(override_features_dc) != 0)
        engine().world.features_dc = _hv_to_dv<float3>(PoolSlot::DebugFeaturesDc, override_features_dc);
    if (override_sh_degree >= 0)
        engine().sh_degree = override_sh_degree;
    engine().background.enabled = false;

    // Restore even when the forward throws: leaving the debug features_dc
    // swapped in would fail every subsequent training step (tensor size
    // mismatch) and kill the session over a bad debug render.
    try {
        forward_3dgs(engine().primitive, engine().sh_degree, engine().packed);

        // Copy rgb result D->H
        auto& rgb = std::get<0>(engine().fwd.renders);
        if (rgb.data_ptr() && std::get<0>(out_rgb) != 0) {
            backend::memcpy_sync((void*)std::get<0>(out_rgb), rgb.data_ptr(),
                       rgb.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
        }
    } catch (...) {
        engine().world.features_dc   = saved_dc;
        engine().sh_degree           = saved_sh;
        engine().background.enabled  = saved_bg_enabled;
        throw;
    }

    // Restore
    engine().world.features_dc   = saved_dc;
    engine().sh_degree           = saved_sh;
    engine().background.enabled  = saved_bg_enabled;
}
