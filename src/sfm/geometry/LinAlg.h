// Small dense linear algebra for the geometry estimators (host only, no Eigen).
//
// Just enough for two-view geometry: fixed 2/3-vectors and 3x3 matrices, a
// cyclic Jacobi eigensolver for symmetric n x n (n <= 9, used for DLT null
// spaces via A^T A), and a 3x3 SVD built on it (rank-2 enforcement, essential/
// homography decomposition). Accuracy over speed -- these run on RANSAC-sized
// samples, not in a hot loop.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sfm {

struct Vec2 {
    double x = 0, y = 0;
};
struct Vec3 {
    double x = 0, y = 0, z = 0;
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    double norm() const { return std::sqrt(dot(*this)); }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    Vec3 normalized() const {
        double n = norm();
        return n > 0 ? Vec3{x / n, y / n, z / n} : *this;
    }
};

// 3x3, row-major.
using Mat3 = std::array<double, 9>;

inline Mat3 mat3Identity() { return {1, 0, 0, 0, 1, 0, 0, 0, 1}; }

// [v]_x, the matrix with [v]_x u == v x u.
inline Mat3 crossMatrix(const Vec3& v) {
    return {0, -v.z, v.y, v.z, 0, -v.x, -v.y, v.x, 0};
}

inline Vec3 mul(const Mat3& M, const Vec3& v) {
    return {M[0] * v.x + M[1] * v.y + M[2] * v.z, M[3] * v.x + M[4] * v.y + M[5] * v.z,
            M[6] * v.x + M[7] * v.y + M[8] * v.z};
}

inline Mat3 mul(const Mat3& A, const Mat3& B) {
    Mat3 C{};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            double s = 0;
            for (int k = 0; k < 3; k++) s += A[3 * i + k] * B[3 * k + j];
            C[3 * i + j] = s;
        }
    return C;
}

inline Mat3 transpose(const Mat3& A) {
    return {A[0], A[3], A[6], A[1], A[4], A[7], A[2], A[5], A[8]};
}

inline double det3(const Mat3& A) {
    return A[0] * (A[4] * A[8] - A[5] * A[7]) - A[1] * (A[3] * A[8] - A[5] * A[6]) +
           A[2] * (A[3] * A[7] - A[4] * A[6]);
}

inline Mat3 inverse3(const Mat3& A, bool* ok = nullptr) {
    double d = det3(A);
    if (ok) *ok = std::fabs(d) > 1e-300;
    if (std::fabs(d) < 1e-300) return mat3Identity();
    double id = 1.0 / d;
    Mat3 C;
    C[0] = (A[4] * A[8] - A[5] * A[7]) * id;
    C[1] = (A[2] * A[7] - A[1] * A[8]) * id;
    C[2] = (A[1] * A[5] - A[2] * A[4]) * id;
    C[3] = (A[5] * A[6] - A[3] * A[8]) * id;
    C[4] = (A[0] * A[8] - A[2] * A[6]) * id;
    C[5] = (A[2] * A[3] - A[0] * A[5]) * id;
    C[6] = (A[3] * A[7] - A[4] * A[6]) * id;
    C[7] = (A[1] * A[6] - A[0] * A[7]) * id;
    C[8] = (A[0] * A[4] - A[1] * A[3]) * id;
    return C;
}

