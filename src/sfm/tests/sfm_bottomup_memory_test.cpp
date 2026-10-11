#include "sfm/map/Bottomup.h"
#include "sfm/tests/TestMain.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <set>

using namespace sfm;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static Pose lookAt(const Vec3& centre) {
    const Vec3 f = (Vec3{0, 0, 0} - centre).normalized();
    const Vec3 r = Vec3{0, 1, 0}.cross(f).normalized();
    const Vec3 u = f.cross(r);
    const Mat3 R = {r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z};
    const Vec3 t = mul(R, centre);
    return {R, {-t.x, -t.y, -t.z}};
}

struct Scene {
    static constexpr uint32_t views = 8, points = 160, features = 240;
    MatchesDatabase db;
    std::vector<FeatureSet> feats;

    Scene() : feats(views) {
        const Camera K = Camera::defaultFor(1, 1280, 960, 1200);
        std::mt19937 random(11);
        std::uniform_real_distribution<double> coordinate(-2.5, 2.5);
        std::normal_distribution<double> noise(0, 0.2);
        std::vector<Vec3> xyz(points);
        for (Vec3& p : xyz) p = {coordinate(random), coordinate(random), coordinate(random)};
        std::vector<std::vector<char>> visible(views, std::vector<char>(points));
        db.images.resize(views);
        for (uint32_t i = 0; i < views; ++i) {
            const double angle = -1.05 + 2.1 * i / (views - 1);
            const Pose pose = lookAt({9 * std::sin(angle), 1.5 * std::sin(0.7 * i), 9 * std::cos(angle)});
            db.images[i] = {"camera" + std::to_string(i), features};
            feats[i].width = 1280; feats[i].height = 960;
            feats[i].keypoints.assign(features, {-1000, -1000, 2, 0, 0});
            for (uint32_t p = 0; p < points; ++p) {
                const Vec3 pc = mul(pose.R, xyz[p]) + pose.t;
                const Vec2 px = K.project(pc);
                if (pc.z <= 0.1 || px.x <= 0 || px.x >= 1280 || px.y <= 0 || px.y >= 960) continue;
                feats[i].keypoints[p] = {(float)(px.x + noise(random)), (float)(px.y + noise(random)), 2, 0, 0};
                visible[i][p] = 1;
            }
        }
        for (uint32_t i = 0; i < views; ++i)
            for (uint32_t j = i + 1; j < views; ++j) {
                TwoViewMatches pair;
                pair.image1 = i; pair.image2 = j;
                pair.config = (int)TwoViewConfig::Uncalibrated;
                for (uint32_t p = 0; p < points; ++p)
                    if (visible[i][p] && visible[j][p]) pair.matches.push_back({p, p});
                if (pair.matches.size() >= 15) db.pairs.push_back(std::move(pair));
            }
    }
};

static MapperOptions cpuOptions() {
    MapperOptions opt;
    opt.sparse_image_features = true;
    opt.ba_real = opt.ba_real_coarse = "cpu";
    opt.threads = 1; opt.verbose = false; opt.report_progress = false;
    opt.focal = 1200; opt.refine_extra_params = false;
    opt.known_focal_cameras.insert(1); opt.given_focal_cameras.insert(1);
    opt.min_model_size = 2; opt.max_init_trials = 1; opt.max_num_models = 1;
    opt.host_budget_bytes = size_t{16} << 20;
    return opt;
}

static void checkModel(const Reconstruction& model, size_t points) {
    require(model.numRegistered() == Scene::views, "spill merge lost a registered image");
    require(model.points3D.size() == points, "spill merge lost a 3D point");
    std::set<uint32_t> ids;
    for (const auto& kv : model.images) {
        if (!kv.second.registered) continue;
        ids.insert(kv.first);
        require(kv.second.id == kv.first && kv.second.camera_id == 1, "spill merge changed global ids");
        require(kv.second.points2D.size() == Scene::features, "spill merge changed feature indexing");
        for (size_t f = 0; f < kv.second.point3D_ids.size(); ++f) {
            const uint64_t id = kv.second.point3D_ids[f];
            if (id == kInvalidPoint3D) continue;
            const auto point = model.points3D.find(id);
            require(point != model.points3D.end(), "image references a missing point after spill merge");
            size_t count = 0;
            for (const TrackElement& e : point->second.track)
                count += e.image_id == kv.first && e.point2D_idx == f;
            require(count == 1, "image-to-track ownership changed after spill merge");
        }
    }
    require(ids == std::set<uint32_t>({0, 1, 2, 3, 4, 5, 6, 7}), "spill merge lost global image coverage");
    for (const auto& kv : model.points3D) {
        require(kv.second.track.size() >= 2, "spill merge produced an unsupported point");
        for (const TrackElement& e : kv.second.track)
            require(model.images.at(e.image_id).point3D_ids.at(e.point2D_idx) == kv.first,
                    "track-to-image ownership changed after spill merge");
    }
}

static std::vector<StoredModel> storeCopies(ModelStore& store, const Reconstruction& model, size_t count) {
    std::vector<StoredModel> stored;
    for (size_t i = 0; i < count; ++i) stored.push_back(store.put(model));
    return stored;
}

