// frame_mask_test -- app/FrameMask.h shapes with no GUI: the even-odd fill
// in core/PolygonFill.h against a ray cast, the path spelling round trip,
// the path fill on a non-square frame, the ordered composition rule, brush
// strokes, pen curves, the SVG file form (app/FrameMaskSvg.h), and a
// stencil resolved per camera folder.

#include "app/FrameMask.h"
#include "app/FrameMaskSvg.h"
#include "core/CubicBezier.h"
#include "core/PolygonFill.h"
#include "core/SourcePath.h"
#include "external/stb_image_write.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

// Even-odd point test by ray casting to +x, the textbook reference the
// scanline fill has to agree with at every pixel centre.
bool ray_inside(const std::vector<float>& p, float x, float y, bool& tie) {
    const size_t n = p.size() / 2;
    bool in = false;
    tie = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const float ax = p[2 * i], ay = p[2 * i + 1], bx = p[2 * j], by = p[2 * j + 1];
        if ((ay > y) == (by > y)) continue;
        const float xi = ax + (y - ay) / (by - ay) * (bx - ax);
        if (std::fabs(xi - x) < 1e-3f) tie = true;
        if (x < xi) in = !in;
    }
    return in;
}

void compare_fill(const std::vector<float>& poly, int W, int H, const std::string& name) {
    std::vector<uint8_t> out((size_t)W * H, 0);
    polyfill::fill_even_odd(poly.data(), poly.size() / 2, W, H, out.data(), 1);
    size_t compared = 0, mismatched = 0, inside = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            bool tie;
            const bool ref = ray_inside(poly, (float)x + 0.5f, (float)y + 0.5f, tie);
            if (tie) continue;
            compared++;
            inside += ref;
            if (ref != (out[(size_t)y * W + x] != 0)) mismatched++;
        }
    check(mismatched == 0, name + ": fill matches the ray cast on " +
                               std::to_string(compared) + " pixels");
    check(compared * 10 >= (size_t)W * H * 9, name + ": at least 90% of pixels compared");
    check(inside > 0, name + ": the polygon covers something");
}

// ---------------------------------------------------------------------------
// The fill
// ---------------------------------------------------------------------------

void test_fill_matches_ray_cast() {
    // A bow tie (self-intersecting), a concave arrow, and a polygon that
    // hangs off every side of the image. Vertices sit at .3/.7 fractions so
    // no crossing lands exactly on a pixel centre.
    compare_fill({3.3f, 2.7f, 30.7f, 20.3f, 30.3f, 2.3f, 3.7f, 20.7f}, 37, 23, "bow tie");
    compare_fill({2.3f, 11.7f, 20.7f, 2.3f, 16.3f, 11.3f, 20.7f, 20.7f}, 25, 23, "arrow");
    compare_fill({-5.3f, -4.7f, 40.7f, -2.3f, 44.3f, 30.7f, -3.7f, 26.3f, 18.3f, 12.7f},
                 37, 23, "overhang");
    std::vector<uint8_t> out(37 * 23, 0);
    const std::vector<float> two = {1.3f, 1.3f, 20.7f, 20.7f};
    polyfill::fill_even_odd(two.data(), 2, 37, 23, out.data(), 1);
    size_t set = 0;
    for (uint8_t v : out) set += v;
    check(set == 0, "two points fill nothing");
    polyfill::fill_even_odd(two.data(), 0, 37, 23, out.data(), 1);
    check(true, "zero points does not crash");
}

// ---------------------------------------------------------------------------
// The spelling
// ---------------------------------------------------------------------------

bool close_to(float a, float b) { return std::fabs(a - b) < 1e-6f; }

