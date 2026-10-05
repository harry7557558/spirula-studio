// A finished model and its video's telemetry, replayed through the sensor
// priors without mapping again: the hand-eye calibration on the model's own
// rotations, the factors a bundle adjustment would take, the registration
// check against every pose, and the sensor gauge.
//
//   SS_TEST_AVATA_MODEL=<sparse/0> SS_TEST_AVATA_OSV=<clip> sfm_attitude_replay_test
//
// Prints SKIP when either is unset. Needs no GPU.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "core/Env.h"
#include "sfm/core/Model.h"
#include "sfm/core/SensorTimeline.h"
#include "sfm/core/Telemetry.h"
#include "sfm/map/SensorGauge.h"
#include "sfm/map/SensorPriors.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;
static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        fails++;
    }
}
static double angleDeg(const Mat3& A, const Mat3& B) {
    const Mat3 D = mul(transpose(A), B);
    const double tr = std::max(-1.0, std::min(1.0, (D[0] + D[4] + D[8] - 1.0) * 0.5));
    return std::acos(tr) * 180.0 / M_PI;
}
static double angleDeg(const Vec3& a, const Vec3& b) {
    const double d = std::max(-1.0, std::min(1.0, a.dot(b) / (a.norm() * b.norm())));
    return std::acos(d) * 180.0 / M_PI;
}
static std::string quantiles(std::vector<double> v) {
    if (v.empty()) return "none";
    std::sort(v.begin(), v.end());
    auto q = [&](double f) { return v[(size_t)std::min((double)v.size() - 1, f * (double)(v.size() - 1))]; };
    char s[128];
    std::snprintf(s, sizeof s, "n=%zu p50 %.3f p95 %.3f max %.3f", v.size(), q(0.5), q(0.95), v.back());
    return s;
}

