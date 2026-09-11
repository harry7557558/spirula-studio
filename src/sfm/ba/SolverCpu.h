// Bundle adjustment on the host, for a device that cannot run the fp64 kernels.
// Same problem, same LM loop and the same two linear solvers as sfm/ba/Solver.h;
// only where the arithmetic happens changes. Parameters live in the BAProblem's
// own vectors, so there is nothing to upload or read back.
//
// The parallel decomposition and what it rests on: README.md, "Host fallback".
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "sfm/ba/CpuCamera.h"
#include "sfm/ba/CpuDense.h"
#include "sfm/ba/CpuParallel.h"
#include "sfm/ba/Options.h"
#include "sfm/ba/Problem.h"
#include "core/Env.h"
#include "sfm/core/HostMemory.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Log.h"

namespace bacpu {

class Solver {
    static constexpr uint32_t kCamBlk = kMaxCamDof * (kMaxCamDof + 1) / 2;
    static constexpr uint32_t kDenseMaxDim = 8192;
    static constexpr uint32_t kNoSlot = 0xFFFFFFFFu;
    static uint32_t pidx(uint32_t r, uint32_t c) { return r * (r + 1) / 2 + c; }

public:
    Solver(BAProblem& P, const SolverOptions& opt) : P_(P), opt_(opt) {}

    void init() {
        n_ = P_.n_dim;
        poseDim_ = P_.pose_dim;
        nImg_ = P_.num_images;
        nPts_ = P_.num_points;
        nObs_ = P_.num_obs;
        pool_ = &Pool::get();
        nthreads_ = opt_.threads > 0 ? std::min(opt_.threads, pool_->size()) : pool_->size();
        lossParam_ = opt_.loss_param;

        buildImageTables();
        decidePaths();
        allocate();

        stats_.vram_mb = allocatedMB();
        stats_.solver = useCG_ ? (haveFallback_ ? "cg+fallback" : "cg") : "dense";
        if (opt_.verbose)
            sfm::slog::diag(sfm::slog::Tag::Map,
                            "[cpu] n_dim = %u, solver = %s, threads = %d, RAM = %.1f MB",
                       n_,
                       stats_.solver, nthreads_, stats_.vram_mb);
    }

    double computeCost() {
        double total = 0;
        const int nt = taskCount(nObs_, 1 << 14, nthreads_);
        part_.assign(nt, 0.0);
        withLoss(opt_.loss, [&](auto L) {
            using LT = decltype(L);
            pool_->run(nt, nthreads_, [&](int t, int) {
                int64_t lo, hi;
                taskRange(nObs_, nt, t, lo, hi);
                double c = 0;
                for (int64_t o = lo; o < hi; o++) {
                    const uint32_t img = P_.obs_image[o], pt = P_.obs_point[o];
                    withModel(model_[img], [&](auto M) {
                        double r[2];
                        const double* pose = &P_.poses[6 * (size_t)frame_[img]];
                        if (eoff_[img] == kNoSlot)
                            residual<decltype(M)>(pose, &P_.intr[ioff_[img]],
                                                  &P_.points[3 * (size_t)pt], &P_.obs_xy[2 * o], r);
                        else
                            residualRig<decltype(M)>(pose, &P_.exts[eoff_[img]], &P_.intr[ioff_[img]],
                                                     &P_.points[3 * (size_t)pt], &P_.obs_xy[2 * o],
                                                     r);
                        c += 0.5 * LT::cost(r[0] * r[0] + r[1] * r[1], lossParam_);
                    });
                }
                part_[t] = c;
            });
        });
        for (double v : part_) total += v;
        return total;
    }

    void solve() {
        auto t0 = std::chrono::high_resolution_clock::now();
        double damping = opt_.init_damping;
        double cost = computeCost();
        stats_.initial_cost = cost;
        int noimprov = 0;

        bool reuse = false;  // after a reject, the assembly still matches the params
        double reject_mult = 2.0;
        int consec_fallbacks = 0;
        for (int it = 0; it < opt_.max_iters; it++) {
            sfm::cancel::check();
            if (opt_.verbose)
                sfm::slog::diag(sfm::slog::Tag::Map, "iter %3d: cost = %.9e, damping = %.3g%s", it,
                                cost,
                           damping,
                           reuse ? " (reuse)" : "");
            const bool cg = useCG_;
            double newCost = iterate(damping, reuse, cg);
            stats_.iterations = it + 1;

            if (cg) {
                stats_.cg_solves++;
                stats_.cg_iters_total += cgIters_;
                if (cgConverged_) {
                    consec_fallbacks = 0;
                    cgMaxit_ = std::min<uint32_t>(
                        std::max<uint32_t>((uint32_t)(1.5 * cgIters_) + 8, 16),
                        (uint32_t)opt_.cg_max_iters);
                } else {
                    const uint32_t usedCap = cgMaxit_;
                    cgMaxit_ = (uint32_t)opt_.cg_max_iters;
                    // a truncated-CG step is still a damped descent step
                    const bool stepOk = std::isfinite(newCost) && newCost <= cost * (1.0 + opt_.rtol);
                    if (stepOk) consec_fallbacks = 0;
                    if (haveFallback_ && !stepOk) {
                        if (opt_.verbose)
                            sfm::slog::diag(sfm::slog::Tag::Map,
                                       "iter %3d: CG hit %u-iteration cap, dense fallback",
                                       it, usedCap);
                        restore();
                        newCost = iterate(damping, true, false);
                        stats_.cg_fallbacks++;
                        if (++consec_fallbacks >= 3) {
                            useCG_ = false;
                            stats_.solver = "cg->dense";
                            if (opt_.verbose)
                                sfm::slog::diag(sfm::slog::Tag::Map,
                                           "[cpu] repeated CG stalls, switching to dense");
                        }
                    }
                }
            }

            if (std::isfinite(newCost) && newCost <= cost * (1.0 + opt_.rtol)) {
                if (newCost / cost >= 1.0 - opt_.rtol) {
                    if (++noimprov >= opt_.patience) { cost = newCost; break; }
                } else {
                    noimprov = 0;
                    damping *= 1.0 / 3.0;
                }
                cost = newCost;
                stats_.accepted++;
                reuse = false;
                reject_mult = 2.0;
            } else {
                restore();
                if (!std::isfinite(newCost)) {
                    if (++noimprov >= opt_.patience) break;
                } else
                    noimprov = 0;
                damping *= reject_mult;
                reject_mult = std::min(reject_mult * 2.0, 32.0);
                reuse = true;
            }
        }
        stats_.final_cost = cost;
        stats_.solve_seconds =
            std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
        if (spirula::env("SFM_MAP_PROF"))
            sfm::slog::diag(sfm::slog::Tag::Map,
                            "[prof]   cpu ba: jac %.3f prep %.3f schur %.3f linear %.3f "
                       "back %.3f cost %.3f s",
                       prof_.jac, prof_.prep, prof_.schur, prof_.lin, prof_.back, prof_.cost);
    }

