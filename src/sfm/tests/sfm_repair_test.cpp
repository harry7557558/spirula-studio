// Mapper::repair on a finished synthetic model, pinhole and equirect: cameras
// knocked out of place come back (named, hinted, or found by the audit), and
// images taken out register again, each near the pose the model first had.
//
//   sfm_repair_test [--device N] [--verbose]
//
// Prints FAIL lines and returns the count. The BA is real (GPU).
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "sfm/map/Mapper.h"
#include "sfm/tests/SyntheticRegister.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;
using namespace synth_reg;

static int fails = 0;
static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        fails++;
    }
}

// A walk through a room: the 360 camera sees every wall point within 7 units.
static Scene makeEquirectScene(int M) {
    const int W = 2048, H = 1024, N = 1500;
    Camera K = Camera::defaultFor(1, W, H, 0, CamModel::Equirect);
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> ux(-10, 10), uy(-3, 3), uz(-5, 5);
    std::normal_distribution<double> noise(0.0, 0.4);
    std::vector<Vec3> pts;
    while ((int)pts.size() < N) {
        Vec3 p = {ux(rng), uy(rng), uz(rng)};
        if (std::fabs(p.z) < 2.0 && std::fabs(p.y) < 1.5) continue;
        pts.push_back(p);
    }
    Scene s;
    std::vector<std::vector<char>> vis(M, std::vector<char>(N, 0));
    s.feats.resize(M);
    for (int c = 0; c < M; c++) {
        const Vec3 C = {-6.0 + 12.0 * c / (M - 1), 0.2 * std::sin(0.9 * c), 0.0};
        const Mat3 R = angleAxisToRotation({0.0, 0.3 * c, 0.0});
        const Vec3 t = mul(R, C);
        Pose P = {R, {-t.x, -t.y, -t.z}};
        s.gt.push_back(P);
        s.feats[c].width = W;
        s.feats[c].height = H;
        s.feats[c].keypoints.resize(N);
        for (int p = 0; p < N; p++) {
            const Vec3 pc = mul(P.R, pts[p]) + P.t;
            if (pc.norm() < 7.0) {
                const Vec2 px = K.project(pc);
                const double x = std::fmod(px.x + noise(rng) + W, (double)W);
                s.feats[c].keypoints[p] = {(float)x, (float)(px.y + noise(rng)), 2, 0, 0};
                vis[c][p] = 1;
            } else {
                s.feats[c].keypoints[p] = {-1000, -1000, 2, 0, 0};
            }
        }
    }
    s.db.images.resize(M);
    for (int c = 0; c < M; c++) s.db.images[c] = {"pano" + std::to_string(c), (uint32_t)N};
    for (int i = 0; i < M; i++)
        for (int j = i + 1; j < M; j++) {
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            for (int p = 0; p < N; p++)
                if (vis[i][p] && vis[j][p]) tv.matches.push_back({(uint32_t)p, (uint32_t)p, 0});
            if (tv.matches.size() >= 15) s.db.pairs.push_back(std::move(tv));
        }
    return s;
}

static double rotDeg(const Pose& a, const Pose& b) {
    return rotationAngleDeg(mul(a.R, transpose(b.R)));
}

static double shiftFrac(const Reconstruction& ref, const Pose& a, const Pose& b) {
    return (cameraCenter(a) - cameraCenter(b)).norm() / Mapper::modelScaleOf(ref);
}

