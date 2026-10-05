// The video's IMU and GPS as a PriorSource (sfm/core/PriorSource.h): the
// gyro's rotation between two frames, gravity in each frame, the
// accelerometer's metric scale and the GPS position, each as a factor a
// bundle adjustment takes while the model is built. The IMU-to-lens rotation
// is calibrated from verified pairs before mapping and refined with gravity
// once a model exists; every gauge quantity (up, scale, biases, the metric
// frame) is refitted from the poses it is handed. A geotagged image folder
// states its EXIF GPS through the same position factors (ExifGpsPriors).
// docs/notes/sensor-priors.md.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sfm/core/Log.h"
#include "sfm/core/PriorSource.h"
#include "sfm/core/SensorTimeline.h"
#include "sfm/map/ImuExtrinsic.h"
#include "sfm/map/ImuScale.h"
#include "sfm/map/MetricGauge.h"
#include "sfm/map/Orient.h"
#include "sfm/map/SensorGauge.h"

namespace sfm {

struct SensorPriorOptions {
    bool rotation = true;    // gyro relative rotations
    bool up = true;          // gravity direction
    bool scale = true;       // accelerometer triples
    bool gps = true;         // GPS positions
    double max_dt = 3.0;     // seconds a relative-rotation prior may span
    // Sigma of a relative rotation grows with the gap: the calibration's own
    // residual plus a gyro bias's drift (a MEMS bias of 0.03 deg/s).
    double drift_deg_per_s = 0.03;
    double min_rot_sigma_deg = 0.2;
    double min_up_sigma_deg = 1.0;
    int calib_min_pairs = 30;    // two-view pairs a group needs before mapping
    int calib_min_frames = 20;   // posed frames a group needs for the gravity refit
    double gps_max_error = 5.0;  // metres, the fit's inlier radius
    double gps_max_error_frac = 0.03;
    // Centre factors, and the frame registrations are checked through; off under
    // --metric-gps none. Positions still propose pairs (gps).
    bool gps_centres = true;
    // A source with no IMU up fits its GPS level-only about the cameras' mean up
    // (--metric-gps horizontal, D75); a telemetry source's IMU up comes first.
    bool gps_flat = false;
    bool trusted_position = false;
    bool verbose = false;
};

// A verified pair's relative rotation, R_j = R_ji R_i over world -> camera.
struct PairRotationObs {
    uint32_t i = 0, j = 0;
    Mat3 R_ji;
};

struct SensorGroupState {
    int capture = -1;
    uint32_t camera = 0;
    std::string name;
    bool ok = false;          // an extrinsic to use
    bool from_pairs = false;  // ... calibrated from two-view rotations alone
    bool gravity = false;     // ... refined with gravity from a model
    ExtrinsicFit fit;
    Mat3 X = mat3Identity();  // camera <- IMU
    double sign = 1.0;        // gyro integration sign
    int frames_at_calib = 0;
    int pairs = 0;            // two-view pairs offered
};

// What the last factors() call produced, for the log.
struct SensorFactorStats {
    int frames = 0;
    int rotations = 0, ups = 0, triples = 0, gps = 0;
    bool up_ok = false;
    double up_spread_deg = 0;
    bool scale_ok = false;
    double scale = 0, scale_sigma = 0, g_angle_deg = 0;
    bool gps_ok = false;
    double gps_rms = 0;
    int gps_out = 0;   // factors stated beyond the fit's inlier radius
    MetricFail gps_reason = MetricFail::Pairs;   // None on a fit
    double gps_perp_frac = 0;
    int level_votes = 0;   // posed images declaring their up
    bool level_ok = false;
    double level_spread_deg = 0;
};

inline const char* metricFailName(MetricFail r) {
    switch (r) {
        case MetricFail::None: return "none";
        case MetricFail::Pairs: return "pairs";
        case MetricFail::Spread: return "spread";
        case MetricFail::Inliers: return "inliers";
        case MetricFail::Collinear: return "collinear";
        case MetricFail::Tilted: return "tilted";
    }
    return "?";
}

// One diag line per GPS fit, refused ones included: a straight seed stretch
// otherwise leaves every BA silent about why it held no GPS factor.
inline void logGpsFit(const char* label, int posed, const MetricFit& fit, int factors, int beyond,
                      const GpsFrame& frame, bool level) {
    char lv[160] = "";
    if (level)
        std::snprintf(lv, sizeof lv, ", level: 3D scale %.4f, altitude sigma %.2f m, vertical %s",
                      fit.scale_3d, fit.vertical_sigma, frame.flat ? "dropped" : "kept");
    if (fit.ok)
        slog::diag(slog::Tag::Map,
                   "[prior] %s over %d posed image(s): fit, %d inlier(s) within %.2f m, RMS %.3f "
                   "m, scale %.4f -> %d centre factor(s), %d beyond the radius, sigma %.2f m%s",
                   label, posed, fit.inliers, fit.max_error, fit.rms, fit.T.scale, factors,
                   beyond, frame.sigma_h, lv);
    else if (fit.reason == MetricFail::Tilted)
        slog::diag(slog::Tag::Map,
                   "[prior] %s over %d posed image(s): no fit (tilted: level scale %.4f against "
                   "3D %.4f, spread %.1f m, %d inlier(s) within %.2f m), 0 centre factor(s)",
                   label, posed, fit.T.scale, fit.scale_3d, fit.spread, fit.inliers,
                   fit.max_error);
    else
        slog::diag(slog::Tag::Map,
                   "[prior] %s over %d posed image(s): no fit (%s, perp %.3f against %.2f, spread "
                   "%.1f m, %d inlier(s) within %.2f m), 0 centre factor(s)",
                   label, posed, metricFailName(fit.reason), fit.perp_frac,
                   kMetricMinPerpFraction, fit.spread, fit.inliers, fit.max_error);
}

// The level sigma of a GPS centre factor: the fit's inlier RMS inflated for the
// receiver's correlated error (D74: residuals under-state it ~4x).
inline double gpsLevelSigma(const MetricFit& fit) {
    return std::max(2.0 * fit.rms, 0.6 * fit.max_error);
}

// A position the user trusts (--metric-gps full) gets COLMAP 4.1.1's factor:
// prior_position_fallback_stddev 1 m, isotropic, under CauchyLoss(prior_position_loss_scale).
constexpr double kTrustedGpsSigma = 1.0;
constexpr double kTrustedGpsCauchy = 7.815;

// A similarity fitted on the inliers, then a centre factor for EVERY positioned image: an
// inlier mask drops exactly the frames that drifted off between two solves. Level with an up
// (D75): vertical only trusted, and only where altitude agrees within the radius; else 3x.
inline MetricFit gpsCentreFactors(MetricRef ref, const Vec3* up_w, double max_error,
                                  double max_error_frac, PosePriors& out, bool trusted = false) {
    MetricFit fit;
    if (ref.centres.size() < 5) return fit;
    const Mat3 R_up = up_w ? rotationUpToZ(*up_w) : mat3Identity();
    for (Vec3& c : ref.centres) c = mul(R_up, c);
    fit = fitMetricGauge(ref, max_error, up_w ? MetricAxes::Horizontal : MetricAxes::Full,
                         max_error_frac);
    if (!fit.ok) return fit;
    const double sh = trusted ? kTrustedGpsSigma : gpsLevelSigma(fit);
    const bool vertical = trusted ? !up_w || fit.vertical_sigma <= fit.max_error : !up_w;
    const double sv = !vertical ? 0.0 : trusted ? kTrustedGpsSigma : 3.0 * sh;
    const Mat3 A = mat3Scale(mul(fit.T.R, R_up), fit.T.scale);
    for (size_t i = 0; i < ref.centres.size(); i++) {
        PriorCentre f;
        f.n = 1;
        f.img[0] = ref.image_ids[i];
        f.A[0] = A;
        f.b = ref.targets[i] - fit.T.t;
        f.sigma = {sh, sh, sv};
        f.cauchy = trusted ? kTrustedGpsCauchy : 0.0;
        out.centres.push_back(f);
    }
    out.gps.ok = true;
    out.gps.flat = sv == 0.0;
    out.gps.A = A;
    out.gps.t = fit.T.t;
    out.gps.sigma_h = sh;
    out.gps.gate = fit.max_error;
    return fit;
}

// A horizon-levelled equirect (DJI Studio's stitch, the Osmo's own) states camera -Y
// as up. Tilt spread about the consensus measured 0.15-0.37 deg on three such
// captures, 2.7 and 10.8 deg on a handheld 360's two lens streams.
inline const Vec3 kLevelErpUp{0, -1, 0};
constexpr double kLevelSigmaDeg = 0.3;      // clean 150-frame blocks of a 1800-frame capture: 0.07-0.24 deg
constexpr double kLevelMaxSpreadDeg = 1.0;  // past this median the set is refused as not level
constexpr int kLevelMinVotes = 30;          // a 10-14 image window of a tilting capture reads under the gate
// A registration may sit max(kLevelTolDeg, kLevelTolSpreadMul * spread) off the consensus.
// Isotropic scatter of median m has tail 2^(-k^2) past k*m: 0.2% at k = 3, against 50% past
// a fixed 1 deg when m sits at the gate.
constexpr double kLevelTolDeg = 1.0;
constexpr double kLevelTolSpreadMul = 3.0;
inline double levelTolDeg(double spread_deg) {
    return std::max(kLevelTolDeg, kLevelTolSpreadMul * spread_deg);
}

struct LevelFit {
    bool ok = false;
    int votes = 0;
    double spread_deg = 0;
    Vec3 up{0, 0, 1};
};

// One up factor per posed image whose `level` entry is set, against the images'
// robust consensus (consensusUp); none, and !ok, when they disagree past
// kLevelMaxSpreadDeg -- a capture that is not level fails closed.
inline LevelFit levelUpFactors(const std::vector<PosedImage>& imgs,
                               const std::vector<char>& level, PosePriors& out) {
    LevelFit f;
    std::vector<Vec3> votes;
    std::vector<uint32_t> ids;
    for (const PosedImage& p : imgs)
        if (p.image < level.size() && level[p.image]) {
            votes.push_back(mul(transpose(p.pose.R), kLevelErpUp));
            ids.push_back(p.image);
        }
    f.votes = (int)votes.size();
    if (f.votes < kLevelMinVotes) return f;
    const UpConsensus c = consensusUp(votes);
    f.spread_deg = c.spread_deg;
    f.up = c.up;
    if (!c.ok || c.spread_deg > kLevelMaxSpreadDeg) return f;
    f.ok = true;
    out.up_w = c.up;
    out.level = {true, c.up, levelTolDeg(c.spread_deg)};
    for (uint32_t i : ids) {
        PriorUp u;
        u.i = i;
        u.u = kLevelErpUp;
        u.sigma = kLevelSigmaDeg * M_PI / 180.0;
        out.ups.push_back(u);
    }
    return f;
}

class TelemetryPriors : public PriorSource {
public:
    TelemetryPriors(std::vector<SensorCapture> caps, const std::vector<std::string>& names,
                    const std::vector<uint32_t>& camera_ids, SensorPriorOptions opt)
        : caps_(std::move(caps)), opt_(opt) {
        using namespace sensor_detail;
        const size_t n = names.size();
        cap_.assign(n, -1);
        grp_.assign(n, -1);
        t_.assign(n, 0.0);
        clock_.assign(caps_.size(), 0.0);
        offset_.assign(caps_.size(), {});
        std::map<std::pair<int, uint32_t>, int> groups;
        for (uint32_t i = 0; i < n; i++) {
            int best = -1;
            for (size_t c = 0; c < caps_.size(); c++) {
                if (!caps_[c].timeline || !(caps_[c].fps > 0)) continue;
                if (!underPrefix(names[i], caps_[c].prefix)) continue;
                if (best < 0 || caps_[c].prefix.size() > caps_[(size_t)best].prefix.size())
                    best = (int)c;
            }
            const int idx = stemIndex(names[i]);
            if (best < 0 || idx < 0) continue;
            const SensorCapture& cap = caps_[(size_t)best];
            cap_[i] = best;
            t_[i] = idx / cap.fps + 0.5 * cap.readout + cap.time_offset;
            const uint32_t cam = i < camera_ids.size() ? camera_ids[i] : 1;
            auto it = groups.find({best, cam});
            if (it == groups.end()) {
                it = groups.emplace(std::make_pair(best, cam), (int)groups_.size()).first;
                SensorGroupState g;
                g.capture = best;
                g.camera = cam;
                g.name = dirOf(names[i]);
                if (g.name.empty()) g.name = ".";
                g.name += " (camera " + std::to_string(cam) + ")";
                groups_.push_back(g);
            }
            grp_[i] = it->second;
        }
        // Time order per capture, for neighbours().
        order_.assign(caps_.size(), {});
        for (uint32_t i = 0; i < n; i++)
            if (cap_[i] >= 0) order_[(size_t)cap_[i]].push_back(i);
        for (std::vector<uint32_t>& o : order_)
            std::sort(o.begin(), o.end(), [&](uint32_t a, uint32_t b) {
                return t_[a] != t_[b] ? t_[a] < t_[b] : a < b;
            });
        rank_.assign(n, 0);
        for (const std::vector<uint32_t>& o : order_)
            for (uint32_t k = 0; k < o.size(); k++) rank_[o[k]] = k;
    }

