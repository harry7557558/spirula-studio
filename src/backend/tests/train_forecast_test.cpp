// TrainForecast against synthetic runs whose true time and VRAM are known.

#include "app/TrainForecast.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

using namespace spirula;

static int g_fail = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            std::printf("FAIL %s:%d: ", __func__, __LINE__); \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
            ++g_fail;                                     \
        }                                                 \
    } while (0)

static DensifyConfig densify(int start, int stop, int every, float growth) {
    DensifyConfig d;
    d.refine_start_iter = start;
    d.refine_stop_iter = stop;
    d.refine_stop_num_iter = 1 << 20;   // leaves refine_stop_iter in charge
    d.refine_every = every;
    d.growth_factor = growth;
    return d;
}

// The replay must agree with stepping densify_grows_at / densify_target, with
// and without a splat budget that holds growth at a ceiling for a while.
static void schedule_matches_engine_rule() {
    DensifyConfig budgeted = densify(500, 15000, 100, 1.05f);
    budgeted.growth_caps = {{0, 250000}, {3000, 500000}, {9000, 1000000}};
    for (const DensifyConfig& d : {densify(500, 15000, 100, 1.05f), budgeted}) {
        const bool b = !d.growth_caps.empty();
        const int T = 30000;
        const int64_t cap = 1000000;
        const SplatSchedule sch(d, T, cap);
        for (int from : {0, 499, 501, 2950, 7300, 20000}) {
            int64_t n = 200000;
            for (int s = 0; s < from; ++s)
                if (densify_grows_at(d, s, T)) n = densify_target(d, n, densify_cap_at(d, s, cap));
            std::vector<int64_t> want(T - from);
            int64_t m = n;
            for (int s = from; s < T; ++s) {
                want[s - from] = m;
                if (densify_grows_at(d, s, T)) m = densify_target(d, m, densify_cap_at(d, s, cap));
            }
            int covered = from;
            bool ok = true;
            sch.segments(from, n, [&](int f, int e, int64_t k) {
                ok = ok && f == covered;
                for (int s = f; s < e; ++s) ok = ok && want[s - from] == k;
                covered = e;
            });
            CHECK(ok && covered == T, "segments diverge from the engine rule, from=%d budget=%d", from, b);
            CHECK(sch.final_count(from, n) == m, "final count %lld vs %lld, budget=%d",
                  (long long)sch.final_count(from, n), (long long)m, b);
            CHECK(m == cap, "budget=%d never reached the cap: %lld", b, (long long)m);
        }
        CHECK(sch.first_densify(0) == 600, "first densify %d", sch.first_densify(0));
        CHECK(sch.first_densify(15000) == -1, "densify past the stop");
    }
    CHECK(densify_cap_at(budgeted, 2999, 1000000) == 250000 && densify_cap_at(budgeted, 3000, 1000000) == 500000 &&
          densify_cap_at(budgeted, 9000, 800000) == 800000, "ceiling lookup");
}

