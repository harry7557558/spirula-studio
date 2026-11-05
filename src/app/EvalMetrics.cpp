#include "app/EvalMetrics.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace spirula {

namespace {

int max_threads() {
#ifdef _OPENMP
    return std::max(1, omp_get_max_threads());
#else
    return 1;
#endif
}

int this_thread() {
#ifdef _OPENMP
    return omp_get_thread_num();
#else
    return 0;
#endif
}

// Sum in thread order, so the result does not depend on who finished first.
double combine(const std::vector<double>& partials) {
    double s = 0.0;
    for (double v : partials) s += v;
    return s;
}

}  // namespace

float image_l1(const float* a, const float* b, int64_t n, const uint8_t* mask, int C) {
    int64_t valid = n;
    if (mask) {
        valid = 0;
        for (int64_t i = 0; i < n; ++i) valid += mask[i / C] != 0;
    }
    std::vector<double> part((size_t)max_threads(), 0.0);
#pragma omp parallel
    {
        double s = 0.0;
#pragma omp for schedule(static) nowait
        for (int64_t i = 0; i < n; i++)
            if (!mask || mask[i / C]) s += std::abs((double)a[i] - (double)b[i]);
        part[(size_t)this_thread()] = s;
    }
    return (float)(combine(part) / (double)std::max<int64_t>(valid, 1));
}

float image_psnr(const float* a, const float* b, int64_t n, const uint8_t* mask, int C) {
    int64_t valid = n;
    if (mask) {
        valid = 0;
        for (int64_t i = 0; i < n; ++i) valid += mask[i / C] != 0;
    }
    std::vector<double> part((size_t)max_threads(), 0.0);
#pragma omp parallel
    {
        double s = 0.0;
#pragma omp for schedule(static) nowait
        for (int64_t i = 0; i < n; i++) {
            if (mask && !mask[i / C]) continue;
            double d = (double)a[i] - (double)b[i];
            s += d * d;
        }
        part[(size_t)this_thread()] = s;
    }
    double mse = combine(part) / (double)std::max<int64_t>(valid, 1);
    if (mse <= 0.0) return std::numeric_limits<float>::infinity();
    return (float)(10.0 * std::log10(1.0 / mse));   // data_range = 1
}

namespace {

// torchmetrics' SSIM constants for data_range = 1.
constexpr double kK1 = 0.01, kK2 = 0.03;
constexpr int    kWin = 11;
constexpr double kSigma = 1.5;

// Separable gaussian, normalized -- torchmetrics builds the 2D window as the
// outer product of this with itself.
std::vector<double> gaussian_window() {
    std::vector<double> g(kWin);
    const int half = kWin / 2;
    double sum = 0.0;
    for (int i = 0; i < kWin; i++) {
        double x = (double)(i - half);
        g[i] = std::exp(-(x * x) / (2.0 * kSigma * kSigma));
        sum += g[i];
    }
    for (double& v : g) v /= sum;
    return g;
}

// torch's mode="reflect": index -1 maps to 1, not to 0 (the border pixel is
// not repeated).
int reflect(int i, int n) {
    if (i < 0) return -i;
    if (i >= n) return 2 * (n - 1) - i;
    return i;
}

// Separable gaussian blur of one channel with reflect padding: [H, W] -> [H, W].
// Each pass splits the reflecting border off the interior so the hot loop is a
// straight dot product. `dst` and `tmp` must already be H*W long.
void conv_reflect(const double* src, int H, int W, const double* g,
                  double* dst, double* tmp) {
    const int p = kWin / 2;

#pragma omp parallel for schedule(static)
    for (int y = 0; y < H; y++) {
        const double* s = src + (size_t)y * W;
        double* d = tmp + (size_t)y * W;
        const int lo = std::min(p, W), hi = std::max(p, W - p);
        for (int x = 0; x < lo; x++) {
            double acc = 0.0;
            for (int k = 0; k < kWin; k++) acc += s[reflect(x + k - p, W)] * g[k];
            d[x] = acc;
        }
        for (int x = lo; x < hi; x++) {       // interior: no reflection
            double acc = 0.0;
            for (int k = 0; k < kWin; k++) acc += s[x + k - p] * g[k];
            d[x] = acc;
        }
        for (int x = hi; x < W; x++) {
            double acc = 0.0;
            for (int k = 0; k < kWin; k++) acc += s[reflect(x + k - p, W)] * g[k];
            d[x] = acc;
        }
    }

#pragma omp parallel for schedule(static)
    for (int y = 0; y < H; y++) {
        double* d = dst + (size_t)y * W;
        if (y >= p && y < H - p) {            // interior row: no reflection
            for (int x = 0; x < W; x++) {
                double acc = 0.0;
                for (int k = 0; k < kWin; k++)
                    acc += tmp[(size_t)(y + k - p) * W + x] * g[k];
                d[x] = acc;
            }
        } else {
            for (int x = 0; x < W; x++) {
                double acc = 0.0;
                for (int k = 0; k < kWin; k++)
                    acc += tmp[(size_t)reflect(y + k - p, H) * W + x] * g[k];
                d[x] = acc;
            }
        }
    }
}

}  // namespace

