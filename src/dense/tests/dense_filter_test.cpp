#include "dense/ConfigFields.h"
#include "dense/Filters.h"
#include "dense/Fusion.h"
#include "dense/ExternalSort.h"
#include "dense/PairSelection.h"
#include "dense/Reconstruction.h"
#include "core/CameraModel.h"
#include "core/Sha256.h"
#include "data/SparseEdit.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

using namespace spirula::dense;
using sfm::Vec3;
namespace fs = std::filesystem;

namespace {

int failures = 0;
const char* current = "";
void check(bool ok, const std::string& what) {
    if (ok) return;
    ++failures;
    std::fprintf(stderr, "FAIL %s: %s\n", current, what.c_str());
}

View pinhole(int64_t source, Vec3 center, double f = 32, int size = 32) {
    View v;
    v.source_image = source; v.center = center;
    v.camera.model = (int)CameraModelType::PINHOLE;
    v.camera.width = v.camera.height = size;
    v.camera.fx = v.camera.fy = f;
    v.camera.cx = v.camera.cy = size / 2.0;
    return v;
}

Vec3 along(const View& v, double px, double py, double distance) {
    Vec3 ray;
    if (!bearing(v, {px, py}, ray)) throw std::runtime_error("no bearing");
    return v.center + sfm::mul(sfm::transpose(v.world_to_camera), ray).normalized() * distance;
}

fs::path scratch(const char* name) {
    const auto dir = fs::temp_directory_path() /
        (std::string("spirula-dense-filter-") + name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    return dir;
}

// Mutants: margin ignored; a view of the point's own track allowed to veto; an empty
// cell read as distance 0 (or as evidence); the test reading the point's own distance.
void free_space_vetoes_only_what_another_image_saw_through() {
    std::vector<View> views{pinhole(0, {0, 0, 0})};
    FreeSpaceGrid grid(views);
    grid.observe({0, 0, 4}, {0});
    check(grid.sees_through({0, 0, 2}, {1, 2}), "a point half way to an observed surface was not seen through");
    check(!grid.sees_through({0, 0, 2}, {0, 1}), "the point's own image vetoed it");
    check(!grid.sees_through({0, 0, 3.9}, {1, 2}), "a point 2.5 % in front vetoed (margin is 5 %)");
    check(grid.sees_through({0, 0, 3.7}, {1, 2}), "a point 7.5 % in front survived");
    check(!grid.sees_through({0, 0, 5}, {1, 2}), "a point behind the surface vetoed");
    check(!grid.sees_through({0.5, 0, 2}, {1, 2}), "a cell with no observation counted as evidence");
}

// Mutant: the veto read from the point's own cell only, not the nearest of the 3 x 3.
void free_space_uses_the_nearest_of_3x3_cells() {
    std::vector<View> views{pinhole(0, {0, 0, 0})};
    FreeSpaceGrid far_only(views), with_edge(views);
    const Vec3 point = along(views[0], 16.2, 16.5, 2);
    far_only.observe(along(views[0], 16.5, 16.5, 4), {0});
    with_edge.observe(along(views[0], 16.5, 16.5, 4), {0});
    with_edge.observe(along(views[0], 17.5, 16.5, 1), {0});
    check(far_only.sees_through(point, {1, 2}), "control: no occluder, yet the point was not seen through");
    check(!with_edge.sees_through(point, {1, 2}), "an occluder one cell over did not stop the veto at a depth edge");
}

// Mutant: the 3 x 3 neighbourhood not wrapped across a panorama's seam.
void free_space_wraps_a_panorama_seam() {
    View pano;
    pano.source_image = 0;
    pano.camera.model = (int)CameraModelType::EQUIRECTANGULAR;
    pano.camera.width = 64; pano.camera.height = 32;
    pano.camera.fx = pano.camera.fy = 64 / (2 * 3.14159265358979323846);
    pano.camera.cx = 32; pano.camera.cy = 16;
    pano.source_camera = true;
    std::vector<View> views{pano};
    FreeSpaceGrid grid(views);
    check(grid.cells_x(0) == 64, "panorama grid width " + std::to_string(grid.cells_x(0)));
    grid.observe(along(pano, 63.5, 16.5, 4), {0});
    check(grid.sees_through(along(pano, 0.5, 16.5, 2), {1, 2}), "the seam's neighbour cell was not consulted");
    check(!grid.sees_through(along(pano, 3.5, 16.5, 2), {1, 2}), "a cell three over saw the seam's surface");
}

// Mutant: every face of a split image gets 512 cells instead of its share of the turn.
void free_space_grid_spans_512_cells_per_turn() {
    std::vector<View> views;
    for (int f = 0; f < 6; ++f) views.push_back(pinhole(0, {0, 0, 0}, 128, 256));
    views.push_back(pinhole(1, {1, 0, 0}, 500, 1000));
    views.push_back(pinhole(2, {2, 0, 0}, 50, 100));
    FreeSpaceGrid grid(views);
    check(grid.cells_x(0) == 128 && grid.cells_x(5) == 128, "a 90-degree face has " + std::to_string(grid.cells_x(0)) + " cells, not 128");
    check(grid.cells_x(6) == 512, "a whole image has " + std::to_string(grid.cells_x(6)) + " cells, not 512");
    check(grid.cells_x(7) == 100, "a small image has more cells than pixels");
}

// Mutants: the mean, or the lower middle, instead of the upper median; a budget
// small enough to spill changing the answer.
void two_image_bar_is_the_median() {
    const auto dir = scratch("median");
    auto median = [&](std::vector<double> v, uint64_t budget = 1 << 20) {
        { std::ofstream out(dir / "v.bin", std::ios::binary); for (double x : v) write_disk_record(out, x); }
        return external_median(dir / "v.bin", v.size(), budget);
    };
    check(median({5, 1, 4, 2, 3}) == 3, "median of 1..5");
    check(median({1, 1, 1, 1, 100}) == 1, "an outlier moved the bar");
    check(median({2, 1}) == 2, "even count takes the upper middle");
    check(median({}) == -1, "no three-image points must give no bar");
    std::vector<double> many;
    for (int i = 0; i < 5001; ++i) many.push_back((i * 7919) % 5001);
    check(median(many, 64) == 2500, "a spilled sort changed the median");
    fs::remove_all(dir);
}

// A sort that cannot open its input must name the file. The old message, "dense intermediate
// write failed", sent a bug report hunting a full disk when the handle table was what ran out.
void sort_open_failure_names_the_file() {
    const auto dir = scratch("sort-io");
    std::string message;
    try { external_median(dir / "absent.bin", 3, 1 << 20); }
    catch (const std::exception& e) { message = e.what(); }
    check(!message.empty(), "a missing sort input was accepted");
    check(message.find("absent.bin") != std::string::npos, "the error does not name the file: " + message);
    fs::remove_all(dir);
}

// Mutants: the finer focal; the worst pair instead of the best; the half angle; the grid scale ignored.
void depth_precision_takes_the_best_pair_and_coarser_focal() {
    const View ref = pinhole(0, {0, 0, 0}, 32), close = pinhole(1, {0.1, 0, 0}, 32), wide = pinhole(2, {1, 0, 0}, 16);
    const double got = best_depth_per_cell({0, 0, 4}, ref, {&close, &wide}, 32, 32);
    check(std::fabs(got - std::sqrt(17.0) / 16) < 1e-12, "best pair " + std::to_string(got) + ", expected 0.2577");
    check(std::fabs(cell_focal(ref, 64, 64) - 64) < 1e-12, "the cell focal ignores the matcher grid");
    View source = ref; source.source_camera = true; source.grid_scale[0] = source.grid_scale[1] = 0.5;
    check(std::fabs(cell_focal(source, 64, 64) - 16) < 1e-12, "a source view's cell focal ignores grid_scale");
}

// 13.8 degrees at f = 320 gives 2.6 %, 3.17 degrees gives 11.3 %. Mutants: the full angle; no 2 % floor.
void automatic_depth_precision_reproduces_the_measured_bars() {
    const double d = 3.14159265358979323846 / 180;
    check(std::fabs(automatic_depth_precision(13.8 * d, 320) - 0.0260) < 5e-4, "basement bar " + std::to_string(automatic_depth_precision(13.8 * d, 320)));
    check(std::fabs(automatic_depth_precision(3.17 * d, 320) - 0.113) < 1e-3, "aerial bar " + std::to_string(automatic_depth_precision(3.17 * d, 320)));
    check(automatic_depth_precision(60 * d, 320) == 0.02, "the 2 % floor");
    check(automatic_depth_precision(0, 320) == 0.02, "no measured angle");
}

// Box [0,10]^3, margin 2, radius 0.9. Mutants: isolation ignored;
// the margin ignored or the 2-norm used; neighbours counted among far points only; at most 2
// turned into fewer than 2 or at most 3; the neighbour scan limited to the point's own cell.
void far_isolated_drops_only_isolated_far_points() {
    std::vector<Vec3> pts; std::vector<int> group; std::set<uint64_t> expected;
    auto add = [&](int g, Vec3 p, bool drop) { if (drop) expected.insert(pts.size()); pts.push_back(p); group.push_back(g); };
    for (int i = 0; i < 20; i++) add(0, {0.5 * i, 5, 5}, false);
    for (int i = 0; i < 3; i++) add(1, {2.0 + 3 * i, 8, 8}, false);
    for (int i = 0; i < 5; i++) add(2, {11.0, 3.0 + 1.5 * i, 2}, false);
    add(3, {11.5, 11.5, 5}, false);
    for (int k = 0; k < 50; k++) add(4, {16, 3.0 * (k % 10), 3.0 * (k / 10)}, true);
    for (int a = 0; a < 5; a++) for (int b = 0; b < 10; b++) add(5, {-6, 100 + 0.5 * a, 100 + 0.5 * b}, false);
    for (int i = 0; i < 3; i++) add(6, {16, 200 + 0.4 * i, 0}, true);
    for (int i = 0; i < 4; i++) add(7, {16, 300 + 0.25 * i, 0}, false);
    add(8, {12.5, 5, 5}, false);
    for (double dy : {-0.3, 0.3, 0.6}) add(9, {11.9, 5 + dy, 5}, false);
    for (double dx : {-0.1, 0.1}) for (double dy : {-0.1, 0.1}) add(10, {-9.0 + dx, 500.4 + dy, 100}, false);
    for (double dy : {-0.1, 0.1}) for (double dz : {-0.1, 0.1}) add(11, {-20, 540 + dy, 540 + dz}, false);
    FarFilter f; f.low = {0, 0, 0}; f.high = {10, 10, 10}; f.margin = 2; f.radius = 0.9;
    uint64_t beyond = 0;
    const auto removed = far_isolated(pts, f, &beyond);
    check(expected.size() == 53, "fixture drifted");
    check(beyond == 116, "far points " + std::to_string(beyond) + ", expected 116");
    check(std::set<uint64_t>(removed.begin(), removed.end()) == expected,
          "removed " + std::to_string(removed.size()) + " points, expected exactly the 53 isolated far ones");
    FarFilter off = f; off.margin = 0;
    check(far_isolated(pts, off).empty(), "a margin of 0 is off");
}

// Mutants: the box from the extremes; one margin for metric and scale-free models; a diagonal
// share other than 0.2; the radius not 8 x the spacing; a filter on under 100 sparse points.
void far_filter_is_scale_free() {
    std::vector<double> xyz;
    for (int i = 0; i <= 1000; i++) for (double v : {(double)i, (double)((i * 3) % 1001), (double)((i * 5) % 1001)}) xyz.push_back(v);
    const auto m = resolve_far_filter(xyz, true, 0.5), u = resolve_far_filter(xyz, false, 0.5);
    check(m.low.x == 5 && m.low.y == 5 && m.low.z == 5 && m.high.x == 995 && m.high.z == 995, "box is not p0.5-p99.5");
    check(m.margin == 2.0, "metric margin " + std::to_string(m.margin));
    check(std::fabs(u.margin - 0.2 * 990 * std::sqrt(3.0)) < 1e-9, "scale-free margin " + std::to_string(u.margin));
    check(m.radius == 4.0 && u.radius == 4.0 && m.max_neighbours == 2, "radius " + std::to_string(m.radius));
    auto outliers = xyz;
    for (double v : {5000.0, 5000.0, 5000.0, -5000.0, -5000.0, -5000.0}) outliers.push_back(v);
    check(resolve_far_filter(outliers, true, 0.5).high.x < 1100, "two outliers moved the box");
    std::vector<double> lattice;
    for (int x = 0; x < 10; ++x) for (int y = 0; y < 10; ++y) for (int z = 0; z < 10; ++z)
        for (double v : {0.3 * x, 0.3 * y, 0.3 * z}) lattice.push_back(v);
    check(std::fabs(median_neighbour_spacing(lattice) - 0.3) < 1e-6, "median spacing of a 0.3 lattice");
    check(std::fabs(resolve_far_filter(lattice, false).radius - 2.4) < 1e-5, "radius from the measured spacing");
    check(!m.active() && u.active(), "a radius of 4 past a margin of 2 left the filter on");
    check(resolve_far_filter(xyz, true, 0.24).active() && !resolve_far_filter(xyz, true, 0.25).active(),
          "metric filter not off once 8 x the spacing reaches the margin");
    xyz.resize(99 * 3);
    check(!resolve_far_filter(xyz, true, 0.1).active(), "a filter on 99 sparse points");
}

std::vector<PairImage> line_of_images() {
    std::vector<PairImage> images(6);
    for (int i = 0; i < 6; ++i) { images[i].source_image = i; images[i].name = "i" + std::to_string(i); }
    for (int i = 0; i < 5; ++i) images[i].center = {(double)i, 0, 0};
    images[5].center = {100, 0, 0};
    auto share = [&](int a, int b, int first, int count) {
        for (int k = 0; k < count; ++k) { images[a].visible_points.push_back(first + k); images[b].visible_points.push_back(first + k); }
    };
    share(0, 5, 0, 50); share(0, 1, 100, 20); share(0, 2, 200, 10); share(1, 2, 300, 20); share(2, 3, 400, 20);
    share(3, 4, 500, 20); share(1, 3, 600, 5); share(2, 4, 700, 5);
    for (auto& image : images) {
        std::sort(image.visible_points.begin(), image.visible_points.end());
        image.visible_points.erase(std::unique(image.visible_points.begin(), image.visible_points.end()), image.visible_points.end());
    }
    return images;
}

// Mutants: the refusal applied on a scale-free model; the limit not applied while ranking;
// a refused neighbour not replaced; the limit from the mean or the maximum baseline.
void max_baseline_refuses_far_neighbours() {
    const auto images = line_of_images();
    PairOptions options; options.neighbors = 2;
    std::set<ImagePair> plain;
    select_pairs(images, options, [&](ImagePair p) { plain.insert(p); });
    check(plain.count({0, 5}) == 1, "control: the far image is a top neighbour without the limit");
    check(resolve_max_baseline(images, options, 0, false) == 0, "refused on a model that is not metric");
    check(resolve_max_baseline(images, options, -1, true) == 0, "a negative setting is off");
    check(resolve_max_baseline(images, options, 7, false) == 7, "an explicit limit applies to any model");
    auto sequential = options; sequential.mode = PairMode::Sequential;
    check(resolve_max_baseline(images, sequential, 0, true) == 0, "applied outside automatic mode");
    uint64_t refused = 0;
    std::vector<double> baselines;
    for (auto p : plain) baselines.push_back((images[p.first].center - images[p.second].center).norm());
    const double limit = resolve_max_baseline(images, options, 0, true, &refused);
    check(limit == automatic_max_baseline(baselines) && limit > 0 && limit < 50, "limit " + std::to_string(limit));
    check(refused == 2, "refused " + std::to_string(refused) + "; (0, 5) and 5's second neighbour (4, 5)");
    auto limited = options; limited.max_baseline = limit;
    std::set<ImagePair> kept;
    select_pairs(images, limited, [&](ImagePair p) { kept.insert(p); });
    check(!kept.count({0, 5}), "the far neighbour was still paired");
    // Directed, each reference ranks alone: 0's top two are {5, 1} unlimited and {1, 2} limited.
    auto directed = options; directed.directed = true; directed.references = {0, 1, 2, 3, 4};
    uint64_t directed_refused = 0;
    directed.max_baseline = resolve_max_baseline(images, directed, 0, true, &directed_refused);
    std::set<uint32_t> of_zero;
    select_pairs(images, directed, [&](ImagePair p) { if (p.first == 0) of_zero.insert(p.second); });
    check(directed.max_baseline == 3 && directed_refused == 1, "directed limit " + std::to_string(directed.max_baseline) +
          ", refused " + std::to_string(directed_refused));
    check(of_zero == std::set<uint32_t>{1, 2}, "the refused neighbour was not replaced by the next-ranked one");
    check(automatic_max_baseline({1, 2, 3, 10}) == 9, "3 x the median");
    // Two images split in three faces each, all nine face pairs sharing points, and a third
    // image 10 away: nine face pairs at 1 outvote the image pairs. Mutant: each face pair counted.
    std::vector<PairImage> faces;
    for (int img = 0; img < 2; ++img) for (int f = 0; f < 3; ++f) {
        PairImage v; v.source_image = img; v.face = f; v.name = "f" + std::to_string(img * 3 + f);
        v.center = {(double)img, 0, 0}; v.visible_points = {1, 2, 3};
        faces.push_back(v);
    }
    PairImage distant; distant.source_image = 2; distant.name = "distant"; distant.center = {10, 0, 0}; distant.visible_points = {9};
    faces.push_back(distant); faces[0].visible_points = {1, 2, 3, 9};
    PairOptions all; all.neighbors = 8;
    std::vector<double> by_face, by_image;
    std::set<std::pair<int64_t,int64_t>> seen;
    select_pairs(faces, all, [&](ImagePair p) {
        const double b = (faces[p.first].center - faces[p.second].center).norm();
        by_face.push_back(b);
        if (seen.insert(std::minmax(faces[p.first].source_image, faces[p.second].source_image)).second) by_image.push_back(b);
    });
    check(automatic_max_baseline(by_face) != automatic_max_baseline(by_image), "fixture lost its power: face and image medians agree");
    check(resolve_max_baseline(faces, all, 0, true) == automatic_max_baseline(by_image), "split faces weighted the median: limit " +
          std::to_string(resolve_max_baseline(faces, all, 0, true)) + ", by image pair " + std::to_string(automatic_max_baseline(by_image)));
}

// Mutants: the angle at the origin or over every point; the setting ignored; a bar with no shared points;
// two-image points without a bar when the setting is off, or with the automatic one over an explicit setting.
void plan_depth_precision_uses_shared_points() {
    std::vector<PairImage> images(2);
    images[0].center = {0, 0, 0}; images[1].center = {1, 0, 0};
    images[0].visible_points = {0, 1, 2}; images[1].visible_points = {0, 1};
    const std::vector<double> xyz{0.5, 0, 2, 0.5, 0, 4, 0.5, 0, 100};
    const std::vector<View> views{pinhole(0, {0, 0, 0}), pinhole(1, {1, 0, 0})};
    const std::vector<std::pair<uint32_t,uint32_t>> pairs{{0, 1}};
    const auto plan = plan_depth_precision(images, pairs, xyz, views, 32, 32, 0, false);
    const double angle = 2 * std::atan(0.5 / 3) * 180 / 3.14159265358979323846;
    const double automatic = automatic_depth_precision(angle * 3.14159265358979323846 / 180, 32);
    check(std::fabs(plan.median_pair_angle_degrees - angle) < 1e-9, "angle " + std::to_string(plan.median_pair_angle_degrees));
    check(plan.median_cell_focal == 32, "focal");
    check(std::fabs(plan.depth_precision - automatic) < 1e-12, "bar");
    check(plan.two_image_depth_precision < 0, "a two-image bar without two-image points");
    check(plan_depth_precision(images, pairs, xyz, views, 32, 32, 0.5, false).depth_precision == 0.5, "explicit setting");
    check(plan_depth_precision(images, pairs, xyz, views, 32, 32, -1, false).depth_precision < 0, "negative is off");
    const auto off = plan_depth_precision(images, pairs, xyz, views, 32, 32, -1, true);
    check(off.depth_precision < 0 && std::fabs(off.two_image_depth_precision - automatic) < 1e-12,
          "two-image bar " + std::to_string(off.two_image_depth_precision) + " with the setting off");
    const auto given = plan_depth_precision(images, pairs, xyz, views, 32, 32, 0.5, true);
    check(given.depth_precision == 0.5 && given.two_image_depth_precision == 0.5, "two-image bar ignored the setting");
    images[1].visible_points = {7};
    const auto unshared = plan_depth_precision(images, pairs, xyz, views, 32, 32, 0, true);
    check(unshared.depth_precision < 0 && unshared.two_image_depth_precision < 0, "a bar without shared points");
}

// Mutants: a cluster keeping its first member's id; ties not broken by the lower id.
void fusion_keeps_the_best_supported_id() {
    auto fuse_two = [](uint32_t sa, uint64_t ia, uint32_t sb, uint64_t ib) {
        const auto dir = scratch("fusion");
        {
            std::ofstream out(dir / "in.bin", std::ios::binary);
            Surface a{}, b{};
            a.point[0] = 0.001; a.radius = b.radius = 1; a.support = sa; a.id = ia;
            b.point[0] = 0.002; b.support = sb; b.id = ib;
            write_disk_record(out, a); write_disk_record(out, b);
        }
        fuse_surfaces(dir / "in.bin", dir / "out.bin", dir, 0.1, 64 << 20, 1);
        std::ifstream in(dir / "out.bin", std::ios::binary);
        Surface s{}; uint64_t n = 0, id = ~0ull;
        while (read_disk_record(in, s)) { ++n; id = s.id; }
        in.close(); fs::remove_all(dir);
        return n == 1 ? id : ~0ull;
    };
    check(fuse_two(2, 7, 5, 9) == 9, "the better-supported member's id was not kept");
    check(fuse_two(5, 7, 2, 9) == 7, "the better-supported member's id was not kept (order swapped)");
    check(fuse_two(3, 9, 3, 7) == 7, "a tie did not keep the lower id");
}

// A pair whose matches lie on z = depth(x, y), with an optional offset across x (off the
// epipolar line for a baseline along x) and deterministic noise along x.
spirula::roma::Prediction plane_prediction(const View& a, const View& b, const std::function<double(int,int)>& depth,
                                           double offset = 0, double noise = 0) {
    spirula::roma::Prediction p;
    p.width = p.height = 32;
    p.warp.resize(32 * 32 * 2); p.overlap.resize(32 * 32, 0); p.precision.resize(32 * 32 * 3);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
        const size_t i = (size_t)y * 32 + x;
        p.precision[i * 3] = p.precision[i * 3 + 2] = 1;
        Vec3 ray;
        if (!bearing(a, {x + 0.5, y + 0.5}, ray)) continue;
        const Vec3 point = a.center + ray * ((depth(x, y) - a.center.z) / ray.z);
        sfm::Vec2 px;
        if (!project(b, point, px)) { p.warp[i * 2] = p.warp[i * 2 + 1] = 2; continue; }
        const double jitter = noise * std::sin(12.9898 * (x + 1) + 78.233 * (y + 1) + 3.1 * a.center.x + 7.7 * b.center.y);
        p.warp[i * 2] = (float)(2 * (px.x + jitter) / 32 - 1);
        p.warp[i * 2 + 1] = (float)(2 * (px.y + offset) / 32 - 1);
        p.overlap[i] = 1;
    }
    return p;
}

ViewPixels grey() {
    ViewPixels pixels; pixels.width = pixels.height = 32;
    pixels.rgb.assign(32 * 32 * 3, 0.5f);
    return pixels;
}

DenseConfig small_config() {
    DenseConfig config;
    config.preset = "custom";
    config.match.low_width = config.match.low_height = 32;
    config.match.high_width = config.match.high_height = 0;
    config.voxel_size = 0.02;
    config.image_cache_bytes = 256ull << 20;
    return config;
}

std::vector<View> wall_views() {
    std::vector<View> views;
    for (int i = 0; i < 4; ++i) views.push_back(pinhole(i, {i * 0.25, 0, 0}));
    return views;
}

void add_wall_pairs(Reconstruction& r, const std::vector<View>& v, const std::function<double(int,int)>& depth, double noise = 0) {
    const auto pixels = grey();
    for (uint32_t a = 0; a < 4; ++a) for (uint32_t b = a + 1; b < 4; ++b)
        r.add_pair(a, b, pixels, pixels, {plane_prediction(v[a], v[b], depth, 0, noise), plane_prediction(v[b], v[a], depth, 0, noise)});
}

uint64_t count_z(const std::string& ply, double lo, double hi) {
    const auto cloud = read_ply_points(ply);
    uint64_t n = 0;
    for (int64_t i = 0; i < cloud.num(); ++i) n += cloud.xyz[(size_t)i * 3 + 2] > lo && cloud.xyz[(size_t)i * 3 + 2] < hi;
    return n;
}

struct TwoImageRun { ReconstructionStatistics stats; uint64_t floaters = 0; };

TwoImageRun two_image_run(const DenseConfig& config, const char* name, const FilterPlan* plan = nullptr) {
    const auto dir = scratch(name);
    auto views = wall_views();
    views.push_back(pinhole(4, {0, 0.5, 0}));       // matched to view 0 on a false layer at z = 2
    views.push_back(pinhole(5, {-0.25, 0.25, 0}));  // matched to view 0 on the wall, exactly
    views.push_back(pinhole(6, {-0.5, 0, 0}));      // matched to view 0 on the wall, 0.2 px off the epipolar line
    Reconstruction r((dir / "work").string(), views, config);
    if (plan) r.set_filter_plan(*plan);
    const std::function<double(int,int)> wall = [](int, int) { return 4.0; }, layer = [](int, int) { return 2.0; };
    add_wall_pairs(r, views, wall, 0.01);
    const auto pixels = grey();
    r.add_pair(0, 4, pixels, pixels, {plane_prediction(views[0], views[4], layer), plane_prediction(views[4], views[0], layer)});
    r.add_pair(0, 5, pixels, pixels, {plane_prediction(views[0], views[5], wall), plane_prediction(views[5], views[0], wall)});
    r.add_pair(0, 6, pixels, pixels, {plane_prediction(views[0], views[6], wall, 0.2), plane_prediction(views[6], views[0], wall, 0.2)});
    ParsedDataset dataset;
    TwoImageRun out;
    out.stats = r.finish((dir / "cloud.ply").string(), dataset);
    out.floaters = count_z((dir / "cloud.ply").string(), 1.8, 2.2);
    fs::remove_all(dir);
    return out;
}

// Mutants: no 3-image point recorded in the free-space grid; the error bar not applied;
// two-image points routed past the rule; the free-space switch ignored; "off" ignored.
void reconstruction_two_image_admission() {
    auto config = small_config();
    config.two_image_points = "auto";
    const auto on = two_image_run(config, "on");
    config.free_space_test = false;
    const auto no_free_space = two_image_run(config, "nofs");
    config.free_space_test = true; config.two_image_points = "off";
    const auto off = two_image_run(config, "off");
    const auto& s = on.stats;
    check(s.two_image_bar > 0 && s.two_image_bar < 0.1, "bar " + std::to_string(s.two_image_bar));
    check(s.seen_through > 100, "only " + std::to_string(s.seen_through) + " false-layer points were seen through");
    check(on.floaters == 0, std::to_string(on.floaters) + " false-layer points reached the cloud");
    check(s.two_image_over_bar > 100, "only " + std::to_string(s.two_image_over_bar) + " points over the bar");
    check(s.two_image_admitted > 100, "only " + std::to_string(s.two_image_admitted) + " consistent two-image points admitted");
    check(s.two_image_candidates == s.two_image_admitted + s.two_image_over_bar + s.seen_through, "candidates do not add up");
    check(no_free_space.stats.seen_through == 0 && no_free_space.floaters > 100 &&
          no_free_space.stats.two_image_admitted == s.two_image_admitted + s.seen_through,
          "with the free-space test off the false layer was not admitted");
    check(off.stats.two_image_candidates == 0 && off.stats.two_image_admitted == 0 && off.floaters == 0, "\"off\" still admitted two-image points");
    check(s.free_space_bytes == 7 * 32 * 32 * sizeof(float), "grid bytes " + std::to_string(s.free_space_bytes));
}

// A 0.2 two-image bar drops the wall pairs with views 5 and 6 (0.36 and 0.25 per cell) and keeps the
// layer pair with view 4 (0.13). Mutants: the two-image bar ignored, applied to every point, or inverted.
void reconstruction_two_image_depth_bar() {
    auto config = small_config();
    config.two_image_points = "auto";
    const auto none = two_image_run(config, "bar-none");
    FilterPlan plan; plan.two_image_depth_precision = 0.2;
    const auto barred = two_image_run(config, "bar-two", &plan);
    const auto& s = barred.stats;
    check(s.imprecise > 100, "only " + std::to_string(s.imprecise) + " imprecise two-image points dropped");
    check(s.two_image_candidates + s.imprecise == none.stats.two_image_candidates, "the bar dropped points with three images");
    check(s.refined - s.two_image_admitted == none.stats.refined - none.stats.two_image_admitted, "three-image points changed");
    check(s.seen_through == none.stats.seen_through && barred.floaters == 0, "the bar dropped the well-conditioned layer pair");
}

ReconstructionStatistics wall_run(const DenseConfig& config, const FilterPlan* plan, const char* name, ParsedDataset dataset = {},
                                  const std::function<double(int,int)>& depth = [](int, int) { return 4.0; },
                                  std::string* keep = nullptr) {
    const auto dir = scratch(name);
    const auto views = wall_views();
    Reconstruction r((dir / "work").string(), views, config);
    if (plan) r.set_filter_plan(*plan);
    add_wall_pairs(r, views, depth);
    auto stats = r.finish((dir / "cloud.ply").string(), dataset);
    if (keep) { const auto kept = fs::temp_directory_path() / (std::string("spirula-dense-filter-") + name + ".ply");
                fs::copy_file(dir / "cloud.ply", kept, fs::copy_options::overwrite_existing); *keep = kept.string(); }
    fs::remove_all(dir);
    return stats;
}

// Mutant: the plan's bar never checked. Best pairs are 0.17 (baseline 0.75) or 0.25 (0.5) per cell;
// 536 is the deterministic count of points whose best pair is the shorter one.
void reconstruction_depth_precision() {
    const auto config = small_config();
    const auto none = wall_run(config, nullptr, "noplan");
    FilterPlan loose; loose.depth_precision = 1.0;
    FilterPlan tight; tight.depth_precision = 0.2;
    const auto a = wall_run(config, &loose, "loose"), b = wall_run(config, &tight, "tight");
    check(none.imprecise == 0 && a.imprecise == 0, "points dropped with no bar or a loose one");
    check(b.imprecise == 536 && b.exported > 0, "a 0.2 bar dropped " + std::to_string(b.imprecise) + " points");
    check(b.depth_precision == 0.2, "the plan was not recorded");
}

ParsedDataset metric_wall_dataset() {
    ParsedDataset ds;
    ds.gauge_metric = true;
    for (double x = -1.8; x <= 2.5; x += 0.02) for (double y = -1.8; y <= 1.8; y += 0.02)
        for (double v : {x, y, 4.0}) ds.points.xyz.push_back(v);
    return ds;
}

// Mutants: the filter run after the point limit; the setting ignored; the filter reading the
// box without the margin. Isolated 3-image points at z = 12 sit 8 m past a metric box at z = 4.
void reconstruction_far_isolated_runs_before_the_limit() {
    auto config = small_config();
    config.cycle_check = false;
    const auto depth = [](int x, int y) { return x % 8 == 4 && y % 8 == 4 ? 12.0 : 4.0; };
    std::string with_far, without_far;
    config.far_isolated = false;
    const auto off = wall_run(config, nullptr, "far-off", metric_wall_dataset(), depth, &with_far);
    config.far_isolated = true;
    const auto on = wall_run(config, nullptr, "far-on", metric_wall_dataset(), depth, &without_far);
    const uint64_t far_points = count_z(with_far, 8, 16);
    check(far_points >= 32, "control: only " + std::to_string(far_points) + " far points without the filter");
    check(off.far_isolated == 0, "removed with the filter off");
    check(on.far_filter.active() && on.far_filter.margin == 2.0, "the filter did not resolve as metric");
    check(on.far_isolated == far_points && on.far_beyond == far_points, "removed " + std::to_string(on.far_isolated) +
          " of " + std::to_string(far_points) + " far points");
    check(count_z(without_far, 8, 16) == 0, "far points reached the cloud");
    config.point_limit = on.exported;
    std::string limited;
    wall_run(config, nullptr, "far-limit", metric_wall_dataset(), depth, &limited);
    check(count_z(limited, 8, 16) == 0, "far points survived under a point limit");
    fs::remove(with_far); fs::remove(without_far); fs::remove(limited);
}

// 200 points written through an output transform, each observed by two views 0.3 px off in x,
// ids a permutation of export order. Mutants: the transform or the centre not undone;
// observations looked up by export order instead of the id; the histogram's bin edges misread.
void reproject_written_measures_a_known_offset() {
    const auto dir = scratch("reproject-unit");
    const std::vector<View> views{pinhole(0, {0, 0, 0}), pinhole(1, {0.5, 0, 0})};
    const std::array<double,16> transform{0,-1,0,10, 1,0,0,20, 0,0,1,30, 0,0,0,1};
    const std::array<double,3> center{100, 5, -2};
    const int n = 200;
    std::vector<ObservationRange> ranges(n);
    std::vector<TrackObservation> observations;
    {
        spirula::PointCloudWriter writer((dir / "cloud.ply").string(), spirula::PlyCoordinates::Float64);
        std::ofstream ids(dir / "ids.bin", std::ios::binary);
        std::vector<std::vector<TrackObservation>> by_id(n);
        for (int k = 0; k < n; ++k) {
            const Vec3 p{-0.5 + 0.005 * k, 0.3 * std::sin(k), 4 + 0.01 * k};
            const double raw[3] = {p.x + center[0], p.y + center[1], p.z + center[2]};
            double xyz[3];
            for (int c = 0; c < 3; ++c) xyz[c] = transform[c * 4] * raw[0] + transform[c * 4 + 1] * raw[1] + transform[c * 4 + 2] * raw[2] + transform[c * 4 + 3];
            const uint8_t rgb[3] = {1, 2, 3};
            writer.append(xyz, rgb);
            const uint64_t id = (uint64_t)(k * 7) % n;
            write_disk_record(ids, id);
            for (uint32_t v = 0; v < 2; ++v) {
                sfm::Vec2 px;
                if (!project(views[v], p, px)) throw std::runtime_error("fixture point outside a view");
                by_id[id].push_back({v, {(float)(px.x + 0.3), (float)px.y}});
            }
        }
        writer.finish();
        for (int id = 0; id < n; ++id) {
            ranges[id] = {observations.size(), by_id[id].size()};
            for (const auto& o : by_id[id]) observations.push_back(o);
        }
        std::ofstream r(dir / "ranges.bin", std::ios::binary), o(dir / "observations.bin", std::ios::binary);
        for (const auto& x : ranges) write_disk_record(r, x);
        for (const auto& x : observations) write_disk_record(o, x);
    }
    auto run = [&](const std::array<double,16>& m, const std::array<double,3>& c) {
        return reproject_written(dir / "cloud.ply", dir / "ids.bin", dir / "ranges.bin", dir / "observations.bin",
                                 views, m, c, 32, 32, 1 << 20);
    };
    const auto good = run(transform, center);
    check(good.observations == 400 && good.invalid == 0, "observations " + std::to_string(good.observations));
    check(std::fabs(good.p50_pixels - 0.3) < 0.011 && std::fabs(good.p95_pixels - 0.3) < 0.011,
          "p50 " + std::to_string(good.p50_pixels) + ", p95 " + std::to_string(good.p95_pixels) + ", injected 0.3");
    check(std::fabs(good.mean_pixels - 0.3) < 1e-4, "mean " + std::to_string(good.mean_pixels));
    const std::array<double,16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const auto wrong = run(identity, center);
    check(wrong.invalid > 0 || wrong.p95_pixels > 5, "a wrong output transform read p95 " + std::to_string(wrong.p95_pixels));
    const auto no_center = run(transform, {0, 0, 0});
    check(no_center.invalid > 0 || no_center.p95_pixels > 5, "an ignored centre read p95 " + std::to_string(no_center.p95_pixels));
    fs::remove_all(dir);
}

// Residuals 0.1, 0.2 ... 10 px. Mutants: p95 read as p50; bin edges for centres; the open bin dropped.
void reprojection_histogram_quantiles() {
    ReprojectionHistogram h;
    for (int k = 1; k <= 100; ++k) h.add(0.1 * k, 0.2 * k);
    h.add_invalid();
    auto s = h.summary();
    check(s.observations == 101 && s.invalid == 1, "counts");
    check(std::fabs(s.p50_pixels - 5.005) < 1e-9 && std::fabs(s.p95_pixels - 9.505) < 1e-9, "p50 " + std::to_string(s.p50_pixels) + " p95 " + std::to_string(s.p95_pixels));
    check(std::fabs(s.p95_cells - 19.005) < 1e-9 && std::fabs(s.mean_pixels - 5.05) < 1e-9, "cells or mean");
    for (int k = 0; k < 200; ++k) h.add(500, 500);
    check(h.summary().p95_pixels == 100, "the open bin read " + std::to_string(h.summary().p95_pixels));
}

// Mutant: the check's switch ignored. End to end, fusion's drift stays under 0.5 px here.
void reconstruction_written_reprojection() {
    auto config = small_config();
    config.reprojection_check = true;
    ParsedDataset dataset;
    dataset.center = {4000000.123456789, 5, -2};
    dataset.raw_to_file = {0,-1,0,10, 1,0,0,20, 0,0,1,30, 0,0,0,1};
    const auto s = wall_run(config, nullptr, "reproject", dataset);
    check(s.reprojection.observations >= 2 * s.exported, "observations " + std::to_string(s.reprojection.observations));
    check(s.reprojection.invalid == 0, "invalid " + std::to_string(s.reprojection.invalid));
    check(s.reprojection.p95_pixels < 0.5, "p95 " + std::to_string(s.reprojection.p95_pixels) + " px");
    config.reprojection_check = false;
    check(wall_run(config, nullptr, "reproject-off", dataset).reprojection.observations == 0, "the check ran while off");
}

// The plane scenario with every filter off. GOLDEN is that scenario's cloud from 95b27a4b,
// before these filters existed. Mutant: any filter, id or side file changing the off path.
void all_filters_off_matches_the_base_pipeline() {
    auto config = small_config();
    config.two_image_points = "off"; config.far_isolated = false; config.reprojection_check = false;
    config.max_depth_error_per_cell = -1; config.max_baseline = -1;
    std::string ply;
    wall_run(config, nullptr, "all-off", metric_wall_dataset(), [](int x, int y) { return x % 8 == 4 && y % 8 == 4 ? 12.0 : 4.0; }, &ply);
    const auto hash = spirula::sha256_file(ply);
    check(hash == "e86ee06443c04f5e44ca5d23c2701a49e60279d6b4dd437110a5c693d36bd34f", "all filters off gave " + hash);
    fs::remove(ply);
}

// Mutants: a filter setting missing from the field table (lost in JSON, presets and the CLI);
// validation accepting an unknown two-image mode or a non-finite bar; defaults other than the chosen ones.
void filter_settings_round_trip_and_validate() {
    const DenseConfig defaults;
    check(defaults.two_image_points == "off" && defaults.free_space_test && defaults.far_isolated &&
          !defaults.reprojection_check && defaults.max_depth_error_per_cell == -1 && defaults.max_baseline == -1, "defaults");
    DenseConfig c;
    c.two_image_points = "auto"; c.free_space_test = false; c.far_isolated = false; c.reprojection_check = true;
    c.max_depth_error_per_cell = 0.05; c.max_baseline = 0;
    DenseConfig d;
    read_config(d, json_parse(config_json(c)));
    check(d.two_image_points == "auto" && !d.free_space_test && !d.far_isolated && d.reprojection_check &&
          d.max_depth_error_per_cell == 0.05 && d.max_baseline == 0, "a filter setting did not survive JSON");
    auto bad = defaults; bad.two_image_points = "maybe";
    bool refused = false;
    try { bad.validate(); } catch (const std::exception&) { refused = true; }
    check(refused, "an unknown two-image mode was accepted");
    bad = defaults; bad.max_depth_error_per_cell = std::nan("");
    refused = false;
    try { bad.validate(); } catch (const std::exception&) { refused = true; }
    check(refused, "a NaN depth-precision bar was accepted");
}

}  // namespace

