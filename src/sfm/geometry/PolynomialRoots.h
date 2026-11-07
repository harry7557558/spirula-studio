// Real roots by scaled Aberth-Ehrlich iteration, shared by minimal solvers.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sfm {

// Rescaling by the geometric mean of root magnitudes avoids the 7% root loss
// observed for generalized P3P when depth-polynomial coefficients spanned 10^10.
template <size_t N>
inline int realPolynomialRoots(const double (&coefficients)[N], int degree, double* out,
                               double imag_tol = 1e-4, int iterations = 40) {
    if (degree < 1 || degree >= static_cast<int>(N)) return 0;
    double amax = 0;
    for (int i = 0; i <= degree; i++) amax = std::max(amax, std::fabs(coefficients[i]));
    if (amax <= 0) return 0;
    int n = degree;
    while (n > 0 && std::fabs(coefficients[n]) < 1e-13 * amax) n--;
    if (n == 0) return 0;

    double a[N];
    for (int i = 0; i <= n; i++) a[i] = coefficients[i] / coefficients[n];
    double rho = std::pow(std::fabs(a[0]), 1.0 / n);
    if (!(rho > 1e-150) || !std::isfinite(rho)) {
        rho = 0;
        for (int k = 1; k <= n; k++)
            rho = std::max(rho, std::pow(std::fabs(a[n - k]), 1.0 / k));
        if (!(rho > 1e-150) || !std::isfinite(rho)) rho = 1;
    }
    for (int i = n - 1, p = 1; i >= 0; i--, p++) a[i] /= std::pow(rho, (double)p);

    auto evalReal = [&](double x) {
        double v = a[n];
        for (int i = n - 1; i >= 0; i--) v = v * x + a[i];
        return v;
    };
    auto evalRealD = [&](double x) {
        double v = n * a[n];
        for (int i = n - 1; i >= 1; i--) v = v * x + i * a[i];
        return v;
    };

    int found = 0;
    auto emit = [&](double x) {
        for (int it = 0; it < 12; it++) {
            const double d = evalRealD(x);
            if (std::fabs(d) < 1e-300) break;
            const double step = evalReal(x) / d;
            x -= step;
            if (std::fabs(step) < 1e-15 * (1.0 + std::fabs(x))) break;
        }
        x *= rho;
        for (int i = 0; i < found; i++)
            if (std::fabs(out[i] - x) < 1e-9 * (1.0 + std::fabs(x))) return;
        out[found++] = x;
    };

    if (n == 1) {
        emit(-a[0]);
        return found;
    }
    if (n == 2) {
        const double disc = a[1] * a[1] - 4 * a[0];
        if (disc < 0) return 0;
        const double s = std::sqrt(disc);
        emit(0.5 * (-a[1] + s));
        emit(0.5 * (-a[1] - s));
        return found;
    }

    double zr[N], zi[N];
    bool done[N];
    for (int k = 0; k < n; k++) {
        const double th = 2.0 * M_PI * k / n + 0.6;
        zr[k] = std::cos(th);
        zi[k] = std::sin(th);
        done[k] = false;
    }
    // Retiring roots one at a time, rather than waiting for the worst of them:
    // a near-double pair trades its last bit back and forth forever, and that
    // was 45% of samples running the whole iteration budget for nothing.
    for (int it = 0; it < iterations; it++) {
        int live = 0;
        for (int k = 0; k < n; k++) {
            if (done[k]) continue;
            live++;
            double vr = a[n], vi = 0, dr = 0, di = 0;
            for (int i = n - 1; i >= 0; i--) {
                const double ndr = dr * zr[k] - di * zi[k] + vr;
                di = dr * zi[k] + di * zr[k] + vi;
                dr = ndr;
                const double nvr = vr * zr[k] - vi * zi[k] + a[i];
                vi = vr * zi[k] + vi * zr[k];
                vr = nvr;
            }
            const double dn = dr * dr + di * di;
            if (dn < 1e-300) continue;
            const double qr = (vr * dr + vi * di) / dn, qi = (vi * dr - vr * di) / dn;
            double sr = 0, si = 0;
            for (int j = 0; j < n; j++) {
                if (j == k) continue;
                const double xr = zr[k] - zr[j], xi = zi[k] - zi[j];
                const double xn = xr * xr + xi * xi;
                if (xn < 1e-300) continue;
                sr += xr / xn;
                si -= xi / xn;
            }
            const double er = 1.0 - (qr * sr - qi * si), ei = -(qr * si + qi * sr);
            const double en = er * er + ei * ei;
            if (en < 1e-300) continue;
            const double wr = (qr * er + qi * ei) / en, wi = (qi * er - qr * ei) / en;
            zr[k] -= wr;
            zi[k] -= wi;
            if (std::fabs(wr) + std::fabs(wi) < 1e-13 * (1.0 + std::fabs(zr[k]))) done[k] = true;
        }
        if (!live) break;
    }
    for (int k = 0; k < n; k++)
        if (std::fabs(zi[k]) < imag_tol * (1.0 + std::fabs(zr[k]))) emit(zr[k]);
    return found;
}

} // namespace sfm
