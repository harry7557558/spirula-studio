// Turning a set of models into reconstructions (D57, D63).
//
// Two mappers produce that set and from there they need the identical thing.
// The flat one seeds, grows until nothing else registers, and seeds again among
// what is left (D41); the bottom-up one cuts the view graph into atoms and
// reconstructs them concurrently (sfm/map/Bottomup.h). Either way what comes
// out is several models that overlap in places, and the rest of the work is:
//
//   * merge the ones that belong together, checking each result against
//     evidence the merge itself never used (ModelOps.h's seam validator);
//   * grow the ones that did not merge, because merging cannot invent overlap
//     that is not there and registration is the only operation that creates it;
//   * optimize what changed, so the next round judges a settled model;
//   * and once at the end, do what no amount of merging can: break a model its
//     own correspondences contradict, cut a fold, register the tail, seed among
//     whatever nothing reached.
//
// The first three are a *level*, and levels repeat until one stops changing
// anything. That is a merge tree when the input is hundreds of atoms and two or
// three rounds when it is the flat mapper's handful of components -- the same
// loop either way, which is why it is here and not in either mapper.
//
// This replaced the manage loop (D44), which drove the same operations in
// rounds but re-ran the expensive ones over models nothing had touched, and
// whose repair pass grew every model it repaired without a bound: on a
// 5356-image capture it spent 65 minutes to merge three models and recover 119
// images, ending with three near-copies of the same reconstruction.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"
#include "sfm/core/Log.h"
#include "sfm/map/ModelOps.h"
#include "i18n/catalog/Sfm.h"

