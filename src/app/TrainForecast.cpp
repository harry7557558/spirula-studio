// TrainForecast.cpp -- see TrainForecast.h and docs/notes/train-forecast.md.

#include "app/TrainForecast.h"

#include <algorithm>
#include <cmath>

namespace spirula {

namespace {

// One scalar measurement y = h . x with variance r. `gate` > 0 inflates r
// until the innovation is at most `gate` sigma, so one bad window nudges the
// state instead of yanking it.
template <int D>
void kf_update(double (&x)[D], double (&P)[D][D], const double (&h)[D], double y,
               double r, double gate = 0.0) {
    double ph[D];
    double s = r, pred = 0.0;
    for (int i = 0; i < D; ++i) {
        ph[i] = 0.0;
        for (int j = 0; j < D; ++j) ph[i] += P[i][j] * h[j];
        s += h[i] * ph[i];
        pred += h[i] * x[i];
    }
    if (!(s > 0.0)) return;
    const double innov = y - pred;
    if (gate > 0.0 && innov * innov > gate * gate * s) s = innov * innov / (gate * gate);
    for (int i = 0; i < D; ++i) x[i] += ph[i] / s * innov;
    for (int i = 0; i < D; ++i)
        for (int j = 0; j < D; ++j) P[i][j] -= ph[i] * ph[j] / s;
}

template <int D>
double quad(const double (&P)[D][D], const double (&h)[D]) {
    double q = 0.0;
    for (int i = 0; i < D; ++i)
        for (int j = 0; j < D; ++j) q += h[i] * P[i][j] * h[j];
    return std::max(0.0, q);
}

double norm_cdf(double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); }
double norm_pdf(double z) {
    return std::exp(-0.5 * z * z) / std::sqrt(2.0 * 3.14159265358979323846);
}

// Acklam's rational approximation, relative error < 1.2e-9.
double norm_quantile(double p) {
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02,
                               -2.759285104469687e+02, 1.383577518672690e+02,
                               -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02,
                               -1.556989798598866e+02, 6.680131188771972e+01,
                               -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01,
                               -2.400758277161838e+00, -2.549732539343734e+00,
                               4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01,
                               2.445134137142996e+00, 3.754408661907416e+00};
    p = std::min(std::max(p, 1e-12), 1.0 - 1e-12);
    if (p < 0.02425) {
        const double q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    if (p > 1.0 - 0.02425) return -norm_quantile(1.0 - p);
    const double q = p - 0.5, r = q * q;
    return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
}

// Expected largest of n standard normals (Blom's plotting position).
double expected_max(double n) {
    if (n <= 1.0) return 0.0;
    return norm_quantile((n - 0.375) / (n + 0.25));
}

// max(c, X) for X ~ N(mu, sigma^2), moment-matched back to a normal.
void max_with(double mu, double sigma, double c, double& mean, double& sd) {
    if (sigma <= 0.0) { mean = std::max(mu, c); sd = 0.0; return; }
    const double d = (mu - c) / sigma;
    const double P = norm_cdf(d), phi = norm_pdf(d);
    mean = mu * P + sigma * phi + c * (1.0 - P);
    const double m2 = (mu * mu + sigma * sigma) * P + (mu + c) * sigma * phi +
                      c * c * (1.0 - P);
    sd = std::sqrt(std::max(0.0, m2 - mean * mean));
}

// Mean and its variance, over the samples within 3x the median: a pipeline
// build when the SH degree steps up is a one-off, not what later steps cost.
void robust_mean(const std::vector<double>& v, double& mean, double& var) {
    std::vector<double> s = v;
    std::nth_element(s.begin(), s.begin() + s.size() / 2, s.end());
    const double cut = 3.0 * s[s.size() / 2];
    double sum = 0.0, sq = 0.0;
    int k = 0;
    for (double x : v)
        if (x <= cut) { sum += x; sq += x * x; ++k; }
    mean = sum / k;
    var = std::max(0.0, sq / k - mean * mean) / k;
}

// Live-splat scratch the first densify step allocates for cap_max, as a
// share of the Splat category before it: 14.9% on bonsai (revised, SH 3).
constexpr double kFirstDensifySplatShare = 0.15;
constexpr double kFirstDensifySplatSigma = 0.08;

// Pipeline creation and first-touch allocation: bonsai's first 100 steps ran
// 17% slower than the next 500 at the same splat count.
constexpr int kWarmupSteps = 50;

// How the splat stages' cost scales with the count. 1.0 fit bonsai at 1/4
// resolution, 0.85 bonsai at 1/2 and 0.8 garden at 1/4; each run weighs them.
constexpr double kGammas[3] = {0.75, 0.875, 1.0};

// Picked by replaying bonsai at 1/4 and 1/2 and garden at 1/4 resolution;
// docs/notes/train-forecast.md has the errors each one bought.
constexpr double kTimeFloor = 0.1;     // wall-time noise floor, per window
constexpr double kTimeDrift = 0.05;    // state drift, per window
constexpr double kGate = 2.0;          // innovations past this many sigma shrink
constexpr double kForget = 0.9;        // per window, of each exponent's score
// Growth starts all per splat (g0 = 0): fitting a fixed chunk under-read
// bonsai's peak by 14 sigma.
constexpr double kMemFloor = 0.05;     // high-water noise floor
constexpr double kMemDrift = 0.02;     // [g0, g1] drift, per window
constexpr double kChunkSigma = 0.5;    // prior sigmas, as shares of the growth
constexpr double kPerSplatSigma = 0.7;
constexpr double kReach = 0.3;         // extra sigma per e-fold of extrapolation

}  // namespace

