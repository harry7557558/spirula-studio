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
    const DensifyConfig& densify() const { return _d; }

private:
    DensifyConfig _d;
    int _total = 0;
    int64_t _cap = 0;
};

struct EtaForecast {
    double seconds = -1.0;   // < 0 while unknown
    double sigma = -1.0;
    double regularizer = 0.0;   // of `seconds`, a regularizer on its own schedule
};

// Device memory, with x the training step. "Ours" is this process; "others" is
// everything else on the device, which includes the backend's own context.
struct VramForecast : ResourceForecast {
    // Ours now, split by pool category, plus scratch and what the pool does
    // not account for; and splat x pixel plus scratch projected at the peak,
    // with the part of that the arena's per-pixel slices hold.
    std::array<double, (int)VramCategory::Count> category{};
    double scratch = 0.0;
    double unpooled = 0.0;
    double grow_peak_mean = 0.0;
    double grow_peak_pixels = 0.0;
    double growth_power = 1.0;   // the rest of it ~ msplats^p, the fits' mixture
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
    // A per-splat regularizer that runs from this step on (-1: none): its
    // time is measured apart (add_regularizer) and forecast on its schedule.
    int reg_start_step = -1;
};

struct MemorySample {
    int step = 0;
    int resolution_divisor = 0; // 0: use the planned schedule (older traces)
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
    // Of pool_cap[SplatXPix], the alias arena; and per PoolPhase the most its
    // per-pixel, its per-pair and (as sized for cap_max) its per-splat slices
    // came to (DevicePool::arena_phases).
    size_t arena_cap = 0;
    std::array<size_t, (int)PoolPhase::Count> phase_pixels{}, phase_splats{}, phase_capped{};
    // The most visible and splat-tile pairs one forward has made
    // (engine_get_isect_peaks): what the tile phase's bands are cut from.
    int64_t isect_pairs = 0, isect_count = 0;
    int64_t isect_step = 0;      // the step's last forward's splat-tile pairs
    // isect_count at the finest binning tile the engine's search may still
    // try, 0 when it cannot go finer (engine_bin_finer_isects).
    int64_t isect_finer = 0;
};

// One finished step for the time breakdown: its wall clock with the host
// spans inside it, and on a timed step every kernel's GPU time.
struct StepSample {
    int step = 0;
    int resolution_divisor = 0;
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

// Splat x pixel kernels' time ~ r^0.45 msplats^0.5 and splat x pixel bytes ~
// r^0.35, r the pixel count over the full resolution's: paired runs on garden
// and bonsai at two resolutions, docs/notes/step-timing.md.
constexpr double kSplatPixelTimeExp = 0.45;
constexpr double kSplatPixelSplatExp = 0.5;
constexpr double kSplatXPixVramExp = 0.35;

class TrainForecast {
public:
    void reset(const ForecastSetup& s);