float image_ssim(const float* a, const float* b, int H, int W, int C, const uint8_t* mask) {
    if (H < kWin || W < kWin)
        throw std::runtime_error("image_ssim: image smaller than the 11x11 window");
    const std::vector<double> gw = gaussian_window();
    const double* g = gw.data();
    const double C1 = (kK1 * 1.0) * (kK1 * 1.0);
    const double C2 = (kK2 * 1.0) * (kK2 * 1.0);
    const size_t np = (size_t)H * W;

    // Reuse the slab to avoid repeated mmap/page-zeroing at 4K. Only the local
    // pointers may be named inside OpenMP regions: thread_local resolves per worker.
    thread_local std::vector<double> slab;
    if (slab.size() < 12 * np) slab.resize(12 * np);
    double* x   = slab.data();
    double* y   = x + np;
    double* xx  = y + np;
    double* yy  = xx + np;
    double* xy  = yy + np;
    double* mx  = xy + np;
    double* my  = mx + np;
    double* mxx = my + np;
    double* myy = mxx + np;
    double* mxy = myy + np;
    double* tmp = mxy + np;
    double* support = tmp + np;
    int64_t valid = np;
    if (mask) {
        valid = 0;
        for (size_t i = 0; i < np; ++i) {
            x[i] = mask[i] ? 1.0 : 0.0;
            valid += mask[i] != 0;
        }
        conv_reflect(x, H, W, g, support, tmp);
    }
    std::vector<double> part((size_t)max_threads());

    double total = 0.0;
    for (int c = 0; c < C; c++) {
#pragma omp parallel for schedule(static)
        for (int64_t p = 0; p < (int64_t)np; p++) {
            double va = a[p * C + c], vb = b[p * C + c];
            if (mask && !mask[p]) va = vb = 0.0;
            x[p] = va;  y[p] = vb;
            xx[p] = va * va;
            yy[p] = vb * vb;
            xy[p] = va * vb;
        }
        conv_reflect(x,  H, W, g, mx,  tmp);
        conv_reflect(y,  H, W, g, my,  tmp);
        conv_reflect(xx, H, W, g, mxx, tmp);
        conv_reflect(yy, H, W, g, myy, tmp);
        conv_reflect(xy, H, W, g, mxy, tmp);

        std::fill(part.begin(), part.end(), 0.0);
#pragma omp parallel
        {
            double acc = 0.0;
#pragma omp for schedule(static) nowait
            for (int64_t i = 0; i < (int64_t)np; i++) {
                if (mask && !mask[i]) continue;
                double w = mask ? 1.0 / std::max(support[i], 1e-30) : 1.0;
                double ux = mx[i] * w, uy = my[i] * w;
                double sxx = std::max(mxx[i] * w - ux * ux, 0.0);
                double syy = std::max(myy[i] * w - uy * uy, 0.0);
                double sxy = mxy[i] * w - ux * uy;
                double num = (2.0 * ux * uy + C1) * (2.0 * sxy + C2);
                double den = (ux * ux + uy * uy + C1) * (sxx + syy + C2);
                acc += num / den;
            }
            part[(size_t)this_thread()] = acc;
        }
        total += combine(part) / (double)std::max<int64_t>(valid, 1);
    }
    return (float)(total / (double)C);
}


