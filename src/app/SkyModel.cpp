#include "app/SkyModel.h"
#include "app/SkySampling.h"
#include "data/CameraMath.h"
#include "data/Knn.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace spirula::sky {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr float dc_basis = 0.28209479177387814f;
constexpr int64_t batch_size = 256;
constexpr size_t projection_parallel_rows = 16384;
constexpr size_t completion_parallel_rows = 4096;
// Rows this close to the horizon get the median colour pass.
constexpr double horizon_band = 0.15;

template<class Work>
void visit_rows(int64_t count, [[maybe_unused]] bool parallel, const Work& work) {
#ifdef _OPENMP
    if (parallel && omp_get_max_threads() > 1 && !omp_in_parallel()) {
        #pragma omp parallel for schedule(dynamic, 1)
        for (int64_t batch = 0; batch < count; batch += batch_size)
            work(batch, std::min(count, batch + batch_size));
        return;
    }
#endif
    work(0, count);
}

std::array<double, 2> unit_circle(double half_turns) {
    const double whole = std::nearbyint(half_turns);
    if (half_turns == whole)
        return {std::fmod(std::fabs(whole), 2.0) == 0.0 ? 1.0 : -1.0, 0.0};
    const bool half = half_turns == whole + 0.5 || half_turns == whole - 0.5;
    return {half ? 0.0 : std::cos(pi * half_turns), std::sin(pi * half_turns)};
}

class Sphere {
public:
    Sphere(double radius, int64_t count) : radius_(radius), count_(count) {}

    std::array<float, 3> point(int64_t ordinal) const {
        const float band = (float)latitude(ordinal);
        const double ring = std::sqrt(1.0 - (double)band * (double)band);
        // Float phase quantized ultra's 262k rows into 64 azimuth columns.
        const auto azimuth = unit_circle(0.7639320225002102 * (double)ordinal);
        return {(float)(azimuth[0] * ring * radius_), (float)((double)band * radius_),
                (float)(azimuth[1] * ring * radius_)};
    }

    float spacing() const {
        const float radius = (float)radius_;
        const float area = 4.0f * (float)pi * (radius * radius) / (float)count_;
        return 1.5f * std::sqrt(area / (float)pi);
    }

    float local_spacing(int64_t ordinal) const {
        constexpr float min_scale = 0.45f;
        return spacing() * std::max(min_scale, (float)std::sqrt(density(ordinal)));
    }

private:
    double latitude(int64_t ordinal) const {
        const double u = ((double)ordinal + 0.5) * (2.0 / (double)count_) - 1.0;
        return u * (kHorizonBias + (1.0 - kHorizonBias) * u * u);
    }
    double density(int64_t ordinal) const {
        const double u = ((double)ordinal + 0.5) * (2.0 / (double)count_) - 1.0;
        return kHorizonBias + 3.0 * (1.0 - kHorizonBias) * u * u;
    }

    double radius_;
    int64_t count_;
};

struct ViewContribution {
    const Rows& geometry;
    const View& frame;
    Bake& colors;

