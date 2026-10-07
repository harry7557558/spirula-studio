// RoiEditor.cpp -- see RoiEditor.h.

#include "app/gui/RoiEditor.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "app/webviewer/RegionOverlay.h"
#include "data/RegionMesh.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/MaskEdit.h"
#include "i18n/catalog/Partition.h"
#include "i18n/catalog/Roi.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <cstddef>
#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
namespace rmsg = spirula::i18n::msg::roi;
namespace gmsg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace kmsg = spirula::i18n::msg::maskedit;
namespace pmsg = spirula::i18n::msg::partition;

using spirula::i18n::format;
using spirula::RoiOp;
using spirula::RoiShape;
using spirula::RoiShapeKind;
using spirula::Sim3;

namespace gui {

namespace {

constexpr double kPi = 3.14159265358979323846;
// Points the start buttons and the outline's height look at: plenty for a
// percentile, and a fit stays instant on a ten-million-point cloud.
constexpr int64_t kFitPoints = 200000;
constexpr int kMeshCells = 96;

constexpr ImU32 kAxisCol[3] = {IM_COL32(250, 51, 79, 255), IM_COL32(140, 219, 0, 255),
                               IM_COL32(41, 140, 250, 255)};
constexpr ImU32 kHot = IM_COL32(255, 235, 90, 255);

using V3 = std::array<double, 3>;

double dot3(const double* a, const double* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void normalize3(double* a) {
    const double n = std::sqrt(dot3(a, a));
    if (n > 1e-300)
        for (int k = 0; k < 3; k++) a[k] /= n;
}

ImU32 op_color(RoiOp op, int alpha) {
    switch (op) {
        case RoiOp::Add: return IM_COL32(110, 220, 120, alpha);
        case RoiOp::Subtract: return IM_COL32(245, 95, 90, alpha);
        case RoiOp::Intersect: return IM_COL32(240, 190, 70, alpha);
    }
    return IM_COL32(200, 200, 200, alpha);
}

const spirula::i18n::Msg& kind_label(RoiShapeKind k) {
    switch (k) {
        case RoiShapeKind::Box: return rmsg::kind_box;
        case RoiShapeKind::Ellipsoid: return rmsg::kind_ellipsoid;
        case RoiShapeKind::Cylinder: return rmsg::kind_cylinder;
        case RoiShapeKind::Prism: return rmsg::kind_prism;
    }
    return rmsg::kind_box;
}

V3 world_of(const RoiShape& s, double x, double y, double z) {
    V3 p;
    for (int k = 0; k < 3; k++) p[k] = s.origin[k] + s.R[k] * x + s.R[3 + k] * y + s.R[6 + k] * z;
    return p;
}

void local_of(const RoiShape& s, const double p[3], double q[3]) {
    const double d[3] = {p[0] - s.origin[0], p[1] - s.origin[1], p[2] - s.origin[2]};
    for (int r = 0; r < 3; r++) q[r] = dot3(&s.R[r * 3], d);
}

// The middle of the side at `sign` along `axis`, in the shape's frame.
V3 side_point(const RoiShape& s, int axis, int sign) {
    double lo[3], hi[3];
    spirula::roi_extents(s, lo, hi);
    V3 q;
    for (int k = 0; k < 3; k++) q[k] = 0.5 * (lo[k] + hi[k]);
    q[axis] = sign > 0 ? hi[axis] : lo[axis];
    return q;
}

void local_dir(const RoiShape& s, const double v[3], double q[3]) {
    for (int r = 0; r < 3; r++) q[r] = dot3(&s.R[r * 3], v);
}

bool proj(const ViewProjection& cam, const double p[3], ImVec2& out) {
    const float q[3] = {(float)p[0], (float)p[1], (float)p[2]};
    float x, y, depth;
    if (!cam.project(q, x, y, depth)) return false;
    out = ImVec2(x, y);
    return true;
}

bool pointer_ray(const ViewProjection& cam, float x, float y, double ro[3], double rd[3]) {
    float o[3], d[3];
    if (!cam.unproject(x, y, o, d)) return false;
    for (int k = 0; k < 3; k++) {
        ro[k] = o[k];
        rd[k] = d[k];
    }
    normalize3(rd);
    return true;
}

// Pixels one unit spans at `p`, across the view.
double pixels_per_unit(const ViewProjection& cam, const double p[3]) {
    const double right[3] = {cam.w2c[0], cam.w2c[1], cam.w2c[2]};
    const double d[3] = {p[0] - cam.eye[0], p[1] - cam.eye[1], p[2] - cam.eye[2]};
    const double h = std::max(std::sqrt(dot3(d, d)) * 1e-3, 1e-9);
    const double q[3] = {p[0] + right[0] * h, p[1] + right[1] * h, p[2] + right[2] * h};
    ImVec2 a, b;
    if (!proj(cam, p, a) || !proj(cam, q, b)) return 0.0;
    return std::hypot((double)(b.x - a.x), (double)(b.y - a.y)) / h;
}

// The parameter along p + t d (d unit) nearest the pointer's ray.
bool line_param(const ViewProjection& cam, float x, float y, const double p[3], const double d[3],
                double& t) {
    double ro[3], rd[3];
    if (!pointer_ray(cam, x, y, ro, rd)) return false;
    const double w0[3] = {p[0] - ro[0], p[1] - ro[1], p[2] - ro[2]};
    const double b = dot3(d, rd), dd = dot3(d, w0), e = dot3(rd, w0);
    const double denom = 1.0 - b * b;
    if (denom < 1e-6) return false;
    t = (b * e - dd) / denom;
    return true;
}

bool ray_plane(const ViewProjection& cam, float x, float y, const double point[3],
               const double n[3], double out[3]) {
    double ro[3], rd[3];
    if (!pointer_ray(cam, x, y, ro, rd)) return false;
    const double denom = dot3(rd, n);
    if (std::fabs(denom) < 1e-4) return false;
    const double w[3] = {point[0] - ro[0], point[1] - ro[1], point[2] - ro[2]};
    const double t = dot3(w, n) / denom;
    if (!(t > 0.0)) return false;
    for (int k = 0; k < 3; k++) out[k] = ro[k] + t * rd[k];
    return true;
}

double seg_distance(ImVec2 p, ImVec2 a, ImVec2 b) {
    const double vx = b.x - a.x, vy = b.y - a.y, wx = p.x - a.x, wy = p.y - a.y;
    const double l2 = vx * vx + vy * vy;
    const double t = l2 > 1e-12 ? std::clamp((wx * vx + wy * vy) / l2, 0.0, 1.0) : 0.0;
    return std::hypot(wx - t * vx, wy - t * vy);
}

// The first surface the ray enters, from outside the shape only: a shape the
// camera stands in would otherwise take every click.
bool ray_entry(const RoiShape& s, const double ro[3], const double rd[3], double& t_hit) {
    double o[3], d[3], lo[3], hi[3], h[3];
    local_of(s, ro, o);
    local_dir(s, rd, d);
    spirula::roi_extents(s, lo, hi);
    // About the middle of the sides; a prism's polygon stays about the pivot.
    for (int k = 0; k < 3; k++) {
        h[k] = 0.5 * (hi[k] - lo[k]);
        if (s.kind != RoiShapeKind::Prism || k == 2) o[k] -= 0.5 * (lo[k] + hi[k]);
    }
    switch (s.kind) {
        case RoiShapeKind::Box: {
            double t0 = -1e300, t1 = 1e300;
            for (int k = 0; k < 3; k++) {
                if (std::fabs(d[k]) < 1e-12) {
                    if (std::fabs(o[k]) > h[k]) return false;
                    continue;
                }
                double a = (-h[k] - o[k]) / d[k], b = (h[k] - o[k]) / d[k];
                if (a > b) std::swap(a, b);
                t0 = std::max(t0, a);
                t1 = std::min(t1, b);
            }
            if (t0 > t1 || !(t0 > 0)) return false;
            t_hit = t0;
            return true;
        }
        case RoiShapeKind::Ellipsoid: {
            const double so[3] = {o[0] / h[0], o[1] / h[1], o[2] / h[2]};
            const double sd[3] = {d[0] / h[0], d[1] / h[1], d[2] / h[2]};
            const double a = dot3(sd, sd), b = 2 * dot3(so, sd), c = dot3(so, so) - 1;
            const double disc = b * b - 4 * a * c;
            if (c <= 0 || disc < 0 || a < 1e-300) return false;
            const double t = (-b - std::sqrt(disc)) / (2 * a);
            if (!(t > 0)) return false;
            t_hit = t;
            return true;
        }
        case RoiShapeKind::Cylinder: {
            const double ox = o[0] / h[0], oy = o[1] / h[1], dx = d[0] / h[0], dy = d[1] / h[1];
            double t0 = -1e300, t1 = 1e300;
            const double a = dx * dx + dy * dy, b = 2 * (ox * dx + oy * dy), c = ox * ox + oy * oy - 1;
            if (a < 1e-18) {
                if (c > 0) return false;
            } else {
                const double disc = b * b - 4 * a * c;
                if (disc < 0) return false;
                t0 = (-b - std::sqrt(disc)) / (2 * a);
                t1 = (-b + std::sqrt(disc)) / (2 * a);
            }
            if (std::fabs(d[2]) < 1e-12) {
                if (std::fabs(o[2]) > h[2]) return false;
            } else {
                double a2 = (-h[2] - o[2]) / d[2], b2 = (h[2] - o[2]) / d[2];
                if (a2 > b2) std::swap(a2, b2);
                t0 = std::max(t0, a2);
                t1 = std::min(t1, b2);
            }
            if (t0 > t1 || !(t0 > 0)) return false;
            t_hit = t0;
            return true;
        }
        case RoiShapeKind::Prism: {
            const size_t n = s.polygon.size() / 2;
            if (n < 3) return false;
            const double* poly = s.polygon.data();
            if (std::fabs(o[2]) <= h[2] && spirula::polygon_contains(poly, n, o[0], o[1])) return false;
            double best = 1e300;
            if (std::fabs(d[2]) > 1e-12)
                for (double z : {-h[2], h[2]}) {
                    const double t = (z - o[2]) / d[2];
                    if (t > 0 && t < best &&
                        spirula::polygon_contains(poly, n, o[0] + t * d[0], o[1] + t * d[1]))
                        best = t;
                }
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                const double ax = poly[j * 2], ay = poly[j * 2 + 1];
                const double ex = poly[i * 2] - ax, ey = poly[i * 2 + 1] - ay;
                const double den = d[0] * ey - d[1] * ex;
                if (std::fabs(den) < 1e-18) continue;
                const double wx = ax - o[0], wy = ay - o[1];
                const double t = (wx * ey - wy * ex) / den;
                const double u = (wx * d[1] - wy * d[0]) / den;
                if (t > 0 && t < best && u >= 0 && u <= 1 && std::fabs(o[2] + t * d[2]) <= h[2])
                    best = t;
            }
            if (best >= 1e300) return false;
            t_hit = best;
            return true;
        }
    }
    return false;
}

// The shape's wireframe as polylines, in the frame its centre is in.
std::vector<std::vector<V3>> outline(const RoiShape& s) {
    std::vector<std::vector<V3>> lines;
    double lo[3], hi[3], m[3], h[3];
    spirula::roi_extents(s, lo, hi);
    for (int k = 0; k < 3; k++) {
        m[k] = 0.5 * (lo[k] + hi[k]);
        h[k] = 0.5 * (hi[k] - lo[k]);
    }
    auto seg = [&](V3 a, V3 b, int n) {
        std::vector<V3> l;
        for (int i = 0; i <= n; i++) {
            const double t = (double)i / n;
            l.push_back(world_of(s, a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]),
                                 a[2] + t * (b[2] - a[2])));
        }
        lines.push_back(std::move(l));
    };
    auto ring = [&](int ax, int ay, double rx, double ry, int az, double z) {
        std::vector<V3> l;
        for (int i = 0; i <= 64; i++) {
            const double a = 2 * kPi * i / 64;
            double q[3];
            q[ax] = m[ax] + rx * std::cos(a);
            q[ay] = m[ay] + ry * std::sin(a);
            q[az] = z;
            l.push_back(world_of(s, q[0], q[1], q[2]));
        }
        lines.push_back(std::move(l));
    };
    switch (s.kind) {
        case RoiShapeKind::Box:
            for (int a = 0; a < 3; a++) {
                const int b = (a + 1) % 3, c = (a + 2) % 3;
                for (int sb : {-1, 1})
                    for (int sc : {-1, 1}) {
                        V3 p, q;
                        p[a] = lo[a];
                        q[a] = hi[a];
                        p[b] = q[b] = sb > 0 ? hi[b] : lo[b];
                        p[c] = q[c] = sc > 0 ? hi[c] : lo[c];
                        seg(p, q, 12);
                    }
            }
            break;
        case RoiShapeKind::Ellipsoid:
            ring(0, 1, h[0], h[1], 2, m[2]);
            ring(1, 2, h[1], h[2], 0, m[0]);
            ring(0, 2, h[0], h[2], 1, m[1]);
            break;
        case RoiShapeKind::Cylinder:
            ring(0, 1, h[0], h[1], 2, lo[2]);
            ring(0, 1, h[0], h[1], 2, hi[2]);
            for (int i = 0; i < 4; i++) {
                const double a = kPi / 2 * i;
                const double x = m[0] + h[0] * std::cos(a), y = m[1] + h[1] * std::sin(a);
                seg(V3{x, y, lo[2]}, V3{x, y, hi[2]}, 8);
            }
            break;
        case RoiShapeKind::Prism: {
            const size_t n = s.polygon.size() / 2;
            for (size_t i = 0; i < n; i++) {
                const size_t j = (i + 1) % n;
                const double x0 = s.polygon[i * 2], y0 = s.polygon[i * 2 + 1];
                const double x1 = s.polygon[j * 2], y1 = s.polygon[j * 2 + 1];
                for (double z : {lo[2], hi[2]}) seg(V3{x0, y0, z}, V3{x1, y1, z}, 8);
                seg(V3{x0, y0, lo[2]}, V3{x0, y0, hi[2]}, 4);
            }
            break;
        }
    }
    return lines;
}

