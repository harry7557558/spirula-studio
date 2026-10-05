// Factory lens calibration to camera params (sfm/core/LensCalibration.h):
// the 5-term refit against the lens itself, and who wins over it.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/Env.h"
#include "sfm/SfmConfig.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/LensCalibration.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        fails++;
    }
}

// A real Avata 360 clip's master lens, as read (sfm_telemetry_test pins the reading).
static LensCalibration master() {
    LensCalibration l;
    l.track = 0;
    l.lens = "master";
    l.width = l.height = 3840;
    l.fx = 1038.1936f; l.fy = 1038.35229f; l.cx = 1926.75354f; l.cy = 1923.78381f;
    const double k[5] = {0.0909626037f, -0.0467612185f, 0.0301645789f, -0.0117680114f, 0.00143191358f};
    for (int i = 0; i < 5; i++) l.k[i] = k[i];
    l.p1 = -0.000353124284f; l.p2 = -0.000570476695f;
    return l;
}

static LensCalibration slave() {
    LensCalibration l;
    l.track = 1;
    l.lens = "slave";
    l.width = l.height = 3840;
    l.fx = 1046.979f; l.fy = 1047.15088f; l.cx = 1906.91724f; l.cy = 1911.23059f;
    const double k[5] = {0.0674323887f, -0.0157264061f, 0.0115848193f, -0.00665603066f, 0.000904370507f};
    for (int i = 0; i < 5; i++) l.k[i] = k[i];
    l.p1 = 0.000813371211f; l.p2 = 0.000434809597f;
    return l;
}

// The lens itself, written out here rather than taken from the code under test.
static double thetaD(const LensCalibration& l, double th) {
    const double t2 = th * th;
    return th * (1 + t2 * (l.k[0] + t2 * (l.k[1] + t2 * (l.k[2] + t2 * (l.k[3] + t2 * l.k[4])))));
}

static Vec2 lensProject(const LensCalibration& l, double th, double phi) {
    const double uf = th * std::cos(phi), vf = th * std::sin(phi), r2 = th * th;
    const double s = th > 0 ? thetaD(l, th) / th : 1.0;
    const double du = 2 * l.p1 * uf * vf + l.p2 * (r2 + 2 * uf * uf);
    const double dv = l.p1 * (r2 + 2 * vf * vf) + 2 * l.p2 * uf * vf;
    return {l.fx * (s * uf + du) + l.cx, l.fy * (s * vf + dv) + l.cy};
}

// Where the lens reaches the frame's inscribed circle, by bisection.
static double edgeTheta(const LensCalibration& l) {
    double lo = 0, hi = 110 * M_PI / 180;
    for (int i = 0; i < 60; i++) {
        const double mid = 0.5 * (lo + hi);
        (l.fx * thetaD(l, mid) < 0.5 * std::min(l.width, l.height) ? lo : hi) = mid;
    }
    return lo;
}

// Worst pixel gap between the lens and `params` read as `model`, out to the edge.
static double worstGap(const LensCalibration& l, CamModel model, const std::vector<double>& params) {
    if ((int)params.size() != camColmapParams(model)) return INFINITY;
    Camera c = Camera::defaultFor(1, l.width, l.height, 0, model);
    unpackColmap(c, params.data());
    const double tmax = edgeTheta(l);
    double worst = 0;
    for (int i = 0; i <= 400; i++) {
        const double th = tmax * i / 400;
        for (int j = 0; j < 16; j++) {
            const double phi = 2 * M_PI * j / 16;
            const Vec3 b{std::sin(th) * std::cos(phi), std::sin(th) * std::sin(phi), std::cos(th)};
            const Vec2 got = c.project(b), want = lensProject(l, th, phi);
            worst = std::max(worst, std::hypot(got.x - want.x, got.y - want.y));
        }
    }
    return worst;
}

// The mapper reads pixels through bearing(): it must invert the refit to the rim.
static double worstBearing(const LensCalibration& l, const std::vector<double>& params) {
    Camera c = Camera::defaultFor(1, l.width, l.height, 0, CamModel::ThinPrismFisheye);
    unpackColmap(c, params.data());
    const double tmax = edgeTheta(l);
    double worst = 0;
    for (int i = 1; i <= 200; i++) {
        const double th = tmax * i / 200;
        for (int j = 0; j < 16; j++) {
            const double phi = 2 * M_PI * j / 16;
            const Vec3 b{std::sin(th) * std::cos(phi), std::sin(th) * std::sin(phi), std::cos(th)};
            const Vec3 r = c.bearing(c.project(b));
            worst = std::max(worst, std::acos(std::min(1.0, r.x * b.x + r.y * b.y + r.z * b.z)));
        }
    }
    return worst * 180 / M_PI;
}

