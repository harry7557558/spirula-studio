#include "app/CaptureTrajectory.h"

#include "core/Env.h"
#include "data/Knn.h"
#include "i18n/Message.h"
#include "i18n/catalog/Lidar.h"
#include "sfm/ba/CpuParallel.h"
#include "sfm/core/Pose.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::lidar;
using spirula::i18n::format;

namespace app::lidar {
namespace {

using sfm::Mat3;
using sfm::Vec3;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

// Measured on a 975 s PortalCam walk against LCC Studio's own offline
// trajectory: 5.4 cm after one rigid fit, the device's 47 cm.
constexpr double kNearM = 1.0;   // the operator walks within it
constexpr double kFarM = 60.0;
constexpr double kSweepVoxelM = 0.1;
constexpr int kNodeSweeps = 10;
constexpr int kNeighbourHalf = 5, kLoopHalf = 15;   // sweeps either side of a node
constexpr int kNeighbourLags = 3;
constexpr size_t kLoopBlock = 16;   // target nodes whose loop submaps are held at once
constexpr double kCropM = 30.0;
constexpr int64_t kLoopGapUs = 20000000;
constexpr int64_t kPassGapUs = 5000000;
constexpr double kLoopRadiusM[2] = {6.0, 3.0};
struct Stage {
    double voxel, reach;   // metres
};
constexpr Stage kNeighbourStage = {0.1, 0.5};
const std::vector<Stage> kLoopStages[2] = {{{0.5, 2.5}, {0.25, 0.8}, {0.15, 0.3}},
                                           {{0.25, 0.8}, {0.15, 0.3}}};
constexpr double kNeighbourInliers = 0.3, kLoopInliers = 0.5;
constexpr double kLoopJumpM = 2.5, kLoopJumpRad = 10 * kDeg;
// Sigmas of an edge, metres and radians: the device's motion, a neighbour's
// registration, a loop's.
constexpr double kSigma[3][2] = {{0.03, 0.3 * kDeg}, {0.01, 0.05 * kDeg}, {0.02, 0.1 * kDeg}};
constexpr double kGravitySigma = 1.0 * kDeg;
// Cauchy widths, wide to narrow, in whitened units; a loop above kPrune after them goes.
constexpr double kCauchy[] = {1e9, 30, 10, 5, 3};
constexpr double kPrune = 8.0;
constexpr int kGraphIterations = 15;
constexpr int kSolverIterations = 5000;
constexpr double kSolverTolerance = 1e-8;
constexpr int kRounds = 2;
constexpr int kSampleEvery = 10;
constexpr double kSampleNearM = 0.5, kSampleVoxelM = 0.05;

enum Kind { Odometry = 0, Neighbour = 1, Loop = 2 };

struct Pose {
    Mat3 R = sfm::mat3Identity();
    Vec3 p;
};

double seconds_since(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

Pose operator*(const Pose& a, const Pose& b) { return {sfm::mul(a.R, b.R), sfm::mul(a.R, b.p) + a.p}; }

Pose inverse(const Pose& a) {
    const Mat3 Rt = sfm::transpose(a.R);
    return {Rt, sfm::mul(Rt, a.p) * -1.0};
}

Vec3 apply(const Pose& T, const float* q) { return sfm::mul(T.R, Vec3{q[0], q[1], q[2]}) + T.p; }

uint64_t voxel_key(const Vec3& p, double size) {
    const int64_t x = (int64_t)std::floor(p.x / size) + (1 << 20), y = (int64_t)std::floor(p.y / size) + (1 << 20),
                  z = (int64_t)std::floor(p.z / size) + (1 << 20);
    return ((uint64_t)x << 42) | ((uint64_t)y << 21) | (uint64_t)z;
}

std::vector<float> voxel_means(const std::vector<float>& xyz, double size) {
    std::unordered_map<uint64_t, uint32_t> at;
    std::vector<double> sum;
    std::vector<uint32_t> n;
    at.reserve(xyz.size() / 3);
    for (size_t i = 0; i < xyz.size(); i += 3) {
        const auto [it, fresh] = at.try_emplace(voxel_key({xyz[i], xyz[i + 1], xyz[i + 2]}, size), (uint32_t)n.size());
        if (fresh) {
            sum.insert(sum.end(), {0.0, 0.0, 0.0});
            n.push_back(0);
        }
        for (int k = 0; k < 3; k++) sum[(size_t)it->second * 3 + k] += xyz[i + k];
        n[it->second]++;
    }
    std::vector<float> out(sum.size());
    for (size_t v = 0; v < n.size(); v++)
        for (int k = 0; k < 3; k++) out[v * 3 + k] = (float)(sum[v * 3 + k] / n[v]);
    return out;
}

// ================
// GICP
// ================

// Points with a covariance each, flattened to a plane: (1e-3, 1, 1) along their
// own axes, as small_gicp regularises them.
struct Cloud {
    std::vector<float> xyz;
    std::vector<std::array<float, 6>> cov;   // xx xy xz yy yz zz
    std::unique_ptr<knn::KdTree3> tree;
    size_t size() const { return xyz.size() / 3; }
};

constexpr int kMinCloud = 20;
constexpr size_t kSourcePoints = 3000;

// Each point's neighbours are the points of the 27 voxels around its own: on a
// cloud of voxel means that is the 20 nearest of small_gicp, without the search.
bool make_cloud(const std::vector<float>& pts, double voxel, Cloud& c) {
    c.xyz = voxel_means(pts, voxel);
    const size_t n = c.size();
    if (n < (size_t)kMinCloud) return false;
    c.tree = std::make_unique<knn::KdTree3>(c.xyz.data(), (int64_t)n);
    std::unordered_map<uint64_t, uint32_t> at;
    at.reserve(n);
    for (size_t i = 0; i < n; i++) at[voxel_key({c.xyz[i * 3], c.xyz[i * 3 + 1], c.xyz[i * 3 + 2]}, voxel)] = (uint32_t)i;
    c.cov.resize(n);
    for (size_t i = 0; i < n; i++) {
        const uint64_t key = voxel_key({c.xyz[i * 3], c.xyz[i * 3 + 1], c.xyz[i * 3 + 2]}, voxel);
        double m[3] = {0, 0, 0}, A[9] = {};
        uint32_t near_idx[27];
        int k = 0;
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dz = -1; dz <= 1; dz++) {
                    const uint64_t q = key + ((uint64_t)(int64_t)dx << 42) + ((uint64_t)(int64_t)dy << 21) + (uint64_t)(int64_t)dz;
                    const auto it = at.find(q);
                    if (it != at.end()) near_idx[k++] = it->second;
                }
        for (int j = 0; j < k; j++)
            for (int a = 0; a < 3; a++) m[a] += c.xyz[(size_t)near_idx[j] * 3 + a] / k;
        for (int j = 0; j < k; j++) {
            double d[3];
            for (int a = 0; a < 3; a++) d[a] = c.xyz[(size_t)near_idx[j] * 3 + a] - m[a];
            for (int a = 0; a < 3; a++)
                for (int b = 0; b < 3; b++) A[a * 3 + b] += d[a] * d[b];
        }
        if (k < 3) {
            c.cov[i] = {1, 0, 0, 1, 0, 1};
            continue;
        }
        double w[3], V[9];
        sfm::jacobiEigenSymmetric(A, 3, w, V);
        int order[3] = {0, 1, 2};
        std::sort(order, order + 3, [&](int a, int b) { return w[a] < w[b]; });
        const double s[3] = {1e-3, 1.0, 1.0};
        double C[9] = {};
        for (int e = 0; e < 3; e++)
            for (int a = 0; a < 3; a++)
                for (int b = 0; b < 3; b++) C[a * 3 + b] += s[e] * V[a * 3 + order[e]] * V[b * 3 + order[e]];
        c.cov[i] = {(float)C[0], (float)C[1], (float)C[2], (float)C[4], (float)C[5], (float)C[8]};
    }
    return true;
}

Mat3 full(const std::array<float, 6>& c) { return {c[0], c[1], c[2], c[1], c[3], c[4], c[2], c[4], c[5]}; }

bool solve6(double H[36], double g[6]) {
    for (int j = 0; j < 6; j++) {
        double d = H[j * 6 + j];
        for (int k = 0; k < j; k++) d -= H[j * 6 + k] * H[j * 6 + k];
        if (!(d > 0)) return false;
        d = std::sqrt(d);
        H[j * 6 + j] = d;
        for (int i = j + 1; i < 6; i++) {
            double s = H[i * 6 + j];
            for (int k = 0; k < j; k++) s -= H[i * 6 + k] * H[j * 6 + k];
            H[i * 6 + j] = s / d;
        }
    }
    for (int i = 0; i < 6; i++) {
        for (int k = 0; k < i; k++) g[i] -= H[i * 6 + k] * g[k];
        g[i] /= H[i * 6 + i];
    }
    for (int i = 6; i-- > 0;) {
        for (int k = i + 1; k < 6; k++) g[i] -= H[k * 6 + i] * g[k];
        g[i] /= H[i * 6 + i];
    }
    return true;
}

struct Linear {
    double H[36] = {}, b[6] = {}, error = 0;
    int64_t inliers = 0;
};

// Target-from-source at `T`: every `stride`-th source point against its nearest
// target point within `reach`, weighted by the inverse of both covariances.
Linear linearize(const Cloud& tgt, const Cloud& src, size_t stride, const Pose& T, double reach) {
    Linear L;
    const double reach2 = reach * reach;
    for (size_t i = 0; i < src.size(); i += stride) {
        const float* p = &src.xyz[i * 3];
        const Vec3 x = apply(T, p);
        const float xf[3] = {(float)x.x, (float)x.y, (float)x.z};
        float d2;
        int32_t j;
        if (tgt.tree->query(xf, -1, 1, &d2, &j) < 1 || d2 > reach2) continue;
        L.inliers++;
        const float* q = &tgt.xyz[(size_t)j * 3];
        const Vec3 e = Vec3{q[0], q[1], q[2]} - x;
        const Mat3 C = sfm::mul(sfm::mul(T.R, full(src.cov[i])), sfm::transpose(T.R));
        Mat3 S = full(tgt.cov[(size_t)j]);
        for (int k = 0; k < 9; k++) S[k] += C[k];
        bool ok = true;
        const Mat3 M = sfm::inverse3(S, &ok);
        if (!ok) continue;
        const Vec3 Me = sfm::mul(M, e);
        L.error += e.dot(Me);
        // e = q - (R p + t) under T <- T Exp(w, v): de/dw = R [p]x, de/dv = -R.
        const Mat3 Jw = sfm::mul(T.R, sfm::crossMatrix(Vec3{p[0], p[1], p[2]}));
        double J[3][6];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                J[r][c] = Jw[r * 3 + c];
                J[r][3 + c] = -T.R[r * 3 + c];
            }
        double MJ[3][6];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 6; c++) MJ[r][c] = M[r * 3] * J[0][c] + M[r * 3 + 1] * J[1][c] + M[r * 3 + 2] * J[2][c];
        const double ev[3] = {Me.x, Me.y, Me.z};
        for (int a = 0; a < 6; a++) {
            for (int c = 0; c <= a; c++) L.H[a * 6 + c] += J[0][a] * MJ[0][c] + J[1][a] * MJ[1][c] + J[2][a] * MJ[2][c];
            L.b[a] += J[0][a] * ev[0] + J[1][a] * ev[1] + J[2][a] * ev[2];
        }
    }
    for (int a = 0; a < 6; a++)
        for (int c = a + 1; c < 6; c++) L.H[a * 6 + c] = L.H[c * 6 + a];
    return L;
}

