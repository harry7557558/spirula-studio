// pose_normalization_test -- compute_normalized_transform's `gsplat` method
// (orientation_method / center_method; docs/notes/pose-normalization.md).
//
// A scene is built upright (a slab of points with its mass toward the floor,
// cameras on a ring looking at it, up = +z), then moved into the frame a file
// might come in -- any rotation, any offset, any up axis -- and the normalized
// frame it comes out in must not care: principal axes on the coordinate axes
// with the thinnest along z, the floor below the median, the cameras' spread
// setting the scale. No GPU, no dataset on disk.

#include "data/DatasetParser.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

using V3 = std::array<double, 3>;
using M3 = std::array<std::array<double, 3>, 3>;

V3 mv(const M3& M, const V3& v) {
    return {M[0][0]*v[0] + M[0][1]*v[1] + M[0][2]*v[2],
            M[1][0]*v[0] + M[1][1]*v[1] + M[1][2]*v[2],
            M[2][0]*v[0] + M[2][1]*v[1] + M[2][2]*v[2]};
}
M3 mm(const M3& A, const M3& B) {
    M3 C{};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            C[r][c] = A[r][0]*B[0][c] + A[r][1]*B[1][c] + A[r][2]*B[2][c];
    return C;
}
V3 cross(const V3& a, const V3& b) {
    return {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]};
}
double dot(const V3& a, const V3& b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
V3 unit(V3 v) {
    const double l = std::sqrt(dot(v, v));
    for (double& x : v) x /= l;
    return v;
}
double det(const M3& M) {
    return M[0][0]*(M[1][1]*M[2][2] - M[1][2]*M[2][1])
         - M[0][1]*(M[1][0]*M[2][2] - M[1][2]*M[2][0])
         + M[0][2]*(M[1][0]*M[2][1] - M[1][1]*M[2][0]);
}

// Rodrigues, axis need not be unit.
M3 rotation(V3 axis, double angle) {
    axis = unit(axis);
    const double c = std::cos(angle), s = std::sin(angle), C = 1.0 - c;
    return {{{c + axis[0]*axis[0]*C, axis[0]*axis[1]*C - axis[2]*s, axis[0]*axis[2]*C + axis[1]*s},
             {axis[1]*axis[0]*C + axis[2]*s, c + axis[1]*axis[1]*C, axis[1]*axis[2]*C - axis[0]*s},
             {axis[2]*axis[0]*C - axis[1]*s, axis[2]*axis[1]*C + axis[0]*s, c + axis[2]*axis[2]*C}}};
}

struct Rng {
    uint64_t s;
    double next() {   // [0, 1)
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(s >> 11) / (double)(1ULL << 53);
    }
};

struct Scene {
    std::vector<double> c2w;   // [N, 3, 4] OpenGL, row-major
    std::vector<double> pts;   // [M, 3]
    int64_t n = 0, m = 0;
};

// Upright scene -- up +z -- then `Q` and `shift` carry it into the file's frame.
// `floor_heavy`: most points near z = 0 with a thin tail up (a ground plane).
Scene make_scene(const M3& Q, const V3& shift, bool floor_heavy, int n_cams = 24,
                 int n_pts = 4000, uint64_t seed = 7) {
    Scene sc;
    Rng rng{seed};
    sc.n = n_cams;
    sc.c2w.resize(n_cams * 12);
    for (int i = 0; i < n_cams; i++) {
        const double a = 2.0 * kPi * i / n_cams;
        const V3 pos = {8.0 * std::cos(a), 8.0 * std::sin(a), 3.0};
        const V3 dir = unit({-pos[0], -pos[1], -pos[2]});
        const V3 up = {0, 0, 1};
        const V3 x = unit(cross(dir, up));
        const V3 z = {-dir[0], -dir[1], -dir[2]};
        const V3 y = cross(z, x);
        const M3 Rc = {{{x[0], y[0], z[0]}, {x[1], y[1], z[1]}, {x[2], y[2], z[2]}}};
        const M3 Rq = mm(Q, Rc);
        V3 t = mv(Q, pos);
        for (int k = 0; k < 3; k++) t[k] += shift[k];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) sc.c2w[i*12 + r*4 + c] = Rq[r][c];
            sc.c2w[i*12 + r*4 + 3] = t[r];
        }
    }
    sc.m = n_pts;
    sc.pts.resize(n_pts * 3);
    for (int i = 0; i < n_pts; i++) {
        const double u = rng.next();
        V3 p = {-10.0 + 20.0 * rng.next(), -5.0 + 10.0 * rng.next(),
                0.5 * (floor_heavy ? u*u*u : 1.0 - u*u*u)};
        p = mv(Q, p);
        for (int k = 0; k < 3; k++) sc.pts[i*3 + k] = p[k] + shift[k];
    }
    return sc;
}

