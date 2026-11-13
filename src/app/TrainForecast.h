#pragma once

// TrainForecast -- where a training run is heading: the wall time it has left
// and the VRAM it will peak at, fitted online from the steps already run and
// the densify schedule still to come. The models and the measurements behind
// their constants: docs/notes/train-forecast.md.

#include "app/ResourceForecast.h"
#include "core/PoolSlots.h"
#include "engine/EngineConfig.h"
#include "engine/StepTiming.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
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

struct EtaForecast {
    double seconds = -1.0;   // < 0 while unknown
    double sigma = -1.0;
};

// Device memory, with x the training step. "Ours" is this process; "others" is
// everything else on the device, which includes the backend's own context.
struct VramForecast : ResourceForecast {
    // Ours now, split by pool category, plus scratch and what the pool does
    // not account for; and the splat x img share projected at the peak.
    std::array<double, (int)VramCategory::Count> category{};
    double scratch = 0.0;
    double unpooled = 0.0;
    double grow_peak_mean = 0.0;
    double growth_power = 1.0;   // splat x img ~ msplats^p, the fits' mixture
    double sh_bytes = 0.0;       // of `category[Splat]`, the SH rows
};

struct ForecastSetup {
    SplatSchedule schedule;
    int start_step = 0;
    int steps_per_save = 0;        // 0: never saves; < 0: only the final one
    int distinct_batches = 1;      // views a step can draw, for the peak draw
    int sh_degree = 0;             // the step's SH degree is
    int sh_degree_every = 0;       //   min(step / sh_degree_every, sh_degree)
    // Progressive resolution: (first step, image divisor), ascending.
    std::vector<std::pair<int, int>> resolution_stages;
    // Splat memory first allocated, for cap_max, at a later step: the
    // local-PCA regularizer's neighbour caches.
    uint64_t deferred_bytes = 0;
    int deferred_step = 0;
};

struct MemorySample {
    int step = 0;
    int64_t splats_ran = 0;      // what the step trained
    int64_t splats_next = 0;     // what the next one will, after densify
    std::array<size_t, (int)VramCategory::Count> pool_used{};
    std::array<size_t, (int)VramCategory::Count> pool_cap{};
    size_t scratch = 0;
    bool has_process = false, has_used = false, has_total = false, has_commit = false;
    uint64_t process_bytes = 0, used_bytes = 0, total_bytes = 0;
    uint64_t process_peak_bytes = 0;   // since the previous sample
    uint64_t commit_free_bytes = 0;
    size_t sh_bytes = 0;         // the splats' SH rows, part of the Splat category
};

// One finished step for the time breakdown: its wall clock with the host
// spans inside it, and on a timed step every kernel's GPU time.
struct StepSample {
    int step = 0;
    double wall_s = 0.0;       // the save included
    double save_s = 0.0;
    double data_wait_s = 0.0;
    bool densify = false;      // densify steps are sampled as their own stratum
    bool timed = false;
    StepCells gpu;
};

// Where time goes, in seconds over `steps` steps. The host spans are the
// Loading and Saving rows; `unaccounted` is the wall time nothing above
// explains: host work between kernels and an idle GPU.
struct TimeBreakdown {
    bool known = false;
    double steps = 0.0;
    int window = 0;            // steps averaged, per step
    StepCells cells;
    double unaccounted = 0.0;
    double total() const { return cells.total() + unaccounted; }
};

// Splat x pixel kernels' time ~ r^0.45 msplats^0.5 and splat x img bytes ~
// r^0.35, r the pixel count over the full resolution's: paired runs on garden
// and bonsai at two resolutions, docs/notes/step-timing.md.
constexpr double kSplatPixelTimeExp = 0.45;
constexpr double kSplatPixelSplatExp = 0.5;
constexpr double kSplatXImgVramExp = 0.35;

class TrainForecast {
public:
    void reset(const ForecastSetup& s);

    // A finished step: its wall time without any checkpoint save, the splats
    // it ran with, and the GPU seconds of its splat-proportional stages
    // (< 0 when not timed).
    void add_step(int step, double wall_s, int64_t splats, double splat_gpu_s);
    void add_save(double seconds, int64_t splats);
    void add_memory(const MemorySample& m);
    void add_breakdown(const StepSample& s);

