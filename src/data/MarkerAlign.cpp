// MarkerAlign.cpp -- coordinate system alignment and metric scaling from markers.
#include "data/MarkerAlign.h"

#include "checkpoint/SplatPly.h"
#include "checkpoint/SplatTransform.h"
#include "common/matd.h"
#include "data/SparseEdit.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

namespace spirula {

namespace {

double norm3(const double v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

double det3(const matd_t* m) {
    return MATD_EL(m, 0, 0) * (MATD_EL(m, 1, 1) * MATD_EL(m, 2, 2) - MATD_EL(m, 1, 2) * MATD_EL(m, 2, 1)) -
           MATD_EL(m, 0, 1) * (MATD_EL(m, 1, 0) * MATD_EL(m, 2, 2) - MATD_EL(m, 1, 2) * MATD_EL(m, 2, 0)) +
           MATD_EL(m, 0, 2) * (MATD_EL(m, 1, 0) * MATD_EL(m, 2, 1) - MATD_EL(m, 1, 1) * MATD_EL(m, 2, 0));
}

}  // namespace

bool estimate_sim3_umeyama(const std::vector<std::array<double, 3>>& src,
                           const std::vector<std::array<double, 3>>& dst,
                           Sim3& out_sim) {
    const size_t n = src.size();
    if (n < 3 || dst.size() != n) return false;

    std::array<double, 3> ms{0.0, 0.0, 0.0}, md{0.0, 0.0, 0.0};
    for (size_t i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            ms[k] += src[i][k];
            md[k] += dst[i][k];
        }
    }
    const double inv_n = 1.0 / (double)n;
    for (int k = 0; k < 3; k++) {
        ms[k] *= inv_n;
        md[k] *= inv_n;
    }

    double var_s = 0.0;
    matd_t* sigma = matd_create(3, 3);
    for (size_t i = 0; i < n; i++) {
        double a[3], b[3];
        for (int k = 0; k < 3; k++) {
            a[k] = src[i][k] - ms[k];
            b[k] = dst[i][k] - md[k];
            var_s += a[k] * a[k];
        }
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                MATD_EL(sigma, r, c) += b[r] * a[c];
            }
        }
    }
    var_s *= inv_n;
    if (var_s < 1e-12) {
        matd_destroy(sigma);
        return false;
    }

    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            MATD_EL(sigma, r, c) *= inv_n;

    matd_svd_t svd = matd_svd(sigma);
    matd_destroy(sigma);

    const double detU = det3(svd.U);
    const double detV = det3(svd.V);
    const double flip = (detU * detV < 0.0) ? -1.0 : 1.0;

    double R[9];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            R[r * 3 + c] = MATD_EL(svd.U, r, 0) * MATD_EL(svd.V, c, 0) +
                           MATD_EL(svd.U, r, 1) * MATD_EL(svd.V, c, 1) +
                           MATD_EL(svd.U, r, 2) * flip * MATD_EL(svd.V, c, 2);
        }
    }

    const double s0 = MATD_EL(svd.S, 0, 0);
    const double s1 = MATD_EL(svd.S, 1, 1);
    const double s2 = MATD_EL(svd.S, 2, 2);
    const double trace = s0 + s1 + flip * s2;
    const double scale = trace / var_s;

    matd_destroy(svd.U);
    matd_destroy(svd.S);
    matd_destroy(svd.V);

    if (scale <= 1e-12 || !std::isfinite(scale)) return false;

    out_sim.s = scale;
    for (int i = 0; i < 9; i++) out_sim.R[i] = R[i];

    double moved_ms[3];
    out_sim.rotate(ms.data(), moved_ms);
    for (int k = 0; k < 3; k++) {
        out_sim.t[k] = md[k] - scale * moved_ms[k];
    }
    out_sim.orthonormalize();
    return true;
}

