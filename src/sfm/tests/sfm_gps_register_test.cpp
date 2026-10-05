// The mapper's GPS check on a registration (Mapper::gpsCheck): a scripted
// source says how far each new registration lands from its fix, so the trigger
// for an early global BA and the refusal of a wrong-place PnP are exercised
// apart from any real fit. The scene is synthetic, the BA real (GPU).
//
//   sfm_gps_register_test [--device N] [--verbose]
//
// Prints FAIL lines and returns the count.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Log.h"
#include "sfm/core/Model.h"
#include "sfm/core/PriorSource.h"
#include "sfm/core/Rig.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/MetricGauge.h"
#include "sfm/tests/SyntheticRegister.h"
#include "sfm/tests/SyntheticRig.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;
static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        fails++;
    }
}

using namespace synth_reg;

// Metres off the fix for each image's FIRST check, in check order; a retry and
// anything past the script read 1 m, `always` pins an image's. No factors, so solves
// are unconstrained. `events` logs 'P' per check and 'F' per factors() call.
class ScriptedGps : public PriorSource {
public:
    explicit ScriptedGps(std::vector<double> script) : script_(std::move(script)) {}
    bool has(uint32_t img) const override { return fix.empty() || fix[img]; }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override { return false; }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        events += 'F';
        PosePriors p;
        p.gps.ok = frame_ok && imgs.size() >= 2;
        p.gps.gate = 5.0;
        p.gps.t = {(double)imgs.size(), 0.0, 0.0};  // which fit a check was made through
        return p;
    }
    bool positionError(uint32_t img, const Pose&, const GpsFrame& f, double& m) const override {
        if (!f.ok || !has(img)) return false;
        fits.push_back(f.t.x);
        const int c = calls[img]++;
        if (always.count(img)) {
            m = always.at(img);
        } else if (c == 0 && next < script_.size()) {
            at[next] = events.size();
            image_at[next] = img;
            m = script_[next++];
        } else {
            m = 1.0;
        }
        events += 'P';
        return true;
    }
    // Whether the event right after script entry k's check was a solve.
    bool solvedAfter(size_t k) const {
        auto it = at.find(k);
        return it != at.end() && it->second + 1 < events.size() && events[it->second + 1] == 'F';
    }

    std::vector<char> fix;
    std::map<uint32_t, double> always;
    bool frame_ok = true;
    mutable std::string events;
    mutable std::vector<double> fits;              // per check: images in the fit it used
    mutable std::map<uint32_t, int> calls;
    mutable std::map<size_t, size_t> at;           // script entry -> its 'P' in events
    mutable std::map<size_t, uint32_t> image_at;   // script entry -> image
private:
    std::vector<double> script_;
    mutable size_t next = 0;
};

struct Run {
    Mapper::PriorStats st;
    uint32_t registered = 0;
    size_t models = 0;
};

static Run runWith(const Scene& s, MapperOptions opt, ScriptedGps& gps,
                   Reconstruction* model = nullptr) {
    Mapper m(s.db, s.feats, opt, {}, nullptr, nullptr, &gps);
    std::vector<Reconstruction> models = m.run();
    if (model && !models.empty()) *model = models.front();
    Run r;
    r.st = m.priorStats();
    r.registered = models.empty() ? 0 : models.front().numRegistered();
    r.models = models.size();
    return r;
}

// Entries k with value v, the rest 1 m.
static std::vector<double> script(size_t n, std::map<size_t, double> at) {
    std::vector<double> s(n, 1.0);
    for (const auto& kv : at) s[kv.first] = kv.second;
    return s;
}

// `full` cut down to images lo..hi, tracks with them: a model the assembler holds.
static Reconstruction subModel(const Reconstruction& full, uint32_t lo, uint32_t hi) {
    Reconstruction r = full;
    for (auto it = r.images.begin(); it != r.images.end();)
        it = it->first < lo || it->first > hi ? r.images.erase(it) : std::next(it);
    for (auto it = r.points3D.begin(); it != r.points3D.end();) {
        auto& tr = it->second.track;
        tr.erase(std::remove_if(tr.begin(), tr.end(),
                                [&](const TrackElement& e) { return e.image_id < lo || e.image_id > hi; }),
                 tr.end());
        it = tr.size() < 2 ? r.points3D.erase(it) : std::next(it);
    }
    return r;
}