void test_path_spelling() {
    std::vector<app::MaskShape> s;
    std::string err;
    check(app::parse_mask_shapes("path 0.1,0.1,0.9,0.1,0.5,0.9", s, err), "parses a path");
    check(s.size() == 1 && s[0].kind == app::MaskShape::Kind::Path && !s[0].remove,
          "one keep path");
    check(s[0].pts.size() == 6 && close_to(s[0].pts[0], 0.1f) && close_to(s[0].pts[5], 0.9f),
          "six numbers in order");
    check(app::parse_mask_shapes("-path 0.1, 0.1, 0.9,0.1, 0.5,0.9", s, err) &&
              s[0].remove && s[0].pts.size() == 6,
          "-path removes, spaces after commas tolerated");
    check(app::parse_mask_shapes("!path 0,0,1,0,1,1,0,1", s, err) && s[0].remove &&
              s[0].pts.size() == 8,
          "! spelling and four corners");
    check(!app::parse_mask_shapes("path 0.1,0.1,0.9,0.1", s, err), "two points rejected");
    check(!app::parse_mask_shapes("path 0.1,0.1,0.9,0.1,0.5", s, err), "odd count rejected");
    check(!app::parse_mask_shapes("path", s, err), "no numbers rejected");
    check(!app::parse_mask_shapes("path 0.1,0.1,0.9,0.1,0.5,x", s, err), "junk rejected");
    check(!app::parse_mask_shapes("path 0.1,0.1,0.9,0.1,0.5,0.9,", s, err),
          "trailing comma rejected");

    // Mixed list, order kept, and the old kinds still spell the same.
    check(app::parse_mask_shapes(
              "ellipse 0.5,0.5,0.49,0.49; -rect 0.2,0.9,0.8,1; -path 0.1,0.1,0.3,0.1,0.2,0.3",
              s, err),
          "mixed list parses");
    check(s.size() == 3 && s[0].kind == app::MaskShape::Kind::Ellipse &&
              s[1].kind == app::MaskShape::Kind::Rect && s[2].kind == app::MaskShape::Kind::Path,
          "kinds in order");
    const std::string back = app::format_mask_shapes(s);
    check(back == "ellipse 0.5000,0.5000,0.4900,0.4900; -rect 0.2000,0.9000,0.8000,1.0000; "
                  "-path 0.1000,0.1000,0.3000,0.1000,0.2000,0.3000",
          "format spells all three: " + back);
    std::vector<app::MaskShape> again;
    check(app::parse_mask_shapes(back, again, err) && again.size() == 3 &&
              again[2].kind == app::MaskShape::Kind::Path && again[2].remove &&
              again[2].pts.size() == 6 && close_to(again[2].pts[4], 0.2f) &&
              close_to(again[2].pts[5], 0.3f),
          "format -> parse is the identity on a path");

    // A long path does not truncate: 40 corners is 80 numbers, past the 128
    // bytes the old fixed buffer held.
    app::MaskShape big;
    big.kind = app::MaskShape::Kind::Path;
    big.remove = true;
    for (int i = 0; i < 40; i++) {
        big.pts.push_back(0.5f + 0.4f * std::cos((float)i * 0.157f));
        big.pts.push_back(0.5f + 0.4f * std::sin((float)i * 0.157f));
    }
    const std::string bigs = app::format_mask_shapes({big});
    std::vector<app::MaskShape> bigp;
    check(app::parse_mask_shapes(bigs, bigp, err) && bigp.size() == 1 && bigp[0].pts.size() == 80,
          "40-corner path survives format -> parse");
    bool all = true;
    for (size_t i = 0; i < 80; i++) all &= std::fabs(bigp[0].pts[i] - big.pts[i]) < 6e-5f;
    check(all, "every corner within the %.4f rounding");
}

// ---------------------------------------------------------------------------
// The fill inside rasterize_frame_mask
// ---------------------------------------------------------------------------

app::MaskShape path_shape(std::vector<float> pts, bool remove) {
    app::MaskShape s;
    s.kind = app::MaskShape::Kind::Path;
    s.remove = remove;
    s.pts = std::move(pts);
    return s;
}

app::MaskShape rect_shape(float x0, float y0, float x1, float y1, bool remove) {
    app::MaskShape s;
    s.kind = app::MaskShape::Kind::Rect;
    s.remove = remove;
    s.cx = x0; s.cy = y0; s.rx = x1; s.ry = y1;
    return s;
}

app::MaskShape ellipse_shape(float cx, float cy, float rx, float ry, bool remove) {
    app::MaskShape s;
    s.kind = app::MaskShape::Kind::Ellipse;
    s.remove = remove;
    s.cx = cx; s.cy = cy; s.rx = rx; s.ry = ry;
    return s;
}

// The polygon in pixels of a W x H frame, for the reference test.
std::vector<float> in_pixels(const std::vector<float>& norm, int W, int H) {
    std::vector<float> px(norm.size());
    for (size_t i = 0; i + 1 < norm.size(); i += 2) {
        px[i] = norm[i] * (float)W;
        px[i + 1] = norm[i + 1] * (float)H;
    }
    return px;
}

void test_path_fill() {
    // 64 x 48: a triangle whose normalised corners give different pixel
    // polygons depending on which dimension scales which axis.
    const int W = 64, H = 48;
    const std::vector<float> tri = {0.13f, 0.11f, 0.87f, 0.21f, 0.47f, 0.93f};
    app::FrameMask m;
    m.shapes.push_back(path_shape(tri, true));
    std::vector<uint8_t> out;
    std::string err;
    check(app::rasterize_frame_mask(m, W, H, out, err), "rasterizes a -path");
    const std::vector<float> px = in_pixels(tri, W, H);
    size_t compared = 0, mismatched = 0, dropped = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            bool tie;
            const bool inside = ray_inside(px, (float)x + 0.5f, (float)y + 0.5f, tie);
            if (tie) continue;
            compared++;
            dropped += out[(size_t)y * W + x] == 0;
            if ((out[(size_t)y * W + x] == 0) != inside) mismatched++;
        }
    check(mismatched == 0, "-path drops exactly the ray-cast inside on 64x48");
    check(dropped > 400 && dropped < 1400, "the triangle is about a third of the frame: " +
                                             std::to_string(dropped));

    // Transposed frame: the same normalised corners on 48 x 64 give a
    // different pixel polygon, and the scaling must follow the axes.
    check(app::rasterize_frame_mask(m, H, W, out, err), "rasterizes on 48x64");
    const std::vector<float> px2 = in_pixels(tri, H, W);
    mismatched = 0;
    for (int y = 0; y < W; y++)
        for (int x = 0; x < H; x++) {
            bool tie;
            const bool inside = ray_inside(px2, (float)x + 0.5f, (float)y + 0.5f, tie);
            if (tie) continue;
            if ((out[(size_t)y * H + x] == 0) != inside) mismatched++;
        }
    check(mismatched == 0, "-path on the transposed frame still matches");

    // A keep path as the only shape: outside is 0, inside 255.
    app::FrameMask k;
    k.shapes.push_back(path_shape(tri, false));
    check(app::rasterize_frame_mask(k, W, H, out, err), "rasterizes a keep path");
    check(out[0] == 0, "outside a lone keep path is dropped");
    check(out[(size_t)(H / 2) * W + W / 2] == 255, "inside a lone keep path is kept");
}