static int cmdReplay(int, char**) {
    const char* model_dir = spirula::env("TEST_AVATA_MODEL");
    const char* clip = spirula::env("TEST_AVATA_OSV");
    if (!model_dir || !clip) {
        std::printf("SKIP replay (SS_TEST_AVATA_MODEL or SS_TEST_AVATA_OSV unset)\n");
        return 0;
    }
    Telemetry t;
    std::string err;
    check(telemetry_read(clip, t, err), "read telemetry: " + err);
    const TelemetryCheck c = telemetry_check(t);
    SensorTimeline tl;
    check(tl.init(t, c, err), "timeline init: " + err);
    std::printf("telemetry: %s, attitude %.0f Hz, accel %zu, fps %.3f, readout %.1f ms | rotation %d, up %d\n",
                t.camera.c_str(), c.orientation.rate_hz, t.accel.size(), t.video_fps,
                1e3 * t.frame_readout, tl.hasRotation(), tl.hasUp());

    const Reconstruction rec = Reconstruction::readBinary(model_dir);
    // The database order the priors index by: the model's images, by name.
    std::vector<uint32_t> ids;
    for (const auto& kv : rec.images)
        if (kv.second.registered) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end(),
              [&](uint32_t a, uint32_t b) { return rec.images.at(a).name < rec.images.at(b).name; });
    std::vector<std::string> names;
    std::vector<uint32_t> cams;
    for (uint32_t id : ids) {
        names.push_back(rec.images.at(id).name);
        cams.push_back(rec.images.at(id).camera_id);
    }
    std::printf("model: %zu registered images, first %s, last %s\n", ids.size(),
                names.empty() ? "-" : names.front().c_str(), names.empty() ? "-" : names.back().c_str());
    SensorCapture cap;
    cap.fps = t.video_fps;
    cap.readout = t.frame_readout;
    cap.timeline = &tl;
    TelemetryPriors src({cap}, names, cams, SensorPriorOptions());
    check(src.timedImages() == names.size(), "every image timed");

    // Calibration pairs as the mapper offers them: the same lens, 0.02-3 s
    // apart, thinned evenly to 600 over the capture.
    std::map<uint32_t, std::vector<uint32_t>> lens;
    for (uint32_t k = 0; k < ids.size(); k++) lens[cams[k]].push_back(k);
    std::vector<PairRotationObs> all;
    for (auto& kv : lens) {
        std::vector<uint32_t>& v = kv.second;
        std::sort(v.begin(), v.end(), [&](uint32_t a, uint32_t b) {
            return sensor_detail::stemIndex(names[a]) < sensor_detail::stemIndex(names[b]);
        });
        for (uint32_t d : {1u, 2u, 3u, 5u, 8u, 13u, 21u})
            for (size_t k = 0; k + d < v.size(); k++) {
                if (!src.calibrationPair(v[k], v[k + d])) continue;
                const Pose& a = rec.images.at(ids[v[k]]).pose;
                const Pose& b = rec.images.at(ids[v[k + d]]).pose;
                all.push_back({v[k], v[k + d], mul(b.R, transpose(a.R))});
            }
    }
    std::vector<PairRotationObs> obs;
    const size_t kMax = 600;
    const double step = all.size() > kMax ? (double)all.size() / (double)kMax : 1.0;
    for (size_t k = 0; k < std::min(all.size(), kMax); k++) obs.push_back(all[(size_t)(k * step)]);
    src.calibrateFromPairs(obs);
    const TimeOffsetFit off = src.timeOffsets().empty() ? TimeOffsetFit{} : src.timeOffsets()[0];
    std::printf("pairs: %zu offered of %zu | IMU clock %.1f ms off the video (found %d, %d pairs)\n",
                obs.size(), all.size(), 1e3 * off.offset, off.found, off.pairs);
    int calibrated = 0;
    for (const SensorGroupState& g : src.groups()) {
        std::printf("  from pairs %s: ok=%d reason=%d pairs=%d sig_rot %.3f deg gap %.1f sign %+.0f\n",
                    g.name.c_str(), g.ok, (int)g.fit.reason, g.pairs, g.fit.sig_rot_deg, g.fit.gap, g.sign);
        calibrated += g.ok && g.fit.sig_rot_deg < 1.0;
    }
    check(calibrated == (int)src.groups().size() && calibrated >= 1, "every lens calibrates, sig_rot < 1 deg");

    // The factors a bundle adjustment over the whole model would take.
    std::vector<PosedImage> imgs;
    for (uint32_t k = 0; k < ids.size(); k++) imgs.push_back({k, cams[k], rec.images.at(ids[k]).pose});
    const PosePriors pf = src.factors(imgs);
    const SensorFactorStats& st = src.lastFactors();
    int vertical = 0;
    for (const PriorCentre& f : pf.centres)
        if (f.n == 1 && f.sigma.z > 0) vertical++;
    std::printf("factors: %zu rotations, %zu ups, %d GPS positions (%d keep the vertical) | up ok=%d "
                "spread %.2f deg | gps ok=%d rms %.3f | scale ok=%d triples %d\n",
                pf.rotations.size(), pf.ups.size(), st.gps, vertical, st.up_ok, st.up_spread_deg,
                st.gps_ok, st.gps_rms, st.scale_ok, st.triples);
    for (const SensorGroupState& g : src.groups()) {
        std::printf("  refit %s: ok=%d gravity=%d sig_rot %.3f sig_grav %.3f deg rot=%d grav=%d gap %.1f "
                    "sign %+.0f det %+.0f\n", g.name.c_str(), g.ok, g.gravity, g.fit.sig_rot_deg,
                    g.fit.sig_grav_deg, g.fit.rot_pairs, g.fit.grav_pairs, g.fit.gap, g.sign,
                    det3(g.X) > 0 ? 1.0 : -1.0);
        // The hand-eye fit settles the sign of X by agreement with the up votes,
        // so a declared vertical with the wrong sign does not move the up
        // consensus: it mirrors X instead. det +1 is what catches it.
        check(det3(g.X) > 0, "refit " + g.name + ": extrinsic is a proper rotation (declared vertical not flipped)");
    }
    const double up_vs_z = st.up_ok ? angleDeg(pf.up_w, Vec3{0, 0, 1}) : 180.0;
    std::printf("up: consensus %.3f deg from the model's +Z\n", up_vs_z);

    // Each factor's residual at the model's own poses.
    auto R = [&](uint32_t k) { return rec.images.at(ids[k]).pose.R; };
    std::vector<double> rot_deg, rot_sig, up_deg;
    for (const PriorRotation& r : pf.rotations) {
        const double e = angleDeg(mul(r.R_ji, R(r.i)), R(r.j));
        rot_deg.push_back(e);
        rot_sig.push_back(e * M_PI / 180.0 / r.sigma);
    }
    for (const PriorUp& u : pf.ups) up_deg.push_back(angleDeg(mul(R(u.i), pf.up_w), u.u));
    std::printf("rotation factor residual deg: %s\n", quantiles(rot_deg).c_str());
    std::printf("rotation factor residual sigma: %s\n", quantiles(rot_sig).c_str());
    std::printf("up factor residual deg: %s\n", quantiles(up_deg).c_str());

    // The registration check (Mapper.h priorCheckPose) on every pose, with
    // every other image registered. A rig-mate at the same instant predicts
    // the rig rotation the model was solved with, so it proves nothing.
    std::vector<std::string> held_names;
    auto regCheck = [&](bool same_lens, int& with_prior, int& held, std::vector<double>& res) {
        with_prior = held = 0;
        for (uint32_t k = 0; k < ids.size(); k++) {
            Mat3 Rp;
            double best = 1e300;
            for (uint32_t j : src.neighbours(k)) {
                Mat3 Rji;
                double sig;
                if (same_lens && cams[j] != cams[k]) continue;
                if (!src.relativeRotation(j, k, Rji, sig) || sig >= best) continue;
                best = sig;
                Rp = mul(Rji, R(j));
            }
            if (best >= 1e300) continue;
            with_prior++;
            res.push_back(angleDeg(R(k), Rp));
            if (res.back() <= std::max(2.0, 3.0 * best * 180.0 / M_PI)) continue;
            held++;
            if (same_lens) held_names.push_back(names[k] + " " + std::to_string(res.back()).substr(0, 4));
        }
    };
    int with_prior = 0, held = 0, any_prior = 0, any_held = 0;
    std::vector<double> reg_deg, any_deg;
    regCheck(true, with_prior, held, reg_deg);
    regCheck(false, any_prior, any_held, any_deg);
    std::printf("registration check, same lens: %d of %zu images have a prior; %d would be re-solved or "
                "refused; residual deg %s\n", with_prior, ids.size(), held, quantiles(reg_deg).c_str());
    for (const std::string& n : held_names) std::printf("  held: %s deg\n", n.c_str());
    std::printf("registration check, any neighbour (rig-mates, tautological): %d held; residual deg %s\n",
                any_held, quantiles(any_deg).c_str());
    check(with_prior >= 0.95 * (double)ids.size(), "registration check: a prior for 95% of images");
    check(with_prior > 0 && held <= 0.01 * with_prior, "registration check accepts 99% of the model's poses");
    check(st.up_ok && up_vs_z < 0.5, "up within 0.5 deg of the model's GPS-levelled +Z");
    check(!pf.rotations.empty(), "rotation factors present");

    // The sensor gauge a finished run would fit.
    SensorGaugeOptions go;
    go.gps_full = true;
    const SensorGaugeResult gr = fitSensorGauge(rec, {cap}, go);
    for (const SensorGroupReport& g : gr.groups)
        std::printf("gauge %s: ok=%d reason=%d sig_rot %.3f sig_grav %.3f frames %d\n", g.name.c_str(),
                    g.fit.ok, (int)g.fit.reason, g.fit.sig_rot_deg, g.fit.sig_grav_deg, g.fit.frames);
    std::printf("gauge: applied=%d metric=%d up_from_imu=%d votes %d spread %.3f outliers %d | IMU up "
                "%.3f deg from the model's +Z | scale %.6f gps rms %.3f\n",
                gr.applied, gr.metric, gr.up_from_imu, gr.up.votes, gr.up.spread_deg, gr.up.outliers,
                gr.up.ok ? angleDeg(gr.up.up, Vec3{0, 0, 1}) : 180.0, gr.scale, gr.gps.rms);
    check(gr.up_from_imu && angleDeg(gr.up.up, Vec3{0, 0, 1}) < 0.5, "gauge: IMU up within 0.5 deg of +Z");
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdReplay); }
