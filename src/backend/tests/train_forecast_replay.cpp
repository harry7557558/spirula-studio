// TrainForecast run again over a recorded run (SS_FORECAST_DUMP): the peak and
// the time left it projected at each checkpoint against what the run came to.
//   train_forecast_replay <trace> [every_steps]

#include "app/ForecastTrace.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace spirula;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <trace> [every_steps]\n", argv[0]);
        return 2;
    }
    ForecastTrace tr;
    if (!forecast_trace_read(argv[1], tr)) {
        std::printf("cannot read %s\n", argv[1]);
        return 1;
    }
    const int every = argc > 2 ? std::max(1, std::atoi(argv[2])) : 1000;
    // A trace from before the pair peaks were recorded: the raster backward's
    // slices are 40 bytes a visible pair + 8 a splat, the owned buffers 60 a
    // pair + 4 a splat-tile pair.
    for (MemorySample& m : tr.samples) {
        if (m.isect_count > 0 || m.arena_cap == 0) continue;
        const double rb = (double)m.phase_splats[(int)PoolPhase::RasterBwd];
        const double pairs = std::max(0.0, (rb - 8.0 * (double)m.splats_ran) / 40.0);
        const double owned = (double)m.pool_cap[(int)VramCategory::SplatXImg] - (double)m.arena_cap;
        m.isect_pairs = (int64_t)pairs;
        m.isect_count = (int64_t)std::max(0.0, (owned - 60.0 * pairs) / 4.0);
    }
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const int sxi = (int)VramCategory::SplatXImg;

    double truth = 0.0, sxi_end = 0.0;
    for (const MemorySample& m : tr.samples) {
        truth = std::max(truth, (double)std::max(m.process_bytes, m.process_peak_bytes));
        sxi_end = std::max(sxi_end, (double)m.pool_cap[sxi]);
    }
    const int last = tr.samples.back().step;
    const bool whole = last + 1 >= tr.setup.schedule.total_steps();
    std::printf("%zu samples to step %d of %d%s; peak %.3f GiB, splat x img %.3f GiB\n",
                tr.samples.size(), last, tr.setup.schedule.total_steps(),
                whole ? "" : " (partial: the peak is so far)", truth / gib, sxi_end / gib);
    std::printf("%8s %10s %8s %8s %8s %7s %7s %6s\n", "step", "splats", "ours", "sxi", "peak",
                "sigma", "err%", "z");

    TrainForecast fc;
    fc.reset(tr.setup);
    double worst_under = 0.0, worst_over = 0.0;
    for (const MemorySample& m : tr.samples) {
        fc.add_memory(m);
        if ((m.step + 1) % every != 0 && m.step != tr.setup.start_step) continue;
        const VramForecast v = fc.vram(false);
        if (!v.projected) continue;
        const double err = v.peak_mean / truth - 1.0;
        const double z = v.peak_sigma > 0.0 ? (truth - v.peak_mean) / v.peak_sigma : 0.0;
        worst_under = std::min(worst_under, err);
        worst_over = std::max(worst_over, err);
        std::printf("%8d %10lld %8.3f %8.3f %8.3f %7.3f %+7.1f %+6.1f\n", m.step + 1,
                    (long long)m.splats_next, v.ours / gib, (double)m.pool_cap[sxi] / gib,
                    v.peak_mean / gib, v.peak_sigma / gib, 100.0 * err, z);
    }
    std::printf("projected peak vs truth: %+.1f%% .. %+.1f%%\n", 100.0 * worst_under,
                100.0 * worst_over);
    if (tr.steps.empty()) return 0;

    // The time model: its ETA at each checkpoint against the wall time the
    // rest of the run took, as a share of the whole run.
    std::vector<double> left(tr.steps.size() + 1, 0.0);
    for (size_t i = tr.steps.size(); i-- > 0;) left[i] = left[i + 1] + tr.steps[i].sample.wall_s;
    const double total = left[0];
    std::printf("\n%.1f s over %zu steps\n%8s %10s %8s %8s %8s %8s %7s\n", total,
                tr.steps.size(), "step", "splats", "eta", "sigma", "reg", "truth", "err%");
    TrainForecast tc;
    tc.reset(tr.setup);
    for (size_t i = 0; i < tr.steps.size(); ++i) {
        const ForecastTraceStep& t = tr.steps[i];
        if (t.reg_kind) tc.add_regularizer(t.reg, t.reg_splats, t.reg_kind == 1);
        if (t.wall >= 0.0) tc.add_step(t.sample.step, t.wall, t.splats, t.gpu, t.sample.resolution_divisor);
        tc.add_breakdown(t.sample);
        if (t.sample.save_s > 0.0) tc.add_save(t.sample.save_s, t.splats);
        if ((t.sample.step + 1) % every != 0 || i + 1 >= tr.steps.size()) continue;
        const EtaForecast e = tc.eta(t.sample.step + 1, tr.steps[i + 1].splats);
        if (e.seconds < 0.0) continue;
        std::printf("%8d %10lld %8.1f %8.1f %8.1f %8.1f %+7.1f\n", t.sample.step + 1,
                    (long long)tr.steps[i + 1].splats, e.seconds, e.sigma, e.regularizer,
                    left[i + 1], 100.0 * (e.seconds - left[i + 1]) / total);
    }
    return 0;
}
