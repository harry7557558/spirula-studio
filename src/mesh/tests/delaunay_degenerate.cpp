// delaunay_degenerate -- the tetrahedralization must terminate, and stay a
// valid complex, on the point cloud meshing actually feeds it: 7 points per
// Gaussian (centre + 6 axis endpoints), collinear, coplanar and cospherical by
// construction. See docs/notes/delaunay-degeneracy.md.
//
//   ./build/delaunay_degenerate [threads]
//
// Each case runs under a watchdog: the failure guarded against is a walk that
// never ends, not a wrong answer.

#include "mesh/Delaunay3D.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

constexpr double kWatchdogSeconds = 120.0;

// The 7 points meshing samples per Gaussian, rounded to float32 as the real
// pipeline does -- exact coincidences depend on that rounding.
void add_splat(std::vector<double>& pts, const double m[3], const double o0[3],
               const double o1[3], const double o2[3]) {
    const double* o[3] = {o0, o1, o2};
    for (int c = 0; c < 3; c++) pts.push_back((float)m[c]);
    for (int a = 0; a < 3; a++)
        for (int s = -1; s <= 1; s += 2)
            for (int c = 0; c < 3; c++)
                pts.push_back((float)(m[c] + s * o[a][c]));
}

// Identical axis-aligned splats on a lattice: each octahedron is cospherical,
// and so are points from different splats. This is what hung the inexact walk,
// and what crashed the old fixed-size cavity's overflow path.
std::vector<double> lattice_cloud(int g) {
    std::vector<double> pts;
    pts.reserve((size_t)g * g * g * 21);
    const double o0[3] = {0.03, 0, 0}, o1[3] = {0, 0.03, 0}, o2[3] = {0, 0, 0.03};
    for (int i = 0; i < g; i++)
        for (int j = 0; j < g; j++)
            for (int k = 0; k < g; k++) {
                const double m[3] = {0.1 * i, 0.1 * j, 0.1 * k};
                add_splat(pts, m, o0, o1, o2);
            }
    return pts;
}

// Flat splats tangent to a surface: the shape a trained scene converges to,
// and the case that must stay fast and lose nothing.
std::vector<double> surface_cloud(int n) {
    std::vector<double> pts;
    pts.reserve((size_t)n * 21);
    uint32_t rng = 12345u;
    auto uni = [&]() {
        rng = rng * 1664525u + 1013904223u;
        return (double)(rng >> 8) / (double)(1u << 24);
    };
    for (int i = 0; i < n; i++) {
        const double z = 2.0 * uni() - 1.0;
        const double phi = 6.283185307179586 * uni();
        const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
        const double d[3] = {r * std::cos(phi), r * std::sin(phi), z};
        double a[3] = {-d[1], d[0], 0.0};
        double alen = std::sqrt(a[0] * a[0] + a[1] * a[1]);
        if (alen < 1e-6) { a[0] = 1.0; a[1] = 0.0; alen = 1.0; }
        for (int c = 0; c < 3; c++) a[c] /= alen;
        const double b[3] = {d[1] * a[2] - d[2] * a[1], d[2] * a[0] - d[0] * a[2],
                             d[0] * a[1] - d[1] * a[0]};
        const double m[3] = {d[0] * (1.0 + 0.01 * uni()), d[1] * (1.0 + 0.01 * uni()),
                             d[2] * (1.0 + 0.01 * uni())};
        const double s = std::pow(10.0, -3.0 + 1.5 * uni());
        const double thin = s * std::pow(10.0, -1.5 * uni());
        double o0[3], o1[3], o2[3];
        for (int c = 0; c < 3; c++) {
            o0[c] = a[c] * s;
            o1[c] = b[c] * s * (0.5 + uni());
            o2[c] = d[c] * thin;
        }
        add_splat(pts, m, o0, o1, o2);
    }
    return pts;
}

