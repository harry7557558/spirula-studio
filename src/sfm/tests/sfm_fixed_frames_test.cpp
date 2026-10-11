#include "sfm/map/Bundle.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/TestMain.h"
#include "sfm/map/RegionalWindows.h"
#include <cstring>

using namespace sfm;
static void require(bool v, const char* message) { if (!v) throw std::runtime_error(message); }

static Reconstruction fixture(uint32_t seed) {
    auto p = synth::makeProblem(5, 8, 120, 1, 0., seed, 0);
    Reconstruction rec; Camera cam = Camera::defaultFor(7, 640, 480, 600, CamModel::Pinhole);
    unpackIntrinsics(cam, p.intr.data()); rec.cameras[7] = cam;
    for (uint32_t i = 0; i < p.num_images; ++i) {
        Image im; im.id = i; im.camera_id = 7; im.registered = true;
        im.pose = bundle_detail::unpackPose(&p.poses[6 * i]); rec.images[i] = im;
    }
    for (uint32_t k = 0; k < p.num_points; ++k) {
        Point3D point; point.xyz = {p.points[3 * k], p.points[3 * k + 1], p.points[3 * k + 2]};
        rec.points3D[k + 1000] = point;
    }
    for (uint32_t o = 0; o < p.num_obs; ++o) {
        auto& im = rec.images.at(p.obs_image[o]); uint32_t f = uint32_t(im.points2D.size());
        uint64_t id = p.obs_point[o] + 1000;
        im.points2D.push_back(cam.project(mul(im.pose.R, rec.points3D.at(id).xyz) + im.pose.t));
        im.point3D_ids.push_back(id); rec.points3D.at(id).track.push_back({im.id, f});
    }
    for (uint32_t i = 0; i < 4; ++i) rec.images.at(i).pose.t = rec.images.at(i).pose.t + Vec3{.02 * (i + 1), -.01, .015};
    for (uint64_t k = 1090; k < 1120; ++k) rec.points3D.at(k).xyz = rec.points3D.at(k).xyz + Vec3{.03, -.02, .04};
    return rec;
}

static void priorMask() {
    auto p = synth::makeProblem(5, 8, 20, 1, 0., 73, 0);
    p.fixed_frames.assign(p.num_frames, 0); p.fixed_frames[1] = 1;
    PosePriors pr; PriorCentre c; c.n = 2; c.img[0] = 0; c.img[1] = 1;
    c.A[1] = {-1, 0, 0, 0, -1, 0, 0, 0, -1}; c.b = {2, 3, 4}; pr.centres.push_back(c); p.priors = &pr;
    PriorAssembler a; a.init(p); require(a.assemble(p, p.poses.data(), p.exts.data(), .01) > 0, "fixed prior cost disappeared");
    double active = 0;
    for (uint32_t k = 0; k < p.pose_dim; ++k) {
        if (k / 6 == 1) require(a.gradient()[k] == 0, "fixed pose has prior gradient");
        else active += std::abs(a.gradient()[k]);
    }
    require(active > 0, "active prior gradient disappeared");
    for (size_t e = 0; e < a.cols().size(); ++e) if (a.cols()[e] == 1 || a.entryRow()[e] == 1)
        for (size_t k = 0; k < 36; ++k) require(a.blocks()[e * 36 + k] == 0, "fixed prior Hessian not zero");
    p.num_obs = 0; p.obs_image.clear(); p.obs_point.clear(); p.obs_xy.clear();
    p.obs_ranges.assign(p.num_points + 1, 0); finalizeTables(p);
    std::vector<double> reference_s, reference_g;
    for (RealCfg real : {RealCfg::CPU, RealCfg::F64}) {
        auto problem = p; SolverOptions opt; opt.real = real; opt.solver = SolverSel::Dense; opt.verbose = false;
        BundleSolver solver(problem, opt); solver.init(); solver.debugAssemble(.01f);
        auto s = solver.debugPackedS(); auto g = solver.debugG();
        for (size_t k = 6; k < 12; ++k) require(g[k] == 0, "solver prior moved fixed pose");
        if (real == RealCfg::CPU) { reference_s = s; reference_g = g; }
        else {
            auto check = [&](const auto& gpu, const auto& cpu) {
                double scale = 1, delta = 0;
                for (size_t k = 0; k < cpu.size(); ++k) { scale = std::max(scale, std::abs(cpu[k])); delta = std::max(delta, std::abs(cpu[k] - gpu[k])); }
                std::printf("masked prior parity: delta %.9g scale %.9g relative %.9g\n", delta, scale, delta / scale);
                require(delta / scale < 1e-7, "isolated masked prior CPU/Vulkan normal equations differ");
            };
            check(s, reference_s); check(g, reference_g);
        }
    }
    p.fixed_frames.resize(1); bool rejected = false;
    try { finalizeTables(p); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "invalid fixed-frame mask accepted");
}

