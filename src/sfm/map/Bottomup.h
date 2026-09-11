// Bottom-up reconstruction: many small models, merged upwards (D57).
//
// The flat mapper builds one large model first and only then discovers what it
// could not reach, so its expensive whole-model passes (global BA,
// retriangulation, filtering) all run at full size, and every repair runs at
// full size too.
//
// This turns the schedule around. The view graph is cut into *atoms* of around
// a hundred images (sfm/map/Partition.h), each reconstructed by the ordinary
// incremental mapper and all of them concurrently (sfm/map/Atoms.h) -- at that
// size its passes are trivial, its failure modes are local, and the atoms are
// independent. From there it is the shared schedule in sfm/map/Assemble.h:
// merge levels with growth and a joint solve between them, then the finishing
// passes. This file is the part that is actually bottom-up -- the cut, the
// atoms, and the one joint refinement that gives them a common gauge before
// any of them are merged.
//
// Two things make the merging work where a single pass over the flat mapper's
// output does not, and both are set up here:
//
//   * **Shared intrinsics throughout.** Every model in flight is bundle-
//     adjusted in one problem with the intrinsics shared per camera group
//     (Mapper::jointRefine). A forty-image atom cannot determine its own focal
//     and must not try; when each model keeps its own answer, the merger is
//     asked to align two reconstructions of the same place in two different
//     gauges, and the pixel-space tests it uses to accept a merge are exactly
//     what that breaks. There is nothing to average at merge time because
//     nothing ever diverged.
//   * **Overlap by construction.** Neighbouring atoms share images
//     (PartitionOptions::overlap), and that overlap is what the first Sim(3)
//     aligns on. Two atoms with nothing in common can only be joined by
//     growth, which is the slow path.
#pragma once

#include <algorithm>
#include <chrono>
#include <numeric>
#include <cstdio>
#include <map>
#include <string>
#include <set>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Model.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Atoms.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"
#include "sfm/map/ModelOps.h"
#include "sfm/map/Partition.h"
#include "sfm/core/Log.h"

