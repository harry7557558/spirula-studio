// Generic LO-RANSAC with MSAC scoring (src/sfm/README.md).
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace sfm {

struct RansacOptions {
    double max_error = 4.0;          // inlier threshold in the residual's units (px)
    double confidence = 0.999;
    int min_num_trials = 100;
    int max_num_trials = 10000;
    double min_inlier_ratio = 0.0;   // give up early if clearly below this
    unsigned seed = 0;
    int lo_iters = 10;               // local-optimization refit rounds
};

template <class Model>
struct RansacReport {
    Model model{};
    std::vector<char> inlier_mask;   // per point
    int num_inliers = 0;
    double score = 0;                // MSAC (lower is better)
    bool success = false;
    int trials = 0;
};

template <class Model>
using FitFn = std::function<std::vector<Model>(const std::vector<int>&)>;
template <class Model>
using ResidualFn = std::function<double(const Model&, int)>;

template <class Res, class Model, class = void>
struct HasPrepare : std::false_type {};
template <class Res, class Model>
struct HasPrepare<Res, Model, std::void_t<decltype(std::declval<const Res&>().prepare(
    std::declval<const Model&>()))>> : std::true_type {};

// A tied inlier count can win on MSAC; only strictly worse counts may bail.
// The mask is valid only when scoring completes.
template <class Model, class Res>
static double scoreModel(const Model& m, int n, double thr2, const Res& res,
                         std::vector<char>& inliers, int& count, int need = 0) {
    inliers.resize(n);
    if constexpr (HasPrepare<Res, Model>::value) {
        res.prepare(m);
    }
    count = 0;
    double score = 0;
    for (int i = 0; i < n; i++) {
        double r2 = res(m, i);
        if (r2 < thr2) {
            inliers[i] = 1;
            count++;
            score += r2;
        } else {
            inliers[i] = 0;
            score += thr2;
            if (count + (n - 1 - i) < need) { count = -1; return score; }
        }
    }
    return score;
}

// The non-minimal solver, handed the incumbent model when it takes one: a
// local optimization that refines rather than refits needs somewhere to start,
// and the inlier set alone does not say where.
template <class Refit, class Model>
static auto refitModels(const Refit& refit, const std::vector<int>& idx, const Model& m) {
    if constexpr (std::is_invocable_v<Refit, const std::vector<int>&, const Model&>)
        return refit(idx, m);
    else
        return refit(idx);
}

template <class Model, class Fit, class Refit, class Res>
RansacReport<Model> loransac(int n, int min_samples, const Fit& fit, const Refit& refit,
                             const Res& res, const RansacOptions& opt) {
    RansacReport<Model> best;
    best.score = 1e300;
    if (n < min_samples) return best;

    const double thr2 = opt.max_error * opt.max_error;
    std::mt19937 rng(opt.seed);
    std::uniform_int_distribution<int> uni(0, n - 1);

    std::vector<char> inl;
    auto consider = [&](const Model& m) {
        int cnt = 0;
        double sc = scoreModel(m, n, thr2, res, inl, cnt, best.num_inliers);
        if (cnt < 0) return;  // bailed: cannot reach the incumbent's inlier count
        if (cnt > best.num_inliers || (cnt == best.num_inliers && sc < best.score)) {
            best.model = m;
            best.inlier_mask.swap(inl);
            best.num_inliers = cnt;
            best.score = sc;
            best.success = cnt >= min_samples;
        }
    };

    std::vector<int> lo_idx;
    auto localOptimize = [&](int rounds) {
        for (int it = 0; it < rounds; it++) {
            lo_idx.clear();
            for (int i = 0; i < n; i++)
                if (best.inlier_mask[i]) lo_idx.push_back(i);
            if ((int)lo_idx.size() < min_samples) break;
            const int before = best.num_inliers;
            for (const Model& m : refitModels(refit, lo_idx, best.model)) consider(m);
            if (best.num_inliers <= before) break;  // converged
        }
    };

    int max_trials = opt.max_num_trials;
    int trial = 0;
    std::vector<int> sample;
    sample.reserve(min_samples);
    for (; trial < max_trials; trial++) {
        sample.clear();
        while ((int)sample.size() < min_samples) {
            int idx = uni(rng);
            if (std::find(sample.begin(), sample.end(), idx) == sample.end()) sample.push_back(idx);
        }
        const int before = best.num_inliers;
        for (const Model& m : fit(sample)) consider(m);
        // Improving the incumbent here lowers the confidence-bound trial count.
        if (opt.lo_iters > 0 && best.num_inliers > before && best.success) localOptimize(1);

        if (best.num_inliers > before) {
            double w = (double)best.num_inliers / n;
            double denom = std::log(1.0 - std::pow(w, min_samples));
            if (denom < 0) {
                double need = std::log(1.0 - opt.confidence) / denom;
                int cap = opt.max_num_trials;
                if (need < opt.max_num_trials) {
                    cap = std::max(opt.min_num_trials, (int)std::ceil(need));
                }
                max_trials = std::min(max_trials, cap);
            }
        }
    }

    if (best.success) localOptimize(opt.lo_iters);
    best.trials = trial;
    if (opt.min_inlier_ratio > 0 && (double)best.num_inliers / n < opt.min_inlier_ratio)
        best.success = false;
    return best;
}

}  // namespace sfm
