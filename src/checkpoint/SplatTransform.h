#pragma once

#include "data/SceneTransform.h"
#include <array>

namespace spirula {

// Transforms a standard channel-major PLY row, including its directional color.
class SplatTransform {
public:
    SplatTransform(const SceneTransform& transform, int sh_coefficients);
    void apply(float* row) const;

private:
    SceneTransform _transform;
    int _degree = 0, _coefficients = 0;
    bool _rotate = false;
    double _quaternion[4] = {1, 0, 0, 0};
    std::array<double, 625> _sh{};
};

}  // namespace spirula