struct Fit {
    Pose T;
    double inlier_fraction = 0;
};

// Levenberg-Marquardt as small_gicp's (lambda from 1e-3, ten tries a step, done
// under 0.1 deg and 1 mm), a tried step's linearization the next one's, the
// source thinned to kSourcePoints: all of a 30k submap gave the same map.
Fit gicp(const Cloud& tgt, const Cloud& src, Pose T, double reach) {
    const size_t stride = std::max<size_t>(1, (src.size() + kSourcePoints - 1) / kSourcePoints);
    double lambda = 1e-3;
    Linear L = linearize(tgt, src, stride, T, reach);
    for (int it = 0; it < 20 && L.inliers >= 6; it++) {
        double step[6] = {};
        bool moved = false;
        for (int tries = 0; tries < 10; tries++) {
            double H[36], g[6];
            std::copy(L.H, L.H + 36, H);
            for (int a = 0; a < 6; a++) {
                H[a * 6 + a] += lambda;
                g[a] = -L.b[a];
            }
            if (!solve6(H, g)) {
                lambda *= 10;
                continue;
            }
            Pose Tn = T;
            Tn.R = sfm::mul(T.R, sfm::so3Exp({g[0], g[1], g[2]}));
            Tn.p = sfm::mul(T.R, Vec3{g[3], g[4], g[5]}) + T.p;
            Linear Ln = linearize(tgt, src, stride, Tn, reach);
            if (Ln.error <= L.error) {
                T = Tn;
                L = Ln;
                std::copy(g, g + 6, step);
                lambda /= 10;
                moved = true;
                break;
            }
            lambda *= 10;
        }
        if (!moved) break;
        if (std::sqrt(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]) < 0.1 * kDeg &&
            std::sqrt(step[3] * step[3] + step[4] * step[4] + step[5] * step[5]) < 1e-3)
            break;
    }
    return {T, (double)L.inliers / (double)((src.size() + stride - 1) / stride)};
}

