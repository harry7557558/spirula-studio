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

// Live-splat scratch the first densify step allocates for cap_max, as a share
// of the Splat category before it: the oversize draw's weights, 2.3% on bicycle
// (revised, SH 3); a sample score or a region weight adds 2.5% / 1.2% more.
constexpr double kFirstDensifySplatShare = 0.03;
constexpr double kFirstDensifySplatSigma = 0.025;

// Pipeline creation and first-touch allocation: bonsai's first 100 steps ran
// 17% slower than the next 500 at the same splat count.
constexpr int kWarmupSteps = 50;

// How the splat stages' cost scales with the count. 1.0 fit bonsai at 1/4
// resolution, 0.85 bonsai at 1/2 and 0.8 garden at 1/4; each run weighs them.
constexpr double kGammas[3] = {0.75, 0.875, 1.0};
// The same for splat x img bytes: smaller splats cover fewer tiles each, and
// the high-water grew as msplats^0.3-0.5 on garden and bonsai to 1M splats.
// The prior leans linear, the side that over-reads the peak.
constexpr double kMemPowers[3] = {0.6, 0.8, 1.0};
constexpr double kMemPrior[3] = {0.2, 0.3, 0.5};

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
// Splats shrink and spread as a run goes, so the tiles each one covers -- and
// with them bytes per splat -- move even at the cap: sigma per e-fold of steps left.
constexpr double kSizeDrift = 0.15;

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
        _mm[i] = MemModel{};
        _mm[i].p = kMemPowers[i];
        _mm[i].score = std::log(kMemPrior[i]);
    }
    _fallback_wall = 0.0;
    _fallback_n = 0;
    _save_per_msplat = 0.0;
    _saves = 0;
    _mw = MemWindow{};
    _mem_init = false;
    _demand_sd = 0.0;
    _swing_sq = 0.0;
    _ours_hw = _grow_hw = 0.0;
    _history.clear();
    _projection.clear();
    _history_stride = 1;
    _history_skip = 0;
    _others_mean = _others_sq = 0.0;
    _others_n = 0;
    _image_bytes = 0.0;
    _image_ratio = 1.0;
    _vram = VramForecast{};
    _steps.clear();
    _timed.clear();
    _run_steps = _run_wall = 0.0;
    _run_cells = StepCells{};
    _px_share = 0.75;
    _sp_share = 0.5;
}

int TrainForecast::sh_coeffs(int step) const {
    const int every = _setup.sh_degree_every;
    const int d = every > 0 ? std::min(step / every, _setup.sh_degree) : _setup.sh_degree;
    return (d + 1) * (d + 1) - 1;
}

double TrainForecast::pixel_ratio(int step) const {
    int divisor = 1;
    for (const auto& [first, d] : _setup.resolution_stages) {
        if (first > step) break;
        divisor = d;
    }
    return 1.0 / ((double)divisor * (double)divisor);
}