// ================
// SplatSchedule
// ================

int64_t SplatSchedule::final_count(int from, int64_t live) const {
    int64_t last = live;
    segments(from, live, [&](int, int, int64_t n) { last = n; });
    return last;
}

int SplatSchedule::first_densify(int from) const {
    if (_d.refine_every <= 0) return -1;
    int g = std::max(from, _d.refine_start_iter + 1);
    g = (g + _d.refine_every - 1) / _d.refine_every * _d.refine_every;
    return densify_grows_at(_d, g, _total) ? g : -1;
}

// ================
// TrainForecast: time
// ================

void TrainForecast::reset(const ForecastSetup& s) {
    std::lock_guard<std::mutex> lk(_mu);
    _setup = s;
    const int every = s.schedule.refine_every();
    _window_len = std::min(500, std::max(20, every > 0 ? every : 100));
    _warmup_left = kWarmupSteps;
    _tw = Window{};
    _time_init = false;
    for (int i = 0; i < 3; ++i) {
        _tm[i] = TimeModel{};
        _tm[i].gamma = kGammas[i];
    }
    _fallback_wall = 0.0;
    _fallback_n = 0;
    _save_per_msplat = 0.0;
    _saves = 0;
    _mw = MemWindow{};
    _mem_init = false;
    _demand_sd = 0.0;
    _ours_hw = _grow_hw = 0.0;
    _history.clear();
    _projection.clear();
    _history_stride = 1;
    _history_skip = 0;
    _others_mean = _others_sq = 0.0;
    _others_n = 0;
    _vram = VramForecast{};
}

int TrainForecast::sh_coeffs(int step) const {
    const int every = _setup.sh_degree_every;
    const int d = every > 0 ? std::min(step / every, _setup.sh_degree) : _setup.sh_degree;
    return (d + 1) * (d + 1) - 1;
}

void TrainForecast::add_step(int step, double wall_s, int64_t splats,
                             double splat_gpu_s) {
    std::lock_guard<std::mutex> lk(_mu);
    // The first step builds the pipelines: seconds, not milliseconds.
    if (_warmup_left < kWarmupSteps) {
        _fallback_wall += wall_s;
        ++_fallback_n;
    }
    if (_warmup_left > 0) { --_warmup_left; return; }
    _tw.wall.push_back(wall_s);
    _tw.gpu.push_back(splat_gpu_s);
    _tw.msplats.push_back((double)splats / 1e6);
    _tw.coeffs.push_back(sh_coeffs(step));
    if ((int)_tw.wall.size() >= _window_len) close_time_window();
}

