// The sensors as priors (docs/notes/sensor-priors.md): the fixed-rotation
// two-view and PnP estimators on scenes with equipment and outliers, then
// the telemetry source on the synthetic walk -- calibrated from pair
// rotations alone, its relative rotations, and the factors it states about
// a posed model in a random gauge (up, rotations, inertial scale, GPS).
//
//   sfm_sensor_prior_test
//
// Prints FAIL lines and returns the count. Needs no GPU.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Log.h"
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
    int positioned = 0;
    for (uint32_t k = 0; k < ids.size(); k++) {
        Vec3 p;
        positioned += src.position(k, p);
    }
    check(positioned > 0 && st.gps_ok && st.gps == positioned,
          "factors: GPS positions, one per positioned frame");
    // Every factor's residual at the true poses must be small: the model is
    // the truth, so only the sensors' own noise and the fit's remain.
    double worst_up = 0, worst_rot = 0, worst_c = 0;
    auto camPose = [&](uint32_t k) { return rec.images.at(ids[k]).pose; };
    for (const PriorUp& u : pf.ups)
        worst_up = std::max(worst_up, angleDeg(mul(camPose(u.i).R, pf.up_w), u.u));
    for (const PriorRotation& r : pf.rotations)
        worst_rot = std::max(worst_rot, angleDeg(mul(r.R_ji, camPose(r.i).R), camPose(r.j).R));
    int gps_n = 0, tri_n = 0, gps_vertical = 0;
    double gps_rms = 0, tri_rel = 0;
    for (const PriorCentre& f : pf.centres) {
        Vec3 sum{0, 0, 0};
        for (int k = 0; k < f.n; k++) sum = sum + mul(f.A[k], cameraCenter(camPose(f.img[k])));
        const Vec3 d = sum - f.b;
        if (f.n == 1) {
            gps_rms += d.x * d.x + d.y * d.y;
            gps_n++;
            if (f.sigma.z != 0.0) gps_vertical++;
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
    // With the IMU's up axis the GPS states the level pair only (D75).
    check(gps_n > 0 && gps_vertical == 0, "factors: gps is horizontal under an up axis");
    {
        // --metric-gps full on video: COLMAP's 1 m Cauchy factor, the vertical kept
        // although the IMU's up levels the fit.
        SensorPriorOptions tpo;
        tpo.trusted_position = true;
        TelemetryPriors trusted({cap}, names, cams, tpo);
        trusted.calibrateFromPairs(obs);
        int n = 0, ok = 0, plain = 0;
        for (const PriorCentre& f : trusted.factors(imgs).centres)
            if (f.n == 1) {
                n++;
                ok += f.cauchy == 7.815 && f.sigma.x == 1.0 && f.sigma.y == 1.0 && f.sigma.z == 1.0;
            }
        for (const PriorCentre& f : pf.centres) plain += f.n == 1 && f.cauchy == 0.0;
        check(n > 0 && ok == n && plain == gps_n, "factors: trusted GPS is 1 m Cauchy, vertical kept");
    }
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

    // A tenth of the log 30 m north (seconds 3, 13, 23, ...), six gates off.
    // Further is not reachable: the timeline drops a jump over 50 m/s. Frames
    // interpolate between 1 Hz fixes, so only some sit the whole 30 m off.
    Telemetry tr = t;
    for (TelemetryGps& g : tr.gps)
        if (std::fmod(std::floor(g.t), 10.0) == 3.0) g.lat += 30.0 / 6378137.0 * 180.0 / M_PI;
    SensorTimeline tl_r;
    check(tl_r.init(tr, telemetry_check(tr), err), "rogue timeline init: " + err);
    SensorCapture cap_r = cap;
    cap_r.timeline = &tl_r;
    TelemetryPriors rogue({cap_r}, names, cams, po);
    rogue.calibrateFromPairs(obs);
    // The two logs' ENU origins differ by the mean of the moved fixes.
    std::vector<Vec3> shift(ids.size());
    std::vector<char> pos_ok(ids.size(), 0);
    std::vector<double> dn;
    for (uint32_t k = 0; k < ids.size(); k++) {
        Vec3 a, b;
        if (!src.position(k, a) || !rogue.position(k, b)) continue;
        pos_ok[k] = 1;
        shift[k] = b - a;
        dn.push_back(shift[k].y);
    }
    std::nth_element(dn.begin(), dn.begin() + dn.size() / 2, dn.end());
    const double origin = dn.empty() ? 0.0 : dn[dn.size() / 2];
    std::vector<char> whole(ids.size(), 0), moved(ids.size(), 0);
    int n_pos = 0, n_whole = 0;
    for (uint32_t k = 0; k < ids.size(); k++) {
        if (!pos_ok[k]) continue;
        const double m = std::hypot(shift[k].x, shift[k].y - origin);
        moved[k] = m > 2.0;
        whole[k] = m > 25.0;
        n_pos++;
        n_whole += whole[k];
    }
    const PosePriors rp = rogue.factors(imgs);
    const SensorFactorStats rs = rogue.lastFactors();
    int single = 0, inl_n = 0;
    double inl_r2 = 0;
    for (const PriorCentre& f : rp.centres) {
        if (f.n != 1) continue;
        single++;
        if (moved[f.img[0]]) continue;
        const Vec3 d = mul(f.A[0], cameraCenter(camPose(f.img[0]))) - f.b;
        inl_r2 += d.x * d.x + d.y * d.y;
        inl_n++;
    }
    const double inl_rms = inl_n ? std::sqrt(inl_r2 / inl_n) : 0;
    int whole_ok = 0;
    double whole_lo = 1e9, whole_hi = 0;
    for (uint32_t k = 0; k < ids.size(); k++) {
        double d;
        if (!whole[k] || !rogue.positionError(k, camPose(k), rp.gps, d)) continue;
        whole_ok += d > 20.0 && d < 40.0;   // the 1 m / 3 m noise of the log on top
        whole_lo = std::min(whole_lo, d);
        whole_hi = std::max(whole_hi, d);
    }
    std::printf("  check on the moved frames: %.1f-%.1f m\n", whole_lo, whole_hi);
    std::printf("rogue fixes: %d of %d frames moved whole | %d gps factors (stats %d, out %d), the "
                "unmoved %.2f m rms at the truth\n", n_whole, n_pos, single, rs.gps, rs.gps_out, inl_rms);
    check(n_whole >= 10 && n_whole <= 0.15 * n_pos, "rogue: some frames moved whole, a minority");
    check(rs.gps_ok && single == n_pos && rs.gps == n_pos, "rogue: every positioned frame keeps its factor");
    check(rs.gps_out >= n_whole, "rogue: the moved frames are beyond the gate");
    check(inl_n > 0 && inl_rms < 4.0, "rogue: the unmoved factors' residual at the truth");
    check(rp.gps.ok && rp.gps.flat && whole_ok == n_whole, "rogue: the check reads the 30 m, four radii off");
}

// ---- a GPS-only source: why a fit is refused, and the level-only rule --------

// A GPS log with no IMU at all, 60 s at 10 Hz: a straight line east with a
// 0.1 m sway, or the same bent north halfway (`bend`).
static Vec3 gpsTrack(double t, bool bend) {
    const double e = bend ? std::min(t, 30.0) * 1.5 : 1.5 * t;
    const double n = bend ? std::max(t - 30.0, 0.0) * 1.5 : 0.0;
    return {e, n + 0.1 * std::sin(0.7 * t), 2.0 + 0.05 * std::sin(0.3 * t)};
}

static Telemetry gpsOnly(bool bend) {
    Telemetry t;
    t.carrier = TelemetryCarrier::DjiDvtm;
    t.camera = "synthetic";
    t.video_fps = 24;
    t.video_duration = 60;
    const double lat0 = 43.66, lon0 = -79.39, Re = 6378137.0;
    for (double ti = 0; ti <= 60.0 + 1e-9; ti += 0.1) {
        const Vec3 p = gpsTrack(ti, bend);
        TelemetryGps g;
        g.t = ti;
        g.fix = true;
        g.lat = lat0 + p.y / Re * 180 / M_PI;
        g.lon = lon0 + p.x / (Re * std::cos(lat0 * M_PI / 180)) * 180 / M_PI;
        g.alt = 100 + p.z;
        g.has_alt = true;
        g.dop = 1.0;
        t.gps.push_back(g);
    }
    return t;
}

// One image a second along the track, level cameras looking east, in a random gauge.
struct GpsOnlyScene {
    std::vector<std::string> names;
    std::vector<uint32_t> cams;
    std::vector<PosedImage> imgs;
};

static GpsOnlyScene gpsOnlyScene(bool bend) {
    GpsOnlyScene s;
    Sim3 M;
    M.scale = 0.21;
    M.R = angleAxisToRotation(Vec3{0.7, -0.3, 1.2});
    M.t = {1.0, 3.0, -2.0};
    const Sim3 Minv = invertSim3(M);
    // Camera x right (south), y down, z forward (east).
    const Mat3 R_wc = {0, 0, 1, -1, 0, 0, 0, -1, 0};
    for (int k = 0; k < 59; k++) {
        const double t = 0.5 + k;
        Pose world;
        world.R = transpose(R_wc);
        world.t = mul(world.R, gpsTrack(t, bend)) * -1.0;
        char nm[32];
        std::snprintf(nm, sizeof nm, "cam0/%05d.jpg", (int)std::lround(t * 24));
        s.names.push_back(nm);
        s.cams.push_back(1);
        s.imgs.push_back({(uint32_t)k, 1, transformPose(Minv, world)});
    }
    return s;
}

struct GpsOnlyRun {
    SensorFactorStats st;
    PosePriors pf;
    std::vector<std::string> lines;
};

static GpsOnlyRun gpsOnlyRun(bool bend, bool flat) {
    GpsOnlyRun r;
    const Telemetry t = gpsOnly(bend);
    SensorTimeline tl;
    std::string err;
    if (!tl.init(t, telemetry_check(t), err)) return r;
    const GpsOnlyScene s = gpsOnlyScene(bend);
    SensorCapture cap;
    cap.fps = 24;
    cap.timeline = &tl;
    SensorPriorOptions po;
    po.verbose = true;
    po.gps_flat = flat;
    TelemetryPriors src({cap}, s.names, s.cams, po);
    slog::set_sink([&](slog::Tag, slog::Level, const std::string& line) { r.lines.push_back(line); });
    r.pf = src.factors(s.imgs);
    slog::set_sink({});
    r.st = src.lastFactors();
    return r;
}

static int linesWith(const std::vector<std::string>& lines, const std::string& a,
                     const std::string& b = "") {
    int n = 0;
    for (const std::string& l : lines)
        n += l.find(a) != std::string::npos && (b.empty() || l.find(b) != std::string::npos);
    return n;
}

static void testGpsOnlySource() {
    const GpsOnlyRun line = gpsOnlyRun(false, false), bent = gpsOnlyRun(true, false);
    std::printf("GPS only, no IMU: straight ok=%d reason=%d perp %.4f, %zu centres | bent ok=%d "
                "reason=%d perp %.3f, %zu centres\n", line.st.gps_ok, (int)line.st.gps_reason,
                line.st.gps_perp_frac, line.pf.centres.size(), bent.st.gps_ok,
                (int)bent.st.gps_reason, bent.st.gps_perp_frac, bent.pf.centres.size());
    for (const std::string& l : line.lines) std::printf("  %s", l.c_str());
    check(bent.st.gps_ok && bent.st.gps_reason == MetricFail::None && bent.pf.centres.size() == 59,
          "fixture: the bent track fits in full");
    check(!line.st.gps_ok && line.pf.centres.empty() && line.st.gps_reason == MetricFail::Collinear &&
              line.st.gps_perp_frac > 0 && line.st.gps_perp_frac < kMetricMinPerpFraction,
          "GPS: a straight track with no up is refused as collinear, and says so");
    check(linesWith(line.lines, "[prior] GPS over 59 posed image(s): no fit (collinear") == 1,
          "GPS: a refused fit is logged with its reason");
    check(linesWith(bent.lines, "[prior] GPS over 59 posed image(s): fit") == 1,
          "GPS: a fit is logged");
    check(linesWith(line.lines, "EXIF") + linesWith(bent.lines, "EXIF") == 0,
          "GPS: the telemetry line is not labelled EXIF");

    // --metric-gps horizontal and no IMU up: level about the cameras' mean up.
    const GpsOnlyRun level = gpsOnlyRun(false, true);
    int vertical = 0, farCount = 0;
    const GpsOnlyScene s = gpsOnlyScene(false);
    for (const PriorCentre& f : level.pf.centres) {
        vertical += f.sigma.z != 0.0;
        Vec3 d = mul(f.A[0], cameraCenter(s.imgs[f.img[0]].pose)) - f.b;
        d.z = 0;
        farCount += d.norm() > 0.5;
    }
    std::printf("GPS only, horizontal: ok=%d reason=%d, %zu centres, %d vertical, %d level "
                "residual over 0.5 m\n", level.st.gps_ok, (int)level.st.gps_reason,
                level.pf.centres.size(), vertical, farCount);
    check(level.st.gps_ok && level.pf.centres.size() == 59 && level.pf.gps.flat && vertical == 0,
          "GPS horizontal: a straight track with no IMU up gets level factors");
    check(farCount == 0, "GPS horizontal: the level factors hold at the true poses");
}

// With an IMU up the telemetry fit is level whatever gps_flat says: the mean up
// of the cameras is the fallback, never a substitute.
static void testFlatKeepsImuUp() {
    Scenario sc;
    const Mat3 R_ci = angleAxisToRotation(Vec3{0.3, -1.2, 0.7});
    const Telemetry t = synthesize(sc, R_ci);
    SensorTimeline tl;
    std::string err;
    tl.init(t, telemetry_check(t), err);
    Sim3 M;
    M.scale = 0.37;
    M.R = angleAxisToRotation(Vec3{1.1, 0.4, -0.9});
    M.t = {2.0, -1.0, 0.5};
    Reconstruction rec = synthesizeModel(sc, M, 2.0);
    std::vector<std::string> names;
    std::vector<uint32_t> cams;
    std::vector<PosedImage> imgs;
    uint32_t k = 0;
    // The first 20 s only: over a whole turn a lens's pitch averages out of the mean up.
    for (const auto& kv : rec.images) {
        if (k == 40) break;
        names.push_back(kv.second.name);
        cams.push_back(1);
        imgs.push_back({k++, 1, kv.second.pose});
    }
    // Every lens pitched 20 deg on its IMU, so the cameras' mean up is 20 deg off
    // the IMU's while the centres stay put: a fit about the wrong one would show.
    const Mat3 tilt = angleAxisToRotation(Vec3{0.35, 0, 0});
    Vec3 cam_up{0, 0, 0};
    for (PosedImage& p : imgs) {
        const Vec3 c = cameraCenter(p.pose);
        p.pose.R = mul(tilt, p.pose.R);
        p.pose.t = mul(p.pose.R, c) * -1.0;
        cam_up = cam_up + mul(transpose(p.pose.R), Vec3{0, -1, 0});
    }
    SensorCapture cap;
    cap.fps = 24;
    cap.timeline = &tl;
    std::vector<PairRotationObs> obs;
    for (uint32_t q = 0; q + 1 < imgs.size(); q++)
        obs.push_back({q, q + 1, mul(imgs[q + 1].pose.R, transpose(imgs[q].pose.R))});
    auto run = [&](bool flat, SensorFactorStats& st) {
        SensorPriorOptions po;
        po.gps_flat = flat;
        TelemetryPriors src({cap}, names, cams, po);
        src.calibrateFromPairs(obs);
        const PosePriors pf = src.factors(imgs);
        st = src.lastFactors();
        std::vector<PriorCentre> out;
        for (const PriorCentre& f : pf.centres)
            if (f.n == 1) out.push_back(f);
        return out;
    };
    SensorFactorStats s0, s1;
    const std::vector<PriorCentre> a = run(false, s0), b = run(true, s1);
    const Vec3 imu_up = mul(transpose(M.R), Vec3{0, 0, 1});
    check(angleDeg(cam_up, imu_up) > 5.0, "fixture: the cameras' mean up is not the IMU's");
    double worst = 0;
    for (size_t q = 0; q < a.size() && q < b.size(); q++)
        worst = std::max(worst, (a[q].b - b[q].b).norm());
    std::printf("IMU up, gps_flat off / on: up %d / %d, %zu / %zu centres, worst target moved "
                "%.3g m\n", s0.up_ok, s1.up_ok, a.size(), b.size(), worst);
    check(s0.up_ok && !a.empty(), "fixture: the walk has an IMU up and GPS factors");
    check(a.size() == b.size() && worst < 1e-9, "GPS horizontal: an IMU up is kept over the cameras'");
}

// The GPS shares the IMU's clock: an image's fix is read at the fitted clock,
// as its rotation is. The telemetry here runs 120 ms ahead of the video.
static void testGpsClock() {
    Scenario sc;
    sc.clock_offset = 0.12;
    const Mat3 R_ci = angleAxisToRotation(Vec3{0.3, -1.2, 0.7});
    const Telemetry base = synthesize(sc, R_ci);
    const double lat0 = 43.66, lon0 = -79.39, Re = 6378137.0;
    auto withGps = [&](double shift) {
        Telemetry t = base;
        t.gps.clear();
        for (double ti = 0; ti <= sc.duration; ti += 0.1) {
            Mat3 R;
            Vec3 p;
            poseAt(sc, ti, R, p);
            TelemetryGps g;
            g.t = ti + shift;
            g.fix = true;
            g.lat = lat0 + p.y / Re * 180 / M_PI;
            g.lon = lon0 + p.x / (Re * std::cos(lat0 * M_PI / 180)) * 180 / M_PI;
            g.alt = 100 + p.z;
            g.has_alt = true;
            g.dop = 1.0;
            t.gps.push_back(g);
        }
        return t;
    };
    const Telemetry on_imu = withGps(sc.clock_offset), on_video = withGps(0.0);
    SensorTimeline ta, tb;
    std::string err;
    ta.init(on_imu, telemetry_check(on_imu), err);
    tb.init(on_video, telemetry_check(on_video), err);
    Sim3 M;
    Reconstruction rec = synthesizeModel(sc, M, 2.0);
    std::vector<std::string> names;
    std::vector<uint32_t> cams;
    std::vector<Pose> poses;
    std::vector<double> times;
    for (const auto& kv : rec.images) {
        names.push_back(kv.second.name);
        cams.push_back(1);
        poses.push_back(kv.second.pose);
        times.push_back(std::stoi(kv.second.name.substr(5, 5)) / 24.0);
    }
    SensorCapture ca, cb;
    ca.fps = cb.fps = 24;
    ca.timeline = &ta;
    cb.timeline = &tb;
    TelemetryPriors a({ca}, names, cams, SensorPriorOptions{});
    TelemetryPriors b({cb}, names, cams, SensorPriorOptions{});
    std::vector<PairRotationObs> obs;
    for (uint32_t k = 0; k + 1 < poses.size(); k++)
        obs.push_back({k, k + 1, mul(poses[k + 1].R, transpose(poses[k].R))});
    a.calibrateFromPairs(obs);
    double worst = 0;
    std::vector<double> motion;
    int n = 0;
    for (uint32_t k = 0; k < names.size(); k++) {
        Vec3 pa, pb;
        if (!a.position(k, pa) || !b.position(k, pb)) continue;
        worst = std::max(worst, (pa - pb).norm());
        Mat3 R;
        Vec3 p0, p1;
        poseAt(sc, times[k], R, p0);
        poseAt(sc, times[k] - sc.clock_offset, R, p1);
        motion.push_back((p0 - p1).norm());
        n++;
    }
    std::sort(motion.begin(), motion.end());
    const double median_motion = motion.empty() ? 0 : motion[motion.size() / 2];
    const TimeOffsetFit& off = a.timeOffsets()[0];
    std::printf("GPS clock: offset %.1f ms (found %d), %d frames, worst %.3f m off the video-clock "
                "fix; the walk moves %.3f m in the offset at the median frame\n",
                1000 * off.offset, off.found, n, worst, median_motion);
    check(off.found && std::fabs(off.offset - sc.clock_offset) < 0.006,
          "fixture: the clock offset is recovered");
    check(n > 100 && median_motion > 0.15, "fixture: the offset moves a frame over 0.15 m");
    check(n > 100 && worst < 0.03, "GPS: a fix is read at the fitted clock");
}

// ---- a fused attitude with no accelerometer to settle its sense -----------------

struct AttitudeRun {
    bool has_up = false, calibrated = false, gravity = false, right_handed = false;
    double sign = 0, x_err = 180, rel_worst = 180, up_err = 180, imu_up_err = 180;
    int rel_n = 0;
    size_t images = 0;
    PosePriors pf;
    SensorFactorStats st;
};

// Pairs up to 2.5 s apart, as the mapper's calibration spread: the wrong sense
// hides at short gaps (1.9 deg at 0.13 s on one Avata flight, 7.8 at 3 s).
static AttitudeRun runAttitude(const Scenario& sc, const Mat3& R_ci) {
    AttitudeRun out;
    Telemetry t = synthesize(sc, R_ci);
    const TelemetryCheck c = telemetry_check(t);
    SensorTimeline tl;
    std::string err;
    check(tl.init(t, c, err), "attitude: timeline init: " + err);
    out.has_up = tl.hasUp();
    check(tl.attitudeSenseOpen() == sc.no_accel, "attitude: sense open exactly when there is no accelerometer");
    Sim3 M;
    M.scale = 0.37;
    M.R = angleAxisToRotation(Vec3{1.1, 0.4, -0.9});
    M.t = {2.0, -1.0, 0.5};
    Reconstruction rec = synthesizeModel(sc, M, 2.0);
    std::vector<std::string> names;
    std::vector<uint32_t> cams, ids;
    for (const auto& kv : rec.images) {
        ids.push_back(kv.first);
        names.push_back(kv.second.name);
        cams.push_back(1);
    }
    out.images = ids.size();
    SensorCapture cap;
    cap.fps = 24;
    cap.timeline = &tl;
    TelemetryPriors src({cap}, names, cams, SensorPriorOptions());
    std::mt19937 rng(4);
    std::normal_distribution<double> N(0, 1);
    std::vector<PairRotationObs> obs;
    for (uint32_t d : {1u, 3u, 5u})
        for (uint32_t k = 0; k + d < ids.size(); k++) {
            const Pose& a = rec.images.at(ids[k]).pose;
            const Pose& b = rec.images.at(ids[k + d]).pose;
            const double s = 0.08 * M_PI / 180.0;
            obs.push_back({k, k + d, mul(angleAxisToRotation({s * N(rng), s * N(rng), s * N(rng)}),
                                         mul(b.R, transpose(a.R)))});
        }
    src.calibrateFromPairs(obs);
    if (src.groups().size() != 1) return out;
    const SensorGroupState& g0 = src.groups()[0];
    out.calibrated = g0.ok;
    out.x_err = std::min(angleDeg(g0.X, R_ci), angleDeg(mat3Scale(g0.X, -1.0), R_ci));
    out.rel_worst = 0;
    for (uint32_t k = 0; k + 3 < ids.size(); k += 7) {
        Mat3 R;
        double sig;
        if (!src.relativeRotation(k, k + 3, R, sig)) continue;
        const Pose& a = rec.images.at(ids[k]).pose;
        const Pose& b = rec.images.at(ids[k + 3]).pose;
        out.rel_worst = std::max(out.rel_worst, angleDeg(R, mul(b.R, transpose(a.R))));
        out.rel_n++;
    }
    std::vector<PosedImage> imgs;
    for (uint32_t k = 0; k < ids.size(); k++) imgs.push_back({k, 1, rec.images.at(ids[k]).pose});
    out.pf = src.factors(imgs);
    out.st = src.lastFactors();
    const SensorGroupState& g = src.groups()[0];
    out.gravity = g.gravity;
    out.right_handed = det3(g.X) > 0;
    out.sign = g.sign;
    out.up_err = angleDeg(out.pf.up_w, mul(transpose(M.R), Vec3{0, 0, 1}));
    // The vote itself, in the IMU frame, against the true up there.
    out.imu_up_err = 0;
    for (double ts = 5; ts < sc.duration - 5; ts += 7) {
        const UpVote v = tl.upAt(ts, 0.25, g.sign);
        Mat3 R_wc;
        Vec3 p;
        poseAt(sc, ts - sc.clock_offset, R_wc, p);
        const double e = v.ok ? angleDeg(v.up, mul(transpose(mul(R_wc, R_ci)), Vec3{0, 0, 1})) : 180.0;
        out.imu_up_err = std::max(out.imu_up_err, e);
    }
    std::printf("attitude (accel %s, up %s): calibrated=%d sign=%.0f X within %.2f deg, rel %d worst "
                "%.3f deg | %zu rotations, %zu ups, up err %.3f deg, IMU-frame up err %.3f deg, "
                "gravity refit %d, det %+.0f, gps %d, triples %d\n",
                sc.no_accel ? "none" : "30 Hz", sc.declare_up ? "declared" : "undeclared",
                out.calibrated, out.sign, out.x_err, out.rel_n, out.rel_worst, out.pf.rotations.size(),
                out.pf.ups.size(), out.up_err, out.imu_up_err, out.gravity, out.right_handed ? 1.0 : -1.0,
                out.st.gps, out.st.triples);
    return out;
}

static void testAttitudeWithoutAccel() {
    const Mat3 R_ci = angleAxisToRotation(Vec3{0.3, -1.2, 0.7});
    // The Avata 360: its vertical declared, as the DJI reader declares it.
    Scenario sc;
    sc.attitude_only = true;
    sc.no_accel = true;
    sc.z_down_world = true;
    sc.declare_up = true;
    const AttitudeRun a = runAttitude(sc, R_ci);
    check(a.calibrated && a.x_err < 1.0, "no accel: sense found, extrinsic within 1 deg");
    check(a.rel_n > 20 && a.rel_worst < 1.5, "no accel: relative rotations within 1.5 deg of the poses");
    check(a.pf.rotations.size() >= a.images - 2, "no accel: a rotation per consecutive pair");
    check(a.has_up && a.gravity, "no accel: the declared vertical is an up source");
    check(a.imu_up_err < 0.1, "no accel: up vote is the declared vertical in the IMU frame");
    check(a.st.up_ok && a.up_err < 0.5 && a.pf.ups.size() >= 0.9 * a.images,
          "no accel: an up per frame, axis within 0.5 deg");
    check(a.right_handed, "no accel: X a rotation, not a reflection");
    check(!a.st.scale_ok && a.st.triples == 0, "no accel: no inertial scale");
    check(a.st.gps_ok && a.st.gps >= 0.8 * (int)a.images, "no accel: GPS positions");

    // Undeclared, the rotations still hold but nothing says which way is up.
    sc.declare_up = false;
    const AttitudeRun b = runAttitude(sc, R_ci);
    check(b.calibrated && b.x_err < 1.0 && b.pf.rotations.size() >= b.images - 2,
          "no accel, undeclared: rotations without a vertical");
    check(!b.has_up && !b.gravity && b.pf.ups.empty() && !b.st.up_ok, "no accel, undeclared: no up");

    // With an accelerometer the declaration is not read: a wrong one changes nothing.
    Scenario so;
    so.attitude_only = true;
    so.accel_rate = 30;
    so.accel_noise = 0.06;
    Telemetry to = synthesize(so, R_ci);
    to.attitude_world_up[0] = 1;
    const TelemetryCheck co = telemetry_check(to);
    check(co.attitude_is_sensor_to_world, "accel: sense measured as sensor->world");
    SensorTimeline tlo;
    std::string err;
    check(tlo.init(to, co, err), "accel: timeline init: " + err);
    // The accelerometer settled the sense, so the conjugate is not a hypothesis
    // to test; the same stream without one leaves it open.
    check(!tlo.attitudeSenseOpen(), "accel: attitude sense settled, not open");
    Mat3 R_wc;
    Vec3 p;
    poseAt(so, 30.0, R_wc, p);
    const UpVote v = tlo.upAt(30.0);
    check(v.ok && angleDeg(v.up, mul(transpose(mul(R_wc, R_ci)), Vec3{0, 0, 1})) < 1.0,
          "accel: up from the accelerometer, not a declared vertical");
}

int cmdSensorPriorTest(int, char**) {
    testKnownRotationTwoView();
    testKnownRotationPnP();
    testTelemetryPriors();
    testAttitudeWithoutAccel();
    testGpsOnlySource();
    testFlatKeepsImuUp();
    testGpsClock();
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdSensorPriorTest); }
