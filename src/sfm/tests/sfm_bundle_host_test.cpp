#include "sfm/map/Bundle.h"
#include "sfm/map/Mapper.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/TestMain.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> watch_allocations{false};
std::atomic<size_t> largest_allocation{0};
}

void* operator new(size_t bytes) {
    if (watch_allocations.load()) {
        size_t largest = largest_allocation.load();
        while (bytes > largest && !largest_allocation.compare_exchange_weak(largest, bytes)) {}
    }
    if (void* p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

namespace {

using namespace sfm;
int failures = 0;

void check(bool ok, const char* message) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", message);
    if (!ok) failures++;
}

struct Fixture {
    Reconstruction rec;
    RigTable rigs;
    PosePriors priors;
};

Fixture fixture(bool rig = false) {
    const BAProblem source = synth::makeProblem(2, 12, 200, 1, 0.1, 37, 3, rig ? 2 : 0);
    Fixture out;
    Camera camera = Camera::defaultFor(7, 640, 480);
    unpackIntrinsics(camera, source.intr.data());
    out.rec.cameras[camera.id] = camera;
    auto image_id = [](uint32_t i) { return 10 + 3 * i; };
    if (rig) {
        RigSpec spec;
        spec.members.resize(2);
        for (uint32_t f = 0; f < source.num_frames; f++)
            spec.frames.push_back({image_id(2 * f), image_id(2 * f + 1)});
        out.rigs.rigs.push_back(spec);
        out.rigs.index(image_id(source.num_images - 1) + 1);
        RigCalib calibration;
        calibration.resize(2);
        calibration.ref = 0;
        for (uint32_t m = 0; m < 2; m++) {
            calibration.cam_from_rig[m] = bundle_detail::unpackPose(&source.exts[6 * m]);
            calibration.established[m] = 1;
            calibration.support[m] = source.num_frames;
        }
        calibration.fixed[0] = 1;
        out.rec.rigs.push_back(calibration);
    }
    for (uint32_t i = 0; i < source.num_images; i++) {
        Image image;
        image.id = image_id(i);
        image.camera_id = camera.id;
        image.name = std::to_string(i) + ".jpg";
        image.registered = true;
        image.pose = bundle_detail::unpackPose(&source.poses[6 * source.image_frame[i]]);
        if (rig) image.pose = composePose(out.rec.rigs[0].cam_from_rig[source.image_member[i]],
                                         image.pose);
        out.rec.images[image.id] = image;
    }
    for (uint32_t p = 0; p < source.num_points; p++) {
        Point3D point;
        point.xyz = {source.points[3 * p], source.points[3 * p + 1], source.points[3 * p + 2]};
        for (uint32_t o = source.obs_ranges[p]; o < source.obs_ranges[p + 1]; o++) {
            Image& image = out.rec.images.at(image_id(source.obs_image[o]));
            point.track.push_back({image.id, (uint32_t)image.points2D.size()});
            image.points2D.push_back({source.obs_xy[2 * o], source.obs_xy[2 * o + 1]});
            image.point3D_ids.push_back(1000 + p);
        }
        std::reverse(point.track.begin(), point.track.end());
        out.rec.points3D[1000 + p] = point;
    }
    const Image& first = out.rec.images.begin()->second;
    PriorCentre centre;
    centre.n = 1;
    centre.img[0] = first.id;
    centre.b = cameraCenter(first.pose);
    centre.sigma = {10, 10, 10};
    out.priors.centres.push_back(centre);
    const Image& last = out.rec.images.rbegin()->second;
    PriorRotation rotation;
    rotation.i = first.id;
    rotation.j = last.id;
    rotation.R_ji = mul(last.pose.R, transpose(first.pose.R));
    out.priors.rotations.push_back(rotation);
    out.priors.ups.push_back({first.id, mul(first.pose.R, out.priors.up_w), 0.2});
    return out;
}

void check_layout(bool rig) {
    Fixture f = fixture(rig);
    Image missing;
    missing.id = 777;
    missing.points2D.push_back({99, 88});
    f.rec.images[missing.id] = missing;
    f.rec.points3D.begin()->second.track.push_back({missing.id, 0});
    BundleOptions options;
    options.real = RealCfg::CPU;
    options.priors = &f.priors;
    if (rig) options.rigs = &f.rigs;
    BundleLayout layout = buildBundle(f.rec, options);
    layout.attachPriors();
    const BAProblem& P = layout.P;
    bool equal = true;
    struct Observation { uint32_t image; double x, y; };
    size_t offset = 0;
    for (uint32_t p = 0; p < layout.ptOf.size(); p++) {
        std::vector<Observation> expected;
        for (const TrackElement& e : layout.ptOf[p]->track) {
            const auto it = std::find_if(layout.imgOf.begin(), layout.imgOf.end(),
                                        [&](const Image* image) { return image->id == e.image_id; });
            if (it == layout.imgOf.end()) continue;
            const Vec2 xy = (*it)->points2D[e.point2D_idx];
            expected.push_back({(uint32_t)(it - layout.imgOf.begin()), xy.x, xy.y});
        }
        std::sort(expected.begin(), expected.end(), [](const auto& a, const auto& b) {
            return a.image < b.image;
        });
        equal = equal && P.obs_ranges[p] == offset;
        for (const auto& obs : expected) {
            equal = equal && P.obs_image[offset] == obs.image && P.obs_point[offset] == p &&
                    P.obs_xy[2 * offset] == obs.x && P.obs_xy[2 * offset + 1] == obs.y;
            offset++;
        }
    }
    check(equal && P.num_obs == offset && P.obs_ranges.back() == offset,
          rig ? "rig observations preserve point/image ordering and coordinates"
              : "plain observations preserve point/image ordering and coordinates");
    check(layout.priors.centres.size() == 1 && layout.priors.centres[0].img[0] == 0 &&
          layout.priors.rotations.size() == 1 && layout.priors.ups.size() == 1 && P.priors,
          "GPS, rotation and up priors preserve remapped indices");
    if (rig) check(P.num_frames == 12 && P.num_images == 24 && P.members.size() == 2,
                   "rig frame/member layout remains intact");
    if (rig) {
        struct Case { CamModel model; size_t pp_min_images; uint32_t free_intr; };
        const Case cases[] = {{CamModel::Radial, 25, 3}, {CamModel::Radial, 20, 5},
                              {CamModel::OpenCV, 20, 8}, {CamModel::FullOpenCV, 20, 12}};
        for (const Case& c : cases) {
            f.rec.cameras.begin()->second.model = c.model;
            options.refine_principal_point = true;
            options.pp_min_images = c.pp_min_images;
            options.index_limit = UINT32_MAX;
            const BundleLayout counted = buildBundle(f.rec, options);
            const BAProblem& Q = counted.P;
            uint64_t expected = 0;
            for (uint32_t image : Q.obs_image)
                expected += 2ull * (6 + (Q.image_member[image] == 0 ? 0 : 6) + c.free_intr);
            check(Q.members[0].n_free == 0 && Q.members[1].n_free == 6 &&
                  Q.groups[0].n_intr == c.free_intr && Q.jc_total == expected,
                  "rig Jc count includes exact member/intrinsic freedom and principal-point gate");
            options.index_limit = expected - 1;
            largest_allocation = 0;
            watch_allocations = true;
            bool declined = false;
            try { (void)buildBundle(f.rec, options); }
            catch (const BAIndexCapacity& e) {
                declined = e.need_elements == expected && e.limit_elements == expected - 1;
            }
            watch_allocations = false;
            check(declined && largest_allocation < Q.num_obs * sizeof(uint32_t),
                  "rig index capacity refuses packing before large observation arrays");
        }
    }
}

void check_cpu_solve() {
    Fixture f = fixture();
    BundleOptions options;
    options.real = RealCfg::CPU;
    options.solver = "cg";
    options.threads = 2;
    options.max_iters = 12;
    options.host_budget_bytes = 128 << 20;
    options.priors = &f.priors;
    BundleLayout layout = buildBundle(f.rec, options);
    layout.attachPriors();
    const auto run = solveBundle(layout.P, bundleSolverOptions(options), nullptr);
    check(run.real == RealCfg::CPU && std::isfinite(run.stats.final_cost) &&
          run.stats.final_cost < run.stats.initial_cost * 0.1,
          "budgeted CPU solve converges with all observations and GPS priors");
    Fixture other = fixture();
    std::vector<Reconstruction*> models{&f.rec, &other.rec};
    std::vector<const PosePriors*> priors{&f.priors, &other.priors};
    const double joint = runJointBA(models, options, &priors);
    check(std::isfinite(joint) && joint > 0, "budgeted joint CPU solve completes");
}

void check_rejections() {
    Fixture f = fixture();
    BundleOptions options;
    options.real = RealCfg::CPU;
    options.host_budget_bytes = 1;
    bool refused = false;
    try { (void)buildBundle(f.rec, options); }
    catch (const BAOverBudget& e) {
        refused = e.need_mb > e.budget_mb && std::string(e.what()).find("host") != std::string::npos;
    }
    check(refused, "tiny host budget refuses a single model before BA packing");

    Fixture other = fixture();
    std::vector<Reconstruction*> models{&f.rec, &other.rec};
    largest_allocation = 0;
    watch_allocations = true;
    refused = false;
    try { (void)runJointBA(models, options); }
    catch (const BAOverBudget&) { refused = true; }
    watch_allocations = false;
    check(refused && largest_allocation.load() < 1024,
          "joint preflight refuses before image/point arrays are copied");

    BAProblem problem = synth::makeProblem(2, 12, 200, 1, 0.1, 5);
    const auto poses = problem.poses, points = problem.points;
    SolverOptions solver;
    solver.real = RealCfg::CPU;
    solver.host_budget_bytes = 1;
    solver.verbose = false;
    solver.solver = SolverSel::CG;
    refused = false;
    try { bacpu::Solver cpu(problem, solver); cpu.init(); }
    catch (const BAOverBudget&) { refused = true; }
    check(refused && poses == problem.poses && points == problem.points,
          "explicit CPU host budget refuses even without over_budget_throws");

    Image sparse;
    sparse.id = UINT32_MAX - 1;
    f.rec.images[sparse.id] = sparse;
    options.host_budget_bytes = 8 << 20;
    largest_allocation = 0;
    watch_allocations = true;
    refused = false;
    try { (void)buildBundle(f.rec, options); }
    catch (const BAOverBudget&) { refused = true; }
    watch_allocations = false;
    check(refused && largest_allocation.load() < 1024,
          "sparse image-id lookup size is checked before allocation");
}

void check_sparse_rig_rejection() {
    Fixture first = fixture(true), second = fixture(true);
    std::vector<Reconstruction*> models{&first.rec, &second.rec};
    BundleOptions options;
    options.real = RealCfg::CPU;
    options.solver = "cg";
    options.rigs = &first.rigs;
    const size_t old_span = first.rigs.of_image.size();
    const size_t before = estimateJointBundleHostBytes(models, options);
    first.rigs.of_image.resize(size_t{1} << 18);
    const size_t after = estimateJointBundleHostBytes(models, options);
    const size_t added = (sizeof(RigSlot) + sizeof(uint32_t)) * models.size() *
                         (first.rigs.of_image.size() - old_span);
    check(after >= before + added,
          "joint host estimate includes shifted rig lookup and image-id span");
    options.host_budget_bytes = 4 << 20;
    largest_allocation = 0;
    watch_allocations = true;
    bool refused = false;
    try { (void)runJointBA(models, options); }
    catch (const BAOverBudget&) { refused = true; }
    watch_allocations = false;
    check(refused && largest_allocation.load() < 1024,
          "sparse rig lookup is refused before copying joint models or indexing rigs");
}

void check_empty_sparse_layout() {
    Fixture f = fixture();
    Image sparse;
    sparse.id = UINT32_MAX - 1;
    f.rec.images[sparse.id] = sparse;
    f.rec.points3D.clear();
    BundleOptions options;
    options.host_budget_bytes = 1;
    largest_allocation = 0;
    watch_allocations = true;
    const BundleLayout empty = buildBundle(f.rec, options);
    watch_allocations = false;
    check(empty.P.num_images == 0 && largest_allocation.load() < 1024,
          "empty sparse-id problem returns before allocating the image lookup");
    f = fixture();
    f.rec.images[sparse.id] = sparse;
    for (auto& image : f.rec.images) image.second.registered = false;
    largest_allocation = 0;
    watch_allocations = true;
    const BundleLayout unregistered = buildBundle(f.rec, options);
    watch_allocations = false;
    check(unregistered.P.num_images == 0 && largest_allocation.load() < 1024,
          "unregistered sparse-id problem returns before allocating the image lookup");
}

void check_mapper_joint_rejection() {
    Fixture first = fixture(), second = fixture();
    const size_t image_count = first.rec.images.rbegin()->first + 1;
    MatchesDatabase db;
    db.images.resize(image_count);
    std::vector<FeatureSet> features(image_count);
    for (size_t i = 0; i < image_count; i++) {
        features[i].width = 640;
        features[i].height = 480;
        db.images[i].name = std::to_string(i) + ".jpg";
        auto image = first.rec.images.find((uint32_t)i);
        if (image == first.rec.images.end()) continue;
        features[i].keypoints.resize(image->second.points2D.size());
        db.images[i].num_features = features[i].count();
    }
    MapperOptions options;
    options.host_budget_bytes = 1;
    options.ba_real = options.ba_real_coarse = "cpu";
    options.ba_solver = "cg";
    options.verbose = false;
    options.threads = 2;
    options.initial_cameras = first.rec.cameras;
    Mapper mapper(db, features, options, std::vector<uint32_t>(image_count, 7));
    std::vector<Reconstruction> models;
    models.push_back(std::move(first.rec));
    models.push_back(std::move(second.rec));
    bool refused = false;
    try { mapper.jointRefine(models); }
    catch (const BAOverBudget& e) { refused = e.need_mb > e.budget_mb; }
    check(refused, "Mapper joint split refuses a tiny budget instead of skipping singletons");
}

int cmdTest(int, char**) {
    check_layout(false);
    check_layout(true);
    check_cpu_solve();
    check_rejections();
    check_sparse_rig_rejection();
    check_empty_sparse_layout();
    check_mapper_joint_rejection();
    if (!failures) std::puts("sfm_bundle_host_test: OK");
    return failures ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdTest); }