static void rigMask() {
    auto rec = fixture(73); RigTable rigs; RigSpec spec; spec.members.resize(2);
    for (uint32_t f = 0; f < 4; ++f) spec.frames.push_back({2 * f, 2 * f + 1});
    rigs.rigs.push_back(spec); rigs.index(8); RigCalib c; c.resize(2); c.ref = 0;
    c.established[0] = c.established[1] = 1; c.fixed[0] = 1; c.cam_from_rig[1].t.x = .1;
    rec.rigs.push_back(c); std::set<uint32_t> support{4};
    BundleOptions opt; opt.rigs = &rigs; opt.fixed_images = &support; opt.rig_min_frames = 1; opt.rig_min_obs = 1;
    auto layout = buildBundle(rec, opt);
    require(layout.P.num_frames == 4 && layout.P.frameFixed(2), "rig frame not held by support image");
    require(layout.P.members[1].n_free == 0, "rig member moved a held support pose");
    auto original = rec;
    for (auto& p : layout.P.poses) p += .2;
    writeBundle(rec, layout, layout.P);
    for (uint32_t id : {4u, 5u}) require(std::memcmp(&rec.images.at(id).pose, &original.images.at(id).pose, sizeof(Pose)) == 0,
        "held rig frame changed in writeback");
}

static void sharedContext() {
    for (RealCfg real : {RealCfg::F64, RealCfg::F32}) {
        VkContext context; double baseline = 0;
        for (int run = 0; run < 3; ++run) {
            auto rec = fixture(73); BundleOptions opt; opt.real = real; opt.solver = "cg";
            opt.loss = "huber"; opt.max_iters = 8; opt.refine_intrinsics = false;
            if (run) opt.shared_ctx = &context;
            auto start = std::chrono::steady_clock::now();
            const double cost = runGlobalBA(rec, opt);
            std::printf("CONTEXT real %d reused %d seconds %.6f cost %.17g\n", int(real), run != 0,
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), cost);
            if (!run) baseline = cost;
            else require(std::abs(cost - baseline) <= std::max(1., baseline) * 1e-9, "reused GPU context changed solution");
            if (run == 1) {
                const double allocated = context.totalAllocatedMB();
                auto repeat = fixture(73); runGlobalBA(repeat, opt);
                require(context.totalAllocatedMB() == allocated, "reused context retained problem buffers");
            }
        }
    }
}

