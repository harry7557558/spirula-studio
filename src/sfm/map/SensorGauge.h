#pragma once
// The gauge of a finished model from the video's own sensors: up from the
// IMU, scale from the accelerometer and from the GPS log, heading and place
// from the GPS. Every reading is a factor on one small state (a Sim3 plus
// biases and per-lens nuisances), initialised in closed form and refined by
// one robust Levenberg-Marquardt solve; a missing sensor is an absent
// factor, a degenerate one is refused by its own uncertainty. Nothing here
// touches the poses. docs/notes/imu-gps-for-sfm.md is the design.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"
#include "sfm/core/SensorTimeline.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/map/ImuExtrinsic.h"
#include "sfm/map/MetricGauge.h"
#include "sfm/map/Orient.h"

namespace sfm {

// One telemetry source and the images it covers.
struct SensorCapture {
    std::string prefix;        // image names under this prefix; "" covers all
    std::string path;          // for reporting
    std::string camera;        // the file's camera string
    double fps = 0;            // frame index in the stem -> seconds
    double time_offset = 0;    // seconds added to every frame time
    double readout = 0;        // rolling-shutter readout, seconds
    const SensorTimeline* timeline = nullptr;
};

enum class SensorMode { Auto, Up, None };

struct SensorGaugeOptions {
    SensorMode mode = SensorMode::Auto;
    bool gps_full = false;          // fit altitude too (D75 says not to)
    double gps_max_error = 5.0;     // metres, the RANSAC radius ...
    double gps_max_error_frac = 0;  // ... or this fraction of the fixes' RMS radius
    bool refine = true;             // the joint solve after the closed forms
    bool verbose = false;
};

enum class SensorFail { None, NoTelemetry, NoFrames, NoUp };
enum class SensorNoScale { None, NotAsked, NoSource, ImuWeak, GpsRefused, Disagree };

struct SensorGroupReport {
    int capture = 0;
    std::string name;               // the image-name directory
    ExtrinsicFit fit;
    int frames = 0, triples = 0, triple_inliers = 0;
    double scale = 0, scale_sigma = 0;   // closed-form, relative sigma
    double g_norm = 0, g_angle_deg = 0;  // gravity recovered with the scale
    bool flipped = false;                // the scale came out negative: X negated
};

struct SensorGaugeResult {
    bool applied = false;    // T is the transform to apply
    bool metric = false;     // ... and its unit is the metre
    bool up_from_imu = false;
    bool scale_from_imu = false, scale_from_gps = false;
    bool place_from_gps = false;
    bool disagree = false;   // IMU and GPS scales differed beyond 3 sigma
    Sim3 T;
    UpConsensus up;
    std::vector<TimeOffsetFit> time_offsets;   // per capture
    std::vector<SensorGroupReport> groups;
    int frames_timed = 0, frames_untimed = 0;
    double scale_imu = 0, scale_imu_sigma = 0;   // relative
    double scale_gps = 0, scale_gps_sigma = 0;
    double scale = 1, scale_sigma = 0;           // the joint answer
    double tilt_sigma_deg = 0;
    MetricFit gps;
    int gps_frames = 0;
    int lm_iterations = 0;
    double lm_cost0 = 0, lm_cost = 0;
    SensorFail fail = SensorFail::None;         // why nothing was applied
    SensorNoScale no_scale = SensorNoScale::None;   // why an applied frame is not metric
};

namespace sensor_detail {

inline int stemIndex(const std::string& name) {
    size_t end = name.find_last_of('.');
    const size_t slash = name.find_last_of('/');
    if (end == std::string::npos || (slash != std::string::npos && end < slash)) end = name.size();
    size_t b = end;
    while (b > 0 && std::isdigit((unsigned char)name[b - 1])) b--;
    if (b == end || end - b > 9) return -1;
    return std::atoi(name.substr(b, end - b).c_str());
}

inline std::string dirOf(const std::string& name) {
    const size_t slash = name.find_last_of('/');
    return slash == std::string::npos ? std::string() : name.substr(0, slash);
}

inline bool underPrefix(const std::string& name, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) return false;
    return name[prefix.size()] == '/';
}

struct PairMeas {
    int j = 0, k = 0;    // frame indices
    Preintegration P;
    Mat3 B;              // attitude-only captures: R_i(j) <- i(k)
    bool has_preint = false;
};

struct Triple {
    int j = 0, k = 0, l = 0;
    int p1 = 0, p2 = 0;      // pair indices
};

inline double huberW(double r, double sigma) {
    const double x = std::fabs(r) / sigma;
    return x <= 1.345 ? 1.0 : 1.345 / x;
}

// The velocity-free position constraint over two consecutive intervals
// (Mur-Artal and Tardos 2017, sec. IV): scale * L = Q in the model frame,
// with the frames' up already known. `X` maps IMU to camera per group.
struct TripleTerms {
    Vec3 L;      // multiplies the scale
    double Gs = 0;   // multiplies the gravity vector
    Vec3 Q;      // the pre-integrated part at the given biases
    Mat3 Ja, Jg; // d Q / d accel bias, d Q / d gyro bias
};

// x = pseudo-inverse solve of the normal equations N x = b (n <= 16).
inline std::vector<double> solveNormal(std::vector<double> N, const std::vector<double>& b, int n) {
    std::vector<double> ev, V, x((size_t)n, 0.0);
    jacobiEigenSymmetric(N, n, ev, V);
    double emax = 0;
    for (double e : ev) emax = std::max(emax, e);
    for (int i = 0; i < n; i++) {
        if (!(ev[(size_t)i] > 1e-12 * emax)) continue;
        double d = 0;
        for (int r = 0; r < n; r++) d += V[(size_t)r * n + i] * b[(size_t)r];
        for (int r = 0; r < n; r++) x[(size_t)r] += V[(size_t)r * n + i] * d / ev[(size_t)i];
    }
    return x;
}

}  // namespace sensor_detail

class SensorGaugeSolver {
public:
    SensorGaugeSolver(const Reconstruction& rec, const std::vector<SensorCapture>& caps,
                      const SensorGaugeOptions& opt)
        : _rec(rec), _caps(caps), _opt(opt) {}

