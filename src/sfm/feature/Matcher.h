// Feature matching: a pluggable IFeatureMatcher interface and a GPU
// brute-force implementation (src/sfm/README.md).
//
// The interface is intentionally the whole contract a matcher must satisfy:
// given two FeatureSets, return the surviving putative correspondences
// (ratio-tested and, optionally, cross-checked). Nothing here assumes 128-D
// uint8 above the shader boundary, so a learned matcher (e.g. LightGlue,
// phase 6) can implement the same interface. Pair selection lives separately in
// sfm/feature/Pairing.h.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/vk/EmbeddedSpirv.h"
#include "sfm/vk/VkContext.h"

namespace sfm {

struct MatchOptions {
    float max_ratio = 0.8f;     // Lowe ratio (best_dist < ratio * second_dist)
    // Absolute cosine similarity a match must reach, for L2-normalized float
    // descriptors only; 0 disables it. COLMAP's ALIKED defaults are
    // min_cossim = 0.85 with max_ratio = 1.0 -- i.e. the ratio test off and
    // this the only filter -- which is a different shape of test from SIFT's
    // and needs both knobs to exist to be expressible.
    float min_similarity = 0.0f;
    bool cross_check = true;    // keep only mutual nearest neighbours
    uint32_t max_num_matches = 32768;  // cap per pair (0 = unlimited)
    int device = -1;
    // Pairs per GPU submission. One command buffer holds every dispatch in a
    // batch, so the fence round trip is paid once per batch instead of per
    // pair; the batch's results are downloaded in one copy (D22).
    int batch_pairs = 64;
    // VRAM for resident descriptors. 8192 features/image is ~1 MB, so the
    // default holds ~1500 images; past that the block is recycled and images
    // are re-uploaded per batch, which is still far better than per pair.
    size_t descriptor_budget_bytes = 1536ull << 20;
};

struct IFeatureMatcher {
    virtual ~IFeatureMatcher() = default;
    // Putative matches between a and b (indices into a/b keypoints).
    virtual std::vector<FeatureMatch> match(const FeatureSet& a, const FeatureSet& b) = 0;
    virtual const char* name() const = 0;

    // Match pairs[begin..end) in one go, appending one entry per pair to `out`
    // in pair order. GPU matchers override this to amortize uploads and
    // submissions over the batch; the default just loops.
    virtual void matchBatch(const std::vector<FeatureSet>& feats,
                            const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                            size_t end, std::vector<std::vector<FeatureMatch>>& out) {
        out.resize(end - begin);
        for (size_t k = begin; k < end; k++)
            out[k - begin] = match(feats[pairs[k].first], feats[pairs[k].second]);
    }
};

// GPU brute-force matcher.
//
// Descriptors for many images stay resident in one device buffer and a whole
// batch of pairs is dispatched from a single command buffer, so an exhaustive
// pass uploads each image roughly once rather than once per pair it appears in
// (at 100 images that is 100 MB instead of 9.9 GB). Distances use the hardware
// packed uint8x4 dot product; see the shader for why that is exact.
class BruteForceMatcher : public IFeatureMatcher {
public:
    explicit BruteForceMatcher(const MatchOptions& opt = {}) : opt_(opt) {
        // DP4A where the device has the instruction, the unpacked equivalent
        // where it lacks it (Intel Gen11/Gen12) or only emulates it (Apple).
        // Same integer result either way -- see bruteforce.slang.
        dot4_ = VkContext::probeCaps(opt.device).intDotProductFast;
        // SS_SFM_NO_DOT4=1 forces the fallback on a device that has DP4A,
        // which is how "same integer result" gets checked without the hardware
        // that lacks it.
        if (spirula::env_on("SFM_NO_DOT4")) dot4_ = false;
        VkContextOptions vo;
        vo.deviceIndex = opt.device;
        vo.needIntDotProduct = dot4_;
        ctx_.init(vo);
    }

    const char* name() const override { return "brute-force"; }

    // Convenience path (selftests, one-off pairs): a two-image session.
    std::vector<FeatureMatch> match(const FeatureSet& a, const FeatureSet& b) override {
        std::vector<FeatureSet> two;  // copies; this path is not the hot one
        two.push_back(a);
        two.push_back(b);
        std::vector<std::pair<uint32_t, uint32_t>> one{{0u, 1u}};
        std::vector<std::vector<FeatureMatch>> out;
        matchBatch(two, one, 0, 1, out);
        return out.empty() ? std::vector<FeatureMatch>() : std::move(out[0]);
    }

