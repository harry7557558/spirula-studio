// GPU SIFT extractor: host-side orchestration of sfm/shaders/sift/sift.slang.
//
// Owns a VkContext, builds the Gaussian scale-space layout (offsets, per-octave
// dimensions, per-step blur kernels) on the host, uploads the image, and drives
// the pyramid -> DoG -> extrema -> orientation -> descriptor dispatches. The
// GPU stages append into device-side lists behind atomic counters; the host
// reads a count back between stages to size the next dispatch (fine for a batch
// extractor -- these are not the latency-critical local-BA solves of phase 4).
//
// Parameters mirror COLMAP's SiftExtractionOptions defaults (docs/porting-
// colmap.md). The compile-time pyramid constants here MUST match the
// static const block at the top of sift.slang.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/SubmitBudget.h"
#include "sfm/core/Log.h"
#include "i18n/catalog/Sfm.h"
#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/vk/EmbeddedSpirv.h"
#include "sfm/vk/VkContext.h"

namespace sfm {

struct SiftOptions {
    int max_num_features = 8192;      // top-K by scale kept (COLMAP semantics)
    int num_octaves = 4;
    int max_num_orientations = 2;
    double peak_threshold = 0.02 / 3.0;
    double edge_threshold = 10.0;
    int device = -1;
    // Canonical uuid:<hex> (core/VulkanDeviceSelection.h). Wins over the
    // ordinal above; "" leaves the shared precedence in charge.
    std::string device_selector;
    bool profile = false;
    bool verbose = true;
    std::string spv_path;             // override embedded "sift" blob
    // Device-list capacities; extraction warns if a list saturates.
    uint32_t max_raw_keypoints = 262144;
    uint32_t max_oriented_keypoints = 262144;
};

class SiftExtractor {
public:
    // Pyramid constants -- keep in lockstep with sift.slang.
    static constexpr int S = 3;
    static constexpr int GAUSS_PER_OCT = S + 3;  // 6
    static constexpr int DOG_PER_OCT = S + 2;    // 5
    static constexpr float SIGMA0 = 1.6f;
    static constexpr int FIRST_OCTAVE = -1;
    static constexpr int KP_STRIDE = 6;
    static constexpr int OKP_STRIDE = 8;
    static constexpr int kNumBins = 2048;  // log2(scale) histogram for top-K
    // Every radius planPyramid() derives, each a case of blur_dog in sift.slang.
    static constexpr int kBlurRadii[] = {5, 7, 8, 10, 13};
    static constexpr uint32_t kBlurTile = 32;
    static constexpr uint32_t kDescThreads = 32;  // per keypoint

    explicit SiftExtractor(const SiftOptions& opt) : opt_(opt) {
        VkContextOptions vo;
        vo.selector = opt.device_selector;
        vo.deviceIndex = opt.device;
        vo.profile = opt.profile;
        ctx_.init(vo);
    }

    VkContext& ctx() { return ctx_; }

    // Buffers grow to the largest image seen, so a batch goes largest-first.
    // `next`, if given, must be the following call's image: its upload and
    // pyramid are queued behind this image's descriptors and run meanwhile.
    FeatureSet extract(const GrayImage& img, const GrayImage* next = nullptr) {
        if (img.width < 4 || img.height < 4)
            throw std::runtime_error("image too small for SIFT");
        if (queued_ != std::make_pair(img.width, img.height)) {
            if (queued_.first) throw std::logic_error("SIFT: not the image queued last call");
            planPyramid(img.width, img.height);
            ensureAllocated();
            uploadTables();
            runPyramid(img, false);
        }
        queued_ = {0, 0};
        uint32_t nkp = runExtrema();
        uint32_t nokp = runOrient(nkp);
        uint32_t nsel = runSelect(nokp);
        const bool ahead = next && next->width >= 4 && next->height >= 4;
        runDescriptor(nsel, ahead);
        if (ahead) {
            planPyramid(next->width, next->height);
            if (!allocated()) ctx_.waitPending();  // rebinding the set in use is illegal
            ensureAllocated();
            uploadTables();
            runPyramid(*next, true);
            queued_ = {next->width, next->height};
        }
        FeatureSet fs = readback(img.width, img.height, nsel);
        if (opt_.profile) ctx_.printProfile();
        return fs;
    }

private:
    // ---- pyramid layout ----
    struct Level { uint32_t off, w, h; };