// ================
// Sweeps, nodes and submaps
// ================

struct Sweeps {
    std::vector<int64_t> time_us;
    std::vector<size_t> begin{0};   // into xyz, in points
    std::vector<float> xyz;         // IMU frame at the sweep's time, 10 cm voxel means
    std::vector<size_t> sample;     // every kSampleEvery-th sweep, its raw points
    std::vector<std::vector<float>> sample_xyz;
    size_t size() const { return time_us.size(); }
};

// Sweeps [centre - half, centre + half) in the frame of `node`, each by its
// pose in `world`, cropped to kCropM.
std::vector<float> gather(const Sweeps& s, const std::vector<Pose>& world, size_t centre, int half,
                          const Pose& node) {
    const Pose inv = inverse(node);
    std::vector<float> out;
    const size_t lo = centre >= (size_t)half ? centre - half : 0, hi = std::min(s.size(), centre + half);
    for (size_t k = lo; k < hi; k++) {
        const Pose T = inv * world[k];
        for (size_t i = s.begin[k]; i < s.begin[k + 1]; i++) {
            const Vec3 q = apply(T, &s.xyz[i * 3]);
            if (q.dot(q) > kCropM * kCropM) continue;
            out.insert(out.end(), {(float)q.x, (float)q.y, (float)q.z});
        }
    }
    return out;
}

int64_t occupied_voxels(const Sweeps& s, const std::vector<Pose>& world) {
    std::unordered_set<uint64_t> v;
    for (size_t n = 0; n < s.sample.size(); n++) {
        const Pose& T = world[s.sample[n]];
        const std::vector<float>& q = s.sample_xyz[n];
        for (size_t i = 0; i < q.size(); i += 3) v.insert(voxel_key(apply(T, &q[i]), kSampleVoxelM));
    }
    return (int64_t)v.size();
}

