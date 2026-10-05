// The open-seam detector (Mapper::openSeams) and weld (Mapper::weldSeams). A straight track
// whose halves were built as two fronts, so every point both halves see is held twice: the
// east half sits a rigid 2 m / 1.5 deg off (offset seam) or turns 1.5 deg about the join
// (turn seam). Plus a loop revisit, which reads as low as a seam but shares its neighbours,
// and an east half that sees its own perturbed copy: duplicates that are not one point.
// The weld's BA is real (GPU).
//
//   sfm_seam_weld_test [--device N] [--verbose] [--gps-cap N]
//
// Prints FAIL lines and returns the count.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/Sequence.h"
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

constexpr int kW = 1280, kH = 960, kPtsPerCam = 170;
constexpr uint32_t kDupA = 3, kDupB = 4;  // the near-duplicate pair: a real link, junk-diluted
constexpr uint32_t kLoopOf = 5;           // the camera the loop revisit comes back to

enum class Seam { None, Offset, Turn };

struct Spec {
    int cams = 24;
    std::function<double(int)> yaw_deg = [](int c) { return 0.1 * c; };
    bool dilute = false;
    std::vector<int> revisits;  // cameras seen again, as extra images after the track
    double east_copy = 0;       // metres: the east half sees each point moved this much at random
};

struct Scene {
    Spec spec;
    MatchesDatabase db;
    std::vector<FeatureSet> feats;
    std::vector<Vec3> pts, east_pts, centres;
    std::vector<Mat3> rots;  // world -> camera
    std::vector<std::vector<char>> vis;
    Camera K;
    int join() const { return spec.cams / 2; }
    int images() const { return (int)centres.size(); }
};

static Mat3 yawRot(double deg) { return angleAxisToRotation({0, deg * M_PI / 180.0, 0}); }

static Scene makeScene(const Spec& spec) {
    Scene s;
    s.spec = spec;
    s.K = Camera::defaultFor(1, kW, kH, 1200);
    const int npts = kPtsPerCam * spec.cams;
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> ux(-5, spec.cams + 4), uy(-3, 3), uz(7, 13);
    std::normal_distribution<double> noise(0.0, 0.3);
    s.pts.resize(npts);
    for (auto& p : s.pts) p = {ux(rng), uy(rng), uz(rng)};
    s.east_pts = s.pts;
    std::normal_distribution<double> copy(0.0, std::max(spec.east_copy, 1.0));
    if (spec.east_copy > 0)
        for (auto& p : s.east_pts) p = p + Vec3{copy(rng), copy(rng), copy(rng)};
    for (int c = 0; c < spec.cams; c++) {
        s.centres.push_back({1.0 * c, 0.3 * std::sin(0.4 * c), 1.5 * std::sin(0.2 * c)});
        s.rots.push_back(yawRot(spec.yaw_deg(c)));
    }
    for (int r : spec.revisits) {
        s.centres.push_back(s.centres[r] + Vec3{0, 0.4, 0});
        s.rots.push_back(s.rots[r]);
    }
    const int n = s.images();
    s.feats.resize(n);
    s.vis.assign(n, std::vector<char>(npts, 0));
    for (int c = 0; c < n; c++) {
        s.feats[c].width = kW;
        s.feats[c].height = kH;
        s.feats[c].keypoints.resize(npts);
        const bool east = c >= spec.cams / 2 && c < spec.cams;
        for (int p = 0; p < npts; p++) {
            const Vec3 pc = mul(s.rots[c], (east ? s.east_pts[p] : s.pts[p]) - s.centres[c]);
            const Vec2 px = s.K.project(pc);
            if (pc.z > 0.1 && px.x > 0 && px.x < kW && px.y > 0 && px.y < kH) {
                const float x = (float)(px.x + noise(rng)), y = (float)(px.y + noise(rng));
                s.feats[c].keypoints[p] = {x, y, 2, 0, 0};
                s.vis[c][p] = 1;
            } else {
                s.feats[c].keypoints[p] = {-1000, -1000, 2, 0, 0};
            }
        }
    }
    s.db.images.resize(n);
    for (int c = 0; c < n; c++) {
        char nm[16];
        std::snprintf(nm, sizeof nm, "cam%02d", c);
        s.db.images[c] = {nm, (uint32_t)npts};
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            std::vector<uint32_t> both;
            for (int p = 0; p < npts; p++)
                if (s.vis[i][p] && s.vis[j][p]) both.push_back(p);
            if (spec.dilute && (uint32_t)i == kDupA && (uint32_t)j == kDupB) {
                // 400 true matches and 850 between features of different points:
                // a stationary pair's worth of evidence that no model can explain.
                std::shuffle(both.begin(), both.end(), rng);
                std::vector<uint32_t> spare(both.begin() + 400, both.end());
                both.resize(400);
                std::vector<uint32_t> other = spare;
                std::rotate(other.begin(), other.begin() + 1, other.end());
                for (uint32_t p : both) tv.matches.push_back({p, p, 0});
                for (size_t k = 0; k < spare.size() && k < 850; k++)
                    tv.matches.push_back({spare[k], other[k], 0});
            } else {
                for (uint32_t p : both) tv.matches.push_back({p, p, 0});
            }
            if (tv.matches.size() >= 15) s.db.pairs.push_back(std::move(tv));
        }
    return s;
}

