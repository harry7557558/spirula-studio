#pragma once
// Helpers for the packed per-splat screen rows (shaders/screen_layout.h).
//
// The parity harnesses feed and read the screen buffer component by component,
// which is also the order their reference files were dumped in before the rows
// were interleaved. These convert between that order and a packed row so the
// refs stay comparable across the layout change.

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "backend/api/BackendRuntime.h"
#include "backend/api/TileIntersect.h"
#include "core/Tensor.h"
#include "shaders/screen_layout.h"

inline ProjEllipseView ellipse_view(const std::vector<DeviceTensorFloatND>& s,
                                    bool eval3d) {
    return proj_ellipse_view(s[0].data_ptr(), eval3d);
}

// Component-major host data -> one packed row buffer. Each entry is
// {row offset, component values}, laid out [n, width].
inline std::vector<float> interleave_screen(
    int64_t n, int stride,
    std::initializer_list<std::pair<int, const std::vector<float>*>> comps
) {
    std::vector<float> rows((size_t)n * stride, 0.0f);
    for (auto [off, src] : comps) {
        const int width = n > 0 ? (int)((int64_t)src->size() / n) : 0;
        for (int64_t i = 0; i < n; i++)
            for (int c = 0; c < width; c++)
                rows[i * stride + off + c] = (*src)[i * width + c];
    }
    return rows;
}

// Packed rows -> component-major, appended to a readback accumulator. Each
// entry is {row offset, component width}.
inline void readback_screen(
    std::vector<float>& acc, const DeviceTensorFloatND& t, int stride,
    std::initializer_list<std::pair<int, int>> comps
) {
    const float* d = t.data_ptr();
    if (d == nullptr || t.numel() == 0) return;
    std::vector<float> rows((size_t)t.numel());
    backend::memcpy_sync(rows.data(), d, rows.size() * sizeof(float),
                         backend::MemcpyKind::DeviceToHost);
    const int64_t n = t.numel() / stride;
    for (auto [off, width] : comps) {
        size_t o = acc.size();
        acc.resize(o + (size_t)n * width);
        for (int64_t i = 0; i < n; i++)
            for (int c = 0; c < width; c++)
                acc[o + i * width + c] = rows[i * stride + off + c];
    }
}

// The two legacy component orders, for readback_screen / interleave_screen.
#define SCREEN_ROWS_2D_COMPS \
    {{SCR2_XY, 2}, {SCR2_DEPTH, 1}, {SCR2_CONIC, 3}, {SCR2_OPAC, 1}, \
     {SCR2_RGB, 3}}
#define SCREEN_ROWS_GUT_COMPS \
    {{SCRG_SCALE, 3}, {SCRG_OPAC, 1}, {SCRG_RGB, 3}}
