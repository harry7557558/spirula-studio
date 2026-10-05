#include "app/LidarAlign.h"

#include "app/E57Dataset.h"
#include "app/ScanDepth.h"
#include "core/CameraModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <numeric>

namespace app::lidar {
namespace {

using sfm::Mat3;
using sfm::Vec3;

constexpr double kPi = 3.14159265358979323846;
constexpr int kNormalNeighbours = 12;
// Anchors agree with a hypothesis when their rotation is this close; E57 image
// poses and SfM rotations agree to ~0.2 deg on the FJD walk.
constexpr double kAnchorRotTol = 3.0 * kPi / 180.0;

Vec3 col(const Mat3& R, int c) { return {R[c], R[3 + c], R[6 + c]}; }

std::string stem(const std::string& s) {
    const size_t dot = s.find_last_of('.'), slash = s.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return s;
    return s.substr(0, dot);
}

double angle_between(const Mat3& A, const Mat3& B) {
    const Mat3 D = sfm::mul(sfm::transpose(A), B);
    return std::acos(std::clamp((D[0] + D[4] + D[8] - 1.0) / 2.0, -1.0, 1.0));
}

Mat3 chordal_mean(const std::vector<Mat3>& Rs) {
    Mat3 acc{};
    for (const Mat3& R : Rs)
        for (int k = 0; k < 9; k++) acc[k] += R[k];
    return sfm::nearestRotation(acc);
}

double median(std::vector<double> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

// Symmetric 3x3 -> eigenvector of the smallest eigenvalue, and the
// eigenvalues ascending (Jacobi; a covariance is small and well conditioned).
void smallest_eigen(double a[6], double n[3], double ev[3]) {
    double A[3][3] = {{a[0], a[1], a[2]}, {a[1], a[3], a[4]}, {a[2], a[4], a[5]}};
    double V[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int sweep = 0; sweep < 12; sweep++) {
        const double off = A[0][1] * A[0][1] + A[0][2] * A[0][2] + A[1][2] * A[1][2];
        if (off < 1e-30) break;
        for (int p = 0; p < 2; p++)
            for (int q = p + 1; q < 3; q++) {
                if (std::fabs(A[p][q]) < 1e-300) continue;
                const double th = 0.5 * (A[q][q] - A[p][p]) / A[p][q];
                const double t = (th >= 0 ? 1.0 : -1.0) / (std::fabs(th) + std::sqrt(th * th + 1));
                const double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < 3; k++) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; k++) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; k++) {
                    const double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp - s * vkq;
                    V[k][q] = s * vkp + c * vkq;
                }
            }
    }
    int order[3] = {0, 1, 2};
    std::sort(order, order + 3, [&](int i, int j) { return A[i][i] < A[j][j]; });
    for (int k = 0; k < 3; k++) {
        ev[k] = A[order[k]][order[k]];
        n[k] = V[k][order[0]];
    }
}

// Solves the 7x7 normal equations H x = g in place (Gaussian elimination with
// partial pivoting); false when singular.
bool solve7(double H[7][7], double g[7], double x[7]) {
    double M[7][8];
    for (int r = 0; r < 7; r++) {
        for (int c = 0; c < 7; c++) M[r][c] = H[r][c];
        M[r][7] = g[r];
    }
    for (int c = 0; c < 7; c++) {
        int piv = c;
        for (int r = c + 1; r < 7; r++)
            if (std::fabs(M[r][c]) > std::fabs(M[piv][c])) piv = r;
        if (std::fabs(M[piv][c]) < 1e-18) return false;
        for (int k = 0; k < 8; k++) std::swap(M[c][k], M[piv][k]);
        for (int r = 0; r < 7; r++) {
            if (r == c) continue;
            const double f = M[r][c] / M[c][c];
            for (int k = c; k < 8; k++) M[r][k] -= f * M[c][k];
        }
    }
    for (int r = 0; r < 7; r++) x[r] = M[r][7] / M[r][r];
    return true;
}

Mat3 rotation_from_vector(const Vec3& w) {
    const double th = w.norm();
    if (th < 1e-15) return sfm::mat3Identity();
    const Vec3 k = w * (1.0 / th);
    const Mat3 K = sfm::crossMatrix(k);
    const Mat3 K2 = sfm::mul(K, K);
    Mat3 R = sfm::mat3Identity();
    for (int i = 0; i < 9; i++) R[i] += std::sin(th) * K[i] + (1 - std::cos(th)) * K2[i];
    return R;
}

}  // namespace