    void matchBatch(const std::vector<FeatureSet>& feats,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                    size_t end, std::vector<std::vector<FeatureMatch>>& out) override {
        matchBatch(asView(feats), pairs, begin, end, out);
    }

    // Pointer view of the feature sets, so a caller whose "images" are drawn
    // from more than one array (pair selection: query subsets plus the full
    // sets they were taken from) does not have to concatenate -- and copy --
    // them into one vector. On a 1200-image capture that copy was a gigabyte
    // of descriptors duplicated for the duration of the stage.
    void matchBatch(const std::vector<const FeatureSet*>& feats,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                    size_t end, std::vector<std::vector<FeatureMatch>>& out) {
        out.assign(end - begin, {});
        if (!prepare(feats, pairs, begin, end)) return;
        for (size_t b = begin; b < end;) {
            size_t e = chunkEnd(feats, pairs, b, end);
            matchChunk(feats, pairs, b, e, &out, nullptr, begin);
            b = e;
        }
    }

    // How many matches each pair *would* produce, without building the lists.
    // Pair selection scores half a million pairs and reads nothing but the
    // count; materializing a std::vector<FeatureMatch> per pair to then call
    // .size() on it was the host half of that stage's cost.
    void countBatch(const std::vector<const FeatureSet*>& feats,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                    size_t end, std::vector<uint32_t>& out) {
        out.assign(end - begin, 0u);
        if (!prepare(feats, pairs, begin, end)) return;
        for (size_t b = begin; b < end;) {
            size_t e = chunkEnd(feats, pairs, b, end);
            matchChunk(feats, pairs, b, e, nullptr, &out, begin);
            b = e;
        }
    }

private:
    // Wrap a contiguous array as the pointer view the core path takes. Rebuilt
    // per call: O(n) pointer writes against a batch of GPU dispatches.
    const std::vector<const FeatureSet*>& asView(const std::vector<FeatureSet>& feats) {
        view_.resize(feats.size());
        for (size_t i = 0; i < feats.size(); i++) view_[i] = &feats[i];
        return view_;
    }

    // Shared entry checks + one-time sizing. False means "nothing to do".
    bool prepare(const std::vector<const FeatureSet*>& feats,
                 const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                 size_t end) {
        if (begin >= end) return false;
        for (size_t k = begin; k < end; k++) {
            checkDescriptors(*feats[pairs[k].first]);
            checkDescriptors(*feats[pairs[k].second]);
        }
        ensureSetup(feats);
        return true;
    }

    // How far a chunk can run from `b`: one device round trip costs a submit,
    // a fence and a staging copy, so a chunk should be as large as the buffers
    // allow. `batch_pairs` cannot express that -- a scoring pair (512 queries,
    // no cross-check) produces ~32x less result data than a full match pair,
    // so any fixed pair count either leaves the buffers empty on the small
    // case or overruns them on the large one. Grow by *bytes* instead, bounded
    // by the result buffer, the staging buffer and the resident descriptor
    // block; always at least one pair, since all three are sized to hold the
    // largest single pair. This is what turns half a million scoring pairs
    // into dozens of submits instead of thousands.
    size_t chunkEnd(const std::vector<const FeatureSet*>& feats,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t b,
                    size_t end) {
        const uint64_t resCap =
            std::min<uint64_t>(resultCap_, VkContext::stagingCapacity() / 16);
        const bool cc = opt_.cross_check;
        if (stamp_.size() < feats.size()) stamp_.assign(feats.size(), 0);
        ++epoch_;
        uint64_t res = 0, desc = used_;
        size_t e = b;
        for (; e < end; e++) {
            const uint32_t ia = pairs[e].first, ib = pairs[e].second;
            const uint32_t na = feats[ia]->count(), nb = feats[ib]->count();
            const uint64_t addRes = (na && nb) ? (uint64_t)na + (cc ? nb : 0) : 0;
            uint64_t addDesc = 0;
            for (uint32_t img : {ia, ib})
                if (stamp_[img] != epoch_) {
                    stamp_[img] = epoch_;
                    if (!resident_.count(img)) addDesc += feats[img]->count();
                }
            if (e > b && (res + addRes > resCap || desc + addDesc > descCap_)) break;
            res += addRes;
            desc += addDesc;
        }
        return e;
    }