// The east half's error: a rigid move about a point at the join, at the scene's mid depth.
struct Piece {
    Mat3 Q = mat3Identity();
    Vec3 o{0, 0, 10}, d{0, 0, 0};
    Vec3 apply(const Vec3& x) const { return mul(Q, x - o) + o + d; }
};

// Revisit images hold 10 % of what they see on the track's own points and the rest on a copy
// 1.5 m off: a loop the mapper never closed, but tied into the same neighbourhood.
static Reconstruction makeModel(const Scene& s, Seam seam, double shift = 2.0) {
    const int J = s.join(), cams = s.spec.cams;
    Piece east;
    east.o.x = J - 0.5;
    if (seam != Seam::None) east.Q = angleAxisToRotation({0, 1.5 * M_PI / 180.0, 0});
    if (seam == Seam::Offset) east.d = {shift, 0, 0};
    Reconstruction m;
    m.cameras[1] = s.K;
    for (int c = 0; c < s.images(); c++) {
        Image im;
        im.id = c;
        im.camera_id = 1;
        im.name = s.db.images[c].name;
        im.registered = true;
        const bool e = seam != Seam::None && c >= J && c < cams;
        const Vec3 C = e ? east.apply(s.centres[c]) : s.centres[c];
        const Mat3 R = e ? mul(s.rots[c], transpose(east.Q)) : s.rots[c];
        const Vec3 t = mul(R, C);
        im.pose = {R, {-t.x, -t.y, -t.z}};
        for (const Keypoint& k : s.feats[c].keypoints) im.points2D.push_back({k.x, k.y});
        im.point3D_ids.assign(s.pts.size(), kInvalidPoint3D);
        m.images[c] = im;
    }
    std::mt19937 rng(3);
    std::uniform_real_distribution<double> u(0, 1);
    auto add = [&](const Vec3& X, const std::vector<TrackElement>& tr) {
        if (tr.size() < 2) return;
        const uint64_t id = m.addPoint3D(X, tr);
        for (const TrackElement& e : tr) m.images[e.image_id].point3D_ids[e.point2D_idx] = id;
    };
    for (uint32_t p = 0; p < (uint32_t)s.pts.size(); p++) {
        std::vector<TrackElement> west, rest, copy;
        for (uint32_t c = 0; c < (uint32_t)s.images(); c++) {
            if (!s.vis[c][p]) continue;
            // Camera J registered last on little support: most of what it sees is free.
            if (seam != Seam::None && c == (uint32_t)J && u(rng) < 0.62) continue;
            const bool revisit = c >= (uint32_t)cams;
            if (revisit && u(rng) >= 0.10) {
                copy.push_back({c, p});
                continue;
            }
            (seam != Seam::None && c < (uint32_t)J ? west : rest).push_back({c, p});
        }
        add(s.pts[p], west);
        add(seam != Seam::None ? east.apply(s.east_pts[p]) : s.pts[p], rest);
        add(s.pts[p] + Vec3{1.5, 0, 0}, copy);
    }
    return m;
}

