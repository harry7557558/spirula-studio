#pragma once

// The view onto a mask: zoom and centre in DISPLAYED mask pixels, the window
// of it a pane shows, and that window's pixels. The document's planes are
// stored-orientation; every texel goes through one displayed-to-stored map.
// No ImGui, no GL. Design: docs/notes/mask-editor.md.

#include "app/gui/mask/MaskDoc.h"

#include <cstdint>
#include <vector>

namespace gui {
namespace mask {

inline constexpr int kMaxWindowTex = 4096;
// Below 1 the picture no longer fills the pane, leaving room to start a shape outside it.
inline constexpr float kMinZoom = 0.25f;
inline constexpr float kMaxZoom = 64.0f;

struct View {
    float zoom = 1.0f;            // 1 = the whole mask fits the pane
    float cx = 0.0f, cy = 0.0f;   // centre, displayed mask pixels
};

// Screen pixels per displayed mask pixel at zoom 1.
float fit_scale(int dw, int dh, float pane_w, float pane_h);

struct Mapping {
    float scale = 1.0f;           // screen px per mask px
    float x0 = 0.0f, y0 = 0.0f;   // mask coordinate at the pane's top-left
    float to_mask_x(float sx) const { return x0 + sx / scale; }
    float to_mask_y(float sy) const { return y0 + sy / scale; }
    float to_screen_x(float mx) const { return (mx - x0) * scale; }
    float to_screen_y(float my) const { return (my - y0) * scale; }
};
Mapping mapping(const View& v, int dw, int dh, float pane_w, float pane_h);

void clamp_view(View& v, int dw, int dh);
// Multiply the zoom by `factor`, keeping the mask point under pane pixel
// (sx, sy) where it is.
void zoom_about(View& v, float factor, float sx, float sy, int dw, int dh,
                float pane_w, float pane_h);
void pan(View& v, float dx_screen, float dy_screen, const Mapping& m, int dw, int dh);

// The status strip height the canvas reserves: the tallest any frame measured
// since the last deliberate change (the mode, the window width), so a line
// that comes and goes never rescales the picture under the cursor.
struct StripReserve {
    float h = 0.0f;
    int mode = -1;
    float width = -1.0f;
    float update(float measured, int now_mode, float now_width);
};

struct Window {
    Rect r;            // displayed mask pixels the texture covers
    int step = 1;      // mask px per texel
    int tw = 0, th = 0;
};
Window window_for(const Mapping& m, int dw, int dh, float pane_w, float pane_h);
bool same_window(const Window& a, const Window& b);

// What a pane shows of the frame. Overlay: the photo, dropped pixels tinted,
// the layers as tints. MaskOnly: the composite as light and dark under the
// same layer tints. Photo: the frame and nothing else, for the peek.
enum class Style { Overlay, MaskOnly, Photo };

struct WindowSource {
    const uint8_t* rgb = nullptr;       // the frame as stored, fw x fh x 3
    int fw = 0, fh = 0;
    const uint8_t* composite = nullptr; // stored W x H, 255 = keep
    const uint8_t* drop = nullptr;
    const uint8_t* keep = nullptr;
    int W = 0, H = 0;
    sfm::ExifTransform turn;            // displayed -> stored
    Style style = Style::Overlay;
};

// `rgba` is resized to win.tw*win.th*4; only the texels under `part`
// (displayed mask pixels, clipped to win.r) are written. Returns that texel
// rectangle, empty when `part` misses the window.
Rect derive_window(const Window& win, const Rect& part, const WindowSource& src,
                   std::vector<uint8_t>& rgba);

}  // namespace mask
}  // namespace gui
