// SceneAlign.cpp -- see SceneAlign.h.

#include "core/SceneAlign.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <random>

namespace spirula {
namespace align {

namespace {

// RANSAC scores a hypothesis over at most this many points: the winner is
// then recounted over all of them, so the cap costs accuracy nowhere.
constexpr int64_t kScoreCap = 40000;
constexpr int kIterations = 500;

double dot(const double a[3], const double b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
void cross(const double a[3], const double b[3], double o[3]) {
    o[0] = a[1]*b[2] - a[2]*b[1];
    o[1] = a[2]*b[0] - a[0]*b[2];
    o[2] = a[0]*b[1] - a[1]*b[0];
}
bool normalize(double v[3]) {
    const double l = std::sqrt(dot(v, v));
    if (!(l > 1e-300)) return false;
    for (int i = 0; i < 3; i++) v[i] /= l;
    return true;
}

// Smallest-eigenvalue eigenvector of a symmetric 3x3 by cyclic Jacobi.
void smallest_eigenvector(double A[9], double out[3]) {
    double V[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (int sweep = 0; sweep < 32; sweep++) {
        double off = A[1]*A[1] + A[2]*A[2] + A[5]*A[5];
        if (off < 1e-30) break;
        for (int p = 0; p < 2; p++)
            for (int q = p + 1; q < 3; q++) {
                const double apq = A[p*3+q];
                if (std::fabs(apq) < 1e-300) continue;
                const double th = (A[q*3+q] - A[p*3+p]) / (2.0 * apq);
                const double t = (th >= 0 ? 1.0 : -1.0) /
                                 (std::fabs(th) + std::sqrt(th*th + 1.0));
                const double c = 1.0 / std::sqrt(t*t + 1.0), s = t * c;
                for (int k = 0; k < 3; k++) {
                    const double akp = A[k*3+p], akq = A[k*3+q];
                    A[k*3+p] = c*akp - s*akq;
                    A[k*3+q] = s*akp + c*akq;
                }
                for (int k = 0; k < 3; k++) {
                    const double apk = A[p*3+k], aqk = A[q*3+k];
                    A[p*3+k] = c*apk - s*aqk;
                    A[q*3+k] = s*apk + c*aqk;
                }
                for (int k = 0; k < 3; k++) {
                    const double vkp = V[k*3+p], vkq = V[k*3+q];
                    V[k*3+p] = c*vkp - s*vkq;
                    V[k*3+q] = s*vkp + c*vkq;
                }
            }
    }
    int lo = 0;
    if (A[4] < A[lo*4]) lo = 1;
    if (A[8] < A[lo*4]) lo = 2;
    for (int k = 0; k < 3; k++) out[k] = V[k*3+lo];
}

// Least squares over the flagged points. Keeps the normal's side.
bool refit(const double* pts, int64_t n, const std::vector<uint8_t>& in,
           Plane& pl) {
    double c[3] = {0, 0, 0};
    int64_t m = 0;
    for (int64_t i = 0; i < n; i++) {
        if (!in[(size_t)i]) continue;
        for (int k = 0; k < 3; k++) c[k] += pts[i*3+k];
        m++;
    }
    if (m < 3) return false;
    for (int k = 0; k < 3; k++) c[k] /= (double)m;
    double A[9] = {0};
    for (int64_t i = 0; i < n; i++) {
        if (!in[(size_t)i]) continue;
        const double d[3] = {pts[i*3]-c[0], pts[i*3+1]-c[1], pts[i*3+2]-c[2]};
        for (int r = 0; r < 3; r++)
            for (int q = 0; q < 3; q++) A[r*3+q] += d[r] * d[q];
    }
    double nrm[3];
    smallest_eigenvector(A, nrm);
    if (!normalize(nrm)) return false;
    if (dot(nrm, pl.n) < 0)
        for (double& v : nrm) v = -v;
    for (int k = 0; k < 3; k++) pl.n[k] = nrm[k];
    pl.d = -dot(nrm, c);
    return true;
}

int64_t mark_inliers(const double* pts, int64_t n, const Plane& pl, double tol,
                     std::vector<uint8_t>& in) {
    in.assign((size_t)n, 0);
    int64_t m = 0;
    for (int64_t i = 0; i < n; i++)
        if (std::fabs(pl.distance(pts + i*3)) <= tol) {
            in[(size_t)i] = 1;
            m++;
        }
    return m;
}

}  // namespace


bool fit_plane(const double* pts, int64_t n, double tol, uint32_t seed,
               Plane& out, std::vector<uint8_t>* inlier) {
    if (n < 3) return false;
    std::mt19937 rng(seed);
    const int64_t step = std::max<int64_t>(1, n / kScoreCap);
    Plane best;
    int64_t best_count = 0;
    for (int it = 0; it < kIterations; it++) {
        const double* a = pts + 3 * (int64_t)(rng() % (uint64_t)n);
        const double* b = pts + 3 * (int64_t)(rng() % (uint64_t)n);
        const double* c = pts + 3 * (int64_t)(rng() % (uint64_t)n);
        const double e1[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]};
        const double e2[3] = {c[0]-a[0], c[1]-a[1], c[2]-a[2]};
        Plane h;
        cross(e1, e2, h.n);
        if (!normalize(h.n)) continue;
        h.d = -dot(h.n, a);
        int64_t count = 0;
        for (int64_t i = 0; i < n; i += step)
            if (std::fabs(h.distance(pts + i*3)) <= tol) count++;
        if (count > best_count) {
            best_count = count;
            best = h;
        }
    }
    if (best_count < 3) return false;
    std::vector<uint8_t> in;
    // Two rounds: the first refit moves the plane, which moves who is on it.
    for (int round = 0; round < 2; round++) {
        if (mark_inliers(pts, n, best, tol, in) < 3) return false;
        if (!refit(pts, n, in, best)) return false;
    }
    best.inliers = mark_inliers(pts, n, best, tol, in);
    out = best;
    if (inlier) inlier->swap(in);
    return best.inliers >= 3;
}

std::vector<Plane> find_planes(const double* pts, int64_t n, double tol, int k,
                               double min_frac) {
    std::vector<Plane> out;
    std::vector<double> rest(pts, pts + n * 3);
    for (int i = 0; i < k; i++) {
        const int64_t m = (int64_t)rest.size() / 3;
        Plane pl;
        std::vector<uint8_t> in;
        if (!fit_plane(rest.data(), m, tol, 977u + (uint32_t)i, pl, &in)) break;
        if ((double)pl.inliers < min_frac * (double)n) break;
        out.push_back(pl);
        std::vector<double> next;
        next.reserve(rest.size());
        for (int64_t j = 0; j < m; j++)
            if (!in[(size_t)j])
                next.insert(next.end(), rest.begin() + j*3, rest.begin() + j*3 + 3);
        rest.swap(next);
    }
    return out;
}

// Least quantile of squares THROUGH the click: smallest 35th-percentile
// residual. No tolerance to choose (a lawn is centimetres thick, a tabletop is
// not), and a wall that outnumbers the floor still misses the click.
static bool fit_plane_lqs(const double* pts, int64_t n, const double at[3],
                          uint32_t seed, Plane& out) {
    if (n < 8) return false;
    std::mt19937 rng(seed);
    const int64_t step = std::max<int64_t>(1, n / 4000);
    std::vector<double> res;
    res.reserve((size_t)(n / step + 1));
    double best = -1.0;
    for (int it = 0; it < kIterations; it++) {
        const double* b = pts + 3 * (int64_t)(rng() % (uint64_t)n);
        const double* c = pts + 3 * (int64_t)(rng() % (uint64_t)n);
        const double e1[3] = {b[0]-at[0], b[1]-at[1], b[2]-at[2]};
        const double e2[3] = {c[0]-at[0], c[1]-at[1], c[2]-at[2]};
        Plane h;
        cross(e1, e2, h.n);
        if (!normalize(h.n)) continue;
        h.d = -dot(h.n, at);
        res.clear();
        for (int64_t i = 0; i < n; i += step) res.push_back(std::fabs(h.distance(pts + i*3)));
        const size_t k = res.size() * 35 / 100;
        std::nth_element(res.begin(), res.begin() + (ptrdiff_t)k, res.end());
        if (best < 0 || res[k] < best) {
            best = res[k];
            out = h;
        }
    }
    return best >= 0;
}

bool fit_plane_at(const double* pts, int64_t n, const double at[3], double r0,
                  Plane& out) {
    auto within = [&](double r, const Plane* slab, double slab_tol) {
        std::vector<double> sub;
        const double r2 = r * r;
        for (int64_t i = 0; i < n; i++) {
            const double d[3] = {pts[i*3]-at[0], pts[i*3+1]-at[1], pts[i*3+2]-at[2]};
            if (dot(d, d) > r2) continue;
            if (slab && std::fabs(slab->distance(pts + i*3)) > slab_tol) continue;
            sub.insert(sub.end(), pts + i*3, pts + i*3 + 3);
        }
        return sub;
    };
    // The first patch has to hold enough of the surface to have a normal.
    std::vector<double> sub;
    double r = r0;
    for (int tries = 0; tries < 6; tries++, r *= 1.6) {
        sub = within(r, nullptr, 0.0);
        if ((int64_t)sub.size() / 3 >= 200) break;
    }
    if ((int64_t)sub.size() / 3 < 8) return false;
    Plane pl;
    if (!fit_plane_lqs(sub.data(), (int64_t)sub.size() / 3, at, 31u, pl)) return false;

    // How thick the surface itself is, from the patch that found it: the slab
    // below is sized by the data's own noise, not by the radius.
    auto thickness = [&](const std::vector<double>& p, const Plane& q) {
        std::vector<double> res;
        res.reserve(p.size() / 3);
        for (size_t i = 0; i + 2 < p.size(); i += 3)
            res.push_back(std::fabs(q.distance(&p[i])));
        if (res.empty()) return 0.0;
        // The 35th percentile of |N(0,1)| is 0.454: the same quantile the fit
        // was scored on, so clutter that outnumbers the surface is not in it.
        const size_t k = res.size() * 35 / 100;
        std::nth_element(res.begin(), res.begin() + (ptrdiff_t)k, res.end());
        return res[k] / 0.454;
    };
    auto tight_refit = [&](const std::vector<double>& p, Plane& q, double tol) {
        const int64_t m = (int64_t)p.size() / 3;
        std::vector<uint8_t> in;
        if (mark_inliers(p.data(), m, q, tol, in) < 8) return false;
        return refit(p.data(), m, in, q);
    };
    double sigma = std::max(thickness(sub, pl), 1e-9 * r);
    tight_refit(sub, pl, 2.5 * sigma);
    tight_refit(sub, pl, 2.5 * sigma);
    {
        std::vector<uint8_t> in;
        mark_inliers(sub.data(), (int64_t)sub.size() / 3, pl, 2.5 * sigma, in);
        std::vector<double> on;
        for (size_t i = 0; i < in.size(); i++)
            if (in[i]) on.insert(on.end(), sub.begin() + i*3, sub.begin() + i*3 + 3);
        sub.swap(on);
        pl.inliers = (int64_t)sub.size() / 3;
    }

    // Wider while it holds. The slab keeps what is off the surface -- the
    // chair standing on the floor -- out of the refit.
    int64_t support = (int64_t)sub.size() / 3;
    for (int grow = 0; grow < 4; grow++) {
        const double r2 = r * 2.0;
        std::vector<double> wide = within(r2, &pl, 4.0 * sigma + 0.01 * r2);
        const int64_t m = (int64_t)wide.size() / 3;
        if (m < support * 2) break;
        Plane next = pl;
        if (!tight_refit(wide, next, 3.0 * sigma)) break;
        if (!tight_refit(wide, next, 3.0 * sigma)) break;
        // A surface that curves away is a different surface.
        if (dot(next.n, pl.n) < 0.985) break;
        std::vector<uint8_t> in;
        next.inliers = mark_inliers(wide.data(), m, next, 3.0 * sigma, in);
        pl = next;
        support = m;
        r = r2;
    }
    out = pl;
    return true;
}

void rotation_between(const double a[3], const double b[3], double R[9]) {
    double axis[3];
    cross(a, b, axis);
    const double c = std::clamp(dot(a, b), -1.0, 1.0);
    if (!normalize(axis)) {
        // Parallel: nothing to do. Opposite: half a turn about anything
        // perpendicular.
        for (int i = 0; i < 9; i++) R[i] = i % 4 == 0 ? 1.0 : 0.0;
        if (c > 0) return;
        double other[3] = {1, 0, 0};
        if (std::fabs(a[0]) > 0.9) { other[0] = 0; other[1] = 1; }
        cross(a, other, axis);
        normalize(axis);
        for (int r = 0; r < 3; r++)
            for (int q = 0; q < 3; q++)
                R[r*3+q] = 2.0 * axis[r] * axis[q] - (r == q ? 1.0 : 0.0);
        return;
    }
    const double pivot[3] = {0, 0, 0};
    const spirula::Sim3 T =
        spirula::Sim3::rotation_about(axis, std::acos(c), pivot);
    for (int i = 0; i < 9; i++) R[i] = T.R[i];
}

int fit_corner(const double* pts, int64_t n, const double at[3], double r0,
               double axes[9], double corner[3]) {
    std::vector<double> sub;
    double r = r0;
    for (int tries = 0; tries < 6; tries++, r *= 1.5) {
        sub.clear();
        const double r2 = r * r;
        for (int64_t i = 0; i < n; i++) {
            const double d[3] = {pts[i*3]-at[0], pts[i*3+1]-at[1], pts[i*3+2]-at[2]};
            if (dot(d, d) <= r2) sub.insert(sub.end(), pts + i*3, pts + i*3 + 3);
        }
        if ((int64_t)sub.size() / 3 >= 300) break;
    }
    const std::vector<Plane> found =
        find_planes(sub.data(), (int64_t)sub.size() / 3, 0.03 * r, 5, 0.06);
    // Greedily: the largest, then the largest roughly square to those kept.
    std::vector<Plane> keep;
    for (const Plane& p : found) {
        bool square = true;
        for (const Plane& k : keep)
            if (std::fabs(dot(p.n, k.n)) > 0.35) square = false;
        if (square) keep.push_back(p);
        if (keep.size() == 3) break;
    }
    const int m = (int)keep.size();
    for (int i = 0; i < 3; i++) corner[i] = at[i];
    for (int i = 0; i < 9; i++) axes[i] = i % 4 == 0 ? 1.0 : 0.0;
    if (m == 0) return 0;

    // Orthonormalize, most trusted first; complete the frame by cross products.
    double e[3][3];
    for (int k = 0; k < 3; k++) e[0][k] = keep[0].n[k];
    if (m >= 2) {
        const double d = dot(keep[1].n, e[0]);
        for (int k = 0; k < 3; k++) e[1][k] = keep[1].n[k] - d * e[0][k];
        normalize(e[1]);
    } else {
        double other[3] = {0, 0, 1};
        if (std::fabs(e[0][2]) > 0.9) { other[2] = 0; other[0] = 1; }
        cross(other, e[0], e[1]);
        normalize(e[1]);
    }
    cross(e[0], e[1], e[2]);
    if (m >= 3 && dot(e[2], keep[2].n) < 0)
        for (double& v : e[2]) v = -v;
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) axes[r*3+k] = e[r][k];