// Images `after` holds that `before` did not, and whether every one was checked.
static bool allChecked(const Reconstruction& before, const Reconstruction& after,
                       const ScriptedGps& g, uint32_t& added) {
    bool ok = true;
    added = 0;
    for (const auto& kv : after.images) {
        if (!kv.second.registered) continue;
        auto b = before.images.find(kv.first);
        if (b != before.images.end() && b->second.registered) continue;
        added++;
        ok = ok && g.calls.count(kv.first) && g.calls.at(kv.first) >= 1;
    }
    return ok && added > 0;
}

// The assembly path (Assemble.h): growth by PnP between joint solves, and the
// continuation of a finished model, each on a model the mapper adopted rather
// than built, so no global solve of its own has run before it registers.
static void testAssembly(const Scene& sc, const Reconstruction& full, MapperOptions opt) {
    const int M = (int)sc.db.images.size();
    {
        ScriptedGps g(script(M, {{0, 25.0}}));
        Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &g);
        std::vector<Reconstruction> models{subModel(full, 0, 19)};
        const Reconstruction before = models[0];
        std::vector<char> dirty(1, 0);
        size_t rejected = 0;
        const size_t reg = detail::growModels(m, models, dirty, {}, 0.25, 10, rejected);
        uint32_t added = 0;
        const bool every = allChecked(before, models[0], g, added);
        std::printf("growth: %zu registered, %zu check(s), refused %u | %s\n", reg, g.at.size(),
                    m.priorStats().gps_refused, g.events.c_str());
        check(every, "assembly: every image growth registers is checked against the GPS");
        check(m.priorStats().gps_refused == 1, "assembly: growth refuses a wrong-place PnP");
    }
    {
        ScriptedGps g(script(M, {}));
        Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &g);
        std::vector<Reconstruction> models{subModel(full, 0, 16), subModel(full, 26, 39)};
        const uint32_t na = models[0].numRegistered(), nb = models[1].numRegistered();
        std::vector<char> dirty(2, 0);
        size_t rejected = 0;
        detail::growModels(m, models, dirty, {}, 0.25, 4, rejected);
        size_t k = 0, a = 0, b = 0;
        while (k < g.fits.size() && g.fits[k] == na) k++, a++;
        while (k < g.fits.size() && g.fits[k] == nb) k++, b++;
        std::printf("two models (%u, %u images): %zu check(s) through a %u-image fit, then %zu "
                    "through a %u-image one, %zu other\n", na, nb, a, na, b, nb,
                    g.fits.size() - k);
        check(na != nb && a > 0 && b > 0 && k == g.fits.size(),
              "assembly: each model is checked through its own fit");
    }
    {
        // No global solve falls due inside this pass, so only a trigger stops it.
        MapperOptions o = opt;
        o.ba_growth_ratio = 3.0;
        for (const bool drift : {false, true}) {
            ScriptedGps g(drift ? script(M, {{10, 6.0}, {11, 6.0}, {12, 6.0}}) : script(M, {}));
            Mapper m(sc.db, sc.feats, o, {}, nullptr, nullptr, &g);
            std::vector<Reconstruction> models{subModel(full, 0, 14)};
            std::vector<char> dirty(1, 0);
            size_t rejected = 0;
            const size_t reg = detail::growModels(m, models, dirty, {}, 0.25, 40, rejected);
            std::printf("growth %s: %zu registered, ba %u, out %u\n",
                        drift ? "drifting at 11-13" : "in gate", reg, m.priorStats().gps_ba,
                        m.priorStats().gps_out);
            if (!drift)
                check(reg == (size_t)M - 15 && m.priorStats().gps_ba == 0,
                      "fixture: growth in the gate takes every image");
            else
                check(reg == 13 && m.priorStats().gps_ba == 1,
                      "assembly: a drifting run stops growth for the joint solve");
        }
    }
    {
        ScriptedGps g(script(M, {{0, 25.0}}));
        Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &g);
        const Reconstruction part = subModel(full, 0, 29);
        Mapper::GrowStats gs;
        const Reconstruction back = m.continueFrom(part, &gs);
        uint32_t added = 0;
        const bool every = allChecked(part, back, g, added);
        std::printf("continuation: %u added, refused %u | %s\n", added,
                    m.priorStats().gps_refused, g.events.c_str());
        check(every, "continuation: every image it registers is checked against the GPS");
        check(m.priorStats().gps_refused == 1, "continuation: refuses a wrong-place PnP");
    }
    {
        // A misplaced image makes the audit repair, and a repair re-grows the model.
        ScriptedGps g(script(M, {}));
        Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &g);
        Reconstruction bad = subModel(full, 6, 29);
        Image moved = full.images.at(5);
        moved.pose.R = mul(angleAxisToRotation({0.0, 0.6, 0.0}), moved.pose.R);
        moved.point3D_ids.assign(moved.points2D.size(), kInvalidPoint3D);
        bad.images[5] = moved;
        Mapper::AuditStats as;
        const Reconstruction back = m.audit(bad, &as);
        uint32_t added = 0;
        const bool every = allChecked(bad, back, g, added);
        std::printf("audit: %u contradicted, %u added | %s\n", as.unsupported, added,
                    g.events.c_str());
        check(as.unsupported >= 1 && every, "audit: every image its repair registers is checked");
    }
}

