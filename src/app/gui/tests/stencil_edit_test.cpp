// stencil_edit_test -- app/gui/StencilEdit.h and StencilPreset.h with no GUI:
// what each tool's stroke becomes, hit tests and moves, a pen shape's points,
// undo, and saving a set of drawn areas and finding it again by name.

#include "app/gui/StencilEdit.h"
#include "app/gui/StencilPreset.h"
#include "core/SourcePath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using Kind = app::MaskShape::Kind;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

bool near_(float a, float b) { return std::fabs(a - b) < 1e-5f; }

gui::ShapeStroke stroke(gui::ShapeKind k, std::vector<float> pts, float r = 0.0f) {
    gui::ShapeStroke s;
    s.kind = k;
    s.pts = std::move(pts);
    s.brush_radius = r;
    return s;
}

void test_from_stroke() {
    app::MaskShape m;
    // A 400 x 200 canvas.
    check(gui::stencil_shape_from_stroke(stroke(gui::ShapeKind::Box, {300, 150, 100, 50}),
                                         400, 200, true, m) &&
              m.kind == Kind::Rect && m.remove && near_(m.cx, 0.25f) && near_(m.cy, 0.25f) &&
              near_(m.rx, 0.75f) && near_(m.ry, 0.75f),
          "box: corners sorted and normalized");
    check(gui::stencil_shape_from_stroke(stroke(gui::ShapeKind::Ellipse, {100, 50, 300, 150}),
                                         400, 200, false, m) &&
              m.kind == Kind::Ellipse && !m.remove && near_(m.cx, 0.5f) && near_(m.rx, 0.25f) &&
              near_(m.ry, 0.25f),
          "ellipse: centre and radii per axis; the keep flag passes through");
    check(!gui::stencil_shape_from_stroke(stroke(gui::ShapeKind::Box, {10, 10, 11, 40}), 400,
                                          200, true, m),
          "a box one pixel wide is a click, not a shape");
    check(gui::stencil_shape_from_stroke(
              stroke(gui::ShapeKind::Polygon, {0, 0, 400, 0, 200, 200}), 400, 200, true, m) &&
              m.kind == Kind::Path && m.pts.size() == 6 && near_(m.pts[2], 1.0f) &&
              near_(m.pts[5], 1.0f),
          "polygon: a path in normalized corners");
    check(!gui::stencil_shape_from_stroke(stroke(gui::ShapeKind::Lasso, {0, 0, 10, 10}), 400,
                                          200, true, m),
          "a lasso of two points is not a shape");
    check(gui::stencil_shape_from_stroke(stroke(gui::ShapeKind::Box, {-40, -20, 100, 50}), 400,
                                         200, true, m) &&
              near_(m.cx, -0.1f) && near_(m.cy, -0.1f) && near_(m.rx, 0.25f),
          "a box started off the picture keeps its corner there");

    // A brush 10 canvas px round on a 2:1 canvas is 10 px on each axis.
    std::vector<float> drag;
    for (int i = 0; i <= 100; i++) drag.insert(drag.end(), {100.0f + i, 100.0f});
    check(gui::stencil_shape_from_stroke(stroke(gui::ShapeKind::Brush, drag, 10.0f), 400, 200,
                                         true, m) &&
              m.kind == Kind::Stroke && near_(m.rx, 10.0f / 400) && near_(m.ry, 10.0f / 200),
          "brush: round in canvas pixels, so rx and ry differ on a 2:1 canvas");
    check(m.pts.size() >= 4 && m.pts.size() < drag.size() / 2 &&
              near_(m.pts[m.pts.size() - 2], 200.0f / 400),
          "brush: every-frame samples thinned, the last point kept (" +
              std::to_string(m.pts.size() / 2) + " points)");
}