    // Where they meet: the click pushed onto each plane found, in turn. For
    // perpendicular planes one pass lands on all of them.
    for (int pass = 0; pass < 4; pass++)
        for (int i = 0; i < m; i++) {
            const double d = keep[(size_t)i].distance(corner);
            for (int k = 0; k < 3; k++) corner[k] -= d * keep[(size_t)i].n[k];
        }
    return m;
}

double robust_extent(const double* pts, int64_t n) {
    if (n <= 0) return 0.0;
    const int64_t step = std::max<int64_t>(1, n / (1 << 20));
    std::vector<double> tmp;
    double c[3];
    for (int d = 0; d < 3; d++) {
        tmp.clear();
        for (int64_t i = 0; i < n; i += step) tmp.push_back(pts[i*3 + d]);
        std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
        c[d] = tmp[tmp.size() / 2];
    }
    tmp.clear();
    for (int64_t i = 0; i < n; i += step) {
        const double v[3] = {pts[i*3] - c[0], pts[i*3+1] - c[1], pts[i*3+2] - c[2]};
        tmp.push_back(dot(v, v));
    }
    std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
    return 2.0 * std::sqrt(tmp[tmp.size() / 2]);
}

AutoAlignResult auto_align(const double* pts, int64_t n, const double* up,
                           const float* normals, const float* weights,
                           const AutoAlignOptions& opt) {
    AutoAlignResult res;
    if (n < 8) return res;
    double prior[3] = {0, 0, 1};
    if (up) {
        for (int k = 0; k < 3; k++) prior[k] = up[k];
        if (!normalize(prior)) { prior[0] = prior[1] = 0; prior[2] = 1; }
    }

    const std::vector<Plane> planes = find_planes(pts, n, opt.tol, 6, 0.03);
    // The ground: well supported, facing up, and with the scene on top of it
    // rather than under it -- which is what tells a floor from a ceiling and,
    // more often, a floor from the largest wall.
    int best = -1;
    double best_score = 0.0;
    std::vector<Plane> oriented = planes;
    // A prior that came from cameras is evidence; +Z is only what the file
    // happens to say, and a model that arrived on its side says it wrongly.
    const bool trusted = up != nullptr;
    for (size_t i = 0; i < oriented.size(); i++) {
        Plane& p = oriented[i];
        int64_t above = 0, below = 0;
        const int64_t step = std::max<int64_t>(1, n / kScoreCap);
        for (int64_t j = 0; j < n; j += step) {
            const double d = p.distance(pts + j*3);
            if (d > 2.0 * opt.tol) above++;
            else if (d < -2.0 * opt.tol) below++;
        }
        // Up is the side the scene is on -- unless cameras said otherwise.
        const bool flip = trusted ? dot(p.n, prior) < 0 : below > above;
        if (flip) {
            for (double& v : p.n) v = -v;
            p.d = -p.d;
            std::swap(above, below);
        }
        const double facing = std::max(0.0, dot(p.n, prior));
        const double on_top = (double)above / (double)(above + below + 1);
        const double score = (double)p.inliers * (0.3 + 0.7 * on_top) *
                             (trusted ? 0.25 + 0.75 * facing : 0.6 + 0.4 * facing);
        if ((trusted ? facing > 0.5 : on_top > 0.6) && score > best_score) {
            best_score = score;
            best = (int)i;
        }
    }

    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double lift = 0.0;
    const double zaxis[3] = {0, 0, 1};
    if (best >= 0) {
        const Plane& g = oriented[(size_t)best];
        res.plane = g;
        rotation_between(g.n, zaxis, R);
        // After the turn the plane is z = -d.
        lift = g.d;
        res.ground = true;
        res.ground_share = (double)g.inliers / (double)n;
    } else {
        rotation_between(prior, zaxis, R);
    }

    // The walls: whatever stands upright, folded by quarter turns so that the
    // four faces of a room vote for the same heading.
    if (opt.yaw) {
        double sx = 0, sy = 0, total = 0, upright = 0;
        auto vote = [&](const double nrm[3], double w) {
            double v[3];
            for (int r = 0; r < 3; r++)
                v[r] = R[r*3]*nrm[0] + R[r*3+1]*nrm[1] + R[r*3+2]*nrm[2];
            total += w;
            if (std::fabs(v[2]) > 0.25) return;
            const double a = 4.0 * std::atan2(v[1], v[0]);
            sx += w * std::cos(a);
            sy += w * std::sin(a);
            upright += w;
        };
        if (normals && weights) {
            const int64_t step = std::max<int64_t>(1, n / 400000);
            for (int64_t i = 0; i < n; i += step) {
                if (!(weights[i] > 0.0f)) continue;
                const double v[3] = {normals[i*3], normals[i*3+1], normals[i*3+2]};
                vote(v, weights[i]);
            }
        } else {
            for (size_t i = 0; i < oriented.size(); i++)
                if ((int)i != best) vote(oriented[i].n, (double)oriented[i].inliers);
            total = (double)n;
        }
        const double agree = upright > 0 ? std::sqrt(sx*sx + sy*sy) / upright : 0.0;
        if (agree > 0.35 && upright > 0.08 * total) {
            const double heading = std::atan2(sy, sx) / 4.0;   // (-45, 45] deg
            const double c = std::cos(-heading), s = std::sin(-heading);
            const double Z[9] = {c, -s, 0, s, c, 0, 0, 0, 1};
            double RZ[9];
            for (int r = 0; r < 3; r++)
                for (int q = 0; q < 3; q++) {
                    RZ[r*3+q] = 0;
                    for (int k = 0; k < 3; k++) RZ[r*3+q] += Z[r*3+k] * R[k*3+q];
                }
            for (int i = 0; i < 9; i++) R[i] = RZ[i];
            res.walls = true;
        }
    }

    for (int i = 0; i < 9; i++) res.T.R[i] = R[i];
    res.T.t[2] = lift;
    if (opt.centre) {
        // The median of the turned footprint: a floater does not drag it.
        const int64_t step = std::max<int64_t>(1, n / 200000);
        std::vector<double> xs, ys;
        for (int64_t i = 0; i < n; i += step) {
            double q[3];
            res.T.apply(pts + i*3, q);
            xs.push_back(q[0]);
            ys.push_back(q[1]);
        }
        std::nth_element(xs.begin(), xs.begin() + xs.size() / 2, xs.end());
        std::nth_element(ys.begin(), ys.begin() + ys.size() / 2, ys.end());
        res.T.t[0] = -xs[xs.size() / 2];
        res.T.t[1] = -ys[ys.size() / 2];
    }
    return res;
}

}  // namespace align
}  // namespace spirula
