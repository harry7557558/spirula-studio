// MaskWindow.cpp -- see MaskWindow.h.

#include "app/gui/mask/MaskWindow.h"

#include "nn/core/Parallel.h"

#include <algorithm>
#include <cmath>

namespace gui {
namespace mask {

float fit_scale(int dw, int dh, float pane_w, float pane_h) {
    if (dw <= 0 || dh <= 0) return 1.0f;
    return std::min(pane_w / (float)dw, pane_h / (float)dh);
}

Mapping mapping(const View& v, int dw, int dh, float pane_w, float pane_h) {
    Mapping m;
    m.scale = fit_scale(dw, dh, pane_w, pane_h) * v.zoom;
    m.x0 = v.cx - 0.5f * pane_w / m.scale;
    m.y0 = v.cy - 0.5f * pane_h / m.scale;
    return m;
}

void clamp_view(View& v, int dw, int dh) {
    v.zoom = std::clamp(v.zoom, kMinZoom, kMaxZoom);
    v.cx = std::clamp(v.cx, 0.0f, (float)dw);
    v.cy = std::clamp(v.cy, 0.0f, (float)dh);
}

void zoom_about(View& v, float factor, float sx, float sy, int dw, int dh,
                float pane_w, float pane_h) {
    const Mapping before = mapping(v, dw, dh, pane_w, pane_h);
    const float mx = before.to_mask_x(sx), my = before.to_mask_y(sy);
    v.zoom = std::clamp(v.zoom * factor, kMinZoom, kMaxZoom);
    const Mapping after = mapping(v, dw, dh, pane_w, pane_h);
    v.cx = mx - (sx - 0.5f * pane_w) / after.scale;
    v.cy = my - (sy - 0.5f * pane_h) / after.scale;
    clamp_view(v, dw, dh);
}

void pan(View& v, float dx_screen, float dy_screen, const Mapping& m, int dw, int dh) {
    v.cx -= dx_screen / m.scale;
    v.cy -= dy_screen / m.scale;
    clamp_view(v, dw, dh);
}

Window window_for(const Mapping& m, int dw, int dh, float pane_w, float pane_h) {
    Window w;
    const Rect r{(int)std::floor(m.x0), (int)std::floor(m.y0),
                 (int)std::ceil(m.x0 + pane_w / m.scale) + 1,
                 (int)std::ceil(m.y0 + pane_h / m.scale) + 1};
    w.r = clip(r, dw, dh);
    if (w.r.empty()) return w;
    const int big = std::max(w.r.w(), w.r.h());
    w.step = std::max(1, (big + kMaxWindowTex - 1) / kMaxWindowTex);
    w.tw = (w.r.w() + w.step - 1) / w.step;
    w.th = (w.r.h() + w.step - 1) / w.step;
    return w;
}

float StripReserve::update(float measured, int now_mode, float now_width) {
    if (now_mode != mode || now_width != width) {
        mode = now_mode;
        width = now_width;
        h = measured;
    } else {
        h = std::max(h, measured);
    }
    return h;
}

bool same_window(const Window& a, const Window& b) {
    return a.r.x0 == b.r.x0 && a.r.y0 == b.r.y0 && a.r.x1 == b.r.x1 &&
           a.r.y1 == b.r.y1 && a.step == b.step && a.tw == b.tw && a.th == b.th;
}

namespace {

// Picture.cpp's tint for a dropped pixel, then the two layers as 25% blends.
// MaskOnly shows the composite as 230/30 grey; Photo shows the frame alone.
void shade(Style style, int r, int g, int b, bool dropped, bool in_drop, bool in_keep,
           uint8_t* o) {
    if (style == Style::MaskOnly) r = g = b = dropped ? 30 : 230;
    else if (style == Style::Overlay && dropped) { r = r / 3 + 150; g = g / 3; b = b / 3; }
    if (style != Style::Photo) {
        if (in_drop) { r = (r * 3 + 235) / 4; g = (g * 3 + 45) / 4; b = (b * 3 + 45) / 4; }
        if (in_keep) { r = (r * 3 + 60) / 4; g = (g * 3 + 220) / 4; b = (b * 3 + 90) / 4; }
    }
    o[0] = (uint8_t)std::clamp(r, 0, 255);
    o[1] = (uint8_t)std::clamp(g, 0, 255);
    o[2] = (uint8_t)std::clamp(b, 0, 255);
    o[3] = 255;
}

}  // namespace

Rect derive_window(const Window& win, const Rect& part, const WindowSource& src,
                   std::vector<uint8_t>& rgba) {
    rgba.resize((size_t)win.tw * win.th * 4);
    const Rect p{std::max(part.x0, win.r.x0), std::max(part.y0, win.r.y0),
                 std::min(part.x1, win.r.x1), std::min(part.y1, win.r.y1)};
    if (p.empty() || !src.rgb || !src.composite) return {};
    const int tx0 = (p.x0 - win.r.x0) / win.step, ty0 = (p.y0 - win.r.y0) / win.step;
    const int tx1 = std::min(win.tw, (p.x1 - win.r.x0 + win.step - 1) / win.step);
    const int ty1 = std::min(win.th, (p.y1 - win.r.y0 + win.step - 1) / win.step);
    nn::parallel_for(ty1 - ty0, [&](int64_t lo, int64_t hi) {
        for (int64_t k = lo; k < hi; k++) {
            const int ty = ty0 + (int)k;
            const int my0 = win.r.y0 + ty * win.step;
            const int my1 = std::min(my0 + win.step, win.r.y1);
            for (int tx = tx0; tx < tx1; tx++) {
                const int mx0 = win.r.x0 + tx * win.step;
                const int mx1 = std::min(mx0 + win.step, win.r.x1);
                int acc[3] = {0, 0, 0}, cnt = 0, dropped = 0, ld = 0, lk = 0;
                for (int my = my0; my < my1; my++)
                    for (int mx = mx0; mx < mx1; mx++) {
                        int sx, sy;
                        to_stored(src.turn, src.W, src.H, mx, my, sx, sy);
                        if (sx < 0 || sy < 0 || sx >= src.W || sy >= src.H) continue;
                        const size_t i = (size_t)sy * src.W + (size_t)sx;
                        const int fx = std::min(src.fw - 1, (int)((int64_t)sx * src.fw / src.W));
                        const int fy = std::min(src.fh - 1, (int)((int64_t)sy * src.fh / src.H));
                        const uint8_t* c = src.rgb + ((size_t)fy * src.fw + (size_t)fx) * 3;
                        acc[0] += c[0];
                        acc[1] += c[1];
                        acc[2] += c[2];
                        cnt++;
                        dropped += src.composite[i] == 0;
                        ld += src.drop && src.drop[i] != 0;
                        lk += src.keep && src.keep[i] != 0;
                    }
                uint8_t* o = &rgba[((size_t)ty * win.tw + (size_t)tx) * 4];
                if (!cnt) { o[0] = o[1] = o[2] = 0; o[3] = 255; continue; }
                // A decimated tie reads as dropped: hiding a correction is worse than over-showing one.
                shade(src.style, acc[0] / cnt, acc[1] / cnt, acc[2] / cnt, dropped * 2 >= cnt,
                      ld * 2 > cnt, lk * 2 > cnt, o);
            }
        }
    }, 4);
    return {tx0, ty0, tx1, ty1};
}

}  // namespace mask
}  // namespace gui