void draw_polyline(ImDrawList* dl, const ViewProjection& cam, ImVec2 o, const std::vector<V3>& l,
                   ImU32 col, float thick) {
    ImVec2 prev;
    bool have = false;
    for (const V3& p : l) {
        ImVec2 q;
        if (!proj(cam, p.data(), q)) {
            have = false;
            continue;
        }
        // An equirectangular view wraps; a segment across the seam is not one.
        if (have && std::fabs(q.x - prev.x) < 0.5f * cam.W)
            dl->AddLine(ImVec2(o.x + prev.x, o.y + prev.y), ImVec2(o.x + q.x, o.y + q.y), col, thick);
        prev = q;
        have = true;
    }
}

// A level frame turned by `yaw` about +Z; rows x, y, z.
void level_axes(double yaw, double R[9]) {
    const double c = std::cos(yaw), s = std::sin(yaw);
    const double M[9] = {c, s, 0, -s, c, 0, 0, 0, 1};
    std::copy(M, M + 9, R);
}

// Turn, tilt forward, tilt sideways: Q = Rz(turn) Ry(tilt) Rx(roll), Q's
// columns the shape's axes, i.e. Q = R^T.
void euler_of(const double R[9], double out[3]) {
    const double q00 = R[0], q10 = R[1], q20 = R[2], q21 = R[5], q22 = R[8];
    out[0] = std::atan2(q10, q00);
    out[1] = std::asin(std::clamp(-q20, -1.0, 1.0));
    out[2] = std::atan2(q21, q22);
}

void rotation_of(const double e[3], double R[9]) {
    const double cz = std::cos(e[0]), sz = std::sin(e[0]);
    const double cy = std::cos(e[1]), sy = std::sin(e[1]);
    const double cx = std::cos(e[2]), sx = std::sin(e[2]);
    const double Q[9] = {cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
                         sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
                         -sy,     cy * sx,                cy * cx};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) R[r * 3 + c] = Q[c * 3 + r];
}

double percentile(std::vector<double>& v, double q) {
    if (v.empty()) return 0.0;
    const size_t k = std::min(v.size() - 1, (size_t)(q * (double)(v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + (ptrdiff_t)k, v.end());
    return v[k];
}

// The points the fits look at, shared frame.
std::vector<double> fit_points(const std::vector<double>& raw, const Sim3& S) {
    const int64_t n = (int64_t)raw.size() / 3;
    const int64_t stride = std::max<int64_t>(1, n / kFitPoints);
    std::vector<double> out;
    out.reserve((size_t)(n / stride + 1) * 3);
    for (int64_t i = 0; i < n; i += stride) {
        double p[3];
        S.apply(&raw[(size_t)i * 3], p);
        out.insert(out.end(), p, p + 3);
    }
    return out;
}

std::string sanitize_name(std::string s) {
    for (char& c : s)
        if ((unsigned char)c < 32 || std::string("<>:\"/\\|?*").find(c) != std::string::npos) c = '_';
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

}  // namespace


RoiEditor::RoiEditor() = default;

RoiEditor::~RoiEditor() {
    if (_worker.joinable()) _worker.join();
    if (_mesh_job.valid()) _mesh_job.wait();
}

void RoiEditor::destroy_gl() { _view.destroy_gl(); }

// ===========================================================================
// Opening and loading
// ===========================================================================

void RoiEditor::open(const std::string& dataset_dir, Hooks hooks, const std::string& file) {
    if (_busy.load()) return;
    if (_open && dataset_dir == _dataset) {
        if (!file.empty() && file != _path) guard([this, file] { load_file(file); });
        return;
    }
    if (_open && dirty()) {
        guard([this, dataset_dir, hooks, file] { open(dataset_dir, hooks, file); });
        return;
    }
    if (_worker.joinable()) _worker.join();
    if (_mesh_job.valid()) _mesh_job.wait();
    _mesh_job = {};
    _open = true;
    _dataset = dataset_dir;
    _hooks = std::move(hooks);
    _want_file = file;
    _loaded = false;
    _load_error.clear();
    _status.clear();
    _notice.clear();
    _doc = _saved = spirula::RoiDocument{};
    _path.clear();
    _name.clear();
    _sel = -1;
    _drawing = false;
    _grab = Handle{};
    _mesh.reset();
    _inside.reset();
    _view.detach();
    _view.set_interactor(nullptr);
    _attached = false;
    reset_history();
    _busy = true;
    _done = false;
    _worker = std::thread([this] {
        try {
            run_load();
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(_mu);
            _load_error = e.what();
        }
        _done = true;
        _busy = false;
    });
}

void RoiEditor::run_load() {
    DatasetParserConfig dcfg;
    dcfg.require_image_files = false;
    ParsedDataset ds = parse_dataset(_dataset, dcfg, "");
    PostSplitCameras post = bake_post_split(ds, false, false);
    std::vector<double> pts(ds.points.xyz.size()), cams((size_t)ds.num_cameras * 3);
    for (size_t i = 0; i < pts.size(); i++) pts[i] = ds.points.xyz[i] + ds.center[i % 3];
    for (int64_t i = 0; i < ds.num_cameras; i++)
        for (int r = 0; r < 3; r++)
            cams[(size_t)i * 3 + r] = ds.c2w[(size_t)i * 12 + r * 4 + 3] + ds.center[(size_t)r];
    auto bounds_of = [](const std::vector<double>& v) {
        std::vector<float> f(v.size());
        for (size_t i = 0; i < v.size(); i++) f[i] = (float)v[i];
        return spirula::robust_bounds(f.data(), (int64_t)f.size() / 3);
    };
    spirula::Aabb box = bounds_of(pts);
    if (box.empty() && !cams.empty()) box = bounds_of(cams);
    double A[16];
    dsparse::train_to_normalized_inverse(ds, A);
    Sim3 shift;
    for (int i = 0; i < 3; i++) shift.t[i] = -ds.center[(size_t)i];
    const double A12[12] = {A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7], A[8], A[9], A[10], A[11]};
    std::lock_guard<std::mutex> lk(_mu);
    _ds = std::move(ds);
    _post = std::move(post);
    _points = std::move(pts);
    _cams = std::move(cams);
    _bounds = box;
    _to_view = Sim3::from_3x4(A12) * shift;
}

void RoiEditor::poll_load() {
    if (!_done.load()) return;
    _done = false;
    if (_worker.joinable()) _worker.join();
    std::string err;
    {
        std::lock_guard<std::mutex> lk(_mu);
        err = _load_error;
    }
    if (!err.empty()) {
        _status = err;
        if (_hooks.log) _hooks.log(err);
        return;
    }
    _loaded = true;
    _view.attach_preview_data(_ds, _post, "roi:" + _dataset, 1.0f, _ds.num_cameras > 0);
    _view.set_interactor(this);
    _attached = true;
    update_frame();
    RoiShape subject;
    _subject_ok = subject_start(subject);
    if (_subject_ok) _subject_raw = to_raw(subject);
    refresh_files();
    if (!_want_file.empty()) load_file(_want_file);
    else if (!_files.empty()) load_file(_files.front());
    else new_region();
}

void RoiEditor::close_now() {
    if (_drawing) end_outline();
    _open = false;
    _grab = Handle{};
    _view.set_interactor(nullptr);
    _view.detach();
    _attached = false;
}

// ===========================================================================
// The document and its history
// ===========================================================================

void RoiEditor::reset_history() {
    _history.assign(1, _doc);
    _hist_pos = 0;
    _gen++;
}

void RoiEditor::touch() { _gen++; }

void RoiEditor::commit() {
    if (_history[(size_t)_hist_pos] == _doc) return;
    _history.resize((size_t)_hist_pos + 1);
    _history.push_back(_doc);
    _hist_pos++;
    if (_history.size() > 200) {
        _history.erase(_history.begin());
        _hist_pos--;
    }
}

void RoiEditor::undo() {
    if (_hist_pos <= 0) return;
    _doc = _history[(size_t)--_hist_pos];
    if (_sel >= (int)_doc.shapes.size()) _sel = (int)_doc.shapes.size() - 1;
    touch();
}

void RoiEditor::redo() {
    if (_hist_pos + 1 >= (int)_history.size()) return;
    _doc = _history[(size_t)++_hist_pos];
    if (_sel >= (int)_doc.shapes.size()) _sel = (int)_doc.shapes.size() - 1;
    touch();
}

void RoiEditor::select(int i) {
    _sel = i >= 0 && i < (int)_doc.shapes.size() ? i : -1;
}

// ===========================================================================
// Files
// ===========================================================================

void RoiEditor::refresh_files() { _files = spirula::list_roi_files(_dataset); }

void RoiEditor::load_file(const std::string& path) {
    std::string err;
    spirula::RoiDocument d;
    _editable = spirula::read_roi_file(path, d, err);
    _status = _editable ? std::string() : err;
    _doc = _saved = d;
    _path = path;
    _name = fs::path(path).stem().string();
    _notice.clear();
    _sel = _doc.shapes.empty() ? -1 : 0;
    _drawing = false;
    reset_history();
}

void RoiEditor::new_region() {
    _doc = _saved = spirula::RoiDocument{};
    _path.clear();
    _editable = true;
    _status.clear();
    _notice.clear();
    std::string name = "region";
    for (int k = 2; fs::exists(fs::path(spirula::roi_folder(_dataset)) / (name + ".json")); k++)
        name = "region-" + std::to_string(k);
    _name = name;
    _sel = -1;
    reset_history();
}

void RoiEditor::save() {
    _status.clear();
    _notice.clear();
    if (!spirula::roi_region(_doc)) {
        _status = rmsg::nothing_to_save.get();
        return;
    }
    const std::string name = sanitize_name(_name);
    if (name.empty()) return;
    const fs::path target = fs::path(spirula::roi_folder(_dataset)) / (name + ".json");
    std::error_code ec;
    if (fs::exists(target, ec) && (_path.empty() || !fs::equivalent(target, _path, ec))) {
        _status = format(rmsg::name_taken, {name});
        return;
    }
    try {
        spirula::write_roi_file(_doc, target.string());
    } catch (const std::exception& e) {
        _status = e.what();
        return;
    }
    // A new name renames the file rather than leaving the old one behind.
    if (!_path.empty() && fs::path(_path) != target) fs::remove(_path, ec);
    _path = target.string();
    _name = name;
    _saved = _doc;
    _notice = format(rmsg::saved_to, {_path});
    if (_hooks.log) _hooks.log(_notice);
    refresh_files();
    if (_hooks.changed) _hooks.changed(_dataset);
}

void RoiEditor::delete_file() {
    if (_path.empty()) return;
    std::error_code ec;
    fs::remove(_path, ec);
    refresh_files();
    if (_hooks.changed) _hooks.changed(_dataset);
    if (!_files.empty()) load_file(_files.front());
    else new_region();
}

void RoiEditor::guard(std::function<void()> then) {
    if (!dirty()) {
        then();
        return;
    }
    _after_discard = std::move(then);
    _confirm = Confirm::Discard;
    _confirm_open = true;
}

// ===========================================================================
// Frames
// ===========================================================================

bool RoiEditor::view(ViewProjection& out) const {
    float x, y, w, h;
    _view.image_rect(x, y, w, h);
    if (w < 8.0f || h < 8.0f) return false;
    out.W = (int)w;
    out.H = (int)h;
    _view.nav_camera(out.W, out.H, out.w2c, out.fx, out.fy, out.camera_model, out.eye);
    out.cx = 0.5f * (float)out.W;
    out.cy = 0.5f * (float)out.H;
    out.ortho_back = _view.ortho_pullback(true);
    return true;
}

void RoiEditor::update_frame() {
    float m[12];
    _view.model_to_shared(m);
    _S = Sim3::from_3x4(m) * _to_view;
    _S_inv = _S.inverse();
}

static RoiShape moved_by(const RoiShape& s, const Sim3& S) {
    RoiShape o = s;
    S.apply(s.origin, o.origin);
    for (int r = 0; r < 3; r++) S.rotate(&s.R[r * 3], &o.R[r * 3]);
    for (int k = 0; k < 3; k++) {
        o.lo[k] *= S.s;
        o.hi[k] *= S.s;
    }
    for (double& v : o.polygon) v *= S.s;
    return o;
}

RoiShape RoiEditor::to_shared(const RoiShape& s) const { return moved_by(s, _S); }

RoiShape RoiEditor::to_raw(const RoiShape& s) const { return moved_by(s, _S_inv); }

// The scene's size in the shared frame: what a new shape is measured against.
double RoiEditor::shared_size() const {
    if (_bounds.empty()) return _S.s;
    double e = 0;
    for (int k = 0; k < 3; k++) e = std::max(e, _bounds.hi[k] - _bounds.lo[k]);
    return std::max(e, 1e-9) * _S.s;
}

// ===========================================================================
// Making shapes
// ===========================================================================

std::string RoiEditor::next_name(RoiShapeKind kind) const {
    const spirula::i18n::Msg* m = kind == RoiShapeKind::Box         ? &rmsg::name_box
                                  : kind == RoiShapeKind::Ellipsoid ? &rmsg::name_ellipsoid
                                  : kind == RoiShapeKind::Cylinder  ? &rmsg::name_cylinder
                                                                    : &rmsg::name_prism;
    for (int k = 1;; k++) {
        const std::string n = format(*m, {k});
        bool taken = false;
        for (const RoiShape& s : _doc.shapes) taken = taken || s.name == n;
        if (!taken) return n;
    }
}

void RoiEditor::add_shape(RoiShape raw) {
    if (raw.name.empty()) raw.name = next_name(raw.kind);
    _doc.shapes.push_back(std::move(raw));
    _sel = (int)_doc.shapes.size() - 1;
    touch();
    commit();
}

// A new shape where the view is looking, level, a quarter of the view across.
void RoiEditor::add_kind(RoiShapeKind kind) {
    if (kind == RoiShapeKind::Prism) {
        start_outline();
        return;
    }
    float target[3], c2w[12];
    _view.nav_target(target);
    _view.nav_pose(c2w, target);
    const double d = std::sqrt(std::pow(c2w[3] - target[0], 2) + std::pow(c2w[7] - target[1], 2) +
                               std::pow(c2w[11] - target[2], 2));
    const double h = std::max(0.2 * d, 1e-3 * shared_size());
    RoiShape s;
    s.kind = kind;
    for (int k = 0; k < 3; k++) {
        s.origin[k] = target[k];
        s.lo[k] = -h;
        s.hi[k] = h;
    }
    if (kind == RoiShapeKind::Cylinder) {
        s.lo[2] = -1.5 * h;
        s.hi[2] = 1.5 * h;
    }
    add_shape(to_raw(s));
    _mode = Mode::Adjust;
}

void RoiEditor::start_box() {
    const std::vector<double> pts = fit_points(_points, _S);
    const size_t n = pts.size() / 3;
    if (n < 4) {
        add_kind(RoiShapeKind::Box);
        return;
    }
    // The horizontal principal axis, so a street or a building gets a box
    // along it rather than one turned 45 degrees across it.
    double mx = 0, my = 0;
    for (size_t i = 0; i < n; i++) { mx += pts[i * 3]; my += pts[i * 3 + 1]; }
    mx /= (double)n;
    my /= (double)n;
    double sxx = 0, sxy = 0, syy = 0;
    for (size_t i = 0; i < n; i++) {
        const double x = pts[i * 3] - mx, y = pts[i * 3 + 1] - my;
        sxx += x * x; sxy += x * y; syy += y * y;
    }
    const double yaw = 0.5 * std::atan2(2 * sxy, sxx - syy);
    RoiShape s;
    s.kind = RoiShapeKind::Box;
    level_axes(yaw, s.R);
    std::vector<double> u[3];
    for (size_t i = 0; i < n; i++) {
        const double d[3] = {pts[i * 3] - mx, pts[i * 3 + 1] - my, pts[i * 3 + 2]};
        for (int k = 0; k < 3; k++) u[k].push_back(dot3(&s.R[k * 3], d));
    }
    double lo[3], hi[3];
    for (int k = 0; k < 3; k++) {
        lo[k] = percentile(u[k], 0.01);
        hi[k] = percentile(u[k], 0.99);
        const double pad = 0.05 * (hi[k] - lo[k]);
        lo[k] -= pad;
        hi[k] += pad;
    }
    const V3 c = world_of(s, 0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1]), 0.5 * (lo[2] + hi[2]));
    for (int k = 0; k < 3; k++) {
        s.origin[k] = c[k] + (k == 0 ? mx : k == 1 ? my : 0.0);
        s.hi[k] = std::max(0.5 * (hi[k] - lo[k]), 1e-6 * shared_size());
        s.lo[k] = -s.hi[k];
    }
    add_shape(to_raw(s));
    frame_selected();
    _mode = Mode::Adjust;
}