// Step time = 20 ms + (40 ms + 3 ms per SH coefficient) per Msplat: the
// naive mean underestimates badly while the count and the SH degree are still
// growing, the model must not.
static void eta_tracks_growth() {
    const DensifyConfig d = densify(500, 15000, 100, 1.05f);
    const int T = 30000;
    const int64_t cap = 3000000;
    const SplatSchedule sch(d, T, cap);
    ForecastSetup fs;
    fs.schedule = sch;
    fs.sh_degree = 3;
    fs.sh_degree_every = 1000;
    TrainForecast fc;
    fc.reset(fs);
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(1.0, 0.1);
    const double a = 0.020, b = 0.040, b_coeff = 0.003;
    auto coeffs = [](int s) {
        const int d = std::min(s / 1000, 3);
        return (d + 1) * (d + 1) - 1;
    };

    std::vector<int64_t> n_at(T);
    sch.segments(0, 300000, [&](int f, int e, int64_t n) {
        for (int s = f; s < e; ++s) n_at[s] = n;
    });
    std::vector<double> t_at(T);
    for (int s = 0; s < T; ++s)
        t_at[s] = a + (b + b_coeff * coeffs(s)) * n_at[s] / 1e6;
    auto remaining = [&](int from) {
        double sum = 0.0;
        for (int s = from; s < T; ++s) sum += t_at[s];
        return sum;
    };

    double recent = 0.0;
    for (int s = 0; s < T; ++s) {
        const double wall = t_at[s] * noise(rng);
        const double gpu = s % 10 == 0 ? (t_at[s] - a) * noise(rng) : -1.0;
        fc.add_step(s, wall, n_at[s], gpu);
        recent = s < 100 ? wall : recent + (wall - recent) / 100.0;
        if (s == 400 || s == 1500 || s == 2500 || s == 8000) {
            const double truth = remaining(s + 1);
            const EtaForecast e = fc.eta(s + 1, n_at[s + 1]);
            const double naive = recent * (T - s - 1);
            std::printf("  eta @%d: model %.0f s (sigma %.0f), naive %.0f s, truth %.0f s\n",
                        s, e.seconds, e.sigma, naive, truth);
            // Before the count moves only the prior speaks, so all it can be
            // held to is its own sigma -- and beating the naive estimate.
            const bool grown = s > 1000;
            // Past ~5300 the count sits at the cap and naive is exact too.
            if (s < 5000)
                CHECK(std::fabs(e.seconds - truth) < std::fabs(naive - truth),
                      "eta at step %d no better than naive", s);
            CHECK(std::fabs(e.seconds - truth) <
                      (grown ? std::max(0.1 * truth, 2.0 * e.sigma) : 3.0 * e.sigma),
                  "eta at step %d off by %.1f%%", s, 100.0 * (e.seconds / truth - 1.0));
        }
    }
}

// splat x img follows 60 bytes per splat at 300k splats, as msplats^power,
// with per-step spread; the rest is 1 GiB.
static VramForecast run_vram(double others_gib, int stop_at, double power = 1.0) {
    const DensifyConfig d = densify(500, 15000, 100, 1.05f);
    const int T = 30000;
    const int64_t cap = 3000000;
    const SplatSchedule sch(d, T, cap);
    ForecastSetup fs;
    fs.schedule = sch;
    fs.distinct_batches = 200;
    TrainForecast fc;
    fc.reset(fs);
    std::mt19937 rng(7);
    std::vector<double> view_ratio(200);
    std::lognormal_distribution<double> spread(0.0, 0.2);
    for (double& r : view_ratio) r = 60.0 * spread(rng);
    std::uniform_int_distribution<int> pick(0, 199);

    const double gib = 1024.0 * 1024.0 * 1024.0;
    std::vector<int64_t> n_at(T + 1);
    sch.segments(0, 300000, [&](int f, int e, int64_t n) {
        for (int s = f; s < e; ++s) n_at[s] = n;
    });
    n_at[T] = n_at[T - 1];
    size_t hw = 0;
    VramForecast at_stop;
    for (int s = 0; s < T; ++s) {
        const size_t used = (size_t)(3e5 * std::pow(n_at[s] / 3e5, power) * view_ratio[pick(rng)]);
        hw = std::max(hw, used);
        MemorySample m;
        m.step = s;
        m.splats_ran = n_at[s];
        m.splats_next = n_at[s + 1];
        m.pool_used[(int)VramCategory::SplatXImg] = used;
        m.pool_cap[(int)VramCategory::SplatXImg] = hw;
        m.pool_cap[(int)VramCategory::Splat] = (size_t)gib;
        m.has_process = m.has_used = m.has_total = true;
        m.process_bytes = (uint64_t)gib + hw;
        m.used_bytes = m.process_bytes + (uint64_t)(others_gib * gib);
        m.total_bytes = (uint64_t)(8.0 * gib);
        fc.add_memory(m);
        if (s == stop_at) at_stop = fc.vram();
    }
    const double truth = gib + (double)hw;
    std::printf("  vram @%d: peak %.3f +- %.3f GiB, truth %.3f GiB, p_oom %.3f\n",
                stop_at, at_stop.peak_mean / gib, at_stop.peak_sigma / gib,
                truth / gib, at_stop.p_oom);
    CHECK(at_stop.projected, "no forecast at step %d", stop_at);
    CHECK(std::fabs(at_stop.peak_mean - truth) < 3.0 * at_stop.peak_sigma + 0.02 * truth,
          "peak off: %.3f vs %.3f GiB", at_stop.peak_mean / gib, truth / gib);
    return at_stop;
}