    // Exactly one of `out` / `counts` is non-null.
    void matchChunk(const std::vector<const FeatureSet*>& feats,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                    size_t end, std::vector<std::vector<FeatureMatch>>* out,
                    std::vector<uint32_t>* counts, size_t outBase) {
        ensureResident(feats, pairs, begin, end);

        // Lay out this batch's results: each dispatch writes nQuery uint4s, and
        // the whole region comes back in one download. The column side (rB) is
        // only ever read by the cross-check, so without it neither the
        // reduce_cols dispatch nor its result slots are paid for.
        const bool cc = opt_.cross_check;
        struct Slot { uint32_t a, b, na, nb; uint32_t oa, ob; };
        std::vector<Slot> slots;
        slots.reserve(end - begin);
        uint64_t outCount = 0;
        for (size_t k = begin; k < end; k++) {
            uint32_t ia = pairs[k].first, ib = pairs[k].second;
            uint32_t na = feats[ia]->count(), nb = feats[ib]->count();
            Slot s{ia, ib, na, nb, (uint32_t)outCount, (uint32_t)(outCount + na)};
            if (na == 0 || nb == 0) s.na = s.nb = 0;  // nothing to dispatch
            else outCount += (uint64_t)na + (cc ? nb : 0);
            slots.push_back(s);
        }
        if (outCount == 0) return;
        if (outCount > resultCap_)  // chunking bounds this; a bug if it ever trips
            throw std::runtime_error("match result buffer too small for a chunk");

        VkCommandBuffer cb = ctx_.begin();
        if (normDirty_.first != normDirty_.second) {
            Push p;
            p.u0 = normDirty_.first;
            p.u1 = normDirty_.second - normDirty_.first;
            ctx_.dispatch(cb, "descriptor_norms", (p.u1 + 63) / 64, p);
            ctx_.barrier(cb);
            normDirty_ = {0, 0};
        }
        // One matrix per pair, then a fold of its column candidates. The two
        // share colPartial, so they are separated by a barrier and consecutive
        // pairs are too; each dispatch already fills the GPU, and the fold is
        // tiny next to the matrix.
        for (const Slot& s : slots) {
            if (s.na == 0 || s.nb == 0) continue;
            // Workgroups for the cross-check path, which is 64 queries wide;
            // the row-only kernel is kRowThreads wide and gets its own count
            // below. u6 is read by reduce_cols, so it stays the 64-wide one.
            uint32_t numWG = (s.na + 63) / 64;
            Push p;
            p.u0 = resident_[s.a];
            p.u1 = s.na;
            p.u2 = resident_[s.b];
            p.u3 = s.nb;
            p.u4 = s.oa;
            p.u5 = s.ob;
            p.u6 = numWG;
            if (cc) {
                // colPartial is shared between pairs, so each pair's matrix and
                // column fold must retire before the next pair starts.
                ctx_.dispatch(cb, "match_pair", numWG, p);
                ctx_.barrier(cb);
                ctx_.dispatch(cb, "reduce_cols", (s.nb + 63) / 64, p);
                ctx_.barrier(cb);
            } else {
                // Row-only dispatches write disjoint result regions and read
                // nothing another pair writes: the whole batch runs with no
                // barriers, which is what keeps thousands of small scoring
                // dispatches from serializing on pipeline drains.
                ctx_.dispatch(cb, "match_rows", (s.na + kRowThreads - 1) / kRowThreads, p);
            }
        }
        // Readback in the same command buffer as the dispatches: one submit and
        // one fence per chunk instead of two. Results are read straight out of
        // the mapped staging buffer, so nothing is copied twice either. A chunk
        // too large for the staging buffer falls back to the chunked download.
        const VkDeviceSize bytes = (VkDeviceSize)outCount * 16;
        const bool fused = bytes <= VkContext::stagingCapacity();
        if (fused) {
            ctx_.barrier(cb);
            ctx_.recordDownload(cb, bResult_, bytes);
        }
        ctx_.submit(cb);

        const uint32_t* res;
        if (fused) {
            res = (const uint32_t*)ctx_.stagingDownloadPtr();
        } else {
            res_.resize((size_t)outCount * 4);
            ctx_.download(bResult_, res_.data(), bytes);
            res = res_.data();
        }

        for (size_t k = 0; k < slots.size(); k++) {
            const Slot& s = slots[k];
            if (s.na == 0 || s.nb == 0) continue;
            const uint32_t* rA = res + (size_t)s.oa * 4;
            const uint32_t* rB = res + (size_t)s.ob * 4;
            if (out) (*out)[begin - outBase + k] = reduce(rA, rB, s.na, s.nb);
            else (*counts)[begin - outBase + k] = countMatches(rA, rB, s.na, s.nb);
        }
    }