int main() {
    struct Test { const char* name; void (*run)(); };
    const Test tests[] = {
        {"free_space_vetoes_only_what_another_image_saw_through", free_space_vetoes_only_what_another_image_saw_through},
        {"free_space_uses_the_nearest_of_3x3_cells", free_space_uses_the_nearest_of_3x3_cells},
        {"free_space_wraps_a_panorama_seam", free_space_wraps_a_panorama_seam},
        {"free_space_grid_spans_512_cells_per_turn", free_space_grid_spans_512_cells_per_turn},
        {"two_image_bar_is_the_median", two_image_bar_is_the_median},
        {"sort_open_failure_names_the_file", sort_open_failure_names_the_file},
        {"depth_precision_takes_the_best_pair_and_coarser_focal", depth_precision_takes_the_best_pair_and_coarser_focal},
        {"automatic_depth_precision_reproduces_the_measured_bars", automatic_depth_precision_reproduces_the_measured_bars},
        {"far_isolated_drops_only_isolated_far_points", far_isolated_drops_only_isolated_far_points},
        {"far_filter_is_scale_free", far_filter_is_scale_free},
        {"max_baseline_refuses_far_neighbours", max_baseline_refuses_far_neighbours},
        {"plan_depth_precision_uses_shared_points", plan_depth_precision_uses_shared_points},
        {"fusion_keeps_the_best_supported_id", fusion_keeps_the_best_supported_id},
        {"reconstruction_two_image_admission", reconstruction_two_image_admission},
        {"reconstruction_two_image_depth_bar", reconstruction_two_image_depth_bar},
        {"reconstruction_depth_precision", reconstruction_depth_precision},
        {"reconstruction_far_isolated_runs_before_the_limit", reconstruction_far_isolated_runs_before_the_limit},
        {"reconstruction_written_reprojection", reconstruction_written_reprojection},
        {"filter_settings_round_trip_and_validate", filter_settings_round_trip_and_validate},
        {"reproject_written_measures_a_known_offset", reproject_written_measures_a_known_offset},
        {"reprojection_histogram_quantiles", reprojection_histogram_quantiles},
        {"all_filters_off_matches_the_base_pipeline", all_filters_off_matches_the_base_pipeline},
    };
    for (const auto& test : tests) {
        current = test.name;
        const int before = failures;
        try { test.run(); } catch (const std::exception& e) { check(false, std::string("threw: ") + e.what()); }
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", test.name);
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