    SensorGaugeResult run() {
        using namespace sensor_detail;
        SensorGaugeResult out;
        if (_opt.mode == SensorMode::None || _caps.empty()) {
            out.fail = SensorFail::NoTelemetry;
            return out;
        }
        collectFrames(out);
        if (_frames.size() < 3) {
            out.fail = SensorFail::NoFrames;
            return out;
        }
        _mean_up_w = meanCameraUp(_rec).normalized();
        timeOffsets(out);
        upVotes();
        calibrateGroups(out);
        consensus(out);
        buildPairs();
        if (_opt.mode == SensorMode::Auto) {
            imuScale(out);
            bool flipped = false;
            for (SensorGroupReport& g : out.groups)
                if (g.flipped) flipped = true;
            if (flipped) {
                consensus(out);
                imuScale(out);
            }
            gpsScale(out);
        }
        assemble(out);
        return out;
    }

private:
    const Reconstruction& _rec;
    const std::vector<SensorCapture>& _caps;
    const SensorGaugeOptions& _opt;

    std::vector<SensorFrame> _frames;            // time-sorted within each group
    std::vector<std::vector<int>> _group_frames; // frame indices per group
    std::vector<int> _group_capture;
    std::vector<std::string> _group_name;
    std::vector<Mat3> _X;                        // per group, camera <- IMU
    std::vector<bool> _group_ok;
    std::vector<double> _gyro_sign;
    std::vector<std::vector<sensor_detail::PairMeas>> _pairs;   // per group
    std::vector<std::vector<sensor_detail::Triple>> _triples;
    Vec3 _mean_up_w;
    Vec3 _up_w{0, 0, 1};
    bool _up_ok = false;
    std::vector<Vec3> _gps_enu;      // per frame; valid where _gps_ok
    std::vector<char> _gps_ok;

    void collectFrames(SensorGaugeResult& out) {
        using namespace sensor_detail;
        std::map<std::string, int> groups;
        for (const auto& kv : _rec.images) {
            const Image& im = kv.second;
            if (!im.registered) continue;
            int best = -1;
            for (size_t c = 0; c < _caps.size(); c++) {
                if (!_caps[c].timeline || !(_caps[c].fps > 0)) continue;
                if (!underPrefix(im.name, _caps[c].prefix)) continue;
                if (best < 0 || _caps[c].prefix.size() > _caps[(size_t)best].prefix.size()) best = (int)c;
            }
            const int idx = stemIndex(im.name);
            if (best < 0 || idx < 0) {
                out.frames_untimed++;
                continue;
            }
            const SensorCapture& cap = _caps[(size_t)best];
            SensorFrame f;
            f.image_id = kv.first;
            f.t = idx / cap.fps + 0.5 * cap.readout + cap.time_offset;
            f.R = im.pose.R;
            f.c = cameraCenter(im.pose);
            f.capture = best;
            const std::string key = std::to_string(best) + ":" + dirOf(im.name);
            auto it = groups.find(key);
            if (it == groups.end()) {
                it = groups.emplace(key, (int)_group_name.size()).first;
                _group_name.push_back(dirOf(im.name));
                _group_capture.push_back(best);
            }
            f.group = it->second;
            _frames.push_back(f);
            out.frames_timed++;
        }
        std::stable_sort(_frames.begin(), _frames.end(), [](const SensorFrame& a, const SensorFrame& b) {
            return a.group != b.group ? a.group < b.group : a.t < b.t;
        });
        _group_frames.assign(_group_name.size(), {});
        for (size_t i = 0; i < _frames.size(); i++) _group_frames[(size_t)_frames[i].group].push_back((int)i);
        _X.assign(_group_name.size(), mat3Identity());
        _group_ok.assign(_group_name.size(), false);
        _gyro_sign.assign(_group_name.size(), 1.0);
    }

    void timeOffsets(SensorGaugeResult& out) {
        out.time_offsets.assign(_caps.size(), {});
        for (size_t c = 0; c < _caps.size(); c++) {
            if (!_caps[c].timeline || !_caps[c].timeline->hasRotation()) continue;
            std::vector<SensorFrame> fr;
            for (const SensorFrame& f : _frames)
                if (f.capture == (int)c) fr.push_back(f);
            const TimeOffsetFit fit = estimateTimeOffset(fr, *_caps[c].timeline);
            out.time_offsets[c] = fit;
            if (fit.found)
                for (SensorFrame& f : _frames)
                    if (f.capture == (int)c) f.t += fit.offset;
        }
    }

    void upVotes() {
        for (SensorFrame& f : _frames) {
            const SensorTimeline* tl = _caps[(size_t)f.capture].timeline;
            f.up = tl->upAt(f.t, 0.25, _gyro_sign[(size_t)f.group]);
        }
    }

    void calibrateGroups(SensorGaugeResult& out) {
        out.groups.assign(_group_name.size(), {});
        for (size_t g = 0; g < _group_name.size(); g++) {
            SensorGroupReport& rep = out.groups[g];
            rep.capture = _group_capture[g];
            rep.name = _group_name[g];
            rep.frames = (int)_group_frames[g].size();
            std::vector<SensorFrame> fr;
            for (int i : _group_frames[g]) fr.push_back(_frames[(size_t)i]);
            const SensorTimeline* tl = _caps[(size_t)rep.capture].timeline;
            if (!tl->hasRotation() || !tl->hasUp()) {
                rep.fit.reason = ExtrinsicFail::NoStream;
                continue;
            }
            rep.fit = calibrateImuExtrinsic(fr, *tl, _mean_up_w);
            if (!rep.fit.ok) continue;
            _X[g] = rep.fit.R_ci;
            _group_ok[g] = true;
            if (rep.fit.gyro_sign != _gyro_sign[g]) {
                _gyro_sign[g] = rep.fit.gyro_sign;
                for (int i : _group_frames[g])
                    _frames[(size_t)i].up = tl->upAt(_frames[(size_t)i].t, 0.25, _gyro_sign[g]);
            }
        }
    }

