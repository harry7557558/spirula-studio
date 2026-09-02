// Incremental Structure-from-Motion mapper (src/sfm/README.md).
//
// MVP control flow, ported in spirit from COLMAP's IncrementalMapper:
//   seed pair -> initialize (relative pose + triangulate)
//   loop: register-next (2D-3D via the correspondence graph, PnP RANSAC)
//         -> continue tracks + triangulate new points
//         -> periodic global BA + reprojection filtering
//
// Robustness (D36) follows COLMAP: strict seed acceptance with stepwise
// relaxation, PnP inlier-count and inlier-ratio gates with nonlinear pose
// refinement, iterated global BA + filtering (reprojection and triangulation
// angle) at 10% model growth, and de-registration of images the filtering
// hollows out -- the mapper's way of undoing a registration that stopped
// agreeing with the model.
//
// Remaining simplifications (src/sfm/README.md / D10): transitive tracks
// approximated by one-hop correspondences, global BA only (no local BA), no
// retriangulation/merge pass. Points are colored by averaging the
// per-keypoint colors sampled at extraction; each has a clear upgrade path.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "sfm/core/Progress.h"
#include "sfm/core/Features.h"
#include "sfm/core/Model.h"
#include "sfm/geometry/AbsolutePose.h"
#include "sfm/geometry/Triangulation.h"
#include "sfm/geometry/TwoView.h"
#include "sfm/core/Log.h"
#include "sfm/map/Bundle.h"
#include "i18n/catalog/Sfm.h"
#include "sfm/map/CorrespondenceGraph.h"
// For alignByStructure: the similarity two models' shared points determine, and
// the pixel scoring it is judged by, are the merger's (D70). Merge.h does not
// include this header, so there is no cycle.
#include "sfm/map/Merge.h"
#include "sfm/map/Profile.h"
#include "core/Env.h"

namespace sfm {

struct MapperOptions {
    double focal = 0;                  // 0 = COLMAP default (1.2 * max dim)
    // Every pixel threshold below is in *extraction* pixels -- the frame the
    // keypoints were measured in, not the frame they are stored in. They are
    // the same thing unless the extractor downscaled; Camera::pixel_scale is
    // the conversion, and it keeps `--max-error 4` meaning one thing across
    // quality presets and across a mixed-resolution capture (D47).
    double max_reproj_error = 3.0;     // extraction px (see above; D47)
    double min_tri_angle_deg = 1.5;
    // Seed acceptance (D36): thresholds are COLMAP's IncrementalMapper
    // defaults. If no candidate pair passes, initialize() relaxes them
    // stepwise rather than failing outright.
    double init_min_tri_angle_deg = 16.0;   // median angle over the seed points
    int init_min_inliers = 100;
    double init_max_forward_motion = 0.95;  // |baseline . viewing dir| cap
    // Registration gates: the ratio is COLMAP's abs_pose_min_inlier_ratio; it
    // rejects an image whose hundreds of 2D-3D correspondences agree only by
    // accident. The inlier count stays at 15 (COLMAP uses 30): sparse-match
    // sets live on 15-25 inlier registrations, and the transactional
    // refinement now catches the ones that turn out toxic (D36).
    int min_num_pnp_inliers = 15;
    double min_pnp_inlier_ratio = 0.25;
    // ... measured over the correspondences the pose could *possibly* explain,
    // not over every one offered (D69). A correspondence whose 3D point falls
    // behind the camera or outside the frame is not evidence against the pose;
    // it is evidence the pool contains points this view does not see, which a
    // dense capture of one room produces in bulk -- images looking at the far
    // wall match images looking back, and their points sit behind. Counting
    // those in the denominator is what makes an entirely sound registration
    // fail COLMAP's 0.25.
    //
    // It is deliberately *not* a licence to ignore competing places: a wrong
    // pose in a similar-looking room projects its rival's points in front of
    // the camera and inside the frame, so they stay in the denominator and the
    // gate still refuses. That is the distinction the raw inlier count could
    // not make -- admitting on absolute support alone recovered 48 images and 8
    // points of AUC on a 1146-image room capture and cost 20 points on a
    // 7620-image capture that is half building interior.
    bool pnp_ratio_visible_only = true;
    // Off, and measured: absolute support as a *substitute* for the ratio --
    // admit any consensus of this many correspondences however small a share of
    // the pool it is -- cannot tell the two reasons a pool is large apart. It
    // recovered 48 images and 8 points of AUC@10 on a 1146-image room capture
    // and cost 20 points on a 7620-image capture that is half building
    // interior, where it walked one incremental model from 2816 to 5379 images
    // straight through the interior and came back warped (median absolute
    // rotation error 4.1 deg against 0.6). The rival test below was added to
    // separate them and did not: it fired 91 times there and the capture got
    // worse still. What does separate them is the denominator, above.
    int strong_pnp_inliers = 0;
    // ... and that support has to be *unambiguous*, which is the other half of
    // the same observation. A pool is large for two opposite reasons. A dense
    // capture of one room offers the same place from twenty views, so the pool
    // is redundant and the ratio gate is mis-reading it. A building of similar
    // rooms offers places the image is not, so the pool is ambiguous and the
    // ratio gate is doing exactly its job. Absolute support cannot tell those
    // apart, and admitting the second is how a room ends up somewhere else:
    // measured, a 7620-image capture whose primary model grew from 2816 to 5379
    // images in one pass and came back with a median absolute rotation error of
    // 4.1 deg against 0.6, and 20 points less AUC@10.
    //
    // What tells them apart is whether a *second* pose explains the
    // correspondences the winner rejected. In a redundant pool those are noise
    // and nothing fits them; in an ambiguous one they are the other room. A
    // rival this large relative to the winner means the image has two plausible
    // places, and two plausible places is not evidence -- so the ratio gate
    // stands. 0 skips the test.
    double strong_pnp_max_rival = 0.5;
    int max_reg_trials = 3;            // per image, COLMAP's default
    // Rank the next image by how well its supported features are spread over
    // the frame (Mapper::pyramidSet) rather than by their raw count. Off is
    // the pre-existing count ranking.
    bool rank_by_visibility = true;
    // A seed retry starts somewhere no earlier attempt reached (D58). Off is
    // the pre-D58 behaviour, where retries could re-seed inside what the last
    // attempt registered and rebuild it. Exists to be turned off when
    // attributing a change: it decides which seed every model is built on, so
    // it moves results that have nothing to do with the retry loop.
    bool seed_blocking = true;
    // Focal search when a camera group registers its first image and its focal
    // is still the 1.2*max_dim guess (D18). COLMAP solves this with a P4Pf
    // minimal solver; a log-spaced sweep of P3P hypotheses is the cheaper
    // equivalent, and the ratio bounds are COLMAP's.
    int min_model_size = 10;           // COLMAP's default; smaller -> retry the seed
    // Re-decide planar-or-panoramic on a seed candidate's own inliers, rather
    // than taking verification's verdict (`config`) as it stands.
    //
    // It looks redundant -- only pairs verification labelled `Uncalibrated`
    // are ever offered as seeds (initializeAttempt filters on it) -- and it is
    // ~90% of the seed search, which was a third of the atom phase. It is not
    // redundant. Verification judges the *putative* matches; this judges the
    // inliers that survived, and the two disagree often enough to matter: over
    // five captures, turning it off cost 2.2 mean AUC@10 and saved nothing
    // overall (a 1322-image capture fell 9.3 points, an 896-image one 2.3,
    // while the atom-phase time it saved came back downstream).
    bool seed_homography = true;
    // Stop retrying seeds once a model is both >= min_model_size and covers
    // this fraction of the images. Below that the model is "a" reconstruction
    // but not "the" reconstruction, and another seed is usually worth the time.
    double min_model_fraction = 0.5;
    int max_init_trials = 8;           // seed attempts before keeping the best
    // Multiple models (D41). A capture that does not form one connected view
    // graph -- two ends of a building with nothing joining them, a sequence
    // broken by a featureless corridor -- yields several reconstructions, and
    // there is no correct way to fuse them without knowing the transform
    // between them. COLMAP's answer is to emit them all as sparse/0, sparse/1,
    // ... for a later merge step, and this matches it:
    //   * the primary model is still the best of max_init_trials seed attempts
    //     (D19) -- unchanged, so a scene that reconstructs as one model behaves
    //     exactly as before;
    //   * afterwards, seeds are searched among images no kept model registered
    //     (COLMAP FindFirstInitialImage's num_registrations == 0 rule) and each
    //     resulting model is kept if it brings min_model_size images no kept
    //     model reached;
    //   * a sub-model may re-register images an earlier model already holds --
    //     that overlap is what a later merge aligns on -- for as long as it is
    //     still finding images nothing holds (max_model_overlap and
    //     model_overlap_ratio below).
    // max_num_models 1 restores single-model output.
    int max_num_models = 50;           // COLMAP's default
    // What a growth pass may take from models already kept. COLMAP stops at an
    // absolute count because it never merges two models, so everything a second
    // model re-registers is waste. Here it is the opposite: overlap is the only
    // evidence a merge has, and a component cut off from its neighbour the
    // moment it touches it can neither cover its own territory nor align with
    // anything (D66). So the count is a floor -- what a Sim(3) needs -- and past
    // it a pass earns one shared image for every `model_overlap_ratio` images
    // it finds that nothing else holds. Ratio 0 restores COLMAP's rule.
    //
    // Measured on a 7620-image capture, where this rule fired 17 times and the
    // sub-models it stopped averaged 100 images against a region several
    // hundred wide.
    int max_model_overlap = 20;        // COLMAP's default
    double model_overlap_ratio = 1.0;
    // Total seed attempts the further-models search may spend, counting the
    // ones discarded for being under min_model_size (COLMAP's init_num_trials,
    // same role and same default). Without it a dataset whose leftovers are
    // dust keeps seeding two-image models until the candidate pair list runs
    // out, which on a 2000-image scene is tens of thousands of attempts.
    int max_model_trials = 20;  // 200
    int focal_search_samples = 15;     // 0 disables the search
    double min_focal_ratio = 0.1, max_focal_ratio = 10.0;
    // Focal bootstrap (D48), for the captures where bundle adjustment cannot
    // recover the focal from the 1.2*max_dim guess: rotation-degenerate motion
    // (a dashcam, a dolly, a rail scan) and genuinely wide lenses. See
    // bootstrapFocalLength. 0 = off; the descent usually stops after 1-2.
    int focal_trials = 5;
    // Relative gain in mutually consistent observations that justifies moving
    // the focal. The guess is at least a *consistent* place to start and a young
    // model's observation count is a noisy statistic, so a hypothesis a few
    // percent ahead of it has shown nothing. Measured separations are far
    // larger on a few real-world datasets.
    double focal_min_gain = 0.15;
    size_t focal_model_size = 20;      // images per trial model
    // Reported, not a gate: the orientation spread below which the focal is a
    // free parameter of the reconstruction, so a bad one leaves no trace in the
    // residual. A straight KITTI drive spans 8.5 deg over 114 frames; the
    // tightest ordinary capture measured (mip-NeRF 360 garden's first 20 images,
    // one corner of an object orbit) spans 32.
    double focal_max_rot_spread_deg = 15.0;
    // Global-refinement cadence and image filtering (D36; D38). COLMAP's
    // trigger ratio, kept: D37's 1.25 measured fine on easy sets but cost
    // registration + pose accuracy on a real-world dataset -- fewer refine rounds
    // means fewer retriangulation passes, exactly the machinery D36 added for
    // such sets. Speed comes from convergence-adaptive iterations + the
    // persistent solver instead (D38).
    double ba_growth_ratio = 1.1;      // COLMAP ba_global_images_ratio
    int ba_max_refinements = 5;        // final pass; growth passes use 2
    double ba_refine_change = 0.0005;  // stop when changed-obs fraction is below
    // Growth-phase BAs stop when relative cost improvement stays below
    // ba_growth_rtol for ba_growth_patience accepted steps (D38): iteration
    // count adapts to convergence -- a shaky young model keeps iterating, a
    // converged large one stops after a few -- replacing D37's fixed
    // iteration cap, which starved exactly the models that still needed work.
    // Final passes keep the solver's tight defaults. 0 = solver default.
    double ba_growth_rtol = 1e-4;
    int ba_growth_patience = 5;
    // Whether the *final* refinement is one of those tight passes. It is for a
    // mapper that owns the answer. It is not for a bottom-up atom: the model it
    // finishes is re-solved jointly the moment the atom phase ends, and again at
    // every merge level above it, so converging a forty-image model to the
    // solver's full tolerance is work thrown away three times over.
    bool ba_final_tight = true;
    // Reprojection acceptance for retriangulation/track completion, as a
    // fraction of max_reproj_error (the churn hysteresis); 0 disables the
    // retriangulation pass entirely.
    double retri_scale = 0.75;
    // Fuse two 3D points a correspondence says are the same feature
    // (Mapper::mergeTracks). Off is the pre-existing behaviour: creation-only
    // tracks, which fragment wherever a point was triangulated twice before the
    // pose joining the two halves existed. Measured worth 13 images and 2.4
    // AUC@5 on a 1146-image capture, at the cost of the solver time longer
    // tracks bring -- which kMergeMaxTrack is what bounds.
    bool merge_tracks = true;
    // Auditing an assembled model (D44). An image is put back only when the
    // structure it did *not* bring supports a competing pose: one that clears
    // the registration gates, explains `audit_alternative_factor` times as
    // many of those correspondences as the current pose does, and points
    // somewhere else. The evidence floor keeps the test off images the model
    // barely sees, where a lucky RANSAC on 20 stray correspondences would
    // otherwise unseat a perfectly good pose.
    int audit_min_evidence = 40;
    int audit_min_alternative = 25;
    double audit_alternative_factor = 3.0;
    double audit_min_rotation_deg = 5.0;
    // ... or a camera-center shift of this fraction of the model's own scale
    // (the RMS spread of its camera centers), which is what catches an image
    // that kept its orientation and moved.
    double audit_min_shift_frac = 0.01;
    int audit_ransac_trials = 1000;
    int min_image_points = 5;          // de-register images that fall below this
    double max_extra_param = 1.0;      // |distortion param| beyond this = bogus
    CamModel camera_model = CamModel::Radial;  // distortion model for new cameras (D29)
    // Starting intrinsics per camera id, built by sfm/core/CameraSetup.h from
    // --camera-model / --focal / EXIF (D46). A camera with no entry here falls
    // back to Camera::defaultFor(focal, camera_model), the old behaviour, so
    // the self-tests and any library caller are unaffected.
    std::map<uint32_t, Camera> initial_cameras;
    // Cameras whose focal came from a *per-group* prior (EXIF, or a --focal
    // that named the group) rather than a guess. The registration focal sweep
    // leaves these alone; a dataset-wide --focal is not in here, deliberately,
    // because it says nothing about which group it describes (D45/D46).
    std::set<uint32_t> known_focal_cameras;
    // Cameras whose focal was *supplied* -- a per-group prior, EXIF, or a
    // dataset-wide --focal (CameraSetup::focal_given). Superset of the above.
    // The focal bootstrap only searches cameras outside it: a supplied focal may
    // be worth refining, but it is never worth replacing with a guess (D48).
    std::set<uint32_t> given_focal_cameras;
    // Cameras whose focal the two-view stage *measured* (D53's epipolar vote,
    // or the fisheye peripheral-inlier search). Not a prior in the sense above:
    // the bootstrap still builds its probe model, because the probe's bundle
    // adjustment refines the value and a refined measurement beats a raw one.
    // What it does skip is the halving ladder, which exists to escape a guess
    // that is wrong by a factor and has nothing to offer a measurement.
    std::set<uint32_t> measured_focal_cameras;
    std::string ba_loss = "huber";     // robust loss for mapping-time BA (D36)
    // Linear solver for the reduced camera system; see BundleOptions::solver.
    std::string ba_solver = "auto";
    double ba_loss_param = 2.0;        // Huber delta / Cauchy c, in extraction px
    // Let BA move each camera's principal point off the image centre. Off, as
    // in COLMAP: it is nearly the same parameter as a camera rotation, so on a
    // rig its per-group drift lands in the relative orientation of the lenses
    // (D50). See BundleOptions::refine_principal_point.
    bool refine_principal_point = false;
    // Release it for one global bundle adjustment at the very end, when the
    // model is complete -- COLMAP's documented advice, and a different question
    // from refining it *during* reconstruction (D51). `Mapper::polish` is what
    // runs it; the CLI drives that, not the mapper's own loop.
    size_t pp_min_images = 20;   // ... for groups with at least this many images
    // Let BA move the distortion coefficients during reconstruction. On, as in
    // COLMAP; off holds them at the camera setup's value -- zero, or what
    // --distortion gave them -- and leaves them to the finishing pass (D72).
    bool refine_extra_params = true;
    // Scalar the solver computes in (sfm/ba/README.md "Scalar configs"). "df"
    // is an fp32 pair with a ~49-bit significand, which on hardware whose fp64
    // rate is a small fraction of its fp32 rate -- every consumer card -- can be
    // the faster way to the same answer.
    std::string ba_real = "double";
    // ... and the scalar for solves whose answer is provisional: a growth-phase
    // refinement, or a merge-tree level that another level will re-solve. Those
    // only need a step good enough to register and filter against, and fp32
    // halves the bytes every kernel moves -- worth 25-35% of the mapping stage.
    //
    // It is nonetheless **double by default**, because fp32 there costs both
    // accuracy and reproducibility. The Schur and Jacobian kernels accumulate
    // with floating-point atomics, whose execution order is arbitrary; at fp64
    // the resulting perturbation (~1e-16) never crosses a decision threshold
    // and the whole pipeline is reproducible run to run, while at fp32 (~1e-7)
    // it crosses them constantly. Measured over three identical runs each: a
    // 379-image capture scored 96.1 AUC@10 every time in fp64 and 96.5 / 92.3 /
    // 91.5 in fp32; an 896-image one 88.3 / 87.7 / 88.0 against 85.2 / 87.5 /
    // 87.1. Not just noisier -- worse on average, by 2.7 and 1.4 points.
    std::string ba_real_coarse = "double";
    int device = -1;
    // Host worker threads for the passes that fan out over points
    // (filterPoints) and for a bundle adjustment that runs on the host.
    // 0 = hardware_concurrency.
    int threads = 0;
    bool verbose = true;
};

class Mapper {
public:
    // `camera_ids[i]` is the (1-based) camera image i belongs to; images sharing
    // an id share intrinsics and one BA parameter group. Empty = one shared
    // camera for everything, the old behaviour. The CLI builds this from
    // --camera-mode (D17).
    Mapper(const MatchesDatabase& db, const std::vector<FeatureSet>& feats, MapperOptions opt,
           std::vector<uint32_t> camera_ids = {})
        : db_(db), feats_(feats), opt_(opt), cam_ids_(std::move(camera_ids)) {}

    // All reconstructions the dataset supports, largest first (by 3D point
    // count -- COLMAP's ReconstructionManager::Write ordering, so models[0] is
    // what lands in sparse/0). Never empty in practice: a dataset that seeds at
    // all yields at least one model, and one that does not yields a single
    // empty reconstruction so callers have something to report on.
    std::vector<Reconstruction> run() {
        auto prof_start = std::chrono::steady_clock::now();
        ensureSetup();
        std::vector<Reconstruction> models;

        // ---- phase 1: the primary model, best of max_init_trials seeds ----
        // A reconstruction is only as good as its seed: the densest verified
        // pair can sit in a small corner of the view graph, and then the model
        // stops after a handful of images with no way to notice. COLMAP's
        // answer is to discard a model smaller than `min_model_size` and start
        // again from the next candidate seed; we keep the largest attempt (D19)
        // -- and, since D41, *all* the attempts, because on a fragmented
        // capture the rejected ones are exactly the other components. They are
        // admitted below rather than rebuilt from scratch.
        // Before anything is built on it: is the focal something this capture
        // can actually determine? (D48; a no-op unless it cannot.)
        {
            ProfTimer pt(g_map_prof.init_seed);
            ProfTimer pb(g_map_prof.bootstrap);
            bootstrapFocalLength();
        }

        std::vector<Reconstruction> attempts;
        size_t seed_from = 0;
        seeded_.clear();  // seed blocking is a device of this loop alone (D58)
        for (int attempt = 0; attempt < std::max(1, opt_.max_init_trials); attempt++) {
            resetModel();
            bool seeded;
            {
                ProfTimer pt(g_map_prof.init_seed);
                seeded = initialize(seed_from);
            }
            if (!seeded) {
                if (opt_.verbose && attempt == 0) reportInitFailure();
                break;
            }
            globalRefine(false);  // COLMAP's two-view BA before any growth
            grow();
            uint32_t reg = rec_.numRegistered();
            if (reg) {
                attempts.push_back(snapshotModel());
                if (opt_.seed_blocking) blockSeeds(attempts.back());
            }
            const uint32_t enough = (uint32_t)std::max(
                (double)opt_.min_model_size, opt_.min_model_fraction * allowedCount());
            if (reg >= enough) break;
            if (opt_.verbose)
                slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_model_too_small,
                         {(long long)reg, (long long)enough, (long long)seededImages()});
        }
        seeded_.clear();
        if (attempts.empty()) {
            // Nothing seeded at all. Hand back one empty reconstruction so the
            // caller has a model to report on, as before D41.
            resetModel();
            models.push_back(snapshotModel());
            return finishRun(models, prof_start);
        }

        // Largest attempt first, so the primary model is the one D19 would have
        // returned; each further attempt is admitted only if it brings images the
        // ones before it did not. Seeds land in the same component all the time,
        // and writing three views of one component as three models would be worse
        // than useless -- but an attempt that *overlaps* one already kept and
        // still covers new ground is the other half of a capture, not a copy of
        // this one, and admitModel keeps it (D66).
        std::stable_sort(attempts.begin(), attempts.end(),
                         [](const Reconstruction& a, const Reconstruction& b) {
                             return a.numRegistered() > b.numRegistered();
                         });
        for (size_t i = 0; i < attempts.size(); i++) {
            if ((int)models.size() >= std::max(1, opt_.max_num_models)) break;
            std::string why;
            if (i > 0 && !admitModel(attempts[i], why)) {
                if (opt_.verbose)
                    fprintf(stderr, "[map] seed attempt discarded: %s\n", why.c_str());
                continue;
            }
            claimImages(attempts[i]);
            recordCameras(attempts[i]);
            models.push_back(std::move(attempts[i]));
        }

        // ---- phase 2: further models from whatever is still unclaimed ----
        // Seeds come only from images no kept model registered, so this does
        // nothing at all when the primary model covers the dataset -- a
        // single-component capture takes the pre-D41 path exactly.
        seedFurtherModels(models);
        return finishRun(models, prof_start);
    }

