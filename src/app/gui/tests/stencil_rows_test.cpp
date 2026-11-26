// stencil_rows_test -- app/gui/StencilRows.h: which row a row follows, an edit
// carried down the rows that matched, an input's lenses split and joined, the
// panel's edits passed on, shared lens circles, new inputs, and saved sets.

#include "app/gui/StencilRows.h"
#include "core/SourcePath.h"

#include <cstdio>
#include <map>
#include <string>

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

using Rows = std::vector<size_t>;
using Setups = std::vector<std::string>;
const std::string kInsv = "insv", kOsv = "osv", kNone;

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
    check(gui::row_follows(in, smoke, 1) && gui::row_follows(in, smoke, 3),
          "smoke list: the second sphere and the second lens follow the areas above");
    check(!gui::row_follows(in, smoke, 4),
          "smoke list: a sphere under a cutting lens does not follow its areas");
    check(!gui::row_follows(in, smoke, 5) && gui::row_follows(in, smoke, 6),
          "smoke list: the dual under the sphere has its own first lens");
}

void test_own_borders() {
    const auto rows = gui::stencil_rows({{""}, {""}, {""}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye(), fisheye()};
    gui::settle_follow(in, rows, {kNone, kNone, kNone});
    check(gui::border_sharers(in, rows, 1).empty(), "a single-lens clip fits its own circle");
    gui::set_row_shrink(in, rows, 0, 0.03f);
    check(at(in, rows, 0).shrink == 0.03f && at(in, rows, 1).shrink == 0.01f,
          "a shrink stays on its row");
    gui::set_row_border(in, rows, 0, false, 0.0f);
    check(!at(in, rows, 0).detect_border && at(in, rows, 1).detect_border,
          "Off is the row's alone");
    check(at(in, rows, 0).shrink == 0.03f, "and keeps the shrink for later");

    const auto lenses = gui::stencil_rows({{"cam0", "cam1"}});
    std::vector<gui::PrepInput> one = {fisheye()};
    gui::set_row_shrink(one, lenses, 1, 0.02f);
    check(one[0].stencil.per_camera() && at(one, lenses, 0).shrink == 0.01f &&
              at(one, lenses, 1).shrink == 0.02f,
          "a lens's own shrink splits the input");
}

// Three dual-fisheye shots of one camera, a phone clip between (issue #420).
void test_shares() {
    const auto rows = gui::stencil_rows({{"cam0", "cam1"}, {""}, {"cam0", "cam1"}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye(false), fisheye(), fisheye()};
    Setups dual = {kInsv, kNone, kInsv, kInsv};
    gui::settle_follow(in, rows, dual);
    check(gui::border_sharers(in, rows, 0) == Rows{0, 3, 5},
          "each shot's first lens shares one circle, past the phone");
    check(gui::border_sharers(in, rows, 6) == Rows{1, 4, 6}, "and each second lens another");
    check(gui::border_sharers(in, rows, 2).empty(), "the phone clip shares nothing");

    gui::set_row_shrink(in, rows, 3, 0.04f);
    check(at(in, rows, 0).shrink == 0.04f && at(in, rows, 5).shrink == 0.04f &&
              at(in, rows, 1).shrink == 0.01f,
          "a shrink reaches the rows sharing the circle, not the other lens");

    gui::set_row_border(in, rows, 5, true, 0.04f);
    check(gui::border_sharers(in, rows, 0) == Rows{0, 3} && gui::border_sharers(in, rows, 5).empty(),
          "Cut away takes a row out of its share");
    gui::set_row_shrink(in, rows, 5, 0.02f);
    check(at(in, rows, 0).shrink == 0.04f, "and its shrink is its own");
    gui::share_border(in, rows, 5, 0);
    check(gui::border_sharers(in, rows, 0) == Rows{0, 3, 5} && at(in, rows, 5).shrink == 0.04f,
          "Shared with joins the picked row's circle and takes its shrink");

    gui::set_row_border(in, rows, 3, false, 0.0f);
    gui::set_row_border(in, rows, 5, false, 0.0f);
    check(gui::border_sharers(in, rows, 0).empty(), "a share left with one cutting row ends");
    gui::settle_follow(in, rows, dual);
    check(gui::border_sharers(in, rows, 0).empty() && gui::border_sharers(in, rows, 1).size() == 3,
          "rows met before keep their choice");

    // A fourth shot below: its lenses are new rows.
    in.push_back(fisheye());
    dual.push_back(kInsv);
    const auto more =
        gui::stencil_rows({{"cam0", "cam1"}, {""}, {"cam0", "cam1"}, {"cam0", "cam1"}, {"cam0", "cam1"}});
    gui::settle_follow(in, more, dual);
    check(gui::border_sharers(in, more, 8) == Rows{1, 4, 6, 8},
          "a new shot's lens joins the circle of the same lens above");
    check(gui::border_sharers(in, more, 7) == Rows{0, 7},
          "past the rows above that no longer cut");
    check(at(in, more, 7).shrink == 0.04f && at(in, more, 8).shrink == 0.01f,
          "each taking the shrink of the circle it joins");

    std::vector<gui::PrepInput> plain = {fisheye(), fisheye()};
    const auto two = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}});
    gui::settle_follow(plain, two);
    check(gui::border_sharers(plain, two, 0).empty(), "no setups, no shares");
}

