// Calibrated five-ray relative pose by cubic elimination and a degree-ten resultant.
#pragma once

#include <array>
#include <vector>

#include "sfm/geometry/Essential.h"
#include "sfm/geometry/PolynomialRoots.h"

namespace sfm {
namespace five_point_detail {

inline constexpr int powers[20][3] = {
    {3,0,0}, {0,3,0}, {2,1,0}, {1,2,0}, {2,0,1}, {2,0,0}, {1,1,1}, {1,1,0},
    {0,2,1}, {0,2,0}, {1,0,2}, {1,0,1}, {1,0,0}, {0,1,2}, {0,1,1}, {0,1,0},
    {0,0,3}, {0,0,2}, {0,0,1}, {0,0,0}
};

inline const std::array<int, 64>& productIndices() {
    static const std::array<int, 64> indices = [] {
        std::array<int, 64> result{};
        for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 4; ++b)
                for (int c = 0; c < 4; ++c) {
                    const int p[3] = {(a == 0) + (b == 0) + (c == 0),
                                      (a == 1) + (b == 1) + (c == 1),
                                      (a == 2) + (b == 2) + (c == 2)};
                    for (int m = 0; m < 20; ++m)
                        if (p[0] == powers[m][0] && p[1] == powers[m][1] && p[2] == powers[m][2])
                            result[16 * a + 4 * b + c] = m;
                }
        return result;
    }();
    return indices;
}

inline void addProduct(double* row, const double* a, const double* b, const double* c,
                       double factor) {
    const auto& indices = productIndices();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const double v = factor * a[i] * b[j];
            for (int k = 0; k < 4; ++k) row[indices[16 * i + 4 * j + k]] += v * c[k];
        }
}

inline bool eliminate(double (&a)[10][20]) {
    for (int r = 0; r < 10; ++r) {
        double scale = 0;
        for (double x : a[r]) scale = std::max(scale, std::fabs(x));
        if (!(scale > 0)) return false;
        for (double& x : a[r]) x /= scale;
    }
    for (int c = 0; c < 10; ++c) {
        int pivot = c;
        for (int r = c + 1; r < 10; ++r)
            if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) pivot = r;
        if (!(std::fabs(a[pivot][c]) > 1e-12)) return false;
        if (pivot != c) for (int j = c; j < 20; ++j) std::swap(a[c][j], a[pivot][j]);
        const double inv = 1.0 / a[c][c];
        for (int j = c; j < 20; ++j) a[c][j] *= inv;
        for (int r = 0; r < 10; ++r) {
            if (r == c) continue;
            const double value = a[r][c];
            for (int j = c; j < 20; ++j) a[r][j] -= value * a[c][j];
        }
    }
    return true;
}

inline double eval(const double* coefficients, int degree, double x) {
    double value = coefficients[degree];
    for (int i = degree - 1; i >= 0; --i) value = value * x + coefficients[i];
    return value;
}

} // namespace five_point_detail

inline std::vector<Mat3> estimateEssential5Bearing(const std::vector<Vec3>& b1,
                                                  const std::vector<Vec3>& b2,
                                                  const std::vector<int>& sample) {
    using namespace five_point_detail;
    std::vector<Mat3> models;
    if (sample.size() < 5) return models;
    std::vector<double> q(9 * sample.size());
    for (size_t i = 0; i < sample.size(); ++i) {
        const Vec3& a = b1[sample[i]];
        const Vec3& b = b2[sample[i]];
        const double row[9] = {b.x*a.x, b.x*a.y, b.x*a.z, b.y*a.x, b.y*a.y, b.y*a.z,
                               b.z*a.x, b.z*a.y, b.z*a.z};
        std::copy(row, row + 9, q.begin() + 9 * i);
    }
    const auto basis = nullVectors9(q, (int)sample.size(), 4);
    double e[9][4];
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 4; ++j) e[i][j] = basis[j][i];
    double a[10][20] = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double* row = a[3 * i + j];
            for (int k = 0; k < 3; ++k)
                for (int l = 0; l < 3; ++l)
                    addProduct(row, e[3*i+l], e[3*k+l], e[3*k+j], 2);
            for (int k = 0; k < 9; ++k) addProduct(row, e[k], e[k], e[3*i+j], -1);
        }
    for (int j = 0; j < 3; ++j) {
        addProduct(a[9], e[j], e[3+(j+1)%3], e[6+(j+2)%3], 1);
        addProduct(a[9], e[j], e[3+(j+2)%3], e[6+(j+1)%3], -1);
    }
    if (!eliminate(a)) return models;

    // x²z = z*x², xyz = z*xy, y²z = z*y² leave three equations linear in x,y.
    double b[3][3][5] = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 10; ++j) {
            const int column = j < 3 ? 0 : j < 6 ? 1 : 2;
            const int power = j < 3 ? 2 - j : j < 6 ? 5 - j : 9 - j;
            b[i][column][power] += a[4 + 2*i][10+j];
            b[i][column][power+1] -= a[5 + 2*i][10+j];
        }
    double polynomial[11] = {};
    for (int j = 0; j < 3; ++j)
        for (int sign : {1, -1}) {
            const int k = (j + (sign > 0 ? 1 : 2)) % 3;
            const int l = (j + (sign > 0 ? 2 : 1)) % 3;
            for (int p = 0; p <= (j == 2 ? 4 : 3); ++p)
                for (int r = 0; r <= (k == 2 ? 4 : 3); ++r)
                    for (int s = 0; s <= (l == 2 ? 4 : 3); ++s)
                        polynomial[p+r+s] += sign * b[0][j][p] * b[1][k][r] * b[2][l][s];
        }
    double roots[10];
    const int count = realPolynomialRoots(polynomial, 10, roots, 1e-7, 80);
    for (int r = 0; r < count; ++r) {
        Mat3 bz;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) bz[3*i+j] = eval(b[i][j], j == 2 ? 4 : 3, roots[r]);
        const Svd3 svd = svd3(bz);
        const Vec3 xy{svd.V[2], svd.V[5], svd.V[8]};
        if (!(std::fabs(xy.z) > 1e-10)) continue;
        Mat3 essential;
        double norm = 0;
        for (int i = 0; i < 9; ++i) {
            essential[i] = basis[0][i] * (xy.x / xy.z) + basis[1][i] * (xy.y / xy.z) +
                           basis[2][i] * roots[r] + basis[3][i];
            norm += essential[i] * essential[i];
        }
        if (!(norm > 0) || !std::isfinite(norm)) continue;
        for (double& v : essential) v /= std::sqrt(norm);
        const Mat3 gram = mul(essential, transpose(essential));
        const Mat3 cubic = mul(gram, essential);
        const double trace = gram[0] + gram[4] + gram[8];
        double residual = 0;
        for (int i = 0; i < 9; ++i) residual += std::pow(2*cubic[i] - trace*essential[i], 2);
        if (residual > 1e-10 || !std::isfinite(residual)) continue;
        int positive = 0;
        recoverRelativePoseBearing(b1, b2, sample, essential, positive);
        if (positive == (int)sample.size()) models.push_back(essential);
    }
    return models;
}

} // namespace sfm
