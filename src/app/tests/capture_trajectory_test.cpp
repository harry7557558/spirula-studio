// A capture's trajectory made again from its LiDAR (app/CaptureTrajectory.h):
// two laps of a synthetic room whose recorded trajectory drifts 5 degrees and
// half a metre over the walk. The rebuilt one must close the laps.

#include "app/CaptureTrajectory.h"
#include "data/XbinReader.h"
#include "data/tests/XbinTestCapture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace xbin_test;

namespace {

int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "ok" : "FAIL", name);
    if (!ok) ++failures;
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kSeconds = 60.0;
constexpr int kSweepPoints = 1500;

struct Box {
    double lo[3], hi[3];
};
// The room, then what stands in it, in metres; the device walks at z = 0.
const Box kRoom = {{-6, -4, -1.2}, {6, 4, 1.8}};
const Box kInside[] = {{{1.2, 0.2, -1.2}, {1.8, 0.8, 1.8}},
                       {{-2.3, -0.8, -1.2}, {-1.9, 0.2, 1.8}},
                       {{4.5, 2.6, -1.2}, {5.5, 4.0, 0.5}},
                       {{-5.0, 2.5, -1.2}, {-4.0, 4.0, 1.8}},
                       {{-0.5, -4.0, -1.2}, {0.7, -3.2, -0.4}}};

// Distance along unit ray d from o to the first surface.
double cast(const double o[3], const double d[3]) {
    double best = 1e9;
    for (int a = 0; a < 3; a++) {
        if (std::fabs(d[a]) < 1e-12) continue;
        const double t = ((d[a] > 0 ? kRoom.hi[a] : kRoom.lo[a]) - o[a]) / d[a];
        if (t > 0) best = std::min(best, t);
    }
    for (const Box& b : kInside) {
        double t0 = 0, t1 = 1e9;
        for (int a = 0; a < 3; a++) {
            if (std::fabs(d[a]) < 1e-12) {
                if (o[a] < b.lo[a] || o[a] > b.hi[a]) t0 = 1e9;
                continue;
            }
            double u = (b.lo[a] - o[a]) / d[a], v = (b.hi[a] - o[a]) / d[a];
            if (u > v) std::swap(u, v);
            t0 = std::max(t0, u);
            t1 = std::min(t1, v);
        }
        if (t0 <= t1 && t0 > 0) best = std::min(best, t0);
    }
    return best;
}

// The true walk: an ellipse twice, facing along it.
void truth(double t, double c[3], double& yaw) {
    const double a = 4 * kPi * t / kSeconds;
    c[0] = 4.0 * std::cos(a);
    c[1] = 2.2 * std::sin(a);
    c[2] = 0;
    yaw = std::atan2(2.2 * std::cos(a), -4.0 * std::sin(a));
}

// What the device recorded: the walk turned and shifted more as it goes.
void drifted(double t, double c[3], double& yaw) {
    truth(t, c, yaw);
    const double turn = 0.08 * kPi / 180.0 * t;
    const double x = c[0], y = c[1];
    c[0] = std::cos(turn) * x - std::sin(turn) * y + 0.008 * t;
    c[1] = std::sin(turn) * x + std::cos(turn) * y + 0.004 * t;
    yaw += turn;
}

std::string poses_csv() {
    std::string out;
    char line[160];
    for (double t = 0; t <= kSeconds + 1e-9; t += 0.1) {
        double c[3], yaw;
        drifted(t, c, yaw);
        std::snprintf(line, sizeof line, "%.6f,%.6f,%.6f,%.6f,0,0,%.12f,%.12f\n", 1.0 + t, c[0], c[1], c[2],
                      std::sin(0.5 * yaw), std::cos(0.5 * yaw));
        out += line;
    }
    return out;
}

// Sweeps of 0.1 s from the true walk, in the LiDAR's frame: the IMU is 1 cm
// ahead of it (kImuLidar), no turn between them.
std::vector<Bytes> sweeps() {
    std::vector<Bytes> out;
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> az(-kPi, kPi), el(-30 * kPi / 180, 50 * kPi / 180);
    std::normal_distribution<double> noise(0.0, 0.005);
    for (int s = 0; s * 0.1 < kSeconds - 0.1; s++) {
        Bytes sweep, head;
        field(head, 1, (uint64_t)s);
        field(head, 2, zz(1000000 + s * 100000));
        field(sweep, 1, head);
        for (int i = 0; i < kSweepPoints; i++) {
            const int64_t dt = i * 99999 / kSweepPoints;
            double c[3], yaw;
            truth(s * 0.1 + dt * 1e-6, c, yaw);
            const double o[3] = {c[0] - 0.01 * std::cos(yaw), c[1] - 0.01 * std::sin(yaw), c[2]};
            const double ea = el(rng), aa = az(rng);
            const double local[3] = {std::cos(ea) * std::cos(aa), std::cos(ea) * std::sin(aa), std::sin(ea)};
            const double d[3] = {std::cos(yaw) * local[0] - std::sin(yaw) * local[1],
                                 std::sin(yaw) * local[0] + std::cos(yaw) * local[1], local[2]};
            const double r = cast(o, d) + noise(rng);
            field(sweep, 2,
                  point(std::llround(1000 * r * local[0]), std::llround(1000 * r * local[1]),
                        std::llround(1000 * r * local[2]), 100, dt));
        }
        out.push_back(sweep);
    }
    return out;
}

