// The sensors as priors (docs/notes/sensor-priors.md): the fixed-rotation
// two-view and PnP estimators on scenes with equipment and outliers, then
// the telemetry source on the synthetic walk -- calibrated from pair
// rotations alone, its relative rotations, and the factors it states about
// a posed model in a random gauge (up, rotations, inertial scale, GPS).
//
//   sfm_sensor_prior_test
//
// Prints FAIL lines and returns the count. Needs no GPU.
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/PriorSource.h"
#include "sfm/core/SensorTimeline.h"
#include "sfm/geometry/KnownRotation.h"
#include "sfm/geometry/TwoView.h"
#include "sfm/map/SensorPriors.h"
#include "sfm/tests/SyntheticTelemetry.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;
using namespace synth_telemetry;

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

// ---- two views with the rotation known ---------------------------------------

static void testKnownRotationTwoView() {
    std::mt19937 rng(3);
    std::normal_distribution<double> N(0, 1);
    std::uniform_real_distribution<double> U(-1, 1);
    const Mat3 R = angleAxisToRotation({0.15, -0.3, 0.08});
    const Vec3 t = Vec3{0.4, -0.1, 0.25}.normalized();
    const int n_scene = 300, n_equip = 200, n_junk = 60;
    std::vector<Vec3> b1, b2;
    const double noise = 0.0005;
    for (int k = 0; k < n_scene; k++) {
        const Vec3 X{2.0 * U(rng), 2.0 * U(rng), 4.0 + 2.0 * U(rng)};
        Vec3 a = X.normalized(), b = (mul(R, X) + t).normalized();
        a = (a + Vec3{noise * N(rng), noise * N(rng), noise * N(rng)}).normalized();
        b = (b + Vec3{noise * N(rng), noise * N(rng), noise * N(rng)}).normalized();
        b1.push_back(a);
        b2.push_back(b);
    }
    // Equipment fixed to the camera: the same bearing in both frames.
    for (int k = 0; k < n_equip; k++) {
        const Vec3 a = Vec3{0.8 * U(rng), 0.6 + 0.3 * U(rng), 1.0}.normalized();
        b1.push_back(a);
        b2.push_back((a + Vec3{noise * N(rng), noise * N(rng), noise * N(rng)}).normalized());
    }
    for (int k = 0; k < n_junk; k++) {
        b1.push_back(Vec3{U(rng), U(rng), 1.0 + 0.5 * U(rng)}.normalized());
        b2.push_back(Vec3{U(rng), U(rng), 1.0 + 0.5 * U(rng)}.normalized());
    }
    KnownRotationOptions ko;
    ko.ransac.max_error = 0.003;
    ko.ransac.seed = 5;
    const KnownRotationGeometry g = estimateTwoViewKnownRotation(b1, b2, R, ko);
    int scene_in = 0, equip_in = 0, junk_in = 0;
    for (int k = 0; k < (int)b1.size(); k++) {
        if (!g.inlier_mask.empty() && g.inlier_mask[(size_t)k]) {
            if (k < n_scene) scene_in++;
            else if (k < n_scene + n_equip) equip_in++;
            else junk_in++;
        }
    }
    std::printf("known-rotation two-view: ok=%d panoramic=%d inliers %d (scene %d/%d, equipment "
                "%d/%d, junk %d/%d), t err %.2f deg\n",
                g.ok, g.panoramic, g.num_inliers, scene_in, n_scene, equip_in, n_equip, junk_in,
                n_junk, g.ok ? angleDeg(g.pose.t, t) : 0.0);
    check(g.ok && !g.panoramic, "known-rotation: geometry found");
    check(scene_in >= 0.9 * n_scene, "known-rotation: the scene is kept");
    check(equip_in <= 0.02 * n_equip, "known-rotation: equipment rejected");
    check(junk_in <= 5, "known-rotation: junk rejected");
    check(g.ok && angleDeg(g.pose.t, t) < 1.0, "known-rotation: translation within 1 deg");

    // The free estimate on the same pair, for the record: with 40% of the
    // matches on the equipment it is what the prior exists to overrule.
    TwoViewOptions tv;
    tv.ransac.max_error = 0.003;
    const TwoViewGeometry f = estimateTwoViewBearing(b1, b2, tv);
    int f_equip = 0;
    for (int k = n_scene; k < n_scene + n_equip; k++)
        if (!f.inlier_mask.empty() && f.inlier_mask[(size_t)k]) f_equip++;
    std::printf("  free estimate: config %s, %d inliers, %d of them equipment\n",
                twoViewConfigName(f.config), f.num_inliers, f_equip);

    // A panorama: no translation at all.
    std::vector<Vec3> p1, p2;
    for (int k = 0; k < 200; k++) {
        const Vec3 a = Vec3{U(rng), U(rng), 1.5}.normalized();
        p1.push_back(a);
        p2.push_back((mul(R, a) + Vec3{noise * N(rng), noise * N(rng), noise * N(rng)}).normalized());
    }
    const KnownRotationGeometry pg = estimateTwoViewKnownRotation(p1, p2, R, ko);
    std::printf("  panorama: ok=%d panoramic=%d rotation-only %d/%zu\n", pg.ok, pg.panoramic,
                pg.rotation_only, p1.size());
    check(pg.ok && pg.panoramic, "known-rotation: a panorama is flagged");
}