// Dual-fisheye files of different cameras: a DJI .osv and an Insta360 .insv.
void test_setups() {
    const auto pair = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye()};
    gui::settle_follow(in, pair, {kOsv, kInsv});
    for (size_t r = 0; r < pair.size(); r++)
        check(gui::border_sharers(in, pair, r).empty(), "an .osv and an .insv share nothing");
    gui::set_row_border(in, pair, 0, true, 0.01f);
    check(gui::border_sharers(in, pair, 1).empty(), "nor after one lens is set to cut away");

    // Two cameras taking turns: each clip joins the last one of its own camera.
    const auto turns =
        gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}, {"cam0", "cam1"}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> four = {fisheye(), fisheye(), fisheye(), fisheye()};
    gui::settle_follow(four, turns, {kInsv, kOsv, kInsv, kOsv});
    check(gui::border_sharers(four, turns, 4) == Rows{0, 4} &&
              gui::border_sharers(four, turns, 7) == Rows{3, 7},
          "each lens shares with the same lens of its own camera, past the other");

    // 8K clips among 5.7K ones of the same camera: another lens mode.
    std::vector<gui::PrepInput> sized = {fisheye(), fisheye(), fisheye(), fisheye()};
    gui::settle_follow(sized, turns, {"insv 2880", "insv 3840", "insv 3840", "insv 2880"});
    check(gui::border_sharers(sized, turns, 0) == Rows{0, 6} &&
              gui::border_sharers(sized, turns, 2) == Rows{2, 4},
          "frames of one size share, past those of another");
}

void test_shared_edit() {
    const auto rows = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye()};
    gui::settle_follow(in, rows, {kInsv, kInsv});
    // The panel on the first file: Edit border ellipse on cam0.
    auto before = gui::row_stencils(in, rows);
    in[0].stencil.cameras["cam0"] = in[0].stencil;
    in[0].stencil.cameras["cam1"] = in[0].stencil;
    app::BorderDetect found;
    found.found = true;
    found.shape.rx = found.shape.ry = 0.48f;
    app::edit_detected_border(in[0].stencil.cameras["cam0"], found);
    gui::follow_edit(in, rows, before, 0);
    check(!at(in, rows, 2).detect_border && at(in, rows, 2).mask.shapes.size() == 1 &&
              !at(in, rows, 2).mask.shapes[0].remove,
          "an edited border ellipse reaches the lens sharing its circle");
    check(at(in, rows, 3).detect_border && at(in, rows, 3).mask.shapes.empty(),
          "and not the other lens, whose areas follow");

    before = gui::row_stencils(in, rows);
    in[0].stencil.cameras["cam1"].shrink = 0.05f;
    gui::follow_edit(in, rows, before, 0);
    check(at(in, rows, 3).shrink == 0.05f, "a shrink set in the panel reaches the shared circle");

    before = gui::row_stencils(in, rows);
    in[0].stencil.cameras["cam0"].mask.shapes.push_back(box(0.4f));
    gui::follow_edit(in, rows, before, 0);
    check(at(in, rows, 2).mask.shapes.size() == 2,
          "areas drawn later on the edited lens still follow");
}