// Each camera between a Cauchy(7.815) centre 10 m along x and a quadratic one where
// it stands, 20 times over: every step clears rtol, and it settles after 500 or more
// iterations. `n` 3 states the same factor as an inertial triple.
class PullGps : public PriorSource {
public:
    // `metres` > 0 states the factors in a GPS frame of that many metres per unit.
    explicit PullGps(int n, double metres = 0) : n_(n), metres_(metres) {}
    bool has(uint32_t) const override { return true; }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override { return false; }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        PosePriors p;
        p.huber = 1e9;
        for (const PosedImage& im : imgs) {
            PriorCentre c;
            c.n = n_;
            for (int k = 0; k < 3; k++) c.img[k] = im.image;
            for (int k = 1; k < 3; k++) c.A[k] = Mat3{};
            for (int r = 0; r < 20; r++) {
                c.b = cameraCenter(im.pose) + Vec3{10.0, 0.0, 0.0};
                c.cauchy = 7.815;
                p.centres.push_back(c);
                c.b = cameraCenter(im.pose);
                c.cauchy = 0;
                p.centres.push_back(c);
            }
        }
        p.gps.ok = metres_ > 0;
        p.gps.A = mat3Identity();
        for (int k = 0; k < 9; k++) p.gps.A[k] *= metres_;
        return p;
    }

private:
    int n_;
    double metres_;
};

// LM iterations per solve of a finishing refinement, and how many solves it ran.
static double finalIters(const Scene& sc, const Reconstruction& model, MapperOptions opt, int n,
                         long& solves, double metres = 0) {
    PullGps g(n, metres);
    Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &g);
    const long b0 = g_map_prof.n_ba, i0 = g_map_prof.n_ba_iters;
    m.refine(model);
    solves = g_map_prof.n_ba - b0;
    return solves ? (double)(g_map_prof.n_ba_iters - i0) / solves : 0;
}