    const SolverStats& stats() const { return stats_; }

    // ---- debug hooks, mirroring BundleSolver's ----

    void assembleOnly(double damping) {
        jacobianPass();
        pointPrep(damping);
        schurAssemble(damping);
    }
    std::vector<double> packedS() const { return S_.data(); }
    std::vector<double> gradient() const { return g_; }

    double compareStep(double damping) {
        if (!useCG_ || !haveFallback_)
            throw std::runtime_error("step comparison needs --solver cg + fallback on");
        jacobianPass();
        pointPrep(damping);
        cgCamDiag(damping);
        cgPrecFactor();
        cgIters_ = runPCG((uint32_t)opt_.cg_max_iters);
        std::vector<double> xcg = g_;
        schurAssemble(damping);
        S_.factorSolve(g_.data(), *pool_, nthreads_);
        double dmax = 0, xmax = 0;
        for (uint32_t i = 0; i < n_; i++) {
            dmax = std::max(dmax, std::fabs(xcg[i] - g_[i]));
            xmax = std::max(xmax, std::fabs(g_[i]));
        }
        const double rel = dmax / std::max(xmax, 1e-300);
        sfm::slog::diag(sfm::slog::Tag::Map,
                   "cmp-step lambda=%g: cg %s in %u iters, |dx_cg - dx_dense|_inf/|dx|_inf = %.3e",
                   damping, cgConverged_ ? "converged" : "hit cap", cgIters_, rel);
        return rel;
    }

private:
    // ================
    // setup
    // ================

    void buildImageTables() {
        dof_.resize(nImg_);
        gz_.resize(nImg_);
        icol_.resize(nImg_);
        ioff_.resize(nImg_);
        model_.resize(nImg_);
        frame_.resize(nImg_);
        eoff_.resize(nImg_);
        efree_.resize(nImg_);
        std::vector<uint32_t> guse(P_.groups.size(), 0);
        for (uint32_t i = 0; i < nImg_; i++) guse[P_.image_group[i]]++;
        for (uint32_t i = 0; i < nImg_; i++) {
            const BAProblem::Group& g = P_.groups[P_.image_group[i]];
            if (g.model >= (uint32_t)kNumModels)
                throw std::runtime_error("camera model index outside the registry");
            const uint32_t m = P_.image_member[i];
            frame_[i] = P_.image_frame[i];
            eoff_[i] = m == kNoMember ? kNoSlot : P_.members[m].ext_offset;
            efree_[i] = (uint8_t)(m == kNoMember ? 0 : P_.members[m].n_free);
            dof_[i] = (uint8_t)(6 + efree_[i] + g.n_intr);
            gz_[i] = (uint8_t)g.n_intr;
            icol_[i] = g.intr_col;
            ioff_[i] = g.intr_offset;
            model_[i] = (uint8_t)g.model;
        }
        exclusive_ = exclusiveGroups(P_);
        tail_ = n_ - poseDim_;

        // Rows an image-per-task assembly cannot own outright: a member's
        // extrinsic columns (every frame of the rig touches them) and the
        // columns of an intrinsics group that more than one image refines.
        srow_.assign(tail_, -1);
        sharedCol_.clear();
        for (const BAProblem::Member& m : P_.members)
            for (uint32_t j = 0; j < m.n_free; j++) {
                srow_[m.ext_col - poseDim_ + j] = (int)sharedCol_.size();
                sharedCol_.push_back(m.ext_col + j);
            }
        for (size_t g = 0; g < P_.groups.size(); g++) {
            const BAProblem::Group& gr = P_.groups[g];
            if (guse[g] < 2 || gr.n_intr == 0) continue;
            for (uint32_t j = 0; j < gr.n_intr; j++) {
                srow_[gr.intr_col - poseDim_ + j] = (int)sharedCol_.size();
                sharedCol_.push_back(gr.intr_col + j);
            }
        }
        m_ = (uint32_t)sharedCol_.size();
        // Preconditioner blocks of the shared partition that several tasks
        // feed: every member's and every group's (buildPrecBlocks' numbering).
        nSharedBlk_ = exclusive_ ? 0 : (uint32_t)(P_.members.size() + P_.groups.size());
    }

    static double defaultBudgetMB() {
        const size_t ram = sfm::physicalRamBytes();
        // Half the machine, not nine tenths of it: unlike a GPU heap this is
        // shared with the rest of the pipeline (features, matches, the
        // reconstruction) and with the page cache.
        return ram ? 0.5 * (double)ram / (1024.0 * 1024.0) : 4096.0;
    }

    double estimateMB(bool withDense, bool withCG) const {
        const double no = (double)nObs_, np = (double)nPts_, ni = (double)nImg_, n = (double)n_;
        double b = 0;
        b += ((double)P_.jc_total + 8 * no) * 8;              // Jc, Jp, res
        b += (9 + 9 + 3 + 3) * np * 8;                        // App, W, Bp, Bp0
        b += (P_.pose_dim + P_.exts.size() + P_.total_intr + 3 * np) * 8;  // parameter backups
        b += 4 * no + 4 * (ni + 1) + 12 * (no / 1024 + ni);   // obs-by-image CSR + chunks
        b += n * 8;                                           // g
        if (withDense) {
            b += (double)DenseSpd::elems(n_) * 8 + 2.0 * n * DenseSpd::kBlock * 8;
            b += (double)nthreads_ * m_ * n * 8;
        }
        if (withCG) {
            const double nblk = exclusive_ ? ni : (double)P_.num_frames + nSharedBlk_;
            b += (4 * n + 3 * np) * 8 + (double)kCamBlk * (ni + nblk) * 8 +
                 16.0 * (ni + (double)P_.groups.size()) +
                 (double)nthreads_ * (2.0 * tail_ + nSharedBlk_ * (double)kCamBlk) * 8;
        }
        return b / (1024.0 * 1024.0);
    }