static int test(int argc, char** argv) {
    if (argc == 5) {
        auto rec = model_store_detail::readModel(argv[1]);
        auto shared = regional::readSharedIndex(argv[2], std::stoull(argv[4]), 256u << 20);
        auto fixed = shared.inject(std::stoull(argv[3]), rec);
        regional::WindowGraph graph(rec, 64u << 20);
        auto group = graph.select(graph.remaining(), 256, 72);
        auto reference_fixed = fixed; auto start = std::chrono::steady_clock::now();
        auto reference = regional::windowModel(rec, group.images, reference_fixed, 1);
        double serial = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        start = std::chrono::steady_clock::now(); auto input = regional::windowModel(rec, group.images, fixed, 0);
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        require(fixed == reference_fixed && reference.points3D.size() == input.points3D.size(), "parallel window selected different landmarks");
        for (const auto& p : reference.points3D) {
            const auto& other = input.points3D.at(p.first);
            require(p.second.track.size() == other.track.size() &&
                    std::memcmp(p.second.track.data(), other.track.data(), p.second.track.size() * sizeof(TrackElement)) == 0,
                    "parallel window changed observations");
        }
        std::printf("WINDOW images %zu points %zu serial %.6fs parallel %.6fs speedup %.3fx observations identical\n",
                    input.images.size(), input.points3D.size(), serial, elapsed, serial / elapsed);
        reference = Reconstruction{};
        rec = Reconstruction{}; shared = regional::SharedIndex{};
        uint64_t observations = 0; for (const auto& p : input.points3D) observations += p.second.track.size();
        VkContext context; double baseline = 0;
        for (int run = 0; run < 4; ++run) {
            auto local = input; BundleOptions opt; opt.real = RealCfg::F32; opt.solver = "cg";
            opt.loss = "huber"; opt.max_iters = 15; opt.fixed_points = &fixed; opt.fixed_images = &group.support;
            opt.shared_ctx = run >= 2 ? &context : nullptr;
            SolverStats stats; opt.stats = &stats; auto start = std::chrono::steady_clock::now();
            const double cost = runGlobalBA(local, opt);
            if (!run) baseline = cost;
            std::printf("REAL_CONTEXT images %zu points %zu observations %llu reused %d seconds %.6f iterations %d cost %.17g allocated_MB %.3f\n",
                input.images.size(), input.points3D.size(), (unsigned long long)observations, run >= 2,
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), stats.iterations, cost, context.totalAllocatedMB());
            require(std::abs(cost - baseline) <= std::max(1., baseline) * 1e-5, "real window reused context changed solution beyond F32 tolerance");
        }
        return 0;
    }
    priorMask(); rigMask(); sharedContext(); std::set<uint32_t> support{4, 5, 6, 7}; std::set<uint64_t> fixed;
    for (uint64_t k = 1000; k < 1090; ++k) fixed.insert(k);
    for (uint32_t seed : {23u, 73u}) for (RealCfg real : {RealCfg::CPU, RealCfg::F64, RealCfg::F32})
        for (const char* solver : {"cg", "dense"}) for (const char* loss : {"trivial", "huber"}) {
            auto original = fixture(seed); auto rec = original;
            BundleOptions opt; opt.real = real; opt.solver = solver; opt.loss = loss; opt.max_iters = 15;
            opt.refine_intrinsics = false; opt.fixed_points = &fixed; opt.fixed_images = &support;
            SolverStats stats; opt.stats = &stats;
            auto layout = buildBundle(rec, opt);
            require(std::count(layout.P.fixed_frames.begin(), layout.P.fixed_frames.end(), 1u) == 4, "support mask mismatched");
            double final = runGlobalBA(rec, opt);
            require(std::isfinite(final) && final < stats.initial_cost * .02, "active poses did not recover with fixed support");
            for (uint32_t id : support) require(std::memcmp(&rec.images.at(id).pose, &original.images.at(id).pose, sizeof(Pose)) == 0,
                "support pose changed in writeback");
            for (uint64_t id : fixed) require(std::memcmp(&rec.points3D.at(id).xyz, &original.points3D.at(id).xyz, sizeof(Vec3)) == 0,
                "fixed landmark changed");
            require((rec.images.at(0).pose.t - original.images.at(0).pose.t).norm() > .01, "core pose did not optimize");
            require((rec.points3D.at(1119).xyz - original.points3D.at(1119).xyz).norm() > .005, "free point did not optimize");
            std::printf("PASS seed %u real %d %s %s cost %.9g -> %.9g\n", seed, int(real), solver, loss, stats.initial_cost, final);
        }
    return 0;
}
int main(int argc, char** argv) { return sfmTestMain(argc, argv, test); }
