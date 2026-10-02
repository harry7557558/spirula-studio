#include "checkpoint/SplatPrune.h"

#include "checkpoint/SplatPly.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace spirula {

std::vector<uint8_t> select_splats_by_contribution(
    const std::vector<float>& contribution, double keep_fraction) {
    if (!(keep_fraction > 0.0 && keep_fraction <= 1.0) || !std::isfinite(keep_fraction))
        throw std::runtime_error("--keep-fraction: expected a number in (0, 1]");
    const int64_t n = (int64_t)contribution.size();
    const int64_t take = (int64_t)std::ceil(keep_fraction * (double)n);
    std::vector<uint8_t> keep((size_t)n, 0);
    if (!take) return keep;

    std::vector<int64_t> order((size_t)n);
    std::iota(order.begin(), order.end(), 0);
    auto before = [&](int64_t a, int64_t b) {
        const float va = std::isfinite(contribution[(size_t)a]) ? contribution[(size_t)a] : 0.0f;
        const float vb = std::isfinite(contribution[(size_t)b]) ? contribution[(size_t)b] : 0.0f;
        return va != vb ? va > vb : a < b;
    };
    if (take < n)
        std::nth_element(order.begin(), order.begin() + take, order.end(), before);
    for (int64_t i = 0; i < take; ++i) keep[(size_t)order[(size_t)i]] = 1;
    return keep;
}

std::vector<uint8_t> select_splats_by_opacity(
    const std::vector<float>& logit_opacity, float min_opacity) {
    if (!(min_opacity >= 0.0f && min_opacity < 1.0f) || !std::isfinite(min_opacity))
        throw std::runtime_error("--min-opacity: expected a number in [0, 1)");
    std::vector<uint8_t> keep(logit_opacity.size(), 1);
    if (min_opacity == 0.0f) return keep;
    const float threshold = std::log(min_opacity / (1.0f - min_opacity));
    for (size_t i = 0; i < keep.size(); ++i)
        keep[i] = logit_opacity[i] >= threshold;
    return keep;
}

void reduce_splat_sh_degree(SplatCloud& cloud, int sh_degree) {
    if (sh_degree < 0 || sh_degree > cloud.sh_degree)
        throw std::runtime_error("--sh-degree: expected a degree from 0 through the model's degree");
    if (sh_degree == cloud.sh_degree) return;
    const int64_t old_k = cloud.dim_sh() - 1;
    const int64_t new_k = (int64_t)(sh_degree + 1) * (sh_degree + 1) - 1;
    std::vector<float> reduced((size_t)cloud.num * new_k * 3);
    for (int64_t i = 0; i < cloud.num; ++i)
        std::copy_n(&cloud.features_sh[(size_t)i * old_k * 3], new_k * 3,
                    &reduced[(size_t)i * new_k * 3]);
    cloud.features_sh = std::move(reduced);
    cloud.sh_degree = sh_degree;
}

}  // namespace spirula
