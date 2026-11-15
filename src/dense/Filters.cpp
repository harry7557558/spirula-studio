#include "dense/Filters.h"

#include "data/Knn.h"
#include "dense/DiskTable.h"
#include "dense/ExternalSort.h"
#include "dense/PairSelection.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <limits>
#include <unordered_map>

namespace spirula::dense {
namespace {
constexpr double kPi = 3.14159265358979323846;

struct CellHash {
    size_t operator()(const std::array<int64_t,3>& k) const {
        return (size_t)(k[0] * 73856093LL) ^ (size_t)(k[1] * 19349663LL) ^ (size_t)(k[2] * 83492791LL);
    }
};

double percentile(std::vector<double> v, double q) {   // numpy's linear interpolation
    std::sort(v.begin(), v.end());
    const double at = q / 100.0 * (double)(v.size() - 1);
    const size_t i = (size_t)std::floor(at);
    return v[i] + (v[std::min(i + 1, v.size() - 1)] - v[i]) * (at - (double)i);
}
}  // namespace

FreeSpaceGrid::FreeSpaceGrid(const std::vector<View>& views, double margin, int cells_per_turn)
    : views_(views), margin_(margin), maps_(views.size()) {
    for (const auto& view : views) sources_.push_back(view.source_image);
    std::sort(sources_.begin(), sources_.end());
    sources_.erase(std::unique(sources_.begin(), sources_.end()), sources_.end());
    by_source_.resize(sources_.size());
    for (uint32_t v = 0; v < views.size(); ++v)
        by_source_[std::lower_bound(sources_.begin(), sources_.end(), views[v].source_image) - sources_.begin()].push_back(v);
    for (uint32_t v = 0; v < views.size(); ++v) {
        const auto& camera = views[v].camera;
        Map& map = maps_[v];
        map.periodic = camhost::full_longitude(camera);
        const size_t siblings = by_source_[std::lower_bound(sources_.begin(), sources_.end(), views[v].source_image) - sources_.begin()].size();
        // One face of a split image takes its share of the turn, so a cube keeps 512 cells around.
        if (siblings > 1 && !map.periodic && camera.fx > 0) {
            const double fov = 2 * std::atan(camera.width / (2 * camera.fx));
            map.w = std::clamp((int)std::lround(cells_per_turn * fov / (2 * kPi)), 8, cells_per_turn);
        } else map.w = std::min(cells_per_turn, camera.width);
        map.w = std::max(1, std::min(map.w, camera.width));
        map.h = std::max(1, (int)std::lround((double)camera.height * map.w / camera.width));
        map.nearest.assign((size_t)map.w * map.h, std::numeric_limits<float>::infinity());
    }
}

bool FreeSpaceGrid::locate(uint32_t view, const sfm::Vec3& point, int& x, int& y, double& distance) const {
    sfm::Vec2 pixel;
    if (!project(views_[view], point, pixel)) return false;
    const Map& map = maps_[view];
    x = (int)std::floor(pixel.x * map.w / views_[view].camera.width);
    y = (int)std::floor(pixel.y * map.h / views_[view].camera.height);
    if (x < 0 || y < 0 || x >= map.w || y >= map.h) return false;
    distance = (point - views_[view].center).norm();
    return true;
}

void FreeSpaceGrid::observe(const sfm::Vec3& point, const std::vector<int64_t>& sources) {
    for (const auto source : sources) {
        const auto at = std::lower_bound(sources_.begin(), sources_.end(), source);
        if (at == sources_.end() || *at != source) continue;
        for (uint32_t v : by_source_[at - sources_.begin()]) {
            int x, y; double distance;
            if (!locate(v, point, x, y, distance)) continue;
            float& cell = maps_[v].nearest[(size_t)y * maps_[v].w + x];
            cell = std::min(cell, (float)distance);
        }
    }
}

bool FreeSpaceGrid::sees_through(const sfm::Vec3& point, const std::vector<int64_t>& sources) const {
    for (uint32_t v = 0; v < views_.size(); ++v) {
        if (std::find(sources.begin(), sources.end(), views_[v].source_image) != sources.end()) continue;
        int cx, cy; double distance;
        if (!locate(v, point, cx, cy, distance)) continue;
        // At a depth edge a cell can hold the far side's points but not the occluder's.
        const Map& map = maps_[v];
        float nearest = std::numeric_limits<float>::infinity();
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            int x = cx + dx; const int y = cy + dy;
            if (y < 0 || y >= map.h) continue;
            if (map.periodic) x = (x + map.w) % map.w;
            else if (x < 0 || x >= map.w) continue;
            nearest = std::min(nearest, map.nearest[(size_t)y * map.w + x]);
        }
        if (std::isfinite(nearest) && distance < (double)nearest * (1 - margin_)) return true;
    }
    return false;
}