    // Float descriptors reach the GPU as uint8.
    //
    // The matching kernel is built around the hardware packed uint8x4 dot
    // product and a 128-byte descriptor; a float path would quadruple its
    // groupshared tile and retune it. It does not have to: for a UNIFORM
    // affine quantization the squared distances all scale by one constant, so
    // the ordering, the Lowe ratio (a ratio of two of them) and the
    // max_num_matches sort are preserved exactly. Only an absolute threshold
    // has to be converted, which is what similarityFromD2 does.
    //
    // kQuantHalfRange is the half-range mapped onto the byte. Measured on real
    // ALIKED descriptors: components are ~N(0, 0.088) with a maximum of 0.392
    // over an image, so 0.4 clips essentially nothing and spends the byte on
    // the range that is actually occupied. The residual is ~1% of a unit
    // descriptor's norm, well under the ratio test's margin.
    static constexpr float kQuantHalfRange = 0.4f;
    static constexpr float kQuantSteps = 127.0f;

    static uint8_t quantize(float v) {
        const float q = v * (kQuantSteps / kQuantHalfRange) + 128.0f;
        return (uint8_t)std::lround(std::min(255.0f, std::max(0.0f, q)));
    }
    // Cosine similarity of two unit descriptors from their quantized squared
    // distance: ||a-b||^2 = 2 - 2 cos, and d2 is that scaled by (steps/range)^2.
    static float similarityFromD2(uint32_t d2) {
        const float k = kQuantHalfRange / kQuantSteps;
        return 1.0f - 0.5f * (float)d2 * k * k;
    }

    // Bytes per descriptor: 128 for SIFT's and ALIKED's, 256 for DeDoDe-G's.
    // Read off the first FeatureSet with any features in it, and the shader is
    // built at both widths (cmake/SsSfm.cmake).
    int descBytes() const { return desc_words_ * 4; }
    // Workgroup width of the row-only kernel; MUST equal TQR in
    // sfm/shaders/match/bruteforce.slang. Each workgroup streams the whole
    // train set through groupshared once, so a pair's train traffic is
    // ceil(nQuery / this) * nTrain -- dispatching the 64-wide count here would
    // launch four times the workgroups, each redoing that streaming for
    // queries that are not even in range.
    static constexpr uint32_t kRowThreads = 256;

    // One byte per component either way: uint8 as SIFT writes them, or f32
    // normalized and quantized on upload (see kQuantHalfRange).
    void checkDescriptors(const FeatureSet& f) {
        if (f.count() == 0) return;
        if (desc_words_ == 0 && (f.dim == 128 || f.dim == 256))
            desc_words_ = (int)f.dim / 4;
        if (f.dim != 128 && f.dim != 256)
            throw std::runtime_error(
                "brute-force matcher expects 128-D or 256-D descriptors, got " +
                std::to_string(f.dim));
        if ((int)f.dim != desc_words_ * 4)
            throw std::runtime_error(
                "brute-force matcher was set up for " + std::to_string(desc_words_ * 4) +
                "-D descriptors and this image has " + std::to_string(f.dim) +
                "; one batch cannot mix widths");
        if (f.dtype != DType::U8 && f.dtype != DType::F32)
            throw std::runtime_error("brute-force matcher expects uint8 or f32 descriptors");
    }

    // Lowe ratio + cross-check, exactly as before -- the GPU only changed how
    // the distances are computed, not what counts as a match.
    std::vector<FeatureMatch> reduce(const uint32_t* rA, const uint32_t* rB, uint32_t na,
                                     uint32_t nb) const {
        std::vector<FeatureMatch> out;
        const float r2 = opt_.max_ratio * opt_.max_ratio;
        for (uint32_t i = 0; i < na; i++) {
            uint32_t j = rA[4 * i + 0];
            uint32_t bestD2 = rA[4 * i + 1], secondD2 = rA[4 * i + 2];
            if (j >= nb) continue;
            if (secondD2 != 0xffffffffu && (float)bestD2 >= r2 * (float)secondD2) continue;
            if (opt_.min_similarity > 0 && similarityFromD2(bestD2) < opt_.min_similarity)
                continue;
            if (opt_.cross_check && rB[4 * j + 0] != i) continue;
            out.push_back({i, j, std::sqrt((float)bestD2)});
        }
        if (opt_.max_num_matches > 0 && out.size() > opt_.max_num_matches) {
            std::partial_sort(out.begin(), out.begin() + opt_.max_num_matches, out.end(),
                              [](const FeatureMatch& x, const FeatureMatch& y) {
                                  return x.distance < y.distance;
                              });
            out.resize(opt_.max_num_matches);
        }
        return out;
    }

