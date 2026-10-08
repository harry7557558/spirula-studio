#include "dense/PairSelection.h"

#include "data/Knn.h"
#include "sfm/feature/Pairing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <unordered_map>
#include <stdexcept>

namespace spirula::dense {
namespace {

bool finite(const sfm::Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

double pose_affinity(const PairImage& a, const PairImage& b) {
    const sfm::Vec3 delta = b.center - a.center;
    const double baseline = delta.norm();
    const sfm::Vec3 direction = delta * (1 / baseline);
    const double angle = std::acos(std::clamp(a.forward.dot(b.forward), -1.0, 1.0));
    const double cone = std::max(0.0, 1 - angle / (a.half_fov_radians + b.half_fov_radians));
    const double facing = std::max(0.0, std::min(a.forward.dot(direction), -b.forward.dot(direction)));
    return (0.01 + std::max(cone, facing)) / baseline;
}

struct RankedPair {
    ImagePair pair;
    uint64_t common = 0;
    double affinity = 0, score = 0;
};

bool eligible_pair(const std::vector<PairImage>& images, ImagePair pair) {
    return pair.first != pair.second && images[pair.first].source_image != images[pair.second].source_image &&
        (images[pair.first].center - images[pair.second].center).norm() > 0;
}

// Without shared points, a pose neighbour is worth matching only when the two can see the same region.
bool sees_same_region(const PairImage& a, const PairImage& b) {
    const sfm::Vec3 direction = (b.center - a.center).normalized();
    const double angle = std::acos(std::clamp(a.forward.dot(b.forward), -1.0, 1.0));
    return angle < a.half_fov_radians + b.half_fov_radians || (a.forward.dot(direction) > 0 && b.forward.dot(direction) < 0);
}

// A shared point seen from the two centres at this angle or wider counts fully;
// narrower ones count by the angle squared (Goesele et al. 2007, view selection).
constexpr double kFullWeightAngle = 10 * 3.141592653589793 / 180;
constexpr size_t kScoredPoints = 1024, kScoredTrack = 256, kPoseCandidates = 48;
// Reference coverage: a view stays a reference while this share of its point cells is
// under-covered, and always when it has fewer cells than this.
constexpr double kFreshShare = 0.1;
constexpr size_t kFewPoints = 32;
// The coverage cell's side, as a share of the median distance from a camera to its points.
constexpr double kCoverageCell = 0.02;

// Near-linear in views and observations: candidates come from the sparse tracks and the
// nearest camera centres, never from a scan over every other view.
void automatic_pairs(const std::vector<PairImage>& images, const PairOptions& options, const std::vector<uint32_t>& order,
                     const std::vector<sfm::Vec3>& points, const std::function<bool(ImagePair)>& publish,
                     PairStatistics& statistics, const std::function<void()>& check_cancel) {
    const size_t n = images.size();
    if (n < 2) return;
    const size_t k = std::min<size_t>((size_t)options.neighbors, n - 1);
    auto identity = [&](uint32_t i) { return std::make_pair(images[i].source_image, images[i].face); };
    auto better = [&](const RankedPair& a, const RankedPair& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.common != b.common) return a.common > b.common;
        if (a.affinity != b.affinity) return a.affinity > b.affinity;
        return std::minmax(identity(a.pair.first), identity(a.pair.second)) <
               std::minmax(identity(b.pair.first), identity(b.pair.second));
    };
    std::vector<std::pair<uint64_t, uint32_t>> observations;
    size_t observation_count = 0;
    for (const auto& image : images) observation_count += image.visible_points.size();
    observations.reserve(observation_count);
    for (uint32_t i = 0; i < n; ++i)
        for (uint64_t point : images[i].visible_points) observations.emplace_back(point, i);
    std::sort(observations.begin(), observations.end());
    std::vector<uint64_t> track_points;
    std::vector<size_t> offsets;
    for (size_t i = 0; i < observations.size(); ++i)
        if (i == 0 || observations[i].first != observations[i - 1].first) {
            track_points.push_back(observations[i].first);
            offsets.push_back(i);
        }
    offsets.push_back(observations.size());
    sfm::Vec3 mean{};
    for (const auto& image : images) mean = mean + image.center * (1.0 / n);
    std::vector<float> centers(n * 3);
    for (size_t i = 0; i < n; ++i) {
        const auto c = images[i].center - mean;
        centers[i * 3] = (float)c.x; centers[i * 3 + 1] = (float)c.y; centers[i * 3 + 2] = (float)c.z;
    }
    const knn::KdTree3 tree(centers.data(), (int64_t)n);
    const int nearest_count = (int)std::min<size_t>(n - 1, kPoseCandidates);
    std::vector<float> nearest_distance((size_t)nearest_count);
    std::vector<int32_t> nearest((size_t)nearest_count);
    std::vector<char> reference(n, 1);
    if (options.directed && !options.references.empty()) {
        std::fill(reference.begin(), reference.end(), 0);
        for (uint32_t r : options.references) if (r < n) reference[r] = 1;
    } else if (options.reference_coverage > 0 && !options.directed) {
        // A view whose points earlier references already cover adds nothing of its own; one
        // with almost no points might see untextured surface, so it always stays a reference.
        // Coverage is kept per small cell of space, since SfM breaks one surface into many short tracks.
        std::fill(reference.begin(), reference.end(), 0);
        std::vector<uint64_t> cell(track_points.size());
        for (size_t i = 0; i < track_points.size(); ++i) cell[i] = track_points[i];
        if (!points.empty()) {
            std::vector<double> depths;
            for (uint32_t a : order) {
                const auto& visible = images[a].visible_points;
                if (!visible.empty()) depths.push_back((points[visible[visible.size() / 2]] - images[a].center).norm());
            }
            std::nth_element(depths.begin(), depths.begin() + depths.size() / 2, depths.end());
            const double size = depths.empty() ? 0 : kCoverageCell * depths[depths.size() / 2];
            if (size > 0)
                for (size_t i = 0; i < track_points.size(); ++i) {
                    const auto& x = points[track_points[i]];
                    uint64_t key = 1469598103934665603ull;
                    for (double v : {x.x, x.y, x.z}) { key ^= (uint64_t)(int64_t)std::floor(v / size); key *= 1099511628211ull; }
                    cell[i] = key;
                }
        }
        std::unordered_map<uint64_t, uint8_t> coverage;
        std::vector<uint64_t> cells;
        for (uint32_t a : order) {
            if (check_cancel) check_cancel();
            cells.clear();
            for (uint64_t point : images[a].visible_points)
                cells.push_back(cell[(size_t)(std::lower_bound(track_points.begin(), track_points.end(), point) - track_points.begin())]);
            std::sort(cells.begin(), cells.end());
            cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
            size_t fresh = 0;
            for (uint64_t c : cells) { const auto it = coverage.find(c); fresh += it == coverage.end() || it->second < options.reference_coverage; }
            reference[a] = cells.size() < kFewPoints || fresh >= kFreshShare * cells.size();
            if (reference[a]) for (uint64_t c : cells) { auto& count = coverage[c]; if (count < 255) ++count; }
        }
    }
    std::vector<double> score(n, 0);
    std::vector<uint64_t> common(n, 0);
    std::vector<uint32_t> touched;
    std::vector<std::vector<RankedPair>> chosen(n);
    std::vector<RankedPair> candidates;
    for (uint32_t a : order) {
        if (!reference[a]) continue;
        ++statistics.references;
        if (check_cancel) check_cancel();
        const auto& visible = images[a].visible_points;
        const size_t step = std::max<size_t>(1, visible.size() / kScoredPoints);
        for (size_t i = 0; i < visible.size(); i += step) {
            const uint64_t point = visible[i];
            const size_t index = (size_t)(std::lower_bound(track_points.begin(), track_points.end(), point) - track_points.begin());
            const size_t begin = offsets[index], end = offsets[index + 1];
            const size_t track_step = std::max<size_t>(1, (end - begin) / kScoredTrack);
            for (size_t j = begin; j < end; j += track_step) {
                const uint32_t b = observations[j].second;
                if (b == a || images[b].source_image == images[a].source_image) continue;
                double weight = 1;
                if (!points.empty()) {
                    const auto ra = points[point] - images[a].center, rb = points[point] - images[b].center;
                    const double norms = ra.norm() * rb.norm();
                    const double angle = norms > 0 ? std::acos(std::clamp(ra.dot(rb) / norms, -1.0, 1.0)) : 0;
                    weight = std::min(1.0, (angle / kFullWeightAngle) * (angle / kFullWeightAngle));
                }
                if (!common[b]++) touched.push_back(b);
                score[b] += weight;
            }
        }
        std::vector<RankedPair> best;
        best.reserve(k + 1);
        auto offer = [&](const RankedPair& item) {
            auto at = std::lower_bound(best.begin(), best.end(), item, better);
            if (at != best.end() || best.size() < k) best.insert(at, item);
            if (best.size() > k) best.pop_back();
        };
        auto pair_with = [&](uint32_t b) { return options.directed ? ImagePair{a, b} : ImagePair(std::minmax(a, b)); };
        for (uint32_t b : touched) {
            const ImagePair pair = pair_with(b);
            if (eligible_pair(images, pair)) offer({pair, common[b], pose_affinity(images[a], images[b]), score[b]});
        }
        if (best.size() < k && !images[a].shared_points_only) {
            const int found = tree.query(&centers[(size_t)a * 3], (int32_t)a, nearest_count, nearest_distance.data(), nearest.data());
            for (int i = 0; i < found; ++i) {
                const uint32_t b = (uint32_t)nearest[(size_t)i];
                if (common[b] || images[b].shared_points_only) continue;
                const ImagePair pair = pair_with(b);
                if (eligible_pair(images, pair) && sees_same_region(images[a], images[b]))
                    offer({pair, 0, pose_affinity(images[a], images[b]), 0});
            }
        }
        for (uint32_t b : touched) { common[b] = 0; score[b] = 0; }
        touched.clear();
        if (options.directed && !options.max_pairs) {
            for (const auto& item : best) if (!publish(item.pair)) return;
        } else {
            candidates.insert(candidates.end(), best.begin(), best.end());
            chosen[a] = std::move(best);
        }
    }
    if (options.directed && !options.max_pairs) return;
    std::sort(candidates.begin(), candidates.end(), better);
    candidates.erase(std::unique(candidates.begin(), candidates.end(), [](const RankedPair& a, const RankedPair& b) {
        return a.pair == b.pair;
    }), candidates.end());
    // With every view a reference both ends are capped, as before reference spacing existed;
    // otherwise only references are, and a neighbour-only view may serve several of them.
    const bool subset = std::find(reference.begin(), reference.end(), 0) != reference.end();
    std::vector<int> degrees(n, 0);
    std::set<ImagePair> emitted;
    auto full = [&](uint32_t view) { return (!subset || reference[view]) && degrees[view] >= options.neighbors; };
    auto accept = [&](ImagePair pair) {
        if (!emitted.insert(pair).second) return true;
        if (!publish(pair)) return false;
        ++degrees[pair.first]; if (!options.directed) ++degrees[pair.second];
        return true;
    };
    for (const auto& candidate : candidates) {
        const auto pair = candidate.pair;
        if (full(pair.first) || (!options.directed && full(pair.second))) continue;
        if (!accept(pair)) return;
    }
    if (!subset) return;
    // A reference whose picks all filled up still gets the two neighbours three-view support needs.
    for (uint32_t a : order)
        for (const auto& candidate : chosen[a]) {
            if (degrees[a] >= std::min(2, options.neighbors)) break;
            if (!accept(candidate.pair)) return;
        }
}
}  // namespace

void PairOptions::validate() const {
    if (mode != PairMode::Automatic && mode != PairMode::Sequential && mode != PairMode::Exhaustive && mode != PairMode::Explicit)
        throw std::runtime_error("unknown dense pair mode");
    if (neighbors <= 0 || sequence_window <= 0)
        throw std::runtime_error("dense neighbor count and sequence window must be positive");
    if (reference_coverage < 0 || reference_coverage > 255)
        throw std::runtime_error("dense reference coverage must be between 0 and 255");
}

bool shares_points(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
    auto i = a.begin(), j = b.begin();
    while (i != a.end() && j != b.end()) {
        if (*i < *j) ++i;
        else if (*j < *i) ++j;
        else return true;
    }
    return false;
}

std::vector<uint32_t> select_references(const std::vector<PairImage>& images, double fraction, uint64_t seed,
                                      const std::function<void()>& check_cancel) {
    if (!(fraction > 0) || fraction > 1 || images.size() > UINT32_MAX)
        throw std::runtime_error("invalid dense reference selection");
    const size_t count = (size_t)std::ceil(images.size() * fraction);
    std::vector<uint32_t> order(images.size()); std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return images[a].source_image < images[b].source_image; });
    if (count == images.size()) return order;
    sfm::Vec3 mean{};
    for (const auto& image : images) mean = mean + image.center * (1.0 / images.size());
    double scale = 0;
    for (const auto& image : images) scale += (image.center - mean).dot(image.center - mean) / images.size();
    if (!(scale > 0)) scale = 1;
    std::vector<double> nearest(images.size(), std::numeric_limits<double>::infinity());
    std::vector<uint32_t> selected;
    uint32_t next = order[seed % order.size()];
    for (size_t i = 0; i < count; ++i) {
        if (check_cancel) check_cancel();
        selected.push_back(next); nearest[next] = -1;
        const auto& a = images[next]; double farthest = -1;
        for (uint32_t b : order) {
            if (nearest[b] < 0) continue;
            const auto position = a.center - images[b].center, forward = a.forward - images[b].forward, up = a.up - images[b].up;
            nearest[b] = std::min(nearest[b], position.dot(position) / scale + forward.dot(forward) + up.dot(up));
            if (nearest[b] > farthest) { farthest = nearest[b]; next = b; }
        }
    }
    std::sort(selected.begin(), selected.end(), [&](uint32_t a, uint32_t b) { return images[a].source_image < images[b].source_image; });
    return selected;
}

