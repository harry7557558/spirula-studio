#include "sfm/map/Bundle.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/TestMain.h"
#include <cstring>

using namespace sfm;
static void require(bool v, const char* message) { if (!v) throw std::runtime_error(message); }

static Reconstruction fixture() {
    auto source = synth::makeProblem(5, 8, 120, 1, 0., 73, 0);
    Reconstruction rec; Camera cam = Camera::defaultFor(7, 640, 480, 600, CamModel::Pinhole);
    unpackIntrinsics(cam, source.intr.data()); rec.cameras[7] = cam;
    for (uint32_t i = 0; i < source.num_images; ++i) {
        Image im; im.id = i; im.camera_id = 7; im.registered = true;
        im.pose = bundle_detail::unpackPose(&source.poses[6 * i]);
        im.pose.t = im.pose.t + Vec3{.02 * (i + 1), -.01, .015}; rec.images[i] = im;
    }
    for (uint32_t p = 0; p < source.num_points; ++p) {
        Point3D point; point.xyz = {source.points[3 * p], source.points[3 * p + 1], source.points[3 * p + 2]};
        if (p >= 90) point.xyz = point.xyz + Vec3{.03, -.02, .04}; rec.points3D[p + 1000] = point;
    }
    for (uint32_t o = 0; o < source.num_obs; ++o) {
        auto& im = rec.images.at(source.obs_image[o]); uint32_t f = uint32_t(im.points2D.size());
        uint64_t id = source.obs_point[o] + 1000;
        auto pose = bundle_detail::unpackPose(&source.poses[6 * im.id]); uint32_t p = source.obs_point[o];
        Vec3 xyz{source.points[3 * p], source.points[3 * p + 1], source.points[3 * p + 2]};
        im.points2D.push_back(cam.project(mul(pose.R, xyz) + pose.t)); im.point3D_ids.push_back(id);
        rec.points3D.at(id).track.push_back({im.id, f});
    }
    return rec;
}

static int test(int, char**) {
    auto original = fixture(); std::set<uint64_t> fixed;
    for (uint64_t p = 1000; p < 1090; ++p) fixed.insert(p);
    auto& single = original.points3D.at(1000);
    for (size_t i = 1; i < single.track.size(); ++i) {
        auto e = single.track[i]; original.images.at(e.image_id).point3D_ids.at(e.point2D_idx) = kInvalidPoint3D;
    }
    single.track.resize(1);
    for (RealCfg real : {RealCfg::CPU, RealCfg::F64, RealCfg::F32}) {
        bool cpu = real == RealCfg::CPU;
        auto rec = original; BundleOptions opt; opt.real = real; opt.solver = "cg";
        opt.refine_intrinsics = false; opt.max_iters = 20; opt.fixed_points = &fixed; SolverStats stats; opt.stats = &stats;
        auto built = buildBundle(rec, opt);
        require(built.P.num_points == 120 && built.P.fixed_points.size() == 120, "fixed single-view landmark was discarded");
        require(std::count(built.P.fixed_points.begin(), built.P.fixed_points.end(), 1u) == 90, "fixed point mask mismatched local IDs");
        double final = runGlobalBA(rec, opt);
        std::printf("%s cost %.9g -> %.9g\n", cpu ? "CPU" : "Vulkan", stats.initial_cost, final);
        require(std::isfinite(final) && final < stats.initial_cost * .02, "fixed landmark BA did not recover poses");
        for (uint64_t id : fixed) require(std::memcmp(&rec.points3D.at(id).xyz, &original.points3D.at(id).xyz, sizeof(Vec3)) == 0,
            "fixed landmark changed during BA");
        require((rec.points3D.at(1119).xyz - original.points3D.at(1119).xyz).norm() > .005, "free landmark did not optimize");
        std::printf("PASS %s fixed landmarks: %.9g -> %.9g\n", cpu ? "CPU" : real == RealCfg::F64 ? "Vulkan F64" : "Vulkan F32", stats.initial_cost, final);
    }
    return 0;
}
int main(int argc, char** argv) { return sfmTestMain(argc, argv, test); }
