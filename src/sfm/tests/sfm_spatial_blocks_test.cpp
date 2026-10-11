#include "sfm/feature/DescriptorCache.h"
#include "sfm/feature/SpatialBlocks.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <numeric>
#include <random>
#include <stdexcept>

using namespace sfm;
using Pair = std::pair<uint32_t, uint32_t>;

static void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

static std::vector<Pair> referencePairs(const std::vector<Vec3>& p, size_t k, double radius) {
    std::vector<Pair> pairs;
    for (uint32_t i = 0; i < p.size(); ++i) {
        std::vector<std::pair<double, uint32_t>> nearest;
        for (uint32_t j = 0; j < p.size(); ++j) {
            const double dx = p[i].x - p[j].x, dy = p[i].y - p[j].y;
            const double d2 = dx * dx + dy * dy;
            if (i != j && (radius == 0 || d2 <= radius * radius)) nearest.emplace_back(d2, j);
        }
        std::sort(nearest.begin(), nearest.end());
        nearest.resize(std::min(k, nearest.size()));
        for (const auto& q : nearest) pairs.emplace_back(std::min(i, q.second), std::max(i, q.second));
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    return pairs;
}

static void testSpatial() {
    std::mt19937 rng(19);
    std::vector<Vec3> p;
    for (int i = 0; i < 237; ++i)
        p.push_back({double(rng() % 47), double(rng() % 41), double(rng() % 1000)});
    for (size_t k : {1u, 8u, 80u, 400u})
        for (double radius : {0.0, 1.0, 15.0}) {
            const auto plan = SpatialBlockIndex(p).plan(17, k, radius);
            check(plan.pairs == referencePairs(p, k, radius), "KD neighbours disagree with exhaustive reference");
            check(plan.blocks == 14, "wrong block count");
            std::vector<size_t> sizes(plan.blocks);
            for (uint32_t owner : plan.owner) ++sizes.at(owner);
            check(*std::max_element(sizes.begin(), sizes.end()) <= 17, "block size exceeded");
            auto ordered = plan.pairs;
            orderSpatialPairs(ordered, plan.owner);
            std::sort(ordered.begin(), ordered.end());
            check(ordered == plan.pairs, "spatial scheduling lost a pair");
            check(SpatialBlockIndex(p).plan(17, k, radius).owner == plan.owner, "unstable blocks");
        }
    p.assign(100, Vec3{3, 4, 0});
    check(SpatialBlockIndex(p).plan(9, 3, 0).pairs == referencePairs(p, 3, 0), "GPS ties are not stable");
    p = {{0, 0, 0}, {1, 0, 500}, {2, 0, 0}, {3, 0, -500}};
    const auto plan = SpatialBlockIndex(p).plan(2, 2, 1.1);
    check(plan.pairs == referencePairs(p, 2, 1.1), "altitude influenced horizontal pairing");
    bool cross = false;
    for (const auto& q : plan.pairs) cross |= plan.owner[q.first] != plan.owner[q.second];
    check(cross, "block boundary lost its connecting pair");
    check(SpatialBlockIndex({}).plan(2, 1, 0).pairs.empty(), "empty positions failed");
    bool rejected = false;
    try { SpatialBlockIndex({Vec3{std::numeric_limits<double>::quiet_NaN(), 0, 0}}); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "invalid GPS was accepted");
}

static FeatureSet feature(size_t i, DType dtype) {
    FeatureSet f;
    f.width = 1024; f.height = 768; f.dim = 8; f.dtype = dtype;
    f.keypoints.resize(8 + i % 4);
    f.descriptors.resize((size_t)f.count() * f.dim * dtypeSize(dtype));
    for (size_t k = 0; k < f.descriptors.size(); ++k) f.descriptors[k] = (uint8_t)(k + i * 13);
    f.colors.assign((size_t)f.count() * 3, (uint8_t)i);
    return f;
}

static std::vector<FeatureMatch> cpuMatch(const FeatureSet& a, const FeatureSet& b) {
    std::vector<FeatureMatch> m;
    for (uint32_t i = 0; i < a.count(); ++i) {
        uint32_t best = 0;
        uint64_t distance = UINT64_MAX;
        const size_t row = (size_t)a.dim * dtypeSize(a.dtype);
        for (uint32_t j = 0; j < b.count(); ++j) {
            uint64_t d = 0;
            for (size_t k = 0; k < row; ++k) {
                const int diff = (int)a.descriptors[i * row + k] - b.descriptors[j * row + k];
                d += (uint64_t)(diff * diff);
            }
            if (d < distance) { distance = d; best = j; }
        }
        m.push_back({i, best});
    }
    return m;
}

static void testCache(const std::filesystem::path& dir, DType dtype) {
    std::vector<std::string> names;
    std::vector<FeatureSet> full, meta;
    for (size_t i = 0; i < 17; ++i) {
        names.push_back("flight/" + std::to_string(i));
        full.push_back(feature(i, dtype));
        writeFeatures((dir / (names.back() + ".bin")).string(), full.back());
        meta.push_back(readFeatures((dir / (names.back() + ".bin")).string(), false));
    }
    std::vector<Pair> pairs;
    for (uint32_t i = 0; i < 17; ++i)
        for (uint32_t j = i + 1; j < 17; ++j) pairs.emplace_back(i, j);
    const size_t budget = dtype == DType::U8 ? 200 : 800;
    {
        DescriptorCache cache(dir, names, meta, budget, 3);
        std::vector<std::vector<FeatureMatch>> out;
        auto run = [&](size_t b, size_t e, std::vector<std::vector<FeatureMatch>>& chunk) {
            size_t resident = 0, bytes = 0;
            for (const auto& f : meta) {
                resident += !f.descriptors.empty();
                bytes += f.descriptors.size();
            }
            check(resident <= 3 && bytes <= budget, "pinned cache exceeds its bounds");
            for (size_t k = b; k < e; ++k)
                chunk.push_back(cpuMatch(meta[pairs[k].first], meta[pairs[k].second]));
        };
        cache.match(pairs, 0, pairs.size(), out, run);
        check(out.size() == pairs.size(), "cache changed result count");
        for (size_t k = 0; k < out.size(); ++k) {
            const auto ref = cpuMatch(full[pairs[k].first], full[pairs[k].second]);
            check(out[k].size() == ref.size(), "cache changed match count");
            for (size_t j = 0; j < ref.size(); ++j)
                check(out[k][j].idx1 == ref[j].idx1 && out[k][j].idx2 == ref[j].idx2,
                      "cache changed descriptor matches");
        }
        check(cache.peakBytes() <= budget && cache.loads() > full.size(), "eviction was not exercised");
        const size_t loads = cache.loads();
        cache.match(pairs, pairs.size() - 1, pairs.size(), out, run);
        check(cache.loads() == loads, "cached pair was reread");
    }
    for (size_t i = 0; i < meta.size(); ++i)
        check(meta[i].descriptors.empty() && meta[i].colors == full[i].colors &&
              meta[i].count() == full[i].count(), "cache cleanup changed feature metadata");
    {
        DescriptorCache cache(dir, names, meta, 1, 2);
        std::vector<std::vector<FeatureMatch>> out;
        bool rejected = false;
        try { cache.match(pairs, 0, 1, out, [](auto, auto, auto&) {}); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected && cache.bytes() == 0, "oversized pair exceeded budget instead of failing");
    }
    std::filesystem::resize_file(dir / (names[1] + ".bin"), 28 + full[1].count() * 16 + 1);
    bool rejected = false;
    try {
        DescriptorCache cache(dir, names, meta, budget, 3);
        std::vector<std::vector<FeatureMatch>> out;
        cache.match(pairs, 0, 1, out, [](auto, auto, auto&) {});
    } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "truncated descriptor file was accepted");
    for (const auto& f : meta) check(f.descriptors.empty(), "failed cache load leaked descriptors");
}

