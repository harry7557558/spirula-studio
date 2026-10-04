#include "app/FocusWeight.h"

#include "nn/core/Parallel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace app {
namespace {

constexpr std::array<double, 5> kScales = {1.0, 1.4, 2.0, 2.8, 4.0};
constexpr int kDetect = 2;   // kScales[kDetect] == 2.0
constexpr double kMinSnr = 4.0, kScaleSnr = 2.0, kMaxResidual = 0.12;

using Plane = std::vector<float>;

inline int reflect101(int i, int n) {
    if (n == 1) return 0;
    while (i < 0 || i >= n) i = i < 0 ? -i : 2 * n - 2 - i;
    return i;
}

// OpenCV's float Gaussian: ksize = round(8 sigma + 1) | 1, BORDER_REFLECT_101.
Plane gaussian(const Plane& src, int w, int h, double sigma) {
    const int k = ((int)std::lround(sigma * 8.0 + 1.0)) | 1, r = k / 2;
    std::vector<float> g((size_t)k);
    double sum = 0;
    for (int i = 0; i < k; ++i) sum += g[(size_t)i] = (float)std::exp(-(i - r) * (i - r) / (2 * sigma * sigma));
    for (float& v : g) v = (float)(v / sum);
    Plane tmp((size_t)w * h), out((size_t)w * h);
    nn::parallel_for(h, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y)
            for (int x = 0; x < w; ++x) {
                float s = 0;
                for (int i = 0; i < k; ++i) s += g[(size_t)i] * src[(size_t)y * w + reflect101(x + i - r, w)];
                tmp[(size_t)y * w + x] = s;
            }
    });
    nn::parallel_for(h, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y)
            for (int x = 0; x < w; ++x) {
                float s = 0;
                for (int i = 0; i < k; ++i) s += g[(size_t)i] * tmp[(size_t)reflect101((int)y + i - r, h) * w + x];
                out[(size_t)y * w + x] = s;
            }
    });
    return out;
}

// Scharr / 32 after a Gaussian of `sigma`: the smoothed image's gradient.
void gradient(const Plane& img, int w, int h, double sigma, Plane* gx, Plane* gy, Plane& energy) {
    const Plane sm = gaussian(img, w, h, sigma);
    energy.assign((size_t)w * h, 0.0f);
    if (gx) gx->assign((size_t)w * h, 0.0f);
    if (gy) gy->assign((size_t)w * h, 0.0f);
    nn::parallel_for(h, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y) {
            const int ym = reflect101((int)y - 1, h), yp = reflect101((int)y + 1, h);
            for (int x = 0; x < w; ++x) {
                const int xm = reflect101(x - 1, w), xp = reflect101(x + 1, w);
                auto at = [&](int yy, int xx) { return sm[(size_t)yy * w + xx]; };
                const float dx = (3 * (at(ym, xp) - at(ym, xm)) + 10 * (at((int)y, xp) - at((int)y, xm)) +
                                  3 * (at(yp, xp) - at(yp, xm))) / 32.0f;
                const float dy = (3 * (at(yp, xm) - at(ym, xm)) + 10 * (at(yp, x) - at(ym, x)) +
                                  3 * (at(yp, xp) - at(ym, xp))) / 32.0f;
                const size_t i = (size_t)y * w + x;
                energy[i] = dx * dx + dy * dy;
                if (gx) (*gx)[i] = dx;
                if (gy) (*gy)[i] = dy;
            }
        }
    });
}

// numpy's default (linear) percentile, q in 0..100.
double percentile(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    const double pos = q / 100.0 * (double)(v.size() - 1);
    const size_t lo = (size_t)std::floor(pos), hi = std::min(lo + 1, v.size() - 1);
    std::nth_element(v.begin(), v.begin() + (long)lo, v.end());
    const double a = v[lo];
    if (hi == lo) return a;
    const double b = *std::min_element(v.begin() + (long)lo + 1, v.end());
    return a + (b - a) * (pos - (double)lo);
}

