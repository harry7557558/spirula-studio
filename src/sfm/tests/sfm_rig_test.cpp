// Rig support: the GPU bundle adjustment against the host one on problems
// whose images share frames and refine member extrinsics, then the mapper on
// a synthetic two-lens rig (docs/notes/sfm-rig-constraints.md).
//
//   sfm_rig_test [--device N] [--real double|df|float]
//
// Prints PASS/FAIL per case and returns 0/1. See docs/testing.md.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Rig.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/TestMain.h"

namespace {

int g_fail = 0;

void report(const char* name, double err, double tol) {
    const bool ok = err < tol && std::isfinite(err);
    printf("%-44s err %.3e (tol %.0e)  %s\n", name, err, tol, ok ? "PASS" : "FAIL");
    if (!ok) g_fail++;
}

double relMax(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0, s = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        d = std::max(d, std::fabs(a[i] - b[i]));
        s = std::max(s, std::fabs(b[i]));
    }
    return d / std::max(s, 1e-300);
}

SolverOptions baseOptions(RealCfg real, int device, bool cg) {
    SolverOptions o;
    o.real = real;
    o.device = device;
    o.loss = "huber";
    o.loss_param = 1.5f;
    o.init_damping = 1e-2;
    o.verbose = false;
    o.solver = cg ? SolverSel::CG : SolverSel::Dense;
    o.cg_tol = 1e-10;
    o.cg_max_iters = 3000;
    o.cg_fallback = CgFallback::Off;
    return o;
}

// One assembly and one step, device against host, then the whole solve.
void testParity(uint32_t model, uint32_t rig, bool rig_free, bool cg, RealCfg real, int device,
                double tol) {
    const BAProblem base =
        synth::makeProblem(model, 9, 120, 1, 0.2, 40 + model + 3 * rig, -1, rig, rig_free);
    char name[96];
    const char* path = cg ? "cg" : "dense";

    // Assembled S and g (dense path only: the CG path never forms S).
    if (!cg) {
        BAProblem Pg = base, Pc = base;
        SolverOptions og = baseOptions(real, device, false), oc = og;
        oc.real = RealCfg::CPU;
        og.max_iters = oc.max_iters = 1;
        BundleSolver sg(Pg, og);
        sg.init();
        sg.debugAssemble(1e-2f);
        BundleSolver sc(Pc, oc);
        sc.init();
        sc.debugAssemble(1e-2f);
        snprintf(name, sizeof name, "S   model=%u rig=%u%s", model, rig,
                 rig && !rig_free ? " held" : "");
        report(name, relMax(sg.debugPackedS(), sc.debugPackedS()), tol);
        snprintf(name, sizeof name, "g   model=%u rig=%u%s", model, rig,
                 rig && !rig_free ? " held" : "");
        report(name, relMax(sg.debugG(), sc.debugG()), tol);
    }

    // The full solve: same descent, same answer on frames and extrinsics.
    BAProblem Pg = base, Pc = base;
    SolverOptions og = baseOptions(real, device, cg), oc = og;
    oc.real = RealCfg::CPU;
    og.max_iters = oc.max_iters = 12;
    BundleSolver sg(Pg, og);
    sg.init();
    sg.solve();
    sg.downloadParams();
    BundleSolver sc(Pc, oc);
    sc.init();
    sc.solve();
    const double c0 = sg.stats().initial_cost, c1 = sg.stats().final_cost;
    snprintf(name, sizeof name, "%s descent model=%u rig=%u", path, model, rig);
    report(name, c1 < c0 ? 0.0 : 1.0, 0.5);
    snprintf(name, sizeof name, "%s cost model=%u rig=%u", path, model, rig);
    report(name, std::fabs(c1 - sc.stats().final_cost) / std::max(c1, 1e-300), tol);
    snprintf(name, sizeof name, "%s frames model=%u rig=%u", path, model, rig);
    report(name, relMax(Pg.poses, Pc.poses), tol);
    if (Pg.ext_dim) {
        snprintf(name, sizeof name, "%s extrinsics model=%u rig=%u", path, model, rig);
        report(name, relMax(Pg.exts, Pc.exts), tol);
    }
}

// ---- the mapper on a rig ---------------------------------------------------