struct Edge {
    size_t i, j;
    Pose Z;     // T_j = T_i Z
    Kind kind;
};

// ================
// Pose graph
// ================

// Per edge: r_rot = Log(Zr^T Ri^T Rj), r_pos = Ri^T (pj - pi) - Zt.
void edge_residual(const std::vector<Pose>& N, const Edge& e, double r[6], Vec3& d) {
    const Pose& a = N[e.i];
    const Pose& b = N[e.j];
    const Vec3 rr = sfm::so3Log(sfm::mul(sfm::transpose(e.Z.R), sfm::mul(sfm::transpose(a.R), b.R)));
    d = sfm::mul(sfm::transpose(a.R), b.p - a.p);
    const Vec3 rp = d - e.Z.p;
    r[0] = rr.x, r[1] = rr.y, r[2] = rr.z, r[3] = rp.x, r[4] = rp.y, r[5] = rp.z;
}

double whitened(const double r[6], Kind k) {
    double s = 0;
    for (int a = 0; a < 3; a++) s += r[a] * r[a] / (kSigma[k][1] * kSigma[k][1]);
    for (int a = 3; a < 6; a++) s += r[a] * r[a] / (kSigma[k][0] * kSigma[k][0]);
    return std::sqrt(s);
}

// The normal equations by node: a 6x6 block on the diagonal, one for each pair
// an edge joins. Conjugate gradients on the inverted diagonal blocks: a dense
// factorization of 977 nodes took 45 s a step.
struct BlockSystem {
    std::vector<std::array<double, 36>> diag;
    std::map<std::pair<size_t, size_t>, std::array<double, 36>> off;   // (i, j), i < j: rows of i
    std::vector<double> g;

    explicit BlockSystem(size_t n) : diag(n, std::array<double, 36>{}), g(6 * n, 0.0) {}

    // += A^T B into block (i, j); A and B 6 rows of 6.
    void add(size_t i, size_t j, const double* A, const double* B) {
        if (i > j) std::swap(i, j), std::swap(A, B);
        double* M = i == j ? diag[i].data() : off[{i, j}].data();
        for (int r = 0; r < 6; r++)
            for (int c = 0; c < 6; c++) {
                double v = 0;
                for (int k = 0; k < 6; k++) v += A[k * 6 + r] * B[k * 6 + c];
                M[r * 6 + c] += v;
            }
    }

    void multiply(const std::vector<double>& x, std::vector<double>& y) const {
        std::fill(y.begin(), y.end(), 0.0);
        for (size_t i = 0; i < diag.size(); i++)
            for (int r = 0; r < 6; r++)
                for (int c = 0; c < 6; c++) y[6 * i + r] += diag[i][r * 6 + c] * x[6 * i + c];
        for (const auto& [ij, M] : off)
            for (int r = 0; r < 6; r++)
                for (int c = 0; c < 6; c++) {
                    y[6 * ij.first + r] += M[r * 6 + c] * x[6 * ij.second + c];
                    y[6 * ij.second + c] += M[r * 6 + c] * x[6 * ij.first + r];
                }
    }

    // `steps` and `residual` (|r| over |g|) say how far the solve got.
    std::vector<double> solve(int& steps, double& residual) const {
        const size_t n = diag.size(), m = 6 * n;
        std::vector<std::array<double, 36>> inv(n);
        for (size_t i = 0; i < n; i++)
            for (int c = 0; c < 6; c++) {
                double H[36], e[6] = {};
                std::copy(diag[i].begin(), diag[i].end(), H);
                e[c] = 1;
                solve6(H, e);
                for (int r = 0; r < 6; r++) inv[i][r * 6 + c] = e[r];
            }
        auto precondition = [&](const std::vector<double>& r, std::vector<double>& z) {
            for (size_t i = 0; i < n; i++)
                for (int a = 0; a < 6; a++) {
                    double v = 0;
                    for (int b = 0; b < 6; b++) v += inv[i][a * 6 + b] * r[6 * i + b];
                    z[6 * i + a] = v;
                }
        };
        std::vector<double> x(m, 0.0), r = g, z(m), p(m), q(m);
        precondition(r, z);
        p = z;
        double rz = 0, b2 = 0;
        for (size_t k = 0; k < m; k++) rz += r[k] * z[k], b2 += g[k] * g[k];
        steps = 0;
        residual = 0;
        for (int it = 0; it < kSolverIterations && b2 > 0; it++) {
            steps = it + 1;
            multiply(p, q);
            double pq = 0;
            for (size_t k = 0; k < m; k++) pq += p[k] * q[k];
            if (!(pq > 0)) break;
            const double alpha = rz / pq;
            double r2 = 0;
            for (size_t k = 0; k < m; k++) {
                x[k] += alpha * p[k];
                r[k] -= alpha * q[k];
                r2 += r[k] * r[k];
            }
            residual = std::sqrt(r2 / b2);
            if (r2 < kSolverTolerance * kSolverTolerance * b2) break;
            precondition(r, z);
            double rz_next = 0;
            for (size_t k = 0; k < m; k++) rz_next += r[k] * z[k];
            const double beta = rz_next / rz;
            rz = rz_next;
            for (size_t k = 0; k < m; k++) p[k] = z[k] + beta * p[k];
        }
        return x;
    }
};