static void test_params() {
    for (const LensCalibration& l : {master(), slave()}) {
        const std::string tag = "lens " + l.lens + ": ";
        LensFit fit;
        check(lensParams(l, CamModel::ThinPrismFisheye, fit) && fit.params.size() == 12, tag + "thin-prism params");
        if (fit.params.size() != 12) continue;
        const double gap = worstGap(l, CamModel::ThinPrismFisheye, fit.params);
        // 1.97 px (master) and 1.22 px (slave) on the refit, measured.
        check(fit.max_px > 0.5 && fit.max_px < 2.5 && gap <= fit.max_px + 0.05,
              tag + "thin-prism reproduces the lens to the edge, within the refit's own bound (" +
                  std::to_string(gap) + " px, bound " + std::to_string(fit.max_px) + ")");
        const double back = worstBearing(l, fit.params);
        check(back < 1e-4, tag + "bearing() inverts the refit to the rim (" + std::to_string(back) + " deg)");
        check(fit.params[2] == l.cx && fit.params[3] == l.cy, tag + "principal point is the lens's own");
        check(std::fabs(fit.params[1] / fit.params[0] - l.fy / l.fx) < 1e-12, tag + "fy/fx kept");
        check(std::fabs(fit.theta_max - edgeTheta(l)) < 1e-6, tag + "refitted out to the inscribed circle");

        LensFit ocv;
        check(lensParams(l, CamModel::OpenCVFisheye, ocv) && ocv.params.size() == 8 &&
                  ocv.params[0] == fit.params[0] && ocv.params[4] == fit.params[4] &&
                  ocv.params[7] == fit.params[9],
              tag + "opencv-fisheye is the same radial fit without p");
        LensFit none;
        check(!lensParams(l, CamModel::Radial, none) && !lensParams(l, CamModel::Pinhole, none),
              tag + "no params for a pinhole model");
    }
    // What the refit exists for: dropping k5 or p misses by more than the bound.
    LensCalibration no5 = master();
    no5.k[4] = 0;
    LensFit f5, fp;
    const double g5 = lensParams(no5, CamModel::ThinPrismFisheye, f5)
                          ? worstGap(master(), CamModel::ThinPrismFisheye, f5.params) : 0;
    check(std::isfinite(g5) && g5 > 20, "fixture: k5 moves the rim > 20 px");
    const double gp = lensParams(master(), CamModel::OpenCVFisheye, fp)
                          ? worstGap(master(), CamModel::OpenCVFisheye, fp.params) : 0;
    check(std::isfinite(gp) && gp > 2.5, "fixture: p moves the rim past the bound");
    // The bearing check can fail: the lens's own k1..k4 without k5 turn over before the rim.
    const LensCalibration m = master();
    const std::vector<double> raw = {m.fx, m.fy, m.cx, m.cy, m.k[0], m.k[1], m.p1, m.p2, m.k[2], m.k[3], 0, 0};
    check(worstBearing(m, raw) > 1, "fixture: a curve that turns over fails the bearing check");
}

// ---------------------------------------------------------------------------
// Precedence: what a person gave wins, and the factory fills the rest
// ---------------------------------------------------------------------------

static std::vector<LensPlan> plans(int w = 3840) {
    std::vector<LensPlan> out(2);
    out[0].prefix = "cam0"; out[0].lens = master();
    out[1].prefix = "cam1"; out[1].lens = slave();
    for (LensPlan& p : out) {
        p.source = "clip.OSV";
        p.width = p.height = w;
    }
    return out;
}

