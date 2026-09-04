// Reconstructing the atoms of a bottom-up run, concurrently.
//
// An atom is a few dozen images (sfm/map/Partition.h), and the incremental
// mapper over one is almost entirely host work: a two-view RANSAC per candidate
// seed, then P3P and triangulation per image, with bundle adjustments too small
// to occupy the device between them. Measured on a 5402-image capture, the atom
// phase was 721 s of a 1137 s mapping stage, on one core, one atom at a time.
//
// Atoms are independent by construction, so this runs them in parallel. What
// makes that possible without threading the mapper's state machine is to give
// each atom its *own* mapper over its own **sub-database**: the atom's images
// renumbered from zero, the verified pairs among them, and their features. Two
// consequences, both of which matter more than the parallelism:
//
//   * A Mapper over 48 images allocates 48 image records, not 5402. The shared
//     mapper's `rec_` carries a points2D / point3D_ids pair for every image in
//     the database whatever the restriction, so building a fresh mapper per
//     atom is *cheaper* than restricting one -- and resetModel(), which the
//     seed retries call, walks the whole database rather than the atom.
//   * A private object needs no locking anywhere. The only shared thing is the
//     Vulkan context each bundle adjustment runs on, and that is per worker:
//     a context is not thread-safe, and creating one costs far more than
//     reconstructing an atom, so it is created once per worker and handed to
//     every mapper that worker builds (Mapper::useBaContext).
//
// Camera ids stay global throughout. The intrinsics an atom must not fit for
// itself are bootstrapped once over the whole database and handed to every
// worker (Mapper::startingCameras), and the joint refinement that follows the
// atom phase shares intrinsics per group -- which is only meaningful if the
// group ids agree across atoms.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <exception>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Model.h"
#include "sfm/map/Mapper.h"
#include "sfm/vk/VkContext.h"