    // Time left from `next_step` with `live` splats entering it.
    EtaForecast eta(int next_step, int64_t live) const;
    // Without the history and projection series when `series` is false.
    VramForecast vram(bool series = true) const;
    // Per step, averaged over the last window of steps.
    TimeBreakdown time_measured() const;
    // This run from its first step to the last: measured, then forecast.
    TimeBreakdown time_forecast(int next_step, int64_t live) const;
    // The image size's pixel count over the full resolution's at `step`.
    double pixel_ratio(int step) const;

private:
    struct Window {
        std::vector<double> wall, gpu;     // gpu < 0: step not timed
        std::vector<double> msplats;
        std::vector<double> pixels;        // pixel_ratio
        std::vector<int> coeffs;
    };
    // Step time = a ra + (b0 + b1 * sh_coeffs) * msplats^gamma * rb, the splat
    // stages' GPU time = c ra + the same term (ra, rb: fixed_load, splat_load):
    // a Kalman filter over [a, b0, b1, c] per exponent, weighted by prediction.
    struct TimeModel {
        double gamma = 1.0;
        double x[4] = {0, 0, 0, 0};
        double P[4][4] = {};
        double score = 0.0;              // discounted log-likelihood
    };
    struct MemWindow {
        int steps = 0;
        double log_ratio = 0.0, log_ratio_sq = 0.0;
        double peak = 0.0;               // the window's own high-water of the growth
    };
    // splat x img + scratch, at its high-water = g0 + g1 * msplats^p * r^q:
    // a Kalman filter over [g0, g1], one per exponent, scored like TimeModel.
    struct MemModel {
        double p = 1.0;
        double g[2] = {0, 0};
        double G[2][2] = {};
        double score = 0.0;
    };
    struct Remaining {
        double seconds = -1.0, sigma = -1.0;
        double saves = 0.0;      // of `seconds`, the checkpoint saves
        double steps = 0.0;
        // Steps left in units of the next step's cost, for the cells that
        // scale with the splats, with splats and pixels, and with pixels.
        double splats = 0.0, splats_pixels = 0.0, pixels = 0.0;
    };
    struct StepRec {
        int step;
        double wall, save, data;
        bool densify;
    };
    struct TimedRec {
        int step;
        bool densify;
        StepCells gpu;
    };

    int sh_coeffs(int step) const;
    double fixed_load(double r) const { return _px_share * r + 1.0 - _px_share; }
    double splat_load(double r) const {
        return _sp_share * std::pow(r, kSplatPixelTimeExp) + 1.0 - _sp_share;
    }
    double memory_x(const MemModel& m, double msplats, int step) const;
    // f(first, end, coeffs, r) over [from, to) in pieces of constant SH
    // coefficients and pixel ratio.
    template <class F>
    void pieces(int from, int to, F&& f) const;
    Remaining remaining(int next_step, int64_t live) const;
    // The window's per-step GPU cells, densify steps weighted by their share.
    bool window_gpu(StepCells& out) const;
    bool measured(TimeBreakdown& b) const;
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

    // Started once the first densify step has run.
    MemWindow _mw;
    bool _mem_init = false;
    std::array<MemModel, 3> _mm;
    double _demand_sd = 0.0;
    double _swing_sq = 0.0;              // windows' peaks about the fit, relative, squared
    double _ours_hw = 0.0, _grow_hw = 0.0;

    std::vector<VramForecast::Sample> _history;
    std::vector<VramForecast::Band> _projection;
    int _history_stride = 1;
    int _history_skip = 0;
    double _others_mean = 0.0, _others_sq = 0.0;
    int _others_n = 0;
    double _image_bytes = 0.0;           // the Image category at _image_ratio
    double _image_ratio = 1.0;
    VramForecast _vram;

    // Of the time that does not follow the splats, and of what does, the shares
    // that follow the pixels: from the breakdown, or these priors without it.
    double _px_share = 0.75, _sp_share = 0.5;
    std::deque<StepRec> _steps;          // the last window
    std::deque<TimedRec> _timed;         // in the window, plus each stratum's latest
    // This run so far: wall, host spans and the GPU cells, estimated from the
    // window on the steps that were not timed.
    double _run_steps = 0.0, _run_wall = 0.0;
    StepCells _run_cells;
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
            const int64_t next = densify_target(_d, n, densify_cap_at(_d, g, _cap));
            if (next == n) {
                // Held at a budget stage's ceiling: wait for a later stage to raise it.
                if (densify_cap_at(_d, stop - 1, _cap) > n) continue;
                break;
            }
            f(s, g + 1, n);
            n = next;
            s = g + 1;
        }
    }
    if (s < _total) f(s, _total, n);
}

}  // namespace spirula
