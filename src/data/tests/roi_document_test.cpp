// roi_document_test -- data/RoiDocument.h: how a list of shapes folds into one
// region, a shape's sides and pivot, the file the editor writes and reads back
// (and a plain region JSON it can import), and which file the trainer picks.

#include "data/RoiDocument.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

spirula::RoiShape box(double x, double half, spirula::RoiOp op) {
    spirula::RoiShape s;
    s.kind = spirula::RoiShapeKind::Box;
    s.op = op;
    s.origin[0] = x;
    for (int k = 0; k < 3; k++) {
        s.lo[k] = -half;
        s.hi[k] = half;
    }
    return s;
}

bool in(const std::shared_ptr<const spirula::Region>& r, double x) {
    return r && r->inside(x, 0, 0);
}

}  // namespace

int main() {
    using namespace spirula;

    // ---- the fold ----
    {
        RoiDocument d;
        check(roi_region(d) == nullptr, "no shapes: no region");
        d.shapes = {box(0, 2, RoiOp::Add), box(5, 1, RoiOp::Add), box(0, 0.5, RoiOp::Subtract)};
        auto r = roi_region(d);
        check(in(r, 1.5) && in(r, 5.5) && !in(r, 0.2) && !in(r, 3), "add, add, then cut a hole");

        d.shapes.push_back(box(1, 1.2, RoiOp::Intersect));
        r = roi_region(d);
        check(in(r, 1.5) && !in(r, 5.5) && !in(r, 0.2), "intersect keeps only the overlap");

        d.shapes[3].enabled = false;
        check(in(roi_region(d), 5.5), "a disabled shape is left out");

        RoiDocument cut;
        cut.shapes = {box(0, 1, RoiOp::Subtract)};
        r = roi_region(cut);
        check(!in(r, 0) && in(r, 50), "a list that opens with a cut keeps everything else");
        cut.shapes.push_back(box(0, 0.3, RoiOp::Add));
        check(in(roi_region(cut), 0) && !in(roi_region(cut), 0.6), "and a later add fills part of it back");

        RoiDocument all_off;
        all_off.shapes = {box(0, 1, RoiOp::Add)};
        all_off.shapes[0].enabled = false;
        check(roi_region(all_off) == nullptr, "every shape disabled: no region");
    }

    // ---- moving sides ----
    {
        RoiShape b = box(0, 1, RoiOp::Add);
        const double R[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};   // turned: its x is world y
        std::copy(R, R + 9, b.R);
        roi_set_extent(b, 0, -1, 4);
        auto r = b.region();
        check(r->inside(0, 3.9f, 0) && !r->inside(0, 4.1f, 0) && r->inside(0, -0.9f, 0) &&
                  !r->inside(0, -1.1f, 0),
              "a side moved along a turned box's own axis; the opposite one stays");
        RoiShape c = b;
        roi_center_pivot(c);
        bool same = std::fabs(c.origin[1] - 1.5) < 1e-12 && std::fabs(c.hi[0] - 2.5) < 1e-12;
        for (double y = -2; y <= 5; y += 0.25)
            same = same && c.region()->inside(0, (float)y, 0) == r->inside(0, (float)y, 0);
        check(same, "centring the pivot leaves the shape where it was");

        RoiShape p;
        p.kind = RoiShapeKind::Prism;
        p.polygon = {0, 0, 2, 0, 2, 1, 1, 1, 1, 2, 0, 2};
        roi_set_extent(p, 0, 0, 4);
        double lo[3], hi[3];
        roi_extents(p, lo, hi);
        check(lo[0] == 0 && hi[0] == 4 && hi[1] == 2 && p.polygon[6] == 2,
              "an outline's side stretches its corners, the far side fixed");
    }

    const fs::path dir = fs::temp_directory_path() /
                         ("roi_document_test_" +
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);

    // ---- the file ----
    {
        RoiDocument d;
        RoiShape p;
        p.name = "footprint";
        p.kind = RoiShapeKind::Prism;
        p.origin[2] = 1;
        p.lo[2] = -2;
        p.hi[2] = 3;   // a pivot off the middle survives the file
        p.polygon = {0, 0, 4, 0, 4, 1, 1, 1, 1, 4, 0, 4};
        RoiShape e;
        e.name = "dome";
        e.kind = RoiShapeKind::Ellipsoid;
        e.hi[0] = 2;
        e.origin[1] = 5312345.678901;   // geo-referenced: %.9g would round this
        const double Rx[9] = {1, 0, 0, 0, 0, 1, 0, -1, 0};
        std::copy(Rx, Rx + 9, e.R);
        RoiShape c = box(10, 1, RoiOp::Subtract);
        c.kind = RoiShapeKind::Cylinder;
        c.name = "post";
        c.enabled = false;
        d.shapes = {p, e, c};
        const std::string path = (dir / "roi" / "a.json").string();
        bool wrote = true;
        try {
            write_roi_file(d, path);
        } catch (const std::exception&) {
            wrote = false;
        }
        check(wrote && fs::exists(path), "writes, making roi/ on the way");

        RoiDocument back;
        std::string err;
        check(read_roi_file(path, back, err) && back == d, "reads back the same list: " + err);

        // The same file is the region the trainer reads.
        std::unique_ptr<Region> r = region_from_json(json_parse_file(path), "", err);
        const auto want = roi_region(d);
        bool same = r != nullptr;
        for (double x = -3; x <= 5 && same; x += 0.25)
            for (double y = -3; y <= 5 && same; y += 0.25)
                for (double z = -2; z <= 4; z += 0.5) {
                    const double q[3] = {x, y, z};
                    same = same && r->contains(q) == want->contains(q);
                }
        check(same, "the file is the region, for the trainer: " + err);

        RoiDocument empty;
        bool threw = false;
        try {
            write_roi_file(empty, (dir / "roi" / "empty.json").string());
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw && !fs::exists(dir / "roi" / "empty.json"), "nothing enabled: nothing written");

        // A region written by hand, with no editor list: a fold of shapes
        // imports, a label field does not.
        {
            std::ofstream f(dir / "roi" / "B.json");
            f << R"({"type":"difference","children":[)"
                 R"({"type":"union","children":[{"type":"sphere","center":[0,0,0],"radius":2},)"
                 R"({"type":"box","center":[3,0,0],"half":[1,1,1]}]},)"
                 R"({"type":"cylinder","center":[0,0,0],"half":[0.5,0.5,9]}]})";
        }
        RoiDocument imp;
        check(read_roi_file((dir / "roi" / "B.json").string(), imp, err) && imp.shapes.size() == 3 &&
                  imp.shapes[0].kind == RoiShapeKind::Ellipsoid && imp.shapes[1].op == RoiOp::Add &&
                  imp.shapes[2].op == RoiOp::Subtract && imp.shapes[2].kind == RoiShapeKind::Cylinder,
              "a plain region of shapes imports as a list: " + err);
        check(!in(roi_region(imp), 0) && in(roi_region(imp), 1.5) && in(roi_region(imp), 3.5),
              "and folds back to the same region");
        RoiDocument bad;
        check(!roi_from_region(json_parse(R"({"type":"halfspace","normal":[0,0,1]})"), bad, err) &&
                  !err.empty(),
              "a region the list cannot hold is refused with a message");
    }

    // ---- which file the trainer picks ----
    {
        const std::string ds = dir.string();
        const std::vector<std::string> files = list_roi_files(ds);
        check(files.size() == 2 && fs::path(files[0]).filename() == "a.json" &&
                  fs::path(files[1]).filename() == "B.json",
              "listed by name, ignoring case");
        RoiChoice c = resolve_roi_setting("", ds);
        check(c.automatic && c.path == files[0], "unset: the first file, automatically");
        c = resolve_roi_setting("off", ds);
        check(c.off && c.path.empty(), "off: no region");
        c = resolve_roi_setting("B", ds);
        check(!c.automatic && fs::path(c.path) == fs::path(files[1]), "a bare name: that file");
        c = resolve_roi_setting("roi/B.json", ds);
        check(fs::path(c.path) == fs::path(files[1]), "relative to the dataset");
        c = resolve_roi_setting("", (dir / "nothing").string());
        check(!c.automatic && !c.off && c.path.empty(), "a dataset without roi/: nothing");
    }

    std::error_code ec;
    fs::remove_all(dir, ec);
    if (g_failures == 0) std::printf("roi_document_test: OK\n");
    return g_failures ? 1 : 0;
}
