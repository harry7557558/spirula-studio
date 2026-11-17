// Sequences (sfm/core/Sequence.h, D79): the table, its window pairs, and a
// synthetic walk past a duplicated structure through the mapper.
//
// Prints PASS/FAIL and returns 0/1. See docs/testing.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Sequence.h"
#include "sfm/feature/Pairing.h"
#include "sfm/feature/PairSelection.h"
#include "sfm/map/Mapper.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        printf("  FAIL: %s\n", what);
        fails++;
    }
}

static void testTable() {
    SequenceDef d;
    check(parseSequenceArg("cam0,cam1/", d).empty() && d.members.size() == 2 &&
              d.members[1] == "cam1",
          "parse: two members, trailing slash dropped");
    check(parseSequenceArg(".", d).empty() && d.members.size() == 1 && d.members[0].empty(),
          "parse: '.' is the image directory");

    const std::vector<std::string> names = {"cam0/00010", "cam0/00020", "cam0/00030",
                                            "cam1/00010", "cam1/00030", "other/a",
                                            "other/b"};
    std::vector<SequenceDef> defs(2);
    defs[0].members = {"cam0", "cam1"};
    defs[1].members = {"other"};
    SequenceTable t = buildSequenceTable(names, defs);
    check(t.length.size() == 2 && t.length[0] == 3 && t.length[1] == 2, "two sequences sized");
    check(t.pos[0] == 0 && t.pos[1] == 1 && t.pos[2] == 2, "positions follow the stems");
    check(t.pos[3] == 0 && t.pos[4] == 2 && t.member[3] == 1, "a rig-mate shares its position");
    check(t.distance(0, 4) == 2 && t.nearby(0, 4, 2) && !t.nearby(0, 4, 1), "distance across lenses");
    check(t.distance(0, 5) == INT_MAX && t.seq[5] == 1, "no distance across sequences");
    check(t.has(6) && t.sequenceOf(6) == 1, "membership");

    const std::vector<uint32_t> local = {kNoImage, 0, 1, kNoImage, 2, kNoImage, kNoImage};
    SequenceTable sub = t.subset(local, 3);
    check(sub.pos[0] == 1 && sub.pos[1] == 2 && sub.pos[2] == 2 && sub.distance(0, 2) == 1,
          "subset keeps positions");

    const auto pairs = sequenceWindowPairs(t, 1, false);
    auto has = [&](uint32_t a, uint32_t b) {
        return std::binary_search(pairs.begin(), pairs.end(), std::make_pair(a, b));
    };
    check(has(0, 1) && has(1, 2) && has(3, 4) && has(5, 6) && !has(2, 3) && !has(0, 3),
          "window pairs run per member and never cross");

    bool threw = false;
    try {
        std::vector<SequenceDef> bad(1);
        bad[0].members = {"cam0", "nothing"};
        buildSequenceTable(names, bad);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "a member no image matches is an error");
    threw = false;
    try {
        std::vector<SequenceDef> twice(2);
        twice[0].members = {"cam0"};
        twice[1].members = {""};
        buildSequenceTable(names, twice);
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "an image in two sequences is an error");
}

// A walk along a wall whose far end repeats its near end feature for feature:
// room A, a corridor of its own, and room B a copy of A. Every camera of B
// matches every camera of A that saw the same part of the pattern.
struct Walk {
    int W = 1280, H = 960;
    double focal = 800;
    std::vector<Pose> gt;
    std::vector<FeatureSet> feats;
    MatchesDatabase db;
    std::vector<std::string> names;
};

