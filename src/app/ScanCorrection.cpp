#include "app/ScanCorrection.h"

#include "sfm/core/Pose.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace app::lidar {
namespace {

using sfm::Mat3;
using sfm::Vec3;

constexpr double kPi = 3.14159265358979323846;
// Photographs of one instant are taken within 51 us of each other.
constexpr int64_t kInstantGapUs = 2000;
// An instant is dropped when it is this far off the fit and more than
// kRejectSpread times as far as the instants are in the median.
constexpr double kRejectFloorM = 0.03;
constexpr double kRejectFloorRad = 0.3 * kPi / 180.0;
constexpr double kRejectSpread = 3.0;
constexpr int kGaussNewton = 6;
// Ties each knot to its neighbours at a tenth of one photograph's weight,
// so a knot that no photograph sees follows them.
constexpr double kKnotTie = 0.1;

double median(std::vector<double> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + (std::ptrdiff_t)(v.size() / 2), v.end());
    return v[v.size() / 2];
}

// Solves H x = g in place for a symmetric positive definite H (n x n, row-major).
bool cholesky_solve(std::vector<double>& H, std::vector<double>& g, size_t n) {
    for (size_t j = 0; j < n; j++) {
        double d = H[j * n + j];
        for (size_t k = 0; k < j; k++) d -= H[j * n + k] * H[j * n + k];
        if (d <= 0) return false;
        d = std::sqrt(d);
        H[j * n + j] = d;
        for (size_t i = j + 1; i < n; i++) {
            double s = H[i * n + j];
            for (size_t k = 0; k < j; k++) s -= H[i * n + k] * H[j * n + k];
            H[i * n + j] = s / d;
        }
    }
    for (size_t i = 0; i < n; i++) {
        double s = g[i];
        for (size_t k = 0; k < i; k++) s -= H[i * n + k] * g[k];
        g[i] = s / H[i * n + i];
    }
    for (size_t i = n; i-- > 0;) {
        double s = g[i];
        for (size_t k = i + 1; k < n; k++) s -= H[k * n + i] * g[k];
        g[i] = s / H[i * n + i];
    }
    return true;
}

// The world's turn as knots `step` apart, blended linearly between them.
struct Knots {
    int64_t t0 = 0, step = 1;
    std::vector<Vec3> l;
    void at(int64_t t, size_t& i, double& s) const {
        const double u = std::clamp((double)(t - t0) / (double)step, 0.0, (double)(l.size() - 1));
        i = std::min((size_t)u, l.size() - 2);
        s = u - (double)i;
    }
    Vec3 turn(int64_t t) const {
        size_t i;
        double s;
        at(t, i, s);
        return l[i] * (1 - s) + l[i + 1] * s;
    }
};

}  // namespace