static bool sameModel(const Reconstruction& a, const Reconstruction& b) {
    if (a.images.size() != b.images.size() || a.points3D.size() != b.points3D.size()) return false;
    for (const auto& kv : a.images) {
        auto it = b.images.find(kv.first);
        if (it == b.images.end()) return false;
        const Pose &p = kv.second.pose, &q = it->second.pose;
        for (int k = 0; k < 9; k++)
            if (p.R[k] != q.R[k]) return false;
        if (p.t.x != q.t.x || p.t.y != q.t.y || p.t.z != q.t.z) return false;
        if (kv.second.point3D_ids != it->second.point3D_ids) return false;
    }
    for (const auto& kv : a.points3D) {
        auto it = b.points3D.find(kv.first);
        if (it == b.points3D.end()) return false;
        const Vec3 &x = kv.second.xyz, &y = it->second.xyz;
        if (x.x != y.x || x.y != y.y || x.z != y.z) return false;
        if (kv.second.track.size() != it->second.track.size()) return false;
    }
    return true;
}

static double turnDeg(const Mat3& a, const Mat3& b) { return rotationAngleDeg(mul(b, transpose(a))); }

// Step and turn across the join, each over the median consecutive one of the track.
static void joinGeometry(const Reconstruction& m, int cams, int J, double& step_ratio,
                         double& kink_deg) {
    std::vector<Vec3> C;
    std::vector<Mat3> R;
    for (int c = 0; c < cams; c++) {
        const Pose& p = m.images.at(c).pose;
        C.push_back(cameraCenter(p));
        R.push_back(p.R);
    }
    std::vector<double> steps;
    for (int c = 0; c + 1 < cams; c++) steps.push_back((C[c + 1] - C[c]).norm());
    const double at = steps[J - 1];
    std::nth_element(steps.begin(), steps.begin() + steps.size() / 2, steps.end());
    step_ratio = at / steps[steps.size() / 2];
    kink_deg = turnDeg(R[J - 1], R[J]);
}

// Metres east/north of a reference as a fix, by the local radii of curvature.
static Geodetic fixAt(double e, double n, double alt) {
    constexpr double lat0 = 42.2, lon0 = -83.6;
    constexpr double a = 6378137.0, f = 1.0 / 298.257223563, e2 = f * (2.0 - f);
    const double p = lat0 * M_PI / 180.0, w = 1.0 - e2 * std::sin(p) * std::sin(p);
    const double M = a * (1.0 - e2) / std::pow(w, 1.5), N = a / std::sqrt(w);
    return {lat0 + n / M * 180.0 / M_PI, lon0 + e / (N * std::cos(p)) * 180.0 / M_PI, alt};
}

using PairSet = std::set<std::pair<uint32_t, uint32_t>>;

static PairSet pairsOf(const std::vector<Mapper::SeamPair>& v) {
    PairSet s;
    for (const Mapper::SeamPair& p : v) s.insert({std::min(p.a, p.b), std::max(p.a, p.b)});
    return s;
}

static const Mapper::SeamPair* find(const std::vector<Mapper::SeamPair>& v, uint32_t a,
                                    uint32_t b) {
    for (const Mapper::SeamPair& p : v)
        if (std::min(p.a, p.b) == a && std::max(p.a, p.b) == b) return &p;
    return nullptr;
}

// Strong pairs across the join, optionally at most `gap` positions apart.
static PairSet across(const Scene& s, int min_matches, int gap = 1 << 30) {
    PairSet out;
    const uint32_t J = (uint32_t)s.join(), cams = (uint32_t)s.spec.cams;
    for (const TwoViewMatches& p : s.db.pairs)
        if (p.image1 < cams && p.image2 < cams && (p.image1 < J) != (p.image2 < J) &&
            p.matches.size() >= (size_t)min_matches &&
            (int)std::max(p.image1, p.image2) - (int)std::min(p.image1, p.image2) <= gap)
            out.insert({std::min(p.image1, p.image2), std::max(p.image1, p.image2)});
    return out;
}