static Pose lookAtX(const Vec3& C) {
    // Camera z along world +x, camera x along world +z, camera y along world -y.
    Mat3 R = {0, 0, 1, 0, -1, 0, 1, 0, 0};
    Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

static Walk makeWalk() {
    Walk w;
    const int NA = 300, NC = 300;
    Camera K = Camera::defaultFor(1, w.W, w.H, w.focal);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> ux(4.5, 7.5), uy(-3.5, 3.5), uz(0, 9), uc(9, 21);
    std::normal_distribution<double> noise(0.0, 0.4);
    // ids 0..NA-1: room A and, 21 units on, its copy; ids NA..: the corridor.
    std::vector<Vec3> A(NA), C(NC);
    for (Vec3& p : A) p = {ux(rng), uy(rng), uz(rng)};
    for (Vec3& p : C) p = {ux(rng), uy(rng), uc(rng)};
    const double D = 21.0;
    const int NF = NA + NC;
    for (double t = -3.0; t <= 33.0 + 1e-9; t += 0.5) w.gt.push_back(lookAtX({0, 0, t}));
    const int M = (int)w.gt.size();
    w.feats.resize(M);
    w.names.resize(M);
    std::vector<std::vector<char>> vis(M, std::vector<char>(NF, 0));
    for (int c = 0; c < M; c++) {
        char nm[32];
        snprintf(nm, sizeof nm, "%03d", c);
        w.names[c] = nm;
        FeatureSet& fs = w.feats[c];
        fs.width = w.W;
        fs.height = w.H;
        fs.keypoints.assign(NF, {-1000, -1000, 2, 0, 0});
        auto place = [&](int id, const Vec3& X) {
            Vec3 pc = mul(w.gt[c].R, X) + w.gt[c].t;
            Vec2 px = K.project(pc);
            if (pc.z > 0.1 && px.x > 0 && px.x < w.W && px.y > 0 && px.y < w.H) {
                fs.keypoints[id] = {(float)(px.x + noise(rng)), (float)(px.y + noise(rng)), 2, 0, 0};
                vis[c][id] = 1;
            }
        };
        for (int p = 0; p < NA; p++) {
            place(p, A[p]);
            if (!vis[c][p]) place(p, A[p] + Vec3{0, 0, D});
        }
        for (int p = 0; p < NC; p++) place(NA + p, C[p]);
    }
    w.db.images.resize(M);
    for (int c = 0; c < M; c++) w.db.images[c] = {w.names[c], (uint32_t)NF};
    for (int i = 0; i < M; i++)
        for (int j = i + 1; j < M; j++) {
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            for (int p = 0; p < NF; p++)
                if (vis[i][p] && vis[j][p]) tv.matches.push_back({(uint32_t)p, (uint32_t)p});
            if (tv.matches.size() >= 15) w.db.pairs.push_back(std::move(tv));
        }
    return w;
}

// Alignment-free shape check: every inter-camera distance of the model over
// the truth's is one scale. Returns the largest relative departure from the
// median ratio, or -1 with too few cameras.
static double shapeError(const Reconstruction& m, const Walk& w, uint32_t& registered) {
    std::vector<uint32_t> ids;
    for (const auto& kv : m.images)
        if (kv.second.registered) ids.push_back(kv.first);
    registered = (uint32_t)ids.size();
    if (ids.size() < 3) return -1;
    std::vector<double> ratio;
    for (size_t a = 0; a < ids.size(); a++)
        for (size_t b = a + 1; b < ids.size(); b++) {
            const double dm = (cameraCenter(m.images.at(ids[a]).pose) -
                               cameraCenter(m.images.at(ids[b]).pose)).norm();
            const double dg = (cameraCenter(w.gt[ids[a]]) - cameraCenter(w.gt[ids[b]])).norm();
            if (dg > 2.0) ratio.push_back(dm / dg);
        }
    std::sort(ratio.begin(), ratio.end());
    const double med = ratio[ratio.size() / 2];
    double worst = 0;
    for (double r : ratio) worst = std::max(worst, std::fabs(r / med - 1.0));
    return worst;
}

static void testOrderWeight() {
    const std::vector<std::string> names = {"a/1", "a/2", "a/3", "a/4", "b/1", "b/2"};
    const FileOrder o = fileOrder(names);
    check(o.pos[0] == 0 && o.pos[3] == 3 && o.pos[4] == 0 && o.run[4] != o.run[0],
          "file order: positions restart per folder");
    check(o.boost(0, 1, 1.0, 5.0) > o.boost(0, 3, 1.0, 5.0) && o.boost(0, 3, 1.0, 5.0) > 1.0,
          "file order: the boost falls with distance");
    check(o.boost(0, 4, 1.0, 5.0) == 1.0 && o.boost(0, 1, 0.0, 5.0) == 1.0,
          "file order: no boost across folders or when off");

    // Image 0 keeps one partner: 4 (other folder, score 10) or its neighbour 1
    // (score 8). 1, 2 and 4 each prefer image 5, so a pair with 0 in the union
    // is image 0's own choice.
    auto best = [&](double w) {
        detail::TopPartners top(6, 1, 4);
        for (uint32_t p : {1u, 2u, 4u}) top.add(p, 5, 1000, 1000 * o.boost(p, 5, w, 5.0));
        for (auto e : std::vector<std::pair<uint32_t, uint32_t>>{{1, 8}, {2, 3}, {4, 10}})
            top.add(0, e.first, e.second, e.second * o.boost(0, e.first, w, 5.0));
        uint32_t p = UINT32_MAX;
        for (const auto& pr : top.pairs())
            if (pr.first == 0) p = pr.second;
        return p;
    };
    check(best(0.0) == 4, "order weight off: content decides");
    check(best(1.0) == 1, "order weight on: the near pair wins a close call");
    check(best(1.0) != 2, "order weight: min_score still gates the raw score");
}

static void testMapperWalk(int device, bool verbose) {
    Walk w = makeWalk();
    MapperOptions opt;
    opt.verbose = verbose;
    opt.focal = w.focal;
    opt.device = device;
    std::vector<SequenceDef> defs(1);
    defs[0].members = {""};
    const SequenceTable seqs = buildSequenceTable(w.names, defs);

    Mapper plain(w.db, w.feats, opt);
    std::vector<Reconstruction> before = plain.run();
    uint32_t reg0 = 0;
    const double err0 = shapeError(before.front(), w, reg0);
    printf("sequence-walk: without sequences %u/%zu cameras in %zu model(s), shape error %.3f\n",
           reg0, w.gt.size(), before.size(), err0);

    Mapper mapper(w.db, w.feats, opt, {}, nullptr, &seqs);
    std::vector<Reconstruction> models = mapper.run();
    uint32_t reg = 0;
    const double err = shapeError(models.front(), w, reg);
    printf("sequence-walk: with the sequence %u/%zu cameras in %zu model(s), shape error %.3f\n",
           reg, w.gt.size(), models.size(), err);
    check(models.size() == 1, "the walk is one model");
    check(reg >= (uint32_t)(0.95 * w.gt.size()), "the walk registers");
    check(err >= 0 && err < 0.05, "the copy of the room is not folded onto the original");
}

static int cmdSequenceTest(int argc, char** argv) {
    int device = -1;
    bool verbose = false, gpu = true;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--device" && i + 1 < argc) device = std::stoi(argv[++i]);
        else if (a == "--verbose") verbose = true;
        else if (a == "--no-gpu") gpu = false;
    }
    testTable();
    testOrderWeight();
    if (gpu) testMapperWalk(device, verbose);
    printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc - 1, argv + 1, cmdSequenceTest); }