double interp(double x, const std::vector<double>& xs, const std::vector<double>& ys) {
    if (x <= xs.front()) return ys.front();
    if (x >= xs.back()) return ys.back();
    const size_t i = (size_t)(std::upper_bound(xs.begin(), xs.end(), x) - xs.begin());
    const double t = (x - xs[i - 1]) / std::max(xs[i] - xs[i - 1], 1e-30);
    return ys[i - 1] + t * (ys[i] - ys[i - 1]);
}

// Expected squared-gradient noise per scale as a function of brightness, from
// the flattest blocks of the image itself -- so the correlated noise a JPEG
// pipeline leaves is what gets subtracted, not a white-noise model of it.
struct NoiseModel {
    std::vector<double> centers;
    std::array<std::vector<double>, kScales.size()> table;

    NoiseModel(const Plane& gray, const std::array<Plane, kScales.size()>& energy, int w, int h) {
        const int B = 32, hb = h / B, wb = w / B, n = hb * wb;
        auto block_mean = [&](const Plane& p) {
            std::vector<double> m((size_t)n, 0.0);
            for (int by = 0; by < hb; ++by)
                for (int y = by * B; y < by * B + B; ++y)
                    for (int bx = 0; bx < wb; ++bx) {
                        double s = 0;
                        for (int x = bx * B; x < bx * B + B; ++x) s += p[(size_t)y * w + x];
                        m[(size_t)by * wb + bx] += s;
                    }
            for (double& v : m) v /= (double)B * B;
            return m;
        };
        const std::vector<double> level = block_mean(gray);
        std::array<std::vector<double>, kScales.size()> eb;
        for (size_t s = 0; s < kScales.size(); ++s) eb[s] = block_mean(energy[s]);
        std::vector<double> edges;
        for (int i = 0; i <= 12; ++i) edges.push_back(percentile(level, 100.0 * i / 12));
        struct Row { double c; std::array<double, kScales.size()> t; };
        std::vector<Row> rows;
        for (int b = 0; b < 12; ++b) {
            std::vector<size_t> sel;
            for (size_t i = 0; i < level.size(); ++i)
                if (level[i] >= edges[(size_t)b] && level[i] <= edges[(size_t)b + 1]) sel.push_back(i);
            if (sel.size() < 6) continue;
            std::vector<double> coarse;
            for (size_t i : sel) coarse.push_back(eb.back()[i]);
            const double cut = percentile(coarse, 15);
            std::vector<double> lv;
            std::array<std::vector<double>, kScales.size()> ev;
            for (size_t i : sel)
                if (eb.back()[i] <= cut) {
                    lv.push_back(level[i]);
                    for (size_t s = 0; s < kScales.size(); ++s) ev[s].push_back(eb[s][i]);
                }
            Row r{percentile(lv, 50), {}};
            for (size_t s = 0; s < kScales.size(); ++s) r.t[s] = std::max(percentile(ev[s], 50), 1e-10);
            rows.push_back(r);
        }
        if (rows.empty()) rows.push_back({0.5, {1e-8, 1e-8, 1e-8, 1e-8, 1e-8}});
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.c < b.c; });
        // A brightness level with no flat block reads edge energy as noise. Noise
        // falls ~1/s^4 across scales and edges ~1/s^2, so a bin whose fine/coarse
        // ratio is not noise-like is replaced from its neighbours.
        std::vector<double> ratio;
        for (const Row& r : rows) ratio.push_back(r.t.front() / r.t.back());
        const double med = percentile(ratio, 50);
        std::vector<double> gc;
        std::array<std::vector<double>, kScales.size()> gt;
        for (size_t i = 0; i < rows.size(); ++i)
            if (ratio[i] >= 0.25 * med) {
                gc.push_back(rows[i].c);
                for (size_t s = 0; s < kScales.size(); ++s) gt[s].push_back(std::log(rows[i].t[s]));
            }
        for (size_t i = 0; i < rows.size(); ++i) {
            if (!gc.empty() && gc.size() < rows.size() && ratio[i] < 0.25 * med)
                for (size_t s = 0; s < kScales.size(); ++s) rows[i].t[s] = std::exp(interp(rows[i].c, gc, gt[s]));
            centers.push_back(rows[i].c);
            for (size_t s = 0; s < kScales.size(); ++s) table[s].push_back(rows[i].t[s]);
        }
    }
    double at(double level, size_t s) const { return interp(level, centers, table[s]); }
};