static void offsetSeamDetector(const Scene& sc, const Reconstruction& seam,
                               const MapperOptions& opt) {
    const PairSet all = across(sc, opt.seam_min_matches), within3 = across(sc, opt.seam_min_matches, 3);
    const uint32_t J = (uint32_t)sc.join();
    Mapper m(sc.db, sc.feats, opt);
    size_t strong = 0;
    std::vector<Mapper::SeamPair> judged;
    const auto open = m.openSeams(seam, &strong, &judged);
    std::printf("offset seam, no order: %zu open of %zu strong (%zu candidates), %zu pairs "
                "cross the join, %zu of them 3 apart or fewer\n", open.size(), strong,
                judged.size(), all.size(), within3.size());
    check(all.size() >= 40 && within3.size() == 6, "fixture: the join has strong pairs across it");
    check(pairsOf(open) == all, "no order: exactly the pairs across the join");
    const Mapper::SeamPair* link = find(judged, J - 1, J);
    check(link && link->nbr_common == 0, "the join link shares no neighbour");
    check(link && link->off_depth > 0.15 && link->off_depth < 0.3,
          "the join link's duplicated points sit 2 m off at 10 m depth");
    const Mapper::SeamPair* dup = find(judged, kDupA, kDupB);
    check(!dup, "the diluted pair is not a candidate at 0.25");
    check(!pairsOf(open).count({kDupA, kDupB}), "the diluted pair is not a seam");

    MapperOptions loose = opt;
    loose.seam_weld_frac = 0.5;
    Mapper ml(sc.db, sc.feats, loose);
    std::vector<Mapper::SeamPair> wide;
    const auto open_wide = ml.openSeams(seam, nullptr, &wide);
    dup = find(wide, kDupA, kDupB);
    std::printf("diluted pair: explained %.3f, shared neighbours %d, offset %.3f of depth\n",
                dup ? dup->frac() : -1.0, dup ? dup->nbr_common : -1, dup ? dup->off_depth : -1.0);
    check(dup && dup->frac() > 0.25 && dup->frac() < 0.5,
          "fixture: the diluted pair is explained between 0.25 and 0.5");
    check(dup && dup->off_depth >= 0 && dup->off_depth < 0.10,
          "the diluted pair's junk matches have no coherent offset");
    check(!pairsOf(open_wide).count({kDupA, kDupB}), "a 0.5 bar still leaves the diluted pair");

    MapperOptions ord = opt;
    ord.seam_order_by_name = true;
    Mapper mo(sc.db, sc.feats, ord);
    const auto open_ord = mo.openSeams(seam);
    std::printf("offset seam, file order: %zu open\n", open_ord.size());
    check(pairsOf(open_ord) == within3, "file order: exactly the pairs across the join 3 apart or fewer");
}

// What the weld must leave alone: a join whose duplicates sit too far apart to be a seam, and
// a join across two lenses of one rig frame.
static void notSeams(const Scene& sc, const Reconstruction& seam, const MapperOptions& opt) {
    Mapper m(sc.db, sc.feats, opt);
    std::vector<Mapper::SeamPair> judged;
    const Reconstruction distant = makeModel(sc, Seam::Offset, 8.0);
    const auto open_far = m.openSeams(distant, nullptr, &judged);
    const Mapper::SeamPair* link = find(judged, sc.join() - 1, sc.join());
    std::printf("8 m offset: %zu open, join link offset %.3f of depth\n", open_far.size(),
                link ? link->off_depth : -1.0);
    check(link && link->off_depth > 0.5 && link->nbr_common == 0,
          "fixture: at 8 m the join link is a weak, unshared pair half the depth off");
    check(open_far.empty(), "duplicates further apart than a seam's: nothing is open");

    const auto open = m.openSeams(seam);
    if (open.empty()) {
        check(false, "fixture: the 2 m seam has open pairs");
        return;
    }
    const uint32_t a = open.front().a, b = open.front().b;
    RigTable rigs;
    RigSpec rig;
    rig.name = "pair";
    rig.members.resize(2);
    rig.frames = {{a, b}};
    rigs.rigs.push_back(rig);
    rigs.index(sc.db.images.size());
    Mapper mr(sc.db, sc.feats, opt, {}, &rigs);
    const auto open_rig = mr.openSeams(seam);
    std::printf("one open pair made a rig frame: %zu open (was %zu)\n", open_rig.size(),
                open.size());
    check(!pairsOf(open_rig).count({std::min(a, b), std::max(a, b)}) &&
              open_rig.size() + 1 == open.size(),
          "rig mates: two lenses of one frame are never a seam, and nothing else changes");
}