static void vram_predicts_peak() {
    const VramForecast early = run_vram(1.0, 1000);
    CHECK(early.risk == OomRisk::Low, "risk %d with 6 GiB spare", (int)early.risk);
    const VramForecast late = run_vram(1.0, 10000);
    CHECK(late.peak_sigma <= early.peak_sigma, "sigma grew with more data");
    // 8 GiB device, 6.8 GiB elsewhere: 1.2 GiB for a ~1.2 GiB peak.
    const VramForecast tight = run_vram(6.8, 1000);
    CHECK(tight.risk >= OomRisk::Medium, "risk %d on a full device", (int)tight.risk);
    // Smaller splats cover fewer tiles: the fit must not hold this to linear.
    run_vram(1.0, 3000, 0.6);
}

// An indoor capture: a few views see the whole model, most a sliver of it, and
// the arena holds a loss phase the pixels size (`loss_gib`). Neither the long
// lower tail nor that phase is growth: the peak is the views that see it all.
static void vram_bounded_views(double loss_gib) {
    const DensifyConfig d = densify(500, 15000, 100, 1.05f);
    const int T = 30000;
    const int64_t cap = 3000000;
    const SplatSchedule sch(d, T, cap);
    ForecastSetup fs;
    fs.schedule = sch;
    fs.distinct_batches = 1000;
    TrainForecast fc;
    fc.reset(fs);
    std::mt19937 rng(11);
    std::vector<double> seen(1000);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    for (double& v : seen) v = u(rng) < 0.1 ? 1.0 : 0.02 + 0.5 * u(rng) * u(rng);
    std::vector<int> order(1000);
    for (int i = 0; i < 1000; ++i) order[i] = i;

    const double gib = 1024.0 * 1024.0 * 1024.0;
    const int sxi = (int)VramCategory::SplatXImg;
    std::vector<int64_t> n_at(T + 1);
    sch.segments(0, 300000, [&](int f, int e, int64_t n) {
        for (int s = f; s < e; ++s) n_at[s] = n;
    });
    n_at[T] = n_at[T - 1];
    // Per visible splat: 60 bytes owned, 100 in the tile phase.
    const size_t loss = (size_t)(loss_gib * gib);
    size_t owned = 0, tile = 0;
    double truth = 0.0;
    std::vector<VramForecast> at;
    const int stops[] = {1000, 3000, 6000, 20000};
    for (int s = 0; s < T; ++s) {
        if (s % 1000 == 0) std::shuffle(order.begin(), order.end(), rng);
        const double visible = (double)n_at[s] * seen[order[s % 1000]];
        owned = std::max(owned, (size_t)(60.0 * visible));
        tile = std::max(tile, (size_t)(100.0 * visible));
        const size_t arena = std::max(loss, tile);
        MemorySample m;
        m.step = s;
        m.splats_ran = n_at[s];
        m.splats_next = n_at[s + 1];
        m.pool_used[sxi] = (size_t)(160.0 * visible);
        m.pool_cap[sxi] = owned + arena;
        m.pool_cap[(int)VramCategory::Splat] = (size_t)gib;
        m.arena_cap = arena;
        m.phase_pixels[(int)PoolPhase::Loss] = loss;
        m.phase_splats[(int)PoolPhase::TileIsect] = tile;
        m.has_process = m.has_used = m.has_total = true;
        m.process_bytes = (uint64_t)gib + owned + arena;
        m.used_bytes = m.process_bytes;
        m.total_bytes = (uint64_t)(24.0 * gib);
        truth = (double)m.process_bytes;
        fc.add_memory(m);
        for (int stop : stops)
            if (s == stop) at.push_back(fc.vram());
    }
    for (size_t i = 0; i < at.size(); ++i) {
        const VramForecast& v = at[i];
        std::printf("  vram, bounded views, %.1f GiB loss phase @%d: peak %.3f +- %.3f GiB, "
                    "truth %.3f GiB\n", loss_gib, stops[i], v.peak_mean / gib,
                    v.peak_sigma / gib, truth / gib);
        CHECK(v.peak_mean > truth - 2.0 * v.peak_sigma - 0.02 * truth, "under-read at %d",
              stops[i]);
        // Every view has come round at the held count by 20000: the peak is known.
        const double slack = stops[i] >= 20000 ? 0.03 : stops[i] >= 6000 ? 0.08 : 0.25;
        CHECK(v.peak_mean < truth * (1.0 + slack), "over-read at %d: %.3f vs %.3f GiB",
              stops[i], v.peak_mean / gib, truth / gib);
    }
}