// cv2.resize(..., INTER_LINEAR): half-pixel centres, edge clamp, no prefilter.
Plane resize_linear(const Plane& src, int sw, int sh, int dw, int dh) {
    Plane out((size_t)dw * dh);
    const double fx = (double)sw / dw, fy = (double)sh / dh;
    nn::parallel_for(dh, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y) {
            double v = ((double)y + 0.5) * fy - 0.5;
            int ya = (int)std::floor(v);
            double ty = v - ya;
            if (ya < 0) { ya = 0; ty = 0; }
            if (ya >= sh - 1) { ya = sh - 1; ty = 0; }
            const int yb = std::min(ya + 1, sh - 1);
            for (int x = 0; x < dw; ++x) {
                double u = ((double)x + 0.5) * fx - 0.5;
                int xa = (int)std::floor(u);
                double tx = u - xa;
                if (xa < 0) { xa = 0; tx = 0; }
                if (xa >= sw - 1) { xa = sw - 1; tx = 0; }
                const int xb = std::min(xa + 1, sw - 1);
                const double a = src[(size_t)ya * sw + xa] * (1 - tx) + src[(size_t)ya * sw + xb] * tx;
                const double b = src[(size_t)yb * sw + xa] * (1 - tx) + src[(size_t)yb * sw + xb] * tx;
                out[(size_t)y * dw + x] = (float)(a * (1 - ty) + b * ty);
            }
        }
    });
    return out;
}

double weighted_quantile(std::vector<std::pair<float, float>>& vw, double q) {
    std::sort(vw.begin(), vw.end());
    double total = 0;
    for (const auto& p : vw) total += p.second;
    double c = 0;
    for (const auto& p : vw) {
        c += p.second;
        if (c >= q * total) return p.first;
    }
    return vw.back().first;
}

// Least squares with x >= 0 for at most three unknowns: the optimum is the
// unconstrained solution on its own positive support, so every support is tried.
std::array<double, 3> nnls3(const std::vector<std::array<double, 3>>& A, const std::vector<double>& b, int cols) {
    std::array<double, 3> best{0, 0, 0};
    double best_res = 0;
    for (double v : b) best_res += v * v;
    for (int mask = 1; mask < (1 << cols); ++mask) {
        int idx[3], m = 0;
        for (int c = 0; c < cols; ++c)
            if (mask & (1 << c)) idx[m++] = c;
        double M[3][3] = {}, r[3] = {};
        for (size_t i = 0; i < A.size(); ++i)
            for (int p = 0; p < m; ++p) {
                r[p] += A[i][(size_t)idx[p]] * b[i];
                for (int q = 0; q < m; ++q) M[p][q] += A[i][(size_t)idx[p]] * A[i][(size_t)idx[q]];
            }
        for (int p = 0; p < m; ++p) {
            int piv = p;
            for (int q = p + 1; q < m; ++q)
                if (std::fabs(M[q][p]) > std::fabs(M[piv][p])) piv = q;
            if (std::fabs(M[piv][p]) < 1e-18) { m = -1; break; }
            std::swap(M[p], M[piv]);
            std::swap(r[p], r[piv]);
            for (int q = p + 1; q < m; ++q) {
                const double f = M[q][p] / M[p][p];
                for (int k = p; k < m; ++k) M[q][k] -= f * M[p][k];
                r[q] -= f * r[p];
            }
        }
        if (m < 0) continue;
        double x[3] = {};
        bool feasible = true;
        for (int p = m - 1; p >= 0; --p) {
            double s = r[p];
            for (int k = p + 1; k < m; ++k) s -= M[p][k] * x[k];
            x[p] = s / M[p][p];
            if (!(x[p] >= 0)) feasible = false;
        }
        if (!feasible) continue;
        std::array<double, 3> cand{0, 0, 0};
        for (int p = 0; p < m; ++p) cand[(size_t)idx[p]] = x[p];
        double res = 0;
        for (size_t i = 0; i < A.size(); ++i) {
            double e = -b[i];
            for (int c = 0; c < cols; ++c) e += A[i][(size_t)c] * cand[(size_t)c];
            res += e * e;
        }
        if (res < best_res) { best_res = res; best = cand; }
    }
    return best;
}