// As `--camera-model thin-prism-fisheye` leaves it (plus whatever `extra` adds).
static SfmConfig config(const std::vector<std::string>& extra = {}) {
    SfmConfig cfg;
    parseCameraOverride("thin-prism-fisheye", OverrideKind::Model, cfg.camera.overrides);
    cfg.camera_model = "thin-prism-fisheye";
    for (const std::string& e : extra) {
        const size_t c = e.find(':');
        const std::string kind = e.substr(0, c), v = e.substr(c + 1);
        parseCameraOverride(v, kind == "focal" ? OverrideKind::Focal
                               : kind == "distortion" ? OverrideKind::Distortion
                                                      : OverrideKind::Model,
                            cfg.camera.overrides);
        if (v.find('=') == std::string::npos && kind == "focal") cfg.focal = std::atof(v.c_str());
        if (v.find('=') == std::string::npos && kind == "distortion") cfg.distortion = v;
    }
    check(cfg.finalize(CMD_AUTO).empty(), "fixture: config finalizes");
    return cfg;
}

static CameraSetup cameras(const SfmConfig& cfg) {
    std::vector<ImageEntry> images = {{"cam0/00008.jpg", 0}, {"cam1/00008.jpg", 0}};
    std::vector<FeatureSet> feats(2);
    for (FeatureSet& f : feats) {
        f.width = f.height = 3840;
        f.extract_width = f.extract_height = 2400;
    }
    return buildCameras(images, feats, cfg.camera);
}

static std::vector<double> colmap(const CameraSetup& s, size_t i) {
    double p[12]{};
    const Camera& c = s.cameras.at(s.ids[i]);
    packColmap(c, p);
    return std::vector<double>(p, p + camColmapParams(c.model));
}

