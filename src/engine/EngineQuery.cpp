// Read-only / cheap query entrypoints (D->H copies for inspection).

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"
#include "backend/common/Profiler.h"
#include "core/HalfFloat.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <vector>


void engine_copy_accum_buffer(TorchTensorView dst) {
    // The preview shows what densification samples, so it follows the
    // final-score-power side buffer whenever that power is in play.
    const DeviceVector<float2>& src =
        engine().optim.densify_sample_score.data_ptr()
            ? engine().optim.densify_sample_score
            : engine().optim.accum_buffer;
    if (src.data_ptr() == nullptr || std::get<0>(dst) == 0)
        return;
    int64_t dst_n = std::get<2>(dst)[0];
    int64_t src_n = src.size();
    int64_t n = std::min(dst_n, src_n);
    backend::memcpy_sync((void*)std::get<0>(dst), src.data_ptr(),
               n * sizeof(float2), backend::MemcpyKind::DeviceToHost);
}


int64_t engine_get_cur_num_splats() {
    return engine().cur_num_splats;
}

int64_t engine_get_max_num_splats() {
    return engine().max_num_splats;
}

int engine_get_num_sh() {
    return engine().num_sh;
}


void engine_copy_render_to_host(
    TorchTensorView out_rgb,
    TorchTensorView out_depth,
    TorchTensorView out_Ts,
    TorchTensorView out_rgb_raw,
    TorchTensorView out_median
) {
    auto& renders = engine().fwd.renders;
    auto& rgb = std::get<0>(renders);
    auto& depth = std::get<1>(renders);
    if (rgb.data_ptr() && std::get<0>(out_rgb) != 0 &&
        engine().fwd.rgb_fmt == PixelFormat::F16) {
        std::vector<uint16_t> h((size_t)rgb.numel() * 3);
        backend::memcpy_sync(h.data(), rgb.data_ptr(), h.size() * sizeof(uint16_t),
                             backend::MemcpyKind::DeviceToHost);
        const float* half_to_float = spirula::half_to_float_table();
        float* dst = (float*)std::get<0>(out_rgb);
        for (size_t i = 0; i < h.size(); i++) dst[i] = half_to_float[h[i]];
    } else if (rgb.data_ptr() && std::get<0>(out_rgb) != 0) {
        backend::memcpy_sync((void*)std::get<0>(out_rgb), rgb.data_ptr(),
                   rgb.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
    }
    if (depth.data_ptr() && std::get<0>(out_depth) != 0) {
        backend::memcpy_sync((void*)std::get<0>(out_depth), depth.data_ptr(),
                   depth.numel() * sizeof(float), backend::MemcpyKind::DeviceToHost);
    }
    if (engine().fwd.render_Ts.data_ptr() && std::get<0>(out_Ts) != 0) {
        backend::memcpy_sync((void*)std::get<0>(out_Ts), engine().fwd.render_Ts.data_ptr(),
                   engine().fwd.render_Ts.numel() * sizeof(float), backend::MemcpyKind::DeviceToHost);
    }
    // The pre-encode (linear / wide-gamut) render: kept in cs.fwd_pre by the
    // per-stage path, redone here for the fused one. Left unwritten without a
    // color space; the caller then mirrors out_rgb.
    auto& cs = engine().color_space;
    DeviceTensor3D<float3> pre = cs.fwd_pre;
    const auto& app = engine().appearance;
    if (std::get<0>(out_rgb_raw) != 0 && pre.data_ptr() == nullptr &&
        app.fused && app.params.cs_enabled) {
        // A training step's loss may have put v_rgb in the float32 raw render.
        const bool half = engine().fwd.raw_rgb16.data_ptr() != nullptr;
        pre = half ? engine().fwd.raw_rgb16 : engine().fwd.raw_rgb;
        if (app.params.bg != AppearanceBg::None || half) {
            AppearanceChainParams p = app.params;
            p.cs_enabled = 0;
            p.ppisp = AppearancePpisp::Off;
            DeviceTensor3D<float3> lin;
            lin.resize(PoolSlot::EngAppearanceLinear, pre.size<0>(), pre.size<1>(),
                       pre.size<2>());
            appearance_chain_forward(
                p, _engine_image_view(pre, half ? PixelFormat::F16 : PixelFormat::F32),
                engine().fwd.render_Ts, _engine_image_view(lin, PixelFormat::F32),
                _tv_null());
            pre = lin;
        }
    }
    if (std::get<0>(out_rgb_raw) != 0 && pre.data_ptr() != nullptr) {
        backend::memcpy_sync((void*)std::get<0>(out_rgb_raw), pre.data_ptr(),
                   pre.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
    }
    if (engine().fwd.render_median.data_ptr() && std::get<0>(out_median) != 0) {
        backend::memcpy_sync((void*)std::get<0>(out_median), engine().fwd.render_median.data_ptr(),
                   engine().fwd.render_median.numel() * sizeof(float), backend::MemcpyKind::DeviceToHost);
    }
}


// Per-pixel distortion image D = W*S - C^2 from the most recent forward
// (populated only when output_distortion was requested). RGB_D primitives
// emit rgb + depth only; normal is left empty. Mirrors
// engine_copy_render_to_host. Null out_* args are skipped.
void engine_copy_distortion_to_host(
    TorchTensorView out_rgb_dist,    // [C, H, W, 3] float32, CPU, optional
    TorchTensorView out_depth_dist   // [C, H, W, 1] float32, CPU, optional
) {
    auto& dist = engine().fwd.distortions;
    auto& rgb = std::get<0>(dist);
    auto& depth = std::get<1>(dist);
    if (rgb.data_ptr() && std::get<0>(out_rgb_dist) != 0) {
        backend::memcpy_sync((void*)std::get<0>(out_rgb_dist), rgb.data_ptr(),
                   rgb.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
    }
    if (depth.data_ptr() && std::get<0>(out_depth_dist) != 0) {
        backend::memcpy_sync((void*)std::get<0>(out_depth_dist), depth.data_ptr(),
                   depth.numel() * sizeof(float), backend::MemcpyKind::DeviceToHost);
    }
}


void engine_copy_splats_to_host(
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
) {
    _dv_to_host(engine().world.means, means);
    _dv_to_host(engine().world.quats, quats);
    _dv_to_host(engine().world.scales, scales);
    _dv_to_host(engine().world.opacities, opacities);
    _dv_to_host(engine().world.features_dc, features_dc);
    // features_sh: DeviceTensor2D<float3>
    if (engine().world.features_sh.data_ptr() && std::get<0>(features_sh) != 0) {
        backend::memcpy_sync((void*)std::get<0>(features_sh), engine().world.features_sh.data_ptr(),
                   engine().world.features_sh.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
    }
}


// Per-splat gradients accumulated by the most recent engine backward
// (engine_compute_loss_backward or engine_backward_from_render_grad). Buffers
// are sized max_num_splats and zeroed at the start of each backward. Mirrors
// engine_copy_splats_to_host but reads engine().grad.* instead of world.*.
void engine_copy_grads_to_host(
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
) {
    _dv_to_host(engine().grad.means, means);
    _dv_to_host(engine().grad.quats, quats);
    _dv_to_host(engine().grad.scales, scales);
    _dv_to_host(engine().grad.opacities, opacities);
    _dv_to_host(engine().grad.features_dc, features_dc);
    // features_sh: DeviceTensor2D<float3>
    if (engine().grad.features_sh.data_ptr() && std::get<0>(features_sh) != 0) {
        backend::memcpy_sync((void*)std::get<0>(features_sh), engine().grad.features_sh.data_ptr(),
                   engine().grad.features_sh.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
    }
}


// ---------------------------------------------------------------------------
// Debug introspection helpers. These pull the current engine training-data
// + rendered images back to host so a Python utility can save / display them
// for visual debugging (especially useful for the warp-to-pinhole path
// where the on-engine GT is generated on GPU and never seen by Python).
// ---------------------------------------------------------------------------

// Shape getters: callers use these to size the host buffer before calling
// the copy. Returns (B, H, W, C); zeros when the buffer is empty.
std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_rgb_shape() {
    const TorchTensorView& t = engine().gt.rgb;
    if (std::get<0>(t) == 0) return {0, 0, 0, 0};
    const auto& s = std::get<2>(t);
    return {s[0], s[1], s[2], 3LL};
}

std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_alpha_shape() {
    auto& t = engine().gt.alpha;
    if (t.data_ptr() == nullptr) return {0, 0, 0, 0};
    return {t.template size<0>(), t.template size<1>(), t.template size<2>(), 1LL};
}

std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_render_rgb_shape() {
    auto& t = std::get<0>(engine().fwd.renders);
    if (t.data_ptr() == nullptr) return {0, 0, 0, 0};
    return {t.template size<0>(), t.template size<1>(), t.template size<2>(), 3LL};
}

// The host buffer is float; an 8-bit GT is widened here, as the loss reads it.
void engine_copy_gt_rgb_to_host(TorchTensorView out) {
    const TorchTensorView& t = engine().gt.rgb;
    if (std::get<0>(t) == 0 || std::get<0>(out) == 0) return;
    size_t n = 1;
    for (int64_t d : std::get<2>(t)) n *= (size_t)d;
    float* dst = (float*)std::get<0>(out);
    if (std::get<1>(t) == 4) {
        backend::memcpy_sync(dst, (const void*)std::get<0>(t), n * sizeof(float),
                             backend::MemcpyKind::DeviceToHost);
        return;
    }
    std::vector<uint8_t> bytes(n);
    backend::memcpy_sync(bytes.data(), (const void*)std::get<0>(t), n,
                         backend::MemcpyKind::DeviceToHost);
    for (size_t i = 0; i < n; ++i) dst[i] = (float)bytes[i] / 255.0f;
}

void engine_copy_gt_alpha_to_host(TorchTensorView out) {
    auto& t = engine().gt.alpha;
    if (t.data_ptr() == nullptr || std::get<0>(out) == 0) return;
    backend::memcpy_sync((void*)std::get<0>(out), t.data_ptr(),
               t.numel() * sizeof(uint8_t), backend::MemcpyKind::DeviceToHost);
}

std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_depth_shape() {
    auto& t = engine().gt.depth;
    if (t.data_ptr() == nullptr) return {0, 0, 0, 0};
    return {t.template size<0>(), t.template size<1>(), t.template size<2>(), 1LL};
}

std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_normal_shape() {
    auto& t = engine().gt.normal;
    if (t.data_ptr() == nullptr) return {0, 0, 0, 0};
    return {t.template size<0>(), t.template size<1>(), t.template size<2>(), 3LL};
}

void engine_copy_gt_depth_to_host(TorchTensorView out) {
    auto& t = engine().gt.depth;
    if (t.data_ptr() == nullptr || std::get<0>(out) == 0) return;
    backend::memcpy_sync((void*)std::get<0>(out), t.data_ptr(),
               t.numel() * sizeof(float), backend::MemcpyKind::DeviceToHost);
}

void engine_copy_gt_normal_to_host(TorchTensorView out) {
    auto& t = engine().gt.normal;
    if (t.data_ptr() == nullptr || std::get<0>(out) == 0) return;
    backend::memcpy_sync((void*)std::get<0>(out), t.data_ptr(),
               t.numel() * sizeof(float3), backend::MemcpyKind::DeviceToHost);
}

void engine_copy_render_depth_normal_to_host(TorchTensorView out) {
    auto& depth = std::get<1>(engine().fwd.renders);
    if (depth.data_ptr() == nullptr || std::get<0>(out) == 0) return;
    const int64_t C = depth.template size<0>();
    const int64_t H = depth.template size<1>();
    const int64_t W = depth.template size<2>();
    TorchTensorView normal = _pool_tv(PoolSlot::EngDepthNormal, C, H, W, 3);
    // is_ray_depth is what EngineLoss.cpp passes; the rasterizer renders ray
    // depth whatever the primitive.
    depth_to_normal_forward(
        engine().camera.model_str, engine().camera.distortion_str,
        _dv_tv(engine().camera.intrins), _dt2d_tv(engine().camera.dist_coeffs),
        /*is_ray_depth=*/true, DeviceTensor3D<float>(_dt3d_tv(depth)),
        DeviceTensor3D<float3>(normal));
    backend::memcpy_sync((void*)std::get<0>(out),
                         (void*)(uintptr_t)std::get<0>(normal),
                         (size_t)(C * H * W) * sizeof(float3),
                         backend::MemcpyKind::DeviceToHost);
}


std::vector<std::tuple<std::string, size_t, size_t>> engine_get_pool_breakdown() {
    return DevicePool::global().getBreakdown();
}

// Same buffers as engine_get_pool_breakdown(), but each row also carries its
// VRAM category as a string ("splat", "splat x img", "image", "appearance",
// "viewer", "other") sourced from the pool-slot metadata table -- so Python
// buckets by an authoritative tag instead of guessing from the key prefix.
std::vector<std::tuple<std::string, std::string, size_t, size_t>>
engine_get_pool_breakdown_categorized() {
    auto raw = DevicePool::global().getBreakdownCategorized();
    std::vector<std::tuple<std::string, std::string, size_t, size_t>> out;
    out.reserve(raw.size());
    for (auto& r : raw)
        out.emplace_back(std::get<0>(r), to_string((VramCategory)std::get<1>(r)),
                         std::get<2>(r), std::get<3>(r));
    return out;
}

size_t engine_get_scratch_bytes() {
    return DeviceScratch::global().capBytes();
}


// ===========================================================================
// SS_PROFILE VRAM report
// ===========================================================================

std::string engine_vram_report() {
    // Arena-backed rows own nothing, so their cap is 0 and the arena's one
    // allocation is a line of its own; the totals still add up.
    auto rows = DevicePool::global().breakdown();
    auto arena = DevicePool::global().arenaStats();
    const int kNCat = (int)VramCategory::Count;
    size_t used[kNCat] = {}, cap[kNCat] = {}, count[kNCat] = {};
    size_t used_total = 0, cap_total = 0;
    for (const auto& r : rows) {
        int c = (int)r.cat;
        count[c]++;
        used[c] += r.used_bytes;
        cap[c]  += r.cap_bytes;
        used_total += r.used_bytes;
        cap_total  += r.cap_bytes;
    }

    std::string out;
    char tmp[512];
    auto add = [&](const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(tmp, sizeof tmp, fmt, ap);
        va_end(ap);
        out += tmp;
    };
    auto mib = [](size_t bytes) { return (double)bytes / (1024.0 * 1024.0); };

    add("\n[spirula-profile] ---- VRAM breakdown (pool high-water) ----\n");
    add("%-30s %11s %11s %11s\n", "category", "buffers", "used_MiB", "cap_MiB");
    for (int c = 0; c < kNCat; c++) {
        if (count[c] == 0) continue;
        add("%-30s %11zu %11.2f %11.2f\n", to_string((VramCategory)c), count[c],
            mib(used[c]), mib(cap[c]));
    }
    size_t scratch = engine_get_scratch_bytes();
    add("%-30s %11zu %11.2f %11.2f\n", "  pool total", rows.size(),
        mib(used_total), mib(cap_total + arena.cap_bytes));
    if (arena.cap_bytes || arena.slots)
        add("%-30s %11zu %11s %11.2f\n", "  of which alias arena", arena.slots,
            "-", mib(arena.cap_bytes));
    add("%-30s %11s %11s %11.2f\n", "  scratch buffer", "-", "-", mib(scratch));
    add("%-30s %11s %11s %11.2f\n", "  pool + scratch", "", "",
        mib(cap_total + arena.cap_bytes + scratch));

    // The driver's figures place the pool against everything else the process
    // holds: staging buffers, backend allocations, NN models, GUI surfaces.
    backend::MemoryUsage mem = backend::memory_usage();
    if (mem.has_process)
        add("%-30s %11s %11s %11.2f\n", "  process (driver)", "", "",
            mib(mem.process_bytes));
    if (mem.has_used && mem.has_total)
        add("%-30s %11s %11s %11.2f / %.2f\n", "  device in use / total", "",
            "", mib(mem.used_bytes), mib(mem.total_bytes));

    // Aliased rows own nothing, so rank on whichever of the two is bigger.
    auto weight = [](const auto& r) {
        return std::max(r.used_bytes, r.cap_bytes);
    };
    std::sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
        return weight(a) > weight(b);
    });
    add("%-30s %11s %11s %11s\n", "individual buffers", "category", "used_MiB",
        "cap_MiB");
    for (const auto& r : rows) {
        if (weight(r) == 0) break;
        const char* cat = to_string(r.cat);
        if (r.arena)
            add("  %-28s %11s %11.2f %11s\n", r.name.c_str(), cat,
                mib(r.used_bytes), "arena");
        else
            add("  %-28s %11s %11.2f %11.2f\n", r.name.c_str(), cat,
                mib(r.used_bytes), mib(r.cap_bytes));
    }
    add("[spirula-profile] --------------------------\n");
    return out;
}

void engine_profile_capture_vram() {
    if (backend::prof::enabled())
        backend::prof::set_exit_note(engine_vram_report());
}
