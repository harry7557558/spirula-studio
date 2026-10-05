// The vocabulary of operations on a *set* of reconstructions.
//
// A capture that does not come back as one model needs the same five things
// whatever produced the pieces: merge what belongs together, break what does
// not, register the images nobody claimed, drop the copies, and cut a model
// that has written two places on top of each other. Both mappers need them with
// the identical thresholds, so they live here and neither owns them; the
// schedule that drives them is sfm/map/Assemble.h.
//
// Everything here is a free function over `Mapper`'s public operations. The
// memo is the one piece of state: passes are skipped for models they have
// already seen in this shape, which is what keeps a schedule that runs them
// repeatedly from paying for them repeatedly.
#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"
#include "sfm/core/Log.h"

namespace sfm {

// The thresholds every pass here is judged by, shared by both mappers. Named
// for the manage loop that used to drive them (D44), and kept as one struct
// because they are one policy: what may be merged, what may be broken, and what
// counts as evidence for either.
struct ManagerOptions {
    MergeOptions merge;
    bool do_merge = true;
    bool do_grow = true;      // register unregistered images into existing models
    bool do_reseed = true;    // look for models among images still uncovered
    // Audit a merged model's poses against the correspondence graph and
    // re-register what it cannot support (Mapper::audit). Off means a merged
    // model is only bundle-adjusted, which cannot undo a gross misplacement.
    //
    // **Off**, on measurement (D71). The repair moves an image to a pose chosen
    // by a RANSAC over correspondences the image does not own, and that pose is
    // -- in the audit's own words -- supported by evidence "weaker than
    // registration demands". At a handful of images that is a good trade. At
    // scale it is not: on a 5356-image capture the audit repaired 77 images and
    // took AUC@10 from **93.5 to 65.0**, and on a 7620-image one it repaired 558
    // and cost 7 points. It is also the most expensive pass in the pipeline --
    // 421 s of a 2076 s run there, 242 s of a 2005 s run here -- so it was
    // buying that with the largest bill of any finishing pass.
    //
    // The refinement it used to end with still happens either way (auditModels
    // falls back to Mapper::refine), so what is switched off is the repair
    // alone. D44's case for it -- rig frames left tens of degrees apart by a
    // merge of two drifted halves -- is real and predates the seam validator
    // (D45) and the splice test, which refuse that merge outright now. If it
    // recurs, it wants a repair that de-registers rather than guesses.
    bool do_audit = false;
    // What an audit's repair pass may grow a model to, as a fraction of what it
    // already held (see Mapper::audit). 0 leaves it uncapped, which is a full
    // incremental growth pass hiding inside a repair; the assembler has a growth
    // schedule of its own and overrides this with it.
    double audit_growth_frac = 0;
    // A model this fraction of whose images another (larger) model also holds
    // carries nothing of its own: growth has made it a duplicate. It is
    // dropped rather than written as a reconstruction in its own right.
    double redundant_ratio = 0.9;
    // Cameras whose focal deviates from the population consensus by more than
    // this factor are refit before merging (see refitOutlierCameras). A model
    // built on a runaway focal cannot align with anything, and on a rig every
    // model is looking at the same physical cameras.
    double focal_consensus_tol = 0.15;  // 0 disables
    // Cross-seam agreement required of a merge (Mapper::checkSeam). A verified
    // pair whose two images end up on opposite sides of the seam is evidence
    // the alignment never saw; the merged model has to reproduce most of them.
    // 0 disables the check.
    double seam_min_agreement = 0.6;   // fraction of tested cross-seam pairs
    double seam_min_pair_fraction = 0.5;  // ... of a pair's matches, to call it holding
    int seam_min_pairs = 10;           // below this there is nothing to judge on
    // ... and that pair fraction is a *ceiling*, not the bar. The bar is what
    // the model's own non-crossing pairs achieve, times this (D68). A verified
    // pair is not ground truth: on a repetitive interior some of them join two
    // places that merely look alike, and there the same measurement over pairs
    // nobody doubts comes back far below 0.5 -- so a fixed bar asks a correct
    // merge to beat evidence the capture does not contain, and refuses it.
    //
    // Measured on a 7620-image capture that is half building interior: the
    // assembler refused 41 of 43 merges, and merging the models it wrote
    // afterwards with no seam test at all recovered 268 images and 3 points of
    // AUC@10. It only ever loosens -- min() with the fraction above -- so a
    // capture whose pairs are trustworthy is judged exactly as before.
    double seam_relative_bar = 0.6;    // 0 uses seam_min_pair_fraction flat
    int seam_reference_pairs = 400;    // non-crossing pairs sampled for it
    // A merge that fails the test above is not necessarily wrong -- it may
    // merely be out of true (D64). The test measures pixels on a model no
    // bundle adjustment has seen: the incoming half arrives under one
    // similarity, so two components that are each sound but drifted differently
    // sit a fraction of a degree apart across the seam, and at 8 px on an
    // 800 px focal that is the whole tolerance. Every cross-seam pair then
    // fails, and a merge of two large correct components is refused.
    //
    // What separates that from a wrong merge is not how many pairs hold, but
    // how much of a pair the model explains when it does not: a seam that is
    // out of true still explains a fifth of a typical pair's matches, two
    // different places explain none. So a refusal whose median pair is above
    // `seam_rescue_frac` earns one coarse refinement of the merged model and a
    // second verdict; the refinement is kept when it passes.
    // Only *failed* rescues are charged against the budget. A refinement that
    // bought a merge is not overhead -- the model is kept in its refined form,
    // so it is work the next pass does not repeat. What has to be bounded is a
    // capture where the rescue never works, and eight wasted refinements is
    // enough to establish that.
    double seam_rescue_frac = 0.2;     // 0 disables the rescue
    int seam_max_rescues = 8;          // refinements it may waste before giving up
    // Break a model whose own verified pairs do not agree with it
    // (Mapper::splitInconsistent). Only pairs with at least
    // `split_min_matches` are trusted to vote, and a resulting group smaller
    // than `split_min_group` is de-registered rather than written out.
    bool do_split = true;
    int split_min_matches = 30;
    size_t split_min_group = 10;
    // Folded models: two similar-looking parts of the capture written on top of
    // each other (findDuplicateStructure). Detected from missing structure, not
    // from disagreement, because a fold agrees with itself.
    //
    // On by default since D46, when the verdict stopped being the conflict
    // *rate* -- which does not separate a fold from a dense capture -- and
    // became what the implied split would cost. Measured with the same options
    // on real-world datasets, the rate is *higher* on the sound model; the cut
    // differs by a hundredfold, because two places written on top of each other
    // never shared structure to  begin with, while cutting a sound model always
    // runs through structure it genuinely shares.
    // See DuplicateOptions::max_cut_fraction.
    bool do_duplicate_split = true;
    DuplicateOptions duplicate;
    bool verbose = true;
};

struct ManagerStats {
    size_t rounds = 0;
    size_t merges = 0, merges_refused = 0;
    size_t grown_images = 0;       // images registered by growth passes
    size_t reseeded_models = 0;
    size_t dropped_redundant = 0;
    size_t audited_out = 0, audited_repaired = 0;
    size_t cameras_refit = 0;
    size_t seam_checked = 0, seam_refused = 0, seam_skipped = 0;
    size_t seam_rescued = 0, seam_rescue_failed = 0;
    // What the merges looked like on both sides of the verdict: the median
    // cross-seam pair's explained fraction and how many pairs there were to
    // judge on, summed so the report can average them. The accepted ones are
    // the reference the refused ones have to be read against -- a threshold is
    // only defensible if the two populations are actually separated.
    // `rescue_gain` is how much the rescue's refinement moved that median when
    // it did not work, which says whether the seam is out of true or wrong.
    double seam_refused_median = 0, seam_passed_median = 0, seam_rescue_gain = 0;
    // Summed over judged merges: the bar each was held to, and what the model's
    // own non-crossing pairs explained. Both are needed to read the two medians
    // above -- a low refused median is only damning against a high bar (D68).
    double seam_bar_sum = 0, seam_reference_sum = 0;
    size_t seam_refused_pairs = 0, seam_passed = 0;
    size_t splits = 0, duplicate_splits = 0, split_dropped = 0;
    size_t seams_welded = 0;       // open pairs Mapper::weldSeams fused
    size_t models_before = 0, models_after = 0;
    size_t covered_before = 0, covered_after = 0;
};

// Models are moved and re-indexed by every pass, so a pass that wants to skip
// what it has already done cannot remember an index. It remembers the shape --
// registered images and 3D points -- which changes whenever anything touched
// the model, and whose collisions cost one skipped attempt that was
// overwhelmingly likely to be a no-op anyway.
struct ModelMemo {
    using Signature = std::pair<uint32_t, size_t>;
    static Signature of(const Reconstruction& m) {
        return {m.numRegistered(), m.points3D.size()};
    }
    std::set<Signature> barren;   // a growth pass could not extend these
    std::set<Signature> audited;  // already checked in this shape
    std::set<Signature> split;    // already examined for a bad seam
    void clear() {
        barren.clear();
        audited.clear();
        split.clear();
    }
};

// Movement at the frame corner, in pixels, below which a joint refinement is
// taken to have changed nothing anyone downstream can act on -- well under the
// registration and filtering tolerances, which are a few pixels.
inline constexpr double kJointBaMovedPx = 1.0;

// How far apart the components' intrinsics are, in pixels at the frame corner.
//
// This is the quantity a joint refinement exists to close (D45): a component
// that fitted its own focal to its own noise. When the components already agree
// to under a pixel there, the solve is a bundle adjustment over the whole
// capture that ends where it started.
inline double focalSpreadPx(const std::vector<Reconstruction>& models) {
    std::map<uint32_t, std::pair<double, double>> range;  // id -> (min f, max f)
    std::map<uint32_t, double> radius;                    // ... and its frame corner
    for (const Reconstruction& m : models)
        for (const auto& kv : m.cameras) {
            auto it = range.find(kv.first);
            if (it == range.end()) {
                range[kv.first] = {kv.second.focal(), kv.second.focal()};
                radius[kv.first] = std::hypot(kv.second.cx, kv.second.cy);
            } else {
                it->second.first = std::min(it->second.first, kv.second.focal());
                it->second.second = std::max(it->second.second, kv.second.focal());
            }
        }
    double px = 0;
    for (const auto& kv : range) {
        const double f = 0.5 * (kv.second.first + kv.second.second);
        if (f > 0)
            px = std::max(px, (kv.second.second - kv.second.first) / f * radius[kv.first]);
    }
    return px;
}

// The cross-seam agreement test, as a merge validator (D45).
//
// Every test MergeSession can run by itself is computed from the evidence the
// alignment already used, and repeated structure satisfies all of it -- a fold
// agrees with itself. This one asks the correspondence graph instead: do the
// verified two-view geometries that cross the seam still hold in the merged
// model? It is the only test that separates "these two places look the same"
// from "these two places are the same".
//
// The bottom-up tree needs it as much as the manage loop does. Its levels
// perform hundreds of merges, and without this they run with nothing watching
// for repeated structure at all.
// The rescue (see ManagerOptions::seam_rescue_frac) makes this a judge that can
// repair what it judges: a marginal verdict is re-taken on a refined model, and
// the refinement is what gets committed when the second verdict passes.
inline std::function<std::string(Reconstruction&, const Reconstruction&, const Sim3&,
                                 const MergeCounts&)>
seamValidator(Mapper& mapper, const ManagerOptions& opt, ManagerStats* st = nullptr) {
    if (opt.seam_min_agreement <= 0) return {};
    Mapper* mp = &mapper;
    const double max_err = opt.merge.max_reproj_error;
    const double pair_frac = opt.seam_min_pair_fraction;
    const double min_agree = opt.seam_min_agreement;
    const size_t min_pairs = (size_t)std::max(0, opt.seam_min_pairs);
    const double rescue_frac = opt.seam_rescue_frac;
    const double rel_bar = opt.seam_relative_bar;
    const size_t ref_pairs = (size_t)std::max(0, opt.seam_reference_pairs);
    // Shared by every merge this validator is handed to, so the budget bounds
    // the pass rather than each attempt in it.
    auto budget = std::make_shared<int>(std::max(0, opt.seam_max_rescues));
    // The reference is a property of the capture's matches, not of one seam, so
    // it is measured once and reused -- but only once there were enough pairs
    // to measure it on, since the first merge of a run may join two models too
    // small to say anything. -1 = not yet established.
    auto reference = std::make_shared<double>(-1.0);
    const double max_splice = opt.merge.max_splice_conflict_ratio;
    return [mp, max_err, pair_frac, min_agree, min_pairs, rescue_frac, rel_bar, ref_pairs,
            max_splice, budget, reference, st](Reconstruction& merged, const Reconstruction& src,
                                               const Sim3&,
                                               const MergeCounts& counts) -> std::string {
        std::set<uint32_t> src_side;
        for (const auto& kv : src.images)
            if (kv.second.registered) src_side.insert(kv.first);
        // A merge the splice test would have refused, sent here because its
        // alignment is too well determined for "these are different places" to
        // be the explanation (D64). Judging it as it stands measures the same
        // unreconciled shapes a second time, so it is refined first, always.
        const bool contested =
            counts.points_spliced &&
            counts.splice_conflicts > max_splice * (double)counts.points_spliced;
        // A model too large to bundle-adjust on this device still gets judged,
        // on the geometry it arrived with. That is a harsher test than the
        // arbitration intends, but it is evidence, and refusing a merge because
        // the machine is small is not.
        if (contested && !mp->refineIfItFits(merged, /*coarse=*/true))
            if (st) st->seam_rescue_failed++;
        // What this capture's own pairs achieve inside the merged model, which
        // is the reference the seam is judged against (D68). Measured on the
        // merged model rather than once per capture: it is the same pairs and
        // the same code, and a model that has just absorbed another is the
        // thing whose seam is in question.
        double bar = pair_frac;
        if (rel_bar > 0 && ref_pairs) {
            if (*reference < 0) {
                Mapper::SeamCheck ref = mp->checkSeam(merged, src_side, max_err, pair_frac,
                                                      ref_pairs, /*crossing=*/false);
                if (ref.tested >= ref_pairs / 2) *reference = ref.median_frac;
                else if (ref.tested >= min_pairs) bar = std::min(bar, rel_bar * ref.median_frac);
            }
            if (*reference >= 0) bar = std::min(bar, rel_bar * *reference);
            if (st) st->seam_reference_sum += *reference >= 0 ? *reference : bar / rel_bar;
        }
        Mapper::SeamCheck sc = mp->checkSeam(merged, src_side, max_err, bar);
        // Nothing to judge on. Counted, because "the test passed" and "the test
        // could not run" are different facts and a merge tree that accumulates
        // the second one is merging unwatched.
        if (sc.tested < min_pairs) {
            if (st) st->seam_skipped++;
            return "";
        }
        if (st) { st->seam_checked++; st->seam_bar_sum += bar; }
        if ((double)sc.agree / (double)sc.tested >= min_agree) {
            if (st) {
                st->seam_passed++;
                st->seam_passed_median += sc.median_frac;
                // A merge the splice test would have refused, carried by the
                // cross-seam evidence once its two halves were reconciled: the
                // case the arbitration exists for.
                if (contested) st->seam_rescued++;
            }
            return "";
        }

        // Close, but not converged: optimize the seam and ask again. The
        // refinement is coarse on purpose -- the question is whether the two
        // halves can be reconciled at all, and whatever keeps this model will
        // solve it properly. A model that comes back passing is kept in its
        // refined form, so the work is not repeated downstream either. A
        // contested merge has already had exactly this and does not get it twice.
        if (!contested && rescue_frac > 0 && sc.median_frac >= rescue_frac && *budget > 0) {
            Reconstruction fixed = merged;
            Mapper::SeamCheck s2;
            if (mp->refineIfItFits(fixed, /*coarse=*/true))
                s2 = mp->checkSeam(fixed, src_side, max_err, bar);
            if (s2.tested >= min_pairs && (double)s2.agree / (double)s2.tested >= min_agree) {
                if (st) st->seam_rescued++;
                merged = std::move(fixed);
                return "";
            }
            --*budget;  // only the wasted ones are charged
            if (st) {
                st->seam_rescue_failed++;
                if (s2.tested) st->seam_rescue_gain += s2.median_frac - sc.median_frac;
            }
            if (s2.tested) sc = s2;
        }
        if (st) {
            st->seam_refused++;
            st->seam_refused_median += sc.median_frac;
            st->seam_refused_pairs += sc.tested;
        }
        char buf[224];
        snprintf(buf, sizeof buf,
                 "only %zu of the %zu verified pairs that cross the seam still hold "
                 "(median %.0f%% of their matches explained, against a %.0f%% bar)",
                 sc.agree, sc.tested, 100.0 * sc.median_frac, 100.0 * bar);
        return std::string(buf);
    };
}

// Distinct images any model registers.
inline std::set<uint32_t> coveredImages(const std::vector<Reconstruction>& models) {
    std::set<uint32_t> ids;
    for (const Reconstruction& m : models)
        for (const auto& kv : m.images)
            if (kv.second.registered) ids.insert(kv.first);
    return ids;
}

// COLMAP orders written models by 3D point count, descending
// (ReconstructionManager::Write), so sparse/0 is the model with the most
// structure. Every pass that ranks models uses the same order.
inline void sortModels(std::vector<Reconstruction>& models) {
    std::stable_sort(models.begin(), models.end(),
                     [](const Reconstruction& a, const Reconstruction& b) {
                         return a.points3D.size() > b.points3D.size();
                     });
}

// ---- the passes ---------------------------------------------------------

// Models that growth has turned into copies of what is already kept.
//
// Against the *union* of the models kept before it, not against one of them at
// a time. Testing one at a time misses the case that actually occurs: on a
// 5356-image capture the run ended with models of 5249, 5168, 3770 and 1389
// images where each of the last three was 98-100% inside the union of the
// others and none was 90% inside any single one, because their images were
// split between two bigger models. Largest first, so what a model is measured
// against is only ever models that outrank it.
// `follow` arrays are reordered and filtered with the models. The bottom-up
// tree carries a per-model "this one merged" flag across this pass, and losing
// it would make its final audit re-check every model instead of the ones with a
// new seam.
inline std::vector<Reconstruction> dropRedundantModels(std::vector<Reconstruction> models,
                                                       const ManagerOptions& opt,
                                                       ManagerStats& st,
                                                       std::vector<std::vector<char>*> follow = {}) {
    std::vector<size_t> order(models.size());
    std::iota(order.begin(), order.end(), (size_t)0);
    std::stable_sort(order.begin(), order.end(), [&models](size_t a, size_t b) {
        return models[a].points3D.size() > models[b].points3D.size();
    });
    std::vector<size_t> keep;
    std::set<uint32_t> covered;
    for (size_t rank = 0; rank < order.size(); rank++) {
        const Reconstruction& m = models[order[rank]];
        const uint32_t n = m.numRegistered();
        size_t shared = 0;
        for (const auto& kv : m.images)
            if (kv.second.registered && covered.count(kv.first)) shared++;
        if (rank && n && (double)shared >= opt.redundant_ratio * (double)n) {
            st.dropped_redundant++;
            if (opt.verbose)
                slog::diag(slog::Tag::Map,
                           "[mgr] dropped a %u-image model: %zu of its images are already in "
                           "larger ones", n, shared);
            continue;
        }
        for (const auto& kv : m.images)
            if (kv.second.registered) covered.insert(kv.first);
        keep.push_back(order[rank]);
    }
    for (std::vector<char>* f : follow) {
        std::vector<char> next;
        next.reserve(keep.size());
        for (size_t i : keep) next.push_back(i < f->size() ? (*f)[i] : 0);
        *f = std::move(next);
    }
    std::vector<Reconstruction> out;
    out.reserve(keep.size());
    for (size_t i : keep) out.push_back(std::move(models[i]));
    return out;
}

// Break models the correspondence graph says are not one thing.
//
// The merge pass refuses a bad merge before it commits, but models arrive here
// that were never merged and are wrong anyway -- a chain of registrations
// through repeated structure does the same damage, and D41's own output has
// models like that. Splitting them is what makes the rest of the loop work:
// each piece is a sound reconstruction, and the merge pass gets to try again
// with the seam test watching.
//
// Only models that changed since they were last split are re-examined, so a
// split and a merge cannot chase each other round after round.
inline std::vector<Reconstruction> splitInconsistentModels(Mapper& mapper,
                                                           std::vector<Reconstruction> models,
                                                           const ManagerOptions& opt,
                                                           ModelMemo& memo, ManagerStats& st) {
    std::vector<Reconstruction> out;
    for (Reconstruction& m : models) {
        const ModelMemo::Signature sig = ModelMemo::of(m);
        if (m.numRegistered() < 2 * opt.split_min_group || !memo.split.insert(sig).second) {
            out.push_back(std::move(m));
            continue;
        }
        Mapper::SplitStats ss;
        std::vector<Reconstruction> parts = mapper.splitInconsistent(
            m, opt.merge.max_reproj_error, opt.seam_min_pair_fraction, opt.split_min_matches,
            opt.split_min_group, &ss);
        if (parts.size() <= 1) {
            out.push_back(std::move(m));
            continue;
        }
        st.splits++;
        st.split_dropped += ss.dropped_images;
        if (opt.verbose)
            slog::diag(slog::Tag::Map,
                       "[mgr] split a %u-image model into %zu (%zu of %zu inner pairs hold; "
                       "largest group %zu, %zu images dropped)",
                       m.numRegistered(), parts.size(), ss.pairs_agree, ss.pairs_tested, ss.largest,
                       ss.dropped_images);
        for (Reconstruction& p : parts) {
            memo.split.insert(ModelMemo::of(p));
            out.push_back(std::move(p));
        }
    }
    return out;
}

// Break a model that has written two places on top of each other. This is the
// failure the agreement split above cannot see -- a fold agrees with itself --
// and it is detected from what the model is *missing*: images it places in the
// same spot looking the same way, with no structure in common
// (findDuplicateStructure).
inline std::vector<Reconstruction> splitFoldedModels(Mapper& mapper,
                                                     std::vector<Reconstruction> models,
                                                     const ManagerOptions& opt, ModelMemo& memo,
                                                     ManagerStats& st) {
    std::vector<Reconstruction> out;
    MatchedFn matched = mapper.matchedPredicate();
    for (Reconstruction& m : models) {
        if (m.numRegistered() < 2 * opt.split_min_group) {
            out.push_back(std::move(m));
            continue;
        }
        DuplicateReport dr = findDuplicateStructure(m, opt.duplicate, matched);
        if (!dr.duplicated(opt.duplicate)) {
            out.push_back(std::move(m));
            continue;
        }
        size_t dropped = 0;
        DuplicateCut cut;
        std::vector<Reconstruction> parts =
            splitDuplicateStructure(m, dr, opt.split_min_group, &dropped, &cut,
                                    opt.duplicate.min_fold_overlap);
        // The conflicts say a fold is possible; the cut says whether the split
        // is cheap. Only a genuine fold is both (D46) -- cutting a sound model
        // always runs through structure it really does share.
        if (parts.size() <= 1 || !foldSplitAccepted(dr, cut, opt.duplicate)) {
            if (opt.verbose && parts.size() > 1)
                slog::diag(slog::Tag::Map,
                           "[mgr] a %u-image model has %zu of %zu co-located pairs with nothing "
                           "in common, but splitting it would sever %.1f%% of its co-visibility "
                           "(>%.1f%%): keeping it whole",
                           m.numRegistered(), dr.conflicts, dr.colocated, 100.0 * cut.fraction(),
                           100.0 * opt.duplicate.max_cut_fraction);
            else if (opt.verbose && cut.reattached)
                slog::diag(slog::Tag::Map,
                           "[mgr] a %u-image model has %zu of %zu co-located pairs with nothing "
                           "in common, but the %zu piece(s) they would cut off stand where nothing "
                           "else does (<%.0f%%): keeping it whole",
                           m.numRegistered(), dr.conflicts, dr.colocated, cut.reattached,
                           100.0 * opt.duplicate.min_fold_overlap);
            out.push_back(std::move(m));
            continue;
        }
        st.duplicate_splits++;
        st.split_dropped += dropped;
        if (opt.verbose) {
            char head[512];
            snprintf(head, sizeof head,
                     "[mgr] a %u-image model has %zu of %zu co-located image pairs with no "
                     "structure in common and no match either (%zu more share nothing but were "
                     "matched), a cut that severs %.2f%% of its co-visibility, and every piece "
                     "standing where another one does (%.0f%% at worst): two places written on "
                     "top of each other. Splitting into",
                     m.numRegistered(), dr.conflicts, dr.colocated, dr.unmatched_but_seen,
                     100.0 * cut.fraction(), 100.0 * cut.min_overlap);
            std::string line = head;
            for (const Reconstruction& p : parts)
                line += " " + std::to_string(p.numRegistered());
            if (dropped) line += " (" + std::to_string(dropped) + " images dropped)";
            slog::diag(slog::Tag::Map, "%s", line.c_str());
        }
        for (Reconstruction& p : parts) {
            memo.split.insert(ModelMemo::of(p));
            out.push_back(std::move(p));
        }
    }
    return out;
}

// ---- camera consensus ----------------------------------------------------
//
// Every model of one capture is looking at the same physical cameras, so a
// camera id that one model puts at a wildly different focal from the rest is
// that model's mistake -- the focal search landing in a bad basin, kept by a BA
// that had too few images to contradict it.
//
// The fix is to give them the consensus intrinsics and re-run the mapper's
// refinement, which re-fits the poses and structure to them. If that was the
// only thing wrong, the model comes back consistent with everything else; if it
// was not, refinement filters it down and the merge tests still refuse it.
// Weighted by image count, so the consensus comes from the models with the
// evidence.
inline void refitOutlierCameras(Mapper& mapper, std::vector<Reconstruction>& models,
                                const ManagerOptions& opt, ManagerStats& st) {
    if (models.size() < 3) return;  // no population to take a consensus from
    std::map<uint32_t, std::vector<std::pair<double, uint32_t>>> focals;  // id -> (f, weight)
    std::map<uint32_t, uint32_t> widest;  // id -> most images any model gives it
    for (const Reconstruction& m : models) {
        std::map<uint32_t, uint32_t> used;
        for (const auto& kv : m.images)
            if (kv.second.registered) used[kv.second.camera_id]++;
        for (const auto& kv : m.cameras) {
            auto u = used.find(kv.first);
            if (u == used.end()) continue;
            focals[kv.first].push_back({kv.second.focal(), u->second});
            widest[kv.first] = std::max(widest[kv.first], u->second);
        }
    }
    std::map<uint32_t, double> consensus;
    for (const auto& kv : focals) {
        // A camera group of one image has no population to be an outlier of:
        // its focal is fitted from that image alone in every model that holds
        // it, so another model's value is not better evidence, and "refitting"
        // costs a full bundle adjustment of the whole model to swap one guess
        // for another. On an internet collection, where every image is its own
        // camera (D20), that fired on hundreds of cameras and made the pass the
        // longest stage of the run.
        if (widest[kv.first] < 2 || kv.second.size() < 3) continue;
        // Weighted median: the model with the most images decides ties, and a
        // couple of runaway small models cannot move it.
        std::vector<std::pair<double, uint32_t>> v = kv.second;
        std::sort(v.begin(), v.end());
        uint64_t total = 0;
        for (const auto& p : v) total += p.second;
        uint64_t acc = 0;
        double med = v.empty() ? 0 : v.front().first;
        for (const auto& p : v) {
            acc += p.second;
            med = p.first;
            if (acc * 2 >= total) break;
        }
        consensus[kv.first] = med;
    }
    for (Reconstruction& m : models) {
        std::vector<uint32_t> bad;
        for (const auto& kv : m.cameras) {
            auto c = consensus.find(kv.first);
            if (c == consensus.end()) continue;
            const double f = kv.second.focal(), ref = c->second;
            if (ref > 0 && std::fabs(f - ref) > opt.focal_consensus_tol * ref)
                bad.push_back(kv.first);
        }
        if (bad.empty()) continue;
        // Take the whole camera from the model that owns the consensus focal,
        // not just its focal: the distortion terms of a camera that ran away are
        // fitted to the same mistake.
        for (uint32_t id : bad) {
            const Camera* donor = nullptr;
            for (const Reconstruction& other : models) {
                auto it = other.cameras.find(id);
                if (it == other.cameras.end()) continue;
                if (std::fabs(it->second.focal() - consensus[id]) < 1e-6 * consensus[id]) {
                    donor = &it->second;
                    break;
                }
            }
            if (!donor) continue;
            if (opt.verbose)
                slog::diag(slog::Tag::Map,
                           "[mgr] camera %u of a %u-image model: focal %.0f vs the %.0f the other "
                           "models agree on; refitting", id, m.numRegistered(),
                           m.cameras[id].focal(), donor->focal());
            m.cameras[id] = *donor;
            st.cameras_refit++;
        }
        const uint32_t before = m.numRegistered();
        m = mapper.refine(m);
        if (opt.verbose)
            slog::diag(slog::Tag::Map, "[mgr] refit model: %u -> %u images, %zu points", before,
                       m.numRegistered(), m.points3D.size());
    }
}

// Check every model that has changed, and refine it.
//
// A merged model is two halves glued along a seam that has never been optimized
// as one, and a single similarity cannot place both halves of a drifted model
// correctly -- it fits the overlap and misplaces what is far from it. Bundle
// adjustment cannot walk a misplacement back, so the mapper first asks the
// correspondence graph whether every image belongs where the model puts it
// (Mapper::audit) and re-registers the ones that do not. Models nothing has
// touched keep their exact shape and are skipped, so this costs nothing on a
// dataset with nothing to fix.
inline std::vector<Reconstruction> auditModels(Mapper& mapper, std::vector<Reconstruction> models,
                                               const ManagerOptions& opt, ModelMemo& memo,
                                               ManagerStats& st) {
    for (Reconstruction& m : models) {
        if (!memo.audited.insert(ModelMemo::of(m)).second) continue;
        const size_t pts = m.points3D.size();
        const uint32_t imgs = m.numRegistered();
        Mapper::AuditStats as;
        const uint32_t cap =
            opt.audit_growth_frac > 0
                ? (uint32_t)std::ceil((1.0 + opt.audit_growth_frac) * (double)imgs)
                : 0;
        m = opt.do_audit ? mapper.audit(m, &as, cap) : mapper.refine(m);
        st.audited_out += as.deregistered;
        st.audited_repaired += as.reregistered;
        memo.audited.insert(ModelMemo::of(m));
        if (opt.verbose && (as.unsupported || m.numRegistered() != imgs))
            slog::diag(slog::Tag::Map, "[mgr] audited a model: %u -> %u images, %zu -> %zu points "
                       "(%u unsupported, %u re-registered, %u dropped)",
                       imgs, m.numRegistered(), pts, m.points3D.size(), as.unsupported,
                       as.reregistered, as.deregistered);
    }
    return models;
}

}  // namespace sfm