struct Result {
    double T[16], R[9], scale;
    bool fell_back;
};

Result normalize(const Scene& sc, const std::string& orient, const std::string& center,
                 bool auto_scale = true, bool with_points = true) {
    Result r{};
    r.scale = dsparse::compute_normalized_transform(
        sc.c2w.data(), sc.n, r.T, r.R, nullptr, orient, center, auto_scale,
        with_points ? sc.pts.data() : nullptr, with_points ? sc.m : 0, &r.fell_back);
    return r;
}

M3 R_of(const Result& r) {
    return {{{r.R[0], r.R[1], r.R[2]}, {r.R[3], r.R[4], r.R[5]}, {r.R[6], r.R[7], r.R[8]}}};
}
V3 apply(const Result& r, const V3& p) {
    return {r.T[0]*p[0] + r.T[1]*p[1] + r.T[2]*p[2] + r.T[3],
            r.T[4]*p[0] + r.T[5]*p[1] + r.T[6]*p[2] + r.T[7],
            r.T[8]*p[0] + r.T[9]*p[1] + r.T[10]*p[2] + r.T[11]};
}

bool is_rotation(const M3& R, double tol = 1e-9) {
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            double d = 0;
            for (int k = 0; k < 3; k++) d += R[a][k] * R[b][k];
            if (std::abs(d - (a == b ? 1.0 : 0.0)) > tol) return false;
        }
    return std::abs(det(R) - 1.0) < tol;
}

double median_of(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Checks one scene, named `what`. `up_in_file` is the scene's up in the frame
// the file came in; when `expect_up` the floor must come out below.
void check_scene(const Scene& sc, const V3& up_in_file, bool expect_up,
                 const std::string& what) {
    const Result r = normalize(sc, "gsplat", "gsplat");
    check(!r.fell_back, what + ": no fallback with points");
    check(is_rotation(R_of(r)), what + ": R is a proper rotation");

    bool consistent = r.scale > 0.0;
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++)
            consistent = consistent &&
                std::abs(r.T[a*4 + b] - r.scale * r.R[a*3 + b]) < 1e-12 * std::max(1.0, r.scale);
    check(consistent, what + ": T is scale * [R | t]");

    // The points in the normalized frame.
    std::vector<V3> pn(sc.m);
    V3 mean{0, 0, 0};
    for (int64_t i = 0; i < sc.m; i++) {
        pn[i] = apply(r, {sc.pts[i*3], sc.pts[i*3 + 1], sc.pts[i*3 + 2]});
        for (int k = 0; k < 3; k++) mean[k] += pn[i][k] / (double)sc.m;
    }
    double var[3] = {}, off[3] = {};   // var x/y/z, and cov xy, xz, yz
    for (const V3& p : pn) {
        const V3 d = {p[0] - mean[0], p[1] - mean[1], p[2] - mean[2]};
        for (int k = 0; k < 3; k++) var[k] += d[k] * d[k] / (double)sc.m;
        off[0] += d[0]*d[1] / (double)sc.m;
        off[1] += d[0]*d[2] / (double)sc.m;
        off[2] += d[1]*d[2] / (double)sc.m;
    }
    check(var[0] > var[1] && var[1] > var[2],
          what + ": principal axes in order, thinnest along z");
    check(std::abs(off[0]) < 0.05 * var[0] && std::abs(off[1]) < 0.05 * var[0] &&
              std::abs(off[2]) < 0.05 * var[0],
          what + ": axes are the covariance's own (no cross terms)");
    const double extent_x = 20.0 * r.scale;
    check(std::abs(mean[0]) < 0.05 * extent_x && std::abs(mean[1]) < 0.05 * extent_x,
          what + ": centred in the plane");

    std::vector<double> zs(sc.m);
    for (int64_t i = 0; i < sc.m; i++) zs[i] = pn[i][2];
    check(median_of(zs) <= mean[2], what + ": the floor is below (median height <= mean)");

    if (expect_up) {
        const V3 up = mv(R_of(r), up_in_file);
        check(std::abs(up[0]) < 0.05 && std::abs(up[1]) < 0.05 && up[2] > 0.99,
              what + ": the cameras' up is +z");
    }

    // Scale: the farthest camera from the cameras' mean is at 1 / 1.1.
    std::vector<V3> cp(sc.n);
    V3 cm{0, 0, 0};
    for (int64_t i = 0; i < sc.n; i++) {
        cp[i] = apply(r, {sc.c2w[i*12 + 3], sc.c2w[i*12 + 7], sc.c2w[i*12 + 11]});
        for (int k = 0; k < 3; k++) cm[k] += cp[i][k] / (double)sc.n;
    }
    double farthest_dist = 0;
    for (const V3& p : cp)
        farthest_dist = std::max(farthest_dist, std::sqrt(dot({p[0]-cm[0], p[1]-cm[1], p[2]-cm[2]},
                                                              {p[0]-cm[0], p[1]-cm[1], p[2]-cm[2]})));
    check(std::abs(farthest_dist - 1.0 / 1.1) < 1e-9, what + ": cameras span 1/1.1");
}

}  // namespace

