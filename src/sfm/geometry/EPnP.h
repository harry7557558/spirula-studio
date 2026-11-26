// EPnP non-minimal absolute pose solver (Lepetit, Moreno-Noguer, Fua 2009).
// Takes n >= 4 3D world points and unit camera bearings (world -> camera).
// Handles 3D and planar scenes, perspective and wide/fisheye rays, via N=1..4
// nullspace hypotheses, beta relinearization, and Levenberg-Marquardt polish.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "sfm/geometry/Essential.h"  // Pose
#include "sfm/geometry/LinAlg.h"

namespace sfm {
namespace epnp_detail {

inline int symIdx(int i, int j, int dim) {
    if (i > j) std::swap(i, j);
    return i * dim - (i * (i - 1)) / 2 + (j - i);
}

// Gaussian elimination with partial pivoting for small dense systems A * x = b.
inline bool solveLinear(double* A, double* b, int dim) {
    double max_entry = 0.0;
    for (int i = 0; i < dim * dim; i++) max_entry = std::max(max_entry, std::fabs(A[i]));
    if (!(max_entry > 0.0)) return false;
    const double tol = 1e-12 * max_entry;
    for (int col = 0; col < dim; col++) {
        int piv = col;
        for (int r = col + 1; r < dim; r++)
            if (std::fabs(A[r * dim + col]) > std::fabs(A[piv * dim + col])) piv = r;
        if (!(std::fabs(A[piv * dim + col]) > tol)) return false;
        if (piv != col) {
            for (int c = col; c < dim; c++) std::swap(A[piv * dim + c], A[col * dim + c]);
            std::swap(b[piv], b[col]);
        }
        const double inv = 1.0 / A[col * dim + col];
        for (int r = col + 1; r < dim; r++) {
            const double f = A[r * dim + col] * inv;
            if (f == 0.0) continue;
            for (int c = col; c < dim; c++) A[r * dim + c] -= f * A[col * dim + c];
            b[r] -= f * b[col];
        }
    }
    for (int r = dim - 1; r >= 0; r--) {
        double s = b[r];
        for (int c = r + 1; c < dim; c++) s -= A[r * dim + c] * b[c];
        b[r] = s / A[r * dim + r];
    }
    for (int i = 0; i < dim; i++)
        if (!std::isfinite(b[i])) return false;
    return true;
}

inline bool solveNormalEquations(const double* A, const double* rhs, int rows, int cols,
                                 double* out) {
    double AtA[36] = {};
    double Atr[6] = {};
    for (int r = 0; r < rows; r++) {
        const double* row = A + r * cols;
        for (int i = 0; i < cols; i++) {
            Atr[i] += row[i] * rhs[r];
            for (int j = i; j < cols; j++) AtA[i * cols + j] += row[i] * row[j];
        }
    }
    double max_diag = 0.0;
    for (int i = 0; i < cols; i++) {
        max_diag = std::max(max_diag, AtA[i * cols + i]);
        for (int j = 0; j < i; j++) AtA[i * cols + j] = AtA[j * cols + i];
    }
    if (!(max_diag > 0.0)) return false;
    const double ridge = 1e-12 * max_diag;
    for (int i = 0; i < cols; i++) AtA[i * cols + i] += ridge;
    if (!solveLinear(AtA, Atr, cols)) return false;
    for (int i = 0; i < cols; i++) out[i] = Atr[i];
    return true;
}

// Recover rank-1 beta from packed symmetric monomials B_{ij} = beta_i * beta_j.
inline bool betaFromSymMonomials(const double* B, int n, std::vector<double>& beta) {
    double M[16] = {}, w[4] = {}, V[16] = {};
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++) M[i * n + j] = M[j * n + i] = B[symIdx(i, j, n)];
    jacobiEigenSymmetric(M, n, w, V);
    int best = 0;
    for (int k = 1; k < n; k++)
        if (w[k] > w[best]) best = k;
    if (!(w[best] > 1e-12) || !std::isfinite(w[best])) return false;
    beta.resize((size_t)n);
    const double s = std::sqrt(w[best]);
    for (int i = 0; i < n; i++) beta[(size_t)i] = s * V[i * n + best];
    return true;
}

inline void optimizeBetasLM(const double* rho, int npairs, const Vec3 d[6][4],
                            std::vector<double>& b, int max_iters = 15) {
    const int N = (int)b.size();
    auto evalCost = [&](const std::vector<double>& bv) {
        double cost = 0.0;
        for (int p = 0; p < npairs; p++) {
            Vec3 dv{0.0, 0.0, 0.0};
            for (int k = 0; k < N; k++) dv = dv + d[p][k] * bv[(size_t)k];
            const double r = dv.dot(dv) - rho[p];
            cost += r * r;
        }
        return cost;
    };
    double c0 = evalCost(b);
    double lam = 1e-4;
    for (int it = 0; it < max_iters; it++) {
        double JtJ[16] = {}, Jtr[4] = {};
        for (int p = 0; p < npairs; p++) {
            Vec3 dv{0.0, 0.0, 0.0};
            for (int k = 0; k < N; k++) dv = dv + d[p][k] * b[(size_t)k];
            const double rp = dv.dot(dv) - rho[p];
            double Jp[4];
            for (int k = 0; k < N; k++) {
                Jp[k] = 2.0 * dv.dot(d[p][k]);
                Jtr[k] += Jp[k] * rp;
            }
            for (int i = 0; i < N; i++)
                for (int j = i; j < N; j++) JtJ[i * N + j] += Jp[i] * Jp[j];
        }
        double max_diag = 0.0;
        for (int i = 0; i < N; i++) {
            max_diag = std::max(max_diag, JtJ[i * N + i]);
            for (int j = 0; j < i; j++) JtJ[i * N + j] = JtJ[j * N + i];
        }
        bool accepted = false;
        for (int tries = 0; tries < 6; tries++) {
            double A[16], g[4];
            for (int i = 0; i < N * N; i++) A[i] = JtJ[i];
            for (int i = 0; i < N; i++) {
                A[i * N + i] += lam * std::max(JtJ[i * N + i], std::max(max_diag * 1e-12, 1e-12));
                g[i] = -Jtr[i];
            }
            if (!solveLinear(A, g, N)) { lam *= 10.0; continue; }
            std::vector<double> bn = b;
            double sn2 = 0.0, bn2 = 0.0;
            for (int i = 0; i < N; i++) {
                bn[(size_t)i] += g[i];
                sn2 += g[i] * g[i];
                bn2 += bn[(size_t)i] * bn[(size_t)i];
            }
            const double cn = evalCost(bn);
            if (cn < c0) {
                b.swap(bn);
                c0 = cn;
                lam = std::max(lam * 0.3, 1e-12);
                accepted = true;
                if (sn2 < 1e-20 * std::max(1.0, bn2)) return;
                break;
            }
            lam *= 10.0;
        }
        if (!accepted && lam > 1e8) break;
    }
}

// Relinearization for the 4D nullspace case (minimally sampled n=4 or weak depth).
inline void addRelinearizationSeeds(const Vec3 d[6][4], const double rho[6],
                                    std::vector<std::vector<double>>& seeds) {
    double L[6][10] = {}, LtL[100] = {}, Ltr[10] = {};
    for (int p = 0; p < 6; p++) {
        for (int i = 0; i < 4; i++)
            for (int j = i; j < 4; j++)
                L[p][symIdx(i, j, 4)] = (i == j ? 1.0 : 2.0) * d[p][i].dot(d[p][j]);
        for (int c = 0; c < 10; c++) {
            Ltr[c] += L[p][c] * rho[p];
            for (int r = c; r < 10; r++) LtL[c * 10 + r] += L[p][c] * L[p][r];
        }
    }
    for (int c = 0; c < 10; c++)
        for (int r = 0; r < c; r++) LtL[c * 10 + r] = LtL[r * 10 + c];

    double evals[10], VL[100];
    jacobiEigenSymmetric(LtL, 10, evals, VL);
    std::array<int, 10> ord{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::sort(ord.begin(), ord.end(), [&](int a, int b) { return evals[a] > evals[b]; });
    if (!(evals[ord[0]] > 0.0) || !(evals[ord[5]] > 1e-12 * evals[ord[0]])) return;

    double b0[10] = {}, K[4][10] = {};
    for (int r = 0; r < 6; r++) {
        const int col = ord[(size_t)r];
        double dot = 0.0;
        for (int i = 0; i < 10; i++) dot += VL[i * 10 + col] * Ltr[i];
        const double inv = dot / evals[col];
        for (int i = 0; i < 10; i++) b0[i] += inv * VL[i * 10 + col];
    }
    for (int a = 0; a < 4; a++) {
        const int col = ord[(size_t)(6 + a)];
        for (int i = 0; i < 10; i++) K[a][i] = VL[i * 10 + col];
    }

    // 20 quadratic 2x2 minor equations over the 15 monomials [g_a*g_b, g_a, 1].
    std::array<std::array<int, 4>, 20> eqs{};
    int neq = 0;
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            eqs[(size_t)neq++] = {symIdx(i, i, 4), symIdx(j, j, 4),
                                  symIdx(i, j, 4), symIdx(i, j, 4)};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = j + 1; k < 4; k++)
                if (j != i && k != i)
                    eqs[(size_t)neq++] = {symIdx(i, i, 4), symIdx(j, k, 4),
                                          symIdx(i, j, 4), symIdx(i, k, 4)};
    eqs[(size_t)neq++] = {symIdx(0, 1, 4), symIdx(2, 3, 4), symIdx(0, 2, 4), symIdx(1, 3, 4)};
    eqs[(size_t)neq++] = {symIdx(0, 1, 4), symIdx(2, 3, 4), symIdx(0, 3, 4), symIdx(1, 2, 4)};

    double AtA[225] = {};
    for (int e = 0; e < neq; e++) {
        const int i1 = eqs[(size_t)e][0], i2 = eqs[(size_t)e][1];
        const int j1 = eqs[(size_t)e][2], j2 = eqs[(size_t)e][3];
        double row[15] = {};
        for (int a = 0; a < 4; a++) {
            for (int b = 0; b < 4; b++)
                row[symIdx(a, b, 4)] += K[a][i1] * K[b][i2] - K[a][j1] * K[b][j2];
            row[10 + a] += b0[i1] * K[a][i2] + b0[i2] * K[a][i1] -
                           b0[j1] * K[a][j2] - b0[j2] * K[a][j1];
        }
        row[14] += b0[i1] * b0[i2] - b0[j1] * b0[j2];
        for (int p = 0; p < 15; p++)
            for (int q = p; q < 15; q++) AtA[p * 15 + q] += row[p] * row[q];
    }
    for (int p = 0; p < 15; p++)
        for (int q = 0; q < p; q++) AtA[p * 15 + q] = AtA[q * 15 + p];

    double eA[15], VA[225];
    jacobiEigenSymmetric(AtA, 15, eA, VA);
    std::array<int, 15> ordA{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    std::sort(ordA.begin(), ordA.end(), [&](int a, int b) { return eA[a] < eA[b]; });
    for (int ck = 0; ck < 4; ck++) {
        const int col = ordA[(size_t)ck];
        const double w = VA[14 * 15 + col];
        if (std::fabs(w) <= 1e-10) continue;
        double B[10];
        for (int m = 0; m < 10; m++) {
            double val = b0[m];
            for (int a = 0; a < 4; a++) val += (VA[(10 + a) * 15 + col] / w) * K[a][m];
            B[m] = val;
        }
        std::vector<double> beta;
        if (betaFromSymMonomials(B, 4, beta)) seeds.push_back(std::move(beta));
    }
}

inline bool procrustesPose(const std::vector<Vec3>& Yn, const std::vector<Vec3>& pc,
                           const Vec3& world_mean, double world_scale, Pose& out, Vec3& t_norm) {
    const size_t n = Yn.size();
    Vec3 mpc{0.0, 0.0, 0.0};
    for (size_t i = 0; i < n; i++) mpc = mpc + pc[i];
    mpc = mpc * (1.0 / (double)n);

    Mat3 H{};
    double var_w = 0.0;
    for (size_t i = 0; i < n; i++) {
        const Vec3 dc = pc[i] - mpc;
        const Vec3& dw = Yn[i];
        var_w += dw.dot(dw);
        H[0] += dc.x * dw.x; H[1] += dc.x * dw.y; H[2] += dc.x * dw.z;
        H[3] += dc.y * dw.x; H[4] += dc.y * dw.y; H[5] += dc.y * dw.z;
        H[6] += dc.z * dw.x; H[7] += dc.z * dw.y; H[8] += dc.z * dw.z;
    }
    if (!(var_w > 1e-12)) return false;

    const Svd3 svd = svd3(H);
    const double flip = det3(svd.U) * det3(svd.V) < 0.0 ? -1.0 : 1.0;
    const Mat3 D = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, flip};
    out.R = mul(mul(svd.U, D), transpose(svd.V));
    const double scale = (svd.s.x + svd.s.y + flip * svd.s.z) / var_w;
    if (!(scale > 1e-12) || !std::isfinite(scale)) return false;

    t_norm = mpc * (1.0 / scale);
    out.t = t_norm * world_scale - mul(out.R, world_mean);
    for (double v : out.R)
        if (!std::isfinite(v)) return false;
    return std::isfinite(out.t.x) && std::isfinite(out.t.y) && std::isfinite(out.t.z);
}

inline void solveConfig(const std::vector<Vec3>& Yn, const std::vector<Vec3>& bn,
                        const Vec3& world_mean, double world_scale, const Vec3 axes[3],
                        const double kr[3], bool planar, Pose& best_pose, int& best_front,
                        double& best_err) {
    const int n = (int)Yn.size();
    const int ncp = planar ? 3 : 4;
    const int dim = 3 * ncp;
    const int npairs = planar ? 3 : 6;
    static const int kPairs[6][2] = {{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 3}, {2, 3}};

    Vec3 Cw[4] = {};
    for (int r = 0; r < ncp - 1; r++) Cw[r + 1] = axes[r] * kr[r];

    std::vector<std::array<double, 4>> alphas((size_t)n, {0.0, 0.0, 0.0, 0.0});
    double MtM[144] = {};
    for (int i = 0; i < n; i++) {
        double a0 = 1.0;
        for (int r = 0; r < ncp - 1; r++) {
            const double ar = Yn[(size_t)i].dot(axes[r]) / kr[r];
            alphas[(size_t)i][(size_t)(r + 1)] = ar;
            a0 -= ar;
        }
        alphas[(size_t)i][0] = a0;

        Vec3 r0, r1;
        const Vec3& ray = bn[(size_t)i];
        if (ray.z > 0.1) {
            r0 = {1.0, 0.0, -ray.x / ray.z};
            r1 = {0.0, 1.0, -ray.y / ray.z};
        } else {
            const Vec3 ax = std::fabs(ray.x) < 0.9 ? Vec3{1.0, 0.0, 0.0} : Vec3{0.0, 1.0, 0.0};
            r0 = ax.cross(ray).normalized();
            r1 = ray.cross(r0);
        }
        double row0[12], row1[12];
        for (int j = 0; j < ncp; j++) {
            const double a = alphas[(size_t)i][(size_t)j];
            row0[3 * j + 0] = a * r0.x; row0[3 * j + 1] = a * r0.y; row0[3 * j + 2] = a * r0.z;
            row1[3 * j + 0] = a * r1.x; row1[3 * j + 1] = a * r1.y; row1[3 * j + 2] = a * r1.z;
        }
        for (int p = 0; p < dim; p++)
            for (int q = p; q < dim; q++)
                MtM[p * dim + q] += row0[p] * row0[q] + row1[p] * row1[q];
    }
    for (int p = 0; p < dim; p++)
        for (int q = 0; q < p; q++) MtM[p * dim + q] = MtM[q * dim + p];

    double ew[12], eV[144];
    jacobiEigenSymmetric(MtM, dim, ew, eV);
    std::array<int, 12> ord{};
    for (int i = 0; i < dim; i++) ord[(size_t)i] = i;
    std::sort(ord.begin(), ord.begin() + dim, [&](int a, int b) { return ew[a] < ew[b]; });

    double rho[6] = {};
    Vec3 d[6][4] = {};
    for (int p = 0; p < npairs; p++) {
        const int a = kPairs[p][0], b = kPairs[p][1];
        const Vec3 dw = Cw[a] - Cw[b];
        rho[p] = dw.dot(dw);
        for (int k = 0; k < 4; k++) {
            const int col = ord[(size_t)k];
            d[p][k] = {eV[(3 * a + 0) * dim + col] - eV[(3 * b + 0) * dim + col],
                       eV[(3 * a + 1) * dim + col] - eV[(3 * b + 1) * dim + col],
                       eV[(3 * a + 2) * dim + col] - eV[(3 * b + 2) * dim + col]};
        }
    }

    std::vector<std::vector<double>> seeds;
    {
        double num = 0.0, den = 0.0;
        for (int p = 0; p < npairs; p++) {
            num += d[p][0].norm() * std::sqrt(rho[p]);
            den += d[p][0].dot(d[p][0]);
        }
        if (den > 1e-24) seeds.push_back({num / den});
    }
    const int maxN = planar ? 2 : 3;
    for (int N = 2; N <= maxN; N++) {
        const int cols = N * (N + 1) / 2;
        double L[36] = {};
        for (int p = 0; p < npairs; p++)
            for (int i = 0; i < N; i++)
                for (int j = i; j < N; j++)
                    L[p * cols + symIdx(i, j, N)] =
                        (i == j ? 1.0 : 2.0) * d[p][i].dot(d[p][j]);
        double B[6] = {};
        std::vector<double> beta;
        if (solveNormalEquations(L, rho, npairs, cols, B) && betaFromSymMonomials(B, N, beta))
            seeds.push_back(std::move(beta));
    }
    if (!planar) {
        for (int anchor = 0; anchor < 4; anchor++) {
            double L4[24] = {}, b4[4] = {};
            for (int p = 0; p < 6; p++) {
                L4[p * 4 + 0] = d[p][anchor].dot(d[p][anchor]);
                int col = 1;
                for (int k = 0; k < 4; k++)
                    if (k != anchor) L4[p * 4 + col++] = 2.0 * d[p][anchor].dot(d[p][k]);
            }
            if (solveNormalEquations(L4, rho, 6, 4, b4) && b4[0] > 1e-12) {
                std::vector<double> bv(4, 0.0);
                const double s0 = std::sqrt(b4[0]);
                bv[(size_t)anchor] = s0;
                int col = 1;
                for (int k = 0; k < 4; k++)
                    if (k != anchor) bv[(size_t)k] = b4[col++] / s0;
                seeds.push_back(std::move(bv));
            }
        }
        if (n <= 6 || ew[ord[3]] <= 1e-2 * std::max(ew[ord[4]], 1e-12))
            addRelinearizationSeeds(d, rho, seeds);
    }

    std::vector<Vec3> pc((size_t)n);
    for (std::vector<double>& beta : seeds) {
        optimizeBetasLM(rho, npairs, d, beta);
        const int N = (int)beta.size();
        Vec3 Cc[4] = {};
        for (int k = 0; k < N; k++) {
            const int col = ord[(size_t)k];
            const double bk = beta[(size_t)k];
            for (int j = 0; j < ncp; j++)
                Cc[j] = Cc[j] + Vec3{eV[(3 * j + 0) * dim + col],
                                     eV[(3 * j + 1) * dim + col],
                                     eV[(3 * j + 2) * dim + col]} * bk;
        }
        int pos = 0, neg = 0;
        for (int i = 0; i < n; i++) {
            Vec3 p{0.0, 0.0, 0.0};
            for (int j = 0; j < ncp; j++) p = p + Cc[j] * alphas[(size_t)i][(size_t)j];
            pc[(size_t)i] = p;
            const double dt = p.dot(bn[(size_t)i]);
            if (dt > 0.0) pos++;
            else if (dt < 0.0) neg++;
        }
        if (pos == 0 && neg == 0) continue;
        if (neg > pos)
            for (Vec3& p : pc) p = p * -1.0;

        Pose cand;
        Vec3 t_norm;
        if (!procrustesPose(Yn, pc, world_mean, world_scale, cand, t_norm)) continue;

        int front = 0;
        double err = 0.0;
        for (int i = 0; i < n; i++) {
            const Vec3 q = mul(cand.R, Yn[(size_t)i]) + t_norm;
            const double qn = q.norm();
            if (!(qn > 1e-12)) continue;
            const double c = q.dot(bn[(size_t)i]) / qn;
            if (c > 0.0) {
                front++;
                err += 1.0 - c;
            }
        }
        if (front > best_front || (front == best_front && err < best_err)) {
            best_pose = cand;
            best_front = front;
            best_err = err;
        }
    }
}

}  // namespace epnp_detail

