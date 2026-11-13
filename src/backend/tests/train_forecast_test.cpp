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

int main() {
    schedule_matches_engine_rule();
    eta_tracks_growth();
    vram_predicts_peak();
    breakdown_adds_up();
    eta_follows_resolution();
    if (g_fail) {
        std::printf("train_forecast_test: %d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("train_forecast_test: OK\n");
    return 0;
}