void test_path_order() {
    // -rect over the whole frame, then a keep path: the path's inside comes
    // back (last shape wins), the rest stays dropped.
    const int W = 64, H = 48;
    app::FrameMask m;
    m.shapes.push_back(rect_shape(0.0f, 0.0f, 1.0f, 1.0f, true));
    m.shapes.push_back(path_shape({0.2f, 0.2f, 0.8f, 0.2f, 0.8f, 0.8f, 0.2f, 0.8f}, false));
    std::vector<uint8_t> out;
    std::string err;
    check(app::rasterize_frame_mask(m, W, H, out, err), "rasterizes rect then path");
    check(out[0] == 0 && out[(size_t)(H / 2) * W + W / 2] == 255,
          "keep path restores its inside over a remove rect");
    // Reverse order: the rect wins everywhere.
    std::swap(m.shapes[0], m.shapes[1]);
    check(app::rasterize_frame_mask(m, W, H, out, err), "rasterizes path then rect");
    size_t kept = 0;
    for (uint8_t v : out) kept += v != 0;
    check(kept == 0, "a remove rect after a keep path drops everything");
    // Two paths: a keep path with a -path hole.
    app::FrameMask h;
    h.shapes.push_back(path_shape({0.1f, 0.1f, 0.9f, 0.1f, 0.9f, 0.9f, 0.1f, 0.9f}, false));
    h.shapes.push_back(path_shape({0.4f, 0.4f, 0.6f, 0.4f, 0.6f, 0.6f, 0.4f, 0.6f}, true));
    check(app::rasterize_frame_mask(h, W, H, out, err), "rasterizes two paths");
    check(out[(size_t)(H / 2) * W + W / 2] == 0 && out[(size_t)(H / 4) * W + W / 4] == 255 &&
              out[0] == 0,
          "hole dropped, ring kept, outside dropped");

    // A remove-path over a keep rect: the rect keeps the frame, the path
    // carves its hole out of it.
    app::FrameMask rp;
    rp.shapes.push_back(rect_shape(0.0f, 0.0f, 1.0f, 1.0f, false));
    rp.shapes.push_back(path_shape({0.4f, 0.4f, 0.6f, 0.4f, 0.6f, 0.6f, 0.4f, 0.6f}, true));
    check(app::rasterize_frame_mask(rp, W, H, out, err), "rasterizes rect then remove-path");
    check(out[(size_t)(H / 2) * W + W / 2] == 0 && out[0] == 255,
          "remove-path bites a hole out of a keep rect");

    // A keep ellipse with a remove-path bite: outside the ellipse is already
    // dropped by the base rule, so the path only matters where they overlap.
    app::FrameMask ep;
    ep.shapes.push_back(ellipse_shape(0.5f, 0.5f, 0.45f, 0.45f, false));
    ep.shapes.push_back(path_shape({0.4f, 0.4f, 0.6f, 0.4f, 0.6f, 0.6f, 0.4f, 0.6f}, true));
    check(app::rasterize_frame_mask(ep, W, H, out, err), "rasterizes ellipse then remove-path");
    check(out[(size_t)(H / 2) * W + W / 2] == 0 && out[0] == 0 &&
              out[(size_t)7 * W + 32] == 255,
          "remove-path bite inside a keep ellipse, outside it stays dropped either way");

    // A keep ellipse restoring over a fully removed path: inside the ellipse
    // comes back, outside it stays gone.
    app::FrameMask pe;
    pe.shapes.push_back(path_shape({0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}, true));
    pe.shapes.push_back(ellipse_shape(0.5f, 0.5f, 0.45f, 0.45f, false));
    check(app::rasterize_frame_mask(pe, W, H, out, err), "rasterizes remove-path then ellipse");
    check(out[(size_t)(H / 2) * W + W / 2] == 255 && out[0] == 0,
          "keep ellipse restores its interior over a fully removed path");
}

// ---------------------------------------------------------------------------
// format_mask_shapes must not truncate at any magnitude
// ---------------------------------------------------------------------------

bool close_rel(float a, float b) { return std::fabs(a - b) <= std::fabs(a) * 1e-5f + 1e-3f; }

