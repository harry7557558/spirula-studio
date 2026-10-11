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
#include <memory>
#include <stdexcept>
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
#include "sfm/map/ModelStore.h"
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
    size_t model_memory_bytes = 0;
    std::string scratch_dir;
    bool verbose = true;
};

struct BottomUpStats {
    size_t atoms = 0;
    size_t atom_images = 0;        // summed over atoms, so overlap counts twice
    size_t models_from_atoms = 0;
    int atom_threads = 1;
    double t_atoms = 0;
    size_t spilled_models = 0;
    size_t spill_rounds = 0;
    size_t peak_loaded_model_bytes = 0;
    // Everything after the atoms, which is the shared schedule.
    AssembleStats assemble;
};

namespace detail {

inline std::vector<Reconstruction> loadStoredModels(
    const ModelStore& store, const std::vector<StoredModel>& stored,
    const std::vector<size_t>& group, size_t limit, BottomUpStats& st) {
    size_t bytes = 0;
    std::vector<Reconstruction> models;
    models.reserve(group.size());
    for (size_t i : group) {
        if (stored[i].resident_bytes > limit - bytes)
            throw std::runtime_error("model group exceeds the bottom-up host memory budget");
        models.push_back(store.load(stored[i], limit - bytes));
        bytes = model_memory_detail::addBytes(bytes, modelResidentBytes(models.back()));
    }
    st.peak_loaded_model_bytes = std::max(st.peak_loaded_model_bytes, bytes);
    return models;
}

inline void seedStoredCameras(std::vector<Reconstruction>& models,
                              const std::map<uint32_t, Camera>& shared) {
    for (Reconstruction& model : models)
        for (auto& kv : model.cameras) {
            const auto it = shared.find(kv.first);
            if (it != shared.end()) kv.second = it->second;
        }
}

inline std::vector<Reconstruction> compactStoredModels(
    Mapper& mapper, ModelStore& store, std::vector<StoredModel> stored,
    const BottomUpOptions& opt, const ManagerOptions& mopt,
    const AssembleOptions& aso, BottomUpStats& st) {
    const size_t limit = opt.model_memory_bytes / 3;
    if (!limit) throw std::runtime_error("bottom-up model memory budget is too small");
    for (size_t round = 0; storedModelBytes(stored) > limit; ++round) {
        cancel::check();
        if (round >= static_cast<size_t>(std::max(1, aso.max_rounds)))
            throw std::runtime_error("bottom-up spill merge reached its round limit above the memory budget");
        if (!mopt.do_merge)
            throw std::runtime_error("atom models exceed the memory budget and merging is disabled");
        const auto groups = storedModelGroups(stored, limit);
        std::vector<StoredModel> next;
        std::map<uint32_t, Camera> shared;
        size_t merged = 0;
        for (const auto& group : groups) {
            cancel::check();
            auto models = loadStoredModels(store, stored, group, limit, st);
            if (group.size() > 1) {
                seedStoredCameras(models, shared);
                const auto t0 = std::chrono::steady_clock::now();
                mapper.jointRefine(models, aso.coarse_joint_ba);
                st.assemble.joint_ba++;
                st.assemble.t_ba += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - t0).count();
                for (const Reconstruction& model : models)
                    for (const auto& kv : model.cameras) shared.emplace(kv.first, kv.second);
                seedStoredCameras(models, shared);
                MergeOptions merge_opt = mopt.merge;
                merge_opt.duplicate = mopt.duplicate;
                const auto validate = seamValidator(mapper, mopt, &st.assemble.finish);
                merge_opt.validate = [limit, validate](Reconstruction& merged_model,
                    const Reconstruction& source, const Sim3& transform,
                    const MergeCounts& counts) -> std::string {
                    if (modelResidentBytes(merged_model) > limit)
                        return "host budget: merged model exceeds the resident model limit";
                    const std::string reason = validate
                        ? validate(merged_model, source, transform, counts) : std::string{};
                    if (reason.empty() && modelResidentBytes(merged_model) > limit)
                        return "host budget: refined merge exceeds the resident model limit";
                    return reason;
                };
                merge_opt.rigs = mapper.rigs();
                std::vector<char> stalled;
                size_t refused = 0;
                const auto tm = std::chrono::steady_clock::now();
                const size_t count = mergeLevel(models, merge_opt, {}, stalled, refused);
                merged += count;
                st.assemble.merges += count;
                st.assemble.merges_refused += refused;
                st.assemble.t_merge += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - tm).count();
            }
            size_t output_bytes = 0;
            for (const Reconstruction& model : models)
                output_bytes = model_memory_detail::addBytes(output_bytes, modelResidentBytes(model));
            if (output_bytes > limit)
                throw std::runtime_error("merged model group exceeds the bottom-up host memory budget");
            st.peak_loaded_model_bytes = std::max(st.peak_loaded_model_bytes, output_bytes);
            for (const Reconstruction& model : models) {
                next.push_back(store.put(model));
                st.spilled_models++;
            }
        }
        st.spill_rounds++;
        if (opt.verbose)
            slog::diag(slog::Tag::Map,
                       "[bup] spill level %zu: %zu models -> %zu, %zu groups, %zu merges; "
                       "loaded model peak %.1f MiB / %.1f MiB", st.spill_rounds,
                       stored.size(), next.size(), groups.size(), merged,
                       st.peak_loaded_model_bytes / 1048576.0, limit / 1048576.0);
        if (next.size() >= stored.size())
            throw std::runtime_error("bottom-up spill merge cannot reduce models to the memory budget");
        for (const StoredModel& meta : stored) store.erase(meta);
        stored = std::move(next);
    }
    std::vector<size_t> all(stored.size());
    std::iota(all.begin(), all.end(), size_t{0});
    return loadStoredModels(store, stored, all, limit, st);
}

}  // namespace detail

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
        size_t smallest = SIZE_MAX, biggest = 0;
        for (const std::vector<uint32_t>& a : atoms) {
            smallest = std::min(smallest, a.size());
            biggest = std::max(biggest, a.size());
        }
        slog::diag(slog::Tag::Map,
                   "[bup] %zu image(s) -> %zu atom(s) of %zu..%zu images (%.2fx cover)",
                   db.images.size(), atoms.size(), atoms.empty() ? 0 : smallest, biggest,
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
    std::unique_ptr<ModelStore> store;
    std::vector<std::vector<StoredModel>> stored_atoms;
    AtomModelSink sink;
    if (opt.model_memory_bytes) {
        store = std::make_unique<ModelStore>(opt.scratch_dir);
        stored_atoms.resize(atoms.size());
        sink = [&](size_t atom, size_t component, Reconstruction&& model) {
            if (modelResidentBytes(model) > opt.model_memory_bytes / 3)
                throw std::runtime_error("one atom model exceeds the bottom-up host memory budget");
            if (component != stored_atoms[atom].size())
                throw std::runtime_error("invalid atom model spill order");
            stored_atoms[atom].push_back(store->put(model));
        };
    }
    std::vector<Reconstruction> models =
        reconstructAtoms(db, feats, mapper.options(), mapper.cameraIds(),
                         mapper.startingCameras(), atoms, ao, as, mapper.rigs(),
                         mapper.sequences(), mapper.priors(), sink);
    st.t_atoms = as.secs;
    st.models_from_atoms = as.models;
    st.atom_threads = as.threads;
    if (opt.verbose)
        slog::diag(slog::Tag::Map,
                   "[bup] %zu atom(s) on %d thread(s) -> %zu model(s), %zu registrations, "
                   "%zu empty: %.1f s", as.atoms, as.threads, as.models, as.registered, as.empty,
                   as.secs);
    if (store) {
        std::vector<StoredModel> stored;
        for (auto& atom : stored_atoms)
            for (StoredModel& meta : atom) stored.push_back(std::move(meta));
        stored_atoms.clear();
        st.spilled_models = stored.size();
        if (opt.verbose)
            slog::diag(slog::Tag::Map,
                       "[bup] spilled %zu atom models; total estimated %.1f MiB, "
                       "resident model limit %.1f MiB (host budget %.1f MiB)", stored.size(),
                       storedModelBytes(stored) / 1048576.0,
                       opt.model_memory_bytes / (3.0 * 1048576.0),
                       opt.model_memory_bytes / 1048576.0);
        models = detail::compactStoredModels(mapper, *store, std::move(stored), opt, mopt, aso, st);
    }
    mapper.claimAll(models);
    // Every atom failed. The flat mapper will fail the same way and fail fast,
    // and it is what gives the caller a model to report the failure on.
    if (models.empty()) return mapper.run();

    // The solve the cheap per-atom cadence is traded for: every atom in one
    // problem, intrinsics shared per camera group. Hundreds of forty-image
    // solves do not fill the device; this is the same work in one that does.
    if (opt.joint_after_atoms && (models.size() >= opt.joint_min_models ||
                                (st.spill_rounds && models.size() > 1))) {
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
