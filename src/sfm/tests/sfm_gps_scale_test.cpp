// The block scale check during growth (Mapper::gpsScaleCheck, map/BlockScale.h): a corridor
// walked at 1 m per frame with a hover and a loop, fixes quantised to 0.01 arcsec as EXIF
// stores them. Past camera `from` the GPS track is stretched by `k` about it, which the
// images cannot follow. The BA is real (GPU, or the host solver without fp64).
//
//   sfm_gps_scale_test [--device N] [--verbose] [--k-big K]
//
// Prints FAIL lines and returns the count.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/SensorPriors.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;
static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        fails++;
    }
}

constexpr int kW = 1280, kH = 960;
constexpr int kWalk = 180;               // walking frames, 1 m apart
constexpr int kHover0 = 40, kHover = 20;  // a still camera, 3 cm per frame
constexpr int kLoop0 = 100, kLoop = 24;   // a loop of radius 2 m

struct Scene {
    MatchesDatabase db;
    std::vector<FeatureSet> feats;
    std::vector<Vec3> centres;
};

static std::vector<int> g_walk_cam;  // walking frame -> camera

// East is x, north is z, y points down; every camera looks north.
static std::vector<Vec3> path() {
    std::vector<Vec3> c;
    double x = 0;
    for (int i = 0; i < kWalk; i++) {
        if (i == kHover0)
            for (int h = 0; h < kHover; h++) c.push_back({x + 0.03 * h, 0.1 * std::sin(0.4 * h), 0});
        if (i == kLoop0)
            for (int l = 0; l < kLoop; l++) {
                const double a = 2 * M_PI * l / kLoop;
                c.push_back({x + 2 * std::sin(a), 0.1 * std::sin(0.4 * l), 2 * (1 - std::cos(a))});
            }
        g_walk_cam.push_back((int)c.size());
        c.push_back({x, 0.3 * std::sin(0.4 * i), 2.0 * std::sin(0.08 * i)});
        x += 1.0;
    }
    return c;
}

static Scene makeScene() {
    Scene s;
    s.centres = path();
    const int n = (int)s.centres.size();
    const Camera K = Camera::defaultFor(1, kW, kH, 1200);
    std::mt19937 rng(7);
    const double x1 = s.centres.back().x + 8;
    std::uniform_real_distribution<double> ux(-8, x1), uy(-3, 3), uz(9, 15);
    std::normal_distribution<double> noise(0.0, 0.3);
    std::vector<Vec3> pts((size_t)(90 * (x1 + 8)));
    for (auto& p : pts) p = {ux(rng), uy(rng), uz(rng)};
    std::vector<std::map<uint32_t, uint32_t>> feat_of(n);  // point -> feature index
    s.feats.resize(n);
    for (int c = 0; c < n; c++) {
        s.feats[c].width = kW;
        s.feats[c].height = kH;
        for (uint32_t p = 0; p < pts.size(); p++) {
            const Vec3 pc = pts[p] - s.centres[c];
            if (pc.z < 0.1 || std::fabs(pc.x) > pc.z) continue;
            const Vec2 px = K.project(pc);
            if (px.x <= 0 || px.x >= kW || px.y <= 0 || px.y >= kH) continue;
            feat_of[c][p] = (uint32_t)s.feats[c].keypoints.size();
            s.feats[c].keypoints.push_back(
                {(float)(px.x + noise(rng)), (float)(px.y + noise(rng)), 2, 0, 0});
        }
    }
    s.db.images.resize(n);
    for (int c = 0; c < n; c++)
        s.db.images[c] = {"cam" + std::to_string(c), (uint32_t)s.feats[c].keypoints.size()};
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            if (std::fabs(s.centres[i].x - s.centres[j].x) > 14) continue;
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            for (const auto& kv : feat_of[i]) {
                auto it = feat_of[j].find(kv.first);
                if (it != feat_of[j].end()) tv.matches.push_back({kv.second, it->second, 0});
            }
            if (tv.matches.size() >= 15) s.db.pairs.push_back(std::move(tv));
        }
    return s;
}

// Metres east/north of a reference as a fix, rounded to 0.01 arcsec as EXIF stores it.
static Geodetic fixAt(double e, double n, double alt) {
    constexpr double lat0 = 42.2, lon0 = -83.6;
    constexpr double a = 6378137.0, f = 1.0 / 298.257223563, e2 = f * (2.0 - f);
    const double p = lat0 * M_PI / 180.0, w = 1.0 - e2 * std::sin(p) * std::sin(p);
    const double M = a * (1.0 - e2) / std::pow(w, 1.5), N = a / std::sqrt(w);
    auto q = [](double deg) { return std::round(deg * 360000.0) / 360000.0; };
    return {q(lat0 + n / M * 180.0 / M_PI), q(lon0 + e / (N * std::cos(p)) * 180.0 / M_PI), alt};
}