void test_format_extreme_values_do_not_truncate() {
    // "rect -123456789275539452985344.0000,...,1000000013848427855085568.0000"
    // needs 127 characters with the remove prefix -- 31 past a 96-byte buffer.
    app::MaskShape r;
    r.kind = app::MaskShape::Kind::Rect;
    r.remove = true;
    r.cx = -1.23456789e23f;
    r.cy = 9.87654321e22f;
    r.rx = -5.55555555e23f;
    r.ry = 1.0e24f;
    app::MaskShape e;
    e.kind = app::MaskShape::Kind::Ellipse;
    e.cx = -2.5e22f;
    e.cy = 3.5e22f;
    e.rx = 4.0e22f;
    e.ry = 6.0e22f;

    const std::string out = app::format_mask_shapes({r, e});
    std::vector<app::MaskShape> parsed;
    std::string err;
    check(app::parse_mask_shapes(out, parsed, err) && parsed.size() == 2,
          "an extreme rect and ellipse both re-parse: " + err);
    check(parsed.size() == 2 && parsed[0].kind == app::MaskShape::Kind::Rect &&
              parsed[0].remove && close_rel(parsed[0].cx, r.cx) &&
              close_rel(parsed[0].cy, r.cy) && close_rel(parsed[0].rx, r.rx) &&
              close_rel(parsed[0].ry, r.ry),
          "the rect's four extreme values survive whole, including the last one");
    check(parsed.size() == 2 && parsed[1].kind == app::MaskShape::Kind::Ellipse &&
              !parsed[1].remove && close_rel(parsed[1].cx, e.cx) &&
              close_rel(parsed[1].cy, e.cy) && close_rel(parsed[1].rx, e.rx) &&
              close_rel(parsed[1].ry, e.ry),
          "the ellipse's four extreme values survive whole");
}

// ---------------------------------------------------------------------------
// polyfill::contains must share fill_even_odd's boundary rule
// ---------------------------------------------------------------------------

// Unlike test_fill_matches_ray_cast's vertices (deliberately off the pixel
// grid, so no crossing lands on a sample point), these sit exactly on it --
// the case a strict ray cast and the fill's closed pixel span disagree on.
void compare_contains(const std::vector<float>& poly, int W, int H, const std::string& name) {
    std::vector<uint8_t> out((size_t)W * H, 0);
    polyfill::fill_even_odd(poly.data(), poly.size() / 2, W, H, out.data(), 1);
    size_t mismatched = 0, inside = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const bool ref = out[(size_t)y * W + x] != 0;
            const bool q = polyfill::contains(poly.data(), poly.size() / 2, (float)x + 0.5f,
                                              (float)y + 0.5f);
            inside += ref;
            if (ref != q) mismatched++;
        }
    check(mismatched == 0, name + ": point query matches the fill on all " +
                               std::to_string((size_t)W * H) + " pixels, " +
                               std::to_string(mismatched) + " mismatched");
    check(inside > 0, name + ": the polygon covers something");
}

void test_path_point_matches_fill_boundary() {
    // A concave pentagon and a bow tie, vertices on the pixel-centre grid,
    // on a non-square frame.
    compare_contains({10.5f, 10.5f, 90.5f, 10.5f, 90.5f, 70.5f, 50.5f, 40.5f, 10.5f, 70.5f},
                     101, 81, "concave, grid-aligned");
    compare_contains({10.5f, 10.5f, 90.5f, 70.5f, 90.5f, 10.5f, 10.5f, 70.5f}, 101, 81,
                     "bow tie, grid-aligned");
}


// A stroke is a capsule chain of rx*W by ry*H pixels.
void test_stroke_fill() {
    app::FrameMask m;
    app::MaskShape s;
    s.kind = app::MaskShape::Kind::Stroke;
    s.remove = true;
    s.rx = 0.05f;
    s.ry = 0.05f;
    s.pts = {0.25f, 0.5f, 0.75f, 0.5f};
    m.shapes.push_back(s);
    std::vector<uint8_t> px;
    std::string err;
    check(app::rasterize_frame_mask(m, 200, 100, px, err), "stroke rasterizes");
    auto at = [&](int x, int y) { return px[(size_t)y * 200 + x]; };
    check(at(100, 50) == 0 && at(50, 50) == 0 && at(150, 50) == 0, "stroke: the segment is removed");
    check(at(100, 53) == 0 && at(100, 57) == 255, "stroke: half-height is ry*H = 5 px");
    check(at(42, 50) == 0 && at(38, 50) == 255, "stroke: the cap reaches rx*W = 10 px past the end");
    check(at(10, 10) == 255, "stroke: the rest is kept");

    // One point is a dot, and the spelling carries the radii first.
    s.pts = {0.5f, 0.5f};
    std::vector<app::MaskShape> back;
    check(app::parse_mask_shapes(app::format_mask_shapes({s}), back, err) && back.size() == 1 &&
              back[0].kind == app::MaskShape::Kind::Stroke && back[0].remove &&
              std::fabs(back[0].rx - 0.05f) < 1e-4f && std::fabs(back[0].ry - 0.05f) < 1e-4f &&
              back[0].pts.size() == 2,
          "stroke spelling round trip: " + app::format_mask_shapes({s}));
}

std::vector<uint8_t> raster(const std::vector<app::MaskShape>& shapes, int W, int H) {
    app::FrameMask m;
    m.shapes = shapes;
    std::vector<uint8_t> px;
    std::string err;
    app::rasterize_frame_mask(m, W, H, px, err);
    return px;
}

