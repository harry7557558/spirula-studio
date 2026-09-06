// The metric gauge: Sim(3) fit to reference camera positions, its uncertainty
// and its gates (host only).
//
// Prints PASS/FAIL and returns 0/1. See docs/testing.md.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Model.h"
#include "sfm/map/Orient.h"
#include "sfm/core/Pose.h"
#include "sfm/map/MetricGauge.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

// Camera at C looking at `target`, y-up. Also in sfm_merge_test.cpp: a local
// copy beats a shared test-support header for nine lines of scene setup.
static Pose lookAt(const Vec3& C, const Vec3& target) {
    Vec3 f = (target - C).normalized();
    Vec3 up0 = {0, 1, 0};
    Vec3 r = up0.cross(f).normalized();
    Vec3 u = f.cross(r);
    Mat3 R = {r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z};
    Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

// Cameras on an arc of radius 3 with height variation, so the centres span all
// three axes: a planar or collinear set is a separate fixture below.
static std::vector<Vec3> arcCentres(int n) {
    std::vector<Vec3> c(n);
    for (int i = 0; i < n; i++) {
        const double a = 2.4 * M_PI * i / n;
        c[i] = {3.0 * std::cos(a), 0.8 * std::sin(3.0 * a), 3.0 * std::sin(a)};
    }
    return c;
}

static double chordal(const Mat3& A, const Mat3& B) {
    double s = 0;
    for (int i = 0; i < 9; i++) s += (A[i] - B[i]) * (A[i] - B[i]);
    return std::sqrt(s);
}

static Mat3 rotFromAxisAngle(const Vec3& axis, double ang) {
    return angleAxisToRotation(axis.normalized() * ang);
}

// The reference positions a known Sim3 would produce from `centres`.
static MetricRef makeRef(const std::vector<Vec3>& centres, const Sim3& T) {
    MetricRef ref;
    ref.centres = centres;
    ref.targets.reserve(centres.size());
    for (const Vec3& c : centres) ref.targets.push_back(transformPoint(T, c));
    return ref;
}

int cmdMetricSelftest(int, char**) {
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) { printf("  FAIL: %s\n", what); fails++; }
        return ok;
    };

    // ---- T1: exact recovery of a known Sim3, at two non-unit scales -------
    // The scales are deliberately not 1: a fit that normalizes by the target
    // variance, or divides the trace by three, is invisible at s = 1.
    for (double s_true : {0.0731, 12.4}) {
        Sim3 T;
        T.scale = s_true;
        T.R = rotFromAxisAngle({0.3, -0.7, 0.5}, 0.9);
        T.t = {11.0, -4.0, 2.5};
        MetricRef ref = makeRef(arcCentres(24), T);
        MetricFit fit = fitMetricGauge(ref, 0.05);
        char what[96];
        snprintf(what, sizeof what, "T1 s=%g: ok", s_true);
        check(fit.ok, what);
        snprintf(what, sizeof what, "T1 s=%g: scale", s_true);
        check(std::fabs(fit.T.scale / s_true - 1.0) <= 1e-9, what);
        snprintf(what, sizeof what, "T1 s=%g: rotation", s_true);
        check(chordal(fit.T.R, T.R) <= 1e-9, what);
        snprintf(what, sizeof what, "T1 s=%g: rms", s_true);
        check(fit.rms <= 1e-9, what);
        snprintf(what, sizeof what, "T1 s=%g: all inliers", s_true);
        check(fit.inliers == fit.n && fit.n == 24, what);
    }

    // ---- T1b: a left-handed reference, where the guard actually fires -----
    // Unguarded this fits to 3e-15 m with det(R) = -1, so no RMS gate can see
    // it. A coplanar fixture cannot test this: det(cov) is then round-off.
    {
        int proper = 0, fitted = 0;
        double worst_rms = 0;
        for (int trial = 0; trial < 200; trial++) {
            std::vector<Vec3> src(24);
            for (int i = 0; i < 24; i++) {
                const double a = 2.4 * M_PI * i / 24;
                src[i] = {3.0 * std::cos(a), 0.02 * std::sin(3.0 * a), 3.0 * std::sin(a)};
            }
            Sim3 T;
            T.scale = 2.5;
            T.R = rotFromAxisAngle({0.3, -0.7, 0.5}, 0.9 + 0.005 * trial);
            T.t = {4.0, 1.0, -2.0};
            MetricRef ref;
            ref.centres = src;
            for (const Vec3& c : src)
                ref.targets.push_back(transformPoint(T, {c.x, -c.y, c.z}));
            MetricFit fit = fitMetricGauge(ref, 0.5);
            if (fit.ok) {
                fitted++;
                worst_rms = std::max(worst_rms, fit.rms);
                if (det3(fit.T.R) > 0.999999 && det3(fit.T.R) < 1.000001) proper++;
            }
        }
        printf("  T1b: fitted %d/200, worst rms %.4f m\n", fitted, worst_rms);
        check(fitted == 200, "T1b mirrored reference: every trial fitted");
        check(proper == 200, "T1b mirrored reference: det(R) = +1 on 200/200");
    }

    // ---- T2: gross outliers, and near-threshold points that must survive --
    {
        Sim3 T;
        T.scale = 12.4;
        T.R = rotFromAxisAngle({0.2, 0.4, -0.9}, 1.7);
        T.t = {-3.0, 8.0, 1.0};
        // Above 3 m, so that comparing an unsquared residual to max_error^2
        // LOOSENS the threshold: 3x is then rejected but 3x < max_err^2, and
        // 3x > 2x means no shift of the model can capture it either.
        const double max_err = 4.0;
        std::vector<Vec3> centres = arcCentres(40);
        MetricRef ref = makeRef(centres, T);
        std::vector<char> want(40, 1);
        std::mt19937 rng(5);
        std::uniform_real_distribution<double> dir(-1.0, 1.0);
        for (int i = 0; i < 40; i++) {
            Vec3 d = Vec3{dir(rng), dir(rng), dir(rng)}.normalized();
            if (i % 10 == 3 || i % 10 == 7) {         // 20 %: out at 3x
                ref.targets[i] = ref.targets[i] + d * (3.0 * max_err);
                want[i] = 0;
            } else if (i % 10 == 5) {                 // 10 %: in at 0.3x
                ref.targets[i] = ref.targets[i] + d * (0.3 * max_err);
            }
        }
        MetricFit fit = fitMetricGauge(ref, max_err);
        check(fit.ok, "T2: ok");
        bool mask_exact = fit.inlier_mask.size() == want.size();
        for (size_t i = 0; mask_exact && i < want.size(); i++)
            mask_exact = (fit.inlier_mask[i] != 0) == (want[i] != 0);
        check(mask_exact, "T2: inlier mask equals the constructed set exactly");
        check(fit.inliers == 32, "T2: 32 inliers");
        {
            int wrong_in = 0, wrong_out = 0;
            for (size_t i = 0; i < want.size(); i++) {
                if (want[i] && !fit.inlier_mask[i]) wrong_out++;
                if (!want[i] && fit.inlier_mask[i]) wrong_in++;
            }
            printf("  T2: inliers %d (want 32), kept-outliers %d, dropped-inliers %d\n",
                   fit.inliers, wrong_in, wrong_out);
        }
        // The kept near-threshold points carry real displacement, so the refit
        // moves off truth by a bounded amount rather than to 1e-9.
        printf("  T2: scale rel err %.3e\n", std::fabs(fit.T.scale / T.scale - 1.0));
        check(std::fabs(fit.T.scale / T.scale - 1.0) <= 5e-3, "T2: scale within 0.5 %");
    }

    // ---- T3: 300 trials of one geometry with fresh noise ------------------
    // Empirical spread of the recovered scale, and of the rotation about the
    // WORST principal axis, against what the fit predicts for each.
    {
        const int trials = 300, n = 50;
        const double sigma = 0.05, s_true = 0.7;
        Sim3 T;
        T.scale = s_true;
        T.R = rotFromAxisAngle({0.5, 0.2, 0.84}, 0.6);
        T.t = {2.0, -1.0, 0.5};
        std::vector<Vec3> centres = arcCentres(n);
        // The axis the reported figure is the worst of: least spread across it,
        // so the largest eigenvalue of the centres' covariance.
        Vec3 cbar{0, 0, 0};
        for (const Vec3& c : centres) cbar = cbar + c;
        cbar = cbar * (1.0 / n);
        std::vector<double> C(9, 0.0), lam, V;
        for (const Vec3& c : centres) {
            const Vec3 a = c - cbar;
            const double v[3] = {a.x, a.y, a.z};
            for (int r = 0; r < 3; r++)
                for (int q = 0; q < 3; q++) C[3 * r + q] += v[r] * v[q] / n;
        }
        jacobiEigenSymmetric(C, 3, lam, V);
        int kmax = 0;
        for (int k = 1; k < 3; k++)
            if (lam[k] > lam[kmax]) kmax = k;
        const Vec3 axis = mul(T.R, Vec3{V[kmax], V[3 + kmax], V[6 + kmax]});

        std::mt19937 rng(3);
        std::normal_distribution<double> nz(0.0, sigma);
        double sum_rel = 0, sum_rel2 = 0, sum_ax2 = 0;
        double pred_scale = 0, pred_rot = 0;
        int ok_count = 0;
        for (int k = 0; k < trials; k++) {
            MetricRef ref = makeRef(centres, T);
            for (Vec3& p : ref.targets) p = p + Vec3{nz(rng), nz(rng), nz(rng)};
            MetricFit fit = fitMetricGauge(ref, 1.0);   // 11 sigma: nothing is rejected
            if (!fit.ok) continue;
            ok_count++;
            const double rel = fit.T.scale / s_true - 1.0;
            sum_rel += rel;
            sum_rel2 += rel * rel;
            const Vec3 dth = rotationToAngleAxis(mul(fit.T.R, transpose(T.R)));
            const double about = dth.dot(axis);
            sum_ax2 += about * about;
            pred_scale += fit.scale_unc / 100.0;
            pred_rot += fit.rot_unc_deg * M_PI / 180.0;
        }
        check(ok_count == trials, "T3: every trial fitted");
        const double emp_scale =
            std::sqrt(sum_rel2 / ok_count - (sum_rel / ok_count) * (sum_rel / ok_count));
        const double emp_rot = std::sqrt(sum_ax2 / ok_count);
        pred_scale /= ok_count;
        pred_rot /= ok_count;
        const double r_s = emp_scale / pred_scale, r_r = emp_rot / pred_rot;
        printf("  T3: scale emp/pred = %.4f (emp %.3e pred %.3e), "
               "rot emp/pred = %.4f (emp %.3e pred %.3e rad)\n",
               r_s, emp_scale, pred_scale, r_r, emp_rot, pred_rot);
        check(r_s >= 0.75 && r_s <= 1.33, "T3: scale uncertainty predicts the spread");
        check(r_r >= 0.75 && r_r <= 1.33, "T3: rotation uncertainty predicts the spread");
    }

    // ---- T4: every refusal, asserted on its REASON, not on the bool ------
    {
        Sim3 T;
        T.scale = 4.0;
        T.R = mat3Identity();
        T.t = {0, 0, 0};
        for (int n : {0, 1, 2}) {
            MetricRef ref = makeRef(arcCentres(std::max(n, 1)), T);
            ref.centres.resize(n);
            ref.targets.resize(n);
            MetricFit fit = fitMetricGauge(ref, 0.5);
            check(!fit.ok && fit.reason == MetricFail::Pairs, "T4: n < 3 -> Pairs");
        }
        {   // every reference position identical: nothing to fit a scale to
            MetricRef ref = makeRef(arcCentres(20), T);
            for (Vec3& p : ref.targets) p = ref.targets[0];
            MetricFit fit = fitMetricGauge(ref, 0.5);
            check(!fit.ok && fit.reason == MetricFail::Spread, "T4: no spread -> Spread");
        }
        {   // cameras exactly on a line: the rotation about it is free
            MetricRef ref;
            for (int i = 0; i < 20; i++) {
                ref.centres.push_back({0.4 * i, 0, 0});
                ref.targets.push_back({1.6 * i, 0, 0});
            }
            MetricFit fit = fitMetricGauge(ref, 0.5);
            check(!fit.ok && fit.reason == MetricFail::Collinear, "T4: collinear -> Collinear");
        }
        {   // every reference position wrong by far more than max_error
            MetricRef ref = makeRef(arcCentres(20), T);
            std::mt19937 rng(9);
            std::uniform_real_distribution<double> big(-500.0, 500.0);
            for (Vec3& p : ref.targets) p = {big(rng), big(rng), big(rng)};
            MetricFit fit = fitMetricGauge(ref, 0.05);
            check(!fit.ok && fit.reason == MetricFail::Inliers, "T4: all outliers -> Inliers");
            check(fit.inliers < (fit.n + 1) / 2, "T4: all outliers -> under half are inliers");
        }
        {   // A minority that agrees perfectly is still a minority: 8 of 24
            // cameras on an exact Sim3, the rest elsewhere. The 8 are well
            // spread, so every uncertainty gate would wave them through.
            Sim3 T2;
            T2.scale = 2.0;
            T2.R = rotFromAxisAngle({0.1, 0.2, 0.97}, 1.3);
            T2.t = {6.0, -2.0, 3.0};
            MetricRef ref = makeRef(arcCentres(24), T2);
            std::mt19937 rng(31);
            std::uniform_real_distribution<double> big(-300.0, 300.0);
            for (int i = 8; i < 24; i++) ref.targets[i] = {big(rng), big(rng), big(rng)};
            MetricFit fit = fitMetricGauge(ref, 0.2);
            printf("  T4: minority consensus %d/%d, scale unc %.4f %%, rot unc %.4f deg\n",
                   fit.inliers, fit.n, fit.scale_unc, fit.rot_unc_deg);
            check(fit.inliers == 8, "T4: the minority consensus is found");
            check(!fit.ok && fit.reason == MetricFail::Inliers,
                  "T4: a well-conditioned minority is still refused");
        }
    }

    // ---- T8: applySim3 puts the centres on the targets, and moves nothing
    // else. Reprojection is gauge-invariant, so it must not budge.
    {
        Sim3 T;
        T.scale = 0.0731;
        T.R = rotFromAxisAngle({0.6, -0.3, 0.74}, 1.1);
        T.t = {40.0, -12.0, 7.0};
        const int M = 18, N = 200;
        const int W = 1280, H = 960;
        Camera K = Camera::defaultFor(1, W, H, 1200);
        Reconstruction rec;
        rec.cameras[1] = K;
        std::mt19937 rng(13);
        std::uniform_real_distribution<double> ub(-2.0, 2.0);
        std::vector<Vec3> pts(N);
        for (Vec3& p : pts) p = {ub(rng), ub(rng), ub(rng)};
        std::vector<Vec3> centres = arcCentres(M);
        for (int i = 0; i < M; i++) {
            Image im;
            im.id = (uint32_t)(i + 1);
            im.camera_id = 1;
            im.name = "img" + std::to_string(i) + ".jpg";
            im.pose = lookAt(centres[i], {0, 0, 0});
            im.registered = true;
            im.points2D.resize(N);
            im.point3D_ids.assign(N, kInvalidPoint3D);
            rec.images[im.id] = im;
        }
        auto px = [&](uint32_t img, const Vec3& X) {
            const Pose& pose = rec.images.at(img).pose;
            return rec.cameras[1].project(mul(pose.R, X) + pose.t);
        };
        for (int j = 0; j < N; j++) {
            std::vector<TrackElement> track;
            for (int i = 0; i < M; i++) {
                rec.images[(uint32_t)(i + 1)].points2D[j] = px((uint32_t)(i + 1), pts[j]);
                track.push_back({(uint32_t)(i + 1), (uint32_t)j});
            }
            rec.addPoint3D(pts[j], track);
        }
        // Projections before, so the invariance claim is measured, not assumed.
        std::vector<Vec2> before;
        for (const auto& kv : rec.points3D)
            for (const TrackElement& e : kv.second.track)
                before.push_back(px(e.image_id, kv.second.xyz));

        MetricRef ref;
        for (int i = 0; i < M; i++) {
            ref.centres.push_back(centres[i]);
            ref.targets.push_back(transformPoint(T, centres[i]));
            ref.image_ids.push_back((uint32_t)(i + 1));
        }
        MetricFit fit = fitMetricGauge(ref, 0.05);
        check(fit.ok, "T8: ok");
        applySim3(rec, fit.T);
        double worst_c = 0;
        for (int i = 0; i < M; i++) {
            Vec3 c = cameraCenter(rec.images.at((uint32_t)(i + 1)).pose);
            worst_c = std::max(worst_c, (c - ref.targets[i]).norm());
        }
        check(worst_c <= 1e-9, "T8: every centre lands on its target");
        size_t k = 0;
        double worst_px = 0;
        for (const auto& kv : rec.points3D)
            for (const TrackElement& e : kv.second.track) {
                const Vec2 uv = px(e.image_id, kv.second.xyz);
                worst_px = std::max(worst_px, std::max(std::fabs(uv.x - before[k].x),
                                                       std::fabs(uv.y - before[k].y)));
                k++;
            }
        check(k == before.size() && k == (size_t)M * N,
              "T8: the same observations project after the transform");
        printf("  T8: worst centre %.3e m, worst reprojection %.3e px\n", worst_c, worst_px);
        // 1e-6 px is what sfm_merge_test asks of the same invariance. The
        // floor here is ~1e-9 px: a metric frame puts the origin metres away,
        // and forming s*x_cam cancels two |t|-sized terms.
        check(worst_px <= 1e-6, "T8: reprojection unchanged");
    }

    // ---- T9: the collinearity gate does not read the noise ----------------
    // Same thin arc at two noise levels 160x apart. A gate on the reported
    // orientation uncertainty passes the quiet one; this one refuses both.
    {
        Sim3 T;
        T.scale = 1.7;
        T.R = rotFromAxisAngle({0.0, 1.0, 0.0}, 0.35);
        T.t = {5.0, 5.0, 5.0};
        std::vector<Vec3> thin(60);
        for (int i = 0; i < 60; i++)
            thin[i] = {0.5 * i, 0.0009 * i * (i % 2 ? 1 : -1), 0.0007 * ((i / 3) % 5)};
        std::mt19937 rng(17);
        auto run = [&](const std::vector<Vec3>& c, double sigma) {
            std::normal_distribution<double> nz(0.0, sigma);
            MetricRef ref = makeRef(c, T);
            for (Vec3& p : ref.targets) p = p + Vec3{nz(rng), nz(rng), nz(rng)};
            return fitMetricGauge(ref, 10.0);
        };
        MetricFit loud = run(thin, 0.08);
        MetricFit quiet = run(thin, 0.0005);
        printf("  T9: thin arc perp frac %.5f (loud) / %.5f (quiet); rot unc %.3f / %.4f deg\n",
               loud.perp_frac, quiet.perp_frac, loud.rot_unc_deg, quiet.rot_unc_deg);
        check(!loud.ok && loud.reason == MetricFail::Collinear, "T9: thin arc -> Collinear");
        check(!quiet.ok && quiet.reason == MetricFail::Collinear,
              "T9: and 160x quieter is still Collinear -- geometry, not noise");
        check(quiet.rot_unc_deg < 5.0 && loud.rot_unc_deg > 5.0,
              "T9: the reported uncertainty WOULD have split them, which is why it cannot gate");
        check(loud.perp_frac < 0.05 && quiet.perp_frac < 0.05, "T9: both below the floor");
        // The same 60 cameras spread across the long axis instead: passes.
        std::vector<Vec3> fat(60);
        for (int i = 0; i < 60; i++)
            fat[i] = {0.5 * i, 4.0 * std::sin(0.3 * i), 3.0 * std::cos(0.21 * i)};
        MetricFit ok = run(fat, 0.08);
        printf("  T9: spread arc perp frac %.4f\n", ok.perp_frac);
        check(ok.ok, "T9: the same cameras spread off the axis pass");
    }

    // ---- T10: same inputs twice, bit for bit ----------------------------
    // Two exact 30-camera consensus sets tie under MSAC, so the winner is
    // whichever the draw reached first; one consensus converges from any seed.
    {
        Sim3 A, B;
        A.scale = 3.3;
        A.R = rotFromAxisAngle({0.3, 0.3, 0.9}, 2.0);
        A.t = {-7.0, 1.0, 4.0};
        B.scale = 1.7;
        B.R = rotFromAxisAngle({0.9, -0.2, 0.1}, 1.1);
        B.t = {50.0, -30.0, 12.0};
        std::vector<Vec3> centres = arcCentres(60);
        MetricRef ref;
        ref.centres = centres;
        for (int i = 0; i < 60; i++)
            ref.targets.push_back(transformPoint(i < 30 ? A : B, centres[i]));
        // Eight fits, not two: which set wins is a coin flip per seed, so a pair
        // that happens to agree proves nothing about a varying one.
        MetricFit a = fitMetricGauge(ref, 0.1);
        bool same = true;
        for (int rep = 0; rep < 7; rep++) {
            MetricFit b = fitMetricGauge(ref, 0.1);
            same = same && a.ok == b.ok && a.reason == b.reason && a.T.scale == b.T.scale &&
                   a.T.t.x == b.T.t.x && a.T.t.y == b.T.t.y && a.T.t.z == b.T.t.z &&
                   a.inliers == b.inliers && a.inlier_mask == b.inlier_mask &&
                   a.rms == b.rms && a.scale_unc == b.scale_unc && a.perp_frac == b.perp_frac;
            for (int i = 0; i < 9; i++) same = same && a.T.R[i] == b.T.R[i];
        }
        printf("  T10: locked onto scale %.4f with %d/%d inliers\n", a.T.scale, a.inliers, a.n);
        check(same, "T10: eight runs agree bit for bit");
        check(a.inliers == 30, "T10: exactly one of the two consensus sets is found");
        const bool bimodal = std::fabs(a.T.scale / A.scale - 1.0) < 1e-9 ||
                             std::fabs(a.T.scale / B.scale - 1.0) < 1e-9;
        check(bimodal, "T10: the fixture really has two answers to choose between");
    }

    // ---- T5: pairing a positions file to image names ----------------------
    // Two files share a basename in different folders, one entry has no
    // extension, one names an image the model does not have.
    {
        namespace fs = std::filesystem;
        const fs::path dir = fs::temp_directory_path() / "sfm_metric_t5";
        fs::remove_all(dir);
        fs::create_directories(dir);
        const fs::path pf = dir / "positions.txt";
        {
            std::ofstream f(pf.string());
            f << "# a comment, and the blank line below\n\n";
            f << "cam1/a.jpg 1 2 3\n";
            f << "cam2/a.jpg 4 5 6\n";
            f << "b 7 8 9\n";              // stem match against b.png
            f << "zzz.jpg 10 11 12\n";     // not in the model
            f << "e.jpg 20 21 22\n";       // in the model, but unregistered
            f << "v1 99 99 99\n";          // a dot in a FOLDER is not an extension
        }
        std::map<std::string, Vec3> pos;
        std::string err;
        check(readMetricPositions(pf.string(), pos, err), "T5: the file parses");
        check(pos.size() == 6, "T5: six entries read");

        Reconstruction rec;
        const char* names[6] = {"cam1/a.jpg", "cam2/a.jpg", "b.png", "d.jpg", "e.jpg",
                                "v1.0/frame"};
        for (int i = 0; i < 6; i++) {
            Image im;
            im.id = (uint32_t)(i + 1);
            im.name = names[i];
            im.registered = i != 4;  // e.jpg is in the model but not solved
            im.pose = {mat3Identity(), {(double)-i, 0, 0}};
            rec.images[im.id] = im;
        }
        MetricRef ref;
        MetricPairCounts pc = pairMetricRef(rec, pos, ref);
        printf("  T5: matched %d, file-only %d, model-only %d\n",
               pc.matched, pc.unmatched_file, pc.unmatched_model);
        check(pc.matched == 3 && (int)ref.targets.size() == 3, "T5: three matched");
        check(pc.unmatched_file == 3, "T5: an unregistered image leaves its entry unused");
        check(pc.unmatched_model == 2, "T5: a dotted folder is not an extension");
        std::map<uint32_t, Vec3> got;
        for (size_t i = 0; i < ref.image_ids.size(); i++) got[ref.image_ids[i]] = ref.targets[i];
        check(got.count(1) && got.count(2) && got.count(3), "T5: the expected three images");
        check(got.count(1) && got[1].x == 1 && got[1].y == 2 && got[1].z == 3 &&
              got.count(2) && got[2].x == 4 && got[2].y == 5 && got[2].z == 6,
              "T5: same basename in two folders stays two cameras");
        check(got.count(3) && got[3].x == 7, "T5: an extensionless entry matches by stem");

        {   // a malformed line is refused, and the message names its number
            const fs::path bad = dir / "bad.txt";
            std::ofstream f(bad.string());
            f << "ok.jpg 1 2 3\n\nbroken.jpg 1 2\n";
            f.close();
            std::map<std::string, Vec3> p2;
            std::string e2;
            check(!readMetricPositions(bad.string(), p2, e2), "T5: a short line is refused");
            check(e2.find("3") != std::string::npos, "T5: the error names line 3");
        }
        {   // a fourth number is not a comment: refuse rather than ignore it
            const fs::path junk = dir / "junk.txt";
            std::ofstream f(junk.string());
            f << "ok.jpg 1 2 3 4\n";
            f.close();
            std::map<std::string, Vec3> pj;
            std::string ej;
            check(!readMetricPositions(junk.string(), pj, ej),
                  "T5: a trailing field is refused");
        }
        {   // the same image twice is a mistake, not a last-one-wins
            const fs::path dup = dir / "dup.txt";
            std::ofstream f(dup.string());
            f << "a.jpg 1 2 3\na.jpg 4 5 6\n";
            f.close();
            std::map<std::string, Vec3> p3;
            std::string e3;
            check(!readMetricPositions(dup.string(), p3, e3), "T5: a repeated name is refused");
        }
        {   // a path that is not there reports so rather than reading nothing
            std::map<std::string, Vec3> p4;
            std::string e4;
            check(!readMetricPositions((dir / "nope.txt").string(), p4, e4),
                  "T5: a missing file is refused");
        }
        fs::remove_all(dir);
    }

    // ---- T6: the GPS IFD, hand-built, both byte orders ---------------------
    // IFD0 carries decoy tags 1..6, which are exactly the GPS IFD's own tag
    // numbers: one switch for both reads latitude out of them.
    struct Tiff {
        std::vector<uint8_t> b;
        bool le;
        void u8(uint8_t v) { b.push_back(v); }
        void u16(uint16_t v) {
            if (le) { u8((uint8_t)v); u8((uint8_t)(v >> 8)); }
            else { u8((uint8_t)(v >> 8)); u8((uint8_t)v); }
        }
        void u32(uint32_t v) {
            if (le) { u16((uint16_t)v); u16((uint16_t)(v >> 16)); }
            else { u16((uint16_t)(v >> 16)); u16((uint16_t)v); }
        }
        void ent(uint16_t tag, uint16_t type, uint32_t count, uint32_t val) {
            u16(tag); u16(type); u32(count); u32(val);
        }
        // A value of 4 bytes or fewer sits in the entry, left-aligned.
        void entIn(uint16_t tag, uint16_t type, uint32_t count,
                   const std::vector<uint8_t>& raw) {
            u16(tag); u16(type); u32(count);
            for (int i = 0; i < 4; i++) u8(i < (int)raw.size() ? raw[i] : 0);
        }
    };
    auto build = [](bool le, char latref, char lonref, uint8_t altref, bool with_gps,
                    uint32_t sec100 = 355, uint32_t lonsec100 = 1786,
                    bool with_alt = true) {
        Tiff t;
        t.le = le;
        t.u8(le ? 'I' : 'M'); t.u8(le ? 'I' : 'M');
        t.u16(42);
        t.u32(8);
        const uint16_t n0 = with_gps ? 7 : 6;
        t.u16(n0);
        for (uint16_t tag = 1; tag <= 6; tag++) t.ent(tag, 3, 1, 0xDEAD);
        const uint32_t gps = 8 + 2 + (uint32_t)n0 * 12 + 4;
        if (with_gps) t.ent(0x8825, 4, 1, gps);
        t.u32(0);
        if (!with_gps) return t.b;
        const uint32_t nv = with_alt ? 6 : 5;
        t.u16((uint16_t)nv);
        const uint32_t vals = gps + 2 + nv * 12 + 4;
        t.entIn(1, 2, 2, {(uint8_t)latref, 0});
        t.ent(2, 5, 3, vals);
        t.entIn(3, 2, 2, {(uint8_t)lonref, 0});
        t.ent(4, 5, 3, vals + 24);
        t.entIn(5, 1, 1, {altref});
        if (with_alt) t.ent(6, 5, 1, vals + 48);
        t.u32(0);
        const uint32_t r[] = {42, 1, 12, 1, sec100, 100,
                              83, 1, 40, 1, lonsec100, 100,
                              25366, 100};
        for (uint32_t v : r) t.u32(v);
        return t.b;
    };
    {
        const double lat = 42.0 + 12.0 / 60.0 + 3.55 / 3600.0;
        const double lon = 83.0 + 40.0 / 60.0 + 17.86 / 3600.0;
        for (bool le : {true, false}) {
            std::vector<uint8_t> blk = build(le, 'N', 'E', 0, true);
            ExifData e = parseExifTiff(blk.data(), blk.size());
            const char* w = le ? "T6 LE" : "T6 BE";
            check(e.valid && e.has_gps, w);
            check(std::fabs(e.lat_deg - lat) <= 1e-9, "T6: latitude, both byte orders");
            check(std::fabs(e.lon_deg - lon) <= 1e-9, "T6: longitude, both byte orders");
            check(e.has_alt && std::fabs(e.alt_m - 253.66) <= 1e-9, "T6: altitude");
        }
        {
            std::vector<uint8_t> blk = build(true, 'S', 'W', 0, true);
            ExifData e = parseExifTiff(blk.data(), blk.size());
            check(std::fabs(e.lat_deg + lat) <= 1e-9, "T6: S is negative");
            check(std::fabs(e.lon_deg + lon) <= 1e-9, "T6: W is negative");
        }
        {   // GPSAltitudeRef 1 means below sea level
            std::vector<uint8_t> blk = build(true, 'N', 'E', 1, true);
            ExifData e = parseExifTiff(blk.data(), blk.size());
            check(std::fabs(e.alt_m + 253.66) <= 1e-9, "T6: altitude ref 1 is below sea level");
        }
        {
            std::vector<uint8_t> blk = build(true, 'N', 'E', 0, false);
            ExifData e = parseExifTiff(blk.data(), blk.size());
            check(e.valid && !e.has_gps, "T6: an IFD0 of decoys is not a GPS fix");
            check(e.lat_deg == 0 && e.lon_deg == 0, "T6: and leaves no position behind");
        }
    }

    // ---- T6b: GPS straight off files, through readExif's JPEG walk ---------
    // Three minimal JPEGs, one with no EXIF at all, so the "no GPS" count is
    // measured rather than assumed.
    {
        namespace fs = std::filesystem;
        const fs::path dir = fs::temp_directory_path() / "sfm_metric_t6b";
        fs::remove_all(dir);
        fs::create_directories(dir / "sub");
        auto write_jpeg = [&](const fs::path& path, const std::vector<uint8_t>& tiff) {
            std::vector<uint8_t> j = {0xFF, 0xD8};
            if (!tiff.empty()) {
                const uint16_t seg = (uint16_t)(tiff.size() + 8);
                j.push_back(0xFF);
                j.push_back(0xE1);
                j.push_back((uint8_t)(seg >> 8));
                j.push_back((uint8_t)seg);
                for (const char* p2 = "Exif"; *p2; p2++) j.push_back((uint8_t)*p2);
                j.push_back(0);
                j.push_back(0);
                j.insert(j.end(), tiff.begin(), tiff.end());
            }
            j.push_back(0xFF);
            j.push_back(0xD9);
            std::ofstream f(path.string(), std::ios::binary);
            f.write((const char*)j.data(), (std::streamsize)j.size());
        };
        write_jpeg(dir / "a.jpg", build(true, 'N', 'W', 0, true, 355, 1786));
        write_jpeg(dir / "sub" / "b.jpg", build(true, 'N', 'W', 0, true, 1055, 1786));
        write_jpeg(dir / "c.jpg", {});
        write_jpeg(dir / "d.jpg", build(true, 'N', 'W', 0, true, 705, 1786, false));
        write_jpeg(dir / "e.jpg", build(true, 'N', 'W', 0, true, 355, 1786));
        Reconstruction rec;
        const char* names[5] = {"a.jpg", "sub/b.jpg", "c.jpg", "d.jpg", "e.jpg"};
        for (int i = 0; i < 5; i++) {
            Image im;
            im.id = (uint32_t)(i + 1);
            im.name = names[i];
            im.registered = i != 4;   // e.jpg has a fix but was never solved
            im.pose = {mat3Identity(), {(double)-i, 0, 0}};
            rec.images[im.id] = im;
        }
        MetricRef ref;
        MetricGpsCounts gc = metricRefFromGps(rec, dir.string(), ref);
        printf("  T6b: gps %d, no-gps %d, no-alt %d\n", gc.matched, gc.no_gps, gc.no_alt);
        check(gc.matched == 3 && gc.no_gps == 1,
              "T6b: three fixes, one file without EXIF, one unregistered");
        check(gc.no_alt == 1, "T6b: the fix with no altitude tag is counted");
        check(ref.targets.size() == 3 && ref.image_ids.size() == 3,
              "T6b: the unpositioned and unregistered images are left out");
        // b.jpg is 7.00 arcseconds of latitude north of a.jpg and nothing
        // else. 215.99 m by the meridional radius; a sphere of radius a gives
        // 216.43, so 0.05 m of tolerance is what refuses a flat-earth ENU.
        const Vec3 d = ref.targets[1] - ref.targets[0];
        printf("  T6b: b - a = (%.4f, %.4f, %.4f) m\n", d.x, d.y, d.z);
        check(std::fabs(d.y - 215.99) < 0.05 && std::fabs(d.x) < 0.01,
              "T6b: 216 m due north, by the meridional radius not by a");
        fs::remove_all(dir);
    }

    // ---- T7: WGS-84, against vectors computed outside this program ---------
    // (0,0,0) and (90,0,0) are the semi-axes and exact by inspection; float
    // ECEF cannot reach 1e-6 m on any of the four.
    {
        struct Case { double lat, lon, h, X, Y, Z; };
        const Case cs[4] = {
            {0, 0, 0, 6378137.000000, 0.0, 0.0},
            {90, 0, 0, 0.0, 0.0, 6356752.314245},
            {45, 0, 0, 4517590.878849, 0.0, 4487348.408866},
            {42.216, -83.664, 253.66, 522118.504341, -4702200.844425, 4263573.714297},
        };
        double worst = 0;
        for (const Case& c : cs) {
            const Vec3 p = ecefFromGeodetic(c.lat, c.lon, c.h);
            worst = std::max(worst, std::max(std::fabs(p.x - c.X),
                                             std::max(std::fabs(p.y - c.Y),
                                                      std::fabs(p.z - c.Z))));
        }
        printf("  T7: worst ECEF error %.3e m\n", worst);
        check(worst <= 1e-6, "T7: the four pinned ECEF vectors");

        const double lat0 = 42.216, lon0 = -83.664, h0 = 253.66;
        std::vector<Geodetic> g = {{lat0, lon0, h0},
                                   {lat0 + 0.001, lon0, h0},
                                   {lat0, lon0 + 0.001, h0},
                                   {lat0, lon0, h0 + 10.0}};
        std::vector<Vec3> enu = enuFromGeodetic(g, Geodetic{lat0, lon0, h0});
        const Vec3 o = enu[0];
        const double want[3][3] = {{-0.000000, 111.081917, -0.000969},
                                   {82.573260, 0.000484, -0.000534},
                                   {0.0, 0.0, 10.000000}};
        double we = 0;
        for (int i = 0; i < 3; i++) {
            const Vec3 d = enu[i + 1] - o;
            we = std::max(we, std::max(std::fabs(d.x - want[i][0]),
                                       std::max(std::fabs(d.y - want[i][1]),
                                                std::fabs(d.z - want[i][2]))));
        }
        printf("  T7: worst ENU displacement error %.3e m\n", we);
        check(we <= 1e-6, "T7: the three pinned ENU displacements");
        // A permuted or mirrored axis set leaves every fit RMS unchanged --
        // the Sim3 absorbs it -- so only the named axes can catch one.
        const Vec3 north = enu[1] - o, east = enu[2] - o, up = enu[3] - o;
        check(north.y > 100.0 && std::fabs(north.x) < 1.0, "T7: +latitude is +N");
        check(east.x > 80.0 && std::fabs(east.y) < 1.0, "T7: +longitude is +E");
        check(up.z > 9.9, "T7: +height is +U");
        check(east.normalized().cross(north.normalized()).dot(up.normalized()) > 0.999,
              "T7: E x N = U, from the fitted axes");
        // The origin --metric-gps uses is the arithmetic mean of the fixes.
        // It is not the ECEF centroid -- that leaves 3e-4 m of offset here,
        // which the Sim3's translation absorbs.
        Geodetic mo;
        for (const Geodetic& q : g) {
            mo.lat_deg += q.lat_deg / g.size();
            mo.lon_deg += q.lon_deg / g.size();
            mo.alt_m += q.alt_m / g.size();
        }
        std::vector<Vec3> by_mean = enuFromGeodetic(g);
        std::vector<Vec3> by_hand = enuFromGeodetic(g, mo);
        double dm = 0;
        for (size_t i = 0; i < g.size(); i++) dm = std::max(dm, (by_mean[i] - by_hand[i]).norm());
        check(dm == 0.0, "T7: the default origin is the mean of the fixes");
        check((by_mean[0] - by_hand[0]).norm() == 0.0 &&
                  (by_mean[0] - enuFromGeodetic(g, g[0])[0]).norm() > 1e-3,
              "T7: and not the first fix");
    }

    // ---- T11: the conditioning floor is pinned from both sides -------------
    // A line with a chosen transverse spread. 0.045 and 0.055 straddle the
    // constant, so moving it outside +/-10 % turns one of these red.
    {
        Sim3 T;
        T.scale = 2.5;
        T.R = rotFromAxisAngle({0.2, 0.5, 0.84}, 0.7);
        T.t = {3.0, -1.0, 8.0};
        // lambda1 = (n^2-1)/12 along x, lambda2 = a^2, so perp_frac is
        // sqrt(a^2/(lambda1+a^2)); the + - - + sign pattern makes the cross
        // covariance with x exactly zero, so those are the true eigenvalues.
        auto line = [](double a) {
            std::vector<Vec3> c(60);
            for (int i = 0; i < 60; i++) {
                const int j = i % 4;
                c[i] = {(double)i, (j == 0 || j == 3) ? a : -a, 0.0};
            }
            return c;
        };
        MetricFit below = fitMetricGauge(makeRef(line(0.780105), T), 0.5);
        MetricFit above = fitMetricGauge(makeRef(line(0.953940), T), 0.5);
        printf("  T11: perp frac %.6f (below the floor) / %.6f (above)\n",
               below.perp_frac, above.perp_frac);
        check(std::fabs(below.perp_frac - 0.045) < 1e-6, "T11: the low fixture really is 0.045");
        check(std::fabs(above.perp_frac - 0.055) < 1e-6, "T11: the high fixture really is 0.055");
        check(!below.ok && below.reason == MetricFail::Collinear,
              "T11: 0.045 is refused, so the floor is not below it");
        check(above.ok, "T11: 0.055 is accepted, so the floor is not above it");
    }

    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdMetricSelftest); }