AlignCloud make_align_cloud(std::vector<double>& xyz, std::vector<uint8_t>& rgb,
                            int64_t target) {
    AlignCloud c;
    c.voxel = thin_to_voxels(xyz, rgb, target);
    const size_t n = xyz.size() / 3;
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (size_t i = 0; i < n; i++)
        for (int a = 0; a < 3; a++) {
            lo[a] = std::min(lo[a], xyz[i * 3 + a]);
            hi[a] = std::max(hi[a], xyz[i * 3 + a]);
        }
    for (int a = 0; a < 3; a++) c.origin[a] = n ? 0.5 * (lo[a] + hi[a]) : 0.0;
    c.xyz.resize(n * 3);
    for (size_t i = 0; i < n; i++)
        for (int a = 0; a < 3; a++) c.xyz[i * 3 + a] = (float)(xyz[i * 3 + a] - c.origin[a]);
    c.rgb = std::move(rgb);
    std::vector<double>().swap(xyz);
    c.tree = std::make_unique<knn::KdTree3>(c.xyz.data(), (int64_t)n);
    c.normal.assign(n * 3, 0.0f);
#pragma omp parallel for schedule(dynamic, 4096)
    for (int64_t i = 0; i < (int64_t)n; i++) {
        float d2[kNormalNeighbours];
        int32_t idx[kNormalNeighbours];
        const int k = c.tree->query(&c.xyz[(size_t)i * 3], -1, kNormalNeighbours, d2, idx);
        if (k < 6) continue;
        double m[3] = {0, 0, 0};
        for (int j = 0; j < k; j++)
            for (int a = 0; a < 3; a++) m[a] += c.xyz[(size_t)idx[j] * 3 + a];
        for (double& v : m) v /= k;
        double cov[6] = {0, 0, 0, 0, 0, 0};
        for (int j = 0; j < k; j++) {
            const float* p = &c.xyz[(size_t)idx[j] * 3];
            const double d[3] = {p[0] - m[0], p[1] - m[1], p[2] - m[2]};
            cov[0] += d[0] * d[0]; cov[1] += d[0] * d[1]; cov[2] += d[0] * d[2];
            cov[3] += d[1] * d[1]; cov[4] += d[1] * d[2]; cov[5] += d[2] * d[2];
        }
        double nv[3], ev[3];
        smallest_eigen(cov, nv, ev);
        // A surface: the thinnest axis well below the middle one.
        if (!(ev[1] > 0) || ev[0] > 0.3 * ev[1]) continue;
        for (int a = 0; a < 3; a++) c.normal[(size_t)i * 3 + a] = (float)nv[a];
    }
    return c;
}

int find_view(const ModelGeometry& m, const std::string& name) {
    const std::string s = stem(name);
    for (size_t i = 0; i < m.views.size(); i++)
        if (m.views[i].name == name || stem(m.views[i].name) == s) return (int)i;
    return -1;
}