// --- colour correction -------------------------------------------------------
//
// The design matrix per pixel is [quadratic terms | linear | bias]:
//   c=0 -> r*r, r*g, r*b ; c=1 -> g*g, g*b ; c=2 -> b*b   (upper triangle)
//   then r, g, b, then 1.
// For C=3 that is 10 columns, so each channel solve is a 10x10 normal-equation
// system -- small enough for a plain Cholesky in double.
//
// All C channels are accumulated in ONE pass over the pixels: the design row
// depends only on the pixel, so building it per channel (as the first version
// did) repeated two thirds of the work.

namespace {

int design_cols(int C) { return C * (C + 1) / 2 + C + 1; }

void build_row(const float* px, int C, double* row) {
    int k = 0;
    for (int c = 0; c < C; c++)
        for (int d = c; d < C; d++) row[k++] = (double)px[c] * (double)px[d];
    for (int c = 0; c < C; c++) row[k++] = (double)px[c];
    row[k++] = 1.0;
}

// Solve (A^T A) w = A^T b for a symmetric positive-(semi)definite normal
// matrix. Returns false if the system is singular, which happens when a
// channel's mask leaves too few unsaturated pixels.
//
// Degeneracy is judged RELATIVE to the original diagonal. An absolute
// `pivot <= 0` test is a hair-trigger: on a rank-deficient system the pivot
// lands within an ulp of zero, so which side it falls on depends on the
// summation order -- i.e. on the thread count, which is how this surfaced. A
// relative cut decides the same way every time, and a pivot that small
// carries no information to lose.
bool solve_spd(std::vector<double>& M, std::vector<double>& v, int n) {
    std::vector<double> tol((size_t)n);
    for (int i = 0; i < n; i++) tol[(size_t)i] = 1e-12 * M[(size_t)i * n + i];

    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double s = M[(size_t)i * n + j];
            for (int k = 0; k < j; k++)
                s -= M[(size_t)i * n + k] * M[(size_t)j * n + k];
            if (i == j) {
                if (!(s > tol[(size_t)i])) return false;   // also catches NaN
                M[(size_t)i * n + j] = std::sqrt(s);
            } else {
                M[(size_t)i * n + j] = s / M[(size_t)j * n + j];
            }
        }
    }
    for (int i = 0; i < n; i++) {                     // forward
        double s = v[i];
        for (int k = 0; k < i; k++) s -= M[(size_t)i * n + k] * v[k];
        v[i] = s / M[(size_t)i * n + i];
    }
    for (int i = n - 1; i >= 0; i--) {                // back
        double s = v[i];
        for (int k = i + 1; k < n; k++) s -= M[(size_t)k * n + i] * v[k];
        v[i] = s / M[(size_t)i * n + i];
    }
    return true;
}

}  // namespace

std::vector<float> color_correct(const float* img, const float* ref,
                                 int64_t num_pixels, int C,
                                 int num_iters, float eps) {
    std::vector<float> out;
    color_correct_into(img, ref, num_pixels, C, out, num_iters, eps);
    return out;
}