static void test_precedence() {
    {
        SfmConfig cfg = config();
        const std::string sig = stageSignature(cfg, CMD_MATCH);
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        size_t added = 0;
        for (const CameraOverride& o : cfg.camera.overrides)
            added += (o.prefix == "cam0" || o.prefix == "cam1") && !o.params.empty();
        check(ps[0].use == LensUse::Used && ps[1].use == LensUse::Used && added == 2,
              "plan: both lenses used by default");
        const CameraSetup s = cameras(cfg);
        check(s.count() == 2 && colmap(s, 0) == ps[0].fit.params && colmap(s, 1) == ps[1].fit.params &&
                  ps[0].fit.params != ps[1].fit.params,
              "plan: each lens folder gets its own factory calibration");
        check(s.focal_known.count(s.ids[0]) && s.focal_known.count(s.ids[1]),
              "plan: a factory focal is known, not searched for");
        check(stageSignature(cfg, CMD_MATCH) != sig, "plan: the factory calibration is in the match signature");
    }
    {
        SfmConfig cfg = config({"focal:cam0=800"});
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        const CameraSetup s = cameras(cfg);
        check(ps[0].use == LensUse::Override && ps[1].use == LensUse::Used &&
                  s.cameras.at(s.ids[0]).fx == 800 && colmap(s, 1) == ps[1].fit.params,
              "plan: a CLI focal for one lens wins over its factory calibration");
    }
    {
        SfmConfig cfg = config({"focal:1222.3"});
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        const CameraSetup s = cameras(cfg);
        check(ps[0].use == LensUse::Override && ps[1].use == LensUse::Override &&
                  s.cameras.at(s.ids[0]).fx == 1222.3 && s.cameras.at(s.ids[1]).fx == 1222.3,
              "plan: a dataset-wide CLI focal wins");
    }
    {
        SfmConfig cfg = config({"distortion:0.1,0.01"});
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::Override && ps[1].use == LensUse::Override,
              "plan: a dataset-wide CLI distortion wins");
    }
    {
        SfmConfig cfg = config();
        cfg.camera.focal = 900;   // a manifest's dataset-wide entry: no override
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::DatasetWide && ps[1].use == LensUse::DatasetWide,
              "plan: a dataset-wide manifest focal wins");
    }
    {
        SfmConfig cfg = config();
        CameraOverride m;
        m.prefix = "cam1";
        m.has_model = true;
        m.model = CamModel::ThinPrismFisheye;
        m.params = {900, 900, 1920, 1920, 0, 0, 0, 0, 0, 0, 0, 0};
        cfg.camera.overrides.push_back(m);
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        const CameraSetup s = cameras(cfg);
        check(ps[1].use == LensUse::Override && colmap(s, 1) == m.params,
              "plan: a manifest calibration for a lens wins over the factory's");
    }
    {
        SfmConfig cfg = config({"model:cam1=opencv-fisheye"});
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        size_t named = 0;
        for (const CameraOverride& o : cfg.camera.overrides) named += o.prefix == "cam1";
        const CameraSetup s = cameras(cfg);
        check(ps[1].use == LensUse::Used && ps[1].fit.params.size() == 8 && named == 1 &&
                  s.cameras.at(s.ids[1]).model == CamModel::OpenCVFisheye && colmap(s, 1) == ps[1].fit.params,
              "plan: a lens given only a model gets the factory calibration in that model");
    }
    {
        SfmConfig cfg = config({"model:radial"});
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::Model && ps[1].use == LensUse::Model, "plan: not used for a pinhole model");
    }
    {
        SfmConfig cfg = config();
        auto ps = plans(1920);
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::Size && ps[1].use == LensUse::Size,
              "plan: not used when the frames are not the calibrated size");
        auto none = plans(0);
        applyLensPlans(cfg.camera, none);
        check(none[0].use == LensUse::NoImages, "plan: not used on a folder with no frames");
        check(cfg.camera.overrides.size() == 1, "plan: nothing added for an unused lens");
    }
    {
        // A non-square frame: the width matches the calibration, the height does not.
        SfmConfig cfg = config();
        auto ps = plans();
        ps[0].height = 1920;
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::Size && ps[1].use == LensUse::Used,
              "plan: not used when only the height differs from the calibrated size");
        SfmConfig cfg2 = config();
        auto wide = plans();
        wide[0].width = 1920;
        applyLensPlans(cfg2.camera, wide);
        check(wide[0].use == LensUse::Size, "plan: not used when only the width differs");
    }
    {
        SfmConfig cfg = config();
        cfg.camera.params = {900, 900, 1920, 1920, 0, 0, 0, 0, 0, 0, 0, 0};   // a manifest's dataset-wide calibration
        auto ps = plans();
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::DatasetWide && ps[1].use == LensUse::DatasetWide,
              "plan: a dataset-wide manifest calibration wins");
    }
    {
        // A focal given for the capture stays in force when another setting
        // names one of its lenses: the shorter prefix must be read too.
        SfmConfig cfg = config({"focal:clip=900", "model:clip/cam0=opencv-fisheye"});
        std::vector<LensPlan> ps(2);
        ps[0].prefix = "clip/cam0"; ps[0].lens = master();
        ps[1].prefix = "clip/cam1"; ps[1].lens = slave();
        for (LensPlan& p : ps) p.width = p.height = 3840;
        applyLensPlans(cfg.camera, ps);
        size_t factory = 0;
        for (const CameraOverride& o : cfg.camera.overrides)
            factory += (o.prefix == "clip/cam0" || o.prefix == "clip/cam1") && !o.params.empty();
        check(ps[0].use == LensUse::Override, "plan: a capture focal covers a lens that another setting names");
        check(ps[1].use == LensUse::Override, "plan: a capture focal covers a lens no other setting names");
        check(factory == 0, "plan: no factory override added under a capture focal");
    }
    {
        SfmConfig cfg = config();
        CameraOverride wide;
        wide.prefix = "clip";
        wide.params = {900, 900, 1920, 1920, 0, 0, 0, 0, 0, 0, 0, 0};
        cfg.camera.overrides.push_back(wide);
        CameraOverride lens;
        lens.prefix = "clip/cam0";
        lens.has_model = true;
        lens.model = CamModel::ThinPrismFisheye;
        cfg.camera.overrides.push_back(lens);
        std::vector<LensPlan> ps(1);
        ps[0].prefix = "clip/cam0"; ps[0].lens = master();
        ps[0].width = ps[0].height = 3840;
        applyLensPlans(cfg.camera, ps);
        size_t factory = 0;
        for (const CameraOverride& o : cfg.camera.overrides) factory += o.prefix == "clip/cam0" && !o.params.empty();
        check(ps[0].use == LensUse::Override && factory == 0,
              "plan: a capture calibration covers a lens that another setting names");
    }
    {
        // The model a person chose for the capture is the one the factory params are cut for.
        SfmConfig cfg = config({"model:clip=opencv-fisheye"});
        CameraOverride nothing;   // an entry for the lens that sets nothing
        nothing.prefix = "clip/cam0";
        cfg.camera.overrides.push_back(nothing);
        std::vector<LensPlan> ps(1);
        ps[0].prefix = "clip/cam0"; ps[0].lens = master();
        ps[0].width = ps[0].height = 3840;
        applyLensPlans(cfg.camera, ps);
        check(ps[0].use == LensUse::Used && ps[0].model == CamModel::OpenCVFisheye &&
                  ps[0].fit.params.size() == 8,
              "plan: the capture's model is kept when a nearer entry names none");
        std::vector<ImageEntry> images = {{"clip/cam0/00008.jpg", 0}};
        std::vector<FeatureSet> feats(1);
        feats[0].width = feats[0].height = 3840;
        const CameraSetup s = buildCameras(images, feats, cfg.camera);
        check(s.cameras.at(s.ids[0]).model == CamModel::OpenCVFisheye && colmap(s, 0) == ps[0].fit.params,
              "plan: that lens is built in the model its params were cut for");
    }
}