// An upright cylinder inside the circle the cameras stand on, centred where
// their optical axes meet, or for a 360 rig on the circle's middle; false
// unless the cameras surround it and it holds a share of the scene.
bool RoiEditor::subject_start(RoiShape& out) const {
    const int64_t n = _ds.num_cameras;
    if (n < 6) return false;
    double A[9] = {}, b[3] = {};
    std::vector<V3> pos((size_t)n), fwd((size_t)n);
    for (int64_t i = 0; i < n; i++) {
        double c[3], f[3];
        const double craw[3] = {_cams[(size_t)i * 3], _cams[(size_t)i * 3 + 1], _cams[(size_t)i * 3 + 2]};
        // OpenGL camera-to-world: the camera looks down its -Z column.
        const double fraw[3] = {-_ds.c2w[(size_t)i * 12 + 2], -_ds.c2w[(size_t)i * 12 + 6],
                                -_ds.c2w[(size_t)i * 12 + 10]};
        _S.apply(craw, c);
        _S.rotate(fraw, f);
        normalize3(f);
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 3; k++) {
                const double m = (r == k ? 1.0 : 0.0) - f[r] * f[k];
                A[r * 3 + k] += m;
                b[r] += m * c[k];
            }
        pos[(size_t)i] = {c[0], c[1], c[2]};
        fwd[(size_t)i] = {f[0], f[1], f[2]};
    }
    const std::vector<double> pts = fit_points(_points, _S);
    // The cameras all around `f` (no gap in azimuth wider than `max_gap`),
    // and the cylinder holding at least `min_share` of the points.
    auto fit = [&](const double f[3], double max_gap, double min_share) {
        std::vector<double> az, dist;
        for (const V3& c : pos) {
            az.push_back(std::atan2(c[1] - f[1], c[0] - f[0]));
            dist.push_back(std::hypot(c[0] - f[0], c[1] - f[1]));
        }
        std::sort(az.begin(), az.end());
        double gap = az.front() + 2 * kPi - az.back();
        for (size_t i = 1; i < az.size(); i++) gap = std::max(gap, az[i] - az[i - 1]);
        const double radius = 0.7 * percentile(dist, 0.5);
        if (gap > max_gap || !(radius > 1e-3 * shared_size())) return false;
        std::vector<double> z;
        for (size_t i = 0; i + 2 < pts.size(); i += 3)
            if (std::hypot(pts[i] - f[0], pts[i + 1] - f[1]) <= radius) z.push_back(pts[i + 2]);
        if ((double)z.size() < min_share * (double)(pts.size() / 3) || z.size() < 20) return false;
        out = RoiShape{};
        out.kind = RoiShapeKind::Cylinder;
        const double lo = percentile(z, 0.02), hi = percentile(z, 0.98);
        out.origin[0] = f[0];
        out.origin[1] = f[1];
        out.origin[2] = 0.5 * (lo + hi);
        const double hz = std::max(0.6 * (hi - lo), 1e-6 * shared_size());
        out.lo[0] = out.lo[1] = -radius;
        out.hi[0] = out.hi[1] = radius;
        out.lo[2] = -hz;
        out.hi[2] = hz;
        return true;
    };
    const double det = A[0] * (A[4] * A[8] - A[5] * A[7]) - A[1] * (A[3] * A[8] - A[5] * A[6]) +
                       A[2] * (A[3] * A[7] - A[4] * A[6]);
    if (std::fabs(det) > 1e-9 * (double)n * n * n) {
        double f[3];
        for (int k = 0; k < 3; k++) {
            double M[9];
            std::copy(A, A + 9, M);
            for (int r = 0; r < 3; r++) M[r * 3 + k] = b[r];
            f[k] = (M[0] * (M[4] * M[8] - M[5] * M[7]) - M[1] * (M[3] * M[8] - M[5] * M[6]) +
                    M[2] * (M[3] * M[7] - M[4] * M[6])) / det;
        }
        int64_t facing = 0;
        for (int64_t i = 0; i < n; i++) {
            const double to[3] = {f[0] - pos[(size_t)i][0], f[1] - pos[(size_t)i][1],
                                  f[2] - pos[(size_t)i][2]};
            facing += dot3(to, fwd[(size_t)i].data()) > 0;
        }
        if (facing >= n * 8 / 10 && fit(f, kPi, 0.0)) return true;
    }
    // A hand-held capture's cameras also stand all around their own middle;
    // what tells it from an orbit is that the scene is not in there with them.
    std::vector<double> xs, ys;
    for (const V3& c : pos) {
        xs.push_back(c[0]);
        ys.push_back(c[1]);
    }
    const double mid[3] = {percentile(xs, 0.5), percentile(ys, 0.5), 0.0};
    return fit(mid, 0.75 * kPi, 0.15);
}

void RoiEditor::start_subject(RoiShapeKind kind) {
    if (!_subject_ok) return;
    RoiShape s = _subject_raw;
    s.kind = kind;
    add_shape(s);
    frame_selected();
    _mode = Mode::Adjust;
}