// Gauss-Newton with one Cauchy weight per edge, node 0 held, each node's up
// direction held near the device's (roll and pitch). Unknowns: a right turn of
// each node's attitude and its position in the world.
void solve_graph(std::vector<Pose>& N, const std::vector<Edge>& E, const std::vector<Vec3>& up_device,
                 TrajectoryReport& report, const std::atomic<bool>* cancel) {
    const size_t n = N.size();
    if (n < 2) return;
    const Vec3 z{0, 0, 1};
    for (const double width : kCauchy) {
        for (int it = 0; it < kGraphIterations; it++) {
            if (cancel && cancel->load()) return;
            BlockSystem S(n - 1);
            for (const Edge& e : E) {
                double r[6];
                Vec3 d;
                edge_residual(N, e, r, d);
                const double w = 1.0 / (1.0 + std::pow(whitened(r, e.kind) / width, 2));
                const double sr = std::sqrt(w) / kSigma[e.kind][1], st = std::sqrt(w) / kSigma[e.kind][0];
                const Mat3& Ri = N[e.i].R;
                const Mat3 Ari = sfm::mul(sfm::transpose(N[e.j].R), Ri);
                const Mat3 D = sfm::crossMatrix(d);
                double Ji[36] = {}, Jj[36] = {};
                for (int a = 0; a < 3; a++)
                    for (int c = 0; c < 3; c++) {
                        Ji[a * 6 + c] = -Ari[a * 3 + c] * sr;
                        Ji[(3 + a) * 6 + c] = D[a * 3 + c] * st;
                        Ji[(3 + a) * 6 + 3 + c] = -Ri[c * 3 + a] * st;
                        Jj[a * 6 + c] = (a == c) * sr;
                        Jj[(3 + a) * 6 + 3 + c] = Ri[c * 3 + a] * st;
                    }
                double rw[6];
                for (int a = 0; a < 6; a++) rw[a] = r[a] * (a < 3 ? sr : st);
                auto grad = [&](size_t node, const double* J) {
                    for (int c = 0; c < 6; c++) {
                        double v = 0;
                        for (int k = 0; k < 6; k++) v += J[k * 6 + c] * rw[k];
                        S.g[6 * (node - 1) + c] -= v;
                    }
                };
                if (e.i > 0) {
                    S.add(e.i - 1, e.i - 1, Ji, Ji);
                    grad(e.i, Ji);
                }
                if (e.j > 0) {
                    S.add(e.j - 1, e.j - 1, Jj, Jj);
                    grad(e.j, Jj);
                }
                if (e.i > 0 && e.j > 0) S.add(e.i - 1, e.j - 1, Ji, Jj);
            }
            for (size_t m = 1; m < n; m++) {
                const Vec3 up = sfm::mul(sfm::transpose(N[m].R), z);
                const Vec3 r = (up - up_device[m]) * (1.0 / kGravitySigma);
                const Mat3 C = sfm::crossMatrix(up);
                double J[36] = {}, rw[6] = {r.x, r.y, r.z, 0, 0, 0};
                for (int a = 0; a < 3; a++)
                    for (int c = 0; c < 3; c++) J[a * 6 + c] = C[a * 3 + c] / kGravitySigma;
                S.add(m - 1, m - 1, J, J);
                for (int c = 0; c < 3; c++) {
                    double v = 0;
                    for (int k = 0; k < 3; k++) v += J[k * 6 + c] * rw[k];
                    S.g[6 * (m - 1) + c] -= v;
                }
            }
            int steps = 0;
            double residual = 0;
            const std::vector<double> x = S.solve(steps, residual);
            report.solver_steps = std::max(report.solver_steps, (int64_t)steps);
            report.solver_residual = std::max(report.solver_residual, residual);
            double largest = 0;
            for (size_t m = 1; m < n; m++) {
                const double* step = &x[6 * (m - 1)];
                N[m].R = sfm::mul(N[m].R, sfm::so3Exp({step[0], step[1], step[2]}));
                N[m].p = N[m].p + Vec3{step[3], step[4], step[5]};
                for (int a = 0; a < 6; a++) largest = std::max(largest, std::fabs(step[a]));
            }
            if (largest < 1e-7) break;
        }
    }
}

// One candidate for each later pass near node i: the closest node of each run
// of nodes without a kPassGapUs gap, over kLoopGapUs after i.
std::vector<std::pair<size_t, size_t>> loop_candidates(const std::vector<int64_t>& t, const std::vector<Pose>& N,
                                                       double radius) {
    std::vector<std::pair<size_t, size_t>> out;
    for (size_t i = 0; i < N.size(); i++) {
        size_t best = SIZE_MAX;
        double best_d = 0;
        int64_t last = 0;
        for (size_t j = i + 1; j <= N.size(); j++) {
            const bool within = j < N.size() && t[j] - t[i] > kLoopGapUs && (N[j].p - N[i].p).norm() < radius;
            if (best != SIZE_MAX && (j == N.size() || (within && t[j] - last > kPassGapUs))) {
                out.push_back({i, best});
                best = SIZE_MAX;
            }
            if (!within) continue;
            const double d = (N[j].p - N[i].p).norm();
            if (best == SIZE_MAX || d < best_d) best = j, best_d = d;
            last = t[j];
        }
    }
    return out;
}