    // ---- the engine, driven from outside (D44) ----------------------------
    //
    // run() is one policy over these; the assembler (sfm/map/Assemble.h) is
    // another, and a bottom-up hierarchical mapper would be a third. They all
    // need the same three operations on an *existing* model, which is why they
    // are public: keep growing it, refine it, or look for more models beside
    // it. Every one of them starts by adopting the model into `rec_`, so the
    // mapper never has to have built it itself -- it may equally have come off
    // disk or out of a merge.

    struct GrowStats {
        uint32_t before = 0, after = 0;
        uint32_t registered = 0;   // images this pass brought in
        bool refined = false;      // whether a final refinement ran
    };

    // Adopt `m` and keep registering into it until nothing else fits. An image
    // another model holds is a legitimate target here -- the overlap it creates
    // is what lets the two models merge afterwards (D43) -- bounded by
    // `overlapBudget`, so the pass may keep taking them while it is still
    // finding images of its own. Pass the other models in `others` to get that
    // bound; with `others` empty the pass is unbounded, and on a fragmented
    // capture that means every model re-registers the whole dataset before the
    // redundant ones are dropped again, which is several full reconstructions'
    // worth of work for nothing.
    //
    // A pass that registers nothing returns `m` untouched -- not a re-refined
    // copy of it. That is what makes the manager's grow round free on a
    // dataset it cannot help: no BA runs, no numbers move.
    // `max_reg` caps the size the model may reach (0 = grow until nothing
    // registers). The overlap bound above only limits images *another model*
    // holds; images nothing covers are unbounded, so a model beside a large
    // uncovered region grows into all of it. That is a full reconstruction, and
    // a caller that wanted a bridge rather than a reconstruction needs to say
    // so.
    Reconstruction continueFrom(const Reconstruction& m, GrowStats* out = nullptr,
                                const std::vector<const Reconstruction*>& others = {},
                                uint32_t max_reg = 0) {
        ensureSetup();
        resetModel();
        adopt(m);
        model_count_.clear();
        for (const Reconstruction* o : others)
            if (o != &m) claimImages(*o);
        rebuildScores();
        GrowStats st;
        st.before = rec_.numRegistered();
        st.registered = growLoop(max_reg);
        st.after = rec_.numRegistered();
        if (out) *out = st;
        if (!st.registered) return m;
        st.refined = true;
        checkedRefine(true);
        if (out) {
            out->refined = true;
            out->after = rec_.numRegistered();
        }
        return snapshotModel();
    }

    // ---- aligning two models that share no image (D70) --------------------
    //
    // A building walked room by room reconstructs as a model per room, and two
    // of them can see the same doorway without either registering a single
    // image the other did: alignReconstructions has nothing to fit, and the
    // pair is never even proposed as a merge candidate. Measured on a
    // 7620-image capture half of which is a building interior, the two largest
    // models -- 5474 and 474 images -- shared fewer than three.
    //
    // The correspondence graph knows better. Two images matched and were
    // verified long before any model existed; if one model triangulated the
    // matched feature on one side and the other model triangulated it on the
    // other, that is one 3D point expressed in two gauges. Enough of them
    // determine the similarity between the gauges, and the merge that follows
    // is the ordinary one, judged by every ordinary test.

    // Model pairs the correspondence graph joins, most evidence first, counted
    // in matched features rather than pairs: what the alignment consumes is
    // correspondences, and two images with 400 matches are worth more than ten
    // with twenty.
    struct StructureLink {
        size_t a = 0, b = 0;
        size_t matches = 0;
    };
    std::vector<StructureLink> structureLinks(const std::vector<Reconstruction>& models,
                                              size_t min_matches) const {
        std::unordered_map<uint32_t, std::vector<uint32_t>> in_model;
        for (size_t i = 0; i < models.size(); i++)
            for (const auto& kv : models[i].images)
                if (kv.second.registered) in_model[kv.first].push_back((uint32_t)i);
        std::unordered_map<uint64_t, size_t> w;
        for (const TwoViewMatches& p : db_.pairs) {
            auto ia = in_model.find(p.image1), ib = in_model.find(p.image2);
            if (ia == in_model.end() || ib == in_model.end()) continue;
            for (uint32_t x : ia->second)
                for (uint32_t y : ib->second) {
                    if (x == y) continue;
                    const uint32_t lo = std::min(x, y), hi = std::max(x, y);
                    w[((uint64_t)lo << 32) | hi] += p.matches.size();
                }
        }
        std::vector<StructureLink> out;
        for (const auto& kv : w) {
            if (kv.second < min_matches) continue;
            out.push_back({(size_t)(kv.first >> 32), (size_t)(kv.first & 0xffffffffu), kv.second});
        }
        // Deterministic: the hash order is unspecified, so a total order first.
        std::sort(out.begin(), out.end(), [](const StructureLink& x, const StructureLink& y) {
            return x.a != y.a ? x.a < y.a : x.b < y.b;
        });
        std::stable_sort(out.begin(), out.end(),
                         [](const StructureLink& x, const StructureLink& y) {
                             return x.matches > y.matches;
                         });
        return out;
    }

    // The similarity taking `src`'s world onto `dst`'s, fitted to points both
    // triangulated. Scored in pixels like every other alignment here: the src
    // point is carried into dst's gauge and has to reproject where dst's own
    // image saw its partner, which is a test the fit itself never used.
    AlignmentResult alignByStructure(const Reconstruction& dst, const Reconstruction& src,
                                     const MergeOptions& opt, size_t max_corr = 6000) const {
        AlignmentResult r;
        struct Corr {
            Vec3 d, s;             // the point in each model's gauge
            uint32_t img, feat;    // the dst observation that scores it
        };
        std::vector<Corr> corr;
        std::set<std::pair<uint64_t, uint64_t>> seen;  // one vote per point pair
        for (const TwoViewMatches& p : db_.pairs) {
            for (int flip = 0; flip < 2; flip++) {
                const uint32_t ia = flip ? p.image2 : p.image1;
                const uint32_t ib = flip ? p.image1 : p.image2;
                auto da = dst.images.find(ia);
                auto sb = src.images.find(ib);
                if (da == dst.images.end() || sb == src.images.end()) continue;
                if (!da->second.registered || !sb->second.registered) continue;
                for (const FeatureMatch& fm : p.matches) {
                    const uint32_t fa = flip ? fm.idx2 : fm.idx1;
                    const uint32_t fb = flip ? fm.idx1 : fm.idx2;
                    if (fa >= da->second.point3D_ids.size() ||
                        fb >= sb->second.point3D_ids.size())
                        continue;
                    const uint64_t pd = da->second.point3D_ids[fa];
                    const uint64_t ps = sb->second.point3D_ids[fb];
                    if (pd == kInvalidPoint3D || ps == kInvalidPoint3D) continue;
                    auto itd = dst.points3D.find(pd);
                    auto its = src.points3D.find(ps);
                    if (itd == dst.points3D.end() || its == src.points3D.end()) continue;
                    if (!seen.insert({pd, ps}).second) continue;
                    corr.push_back({itd->second.xyz, its->second.xyz, ia, fa});
                }
            }
        }
        r.structure_pairs = corr.size();
        r.from_structure = true;
        const size_t need = (size_t)std::max(3, opt.min_common_images);
        if (corr.size() < need) {
            r.reason = "only " + std::to_string(corr.size()) +
                       " point(s) triangulated by both models";
            return r;
        }
        // A long walk can produce hundreds of thousands of these, and RANSAC
        // scores every one of them on every trial. Thinning keeps the spread --
        // the sample is strided, not truncated, so it is not one end of the
        // seam.
        if (corr.size() > max_corr) {
            std::vector<Corr> thin;
            thin.reserve(max_corr);
            const double step = (double)corr.size() / (double)max_corr;
            for (size_t k = 0; k < max_corr; k++) thin.push_back(corr[(size_t)(k * step)]);
            corr.swap(thin);
        }

        const int n = (int)corr.size();
        auto fit = [&](const std::vector<int>& idx) {
            std::vector<Sim3> out;
            std::vector<Vec3> a, b;
            for (int i : idx) { a.push_back(corr[i].s); b.push_back(corr[i].d); }
            Sim3 t;
            if (estimateSim3(a, b, t)) out.push_back(t);
            return out;
        };
        auto res = [&](const Sim3& t, int i) {
            const Corr& c = corr[i];
            const Image& im = dst.images.at(c.img);
            auto cam = dst.cameras.find(im.camera_id);
            if (cam == dst.cameras.end()) return 1e30;
            const double e = reprojErrorAt(cam->second, im.pose, kp(c.img, c.feat),
                                           transformPoint(t, c.s));
            return e * e;
        };
        RansacOptions ro;
        ro.max_error = opt.max_reproj_error;
        ro.seed = opt.seed;
        ro.max_num_trials = opt.ransac_max_trials;
        RansacReport<Sim3> rep = loransac<Sim3>(n, 3, fit, fit, res, ro);
        if (!rep.success || rep.num_inliers < (int)need) {
            r.reason = "alignment on shared structure found only " +
                       std::to_string(rep.success ? rep.num_inliers : 0) + "/" +
                       std::to_string(n) + " consistent point(s)";
            return r;
        }
        // The same inlier-ratio bar the pose alignment applies, on the same
        // reasoning: a handful of agreeing points out of thousands is a
        // coincidence between two similar places, not a transform.
        if ((double)rep.num_inliers < opt.min_inlier_ratio * (double)n) {
            r.reason = "only " + std::to_string(rep.num_inliers) + "/" + std::to_string(n) +
                       " shared points agree";
            return r;
        }
        double sum = 0;
        for (int i = 0; i < n; i++)
            if (rep.inlier_mask[i]) sum += std::sqrt(res(rep.model, i));
        r.transform = rep.model;
        r.inliers = (size_t)rep.num_inliers;
        r.mean_error = sum / (double)rep.num_inliers;
        r.common_images = sharedImages(src, dst).size();
        r.success = true;
        return r;
    }

    // Register what this model can still take, by PnP alone (D57).
    //
    // No bundle adjustment, no refinement, and it stops as soon as one would be
    // due. The caller is expected to follow a whole round of these with one
    // joint refinement over every model (jointRefine), and that is the point: a
    // level that grows twenty models pays for one solve instead of twenty.
    // Bundle adjustments on small components were most of what a bottom-up run
    // spent -- growing 37 models through `continueFrom` cost 8000 solves,
    // because each refines on its own growth schedule and then audits, and the
    // audit refines again.
    //
    // Only what this pass registered is checked, and a contradicted image is
    // de-registered rather than repaired: the rest of the model was audited
    // when it was built or merged, and moving a pose needs exactly the
    // refinement this call exists to avoid. Dropping an image costs nothing --
    // a later pass can register it again.
    Reconstruction growByPnP(const Reconstruction& m, GrowStats* out,
                             const std::vector<const Reconstruction*>& others, uint32_t max_reg,
                             uint32_t* rejected = nullptr) {
        ensureSetup();
        resetModel();
        adopt(m);
        model_count_.clear();
        for (const Reconstruction* o : others)
            if (o != &m) claimImages(*o);
        rebuildScores();
        GrowStats st;
        st.before = rec_.numRegistered();
        st.registered = growLoop(max_reg, /*stop_at_ba=*/true);
        if (st.registered) {
            modelScale();  // warm the lazy cache poseContradicted reads
            const std::vector<uint32_t> fresh = recent_regs_;
            uint32_t bad = 0;
            for (uint32_t img : fresh) {
                Pose alt;
                if (!rec_.images.at(img).registered || !poseContradicted(img, alt)) continue;
                deregisterImage(img);
                bad++;
            }
            if (rejected) *rejected = bad;
            st.registered -= std::min(st.registered, bad);
        }
        st.after = rec_.numRegistered();
        if (out) *out = st;
        if (!st.registered) return m;  // unchanged: hand back the original
        return snapshotModel();
    }

    // Choose the starting intrinsics before any model is built (D48). Part of
    // run(); public so a bottom-up schedule can do it once over the whole
    // database, instead of leaving the choice to whichever small piece of the
    // capture it happens to reconstruct first.
    void bootstrapCameras() {
        ensureSetup();
        bootstrapFocalLength();
    }

    // Adopt and bundle-adjust, with the mapper's own filtering and
    // de-registration rules. This is what a merged model needs: two halves
    // glued along a seam that has never been optimized as one.
    //
    // `coarse` runs the growth-phase schedule instead of the final one -- two
    // refinement rounds at the loose tolerance rather than five at the tight
    // one. For a caller asking a yes/no question about the result (can this
    // seam be reconciled at all?) the last digits of convergence decide
    // nothing, and something else optimizes the model properly afterwards.
    Reconstruction refine(const Reconstruction& m, bool coarse = false) {
        ensureSetup();
        resetModel();
        adopt(m);
        rebuildScores();
        const bool tight = opt_.ba_final_tight;
        opt_.ba_final_tight = tight && !coarse;
        globalRefine(true);
        opt_.ba_final_tight = tight;
        return snapshotModel();
    }

    // The same, in place, for a caller that is asking a question rather than
    // producing an answer: false means the solve did not fit the device and
    // `m` is untouched. A single model's bundle adjustment cannot be split the
    // way a joint one can (D65) -- its points are shared across all of it --
    // so "it does not fit" is a real verdict and the caller has to have one.
    bool refineIfItFits(Reconstruction& m, bool coarse = false) {
        ba_over_budget_throws_ = true;
        try {
            m = refine(m, coarse);
        } catch (const BAOverBudget& e) {
            ba_over_budget_throws_ = false;
            if (opt_.verbose)
                fprintf(stderr, "[map] a %u-image refinement needs %.0f MB against a %.0f MB "
                        "budget; declining it\n", m.numRegistered(), e.need_mb, e.budget_mb);
            return false;
        }
        ba_over_budget_throws_ = false;
        return true;
    }

    // One more global bundle adjustment on a *finished* model, with what the
    // mapper held now released: the principal point (D51) and the distortion
    // coefficients (D72). See src/sfm/README.md, "The finishing passes".
    Reconstruction polish(const Reconstruction& m, bool free_pp = true, bool free_extra = false) {
        // This run's grouping, not the model's ids: a model read back from disk
        // carries one camera per frame size (splitCamerasBySize).
        std::set<uint32_t> groups;
        for (const auto& kv : m.images)
            if (kv.second.registered)
                groups.insert(kv.first < cam_ids_.size() ? cam_ids_[kv.first]
                                                        : kv.second.camera_id);
        // Two groups or more and each principal point drifts its own way; the
        // difference is a real error in their relative orientation, which on a
        // dual-fisheye rig cost 21 points of AUC (D51).
        const bool pp = free_pp && groups.size() == 1;
        if (free_pp && !pp && opt_.verbose)
            slog::err(slog::Tag::Map, spirula::i18n::msg::sfm::map_pp_skipped,
                      {(long long)groups.size()});
        // Releasing what mapping was already refining is a solve that ends
        // where it started.
        const bool extra = free_extra && !opt_.refine_extra_params;
        if (!pp && !extra) return m;
        ensureSetup();
        resetModel();
        adopt(m);
        rebuildScores();
        std::map<uint32_t, Vec2> before;
        for (const auto& kv : rec_.cameras) before[kv.first] = {kv.second.cx, kv.second.cy};
        final_.pp = pp;
        final_.extra = extra;
        globalRefine(true);
        final_ = FinalRelease{};
        if (opt_.verbose)
            for (const auto& kv : rec_.cameras) {
                const Vec2& b = before[kv.first];
                const double d = std::hypot(kv.second.cx - b.x, kv.second.cy - b.y);
                if (d > 1e-9)
                    fprintf(stderr,
                            "[map] camera %u principal point %.1f,%.1f -> %.1f,%.1f (%.1f px)\n",
                            kv.first, b.x, b.y, kv.second.cx, kv.second.cy, d);
            }
        return snapshotModel();
    }

    // The same finished model with every image on its own intrinsics (D73).
    // Sharing a camera is what makes a focal observable while the model is
    // being built; only a complete one can pay for each frame to depart.
    Reconstruction perImageIntrinsics(const Reconstruction& m, bool free_extra = true) {
        if (m.numRegistered() < 2) return m;
        ensureSetup();
        resetModel();
        adopt(m);
        rebuildScores();
        if (!splitCamerasPerImage()) return m;
        final_.extra = free_extra;
        final_.no_sanitize = true;
        globalRefine(true);
        final_ = FinalRelease{};
        return snapshotModel();
    }

    struct AuditStats {
        uint32_t checked = 0, deregistered = 0, unsupported = 0, reregistered = 0;
    };