    void planPyramid(int w0, int h0) {
        W0_ = w0 * 2;  // first_octave = -1
        H0_ = h0 * 2;
        planKey_ = {W0_, H0_};
        int O = 0;
        while (O < opt_.num_octaves && (W0_ >> O) >= 8 && (H0_ >> O) >= 8) O++;
        octaves_ = std::max(1, O);

        gLevels_.clear();
        dLevels_.clear();
        uint32_t goff = 0, doff = 0;
        for (int o = 0; o < octaves_; o++) {
            uint32_t w = (uint32_t)(W0_ >> o), h = (uint32_t)(H0_ >> o);
            for (int s = 0; s < GAUSS_PER_OCT; s++) {
                gLevels_.push_back({goff, w, h});
                goff += w * h;
            }
            for (int d = 0; d < DOG_PER_OCT; d++) {
                dLevels_.push_back({doff, w, h});
                doff += w * h;
            }
        }
        gaussFloats_ = goff;
        dogFloats_ = doff;

        // Per-step blur kernels. Step 0 = initial blur of the upsampled image
        // (nominal sigma 1.0 in octave-0 px) up to SIGMA0. Steps 1..GAUSS_PER_OCT-1
        // = incremental blur within an octave (identical across octaves).
        weights_.clear();
        stepOff_.clear();
        stepRad_.clear();
        auto addKernel = [&](double dsigma) {
            int r = std::max(1, (int)std::ceil(4.0 * dsigma));
            if (std::find(std::begin(kBlurRadii), std::end(kBlurRadii), r) == std::end(kBlurRadii))
                throw std::logic_error("SIFT blur radius " + std::to_string(r) +
                                       " has no case in blur_dog (sift.slang)");
            stepOff_.push_back((uint32_t)weights_.size());
            stepRad_.push_back((uint32_t)r);
            double sum = 0;
            std::vector<double> k(2 * r + 1);
            for (int i = -r; i <= r; i++) {
                double v = std::exp(-0.5 * (i / dsigma) * (i / dsigma));
                k[i + r] = v;
                sum += v;
            }
            for (double v : k) weights_.push_back((float)(v / sum));
        };
        double sigmaNominal = 1.0;  // 0.5 * 2 after upsample, in octave-0 pixels
        addKernel(std::sqrt(std::max(SIGMA0 * SIGMA0 - sigmaNominal * sigmaNominal, 0.01)));
        for (int s = 1; s < GAUSS_PER_OCT; s++) {
            double sp = SIGMA0 * std::pow(2.0, (s - 1) / (double)S);
            double sc = SIGMA0 * std::pow(2.0, s / (double)S);
            addKernel(std::sqrt(sc * sc - sp * sp));
        }
    }

    const Level& gL(int o, int s) const { return gLevels_[o * GAUSS_PER_OCT + s]; }
    const Level& dL(int o, int d) const { return dLevels_[o * DOG_PER_OCT + d]; }

    // ---- allocation + descriptor set ----
    // Size-dependent buffers grow for an image larger than any before; the
    // keypoint lists are sized from the options and never do.
    bool allocated() const {
        size_t needImg = (size_t)(W0_ / 2) * (H0_ / 2);
        return setup_ && gaussFloats_ <= capGauss_ && dogFloats_ <= capDog_ &&
               needImg <= capImg_ && (size_t)W0_ * H0_ <= capTmp_ &&
               gLevels_.size() <= capGlev_ && dLevels_.size() <= capDlev_ &&
               weights_.size() <= capW_;
    }

    void ensureAllocated() {
        if (allocated()) return;
        size_t needImg = (size_t)(W0_ / 2) * (H0_ / 2);
        allocate();
        setup_ = true;
        capGauss_ = gaussFloats_;
        capDog_ = dogFloats_;
        capImg_ = needImg;
        capTmp_ = (size_t)W0_ * H0_;
        capGlev_ = gLevels_.size();
        capDlev_ = dLevels_.size();
        capW_ = weights_.size();
    }