double blur_at(const FocusCurve& c, double u) {
    const double d = u - c.u_f;
    const double n = c.k_near * std::max(d, 0.0), f = c.k_far * std::max(-d, 0.0);
    return std::sqrt(c.b0 * c.b0 + n * n + f * f);
}

}  // namespace

int focus_measure_factor(int width, int height) {
    return std::max(1, std::max(width, height) / 3000);
}

std::vector<float> gray_downsampled(const uint8_t* rgb, int w, int h, int f, int& ow, int& oh) {
    ow = std::max(1, w / f);
    oh = std::max(1, h / f);
    std::vector<float> out((size_t)ow * oh);
    const float norm = 1.0f / (255.0f * (float)(f * f));
    nn::parallel_for(oh, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y)
            for (int x = 0; x < ow; ++x) {
                float s = 0;
                for (int dy = 0; dy < f; ++dy) {
                    const uint8_t* p = rgb + (((size_t)y * f + dy) * w + (size_t)x * f) * 3;
                    for (int dx = 0; dx < f; ++dx, p += 3) s += 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
                }
                out[(size_t)y * ow + x] = s * norm;
            }
    });
    return out;
}

// Step edge blurred by sigma, re-smoothed by s: 1/m2(s) is linear in s^2 with
// intercept/slope = sigma^2, whatever the contrast. Measured noise energy is
// subtracted; only scales the edge clears by 2x in amplitude enter the fit.
FocusEdges measure_edge_blur(const Plane& gray, int w, int h) {
    std::array<Plane, kScales.size()> energy;
    Plane gx, gy;
    for (size_t s = 0; s < kScales.size(); ++s)
        gradient(gray, w, h, kScales[s], s == (size_t)kDetect ? &gx : nullptr,
                 s == (size_t)kDetect ? &gy : nullptr, energy[s]);
    const NoiseModel noise(gray, energy, w, h);
    const Plane level = gaussian(gray, w, h, 4.0);
    const Plane& e_det = energy[kDetect];

    FocusEdges out;
    const int pad = 12;
    static const int kOff[4][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}};
    const double s2[5] = {1.0, 1.96, 4.0, 7.84, 16.0};
    for (int y = pad; y < h - pad; ++y)
        for (int x = pad; x < w - pad; ++x) {
            const size_t i = (size_t)y * w + x;
            const double det_noise = noise.at(level[i], kDetect);
            if (!(e_det[i] > kMinSnr * kMinSnr * det_noise)) continue;
            double ang = std::atan2((double)gy[i], (double)gx[i]) * 57.29577951308232 + 180.0;
            ang = std::fmod(ang, 180.0);
            const int q = ((int)std::floor((ang + 22.5) / 45.0)) % 4;
            const float mag = std::sqrt(e_det[i]);
            const int dy = kOff[q][0], dx = kOff[q][1];
            const float a = std::sqrt(e_det[(size_t)(y - dy) * w + (x - dx)]);
            const float b = std::sqrt(e_det[(size_t)(y + dy) * w + (x + dx)]);
            if (!(mag >= a && mag > b)) continue;

            double sw = 0, swx = 0, swy = 0, swxx = 0, swxy = 0, yv[5], n2[5];
            bool valid[5];
            int count = 0, first = -1;
            for (int s = 0; s < 5; ++s) {
                n2[s] = noise.at(level[i], (size_t)s);
                const double sig = (double)energy[(size_t)s][i] - n2[s];
                valid[s] = sig > (kScaleSnr * kScaleSnr - 1.0) * n2[s];
                yv[s] = 1.0 / std::max(sig, 1e-12);
                if (!valid[s]) continue;
                if (first < 0) first = s;
                ++count;
                const double rel = n2[s] / std::max(sig, 1e-12) + 0.02;
                const double wt = 1.0 / std::max(rel * rel * yv[s] * yv[s], 1e-30);
                sw += wt; swx += wt * s2[s]; swy += wt * yv[s];
                swxx += wt * s2[s] * s2[s]; swxy += wt * s2[s] * yv[s];
            }
            if (count < 3) continue;
            const double det = sw * swxx - swx * swx;
            const double slope = (sw * swxy - swx * swy) / det;
            const double intercept = (swy - slope * swx) / sw;
            double res = 0;
            for (int s = 0; s < 5; ++s)
                if (valid[s]) {
                    const double fit = intercept + slope * s2[s];
                    res += ((yv[s] - fit) / fit) * ((yv[s] - fit) / fit);
                }
            res = std::sqrt(res / count);
            const double sigma2 = intercept / slope;
            if (!std::isfinite(sigma2) || !(slope > 0) || !(res < kMaxResidual) || !(sigma2 > -0.6)) continue;
            const double snr = std::sqrt(e_det[i] / std::max(det_noise, 1e-12));
            const double weight = std::clamp((snr - kMinSnr) / (3 * kMinSnr), 0.05, 1.0) *
                                  std::clamp(1.0 - res / kMaxResidual, 0.0, 1.0) * (first == 0 ? 1.0 : 0.6);
            if (!(weight > 0)) continue;
            out.y.push_back(y);
            out.x.push_back(x);
            out.sigma.push_back((float)std::sqrt(std::max(sigma2, 0.0)));
            out.weight.push_back((float)weight);
        }
    return out;
}

