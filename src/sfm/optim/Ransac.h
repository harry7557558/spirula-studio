// Generic LO-RANSAC with MSAC scoring (src/sfm/README.md).
//
// Estimator-agnostic: the caller supplies a minimal solver (`fit`, exactly
// `min_samples` points, may return several candidates), a non-minimal solver
// (`refit`, all current inliers, for the local-optimization step), and a
// squared-residual function. Scoring is MSAC (truncated quadratic), inliers are
// residual^2 < max_error^2, and the trial count adapts to the best inlier ratio
// (COLMAP's defaults: confidence 0.999, 100..10000 trials).
//
// SPRT was measured and rejected (D26): residual evaluation is a few percent of
// RANSAC's cost, so a statistical test that stops scoring early buys almost
// nothing. What does buy something, and is exact rather than statistical, is
// the bail in `scoreModel` -- a model that cannot reach the incumbent's inlier
// count stops being scored -- and running the local optimization *inside* the
// trial loop, which raises the inlier ratio the stopping rule reads.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <type_traits>
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
struct HasPrepare<Res, Model, std::void_t<decltype(std::declval<Res&>().prepare(std::declval<const Model&>()))>> : std::true_type {};

// Score a model over all points; bail early when remaining points cannot beat `need`.
// Writes inliers during the pass without pre-zeroing; mask is valid whenever scoring completes.
template <class Model, class Res>
static double scoreModel(const Model& m, int n, double thr2, const Res& res,
                         std::vector<char>& inliers, int& count, int need = 0) {
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

// `fit` / `refit` / `res` are deduced, so callers pass lambdas and everything
// inlines; the FitFn / ResidualFn aliases above still work for a caller that
// wants a type-erased one. Model stays explicit at the call sites.
template <class Model, class Fit, class Refit, class Res>
RansacReport<Model> loransac(int n, int min_samples, const Fit& fit, const Refit& refit,
                             const Res& res, const RansacOptions& opt) {
    RansacReport<Model> best;
    best.score = 1e300;
    if (n < min_samples) return best;

    const double thr2 = opt.max_error * opt.max_error;
    std::mt19937 rng(opt.seed);
    std::uniform_int_distribution<int> uni(0, n - 1);

    std::vector<char> inl(n, 0);
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
            inl.resize(n);
        }
    };

    // Local optimization on the current inliers, `rounds` times or until it
    // stops helping. Used both inside the trial loop (where a better model
    // raises the inlier ratio and so lowers the trial count the stopping rule
    // demands) and once more at the end.
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
        // progressive pool: early trials sample from the top-ranked subset
        const int pool = (trial < 100)
            ? std::min(n, std::max(min_samples + 4, (int)((int64_t)n * (trial + 1) / 100)))
            : n;
        std::uniform_int_distribution<int> pool_uni(0, pool - 1);
        sample.clear();
        while ((int)sample.size() < min_samples) {
            int idx = pool_uni(rng);
            if (std::find(sample.begin(), sample.end(), idx) == sample.end()) sample.push_back(idx);
        }
        const int before = best.num_inliers;
        for (const Model& m : fit(sample)) consider(m);
        // Textbook LO-RANSAC: optimize as soon as the incumbent improves, not
        // only at the end. The refit model explains more of the data, which
        // both raises the inlier ratio the stopping rule reads (fewer trials
        // for the same confidence) and tightens the early-bail threshold
        // (cheaper trials). One round is enough here -- the full ladder runs
        // after the loop.
        if (opt.lo_iters > 0 && best.num_inliers > before && best.success) localOptimize(1);

        // adaptive stopping from the best inlier ratio so far
        if (best.num_inliers > 0) {
            double w = (double)best.num_inliers / n;
            double denom = std::log(1.0 - std::pow(w, min_samples));
            if (denom < 0) {
                double need = std::log(1.0 - opt.confidence) / denom;
                int cap = opt.max_num_trials;
                if (need < opt.max_num_trials) {
                    cap = std::max(opt.min_num_trials, (int)std::ceil(need));
                }
                if (trial + 1 >= cap) { trial++; break; }
            }
        }
    }

    // Final local optimization. (`refit` is a deduced callable rather than a
    // std::function, so there is no "empty" state to test for -- every caller
    // passes a real one, and an estimator with no non-minimal solver passes its
    // minimal one twice.)
    if (best.success) localOptimize(opt.lo_iters);
    best.trials = trial;
    if (opt.min_inlier_ratio > 0 && (double)best.num_inliers / n < opt.min_inlier_ratio)
        best.success = false;
    return best;
}

}  // namespace sfm