    // Ask the correspondence graph whether every image in `m` belongs where
    // `m` says it does, and move the ones that do not (D44).
    //
    // A model assembled from parts -- merged (D43), or stacked up by a
    // hierarchical mapper -- can place an image somewhere its own observations
    // still support, because those observations came along with it. What it
    // cannot fake is the rest of the model: if the structure the image did
    // *not* bring supports a different pose decisively better (see
    // poseContradicted), the image is in the wrong place, and no amount of
    // bundle adjustment will walk it back.
    //
    // A contradicted image is moved to the pose that won and re-triangulated
    // there, rather than deleted. Deleting was tried first and cost 60 images
    // on one a merge on a dataset: re-registration applies the full
    // registration gates, including an inlier *ratio* that a misplaced image's
    // huge junk-dominated correspondence pool cannot meet, so the ones the
    // audit unseated could not come back. The evidence that selected the new
    // pose is weaker than registration demands but decisively stronger than
    // what the old pose had, and the refinement that follows -- BA, filtering,
    // de-registration of anything hollow -- judges the result under the
    // ordinary rules.
    //
    // Measured on the same dataset: without this, merging models that have
    // each drifted along a long walk left ~7% of the rig frames (which share
    // a pose by construction) tens of degrees apart.
    // `max_reg` caps the size the repair may grow the model to (0 = uncapped).
    // The repair re-registers the images it moved, and images that could not
    // register before may be able to now -- but with nothing bounding it that
    // bonus is a full incremental growth pass hiding inside a repair, at the
    // repair's cadence rather than the caller's. Measured, one took a model
    // from 1752 to 3933 images.
    Reconstruction audit(const Reconstruction& m, AuditStats* out = nullptr,
                         uint32_t max_reg = 0) {
        ensureSetup();
        resetModel();
        adopt(m);
        // As in continueFrom: this is not a sub-model being built beside the
        // others, so the claim bookkeeping (and the overlap break it drives)
        // must not stop the re-registration loop below.
        model_count_.clear();
        rebuildScores();
        AuditStats st;
        std::vector<std::pair<uint32_t, Pose>> repairs;
        // One RANSAC per registered image at audit_ransac_trials trials is what
        // a manage round spends its time on, and the test is read-only --
        // poseContradicted() only looks at the finished model -- so it fans
        // out. Verdicts are collected by index and applied in image order, so
        // `repairs` is what the serial loop produced. The dump path stays
        // serial to keep its per-image lines in order.
        std::vector<uint32_t> ids;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) ids.push_back(kv.first);
        st.checked = (uint32_t)ids.size();
        auto audit_t0 = std::chrono::steady_clock::now();
        modelScale();  // warm the lazy cache before any worker reads it
        std::vector<char> hit(ids.size(), 0);
        std::vector<Pose> alts(ids.size());
        const unsigned hc = std::thread::hardware_concurrency();
        int nt = opt_.threads > 0 ? opt_.threads : (hc > 0 ? (int)hc : 1);
        if (audit_dump_) nt = 1;
        nt = std::max(1, std::min<int>(nt, (int)std::max<size_t>(ids.size(), 1)));
        std::atomic<size_t> next{0};
        auto worker = [&] {
            for (size_t i = next++; i < ids.size(); i = next++)
                hit[i] = poseContradicted(ids[i], alts[i]) ? 1 : 0;
        };
        if (nt == 1) {
            worker();
        } else {
            std::vector<std::thread> pool;
            pool.reserve(nt);
            for (int t = 0; t < nt; t++) pool.emplace_back(worker);
            for (std::thread& t : pool) t.join();
        }
        for (size_t i = 0; i < ids.size(); i++)
            if (hit[i]) repairs.emplace_back(ids[i], alts[i]);
        st.unsupported = (uint32_t)repairs.size();
        auto audit_t1 = std::chrono::steady_clock::now();
        g_map_prof.audit_check += std::chrono::duration<double>(audit_t1 - audit_t0).count();
        ProfTimer audit_pt(g_map_prof.audit_fix);
        if (!repairs.empty()) {
            if (opt_.verbose)
                fprintf(stderr, "[map] audit: %u/%u image(s) sit where the rest of the model "
                        "contradicts them; moving\n", st.unsupported, st.checked);
            // Detach first, all of them: their old observations are evidence
            // for the old pose and must not survive it.
            for (const auto& r : repairs) deregisterImage(r.first);
            for (const auto& r : repairs) {
                Image& im = rec_.images[r.first];
                im.pose = r.second;
                im.registered = true;
            }
            rebuildScores();
            // Attach to what the new pose can see, then triangulate what
            // nothing sees yet -- registration's two steps, for a pose that
            // came from outside instead of from PnP. Without the first, a
            // repaired image owns no observations at all (everything it looks
            // at is already triangulated, so there is nothing to *create*) and
            // the next filtering pass de-registers it as hollow.
            for (const auto& r : repairs) attachExisting(r.first);
            for (const auto& r : repairs) triangulateForImage(r.first);
            rebuildScores();
            // Images that could not register before may be able to now, both
            // because the model changed and because their trial budget is
            // reset; that is a bonus, not a side effect to design around.
            reg_trials_.assign(db_.images.size(), 0);
            growLoop(max_reg);
        }
        checkedRefine(true);
        for (const auto& r : repairs)
            if (rec_.images.at(r.first).registered) st.reregistered++;
        st.deregistered = st.unsupported - st.reregistered;
        if (out) *out = st;
        return snapshotModel();
    }

    // Seed and grow further models among images that `models` does not cover,
    // appending each admitted one. `restart_relaxation` re-arms the seed
    // threshold ladder, which a later pass needs: the first pass leaves it
    // exhausted, but by then the claimed set has changed and pairs that were
    // ineligible are not any more.
    void seedFurtherModels(std::vector<Reconstruction>& models, bool restart_relaxation = false) {
        ensureSetup();
        if (restart_relaxation) init_relax_ = 0;
        int trials = 0;
        // Not `unclaimedImages() > 0`: admitModel keeps a sub-model only if at
        // least min_model_size of its images are ones no kept model covers, and
        // an unclaimed image is exactly that -- so with fewer than that many
        // left, every attempt is *guaranteed* to be discarded. Each of them is a
        // seed plus an unbounded grow plus the refinements along the way, i.e. a
        // reconstruction of the neighbourhood, thrown away. A bottom-up atom
        // ends with a handful of images its primary model missed, so it paid
        // this several times over per atom.
        const size_t need = (size_t)std::max(1, opt_.min_model_size);
        while ((int)models.size() < std::max(1, opt_.max_num_models) &&
               unclaimedImages() >= need) {
            if (++trials > std::max(1, opt_.max_model_trials)) {
                if (opt_.verbose)
                    fprintf(stderr, "[map] sub-model search hit its %d-attempt budget with %zu "
                            "image(s) unaccounted for\n", opt_.max_model_trials,
                            unclaimedImages());
                break;
            }
            resetModel();
            size_t from = 0;
            bool seeded;
            {
                ProfTimer pt(g_map_prof.init_seed);
                seeded = initialize(from);
            }
            if (!seeded) break;  // nothing left that can seed a model
            globalRefine(false);
            grow();
            Reconstruction sub = snapshotModel();
            std::string why;
            if (!admitModel(sub, why)) {
                // Not worth a directory of its own, and claiming its images
                // would only starve later passes. Leave them for the next seed;
                // `used_seeds_` stops this pair being tried again.
                if (opt_.verbose)
                    fprintf(stderr, "[map] sub-model discarded: %s\n", why.c_str());
                continue;
            }
            const uint32_t reg = sub.numRegistered();
            models.push_back(std::move(sub));
            claimImages(models.back());
            recordCameras(models.back());
            if (opt_.verbose)
                fprintf(stderr, "[map] sub-model %zu: %u images, %zu points (%zu images left)\n",
                        models.size() - 1, reg, models.back().points3D.size(), unclaimedImages());
        }
    }

    // Record `models` as the claimed set, replacing whatever was there.
    void claimAll(const std::vector<Reconstruction>& models) {
        model_count_.assign(db_.images.size(), 0);
        for (const Reconstruction& m : models) claimImages(m);
    }

    // Images some seed attempt has already reached (D58). Distinct from the
    // claimed set on purpose: a claim also bounds *growth*, and a retry has to
    // keep growing without a bound -- the whole point of retrying is that a
    // better seed can reach further than the last one did, through the same
    // images. What must not repeat is starting in the same place, because the
    // reconstruction that follows is then the one already built.
    void blockSeeds(const Reconstruction& m) {
        if (seeded_.size() != db_.images.size()) seeded_.assign(db_.images.size(), 0);
        for (const auto& kv : m.images)
            if (kv.second.registered && kv.first < seeded_.size()) seeded_[kv.first] = 1;
    }
    bool seedBlocked(uint32_t img) const {
        return img < seeded_.size() && seeded_[img];
    }
    size_t seededImages() const {
        size_t n = 0;
        for (uint32_t i = 0; i < seeded_.size(); i++) n += (seeded_[i] && allowed(i));
        return n;
    }

    // ---- does a model agree with the two-view geometries it was built from? --
    //
    // Every acceptance test the merger can run by itself is computed from the
    // same evidence the alignment used: the images the two models share, and
    // the points they both triangulated. A capture that walks past the same
    // facade twice can satisfy all of it and still be glued together wrongly,
    // because the repeated structure is genuinely consistent -- locally.
    //
    // The correspondence graph knows something the models do not. Two images
    // were matched and verified long before any model existed, and their
    // two-view geometry does not care where a merge, or a registration, later
    // put them. So: compute the relative pose the model implies for a verified
    // pair and ask how much of that pair's own evidence it still explains. A
    // correct model reproduces those geometries; a wrong one cannot, and no
    // amount of internal self-consistency will save it.
    //
    // Bearings throughout, so this is meaningful for a fisheye (D45).

    // Fraction of a verified pair's matches the model's relative pose explains,
    // or -1 when the pair cannot be judged (an image missing, no camera).
    double pairAgreement(const Reconstruction& m, const TwoViewMatches& p, double max_error_px,
                         double model_scale) const {
        auto ia = m.images.find(p.image1);
        auto ib = m.images.find(p.image2);
        if (ia == m.images.end() || ib == m.images.end()) return -1;
        if (!ia->second.registered || !ib->second.registered || p.matches.empty()) return -1;
        auto ca = m.cameras.find(ia->second.camera_id);
        auto cb = m.cameras.find(ib->second.camera_id);
        if (ca == m.cameras.end() || cb == m.cameras.end()) return -1;
        Mat3 R = mul(ib->second.pose.R, transpose(ia->second.pose.R));
        Vec3 t = ib->second.pose.t - mul(R, ia->second.pose.t);
        // px -> rad, in the frame the keypoints were measured in (D47).
        const double thr =
            0.5 * (ca->second.errRad(max_error_px) + cb->second.errRad(max_error_px));
        const bool wide_baseline = model_scale > 0 && t.norm() > 1e-3 * model_scale;
        Mat3 E = mul(crossMatrix(t.normalized()), R);
        size_t ok = 0;
        for (const FeatureMatch& fm : p.matches) {
            Vec3 b1 = ca->second.bearing(kp(p.image1, fm.idx1));
            Vec3 b2 = cb->second.bearing(kp(p.image2, fm.idx2));
            double err2;
            if (wide_baseline) {
                err2 = sampsonSqBearing(E, b1, b2);
            } else {
                // No baseline to speak of: the epipolar constraint is vacuous,
                // so compare the rays directly through R.
                Vec3 pred = mul(R, b1);
                Vec3 c = pred.cross(b2);
                double ang = std::atan2(c.norm(), pred.dot(b2));
                err2 = ang * ang;
            }
            if (err2 < thr * thr) ok++;
        }
        return (double)ok / (double)p.matches.size();
    }

    struct SeamCheck {
        size_t cross_pairs = 0;    // verified pairs found across the seam
        size_t tested = 0;         // ... of which were actually evaluated
        size_t agree = 0;
        double median_frac = 0;    // median per-pair fraction of matches explained
    };

    // `crossing` false asks the same question of the pairs that do *not* cross
    // the seam: how well does this capture's own two-view geometry agree with a
    // reconstruction it is already part of? That is the reference the seam has
    // to be read against. It is not a constant -- a well-textured outdoor
    // capture answers 0.95 and a repetitive interior far less, because there
    // some verified pairs are themselves wrong (two corridors that look alike),
    // and a fixed bar then refuses correct merges for failing a test its
    // evidence could never pass (D68).
    SeamCheck checkSeam(const Reconstruction& m, const std::set<uint32_t>& src_side,
                        double max_error_px = 8.0, double min_pair_frac = 0.5,
                        size_t max_pairs = 600, bool crossing = true) const {
        SeamCheck sc;
        std::vector<const TwoViewMatches*> cross;
        for (const TwoViewMatches& p : db_.pairs) {
            auto ia = m.images.find(p.image1);
            auto ib = m.images.find(p.image2);
            if (ia == m.images.end() || ib == m.images.end()) continue;
            if (!ia->second.registered || !ib->second.registered) continue;
            if ((src_side.count(p.image1) == src_side.count(p.image2)) == crossing) continue;
            cross.push_back(&p);
        }
        sc.cross_pairs = cross.size();
        if (cross.empty()) return sc;
        // Prefer the pairs with the most evidence, then spread the sample over
        // them: a seam is only as good as its strongest links, and a few
        // hundred are plenty to tell a good merge from a bad one.
        std::stable_sort(cross.begin(), cross.end(), [](const TwoViewMatches* a,
                                                        const TwoViewMatches* b) {
            return a->matches.size() > b->matches.size();
        });
        if (cross.size() > max_pairs) cross.resize(max_pairs);

        const double scale = modelScaleOf(m);
        std::vector<double> fracs;
        for (const TwoViewMatches* p : cross) {
            double frac = pairAgreement(m, *p, max_error_px, scale);
            if (frac < 0) continue;
            fracs.push_back(frac);
            sc.tested++;
            if (frac >= min_pair_frac) sc.agree++;
        }
        if (!fracs.empty()) {
            std::sort(fracs.begin(), fracs.end());
            sc.median_frac = fracs[fracs.size() / 2];
        }
        return sc;
    }

    // ---- split a model along the geometries it violates (D45) --------------
    //
    // The same measurement, applied to every verified pair inside one model,
    // says more than pass/fail: it says *where* the model stops being true.
    // Keep only the pairs the model reproduces, and the images fall into
    // connected groups. One group means the model is coherent. Two large ones
    // joined by nothing means two pieces of the capture were welded at the
    // wrong relative pose -- by a merge, or by a chain of registrations through
    // repeated structure -- and no bundle adjustment will ever pull them apart,
    // because each piece is internally perfect.
    //
    // Splitting is the honest response: the pieces are real reconstructions,
    // and once separated they can be re-merged (this time against the seam
    // test) or grown independently. Images in groups too small to keep are
    // simply de-registered; growth will offer them a place again.
    struct SplitStats {
        size_t pairs_tested = 0, pairs_agree = 0;
        size_t groups = 0;          // connected groups of agreeing images
        size_t largest = 0;
        size_t dropped_images = 0;  // in groups too small to keep
        // Per-pair fraction explained, sorted. A model with a real problem has
        // a bimodal distribution -- most pairs near 1, a tail near 0 -- while a
        // threshold that is merely too tight shows a smooth spread.
        std::vector<double> fractions;
        double percentile(double q) const {
            if (fractions.empty()) return 0;
            size_t i = (size_t)(q * (double)(fractions.size() - 1));
            return fractions[i];
        }
    };

    std::vector<Reconstruction> splitInconsistent(const Reconstruction& m, double max_error_px,
                                                  double min_pair_frac, int min_matches,
                                                  size_t min_group, SplitStats* out = nullptr) const {
        SplitStats st;
        std::vector<uint32_t> ids;
        std::map<uint32_t, size_t> pos;
        for (const auto& kv : m.images)
            if (kv.second.registered) { pos[kv.first] = ids.size(); ids.push_back(kv.first); }
        if (ids.size() < 2) return {m};

        std::vector<size_t> parent(ids.size());
        for (size_t i = 0; i < parent.size(); i++) parent[i] = i;
        std::function<size_t(size_t)> find = [&](size_t x) {
            while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
            return x;
        };
        const double scale = modelScaleOf(m);
        for (const TwoViewMatches& p : db_.pairs) {
            if ((int)p.matches.size() < min_matches) continue;
            auto a = pos.find(p.image1);
            auto b = pos.find(p.image2);
            if (a == pos.end() || b == pos.end()) continue;
            double frac = pairAgreement(m, p, max_error_px, scale);
            if (frac < 0) continue;
            st.pairs_tested++;
            st.fractions.push_back(frac);
            if (frac < min_pair_frac) continue;
            st.pairs_agree++;
            size_t ra = find(a->second), rb = find(b->second);
            if (ra != rb) parent[ra] = rb;
        }

        std::sort(st.fractions.begin(), st.fractions.end());
        std::map<size_t, std::vector<uint32_t>> groups;
        for (size_t i = 0; i < ids.size(); i++) groups[find(i)].push_back(ids[i]);
        std::vector<std::vector<uint32_t>> gs;
        for (auto& kv : groups) gs.push_back(std::move(kv.second));
        std::sort(gs.begin(), gs.end(),
                  [](const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
                      return a.size() > b.size();
                  });
        st.groups = gs.size();
        st.largest = gs.empty() ? 0 : gs[0].size();
        if (out) *out = st;
        if (gs.size() <= 1) return {m};

        std::vector<Reconstruction> parts;
        for (const std::vector<uint32_t>& g : gs) {
            if (g.size() < min_group) { st.dropped_images += g.size(); continue; }
            std::set<uint32_t> keep(g.begin(), g.end());
            parts.push_back(subsetModel(m, keep));
        }
        if (out) *out = st;
        if (parts.empty()) return {m};
        return parts;
    }

    // "Were these two images matched to each other with real support?" -- the
    // question findDuplicateStructure needs the correspondence graph for
    // (D45). Built once per call over the verified pair list.
    MatchedFn matchedPredicate(int min_matches = 15) const {
        auto index = std::make_shared<std::set<std::pair<uint32_t, uint32_t>>>();
        for (const TwoViewMatches& p : db_.pairs) {
            if ((int)p.matches.size() < min_matches) continue;
            uint32_t a = p.image1, b = p.image2;
            if (a > b) std::swap(a, b);
            index->insert({a, b});
        }
        return [index](uint32_t a, uint32_t b) {
            if (a > b) std::swap(a, b);
            return index->count({a, b}) > 0;
        };
    }

    // Bundle-adjust every component at once with one shared set of intrinsics
    // per camera group (D45; runJointBA). Cheaper than it sounds -- it is one
    // solve instead of N -- and it is the only place where a small component's
    // intrinsics are constrained by the big component's evidence. Each model is
    // then filtered and re-refined through the ordinary path, so the same
    // observation and image gates apply as after any other BA.
    // `coarse` runs the solve to the growth-phase tolerance instead of the
    // solver's full one. A merge tree's intermediate levels are each followed
    // by more merges, more growth and another joint solve, so converging one
    // tightly is work the next level throws away; the passes that make
    // kill-or-keep decisions (audit, refine) are tight, and they run last.
    void jointRefine(std::vector<Reconstruction>& models, bool coarse = false) {
        ensureSetup();
        size_t live = 0;
        for (const Reconstruction& m : models)
            if (m.numRegistered() >= 2) live++;
        if (live < 2) return;
        BundleOptions bo;
        bo.real = baReal(coarse);
        bo.device = opt_.device;
        bo.threads = opt_.threads;
        bo.verbose = false;
        bo.loss = opt_.ba_loss;
        bo.loss_param = (float)(opt_.ba_loss_param * medianPixelScale());
        bo.refine_principal_point = opt_.refine_principal_point || final_.pp;
        bo.refine_extra_params = opt_.refine_extra_params || final_.extra;
        bo.pp_min_images = opt_.pp_min_images;
        bo.solver = opt_.ba_solver;
        if (coarse && opt_.ba_growth_rtol > 0) {
            bo.rtol = opt_.ba_growth_rtol;
            bo.patience = opt_.ba_growth_patience;
        }
        bo.shared_ctx = &baContext(coarse);
        bo.over_budget_throws = true;
        // One problem if it fits, and the device decides whether it does. A
        // capture cut into hundreds of atoms puts every atom's images in the
        // solve at once -- 5356 images arrive as 11564 image-instances at 2.2x
        // cover -- and on an 8 GB card that is over the budget before the tree
        // has merged anything. There is nothing below CG to fall back to, so
        // the answer has to be a smaller problem (D65).
        //
        // Splitting is sound because the models are coupled only through the
        // intrinsics: no 3D point is shared between two of them. So a batch is
        // a self-contained bundle adjustment, and the only thing that must not
        // vary between batches is what they conclude about the cameras -- see
        // below.
        for (int batches = 1;; ) {
            try {
                jointRefineBatched(models, bo, batches);
                break;
            } catch (const BAOverBudget& e) {
                const int want =
                    std::max(batches + 1, (int)std::ceil(e.need_mb / e.budget_mb * batches));
                if (want > 64) throw;
                if (opt_.verbose)
                    fprintf(stderr,
                            "[map] the joint solve needs %.0f MB against a %.0f MB budget: "
                            "splitting it %d ways\n", e.need_mb, e.budget_mb, want);
                batches = want;
            }
        }
        // Deliberately no per-model refine afterwards: that would re-fit each
        // component's intrinsics to its own observations and undo the sharing
        // this pass exists for. Observations the shared solution no longer
        // explains are dropped by the next audit or growth pass, which refine
        // through the ordinary gates.
        clearCameraConsensus();
        for (const Reconstruction& m : models) recordCameras(m);
    }

private:
    // `batches` solves over disjoint groups of models, dealt round-robin from
    // largest to smallest so every group is a representative sample of the
    // capture -- each one has to determine the intrinsics on its own evidence,
    // and a group of only the small models could not.
    //
    // The first group's cameras are then the answer for all of them. Letting
    // each group keep its own would reintroduce exactly what the joint solve
    // exists to prevent: components in different gauges, judged by merge tests
    // measured in pixels. The first group holds the largest models, so it has
    // the most to say; the later groups optimize their poses and points against
    // intrinsics seeded from it and differ from it by far less than the
    // tolerances downstream.
    void jointRefineBatched(std::vector<Reconstruction>& models, const BundleOptions& bo,
                            int batches) {
        if (batches <= 1) {
            runJointBA(models, bo);
            return;
        }
        std::vector<size_t> order(models.size());
        std::iota(order.begin(), order.end(), (size_t)0);
        std::stable_sort(order.begin(), order.end(), [&models](size_t a, size_t b) {
            return models[a].numRegistered() > models[b].numRegistered();
        });
        std::vector<std::vector<Reconstruction*>> group((size_t)batches);
        for (size_t k = 0; k < order.size(); k++)
            group[k % (size_t)batches].push_back(&models[order[k]]);
        std::map<uint32_t, Camera> shared;
        for (size_t b = 0; b < group.size(); b++) {
            if (group[b].size() < 2) continue;
            // Seed this group with what the first one settled on, so it starts
            // where the others are rather than where its own atoms left it.
            if (b)
                for (Reconstruction* m : group[b])
                    for (auto& kv : m->cameras) {
                        auto it = shared.find(kv.first);
                        if (it != shared.end()) kv.second = it->second;
                    }
            runJointBA(group[b], bo);
            if (!b)
                for (const Reconstruction* m : group[b])
                    for (const auto& kv : m->cameras) shared.emplace(kv.first, kv.second);
        }
        for (Reconstruction& m : models)
            for (auto& kv : m.cameras) {
                auto it = shared.find(kv.first);
                if (it != shared.end()) kv.second = it->second;
            }
    }

public:

    // ---- camera consensus across sub-models (D45) -------------------------
    //
    // Physically, one camera group is one lens: the same intrinsics whichever
    // component an image ended up in. The mapper used to forget that between
    // models -- every new sub-model started its cameras from the geometric
    // default and ran its own focal search, on a handful of images with almost
    // no parallax.
    //
    // So: a model that is admitted publishes its intrinsics, weighted by how
    // many images constrain them, and every later model starts from the best
    // set published so far instead of from the default. Weight, not recency,
    // decides -- the primary model is built first and is nearly always the
    // best-constrained, and a 12-image component never overwrites it.
    void recordCameras(const Reconstruction& m) {
        std::map<uint32_t, double> weight;
        for (const auto& kv : m.images)
            if (kv.second.registered) weight[kv.second.camera_id] += 1.0;
        for (const auto& kv : weight) {
            auto it = m.cameras.find(kv.first);
            if (it == m.cameras.end()) continue;
            auto cur = cam_consensus_.find(kv.first);
            if (cur == cam_consensus_.end() || kv.second > cur->second.second)
                cam_consensus_[kv.first] = {it->second, kv.second};
        }
    }

    // Drop what earlier models published (the manager re-publishes from the
    // current set, so a model that has since been split or repaired does not
    // keep voting).
    void clearCameraConsensus() { cam_consensus_.clear(); }

    // The intrinsics a fresh model would start from, for reporting.
    const std::map<uint32_t, std::pair<Camera, double>>& cameraConsensus() const {
        return cam_consensus_;
    }

    // Does the rest of the model support a *different* pose for this image
    // than the one it has?
    //
    // Asking whether the current pose is "supported" does not work, and the
    // measurement says why: only the features that carry no 3D point of their
    // own are evidence (an image that was moved wrongly kept its own tracks,
    // which reproject perfectly wherever it went), and that pool is mostly
    // junk -- one-hop graph approximations and matches an earlier filter
    // already rejected.
    //
    // Posing it as a competition does separate. Run the ordinary PnP RANSAC on
    // exactly that pool: if the outside structure has a pose for this image
    // that clears the registration gates and is somewhere else entirely, the
    // image is in the wrong place, and no amount of bundle adjustment will
    // walk it back. If the pool is noise, RANSAC finds nothing and the image
    // is left alone -- which is the common case and costs one failed RANSAC.
    //
    // On a hit, `alternative` is the pose that won, and the caller moves the
    // image there rather than throwing it away: this evidence is by
    // construction weaker than registration demands, but it is decisively
    // better than the pose in place, and the refinement that follows filters
    // the result honestly.
    bool poseContradicted(uint32_t img, Pose& alternative) const {
        std::vector<Vec3> X, br;
        const Image& im = rec_.images.at(img);
        for (uint32_t f = 0; f < feats_[img].count(); f++) {
            if (im.point3D_ids[f] != kInvalidPoint3D) continue;  // evidence it brought itself
            for (const Correspondence& c : graph_.at(img, f)) {
                if (c.image_id == img) continue;
                const Image& oi = rec_.images.at(c.image_id);
                if (!oi.registered) continue;
                uint64_t pid = oi.point3D_ids[c.feature_idx];
                if (pid == kInvalidPoint3D) continue;
                auto pt = rec_.points3D.find(pid);
                if (pt == rec_.points3D.end()) continue;
                X.push_back(pt->second.xyz);
                br.push_back(bearing(img, f));
                break;
            }
        }
        const int n = (int)X.size();
        if (n < opt_.audit_min_evidence) return false;  // nothing to contradict it with

        // How well the current pose explains that pool, for comparison.
        const double thr = camOf(img).errRad(opt_.max_reproj_error);
        const double thr2 = thr * thr;
        int cur = 0;
        for (int k = 0; k < n; k++) cur += pnpResidualSq(im.pose, X[k], br[k]) < thr2 ? 1 : 0;

        // The RANSAC below can return at most `n` inliers, so once the pose in
        // place explains enough of the pool that no alternative could clear the
        // dominance bar, there is nothing to find and the search is skipped.
        // Exact, not a heuristic -- the verdict is identical either way -- and
        // it is most of the pass: an image that sits where it belongs explains
        // its own correspondences, so on a settled model almost every image
        // takes this exit. Measured on a 7620-image capture, where the audit
        // ran a RANSAC over 5107 images to move 6 of them, and was the single
        // largest line in the finishing bill on every large capture.
        if (n <= (int)(opt_.audit_alternative_factor * cur) || n < opt_.audit_min_alternative) {
            if (audit_dump_)
                fprintf(stderr, "[audit] %s: pool %d, current %d -> ok (no alternative can win)\n",
                        db_.images[img].name.c_str(), n, cur);
            return false;
        }

        PnPResult r = ransacPnP(X, br, camOf(img).focal(), errPx(img), 0,
                                opt_.audit_ransac_trials);
        bool contradicted = false;
        double rot_deg = 0, shift = 0;
        // Note what is *not* required: a fraction of the pool. The pool of an
        // image that was placed wrongly is enormous precisely because none of
        // it was spliced, so a ratio gate hides the very case this exists for.
        // Absolute support, and dominance over the pose in place, are the honest tests.
        if (r.success && r.num_inliers >= opt_.audit_min_alternative &&
            r.num_inliers > (int)(opt_.audit_alternative_factor * cur)) {
            // A pose is only "different" if it is different: the alternative
            // usually *is* the current pose, recovered from the same geometry,
            // and finding it again is a confirmation rather than a problem.
            //
            // Both halves have to be tested. An image put down in the wrong
            // *place* -- a repeated facade, the same corridor one floor up --
            // keeps its orientation and only moves, which is precisely the
            // failure that survived a rotation-only test on a dataset.
            Mat3 D = mul(r.pose.R, transpose(im.pose.R));
            double tr = std::max(-1.0, std::min(1.0, (D[0] + D[4] + D[8] - 1) * 0.5));
            rot_deg = std::acos(tr) * 180.0 / M_PI;
            shift = (cameraCenter(r.pose) - cameraCenter(im.pose)).norm() / modelScale();
            contradicted =
                rot_deg > opt_.audit_min_rotation_deg || shift > opt_.audit_min_shift_frac;
            if (contradicted) alternative = r.pose;
        }
        if (audit_dump_)
            fprintf(stderr,
                    "[audit] %s: pool %d, current %d, alternative %d, rot %.1f deg, shift %.4f "
                    "-> %s\n", db_.images[img].name.c_str(), n, cur,
                    r.success ? r.num_inliers : 0, rot_deg, shift,
                    contradicted ? "CONTRADICTED" : "ok");
        return contradicted;
    }

    // A length to measure pose differences against, since a reconstruction has
    // no units: the RMS distance of the registered camera centers from their
    // centroid. Cached per adopted model -- the audit asks for it once per
    // image and the model does not move underneath it.
    double modelScale() const {
        if (scale_cache_ > 0) return scale_cache_;
        Vec3 c{0, 0, 0};
        size_t n = 0;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) { c = c + cameraCenter(kv.second.pose); n++; }
        if (!n) return scale_cache_ = 1.0;
        c = c * (1.0 / (double)n);
        double s = 0;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) {
                Vec3 d = cameraCenter(kv.second.pose) - c;
                s += d.dot(d);
            }
        return scale_cache_ = std::max(1e-12, std::sqrt(s / (double)n));
    }

    // The same scale for a model the mapper has not adopted (checkSeam works
    // on a candidate merge, which is nobody's `rec_` yet).
    static double modelScaleOf(const Reconstruction& m) {
        Vec3 c{0, 0, 0};
        size_t n = 0;
        for (const auto& kv : m.images)
            if (kv.second.registered) { c = c + cameraCenter(kv.second.pose); n++; }
        if (!n) return 1.0;
        c = c * (1.0 / (double)n);
        double s = 0;
        for (const auto& kv : m.images)
            if (kv.second.registered) {
                Vec3 d = cameraCenter(kv.second.pose) - c;
                s += d.dot(d);
            }
        return std::max(1e-12, std::sqrt(s / (double)n));
    }

    // Restrict every seed and every registration to a subset of the database
    // (empty = the whole thing). This is what lets one Mapper reconstruct a
    // *cluster* without copying the graph, the features or the match database:
    // a hierarchical run partitions the images and reconstructs each part with
    // the same object and the same memoized two-view geometry.
    void restrictTo(const std::vector<uint32_t>& images) {
        ensureSetup();
        // A new subset is a new problem: the seed ladder and the used-seed set
        // belong to the last one and would otherwise carry over, starting the
        // next cluster at whatever relaxation the previous one had to reach.
        used_seeds_.clear();
        init_relax_ = 0;
        seed_pair_ = nullptr;
        seeded_.clear();
        seed_cand_valid_ = false;  // it is filtered by `allowed`
        if (images.empty()) {
            allow_.clear();
            allow_count_ = db_.images.size();
            return;
        }
        allow_.assign(db_.images.size(), 0);
        for (uint32_t i : images)
            if (i < allow_.size()) allow_[i] = 1;
        allow_count_ = images.size();
    }
    bool allowed(uint32_t img) const { return allow_.empty() || allow_[img]; }
    // Images the mapper may touch: the restriction, or the whole database.
    size_t allowedCount() const { return allow_.empty() ? db_.images.size() : allow_count_; }

    // Bundle-adjust on a context this mapper does not own. Creating a Vulkan
    // device costs far more than reconstructing one atom, so the atom workers
    // (sfm/map/Atoms.h) create a short-lived Mapper per atom over a per-atom
    // sub-database and hand every one of them the worker's own context. Must
    // be set before the first bundle adjustment; a context is not shareable
    // across threads, so one per worker, never one for all of them.
    void useBaContext(VkContext* ctx) { ext_ba_ctx_ = ctx; }

    // The per-group starting intrinsics, after any focal bootstrap. A forty-
    // image atom cannot determine its own focal and must not search for one
    // (D48), so the atom workers are handed these and told the focal is
    // measured -- which is what the shared mapper's cam_consensus_ did for
    // every atom after the first, only now it holds for the first one too.
    const std::map<uint32_t, Camera>& startingCameras() const { return default_cams_; }

    // Per image, the camera group it belongs to. Filled by setup(), so it is
    // the resolved answer rather than the constructor argument, which may be
    // empty for "one camera for everything".
    const std::vector<uint32_t>& cameraIds() const { return cam_ids_; }

    size_t unclaimed() const { return unclaimedImages(); }
    size_t numImages() const { return db_.images.size(); }
    const MapperOptions& options() const { return opt_; }
    MapperOptions& options() { return opt_; }

