// The shared memory forecast (app/ResourceForecast.h) and the report a child
// writes it from (app/RunReport.h), on cases whose answers are known.

#include "app/ResourceForecast.h"
#include "app/RunReport.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>

using namespace spirula;

static int g_fail = 0;
#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (!(cond)) {                                       \
            std::printf("FAIL %s:%d: ", __func__, __LINE__); \
            std::printf(__VA_ARGS__);                        \
            std::printf("\n");                               \
            ++g_fail;                                        \
        }                                                    \
    } while (0)

static constexpr double GiB = 1024.0 * 1024.0 * 1024.0;

static void risk_follows_the_tail() {
    ResourceForecast f;
    f.projected = true;
    f.limit = 8 * GiB;
    f.peak_mean = 4 * GiB; f.peak_sigma = 0.5 * GiB;
    assess_oom(f);
    CHECK(f.risk == OomRisk::Low && f.p_oom < 1e-6, "8 sigma of room is risk %d, p %.3g", (int)f.risk, f.p_oom);
    f.peak_mean = 7.5 * GiB;
    assess_oom(f);
    CHECK(f.risk == OomRisk::Medium, "one sigma of room is risk %d, p %.3f", (int)f.risk, f.p_oom);
    f.peak_mean = 8.5 * GiB;
    assess_oom(f);
    CHECK(f.risk == OomRisk::High, "a peak past the limit is risk %d", (int)f.risk);
    f.peak_mean = 7.0 * GiB; f.peak_sigma = 0.1 * GiB; f.others_sigma = 1.0 * GiB;
    assess_oom(f);
    CHECK(f.risk == OomRisk::Medium, "others' spread is ignored: risk %d, p %.3f", (int)f.risk, f.p_oom);
    f.commit_limited = true;
    assess_oom(f);
    CHECK(f.risk == OomRisk::Low, "a commit limit already counts others: risk %d", (int)f.risk);
    f.projected = false;
    assess_oom(f);
    CHECK(f.risk == OomRisk::Unknown, "no forecast has a risk");
}

// Memory that grows linearly with the work, under noise that swings below the
// high-water: the peak at the end must lie within the stated spread.
static void growth_extrapolates_with_honest_spread() {
    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0.0, 0.08 * GiB);
    const double base = 2 * GiB, per_work = 3 * GiB;
    for (double stop : {0.15, 0.3, 0.6, 0.9}) {
        GrowthForecast g;
        double truth = 0;
        for (int i = 0; i <= 1000; ++i) {
            const double w = i / 1000.0;
            const double bytes = base + per_work * w + noise(rng) - 0.1 * GiB * (i % 7 == 0);
            truth = std::max(truth, bytes);
            if (w <= stop) g.add(w, bytes);
        }
        double mean = 0, sigma = 0;
        g.peak(mean, sigma);
        std::printf("  growth @%.2f: peak %.2f +- %.2f GiB, truth %.2f\n", stop, mean / GiB, sigma / GiB, truth / GiB);
        CHECK(std::fabs(mean - truth) < 2.5 * sigma, "peak at %.2f off by %.1f sigma", stop, std::fabs(mean - truth) / sigma);
        CHECK(sigma < 0.5 * truth, "spread at %.2f says nothing: %.2f GiB", stop, sigma / GiB);
    }
    GrowthForecast early;
    early.add(0.0, 1 * GiB);
    early.add(0.02, 1.1 * GiB);
    double mean = 0, sigma = 0;
    early.peak(mean, sigma);
    CHECK(sigma >= 0.9 * GiB, "two samples claim to know the rest: +- %.2f GiB", sigma / GiB);
}

static void report_round_trips() {
    const auto dir = std::filesystem::temp_directory_path() / "spirula-run-report-test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    RunReport read;
    CHECK(!read_run_report(dir, read), "a missing report was read");
    RunReport r;
    r.sequence = 7; r.phase = "match"; r.work = 0.25;
    r.host_process = 3ull << 30; r.host_total = (1ull << 45) + 3; r.host_commit_free = 5ull << 30;
    r.host_peak = {6.5 * GiB, 0.5 * GiB, true};
    r.device_known = true; r.device_ours = 1ull << 30; r.device_others = 2ull << 30; r.device_capacity = 8ull << 30;
    r.device_peak = {2 * GiB, 0.25 * GiB};
    write_run_report(dir, r);
    CHECK(read_run_report(dir, read), "report round trip failed");
    CHECK(read.sequence == 7 && read.phase == "match" && read.work == 0.25 && read.host_total == r.host_total &&
              read.host_commit_free == r.host_commit_free && read.host_peak.mean == r.host_peak.mean &&
              read.host_peak.sigma == r.host_peak.sigma && read.host_peak.ceiling && !read.device_peak.ceiling &&
              read.device_known && read.device_ours == r.device_ours &&
              read.device_others == r.device_others && read.device_capacity == r.device_capacity &&
              read.device_peak.mean == r.device_peak.mean,
          "report fields changed in transit");
    { std::FILE* f = std::fopen(run_report_path(dir).string().c_str(), "wb"); std::fputs("{\"sequence\": ", f); std::fclose(f); }
    CHECK(!read_run_report(dir, read) && read.sequence == 7, "a torn report replaced the last good one");
    std::filesystem::remove_all(dir);
}

int main() {
    risk_follows_the_tail();
    growth_extrapolates_with_honest_spread();
    report_round_trips();
    std::printf(g_fail ? "FAIL %d\n" : "PASS resource forecast\n", g_fail);
    return g_fail ? 1 : 0;
}
