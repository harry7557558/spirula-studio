// scan_correction -- a capture's scan following its refined photographs
// (app/ScanCorrection.h) on a synthetic 300 s walk with two cameras: the device's
// trajectory drifts up to 62 cm and 1 deg from the truth, each camera's mount is
// turned a fixed 0.7 or 0.4 deg, each refined pose has 1.5 cm and 0.1 deg of
// its own error, and five instants are 30 cm off. The moves must give back the
// drift, not the mounts, and leave the five out.

#include "app/ScanCorrection.h"
#include "sfm/core/Pose.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

using sfm::Mat3;
using sfm::Vec3;

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr int kInstants = 300;
const std::set<int> kOutliers = {40, 41, 120, 200, 260};

struct Rigid {
    Mat3 R = sfm::mat3Identity();
    Vec3 t;
};

Rigid compose(const Rigid& a, const Rigid& b) {
    return {sfm::mul(a.R, b.R), sfm::mul(a.R, b.t) + a.t};
}

// The device's IMU pose: a loop walked, turned as a hand turns it (14 deg/s
// median on a PortalCam walk) and rolled.
Rigid device_imu(double s) {
    const double yaw = 0.05 * s + 0.7 * std::sin(0.45 * s) + 0.3 * std::sin(1.3 * s);
    const Mat3 R = sfm::mul(sfm::so3Exp({0, 0, yaw}), sfm::so3Exp({0.3 * std::sin(0.7 * s), 0.2 * std::cos(0.55 * s), 0}));
    return {R, {8 * std::cos(0.02 * s), 5 * std::sin(0.02 * s), 1.4 + 0.1 * std::sin(0.5 * s)}};
}

// How far the device's world is from the truth: slow, as drift is, and with no
// mean turn over the walk, as under the refinement's priors on each instant.
Rigid drift(double s) {
    const double w = 2 * kPi / kInstants;
    return {sfm::so3Exp({0.3 * kDeg * std::sin(2 * w * s), 0.2 * kDeg * std::cos(3 * w * s), kDeg * std::sin(w * s)}),
            {0.4 * std::sin(w * s), 0.25 * std::cos(1.5 * w * s) - 0.25, 0.05 * std::sin(4 * w * s)}};
}

double turn_deg(const Mat3& a, const Mat3& b) {
    return sfm::so3Log(sfm::mul(sfm::transpose(a), b)).norm() / kDeg;
}

Rigid key_move(const std::array<double, 6>& m) {
    return {sfm::so3Exp({m[0], m[1], m[2]}), {m[3], m[4], m[5]}};
}

}  // namespace

int main() {
    const Rigid mount[2] = {{sfm::so3Exp({kPi / 2, 0, 0}), {0.05, 0, 0.02}},
                            {sfm::so3Exp({kPi / 2, kPi, 0}), {-0.05, 0, 0.02}}};
    const Mat3 mount_error[2] = {sfm::so3Exp({0.7 * kDeg, 0, 0}), sfm::so3Exp({0, 0.2 * kDeg, 0.35 * kDeg})};
    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0, 1);
    auto jitter = [&](double sigma) { return Vec3{sigma * noise(rng), sigma * noise(rng), sigma * noise(rng)}; };

    std::vector<app::lidar::PosePair> pairs;
    const int kLeftOut = 150;   // an instant the refinement did not move
    for (int k = 0; k < kInstants; k++) {
        if (k == kLeftOut) continue;
        const double s = k + 0.5;
        for (int c = 0; c < 2; c++) {
            const Rigid D = compose(device_imu(s), mount[c]);
            Rigid F = compose(drift(s), D);
            F.R = sfm::mul(F.R, mount_error[c]);
            F.R = sfm::mul(sfm::so3Exp(jitter(0.1 * kDeg / std::sqrt(3.0))), F.R);
            F.t = F.t + jitter(0.015 / std::sqrt(3.0));
            if (kOutliers.count(k)) F.t = F.t + Vec3{0.3, 0, 0};
            app::lidar::PosePair p;
            p.time_us = (int64_t)std::llround(s * 1e6) + c * 40;
            p.camera = c == 0 ? "camera_0" : "camera_1";
            p.R_device = D.R;
            p.c_device = D.t;
            p.R_refined = F.R;
            p.c_refined = F.t;
            pairs.push_back(p);
        }
    }

    const app::lidar::ScanCorrection sc = app::lidar::scan_correction(pairs, 6.0);
    expect((int)sc.instants.size() == kInstants - 1, "one instant for both cameras' photographs of a moment");
    std::printf("     fixed turn %.3f deg, mean shift %.1f cm\n", sc.mount_deg, 100 * sc.mount_shift_m);
    expect(std::abs(sc.mount_deg - 0.7) < 0.05, "the larger mount error, 0.7 deg, is what is left out");

    bool outliers_out = true;
    for (size_t i = 0; i < sc.instants.size(); i++) {
        const int k = (int)(sc.instants[i] / 1000000);
        if (kOutliers.count(k) && sc.kept[i]) outliers_out = false;
    }
    std::printf("     dropped %lld\n", (long long)sc.rejected);
    expect(outliers_out, "the five instants 30 cm off are dropped");
    expect(sc.rejected <= 15, "few others are dropped");

    // A scan point near the lead camera, placed by the device, then moved.
    std::vector<double> shift_err, turn_err;
    for (size_t i = 0; i < sc.keys.time_us.size(); i++) {
        const double s = sc.keys.time_us[i] * 1e-6;
        const Rigid truth = drift(s);
        const Rigid got = key_move(sc.keys.move[i]);
        const Vec3 p = compose(device_imu(s), mount[0]).t + Vec3{1, 2, 0.5};
        shift_err.push_back(((sfm::mul(got.R, p) + got.t) - (sfm::mul(truth.R, p) + truth.t)).norm());
        turn_err.push_back(turn_deg(got.R, truth.R));
    }
    std::sort(shift_err.begin(), shift_err.end());
    std::sort(turn_err.begin(), turn_err.end());
    const double med = shift_err[shift_err.size() / 2], worst = shift_err.back();
    std::printf("     moved point %.1f cm off the truth (median), %.1f cm worst; turn %.3f deg (median)\n",
                100 * med, 100 * worst, turn_err[turn_err.size() / 2]);
    expect(med < 0.01, "a moved scan point is within 1 cm of the truth (median)");
    expect(worst < 0.03, "... and within 3 cm everywhere");
    expect(turn_err[turn_err.size() / 2] < 0.05, "the move turns the scan as the drift did, within 0.05 deg");

    // The instant left out: its photograph goes where the scan goes.
    const double s = kLeftOut + 0.5;
    const Rigid D = compose(device_imu(s), mount[0]);
    const Rigid moved = compose(key_move(sc.keys.at((int64_t)std::llround(s * 1e6))), D);
    const Rigid truth = compose(drift(s), D);
    const double off = (moved.t - truth.t).norm();
    std::printf("     unrefined photograph %.1f cm and %.3f deg off the drift\n", 100 * off, turn_deg(moved.R, truth.R));
    expect(off < 0.02, "a photograph the refinement did not move follows the scan, within 2 cm");

    std::printf("%s (%d failures)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
