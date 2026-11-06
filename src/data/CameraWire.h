#pragma once

#include "data/DatasetParser.h"
#include "data/FrustumTemplate.h"

namespace dsparse {

struct CameraWireSegment {
    std::array<float, 3> origin, a, b;
};

inline std::vector<CameraWireSegment> camera_wire(const ParsedDataset& ds,
                                                  const std::vector<int32_t>& indices) {
    std::vector<CameraWireSegment> out;
    for (int32_t i : indices) {
        if (i < 0 || i >= ds.num_cameras) continue;
        const float* m = &ds.c2w[i*12];
        const float* k = &ds.intrins[i*4];
        auto shape = camhost::frustum_template(ds.camera_models[i], ds.camera_distortions[i],
            ds.widths[i], ds.heights[i], k[0], k[1], k[2], k[3], &ds.dist_coeffs[i*8]);
        auto offset = [&](camhost::FrustumPoint p) {
            std::array<float, 3> v;
            for (int r = 0; r < 3; r++) v[r] = m[r*4]*p.x - m[r*4+1]*p.y - m[r*4+2]*p.z;
            return v;
        };
        auto line = [&](camhost::FrustumPoint a, camhost::FrustumPoint b) {
            out.push_back({{m[3], m[7], m[11]}, offset(a), offset(b)});
        };
        for (const auto& l : shape.lines) {
            for (size_t j = 1; j < l.pts.size(); j++) line(l.pts[j-1], l.pts[j]);
            if (l.closed && !l.pts.empty()) line(l.pts.back(), l.pts.front());
        }
        for (const auto& p : shape.anchors) line({0,0,0}, p);
    }
    return out;
}

} // namespace dsparse
