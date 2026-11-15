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

// A probe pass runs on at most this many splats: enough that the gathers miss
// the caches as a full pass does, and under 3 ms on a desktop GPU.
constexpr int64_t kProbeSplats = 1 << 20;

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

constexpr size_t kWorkAlign = 256;
size_t work_span(size_t bytes) { return (bytes + kWorkAlign - 1) & ~(kWorkAlign - 1); }

// How one pass over n splats uses pca_reg.work: the order at its head with the
// sort's scratch behind it, then, over that scratch once the sort is done, the
// centres by rank and the gradients the optim step is handed.
struct Plan {
    PcaRegMode mode = PcaRegMode::List;
    bool sort = false, sorted = false, store = false;
    bool grad[4] = {};          // means, quats, scales, opacities
    size_t sorted_at = 0, grad_at[4] = {}, total = 0;
};

Plan make_plan(const PcaRegConfig& cfg, int64_t n, int k, bool rebuild, const bool grad[4]) {
    Plan p;
    const bool exact = cfg.knn == PcaRegKnn::Exact || cfg.knn == PcaRegKnn::ExactCached;
    if (cfg.knn == PcaRegKnn::Morton || cfg.knn == PcaRegKnn::MortonCached)
        p.mode = PcaRegMode::Window;
    else if (rebuild)
        p.mode = exact ? PcaRegMode::Tree : PcaRegMode::Best;
    p.sort = rebuild;
    p.store = rebuild && (cfg.knn == PcaRegKnn::ExactCached || cfg.knn == PcaRegKnn::MortonBestCached);
    const int window = cfg.window > 0 ? cfg.window : 4 * (k + 1);
    p.sorted = p.mode != PcaRegMode::List &&
               !pca_reg_uses_slab((int)p.mode, k, window, cfg.w_transparency);
    size_t at = p.sort ? work_span((size_t)n * sizeof(int32_t)) : 0;
    if (p.sorted) {
        p.sorted_at = at;
        at += work_span((size_t)n * sizeof(float4));
    }
    const size_t size[4] = {sizeof(float3), sizeof(float4), sizeof(float3), sizeof(float)};
    for (int i = 0; i < 4; i++) {
        p.grad[i] = grad[i];
        if (!grad[i]) continue;
        p.grad_at[i] = at;
        at += work_span((size_t)n * size[i]);
    }
    p.total = std::max(at, p.sort ? pca_reg_sort_bytes(n) : (size_t)0);
    return p;
}

// The exact search's tree, beside the work buffer.
size_t tree_bytes(int64_t n) {
    return work_span(2 * (size_t)(n - 1) * sizeof(float4)) + work_span((size_t)(n - 1) * sizeof(int4)) +
           work_span((size_t)n * sizeof(int32_t));
}

template <class T>
DeviceVector<T> work_view(uint8_t* base, size_t at, int64_t count) {
    return DeviceVector<T>(TorchTensorView((uint64_t)(base + at), (uint32_t)sizeof(T), {count, 1}));
}

struct Pass {
    int64_t n = 0;
    int k = 0;
    float scale = 1.0f;                 // on every weight, besides 1 / n
    DeviceVector<int32_t> perm, nbrs;   // the caches, or empty
    DeviceVector<int32_t> keep;         // receives a rebuilt order
    DeviceVector<float3> v_means;       // where a gradient the plan does not own goes
    DeviceVector<float4> v_quats;
    DeviceVector<float3> v_scales;
    DeviceVector<float> v_opacities;
    DeviceVector<float> sums;
};