    // The cameras' mean up settles each group's sign on its own; a view
    // pointing straight up or down has no such axis to speak of, so a group
    // whose votes oppose the other groups' consensus is negated.
    void consensus(SensorGaugeResult& out) {
        for (int pass = 0; pass < 2; pass++) {
            std::vector<Vec3> votes;
            for (const SensorFrame& f : _frames) {
                if (!_group_ok[(size_t)f.group] || !f.up.ok) continue;
                votes.push_back(mul(transpose(f.R), mul(_X[(size_t)f.group], f.up.up)));
            }
            out.up = consensusUp(votes);
            if (!out.up.ok || pass == 1) break;
            bool flipped = false;
            for (size_t g = 0; g < _group_name.size(); g++) {
                if (!_group_ok[g]) continue;
                double agree = 0;
                int n = 0;
                for (int i : _group_frames[g]) {
                    const SensorFrame& f = _frames[(size_t)i];
                    if (!f.up.ok) continue;
                    agree += mul(transpose(f.R), mul(_X[g], f.up.up)).dot(out.up.up);
                    n++;
                }
                if (n == 0 || agree >= 0) continue;
                _X[g] = mat3Scale(_X[g], -1.0);
                out.groups[g].fit.R_ci = _X[g];
                out.groups[g].fit.mirrored = det3(_X[g]) < 0;
                flipped = true;
            }
            if (!flipped) break;
        }
        _up_ok = out.up.ok;
        _up_w = _up_ok ? out.up.up : _mean_up_w;
        out.up_from_imu = _up_ok;
    }

    // Consecutive frames of a group, pre-integrated where the IMU allows.
    void buildPairs() {
        using namespace sensor_detail;
        _pairs.assign(_group_name.size(), {});
        _triples.assign(_group_name.size(), {});
        for (size_t g = 0; g < _group_name.size(); g++) {
            if (!_group_ok[g]) continue;
            const SensorTimeline* tl = _caps[(size_t)_group_capture[g]].timeline;
            const std::vector<int>& fi = _group_frames[g];
            for (size_t k = 1; k < fi.size(); k++) {
                const SensorFrame& a = _frames[(size_t)fi[k - 1]];
                const SensorFrame& b = _frames[(size_t)fi[k]];
                const double dt = b.t - a.t;
                if (dt < 0.02 || dt > 1.5) continue;
                PairMeas pm;
                pm.j = fi[k - 1];
                pm.k = fi[k];
                if (tl->canPreintegrate()) {
                    pm.P = tl->preintegrate(a.t, b.t, {0, 0, 0}, {0, 0, 0}, _gyro_sign[g]);
                    pm.has_preint = pm.P.ok();
                }
                if (pm.has_preint) pm.B = pm.P.dR;
                else if (!tl->rotationBetween(a.t, b.t, pm.B, _gyro_sign[g])) continue;
                _pairs[g].push_back(pm);
            }
        }
    }

    struct ScaleFit {
        double s = 0, sigma = 0;   // sigma absolute
        int triples = 0, inliers = 0;
        Vec3 ba, bg;
        double g_norm = 0, g_angle_deg = 0;
    };

    // Solved for the INVERSE scale with the centres as the response: noise in
    // a regressor attenuates a slope (a 360 rig's views scatter 5 cm about
    // their frame and came out 10-50% low), in the response it only widens it.
    ScaleFit solveScale(const std::vector<size_t>& groups) const {
        using namespace sensor_detail;
        ScaleFit fit;
        std::vector<std::pair<size_t, size_t>> ids;   // (group, triple)
        for (size_t g : groups)
            for (size_t i = 0; i < _triples[g].size(); i++) ids.push_back({g, i});
        fit.triples = (int)ids.size();
        if (ids.size() < 5) return fit;

        // Gyro bias from the rotation pairs alone.
        Vec3 bg{0, 0, 0};
        {
            std::vector<double> N(9, 0.0), rhs(3, 0.0);
            for (int c = 0; c < 3; c++) N[(size_t)c * 4] += 1.0 / (0.02 * 0.02);
            for (size_t g : groups)
                for (const PairMeas& pm : _pairs[g]) {
                    if (!pm.has_preint) continue;
                    const SensorFrame& fj = _frames[(size_t)pm.j];
                    const SensorFrame& fk = _frames[(size_t)pm.k];
                    const Mat3 A = mul(fj.R, transpose(fk.R));
                    const Vec3 e = so3Log(mul(transpose(pm.P.dR), mul(transpose(_X[g]), mul(A, _X[g]))));
                    const double sr = 0.5 * M_PI / 180.0;
                    for (int r = 0; r < 3; r++) {
                        const double* row = &pm.P.dR_dbg[(size_t)r * 3];
                        for (int a = 0; a < 3; a++) {
                            rhs[(size_t)a] += row[a] * (&e.x)[r] / (sr * sr);
                            for (int c = 0; c < 3; c++) N[(size_t)a * 3 + c] += row[a] * row[c] / (sr * sr);
                        }
                    }
                }
            const std::vector<double> x = solveNormal(N, rhs, 3);
            bg = {x[0], x[1], x[2]};
        }

        std::vector<double> w(ids.size(), 1.0);
        double k = 1.0, var_k = 0;
        Vec3 bak{0, 0, 0};   // accel bias times k
        auto solve = [&](bool with_g, Vec3& g_out) {
            const int n = with_g ? 7 : 4;
            std::vector<double> N((size_t)n * n, 0.0), rhs((size_t)n, 0.0);
            auto add_row = [&](const double* row, double b, double wi) {
                for (int a = 0; a < n; a++) {
                    rhs[(size_t)a] += wi * row[a] * b;
                    for (int c = 0; c < n; c++) N[(size_t)a * n + c] += wi * row[a] * row[c];
                }
            };
            double noise_n00 = 0;
            for (size_t i = 0; i < ids.size(); i++) {
                const Triple& tr = _triples[ids[i].first][ids[i].second];
                const TripleTerms T = tripleTerms(ids[i].first, tr, _X[ids[i].first], bg, {0, 0, 0}, {0, 0, 0});
                const Vec3 pred = T.Q + _up_w * (T.Gs * -9.81);
                for (int r = 0; r < 3; r++) {
                    double row[7] = {0, 0, 0, 0, 0, 0, 0};
                    row[0] = with_g ? (&T.Q.x)[r] : (&pred.x)[r];
                    for (int c = 0; c < 3; c++) row[1 + c] = T.Ja[3 * r + c];
                    if (with_g) row[4 + r] = T.Gs;
                    add_row(row, (&T.L.x)[r], w[i]);
                }
                noise_n00 += w[i] * 3.0 * tripleNoise(ids[i].first, tr);
            }
            // The pre-integrated regressor carries the accelerometer's own
            // noise, which attenuates k -- 10% on the DJI's 30 Hz stream over
            // 1 s pairs. Corrected least squares takes that variance back out.
            N[0] = std::max(N[0] - noise_n00, 0.5 * N[0]);
            const double prior = 0.2 * std::max(k, 0.05);   // 0.2 m/s^2 on the bias itself
            for (int c = 0; c < 3; c++) {
                double row[7] = {0, 0, 0, 0, 0, 0, 0};
                row[1 + c] = 1;
                add_row(row, 0.0, 1.0 / (prior * prior));
            }
            const std::vector<double> x = solveNormal(N, rhs, n);
            k = x[0];
            bak = {x[1], x[2], x[3]};
            if (with_g) g_out = {x[4], x[5], x[6]};
            std::vector<double> Ncopy = N, ev, V;
            jacobiEigenSymmetric(Ncopy, n, ev, V);
            double var = 0;
            for (int i = 0; i < n; i++)
                if (ev[(size_t)i] > 1e-12) var += V[(size_t)i] * V[(size_t)i] / ev[(size_t)i];
            return var;
        };
        Vec3 g_unused;
        double var_r = 1;
        for (int round = 0; round < 6; round++) {
            var_k = solve(false, g_unused);
            std::vector<double> res;
            for (size_t i = 0; i < ids.size(); i++) {
                const Triple& tr = _triples[ids[i].first][ids[i].second];
                const TripleTerms T = tripleTerms(ids[i].first, tr, _X[ids[i].first], bg, {0, 0, 0}, {0, 0, 0});
                const Vec3 pred = (T.Q + _up_w * (T.Gs * -9.81)) * k + mul(T.Ja, bak);
                res.push_back((T.L - pred).norm());
            }
            const double scale_r = std::max(extrinsic_detail::medianOf(res), 1e-9);
            double ss = 0, sw = 0;
            fit.inliers = 0;
            for (size_t i = 0; i < ids.size(); i++) {
                w[i] = huberW(res[i], scale_r);
                if (res[i] < 3 * scale_r) fit.inliers++;
                ss += w[i] * res[i] * res[i];
                sw += w[i];
            }
            var_r = sw > 3 ? ss / (3 * sw - 4) : 1.0;
        }
        if (!(k > 0) && !(k < 0)) return fit;
        fit.s = 1.0 / k;
        fit.sigma = std::sqrt(var_k * var_r) / (k * k);
        fit.ba = bak * (1.0 / k);
        fit.bg = bg;
        // Gravity check: the same solve with the gravity vector free.
        Vec3 gk;
        const double k_fixed = k;
        const Vec3 bak_fixed = bak;
        solve(true, gk);
        const Vec3 gv = gk * (1.0 / k);
        fit.g_norm = gv.norm();
        fit.g_angle_deg = gv.norm() > 0 ? extrinsic_detail::angleDeg(gv, _up_w * -1.0) : 0;
        k = k_fixed;
        bak = bak_fixed;
        return fit;
    }

