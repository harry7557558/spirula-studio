// Rig support: the GPU bundle adjustment against the host one on problems
// whose images share frames and refine member extrinsics, then the mapper on
// a synthetic two-lens rig (docs/notes/sfm-rig-constraints.md).
//
//   sfm_rig_test [--device N] [--real double|df|float]
//
// Prints PASS/FAIL per case and returns 0/1. See docs/testing.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Rig.h"
#include "sfm/feature/Pairing.h"
#include "sfm/feature/RigPairs.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/SyntheticRig.h"
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
    o.cg_model_tol = 0;
    o.cg_max_iters = 3000;
    o.cg_fallback = CgFallback::Off;
    return o;
}

// One assembly and one step, device against host, then the whole solve.
void testParity(uint32_t model, uint32_t rig, bool rig_free, bool cg, RealCfg real, int device,
                double tol, uint32_t rig_mask = kExtAll) {
    const BAProblem base = synth::makeProblem(model, 9, 120, 1, 0.2, 40 + model + 3 * rig, -1,
                                              rig, rig_free, rig_mask);
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
        snprintf(name, sizeof name, "S   model=%u rig=%u%s mask=%x", model, rig,
                 rig && !rig_free ? " held" : "", rig_mask);
        report(name, relMax(sg.debugPackedS(), sc.debugPackedS()), tol);
        snprintf(name, sizeof name, "g   model=%u rig=%u%s mask=%x", model, rig,
                 rig && !rig_free ? " held" : "", rig_mask);
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

// ---- which pairs a rig implies ---------------------------------------------

void testPairSources() {
    using namespace sfm;
    auto has = [](const std::vector<std::pair<uint32_t, uint32_t>>& v, uint32_t a, uint32_t b) {
        return std::binary_search(v.begin(), v.end(), std::make_pair(a, b));
    };
    // Two folders of 40: the window stays inside each, and reaches 16 and 32.
    std::vector<uint32_t> run(80);
    for (uint32_t i = 0; i < 80; i++) run[i] = i / 40;
    const auto seq = sequentialPairs(80, 10, true, run);
    report("pairs: window reaches +10", has(seq, 0, 10) && !has(seq, 0, 11) ? 0.0 : 1.0, 0.5);
    report("pairs: quadratic reaches +16, +32", has(seq, 0, 16) && has(seq, 3, 35) ? 0.0 : 1.0,
           0.5);
    report("pairs: no window across folders", !has(seq, 39, 40) && !has(seq, 30, 46) ? 0.0 : 1.0,
           0.5);
    report("pairs: plain window unchanged",
           sequentialPairs(80, 10, false) == generatePairs(80, PairMode::Sequential, 10) ? 0.0
                                                                                           : 1.0,
           0.5);

    // A dual fisheye of three frames: cam0 = images 0..2, cam1 = 3..5.
    RigDef d;
    d.members = {RigMemberDef{"cam0"}, RigMemberDef{"cam1"}};
    d.kind = "dual-fisheye";
    applyRigKind(d);
    const RigTable t = buildRigTable(
        {"cam0/a", "cam0/b", "cam0/c", "cam1/a", "cam1/b", "cam1/c"}, {d});
    const auto mates = rigMatePairs(t, {{0, 1}, {0, 5}, {0, 3}, {3, 4}}, 60.0);
    report("rig pairs: cam0-cam1 brings cam1-cam0", has(mates, 2, 3) ? 0.0 : 1.0, 0.5);
    report("rig pairs: a seed is not its own mate", !has(mates, 3, 4) && !has(mates, 0, 1) ? 0.0 : 1.0, 0.5);
    report("rig pairs: nothing else", mates.size() == 1 ? 0.0 : 1.0, 0.5);
}

// ---- the mapper on a rig ---------------------------------------------------

void testMapperRig(int device) {
    using namespace sfm;
    synth_rig::RigScene sc = synth_rig::makeRigScene();
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
    synth_rig::RigScene sc = synth_rig::makeRigScene();
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
        testParity(6, rig, true, false, got, device, tol, 0x27);
        testParity(6, rig, true, true, got, device, tol, 0x27);
        testParity(3, rig, true, false, got, device, tol, 0x20);
    }
    // The rig-free problem still takes the plain kernels.
    testParity(3, 0, true, false, got, device, tol);
    testParity(3, 0, true, true, got, device, tol);

    testPairSources();
    testRigAlignment();
    testMapperRig(device);

    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