template <class F>
void TrainForecast::pieces(int from, int to, F&& f) const {
    const int every = _setup.sh_degree_every;
    for (int t = from; t < to;) {
        int end = to;
        if (every > 0 && t / every < _setup.sh_degree)
            end = std::min(end, (t / every + 1) * every);
        for (const auto& stage : _setup.resolution_stages)
            if (stage.first > t) {
                end = std::min(end, stage.first);
                break;
            }
        f(t, end, sh_coeffs(t), pixel_ratio(t));
        t = end;
    }
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
    _tw.pixels.push_back(pixel_ratio(step));
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
    double n = 0.0, nk = 0.0, r = 0.0;
    for (size_t i = 0; i < w.msplats.size(); ++i) {
        const double load = std::pow(w.msplats[i], t.gamma) * splat_load(w.pixels[i]);
        n += load;
        nk += load * w.coeffs[i];
        r += fixed_load(w.pixels[i]);
    }
    const double len = (double)w.msplats.size();
    n = std::max(1e-9, n / len);
    nk /= len;
    r = std::max(1e-9, r / len);
    if (first) {
        // Until the count moves, nothing measures how cost scales with it.
        // Half the splat stages' GPU time is the prior, +-70%: their per-pixel
        // share measured ~45% at 1/4 resolution and ~100% at 1/2 on bonsai.
        const double splat_part = timed ? 0.5 * std::min(g, y) : 0.5 * y;
        t.x[1] = splat_part / n;
        // The full SH band (15 coefficients) doubling the per-splat cost,
        // +-100%: whole-run fits on bonsai gave +127% at 1/4 resolution.
        t.x[2] = t.x[1] / 15.0;
        t.x[0] = (y - splat_part) / r;
        t.x[3] = timed ? (g - splat_part) / r : 0.0;
        for (auto& row : t.P) for (double& p : row) p = 0.0;
        t.P[0][0] = (0.5 * y / r) * (0.5 * y / r);
        t.P[1][1] = (0.7 * t.x[1]) * (0.7 * t.x[1]);
        t.P[2][2] = t.x[2] * t.x[2];
        t.P[3][3] = (0.5 * g / r) * (0.5 * g / r) + 1e-12;
    } else {
        // Drift: splats shrink and the screen footprint changes as training runs.
        for (int i = 0; i < 4; ++i) t.P[i][i] += (kTimeDrift * t.x[i]) * (kTimeDrift * t.x[i]) + 1e-12;
        // Each exponent is scored on how well it predicted this window's wall
        // time before seeing it; the clamp keeps one bad window from deciding.
        const double pred = t.x[0] * r + t.x[1] * n + t.x[2] * nk;
        const double s = quad(t.P, {r, n, nk, 0.0}) + var_y;
        const double z2 = std::min((y - pred) * (y - pred) / s, 4.0 * kGate * kGate);
        t.score = kForget * t.score - 0.5 * (z2 + std::log(s));
    }
    kf_update(t.x, t.P, {r, n, nk, 0.0}, y, var_y, kGate);
    // The splat stages' GPU time has less host noise than the wall clock, so
    // it pins the slope sooner once the count moves -- through its own
    // intercept c, the per-pixel work inside those same stages.
    if (timed) kf_update(t.x, t.P, {0.0, n, nk, r}, g, var_g, kGate);
    for (double& v : t.x) v = std::max(0.0, v);
}

void TrainForecast::add_save(double seconds, int64_t splats) {
    std::lock_guard<std::mutex> lk(_mu);
    if (splats <= 0) return;
    const double per = seconds / ((double)splats / 1e6);
    ++_saves;
    _save_per_msplat += (per - _save_per_msplat) / _saves;
}

TrainForecast::Remaining TrainForecast::remaining(int next_step, int64_t live) const {
    Remaining out;
    const SplatSchedule& sch = _setup.schedule;
    const int T = sch.total_steps();
    if (next_step >= T) {
        out.seconds = out.sigma = 0.0;
        return out;
    }

    const int sps = _setup.steps_per_save;
    const double m0 = std::max(1e-9, (double)live / 1e6);
    const double r0 = pixel_ratio(next_step);
    const int k0 = sh_coeffs(next_step);
    double steps = 0.0, steps_a = 0.0, steps_r = 0.0, save_msplat = 0.0;
    // The breakdown's columns: splats alone (linear, SH-weighted below), and
    // splats x pixels, at the exponents the paired runs measured.
    double lin = 0.0, lin_k = 0.0, sp = 0.0;
    double load[3] = {}, load_k[3] = {};
    int64_t last = live;
    sch.segments(next_step, live, [&](int f, int e, int64_t n) {
        const double m = (double)n / 1e6;
        pieces(f, e, [&](int a, int b, int k, double r) {
            const double len = b - a;
            steps += len;
            steps_a += len * fixed_load(r);
            steps_r += len * r / r0;
            lin += len * m / m0;
            lin_k += len * m / m0 * k;
            sp += len * std::pow(m / m0, kSplatPixelSplatExp) *
                  std::pow(r / r0, kSplatPixelTimeExp);
            for (int i = 0; i < 3; ++i) {
                const double l = std::pow(m, _tm[i].gamma) * splat_load(r);
                load[i] += len * l;
                load_k[i] += len * l * k;
            }
        });
        if (sps > 0) {
            const int lo = std::max(f, 1);
            if (e > lo) save_msplat += ((e - 1) / sps - (lo - 1) / sps) * m;
        }
        last = n;
    });
    if (sps != 0) save_msplat += (double)last / 1e6;
    out.steps = steps;
    out.pixels = steps_r;
    out.splats = lin;
    out.splats_pixels = sp;

    if (_time_init) {
        double best = _tm[0].score;
        for (const TimeModel& t : _tm) best = std::max(best, t.score);
        double wsum = 0.0, mean = 0.0, second = 0.0, b0 = 0.0, b1 = 0.0;
        for (int i = 0; i < 3; ++i) {
            const TimeModel& t = _tm[i];
            const double wi = std::exp(t.score - best);
            const double sec = t.x[0] * steps_a + t.x[1] * load[i] + t.x[2] * load_k[i];
            const double var = quad(t.P, {steps_a, load[i], load_k[i], 0.0});
            wsum += wi;
            mean += wi * sec;
            second += wi * (var + sec * sec);
            b0 += wi * t.x[1];
            b1 += wi * t.x[2];
        }
        out.seconds = mean / wsum;
        out.sigma = std::sqrt(std::max(0.0, second / wsum - out.seconds * out.seconds));
        // The splat-only kernels carry the SH cost: per coefficient, b1 / b0.
        const double now = b0 + b1 * k0;
        if (now > 0.0) out.splats = (b0 * lin + b1 * lin_k) / now;
    } else if (_fallback_n > 0) {
        out.seconds = _fallback_wall / _fallback_n * steps;
    } else {
        return out;
    }
    if (_saves > 0) {
        out.saves = _save_per_msplat * save_msplat;
        out.seconds += out.saves;
    }
    return out;
}