namespace sfm {

struct AssembleOptions {
    // Levels of the merge tree. Each halves the model count, so the ceiling is
    // log2(models) plus the extra levels a refused pair earns after a
    // refinement.
    int max_rounds = 16;
    // One bundle adjustment across every live model with intrinsics shared per
    // camera group, after every level that changed anything.
    bool joint_intrinsics = true;
    // Run those joint solves to the growth-phase tolerance rather than the
    // solver's full one (Mapper::jointRefine). A level's solve is followed by
    // more merges and another solve, so converging it tightly is work the next
    // level discards -- but the merge tests that run in between are measured in
    // pixels of reprojection error, so a half-converged model is also a model
    // the cross-seam test judges more harshly.
    bool coarse_joint_ba = true;
    // Levels between those joint solves. Every level costs one bundle
    // adjustment over the whole capture, and a tree over a thousand images is
    // seven to nine levels deep, so 2 halves that bill at the price of letting
    // a level align on seams one level older. The last level always solves --
    // it is the geometry the finishing passes and the caller see.
    //
    // 1 anyway. At 2, four of five medium captures were 5-56 s faster at the
    // same accuracy (windmill's tree solves 152 -> 93 s, vicon_room 31 -> 25 s)
    // and the fifth dropped 6 points -- but that fifth capture fragments into
    // six or seven components and swings 9.3 points between two runs of the
    // *same* configuration, so it cannot arbitrate this and nothing else can
    // outvote it. The flag is there for a capture that needs the time.
    int joint_every = 1;
    // Levels between growth passes over the models that did not merge; 0
    // disables in-level growth. `max_grow_passes` bounds the total so a large
    // tree cannot spend all its time registering.
    int grow_every = 1;
    int max_grow_passes = 8;
    // What one growth pass may add to a model, as a fraction of what it already
    // holds and never fewer than `grow_budget_min`. The pass is asked for a
    // bridge to its neighbour, not for a reconstruction: the overlap bound in
    // growByPnP limits only images another model holds, so without this a model
    // beside a large uncovered region grows into all of it.
    double grow_budget_frac = 0.25;
    size_t grow_budget_min = 25;
    // Merges on shared *structure* when a level runs out of shared images
    // (D70), per level. Each is a RANSAC over thousands of point
    // correspondences, so only the model pairs the correspondence graph joins
    // most strongly are worth trying, and only a few of them.
    //
    // **Off**, and the reason is not the alignment -- that works, and recovers
    // pairs no shared image could ever align. It is that turning it on breaks
    // the independence the acceptance tests rely on. An ordinary merge is
    // aligned on shared *poses* and then judged on *matches* that the alignment
    // never saw, which is what catches two places that merely look alike; a
    // merge aligned on matches is judged on the same evidence that produced it,
    // so a wrong one sails through. Measured on a 7620-image capture that is
    // half building interior, over seven runs: every run with this on scored
    // 17.0 to 25.7 AUC@10, every run without it 44.7 to 46.3, and the merges it
    // made added nine and sixty-five images. It needs an independent test
    // before it can be a default.
    int max_bridges = 0;
    size_t bridge_min_matches = 500;  // matched features joining two models
    // Seed attempts the finishing reseed pass may spend. Far below the mapper's
    // own budget: what is left after the levels is the tail of the view graph,
    // and failing three times there means there is nothing to find.
    int reseed_trials = 3;
    // Cap on what the audit's repair may grow a model to, as a fraction of what
    // it held. Uncapped, that repair is a full incremental growth pass at the
    // repair's cadence rather than the schedule's (see Mapper::audit) -- which
    // is what the manage loop did, and what turned its small models into copies
    // of its large ones.
    double audit_growth_frac = 0.25;
    bool verbose = true;
    const char* tag = "asm";  // log prefix, so a run says which mapper it came from
};

struct AssembleStats {
    size_t models_in = 0;
    size_t rounds = 0;
    size_t merges = 0, merges_refused = 0;
    size_t bridges = 0;            // ... of which came from shared structure alone
    size_t joint_ba = 0;
    size_t grown_images = 0;       // registered by a level's growth pass
    size_t grown_rejected = 0;     // ... and dropped again by the pose check
    ManagerStats finish;           // what the passes in ModelOps.h did
    double t_merge = 0, t_ba = 0, t_grow = 0;
    // The finishing passes, one by one: they answer different failures and cost
    // wildly different amounts, and a single total hides which one is worth its
    // price on a given capture.
    double t_audit = 0, t_split = 0, t_grow_tail = 0, t_reseed = 0, t_final_merge = 0, t_fold = 0;
    double finishSecs() const {
        return t_audit + t_split + t_grow_tail + t_reseed + t_final_merge + t_fold;
    }
};

namespace detail {

// One level: every model absorbs at most one other, best overlap first and
// smallest pair first, so the models grow *together*.
//
// The alternative -- MergeSession::runAuto, which chains until nothing else
// merges -- is both slower and worse here. Slower because every attempt copies
// the anchor (the merge has no cheaper inverse), and an anchor that absorbs a
// hundred atoms in turn is copied at every size it passes through: quadratic,
// where a matching copies the smaller half and doubles the model size per
// level. Worse because each link in the chain is a seam that has never been
// optimized, and the ones at the end of the chain are asked to align across all
// of them at once.
//
// Every `carry` array is reindexed with the surviving models, and `stalled` is
// filled with which of them got through the level without merging -- so the
// caller can tell "changed" from "has a new seam", which are different
// questions and want different treatment afterwards.
inline size_t mergeLevel(std::vector<Reconstruction>& models, const MergeOptions& opt,
                         std::vector<std::vector<char>*> carry, std::vector<char>& stalled,
                         size_t& refused, std::map<std::string, size_t>* why = nullptr) {
    MergeSession s(std::move(models), opt);
    std::vector<MergeCandidate> cands = s.candidates();
    std::stable_sort(cands.begin(), cands.end(),
                     [&s](const MergeCandidate& a, const MergeCandidate& b) {
                         if (a.common_images != b.common_images)
                             return a.common_images > b.common_images;
                         const uint32_t sa = s.model(a.dst).numRegistered() +
                                             s.model(a.src).numRegistered();
                         const uint32_t sb = s.model(b.dst).numRegistered() +
                                             s.model(b.src).numRegistered();
                         return sa < sb;
                     });
    std::vector<char> busy(s.numModels(), 0);
    size_t merges = 0;
    for (const MergeCandidate& c : cands) {
        if (busy[c.dst] || busy[c.src]) continue;
        // A refusal does not retire either model for the level: it may still
        // have a partner it can align with.
        const MergeAttempt a = s.tryMerge(c.dst, c.src);
        if (a.merged) {
            merges++;
            busy[c.dst] = busy[c.src] = 1;
        } else {
            refused++;
            // Reasons carry counts, which would make every one unique; keep the
            // leading words so a level's refusals group into kinds.
            if (why) {
                std::string k = a.reason.substr(0, a.reason.find_first_of("0123456789"));
                while (!k.empty() && (k.back() == ' ' || k.back() == '(')) k.pop_back();
                (*why)[k.empty() ? a.reason : k]++;
            }
        }
    }
    std::vector<std::vector<char>> was;
    for (const std::vector<char>* c : carry) was.push_back(*c);
    models.clear();
    stalled.clear();
    for (std::vector<char>* c : carry) c->clear();
    for (size_t i = 0; i < s.numModels(); i++) {
        if (!s.alive(i)) continue;
        stalled.push_back(busy[i] ? 0 : 1);  // survived the level without merging
        models.push_back(std::move(s.modelMut(i)));
        for (size_t k = 0; k < carry.size(); k++)
            carry[k]->push_back(i < was[k].size() ? was[k][i] : 0);
    }
    return merges;
}

// Register whatever each model can still take, largest first, by PnP alone.
//
// This is the incremental half of the hybrid, and it is what a stalled level
// needs. A level stops for one of two reasons and growth answers both: either
// too few pairs still share `min_common_images`, in which case the models are
// not wrong but simply do not touch, and only registering more images can make
// them touch; or a merge was refused on thin evidence, in which case more
// shared images give the alignment more to fit and the cross-seam test more to
// judge on -- so a correct merge that was refused gets another chance, and a
// wrong one is refused with more confidence.
//
// An image another model already holds is a legitimate target, and in fact the
// point: that overlap *is* what the next Sim(3) aligns on. `growByPnP` bounds
// it by Mapper::overlapBudget -- enough to determine a transform, and more than
// that while the pass is still finding ground of its own -- with the size cap
// below stopping it well short of absorbing a neighbour, and it checks every
// image it registered against the rest of the model before returning -- which
// is not optional. Unaudited, growth does not merely add bad poses, it makes
// the cross-seam test agree with the merges built on them (measured: 21 points
// of AUC@5, and the same whole-session displacement the flat mapper produces).
inline size_t growModels(Mapper& mapper, std::vector<Reconstruction>& models,
                         std::vector<char>& dirty, const std::vector<char>& which,
                         double budget_frac, size_t budget_min, size_t& rejected) {
    std::vector<size_t> order(models.size());
    std::iota(order.begin(), order.end(), (size_t)0);
    std::stable_sort(order.begin(), order.end(), [&models](size_t a, size_t b) {
        return models[a].numRegistered() > models[b].numRegistered();
    });
    size_t registered = 0;
    for (size_t i : order) {
        if (i < which.size() && !which[i]) continue;
        const uint32_t have = models[i].numRegistered();
        if (have < 2) continue;
        std::vector<const Reconstruction*> others;
        for (size_t j = 0; j < models.size(); j++)
            if (j != i) others.push_back(&models[j]);
        // Bridge, not reconstruct. The pass is asked for enough images to make
        // a merge possible, scaled to the model so a big one may reach further
        // than a small one, and it is stopped well before it could grow into a
        // whole uncovered region on its own -- which would be the flat mapper
        // again, at the flat mapper's cost.
        const uint32_t budget =
            (uint32_t)std::max((double)budget_min, budget_frac * (double)have);
        Mapper::GrowStats gs;
        uint32_t bad = 0;
        Reconstruction grown =
            mapper.growByPnP(models[i], &gs, others, have + budget, &bad);
        rejected += bad;
        if (!gs.registered) continue;
        registered += gs.registered;
        models[i] = std::move(grown);
        dirty[i] = 1;
    }
    return registered;
}

// Merge models the correspondence graph joins but no shared image does (D70).
//
// This is what a level cannot reach. `mergeLevel` only ever proposes pairs with
// `min_common_images` in common, so two models that see the same place from two
// passes -- and register none of the same frames -- are never candidates, and no
// amount of growth changes that when registration between them is what failed in
// the first place.
//
// Run only when a level merged nothing, and bounded to `max_bridges` attempts:
// each one is a RANSAC over thousands of point correspondences, and the pairs
// worth trying are the few with the most evidence. Everything after the
// transform is the ordinary merge, so the splice test, the fold test and the
// seam validator all still have their say.
inline size_t bridgeModels(Mapper& mapper, std::vector<Reconstruction>& models,
                           const MergeOptions& opt, size_t min_matches, int max_bridges,
                           std::vector<std::vector<char>*> carry, size_t& refused,
                           std::map<std::string, size_t>* why = nullptr) {
    if (max_bridges <= 0 || models.size() < 2) return 0;
    std::vector<Mapper::StructureLink> links = mapper.structureLinks(models, min_matches);
    if (links.empty()) return 0;
    if (opt.verbose)
        slog::diag(slog::Tag::Merge,
                   "[merge] %zu model pair(s) the correspondence graph joins, strongest "
                   "%zu matched features", links.size(), links.front().matches);

    MergeSession s(std::move(models), opt);
    std::vector<char> busy(s.numModels(), 0);
    size_t merges = 0;
    int tried = 0;
    for (const Mapper::StructureLink& l : links) {
        if (tried >= max_bridges) break;
        if (busy[l.a] || busy[l.b]) continue;
        // The bigger model keeps its gauge and its intrinsics, as everywhere.
        const bool a_first = s.model(l.a).numRegistered() >= s.model(l.b).numRegistered();
        const size_t dst = a_first ? l.a : l.b, src = a_first ? l.b : l.a;
        tried++;
        AlignmentResult al = mapper.alignByStructure(s.model(dst), s.model(src), opt);
        // Printed in full, not grouped: there are at most `max_bridges` of
        // these a level, and each one is the answer to "why are those two
        // models still apart".
        if (opt.verbose)
            slog::diag(slog::Tag::Merge,
                       "[merge] structure link %zu <- %zu (%zu matched features): %s", dst,
                       src, l.matches, al.success ? "aligned" : al.reason.c_str());
        if (!al.success) {
            refused++;
            if (why) (*why)["shared structure: " + al.reason.substr(0, al.reason.find_first_of(
                                                       "0123456789"))]++;
            continue;
        }
        const MergeAttempt a = s.tryMerge(dst, src, al);
        if (a.merged) {
            merges++;
            busy[dst] = busy[src] = 1;
        } else {
            refused++;
            if (why) {
                std::string k = a.reason.substr(0, a.reason.find_first_of("0123456789"));
                while (!k.empty() && (k.back() == ' ' || k.back() == '(')) k.pop_back();
                (*why)["shared structure: " + (k.empty() ? a.reason : k)]++;
            }
        }
    }
    std::vector<std::vector<char>> was;
    for (const std::vector<char>* c : carry) was.push_back(*c);
    models.clear();
    for (std::vector<char>* c : carry) c->clear();
    for (size_t i = 0; i < s.numModels(); i++) {
        if (!s.alive(i)) continue;
        models.push_back(std::move(s.modelMut(i)));
        for (size_t k = 0; k < carry.size(); k++)
            carry[k]->push_back(i < was[k].size() ? (busy[i] ? 1 : was[k][i]) : 0);
    }
    return merges;
}

}  // namespace detail

// The levels: merge, grow what did not merge, optimize, repeat.
//
// `dirty` marks models something changed and `seamed` the ones that absorbed
// another; both are sized to `models` on entry and reindexed with it, and the
// finishing passes read them to skip work that has already been done.
inline void mergeUpwards(Mapper& mapper, std::vector<Reconstruction>& models,
                         const MergeOptions& merge_opt, const ManagerOptions& mopt,
                         const AssembleOptions& opt, AssembleStats& st,
                         std::vector<char>& dirty, std::vector<char>& seamed) {
    auto clk = [] { return std::chrono::steady_clock::now(); };
    auto secs = [](auto a, auto b) { return std::chrono::duration<double>(b - a).count(); };

    dirty.resize(models.size(), 0);
    seamed.resize(models.size(), 0);
    std::vector<char> stalled(models.size(), 1);
    int grow_passes = 0;
    bool solve_pending = false;  // a level merged and its joint solve was deferred
    if (!mopt.do_merge && !mopt.do_grow) return;
    for (int round = 0; round < std::max(1, opt.max_rounds) && models.size() > 1; round++) {
        st.rounds = (size_t)round + 1;
        size_t refused = 0;
        std::map<std::string, size_t> why;
        auto t0 = clk();
        const size_t merges =
            mopt.do_merge
                ? detail::mergeLevel(models, merge_opt, {&dirty, &seamed}, stalled, refused, &why)
                : 0;
        for (size_t i = 0; i < models.size(); i++)
            if (!stalled[i]) dirty[i] = seamed[i] = 1;
        st.t_merge += secs(t0, clk());
        st.merges += merges;
        st.merges_refused += refused;
        if (opt.verbose) {
            slog::diag(slog::Tag::Merge,
                       "[%s] level %zu: %zu merge(s), %zu refused, %zu model(s) left",
                       opt.tag, st.rounds, merges, refused, models.size());
            // Hundreds of merges a level, so the reasons are summarized by kind
            // rather than printed one by one -- but they have to be visible, or
            // a level that refuses everything looks the same as one with
            // nothing left to merge.
            for (const auto& kv : why)
                slog::diag(slog::Tag::Merge, "[%s]   %4zu x %s", opt.tag, kv.second,
                           kv.first.c_str());
        }
        if (models.size() <= 1) break;

        // Grow the models that did *not* merge, before the joint refinement.
        // Models that just merged are left alone -- they have a fresh seam to
        // settle and already have work at the next level.
        size_t reg = 0;
        if (mopt.do_grow && opt.grow_every > 0 && grow_passes < opt.max_grow_passes &&
            (round % opt.grow_every) == 0) {
            size_t want = 0;
            for (char c : stalled) want += c ? 1 : 0;
            if (want) {
                t0 = clk();
                size_t rejected = 0;
                reg = detail::growModels(mapper, models, dirty, stalled, opt.grow_budget_frac,
                                         opt.grow_budget_min, rejected);
                st.grown_rejected += rejected;
                st.t_grow += secs(t0, clk());
                st.grown_images += reg;
                if (reg) grow_passes++;
                if (opt.verbose)
                    slog::diag(slog::Tag::Merge,
                               "[%s]   growth: %zu image(s) into %zu of %zu model(s) that "
                               "did not merge (%zu rejected by the pose check)", opt.tag, reg, want,
                               models.size(), st.grown_rejected);
            }
        }
        // A level that merged nothing has exhausted what shared images can do.
        // What the correspondence graph can still say is a different question,
        // and one growth cannot answer: two models that see the same place from
        // two passes share no frame, so no amount of registering brings them
        // within `min_common_images` of each other (D70).
        size_t bridged = 0;
        if (merges == 0 && mopt.do_merge && opt.max_bridges > 0) {
            t0 = clk();
            std::map<std::string, size_t> bwhy;
            bridged = detail::bridgeModels(mapper, models, merge_opt, opt.bridge_min_matches,
                                           opt.max_bridges, {&dirty, &seamed, &stalled}, refused,
                                           &bwhy);
            st.t_merge += secs(t0, clk());
            st.merges += bridged;
            st.bridges += bridged;
            if (opt.verbose && (bridged || !bwhy.empty())) {
                slog::diag(slog::Tag::Merge, "[%s]   %zu merge(s) on shared structure alone",
                           opt.tag,
                           bridged);
                for (const auto& kv : bwhy)
                    slog::diag(slog::Tag::Merge, "[%s]   %4zu x %s", opt.tag, kv.second,
                               kv.first.c_str());
            }
        }

        // Nothing merged, and what grew cannot change that. A level costs a
        // joint solve over the whole capture, and it is only worth paying when
        // the growth before it plausibly gave some pair enough new overlap to
        // align on -- one model's minimum budget is the least that could.
        // Measured on a 7620-image capture: levels 5 to 9 merged nothing while
        // growth trickled 13, 5, 7 and 0 images, and each still solved.
        if (merges == 0 && bridged == 0 && reg < opt.grow_budget_min) break;

        // Growth aims models at images their neighbours hold, so it is growth,
        // not merging, that turns a small model into a copy of a bigger one.
        // Dropping those here rather than at the end keeps them out of the next
        // level's candidate list and out of the next joint solve.
        models = dropRedundantModels(std::move(models), mopt, st.finish,
                                     {&dirty, &seamed, &stalled});

        // One solve over every live model, intrinsics shared per camera group.
        //
        // Unconditionally, even once the models agree about the intrinsics and
        // the manage loop's skip rule (focalSpreadPx) would fire. That rule is
        // right for a loop whose alternative is doing nothing; here the
        // alternative is one refine per changed model, and measured on four
        // captures that was consistently slower than the single joint solve it
        // replaced. A bundle adjustment's cost on this device is dominated by
        // submits rather than by arithmetic, so N small solves beat one large
        // one at no size that occurs here.
        //
        // Not necessarily at every level: `joint_every` lets a level align on
        // seams the level below left unsettled. Once two models are left the
        // next level ends it, so from there it always solves, and an exit on a
        // skipped level solves on the way out.
        const bool solve_now =
            opt.joint_every <= 1 || models.size() <= 2 || (round % opt.joint_every) == 0;
        if (!solve_now) {
            solve_pending = true;
            continue;
        }
        solve_pending = false;
        t0 = clk();
        if (opt.joint_intrinsics && models.size() > 1) {
            mapper.jointRefine(models, opt.coarse_joint_ba);
            st.joint_ba++;
        } else {
            for (size_t i = 0; i < models.size(); i++)
                if (dirty[i] && models[i].numRegistered() >= 2)
                    models[i] = mapper.refine(models[i]);
        }
        st.t_ba += secs(t0, clk());
    }
    if (solve_pending && models.size() > 1) {
        auto t0 = clk();
        mapper.jointRefine(models, opt.coarse_joint_ba);
        st.joint_ba++;
        st.t_ba += secs(t0, clk());
    }

    if (opt.verbose) {
        const ManagerStats& f = st.finish;
        slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_assembled,
                 {slog::num(st.t_merge + st.t_grow + st.t_ba, 2), (long long)st.models_in,
                  (long long)models.size(), (long long)st.rounds, (long long)st.merges,
                  (long long)st.merges_refused, (long long)st.grown_images,
                  (long long)f.covered_before, (long long)coveredImages(models).size()});
        // The seam test's own numbers stay English: they are a diagnostic for
        // whoever is calibrating the merge validator, not for the person
        // waiting on a reconstruction (sfm/core/Log.h).
        // A refusal over a handful of cross-seam pairs is a seam with nothing
        // on it, and no refinement can rescue that -- only more overlap can.
        // One over hundreds that still explains a third of them is a seam that
        // is merely out of true. The two want opposite responses, so the report
        // has to separate them.
        if (f.seam_refused)
            slog::diag(slog::Tag::Merge,
                       "[%s]   a refused merge explained a median %.0f%% of its cross-seam "
                       "matches over %zu pair(s), an accepted one %.0f%%, against a %.0f%% bar "
                       "(the same pairs inside the model: %.0f%%); the rescue's refinement "
                       "moved a refusal by %+.0f points", opt.tag,
                       100.0 * f.seam_refused_median / (double)f.seam_refused,
                       f.seam_refused_pairs / f.seam_refused,
                       f.seam_passed ? 100.0 * f.seam_passed_median / (double)f.seam_passed : 0.0,
                       f.seam_checked ? 100.0 * f.seam_bar_sum / (double)f.seam_checked : 0.0,
                       f.seam_checked ? 100.0 * f.seam_reference_sum / (double)f.seam_checked : 0.0,
                       f.seam_rescue_failed
                       ? 100.0 * f.seam_rescue_gain / (double)f.seam_rescue_failed : 0.0);
    }
}

