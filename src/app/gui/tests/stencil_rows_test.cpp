// stencil_rows_test -- app/gui/StencilRows.h: which row a row follows, an edit
// carried down the rows that matched, an input's lenses split and joined, the
// panel's edits passed on, new inputs, and naming a saved set.

#include "app/gui/StencilRows.h"
#include "core/SourcePath.h"

#include <cstdio>
#include <string>

using gui::StencilPart;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

app::MaskShape box(float x0, bool remove = true) {
    app::MaskShape s;
    s.kind = app::MaskShape::Kind::Rect;
    s.remove = remove;
    s.cx = x0;
    s.cy = 0.8f;
    s.rx = x0 + 0.1f;
    s.ry = 1.0f;
    return s;
}

gui::PrepInput fisheye(bool border = true) {
    gui::PrepInput in;
    in.stencil.detect_border = border;
    return in;
}

const app::CameraStencil& at(const std::vector<gui::PrepInput>& in,
                             const std::vector<gui::StencilRow>& rows, size_t r) {
    return gui::row_stencil(in, rows[r]);
}

// Two dual-fisheye files, a phone clip, and a third dual file.
std::vector<std::vector<std::string>> mixed_keys() {
    return {{"cam0", "cam1"}, {"cam0", "cam1"}, {""}, {"cam0", "cam1"}};
}

