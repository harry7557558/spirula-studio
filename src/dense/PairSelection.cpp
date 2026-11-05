#include "dense/PairSelection.h"

#include "data/Knn.h"
#include "dense/Filters.h"
#include "sfm/feature/Pairing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

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
// References stop once no view would add this share of a typical view's cells. Measured against
// each view's own count, views that see a sliver qualified and denser captures made more of them.
constexpr double kFreshShare = 0.1;
// A cell's side, as a share of the median distance from the cameras that see its points, rounded
// down to a power of two: a cell is then about the same share of every view that sees it.
constexpr double kCoverageCell = 0.04;
// Cells kept per view (median), by a hash every view agrees on, which bounds planning time.
constexpr size_t kCellsPerView = 512;
// A partner sees a cell for coverage from this angle up, and by the angle squared below it: a
// distant cell that no pair sees widely must not draw partners until the cap.
constexpr double kCoverageAngle = 3 * 3.141592653589793 / 180;
// A reference takes another neighbour only while it adds this much coverage per cell of its view.
constexpr double kMinNeighbourGain = 0.05;
// Partners a reference ranks by shared cells before coverage chooses among them.
constexpr size_t kCandidates = 24;

double pair_angle(const sfm::Vec3& x, const sfm::Vec3& a, const sfm::Vec3& b) {
    const auto ra = x - a, rb = x - b;
    const double norms = ra.norm() * rb.norm();
    return norms > 0 ? std::acos(std::clamp(ra.dot(rb) / norms, -1.0, 1.0)) : 0;
}

double angle_weight(const sfm::Vec3& x, const sfm::Vec3& a, const sfm::Vec3& b) {
    const double angle = pair_angle(x, a, b);
    return std::min(1.0, (angle / kFullWeightAngle) * (angle / kFullWeightAngle));
}