EtaForecast TrainForecast::eta(int next_step, int64_t live) const {
    std::lock_guard<std::mutex> lk(_mu);
    const Remaining r = remaining(next_step, live);
    EtaForecast out;
    out.seconds = r.seconds;
    out.sigma = r.sigma;
    return out;
}

// ================
// TrainForecast: where the time goes
// ================

void TrainForecast::add_breakdown(const StepSample& s) {
    std::lock_guard<std::mutex> lk(_mu);
    _steps.push_back({s.step, s.wall_s, s.save_s, s.data_wait_s, s.densify});
    while ((int)_steps.size() > _window_len) _steps.pop_front();
    if (s.timed) _timed.push_back({s.step, s.densify, s.gpu});
    // A record leaves with its window, except each stratum's latest: a densify
    // step is sampled once per refine_every steps, and may be the window's only one.
    const int oldest = _steps.front().step;
    for (auto it = _timed.begin(); it != _timed.end();) {
        bool newer_same = false;
        for (auto jt = std::next(it); jt != _timed.end() && !newer_same; ++jt)
            newer_same = jt->densify == it->densify;
        if (it->step < oldest && newer_same) it = _timed.erase(it);
        else ++it;
    }

    TimeBreakdown now;
    if (s.timed && measured(now)) {
        // Shares measured at r, as the shares at full resolution the loads
        // take: s = f r / (f r + 1 - f) solved for f.
        const auto at_full = [](double sh, double r) { return sh / (r + sh - sh * r); };
        const double r = pixel_ratio(s.step);
        const StepCells& w = now.cells;
        const double px = w.scale(StepScale::Pixels);
        const double fixed = px + w.scale(StepScale::Images) + w.scale(StepScale::Fixed) +
                             now.unaccounted;
        const double sp = w.scale(StepScale::SplatsPixels);
        const double splat = sp + w.scale(StepScale::Splats);
        if (fixed > 0.0) _px_share = at_full(px / fixed, r);
        if (splat > 0.0) _sp_share = at_full(sp / splat, std::pow(r, kSplatPixelTimeExp));
    }

    _run_steps += 1.0;
    _run_wall += s.wall_s;
    _run_cells.at(StepPart::Loading, StepScale::Pixels) += s.data_wait_s;
    _run_cells.at(StepPart::Saving, StepScale::Splats) += s.save_s;
    StepCells gpu;
    bool have = s.timed;
    if (s.timed) {
        gpu = s.gpu;
    } else {
        int n = 0;
        for (const TimedRec& t : _timed)
            if (t.densify == s.densify && t.step >= oldest) { gpu.add(t.gpu); ++n; }
        if (n > 0) {
            for (auto& row : gpu.s) for (double& v : row) v /= n;
            have = true;
        } else {
            have = window_gpu(gpu);
        }
    }
    if (have) {
        // The save's own copies are inside its host span.
        for (double& v : gpu.s[(int)StepPart::Saving]) v = 0.0;
        _run_cells.add(gpu);
    }
}