void test_hit_and_move() {
    app::MaskShape s;
    s.kind = Kind::Stroke;
    s.rx = 0.05f;
    s.ry = 0.1f;
    s.pts = {0.2f, 0.5f, 0.8f, 0.5f};
    check(gui::stencil_contains(s, 0.5f, 0.59f) && !gui::stencil_contains(s, 0.5f, 0.61f) &&
              gui::stencil_contains(s, 0.84f, 0.5f) && !gui::stencil_contains(s, 0.86f, 0.5f),
          "stroke hit: within ry above the line, rx past the end");
    gui::stencil_move(s, 0.1f, -0.1f);
    check(near_(s.pts[0], 0.3f) && near_(s.pts[3], 0.4f), "stroke moves by its points");
    float u[3], v[3];
    check(gui::stencil_handles(s, u, v) == 0, "a stroke has no resize handles");

    app::MaskShape r;
    r.kind = Kind::Rect;
    r.cx = 0.1f; r.cy = 0.1f; r.rx = 0.3f; r.ry = 0.2f;
    check(gui::stencil_handles(r, u, v) == 3 && near_(u[0], 0.2f) && near_(v[0], 0.15f),
          "rect: the first handle is its centre");
    gui::stencil_move_handle(r, 0, 0.5f, 0.5f);
    check(near_(r.cx, 0.4f) && near_(r.ry, 0.55f), "rect: the centre handle moves it whole");
}

std::vector<uint8_t> raster(const app::MaskShape& s, int W, int H) {
    app::FrameMask m;
    m.shapes.push_back(s);
    std::vector<uint8_t> out;
    std::string err;
    app::rasterize_frame_mask(m, W, H, out, err);
    return out;
}

// The zoomed panel rasterizes only the part of the frame on screen: a crop
// has to land on exactly the full raster's pixels there.
void test_crop() {
    const int W = 400, H = 200;
    const float u0 = 0.25f, v0 = 0.25f, u1 = 0.75f, v1 = 0.75f;
    std::vector<app::MaskShape> shapes(5);
    shapes[0].kind = Kind::Rect;
    shapes[0].cx = -0.2f; shapes[0].cy = -0.1f; shapes[0].rx = 0.413f; shapes[0].ry = 0.587f;
    shapes[1].kind = Kind::Ellipse;
    shapes[1].cx = 0.9f; shapes[1].cy = 0.5f; shapes[1].rx = 0.3f; shapes[1].ry = 0.2f;
    shapes[2].kind = Kind::Path;
    shapes[2].pts = {-0.3f, 0.2f, 0.61f, 0.33f, 0.4f, 0.9f};
    shapes[3].kind = Kind::Stroke;
    shapes[3].rx = 0.02f; shapes[3].ry = 0.04f;
    shapes[3].pts = {0.1f, 0.62f, 0.7f, 0.38f};
    shapes[4].kind = Kind::Bezier;
    shapes[4].pts = {0.3f, 0.2f, 0.4f, 0.3f, 0.5f, 0.4f,  0.7f, 0.5f, 0.6f, 0.7f, 0.5f, 0.9f};
    const char* names[5] = {"rect", "ellipse", "path", "stroke", "bezier"};
    for (int k = 0; k < 5; k++) {
        const std::vector<uint8_t> full = raster(shapes[(size_t)k], W, H);
        const int cw = (int)((u1 - u0) * W), ch = (int)((v1 - v0) * H);
        const std::vector<uint8_t> part =
            raster(gui::stencil_crop(shapes[(size_t)k], u0, v0, u1, v1), cw, ch);
        int differ = 0, dropped = 0;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++) {
                const uint8_t a = part[(size_t)y * cw + x];
                differ += a != full[(size_t)(y + (int)(v0 * H)) * W + x + (int)(u0 * W)];
                dropped += a == 0;
            }
        check(dropped > 0 && differ <= 2, std::string("crop: ") + names[k] +
                                              " matches the full raster there (" +
                                              std::to_string(differ) + " pixels differ)");
    }
}

