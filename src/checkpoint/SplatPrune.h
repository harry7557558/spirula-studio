#pragma once

#include <cstdint>
#include <vector>

namespace spirula {

struct SplatCloud;

// Returns a keep flag for the highest-contributing fraction of splats.
std::vector<uint8_t> select_splats_by_contribution(
    const std::vector<float>& contribution, double keep_fraction);

std::vector<uint8_t> select_splats_by_opacity(
    const std::vector<float>& logit_opacity, float min_opacity);

void reduce_splat_sh_degree(SplatCloud& cloud, int sh_degree);

}  // namespace spirula