// Every kind written and read back rasterizes to the same pixels; a keep
// first makes the base black in the file as in the fill.
void test_svg_round_trip() {
    std::vector<app::MaskShape> shapes(5);
    shapes[0].kind = app::MaskShape::Kind::Rect;
    shapes[0].remove = true;
    shapes[0].cx = 0.1f; shapes[0].cy = 0.7f; shapes[0].rx = 0.9f; shapes[0].ry = 1.0f;
    shapes[1].kind = app::MaskShape::Kind::Ellipse;
    shapes[1].remove = false;
    shapes[1].cx = 0.5f; shapes[1].cy = 0.8f; shapes[1].rx = 0.2f; shapes[1].ry = 0.1f;
    shapes[2].kind = app::MaskShape::Kind::Path;
    shapes[2].remove = true;
    shapes[2].pts = {0.1f, 0.1f, 0.4f, 0.1f, 0.25f, 0.4f};
    shapes[3].kind = app::MaskShape::Kind::Stroke;
    shapes[3].remove = true;
    shapes[3].rx = 0.02f;
    shapes[3].ry = 0.02f;
    shapes[3].pts = {0.6f, 0.1f, 0.9f, 0.3f, 0.6f, 0.5f};
    shapes[4].kind = app::MaskShape::Kind::Stroke;
    shapes[4].remove = false;
    shapes[4].rx = 0.03f;
    shapes[4].ry = 0.06f;
    shapes[4].pts = {0.75f, 0.3f};
    const std::string svg = app::write_mask_svg(shapes, "Selfie stick & me");
    std::vector<app::MaskShape> back;
    std::string title, err;
    check(app::read_mask_svg(svg, back, title, err), "svg reads back: " + err);
    check(title == "Selfie stick & me", "svg title round trip: '" + title + "'");
    check(back.size() == shapes.size(), "svg: every shape comes back, " +
                                            std::to_string(back.size()));
    for (size_t i = 0; i < back.size() && i < shapes.size(); i++)
        check(back[i].kind == shapes[i].kind && back[i].remove == shapes[i].remove,
              "svg: shape " + std::to_string(i) + " keeps its kind and op");
    check(raster(back, 160, 90) == raster(shapes, 160, 90), "svg: identical raster at 160x90");
    check(svg.find("data-rx=\"0.03\" data-ry=\"0.06\"") != std::string::npos &&
              svg.find("data-rx=\"0.02\"") == std::string::npos,
          "svg: only a stroke that differs per axis carries data-rx/ry");
    check(svg.find("fill=\"#fff\"/>") != std::string::npos &&
              svg.find("data-role=\"base\"") != std::string::npos,
          "svg: the base rect is written");

    std::vector<app::MaskShape> keep_first(shapes.begin() + 1, shapes.end());
    const std::string k = app::write_mask_svg(keep_first);
    check(k.find("data-role=\"base\" x=\"0\" y=\"0\" width=\"1\" height=\"1\" fill=\"#000\"") !=
              std::string::npos,
          "svg: a keep first paints a black base");
    check(app::read_mask_svg(k, back, title, err) &&
              raster(back, 64, 64) == raster(keep_first, 64, 64),
          "svg: keep-first file reads back to the same raster");
}

// Hand-made SVG: pixel viewBox, circle, polygon, curves, style="", groups,
// defaults (a bare shape is black, so it removes).
void test_svg_hand_made() {
    const std::string svg =
        "<?xml version='1.0'?><!-- made by hand -->\n"
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 100'>"
        "<defs><rect x='0' y='0' width='200' height='100'/></defs>"
        "<g fill='white'><circle cx='100' cy='50' r='10' fill='black'/>"
        "<polygon points='0,0 20,0 0,20'/></g>"
        "<path style='fill:none;stroke:#000;stroke-width:4' d='M150 10 C 160 10, 170 20, 170 30'/>"
        "<path d='M10 90 h20 v-10 h-20 z m40 0 h10 v-10 h-10 z'/>"
        "<path d='M 100 90 A 5 5 0 1 0 110 90'/>"
        "</svg>";
    std::vector<app::MaskShape> out;
    std::string title, err;
    check(app::read_mask_svg(svg, out, title, err), "hand-made svg reads: " + err);
    check(out.size() == 6, "hand-made svg: circle, polygon, stroke, two subpaths, arc -- " +
                               std::to_string(out.size()));
    if (out.size() == 6) {
        check(out[0].kind == app::MaskShape::Kind::Ellipse && out[0].remove &&
                  std::fabs(out[0].cx - 0.5f) < 1e-5f && std::fabs(out[0].rx - 0.05f) < 1e-5f &&
                  std::fabs(out[0].ry - 0.1f) < 1e-5f,
              "circle: normalized per axis by the viewBox");
        check(out[1].kind == app::MaskShape::Kind::Path && !out[1].remove,
              "polygon inherits the group's white fill: keeps");
        check(out[2].kind == app::MaskShape::Kind::Stroke && out[2].remove &&
                  out[2].pts.size() > 4 && std::fabs(out[2].rx - 0.01f) < 1e-6f &&
                  std::fabs(out[2].ry - 0.02f) < 1e-6f,
              "style='' stroke becomes a flattened stroke, 4 units wide per viewBox axis");
        check(out[3].kind == app::MaskShape::Kind::Path && out[4].kind == app::MaskShape::Kind::Path &&
                  std::fabs(out[4].pts[0] - 0.25f) < 1e-5f,
              "relative subpaths: one shape each, the second at x=50");
        check(out[5].kind == app::MaskShape::Kind::Path && out[5].pts.size() > 8,
              "an arc flattens into a filled path");
    }
    check(!app::read_mask_svg("<svg><rect transform='rotate(4)' width='1' height='1'/></svg>",
                              out, title, err) &&
              err.find("transform") != std::string::npos,
          "a transform is refused by name");
    check(!app::read_mask_svg("hello", out, title, err), "not an SVG is refused");
}