    void allocate() {
        plannedOnce_ = false;  // fresh buffers hold nothing; re-upload the tables
        bImg_ = ctx_.createBuffer((VkDeviceSize)(W0_ / 2) * (H0_ / 2) * 4);
        bGauss_ = ctx_.createBuffer((VkDeviceSize)gaussFloats_ * 4);
        bDog_ = ctx_.createBuffer((VkDeviceSize)dogFloats_ * 4);
        bTmp_ = ctx_.createBuffer((VkDeviceSize)W0_ * H0_ * 4);
        bWeights_ = ctx_.createBuffer(std::max<VkDeviceSize>(16, weights_.size() * 4));
        bGlev_ = ctx_.createBuffer((VkDeviceSize)gLevels_.size() * 16);
        bDlev_ = ctx_.createBuffer((VkDeviceSize)dLevels_.size() * 16);
        bKp_ = ctx_.createBuffer((VkDeviceSize)opt_.max_raw_keypoints * KP_STRIDE * 4);
        bKpCnt_ = ctx_.createBuffer(16);
        bOkp_ = ctx_.createBuffer((VkDeviceSize)opt_.max_oriented_keypoints * OKP_STRIDE * 4);
        bOkpCnt_ = ctx_.createBuffer(16);
        bDesc_ = ctx_.createBuffer((VkDeviceSize)opt_.max_oriented_keypoints * 32 * 4);
        bHist_ = ctx_.createBuffer((VkDeviceSize)kNumBins * 4);
        bFokp_ = ctx_.createBuffer((VkDeviceSize)opt_.max_oriented_keypoints * OKP_STRIDE * 4);
        bSelCnt_ = ctx_.createBuffer(16);

        ctx_.createDescriptors({bImg_.buf, bGauss_.buf, bDog_.buf, bTmp_.buf, bWeights_.buf,
                                bGlev_.buf, bDlev_.buf, bKp_.buf, bKpCnt_.buf, bOkp_.buf,
                                bOkpCnt_.buf, bDesc_.buf, bHist_.buf, bFokp_.buf, bSelCnt_.buf});

        // Pipelines depend only on the (fixed) descriptor-set layout, so load
        // them once even if the descriptor set is later rebound to grown buffers
        // (an identical layout stays pipeline-compatible).
        if (!pipelinesLoaded_) {
            size_t words = 0;
            if (!opt_.spv_path.empty()) {
                ctx_.loadPipelines(opt_.spv_path, kEntries());
            } else {
                const uint32_t* code = findSpirv("sift", &words);
                if (!code) throw std::runtime_error("sift shader not built into this binary");
                ctx_.loadPipelines(code, words * 4, kEntries());
            }
            pipelinesLoaded_ = true;
        }
    }

    static std::vector<std::string> kEntries() {
        return {"upsample", "blur_dog",   "downsample",  "extrema",
                "orient",   "scale_hist", "select_topk", "descriptor"};
    }

    // The blur weights and the level tables only change when planPyramid()
    // produces a different layout, which for a batch sorted largest-first is a
    // handful of times over thousands of images.
    void uploadTables() {
        if (planKey_ == lastPlanKey_ && plannedOnce_) return;
        ctx_.upload(bWeights_, weights_.data(), weights_.size() * 4);
        std::vector<uint32_t> gt(gLevels_.size() * 4), dt(dLevels_.size() * 4);
        for (size_t i = 0; i < gLevels_.size(); i++) {
            gt[4 * i + 0] = gLevels_[i].off;
            gt[4 * i + 1] = gLevels_[i].w;
            gt[4 * i + 2] = gLevels_[i].h;
            gt[4 * i + 3] = 0;
        }
        for (size_t i = 0; i < dLevels_.size(); i++) {
            dt[4 * i + 0] = dLevels_[i].off;
            dt[4 * i + 1] = dLevels_[i].w;
            dt[4 * i + 2] = dLevels_[i].h;
            dt[4 * i + 3] = 0;
        }
        ctx_.upload(bGlev_, gt.data(), gt.size() * 4);
        ctx_.upload(bDlev_, dt.data(), dt.size() * 4);
        lastPlanKey_ = planKey_;
        plannedOnce_ = true;
    }