sfm::Pose lookAt(const sfm::Vec3& C, const sfm::Vec3& target) {
    using namespace sfm;
    Vec3 f = (target - C).normalized();
    Vec3 up0 = {0, 1, 0};
    Vec3 r = up0.cross(f).normalized();
    Vec3 u = f.cross(r);
    Mat3 R = {r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z};
    Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

// Two lenses on an arc: cam0 looks at the origin, cam1 sits on it turned
// 22 deg and offset; the last frames' cam1 sees nothing (a lens on the sky),
// so only the rig can place them.
struct RigScene {
    int W = 1280, H = 960, M = 10, N = 260, blind_from = 7;
    sfm::Pose ext;                     // cam1_from_cam0, the truth
    std::vector<sfm::Pose> gt;         // by image id: cam0 = f, cam1 = M + f
    std::vector<sfm::Vec3> pts;
    std::vector<sfm::FeatureSet> feats;
    sfm::MatchesDatabase db;
    std::vector<uint32_t> cam_ids;
    std::vector<std::string> names;
};

RigScene makeRigScene() {
    using namespace sfm;
    RigScene sc;
    Camera K = Camera::defaultFor(1, sc.W, sc.H, 1200);
    std::mt19937 rng(23);
    std::uniform_real_distribution<double> ub(-4.0, 4.0);
    std::normal_distribution<double> noise(0.0, 0.3);
    sc.pts.resize(sc.N);
    for (Vec3& p : sc.pts) p = {ub(rng), ub(rng), ub(rng)};
    sc.ext.R = angleAxisToRotation({0.05, 22.0 * M_PI / 180.0, -0.03});
    sc.ext.t = {0.3, 0.05, -0.1};
    const int n = 2 * sc.M;
    sc.gt.resize(n);
    sc.feats.resize(n);
    sc.cam_ids.resize(n);
    sc.names.resize(n);
    std::vector<std::vector<char>> vis(n, std::vector<char>(sc.N, 0));
    for (int f = 0; f < sc.M; f++) {
        const double ang = -1.2 + 2.4 * f / (sc.M - 1);
        sc.gt[f] = lookAt({9 * std::sin(ang), 1.5 * std::sin(0.7 * f), 9 * std::cos(ang)},
                          {0, 0, 0});
        sc.gt[sc.M + f] = composePose(sc.ext, sc.gt[f]);
    }
    for (int i = 0; i < n; i++) {
        const bool blind = i >= sc.M && i - sc.M >= sc.blind_from;
        char nm[32];
        snprintf(nm, sizeof nm, "%s/%03d", i < sc.M ? "cam0" : "cam1", i < sc.M ? i : i - sc.M);
        sc.names[i] = nm;
        sc.cam_ids[i] = i < sc.M ? 1 : 2;
        FeatureSet& fs = sc.feats[i];
        fs.width = sc.W;
        fs.height = sc.H;
        fs.keypoints.resize(sc.N);
        for (int p = 0; p < sc.N; p++) {
            Vec3 pc = mul(sc.gt[i].R, sc.pts[p]) + sc.gt[i].t;
            Vec2 px = K.project(pc);
            if (!blind && pc.z > 0.1 && px.x > 0 && px.x < sc.W && px.y > 0 && px.y < sc.H) {
                fs.keypoints[p] = {(float)(px.x + noise(rng)), (float)(px.y + noise(rng)), 2, 0, 0};
                vis[i][p] = 1;
            } else {
                fs.keypoints[p] = {-1000, -1000, 2, 0, 0};
            }
        }
    }
    sc.db.images.resize(n);
    for (int i = 0; i < n; i++) sc.db.images[i] = {sc.names[i], (uint32_t)sc.N};
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            for (int p = 0; p < sc.N; p++)
                if (vis[i][p] && vis[j][p]) tv.matches.push_back({(uint32_t)p, (uint32_t)p, 0});
            if (tv.matches.size() >= 15) sc.db.pairs.push_back(std::move(tv));
        }
    return sc;
}

