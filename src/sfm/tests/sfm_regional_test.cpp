#include "sfm/map/RegionalStore.h"
#include "sfm/map/RegionalPlan.h"
#include "sfm/tests/TestMain.h"
#include <cstdio>

namespace fs = std::filesystem;
using namespace sfm;

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

static int test(int, char**) {
    fs::path root = fs::temp_directory_path() / ("sfm-regional-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "features");
    MatchesDatabase db; Camera camera = Camera::defaultFor(0, 800, 600, 500, CamModel::Pinhole);
    db.cameras = {camera}; db.focal_prior = {1}; db.focal_measured = {1};
    for (uint32_t i = 0; i < 4; ++i) {
        db.images.push_back({"photo" + std::to_string(i), 3}); db.camera_ids.push_back(0);
        FeatureSet f; f.width = 800; f.height = 600; f.keypoints.resize(3); f.descriptors.resize(3 * f.dim);
        for (uint32_t k = 0; k < 3; ++k) { f.keypoints[k].x = 300.f + 20 * k; f.keypoints[k].y = 300.f; }
        writeFeatures((root / "features" / (db.images.back().name + ".bin")).string(), f);
    }
    db.pairs = {{0, 1, 2, {{1, 2}}}, {1, 2, 3, {{2, 1}}}, {2, 3, 4, {{1, 2}}}};
    writeMatches((root / "matches.bin").string(), db);
    MatchesIndex index; require(indexMatches((root / "matches.bin").string(), index) && index.complete, "index incomplete");
    require(index.pairs[1].config == 3 && index.metadata.camera_ids == db.camera_ids && index.metadata.focal_measured == db.focal_measured, "indexed camera or pair metadata lost");
    std::vector<regional::Region> plan = regional::planRegions(index, {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 2);
    std::vector<uint32_t> owned;
    for (const auto& r : plan) { owned.insert(owned.end(), r.core.begin(), r.core.end()); require(r.core.size() <= 2, "requested region size ignored"); }
    std::sort(owned.begin(), owned.end()); require(owned == std::vector<uint32_t>({0, 1, 2, 3}), "regional ownership incomplete");
    MatchesIndex dense = index;
    for (auto& im : dense.images) im.num_features = 1000000;
    for (auto& pair : dense.pairs) pair.count = 1000000;
    auto custom = regional::planRegions(dense, {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 3);
    require(custom.size() == 2 && custom[0].core.size() == 3, "custom core limit was preemptively reduced by density");
    {
        MatchesIndex links = index;
        links.pairs = {{0, 1, 30, 0, 0}, {0, 2, 90, 0, 0}, {0, 3, 60, 0, 0}};
        regional::Region narrow{{0}, {0}};
        require(regional::expandAlignmentContext(links, narrow, {0}, {1, 2}, 1) && narrow.images == std::vector<uint32_t>({0, 2}),
                "alignment context did not prefer the strongest registered anchor");
        require(narrow.core == std::vector<uint32_t>({0}), "alignment expansion changed ownership");
        require(regional::expandAlignmentContext(links, narrow, {0}, {1, 2}, 1) && narrow.images == std::vector<uint32_t>({0, 1, 2}),
                "alignment retry did not add the remaining anchor");
        require(!regional::expandAlignmentContext(links, narrow, {0}, {1, 2}, 1), "alignment retry added an unregistered anchor");
    }
    regional::Region subset{{1, 2}, {1, 2}};
    bool input_guard = false;
    try { regional::loadRegion(index, subset, root / "matches.bin", root / "features", 1); }
    catch (const regional::RegionInputOverBudget&) { input_guard = true; }
    require(input_guard, "actual region input no longer respects the working budget");
    auto input = regional::loadRegion(index, subset, root / "matches.bin", root / "features", 1u << 20);
    require(input.db.pairs.size() == 1 && input.db.pairs[0].config == 3 && input.global == subset.images, "foreign region pair loaded");
    require(input.features[0].count() == 1 && input.original[0][0] == 2 && input.original[1][0] == 1, "regional compaction lost feature identities");
    Reconstruction compact; compact.cameras[0] = camera;
    for (uint32_t i = 0; i < 2; ++i) {
        Image im; im.id = i; im.registered = true; im.name = db.images[i + 1].name;
        im.points2D = {{input.features[i].keypoints[0].x, input.features[i].keypoints[0].y}};
        im.point3D_ids = {1}; compact.images[i] = im;
    }
    Point3D cp; cp.xyz = {0, 0, 5}; cp.track = {{0, 0}, {1, 0}}; compact.points3D[1] = cp;
    regional::restoreFeatureIds(compact, input, index);
    require(compact.images.count(1) && compact.images.count(2) && compact.points3D.at(1).track[0].point2D_idx == 2 &&
        compact.points3D.at(1).track[1].point2D_idx == 1 && compact.images.at(2).point3D_ids.at(1) == 1, "global feature restoration failed");
    {
        regional::PointUnion points(root / "union.bin", sizeof(regional::PointNode) * 512);
        Point3D p; p.xyz = {1, 2, 3}; p.track = {{0, 0}, {1, 0}};
        for (size_t i = 0; i < 1700; ++i) points.add(p);
        points.join(0, 1699); points.join(700, 1699); points.join(1200, 700);
        require(points.root(0) == points.root(1200) && points.nodes.get(points.root(0)).fragments == 4, "paged union lost evicted roots");
        points.nodes.flush();
    }
    {
        fs::path records = root / "records.bin";
        { std::ofstream out(records, std::ios::binary); for (uint64_t i = 500; i > 0; --i) regional::record(out, regional::Observation{i % 71, i}); }
        auto sorted = regional::sortObservations(records, root / "sort", sizeof(regional::Observation) * 3);
        std::ifstream in(sorted, std::ios::binary); regional::Observation row, previous; bool have = false; size_t count = 0;
        while (regional::nextRecord(in, row)) { require(!have || !(row < previous), "external merge order incorrect"); previous = row; have = true; count++; }
        require(count == 500, "external sort lost records");
    }
    Reconstruction poses; poses.cameras[0] = camera;
    Vec3 xyz{0, 0, 5};
    for (uint32_t i = 0; i < 3; ++i) {
        Image im; im.id = i; im.camera_id = 0; im.name = "photo" + std::to_string(i); im.registered = true;
        im.pose = {mat3Identity(), {-0.1 * i, 0, 0}}; poses.images[i] = im;
        FeatureSet f = readFeatures((root / "features" / (im.name + ".bin")).string());
        Vec3 projected = mul(im.pose.R, xyz) + im.pose.t;
        f.keypoints[1].x = float(camera.fx * projected.x / projected.z + camera.cx); f.keypoints[1].y = float(camera.cy);
        writeFeatures((root / "features" / (im.name + ".bin")).string(), f);
    }
    auto block = [&](uint32_t a, uint32_t b) {
        Reconstruction rec; rec.cameras = poses.cameras;
        for (uint32_t i : {a, b}) {
            Image im = poses.images.at(i); FeatureSet f = readFeatures((root / "features" / (im.name + ".bin")).string(), false);
            for (const auto& k : f.keypoints) im.points2D.push_back({k.x, k.y});
            im.point3D_ids.assign(3, kInvalidPoint3D); im.point3D_ids[1] = 1; rec.images[i] = im;
        }
        Point3D p; p.xyz = xyz; p.track = {{a, 1}, {b, 1}}; rec.points3D[1] = p; return rec;
    };
    std::vector<Reconstruction> blocks = {block(0, 1), block(1, 2)};
    blocks[0].points3D.at(1).xyz = {0.005, 0.002, 5.02};
    blocks[1].points3D.at(1).xyz = {-0.004, -0.001, 4.98};
    auto stats = regional::exportUnified(blocks.size(), [&](size_t i) { return blocks[i]; }, poses,
        [&](uint32_t i) { return root / "features" / ("photo" + std::to_string(i) + ".bin"); },
        root / "export", root / "output", sizeof(regional::PointNode) * 512, 3);
    Reconstruction merged = Reconstruction::readBinary((root / "output").string());
    require(stats.images == 3 && stats.points == 1 && stats.observations == 3 && merged.points3D.size() == 1, "cross-region track not unified");
    const auto& point = merged.points3D.begin()->second;
    for (const auto& e : point.track) require(merged.images.at(e.image_id).point3D_ids.at(e.point2D_idx) == merged.points3D.begin()->first, "COLMAP reciprocal track inconsistent");
    require(point.track.size() == 3 && stats.error < 1e-5, "duplicate boundary observation or wrong export pose");
    Vec3 displacement = point.xyz - xyz;
    require(displacement.x * displacement.x + displacement.y * displacement.y + displacement.z * displacement.z < 1e-10,
        "unified triangulation did not recover consistent geometry");
    fs::path checkpoint = root / "checkpoint.bin"; regional::atomicModel(checkpoint, merged);
    require(fs::exists(checkpoint) && !fs::exists(checkpoint.string() + ".part"), "checkpoint not committed");
    Reconstruction restored = model_store_detail::readModel(checkpoint);
    require(restored.numRegistered() == 3 && restored.points3D.size() == 1, "persistent checkpoint does not restore");
    fs::remove_all(root);
    std::puts("PASS: indexed region input, ownership, original feature identities, paged point union, bounded external sort, cross-region COLMAP tracks and persistent checkpoint");
    return 0;
}
int main(int argc, char** argv) { return sfmTestMain(argc, argv, test); }
