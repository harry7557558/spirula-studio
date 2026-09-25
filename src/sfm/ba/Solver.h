// Levenberg-Marquardt driver: owns GPU buffers, records each iteration
// (assembly -> dense Cholesky or implicit-Schur PCG -> updates) and runs the
// accept/reject loop on the host. An iteration goes to the device in as many
// submits as the watchdog budget needs (core/SubmitBudget.h), so a large
// problem on a slow GPU does not lose the device. Path selection, VRAM budget
// and the dense fallback: sfm/ba/README.md. A device that can run none of the
// scalar configurations gets `RealCfg::CPU`, and every entry point delegates
// to bacpu::Solver -- the same LM loop and linear solvers, on the host.
#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <memory>
#include <vector>

#include "core/SubmitBudget.h"
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

// Cache capabilities by canonical UUID; ordinals may change between enumerations.
inline const VkDeviceCaps& cachedDeviceCaps(const std::string& selector) {
    static std::mutex m;
    static std::map<std::string, VkDeviceCaps> cache;  // node-based: references stay valid
    std::lock_guard<std::mutex> g(m);
    auto it = cache.find(selector);
    if (it == cache.end())
        it = cache.emplace(selector, VkContext::probeCaps(deviceOnlyOpt(-1, selector))).first;
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
        // An explicit request resolves even against a live context and never
        // falls back to CPU or capability; only an implicit one may.
        const bool explicit_req =
            !opt_.device_selector.empty() || opt_.device >= 0;
        const bool validate_request =
            explicit_req || (opt_.real != RealCfg::CPU && !ctx_.initialized());
        if (validate_request) {
            const spirula::vkselect::Resolution& res =
                VkContext::cachedResolution(
                    deviceOnlyOpt(opt_.device, opt_.device_selector));
            if (res.ok())
                selector_ = res.selector;
            else if (explicit_req ||
                     res.status != spirula::vkselect::ResolveStatus::NoDevice)
                throw std::runtime_error(res.error);
        }
        // The identity is what a shared context is; an ordinal says nothing
        // across instances. Refuse before any capability choice or allocation.
        if (ctx_.initialized() && !selector_.empty() &&
            selector_ != ctx_.selector())
            throw std::runtime_error("requested device " + selector_ +
                                     " is not the live device " + ctx_.selector());
        if (opt_.real != RealCfg::CPU) {
            static const VkDeviceCaps kNoDevice{};
            const VkDeviceCaps& caps =
                ctx_.initialized() ? ctx_.caps()
                : selector_.empty() ? kNoDevice
                                    : cachedDeviceCaps(selector_);
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
        vopt.selector = selector_;      // canonical identity; empty = the shared precedence
        vopt.deviceIndex = opt_.device; // legacy ordinal, only when no UUID was resolved
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
        const uint64_t packedTc = (uint64_t)tcN_ * (tcN_ + 1) / 2;
        bS_ = mkReal(std::max<uint64_t>(needS ? packed : 1, packedTc + 42 * (uint64_t)P_.num_frames));
        bG_ = mkReal(P_.n_dim);
        bApp_ = mkReal(9 * (uint64_t)P_.num_points);
        bBp_ = mkReal(3 * (uint64_t)P_.num_points);
        bCost_ = mkReal(4);
        bPosesBak_ = mkReal(P_.pose_dim);
        bExtsBak_ = mkReal(P_.exts.size());
        bIntrBak_ = mkReal(P_.total_intr);
        bPointsBak_ = mkReal(3 * (uint64_t)P_.num_points);
        bPairEntries_ = mkUint(std::max<size_t>(P_.pair_entries.size() + tcEnt_.size(), 2));
        bPairChunks_ = mkUint(std::max<size_t>(P_.pair_chunks.size() + tcChk_.size(), 2));
        // S/g are rebuilt from the per-observation Jacobians on every path, so
        // a rejected step needs no snapshot of them -- only Bp, which the
        // point back-substitution overwrites in place.
        bBp0_ = mkReal(3 * (uint64_t)P_.num_points);
        bJc_ = mkReal(P_.jc_total);
        bRes_ = mkReal(2 * (uint64_t)P_.num_obs);
        bW_ = mkReal(9 * (uint64_t)P_.num_points);
        bYp_ = mkReal(needS && P_.use_pair_schur ? 6 * (uint64_t)P_.num_obs : 1);
        bY_ = mkReal(std::max<uint64_t>(P_.n_dim, 34 * (uint64_t)tcN_));
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
        bCgScal_ = mkReal(16);
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
        if (tcN_) {
            const char* tc[] = {"tc_basis", "tc_schur", "tc_reg", "tc_dinv", "tc_inv_row",
                                "tc_inv_copy", "tc_restrict", "tc_lmul", "tc_ltmul",
                                "tc_prolong", "chol_diag", "chol_panel", "chol_update"};
            entries.insert(entries.end(), std::begin(tc), std::end(tc));
            entries.push_back(std::string("tc_bpart") + cgSuffix_);
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
            mi.push_back(m.mask);
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
        if (P_.use_pair_schur || tcN_) {
            tcEnt_.insert(tcEnt_.begin(), P_.pair_entries.begin(), P_.pair_entries.end());
            tcChk_.insert(tcChk_.begin(), P_.pair_chunks.begin(), P_.pair_chunks.end());
            up.push_back({&bPairEntries_, tcEnt_.data(), tcEnt_.size() * 4});
            up.push_back({&bPairChunks_, tcChk_.data(), tcChk_.size() * 4});
        }
        struct Release {
            std::vector<uint32_t>& a;
            std::vector<uint32_t>& b;
            ~Release() { a = {}; b = {}; }
        } release{tcEnt_, tcChk_};
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
        beginSeg();
        recordCost();
        ctx_.barrier(cb_);
        ctx_.recordDownload(cb_, bCost_, realSize(opt_.real), 0, kDlCost);
        endSeg();
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
        auto last_ckpt = std::chrono::steady_clock::now();
        for (int it = 0; it < opt_.max_iters; it++) {
            sfm::cancel::check();
            if (opt_.verbose)
                sfm::slog::diag(sfm::slog::Tag::Map, "iter %3d: cost = %.9e, damping = %.3g%s", it,
                                cost,
                           damping,
                           reuse ? " (reuse)" : "");

            LinSolve path = useCG_ ? LinSolve::CG : densePath_;
            // A few CG iterations do not repay building A_c, and a stale one
            // only costs iterations: it is rebuilt every third solve, or once
            // the damping has moved tenfold.
            tcUse_ = tcN_ && !tcOff_ && lastCg_ > kTcMinIters;
            const bool tcBuild = tcUse_ && (!tcHave_ || tcAge_ >= 2 ||
                                            std::fabs(std::log(damping / tcLambda_)) > std::log(10.0));
            tcBuild_ = tcBuild;
            if (tcBuild) {
                tcHave_ = true;
                tcAge_ = 0;
                tcLambda_ = damping;
            } else if (tcUse_) {
                tcAge_++;
            }
            beginSeg();
            recordIteration((float)damping, reuse, path);
            endSeg();
            double newCost = readCost();
            stats_.iterations = it + 1;

            if (path == LinSolve::CG) {
                bool conv;
                double cg_iters;
                readCgStatus(conv, cg_iters);
                // CG stopped before its first step (r.z or p.Sp not positive):
                // retried without A_c, then a failed step that raises the damping
                // -- S went indefinite by rounding below ~1e-9 on Gram blocks of 1e23.
                if (cg_iters == 0 && !conv && tcUse_) {
                    tcUse_ = tcBuild_ = false;
                    restore_pending_ = true;
                    beginSeg();
                    recordIteration((float)damping, true, path);
                    endSeg();
                    newCost = readCost();
                    readCgStatus(conv, cg_iters);
                    if (cg_iters > 0 || conv) {
                        tcOff_ = true;
                        sfm::slog::diag(sfm::slog::Tag::Map,
                                        "[vk] coarse correction broke down; continuing without it");
                    }
                }
                if (cg_iters == 0 && !conv) newCost = std::numeric_limits<double>::infinity();
                stats_.cg_solves++;
                stats_.cg_iters_total += cg_iters;
                lastCg_ = conv ? cg_iters : 1e9;
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
                        beginSeg();
                        recordIteration((float)damping, true, densePath_);
                        endSeg();
                        newCost = readCost();
                        tcHave_ = false;  // the dense solve reused u_S
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
                    damping = std::max(damping / 3.0, kMinDamping);
                }
                cost = newCost;
                stats_.accepted++;
                reuse = false;
                reject_mult = 2.0;
                // 5 s of progress costs one parameter download (~20 ms for
                // 4M points); an iteration of a small solve never reaches it.
                const auto now = std::chrono::steady_clock::now();
                if (opt_.checkpoint && now - last_ckpt > std::chrono::seconds(5)) {
                    downloadParams();
                    *opt_.checkpoint = {it + 1, damping, cost};
                    last_ckpt = now;
                }
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
        beginSeg();
        recordAssembly(damping, false, densePath_);
        endSeg();
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
        beginSeg();
        recordAssembly(damping, false, LinSolve::CG);
        recordPCG((uint32_t)opt_.cg_max_iters);
        ctx_.barrier(cb_);
        ctx_.recordDownload(cb_, bCgScal_, 8 * realSize(opt_.real), 0, kDlCgScal);
        endSeg();
        std::vector<double> xcg = downloadG();
        bool conv;
        double cg_iters;
        readCgStatus(conv, cg_iters);
        beginSeg();
        recordAssembly(damping, true, densePath_);  // reuse the same assembly
        recordCholesky();
        endSeg();
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

    // Factor the packed S in place and solve against g (sfm_cholesky_test).
    void cholesky() {
        beginSeg();
        recordCholesky();
        endSeg();
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
        wide_ = std::max(maxDof, 6u) / 24.0;
        double t1 = 0, t2 = 0;
        for (uint32_t p = 0; p < P_.num_points; p++) {
            const double t = P_.obs_ranges[p + 1] - P_.obs_ranges[p];
            t1 += t;
            t2 += t * t;
        }
        if (t1 > 0) meanTrackT_ = t2 / t1;
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
        double cgMB = estimateMB(false, true, false, 0);
        double bothMB = estimateMB(true, true, pairOk, pairEntries);

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
                useCG_ = cgOk && (P_.n_dim > kDenseMaxDim || !denseOk || denseMB > budget ||
                                  cgWork() < 0.5 * denseWork(pairOk, pairEntries));
                if (!useCG_ && !denseOk)
                    throw std::runtime_error("reduced system too large for the dense solver");
                break;
        }

        if (useCG_ && ::planCoarse(P_, kTcMaxDim, tcK_, tcN_, tcEntries_)) {
            cgMB = estimateMB(false, true, false, 0);
            bothMB = estimateMB(true, true, pairOk, pairEntries);
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
        if (tcN_) {
            // Chunks of at most 128 entries of one cluster pair, their offsets
            // past the pair-Schur entries they follow on the device.
            std::vector<uint32_t> key;
            buildCoarseEntries(P_, tcK_, tcEnt_, key);
            const uint32_t base = (uint32_t)(P_.pair_entries.size() / 2);
            tcChk_.clear();
            for (size_t kk = 0; kk + 1 < key.size(); kk++)
                for (uint32_t o = key[kk]; o < key[kk + 1]; o += 128) {
                    tcChk_.push_back(base + o);
                    tcChk_.push_back(std::min(128u, key[kk + 1] - o));
                }
            tcChunks_ = (uint32_t)(tcChk_.size() / 2);
        }
    }

    static constexpr uint32_t kTcMaxDim = 4096;

    // One LM iteration of each path in the budget's units. The dense assembly
    // is quadratic in track length: a 1068-image capture whose tracks average
    // 70 observations spent 3.7 s an iteration there, against 0.4 s on CG.
    double denseWork(bool pair, uint64_t pairEntries) const {
        const double n = P_.n_dim;
        const double schur = pair ? kWPairEntry * wide_ * wide_ * (double)pairEntries
                                  : kWSchurObs * wide_ * wide_ * meanTrackT_ / kSchurObsT *
                                        P_.num_obs;
        return schur + kWFlop * 2 * n * n * n / 3 + kWJac * wide_ * P_.num_obs;
    }
    double cgWork() const {
        return kWJac * wide_ * P_.num_obs + kWCamDiag * wide_ * wide_ * P_.num_obs +
               kCgItersGuess * (kWGather + kWScatter) * wide_ * P_.num_obs;
    }
    static constexpr double kCgItersGuess = 40;

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
        if (withCG && tcN_) {
            const double tn = tcN_, tc = tn * (tn + 1) / 2 + 42.0 * P_.num_frames;
            b += (std::max(withDense ? packed : 0.0, tc) - (withDense ? packed : 0.0)) * rs;
            b += std::max(0.0, 34.0 * tn - n) * rs + 8.0 * tcEntries_ * 1.01;
        }
        if (withCG)
            b += (4 * n + 3 * np + ni * (double)bBlk_ +
                  (ni + (double)P_.members.size() + (double)P_.groups.size()) * kCamBlk) * rs +
                 4 * (ni + 1) + 4 * no + 12 * (no / 1024 + ni) +
                 16 * (ni + (double)P_.groups.size());  // chunk + prec-block tables
        return b / (1024.0 * 1024.0);
    }

    // ---- submit budget ----

    // Launch costs in the budget's unit, about a nanosecond of an RTX 5070 at
    // fp64 with the 24-wide rig camera block (--profile, 6946-image capture);
    // wide_ scales the camera-block kernels to a narrower tier.
    static constexpr double kWLaunch = 2000, kWPoint = 0.25, kWVec = 1, kWImage = 5;
    static constexpr double kWCost = 1.5, kWJac = 5.5, kWDp = 0.6, kWYPrep = 0.3;
    static constexpr double kWCamDiag = 12.7, kWGather = 0.65, kWScatter = 1.2;
    static constexpr double kWSchurObs = 40, kWPairEntry = 3, kWFlop = 2.6e-3, kWTcSchur = 6;
    // schur_obs walks the track per observation: kWSchurObs is at the 5.8
    // observations of that capture's sum t^2 / sum t.
    static constexpr double kSchurObsT = 5.8;
    // Until a submit has been timed, a device is taken to be 64x slower.
    static constexpr double kPriorRate = 1e9 / 64;

    static std::mutex& budgetMutex() {
        static std::mutex m;
        return m;
    }
    // One per device and scalar config: the mapper builds a solver per BA,
    // and each should start from what the last one measured.
    spirula::SubmitBudget& budgetLocked() {
        static std::map<std::string, spirula::SubmitBudget> m;
        return m.try_emplace(ctx_.selector() + realCfgName(opt_.real), kPriorRate).first->second;
    }
    double budgetLimit() {
        std::lock_guard<std::mutex> g(budgetMutex());
        return budgetLocked().limit();
    }
    // Measured over modelled cost of one kernel on this device, 0 until probed.
    double& kernelScaleLocked(const std::string& name) {
        static std::map<std::string, double> m;
        return m[ctx_.selector() + realCfgName(opt_.real) + name];
    }

    void beginSeg() {
        cb_ = ctx_.begin();
        open_ = 0;
        segCg_ = pollCg_;
        segTop_.clear();
        segTopWork_ = 0;
    }
    // A CG kernel past convergence returns at once, so a segment holding one
    // is timed only if the flag it reads back is still clear (a 2-CU iGPU
    // lost the device on a budget learned from such no-op segments).
    double endSeg(bool record = true) {
        if (segCg_) ctx_.recordDownload(cb_, bCgScal_, 8 * realSize(opt_.real), 0, kDlCgScal);
        const auto t0 = std::chrono::steady_clock::now();
        VkCommandBuffer cb = cb_;
        cb_ = VK_NULL_HANDLE;
        ctx_.submit(cb);
        const double dt =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        cgNoop_ = segCg_ && cgFlagStaged();
        segCg_ = false;
        std::lock_guard<std::mutex> g(budgetMutex());
        spirula::SubmitBudget& b = budgetLocked();
        if (!record || cgNoop_) return dt;
        // A segment that is mostly one probed kernel corrects that kernel's
        // ratio, so the global rate drifting with the others cannot oversize
        // it (jac in df drifted 3x on an iGPU); 2x a step, as tiny launches plateau.
        double* scale = segTop_.empty() ? nullptr : &kernelScaleLocked(segTop_);
        if (scale && *scale > 0 && segTopWork_ > 0.8 * open_ && dt > b.target() / 16 &&
            b.rate() > 0) {
            const double ratio = dt * b.rate() / open_;
            *scale *= ratio > 1 ? std::min(ratio, 2.0) : std::sqrt(ratio);
            *scale = std::min(std::max(*scale, 1.0 / 16), 256.0);
        } else {
            b.record(open_, dt);
        }
        return dt;
    }
    void split() {
        endSeg();
        beginSeg();
    }
    // Called after a barrier, before recording `w` more work: past the budget,
    // what is recorded goes to the device first. True when it did.
    bool room(double w) {
        const bool full = open_ > 0 && open_ + w > budgetLimit();
        if (full) split();
        open_ += w;
        return full;
    }

    // `n` items of `name` in budget-sized ranges, placed by `at(push, first, count)`.
    // The weights are one GPU's, so a big kernel's first range is 1/32 of a
    // budget, timed alone (schur_obs in df on a 2-CU iGPU took 21x its weight).
    template <class At>
    void launch(const std::string& name, uint32_t n, uint32_t per_group, double w_item,
                const Push& p, At at) {
        if (n == 0) return;
        double lim, scale, rate;
        bool measured;
        auto read = [&] {
            std::lock_guard<std::mutex> g(budgetMutex());
            lim = budgetLocked().limit();
            rate = budgetLocked().rate();
            measured = budgetLocked().measured();
            scale = kernelScaleLocked(name);
        };
        read();
        const bool big = n * w_item > lim / 32;
        if (scale == 0 && big && open_ > 0) {
            split();  // what is pending may be the measurement a probe needs
            read();
        }
        // At most 256 ranges: a small launch runs at a latency floor that no
        // per-item cost fits, and 1/256 of any launch here is far under 2 s.
        auto rangeOf = [&](double units) {
            const double w = w_item * (scale > 0 ? scale : 1);
            uint64_t c = lim > 0 ? (uint64_t)(units / std::max(w, 1e-9)) : n;
            c = std::max<uint64_t>(c, n / 256);
            return std::max<uint64_t>(per_group, c / per_group * per_group);
        };
        uint64_t a = 0;
        if (scale == 0 && measured && big) {
            if (open_ > 0) split();
            const uint32_t c = (uint32_t)std::min<uint64_t>(rangeOf(lim / 32), n);
            Push q = p;
            at(q, 0, c);
            ctx_.dispatch(cb_, name, (c + per_group - 1) / per_group, q);
            open_ = kWLaunch + c * w_item;
            const double dt = endSeg(false);
            beginSeg();
            if (!cgNoop_) {
                std::lock_guard<std::mutex> g(budgetMutex());
                kernelScaleLocked(name) =
                    std::min(std::max(dt * rate / (c * w_item), 1.0 / 16), 256.0);
            }
            a = c;
        }
        while (a < n) {
            read();
            const uint32_t c = (uint32_t)std::min<uint64_t>(rangeOf(lim), n - a);
            const double w = c * w_item * (scale > 0 ? scale : 1);
            room(kWLaunch + w);
            if (name == segTop_) segTopWork_ += w;
            else if (w > segTopWork_) segTop_ = name, segTopWork_ = w;
            Push q = p;
            at(q, (uint32_t)a, c);
            ctx_.dispatch(cb_, name, (c + per_group - 1) / per_group, q);
            a += c;
        }
    }
    // Kernels that take the whole count in u0 and a range's first item in u4.
    static void atBase(Push& q, uint32_t first, uint32_t) { q.u4 = first; }

    // ... and the per-model kernels, whose u0/u1 already are a count and an offset.
    template <class Range>
    static auto atModel(const Range& mr) {
        return [&mr](Push& q, uint32_t first, uint32_t count) {
            q.u0 = count;
            q.u1 = mr.offset + first;
        };
    }

    bool cgFlagStaged() {
        std::vector<double> v;
        unpackReals(v, (const uint8_t*)ctx_.stagingDownloadPtr() + kDlCgScal, 8, opt_.real);
        return v[6] > 0.5;
    }

    // ---- recording ----

    // chol_update also factors the next diagonal tile, so chol_diag proper
    // only runs for the first block; the triangular solves are one fused
    // dispatch per block (see cholesky.slang).
    void recordCholesky() {
        const uint32_t n = P_.n_dim, bs = 32;
        const uint32_t nb = (n + bs - 1) / bs;
        recordFactor(n);
        Push p;
        p.u0 = n;
        for (uint32_t k = 0; k < nb; k++) {
            p.u1 = k;
            p.u2 = (k + 1) * bs < n ? n - (k + 1) * bs : 0;
            room(kWLaunch + 2.0 * bs * p.u2 * kWFlop);
            ctx_.dispatch(cb_, "tri_fwd", std::max(1u, (p.u2 + 255) / 256), p);
            ctx_.barrier(cb_);
        }
        for (int k = (int)nb - 1; k >= 0; k--) {
            p.u1 = (uint32_t)k;
            p.u2 = (uint32_t)k * bs;
            room(kWLaunch + 2.0 * bs * p.u2 * kWFlop);
            ctx_.dispatch(cb_, "tri_bwd", std::max(1u, (p.u2 + 255) / 256), p);
            ctx_.barrier(cb_);
        }
    }

    // Factor the leading n x n packed triangle of u_S in place; `rel` > 0
    // replaces a pivot under that fraction of the diagonal tc_reg saved with it.
    void recordFactor(uint32_t n, float rel = 0) {
        const uint32_t bs = 32;
        const uint32_t nb = (n + bs - 1) / bs;
        const double tile = 2.0 * bs * bs * bs * kWFlop;
        Push p;
        p.u0 = n;
        p.u1 = 0;
        p.u3 = rel > 0 ? 1 : 0;
        p.f0 = rel;
        room(kWLaunch + tile);
        ctx_.dispatch(cb_, "chol_diag", 1, p);
        ctx_.barrier(cb_);
        for (uint32_t k = 0; k + 1 < nb; k++) {
            p.u1 = k;
            uint32_t below = nb - 1 - k;
            launch("chol_panel", below, 1, tile, p, atBase);
            ctx_.barrier(cb_);
            p.u2 = below * (below + 1) / 2;
            launch("chol_update", p.u2, 1, tile, p, atBase);
            ctx_.barrier(cb_);
        }
    }

    // A_c = P^T S P from this iteration's B and W; its Cholesky factor is then
    // inverted in place, so an application is two matrix-vector products.
    void recordCoarse() {
        const uint32_t packedTc = tcN_ * (tcN_ + 1) / 2;
        Push p;
        p.u0 = P_.num_frames;
        p.u2 = tcK_;
        p.u3 = packedTc;
        room(2 * kWLaunch + (P_.num_frames + P_.num_images) * kWImage);
        ctx_.dispatch(cb_, "tc_basis", (P_.num_frames + 63) / 64, p);
        ctx_.fillZero(cb_, bS_, 0, (VkDeviceSize)packedTc * realSize(opt_.real));
        ctx_.barrier(cb_);
        p.u0 = P_.num_images;
        ctx_.dispatch(cb_, std::string("tc_bpart") + cgSuffix_, (P_.num_images + 63) / 64, p);
        p.u0 = tcChunks_;
        p.u1 = P_.num_pair_chunks;
        launch("tc_schur", tcChunks_, 1,
               kWTcSchur * (double)tcEntries_ / std::max(1u, tcChunks_), p, atBase);
        ctx_.barrier(cb_);
        Push q;
        q.u0 = tcN_;
        q.f0 = opt_.real == RealCfg::F32 ? 1e-4f : 1e-8f;
        room(kWLaunch + tcN_ * kWVec);
        ctx_.dispatch(cb_, "tc_reg", (tcN_ + 255) / 256, q);
        ctx_.barrier(cb_);
        recordFactor(tcN_, opt_.real == RealCfg::F32 ? 1e-3f : 1e-6f);

        const uint32_t bs = 32, nb = (tcN_ + bs - 1) / bs;
        const double tile = 2.0 * bs * bs * bs * kWFlop;
        Push d;
        d.u0 = tcN_;
        room(kWLaunch + nb * tile);
        ctx_.dispatch(cb_, "tc_dinv", nb, d);
        ctx_.barrier(cb_);
        d.u2 = 2 * tcN_;  // scratch rows follow the two vectors in u_y
        for (uint32_t i = 1; i < nb; i++) {
            d.u1 = i;
            room(2 * kWLaunch + i * (i + 1) / 2.0 * tile);
            ctx_.dispatch(cb_, "tc_inv_row", i, d);
            ctx_.barrier(cb_);
            ctx_.dispatch(cb_, "tc_inv_copy", (bs * bs * i + 255) / 256, d);
            ctx_.barrier(cb_);
        }
    }

    // z += P A_c^-1 P^T r, after the block-Jacobi part of the preconditioner.
    void recordCoarseApply() {
        Push p;
        p.u0 = tcN_;
        p.u1 = P_.num_frames;
        p.u2 = tcK_;
        p.u3 = tcN_ * (tcN_ + 1) / 2;
        room(4 * kWLaunch + (6.0 * P_.num_frames + (double)tcN_ * tcN_) * kWVec);
        ctx_.dispatch(cb_, "tc_restrict", (tcN_ + 255) / 256, p);
        ctx_.barrier(cb_);
        ctx_.dispatch(cb_, "tc_lmul", (tcN_ + 7) / 8, p);
        ctx_.barrier(cb_);
        ctx_.dispatch(cb_, "tc_ltmul", (tcN_ + 31) / 32, p);
        ctx_.barrier(cb_);
        p.u0 = P_.pose_dim;
        ctx_.dispatch(cb_, "tc_prolong", (P_.pose_dim + 255) / 256, p);
        ctx_.barrier(cb_);
    }

    void recordCost() {
        ctx_.fillZero(cb_, bCost_);
        ctx_.barrier(cb_);
        for (auto& mr : P_.model_ranges) {
            Push p;
            p.f0 = opt_.loss_param;
            launch(costEntry(mr), mr.count, 256, kWCost, p, atModel(mr));
        }
        ctx_.barrier(cb_);
    }

    void recordAssembly(float damping, bool reuse, LinSolve path) {
        const bool dense = path != LinSolve::CG;
        if (reuse) {
            // Params were restored after a reject; the per-observation
            // Jacobians (Jc/Jp/res/App) still match them, so skip the Jacobian
            // pass. S and g are rebuilt from those by the Schur kernels -- on
            // every path, which is why none of them is snapshotted. Bp is the
            // one thing the back-substitution overwrote, so it is.
            ctx_.copy(cb_, bBp0_, bBp_, bBp_.size);
            if (dense) {
                ctx_.fillZero(cb_, bS_);
                ctx_.fillZero(cb_, bG_);
            }
            ctx_.barrier(cb_);
        } else {
            // backup params for possible reject
            ctx_.copy(cb_, bPoses_, bPosesBak_, bPoses_.size);
            ctx_.copy(cb_, bIntr_, bIntrBak_, bIntr_.size);
            ctx_.copy(cb_, bPoints_, bPointsBak_, bPoints_.size);
            if (!P_.exts.empty()) ctx_.copy(cb_, bExts_, bExtsBak_, bExts_.size);

            if (dense) {
                ctx_.fillZero(cb_, bS_);
                ctx_.fillZero(cb_, bG_);
            }
            ctx_.fillZero(cb_, bApp_);
            ctx_.fillZero(cb_, bBp_);
            ctx_.barrier(cb_);

            for (auto& mr : P_.model_ranges) {
                Push p;
                p.f0 = opt_.loss_param;
                launch(jacEntry(mr), mr.count, 128, kWJac * wide_, p, atModel(mr));
            }
            ctx_.barrier(cb_);

            ctx_.copy(cb_, bBp_, bBp0_, bBp_.size);
            ctx_.barrier(cb_);
        }

        {
            Push q;
            q.u0 = P_.num_points;
            q.f0 = damping;
            room(kWLaunch + P_.num_points * kWPoint);
            ctx_.dispatch(cb_, "point_prep", (P_.num_points + 255) / 256, q);
            if (path == LinSolve::CG) {  // chunked cg_cam_diag accumulates atomically
                ctx_.fillZero(cb_, bCgB_);
                ctx_.fillZero(cb_, bCgM_);
                ctx_.fillZero(cb_, bG_);
            }
        }
        ctx_.barrier(cb_);

        {
            Push p;
            p.f0 = damping;
            if (path == LinSolve::DensePair) {
                p.u0 = P_.num_obs;
                launch("y_prep", P_.num_obs, 256, kWYPrep, p, atBase);
                ctx_.barrier(cb_);
                p.u0 = P_.num_pair_chunks;
                const double entries = P_.pair_entries.size() / 2.0;
                launch(std::string("schur_pair") + schurSuffix_, P_.num_pair_chunks, 1,
                       kWPairEntry * wide_ * wide_ * entries / std::max(1u, P_.num_pair_chunks),
                       p, atBase);
            } else if (path == LinSolve::DenseObs) {
                p.u0 = P_.num_obs;
                launch(std::string("schur_obs") + schurSuffix_, P_.num_obs, 128,
                       kWSchurObs * wide_ * wide_ * meanTrackT_ / kSchurObsT, p, atBase);
            } else {
                p.u0 = P_.num_cam_chunks;
                p.u1 = P_.prec_exclusive ? 1 : 0;
                p.u2 = P_.num_frames;  // member blocks follow the frame ones, then groups
                p.u3 = P_.num_frames + (uint32_t)P_.members.size();
                launch(std::string("cg_cam_diag") + cgSuffix_, P_.num_cam_chunks, 1,
                       kWCamDiag * wide_ * wide_ * P_.num_obs / std::max(1u, P_.num_cam_chunks),
                       p, atBase);
                ctx_.barrier(cb_);
                p.u0 = P_.num_prec_blocks;
                room(kWLaunch + P_.num_prec_blocks * kWImage);
                ctx_.dispatch(cb_, "cg_prec_fact", (P_.num_prec_blocks + 255) / 256, p);
                if (tcBuild_) recordCoarse();
            }
        }
        ctx_.barrier(cb_);
    }

    // Record the device-side PCG loop (see cg.slang). Every kernel no-ops once
    // the convergence flag is set; where the budget splits the loop, the flag
    // is read back and the rest is not recorded.
    void recordPCG(uint32_t maxit) {
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
        const double wVecs = 5 * kWLaunch + 3.0 * n * kWVec;
        room(wVecs + P_.num_prec_blocks * kWImage);
        Push pn;
        pn.u0 = n;
        ctx_.dispatch(cb_, "cg_init", ng, pn);
        ctx_.barrier(cb_);
        Push pc;
        pc.u0 = P_.num_prec_blocks;
        pc.u1 = 0;  // flag was just cleared
        ctx_.dispatch(cb_, "cg_prec_apply", nib, pc);
        ctx_.barrier(cb_);
        if (tcUse_) recordCoarseApply();
        Push pr;
        pr.u0 = n;
        pr.u1 = 1;
        pr.u2 = npart;
        pr.u3 = 0;
        ctx_.dispatch(cb_, "cg_red2", ng, pr);
        ctx_.barrier(cb_);
        Push pf;
        pf.u0 = npart;
        pf.u1 = 0;
        pf.u2 = npart;
        pf.f0 = (float)opt_.cg_tol;
        pf.f1 = (float)opt_.cg_model_tol;
        ctx_.dispatch(cb_, "cg_fin", 1, pf);
        ctx_.barrier(cb_);
        ctx_.dispatch(cb_, "cg_copy", ng, pn);
        ctx_.barrier(cb_);
        pc.u1 = 1;
        pr.u3 = 1;
        const double wTrack = (double)P_.num_obs / std::max(1u, P_.num_points);
        const double wChunk = (double)P_.num_obs / std::max(1u, P_.num_cam_chunks);
        pollCg_ = true;
        for (uint32_t it = 0; it < maxit; it++) {
            if (room(2 * wVecs + P_.num_images * kWImage) && cgNoop_) break;
            segCg_ = true;
            Push pg;
            pg.u0 = P_.num_points;
            launch(std::string("cg_gather") + cgSuffix_, P_.num_points, 256,
                   kWGather * wide_ * wTrack, pg, atBase);
            ctx_.barrier(cb_);
            Push ps;
            ps.u0 = P_.num_images;
            ps.u1 = P_.prec_exclusive ? 1 : 0;
            if (zeroIntr) {
                ctx_.fillZero(cb_, bCgSp_, intrOff, intrSize);
                ctx_.barrier(cb_);
            }
            ctx_.dispatch(cb_, std::string("cg_bmul") + cgSuffix_, P_.num_images, ps);
            ctx_.barrier(cb_);
            ps.u0 = P_.num_cam_chunks;
            launch(std::string("cg_scatter") + cgSuffix_, P_.num_cam_chunks, 1,
                   kWScatter * wide_ * wChunk, ps, atBase);
            ctx_.barrier(cb_);
            pr.u1 = 0;
            ctx_.dispatch(cb_, "cg_red2", ng, pr);
            ctx_.barrier(cb_);
            pf.u1 = 1;
            ctx_.dispatch(cb_, "cg_fin", 1, pf);
            ctx_.barrier(cb_);
            ctx_.dispatch(cb_, "cg_axpy", ng, pn);
            ctx_.barrier(cb_);
            ctx_.dispatch(cb_, "cg_prec_apply", nib, pc);
            ctx_.barrier(cb_);
            if (tcUse_) recordCoarseApply();
            pr.u1 = 1;
            ctx_.dispatch(cb_, "cg_red2", ng, pr);
            ctx_.barrier(cb_);
            pf.u1 = 2;
            ctx_.dispatch(cb_, "cg_fin", 1, pf);
            ctx_.barrier(cb_);
            ctx_.dispatch(cb_, "cg_updp", ng, pn);
            ctx_.barrier(cb_);
        }
        pollCg_ = false;
    }

    // Put the parameters back where the last accepted step left them. A reject
    // (and the CG fallback, which is a reject that retries) needs this before
    // anything else touches them -- but it is three buffer copies, and a submit
    // of its own costs a fence round trip on a device several solvers are
    // sharing. So the reject only *marks* it, and the next command buffer to be
    // recorded carries it. Nothing runs in between: the LM loop either records
    // another iteration or leaves, and leaving flushes it (see solve()).
    void recordRestore() {
        ctx_.copy(cb_, bPosesBak_, bPoses_, bPoses_.size);
        ctx_.copy(cb_, bIntrBak_, bIntr_, bIntr_.size);
        ctx_.copy(cb_, bPointsBak_, bPoints_, bPoints_.size);
        if (!P_.exts.empty()) ctx_.copy(cb_, bExtsBak_, bExts_, bExts_.size);
        ctx_.barrier(cb_);
    }

    // Emit a marked restore on its own, for the one caller that cannot defer:
    // the loop is over and the parameters are about to be read back.
    void flushRestore() {
        if (!restore_pending_) return;
        restore_pending_ = false;
        beginSeg();
        recordRestore();
        endSeg();
    }

    void recordIteration(float damping, bool reuse, LinSolve path) {
        if (restore_pending_) {
            restore_pending_ = false;
            recordRestore();
        }
        recordAssembly(damping, reuse, path);

        if (path == LinSolve::CG)
            recordPCG(cgMaxit_);
        else
            recordCholesky();

        {
            Push p;
            p.u0 = P_.num_obs;
            launch(std::string("dp_accum") + schurSuffix_, P_.num_obs, 256, kWDp * wide_, p,
                   atBase);
        }
        ctx_.barrier(cb_);

        {
            room(4 * kWLaunch + P_.num_points * kWPoint);
            Push p;
            p.u0 = P_.num_points;
            ctx_.dispatch(cb_, "point_update", (P_.num_points + 255) / 256, p);
            Push q;
            q.u0 = P_.pose_dim;
            ctx_.dispatch(cb_, "cam_update", (P_.pose_dim + 255) / 256, q);
            if (!P_.members.empty()) {
                Push e;
                e.u0 = (uint32_t)P_.members.size();
                ctx_.dispatch(cb_, "ext_update", ((uint32_t)P_.members.size() + 63) / 64, e);
            }
            if (!P_.groups.empty()) {
                Push r;
                r.u0 = (uint32_t)P_.groups.size();
                ctx_.dispatch(cb_, "intr_update", ((uint32_t)P_.groups.size() + 63) / 64, r);
            }
        }
        ctx_.barrier(cb_);

        recordCost();
        // Fold the two readbacks the LM loop needs into this command buffer.
        // A separate download() is its own fenced submit, so taking them here
        // halves the submits per iteration -- and a submit's latency, not its
        // arithmetic, is what a forty-image solve costs. The atom phase of a
        // bottom-up run spends five thousand iterations on problems that size,
        // with several solvers sharing the device.
        ctx_.barrier(cb_);
        ctx_.recordDownload(cb_, bCost_, realSize(opt_.real), 0, kDlCost);
        if (path == LinSolve::CG)
            ctx_.recordDownload(cb_, bCgScal_, 8 * realSize(opt_.real), 0, kDlCgScal);
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
    // Canonical uuid:<hex> this solve resolved to, empty before init() and when
    // no device at all was usable (the host path). What the capability cache is
    // keyed by, so two solves on one device share one probe.
    std::string selector_;
    std::unique_ptr<bacpu::Solver> cpu_;  // non-null when running on the host
    const char* schurSuffix_ = "_c";  // dof tier of the Schur kernels (pickTiers)
    const char* cgSuffix_ = "_w";     // ... and of the CG ones
    uint32_t bBlk_ = kCamBlk;         // per-image B block stride at that tier
    double wide_ = 1;                 // widest camera block over the rig tier's 24
    double meanTrackT_ = kSchurObsT;  // sum t^2 / sum t over the tracks
    SolverStats stats_;
    std::unique_ptr<VkContext> owned_;      // null when running on a shared context
    VkContext& ctx_;
    std::vector<GpuBuffer*> ownBufs_;   // this instance's buffers (freed in dtor if shared)

    LinSolve densePath_ = LinSolve::DenseObs;
    bool useCG_ = false;       // CG is the active path (may demote to dense)
    bool cgAllocated_ = false; // CG buffers/tables exist
    bool haveFallback_ = false;
    uint32_t cgMaxit_ = 100;
    // Coarse correction: frames per cluster and the coarse dimension (0: off),
    // and whether this iteration's solve uses it.
    uint32_t tcK_ = 0, tcN_ = 0;
    uint64_t tcEntries_ = 0;
    uint32_t tcChunks_ = 0;
    std::vector<uint32_t> tcEnt_, tcChk_;  // follow the pair-Schur tables on the device
    bool tcUse_ = false, tcBuild_ = false, tcHave_ = false, tcOff_ = false;
    int tcAge_ = 0;
    double tcLambda_ = 0;
    double lastCg_ = 0;  // iterations the last CG solve took
    static constexpr double kTcMinIters = 12;
    // Below ~1e-9 rounding outweighs the damping on a badly scaled camera (Gram
    // entries of 1e23, from a point at depth 2e-9 in a 22042-image model) and S
    // goes indefinite; 1e-8 is still Gauss-Newton to eight digits.
    static constexpr double kMinDamping = 1e-8;

    GpuBuffer bObs_, bObsImage_, bObsPoint_, bImageInfo_, bGroupInfo_, bMemberInfo_;
    GpuBuffer bPoses_, bExts_, bIntr_, bPoints_, bObsRanges_, bModelObs_, bJcOff_;
    GpuBuffer bJp_, bS_, bG_, bApp_, bBp_, bCost_;
    GpuBuffer bPosesBak_, bExtsBak_, bIntrBak_, bPointsBak_;
    bool restore_pending_ = false;  // a rejected step's parameters are still live
    VkCommandBuffer cb_ = VK_NULL_HANDLE;  // being recorded (beginSeg .. endSeg)
    double open_ = 0;                      // budget work recorded into cb_
    bool pollCg_ = false;                  // recording the PCG loop
    bool segCg_ = false;                   // cb_ holds PCG loop kernels
    bool cgNoop_ = false;                  // ... and the last one submitted found CG converged
    std::string segTop_;                   // the launch() kernel with the most work in cb_
    double segTopWork_ = 0;
    GpuBuffer bBp0_, bJc_, bRes_;
    GpuBuffer bPairEntries_, bPairChunks_, bW_, bYp_, bY_;
    GpuBuffer bCamRanges_, bCamObs_, bCamChunks_, bCgR_, bCgZ_, bCgP_, bCgSp_;
    GpuBuffer bCgV_, bCgB_, bCgM_, bCgScal_, bCgPart_, bPrecBlocks_;
};