// Cyclic Jacobi eigen-decomposition of a symmetric n x n matrix (row-major in
// `A`, destroyed). Eigenvalues -> w[n], eigenvectors -> columns of V[n*n].
// Not sorted. This form allocates nothing, for callers in a hot loop.
inline void jacobiEigenSymmetric(double* A, int n, double* w, double* V) {
    for (int i = 0; i < n; i++) w[i] = 0;
    for (size_t i = 0; i < (size_t)n * n; i++) V[i] = 0;
    for (int i = 0; i < n; i++) V[(size_t)i * n + i] = 1.0;

    // Convergence is measured against the matrix's own scale. The old test was
    // the absolute `off < 1e-30`, which a badly scaled A never reaches, so it
    // burned all 100 sweeps every call (D24).
    double frob2 = 0;
    for (size_t i = 0; i < (size_t)n * n; i++) frob2 += A[i] * A[i];
    if (!(frob2 > 0)) return;  // zero matrix: eigenvalues 0, V = I
    const double tol = 1e-30 * frob2;

    for (int sweep = 0; sweep < 50; sweep++) {
        double off = 0;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) off += A[(size_t)p * n + q] * A[(size_t)p * n + q];
        if (off <= tol) break;
        // Classic threshold strategy: while still far from converged, only
        // rotate the off-diagonals that carry real weight.
        const double tresh = sweep < 3 ? 0.2 * off / ((double)n * n) : 0.0;

        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) {
                double apq = A[(size_t)p * n + q];
                if (apq * apq <= tresh * tresh) continue;  // also skips apq == 0
                double app = A[(size_t)p * n + p], aqq = A[(size_t)q * n + q];
                // Rotation from the standard tangent form instead of
                // atan2/cos/sin: with theta = cot(2 phi),
                //   t = tan(phi) = sign(theta) / (|theta| + sqrt(theta^2 + 1))
                // is the same rotation on the |phi| <= pi/4 branch, for one
                // sqrt instead of three transcendental calls. This is the inner
                // loop of every RANSAC trial, so it dominates verification.
                double theta = 0.5 * (aqq - app) / apq;
                double t = (theta >= 0 ? 1.0 : -1.0) /
                           (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                double c = 1.0 / std::sqrt(t * t + 1.0);
                double s = t * c;
                for (int k = 0; k < n; k++) {
                    double akp = A[(size_t)k * n + p], akq = A[(size_t)k * n + q];
                    A[(size_t)k * n + p] = c * akp - s * akq;
                    A[(size_t)k * n + q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; k++) {
                    double apk = A[(size_t)p * n + k], aqk = A[(size_t)q * n + k];
                    A[(size_t)p * n + k] = c * apk - s * aqk;
                    A[(size_t)q * n + k] = s * apk + c * aqk;
                }
                for (int k = 0; k < n; k++) {
                    double vkp = V[(size_t)k * n + p], vkq = V[(size_t)k * n + q];
                    V[(size_t)k * n + p] = c * vkp - s * vkq;
                    V[(size_t)k * n + q] = s * vkp + c * vkq;
                }
            }
    }
    for (int i = 0; i < n; i++) w[i] = A[(size_t)i * n + i];
}

inline void jacobiEigenSymmetric(std::vector<double>& A, int n, std::vector<double>& w,
                                 std::vector<double>& V) {
    w.assign(n, 0);
    V.assign((size_t)n * n, 0);
    jacobiEigenSymmetric(A.data(), n, w.data(), V.data());
}

// Exact null space of an m x 9 matrix with m < 9, by Householder QR of A^T.
//
// A minimal RANSAC sample gives a constraint matrix that is rank deficient *by
// construction*, so its null space is exact and needs no iteration at all. That
// matters because this is the overwhelming majority of calls and the iterative
// A^T A + Jacobi path below was, measured, essentially the entire cost of
// geometric verification (D27).
//
// A^T = Q^T R with Q = H_{m-1} ... H_0, so the first m rows of Q span the row
// space of A and rows m..8 span its null space. Q is a product of Householder
// reflections, hence orthogonal whatever the input: a degenerate sample yields
// vectors that are still orthonormal but no longer span the null space, and
// RANSAC scores and discards the resulting model exactly as it would any other.
inline std::vector<std::array<double, 9>> nullSpaceQR9(const std::vector<double>& A, int m,
                                                       int count) {
    double M[9][9] = {};  // A^T, 9 x m
    for (int r = 0; r < m; r++)
        for (int c = 0; c < 9; c++) M[c][r] = A[(size_t)r * 9 + c];
    double Q[9][9] = {};
    for (int i = 0; i < 9; i++) Q[i][i] = 1.0;

    for (int k = 0; k < m; k++) {
        double nrm = 0;
        for (int i = k; i < 9; i++) nrm += M[i][k] * M[i][k];
        nrm = std::sqrt(nrm);
        if (!(nrm > 1e-300)) continue;  // column already eliminated
        // Reflect away from the larger component to avoid cancellation.
        const double alpha = M[k][k] > 0 ? -nrm : nrm;
        double v[9] = {};
        for (int i = k; i < 9; i++) v[i] = M[i][k];
        v[k] -= alpha;
        double vn = 0;
        for (int i = k; i < 9; i++) vn += v[i] * v[i];
        if (!(vn > 1e-300)) continue;
        for (int c = k; c < m; c++) {  // apply H to the remaining columns of M
            double d = 0;
            for (int i = k; i < 9; i++) d += v[i] * M[i][c];
            d = 2 * d / vn;
            for (int i = k; i < 9; i++) M[i][c] -= d * v[i];
        }
        for (int c = 0; c < 9; c++) {  // accumulate H into Q
            double d = 0;
            for (int i = k; i < 9; i++) d += v[i] * Q[i][c];
            d = 2 * d / vn;
            for (int i = k; i < 9; i++) Q[i][c] -= d * v[i];
        }
    }

    std::vector<std::array<double, 9>> out;
    for (int r = m; r < 9 && (int)out.size() < count; r++) {
        std::array<double, 9> v{};
        for (int c = 0; c < 9; c++) v[c] = Q[r][c];
        out.push_back(v);
    }
    return out;
}