void TrainForecast::close_time_window() {
    Window w;
    std::swap(w, _tw);
    double y = 0.0, var_y = 0.0, g = 0.0, var_g = 0.0;
    robust_mean(w.wall, y, var_y);
    // Laptop GPUs change clocks for whole windows at a time: bonsai at 1/2
    // resolution ran 40-60% slow for 100 steps at a stretch, four times.
    var_y += (kTimeFloor * y) * (kTimeFloor * y);
    std::vector<double> timed;
    for (double v : w.gpu)
        if (v >= 0.0) timed.push_back(v);
    const bool has_gpu = timed.size() >= 2;
    if (has_gpu) {
        robust_mean(timed, g, var_g);
        var_g += (0.05 * g) * (0.05 * g);
    }
    for (TimeModel& t : _tm)
        update_time_model(t, w, y, var_y, g, var_g, has_gpu, !_time_init);
    _time_init = true;
}

void TrainForecast::update_time_model(TimeModel& t, const Window& w, double y,
                                      double var_y, double g, double var_g,
                                      bool timed, bool first) {
    double n = 0.0, nk = 0.0;
    for (size_t i = 0; i < w.msplats.size(); ++i) {
        const double load = std::pow(w.msplats[i], t.gamma);
        n += load;
        nk += load * w.coeffs[i];
    }
    n = std::max(1e-9, n / w.msplats.size());
    nk /= w.msplats.size();
    if (first) {
        // Until the count moves, nothing measures how cost scales with it.
        // Half the splat stages' GPU time is the prior, +-70%: their per-pixel
        // share measured ~45% at 1/4 resolution and ~100% at 1/2 on bonsai.
        const double splat_part = timed ? 0.5 * std::min(g, y) : 0.5 * y;
        t.x[1] = splat_part / n;
        // The full SH band (15 coefficients) doubling the per-splat cost,
        // +-100%: whole-run fits on bonsai gave +127% at 1/4 resolution.
        t.x[2] = t.x[1] / 15.0;
        t.x[0] = y - splat_part;
        t.x[3] = timed ? g - splat_part : 0.0;
        for (auto& row : t.P) for (double& p : row) p = 0.0;
        t.P[0][0] = (0.5 * y) * (0.5 * y);
        t.P[1][1] = (0.7 * t.x[1]) * (0.7 * t.x[1]);
        t.P[2][2] = t.x[2] * t.x[2];
        t.P[3][3] = (0.5 * g) * (0.5 * g) + 1e-12;
    } else {
        // Drift: splats shrink and the screen footprint changes as training runs.
        for (int i = 0; i < 4; ++i) t.P[i][i] += (kTimeDrift * t.x[i]) * (kTimeDrift * t.x[i]) + 1e-12;
        // Each exponent is scored on how well it predicted this window's wall
        // time before seeing it; the clamp keeps one bad window from deciding.
        const double pred = t.x[0] + t.x[1] * n + t.x[2] * nk;
        const double s = quad(t.P, {1.0, n, nk, 0.0}) + var_y;
        const double z2 = std::min((y - pred) * (y - pred) / s, 4.0 * kGate * kGate);
        t.score = kForget * t.score - 0.5 * (z2 + std::log(s));
    }
    kf_update(t.x, t.P, {1.0, n, nk, 0.0}, y, var_y, kGate);
    // The splat stages' GPU time has less host noise than the wall clock, so
    // it pins the slope sooner once the count moves -- through its own
    // intercept c, the per-pixel work inside those same stages.
    if (timed) kf_update(t.x, t.P, {0.0, n, nk, 1.0}, g, var_g, kGate);
    for (double& v : t.x) v = std::max(0.0, v);
}

