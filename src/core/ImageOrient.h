#pragma once

// Quarter turns and mirroring of an interleaved host image buffer: the EXIF
// Orientation transform (sfm/core/Exif.h ExifTransform) applied to pixels, in
// the one place every path that bakes it in reads it from.

#include <cstddef>
#include <utility>

namespace spirula {

// The size `turns_cw` quarter turns leave a w x h image at.
inline void oriented_size(int turns_cw, int& w, int& h) {
    if (turns_cw & 1) std::swap(w, h);
}

// Turn clockwise, then mirror horizontally, matching ExifTransform. `dst` holds
// w*h*channels elements and may not alias `src`.
template <typename T>
void orient_pixels(const T* src, int w, int h, int channels,
                   int turns_cw, bool mirror, T* dst) {
    const int t = turns_cw & 3;
    int dw = w, dh = h;
    oriented_size(t, dw, dh);
    const size_t c = (size_t)channels;
    for (int dy = 0; dy < dh; dy++) {
        for (int dx = 0; dx < dw; dx++) {
            const int mx = mirror ? dw - 1 - dx : dx;
            int sx = 0, sy = 0;
            switch (t) {
                case 1:  sx = dy;         sy = h - 1 - mx; break;
                case 2:  sx = w - 1 - mx; sy = h - 1 - dy; break;
                case 3:  sx = w - 1 - dy; sy = mx;         break;
                default: sx = mx;         sy = dy;         break;
            }
            const T* s = src + ((size_t)sy * w + sx) * c;
            T* d = dst + ((size_t)dy * dw + dx) * c;
            for (size_t k = 0; k < c; k++) d[k] = s[k];
        }
    }
}

}  // namespace spirula
