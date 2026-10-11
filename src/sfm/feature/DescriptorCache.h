#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <list>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"

namespace sfm {

class DescriptorCache {
public:
    DescriptorCache(std::filesystem::path dir, const std::vector<std::string>& names,
                    std::vector<FeatureSet>& feats, size_t budget, size_t max_images)
        : dir_(std::move(dir)), names_(names), feats_(feats), budget_(budget),
          max_images_(max_images) {
        if (names.size() != feats.size() || !budget || max_images < 2)
            throw std::runtime_error("invalid descriptor cache options");
        for (const auto& f : feats)
            if (!f.descriptors.empty())
                throw std::runtime_error("descriptor cache requires metadata-only features");
    }
    DescriptorCache(const DescriptorCache&) = delete;
    DescriptorCache& operator=(const DescriptorCache&) = delete;
    ~DescriptorCache() {
        for (uint32_t i : lru_) std::vector<uint8_t>().swap(feats_[i].descriptors);
    }

    size_t bytes() const { return bytes_; }
    size_t peakBytes() const { return peak_; }
    size_t loads() const { return loads_; }

    template <class Match>
    void match(const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
               size_t end, std::vector<std::vector<FeatureMatch>>& out, Match run) {
        out.clear();
        for (size_t b = begin; b < end;) {
            std::set<uint32_t> pinned;
            size_t needed = 0, e = b;
            for (; e < end; ++e) {
                const auto& p = pairs.at(e);
                std::set<uint32_t> added;
                for (uint32_t i : {p.first, p.second})
                    if (!pinned.count(i)) added.insert(i);
                size_t extra = 0;
                for (uint32_t i : added) extra += descriptorBytes(i);
                if (needed + extra > budget_ || pinned.size() + added.size() > max_images_) {
                    if (e == b)
                        throw std::runtime_error("a matching pair exceeds --block-cache-mb; increase it");
                    break;
                }
                needed += extra;
                pinned.insert(added.begin(), added.end());
            }
            pin(pinned);
            std::vector<std::vector<FeatureMatch>> chunk;
            run(b, e, chunk);
            if (chunk.size() != e - b) throw std::runtime_error("matcher returned a short batch");
            for (auto& m : chunk) out.push_back(std::move(m));
            b = e;
        }
    }

private:
    std::filesystem::path dir_;
    const std::vector<std::string>& names_;
    std::vector<FeatureSet>& feats_;
    size_t budget_, max_images_, bytes_ = 0, peak_ = 0, loads_ = 0;
    std::list<uint32_t> lru_;
    std::unordered_map<uint32_t, std::list<uint32_t>::iterator> resident_;

    size_t descriptorBytes(uint32_t i) const {
        const FeatureSet& f = feats_.at(i);
        return (size_t)f.count() * f.dim * dtypeSize(f.dtype);
    }

    void pin(const std::set<uint32_t>& pinned) {
        size_t extra = 0, images = 0;
        for (uint32_t i : pinned)
            if (!resident_.count(i)) { extra += descriptorBytes(i); ++images; }
        for (auto it = lru_.begin();
             bytes_ + extra > budget_ || resident_.size() + images > max_images_;) {
            if (it == lru_.end()) throw std::runtime_error("descriptor cache cannot evict pinned images");
            const uint32_t i = *it;
            if (pinned.count(i)) { ++it; continue; }
            bytes_ -= feats_[i].descriptors.size();
            std::vector<uint8_t>().swap(feats_[i].descriptors);
            resident_.erase(i);
            it = lru_.erase(it);
        }
        for (uint32_t i : pinned) {
            const auto old = resident_.find(i);
            if (old != resident_.end()) {
                lru_.splice(lru_.end(), lru_, old->second);
                continue;
            }
            read(i);
            lru_.push_back(i);
            resident_[i] = std::prev(lru_.end());
            bytes_ += feats_[i].descriptors.size();
            peak_ = std::max(peak_, bytes_);
            ++loads_;
        }
    }

    void read(uint32_t i) {
        const std::filesystem::path path = dir_ / (names_[i] + ".bin");
        std::ifstream in(path, std::ios::binary);
        char magic[4]{};
        uint32_t header[6]{};
        in.read(magic, 4);
        in.read((char*)header, sizeof header);
        const FeatureSet& f = feats_[i];
        if (!in || std::memcmp(magic, "VKFT", 4) || header[3] != f.count() ||
            header[4] != f.dim || header[5] != (uint32_t)f.dtype ||
            header[1] != (uint32_t)f.width || header[2] != (uint32_t)f.height)
            throw std::runtime_error("feature header changed or is invalid: " + path.string());
        in.seekg(28 + (std::streamoff)f.count() * 16);
        auto& descriptors = feats_[i].descriptors;
        descriptors.resize(descriptorBytes(i));
        in.read((char*)descriptors.data(), (std::streamsize)descriptors.size());
        if (!in) {
            std::vector<uint8_t>().swap(descriptors);
            throw std::runtime_error("truncated descriptors: " + path.string());
        }
    }
};

}  // namespace sfm