// long double, matching the predicate the triangulation itself filters on: a
// cell it certified POSITIVE cannot evaluate <= 0 here.
long double orient_3d(const double* a, const double* b, const double* c,
                      const double* d) {
    const long double a00 = (long double)b[0] - a[0], a01 = (long double)b[1] - a[1],
                      a02 = (long double)b[2] - a[2];
    const long double a10 = (long double)c[0] - a[0], a11 = (long double)c[1] - a[1],
                      a12 = (long double)c[2] - a[2];
    const long double a20 = (long double)d[0] - a[0], a21 = (long double)d[1] - a[1],
                      a22 = (long double)d[2] - a[2];
    return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
           a02 * (a10 * a21 - a11 * a20);
}

delaunay3d::Delaunay3DResult run_watched(const std::vector<double>& pts,
                                         int threads, const char* what) {
    delaunay3d::Delaunay3DResult result;
    std::atomic<bool> done{false};
    const int n = (int)(pts.size() / 3);
    std::thread worker([&] {
        result = delaunay3d::compute_delaunay_3d(pts.data(), n, threads, false);
        done.store(true);
    });
    const auto t0 = std::chrono::steady_clock::now();
    while (!done.load()) {
        const double waited =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (waited > kWatchdogSeconds) {
            std::printf("BAD  %s t%d: still running after %.0f s\n", what, threads,
                        kWatchdogSeconds);
            std::fflush(stdout);
            std::_Exit(1);  // the worker is stuck; there is nothing to join
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    worker.join();
    return result;
}

int check(const std::vector<double>& pts, int threads, const char* what,
          double min_point_fraction) {
    const int n = (int)(pts.size() / 3);
    const auto t0 = std::chrono::steady_clock::now();
    const delaunay3d::Delaunay3DResult tri = run_watched(pts, threads, what);
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::vector<char> used((size_t)n, 0);
    for (int t = 0; t < tri.nb_cells; t++) {
        const int* c = (const int*)&tri.cell_vertices[(size_t)t * 4];
        for (int i = 0; i < 4; i++) {
            if (c[i] < 0 || c[i] >= n) {
                std::printf("BAD  %s t%d: tet %d vertex %d out of range (%d)\n", what,
                            threads, t, i, c[i]);
                return 1;
            }
            used[(size_t)c[i]] = 1;
            for (int j = i + 1; j < 4; j++)
                if (c[i] == c[j]) {
                    std::printf("BAD  %s t%d: tet %d repeats vertex %d\n", what, threads,
                                t, c[i]);
                    return 1;
                }
        }
        // Degenerate input must cost points, never validity: a flat or inverted
        // cell means a cavity was stellated that the point could not see.
        if (orient_3d(&pts[3 * (size_t)c[0]], &pts[3 * (size_t)c[1]],
                      &pts[3 * (size_t)c[2]], &pts[3 * (size_t)c[3]]) <= 0.0L) {
            std::printf("BAD  %s t%d: tet %d is flat or inverted\n", what, threads, t);
            return 1;
        }
    }

    int nb_used = 0;
    for (int i = 0; i < n; i++) nb_used += used[i];
    const double fraction = (double)nb_used / (double)n;
    if (fraction < min_point_fraction) {
        std::printf("BAD  %s t%d: only %.2f%% of points triangulated, wanted %.0f%%\n",
                    what, threads, 100.0 * fraction, 100.0 * min_point_fraction);
        return 1;
    }
    std::printf("ok   %s t%d: %d points, %d tetrahedra, %.2f%% of points, %.2f s\n", what,
                threads, n, tri.nb_cells, 100.0 * fraction, secs);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const int hw = (int)std::thread::hardware_concurrency();
    std::vector<int> thread_counts = {1, hw > 1 ? hw : 4};
    if (argc > 1) thread_counts = {std::atoi(argv[1])};

    const std::vector<double> lattice = lattice_cloud(59);
    const std::vector<double> surface = surface_cloud(100000);
    int failed = 0;
    for (int t : thread_counts) {
        // The lattice is the worst case the perturbation cannot separate;
        // 95% is what it triangulates today, and a real scene loses nothing.
        failed += check(lattice, t, "lattice", 0.90);
        failed += check(surface, t, "surface", 0.999);
    }

    std::printf("\n%d failures\n", failed);
    return failed ? 1 : 0;
}