    const SensorPriorOptions& options() const { return opt_; }
    const std::vector<SensorGroupState>& groups() const { return groups_; }
    const std::vector<TimeOffsetFit>& timeOffsets() const { return offset_; }
    const SensorFactorStats& lastFactors() const { return stats_; }
    size_t timedImages() const {
        size_t k = 0;
        for (int c : cap_) k += c >= 0 ? 1 : 0;
        return k;
    }
    bool anyRotation() const {
        for (const SensorGroupState& g : groups_)
            if (g.ok && caps_[(size_t)g.capture].timeline->hasRotation()) return true;
        return false;
    }
    // Whether a two-view pair is worth offering to calibrateFromPairs.
    bool calibrationPair(uint32_t i, uint32_t j) const {
        if (!has(i) || !has(j) || cap_[i] != cap_[j] || grp_[i] != grp_[j]) return false;
        const double dt = std::fabs(t_[j] - t_[i]);
        return dt >= 0.02 && dt <= opt_.max_dt &&
               caps_[(size_t)cap_[i]].timeline->hasRotation();
    }

    // The IMU-to-lens rotation per group from verified pairs alone: the
    // clock offset per capture first, then the hand-eye fit. Gravity is not
    // available yet, so the sign of X stays open until a model settles it.
    void calibrateFromPairs(const std::vector<PairRotationObs>& obs) {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<std::vector<RotationPairObs>> per_group(groups_.size());
        for (const PairRotationObs& o : obs) {
            if (!calibrationPair(o.i, o.j)) continue;
            uint32_t a = o.i, b = o.j;
            // A = R(t0) R(t1)^T with t0 < t1: R_i R_j^T = R_ji^T when i is first.
            Mat3 A = transpose(o.R_ji);
            if (t_[a] > t_[b]) {
                std::swap(a, b);
                A = o.R_ji;
            }
            per_group[(size_t)grp_[a]].push_back({t_[a], t_[b], A});
        }
        for (size_t c = 0; c < caps_.size(); c++) {
            std::vector<RotationPairObs> all;
            for (size_t g = 0; g < groups_.size(); g++)
                if (groups_[g].capture == (int)c)
                    all.insert(all.end(), per_group[g].begin(), per_group[g].end());
            if (all.empty() || !caps_[c].timeline->hasRotation()) continue;
            offset_[c] = estimateTimeOffsetFrom(all, *caps_[c].timeline);
            clock_[c] = offset_[c].found ? offset_[c].offset : 0.0;
        }
        for (size_t g = 0; g < groups_.size(); g++) {
            SensorGroupState& st = groups_[g];
            st.pairs = (int)per_group[g].size();
            const SensorTimeline& tl = *caps_[(size_t)st.capture].timeline;
            if (!tl.hasRotation() || st.pairs < opt_.calib_min_pairs) continue;
            for (RotationPairObs& rp : per_group[g]) {
                rp.t0 += clock_[(size_t)st.capture];
                rp.t1 += clock_[(size_t)st.capture];
            }
            st.fit = calibrateImuExtrinsicFrom(per_group[g], {}, tl, Vec3{0, 0, 1});
            if (!st.fit.ok) continue;
            st.ok = true;
            st.from_pairs = true;
            st.gravity = false;
            st.X = st.fit.R_ci;
            st.sign = st.fit.gyro_sign;
        }
        preint_.clear();
    }

