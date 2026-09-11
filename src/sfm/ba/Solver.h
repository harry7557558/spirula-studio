// Levenberg-Marquardt driver: owns GPU buffers, records the per-iteration
// command stream (assembly -> linear solve -> updates), and runs the
// accept/reject loop on the host (one small readback per iteration).
//
// Two linear solvers for the reduced camera system S dU = g:
//   dense - packed in-place blocked Cholesky (cholesky.slang), with S built
//           by the pair-aggregated (or atomic fallback) Schur kernels
//   cg    - implicit-Schur block-Jacobi PCG (cg.slang); S is never formed,
//           so the packed S, Y and pair-entry buffers are not allocated and
//           peak VRAM stays linear in the observation count
// Selection is automatic by problem size and a VRAM budget (see decidePaths),
// or forced with SolverOptions::solver. Optionally the dense machinery is
// kept allocated as a fallback: if CG fails to converge, the iteration is
// re-solved densely (assembly reuse, like a rejected step).
//
// A device that can run none of the scalar configurations gets `RealCfg::CPU`,
// and every entry point below delegates to bacpu::Solver -- the same LM loop
// and the same two linear solvers, on the host.
#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <memory>
#include <vector>

#include "sfm/ba/Options.h"
#include "sfm/ba/Problem.h"
#include "sfm/ba/SolverCpu.h"
#include "sfm/vk/EmbeddedSpirv.h"
#include "sfm/vk/VkContext.h"
#include "core/Env.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Log.h"

// Can this device run the kernels compiled for `c`?
//   double - fp64 arithmetic, and an fp64 atomic add for the reductions
//   float  - an fp32 atomic add
//   df     - neither; the emulated double-float pair reduces through int64
//            atomics
//   cpu    - nothing at all; it runs on the host (sfm/ba/SolverCpu.h)
inline bool realSupportedByDevice(RealCfg c, const VkDeviceCaps& caps) {
    switch (c) {
        case RealCfg::F64:
            return caps.float64 && caps.float32AtomicAdd && caps.float64AtomicAdd;
        case RealCfg::F32:
            return caps.float32AtomicAdd;
        case RealCfg::DF64:
            return caps.int64Atomics;
        case RealCfg::CPU:
            return true;
    }
    return false;
}

// The closest thing to `want` the device can run: fp64, else the host. Neither
// fp32-based configuration is in the chain -- `float` stalls the normal
// equations above ~1e-7 relative accuracy and `df` buys its ~48 bits with
// CAS-loop atomics; both stay available on request (../README.md).
inline RealCfg pickRealForDevice(RealCfg want, const VkDeviceCaps& caps) {
    if (realSupportedByDevice(want, caps)) return want;
    if (realSupportedByDevice(RealCfg::F64, caps)) return RealCfg::F64;
    return RealCfg::CPU;
}

// Probing means an instance + an enumeration, and the mapper's scoped solves
// would pay it per BA. Device features do not change under us, so probe once
// per device index.
inline const VkDeviceCaps& cachedDeviceCaps(int deviceIndex) {
    static std::mutex m;
    static std::map<int, VkDeviceCaps> cache;  // node-based: references stay valid
    std::lock_guard<std::mutex> g(m);
    auto it = cache.find(deviceIndex);
    if (it == cache.end())
        it = cache.emplace(deviceIndex, VkContext::probeCaps(deviceIndex)).first;
    return it->second;
}

class BundleSolver {
    enum class LinSolve { DenseObs, DensePair, CG };
    // Preconditioner block stride; matches kCamBlk in cg.slang.
    static constexpr uint32_t kCamBlk = kMaxPlainDof * (kMaxPlainDof + 1) / 2;

public:
    // With `shared` the solver runs on a caller-owned persistent context:
    // device, pipelines and descriptor machinery are created once and reused
    // across solver instances (the mapper's periodic global BAs), and only the
    // problem-sized buffers are per-instance -- created in init(), freed in
    // the destructor. Without it the solver owns a scoped context, the old
    // behavior (ba_main, selftests).
    BundleSolver(BAProblem& P, const SolverOptions& opt, VkContext* shared = nullptr)
        : P_(P), opt_(opt), owned_(shared ? nullptr : new VkContext),
          ctx_(shared ? *shared : *owned_) {}

    ~BundleSolver() {
        // Persistent context: return this problem's VRAM now, not at ctx
        // teardown. (Owned context frees everything in ~VkContext anyway.)
        if (!owned_)
            for (GpuBuffer* b : ownBufs_) ctx_.destroyBuffer(*b);
    }