static double cost(StepCells& gpu, double m, double r, bool densify) {
    gpu = StepCells{};
    gpu.at(StepPart::Render, StepScale::Splats) = 0.004 * m;
    gpu.at(StepPart::Render, StepScale::SplatsPixels) = 0.006 * std::sqrt(m) * std::pow(r, 0.45);
    gpu.at(StepPart::Loss, StepScale::Pixels) = 0.008 * r;
    gpu.at(StepPart::Appearance, StepScale::Images) = 0.001;
    if (densify) gpu.at(StepPart::Densify, StepScale::Splats) = 0.050 * m;
    return gpu.total();
}

// Cells whose truth is known: the window weights densify steps by their
// share, the run's total is elapsed plus the ETA, and the columns still to
// come follow the splat count.
static void breakdown_adds_up() {
    const DensifyConfig d = densify(500, 15000, 100, 1.05f);
    const int T = 20000;
    const SplatSchedule sch(d, T, 2000000);
    ForecastSetup fs;
    fs.schedule = sch;
    TrainForecast fc;
    fc.reset(fs);
    std::vector<int64_t> n_at(T);
    sch.segments(0, 300000, [&](int f, int e, int64_t n) {
        for (int s = f; s < e; ++s) n_at[s] = n;
    });
    const double data = 0.0005, idle = 0.001;
    double elapsed = 0.0;
    for (int s = 0; s < T; ++s) {
        const double m = n_at[s] / 1e6;
        const bool dens = s % 100 == 0;
        StepSample ss;
        const double wall = cost(ss.gpu, m, 1.0, dens) + data + idle;
        elapsed += wall;
        fc.add_step(s, wall, n_at[s], -1.0);
        ss.step = s;
        ss.wall_s = wall;
        ss.data_wait_s = data;
        ss.densify = dens;
        ss.timed = s % 10 == 0 || dens;
        fc.add_breakdown(ss);
        if (s == 400 || s == 1500) {
            // The columns say how the step splits before the count has moved.
            double truth = 0.0;
            StepCells unused;
            for (int t = s + 1; t < T; ++t)
                truth += cost(unused, n_at[t] / 1e6, 1.0, t % 100 == 0) + data + idle;
            const double eta = fc.eta(s + 1, n_at[s + 1]).seconds;
            std::printf("  eta @%d from the columns: %.0f s, truth %.0f s\n", s, eta, truth);
            CHECK(std::fabs(eta / truth - 1.0) < 0.05, "eta off by %.1f%% at %d",
                  100.0 * (eta / truth - 1.0), s);
        }
        if (s != 2500 && s != 8000) continue;
        const TimeBreakdown now = fc.time_measured();
        const double dens_truth = 0.050 * m / 100.0;
        std::printf("  breakdown @%d: %.3f ms/step, densify %.4f (truth %.4f), unaccounted %.3f\n",
                    s, now.total() * 1e3, now.cells.part(StepPart::Densify) * 1e3,
                    dens_truth * 1e3, now.unaccounted * 1e3);
        CHECK(now.known && now.window == 100, "window %d", now.window);
        CHECK(std::fabs(now.cells.part(StepPart::Densify) / dens_truth - 1.0) < 0.15,
              "densify share off at %d", s);
        CHECK(std::fabs(now.unaccounted / idle - 1.0) < 0.3, "unaccounted %.4f at %d",
              now.unaccounted, s);
        CHECK(std::fabs(now.cells.part(StepPart::Loading) / data - 1.0) < 0.01, "loading");

        const TimeBreakdown run = fc.time_forecast(s + 1, n_at[s + 1]);
        const double eta = fc.eta(s + 1, n_at[s + 1]).seconds;
        CHECK(std::fabs(run.total() - (elapsed + eta)) < 1e-6 * run.total(),
              "run %.3f s vs elapsed + eta %.3f s", run.total(), elapsed + eta);
        double splat_truth = 0.0;
        for (int t = 0; t <= s; ++t) splat_truth += 0.004 * n_at[t] / 1e6;
        for (int t = s + 1; t < T; ++t) splat_truth += 0.004 * n_at[t] / 1e6;
        const double splat_run = run.cells.at(StepPart::Render, StepScale::Splats);
        std::printf("  run @%d: %.1f s, splat column %.1f s (truth %.1f s)\n", s, run.total(),
                    splat_run, splat_truth);
        CHECK(std::fabs(splat_run / splat_truth - 1.0) < 0.25, "splat column off at %d", s);
    }
}

