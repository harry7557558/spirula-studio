// MarkerTriangulate.cpp -- 3D triangulation of marker observations.
#include "data/MarkerTriangulate.h"

#include "data/CameraMath.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace spirula {

namespace {

void cross_prod(const double a[3], const double b[3], double out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

double norm3(const double v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

}  // namespace

bool intersect_rays(const double* origins, const double* dirs, int n_rays,
                    double out_point[3], double* out_residual) {
    if (n_rays < 2 || !origins || !dirs || !out_point) return false;

    double A[9] = {0.0};
    double b[3] = {0.0};

    for (int i = 0; i < n_rays; i++) {
        const double* o = origins + i * 3;
        const double* d = dirs + i * 3;
        const double dx = d[0], dy = d[1], dz = d[2];

        const double p00 = 1.0 - dx * dx;
        const double p01 = -dx * dy;
        const double p02 = -dx * dz;
        const double p11 = 1.0 - dy * dy;
        const double p12 = -dy * dz;
        const double p22 = 1.0 - dz * dz;

        A[0] += p00; A[1] += p01; A[2] += p02;
        A[3] += p01; A[4] += p11; A[5] += p12;
        A[6] += p02; A[7] += p12; A[8] += p22;

        b[0] += p00 * o[0] + p01 * o[1] + p02 * o[2];
        b[1] += p01 * o[0] + p11 * o[1] + p12 * o[2];
        b[2] += p02 * o[0] + p12 * o[1] + p22 * o[2];
    }

    const double det = A[0] * (A[4] * A[8] - A[5] * A[7]) -
                       A[1] * (A[3] * A[8] - A[5] * A[6]) +
                       A[2] * (A[3] * A[7] - A[4] * A[6]);
    if (std::fabs(det) < 1e-11) return false;

    const double inv_det = 1.0 / det;
    const double invA[9] = {
        (A[4] * A[8] - A[5] * A[7]) * inv_det,
        (A[2] * A[7] - A[1] * A[8]) * inv_det,
        (A[1] * A[5] - A[2] * A[4]) * inv_det,
        (A[5] * A[6] - A[3] * A[8]) * inv_det,
        (A[0] * A[8] - A[2] * A[6]) * inv_det,
        (A[2] * A[3] - A[0] * A[5]) * inv_det,
        (A[3] * A[7] - A[4] * A[6]) * inv_det,
        (A[1] * A[6] - A[0] * A[7]) * inv_det,
        (A[0] * A[4] - A[1] * A[3]) * inv_det,
    };

    out_point[0] = invA[0] * b[0] + invA[1] * b[1] + invA[2] * b[2];
    out_point[1] = invA[3] * b[0] + invA[4] * b[1] + invA[5] * b[2];
    out_point[2] = invA[6] * b[0] + invA[7] * b[1] + invA[8] * b[2];

    if (out_residual) {
        double sum_sq = 0.0;
        for (int i = 0; i < n_rays; i++) {
            const double* o = origins + i * 3;
            const double* d = dirs + i * 3;
            const double v[3] = {out_point[0] - o[0], out_point[1] - o[1], out_point[2] - o[2]};
            const double proj = v[0] * d[0] + v[1] * d[1] + v[2] * d[2];
            const double rx = v[0] - proj * d[0];
            const double ry = v[1] - proj * d[1];
            const double rz = v[2] - proj * d[2];
            sum_sq += rx * rx + ry * ry + rz * rz;
        }
        *out_residual = std::sqrt(sum_sq / (double)n_rays);
    }
    return true;
}

namespace {

bool camera_ray(const ParsedDataset& ds, uint32_t cam_idx, double px, double py,
                double out_origin[3], double out_dir[3]) {
    if (cam_idx >= (uint32_t)ds.num_cameras) return false;

    const float* c2w = &ds.c2w[cam_idx * 12];
    const float* intr = &ds.intrins[cam_idx * 4];
    const float fx = intr[0], fy = intr[1], cx = intr[2], cy = intr[3];
    if (fx <= 0 || fy <= 0) return false;

    const int model = ds.camera_models[cam_idx];
    const int tier = ds.camera_distortions[cam_idx];
    const float* dist = &ds.dist_coeffs[cam_idx * 8];

    const double u = (px - (double)cx) / (double)fx;
    const double v = (py - (double)cy) / (double)fy;

    double ray_cv[3];
    if (!camhost::generate_ray(u, v, model, tier, dist, ray_cv)) return false;

    const double ray_gl[3] = {ray_cv[0], -ray_cv[1], -ray_cv[2]};

    out_origin[0] = (double)c2w[3];
    out_origin[1] = (double)c2w[7];
    out_origin[2] = (double)c2w[11];

    out_dir[0] = (double)c2w[0] * ray_gl[0] + (double)c2w[1] * ray_gl[1] + (double)c2w[2] * ray_gl[2];
    out_dir[1] = (double)c2w[4] * ray_gl[0] + (double)c2w[5] * ray_gl[1] + (double)c2w[6] * ray_gl[2];
    out_dir[2] = (double)c2w[8] * ray_gl[0] + (double)c2w[9] * ray_gl[1] + (double)c2w[10] * ray_gl[2];

    const double len = norm3(out_dir);
    if (len < 1e-12) return false;
    out_dir[0] /= len; out_dir[1] /= len; out_dir[2] /= len;
    return true;
}

double project_error(const ParsedDataset& ds, uint32_t cam_idx, const double pt[3],
                     double px, double py) {
    if (cam_idx >= (uint32_t)ds.num_cameras) return 1e9;
    const float* c2w = &ds.c2w[cam_idx * 12];
    const double delta[3] = {pt[0] - (double)c2w[3], pt[1] - (double)c2w[7], pt[2] - (double)c2w[11]};

    const double x_gl = (double)c2w[0] * delta[0] + (double)c2w[4] * delta[1] + (double)c2w[8] * delta[2];
    const double y_gl = (double)c2w[1] * delta[0] + (double)c2w[5] * delta[1] + (double)c2w[9] * delta[2];
    const double z_gl = (double)c2w[2] * delta[0] + (double)c2w[6] * delta[1] + (double)c2w[10] * delta[2];

    const double x_cv[3] = {x_gl, -y_gl, -z_gl};
    if (x_cv[2] <= 1e-5) return 1e9;

    const int model = ds.camera_models[cam_idx];
    const int tier = ds.camera_distortions[cam_idx];
    const float* dist = &ds.dist_coeffs[cam_idx * 8];
    const float* intr = &ds.intrins[cam_idx * 4];

    double norm_out[2];
    if (!camhost::project_ray(x_cv, model, tier, dist, norm_out)) return 1e9;

    const double proj_x = norm_out[0] * (double)intr[0] + (double)intr[2];
    const double proj_y = norm_out[1] * (double)intr[1] + (double)intr[3];
    const double dx = proj_x - px, dy = proj_y - py;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

bool triangulate_marker(int marker_id,
                        const std::vector<MarkerObservation>& obs,
                        const ParsedDataset& ds,
                        Marker3D& out, MarkerFamily family) {
    std::vector<const MarkerObservation*> matching;
    for (const auto& o : obs) {
        if (o.det.id == marker_id && (family == MarkerFamily::Auto || o.det.family == family))
            matching.push_back(&o);
    }
    if (matching.size() < 2) return false;
    for (const auto* o : matching)
        if (o->det.family != matching.front()->det.family) return false;

    std::vector<double> c_origins, c_dirs;
    std::vector<std::vector<double>> corn_origins(4), corn_dirs(4);
    std::vector<uint32_t> valid_cams;

    for (const auto* o : matching) {
        double o_ray[3], d_ray[3];
        if (!camera_ray(ds, o->image_id, o->det.center[0], o->det.center[1], o_ray, d_ray))
            continue;
        c_origins.insert(c_origins.end(), o_ray, o_ray + 3);
        c_dirs.insert(c_dirs.end(), d_ray, d_ray + 3);
        valid_cams.push_back(o->image_id);

        for (int k = 0; k < 4; k++) {
            double co[3], cd[3];
            if (camera_ray(ds, o->image_id, o->det.corners[k][0], o->det.corners[k][1], co, cd)) {
                corn_origins[k].insert(corn_origins[k].end(), co, co + 3);
                corn_dirs[k].insert(corn_dirs[k].end(), cd, cd + 3);
            }
        }
    }

    const int valid_count = (int)valid_cams.size();
    if (valid_count < 2) return false;

    out.id = marker_id;
    out.family = matching.front()->det.family;
    out.num_views = valid_count;

    if (!intersect_rays(c_origins.data(), c_dirs.data(), valid_count, out.center))
        return false;

    for (int k = 0; k < 4; k++) {
        const int m = (int)corn_origins[k].size() / 3;
        if (m >= 2) {
            intersect_rays(corn_origins[k].data(), corn_dirs[k].data(), m, out.corners[k]);
        }
    }

    const double v1[3] = {out.corners[1][0] - out.corners[0][0],
                          out.corners[1][1] - out.corners[0][1],
                          out.corners[1][2] - out.corners[0][2]};
    const double v3[3] = {out.corners[3][0] - out.corners[0][0],
                          out.corners[3][1] - out.corners[0][1],
                          out.corners[3][2] - out.corners[0][2]};
    cross_prod(v1, v3, out.normal);
    const double nlen = norm3(out.normal);
    if (nlen > 1e-12) {
        out.normal[0] /= nlen; out.normal[1] /= nlen; out.normal[2] /= nlen;
    } else {
        out.normal[0] = 0.0; out.normal[1] = 0.0; out.normal[2] = 1.0;
    }

    double err_sum = 0.0;
    int err_count = 0;
    for (size_t i = 0; i < matching.size(); i++) {
        const auto* o = matching[i];
        const double err = project_error(ds, o->image_id, out.center, o->det.center[0], o->det.center[1]);
        if (err < 1e8) {
            err_sum += err;
            err_count++;
        }
    }
    if (err_count < 2 || err_count != valid_count) return false;
    out.reproj_error_px = err_sum / (double)err_count;
    return true;
}

std::vector<Marker3D> triangulate_markers(const std::vector<MarkerObservation>& obs,
                                          const ParsedDataset& ds) {
    std::map<std::pair<MarkerFamily, int>, int> counts;
    for (const auto& o : obs) counts[{o.det.family, o.det.id}]++;

    std::vector<Marker3D> res;
    for (const auto& kv : counts) {
        if (kv.second < 2) continue;
        Marker3D m;
        if (triangulate_marker(kv.first.second, obs, ds, m, kv.first.first)) {
            res.push_back(m);
        }
    }
    return res;
}

bool save_markers_json(const std::string& path, const std::vector<Marker3D>& markers) {
    JsonWriter w;
    w.object();
    w.key("markers").array();
    for (const auto& m : markers) {
        w.object();
        w.field("id", m.id);
        w.field("family", marker_family_name(m.family));
        w.field("num_views", m.num_views);
        w.field("reproj_error_px", m.reproj_error_px);
        w.key("center").array();
        for (int k = 0; k < 3; k++) w.raw(json_number_exact(m.center[k]));
        w.end();
        w.key("normal").array();
        for (int k = 0; k < 3; k++) w.raw(json_number_exact(m.normal[k]));
        w.end();
        w.key("corners").array();
        for (int c = 0; c < 4; c++) {
            w.array();
            for (int k = 0; k < 3; k++) w.raw(json_number_exact(m.corners[c][k]));
            w.end();
        }
        w.end();
        w.end();
    }
    w.end();
    w.end();

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    const std::string s = w.str();
    const size_t written = std::fwrite(s.data(), 1, s.size(), f);
    std::fclose(f);
    return written == s.size();
}

bool load_markers_json(const std::string& path, std::vector<Marker3D>& markers) {
    try {
        const JsonValue root = json_parse_file(path);
        const JsonValue* arr = root.find("markers");
        if (!arr || !arr->is_array()) return false;
        std::vector<Marker3D> loaded;
        for (const auto& item : arr->arr) {
            Marker3D m;
            if (const auto* f = item.find("family")) {
                if (!parse_marker_family(f->as_string(), m.family) || m.family == MarkerFamily::Auto)
                    return false;
            }
            const JsonValue* v_id = item.find("id");
            if (!v_id || v_id->type != JsonValue::Type::Number ||
                !std::isfinite(v_id->num) || v_id->num < 0 || v_id->num > 65535 ||
                v_id->num != std::floor(v_id->num)) return false;
            m.id = (int)v_id->num;
            const JsonValue* v_nv = item.find("num_views");
            m.num_views = v_nv ? (int)v_nv->as_int(0) : 0;
            m.reproj_error_px = item.get_double("reproj_error_px", 0.0);
            const JsonValue* c = item.find("center");
            if (!c || !c->is_array() || c->arr.size() != 3) return false;
            for (int k = 0; k < 3; k++) {
                m.center[k] = c->arr[k].as_double(std::numeric_limits<double>::quiet_NaN());
                if (!std::isfinite(m.center[k])) return false;
            }
            const JsonValue* norm = item.find("normal");
            if (norm && norm->is_array() && norm->arr.size() == 3) {
                for (int k = 0; k < 3; k++) m.normal[k] = norm->arr[k].as_double();
            }
            const JsonValue* corn = item.find("corners");
            if (corn && corn->is_array() && corn->arr.size() == 4) {
                for (int i = 0; i < 4; i++) {
                    if (corn->arr[i].is_array() && corn->arr[i].arr.size() == 3) {
                        for (int k = 0; k < 3; k++) m.corners[i][k] = corn->arr[i].arr[k].as_double();
                    }
                }
            }
            loaded.push_back(m);
        }
        markers = std::move(loaded);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace spirula

