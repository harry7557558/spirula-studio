// scene_center_test -- `--scene-center auto` (dsparse::resolve_scene_center)
// on clouds whose distance from the origin is known in their own radii.

#include "data/DatasetParser.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace dsparse;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

// n points of a unit Gaussian about `at`: a median radius of ~1.54.
std::vector<double> cloud(int64_t n, const std::array<double, 3>& at, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> xyz((size_t)n * 3);
    for (int64_t i = 0; i < n; i++)
        for (int r = 0; r < 3; r++) xyz[(size_t)i * 3 + r] = at[r] + g(rng);
    return xyz;
}

// c2w [N,3,4], identity rotations, one camera per point of `pos`.
std::vector<double> cameras(const std::vector<double>& pos) {
    const size_t n = pos.size() / 3;
    std::vector<double> c2w(n * 12, 0.0);
    for (size_t i = 0; i < n; i++) {
        c2w[i * 12] = c2w[i * 12 + 5] = c2w[i * 12 + 10] = 1.0;
        for (int r = 0; r < 3; r++) c2w[i * 12 + r * 4 + 3] = pos[i * 3 + r];
    }
    return c2w;
}

ResolvedCenter resolve(const std::string& name, float threshold,
                       const std::vector<double>& c2w, const std::vector<double>& pts) {
    return resolve_scene_center(name, threshold, c2w.data(), (int64_t)c2w.size() / 12,
                                pts.data(), (int64_t)pts.size() / 3);
}

bool same(const std::array<double, 3>& a, const std::array<double, 3>& b) {
    // OpenMP reductions can sum the same coordinates in different orders.
    for (int r = 0; r < 3; ++r)
        if (!(std::fabs(a[r] - b[r]) <= 1e-12 * std::max(1.0, std::fabs(b[r]))))
            return false;
    return true;
}

}  // namespace

int main() {
    const std::array<double, 3> origin{0, 0, 0};
    const std::array<double, 3> ecef{4.2e6, 1.7e5, 4.8e6};
    const std::array<double, 3> ten_radii{12.0, 9.0, 0.0};

    const std::vector<double> cams_here = cameras(cloud(300, origin, 1));
    const std::vector<double> pts_here = cloud(5000, origin, 2);
    const std::vector<double> cams_far = cameras(cloud(300, ecef, 3));
    const std::vector<double> pts_far = cloud(5000, ecef, 4);

    {
        const ResolvedCenter c = resolve("auto", 20.0f, cams_here, pts_here);
        check(c.mode == CenterMode::None && same(c.center, origin),
              "a scene at the origin stays put");
    }
    {
        const ResolvedCenter c = resolve("auto", 20.0f, cams_far, pts_far);
        const std::array<double, 3> want = scene_center(
            CenterMode::PointMedian, cams_far.data(), 300, pts_far.data(), 5000);
        check(c.mode == CenterMode::PointMedian && same(c.center, want),
              "an ECEF scene centres on what point-median gives");
    }
    {
        const ResolvedCenter a = resolve("auto", 20.0f, cams_far, pts_here);
        const ResolvedCenter b = resolve("auto", 20.0f, cams_here, pts_far);
        check(a.mode == CenterMode::None && b.mode == CenterMode::None,
              "far cameras or far points alone do not centre");
    }
    {
        const std::vector<double> cams = cameras(cloud(300, ten_radii, 5));
        const std::vector<double> pts = cloud(5000, ten_radii, 6);
        check(resolve("auto", 20.0f, cams, pts).mode == CenterMode::None,
              "10 radii out stays put at threshold 20");
        check(resolve("auto", 5.0f, cams, pts).mode == CenterMode::PointMedian,
              "10 radii out centres at threshold 5");
    }
    {
        const ResolvedCenter c = resolve("auto", 20.0f, cams_far, {});
        const std::array<double, 3> want = scene_center(
            CenterMode::CameraMedian, cams_far.data(), 300, (const double*)nullptr, 0);
        check(c.mode == CenterMode::CameraMedian && same(c.center, want),
              "no points: decided and centred on the cameras");
    }
    {
        // Past the 2^18 sample the verdict uses, the centre is still the full median.
        const int64_t m = (1 << 18) * 3 + 17;
        const std::vector<double> pts = cloud(m, ecef, 7);
        const ResolvedCenter c = resolve("auto", 20.0f, cams_far, pts);
        const std::array<double, 3> want = scene_center(
            CenterMode::PointMedian, cams_far.data(), 300, pts.data(), m);
        check(c.mode == CenterMode::PointMedian && same(c.center, want),
              "a large cloud centres on its full median");
    }
    {
        const ResolvedCenter a = resolve("", 20.0f, cams_far, pts_far);
        const ResolvedCenter b = resolve("camera-mean", 20.0f, cams_here, pts_here);
        const std::array<double, 3> want = scene_center(
            CenterMode::CameraMean, cams_here.data(), 300, pts_here.data(), 5000);
        check(a.mode == CenterMode::None && same(a.center, origin) &&
                  b.mode == CenterMode::CameraMean && same(b.center, want),
              "a named mode is taken as named, whatever the distance");
    }
    {
        DatasetParserConfig cfg;
        cfg.center_mode = "point-median";
        cfg.center = std::array<double, 3>{1.5, -2.0, 3.25};
        ColmapPoints3D pts;
        pts.xyz = pts_far;
        const ResolvedCenter c = parse_center(cfg, cams_far.data(), 300, pts);
        check(c.mode == CenterMode::PointMedian && same(c.center, *cfg.center),
              "a recorded centre is used as is");
    }

    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "passed", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