AnchorFit fit_anchors(const ModelGeometry& m, const std::vector<Anchor>& anchors,
                      double scale) {
    AnchorFit fit;
    struct Pair { Mat3 Rm, Rs; Vec3 cm, cs; size_t anchor; };
    std::vector<Pair> pairs;
    for (const Anchor& a : anchors) {
        const int v = find_view(m, a.name);
        if (v < 0) continue;
        Pair p;
        p.anchor = (size_t)(&a - anchors.data());
        p.Rm = m.views[(size_t)v].R;
        p.cm = m.views[(size_t)v].centre;
        p.Rs = a.R;
        p.cs = a.centre;
        pairs.push_back(p);
    }
    fit.registered = (int)pairs.size();
    if (pairs.empty()) return fit;

    // Rotation: the model-to-scan turn each anchor implies, R_scan R_model^T;
    // the one most of them agree with, averaged over those.
    std::vector<Mat3> turns(pairs.size());
    for (size_t i = 0; i < pairs.size(); i++)
        turns[i] = sfm::mul(pairs[i].Rs, sfm::transpose(pairs[i].Rm));
    size_t best = 0, best_n = 0;
    for (size_t i = 0; i < turns.size(); i++) {
        size_t n = 0;
        for (size_t j = 0; j < turns.size(); j++)
            n += angle_between(turns[i], turns[j]) < kAnchorRotTol;
        if (n > best_n) { best_n = n; best = i; }
    }
    std::vector<size_t> rot_in;
    std::vector<Mat3> agree;
    for (size_t j = 0; j < turns.size(); j++)
        if (angle_between(turns[best], turns[j]) < kAnchorRotTol) {
            rot_in.push_back(j);
            agree.push_back(turns[j]);
        }
    const Mat3 R = chordal_mean(agree);

    // Scale and translation from the centres, the rotation fixed: RANSAC over
    // pairs of anchors that stand apart, then least squares over the inliers.
    double spread = 0;
    for (size_t a : rot_in)
        for (size_t b : rot_in) spread = std::max(spread, (pairs[a].cs - pairs[b].cs).norm());
    const double tol = std::max(0.05, 0.02 * spread);
    auto inliers_of = [&](double s, const Vec3& t) {
        std::vector<size_t> in;
        for (size_t k : rot_in) {
            const Vec3 p = sfm::mul(R, pairs[k].cm) * s + t;
            if ((p - pairs[k].cs).norm() < tol) in.push_back(k);
        }
        return in;
    };
    double s = scale;
    Vec3 t;
    std::vector<size_t> in;
    if (scale > 0) {
        for (size_t k : rot_in) {
            std::vector<size_t> cand = inliers_of(s, pairs[k].cs - sfm::mul(R, pairs[k].cm) * s);
            if (cand.size() > in.size()) in = cand;
        }
        Vec3 sum;
        for (size_t k : in) sum = sum + (pairs[k].cs - sfm::mul(R, pairs[k].cm) * s);
        t = sum * (1.0 / (double)in.size());
        fit.scale_known = true;
    } else {
        for (size_t ia = 0; ia < rot_in.size(); ia++)
            for (size_t ib = ia + 1; ib < rot_in.size(); ib++) {
                const Pair& A = pairs[rot_in[ia]];
                const Pair& B = pairs[rot_in[ib]];
                const double dm = (A.cm - B.cm).norm(), ds = (A.cs - B.cs).norm();
                // Two anchors closer than the tolerance say nothing of the
                // size (one station's faces would make it 0).
                if (ds < std::max(0.25 * spread, tol) || dm <= 0) continue;
                const double sc = ds / dm;
                const Vec3 tc = A.cs - sfm::mul(R, A.cm) * sc;
                std::vector<size_t> cand = inliers_of(sc, tc);
                if (cand.size() > in.size()) { in = cand; s = sc; t = tc; }
            }
        fit.scale_known = in.size() >= 2 && s > 0;
        Vec3 mm, ms;
        if (!fit.scale_known) in = rot_in;
        for (size_t k : in) { mm = mm + pairs[k].cm; ms = ms + pairs[k].cs; }
        mm = mm * (1.0 / in.size());
        ms = ms * (1.0 / in.size());
        if (!fit.scale_known) {
            // Every anchor at one place (a single station's faces): the turn and
            // the place are known, the size is not.
            s = 1.0;
        } else {
            double num = 0, den = 0;
            for (size_t k : in) {
                const Vec3 a = sfm::mul(R, pairs[k].cm - mm), b = pairs[k].cs - ms;
                num += a.dot(b);
                den += a.dot(a);
            }
            if (den > 0 && num > 0) s = num / den;
        }
        t = ms - sfm::mul(R, mm) * s;
    }
    fit.T.scale = s;
    fit.T.R = R;
    fit.T.t = t;
    fit.inliers = (int)in.size();
    std::vector<double> re, ce;
    for (size_t k : in) {
        re.push_back(angle_between(sfm::mul(R, pairs[k].Rm), pairs[k].Rs) * 180.0 / kPi);
        ce.push_back((sfm::mul(R, pairs[k].cm) * s + t - pairs[k].cs).norm());
    }
    fit.rot_err_deg = median(re);
    fit.centre_err_m = median(ce);
    for (size_t k : in) fit.used.push_back(pairs[k].anchor);
    fit.ok = fit.inliers >= 1;
    return fit;
}