void RoiEditor::frame_selected() {
    double c[3], r;
    if (!frame_bounds(c, r)) return;
    const float cf[3] = {(float)c[0], (float)c[1], (float)c[2]};
    _view.frame_view(cf, (float)r);
}

// Drawn in the view as it is, made orthographic so a straight edge on screen
// is a straight wall through the scene.
void RoiEditor::start_outline(int replace) {
    if (!_drawing) _was_ortho = _view.ortho();
    _drawing = true;
    _draw_fixed = false;
    _draw_replace = replace;
    _draw_pts.clear();
    _grab = Handle{};
    _view.set_ortho(true);
}

void RoiEditor::end_outline() {
    _drawing = false;
    _draw_pts.clear();
    _view.set_ortho(_was_ortho);
}

// The plane an outline is drawn on: through the orbit pivot, square to the
// view; rows right, up, toward the viewer. Follows the camera until the
// first corner pins it.
void RoiEditor::set_draw_frame(const ViewProjection& cam) {
    float target[3];
    _view.nav_target(target);
    for (int k = 0; k < 3; k++) {
        _draw_frame.origin[k] = target[k];
        _draw_frame.R[k] = cam.w2c[k];
        _draw_frame.R[3 + k] = -cam.w2c[4 + k];
        _draw_frame.R[6 + k] = -cam.w2c[8 + k];
    }
}

// How deep an outline runs: what the points inside its footprint span along
// its axis.
void RoiEditor::fit_depth(RoiShape& s) const {
    const std::vector<double> pts = fit_points(_points, _S);
    std::vector<double> z, all;
    const size_t n = s.polygon.size() / 2;
    for (size_t i = 0; i + 2 < pts.size(); i += 3) {
        double q[3];
        local_of(s, &pts[i], q);
        all.push_back(q[2]);
        if (spirula::polygon_contains(s.polygon.data(), n, q[0], q[1])) z.push_back(q[2]);
    }
    std::vector<double>& use = z.size() >= 20 ? z : all;
    if (use.empty()) return;
    const double lo = percentile(use, 0.02), hi = percentile(use, 0.98);
    const double pad = 0.1 * (hi - lo) + 1e-6 * shared_size();
    s.lo[2] = lo - pad;
    s.hi[2] = hi + pad;
}

void RoiEditor::finish_outline() {
    const size_t n = _draw_pts.size() / 2;
    if (n < 3) {
        end_outline();
        return;
    }
    RoiShape s;
    if (_draw_replace >= 0 && _draw_replace < (int)_doc.shapes.size())
        s = to_shared(_doc.shapes[(size_t)_draw_replace]);
    s.kind = RoiShapeKind::Prism;
    std::copy(_draw_frame.R, _draw_frame.R + 9, s.R);
    double cx = 0, cy = 0;
    for (size_t i = 0; i < n; i++) { cx += _draw_pts[i * 2]; cy += _draw_pts[i * 2 + 1]; }
    cx /= (double)n;
    cy /= (double)n;
    const V3 o = world_of(_draw_frame, cx, cy, 0.0);
    std::copy(o.begin(), o.end(), s.origin);
    s.polygon.clear();
    for (size_t i = 0; i < n; i++) {
        s.polygon.push_back(_draw_pts[i * 2] - cx);
        s.polygon.push_back(_draw_pts[i * 2 + 1] - cy);
    }
    s.lo[0] = s.lo[1] = s.hi[0] = s.hi[1] = 0;
    fit_depth(s);
    end_outline();
    if (_draw_replace >= 0 && _draw_replace < (int)_doc.shapes.size()) {
        _doc.shapes[(size_t)_draw_replace] = to_raw(s);
        _sel = _draw_replace;
        touch();
        commit();
    } else {
        s.name.clear();
        add_shape(to_raw(s));
    }
    _mode = Mode::Adjust;
}

// ===========================================================================
// What the region keeps: dimmed points, the boundary, the counts
// ===========================================================================

void RoiEditor::refresh_overlays() {
    if (!_loaded) return;
    const double now = ImGui::GetTime();
    bool changed = false;
    if (_inside_gen != _gen && (_grab.kind == Grab::None || now - _inside_time > 0.08)) {
        _inside_gen = _gen;
        _inside_time = now;
        const std::shared_ptr<const spirula::Region> r = spirula::roi_region(_doc);
        const int64_t n = (int64_t)_points.size() / 3, nc = (int64_t)_cams.size() / 3;
        if (r) {
            auto flags = std::make_shared<std::vector<uint8_t>>((size_t)n);
            r->contains_many(_points.data(), n, flags->data());
            std::vector<uint8_t> cf((size_t)nc);
            r->contains_many(_cams.data(), nc, cf.data());
            _n_inside = std::count(flags->begin(), flags->end(), (uint8_t)1);
            _n_cams_inside = std::count(cf.begin(), cf.end(), (uint8_t)1);
            _inside = flags;
        } else {
            _n_inside = n;
            _n_cams_inside = nc;
            _inside.reset();
        }
        changed = true;
    }
    if (_mesh_job.valid() &&
        _mesh_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Overlays o = _mesh_job.get();
        _mesh = o.mesh;
        _mesh_gen = o.gen;
        changed = true;
    }
    if (!_mesh_job.valid() && _mesh_gen != _gen) {
        const std::shared_ptr<const spirula::Region> r = spirula::roi_region(_doc);
        const uint64_t gen = _gen;
        if (!r) {
            _mesh.reset();
            _mesh_gen = gen;
            changed = true;
        } else {
            // All of a bounded region, however far past the points; an
            // unbounded one (a list opening with a cut) up to the scene's edge.
            spirula::Aabb box = _bounds;
            const spirula::Aabb rb = r->bounds();
            if (!rb.empty() && !rb.unbounded())
                for (int a = 0; a < 3; a++) {
                    const double pad = 0.05 * (rb.hi[a] - rb.lo[a]) + 1e-9;
                    box.lo[a] = rb.lo[a] - pad;
                    box.hi[a] = rb.hi[a] + pad;
                }
            const std::array<double, 3> c = _ds.center;
            _mesh_job = std::async(std::launch::async, [r, box, c, gen]() {
                Overlays o;
                o.gen = gen;
                if (box.empty() || box.lo[0] >= box.hi[0]) return o;
                const spirula::RegionMesh m = spirula::region_boundary_mesh(*r, box, kMeshCells);
                const float to_points[12] = {1, 0, 0, (float)-c[0], 0, 1, 0, (float)-c[1],
                                             0, 0, 1, (float)-c[2]};
                const float rgb[3] = {1.0f, 0.55f, 0.1f};
                auto ov = std::make_shared<spirula::RegionOverlay>();
                ov->add(m, rgb, to_points);
                o.mesh = ov;
                return o;
            });
        }
    }
    if (changed) _view.set_region_overlay(nullptr, _mesh, _inside);
}

// ===========================================================================
// The view: handles, picking, dragging
// ===========================================================================

namespace {

constexpr float kArrowPx = 84.0f;
constexpr float kRingPx = 70.0f;

// Corner `c` of the box the sides make (bit k: the high side of axis k).
V3 box_corner(const double lo[3], const double hi[3], int c) {
    return V3{(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2]};
}

// The outward normal of an outline's edge from corner `i` to the next, in its
// plane; the winding decides which side is out.
void wall_normal(const RoiShape& s, int i, double n[2]) {
    const size_t nv = s.polygon.size() / 2, a = (size_t)i, b = (a + 1) % nv;
    double area = 0;
    for (size_t k = 0, j = nv - 1; k < nv; j = k++)
        area += s.polygon[j * 2] * s.polygon[k * 2 + 1] - s.polygon[k * 2] * s.polygon[j * 2 + 1];
    const double dx = s.polygon[b * 2] - s.polygon[a * 2], dy = s.polygon[b * 2 + 1] - s.polygon[a * 2 + 1];
    const double len = std::max(std::hypot(dx, dy), 1e-300), sg = area >= 0 ? 1.0 : -1.0;
    n[0] = sg * dy / len;
    n[1] = -sg * dx / len;
}

}  // namespace

RoiEditor::Handle RoiEditor::hit_handle(const ViewProjection& cam, float x, float y) const {
    Handle none;
    if (_sel < 0 || _sel >= (int)_doc.shapes.size()) return none;
    const RoiShape s = to_shared(_doc.shapes[(size_t)_sel]);
    const ImVec2 m(x, y);
    const double ppu = pixels_per_unit(cam, s.origin);
    ImVec2 c;
    const bool c_ok = proj(cam, s.origin, c);
    const float r_hit = px(9.0f), l_hit = px(6.0f);
    Handle best;
    double best_d = 1e30;
    auto take = [&](double d, const Handle& h, double limit) {
        if (d < limit && d < best_d) {
            best_d = d;
            best = h;
        }
    };
    auto take_point = [&](const V3& q, const Handle& h, double bias = 0.0) {
        const V3 p = world_of(s, q[0], q[1], q[2]);
        ImVec2 sp;
        if (proj(cam, p.data(), sp)) take(std::hypot(m.x - sp.x, m.y - sp.y) + bias, h, r_hit);
    };
    if (_mode == Mode::Move) {
        if (c_ok) take(std::hypot(m.x - c.x, m.y - c.y), Handle{Grab::Center, 0, 1}, r_hit + px(3));
        if (c_ok && ppu > 0)
            for (int k = 0; k < 3; k++) {
                double e[3] = {0, 0, 0};
                e[k] = px(kArrowPx) / ppu;
                const double end[3] = {s.origin[0] + e[0], s.origin[1] + e[1], s.origin[2] + e[2]};
                ImVec2 q;
                if (!proj(cam, end, q)) continue;
                if (std::hypot(m.x - c.x, m.y - c.y) < px(12)) continue;
                take(seg_distance(m, c, q), Handle{Grab::Axis, k, 1}, l_hit);
            }
    } else if (_mode == Mode::Adjust || _mode == Mode::Resize) {
        const Grab side = _mode == Mode::Adjust ? Grab::Face : Grab::Stretch;
        for (int k = 0; k < 3; k++)
            for (int sg : {-1, 1}) take_point(side_point(s, k, sg), Handle{side, k, sg});
        if (_mode == Mode::Resize) {
            double lo[3], hi[3];
            spirula::roi_extents(s, lo, hi);
            for (int cn = 0; cn < 8; cn++) take_point(box_corner(lo, hi, cn), Handle{Grab::Scale, cn, 1});
        } else if (s.kind == RoiShapeKind::Prism) {
            const size_t n = s.polygon.size() / 2;
            for (size_t i = 0; i < n; i++) {
                const size_t j = (i + 1) % n;
                take_point(V3{s.polygon[i * 2], s.polygon[i * 2 + 1], s.hi[2]},
                           Handle{Grab::Corner, (int)i, 1}, -1.0);
                take_point(V3{0.5 * (s.polygon[i * 2] + s.polygon[j * 2]),
                              0.5 * (s.polygon[i * 2 + 1] + s.polygon[j * 2 + 1]), s.hi[2]},
                           Handle{Grab::Midpoint, (int)i, 1});
            }
        }
    } else if (c_ok && ppu > 0) {
        const double rad = px(kRingPx) / ppu;
        for (int k = 0; k < 3; k++) {
            const double* u = &s.R[((k + 1) % 3) * 3];
            const double* v = &s.R[((k + 2) % 3) * 3];
            ImVec2 prev;
            bool have = false;
            double d = 1e30;
            for (int i = 0; i <= 64; i++) {
                const double a = 2 * kPi * i / 64;
                double p[3];
                for (int t = 0; t < 3; t++)
                    p[t] = s.origin[t] + rad * (std::cos(a) * u[t] + std::sin(a) * v[t]);
                ImVec2 q;
                if (!proj(cam, p, q)) { have = false; continue; }
                if (have) d = std::min(d, seg_distance(m, prev, q));
                prev = q;
                have = true;
            }
            take(d, Handle{Grab::Ring, k, 1}, l_hit);
        }
    }
    return best;
}

