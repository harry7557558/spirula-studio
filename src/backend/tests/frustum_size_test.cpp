// Host-only check of camhost::frustum_display_size: the ball average against
// quadrature, the ~1% screen budget it promises, 1/sqrt(N) scaling, unit
// invariance, bracket merging, stray cameras and the degenerate fallbacks.
// No GPU. Exit code 0 = every check passed.

#include "data/FrustumSize.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_fail = 0;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::fprintf(stderr, "  FAIL: " __VA_ARGS__);                  \
            std::fprintf(stderr, "\n");                                    \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

constexpr double kPi = 3.14159265358979323846;

struct Cloud {
    std::vector<float> c2w;   // [n, 3, 4], translation column only
    int64_t n = 0;
    void add(double x, double y, double z) {
        float row[12] = {0, 0, 0, (float)x, 0, 0, 0, (float)y, 0, 0, 0, (float)z};
        c2w.insert(c2w.end(), row, row + 12);
        n++;
    }
    double size() const { return camhost::frustum_display_size(c2w.data(), n); }
};

// n points spread evenly over a sphere of radius r.
Cloud fibonacci_sphere(int n, double r) {
    Cloud c;
    const double golden = (1.0 + std::sqrt(5.0)) / 2.0;
    for (int i = 0; i < n; i++) {
        double z = 1.0 - 2.0 * (i + 0.5) / n, rho = std::sqrt(1.0 - z * z);
        double th = 2.0 * kPi * i / golden;
        c.add(r * rho * std::cos(th), r * rho * std::sin(th), r * z);
    }
    return c;
}

// Midpoint quadrature of the ball mean of 1/|c - e|^2 over (shell radius,
// cos angle). The integrand has a log singularity at s = r, so this is only
// accurate for cameras well inside or outside the ball.
double ball_mean_quadrature_2d(double rho, double r) {
    const int ns = 1500, nu = 1500;
    double acc = 0.0;
    for (int i = 0; i < ns; i++) {
        double s = rho * (i + 0.5) / ns;
        double inner = 0.0;
        for (int j = 0; j < nu; j++) {
            double u = -1.0 + 2.0 * (j + 0.5) / nu;
            inner += 1.0 / (s * s + r * r - 2.0 * s * r * u);
        }
        acc += s * s * inner * (2.0 / nu);
    }
    acc *= rho / ns;
    return acc * 0.5 * 3.0 / (rho * rho * rho);
}

// The angular integral done by hand leaves the radial one, which 4M midpoint
// bins resolve through the singularity to well under 0.1%.
double ball_mean_quadrature_1d(double rho, double r) {
    const int ns = 4000000;
    double acc = 0.0;
    for (int i = 0; i < ns; i++) {
        double s = rho * (i + 0.5) / ns;
        acc += s * std::log((s + r) / std::fabs(s - r));
    }
    acc *= rho / ns;
    return acc * 3.0 / (2.0 * rho * rho * rho * r);
}

void test_ball_formula() {
    using camhost::frustum_size_detail::ball_mean_inv_dist2;
    for (double r : {0.0, 0.3, 1.5, 3.0}) {
        double closed = ball_mean_inv_dist2(1.0, r), quad = ball_mean_quadrature_2d(1.0, r);
        CHECK(std::fabs(closed - quad) < 0.01 * quad,
              "ball mean at r=%.1f: closed %.5f vs 2d quadrature %.5f", r, closed, quad);
    }
    for (double r : {0.001, 0.3, 0.9, 0.99, 1.0, 1.01, 1.5, 3.0}) {
        double closed = ball_mean_inv_dist2(1.0, r), quad = ball_mean_quadrature_1d(1.0, r);
        CHECK(std::fabs(closed - quad) < 1e-3 * quad,
              "ball mean at r=%.3f: closed %.6f vs 1d quadrature %.6f", r, closed, quad);
    }
}