Pose pose_at(const spirula::xbin::Correction& L, int64_t t, const Pose& device) {
    const std::array<double, 6> m = L.at(t);
    return Pose{sfm::so3Exp({m[0], m[1], m[2]}), Vec3{m[3], m[4], m[5]}} * device;
}

}  // namespace

std::string trajectory_path(const std::string& lidar_dir, const std::string& capture) {
    return (fs::path(lidar_dir) / (fs::path(capture).stem().string() + ".trajectory.csv")).string();
}

spirula::xbin::Correction read_trajectory(const std::string& path) {
    spirula::xbin::Correction c;
    std::ifstream f(path);
    if (!f) return c;
    for (std::string line; std::getline(f, line);) {
        if (line.empty() || line[0] == '#') continue;
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ls(line);
        int64_t t = 0;
        std::array<double, 6> m;
        bool whole = (bool)(ls >> t);
        for (double& v : m) whole = whole && (bool)(ls >> v);
        if (!whole || (!c.time_us.empty() && t <= c.time_us.back()))
            throw std::runtime_error(path + ": not a trajectory, at '" + line + "'");
        c.time_us.push_back(t);
        c.move.push_back(m);
    }
    return c;
}

void write_trajectory(const std::string& path, const spirula::xbin::Correction& c) {
    std::ofstream f(path);
    f << "# time_us,r0,r1,r2,t0,t1,t2: x' = Exp(r) x + t in the device's world\n";
    char buf[64];
    for (size_t i = 0; i < c.time_us.size(); i++) {
        f << c.time_us[i];
        for (double v : c.move[i]) {
            std::snprintf(buf, sizeof buf, ",%.17g", v);
            f << buf;
        }
        f << '\n';
    }
    if (!f) throw std::runtime_error(path + ": cannot write");
}