RoiEditor::Handle RoiEditor::side_under(const RoiShape& s, const ViewProjection& cam, float x,
                                        float y) const {
    double ro[3], rd[3], t;
    if (!pointer_ray(cam, x, y, ro, rd) || !ray_entry(s, ro, rd, t)) return Handle{};
    const double p[3] = {ro[0] + t * rd[0], ro[1] + t * rd[1], ro[2] + t * rd[2]};
    double q[3], lo[3], hi[3], u[3];
    local_of(s, p, q);
    spirula::roi_extents(s, lo, hi);
    for (int k = 0; k < 3; k++)
        u[k] = (q[k] - 0.5 * (lo[k] + hi[k])) / std::max(0.5 * (hi[k] - lo[k]), 1e-300);
    auto face = [](int k, double v) { return Handle{Grab::Face, k, v < 0 ? -1 : 1}; };
    const bool on_cap = std::fabs(u[2]) > 1.0 - 1e-6;
    switch (s.kind) {
        case RoiShapeKind::Box:
        case RoiShapeKind::Ellipsoid: {
            int k = 0;
            for (int a = 1; a < 3; a++)
                if (std::fabs(u[a]) > std::fabs(u[k])) k = a;
            return face(k, u[k]);
        }
        case RoiShapeKind::Cylinder:
            if (on_cap) return face(2, u[2]);
            return std::fabs(u[0]) >= std::fabs(u[1]) ? face(0, u[0]) : face(1, u[1]);
        case RoiShapeKind::Prism: {
            if (on_cap) return face(2, u[2]);
            const size_t n = s.polygon.size() / 2;
            int best = 0;
            double best_d = 1e300;
            for (size_t i = 0; i < n; i++) {
                const size_t j = (i + 1) % n;
                const double ax = s.polygon[i * 2], ay = s.polygon[i * 2 + 1];
                const double vx = s.polygon[j * 2] - ax, vy = s.polygon[j * 2 + 1] - ay;
                const double l2 = vx * vx + vy * vy;
                const double f = l2 > 0 ? std::clamp(((q[0] - ax) * vx + (q[1] - ay) * vy) / l2, 0.0, 1.0) : 0.0;
                const double d = std::hypot(q[0] - ax - f * vx, q[1] - ay - f * vy);
                if (d < best_d) {
                    best_d = d;
                    best = (int)i;
                }
            }
            return Handle{Grab::Wall, best, 1};
        }
    }
    return Handle{};
}

// The side a drag in Adjust would move, filled and traced.
void RoiEditor::draw_side(ImDrawList* dl, const ViewProjection& cam, const ImVec2& o,
                          const RoiShape& s, const Handle& h) const {
    double lo[3], hi[3], mid[3], half[3];
    spirula::roi_extents(s, lo, hi);
    for (int k = 0; k < 3; k++) {
        mid[k] = 0.5 * (lo[k] + hi[k]);
        half[k] = 0.5 * (hi[k] - lo[k]);
    }
    std::vector<V3> loop;   // closed, in the shape's frame
    if (h.kind == Grab::Wall) {
        const size_t n = s.polygon.size() / 2, i = (size_t)h.index, j = (i + 1) % n;
        loop = {V3{s.polygon[i * 2], s.polygon[i * 2 + 1], lo[2]}, V3{s.polygon[j * 2], s.polygon[j * 2 + 1], lo[2]},
                V3{s.polygon[j * 2], s.polygon[j * 2 + 1], hi[2]}, V3{s.polygon[i * 2], s.polygon[i * 2 + 1], hi[2]}};
    } else if (h.kind == Grab::Face) {
        const int k = h.index, a = (k + 1) % 3, b = (k + 2) % 3;
        const double side = h.sign > 0 ? hi[k] : lo[k];
        if (s.kind == RoiShapeKind::Prism && k == 2) {
            for (size_t i = 0; i + 1 < s.polygon.size(); i += 2) loop.push_back(V3{s.polygon[i], s.polygon[i + 1], side});
        } else if (s.kind == RoiShapeKind::Cylinder && k == 2) {
            for (int i = 0; i < 48; i++) {
                const double t = 2 * kPi * i / 48;
                loop.push_back(V3{mid[0] + half[0] * std::cos(t), mid[1] + half[1] * std::sin(t), side});
            }
        } else if (s.kind == RoiShapeKind::Cylinder) {
            // The stretch of wall around that side: top arc out, bottom back.
            const double t0 = k == 0 ? (h.sign > 0 ? 0.0 : kPi) : (h.sign > 0 ? 0.5 * kPi : -0.5 * kPi);
            for (int e = 0; e < 2; e++)
                for (int i = 0; i <= 12; i++) {
                    const double t = t0 + (e ? -1.0 : 1.0) * 0.6 * (i / 6.0 - 1.0);
                    loop.push_back(V3{mid[0] + half[0] * std::cos(t), mid[1] + half[1] * std::sin(t),
                                      e ? lo[2] : hi[2]});
                }
        } else if (s.kind == RoiShapeKind::Ellipsoid) {
            // The cap around that pole, where it bulges most.
            const double f = 0.85, r = std::sqrt(1 - f * f);
            for (int i = 0; i < 48; i++) {
                const double t = 2 * kPi * i / 48;
                V3 q;
                q[k] = mid[k] + h.sign * f * half[k];
                q[a] = mid[a] + r * half[a] * std::cos(t);
                q[b] = mid[b] + r * half[b] * std::sin(t);
                loop.push_back(q);
            }
        } else {
            for (int c = 0; c < 4; c++) {
                V3 q;
                q[k] = side;
                q[a] = (c == 1 || c == 2) ? hi[a] : lo[a];
                q[b] = c >= 2 ? hi[b] : lo[b];
                loop.push_back(q);
            }
        }
    }
    if (loop.empty()) return;
    std::vector<ImVec2> pts;
    std::vector<V3> world;
    for (const V3& q : loop) {
        const V3 p = world_of(s, q[0], q[1], q[2]);
        world.push_back(p);
        ImVec2 sp;
        if (proj(cam, p.data(), sp)) pts.push_back(ImVec2(o.x + sp.x, o.y + sp.y));
    }
    if (pts.size() == loop.size() && pts.size() >= 3)
        dl->AddConcavePolyFilled(pts.data(), (int)pts.size(), IM_COL32(255, 235, 90, 60));
    world.push_back(world.front());
    draw_polyline(dl, cam, o, world, kHot, px(2.5f));
}

int RoiEditor::pick_shape(const ViewProjection& cam, float x, float y, double* depth) const {
    double ro[3], rd[3];
    if (!pointer_ray(cam, x, y, ro, rd)) return -1;
    int best = -1;
    double best_t = 1e300;
    for (int i = 0; i < (int)_doc.shapes.size(); i++) {
        if (!_show_all && i != _sel) continue;
        double t;
        if (ray_entry(to_shared(_doc.shapes[(size_t)i]), ro, rd, t) && t < best_t) {
            best_t = t;
            best = i;
        }
    }
    if (depth) *depth = best_t;
    return best;
}

void RoiEditor::begin_drag(const Handle& h, const ViewProjection& cam, float x, float y) {
    _grab = h;
    _start = to_shared(_doc.shapes[(size_t)_sel]);
    _press[0] = x;
    _press[1] = y;
    _angle = 0.0;
    _last_angle = 0.0;
    double ro[3], rd[3];
    const bool ray = pointer_ray(cam, x, y, ro, rd);
    switch (h.kind) {
        case Grab::Body:
        case Grab::Center: {
            // Along the ground; seen edge-on the ground is no plane to drag
            // on, so the view's own plane instead.
            for (int k = 0; k < 3; k++) _anchor[k] = _start.origin[k];
            if (h.kind == Grab::Body && ray) {
                double t;
                if (ray_entry(_start, ro, rd, t))
                    for (int k = 0; k < 3; k++) _anchor[k] = ro[k] + t * rd[k];
            }
            _plane_n[0] = _plane_n[1] = 0;
            _plane_n[2] = 1;
            if (!ray || std::fabs(rd[2]) < 0.12)
                for (int k = 0; k < 3; k++) _plane_n[k] = cam.w2c[8 + k];
            break;
        }
        case Grab::Axis: {
            double e[3] = {0, 0, 0};
            e[h.index] = 1;
            if (!line_param(cam, x, y, _start.origin, e, _t0)) _t0 = 0;
            break;
        }
        case Grab::Face:
        case Grab::Stretch: {
            const double* a = &_start.R[h.index * 3];
            if (!line_param(cam, x, y, _start.origin, a, _t0)) _t0 = 0;
            break;
        }
        case Grab::Wall: {
            wall_normal(_start, h.index, _push_n);
            for (int k = 0; k < 3; k++) _push_dir[k] = _start.R[k] * _push_n[0] + _start.R[3 + k] * _push_n[1];
            if (!line_param(cam, x, y, _start.origin, _push_dir, _t0)) _t0 = 0;
            break;
        }
        case Grab::Scale: {
            // Along the diagonal from the middle through the grabbed corner.
            double lo[3], hi[3], mid[3];
            spirula::roi_extents(_start, lo, hi);
            for (int k = 0; k < 3; k++) mid[k] = 0.5 * (lo[k] + hi[k]);
            const V3 m = world_of(_start, mid[0], mid[1], mid[2]);
            const V3 cq = box_corner(lo, hi, h.index);
            const V3 cw = world_of(_start, cq[0], cq[1], cq[2]);
            for (int k = 0; k < 3; k++) {
                _anchor[k] = m[k];
                _push_dir[k] = cw[k] - m[k];
            }
            normalize3(_push_dir);
            if (!line_param(cam, x, y, _anchor, _push_dir, _t0) || !(_t0 > 1e-12)) _t0 = 0;
            break;
        }
        case Grab::Midpoint: {
            // A new corner halfway along the edge, then dragged as one.
            const size_t n = _start.polygon.size() / 2, i = (size_t)h.index, j = (i + 1) % n;
            const double mx = 0.5 * (_start.polygon[i * 2] + _start.polygon[j * 2]);
            const double my = 0.5 * (_start.polygon[i * 2 + 1] + _start.polygon[j * 2 + 1]);
            _start.polygon.insert(_start.polygon.begin() + (ptrdiff_t)(i + 1) * 2, {mx, my});
            _doc.shapes[(size_t)_sel] = to_raw(_start);
            _grab = Handle{Grab::Corner, (int)i + 1, 1};
            touch();
        }
            [[fallthrough]];
        case Grab::Corner: {
            const int i = _grab.index;
            const V3 p = world_of(_start, _start.polygon[(size_t)i * 2], _start.polygon[(size_t)i * 2 + 1],
                                  _start.hi[2]);
            for (int k = 0; k < 3; k++) _anchor[k] = p[k];
            break;
        }
        case Grab::Ring: {
            ImVec2 c;
            _last_angle = proj(cam, _start.origin, c)
                              ? std::atan2((double)(y - c.y), (double)(x - c.x)) : 0.0;
            break;
        }
        case Grab::None:
            break;
    }
}

