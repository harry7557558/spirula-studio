// The mapper's level check on a registration (Mapper::levelCheck): a scripted
// source declares each image's true camera-frame up, one image or one rig lens
// 3 deg off it, and the check must refuse exactly that one. Apart from
// sfm_gps_register_test so neither binary opens more GPU contexts than a
// process on some drivers can re-enumerate by uuid.
//
//   sfm_level_register_test [--device N] [--verbose]
//
// Prints FAIL lines and returns the count. The BA is real (GPU).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "sfm/core/PriorSource.h"
#include "sfm/core/Rig.h"
#include "sfm/map/ImuExtrinsic.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/SensorPriors.h"
#include "sfm/tests/SyntheticRegister.h"
#include "sfm/tests/SyntheticRig.h"
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

// ---- the level check on a registration (Mapper::levelCheck) -----------------

// Every image declares its true camera-frame up (world +Y through its pose), the
// images in `lie` one 3 deg off it; the level frame is the posed images' consensus,
// as ExifGpsPriors fits it. `checked` lists the images the check asked about, in order.
class TruthLevel : public PriorSource {
public:
    TruthLevel(const std::vector<Pose>& gt, std::set<uint32_t> lie) : lie_(std::move(lie)) {
        for (const Pose& p : gt) u_.push_back(mul(p.R, Vec3{0, 1, 0}));
    }
    bool has(uint32_t) const override { return false; }
    bool relativeRotation(uint32_t, uint32_t, Mat3&, double&) const override { return false; }
    std::vector<uint32_t> neighbours(uint32_t) const override { return {}; }
    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        std::vector<Vec3> votes;
        for (const PosedImage& im : imgs)
            if (im.image < u_.size() && !lie_.count(im.image))
                votes.push_back(mul(transpose(im.pose.R), u_[im.image]));
        PosePriors p;
        const UpConsensus c = consensusUp(votes);
        p.level = {c.ok, c.up, 1.0};
        return p;
    }
    bool declaredUp(uint32_t img, Vec3& u) const override {
        if (img >= u_.size()) return false;
        checked.push_back(img);
        u = lie_.count(img) ? mul(angleAxisToRotation({0.0, 0.0, 3.0 * M_PI / 180.0}), u_[img])
                            : u_[img];
        return true;
    }
    mutable std::vector<uint32_t> checked;

private:
    std::set<uint32_t> lie_;
    std::vector<Vec3> u_;
};

static void testLevel(const Scene& sc, MapperOptions opt) {
    const uint32_t M = (uint32_t)sc.db.images.size();
    TruthLevel honest(sc.gt, {});
    Mapper m0(sc.db, sc.feats, opt, {}, nullptr, nullptr, &honest);
    const std::vector<Reconstruction> r0 = m0.run();
    const Mapper::PriorStats s0 = m0.priorStats();
    const uint32_t reg0 = r0.empty() ? 0 : r0.front().numRegistered();
    std::printf("level honest: %u/%u registered, checked %u, refused %u\n", reg0, M,
                s0.level_checked, s0.level_refused);
    check(reg0 == M && s0.level_refused == 0, "level: no registration of a level image is refused");
    check(s0.level_checked >= M / 2 && s0.level_checked == honest.checked.size(),
          "level: the check reads every registration once the frame exists");
    const uint32_t liar = honest.checked.empty() ? 0 : honest.checked.back();

    TruthLevel lying(sc.gt, {liar});
    Mapper m1(sc.db, sc.feats, opt, {}, nullptr, nullptr, &lying);
    const std::vector<Reconstruction> r1 = m1.run();
    const Mapper::PriorStats s1 = m1.priorStats();
    const bool in = !r1.empty() && registered(r1.front(), liar);
    const size_t asked = (size_t)std::count(lying.checked.begin(), lying.checked.end(), liar);
    std::printf("level, image %u 3 deg off: in %d, asked %zu time(s), refused %u, %u/%u registered\n",
                liar, (int)in, asked, s1.level_refused, r1.empty() ? 0 : r1.front().numRegistered(), M);
    check(asked >= 1 && !in && s1.level_refused >= 1,
          "level: a registration tilted off its declared up is refused");
    check(!r1.empty() && r1.front().numRegistered() == M - 1 && s1.refused == 0 &&
              s1.gps_refused == 0,
          "level: every other image registers, and the refusal is the level check's");
}