void test_border_adjustment() {
    constexpr int W = 160, H = 200;
    app::BorderAccumulator acc;
    std::vector<uint8_t> pixels(W * H, 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (std::hypot(float(x - 76), float(y - 105)) < 68.0f)
                pixels[y * W + x] = 180;
    acc.add(pixels.data(), W, H, 1);
    acc.add(pixels.data(), W, H, 1);
    app::BorderDetectOptions options;
    options.shrink = 0.0f;
    const auto raw = acc.finish(options);
    check(raw.found, "border: offset circle detected in a portrait frame");
    if (!raw.found) return;

    for (float amount : {-0.1f, 0.0f, 0.12f, 0.3f, 0.6f}) {
        options.shrink = amount;
        const auto batch = acc.finish(options);
        const auto preview = app::shrink_border(raw.shape, amount);
        check(batch.found && raster({batch.shape}, W, H) == raster({preview}, W, H),
              "border: preview and batch agree at " + std::to_string(amount));
    }
    const auto small_shrink = raster({app::shrink_border(raw.shape, 0.12f)}, W, H);
    const auto large_shrink = raster({app::shrink_border(raw.shape, 0.3f)}, W, H);
    check(small_shrink[105 * W + 132] == 255 && large_shrink[105 * W + 132] == 0 &&
              large_shrink[105 * W + 76] == 255,
          "border: extended shrink removes the remaining rim while keeping the centre");

    app::FrameStencil stencil;
    stencil.detect_border = true;
    stencil.shrink = 0.3f;
    app::MaskShape remove;
    remove.kind = app::MaskShape::Kind::Rect;
    remove.remove = true;
    remove.cx = 0.4f; remove.cy = 0.4f;
    remove.rx = 0.6f; remove.ry = 0.6f;
    stencil.mask.shapes.push_back(remove);
    stencil.mask.image = "stencil.png";
    const auto before = raster({app::shrink_border(raw.shape, stencil.shrink), remove}, W, H);
    check(!app::edit_detected_border(stencil, app::BorderDetect{}),
          "border: failed detection cannot become an editable ellipse");
    check(app::edit_detected_border(stencil, raw) && !stencil.detect_border &&
              stencil.mask.image == "stencil.png" && stencil.mask.shapes.size() == 2 &&
              raster(stencil.mask.shapes, W, H) == before,
          "border: conversion preserves the mask, image stencil and shape order");
    check(!app::edit_detected_border(stencil, raw) && stencil.mask.shapes.size() == 2,
          "border: conversion cannot insert the border twice");
    auto& ellipse = stencil.mask.shapes.front();
    ellipse.cx += 0.05f;
    ellipse.ry *= 0.9f;
    std::vector<app::MaskShape> restored;
    std::string error;
    check(app::parse_mask_shapes(app::format_mask_shapes(stencil.mask.shapes), restored, error) &&
              raster(restored, W, H) == raster(stencil.mask.shapes, W, H),
          "border: manually adjusted ellipse survives serialization");
}

// A circle as four cubics (the 0.5523 handle length), set against the
// ellipse it approximates to within 0.03% of its radius.
app::MaskShape bezier_circle(float cx, float cy, float r, bool remove) {
    const float k = 0.5522847f * r;
    app::MaskShape s;
    s.kind = app::MaskShape::Kind::Bezier;
    s.remove = remove;
    s.pts = {cx - k, cy - r, cx, cy - r, cx + k, cy - r,
             cx + r, cy - k, cx + r, cy, cx + r, cy + k,
             cx + k, cy + r, cx, cy + r, cx - k, cy + r,
             cx - r, cy + k, cx - r, cy, cx - r, cy - k};
    return s;
}

void test_bezier() {
    const int W = 200, H = 200;
    app::MaskShape e;
    e.kind = app::MaskShape::Kind::Ellipse;
    e.remove = true;
    e.cx = e.cy = 0.5f;
    e.rx = e.ry = 0.3f;
    const app::MaskShape c = bezier_circle(0.5f, 0.5f, 0.3f, true);
    const std::vector<uint8_t> a = raster({e}, W, H), b = raster({c}, W, H);
    size_t differ = 0, removed = 0;
    for (size_t i = 0; i < a.size(); i++) {
        differ += a[i] != b[i];
        removed += b[i] == 0;
    }
    // Pixel centres within the flattening tolerance of the edge may go either way.
    check(differ <= 40 && removed > 10000,
          "bezier: four cubics fill the circle they approximate (" + std::to_string(differ) +
              " of " + std::to_string(removed) + " pixels differ)");

    // Corners only: the same polygon as a path, pixel for pixel.
    app::MaskShape square, path;
    square.kind = app::MaskShape::Kind::Bezier;
    path.kind = app::MaskShape::Kind::Path;
    square.remove = path.remove = true;
    const float corners[] = {0.2f, 0.1f, 0.7f, 0.3f, 0.4f, 0.9f};
    for (int k = 0; k < 3; k++) {
        const float x = corners[2 * k], y = corners[2 * k + 1];
        square.pts.insert(square.pts.end(), {x, y, x, y, x, y});
        path.pts.insert(path.pts.end(), {x, y});
    }
    check(raster({square}, 97, 61) == raster({path}, 97, 61),
          "bezier: straight segments fill exactly as the polygon does");

    std::vector<app::MaskShape> back;
    std::string err;
    const std::string spec = app::format_mask_shapes({c, square});
    check(app::parse_mask_shapes(spec, back, err) && back.size() == 2 &&
              back[0].kind == app::MaskShape::Kind::Bezier && back[0].pts.size() == c.pts.size() &&
              raster(back, W, H) == raster({c, square}, W, H),
          "bezier: spelling round trip -- " + spec.substr(0, 40) + "...");
    check(!app::parse_mask_shapes("bezier 0,0,0.5,0.5,1,1", back, err) &&
              !app::parse_mask_shapes("bezier 0,0,0.5,0.5,1,1,0,0,0.5,0.5,1,1,0", back, err),
          "bezier: one anchor, or a count not a multiple of six, is refused");

    // SVG: a pen shape writes C commands and reads back as the same curve.
    const std::string svg = app::write_mask_svg({c, square}, "curves");
    std::string title;
    err.clear();
    check(svg.find(" C") != std::string::npos && app::read_mask_svg(svg, back, title, err) &&
              back.size() == 2 && back[0].kind == app::MaskShape::Kind::Bezier &&
              back[1].kind == app::MaskShape::Kind::Bezier &&
              back[0].pts.size() == c.pts.size() && back[1].pts.size() == square.pts.size(),
          "bezier svg: both come back as curves with their anchor counts: " + err);
    float worst = 0.0f;
    for (size_t i = 0; i < back[0].pts.size() && i < c.pts.size(); i++)
        worst = std::max(worst, std::fabs(back[0].pts[i] - c.pts[i]));
    check(worst < 1e-5f, "bezier svg: every handle to within 1e-5 (" + std::to_string(worst) + ")");
    check(raster(back, W, H) == raster({c, square}, W, H), "bezier svg: identical raster");

    // Hand-made: an open-ended C/L/Q outline, and a closed one that returns
    // to its start, whose last anchor merges into the first.
    const std::string hand =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
        "<path d='M10 10 C 20 0, 40 0, 50 10 L 50 50 Q 30 70 10 50 Z'/>"
        "<path d='M10 10 C 20 0 40 0 50 10 C 60 20 60 40 50 50 C 40 60 20 60 10 50 C 0 40 0 20 "
        "10 10 Z'/>"
        "</svg>";
    check(app::read_mask_svg(hand, back, title, err) && back.size() == 2 &&
              back[0].kind == app::MaskShape::Kind::Bezier && back[0].pts.size() == 24 &&
              back[1].kind == app::MaskShape::Kind::Bezier && back[1].pts.size() == 24,
          "bezier svg: hand-made curves keep their four anchors each: " + err);
    if (back.size() == 2 && back[1].pts.size() == 24)
        check(std::fabs(back[1].pts[0] - 0.0f) < 1e-6f && std::fabs(back[1].pts[1] - 0.2f) < 1e-6f,
              "bezier svg: the returning curve's last handle becomes the first anchor's in-handle");
    if (back.size() == 2 && back[0].pts.size() == 24)
        check(std::fabs(back[0].pts[18 + 0] - (0.1f + 2.0f / 3.0f * 0.2f)) < 1e-5f &&
                  std::fabs(back[0].pts[18 + 1] - (0.5f + 2.0f / 3.0f * 0.2f)) < 1e-5f,
              "bezier svg: a quadratic is raised to the cubic with the same shape");
}

void test_per_camera_stencil() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "spirula_frame_mask_test_cameras";
    std::error_code ec;
    fs::remove_all(root, ec);
    const int W = 20, H = 10;
    const std::vector<uint8_t> grey((size_t)W * H * 3, 128);
    for (const char* cam : {"cam0", "cam1", "cam2"}) {
        fs::create_directories(root / "images" / cam, ec);
        stbi_write_png((root / "images" / cam / "f.png").string().c_str(), W, H, 3, grey.data(),
                       W * 3);
    }
    app::MaskShape left, right;
    left.kind = right.kind = app::MaskShape::Kind::Rect;
    left.remove = right.remove = true;
    left.cx = 0.0f; left.cy = 0.0f; left.rx = 0.5f; left.ry = 1.0f;
    right.cx = 0.5f; right.cy = 0.0f; right.rx = 1.0f; right.ry = 1.0f;

    app::FrameStencilRun run;
    run.image_dir = (root / "images").string();
    run.mask_dir = (root / "masks").string();
    run.stencil.mask.shapes = {left};
    run.stencil.cameras["cam1"].mask.shapes = {right};
    run.stencil.cameras["cam2"] = app::CameraStencil{};
    check(&run.stencil.for_camera("cam0") == &run.stencil &&
              run.stencil.for_camera("cam1").mask.shapes.size() == 1,
          "per camera: a listed camera has its own, the rest share the input's");
    std::vector<std::string> seen;
    app::FrameStencilSinks sinks;
    sinks.camera = [&](const std::string& rel, int64_t) { seen.push_back(rel); };
    std::string err;
    const int64_t written = app::apply_frame_stencil(run, sinks, err);
    check(written == 2 && seen.size() == 3,
          "per camera: cam2's empty stencil writes nothing (" + std::to_string(written) + ")");
    auto kept = [&](const char* cam, int x) {
        int w = 0, h = 0;
        std::vector<uint8_t> m;
        if (!app::load_stencil((root / "masks" / cam / "f.png").string(), w, h, m)) return -1;
        return m[(size_t)(h / 2) * w + x] ? 1 : 0;
    };
    check(kept("cam0", 2) == 0 && kept("cam0", 17) == 1,
          "per camera: cam0 takes the input's shapes, the left half out");
    check(kept("cam1", 2) == 1 && kept("cam1", 17) == 0,
          "per camera: cam1 takes its own, the right half out");
    check(!fs::exists(root / "masks" / "cam2" / "f.png"), "per camera: cam2 has no mask");
    fs::remove_all(root, ec);
}

