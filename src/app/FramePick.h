#pragma once

// FramePick -- which of the frames a run chose are left out after all: blurred,
// or too like the last one kept.
//
// Blur is read off the frame itself -- how wide its edges are
// (app/FrameSharpness.h, edge_width_px) -- so the rule asks nothing of where
// the frame came from: a video, a sequence an editor exported, photographs.
// One rule (PickRule) is walked by both decoders, by gui/FrameSelect, and again
// by the screen when a threshold is tried after the frames are out.

#include "app/FrameSharpness.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace app {

enum class PickVerdict : uint8_t { Kept, Blurry, Similar };

// A chosen frame as the rule sees it.
struct FramePick {
    int64_t  index = 0;
    float    width = -1.0f;  // edge_width_px; negative unmeasured
    float    blur = -1.0f;   // what PickRule made of it, pixels; negative unmeasured
    uint64_t hash = 0;       // frame_dhash
    PickVerdict verdict = PickVerdict::Kept;
};

struct PickRule {
    // A frame blurred across more pixels than this is left out; 0 = off.
    float max_blur = 0.0f;
    // Refuse at most two blurry frames in a row, so each one left out keeps a
    // neighbour and the frames either side of a bad stretch still match.
    bool neighbour = true;
    // A frame must differ from the last one kept by this many bits of 64; 0 = off.
    int similar_bits = 0;

    int run = 0;
    bool seen = false;
    uint64_t last = 0;
    std::vector<float> widths;   // every measured frame so far

    bool on() const { return max_blur > 0.0f || similar_bits > 0; }

    // Bits `hash` differs from the last kept frame by; 64 before there is one.
    int distance(uint64_t hash) const {
        if (!seen) return 64;
        int n = 0;
        for (uint64_t x = hash ^ last; x; x &= x - 1) n++;
        return n;
    }

    // An edge width as blur: what is left once the lens's own share is taken
    // off in quadrature -- the 10th percentile of the widths so far, plus 8%
    // (of 379 frames shot at 1/384 s, 21 read over 8 px without it, 14 with).
    float blur(float width) const {
        if (width < 0.0f) return -1.0f;
        std::vector<float> s(widths);
        s.push_back(width);
        const size_t at = s.size() / 10;
        std::nth_element(s.begin(), s.begin() + at, s.end());
        const float lens = 1.08f * s[at];
        return std::sqrt(std::max(width * width - lens * lens, 0.0f));
    }

    // Fills in `p.blur` and `p.verdict` from `p.width` and `p.hash`.
    PickVerdict judge(FramePick& p) {
        p.blur = blur(p.width);
        if (p.width >= 0.0f) widths.push_back(p.width);
        p.verdict = PickVerdict::Kept;
        // A still camera is not a bad stretch: it does not count towards `run`.
        if (similar_bits > 0 && distance(p.hash) < similar_bits) {
            p.verdict = PickVerdict::Similar;
        } else if (max_blur > 0.0f && p.blur > max_blur && !(neighbour && run >= 2)) {
            run++;
            p.verdict = PickVerdict::Blurry;
        } else {
            run = 0;
            seen = true;
            last = p.hash;
        }
        return p.verdict;
    }
};

// A difference hash of `rgb` (w*h*3): 9x8 cells of mean luma, one bit for each
// cell brighter than the one to its right. Cells are sampled on a lattice, not
// boxed: this runs once a kept frame on the thread feeding the decoder.
inline uint64_t frame_dhash(const uint8_t* rgb, int w, int h) {
    if (!rgb || w < 9 || h < 8) return 0;
    constexpr int kTaps = 16;
    unsigned cell[8][9];
    for (int cy = 0; cy < 8; cy++)
        for (int cx = 0; cx < 9; cx++) {
            unsigned sum = 0;
            for (int j = 0; j < kTaps; j++) {
                const int y = (int)(((int64_t)(cy * kTaps + j) * 2 + 1) * h / (16 * kTaps));
                for (int i = 0; i < kTaps; i++) {
                    const int x = (int)(((int64_t)(cx * kTaps + i) * 2 + 1) * w / (18 * kTaps));
                    const uint8_t* p = rgb + ((size_t)y * w + (size_t)x) * 3;
                    sum += (77u * p[0] + 150u * p[1] + 29u * p[2]) >> 8;
                }
            }
            cell[cy][cx] = sum;
        }
    uint64_t bits = 0;
    for (int cy = 0; cy < 8; cy++)
        for (int cx = 0; cx < 8; cx++)
            bits = (bits << 1) | (cell[cy][cx] > cell[cy][cx + 1] ? 1u : 0u);
    return bits;
}

// What the rule reads off a frame's pixels. The edge width is the costly half
// -- tens of milliseconds at 5K -- so only where blur is being judged.
inline void measure_pick(FramePick& p, const uint8_t* rgb, int w, int h, bool blur) {
    p.hash = frame_dhash(rgb, w, h);
    if (blur) p.width = edge_width_px(rgb, w, h);
}

}  // namespace app
