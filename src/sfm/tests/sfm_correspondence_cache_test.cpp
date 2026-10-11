#include "sfm/map/CorrespondenceGraph.h"

#include <cstdio>
#include <random>
#include <thread>

using namespace sfm;
namespace fs = std::filesystem;

namespace {

void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

struct Fixture {
    fs::path dir;
    Fixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = fs::temp_directory_path() / ("spirula-graph-test-" + std::to_string(stamp));
        check(fs::create_directory(dir), "reserve private test directory");
        std::ofstream(dir / "keep.txt") << "unrelated caller file";
    }
    ~Fixture() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

MatchesDatabase makeDatabase(std::vector<uint32_t>& features) {
    MatchesDatabase db;
    std::mt19937 random(381);
    features.resize(128);
    db.images.resize(features.size());
    for (uint32_t i = 0; i < features.size(); ++i) {
        features[i] = 1024 + i % 11;
        db.images[i] = {std::to_string(i), features[i]};
        for (uint32_t j = i + 1; j < std::min<uint32_t>(features.size(), i + 6); ++j) {
            TwoViewMatches pair;
            pair.image1 = i;
            pair.image2 = j;
            for (uint32_t m = 0; m < 512; ++m)
                pair.matches.push_back({uint32_t(random() % 1024), uint32_t(random() % 1024)});
            db.pairs.push_back(std::move(pair));
        }
    }
    TwoViewMatches self;
    self.image1 = self.image2 = 19;
    self.matches = {{3, 5}, {3, 3}};
    db.pairs.push_back(std::move(self));
    db.pairs[0].matches.push_back({UINT32_MAX, 0});
    db.pairs[0].matches.push_back({0, UINT32_MAX});
    return db;
}

bool equal(CorrespondenceView a, CorrespondenceView b) {
    if (a.size() != b.size()) return false;
    auto other = b.begin();
    for (const auto value : a) {
        const auto expected = *other;
        if (value.image_id != expected.image_id || value.feature_idx != expected.feature_idx)
            return false;
        ++other;
    }
    return true;
}

void compareAll(const CorrespondenceGraph& a, const CorrespondenceGraph& b) {
    check(a.numImages() == b.numImages(), "image counts agree");
    for (uint32_t i = 0; i < a.numImages(); ++i) {
        check(a.numFeatures(i) == b.numFeatures(i), "feature counts agree");
        for (uint32_t f = 0; f < a.numFeatures(i); ++f)
            check(equal(a.at(i, f), b.at(i, f)), "correspondence order and multiplicity agree");
    }
}

void testCache(const MatchesDatabase& db, const std::vector<uint32_t>& features,
               const CorrespondenceGraph& reference, const fs::path& dir) {
    CorrespondenceGraph graph;
    constexpr size_t budget = 64 * 1024;
    graph.build(db, features, {budget, dir});
    check(graph.stats().disk_backed && graph.stats().spilled_images == features.size(),
          "large graph spills every image to private storage");
    size_t disk_bytes = 0;
    for (const auto& file : fs::directory_iterator(graph.cacheDirectory()))
        disk_bytes += fs::file_size(file.path());
    check(graph.stats().spilled_bytes == disk_bytes, "spill byte statistics describe files");
    compareAll(graph, reference);
    check(graph.stats().evictions > 0 && graph.stats().loads > 0,
          "image LRU reloads and evicts under pressure");

    const auto outer = graph.at(60, 3);
    std::vector<CorrespondenceView> pinned;
    for (uint32_t i = 0; i < features.size(); ++i) {
        pinned.push_back(graph.at(i, 3));
        check(equal(outer, reference.at(60, 3)), "nested views survive cache pressure");
    }
    check(graph.stats().mapped_fallbacks > 0, "pinned views use file mappings instead of growing heap");
    check(graph.stats().resident_bytes <= budget && graph.stats().peak_resident_bytes <= budget,
          "active nested leases cannot grow the heap cache beyond its budget");
    for (uint32_t i = 0; i < pinned.size(); ++i)
        check(equal(pinned[i], reference.at(i, 3)), "all held views retain exact correspondences");
    pinned.clear();

    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (uint32_t worker = 0; worker < 12; ++worker)
        workers.emplace_back([&, worker] {
            try {
                std::mt19937 random(worker + 16);
                for (int k = 0; k < 500; ++k) {
                    const uint32_t i = random() % features.size();
                    const uint32_t f = random() % features[i];
                    const auto a = graph.at(i, f);
                    const auto b = graph.at((i + 31) % features.size(), 7);
                    if (!equal(a, reference.at(i, f)) ||
                        !equal(b, reference.at((i + 31) % features.size(), 7))) failed = true;
                }
            } catch (...) { failed = true; }
        });
    for (auto& thread : workers) thread.join();
    check(!failed, "parallel nested queries preserve all view lifetimes");
    check(graph.stats().peak_resident_bytes <= budget, "concurrent reads keep the heap budget");
    const auto stats = graph.stats();
    std::printf("128-image graph: disk %zu bytes, heap peak %zu bytes, loads %zu, mapped %zu\n",
                stats.spilled_bytes, stats.peak_resident_bytes, stats.loads, stats.mapped_fallbacks);
}

void testLeases(const MatchesDatabase& db, const std::vector<uint32_t>& features,
                const CorrespondenceGraph& reference, const fs::path& dir) {
    CorrespondenceView view;
    fs::path owned;
    {
        CorrespondenceGraph graph;
        graph.build(db, features, {1, dir});
        owned = graph.cacheDirectory();
        view = graph.at(9, 3);
        check(equal(view, reference.at(9, 3)), "oversized image maps without a heap allocation");
        check(graph.stats().resident_bytes == 0, "tiny budgets never allocate an oversized heap image");
    }
    check(fs::exists(owned) && equal(view, reference.at(9, 3)),
          "view lease owns graph storage after graph destruction");
    view = {};
    check(!fs::exists(owned) && fs::is_regular_file(dir / "keep.txt"),
          "last lease removes only the graph's private directory");

    CorrespondenceGraph graph;
    graph.build(db, features, {1, dir});
    owned = graph.cacheDirectory();
    view = graph.at(14, 7);
    graph.build(MatchesDatabase{}, {});
    check(graph.numImages() == 0 && !graph.stats().disk_backed &&
              equal(view, reference.at(14, 7)), "rebuild preserves leases of the old graph");
    view = {};
    check(!fs::exists(owned), "retired graph storage cleans up after its last view");
}

void testFailures(const MatchesDatabase& db, const std::vector<uint32_t>& features,
                  const fs::path& dir) {
    fs::path owned;
    {
        CorrespondenceGraph graph;
        graph.build(db, features, {1, dir});
        owned = graph.cacheDirectory();
        fs::resize_file(owned / "0.csr", 2);
        bool rejected = false;
        try { graph.at(0, 0); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "truncated mapped cache files are rejected");
    }
    check(!fs::exists(owned), "read failure does not leak cache storage");
    {
        CorrespondenceGraph graph;
        graph.build(db, features, {64 * 1024, dir});
        owned = graph.cacheDirectory();
        fs::resize_file(owned / "0.csr", 2);
        bool rejected = false;
        try { graph.at(0, 0); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected && !graph.stats().resident_bytes, "truncated heap reads leave no resident cache entry");
    }
    check(!fs::exists(owned), "heap read failure does not leak cache storage");
    CorrespondenceGraph graph;
    bool rejected = false;
    try { graph.build(db, features, {1, dir / "keep.txt"}); }
    catch (const std::exception&) { rejected = true; }
    check(rejected && fs::is_regular_file(dir / "keep.txt"),
          "invalid scratch paths preserve caller files");
    auto invalid = db;
    invalid.pairs[0].image1 = features.size();
    rejected = false;
    try { graph.build(invalid, features, {1, dir}); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "invalid image IDs fail before cache creation");
}

}  // namespace

int main() {
    try {
        Fixture fixture;
        std::vector<uint32_t> features;
        const auto db = makeDatabase(features);
        CorrespondenceGraph reference, ramCached;
        reference.build(db, features);
        ramCached.build(db, features, {64 * 1024 * 1024, fixture.dir});
        check(!reference.stats().disk_backed && !ramCached.stats().disk_backed,
              "default and sufficiently ramCached graphs retain the in-memory backend");
        compareAll(ramCached, reference);
        testCache(db, features, reference, fixture.dir);
        testLeases(db, features, reference, fixture.dir);
        testFailures(db, features, fixture.dir);
        check(fs::is_regular_file(fixture.dir / "keep.txt"), "caller scratch files survive all graphs");
        std::printf("correspondence cache: PASS\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "correspondence cache: FAIL: %s\n", e.what());
        return 1;
    }
}