void color_correct_into(const float* img, const float* ref,
                        int64_t num_pixels, int C, std::vector<float>& out,
                        int num_iters, float eps, const uint8_t* valid_mask) {
    const int n = design_cols(C);
    const size_t nv = (size_t)num_pixels * C;
    const double lo = (double)eps, hi = 1.0 - (double)eps;
    auto unclipped = [&](double z) { return z >= lo && z <= hi; };

    out.resize(nv);
    thread_local std::vector<float> scratch;
    thread_local std::vector<uint8_t> mask0;
    if (scratch.size() < nv) scratch.resize(nv);
    if (mask0.size() < nv) mask0.resize(nv);

    float* cur  = out.data();
    float* next = scratch.data();
    std::memcpy(cur, img, nv * sizeof(float));

    // Saturation of the ORIGINAL input, fixed across iterations (mask0).
    uint8_t* mask = mask0.data();
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < num_pixels * C; i++)
        mask[(size_t)i] = unclipped((double)img[i]) ? 1 : 0;

    const int T = max_threads();
    const size_t per_thread = (size_t)C * ((size_t)n * n + n);
    std::vector<double> acc((size_t)T * per_thread);

    std::vector<double> warp((size_t)n * C);
    std::vector<double> M((size_t)n * n), v((size_t)n);

    for (int it = 0; it < num_iters; it++) {
        std::fill(acc.begin(), acc.end(), 0.0);

        // One pass over the pixels fills every channel's normal equations.
#pragma omp parallel
        {
            double* mine = acc.data() + (size_t)this_thread() * per_thread;
            std::vector<double> row((size_t)n);
#pragma omp for schedule(static) nowait
            for (int64_t p = 0; p < num_pixels; p++) {
                if (valid_mask && !valid_mask[p]) continue;
                const float* px = &cur[(size_t)p * C];
                // Rows saturated in the input, in the current estimate, or in
                // the reference contribute nothing.
                unsigned live = 0;
                for (int c = 0; c < C; c++)
                    if (mask[(size_t)p * C + c] && unclipped((double)px[c]) &&
                        unclipped((double)ref[(size_t)p * C + c]))
                        live |= 1u << c;
                if (!live) continue;

                build_row(px, C, row.data());
                const double* r = row.data();
                for (int c = 0; c < C; c++) {
                    if (!(live & (1u << c))) continue;
                    double* Mc = mine + (size_t)c * ((size_t)n * n + n);
                    double* vc = Mc + (size_t)n * n;
                    const double b = (double)ref[(size_t)p * C + c];
                    for (int i = 0; i < n; i++) {
                        const double ri = r[i];
                        double* Mi = Mc + (size_t)i * n;
                        for (int j = 0; j <= i; j++) Mi[j] += ri * r[j];
                        vc[i] += ri * b;
                    }
                }
            }
        }

        for (int c = 0; c < C; c++) {
            std::fill(M.begin(), M.end(), 0.0);
            std::fill(v.begin(), v.end(), 0.0);
            for (int t = 0; t < T; t++) {          // thread order: reproducible
                const double* Mc = acc.data() + (size_t)t * per_thread
                                 + (size_t)c * ((size_t)n * n + n);
                const double* vc = Mc + (size_t)n * n;
                for (int i = 0; i < n; i++) {
                    for (int j = 0; j <= i; j++) M[(size_t)i * n + j] += Mc[(size_t)i * n + j];
                    v[i] += vc[i];
                }
            }
            for (int i = 0; i < n; i++)                 // mirror to full
                for (int j = i + 1; j < n; j++)
                    M[(size_t)i * n + j] = M[(size_t)j * n + i];
            if (!solve_spd(M, v, n)) {
                // Degenerate channel: leave it untouched (identity warp).
                std::fill(v.begin(), v.end(), 0.0);
                v[(size_t)(C * (C + 1) / 2 + c)] = 1.0;
            }
            for (int i = 0; i < n; i++) warp[(size_t)i * C + c] = v[i];
        }

        // Apply the warp to update the running estimate.
#pragma omp parallel
        {
            std::vector<double> row((size_t)n);
#pragma omp for schedule(static)
            for (int64_t p = 0; p < num_pixels; p++) {
                build_row(&cur[(size_t)p * C], C, row.data());
                for (int c = 0; c < C; c++) {
                    double s = 0.0;
                    for (int i = 0; i < n; i++) s += row[(size_t)i] * warp[(size_t)i * C + c];
                    next[(size_t)p * C + c] = (float)std::min(std::max(s, 0.0), 1.0);
                }
            }
        }
        std::swap(cur, next);
    }
    // An odd iteration count leaves the answer in the scratch half.
    if (cur != out.data()) std::memcpy(out.data(), cur, nv * sizeof(float));
}

}  // namespace spirula