    // The same acceptance test as reduce(), counted rather than collected. Kept
    // adjacent so the two cannot drift apart.
    uint32_t countMatches(const uint32_t* rA, const uint32_t* rB, uint32_t na,
                          uint32_t nb) const {
        uint32_t n = 0;
        const float r2 = opt_.max_ratio * opt_.max_ratio;
        for (uint32_t i = 0; i < na; i++) {
            uint32_t j = rA[4 * i + 0];
            uint32_t bestD2 = rA[4 * i + 1], secondD2 = rA[4 * i + 2];
            if (j >= nb) continue;
            if (secondD2 != 0xffffffffu && (float)bestD2 >= r2 * (float)secondD2) continue;
            if (opt_.min_similarity > 0 && similarityFromD2(bestD2) < opt_.min_similarity)
                continue;
            if (opt_.cross_check && rB[4 * j + 0] != i) continue;
            n++;
        }
        if (opt_.max_num_matches > 0 && n > opt_.max_num_matches) n = opt_.max_num_matches;
        return n;
    }

    // All three buffers and the descriptor set must exist before the pipelines
    // load: createDescriptors is what builds the pipeline layout the compute
    // pipelines are created against. Everything is sized once, from the whole
    // feature set, so nothing is reallocated underneath a live pipeline.
    void ensureSetup(const std::vector<const FeatureSet*>& feats) {
        // Every image empty: nothing will be matched, but the buffers are still
        // sized and divided by, so pick the ordinary width.
        if (desc_words_ == 0) desc_words_ = 32;
        uint64_t total = 0, maxCount = 0;
        for (const FeatureSet* f : feats) {
            total += f->count();
            maxCount = std::max<uint64_t>(maxCount, f->count());
        }
        if (setup_) {
            // Buffers are allocated once and VkContext frees only at destruction
            // (no per-buffer regrow), so a second feature set that needs more
            // room cannot be served. Say so instead of overrunning: one
            // matcher per feature set.
            if (maxCount > setupMaxCount_)
                throw std::runtime_error(
                    "matcher was sized for a smaller feature set; use a fresh BruteForceMatcher");
            return;
        }
        setupMaxCount_ = maxCount;

        // A single batch can reference 2*batch_pairs distinct images, and all of
        // them must be resident at once, so that is the floor -- otherwise a
        // matcher first used on a two-image pair could never serve a real batch.
        const uint64_t batchFloor =
            (uint64_t)std::max(1, opt_.batch_pairs) * 2 * std::max<uint64_t>(maxCount, 1);
        const uint64_t budget =
            std::max<uint64_t>(opt_.descriptor_budget_bytes / descBytes(), batchFloor);
        descCap_ = (uint32_t)std::min<uint64_t>(std::max<uint64_t>(total, batchFloor), budget);
        // Worst case a batch can ask for: every pair contributing both images'
        // results, all at the largest feature count.
        resultCap_ = (uint32_t)std::max<uint64_t>(
            1, (uint64_t)std::max(1, opt_.batch_pairs) * 2 * std::max<uint64_t>(maxCount, 1));
        // One pair's column candidates: ceil(nA/64) workgroups x nB columns.
        // Reused pair to pair, so it is sized for the largest single pair.
        const uint64_t mc = std::max<uint64_t>(maxCount, 1);
        uint64_t colCap = ((mc + 63) / 64) * mc;

        bDesc_ = ctx_.createBuffer((VkDeviceSize)descCap_ * descBytes());
        bNorm_ = ctx_.createBuffer((VkDeviceSize)descCap_ * 4);
        bResult_ = ctx_.createBuffer((VkDeviceSize)resultCap_ * 16);
        bCol_ = ctx_.createBuffer((VkDeviceSize)colCap * 4);
        ctx_.createDescriptors({bDesc_.buf, bNorm_.buf, bResult_.buf, bCol_.buf});

        size_t words = 0;
        const std::string blob = std::string(dot4_ ? "match" : "match_nodot") +
                                 (desc_words_ == 64 ? "_d256" : "");
        const uint32_t* code = findSpirv(blob.c_str(), &words);
        if (!code)
            throw std::runtime_error(blob + " shader not built into this binary");
        ctx_.loadPipelines(code, words * 4,
                           {"match_pair", "match_rows", "reduce_cols", "descriptor_norms"});
        setup_ = true;
    }