// A model given to the capture reaches its lens folders: the factory override
// must carry it, or its params would be read in the dataset-wide model's order.
static void test_nested_model() {
    SfmConfig cfg = config({"model:clip=opencv-fisheye"});
    std::vector<LensPlan> ps(1);
    ps[0].prefix = "clip/cam0";
    ps[0].lens = master();
    ps[0].width = ps[0].height = 3840;
    applyLensPlans(cfg.camera, ps);
    std::vector<ImageEntry> images = {{"clip/cam0/00008.jpg", 0}};
    std::vector<FeatureSet> feats(1);
    feats[0].width = feats[0].height = 3840;
    const CameraSetup s = buildCameras(images, feats, cfg.camera);
    check(ps[0].use == LensUse::Used && s.cameras.at(s.ids[0]).model == CamModel::OpenCVFisheye &&
              colmap(s, 0) == ps[0].fit.params,
          "plan: a model given for the capture reaches its lenses");
}

static std::vector<LensCalibration> fakeLenses(const std::string& path) {
    return path == "x.OSV" ? std::vector<LensCalibration>{master(), slave()} : std::vector<LensCalibration>{};
}

static void test_collect() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "sfm_lens_calib_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "clip" / "cam0");
    fs::create_directories(dir / "clip" / "cam1");
    std::ofstream(dir / "clip" / "cam0" / "0notes.txt") << "not an image";
    std::ofstream(dir / "clip" / "cam0" / "00008.pgm") << "P5\n3840 3840\n255\n";
    // stb reads this size and then rejects the file: no size, not 1920.
    std::ofstream(dir / "clip" / "cam1" / "00008.pgm") << "P5\n1920 1920\n70000\n";
    int w = 0, h = 0;
    check(!imageSize((dir / "clip" / "cam1" / "00008.pgm").string(), w, h) && w == 1920,
          "fixture: a frame whose size is read before it is rejected");
    const auto ps = collectLensPlans({{"clip", "x.OSV", 0, 0}, {"", "none.OSV", 0, 0}}, dir.string(), fakeLenses);
    check(ps.size() == 2 && ps[0].prefix == "clip/cam0" && ps[0].width == 3840 && ps[0].height == 3840 &&
              ps[0].lens.lens == "master" && ps[1].prefix == "clip/cam1" && ps[1].width == 0 &&
              ps[1].source == "x.OSV",
          "collect: capture prefix + camN, sized by the first frame that reads");
    fs::remove_all(dir, ec);
}

static void test_real_clip() {
    const char* path = spirula::env("TEST_AVATA_OSV");
    if (!path) { std::printf("SKIP lens calibration real clip (SS_TEST_AVATA_OSV unset)\n"); return; }
    const auto ls = video_lenses(std::string(path));
    check(ls.size() == 2, "avata flight: two lenses");
    for (const LensCalibration& l : ls) {
        LensFit fit;
        const bool ok = lensParams(l, CamModel::ThinPrismFisheye, fit);
        check(ok && fit.max_px < 2.5 && worstGap(l, CamModel::ThinPrismFisheye, fit.params) <= fit.max_px + 0.05 &&
                  std::fabs(fit.theta_max * 180 / M_PI - 100.1) < 0.5,
              "avata flight " + l.lens + ": thin-prism refit to 100 deg within 2.5 px");
    }
}

static int cmdLensCalibTest(int, char**) {
    test_params();
    test_precedence();
    test_nested_model();
    test_collect();
    test_real_clip();
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdLensCalibTest); }