namespace sfm {

struct AtomOptions {
    // Workers, each with its own Vulkan context. 0 = hardware_concurrency,
    // clamped by `max_threads`: the contexts are the reason for a ceiling
    // (each maps two 64 MB staging buffers), and past a handful of them the
    // atoms' small solves are contending for one device anyway.
    int threads = 0;
    int max_threads = 8;
    // Bundle-adjustment cadence inside an atom. Far looser than the mapper's
    // global default because a forty-image solve does not fill the hardware,
    // and what it protects against is a bad registration in a model small
    // enough for the joint refinement afterwards to absorb.
    double ba_growth = 2.0;
    // Whether the atom's closing refinement is a tight one. See
    // MapperOptions::ba_final_tight: everything an atom converges is re-solved
    // jointly above it, three times over.
    //
    // Loosening the *growth* tolerance was tried alongside and is not the same
    // trade: rtol 1e-3 with patience 2 took a 379-image capture's median
    // relative rotation from 0.286 to 0.672 deg and its AUC@10 from 96.1 to
    // 92.5, to save two seconds. Iteration count is not where an atom's time is.
    bool tight_final_ba = false;
    // Seed attempts an atom may spend looking for further components inside
    // itself. An atom that fragments is usually dust, and what it leaves
    // behind is picked up by the tree's growth passes.
    int model_trials = 4;
    // Seed attempts for the atom's *primary* model, and how much of the atom
    // that model must cover to be kept. The mapper's own defaults (8 and 0.5)
    // are written for a whole capture, where failing to cover half of it is a
    // failure worth retrying from another seed. An atom is not a capture: a
    // partial one is merged and grown by the levels above, so trial
    // reconstructions spent covering it are work the schedule redoes anyway.
    // 0 keeps the mapper's own value.
    int init_trials = 8;
    double min_model_fraction = 0;
    bool verbose = true;
};

struct AtomStats {
    size_t atoms = 0;      // clusters handed in
    size_t models = 0;     // reconstructions they produced
    size_t registered = 0; // summed over models, so overlap counts twice
    size_t empty = 0;      // atoms that produced nothing
    int threads = 1;
    double secs = 0;
};

namespace detail {

// The atom's images renumbered 0..n-1, with the pairs among them.
struct SubDatabase {
    MatchesDatabase db;
    std::vector<FeatureSet> feats;
    std::vector<uint32_t> cam_ids;   // per local image, the *global* camera id
    std::vector<uint32_t> to_global; // local id -> database id
};

// `adj[i]` = indices into db.pairs of every pair image i takes part in. Built
// once for the whole database; a per-atom scan of db.pairs would be O(atoms x
// pairs), which on a capture with 300 atoms and 700k pairs is the dominant
// cost of the phase it is meant to make cheap.
inline std::vector<std::vector<uint32_t>> pairAdjacency(const MatchesDatabase& db) {
    std::vector<std::vector<uint32_t>> adj(db.images.size());
    std::vector<uint32_t> deg(db.images.size(), 0);
    for (const TwoViewMatches& p : db.pairs) {
        if (p.image1 < deg.size()) deg[p.image1]++;
        if (p.image2 < deg.size()) deg[p.image2]++;
    }
    for (size_t i = 0; i < adj.size(); i++) adj[i].reserve(deg[i]);
    for (uint32_t k = 0; k < db.pairs.size(); k++) {
        const TwoViewMatches& p = db.pairs[k];
        if (p.image1 < adj.size()) adj[p.image1].push_back(k);
        if (p.image2 < adj.size()) adj[p.image2].push_back(k);
    }
    return adj;
}

// Carve out one atom. `local` is scratch sized to the database, holding
// UINT32_MAX for images outside the atom; the caller reuses it across atoms so
// this stays O(atom), and it is left clean on return.
inline SubDatabase carveAtom(const MatchesDatabase& db, const std::vector<FeatureSet>& feats,
                             const std::vector<uint32_t>& cam_ids,
                             const std::vector<std::vector<uint32_t>>& adj,
                             const std::vector<uint32_t>& images,
                             std::vector<uint32_t>& local) {
    SubDatabase s;
    s.to_global = images;
    std::sort(s.to_global.begin(), s.to_global.end());
    s.to_global.erase(std::unique(s.to_global.begin(), s.to_global.end()), s.to_global.end());
    for (uint32_t i = 0; i < s.to_global.size(); i++) local[s.to_global[i]] = i;

    s.db.images.resize(s.to_global.size());
    s.feats.resize(s.to_global.size());
    s.cam_ids.resize(s.to_global.size());
    for (uint32_t i = 0; i < s.to_global.size(); i++) {
        const uint32_t g = s.to_global[i];
        s.db.images[i] = db.images[g];
        // Everything the mapper reads and nothing else. Descriptors are half a
        // megabyte an image and no mapping code touches them -- the `map`
        // subcommand does not even load them -- but `auto` still holds the ones
        // matching used, and copying those per atom would cost more memory than
        // the whole reconstruction.
        const FeatureSet& f = feats[g];
        FeatureSet& sf = s.feats[i];
        sf.width = f.width;
        sf.height = f.height;
        sf.extract_width = f.extract_width;
        sf.extract_height = f.extract_height;
        sf.exif_focal = f.exif_focal;
        sf.exif_camera = f.exif_camera;
        sf.keypoints = f.keypoints;
        sf.colors = f.colors;
        s.cam_ids[i] = cam_ids.empty() ? 1 : cam_ids[g];
    }
    // Take each pair from its lower endpoint so it is collected exactly once.
    for (uint32_t i = 0; i < s.to_global.size(); i++) {
        const uint32_t g = s.to_global[i];
        for (uint32_t k : adj[g]) {
            const TwoViewMatches& p = db.pairs[k];
            if (p.image1 != g) continue;
            if (p.image2 >= local.size() || local[p.image2] == UINT32_MAX) continue;
            s.db.pairs.push_back(p);
            s.db.pairs.back().image1 = i;
            s.db.pairs.back().image2 = local[p.image2];
        }
    }
    for (uint32_t g : s.to_global) local[g] = UINT32_MAX;
    return s;
}

// Renumber a reconstruction built on a sub-database back to database ids.
// Camera ids are global already and are left alone.
inline void toGlobalIds(Reconstruction& m, const std::vector<uint32_t>& to_global) {
    std::map<uint32_t, Image> images;
    for (auto& kv : m.images) {
        if (kv.first >= to_global.size()) continue;
        Image im = std::move(kv.second);
        im.id = to_global[kv.first];
        images[im.id] = std::move(im);
    }
    m.images = std::move(images);
    for (auto& kv : m.points3D)
        for (TrackElement& e : kv.second.track)
            if (e.image_id < to_global.size()) e.image_id = to_global[e.image_id];
}

}  // namespace detail

// Reconstruct every atom and return the models, in atom order. `base` is the
// mapper options for the whole run; `seed_mapper` supplies the bootstrapped
// intrinsics and is not otherwise used.
inline std::vector<Reconstruction> reconstructAtoms(
    const MatchesDatabase& db, const std::vector<FeatureSet>& feats,
    const MapperOptions& base, const std::vector<uint32_t>& cam_ids,
    const std::map<uint32_t, Camera>& start_cams,
    const std::vector<std::vector<uint32_t>>& atoms, const AtomOptions& opt, AtomStats& st) {
    const auto t0 = std::chrono::steady_clock::now();
    st.atoms = atoms.size();

    MapperOptions mo = base;
    mo.verbose = false;
    mo.threads = 1;  // the parallelism is over atoms; nesting only oversubscribes
    mo.ba_growth_ratio = std::max(1.0 + 1e-9, opt.ba_growth);
    mo.ba_final_tight = opt.tight_final_ba;
    // Every solve an atom runs is a coarse one (nothing here is the final
    // answer), so one scalar configuration covers the worker, and one context
    // with it -- which is what makes the per-worker context in the first place.
    if (!opt.tight_final_ba) mo.ba_real = mo.ba_real_coarse;
    mo.max_model_trials = opt.model_trials;
    // The focal was settled over the whole database before this ran. An atom
    // that searched again would be answering from a few dozen images the
    // question D48 exists to keep it away from, and would pay a trial
    // reconstruction per atom to do it.
    mo.focal_trials = 0;
    if (opt.init_trials > 0) mo.max_init_trials = opt.init_trials;
    if (opt.min_model_fraction > 0) mo.min_model_fraction = opt.min_model_fraction;
    mo.initial_cameras = start_cams;
    for (const auto& kv : start_cams) mo.known_focal_cameras.insert(kv.first);

    const std::vector<std::vector<uint32_t>> adj = detail::pairAdjacency(db);

    // An explicit count is taken literally; the default is capped, because the
    // ceiling exists for the contexts (two 64 MB staging buffers each) and for
    // a device that a handful of concurrent small solves already saturates.
    unsigned hc = std::thread::hardware_concurrency();
    int nt = opt.threads > 0 ? opt.threads
                             : std::min(hc > 0 ? (int)hc : 1, std::max(1, opt.max_threads));
    nt = std::max(1, std::min<int>(nt, (int)std::max<size_t>(atoms.size(), 1)));
    st.threads = nt;

    std::vector<std::vector<Reconstruction>> per_atom(atoms.size());
    std::atomic<size_t> next{0};
    std::mutex log_mu;
    std::atomic<size_t> done{0};
    // A worker whose device fails has to report itself: an exception crossing
    // a thread boundary is std::terminate, and a terminate handler is a worse
    // way to learn about a lost device than the message the failure carries.
    std::mutex err_mu;
    std::exception_ptr first_error;

    auto worker = [&] {
        try {
            // One context for every atom this worker builds. Creating a Vulkan
            // device takes longer than reconstructing an atom, and it cannot be
            // shared with another thread.
            VkContext ctx;
            std::vector<uint32_t> local(db.images.size(), UINT32_MAX);
            for (size_t i = next++; i < atoms.size(); i = next++) {
                detail::SubDatabase sub =
                    detail::carveAtom(db, feats, cam_ids, adj, atoms[i], local);
                Mapper m(sub.db, sub.feats, mo, sub.cam_ids);
                m.useBaContext(&ctx);
                uint32_t reg = 0;
                for (Reconstruction& r : m.run()) {
                    if (r.numRegistered() < 2) continue;
                    detail::toGlobalIds(r, sub.to_global);
                    reg += r.numRegistered();
                    per_atom[i].push_back(std::move(r));
                }
                if (opt.verbose) {
                    const size_t n = ++done;
                    std::lock_guard<std::mutex> lk(log_mu);
                    fprintf(stderr,
                            "[bup] atom %zu/%zu: %zu images -> %zu model(s), %u registered\n",
                            n, atoms.size(), atoms[i].size(), per_atom[i].size(), reg);
                }
            }
        } catch (...) {
            std::lock_guard<std::mutex> lk(err_mu);
            if (!first_error) first_error = std::current_exception();
            next = atoms.size();  // nothing left for the others to claim
        }
    };
    if (nt == 1) {
        worker();
    } else {
        std::vector<std::thread> pool;
        pool.reserve(nt);
        for (int t = 0; t < nt; t++) pool.emplace_back(worker);
        for (std::thread& t : pool) t.join();
    }
    if (first_error) std::rethrow_exception(first_error);

    std::vector<Reconstruction> models;
    for (std::vector<Reconstruction>& v : per_atom) {
        if (v.empty()) st.empty++;
        for (Reconstruction& r : v) {
            st.registered += r.numRegistered();
            models.push_back(std::move(r));
        }
    }
    st.models = models.size();
    st.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return models;
}

}  // namespace sfm