// Eyes on a grid over the shell the heuristic assumes, looking at everything:
// the frusta of size s must cover ~1% of a 90-degree square view.
void test_screen_budget() {
    Cloud cams = fibonacci_sphere(200, 1.0);
    double s = cams.size();
    const int g = 48;
    double cover = 0.0;
    int eyes = 0;
    for (int ix = 0; ix < g; ix++)
        for (int iy = 0; iy < g; iy++)
            for (int iz = 0; iz < g; iz++) {
                double e[3] = {-2.0 + 4.0 * (ix + 0.5) / g, -2.0 + 4.0 * (iy + 0.5) / g,
                               -2.0 + 4.0 * (iz + 0.5) / g};
                double d = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
                if (d < 0.5 || d > 2.0) continue;
                double sum = 0.0;
                for (int64_t i = 0; i < cams.n; i++) {
                    double dx = cams.c2w[i * 12 + 3] - e[0], dy = cams.c2w[i * 12 + 7] - e[1],
                           dz = cams.c2w[i * 12 + 11] - e[2];
                    sum += 1.0 / (dx * dx + dy * dy + dz * dz);
                }
                cover += s * s * sum / 4.0;   // (s F / D)^2 / W^2 with F = W / 2
                eyes++;
            }
    cover /= eyes;
    CHECK(std::fabs(cover - 0.01) < 0.001, "screen coverage %.4f, wanted 0.01", cover);
}

void test_scaling() {
    double s200 = fibonacci_sphere(200, 1.0).size();
    double s800 = fibonacci_sphere(800, 1.0).size();
    CHECK(std::fabs(s800 / s200 - 0.5) < 1e-3, "4x cameras -> %.4fx size, wanted 0.5", s800 / s200);
    double s_big = fibonacci_sphere(200, 10.0).size();
    CHECK(std::fabs(s_big / s200 - 10.0) < 1e-6, "10x positions -> %.6fx size", s_big / s200);
}

// 45 shots, each with 4 more brackets jittered by 0.1% of the radius: the
// same size as the 45 shots alone.
void test_bracket_merge() {
    Cloud shots = fibonacci_sphere(45, 1.0);
    Cloud brackets;
    for (int64_t i = 0; i < shots.n; i++) {
        double x = shots.c2w[i * 12 + 3], y = shots.c2w[i * 12 + 7], z = shots.c2w[i * 12 + 11];
        brackets.add(x, y, z);
        for (int b = 1; b <= 4; b++)
            brackets.add(x + 1e-3 * std::cos(b + i), y + 1e-3 * std::sin(b * 0.7 + i), z + 5e-4 * b);
    }
    double s_shots = shots.size(), s_brackets = brackets.size();
    CHECK(std::fabs(s_brackets - s_shots) < 1e-9 * s_shots,
          "brackets %.6f vs shots %.6f", s_brackets, s_shots);
    CHECK(s_shots > 0.02 && s_shots < 0.06, "45-shot size %.4f outside the plausible band", s_shots);
}

void test_fallbacks() {
    Cloud none;
    CHECK(none.size() == 0.2, "empty -> %.3f", none.size());
    Cloud one;
    one.add(3, 4, 5);
    CHECK(one.size() == 0.2, "one camera -> %.3f", one.size());
    Cloud same;
    for (int i = 0; i < 5; i++) same.add(3, 4, 5);
    CHECK(same.size() == 0.2, "coincident cameras -> %.3f", same.size());
    Cloud two;
    two.add(1, 0, 0); two.add(-1, 0, 0);
    CHECK(std::fabs(two.size() - 0.15) < 1e-9, "two cameras -> %.4f, cap 0.15", two.size());
    Cloud nan_cam = fibonacci_sphere(50, 1.0);
    double s_clean = nan_cam.size();
    nan_cam.add(std::nan(""), 0, 0);
    CHECK(nan_cam.size() == s_clean, "a NaN position changed the size");
}

// The Stategallery RealityScan export: 649 cameras within ~16 units and a
// handful registered 1000-13800 units out.
void test_strays() {
    Cloud c = fibonacci_sphere(600, 10.0);
    const double s_clean = c.size();
    for (double d : {1157.0, 2056.0, 2443.0, 7487.0, 13781.0}) c.add(d, 0.3 * d, 0);
    CHECK(std::fabs(c.size() - s_clean) < 1e-9 * s_clean,
          "stray cameras moved the size %.4f -> %.4f", s_clean, c.size());
    Cloud walk;
    for (int i = 0; i < 100; i++) walk.add(i, 0, 0);
    const double s_walk = walk.size();
    walk.add(150, 0, 0);
    CHECK(walk.size() != s_walk, "a camera 1.5 walk-lengths on was dropped as a stray");
}

}  // namespace

int main() {
    test_ball_formula();
    test_screen_budget();
    test_scaling();
    test_bracket_merge();
    test_fallbacks();
    test_strays();
    if (g_fail) {
        std::fprintf(stderr, "frustum_size_test: %d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("frustum_size_test: all checks passed\n");
    return 0;
}