void TrainForecast::add_save(double seconds, int64_t splats) {
    std::lock_guard<std::mutex> lk(_mu);
    if (splats <= 0) return;
    const double per = seconds / ((double)splats / 1e6);
    ++_saves;
    _save_per_msplat += (per - _save_per_msplat) / _saves;
}

EtaForecast TrainForecast::eta(int next_step, int64_t live) const {
    std::lock_guard<std::mutex> lk(_mu);
    EtaForecast out;
    const SplatSchedule& sch = _setup.schedule;
    const int T = sch.total_steps();
    if (next_step >= T) { out.seconds = out.sigma = 0.0; return out; }

    const int sps = _setup.steps_per_save;
    const int every = _setup.sh_degree_every;
    double steps = 0.0, save_msplat = 0.0;
    double load[3] = {0, 0, 0}, load_k[3] = {0, 0, 0};
    int64_t last = live;
    sch.segments(next_step, live, [&](int f, int e, int64_t n) {
        const double m = (double)n / 1e6;
        steps += e - f;
        for (int i = 0; i < 3; ++i) {
            const double l = std::pow(m, _tm[i].gamma);
            load[i] += (e - f) * l;
            for (int t = f; t < e;) {
                const int end = every > 0 && t / every < _setup.sh_degree
                    ? std::min(e, (t / every + 1) * every) : e;
                load_k[i] += (end - t) * l * sh_coeffs(t);
                t = end;
            }
        }
        if (sps > 0) {
            const int lo = std::max(f, 1);
            if (e > lo) save_msplat += ((e - 1) / sps - (lo - 1) / sps) * m;
        }
        last = n;
    });
    if (sps != 0) save_msplat += (double)last / 1e6;

    if (_time_init) {
        double best = _tm[0].score;
        for (const TimeModel& t : _tm) best = std::max(best, t.score);
        double wsum = 0.0, mean = 0.0, second = 0.0;
        for (int i = 0; i < 3; ++i) {
            const TimeModel& t = _tm[i];
            const double wi = std::exp(t.score - best);
            const double sec = t.x[0] * steps + t.x[1] * load[i] + t.x[2] * load_k[i];
            const double var = quad(t.P, {steps, load[i], load_k[i], 0.0});
            wsum += wi;
            mean += wi * sec;
            second += wi * (var + sec * sec);
        }
        out.seconds = mean / wsum;
        out.sigma = std::sqrt(std::max(0.0, second / wsum - out.seconds * out.seconds));
    } else if (_fallback_n > 0) {
        out.seconds = _fallback_wall / _fallback_n * steps;
    } else {
        return out;
    }
    if (_saves > 0) out.seconds += _save_per_msplat * save_msplat;
    return out;
}

// ================
// TrainForecast: VRAM
// ================

void TrainForecast::add_memory(const MemorySample& m) {
    std::lock_guard<std::mutex> lk(_mu);
    const int sxi = (int)VramCategory::SplatXImg;
    if (m.has_used && m.has_process) {
        const double o = (double)m.used_bytes - (double)m.process_bytes;
        const double a = _others_n == 0 ? 1.0 : 0.05;
        _others_mean += a * (o - _others_mean);
        _others_sq += a * (o * o - _others_sq);
        ++_others_n;
    }
    if (m.splats_ran > 0 && m.pool_used[sxi] > 0) {
        const double lr = std::log((double)m.pool_used[sxi] / (double)m.splats_ran);
        _mw.log_ratio += lr;
        _mw.log_ratio_sq += lr * lr;
    }
    // What grows is measured off the process high-water, not the pool: a step
    // that overflows the alias arena holds private buffers beside it for a
    // while, and that transient -- not the steady cap -- is the real peak.
    double pool = (double)m.scratch;
    for (size_t c : m.pool_cap) pool += (double)c;
    const double ours = m.has_process ? (double)m.process_bytes : pool;
    const double steady = (double)m.pool_cap[sxi] + (double)m.scratch;
    _ours_hw = std::max(_ours_hw, ours);
    _grow_hw = std::max(steady, _ours_hw - (ours - steady));
    if (++_mw.steps >= _window_len) close_mem_window(m);
    refresh_vram(m);
}

