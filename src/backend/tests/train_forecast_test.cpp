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

// The replay must agree with stepping densify_grows_at / densify_target.
static void schedule_matches_engine_rule() {
    const DensifyConfig d = densify(500, 15000, 100, 1.05f);
    const int T = 30000;
    const int64_t cap = 1000000;
    const SplatSchedule sch(d, T, cap);
    for (int from : {0, 499, 501, 7300, 20000}) {
        int64_t n = 200000;
        for (int s = 0; s < from; ++s)
            if (densify_grows_at(d, s, T)) n = densify_target(d, n, cap);
        std::vector<int64_t> want(T - from);
        int64_t m = n;
        for (int s = from; s < T; ++s) {
            want[s - from] = m;
            if (densify_grows_at(d, s, T)) m = densify_target(d, m, cap);
        }
        int covered = from;
        bool ok = true;
        sch.segments(from, n, [&](int f, int e, int64_t k) {
            ok = ok && f == covered;
            for (int s = f; s < e; ++s) ok = ok && want[s - from] == k;
            covered = e;
        });
        CHECK(ok && covered == T, "segments diverge from the engine rule, from=%d", from);
        CHECK(sch.final_count(from, n) == m, "final count %lld vs %lld",
              (long long)sch.final_count(from, n), (long long)m);
    }
    CHECK(sch.first_densify(0) == 600, "first densify %d", sch.first_densify(0));
    CHECK(sch.first_densify(15000) == -1, "densify past the stop");
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

// splat x img follows 60 bytes per splat with per-step spread; the rest is 1 GiB.
static VramForecast run_vram(double others_gib, int stop_at) {
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
        const size_t used = (size_t)(n_at[s] * view_ratio[pick(rng)]);
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
    CHECK(at_stop.valid, "no forecast at step %d", stop_at);
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
}

int main() {
    schedule_matches_engine_rule();
    eta_tracks_growth();
    vram_predicts_peak();
    if (g_fail) {
        std::printf("train_forecast_test: %d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("train_forecast_test: OK\n");
    return 0;
}
