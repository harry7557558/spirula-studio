// Synthetic bundle-adjustment problems for the solver tests: cameras on a
// sphere looking at points inside it, optionally as rig frames.
#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "sfm/ba/CpuCamera.h"
#include "sfm/ba/Problem.h"
#include "sfm/core/Pose.h"

namespace synth {

// Camera-frame +z is forward for every model but Snavely's, which projects
// along -z; the generator flips the look-at frame for those two.
inline bool forwardIsMinusZ(uint32_t model) { return model == 0 || model == 1; }

inline const double* defaultIntr(uint32_t model, int& n) {
    static const double snavely[3] = {std::log(600.0), -1e-8, 1e-12};
    static const double snavely_f[3] = {600.0, -1e-8, 1e-12};
    static const double radial[5] = {600.0, -0.02, 0.003, 320.0, 240.0};
    static const double opencv[8] = {600.0, 605.0, -0.02, 0.003, 1e-4, -2e-4, 320.0, 240.0};
    static const double simple[3] = {600.0, 320.0, 240.0};
    static const double pinhole[4] = {600.0, 605.0, 320.0, 240.0};
    static const double fisheye[8] = {300.0, 305.0, 0.01, -0.002, 3e-4, -1e-5, 320.0, 240.0};
    static const double full[12] = {600.0, 605.0, -0.02, 0.003, 1e-4,  -2e-4,
                                    1e-4,  5e-3,  -8e-4, 2e-5,  320.0, 240.0};
    static const double prism[12] = {300.0, 305.0, 0.01,  -0.002, 3e-4,  -1e-5,
                                     2e-4,  -1e-6, 1e-4,  -2e-4,  320.0, 240.0};
    static const double equirect[2] = {640.0, 480.0};
    switch (model) {
        case 0: n = 3; return snavely;
        case 1: n = 3; return snavely_f;
        case 2: n = 5; return radial;
        case 3: n = 8; return opencv;
        case 4: n = 3; return simple;
        case 5: n = 4; return pinhole;
        case 6: n = 8; return fisheye;
        case 7: n = 12; return full;
        case 8: n = 12; return prism;
        default: n = 2; return equirect;
    }
}

inline void project(uint32_t model, const double* intr, const double p[3], double out[2]) {
    bacpu::withModel(model, [&](auto M) { decltype(M)::template project<double>(intr, p, out); });
}

// nImg cameras on a sphere looking at the origin, nPt points inside it.
// `groups` is 1 (one shared camera) or nImg (one per image); `rig` > 1 makes
// each camera a frame of that many members, refined or held by `rig_free`.
inline BAProblem makeProblem(uint32_t model, uint32_t nImg, uint32_t nPt, uint32_t groups,
                      double noise, uint32_t seed, int nfree = -1, uint32_t rig = 0,
                      bool rig_free = true) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss;
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    int ni = 0;
    const double* base = defaultIntr(model, ni);
    const bool minusZ = forwardIsMinusZ(model);

    BAProblem P;
    const uint32_t nFrames = nImg;
    const uint32_t members = std::max(1u, rig);
    nImg = nFrames * members;
    P.num_images = nImg;
    P.num_frames = nFrames;
    P.poses.resize(6 * (size_t)nFrames);
    P.image_frame.resize(nImg);
    P.image_member.assign(nImg, kNoMember);
    std::vector<sfm::Pose> ext(members, sfm::Pose{sfm::mat3Identity(), {0, 0, 0}});
    for (uint32_t m = 1; m < members; m++) {
        ext[m].R = sfm::angleAxisToRotation({0.3 * unit(rng), 0.3 * unit(rng), 0.3 * unit(rng)});
        ext[m].t = {0.2 * unit(rng), 0.2 * unit(rng), 0.2 * unit(rng)};
    }
    if (rig > 1) {
        P.exts.resize(6 * (size_t)members);
        for (uint32_t m = 0; m < members; m++) {
            const sfm::Vec3 aa = sfm::rotationToAngleAxis(ext[m].R);
            P.exts[6 * (size_t)m + 0] = aa.x;
            P.exts[6 * (size_t)m + 1] = aa.y;
            P.exts[6 * (size_t)m + 2] = aa.z;
            P.exts[6 * (size_t)m + 3] = ext[m].t.x;
            P.exts[6 * (size_t)m + 4] = ext[m].t.y;
            P.exts[6 * (size_t)m + 5] = ext[m].t.z;
            P.members.push_back({6 * m, 0, m > 0 && rig_free ? 6u : 0u});
        }
    }
    std::vector<double> centers(3 * (size_t)nImg), rot(9 * (size_t)nImg);
    for (uint32_t f = 0; f < nFrames; f++) {
        const double th = 2.0 * M_PI * f / nFrames + 0.03 * unit(rng);
        const double ph = 0.4 * unit(rng);
        const sfm::Vec3 c{4.0 * std::cos(th) * std::cos(ph), 4.0 * std::sin(ph),
                          4.0 * std::sin(th) * std::cos(ph)};
        const sfm::Vec3 fw = (sfm::Vec3{0, 0, 0} - c).normalized();
        const sfm::Vec3 rt = sfm::Vec3{0, 1, 0}.cross(fw).normalized();
        const sfm::Vec3 u2 = fw.cross(rt);
        // -z forward for Snavely; flipping two rows keeps det = +1
        const double s = minusZ ? -1.0 : 1.0;
        const sfm::Mat3 R{rt.x,     rt.y,     rt.z,     s * u2.x, s * u2.y,
                          s * u2.z, s * fw.x, s * fw.y, s * fw.z};
        const sfm::Pose frame{R, sfm::mul(R, c) * -1.0};
        const sfm::Vec3 aa = sfm::rotationToAngleAxis(R);
        P.poses[6 * (size_t)f + 0] = aa.x;
        P.poses[6 * (size_t)f + 1] = aa.y;
        P.poses[6 * (size_t)f + 2] = aa.z;
        P.poses[6 * (size_t)f + 3] = frame.t.x;
        P.poses[6 * (size_t)f + 4] = frame.t.y;
        P.poses[6 * (size_t)f + 5] = frame.t.z;
        for (uint32_t m = 0; m < members; m++) {
            const uint32_t i = f * members + m;
            P.image_frame[i] = f;
            if (rig > 1) P.image_member[i] = m;
            const sfm::Pose cam = sfm::composePose(ext[m], frame);
            const sfm::Vec3 cc = sfm::cameraCenter(cam);
            for (int r = 0; r < 3; r++)
                for (int k = 0; k < 3; k++) rot[9 * (size_t)i + 3 * r + k] = cam.R[3 * r + k];
            centers[3 * (size_t)i + 0] = cc.x;
            centers[3 * (size_t)i + 1] = cc.y;
            centers[3 * (size_t)i + 2] = cc.z;
        }
    }