spirula::xbin::Correction build_trajectory(spirula::xbin::Reader& reader, TrajectoryReport& report,
                                           const std::function<void(double)>& progress,
                                           const std::atomic<bool>* cancel) {
    report = {};
    auto cancelled = [&] { return cancel && cancel->load(); };
    std::mutex said;
    auto say = [&](double f) {
        if (!progress) return;
        std::lock_guard<std::mutex> lock(said);
        progress(std::clamp(f, 0.0, 1.0));
    };
    reader.set_correction({});

    // ---- sweeps ----
    const auto t_read = std::chrono::steady_clock::now();
    Sweeps s;
    const double total = (double)std::max<int64_t>(reader.sweeps(), 1);
    std::vector<float> near_out;
    if (!reader.read_sweeps(kSampleNearM, kFarM, [&](const spirula::xbin::Sweep& w) {
            if (cancelled()) return false;
            near_out.clear();
            for (size_t i = 0; i < w.xyz.size(); i += 3) {
                const float x = w.xyz[i], y = w.xyz[i + 1], z = w.xyz[i + 2];
                if (x * x + y * y + z * z >= kNearM * kNearM) near_out.insert(near_out.end(), {x, y, z});
            }
            const std::vector<float> v = voxel_means(near_out, kSweepVoxelM);
            if (s.size() % kSampleEvery == 0) {
                s.sample.push_back(s.size());
                s.sample_xyz.push_back(w.xyz);
            }
            s.time_us.push_back(w.time_us);
            s.xyz.insert(s.xyz.end(), v.begin(), v.end());
            s.begin.push_back(s.xyz.size() / 3);
            if (s.size() % 64 == 0) say(0.2 * (double)s.size() / total);
            return true;
        }))
        return {};
    report.sweeps = (int64_t)s.size();
    report.seconds_read = seconds_since(t_read);
    const size_t ns = s.size();
    if (ns < (size_t)(2 * kNodeSweeps)) return {};
    std::vector<Pose> device(ns);
    for (size_t k = 0; k < ns; k++) reader.imu_pose(s.time_us[k], device[k].R.data(), &device[k].p.x);

    // ---- nodes: the middle sweep of each kNodeSweeps ----
    std::vector<size_t> centre;
    for (size_t k = kNodeSweeps / 2; k < ns; k += kNodeSweeps) centre.push_back(k);
    const size_t nn = centre.size();
    report.nodes = (int64_t)nn;
    std::vector<int64_t> tn(nn);
    std::vector<Pose> N(nn), Ndev(nn);
    std::vector<Vec3> up_device(nn);
    for (size_t i = 0; i < nn; i++) {
        tn[i] = s.time_us[centre[i]];
        N[i] = Ndev[i] = device[centre[i]];
        up_device[i] = sfm::mul(sfm::transpose(Ndev[i].R), Vec3{0, 0, 1});
    }
    std::vector<Edge> odometry;
    for (size_t i = 0; i + 1 < nn; i++) odometry.push_back({i, i + 1, inverse(Ndev[i]) * Ndev[i + 1], Odometry});

    spirula::xbin::Correction L;
    std::vector<Pose> world = device;
    std::map<std::pair<size_t, size_t>, Pose> loops;
    bacpu::Pool& pool = bacpu::Pool::get();
    for (int round = 0; round < kRounds; round++) {
        struct Job {
            size_t i, j;
            Kind kind;
            Pose guess, fit;
            double inliers = 0;
        };
        std::vector<Job> jobs;
        for (size_t i = 0; i < nn; i++)
            for (int lag = 1; lag <= kNeighbourLags && i + lag < nn; lag++)
                jobs.push_back({i, i + lag, Neighbour, inverse(N[i]) * N[i + lag], {}});
        const size_t n_neighbour = jobs.size();
        for (const auto& [i, j] : loop_candidates(tn, N, kLoopRadiusM[round]))
            jobs.push_back({i, j, Loop, inverse(N[i]) * N[j], {}});
        report.loops_tried += (int64_t)(jobs.size() - n_neighbour);

        // Each node's neighbour submap once, in blocks, so memory stays bounded.
        const auto t_neighbours = std::chrono::steady_clock::now();
        constexpr size_t kBlock = 64;
        std::atomic<int64_t> done{0};
        const double base = 0.2 + 0.35 * round, span = 0.35;
        for (size_t a = 0; a < nn && !cancelled(); a += kBlock) {
            const size_t b = std::min(nn, a + kBlock + kNeighbourLags);
            std::vector<Cloud> sub(b - a);
            std::vector<char> ok(b - a, 0);
            pool.run((int)(b - a), [&](int t, int) {
                ok[t] = make_cloud(gather(s, world, centre[a + t], kNeighbourHalf, N[a + t]), kNeighbourStage.voxel, sub[t]);
            });
            std::vector<size_t> mine;
            for (size_t q = 0; q < n_neighbour; q++)
                if (jobs[q].i >= a && jobs[q].i < std::min(nn, a + kBlock)) mine.push_back(q);
            pool.run((int)mine.size(), [&](int t, int) {
                Job& J = jobs[mine[t]];
                if (!ok[J.i - a] || !ok[J.j - a]) return;
                const Fit f = gicp(sub[J.i - a], sub[J.j - a], J.guess, kNeighbourStage.reach);
                J.fit = f.T;
                J.inliers = f.inlier_fraction;
                say(base + span * (double)++done / (double)jobs.size());
            });
        }
        report.seconds_neighbours += seconds_since(t_neighbours);
        // Loops in blocks of target nodes, each node's submaps built once a block.
        const auto t_loops = std::chrono::steady_clock::now();
        const std::vector<Stage>& stages = kLoopStages[round];
        for (size_t q0 = n_neighbour; q0 < jobs.size() && !cancelled();) {
            size_t q1 = q0;
            while (q1 < jobs.size() && jobs[q1].i < jobs[q0].i + kLoopBlock) q1++;
            std::vector<size_t> nodes;
            for (size_t q = q0; q < q1; q++) nodes.insert(nodes.end(), {jobs[q].i, jobs[q].j});
            std::sort(nodes.begin(), nodes.end());
            nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
            std::vector<std::vector<Cloud>> clouds(nodes.size());
            std::vector<char> ok(nodes.size(), 1);
            pool.run((int)nodes.size(), [&](int t, int) {
                const std::vector<float> pts = gather(s, world, centre[nodes[t]], kLoopHalf, N[nodes[t]]);
                clouds[t].resize(stages.size());
                for (size_t k = 0; k < stages.size(); k++) ok[t] = ok[t] && make_cloud(pts, stages[k].voxel, clouds[t][k]);
            });
            auto slot = [&](size_t node) {
                return (size_t)(std::lower_bound(nodes.begin(), nodes.end(), node) - nodes.begin());
            };
            pool.run((int)(q1 - q0), [&](int t, int) {
                Job& J = jobs[q0 + t];
                const size_t a = slot(J.i), b = slot(J.j);
                if (!ok[a] || !ok[b]) return;
                Pose T = J.guess;
                for (size_t k = 0; k < stages.size(); k++) {
                    const Fit f = gicp(clouds[a][k], clouds[b][k], T, stages[k].reach);
                    T = f.T;
                    J.inliers = f.inlier_fraction;
                }
                J.fit = T;
                say(base + span * (double)++done / (double)jobs.size());
            });
            q0 = q1;
        }
        report.seconds_loops += seconds_since(t_loops);
        if (cancelled()) return {};

        std::vector<Edge> edges = odometry;
        report.neighbour_edges = 0;
        for (size_t q = 0; q < n_neighbour; q++)
            if (jobs[q].inliers > kNeighbourInliers) {
                edges.push_back({jobs[q].i, jobs[q].j, jobs[q].fit, Neighbour});
                report.neighbour_edges++;
            }
        for (size_t q = n_neighbour; q < jobs.size(); q++) {
            const Job& J = jobs[q];
            const Pose jump = inverse(J.guess) * J.fit;
            if (J.inliers >= kLoopInliers && jump.p.norm() < kLoopJumpM && sfm::so3Log(jump.R).norm() < kLoopJumpRad)
                loops[{J.i, J.j}] = J.fit;
        }
        const size_t n_fixed = edges.size();
        for (const auto& [ij, Z] : loops) edges.push_back({ij.first, ij.second, Z, Loop});
        const auto t_graph = std::chrono::steady_clock::now();
        std::vector<Pose> solved = N;
        solve_graph(solved, edges, up_device, report, cancel);
        bool pruned = false;
        for (size_t q = n_fixed; q < edges.size(); q++) {
            double r[6];
            Vec3 d;
            edge_residual(solved, edges[q], r, d);
            if (whitened(r, Loop) > kPrune) {
                loops.erase({edges[q].i, edges[q].j});
                pruned = true;
            }
        }
        if (pruned) {
            edges.resize(n_fixed);
            for (const auto& [ij, Z] : loops) edges.push_back({ij.first, ij.second, Z, Loop});
            solve_graph(solved, edges, up_device, report, cancel);
        }
        report.seconds_graph += seconds_since(t_graph);
        if (cancelled()) return {};
        N = solved;
        report.loops = (int64_t)loops.size();
        L = {};
        for (size_t i = 0; i < nn; i++) {
            const Pose M = N[i] * inverse(Ndev[i]);
            const Vec3 r = sfm::so3Log(M.R);
            L.time_us.push_back(tn[i]);
            L.move.push_back({r.x, r.y, r.z, M.p.x, M.p.y, M.p.z});
        }
        for (size_t k = 0; k < ns; k++) world[k] = pose_at(L, s.time_us[k], device[k]);
    }

    // ---- the device's frame kept: one turn about the vertical and one shift
    // that best bring the nodes back onto the device's ----
    {
        Vec3 ca, cb;
        for (size_t i = 0; i < nn; i++) {
            ca = ca + N[i].p * (1.0 / nn);
            cb = cb + Ndev[i].p * (1.0 / nn);
        }
        double sc = 0, ss = 0;
        for (size_t i = 0; i < nn; i++) {
            const Vec3 a = N[i].p - ca, b = Ndev[i].p - cb;
            sc += a.x * b.x + a.y * b.y;
            ss += a.x * b.y - a.y * b.x;
        }
        const double yaw = std::atan2(ss, sc);
        Pose G{sfm::so3Exp({0, 0, yaw}), {}};
        G.p = cb - sfm::mul(G.R, ca);
        std::vector<double> moved(nn);
        L = {};
        for (size_t i = 0; i < nn; i++) {
            N[i] = G * N[i];
            moved[i] = (N[i].p - Ndev[i].p).norm();
            const Pose M = N[i] * inverse(Ndev[i]);
            const Vec3 r = sfm::so3Log(M.R);
            L.time_us.push_back(tn[i]);
            L.move.push_back({r.x, r.y, r.z, M.p.x, M.p.y, M.p.z});
        }
        std::nth_element(moved.begin(), moved.begin() + nn / 2, moved.end());
        report.moved_m = moved[nn / 2];
        for (size_t k = 0; k < ns; k++) world[k] = pose_at(L, s.time_us[k], device[k]);
    }
    report.voxels_device = occupied_voxels(s, device);
    report.voxels_built = occupied_voxels(s, world);
    say(1.0);
    report.kept = report.voxels_built < report.voxels_device;
    return report.kept ? L : spirula::xbin::Correction{};
}