void testMapperRig(int device) {
    using namespace sfm;
    RigScene sc = makeRigScene();
    RigTable rigs = buildRigTable(sc.names, {RigDef{"rig", {{"cam0"}, {"cam1"}}}});
    MapperOptions opt;
    opt.verbose = false;
    opt.focal = 1200;
    opt.device = device;
    Mapper mapper(sc.db, sc.feats, opt, sc.cam_ids, &rigs);
    std::vector<Reconstruction> models = mapper.run();
    const Reconstruction& rec = models.front();
    const int n = 2 * sc.M;
    printf("mapper on a rig: %u/%d images, %zu points, %zu model(s)\n", rec.numRegistered(), n,
           rec.points3D.size(), models.size());
    report("rig: every image registered", rec.numRegistered() == (uint32_t)n ? 0.0 : 1.0, 0.5);
    report("rig: one model", models.size() == 1 ? 0.0 : 1.0, 0.5);
    size_t blind_reg = 0;
    for (int f = sc.blind_from; f < sc.M; f++) {
        auto it = rec.images.find(sc.M + f);
        if (it != rec.images.end() && it->second.registered) blind_reg++;
    }
    report("rig: blind lenses placed by the rig",
           blind_reg == (size_t)(sc.M - sc.blind_from) ? 0.0 : 1.0, 0.5);

    // The calibration: rotation against the truth, and rigidity across frames.
    double rot_err = 180, rigid = 0;
    if (rec.rigs.size() == 1 && rec.rigs[0].ref >= 0 && rec.rigs[0].usable(1 - rec.rigs[0].ref)) {
        const RigCalib& c = rec.rigs[0];
        const Pose rel01 = relativePose(c.cam_from_rig[0], c.cam_from_rig[1]);
        rot_err = rotationAngleDeg(mul(rel01.R, transpose(sc.ext.R)));
        for (int f = 0; f < sc.M; f++) {
            auto a = rec.images.find(f), b = rec.images.find(sc.M + f);
            if (a == rec.images.end() || b == rec.images.end()) continue;
            if (!a->second.registered || !b->second.registered) continue;
            const Pose r = relativePose(a->second.pose, b->second.pose);
            rigid = std::max(rigid, rotationAngleDeg(mul(r.R, transpose(rel01.R))));
        }
    }
    printf("  cam1_from_cam0: rotation error %.3f deg, worst frame deviation %.4f deg\n",
           rot_err, rigid);
    report("rig: extrinsic rotation", rot_err, 0.2);
    report("rig: rigid across frames", rigid, 1e-3);

    // Pose accuracy of every image against the truth, through one similarity.
    std::vector<Pose> src, dst;
    for (int i = 0; i < n; i++) {
        auto it = rec.images.find(i);
        if (it == rec.images.end() || !it->second.registered) continue;
        src.push_back(it->second.pose);
        dst.push_back(sc.gt[i]);
    }
    Sim3 T;
    double worst = 180;
    if (estimateSim3FromPoses(src, dst, T)) {
        worst = 0;
        for (size_t k = 0; k < src.size(); k++) {
            const Pose p = transformPose(T, src[k]);
            worst = std::max(worst, rotationAngleDeg(mul(p.R, transpose(dst[k].R))));
        }
    }
    printf("  worst absolute rotation error %.3f deg over %zu images\n", worst, src.size());
    report("rig: poses against the truth", worst, 0.3);

    // The same capture with the rig ignored leaves the blind lenses out.
    MapperOptions plain = opt;
    plain.use_rigs = false;
    Mapper flat(sc.db, sc.feats, plain, sc.cam_ids, &rigs);
    std::vector<Reconstruction> pm = flat.run();
    printf("  without the rig: %u/%d images\n", pm.front().numRegistered(), n);
    report("rig: the rig adds coverage",
           pm.front().numRegistered() < rec.numRegistered() ? 0.0 : 1.0, 0.5);

    // The final free refinement keeps every image and stays near the rig.
    Reconstruction freed = mapper.releaseRigs(rec);
    double drift = 0;
    for (int f = 0; f < sc.blind_from; f++) {
        auto a = freed.images.find(f), b = freed.images.find(sc.M + f);
        if (a == freed.images.end() || b == freed.images.end()) continue;
        if (!a->second.registered || !b->second.registered) continue;
        const Pose r = relativePose(a->second.pose, b->second.pose);
        drift = std::max(drift, rotationAngleDeg(mul(r.R, transpose(sc.ext.R))));
    }
    printf("  after releasing the rig: %u images, relative pose within %.3f deg of the truth\n",
           freed.numRegistered(), drift);
    report("rig: released poses stay put", drift, 0.5);
}