static void testLevelRig(MapperOptions opt) {
    std::vector<char> weak(40, 0);
    for (int f = 8; f < 37; f++) weak[f] = f % 4 != 0;
    const synth_rig::RigScene sc = synth_rig::makeRigScene(40, 37, weak);
    const uint32_t M = (uint32_t)sc.M, n = 2 * M;
    const RigTable rigs = buildRigTable(sc.names, {RigDef{"rig", {{"cam0"}, {"cam1"}}}});
    TruthLevel honest(sc.gt, {});
    const RigRun r0 = runRig(sc, rigs, opt, honest, "F");
    std::printf("level rig honest: %u/%u registered, checked %u, refused %u\n",
                r0.model.numRegistered(), n, r0.st.level_checked, r0.st.level_refused);
    check(r0.model.numRegistered() == n && r0.st.level_refused == 0,
          "level rig: no frame of level images is refused");
    // A weak frame the rig placed late; only its cam1 lies, so the rig must check every lens.
    uint32_t f = M;
    for (auto it = r0.placed.rbegin(); it != r0.placed.rend() && f == M; ++it)
        if (weak[*it % M]) f = *it % M;
    TruthLevel lying(sc.gt, {M + f});
    const RigRun r1 = runRig(sc, rigs, opt, lying, "F");
    std::printf("level rig, frame %u cam1 3 deg off: in %d %d, refused %u, %u/%u registered\n", f,
                (int)registered(r1.model, f), (int)registered(r1.model, M + f), r1.st.level_refused,
                r1.model.numRegistered(), n);
    check(f < M && !registered(r1.model, M + f) && r1.st.level_refused >= 1,
          "level rig: a frame with one lens tilted off its declared up is refused");
}

// ---- the production source through the mapper ---------------------------------

// The real ExifGpsPriors on an image set with no fixes, every image marked level,
// recording what the mapper was handed on each solve. The constants under test are
// the production ones; nothing here restates them except the checks' literals.
class ProdLevel : public PriorSource {
public:
    struct Call {
        int votes = 0;
        bool ok = false;
        double spread = 0, tol = 0, sigma = 0;
        size_t ups = 0;
    };
    explicit ProdLevel(size_t n)
        : inner_(std::vector<std::optional<Geodetic>>(n), SensorPriorOptions{},
                 std::vector<char>(n, 1)) {}
    bool has(uint32_t i) const override { return inner_.has(i); }
    bool relativeRotation(uint32_t i, uint32_t j, Mat3& R, double& s) const override {
        return inner_.relativeRotation(i, j, R, s);
    }
    std::vector<uint32_t> neighbours(uint32_t i) const override { return inner_.neighbours(i); }
    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        PosePriors p = inner_.factors(imgs);
        const SensorFactorStats st = inner_.lastFactors();
        Call c;
        c.votes = st.level_votes;
        c.ok = p.level.ok;
        c.spread = st.level_spread_deg;
        c.tol = p.level.tol_deg;
        c.ups = p.ups.size();
        c.sigma = p.ups.empty() ? 0.0 : p.ups.front().sigma * 180.0 / M_PI;
        calls.push_back(c);
        return p;
    }
    bool declaredUp(uint32_t i, Vec3& u) const override { return inner_.declaredUp(i, u); }
    void disableLevel() override {
        disables++;
        inner_.disableLevel();
    }
    // The call over the most posed images: the whole capture's reading.
    Call fullest() const {
        Call best;
        for (const Call& c : calls)
            if (c.votes >= best.votes) best = c;
        return best;
    }
    bool everOk() const {
        for (const Call& c : calls)
            if (c.ok) return true;
        return false;
    }
    std::vector<Call> calls;
    int disables = 0;

private:
    ExifGpsPriors inner_;
};

// Roll (deg) of camera i: a steady ramp of `total` degrees over the set, about its mean.
static std::vector<double> rampRoll(int M, double total) {
    std::vector<double> r((size_t)M);
    for (int i = 0; i < M; i++) r[(size_t)i] = total * (double(i) / (M - 1) - 0.5);
    return r;
}

struct ProdRun {
    uint32_t registered = 0, total = 0;
    Mapper::PriorStats st;
    ProdLevel::Call full;
    bool ever_ok = false;
    int disables = 0;
    std::vector<ProdLevel::Call> calls;
};

static ProdRun runProd(int M, const std::vector<double>& roll, MapperOptions opt) {
    const Scene sc = makeScene(M, roll, 0.0);
    ProdLevel src((size_t)M);
    Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &src);
    const std::vector<Reconstruction> r = m.run();
    ProdRun o;
    o.total = (uint32_t)M;
    o.registered = r.empty() ? 0 : r.front().numRegistered();
    o.st = m.priorStats();
    o.full = src.fullest();
    o.ever_ok = src.everOk();
    o.disables = src.disables;
    o.calls = src.calls;
    return o;
}

