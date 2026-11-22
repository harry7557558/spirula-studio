#pragma once

#include "core/ColorBasis.h"
#include "core/ShRotation.h"
#include "core/SphericalBeta.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spirula {

// SuperSplat's PLY import adds Rz(pi): the stored turn is (x,y,z)->(-x,-z,-y).
// This rotation is its own inverse; quaternion signs distinguish the two trips.
class SplatYUp {
public:
    SplatYUp() {
        const double rotation[9] = {-1,0,0, 0,0,-1, 0,-1,0};
        const ShRotation sh(rotation, ShRotation::kMaxDegree);
        for (int l = 1; l <= ShRotation::kMaxDegree; ++l) {
            const int w = 2*l+1, at = l*l-1;
            for (int a = 0; a < w; ++a)
                for (int b = 0; b < w; ++b) {
                    const double value = sh.band(l)[a*w+b];
                    if (std::abs(value) > 1e-12)
                        _rows[at+a].push_back({at+b, float(value)});
                }
        }
    }

    void apply(float* mean, float* quat, float* rest, ColorBasis basis,
               bool to_native = false) const {
        const float y = mean[1];
        mean[0] = -mean[0]; mean[1] = -mean[2]; mean[2] = -y;
        const float h = to_native ? -0.7071067811865475244f : 0.7071067811865475244f;
        const float w = quat[0], x = quat[1], qy = quat[2], z = quat[3];
        quat[0] = h*(z-qy); quat[1] = h*(z+qy);
        quat[2] = h*(w-x); quat[3] = -h*(w+x);
        if (!rest) return;
        const int n = basis.sh_coeffs();
        float rotated[24*3]{};
        for (int a = 0; a < n; ++a)
            for (const auto& term : _rows[a])
                for (int ch = 0; ch < 3; ++ch)
                    rotated[3*a+ch] += term.value * rest[3*term.column+ch];
        std::copy(rotated, rotated+3*n, rest);
        for (int l = 0; l < basis.sb_lobes; ++l) {
            float* lobe = rest + 3*n + sb::kLobeFloats*l;
            double a[3];
            sb::axis(lobe[3], lobe[4], a);
            lobe[3] = float(std::acos(std::fmin(std::fmax(-a[1], -1.0), 1.0)));
            lobe[4] = float(std::atan2(-a[2], -a[0]));
        }
    }

private:
    struct Term { int column; float value; };
    std::array<std::vector<Term>, 24> _rows;
};

} // namespace spirula