    void init() {
        auto prof_t0 = std::chrono::steady_clock::now();
        auto prof_lap = [&prof_t0] {
            auto t1 = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(t1 - prof_t0).count();
            prof_t0 = t1;
            return dt;
        };
        // Fall back rather than fail: an unsupported request used to surface as
        // VK_ERROR_FEATURE_NOT_PRESENT from vkCreateDevice, which killed the
        // run at its first bundle adjustment. No AMD part has an fp64 buffer
        // atomic add, so this is the ordinary path, not a corner.
        if (opt_.real != RealCfg::CPU) {
            const VkDeviceCaps& caps = ctx_.initialized()
                                           ? ctx_.caps()
                                           : cachedDeviceCaps(opt_.device);
            RealCfg real = pickRealForDevice(opt_.real, caps);
            if (real != opt_.real) {
                // Once per (asked, got) pair: the mapper builds a solver per
                // global BA, and a hundred identical lines say nothing the
                // first one did not.
                static std::mutex said_mu;
                static std::set<std::pair<int, int>> said;
                bool first;
                {
                    std::lock_guard<std::mutex> g(said_mu);
                    first = said.emplace((int)opt_.real, (int)real).second;
                }
                if (first)
                    sfm::slog::diag(sfm::slog::Tag::Map,
                                    "[ba] device does not support '%s' arithmetic; "
                               "falling back to '%s'",
                               realCfgName(opt_.real), realCfgName(real));
                opt_.real = real;
            }
        }
        if (opt_.real == RealCfg::CPU) {
            cpu_.reset(new bacpu::Solver(P_, opt_));
            cpu_->init();
            return;
        }
        VkContextOptions vopt;
        vopt.needFloat64 = opt_.real == RealCfg::F64;
        vopt.needFloatAtomics = opt_.real != RealCfg::DF64;
        vopt.needInt64Atomics = opt_.real == RealCfg::DF64;
        vopt.deviceIndex = opt_.device;
        vopt.validate = opt_.validate;
        vopt.profile = opt_.profile;
        if (!ctx_.initialized()) ctx_.init(vopt);
        double t_ctx = prof_lap();

        decidePaths();

        const size_t rs = realSize(opt_.real);
        const uint64_t packed = (uint64_t)P_.n_dim * (P_.n_dim + 1) / 2;
        const bool needS = !useCG_ || haveFallback_;
        const uint32_t npart = (P_.n_dim + 255) / 256;

        auto mkReal = [&](uint64_t n) { return ctx_.createBuffer(std::max<uint64_t>(n, 1) * rs); };
        auto mkUint = [&](uint64_t n) { return ctx_.createBuffer(std::max<uint64_t>(n, 1) * 4); };

        bObs_ = mkReal(2 * (uint64_t)P_.num_obs);
        bObsImage_ = mkUint(P_.num_obs);
        bObsPoint_ = mkUint(P_.num_obs);
        bImageInfo_ = ctx_.createBuffer(std::max<size_t>(P_.num_images, 1) * 16);
        bGroupInfo_ = ctx_.createBuffer(std::max<size_t>(P_.groups.size(), 1) * 16);
        bMemberInfo_ = ctx_.createBuffer(std::max<size_t>(P_.members.size(), 1) * 16);
        bPoses_ = mkReal(P_.pose_dim);
        bExts_ = mkReal(P_.exts.size());
        bIntr_ = mkReal(P_.total_intr);
        bPoints_ = mkReal(3 * (uint64_t)P_.num_points);
        bObsRanges_ = mkUint(P_.num_points + 1);
        bModelObs_ = mkUint(P_.model_obs.size());
        bJcOff_ = mkUint(P_.num_obs);
        bJp_ = mkReal(6 * (uint64_t)P_.num_obs);
        bS_ = mkReal(needS ? packed : 1);
        bG_ = mkReal(P_.n_dim);
        bApp_ = mkReal(9 * (uint64_t)P_.num_points);
        bBp_ = mkReal(3 * (uint64_t)P_.num_points);
        bCost_ = mkReal(4);
        bPosesBak_ = mkReal(P_.pose_dim);
        bExtsBak_ = mkReal(P_.exts.size());
        bIntrBak_ = mkReal(P_.total_intr);
        bPointsBak_ = mkReal(3 * (uint64_t)P_.num_points);
        bPairEntries_ = mkUint(std::max<size_t>(P_.pair_entries.size(), 2));
        bPairChunks_ = mkUint(std::max<size_t>(P_.pair_chunks.size(), 2));
        // S/g are rebuilt from the per-observation Jacobians on every path, so
        // a rejected step needs no snapshot of them -- only Bp, which the
        // point back-substitution overwrites in place.
        bBp0_ = mkReal(3 * (uint64_t)P_.num_points);
        bJc_ = mkReal(P_.jc_total);
        bRes_ = mkReal(2 * (uint64_t)P_.num_obs);
        bW_ = mkReal(9 * (uint64_t)P_.num_points);
        bYp_ = mkReal(needS && P_.use_pair_schur ? 6 * (uint64_t)P_.num_obs : 1);
        bY_ = mkReal(P_.n_dim);
        // CG path buffers (1-element dummies when unused)
        bCamRanges_ = mkUint(cgAllocated_ ? P_.num_images + 1 : 1);
        bCamObs_ = mkUint(cgAllocated_ ? P_.num_obs : 1);
        bCamChunks_ = mkUint(cgAllocated_ ? P_.cam_chunks.size() : 3);
        bCgR_ = mkReal(cgAllocated_ ? P_.n_dim : 1);
        bCgZ_ = mkReal(cgAllocated_ ? P_.n_dim : 1);
        bCgP_ = mkReal(cgAllocated_ ? P_.n_dim : 1);
        bCgSp_ = mkReal(cgAllocated_ ? P_.n_dim : 1);
        bCgV_ = mkReal(cgAllocated_ ? 3 * (uint64_t)P_.num_points : 1);
        bCgB_ = mkReal(cgAllocated_ ? (uint64_t)bBlk_ * P_.num_images : 1);
        bCgM_ = mkReal(cgAllocated_ ? (uint64_t)kCamBlk * P_.num_prec_blocks : 1);
        bCgScal_ = mkReal(8);
        bCgPart_ = mkReal(cgAllocated_ ? 2 * (uint64_t)npart : 1);
        bPrecBlocks_ = mkUint(cgAllocated_ ? P_.prec_blocks.size() : 4);

        // binding order must match sfm/shaders/ba/ba.slang + cg.slang; atomic views
        // alias the same VkBuffer at the odd bindings
        std::vector<VkBuffer> binds = {
            bObs_.buf, bObsImage_.buf, bObsPoint_.buf, bImageInfo_.buf, bGroupInfo_.buf,
            bPoses_.buf, bIntr_.buf, bPoints_.buf, bObsRanges_.buf, bModelObs_.buf, bJcOff_.buf,
            bJp_.buf,
            bS_.buf, bS_.buf, bG_.buf, bG_.buf, bApp_.buf, bApp_.buf, bBp_.buf, bBp_.buf,
            bCost_.buf, bCost_.buf,
            bPairEntries_.buf, bPairChunks_.buf, bW_.buf, bYp_.buf, bY_.buf,
            bJc_.buf, bRes_.buf,
            bCamRanges_.buf, bCamObs_.buf, bCgR_.buf, bCgZ_.buf, bCgP_.buf, bCgSp_.buf,
            bCgV_.buf, bCgB_.buf, bCgM_.buf, bCgScal_.buf, bCgPart_.buf,
            bCgSp_.buf, bCgB_.buf, bCgM_.buf, bCamChunks_.buf, bPrecBlocks_.buf,
            bMemberInfo_.buf, bExts_.buf,
        };
        ownBufs_ = {&bObs_, &bObsImage_, &bObsPoint_, &bImageInfo_, &bGroupInfo_,
                    &bMemberInfo_, &bExts_, &bExtsBak_,
                    &bPoses_, &bIntr_, &bPoints_, &bObsRanges_, &bModelObs_, &bJcOff_,
                    &bJp_, &bS_, &bG_, &bApp_, &bBp_, &bCost_,
                    &bPosesBak_, &bIntrBak_, &bPointsBak_,
                    &bBp0_, &bJc_, &bRes_,
                    &bPairEntries_, &bPairChunks_, &bW_, &bYp_, &bY_,
                    &bCamRanges_, &bCamObs_, &bCamChunks_, &bCgR_, &bCgZ_, &bCgP_, &bCgSp_,
                    &bCgV_, &bCgB_, &bCgM_, &bCgScal_, &bCgPart_, &bPrecBlocks_};
        double t_buf = prof_lap();
        ctx_.createDescriptors(binds);
        // Fresh-from-the-driver allocations happen to arrive zeroed; memory
        // recycled within a persistent context does not -- it still holds the
        // previous solve's data, and any kernel that accumulates into a
        // buffer it assumes zero would inherit garbage. Make the guarantee
        // explicit instead of allocation-dependent. (After createDescriptors:
        // begin() binds the descriptor set, which must not still reference
        // the previous solve's freed buffers.)
        {
            VkCommandBuffer cb = ctx_.begin();
            for (GpuBuffer* b : ownBufs_) ctx_.fillZero(cb, *b);
            ctx_.submit(cb);
        }

        // Only the entry points this problem dispatches: a cold driver cache
        // spends ~90 ms compiling each of the module's seventy-odd, and
        // loadPipelines skips what a shared context already built.
        std::vector<std::string> entries = {
            "point_prep", "point_update", "cam_update", "intr_update",
            std::string("dp_accum") + schurSuffix_,
        };
        if (!P_.members.empty()) entries.push_back("ext_update");
        if (useCG_) {
            const char* cg[] = {"cg_prec_fact", "cg_prec_apply", "cg_init", "cg_copy",
                                "cg_red2", "cg_fin", "cg_axpy", "cg_updp"};
            entries.insert(entries.end(), std::begin(cg), std::end(cg));
            for (const char* k : {"cg_cam_diag", "cg_gather", "cg_bmul", "cg_scatter"})
                entries.push_back(std::string(k) + cgSuffix_);
        }
        if (!useCG_ || haveFallback_) {
            const char* ch[] = {"chol_diag", "chol_panel", "chol_update", "tri_fwd", "tri_bwd"};
            entries.insert(entries.end(), std::begin(ch), std::end(ch));
            if (P_.use_pair_schur) {
                entries.push_back("y_prep");
                entries.push_back(std::string("schur_pair") + schurSuffix_);
            } else {
                entries.push_back(std::string("schur_obs") + schurSuffix_);
            }
        }
        for (const BAProblem::ModelRange& mr : P_.model_ranges) {
            entries.push_back(costEntry(mr));
            entries.push_back(jacEntry(mr));
        }
        // A shared context keeps its pipelines across solver instances; the
        // caller owning it must keep (real, loss) fixed, since the module is
        // compiled per pair (runGlobalBA's cache does -- the mapper never
        // varies them mid-run).
        if (!opt_.spv_path.empty()) {
            ctx_.loadPipelines(opt_.spv_path, entries);
        } else {
            std::string blob =
                std::string("ba_") + realCfgName(opt_.real) + "_" + opt_.loss;
            size_t words = 0;
            const uint32_t* code = sfm::findSpirv(blob.c_str(), &words);
            if (!code) {
                std::string have;
                for (size_t i = 0; const char* n = sfm::spirvBlobName(i); i++)
                    have += (i ? ", " : "") + std::string(n);
                throw std::runtime_error("shader variant '" + blob +
                                         "' is not built into this binary "
                                         "(configured with SS_SFM_REALS / "
                                         "SS_SFM_LOSSES); available: " + have);
            }
            ctx_.loadPipelines(code, words * 4, entries);
        }
        double t_pipe = prof_lap();

        // Upload static data + initial parameters, in one submit. Seventeen
        // fenced copies were most of a small solve's cost once several solvers
        // share a device (see VkContext::uploadMany).
        std::vector<uint8_t> obs, poses, exts, intr, points;
        packReals(obs, P_.obs_xy.data(), P_.obs_xy.size(), opt_.real);
        packReals(poses, P_.poses.data(), P_.poses.size(), opt_.real);
        packReals(exts, P_.exts.data(), P_.exts.size(), opt_.real);
        packReals(intr, P_.intr.data(), P_.intr.size(), opt_.real);
        packReals(points, P_.points.data(), P_.points.size(), opt_.real);
        std::vector<uint32_t> gi;
        for (auto& g : P_.groups) {
            gi.push_back(g.intr_offset);
            gi.push_back(g.intr_col);
            gi.push_back(g.n_intr);
            gi.push_back(g.model);
        }
        // Per image: frame, member, group, and whether the frame is shared
        // (which decides store vs. accumulate in cg_bmul).
        std::vector<uint32_t> frame_images(P_.num_frames, 0);
        for (uint32_t i = 0; i < P_.num_images; i++) frame_images[P_.image_frame[i]]++;
        std::vector<uint32_t> ii;
        ii.reserve(4 * P_.num_images);
        for (uint32_t i = 0; i < P_.num_images; i++) {
            ii.push_back(P_.image_frame[i]);
            ii.push_back(P_.image_member[i]);
            ii.push_back(P_.image_group[i]);
            ii.push_back(frame_images[P_.image_frame[i]] > 1 ? 1u : 0u);
        }
        std::vector<uint32_t> mi;
        for (auto& m : P_.members) {
            mi.push_back(m.ext_offset);
            mi.push_back(m.ext_col);
            mi.push_back(m.n_free);
            mi.push_back(0);
        }
        std::vector<VkContext::UploadItem> up = {
            {&bObs_, obs.data(), obs.size()},
            {&bObsImage_, P_.obs_image.data(), P_.obs_image.size() * 4},
            {&bObsPoint_, P_.obs_point.data(), P_.obs_point.size() * 4},
            {&bImageInfo_, ii.data(), ii.size() * 4},
            {&bGroupInfo_, gi.data(), gi.size() * 4},
            {&bObsRanges_, P_.obs_ranges.data(), P_.obs_ranges.size() * 4},
            {&bModelObs_, P_.model_obs.data(), P_.model_obs.size() * 4},
            {&bJcOff_, P_.jc_off.data(), P_.jc_off.size() * 4},
            {&bPoses_, poses.data(), poses.size()},
            {&bIntr_, intr.data(), intr.size()},
            {&bPoints_, points.data(), points.size()},
        };
        if (!P_.members.empty()) {
            up.push_back({&bMemberInfo_, mi.data(), mi.size() * 4});
            up.push_back({&bExts_, exts.data(), exts.size()});
        }
        if (P_.use_pair_schur) {
            up.push_back({&bPairEntries_, P_.pair_entries.data(), P_.pair_entries.size() * 4});
            up.push_back({&bPairChunks_, P_.pair_chunks.data(), P_.pair_chunks.size() * 4});
        }
        if (cgAllocated_) {
            up.push_back({&bCamRanges_, P_.cam_obs_ranges.data(), P_.cam_obs_ranges.size() * 4});
            up.push_back({&bCamObs_, P_.cam_obs.data(), P_.cam_obs.size() * 4});
            up.push_back({&bCamChunks_, P_.cam_chunks.data(), P_.cam_chunks.size() * 4});
            up.push_back({&bPrecBlocks_, P_.prec_blocks.data(), P_.prec_blocks.size() * 4});
        }
        ctx_.uploadMany(up.data(), up.size());

        double t_upload = prof_lap();
        if (spirula::env("SFM_MAP_PROF"))
            sfm::slog::diag(sfm::slog::Tag::Map,
                       "[prof]   solver init: ctx %.3f buf %.3f pipe %.3f upload %.3f s",
                       t_ctx, t_buf, t_pipe, t_upload);
        stats_.vram_mb = ctx_.totalAllocatedMB();
        stats_.solver = useCG_ ? (haveFallback_ ? "cg+fallback" : "cg") : "dense";
        if (opt_.verbose)
            sfm::slog::diag(sfm::slog::Tag::Map,
                            "[vk] n_dim = %u, solver = %s, VRAM allocated = %.1f MB",
                       P_.n_dim, stats_.solver, stats_.vram_mb);
    }