void TrainForecast::close_mem_window(const MemorySample& m) {
    const MemWindow w = _mw;
    _mw = MemWindow{};
    if (w.steps >= 2) {
        const double mean = w.log_ratio / w.steps;
        const double sd =
            std::sqrt(std::max(0.0, w.log_ratio_sq / w.steps - mean * mean));
        _demand_sd = _demand_sd > 0.0 ? 0.7 * _demand_sd + 0.3 * sd : sd;
    }
    // The first densify step allocates splat x img scratch of its own -- 10
    // -> 55 MiB on garden with the count unchanged -- so nothing before it
    // says how the rest grows.
    const int first = _setup.schedule.first_densify(_setup.start_step);
    if (_grow_hw <= 0.0 || m.splats_ran <= 0 || (first >= 0 && m.step <= first))
        return;
    const double n = (double)m.splats_ran / 1e6;
    if (!_mem_init) {
        _g[0] = 0.0;
        _g[1] = _grow_hw / n;
        _G[0][0] = (kChunkSigma * _grow_hw) * (kChunkSigma * _grow_hw);
        _G[1][1] = (kPerSplatSigma * _g[1]) * (kPerSplatSigma * _g[1]);
        _G[0][1] = _G[1][0] = 0.0;
        _mem_init = true;
    } else {
        _G[0][0] += (kMemDrift * _g[0]) * (kMemDrift * _g[0]);
        _G[1][1] += (kMemDrift * _g[1]) * (kMemDrift * _g[1]);
    }
    kf_update(_g, _G, {1.0, n}, _grow_hw, (kMemFloor * _grow_hw) * (kMemFloor * _grow_hw), 3.0);
    _g[0] = std::max(0.0, _g[0]);
    _g[1] = std::max(0.0, _g[1]);
}

