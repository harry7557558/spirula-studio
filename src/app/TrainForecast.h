#pragma once

// TrainForecast -- where a training run is heading: the wall time it has left
// and the VRAM it will peak at, fitted online from the steps already run and
// the densify schedule still to come. The models and the measurements behind
// their constants: docs/notes/train-forecast.md.

#include "core/PoolSlots.h"
#include "engine/EngineConfig.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace spirula {

// The splat count each step trains with, replayed from the densify schedule.
class SplatSchedule {
public:
    SplatSchedule() = default;
    SplatSchedule(const DensifyConfig& d, int total_steps, int64_t cap)
        : _d(d), _total(total_steps), _cap(cap) {}

    // f(first, end, n): steps [first, end) run with n splats, covering
    // [from, total_steps) given `live` splats entering step `from`.
    template <class F>
    void segments(int from, int64_t live, F&& f) const;

    int64_t final_count(int from, int64_t live) const;
    // The first densify step >= from, or -1. It runs even at the cap: it
    // relocates, and allocates the live-splat scratch.
    int first_densify(int from) const;
    int total_steps() const { return _total; }
    int refine_every() const { return _d.refine_every; }
    int64_t cap() const { return _cap; }

private:
    DensifyConfig _d;
    int _total = 0;
    int64_t _cap = 0;
};

enum class OomRisk : uint8_t { Unknown, Low, Medium, High };

struct EtaForecast {
    double seconds = -1.0;   // < 0 while unknown
    double sigma = -1.0;
};

// All byte figures are device memory. "Ours" is this process; "others" is
// everything else on the device, which includes the backend's own context.
struct VramForecast {
    bool valid = false;
    // No densify step has run yet, so the live-splat scratch the first one
    // allocates is an estimate rather than a measurement.
    bool provisional = false;
    double total_bytes = 0.0;
    double ours_bytes = 0.0;
    double others_bytes = 0.0;
    double others_sigma = 0.0;

    struct Sample { int step; double ours; double others; };
    std::vector<Sample> history;
    struct Band { int step; double mean; double sigma; };
    std::vector<Band> projection;      // ours, from the next step to the end

    double peak_mean = 0.0;            // ours at the last step
    double peak_sigma = 0.0;
    double p_oom = 0.0;                // chance ours + others exceeds the device
    OomRisk risk = OomRisk::Unknown;

    // Ours now, split by pool category, plus scratch and what the pool does
    // not account for; and the splat x img share projected at the peak.
    std::array<double, (int)VramCategory::Count> category{};
    double scratch = 0.0;
    double unpooled = 0.0;
    double grow_peak_mean = 0.0;
};

struct ForecastSetup {
    SplatSchedule schedule;
    int start_step = 0;
    int steps_per_save = 0;        // 0: never saves; < 0: only the final one
    int distinct_batches = 1;      // views a step can draw, for the peak draw
    int sh_degree = 0;             // the step's SH degree is
    int sh_degree_every = 0;       //   min(step / sh_degree_every, sh_degree)
};

struct MemorySample {
    int step = 0;
    int64_t splats_ran = 0;      // what the step trained
    int64_t splats_next = 0;     // what the next one will, after densify
    std::array<size_t, (int)VramCategory::Count> pool_used{};
    std::array<size_t, (int)VramCategory::Count> pool_cap{};
    size_t scratch = 0;
    bool has_process = false, has_used = false, has_total = false;
    uint64_t process_bytes = 0, used_bytes = 0, total_bytes = 0;
};

class TrainForecast {
public:
    void reset(const ForecastSetup& s);

    // A finished step: its wall time without any checkpoint save, the splats
    // it ran with, and the GPU seconds of its splat-proportional stages
    // (< 0 when not timed).
    void add_step(int step, double wall_s, int64_t splats, double splat_gpu_s);
    void add_save(double seconds, int64_t splats);
    void add_memory(const MemorySample& m);

    // Time left from `next_step` with `live` splats entering it.
    EtaForecast eta(int next_step, int64_t live) const;
    // Without the history and projection series when `series` is false.
    VramForecast vram(bool series = true) const;

private:
    struct Window {
        std::vector<double> wall, gpu;     // gpu < 0: step not timed
        std::vector<double> msplats;
        std::vector<int> coeffs;
    };
    // Step time = a + (b0 + b1 * sh_coeffs) * msplats^gamma, and the splat
    // stages' GPU time = c + the same term: a Kalman filter over
    // [a, b0, b1, c], one per exponent, weighted by how well each predicted.
    struct TimeModel {
        double gamma = 1.0;
        double x[4] = {0, 0, 0, 0};
        double P[4][4] = {};
        double score = 0.0;              // discounted log-likelihood
    };
    struct MemWindow {
        int steps = 0;
        double log_ratio = 0.0, log_ratio_sq = 0.0;
    };

    int sh_coeffs(int step) const;
    void close_time_window();
    void update_time_model(TimeModel& t, const Window& w, double y, double var_y,
                           double g, double var_g, bool timed, bool first);
    void close_mem_window(const MemorySample& m);
    void refresh_vram(const MemorySample& m);

    mutable std::mutex _mu;
    ForecastSetup _setup;
    int _window_len = 100;
    int _warmup_left = 0;

    Window _tw;
    bool _time_init = false;
    std::array<TimeModel, 3> _tm;
    double _fallback_wall = 0.0;
    int _fallback_n = 0;
    double _save_per_msplat = 0.0;
    int _saves = 0;

    // splat x img + scratch, at its high-water = g0 + g1 * msplats: a Kalman
    // filter over [g0, g1], started once the first densify step has run.
    MemWindow _mw;
    bool _mem_init = false;
    double _g[2] = {0, 0};
    double _G[2][2] = {};
    double _demand_sd = 0.0;
    double _ours_hw = 0.0, _grow_hw = 0.0;

    std::vector<VramForecast::Sample> _history;
    std::vector<VramForecast::Band> _projection;
    int _history_stride = 1;
    int _history_skip = 0;
    double _others_mean = 0.0, _others_sq = 0.0;
    int _others_n = 0;
    VramForecast _vram;
};

template <class F>
void SplatSchedule::segments(int from, int64_t live, F&& f) const {
    int s = from;
    int64_t n = live;
    const int stop = std::min(
        _total, std::max(_d.refine_stop_iter, _total - _d.refine_stop_num_iter));
    if (_d.refine_every > 0 && n < _cap) {
        int g = std::max(s, _d.refine_start_iter + 1);
        g = (g + _d.refine_every - 1) / _d.refine_every * _d.refine_every;
        for (; g < stop && n < _cap; g += _d.refine_every) {
            const int64_t next = densify_target(_d, n, _cap);
            if (next == n) break;
            f(s, g + 1, n);
            n = next;
            s = g + 1;
        }
    }
    if (s < _total) f(s, _total, n);
}

}  // namespace spirula