// Median distance of the reader's IMU positions from the true walk, once a
// turn about the vertical and a shift have brought them onto it.
double error_m(const spirula::xbin::Reader& r) {
    std::vector<double> a, b;
    for (double t = 0.5; t < kSeconds - 0.5; t += 0.5) {
        double R[9], c[3], tc[3], yaw;
        if (!r.imu_pose(std::llround((1.0 + t) * 1e6), R, c)) continue;
        truth(t, tc, yaw);
        a.insert(a.end(), {c[0], c[1]});
        b.insert(b.end(), {tc[0], tc[1]});
    }
    const size_t n = a.size() / 2;
    double ma[2] = {0, 0}, mb[2] = {0, 0};
    for (size_t i = 0; i < n; i++)
        for (int k = 0; k < 2; k++) {
            ma[k] += a[2 * i + k] / n;
            mb[k] += b[2 * i + k] / n;
        }
    double sc = 0, ss = 0;
    for (size_t i = 0; i < n; i++) {
        const double ax = a[2 * i] - ma[0], ay = a[2 * i + 1] - ma[1], bx = b[2 * i] - mb[0], by = b[2 * i + 1] - mb[1];
        sc += ax * bx + ay * by;
        ss += ax * by - ay * bx;
    }
    const double th = std::atan2(ss, sc);
    std::vector<double> e;
    for (size_t i = 0; i < n; i++) {
        const double ax = a[2 * i] - ma[0], ay = a[2 * i + 1] - ma[1];
        const double x = std::cos(th) * ax - std::sin(th) * ay + mb[0], y = std::sin(th) * ax + std::cos(th) * ay + mb[1];
        e.push_back(std::hypot(x - b[2 * i], y - b[2 * i + 1]));
    }
    std::nth_element(e.begin(), e.begin() + e.size() / 2, e.end());
    return e[e.size() / 2];
}

int run() {
    const fs::path dir = fs::temp_directory_path() / "spirula_capture_trajectory_test";
    fs::remove_all(dir);
    Layout layout;
    layout.sweeps = sweeps();
    const std::string poses = poses_csv();
    const fs::path path = write_capture(dir, poses.c_str(), {}, layout);
    {
        spirula::xbin::Reader r(path.string());
        const double before = error_m(r);
        app::lidar::TrajectoryReport report;
        const spirula::xbin::Correction L = app::lidar::build_trajectory(r, report);
        r.set_correction(L);
        const double after = error_m(r);
        std::printf("  %lld sweeps, %lld nodes, %lld loops of %lld tried, voxels %lld -> %lld\n",
                    (long long)report.sweeps, (long long)report.nodes, (long long)report.loops,
                    (long long)report.loops_tried, (long long)report.voxels_device, (long long)report.voxels_built);
        std::printf("  error from the true walk: %.3f m recorded, %.3f m rebuilt\n", before, after);
        std::printf("  read %.1f s, neighbours %.1f s, loops %.1f s, graph %.1f s\n", report.seconds_read,
                    report.seconds_neighbours, report.seconds_loops, report.seconds_graph);
        check(report.kept && !L.time_us.empty(), "the rebuilt trajectory is kept");
        check(report.loops > 0, "the second lap closes on the first");
        check(report.voxels_built < report.voxels_device, "the map is sharper");
        check(after < 0.25 * before && after < 0.05, "the rebuilt trajectory is near the true walk");

        app::lidar::write_trajectory((dir / "t.csv").string(), L);
        const spirula::xbin::Correction back = app::lidar::read_trajectory((dir / "t.csv").string());
        bool same = back.time_us == L.time_us;
        for (size_t i = 0; same && i < L.move.size(); i++)
            for (int k = 0; k < 6; k++) same = same && back.move[i][k] == L.move[i][k];
        check(same, "a trajectory file reads back exactly");
        check(app::lidar::read_trajectory((dir / "none.csv").string()).time_us.empty(),
              "no file is the device's trajectory");
    }
    fs::remove_all(dir);
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

}  // namespace

int main() {
    try {
        return run();
    } catch (const std::exception& e) {
        std::printf("FAIL threw: %s\n", e.what());
        return 1;
    }
}