// The regularizer over the first `pass.n` splats, its scratch at `base`.
void run_pass(const PcaRegConfig& cfg, const Plan& plan, uint8_t* base, Pass& pass,
              PartTimer* timer) {
    auto& e = engine();
    const int64_t n = pass.n;
    const float inv_n = pass.scale / (float)n;
    if (timer) timer->mark(0);
    DeviceVector<int32_t> perm = pass.perm, ranges;
    DeviceVector<float4> sorted, nodes;
    DeviceVector<int4> meta;
    DeviceVector<uint32_t> box;
    if (plan.sorted) sorted = work_view<float4>(base, plan.sorted_at, n);
    if (plan.sort)
        std::tie(perm, box) = pca_reg_order_tensor(
            n, e.world.means, work_view<int32_t>(base, 0, 6 * n), sorted, pass.keep);
    else if (plan.sorted)
        pca_reg_gather_tensor(n, e.world.means, perm, sorted);
    if (timer) timer->mark(1);
    if (plan.mode == PcaRegMode::Tree)
        std::tie(nodes, meta, ranges) = pca_reg_tree_tensor(n, sorted, box);
    if (timer) timer->mark(2);

    // Over the sort's scratch, so only once the order is made.
    if (plan.grad[0]) pass.v_means = work_view<float3>(base, plan.grad_at[0], n);
    if (plan.grad[1]) pass.v_quats = work_view<float4>(base, plan.grad_at[1], n);
    if (plan.grad[2]) pass.v_scales = work_view<float3>(base, plan.grad_at[2], n);
    if (plan.grad[3]) pass.v_opacities = work_view<float>(base, plan.grad_at[3], n);
    const size_t size[4] = {sizeof(float3), sizeof(float4), sizeof(float3), sizeof(float)};
    for (int i = 0; i < 4; i++)
        if (plan.grad[i])
            backend::memset_async(base + plan.grad_at[i], 0, (size_t)n * size[i],
                                  backend::kDefaultStream);
    backend::memset_async(pass.sums.data_ptr(), 0, 5 * sizeof(float), backend::kDefaultStream);

    const int window = cfg.window > 0 ? cfg.window : 4 * (pass.k + 1);
    pca_reg_neighborhood_tensor(
        n, (int)plan.mode, pass.k, window, e.world.means, e.world.quats, e.world.scales,
        e.world.opacities, (int)cfg.covariance, perm, sorted, nodes, meta, ranges, pass.nbrs,
        plan.store, e.optim.radii, cfg.rendered_means_only, cfg.scale_agnostic, pass.v_means,
        pass.v_quats, pass.v_scales, pass.v_opacities, cfg.w_pca * inv_n,
        cfg.w_pca * cfg.tangent * inv_n, cfg.w_normal * inv_n, cfg.w_surfel * inv_n,
        cfg.w_erank * inv_n, cfg.w_transparency * inv_n, pass.sums);
    if (timer) timer->mark(3);
}

// One pass's device time for the ETA, or a probe's two, the second over half
// the splats. Read at the next call: by then the events are behind the device
// and the read does not wait.
struct PassClock {
    backend::Event* ev[3] = {};
    int marks = 0;          // 2: a pass; 3: a probe; 0: nothing pending
    int64_t n = 0, n_probe = 0;
    uint64_t generation = 0;

    void mark(int i) {
        if (!ev[i]) ev[i] = backend::event_create(true);
        backend::event_record(ev[i]);
        marks = i + 1;
        generation = spirula::async_readout_generation();
    }
    void resolve(PcaRegState& s) {
        const int have = marks;
        marks = 0;
        // Events from before a pool reset are not this run's.
        if (have < 2 || generation != spirula::async_readout_generation()) return;
        marks = have;
        backend::event_synchronize(ev[marks - 1]);
        double seconds = backend::event_elapsed_ms(ev[0], ev[1]) * 1e-3;
        if (marks == 3) {
            // A pass over few splats is mostly its launches' fixed cost (+41%
            // a splat at 139k against 1M); the slope between the two sizes
            // misses what larger arrays cost the gathers (-23%). Their mean.
            const double half = backend::event_elapsed_ms(ev[1], ev[2]) * 1e-3;
            const double slope = (seconds - half) / (double)(n_probe - n_probe / 2);
            const double whole = seconds / (double)n_probe;
            seconds = (double)n * (slope > 0.0 ? 0.5 * (slope + whole) : whole);
        }
        s.timed_seconds = seconds;
        s.timed_splats = n;
        s.timed_probe = marks == 3;
        marks = 0;
    }
};

}  // namespace