FocusCurve fit_focus_curve(const FocusEdges& e, int mw, int mh, const Plane& inv_depth, int dw, int dh) {
    FocusCurve c;
    Plane valid((size_t)dw * dh);
    for (size_t i = 0; i < valid.size(); ++i) valid[i] = inv_depth[i] > 0 ? 1.0f : 0.0f;
    const Plane um = resize_linear(inv_depth, dw, dh, mw, mh);
    const Plane vm = resize_linear(valid, dw, dh, mw, mh);
    std::vector<double> vals;
    for (size_t i = 0; i < um.size(); ++i)
        if (vm[i] > 0.99f) vals.push_back(um[i]);
    if (vals.size() < 100) return c;
    c.u_lo = percentile(vals, 0.5);
    c.u_hi = percentile(vals, 99.5);
    const double span = std::max(c.u_hi - c.u_lo, 1e-12);

    // Edges on a depth discontinuity mix two depths and stay out of the fit.
    const int R = 3;
    constexpr int kBins = 64;
    std::vector<std::vector<std::pair<float, float>>> slice(kBins);
    for (size_t k = 0; k < e.y.size(); ++k) {
        const int y = e.y[k], x = e.x[k];
        if (!(vm[(size_t)y * mw + x] > 0.99f)) continue;
        float lo = 1e30f, hi = -1e30f;
        for (int yy = std::max(0, y - R); yy <= std::min(mh - 1, y + R); ++yy)
            for (int xx = std::max(0, x - R); xx <= std::min(mw - 1, x + R); ++xx) {
                lo = std::min(lo, um[(size_t)yy * mw + xx]);
                hi = std::max(hi, um[(size_t)yy * mw + xx]);
            }
        if (!(hi - lo < 0.03 * span)) continue;
        const double t = (um[(size_t)y * mw + x] - c.u_lo) / span;
        const int b = std::clamp((int)(t * kBins), 0, kBins - 1);
        slice[(size_t)b].push_back({e.sigma[k], e.weight[k]});
        ++c.edges_used;
    }
    // The sharpest quarter of a slice's edges bounds the optics there; soft
    // content (shading, rounded surfaces) only ever adds blur.
    std::vector<double> xs, ys, mass;
    for (int b = 0; b < kBins; ++b) {
        double m = 0;
        for (const auto& p : slice[(size_t)b]) m += p.second;
        if (m < 20.0) continue;
        xs.push_back(c.u_lo + (b + 0.5) / kBins * span);
        ys.push_back(weighted_quantile(slice[(size_t)b], 0.25));
        mass.push_back(m);
    }
    if (xs.size() < 4) return c;

    // The estimator saturates above ~2 px (coarsest scale 4 px): far slices form
    // a plateau a V cannot follow, which widened the fitted bottom and pulled
    // u_f off the minimum (DSC07690-like scenes), so they are down-weighted.
    std::vector<double> base(xs.size());
    for (size_t i = 0; i < xs.size(); ++i) base[i] = std::sqrt(mass[i]) / (1.0 + std::pow(ys[i] / 1.9, 8.0));
    const double huber = 0.35;
    double best = 1e300;
    const double x0 = *std::min_element(xs.begin(), xs.end()), x1 = *std::max_element(xs.begin(), xs.end());
    for (int g = 0; g < 241; ++g) {
        const double uf = x0 + (x1 - x0) * g / 240.0;
        std::vector<std::array<double, 3>> A(xs.size());
        for (size_t i = 0; i < xs.size(); ++i) {
            const double d = xs[i] - uf;
            A[i] = {1.0, std::max(d, 0.0) * std::max(d, 0.0), std::max(-d, 0.0) * std::max(-d, 0.0)};
        }
        std::vector<double> w = base, pred(xs.size()), r(xs.size());
        for (size_t i = 0; i < xs.size(); ++i) pred[i] = std::max(ys[i], 0.3);
        std::array<double, 3> coef{};
        for (int it = 0; it < 4; ++it) {
            std::vector<std::array<double, 3>> Aw(xs.size());
            std::vector<double> bw(xs.size());
            for (size_t i = 0; i < xs.size(); ++i) {
                // b^2 residuals linearized back to b units: d(b^2) = 2 b db.
                const double sw = std::sqrt(w[i]) / (2.0 * std::max(pred[i], 0.3));
                for (int k = 0; k < 3; ++k) Aw[i][(size_t)k] = A[i][(size_t)k] * sw;
                bw[i] = ys[i] * ys[i] * sw;
            }
            coef = nnls3(Aw, bw, 3);
            for (size_t i = 0; i < xs.size(); ++i) {
                pred[i] = std::sqrt(std::max(A[i][0] * coef[0] + A[i][1] * coef[1] + A[i][2] * coef[2], 1e-12));
                r[i] = std::fabs(pred[i] - ys[i]);
                w[i] = base[i] * (r[i] <= huber ? 1.0 : huber / std::max(r[i], 1e-12));
            }
        }
        double loss = 0, sw = 0, sr = 0;
        for (size_t i = 0; i < xs.size(); ++i) {
            loss += base[i] * (r[i] <= huber ? 0.5 * r[i] * r[i] : huber * (r[i] - 0.5 * huber));
            sw += base[i];
            sr += base[i] * r[i] * r[i];
        }
        if (loss < best) {
            best = loss;
            c.u_f = uf;
            c.b0 = std::sqrt(coef[0]);
            c.k_near = std::sqrt(coef[1]);
            c.k_far = std::sqrt(coef[2]);
            c.rms = std::sqrt(sr / std::max(sw, 1e-30));
        }
    }
    c.ok = true;
    return c;
}