// Progressive resolution: 1/4, then 1/2, then full. A step that costs more as
// the images grow must be forecast from the low-resolution steps.
static void eta_follows_resolution() {
    const int T = 12000;
    const SplatSchedule sch(densify(500, 15000, 100, 1.05f), T, 1000000);
    ForecastSetup fs;
    fs.schedule = sch;
    fs.resolution_stages = {{0, 4}, {1500, 2}, {4000, 1}};
    TrainForecast fc;
    fc.reset(fs);
    std::vector<double> t_at(T);
    for (int s = 0; s < T; ++s) {
        StepCells gpu;
        t_at[s] = cost(gpu, 1.0, fc.pixel_ratio(s), false) + 0.002;
    }
    double recent = 0.0;
    for (int s = 0; s < T; ++s) {
        StepSample ss;
        const double wall = cost(ss.gpu, 1.0, fc.pixel_ratio(s), false) + 0.002;
        fc.add_step(s, wall, 1000000, -1.0);
        ss.step = s;
        ss.wall_s = wall;
        ss.timed = s % 10 == 0;
        fc.add_breakdown(ss);
        recent = s < 100 ? wall : recent + (wall - recent) / 100.0;
        if (s != 1000 && s != 2500) continue;
        double truth = 0.0;
        for (int t = s + 1; t < T; ++t) truth += t_at[t];
        const double eta = fc.eta(s + 1, 1000000).seconds;
        const double naive = recent * (T - s - 1);
        std::printf("  eta @%d (1/%d res): model %.0f s, naive %.0f s, truth %.0f s\n", s,
                    s < 1500 ? 4 : 2, eta, naive, truth);
        CHECK(std::fabs(eta - truth) < std::fabs(naive - truth), "no better than naive at %d", s);
        CHECK(std::fabs(eta / truth - 1.0) < (s < 1500 ? 0.25 : 0.12), "eta off by %.1f%% at %d",
              100.0 * (eta / truth - 1.0), s);
    }
}