void RoiEditor::update_drag(const ViewProjection& cam, float x, float y, bool shift, bool ctrl) {
    RoiShape s = _start;
    const double min_len = 2e-5 * shared_size();
    switch (_grab.kind) {
        case Grab::Body:
        case Grab::Center: {
            double hit[3], hit0[3];
            if (!ray_plane(cam, x, y, _anchor, _plane_n, hit) ||
                !ray_plane(cam, _press[0], _press[1], _anchor, _plane_n, hit0))
                return;
            for (int k = 0; k < 3; k++) s.origin[k] += hit[k] - hit0[k];
            break;
        }
        case Grab::Axis: {
            double e[3] = {0, 0, 0}, t;
            e[_grab.index] = 1;
            if (!line_param(cam, x, y, _start.origin, e, t)) return;
            s.origin[_grab.index] += t - _t0;
            break;
        }
        case Grab::Face:
        case Grab::Stretch: {
            const int k = _grab.index, sg = _grab.sign;
            const double* a = &_start.R[k * 3];
            double t;
            if (!line_param(cam, x, y, _start.origin, a, t)) return;
            const double delta = t - _t0;
            double lo[3], hi[3];
            spirula::roi_extents(_start, lo, hi);
            // The grabbed side follows the pointer; when both move, the
            // opposite one mirrors it. Neither passes the other.
            const bool both = _grab.kind == Grab::Stretch || shift;
            double& grabbed = sg > 0 ? hi[k] : lo[k];
            double& other = sg > 0 ? lo[k] : hi[k];
            grabbed += delta;
            if (both) other -= delta;
            if (hi[k] - lo[k] < min_len) {
                if (both) {
                    const double m = 0.5 * (lo[k] + hi[k]);
                    lo[k] = m - 0.5 * min_len;
                    hi[k] = m + 0.5 * min_len;
                } else {
                    grabbed = other + sg * min_len;
                }
            }
            spirula::roi_set_extent(s, k, lo[k], hi[k]);
            break;
        }
        case Grab::Wall: {
            double t;
            if (!line_param(cam, x, y, _start.origin, _push_dir, t)) return;
            const double delta = t - _t0;
            const size_t n = s.polygon.size() / 2, i = (size_t)_grab.index, j = (i + 1) % n;
            for (size_t v : {i, j}) {
                s.polygon[v * 2] += delta * _push_n[0];
                s.polygon[v * 2 + 1] += delta * _push_n[1];
            }
            break;
        }
        case Grab::Scale: {
            double t;
            if (!(_t0 > 0) || !line_param(cam, x, y, _anchor, _push_dir, t)) return;
            double f = std::max(t / _t0, 1e-3);
            if (ctrl) f = std::max(0.1, std::round(f * 10) / 10);
            double lo[3], hi[3];
            spirula::roi_extents(_start, lo, hi);
            for (int k = 0; k < 3; k++) {
                const double m = 0.5 * (lo[k] + hi[k]), h = 0.5 * (hi[k] - lo[k]) * f;
                spirula::roi_set_extent(s, k, m - h, m + h);
            }
            break;
        }
        case Grab::Corner: {
            const double* nz = &_start.R[6];
            double hit[3], hit0[3];
            if (!ray_plane(cam, x, y, _anchor, nz, hit) ||
                !ray_plane(cam, _press[0], _press[1], _anchor, nz, hit0))
                return;
            const double d[3] = {hit[0] - hit0[0], hit[1] - hit0[1], hit[2] - hit0[2]};
            const size_t i = (size_t)_grab.index;
            s.polygon[i * 2] += dot3(&_start.R[0], d);
            s.polygon[i * 2 + 1] += dot3(&_start.R[3], d);
            break;
        }
        case Grab::Ring: {
            const double* ax = &_start.R[_grab.index * 3];
            ImVec2 c;
            if (!proj(cam, _start.origin, c)) return;
            const double a = std::atan2((double)(y - c.y), (double)(x - c.x));
            _angle += std::remainder(a - _last_angle, 2 * kPi);
            _last_angle = a;
            // The pointer turns clockwise on screen as y grows downward; a
            // turn about an axis facing the viewer looks the other way.
            const double to_eye[3] = {cam.eye[0] - _start.origin[0], cam.eye[1] - _start.origin[1],
                                      cam.eye[2] - _start.origin[2]};
            double angle = dot3(ax, to_eye) > 0 ? -_angle : _angle;
            if (ctrl) angle = std::round(angle / (kPi / 12)) * (kPi / 12);
            const Sim3 rot = Sim3::rotation_about(ax, angle, _start.origin);
            for (int r = 0; r < 3; r++) rot.rotate(&_start.R[r * 3], &s.R[r * 3]);
            break;
        }
        case Grab::Midpoint:
        case Grab::None:
            return;
    }
    _doc.shapes[(size_t)_sel] = to_raw(s);
    touch();
}

void RoiEditor::end_drag(bool keep) {
    if (_grab.kind == Grab::None) return;
    if (keep) {
        commit();
    } else {
        _doc = _history[(size_t)_hist_pos];
        touch();
    }
    _grab = Handle{};
    _armed = false;
}

bool RoiEditor::owns_left_button() const { return _drawing || _grab.kind != Grab::None; }

bool RoiEditor::blocks_fly_keys() const { return _drawing || _sel >= 0; }

bool RoiEditor::owns_right_button() const { return _drawing || _grab.kind != Grab::None; }

bool RoiEditor::frame_bounds(double centre[3], double& radius) {
    spirula::Aabb b;
    for (int i = 0; i < (int)_doc.shapes.size(); i++)
        if (_sel < 0 || i == _sel) b.expand(to_shared(_doc.shapes[(size_t)i]).region()->bounds());
    if (b.empty() || b.unbounded()) return false;
    double r2 = 0;
    for (int k = 0; k < 3; k++) {
        centre[k] = 0.5 * (b.lo[k] + b.hi[k]);
        r2 += 0.25 * (b.hi[k] - b.lo[k]) * (b.hi[k] - b.lo[k]);
    }
    radius = std::sqrt(r2);
    return true;
}

bool RoiEditor::on_viewport_input(const ViewportInput& in) {
    _in = in;
    if (!_loaded || !_editable) return false;
    update_frame();
    ViewProjection cam;
    if (!view(cam)) return false;

    if (_drawing) {
        if (in.right_clicked) {
            finish_outline();
            return true;
        }
        if (in.double_clicked) {
            finish_outline();
            return true;
        }
        if (!_draw_fixed) set_draw_frame(cam);
        if (in.clicked) {
            double hit[3], q[3];
            if (!ray_plane(cam, in.x, in.y, _draw_frame.origin, &_draw_frame.R[6], hit)) return true;
            // The first corner, clicked again, closes the outline.
            if (_draw_pts.size() >= 6) {
                const V3 first = world_of(_draw_frame, _draw_pts[0], _draw_pts[1], 0.0);
                ImVec2 f;
                if (proj(cam, first.data(), f) && std::hypot(f.x - in.x, f.y - in.y) < px(10.0f)) {
                    finish_outline();
                    return true;
                }
            }
            _draw_fixed = true;
            local_of(_draw_frame, hit, q);
            _draw_pts.push_back(q[0]);
            _draw_pts.push_back(q[1]);
        }
        return in.hovered;
    }

    if (_grab.kind != Grab::None) {
        if (in.right_clicked) {
            end_drag(false);
            return true;
        }
        if (_armed && std::hypot(in.x - _press[0], in.y - _press[1]) < px(3.0f)) {
            if (!in.down) end_drag(true);
            return true;
        }
        _armed = false;
        if (in.down) update_drag(cam, in.x, in.y, in.shift, in.ctrl);
        else end_drag(true);
        return true;
    }

    _hot = in.hovered ? hit_handle(cam, in.x, in.y) : Handle{};
    _hover = in.hovered && _hot.kind == Grab::None ? pick_shape(cam, in.x, in.y) : -1;
    // In Adjust the selected shape's own surface is a handle, where it is in front.
    if (_mode == Mode::Adjust && _hover >= 0 && _hover == _sel) {
        _hot = side_under(to_shared(_doc.shapes[(size_t)_sel]), cam, in.x, in.y);
        if (_hot.kind != Grab::None) _hover = -1;
    }

    if (in.double_clicked && _hot.kind == Grab::Corner && _sel >= 0) {
        RoiShape& s = _doc.shapes[(size_t)_sel];
        if (s.polygon.size() > 6) {
            s.polygon.erase(s.polygon.begin() + _hot.index * 2, s.polygon.begin() + _hot.index * 2 + 2);
            touch();
            commit();
        }
        _hot = Handle{};
        return true;
    }
    if (in.clicked) {
        if (_hot.kind != Grab::None) {
            begin_drag(_hot, cam, in.x, in.y);
            return true;
        }
        if (_hover >= 0) {
            // Adjusting is what a drag does to the selection; a click on
            // another shape only picks it.
            select(_hover);
            if (_mode != Mode::Adjust) {
                begin_drag(Handle{Grab::Body, 0, 1}, cam, in.x, in.y);
                _armed = true;
            }
            return true;
        }
        _press_empty = true;
        _empty_xy[0] = in.x;
        _empty_xy[1] = in.y;
        return false;
    }
    if (_press_empty && !in.down) {
        _press_empty = false;
        if (in.hovered && std::hypot(in.x - _empty_xy[0], in.y - _empty_xy[1]) < px(3.0f)) select(-1);
    }
    return false;
}