bool TrainForecast::window_gpu(StepCells& out) const {
    if (_steps.empty() || _timed.empty()) return false;
    const int oldest = _steps.front().step;
    StepCells mean[2];
    int timed[2] = {0, 0};
    for (const TimedRec& t : _timed)
        if (t.step >= oldest) { mean[t.densify].add(t.gpu); ++timed[t.densify]; }
    // A stratum with no sample in the window takes its latest one from before.
    for (int k = 0; k < 2; ++k)
        if (timed[k] == 0)
            for (auto it = _timed.rbegin(); it != _timed.rend(); ++it)
                if ((int)it->densify == k) { mean[k].add(it->gpu); timed[k] = 1; break; }
    double steps[2] = {0.0, 0.0};
    for (const StepRec& r : _steps) steps[r.densify] += 1.0;
    if (timed[0] == 0) { mean[0] = mean[1]; timed[0] = timed[1]; }
    if (timed[1] == 0) { mean[1] = mean[0]; timed[1] = timed[0]; }
    if (timed[0] == 0) return false;
    out = StepCells{};
    const double all = steps[0] + steps[1];
    for (int k = 0; k < 2; ++k) out.add(mean[k], steps[k] / all / timed[k]);
    for (double& v : out.s[(int)StepPart::Saving]) v = 0.0;
    return true;
}

namespace {

// Host spans may overlap the GPU still finishing the step before: what the
// wall clock cannot hold comes off the image wait first.
void settle_host(TimeBreakdown& b, double wall) {
    double& loading = b.cells.at(StepPart::Loading, StepScale::Pixels);
    const double over = b.cells.total() - wall;
    if (over > 0.0) loading = std::max(0.0, loading - over);
    b.unaccounted = std::max(0.0, wall - b.cells.total());
}

}  // namespace

TimeBreakdown TrainForecast::time_measured() const {
    std::lock_guard<std::mutex> lk(_mu);
    TimeBreakdown b;
    measured(b);
    return b;
}

bool TrainForecast::measured(TimeBreakdown& b) const {
    b = TimeBreakdown{};
    if (!window_gpu(b.cells)) return false;
    double wall = 0.0, save = 0.0, data = 0.0;
    for (const StepRec& r : _steps) {
        wall += r.wall;
        save += r.save;
        data += r.data;
    }
    const double n = (double)_steps.size();
    b.cells.at(StepPart::Loading, StepScale::Pixels) += data / n;
    b.cells.at(StepPart::Saving, StepScale::Splats) += save / n;
    settle_host(b, wall / n);
    b.steps = 1.0;
    b.window = (int)_steps.size();
    b.known = true;
    return true;
}

TimeBreakdown TrainForecast::time_forecast(int next_step, int64_t live) const {
    std::lock_guard<std::mutex> lk(_mu);
    TimeBreakdown now, b;
    if (!measured(now)) return b;
    b.cells = _run_cells;
    settle_host(b, _run_wall);

    const Remaining r = remaining(next_step, live);
    TimeBreakdown rest;
    for (int p = 0; p < kStepParts; ++p) {
        if (p == (int)StepPart::Saving) continue;
        rest.cells.s[p][(int)StepScale::Splats] = now.cells.s[p][(int)StepScale::Splats] * r.splats;
        rest.cells.s[p][(int)StepScale::SplatsPixels] =
            now.cells.s[p][(int)StepScale::SplatsPixels] * r.splats_pixels;
        rest.cells.s[p][(int)StepScale::Pixels] = now.cells.s[p][(int)StepScale::Pixels] * r.pixels;
        rest.cells.s[p][(int)StepScale::Images] = now.cells.s[p][(int)StepScale::Images] * r.steps;
        rest.cells.s[p][(int)StepScale::Fixed] = now.cells.s[p][(int)StepScale::Fixed] * r.steps;
    }
    rest.unaccounted = now.unaccounted * r.steps;
    // The ETA's model sets the total; the cells, how it divides.
    const double raw = rest.total();
    if (r.seconds >= 0.0 && raw > 0.0) {
        const double k = std::max(0.0, r.seconds - r.saves) / raw;
        for (auto& row : rest.cells.s) for (double& v : row) v *= k;
        rest.unaccounted *= k;
    }
    rest.cells.at(StepPart::Saving, StepScale::Splats) = r.saves;
    b.cells.add(rest.cells);
    b.unaccounted += rest.unaccounted;
    b.steps = _run_steps + r.steps;
    b.known = true;
    return b;
}

// ================
// TrainForecast: VRAM
// ================