static void testLarge() {
    std::vector<Vec3> p;
    for (int y = 0; y < 150; ++y)
        for (int x = 0; x < 200; ++x) p.push_back({x * 5.0, y * 5.0, 100.0});
    const auto start = std::chrono::steady_clock::now();
    const auto plan = SpatialBlockIndex(p).plan(512, 40, 0);
    check(plan.owner.size() == 30000 && plan.blocks == 59, "30k plan is incomplete");
    check(plan.pairs.size() <= p.size() * 40, "pair count is not bounded by N*k");
    size_t cross = 0;
    for (const auto& q : plan.pairs) cross += plan.owner[q.first] != plan.owner[q.second];
    check(cross > 0, "30k plan has no cross-block connections");
    std::printf("30k grid: %zu blocks, %zu pairs, %zu cross-block pairs, %.3f s\n",
                plan.blocks, plan.pairs.size(), cross,
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
}

int main() {
    const auto dir = std::filesystem::temp_directory_path() /
        ("spirula-spatial-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(dir / "flight");
        testSpatial();
        testCache(dir, DType::U8);
        testCache(dir, DType::F32);
        testLarge();
        std::filesystem::remove_all(dir);
        std::puts("sfm_spatial_blocks_test: PASS");
        return 0;
    } catch (const std::exception& e) {
        std::filesystem::remove_all(dir);
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