namespace sfm {

struct BottomUpOptions {
    // Atom size, and how many images neighbouring atoms share.
    //
    // The overlap is what a Sim(3) merge aligns on, so it is not optional: two
    // atoms with nothing in common can only be joined by growth, which is the
    // slow path. It is also not the knob to economize on -- measured, cutting
    // it from 12 to 8 saves a fifth of the time and loses as many images as
    // doubling the atom size does. Overlap below min_part on purpose: it is
    // what keeps a cut part strictly smaller than what it was cut from (see
    // bisect).
    //
    // 48 is small, and deliberately so. A model pays a fixed number of bundle
    // adjustments as it grows, so a small atom is *less* efficient per image,
    // not more: at 48 the partition asks for 2.1-2.4x as many image-slots as
    // the capture has, and 96 brings that to ~1.4x and the whole run 1.2-2.1x
    // faster. That was tried and reverted. On a 798-image capture, 96 put half
    // the reconstruction half a scene-extent from where it belonged -- with
    // *more* images registered and a better median rotation error than the flat
    // mapper, which is the signature of a fold. On a 1322-image one it left six
    // fragments where 48 left one large model.
    //
    // What makes that worth 40 % of the run time is that nothing else fixed it,
    // and the failure is silent. A stricter merge threshold did not (12 shared
    // images instead of 3: unchanged). Tightening the tree's bundle adjustments
    // did not. Splitting the atoms first did not -- neither by their own
    // contradicted pairs nor by the fold detector, both of which found *nothing*
    // in any atom. And the cross-seam test ran on all fifteen merges and refused
    // one, so the weld went through a test built to catch exactly this.
    //
    // Which leaves the size itself as the only thing that separates a good run
    // from a bad one here, and no test downstream that will notice when it goes
    // wrong. `--bup-atom-size` exists for anyone who wants the time back on a
    // capture they can verify.
    PartitionOptions partition{48, 12, 16};
    // How the atoms themselves are built, and on how many threads.
    AtomOptions atom;
    // One joint bundle adjustment over every atom, with intrinsics shared per
    // camera group, before any merging. This is the solve the loose per-atom
    // cadence is traded for: the same work as hundreds of small problems, in
    // one that saturates the device, and it is where the atoms stop each having
    // their own opinion about the focal. Only worth it with enough atoms to be
    // that trade -- with a dozen it is one extra full solve buying back a
    // handful of tiny ones, measured at 19 % on a 480-image capture. The same
    // threshold decides whether the tree is big enough to be trusted with the
    // schedule at all.
    bool joint_after_atoms = true;
    size_t joint_min_models = 32;
    bool verbose = true;
};

struct BottomUpStats {
    size_t atoms = 0;
    size_t atom_images = 0;        // summed over atoms, so overlap counts twice
    size_t models_from_atoms = 0;
    int atom_threads = 1;
    double t_atoms = 0;
    // Everything after the atoms, which is the shared schedule.
    AssembleStats assemble;
};

// Reconstruct bottom-up. `mapper` must already be set up for the whole
// database; it builds no atoms itself (each of those gets its own, over its own
// sub-database) but performs every operation above them. `mopt` supplies the
// merge and cleanup thresholds, which are shared with the flat mapper -- there
// is one set of them and this is the same set.
inline std::vector<Reconstruction> bottomUpReconstruct(Mapper& mapper, const MatchesDatabase& db,
                                                       const std::vector<FeatureSet>& feats,
                                                       const BottomUpOptions& opt,
                                                       const ManagerOptions& mopt,
                                                       const AssembleOptions& aso,
                                                       BottomUpStats& st) {
    auto clk = [] { return std::chrono::steady_clock::now(); };
    auto secs = [](auto a, auto b) { return std::chrono::duration<double>(b - a).count(); };

    // The starting intrinsics are chosen once, over the whole database, and
    // every atom inherits them. Left to the atoms, the first one built would
    // pick for all the others from a few dozen images (D48).
    mapper.bootstrapCameras();

    ViewGraph g = buildViewGraph(db);
    std::vector<std::vector<uint32_t>> atoms = partitionViewGraph(g, opt.partition);
    st.atoms = atoms.size();
    for (const std::vector<uint32_t>& a : atoms) st.atom_images += a.size();
    if (opt.verbose) {
        size_t small = SIZE_MAX, big = 0;
        for (const std::vector<uint32_t>& a : atoms) {
            small = std::min(small, a.size());
            big = std::max(big, a.size());
        }
        slog::diag(slog::Tag::Map,
                   "[bup] %zu image(s) -> %zu atom(s) of %zu..%zu images (%.2fx cover)",
                   db.images.size(), atoms.size(), atoms.empty() ? 0 : small, big,
                   db.images.empty() ? 0.0 : (double)st.atom_images / (double)db.images.size());
    }

    // A capture that does not split into at least two atoms has nothing to
    // merge, and the flat mapper is what a single atom would have run anyway.
    if (atoms.size() < 2) {
        if (opt.verbose) slog::diag(slog::Tag::Map, "[bup] one atom: reconstructing it flat");
        return mapper.run();
    }

    // ---- the atoms -------------------------------------------------------
    AtomStats as;
    AtomOptions ao = opt.atom;
    ao.verbose = opt.verbose;
    std::vector<Reconstruction> models =
        reconstructAtoms(db, feats, mapper.options(), mapper.cameraIds(),
                         mapper.startingCameras(), atoms, ao, as, mapper.rigs());
    st.t_atoms = as.secs;
    st.models_from_atoms = models.size();
    st.atom_threads = as.threads;
    if (opt.verbose)
        slog::diag(slog::Tag::Map,
                   "[bup] %zu atom(s) on %d thread(s) -> %zu model(s), %zu registrations, "
                   "%zu empty: %.1f s", as.atoms, as.threads, as.models, as.registered, as.empty,
                   as.secs);
    mapper.claimAll(models);
    // Every atom failed. The flat mapper will fail the same way and fail fast,
    // and it is what gives the caller a model to report the failure on.
    if (models.empty()) return mapper.run();

    // The solve the cheap per-atom cadence is traded for: every atom in one
    // problem, intrinsics shared per camera group. Hundreds of forty-image
    // solves do not fill the device; this is the same work in one that does.
    if (opt.joint_after_atoms && models.size() >= opt.joint_min_models) {
        auto t0 = clk();
        mapper.jointRefine(models, aso.coarse_joint_ba);
        st.assemble.joint_ba++;
        st.assemble.t_ba += secs(t0, clk());
        if (opt.verbose)
            slog::diag(slog::Tag::Map, "[bup] joint refinement over %zu atom model(s): %.1f s",
                       models.size(), st.assemble.t_ba);
    }

    // ---- upwards, then the finishing passes -------------------------------
    AssembleOptions aopt = aso;
    aopt.verbose = opt.verbose;
    aopt.tag = "bup";
    return assembleModels(mapper, std::move(models), mopt, aopt, st.assemble);
}

}  // namespace sfm