    void decidePaths() {
        const double budget =
            opt_.vram_budget_mb > 0 ? opt_.vram_budget_mb : defaultBudgetMB();
        const bool cgOk = nObs_ > 0;
        const double denseMB = estimateMB(true, false);
        const double cgMB = estimateMB(false, true);
        const double bothMB = estimateMB(true, true);

        switch (opt_.solver) {
            case SolverSel::Dense: useCG_ = false; break;
            case SolverSel::CG:
                useCG_ = cgOk;
                if (!cgOk) sfm::slog::diag(sfm::slog::Tag::Map,
                                      "[cpu] warning: no observations, falling back to dense");
                break;
            case SolverSel::Auto:
                useCG_ = cgOk && (n_ > kDenseMaxDim || denseMB > budget);
                break;
        }
        haveFallback_ = false;
        if (useCG_)
            haveFallback_ = opt_.cg_fallback == CgFallback::On ||
                            (opt_.cg_fallback == CgFallback::Auto && bothMB <= 0.5 * budget);

        if (opt_.verbose)
            sfm::slog::diag(sfm::slog::Tag::Map,
                       "[cpu] RAM estimates: dense %.0f MB, cg %.0f MB (budget %.0f MB)",
                       denseMB, cgMB, budget);
        const double needMB = (useCG_ ? cgMB : denseMB) + (haveFallback_ ? denseMB : 0);
        if (needMB > budget) {
            if (opt_.over_budget_throws) throw BAOverBudget(needMB, budget);
            sfm::slog::diag(sfm::slog::Tag::Map,
                       "[cpu] warning: the %s solver needs ~%.0f MB and the budget is %.0f MB",
                       useCG_ ? "cg" : "dense", needMB, budget);
        }

        P_.use_pair_schur = false;  // the pair tables are a GPU-only accelerator
        buildCamTables(P_);         // the dense path walks the same per-image lists
        if (useCG_) buildPrecBlocks(P_, exclusive_);
        cgMaxit_ = (uint32_t)opt_.cg_max_iters;
    }

    void allocate() {
        Jc_.assign(P_.jc_total, 0.0);
        Jp_.assign(6 * (size_t)nObs_, 0.0);
        res_.assign(2 * (size_t)nObs_, 0.0);
        App_.assign(9 * (size_t)nPts_, 0.0);
        W_.assign(9 * (size_t)nPts_, 0.0);
        Bp_.assign(3 * (size_t)nPts_, 0.0);
        Bp0_.assign(3 * (size_t)nPts_, 0.0);
        g_.assign(n_, 0.0);
        poses0_ = P_.poses;
        exts0_ = P_.exts;
        intr0_ = P_.intr;
        points0_ = P_.points;

        // Work split: by Schur entry count for the dense assembly (a track's
        // contribution is quadratic in its length, so observation counts alone
        // balance it badly), by observation count for the per-camera CG passes.
        std::vector<uint64_t> ew(nImg_ + 1, 0), ow(nImg_ + 1, 0);
        for (uint32_t a = 0; a < nImg_; a++) {
            uint64_t e = 0;
            for (uint32_t t = P_.cam_obs_ranges[a]; t < P_.cam_obs_ranges[a + 1]; t++) {
                const uint32_t o = P_.cam_obs[t];
                e += o - P_.obs_ranges[P_.obs_point[o]] + 1;
            }
            ew[a + 1] = ew[a] + e;
            ow[a + 1] = ow[a] + (P_.cam_obs_ranges[a + 1] - P_.cam_obs_ranges[a]);
        }
        splitByWeight(ew, taskCount((int64_t)ew[nImg_], 1 << 14, nthreads_), asmSplit_);
        splitByWeight(ow, taskCount((int64_t)ow[nImg_], 1 << 13, nthreads_), cgSplit_);
        // A task owns whole frames: the rows of a frame's pose are written by
        // every image in it, so a split inside one would race.
        snapToFrames(asmSplit_);
        snapToFrames(cgSplit_);
        const int nAsm = (int)asmSplit_.size() - 1, nCg = (int)cgSplit_.size() - 1;

        if (!useCG_ || haveFallback_) {
            S_.init(n_);
            if (m_) {
                sbuf_.assign((size_t)nAsm * m_ * n_, 0.0);
                sgbuf_.assign((size_t)nAsm * m_, 0.0);
            }
        }
        if (useCG_) {
            cgR_.assign(n_, 0.0);
            cgZ_.assign(n_, 0.0);
            cgP_.assign(n_, 0.0);
            cgSp_.assign(n_, 0.0);
            cgV_.assign(3 * (size_t)nPts_, 0.0);
            cgB_.assign((size_t)kCamBlk * nImg_, 0.0);
            cgM_.assign((size_t)kCamBlk * P_.num_prec_blocks, 0.0);
            if (!exclusive_) {
                cgGIntr_.assign((size_t)nCg * tail_, 0.0);
                cgSpIntr_.assign((size_t)nCg * tail_, 0.0);
                cgGrp_.assign((size_t)nCg * nSharedBlk_ * kCamBlk, 0.0);
            }
        }
    }

    double allocatedMB() const {
        size_t b = (Jc_.capacity() + Jp_.capacity() + res_.capacity() + App_.capacity() +
                    W_.capacity() + Bp_.capacity() + Bp0_.capacity() + g_.capacity() +
                    poses0_.capacity() + exts0_.capacity() + intr0_.capacity() +
                    points0_.capacity() +
                    sbuf_.capacity() + sgbuf_.capacity() + cgR_.capacity() + cgZ_.capacity() +
                    cgP_.capacity() + cgSp_.capacity() + cgV_.capacity() + cgB_.capacity() +
                    cgM_.capacity() + cgGIntr_.capacity() + cgSpIntr_.capacity() +
                    cgGrp_.capacity()) *
                   8;
        b += S_.bytes() + (P_.cam_obs.capacity() + P_.cam_obs_ranges.capacity() +
                           P_.cam_chunks.capacity() + P_.prec_blocks.capacity()) * 4;
        return (double)b / (1024.0 * 1024.0);
    }

    static void splitByWeight(const std::vector<uint64_t>& pre, int ntasks,
                              std::vector<uint32_t>& out) {
        const uint32_t n = (uint32_t)pre.size() - 1;
        out.assign(1, 0);
        for (int t = 1; t < ntasks; t++) {
            const uint64_t target = pre[n] * (uint64_t)t / (uint64_t)ntasks;
            uint32_t i = (uint32_t)(std::lower_bound(pre.begin(), pre.end(), target) - pre.begin());
            out.push_back(std::max(i, out.back()));
        }
        out.push_back(n);
        for (size_t i = 1; i < out.size(); i++) out[i] = std::max(out[i], out[i - 1]);
    }

    // Move each boundary of an image split up to the start of its frame.
    void snapToFrames(std::vector<uint32_t>& split) const {
        if (!P_.hasRigs()) return;
        for (size_t k = 1; k + 1 < split.size(); k++) {
            uint32_t i = split[k];
            while (i > 0 && i < nImg_ && frame_[i - 1] == frame_[i]) i--;
            split[k] = std::max(i, split[k - 1]);
        }
    }

    uint32_t imgCols(uint32_t img, uint32_t* cols) const {
        return imageColumns(P_, img, cols);
    }

    // ================
    // one LM iteration
    // ================