void RoiEditor::draw_viewport_overlay(const ViewportOverlay& v) {
    if (!_loaded) return;
    update_frame();
    ViewProjection cam;
    if (!view(cam)) return;
    ImDrawList* dl = v.dl;
    const ImVec2 o(v.x, v.y);
    auto at = [&](ImVec2 p) { return ImVec2(o.x + p.x, o.y + p.y); };

    for (int i = 0; i < (int)_doc.shapes.size(); i++) {
        if (!_show_all && i != _sel) continue;
        const RoiShape s = to_shared(_doc.shapes[(size_t)i]);
        const bool sel = i == _sel, hot = i == _hover;
        ImU32 col = s.enabled ? op_color(s.op, sel ? 255 : hot ? 230 : 160)
                              : IM_COL32(160, 160, 160, sel ? 220 : 110);
        const float thick = sel ? px(2.2f) : hot ? px(1.8f) : px(1.2f);
        for (const std::vector<V3>& l : outline(s)) draw_polyline(dl, cam, o, l, col, thick);
    }

    if (_sel >= 0 && _sel < (int)_doc.shapes.size() && !_drawing && _editable) {
        const RoiShape s = to_shared(_doc.shapes[(size_t)_sel]);
        const double ppu = pixels_per_unit(cam, s.origin);
        ImVec2 c;
        const bool c_ok = proj(cam, s.origin, c);
        auto is_hot = [&](Grab g, int idx, int sg) {
            const Handle& h = _grab.kind != Grab::None ? _grab : _hot;
            return h.kind == g && h.index == idx && h.sign == sg;
        };
        const float r = px(5.0f);
        const Handle& now = _grab.kind != Grab::None ? _grab : _hot;
        if (_mode == Mode::Adjust && (now.kind == Grab::Face || now.kind == Grab::Wall))
            draw_side(dl, cam, o, s, now);
        auto square = [&](const V3& q, ImU32 col) {
            const V3 p = world_of(s, q[0], q[1], q[2]);
            ImVec2 sp;
            if (!proj(cam, p.data(), sp)) return;
            dl->AddRectFilled(at(ImVec2(sp.x - r - px(1.5f), sp.y - r - px(1.5f))),
                              at(ImVec2(sp.x + r + px(1.5f), sp.y + r + px(1.5f))), IM_COL32(20, 20, 20, 200));
            dl->AddRectFilled(at(ImVec2(sp.x - r, sp.y - r)), at(ImVec2(sp.x + r, sp.y + r)), col);
        };
        if (_mode == Mode::Move && c_ok && ppu > 0) {
            for (int k = 0; k < 3; k++) {
                double end[3] = {s.origin[0], s.origin[1], s.origin[2]};
                end[k] += px(kArrowPx) / ppu;
                ImVec2 q;
                if (!proj(cam, end, q)) continue;
                const ImU32 col = is_hot(Grab::Axis, k, 1) ? kHot : kAxisCol[k];
                dl->AddLine(at(c), at(q), col, px(2.5f));
                const float dx = q.x - c.x, dy = q.y - c.y, len = std::hypot(dx, dy);
                if (len > 1) {
                    const float ux = dx / len, uy = dy / len, w = px(6.0f), l = px(12.0f);
                    dl->AddTriangleFilled(at(ImVec2(q.x + ux * l * 0.6f, q.y + uy * l * 0.6f)),
                                          at(ImVec2(q.x - ux * l * 0.4f - uy * w, q.y - uy * l * 0.4f + ux * w)),
                                          at(ImVec2(q.x - ux * l * 0.4f + uy * w, q.y - uy * l * 0.4f - ux * w)), col);
                }
            }
            const ImU32 col = is_hot(Grab::Center, 0, 1) ? kHot : IM_COL32(245, 245, 245, 255);
            dl->AddCircleFilled(at(c), r + px(2), IM_COL32(20, 20, 20, 200));
            dl->AddCircleFilled(at(c), r, col);
        } else if (_mode == Mode::Adjust || _mode == Mode::Resize) {
            const Grab side = _mode == Mode::Adjust ? Grab::Face : Grab::Stretch;
            for (int k = 0; k < 3; k++)
                for (int sg : {-1, 1}) square(side_point(s, k, sg), is_hot(side, k, sg) ? kHot : kAxisCol[k]);
            if (_mode == Mode::Resize) {
                double lo[3], hi[3];
                spirula::roi_extents(s, lo, hi);
                for (int cn = 0; cn < 8; cn++) square(box_corner(lo, hi, cn), is_hot(Grab::Scale, cn, 1)
                                                                                  ? kHot : IM_COL32(245, 245, 245, 255));
            } else if (s.kind == RoiShapeKind::Prism) {
                const size_t n = s.polygon.size() / 2;
                for (size_t i = 0; i < n; i++) {
                    const size_t j = (i + 1) % n;
                    const V3 a = world_of(s, s.polygon[i * 2], s.polygon[i * 2 + 1], s.hi[2]);
                    const V3 b = world_of(s, 0.5 * (s.polygon[i * 2] + s.polygon[j * 2]),
                                          0.5 * (s.polygon[i * 2 + 1] + s.polygon[j * 2 + 1]), s.hi[2]);
                    ImVec2 sa, sb;
                    if (proj(cam, b.data(), sb))
                        dl->AddCircle(at(sb), r - px(1), is_hot(Grab::Midpoint, (int)i, 1) ? kHot
                                                                                          : IM_COL32(235, 235, 235, 220),
                                      12, px(1.5f));
                    if (proj(cam, a.data(), sa)) {
                        dl->AddCircleFilled(at(sa), r + px(1.5f), IM_COL32(20, 20, 20, 200));
                        dl->AddCircleFilled(at(sa), r, is_hot(Grab::Corner, (int)i, 1) ? kHot
                                                                                        : IM_COL32(245, 245, 245, 255));
                    }
                }
            }
        } else if (_mode == Mode::Rotate && c_ok && ppu > 0) {
            const double rad = px(kRingPx) / ppu;
            for (int k = 0; k < 3; k++) {
                const double* u = &s.R[((k + 1) % 3) * 3];
                const double* w = &s.R[((k + 2) % 3) * 3];
                std::vector<V3> l;
                for (int i = 0; i <= 64; i++) {
                    const double a = 2 * kPi * i / 64;
                    V3 p;
                    for (int t = 0; t < 3; t++)
                        p[t] = s.origin[t] + rad * (std::cos(a) * u[t] + std::sin(a) * w[t]);
                    l.push_back(p);
                }
                draw_polyline(dl, cam, o, l, is_hot(Grab::Ring, k, 1) ? kHot : kAxisCol[k], px(2.5f));
            }
            dl->AddCircleFilled(at(c), px(3.0f), IM_COL32(245, 245, 245, 255));
        }
    }

    if (_drawing) {
        const ImU32 col = IM_COL32(255, 220, 90, 255);
        std::vector<ImVec2> pts;
        for (size_t i = 0; i + 1 < _draw_pts.size(); i += 2) {
            const V3 p = world_of(_draw_frame, _draw_pts[i], _draw_pts[i + 1], 0.0);
            ImVec2 q;
            if (proj(cam, p.data(), q)) pts.push_back(at(q));
        }
        for (size_t i = 0; i + 1 < pts.size(); i++) dl->AddLine(pts[i], pts[i + 1], col, px(2.0f));
        if (!pts.empty() && _in.hovered) {
            const ImVec2 m = at(ImVec2(_in.x, _in.y));
            dl->AddLine(pts.back(), m, IM_COL32(255, 220, 90, 150), px(1.5f));
            if (pts.size() >= 3) dl->AddLine(m, pts.front(), IM_COL32(255, 220, 90, 70), px(1.0f));
        }
        for (size_t i = 0; i < pts.size(); i++) {
            const bool close = i == 0 && pts.size() >= 3 && _in.hovered &&
                               std::hypot(pts[0].x - o.x - _in.x, pts[0].y - o.y - _in.y) < px(10.0f);
            dl->AddCircleFilled(pts[i], close ? px(7.0f) : px(4.0f), close ? kHot : col);
        }
    }

    // What the pointer does here, on the image: a modal grammar nobody can
    // see is a modal grammar nobody uses.
    const spirula::i18n::Msg& hint = _drawing                ? rmsg::hint_draw
                                     : _sel < 0              ? rmsg::hint_none
                                     : _mode == Mode::Adjust ? rmsg::hint_adjust
                                     : _mode == Mode::Move   ? rmsg::hint_move
                                     : _mode == Mode::Resize ? rmsg::hint_resize
                                                             : rmsg::hint_rotate;
    if (_editable) {
        const float wrap = std::max(px(120.0f), v.w - px(20.0f));
        const ImVec2 size = ImGui::CalcTextSize(hint.get(), nullptr, false, wrap);
        const ImVec2 p0(v.x + px(8.0f), v.y + v.h - size.y - px(12.0f));
        dl->AddRectFilled(ImVec2(p0.x - px(5), p0.y - px(4)),
                          ImVec2(p0.x + size.x + px(5), p0.y + size.y + px(4)),
                          IM_COL32(0, 0, 0, 150), px(4.0f));
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), p0, IM_COL32(235, 235, 235, 255),
                    hint.get(), nullptr, wrap);
    }
}

void RoiEditor::handle_keys() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !_editable) return;
    const bool ctrl = io.KeyCtrl;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (_drawing) end_outline();
        else if (_grab.kind != Grab::None) end_drag(false);
        else select(-1);
    }
    if (_drawing) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            finish_outline();
        if ((ImGui::IsKeyPressed(ImGuiKey_Backspace, true) || (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, true))) &&
            _draw_pts.size() >= 2)
            _draw_pts.resize(_draw_pts.size() - 2);
        return;
    }
    if (_grab.kind != Grab::None) return;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, true)) {
        if (io.KeyShift) redo();
        else undo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, true)) redo();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save();
    if (_sel < 0) return;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        RoiShape copy = _doc.shapes[(size_t)_sel];
        copy.name.clear();
        _doc.shapes.insert(_doc.shapes.begin() + _sel + 1, copy);
        _doc.shapes[(size_t)_sel + 1].name = next_name(copy.kind);
        _sel++;
        touch();
        commit();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        _doc.shapes.erase(_doc.shapes.begin() + _sel);
        select(std::min(_sel, (int)_doc.shapes.size() - 1));
        touch();
        commit();
        return;
    }
    if (!ctrl && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_A, false)) _mode = Mode::Adjust;
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) _mode = Mode::Move;
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) _mode = Mode::Resize;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) _mode = Mode::Rotate;
    }
}

// ===========================================================================
// The window
// ===========================================================================

void RoiEditor::draw() {
    if (!_open) return;
    poll_load();
    refresh_overlays();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.88f, vp->WorkSize.y * 0.88f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (!ImGui::Begin(ui::detail::label(rmsg::editor_title), &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) guard([this] { close_now(); });
        return;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) handle_keys();
    ui::TextDisabledRaw(_dataset);

    const float side_w = std::clamp(px(380.0f), px(320.0f), ImGui::GetContentRegionAvail().x * 0.42f);
    ImGui::BeginChild("##roiside", ImVec2(side_w, 0), ImGuiChildFlags_None);
    if (_busy.load()) {
        ui::TextDisabled(pmsg::status_reading);
    } else if (_loaded) {
        draw_file_row();
        if (_editable) {
            draw_start();
            draw_shape_list();
            draw_properties();
            draw_stats();
        }
    } else if (!_status.empty()) {
        ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _status);
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##roiview", ImVec2(0, 0), ImGuiChildFlags_None);
    if (_loaded && _view.preview_active()) {
        if (_editable) draw_toolbar();
        _view.draw(/*training=*/false);
    }
    ImGui::EndChild();
    draw_confirm();
    ImGui::End();
    if (!open) guard([this] { close_now(); });
}

void RoiEditor::draw_file_row() {
    ui::Text(rmsg::lbl_region);
    ImGui::SetNextItemWidth(px(-8.0f));
    const std::string shown = _path.empty() ? std::string(rmsg::file_new.get()) : fs::path(_path).stem().string();
    if (ui::BeginComboRaw("##roifile", (shown + (dirty() ? " *" : "")).c_str())) {
        for (size_t i = 0; i < _files.size(); i++) {
            ImGui::PushID((int)i);
            const std::string stem = fs::path(_files[i]).stem().string();
            if (ui::SelectableRaw(stem, _files[i] == _path)) {
                const std::string f = _files[i];
                guard([this, f] { load_file(f); });
            }
            ImGui::PopID();
        }
        if (ui::Selectable(rmsg::file_new, _path.empty())) guard([this] { new_region(); });
        ImGui::EndCombo();
    }
    if (_files.empty()) ui::TextDisabledWrapped(rmsg::no_files_note);
    else ui::TextDisabledWrapped(rmsg::file_default_note, {fs::path(_files.front()).filename().string()});

    ImGui::BeginDisabled(!_editable);
    ui::Text(rmsg::lbl_name);
    ImGui::SameLine();
    const float bw = ImGui::CalcTextSize(kmsg::save.get()).x + ImGui::CalcTextSize(rmsg::btn_delete.get()).x +
                     4 * ImGui::GetStyle().FramePadding.x + 2 * ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(px(80.0f), ImGui::GetContentRegionAvail().x - bw - px(8.0f)));
    ui::InputTextRaw("##roiname", &_name);
    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty() && !_path.empty() && sanitize_name(_name) == fs::path(_path).stem().string());
    if (ui::Button(kmsg::save)) save();
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(_path.empty());
    if (ui::Button(rmsg::btn_delete)) {
        _confirm = Confirm::Delete;
        _confirm_open = true;
    }
    ImGui::EndDisabled();
    if (!_status.empty()) ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _status);
    if (!_editable) ui::TextColoredWrapped(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), rmsg::not_editable);
    else if (dirty()) ui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), rmsg::unsaved);
    else if (!_notice.empty()) ui::TextDisabledWrapped(rmsg::saved_to, {_path});
}

void RoiEditor::draw_start() {
    ui::SeparatorText(rmsg::start_head);
    const float w = px(-8.0f);
    ImGui::BeginDisabled(_drawing);
    if (ui::Button(rmsg::start_box, ImVec2(w, 0))) start_box();
    ui::help_on_hover(rmsg::start_box_help);
    ImGui::BeginDisabled(!_subject_ok);
    if (ui::Button(rmsg::start_cylinder, ImVec2(w, 0))) start_subject(RoiShapeKind::Cylinder);
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(_subject_ok ? rmsg::start_cylinder_help : rmsg::start_cylinder_off);
    ImGui::BeginDisabled(!_subject_ok);
    if (ui::Button(rmsg::start_ellipsoid, ImVec2(w, 0))) start_subject(RoiShapeKind::Ellipsoid);
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(_subject_ok ? rmsg::start_ellipsoid_help : rmsg::start_cylinder_off);
    if (ui::Button(rmsg::start_outline, ImVec2(w, 0))) start_outline();
    ui::help_on_hover(rmsg::start_outline_help);
    ImGui::EndDisabled();
}