    void operator()(int64_t first, int64_t last) const {
        camhost::Camera lens{frame.model, frame.tier, frame.width, frame.height,
                             frame.fx, frame.fy, frame.cx, frame.cy};
        std::copy_n(frame.dist, 8, lens.dist);
        const auto* pose = frame.c2w;
        const std::array<double, 3> center{pose[3], pose[7], pose[11]};
        const double axes[3][3] = {{pose[0], pose[4], pose[8]},
                                    {-pose[1], -pose[5], -pose[9]},
                                    {-pose[2], -pose[6], -pose[10]}};
        for (int64_t row = first; row != last; ++row) {
            const size_t offset = (size_t)row * 3;
            const double delta[3] = {geometry.means[offset] - center[0],
                                     geometry.means[offset + 1] - center[1],
                                     geometry.means[offset + 2] - center[2]};
            double direction[3];
            for (int axis = 0; axis != 3; ++axis)
                direction[axis] = (axes[axis][0] * delta[0] + axes[axis][1] * delta[1]) +
                                  axes[axis][2] * delta[2];
            double pixel[2];
            if (!(direction[2] > 0.0) || !camhost::ray_pixel(lens, direction, pixel))
                continue;
            if (pixel[0] < 0.0 || pixel[1] < 0.0 ||
                pixel[0] > (double)frame.width || pixel[1] > (double)frame.height)
                continue;
            const auto horizontal = sampling::bracket((float)pixel[0] - 0.5f,
                                                       frame.width, sampling::Border::LastCell);
            const auto vertical = sampling::bracket((float)pixel[1] - 0.5f,
                                                     frame.height, sampling::Border::LastCell);
            const auto color = sampling::rgb(frame.rgb, frame.width, horizontal, vertical);
            for (int channel = 0; channel != 3; ++channel) {
                const float normalized = color[channel] * (1.0f / 255.0f);
                colors.sum[offset + channel] += normalized;
            }
            ++colors.hits[row];
        }
    }
};

struct Observations {
    std::vector<float> positions;
    std::vector<int64_t> original_rows;

    Observations(const Rows& rows, const Bake& bake, size_t covered) {
        positions.reserve(covered * 3);
        original_rows.reserve(covered);
        for (size_t row = 0; row != bake.hits.size(); ++row) {
            if (bake.hits[row] == 0) continue;
            const auto point = rows.means.begin() + row * 3;
            positions.insert(positions.end(), point, point + 3);
            original_rows.push_back((int64_t)row);
        }
    }
};

struct CompleteColors {
    Rows& output;
    const Bake& coverage;
    const Observations& observed;
    const knn::KdTree3& nearest;

    void operator()(int64_t first, int64_t last) const {
        for (int64_t row = first; row != last; ++row) {
            if (coverage.hits[row] != 0) continue;
            int32_t neighbor = -1;
            float distance;
            nearest.query(output.means.data() + row * 3, -1, 1, &distance, &neighbor);
            if (neighbor < 0) continue;
            const auto source = output.features_dc.begin() + observed.original_rows[neighbor] * 3;
            std::copy_n(source, 3, output.features_dc.begin() + row * 3);
        }
    }
};

void average_colors(Rows& output, const Bake& coverage) {
    for (size_t row = 0; row != coverage.hits.size(); ++row) {
        const float divisor = coverage.hits[row] == 0 ? 1.0f : (float)coverage.hits[row];
        const size_t start = row * 3;
        for (int channel = 0; channel != 3; ++channel)
            output.features_dc[start + channel] =
                (coverage.sum[start + channel] / divisor - 0.5f) / dc_basis;
    }
}

}  // namespace

double scene_radius(const double* xyz, int64_t count) {
    float furthest = 0.0f;
    for (int64_t point = 0; point < count; ++point) {
        const auto* source = xyz + point * 3;
        const float x = (float)source[0], y = (float)source[1], z = (float)source[2];
        const float squared = (x * x + y * y) + z * z;
        if (squared > furthest) furthest = squared;
    }
    return furthest == 0.0f ? 0.0 : (double)(std::sqrt(furthest) * 1.5f);
}

void spiral_point(int64_t index, double radius, float xyz[3], int64_t splat_count) {
    const auto point = Sphere(radius, splat_count).point(index);
    std::copy(point.begin(), point.end(), xyz);
}

bool generate(const double* xyz, int64_t point_count, Rows& rows, int64_t splat_count) {
    const double extent = scene_radius(xyz, point_count);
    if (!(extent > 0.0)) return false;
    generate_at_radius(extent, rows, splat_count);
    return true;
}