// `p` turned `deg` about its own centre and moved `shift` model scales along x.
static Pose perturb(const Reconstruction& ref, const Pose& p, double deg, double shift) {
    const Vec3 C = cameraCenter(p) + Vec3{shift * Mapper::modelScaleOf(ref), 0, 0};
    const Mat3 R = mul(angleAxisToRotation({0.0, deg * M_PI / 180.0, 0.0}), p.R);
    const Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

static bool closeTo(const Reconstruction& ref, const Reconstruction& got, uint32_t img,
                    const char* tag) {
    if (!registered(got, img)) {
        std::printf("  %s: image %u not registered\n", tag, img);
        return false;
    }
    const Pose& a = got.images.at(img).pose;
    const Pose& b = ref.images.at(img).pose;
    const double r = rotDeg(a, b), s = shiftFrac(ref, a, b);
    if (r < 1.0 && s < 0.02) return true;
    std::printf("  %s: image %u off by %.2f deg, %.4f scale\n", tag, img, r, s);
    return false;
}

// `img` left registered at `pose` with none of its observations: where the
// audit's evidence is, since what an image observes itself proves nothing.
static void misplace(Reconstruction& m, uint32_t img, const Pose& pose) {
    std::set<uint32_t> others;
    for (const auto& kv : m.images)
        if (kv.second.registered && kv.first != img) others.insert(kv.first);
    Image keep = m.images.at(img);
    m = subsetModel(m, others);
    keep.pose = pose;
    std::fill(keep.point3D_ids.begin(), keep.point3D_ids.end(), kInvalidPoint3D);
    m.images[img] = keep;
}

static bool outcome(const Mapper::RepairStats& st, uint32_t img, Mapper::RepairOutcome o) {
    for (const auto& it : st.items)
        if (it.image == img) return it.outcome == o;
    return false;
}

static void testScene(const char* name, const Scene& sc, MapperOptions opt) {
    const uint32_t M = (uint32_t)sc.db.images.size();
    Mapper base(sc.db, sc.feats, opt);
    std::vector<Reconstruction> models = base.run();
    check(!models.empty() && models.front().numRegistered() == M,
          std::string(name) + ": the base run registers every image");
    if (models.empty()) return;
    const Reconstruction ref = models.front();
    const std::vector<uint32_t> bad = {M / 4, M / 2, 3 * M / 4};

    {
        Reconstruction m = ref;
        for (uint32_t i : bad) m.images[i].pose = perturb(ref, ref.images.at(i).pose, 90.0, 0.3);
        Mapper mp(sc.db, sc.feats, opt);
        Mapper::RepairRequest rq;
        rq.replace = bad;
        Mapper::RepairStats st;
        const Reconstruction out = mp.repair(m, rq, &st);
        for (uint32_t i : bad) {
            check(closeTo(ref, out, i, "replace"),
                  std::string(name) + ": a re-placed camera returns to its pose");
            check(outcome(st, i, Mapper::RepairOutcome::Moved),
                  std::string(name) + ": a re-placed camera is reported moved");
        }
        check(out.numRegistered() == M, std::string(name) + ": re-placing loses no image");
    }
    {
        Reconstruction m = ref;
        Mapper::RepairRequest rq;
        for (uint32_t i : bad) {
            m.images[i].pose = perturb(ref, ref.images.at(i).pose, 150.0, 1.0);
            rq.hints.emplace_back(i, perturb(ref, ref.images.at(i).pose, 15.0, 0.1));
        }
        Mapper mp(sc.db, sc.feats, opt);
        Mapper::RepairStats st;
        const Reconstruction out = mp.repair(m, rq, &st);
        for (uint32_t i : bad) {
            check(closeTo(ref, out, i, "hint"),
                  std::string(name) + ": a hinted camera snaps to its pose");
            check(outcome(st, i, Mapper::RepairOutcome::Moved),
                  std::string(name) + ": a hinted camera is reported moved");
        }
    }
    {
        const std::vector<uint32_t> gone = {1, M / 3, 2 * M / 3, M - 2};
        std::set<uint32_t> keep;
        for (uint32_t i = 0; i < M; i++) keep.insert(i);
        for (uint32_t i : gone) keep.erase(i);
        const Reconstruction m = subsetModel(ref, keep);
        Mapper mp(sc.db, sc.feats, opt);
        Mapper::RepairRequest rq;
        rq.add = {gone[0], gone[1]};
        Mapper::RepairStats st;
        const Reconstruction out = mp.repair(m, rq, &st);
        for (uint32_t i : rq.add) {
            check(closeTo(ref, out, i, "add"), std::string(name) + ": a missing image registers");
            check(outcome(st, i, Mapper::RepairOutcome::Added),
                  std::string(name) + ": a missing image is reported added");
        }
        check(!registered(out, gone[2]) && !registered(out, gone[3]),
              std::string(name) + ": growth stays on the images asked for");
    }
    {
        Reconstruction m = ref;
        const uint32_t i = bad[1];
        misplace(m, i, perturb(ref, ref.images.at(i).pose, 120.0, 0.5));
        Mapper mp(sc.db, sc.feats, opt);
        Mapper::RepairRequest rq;
        rq.audit_all = true;
        Mapper::RepairStats st;
        const Reconstruction out = mp.repair(m, rq, &st);
        check(st.audit.checked == M, std::string(name) + ": the audit checks every image");
        check(closeTo(ref, out, i, "audit"),
              std::string(name) + ": the audit finds and moves a misplaced camera");
        int untouched = 0;
        for (uint32_t k = 0; k < M; k++)
            if (k != i && closeTo(ref, out, k, "audit untouched")) untouched++;
        check(untouched == (int)M - 1, std::string(name) + ": the audit leaves the rest alone");
    }
}

static int body(int argc, char** argv) {
    MapperOptions opt;
    opt.verbose = false;
    opt.focal = 1200;
    opt.focal_trials = 0;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--device" && i + 1 < argc) opt.device = std::stoi(argv[++i]);
        else if (a == "--verbose") opt.verbose = true;
    }
    testScene("pinhole", makeScene(24), opt);
    MapperOptions eq = opt;
    eq.camera_model = CamModel::Equirect;
    eq.known_focal_cameras = {1};
    testScene("equirect", makeEquirectScene(20), eq);
    std::printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails, fails == 1 ? "" : "s");
    return fails;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, body); }