void RoiEditor::draw_shape_list() {
    ui::SeparatorText(rmsg::shapes_head);
    ImGui::BeginDisabled(_drawing);
    ui::Text(rmsg::add_label);
    for (RoiShapeKind k : {RoiShapeKind::Box, RoiShapeKind::Ellipsoid, RoiShapeKind::Cylinder,
                           RoiShapeKind::Prism}) {
        ImGui::SameLine();
        if (ui::Button(kind_label(k))) add_kind(k);
    }
    ImGui::BeginDisabled(_sel < 0);
    if (ui::Button(rmsg::btn_duplicate)) {
        RoiShape copy = _doc.shapes[(size_t)_sel];
        copy.name = next_name(copy.kind);
        _doc.shapes.insert(_doc.shapes.begin() + _sel + 1, copy);
        _sel++;
        touch();
        commit();
    }
    ImGui::SameLine();
    if (ui::Button(rmsg::btn_remove)) {
        _doc.shapes.erase(_doc.shapes.begin() + _sel);
        select(std::min(_sel, (int)_doc.shapes.size() - 1));
        touch();
        commit();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(_sel <= 0);
    if (ui::ArrowButtonRaw("##roiup", ImGuiDir_Up)) {
        std::swap(_doc.shapes[(size_t)_sel], _doc.shapes[(size_t)_sel - 1]);
        _sel--;
        touch();
        commit();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(_sel < 0 || _sel + 1 >= (int)_doc.shapes.size());
    if (ui::ArrowButtonRaw("##roidown", ImGuiDir_Down)) {
        std::swap(_doc.shapes[(size_t)_sel], _doc.shapes[(size_t)_sel + 1]);
        _sel++;
        touch();
        commit();
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(_hist_pos <= 0);
    if (ui::Button(kmsg::undo)) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(_hist_pos + 1 >= (int)_history.size());
    if (ui::Button(kmsg::redo)) redo();
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (_doc.shapes.empty()) {
        ui::TextDisabledWrapped(rmsg::list_empty);
        return;
    }
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
    const float rows = (float)std::min((int)_doc.shapes.size(), 8);
    if (!ImGui::BeginTable("##roishapes", 3, flags,
                           ImVec2(0, ImGui::GetFrameHeightWithSpacing() * (rows + 0.3f))))
        return;
    ui::TableSetupColumnRaw("##on", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
    ui::TableSetupColumnRaw("##op", ImGuiTableColumnFlags_WidthFixed, px(70.0f));
    ui::TableSetupColumnRaw("##name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    for (int i = 0; i < (int)_doc.shapes.size(); i++) {
        RoiShape& s = _doc.shapes[(size_t)i];
        ImGui::PushID(i);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ui::CheckboxRaw("##on", &s.enabled)) {
            touch();
            commit();
        }
        ui::help_on_hover(rmsg::enabled_help);
        ImGui::TableNextColumn();
        const spirula::i18n::Msg& op = s.op == RoiOp::Add ? rmsg::op_add
                                       : s.op == RoiOp::Subtract ? rmsg::op_cut : rmsg::op_overlap;
        ui::TextColored(ImGui::ColorConvertU32ToFloat4(op_color(s.op, 255)), op);
        ImGui::TableNextColumn();
        if (ui::SelectableRaw((s.name + "##sel").c_str(), i == _sel, ImGuiSelectableFlags_SpanAllColumns |
                                                                         ImGuiSelectableFlags_AllowOverlap))
            select(i == _sel ? -1 : i);
        if (ImGui::IsItemHovered()) ui::SetTooltip(kind_label(s.kind));
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void RoiEditor::draw_properties() {
    if (_sel < 0 || _sel >= (int)_doc.shapes.size()) return;
    ImGui::Separator();
    RoiShape& raw = _doc.shapes[(size_t)_sel];
    ui::TextDisabled(kind_label(raw.kind));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(-8.0f));
    if (ui::InputTextRaw("##shapename", &raw.name)) touch();
    if (ImGui::IsItemDeactivatedAfterEdit()) commit();

    int op = (int)raw.op;
    if (ui::RadioButton(rmsg::op_add, op == 0)) op = 0;
    ImGui::SameLine();
    if (ui::RadioButton(rmsg::op_cut, op == 1)) op = 1;
    ImGui::SameLine();
    if (ui::RadioButton(rmsg::op_overlap, op == 2)) op = 2;
    ImGui::SameLine();
    ui::TextDisabledRaw("(?)");
    ui::help_on_hover(rmsg::op_help);
    if (op != (int)raw.op) {
        raw.op = (RoiOp)op;
        touch();
        commit();
    }

    // Dataset units, whatever the view's scale.
    const double unit = std::max(1e-9, shared_size() / std::max(_S.s, 1e-300));
    const float speed = (float)(unit * 0.002);
    const char* fmt = unit > 100 ? "%.2f" : unit > 1 ? "%.3f" : "%.4f";
    const float lw = px(-90.0f);
    auto edited = [this] {
        if (ImGui::IsItemDeactivatedAfterEdit()) commit();
    };

    ImGui::SetNextItemWidth(lw);
    if (ui::DragDoubleNRaw("##pos", raw.origin, 3, speed, 0.0, 0.0, fmt)) touch();
    edited();
    ImGui::SameLine();
    ui::Text(rmsg::lbl_position);
    ui::help_on_hover(rmsg::position_help);

    // Angles in the shared frame, where +Z is up.
    RoiShape s = to_shared(raw);
    double e[3];
    euler_of(s.R, e);
    double deg[3];
    for (int k = 0; k < 3; k++) {
        deg[k] = e[k] * 180 / kPi;
        if (std::fabs(deg[k]) < 0.05) deg[k] = 0.0;   // not "-0.0"
    }
    ImGui::SetNextItemWidth(lw);
    if (ui::DragDoubleNRaw("##rot", deg, 3, 0.5f, 0.0, 0.0, "%.1f")) {
        for (int k = 0; k < 3; k++) e[k] = deg[k] * kPi / 180;
        rotation_of(e, s.R);
        raw = to_raw(s);
        touch();
    }
    edited();
    ImGui::SameLine();
    ui::Text(rmsg::lbl_rotation);
    ui::help_on_hover(rmsg::rotation_help);

    // Each pair of sides along the shape's own axes, from the pivot.
    ui::Text(rmsg::lbl_sides);
    ui::help_on_hover(rmsg::sides_help);
    const double min_len = unit * 2e-5;
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##sides", 4, flags)) {
        ui::TableSetupColumnRaw("##axis", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize());
        ui::TableSetupColumnRaw("-");
        ui::TableSetupColumnRaw("+");
        ui::TableSetupColumn(rmsg::lbl_length);
        ImGui::TableHeadersRow();
        double lo[3], hi[3];
        spirula::roi_extents(raw, lo, hi);
        for (int k = 0; k < 3; k++) {
            ImGui::PushID(k);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ui::TextColoredRaw(ImGui::ColorConvertU32ToFloat4(kAxisCol[k]), k == 0 ? "X" : k == 1 ? "Y" : "Z");
            double a = lo[k], b = hi[k], len = hi[k] - lo[k];
            bool changed = false;
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ui::DragDoubleNRaw("##lo", &a, 1, speed, -1e300, b - min_len, fmt)) changed = true;
            edited();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ui::DragDoubleNRaw("##hi", &b, 1, speed, a + min_len, 1e300, fmt)) changed = true;
            edited();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ui::DragDoubleNRaw("##len", &len, 1, speed, min_len, 1e300, fmt)) {
                const double m = 0.5 * (lo[k] + hi[k]);
                a = m - 0.5 * len;
                b = m + 0.5 * len;
                changed = true;
            }
            edited();
            if (changed) {
                spirula::roi_set_extent(raw, k, std::min(a, b - min_len), std::max(b, a + min_len));
                touch();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ui::Button(rmsg::btn_level)) {
        s = to_shared(raw);
        euler_of(s.R, e);
        e[1] = e[2] = 0;
        rotation_of(e, s.R);
        raw = to_raw(s);
        touch();
        commit();
    }
    ui::help_on_hover(rmsg::btn_level_help);
    ImGui::SameLine();
    if (ui::Button(rmsg::btn_center_pivot)) {
        spirula::roi_center_pivot(raw);
        touch();
        commit();
    }
    ui::help_on_hover(rmsg::btn_center_pivot_help);

    if (raw.kind != RoiShapeKind::Prism) return;
    ImGui::SameLine();
    if (ui::Button(rmsg::btn_redraw)) {
        start_outline(_sel);
        return;
    }
    const size_t n = raw.polygon.size() / 2;
    if (ui::TreeNode(rmsg::lbl_corners, {(long long)n})) {
        int remove = -1;
        for (size_t i = 0; i < n; i++) {
            ImGui::PushID((int)i);
            ui::TextDisabledRaw(std::to_string(i + 1));
            ImGui::SameLine(px(36.0f));
            ImGui::SetNextItemWidth(px(-36.0f));
            if (ui::DragDoubleNRaw("##corner", &raw.polygon[i * 2], 2, speed, 0.0, 0.0, fmt)) touch();
            edited();
            ImGui::SameLine();
            ImGui::BeginDisabled(n <= 3);
            if (ui::ButtonRaw("x##del")) remove = (int)i;
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (remove >= 0) {
            raw.polygon.erase(raw.polygon.begin() + remove * 2, raw.polygon.begin() + remove * 2 + 2);
            touch();
            commit();
        }
        ImGui::TreePop();
    }
}

void RoiEditor::draw_stats() {
    ImGui::Separator();
    const int64_t n = (int64_t)_points.size() / 3, nc = (int64_t)_cams.size() / 3;
    if (!spirula::roi_region(_doc)) {
        ui::TextDisabledWrapped(rmsg::stats_whole);
        return;
    }
    char pct[16];
    std::snprintf(pct, sizeof pct, "%.1f", n > 0 ? 100.0 * (double)_n_inside / (double)n : 0.0);
    ui::Text(rmsg::stats_points, {(long long)_n_inside, (long long)n, pct});
    if (nc > 0) ui::Text(rmsg::stats_cameras, {(long long)_n_cams_inside, (long long)nc});
}

void RoiEditor::draw_toolbar() {
    const float w = px(96.0f);
    ImGui::BeginDisabled(_drawing);
    if (ui::KeyButton(rmsg::mode_adjust, w, "A", _mode == Mode::Adjust)) _mode = Mode::Adjust;
    ImGui::SameLine();
    if (ui::KeyButton(rmsg::mode_move, w, "G", _mode == Mode::Move)) _mode = Mode::Move;
    ImGui::SameLine();
    if (ui::KeyButton(rmsg::mode_resize, w, "S", _mode == Mode::Resize)) _mode = Mode::Resize;
    ImGui::SameLine();
    if (ui::KeyButton(rmsg::mode_rotate, w, "R", _mode == Mode::Rotate)) _mode = Mode::Rotate;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ui::Checkbox(rmsg::show_outlines, &_show_all);
}

void RoiEditor::draw_confirm() {
    if (_confirm == Confirm::None) return;
    const spirula::i18n::Msg& title = _confirm == Confirm::Delete ? rmsg::delete_title : emsg::discard_title;
    if (_confirm_open) {
        ui::OpenPopup(title);
        _confirm_open = false;
    }
    if (!ui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        _confirm = Confirm::None;
        return;
    }
    const std::string what = _path.empty() ? _name : fs::path(_path).filename().string();
    if (_confirm == Confirm::Delete) {
        ui::Text(rmsg::delete_body, {what});
        if (ui::Button(rmsg::btn_delete)) {
            delete_file();
            _confirm = Confirm::None;
            ImGui::CloseCurrentPopup();
        }
    } else {
        ui::Text(emsg::discard_body, {what});
        if (ui::Button(emsg::discard_yes)) {
            std::function<void()> then = std::move(_after_discard);
            _after_discard = nullptr;
            _confirm = Confirm::None;
            ImGui::CloseCurrentPopup();
            _doc = _saved;
            if (then) then();
        }
    }
    ImGui::SameLine();
    if (ui::Button(_confirm == Confirm::Delete ? gmsg::cancel : emsg::discard_no)) {
        _after_discard = nullptr;
        _confirm = Confirm::None;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace gui