// Right null vector(s) of an m x 9 matrix A: the eigenvectors of A^T A with the
// `count` smallest eigenvalues, smallest first. Returns count vectors of len 9.
inline std::vector<std::array<double, 9>> nullVectors9(const std::vector<double>& A, int m,
                                                       int count) {
    // Under-determined: take the exact null space directly rather than iterate
    // an eigensolver for it (D27). Over-determined systems fall through -- there
    // the answer is the *smallest singular vector*, not a null space, and only
    // the eigendecomposition gives it. That is the local-optimization refit,
    // which runs ~10 times per pair against the minimal solver's ~10000.
    if (m < 9 && count <= 9 - m) return nullSpaceQR9(A, m, count);

    std::vector<double> ata((size_t)9 * 9, 0.0);
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++) {
            double s = 0;
            for (int r = 0; r < m; r++) s += A[(size_t)r * 9 + i] * A[(size_t)r * 9 + j];
            ata[(size_t)i * 9 + j] = s;
        }
    std::vector<double> w, V;
    jacobiEigenSymmetric(ata, 9, w, V);
    // indices of ascending eigenvalue
    std::array<int, 9> idx{0, 1, 2, 3, 4, 5, 6, 7, 8};
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return w[a] < w[b]; });
    std::vector<std::array<double, 9>> out;
    for (int c = 0; c < count; c++) {
        std::array<double, 9> v{};
        for (int r = 0; r < 9; r++) v[r] = V[(size_t)r * 9 + idx[c]];
        out.push_back(v);
    }
    return out;
}

// Smallest right singular vector of a rows x cols matrix (row-major `A`), via
// the smallest eigenvector of A^T A. `cols` up to ~16.
inline std::vector<double> nullspaceVector(const std::vector<double>& A, int rows, int cols) {
    std::vector<double> ata((size_t)cols * cols, 0.0);
    for (int i = 0; i < cols; i++)
        for (int j = 0; j < cols; j++) {
            double s = 0;
            for (int r = 0; r < rows; r++) s += A[(size_t)r * cols + i] * A[(size_t)r * cols + j];
            ata[(size_t)i * cols + j] = s;
        }
    std::vector<double> w, V;
    jacobiEigenSymmetric(ata, cols, w, V);
    int mi = 0;
    for (int i = 1; i < cols; i++)
        if (w[i] < w[mi]) mi = i;
    std::vector<double> v(cols);
    for (int r = 0; r < cols; r++) v[r] = V[(size_t)r * cols + mi];
    return v;
}

// SVD of a 3x3 matrix: A = U * diag(s) * V^T, singular values descending,
// U and V orthonormal. Built from the symmetric eigen-decomposition of A^T A.
struct Svd3 {
    Mat3 U, V;
    Vec3 s;
};