    // ---- dispatch helpers ----
    static Push pk(uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0, uint32_t e = 0,
                   uint32_t f = 0, uint32_t g = 0, uint32_t h = 0) {
        Push p;
        uint32_t v[8] = {a, b, c, d, e, f, g, h};
        std::memcpy(&p, v, sizeof v);
        return p;
    }
    static uint32_t fbits(double x) {
        float xf = (float)x;
        uint32_t u;
        std::memcpy(&u, &xf, 4);
        return u;
    }
    static uint32_t grid(uint32_t n, uint32_t local) { return (n + local - 1) / local; }

    void img2d(VkCommandBuffer cb, const char* name, uint32_t w, uint32_t h, const Push& p) {
        ctx_.dispatch(cb, name, grid(w, 16), p, grid(h, 16));
    }

    // Submitted in pieces of the budget's size: a pyramid is 0.44 s on a 2-CU
    // RADV iGPU at 3200 px. The image rides in the first piece; every fenced
    // submit is a round trip the GPU spends idle.
    void runPyramid(const GrayImage& img, bool async) {
        const VkDeviceSize bytes = img.data.size() * 4;
        if (bytes > VkContext::stagingCapacity()) ctx_.upload(bImg_, img.data.data(), bytes);
        VkCommandBuffer cb = ctx_.begin();
        if (bytes <= VkContext::stagingCapacity()) {
            ctx_.recordUpload(cb, bImg_, img.data.data(), bytes);
            ctx_.barrier(cb);
        }
        for (int o = 0; o < octaves_; o++) {
            const Level& g0 = gL(o, 0);
            if (o == 0) {
                // upsample original -> tmp, then blur to SIGMA0 into gauss(0,0)
                img2d(cb, "upsample", g0.w, g0.h,
                      pk(0, (uint32_t)(W0_ / 2), (uint32_t)(H0_ / 2), g0.w, g0.h));
                ctx_.barrier(cb);
                blur(cb, 0, g0.off, g0.w, g0.h, 0, kNoDog, true);
            } else {
                const Level& prevTop = gL(o - 1, S);
                img2d(cb, "downsample", g0.w, g0.h,
                      pk(prevTop.off, g0.off, prevTop.w, g0.w, g0.h));
                ctx_.barrier(cb);
            }
            // gauss(o,s) and dog(o,s-1) = gauss(o,s) - gauss(o,s-1) in one pass
            for (int s = 1; s < GAUSS_PER_OCT; s++) {
                const Level& a = gL(o, s - 1);
                const Level& b = gL(o, s);
                blur(cb, a.off, b.off, b.w, b.h, s, dL(o, s - 1).off, false);
            }
        }
        submitTimed(cb, pyramidBudget_, pyramidWork_, async);
    }

    static constexpr uint32_t kNoDog = 0xffffffffu;

    // src and dst must not overlap: tiles read their halo from src while
    // neighbouring tiles write dst.
    void blur(VkCommandBuffer& cb, uint32_t srcOff, uint32_t dstOff, uint32_t w, uint32_t h,
              int step, uint32_t dogOff, bool srcIsTmp) {
        uint32_t woff = stepOff_[step], r = stepRad_[step];
        ctx_.dispatch(cb, "blur_dog", grid(w, kBlurTile),
                      pk(srcOff, dstOff, w, h, woff, r, dogOff, srcIsTmp ? 1u : 0u),
                      grid(h, kBlurTile));
        ctx_.barrier(cb);
        pyramidWork_ += 2.0 * w * h * (2 * r + 1);
        if (pyramidWork_ >= pyramidBudget_.limit()) {
            submitTimed(cb, pyramidBudget_, pyramidWork_);
            cb = ctx_.begin();
        }
    }

    // An async submit is not timed: its wait also covers whatever the host
    // did meanwhile.
    void submitTimed(VkCommandBuffer cb, spirula::SubmitBudget& budget, double& work,
                     bool async = false) {
        ctx_.waitPending();
        const auto t0 = std::chrono::steady_clock::now();
        if (async) {
            ctx_.submitAsync(cb);
        } else {
            ctx_.submit(cb);
            budget.record(work, std::chrono::duration<double>(
                                    std::chrono::steady_clock::now() - t0).count());
        }
        work = 0;
    }