ScanCorrection scan_correction(const std::vector<PosePair>& pairs, double window_s) {
    ScanCorrection out;
    if (pairs.empty()) return out;

    // ---- the instants, each from its lead photograph ----
    std::vector<size_t> order(pairs.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return pairs[a].time_us != pairs[b].time_us ? pairs[a].time_us < pairs[b].time_us
                                                    : pairs[a].camera < pairs[b].camera;
    });
    std::vector<size_t> lead;
    for (size_t k = 0; k < order.size(); k++) {
        const PosePair& p = pairs[order[k]];
        if (!lead.empty() && p.time_us - pairs[lead.back()].time_us <= kInstantGapUs) {
            if (p.camera < pairs[lead.back()].camera) lead.back() = order[k];
            continue;
        }
        lead.push_back(order[k]);
    }
    const size_t n = lead.size();
    for (size_t i : lead) out.instants.push_back(pairs[i].time_us);

    // ---- F = L D M: the world's slow turn L and each camera's fixed turn M, together ----
    // A mount error turns with the camera, drift does not, so the walk's turns tell them
    // apart; averaging D^-1 F instead found 0.58 of a 0.7 deg mount, the rest taken for drift.
    std::map<std::string, size_t> cam_index;
    for (const PosePair& p : pairs) cam_index.emplace(p.camera, cam_index.size());
    Knots K;
    K.t0 = out.instants.front();
    K.step = std::max<int64_t>(1, (int64_t)std::llround(window_s * 1e6));
    K.l.assign((size_t)((out.instants.back() - K.t0) / K.step) + 2, Vec3{});
    std::vector<Vec3> m(cam_index.size());
    const size_t nl = 3 * K.l.size(), nx = nl + 3 * m.size();
    std::vector<double> weight(pairs.size(), 1.0), residual(pairs.size(), 0.0);
    for (int it = 0; it < kGaussNewton; it++) {
        std::vector<double> H(nx * nx, 0.0), g(nx, 0.0);
        for (size_t k = 0; k < pairs.size(); k++) {
            const PosePair& p = pairs[k];
            const size_t c = cam_index.at(p.camera);
            const Mat3 A = sfm::mul(sfm::mul(sfm::so3Exp(K.turn(p.time_us)), p.R_device), sfm::so3Exp(m[c]));
            const Vec3 r = sfm::so3Log(sfm::mul(sfm::transpose(A), p.R_refined));
            residual[k] = r.norm();
            weight[k] = residual[k] <= kRejectFloorRad ? 1.0 : kRejectFloorRad / residual[k];
            size_t i;
            double s;
            K.at(p.time_us, i, s);
            // r(d) = r - A^T (dl_i (1 - s) + dl_i+1 s) - dm: columns of the knots, then the mount.
            const size_t col[3] = {3 * i, 3 * (i + 1), nl + 3 * c};
            double J[3][3][3];
            for (int a = 0; a < 3; a++)
                for (int b = 0; b < 3; b++) {
                    J[0][a][b] = A[b * 3 + a] * (1 - s);
                    J[1][a][b] = A[b * 3 + a] * s;
                    J[2][a][b] = a == b ? 1.0 : 0.0;
                }
            const double rv[3] = {r.x, r.y, r.z};
            for (int u = 0; u < 3; u++)
                for (int a = 0; a < 3; a++) {
                    double ju_r = 0;
                    for (int q = 0; q < 3; q++) ju_r += J[u][q][a] * rv[q];
                    g[col[u] + a] += weight[k] * ju_r;
                    for (int v = 0; v < 3; v++)
                        for (int b = 0; b < 3; b++) {
                            double jj = 0;
                            for (int q = 0; q < 3; q++) jj += J[u][q][a] * J[v][q][b];
                            H[(col[u] + a) * nx + col[v] + b] += weight[k] * jj;
                        }
                }
        }
        for (size_t i = 0; i + 1 < K.l.size(); i++) {
            const Vec3 d = K.l[i] - K.l[i + 1];
            const double dv[3] = {d.x, d.y, d.z};
            for (int a = 0; a < 3; a++) {
                const size_t u = 3 * i + a, v = 3 * (i + 1) + a;
                H[u * nx + u] += kKnotTie;
                H[v * nx + v] += kKnotTie;
                H[u * nx + v] -= kKnotTie;
                H[v * nx + u] -= kKnotTie;
                g[u] -= kKnotTie * dv[a];
                g[v] += kKnotTie * dv[a];
            }
        }
        for (size_t j = 0; j < nx; j++) H[j * nx + j] += 1e-9;
        if (!cholesky_solve(H, g, nx)) return out;
        for (size_t i = 0; i < K.l.size(); i++)
            K.l[i] = sfm::so3Log(sfm::mul(sfm::so3Exp({g[3 * i], g[3 * i + 1], g[3 * i + 2]}), sfm::so3Exp(K.l[i])));
        for (size_t c = 0; c < m.size(); c++)
            m[c] = sfm::so3Log(sfm::mul(sfm::so3Exp(m[c]), sfm::so3Exp({g[nl + 3 * c], g[nl + 3 * c + 1], g[nl + 3 * c + 2]})));
    }
    for (const Vec3& v : m) out.mount_deg = std::max(out.mount_deg, v.norm() * 180.0 / kPi);

    // ---- each instant's shift under that turn, smoothed, instants off the fit dropped ----
    std::vector<Vec3> shift(n);
    std::vector<double> lead_residual(n);
    for (size_t i = 0; i < n; i++) {
        const PosePair& p = pairs[lead[i]];
        const Vec3 l = K.turn(p.time_us);
        shift[i] = p.c_refined - sfm::mul(sfm::so3Exp(l), p.c_device);
        out.raw.push_back({l.x, l.y, l.z, shift[i].x, shift[i].y, shift[i].z});
        lead_residual[i] = residual[lead[i]];
    }
    const int64_t half = K.step / 2;
    auto window = [&](size_t i, const std::vector<char>* only) {
        std::vector<size_t> in;
        for (size_t j = i; j-- > 0 && out.instants[i] - out.instants[j] <= half;)
            if (!only || (*only)[j]) in.push_back(j);
        for (size_t j = i; j < n && out.instants[j] - out.instants[i] <= half; j++)
            if (!only || (*only)[j]) in.push_back(j);
        return in;
    };
    auto median_shift = [&](const std::vector<size_t>& in) {
        std::vector<double> x, y, z;
        for (size_t j : in) {
            x.push_back(shift[j].x);
            y.push_back(shift[j].y);
            z.push_back(shift[j].z);
        }
        return Vec3{median(x), median(y), median(z)};
    };
    const double max_turn = std::max(kRejectFloorRad, kRejectSpread * median(lead_residual));
    out.kept.assign(n, 1);
    for (size_t i = 0; i < n; i++) {
        const std::vector<size_t> in = window(i, nullptr);
        const Vec3 c = median_shift(in);
        std::vector<double> d;
        for (size_t j : in) d.push_back((shift[j] - c).norm());
        const double max_shift = std::max(kRejectFloorM, kRejectSpread * median(d));
        if ((shift[i] - c).norm() > max_shift || lead_residual[i] > max_turn) {
            out.kept[i] = 0;
            out.rejected++;
        }
    }
    std::vector<double> moved, turned;
    for (size_t i = 0; i < n; i++) {
        if (!out.kept[i]) continue;
        const Vec3 l = K.turn(out.instants[i]);
        const Vec3 t = median_shift(window(i, &out.kept));
        out.keys.time_us.push_back(out.instants[i]);
        out.keys.move.push_back({l.x, l.y, l.z, t.x, t.y, t.z});
        const Vec3 at = pairs[lead[i]].c_device;
        moved.push_back((sfm::mul(sfm::so3Exp(l), at) + t - at).norm());
        turned.push_back(l.norm() * 180.0 / kPi);
    }
    std::map<std::string, std::pair<Vec3, int>> mean_shift;
    for (const PosePair& p : pairs) {
        auto& [sum, count] = mean_shift[p.camera];
        sum = sum + sfm::mul(sfm::transpose(p.R_device), p.c_refined - p.c_device);
        count++;
    }
    for (const auto& [cam, sc] : mean_shift)
        out.mount_shift_m = std::max(out.mount_shift_m, (sc.first * (1.0 / sc.second)).norm());
    out.moved_m = median(moved);
    out.turned_deg = median(turned);
    return out;
}

}  // namespace app::lidar