    // The host solver works on the problem's own parameter vectors, so upload
    // and download are the identity there.
    void uploadParams() {
        if (cpu_) return;
        std::vector<uint8_t> poses, exts, intr, points;
        packReals(poses, P_.poses.data(), P_.poses.size(), opt_.real);
        packReals(exts, P_.exts.data(), P_.exts.size(), opt_.real);
        packReals(intr, P_.intr.data(), P_.intr.size(), opt_.real);
        packReals(points, P_.points.data(), P_.points.size(), opt_.real);
        std::vector<VkContext::UploadItem> up = {
            {&bPoses_, poses.data(), poses.size()},
            {&bIntr_, intr.data(), intr.size()},
            {&bPoints_, points.data(), points.size()},
        };
        if (!P_.exts.empty()) up.push_back({&bExts_, exts.data(), exts.size()});
        ctx_.uploadMany(up.data(), up.size());
    }

    void downloadParams() {
        if (cpu_) return;
        std::vector<uint8_t> tmp(std::max({bPoses_.size, bIntr_.size, bPoints_.size, bExts_.size}));
        ctx_.download(bPoses_, tmp.data(), P_.poses.size() * realSize(opt_.real));
        unpackReals(P_.poses, tmp.data(), P_.poses.size(), opt_.real);
        ctx_.download(bIntr_, tmp.data(), P_.intr.size() * realSize(opt_.real));
        unpackReals(P_.intr, tmp.data(), P_.intr.size(), opt_.real);
        ctx_.download(bPoints_, tmp.data(), P_.points.size() * realSize(opt_.real));
        unpackReals(P_.points, tmp.data(), P_.points.size(), opt_.real);
        if (!P_.exts.empty()) {
            ctx_.download(bExts_, tmp.data(), P_.exts.size() * realSize(opt_.real));
            unpackReals(P_.exts, tmp.data(), P_.exts.size(), opt_.real);
        }
    }

