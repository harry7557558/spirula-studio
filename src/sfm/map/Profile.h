// Lightweight wall-clock accumulators for mapper profiling (host-time work,
// src/sfm/README.md "The mapper is the stage that is left"). Zero-dependency,
// always compiled; reporting is opt-in via SS_SFM_MAP_PROF=1 so normal runs
// produce identical output.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "core/Env.h"
#include "sfm/core/Log.h"

namespace sfm {

// An accumulator several threads may add to. The atom phase reconstructs
// clusters concurrently (sfm/map/Atoms.h) and every one of them runs the same
// instrumented code, so a plain `double +=` here is a data race in every
// bottom-up run. std::atomic<double>::fetch_add is C++20; the compare-exchange
// loop is what C++17 offers, and it is uncontended in practice -- one add per
// phase per call, against work measured in milliseconds.
class ProfAcc {
public:
    ProfAcc& operator+=(double v) {
        double cur = v_.load(std::memory_order_relaxed);
        while (!v_.compare_exchange_weak(cur, cur + v, std::memory_order_relaxed)) {}
        return *this;
    }
    double get() const { return v_.load(std::memory_order_relaxed); }

private:
    std::atomic<double> v_{0};
};

struct MapProf {
    // mapper host phases
    ProfAcc init_seed;   // initialize(): seed search incl. two-view RANSAC
    ProfAcc seed_geom;   // of which seedGeometry(): the two-view RANSAC itself
    ProfAcc bootstrap;   // bootstrapFocalLength(), counted inside init_seed
    ProfAcc choose;      // chooseNextImages(): ranking candidates
    ProfAcc reg;         // registerImage(): PnP RANSAC + refine + recount
    ProfAcc tri;         // triangulateForImage() during growth
    ProfAcc retri;       // completeAndRetriangulate()
    ProfAcc merge;       // mergeTracks(), inside the above
    ProfAcc filter;      // sanitizeCameras + filterPoints + filterImages
    ProfAcc snapshot;    // checkedRefine() reconstruction copy
    ProfAcc audit_check;  // audit(): poseContradicted over every registered image
    ProfAcc audit_fix;    // ... and everything after it, incl. the refinement
    // global BA, split (sfm/map/Bundle.h)
    ProfAcc ba_build;    // BAProblem assembly from the Reconstruction
    ProfAcc ba_init;     // BundleSolver ctor + init (device + pipelines + upload)
    ProfAcc ba_solve;    // solver.solve()
    ProfAcc ba_write;    // download + writeback
    std::atomic<long> n_ba{0}, n_ba_iters{0}, n_choose{0}, n_reg_try{0}, n_reg_ok{0};
    std::atomic<long> n_seed_geom{0};  // two-view RANSACs run (cache misses)
    std::atomic<long> n_merged{0};  // observations absorbed by track merging
    std::atomic<uint64_t> track_completed{0}, track_created{0}, track_removed{0};
    std::atomic<uint64_t> fusion_cap{0}, fusion_clash{0}, fusion_error{0}, fusion_angle{0};
    std::atomic<uint64_t> association_features{0}, association_ambiguous{0};
    std::atomic<uint64_t> association_changed{0}, association_deferred{0};
    std::atomic<uint64_t> association_duplicates{0};

    static bool enabled() {
        static bool e = spirula::env("SFM_MAP_PROF") != nullptr;
        return e;
    }

    // Counters are cumulative, so a caller may report more than once: the
    // mapper reports its own stage, the CLI reports again once the passes that
    // assemble its models have finished adding to them. `what` says which.
    void report(double total_s, const char* what = "mapper") const {
        if (!enabled()) return;
        const double init_seed = this->init_seed.get(), choose = this->choose.get();
        const double seed_geom = this->seed_geom.get(), bootstrap = this->bootstrap.get();
        const long n_seed_geom = this->n_seed_geom;
        const double reg = this->reg.get(), tri = this->tri.get(), retri = this->retri.get();
        const double merge = this->merge.get(), filter = this->filter.get();
        const double snapshot = this->snapshot.get(), ba_build = this->ba_build.get();
        const double audit_check = this->audit_check.get(), audit_fix = this->audit_fix.get();
        const double ba_init = this->ba_init.get(), ba_solve = this->ba_solve.get();
        const double ba_write = this->ba_write.get();
        const long n_ba = this->n_ba, n_ba_iters = this->n_ba_iters, n_choose = this->n_choose;
        const long n_reg_try = this->n_reg_try, n_reg_ok = this->n_reg_ok;
        const long n_merged = this->n_merged;
        double ba = ba_build + ba_init + ba_solve + ba_write;
        double accounted = init_seed + choose + reg + tri + retri + filter + snapshot + ba;
        slog::diag(slog::Tag::Map, "[prof] %s total %.2f s, accounted %.2f s (%.0f%%)\n"
                   "[prof]   seed search   %8.2f s  (two-view %.2f s over %ld pair(s), "
                   "focal bootstrap %.2f s)\n"
                   "[prof]   choose-next   %8.2f s  (%ld calls)\n"
                   "[prof]   register      %8.2f s  (%ld tries, %ld ok)\n"
                   "[prof]   triangulate   %8.2f s\n"
                   "[prof]   retriangulate %8.2f s  (of which merge %.2f s, %ld obs absorbed)\n"
                   "[prof]   filter        %8.2f s\n"
                   "[prof]   snapshot      %8.2f s\n"
                   "[prof]   audit         %8.2f s  (check %.2f + repair and refine %.2f)\n"
                   "[prof]   BA            %8.2f s  (%ld calls, %ld LM iters): "
                   "build %.2f + init %.2f + solve %.2f + write %.2f",
                   what, total_s, accounted, total_s > 0 ? 100.0 * accounted / total_s : 0.0,
                   init_seed, seed_geom, n_seed_geom, bootstrap, choose, n_choose, reg, n_reg_try,
                   n_reg_ok, tri, retri, merge,
                   n_merged, filter, snapshot, audit_check + audit_fix, audit_check, audit_fix,
                   ba, n_ba, n_ba_iters, ba_build, ba_init, ba_solve, ba_write);
        slog::diag(slog::Tag::Map,
                   "[tracks] completed %llu created %llu removed %llu | fusion refusals "
                   "cap %llu clash %llu reprojection %llu angle %llu | associations "
                   "features %llu ambiguous %llu changed %llu deferred %llu duplicates %llu",
                   (unsigned long long)track_completed.load(),
                   (unsigned long long)track_created.load(),
                   (unsigned long long)track_removed.load(),
                   (unsigned long long)fusion_cap.load(),
                   (unsigned long long)fusion_clash.load(),
                   (unsigned long long)fusion_error.load(),
                   (unsigned long long)fusion_angle.load(),
                   (unsigned long long)association_features.load(),
                   (unsigned long long)association_ambiguous.load(),
                   (unsigned long long)association_changed.load(),
                   (unsigned long long)association_deferred.load(),
                   (unsigned long long)association_duplicates.load());
    }
};

inline MapProf g_map_prof;

// RAII accumulator: adds elapsed seconds to `acc` at scope exit.
class ProfTimer {
public:
    explicit ProfTimer(ProfAcc& acc)
        : acc_(acc), t0_(std::chrono::steady_clock::now()) {}
    ~ProfTimer() {
        acc_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
    }

private:
    ProfAcc& acc_;
    std::chrono::steady_clock::time_point t0_;
};

}  // namespace sfm