inline Svd3 svd3(const Mat3& A) {
    // V and s^2 from eigen(A^T A)
    Mat3 AtA = mul(transpose(A), A);
    std::vector<double> M(AtA.begin(), AtA.end()), w, Vv;
    jacobiEigenSymmetric(M, 3, w, Vv);
    int idx[3] = {0, 1, 2};
    std::sort(idx, idx + 3, [&](int a, int b) { return w[a] > w[b]; });

    Svd3 r;
    r.s = {0, 0, 0};
    Vec3 vcol[3];
    for (int c = 0; c < 3; c++) {
        (&r.s.x)[c] = std::sqrt(std::max(0.0, w[idx[c]]));
        vcol[c] = {Vv[0 * 3 + idx[c]], Vv[1 * 3 + idx[c]], Vv[2 * 3 + idx[c]]};
    }
    // V columns
    for (int c = 0; c < 3; c++) {
        r.V[0 * 3 + c] = vcol[c].x;
        r.V[1 * 3 + c] = vcol[c].y;
        r.V[2 * 3 + c] = vcol[c].z;
    }
    // U columns = A v_c / s_c, completing any direction whose singular value is
    // numerically zero.
    //
    // The cutoff has to be RELATIVE, and generous. s_c comes from the square
    // root of an eigenvalue of A^T A, so an eigenvalue carrying the usual ~eps
    // relative error surfaces as a singular value of about sqrt(eps)*s_max,
    // i.e. ~1e-8*s_max, not ~1e-16*s_max. The old absolute `1e-12` therefore
    // classified an exactly rank-deficient A as full rank and divided
    // A*v_c (~1e-17) by ~1e-8, yielding a ZERO column in U -- a silently
    // non-orthonormal "SVD". That is not cosmetic: it broke the essential
    // matrix decomposition, whose input is always rank 2, for ~46% of poses
    // (D25).
    const double sigTol = 1e-6 * r.s.x;

    Vec3 ucol[3]{};
    bool have[3] = {false, false, false};

    // Trusted directions, largest singular value first, Gram-Schmidt'd as we
    // go. The orthogonalization is not redundant: u_c = A v_c / s_c amplifies
    // rounding by s_max/s_c, so a direction just above the cutoff can come out
    // 1e-10 off orthogonal. Re-orthonormalizing costs nothing here and it is
    // what makes U an actual rotation for the callers that need one.
    for (int c = 0; c < 3; c++) {
        double sig = (&r.s.x)[c];
        if (sig <= sigTol) {
            (&r.s.x)[c] = 0.0;  // it is zero; report it as zero
            continue;
        }
        Vec3 u = mul(A, vcol[c]) * (1.0 / sig);
        for (int d = 0; d < c; d++)
            if (have[d]) u = u - ucol[d] * u.dot(ucol[d]);
        double n = u.norm();
        if (n > 1e-12) {
            ucol[c] = u * (1.0 / n);
            have[c] = true;
        } else {
            (&r.s.x)[c] = 0.0;  // numerically dependent on an earlier column
        }
    }
    // Complete the remaining directions with whichever axis is furthest from
    // the span of what we already have. Works at any rank, unlike the old
    // cross-product special case that assumed exactly rank 2.
    const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int c = 0; c < 3; c++) {
        if (have[c]) continue;
        Vec3 best{0, 0, 1};
        double bestNorm = -1;
        for (const Vec3& axis : axes) {
            Vec3 v = axis;
            for (int d = 0; d < 3; d++)
                if (have[d]) v = v - ucol[d] * v.dot(ucol[d]);
            double n = v.norm();
            if (n > bestNorm) { bestNorm = n; best = v; }
        }
        ucol[c] = bestNorm > 1e-12 ? best.normalized() : Vec3{0, 0, 1};
        have[c] = true;  // later completions stay orthogonal to it
    }
    for (int c = 0; c < 3; c++) {
        r.U[0 * 3 + c] = ucol[c].x;
        r.U[1 * 3 + c] = ucol[c].y;
        r.U[2 * 3 + c] = ucol[c].z;
    }
    return r;
}

}  // namespace sfm
