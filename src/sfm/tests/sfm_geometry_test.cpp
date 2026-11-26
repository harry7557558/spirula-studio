// Two-view geometry and PnP: F, H, E, P3P, EPnP, triangulation, RANSAC (host only).
//
// Prints PASS/FAIL and returns 0/1. See docs/testing.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/feature/Verification.h"
#include "sfm/geometry/AbsolutePose.h"
#include "sfm/geometry/EPnP.h"
#include "sfm/geometry/TwoView.h"
#include "sfm/tests/TestMain.h"

namespace fs = std::filesystem;
using namespace sfm;

// -----------------------------------------------------------------------
// geom-selftest: synthetic two-view geometry (host only, no GPU)
// -----------------------------------------------------------------------
static Mat3 rotationY(double deg) {
    double a = deg * M_PI / 180.0, c = std::cos(a), s = std::sin(a);
    return {c, 0, s, 0, 1, 0, -s, 0, c};
}
static double rotationErrorDeg(const Mat3& A, const Mat3& B) {
    Mat3 D = mul(A, transpose(B));
    double tr = D[0] + D[4] + D[8];
    return std::acos(std::max(-1.0, std::min(1.0, (tr - 1.0) / 2.0))) * 180.0 / M_PI;
}
static double vecAngleDeg(const Vec3& a, const Vec3& b) {
    double d = a.normalized().dot(b.normalized());
    return std::acos(std::min(1.0, std::fabs(d))) * 180.0 / M_PI;  // sign-agnostic
}