    // ---- PriorSource ----

    bool has(uint32_t img) const override { return img < cap_.size() && cap_[img] >= 0; }

    bool relativeRotation(uint32_t i, uint32_t j, Mat3& R, double& sigma) const override {
        if (!opt_.rotation || !has(i) || !has(j) || cap_[i] != cap_[j]) return false;
        std::lock_guard<std::mutex> lk(mu_);
        return relativeRotationLocked(i, j, R, sigma);
    }

    bool relativeRotationLocked(uint32_t i, uint32_t j, Mat3& R, double& sigma) const {
        if (!opt_.rotation || !has(i) || !has(j) || cap_[i] != cap_[j]) return false;
        const SensorGroupState& gi = groups_[(size_t)grp_[i]];
        const SensorGroupState& gj = groups_[(size_t)grp_[j]];
        if (!gi.ok || !gj.ok || gi.sign != gj.sign) return false;
        // Before gravity settled it the sign of X is open, but X B^T X^T does
        // not care; what it cannot survive is a degenerate (one-axis) fit.
        if (gi.fit.degenerate || gj.fit.degenerate) return false;
        const double ti = time(i), tj = time(j);
        const double dt = std::fabs(tj - ti);
        if (dt > opt_.max_dt) return false;
        const SensorTimeline& tl = *caps_[(size_t)cap_[i]].timeline;
        Mat3 B;
        if (!tl.rotationBetween(ti, tj, B, gi.sign)) return false;
        R = mul(mul(gj.X, transpose(B)), transpose(gi.X));
        const double deg = std::max(std::max(gi.fit.sig_rot_deg, gj.fit.sig_rot_deg),
                                    opt_.min_rot_sigma_deg) +
                           opt_.drift_deg_per_s * dt;
        sigma = deg * M_PI / 180.0;
        return true;
    }