void test_rows() {
    const auto rows = gui::stencil_rows(mixed_keys());
    check(rows.size() == 7, "a row per lens, one for the phone");
    check(rows[0].above == -1 && rows[1].above == 0,
          "the first row follows nothing; its second lens follows its first");
    check(rows[2].above == 0 && rows[3].above == 2,
          "an input's first lens follows the first lens above; its second, its own first");
    check(rows[4].above == 3, "a camera the input above lacks follows the row before it");
    check(rows[5].above == 4 && rows[6].above == 5,
          "an input further up is never followed, even with the same lens");

    // Two GoPro sphere files, a dual-fisheye one, a third sphere, a fourth
    // dual: the third sphere's border is off and the lens above it cuts.
    const auto smoke = gui::stencil_rows({{""}, {""}, {"cam0", "cam1"}, {""}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> in = {fisheye(false), fisheye(false), fisheye(), fisheye(false),
                                      fisheye()};
    check(gui::row_follows(in, smoke, 1, StencilPart::Border) &&
              gui::row_follows(in, smoke, 3, StencilPart::Border),
          "smoke list: the second sphere and the second lens follow");
    check(!gui::row_follows(in, smoke, 4, StencilPart::Border) &&
              !gui::row_follows(in, smoke, 4, StencilPart::Areas),
          "smoke list: a sphere under a cutting lens follows neither its fit nor its areas");
    check(gui::row_follows(in, smoke, 1, StencilPart::Areas),
          "smoke list: the second sphere follows the first's areas");
    check(!gui::row_follows(in, smoke, 5, StencilPart::Border) &&
              gui::row_follows(in, smoke, 6, StencilPart::Border),
          "smoke list: the dual under the sphere has its own first lens");
}

void test_border_chain() {
    const auto rows = gui::stencil_rows({{""}, {""}, {""}, {""}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye(), fisheye(false), fisheye(false)};
    check(gui::row_follows(in, rows, 1, StencilPart::Border) &&
              !gui::row_follows(in, rows, 2, StencilPart::Border) &&
              gui::row_follows(in, rows, 3, StencilPart::Border),
          "equal borders read as following");
    gui::set_row_border(in, rows, 0, true, 0.03f);
    check(at(in, rows, 1).shrink == 0.03f && at(in, rows, 2).detect_border == false,
          "a shrink moves the row that followed and stops at the one that did not");
    gui::set_row_border(in, rows, 2, true, 0.05f);
    check(at(in, rows, 3).detect_border && at(in, rows, 3).shrink == 0.05f &&
              at(in, rows, 0).shrink == 0.03f,
          "turning a row on carries down, never up");
    gui::follow_above(in, rows, 2, StencilPart::Border);
    check(at(in, rows, 2).shrink == 0.03f && at(in, rows, 3).shrink == 0.03f,
          "same as above takes the row above, and its followers come along");
    gui::set_row_border(in, rows, 1, false, 0.0f);
    check(!at(in, rows, 3).detect_border && at(in, rows, 1).shrink == 0.03f,
          "off keeps the shrink for later");
}

void test_lenses() {
    const auto rows = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye()};
    gui::set_row_border(in, rows, 0, true, 0.02f);
    check(!in[0].stencil.per_camera() && in[0].stencil.shrink == 0.02f &&
              in[1].stencil.shrink == 0.02f,
          "lenses that still agree stay one stencil");
    app::MaskSet stick;
    stick.shapes = {box(0.4f)};
    gui::set_row_areas(in, rows, 0, stick);
    check(!in[0].stencil.per_camera() && in[0].stencil.mask.shapes.size() == 1 &&
              !in[1].stencil.per_camera() && in[1].stencil.mask.shapes.size() == 1,
          "drawing on the first lens goes on the second too, and down");
    gui::set_row_areas(in, rows, 1, app::MaskSet{});
    check(in[0].stencil.per_camera() && in[0].stencil.cameras.at("cam0").mask.shapes.size() == 1 &&
              in[0].stencil.cameras.at("cam1").mask.shapes.empty(),
          "a second lens set apart splits the input");
    check(!in[1].stencil.per_camera() && in[1].stencil.mask.shapes.size() == 1,
          "and the input below still follows the first lens");

    app::MaskSet rig;
    rig.cameras["cam0"] = {box(0.1f)};
    rig.cameras["cam1"] = {box(0.2f), box(0.3f)};
    gui::set_row_areas(in, rows, 0, rig);
    check(at(in, rows, 0).mask.shapes.size() == 1 && at(in, rows, 1).mask.shapes.empty(),
          "the lens set apart keeps its own when the first changes");
    check(at(in, rows, 2).mask.shapes.size() == 1 && at(in, rows, 3).mask.shapes.size() == 2,
          "a per-camera set gives each lens that follows its own list");

    std::vector<app::MaskSet> sets = {stick, rig};
    check(gui::row_set(sets, rows[0], at(in, rows, 0)) == 1 &&
              gui::row_set(sets, rows[3], at(in, rows, 3)) == 1,
          "a row drawing a set's list for its lens is named after the set");
    check(gui::common_set(sets, in, rows) == -1, "a lens set apart: no common set");
    gui::set_row_areas(in, rows, 1, rig);
    check(gui::common_set(sets, in, rows) == 1, "every lens drawing its list: the common set");

    // A set saved with only the first lens drawn on, the usual way.
    std::vector<gui::PrepInput> two = {fisheye(), fisheye()};
    app::MaskSet one;
    one.cameras["cam0"] = {box(0.7f, /*remove=*/false), box(0.6f)};
    gui::set_row_areas(two, rows, 0, one);
    check(gui::row_follows(two, rows, 1, StencilPart::Areas) &&
              at(two, rows, 1).mask.shapes.size() == 2 && at(two, rows, 3).mask.shapes.size() == 2,
          "a set naming only the first lens goes on the second too, and down");
    const std::vector<app::MaskSet> ones = {one};
    check(gui::row_set(ones, rows[1], at(two, rows, 1)) == 0,
          "and the second lens is named after it");
    check(gui::common_set(ones, two, rows) == -1,
          "but a preset does not name it: a batch would draw it on the first lens only");
    gui::set_row_areas(two, rows, 1, app::MaskSet{});
    gui::set_row_areas(two, rows, 0, stick);
    check(at(two, rows, 1).mask.shapes.empty() && at(two, rows, 3).mask.shapes.size() == 1,
          "a second lens set apart stays so; the input below follows its first lens");

    app::MaskSet keep;
    keep.shapes = {box(0.0f, /*remove=*/false)};
    gui::set_row_areas(in, rows, 3, keep);
    check(at(in, rows, 3).detect_border && at(in, rows, 3).mask.shapes.size() == 1,
          "drawn areas leave the border alone, even a list starting with a keep shape");
    gui::set_row_border(in, rows, 1, false, 0.0f);
    gui::set_row_areas(in, rows, 1, stick);
    check(!at(in, rows, 1).detect_border, "and do not turn an off border back on");
}

void test_panel_edit() {
    const auto rows = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}, {""}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye(), fisheye(false)};
    // The panel with separate areas on draws on cam0 only.
    gui::settle_follow(in, rows);
    auto before = gui::row_stencils(in, rows);
    in[0].stencil.cameras["cam0"] = in[0].stencil;
    in[0].stencil.cameras["cam1"] = in[0].stencil;
    in[0].stencil.cameras["cam0"].mask.shapes = {box(0.4f)};
    gui::follow_edit(in, rows, before, 0);
    check(at(in, rows, 1).mask.shapes.empty(),
          "the panel's other lens is left as the panel left it");
    check(at(in, rows, 2).mask.shapes.size() == 1 && at(in, rows, 3).mask.shapes.size() == 1,
          "the input below follows the edit, on both its lenses");
    check(at(in, rows, 4).mask.shapes.empty(),
          "the sphere under it, whose fit differs, does not");

    gui::settle_follow(in, rows);
    before = gui::row_stencils(in, rows);
    in[1].stencil.cameras["cam0"] = in[1].stencil;
    in[1].stencil.cameras["cam1"] = in[1].stencil;
    in[1].stencil.cameras["cam1"].shrink = 0.04f;
    gui::follow_edit(in, rows, before, 1);
    check(!at(in, rows, 4).detect_border, "an off border is not turned on by a shrink above");
    check(!gui::row_follows(in, rows, 3, StencilPart::Border),
          "a following lens the panel changed is its own after");

    gui::settle_follow(in, rows);
    before = gui::row_stencils(in, rows);
    in[2].stencil.mask.shapes = {box(0.7f)};
    gui::follow_edit(in, rows, before, 2);
    check(at(in, rows, 0).mask.shapes.size() == 1 && at(in, rows, 3).mask.shapes.size() == 1,
          "nothing above the edited input moves");

    gui::settle_follow(in, rows);
    before = gui::row_stencils(in, rows);
    gui::follow_edit(in, rows, before, 0);
    check(at(in, rows, 2).mask.shapes.size() == 1, "an edit that changed nothing passes nothing");
}