std::vector<uint8_t> render_focus_weight(const FocusCurve& c, const Plane& inv_depth, int dw, int dh, int mw,
                                         int mh, int fw, int fh, const FocusSettings& s) {
    const double to_train = std::min((double)s.train_side, (double)std::max(fw, fh)) / std::max(mw, mh);
    const double top = 0.5 + 0.5 * std::tanh(1.5 / s.softness);
    Plane wgt((size_t)dw * dh, 0.0f);
    for (size_t i = 0; i < wgt.size(); ++i) {
        if (!(inv_depth[i] > 0)) continue;
        const double b = blur_at(c, inv_depth[i]);
        const double d = std::sqrt(std::max(b * b - c.b0 * c.b0, 0.0)) * to_train;
        const double t = (d / std::max((double)s.allowed_px, 1e-6) - 1.0) / s.softness;
        wgt[i] = (float)std::clamp((0.5 - 0.5 * std::tanh(1.5 * t)) / top, 0.0, 1.0);
    }
    const Plane full = resize_linear(wgt, dw, dh, fw, fh);
    std::vector<uint8_t> out(full.size());
    for (size_t i = 0; i < full.size(); ++i) out[i] = (uint8_t)std::clamp(full[i] * 255.0f + 0.5f, 0.0f, 255.0f);
    return out;
}

}  // namespace app