private:
    // Load an existing model into `rec_`: its poses, its cameras (whose focals
    // are then facts, not guesses), and its points re-added as fresh tracks.
    // Images the model does not hold keep the cleared state resetModel() left.
    void adopt(const Reconstruction& m) {
        size_t missing = 0, name_mismatch = 0, count_mismatch = 0;
        // point2D_idx is an index into this run's feature arrays, so a model
        // whose image holds a different number of keypoints indexes different
        // features -- what --compact-unused-features does on one side only.
        std::set<uint32_t> reindexed;
        // The file holds one camera per frame size; this run's grouping says what
        // shares intrinsics, so the parameters land on the camera it gives the
        // image. Frame and pixel_scale stay -- cameras.bin carries neither (D47).
        std::set<uint32_t> adopted;
        for (const auto& kv : m.images) {
            if (!kv.second.registered) continue;
            auto im = rec_.images.find(kv.first);
            if (im == rec_.images.end()) continue;
            auto mc = m.cameras.find(kv.second.camera_id);
            auto cur = rec_.cameras.find(im->second.camera_id);
            if (mc == m.cameras.end() || cur == rec_.cameras.end()) continue;
            if (!adopted.insert(cur->first).second) continue;
            const int w = cur->second.width, h = cur->second.height;
            const double ps = cur->second.pixel_scale;
            cur->second = mc->second;
            cur->second.id = cur->first;
            cur->second.width = w;
            cur->second.height = h;
            cur->second.pixel_scale = ps;
            focal_known_.insert(cur->first);
        }
        for (const auto& kv : m.images) {
            if (!kv.second.registered) continue;
            auto it = rec_.images.find(kv.first);
            if (it == rec_.images.end()) { missing++; continue; }
            // Image ids are positions in this database. A model from a
            // *different* database would adopt cleanly and silently reconstruct
            // nonsense, so the names are checked (the model's carry an
            // extension, the database's do not).
            if (!kv.second.name.empty()) {
                const std::string& want = it->second.name;
                if (kv.second.name.compare(0, want.size(), want) != 0 ||
                    (kv.second.name.size() > want.size() && kv.second.name[want.size()] != '.'))
                    name_mismatch++;
            }
            if (kv.second.points2D.size() != it->second.points2D.size()) {
                count_mismatch++;
                reindexed.insert(kv.first);
            }
            it->second.pose = kv.second.pose;
            it->second.registered = true;
        }
        rec_.points3D.clear();
        rec_.next_point3D_id = 1;
        for (const auto& kv : m.points3D) {
            std::vector<TrackElement> tr;
            for (const TrackElement& e : kv.second.track) {
                auto it = rec_.images.find(e.image_id);
                if (it == rec_.images.end() || !it->second.registered) continue;
                if (reindexed.count(e.image_id)) continue;
                if (e.point2D_idx >= it->second.point3D_ids.size()) continue;
                if (it->second.point3D_ids[e.point2D_idx] != kInvalidPoint3D) continue;
                tr.push_back(e);
            }
            if (tr.size() >= 2) {
                uint64_t id = rec_.addPoint3D(kv.second.xyz, tr);
                rec_.points3D[id].rgb[0] = kv.second.rgb[0];
                rec_.points3D[id].rgb[1] = kv.second.rgb[1];
                rec_.points3D[id].rgb[2] = kv.second.rgb[2];
            }
        }
        if (name_mismatch)
            fprintf(stderr,
                    "[map] WARNING: %zu adopted image(s) have a different name than the database "
                    "entry with the same id -- the model was probably built from other matches, "
                    "and adopting it will produce nonsense\n", name_mismatch);
        if (count_mismatch)
            fprintf(stderr,
                    "[map] WARNING: %zu adopted image(s) hold a different keypoint count than "
                    "this run's features -- their observations index other keypoints and were "
                    "dropped, poses kept. Feature compaction on one side only does this; "
                    "--compact-unused-features has to match the run that wrote the model\n",
                    count_mismatch);
        if (missing && opt_.verbose)
            fprintf(stderr,
                    "[map] adopted model: %zu image(s) not in this database\n", missing);
    }

    // Sort, report and return. Split out only because run() has two exits.
    std::vector<Reconstruction> finishRun(std::vector<Reconstruction>& models,
                                          std::chrono::steady_clock::time_point prof_start) {
        // COLMAP orders the written models by 3D point count, descending
        // (ReconstructionManager::Write); sparse/0 is therefore the model with
        // the most structure, not the first one found.
        std::stable_sort(models.begin(), models.end(),
                         [](const Reconstruction& a, const Reconstruction& b) {
                             return a.points3D.size() > b.points3D.size();
                         });
        if (opt_.verbose) {
            // Distinct, not the sum: models overlap by design (up to
            // max_model_overlap), so summing would double-count the joins.
            std::set<uint32_t> covered;
            for (const Reconstruction& m : models)
                for (const auto& kv : m.images)
                    if (kv.second.registered) covered.insert(kv.first);
            slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_done,
                     {(long long)models.size(), (long long)covered.size(),
                      (long long)db_.images.size()});
            for (size_t i = 0; i < models.size(); i++)
                slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_model_line,
                         {(long long)i, (long long)models[i].numRegistered(),
                          (long long)models[i].points3D.size()});
            // Why the rest did not come in. The four counts call for different
            // fixes -- too few 2D-3D candidates is a matching or coverage
            // problem, a low inlier *ratio* is usually the image being
            // genuinely somewhere else -- so they are worth separating.
            if (covered.size() < db_.images.size())
                fprintf(stderr,
                        "[map] registration attempts that failed: %u too few candidates, "
                        "%u too few PnP inliers, %u inlier ratio below %.2f, %u lost on refit "
                        "(%u came in on absolute support with the ratio failed, %u more were "
                        "refused for a rival pose fitting the leftovers; %u correspondence(s) "
                        "were left out of the ratio as unseeable)\n",
                        reg_fail_.few_corr, reg_fail_.few_inliers, reg_fail_.low_ratio,
                        opt_.min_pnp_inlier_ratio, reg_fail_.refined_out, reg_fail_.strong,
                        reg_fail_.ambiguous, reg_fail_.occluded);
        }
        g_map_prof.report(std::chrono::duration<double>(
                              std::chrono::steady_clock::now() - prof_start).count());
        return models;
    }