uint64_t FreeSpaceGrid::bytes() const {
    uint64_t total = 0;
    for (const auto& map : maps_) total += map.nearest.size() * sizeof(float);
    return total;
}

double external_median(const std::filesystem::path& values, uint64_t count, uint64_t budget, const std::function<void()>& check) {
    if (!count) return -1;
    auto sorted = values; sorted += ".sorted";
    external_sort<double>(values, sorted, budget, std::less<double>{}, check);
    BufferedInputFile input(sorted, std::ios::binary, disk_io_buffer_bytes(budget));
    input.seekg((std::streamoff)((count / 2) * sizeof(double)));
    double median = -1;
    const bool read = read_disk_record(input, median);
    input.close(); std::filesystem::remove(sorted);
    if (!read) throw std::runtime_error("cannot read dense error median");
    return median;
}

double cell_focal(const View& view, int grid_width, int grid_height) {
    const double sx = view.source_camera ? view.grid_scale[0] : (double)grid_width / view.camera.width;
    const double sy = view.source_camera ? view.grid_scale[1] : (double)grid_height / view.camera.height;
    return 0.5 * (view.camera.fx * sx + view.camera.fy * sy);
}

double best_depth_per_cell(const sfm::Vec3& point, const View& reference, const std::vector<const View*>& others,
                           int grid_width, int grid_height) {
    const auto to_reference = (reference.center - point).normalized();
    const double reference_focal = cell_focal(reference, grid_width, grid_height);
    double best = std::numeric_limits<double>::infinity();
    for (const View* other : others) {
        const auto to_other = (other->center - point).normalized();
        const double sine = to_reference.cross(to_other).norm();
        const double focal = std::min(reference_focal, cell_focal(*other, grid_width, grid_height));
        if (sine > 0 && focal > 0) best = std::min(best, 1 / (focal * sine));
    }
    return best;
}

double automatic_depth_precision(double median_pair_angle_radians, double median_cell_focal) {
    double bar = 0.02;
    if (median_pair_angle_radians > 0 && median_cell_focal > 0)
        bar = std::max(bar, 1 / (median_cell_focal * std::sin(0.5 * median_pair_angle_radians)));
    return bar;
}

double median_neighbour_spacing(const std::vector<double>& xyz) {
    const int64_t n = (int64_t)xyz.size() / 3;
    if (n < 2) return 0;
    double origin[3] = {xyz[0], xyz[1], xyz[2]};
    std::vector<float> points((size_t)n * 3);
    for (int64_t i = 0; i < n * 3; ++i) points[(size_t)i] = (float)(xyz[(size_t)i] - origin[i % 3]);
    knn::KdTree3 tree(points.data(), n);
    std::vector<double> spacing((size_t)n);
    for (int64_t i = 0; i < n; ++i) {
        float d2 = 0;
        spacing[(size_t)i] = tree.query(&points[(size_t)i * 3], (int32_t)i, 1, &d2) ? std::sqrt((double)d2) : 0;
    }
    const auto middle = spacing.begin() + (std::ptrdiff_t)(spacing.size() / 2);
    std::nth_element(spacing.begin(), middle, spacing.end());
    return *middle;
}

FarFilter resolve_far_filter(const std::vector<double>& xyz, bool metric) {
    return xyz.size() / 3 < 100 ? FarFilter{} : resolve_far_filter(xyz, metric, median_neighbour_spacing(xyz));
}

FarFilter resolve_far_filter(const std::vector<double>& xyz, bool metric, double spacing) {
    FarFilter filter;
    const size_t n = xyz.size() / 3;
    if (n < 100 || !(spacing > 0)) return filter;
    double low[3], high[3];
    for (int c = 0; c < 3; ++c) {
        std::vector<double> axis(n);
        for (size_t i = 0; i < n; ++i) axis[i] = xyz[i * 3 + c];
        low[c] = percentile(axis, 0.5); high[c] = percentile(axis, 99.5);
    }
    filter.low = {low[0], low[1], low[2]}; filter.high = {high[0], high[1], high[2]};
    filter.margin = metric ? 2.0 : 0.2 * (filter.high - filter.low).norm();
    filter.radius = 8 * spacing;
    return filter;
}

double box_excess(const FarFilter& f, const sfm::Vec3& p) {
    return std::max({f.low.x - p.x, p.x - f.high.x, f.low.y - p.y, p.y - f.high.y, f.low.z - p.z, p.z - f.high.z, 0.0});
}

