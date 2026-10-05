#pragma once
// The block scale statistic: whether the stretch of chain growth is extending has left the
// GPS's scale. Detection only -- Mapper::gpsScaleCheck/gpsScaleEnd gather the frames, read
// this, and ask for a bundle adjustment; they do not rescale anything themselves (the
// requested BA alone sets the block's scale, whatever came before it -- see gpsScaleCheck).
// Pure functions over one frame per capture position in capture order.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_set>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"

namespace sfm {
namespace bss {

// A node is the first fresh fix at kNode metres of MODEL chord from the last one, within
// 2 kNode of model path: nodes on the GPS walk with a stationary receiver's quantisation
// (0.23-0.38 m steps on EXIF), and a frame-step path is 10-20 % long at 0.4 m/frame.
constexpr double kNode = 5.0;
constexpr int64_t kMaxGap = 3;  // capture positions; a wider gap ends a run
constexpr int kLengths = 3;
constexpr double kLength[kLengths] = {60, 100, 150};
// Open-sky sd of the log ratio on finished canopy-capture, power-corridor and OSV models, per length.
constexpr double kSigma[kLengths] = {0.008, 0.006, 0.0045};
// In-run: no fire at sky >= 0.5 in 3 x 1795 canopy-capture checks or 939 power-corridor ones. After
// growth, lower: the canopy capture's milder modes finish their west chain at 0.956-0.958 over 60 m.
constexpr double kTauRun[kLengths] = {0.05, 0.04, 0.03};
constexpr double kTauEnd[kLengths] = {0.04, 0.03, 0.025};
constexpr size_t kEndChecks = 20;
// A model-space jump this big between capture-adjacent frames is an unwelded seam, not chain
// drift (rigcheck's own "0 model jumps" bar). gpsScaleEnd excludes any window crossing one, so
// a seam step cannot outbid a real reading (canopy capture: seam window 1.046 vs west chain 0.958).
constexpr double kSeamJump = 1.5;

struct Frame {
    uint32_t img = 0;
    int64_t seq = 0, pos = 0;
    Vec3 c{0, 0, 0};      // model centre through the GPS fit, metres; z = 0 on a flat fit
    Vec3 g{0, 0, 0};      // the fix, the same way
    bool fresh = true;    // the fix differs from the capture predecessor's
    uint32_t stamp = 0;   // the check that first saw it registered; 0 = before growth
};

struct Pair {
    size_t a = 0, b = 0;
};

// One frame per capture position, the first registered there, in capture order: a rig's
// lenses add their baseline to every walk otherwise (5.1 cm on the OSV rig, 1045 pairs).
inline std::vector<Frame> collapse(std::vector<Frame> all) {
    std::sort(all.begin(), all.end(), [](const Frame& a, const Frame& b) {
        if (a.seq != b.seq) return a.seq < b.seq;
        if (a.pos != b.pos) return a.pos < b.pos;
        return a.stamp != b.stamp ? a.stamp < b.stamp : a.img < b.img;
    });
    std::vector<Frame> out;
    for (const Frame& f : all)
        if (out.empty() || out.back().seq != f.seq || out.back().pos != f.pos) out.push_back(f);
    return out;
}

// The capture-contiguous run holding frames[k]: [lo, hi] with no position gap over kMaxGap.
inline void runOf(const std::vector<Frame>& f, size_t k, size_t& lo, size_t& hi) {
    lo = hi = k;
    while (lo > 0 && f[lo - 1].seq == f[k].seq && f[lo].pos - f[lo - 1].pos <= kMaxGap) lo--;
    while (hi + 1 < f.size() && f[hi + 1].seq == f[k].seq && f[hi + 1].pos - f[hi].pos <= kMaxGap)
        hi++;
}

// Node pairs walking from frames[from] by `dir` (+1 or -1) inside [lo, hi]. Every anchor is
// a fresh fix: a stalled receiver's repeat is v * stall off (15.6 m in 60 m for 12 frames at
// 1.3 m). A hover or loop runs up 2 kNode of path first and drops its segment.
inline std::vector<Pair> nodePairs(const std::vector<Frame>& f, size_t from, int dir, size_t lo,
                                   size_t hi) {
    std::vector<Pair> out;
    size_t a = from, prev = from;
    bool anchored = f[from].fresh;
    double path = 0;
    for (long i = (long)from + dir; i >= (long)lo && i <= (long)hi; i += dir) {
        path += (f[i].c - f[prev].c).norm();
        prev = (size_t)i;
        const bool node = anchored && (f[i].c - f[a].c).norm() >= kNode && f[i].fresh;
        if (node) out.push_back({a, (size_t)i});
        if (node || !anchored || path > 2 * kNode) {
            anchored = f[i].fresh;
            a = (size_t)i;
            path = 0;
        }
    }
    return out;
}

inline void chordSums(const std::vector<Frame>& f, const std::vector<Pair>& p, size_t n,
                      double& m, double& g) {
    m = g = 0;
    for (size_t j = 0; j < n && j < p.size(); j++) {
        m += (f[p[j].b].c - f[p[j].a].c).norm();
        g += (f[p[j].b].g - f[p[j].a].g).norm();
    }
}

// Model over GPS chord sums through every run, walked forward: the reference each window is
// read against, so the fit's own scale cancels. 0 when there is no pair.
inline double globalRatio(const std::vector<Frame>& f) {
    double m = 0, g = 0;
    for (size_t lo = 0; lo < f.size();) {
        size_t a, hi;
        runOf(f, lo, a, hi);
        const std::vector<Pair> p = nodePairs(f, lo, +1, lo, hi);
        double pm, pg;
        chordSums(f, p, p.size(), pm, pg);
        m += pm;
        g += pg;
        lo = hi + 1;
    }
    return g > 0 && m > 0 ? m / g : 0;
}

// x[side][L] = ln(window ratio / whole-model ratio) over the first N = L / kNode node pairs
// walking away from frames[k]; side 0 toward earlier positions, 1 toward later ones.
struct Reading {
    bool have[2][kLengths] = {};
    double x[2][kLengths] = {};
};

inline Reading read(const std::vector<Frame>& f, size_t k, double all) {
    Reading r;
    if (all <= 0) return r;
    size_t lo, hi;
    runOf(f, k, lo, hi);
    for (int side = 0; side < 2; side++) {
        const std::vector<Pair> p = nodePairs(f, k, side ? +1 : -1, lo, hi);
        for (int l = 0; l < kLengths; l++) {
            const size_t n = (size_t)std::lround(kLength[l] / kNode);
            if (p.size() < n) continue;
            double m, g;
            chordSums(f, p, n, m, g);
            if (m <= 0 || g <= 0) continue;
            r.have[side][l] = true;
            r.x[side][l] = std::log(m / g) - std::log(all);
        }
    }
    return r;
}

struct Pick {
    bool ok = false;
    int side = 0, l = 0;
    double x = 0;
};

// A length is read only where its threshold clears kNoiseSigmas times the spread of the readings
// taken so far: GPS wander and model noise together, robust to a drifted block that is a third
// of them. The fit's RMS will not do -- a stretched tail alone lifted it from 0.12 to 2.0 m.
constexpr double kNoiseSigmas = 3.0;
constexpr size_t kNoiseMinReadings = 20;
struct Noise {
    double sigma[kLengths] = {};
    size_t n[kLengths] = {};
};
// Per length over every reading kept, both sides: the lower quartile of |x| over 0.3186, a
// Gaussian's sigma. A median read a stretched tail's readings as noise once they were a third.
inline Noise noiseOf(const std::vector<double> (&hist)[kLengths]) {
    Noise z;
    for (int l = 0; l < kLengths; l++) {
        std::vector<double> a;
        a.reserve(hist[l].size());
        for (double x : hist[l]) a.push_back(std::fabs(x));
        z.n[l] = a.size();
        if (a.empty()) continue;
        std::nth_element(a.begin(), a.begin() + (long)(a.size() / 4), a.end());
        z.sigma[l] = a[a.size() / 4] / 0.3186;
    }
    return z;
}
inline bool readable(const Noise& z, int l, double tau) {
    return z.n[l] >= kNoiseMinReadings && kNoiseSigmas * z.sigma[l] <= tau;
}
inline bool anyReadable(const Noise& z, const double (&tau)[kLengths]) {
    for (int l = 0; l < kLengths; l++)
        if (readable(z, l, tau[l])) return true;
    return false;
}
inline Reading maskNoise(Reading r, const Noise& z, const double (&tau)[kLengths]) {
    for (int l = 0; l < kLengths; l++)
        if (!readable(z, l, tau[l])) r.have[0][l] = r.have[1][l] = false;
    return r;
}
inline void addReading(const Reading& r, std::vector<double> (&hist)[kLengths]) {
    for (int side = 0; side < 2; side++)
        for (int l = 0; l < kLengths; l++)
            if (r.have[side][l]) hist[l].push_back(r.x[side][l]);
}

// The reading past its threshold that is most sigmas out, if any.
inline Pick pickOver(const Reading& r, const double (&tau)[kLengths]) {
    Pick best;
    double z = 0;
    for (int side = 0; side < 2; side++)
        for (int l = 0; l < kLengths; l++) {
            if (!r.have[side][l] || std::fabs(r.x[side][l]) <= tau[l]) continue;
            const double zz = std::fabs(r.x[side][l]) / kSigma[l];
            if (best.ok && zz <= z) continue;
            best = {true, side, l, r.x[side][l]};
            z = zz;
        }
    return best;
}

// The reading most sigmas out, whatever its size; `ok` only if it clears `tau`.
inline Pick pickStrongest(const Reading& r, const double (&tau)[kLengths], double& z) {
    Pick best;
    z = -1;
    for (int side = 0; side < 2; side++)
        for (int l = 0; l < kLengths; l++) {
            if (!r.have[side][l]) continue;
            const double zz = std::fabs(r.x[side][l]) / kSigma[l];
            if (zz <= z) continue;
            best = {false, side, l, r.x[side][l]};
            z = zz;
        }
    best.ok = z >= 0 && std::fabs(best.x) > tau[best.l];
    return best;
}

// After growth: the strongest of the stored readings, NOT the newest frame's. The last
// registrations are the ones that bridge two fronts, and a window ending on them reads the
// step between the fronts (canopy capture: 1.043 at the last check, 0.958 three checks earlier).
inline Pick pickEnd(const std::vector<Reading>& stored, size_t& which) {
    Pick best;
    double z = -1;
    which = 0;
    for (size_t i = 0; i < stored.size(); i++) {
        double zz;
        const Pick p = pickStrongest(stored[i], kTauEnd, zz);
        if (zz <= z) continue;
        best = p;
        z = zz;
        which = i;
    }
    return best;
}

// True if any capture-adjacent registered step within [lo, hi] jumps more than `threshold` in
// model space: two fronts meeting at a not-yet-welded seam, not chain drift.
inline bool hasJump(const std::vector<Frame>& f, size_t lo, size_t hi, double threshold) {
    for (size_t i = lo + 1; i <= hi && i < f.size(); i++)
        if (f[i].seq == f[i - 1].seq && (f[i].c - f[i - 1].c).norm() > threshold) return true;
    return false;
}

// `r`, with every (side, length) whose node-pair window crosses a jump over `threshold` between
// frames[k] and the window's far boundary cleared: gpsScaleEnd's pool of stored readings must
// not choose an unwelded seam bridge as though it were ordinary drift.
inline Reading maskSeamJumps(const std::vector<Frame>& f, size_t k, Reading r, double threshold) {
    size_t lo, hi;
    runOf(f, k, lo, hi);
    for (int side = 0; side < 2; side++) {
        const int dir = side ? +1 : -1;
        const std::vector<Pair> p = nodePairs(f, k, dir, lo, hi);
        for (int l = 0; l < kLengths; l++) {
            if (!r.have[side][l]) continue;
            const size_t n = (size_t)std::lround(kLength[l] / kNode);
            if (n > p.size() || n == 0) continue;
            const size_t boundary = p[n - 1].b;
            const size_t a = std::min(k, boundary), b = std::max(k, boundary);
            if (hasJump(f, a, b, threshold)) r.have[side][l] = false;
        }
    }
    return r;
}

}  // namespace bss
}  // namespace sfm
