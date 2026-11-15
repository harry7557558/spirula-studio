// Correspondence graph: for every (image, feature), the set of features in
// other images it is matched to (COLMAP's scene/correspondence_graph).
// Built from the verified two-view matches; the incremental mapper
// queries it to find 2D-3D correspondences (register-next) and to grow tracks
// (triangulation).
//
// Stored per image as CSR -- one offsets array over the image's features plus
// one flat correspondence array -- rather than a vector per feature. An entry
// is one word, (image << feature_bits) | feature, whenever both fit in 32 bits
// (16k images of 256k features do), else two: 3.6 GB -> 1.8 GB on a
// 4000-image video's 225M matches.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sfm/core/Matches.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Spill.h"

namespace sfm {

struct Correspondence {
    uint32_t image_id;
    uint32_t feature_idx;
};

// Contiguous run of correspondences for one (image, feature), decoded as read.
class CorrespondenceView {
public:
    class iterator {
    public:
        iterator(const uint32_t* p, uint32_t stride, uint32_t shift)
            : p_(p), stride_(stride), shift_(shift) {}
        Correspondence operator*() const {
            if (stride_ == 2) return {p_[0], p_[1]};
            return {p_[0] >> shift_, p_[0] & ((1u << shift_) - 1u)};
        }
        iterator& operator++() {
            p_ += stride_;
            return *this;
        }
        bool operator!=(const iterator& o) const { return p_ != o.p_; }
        bool operator==(const iterator& o) const { return p_ == o.p_; }

    private:
        const uint32_t* p_;
        uint32_t stride_, shift_;
    };

    CorrespondenceView() = default;
    CorrespondenceView(const uint32_t* first, const uint32_t* last, uint32_t stride,
                       uint32_t shift)
        : first_(first), last_(last), stride_(stride), shift_(shift) {}
    iterator begin() const { return {first_, stride_, shift_}; }
    iterator end() const { return {last_, stride_, shift_}; }
    size_t size() const { return (size_t)(last_ - first_) / stride_; }
    bool empty() const { return first_ == last_; }

private:
    const uint32_t* first_ = nullptr;
    const uint32_t* last_ = nullptr;
    uint32_t stride_ = 1, shift_ = 0;
};

class CorrespondenceGraph {
public:
    // `num_features[i]` = feature count of image i (matching db.images order).
    // `wide` takes two words per entry even where one would do (the tests).
    void build(const MatchesDatabase& db, const std::vector<uint32_t>& num_features,
               bool wide = false) {
        const size_t n = num_features.size();
        uint32_t max_features = 1;
        for (uint32_t c : num_features) max_features = std::max(max_features, c);
        auto bits = [](uint64_t v) {
            uint32_t b = 1;
            while (b < 32 && (v >> b) != 0) b++;
            return b;
        };
        shift_ = bits(max_features - 1);
        stride_ = !wide && shift_ + bits(n > 0 ? n - 1 : 0) <= 32 ? 1 : 2;
        spill_.reset();
        starts_.assign(n, {});
        data_.assign(n, {});
        for (size_t i = 0; i < n; i++) starts_[i].assign((size_t)num_features[i] + 1, 0);

        // Count, prefix-sum, scatter. The +1 offset in the counting pass makes
        // the prefix sum land directly in the final offsets array.
        for (const TwoViewMatches& p : db.pairs) {
            cancel::check();
            for (const FeatureMatch& m : p.matches) {
                if (m.idx1 + 1 >= starts_[p.image1].size() ||
                    m.idx2 + 1 >= starts_[p.image2].size())
                    continue;
                starts_[p.image1][m.idx1 + 1]++;
                starts_[p.image2][m.idx2 + 1]++;
            }
        }
        std::vector<std::vector<uint32_t>> fill(n);
        for (size_t i = 0; i < n; i++) {
            cancel::check();
            std::vector<uint32_t>& s = starts_[i];
            for (size_t f = 1; f < s.size(); f++) s[f] += s[f - 1];
            data_[i].resize(s.empty() ? 0 : (size_t)s.back() * stride_);
            fill[i].assign(s.begin(), s.end() - (s.empty() ? 0 : 1));
        }
        auto put = [&](uint32_t img, uint32_t feat, uint32_t other, uint32_t other_feat) {
            uint32_t* e = &data_[img][(size_t)fill[img][feat]++ * stride_];
            if (stride_ == 2) {
                e[0] = other;
                e[1] = other_feat;
            } else {
                e[0] = (other << shift_) | other_feat;
            }
        };
        for (const TwoViewMatches& p : db.pairs) {
            cancel::check();
            for (const FeatureMatch& m : p.matches) {
                if (m.idx1 + 1 >= starts_[p.image1].size() ||
                    m.idx2 + 1 >= starts_[p.image2].size())
                    continue;
                put(p.image1, m.idx1, p.image2, m.idx2);
                put(p.image2, m.idx2, p.image1, m.idx1);
            }
        }
        rows_.resize(n);
        for (size_t i = 0; i < n; i++)
            rows_[i] = {starts_[i].data(), data_[i].data(), num_features[i]};
    }

    // Both arrays into a SpillFile at `path`, read from the mapping from here
    // on: the graph is never written after build(). False keeps them in memory.
    bool spill(const std::string& path) {
        std::vector<std::pair<const void*, size_t>> chunks;
        for (size_t i = 0; i < rows_.size(); i++) {
            chunks.push_back({starts_[i].data(), starts_[i].size() * 4});
            if (!data_[i].empty()) chunks.push_back({data_[i].data(), data_[i].size() * 4});
        }
        std::shared_ptr<const SpillFile> f = SpillFile::write(path, chunks);
        if (!f) return false;
        const uint32_t* at = reinterpret_cast<const uint32_t*>(f->data());
        for (size_t i = 0; i < rows_.size(); i++) {
            rows_[i].starts = at;
            at += starts_[i].size();
            rows_[i].data = at;
            at += data_[i].size();
        }
        std::vector<std::vector<uint32_t>>().swap(starts_);
        std::vector<std::vector<uint32_t>>().swap(data_);
        spill_ = std::move(f);
        releaseFreedHeap();
        return true;
    }

    CorrespondenceView at(uint32_t image, uint32_t feature) const {
        const Row& r = rows_[image];
        return {r.data + (size_t)r.starts[feature] * stride_,
                r.data + (size_t)r.starts[feature + 1] * stride_, stride_, shift_};
    }

    bool isolatedPair(uint32_t image, uint32_t feature) const {
        const auto matches = at(image, feature);
        if (matches.size() != 1) return false;
        const auto other = *matches.begin();
        return at(other.image_id, other.feature_idx).size() == 1;
    }

    size_t numImages() const { return rows_.size(); }
    uint32_t numFeatures(uint32_t image) const { return rows_[image].features; }

private:
    // Per image: offsets over the image's features (features+1 entries) and the
    // correspondences they index into, `stride_` words each. `rows_` points at
    // the vectors below, or into spill_ once they are gone.
    struct Row {
        const uint32_t* starts = nullptr;
        const uint32_t* data = nullptr;
        uint32_t features = 0;
    };
    std::vector<Row> rows_;
    std::vector<std::vector<uint32_t>> starts_;
    std::vector<std::vector<uint32_t>> data_;
    std::shared_ptr<const SpillFile> spill_;
    uint32_t stride_ = 1, shift_ = 1;
};

}  // namespace sfm