// What the levels cannot do by construction.
//
// Once, in this order, and never in a loop. Each answers a failure merging has
// no move against, and each is a pass from ModelOps.h with the same thresholds
// the merging used.
inline std::vector<Reconstruction> finishModels(Mapper& mapper,
                                                std::vector<Reconstruction> models,
                                                const MergeOptions& merge_opt,
                                                const ManagerOptions& mopt,
                                                const AssembleOptions& opt, AssembleStats& st,
                                                ModelMemo& memo, const std::vector<char>& dirty,
                                                const std::vector<char>& seamed) {
    auto clk = [] { return std::chrono::steady_clock::now(); };
    auto secs = [](auto a, auto b) { return std::chrono::duration<double>(b - a).count(); };
    ManagerOptions fopt = mopt;
    fopt.audit_growth_frac = opt.audit_growth_frac;

    // The joint pass optimizes but does not filter: it is one bundle
    // adjustment, with none of the mapper's retriangulation, track completion
    // or de-registration, and a single similarity cannot place both halves of a
    // drifted model correctly. So every model with a new seam is audited
    // against the correspondence graph and then refined -- which is what the
    // audit ends with anyway, so this replaces the refine rather than adding to
    // it. A model that merely grew was checked image by image as it grew
    // (growByPnP rejects a registration the rest of the model contradicts), and
    // one that did neither was refined by the run that built it -- so what is
    // left for the audit is the seam, which is what it is for. Those two still
    // want the refine the audit ends with.
    for (size_t i = 0; i < models.size(); i++) {
        if (i < seamed.size() && seamed[i]) continue;
        if (i < dirty.size() && dirty[i] && models[i].numRegistered() >= 2)
            models[i] = mapper.refine(models[i]);
        memo.audited.insert(ModelMemo::of(models[i]));
    }
    auto t0 = clk();
    models = auditModels(mapper, std::move(models), fopt, memo, st.finish);
    st.t_audit = secs(t0, clk());

    // A model whose own verified pairs contradict it. Merging refuses a bad
    // result before it commits, but a model can be wrong on its own -- a chain
    // of registrations through repeated structure does the same damage -- and
    // growth builds on whatever it was given.
    size_t changed = 0;
    if (mopt.do_split) {
        t0 = clk();
        const size_t before = models.size();
        models = splitInconsistentModels(mapper, std::move(models), fopt, memo, st.finish);
        changed += models.size() - std::min(models.size(), before);
        st.t_split = secs(t0, clk());
    }

    // Images no model holds, offered to the models that might take them.
    //
    // In-level growth only runs on models that failed to *merge*, and it stops
    // entirely once one model is left -- so a capture that comes back in one
    // piece never gets a growth pass at all, and whatever was left at the
    // boundaries stays out. That tail is worth more than it looks: on a
    // 550-image capture the difference between a 2.4x-cover partition and a
    // 1.5x one was eight images, and eight images is 2.6 points of AUC@10
    // because every pair involving them counts as a failure. Their *poses* were
    // never the problem -- median position error is 0.05 % of scene extent
    // either way.
    //
    // With the mapper's own schedule, not growByPnP: this is the last pass that
    // will register anything, there is no joint solve after it to pay for the
    // geometry, and by here there are a handful of images to find rather than a
    // bridge to build. Bounded by what is actually missing, so it cannot become
    // the incremental mapper again.
    if (mopt.do_grow) {
        t0 = clk();
        mapper.claimAll(models);
        const size_t missing = mapper.unclaimed();
        if (missing) {
            size_t reg = 0;
            for (size_t i = 0; i < models.size(); i++) {
                if (models[i].numRegistered() < 2) continue;
                std::vector<const Reconstruction*> others;
                for (size_t j = 0; j < models.size(); j++)
                    if (j != i) others.push_back(&models[j]);
                Mapper::GrowStats gs;
                Reconstruction grown = mapper.continueFrom(
                    models[i], &gs, others, (uint32_t)(models[i].numRegistered() + missing));
                if (!gs.registered) continue;
                reg += gs.registered;
                models[i] = std::move(grown);
                mapper.claimAll(models);
            }
            st.grown_images += reg;
            // Registration is the one operation that creates overlap, and
            // overlap is what a merge aligns on -- so this pass can make a
            // merge possible that was not before. On a 5356-image capture it
            // is *the* thing that does: the two components that came out of
            // the tree only came to share images (1291 of them) once this had
            // run, and without counting it the merge below never ran at all.
            if (reg) changed++;
            if (opt.verbose)
                slog::diag(slog::Tag::Merge,
                           "[%s] tail growth: %zu of %zu uncovered image(s) registered",
                           opt.tag, reg, missing);
        }
        st.t_grow_tail = secs(t0, clk());
    }

    // Images nothing reached: a component the partition cut too thin to seed,
    // or one the whole capture only connects to weakly. This is the one
    // finishing pass that can still find a model rather than repair one.
    // Bounded twice, because unbounded it is the manage loop's reseed pass with
    // none of the manage loop's reasons: what is left here is barely connected
    // to the view graph, and a handful of such images cannot make a model worth
    // keeping. Measured on a 1146-image capture, an unbounded pass spent 21.6 s
    // to find nothing.
    if (mopt.do_reseed) {
        t0 = clk();
        mapper.claimAll(models);
        if (mapper.unclaimed() >= (size_t)std::max(2, mapper.options().min_model_size)) {
            const size_t before = models.size();
            const int saved_trials = mapper.options().max_model_trials;
            mapper.options().max_model_trials = opt.reseed_trials;
            mapper.seedFurtherModels(models, true);
            mapper.options().max_model_trials = saved_trials;
            st.finish.reseeded_models += models.size() - before;
            changed += models.size() - before;
            if (opt.verbose && models.size() != before)
                slog::diag(slog::Tag::Merge,
                           "[%s] reseeded %zu model(s) among the images nothing reached",
                           opt.tag, models.size() - before);
        }
        st.t_reseed = secs(t0, clk());
    }

    // Anything the last two passes produced has never been offered a merge.
    if (changed && models.size() > 1 && mopt.do_merge) {
        t0 = clk();
        std::vector<char> s2(models.size(), 1);
        size_t refused = 0;
        const size_t merges = detail::mergeLevel(models, merge_opt, {}, s2, refused);
        st.merges += merges;
        st.merges_refused += refused;
        if (opt.verbose)
            slog::diag(slog::Tag::Merge,
                       "[%s] final level: %zu merge(s), %zu refused, %zu model(s) left",
                       opt.tag, merges, refused, models.size());
        if (merges)
            for (size_t i = 0; i < models.size(); i++)
                if (!s2[i] && models[i].numRegistered() >= 2)
                    models[i] = mapper.refine(models[i]);
        st.t_final_merge = secs(t0, clk());
    }

    models = dropRedundantModels(std::move(models), mopt, st.finish);

    // Folds are cut last, once. Two similar-looking parts of the capture
    // written on top of each other is the failure every agreement test misses,
    // because a fold agrees with itself; it is detected from what the model is
    // *missing* (D46).
    if (mopt.do_duplicate_split) {
        t0 = clk();
        const size_t before = models.size();
        models = splitFoldedModels(mapper, std::move(models), fopt, memo, st.finish);
        if (models.size() != before)
            for (Reconstruction& m : models)
                if (m.numRegistered() >= 2 && memo.audited.insert(ModelMemo::of(m)).second)
                    m = mapper.refine(m);
        st.t_fold = secs(t0, clk());
    }

    sortModels(models);
    st.finish.models_after = models.size();
    st.finish.covered_after = coveredImages(models).size();
    if (opt.verbose)
        slog::out(slog::Tag::Map, spirula::i18n::msg::sfm::map_finishing,
                 {slog::num(st.finishSecs(), 1), (long long)st.finish.splits,
                  (long long)st.finish.duplicate_splits,
                  (long long)st.finish.reseeded_models,
                  (long long)st.finish.dropped_redundant,
                  (long long)st.finish.audited_repaired,
                  (long long)st.finish.audited_out});
    mapper.claimAll(models);
    return models;
}