// The weld undone: the east half sees a copy of the scene moved 0.6 m per point at random, so
// the join still opens (a coherent 2 m apart) but no fused point can satisfy both sides.
static void undoneWeld(const MapperOptions& opt) {
    Spec spec;
    spec.east_copy = 0.6;
    const Scene sc = makeScene(spec);
    const Reconstruction seam = makeModel(sc, Seam::Offset);
    Mapper m(sc.db, sc.feats, opt);
    Mapper::SeamStats st;
    const Reconstruction out = m.weldSeams(seam, &st);
    std::printf("perturbed copy: %zu open, %zu point(s) fused, %.0f%% of the ties held, images "
                "%u -> %u, reproj %.3f -> %.3f px, undone: %s\n", st.open.size(), st.points,
                100.0 * st.held, st.images_before, st.images_after, st.reproj_before,
                st.reproj_after, st.undone ? st.undone : "no");
    check(!st.open.empty() && st.points > 0, "fixture: the perturbed copy opens and is fused");
    check(st.undone != nullptr, "perturbed copy: the weld does not hold and is undone");
    check(sameModel(out, seam), "perturbed copy: the model comes back exactly as it went in");

    Mapper::SeamStats ok;
    ok.open.resize(2);
    ok.after.resize(2);
    ok.open[0].explained = 3;
    ok.after[0].explained = 40;
    ok.open[1].explained = 0;
    ok.after[1].explained = 25;
    ok.images_before = ok.images_after = 24;
    ok.held = 0.8;
    ok.reproj_before = ok.reproj_after = 0.5;
    check(Mapper::weldFailure(ok) == nullptr, "weldFailure: a weld that holds is kept");
    Mapper::SeamStats bad = ok;
    bad.images_after = 23;
    check(Mapper::weldFailure(bad) != nullptr, "weldFailure: a dropped image undoes it");
    bad = ok;
    bad.after[1].explained = 0;
    bad.open[1].explained = 9;
    check(Mapper::weldFailure(bad) != nullptr, "weldFailure: a pair left less tied undoes it");
    bad = ok;
    bad.held = 0.3;
    check(Mapper::weldFailure(bad) != nullptr, "weldFailure: fused points that do not hold undo it");
    bad = ok;
    bad.reproj_after = 0.6;
    check(Mapper::weldFailure(bad) != nullptr, "weldFailure: a reprojection 20 % worse undoes it");

    // The bookkeeping after a refine that dropped an image must not throw.
    Reconstruction gone = seam;
    const TwoViewMatches* pr = nullptr;
    for (const TwoViewMatches& p : sc.db.pairs)
        if (p.image1 == 0) pr = &p;
    if (pr) gone.images.erase(pr->image2);
    Mapper::SeamPair sp;
    sp.a = 0;
    sp.b = pr ? pr->image2 : 1;
    check(pr && m.explainedMatches(gone, *pr) == 0 && std::isnan(Mapper::pairTurnDeg(gone, sp)),
          "a pair whose image the refine dropped reads 0 ties and no turn, without throwing");
}

static void loopDetector(const MapperOptions& opt) {
    Spec spec;
    spec.revisits = {(int)kLoopOf, (int)kLoopOf + 1};
    const Scene sc = makeScene(spec);
    const Reconstruction m0 = makeModel(sc, Seam::None);
    const uint32_t loop = (uint32_t)spec.cams;
    Mapper m(sc.db, sc.feats, opt);
    std::vector<Mapper::SeamPair> judged;
    const auto open = m.openSeams(m0, nullptr, &judged);
    const Mapper::SeamPair* lp = find(judged, kLoopOf, loop);
    std::printf("loop revisit: %zu open, %zu candidates; loop pair explained %.3f, shared "
                "neighbours %d, offset %.3f of depth\n", open.size(), judged.size(),
                lp ? lp->frac() : -1.0, lp ? lp->nbr_common : -1, lp ? lp->off_depth : -1.0);
    check(lp && lp->off_depth >= 0.10,
          "fixture: the loop pair is a candidate whose copy sits 1.5 m off");
    check(lp && lp->nbr_common > 1, "the loop pair shares its neighbours");
    check(open.empty(), "loop revisit: nothing is open");
}

