// Growing a detection before it joins the mask -- see MaskOptions::dilate_ratio.
//
// Separate from Masking.cpp because none of it needs a model, a device or a
// session: it is geometry over one binary mask, and a test binary can run it
// on a machine with no GPU.

#include "sam/Masking.h"

#include "core/DistanceTransform.h"
#include "nn/core/Parallel.h"

#include <algorithm>
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
    if (!(dilate_ratio > 0.0f)) return 0;
    const float bw = std::max(1.0f, box.x1 - box.x0);
    const float bh = std::max(1.0f, box.y1 - box.y0);
    // The odd kernel SIZE first, then the radius as half of it: growing by r on
    // each side widens the box by 2r, so the box gains exactly `dilate_ratio`
    // of its own mean side. Floor of 3 keeps a tiny detection growing at all.
    int k = (int)(dilate_ratio * 0.5f * (bw + bh));
    if (k < 3) k = 3;
    k |= 1;
    return (k - 1) / 2;
}

void accumulate_dilated(const Mask& m, int radius, std::vector<uint8_t>& hit) {
    if (m.data.empty() || m.data.size() != hit.size()) return;
    // A mask whose dimensions do not describe its bytes cannot be cropped, and
    // the union is still well defined; take it flat rather than drop it.
    const bool shaped = (size_t)m.width * (size_t)m.height == m.data.size();
    if (radius <= 0 || !shaped) {
        or_into(m, hit);
        return;
    }

    int x0 = m.width, y0 = m.height, x1 = -1, y1 = -1;
    for (int y = 0; y < m.height; ++y) {
        const uint8_t* row = m.data.data() + (size_t)y * m.width;
        int rx0 = -1, rx1 = -1;
        for (int x = 0; x < m.width; ++x)
            if (row[x] > 127) { if (rx0 < 0) rx0 = x; rx1 = x; }
        if (rx0 < 0) continue;
        if (rx0 < x0) x0 = rx0;
        if (rx1 > x1) x1 = rx1;
        if (y < y0) y0 = y;
        y1 = y;
    }
    if (x1 < 0) return;

    // The transform runs on the bounding box plus the margin and nothing else:
    // every pixel it can reach is within `radius` of a set pixel, so the crop
    // holds the whole answer, and clipping it to the frame IS the border rule.
    const int cx0 = std::max(0, x0 - radius), cy0 = std::max(0, y0 - radius);
    const int cx1 = std::min(m.width - 1, x1 + radius);
    const int cy1 = std::min(m.height - 1, y1 + radius);
    const int cw = cx1 - cx0 + 1, ch = cy1 - cy0 + 1;

    std::vector<uint8_t> crop((size_t)cw * ch);
    for (int y = 0; y < ch; ++y) {
        const uint8_t* src = m.data.data() + (size_t)(cy0 + y) * m.width + cx0;
        uint8_t* dst = crop.data() + (size_t)y * cw;
        for (int x = 0; x < cw; ++x) dst[x] = src[x] > 127 ? 1 : 0;
    }
    edt::apply_mask_boundary_offset_in_place(crop.data(), ch, cw, (float)radius);
    for (int y = 0; y < ch; ++y) {
        const uint8_t* src = crop.data() + (size_t)y * cw;
        uint8_t* dst = hit.data() + (size_t)(cy0 + y) * m.width + cx0;
        for (int x = 0; x < cw; ++x)
            if (src[x]) dst[x] = 1;
    }
}

void compose_hit(const Result& positive, const Result& negative,
                 float dilate_ratio, std::vector<uint8_t>& hit) {
    for (const Detection& d : positive.detections) {
        if (d.mask.data.size() != hit.size()) continue;
        accumulate_dilated(d.mask, dilate_radius_px(d.box, dilate_ratio), hit);
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