// Two models that share no image -- one lens each -- align through the rig.
void testRigAlignment() {
    using namespace sfm;
    RigScene sc = makeRigScene();
    sc.blind_from = sc.M;
    RigTable rigs = buildRigTable(sc.names, {RigDef{"rig", {{"cam0"}, {"cam1"}}}});
    Camera K = Camera::defaultFor(1, sc.W, sc.H, 1200);
    auto build = [&](int member, const Sim3& gauge) {
        Reconstruction m;
        m.cameras[1] = K;
        for (int f = 0; f < sc.M; f++) {
            const int id = member * sc.M + f;
            Image im;
            im.id = id;
            im.camera_id = 1;
            im.name = sc.names[id];
            im.registered = true;
            im.pose = transformPose(gauge, sc.gt[id]);
            im.points2D.resize(sc.N);
            im.point3D_ids.assign(sc.N, kInvalidPoint3D);
            for (int p = 0; p < sc.N; p++) {
                const Keypoint& k = sc.feats[id].keypoints[p];
                im.points2D[p] = {k.x, k.y};
            }
            m.images[id] = im;
        }
        for (int p = 0; p < sc.N; p++) {
            std::vector<TrackElement> track;
            for (int f = 0; f < sc.M; f++)
                if (sc.feats[member * sc.M + f].keypoints[p].x > 0)
                    track.push_back({(uint32_t)(member * sc.M + f), (uint32_t)p});
            if (track.size() >= 2) m.addPoint3D(transformPoint(gauge, sc.pts[p]), track);
        }
        RigCalib c;
        c.resize(2);
        c.ref = 0;
        c.cam_from_rig[1] = sc.ext;
        c.cam_from_rig[1].t = c.cam_from_rig[1].t * gauge.scale;
        c.established[0] = c.established[1] = 1;
        m.rigs.push_back(c);
        return m;
    };
    Sim3 g;
    g.scale = 0.6;
    g.R = angleAxisToRotation({0.2, -0.4, 0.1});
    g.t = {1.0, -2.0, 0.5};
    Reconstruction A = build(0, Sim3{});
    Reconstruction B = build(1, g);
    MergeOptions mo;
    mo.verbose = false;
    mo.rigs = &rigs;
    AlignmentResult al = alignReconstructions(B, A, mo);
    printf("rig alignment: %s, %zu correspondence(s) (%zu through the rig), %zu inliers\n",
           al.success ? "aligned" : al.reason.c_str(), al.common_images, al.rig_views,
           al.inliers);
    report("rig: models sharing no image align", al.success ? 0.0 : 1.0, 0.5);
    if (al.success) {
        const Sim3 want = invertSim3(g);
        const double rot = rotationAngleDeg(mul(al.transform.R, transpose(want.R)));
        report("rig: alignment rotation", rot, 0.2);
        report("rig: alignment scale", std::fabs(al.transform.scale / want.scale - 1.0), 0.02);
    }
    MergeSession session({A, B}, mo);
    std::vector<MergeCandidate> cands = session.candidates();
    report("rig: shared frames make a merge candidate",
           !cands.empty() && cands[0].common_images == (size_t)sc.M ? 0.0 : 1.0, 0.5);
    const MergeAttempt at = session.tryMerge(0, 1);
    printf("  merge: %s\n", at.merged ? "ok" : at.reason.c_str());
    report("rig: the merge goes through", at.merged ? 0.0 : 1.0, 0.5);
    if (at.merged) {
        const Reconstruction& M0 = session.model(0);
        double worst = 0;
        for (int f = 0; f < sc.M; f++) {
            const Pose r = relativePose(M0.images.at(f).pose, M0.images.at(sc.M + f).pose);
            worst = std::max(worst, rotationAngleDeg(mul(r.R, transpose(sc.ext.R))));
        }
        report("rig: merged frames keep the rig", worst, 0.2);
    }
}

int run(int argc, char** argv) {
    int device = -1;
    RealCfg real = RealCfg::F64;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--device" && i + 1 < argc) device = std::stoi(argv[++i]);
        else if (std::string(argv[i]) == "--real" && i + 1 < argc) real = realCfgFromName(argv[++i]);
    }
    const VkDeviceCaps caps = VkContext::probeCaps(device);
    const RealCfg got = pickRealForDevice(real, caps);
    if (got != real)
        printf("  note: '%s' is not supported here; running '%s'\n", realCfgName(real),
               realCfgName(got));
    // fp64 kernels and the host agree on S and g to ~1e-7, not to rounding
    // (measured the same on rig-free problems before rigs existed); the
    // emulated pair and fp32 leave more.
    const double tol = got == RealCfg::F64 ? 1e-5 : got == RealCfg::DF64 ? 1e-4 : 1e-3;

    for (uint32_t rig : {2u, 3u}) {
        testParity(3, rig, true, false, got, device, tol);
        testParity(3, rig, false, false, got, device, tol);
        testParity(7, rig, true, false, got, device, tol);   // full_opencv: dof 24
        testParity(6, rig, true, true, got, device, tol);
        testParity(3, rig, true, true, got, device, tol);
    }
    // The rig-free problem still takes the plain kernels.
    testParity(3, 0, true, false, got, device, tol);
    testParity(3, 0, true, true, got, device, tol);

    testRigAlignment();
    testMapperRig(device);

    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