// COLMAP's global BA allows 50 LM iterations; the mapper's own cap is 25.
// The model is put in its first image's frame, at exactly the zero angle-axis and
// origin a seed pair starts from.
static void testFinalCap(const Scene& sc, const Reconstruction& full, MapperOptions opt) {
    Reconstruction model = full;
    Image& seed = model.images.begin()->second;
    Sim3 to;
    to.R = seed.pose.R;
    to.t = seed.pose.t;
    for (auto& kv : model.images) kv.second.pose = transformPose(to, kv.second.pose);
    for (auto& kv : model.points3D) kv.second.xyz = transformPoint(to, kv.second.xyz);
    seed.pose = {mat3Identity(), {0, 0, 0}};
    const Vec3 aa = rotationToAngleAxis(seed.pose.R);
    check(aa.x == 0 && aa.y == 0 && aa.z == 0, "fixture: an image at exactly the zero angle-axis");
    long s1 = 0, s3 = 0, sa = 0;
    const double abs = finalIters(sc, model, opt, 1, s1);
    const double tri = finalIters(sc, model, opt, 3, s3);
    MapperOptions atom = opt;
    atom.ba_final_tight = false;
    atom.ba_growth_rtol = 0;
    const double loose = finalIters(sc, model, atom, 1, sa);
    std::printf("final cap: absolute centres %.1f its x %ld, inertial triples %.1f x %ld, "
                "not a final pass %.1f x %ld\n", abs, s1, tri, s3, loose, sa);
    check(s1 > 0 && abs == 50, "final cap: a finishing solve on GPS centres runs 50 iterations");
    check(s3 > 0 && tri == 25, "final cap: a finishing solve on inertial triples keeps 25");
    check(sa > 0 && loose == 25, "final cap: a solve that is not a final pass keeps 25");

    // A tolerance no norm can exceed stops exactly the solves the gate names, at once.
    MapperOptions any = opt, anyAtom = atom;
    any.ba_final_prior_gradient_tol = anyAtom.ba_final_prior_gradient_tol = 1e300;
    const double gAbs = finalIters(sc, model, any, 1, s1);
    const double gTri = finalIters(sc, model, any, 3, s3);
    const double gLoose = finalIters(sc, model, anyAtom, 1, sa);
    std::printf("gradient stop: absolute centres %.1f its, inertial triples %.1f, not a final "
                "pass %.1f\n", gAbs, gTri, gLoose);
    check(s1 > 0 && gAbs == 0, "gradient stop: a finishing solve on GPS centres stops on it");
    check(s3 > 0 && gTri == 25, "gradient stop: a finishing solve on inertial triples does not");
    check(sa > 0 && gLoose == 25, "gradient stop: a solve that is not a final pass does not");
    // At 1e-12 m per unit every length gradient is 1e12 times larger in metres,
    // so a 1e12 tolerance stops the solve only if that scale never reached it.
    MapperOptions tight = opt;
    tight.ba_final_prior_gradient_tol = 1e12;
    const double unscaled = finalIters(sc, model, tight, 1, s1);
    const double scaled = finalIters(sc, model, tight, 1, s1, 1e-12);
    std::printf("gradient stop at 1e12: GPS frame absent %.1f its, at 1e-12 m per unit %.1f\n",
                unscaled, scaled);
    check(unscaled == 0, "fixture: a 1e12 tolerance stops the solve in model units");
    check(scaled == 50, "gradient stop: lengths are measured in the GPS frame's metres");
}

// ---- the rig's registrations (Mapper::registerFrame) ------------------------

// Each image's true centre, `k` metres per scene unit, for the images `fix` names;
// the frame a similarity fitted to the posed ones, as a real source's is. No
// factors, so solves stay unconstrained. The check reads the pose it is handed.
class TruthGps : public PriorSource {
public:
    TruthGps(const synth_rig::RigScene& sc, double k, std::vector<char> fix)
        : k_(k), fix_(std::move(fix)) {
        for (const Pose& p : sc.gt) centre_.push_back(cameraCenter(p));
    }
    bool has(uint32_t img) const override { return img < fix_.size() && fix_[img]; }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override { return false; }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    bool position(uint32_t img, Vec3& p) const override {
        if (!has(img)) return false;
        p = centre_[img] * k_;
        return true;
    }
    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        events += 'F';
        std::vector<Vec3> src, dst;
        for (const PosedImage& im : imgs) {
            Vec3 x;
            if (!position(im.image, x)) continue;
            src.push_back(cameraCenter(im.pose));
            dst.push_back(x);
        }
        PosePriors p;
        Sim3 T;
        if (src.size() < 3 || !estimateSim3(src, dst, T)) return p;
        p.gps.ok = true;
        for (int k = 0; k < 9; k++) p.gps.A[k] = T.R[k] * T.scale;
        p.gps.t = T.t;
        p.gps.gate = 5.0;
        return p;
    }
    bool positionError(uint32_t img, const Pose& pose, const GpsFrame& f,
                       double& m) const override {
        if (!PriorSource::positionError(img, pose, f, m)) return false;
        calls[img]++;
        worst = std::max(worst, m);
        events += 'P';
        return true;
    }

    mutable std::string events;
    mutable std::map<uint32_t, int> calls;
    mutable double worst = 0;