static void eta_hides_loading_stalls() {
    ForecastSetup fs;
    fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 5000, 1000000);
    fs.resolution_stages = {{0, 4}, {1000, 2}, {3000, 1}};
    TrainForecast fc;
    fc.reset(fs);
    auto gpu_cost = [&](int step, StepCells& gpu) {
        const double r = fc.pixel_ratio(step);
        gpu.at(StepPart::Optimizer, StepScale::Splats) = 0.002;
        gpu.at(StepPart::Render, StepScale::SplatsPixels) = 0.010 * std::pow(r, 0.7);
        gpu.at(StepPart::Loss, StepScale::Pixels) = 0.030 * r;
        gpu.at(StepPart::Other, StepScale::Fixed) = 0.001;
        return gpu.total();
    };
    for (int step = 0; step < 2200; ++step) {
        StepSample ss;
        ss.step = step;
        const double gpu = gpu_cost(step, ss.gpu);
        ss.data_wait_s = step % 10 < 4 ? std::max(0.0, 0.030 - gpu) / 0.4 : 0.0;
        ss.wall_s = gpu + ss.data_wait_s;
        ss.timed = step % 10 == 0;
        fc.add_step(step, ss.wall_s, 1000000, -1.0);
        fc.add_breakdown(ss);
    }
    double truth = 0.0;
    for (int step = 2200; step < 5000; ++step) {
        StepCells gpu;
        truth += std::max(0.030, gpu_cost(step, gpu));
    }
    const auto e = fc.eta(2200, 1000000);
    std::printf("  loading overlap: ETA %.1f, truth %.1f, sigma %.1f\n", e.seconds, truth, e.sigma);
    CHECK(std::abs(e.seconds / truth - 1.0) < 0.10, "GPU growth must hide producer stalls");
    CHECK(fc.time_measured().cells.at(StepPart::Loading, StepScale::Pixels) == 0.0,
          "source decoding is not rendered-pixel work");
}

static void eta_counts_recurring_stalls() {
    ForecastSetup fs;
    fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 5000, 1000000);
    TrainForecast fc;
    fc.reset(fs);
    for (int step = 0; step < 2000; ++step)
        fc.add_step(step, step % 10 < 4 ? 0.050 : 0.010, 1000000, -1.0);
    const double truth = 3000 * (0.4 * 0.050 + 0.6 * 0.010);
    CHECK(std::abs(fc.eta(2000, 1000000).seconds / truth - 1.0) < 0.05,
          "recurring loading stalls were discarded as timing outliers");
}

static void eta_learns_pixel_cost() {
    for (double exponent : {0.2, 0.8}) {
        ForecastSetup fs;
        fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 5000, 1000000);
        fs.resolution_stages = {{0, 4}, {1000, 2}, {3000, 1}};
        TrainForecast fc;
        fc.reset(fs);
        auto cost = [&](int step, StepCells& gpu) {
            const double r = fc.pixel_ratio(step);
            gpu.at(StepPart::Optimizer, StepScale::Splats) = 0.004;
            gpu.at(StepPart::Render, StepScale::SplatsPixels) = 0.04 * std::pow(r, exponent);
            gpu.at(StepPart::Loss, StepScale::Pixels) = 0.008 * r;
            gpu.at(StepPart::Other, StepScale::Fixed) = 0.002;
            return gpu.total() + 0.001;
        };
        for (int step = 0; step < 2200; ++step) {
            StepSample ss;
            ss.step = step;
            ss.wall_s = cost(step, ss.gpu);
            ss.timed = step % 10 == 0;
            fc.add_step(step, ss.wall_s, 1000000, -1.0);
            fc.add_breakdown(ss);
        }
        double truth = 0.0;
        for (int step = 2200; step < 5000; ++step) {
            StepCells gpu;
            truth += cost(step, gpu);
        }
        const auto e = fc.eta(2200, 1000000);
        std::printf("  pixel exponent %.1f: ETA %.1f, truth %.1f, sigma %.1f\n", exponent,
                    e.seconds, truth, e.sigma);
        CHECK(std::abs(e.seconds / truth - 1.0) < 0.10,
              "pixel exponent %.1f still uses the fixed prior", exponent);
    }
}

static void eta_before_first_window() {
    ForecastSetup fs;
    fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 1000, 1000000);
    fs.resolution_stages = {{0, 4}, {300, 2}, {700, 1}};
    TrainForecast fc;
    fc.reset(fs);
    auto cost = [&](int s) {
        const double r = fc.pixel_ratio(s);
        return 0.01 * (0.75 * r + 0.25 + 0.5 * std::pow(r, kSplatPixelTimeExp) + 0.5);
    };
    for (int s = 0; s < 30; ++s) fc.add_step(s, cost(s), 1000000, -1.0);
    double truth = 0.0;
    for (int s = 30; s < 1000; ++s) truth += cost(s);
    CHECK(std::abs(fc.eta(30, 1000000).seconds / truth - 1.0) < 0.001,
          "warmup ETA omits scheduled resolution growth");
}