    uint32_t runExtrema() {
        VkCommandBuffer cb = ctx_.begin();
        ctx_.fillZero(cb, bKpCnt_);
        ctx_.barrier(cb);
        for (int o = 0; o < octaves_; o++) {
            const Level& base = dL(o, 0);
            for (int d = 1; d <= S; d++)
                img2d(cb, "extrema", base.w, base.h,
                      pk((uint32_t)o, (uint32_t)d, base.off, base.w, base.h,
                         opt_.max_raw_keypoints, fbits(opt_.peak_threshold),
                         fbits(opt_.edge_threshold)));
        }
        uint32_t n = submitCount(cb, bKpCnt_);
        if (n > opt_.max_raw_keypoints) {
            if (opt_.verbose)
                slog::warn(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_saturated_raw,
                           {(long long)n, (long long)opt_.max_raw_keypoints});
            n = opt_.max_raw_keypoints;
        }
        if (opt_.verbose)
            slog::err(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_raw,
                      {(long long)octaves_, (long long)n});
        return n;
    }

    // orient and descriptor are ~200 and ~50 ms of one 3200 px image on a 2-CU
    // RADV iGPU, so they go in keypoint ranges.
    uint32_t runOrient(uint32_t nkp) {
        VkCommandBuffer cb = ctx_.begin();
        ctx_.fillZero(cb, bOkpCnt_);
        ctx_.barrier(cb);
        for (uint32_t k0 = 0;;) {
            const uint32_t n = (uint32_t)std::min<int64_t>(nkp - k0, orientBudget_.chunk(4096, nkp));
            if (n > 0)
                ctx_.dispatch(cb, "orient", grid(n, 64),
                              pk(k0 + n, (uint32_t)opt_.max_num_orientations,
                                 opt_.max_oriented_keypoints, k0));
            k0 += n;
            if (k0 >= nkp) {
                ctx_.barrier(cb);
                ctx_.recordDownload(cb, bOkpCnt_, 4);
            }
            double work = n;
            submitTimed(cb, orientBudget_, work);
            if (k0 >= nkp) break;
            cb = ctx_.begin();
        }
        uint32_t n = 0;
        std::memcpy(&n, ctx_.stagingDownloadPtr(), 4);
        if (n > opt_.max_oriented_keypoints) {
            if (opt_.verbose)
                slog::warn(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_saturated_oriented,
                           {(long long)n, (long long)opt_.max_oriented_keypoints});
            n = opt_.max_oriented_keypoints;
        }
        return n;
    }

    // GPU top-K by scale: histogram-select a scale threshold that keeps ~K of
    // the largest-scale oriented keypoints, then compact the survivors into
    // `fokp` so only they are described.  Returns the survivor count (>= K by a
    // small, single-bin overshoot; the host applies the exact cut later).
    uint32_t runSelect(uint32_t nokp) {
        if (nokp == 0) return 0;
        uint32_t K = (uint32_t)opt_.max_num_features;
        float threshold = 0.0f;  // keep all (scales are strictly positive)

        if (opt_.max_num_features > 0 && nokp > K) {
            // log2(scale) range for the histogram, from the pyramid geometry.
            double lo = 0.5 * SIGMA0 * std::exp2((double)FIRST_OCTAVE);
            double hi = SIGMA0 * std::exp2((S + 1.0) / S) *
                        std::exp2((double)(octaves_ - 1 + FIRST_OCTAVE)) * 1.5;
            double logMin = std::log2(lo), logMax = std::log2(hi);
            double invRange = 1.0 / (logMax - logMin);

            VkCommandBuffer cb = ctx_.begin();
            ctx_.fillZero(cb, bHist_);
            ctx_.barrier(cb);
            ctx_.dispatch(cb, "scale_hist", grid(nokp, 64),
                          pk(nokp, kNumBins, fbits(logMin), fbits(invRange)));
            ctx_.barrier(cb);
            ctx_.recordDownload(cb, bHist_, kNumBins * 4);
            ctx_.submit(cb);

            std::vector<uint32_t> hist(kNumBins);
            std::memcpy(hist.data(), ctx_.stagingDownloadPtr(), hist.size() * 4);
            // Accumulate from the high-scale end until we have >= K, keeping the
            // last bin included (smallest overshoot, never selects nothing).
            uint64_t run = 0;
            int thrBin = 0;
            for (int b = kNumBins - 1; b >= 0; b--) {
                if (run >= K) break;
                run += hist[b];
                thrBin = b;
            }
            threshold = (float)std::exp2(logMin + (double)thrBin / kNumBins * (logMax - logMin));
        }

        VkCommandBuffer cb = ctx_.begin();
        ctx_.fillZero(cb, bSelCnt_);
        ctx_.barrier(cb);
        ctx_.dispatch(cb, "select_topk", grid(nokp, 64),
                      pk(nokp, fbits((double)threshold), opt_.max_oriented_keypoints));
        const uint32_t nsel = std::min(submitCount(cb, bSelCnt_), opt_.max_oriented_keypoints);
        if (opt_.verbose)
            slog::err(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_selected,
                      {(long long)nokp, (long long)nsel, slog::num(threshold, 3)});
        return nsel;
    }