    P.groups.resize(groups);
    P.image_group.resize(nImg);
    P.intr.resize((size_t)ni * groups);
    for (uint32_t g = 0; g < groups; g++) {
        for (int j = 0; j < ni; j++) P.intr[(size_t)ni * g + j] = base[j] * (1.0 + 0.005 * unit(rng));
        // EQUIRECTANGULAR's two parameters are the image size and never refine
        const uint32_t nf = model == 9 ? 0u : (uint32_t)(nfree >= 0 ? std::min(nfree, ni) : ni);
        P.groups[g] = {(uint32_t)(ni * g), 0, nf, model};
    }
    for (uint32_t i = 0; i < nImg; i++) P.image_group[i] = groups == 1 ? 0 : i;

    P.num_points = nPt;
    P.points.resize(3 * (size_t)nPt);
    for (uint32_t p = 0; p < nPt; p++)
        for (int k = 0; k < 3; k++) P.points[3 * (size_t)p + k] = 1.2 * unit(rng);

    P.obs_ranges.assign(nPt + 1, 0);
    for (uint32_t p = 0; p < nPt; p++) {
        for (uint32_t i = 0; i < nImg; i++) {
            double d[3] = {P.points[3 * (size_t)p] - centers[3 * (size_t)i],
                           P.points[3 * (size_t)p + 1] - centers[3 * (size_t)i + 1],
                           P.points[3 * (size_t)p + 2] - centers[3 * (size_t)i + 2]};
            double pc[3];
            for (int r = 0; r < 3; r++)
                pc[r] = rot[9 * (size_t)i + 3 * r] * d[0] + rot[9 * (size_t)i + 3 * r + 1] * d[1] +
                        rot[9 * (size_t)i + 3 * r + 2] * d[2];
            if (minusZ) pc[2] = -std::fabs(pc[2]);
            double px[2];
            project(model, &P.intr[(size_t)ni * (groups == 1 ? 0 : i)], pc, px);
            if (!std::isfinite(px[0]) || !std::isfinite(px[1])) continue;
            if (std::fabs(px[0]) > 4000 || std::fabs(px[1]) > 4000) continue;
            P.obs_image.push_back(i);
            P.obs_point.push_back(p);
            P.obs_xy.push_back(px[0] + noise * gauss(rng));
            P.obs_xy.push_back(px[1] + noise * gauss(rng));
        }
        P.obs_ranges[p + 1] = (uint32_t)P.obs_image.size();
    }
    P.num_obs = (uint32_t)P.obs_image.size();

    P.pose_dim = 6 * nFrames;
    P.ext_dim = 0;
    for (BAProblem::Member& m : P.members) {
        m.ext_col = P.pose_dim + P.ext_dim;
        P.ext_dim += m.n_free;
    }
    P.total_intr = (uint32_t)P.intr.size();
    P.free_intr = 0;
    for (BAProblem::Group& g : P.groups) {
        g.intr_col = P.pose_dim + P.ext_dim + P.free_intr;
        P.free_intr += g.n_intr;
    }
    P.n_dim = P.pose_dim + P.ext_dim + P.free_intr;
    finalizeTables(P);

    // perturb, so the solver has something to do
    for (uint32_t i = 0; i < 6 * nFrames; i++) P.poses[i] += 0.004 * gauss(rng);
    for (uint32_t i = 0; i < P.exts.size(); i++) P.exts[i] += 0.003 * gauss(rng);
    for (uint32_t p = 0; p < 3 * nPt; p++) P.points[p] += 0.01 * gauss(rng);
    return P;
}

}  // namespace synth