    // Per group: the triples and a scale of its own, which settles the sign
    // of X. Per capture: the scale that is reported, from every group at once.
    void imuScale(SensorGaugeResult& out) {
        using namespace sensor_detail;
        for (size_t g = 0; g < _group_name.size(); g++) {
            SensorGroupReport& rep = out.groups[g];
            rep.flipped = false;
            _triples[g].clear();
            if (!_group_ok[g]) continue;
            const SensorTimeline* tl = _caps[(size_t)rep.capture].timeline;
            const std::vector<PairMeas>& pairs = _pairs[g];
            if (!tl->canPreintegrate() || !_up_ok || rep.fit.degenerate) continue;
            std::vector<Triple>& tr = _triples[g];
            for (size_t p = 1; p < pairs.size(); p++) {
                if (pairs[p - 1].k != pairs[p].j) continue;
                if (!pairs[p - 1].has_preint || !pairs[p].has_preint) continue;
                Triple t;
                t.j = pairs[p - 1].j;
                t.k = pairs[p].j;
                t.l = pairs[p].k;
                t.p1 = (int)p - 1;
                t.p2 = (int)p;
                tr.push_back(t);
            }
            rep.triples = (int)tr.size();
            if (tr.size() < 5) continue;
            const ScaleFit f = solveScale({g});
            rep.scale = f.s;
            rep.scale_sigma = f.s != 0 ? std::fabs(f.sigma / f.s) : 1e9;
            rep.triple_inliers = f.inliers;
            rep.g_norm = f.g_norm;
            rep.g_angle_deg = f.g_angle_deg;
            if (f.s < 0 && rep.scale_sigma < 0.3) {
                rep.flipped = true;
                _X[g] = mat3Scale(_X[g], -1.0);
                rep.fit.R_ci = _X[g];
                rep.fit.mirrored = det3(_X[g]) < 0;
            }
        }
        double num = 0, den = 0;
        int all_triples = 0;
        bool weak = false;
        for (size_t c = 0; c < _caps.size(); c++) {
            std::vector<size_t> groups;
            for (size_t g = 0; g < _group_name.size(); g++)
                if (_group_capture[g] == (int)c && !_triples[g].empty() && !out.groups[g].flipped)
                    groups.push_back(g);
            if (groups.empty()) continue;
            const ScaleFit f = solveScale(groups);
            if (f.triples < 5) continue;
            const double rel = std::fabs(f.sigma / f.s);
            if (f.s > 0 && rel < 0.5 && f.g_angle_deg < 20) {
                num += f.s / (f.sigma * f.sigma);
                den += 1.0 / (f.sigma * f.sigma);
                all_triples += f.triples;
            } else {
                weak = true;
                all_triples += f.triples;
            }
        }
        if (den > 0) {
            out.scale_imu = num / den;
            out.scale_imu_sigma = 1.0 / (std::sqrt(den) * std::fabs(out.scale_imu));
            out.scale_from_imu = out.scale_imu > 0 && out.scale_imu_sigma < 0.1 && all_triples >= 10;
        } else if (weak) {
            out.scale_imu_sigma = 1.0;   // a capture that hardly accelerated: reported as 100%
        }
    }