bool engine_pca_reg_take_timing(double& seconds, int64_t& splats, bool& probed) {
    auto& s = engine().pca_reg;
    if (s.timed_splats <= 0) return false;
    seconds = s.timed_seconds;
    splats = s.timed_splats;
    probed = s.timed_probe;
    s.timed_splats = 0;
    return true;
}

void engine_pca_reg_step(int step, const PcaRegConfig& cfg,
                         std::map<std::string, float>& loss_dict) {
    StepTimingScope timing(StepPart::Regularizer, StepScale::Splats);
    auto& e = engine();
    auto& s = e.pca_reg;
    s.v_means = DeviceVector<float3>();
    s.v_quats = DeviceVector<float4>();
    s.v_scales = DeviceVector<float3>();
    s.v_opacities = DeviceVector<float>();
    static PassClock clock;
    clock.resolve(s);
    const int64_t n = e.cur_num_splats;
    if (cfg.neighbors <= 0 || n < 3 ||
        (cfg.w_pca == 0.0f && cfg.w_normal == 0.0f && cfg.w_surfel == 0.0f &&
         cfg.w_erank == 0.0f && cfg.w_transparency == 0.0f))
        return;
    const int k = (int)std::min<int64_t>(
        {(int64_t)cfg.neighbors, (int64_t)kPcaRegMaxNeighbors - 1, n - 1});

    const bool fused = e.optim.use_fused_proj_bwd_optim;
    if (!cfg.wait && !fused && e.grad.quantize_grad && e.grad.means.data_ptr() == nullptr)
        throw std::runtime_error(
            "pca_reg: the quantized-gradient path (quantization_level > 0 without the "
            "fused projection-backward optimizer) has no fp32 buffer to add into");
    // The fused 3dgs / mip step reads world gradients only from what it is
    // handed; everywhere else they accumulate into engine().grad, which the
    // optim step scales by grad_scale.
    const bool own_grad = fused && e.grad.means.data_ptr() == nullptr;
    const bool mixture = cfg.covariance == PcaRegCovariance::GaussianMixture;
    const bool need_quats = cfg.w_normal != 0 || mixture || cfg.w_transparency != 0;
    const bool need_scales = cfg.w_surfel != 0 || mixture || cfg.w_transparency != 0;
    const bool need_opacities =
        cfg.covariance != PcaRegCovariance::Centers || cfg.w_transparency != 0;
    const bool grad[4] = {own_grad, own_grad && need_quats, own_grad && need_scales,
                          need_opacities};

    const bool cached = knn_cached(cfg.knn);
    const bool lists = cfg.knn == PcaRegKnn::ExactCached || cfg.knn == PcaRegKnn::MortonBestCached;
    const bool exact = cfg.knn == PcaRegKnn::Exact || cfg.knn == PcaRegKnn::ExactCached;
    if (cached) s.perm.resize(PoolSlot::PcaRegPermCache, n);
    if (lists) s.nbrs.resize(PoolSlot::PcaRegNbrsCache, n * k);

    // The backward leaves RasterBwd open with the fused step's screen
    // gradients in it; this joins that phase (POOL_ALIAS_TABLE).
    if (DevicePool::global().phase() != PoolPhase::RasterBwd)
        pool_begin_phase(PoolPhase::RasterBwd);

    SplatStageTimer stage_timer;
    static PartTimer timer;
    DeviceVector<float> sums;
    sums.resize(PoolSlot::PcaRegLossSums, 5);

    if (cfg.wait) {
        // Not started: the arena holds what a rebuilding step will carve, so
        // the start is no jump. A probe times the pass on a prefix of the splats
        // and on half of it, into scratch nothing reads (the Morton methods only).
        const Plan full = make_plan(cfg, n, k, true, grad);
        size_t carved = 0;
        if (cfg.probe && !cached && !exact) {
            const bool all[4] = {true, need_quats, need_scales, need_opacities};
            const size_t per_splat = make_plan(cfg, 1 << 20, k, true, all).total >> 20;
            const int64_t n_probe =
                std::min<int64_t>({n, kProbeSplats, (int64_t)(full.total / (per_splat + 1))});
            if (n_probe >= 64) {
                DeviceVector<uint8_t> work;
                const Plan plan = make_plan(cfg, n_probe, k, true, all);
                work.resize(PoolSlot::PcaRegWork, (int64_t)plan.total);
                carved = work_span(plan.total);
                // The first probe builds the pipelines: seconds, and not timed.
                const bool timed = s.probed;
                s.probed = true;
                if (timed) clock.mark(0);
                for (int half = 0; half < 2; half++) {
                    Pass pass;
                    pass.n = half ? n_probe / 2 : n_probe;
                    pass.k = k;
                    pass.sums = sums;
                    run_pass(cfg, make_plan(cfg, pass.n, k, true, all), work.data_ptr(), pass,
                             nullptr);
                    if (timed) clock.mark(1 + half);
                }
                clock.n = n;
                clock.n_probe = n_probe;
            }
        }
        const size_t rest = full.total + (exact ? tree_bytes(n) : 0);
        if (rest > carved) DevicePool::global().arena_expect(VramCategory::Splat, rest - carved);
        return;
    }

    const bool rebuild = !cached || s.cache_n != n || s.cache_k != k ||
                         s.cache_knn != (int)cfg.knn || s.cache_epoch != s.splat_epoch ||
                         step < s.cache_step ||
                         step - s.cache_step >= std::max(cfg.refresh_every, 1);
    const Plan plan = make_plan(cfg, n, k, rebuild, grad);
    DeviceVector<uint8_t> work;
    work.resize(PoolSlot::PcaRegWork, (int64_t)plan.total);

    Pass pass;
    pass.n = n;
    pass.k = k;
    pass.scale = fused ? 1.0f : 1.0f / e.optim.grad_scale;
    if (cached) pass.perm = pass.keep = s.perm;
    if (lists) pass.nbrs = s.nbrs;
    pass.v_means = e.grad.means;
    if (need_quats) pass.v_quats = e.grad.quats;
    if (need_scales) pass.v_scales = e.grad.scales;
    pass.sums = sums;
    if (cfg.timed) clock.mark(0);
    run_pass(cfg, plan, work.data_ptr(), pass, &timer);
    if (cfg.timed) {
        clock.mark(1);
        clock.n = n;
    }
    if (own_grad) {
        s.v_means = pass.v_means;
        s.v_quats = pass.v_quats;
        s.v_scales = pass.v_scales;
    }
    s.v_opacities = pass.v_opacities;
    if (cached && rebuild) {
        s.cache_n = n;
        s.cache_k = k;
        s.cache_knn = (int)cfg.knn;
        s.cache_step = step;
        s.cache_epoch = s.splat_epoch;
    }
    timer.finish(n, k, cfg.knn, rebuild);

    static AsyncReadout<float> readout(5);
    static int64_t readout_n = 1;
    if (const float* h = readout.read_previous()) {
        loss_dict["pca_lambda1"] = h[0] / (float)readout_n;
        loss_dict["pca_tangent"] = h[1] / (float)readout_n;
        loss_dict["pca_normal"] = h[2] / (float)readout_n;
        if (cfg.w_erank != 0) loss_dict["pca_erank"] = h[3] / (float)readout_n;
        if (cfg.w_transparency != 0) loss_dict["pca_transparency"] = h[4] / (float)readout_n;
    }
    readout.issue(sums.data_ptr());
    readout_n = n;
}
