// anchor_refine -- a capture's photographs refined from the device's poses
// (app/AnchorRefine.h) on a synthetic two-camera rig: the device's poses off
// by 2 cm and 0.4 deg at each instant, its lens 0.6 % off in focal and
// camera_0's principal point 10 px off in y, and the matches exact up to 0.3 px.
// The refinement must come closer to the truth than the device was, keep the
// rig, and find the focal and the principal point.

#include "app/AnchorRefine.h"
#include "sfm/core/Camera.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Pose.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

using sfm::Mat3;
using sfm::Pose;
using sfm::Vec3;

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

constexpr double kPi = 3.14159265358979323846;
constexpr int kInstants = 40, kWidth = 1000, kHeight = 800;
const double kTrueLens[8] = {600, 600, 500, 400, -0.02, 0.003, 0, 0};
constexpr double kCyOff = 10;
constexpr int kLonely = 20;   // an instant left out of every pair

double median(std::vector<double> v) {
    std::nth_element(v.begin(), v.begin() + (std::ptrdiff_t)(v.size() / 2), v.end());
    return v[v.size() / 2];
}

double angle_deg(const Mat3& a, const Mat3& b) {
    const Mat3 d = sfm::mul(sfm::transpose(a), b);
    return std::acos(std::max(-1.0, std::min(1.0, 0.5 * (d[0] + d[4] + d[8] - 1.0)))) * 180 / kPi;
}

// World +Z up, the camera's +Y down: a camera looking along +X at yaw 0.
Mat3 look(double yaw) {
    const Mat3 base{0, -1, 0, 0, 0, -1, 1, 0, 0};
    return sfm::mul(base, sfm::angleAxisToRotation({0, 0, -yaw}));
}

}  // namespace