    double computeCost() {
        if (cpu_) return cpu_->computeCost();
        VkCommandBuffer cb = ctx_.begin();
        recordCost(cb);
        ctx_.barrier(cb);
        ctx_.recordDownload(cb, bCost_, realSize(opt_.real), 0, kDlCost);
        ctx_.submit(cb);
        return readCost();
    }

    void solve() {
        if (cpu_) return cpu_->solve();
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

            LinSolve path = useCG_ ? LinSolve::CG : densePath_;
            VkCommandBuffer cb = ctx_.begin();
            recordIteration(cb, (float)damping, reuse, path);
            ctx_.submit(cb);
            double newCost = readCost();
            stats_.iterations = it + 1;

            if (path == LinSolve::CG) {
                bool conv;
                double cg_iters;
                readCgStatus(conv, cg_iters);
                stats_.cg_solves++;
                stats_.cg_iters_total += cg_iters;
                if (conv) {
                    consec_fallbacks = 0;
                    // adapt the recorded iteration cap to the observed count
                    cgMaxit_ = std::min<uint32_t>(
                        std::max<uint32_t>((uint32_t)(1.5 * cg_iters) + 8, 16),
                        (uint32_t)opt_.cg_max_iters);
                } else {
                    uint32_t usedCap = cgMaxit_;
                    cgMaxit_ = (uint32_t)opt_.cg_max_iters;
                    // a truncated-CG step is still a damped descent step; keep
                    // it if it improved the cost and only pay for the dense
                    // re-solve when the step would be rejected anyway
                    bool stepOk = std::isfinite(newCost) && newCost <= cost * (1.0 + opt_.rtol);
                    if (stepOk) consec_fallbacks = 0;
                    if (haveFallback_ && !stepOk) {
                        // discard the step and redo this iteration with the
                        // dense solver, reusing the assembly (reject flow)
                        if (opt_.verbose)
                            sfm::slog::diag(sfm::slog::Tag::Map,
                                       "iter %3d: CG hit %u-iteration cap, dense fallback",
                                       it, usedCap);
                        restore_pending_ = true;
                        cb = ctx_.begin();
                        recordIteration(cb, (float)damping, true, densePath_);
                        ctx_.submit(cb);
                        newCost = readCost();
                        stats_.cg_fallbacks++;
                        if (++consec_fallbacks >= 3) {
                            useCG_ = false;  // CG is not paying off; stay dense
                            stats_.solver = "cg->dense";
                            if (opt_.verbose)
                                sfm::slog::diag(sfm::slog::Tag::Map,
                                           "[vk] repeated CG stalls, switching to dense");
                        }
                    }
                }
            }

            if (std::isfinite(newCost) && newCost <= cost * (1.0 + opt_.rtol)) {
                if (newCost / cost >= 1.0 - opt_.rtol) {
                    // tie-zone accept: count toward patience and nudge lambda
                    // UP -- shrinking it on micro-improvements lets it
                    // collapse and the solver stall in an ill-conditioned
                    // plateau (observed with the df config on 871)
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
                // reject: restore parameters; the assembly snapshot stays valid
                restore_pending_ = true;
                if (!std::isfinite(newCost)) {
                    if (++noimprov >= opt_.patience) break;
                } else
                    noimprov = 0;
                // rejects are cheap (assembly is reused), so search lambda
                // finely at first, but escalate on consecutive rejects
                damping *= reject_mult;
                reject_mult = std::min(reject_mult * 2.0, 32.0);
                reuse = true;
            }
        }
        // The last iteration may have been rejected; what the caller downloads
        // must be the last *accepted* parameters.
        flushRestore();
        stats_.final_cost = cost;
        auto t1 = std::chrono::high_resolution_clock::now();
        stats_.solve_seconds = std::chrono::duration<double>(t1 - t0).count();
        ctx_.printProfile();
    }