    std::vector<uint32_t> neighbours(uint32_t img) const override {
        std::vector<uint32_t> out;
        if (!has(img)) return out;
        const std::vector<uint32_t>& o = order_[(size_t)cap_[img]];
        const uint32_t r = rank_[img];
        for (uint32_t d = 1; d <= o.size() && out.size() < 8; d++) {
            bool any = false;
            if (r >= d && std::fabs(t_[o[r - d]] - t_[img]) <= opt_.max_dt) {
                out.push_back(o[r - d]);
                any = true;
            }
            if (r + d < o.size() && std::fabs(t_[o[r + d]] - t_[img]) <= opt_.max_dt) {
                out.push_back(o[r + d]);
                any = true;
            }
            if (!any) break;
        }
        return out;
    }

    bool position(uint32_t img, Vec3& p) const override {
        if (!opt_.gps || !has(img)) return false;
        std::lock_guard<std::mutex> lk(mu_);
        return positionLocked(img, p);
    }

    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        // Atom mappers on several threads share one source (map/Atoms.h).
        std::lock_guard<std::mutex> lk(mu_);
        return factorsLocked(imgs);
    }

private:
    bool positionLocked(uint32_t img, Vec3& p) const {
        const SensorTimeline& tl = *caps_[(size_t)cap_[img]].timeline;
        if (!tl.hasGps()) return false;
        TelemetryGps g;
        if (!tl.gpsAt(time(img), g)) return false;
        ensureEnuOrigin();
        p = enuFromGeodetic({{g.lat, g.lon, g.has_alt ? g.alt : 0.0}}, enu_origin_)[0];
        return true;
    }