    double iterate(double damping, bool reuse, bool cg) {
        auto mark = std::chrono::steady_clock::now();
        auto lap = [&mark] {
            const auto now = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(now - mark).count();
            mark = now;
            return dt;
        };
        if (reuse) {
            Bp_ = Bp0_;  // the point back-substitution overwrote it in place
        } else {
            poses0_ = P_.poses;
            exts0_ = P_.exts;
            intr0_ = P_.intr;
            points0_ = P_.points;
            jacobianPass();
            Bp0_ = Bp_;
        }
        prof_.jac += lap();
        pointPrep(damping);
        prof_.prep += lap();
        if (cg) {
            cgCamDiag(damping);
            cgPrecFactor();
            prof_.schur += lap();
            cgIters_ = runPCG(cgMaxit_);
        } else {
            schurAssemble(damping);
            prof_.schur += lap();
            S_.factorSolve(g_.data(), *pool_, nthreads_);
        }
        prof_.lin += lap();
        dpAccum();
        pointUpdate();
        camUpdate();
        prof_.back += lap();
        const double c = computeCost();
        prof_.cost += lap();
        return c;
    }

    void restore() {
        P_.poses = poses0_;
        P_.exts = exts0_;
        P_.intr = intr0_;
        P_.points = points0_;
    }

    // Jc, Jp, the weighted residual and the per-point normal blocks. One task
    // per range of points, so App/Bp need no atomics and every write is
    // sequential (observations are stored point-major).
    void jacobianPass() {
        withLoss(opt_.loss, [&](auto L) {
            using LT = decltype(L);
            const int nt = taskCount(nPts_, 2048, nthreads_);
            pool_->run(nt, nthreads_, [&](int t, int) {
                int64_t lo, hi;
                taskRange(nPts_, nt, t, lo, hi);
                double jcf[2 * kMaxCamDof], jpf[6], r[2];
                for (int64_t p = lo; p < hi; p++) {
                    double App[9] = {}, Bp[3] = {};
                    for (uint32_t o = P_.obs_ranges[p]; o < P_.obs_ranges[p + 1]; o++) {
                        const uint32_t img = P_.obs_image[o];
                        const uint32_t dofw = dof_[img], ne = efree_[img], gz = gz_[img];
                        double* jc = &Jc_[P_.jc_off[o]];
                        double* jp = &Jp_[6 * (size_t)o];
                        const double* pose = &P_.poses[6 * (size_t)frame_[img]];
                        withModel(model_[img], [&](auto M) {
                            using MT = decltype(M);
                            // The evaluated block is [6 | NE | kNumIntr]; the
                            // stored one [6 | ne | gz] (sfm/ba/Problem.h).
                            const bool rig = eoff_[img] != kNoSlot;
                            const int NE = rig ? 6 : 0;
                            const int DOF = 6 + NE + MT::kNumIntr;
                            if (rig)
                                jacobianRig<MT>(pose, &P_.exts[eoff_[img]], &P_.intr[ioff_[img]],
                                                &P_.points[3 * (size_t)p], &P_.obs_xy[2 * (size_t)o],
                                                r, jcf, jpf);
                            else
                                jacobian<MT>(pose, &P_.intr[ioff_[img]], &P_.points[3 * (size_t)p],
                                             &P_.obs_xy[2 * (size_t)o], r, jcf, jpf);
                            const double sw =
                                std::sqrt(LT::weight(r[0] * r[0] + r[1] * r[1], lossParam_));
                            for (int row = 0; row < 2; row++) {
                                const double* src = jcf + row * DOF;
                                double* dst = jc + row * dofw;
                                for (uint32_t a = 0; a < 6 + ne; a++) dst[a] = src[a] * sw;
                                for (uint32_t i = 0; i < gz; i++)
                                    dst[6 + ne + i] = src[6 + NE + i] * sw;
                                for (int j = 0; j < 3; j++) jp[row * 3 + j] = jpf[row * 3 + j] * sw;
                            }
                            r[0] *= sw;
                            r[1] *= sw;
                        });
                        res_[2 * (size_t)o] = r[0];
                        res_[2 * (size_t)o + 1] = r[1];
                        for (int i = 0; i < 3; i++) {
                            for (int j = 0; j < 3; j++) {
                                const double v = jp[i] * jp[j] + jp[3 + i] * jp[3 + j];
                                if (std::isfinite(v)) App[3 * i + j] += v;
                            }
                            const double bv = jp[i] * r[0] + jp[3 + i] * r[1];
                            if (std::isfinite(bv)) Bp[i] += bv;
                        }
                    }
                    memcpy(&App_[9 * (size_t)p], App, sizeof App);
                    memcpy(&Bp_[3 * (size_t)p], Bp, sizeof Bp);
                }
            });
        });
    }