    void gpsScale(SensorGaugeResult& out) {
        _gps_enu.assign(_frames.size(), Vec3{0, 0, 0});
        _gps_ok.assign(_frames.size(), 0);
        std::vector<Geodetic> all;
        for (const SensorCapture& c : _caps)
            if (c.timeline && c.timeline->hasGps())
                for (const TelemetryGps& g : c.timeline->gpsFixes()) all.push_back({g.lat, g.lon, g.has_alt ? g.alt : 0.0});
        if (all.size() < 5) return;
        Geodetic origin;
        for (const Geodetic& g : all) {
            origin.lat_deg += g.lat_deg / (double)all.size();
            origin.lon_deg += g.lon_deg / (double)all.size();
            origin.alt_m += g.alt_m / (double)all.size();
        }
        std::vector<Geodetic> geo;
        std::vector<int> owner;
        for (size_t i = 0; i < _frames.size(); i++) {
            const SensorTimeline* tl = _caps[(size_t)_frames[i].capture].timeline;
            TelemetryGps g;
            if (!tl->hasGps() || !tl->gpsAt(_frames[i].t, g)) continue;
            geo.push_back({g.lat, g.lon, g.has_alt ? g.alt : 0.0});
            owner.push_back((int)i);
        }
        const std::vector<Vec3> enu = enuFromGeodetic(geo, origin);
        MetricRef ref;
        const Mat3 R_up = rotationUpToZ(_up_w);
        for (size_t i = 0; i < enu.size(); i++) {
            _gps_enu[(size_t)owner[i]] = enu[i];
            _gps_ok[(size_t)owner[i]] = 1;
            ref.centres.push_back(mul(R_up, _frames[(size_t)owner[i]].c));
            ref.targets.push_back(enu[i]);
            ref.image_ids.push_back(_frames[(size_t)owner[i]].image_id);
        }
        out.gps_frames = (int)ref.centres.size();
        out.gps = fitMetricGauge(ref, _opt.gps_max_error,
                                 _opt.gps_full ? MetricAxes::Full : MetricAxes::Horizontal,
                                 _opt.gps_max_error_frac);
        if (!out.gps.ok) return;
        for (size_t i = 0; i < owner.size(); i++)
            if (!out.gps.inlier_mask[i]) _gps_ok[(size_t)owner[i]] = 0;
        out.scale_gps = out.gps.T.scale;
        // The residual-derived sigma under-states a correlated receiver's
        // error by ~4x (D74), and the scale sigma is used to weigh it here.
        out.scale_gps_sigma = 4.0 * out.gps.scale_unc / 100.0;
        out.scale_from_gps = true;
        out.place_from_gps = true;
    }

    sensor_detail::TripleTerms tripleTerms(size_t g, const sensor_detail::Triple& t, const Mat3& X,
                                           const Vec3& bg, const Vec3& ba, const Vec3& lever) const {
        using namespace sensor_detail;
        const std::vector<PairMeas>& pairs = _pairs[g];
        const Preintegration& P1 = pairs[(size_t)t.p1].P;
        const Preintegration& P2 = pairs[(size_t)t.p2].P;
        const SensorFrame& fj = _frames[(size_t)t.j];
        const SensorFrame& fk = _frames[(size_t)t.k];
        const SensorFrame& fl = _frames[(size_t)t.l];
        const double d1 = P1.dt, d2 = P2.dt;
        TripleTerms T;
        T.L = (fl.c - fk.c) * d1 - (fk.c - fj.c) * d2;
        T.Gs = 0.5 * d1 * d2 * (d1 + d2);
        const Mat3 Rj = mul(transpose(fj.R), X), Rk = mul(transpose(fk.R), X);
        T.Q = mul(Rj, P1.velocity(bg, ba) * (d1 * d2) - P1.position(bg, ba) * d2) +
              mul(Rk, P2.position(bg, ba) * d1);
        T.Ja = mat3Add(mul(Rj, mat3Add(mat3Scale(P1.dv_dba, d1 * d2), mat3Scale(P1.dp_dba, -d2))),
                       mul(Rk, mat3Scale(P2.dp_dba, d1)));
        T.Jg = mat3Add(mul(Rj, mat3Add(mat3Scale(P1.dv_dbg, d1 * d2), mat3Scale(P1.dp_dbg, -d2))),
                       mul(Rk, mat3Scale(P2.dp_dbg, d1)));
        // The IMU sits `lever` from the lens (camera frame, metres).
        const Vec3 arm = (mul(transpose(fl.R), lever) - mul(transpose(fk.R), lever)) * d1 -
                         (mul(transpose(fk.R), lever) - mul(transpose(fj.R), lever)) * d2;
        T.Q = T.Q - arm;
        return T;
    }

    // Variance of one component of a triple's pre-integrated position, for
    // white accelerometer noise of density q: (q/3) d1^2 d2^2 (d1 + d2), the
    // velocity-free combination of two random walks.
    double tripleNoise(size_t g, const sensor_detail::Triple& t) const {
        const double q = _caps[(size_t)_group_capture[g]].timeline->noise.accel;
        const double d1 = _pairs[g][(size_t)t.p1].P.dt, d2 = _pairs[g][(size_t)t.p2].P.dt;
        return q * q * d1 * d1 * d2 * d2 * (d1 + d2) / 3.0;
    }

    // ---- the joint solve --------------------------------------------------

    struct Params {
        double log_s = 0;
        Vec3 rot;        // left-multiplied increment on the base rotation
        Vec3 t;
        std::vector<Vec3> bg, ba;        // per capture
        std::vector<Vec3> lever, dX;     // per group
    };

    struct Layout {
        int n = 0;
        int i_s = -1, i_rot = -1, i_t = -1;
        std::vector<int> i_bg, i_ba, i_lever, i_dX;
        std::vector<bool> active;
    };