    PosePriors factorsLocked(const std::vector<PosedImage>& imgs) {
        PosePriors out;
        stats_ = SensorFactorStats{};
        std::vector<SensorFrame> frames;
        std::vector<uint32_t> frame_img;
        for (const PosedImage& p : imgs) {
            if (!has(p.image)) continue;
            SensorFrame f;
            f.image_id = p.image;
            f.t = time(p.image);
            f.R = p.pose.R;
            f.c = cameraCenter(p.pose);
            f.group = grp_[p.image];
            f.capture = cap_[p.image];
            frames.push_back(f);
            frame_img.push_back(p.image);
        }
        stats_.frames = (int)frames.size();
        if (frames.size() < 3) return out;
        // The cameras' mean up, which settles each group's sign.
        Vec3 mean_up{0, 0, 0};
        for (const SensorFrame& f : frames) mean_up = mean_up + mul(transpose(f.R), Vec3{0, -1, 0});
        if (mean_up.norm() > 0) mean_up = mean_up.normalized();

        std::vector<std::vector<size_t>> by_group(groups_.size());
        for (size_t k = 0; k < frames.size(); k++) by_group[(size_t)frames[k].group].push_back(k);
        for (std::vector<size_t>& fi : by_group)
            std::sort(fi.begin(), fi.end(), [&](size_t a, size_t b) { return frames[a].t < frames[b].t; });
        refineClocks(frames, by_group);
        for (size_t g = 0; g < groups_.size(); g++) refineGroup(g, frames, by_group[g], mean_up);

        // Up votes, the consensus, and the groups' signs against it.
        for (SensorFrame& f : frames) {
            const SensorGroupState& st = groups_[(size_t)f.group];
            if (!st.ok) continue;
            f.up = caps_[(size_t)f.capture].timeline->upAt(f.t, 0.25, st.sign);
        }
        UpConsensus up = settleSigns(frames, by_group, mean_up);
        stats_.up_ok = up.ok;
        stats_.up_spread_deg = up.spread_deg;
        if (up.ok) out.up_w = up.up;
        if (opt_.up && up.ok)
            for (const SensorFrame& f : frames) {
                const SensorGroupState& st = groups_[(size_t)f.group];
                if (!st.ok || !f.up.ok) continue;
                PriorUp u;
                u.i = f.image_id;
                u.u = mul(st.X, f.up.up).normalized();
                u.sigma = std::max(st.fit.sig_grav_deg, opt_.min_up_sigma_deg) * M_PI / 180.0;
                out.ups.push_back(u);
            }
        // Relative rotations along each lens's own chain.
        if (opt_.rotation)
            for (size_t g = 0; g < groups_.size(); g++) {
                const std::vector<size_t>& fi = by_group[g];
                for (size_t k = 1; k < fi.size(); k++) {
                    PriorRotation r;
                    r.i = frames[fi[k - 1]].image_id;
                    r.j = frames[fi[k]].image_id;
                    if (!relativeRotationLocked(r.i, r.j, r.R_ji, r.sigma)) continue;
                    out.rotations.push_back(r);
                }
            }
        stats_.rotations = (int)out.rotations.size();
        stats_.ups = (int)out.ups.size();
        if (opt_.scale && up.ok) scaleFactors(frames, by_group, up.up, out);
        const Vec3* gps_up = up.ok ? &up.up : nullptr;
        if (!gps_up && opt_.gps_flat && mean_up.norm() > 0) gps_up = &mean_up;
        if (opt_.gps && opt_.gps_centres) gpsFactors(frames, gps_up, out);
        return out;
    }

    double time(uint32_t img) const { return t_[img] + clock_[(size_t)cap_[img]]; }

    // The IMU clock offset again, from the model's own rotations once a
    // capture has enough posed frames and each time that count has doubled:
    // the two-view rotations the pair stage searched it on are the noisier.
    void refineClocks(std::vector<SensorFrame>& frames,
                      const std::vector<std::vector<size_t>>& by_group) {
        if (clock_frames_.size() != caps_.size()) clock_frames_.assign(caps_.size(), 0);
        for (size_t c = 0; c < caps_.size(); c++) {
            if (!caps_[c].timeline->hasRotation()) continue;
            std::vector<RotationPairObs> rot;
            size_t n = 0;
            for (size_t g = 0; g < groups_.size(); g++) {
                if (groups_[g].capture != (int)c) continue;
                const std::vector<size_t>& fi = by_group[g];
                n += fi.size();
                for (size_t k = 1; k < fi.size(); k++)
                    rot.push_back({frames[fi[k - 1]].t, frames[fi[k]].t,
                                   mul(frames[fi[k - 1]].R, transpose(frames[fi[k]].R))});
            }
            if ((int)n < opt_.calib_min_frames || n < 2 * clock_frames_[c]) continue;
            clock_frames_[c] = n;
            const TimeOffsetFit fit = estimateTimeOffsetFrom(rot, *caps_[c].timeline);
            if (!fit.found || std::fabs(fit.offset) < 1e-4) continue;
            clock_[c] += fit.offset;
            offset_[c] = fit;
            offset_[c].offset = clock_[c];
            for (SensorFrame& f : frames)
                if (f.capture == (int)c) f.t += fit.offset;
            preint_.clear();
        }
    }

