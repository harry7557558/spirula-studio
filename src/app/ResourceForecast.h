#pragma once

// One resource -- system RAM or device memory -- as a running job holds it and
// where it is heading: the peak expected with its spread, what the machine
// lets the job reach, and the chance it runs out. The trainer, dense, meshing
// and mapping all fill this one shape. docs/notes/memory-forecast.md.

#include <cstdint>
#include <vector>

namespace spirula {

enum class OomRisk : uint8_t { Unknown, Low, Medium, High };

struct ResourceForecast {
    bool known = false;          // something has been measured
    double capacity = 0.0;       // the machine's: device size or physical RAM
    double ours = 0.0;           // this job, now
    double others = 0.0;         // everything else, now
    double others_sigma = 0.0;
    // What ours may reach: capacity less others, or on Windows ours plus the
    // commit still free when that is lower (commit_limited).
    double limit = 0.0;
    bool commit_limited = false;

    bool projected = false;      // the peak below is a forecast
    double peak_mean = 0.0;      // of ours, over the rest of the job
    double peak_sigma = 0.0;
    double p_oom = 0.0;
    OomRisk risk = OomRisk::Unknown;

    // x is the job's own axis: training steps, or seconds since start.
    struct Sample { double x, ours, others; };
    std::vector<Sample> history;
    struct Band { double x, mean, sigma; };
    std::vector<Band> projection;    // empty: the peak is one level to the end
};

// p_oom and risk from the peak, the spread of others, and the limit.
void assess_oom(ResourceForecast& f);

// A job's memory against its own progress (0..1), extrapolated to the end. For
// a job with no plan of its own: mapping, whose model grows as images join it.
class GrowthForecast {
public:
    void reset() { *this = GrowthForecast{}; }
    void add(double work, double bytes);
    // Peak of ours from here to work = 1.
    void peak(double& mean, double& sigma) const;

private:
    struct Point { double w, hw; };
    std::vector<Point> _pts;     // the high-water against work, one per new work value
    double _hw = 0.0;
    double _jitter_sq = 0.0;     // swing below the high-water, which a later spike can repeat
    double _w = 0.0;
};

}  // namespace spirula