void TrainForecast::refresh_vram(const MemorySample& m) {
    VramForecast v;
    const int nc = (int)VramCategory::Count;
    const int sxi = (int)VramCategory::SplatXImg;
    double pool_total = (double)m.scratch;
    for (int c = 0; c < nc; ++c) {
        v.category[c] = (double)m.pool_cap[c];
        pool_total += (double)m.pool_cap[c];
    }
    v.scratch = (double)m.scratch;
    const double ours = m.has_process ? (double)m.process_bytes : pool_total;
    v.unpooled = std::max(0.0, ours - pool_total);
    v.ours_bytes = ours;
    v.total_bytes = m.has_total ? (double)m.total_bytes : 0.0;
    if (_others_n > 0) {
        v.others_bytes = std::max(0.0, _others_mean);
        v.others_sigma = std::max(
            std::sqrt(std::max(0.0, _others_sq - _others_mean * _others_mean)),
            0.01 * v.total_bytes);
    }

    if (_history_skip++ % _history_stride == 0)
        _history.push_back({m.step, ours, v.others_bytes});
    if (_history.size() > 1024) {
        std::vector<VramForecast::Sample> half;
        for (size_t i = 0; i < _history.size(); i += 2) half.push_back(_history[i]);
        _history.swap(half);
        _history_stride *= 2;
    }

    const double fixed = ours - (v.category[sxi] + v.scratch);
    const SplatSchedule& sch = _setup.schedule;
    const int next = m.step + 1;
    const int first = sch.first_densify(_setup.start_step);
    v.provisional = first >= next;
    const double extra_mean =
        v.provisional ? kFirstDensifySplatShare * v.category[(int)VramCategory::Splat] : 0.0;
    const double extra_sd =
        v.provisional ? kFirstDensifySplatSigma * v.category[(int)VramCategory::Splat] : 0.0;

    if (next >= sch.total_steps() || m.splats_ran <= 0 || _grow_hw <= 0.0) {
        v.peak_mean = _ours_hw;
        v.valid = next >= sch.total_steps();
        _projection.clear();
        _vram = std::move(v);
        return;
    }

    // Before the filter starts: the prior it will start from.
    const double n_now = (double)m.splats_ran / 1e6;
    double g[2] = {_g[0], _g[1]};
    double G[2][2] = {{_G[0][0], _G[0][1]}, {_G[1][0], _G[1][1]}};
    if (!_mem_init) {
        g[0] = 0.0;
        g[1] = _grow_hw / n_now;
        G[0][0] = G[0][1] = G[1][0] = 0.0;
        G[1][1] = (kPerSplatSigma * g[1]) * (kPerSplatSigma * g[1]);
    }

    const double C = std::max(1, _setup.distinct_batches);
    const double seen = expected_max(std::min<double>(_window_len, C));
    double hw_mean = _grow_hw, hw_sd = 0.0;
    auto band = [&](int step, double g_mean, double g_sd) {
        const bool extra = v.provisional && step >= first;
        const double mean = fixed + g_mean + (extra ? extra_mean : 0.0);
        const double sd = std::sqrt(g_sd * g_sd + (extra ? extra_sd * extra_sd : 0.0));
        v.projection.push_back({step, mean, sd});
    };
    band(next, hw_mean, 0.0);
    sch.segments(next, m.splats_next, [&](int f, int e, int64_t count) {
        const double n = (double)count / 1e6;
        const double per_splat = g[1] * n;
        // The pool keeps the largest draw: more draws at one count, larger peak.
        const double corr = std::max(
            0.0, _demand_sd * (expected_max(std::min<double>(e - f, C)) - seen));
        // Bytes per splat drift as splats shrink; the further the count is
        // extrapolated, the less the fit so far says.
        const double reach = kReach * std::log(n / n_now) * per_splat;
        const double g_mean = g[0] + per_splat * (1.0 + corr);
        const double g_sd = std::sqrt(quad(G, {1.0, n}) + reach * reach +
                                      0.25 * corr * corr * per_splat * per_splat);
        double mean, sd;
        max_with(g_mean, g_sd, hw_mean, mean, sd);
        const double keep = hw_sd > 0.0
            ? 1.0 - norm_cdf((g_mean - hw_mean) / std::max(g_sd, 1.0)) : 0.0;
        sd = std::sqrt(sd * sd + keep * keep * hw_sd * hw_sd);
        if (f > next) band(f, hw_mean, hw_sd);
        hw_mean = mean;
        hw_sd = sd;
        band(e - 1, hw_mean, hw_sd);
    });

    v.valid = true;
    v.grow_peak_mean = hw_mean;
    v.peak_mean = std::max(_ours_hw, v.projection.back().mean);
    v.peak_sigma = v.projection.back().sigma;
    if (v.total_bytes > 0.0) {
        // Allocator fragmentation keeps the last percent out of reach.
        const double limit = 0.99 * v.total_bytes - v.others_bytes;
        const double sd = std::sqrt(v.peak_sigma * v.peak_sigma +
                                    v.others_sigma * v.others_sigma);
        v.p_oom = sd > 0.0 ? 1.0 - norm_cdf((limit - v.peak_mean) / sd)
                           : (v.peak_mean > limit ? 1.0 : 0.0);
        v.risk = v.p_oom < 0.1 ? OomRisk::Low
               : v.p_oom < 0.5 ? OomRisk::Medium : OomRisk::High;
    }
    _projection.swap(v.projection);
    v.projection.clear();
    _vram = std::move(v);
}

VramForecast TrainForecast::vram(bool series) const {
    std::lock_guard<std::mutex> lk(_mu);
    VramForecast out = _vram;
    if (series) {
        out.history = _history;
        out.projection = _projection;
    }
    return out;
}

}  // namespace spirula
