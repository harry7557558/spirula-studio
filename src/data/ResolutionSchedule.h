#pragma once

// Progressive training resolution: which image divisor each step trains at.
// A divisor applies on top of the resolution the dataset was loaded at, and
// switches on epoch boundaries, so every image is trained the same number of
// times at each resolution. The splat budget can follow the same stages.
// docs/progressive-resolution.md has the user view.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace progressive {

// (first step or epoch, divisor), ascending, divisors non-increasing.
using Setpoints = std::vector<std::pair<int64_t, int>>;

// A dimension divided and floored, never below one pixel. The intrinsics scale
// by the realized ratio, so the image and its camera stay consistent.
inline int scaled_extent(int extent, int divisor) {
    return divisor <= 1 ? extent : std::max(1, extent / divisor);
}

// Coarsest first, halving to full: with start 4, the 1/4 and 1/2 stages end
// at 1/3 and 3/3 of `full_at`. Each stage is twice as long as the one before,
// since a finer step costs about four times as much.
inline Setpoints automatic_setpoints(int start_divisor, double full_at, int64_t num_iterations) {
    if (start_divisor < 2 || (start_divisor & (start_divisor - 1)))
        throw std::runtime_error("progressive_resolution_start must be a power of two of at least 2");
    if (!(full_at > 0.0) || full_at > 1.0)
        throw std::runtime_error("progressive_resolution_full_at must be in (0, 1]");
    int stages = 0;
    for (int d = start_divisor; d > 1; d /= 2) ++stages;
    const double end = full_at * (double)num_iterations, total = (double)((1 << stages) - 1);
    Setpoints out{{0, start_divisor}};
    for (int k = 1; k <= stages; ++k)
        out.emplace_back((int64_t)std::llround(end * (double)((1 << k) - 1) / total), start_divisor >> k);
    return out;
}

// "step:divisor" pairs separated by commas or spaces, e.g. "0:4, 3000:2, 9000:1".
inline Setpoints parse_setpoints(const std::string& text) {
    std::string flat = text;
    std::replace(flat.begin(), flat.end(), ',', ' ');
    std::istringstream in(flat);
    Setpoints out;
    std::string item;
    while (in >> item) {
        const auto colon = item.find(':');
        size_t used_step = 0, used_div = 0;
        long long step = -1; long divisor = 0;
        try {
            if (colon == std::string::npos) throw std::invalid_argument(item);
            step = std::stoll(item.substr(0, colon), &used_step);
            divisor = std::stol(item.substr(colon + 1), &used_div);
        } catch (const std::exception&) { used_step = 0; }
        if (colon == std::string::npos || used_step != colon || used_div != item.size() - colon - 1 ||
            step < 0 || divisor < 1 || divisor > 64)
            throw std::runtime_error("progressive_resolution_schedule: '" + item +
                                     "' is not step:divisor (a step >= 0 and a whole divisor 1..64)");
        out.emplace_back(step, (int)divisor);
    }
    if (out.empty()) throw std::runtime_error("progressive_resolution_schedule is empty");
    std::sort(out.begin(), out.end());
    for (size_t i = 1; i < out.size(); ++i) {
        if (out[i].first == out[i - 1].first)
            throw std::runtime_error("progressive_resolution_schedule names step " +
                                     std::to_string(out[i].first) + " twice");
        if (out[i].second > out[i - 1].second)
            throw std::runtime_error("progressive_resolution_schedule must only get finer: a divisor may not grow");
    }
    if (out.front().first != 0) out.insert(out.begin(), {0, out.front().second});
    return out;
}

// Steps to epochs: each switch moves to the nearest epoch boundary. Stages that
// collapse onto one boundary keep the finest; equal neighbours merge.
inline Setpoints to_epochs(const Setpoints& steps, int64_t steps_per_epoch) {
    const int64_t per = std::max<int64_t>(1, steps_per_epoch);
    Setpoints out;
    for (const auto& [step, divisor] : steps) {
        const int64_t epoch = (step + per / 2) / per;
        if (!out.empty() && out.back().first == epoch) out.back().second = divisor;
        else out.emplace_back(epoch, divisor);
    }
    Setpoints merged;
    for (const auto& s : out)
        if (merged.empty() || merged.back().second != s.second) merged.push_back(s);
    return merged;
}

// 1 when the schedule is empty or `at` precedes it.
inline int divisor_at(const Setpoints& stages, int64_t at) {
    int divisor = 1;
    for (const auto& [first, d] : stages) {
        if (first > at) break;
        divisor = d;
    }
    return divisor;
}

// (first step, splats), ascending and never shrinking: how far growth may go.
using Budget = std::vector<std::pair<int64_t, int64_t>>;

// From `seed` splats to `cap` by the same ratio each stage, reaching `cap` at the
// last: growth is multiplicative, so equal ratios are equal shares of the work.
inline Budget automatic_budget(const Setpoints& step_stages, int64_t seed, int64_t cap) {
    Budget out;
    const double n = (double)step_stages.size();
    const bool spread = seed > 0 && seed < cap && n > 0;
    for (size_t k = 0; k < step_stages.size(); ++k) {
        const int64_t at = spread ? (int64_t)std::llround((double)seed * std::pow((double)cap / (double)seed, (double)(k + 1) / n))
                                  : cap;
        out.emplace_back(step_stages[k].first, std::clamp<int64_t>(at, 1, std::max<int64_t>(cap, 1)));
    }
    if (!out.empty()) out.back().second = std::max<int64_t>(cap, 1);
    return out;
}