bool solve_marker_system(const std::vector<Marker3D>& markers,
                         const MarkerSystemConfig& cfg,
                         Sim3& out_sim) {
    std::map<int, const Marker3D*> by_id;
    for (const auto& m : markers) {
        if (!by_id.emplace(m.id, &m).second) return false;
    }

    if (cfg.has_three_markers()) {
        const auto it0 = by_id.find(cfg.three_markers.origin_id);
        const auto it1 = by_id.find(cfg.three_markers.x_axis_id);
        const auto it2 = by_id.find(cfg.three_markers.xy_plane_id);
        if (it0 == by_id.end() || it1 == by_id.end() || it2 == by_id.end()) return false;

        const double* p0 = it0->second->center;
        const double* p1 = it1->second->center;
        const double* p2 = it2->second->center;

        const double v01[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        const double lx = norm3(v01);
        if (lx < 1e-6) return false;

        const double ux[3] = {v01[0] / lx, v01[1] / lx, v01[2] / lx};
        const double dx = (cfg.three_markers.dist_x > 0.0) ? cfg.three_markers.dist_x : lx;
        const double s_target = dx / lx;

        const double v02[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        const double dot_x = v02[0] * ux[0] + v02[1] * ux[1] + v02[2] * ux[2];

        const double v_perp[3] = {v02[0] - dot_x * ux[0],
                                  v02[1] - dot_x * ux[1],
                                  v02[2] - dot_x * ux[2]};
        const double ly = norm3(v_perp);
        if (ly < 1e-6) return false;

        const std::vector<std::array<double, 3>> src = {
            {p0[0], p0[1], p0[2]},
            {p1[0], p1[1], p1[2]},
            {p2[0], p2[1], p2[2]}
        };
        const std::vector<std::array<double, 3>> dst = {
            {0.0, 0.0, 0.0},
            {dx, 0.0, 0.0},
            {dot_x * s_target, ly * s_target, 0.0}
        };
        return estimate_sim3_umeyama(src, dst, out_sim);
    }

    if (!cfg.gcp_table.empty()) {
        std::vector<std::array<double, 3>> src, dst;
        for (const auto& g : cfg.gcp_table) {
            const auto it = by_id.find(g.id);
            if (it == by_id.end()) continue;
            src.push_back({it->second->center[0], it->second->center[1], it->second->center[2]});
            dst.push_back({g.pos[0], g.pos[1], g.pos[2]});
        }
        if (src.size() < 3) return false;
        return estimate_sim3_umeyama(src, dst, out_sim);
    }

    return false;
}

Marker3Def suggest_marker_system(const std::vector<Marker3D>& markers, const double up[3]) {
    Marker3Def out;
    if (markers.empty()) return out;
    out.origin_id = markers[0].id;
    if (markers.size() < 2) return out;
    out.x_axis_id = markers[1].id;
    if (markers.size() < 3) return out;
    size_t a = 0, b = 1, c = 2;
    double longest = 0;
    for (size_t i = 0; i < markers.size(); ++i) for (size_t j = i + 1; j < markers.size(); ++j) {
        double length = 0;
        for (int k = 0; k < 3; ++k) {
            const double d = markers[j].center[k] - markers[i].center[k];
            length += d * d;
        }
        if (length > longest) { longest = length; a = i; b = j; }
    }
    double area = -1;
    for (size_t i = 0; i < markers.size(); ++i) {
        if (i == a || i == b) continue;
        double u[3], v[3];
        for (int k = 0; k < 3; ++k) {
            u[k] = markers[b].center[k] - markers[a].center[k];
            v[k] = markers[i].center[k] - markers[a].center[k];
        }
        const double cross[3] = {u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]};
        const double value = norm3(cross);
        if (value > area) { area = value; c = i; }
    }
    const size_t triangle[3] = {a, b, c};
    double best = -1e30;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
        if (i == j) continue;
        MarkerSystemConfig config;
        config.three_markers.origin_id = markers[triangle[i]].id;
        config.three_markers.x_axis_id = markers[triangle[j]].id;
        config.three_markers.xy_plane_id = markers[triangle[3-i-j]].id;
        Sim3 frame;
        if (!solve_marker_system(markers, config, frame)) continue;
        const double score = 4 * (frame.R[6]*up[0] + frame.R[7]*up[1] + frame.R[8]*up[2]) + frame.R[0] + frame.R[4];
        if (score > best) { best = score; out = config.three_markers; }
    }
    if (out.xy_plane_id < 0) out.xy_plane_id = markers[c].id;
    return out;
}

bool apply_marker_alignment(const std::string& dataset_dir,
                            const Sim3& T,
                            std::string& error) {
    try {
        SparseKeep keep;
        sparse_write_filtered(dataset_dir, keep, &T, nullptr);

        auto write_gauge = [](const fs::path& p) {
            std::ofstream f(p);
            if (f) f << "oriented 1\nmetric 1\n";
        };

        const fs::path root(dataset_dir);
        write_gauge(root / "gauge.txt");
        if (fs::exists(root / "sparse" / "0")) {
            write_gauge(root / "sparse" / "0" / "gauge.txt");
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool apply_marker_alignment_splat(const std::string& in_splat_path,
                                  const std::string& out_splat_path,
                                  const Sim3& T,
                                  std::string& error) {
    try {
        SplatCloud cloud = read_splat_ply(in_splat_path, true);
        SplatTransform xform(T, cloud.basis());
        write_splat_ply(cloud, out_splat_path, nullptr, &xform);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

}  // namespace spirula