    // Make every image this batch touches resident, uploading the newly added
    // ones as one contiguous block. The allocator bumps; when a batch does not
    // fit it recycles the whole block (exhaustive order means the working set
    // is the whole dataset, so partial eviction would buy nothing).
    void ensureResident(const std::vector<const FeatureSet*>& feats,
                        const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                        size_t end) {
        const uint32_t capDesc = descCap_;
        std::vector<uint32_t> need;
        for (size_t k = begin; k < end; k++)
            for (uint32_t img : {pairs[k].first, pairs[k].second})
                if (!resident_.count(img)) need.push_back(img);
        std::sort(need.begin(), need.end());
        need.erase(std::unique(need.begin(), need.end()), need.end());
        if (need.empty()) return;

        uint32_t want = 0;
        for (uint32_t img : need) want += feats[img]->count();
        if (used_ + want > capDesc) {  // recycle
            resident_.clear();
            used_ = 0;
            normDirty_ = {0, 0};
            need.clear();
            for (size_t k = begin; k < end; k++)
                for (uint32_t img : {pairs[k].first, pairs[k].second}) need.push_back(img);
            std::sort(need.begin(), need.end());
            need.erase(std::unique(need.begin(), need.end()), need.end());
            want = 0;
            for (uint32_t img : need) want += feats[img]->count();
            if (want > capDesc)
                throw std::runtime_error(  // batchFloor in ensureSetup rules this out
                    "descriptor budget too small for one batch of pairs");
        }

        // Gather into one staging-friendly blob: the new images occupy a
        // contiguous descriptor range, so this is a single upload and a single
        // norm dispatch instead of one of each per image.
        const uint32_t first = used_;
        const size_t dsz = (size_t)descBytes();
        blob_.resize((size_t)want * dsz);
        uint8_t* blob = blob_.data();
        size_t off = 0;
        for (uint32_t img : need) {
            const FeatureSet& f = *feats[img];
            resident_[img] = used_;
            used_ += f.count();
            if (!f.count()) continue;
            const size_t bytes = (size_t)f.count() * dsz;
            if (f.dtype == DType::U8) {
                memcpy(blob + off, f.descriptors.data(), bytes);
            } else {
                // Float descriptors are L2-normalized on the way in. ALIKED's
                // already are, so this is a no-op there; DeDoDe's are NOT, and
                // both the quantization range and similarityFromD2 assume it.
                const float* src = reinterpret_cast<const float*>(f.descriptors.data());
                for (uint32_t r = 0; r < f.count(); r++) {
                    const float* row = src + (size_t)r * dsz;
                    double sq = 0;
                    for (size_t i = 0; i < dsz; i++) sq += (double)row[i] * row[i];
                    const float inv = sq > 0 ? (float)(1.0 / std::sqrt(sq)) : 1.0f;
                    uint8_t* dst = blob + off + (size_t)r * dsz;
                    for (size_t i = 0; i < dsz; i++) dst[i] = quantize(row[i] * inv);
                }
            }
            off += bytes;
        }
        ctx_.upload(bDesc_, blob, (VkDeviceSize)want * dsz, (VkDeviceSize)first * dsz);
        normDirty_ = {first, used_};
    }

    MatchOptions opt_;
    bool dot4_ = true;  // device has VK_KHR_shader_integer_dot_product
    VkContext ctx_;
    GpuBuffer bDesc_, bNorm_, bResult_, bCol_;
    bool setup_ = false;
    std::vector<const FeatureSet*> view_;      // scratch for the vector<FeatureSet> overload
    std::vector<uint32_t> stamp_;             // chunkEnd's "seen in this chunk" marks
    uint32_t epoch_ = 0;
    std::vector<uint32_t> res_;               // download scratch, reused across chunks
    std::vector<uint8_t> blob_;               // upload scratch, reused across chunks
    std::map<uint32_t, uint32_t> resident_;   // image index -> first descriptor index
    std::pair<uint32_t, uint32_t> normDirty_{0, 0};  // descriptor range awaiting ||d||^2
    int desc_words_ = 0;                      // 32 or 64, from the first FeatureSet
    uint32_t used_ = 0, descCap_ = 0, resultCap_ = 0;
    uint64_t setupMaxCount_ = 0;  // largest feature count the buffers were sized for
};

}  // namespace sfm