    // The gravity-inclusive calibration of one group over the model's frames,
    // once it has enough of them and each time that count has doubled.
    void refineGroup(size_t g, const std::vector<SensorFrame>& frames,
                     const std::vector<size_t>& idx, const Vec3& mean_up) {
        SensorGroupState& st = groups_[g];
        const SensorTimeline& tl = *caps_[(size_t)st.capture].timeline;
        if (!tl.hasRotation() || !tl.hasUp()) return;
        if ((int)idx.size() < opt_.calib_min_frames) return;
        if (st.gravity && (int)idx.size() < 2 * st.frames_at_calib) return;
        std::vector<SensorFrame> fr;
        for (size_t k : idx) fr.push_back(frames[k]);
        std::sort(fr.begin(), fr.end(), [](const SensorFrame& a, const SensorFrame& b) { return a.t < b.t; });
        for (SensorFrame& f : fr) f.up = tl.upAt(f.t, 0.25, st.ok ? st.sign : 1.0);
        ExtrinsicFit fit = calibrateImuExtrinsic(fr, tl, mean_up);
        if (!fit.ok) {
            st.frames_at_calib = (int)idx.size();
            return;
        }
        st.fit = fit;
        st.X = fit.R_ci;
        st.sign = fit.gyro_sign;
        st.ok = true;
        st.gravity = true;
        st.from_pairs = false;
        st.frames_at_calib = (int)idx.size();
        preint_.clear();
    }

    // Each group's votes must agree with the cameras' mean up, then with the
    // consensus of every group -- a lens looking straight down has no mean up
    // to speak of, and X and -X satisfy the hand-eye constraint alike.
    UpConsensus settleSigns(const std::vector<SensorFrame>& frames,
                            const std::vector<std::vector<size_t>>& by_group, const Vec3& mean_up) {
        auto vote = [&](const SensorFrame& f) {
            return mul(transpose(f.R), mul(groups_[(size_t)f.group].X, f.up.up));
        };
        for (size_t g = 0; g < groups_.size(); g++) {
            SensorGroupState& st = groups_[g];
            if (!st.ok || st.gravity) continue;
            double agree = 0;
            for (size_t k : by_group[g])
                if (frames[k].up.ok) agree += vote(frames[k]).dot(mean_up);
            if (agree < 0) st.X = mat3Scale(st.X, -1.0);
        }
        UpConsensus up;
        for (int pass = 0; pass < 2; pass++) {
            std::vector<Vec3> votes;
            for (const SensorFrame& f : frames)
                if (groups_[(size_t)f.group].ok && f.up.ok) votes.push_back(vote(f));
            up = consensusUp(votes);
            if (!up.ok || pass == 1) break;
            bool flipped = false;
            for (size_t g = 0; g < groups_.size(); g++) {
                SensorGroupState& st = groups_[g];
                if (!st.ok) continue;
                double agree = 0;
                int n = 0;
                for (size_t k : by_group[g])
                    if (frames[k].up.ok) { agree += vote(frames[k]).dot(up.up); n++; }
                if (n == 0 || agree >= 0) continue;
                st.X = mat3Scale(st.X, -1.0);
                flipped = true;
            }
            if (!flipped) break;
        }
        return up;
    }

    const Preintegration& preintegrated(uint32_t i, uint32_t j, const SensorGroupState& st) {
        const uint64_t key = ((uint64_t)i << 32) | j;
        auto it = preint_.find(key);
        if (it != preint_.end()) return it->second;
        const SensorTimeline& tl = *caps_[(size_t)st.capture].timeline;
        return preint_.emplace(key, tl.preintegrate(time(i), time(j), {0, 0, 0}, {0, 0, 0}, st.sign))
            .first->second;
    }