int main() {
    const M3 I = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

    // 1. The upright scene as it is, and in other frames.
    check_scene(make_scene(I, {0, 0, 0}, true), {0, 0, 1}, true, "identity");
    {
        const M3 Q = rotation({1, 2, 3}, 1.1);
        check_scene(make_scene(Q, {100, -50, 30}, true), mv(Q, {0, 0, 1}), true,
                    "rotated + offset");
    }
    {
        const M3 Q = rotation({-3, 1, 0.5}, 2.6);
        check_scene(make_scene(Q, {1e5, 2e5, -3e4}, true, 31, 6000, 99), mv(Q, {0, 0, 1}),
                    true, "rotated, far from the origin");
    }
    {
        // A y-up file (COLMAP/nerfstudio cameras often are): the levelling
        // starts from exactly opposite up vectors, where the closed-form
        // rotation divides by zero.
        const M3 Q = rotation({1, 0, 0}, -kPi / 2);   // z -> y
        const V3 up = mv(Q, {0, 0, 1});
        check(std::abs(up[1] - 1.0) < 1e-12, "y-up setup puts +z on +y");
        check_scene(make_scene(Q, {3, 4, 5}, true), up, true, "y-up file");
    }
    {
        // Nearly y-up, just off the singular case.
        const M3 Q = mm(rotation({0, 0, 1}, 0.3), rotation({1, 0, 0}, -kPi / 2 + 1e-5));
        check_scene(make_scene(Q, {0, 0, 0}, true), mv(Q, {0, 0, 1}), true,
                    "almost y-up file");
    }
    {
        // Same scene, same answer for an equal input: no dependence on eigensolver luck.
        const M3 Q = rotation({0.3, -1, 0.2}, 0.8);
        const Scene a = make_scene(Q, {1, 2, 3}, true), b = make_scene(Q, {1, 2, 3}, true);
        const Result ra = normalize(a, "gsplat", "gsplat"), rb = normalize(b, "gsplat", "gsplat");
        bool same = true;
        for (int k = 0; k < 16; k++) same = same && ra.T[k] == rb.T[k];
        check(same, "deterministic");
    }

    // 2. Mass at the top: gsplat's heuristic takes that for an upside-down
    // scene and turns it over, whatever the cameras' up says.
    {
        const M3 Q = rotation({1, 1, 0}, 0.7);
        check_scene(make_scene(Q, {5, 5, 5}, false), mv(Q, {0, 0, 1}), false, "mass at the top");
    }

    // 3. Semantics.
    {
        const M3 Q = rotation({1, 2, 3}, 1.1);
        const Scene sc = make_scene(Q, {100, -50, 30}, true);
        const Result both = normalize(sc, "gsplat", "gsplat");

        // Either name selects the joint method; the other option is ignored.
        const Result only_o = normalize(sc, "gsplat", "poses");
        const Result only_c = normalize(sc, "up", "gsplat");
        bool same = true;
        for (int k = 0; k < 16; k++)
            same = same && both.T[k] == only_o.T[k] && both.T[k] == only_c.T[k];
        check(same, "either option selects gsplat for both");

        const Result off = normalize(sc, "gsplat", "gsplat", /*auto_scale=*/false);
        check(off.scale == 1.0 && is_rotation(R_of(off)), "auto_scale_poses off: scale 1");
        bool rigid = true;
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++) rigid = rigid && std::abs(off.T[a*4 + b] - both.R[a*3 + b]) < 1e-12;
        check(rigid, "auto_scale_poses off: T is the rigid part alone");

        // Unchanged for the other methods, with or without points.
        const Result up_poses = normalize(sc, "up", "poses");
        const Result up_poses_pts = normalize(sc, "up", "poses", true, false);
        bool unchanged = up_poses.scale == up_poses_pts.scale && !up_poses.fell_back;
        for (int k = 0; k < 16; k++) unchanged = unchanged && up_poses.T[k] == up_poses_pts.T[k];
        check(unchanged, "up/poses ignore the points and never report a fallback");
        check(!normalize(sc, "pca", "focus").fell_back, "pca/focus: no fallback flag");

        // Without usable points: up/poses, flagged.
        const Result none = normalize(sc, "gsplat", "gsplat", true, false);
        bool fallback_same = none.fell_back && none.scale == up_poses.scale;
        for (int k = 0; k < 16; k++) fallback_same = fallback_same && none.T[k] == up_poses.T[k];
        check(fallback_same, "no points: falls back to up/poses and says so");

        Scene few = sc;
        few.m = 3;
        check(normalize(few, "gsplat", "gsplat").fell_back, "three points: falls back");

        Scene flat = sc;
        for (int64_t i = 0; i < flat.m; i++)
            for (int k = 0; k < 3; k++) flat.pts[i*3 + k] = 7.0;
        check(normalize(flat, "gsplat", "gsplat").fell_back, "all points identical: falls back");

        Scene nan = sc;
        nan.pts[0] = std::numeric_limits<double>::quiet_NaN();
        nan.pts[4] = std::numeric_limits<double>::infinity();
        const Result rn = normalize(nan, "gsplat", "gsplat");
        bool close = !rn.fell_back;
        for (int k = 0; k < 16; k++) close = close && std::isfinite(rn.T[k]);
        check(close, "non-finite points are skipped, not propagated");

        Scene no_cams = sc;
        no_cams.n = 0;
        const Result rz = normalize(no_cams, "gsplat", "gsplat");
        check(rz.scale == 1.0 && rz.T[0] == 1.0 && rz.T[5] == 1.0 && rz.T[10] == 1.0,
              "no cameras: identity");
    }

    // 4. A stray camera does not set the scale (outlier cameras are kept in
    // the set but must not own the scene's size).
    {
        Scene sc = make_scene(I, {0, 0, 0}, true);
        const Result clean = normalize(sc, "gsplat", "gsplat");
        sc.c2w[3] = 1e6;   // first camera, 1e6 units out
        const Result stray = normalize(sc, "gsplat", "gsplat");
        check(stray.scale > 0.5 * clean.scale && stray.scale < 2.0 * clean.scale,
              "a stray camera does not set the scale");
    }

    // 5. Tiny and huge scenes resolve their axes alike.
    for (double s : {1e-6, 1e6}) {
        Scene sc = make_scene(I, {0, 0, 0}, true);
        for (double& v : sc.pts) v *= s;
        for (int64_t i = 0; i < sc.n; i++)
            for (int r = 0; r < 3; r++) sc.c2w[i*12 + r*4 + 3] *= s;
        const Result r = normalize(sc, "gsplat", "gsplat");
        char what[64];
        std::snprintf(what, sizeof what, "scene scaled by %g", s);
        const V3 up = mv(R_of(r), {0, 0, 1});
        check(!r.fell_back && is_rotation(R_of(r)) && up[2] > 0.99, what);
    }

    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}