struct Out {
    uint32_t in_run = 0, end = 0, registered = 0;
    std::vector<Mapper::PriorStats::ScaleRequest> req;
    Reconstruction model;
};

// GPS stretched by `k` past the walking frame `from_walk`, about it.
static Out run(const Scene& sc, MapperOptions opt, double k, int from_walk, double band) {
    const int n = (int)sc.centres.size();
    const int from = from_walk < kWalk ? g_walk_cam[from_walk] : n;
    std::vector<std::optional<Geodetic>> fixes(n);
    for (int c = 0; c < n; c++) {
        const Vec3 o = sc.centres[std::min(from, n - 1)];
        const Vec3 C = c < from ? sc.centres[c] : o + (sc.centres[c] - o) * k;
        fixes[c] = fixAt(C.x, C.z, 250.0 - C.y);
    }
    SensorPriorOptions so;
    so.verbose = opt.verbose;
    so.gps_flat = true;
    ExifGpsPriors gps(fixes, so);
    opt.gps_scale_band = band;
    Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &gps);
    std::vector<Reconstruction> models = m.run();
    Out r;
    r.in_run = m.priorStats().gps_scale_ba;
    r.end = m.priorStats().gps_scale_end;
    r.req = m.priorStats().scale_requests;
    if (!models.empty()) {
        r.model = models.front();
        r.registered = r.model.numRegistered();
    }
    return r;
}

static double centreDiff(const Reconstruction& a, const Reconstruction& b) {
    if (a.images.size() != b.images.size()) return INFINITY;
    double d = 0;
    for (const auto& kv : a.images) {
        auto it = b.images.find(kv.first);
        if (it == b.images.end()) return INFINITY;
        d = std::max(d, (cameraCenter(kv.second.pose) - cameraCenter(it->second.pose)).norm());
    }
    return d;
}

static void print(const char* what, const Out& o) {
    std::printf("%s: %u registered, %u in-run request(s), %u after growth\n", what, o.registered,
                o.in_run, o.end);
    for (const auto& q : o.req)
        std::printf("  %s img %u side %d | 60/100/150 %.4f %.4f %.4f -> BA %.4f %.4f %.4f\n",
                    q.end ? "end" : "run", q.img, q.side, std::exp(q.x[0]), std::exp(q.x[1]),
                    std::exp(q.x[2]), std::exp(q.post[0]), std::exp(q.post[1]), std::exp(q.post[2]));
}

// `full`, cut down to images 0..hi and the tracks that still have 2+ views left.
static Reconstruction subModel(const Reconstruction& full, uint32_t hi) {
    Reconstruction r = full;
    for (auto it = r.images.begin(); it != r.images.end();)
        it = it->first > hi ? r.images.erase(it) : std::next(it);
    for (auto it = r.points3D.begin(); it != r.points3D.end();) {
        auto& tr = it->second.track;
        tr.erase(std::remove_if(tr.begin(), tr.end(),
                                [&](const TrackElement& e) { return e.image_id > hi; }),
                 tr.end());
        it = tr.size() < 2 ? r.points3D.erase(it) : std::next(it);
    }
    return r;
}