    // The velocity-free triples of every lens, one scale per capture, and a
    // centre factor per triple at the fitted scale, biases and gravity.
    void scaleFactors(const std::vector<SensorFrame>& frames,
                      const std::vector<std::vector<size_t>>& by_group, const Vec3& up_w,
                      PosePriors& out) {
        std::vector<std::vector<ImuPair>> pairs(groups_.size());
        std::vector<std::vector<ImuTriple>> triples(groups_.size());
        for (size_t g = 0; g < groups_.size(); g++) {
            const SensorGroupState& st = groups_[g];
            const SensorTimeline& tl = *caps_[(size_t)st.capture].timeline;
            if (!st.ok || !st.gravity || st.fit.degenerate || !tl.canPreintegrate()) continue;
            const std::vector<size_t>& fi = by_group[g];
            for (size_t k = 1; k < fi.size(); k++) {
                const SensorFrame& a = frames[fi[k - 1]];
                const SensorFrame& b = frames[fi[k]];
                const double dt = b.t - a.t;
                if (dt < 0.02 || dt > 1.5) continue;
                ImuPair pm;
                pm.j = (int)fi[k - 1];
                pm.k = (int)fi[k];
                pm.P = preintegrated(a.image_id, b.image_id, st);
                pm.has_preint = pm.P.ok();
                if (!pm.has_preint) continue;
                pm.B = pm.P.dR;
                pairs[g].push_back(pm);
            }
            triples[g] = imuTriples(pairs[g]);
        }
        for (size_t c = 0; c < caps_.size(); c++) {
            std::vector<ImuGroupView> views;
            std::vector<size_t> gs;
            for (size_t g = 0; g < groups_.size(); g++) {
                if (groups_[g].capture != (int)c || triples[g].empty()) continue;
                views.push_back({&frames, &pairs[g], &triples[g], groups_[g].X,
                                 caps_[c].timeline->noise.accel});
                gs.push_back(g);
            }
            if (views.empty()) continue;
            const ImuScaleFit fit = solveImuScale(views, up_w);
            stats_.triples += fit.triples;
            if (fit.triples < 10 || !(fit.s > 0)) continue;
            const double rel = std::fabs(fit.sigma / fit.s);
            stats_.scale = fit.s;
            stats_.scale_sigma = rel;
            stats_.g_angle_deg = fit.g_angle_deg;
            if (rel > 0.2 || fit.g_angle_deg > 20.0) continue;
            stats_.scale_ok = true;
            const double sigma = std::max(1.4826 * fit.res_scale, 1e-9);
            for (size_t v = 0; v < views.size(); v++) {
                const size_t g = gs[v];
                for (const ImuTriple& t : triples[g]) {
                    const ImuTripleTerms T = imuTripleTerms(frames, pairs[g], t, groups_[g].X,
                                                            fit.bg, fit.ba, {0, 0, 0});
                    const double d1 = pairs[g][(size_t)t.p1].P.dt, d2 = pairs[g][(size_t)t.p2].P.dt;
                    PriorCentre f;
                    f.n = 3;
                    f.img[0] = frames[(size_t)t.j].image_id;
                    f.img[1] = frames[(size_t)t.k].image_id;
                    f.img[2] = frames[(size_t)t.l].image_id;
                    for (int k = 0; k < 9; k++) {
                        f.A[0][k] = mat3Identity()[k] * d2;
                        f.A[1][k] = mat3Identity()[k] * -(d1 + d2);
                        f.A[2][k] = mat3Identity()[k] * d1;
                    }
                    f.b = (T.Q + up_w * (T.Gs * -9.81)) * (1.0 / fit.s);
                    // A triple the fit rejected still gets its factor; the
                    // Huber weight in the solve is what discounts it.
                    f.sigma = {sigma, sigma, sigma};
                    out.centres.push_back(f);
                }
            }
        }
    }

    void ensureEnuOrigin() const {
        if (enu_origin_ok_) return;
        std::vector<Geodetic> all;
        for (const SensorCapture& c : caps_)
            if (c.timeline && c.timeline->hasGps())
                for (const TelemetryGps& g : c.timeline->gpsFixes())
                    all.push_back({g.lat, g.lon, g.has_alt ? g.alt : 0.0});
        enu_origin_ = Geodetic{};
        for (const Geodetic& g : all) {
            enu_origin_.lat_deg += g.lat_deg / (double)all.size();
            enu_origin_.lon_deg += g.lon_deg / (double)all.size();
            enu_origin_.alt_m += g.alt_m / (double)all.size();
        }
        enu_origin_ok_ = true;
    }

    void gpsFactors(const std::vector<SensorFrame>& frames, const Vec3* up_w, PosePriors& out) {
        MetricRef ref;
        for (const SensorFrame& f : frames) {
            Vec3 p;
            if (!positionLocked(f.image_id, p)) continue;
            ref.centres.push_back(f.c);
            ref.targets.push_back(p);
            ref.image_ids.push_back(f.image_id);
        }
        const int posed = (int)ref.centres.size();
        const size_t before = out.centres.size();
        const MetricFit fit = gpsCentreFactors(std::move(ref), up_w, opt_.gps_max_error,
                                               opt_.gps_max_error_frac, out, opt_.trusted_position);
        stats_.gps_reason = fit.reason;
        stats_.gps_perp_frac = fit.perp_frac;
        if (fit.ok) {
            stats_.gps_ok = true;
            stats_.gps_rms = fit.rms;
            stats_.gps_out = fit.n - fit.inliers;
            stats_.gps = (int)(out.centres.size() - before);
        }
        if (opt_.verbose && posed >= 5)
            logGpsFit("GPS", posed, fit, stats_.gps, stats_.gps_out, out.gps, up_w != nullptr);
    }

