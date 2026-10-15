#pragma once

// Builds a GaussianHierarchy from a SplatCloud via k-d tree + moment matching.

#include "lod/HierarchyNode.h"
#include "lod/LodConfig.h"
#include "checkpoint/SplatPly.h"

#include <functional>
#include <string>

namespace spirula {

using LodBuildLog = std::function<void(const std::string&)>;

GaussianHierarchy build_gaussian_hierarchy(
    const SplatCloud& cloud,
    const LodBuildConfig& cfg = {},
    const LodBuildLog& log = {});

}  // namespace spirula