static void vram_resolution_boundaries() {
    constexpr size_t MiB = 1024 * 1024;
    ForecastSetup fs;
    fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 1000, 1000000);
    fs.resolution_stages = {{0, 4}, {300, 2}, {700, 1}};
    TrainForecast fc;
    fc.reset(fs);
    MemorySample m;
    m.step = 0;
    m.resolution_divisor = 4;
    m.splats_ran = m.splats_next = 1000000;
    m.pool_cap[(int)VramCategory::Splat] = 200 * MiB;
    m.pool_cap[(int)VramCategory::Image] = 4 * MiB;
    m.pool_cap[(int)VramCategory::SplatXImg] = 32 * MiB;
    m.arena_cap = 16 * MiB;
    m.phase_pixels[(int)PoolPhase::Loss] = 16 * MiB;
    m.isect_pairs = 100000;
    m.isect_count = m.isect_step = 100000;
    fc.add_memory(m);
    const auto v = fc.vram();
    for (int step : {299, 300, 699, 700}) {
        const auto it = std::find_if(v.projection.begin(), v.projection.end(),
                                    [&](const auto& b) { return b.x == step; });
        CHECK(it != v.projection.end(), "missing resolution boundary at %d", step);
    }
    CHECK(v.peak_mean > 500 * MiB, "future image buffers missing: %.1f MiB", v.peak_mean / MiB);
    m.step = 299;
    m.resolution_divisor = 2;
    m.pool_cap[(int)VramCategory::Image] *= 4;
    m.pool_cap[(int)VramCategory::SplatXImg] += 48 * MiB;
    m.arena_cap *= 4;
    m.phase_pixels[(int)PoolPhase::Loss] *= 4;
    fc.add_memory(m);
    const double early = fc.vram(false).peak_mean;
    m.step = 300;
    m.resolution_divisor = 4;
    fc.add_memory(m);
    CHECK(std::abs(fc.vram(false).peak_mean / early - 1.0) < 0.02,
          "late coarse batch rescales retained image allocations");
}

static void timing_fallback_stays_in_its_resolution() {
    ForecastSetup fs;
    fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 3000, 1000000);
    fs.resolution_stages = {{0, 4}, {550, 2}, {1500, 1}};
    TrainForecast all, sparse;
    all.reset(fs);
    sparse.reset(fs);
    for (int step = 0; step < 750; ++step) {
        StepSample sample;
        sample.step = step;
        sample.resolution_divisor = step < 500 ? 4 : 2;
        sample.densify = step % 5 == 0;
        const double r = 1.0 / (sample.resolution_divisor * sample.resolution_divisor);
        sample.gpu.at(StepPart::Optimizer, StepScale::Splats) = 0.002;
        sample.gpu.at(StepPart::Render, StepScale::SplatsPixels) = 0.040 * std::pow(r, 0.7);
        sample.gpu.at(StepPart::Loss, StepScale::Pixels) = 0.008 * r;
        sample.wall_s = sample.gpu.total() + 0.001;
        all.add_step(step, sample.wall_s, 1000000, -1.0, sample.resolution_divisor);
        sparse.add_step(step, sample.wall_s, 1000000, -1.0, sample.resolution_divisor);
        sample.timed = sample.densify || step % 5 == 1;
        all.add_breakdown(sample);
        if (step >= 500 && sample.densify) sample.timed = false;
        sparse.add_breakdown(sample);
    }
    CHECK(std::abs(all.time_measured().cells.total() - sparse.time_measured().cells.total()) < 1e-10,
          "a retained coarse densify timing contaminated the fine-stage breakdown");
    CHECK(std::abs(all.eta(750, 1000000).seconds / sparse.eta(750, 1000000).seconds - 1.0) < 1e-8,
          "old-stage timing fallback changed the fitted pixel cost");
}

