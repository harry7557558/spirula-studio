// Spilled pair lists and correspondence graph (sfm/core/Spill.h) read back the
// same as the heap copies they replace, and a write to a spilled list throws.
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Matches.h"
#include "sfm/map/CorrespondenceGraph.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;
namespace fs = std::filesystem;

static int fails = 0;

static void check(bool ok, const char* what) {
    std::printf("  %-58s %s\n", what, ok ? "ok" : "BAD");
    if (!ok) fails++;
}

static MatchesDatabase database(uint32_t images, uint32_t features) {
    MatchesDatabase db;
    for (uint32_t i = 0; i < images; i++)
        db.images.push_back({"img" + std::to_string(i), features});
    for (uint32_t a = 0; a < images; a++)
        for (uint32_t b = a + 1; b < images && b <= a + 3; b++) {
            TwoViewMatches p{a, b, 2, {}};
            for (uint32_t f = (a + b) % 3; f < features; f += 3)
                p.matches.push_back({f, (f * 7 + a) % features});
            db.pairs.push_back(std::move(p));
        }
    db.pairs.push_back({0, images - 1, 0, {}});  // a pair with nothing in it
    return db;
}

static std::vector<std::vector<Correspondence>> dump(const CorrespondenceGraph& g) {
    std::vector<std::vector<Correspondence>> out;
    for (uint32_t i = 0; i < g.numImages(); i++)
        for (uint32_t f = 0; f < g.numFeatures(i); f++) {
            out.emplace_back();
            for (const Correspondence& c : g.at(i, f)) out.back().push_back(c);
        }
    return out;
}

static bool same(const std::vector<std::vector<Correspondence>>& a,
                 const std::vector<std::vector<Correspondence>>& b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); k++) {
        if (a[k].size() != b[k].size()) return false;
        for (size_t j = 0; j < a[k].size(); j++)
            if (a[k][j].image_id != b[k][j].image_id ||
                a[k][j].feature_idx != b[k][j].feature_idx)
                return false;
    }
    return true;
}

static int run(int, char**) {
    const fs::path dir = fs::temp_directory_path() / "spirula_sfm_spill_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    MatchesDatabase db = database(12, 500);
    const MatchesDatabase heap = db;
    check(spillMatches(db, (dir / "matches.spill").string()), "pair lists spill");
    bool equal = db.pairs.size() == heap.pairs.size();
    for (size_t p = 0; equal && p < db.pairs.size(); p++) {
        equal = db.pairs[p].matches.size() == heap.pairs[p].matches.size() &&
                (db.pairs[p].matches.empty() || db.pairs[p].matches.isView());
        for (size_t k = 0; equal && k < db.pairs[p].matches.size(); k++)
            equal = db.pairs[p].matches[k].idx1 == heap.pairs[p].matches[k].idx1 &&
                    db.pairs[p].matches[k].idx2 == heap.pairs[p].matches[k].idx2;
    }
    check(equal, "... and read back as they were, from the mapping");
    bool threw = false;
    try {
        db.pairs[0].matches.push_back({0, 0});
    } catch (const std::logic_error&) {
        threw = true;
    }
    check(threw, "a write to a spilled list throws");
    const MatchesDatabase copy = db;
    check(copy.pairs[1].matches.data() == db.pairs[1].matches.data(),
          "a copy of the database shares the mapping");

    const std::vector<uint32_t> nf(db.images.size(), 500u);
    CorrespondenceGraph narrow, wide;
    narrow.build(heap, nf);
    wide.build(db, nf, /*wide=*/true);
    const auto before = dump(narrow);
    check(same(before, dump(wide)), "one- and two-word entries agree, spilled lists or not");
    CorrespondenceGraph paged;
    paged.build(db, nf, CorrespondenceGraph::Options{4096, dir});
    check(paged.stats().disk_backed && same(before, dump(paged)),
          "bounded graph cache reads spilled match lists identically");
    check(narrow.spill((dir / "graph1.spill").string()) &&
              wide.spill((dir / "graph2.spill").string()),
          "graph spills");
    check(same(before, dump(narrow)) && same(before, dump(wide)), "... and answers as it did");
#if !defined(_WIN32)
    check(fs::is_empty(dir), "nothing is left in the directory while mapped");
#endif
    fs::remove_all(dir);
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