// Explicit choices that happen to equal what is above them.
void test_explicit() {
    const auto rows = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}, {""}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye(), fisheye()};
    check(gui::row_follows(in, rows, 4, StencilPart::Areas), "a row starts out following");
    gui::set_row_areas(in, rows, 4, app::MaskSet{});
    check(!gui::row_follows(in, rows, 4, StencilPart::Areas),
          "None picked under a row that draws nothing stays None, not same as above");
    app::MaskSet rig;
    rig.cameras["cam0"] = {box(0.1f, /*remove=*/false), box(0.2f)};
    gui::set_row_areas(in, rows, 0, rig);
    check(at(in, rows, 4).mask.shapes.empty(), "and ignores a set drawn above it later");
    check(gui::row_follows(in, rows, 2, StencilPart::Areas) &&
              at(in, rows, 2).mask.shapes.size() == 2,
          "the lens below follows the set");
    gui::set_row_areas(in, rows, 2, rig);
    check(!gui::row_follows(in, rows, 2, StencilPart::Areas),
          "the set picked again under the same set is the row's own");
    gui::set_row_areas(in, rows, 0, app::MaskSet{});
    check(at(in, rows, 1).mask.shapes.empty() && at(in, rows, 2).mask.shapes.size() == 2,
          "and an own row ignores the row above");
    gui::set_row_border(in, rows, 3, true, 0.01f);
    check(!gui::row_follows(in, rows, 3, StencilPart::Border),
          "Cut away picked under Cut away stays Cut away");
    gui::set_row_border(in, rows, 2, true, 0.05f);
    check(at(in, rows, 3).shrink == 0.01f, "so a shrink above does not reach it");
    gui::follow_above(in, rows, 3, StencilPart::Border);
    check(gui::row_follows(in, rows, 3, StencilPart::Border) && at(in, rows, 3).shrink == 0.05f,
          "same as above picked again follows again");

    std::vector<gui::PrepInput> fewer = {in[0], in[2]};
    const auto two = gui::stencil_rows({{"cam0", "cam1"}, {""}});
    fewer[1].stencil.mask.shapes = {box(0.6f)};
    fewer[1].stencil_follow[""].areas = true;
    gui::settle_follow(fewer, two);
    check(!gui::row_follows(fewer, two, 2, StencilPart::Areas) &&
              fewer[1].stencil.mask.shapes.size() == 1,
          "a following row that no longer matches keeps its value as its own");
}

void test_adopt() {
    std::vector<gui::PrepInput> in = {fisheye(), fisheye(), fisheye(false)};
    in[0].stencil.shrink = 0.05f;
    in[0].stencil.mask.shapes = {box(0.4f)};
    in[1].stencil.mask.shapes = {};
    const auto rows = gui::stencil_rows({{""}, {""}, {""}});
    gui::adopt_rows_above(in, rows, 1, {true, true, false});
    check(in[1].stencil.mask.shapes.size() == 1 && in[1].stencil.shrink == 0.05f,
          "a new input of the same lens takes the areas and the shrink above");
    check(in[2].stencil.mask.shapes.empty(), "a new input of another kind of lens takes nothing");

    std::vector<app::MaskSet> sets(1);
    sets[0].shapes = {box(0.4f)};
    check(gui::common_set(sets, in, rows) == -1, "a row without the set: no common set");
    in[2].stencil.mask.shapes = {box(0.4f)};
    check(gui::common_set(sets, in, rows) == 0, "every row drawing it: the common set");
}

}  // namespace

int main() {
    test_rows();
    test_border_chain();
    test_lenses();
    test_panel_edit();
    test_explicit();
    test_adopt();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
