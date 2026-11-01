#include "dense/Visibility.h"

#include "core/HardwareThreads.h"
#include "data/Knn.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace spirula::dense {
namespace {

// Surface samples: one point per cell whose side is this share of the distance to the nearest
// camera, or of the median such distance when nearer, rounded down to a power of two: the
// floor keeps a denser path of cameras from cutting the surface into more cells.
constexpr double kSampleCell = 0.05;
constexpr size_t kSamples = 1 << 15;
constexpr int kGrid = 24;
constexpr double kBehind = 1.15;
// Optical axes closer to parallel than this do not locate where two views look.
constexpr double kMinAxisAngle = 5 * 3.141592653589793 / 180;

uint64_t mix(uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

template<class F> void parallel(size_t n, const F& body) {
    std::atomic<size_t> next{0};
    auto run = [&] { for (size_t i; (i = next.fetch_add(4096)) < n;) for (size_t j = i; j < std::min(n, i + 4096); ++j) body(j); };
    std::vector<std::thread> pool;
    for (unsigned t = 1; t < std::max(1u, spirula::hardware_threads()); ++t) pool.emplace_back(run);
    run();
    for (auto& thread : pool) thread.join();
}

std::vector<sfm::Vec3> surface_samples(const std::vector<sfm::Vec3>& input, const std::vector<View>& views) {
    std::vector<sfm::Vec3> points;
    const size_t step = std::max<size_t>(1, input.size() / (64 * kSamples));
    for (size_t i = 0; i < input.size(); i += step)
        if (std::isfinite(input[i].x) && std::isfinite(input[i].y) && std::isfinite(input[i].z)) points.push_back(input[i]);
    if (points.empty() || views.empty()) return points;
    sfm::Vec3 mean{};
    for (const auto& v : views) mean = mean + v.center * (1.0 / views.size());
    std::vector<float> centers(views.size() * 3);
    for (size_t i = 0; i < views.size(); ++i) {
        const auto c = views[i].center - mean;
        centers[i * 3] = (float)c.x; centers[i * 3 + 1] = (float)c.y; centers[i * 3 + 2] = (float)c.z;
    }
    const knn::KdTree3 tree(centers.data(), (int64_t)views.size());
    std::vector<double> distance(points.size(), 0);
    parallel(points.size(), [&](size_t i) {
        const auto q = points[i] - mean;
        const float query[3] = {(float)q.x, (float)q.y, (float)q.z};
        float d2 = 0; int32_t index = 0;
        if (tree.query(query, -1, 1, &d2, &index)) distance[i] = std::sqrt((double)d2);
    });
    std::vector<double> sorted = distance;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double floor = sorted[sorted.size() / 2];
    std::vector<std::pair<uint64_t, uint32_t>> keys(points.size());
    parallel(points.size(), [&](size_t i) {
        const auto& p = points[i];
        const double side = kSampleCell * std::max(floor, distance[i]);
        const int level = side > 0 && std::isfinite(side) ? std::ilogb(side) : 0;
        const double size = std::ldexp(1.0, level);
        uint64_t k = mix((uint64_t)(int64_t)level);
        for (double coordinate : {p.x, p.y, p.z}) k = mix(k ^ (uint64_t)(int64_t)std::floor(coordinate / size));
        keys[i] = {k, (uint32_t)i};
    });
    std::sort(keys.begin(), keys.end());
    size_t cells = 0;
    for (size_t i = 0; i < keys.size(); ++i) cells += i == 0 || keys[i].first != keys[i - 1].first;
    const double keep = std::min(1.0, (double)kSamples / (double)cells);
    std::vector<uint32_t> kept;
    for (size_t i = 0; i < keys.size(); ++i)
        if ((i == 0 || keys[i].first != keys[i - 1].first) && (double)(mix(keys[i].first) >> 11) * 0x1.0p-53 < keep)
            kept.push_back(keys[i].second);
    std::sort(kept.begin(), kept.end());
    std::vector<sfm::Vec3> out;
    for (uint32_t i : kept) out.push_back(points[i]);
    return out;
}

sfm::Vec3 forward(const View& view) { return {view.world_to_camera[6], view.world_to_camera[7], view.world_to_camera[8]}; }

// Distance along each view's axis to where it passes closest to its neighbours' axes, a median
// over those in front of both; views with too few such neighbours take the median of the rest.
std::vector<double> axis_depths(const std::vector<View>& views) {
    const size_t n = views.size();
    std::vector<double> depth(n, 0);
    if (n < 2) return depth;
    sfm::Vec3 mean{};
    for (const auto& v : views) mean = mean + v.center * (1.0 / n);
    std::vector<float> centers(n * 3);
    double spread = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto c = views[i].center - mean;
        centers[i * 3] = (float)c.x; centers[i * 3 + 1] = (float)c.y; centers[i * 3 + 2] = (float)c.z;
        spread += c.dot(c) / n;
    }
    const knn::KdTree3 tree(centers.data(), (int64_t)n);
    const int k = (int)std::min<size_t>(n - 1, 16);
    std::vector<float> distance((size_t)k);
    std::vector<int32_t> nearest((size_t)k);
    std::vector<double> found;
    const double near_parallel = std::sin(kMinAxisAngle) * std::sin(kMinAxisAngle);
    for (size_t v = 0; v < n; ++v) {
        const int count = tree.query(&centers[v * 3], (int32_t)v, k, distance.data(), nearest.data());
        const auto fv = forward(views[v]);
        std::vector<double> t;
        for (int i = 0; i < count; ++i) {
            const auto& u = views[(size_t)nearest[(size_t)i]];
            if (u.source_image == views[v].source_image) continue;
            const auto fu = forward(u), w = views[v].center - u.center;
            const double b = fv.dot(fu), d = fv.dot(w), e = fu.dot(w), denominator = 1 - b * b;
            if (denominator < near_parallel) continue;
            const double tv = (b * e - d) / denominator, tu = (e - b * d) / denominator;
            if (tv > 0 && tu > 0) t.push_back(tv);
        }
        if (t.size() >= 3) {
            std::nth_element(t.begin(), t.begin() + t.size() / 2, t.end());
            depth[v] = t[t.size() / 2];
            found.push_back(depth[v]);
        }
    }
    double fallback = spread > 0 ? 2 * std::sqrt(spread) : 1;
    if (!found.empty()) {
        std::nth_element(found.begin(), found.begin() + found.size() / 2, found.end());
        fallback = found[found.size() / 2];
    }
    for (double& d : depth) if (!(d > 0)) d = fallback;
    return depth;
}

std::vector<sfm::Vec3> ray_points(const std::vector<View>& views) {
    const auto depth = axis_depths(views);
    std::vector<sfm::Vec3> out;
    for (size_t v = 0; v < views.size(); ++v) {
        const auto& view = views[v];
        const auto to_world = sfm::transpose(view.world_to_camera);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                sfm::Vec3 ray;
                if (!bearing(view, {(x + 0.5) * view.camera.width / 4, (y + 0.5) * view.camera.height / 4}, ray)) continue;
                out.push_back(view.center + sfm::mul(to_world, ray.normalized() * depth[v]));
            }
    }
    return out;
}

}  // namespace