    const SolverStats& stats() const { return cpu_ ? cpu_->stats() : stats_; }
    // The scalar type actually in use, which init() may have stepped down from
    // what SolverOptions asked for (see pickRealForDevice). Anything that packs
    // or unpacks solver buffers from outside has to ask -- packing `double`
    // into buffers a `df` kernel reads gives silent garbage, not an error.
    RealCfg real() const { return opt_.real; }
    // GPU-path handles; the host solver has no context and no device buffers.
    VkContext& ctx() { return ctx_; }
    GpuBuffer& bufS() { return bS_; }
    GpuBuffer& bufG() { return bG_; }

    // debug: run one full assembly (no factor/solve) so S and g can be dumped
    void debugAssemble(float damping) {
        if (cpu_) return cpu_->assembleOnly(damping);
        VkCommandBuffer cb = ctx_.begin();
        recordAssembly(cb, damping, false, densePath_);
        ctx_.submit(cb);
    }

    // debug: the assembled S (packed lower triangle) and g, from whichever path
    // ran, as doubles
    std::vector<double> debugPackedS() {
        if (cpu_) return cpu_->packedS();
        std::vector<uint8_t> raw(bS_.size);
        ctx_.download(bS_, raw.data(), bS_.size);
        std::vector<double> v;
        unpackReals(v, raw.data(), (uint64_t)P_.n_dim * (P_.n_dim + 1) / 2, opt_.real);
        return v;
    }
    std::vector<double> debugG() { return cpu_ ? cpu_->gradient() : downloadG(); }

    // debug: solve the same assembly with both paths and report the step
    // difference (requires the cg+fallback configuration)
    double debugCompareStep(float damping) {
        if (cpu_) return cpu_->compareStep(damping);
        if (!useCG_ || !haveFallback_)
            throw std::runtime_error("step comparison needs --solver cg + fallback on");
        VkCommandBuffer cb = ctx_.begin();
        recordAssembly(cb, damping, false, LinSolve::CG);
        recordPCG(cb, (uint32_t)opt_.cg_max_iters);
        ctx_.barrier(cb);
        ctx_.recordDownload(cb, bCgScal_, 8 * realSize(opt_.real), 0, kDlCgScal);
        ctx_.submit(cb);
        std::vector<double> xcg = downloadG();
        bool conv;
        double cg_iters;
        readCgStatus(conv, cg_iters);
        cb = ctx_.begin();
        recordAssembly(cb, damping, true, densePath_);  // reuse the same assembly
        recordCholesky(cb);
        ctx_.submit(cb);
        std::vector<double> xd = downloadG();
        double dmax = 0, xmax = 0;
        for (uint32_t i = 0; i < P_.n_dim; i++) {
            dmax = std::max(dmax, std::fabs(xcg[i] - xd[i]));
            xmax = std::max(xmax, std::fabs(xd[i]));
        }
        double rel = dmax / std::max(xmax, 1e-300);
        sfm::slog::diag(sfm::slog::Tag::Map,
                        "cmp-step lambda=%g: cg %s in %.0f iters, "
                        "|dx_cg - dx_dense|_inf/|dx|_inf = %.3e",
                        damping, conv ? "converged" : "hit cap", cg_iters, rel);
        return rel;
    }

    // record the in-place dense factor + triangular solves (public for
    // selftest). chol_update also factors the next diagonal tile, so
    // chol_diag proper only runs for the first block; the triangular solves
    // are one fused dispatch per block (see cholesky.slang).
    void recordCholesky(VkCommandBuffer cb) {
        const uint32_t n = P_.n_dim, bs = 32;
        const uint32_t nb = (n + bs - 1) / bs;
        Push p;
        p.u0 = n;
        p.u1 = 0;
        ctx_.dispatch(cb, "chol_diag", 1, p);
        ctx_.barrier(cb);
        for (uint32_t k = 0; k + 1 < nb; k++) {
            p.u1 = k;
            uint32_t below = nb - 1 - k;
            ctx_.dispatch(cb, "chol_panel", below, p);
            ctx_.barrier(cb);
            p.u2 = below * (below + 1) / 2;
            ctx_.dispatch(cb, "chol_update", p.u2, p);
            ctx_.barrier(cb);
        }
        for (uint32_t k = 0; k < nb; k++) {
            p.u1 = k;
            p.u2 = (k + 1) * bs < n ? n - (k + 1) * bs : 0;
            ctx_.dispatch(cb, "tri_fwd", std::max(1u, (p.u2 + 255) / 256), p);
            ctx_.barrier(cb);
        }
        for (int k = (int)nb - 1; k >= 0; k--) {
            p.u1 = (uint32_t)k;
            p.u2 = (uint32_t)k * bs;
            ctx_.dispatch(cb, "tri_bwd", std::max(1u, (p.u2 + 255) / 256), p);
            ctx_.barrier(cb);
        }
    }

private:
    // The dof tiers the kernels are built at (ba.slang): the Schur kernels at
    // four, the CG ones at two. A problem pays only for the widest camera it
    // uses, and only a refined member extrinsic reaches the rig tier.
    void pickTiers() {
        uint32_t maxDof = 0;
        for (uint32_t i = 0; i < P_.num_images; i++)
            maxDof = std::max(maxDof, 6 + P_.memberFree(i) + P_.groups[P_.image_group[i]].n_intr);
        schurSuffix_ = maxDof <= 12 ? "_c" : maxDof <= 14 ? "_m" : maxDof <= 18 ? "_w" : "_x";
        cgSuffix_ = maxDof <= 18 ? "_w" : "_x";
        const uint32_t tier = maxDof <= 18 ? 18 : 24;
        bBlk_ = tier * (tier + 1) / 2;
    }

    static std::string costEntry(const BAProblem::ModelRange& mr) {
        return std::string(kModels[mr.model].cost_entry) + (mr.rig ? "_rig" : "");
    }
    static std::string jacEntry(const BAProblem::ModelRange& mr) {
        return std::string(kModels[mr.model].jac_entry) + (mr.rig ? "_rig" : "");
    }