    void assemble(SensorGaugeResult& out) {
        using namespace sensor_detail;
        const bool imu = out.scale_from_imu, gps = out.scale_from_gps;
        // Disagreement: keep the more certain source and say so.
        bool use_imu = imu, use_gps = gps;
        if (imu && gps) {
            const double z = std::fabs(std::log(out.scale_imu / out.scale_gps)) /
                             std::sqrt(out.scale_imu_sigma * out.scale_imu_sigma +
                                       out.scale_gps_sigma * out.scale_gps_sigma);
            if (z > 3.0) {
                out.disagree = true;
                if (out.scale_imu_sigma < out.scale_gps_sigma) use_gps = false;
                else use_imu = false;
            }
        }
        const bool any_scale = use_imu || use_gps;
        if (!_up_ok && !any_scale) {
            out.fail = SensorFail::NoUp;
            return;
        }

        // Base: up to +Z, then the GPS yaw and place, then the scale.
        const Mat3 R_up = rotationUpToZ(_up_w);
        Mat3 R0 = R_up;
        Vec3 t0{0, 0, 0};
        double s0 = 1.0;
        if (use_gps) {
            R0 = mul(out.gps.T.R, R_up);
            t0 = out.gps.T.t;
            s0 = out.gps.T.scale;
        }
        if (use_imu && use_gps) {
            const double wi = 1.0 / (out.scale_imu_sigma * out.scale_imu_sigma);
            const double wg = 1.0 / (out.scale_gps_sigma * out.scale_gps_sigma);
            s0 = std::exp((wi * std::log(out.scale_imu) + wg * std::log(out.scale_gps)) / (wi + wg));
            t0 = t0 * (s0 / out.gps.T.scale);
        } else if (use_imu) {
            s0 = out.scale_imu;
        }

        Params p;
        p.bg.assign(_caps.size(), Vec3{0, 0, 0});
        p.ba.assign(_caps.size(), Vec3{0, 0, 0});
        p.lever.assign(_group_name.size(), Vec3{0, 0, 0});
        p.dX.assign(_group_name.size(), Vec3{0, 0, 0});
        _base_R = R0;
        _base_s = s0;
        _base_t = t0;
        _use_imu = use_imu;
        _use_gps = use_gps;
        if (_opt.refine) refine(p, out);

        const double s = _base_s * std::exp(p.log_s);
        const Mat3 R = mul(so3Exp(p.rot), _base_R);
        Vec3 t = _base_t + p.t;
        if (!use_gps) {
            Vec3 mid{0, 0, 0};
            int n = 0;
            for (const auto& kv : _rec.images)
                if (kv.second.registered) { mid = mid + cameraCenter(kv.second.pose); n++; }
            if (n) mid = mid * (1.0 / n);
            t = mul(R, mid) * -s;
        }
        if (!any_scale) {
            // Upright from the IMU, sized as Orient.h sizes a model.
            out.T = normalizingTransform(_rec, mul(transpose(R), Vec3{0, 0, 1}));
            out.applied = true;
            out.metric = false;
            out.scale = out.T.scale;
            out.no_scale = _opt.mode != SensorMode::Auto ? SensorNoScale::NotAsked
                           : (out.scale_imu_sigma > 0 && !imu) ? SensorNoScale::ImuWeak
                           : (out.gps_frames > 0 && !out.gps.ok) ? SensorNoScale::GpsRefused
                                                                  : SensorNoScale::NoSource;
            return;
        }
        out.T = {s, R, t};
        out.scale = s;
        out.applied = true;
        out.metric = true;
        out.scale_from_imu = use_imu;
        out.scale_from_gps = use_gps;
        out.place_from_gps = use_gps;
    }

    Mat3 _base_R = mat3Identity();
    Vec3 _base_t;
    double _base_s = 1;
    bool _use_imu = false, _use_gps = false;
    double _sig_up = 1, _sig_rot = 1, _sig_trip = 1, _sig_gps = 1;

    // Residuals, each already divided by its sigma; `w` carries the robust
    // weight assigned at the previous evaluation (IRLS).
    void residuals(const Params& p, std::vector<double>& r,
                   std::vector<double>* noise = nullptr) const {
        using namespace sensor_detail;
        r.clear();
        if (noise) noise->clear();
        const double s = _base_s * std::exp(p.log_s);
        const Mat3 R = mul(so3Exp(p.rot), _base_R);
        const Vec3 t = _base_t + p.t;
        std::vector<Mat3> X(_group_name.size());
        for (size_t g = 0; g < X.size(); g++) X[g] = mul(_X[g], so3Exp(p.dX[g]));
        const Vec3 ez{0, 0, 1};
        if (_up_ok)
            for (const SensorFrame& f : _frames) {
                if (!_group_ok[(size_t)f.group] || !f.up.ok) continue;
                const Vec3 v = mul(R, mul(transpose(f.R), mul(X[(size_t)f.group], f.up.up)));
                const Vec3 d = v - ez;
                r.push_back(d.x / _sig_up);
                r.push_back(d.y / _sig_up);
                r.push_back(d.z / _sig_up);
            }
        for (size_t g = 0; g < _group_name.size(); g++) {
            if (!_group_ok[g]) continue;
            const int cap = _group_capture[g];
            for (const PairMeas& pm : _pairs[g]) {
                const SensorFrame& fj = _frames[(size_t)pm.j];
                const SensorFrame& fk = _frames[(size_t)pm.k];
                const Mat3 B = pm.has_preint ? pm.P.rotation(p.bg[(size_t)cap]) : pm.B;
                const Mat3 A = mul(fj.R, transpose(fk.R));
                const Vec3 e = so3Log(mul(transpose(B), mul(transpose(X[g]), mul(A, X[g]))));
                r.push_back(e.x / _sig_rot);
                r.push_back(e.y / _sig_rot);
                r.push_back(e.z / _sig_rot);
            }
            if (!_use_imu) continue;
            for (const Triple& tr : _triples[g]) {
                const TripleTerms T = tripleTerms(g, tr, X[g], p.bg[(size_t)cap], p.ba[(size_t)cap], p.lever[g]);
                // Centres as the response (see solveScale), in the target frame.
                const Vec3 e = mul(R, T.L) - (mul(R, T.Q) + mul(R, _up_w) * (T.Gs * -9.81)) * (1.0 / s);
                r.push_back(e.x / _sig_trip);
                r.push_back(e.y / _sig_trip);
                r.push_back(e.z / _sig_trip);
                if (noise) {
                    noise->resize(r.size() - 3, 0.0);
                    noise->resize(r.size(), tripleNoise(g, tr));
                }
            }
        }
        if (_use_gps)
            for (size_t i = 0; i < _frames.size(); i++) {
                if (!_gps_ok[i]) continue;
                const Vec3 e = mul(R, _frames[i].c) * s + t - _gps_enu[i];
                r.push_back(e.x / _sig_gps);
                r.push_back(e.y / _sig_gps);
                if (_opt.gps_full) r.push_back(e.z / (3.0 * _sig_gps));
            }
        for (size_t c = 0; c < _caps.size(); c++) {
            for (int k = 0; k < 3; k++) r.push_back((&p.bg[c].x)[k] / 0.02);   // rad/s
            for (int k = 0; k < 3; k++) r.push_back((&p.ba[c].x)[k] / 0.2);    // m/s^2
        }
        for (size_t g = 0; g < _group_name.size(); g++) {
            for (int k = 0; k < 3; k++) r.push_back((&p.lever[g].x)[k] / 0.1);   // metres
            for (int k = 0; k < 3; k++) r.push_back((&p.dX[g].x)[k] / 0.05);     // rad
        }
        if (noise) noise->resize(r.size(), 0.0);
    }