// Levels, then the finishing passes: everything both mappers do after they have
// models. `mopt` supplies the merge and cleanup thresholds -- there is one set
// of them and this is the same set.
inline std::vector<Reconstruction> assembleModels(Mapper& mapper,
                                                  std::vector<Reconstruction> models,
                                                  const ManagerOptions& mopt,
                                                  const AssembleOptions& opt, AssembleStats& st) {
    st.models_in = models.size();
    st.finish.models_before = models.size();
    st.finish.covered_before = coveredImages(models).size();

    // Before anything expensive touches them. The flat mapper's seed retries
    // and its sub-model search can both hand over a model a bigger one already
    // contains (D19, D41), and this is set arithmetic -- without it the first
    // level bundle-adjusts, splits, merges, audits and grows every copy.
    models = dropRedundantModels(std::move(models), mopt, st.finish);

    // The only test that catches repeated structure is the one the merger
    // cannot run by itself (D45): without it a capture with two similar rooms
    // merges one onto the other and every internal test agrees, because a fold
    // agrees with itself.
    MergeOptions merge_opt = mopt.merge;
    merge_opt.duplicate = mopt.duplicate;
    merge_opt.validate = seamValidator(mapper, mopt, &st.finish);
    merge_opt.rigs = mapper.rigs();

    // A component that fitted its own focal to its own noise cannot align with
    // anything, and on a rig every component is looking at the same physical
    // cameras. Before the first merge, because the tests that accept one are
    // measured in pixels. Two steps: a focal that has run away needs replacing
    // rather than averaging, and only then is one shared solve meaningful.
    if (mopt.focal_consensus_tol > 0) refitOutlierCameras(mapper, models, mopt, st.finish);
    if (opt.joint_intrinsics && models.size() > 1 && focalSpreadPx(models) > kJointBaMovedPx) {
        const auto t0 = std::chrono::steady_clock::now();
        mapper.jointRefine(models, opt.coarse_joint_ba);
        st.joint_ba++;
        st.t_ba += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (opt.verbose)
            slog::diag(slog::Tag::Merge,
                       "[%s] the components disagreed about the intrinsics: one joint "
                       "refinement before the first level", opt.tag);
    }

    std::vector<char> dirty(models.size(), 0), seamed(models.size(), 0);
    mergeUpwards(mapper, models, merge_opt, mopt, opt, st, dirty, seamed);
    ModelMemo memo;
    return finishModels(mapper, std::move(models), merge_opt, mopt, opt, st, memo, dirty, seamed);
}

}  // namespace sfm
