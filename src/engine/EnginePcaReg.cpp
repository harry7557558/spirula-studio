// The local-PCA regularizer, run between the backward and the optim step:
// find each splat's neighbours the configured way, then add the gradient of
// L_PCA and L_normal where the optim step reads it. docs/notes/pca-reg.md.

#include "engine/Engine.h"
#include "backend/api/Loss.h"
#include "core/Env.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <tuple>

namespace {

bool knn_cached(PcaRegKnn m) {
    return m == PcaRegKnn::ExactCached || m == PcaRegKnn::MortonCached ||
           m == PcaRegKnn::MortonBestCached;
}

// SS_PCA_REG_TIMING=<steps>: device time of the regularizer's parts, averaged
// over that many steps, in English like the other deep diagnostics.
struct PartTimer {
    int every = 0;
    int steps = 0;
    double ms[3] = {0, 0, 0};
    backend::Event* ev[4] = {};

    PartTimer() {
        const char* v = spirula::env("PCA_REG_TIMING");
        every = v ? std::max(0, std::atoi(v)) : 0;
        if (every)
            for (auto*& e : ev) e = backend::event_create(true);
    }
    void mark(int i) {
        if (every) backend::event_record(ev[i]);
    }
    void finish(int64_t n, int k, PcaRegKnn knn, bool rebuilt) {
        if (!every) return;
        backend::event_synchronize(ev[3]);
        for (int i = 0; i < 3; i++) ms[i] += backend::event_elapsed_ms(ev[i], ev[i + 1]);
        if (++steps < every) return;
        std::fprintf(stderr,
                     "[pca-reg] n=%lld k=%d knn=%d last=%s: order %.3f ms, tree %.3f ms, "
                     "neighbourhoods %.3f ms per step over %d steps\n",
                     (long long)n, k, (int)knn, rebuilt ? "rebuild" : "reuse", ms[0] / steps,
                     ms[1] / steps, ms[2] / steps, steps);
        steps = 0;
        ms[0] = ms[1] = ms[2] = 0;
    }
};

}  // namespace