static void testCompaction(const Scene& scene, const Reconstruction& model) {
    ModelStore store("");
    auto stored = storeCopies(store, model, 8);
    const size_t resident_limit = modelResidentBytes(model) * 2 + 4096;
    require(storedModelBytes(stored) > resident_limit, "fixture does not require spill compaction");
    BottomUpOptions bup;
    bup.model_memory_bytes = resident_limit * 3; bup.verbose = false;
    ManagerOptions manager;
    manager.merge.verbose = false; manager.merge.host_budget_bytes = resident_limit * 2;
    AssembleOptions assemble;
    BottomUpStats stats;
    Mapper mapper(scene.db, scene.feats, cpuOptions());
    auto compacted = detail::compactStoredModels(mapper, store, std::move(stored), bup,
                                                 manager, assemble, stats);
    require(stats.spill_rounds >= 2 && stats.assemble.merges >= 6, "bounded merge levels did not run");
    require(compacted.size() == 2, "unexpected compacted model count");
    require(stats.peak_loaded_model_bytes <= resident_limit, "compaction exceeded its resident limit");
    const Camera& shared = compacted.front().cameras.at(1);
    for (const Reconstruction& result : compacted) {
        checkModel(result, model.points3D.size());
        const Camera& camera = result.cameras.at(1);
        require(camera.fx == shared.fx && camera.fy == shared.fy &&
                camera.cx == shared.cx && camera.cy == shared.cy && camera.k1 == shared.k1 &&
                camera.pixel_scale == shared.pixel_scale, "spill levels lost shared camera calibration");
    }
    std::printf("  spill compaction: %zu levels, %zu merges, %u images / %zu points; loaded %zu / %zu bytes\n",
                stats.spill_rounds, stats.assemble.merges, Scene::views, model.points3D.size(),
                stats.peak_loaded_model_bytes, resident_limit);
}

static void testRefusalsAndFailures(const Scene& scene, const Reconstruction& model) {
    const size_t resident_limit = modelResidentBytes(model) * 2 + 4096;
    BottomUpOptions bup;
    bup.model_memory_bytes = resident_limit * 3; bup.verbose = false;
    ManagerOptions manager;
    manager.merge.verbose = false; manager.merge.host_budget_bytes = resident_limit * 2;
    AssembleOptions assemble;
    Reconstruction refused = model;
    refused.images.at(0).name = "different-source-name";
    {
        ModelStore store("");
        auto stored = storeCopies(store, model, 3);
        stored.push_back(store.put(refused));
        Mapper mapper(scene.db, scene.feats, cpuOptions());
        BottomUpStats stats;
        auto compacted = detail::compactStoredModels(mapper, store, std::move(stored), bup,
                                                     manager, assemble, stats);
        require(stats.assemble.merges_refused > 0 && compacted.size() == 2, "refused component disappeared");
        size_t retained = 0;
        for (const Reconstruction& result : compacted) {
            checkModel(result, model.points3D.size());
            retained += result.images.at(0).name == refused.images.at(0).name;
        }
        require(retained == 1, "refused source was silently dropped");
    }
    {
        ModelStore store("");
        Reconstruction another = refused;
        another.images.at(0).name = "third-source-name";
        std::vector<StoredModel> stored = {store.put(model), store.put(refused), store.put(another)};
        Mapper mapper(scene.db, scene.feats, cpuOptions());
        BottomUpStats stats;
        bool failed = false;
        try {
            detail::compactStoredModels(mapper, store, stored, bup, manager, assemble, stats);
        } catch (const std::runtime_error& e) {
            failed = std::string(e.what()).find("cannot reduce models") != std::string::npos;
        }
        require(failed && stats.assemble.merges_refused > 0, "non-progress did not fail explicitly");
        for (const StoredModel& meta : stored) checkModel(store.load(meta), model.points3D.size());
    }
    {
        ModelStore store("");
        auto stored = storeCopies(store, model, 2);
        bup.model_memory_bytes = (modelResidentBytes(model) - 1) * 3;
        Mapper mapper(scene.db, scene.feats, cpuOptions());
        BottomUpStats stats;
        bool failed = false;
        try {
            detail::compactStoredModels(mapper, store, stored, bup, manager, assemble, stats);
        } catch (const std::runtime_error& e) {
            failed = std::string(e.what()).find("one atom model exceeds") != std::string::npos;
        }
        require(failed && stats.peak_loaded_model_bytes == 0, "oversized model did not fail before loading");
        for (const StoredModel& meta : stored) checkModel(store.load(meta), model.points3D.size());
    }
    std::puts("  refused sources / non-progress / oversized model: PASS");
}

static int runTests(int, char**) {
    Scene scene;
    Mapper mapper(scene.db, scene.feats, cpuOptions());
    const auto models = mapper.run();
    require(models.size() == 1 && models.front().points3D.size() >= 100, "synthetic CPU fixture failed");
    checkModel(models.front(), models.front().points3D.size());
    testCompaction(scene, models.front());
    testRefusalsAndFailures(scene, models.front());
    std::puts("sfm_bottomup_memory_test: PASS");
    return 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, runTests); }