uint64_t mix(uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

// The surface the views see, as cells of sparse points, and which views see each cell.
struct Cells {
    std::vector<std::vector<uint32_t>> of_view;   // sorted cell ids
    std::vector<sfm::Vec3> centroid;              // empty without point positions
    std::vector<uint64_t> begin;                  // views[begin[c] .. begin[c + 1]] see cell c
    std::vector<uint32_t> views;
    size_t count() const { return begin.empty() ? 0 : begin.size() - 1; }
};

Cells surface_cells(const std::vector<PairImage>& images, const std::vector<sfm::Vec3>& points,
                    const std::function<void()>& check_cancel) {
    Cells out;
    const size_t n = images.size();
    out.of_view.resize(n);
    std::vector<uint64_t> ids;
    for (const auto& image : images) ids.insert(ids.end(), image.visible_points.begin(), image.visible_points.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (ids.empty()) return out;
    std::vector<std::vector<uint32_t>> compact(n);
    std::vector<uint64_t> first(ids.size() + 1, 0);
    for (size_t v = 0; v < n; ++v) {
        if (check_cancel) check_cancel();
        auto at = ids.begin();
        for (uint64_t point : images[v].visible_points) {
            at = std::lower_bound(at, ids.end(), point);
            compact[v].push_back((uint32_t)(at - ids.begin()));
            ++first[compact[v].back() + 1];
        }
    }
    std::partial_sum(first.begin(), first.end(), first.begin());
    std::vector<uint32_t> observers(first.back());
    {
        std::vector<uint64_t> fill(first.begin(), first.end() - 1);
        for (size_t v = 0; v < n; ++v) for (uint32_t p : compact[v]) observers[fill[p]++] = (uint32_t)v;
    }
    std::vector<uint64_t> key(ids.size());
    for (size_t p = 0; p < ids.size(); ++p) {
        if (points.empty()) { key[p] = mix(ids[p]); continue; }
        const auto& x = points[ids[p]];
        double distance[8];
        size_t m = 0;
        const uint64_t step = std::max<uint64_t>(1, (first[p + 1] - first[p]) / 8);
        for (uint64_t o = first[p]; o < first[p + 1] && m < 8; o += step) distance[m++] = (x - images[observers[o]].center).norm();
        std::nth_element(distance, distance + m / 2, distance + m);
        const double side = kCoverageCell * distance[m / 2];
        const int level = side > 0 && std::isfinite(side) ? std::ilogb(side) : 0;
        const double size = std::ldexp(1.0, level);
        uint64_t k = mix((uint64_t)(int64_t)level);
        for (double coordinate : {x.x, x.y, x.z}) k = mix(k ^ (uint64_t)(int64_t)std::floor(coordinate / size));
        key[p] = k;
    }
    std::vector<uint64_t> keys = key;
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    std::vector<uint32_t> cell(ids.size());
    for (size_t p = 0; p < ids.size(); ++p) cell[p] = (uint32_t)(std::lower_bound(keys.begin(), keys.end(), key[p]) - keys.begin());
    std::vector<size_t> sizes;
    for (size_t v = 0; v < n; ++v) {
        auto& cells = out.of_view[v];
        for (uint32_t p : compact[v]) cells.push_back(cell[p]);
        std::vector<uint32_t>().swap(compact[v]);
        std::sort(cells.begin(), cells.end());
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        if (!cells.empty()) sizes.push_back(cells.size());
    }
    std::nth_element(sizes.begin(), sizes.begin() + sizes.size() / 2, sizes.end());
    const double keep = std::min(1.0, (double)kCellsPerView / std::max<size_t>(1, sizes[sizes.size() / 2]));
    std::vector<uint32_t> renumber(keys.size(), UINT32_MAX);
    uint32_t kept = 0;
    for (size_t c = 0; c < keys.size(); ++c)
        if ((double)(mix(keys[c] ^ 0x5bd1e995ull) >> 11) * 0x1.0p-53 < keep) renumber[c] = kept++;
    for (auto& cells : out.of_view) {
        size_t o = 0;
        for (uint32_t c : cells) if (renumber[c] != UINT32_MAX) cells[o++] = renumber[c];
        cells.resize(o); cells.shrink_to_fit();
    }
    if (!points.empty()) {
        out.centroid.assign(kept, {});
        std::vector<uint32_t> members(kept, 0);
        for (size_t p = 0; p < ids.size(); ++p)
            if (const uint32_t c = renumber[cell[p]]; c != UINT32_MAX) { out.centroid[c] = out.centroid[c] + points[ids[p]]; ++members[c]; }
        for (uint32_t c = 0; c < kept; ++c) out.centroid[c] = out.centroid[c] * (1.0 / members[c]);
    }
    out.begin.assign((size_t)kept + 1, 0);
    for (const auto& cells : out.of_view) for (uint32_t c : cells) ++out.begin[c + 1];
    std::partial_sum(out.begin.begin(), out.begin.end(), out.begin.begin());
    out.views.resize(out.begin.back());
    std::vector<uint64_t> fill(out.begin.begin(), out.begin.end() - 1);
    for (size_t v = 0; v < n; ++v) for (uint32_t c : out.of_view[v]) out.views[fill[c]++] = (uint32_t)v;
    return out;
}

// Greedy multi-cover: the view that sees the most cells still short of `coverage` references
// goes next. Views that see nothing are never references.
std::vector<uint32_t> cover_references(const Cells& cells, int coverage, const std::vector<uint32_t>& order,
                                       const std::function<void()>& check_cancel) {
    std::vector<uint8_t> count(cells.count(), 0);
    auto gain = [&](uint32_t v) {
        size_t g = 0;
        for (uint32_t c : cells.of_view[v]) g += count[c] < coverage;
        return g;
    };
    // A view's gain only falls as references are added, so a stale bound is still an upper bound.
    std::priority_queue<std::tuple<size_t, int64_t, uint32_t>> queue;
    for (size_t rank = 0; rank < order.size(); ++rank)
        if (!cells.of_view[order[rank]].empty()) queue.emplace(cells.of_view[order[rank]].size(), -(int64_t)rank, order[rank]);
    std::vector<uint32_t> selected;
    std::vector<size_t> sizes;
    for (const auto& c : cells.of_view) if (!c.empty()) sizes.push_back(c.size());
    if (!sizes.empty()) std::nth_element(sizes.begin(), sizes.begin() + sizes.size() / 2, sizes.end());
    const double typical = sizes.empty() ? 1.0 : (double)sizes[sizes.size() / 2];
    while (!queue.empty()) {
        if (check_cancel && queue.size() % 1024 == 0) check_cancel();
        auto [bound, rank, v] = queue.top();
        queue.pop();
        const size_t g = gain(v);
        if ((double)g < std::max(1.0, kFreshShare * typical)) continue;
        if (!queue.empty() && std::make_tuple(g, rank) < std::make_tuple(std::get<0>(queue.top()), std::get<1>(queue.top()))) {
            queue.emplace(g, rank, v);
            continue;
        }
        selected.push_back(v);
        for (uint32_t c : cells.of_view[v]) if (count[c] < 255) ++count[c];
    }
    return selected;
}

struct Shared { uint32_t at_a, at_b; float unit, weight; };

std::vector<Shared> shared_cells(const Cells& cells, uint32_t a, uint32_t b, const std::vector<PairImage>& images) {
    std::vector<Shared> out;
    const auto& x = cells.of_view[a], &y = cells.of_view[b];
    for (size_t i = 0, j = 0; i < x.size() && j < y.size();) {
        if (x[i] < y[j]) ++i;
        else if (y[j] < x[i]) ++j;
        else {
            const double angle = cells.centroid.empty() ? kFullWeightAngle : pair_angle(cells.centroid[x[i]], images[a].center, images[b].center);
            out.push_back({(uint32_t)i, (uint32_t)j, (float)std::min(1.0, (angle / kCoverageAngle) * (angle / kCoverageAngle)),
                           (float)std::min(1.0, (angle / kFullWeightAngle) * (angle / kFullWeightAngle))});
            ++i; ++j;
        }
    }
    return out;
}

// Undirected automatic pairing sized by the surface rather than by the number of views: references
// cover every cell `reference_coverage` times, and each reference takes neighbours until every
// part of its view is matched against `neighbors` of them, weighted by triangulation angle.
void coverage_pairs(const std::vector<PairImage>& images, const PairOptions& options, const std::vector<uint32_t>& order,
                    const std::vector<sfm::Vec3>& points, const std::function<bool(ImagePair)>& publish,
                    PairStatistics& statistics, const std::function<void()>& check_cancel) {
    const size_t n = images.size();
    if (n < 2) return;
    const Cells cells = surface_cells(images, points, check_cancel);
    statistics.cells = cells.count();
    const std::vector<uint32_t> references = options.reference_coverage > 0 ?
        cover_references(cells, options.reference_coverage, order, check_cancel) : order;
    statistics.references = references.size();
    std::vector<int32_t> slot(n, -1);
    std::vector<std::vector<float>> got(references.size());
    for (size_t k = 0; k < references.size(); ++k) {
        slot[references[k]] = (int32_t)k;
        got[k].assign(cells.of_view[references[k]].size(), 0.f);
    }
    const double target = options.neighbors;
    const int cap = 2 * options.neighbors;
    auto identity = [&](uint32_t i) { return std::make_pair(images[i].source_image, images[i].face); };
    auto better = [&](const RankedPair& a, const RankedPair& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.common != b.common) return a.common > b.common;
        if (a.affinity != b.affinity) return a.affinity > b.affinity;
        return std::minmax(identity(a.pair.first), identity(a.pair.second)) <
               std::minmax(identity(b.pair.first), identity(b.pair.second));
    };
    auto allowed = [&](ImagePair pair) {
        return eligible_pair(images, pair) && (options.max_baseline <= 0 ||
            (images[pair.first].center - images[pair.second].center).norm() <= options.max_baseline);
    };
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
    std::vector<int> degree(n, 0);
    std::unordered_set<uint64_t> paired;
    auto pair_key = [](uint32_t a, uint32_t b) { return ((uint64_t)std::min(a, b) << 32) | std::max(a, b); };
    std::vector<double> score(n, 0);
    std::vector<uint64_t> common(n, 0);
    std::vector<uint32_t> touched;
    struct Candidate { uint32_t b; std::vector<Shared> shared; double quality = 0, bound = 0; bool open = true; };
    for (const uint32_t r : references) {
        if (check_cancel) check_cancel();
        const auto& mine = cells.of_view[r];
        for (uint32_t c : mine) {
            const uint64_t begin = cells.begin[c], end = cells.begin[c + 1];
            const uint64_t step = std::max<uint64_t>(1, (end - begin) / kScoredTrack);
            for (uint64_t o = begin; o < end; o += step) {
                const uint32_t b = cells.views[o];
                if (b == r || images[b].source_image == images[r].source_image) continue;
                if (!common[b]++) touched.push_back(b);
                score[b] += cells.centroid.empty() ? 1 : angle_weight(cells.centroid[c], images[r].center, images[b].center);
            }
        }
        std::vector<RankedPair> ranked;
        for (uint32_t b : touched) {
            const ImagePair pair = std::minmax(r, b);
            if (allowed(pair) && !paired.count(pair_key(r, b))) ranked.push_back({pair, common[b], pose_affinity(images[r], images[b]), score[b]});
        }
        if (ranked.size() < (size_t)options.neighbors && !images[r].shared_points_only) {
            const int found = tree.query(&centers[(size_t)r * 3], (int32_t)r, nearest_count, nearest_distance.data(), nearest.data());
            for (int i = 0; i < found; ++i) {
                const uint32_t b = (uint32_t)nearest[(size_t)i];
                if (common[b] || images[b].shared_points_only || paired.count(pair_key(r, b))) continue;
                const ImagePair pair = std::minmax(r, b);
                if (allowed(pair) && sees_same_region(images[r], images[b])) ranked.push_back({pair, 0, pose_affinity(images[r], images[b]), 0});
            }
        }
        for (uint32_t b : touched) { common[b] = 0; score[b] = 0; }
        touched.clear();
        std::sort(ranked.begin(), ranked.end(), better);
        if (ranked.size() > kCandidates) ranked.resize(kCandidates);
        std::vector<Candidate> candidates;
        for (const auto& item : ranked) {
            const uint32_t b = item.pair.first == r ? item.pair.second : item.pair.first;
            Candidate c{b, shared_cells(cells, r, b, images)};
            for (const auto& s : c.shared) c.quality += s.weight / (double)c.shared.size();
            candidates.push_back(std::move(c));
        }
        auto gain = [&](const Candidate& c) {
            double g = 0;
            const auto& own = got[(size_t)slot[r]];
            const int32_t other = slot[c.b];
            for (const auto& s : c.shared) {
                g += std::min<double>(s.unit, std::max(0.0, target - own[s.at_a]));
                if (other >= 0) g += std::min<double>(s.unit, std::max(0.0, target - got[(size_t)other][s.at_b]));
            }
            return g;
        };
        bool stopped = false;
        auto accept = [&](Candidate& c) {
            c.open = false;
            paired.insert(pair_key(r, c.b));
            ++degree[r]; ++degree[c.b];
            const int32_t other = slot[c.b];
            for (const auto& s : c.shared) {
                got[(size_t)slot[r]][s.at_a] += s.unit;
                if (other >= 0) got[(size_t)other][s.at_b] += s.unit;
            }
            if (!publish(std::minmax(r, c.b))) stopped = true;
        };
        // Coverage decides whether a partner is worth a pair; among those, wider angles go first.
        auto priority = [](const Candidate& c, double g) { return g * (0.5 + 0.5 * c.quality); };
        for (auto& c : candidates) c.bound = priority(c, gain(c));
        const double minimum = kMinNeighbourGain * (double)std::max<size_t>(1, mine.size());
        while (!stopped && degree[r] < cap) {
            Candidate* best = nullptr;
            for (auto& c : candidates)
                if (c.open && (!best || c.bound > best->bound)) best = &c;
            if (!best || best->bound < 0.5 * minimum) break;
            const double g = gain(*best);
            if (g < minimum || degree[best->b] >= cap) { best->open = false; continue; }
            const double p = priority(*best, g);
            bool still_best = true;
            for (const auto& c : candidates)
                if (c.open && &c != best && c.bound > p) { still_best = false; break; }
            best->bound = p;
            if (still_best) accept(*best);
        }
        // Three-image support needs two neighbours, whatever they add.
        for (auto& c : candidates) {
            if (stopped || degree[r] >= std::min(2, options.neighbors)) break;
            if (c.open) accept(c);
        }
        if (stopped) return;
    }
}

// Directed (source-image) pairing: each given reference keeps its best-ranked neighbours.
// Candidates come from the sparse tracks and the nearest camera centres, never from a scan
// over every other view.
void directed_pairs(const std::vector<PairImage>& images, const PairOptions& options, const std::vector<uint32_t>& order,
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
    std::vector<char> reference(n, options.references.empty());
    for (uint32_t r : options.references) if (r < n) reference[r] = 1;
    std::vector<double> score(n, 0);
    std::vector<uint64_t> common(n, 0);
    std::vector<uint32_t> touched;
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
                const double weight = points.empty() ? 1 : angle_weight(points[point], images[a].center, images[b].center);
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
        auto allowed = [&](ImagePair pair) {
            return eligible_pair(images, pair) && (options.max_baseline <= 0 ||
                (images[pair.first].center - images[pair.second].center).norm() <= options.max_baseline);
        };
        for (uint32_t b : touched) {
            const ImagePair pair{a, b};
            if (allowed(pair)) offer({pair, common[b], pose_affinity(images[a], images[b]), score[b]});
        }
        if (best.size() < k && !images[a].shared_points_only) {
            const int found = tree.query(&centers[(size_t)a * 3], (int32_t)a, nearest_count, nearest_distance.data(), nearest.data());
            for (int i = 0; i < found; ++i) {
                const uint32_t b = (uint32_t)nearest[(size_t)i];
                if (common[b] || images[b].shared_points_only) continue;
                const ImagePair pair{a, b};
                if (allowed(pair) && sees_same_region(images[a], images[b]))
                    offer({pair, 0, pose_affinity(images[a], images[b]), 0});
            }
        }
        for (uint32_t b : touched) { common[b] = 0; score[b] = 0; }
        touched.clear();
        if (!options.max_pairs) {
            for (const auto& item : best) if (!publish(item.pair)) return;
        } else candidates.insert(candidates.end(), best.begin(), best.end());
    }
    if (!options.max_pairs) return;
    std::sort(candidates.begin(), candidates.end(), better);
    for (const auto& candidate : candidates) if (!publish(candidate.pair)) return;
}

}  // namespace