// The bottom-up/atoms path (Assemble.h's growModels, growByPnP's only caller): unlike
// Mapper::run()'s flat path (grow() -> checkedRefine -> bssAfterBa), growByPnP defers its own
// BA to the caller's later joint solve, so a request it raises here is never resolved.
static void testAssemblyScaleRequest(const Scene& sc, const Reconstruction& full,
                                     MapperOptions opt, int from_walk, double k) {
    const int n = (int)sc.centres.size();
    const int from = from_walk < kWalk ? g_walk_cam[from_walk] : n;
    // 15 images of margin: enough that gpsScaleCheck's own 10-registrations-since-adoption
    // gate clears at almost exactly the point growth crosses into the drifted tail, so most
    // of the growth this test drives sees the stretch.
    const uint32_t hi = (uint32_t)std::max(0, from - 15);
    const Reconstruction part = subModel(full, hi);

    std::vector<std::optional<Geodetic>> fixes(n);
    for (int c = 0; c < n; c++) {
        const Vec3 o = sc.centres[std::min(from, n - 1)];
        const Vec3 C = c < from ? sc.centres[c] : o + (sc.centres[c] - o) * k;
        fixes[c] = fixAt(C.x, C.z, 250.0 - C.y);
    }
    SensorPriorOptions so;
    so.gps_flat = true;
    ExifGpsPriors gps(fixes, so);
    MapperOptions o = opt;
    o.ba_growth_ratio = 1.5;
    o.gps_scale_band = 1.0;
    Mapper m(sc.db, sc.feats, o, {}, nullptr, nullptr, &gps);

    std::vector<Reconstruction> models{part};
    std::vector<char> dirty(1, 0);
    size_t rejected = 0;
    detail::growModels(m, models, dirty, {}, 4.0, (size_t)n, rejected);

    const std::vector<Mapper::PriorStats::ScaleRequest> req = m.priorStats().scale_requests;
    std::printf("growByPnP scale: %u..%u adopted, %u registered after growth, %zu request(s)\n",
               0u, hi, models[0].numRegistered(), req.size());
    check(!req.empty(),
         "fixture: growth through growByPnP alone reaches the drift and asks for a BA");
    if (req.empty()) return;
    const Mapper::PriorStats::ScaleRequest& q = req.back();
    // Only the (side, l) the pick actually chose is guaranteed a reading at request time
    // (q.x itself is NaN on the other lengths whenever their own window lacks support).
    check(!std::isnan(q.x[q.l]),
         "fixture: the recorded request carries the firing side's reading");
    check(std::isnan(q.post[q.l]),
         "assembly: growByPnP's request is recorded but never resolved -- post stays unset, "
         "because that model's BA is the caller's later joint solve");
}

static int body(int argc, char** argv) {
    MapperOptions opt;
    opt.verbose = false;
    opt.focal = 1200;
    opt.focal_trials = 0;
    double k_big = 1.15;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--device" && i + 1 < argc) opt.device = std::stoi(argv[++i]);
        else if (a == "--verbose") opt.verbose = true;
        else if (a == "--k-big" && i + 1 < argc) k_big = std::stod(argv[++i]);
    }
    const Scene sc = makeScene();
    const int n = (int)sc.centres.size();
    const Out same = run(sc, opt, 1.0, kWalk, 1.0);
    print("true scale", same);
    check(same.registered == (uint32_t)n, "fixture: every image registers");
    check(same.in_run == 0 && same.end == 0,
          "scale: a true-scale track with a hover and a loop asks for nothing");

    // A canopy drone capture's west chain grew 112 frames past its last growth BA; at 1.1 the corridor's
    // blocks would be ten frames, whose factor has to come from the window, not the block.
    MapperOptions sparse = opt;
    sparse.ba_growth_ratio = 1.5;
    const Out big = run(sc, sparse, k_big, kWalk - 80, 1.0);
    print("stretched", big);
    check(big.in_run >= 1, "scale: a stretched GPS tail asks for a BA during growth");
    check(!big.req.empty() && !big.req[0].end && !std::isnan(big.req[0].x[0]),
          "scale: the first request reads the 60 m window");
    check(!big.req.empty() && !big.req[0].end &&
              std::fabs(big.req[0].post[0]) < 0.7 * std::fabs(big.req[0].x[0]),
          "scale: after the requested BA alone (no rescale) the reading is much nearer the GPS");
    check(big.in_run <= 1 + big.registered / 10,
          "scale: at most one request per ten registrations");

    testAssemblyScaleRequest(sc, same.model, opt, kWalk - 80, k_big);

    using Req = Mapper::PriorStats::ScaleRequest;
    const Req last = big.req.empty() ? Req{} : big.req.back();
    check(big.end == 1 && last.end && last.x[0] < -bss::kTauEnd[0],
          "scale: the tail growth leaves short asks for a BA once more, after growth");

    const Out off = run(sc, sparse, k_big, kWalk - 80, 0);
    check(off.in_run == 0 && off.end == 0 && off.req.empty(), "scale: a band of 0 asks for nothing");
    const Out off_same = run(sc, opt, 1.0, kWalk, 0), again = run(sc, opt, 1.0, kWalk, 1.0);
    const double floor = centreDiff(same.model, again.model);
    const double moved = centreDiff(same.model, off_same.model);
    std::printf("never fires: %.3e against a run-to-run floor of %.3e\n", moved, floor);
    check(again.in_run == 0 && again.end == 0 && moved <= std::max(2 * floor, 1e-6),
          "scale: a check that never fires moves nothing beyond run-to-run noise");
    std::printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails, fails == 1 ? "" : "s");
    return fails;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, body); }
