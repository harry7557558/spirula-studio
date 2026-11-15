// Moving a detection's boundary before it joins the mask -- outward or inward,
// see MaskOptions::dilate_ratio. The geometry is core/MaskMargin.h, shared with
// the mask editor; this file adapts it to sam::Mask and composes the union.

#include "sam/Masking.h"

#include "core/MaskMargin.h"
#include "nn/core/Parallel.h"

#include <vector>

namespace sam {

namespace {

// The union without a margin, and the path `dilate_ratio = 0` must keep taking
// byte for byte.
void or_into(const Mask& m, std::vector<uint8_t>& hit) {
    const uint8_t* src = m.data.data();
    uint8_t* dst = hit.data();
    nn::parallel_for((int64_t)m.data.size(), [src, dst](int64_t lo, int64_t hi) {
        for (int64_t i = lo; i < hi; ++i)
            if (src[i] > 127) dst[i] = 1;
    }, /*min_chunk=*/65536);
}

}  // namespace

int dilate_radius_px(const Box& box, float dilate_ratio) {
    return margin::radius_px(box.x0, box.y0, box.x1, box.y1, dilate_ratio);
}

void accumulate_dilated(const Mask& m, int radius, std::vector<uint8_t>& hit) {
    if (m.data.empty() || m.data.size() != hit.size()) return;
    // A mask whose dimensions do not describe its bytes cannot be cropped, and
    // the union is still well defined; take it flat rather than drop it.
    const bool shaped = (size_t)m.width * (size_t)m.height == m.data.size();
    if (radius == 0 || !shaped) {
        or_into(m, hit);
        return;
    }
    margin::accumulate(m.data.data(), m.width, m.height, radius, hit);
}

void compose_hit(const Result& positive, const Result& negative,
                 float dilate_ratio, std::vector<uint8_t>& hit) {
    for (const Detection& d : positive.detections) {
        if (d.mask.data.size() != hit.size()) continue;
        const Mask& m = d.mask;
        if (dilate_ratio == 0.0f || (size_t)m.width * m.height != m.data.size())
            or_into(m, hit);
        else
            margin::accumulate_components(m.data.data(), m.width, m.height, dilate_ratio, hit);
    }
    // After, never before: the margin is a guess about where the object really
    // ends, and a negative phrase is the user saying it does not end there.
    for (const Detection& d : negative.detections) {
        if (d.mask.data.size() != hit.size()) continue;
        const uint8_t* src = d.mask.data.data();
        uint8_t* dst = hit.data();
        nn::parallel_for((int64_t)hit.size(), [src, dst](int64_t lo, int64_t hi) {
            for (int64_t i = lo; i < hi; ++i)
                if (src[i] > 127) dst[i] = 0;
        }, /*min_chunk=*/65536);
    }
}

}  // namespace sam