    // Choose the linear solver path from the problem shape, the options and
    // the VRAM budget, and build the host-side tables the choice needs.
    void decidePaths() {
        pickTiers();
        const bool exclusive = exclusiveGroups(P_);
        const uint64_t packed = (uint64_t)P_.n_dim * (P_.n_dim + 1) / 2;
        const bool denseOk = packed <= 0x7FFFFFFFull;  // 32-bit packed indexing
        const uint64_t pairEntries = pairEntryCount(P_);
        const bool cgOk = P_.num_obs > 0;
        const double budget = opt_.vram_budget_mb > 0 ? opt_.vram_budget_mb
                                                      : 0.9 * ctx_.deviceLocalHeapMB();
        // The pair-aggregated Schur assembly is the faster of the two dense
        // assemblies but the only one that costs memory (the entry lists and
        // the per-observation Y). Treat it as what it is -- an optional
        // accelerator -- so a dense problem that no longer fits with it
        // degrades to the atomic kernel instead of jumping to CG.
        const double denseObsMB = estimateMB(true, false, false, 0);
        const double densePairMB = estimateMB(true, false, true, pairEntries);
        const bool pairOk = exclusive && P_.num_obs > 0 && pairEntries <= kMaxPairEntries &&
                            densePairMB <= budget;
        const double denseMB = pairOk ? densePairMB : denseObsMB;
        const double cgMB = estimateMB(false, true, false, 0);
        const double bothMB = estimateMB(true, true, pairOk, pairEntries);

        const uint32_t kDenseMaxDim = 8192;

        switch (opt_.solver) {
            case SolverSel::Dense:
                useCG_ = false;
                if (!denseOk)
                    throw std::runtime_error(
                        "reduced system too large for the dense solver (n_dim*(n_dim+1)/2 "
                        "exceeds 32-bit packed indexing); use --solver cg");
                break;
            case SolverSel::CG:
                useCG_ = cgOk;
                if (!cgOk) {
                    sfm::slog::diag(sfm::slog::Tag::Map,
                               "[vk] warning: no observations, falling back to dense");
                    if (!denseOk) throw std::runtime_error("no usable solver path");
                }
                break;
            case SolverSel::Auto:
                useCG_ = cgOk && (P_.n_dim > kDenseMaxDim || !denseOk || denseMB > budget);
                if (!useCG_ && !denseOk)
                    throw std::runtime_error("reduced system too large for the dense solver");
                break;
        }

        haveFallback_ = false;
        if (useCG_) {
            bool want = opt_.cg_fallback == CgFallback::On ||
                        (opt_.cg_fallback == CgFallback::Auto && bothMB <= 0.5 * budget);
            haveFallback_ = want && pairOk && denseOk;
            if (opt_.cg_fallback == CgFallback::On && !haveFallback_)
                sfm::slog::diag(sfm::slog::Tag::Map, "[vk] warning: dense fallback unavailable "
                           "(pair-Schur or packed-index limits)");
        }

        if (opt_.verbose)
            sfm::slog::diag(sfm::slog::Tag::Map,
                       "[vk] VRAM estimates: dense %.0f MB (%s Schur), cg %.0f MB (budget %.0f MB)",
                       denseMB, pairOk ? "pair" : "per-obs", cgMB, budget);
        // Say so before the driver does. There is nothing below CG to fall back
        // to -- its footprint is the problem data plus a few vectors -- so this
        // is the point at which the answer is a smaller problem or more memory.
        const double needMB = (useCG_ ? cgMB : denseMB) + (haveFallback_ ? denseMB : 0);
        if (needMB > budget) {
            if (opt_.over_budget_throws) throw BAOverBudget(needMB, budget);
            sfm::slog::diag(sfm::slog::Tag::Map,
                       "[vk] warning: the %s solver needs ~%.0f MB and the budget is %.0f MB; "
                       "this may run out of device memory",
                       useCG_ ? "cg" : "dense", needMB, budget);
        }

        // host tables for the chosen paths
        P_.use_pair_schur = false;
        if ((!useCG_ || haveFallback_) && pairOk) buildPairTables(P_);
        densePath_ = P_.use_pair_schur ? LinSolve::DensePair : LinSolve::DenseObs;
        cgAllocated_ = useCG_;
        if (cgAllocated_) {
            buildCamTables(P_);
            buildPrecBlocks(P_, exclusive);
        }
        cgMaxit_ = (uint32_t)opt_.cg_max_iters;
    }

    // device-buffer footprint of a path combination, in MB (mirrors init())
    double estimateMB(bool withDense, bool withCG, bool pairTables, uint64_t pairEntries) const {
        const double rs = (double)realSize(opt_.real);
        const double n = P_.n_dim, no = P_.num_obs, np = P_.num_points, ni = P_.num_images;
        const double packed = n * (n + 1) / 2;
        double b = 0;
        b += no * (2 * rs + 12) + 4 * (double)P_.model_obs.size();  // obs + index tables
        b += 16 * ni + 16 * (double)(P_.groups.size() + P_.members.size());
        b += 2 * (P_.pose_dim + P_.exts.size() + P_.total_intr) * rs;  // params + backups
        b += 3 * np * rs * 3;                                      // points, backup, Bp0
        b += 4 * (np + 1);
        b += ((double)P_.jc_total + 8 * no) * rs;                  // Jc, Jp, res
        b += (9 + 3 + 9) * np * rs;                                // App, Bp, W
        b += 2 * n * rs;                                           // g, y
        if (withDense) {
            b += packed * rs;
            if (pairTables)
                b += 8.0 * pairEntries * 1.01 + 6 * no * rs;       // pair entries + Y
        }
        if (withCG)
            b += (4 * n + 3 * np + ni * (double)bBlk_ +
                  (ni + (double)P_.members.size() + (double)P_.groups.size()) * kCamBlk) * rs +
                 4 * (ni + 1) + 4 * no + 12 * (no / 1024 + ni) +
                 16 * (ni + (double)P_.groups.size());  // chunk + prec-block tables
        return b / (1024.0 * 1024.0);
    }

    void recordCost(VkCommandBuffer cb) {
        ctx_.fillZero(cb, bCost_);
        ctx_.barrier(cb);
        for (auto& mr : P_.model_ranges) {
            Push p;
            p.u0 = mr.count;
            p.u1 = mr.offset;
            p.f0 = opt_.loss_param;
            ctx_.dispatch(cb, costEntry(mr), (mr.count + 255) / 256, p);
        }
        ctx_.barrier(cb);
    }