    // W = (App + lambda)^-1, by the adjugate formula of common/linalg.slang.
    void pointPrep(double lambda) {
        const double d = 1.0 + lambda;
        const int nt = taskCount(nPts_, 4096, nthreads_);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(nPts_, nt, t, lo, hi);
            for (int64_t p = lo; p < hi; p++) {
                double m[9];
                memcpy(m, &App_[9 * (size_t)p], sizeof m);
                m[0] *= d;
                m[4] *= d;
                m[8] *= d;
                const double c00 = m[4] * m[8] - m[5] * m[7];
                const double c01 = m[5] * m[6] - m[3] * m[8];
                const double c02 = m[3] * m[7] - m[4] * m[6];
                const double inv = 1.0 / (m[0] * c00 + m[1] * c01 + m[2] * c02);
                double* w = &W_[9 * (size_t)p];
                w[0] = c00 * inv;
                w[1] = (m[2] * m[7] - m[1] * m[8]) * inv;
                w[2] = (m[1] * m[5] - m[2] * m[4]) * inv;
                w[3] = c01 * inv;
                w[4] = (m[0] * m[8] - m[2] * m[6]) * inv;
                w[5] = (m[2] * m[3] - m[0] * m[5]) * inv;
                w[6] = c02 * inv;
                w[7] = (m[1] * m[6] - m[0] * m[7]) * inv;
                w[8] = (m[0] * m[4] - m[1] * m[3]) * inv;
            }
        });
    }

    // ================
    // dense path
    // ================

    void addAt(uint32_t R, uint32_t C, double v, double* sb) {
        if (R >= poseDim_) {
            const int k = srow_[R - poseDim_];
            if (k >= 0) {
                sb[(size_t)C * m_ + k] += v;
                return;
            }
        }
        S_.row(R)[C] += v;
    }
    // An element whose two columns coincide is reached by both orderings of the
    // observation pair, so it takes the value twice.
    void addSym(uint32_t u, uint32_t v, double val, double* sb) {
        if (u == v) {
            addAt(u, u, val + val, sb);
            return;
        }
        addAt(u > v ? u : v, u > v ? v : u, val, sb);
    }
    void addG(uint32_t col, double v, double* sg) {
        if (col >= poseDim_) {
            const int k = srow_[col - poseDim_];
            if (k >= 0) {
                sg[k] += v;
                return;
            }
        }
        g_[col] += v;
    }

    void schurAssemble(double lambda) {
        S_.zero(*pool_, nthreads_);
        std::fill(g_.begin(), g_.end(), 0.0);
        if (m_) {
            std::fill(sbuf_.begin(), sbuf_.end(), 0.0);
            std::fill(sgbuf_.begin(), sgbuf_.end(), 0.0);
        }
        const double dmp = 1.0 + lambda;
        const int nt = (int)asmSplit_.size() - 1;
        pool_->run(nt, nthreads_, [&](int task, int) {
            double* sb = m_ ? &sbuf_[(size_t)task * m_ * n_] : nullptr;
            double* sg = m_ ? &sgbuf_[(size_t)task * m_] : nullptr;
            uint32_t colsA[kMaxCamDof];
            double z0[kMaxCamDof], z1[kMaxCamDof];
            for (uint32_t a = asmSplit_[task]; a < asmSplit_[task + 1]; a++) {
                const uint32_t dofi = imgCols(a, colsA);
                for (uint32_t t = P_.cam_obs_ranges[a]; t < P_.cam_obs_ranges[a + 1]; t++) {
                    const uint32_t oi = P_.cam_obs[t];
                    const uint32_t p = P_.obs_point[oi];
                    const double* Wp = &W_[9 * (size_t)p];
                    const double* Jpi = &Jp_[6 * (size_t)oi];
                    const double* Jci = &Jc_[P_.jc_off[oi]];
                    double Y[6];
                    for (int row = 0; row < 2; row++)
                        for (int j = 0; j < 3; j++)
                            Y[3 * row + j] = Jpi[3 * row] * Wp[j] + Jpi[3 * row + 1] * Wp[3 + j] +
                                             Jpi[3 * row + 2] * Wp[6 + j];
                    for (uint32_t oj = P_.obs_ranges[p]; oj <= oi; oj++) {
                        const double* Jpj = &Jp_[6 * (size_t)oj];
                        const double q00 = Y[0] * Jpj[0] + Y[1] * Jpj[1] + Y[2] * Jpj[2];
                        const double q01 = Y[0] * Jpj[3] + Y[1] * Jpj[4] + Y[2] * Jpj[5];
                        const double q10 = Y[3] * Jpj[0] + Y[4] * Jpj[1] + Y[5] * Jpj[2];
                        const double q11 = Y[3] * Jpj[3] + Y[4] * Jpj[4] + Y[5] * Jpj[5];
                        for (uint32_t r = 0; r < dofi; r++) {
                            z0[r] = Jci[r] * q00 + Jci[dofi + r] * q10;
                            z1[r] = Jci[r] * q01 + Jci[dofi + r] * q11;
                        }
                        if (oj == oi) {
                            const double* Bp = &Bp_[3 * (size_t)p];
                            const double yb0 = Y[0] * Bp[0] + Y[1] * Bp[1] + Y[2] * Bp[2];
                            const double yb1 = Y[3] * Bp[0] + Y[4] * Bp[1] + Y[5] * Bp[2];
                            const double r0 = res_[2 * (size_t)oi] - yb0;
                            const double r1 = res_[2 * (size_t)oi + 1] - yb1;
                            for (uint32_t r = 0; r < dofi; r++) {
                                const double gv = Jci[r] * r0 + Jci[dofi + r] * r1;
                                if (std::isfinite(gv)) addG(colsA[r], gv, sg);
                                for (uint32_t c = 0; c <= r; c++) {
                                    double jj = Jci[r] * Jci[c] + Jci[dofi + r] * Jci[dofi + c];
                                    if (r == c) jj *= dmp;
                                    const double v =
                                        jj - (z0[r] * Jci[c] + z1[r] * Jci[dofi + c]);
                                    if (std::isfinite(v)) addAt(colsA[r], colsA[c], v, sb);
                                }
                            }
                            continue;
                        }
                        const uint32_t b = P_.obs_image[oj];
                        const uint32_t dofj = dof_[b];
                        const double* Jcj = &Jc_[P_.jc_off[oj]];
                        uint32_t colsB[kMaxCamDof];
                        imgCols(b, colsB);
                        // Track images ascend, so frame(b) <= frame(a): two
                        // frames put the pose x pose block below the diagonal,
                        // one frame (rig-mates) folds both orderings onto it.
                        if (frame_[a] != frame_[b]) {
                            const uint32_t bp = 6 * frame_[b];
                            for (uint32_t r = 0; r < 6; r++) {
                                double* Srow = S_.row(colsA[r]) + bp;
                                for (uint32_t c = 0; c < 6; c++) {
                                    const double v = -(z0[r] * Jcj[c] + z1[r] * Jcj[dofj + c]);
                                    if (std::isfinite(v)) Srow[c] += v;
                                }
                                for (uint32_t c = 6; c < dofj; c++) {
                                    const double v = -(z0[r] * Jcj[c] + z1[r] * Jcj[dofj + c]);
                                    if (std::isfinite(v)) addAt(colsB[c], colsA[r], v, sb);
                                }
                            }
                            for (uint32_t r = 6; r < dofi; r++)
                                for (uint32_t c = 0; c < dofj; c++) {
                                    const double v = -(z0[r] * Jcj[c] + z1[r] * Jcj[dofj + c]);
                                    if (std::isfinite(v)) addSym(colsA[r], colsB[c], v, sb);
                                }
                        } else {
                            for (uint32_t r = 0; r < dofi; r++)
                                for (uint32_t c = 0; c < dofj; c++) {
                                    const double v = -(z0[r] * Jcj[c] + z1[r] * Jcj[dofj + c]);
                                    if (std::isfinite(v)) addSym(colsA[r], colsB[c], v, sb);
                                }
                        }
                    }
                }
            }
        });
        if (!m_) return;
        const int nt2 = taskCount(n_, 4096, nthreads_);
        pool_->run(nt2, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(n_, nt2, t, lo, hi);
            for (uint32_t k = 0; k < m_; k++) {
                const uint32_t R = sharedCol_[k];
                double* Srow = S_.row(R);
                for (int64_t c = lo; c < hi && c <= (int64_t)R; c++) {
                    double s = 0;
                    for (int q = 0; q < nt; q++)
                        s += sbuf_[(size_t)q * m_ * n_ + (size_t)c * m_ + k];
                    Srow[c] += s;
                }
            }
        });
        for (uint32_t k = 0; k < m_; k++) {
            double s = 0;
            for (int q = 0; q < nt; q++) s += sgbuf_[(size_t)q * m_ + k];
            g_[sharedCol_[k]] += s;
        }
    }

    // ================
    // implicit-Schur PCG
    // ================

    // B_c, the diagonal blocks of S under the preconditioner partition, and the
    // reduced right-hand side -- the diagonal-pair part of the dense assembly,
    // per camera (see cg_cam_diag in sfm/shaders/ba/cg.slang).
    void cgCamDiag(double lambda) {
        std::fill(cgB_.begin(), cgB_.end(), 0.0);
        std::fill(cgM_.begin(), cgM_.end(), 0.0);
        std::fill(g_.begin(), g_.end(), 0.0);
        if (!exclusive_) {
            std::fill(cgGIntr_.begin(), cgGIntr_.end(), 0.0);
            std::fill(cgGrp_.begin(), cgGrp_.end(), 0.0);
        }
        const double dmp = 1.0 + lambda;
        const int nt = (int)cgSplit_.size() - 1;
        const uint32_t mbase = P_.num_frames;
        pool_->run(nt, nthreads_, [&](int task, int) {
            double* gi = exclusive_ ? nullptr : &cgGIntr_[(size_t)task * tail_];
            double* gb = exclusive_ ? nullptr : &cgGrp_[(size_t)task * nSharedBlk_ * kCamBlk];
            uint32_t cols[kMaxCamDof];
            double accB[kCamBlk], accM[kCamBlk], gacc[kMaxCamDof], z0[kMaxCamDof], z1[kMaxCamDof];
            for (uint32_t img = cgSplit_[task]; img < cgSplit_[task + 1]; img++) {
                const uint32_t dof = imgCols(img, cols);
                const uint32_t nb = dof * (dof + 1) / 2;
                std::fill(accB, accB + nb, 0.0);
                std::fill(accM, accM + nb, 0.0);
                std::fill(gacc, gacc + dof, 0.0);
                for (uint32_t t = P_.cam_obs_ranges[img]; t < P_.cam_obs_ranges[img + 1]; t++) {
                    const uint32_t o = P_.cam_obs[t], p = P_.obs_point[o];
                    const double* Jc = &Jc_[P_.jc_off[o]];
                    const double* Jp = &Jp_[6 * (size_t)o];
                    const double* Wp = &W_[9 * (size_t)p];
                    const double* Bp = &Bp_[3 * (size_t)p];
                    double Y[6];
                    for (int row = 0; row < 2; row++)
                        for (int j = 0; j < 3; j++)
                            Y[3 * row + j] = Jp[3 * row] * Wp[j] + Jp[3 * row + 1] * Wp[3 + j] +
                                             Jp[3 * row + 2] * Wp[6 + j];
                    const double q00 = Y[0] * Jp[0] + Y[1] * Jp[1] + Y[2] * Jp[2];
                    const double q01 = Y[0] * Jp[3] + Y[1] * Jp[4] + Y[2] * Jp[5];
                    const double q10 = Y[3] * Jp[0] + Y[4] * Jp[1] + Y[5] * Jp[2];
                    const double q11 = Y[3] * Jp[3] + Y[4] * Jp[4] + Y[5] * Jp[5];
                    for (uint32_t r = 0; r < dof; r++) {
                        z0[r] = Jc[r] * q00 + Jc[dof + r] * q10;
                        z1[r] = Jc[r] * q01 + Jc[dof + r] * q11;
                    }
                    for (uint32_t r = 0; r < dof; r++)
                        for (uint32_t c = 0; c <= r; c++) {
                            accM[pidx(r, c)] += z0[r] * Jc[c] + z1[r] * Jc[dof + c];
                            accB[pidx(r, c)] += Jc[r] * Jc[c] + Jc[dof + r] * Jc[dof + c];
                        }
                    const double yb0 = Y[0] * Bp[0] + Y[1] * Bp[1] + Y[2] * Bp[2];
                    const double yb1 = Y[3] * Bp[0] + Y[4] * Bp[1] + Y[5] * Bp[2];
                    const double d0 = yb0 - res_[2 * (size_t)o];
                    const double d1 = yb1 - res_[2 * (size_t)o + 1];
                    for (uint32_t r = 0; r < dof; r++)
                        gacc[r] += Jc[r] * d0 + Jc[dof + r] * d1;
                }
                double* Bblk = &cgB_[(size_t)kCamBlk * img];
                // M's rows go to the partition block owning them: the image's
                // (exclusive) or the frame's directly, this task's both; the
                // member's and group's through per-task slots summed afterwards.
                double* Fblk = exclusive_ ? &cgM_[(size_t)kCamBlk * img]
                                          : &cgM_[(size_t)kCamBlk * frame_[img]];
                double* Mblk = nullptr;
                double* Gblk = nullptr;
                const uint32_t ne = efree_[img];
                if (!exclusive_) {
                    if (ne) Mblk = gb + (size_t)(P_.image_member[img]) * kCamBlk;
                    Gblk = gb + (size_t)(P_.members.size() + P_.image_group[img]) * kCamBlk;
                }
                for (uint32_t r = 0; r < dof; r++)
                    for (uint32_t c = 0; c <= r; c++) {
                        double jj = accB[pidx(r, c)];
                        if (r == c) jj *= dmp;
                        const double mv = jj - accM[pidx(r, c)];
                        if (std::isfinite(jj)) Bblk[pidx(r, c)] += jj;
                        if (!std::isfinite(mv)) continue;
                        if (exclusive_ || r < 6) Fblk[pidx(r, c)] += mv;
                        else if (r < 6 + ne) { if (c >= 6) Mblk[pidx(r - 6, c - 6)] += mv; }
                        else if (c >= 6 + ne) Gblk[pidx(r - 6 - ne, c - 6 - ne)] += mv;
                    }
                for (uint32_t r = 0; r < dof; r++) {
                    if (!std::isfinite(gacc[r])) continue;
                    if (exclusive_ || cols[r] < poseDim_) g_[cols[r]] -= gacc[r];
                    else gi[cols[r] - poseDim_] -= gacc[r];
                }
            }
        });
        if (exclusive_) return;
        for (int q = 0; q < nt; q++) {
            const double* gq = &cgGIntr_[(size_t)q * tail_];
            for (uint32_t i = 0; i < tail_; i++) g_[poseDim_ + i] += gq[i];
        }
        for (uint32_t b = 0; b < nSharedBlk_; b++) {
            double* dst = &cgM_[(size_t)kCamBlk * (mbase + b)];
            for (int q = 0; q < nt; q++) {
                const double* src = &cgGrp_[((size_t)q * nSharedBlk_ + b) * kCamBlk];
                for (uint32_t i = 0; i < kCamBlk; i++) dst[i] += src[i];
            }
        }
    }

    void cgPrecFactor() {
        const uint32_t nblk = P_.num_prec_blocks;
        const int nt = taskCount(nblk, 256, nthreads_);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(nblk, nt, t, lo, hi);
            for (int64_t b = lo; b < hi; b++) {
                const uint32_t dof = P_.prec_blocks[4 * b + 1] + P_.prec_blocks[4 * b + 3];
                if (!dof) continue;
                double* L = &cgM_[(size_t)kCamBlk * b];
                for (uint32_t j = 0; j < dof; j++) {
                    const double d = std::sqrt(std::max(L[pidx(j, j)], 1e-30));
                    L[pidx(j, j)] = d;
                    for (uint32_t i = j + 1; i < dof; i++) L[pidx(i, j)] /= d;
                    for (uint32_t c = j + 1; c < dof; c++)
                        for (uint32_t i = c; i < dof; i++)
                            L[pidx(i, c)] -= L[pidx(i, j)] * L[pidx(c, j)];
                }
            }
        });
    }

    uint32_t precCols(uint32_t b, uint32_t* cols) const {
        const uint32_t c0 = P_.prec_blocks[4 * b], l0 = P_.prec_blocks[4 * b + 1];
        const uint32_t c1 = P_.prec_blocks[4 * b + 2], l1 = P_.prec_blocks[4 * b + 3];
        for (uint32_t i = 0; i < l0; i++) cols[i] = c0 + i;
        for (uint32_t i = 0; i < l1; i++) cols[l0 + i] = c1 + i;
        return l0 + l1;
    }

    void cgPrecApply(const std::vector<double>& r, std::vector<double>& z) {
        const uint32_t nblk = P_.num_prec_blocks;
        const int nt = taskCount(nblk, 256, nthreads_);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(nblk, nt, t, lo, hi);
            uint32_t cols[kMaxCamDof];
            double y[kMaxCamDof];
            for (int64_t b = lo; b < hi; b++) {
                const uint32_t dof = precCols((uint32_t)b, cols);
                if (!dof) continue;
                const double* L = &cgM_[(size_t)kCamBlk * b];
                for (uint32_t i = 0; i < dof; i++) {
                    double v = r[cols[i]];
                    for (uint32_t j = 0; j < i; j++) v -= L[pidx(i, j)] * y[j];
                    y[i] = v / L[pidx(i, i)];
                }
                for (int i = (int)dof - 1; i >= 0; i--) {
                    double v = y[i];
                    for (uint32_t j = (uint32_t)i + 1; j < dof; j++)
                        v -= L[pidx(j, (uint32_t)i)] * y[j];
                    y[i] = v / L[pidx((uint32_t)i, (uint32_t)i)];
                }
                for (uint32_t i = 0; i < dof; i++) z[cols[i]] = y[i];
            }
        });
    }

    void cgGather() {
        const int nt = taskCount(nPts_, 1024, nthreads_);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(nPts_, nt, t, lo, hi);
            uint32_t cols[kMaxCamDof];
            for (int64_t p = lo; p < hi; p++) {
                double u0 = 0, u1 = 0, u2 = 0;
                for (uint32_t o = P_.obs_ranges[p]; o < P_.obs_ranges[p + 1]; o++) {
                    const uint32_t img = P_.obs_image[o];
                    const double* Jc = &Jc_[P_.jc_off[o]];
                    const double* Jp = &Jp_[6 * (size_t)o];
                    const uint32_t dof = imgCols(img, cols);
                    double d0 = 0, d1 = 0;
                    for (uint32_t a = 0; a < dof; a++) {
                        const double xa = cgP_[cols[a]];
                        d0 += Jc[a] * xa;
                        d1 += Jc[dof + a] * xa;
                    }
                    u0 += Jp[0] * d0 + Jp[3] * d1;
                    u1 += Jp[1] * d0 + Jp[4] * d1;
                    u2 += Jp[2] * d0 + Jp[5] * d1;
                }
                const double* w = &W_[9 * (size_t)p];
                for (int i = 0; i < 3; i++)
                    cgV_[3 * (size_t)p + i] = w[3 * i] * u0 + w[3 * i + 1] * u1 + w[3 * i + 2] * u2;
            }
        });
    }

    // Sp = B p, then Sp -= sum_obs Acp v. A frame's rows belong to one task
    // (rig-mates share a task); the tail past the poses is summed per task.
    void cgMatvec() {
        const int nt = (int)cgSplit_.size() - 1;
        if (!exclusive_) std::fill(cgSpIntr_.begin(), cgSpIntr_.end(), 0.0);
        const bool rigs = P_.hasRigs();
        if (rigs) std::fill(cgSp_.begin(), cgSp_.begin() + poseDim_, 0.0);
        pool_->run(nt, nthreads_, [&](int task, int) {
            double* si = exclusive_ ? nullptr : &cgSpIntr_[(size_t)task * tail_];
            uint32_t cols[kMaxCamDof];
            for (uint32_t img = cgSplit_[task]; img < cgSplit_[task + 1]; img++) {
                const uint32_t dof = imgCols(img, cols);
                const double* B = &cgB_[(size_t)kCamBlk * img];
                for (uint32_t r = 0; r < dof; r++) {
                    double acc = 0;
                    for (uint32_t c = 0; c < dof; c++)
                        acc += B[r >= c ? pidx(r, c) : pidx(c, r)] * cgP_[cols[c]];
                    if (exclusive_ || r < 6) {
                        if (rigs) cgSp_[cols[r]] += acc;
                        else cgSp_[cols[r]] = acc;
                    } else if (std::isfinite(acc)) {
                        si[cols[r] - poseDim_] += acc;
                    }
                }
            }
        });
        pool_->run(nt, nthreads_, [&](int task, int) {
            double* si = exclusive_ ? nullptr : &cgSpIntr_[(size_t)task * tail_];
            uint32_t cols[kMaxCamDof];
            double acc[kMaxCamDof];
            for (uint32_t img = cgSplit_[task]; img < cgSplit_[task + 1]; img++) {
                const uint32_t dof = imgCols(img, cols);
                std::fill(acc, acc + dof, 0.0);
                for (uint32_t t = P_.cam_obs_ranges[img]; t < P_.cam_obs_ranges[img + 1]; t++) {
                    const uint32_t o = P_.cam_obs[t], p = P_.obs_point[o];
                    const double* Jc = &Jc_[P_.jc_off[o]];
                    const double* Jp = &Jp_[6 * (size_t)o];
                    const double* v = &cgV_[3 * (size_t)p];
                    const double d0 = Jp[0] * v[0] + Jp[1] * v[1] + Jp[2] * v[2];
                    const double d1 = Jp[3] * v[0] + Jp[4] * v[1] + Jp[5] * v[2];
                    for (uint32_t a = 0; a < dof; a++) acc[a] += Jc[a] * d0 + Jc[dof + a] * d1;
                }
                for (uint32_t r = 0; r < dof; r++) {
                    if (!std::isfinite(acc[r])) continue;
                    if (exclusive_ || r < 6) cgSp_[cols[r]] -= acc[r];
                    else si[cols[r] - poseDim_] -= acc[r];
                }
            }
        });
        if (exclusive_) return;
        for (uint32_t i = 0; i < tail_; i++) {
            double s = 0;
            for (int q = 0; q < nt; q++) s += cgSpIntr_[(size_t)q * tail_ + i];
            cgSp_[poseDim_ + i] = s;
        }
    }

    double dot(const std::vector<double>& a, const std::vector<double>& b) {
        const int nt = taskCount(n_, 1 << 14, nthreads_);
        part_.assign(nt, 0.0);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(n_, nt, t, lo, hi);
            double s = 0;
            for (int64_t i = lo; i < hi; i++) s += a[i] * b[i];
            part_[t] = s;
        });
        double s = 0;
        for (double v : part_) s += v;
        return s;
    }

    uint32_t runPCG(uint32_t maxit) {
        cgR_ = g_;
        std::fill(g_.begin(), g_.end(), 0.0);
        cgPrecApply(cgR_, cgZ_);
        double rho = dot(cgR_, cgZ_), rr = dot(cgR_, cgR_);
        const double tol2 = opt_.cg_tol * opt_.cg_tol * rr;
        bool conv = !(std::isfinite(rho) && rho > 0.0);
        uint32_t iters = 0;
        cgP_ = cgZ_;
        while (!conv && iters < maxit) {
            cgGather();
            cgMatvec();
            const double pAp = dot(cgP_, cgSp_);
            if (!(std::isfinite(pAp) && pAp > 0.0)) {  // lost positive-definiteness
                conv = true;
                break;
            }
            const double alpha = rho / pAp;
            const int nt = taskCount(n_, 1 << 14, nthreads_);
            pool_->run(nt, nthreads_, [&](int t, int) {
                int64_t lo, hi;
                taskRange(n_, nt, t, lo, hi);
                for (int64_t i = lo; i < hi; i++) {
                    g_[i] += alpha * cgP_[i];
                    cgR_[i] -= alpha * cgSp_[i];
                }
            });
            cgPrecApply(cgR_, cgZ_);
            const double a = dot(cgR_, cgZ_), bb = dot(cgR_, cgR_);
            const double beta = a / rho;
            rho = a;
            rr = bb;
            iters++;
            if (!(bb > tol2) || !std::isfinite(a) || !(a > 0.0)) {
                conv = true;
                break;
            }
            pool_->run(nt, nthreads_, [&](int t, int) {
                int64_t lo, hi;
                taskRange(n_, nt, t, lo, hi);
                for (int64_t i = lo; i < hi; i++) cgP_[i] = cgZ_[i] + beta * cgP_[i];
            });
        }
        cgConverged_ = conv && rr <= tol2;
        return iters;
    }

    // ================
    // back-substitution and updates
    // ================

    void dpAccum() {
        const int nt = taskCount(nPts_, 1024, nthreads_);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(nPts_, nt, t, lo, hi);
            uint32_t cols[kMaxCamDof];
            for (int64_t p = lo; p < hi; p++) {
                double* Bp = &Bp_[3 * (size_t)p];
                for (uint32_t o = P_.obs_ranges[p]; o < P_.obs_ranges[p + 1]; o++) {
                    const uint32_t img = P_.obs_image[o];
                    const double* Jc = &Jc_[P_.jc_off[o]];
                    const double* Jp = &Jp_[6 * (size_t)o];
                    const uint32_t dof = imgCols(img, cols);
                    double d0 = 0, d1 = 0;
                    for (uint32_t a = 0; a < dof; a++) {
                        const double x = g_[cols[a]];
                        d0 += Jc[a] * x;
                        d1 += Jc[dof + a] * x;
                    }
                    for (int j = 0; j < 3; j++) {
                        const double v = Jp[j] * d0 + Jp[3 + j] * d1;
                        if (std::isfinite(v)) Bp[j] -= v;
                    }
                }
            }
        });
    }

    void pointUpdate() {
        const int nt = taskCount(nPts_, 4096, nthreads_);
        pool_->run(nt, nthreads_, [&](int t, int) {
            int64_t lo, hi;
            taskRange(nPts_, nt, t, lo, hi);
            for (int64_t p = lo; p < hi; p++) {
                const double* w = &W_[9 * (size_t)p];
                const double* Bp = &Bp_[3 * (size_t)p];
                for (int i = 0; i < 3; i++) {
                    const double d = w[3 * i] * Bp[0] + w[3 * i + 1] * Bp[1] + w[3 * i + 2] * Bp[2];
                    if (std::isfinite(d)) P_.points[3 * (size_t)p + i] -= d;
                }
            }
        });
    }

    void camUpdate() {
        for (uint32_t i = 0; i < poseDim_; i++)
            if (std::isfinite(g_[i])) P_.poses[i] -= g_[i];
        for (const BAProblem::Member& m : P_.members)
            for (uint32_t j = 0; j < m.n_free; j++)
                if (std::isfinite(g_[m.ext_col + j])) P_.exts[m.ext_offset + j] -= g_[m.ext_col + j];
        for (const BAProblem::Group& g : P_.groups)
            for (uint32_t j = 0; j < g.n_intr; j++)
                if (std::isfinite(g_[g.intr_col + j])) P_.intr[g.intr_offset + j] -= g_[g.intr_col + j];
    }

    BAProblem& P_;
    SolverOptions opt_;
    SolverStats stats_;
    Pool* pool_ = nullptr;
    int nthreads_ = 1;
    double lossParam_ = 1.0;

    uint32_t n_ = 0, poseDim_ = 0, tail_ = 0, nImg_ = 0, nPts_ = 0, nObs_ = 0;
    std::vector<uint8_t> dof_, gz_, model_, efree_;
    std::vector<uint32_t> icol_, ioff_, frame_, eoff_;  // eoff_: into exts, or kNoSlot
    bool exclusive_ = true;

    std::vector<int32_t> srow_;       // tail column -> shared-row slot, or -1
    std::vector<uint32_t> sharedCol_;
    uint32_t m_ = 0, nSharedBlk_ = 0;

    bool useCG_ = false, haveFallback_ = false, cgConverged_ = false;
    uint32_t cgMaxit_ = 100, cgIters_ = 0;

    std::vector<double> Jc_, Jp_, res_, App_, W_, Bp_, Bp0_, g_;
    std::vector<double> poses0_, exts0_, intr0_, points0_;
    std::vector<double> sbuf_, sgbuf_, part_;
    std::vector<double> cgR_, cgZ_, cgP_, cgSp_, cgV_, cgB_, cgM_, cgGIntr_, cgSpIntr_, cgGrp_;
    std::vector<uint32_t> asmSplit_, cgSplit_;
    DenseSpd S_;
    struct { double jac = 0, prep = 0, schur = 0, lin = 0, back = 0, cost = 0; } prof_;
};

}  // namespace bacpu