    Layout layout() const {
        Layout L;
        auto add = [&](int& idx, int n, bool on) {
            idx = L.n;
            for (int k = 0; k < n; k++) L.active.push_back(on);
            L.n += n;
        };
        add(L.i_s, 1, _use_imu || _use_gps);
        add(L.i_rot, 3, true);
        add(L.i_t, 3, _use_gps);
        L.i_bg.resize(_caps.size());
        L.i_ba.resize(_caps.size());
        for (size_t c = 0; c < _caps.size(); c++) {
            bool has = false;
            for (size_t g = 0; g < _group_name.size(); g++)
                if (_group_capture[g] == (int)c && _group_ok[g] && !_pairs[g].empty() && _pairs[g][0].has_preint)
                    has = true;
            const bool gyro = _caps[c].timeline && _caps[c].timeline->hasGyro();
            add(L.i_bg[c], 3, has && gyro);
            add(L.i_ba[c], 3, has && _use_imu);
        }
        L.i_lever.resize(_group_name.size());
        L.i_dX.resize(_group_name.size());
        for (size_t g = 0; g < _group_name.size(); g++) {
            add(L.i_lever[g], 3, _use_imu && _group_ok[g] && !_triples[g].empty());
            add(L.i_dX[g], 3, _group_ok[g]);
        }
        // Yaw is observed by GPS alone; tilt by the up votes alone.
        L.active[(size_t)L.i_rot + 2] = _use_gps;
        L.active[(size_t)L.i_rot] = L.active[(size_t)L.i_rot + 1] = _up_ok;
        return L;
    }

    static void unpack(const Layout& L, const std::vector<double>& x, Params& p) {
        p.log_s = x[(size_t)L.i_s];
        p.rot = {x[(size_t)L.i_rot], x[(size_t)L.i_rot + 1], x[(size_t)L.i_rot + 2]};
        p.t = {x[(size_t)L.i_t], x[(size_t)L.i_t + 1], x[(size_t)L.i_t + 2]};
        for (size_t c = 0; c < p.bg.size(); c++) {
            p.bg[c] = {x[(size_t)L.i_bg[c]], x[(size_t)L.i_bg[c] + 1], x[(size_t)L.i_bg[c] + 2]};
            p.ba[c] = {x[(size_t)L.i_ba[c]], x[(size_t)L.i_ba[c] + 1], x[(size_t)L.i_ba[c] + 2]};
        }
        for (size_t g = 0; g < p.lever.size(); g++) {
            p.lever[g] = {x[(size_t)L.i_lever[g]], x[(size_t)L.i_lever[g] + 1], x[(size_t)L.i_lever[g] + 2]};
            p.dX[g] = {x[(size_t)L.i_dX[g]], x[(size_t)L.i_dX[g] + 1], x[(size_t)L.i_dX[g] + 2]};
        }
    }

    void calibrateSigmas(const Params& p) {
        using namespace sensor_detail;
        _sig_up = _sig_rot = _sig_trip = _sig_gps = 1.0;
        std::vector<double> r;
        residuals(p, r);
        // Same order as residuals(): up, then per group rotation + triples,
        // then GPS. Recover the block sizes to scale each family by its MAD.
        size_t pos = 0;
        auto take = [&](size_t n) {
            std::vector<double> v;
            for (size_t i = 0; i < n && pos < r.size(); i++) v.push_back(r[pos++]);
            return v;
        };
        size_t n_up = 0, n_rot = 0, n_trip = 0, n_gps = 0;
        if (_up_ok)
            for (const SensorFrame& f : _frames)
                if (_group_ok[(size_t)f.group] && f.up.ok) n_up += 3;
        for (size_t g = 0; g < _group_name.size(); g++) {
            if (!_group_ok[g]) continue;
            n_rot += 3 * _pairs[g].size();
            if (_use_imu) n_trip += 3 * _triples[g].size();
        }
        if (_use_gps)
            for (size_t i = 0; i < _frames.size(); i++)
                if (_gps_ok[i]) n_gps += _opt.gps_full ? 3 : 2;
        // The rotation and triple blocks interleave per group; gather them.
        std::vector<double> up = take(n_up), rot, trip;
        for (size_t g = 0; g < _group_name.size(); g++) {
            if (!_group_ok[g]) continue;
            std::vector<double> a = take(3 * _pairs[g].size());
            rot.insert(rot.end(), a.begin(), a.end());
            if (_use_imu) {
                std::vector<double> b = take(3 * _triples[g].size());
                trip.insert(trip.end(), b.begin(), b.end());
            }
        }
        std::vector<double> gps = take(n_gps);
        auto sig = [](const std::vector<double>& v, double floor_) {
            std::vector<double> a;
            for (double x : v) a.push_back(std::fabs(x));
            return std::max(1.4826 * extrinsic_detail::medianOf(a), floor_);
        };
        if (!up.empty()) _sig_up = sig(up, 0.3 * M_PI / 180.0);
        if (!rot.empty()) _sig_rot = sig(rot, 0.1 * M_PI / 180.0);
        if (!trip.empty()) _sig_trip = sig(trip, 1e-9);
        if (!gps.empty()) _sig_gps = 3.0 * sig(gps, 1.0);
    }