    // The last chunk also copies the keypoints and descriptors out, at
    // descOffset() in the download staging buffer.
    void runDescriptor(uint32_t nsel, bool asyncLast) {
        for (uint32_t k0 = 0; k0 < nsel;) {
            const uint32_t n = (uint32_t)std::min<int64_t>(nsel - k0, descBudget_.chunk(1024, nsel));
            VkCommandBuffer cb = ctx_.begin();
            ctx_.dispatch(cb, "descriptor", grid(n * kDescThreads, 64), pk(k0 + n, k0));
            k0 += n;
            if (k0 >= nsel && fusedReadback(nsel)) {
                ctx_.barrier(cb);
                ctx_.recordDownload(cb, bFokp_, (VkDeviceSize)nsel * OKP_STRIDE * 4);
                ctx_.recordDownload(cb, bDesc_, (VkDeviceSize)nsel * 128, 0, descOffset(nsel));
            }
            double work = n;
            submitTimed(cb, descBudget_, work, asyncLast && k0 >= nsel);
        }
    }

    static VkDeviceSize descOffset(uint32_t nsel) {
        return ((VkDeviceSize)nsel * OKP_STRIDE * 4 + 15) & ~(VkDeviceSize)15;
    }
    static bool fusedReadback(uint32_t nsel) {
        return descOffset(nsel) + (VkDeviceSize)nsel * 128 <= VkContext::stagingCapacity();
    }

    // Submit `cb` with a copy of the counter at the front of `counter`, and
    // return it.
    uint32_t submitCount(VkCommandBuffer cb, const GpuBuffer& counter) {
        ctx_.barrier(cb);
        ctx_.recordDownload(cb, counter, 4);
        ctx_.submit(cb);
        uint32_t n = 0;
        std::memcpy(&n, ctx_.stagingDownloadPtr(), 4);
        return n;
    }