// One budget value: a splat count ("500000"), a fraction of `cap` ("0.25") or a
// percent ("25%"). False when `text` is none of these.
inline bool budget_value(const std::string& text, int64_t cap, int64_t& out) {
    std::string t = text;
    while (!t.empty() && (t.back() == ' ' || t.back() == '	')) t.pop_back();
    while (!t.empty() && (t.front() == ' ' || t.front() == '	')) t.erase(t.begin());
    const bool percent = !t.empty() && t.back() == '%';
    if (percent) t.pop_back();
    size_t used = 0;
    double v = 0.0;
    try { v = std::stod(t, &used); } catch (const std::exception&) { return false; }
    if (used != t.size() || !(v > 0.0) || (percent && v > 100.0)) return false;
    const double splats = percent ? v / 100.0 * (double)cap : v <= 1.0 ? v * (double)cap : v;
    out = std::clamp<int64_t>((int64_t)std::llround(splats), 1, std::max<int64_t>(cap, 1));
    return true;
}

// One value per resolution stage, comma separated; a blank entry, or a stage past
// the last entry, takes the automatic value. Ceilings may only grow.
inline Budget stage_budget(const Setpoints& step_stages, const std::string& text, int64_t seed, int64_t cap) {
    Budget out = automatic_budget(step_stages, seed, cap);
    std::vector<std::string> items(1);
    for (char c : text) {
        if (c == ',') items.emplace_back();
        else items.back() += c;
    }
    if (items.size() > out.size())
        throw std::runtime_error("progressive_splat_budget_schedule has " + std::to_string(items.size()) +
                                 " values for " + std::to_string(out.size()) + " resolution stages");
    for (size_t k = 0; k < items.size(); ++k) {
        if (items[k].find_first_not_of(" 	") == std::string::npos) continue;
        if (!budget_value(items[k], cap, out[k].second))
            throw std::runtime_error("progressive_splat_budget_schedule: '" + items[k] +
                                     "' is not a splat count, a fraction of the maximum or a percent");
    }
    for (size_t k = 1; k < out.size(); ++k)
        if (out[k].second < out[k - 1].second)
            throw std::runtime_error("progressive_splat_budget_schedule may only grow: a later stage's budget is smaller");
    return out;
}

// The budget a run follows: automatic when `text` is empty, explicit steps when it
// holds step:splats pairs, else one value per stage.
inline Budget resolve_budget(const Setpoints& step_stages, const std::string& text, int64_t seed, int64_t cap);

// "step:splats" pairs, splats a whole number or a percent of `cap`, e.g.
// "0:25%, 3000:500000, 9000:100%". Clamped to `cap`; may only grow.
inline Budget parse_budget(const std::string& text, int64_t cap) {
    std::string flat = text;
    std::replace(flat.begin(), flat.end(), ',', ' ');
    std::istringstream in(flat);
    Budget out;
    std::string item;
    while (in >> item) {
        const auto colon = item.find(':');
        const bool percent = !item.empty() && item.back() == '%';
        const size_t end = item.size() - (percent ? 1 : 0);
        size_t used_step = 0, used_n = 0;
        long long step = -1;
        double n = -1.0;
        try {
            if (colon == std::string::npos) throw std::invalid_argument(item);
            step = std::stoll(item.substr(0, colon), &used_step);
            n = std::stod(item.substr(colon + 1, end - colon - 1), &used_n);
        } catch (const std::exception&) { used_step = 0; }
        if (colon == std::string::npos || used_step != colon || used_n != end - colon - 1 ||
            step < 0 || !(n > 0.0) || (percent && n > 100.0))
            throw std::runtime_error("progressive_splat_budget_schedule: '" + item +
                                     "' is not step:splats (a step >= 0 and a splat count or a percent up to 100%)");
        const double splats = percent ? n / 100.0 * (double)cap : n;
        out.emplace_back(step, std::clamp<int64_t>((int64_t)std::llround(splats), 1, std::max<int64_t>(cap, 1)));
    }
    if (out.empty()) throw std::runtime_error("progressive_splat_budget_schedule is empty");
    std::sort(out.begin(), out.end());
    for (size_t i = 1; i < out.size(); ++i) {
        if (out[i].first == out[i - 1].first)
            throw std::runtime_error("progressive_splat_budget_schedule names step " +
                                     std::to_string(out[i].first) + " twice");
        if (out[i].second < out[i - 1].second)
            throw std::runtime_error("progressive_splat_budget_schedule may only grow: a later budget is smaller");
    }
    if (out.front().first != 0) out.insert(out.begin(), {0, out.front().second});
    return out;
}

inline Budget resolve_budget(const Setpoints& step_stages, const std::string& text, int64_t seed, int64_t cap) {
    if (text.find_first_not_of(" ,	") == std::string::npos && text.find(',') == std::string::npos)
        return automatic_budget(step_stages, seed, cap);
    if (text.find(':') != std::string::npos) return parse_budget(text, cap);
    return stage_budget(step_stages, text, seed, cap);
}

}  // namespace progressive