double scale_from_depth(const ModelGeometry& m, const std::vector<Anchor>& anchors,
                        const AnchorFit& fit, const ScanCloud& scan, int64_t* points) {
    // A cube of 90-degree faces around each anchor covers every ray it sees.
    constexpr int kSide = 256, kMaxAnchors = 8, kMinRatios = 20;
    const Vec3 kForward[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    std::vector<double> ratio;
    int used = 0;
    for (size_t ai : fit.used) {
        if (used == kMaxAnchors) break;
        const int vi = find_view(m, anchors[ai].name);
        if (vi < 0 || m.views[(size_t)vi].seen.empty()) continue;
        used++;
        const ModelGeometry::View& v = m.views[(size_t)vi];
        const Vec3 c = anchors[ai].centre;
        for (const Vec3& fw : kForward) {
            const Vec3 up = std::fabs(fw.z) > 0.5 ? Vec3{0, 1, 0} : Vec3{0, 0, 1};
            const Vec3 right = fw.cross(up).normalized();
            const Vec3 down = fw.cross(right);
            MapCamera cam;
            cam.model = (int)CameraModelType::PINHOLE;
            cam.width = cam.height = kSide;
            cam.fx = cam.fy = cam.cx = cam.cy = kSide / 2.0;
            const Vec3 axes[3] = {right, down, fw};
            for (int r = 0; r < 3; r++) {
                for (int k = 0; k < 3; k++) {
                    const Vec3& a = axes[k];
                    cam.c2w[r * 4 + k] = r == 0 ? a.x : r == 1 ? a.y : a.z;
                }
                cam.c2w[r * 4 + 3] = r == 0 ? c.x : r == 1 ? c.y : c.z;
            }
            std::vector<float> depth, normal;
            render_depth_normal(scan, cam, /*ray_depth=*/true, depth, normal);
            for (int p : v.seen) {
                const Vec3 d = sfm::mul(fit.T.R, m.points[(size_t)p] - v.centre);
                const double len = d.norm(), z = d.dot(fw);
                if (!(len > 0) || z <= 0) continue;
                const double u = d.dot(right) / z, w = d.dot(down) / z;
                if (std::fabs(u) > 1 || std::fabs(w) > 1) continue;
                const int x = std::min(kSide - 1, (int)(cam.fx * u + cam.cx));
                const int y = std::min(kSide - 1, (int)(cam.fy * w + cam.cy));
                const float ds = depth[(size_t)y * kSide + x];
                if (ds > 0) ratio.push_back(ds / len);
            }
        }
    }
    if (points) *points = (int64_t)ratio.size();
    if ((int)ratio.size() < kMinRatios) return 0;
    return median(ratio);
}

sfm::Sim3 refine_icp(const ModelGeometry& m, const AlignCloud& cloud,
                     const sfm::Sim3& init, IcpStats* stats) {
    // Points seen from three or more images; the weight saturates at ten.
    std::vector<Vec3> pts;
    std::vector<double> wt;
    for (size_t i = 0; i < m.points.size(); i++) {
        if (m.track[i] < 3) continue;
        pts.push_back(m.points[i]);
        wt.push_back((double)std::min(m.track[i], 10));
    }
    const Vec3 o{cloud.origin[0], cloud.origin[1], cloud.origin[2]};
    // Work in the cloud's local frame: X = s R p + t - o.
    double s = init.scale;
    Mat3 R = init.R;
    Vec3 t = init.t - o;
    const int64_t n = (int64_t)pts.size();
    std::vector<float> res(n), dist(n);
    std::vector<int32_t> nn(n);
    double sigma = -1;
    const double sigma_min = std::max(0.01, 1.5 * cloud.voxel);
    IcpStats st;
    st.points = n;
    if (n < 20 || cloud.size() == 0) {
        if (stats) *stats = st;
        return init;
    }
    for (int it = 0; it < 60; it++) {
#pragma omp parallel for schedule(dynamic, 2048)
        for (int64_t i = 0; i < n; i++) {
            const Vec3 X = sfm::mul(R, pts[(size_t)i]) * s + t;
            const float q[3] = {(float)X.x, (float)X.y, (float)X.z};
            float d2;
            int32_t j;
            nn[(size_t)i] = cloud.tree->query(q, -1, 1, &d2, &j) ? j : -1;
            dist[(size_t)i] = std::sqrt(d2);
            if (nn[(size_t)i] >= 0) {
                const float* nv = &cloud.normal[(size_t)j * 3];
                const float* c = &cloud.xyz[(size_t)j * 3];
                res[(size_t)i] = (float)(nv[0] * (X.x - c[0]) + nv[1] * (X.y - c[1]) +
                                         nv[2] * (X.z - c[2]));
            }
        }
        if (sigma < 0) {
            std::vector<double> d(dist.begin(), dist.end());
            sigma = std::max(sigma_min, 3.0 * median(d));
        }
        double H[7][7] = {}, g[7] = {};
        std::vector<double> absr;
        int64_t used = 0;
        for (int64_t i = 0; i < n; i++) {
            const int32_t j = nn[(size_t)i];
            if (j < 0) continue;
            const float* nv = &cloud.normal[(size_t)j * 3];
            if (nv[0] == 0 && nv[1] == 0 && nv[2] == 0) continue;
            if (dist[(size_t)i] > 3 * sigma) continue;
            const double r = res[(size_t)i];
            // Geman-McClure: a correspondence fades out past sigma.
            const double k = sigma * sigma / (sigma * sigma + r * r);
            const double w = wt[(size_t)i] * k * k;
            const Vec3 X = sfm::mul(R, pts[(size_t)i]) * s + t;
            const Vec3 nvec{nv[0], nv[1], nv[2]};
            const Vec3 xn = X.cross(nvec);
            const double J[7] = {xn.x, xn.y, xn.z, nvec.x, nvec.y, nvec.z, nvec.dot(X)};
            for (int a = 0; a < 7; a++) {
                g[a] -= w * J[a] * r;
                for (int b = 0; b < 7; b++) H[a][b] += w * J[a] * J[b];
            }
            absr.push_back(std::fabs(r));
            used++;
        }
        if (used < 20) break;
        for (int a = 0; a < 7; a++) H[a][a] += 1e-9;
        double x[7];
        if (!solve7(H, g, x)) break;
        st.iterations = it + 1;
        const Mat3 dR = rotation_from_vector({x[0], x[1], x[2]});
        const double ds = 1.0 + x[6];
        s *= ds;
        R = sfm::mul(dR, R);
        t = sfm::mul(dR, t) * ds + Vec3{x[3], x[4], x[5]};
        const double med = median(absr);
        st.median_m = med;
        int64_t close = 0;
        for (double v : absr) close += v < sigma;
        st.inlier_frac = (double)close / (double)n;
        const double step = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]) + std::fabs(x[6]);
        const double next = std::max({sigma * 0.7, sigma_min, 3.0 * 1.4826 * med});
        if (step < 1e-7 && next >= sigma) break;
        sigma = next;
    }
    if (stats) *stats = st;
    sfm::Sim3 out;
    out.scale = s;
    out.R = R;
    out.t = t + o;
    return out;
}

