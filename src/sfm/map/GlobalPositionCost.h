// Direction residual and Jacobian for global camera/point positioning.
#pragma once

#include "sfm/geometry/LinAlg.h"

namespace sfm {

struct GlobalPositionLinearization {
    Vec3 residual;
    double point_diagonal;
    double camera_diagonal;
    Vec3 scale_column;
};

inline GlobalPositionLinearization globalPositionLinearize(const Vec3& ray, const Vec3& point,
                                                            const Vec3& camera, double scale) {
    const Vec3 delta = point - camera;
    return {ray - delta * scale, -scale, scale, delta * -1.0};
}

}  // namespace sfm