void engine_pca_reg_step(int step, const PcaRegConfig& cfg,
                         std::map<std::string, float>& loss_dict) {
    auto& e = engine();
    auto& s = e.pca_reg;
    s.v_means = DeviceVector<float3>();
    s.v_quats = DeviceVector<float4>();
    s.v_scales = DeviceVector<float3>();
    const int64_t n = e.cur_num_splats;
    if (cfg.neighbors <= 0 || n < 3 ||
        (cfg.w_pca == 0.0f && cfg.w_normal == 0.0f && cfg.w_surfel == 0.0f))
        return;
    const int k = (int)std::min<int64_t>(
        {(int64_t)cfg.neighbors, (int64_t)kPcaRegMaxNeighbors - 1, n - 1});

    const bool fused = e.optim.use_fused_proj_bwd_optim;
    if (!fused && e.grad.quantize_grad)
        throw std::runtime_error(
            "pca_reg: the quantized-gradient path (quantization_level > 0 without the "
            "fused projection-backward optimizer) has no fp32 buffer to add into");
    // The fused 3dgs / mip step reads world gradients only from what it is
    // handed; everywhere else they accumulate into engine().grad, which the
    // optim step scales by grad_scale.
    const bool own_grad = fused && e.grad.means.data_ptr() == nullptr;
    const float out_scale = fused ? 1.0f : 1.0f / e.optim.grad_scale;
    const float inv_n = out_scale / (float)n;
    const float w_plane = cfg.w_pca * inv_n;
    const float w_tangent = cfg.w_pca * cfg.tangent * inv_n;
    const float w_normal = cfg.w_normal * inv_n;
    const float w_surfel = cfg.w_surfel * inv_n;

    // The backward leaves RasterBwd open with the fused step's screen
    // gradients in it; these slices join that phase (POOL_ALIAS_TABLE).
    if (DevicePool::global().phase() != PoolPhase::RasterBwd)
        pool_begin_phase(PoolPhase::RasterBwd);

    SplatStageTimer stage_timer;
    static PartTimer timer;
    timer.mark(0);

    DeviceVector<float3> v_means = e.grad.means;
    DeviceVector<float4> v_quats = w_normal != 0.0f ? e.grad.quats : DeviceVector<float4>();
    DeviceVector<float3> v_scales = w_surfel != 0.0f ? e.grad.scales : DeviceVector<float3>();
    if (own_grad) {
        s.v_means.resize(PoolSlot::PcaRegVMeans, n);
        backend::memset_async(s.v_means.data_ptr(), 0, n * sizeof(float3), backend::kDefaultStream);
        if (w_normal != 0.0f) {
            s.v_quats.resize(PoolSlot::PcaRegVQuats, n);
            backend::memset_async(s.v_quats.data_ptr(), 0, n * sizeof(float4),
                                  backend::kDefaultStream);
        }
        if (w_surfel != 0.0f) {
            s.v_scales.resize(PoolSlot::PcaRegVScales, n);
            backend::memset_async(s.v_scales.data_ptr(), 0, n * sizeof(float3),
                                  backend::kDefaultStream);
        }
        v_means = s.v_means;
        v_quats = s.v_quats;
        v_scales = s.v_scales;
    }

    const bool cached = knn_cached(cfg.knn);
    const bool lists = cfg.knn == PcaRegKnn::ExactCached || cfg.knn == PcaRegKnn::MortonBestCached;
    const bool rebuild = !cached || s.cache_n != n || s.cache_k != k ||
                         s.cache_knn != (int)cfg.knn || s.cache_epoch != s.splat_epoch ||
                         step < s.cache_step ||
                         step - s.cache_step >= std::max(cfg.refresh_every, 1);

    DeviceVector<int32_t> perm, nbrs, ranges;
    DeviceVector<float4> sorted, nodes;
    DeviceVector<int4> meta;
    DeviceVector<uint32_t> box;
    if (cached) {
        s.perm.resize(PoolSlot::PcaRegPermCache, n);
        perm = s.perm;
    }
    if (lists) {
        s.nbrs.resize(PoolSlot::PcaRegNbrsCache, n * k);
        nbrs = s.nbrs;
    }
    if (rebuild)
        std::tie(perm, sorted, box) =
            pca_reg_order_tensor(n, e.world.means, cached ? s.perm : DeviceVector<int32_t>());
    else if (cfg.knn == PcaRegKnn::MortonCached)
        sorted = pca_reg_gather_tensor(n, e.world.means, perm);
    timer.mark(1);

    PcaRegMode mode = PcaRegMode::List;
    if (cfg.knn == PcaRegKnn::Morton || cfg.knn == PcaRegKnn::MortonCached) {
        mode = PcaRegMode::Window;
    } else if (rebuild) {
        const bool exact = cfg.knn == PcaRegKnn::Exact || cfg.knn == PcaRegKnn::ExactCached;
        mode = exact ? PcaRegMode::Tree : PcaRegMode::Best;
        if (exact) std::tie(nodes, meta, ranges) = pca_reg_tree_tensor(n, sorted, box);
    }
    timer.mark(2);
    if (cached && rebuild) {
        s.cache_n = n;
        s.cache_k = k;
        s.cache_knn = (int)cfg.knn;
        s.cache_step = step;
        s.cache_epoch = s.splat_epoch;
    }

    DeviceVector<float> sums;
    sums.resize(PoolSlot::PcaRegLossSums, 3);
    backend::memset_async(sums.data_ptr(), 0, 3 * sizeof(float), backend::kDefaultStream);
    const int window = cfg.window > 0 ? cfg.window : 4 * (k + 1);
    pca_reg_neighborhood_tensor(n, (int)mode, k, window, e.world.means, e.world.quats,
                                e.world.scales, perm, sorted, nodes, meta, ranges, nbrs,
                                lists && rebuild, e.optim.radii, cfg.rendered_means_only,
                                cfg.scale_agnostic, v_means, v_quats, v_scales, w_plane,
                                w_tangent, w_normal, w_surfel, sums);
    timer.mark(3);
    timer.finish(n, k, cfg.knn, rebuild);

    static AsyncReadout<float> readout(3);
    static int64_t readout_n = 1;
    if (const float* h = readout.read_previous()) {
        loss_dict["pca_lambda1"] = h[0] / (float)readout_n;
        loss_dict["pca_tangent"] = h[1] / (float)readout_n;
        loss_dict["pca_normal"] = h[2] / (float)readout_n;
    }
    readout.issue(sums.data_ptr());
    readout_n = n;
}