    void refine(Params& p, SensorGaugeResult& out) {
        calibrateSigmas(p);
        const Layout L = layout();
        std::vector<double> x((size_t)L.n, 0.0);
        std::vector<int> act;
        for (int i = 0; i < L.n; i++)
            if (L.active[(size_t)i]) act.push_back(i);
        if (act.empty()) return;
        const int m = (int)act.size();

        std::vector<double> r, r2, w, noise;
        auto eval = [&](const std::vector<double>& xv, std::vector<double>& rv) {
            Params q = p;
            unpack(L, xv, q);
            residuals(q, rv);
        };
        residuals(p, r, &noise);
        int i_scale = -1;
        for (int i = 0; i < (int)act.size(); i++)
            if (act[(size_t)i] == L.i_s) i_scale = i;
        // solveScale's correction, in the curvature: the scale column of a
        // triple row is the noisy pre-integrated regressor, and left alone the
        // solve would take the attenuation back.
        auto noiseCurvature = [&](double s) {
            if (i_scale < 0) return 0.0;
            double c = 0;
            for (size_t i = 0; i < noise.size() && i < w.size(); i++)
                c += w[i] * noise[i] / (s * s * _sig_trip * _sig_trip);
            return c;
        };
        auto cost = [&](const std::vector<double>& rv) {
            double c = 0;
            for (size_t i = 0; i < rv.size(); i++) c += w[i] * rv[i] * rv[i];
            return c;
        };
        eval(x, r);
        w.assign(r.size(), 1.0);
        auto reweight = [&](const std::vector<double>& rv) {
            for (size_t i = 0; i < rv.size(); i++) w[i] = sensor_detail::huberW(rv[i], 1.0);
        };
        reweight(r);
        double c = cost(r);
        out.lm_cost0 = c;
        double lambda = 1e-3;
        std::vector<double> J;   // rows x m
        bool done = false;
        for (int iter = 0; iter < 20 && !done; iter++) {
            const size_t n = r.size();
            J.assign(n * (size_t)m, 0.0);
            for (int a = 0; a < m; a++) {
                const double h = 1e-5;
                std::vector<double> xp = x, xm = x;
                xp[(size_t)act[(size_t)a]] += h;
                xm[(size_t)act[(size_t)a]] -= h;
                std::vector<double> rp, rm;
                eval(xp, rp);
                eval(xm, rm);
                for (size_t i = 0; i < n; i++) J[i * (size_t)m + (size_t)a] = (rp[i] - rm[i]) / (2 * h);
            }
            std::vector<double> H((size_t)m * (size_t)m, 0.0), g((size_t)m, 0.0);
            for (size_t i = 0; i < n; i++) {
                const double* Ji = &J[i * (size_t)m];
                for (int a = 0; a < m; a++) {
                    g[(size_t)a] += w[i] * Ji[a] * r[i];
                    for (int b = 0; b < m; b++) H[(size_t)a * (size_t)m + (size_t)b] += w[i] * Ji[a] * Ji[b];
                }
            }
            const double c_n = noiseCurvature(_base_s * std::exp(p.log_s + x[(size_t)L.i_s]));
            if (i_scale >= 0) {
                const size_t d = (size_t)i_scale * (size_t)m + (size_t)i_scale;
                g[(size_t)i_scale] += c_n;
                H[d] = std::max(H[d] - c_n, 0.5 * H[d]);
            }
            bool accepted = false;
            for (int tries = 0; tries < 8 && !accepted; tries++) {
                std::vector<double> Hd = H;
                for (int a = 0; a < m; a++) Hd[(size_t)a * (size_t)m + (size_t)a] *= (1 + lambda);
                std::vector<double> ev, V;
                jacobiEigenSymmetric(Hd, m, ev, V);
                std::vector<double> dx((size_t)m, 0.0);
                for (int i = 0; i < m; i++) {
                    if (!(ev[(size_t)i] > 1e-12)) continue;
                    double d = 0;
                    for (int a = 0; a < m; a++) d += V[(size_t)a * (size_t)m + (size_t)i] * g[(size_t)a];
                    for (int a = 0; a < m; a++) dx[(size_t)a] -= V[(size_t)a * (size_t)m + (size_t)i] * d / ev[(size_t)i];
                }
                std::vector<double> xn = x;
                for (int a = 0; a < m; a++) xn[(size_t)act[(size_t)a]] += dx[(size_t)a];
                eval(xn, r2);
                const double c2 = cost(r2);
                if (c2 < c) {
                    x = xn;
                    r = r2;
                    accepted = true;
                    lambda = std::max(lambda * 0.3, 1e-6);
                    const double drop = (c - c2) / std::max(c, 1e-12);
                    c = c2;
                    reweight(r);
                    c = cost(r);
                    out.lm_iterations = iter + 1;
                    done = drop < 1e-5;
                } else {
                    lambda *= 10;
                }
            }
            if (!accepted) break;
        }
        out.lm_cost = c;
        unpack(L, x, p);

        // Uncertainties from the final curvature.
        {
            const size_t n = r.size();
            std::vector<double> H((size_t)m * (size_t)m, 0.0);
            for (size_t i = 0; i < n; i++) {
                const double* Ji = &J[i * (size_t)m];
                for (int a = 0; a < m; a++)
                    for (int b = 0; b < m; b++) H[(size_t)a * (size_t)m + (size_t)b] += w[i] * Ji[a] * Ji[b];
            }
            if (i_scale >= 0) {
                const size_t d = (size_t)i_scale * (size_t)m + (size_t)i_scale;
                H[d] = std::max(H[d] - noiseCurvature(_base_s * std::exp(p.log_s)), 0.5 * H[d]);
            }
            std::vector<double> ev, V;
            jacobiEigenSymmetric(H, m, ev, V);
            auto var_of = [&](int param) {
                int a = -1;
                for (int k = 0; k < m; k++)
                    if (act[(size_t)k] == param) a = k;
                if (a < 0) return 0.0;
                double v = 0;
                for (int i = 0; i < m; i++) {
                    const double e = V[(size_t)a * (size_t)m + (size_t)i];
                    v += ev[(size_t)i] > 1e-12 ? e * e / ev[(size_t)i] : 0.0;
                }
                return v;
            };
            out.scale_sigma = std::sqrt(var_of(L.i_s));
            out.tilt_sigma_deg = std::sqrt(var_of(L.i_rot) + var_of(L.i_rot + 1)) * 180.0 / M_PI;
        }
    }
};

inline SensorGaugeResult fitSensorGauge(const Reconstruction& rec,
                                        const std::vector<SensorCapture>& caps,
                                        const SensorGaugeOptions& opt) {
    SensorGaugeSolver solver(rec, caps, opt);
    return solver.run();
}

}  // namespace sfm