    // Reads what runDescriptor's last chunk staged: no download in between.
    FeatureSet readback(int w0, int h0, uint32_t nsel) {
        FeatureSet fs;
        fs.width = w0;
        fs.height = h0;
        fs.dim = 128;
        fs.dtype = DType::U8;
        if (nsel == 0) return fs;

        std::vector<float> okpBuf;
        std::vector<uint8_t> descBuf;
        const float* okp;
        const uint8_t* desc;
        if (fusedReadback(nsel)) {
            const uint8_t* staged = (const uint8_t*)ctx_.stagingDownloadPtr();
            okp = (const float*)staged;
            desc = staged + descOffset(nsel);
        } else {
            okpBuf.resize((size_t)nsel * OKP_STRIDE);
            ctx_.download(bFokp_, okpBuf.data(), okpBuf.size() * 4);
            descBuf.resize((size_t)nsel * 128);
            ctx_.download(bDesc_, descBuf.data(), descBuf.size());
            okp = okpBuf.data();
            desc = descBuf.data();
        }

        std::vector<Keypoint> kps(nsel);
        for (uint32_t i = 0; i < nsel; i++) {
            const float* p = &okp[(size_t)i * OKP_STRIDE];
            kps[i] = {p[0], p[1], p[2], p[3], 0.0f};
        }

        // Exact top-K cut on the (already small) survivor list, by scale, then a
        // canonical output order.
        //
        // Both need a *total* order, because the GPU appends keypoints through
        // atomics and the device order varies run to run. Leaving ties to that
        // order made features.bin -- and so match indices, the seed pair and
        // the whole reconstruction -- irreproducible (D16).
        //
        // The emitted order is by *position*, deliberately not by scale.
        // Downstream tie-breaks are index-ordered (the matcher's max_num_matches
        // cap, the mapper taking the first 3D point a feature corresponds to),
        // so a scale-sorted index would quietly bias all of them toward
        // large-scale, poorly-localized features. Position is uncorrelated with
        // feature quality, which is what a canonical order should be.
        auto byScale = [&](uint32_t a, uint32_t b) {
            const Keypoint& p = kps[a];
            const Keypoint& q = kps[b];
            if (p.scale != q.scale) return p.scale > q.scale;
            if (p.x != q.x) return p.x < q.x;
            if (p.y != q.y) return p.y < q.y;
            return p.orientation < q.orientation;
        };
        auto byPosition = [&](uint32_t a, uint32_t b) {
            const Keypoint& p = kps[a];
            const Keypoint& q = kps[b];
            if (p.x != q.x) return p.x < q.x;
            if (p.y != q.y) return p.y < q.y;
            if (p.scale != q.scale) return p.scale > q.scale;
            return p.orientation < q.orientation;
        };
        std::vector<uint32_t> idx(nsel);
        for (uint32_t i = 0; i < nsel; i++) idx[i] = i;
        uint32_t keep = nsel;
        if (opt_.max_num_features > 0 && nsel > (uint32_t)opt_.max_num_features) {
            keep = (uint32_t)opt_.max_num_features;
            std::partial_sort(idx.begin(), idx.begin() + keep, idx.end(), byScale);
            idx.resize(keep);
        }
        std::sort(idx.begin(), idx.end(), byPosition);

        fs.keypoints.resize(keep);
        fs.descriptors.resize((size_t)keep * 128);
        for (uint32_t i = 0; i < keep; i++) {
            fs.keypoints[i] = kps[idx[i]];
            std::memcpy(&fs.descriptors[(size_t)i * 128], &desc[(size_t)idx[i] * 128], 128);
        }
        if (opt_.verbose)
            slog::err(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_features,
                      {(long long)keep});
        return fs;
    }

    SiftOptions opt_;
    VkContext ctx_;
    // Pyramid work is pixels x taps; orient and descriptor count keypoints.
    spirula::SubmitBudget pyramidBudget_, orientBudget_, descBudget_;
    double pyramidWork_ = 0;
    int W0_ = 0, H0_ = 0, octaves_ = 0;
    uint32_t gaussFloats_ = 0, dogFloats_ = 0;
    std::vector<Level> gLevels_, dLevels_;
    std::vector<float> weights_;
    std::vector<uint32_t> stepOff_, stepRad_;

    GpuBuffer bImg_, bGauss_, bDog_, bTmp_, bWeights_, bGlev_, bDlev_;
    GpuBuffer bKp_, bKpCnt_, bOkp_, bOkpCnt_, bDesc_;
    GpuBuffer bHist_, bFokp_, bSelCnt_;

    bool setup_ = false, pipelinesLoaded_ = false;
    // Which pyramid layout the device-side weight/level tables currently hold.
    std::pair<int, int> planKey_{0, 0}, lastPlanKey_{-1, -1};
    std::pair<int, int> queued_{0, 0};  // size of the image whose pyramid is in flight
    bool plannedOnce_ = false;
    size_t capGauss_ = 0, capDog_ = 0, capImg_ = 0, capTmp_ = 0, capGlev_ = 0, capDlev_ = 0,
           capW_ = 0;
};

}  // namespace sfm
