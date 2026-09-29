// A synthetic capture for the sensor tests: a walking trajectory with a
// known IMU-to-lens rotation, IMU noise, biases, a GPS log, a clock offset,
// and its reconstruction in a random gauge.
#pragma once

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"
#include "sfm/core/Telemetry.h"

namespace synth_telemetry {
using namespace sfm;

// ---- the synthetic world ---------------------------------------------------

struct Scenario {
    double duration = 120;
    double motion = 1.0;       // 0 = a camera that never moves
    double turning = 1.0;      // 0 = a camera that never rotates
    bool gps = true;
    bool stale_gps = false;
    bool mirrored = false;     // IMU axes reported left-handed
    double clock_offset = 0;   // IMU clock ahead of the video by this many seconds
    Vec3 bg{0.004, -0.003, 0.002};
    Vec3 ba{0.03, -0.02, 0.05};
    double gyro_noise = 0.002, accel_noise = 0.02;
    // A camera writing a fused attitude and one accelerometer reading per
    // frame instead of a raw gyro: the DJI Osmo 360.
    bool attitude_only = false;
    double accel_rate = 0;   // 0 keeps the 1 kHz the gyro is written at
    unsigned seed = 7;
};

// Camera-to-world pose of the walk at t: an arc with a bob and a wobble.
inline void poseAt(const Scenario& sc, double t, Mat3& R_wc, Vec3& p) {
    const double m = sc.motion, q = sc.turning;
    const double w = 2 * M_PI / 60;
    // A walk round a circle with speed changes and turns of about 0.5 m/s^2:
    // the low-frequency accelerations frames a second apart can see.
    const double th = (w * t + 0.12 * std::sin(0.5 * t) + 0.03 * std::sin(1.3 * t + 0.7)) * m;
    const double rad = 15 + 0.6 * std::sin(0.9 * t) * m;
    p = {rad * std::cos(th) + 0.03 * std::sin(2 * M_PI * 2 * t) * m,
         rad * std::sin(th) + 0.03 * std::cos(2 * M_PI * 1.9 * t) * m,
         1.5 + 0.02 * std::sin(2 * M_PI * 2 * t + 0.4) * m + 0.4 * std::sin(0.31 * t) * m};
    const double yaw = (th + 0.5 * std::sin(0.7 * t)) * q, pitch = 0.35 * std::sin(0.45 * t + 1.0) * q,
                 roll = 0.25 * std::sin(0.83 * t + 2.0) * q;
    // Camera looks along +z of a frame that is yawed/pitched/rolled; y down.
    const Mat3 Rz = angleAxisToRotation({0, 0, yaw});
    const Mat3 Rx = angleAxisToRotation({pitch, 0, 0});
    const Mat3 Ry = angleAxisToRotation({0, roll, 0});
    // Base: camera z = world x (forward), camera y = world -z (down), camera x = world -y.
    const Mat3 base = {0, -1, 0, 0, 0, -1, 1, 0, 0};   // columns are camera axes in world
    R_wc = mul(mul(mul(Rz, Rx), Ry), transpose(base));
}

inline Telemetry synthesize(const Scenario& sc, const Mat3& R_ci) {
    std::mt19937 rng(sc.seed);
    std::normal_distribution<double> N(0, 1);
    Telemetry t;
    t.carrier = TelemetryCarrier::Camm;
    t.camera = "synthetic";
    t.video_fps = 24;
    t.video_duration = sc.duration;
    const double dt = 1e-3;
    const Vec3 g_w{0, 0, -9.81};
    const Mat3 D = sc.mirrored ? Mat3{-1, 0, 0, 0, 1, 0, 0, 0, 1} : mat3Identity();
    for (double ti = -0.5; ti <= sc.duration + 0.5; ti += dt) {
        Mat3 R0, R1, R2;
        Vec3 p0, p1, p2;
        const double h = 2e-3;
        poseAt(sc, ti - h, R0, p0);
        poseAt(sc, ti, R1, p1);
        poseAt(sc, ti + h, R2, p2);
        const Vec3 acc_w = (p2 - p1 * 2.0 + p0) * (1.0 / (h * h));
        // omega in the camera frame from the finite-difference of R_wc.
        const Mat3 dR = mul(transpose(R0), R2);
        const Vec3 omega_c = rotationToAngleAxis(dR) * (1.0 / (2 * h));
        const Mat3 R_wi = mul(R1, R_ci);
        const Vec3 omega_i = mul(transpose(R_ci), omega_c);
        const Vec3 f_i = mul(transpose(R_wi), acc_w - g_w);
        const double adt = sc.accel_rate > 0 ? 1.0 / sc.accel_rate : dt;
        Vec3 wm = mul(D, omega_i + sc.bg + Vec3{N(rng), N(rng), N(rng)} * (sc.gyro_noise / std::sqrt(dt)));
        Vec3 am = mul(D, f_i + sc.ba + Vec3{N(rng), N(rng), N(rng)} * (sc.accel_noise / std::sqrt(adt)));
        const double tv = ti + sc.clock_offset;
        if (sc.attitude_only) {
            const Quat q = rotationToQuaternion(mul(R_wi, transpose(D)));
            t.orientation.push_back({tv, q[0], q[1], q[2], q[3]});
        } else {
            t.gyro.push_back({tv, wm.x, wm.y, wm.z});
        }
        if (sc.accel_rate > 0 && std::fmod(ti + 0.5, adt) >= dt) continue;
        t.accel.push_back({tv, am.x, am.y, am.z});
    }
    if (sc.gps) {
        const double lat0 = 43.66, lon0 = -79.39;
        const double Re = 6378137.0;
        for (double ti = 0; ti <= sc.duration; ti += 0.1) {
            Mat3 R;
            Vec3 p;
            const double tf = sc.stale_gps ? 0.0 : std::floor(ti);   // 1 Hz updates
            poseAt(sc, tf, R, p);
            std::mt19937 rj((unsigned)(tf * 1000) + sc.seed);
            const double e = p.x + 1.0 * N(rj), n = p.y + 1.0 * N(rj), u = p.z + 3.0 * N(rj);
            TelemetryGps g;
            g.t = ti;
            g.fix = true;
            g.lat = lat0 + n / Re * 180 / M_PI;
            g.lon = lon0 + e / (Re * std::cos(lat0 * M_PI / 180)) * 180 / M_PI;
            g.alt = 100 + u;
            g.has_alt = true;
            g.dop = 1.5;
            t.gps.push_back(g);
        }
    }
    return t;
}

// The reconstruction: one image per second, poses in a random gauge M
// (world = M(model)), so the solver has to find M.
inline Reconstruction synthesizeModel(const Scenario& sc, const Sim3& M, double fps_frames = 1.0) {
    Reconstruction rec;
    Camera cam;
    cam.width = 1000;
    cam.height = 1000;
    rec.cameras[1] = cam;
    const Sim3 Minv = invertSim3(M);
    uint32_t id = 1;
    for (double ti = 0.5; ti < sc.duration; ti += 1.0 / fps_frames) {
        Mat3 R_wc;
        Vec3 p;
        poseAt(sc, ti, R_wc, p);
        Pose world;
        world.R = transpose(R_wc);
        world.t = mul(world.R, p) * -1.0;
        Image im;
        im.id = id;
        im.camera_id = 1;
        char name[64];
        std::snprintf(name, sizeof name, "cam0/%05d.jpg", (int)std::lround(ti * 24));
        im.name = name;
        im.pose = transformPose(Minv, world);
        im.registered = true;
        rec.images[id++] = im;
    }
    return rec;
}

}  // namespace synth_telemetry