std::vector<uint64_t> far_isolated(const std::vector<sfm::Vec3>& points, const FarFilter& filter, uint64_t* beyond) {
    std::vector<uint64_t> outlying, drop;
    if (beyond) *beyond = 0;
    if (!filter.active()) return drop;
    auto key = [&](const sfm::Vec3& p) {
        return std::array<int64_t,3>{(int64_t)std::floor(p.x / filter.radius), (int64_t)std::floor(p.y / filter.radius),
                                     (int64_t)std::floor(p.z / filter.radius)};
    };
    // A neighbour of a far point lies beyond margin - radius.
    const double reach = filter.margin - filter.radius;
    std::unordered_map<std::array<int64_t,3>, std::vector<uint64_t>, CellHash> grid;
    for (uint64_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        if (!std::isfinite(p.x + p.y + p.z)) continue;
        const double excess = box_excess(filter, p);
        if (excess > reach) grid[key(p)].push_back(i);
        if (excess > filter.margin) outlying.push_back(i);
    }
    if (beyond) *beyond = outlying.size();
    const double r2 = filter.radius * filter.radius;
    for (uint64_t i : outlying) {
        const auto k = key(points[i]);
        int found = 0;
        for (int a = -1; a <= 1; ++a) for (int b = -1; b <= 1; ++b) for (int c = -1; c <= 1; ++c) {
            const auto it = grid.find({k[0] + a, k[1] + b, k[2] + c});
            if (it == grid.end()) continue;
            for (uint64_t j : it->second) {
                if (j == i) continue;
                const auto d = points[j] - points[i];
                if (d.dot(d) <= r2) ++found;
            }
        }
        if (found <= filter.max_neighbours) drop.push_back(i);
    }
    return drop;
}

double automatic_max_baseline(std::vector<double> baselines) {
    if (baselines.empty()) return 0;
    const auto middle = baselines.begin() + (std::ptrdiff_t)(baselines.size() / 2);
    std::nth_element(baselines.begin(), middle, baselines.end());
    return 3 * *middle;
}

FilterPlan plan_depth_precision(const std::vector<PairImage>& images, const std::vector<std::pair<uint32_t,uint32_t>>& pairs,
                                const std::vector<double>& xyz, const std::vector<View>& views,
                                int grid_width, int grid_height, double setting, bool two_image) {
    FilterPlan plan;
    std::vector<double> angles, focals;
    for (const auto& pair : pairs) {
        const auto& a = images[pair.first], &b = images[pair.second];
        sfm::Vec3 sum{}; uint64_t shared = 0;
        for (auto i = a.visible_points.begin(), j = b.visible_points.begin(); i != a.visible_points.end() && j != b.visible_points.end();) {
            if (*i < *j) ++i;
            else if (*j < *i) ++j;
            else { sum = sum + sfm::Vec3{xyz[*i * 3], xyz[*i * 3 + 1], xyz[*i * 3 + 2]}; ++shared; ++i; ++j; }
        }
        if (!shared) continue;
        const auto centroid = sum * (1.0 / (double)shared);
        const auto ra = (a.center - centroid).normalized(), rb = (b.center - centroid).normalized();
        angles.push_back(std::atan2(ra.cross(rb).norm(), ra.dot(rb)));
    }
    for (const auto& view : views) focals.push_back(cell_focal(view, grid_width, grid_height));
    auto median = [](std::vector<double>& v) {
        const auto middle = v.begin() + (std::ptrdiff_t)(v.size() / 2);
        std::nth_element(v.begin(), middle, v.end()); return *middle;
    };
    if (!angles.empty()) plan.median_pair_angle_degrees = median(angles) * 180 / kPi;
    if (!focals.empty()) plan.median_cell_focal = median(focals);
    const double automatic = angles.empty() ? -1 :
        automatic_depth_precision(plan.median_pair_angle_degrees * kPi / 180, plan.median_cell_focal);
    if (setting > 0) plan.depth_precision = setting;
    else if (setting == 0) plan.depth_precision = automatic;
    if (two_image) plan.two_image_depth_precision = plan.depth_precision > 0 ? plan.depth_precision : automatic;
    return plan;
}

void ReprojectionHistogram::add(double pixels, double cells) {
    ++pixels_[std::min(kBins, (size_t)(pixels / kBin))];
    ++cells_[std::min(kBins, (size_t)(cells / kBin))];
    sum_ += pixels; ++count_;
}

ReprojectionSummary ReprojectionHistogram::summary() const {
    ReprojectionSummary s;
    s.observations = count_ + invalid_; s.invalid = invalid_;
    if (!count_) return s;
    // The bin centre where the cumulative count first reaches q of the total; the open bin reads as its edge.
    auto quantile = [&](const std::vector<uint64_t>& bins, double q) {
        const uint64_t want = std::max<uint64_t>(1, (uint64_t)std::ceil(q * (double)count_));
        uint64_t seen = 0;
        for (size_t b = 0; b < bins.size(); ++b)
            if ((seen += bins[b]) >= want) return b == kBins ? kBins * kBin : (b + 0.5) * kBin;
        return kBins * kBin;
    };
    s.mean_pixels = sum_ / (double)count_;
    s.p50_pixels = quantile(pixels_, 0.5); s.p95_pixels = quantile(pixels_, 0.95);
    s.p95_cells = quantile(cells_, 0.95);
    return s;
}