static void testKnownRotationPnP() {
    std::mt19937 rng(9);
    std::normal_distribution<double> N(0, 1);
    std::uniform_real_distribution<double> U(-1, 1);
    const Pose truth{angleAxisToRotation({-0.2, 0.4, 0.1}), {0.3, -0.7, 2.0}};
    std::vector<Vec3> X, b;
    const int n_good = 60, n_bad = 60;
    for (int k = 0; k < n_good; k++) {
        const Vec3 Xw{3.0 * U(rng), 3.0 * U(rng), 3.0 * U(rng)};
        Vec3 pc = mul(truth.R, Xw) + truth.t;
        if (pc.z < 0.5) pc.z = 0.5 + std::fabs(pc.z);
        const Vec3 Xfix = mul(transpose(truth.R), pc - truth.t);
        X.push_back(Xfix);
        b.push_back((pc.normalized() + Vec3{2e-4 * N(rng), 2e-4 * N(rng), 2e-4 * N(rng)}).normalized());
    }
    for (int k = 0; k < n_bad; k++) {
        X.push_back({3.0 * U(rng), 3.0 * U(rng), 3.0 * U(rng)});
        b.push_back(Vec3{U(rng), U(rng), 1.0}.normalized());
    }
    const PnPResult r = ransacPnPKnownRotation(X, b, truth.R, 1000.0, 3.0, 1);
    std::printf("known-rotation PnP: ok=%d inliers %d/%d, t err %.4f\n", r.success, r.num_inliers,
                n_good + n_bad, r.success ? (r.pose.t - truth.t).norm() : 0.0);
    check(r.success && r.num_inliers >= 0.9 * n_good, "known-rotation PnP: inliers");
    check(r.success && (r.pose.t - truth.t).norm() < 0.01, "known-rotation PnP: translation");

    // The rig form: two lenses, the frame rotation given.
    const Pose ext1{mat3Identity(), {0, 0, 0}};
    const Pose ext2{angleAxisToRotation({0, M_PI, 0}), {0.02, 0, -0.05}};
    std::vector<Vec3> X2, b2;
    for (int k = 0; k < n_good; k++) {
        const Vec3 Xw{3.0 * U(rng), 3.0 * U(rng), 3.0 * U(rng)};
        const Vec3 pc = mul(ext2.R, mul(truth.R, Xw) + truth.t) + ext2.t;
        X2.push_back(Xw);
        b2.push_back((pc.normalized() + Vec3{2e-4 * N(rng), 2e-4 * N(rng), 2e-4 * N(rng)}).normalized());
    }
    std::vector<RigPnPMember> members = {{&X, &b, ext1, 0.003}, {&X2, &b2, ext2, 0.003}};
    const RigPnPResult rr = ransacRigPnPKnownRotation(members, truth.R, 2);
    std::printf("  rig form: ok=%d inliers %d, t err %.4f\n", rr.success, rr.num_inliers,
                rr.success ? (rr.rig_from_world.t - truth.t).norm() : 0.0);
    check(rr.success && (rr.rig_from_world.t - truth.t).norm() < 0.01, "known-rotation rig PnP");
}

// ---- the telemetry source on the synthetic walk ------------------------------