    void recordAssembly(VkCommandBuffer cb, float damping, bool reuse, LinSolve path) {
        const bool dense = path != LinSolve::CG;
        if (reuse) {
            // Params were restored after a reject; the per-observation
            // Jacobians (Jc/Jp/res/App) still match them, so skip the Jacobian
            // pass. S and g are rebuilt from those by the Schur kernels -- on
            // every path, which is why none of them is snapshotted. Bp is the
            // one thing the back-substitution overwrote, so it is.
            ctx_.copy(cb, bBp0_, bBp_, bBp_.size);
            if (dense) {
                ctx_.fillZero(cb, bS_);
                ctx_.fillZero(cb, bG_);
            }
            ctx_.barrier(cb);
        } else {
            // backup params for possible reject
            ctx_.copy(cb, bPoses_, bPosesBak_, bPoses_.size);
            ctx_.copy(cb, bIntr_, bIntrBak_, bIntr_.size);
            ctx_.copy(cb, bPoints_, bPointsBak_, bPoints_.size);
            if (!P_.exts.empty()) ctx_.copy(cb, bExts_, bExtsBak_, bExts_.size);

            if (dense) {
                ctx_.fillZero(cb, bS_);
                ctx_.fillZero(cb, bG_);
            }
            ctx_.fillZero(cb, bApp_);
            ctx_.fillZero(cb, bBp_);
            ctx_.barrier(cb);

            for (auto& mr : P_.model_ranges) {
                Push p;
                p.u0 = mr.count;
                p.u1 = mr.offset;
                p.f0 = opt_.loss_param;
                ctx_.dispatch(cb, jacEntry(mr), (mr.count + 127) / 128, p);
            }
            ctx_.barrier(cb);

            ctx_.copy(cb, bBp_, bBp0_, bBp_.size);
            ctx_.barrier(cb);
        }

        {
            Push q;
            q.u0 = P_.num_points;
            q.f0 = damping;
            ctx_.dispatch(cb, "point_prep", (P_.num_points + 255) / 256, q);
            if (path == LinSolve::CG) {  // chunked cg_cam_diag accumulates atomically
                ctx_.fillZero(cb, bCgB_);
                ctx_.fillZero(cb, bCgM_);
                ctx_.fillZero(cb, bG_);
            }
        }
        ctx_.barrier(cb);

        {
            Push p;
            p.f0 = damping;
            if (path == LinSolve::DensePair) {
                p.u0 = P_.num_obs;
                ctx_.dispatch(cb, "y_prep", (P_.num_obs + 255) / 256, p);
                ctx_.barrier(cb);
                p.u0 = P_.num_pair_chunks;
                ctx_.dispatch(cb, std::string("schur_pair") + schurSuffix_, P_.num_pair_chunks, p);
            } else if (path == LinSolve::DenseObs) {
                p.u0 = P_.num_obs;
                ctx_.dispatch(cb, std::string("schur_obs") + schurSuffix_, (P_.num_obs + 127) / 128, p);
            } else {
                p.u0 = P_.num_cam_chunks;
                p.u1 = P_.prec_exclusive ? 1 : 0;
                p.u2 = P_.num_frames;  // member blocks follow the frame ones, then groups
                p.u3 = P_.num_frames + (uint32_t)P_.members.size();
                ctx_.dispatch(cb, std::string("cg_cam_diag") + cgSuffix_, P_.num_cam_chunks, p);
                ctx_.barrier(cb);
                p.u0 = P_.num_prec_blocks;
                ctx_.dispatch(cb, "cg_prec_fact", (P_.num_prec_blocks + 255) / 256, p);
            }
        }
        ctx_.barrier(cb);
    }

    // Record the device-side PCG loop (see cg.slang). A fixed iteration count
    // is recorded; every kernel no-ops once the convergence flag is set.
    void recordPCG(VkCommandBuffer cb, uint32_t maxit) {
        const uint32_t n = P_.n_dim;
        const uint32_t ng = (n + 255) / 256;
        const uint32_t npart = ng;
        const uint32_t nib = (P_.num_prec_blocks + 255) / 256;
        // Shared columns: cg_bmul accumulates into them instead of storing, so
        // they must start at zero -- the tail past the poses (members and
        // groups), or the whole vector when rig frames are shared too.
        const bool rigs = P_.hasRigs();
        const VkDeviceSize intrOff = rigs ? 0 : (VkDeviceSize)P_.pose_dim * realSize(opt_.real);
        const VkDeviceSize intrSize =
            (VkDeviceSize)(rigs ? n : n - P_.pose_dim) * realSize(opt_.real);
        const bool zeroIntr = !P_.prec_exclusive && intrSize > 0;
        Push pn;
        pn.u0 = n;
        ctx_.dispatch(cb, "cg_init", ng, pn);
        ctx_.barrier(cb);
        Push pc;
        pc.u0 = P_.num_prec_blocks;
        pc.u1 = 0;  // flag was just cleared
        ctx_.dispatch(cb, "cg_prec_apply", nib, pc);
        ctx_.barrier(cb);
        Push pr;
        pr.u0 = n;
        pr.u1 = 1;
        pr.u2 = npart;
        pr.u3 = 0;
        ctx_.dispatch(cb, "cg_red2", ng, pr);
        ctx_.barrier(cb);
        Push pf;
        pf.u0 = npart;
        pf.u1 = 0;
        pf.u2 = npart;
        pf.f0 = (float)opt_.cg_tol;
        ctx_.dispatch(cb, "cg_fin", 1, pf);
        ctx_.barrier(cb);
        ctx_.dispatch(cb, "cg_copy", ng, pn);
        ctx_.barrier(cb);
        pc.u1 = 1;
        pr.u3 = 1;
        for (uint32_t it = 0; it < maxit; it++) {
            Push pg;
            pg.u0 = P_.num_points;
            ctx_.dispatch(cb, std::string("cg_gather") + cgSuffix_, (P_.num_points + 255) / 256, pg);
            ctx_.barrier(cb);
            Push ps;
            ps.u0 = P_.num_images;
            ps.u1 = P_.prec_exclusive ? 1 : 0;
            if (zeroIntr) {
                ctx_.fillZero(cb, bCgSp_, intrOff, intrSize);
                ctx_.barrier(cb);
            }
            ctx_.dispatch(cb, std::string("cg_bmul") + cgSuffix_, P_.num_images, ps);
            ctx_.barrier(cb);
            ps.u0 = P_.num_cam_chunks;
            ctx_.dispatch(cb, std::string("cg_scatter") + cgSuffix_, P_.num_cam_chunks, ps);
            ctx_.barrier(cb);
            pr.u1 = 0;
            ctx_.dispatch(cb, "cg_red2", ng, pr);
            ctx_.barrier(cb);
            pf.u1 = 1;
            ctx_.dispatch(cb, "cg_fin", 1, pf);
            ctx_.barrier(cb);
            ctx_.dispatch(cb, "cg_axpy", ng, pn);
            ctx_.barrier(cb);
            ctx_.dispatch(cb, "cg_prec_apply", nib, pc);
            ctx_.barrier(cb);
            pr.u1 = 1;
            ctx_.dispatch(cb, "cg_red2", ng, pr);
            ctx_.barrier(cb);
            pf.u1 = 2;
            ctx_.dispatch(cb, "cg_fin", 1, pf);
            ctx_.barrier(cb);
            ctx_.dispatch(cb, "cg_updp", ng, pn);
            ctx_.barrier(cb);
        }
    }

