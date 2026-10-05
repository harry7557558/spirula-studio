// region_test -- data/Region.h and data/LabelField.h: the primitives against
// points whose side is known (the ROI editor's ellipsoid, cylinder and
// prism among them), the CSG operations, a mesh cube by ray parity,
// the JSON round trip, and a label field built from two labelled blobs.

#include "data/LabelField.h"
#include "data/Region.h"
#include "data/RegionMesh.h"
#include "data/RegionProgram.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

std::unique_ptr<spirula::MeshRegion> unit_cube(double half, const double c[3]) {
    std::vector<double> v;
    for (int i = 0; i < 8; i++) {
        v.push_back(c[0] + ((i & 1) ? half : -half));
        v.push_back(c[1] + ((i & 2) ? half : -half));
        v.push_back(c[2] + ((i & 4) ? half : -half));
    }
    // Twelve triangles, outward or inward does not matter for parity.
    const uint32_t f[36] = {0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5, 0, 4, 5, 0, 5, 1,
                            2, 3, 7, 2, 7, 6, 0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3};
    return std::make_unique<spirula::MeshRegion>(v, std::vector<uint32_t>(f, f + 36));
}

}  // namespace

int main() {
    using namespace spirula;

    // ---- primitives ----
    {
        BoxRegion b;
        b.center[0] = 1; b.half[0] = 2; b.half[1] = 1; b.half[2] = 0.5;
        check(b.inside(2.9f, 0.9f, 0.4f), "box: inside near a corner");
        check(!b.inside(3.1f, 0.0f, 0.0f), "box: outside along x");
        check(!b.inside(1.0f, 0.0f, 0.6f), "box: outside along z");
        // Rotated 90 degrees about z: x and y extents swap.
        BoxRegion r = b;
        r.center[0] = 0;
        const double R[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};
        for (int i = 0; i < 9; i++) r.R[i] = R[i];
        check(r.inside(0.9f, 1.9f, 0.0f), "rotated box: inside along the swapped axis");
        check(!r.inside(1.9f, 0.0f, 0.0f), "rotated box: outside along the old axis");
        const Aabb rb = r.bounds();
        check(std::fabs(rb.hi[0] - 1.0) < 1e-9 && std::fabs(rb.hi[1] - 2.0) < 1e-9,
              "rotated box: bounds follow the rotation");

        SphereRegion s;
        s.center[2] = 2; s.radius = 1.5;
        check(s.inside(0.0f, 0.0f, 3.4f) && !s.inside(0.0f, 0.0f, 3.6f), "sphere");

        HalfSpaceRegion h;
        h.normal[0] = 1; h.normal[2] = 0; h.offset = -1;
        check(h.inside(1.5f, 0, 0) && !h.inside(0.5f, 0, 0), "halfspace x >= 1");
    }

    // ---- the ROI editor's shapes ----
    {
        const double Rz[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};   // axes: y, -x, z
        EllipsoidRegion e;
        e.center[2] = 1; e.half[0] = 3; e.half[1] = 1; e.half[2] = 0.5;
        for (int i = 0; i < 9; i++) e.R[i] = Rz[i];
        check(e.inside(0.0f, 2.9f, 1.0f) && !e.inside(2.0f, 0.0f, 1.0f) && !e.inside(0, 0, 1.6f),
              "ellipsoid: long axis follows the rotation");
        const Aabb eb = e.bounds();
        check(std::fabs(eb.hi[0] - 1.0) < 1e-9 && std::fabs(eb.hi[1] - 3.0) < 1e-9 &&
                  std::fabs(eb.lo[2] - 0.5) < 1e-9,
              "ellipsoid: bounds are tight");

        CylinderRegion c;
        c.half[0] = 2; c.half[1] = 1; c.half[2] = 4;
        check(c.inside(1.9f, 0, 3.9f) && !c.inside(1.5f, 0.8f, 0) && !c.inside(0, 0, 4.1f),
              "cylinder: elliptic section, capped");

        PrismRegion l;   // an L: the unit square at the origin's corner is cut out
        l.center[2] = 2; l.half_height = 1;
        l.polygon = {0, 0, 2, 0, 2, 1, 1, 1, 1, 2, 0, 2};
        check(l.inside(0.5f, 0.5f, 2.5f) && l.inside(1.5f, 0.5f, 1.5f) && l.inside(0.5f, 1.5f, 2.0f),
              "prism: inside every arm of the L");
        check(!l.inside(1.5f, 1.5f, 2.0f) && !l.inside(0.5f, 0.5f, 3.1f) && !l.inside(-0.1f, 0.5f, 2.0f),
              "prism: outside the notch, above the top, beside the outline");
        const Aabb lb = l.bounds();
        check(std::fabs(lb.hi[0] - 2) < 1e-9 && std::fabs(lb.lo[2] - 1) < 1e-9 && std::fabs(lb.hi[2] - 3) < 1e-9,
              "prism: bounds");

        std::string err;
        for (const Region* r : std::initializer_list<const Region*>{&e, &c, &l}) {
            std::unique_ptr<Region> back = region_from_json(json_parse(region_to_json(*r)), "", err);
            RegionProgram prog, moved;
            check(back && std::string(back->kind()) == r->kind() && compile_region(*r, prog, err),
                  std::string(r->kind()) + " round-trips through JSON and compiles: " + err);
            moved = prog;
            const double sc = 0.4, sh[3] = {-2, 7, 1};
            moved.apply_similarity(sc, sh);
            std::mt19937 rng(9);
            std::uniform_real_distribution<double> uu(-4.0, 4.0);
            int differ = 0;
            for (int i = 0; i < 4000; i++) {
                const double p[3] = {uu(rng), uu(rng), uu(rng)};
                const double q[3] = {sc * p[0] + sh[0], sc * p[1] + sh[1], sc * p[2] + sh[2]};
                const bool want = r->contains(p);
                differ += (back && back->contains(p) != want) + (program_contains(prog, p) != want) +
                          (program_contains(moved, q) != want);
            }
            check(differ <= 3, std::string(r->kind()) + ": parsed, compiled and moved agree (" +
                                   std::to_string(differ) + " differ)");
        }
        // A prism's polygon fills whole nodes after it, which the evaluator
        // has to step over to reach the node that follows.
        PrismRegion many;
        many.half_height = 1;
        for (int k = 0; k < 40; k++)
            for (double v : {3.0 * std::cos(k * 0.157), 3.0 * std::sin(k * 0.157)}) many.polygon.push_back(v);
        auto pm = std::make_shared<PrismRegion>(many);
        auto ball = std::make_shared<SphereRegion>();
        ball->radius = 1;
        CsgRegion cut;
        cut.op = CsgOp::Difference;
        cut.children = {pm, ball};
        RegionProgram prog;
        check(compile_region(cut, prog, err) &&
                  prog.num_nodes() == 3 + RegionProgram::payload_nodes(40) &&
                  RegionProgram::payload_nodes(40) == 4,
              "a 40-gon prism takes four payload nodes");
        check(program_contains(prog, std::array<double, 3>{2.0, 0, 0}.data()) &&
                  !program_contains(prog, std::array<double, 3>{0.5, 0, 0}.data()),
              "the node after the payload is evaluated");

        // Geo-referenced: float spacing at 5e6 is 0.5, so a box whose face is
        // 0.8 from the shifted origin only keeps it when the shift happens
        // in double, before the nodes are narrowed.
        BoxRegion geo;
        geo.center[0] = 5e6 + 0.3; geo.center[1] = -3e6; geo.center[2] = 100;
        for (double& h : geo.half) h = 0.5;
        const double shift[3] = {-5e6, 3e6, -100};
        RegionProgram local;
        check(compile_region(geo, local, err, 1.0, shift) &&
                  program_contains(local, std::array<double, 3>{0.78, 0, 0}.data()) &&
                  !program_contains(local, std::array<double, 3>{0.82, 0, 0}.data()),
              "a geo-referenced box compiled into a local frame keeps its faces");
    }

    // ---- mesh by parity ----
    {
        const double c[3] = {0.5, -1.0, 2.0};
        auto cube = unit_cube(1.0, c);
        std::mt19937 rng(3);
        std::uniform_real_distribution<double> u(-1.6, 1.6);
        int wrong = 0, n = 0;
        for (int i = 0; i < 4000; i++) {
            const double p[3] = {c[0] + u(rng), c[1] + u(rng), c[2] + u(rng)};
            const bool truth = std::fabs(p[0] - c[0]) < 1 && std::fabs(p[1] - c[1]) < 1 &&
                               std::fabs(p[2] - c[2]) < 1;
            // Skip the faces themselves, where either answer is fine.
            if (std::fabs(std::fabs(p[0] - c[0]) - 1) < 1e-3 ||
                std::fabs(std::fabs(p[1] - c[1]) - 1) < 1e-3 ||
                std::fabs(std::fabs(p[2] - c[2]) - 1) < 1e-3)
                continue;
            n++;
            wrong += cube->contains(p) != truth;
        }
        check(wrong == 0, "mesh cube: parity agrees with the box on " + std::to_string(n) +
                              " random points (" + std::to_string(wrong) + " wrong)");
        // Many small triangles: the BVH has interior nodes.
        std::vector<double> v;
        std::vector<uint32_t> f;
        const int m = 24;
        for (int i = 0; i <= m; i++)
            for (int j = 0; j <= m; j++) {
                const double th = 3.14159265358979 * i / m, ph = 2 * 3.14159265358979 * j / m;
                v.push_back(2.0 * std::sin(th) * std::cos(ph));
                v.push_back(2.0 * std::sin(th) * std::sin(ph));
                v.push_back(2.0 * std::cos(th));
            }
        for (int i = 0; i < m; i++)
            for (int j = 0; j < m; j++) {
                const uint32_t a = (uint32_t)(i * (m + 1) + j), b = a + 1, cc = a + m + 1, d = cc + 1;
                f.insert(f.end(), {a, b, d, a, d, cc});
            }
        MeshRegion sphere(v, f);
        wrong = 0;
        for (int i = 0; i < 3000; i++) {
            const double p[3] = {u(rng) * 1.6, u(rng) * 1.6, u(rng) * 1.6};
            const double r = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
            if (std::fabs(r - 2.0) < 0.05) continue;
            wrong += sphere.contains(p) != (r < 2.0);
        }
        check(wrong == 0, "mesh sphere with 1152 triangles: " + std::to_string(wrong) + " wrong");
    }

    // ---- CSG ----
    {
        auto a = std::make_shared<SphereRegion>();
        a->radius = 1.0;
        auto b = std::make_shared<SphereRegion>();
        b->center[0] = 1.0;
        b->radius = 1.0;
        CsgRegion u;
        u.op = CsgOp::Union;
        u.children = {a, b};
        CsgRegion i;
        i.op = CsgOp::Intersection;
        i.children = {a, b};
        CsgRegion d;
        d.op = CsgOp::Difference;
        d.children = {a, b};
        CsgRegion c;
        c.op = CsgOp::Complement;
        c.children = {a};
        check(u.inside(1.8f, 0, 0) && u.inside(-0.8f, 0, 0) && !u.inside(0, 1.5f, 0), "union");
        check(i.inside(0.5f, 0, 0) && !i.inside(-0.5f, 0, 0) && !i.inside(1.5f, 0, 0),
              "intersection");
        check(d.inside(-0.5f, 0, 0) && !d.inside(0.5f, 0, 0), "difference");
        check(c.inside(5, 0, 0) && !c.inside(0, 0, 0), "complement");
        const Aabb ib = i.bounds();
        check(std::fabs(ib.lo[0] - 0.0) < 1e-9 && std::fabs(ib.hi[0] - 1.0) < 1e-9,
              "intersection bounds are the overlap");

        // JSON round trip of the whole tree.
        const std::string text = region_to_json(d);
        std::string err;
        std::unique_ptr<Region> back = region_from_json(json_parse(text), "", err);
        check(back != nullptr, "difference parses back: " + err);
        if (back) {
            check(std::string(back->kind()) == "difference", "kind survives");
            check(back->inside(-0.5f, 0, 0) && !back->inside(0.5f, 0, 0),
                  "parsed difference answers the same");
        }
        const double cc[3] = {0, 0, 0};
        auto cube = unit_cube(0.5, cc);
        std::unique_ptr<Region> cube_back = region_from_json(json_parse(region_to_json(*cube)), "", err);
        check(cube_back && cube_back->inside(0.2f, 0.2f, 0.2f) && !cube_back->inside(0.7f, 0, 0),
              "mesh region round-trips through JSON");
        std::unique_ptr<Region> bad = region_from_json(json_parse("{\"type\":\"donut\"}"), "", err);
        check(!bad && !err.empty(), "unknown type is refused with a message");
    }

    // ---- label field ----
    {
        // Two blobs of labelled points, 0 around x=-3 and 1 around x=+3, seen
        // from above; the boundary must sit near x=0, every point of space
        // gets a label, and far away the nearer blob wins.
        std::mt19937 rng(11);
        std::normal_distribution<double> g(0.0, 0.7);
        std::vector<float> xyz, dirs;
        std::vector<int32_t> lab;
        for (int i = 0; i < 4000; i++) {
            const int l = i & 1;
            xyz.push_back((float)((l ? 3.0 : -3.0) + g(rng)));
            xyz.push_back((float)g(rng));
            xyz.push_back((float)g(rng));
            dirs.insert(dirs.end(), {0.f, 0.f, 1.f});
            lab.push_back(l);
        }
        xyz.insert(xyz.end(), {0.f, 0.f, 0.f});
        dirs.insert(dirs.end(), {0.f, 0.f, 0.f});
        lab.push_back(-1);   // votes for nothing
        LabelField v = LabelField::build(xyz.data(), lab.data(), dirs.data(), (int64_t)lab.size(), 8);
        check(!v.empty() && v.num_labels() == 2 && v.num_seeds() == 4000, "field built with two labels");
        check(v.label(-3.0f, 0, 0) == 0 && v.label(3.0f, 0, 0) == 1, "blob centres are owned");
        check(v.label(-50.0f, 40.0f, 40.0f) == 0 && v.label(50.0f, 0, 5.0f) == 1,
              "far away, the nearer blob owns the point");
        int flips = 0;
        int last = v.label(-2.5f, 0, 1.0f);
        double where = 0;
        for (double x = -2.5; x <= 2.5; x += 0.05) {
            const int l = v.label((float)x, 0, 1.0f);
            if (l != last) { flips++; where = x; last = l; }
        }
        check(flips == 1 && std::fabs(where) < 0.6,
              "one boundary along the line between the blobs, at x=" + std::to_string(where));
        const std::vector<int64_t> h = v.histogram();
        check(h.size() == 2 && h[0] == 2000 && h[1] == 2000, "histogram counts the seeds");

        // The side a seed was seen from: a point behind it (below, here) is
        // farther than the same distance in front.
        {
            std::vector<float> one = {0, 0, 0, 5, 0, 0};
            std::vector<float> d1 = {0, 0, 1, 0, 0, 0};
            std::vector<int32_t> l1 = {0, 1};
            LabelField w = LabelField::build(one.data(), l1.data(), d1.data(), 2, 8);
            check(w.label(0.0f, 0, 2.0f) == 0, "in front of the seed: the seed's");
            check(w.label(0.0f, 0, -2.0f) == 1, "behind the seed: the omni camera's, though farther");
            const double p[3] = {0.5, 0, 0};
            const double up[3] = {0, 0, 1}, down[3] = {0, 0, -1};
            check(w.label(p, up) == 0 && w.label(p, down) == 1,
                  "a query facing away from where the seed was seen belongs elsewhere");
            // A camera behind a surface did not see it either.
            const double away[3] = {-1, 0, 0};
            check(w.label(p, away) == 0, "a query facing away from the camera is the seed's");
        }
        // Two rooms and a wall seen from one: the wall's back belongs to the
        // room whose cameras face it, even 1 cm from the textured side.
        {
            std::vector<float> xyz, dirs;
            std::vector<int32_t> lab;
            for (int i = 0; i < 200; i++) {           // wall at x=0 seen from x<0
                xyz.insert(xyz.end(), {0.f, -5.f + 0.05f * i, -2.f + 0.02f * i});
                dirs.insert(dirs.end(), {-1.f, 0.f, 0.f});
                lab.push_back(0);
            }
            for (int i = 0; i < 60; i++) {            // floors of both rooms
                xyz.insert(xyz.end(), {-4.f + 0.1f * i * (i % 2 ? 1 : -1), -2.f + 0.07f * i, -3.f});
                dirs.insert(dirs.end(), {0.f, 0.f, 1.f});
                lab.push_back(0);
                xyz.insert(xyz.end(), {4.f + 0.1f * i * (i % 2 ? 1 : -1), -2.f + 0.07f * i, -3.f});
                dirs.insert(dirs.end(), {0.f, 0.f, 1.f});
                lab.push_back(1);
            }
            for (int i = 0; i < 10; i++) {            // cameras 2 m into each room
                xyz.insert(xyz.end(), {-2.f, -2.f + 0.4f * i, 0.f});
                dirs.insert(dirs.end(), {0.f, 0.f, 0.f});
                lab.push_back(0);
                xyz.insert(xyz.end(), {2.f, -2.f + 0.4f * i, 0.f});
                dirs.insert(dirs.end(), {0.f, 0.f, 0.f});
                lab.push_back(1);
            }
            LabelField w = LabelField::build(xyz.data(), lab.data(), dirs.data(), (int64_t)lab.size(), 8);
            const double back[3] = {0.01, 0, 0}, front[3] = {-0.01, 0, 0};
            const double toB[3] = {1, 0, 0}, toA[3] = {-1, 0, 0};
            check(w.label(back, toB) == 1, "wall back facing room B is B's");
            check(w.label(front, toA) == 0, "wall front facing room A is A's");
            const double behind[3] = {0.3, 0, 0};
            check(w.label(behind) == 1 && w.label(front) == 0,
                  "without a normal, the side of the wall decides a hand's width away");
            const double air[3] = {1.0, 0, 0};
            check(w.label(air) == 1, "air a metre into room B is B's");
        }

        // Binary round trip, and the region view.
        std::string bytes;
        v.write(bytes);
        LabelField back;
        size_t used = 0;
        // Byte compare: the packed lanes hold bit patterns that are NaN as floats.
        auto same = [](const std::vector<float>& a, const std::vector<float>& b) {
            return a.size() == b.size() &&
                   std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
        };
        check(LabelField::read(bytes.data(), bytes.size(), used, back) && used == bytes.size() &&
                  same(back.seeds, v.seeds) && same(back.nodes, v.nodes),
              "field round-trips through bytes");
        auto shared = std::make_shared<LabelField>(v);
        LabelRegion r0(shared, 0), r1(shared, 1);
        check(r0.inside(-3.0f, 0, 0) && !r1.inside(-3.0f, 0, 0), "label regions");
        std::string err;
        std::unique_ptr<Region> rj = region_from_json(json_parse(region_to_json(r1)), "", err);
        check(rj && rj->inside(3.0f, 0, 0) && !rj->inside(-3.0f, 0, 0),
              "label region round-trips inline through JSON: " + err);

        // Batch query agrees with the scalar one; the lattice samples.
        std::vector<float> q = {-3, 0, 0, 3, 0, 0, 0.3f, 0, 1};
        int32_t out[3];
        v.labels_of(q.data(), 3, out);
        check(out[0] == 0 && out[1] == 1 && out[2] == v.label(0.3f, 0, 1.0f), "labels_of matches label");
        std::vector<float> lx;
        std::vector<int32_t> ll;
        v.lattice(8, lx, ll);
        check(ll.size() == 512 && lx.size() == ll.size() * 3, "lattice sample");

        // Compiled program: the region evaluated by the host mirror agrees
        // with the Region objects it was compiled from.
        {
            auto box = std::make_shared<BoxRegion>();
            box->center[0] = 3; box->half[0] = 1; box->half[1] = 1; box->half[2] = 1;
            CsgRegion u;
            u.op = CsgOp::Difference;
            u.children = {std::make_shared<LabelRegion>(shared, 1), box};
            RegionProgram prog;
            std::string perr;
            check(compile_region(u, prog, perr) && prog.num_nodes() == 3, "program compiles: " + perr);
            int disagree = 0;
            std::uniform_real_distribution<double> uu(-6.0, 6.0);
            for (int i = 0; i < 2000; i++) {
                const double p[3] = {uu(rng), uu(rng), uu(rng)};
                disagree += program_contains(prog, p) != u.contains(p);
            }
            check(disagree == 0, "program mirror agrees with the region objects");
            RegionProgram moved = prog;
            const double sc = 2.5, sh[3] = {10, -3, 0.5};
            moved.apply_similarity(sc, sh);
            disagree = 0;
            for (int i = 0; i < 2000; i++) {
                const double p[3] = {uu(rng), uu(rng), uu(rng)};
                const double q[3] = {sc * p[0] + sh[0], sc * p[1] + sh[1], sc * p[2] + sh[2]};
                disagree += program_contains(moved, q) != program_contains(prog, p);
            }
            check(disagree == 0 && moved.field != prog.field, "a moved program answers as the original did");
            HalfSpaceRegion hs;
            hs.normal[0] = 0.6; hs.normal[2] = 0.8; hs.offset = -0.7;
            RegionProgram hp;
            compile_region(hs, hp, perr);
            RegionProgram hm = hp;
            hm.apply_similarity(sc, sh);
            disagree = 0;
            for (int i = 0; i < 2000; i++) {
                const double p[3] = {uu(rng), uu(rng), uu(rng)};
                const double q[3] = {sc * p[0] + sh[0], sc * p[1] + sh[1], sc * p[2] + sh[2]};
                disagree += program_contains(hm, q) != hs.contains(p);
            }
            check(disagree == 0, "a moved half-space answers as the original did");
            const double c[3] = {0, 0, 0};
            auto cube = unit_cube(0.5, c);
            check(!compile_region(*cube, prog, perr) && !perr.empty(), "a mesh has no device form");
        }
    }

    // ---- boundary mesh ----
    {
        SphereRegion sph;
        sph.center[0] = 0.3;
        sph.radius = 1.0;
        Aabb box;
        for (int a = 0; a < 3; a++) { box.lo[a] = -2; box.hi[a] = 2; }
        const RegionMesh m = region_boundary_mesh(sph, box, 48);
        double vol = 0, worst = 0;
        std::map<std::pair<uint32_t, uint32_t>, int> edges;
        for (size_t t = 0; t < m.tri.size(); t += 3) {
            const float* a = &m.xyz[m.tri[t] * 3];
            const float* b = &m.xyz[m.tri[t + 1] * 3];
            const float* c = &m.xyz[m.tri[t + 2] * 3];
            vol += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                    a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0;
            for (int e = 0; e < 3; e++) edges[{m.tri[t + e], m.tri[t + (e + 1) % 3]}]++;
        }
        for (size_t v = 0; v < m.xyz.size(); v += 3) {
            const double r = std::sqrt((m.xyz[v] - 0.3) * (m.xyz[v] - 0.3) + m.xyz[v + 1] * m.xyz[v + 1] +
                                       m.xyz[v + 2] * m.xyz[v + 2]);
            worst = std::max(worst, std::fabs(r - 1.0));
        }
        bool closed = true;
        for (const auto& [e, n] : edges) closed = closed && n == 1 && edges.count({e.second, e.first});
        check(!m.empty() && closed, "sphere boundary: closed, every edge once each way");
        check(std::fabs(vol / (4.0 / 3.0 * M_PI) - 1.0) < 0.03, "sphere boundary: wound outward, volume within 3%");
        check(worst < 4.0 / 48, "sphere boundary: vertices within a cell of the surface");

        HalfSpaceRegion hs;
        const RegionMesh plane = region_boundary_mesh(hs, box, 16);
        bool flat = !plane.empty();
        for (size_t v = 2; v < plane.xyz.size(); v += 3) flat = flat && std::fabs(plane.xyz[v]) < 0.25 / 64;
        check(flat, "unbounded half-space: only the plane, open at the box");

        const float seeds[6] = {-1, 0, 0, 1, 0, 0};
        const int32_t labels[2] = {0, 1};
        const LabelField two = LabelField::build(seeds, labels, nullptr, 2);
        const std::vector<RegionMesh> parts = label_boundary_meshes(two, box, 16);
        bool mid = parts.size() == 2 && !parts[0].empty() && !parts[1].empty();
        for (const RegionMesh& pm : parts)
            for (size_t v = 0; v < pm.xyz.size(); v += 3) mid = mid && std::fabs(pm.xyz[v]) < 0.25 / 64;
        check(mid, "label meshes: each part's boundary is the bisecting plane");
    }

    if (g_failures == 0) std::printf("region_test: OK\n");
    return g_failures ? 1 : 0;
}