void test_pen() {
    // Four corners on a 400 x 200 canvas: (100,50) (300,50) (300,150) (100,150).
    std::vector<float> px;
    const float corners[] = {100, 50, 300, 50, 300, 150, 100, 150};
    for (int i = 0; i < 8; i += 2)
        px.insert(px.end(), {corners[i], corners[i + 1], corners[i], corners[i + 1],
                             corners[i], corners[i + 1]});
    app::MaskShape s;
    check(gui::stencil_shape_from_pen(px, 400, 200, true, s) && s.kind == Kind::Bezier &&
              s.remove && s.pts.size() == 24 && near_(s.pts[2], 0.25f) && near_(s.pts[3], 0.25f),
          "pen: anchors normalized per axis, the remove flag passes through");
    const std::vector<float> two = {10, 10, 10, 10, 10, 10, 50, 50, 50, 50, 50, 50};
    check(!gui::stencil_shape_from_pen(two, 400, 200, true, s), "pen: two corners are no shape");
    check(gui::stencil_shape_from_pen({0, 30, 10, 10, 20, -10, 40, -10, 50, 10, 60, 30}, 400,
                                      200, false, s),
          "pen: two anchors with handles enclose a lens");
    gui::stencil_shape_from_pen(px, 400, 200, true, s);
    check(gui::stencil_contains(s, 0.5f, 0.5f) && !gui::stencil_contains(s, 0.2f, 0.5f),
          "pen: inside test");

    // Hits: radii in canvas pixels.
    gui::PenHit h = gui::pen_hit(s, 0.25f + 3.0f / 400, 0.25f, 400, 200, 6.0f, true);
    check(h.part == gui::PenPart::Anchor && h.index == 0, "pen hit: 3 px from anchor 0");
    h = gui::pen_hit(s, 0.5f, 0.25f + 4.0f / 200, 400, 200, 6.0f, true);
    check(h.part == gui::PenPart::Segment && h.index == 0 && std::fabs(h.t - 0.5f) < 1e-3f,
          "pen hit: 4 px under the top edge's middle is segment 0 at t=0.5");
    h = gui::pen_hit(s, 0.5f, 0.5f, 400, 200, 6.0f, true);
    check(h.part == gui::PenPart::None, "pen hit: the middle is nothing");

    // Insert keeps the outline; delete stops at two.
    const std::vector<uint8_t> before = raster(s, 120, 80);
    const int k = gui::pen_insert(s, 0, 0.5f);
    check(k == 1 && s.pts.size() == 30 && near_(s.pts[8], 0.5f) && near_(s.pts[9], 0.25f),
          "pen insert: a new anchor at the top edge's middle");
    check(raster(s, 120, 80) == before, "pen insert: the outline is unchanged");
    check(gui::pen_delete(s, 1) && s.pts.size() == 24 && raster(s, 120, 80) == before,
          "pen delete: back to the square");
    check(gui::pen_delete(s, 0) && gui::pen_delete(s, 0) && !gui::pen_delete(s, 0) &&
              s.pts.size() == 12,
          "pen delete: refused at two anchors");

    // Handles: a smooth anchor's other handle swings opposite at its own
    // length in PIXELS; split leaves it; an anchor carries both.
    app::MaskShape c;
    c.kind = Kind::Bezier;
    c.pts = {0.4f, 0.5f, 0.5f, 0.5f, 0.6f, 0.5f,   0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
    check(gui::pen_is_smooth(c, 0, 400, 200) && !gui::pen_is_smooth(c, 1, 400, 200),
          "pen: opposite handles are smooth, a corner is not");
    gui::pen_move_point(c, 0, gui::PenPart::Out, 0.5f, 0.3f, 400, 200, false);
    check(near_(c.pts[4], 0.5f) && near_(c.pts[5], 0.3f) && near_(c.pts[0], 0.5f) &&
              near_(c.pts[1], 0.5f + 40.0f / 200),
          "pen: out turned straight up, in swings straight down 40 px -- its own length");
    gui::pen_move_point(c, 0, gui::PenPart::In, 0.45f, 0.6f, 400, 200, true);
    check(near_(c.pts[4], 0.5f) && near_(c.pts[5], 0.3f) && !gui::pen_is_smooth(c, 0, 400, 200),
          "pen: split moves one handle alone and leaves a cusp");
    gui::pen_move_point(c, 0, gui::PenPart::Anchor, 0.6f, 0.6f, 400, 200, false);
    check(near_(c.pts[2], 0.6f) && near_(c.pts[3], 0.6f) && near_(c.pts[0], 0.55f) &&
              near_(c.pts[5], 0.4f),
          "pen: an anchor carries its handles");
    gui::pen_make_corner(c, 0);
    check(near_(c.pts[0], 0.6f) && near_(c.pts[5], 0.6f), "pen: a corner has its handles on it");
    gui::stencil_move(c, 0.1f, 0.0f);
    check(near_(c.pts[0], 0.7f) && near_(c.pts[6], 0.9f), "pen: a shape moves by all its points");
}

void test_history() {
    gui::StencilHistory h;
    app::FrameStencil st;
    app::MaskShape a;
    h.push(st);
    st.mask.shapes.push_back(a);
    h.push(st);
    st.mask.shapes.push_back(a);
    check(h.undo(st) && st.mask.shapes.size() == 1, "undo takes back the last add");
    check(h.undo(st) && st.mask.shapes.empty() && !h.can_undo(), "and the one before");
    check(h.redo(st) && st.mask.shapes.size() == 1 && h.can_redo(), "redo puts it back");
    h.push(st);
    st.cameras["cam1"].mask.shapes.push_back(a);
    st.cameras["cam1"].detect_border = true;
    check(!h.can_redo(), "a new change drops what redo held");
    check(h.undo(st) && !st.per_camera(), "undo covers a camera's own stencil too");
}

app::MaskSet one_list(std::vector<app::MaskShape> shapes) {
    app::MaskSet set;
    set.shapes = std::move(shapes);
    return set;
}

void test_presets() {
    const fs::path root = fs::temp_directory_path() / "ss_stencil_edit_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
#ifdef _WIN32
    _putenv_s("APPDATA", root.string().c_str());
#else
    setenv("XDG_CONFIG_HOME", root.string().c_str(), 1);
#endif
    std::vector<app::MaskShape> shapes(1);
    shapes[0].kind = Kind::Rect;
    shapes[0].remove = true;
    shapes[0].cx = 0.0f; shapes[0].cy = 0.9f; shapes[0].rx = 1.0f; shapes[0].ry = 1.0f;
    std::string path, err;
    check(gui::save_stencil_preset("Selfie stick", one_list(shapes), path, err), "save: " + err);
    check(fs::path(path).extension() == ".svg" &&
              fs::path(path).parent_path() == fs::path(gui::stencil_preset_dir()),
          "saved as an .svg in the stencil folder: " + path);
    auto list = gui::list_stencil_presets();
    check(list.size() == 1 && list[0].name == "Selfie stick" && list[0].cameras.empty(),
          "listed by its title");
    app::MaskSet back;
    check(gui::load_stencil_preset("Selfie stick", back, err) && back.shapes.size() == 1 &&
              !back.per_camera() && back.shapes[0].kind == Kind::Rect &&
              near_(back.shapes[0].cy, 0.9f),
          "loaded back by name");
    check(gui::load_stencil_preset(path, back, err), "and by path");
    shapes.push_back(shapes[0]);
    check(gui::save_stencil_preset("Selfie stick", one_list(shapes), path, err) &&
              gui::list_stencil_presets().size() == 1 &&
              gui::load_stencil_preset("Selfie stick", back, err) && back.shapes.size() == 2,
          "saving the same name replaces it");
    check(!gui::load_stencil_preset("nothing by this name", back, err),
          "an unknown name is refused");

    // A set drawn per camera: a file each, listed once, loaded back whole.
    app::MaskSet rig;
    rig.cameras["cam0"] = {shapes[0]};
    rig.cameras["cam1"] = shapes;
    err.clear();
    check(gui::save_stencil_preset("Rig", rig, path, err), "per camera: save: " + err);
    const fs::path dir = gui::stencil_preset_dir();
    check(fs::exists(dir / "rig-cam0.svg") && fs::exists(dir / "rig-cam1.svg") &&
              !fs::exists(dir / "rig.svg"),
          "per camera: rig-cam0.svg and rig-cam1.svg, no shared file");
    list = gui::list_stencil_presets();
    const gui::StencilPreset* entry = nullptr;
    for (const gui::StencilPreset& p : list)
        if (p.name == "Rig") entry = &p;
    check(list.size() == 2 && entry && entry->cameras == std::vector<std::string>{"cam0", "cam1"} &&
              gui::stencil_preset_label(*entry) == "Rig [cam0, cam1]",
          "per camera: one entry, its cameras named");
    check(gui::load_stencil_preset("Rig", back, err) && back.per_camera() &&
              back.cameras.size() == 2 && back.cameras["cam0"].size() == 1 &&
              back.cameras["cam1"].size() == 2 && back.shapes.empty(),
          "per camera: loaded by name, each camera its own");
    check(gui::load_stencil_preset((dir / "rig-cam1.svg").string(), back, err) &&
              back.cameras.size() == 2,
          "per camera: loading one file brings the other");
    check(gui::save_stencil_preset("Rig", one_list(shapes), path, err) &&
              !fs::exists(dir / "rig-cam0.svg") && !fs::exists(dir / "rig-cam1.svg") &&
              gui::load_stencil_preset("Rig", back, err) && !back.per_camera(),
          "per camera: saved again as one list, the camera files go");

    // A run keeps its inputs' shapes beside the dataset: one file for an
    // input, one per camera when its cameras differ.
    const fs::path ws = root / "capture_dataset";
    fs::create_directories(ws / gui::kDatasetStencilDir, ec);
    { std::ofstream stale((ws / gui::kDatasetStencilDir / "old.svg").string()); stale << "<svg/>"; }
    app::FrameStencil same, differ;
    same.cameras["cam0"].mask.shapes = shapes;
    same.cameras["cam1"].mask.shapes = shapes;
    differ.mask.shapes = shapes;    // a fallback no camera uses
    differ.cameras["cam0"].mask.shapes = {shapes[0]};
    differ.cameras["cam1"].mask.shapes = shapes;
    err.clear();
    const auto written = gui::save_dataset_stencils(
        ws.string(),
        {{(ws / "images").string(), one_list(shapes)},
         {"/videos/front.mp4", app::MaskSet{}},
         {"/videos/back.insv", app::mask_set_of(same)},
         {"/videos/dual.insv", app::mask_set_of(differ)},
         {"/other/dual.insv", app::mask_set_of(differ)}},
        err);
    std::string names;
    for (const std::string& f : written) names += fs::path(f).filename().string() + " ";
    check(written.size() == 6 && err.empty(),
          "dataset stencils: one per input, two per differing one: " + names);
    check(!fs::exists(ws / gui::kDatasetStencilDir / "old.svg"),
          "dataset stencils: the folder holds only this run's");
    check(names == "capture-dataset-images.svg back.svg dual-cam0.svg dual-cam1.svg "
                   "dual-2-cam0.svg dual-2-cam1.svg ",
          "dataset stencils: cameras that agree share a file, a second dual gets its own stem");
    const auto kept = gui::list_dataset_stencils(ws.string());
    std::string labels;
    for (const gui::StencilPreset& p : kept) labels += gui::stencil_preset_label(p) + " | ";
    check(kept.size() == 4, "dataset stencils: four sets listed: " + labels);
    bool dual_back = false;
    for (const gui::StencilPreset& p : kept)
        if (p.cameras.size() == 2 && p.name == "capture_dataset - dual.insv")
            dual_back = gui::load_stencil_preset(p.path, back, err) &&
                        back.cameras["cam0"].size() == 1 && back.cameras["cam1"].size() == 2;
    check(dual_back, "dataset stencils: the first dual's set loads back per camera, not mixed "
                     "with the second's");
    check(gui::list_dataset_stencils("").empty(), "dataset stencils: no workspace, nothing listed");
    fs::remove_all(root, ec);
}

}  // namespace

int main() {
    test_from_stroke();
    test_hit_and_move();
    test_crop();
    test_pen();
    test_history();
    test_presets();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