    // Put the parameters back where the last accepted step left them. A reject
    // (and the CG fallback, which is a reject that retries) needs this before
    // anything else touches them -- but it is three buffer copies, and a submit
    // of its own costs a fence round trip on a device several solvers are
    // sharing. So the reject only *marks* it, and the next command buffer to be
    // recorded carries it. Nothing runs in between: the LM loop either records
    // another iteration or leaves, and leaving flushes it (see solve()).
    void recordRestore(VkCommandBuffer cb) {
        ctx_.copy(cb, bPosesBak_, bPoses_, bPoses_.size);
        ctx_.copy(cb, bIntrBak_, bIntr_, bIntr_.size);
        ctx_.copy(cb, bPointsBak_, bPoints_, bPoints_.size);
        if (!P_.exts.empty()) ctx_.copy(cb, bExtsBak_, bExts_, bExts_.size);
        ctx_.barrier(cb);
    }

    // Emit a marked restore on its own, for the one caller that cannot defer:
    // the loop is over and the parameters are about to be read back.
    void flushRestore() {
        if (!restore_pending_) return;
        restore_pending_ = false;
        VkCommandBuffer cb = ctx_.begin();
        recordRestore(cb);
        ctx_.submit(cb);
    }

    void recordIteration(VkCommandBuffer cb, float damping, bool reuse, LinSolve path) {
        if (restore_pending_) {
            restore_pending_ = false;
            recordRestore(cb);
        }
        recordAssembly(cb, damping, reuse, path);

        if (path == LinSolve::CG)
            recordPCG(cb, cgMaxit_);
        else
            recordCholesky(cb);

        {
            Push p;
            p.u0 = P_.num_obs;
            ctx_.dispatch(cb, std::string("dp_accum") + schurSuffix_, (P_.num_obs + 255) / 256, p);
        }
        ctx_.barrier(cb);

        {
            Push p;
            p.u0 = P_.num_points;
            ctx_.dispatch(cb, "point_update", (P_.num_points + 255) / 256, p);
            Push q;
            q.u0 = P_.pose_dim;
            ctx_.dispatch(cb, "cam_update", (P_.pose_dim + 255) / 256, q);
            if (!P_.members.empty()) {
                Push e;
                e.u0 = (uint32_t)P_.members.size();
                ctx_.dispatch(cb, "ext_update", ((uint32_t)P_.members.size() + 63) / 64, e);
            }
            if (!P_.groups.empty()) {
                Push r;
                r.u0 = (uint32_t)P_.groups.size();
                ctx_.dispatch(cb, "intr_update", ((uint32_t)P_.groups.size() + 63) / 64, r);
            }
        }
        ctx_.barrier(cb);

        recordCost(cb);
        // Fold the two readbacks the LM loop needs into this command buffer.
        // A separate download() is its own fenced submit, so taking them here
        // halves the submits per iteration -- and a submit's latency, not its
        // arithmetic, is what a forty-image solve costs. The atom phase of a
        // bottom-up run spends five thousand iterations on problems that size,
        // with several solvers sharing the device.
        ctx_.barrier(cb);
        ctx_.recordDownload(cb, bCost_, realSize(opt_.real), 0, kDlCost);
        if (path == LinSolve::CG)
            ctx_.recordDownload(cb, bCgScal_, 8 * realSize(opt_.real), 0, kDlCgScal);
    }

    // Offsets into the download staging buffer for the folded readbacks above.
    static constexpr VkDeviceSize kDlCost = 0, kDlCgScal = 64;

    double readCost() {
        std::vector<double> v;
        unpackReals(v, (const uint8_t*)ctx_.stagingDownloadPtr() + kDlCost, 1, opt_.real);
        return v[0];
    }

    void readCgStatus(bool& converged, double& iters) {
        std::vector<double> v;
        unpackReals(v, (const uint8_t*)ctx_.stagingDownloadPtr() + kDlCgScal, 8, opt_.real);
        // flag set AND tolerance met (flag alone can also mean breakdown;
        // flag unset means the recorded iteration cap was hit)
        converged = v[6] > 0.5 && v[4] <= v[5];
        iters = v[7];
    }

    std::vector<double> downloadG() {
        std::vector<uint8_t> raw(P_.n_dim * realSize(opt_.real));
        ctx_.download(bG_, raw.data(), raw.size());
        std::vector<double> v;
        unpackReals(v, raw.data(), P_.n_dim, opt_.real);
        return v;
    }

    BAProblem& P_;
    SolverOptions opt_;
    std::unique_ptr<bacpu::Solver> cpu_;  // non-null when running on the host
    const char* schurSuffix_ = "_c";  // dof tier of the Schur kernels (pickTiers)
    const char* cgSuffix_ = "_w";     // ... and of the CG ones
    uint32_t bBlk_ = kCamBlk;         // per-image B block stride at that tier
    SolverStats stats_;
    std::unique_ptr<VkContext> owned_;      // null when running on a shared context
    VkContext& ctx_;
    std::vector<GpuBuffer*> ownBufs_;   // this instance's buffers (freed in dtor if shared)

    LinSolve densePath_ = LinSolve::DenseObs;
    bool useCG_ = false;       // CG is the active path (may demote to dense)
    bool cgAllocated_ = false; // CG buffers/tables exist
    bool haveFallback_ = false;
    uint32_t cgMaxit_ = 100;

    GpuBuffer bObs_, bObsImage_, bObsPoint_, bImageInfo_, bGroupInfo_, bMemberInfo_;
    GpuBuffer bPoses_, bExts_, bIntr_, bPoints_, bObsRanges_, bModelObs_, bJcOff_;
    GpuBuffer bJp_, bS_, bG_, bApp_, bBp_, bCost_;
    GpuBuffer bPosesBak_, bExtsBak_, bIntrBak_, bPointsBak_;
    bool restore_pending_ = false;  // a rejected step's parameters are still live
    GpuBuffer bBp0_, bJc_, bRes_;
    GpuBuffer bPairEntries_, bPairChunks_, bW_, bYp_, bY_;
    GpuBuffer bCamRanges_, bCamObs_, bCamChunks_, bCgR_, bCgZ_, bCgP_, bCgSp_;
    GpuBuffer bCgV_, bCgB_, bCgM_, bCgScal_, bCgPart_, bPrecBlocks_;
};