double TrainForecast::memory_x(const MemModel& m, double msplats, int step) const {
    return std::pow(std::max(msplats, 1e-9), m.p) *
           std::pow(pixel_ratio(step), kSplatXImgVramExp);
}

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
    // What grows is measured off the process high-water, not the pool: a
    // transient inside the step -- an arena overflow, a phase growing -- is
    // the real peak, and only the backend's peak counter sees it.
    double pool = (double)m.scratch;
    for (size_t c : m.pool_cap) pool += (double)c;
    const double ours = m.has_process ? (double)m.process_bytes : pool;
    const double steady = (double)m.pool_cap[sxi] + (double)m.scratch;
    const double peak_now = m.has_process ? std::max(ours, (double)m.process_peak_bytes) : ours;
    _ours_hw = std::max(_ours_hw, peak_now);
    _grow_hw = std::max(steady, _ours_hw - (ours - steady));
    _mw.peak = std::max(_mw.peak, std::max(steady, peak_now - (ours - steady)));
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
    double best = _mm[0].score;
    for (const MemModel& mm : _mm) best = std::max(best, mm.score);
    // How far each window's own peak strays from the fit: the ceiling a view
    // sets swings from window to window, and the projection must allow for it.
    if (_mem_init && w.peak > 0.0) {
        double pred = 0.0, wsum = 0.0;
        for (const MemModel& mm : _mm) {
            const double wi = std::exp(mm.score - best);
            pred += wi * (mm.g[0] + mm.g[1] * memory_x(mm, n, m.step));
            wsum += wi;
        }
        pred /= wsum;
        if (pred > 0.0) {
            const double r = (w.peak - pred) / pred;
            _swing_sq = _swing_sq > 0.0 ? 0.7 * _swing_sq + 0.3 * r * r : r * r;
        }
    }
    const double R = (kMemFloor * _grow_hw) * (kMemFloor * _grow_hw);
    for (MemModel& mm : _mm) {
        const double x = memory_x(mm, n, m.step);
        if (!_mem_init) {
            mm.g[0] = 0.0;
            mm.g[1] = _grow_hw / x;
            mm.G[0][0] = (kChunkSigma * _grow_hw) * (kChunkSigma * _grow_hw);
            mm.G[1][1] = (kPerSplatSigma * mm.g[1]) * (kPerSplatSigma * mm.g[1]);
            mm.G[0][1] = mm.G[1][0] = 0.0;
        } else {
            mm.G[0][0] += (kMemDrift * mm.g[0]) * (kMemDrift * mm.g[0]);
            mm.G[1][1] += (kMemDrift * mm.g[1]) * (kMemDrift * mm.g[1]);
            // Scored on the high-water before seeing it, like the time model.
            const double pred = mm.g[0] + mm.g[1] * x;
            const double s = quad(mm.G, {1.0, x}) + R;
            const double z2 = std::min((_grow_hw - pred) * (_grow_hw - pred) / s, 36.0);
            mm.score = kForget * mm.score - 0.5 * (z2 + std::log(s));
        }
        kf_update(mm.g, mm.G, {1.0, x}, _grow_hw, R, 3.0);
        mm.g[0] = std::max(0.0, mm.g[0]);
        mm.g[1] = std::max(0.0, mm.g[1]);
    }
    _mem_init = true;
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
    v.sh_bytes = (double)m.sh_bytes;
    const double ours = m.has_process ? (double)m.process_bytes : pool_total;
    v.unpooled = std::max(0.0, ours - pool_total);
    v.known = true;
    v.ours = ours;
    v.capacity = m.has_total ? (double)m.total_bytes : 0.0;
    if (_others_n > 0) {
        v.others = std::max(0.0, _others_mean);
        v.others_sigma = std::max(
            std::sqrt(std::max(0.0, _others_sq - _others_mean * _others_mean)),
            0.01 * v.capacity);
    }
    _image_bytes = v.category[(int)VramCategory::Image];
    _image_ratio = pixel_ratio(m.step);

    if (_history_skip++ % _history_stride == 0)
        _history.push_back({(double)m.step, ours, v.others});
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
    // Before the first densify step the live-splat scratch it allocates is an
    // estimate, and its spread says so.
    const bool provisional = first >= next;
    const double extra_mean =
        provisional ? kFirstDensifySplatShare * v.category[(int)VramCategory::Splat] : 0.0;
    const double extra_sd =
        provisional ? kFirstDensifySplatSigma * v.category[(int)VramCategory::Splat] : 0.0;

    if (next >= sch.total_steps() || m.splats_ran <= 0 || _grow_hw <= 0.0) {
        v.peak_mean = _ours_hw;
        v.projected = next >= sch.total_steps();
        _projection.clear();
        _vram = std::move(v);
        return;
    }

    // Before the filter starts: the prior it will start from.
    const double n_now = (double)m.splats_ran / 1e6;
    struct Fit { MemModel m; double w; };
    std::array<Fit, 3> fits;
    double best = _mm[0].score;
    for (const MemModel& mm : _mm) best = std::max(best, mm.score);
    double wsum = 0.0;
    for (int i = 0; i < 3; ++i) {
        fits[i].m = _mm[i];
        fits[i].w = std::exp(_mm[i].score - best);
        wsum += fits[i].w;
        if (!_mem_init) {
            MemModel& f = fits[i].m;
            f.g[0] = 0.0;
            f.g[1] = _grow_hw / memory_x(f, n_now, m.step);
            f.G[0][0] = f.G[0][1] = f.G[1][0] = 0.0;
            f.G[1][1] = (kPerSplatSigma * f.g[1]) * (kPerSplatSigma * f.g[1]);
        }
    }
    for (Fit& f : fits) f.w /= wsum;
    v.growth_power = 0.0;
    for (const Fit& f : fits) v.growth_power += f.w * f.m.p;

    const double C = std::max(1, _setup.distinct_batches);
    const double seen = expected_max(std::min<double>(_window_len, C));
    double hw_mean = _grow_hw, hw_sd = 0.0;
    const double deferred =
        next <= _setup.deferred_step ? (double)_setup.deferred_bytes : 0.0;
    auto band = [&](int step, double g_mean, double g_sd) {
        const bool extra = provisional && step >= first;
        // Progressive resolution: the per-image buffers follow the pixels.
        const double images =
            _image_bytes * std::max(0.0, pixel_ratio(step) / _image_ratio - 1.0);
        const double mean = fixed + g_mean + images + (extra ? extra_mean : 0.0) +
                            (step >= _setup.deferred_step ? deferred : 0.0);
        const double sd = std::sqrt(g_sd * g_sd + (extra ? extra_sd * extra_sd : 0.0));
        v.projection.push_back({(double)step, mean, sd});
    };
    const double swing = std::sqrt(_swing_sq);
    band(next, hw_mean, 0.0);
    sch.segments(next, m.splats_next, [&](int f, int e, int64_t count) {
        const double n = (double)count / 1e6;
        // The exponents' mixture: its mean and its spread, the fits' own
        // uncertainty plus how far they disagree.
        double g0 = 0.0, per_splat = 0.0, model_var = 0.0;
        double mean_i[3];
        for (int i = 0; i < 3; ++i) {
            const MemModel& mm = fits[i].m;
            const double x = memory_x(mm, n, e - 1);
            mean_i[i] = mm.g[0] + mm.g[1] * x;
            g0 += fits[i].w * mm.g[0];
            per_splat += fits[i].w * mm.g[1] * x;
            model_var += fits[i].w * quad(mm.G, {1.0, x});
        }
        for (int i = 0; i < 3; ++i)
            model_var += fits[i].w * (mean_i[i] - g0 - per_splat) * (mean_i[i] - g0 - per_splat);
        // The pool keeps the largest draw: more draws at one count, larger peak.
        const double corr = std::max(
            0.0, _demand_sd * (expected_max(std::min<double>(e - f, C)) - seen));
        // Bytes per splat drift as splats shrink; the further the count is
        // extrapolated, the less the fit so far says.
        const double reach = kReach * std::log(n / n_now) * per_splat;
        const double drift = kSizeDrift * std::log((double)e / (double)std::max(1, next)) * per_splat;
        const double g_mean = g0 + per_splat * (1.0 + corr);
        const double g_sd = std::sqrt(model_var + reach * reach + drift * drift +
                                      swing * swing * per_splat * per_splat +
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

    v.projected = true;
    v.grow_peak_mean = hw_mean;
    v.peak_mean = std::max(_ours_hw, v.projection.back().mean);
    v.peak_sigma = v.projection.back().sigma;
    if (v.capacity > 0.0) {
        // Allocator fragmentation keeps the last percent out of reach.
        const double vram_limit = 0.99 * v.capacity - v.others;
        const double commit_limit =
            m.has_commit ? ours + (double)m.commit_free_bytes : vram_limit;
        v.commit_limited = commit_limit < vram_limit;
        v.limit = std::min(vram_limit, commit_limit);
        assess_oom(v);
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