ReprojectionSummary reproject_written(const std::filesystem::path& ply, const std::filesystem::path& ids_path,
                                      const std::filesystem::path& ranges_path, const std::filesystem::path& observations_path,
                                      const std::vector<View>& views, const std::array<double,16>& m,
                                      const std::array<double,3>& center, int grid_width, int grid_height,
                                      uint64_t budget, const std::function<void()>& check) {
    BufferedInputFile input(ply, std::ios::binary, disk_io_buffer_bytes(budget)), ids(ids_path, std::ios::binary, disk_io_buffer_bytes(budget));
    std::string line, header;
    uint64_t count = 0;
    while (std::getline(input, line) && line != "end_header") {
        header += line + "\n";
        if (line.rfind("element vertex ", 0) == 0) count = std::stoull(line.substr(15));
    }
    if (!input || header.find("format binary_little_endian 1.0") == std::string::npos ||
        header.find("property double x\nproperty double y\nproperty double z\nproperty uchar red\n"
                    "property uchar green\nproperty uchar blue\n") == std::string::npos)
        throw std::runtime_error("dense reprojection check expects the writer's double PLY");
    DiskTable<ObservationRange> ranges(ranges_path, std::max<uint64_t>(4096, budget / 2));
    DiskTable<TrackObservation> observations(observations_path, std::max<uint64_t>(4096, budget / 2));
    const double a[9] = {m[0],m[1],m[2], m[4],m[5],m[6], m[8],m[9],m[10]};
    const double det = a[0]*(a[4]*a[8]-a[5]*a[7]) - a[1]*(a[3]*a[8]-a[5]*a[6]) + a[2]*(a[3]*a[7]-a[4]*a[6]);
    if (!(std::fabs(det) > 0)) throw std::runtime_error("dense output transform is singular");
    const double inverse[9] = {(a[4]*a[8]-a[5]*a[7])/det, (a[2]*a[7]-a[1]*a[8])/det, (a[1]*a[5]-a[2]*a[4])/det,
                               (a[5]*a[6]-a[3]*a[8])/det, (a[0]*a[8]-a[2]*a[6])/det, (a[2]*a[3]-a[0]*a[5])/det,
                               (a[3]*a[7]-a[4]*a[6])/det, (a[1]*a[6]-a[0]*a[7])/det, (a[0]*a[4]-a[1]*a[3])/det};
    ReprojectionHistogram histogram;
    for (uint64_t i = 0; i < count; ++i) {
        if (check) check();
        double xyz[3]; uint8_t rgb[3]; uint64_t id = 0;
        if (!input.read(reinterpret_cast<char*>(xyz), sizeof xyz) || !input.read(reinterpret_cast<char*>(rgb), sizeof rgb))
            throw std::runtime_error("truncated dense cloud in the reprojection check");
        if (!read_disk_record(ids, id) || id >= ranges.size()) throw std::runtime_error("dense exported point has no observations");
        double shifted[3], raw[3];
        for (int c = 0; c < 3; ++c) shifted[c] = xyz[c] - m[c * 4 + 3];
        for (int c = 0; c < 3; ++c) raw[c] = inverse[c*3]*shifted[0] + inverse[c*3+1]*shifted[1] + inverse[c*3+2]*shifted[2];
        const sfm::Vec3 point{raw[0] - center[0], raw[1] - center[1], raw[2] - center[2]};
        const auto range = ranges.get(id);
        for (uint64_t k = 0; k < range.count; ++k) {
            const auto o = observations.get(range.offset + k);
            if (o.view >= views.size()) throw std::runtime_error("dense observation names an unknown view");
            const View& view = views[o.view];
            sfm::Vec2 projected;
            if (!project(view, point, projected)) { histogram.add_invalid(); continue; }
            const auto r = pixel_residual(view, projected, {o.pixel[0], o.pixel[1]});
            if (!std::isfinite(r.x) || !std::isfinite(r.y)) { histogram.add_invalid(); continue; }
            const double sx = view.source_camera ? view.grid_scale[0] : (double)grid_width / view.camera.width;
            const double sy = view.source_camera ? view.grid_scale[1] : (double)grid_height / view.camera.height;
            histogram.add(std::hypot(r.x, r.y), std::hypot(r.x * sx, r.y * sy));
        }
    }
    return histogram.summary();
}

}  // namespace spirula::dense