// EPnP pose from >= 4 correspondences: world points `X` and unit bearings `b`.
// Returns 0 or 1 candidate pose in world->camera convention (x_cam = R X + t).
inline std::vector<Pose> epnp(const std::vector<Vec3>& X, const std::vector<Vec3>& b,
                              const std::vector<int>& idx) {
    using namespace epnp_detail;
    if (X.size() != b.size() || idx.size() < 4) return {};

    const size_t n = idx.size();
    Vec3 mean{0.0, 0.0, 0.0};
    std::vector<Vec3> bn(n);
    for (size_t k = 0; k < n; k++) {
        const int i = idx[k];
        if (i < 0 || i >= (int)X.size()) return {};
        const Vec3& Xi = X[(size_t)i];
        const Vec3& bi = b[(size_t)i];
        if (!std::isfinite(Xi.x) || !std::isfinite(Xi.y) || !std::isfinite(Xi.z) ||
            !std::isfinite(bi.x) || !std::isfinite(bi.y) || !std::isfinite(bi.z))
            return {};
        const double bnorm = bi.norm();
        if (!(bnorm > 1e-12)) return {};
        bn[k] = bi * (1.0 / bnorm);
        mean = mean + Xi;
    }
    mean = mean * (1.0 / (double)n);

    std::vector<Vec3> Yn(n);
    double sum2 = 0.0;
    for (size_t k = 0; k < n; k++) {
        const Vec3 y = X[(size_t)idx[k]] - mean;
        Yn[k] = y;
        sum2 += y.dot(y);
    }
    const double world_scale = std::sqrt(sum2 / (double)n);
    if (!(world_scale > 1e-15) || !std::isfinite(world_scale)) return {};

    double S[9] = {};
    const double inv_scale = 1.0 / world_scale;
    for (size_t k = 0; k < n; k++) {
        Yn[k] = Yn[k] * inv_scale;
        const Vec3& y = Yn[k];
        S[0] += y.x * y.x; S[1] += y.x * y.y; S[2] += y.x * y.z;
        S[4] += y.y * y.y; S[5] += y.y * y.z; S[8] += y.z * y.z;
    }
    S[3] = S[1]; S[6] = S[2]; S[7] = S[5];

    double w[3] = {}, V[9] = {};
    jacobiEigenSymmetric(S, 3, w, V);
    std::array<int, 3> ord{0, 1, 2};
    std::sort(ord.begin(), ord.end(), [&](int a, int b_) { return w[a] > w[b_]; });
    if (!(w[ord[0]] > 0.0) || !(w[ord[1]] > 1e-12 * w[ord[0]])) return {};

    Vec3 axes[3];
    axes[0] = Vec3{V[0 * 3 + ord[0]], V[1 * 3 + ord[0]], V[2 * 3 + ord[0]]}.normalized();
    axes[1] = Vec3{V[0 * 3 + ord[1]], V[1 * 3 + ord[1]], V[2 * 3 + ord[1]]};
    axes[1] = (axes[1] - axes[0] * axes[1].dot(axes[0])).normalized();
    axes[2] = axes[0].cross(axes[1]).normalized();

    const double kr[3] = {std::sqrt(w[ord[0]] / (double)n),
                          std::sqrt(w[ord[1]] / (double)n),
                          std::sqrt(std::max(0.0, w[ord[2]]) / (double)n)};

    Pose best_pose;
    int best_front = 0;
    double best_err = 1e300;
    if (kr[2] <= 1e-6 * kr[0]) {
        solveConfig(Yn, bn, mean, world_scale, axes, kr, true, best_pose, best_front, best_err);
    } else {
        solveConfig(Yn, bn, mean, world_scale, axes, kr, false, best_pose, best_front, best_err);
        if (kr[2] < 1e-2 * kr[0])
            solveConfig(Yn, bn, mean, world_scale, axes, kr, true, best_pose, best_front, best_err);
    }
    if (best_front == 0) return {};
    return {best_pose};
}

inline std::vector<Pose> epnp(const std::vector<Vec3>& X, const std::vector<Vec3>& b) {
    std::vector<int> idx(X.size());
    for (size_t i = 0; i < X.size(); i++) idx[i] = (int)i;
    return epnp(X, b, idx);
}

}  // namespace sfm
