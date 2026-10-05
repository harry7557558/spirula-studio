// A synthetic two-lens rig for the mapper tests: frames on an arc round a
// cloud of points, cam0 looking at the origin, cam1 turned 22 deg and offset;
// cam1 of the last frames sees nothing (a lens on the sky), so only the rig
// can place them. `weak` frames keep 12 observations per lens, under the 15 a
// lone PnP needs: only a whole-frame registration places them.
#pragma once

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Pose.h"

namespace synth_rig {

inline sfm::Pose lookAt(const sfm::Vec3& C, const sfm::Vec3& target) {
    using namespace sfm;
    Vec3 f = (target - C).normalized();
    Vec3 up0 = {0, 1, 0};
    Vec3 r = up0.cross(f).normalized();
    Vec3 u = f.cross(r);
    Mat3 R = {r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z};
    Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

struct RigScene {
    int W = 1280, H = 960, M = 10, N = 260, blind_from = 7;
    std::vector<char> weak;            // by frame
    sfm::Pose ext;                     // cam1_from_cam0, the truth
    std::vector<sfm::Pose> gt;         // by image id: cam0 = f, cam1 = M + f
    std::vector<sfm::Vec3> pts;
    std::vector<sfm::FeatureSet> feats;
    sfm::MatchesDatabase db;
    std::vector<uint32_t> cam_ids;
    std::vector<std::string> names;
};

inline RigScene makeRigScene(int frames = 10, int blind_from = 7,
                             std::vector<char> weak = {}) {
    using namespace sfm;
    RigScene sc;
    sc.M = frames;
    sc.blind_from = blind_from;
    sc.weak = weak.empty() ? std::vector<char>(frames, 0) : weak;
    Camera K = Camera::defaultFor(1, sc.W, sc.H, 1200);
    std::mt19937 rng(23);
    std::uniform_real_distribution<double> ub(-4.0, 4.0);
    std::normal_distribution<double> noise(0.0, 0.3);
    sc.pts.resize(sc.N);
    for (Vec3& p : sc.pts) p = {ub(rng), ub(rng), ub(rng)};
    sc.ext.R = angleAxisToRotation({0.05, 22.0 * M_PI / 180.0, -0.03});
    sc.ext.t = {0.3, 0.05, -0.1};
    const int n = 2 * sc.M;
    sc.gt.resize(n);
    sc.feats.resize(n);
    sc.cam_ids.resize(n);
    sc.names.resize(n);
    std::vector<std::vector<char>> vis(n, std::vector<char>(sc.N, 0));
    for (int f = 0; f < sc.M; f++) {
        const double ang = -1.2 + 2.4 * f / (sc.M - 1);
        sc.gt[f] = lookAt({9 * std::sin(ang), 1.5 * std::sin(0.7 * f), 9 * std::cos(ang)},
                          {0, 0, 0});
        sc.gt[sc.M + f] = composePose(sc.ext, sc.gt[f]);
    }
    for (int i = 0; i < n; i++) {
        const bool blind = i >= sc.M && i - sc.M >= sc.blind_from;
        char nm[32];
        snprintf(nm, sizeof nm, "%s/%03d", i < sc.M ? "cam0" : "cam1", i < sc.M ? i : i - sc.M);
        sc.names[i] = nm;
        sc.cam_ids[i] = i < sc.M ? 1 : 2;
        FeatureSet& fs = sc.feats[i];
        fs.width = sc.W;
        fs.height = sc.H;
        fs.keypoints.resize(sc.N);
        const bool weak_lens = sc.weak[i % sc.M];
        int kept = 0;
        for (int p = 0; p < sc.N; p++) {
            Vec3 pc = mul(sc.gt[i].R, sc.pts[p]) + sc.gt[i].t;
            Vec2 px = K.project(pc);
            bool see = !blind && pc.z > 0.1 && px.x > 0 && px.x < sc.W && px.y > 0 && px.y < sc.H;
            if (see && weak_lens) see = p % 2 == (i < sc.M ? 0 : 1) && kept++ < 12;
            if (see) {
                fs.keypoints[p] = {(float)(px.x + noise(rng)), (float)(px.y + noise(rng)), 2, 0, 0};
                vis[i][p] = 1;
            } else {
                fs.keypoints[p] = {-1000, -1000, 2, 0, 0};
            }
        }
    }
    sc.db.images.resize(n);
    for (int i = 0; i < n; i++) sc.db.images[i] = {sc.names[i], (uint32_t)sc.N};
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            for (int p = 0; p < sc.N; p++)
                if (vis[i][p] && vis[j][p]) tv.matches.push_back({(uint32_t)p, (uint32_t)p, 0});
            const bool weak_pair = sc.weak[i % sc.M] || sc.weak[j % sc.M];
            if (tv.matches.size() >= (weak_pair ? 6u : 15u)) sc.db.pairs.push_back(std::move(tv));
        }
    return sc;
}

}  // namespace synth_rig