void test_lenses() {
    const auto rows = gui::stencil_rows({{"cam0", "cam1"}, {"cam0", "cam1"}});
    std::vector<gui::PrepInput> in = {fisheye(), fisheye()};
    app::MaskSet stick;
    stick.shapes = {box(0.4f)};
    gui::set_row_areas(in, rows, 0, stick);
    check(!in[0].stencil.per_camera() && in[0].stencil.mask.shapes.size() == 1 &&
              !in[1].stencil.per_camera() && in[1].stencil.mask.shapes.size() == 1,
          "drawing on the first lens goes on the second too, and down; lenses that agree stay one stencil");
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
    check(gui::row_follows(two, rows, 1) && at(two, rows, 1).mask.shapes.size() == 2 &&
              at(two, rows, 3).mask.shapes.size() == 2,
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
    check(gui::row_follows(in, rows, 4), "a row starts out following");
    gui::set_row_areas(in, rows, 4, app::MaskSet{});
    check(!gui::row_follows(in, rows, 4),
          "None picked under a row that draws nothing stays None, not same as above");
    app::MaskSet rig;
    rig.cameras["cam0"] = {box(0.1f, /*remove=*/false), box(0.2f)};
    gui::set_row_areas(in, rows, 0, rig);
    check(at(in, rows, 4).mask.shapes.empty(), "and ignores a set drawn above it later");
    check(gui::row_follows(in, rows, 2) && at(in, rows, 2).mask.shapes.size() == 2,
          "the lens below follows the set");
    gui::set_row_areas(in, rows, 2, rig);
    check(!gui::row_follows(in, rows, 2),
          "the set picked again under the same set is the row's own");
    gui::set_row_areas(in, rows, 0, app::MaskSet{});
    check(at(in, rows, 1).mask.shapes.empty() && at(in, rows, 2).mask.shapes.size() == 2,
          "and an own row ignores the row above");
    gui::follow_above(in, rows, 2);
    check(gui::row_follows(in, rows, 2) && at(in, rows, 2).mask.shapes.empty(),
          "same as above picked again follows again");

    std::vector<gui::PrepInput> fewer = {in[0], in[2]};
    const auto two = gui::stencil_rows({{"cam0", "cam1"}, {""}});
    fewer[1].stencil.mask.shapes = {box(0.6f)};
    fewer[1].stencil_follow[""].areas = true;
    gui::settle_follow(fewer, two);
    check(!gui::row_follows(fewer, two, 2) && fewer[1].stencil.mask.shapes.size() == 1,
          "a following row that no longer matches keeps its value as its own");
}

// A laser scan's photographs after the inputs: rows of their own.
void test_scans() {
    std::vector<gui::PrepInput> in = {fisheye(false), fisheye(false)};
    app::FrameStencil scan_a, scan_b;
    std::map<std::string, gui::StencilFollow> follow_a, follow_b;
    gui::StencilOwners owners = gui::stencil_owners(in);
    owners.push_back({&scan_a, &follow_a});
    owners.push_back({&scan_b, &follow_b});
    auto rows = gui::stencil_rows({{""}, {""}});
    gui::append_rows(rows, {{""}, {""}}, 2);
    check(rows.size() == 4 && rows[2].input == 2 && rows[2].above == -1 && rows[3].above == 2,
          "scan rows start a chain of their own");
    gui::settle_follow(owners, rows);
    app::MaskSet stick;
    stick.shapes = {box(0.4f)};
    gui::set_row_areas(owners, rows, 0, stick);
    check(in[1].stencil.mask.shapes.size() == 1 && scan_a.mask.shapes.empty(),
          "areas drawn on the inputs never reach a scan's photographs");
    app::MaskSet tripod;
    tripod.shapes = {box(0.1f), box(0.2f)};
    gui::set_row_areas(owners, rows, 2, tripod);
    check(scan_a.mask.shapes.size() == 2 && scan_b.mask.shapes.size() == 2 &&
              in[0].stencil.mask.shapes.size() == 1,
          "a scan's areas reach the scan below that follows it, and no input");
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
    test_own_borders();
    test_shares();
    test_setups();
    test_shared_edit();
    test_lenses();
    test_panel_edit();
    test_explicit();
    test_scans();
    test_adopt();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