std::vector<float> scan_distances(const ModelGeometry& m, const AlignCloud& cloud,
                                  const sfm::Sim3& T, double cap) {
    const std::vector<Vec3>& pts = m.points;
    const Vec3 o{cloud.origin[0], cloud.origin[1], cloud.origin[2]};
    std::vector<float> out(pts.size(), std::numeric_limits<float>::quiet_NaN());
#pragma omp parallel for schedule(dynamic, 2048)
    for (int64_t i = 0; i < (int64_t)pts.size(); i++) {
        const Vec3 X = sfm::transformPoint(T, pts[(size_t)i]) - o;
        const float q[3] = {(float)X.x, (float)X.y, (float)X.z};
        float d2;
        if (cloud.tree->query(q, -1, 1, &d2) && std::sqrt(d2) <= cap)
            out[(size_t)i] = std::sqrt(d2);
    }
    return out;
}

std::string describe(const sfm::Sim3& T) {
    // Rotation angle, scale and translation, for the log.
    const double ang = std::acos(std::clamp((T.R[0] + T.R[4] + T.R[8] - 1.0) / 2.0, -1.0, 1.0));
    char b[160];
    std::snprintf(b, sizeof b, "scale %.6g, rotation %.2f deg, translation (%.3f, %.3f, %.3f)",
                  T.scale, ang * 180.0 / kPi, T.t.x, T.t.y, T.t.z);
    return b;
}