int main() {
    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0, 1);
    std::uniform_real_distribution<double> unit(0, 1);

    // A room 12 x 8 x 3 m, its walls covered in points.
    std::vector<Vec3> points;
    for (int i = 0; i < 6000; i++) {
        const int wall = i % 5;
        const double u = unit(rng), v = unit(rng);
        Vec3 p = wall == 0 ? Vec3{-4, 8 * u - 4, 3 * v} : wall == 1 ? Vec3{8, 8 * u - 4, 3 * v}
               : wall == 2 ? Vec3{12 * u - 4, -4, 3 * v} : wall == 3 ? Vec3{12 * u - 4, 4, 3 * v}
                           : Vec3{12 * u - 4, 8 * u - 4, 3.0};
        points.push_back(p);
    }

    // Camera 1 sits 6 cm to the right of camera 0 and looks 90 deg to its right.
    const Pose cam1_from_cam0{sfm::angleAxisToRotation({0, kPi / 2, 0}), {-0.06, 0, 0}};
    sfm::Camera lens;
    lens.width = kWidth;
    lens.height = kHeight;
    lens.model = sfm::CamModel::OpenCV;
    sfm::unpackColmap(lens, kTrueLens);

    std::vector<app::lidar::Anchor> anchors;
    std::vector<Pose> truth;
    sfm::MatchesDatabase db;
    std::vector<sfm::FeatureSet> features;
    std::vector<std::vector<int>> seen;   // point index of each keypoint
    for (int t = 0; t < kInstants; t++) {
        const double s = (double)t / (kInstants - 1);
        const Vec3 centre{-2 + 6 * s, 1.5 * std::sin(3 * s), 1.4};
        const Mat3 R = look(1.2 * s);
        const Pose rig{R, sfm::mul(R, centre) * -1.0};
        const Pose off{sfm::mul(sfm::angleAxisToRotation(Vec3{noise(rng), noise(rng), noise(rng)} *
                                                         (0.4 * kPi / 180 / std::sqrt(3.0))),
                                R),
                       {}};
        const Vec3 off_centre = centre + Vec3{noise(rng), noise(rng), noise(rng)} * (0.02 / std::sqrt(3.0));
        const Pose device_rig{off.R, sfm::mul(off.R, off_centre) * -1.0};
        for (int c = 0; c < 2; c++) {
            const Pose p = c == 0 ? rig : sfm::composePose(cam1_from_cam0, rig);
            const Pose d = c == 0 ? device_rig : sfm::composePose(cam1_from_cam0, device_rig);
            char name[64];
            std::snprintf(name, sizeof name, "cap/camera_%d/%06d", c, t);
            app::lidar::Anchor a;
            a.name = std::string(name) + ".jpg";
            a.R = sfm::transpose(d.R);
            a.centre = sfm::cameraCenter(d);
            a.camera_model = 4;
            a.width = kWidth;
            a.height = kHeight;
            a.params.assign(kTrueLens, kTrueLens + 8);
            a.params[0] *= 1.006;
            a.params[1] *= 1.006;
            if (c == 0) a.params[3] += kCyOff;
            anchors.push_back(a);
            truth.push_back(p);
            sfm::FeatureSet f;
            f.width = f.extract_width = kWidth;
            f.height = f.extract_height = kHeight;
            std::vector<int> ids;
            for (size_t i = 0; i < points.size(); i++) {
                const Vec3 q = sfm::mul(p.R, points[i]) + p.t;
                if (q.z < 0.3) continue;
                const sfm::Vec2 uv = lens.project(q);
                if (uv.x < 0 || uv.y < 0 || uv.x >= kWidth || uv.y >= kHeight) continue;
                sfm::Keypoint k;
                k.x = (float)(uv.x + 0.3 * noise(rng));
                k.y = (float)(uv.y + 0.3 * noise(rng));
                k.scale = 2;
                f.keypoints.push_back(k);
                ids.push_back((int)i);
            }
            db.images.push_back({name, (uint32_t)f.keypoints.size()});
            features.push_back(std::move(f));
            seen.push_back(std::move(ids));
        }
    }
    // Pairs as a capture's matcher makes them: neighbours in time and the lenses of one instant.
    const size_t n = db.images.size();
    for (size_t a = 0; a < n; a++)
        for (size_t b = a + 1; b < n && b <= a + 6; b++) {
            if (a / 2 == kLonely || b / 2 == kLonely) continue;
            sfm::TwoViewMatches tv;
            tv.image1 = (uint32_t)a;
            tv.image2 = (uint32_t)b;
            tv.config = 2;
            std::vector<sfm::FeatureMatch> m;
            for (size_t i = 0; i < seen[a].size(); i++) {
                const auto it = std::find(seen[b].begin(), seen[b].end(), seen[a][i]);
                if (it != seen[b].end()) m.push_back({(uint32_t)i, (uint32_t)(it - seen[b].begin())});
            }
            if (m.size() < 15) continue;
            tv.matches = sfm::MatchList(std::move(m));
            db.pairs.push_back(std::move(tv));
        }

    std::vector<double> rot_before, pos_before;
    for (size_t i = 0; i < anchors.size(); i++) {
        rot_before.push_back(angle_deg(anchors[i].R, sfm::transpose(truth[i].R)));
        pos_before.push_back((anchors[i].centre - sfm::cameraCenter(truth[i])).norm());
    }
    const std::vector<app::lidar::Anchor> device = anchors;
    const app::lidar::RefineResult r =
        app::lidar::refine_anchors(anchors, db, features, app::lidar::RefineOptions{});
    std::vector<double> rot_after, pos_after;
    for (size_t i = 0; i < anchors.size(); i++) {
        rot_after.push_back(angle_deg(anchors[i].R, sfm::transpose(truth[i].R)));
        pos_after.push_back((anchors[i].centre - sfm::cameraCenter(truth[i])).norm());
    }
    std::printf("     points %lld  error %.2f px  rotation %.3f -> %.3f deg  centre %.4f -> %.4f m  fx %.2f  cy %.2f\n",
                (long long)r.points, r.error_px, median(rot_before), median(rot_after),
                median(pos_before), median(pos_after), anchors[0].params[0], anchors[0].params[3]);

    expect(r.photos == 2 * kInstants && r.matched == 2 * kInstants && r.moments == kInstants,
           "every photograph is in, one instant per pair of lenses");
    expect(r.points > 1000, "the matches give points under the device's poses");
    expect(r.error_px < 1.0, "the refined model reprojects within 1 px");
    expect(median(rot_after) < 0.5 * median(rot_before), "rotations come closer to the truth");
    expect(median(pos_after) < median(pos_before), "centres come closer to the truth");
    expect(std::fabs(anchors[0].params[0] - kTrueLens[0]) < 0.3 * 0.006 * kTrueLens[0],
           "the focal moves back towards the true one");
    expect(anchors[0].params[0] == anchors[2].params[0], "one lens for each camera folder");
    expect(std::fabs(anchors[0].params[3] - kTrueLens[3]) < 0.2 * kCyOff, "camera_0's principal point comes back");
    bool lonely_kept = true;
    for (int c = 0; c < 2; c++) {
        const app::lidar::Anchor& a = anchors[2 * kLonely + c];
        const app::lidar::Anchor& d = device[2 * kLonely + c];
        lonely_kept = lonely_kept && a.params == d.params && a.R == d.R && (a.centre - d.centre).norm() == 0;
    }
    expect(lonely_kept, "an instant that sees no point keeps the device's poses and lens");
    double rig = 0;
    for (int t = 0; t < kInstants; t++) {
        const Pose p0{sfm::transpose(anchors[2 * t].R), {}};
        const Pose p1{sfm::transpose(anchors[2 * t + 1].R), {}};
        rig = std::max(rig, angle_deg(sfm::mul(p1.R, sfm::transpose(p0.R)), cam1_from_cam0.R));
    }
    expect(rig < 0.2, "the rig stays within 0.2 deg of the truth");

    std::printf("%s (%d failures)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