private:
    // ---- multi-model bookkeeping (D41) ----
    // `rec_` as a standalone reconstruction: its registered images only, the
    // cameras they use, points colored. `rec_` itself keeps every image --
    // resetModel() reuses those records for the next attempt, so the pruning
    // happens on the copy.
    //
    // Pruning is what makes keeping several models affordable: `rec_.images`
    // carries a points2D / point3D_ids pair for *every* image in the dataset
    // (~200 kB each at 8192 features), so an unpruned 2000-image model costs
    // ~400 MB whether it registered 2000 images or 20.
    Reconstruction snapshotModel() const {
        Reconstruction m = rec_;
        for (auto it = m.images.begin(); it != m.images.end();)
            it = it->second.registered ? std::next(it) : m.images.erase(it);
        // Cameras that ended with no registered image (an unused resolution
        // bucket, or a group whose images were all rejected) would be written
        // to cameras.bin with their default-guess intrinsics -- drop them.
        std::set<uint32_t> used;
        for (const auto& kv : m.images) used.insert(kv.second.camera_id);
        for (auto it = m.cameras.begin(); it != m.cameras.end();)
            it = used.count(it->first) ? std::next(it) : m.cameras.erase(it);
        assignColors(m);
        return m;
    }

    // Record the images a kept model registered. They stop being seed
    // candidates; a later model may still re-register them, within
    // overlapBudget, which is the overlap a merge step aligns on.
    void claimImages(const Reconstruction& m) {
        if (model_count_.size() != db_.images.size()) model_count_.assign(db_.images.size(), 0);
        for (const auto& kv : m.images)
            if (kv.second.registered) model_count_[kv.first]++;
    }

    size_t unclaimedImages() const {
        size_t n = 0;
        for (uint32_t i = 0; i < model_count_.size(); i++) n += (model_count_[i] == 0 && allowed(i));
        return n;
    }

    bool claimed(uint32_t img) const {
        return img < model_count_.size() && model_count_[img] > 0;
    }

    // Images another model already holds that one growth pass may take, given
    // how many it has found that nothing holds. See max_model_overlap: the
    // count is the floor and the ratio is what the pass earns on top of it, so
    // a pass still discovering territory is never cut off, and one that has run
    // out of its own stops as soon as it has a Sim(3)'s worth of overlap.
    size_t overlapBudget(size_t fresh) const {
        const double floor_v = (double)std::max(1, opt_.max_model_overlap);
        return (size_t)std::max(floor_v, opt_.model_overlap_ratio * (double)fresh);
    }

    // How many of `m`'s images some already-kept model also holds.
    size_t overlapWithKept(const Reconstruction& m) const {
        if (model_count_.empty()) return 0;
        size_t n = 0;
        for (const auto& kv : m.images)
            if (kv.second.registered && model_count_[kv.first] > 0) n++;
        return n;
    }

    // Is `m` worth a directory of its own, given what is already kept? One
    // question, and it is about what `m` *adds*: enough images no kept model
    // reached. What it re-registers is not held against it, however much of it
    // there is -- that overlap is what a merge aligns on, and a model refused
    // for having it is a model whose images have to be found again from a worse
    // seed (D66).
    //
    // It used to be refused, on COLMAP's max_model_overlap, and on a 7620-image
    // capture that discarded two attempts of 2751 and 1751 images for sharing
    // 212 and 27 with the one already kept -- 4502 images of finished
    // reconstruction, of which the search that followed recovered 1188.
    bool admitModel(const Reconstruction& m, std::string& why) const {
        const uint32_t reg = m.numRegistered();
        const size_t fresh = reg - overlapWithKept(m);
        if (fresh < (size_t)opt_.min_model_size) {
            char buf[160];
            snprintf(buf, sizeof buf, "%u images but only %zu not already covered", reg, fresh);
            why = buf;
            return false;
        }
        return true;
    }

    // Same question for the model under construction. Free during the primary
    // model (nothing is claimed yet); a linear scan afterwards, which is noise
    // next to the PnP it gates.
    size_t sharedRegistered() const {
        if (model_count_.empty()) return 0;
        size_t n = 0;
        for (const auto& kv : rec_.images)
            if (kv.second.registered && model_count_[kv.first] > 0) n++;
        return n;
    }

    // The mean of one point's observations' keypoint colors (COLMAP's
    // ExtractColorsForAllImages, but the samples were taken once at extraction
    // so no image is decoded again here). `rgb` is left alone -- the neutral
    // gray a Point3D starts with -- when no observation carries a color.
    void pointColor(const Point3D& p, uint8_t rgb[3]) const {
        uint32_t acc[3] = {0, 0, 0}, n = 0;
        for (const TrackElement& e : p.track) {
            const FeatureSet& fs = feats_[e.image_id];
            if (!fs.hasColors()) continue;
            const uint8_t* c = &fs.colors[(size_t)e.point2D_idx * 3];
            acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]; n++;
        }
        if (!n) return;
        for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)((acc[k] + n / 2) / n);
    }

    void assignColors(Reconstruction& rec) const {
        for (auto& kv : rec.points3D) pointColor(kv.second, kv.second.rgb);
    }

    // Grow the current model until nothing else registers, or until it has
    // spent its overlapBudget on images a previously-kept model already holds
    // (D41, D66). During the primary model nothing is claimed yet, so the
    // overlap test is inert and this is the pre-D41 loop exactly.
    void grow() {
        growLoop();
        checkedRefine(true);
    }

    // The registration loop alone, returning how many images it brought in.
    // Split from grow() so a continuation pass can skip the final refinement
    // when it registered nothing -- refining a model that did not change is
    // both wasted time and a silent perturbation of a finished result.
    // `max_reg` caps the model's size (0 = grow until nothing registers); the
    // focal bootstrap uses it to build a model just big enough to score.
    // `stop_at_ba` returns instead of refining when the model has grown enough
    // to trigger one. The caller is then responsible for the optimization --
    // see growByPnP, where a whole level's worth of growth is paid for by a
    // single joint bundle adjustment rather than one per model.
    uint32_t growLoop(uint32_t max_reg = 0, bool stop_at_ba = false) {
        uint32_t registered_here = 0;
        // Relative to what the model already holds, not absolute. 3 is right
        // for a two-image seed and wrong for every continuation: an adopted
        // 200-image model satisfies `>= 3` on its first registration, so it
        // refined after every single image -- and with stop_at_ba it stopped
        // after one, which is why a growth pass over 33 models registered 23
        // images between them.
        double next_ba = std::max(3.0, std::ceil(rec_.numRegistered() * opt_.ba_growth_ratio));
        recent_regs_.clear();
        rebuildScores();
        // The overlap budget is spent by *this* pass. A continuation of a model
        // that already shares images with another (a merge just gave it some)
        // would otherwise be over budget before it registered anything.
        const size_t shared_at_entry = sharedRegistered();
        while (true) {
            if (max_reg && rec_.numRegistered() >= max_reg) break;
            // Both counts are of *this* pass, and both can be nudged by a
            // de-registration mid-pass, so neither subtraction may wrap.
            const size_t shared_now = sharedRegistered();
            const size_t shared_here = shared_now > shared_at_entry ? shared_now - shared_at_entry : 0;
            const size_t fresh_here = registered_here > shared_here ? registered_here - shared_here : 0;
            if (shared_here > overlapBudget(fresh_here)) {
                if (opt_.verbose)
                    fprintf(stderr, "[map] growth took %zu image(s) an earlier model holds "
                            "against %zu of its own; stopping it\n", shared_here, fresh_here);
                break;
            }
            // COLMAP's shape: rank all candidates, try them in order until one
            // registers, and only then recompute the ranking. A single failure
            // must not retire an image -- it usually just means not enough of
            // the scene is triangulated *yet* (see D15).
            std::vector<uint32_t> cands;
            {
                ProfTimer pt(g_map_prof.choose);
                g_map_prof.n_choose++;
                cands = chooseNextImages();
            }
            if (cands.empty()) break;
            bool registered = false;
            for (uint32_t img : cands) {
                reg_trials_[img]++;
                g_map_prof.n_reg_try++;
                bool ok;
                {
                    ProfTimer pt(g_map_prof.reg);
                    ok = registerImage(img);
                }
                if (ok) {
                    g_map_prof.n_reg_ok++;
                    registered = true;
                    registered_here++;
                    ProfTimer pt(g_map_prof.tri);
                    triangulateForImage(img);
                    recent_regs_.push_back(img);
                    break;
                }
            }
            if (!registered) break;  // nothing in the ranking can be registered
            if (rec_.numRegistered() >= next_ba) {
                if (stop_at_ba) break;
                checkedRefine(false);
                // Refinement mutates observations wholesale (filtering,
                // retriangulation, de-registration, possibly a snapshot
                // restore), so the incremental score cache starts over.
                rebuildScores();
                // De-registration may have shrunk the model; the next trigger
                // is always relative to what actually survived.
                next_ba = std::ceil(rec_.numRegistered() * opt_.ba_growth_ratio);
            }
        }
        return registered_here;
    }

    // Transactional global refinement (D36): one toxic registration reaching a
    // trivial-loss global BA can bend a small model so far that the filters
    // shred it, seed included. Snapshot first; if refinement collapses the
    // model, restore the snapshot and de-register the images added since the
    // last refinement -- the suspects -- instead of keeping the wreckage. This
    // is the mapper "going back": the registrations are undone, the images
    // keep their remaining trials, and growth continues from known-good state.
    // Undo state for a transactional refine. Copying the whole Reconstruction
    // is O(database), not O(model): `rec_` carries an entry for every image the
    // database has (resetModel builds them all), each with a point3D_ids vector
    // as long as that image's feature list, and refinement can only touch the
    // registered ones. Unregistered images are all-invalid by invariant --
    // deregisterImage clears them -- so they restore without being copied. On a
    // 40-image cluster of a 5400-image capture that is 135x less per refine.
    struct Snapshot {
        std::map<uint32_t, Camera> cameras;
        std::map<uint64_t, Point3D> points3D;
        uint64_t next_point3D_id = 1;
        std::vector<std::pair<uint32_t, Image>> images;  // registered only
        std::set<uint32_t> focal_known;
    };

    Snapshot takeSnapshot() const {
        Snapshot s;
        s.cameras = rec_.cameras;
        s.points3D = rec_.points3D;
        s.next_point3D_id = rec_.next_point3D_id;
        s.focal_known = focal_known_;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) s.images.emplace_back(kv.first, kv.second);
        return s;
    }

    void restoreSnapshot(Snapshot& s) {
        rec_.cameras = std::move(s.cameras);
        rec_.points3D = std::move(s.points3D);
        rec_.next_point3D_id = s.next_point3D_id;
        focal_known_ = std::move(s.focal_known);
        std::set<uint32_t> was;
        for (const auto& kv : s.images) was.insert(kv.first);
        // Refinement only ever de-registers, so this loop is normally empty;
        // it is here so the restore does not depend on that staying true.
        for (auto& kv : rec_.images) {
            if (!kv.second.registered || was.count(kv.first)) continue;
            kv.second.registered = false;
            kv.second.pose = {mat3Identity(), {0, 0, 0}};
            std::fill(kv.second.point3D_ids.begin(), kv.second.point3D_ids.end(),
                      kInvalidPoint3D);
        }
        for (auto& kv : s.images) rec_.images[kv.first] = std::move(kv.second);
    }

    void checkedRefine(bool final_pass) {
        uint32_t reg_before = rec_.numRegistered();
        Snapshot snap;
        {
            ProfTimer pt(g_map_prof.snapshot);
            snap = takeSnapshot();
        }
        globalRefine(final_pass);
        // Collapse = losing half the registered images. Observation loss alone
        // is NOT a collapse: shredding most of a junk-heavy image's points
        // while every pose survives is the filters working as intended, and
        // vegetation-grade bootstraps routinely shed 2/3 of their observations.
        bool collapsed = 2 * rec_.numRegistered() < reg_before;
        if (collapsed && !recent_regs_.empty()) {
            if (opt_.verbose)
                fprintf(stderr,
                        "[map] refinement collapsed the model (%u -> %u images); undoing %zu "
                        "recent registration(s)\n", reg_before, rec_.numRegistered(),
                        recent_regs_.size());
            restoreSnapshot(snap);
            for (uint32_t img : recent_regs_) deregisterImage(img);
            resetOrphanCameras();
        }
        recent_regs_.clear();
    }

    // ---- helpers ----
    Vec2 kp(uint32_t img, uint32_t f) const {
        const Keypoint& k = feats_[img].keypoints[f];
        return {k.x, k.y};
    }
    // Images may have different intrinsics *and* different resolutions, so
    // every projection has to go through the image's own camera.
    const Camera& camOf(uint32_t img) const {
        return rec_.cameras.at(rec_.images.at(img).camera_id);
    }
    // A pixel threshold in image `img`'s own pixels. Thresholds are given in
    // extraction pixels, where feature noise lives; this is the only place that
    // knows the difference (D47, Camera::pixel_scale).
    double errPx(uint32_t img) const { return camOf(img).errPx(opt_.max_reproj_error); }
    // The same conversion for a whole-problem scalar the GPU solver can only
    // take one of: the median over the registered cameras. Exact on the common
    // case (one extraction scale for the capture) and a compromise only when a
    // dataset mixes resolutions *and* crosses --max-image-size in one run.
    double medianPixelScale() const {
        std::vector<double> v;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) v.push_back(camOf(kv.first).pixel_scale);
        if (v.empty()) {
            for (const auto& kv : rec_.cameras) v.push_back(kv.second.pixel_scale);
            if (v.empty()) return 1.0;
        }
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v[v.size() / 2];
    }
    // Unit viewing ray for feature f of image img (the geometry core's
    // interchange type, D31; fisheye-ready, unlike z=1 normalized coords).
    Vec3 bearing(uint32_t img, uint32_t f) const { return camOf(img).bearing(kp(img, f)); }
    Mat34 Pmat(uint32_t img) const {
        const Pose& p = rec_.images.at(img).pose;
        return {p.R[0], p.R[1], p.R[2], p.t.x, p.R[3], p.R[4], p.R[5], p.t.y,
                p.R[6], p.R[7], p.R[8], p.t.z};
    }
    // A point is "in front" of a camera if it lies along the viewing ray. For
    // the pinhole family that is p.z > 0 (kept bit-identical); a wide-FOV camera
    // sees past 90 deg, where p.z < 0 is still valid, so the test becomes the
    // sign of p . bearing (D33). `b` is the observation's unit bearing (already
    // computed by the caller for the geometry, so this adds no unprojection).
    static bool inFront(const Camera& cam, const Vec3& pc, const Vec3& b, double zmin) {
        return cam.wideFov() ? pc.dot(b) > 0 : pc.z >= zmin;
    }
    double reprojErr(uint32_t img, uint32_t f, const Vec3& X) const {
        const Pose& p = rec_.images.at(img).pose;
        const Camera& cam = camOf(img);
        Vec3 pc = mul(p.R, X) + p.t;
        // Cheirality: pinhole path stays exactly `pc.z < 1e-8` (no bearing cost);
        // a wide-FOV camera sees past 90 deg, so it tests the sign along the ray
        // (D33). For a spherical camera every direction is in view, and the dot
        // test costs nothing beyond the bearing it already needs.
        if (cam.wideFov()) {
            if (pc.dot(cam.bearing(kp(img, f))) <= 0) return 1e30;
        } else if (pc.z < 1e-8) {
            return 1e30;
        }
        Vec2 px = cam.project(pc);
        Vec2 o = kp(img, f);
        return std::hypot(px.x - o.x, px.y - o.y);
    }

    // ---- flat model index -------------------------------------------------
    //
    // rec_.images and rec_.cameras are std::maps, and the geometry helpers
    // above look up two or three of them per call -- reprojErr() alone does
    // three. That is more work than the projection itself once a pass walks
    // millions of observations, and the filter/triangulate passes do exactly
    // that on every global-refinement round. Image ids are dense positions in
    // the database, so a vector answers the same question in one load.
    //
    // Only valid while nothing is inserted into or erased from rec_.images /
    // rec_.cameras, which is why it is built by the pass that uses it and never
    // stored. Mutating an existing entry (poses, point3D_ids, intrinsics) is
    // fine: std::map never moves its nodes.
    //
    // It carries pixel_scale rather than a finished threshold so that each call
    // site can keep writing the expression it wrote before: `errPx(x)` is
    // `x * pixel_scale`, and re-associating a product of doubles can move the
    // last bit and with it a borderline accept/reject.
    struct ModelIndex {
        std::vector<Image*> img;          // by image id, null if absent
        std::vector<Camera*> cam;         // that image's camera
        std::vector<double> pixel_scale;  // Camera::pixel_scale (D47)
    };

    ModelIndex indexModel() {
        ModelIndex mi;
        const size_t n = db_.images.size();
        mi.img.assign(n, nullptr);
        mi.cam.assign(n, nullptr);
        mi.pixel_scale.assign(n, 1.0);
        for (size_t i = 0; i < n; i++) {
            auto it = rec_.images.find((uint32_t)i);
            if (it == rec_.images.end()) continue;
            mi.img[i] = &it->second;
            auto ct = rec_.cameras.find(it->second.camera_id);
            if (ct == rec_.cameras.end()) continue;
            mi.cam[i] = &ct->second;
            mi.pixel_scale[i] = ct->second.pixel_scale;
        }
        return mi;
    }

    // reprojErr() against the index. Same arithmetic, same cheirality rules.
    double reprojErrAt(const ModelIndex& mi, uint32_t img, uint32_t f, const Vec3& X) const {
        const Pose& p = mi.img[img]->pose;
        const Camera& cam = *mi.cam[img];
        Vec3 pc = mul(p.R, X) + p.t;
        if (cam.wideFov()) {
            if (pc.dot(cam.bearing(kp(img, f))) <= 0) return 1e30;
        } else if (pc.z < 1e-8) {
            return 1e30;
        }
        Vec2 px = cam.project(pc);
        Vec2 o = kp(img, f);
        return std::hypot(px.x - o.x, px.y - o.y);
    }

    // triangulatePair() against the index.
    bool triangulatePairAt(const ModelIndex& mi, uint32_t a, uint32_t fa, uint32_t b, uint32_t fb,
                           Vec3& X, double err_scale = 1.0) const {
        const Pose& pa_ = mi.img[a]->pose;
        const Pose& pb_ = mi.img[b]->pose;
        const Camera& ca = *mi.cam[a];
        const Camera& cb = *mi.cam[b];
        Vec3 ba = ca.bearing(kp(a, fa)), bb = cb.bearing(kp(b, fb));
        Mat34 Pa{pa_.R[0], pa_.R[1], pa_.R[2], pa_.t.x, pa_.R[3], pa_.R[4], pa_.R[5], pa_.t.y,
                 pa_.R[6], pa_.R[7], pa_.R[8], pa_.t.z};
        Mat34 Pb{pb_.R[0], pb_.R[1], pb_.R[2], pb_.t.x, pb_.R[3], pb_.R[4], pb_.R[5], pb_.t.y,
                 pb_.R[6], pb_.R[7], pb_.R[8], pb_.t.z};
        X = triangulateDLT(Pa, Pb, ba, bb);
        Vec3 pa = mul(pa_.R, X) + pa_.t;
        Vec3 pb = mul(pb_.R, X) + pb_.t;
        if (!inFront(ca, pa, ba, 1e-6) || !inFront(cb, pb, bb, 1e-6)) return false;
        double ang = triangulationAngle(X, cameraCenter(pa_), cameraCenter(pb_));
        if (ang * 180.0 / M_PI < opt_.min_tri_angle_deg) return false;
        if (reprojErrAt(mi, a, fa, X) > err_scale * (opt_.max_reproj_error * mi.pixel_scale[a]))
            return false;
        if (reprojErrAt(mi, b, fb, X) > err_scale * (opt_.max_reproj_error * mi.pixel_scale[b]))
            return false;
        return true;
    }

    // Triangulate the correspondence (a,fa)<->(b,fb); accept on cheirality,
    // angle and reprojection. Returns true and fills X on success.
    // `err_scale` tightens the reprojection acceptance (< 1 during
    // retriangulation, so re-created points must beat the filter's kill
    // threshold with margin -- the churn hysteresis, see D36).
    bool triangulatePair(uint32_t a, uint32_t fa, uint32_t b, uint32_t fb, Vec3& X,
                         double err_scale = 1.0) const {
        Vec3 ba = bearing(a, fa), bb = bearing(b, fb);
        X = triangulateDLT(Pmat(a), Pmat(b), ba, bb);
        Vec3 pa = mul(rec_.images.at(a).pose.R, X) + rec_.images.at(a).pose.t;
        Vec3 pb = mul(rec_.images.at(b).pose.R, X) + rec_.images.at(b).pose.t;
        if (!inFront(camOf(a), pa, ba, 1e-6) || !inFront(camOf(b), pb, bb, 1e-6)) return false;
        double ang = triangulationAngle(X, cameraCenter(rec_.images.at(a).pose),
                                        cameraCenter(rec_.images.at(b).pose));
        if (ang * 180.0 / M_PI < opt_.min_tri_angle_deg) return false;
        if (reprojErr(a, fa, X) > err_scale * errPx(a)) return false;
        if (reprojErr(b, fb, X) > err_scale * errPx(b)) return false;
        return true;
    }

    // Idempotent: every public entry point calls it, and only the first does
    // anything. The graph and the per-image records are properties of the
    // database, not of a particular model.
    void ensureSetup() {
        if (!setup_done_) {
            setup();
            setup_done_ = true;
        }
    }

    void setup() {
        if (cam_ids_.size() != db_.images.size()) cam_ids_.assign(db_.images.size(), 1);
        // One Camera per distinct id, sized from the first image that uses it.
        // The pristine default is kept so a camera whose images all get
        // de-registered can start over instead of retrying from bad intrinsics.
        for (uint32_t i = 0; i < db_.images.size(); i++) {
            uint32_t cid = cam_ids_[i];
            if (rec_.cameras.count(cid)) continue;
            auto proto = opt_.initial_cameras.find(cid);
            rec_.cameras[cid] = proto != opt_.initial_cameras.end()
                                    ? proto->second
                                    : Camera::defaultFor(cid, feats_[i].width, feats_[i].height,
                                                         opt_.focal, opt_.camera_model);
            rec_.cameras[cid].id = cid;
            default_cams_[cid] = rec_.cameras[cid];
        }
        // Priors survive every reset; see MapperOptions::known_focal_cameras.
        for (uint32_t cid : opt_.known_focal_cameras)
            if (rec_.cameras.count(cid)) focal_known_.insert(cid);
        for (uint32_t i = 0; i < db_.images.size(); i++) {
            Image im;
            im.id = i;
            im.camera_id = cam_ids_[i];
            im.name = db_.images[i].name;  // feature stem; CLI resolves the real filename
            im.points2D.resize(feats_[i].count());
            im.point3D_ids.assign(feats_[i].count(), kInvalidPoint3D);
            for (uint32_t f = 0; f < feats_[i].count(); f++) im.points2D[f] = kp(i, f);
            rec_.images[i] = im;
        }
        graph_.build(db_, [&] {
            std::vector<uint32_t> nf(db_.images.size());
            for (size_t i = 0; i < db_.images.size(); i++) nf[i] = feats_[i].count();
            return nf;
        }());
        reg_trials_.assign(db_.images.size(), 0);
        // Size the score bookkeeping now: attachObservation can run before the
        // first grow() (the post-seed globalRefine retriangulates), and must
        // never index unallocated rows. Values there are throwaway -- grow()
        // rebuilds before the first ranking.
        rebuildScores();
    }

    // Wipe everything a previous seed attempt built, so the next attempt starts
    // from the same state setup() left behind.
    void resetModel() {
        scale_cache_ = 0;
        rec_.points3D.clear();
        rec_.cameras.clear();
        focal_known_ = opt_.known_focal_cameras;
        for (uint32_t i = 0; i < db_.images.size(); i++) {
            uint32_t cid = cam_ids_[i];
            if (!rec_.cameras.count(cid)) {
                // An earlier model's refined intrinsics if there are any, and
                // then the focal search stays off: those parameters were fitted
                // on hundreds of images, and a new model's first registration
                // has no business overwriting them (D45).
                auto cons = cam_consensus_.find(cid);
                if (cons != cam_consensus_.end()) {
                    rec_.cameras[cid] = cons->second.first;
                    focal_known_.insert(cid);
                } else {
                    rec_.cameras[cid] = default_cams_.at(cid);
                }
            }
            Image& im = rec_.images[i];
            im.registered = false;
            im.pose = {mat3Identity(), {0, 0, 0}};
            im.point3D_ids.assign(feats_[i].count(), kInvalidPoint3D);
        }
        reg_trials_.assign(db_.images.size(), 0);
    }

    // ---- initialization ----
    // Why every candidate seed was turned down. "initialization failed" on its
    // own tells a user nothing they can act on, and the reasons call for
    // opposite responses: `few_inliers` means the matcher or the pair selection
    // came up short, `low_angle` means the capture has no wide baseline to
    // start from, `forward` means it is a dolly/driving shot. Printed once when
    // initialize() gives up.
    struct InitTally {
        size_t candidates = 0, no_pose = 0, config = 0, few_inliers = 0, forward = 0,
               few_points = 0, low_angle = 0;
        double best_angle = 0;      // best median angle any candidate reached
        double best_forward = 2.0;  // most sideways motion any candidate had
    };

    void reportInitFailure() const {
        const InitTally& t = init_tally_;
        fprintf(stderr, "[map] initialization failed: %zu candidate pair(s) tried; "
                "rejected %zu no pose, %zu wrong config, %zu too few inliers, "
                "%zu forward motion, %zu too few points, %zu low angle\n",
                t.candidates, t.no_pose, t.config, t.few_inliers, t.forward, t.few_points,
                t.low_angle);
        if (t.candidates) {
            fprintf(stderr, "[map]   best median triangulation angle %.2f deg, "
                    "most sideways baseline %.3f (cap %.2f)\n",
                    t.best_angle, t.best_forward, opt_.init_max_forward_motion);
            if (t.best_angle < opt_.init_min_tri_angle_deg / 8)
                fprintf(stderr, "[map]   no pair has enough parallax to triangulate on: "
                        "the capture may be a pure rotation, or too small a sweep\n");
        }
    }

    // ---- focal bootstrap for rotation-degenerate captures (D48) ----
    // The largest angle between any two registered cameras' orientations. This
    // is the quantity that decides whether a self-calibrating bundle adjustment
    // can see the focal length at all: for a set of cameras that all point the
    // same way, stretching the scene along the viewing axis and scaling the
    // focal to match reproduces every image exactly, so the focal is a free
    // parameter of the reconstruction (the classical critical motion sequence
    // -- pure translation -- and it does not matter whether the translation is
    // forwards or sideways). Only the distortion terms, whose radial
    // polynomial is not scale-covariant, break the tie, and they break it
    // weakly.
    //   Measured: a straight KITTI drive spans 8.5 deg, while all 18 benchmark
    // scenes span 180 -- there is no threshold-tuning problem here.
    double rotationSpreadDeg() const {
        std::vector<const Mat3*> Rs;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) Rs.push_back(&kv.second.pose.R);
        // Sampled: this is O(n^2) in a pass that runs per focal hypothesis, and
        // the *maximum* of a 60-camera sample is within a degree of the true
        // maximum on every capture we have -- it is a yes/no question.
        const size_t kMax = 60;
        const size_t stride = std::max<size_t>(1, Rs.size() / kMax);
        double worst = 0;
        for (size_t i = 0; i < Rs.size(); i += stride)
            for (size_t j = i + stride; j < Rs.size(); j += stride) {
                const Mat3& A = *Rs[i];
                const Mat3& B = *Rs[j];
                // trace(A^T B), i.e. sum of the columnwise dot products.
                double tr = 0;
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 3; c++) tr += A[r * 3 + c] * B[r * 3 + c];
                double cs = std::min(1.0, std::max(-1.0, 0.5 * (tr - 1.0)));
                worst = std::max(worst, std::acos(cs) * 180.0 / M_PI);
            }
        return worst;
    }

    // The distortion the camera setup started this group at: zero, or whatever
    // --distortion supplied. What a focal trial resets to -- the trials must
    // discard each other's fitted coefficients, not the user's calibration.
    void resetExtraParams(Camera& c) const {
        Camera src;
        auto it = opt_.initial_cameras.find(c.id);
        if (it != opt_.initial_cameras.end()) src = it->second;
        c.k1 = src.k1; c.k2 = src.k2; c.p1 = src.p1; c.p2 = src.p2;
        c.k3 = src.k3; c.k4 = src.k4; c.k5 = src.k5; c.k6 = src.k6;
        c.sx1 = src.sx1; c.sy1 = src.sy1;
    }

    struct FocalTrial {
        bool ok = false;
        double focal = 0;
        uint32_t registered = 0;
        size_t observations = 0;
        double rot_spread = 0;
    };

    // Build a small model from `pm` with every guessed camera group set to
    // `focal`, and report how much mutually consistent structure it supports.
    // The score is the observation count, not the reprojection residual: a
    // wrong focal in a degenerate capture fits its own reconstruction just as
    // tightly (0.29 px either way on KITTI) but cannot make as many
    // correspondences agree with one another, so tracks break and the filters
    // take them out. Measured on 25 KITTI frames, 73.5k observations at the
    // right focal against 52k at 1.9x it, with the residual flat throughout.
    FocalTrial focalTrial(const TwoViewMatches& pm, const std::vector<uint32_t>& cams,
                          double focal, uint32_t cap) {
        for (uint32_t cid : cams) {
            default_cams_[cid].setFocal(focal);
            resetExtraParams(default_cams_[cid]);
        }
        resetModel();
        FocalTrial t;
        t.focal = focal;
        if (!trySeedPair(pm, 0.0, 0, true, false)) return t;
        globalRefine(false);
        growLoop(cap);
        globalRefine(false);
        t.ok = rec_.numRegistered() >= 3;
        t.registered = rec_.numRegistered();
        t.observations = countObservations();
        t.rot_spread = rotationSpreadDeg();
        // The trial refined the focal itself; report where it landed, since
        // that is the value worth carrying forward.
        if (!cams.empty() && rec_.cameras.count(cams[0])) t.focal = rec_.cameras[cams[0]].focal();
        return t;
    }

    // Cameras whose focal is still the geometric guess -- nothing measured it,
    // so nothing is lost by searching it. A camera an earlier model already
    // published intrinsics for (cam_consensus_) counts as measured: those were
    // fitted over that model's images, and resetModel starts every later model
    // from them, so a trial could not depart from them anyway (D45).
    std::vector<uint32_t> guessedFocalCameras() const {
        std::vector<uint32_t> out;
        for (const auto& kv : rec_.cameras)
            if (!opt_.given_focal_cameras.count(kv.first) && !cam_consensus_.count(kv.first) &&
                !kv.second.isSpherical())  // no focal to search for (D49)
                out.push_back(kv.first);
        return out;
    }

    // Pick the starting focal by trial reconstruction whenever nothing measured
    // it. Runs before the primary model and leaves nothing behind but a focal.
    //
    // The search is a descent rather than a grid, because that is what makes it
    // cheap enough to always run: halve the focal, and keep halving only while
    // each step makes materially more of the correspondences agree. A capture
    // whose guess is already in the right basin pays one extra trial model and
    // stops. Everything is grown from the *same* seed pair, so the trials'
    // scores differ only by the focal.
    void bootstrapFocalLength() {
        if (opt_.focal_trials <= 0) return;
        std::vector<uint32_t> cams = guessedFocalCameras();
        if (cams.empty()) return;
        // One camera group only. With several, a single scalar hypothesis would
        // be applied to lenses that need different answers, and the score
        // cannot say which one was wrong -- exactly the reason bootstrapFocal
        // works per group (D46). Two guessed groups means a rig or a mixed
        // collection, and there the per-group EXIF prior or the calibration is
        // the answer.
        if (cams.size() > 1) return;
        const double f0 = rec_.cameras.at(cams[0]).focal();

        // Probe: the ordinary seed, grown just far enough to have an opinion.
        const uint32_t cap = (uint32_t)std::max<size_t>(
            8, std::min<size_t>(opt_.focal_model_size, db_.images.size()));
        size_t from = 0;
        resetModel();
        if (!initialize(from) || !seed_pair_) { restoreAfterBootstrap(f0, cams, f0); return; }
        const TwoViewMatches& pm = *seed_pair_;
        globalRefine(false);
        growLoop(cap);
        globalRefine(false);
        FocalTrial base;
        base.ok = true;
        base.focal = rec_.cameras.at(cams[0]).focal();
        base.registered = rec_.numRegistered();
        base.observations = countObservations();
        base.rot_spread = rotationSpreadDeg();
        if (opt_.verbose)
            fprintf(stderr, "[map] focal probe at %.0f: %u image(s), %zu observation(s), "
                    "orientations span %.1f deg%s\n", f0, base.registered, base.observations,
                    base.rot_spread,
                    base.rot_spread <= opt_.focal_max_rot_spread_deg
                        ? " (too little to determine the focal)" : "");

        FocalTrial best = base;
        bool moved = false;
        int used = 0;
        // A measured focal has already been placed in the right basin; the
        // probe's own refinement above is the improvement, and the ladder below
        // would only be trading it for whichever trial happened to register
        // more images.
        if (opt_.measured_focal_cameras.count(cams[0])) {
            if (opt_.verbose)
                fprintf(stderr, "[map] focal %.0f -> %.0f (probe refinement of a measured "
                        "focal)\n", f0, best.focal);
            restoreAfterBootstrap(best.focal, cams, f0);
            return;
        }
        // Descend. The direction is not symmetric: a too-short focal splays the
        // bearings, which leaves a model BA can walk back up (every KITTI start
        // from 0.36x to 1.0x of the truth converged on it), while a too-long one
        // flattens the model into a self-consistent pancake it cannot climb out
        // of. So the search only ever needs to look down, and overshooting down
        // costs little.
        double f = f0;
        while (used < opt_.focal_trials) {
            f *= 0.5;
            used++;
            FocalTrial t = focalTrial(pm, cams, f, cap);
            const bool better = t.ok && (double)t.observations >
                                        (1.0 + opt_.focal_min_gain) * (double)best.observations;
            if (opt_.verbose)
                fprintf(stderr, "[map]   focal %.0f -> %.0f: %u image(s), %zu observation(s)%s\n",
                        f, t.focal, t.registered, t.observations,
                        !t.ok ? " (failed)" : better ? " (better)" : " (no better, stopping)");
            if (!better) break;
            best = t;
            moved = true;
        }
        // Nothing below the guess helped. Look once above it, in case the guess
        // is the short one -- a telephoto capture, where the guess is 2-4x low.
        if (!moved && used < opt_.focal_trials) {
            FocalTrial t = focalTrial(pm, cams, f0 * 1.6, cap);
            const bool better = t.ok && (double)t.observations >
                                        (1.0 + opt_.focal_min_gain) * (double)base.observations;
            if (opt_.verbose)
                fprintf(stderr, "[map]   focal %.0f -> %.0f: %u image(s), %zu observation(s)%s\n",
                        f0 * 1.6, t.focal, t.registered, t.observations,
                        !t.ok ? " (failed)" : better ? " (better)" : " (no better)");
            if (better) { best = t; moved = true; }
        }
        // Either way the probe's *refined* focal is what carries forward, not the
        // raw guess: the probe is a real reconstruction of `cap` images and its
        // bundle adjustment has already had a say. That matters where the guess
        // is far out but the descent finds no decisive gain.
        if (opt_.verbose)
            fprintf(stderr, "[map] focal %.0f -> %.0f (%s; %zu observations vs %zu at the "
                    "guess)\n", f0, best.focal,
                    moved ? "trial reconstruction" : "probe refinement only",
                    best.observations, base.observations);
        restoreAfterBootstrap(best.focal, cams, f0);
    }

    // Put back everything the trials disturbed, keeping only the focal. The
    // primary model must be built exactly as it would have been with that focal
    // supplied on the command line -- no trial's seed choice, relaxation level
    // or cached geometry may leak into it.
    //
    // The one thing worth keeping is the memoized two-view geometry, and only
    // when the focal did *not* move: those RANSACs are the cost of the candidate
    // scan, and on a dataset that seeds at no level at all (where the scan is
    // exhaustive and the probe learned nothing) throwing them away would make
    // the pipeline pay for the whole ladder twice to reach the same verdict.
    // `probe_focal` is the focal the memoized geometry was computed at (the
    // trials never write the cache), so the cache survives exactly when the
    // committed focal is that same value.
    void restoreAfterBootstrap(double focal, const std::vector<uint32_t>& cams,
                               double probe_focal) {
        for (uint32_t cid : cams) {
            Camera fresh = default_cams_.at(cid);
            fresh.setFocal(focal);
            resetExtraParams(fresh);
            default_cams_[cid] = fresh;
        }
        if (std::fabs(focal - probe_focal) > 1e-9) seed_geom_.clear();
        used_seeds_.clear();
        seed_pair_ = nullptr;
        init_relax_ = 0;
        resetModel();
    }

    // `from` is an in/out cursor into the ranked candidate list, so a retry
    // resumes after the seed that just produced too small a model. COLMAP's
    // thresholds (16 deg median angle, 100 inliers) are demanding on purpose:
    // everything is built on the seed. When no pair on the dataset can meet
    // them, halving stepwise is strictly better than not reconstructing (D36).
    //
    // The ladder has two rungs (D48). The first four levels relax the angle and
    // inlier demands with the forward-motion cap in force, which is every
    // dataset that has any sideways pair at all -- they behave exactly as
    // before. The next four repeat the sequence with the cap lifted, for
    // captures where *no* pair is sideways: a dashcam, a dolly shot, a drone
    // flying down a corridor. For those, the cap is not a quality filter but a
    // veto on the whole dataset, and lifting it last means a sideways seed is
    // always preferred when one exists, however weak.
    //   Lifting it is safe only because the criterion the cap approximates --
    // is this pair well enough conditioned to build a model on? -- is measured
    // directly by the seed's median triangulation angle, which is still
    // enforced at every level. A forward pair with 16 deg of median parallax is
    // a better seed than a sideways pair with 2.
    struct InitLevel { double angle; int inliers; bool allow_forward; };

    InitLevel initLevel(int level) const {
        InitLevel l{opt_.init_min_tri_angle_deg, opt_.init_min_inliers, level >= 4};
        for (int r = level % 4; r > 0; r--) {
            l.angle = std::max(l.angle * 0.5, 2.0);
            l.inliers = std::max(l.inliers / 2, 2 * TwoViewOptions().min_num_inliers);
        }
        return l;
    }

    bool initialize(size_t& from) {
        init_tally_ = InitTally();
        // The reached relaxation level is sticky across seed retries (D19's
        // best-of-attempts loop): a fresh attempt resumes at the level that
        // last produced a seed, keeping the `from` cursor's indexing valid.
        const int levels = opt_.init_max_forward_motion >= 1.0 ? 4 : 8;
        for (; init_relax_ < levels; init_relax_++, from = 0) {
            const InitLevel l = initLevel(init_relax_);
            if (init_relax_ && from == 0 && opt_.verbose)
                slog::out(slog::Tag::Map,
                         l.allow_forward ? spirula::i18n::msg::sfm::map_seed_relax_forward
                                         : spirula::i18n::msg::sfm::map_seed_relax,
                         {(long long)l.inliers, slog::num(l.angle, 0)});
            if (initializeAttempt(from, l.angle, l.inliers, l.allow_forward)) return true;
        }
        return false;
    }

    bool initializeAttempt(size_t& from, double min_ang_deg, int min_inliers,
                           bool allow_forward) {
        // Candidate pairs: verified, non-planar, most inliers first. Built once
        // per restriction and reused: the relaxation ladder only ever *lowers*
        // min_inliers, so every level's candidate set is a prefix of this one
        // ordering, and rebuilding it per level per attempt meant scanning and
        // sorting the whole pair list (74k of them on a 5400-image capture)
        // once per cluster -- a quarter of a bottom-up run's mapper time.
        if (!seed_cand_valid_) {
            seed_cand_.clear();
            for (const TwoViewMatches& p : db_.pairs)
                if (p.config == (int)TwoViewConfig::Uncalibrated && allowed(p.image1) &&
                    allowed(p.image2))
                    seed_cand_.push_back(&p);
            std::sort(seed_cand_.begin(), seed_cand_.end(), [](auto* a, auto* b) {
                if (a->matches.size() != b->matches.size())
                    return a->matches.size() > b->matches.size();
                return a->image1 != b->image1 ? a->image1 < b->image1 : a->image2 < b->image2;
            });
            seed_cand_valid_ = true;
        }
        const std::vector<const TwoViewMatches*>& cand = seed_cand_;

        // The scan is serial by construction -- the first candidate that clears
        // the level's thresholds wins, and each trial mutates rec_ -- but the
        // expensive part of a trial is `seedGeometry`, a pure function of the
        // pair (D38's memoization already relies on that). So the candidates
        // ahead of the cursor are precomputed a block at a time on all cores
        // and the serial loop below then only reads the cache. Same answer,
        // same order; a rejected candidate's RANSAC just no longer costs wall
        // clock. On a capture where the seed search rejects hundreds of pairs
        // this is the difference between minutes and seconds of dead GPU.
        // The block doubles rather than starting at full width. Speculating a
        // core's worth of RANSACs ahead is free only when the search is going
        // to reject that many candidates; on a small restricted graph -- a
        // bottom-up atom -- the first candidate usually seeds, and every other
        // pair in the block was computed for nothing, once per atom.
        //
        // With one thread it is not speculation at all, only extra work: the
        // block would be computed on this very thread, in order, and the loop
        // below computes exactly what it needs on demand (trySeedPair fills the
        // same cache). That is the atom case -- the parallelism there is over
        // atoms, so each mapper runs single-threaded -- and leaving the
        // prefetch on cost 2578 two-view RANSACs where 34 atoms needed 84.
        const unsigned hc = std::thread::hardware_concurrency();
        const size_t max_block =
            std::max<size_t>(1, opt_.threads > 0 ? (size_t)opt_.threads : (hc ? hc : 1));
        size_t block = 1;
        size_t prefetched = max_block > 1 ? from : cand.size();

        for (size_t ci = from; ci < cand.size(); ci++) {
            const TwoViewMatches* p = cand[ci];
            // Sorted by inlier count, so the first pair under this level's
            // threshold ends the level.
            if ((int)p->matches.size() < min_inliers) break;
            uint32_t a = p->image1, b = p->image2;
            // Relaxation levels re-scan the (re-sorted) candidate list, so the
            // cursor alone cannot prevent rebuilding a seed a previous attempt
            // already grew and rejected -- burning retry budget on duplicates.
            if (used_seeds_.count({a, b})) continue;
            // A further model must start somewhere no kept model reached, or it
            // would just rebuild the model that is already written (COLMAP
            // FindFirstInitialImage's `num_registrations == 0` rule, D41).
            // Inert while the primary model is being built -- but the same
            // argument applies between that model's own seed attempts, where
            // nothing is claimed yet: a retry that starts inside what the last
            // attempt registered rebuilds it (D58).
            if (claimed(a) || claimed(b) || seedBlocked(a) || seedBlocked(b)) continue;
            if (ci >= prefetched) {
                prefetched = std::min(cand.size(), ci + block);
                prefetchSeedGeometry(cand, ci, prefetched);
                block = std::min(max_block, block * 2);
            }
            if (!trySeedPair(*p, min_ang_deg, min_inliers, allow_forward, true)) continue;
            used_seeds_.insert({a, b});
            from = ci + 1;
            return true;
        }
        return false;
    }

    // Two-view geometry for a seed candidate. Pure: it reads the pristine
    // cameras and the features, and touches no reconstruction state -- which is
    // what lets it be memoized (D38) and precomputed off-thread.
    TwoViewGeometry seedGeometry(const TwoViewMatches& pm) const {
        ProfTimer pt(g_map_prof.seed_geom);
        g_map_prof.n_seed_geom++;
        const uint32_t a = pm.image1, b = pm.image2;
        TwoViewOptions tvo;
        tvo.recover_pose = true;
        // Whether to re-derive planar-or-panoramic here. `pm.matches` are
        // verification's surviving inliers and `pm.config` is its verdict on
        // the pair; only Uncalibrated ones reach here (initializeAttempt filters
        // on it). Re-deriving it costs a homography RANSAC, which is ~90% of the
        // seed search on a 550-image capture -- H's inlier ratio on a
        // non-planar pair is low, so its trial count adapts into the hundreds
        // where F, fed its own inliers, converges in a handful.
        tvo.estimate_homography = opt_.seed_homography;
        const Camera& ca = camOf(a);
        const Camera& cb = camOf(b);
        if (ca.wideFov() || cb.wideFov()) {
            // Same reason verification works on bearings (D45): a pinhole seed
            // throws away every wide correspondence, and the seed is what the
            // whole model is built on. The thresholds move from pixels to
            // radians with the focal.
            std::vector<Vec3> b1(pm.matches.size()), b2(pm.matches.size());
            for (size_t k = 0; k < pm.matches.size(); k++) {
                b1[k] = ca.bearing(kp(a, pm.matches[k].idx1));
                b2[k] = cb.bearing(kp(b, pm.matches[k].idx2));
            }
            tvo.ransac.max_error =
                0.5 * (ca.errRad(tvo.ransac.max_error) + cb.errRad(tvo.ransac.max_error));
            return estimateTwoViewBearing(b1, b2, tvo);
        }
        std::vector<Vec2> q1(pm.matches.size()), q2(pm.matches.size());
        for (size_t k = 0; k < pm.matches.size(); k++) {
            q1[k] = kp(a, pm.matches[k].idx1);
            q2[k] = kp(b, pm.matches[k].idx2);
        }
        tvo.K1 = ca.K();
        tvo.K2 = cb.K();
        return estimateTwoView(q1, q2, tvo);
    }

    // Fill seed_geom_ for cand[lo, hi) on a thread pool. Candidates already
    // cached are skipped; the map itself is only written on this thread.
    void prefetchSeedGeometry(const std::vector<const TwoViewMatches*>& cand, size_t lo,
                              size_t hi) {
        std::vector<const TwoViewMatches*> todo;
        for (size_t i = lo; i < hi; i++) {
            const TwoViewMatches* p = cand[i];
            std::pair<uint32_t, uint32_t> key{p->image1, p->image2};
            if (used_seeds_.count(key) || claimed(key.first) || claimed(key.second)) continue;
            if (seed_geom_.count(key)) continue;
            todo.push_back(p);
        }
        if (todo.size() < 2) return;  // one pair is not worth a pool
        std::vector<TwoViewGeometry> out(todo.size());
        std::atomic<size_t> next{0};
        const unsigned hc = std::thread::hardware_concurrency();
        const size_t want = opt_.threads > 0 ? (size_t)opt_.threads : std::max(1u, hc);
        const size_t nt = std::min<size_t>(todo.size(), want);
        std::vector<std::thread> pool;
        pool.reserve(nt);
        for (size_t t = 0; t < nt; t++)
            pool.emplace_back([&] {
                for (size_t i = next++; i < todo.size(); i = next++)
                    out[i] = seedGeometry(*todo[i]);
            });
        for (std::thread& t : pool) t.join();
        for (size_t i = 0; i < todo.size(); i++)
            seed_geom_[{todo[i]->image1, todo[i]->image2}] = std::move(out[i]);
    }

    // Build the two-camera seed on one candidate pair and keep it if it clears
    // the level's thresholds; otherwise roll the model back to empty. Split out
    // of the candidate scan so the focal bootstrap can rebuild *the same* pair
    // under a different focal -- comparing hypotheses on one pair is what makes
    // its scores comparable.
    bool trySeedPair(const TwoViewMatches& pm, double min_ang_deg, int min_inliers,
                     bool allow_forward, bool memoize) {
        const TwoViewMatches* p = &pm;
        const uint32_t a = p->image1, b = p->image2;
        // Memoized (D38): relaxation levels and later seed attempts rescan
        // the candidate list, and estimateTwoView (full two-view RANSAC) is by
        // far its cost. The result is identical on every rescan -- cameras are
        // pristine defaults whenever initialize() runs (resetModel precedes
        // it), and the estimator is deterministic. The focal bootstrap passes
        // memoize=false because it is *changing* the cameras between calls,
        // which is exactly the assumption the cache rests on.
        TwoViewGeometry g;
        auto cached = memoize ? seed_geom_.find({a, b}) : seed_geom_.end();
        if (cached != seed_geom_.end()) {
            g = cached->second;
        } else {
            g = seedGeometry(pm);
            if (memoize) seed_geom_[{a, b}] = g;
        }
        init_tally_.candidates++;
        if (!g.has_pose) { init_tally_.no_pose++; return false; }
        if (g.config != TwoViewConfig::Uncalibrated) { init_tally_.config++; return false; }
        if (g.num_inliers < min_inliers) { init_tally_.few_inliers++; return false; }

        // Near-pure forward motion: the baseline is parallel to the
        // viewing directions, points triangulate on needle-thin cones
        // around the epipole (COLMAP init_max_forward_motion).
        //
        // The veto is about what the *image* covers, not about the motion:
        // a narrow lens pointed along its own baseline sees only the pencil of
        // rays around the epipole. A spherical camera sees the whole sphere, so
        // the points abeam have full parallax and driving straight forward is
        // as well conditioned as any other motion -- the cap would be measuring
        // a degeneracy that is not there (D49).
        double fwd;
        {
            Vec3 c2 = cameraCenter(g.pose).normalized();  // cam1 is at origin
            fwd = std::max(std::fabs(c2.z),
                           std::fabs(c2.x * g.pose.R[6] + c2.y * g.pose.R[7] +
                                     c2.z * g.pose.R[8]));
            init_tally_.best_forward = std::min(init_tally_.best_forward, fwd);
            const bool spherical = camOf(a).isSpherical() && camOf(b).isSpherical();
            if (!allow_forward && !spherical && fwd > opt_.init_max_forward_motion) {
                init_tally_.forward++;
                return false;
            }
        }

        // Set up the two cameras and triangulate the inliers.
        rec_.images[a].pose = {mat3Identity(), {0, 0, 0}};
        rec_.images[a].registered = true;
        rec_.images[b].pose = g.pose;
        rec_.images[b].registered = true;

        std::vector<double> angles;
        int created = 0;
        for (size_t k = 0; k < p->matches.size(); k++) {
            if (!g.inlier_mask[k]) continue;
            uint32_t fa = p->matches[k].idx1, fb = p->matches[k].idx2;
            Vec3 X;
            if (!triangulatePair(a, fa, b, fb, X)) continue;
            double ang = triangulationAngle(X, cameraCenter(rec_.images[a].pose),
                                            cameraCenter(rec_.images[b].pose));
            angles.push_back(ang * 180.0 / M_PI);
            rec_.addPoint3D(X, {{a, fa}, {b, fb}});
            created++;
        }
        double medAng = 0;
        if (!angles.empty()) {
            std::sort(angles.begin(), angles.end());
            medAng = angles[angles.size() / 2];
        }
        init_tally_.best_angle = std::max(init_tally_.best_angle, medAng);
        if (created < std::max(30, min_inliers / 2)) init_tally_.few_points++;
        else if (medAng < min_ang_deg) init_tally_.low_angle++;
        if (created >= std::max(30, min_inliers / 2) && medAng >= min_ang_deg) {
            // The seed cameras are about to be refined by the first global
            // BA; do not let a later focal search overwrite that.
            focal_known_.insert(rec_.images[a].camera_id);
            focal_known_.insert(rec_.images[b].camera_id);
            seed_pair_ = &pm;
            seed_forward_ = fwd;
            if (opt_.verbose)
                slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_init_pair,
                         {(long long)a, (long long)b, (long long)created, slog::num(medAng, 1),
                          (fwd > 0.5 ? spirula::i18n::msg::sfm::baseline_forward
                                     : spirula::i18n::msg::sfm::baseline_sideways).get()});
            return true;
        }
        // Roll back and let the caller try the next candidate.
        rollbackInit(a, b);
        return false;
    }

    void rollbackInit(uint32_t a, uint32_t b) {
        rec_.points3D.clear();
        rec_.next_point3D_id = 1;
        for (auto& kv : rec_.images) {
            kv.second.registered = false;
            std::fill(kv.second.point3D_ids.begin(), kv.second.point3D_ids.end(), kInvalidPoint3D);
        }
    }

    // ---- registration ----
    // Count 2D-3D correspondences an unregistered image has to the model.
    // Reference implementation: score_cache_ maintains exactly this value
    // incrementally (SS_SFM_SCORE_CHECK=1 cross-checks them on every ranking).
    int score(uint32_t img) const {
        if (rec_.images.at(img).registered) return -1;
        int n = 0;
        for (uint32_t f = 0; f < feats_[img].count(); f++)
            for (const Correspondence& c : graph_.at(img, f))
                if (rec_.images.at(c.image_id).registered &&
                    rec_.images.at(c.image_id).point3D_ids[c.feature_idx] != kInvalidPoint3D) {
                    n++;
                    break;  // count the feature once
                }
        return n;
    }

    // Incremental next-image scoring (D37). Rescoring every unregistered image
    // after every registration was 1/3 of kitchen279's map time, and almost
    // all of it recomputed unchanged values. Bookkeeping instead:
    // support_[i][f] = number of (registered image, triangulated feature)
    // correspondences of (i, f); score_cache_[i] = #features with support > 0
    // == score(i). attachObservation() updates it when a feature of a
    // registered image joins a 3D point (the only mutation on the pure growth
    // path); everything wholesale (refine, undo, reseed) triggers
    // rebuildScores() instead of being tracked piecemeal.
    void rebuildScores() {
        ProfTimer pt(g_map_prof.choose);
        if (support_.size() != db_.images.size()) {
            support_.resize(db_.images.size());
            for (size_t i = 0; i < db_.images.size(); i++)
                support_[i].assign(feats_[i].count(), 0);
            score_cache_.assign(db_.images.size(), 0);
            pyramid_.assign(db_.images.size(), std::vector<uint16_t>(kPyrCells, 0));
            pyramid_score_.assign(db_.images.size(), 0);
        } else {
            for (auto& s : support_) std::fill(s.begin(), s.end(), 0);
            std::fill(score_cache_.begin(), score_cache_.end(), 0);
            for (auto& p : pyramid_) std::fill(p.begin(), p.end(), 0);
            std::fill(pyramid_score_.begin(), pyramid_score_.end(), 0);
        }
        for (const auto& kv : rec_.images) {
            const Image& im = kv.second;
            if (!im.registered) continue;
            for (uint32_t f = 0; f < (uint32_t)im.point3D_ids.size(); f++)
                if (im.point3D_ids[f] != kInvalidPoint3D) attachObservation(kv.first, f);
        }
    }

    // Feature f of registered image img just joined a 3D point: every
    // correspondence of (img, f) gains one unit of support.
    void attachObservation(uint32_t img, uint32_t f) {
        for (const Correspondence& c : graph_.at(img, f))
            if (++support_[c.image_id][c.feature_idx] == 1) {
                score_cache_[c.image_id]++;
                pyramidSet(c.image_id, c.feature_idx);
            }
    }

    // ---- visibility pyramid (D52) -----------------------------------------
    //
    // A candidate image's rank is not how *many* of its features see
    // triangulated structure but how well those features are spread over the
    // frame: a hundred correspondences in one corner condition a pose far worse
    // than fifty across the whole image, and a pose that starts badly
    // conditioned is what later turns into a misplacement. COLMAP ranks by the
    // same quantity (its default MIN_UNCERTAINTY /
    // ObservationManager::Point3DVisibilityScore) over the same pyramid.
    //
    // Levels are 2x2 .. 32x32; a cell becoming occupied adds that level's cell
    // count to the score, so a coarse cell is worth as much as the whole finer
    // level under it and spread beats density at every scale. The counts are
    // maintained incrementally beside support_, from the same 0 -> 1 event.
    static constexpr int kPyrLevels = 5;
    static constexpr int kPyrDim = 1 << kPyrLevels;             // finest grid
    static constexpr int kPyrCells = (4 * ((1 << (2 * kPyrLevels)) - 1)) / 3;  // 4+16+..+1024

    void pyramidSet(uint32_t img, uint32_t f) {
        const FeatureSet& fs = feats_[img];
        if (fs.width <= 0 || fs.height <= 0) return;
        const Keypoint& k = fs.keypoints[f];
        int cx = (int)(kPyrDim * (double)k.x / fs.width);
        int cy = (int)(kPyrDim * (double)k.y / fs.height);
        cx = std::min(std::max(cx, 0), kPyrDim - 1);
        cy = std::min(std::max(cy, 0), kPyrDim - 1);
        uint16_t* level = pyramid_[img].data() + kPyrCells;  // walk levels finest first
        uint32_t score = 0;
        for (int i = kPyrLevels - 1; i >= 0; i--) {
            const int dim = 2 << i;
            level -= (size_t)dim * dim;
            if (++level[(size_t)cy * dim + cx] == 1) score += (uint32_t)dim * dim;
            cx >>= 1;
            cy >>= 1;
        }
        pyramid_score_[img] += score;
    }

    // Candidates for the next registration, best first: unregistered, still
    // within their trial budget, and seeing at least min_num_pnp_inliers
    // triangulated points. The gate is the correspondence count; the *order* is
    // the visibility-pyramid score, which prefers structure spread across the
    // frame over structure piled in one corner. Images that already failed once
    // sort behind every untried one, as in COLMAP: a retry is worth having but
    // not worth delaying a fresh candidate for.
    std::vector<uint32_t> chooseNextImages() const {
        static const bool score_check = spirula::env("SFM_SCORE_CHECK") != nullptr;
        std::vector<std::pair<uint64_t, uint32_t>> ranked;
        for (uint32_t i = 0; i < db_.images.size(); i++) {
            if (rec_.images.at(i).registered) continue;
            if (reg_trials_[i] >= opt_.max_reg_trials || !allowed(i)) continue;
            int s = score_cache_[i];
            if (score_check && s != score(i)) {
                fprintf(stderr, "[map] SCORE MISMATCH image %u: cache %d, reference %d\n",
                        i, s, score(i));
                abort();
            }
            if (s < opt_.min_num_pnp_inliers) continue;
            // Off: the raw correspondence count, every candidate in one bucket.
            // On: COLMAP's policy -- spread-based rank, and an image that has
            // already failed once sorts behind every untried one.
            const uint64_t rank =
                opt_.rank_by_visibility
                    ? ((reg_trials_[i] ? 0ull : 1ull) << 32) | pyramid_score_[i]
                    : (uint64_t)s;
            ranked.emplace_back(rank, i);
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second < b.second;  // stable, index-ordered tie-break
        });
        std::vector<uint32_t> out;
        out.reserve(ranked.size());
        for (const auto& r : ranked) out.push_back(r.second);
        return out;
    }

    bool registerImage(uint32_t img) {
        // Gather 2D-3D correspondences (one 3D point per feature: the first seen).
        std::vector<Vec3> X;
        std::vector<Vec3> br;   // observed unit bearings
        std::vector<uint32_t> feat;
        std::vector<uint64_t> pid;
        for (uint32_t f = 0; f < feats_[img].count(); f++) {
            uint64_t chosen = kInvalidPoint3D;
            for (const Correspondence& c : graph_.at(img, f)) {
                const Image& oi = rec_.images.at(c.image_id);
                if (oi.registered && oi.point3D_ids[c.feature_idx] != kInvalidPoint3D) {
                    chosen = oi.point3D_ids[c.feature_idx];
                    break;
                }
            }
            if (chosen == kInvalidPoint3D) continue;
            X.push_back(rec_.points3D[chosen].xyz);
            br.push_back(bearing(img, f));
            feat.push_back(f);
            pid.push_back(chosen);
        }
        if ((int)X.size() < opt_.min_num_pnp_inliers) { reg_fail_.few_corr++; return false; }

        const uint32_t cid = rec_.images[img].camera_id;
        PnPResult r;
        double swept_f = 0;  // focal chosen by the search, 0 = no search ran
        // A supplied focal does *not* switch the sweep off, though the argument
        // for switching it off is tempting (it is a measurement, and a sweep on
        // one image's correspondences once took a dataset's camera
        // from 549 to 5493 px). Measured, gating on it cost that dataset 75
        // images of the primary model and 39 of total coverage: the sweep also
        // lets a second camera group depart from a focal that was measured
        // over the first. The runaway is caught by sanitizeCameras and by the
        // joint refinement (D45), so the sweep stays.
        if (focal_known_.count(cid) || opt_.focal_search_samples <= 0) {
            r = ransacPnP(X, br, camOf(img).focal(), errPx(img));
        } else {
            // First image of this camera: its focal is still the no-EXIF guess,
            // and P3P consumes *calibrated* bearings, so a wrong focal fails
            // outright rather than degrading. Sweep hypotheses log-uniformly
            // and keep the one with the most inliers; BA refines from there.
            const Camera& c0 = camOf(img);
            const int N = opt_.focal_search_samples;
            for (int s = 0; s < N; s++) {
                double t = N > 1 ? (double)s / (N - 1) : 0.0;
                double ratio = opt_.min_focal_ratio *
                               std::pow(opt_.max_focal_ratio / opt_.min_focal_ratio, t);
                Camera trial = c0;
                trial.setFocal(c0.focal() * ratio);
                trial.k1 = trial.k2 = trial.p1 = trial.p2 = 0;
                std::vector<Vec3> bt(feat.size());
                for (size_t k = 0; k < feat.size(); k++) bt[k] = trial.bearing(kp(img, feat[k]));
                PnPResult t_r = ransacPnP(X, bt, trial.focal(), errPx(img));
                if (t_r.success && t_r.num_inliers > r.num_inliers) { r = t_r; swept_f = trial.focal(); }
            }
        }
        // Acceptance gates (COLMAP's): enough inliers *and* enough of the
        // offered correspondences agreeing. An image whose hundreds of 2D-3D
        // candidates yield a bare-minimum consensus is a misregistration
        // waiting to bend the model (D36).
        if (!r.success || r.num_inliers < opt_.min_num_pnp_inliers) {
            reg_fail_.few_inliers++;
            return false;
        }
        if (!ratioOk(r.num_inliers, visiblePool(img, X, br, r.pose)) &&
            !strongUnambiguous(img, X, br, r)) {
            reg_fail_.low_ratio++;
            return false;
        }

        // Commit the searched focal only after the gates pass, then refine the
        // pose on the inlier set (COLMAP refines every accepted PnP pose; the
        // raw P3P/DLT pose is what the next triangulations would build on).
        // A swept focal is only a coarse grid hypothesis, so it is refined
        // jointly with the pose -- a 20% focal error passes RANSAC fine but
        // locks bad intrinsics into the group, and BA then papers over the
        // mismatch with runaway distortion (D36).
        if (swept_f > 0) {
            const double f0 = camOf(img).focal();
            rec_.cameras[cid].setFocal(swept_f);
            for (size_t k = 0; k < feat.size(); k++) br[k] = bearing(img, feat[k]);
            double fs = 1.0;
            refinePose(X, br, r.inlier_mask, r.pose, &fs);
            double refined = swept_f * fs;
            double ratio = refined / default_cams_.at(cid).focal();
            if (fs != 1.0 && ratio > opt_.min_focal_ratio && ratio < opt_.max_focal_ratio) {
                rec_.cameras[cid].setFocal(refined);
                for (size_t k = 0; k < feat.size(); k++) br[k] = bearing(img, feat[k]);
            }
            if (opt_.verbose && std::fabs(camOf(img).focal() - f0) > 1e-6)
                slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_camera_focal,
                         {(long long)cid, slog::num(f0, 0),
                          slog::num(camOf(img).focal(), 0),
                          (long long)r.num_inliers});
        } else {
            refinePose(X, br, r.inlier_mask, r.pose);
        }
        // Re-classify against the refined pose; the gates apply to the final
        // consensus, not the RANSAC one.
        double thr = camOf(img).errRad(opt_.max_reproj_error);
        thr *= thr;
        r.num_inliers = 0;
        for (size_t k = 0; k < X.size(); k++) {
            r.inlier_mask[k] = pnpResidualSq(r.pose, X[k], br[k]) < thr;
            r.num_inliers += r.inlier_mask[k] ? 1 : 0;
        }
        if (r.num_inliers < opt_.min_num_pnp_inliers) { reg_fail_.refined_out++; return false; }
        const size_t pool = visiblePool(img, X, br, r.pose);
        if (!ratioOk(r.num_inliers, pool)) {
            reg_fail_.refined_out++;
            return false;
        }
        if ((double)r.num_inliers < opt_.min_pnp_inlier_ratio * (double)pool)
            reg_fail_.strong++;
        if (pool < X.size()) reg_fail_.occluded += (uint32_t)(X.size() - pool);
        focal_known_.insert(cid);

        rec_.images[img].pose = r.pose;
        rec_.images[img].registered = true;

        // Track continuation: attach inlier features to their 3D points.
        for (size_t k = 0; k < feat.size(); k++) {
            if (!r.inlier_mask[k]) continue;
            uint32_t f = feat[k];
            if (rec_.images[img].point3D_ids[f] != kInvalidPoint3D) continue;
            Point3D& pt = rec_.points3D[pid[k]];
            if (reprojErr(img, f, pt.xyz) > errPx(img)) continue;
            pt.track.push_back({img, f});
            rec_.images[img].point3D_ids[f] = pid[k];
            attachObservation(img, f);
        }
        if (opt_.verbose)
            slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_registered,
                     {(long long)img, (long long)r.num_inliers, (long long)X.size(),
                      (long long)rec_.numRegistered()});
        // Rate-limited inside, and a no-op without --progress-dir: this is the
        // one point at which the model visibly grows. The colouring runs only
        // over the points a snapshot writes, which is why it is a callback and
        // not an assignColors pass per registration.
        progress::model(rec_, false,
                        [this](const Point3D& p, uint8_t rgb[3]) {
                            pointColor(p, rgb);
                        });
        return true;
    }

    // Join this image's features to 3D points the model already has, wherever
    // its pose explains them: registerImage's track-continuation step, split
    // out so a pose set from outside (the audit's repair) can use it too.
    // Returns the number of observations attached.
    uint32_t attachExisting(uint32_t img) {
        uint32_t attached = 0;
        Image& im = rec_.images[img];
        for (uint32_t f = 0; f < feats_[img].count(); f++) {
            if (im.point3D_ids[f] != kInvalidPoint3D) continue;
            for (const Correspondence& c : graph_.at(img, f)) {
                if (c.image_id == img) continue;
                const Image& oi = rec_.images.at(c.image_id);
                if (!oi.registered) continue;
                uint64_t pid = oi.point3D_ids[c.feature_idx];
                if (pid == kInvalidPoint3D) continue;
                auto pt = rec_.points3D.find(pid);
                if (pt == rec_.points3D.end()) continue;
                if (reprojErr(img, f, pt->second.xyz) > errPx(img)) continue;
                bool dup = false;  // one observation per image on a track
                for (const TrackElement& e : pt->second.track)
                    if (e.image_id == img) { dup = true; break; }
                if (dup) continue;
                pt->second.track.push_back({img, f});
                im.point3D_ids[f] = pid;
                attachObservation(img, f);
                attached++;
                break;
            }
        }
        return attached;
    }

    // ---- triangulation of new points seen by a freshly registered image ----
    void triangulateForImage(uint32_t img, double err_scale = 1.0) {
        ModelIndex mi = indexModel();
        triangulateForImageAt(mi, img, err_scale);
    }

    // The index is a whole-model scan, so a caller that runs this over every
    // registered image (completeAndRetriangulate) builds it once.
    void triangulateForImageAt(const ModelIndex& mi, uint32_t img, double err_scale) {
        Image& me = *mi.img[img];
        std::vector<Correspondence> obs;   // reused across features
        std::vector<TrackElement> track;
        for (uint32_t f = 0; f < feats_[img].count(); f++) {
            if (me.point3D_ids[f] != kInvalidPoint3D) continue;
            // candidate observations: registered, feature not yet on a 3D point
            obs.clear();
            for (const Correspondence& c : graph_.at(img, f))
                if (mi.img[c.image_id]->registered &&
                    mi.img[c.image_id]->point3D_ids[c.feature_idx] == kInvalidPoint3D)
                    obs.push_back(c);
            if (obs.empty()) continue;

            // Triangulate with the correspondence of maximum parallax.
            Vec3 bestX;
            double bestAng = -1;
            const Correspondence* bestC = nullptr;
            for (const Correspondence& c : obs) {
                Vec3 X;
                if (!triangulatePairAt(mi, img, f, c.image_id, c.feature_idx, X, err_scale))
                    continue;
                double ang = triangulationAngle(X, cameraCenter(me.pose),
                                                cameraCenter(mi.img[c.image_id]->pose));
                if (ang > bestAng) { bestAng = ang; bestX = X; bestC = &c; }
            }
            if (!bestC) continue;

            // Build the track: this obs + every candidate that reprojects well.
            track.clear();
            track.push_back({img, f});
            for (const Correspondence& c : obs) {
                if (mi.img[c.image_id]->point3D_ids[c.feature_idx] != kInvalidPoint3D) continue;
                if (reprojErrAt(mi, c.image_id, c.feature_idx, bestX) <=
                    err_scale * (opt_.max_reproj_error * mi.pixel_scale[c.image_id]))
                    track.push_back({c.image_id, c.feature_idx});
            }
            if (track.size() < 2) continue;
            // Guard against two features from the same image on one track.
            std::sort(track.begin(), track.end(),
                      [](const TrackElement& a, const TrackElement& b) {
                          return a.image_id < b.image_id;
                      });
            track.erase(std::unique(track.begin(), track.end(),
                                    [](const TrackElement& a, const TrackElement& b) {
                                        return a.image_id == b.image_id;
                                    }),
                        track.end());
            if (track.size() < 2) continue;
            rec_.addPoint3D(bestX, track);
            // Every track element is a registered image's feature joining a
            // point. (Also reached from completeAndRetriangulate, where the
            // counts drift against its direct track edits -- harmless, no
            // ranking happens before the post-refine rebuildScores().)
            for (const TrackElement& e : track) attachObservation(e.image_id, e.point2D_idx);
        }
    }

    // ---- global refinement: BA + filtering + image de-registration ----
    //
    // COLMAP's IterativeGlobalRefinement in miniature: bundle-adjust, filter,
    // and repeat while the model keeps changing, then de-register images the
    // filtering hollowed out. De-registration is the mapper's undo: a
    // registration that passed its PnP gates but stopped agreeing with the
    // refined model exits (and may re-register later, better) instead of
    // staying and bending everything around it (D36).
    void globalRefine(bool final_pass) {
        if (rec_.numRegistered() < 2 || rec_.points3D.size() < 10) return;
        const bool tight = final_pass && opt_.ba_final_tight;
        int rounds = tight ? opt_.ba_max_refinements : 2;
        for (int i = 0; i < rounds; i++) {
            // Observations shredded by the previous round's filtering (or
            // never triangulated because the poses were still rough) get a
            // second chance against the refined geometry -- COLMAP's
            // Retriangulate + CompleteTracks. Without it refinement can only
            // ever LOSE observations, and on sparse match graphs the model
            // starves right after bootstrap (D36).
            if (i > 0 && opt_.retri_scale > 0) {
                ProfTimer pt(g_map_prof.retri);
                completeAndRetriangulate();
            }
            size_t before = countObservations();
            BundleOptions bo;
            bo.real = realCfgFromName(opt_.ba_real);
            bo.device = opt_.device;
            bo.threads = opt_.threads;
            bo.verbose = false;
            bo.loss = opt_.ba_loss;
            bo.loss_param = (float)(opt_.ba_loss_param * medianPixelScale());
            bo.refine_principal_point = opt_.refine_principal_point || final_.pp;
            bo.refine_extra_params = opt_.refine_extra_params || final_.extra;
            bo.pp_min_images = opt_.pp_min_images;
            // Convergence-adaptive iterations (D38): the first round of a
            // growth refine runs to a loose tolerance -- most refines stop
            // there because the model barely changed. A round beyond the
            // first only happens when the previous one moved >ba_refine_change
            // of the observations (retriangulation just rebuilt structure, or
            // filtering shredded it), and those rounds run to the solver's
            // full tolerance: the filters that follow are about to make
            // kill/keep decisions against this geometry, and judging them
            // against a half-converged model is what cost a dataset
            // its tail under D37's fixed cap. Final passes are always tight.
            //
            // The scalar follows the *tolerance*, not the pass: fp32 belongs
            // exactly where the stopping threshold is one it can reach. A round
            // past the first runs to the solver's full tolerance even in a
            // growth refine, and asking fp32 for that means the LM loop spends
            // its whole iteration budget on a threshold below its noise floor.
            // Measured on a 1194-image capture, getting this pairing wrong took
            // the finishing passes from 48 s to 260 s.
            const bool loose = !tight && i == 0 && opt_.ba_growth_rtol > 0;
            if (loose) {
                bo.rtol = opt_.ba_growth_rtol;
                bo.patience = opt_.ba_growth_patience;
            }
            bo.solver = opt_.ba_solver;
            bo.real = baReal(loose);
            bo.shared_ctx = &baContext(loose);
            bo.over_budget_throws = ba_over_budget_throws_;
            double cost = runGlobalBA(rec_, bo);
            ProfTimer pt(g_map_prof.filter);
            // Runs after every mapping BA, and it is load-bearing: without it
            // a capture whose distortion terms drift lands in a self-consistent
            // pancake it cannot climb out of. Measured on an 811-image object
            // capture, disabling it took AUC@10 from 84.3 to 0.0 and the focal
            // to 25219 from a 2813 default. It was once disabled as breaking
            // internet image collections; on the only such collection here
            // (1363 images, one camera each) it is the other way round --
            // AUC@10 83.2 with, 68.7 without.
            if (!final_.no_sanitize) sanitizeCameras();
            int removedObs = 0, removedPts = 0;
            filterPoints(removedObs, removedPts);
            if (opt_.verbose) {
                char cost_s[32];
                std::snprintf(cost_s, sizeof cost_s, "%.3e", cost);
                slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_global_ba,
                         {cost_s, (long long)removedObs, (long long)removedPts,
                          (long long)rec_.points3D.size()});
            }
            if (!before || (double)removedObs / (double)before <= opt_.ba_refine_change) break;
        }
        int dropped;
        {
            ProfTimer pt(g_map_prof.filter);
            dropped = filterImages();
        }
        if (dropped && opt_.verbose)
            fprintf(stderr, "[map] de-registered %d image(s) (few points or bogus camera), "
                    "%u remain\n", dropped, rec_.numRegistered());
    }

    // Fuse two 3D points that a correspondence says are the same feature
    // (COLMAP's IncrementalTriangulator::MergeTracks). Track *creation* only
    // ever gathers features that are still free, so a point triangulated from
    // images A,B and one triangulated from C,D stay separate for good once the
    // pose that connects them arrives -- every later pass sees both features
    // already assigned and leaves them alone. Fusing them is what turns a
    // revisit into a loop closure: one long track constrains the two ends of
    // the loop against each other, two short ones constrain nothing.
    //
    // A merge is accepted only if *every* element of the union reprojects
    // within `err` of the track-length-weighted average position -- the same
    // all-inliers rule COLMAP uses -- if the union keeps at most one
    // observation per image, the invariant the rest of the mapper relies on,
    // and if it still subtends the minimum triangulation angle (D54).
    size_t mergeTracks(const ModelIndex& mi, double err_scale) {
        const double err = err_scale * opt_.max_reproj_error;
        size_t merged = 0;
        std::vector<uint64_t> ids;
        ids.reserve(rec_.points3D.size());
        for (const auto& kv : rec_.points3D) ids.push_back(kv.first);
        // Pairs already judged, so a pair reached from both of its ends costs
        // one test rather than two. Keyed by the two ids packed into one word:
        // this set takes a lookup per (observation, correspondence) over the
        // whole model, which is millions per pass, and a tree node each was
        // most of the cost. Point ids are handed out one per triangulation, so
        // the 32-bit halves are not a practical limit.
        std::unordered_set<uint64_t> tried;
        tried.reserve(4 * rec_.points3D.size());
        std::vector<uint8_t> on_track(db_.images.size(), 0);
        std::vector<Vec3> centers(db_.images.size());
        for (uint32_t i = 0; i < db_.images.size(); i++)
            if (mi.img[i] && mi.img[i]->registered) centers[i] = cameraCenter(mi.img[i]->pose);
        for (uint64_t pid : ids) {
            // The loop below may absorb a point into another and erase it, so
            // the id is re-checked and the walk restarted from the survivor.
            for (int hop = 0; hop < 8; hop++) {
                auto pit = rec_.points3D.find(pid);
                if (pit == rec_.points3D.end()) break;
                uint64_t absorbed = mergeOne(mi, pit->first, err, tried, on_track, centers);
                if (!absorbed) break;
                merged += absorbed;
            }
        }
        return merged;
    }

    // One merge attempt over every correspondence of `pid`'s track; returns the
    // number of observations absorbed (0 if nothing merged).
    // Observations a merged track may reach; see the note in mergeOne.
    static constexpr size_t kMergeMaxTrack = 20;

    size_t mergeOne(const ModelIndex& mi, uint64_t pid, double err,
                    std::unordered_set<uint64_t>& tried, std::vector<uint8_t>& on_track,
                    const std::vector<Vec3>& centers) {
        Point3D& pt = rec_.points3D.at(pid);
        for (size_t ti = 0; ti < pt.track.size(); ti++) {
            const TrackElement e = pt.track[ti];
            for (const Correspondence& c : graph_.at(e.image_id, e.point2D_idx)) {
                const Image* oi = mi.img[c.image_id];
                if (!oi || !oi->registered) continue;
                const uint64_t qid = oi->point3D_ids[c.feature_idx];
                if (qid == kInvalidPoint3D || qid == pid) continue;
                const uint64_t lo = std::min(pid, qid), hi = std::max(pid, qid);
                if (!tried.insert((lo << 32) ^ hi).second) continue;
                auto qit = rec_.points3D.find(qid);
                if (qit == rec_.points3D.end()) continue;
                Point3D& q = qit->second;

                // The reduced camera system the solver builds has an entry per
                // image *pair* on a track, so a track of length t costs
                // t(t+1)/2 -- fusing two length-10 tracks into one costs three
                // times what the two cost apart, for a twentieth observation
                // that constrains a point nineteen views already pin down. On a
                // 1194-image fisheye capture, unbounded fusion took the mean
                // track from 3.5 to 7.2 observations and the bundle adjustment
                // from 0.03 to 0.36 seconds per iteration.
                if (pt.track.size() + q.track.size() > kMergeMaxTrack) continue;
                // One observation per image, or the union is not a track.
                bool clash = false;
                for (const TrackElement& a : pt.track) on_track[a.image_id] = 1;
                for (const TrackElement& b : q.track)
                    if (on_track[b.image_id]) { clash = true; break; }
                for (const TrackElement& a : pt.track) on_track[a.image_id] = 0;
                if (clash) continue;

                const double wa = (double)pt.track.size(), wb = (double)q.track.size();
                const Vec3 x = (pt.xyz * wa + q.xyz * wb) * (1.0 / (wa + wb));
                bool ok = true;
                for (const std::vector<TrackElement>* tr : {&pt.track, &q.track}) {
                    for (const TrackElement& el : *tr)
                        if (reprojErrAt(mi, el.image_id, el.point2D_idx, x) >
                            err * mi.pixel_scale[el.image_id]) { ok = false; break; }
                    if (!ok) break;
                }
                if (!ok) continue;
                // The union must still be a triangulation. Without this a merge
                // can produce a point whose views all sit on one line -- every
                // observation reprojects, so the test above passes -- and the
                // very next filterPoints kills it for lost parallax. The two
                // halves are then retriangulated and merged again on the next
                // round: churn that costs a bundle adjustment each time and
                // ends where it started.
                if (!wellTriangulated(pt.track, q.track, x, centers)) continue;

                const size_t absorbed = q.track.size();
                pt.xyz = x;
                for (const TrackElement& el : q.track) {
                    mi.img[el.image_id]->point3D_ids[el.point2D_idx] = pid;
                    pt.track.push_back(el);
                }
                rec_.points3D.erase(qit);
                return absorbed;
            }
        }
        return 0;
    }

    // Does some pair of views of `a` + `b` see `x` from far enough apart to fix
    // its depth? Same criterion filterPoints applies, so a merge this accepts
    // is one the filter will keep.
    bool wellTriangulated(const std::vector<TrackElement>& a, const std::vector<TrackElement>& b,
                          const Vec3& x, const std::vector<Vec3>& centers) const {
        const double min_ang = opt_.min_tri_angle_deg * M_PI / 180.0;
        // A long track is the case where the pairwise scan would be quadratic
        // and where it is also least needed, so it is sampled with a stride
        // rather than truncated -- the widest baseline of a dolly capture is
        // between its two ends, and a prefix would never see it.
        constexpr size_t kMaxScan = 12;
        std::vector<uint32_t> imgs;
        imgs.reserve(2 * kMaxScan);
        for (const std::vector<TrackElement>* t : {&a, &b}) {
            const size_t stride = std::max<size_t>(1, t->size() / kMaxScan);
            for (size_t i = 0; i < t->size(); i += stride) imgs.push_back((*t)[i].image_id);
        }
        for (size_t i = 0; i + 1 < imgs.size(); i++)
            for (size_t j = i + 1; j < imgs.size(); j++)
                if (triangulationAngle(x, centers[imgs[i]], centers[imgs[j]]) >= min_ang)
                    return true;
        return false;
    }

    // Extend existing tracks to unassigned features that reproject well
    // (transitively: added elements are sources for further completion), then
    // re-run triangulation for every registered image so features whose
    // earlier points were filtered can rebuild them from the refined poses, and
    // finally fuse the tracks that turn out to be the same point.
    //
    // Everything re-added must clear a *stricter* bar (0.75x) than the filter
    // kills at: without the hysteresis, junk in the borderline band churns --
    // filtered at >4 px, immediately re-created at <=4 px -- and every BA
    // round drags the poses toward it again (D36).
    void completeAndRetriangulate() {
        const double err = opt_.retri_scale * opt_.max_reproj_error;  // x pixel_scale below
        ModelIndex mi = indexModel();
        // "Is this image already on the track" as a dense flag rather than a
        // std::set rebuilt (and heap-allocated) per point: there are hundreds
        // of thousands of points and the sets were short-lived and tiny.
        // Cleared by unsetting only what was set, so it stays O(track).
        std::vector<uint8_t> on_track(db_.images.size(), 0);
        for (auto& kv : rec_.points3D) {
            Point3D& pt = kv.second;
            for (const TrackElement& e : pt.track) on_track[e.image_id] = 1;
            for (size_t ti = 0; ti < pt.track.size(); ti++) {
                const TrackElement e = pt.track[ti];  // copy: track grows below
                for (const Correspondence& c : graph_.at(e.image_id, e.point2D_idx)) {
                    Image& oi = *mi.img[c.image_id];
                    if (!oi.registered || on_track[c.image_id] ||
                        oi.point3D_ids[c.feature_idx] != kInvalidPoint3D)
                        continue;
                    if (reprojErrAt(mi, c.image_id, c.feature_idx, pt.xyz) <=
                        err * mi.pixel_scale[c.image_id]) {
                        pt.track.push_back({c.image_id, c.feature_idx});
                        on_track[c.image_id] = 1;
                        oi.point3D_ids[c.feature_idx] = kv.first;
                    }
                }
            }
            for (const TrackElement& e : pt.track) on_track[e.image_id] = 0;
        }
        for (auto& kv : rec_.images)
            if (kv.second.registered) triangulateForImageAt(mi, kv.first, opt_.retri_scale);
        if (opt_.merge_tracks) {
            ProfTimer pt(g_map_prof.merge);
            g_map_prof.n_merged += mergeTracks(mi, opt_.retri_scale);
        }
    }

    size_t countObservations() const {
        size_t n = 0;
        for (const auto& kv : rec_.points3D) n += kv.second.track.size();
        return n;
    }

    // Drop observations that reproject badly and points whose track lost its
    // parallax: a track whose best view pair subtends less than min_tri_angle
    // sits on a near-degenerate cone and feeds PnP unstable geometry (COLMAP
    // filters on both criteria; the old code only checked reprojection).
    void filterPoints(int& removedObs, int& removedPts) {
        // This pass touches every observation in the model on every global
        // refinement round, so it goes through the flat index rather than
        // reprojErr()/errPx()'s five std::map lookups per observation.
        ModelIndex mi = indexModel();
        std::vector<Vec3> centers(db_.images.size());
        for (uint32_t i = 0; i < db_.images.size(); i++)
            if (mi.img[i] && mi.img[i]->registered) centers[i] = cameraCenter(mi.img[i]->pose);
        const double min_ang = opt_.min_tri_angle_deg * M_PI / 180.0;

        // Points are independent here: each one reads its own track and the
        // shared pose/camera table, and the only shared write is clearing
        // point3D_ids[image][feature], which exactly one point owns. So the
        // pass fans out over the point list. It is the mapper's largest host
        // cost on a fisheye capture -- reprojErr's cheirality test inverts the
        // distortion for every observation, and this runs after every one of
        // dozens of bundle adjustments. Results do not depend on the split:
        // each point's verdict is a pure function of its own track.
        std::vector<std::pair<uint64_t, Point3D*>> pts;
        pts.reserve(rec_.points3D.size());
        for (auto& kv : rec_.points3D) pts.emplace_back(kv.first, &kv.second);

        std::vector<uint64_t> drop;
        std::atomic<int> removed_obs_atomic{0};
        const unsigned hc = std::thread::hardware_concurrency();
        int nt = opt_.threads > 0 ? opt_.threads : (hc > 0 ? (int)hc : 1);
        nt = std::max(1, std::min<int>(nt, (int)std::max<size_t>(pts.size() / 512, 1)));
        std::vector<std::vector<uint64_t>> drop_per_thread(nt);
        std::atomic<size_t> next{0};
        const size_t kBlock = 256;

        auto worker = [&](int t) {
            int local_removed = 0;
            std::vector<uint64_t>& local_drop = drop_per_thread[t];
            for (;;) {
                const size_t b = next.fetch_add(kBlock);
                if (b >= pts.size()) break;
                const size_t e = std::min(b + kBlock, pts.size());
                for (size_t pi = b; pi < e; pi++) {
                    Point3D& pt = *pts[pi].second;
                    size_t keep = 0;  // compact in place; surviving order unchanged
                    for (size_t r = 0; r < pt.track.size(); r++) {
                        const TrackElement el = pt.track[r];
                        if (reprojErrAt(mi, el.image_id, el.point2D_idx, pt.xyz) <=
                            opt_.max_reproj_error * mi.pixel_scale[el.image_id]) {
                            pt.track[keep++] = el;
                        } else {
                            mi.img[el.image_id]->point3D_ids[el.point2D_idx] = kInvalidPoint3D;
                            local_removed++;
                        }
                    }
                    pt.track.resize(keep);
                    bool degenerate = pt.track.size() < 2;
                    if (!degenerate) {
                        double best = 0;
                        for (size_t i = 0; i + 1 < pt.track.size() && best < min_ang; i++)
                            for (size_t j = i + 1; j < pt.track.size() && best < min_ang; j++)
                                best = std::max(best,
                                                triangulationAngle(pt.xyz,
                                                                   centers[pt.track[i].image_id],
                                                                   centers[pt.track[j].image_id]));
                        degenerate = best < min_ang;
                    }
                    if (degenerate) {
                        for (const TrackElement& el : pt.track) {
                            mi.img[el.image_id]->point3D_ids[el.point2D_idx] = kInvalidPoint3D;
                            local_removed++;
                        }
                        local_drop.push_back(pts[pi].first);
                    }
                }
            }
            removed_obs_atomic += local_removed;
        };
        if (nt == 1) {
            worker(0);
        } else {
            std::vector<std::thread> pool;
            pool.reserve(nt);
            for (int t = 0; t < nt; t++) pool.emplace_back(worker, t);
            for (std::thread& t : pool) t.join();
        }
        removedObs += removed_obs_atomic.load();
        for (const std::vector<uint64_t>& d : drop_per_thread)
            drop.insert(drop.end(), d.begin(), d.end());
        for (uint64_t id : drop) { rec_.points3D.erase(id); removedPts++; }
    }

    // Undo a registration: detach every observation, drop tracks that fall
    // below two views, unregister. reg_trials_ already counted the attempt,
    // so the image can be retried until its budget runs out (D15's ranking
    // naturally re-offers it once more of the scene exists).
    void deregisterImage(uint32_t img) {
        Image& im = rec_.images[img];
        for (uint32_t f = 0; f < (uint32_t)im.point3D_ids.size(); f++) {
            uint64_t id = im.point3D_ids[f];
            if (id == kInvalidPoint3D) continue;
            im.point3D_ids[f] = kInvalidPoint3D;
            auto it = rec_.points3D.find(id);
            if (it == rec_.points3D.end()) continue;
            auto& tr = it->second.track;
            tr.erase(std::remove_if(tr.begin(), tr.end(),
                                    [&](const TrackElement& e) { return e.image_id == img; }),
                     tr.end());
            if (tr.size() < 2) {
                for (const TrackElement& e : tr)
                    rec_.images[e.image_id].point3D_ids[e.point2D_idx] = kInvalidPoint3D;
                rec_.points3D.erase(it);
            }
        }
        im.registered = false;
    }

    // A camera that left the physically plausible regime is pulled back in
    // rather than getting its images de-registered (where COLMAP kills the
    // images, fatal when a camera group covers many of them). If the wild
    // parameters were benign -- high-order distortion terms unconstrained
    // outside the data's radial support -- residuals barely move and
    // everything survives; if they were absorbing bad geometry, the very next
    // reprojection filter exposes exactly the observations that depended on
    // them, and de-registration proceeds from evidence (D36).
    void sanitizeCameras() {
        for (auto& kv : rec_.cameras) {
            Camera& c = kv.second;
            const Camera& d = default_cams_.at(kv.first);
            int fixed = 0;
            double ratio = c.focal() / d.focal();
            if (!(ratio > opt_.min_focal_ratio && ratio < opt_.max_focal_ratio)) {
                c.setFocal(d.focal());
                fixed++;
            }
            // FullOpenCV's k4..k6 are the rational denominator, but in the same
            // normalized-radius units as k1..k3, so one threshold covers both.
            for (double* k : {&c.k1, &c.k2, &c.k3, &c.k4, &c.k5, &c.k6, &c.p1, &c.p2,
                              &c.sx1, &c.sy1})
                if (std::fabs(*k) > opt_.max_extra_param) { *k = 0; fixed++; }
            // The mapper never has evidence to move the principal point far
            // from the center (COLMAP does not refine it at all during
            // mapping); a large excursion is BA absorbing something else.
            if (std::fabs(c.cx - d.cx) > 0.2 * c.width) { c.cx = d.cx; fixed++; }
            if (std::fabs(c.cy - d.cy) > 0.2 * c.height) { c.cy = d.cy; fixed++; }
            if (fixed && opt_.verbose)
                slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_runaway_params,
                         {(long long)kv.first, (long long)fixed});
        }
    }

    // COLMAP's FilterImages, minus the bogus-camera criterion (cameras are
    // sanitized in place instead): de-register images whose observations
    // collapsed under filtering.
    int filterImages() {
        if (rec_.numRegistered() <= 2) return 0;
        std::vector<uint32_t> drop;
        for (auto& kv : rec_.images) {
            Image& im = kv.second;
            if (!im.registered) continue;
            if ((int)im.numPoint3D() < opt_.min_image_points) drop.push_back(kv.first);
        }
        for (uint32_t id : drop) deregisterImage(id);
        if (!drop.empty()) resetOrphanCameras();
        return (int)drop.size();
    }

    // Give every registered image its own camera, copied from the group it was
    // in. Returns how many were made; 0 when the images already have one each.
    size_t splitCamerasPerImage() {
        std::set<uint32_t> used;
        size_t registered = 0;
        uint32_t next = 0;
        for (const auto& kv : rec_.cameras) next = std::max(next, kv.first);
        for (const auto& kv : rec_.images)
            if (kv.second.registered) {
                used.insert(kv.second.camera_id);
                registered++;
            }
        if (used.size() >= registered) return 0;
        size_t made = 0;
        for (auto& kv : rec_.images) {
            if (!kv.second.registered) continue;
            Camera c = rec_.cameras.at(kv.second.camera_id);
            c.id = ++next;
            rec_.cameras[c.id] = c;
            default_cams_[c.id] = c;
            kv.second.camera_id = c.id;
            made++;
        }
        return made;
    }

    // A camera whose registered images all went away starts over from the
    // pristine default: its focal search / BA state was fit to registrations
    // that have been rejected, and a retry must not inherit that.
    void resetOrphanCameras() {
        std::set<uint32_t> used;
        for (const auto& kv : rec_.images)
            if (kv.second.registered) used.insert(kv.second.camera_id);
        for (auto& kv : rec_.cameras)
            if (!used.count(kv.first) && focal_known_.count(kv.first)) {
                kv.second = default_cams_.at(kv.first);
                // A prior is a measurement of the lens, not of the
                // registrations that were just rejected: it stays known, and
                // default_cams_ already holds it.
                if (!opt_.known_focal_cameras.count(kv.first)) focal_known_.erase(kv.first);
            }
    }

    const MatchesDatabase& db_;
    const std::vector<FeatureSet>& feats_;
    MapperOptions opt_;
    std::vector<uint32_t> cam_ids_;   // per image, 1-based camera id
    std::set<uint32_t> focal_known_;  // cameras whose focal is no longer a guess
    // What a finishing pass frees beyond the mapping-time settings, and what it
    // skips. Set for the duration of one call; empty everywhere else.
    struct FinalRelease {
        bool pp = false;           // the principal point (D51)
        bool extra = false;        // the distortion coefficients (D72)
        bool no_sanitize = false;  // per-image intrinsics: no group to clamp to (D73)
    };
    FinalRelease final_;
    std::map<uint32_t, Camera> default_cams_;  // pristine per-group defaults
    // Best intrinsics any admitted model has produced for each camera group,
    // with the number of images that constrained them (D45; recordCameras).
    std::map<uint32_t, std::pair<Camera, double>> cam_consensus_;
    bool setup_done_ = false;
    // SS_SFM_AUDIT_DUMP=1 prints the support ratio of every audited image,
    // which is how the threshold above was chosen against a rig capture.
    const bool audit_dump_ = spirula::env("SFM_AUDIT_DUMP") != nullptr;
    mutable double scale_cache_ = 0;  // modelScale(), reset by resetModel()
    int init_relax_ = 0;              // reached seed-threshold relaxation level
    InitTally init_tally_;            // why the last initialize() found nothing
    const TwoViewMatches* seed_pair_ = nullptr;  // pair the last seed was built on
    double seed_forward_ = 0;         // its |baseline . viewing dir|
    std::set<std::pair<uint32_t, uint32_t>> used_seeds_;  // seeds already grown
    std::map<std::pair<uint32_t, uint32_t>, TwoViewGeometry> seed_geom_;  // memoized (D38)
    Reconstruction rec_;
    CorrespondenceGraph graph_;
    std::vector<std::vector<uint16_t>> support_;  // per (image, feature), see rebuildScores
    std::vector<int> score_cache_;                // == score(i) for every image
    std::vector<std::vector<uint16_t>> pyramid_;  // per image, kPyrCells occupancy counts
    std::vector<uint32_t> pyramid_score_;         // per image, see pyramidSet
    std::vector<int> reg_trials_;
    // Per image, how many *kept* models registered it (D41). Empty until the
    // first model is kept, which is what makes the whole multi-model path inert
    // while the primary model is being built.
    // Why registerImage() turned an attempt down, summed over the run.
    // Does this consensus clear the ratio gate, or stand on its own without it?
    // See strong_pnp_inliers. Used for the *second* look, after the pose has
    // been refined: the ambiguity test below has already run on the same image.
    bool ratioOk(int inliers, size_t pool) const {
        if ((double)inliers >= opt_.min_pnp_inlier_ratio * (double)pool) return true;
        return opt_.strong_pnp_inliers > 0 && inliers >= opt_.strong_pnp_inliers;
    }

    // How many of the offered correspondences this pose could explain at all:
    // the point in front of the camera and projecting inside the frame. See
    // pnp_ratio_visible_only -- this is the ratio's denominator.
    size_t visiblePool(uint32_t img, const std::vector<Vec3>& X,
                       const std::vector<Vec3>& br, const Pose& pose) const {
        if (!opt_.pnp_ratio_visible_only) return X.size();
        const Camera& cam = camOf(img);
        const double w = cam.width > 0 ? (double)cam.width : 1e9;
        const double h = cam.height > 0 ? (double)cam.height : 1e9;
        // A margin, because a point just outside the frame would have been seen
        // by a pose a pixel away and the gate must not turn on that.
        const double mx = 0.05 * w, my = 0.05 * h;
        size_t n = 0;
        for (size_t k = 0; k < X.size(); k++) {
            const Vec3 pc = mul(pose.R, X[k]) + pose.t;
            // Cheirality as the rest of the mapper does it (D33): a pinhole
            // tests z, a camera that sees past 90 deg tests the sign along the
            // ray the keypoint was measured on.
            if (cam.wideFov()) {
                if (k < br.size() && pc.dot(br[k]) <= 0) continue;
            } else if (pc.z < 1e-8) {
                continue;
            }
            const Vec2 px = cam.project(pc);
            if (!std::isfinite(px.x) || !std::isfinite(px.y)) continue;
            if (px.x < -mx || px.y < -my || px.x > w + mx || px.y > h + my) continue;
            n++;
        }
        return n;
    }

    // Is a consensus that failed the ratio gate both large enough to stand
    // without it and the only one on offer? See strong_pnp_max_rival: the rival
    // is searched for among the correspondences this pose rejected, which is
    // where the other place's would be.
    bool strongUnambiguous(uint32_t img, const std::vector<Vec3>& X,
                           const std::vector<Vec3>& br, const PnPResult& r) {
        if (opt_.strong_pnp_inliers <= 0 || r.num_inliers < opt_.strong_pnp_inliers) return false;
        if (opt_.strong_pnp_max_rival <= 0) return true;
        std::vector<Vec3> X2, b2;
        X2.reserve(X.size());
        b2.reserve(X.size());
        for (size_t k = 0; k < X.size(); k++)
            if (!r.inlier_mask[k]) { X2.push_back(X[k]); b2.push_back(br[k]); }
        const int need = (int)std::ceil(opt_.strong_pnp_max_rival * (double)r.num_inliers);
        if ((int)X2.size() < need) return true;  // not enough left to host a rival
        PnPResult alt = ransacPnP(X2, b2, camOf(img).focal(), errPx(img), 0,
                                  opt_.audit_ransac_trials);
        if (!alt.success || alt.num_inliers < need) return true;
        // A rival that is the *same* pose is the inlier threshold speaking, not
        // a second place. Scale-free: the centres are compared against how far
        // the winning pose stands from what it sees.
        Mat3 D = mul(alt.pose.R, transpose(r.pose.R));
        const double tr = std::max(-1.0, std::min(1.0, (D[0] + D[4] + D[8] - 1) * 0.5));
        if (std::acos(tr) * 180.0 / M_PI > opt_.audit_min_rotation_deg) {
            reg_fail_.ambiguous++;
            return false;
        }
        const Vec3 c = cameraCenter(r.pose);
        double depth = 0;
        size_t nd = 0;
        for (size_t k = 0; k < X.size(); k++)
            if (r.inlier_mask[k]) { depth += (X[k] - c).norm(); nd++; }
        if (nd && (cameraCenter(alt.pose) - c).norm() > 0.1 * depth / (double)nd) {
            reg_fail_.ambiguous++;
            return false;
        }
        return true;
    }

    struct RegFail {
        uint32_t few_corr = 0, few_inliers = 0, low_ratio = 0, refined_out = 0;
        uint32_t strong = 0;     // admitted on absolute support with the ratio failed (D69)
        uint32_t ambiguous = 0;  // ... refused instead because a rival pose fit the leftovers
        uint32_t occluded = 0;   // correspondences the accepted pose could not see at all
    } reg_fail_;
    std::vector<uint8_t> allow_;      // restrictTo(); empty = every image
    size_t allow_count_ = 0;          // ... and how many are set
    std::vector<uint32_t> model_count_;
    std::vector<uint8_t> seeded_;     // blockSeeds(); images an attempt reached
    // Seed candidates for the current restriction, most inliers first.
    std::vector<const TwoViewMatches*> seed_cand_;
    bool seed_cand_valid_ = false;
    // Persistent BA context (D38): device + pipelines survive across the
    // mapper's many global BAs; each solve only creates/frees its own
    // problem-sized buffers. Uninitialized until the first BA initializes it.
    // One per scalar configuration in use: a context holds the shader module it
    // was built from, and that module is compiled for one Real. So a coarse
    // solve in float and a tight one in double cannot share a context -- ba_ctx_[1]
    // exists only when the two differ, and a caller-supplied context serves
    // every solve when they do not (which is the atom workers' case).
    VkContext ba_ctx_[2];
    VkContext* ext_ba_ctx_ = nullptr;  // useBaContext(); null = ba_ctx_ above
    bool ba_over_budget_throws_ = false;  // refineIfItFits() only
    RealCfg baReal(bool coarse) const {
        return realCfgFromName(coarse ? opt_.ba_real_coarse : opt_.ba_real);
    }
    VkContext& baContext(bool coarse) {
        const bool second = coarse && baReal(true) != baReal(false);
        if (ext_ba_ctx_ && !second) return *ext_ba_ctx_;
        return ba_ctx_[second];
    }
    std::vector<uint32_t> recent_regs_;  // registered since the last refinement
};

}  // namespace sfm