ScanFrames group_scan_frames(const std::vector<ModelGeometry>& models,
                             const std::vector<Anchor>& anchors,
                             const std::vector<int>& scan_of, int scans) {
    std::vector<int> parent((size_t)scans);
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](int k) {
        while (parent[(size_t)k] != k) k = parent[(size_t)k] = parent[(size_t)parent[(size_t)k]];
        return k;
    };
    std::vector<int64_t> support((size_t)scans, 0);
    for (const ModelGeometry& m : models) {
        std::vector<Anchor> left;
        std::vector<int> left_scan;
        for (size_t i = 0; i < anchors.size(); i++) {
            if (scan_of[i] < 0 || find_view(m, anchors[i].name) < 0) continue;
            left.push_back(anchors[i]);
            left_scan.push_back(scan_of[i]);
            support[(size_t)scan_of[i]]++;
        }
        // The scans most of whose anchors one fit holds are one frame; the
        // rest are fitted again, until none are left.
        while (!left.empty()) {
            const AnchorFit fit = fit_anchors(m, left);
            if (!fit.ok) break;
            std::map<int, std::pair<int, int>> held;   // scan -> (inliers, anchors)
            for (int sc : left_scan) held[sc].second++;
            for (size_t k : fit.used) held[left_scan[k]].first++;
            std::vector<int> joined;
            int most = -1;
            for (const auto& [sc, n] : held) {
                if (n.first > 0 && 2 * n.first >= n.second) joined.push_back(sc);
                if (most < 0 || n.first > held[most].first) most = sc;
            }
            if (joined.empty()) joined.push_back(most);
            for (int sc : joined) parent[(size_t)root(sc)] = root(joined[0]);
            std::vector<Anchor> rest;
            std::vector<int> rest_scan;
            for (size_t k = 0; k < left.size(); k++) {
                if (std::find(joined.begin(), joined.end(), left_scan[k]) != joined.end()) continue;
                rest.push_back(left[k]);
                rest_scan.push_back(left_scan[k]);
            }
            left.swap(rest);
            left_scan.swap(rest_scan);
        }
    }
    std::map<int, int64_t> by_root;
    for (int k = 0; k < scans; k++)
        if (support[(size_t)k] > 0) by_root[root(k)] += support[(size_t)k];
    std::vector<std::pair<int64_t, int>> order;
    for (const auto& [r, n] : by_root) order.push_back({-n, r});
    std::sort(order.begin(), order.end());
    ScanFrames out;
    out.frame.assign((size_t)scans, -1);
    if (order.size() <= 1) {
        out.frame.assign((size_t)scans, 0);
        out.count = 1;
        return out;
    }
    for (int k = 0; k < scans; k++) {
        if (support[(size_t)k] == 0) continue;
        for (size_t f = 0; f < order.size(); f++)
            if (order[f].second == root(k)) out.frame[(size_t)k] = (int)f;
    }
    out.count = (int)order.size();
    return out;
}

bool scans_share_frame(const std::vector<std::vector<spirula::cloud::Station>>& stations) {
    if (stations.size() <= 1) return true;
    // A file without a station of its own stands for one at its origin.
    std::vector<std::vector<Vec3>> at;
    int bare = 0;
    for (const auto& file : stations) {
        at.emplace_back();
        for (const spirula::cloud::Station& st : file)
            at.back().push_back({st.origin[0], st.origin[1], st.origin[2]});
        if (file.empty()) {
            bare++;
            at.back().push_back({0, 0, 0});
        }
    }
    if (bare > 1) return false;
    constexpr double kSamePlace = 0.01;   // metres
    for (size_t i = 0; i < at.size(); i++)
        for (size_t j = i + 1; j < at.size(); j++)
            for (const Vec3& a : at[i])
                for (const Vec3& b : at[j])
                    if ((a - b).norm() < kSamePlace) return false;
    return true;
}

std::vector<FramePlacement> place_frames(const std::vector<ModelGeometry>& models,
                                         const std::vector<std::optional<sfm::Sim3>>& fits,
                                         const std::vector<Anchor>& anchors,
                                         const std::vector<int>& scan_of,
                                         const ScanFrames& frames) {
    std::vector<FramePlacement> out((size_t)frames.count);
    if (out.empty()) return out;
    out[0].placed = true;
    auto frame_of = [&](size_t i) {
        return scan_of[i] < 0 ? -1 : frames.frame[(size_t)scan_of[i]];
    };
    for (int f = 1; f < frames.count; f++) {
        std::vector<Anchor> best;
        int best_m = -1;
        for (size_t m = 0; m < models.size(); m++) {
            if (!fits[m]) continue;
            std::vector<Anchor> mine;
            for (size_t i = 0; i < anchors.size(); i++)
                if (frame_of(i) == f && find_view(models[m], anchors[i].name) >= 0)
                    mine.push_back(anchors[i]);
            if (mine.size() > best.size()) {
                best.swap(mine);
                best_m = (int)m;
            }
        }
        if (best_m < 0) continue;
        const sfm::Sim3& T0 = *fits[(size_t)best_m];
        const AnchorFit fit = fit_anchors(models[(size_t)best_m], best, T0.scale);
        if (!fit.ok) continue;
        FramePlacement& p = out[(size_t)f];
        p.placed = true;
        p.T = sfm::composeSim3(T0, sfm::invertSim3(fit.T));
        p.T.scale = 1.0;
        p.model = best_m;
        p.anchors = fit.inliers;
    }
    return out;
}

}  // namespace app::lidar