    // A finished step: its wall time without any checkpoint save, the splats
    // it ran with, and the GPU seconds of its splat-proportional stages
    // (< 0 when not timed). add_breakdown follows this call.
    void add_step(int step, double wall_s, int64_t splats, double splat_gpu_s,
                  int resolution_divisor = 0);
    void add_save(double seconds, int64_t splats);
    // One pass of the regularizer over `splats`: its GPU seconds, measured on
    // a step that ran it or scaled up from a probe before it starts.
    void add_regularizer(double seconds, int64_t splats, bool probed);
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
    // Step time less reg_seconds = a ra + (b0 + b1 * sh_coeffs) * msplats^gamma
    // * rb, the splat stages' GPU time = c ra + the same (ra, rb: fixed_load,
    // splat_load): a Kalman filter over [a, b0, b1, c] per exponent.
    struct TimeModel {
        double gamma = 1.0;
        double x[4] = {0, 0, 0, 0};
        double P[4][4] = {};
        double score = 0.0;              // discounted log-likelihood
    };
    // Separate splat and pixel exponents keep resolution changes from being
    // learned as changes in the cost per splat.
    struct ColumnModel {
        bool init = false;
        double a = 0.0, a_var = 0.0;
        double b[2] = {0, 0}, B[2][2] = {};
        double s[3] = {0, 0, 0}, S[3][3] = {};
    };
    struct MemWindow {
        int steps = 0;
        std::vector<float> log_ratio;    // per step: splat x pixel bytes in use, per splat
        double peak = 0.0;               // the window's own high-water of the growth
    };
    // What follows the splats (splat_demand), at its high-water = g0 + g1 *
    // msplats^p * r^q: a Kalman filter over [g0, g1], one per exponent, scored
    // like TimeModel.
    struct MemModel {
        double p = 1.0;
        double g[2] = {0, 0};
        double G[2][2] = {};
        double score = 0.0;
    };
    struct Remaining {
        double seconds = -1.0, sigma = -1.0;
        double saves = 0.0;      // of `seconds`, the checkpoint saves
        double reg = 0.0;        // and the regularizer, when it is measured
        double steps = 0.0;
        // Steps left in units of the next step's cost, for the cells that
        // scale with the splats, with splats and pixels, and with pixels.
        double splats = 0.0, splats_pixels = 0.0, pixels = 0.0;
    };
    struct StepRec {
        int step;
        double wall, save, data, pixels;
        bool densify;
    };
    struct TimedRec {
        int step;
        bool densify;
        StepCells gpu;
        double pixels;
    };

    int sh_coeffs(int step) const;
    // The regularizer's seconds in a step of `msplats`, once measured.
    double reg_seconds(int step, double msplats) const {
        const int first = _setup.reg_start_step;
        return _reg_n > 0 && first >= 0 && step >= first ? _reg_per_msplat * msplats : 0.0;
    }
    double fixed_load(double r) const { return _px_share * r + 1.0 - _px_share; }
    double splat_load(double r) const {
        return _sp_share * std::pow(r, kSplatPixelTimeExp) + 1.0 - _sp_share;
    }
    double memory_x(const MemModel& m, double msplats, int step) const;
    double sample_ratio(int step, int divisor) const;
    // f(first, end, coeffs, r) over [from, to) in pieces of constant SH
    // coefficients and pixel ratio.
    template <class F>
    void pieces(int from, int to, F&& f) const;
    Remaining remaining(int next_step, int64_t live) const;
    // The window's per-step GPU cells, densify steps weighted by their share.
    bool window_gpu(StepCells& out, double pixels = 0.0) const;
    bool measured(TimeBreakdown& b) const;
    void close_time_window();
    void update_time_model(TimeModel& t, const Window& w, double y, double var_y,
                           double g, double var_g, bool timed, bool first);
    void update_column_model(const Window& w, double y, double var_y, const StepCells& cells);
    void close_mem_window(const MemorySample& m);
    void refresh_vram(const MemorySample& m);

    mutable std::mutex _mu;
    ForecastSetup _setup;
    int _window_len = 100;
    int _warmup_left = 0;

    Window _tw;
    bool _time_init = false;
    std::array<TimeModel, 3> _tm;
    ColumnModel _cm;
    double _fallback_wall = 0.0, _fallback_load = 0.0;
    double _host_floor = 0.0; // seconds per step limited by asynchronous image supply
    int _fallback_n = 0;
    double _save_per_msplat = 0.0;
    int _saves = 0;
    double _reg_per_msplat = 0.0;        // seconds a step per million splats
    std::deque<double> _reg_recent;      // its last measurements: it is their median
    int _reg_n = 0;
    bool _reg_ran = false;               // measured on a real pass, not a probe

    // Started once the first densify step has run.
    MemWindow _mw;
    bool _mem_init = false;
    std::array<MemModel, 3> _mm;
    double _demand_sd = 0.0;             // of a step's log demand, its upper tail's
    bool _demand_init = false;
    double _swing_sq = 0.0;              // windows' peaks above the fit, relative, squared
    bool _swing_init = false;
    double _ours_hw = 0.0, _grow_hw = 0.0;
    double _transient = 0.0;             // the most a step's peak stood above its end
    int64_t _held_count = 0;             // the count the last steps ran with,
    int _held_steps = 0;                 //   and how many of them

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
