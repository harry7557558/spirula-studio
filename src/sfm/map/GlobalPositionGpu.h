#pragma once

#include <string>
#include "sfm/map/GlobalPosition.h"

namespace sfm {

GlobalPositionResult solveGlobalPositionGpu(GlobalPositionProblem& problem,
                                            const GlobalPositionOptions& options = {},
                                            int device = -1,
                                            const std::string& device_selector = {});

}  // namespace sfm