static void testTelemetryPriors() {
    Scenario sc;
    sc.clock_offset = 0.02;
    const Mat3 R_ci = angleAxisToRotation(Vec3{0.3, -1.2, 0.7});
    Telemetry t = synthesize(sc, R_ci);
    const TelemetryCheck c = telemetry_check(t);
    SensorTimeline tl;
    std::string err;
    check(tl.init(t, c, err), "timeline init: " + err);
    Sim3 M;
    M.scale = 0.37;
    M.R = angleAxisToRotation(Vec3{1.1, 0.4, -0.9});
    M.t = {2.0, -1.0, 0.5};
    // Two frames a second, so the chains and the triples have something to hold.
    Reconstruction rec = synthesizeModel(sc, M, 2.0);
    std::vector<std::string> names;
    std::vector<uint32_t> cams;
    std::vector<uint32_t> ids;
    for (const auto& kv : rec.images) {
        ids.push_back(kv.first);
        names.push_back(kv.second.name);
        cams.push_back(1);
    }
    // Image ids are the database positions; the model's are 1-based.
    std::map<uint32_t, uint32_t> pos;
    for (uint32_t k = 0; k < ids.size(); k++) pos[ids[k]] = k;
    SensorCapture cap;
    cap.fps = 24;
    cap.timeline = &tl;
    SensorPriorOptions po;
    TelemetryPriors src({cap}, names, cams, po);
    check(src.timedImages() == names.size(), "every image timed");

    // Calibrate from the pairs' own relative rotations, a little noisy.
    std::mt19937 rng(4);
    std::normal_distribution<double> N(0, 1);
    std::vector<PairRotationObs> obs;
    for (uint32_t k = 0; k + 1 < ids.size(); k++) {
        const Pose& a = rec.images.at(ids[k]).pose;
        const Pose& b = rec.images.at(ids[k + 1]).pose;
        PairRotationObs o;
        o.i = k;
        o.j = k + 1;
        // A tenth of a degree, what a verified pair's essential matrix gives
        // on a real capture; the offset search reads the mismatch at 20 ms.
        const double s = 0.08 * M_PI / 180.0;
        o.R_ji = mul(angleAxisToRotation({s * N(rng), s * N(rng), s * N(rng)}), mul(b.R, transpose(a.R)));
        obs.push_back(o);
    }
    src.calibrateFromPairs(obs);
    check(src.groups().size() == 1 && src.groups()[0].ok, "calibrated from pairs");
    if (!src.groups().empty()) {
        const SensorGroupState& g = src.groups()[0];
        // The sign of X is open before gravity: compare against both.
        const double e = std::min(angleDeg(g.X, R_ci), angleDeg(mat3Scale(g.X, -1.0), R_ci));
        std::printf("pair calibration: ok=%d pairs=%d sig_rot=%.2f deg, X within %.2f deg, "
                    "offset %.1f ms (found %d)\n", g.ok, g.pairs, g.fit.sig_rot_deg, e,
                    1000.0 * src.timeOffsets()[0].offset, src.timeOffsets()[0].found);
        check(e < 1.0, "pair calibration: extrinsic within 1 deg");
        check(src.timeOffsets()[0].found && std::fabs(src.timeOffsets()[0].offset - sc.clock_offset) < 0.006,
              "pair calibration: clock offset recovered");
    }
    // Relative rotations against the poses.
    double worst = 0;
    int n_rel = 0;
    for (uint32_t k = 0; k + 3 < ids.size(); k += 7) {
        Mat3 R;
        double sig;
        if (!src.relativeRotation(k, k + 3, R, sig)) continue;
        const Pose& a = rec.images.at(ids[k]).pose;
        const Pose& b = rec.images.at(ids[k + 3]).pose;
        worst = std::max(worst, angleDeg(R, mul(b.R, transpose(a.R))));
        n_rel++;
    }
    std::printf("relative rotations: %d checked, worst %.3f deg\n", n_rel, worst);
    check(n_rel > 20 && worst < 1.5, "relative rotations within 1.5 deg of the poses");
    check(src.neighbours(10).size() >= 4, "neighbours");

    // The factors over the posed model.
    std::vector<PosedImage> imgs;
    for (uint32_t k = 0; k < ids.size(); k++) imgs.push_back({k, 1, rec.images.at(ids[k]).pose});
    const PosePriors pf = src.factors(imgs);
    const SensorFactorStats& st = src.lastFactors();
    const Vec3 up_model = mul(transpose(M.R), Vec3{0, 0, 1});
    std::printf("factors: %zu rotations, %zu ups, %zu centres | up ok=%d spread %.2f deg, up err "
                "%.3f deg | scale ok=%d s=%.4f (true %.4f) sigma %.2f%% g %.1f deg | gps ok=%d "
                "n=%d rms %.2f\n",
                pf.rotations.size(), pf.ups.size(), pf.centres.size(), st.up_ok, st.up_spread_deg,
                angleDeg(pf.up_w, up_model), st.scale_ok, st.scale, M.scale, 100 * st.scale_sigma,
                st.g_angle_deg, st.gps_ok, st.gps, st.gps_rms);
    check(st.up_ok && angleDeg(pf.up_w, up_model) < 0.5, "factors: up axis within 0.5 deg");
    check(pf.ups.size() >= 0.9 * ids.size(), "factors: an up per frame");
    check(pf.rotations.size() >= ids.size() - 2, "factors: a rotation per consecutive pair");
    check(st.scale_ok && std::fabs(st.scale / M.scale - 1.0) < 0.03, "factors: scale within 3%");
    check(st.gps_ok && st.gps >= 0.8 * (int)ids.size(), "factors: GPS positions");
    // Every factor's residual at the true poses must be small: the model is
    // the truth, so only the sensors' own noise and the fit's remain.
    double worst_up = 0, worst_rot = 0, worst_c = 0;
    auto camPose = [&](uint32_t k) { return rec.images.at(ids[k]).pose; };
    for (const PriorUp& u : pf.ups)
        worst_up = std::max(worst_up, angleDeg(mul(camPose(u.i).R, pf.up_w), u.u));
    for (const PriorRotation& r : pf.rotations)
        worst_rot = std::max(worst_rot, angleDeg(mul(r.R_ji, camPose(r.i).R), camPose(r.j).R));
    int gps_n = 0, tri_n = 0;
    double gps_rms = 0, tri_rel = 0;
    for (const PriorCentre& f : pf.centres) {
        Vec3 sum{0, 0, 0};
        for (int k = 0; k < f.n; k++) sum = sum + mul(f.A[k], cameraCenter(camPose(f.img[k])));
        const Vec3 d = sum - f.b;
        if (f.n == 1) {
            gps_rms += d.x * d.x + d.y * d.y;
            gps_n++;
        } else {
            tri_rel = std::max(tri_rel, d.norm() / std::max(1e-9, f.sigma.x));
            tri_n++;
        }
        worst_c = std::max(worst_c, d.norm());
    }
    gps_rms = gps_n ? std::sqrt(gps_rms / gps_n) : 0;
    std::printf("  residuals at the truth: up %.2f deg, rotation %.3f deg, gps rms %.2f m over %d, "
                "%d triples worst %.1f sigma\n", worst_up, worst_rot, gps_rms, gps_n, tri_n, tri_rel);
    check(worst_rot < 1.0, "factors: rotation residual");
    check(worst_up < 15.0, "factors: up residual");
    check(gps_n > 0 && gps_rms < 4.0, "factors: gps residual");
    check(tri_n >= 10, "factors: triples");

    // The same source through a renumbering.
    std::vector<uint32_t> to_global;
    for (uint32_t k = 20; k < 60; k++) to_global.push_back(k);
    RemappedPriorSource sub(src, to_global);
    Mat3 Ra, Rb;
    double sa, sb;
    const bool ok_a = sub.relativeRotation(0, 1, Ra, sa), ok_b = src.relativeRotation(20, 21, Rb, sb);
    check(ok_a && ok_b && angleDeg(Ra, Rb) < 1e-9, "remapped: relative rotation");
    check(!sub.has(45) || sub.neighbours(5).size() >= 2, "remapped: neighbours");
    std::vector<PosedImage> local;
    for (uint32_t k = 0; k < to_global.size(); k++)
        local.push_back({k, 1, rec.images.at(ids[to_global[k]]).pose});
    const PosePriors lp = sub.factors(local);
    bool in_range = true;
    for (const PriorRotation& r : lp.rotations) in_range = in_range && r.i < 40 && r.j < 40;
    for (const PriorUp& u : lp.ups) in_range = in_range && u.i < 40;
    check(in_range && lp.rotations.size() >= 38, "remapped: factors on local ids");
}

int cmdSensorPriorTest(int, char**) {
    testKnownRotationTwoView();
    testKnownRotationPnP();
    testTelemetryPriors();
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdSensorPriorTest); }