static void report(const char* what, const ProdRun& o) {
    std::printf("%s: %u/%u registered, checked %u, refused %u, spread %.3f tol %.2f, gate %d, "
                "latched %d (%d disable call(s))\n",
                what, o.registered, o.total, o.st.level_checked, o.st.level_refused, o.full.spread,
                o.full.tol, (int)o.full.ok, (int)o.st.level_latched, o.disables);
}

// A capture that passes the gate refuses none of its honest images, however the tilt
// is spread inside the gate: a steady roll ramp of 3 and 4 deg.
static void testLevelRamp(MapperOptions opt) {
    for (int M : {40, 60})
        for (double total : {3.0, 4.0}) {
            const ProdRun o = runProd(M, rampRoll(M, total), opt);
            char what[96];
            std::snprintf(what, sizeof what, "level ramp %.0f deg over %d", total, M);
            report(what, o);
            check(o.registered == o.total, "level ramp: every image of a capture inside the gate registers");
            check(o.st.level_refused == 0 && !o.st.level_latched,
                  "level ramp: no honest image is refused and the prior is not latched off");
            check(o.ever_ok, "level ramp: the level prior engaged");
            if (M == 60 && total == 4.0) {
                // The tolerance the mapper last applied is one the source stated: 3 x a spread.
                bool three_x = false;
                for (const ProdLevel::Call& c : o.calls)
                    three_x = three_x || (c.ok && std::fabs(c.tol - o.st.level_tol) < 1e-9 &&
                                          c.tol > 1.8 && std::fabs(c.tol - 3.0 * c.spread) < 1e-9);
                check(three_x, "level ramp: the mapper applies a tolerance of 3 x the measured spread");
            }
            if (M == 40 && total == 3.0)
                check(o.full.spread > 0.6 && o.full.spread < 0.9 && o.full.tol > 1.8,
                      "level ramp: the fixture sits at spread 0.6-0.9 and the tolerance follows it");
        }
}

// The gate on the whole capture, through the mapper: spread 1.25 deg is refused, 0.83 is not.
static void testLevelGate(MapperOptions opt) {
    const ProdRun over = runProd(40, rampRoll(40, 6.0), opt);
    report("level gate, spread ~1.25", over);
    check(over.registered == over.total && !over.full.ok && over.full.spread > 1.0,
          "level gate: a capture spread 1.25 deg states no up, and registers");
    const ProdRun under = runProd(40, rampRoll(40, 4.0), opt);
    report("level gate, spread ~0.83", under);
    check(under.registered == under.total && under.full.ok && under.full.spread < 1.0,
          "level gate: a capture spread 0.83 deg keeps its up");
}

// The per-image tolerance: 1 deg floor on a level capture, 3 x spread on a rolled one.
static void testLevelTolerance(MapperOptions opt) {
    for (const auto& [roll_deg, want_in, what] :
         {std::tuple<double, bool, const char*>{1.5, false, "level tolerance: 1.5 deg off a level capture is refused"},
          std::tuple<double, bool, const char*>{0.8, true, "level tolerance: 0.8 deg off a level capture is kept"}}) {
        std::vector<double> roll(60, 0.0);
        roll[5] = roll_deg;
        const Scene sc = makeScene(60, roll, 0.0);
        ProdLevel src(60);
        Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &src);
        const std::vector<Reconstruction> r = m.run();
        const bool in = !r.empty() && registered(r.front(), 5);
        const Mapper::PriorStats st = m.priorStats();
        std::printf("level tolerance, image 5 %.1f deg off: in %d, checked %u, refused %u, latched %d\n",
                    roll_deg, (int)in, st.level_checked, st.level_refused, (int)st.level_latched);
        check(in == want_in && !st.level_latched, what);
        check(!r.empty() && r.front().numRegistered() == (want_in ? 60u : 59u),
              "level tolerance: the other images are untouched by it");
    }
    // Spread ~0.75: 3 x spread = 2.25 deg, 4 x = 3.0. One image 2.9 deg off is refused; the
    // ramp's own ends, 1.8 deg off, are not; and one refusal in ~30 checked images does not latch.
    std::vector<double> roll = rampRoll(60, 3.6);
    roll[5] = 2.9;
    const Scene sc = makeScene(60, roll, 0.0);
    ProdLevel src(60);
    Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &src);
    const std::vector<Reconstruction> r = m.run();
    const Mapper::PriorStats st = m.priorStats();
    std::printf("level tolerance, spread 0.75 + one image 2.9 deg off: %u/60, refused %u, latched %d\n",
                r.empty() ? 0 : r.front().numRegistered(), st.level_refused, (int)st.level_latched);
    check(!r.empty() && !registered(r.front(), 5) && r.front().numRegistered() == 59,
          "level tolerance: 3 x spread refuses one image at 2.9 deg and only that one");
    check(!st.level_latched && src.disables == 0, "level latch: one dissenter does not switch the prior off");
}

