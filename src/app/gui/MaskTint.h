#pragma once

// How a masked pixel is drawn over its photograph, by the mask preview and by
// the run's reel alike, so the two read as one answer.

#include <algorithm>
#include <cstdint>

namespace gui {

// Dropped from everything: dimmed and pulled to red.
inline void tint_removed(uint8_t* px) {
    px[0] = (uint8_t)(px[0] / 3 + 150);
    px[1] = (uint8_t)(px[1] / 3);
    px[2] = (uint8_t)(px[2] / 3);
}

// Trained on, but kept out of feature extraction: amber hatching over a light
// wash. Lightness and pattern tell it from the red, not hue alone, which a
// red-green colour-blind eye does not separate from orange.
inline void tint_features_only(uint8_t* px, int x, int y, int period) {
    static constexpr int kAmber[3] = {255, 176, 32};
    const int a = (x + y) % period < (period + 2) / 4 ? 170 : 80;   // of 256
    for (int c = 0; c < 3; c++)
        px[c] = (uint8_t)((px[c] * (256 - a) + kAmber[c] * a) >> 8);
}

// The hatch spacing, in the pixels of a w x h picture, that looks the same on
// screen whatever size the picture was made at.
inline int hatch_period(int w, int h) { return std::max(6, std::max(w, h) / 90); }

}  // namespace gui