int cmdGeomSelftest(int, char**) {
    const double f = 1200, cx = 640, cy = 480;
    Mat3 K = {f, 0, cx, 0, f, cy, 0, 0, 1};
    Mat3 Rgt = rotationY(15.0);
    Vec3 tgt = Vec3{1.0, 0.2, 0.3}.normalized();

    std::mt19937 rng(7);
    std::uniform_real_distribution<double> ux(-2, 2), uz(4, 9), upix(0, 1280);
    std::normal_distribution<double> noise(0.0, 0.5);

    auto project = [&](const Mat3& R, const Vec3& t, const Vec3& X) {
        Vec3 xc = mul(R, X) + t;
        Vec3 px = mul(K, Vec3{xc.x / xc.z, xc.y / xc.z, 1});
        return Vec2{px.x, px.y};
    };

    int fails = 0;

    // ---- angle-axis round trip, up to and at 180 degrees ----
    {
        double worst = 0;
        std::uniform_real_distribution<double> u(-1, 1), ue(1, 16);
        for (int k = 0; k < 20000; k++) {
            const Vec3 n = Vec3{u(rng), u(rng), u(rng)}.normalized();
            const double th = k % 2 ? M_PI - std::pow(10.0, -ue(rng)) : M_PI * 0.5 * (1 + u(rng));
            const Mat3 R = angleAxisToRotation(n * th);
            const Mat3 R2 = angleAxisToRotation(rotationToAngleAxis(R));
            for (int i = 0; i < 9; i++) worst = std::max(worst, std::fabs(R[i] - R2[i]));
        }
        printf("angle-axis round trip near pi: %.2e\n", worst);
        if (worst > 1e-12) { printf("  FAIL: angle-axis round trip\n"); fails++; }
    }

    // ---- non-planar scene with noise + outliers ----
    {
        std::vector<Vec2> p1, p2;
        std::vector<char> truth;  // 1 = true inlier
        int N = 300;
        for (int i = 0; i < N; i++) {
            Vec3 X = {ux(rng), ux(rng), uz(rng)};
            Vec3 xc2 = mul(Rgt, X) + tgt;
            if (X.z <= 0.1 || xc2.z <= 0.1) continue;
            Vec2 a = project(mat3Identity(), {0, 0, 0}, X);
            Vec2 b = project(Rgt, tgt, X);
            a.x += noise(rng); a.y += noise(rng);
            b.x += noise(rng); b.y += noise(rng);
            if (a.x < 0 || a.x > 1280 || b.x < 0 || b.x > 1280) continue;
            p1.push_back(a); p2.push_back(b); truth.push_back(1);
        }
        // 30% outliers: random p2
        int nOut = (int)(0.3 * p1.size());
        for (int i = 0; i < nOut; i++) {
            p1.push_back({upix(rng), upix(rng)});
            p2.push_back({upix(rng), upix(rng)});
            truth.push_back(0);
        }

        TwoViewOptions opt;
        opt.recover_pose = true;
        opt.K1 = opt.K2 = K;
        TwoViewGeometry g = estimateTwoView(p1, p2, opt);

        int tp = 0, fp = 0, trueInl = 0;
        for (size_t i = 0; i < p1.size(); i++) {
            if (truth[i]) trueInl++;
            if (i < g.inlier_mask.size() && g.inlier_mask[i]) {
                if (truth[i]) tp++; else fp++;
            }
        }
        double recall = trueInl ? (double)tp / trueInl : 0;
        double prec = (tp + fp) ? (double)tp / (tp + fp) : 0;
        double rErr = rotationErrorDeg(g.pose.R, Rgt);
        double tErr = vecAngleDeg(g.pose.t, tgt);
        printf("geom: two-view %s, %d inliers, recall %.2f prec %.2f, R err %.2f deg, "
               "t err %.2f deg\n", twoViewConfigName(g.config), g.num_inliers, recall, prec,
               rErr, tErr);
        bool ok = g.config == TwoViewConfig::Uncalibrated && recall > 0.8 && prec > 0.9 &&
                  rErr < 2.0 && tErr < 5.0;
        if (!ok) { printf("  FAIL: two-view estimation\n"); fails++; }
    }

    // ---- planar scene: homography should win ----
    {
        std::vector<Vec2> p1, p2;
        for (int i = 0; i < 200; i++) {
            double X = ux(rng), Y = ux(rng), Z = 6.0 + 0.3 * X - 0.2 * Y;  // a plane
            Vec3 P = {X, Y, Z};
            Vec3 xc2 = mul(Rgt, P) + tgt;
            if (xc2.z <= 0.1) continue;
            Vec2 a = project(mat3Identity(), {0, 0, 0}, P);
            Vec2 b = project(Rgt, tgt, P);
            a.x += noise(rng); a.y += noise(rng);
            b.x += noise(rng); b.y += noise(rng);
            p1.push_back(a); p2.push_back(b);
        }
        TwoViewGeometry g = estimateTwoView(p1, p2, TwoViewOptions{});
        printf("geom: planar scene -> %s (%d inliers)\n", twoViewConfigName(g.config),
               g.num_inliers);
        if (g.config != TwoViewConfig::PlanarOrPanoramic) {
            printf("  FAIL: planar scene not detected as homography\n");
            fails++;
        }
    }

    // ---- PnP with distant points / small baseline (mapper-like) ----
    {
        Camera pc;
        pc.setFocal(777.6); pc.cx = 324; pc.cy = 210;
        Mat3 Rp = rotationY(8.0);
        Vec3 tp = {0.7, -0.2, 0.1};
        std::mt19937 r2(3);
        std::uniform_real_distribution<double> updist(50, 600), depth(12, 35);
        std::vector<Vec3> Xs;
        std::vector<Vec3> brs;  // observed unit bearings
        for (int i = 0; i < 80; i++) {
            // back-project a random pixel at random depth in the reference frame
            double u = updist(r2), v = updist(r2), d = depth(r2);
            Vec3 X = {(u - 324) / 777.6 * d, (v - 210) / 777.6 * d, d};
            Vec3 pc3 = mul(Rp, X) + tp;
            if (pc3.z < 0.1) continue;
            Vec2 px = {777.6 * pc3.x / pc3.z + 324 + noise(r2),
                       777.6 * pc3.y / pc3.z + 210 + noise(r2)};
            Xs.push_back(X);
            brs.push_back(pc.bearing(px));
        }
        PnPResult pr = ransacPnP(Xs, brs, 777.6, 4.0);
        double rErr = rotationErrorDeg(pr.pose.R, Rp);
        double tErr = (pr.pose.t - tp).norm();
        printf("geom: PnP (distant pts) %d/%zu inliers, R err %.2f deg, t err %.3f\n",
               pr.num_inliers, Xs.size(), rErr, tErr);
        if (pr.num_inliers < (int)Xs.size() * 0.8 || rErr > 1.0 || tErr > 0.1) {
            printf("  FAIL: PnP\n");
            fails++;
        }

        // ---- nonlinear pose refinement (D36) ----
        // Perturb the ground-truth pose and refine on all correspondences: the
        // LM polish must recover it more closely than the perturbation.
        {
            Pose p0;
            p0.R = mul(rotationY(1.5), Rp);
            p0.t = {tp.x + 0.05, tp.y - 0.04, tp.z + 0.03};
            std::vector<char> all(Xs.size(), 1);
            bool ok = refinePose(Xs, brs, all, p0);
            double rr = rotationErrorDeg(p0.R, Rp);
            double tt = (p0.t - tp).norm();
            printf("geom: refinePose 1.50 deg / 0.071 -> %.3f deg / %.4f\n", rr, tt);
            // 0.4 px observation noise on a short baseline bounds how well the
            // translation can be recovered; beating both the perturbation and
            // the unrefined RANSAC pose (0.026) is the requirement.
            if (!ok || rr > 0.1 || tt > 0.02) { printf("  FAIL: refinePose\n"); fails++; }
        }

        // ---- joint pose+focal refinement (D36) ----
        // The same observations interpreted with a 25% wrong focal: the joint
        // refinement must recover the true focal as a scale on the guess.
        {
            Camera wrong = pc;
            wrong.setFocal(777.6 * 1.25);
            std::vector<Vec3> brw(Xs.size());
            for (size_t i = 0; i < Xs.size(); i++) {
                // re-derive the pixel from the true bearing, unproject with the
                // wrong camera
                Vec2 px = pc.project(brs[i]);
                brw[i] = wrong.bearing(px);
            }
            Pose p0;
            p0.R = Rp;
            p0.t = tp;
            std::vector<char> all(Xs.size(), 1);
            double fs = 1.0;
            bool ok = refinePose(Xs, brw, all, p0, &fs);
            double frel = wrong.focal() * fs / 777.6 - 1.0;
            printf("geom: refinePose+focal: 25%% focal error -> %+.2f%%\n", 100 * frel);
            if (!ok || std::fabs(frel) > 0.02) { printf("  FAIL: refinePose focal\n"); fails++; }
        }
    }

    // ---- generalized PnP: a rig posed as one camera (rig constraints) ----
    //
    // Rays that miss a common centre, rays that share one, and the case the
    // mapper needs: ten views on two lenses, none able to pose itself.
    {
        std::mt19937 rg(23);
        std::uniform_real_distribution<double> ur(-1.0, 1.0), z01(0.0, 1.0);
        const double focal = 750.0, err_px = 4.0, err_rad = err_px / focal;

        int missed = 0;
        double worst = 0;
        for (int t = 0; t < 400; t++) {
            Pose F;
            F.R = angleAxisToRotation({ur(rg) * 2, ur(rg) * 2, ur(rg) * 2});
            F.t = {ur(rg) * 5, ur(rg) * 5, ur(rg) * 5};
            std::array<RigRay, 3> rays;
            std::array<Vec3, 3> Xs;
            const Vec3 anchor{ur(rg) * 3, ur(rg) * 3, 4.0 + 12.0 * z01(rg)};
            for (int k = 0; k < 3; k++) {
                const Vec3 in_rig = anchor + Vec3{ur(rg), ur(rg), ur(rg)};
                rays[k].o = {ur(rg) * 0.3, ur(rg) * 0.3, ur(rg) * 0.3};
                rays[k].d = (in_rig - rays[k].o).normalized();
                Xs[k] = mul(transpose(F.R), in_rig - F.t);
            }
            double best = 1e9;
            for (const Pose& p : gp3p(rays, Xs))
                best = std::min(best, rotationErrorDeg(p.R, F.R) + (p.t - F.t).norm());
            if (best > 1e-3) missed++;
            else worst = std::max(worst, best);
        }
        printf("geom: gp3p on 400 non-central samples: %d missed, worst %.2e\n", missed, worst);
        if (missed > 8) { printf("  FAIL: gp3p\n"); fails++; }

        // Concurrent rays: the same three bearings through one centre, which
        // has no baseline to solve with and must come back as P3P does.
        {
            Pose F;
            F.R = rotationY(23.0);
            F.t = {0.4, -0.3, 0.2};
            const Vec3 centre{0.8, -0.1, 0.05};
            std::array<RigRay, 3> rays;
            std::array<Vec3, 3> Xs;
            std::array<Vec3, 3> bear;
            const Vec3 pts[3] = {{1.0, -0.5, 7.0}, {2.1, 0.9, 8.4}, {-0.6, 1.4, 6.2}};
            for (int k = 0; k < 3; k++) {
                const Vec3 in_rig = pts[k];
                rays[k].o = centre;
                rays[k].d = (in_rig - centre).normalized();
                bear[k] = rays[k].d;
                Xs[k] = mul(transpose(F.R), in_rig - F.t);
            }
            double g_best = 1e9, p_best = 1e9;
            for (const Pose& p : gp3p(rays, Xs))
                g_best = std::min(g_best, rotationErrorDeg(p.R, F.R) + (p.t - F.t).norm());
            // P3P sees the same rays from the origin, so its pose is the
            // rig's shifted by the centre they share.
            for (const Pose& p : p3p(bear, Xs))
                p_best = std::min(p_best,
                                  rotationErrorDeg(p.R, F.R) + (p.t + centre - F.t).norm());
            printf("geom: gp3p on concurrent rays: %.2e (P3P %.2e)\n", g_best, p_best);
            if (g_best > 1e-6 || p_best > 1e-4) {  // 1e-4: lambdatwist's own floor
                printf("  FAIL: gp3p concurrent\n");
                fails++;
            }
        }

        // Ten views, two optical centres, six correspondences each and 60%
        // of them wrong: no lens can pose itself, the rig can.
        {
            const int kLenses = 2, kViews = 5, kCorr = 6;
            const double baseline = 0.03, outlier = 0.6;
            int recovered = 0, runs = 60;
            for (int r = 0; r < runs; r++) {
                Pose F;
                F.R = angleAxisToRotation({ur(rg) * 2, ur(rg) * 2, ur(rg) * 2});
                F.t = {ur(rg) * 5, ur(rg) * 5, ur(rg) * 5};
                std::vector<Pose> cam_from_rig;
                for (int L = 0; L < kLenses; L++) {
                    const Vec3 centre{L * baseline, 0, 0};
                    for (int v = 0; v < kViews; v++) {
                        Pose e;
                        e.R = angleAxisToRotation(
                            {0.3 * ur(rg), 2.0 * M_PI * v / kViews + (L ? M_PI : 0.0), 0.2 * ur(rg)});
                        e.t = mul(e.R, centre) * -1.0;
                        cam_from_rig.push_back(e);
                    }
                }
                const size_t M = cam_from_rig.size();
                std::vector<std::vector<Vec3>> X(M), b(M);
                for (size_t m = 0; m < M; m++) {
                    const Pose cam = composePose(cam_from_rig[m], F);
                    for (int k = 0; k < kCorr; k++) {
                        const Vec3 pc{ur(rg) * 3, ur(rg) * 3, 3.0 + 17.0 * z01(rg)};
                        const Vec3 unit = pc.normalized();
                        b[m].push_back(Vec3{unit.x / unit.z + noise(rg) / focal,
                                            unit.y / unit.z + noise(rg) / focal, 1.0}
                                           .normalized());
                        Vec3 world = mul(transpose(cam.R), pc - cam.t);
                        if (z01(rg) < outlier)
                            world = world + Vec3{ur(rg) * 8, ur(rg) * 8, ur(rg) * 8};
                        X[m].push_back(world);
                    }
                }
                std::vector<RigPnPMember> gm;
                for (size_t m = 0; m < M; m++) gm.push_back({&X[m], &b[m], cam_from_rig[m], err_rad});
                const RigPnPResult rr = ransacRigPnP(gm);
                if (rr.success && rotationErrorDeg(rr.rig_from_world.R, F.R) < 1.0 &&
                    (rr.rig_from_world.t - F.t).norm() < 0.2)
                    recovered++;
            }
            printf("geom: rig PnP, 10 views x 6 corr at 60%% outliers: %d/%d recovered\n",
                   recovered, runs);
            if (recovered < runs - 3) { printf("  FAIL: rig PnP\n"); fails++; }
        }
    }

    // ---- fisheye two-view: bearings vs pixels (D45) ----
    //
    // The claim D45 rests on: on a wide lens, verification on raw pixels loses
    // the correspondences that carry the field of view, because no fundamental
    // matrix can explain a ray 100 deg off axis. Same scene, same matches,
    // same RANSAC -- only the coordinate system differs.
    {
        Camera fc;
        fc.model = CamModel::ThinPrismFisheye;
        fc.width = fc.height = 1920;
        fc.setFocal(520);
        fc.cx = 960; fc.cy = 960;
        fc.k1 = 0.023; fc.k2 = 0.016; fc.k3 = -0.006;   // Metashape's, for this lens

        Mat3 Rf = rotationY(12.0);
        Vec3 tf = {0.9, 0.05, 0.15};
        std::mt19937 fr(11);
        std::uniform_real_distribution<double> sph(-1, 1), rad(3.0, 9.0), upx(0, 1920);
        std::normal_distribution<double> fnoise(0.0, 0.5);

        std::vector<Vec2> p1, p2;
        std::vector<Vec3> b1, b2;
        std::vector<char> truth;
        std::vector<double> theta;
        while (p1.size() < 250) {
            // Points all around the first camera, out to 100 deg off axis --
            // the working range of a 200 deg lens, and exactly what a pinhole
            // model cannot represent.
            Vec3 d = {sph(fr), sph(fr), sph(fr)};
            if (d.norm() < 1e-3) continue;
            d = d.normalized();
            double th = std::acos(std::max(-1.0, std::min(1.0, d.z)));
            if (th > 100.0 * M_PI / 180.0) continue;
            Vec3 X = d * rad(fr);
            Vec3 xc2 = mul(Rf, X) + tf;
            if (std::acos(std::max(-1.0, std::min(1.0, xc2.normalized().z))) >
                100.0 * M_PI / 180.0)
                continue;
            Vec2 a = fc.project(X), b = fc.project(xc2);
            a.x += fnoise(fr); a.y += fnoise(fr);
            b.x += fnoise(fr); b.y += fnoise(fr);
            if (a.x < 0 || a.x > 1920 || a.y < 0 || a.y > 1920) continue;
            if (b.x < 0 || b.x > 1920 || b.y < 0 || b.y > 1920) continue;
            p1.push_back(a); p2.push_back(b); truth.push_back(1);
            theta.push_back(th * 180.0 / M_PI);
        }
        size_t nTrue = p1.size();
        for (size_t i = 0; i < nTrue / 3; i++) {  // 25% outliers
            p1.push_back({upx(fr), upx(fr)});
            p2.push_back({upx(fr), upx(fr)});
            truth.push_back(0);
            theta.push_back(0);
        }
        for (size_t i = 0; i < p1.size(); i++) {
            b1.push_back(fc.bearing(p1[i]));
            b2.push_back(fc.bearing(p2[i]));
        }

        auto recallOf = [&](const TwoViewGeometry& g, bool wide_only) {
            int tp = 0, n = 0;
            for (size_t i = 0; i < p1.size(); i++) {
                if (!truth[i]) continue;
                if (wide_only && theta[i] < 60.0) continue;
                n++;
                if (i < g.inlier_mask.size() && g.inlier_mask[i]) tp++;
            }
            return n ? (double)tp / n : 0.0;
        };

        TwoViewOptions px_opt;
        TwoViewGeometry gp = estimateTwoView(p1, p2, px_opt);

        TwoViewOptions br_opt;
        br_opt.recover_pose = true;
        br_opt.ransac.max_error /= 520.0;  // px -> rad
        TwoViewGeometry gb = estimateTwoViewBearing(b1, b2, br_opt);

        double rErr = rotationErrorDeg(gb.pose.R, Rf);
        double tErr = vecAngleDeg(gb.pose.t, tf);
        printf("geom: fisheye two-view, %zu true corr to 100 deg: pixels recall %.2f "
               "(beyond 60 deg %.2f), bearings recall %.2f (beyond 60 deg %.2f)\n",
               nTrue, recallOf(gp, false), recallOf(gp, true), recallOf(gb, false),
               recallOf(gb, true));
        printf("geom: fisheye two-view pose from bearings: R err %.2f deg, t err %.2f deg\n",
               rErr, tErr);
        bool ok = gb.config == TwoViewConfig::Uncalibrated && recallOf(gb, false) > 0.9 &&
                  recallOf(gb, true) > 0.9 && rErr < 1.0 && tErr < 3.0 &&
                  recallOf(gb, true) > 2.0 * recallOf(gp, true);
        if (!ok) { printf("  FAIL: fisheye two-view on bearings\n"); fails++; }

        // A planar fisheye scene must still be recognised as a homography:
        // the ray-to-ray H and its angular transfer error are the bearing
        // path's half of the model selection, and without them every planar
        // pair would be handed to the mapper as a usable epipolar geometry.
        {
            std::vector<Vec3> h1, h2;
            std::mt19937 pr(19);
            std::uniform_real_distribution<double> pu(-4, 4);
            while (h1.size() < 200) {
                double X = pu(pr), Y = pu(pr);
                Vec3 P = {X, Y, 6.0 + 0.3 * X - 0.2 * Y};   // a plane
                Vec3 xc2 = mul(Rf, P) + tf;
                Vec2 a = fc.project(P), b = fc.project(xc2);
                if (a.x < 0 || a.x > 1920 || a.y < 0 || a.y > 1920) continue;
                if (b.x < 0 || b.x > 1920 || b.y < 0 || b.y > 1920) continue;
                a.x += fnoise(pr); a.y += fnoise(pr);
                b.x += fnoise(pr); b.y += fnoise(pr);
                h1.push_back(fc.bearing(a));
                h2.push_back(fc.bearing(b));
            }
            TwoViewOptions ho;
            ho.ransac.max_error /= 520.0;
            TwoViewGeometry gh = estimateTwoViewBearing(h1, h2, ho);
            printf("geom: planar fisheye scene on bearings -> %s (%d inliers)\n",
                   twoViewConfigName(gh.config), gh.num_inliers);
            if (gh.config != TwoViewConfig::PlanarOrPanoramic) {
                printf("  FAIL: planar fisheye scene not detected as a homography\n");
                fails++;
            }
        }

        // The focal the bootstrap would pick, on this one synthetic pair. The
        // search maximizes peripheral inliers over a FOV grid; here the truth
        // is known, so we can check it lands on it.
        {
            FeatureSet fs1, fs2;
            fs1.width = fs2.width = 1920;
            fs1.height = fs2.height = 1920;
            std::vector<FeatureMatch> mm;
            for (size_t i = 0; i < p1.size(); i++) {
                fs1.keypoints.push_back({(float)p1[i].x, (float)p1[i].y, 1, 0, 0});
                fs2.keypoints.push_back({(float)p2[i].x, (float)p2[i].y, 1, 0, 0});
                mm.push_back({(uint32_t)i, (uint32_t)i});
            }
            std::vector<FeatureSet> feats = {fs1, fs2};
            Camera guess = Camera::defaultFor(1, 1920, 1920, 0, CamModel::ThinPrismFisheye);
            std::vector<Camera> cams = {guess, guess};
            double fb = bootstrapFocal(feats, {{0, 1}}, {mm}, cams, TwoViewOptions{}, 1, false);
            double rel = 100.0 * (fb - 520.0) / 520.0;
            printf("geom: focal bootstrap from one pair: %.0f px (truth 520, %+.1f%%; "
                   "the geometric default would be %.0f)\n", fb, rel, guess.focal());
            if (std::fabs(rel) > 12.0) { printf("  FAIL: focal bootstrap\n"); fails++; }
        }
    }

    // ---- triangulation round-trip ----
    {
        Mat34 P1 = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        Mat34 P2 = {Rgt[0], Rgt[1], Rgt[2], tgt.x, Rgt[3], Rgt[4], Rgt[5], tgt.y,
                    Rgt[6], Rgt[7], Rgt[8], tgt.z};
        double maxErr = 0;
        for (int i = 0; i < 50; i++) {
            Vec3 X = {ux(rng), ux(rng), uz(rng)};
            Vec3 xc2 = mul(Rgt, X) + tgt;
            Vec3 a = X.normalized(), b = xc2.normalized();  // unit bearings
            Vec3 Xr = triangulateDLT(P1, P2, a, b);
            maxErr = std::max(maxErr, (Xr - X).norm());
        }
        printf("geom: triangulation max error %.2e\n", maxErr);
        if (maxErr > 1e-6) { printf("  FAIL: triangulation\n"); fails++; }
    }

    // ---- linear algebra the estimators stand on (D24, D25) ----
    //
    // These two used to be checked only indirectly, through pose accuracy, which
    // is how a silently non-orthonormal svd3 survived: it produced a U with a
    // zero column for every rank-deficient input -- i.e. every essential matrix
    // -- and the two-view test still passed on the pose it happened to pick.
    {
        std::mt19937 lrng(11);
        std::uniform_real_distribution<double> lu(-1, 1);

        // Symmetric eigen-decomposition: A v = lambda v, and V orthonormal,
        // across sizes and a wide range of scales.
        double eigErr = 0, orthErr = 0;
        for (int t = 0; t < 300; t++) {
            int n = 3 + (t % 10);
            double scale = std::pow(10.0, (t % 7) - 3);
            std::vector<double> A((size_t)n * n), A0;
            for (int i = 0; i < n; i++)
                for (int j = i; j < n; j++) {
                    double v = lu(lrng) * scale;
                    A[(size_t)i * n + j] = v;
                    A[(size_t)j * n + i] = v;
                }
            A0 = A;
            std::vector<double> w, V;
            jacobiEigenSymmetric(A, n, w, V);
            for (int c = 0; c < n; c++)
                for (int r = 0; r < n; r++) {
                    double s = 0;
                    for (int k = 0; k < n; k++) s += A0[(size_t)r * n + k] * V[(size_t)k * n + c];
                    eigErr = std::max(eigErr, std::fabs(s - w[c] * V[(size_t)r * n + c]) / scale);
                }
            for (int a = 0; a < n; a++)
                for (int b = 0; b < n; b++) {
                    double s = 0;
                    for (int k = 0; k < n; k++) s += V[(size_t)k * n + a] * V[(size_t)k * n + b];
                    orthErr = std::max(orthErr, std::fabs(s - (a == b ? 1.0 : 0.0)));
                }
        }
        printf("geom: jacobi eigen residual %.2e, orthonormality %.2e\n", eigErr, orthErr);
        if (eigErr > 1e-9 || orthErr > 1e-11) { printf("  FAIL: jacobiEigenSymmetric\n"); fails++; }

        // svd3 on deliberately rank-deficient input: U and V must stay
        // orthonormal and A = U diag(s) V^T must still hold.
        double recErr = 0, uErr = 0;
        for (int t = 0; t < 300; t++) {
            Mat3 A{};
            int rank = t % 4;  // 0..3, so ranks 0,1,2 are all exercised
            for (int k = 0; k < rank; k++) {
                Vec3 a{lu(lrng), lu(lrng), lu(lrng)}, b{lu(lrng), lu(lrng), lu(lrng)};
                for (int i = 0; i < 3; i++)
                    for (int j = 0; j < 3; j++)
                        A[3 * i + j] += (&a.x)[i] * (&b.x)[j];
            }
            Svd3 s = svd3(A);
            Mat3 D = {s.s.x, 0, 0, 0, s.s.y, 0, 0, 0, s.s.z};
            Mat3 rec = mul(mul(s.U, D), transpose(s.V));
            for (int i = 0; i < 9; i++) recErr = std::max(recErr, std::fabs(rec[i] - A[i]));
            Mat3 UtU = mul(transpose(s.U), s.U), VtV = mul(transpose(s.V), s.V);
            for (int i = 0; i < 9; i++) {
                double id = (i % 4 == 0) ? 1.0 : 0.0;
                uErr = std::max(uErr, std::fabs(UtU[i] - id));
                uErr = std::max(uErr, std::fabs(VtV[i] - id));
            }
        }
        printf("geom: svd3 (ranks 0-3) reconstruction %.2e, orthonormality %.2e\n", recErr, uErr);
        if (recErr > 1e-12 || uErr > 1e-12) { printf("  FAIL: svd3\n"); fails++; }

        // The case that actually bit: every essential matrix is rank 2, and the
        // true (R,t) must be among the four decomposition candidates.
        int missed = 0;
        for (int t = 0; t < 300; t++) {
            double a = lu(lrng) * 0.6, b = lu(lrng) * 0.6, c = lu(lrng) * 0.6;
            Mat3 Rx = {1, 0, 0, 0, std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a)};
            Mat3 Ry = {std::cos(b), 0, std::sin(b), 0, 1, 0, -std::sin(b), 0, std::cos(b)};
            Mat3 Rz = {std::cos(c), -std::sin(c), 0, std::sin(c), std::cos(c), 0, 0, 0, 1};
            Mat3 R = mul(mul(Rz, Ry), Rx);
            Vec3 tv = Vec3{lu(lrng), lu(lrng), lu(lrng)}.normalized();
            Mat3 Ex = mul(Mat3{0, -tv.z, tv.y, tv.z, 0, -tv.x, -tv.y, tv.x, 0}, R);
            bool found = false;
            for (const Pose& cand : decomposeEssential(Ex)) {
                double dr = 0;
                for (int i = 0; i < 9; i++) dr = std::max(dr, std::fabs(cand.R[i] - R[i]));
                if (dr < 1e-8) found = true;
            }
            if (!found) missed++;
        }
        printf("geom: essential decomposition recovers truth %d/300\n", 300 - missed);
        if (missed > 0) { printf("  FAIL: decomposeEssential\n"); fails++; }

        // Null spaces of under-determined systems (D27), the RANSAC minimal
        // solvers' inner loop: the returned basis must satisfy A x = 0 and be
        // orthonormal, at the shapes the estimators actually use.
        double axErr = 0, nsOrth = 0;
        for (int t = 0; t < 400; t++) {
            const int m = (t % 2) ? 7 : 8;         // 7-point F, and 4-point H / 8-point F
            const int want = 9 - m;
            const double scale = std::pow(10.0, (t % 5) - 2);
            std::vector<double> A((size_t)m * 9);
            for (double& v : A) v = lu(lrng) * scale;
            auto ns = nullVectors9(A, m, want);
            if ((int)ns.size() != want) { axErr = 1e9; break; }
            for (size_t a = 0; a < ns.size(); a++) {
                for (int r = 0; r < m; r++) {
                    double s = 0;
                    for (int c = 0; c < 9; c++) s += A[(size_t)r * 9 + c] * ns[a][c];
                    axErr = std::max(axErr, std::fabs(s) / scale);
                }
                for (size_t b = 0; b < ns.size(); b++) {
                    double d = 0;
                    for (int c = 0; c < 9; c++) d += ns[a][c] * ns[b][c];
                    nsOrth = std::max(nsOrth, std::fabs(d - (a == b ? 1.0 : 0.0)));
                }
            }
        }
        printf("geom: null space (7x9, 8x9) max |A x| %.2e, orthonormality %.2e\n", axErr, nsOrth);
        if (axErr > 1e-12 || nsOrth > 1e-12) { printf("  FAIL: nullVectors9\n"); fails++; }
    }

    // ---- EPnP: 3D, planar, wide-angle, noisy, degenerate, LO-RANSAC ----
    {
        std::mt19937 erng(91);
        std::uniform_real_distribution<double> uxy(-2.0, 2.0), uz3(-1.5, 1.5);
        std::uniform_real_distribution<double> ua(-1.0, 1.0), u01(0.0, 1.0);
        std::normal_distribution<double> pnoise(0.0, 0.5 / 800.0);

        auto runSuite = [&](const char* label, int n, bool planar, bool wide, double zscale,
                            double worldScale, Vec3 shift, double sig) {
            int ok = 0;
            const int trials = 25;
            for (int t = 0; t < trials; t++) {
                Mat3 Rgt = angleAxisToRotation({ua(erng) * 0.6, ua(erng) * 0.6, ua(erng) * 0.6});
                Vec3 tgt = Vec3{ua(erng), ua(erng), wide ? 1.5 : 7.0} * worldScale - mul(Rgt, shift);
                Vec3 Cgt = cameraCenter({Rgt, tgt});
                std::vector<Vec3> X, br;
                while ((int)X.size() < n) {
                    Vec3 Xw;
                    if (planar) {
                        double x = uxy(erng), y = uxy(erng);
                        Xw = Vec3{x, y, 0.2 * x - 0.1 * y + 1.0} * worldScale + shift;
                    } else {
                        Xw = Vec3{uxy(erng), uxy(erng), uz3(erng) * zscale} * worldScale + shift;
                    }
                    Vec3 xc = mul(Rgt, Xw) + tgt;
                    double xcNorm = xc.norm();
                    if (xcNorm < 0.2 * worldScale) continue;
                    double ang = std::acos(std::max(-1.0, std::min(1.0, xc.z / xcNorm)));
                    if (wide ? ang > 110.0 * M_PI / 180.0 : xc.z < 0.5 * worldScale) continue;
                    Vec3 ray = xc * (1.0 / xcNorm);
                    if (sig > 0.0)
                        ray = (ray + Vec3{pnoise(erng), pnoise(erng), pnoise(erng)}).normalized();
                    X.push_back(Xw);
                    br.push_back(ray);
                }
                std::vector<Pose> got = epnp(X, br);
                if (got.size() == 1) {
                    double rErr = rotationErrorDeg(got[0].R, Rgt);
                    double cErr = (cameraCenter(got[0]) - Cgt).norm() / worldScale;
                    double rTol = sig > 0.0 ? 2.5 : 1e-3;
                    double cTol = sig > 0.0 ? 0.35 : 1e-3;
                    if (rErr < rTol && cErr < cTol) ok++;
                }
            }
            const int minOk = sig > 0.0 ? trials - 1 : trials;
            printf("geom: EPnP %-22s %d/%d\n", label, ok, trials);
            if (ok < minOk) { printf("  FAIL: EPnP %s\n", label); fails++; }
        };

        runSuite("3d n=4 shifted", 4, false, false, 1.0, 1.0, {1000.0, -500.0, 200.0}, 0.0);
        runSuite("3d n=5 scaled", 5, false, false, 1.0, 1e-3, {0.0, 0.0, 0.0}, 0.0);
        runSuite("3d n=25 noisy", 25, false, false, 1.0, 100.0, {1000.0, -500.0, 200.0}, 1.0);
        runSuite("near-planar 1e-4", 20, false, false, 1e-4, 1.0, {0.0, 0.0, 0.0}, 0.0);
        runSuite("planar n=4", 4, true, false, 0.0, 1.0, {0.0, 0.0, 0.0}, 0.0);
        runSuite("planar n=15 noisy", 15, true, false, 0.0, 1.0, {0.0, 0.0, 0.0}, 1.0);
        runSuite("wide 110deg 3d", 20, false, true, 1.0, 1.0, {0.0, 0.0, 0.0}, 1.0);
        runSuite("wide 110deg planar", 15, true, true, 0.0, 1.0, {0.0, 0.0, 0.0}, 0.0);

        // Degenerate / malformed inputs must fail cleanly without NaNs.
        std::vector<Vec3> dX = {{0, 0, 5}, {1, 0, 5}, {2, 0, 5}, {3, 0, 5}};  // collinear
        std::vector<Vec3> dB = {Vec3{0, 0, 1}, Vec3{0.1, 0, 1}.normalized(),
                                Vec3{0.2, 0, 1}.normalized(), Vec3{0.3, 0, 1}.normalized()};
        if (!epnp({{0, 0, 5}, {1, 0, 5}, {0, 1, 5}}, {dB[0], dB[1], dB[2]}).empty() ||
            !epnp(std::vector<Vec3>(4, {1, 2, 3}), dB).empty() ||
            !epnp(dX, dB).empty() ||
            !epnp(dX, {dB[0], dB[1], dB[2], {0, 0, 0}}).empty() ||
            !epnp(dX, {dB[0], dB[1], dB[2]}).empty()) {
            printf("  FAIL: EPnP degenerate inputs\n");
            fails++;
        }

        // RANSAC PnP on planar + wide-angle outlier-rich scenes via EPnP refit.
        for (int mode = 0; mode < 2; mode++) {
            const bool isPlanar = (mode == 0);
            Mat3 Rgt = rotationY(isPlanar ? 18.0 : 12.0);
            Vec3 tgt = isPlanar ? Vec3{0.6, -0.3, 5.0} : Vec3{0.8, 0.1, 1.2};
            std::vector<Vec3> rX, rB;
            while ((int)rX.size() < 40) {
                Vec3 Xw = isPlanar ? Vec3{uxy(erng), uxy(erng), 0.15 * uxy(erng) + 1.0}
                                   : Vec3{uxy(erng), uxy(erng), uz3(erng)};
                Vec3 xc = mul(Rgt, Xw) + tgt;
                if (xc.norm() < 0.3 || (isPlanar && xc.z < 1.0)) continue;
                rX.push_back(Xw);
                rB.push_back((xc.normalized() + Vec3{pnoise(erng), pnoise(erng), pnoise(erng)}).normalized());
            }
            for (int i = 0; i < 15; i++) {
                rX.push_back({uxy(erng) * 3.0, uxy(erng) * 3.0, 2.0 + u01(erng) * 5.0});
                rB.push_back(Vec3{ua(erng), ua(erng), isPlanar ? 1.0 : ua(erng)}.normalized());
            }
            PnPResult pr = ransacPnP(rX, rB, 800.0, 4.0, 7);
            double rErr = rotationErrorDeg(pr.pose.R, Rgt);
            double tErr = (pr.pose.t - tgt).norm();
            printf("geom: ransacPnP (%s+outliers): %d/%zu inliers, R %.2f deg, t %.3f\n",
                   isPlanar ? "planar" : "wide", pr.num_inliers, rX.size(), rErr, tErr);
            if (!pr.success || pr.num_inliers < 35 || rErr > 1.0 || tErr > 0.15) {
                printf("  FAIL: ransacPnP (%s)\n", isPlanar ? "planar" : "wide");
                fails++;
            }
        }
    }

    printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main() { return sfmTestMain(0, nullptr, cmdGeomSelftest); }