void generate_at_radius(double radius, Rows& rows, int64_t splat_count) {
    // The optimizer and densify kernels skip whole 256-row blocks.
    if (splat_count <= 0 || splat_count % 256 != 0)
        throw std::invalid_argument("sky splat count must be a positive multiple of 256");
    const Sphere sphere(radius, splat_count);
    rows.radius = radius;
    rows.spacing = sphere.spacing();
    const size_t count = (size_t)splat_count;
    rows.means.resize(count * 3);
    rows.quats.resize(count * 4);
    rows.scales.resize(count * 3);
    rows.opacities.assign(count, std::numeric_limits<float>::infinity());
    rows.features_dc.assign(count * 3, 0.0f);
    constexpr std::array<float, 4> identity{1.0f, 0.0f, 0.0f, 0.0f};
    for (size_t row = 0; row != count; ++row) {
        const auto point = sphere.point((int64_t)row);
        std::copy(point.begin(), point.end(), rows.means.begin() + row * 3);
        std::copy(identity.begin(), identity.end(), rows.quats.begin() + row * 4);
        const float scale = (float)std::log((double)sphere.local_spacing((int64_t)row));
        rows.scales[row * 3 + 0] = scale;
        rows.scales[row * 3 + 1] = scale;
        rows.scales[row * 3 + 2] = scale;
    }
}

void bake_accumulate(const Rows& rows, const View& view, Bake& bake) {
    const size_t count = rows.means.size() / 3;
    if (bake.sum.size() != count * 3) bake.sum.assign(count * 3, 0.0f);
    if (bake.hits.size() != count) bake.hits.assign(count, 0u);
    if (!view.rgb || view.width < 1 || view.height < 1) return;
    visit_rows((int64_t)count, count >= projection_parallel_rows,
                ViewContribution{rows, view, bake});
}

void bake_finalize(Rows& rows, const Bake& bake) {
    const size_t count = rows.means.size() / 3;
    if (bake.hits.size() != count || bake.sum.size() != count * 3)
        throw std::runtime_error("sky::bake_finalize: bake was never accumulated");
    const auto covered = (size_t)std::count_if(bake.hits.begin(), bake.hits.end(),
                                              [](uint32_t hits) { return hits != 0; });
    if (covered == 0) {
        std::fill(rows.features_dc.begin(), rows.features_dc.end(), 1.0f);
        return;
    }
    average_colors(rows, bake);
    if (covered != count) {
        const Observations observed(rows, bake, covered);
        const knn::KdTree3 nearest(observed.positions.data(), (int64_t)covered);
        visit_rows((int64_t)count, count - covered >= completion_parallel_rows,
                    CompleteColors{rows, bake, observed, nearest});
    }
    median_filter_horizon(rows);
}

void median_filter_horizon(Rows& rows) {
    const int64_t count = (int64_t)(rows.means.size() / 3);
    if (rows.spacing <= 0.0f || rows.radius <= 0.0 || count < 2) return;
    // Read the unfiltered colours: in-place neighbours would race.
    const std::vector<float> source = rows.features_dc;
    const knn::KdTree3 tree(rows.means.data(), count);
    const auto filter = [&](int64_t first, int64_t last) {
        for (int64_t row = first; row != last; ++row) {
            if (std::fabs(rows.means[(size_t)row * 3 + 1]) >=
                horizon_band * (float)rows.radius) continue;
            float d2[8];
            int32_t ids[8];
            const int found = tree.query(rows.means.data() + row * 3,
                                         (int32_t)row, 8, d2, ids);
            for (int channel = 0; channel != 3; ++channel) {
                float values[9];
                values[0] = source[(size_t)row * 3 + channel];
                for (int i = 0; i != found; ++i)
                    values[i + 1] = source[(size_t)ids[i] * 3 + channel];
                const int n = found + 1;
                std::nth_element(values, values + n / 2, values + n);
                rows.features_dc[(size_t)row * 3 + channel] = values[n / 2];
            }
        }
    };
    visit_rows(count, count >= (int64_t)projection_parallel_rows, filter);
}

}  // namespace spirula::sky