bool ensure_trajectory(const std::string& capture, const std::string& lidar_dir,
                       const std::function<void(const std::string&)>& log,
                       const std::function<void(double)>& progress, const std::atomic<bool>* cancel) {
    const std::string path = trajectory_path(lidar_dir, capture);
    std::error_code ec;
    if (fs::exists(path, ec)) return true;
    spirula::xbin::Reader reader(capture);
    TrajectoryReport r;
    const spirula::xbin::Correction L = build_trajectory(reader, r, progress, cancel);
    if (cancel && cancel->load()) return false;
    const std::string name = fs::path(capture).filename().string();
    if (spirula::env("TRAJECTORY_PROF"))
        std::fprintf(stderr,
                     "trajectory: %lld sweeps, %lld nodes; read %.1f s, neighbours %.1f s, loops %.1f s, graph %.1f s "
                     "(CG at most %lld steps, residual %.2g); voxels %lld -> %lld\n",
                     (long long)r.sweeps, (long long)r.nodes, r.seconds_read, r.seconds_neighbours, r.seconds_loops,
                     r.seconds_graph, (long long)r.solver_steps, r.solver_residual, (long long)r.voxels_device,
                     (long long)r.voxels_built);
    if (r.kept) {
        const double sharper = 100.0 * (double)(r.voxels_device - r.voxels_built) / (double)r.voxels_device;
        char moved[32], gain[32];
        std::snprintf(moved, sizeof moved, "%.3f", r.moved_m);
        std::snprintf(gain, sizeof gain, "%.1f", sharper);
        log(format(lmsg::trajectory_built, {name, (long long)r.loops, (long long)r.loops_tried, std::string(moved),
                                            std::string(gain)}));
    } else {
        log(format(lmsg::trajectory_device, {name, (long long)r.voxels_built, (long long)r.voxels_device}));
    }
    fs::create_directories(lidar_dir, ec);
    write_trajectory(path, L);
    return true;
}

}  // namespace app::lidar
