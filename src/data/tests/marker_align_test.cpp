// marker_align_test.cpp -- unit tests for marker triangulation, Sim(3) alignment and ROI crop.
#include "data/MarkerAlign.h"
#include "data/MarkerCropBox.h"
#include "data/MarkerDetector.h"
#include "data/MarkerTriangulate.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

}  // namespace

int main() {
    using namespace spirula;

    // ---- 1. Ray intersection ----
    {
        const double gt[3] = {1.5, -2.0, 3.25};
        const double o1[3] = {0.0, 0.0, 0.0};
        const double o2[3] = {5.0, 2.0, 1.0};
        const double o3[3] = {-3.0, 4.0, 6.0};

        auto unit_dir = [&](const double o[3], double d[3]) {
            d[0] = gt[0] - o[0]; d[1] = gt[1] - o[1]; d[2] = gt[2] - o[2];
            const double l = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
            d[0] /= l; d[1] /= l; d[2] /= l;
        };

        double d1[3], d2[3], d3[3];
        unit_dir(o1, d1);
        unit_dir(o2, d2);
        unit_dir(o3, d3);

        const double origins[9] = {o1[0], o1[1], o1[2], o2[0], o2[1], o2[2], o3[0], o3[1], o3[2]};
        const double dirs[9] = {d1[0], d1[1], d1[2], d2[0], d2[1], d2[2], d3[0], d3[1], d3[2]};

        double est[3], res = 0.0;
        const bool ok = intersect_rays(origins, dirs, 3, est, &res);
        const double err = std::sqrt((est[0]-gt[0])*(est[0]-gt[0]) +
                                     (est[1]-gt[1])*(est[1]-gt[1]) +
                                     (est[2]-gt[2])*(est[2]-gt[2]));

        check(ok, "intersect_rays succeeds on 3 rays");
        check(err < 1e-9, "intersect_rays recovers exact 3D position");
        check(res < 1e-9, "intersect_rays reports near-zero residual");
    }

    // ---- 2. Umeyama Sim3 SVD solver ----
    {
        const std::vector<std::array<double, 3>> src = {
            {0.0, 0.0, 0.0},
            {2.0, 0.0, 0.0},
            {0.0, 3.0, 0.0},
            {1.0, 1.0, 2.0}
        };

        const double axis[3] = {1.0 / std::sqrt(14.0), 2.0 / std::sqrt(14.0), 3.0 / std::sqrt(14.0)};
        const double pivot[3] = {0.0, 0.0, 0.0};
        const double scale_gt = 2.5;
        const double trans_gt[3] = {10.0, -5.0, 7.5};

        Sim3 T_gt = Sim3::rotation_about(axis, 0.75, pivot);
        T_gt.s = scale_gt;
        for (int k = 0; k < 3; k++) T_gt.t[k] = trans_gt[k];

        std::vector<std::array<double, 3>> dst(src.size());
        for (size_t i = 0; i < src.size(); i++) {
            double p[3];
            T_gt.apply(src[i].data(), p);
            dst[i] = {p[0], p[1], p[2]};
        }

        Sim3 T_est;
        const bool ok = estimate_sim3_umeyama(src, dst, T_est);
        check(ok, "estimate_sim3_umeyama succeeds on 4 non-coplanar points");

        const double s_err = std::fabs(T_est.s - T_gt.s);
        check(s_err < 1e-9, "estimate_sim3_umeyama recovers exact scale");

        double max_r_err = 0.0;
        for (int i = 0; i < 9; i++) {
            max_r_err = std::max(max_r_err, std::fabs(T_est.R[i] - T_gt.R[i]));
        }
        check(max_r_err < 1e-8, "estimate_sim3_umeyama recovers exact rotation");

        double max_t_err = 0.0;
        for (int k = 0; k < 3; k++) {
            max_t_err = std::max(max_t_err, std::fabs(T_est.t[k] - T_gt.t[k]));
        }
        check(max_t_err < 1e-8, "estimate_sim3_umeyama recovers exact translation");
    }

    // ---- 3. Marker system alignment (3 markers) ----
    {
        Marker3D m0, m1, m2;
        m0.id = 10; m0.center[0] = 1.0; m0.center[1] = 1.0; m0.center[2] = 0.0;
        m1.id = 20; m1.center[0] = 4.0; m1.center[1] = 5.0; m1.center[2] = 0.0;
        m2.id = 30; m2.center[0] = 1.0; m2.center[1] = 4.0; m2.center[2] = 0.0;

        MarkerSystemConfig cfg;
        cfg.three_markers.origin_id = 10;
        cfg.three_markers.x_axis_id = 20;
        cfg.three_markers.xy_plane_id = 30;
        cfg.three_markers.dist_x = 1.0;

        Sim3 T;
        const bool ok = solve_marker_system({m0, m1, m2}, cfg, T);
        check(ok, "solve_marker_system succeeds on 3 markers");

        double p0[3], p1[3], p2[3];
        T.apply(m0.center, p0);
        T.apply(m1.center, p1);
        T.apply(m2.center, p2);

        const double p0_dist = std::sqrt(p0[0]*p0[0] + p0[1]*p0[1] + p0[2]*p0[2]);
        check(p0_dist < 1e-8, "origin marker lands at (0, 0, 0)");

        const double p1_err = std::sqrt((p1[0]-1.0)*(p1[0]-1.0) + p1[1]*p1[1] + p1[2]*p1[2]);
        check(p1_err < 1e-8, "x-axis marker lands at (dist_x, 0, 0)");

        check(std::fabs(p2[2]) < 1e-8, "plane marker lands at z=0");
        check(p2[1] > 0.0, "plane marker lands on positive y side");
    }

    // ---- 4. Automatic crop box ----
    {
        Marker3D m0, m1;
        m0.id = 0; m0.center[0] = 0.0; m0.center[1] = 0.0; m0.center[2] = 0.0;
        m1.id = 1; m1.center[0] = 1.0; m1.center[1] = 1.0; m1.center[2] = 0.0;

        MarkerCropOptions opt;
        opt.pad_xy = 0.1;
        opt.z_min = -0.05;
        opt.z_max = 0.80;

        RoiDocument doc = compute_marker_crop_box({m0, m1}, opt);
        check(doc.shapes.size() == 1, "crop box produces exactly 1 shape");

        auto reg = roi_region(doc);
        check(reg != nullptr, "crop box builds valid region");
        check(reg->inside(0.5, 0.5, 0.2), "center point inside crop box");
        check(reg->inside(-0.05, 0.0, 0.0), "padded origin inside crop box");
        check(!reg->inside(2.0, 2.0, 0.2), "far point outside crop box");
        check(!reg->inside(0.5, 0.5, 1.2), "point above z_max outside crop box");

        const fs::path tmp_dir = fs::temp_directory_path() /
            ("marker_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tmp_dir);

        std::string out_path, err;
        const bool saved = save_marker_crop_roi(tmp_dir.string(), doc, "test_crop.json", out_path, err);
        check(saved, "save_marker_crop_roi writes JSON file");
        check(fs::exists(out_path), "saved ROI file exists on disk");

        fs::remove_all(tmp_dir);
    }

    // ---- 5. Detector smoke test ----
    {
        MarkerDetector detector(MarkerFamily::Tag36h10, 1, 1.0);
        std::vector<uint8_t> blank(64 * 64, 128);
        auto detections = detector.detect_grayscale(blank.data(), 64, 64);
        check(detections.empty(), "blank image yields 0 marker detections");
    }

    if (g_failures == 0) std::printf("marker_align_test: ALL TESTS PASSED\n");
    return g_failures ? 1 : 0;
}

