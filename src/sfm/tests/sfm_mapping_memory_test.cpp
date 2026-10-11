#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/map/Mapper.h"
#include "sfm/map/MemoryPolicy.h"
#include "sfm/tests/TestMain.h"

static std::atomic<bool> count_allocations{false};
static std::atomic<size_t> allocated_bytes{0};

void* operator new(size_t bytes) {
    void* p = std::malloc(bytes ? bytes : 1);
    if (!p) throw std::bad_alloc();
    if (count_allocations.load(std::memory_order_relaxed))
        allocated_bytes.fetch_add(bytes, std::memory_order_relaxed);
    return p;
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

using namespace sfm;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void testMemoryPolicy() {
    constexpr size_t mib = size_t{1024} * 1024, gib = 1024 * mib;
    struct Case {
        size_t total_gib, available_gib;
        int requested_mb;
        size_t working_mb, graph_mb, bundle_mb;
    };
    const Case cases[] = {
        {32, 24, 0, 16384, 512, 8192},
        {32, 8, 0, 6144, 512, 3072},
        {32, 8, 2048, 2048, 256, 1024},
        {32, 1, 0, 768, 96, 384},
        {0, 0, 0, 2048, 256, 1024},
    };
    for (const Case& c : cases) {
        const MappingMemoryLimits limits =
            mappingMemoryLimits(c.total_gib * gib, c.available_gib * gib, c.requested_mb);
        require(limits.working_bytes == c.working_mb * mib &&
                limits.graph_bytes == c.graph_mb * mib &&
                limits.bundle_bytes == c.bundle_mb * mib,
                "mapping memory policy exceeded its expected budget");
    }
    require(availableBundleBudget(0) == 0, "unset bundle budget became a memory limit");
    std::puts("  mapping memory policy boundaries: PASS");
}

struct AllocationScope {
    AllocationScope() {
        allocated_bytes.store(0);
        count_allocations.store(true);
    }
    ~AllocationScope() { count_allocations.store(false); }
    size_t finish() {
        count_allocations.store(false);
        return allocated_bytes.load();
    }
};

static MapperOptions cpuOptions(bool sparse) {
    MapperOptions opt;
    opt.sparse_image_features = sparse;
    opt.ba_real = opt.ba_real_coarse = "cpu";
    opt.threads = 1;
    opt.verbose = false;
    opt.report_progress = false;
    opt.focal = 1200;
    opt.refine_extra_params = false;
    opt.known_focal_cameras.insert(1);
    opt.given_focal_cameras.insert(1);
    opt.min_model_size = 2;
    opt.max_init_trials = 1;
    opt.max_num_models = 1;
    return opt;
}

static void testCameraBootstrap() {
    const size_t images = 128, features = 8192;
    MatchesDatabase db;
    db.images.resize(images);
    std::vector<FeatureSet> feats(images);
    std::vector<uint32_t> camera_ids(images);
    for (size_t i = 0; i < images; i++) {
        db.images[i] = {"bootstrap" + std::to_string(i), (uint32_t)features};
        feats[i].width = i % 2 ? 1920 : 1280;
        feats[i].height = i % 2 ? 1080 : 960;
        feats[i].keypoints.resize(features);
        camera_ids[i] = (uint32_t)(1 + i % 2);
    }
    MapperOptions opt = cpuOptions(true);
    opt.known_focal_cameras.insert(2);
    opt.given_focal_cameras.insert(2);
    opt.initial_cameras[1] = Camera::defaultFor(1, 1280, 960, 1200);
    opt.initial_cameras[2] = Camera::defaultFor(2, 1920, 1080, 1700);
    Mapper mapper(db, feats, opt, camera_ids);
    AllocationScope allocations;
    mapper.bootstrapCameras();
    mapper.bootstrapCameras();
    const size_t bytes = allocations.finish();
    // Full image arrays alone would allocate 24 MiB for this fixture.
    require(bytes < 128 * 1024, "known-focal bootstrap allocated global feature arrays");
    require(mapper.startingCameras().size() == 2, "bootstrap lost a camera group");
    require(mapper.startingCameras().at(1).focal() == 1200 &&
            mapper.startingCameras().at(2).focal() == 1700,
            "bootstrap changed supplied focal lengths");
    require(mapper.cameraIds() == camera_ids, "bootstrap changed image camera groups");
    std::printf("  known-focal bootstrap: %zu allocated bytes for %zu features\n",
                bytes, images * features);

    opt.focal_trials = 0;
    opt.given_focal_cameras.clear();
    opt.known_focal_cameras.clear();
    Mapper disabled(db, feats, opt);
    AllocationScope off_allocations;
    disabled.bootstrapCameras();
    require(off_allocations.finish() < 128 * 1024,
            "disabled focal bootstrap allocated global feature arrays");
    require(disabled.cameraIds().size() == images, "default camera ids were not resolved");
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
    std::vector<Pose> truth;

    Scene() : feats(views), truth(views) {
        const Camera K = Camera::defaultFor(1, 1280, 960, 1200);
        std::mt19937 rng(11);
        std::uniform_real_distribution<double> coordinate(-2.5, 2.5);
        std::normal_distribution<double> noise(0.0, 0.2);
        std::vector<Vec3> xyz(points);
        for (Vec3& p : xyz) p = {coordinate(rng), coordinate(rng), coordinate(rng)};
        std::vector<std::vector<char>> visible(views, std::vector<char>(points));
        db.images.resize(views);
        for (uint32_t i = 0; i < views; i++) {
            const double angle = -1.05 + 2.1 * i / (views - 1);
            truth[i] = lookAt({9 * std::sin(angle), 1.5 * std::sin(0.7 * i),
                               9 * std::cos(angle)});
            db.images[i] = {"camera" + std::to_string(i), features};
            feats[i].width = 1280;
            feats[i].height = 960;
            feats[i].keypoints.assign(features, {-1000, -1000, 2, 0, 0});
            for (uint32_t p = 0; p < points; p++) {
                const Vec3 pc = mul(truth[i].R, xyz[p]) + truth[i].t;
                const Vec2 px = K.project(pc);
                if (pc.z <= 0.1 || px.x <= 0 || px.x >= 1280 || px.y <= 0 || px.y >= 960)
                    continue;
                feats[i].keypoints[p] = {(float)(px.x + noise(rng)),
                                         (float)(px.y + noise(rng)), 2, 0, 0};
                visible[i][p] = 1;
            }
        }
        for (uint32_t i = 0; i < views; i++)
            for (uint32_t j = i + 1; j < views; j++) {
                TwoViewMatches pair;
                pair.image1 = i;
                pair.image2 = j;
                pair.config = (int)TwoViewConfig::Uncalibrated;
                for (uint32_t p = 0; p < points; p++)
                    if (visible[i][p] && visible[j][p]) pair.matches.push_back({p, p});
                if (pair.matches.size() >= 15) db.pairs.push_back(std::move(pair));
            }
    }
};

static void checkModel(const Reconstruction& model, const Scene& scene, uint32_t registered) {
    require(model.numRegistered() == registered, "unexpected registered image count");
    require(model.images.size() == registered, "snapshot retained unregistered images");
    require(model.points3D.size() >= 100, "synthetic model lost its structure");
    size_t observations = 0;
    double reprojection = 0;
    for (const auto& kv : model.images) {
        const Image& image = kv.second;
        require(image.registered && image.id == kv.first, "invalid materialized image metadata");
        require(image.name == scene.db.images.at(kv.first).name, "image name changed");
        const FeatureSet& feats = scene.feats.at(kv.first);
        require(image.points2D.size() == feats.count() && image.point3D_ids.size() == feats.count(),
                "materialized image has incomplete feature arrays");
        for (size_t f = 0; f < feats.count(); f++) {
            require(image.points2D[f].x == feats.keypoints[f].x &&
                    image.points2D[f].y == feats.keypoints[f].y,
                    "materialization changed keypoint coordinates");
            const uint64_t id = image.point3D_ids[f];
            if (id == kInvalidPoint3D) continue;
            const auto point = model.points3D.find(id);
            require(point != model.points3D.end(), "image references a missing point");
            size_t count = 0;
            for (const TrackElement& e : point->second.track)
                count += e.image_id == kv.first && e.point2D_idx == f;
            require(count == 1, "image-to-track link is inconsistent");
        }
    }
    for (const auto& kv : model.points3D) {
        require(kv.second.track.size() >= 2, "point has fewer than two observations");
        for (const TrackElement& e : kv.second.track) {
            const Image& image = model.images.at(e.image_id);
            require(image.point3D_ids.at(e.point2D_idx) == kv.first,
                    "track-to-image link is inconsistent");
            const Vec2 predicted = model.cameras.at(image.camera_id).project(
                mul(image.pose.R, kv.second.xyz) + image.pose.t);
            const Vec2 observed = image.points2D.at(e.point2D_idx);
            const double error = std::hypot(predicted.x - observed.x, predicted.y - observed.y);
            require(std::isfinite(error), "non-finite reconstruction geometry");
            reprojection += error;
            observations++;
        }
    }
    require(observations && reprojection / observations < 1.0,
            "materialized model has excessive reprojection error");
}

static void checkEquivalent(const Reconstruction& dense, const Reconstruction& sparse) {
    require(dense.images.size() == sparse.images.size() &&
            dense.points3D.size() == sparse.points3D.size(), "sparse/dense topology differs");
    for (const auto& kv : dense.images) {
        const Image& other = sparse.images.at(kv.first);
        require(kv.second.point3D_ids == other.point3D_ids, "sparse/dense feature ownership differs");
        for (int j = 0; j < 9; j++)
            require(std::abs(kv.second.pose.R[j] - other.pose.R[j]) < 1e-10,
                    "sparse/dense rotations differ");
        require((kv.second.pose.t - other.pose.t).norm() < 1e-10,
                "sparse/dense translations differ");
    }
    for (const auto& kv : dense.points3D) {
        const Point3D& other = sparse.points3D.at(kv.first);
        require((kv.second.xyz - other.xyz).norm() < 1e-10 &&
                kv.second.track.size() == other.track.size(), "sparse/dense points differ");
        for (size_t j = 0; j < kv.second.track.size(); j++)
            require(kv.second.track[j].image_id == other.track[j].image_id &&
                    kv.second.track[j].point2D_idx == other.track[j].point2D_idx,
                    "sparse/dense tracks differ");
    }
}

static double modelError(const Reconstruction& model) {
    double squared = 0;
    for (const auto& kv : model.points3D)
        for (const TrackElement& e : kv.second.track) {
            const Image& image = model.images.at(e.image_id);
            const Vec2 p = model.cameras.at(image.camera_id).project(
                mul(image.pose.R, kv.second.xyz) + image.pose.t);
            const Vec2 q = image.points2D.at(e.point2D_idx);
            squared += (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
        }
    return squared;
}

static Reconstruction registeredSubset(const Reconstruction& model,
                                        const std::set<uint32_t>& ids) {
    Reconstruction subset = subsetModel(model, ids);
    for (auto it = subset.images.begin(); it != subset.images.end();)
        it = it->second.registered ? std::next(it) : subset.images.erase(it);
    return subset;
}

static void testIndexCapacityMapping(const Scene& scene, const Reconstruction& model) {
    Reconstruction left = registeredSubset(model, {0, 1, 2, 3});
    Reconstruction right = registeredSubset(model, {4, 5, 6, 7});
    BundleOptions bo;
    bo.real = RealCfg::CPU;
    bo.threads = 1;
    bo.refine_extra_params = false;
    const uint64_t left_need = buildBundle(left, bo).P.jc_total;
    const uint64_t right_need = buildBundle(right, bo).P.jc_total;
    bo.index_limit = std::max(left_need, right_need);
    std::vector<Reconstruction*> together = {&left, &right};
    bool rejected = false;
    try {
        runJointBA(together, bo);
    } catch (const BAIndexCapacity& e) {
        rejected = e.need_elements == left_need + right_need &&
                   e.limit_elements == bo.index_limit && e.pool == "Jc pool";
    }
    require(rejected, "joint fixture did not exceed its artificial index capacity");

    for (Reconstruction* rec : together)
        for (auto& kv : rec->points3D) {
            kv.second.xyz.x += 0.08;
            kv.second.xyz.y += 0.04;
        }
    const double left_before = modelError(left), right_before = modelError(right);
    Reconstruction inactive;
    inactive.cameras = model.cameras;
    std::vector<Reconstruction> components = {left, inactive, right};
    MapperOptions opt = cpuOptions(true);
    opt.ba_index_limit = bo.index_limit;
    Mapper mapper(scene.db, scene.feats, opt);
    mapper.jointRefine(components);
    checkModel(components[0], scene, 4);
    checkModel(components[2], scene, 4);
    require(modelError(components[0]) < 0.1 * left_before &&
            modelError(components[2]) < 0.1 * right_before,
            "capacity batching skipped a singleton component solve");
    require(components[0].cameras.at(1).fx == components[1].cameras.at(1).fx &&
            components[0].cameras.at(1).fx == components[2].cameras.at(1).fx,
            "capacity batching lost shared camera parameters");

    opt.ba_index_limit = 1;
    Mapper rejecting(scene.db, scene.feats, opt);
    Reconstruction single = components[0];
    const Reconstruction original = single;
    require(!rejecting.refineIfItFits(single, true), "oversized single-model refine was accepted");
    checkEquivalent(original, single);
    require(rejecting.options().ba_final_tight, "failed refinement left a temporary option active");
    rejected = false;
    try {
        rejecting.jointRefine(components);
    } catch (const BAIndexCapacity&) {
        rejected = true;
    }
    require(rejected, "singleton index overflow was silently skipped after batching");
    rejecting.options().ba_index_limit = UINT32_MAX;
    require(rejecting.refineIfItFits(single), "mapper could not retry after a capacity refusal");
    checkModel(single, scene, 4);
    std::puts("  index capacity: automatic CPU batches / singleton refusal / retry PASS");
}

static void testMapping() {
    Scene scene;
    Mapper dense(scene.db, scene.feats, cpuOptions(false));
    const std::vector<Reconstruction> dense_models = dense.run();
    require(dense_models.size() == 1, "dense fixture split into multiple models");
    checkModel(dense_models.front(), scene, Scene::views);

    Mapper sparse(scene.db, scene.feats, cpuOptions(true));
    sparse.bootstrapCameras();
    const std::vector<Reconstruction> sparse_models = sparse.run();
    require(sparse_models.size() == 1, "sparse fixture split into multiple models");
    checkModel(sparse_models.front(), scene, Scene::views);
    checkEquivalent(dense_models.front(), sparse_models.front());
    testIndexCapacityMapping(scene, sparse_models.front());
    std::printf("  sparse/dense CPU mapping: %u images, %zu points\n", Scene::views,
                sparse_models.front().points3D.size());

    Mapper growing(scene.db, scene.feats, cpuOptions(true));
    growing.restrictTo({0, 7});
    const std::vector<Reconstruction> seeds = growing.run();
    require(seeds.size() == 1, "seed fixture split into multiple models");
    checkModel(seeds.front(), scene, 2);
    growing.restrictTo({});
    Mapper::GrowStats pnp_stats;
    const Reconstruction partial = growing.growByPnP(seeds.front(), &pnp_stats, {}, 4);
    require(pnp_stats.before == 2 && pnp_stats.registered > 0 &&
            partial.numRegistered() > 2 && partial.numRegistered() <= 4,
            "PnP growth did not materialize new images");
    checkModel(partial, scene, partial.numRegistered());
    Mapper::GrowStats growth_stats;
    const Reconstruction grown = growing.continueFrom(partial, &growth_stats);
    require(growth_stats.registered > 0 && growth_stats.refined,
            "continuation did not expand the seed");
    checkModel(grown, scene, Scene::views);
    checkModel(seeds.front(), scene, 2);

    const Reconstruction refined = growing.refine(grown);
    checkModel(refined, scene, Scene::views);
    const Reconstruction subset = subsetModel(refined, {2, 5});
    const Reconstruction adopted = growing.refine(subset);
    checkModel(adopted, scene, 2);
    const Reconstruction repeated = growing.continueFrom(seeds.front());
    checkModel(repeated, scene, Scene::views);
    checkModel(adopted, scene, 2);
    std::printf("  seed / PnP / continuation / repeated adoption: PASS\n");
}

static int runTests(int, char**) {
    testMemoryPolicy();
    testCameraBootstrap();
    testMapping();
    std::puts("sfm_mapping_memory_test: PASS");
    return 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, runTests); }