private:
    double k_;
    std::vector<char> fix_;
    std::vector<Vec3> centre_;
};


// Frames placed whole after a fit, and how many of them one check covered exactly.
template <class Calls>
static void checkedOnce(const RigRun& r, uint32_t M, const Calls& calls, size_t& frames,
                        size_t& once) {
    frames = once = 0;
    auto n = [&](uint32_t i) { return calls.count(i) ? calls.at(i) : 0; };
    for (size_t k = 0; k < r.placed.size(); k++) {
        if (!r.fitted[k]) continue;
        const uint32_t f = r.placed[k] % M;
        frames++;
        once += n(f) + n(M + f) == 1;
    }
}

static void testRig(MapperOptions opt) {
    // Frames 0-7, every fourth after and the three with a blind cam1 are whole, so the
    // rig calibrates early; the rest are weak, and wait for the rig to place them.
    std::vector<char> weak(40, 0);
    for (int f = 8; f < 37; f++) weak[f] = f % 4 != 0;
    const synth_rig::RigScene sc = synth_rig::makeRigScene(40, 37, weak);
    const uint32_t M = (uint32_t)sc.M, n = 2 * M;
    const RigTable rigs = buildRigTable(sc.names, {RigDef{"rig", {{"cam0"}, {"cam1"}}}});

    ScriptedGps base(script(n, {}));
    const RigRun r0 = runRig(sc, rigs, opt, base, base.events);
    size_t frames = 0, once = 0;
    checkedOnce(r0, M, base.calls, frames, once);
    const size_t checks = (size_t)std::count(base.events.begin(), base.events.end(), 'P');
    std::printf("rig in gate: %u/%u registered, %zu model(s), %zu frame(s) placed whole after a "
                "fit, %zu checked once | checked %u, %zu checks\n", r0.model.numRegistered(), n,
                r0.models, frames, once, r0.st.gps_checked, checks);
    check(r0.model.numRegistered() == n && r0.models == 1, "rig fixture: every image registers");
    check(frames >= 20, "rig fixture: the rig places frames after the GPS fit");
    check(once == frames, "rig: every frame the rig places is checked, once");
    check(r0.st.gps_checked == checks && checks > 0, "rig: gps_checked counts every check");

    // A frame four radii off, every time it is checked: it never comes in, and
    // nothing of it is committed before the check refuses it.
    {
        size_t pick = 0;
        for (size_t k = 0, seen = 0; k < r0.placed.size(); k++)
            if (r0.fitted[k] && seen++ == frames / 2) pick = k;
        const uint32_t f = r0.placed.empty() ? 0 : r0.placed[pick] % M;
        ScriptedGps farGps(script(n, {}));
        farGps.always = {{f, 25.0}, {M + f, 25.0}};
        const RigRun r = runRig(sc, rigs, opt, farGps, farGps.events);
        std::printf("rig, frame %u at 25 m: %u/%u registered, frame in: %d %d | refused %u, gyro "
                    "refused %u\n", f, r.model.numRegistered(), n, (int)registered(r.model, f),
                    (int)registered(r.model, M + f), r.st.gps_refused, r.st.refused);
        check(!registered(r.model, f) && !registered(r.model, M + f),
              "rig: a refused frame leaves no image registered");
        check(r.st.gps_refused >= 1 && r.st.refused == 0,
              "rig: the refusal is counted as the GPS's, not the gyro's");
        check(r.model.numRegistered() == n - 2, "rig: every other frame registers");
    }

    // Trigger runs start at script entry j, past ten registrations, on a stretch of
    // frames the rig placed whole; the trigger's own line names where it fired.
    std::vector<char> whole(M, 0);
    for (size_t q = 0; q < r0.placed.size(); q++)
        if (r0.fitted[q]) whole[r0.placed[q] % M] = 1;
    auto rigStretch = [&](size_t lo, size_t hi) {
        for (size_t q = lo; q <= hi; q++)
            if (!base.image_at.count(q) || !whole[base.image_at.at(q) % M]) return false;
        return true;
    };
    size_t j = 12;
    while (j + 14 < checks && !rigStretch(j - 4, j + 14)) j++;
    std::printf("rig trigger runs start at script entry %zu of %zu\n", j, checks);
    check(j + 14 < checks && rigStretch(j - 4, j + 14),
          "rig fixture: a stretch of whole-frame registrations for the trigger runs");
    MapperOptions talk = opt;
    talk.verbose = true;
    auto fired = [&](const RigRun& r, const ScriptedGps& g) {
        std::vector<long> at;
        for (uint32_t img : r.triggers)
            for (const auto& kv : g.image_at)
                if (kv.second == img) at.push_back((long)kv.first);
        return at;
    };

    ScriptedGps three(script(n, {{j - 2, 6.0}, {j - 1, 6.0}, {j, 6.0}}));
    const RigRun r1 = runRig(sc, rigs, talk, three, three.events);
    const std::vector<long> t1 = fired(r1, three);
    std::printf("rig, three at 6 m: ba %u out %u, triggers at %s\n", r1.st.gps_ba, r1.st.gps_out,
                t1.empty() ? "-" : std::to_string(t1[0]).c_str());
    check(t1.size() == 1 && t1[0] == (long)j && r1.st.gps_out == 3,
          "rig: three frames in a row beyond the radius trigger a BA");

    ScriptedGps alt(script(n, {{j - 4, 6.0}, {j - 2, 6.0}, {j, 6.0}}));
    const RigRun r2 = runRig(sc, rigs, talk, alt, alt.events);
    std::printf("rig, 6,1,6,1,6: ba %u out %u, %zu trigger(s)\n", r2.st.gps_ba, r2.st.gps_out,
                r2.triggers.size());
    check(r2.triggers.empty() && r2.st.gps_out == 3, "rig: an in-radius frame breaks the run");

    // Every check from j-2 on beyond the radius: after the triggered BA the run starts
    // over, and the next trigger waits for ten registrations -- frames, not images.
    std::map<size_t, double> drift_at;
    for (size_t q = j - 2; q <= j + 14; q++) drift_at[q] = 6.0;
    ScriptedGps drift(script(n, drift_at));
    const RigRun r3 = runRig(sc, rigs, talk, drift, drift.events);
    const std::vector<long> t3 = fired(r3, drift);
    std::printf("rig, drifting from %zu: triggers at", j);
    for (long t : t3) std::printf(" %ld", t);
    std::printf("\n");
    check(t3.size() >= 2 && t3[0] == (long)j && t3[1] - t3[0] == 11,
          "rig: the trigger's rate limit counts frames, not images");

    // Which pose is checked: the lenses 12 m apart against a 5 m gate, with the
    // fixes on one lens only, so every frame is checked through that lens.
    const double k_m = 12.0 / sc.ext.t.norm();
    check(sc.ext.t.norm() * k_m >= 2 * 5.0, "fixture: the lenses sit two gates apart");
    size_t no_fix_candidates = 0;
    for (const uint32_t lens : {0u, 1u}) {
        std::vector<char> fix(n, 0);
        for (uint32_t f = 0; f < M; f++) fix[lens * M + f] = 1;
        TruthGps truth(sc, k_m, fix);
        const RigRun r = runRig(sc, rigs, opt, truth, truth.events);
        size_t fr = 0, on = 0;
        checkedOnce(r, M, truth.calls, fr, on);
        for (size_t q = 0; q < r.placed.size(); q++)
            no_fix_candidates += r.fitted[q] && !fix[r.placed[q]];
        std::printf("rig, fixes on cam%u only: %u/%u registered, worst %.2f m | checked %u, out "
                    "%u, refused %u | %zu frame(s) placed whole, %zu checked once\n", lens,
                    r.model.numRegistered(), n, truth.worst, r.st.gps_checked, r.st.gps_out,
                    r.st.gps_refused, fr, on);
        check(fr >= 20 && on == fr,
              "rig: a frame is checked once, through the lens that has a position");
        check(r.st.gps_checked >= fr && r.st.gps_out == 0 && r.st.gps_refused == 0,
              "rig: each lens is checked at its own camera pose");
    }
    check(no_fix_candidates > 0, "fixture: some frame's candidate lens has no position");
}