// The east half turns 1.5 deg about the join and does not move: no coherent offset, so only
// the capture-order branch can see it. Cameras turn 0.02 deg a position near the join and
// 0.6 elsewhere, so only a local window reads the join as a kink.
static void turnSeamDetector(const MapperOptions& opt) {
    Spec spec;
    spec.cams = 48;
    const int J = spec.cams / 2;
    spec.yaw_deg = [J](int c) {
        double y = 0;
        for (int k = 1; k <= c; k++) y += std::abs(k - J) <= 8 ? 0.02 : 0.6;
        return y;
    };
    const Scene sc = makeScene(spec);
    const Reconstruction seam = makeModel(sc, Seam::Turn);
    const PairSet within3 = across(sc, opt.seam_min_matches, 3);

    std::vector<double> rot;
    for (int c = 0; c + 1 < spec.cams; c++)
        rot.push_back(turnDeg(seam.images.at(c).pose.R, seam.images.at(c + 1).pose.R));
    const double at = rot[J - 1];
    std::nth_element(rot.begin(), rot.begin() + rot.size() / 2, rot.end());
    double step, kink;
    joinGeometry(seam, spec.cams, J, step, kink);
    std::printf("turn seam: join turns %.2f deg against a track median %.2f; step x%.2f\n", at,
                rot[rot.size() / 2], step);
    check(at / rot[rot.size() / 2] < 10, "fixture: against the whole track the join is no kink");
    check(step < 1.5, "fixture: the join does not step");

    Mapper m(sc.db, sc.feats, opt);
    std::vector<Mapper::SeamPair> judged;
    const auto open = m.openSeams(seam, nullptr, &judged);
    const Mapper::SeamPair* link = find(judged, (uint32_t)J - 1, (uint32_t)J);
    std::printf("turn seam, no order: %zu open, %zu candidates; join link offset %.3f\n",
                open.size(), judged.size(), link ? link->off_depth : -1.0);
    check(link && link->nbr_common == 0 && link->off_depth < 0.10,
          "fixture: the join link is a candidate with no coherent offset");
    check(open.empty(), "turn seam, no order: nothing is open");

    MapperOptions ord = opt;
    ord.seam_order_by_name = true;
    Mapper mo(sc.db, sc.feats, ord);
    std::vector<Mapper::SeamPair> jo;
    const auto open_ord = mo.openSeams(seam, nullptr, &jo);
    link = find(jo, (uint32_t)J - 1, (uint32_t)J);
    std::printf("turn seam, file order: %zu open of %zu 3 apart or fewer; join link gap %d, "
                "kink ratio %.1f\n", open_ord.size(), within3.size(), link ? link->gap : -1,
                link ? link->kink_ratio : -1.0);
    check(within3.size() == 6, "fixture: six strong pairs cross the join 3 apart or fewer");
    check(pairsOf(open_ord) == within3, "file order: the turn is open across the join");

    std::vector<std::string> names;
    for (const ImageEntry& im : sc.db.images) names.push_back(im.name);
    const SequenceTable seqs = buildSequenceTable(names, {SequenceDef{{""}}});
    Mapper ms(sc.db, sc.feats, opt, {}, nullptr, &seqs);
    check(pairsOf(ms.openSeams(seam)) == within3, "declared sequence: the turn is open across the join");
}

