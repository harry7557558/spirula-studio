#pragma once

#include "data/ScenePartition.h"
#include "data/Knn.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>

namespace spirula {

inline std::vector<float> voronoi_fit_positions(const std::vector<float>& cameras, int minimum,
                                               const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) throw PartitionCancelled{};
    for (float value : cameras)
        if (!std::isfinite(value)) throw std::runtime_error("partition: non-finite camera position");
    const size_t n = cameras.size() / 3;
    if (n < 32) return cameras;
    const knn::KdTree3 tree(cameras.data(), (int64_t)n);
    std::vector<float> distance(n), positive;
    for (size_t i = 0; i < n; i++) {
        if (i % 1024 == 0 && cancel && cancel->load()) throw PartitionCancelled{};
        float d[8];
        int32_t ids[8];
        const int found = tree.query(cameras.data() + i * 3, (int32_t)i, 8, d, ids);
        distance[i] = found ? *std::max_element(d, d + found) : 0;
        if (distance[i] > 0) positive.push_back(distance[i]);
    }
    if (positive.empty()) return cameras;
    const size_t middle = positive.size() / 2;
    std::nth_element(positive.begin(), positive.begin() + middle, positive.end());
    const double threshold = 16.0 * 16.0 * positive[middle];
    std::vector<size_t> isolated;
    for (size_t i = 0; i < n; i++) if (distance[i] > threshold) isolated.push_back(i);
    if (isolated.empty()) return cameras;
    std::sort(isolated.begin(), isolated.end(), [&](size_t a, size_t b) {
        return distance[a] != distance[b] ? distance[a] > distance[b] : a < b;
    });
    isolated.resize(std::min(isolated.size(), n / 50));
    if (n - isolated.size() < (size_t)minimum) return cameras;
    std::vector<bool> omit(n, false);
    for (size_t i : isolated) omit[i] = true;
    std::vector<float> fit;
    fit.reserve((n - isolated.size()) * 3);
    for (size_t i = 0; i < n; i++)
        if (!omit[i]) fit.insert(fit.end(), cameras.begin() + i * 3, cameras.begin() + i * 3 + 3);
    std::set<std::array<float, 3>> distinct;
    for (size_t i = 0; i < fit.size() && distinct.size() < (size_t)minimum; i += 3)
        distinct.insert({fit[i], fit[i + 1], fit[i + 2]});
    if (distinct.size() < (size_t)minimum) return cameras;
    return fit;
}

inline std::vector<float> voronoi_centers(const std::vector<float>& cameras, int parts,
                                         const std::atomic<bool>* cancel) {
    const size_t n = cameras.size() / 3;
    if (n == 0 || cameras.size() % 3 || parts < 1 || parts > 254 || (size_t)parts > n)
        throw std::runtime_error("partition: invalid Voronoi camera count or part count");
    for (float value : cameras)
        if (!std::isfinite(value)) throw std::runtime_error("partition: non-finite camera position");
    std::vector<float> centers(cameras.begin(), cameras.begin() + 3);
    std::vector<double> distance(n, std::numeric_limits<double>::infinity());
    auto squared = [&](size_t i, const float* c) {
        double d = 0;
        for (int r = 0; r < 3; r++) {
            const double v = (double)cameras[i * 3 + r] - c[r];
            d += v * v;
        }
        return d;
    };
    for (int k = 1; k < parts; k++) {
        if (cancel && cancel->load()) throw PartitionCancelled{};
        const float* last = centers.data() + (k - 1) * 3;
        for (size_t i = 0; i < n; i++) distance[i] = std::min(distance[i], squared(i, last));
        const size_t farthest = (size_t)(std::max_element(distance.begin(), distance.end()) - distance.begin());
        if (!(distance[farthest] > 0))
            throw std::runtime_error("partition: fewer distinct camera positions than Voronoi parts");
        centers.insert(centers.end(), cameras.begin() + farthest * 3, cameras.begin() + farthest * 3 + 3);
    }
    std::vector<int> labels(n, -1);
    for (int iteration = 0; iteration < 100; iteration++) {
        if (cancel && cancel->load()) throw PartitionCancelled{};
        std::vector<double> sums((size_t)parts * 3, 0);
        std::vector<int64_t> counts((size_t)parts, 0);
        bool changed = false;
        for (size_t i = 0; i < n; i++) {
            int best = 0;
            double d = squared(i, centers.data());
            for (int k = 1; k < parts; k++) {
                const double candidate = squared(i, centers.data() + k * 3);
                if (candidate < d) { d = candidate; best = k; }
            }
            changed |= labels[i] != best;
            labels[i] = best;
            counts[(size_t)best]++;
            for (int r = 0; r < 3; r++) sums[(size_t)best * 3 + r] += cameras[i * 3 + r];
        }
        if (!changed) break;
        for (int k = 0; k < parts; k++) {
            if (!counts[(size_t)k])
                throw std::runtime_error("partition: empty Voronoi cell; request fewer parts");
            for (int r = 0; r < 3; r++)
                centers[(size_t)k * 3 + r] = (float)(sums[(size_t)k * 3 + r] / counts[(size_t)k]);
        }
    }
    return centers;
}

inline void partition_voronoi(ScenePartition& p, const std::atomic<bool>* cancel) {
    const int64_t n = (int64_t)p.frame_centers.size() / 3;
    const auto& opt = p.options;
    int parts = opt.parts > 0 ? opt.parts : (int)((n + std::max(1, opt.max_images) - 1) / std::max(1, opt.max_images));
    for (;;) {
        if (parts > 254 || parts > n) throw std::runtime_error("partition: too many Voronoi parts");
        const auto fit = voronoi_fit_positions(p.frame_centers, parts, cancel);
        const auto centers = voronoi_centers(fit, parts, cancel);
        std::vector<int32_t> labels((size_t)parts);
        std::iota(labels.begin(), labels.end(), 0);
        auto field = LabelField::build(centers.data(), labels.data(), nullptr, parts, 8, true);
        p.frame_label.resize((size_t)n);
        field.labels_of(p.frame_centers.data(), n, p.frame_label.data());
        std::vector<int64_t> counts((size_t)parts, 0);
        for (int32_t label : p.frame_label) {
            if (label < 0 || label >= parts) throw std::runtime_error("partition: invalid Voronoi owner");
            counts[(size_t)label]++;
        }
        if (*std::min_element(counts.begin(), counts.end()) == 0)
            throw std::runtime_error("partition: empty Voronoi cell; request fewer parts");
        if (opt.parts <= 0 && *std::max_element(counts.begin(), counts.end()) > opt.max_images) {
            parts++;
            continue;
        }
        std::vector<int32_t> order = labels;
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return counts[a] > counts[b]; });
        for (int k = 0; k < parts; k++) labels[(size_t)order[(size_t)k]] = k;
        p.field = std::make_shared<LabelField>(LabelField::build(centers.data(), labels.data(), nullptr, parts, 8, true));
        p.field->labels_of(p.frame_centers.data(), n, p.frame_label.data());
        p.num_parts = parts;
        return;
    }
}

}  // namespace spirula