ProjectedPoints project_visibility(const std::vector<View>& views, const std::vector<sfm::Vec3>& points,
                                   const std::function<void()>& check_cancel) {
    ProjectedPoints out;
    out.points = surface_samples(points.empty() ? ray_points(views) : points, views);
    out.visible.resize(views.size());
    std::atomic<size_t> next{0};
    std::atomic<bool> stop{false};
    std::exception_ptr failure;
    std::mutex mutex;
    auto work = [&] {
        struct Seen { int cell; double depth; uint32_t index; };
        std::vector<Seen> seen;
        std::vector<double> nearest(kGrid * kGrid), around(kGrid * kGrid);
        for (size_t v; !stop && (v = next.fetch_add(1)) < views.size();) {
            try {
                if (check_cancel && v % 256 == 0) { std::lock_guard<std::mutex> lock(mutex); check_cancel(); }
                const View& view = views[v];
                seen.clear();
                std::fill(nearest.begin(), nearest.end(), std::numeric_limits<double>::infinity());
                for (uint32_t i = 0; i < out.points.size(); ++i) {
                    sfm::Vec2 pixel;
                    if (!project(view, out.points[i], pixel) || pixel.x < 0 || pixel.y < 0 ||
                        pixel.x >= view.camera.width || pixel.y >= view.camera.height) continue;
                    const int cell = std::min(kGrid - 1, (int)(pixel.y * kGrid / view.camera.height)) * kGrid +
                                     std::min(kGrid - 1, (int)(pixel.x * kGrid / view.camera.width));
                    const double d = depth(view, out.points[i]);
                    if (!(d > 0)) continue;
                    seen.push_back({cell, d, i});
                    nearest[(size_t)cell] = std::min(nearest[(size_t)cell], d);
                }
                // The 3x3 cells around: samples are sparse enough that a cell can miss the nearer surface.
                for (int y = 0; y < kGrid; ++y)
                    for (int x = 0; x < kGrid; ++x) {
                        double d = std::numeric_limits<double>::infinity();
                        for (int j = std::max(0, y - 1); j <= std::min(kGrid - 1, y + 1); ++j)
                            for (int i = std::max(0, x - 1); i <= std::min(kGrid - 1, x + 1); ++i) d = std::min(d, nearest[(size_t)(j * kGrid + i)]);
                        around[(size_t)(y * kGrid + x)] = d;
                    }
                auto& visible = out.visible[v];
                for (const auto& s : seen) if (s.depth <= kBehind * around[(size_t)s.cell]) visible.push_back(s.index);
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                if (!failure) failure = std::current_exception();
                stop = true;
            }
        }
    };
    const unsigned threads = std::max(1u, spirula::hardware_threads());
    std::vector<std::thread> pool;
    for (unsigned t = 1; t < std::min<size_t>(threads, views.size()); ++t) pool.emplace_back(work);
    work();
    for (auto& thread : pool) thread.join();
    if (failure) std::rethrow_exception(failure);
    return out;
}

}  // namespace spirula::dense