// One file per camera: data-camera says whose it is, a file that names a
// camera brings its set's others, and a stencil goes to a set and back.
void test_camera_sets() {
    namespace fs = std::filesystem;
    app::MaskShape box;
    box.kind = app::MaskShape::Kind::Rect;
    box.remove = true;
    box.cx = 0.1f; box.cy = 0.1f; box.rx = 0.4f; box.ry = 0.4f;
    std::vector<app::MaskShape> back;
    std::string title, camera, err;
    const std::string one = app::write_mask_svg({box}, "Rig", "cam0/sub & more");
    check(app::read_mask_svg(one, back, title, err, &camera) && camera == "cam0/sub & more" &&
              title == "Rig" && back.size() == 1,
          "camera sets: data-camera round trip, '/' and '&' included");
    check(app::read_mask_svg(app::write_mask_svg({box}, "Rig"), back, title, err, &camera) &&
              camera.empty(),
          "camera sets: a file for every camera names none");

    const fs::path dir = fs::temp_directory_path() / "spirula_frame_mask_test_sets";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    app::MaskShape wide = box;
    wide.rx = 0.9f;
    app::save_mask_svg((dir / "a-cam0.svg").string(), {box}, "Rig", err, "cam0");
    app::save_mask_svg((dir / "a-cam1.svg").string(), {box, wide}, "Rig", err, "cam1");
    app::save_mask_svg((dir / "other-cam2.svg").string(), {box}, "Other rig", err, "cam2");
    app::save_mask_svg((dir / "shared.svg").string(), {wide}, "Rig", err);
    app::MaskSet set;
    check(app::load_mask_svg_set((dir / "a-cam1.svg").string(), set, title, err) &&
              set.cameras.size() == 2 && set.cameras["cam0"].size() == 1 &&
              set.cameras["cam1"].size() == 2 && set.shapes.empty(),
          "camera sets: one file brings its set, not another title's nor a shared file");
    check(app::load_mask_svg_set((dir / "shared.svg").string(), set, title, err) &&
              !set.per_camera() && set.shapes.size() == 1,
          "camera sets: a file naming no camera stands alone");
    fs::remove_all(dir, ec);

    app::FrameStencil st;
    st.detect_border = true;
    st.mask.shapes = {wide};
    set = app::MaskSet{};
    set.cameras["cam0"] = {box};
    set.cameras["cam1"] = {};
    app::apply_mask_set(st, set);
    check(st.mask.shapes.empty() && st.cameras.size() == 2 &&
              st.cameras["cam0"].mask.shapes.size() == 1 && st.cameras["cam1"].mask.shapes.empty() &&
              st.cameras["cam0"].detect_border,
          "camera sets: applied, each named camera its own, borders kept");
    app::FrameStencil ring;
    ring.detect_border = true;
    app::MaskShape lens;
    lens.kind = app::MaskShape::Kind::Ellipse;
    lens.cx = lens.cy = 0.5f;
    lens.rx = lens.ry = 0.45f;
    app::MaskSet edited;
    edited.cameras["cam0"] = {lens, box};
    edited.cameras["cam1"] = {box};
    app::apply_mask_set(ring, edited);
    check(!ring.cameras["cam0"].detect_border && ring.cameras["cam1"].detect_border,
          "camera sets: a list that keeps first turns its camera's fit off, the other keeps it");
    const app::MaskSet round = app::mask_set_of(st);
    check(round.per_camera() && round.cameras.size() == 2 &&
              round.cameras.at("cam0").size() == 1,
          "camera sets: cameras that differ stay per camera");
    st.cameras["cam1"].mask.shapes = {box};
    const app::MaskSet same = app::mask_set_of(st);
    check(!same.per_camera() && same.shapes.size() == 1,
          "camera sets: cameras that agree are one list");
}

}  // namespace

int main() {
    test_fill_matches_ray_cast();
    test_path_spelling();
    test_path_fill();
    test_path_order();
    test_format_extreme_values_do_not_truncate();
    test_path_point_matches_fill_boundary();
    test_stroke_fill();
    test_svg_round_trip();
    test_svg_hand_made();
    test_border_adjustment();
    test_bezier();
    test_per_camera_stencil();
    test_camera_sets();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