static int body(int argc, char** argv) {
    int gps_cap = 3;
    MapperOptions opt;
    opt.verbose = false;
    opt.focal = 1200;
    opt.focal_trials = 0;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--device" && i + 1 < argc) opt.device = std::stoi(argv[++i]);
        else if (a == "--verbose") opt.verbose = true;
        else if (a == "--gps-cap" && i + 1 < argc) gps_cap = std::stoi(argv[++i]);
    }
    Spec spec;
    spec.dilute = true;
    const Scene sc = makeScene(spec);
    const int cams = spec.cams, J = sc.join();
    const Reconstruction healthy = makeModel(sc, Seam::None);
    const Reconstruction seam = makeModel(sc, Seam::Offset);

    check(Mapper::medianOf({10, 1, 3, 2}) == 2.5 && Mapper::medianOf({5, 1, 3}) == 3,
          "the offset's and the kink's median average the middle two of an even count");
    offsetSeamDetector(sc, seam, opt);
    notSeams(sc, seam, opt);
    loopDetector(opt);
    turnSeamDetector(opt);
    {
        double r0, k0;
        joinGeometry(seam, cams, J, r0, k0);
        Mapper m(sc.db, sc.feats, opt);
        Mapper::SeamStats st;
        const Reconstruction out = m.weldSeams(seam, &st);
        double r1, k1;
        joinGeometry(out, cams, J, r1, k1);
        double worst = 1;
        for (const Mapper::SeamPair& p : st.after) worst = std::min(worst, p.frac());
        std::printf("weld: %zu pair(s), %zu point(s) fused; join step x%.2f -> x%.2f, kink "
                    "%.2f -> %.3f deg; weakest welded pair after %.3f; reproj %.3f -> %.3f px; "
                    "%d BA round(s)\n", st.open.size(), st.points, r0, r1, k0, k1, worst,
                    st.reproj_before, st.reproj_after, st.rounds);
        check(r0 > 2.0 && k0 > 1.4, "fixture: the join steps and kinks before the weld");
        check(st.points > 0 && st.after.size() == st.open.size(), "weld: the open pairs are fused");
        check(std::fabs(r1 - 1.0) <= 0.1, "weld: the step at the join closes");
        check(k1 < 0.3, "weld: the kink at the join closes");
        check(st.reproj_after > 0 && st.reproj_after < 1.0, "weld: the welded model reprojects");
        check(out.numRegistered() == (uint32_t)cams, "weld: every image stays registered");
        // Fuse only, no retriangulation (that re-tracks the whole model and cost a canopy drone capture
        // +0.086 px): a forced second round pulls a large kink further than one round does.
        check(st.rounds == 2, "weld: the refine always runs a forced second round");
        check(worst < 0.5, "weld: the forced second round does not retriangulate");
        std::printf("weld holds: %.0f%% of the ties, images %u -> %u, undone: %s\n",
                    100.0 * st.held, st.images_before, st.images_after,
                    st.undone ? st.undone : "no");
        check(!st.undone && st.held >= 0.9, "weld: the fused points hold, and the weld is kept");
    }
    {
        // --metric-gps full at the true centres, each round capped at a few LM iterations, as
        // a canopy drone capture's final solves are: the fused points must survive a round that ends short.
        std::vector<std::optional<Geodetic>> fixes(cams);
        for (int c = 0; c < cams; c++)
            fixes[c] = fixAt(sc.centres[c].x, sc.centres[c].z, 250.0 - sc.centres[c].y);
        SensorPriorOptions po;
        po.trusted_position = true;
        ExifGpsPriors gps(fixes, po);
        MapperOptions capped = opt;
        capped.ba_final_prior_max_iters = gps_cap;
        Mapper m(sc.db, sc.feats, capped, {}, nullptr, nullptr, &gps);
        Mapper::SeamStats st;
        const Reconstruction out = m.weldSeams(seam, &st);
        double r1, k1;
        joinGeometry(out, cams, J, r1, k1);
        std::printf("weld under GPS, %d LM its a round: %zu point(s) fused; join step x%.2f, "
                    "kink %.3f deg\n", gps_cap, st.points, r1, k1);
        check(std::fabs(r1 - 1.0) <= 0.1, "weld, capped under GPS: the step at the join closes");
        check(k1 < 0.3, "weld, capped under GPS: the kink at the join closes");
        check(st.rounds == 2,
              "weld, capped under GPS: a real few-iteration round still forces a second");
    }
    undoneWeld(opt);
    {
        MapperOptions off = opt;
        off.seam_weld_frac = 0;
        Mapper m(sc.db, sc.feats, off);
        Mapper::SeamStats st;
        const Reconstruction out = m.weldSeams(seam, &st);
        check(st.open.empty() && sameModel(out, seam),
              "off: a bar of 0 finds nothing and returns the model untouched");
    }
    {
        Mapper m(sc.db, sc.feats, opt);
        Mapper::SeamStats st;
        const Reconstruction out = m.weldSeams(healthy, &st);
        std::printf("healthy model: %zu open of %zu strong\n", st.open.size(), st.strong);
        check(st.strong > 100, "fixture: the healthy model has strong pairs to judge");
        check(st.open.empty() && sameModel(out, healthy),
              "healthy: no open seam, and the model comes back untouched");
    }

    std::printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails, fails == 1 ? "" : "s");
    return fails;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, body); }