// A capture whose level images are a minority-tilted mix: the gate opens on the first
// level window, the tolerance refuses the rest, and the latch must put them back.
static void testLevelLatch(MapperOptions opt) {
    std::vector<double> roll(80, 0.0);
    for (int i = 56; i < 80; i++) roll[(size_t)i] = 8.0;
    const ProdRun o = runProd(80, roll, opt);
    report("level latch, images 56-79 rolled 8 deg", o);
    check(o.registered == o.total, "level latch: every image registers once the prior is latched off");
    check(o.st.level_latched && o.disables >= 1, "level latch: refusals past a few percent switch the prior off");
    check(o.st.level_refused >= 1, "level latch: the dissenters were refused before it fired");
}

// The latch through a scripted source that keeps declaring its up after the switch: the
// mapper must drop its own level frame, or the liars stay refused.
static void testLevelLatchScripted(const Scene& sc, MapperOptions opt) {
    std::set<uint32_t> liars;
    for (uint32_t i = 56; i < 80; i++) liars.insert(i);
    TruthLevel src(sc.gt, liars);
    Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &src);
    const std::vector<Reconstruction> r = m.run();
    const Mapper::PriorStats st = m.priorStats();
    std::printf("level latch, scripted: %u/80 registered, refused %u, latched %d\n",
                r.empty() ? 0 : r.front().numRegistered(), st.level_refused, (int)st.level_latched);
    check(st.level_latched && st.level_refused >= 1, "level latch: a scripted source's liars trip it");
    check(!r.empty() && r.front().numRegistered() == 80,
          "level latch: the mapper clears its own level frame when it latches");
}

// A capture that is not level at all: every image registers, and the prior is off
// (the gate closed, or latched) by the end.
static void testLevelSteep(MapperOptions opt) {
    for (double total : {8.0, 12.0}) {
        const ProdRun o = runProd(80, rampRoll(80, total), opt);
        char what[96];
        std::snprintf(what, sizeof what, "level steep ramp %.0f deg over 80", total);
        report(what, o);
        check(o.registered == o.total, "level steep: every image of a non-level capture registers");
        check(!o.full.ok || o.st.level_latched, "level steep: the prior is off by the end of a non-level capture");
    }
}

// The vote minimum and the up factors' sigma, as the mapper meets them.
static void testLevelVotesAndSigma(MapperOptions opt) {
    const ProdRun few = runProd(29, {}, opt);
    report("level, 29 images", few);
    check(!few.ever_ok && few.st.level_checked == 0, "level votes: 29 posed images never open the gate");
    const Scene sc = makeScene(45, {}, 0.0);
    ProdLevel src(45);
    Mapper m(sc.db, sc.feats, opt, {}, nullptr, nullptr, &src);
    const std::vector<Reconstruction> r = m.run();
    const Mapper::PriorStats st = m.priorStats();
    double sigma = 0;
    for (const ProdLevel::Call& c : src.calls)
        if (c.ok) sigma = c.sigma;
    std::printf("level, 45 images: %u checked, up sigma %.4f deg\n", st.level_checked, sigma);
    check(!r.empty() && r.front().numRegistered() == 45 && src.everOk() && st.level_checked > 0,
          "level votes: a 45-image level capture opens the gate and is checked");
    check(std::fabs(sigma - 0.3) < 1e-9, "level sigma: the BA is handed 0.3 deg up factors");
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
    testLevel(makeScene(40), opt);
    testLevelRig(opt);
    testLevelRamp(opt);
    testLevelGate(opt);
    testLevelTolerance(opt);
    testLevelLatch(opt);
    testLevelLatchScripted(makeScene(80), opt);
    testLevelSteep(opt);
    testLevelVotesAndSigma(opt);
    std::printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails, fails == 1 ? "" : "s");
    return fails;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, body); }