PairStatistics select_pairs(const std::vector<PairImage>& images, const PairOptions& options,
                               const std::function<void(ImagePair)>& emit, const std::function<void()>& check_cancel,
                               const std::vector<sfm::Vec3>& points) {
    options.validate();
    if (!emit || images.size() > UINT32_MAX) throw std::runtime_error("invalid dense pair selection request");
    std::set<std::pair<int64_t, int>> identities;
    for (const auto& image : images) {
        if (image.source_image < 0 || !identities.insert({image.source_image, image.face}).second || !finite(image.center) ||
            !finite(image.forward) || std::abs(image.forward.norm() - 1) > 1e-6 ||
            !std::isfinite(image.half_fov_radians) || image.half_fov_radians <= 0 || image.half_fov_radians > 3.141592653589793 ||
            !std::is_sorted(image.visible_points.begin(), image.visible_points.end()) ||
            std::adjacent_find(image.visible_points.begin(), image.visible_points.end()) != image.visible_points.end() ||
            (!points.empty() && !image.visible_points.empty() && image.visible_points.back() >= points.size()))
            throw std::runtime_error("dense pair views need unique identities, finite poses, and sorted distinct visibility tracks");
    }
    PairStatistics statistics;
    auto eligible = [&](ImagePair pair) {
        return pair.first != pair.second && images[pair.first].source_image != images[pair.second].source_image &&
            (images[pair.first].center - images[pair.second].center).norm() > 0;
    };
    auto publish = [&](ImagePair pair) {
        if (check_cancel) check_cancel();
        if (options.max_pairs && statistics.emitted >= options.max_pairs) return false;
        if (!eligible(pair)) { ++statistics.rejected_baseline; return true; }
        emit(pair); ++statistics.emitted;
        return true;
    };
    auto identity = [&](uint32_t i) { return std::make_pair(images[i].source_image, images[i].face); };
    std::vector<uint32_t> order(images.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return identity(a) < identity(b); });
    if (options.mode == PairMode::Exhaustive) {
        for (size_t a = 0; a < order.size(); ++a)
            for (size_t b = a + 1; b < order.size(); ++b)
                if (!publish(std::minmax(order[a], order[b]))) return statistics;
        return statistics;
    }
    std::vector<ImagePair> pairs;
    if (options.mode == PairMode::Sequential) {
        std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
            return images[a].name != images[b].name ? images[a].name < images[b].name : identity(a) < identity(b);
        });
        std::vector<std::string> names;
        for (uint32_t i : order) names.push_back(images[i].name);
        for (const auto& pair : sfm::sequentialPairs((uint32_t)order.size(), options.sequence_window, false, sfm::folderRuns(names)))
            pairs.push_back(std::minmax(order[pair.first], order[pair.second]));
    } else if (options.mode == PairMode::Explicit) {
        pairs = options.explicit_pairs;
        for (auto& pair : pairs) {
            if (pair.first >= images.size() || pair.second >= images.size()) throw std::runtime_error("dense pair list references an unknown image");
            if (pair.first > pair.second) std::swap(pair.first, pair.second);
        }
    } else {
        automatic_pairs(images, options, order, points, publish, statistics, check_cancel);
        return statistics;
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    std::sort(pairs.begin(), pairs.end(), [&](ImagePair a, ImagePair b) {
        const auto x = std::minmax(identity(a.first), identity(a.second)), y = std::minmax(identity(b.first), identity(b.second));
        return x < y;
    });
    for (const auto& pair : pairs) if (!publish(pair)) break;
    return statistics;
}

}  // namespace spirula::dense