    std::vector<SensorCapture> caps_;
    SensorPriorOptions opt_;
    std::vector<int32_t> cap_, grp_;    // per image; -1 = not timed
    std::vector<double> t_;             // per image, video time
    std::vector<double> clock_;         // per capture: the fitted IMU clock offset
    std::vector<size_t> clock_frames_;  // ... and the posed frames it was last fitted on
    std::vector<TimeOffsetFit> offset_;
    std::vector<std::vector<uint32_t>> order_;   // per capture, images by time
    std::vector<uint32_t> rank_;                 // per image, its place in order_
    std::vector<SensorGroupState> groups_;
    std::unordered_map<uint64_t, Preintegration> preint_;
    SensorFactorStats stats_;
    mutable Geodetic enu_origin_;
    mutable bool enu_origin_ok_ = false;
    mutable std::mutex mu_;
};

// A geotagged image set as a PriorSource: EXIF fixes in east-north-up about their
// mean. No IMU: the GPS fit is level about the `level` images' up when they agree
// on one (levelUpFactors), else full unless gps_flat takes the cameras' up (D75).
class ExifGpsPriors : public PriorSource {
public:
    ExifGpsPriors(const std::vector<std::optional<Geodetic>>& fixes, SensorPriorOptions opt,
                  std::vector<char> level = {})
        : opt_(opt), level_(std::move(level)) {
        std::vector<Geodetic> g;
        for (const std::optional<Geodetic>& f : fixes)
            if (f) g.push_back(*f);
        const std::vector<Vec3> enu = enuFromGeodetic(g);
        pos_.resize(fixes.size());
        for (size_t i = 0, k = 0; i < fixes.size(); i++)
            if (fixes[i]) pos_[i] = enu[k++];
    }

    const SensorPriorOptions& options() const { return opt_; }
    size_t positioned() const {
        size_t n = 0;
        for (const std::optional<Vec3>& p : pos_) n += p ? 1 : 0;
        return n;
    }
    SensorFactorStats lastFactors() const {
        std::lock_guard<std::mutex> lk(mu_);
        return stats_;
    }

    bool has(uint32_t img) const override { return img < pos_.size() && pos_[img].has_value(); }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override { return false; }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    bool position(uint32_t img, Vec3& p) const override {
        if (!opt_.gps || !has(img)) return false;
        p = *pos_[img];
        return true;
    }
    bool declaredUp(uint32_t img, Vec3& u) const override {
        if (level_off_ || img >= level_.size() || !level_[img]) return false;
        u = kLevelErpUp;
        return true;
    }
    void disableLevel() override { level_off_ = true; }

    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        PosePriors out;
        SensorFactorStats st;
        const LevelFit lv = level_off_ ? LevelFit{} : levelUpFactors(imgs, level_, out);
        st.level_votes = lv.votes;
        st.level_ok = lv.ok;
        st.level_spread_deg = lv.spread_deg;
        st.ups = (int)out.ups.size();
        if (opt_.gps_centres) gpsFactors(imgs, lv, out, st);
        if (opt_.verbose && lv.votes >= kLevelMinVotes)
            slog::diag(slog::Tag::Map,
                       "[prior] level up over %d posed image(s): spread %.3f deg against %.1f -> "
                       "%d up factor(s)",
                       lv.votes, lv.spread_deg, kLevelMaxSpreadDeg, st.ups);
        std::lock_guard<std::mutex> lk(mu_);
        stats_ = st;
        return out;
    }

private:
    void gpsFactors(const std::vector<PosedImage>& imgs, const LevelFit& lv, PosePriors& out,
                    SensorFactorStats& st) const {
        MetricRef ref;
        for (const PosedImage& p : imgs) {
            Vec3 x;
            if (!position(p.image, x)) continue;
            ref.centres.push_back(cameraCenter(p.pose));
            ref.targets.push_back(x);
            ref.image_ids.push_back(p.image);
        }
        st.frames = (int)ref.centres.size();
        // The rule of meanCameraUp (map/Orient.h) over the posed images.
        Vec3 up{0, 0, 0};
        for (const PosedImage& p : imgs)
            if (has(p.image)) up = up + mul(transpose(p.pose.R), Vec3{0, -1, 0});
        const bool flat = opt_.gps_flat && up.norm() > 0;
        if (flat) up = up.normalized();
        const Vec3* gps_up = lv.ok ? &lv.up : flat ? &up : nullptr;
        const MetricFit fit = gpsCentreFactors(std::move(ref), gps_up, opt_.gps_max_error,
                                               opt_.gps_max_error_frac, out,
                                               opt_.trusted_position);
        st.gps_ok = fit.ok;
        st.gps_rms = fit.rms;
        st.gps_out = fit.ok ? fit.n - fit.inliers : 0;
        st.gps = (int)out.centres.size();
        st.gps_reason = fit.reason;
        st.gps_perp_frac = fit.perp_frac;
        if (opt_.verbose && st.frames >= 5)
            logGpsFit("EXIF GPS", st.frames, fit, st.gps, st.gps_out, out.gps, gps_up != nullptr);
    }

    SensorPriorOptions opt_;
    std::vector<char> level_;
    std::atomic<bool> level_off_{false};
    std::vector<std::optional<Vec3>> pos_;
    SensorFactorStats stats_;
    mutable std::mutex mu_;
};

}  // namespace sfm