void PairOptions::validate() const {
    if (mode != PairMode::Automatic && mode != PairMode::Sequential && mode != PairMode::Exhaustive && mode != PairMode::Explicit)
        throw std::runtime_error("unknown dense pair mode");
    if (neighbors <= 0 || sequence_window <= 0)
        throw std::runtime_error("dense neighbor count and sequence window must be positive");
    if (!std::isfinite(max_baseline) || max_baseline < 0)
        throw std::runtime_error("dense maximum pair baseline must be finite and non-negative");
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
        if (options.directed) directed_pairs(images, options, order, points, publish, statistics, check_cancel);
        else coverage_pairs(images, options, order, points, publish, statistics, check_cancel);
        return statistics;
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    std::sort(pairs.begin(), pairs.end(), [&](ImagePair a, ImagePair b) {
        const auto x = std::minmax({identity(a.first), identity(a.second)});
        const auto y = std::minmax({identity(b.first), identity(b.second)});
        return x < y;
    });
    for (const auto& pair : pairs) if (!publish(pair)) break;
    return statistics;
}

SurfaceCoverage measure_coverage(const std::vector<PairImage>& images, const std::vector<ImagePair>& pairs,
                                 const std::vector<sfm::Vec3>& points, bool directed) {
    SurfaceCoverage out;
    const Cells cells = surface_cells(images, points, {});
    out.cells = cells.count();
    std::vector<std::vector<uint32_t>> partners(images.size());
    for (const auto& pair : pairs) {
        partners[pair.first].push_back(pair.second);
        if (!directed) partners[pair.second].push_back(pair.first);
    }
    std::vector<uint32_t> reconstructions(cells.count(), 0);
    std::vector<uint32_t> per_reference;
    for (size_t v = 0; v < images.size(); ++v) {
        if (partners[v].size() < 2) continue;
        for (uint32_t c : cells.of_view[v]) {
            uint32_t seen = 0;
            for (uint32_t u : partners[v])
                seen += std::binary_search(cells.of_view[u].begin(), cells.of_view[u].end(), c);
            if (seen >= 2) { ++reconstructions[c]; per_reference.push_back(seen); }
        }
    }
    std::vector<uint32_t> per_cell;
    for (size_t c = 0; c < cells.count(); ++c)
        if (cells.begin[c + 1] - cells.begin[c] >= 3) per_cell.push_back(reconstructions[c]);
    auto median = [](std::vector<uint32_t>& v) {
        if (v.empty()) return 0.0;
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return (double)v[v.size() / 2];
    };
    auto mean = [](const std::vector<uint32_t>& v) {
        return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / (double)v.size();
    };
    out.covered = per_cell.size() - (size_t)std::count(per_cell.begin(), per_cell.end(), 0u);
    out.mean_references = mean(per_cell); out.mean_neighbors = mean(per_reference);
    out.references = median(per_cell); out.neighbors = median(per_reference);
    out.reconstructable = per_cell.size();
    return out;
}

double resolve_max_baseline(const std::vector<PairImage>& images, const PairOptions& options, double setting,
                            bool metric, uint64_t* refused, const std::function<void()>& check_cancel,
                            const std::vector<sfm::Vec3>& points) {
    if (refused) *refused = 0;
    if (options.mode != PairMode::Automatic || setting < 0 || (setting == 0 && !metric)) return 0;
    auto unlimited = options; unlimited.max_baseline = 0;
    // One baseline per pair of source images, however many of their faces were paired.
    std::set<std::pair<int64_t,int64_t>> seen;
    std::vector<double> baselines;
    select_pairs(images, unlimited, [&](ImagePair pair) {
        if (seen.insert(std::minmax(images[pair.first].source_image, images[pair.second].source_image)).second)
            baselines.push_back((images[pair.first].center - images[pair.second].center).norm());
    }, check_cancel, points);
    const double limit = setting > 0 ? setting : automatic_max_baseline(baselines);
    if (refused) for (double b : baselines) *refused += limit > 0 && b > limit;
    return limit;
}

}  // namespace spirula::dense