static void vram_sh_warmup_keeps_view_horizon() {
    constexpr size_t MiB = 1024 * 1024;
    for (bool progressive : {false, true}) {
        ForecastSetup fs;
        fs.schedule = SplatSchedule(densify(10000, 10000, 100, 1.05f), 3000, 1000000);
        fs.distinct_batches = 10000;
        fs.sh_degree = 3;
        if (progressive) fs.resolution_stages = {{0, 2}, {500, 1}};
        TrainForecast full, warmup;
        full.reset(fs);
        fs.sh_degree_every = 700;
        warmup.reset(fs);
        MemorySample m;
        m.step = 0;
        m.splats_ran = m.splats_next = 1000000;
        m.pool_cap[(int)VramCategory::Splat] = 200 * MiB;
        m.pool_cap[(int)VramCategory::SplatXImg] = 32 * MiB;
        m.arena_cap = 16 * MiB;
        m.phase_splats[(int)PoolPhase::RasterBwd] = 16 * MiB;
        m.isect_pairs = 100000;
        m.isect_count = m.isect_step = 100000;
        full.add_memory(m);
        warmup.add_memory(m);
        const auto a = full.vram(), b = warmup.vram();
        CHECK(std::abs(a.peak_mean - b.peak_mean) < 1e-6 &&
              std::abs(a.peak_sigma - b.peak_sigma) < 1e-6,
              "SH boundaries shortened the held-count view horizon (progressive=%d)", progressive);
    }
}

// A per-splat regularizer that starts mid-run, probed before it does: the ETA
// carries it from the first probe on, and does not move when it starts.
static void eta_counts_regularizer() {
    const int T = 20000, start = 6000;
    const SplatSchedule sch(densify(500, 15000, 100, 1.05f), T, 2000000);
    ForecastSetup fs;
    fs.schedule = sch;
    fs.reg_start_step = start;
    TrainForecast fc;
    fc.reset(fs);
    std::mt19937 rng(3);
    std::normal_distribution<double> noise(1.0, 0.05);
    const double a = 0.010, b = 0.020, reg = 0.012;
    std::vector<int64_t> n_at(T);
    sch.segments(0, 300000, [&](int f, int e, int64_t n) {
        for (int s = f; s < e; ++s) n_at[s] = n;
    });
    auto cost = [&](int s) {
        const double m = (double)n_at[s] / 1e6;
        return a + b * m + (s >= start ? reg * m : 0.0);
    };
    for (int s = 0; s < T; ++s) {
        const double m = (double)n_at[s] / 1e6;
        // A probe's prefix runs 10% fast; a real pass is what it is.
        if (s < start && s % 50 == 10) fc.add_regularizer(0.9 * reg * m * noise(rng), n_at[s], true);
        if (s >= start && s % 10 == 0) fc.add_regularizer(reg * m * noise(rng), n_at[s], false);
        fc.add_step(s, cost(s) * noise(rng), n_at[s], -1.0);
        if (s != 3000 && s != 5900 && s != 6100 && s != 9000) continue;
        double truth = 0.0;
        for (int t = s + 1; t < T; ++t) truth += cost(t);
        const EtaForecast e = fc.eta(s + 1, n_at[s + 1]);
        std::printf("  eta @%d (regularizer from %d): model %.0f s (sigma %.0f), truth %.0f s\n", s,
                    start, e.seconds, e.sigma, truth);
        CHECK(std::fabs(e.seconds / truth - 1.0) < 0.08, "eta off by %.1f%% at %d",
              100.0 * (e.seconds / truth - 1.0), s);
    }
}

int main() {
    schedule_matches_engine_rule();
    eta_tracks_growth();
    vram_predicts_peak();
    vram_bounded_views(0.0);
    vram_bounded_views(0.5);
    breakdown_adds_up();
    eta_follows_resolution();
    eta_counts_regularizer();
    vram_resolution_boundaries();
    timing_fallback_stays_in_its_resolution();
    vram_sh_warmup_keeps_view_horizon();
    eta_before_first_window();
    eta_learns_pixel_cost();
    eta_counts_recurring_stalls();
    eta_hides_loading_stalls();
    if (g_fail) {
        std::printf("train_forecast_test: %d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("train_forecast_test: OK\n");
    return 0;
}