static int body(int argc, char** argv) {
    MapperOptions opt;
    opt.verbose = false;
    opt.focal = 1200;
    opt.focal_trials = 0;   // no bootstrap growth: one pass consumes the script
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--device" && i + 1 < argc) opt.device = std::stoi(argv[++i]);
        else if (a == "--verbose") opt.verbose = true;
    }
    const int M = 40;
    const Scene sc = makeScene(M);
    const size_t n = M;

    ScriptedGps base(script(n, {}));
    Reconstruction full;
    const Run r0 = runWith(sc, opt, base, &full);
    std::printf("in gate: %u/%d registered, %zu model(s), %zu checks | ba %u refused %u out %u\n  %s\n",
                r0.registered, M, r0.models, base.at.size(), r0.st.gps_ba, r0.st.gps_refused,
                r0.st.gps_out, base.events.c_str());
    check(r0.registered == (uint32_t)M && r0.models == 1, "in gate: every image registers");
    check(base.at.size() >= 30, "in gate: the registrations are checked");
    check(r0.st.gps_ba == 0 && r0.st.gps_refused == 0 && r0.st.gps_out == 0,
          "in gate: no trigger, no refusal");
    const size_t checks0 = (size_t)std::count(base.events.begin(), base.events.end(), 'P');
    std::printf("checked %u of %zu checks\n", r0.st.gps_checked, checks0);
    check(r0.st.gps_checked == checks0, "gps_checked: one per check made");

    ScriptedGps unfitted(script(n, {}));
    unfitted.frame_ok = false;
    const Run ru = runWith(sc, opt, unfitted);
    std::printf("no GPS frame: %u/%d registered, checked %u\n", ru.registered, M,
                ru.st.gps_checked);
    check(ru.registered == (uint32_t)M && ru.st.gps_checked == 0, "no frame: nothing is checked");

    // The run must end on a registration the plain schedule does not solve
    // after, or an ordinary BA would stand in for the triggered one.
    size_t j = 14;
    while (j + 5 < base.at.size() && base.solvedAfter(j)) j++;
    std::printf("run ends at script entry %zu (plain schedule solves after it: %d)\n", j,
                (int)base.solvedAfter(j));
    check(j >= 12 && j + 5 < base.at.size() && !base.solvedAfter(j),
          "fixture: a registration the ordinary schedule does not solve after");

    ScriptedGps three(script(n, {{j - 2, 6.0}, {j - 1, 6.0}, {j, 6.0}}));
    const Run r1 = runWith(sc, opt, three);
    std::printf("three at 6 m: ba %u refused %u out %u, solve right after: %d\n  %s\n", r1.st.gps_ba,
                r1.st.gps_refused, r1.st.gps_out, (int)three.solvedAfter(j), three.events.c_str());
    check(r1.st.gps_ba == 1 && r1.st.gps_out == 3, "trigger: three in a row beyond the radius");
    check(three.solvedAfter(j), "trigger: the BA runs on the third registration");
    check(r1.registered == (uint32_t)M, "trigger: every image still registers");

    ScriptedGps alt(script(n, {{j - 4, 6.0}, {j - 2, 6.0}, {j, 6.0}}));
    const Run r2 = runWith(sc, opt, alt);
    std::printf("6,1,6,1,6: ba %u out %u\n", r2.st.gps_ba, r2.st.gps_out);
    check(r2.st.gps_ba == 0 && r2.st.gps_out == 3, "trigger: an in-radius registration breaks the run");

    // Every one of the first ten registrations here is followed by an ordinary
    // BA, so the rate limit is tested after the triggered one: a second run
    // straight after it waits for ten registrations, a third one later fires.
    size_t k = j + 3, m = j + 13;
    while (m + 1 < three.at.size() && three.solvedAfter(m)) m++;
    std::printf("rate limit: second run ends at %zu (plain solve after: %d), third at %zu (%d)\n", k,
                (int)three.solvedAfter(k), m, (int)three.solvedAfter(m));
    check(!three.solvedAfter(k) && !three.solvedAfter(m) && m < three.at.size(),
          "fixture: no ordinary BA right after either later run");
    ScriptedGps limited(script(n, {{j - 2, 6.0}, {j - 1, 6.0}, {j, 6.0}, {k - 2, 6.0}, {k - 1, 6.0},
                                   {k, 6.0}, {m - 2, 6.0}, {m - 1, 6.0}, {m, 6.0}}));
    const Run r3 = runWith(sc, opt, limited);
    std::printf("three runs: ba %u, solve after each %d %d %d\n", r3.st.gps_ba,
                (int)limited.solvedAfter(j), (int)limited.solvedAfter(k), (int)limited.solvedAfter(m));
    check(r3.st.gps_ba == 2 && limited.solvedAfter(j) && !limited.solvedAfter(k) &&
              limited.solvedAfter(m),
          "trigger: ten registrations between triggered BAs");

    ScriptedGps farGps(script(n, {{j, 25.0}}));
    const Run r4 = runWith(sc, opt, farGps);
    const uint32_t far_img = farGps.image_at.count(j) ? farGps.image_at.at(j) : ~0u;
    const int far_calls = farGps.calls.count(far_img) ? farGps.calls.at(far_img) : 0;
    std::printf("one at 25 m: refused %u, ba %u, image %u checked %d time(s), %u/%d registered\n",
                r4.st.gps_refused, r4.st.gps_ba, far_img, far_calls, r4.registered, M);
    check(r4.st.gps_refused == 1 && r4.st.gps_ba == 0, "refuse: four radii off after an in-radius one");
    check(far_calls >= 2 && r4.registered == (uint32_t)M, "refuse: the image registers on a later trial");

    // A drifting run reaches 25 m from beyond the radius, never from inside it.
    ScriptedGps drift(script(n, {{j - 2, 6.0}, {j - 1, 25.0}, {j, 25.0}}));
    const Run r5 = runWith(sc, opt, drift);
    std::printf("6 then two at 25 m: refused %u, ba %u, %u/%d registered\n", r5.st.gps_refused,
                r5.st.gps_ba, r5.registered, M);
    check(r5.st.gps_refused == 0 && r5.st.gps_ba == 1, "refuse: never a drifting run");
    check(r5.registered == (uint32_t)M, "refuse: the drifting run registers");

    // A wrong frame refuses for good: every third image a kilometre off. Past a
    // fifth of the checked images (and ten), the model drops it and takes them.
    ScriptedGps wrong(script(n, {}));
    for (uint32_t i = 0; i < M; i += 3) wrong.always[i] = 1000.0;
    const Run r6 = runWith(sc, opt, wrong);
    std::printf("every third image 1 km off: refused %u, dropped %u, %u/%d registered\n",
                r6.st.gps_refused, r6.st.gps_latched, r6.registered, M);
    check(r6.st.gps_latched == 1 && r6.st.gps_refused >= 10 && r6.registered == (uint32_t)M,
          "latch: a frame refusing a fifth is dropped, and every image registers");

    ScriptedGps few(script(n, {}));
    for (uint32_t i = 6; i < M; i += 8) few.always[i] = 1000.0;
    const Run r7 = runWith(sc, opt, few);
    std::printf("five images 1 km off: refused %u, dropped %u, %u/%d registered\n",
                r7.st.gps_refused, r7.st.gps_latched, r7.registered, M);
    check(r7.st.gps_latched == 0 && r7.registered == (uint32_t)M - 5,
          "latch: under ten standing refusals the frame stays and refuses them");

    testAssembly(sc, full, opt);
    testFinalCap(sc, full, opt);
    testRig(opt);

    std::printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails, fails == 1 ? "" : "s");
    return fails;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, body); }
