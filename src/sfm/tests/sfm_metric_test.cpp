// The metric gauge: Sim(3) fit to reference camera positions, its uncertainty
// and its gates (host only).
//
// Prints PASS/FAIL and returns 0/1. See docs/testing.md.
#include <cmath>
#include <cstdio>
#include <random>
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
            check(!fit.ok && fit.reason == MetricFail::Rotation, "T4: collinear -> Rotation");
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

    // ---- T9: G4 is wired to the reported rotation uncertainty ------------
    // One long thin arc at two noise levels. The geometry is identical; only
    // sigma moves, so a gate that fires on anything else fails this.
    {
        Sim3 T;
        T.scale = 1.7;
        T.R = rotFromAxisAngle({0.0, 1.0, 0.0}, 0.35);
        T.t = {5.0, 5.0, 5.0};
        std::vector<Vec3> thin(60);
        for (int i = 0; i < 60; i++)
            thin[i] = {0.5 * i, 0.0009 * i * (i % 2 ? 1 : -1), 0.0007 * ((i / 3) % 5)};
        std::mt19937 rng(17);
        auto run = [&](double sigma) {
            std::normal_distribution<double> nz(0.0, sigma);
            MetricRef ref = makeRef(thin, T);
            for (Vec3& p : ref.targets) p = p + Vec3{nz(rng), nz(rng), nz(rng)};
            return fitMetricGauge(ref, 10.0);
        };
        MetricFit loud = run(0.08);
        MetricFit quiet = run(0.0005);
        printf("  T9: thin arc rot unc %.3f deg (loud) vs %.4f deg (quiet)\n",
               loud.rot_unc_deg, quiet.rot_unc_deg);
        check(!loud.ok && loud.reason == MetricFail::Rotation, "T9: noisy thin arc -> Rotation");
        check(loud.rot_unc_deg > 5.0, "T9: reported rotation uncertainty above the gate");
        check(quiet.ok, "T9: the same arc, 100x quieter, passes");
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
        MetricFit a = fitMetricGauge(ref, 0.1);
        MetricFit b = fitMetricGauge(ref, 0.1);
        bool same = a.ok == b.ok && a.reason == b.reason && a.T.scale == b.T.scale &&
                    a.T.t.x == b.T.t.x && a.T.t.y == b.T.t.y && a.T.t.z == b.T.t.z &&
                    a.inliers == b.inliers && a.inlier_mask == b.inlier_mask &&
                    a.rms == b.rms && a.scale_unc == b.scale_unc;
        for (int i = 0; i < 9; i++) same = same && a.T.R[i] == b.T.R[i];
        printf("  T10: locked onto scale %.4f with %d/%d inliers\n", a.T.scale, a.inliers, a.n);
        check(same, "T10: two runs agree bit for bit");
        check(a.inliers == 30, "T10: exactly one of the two consensus sets is found");
        const bool bimodal = std::fabs(a.T.scale / A.scale - 1.0) < 1e-9 ||
                             std::fabs(a.T.scale / B.scale - 1.0) < 1e-9;
        check(bimodal, "T10: the fixture really has two answers to choose between");
    }

    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdMetricSelftest); }
