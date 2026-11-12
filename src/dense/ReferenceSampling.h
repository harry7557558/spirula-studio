#pragma once

#include "dense/DiskArray.h"
#include "dense/ExternalSort.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <vector>

namespace spirula::dense {

inline std::pair<int, int> sampling_layout(int width, int height, uint64_t count) {
    const uint64_t coverage = (uint64_t)std::ceil(count * 0.15);
    const int columns = std::max(1, (int)std::ceil(std::sqrt((double)coverage * width / height)));
    return {columns, std::max(1, (int)((coverage + columns - 1) / columns))};
}

inline uint64_t sampling_hash(uint64_t value) {
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

inline double sampling_weight(uint64_t seed, uint32_t pixel, float confidence) {
    const double uniform = ((sampling_hash(seed ^ pixel) >> 11) + 0.5) * (1.0 / 9007199254740992.0);
    return std::log(uniform) / confidence;
}

class ReferenceSampling {
public:
    ReferenceSampling(int width, int height, uint64_t count, uint64_t seed)
        : width_(width), height_(height), count_(count), seed_(seed) {
        const auto layout = sampling_layout(width,height,count); columns_ = layout.first; rows_ = layout.second;
    }

    void add(uint32_t pixel, float confidence) {
        if (!(confidence > 0) || !std::isfinite(confidence) || !count_) return;
        const Item item{sampling_weight(seed_,pixel,confidence), pixel};
        if (weighted_.size() < count_) weighted_.push(item);
        else if (item > weighted_.top()) { weighted_.pop(); weighted_.push(item); }
        const uint64_t cell = (uint64_t)(pixel / width_) * rows_ / height_ * columns_ +
                              (uint64_t)(pixel % width_) * columns_ / width_;
        const Item candidate{confidence, pixel};
        auto found = coverage_.find(cell);
        if (found == coverage_.end() || candidate > found->second) coverage_[cell] = candidate;
    }

    std::set<uint32_t> selected() {
        std::vector<Item> coverage, weighted;
        for (const auto& [cell, item] : coverage_) coverage.push_back(item);
        std::sort(coverage.begin(), coverage.end(), std::greater<Item>());
        std::set<uint32_t> out;
        for (size_t i = 0; i < std::min<uint64_t>(coverage.size(), (uint64_t)std::ceil(count_ * 0.15)); ++i)
            out.insert(coverage[i].second);
        while (!weighted_.empty()) { weighted.push_back(weighted_.top()); weighted_.pop(); }
        for (auto it = weighted.rbegin(); it != weighted.rend() && out.size() < count_; ++it) out.insert(it->second);
        return out;
    }

private:
    using Item = std::pair<double, uint32_t>;
    int width_, height_, columns_ = 1, rows_ = 1;
    uint64_t count_, seed_;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> weighted_;
    std::map<uint64_t, Item> coverage_;
};

class ExternalReferenceSampling {
public:
    ExternalReferenceSampling(const std::filesystem::path& prefix, int width, int height,
                               uint64_t count, uint64_t seed, uint64_t budget)
        : prefix_(prefix), width_(width), height_(height), count_(count), seed_(seed), budget_(budget) {
        const auto layout = sampling_layout(width,height,count); columns_ = layout.first; rows_ = layout.second;
        output_.open(path("input"),std::ios::binary | std::ios::trunc, disk_io_buffer_bytes(budget_));
        if (!output_) throw std::runtime_error("cannot write dense sample weights");
    }

    void add(uint32_t pixel, float confidence) {
        if (!(confidence > 0) || !std::isfinite(confidence) || !count_) return;
        const uint64_t cell = (uint64_t)(pixel / width_) * rows_ / height_ * columns_ +
                              (uint64_t)(pixel % width_) * columns_ / width_;
        write_disk_record(output_,Item{cell,sampling_weight(seed_,pixel,confidence),confidence,pixel});
    }

    std::filesystem::path selected(const std::function<void()>& check = {}) {
        output_.flush(); if (!output_) throw std::runtime_error("cannot flush dense sample weights"); output_.close();
        external_sort<Item>(path("input"),path("cells"),budget_,[](const Item& a,const Item& b) {
            if (a.cell != b.cell) return a.cell < b.cell;
            return std::tie(a.confidence,a.pixel) > std::tie(b.confidence,b.pixel);
        },check);
        {
            BufferedInputFile input(path("cells"),std::ios::binary, disk_io_buffer_bytes(budget_));
            BufferedOutputFile output(path("coverage"),std::ios::binary | std::ios::trunc, disk_io_buffer_bytes(budget_));
            Item item; uint64_t cell = UINT64_MAX;
            while (read_disk_record(input,item)) if (cell != item.cell) { write_disk_record(output,item); cell = item.cell; }
            flush_disk_output(output);
        }
        external_sort<Item>(path("coverage"),path("ranked-coverage"),budget_,[](const Item& a,const Item& b) {
            return std::tie(a.confidence,a.pixel) > std::tie(b.confidence,b.pixel);
        },check);
        {
            BufferedInputFile input(path("ranked-coverage"),std::ios::binary, disk_io_buffer_bytes(budget_));
            BufferedOutputFile output(path("cover-pixels"),std::ios::binary | std::ios::trunc, disk_io_buffer_bytes(budget_));
            Item item; uint64_t count = 0, coverage = (uint64_t)std::ceil(count_ * 0.15);
            while (count < coverage && read_disk_record(input,item)) { write_disk_record(output,item.pixel); ++count; }
            flush_disk_output(output);
        }
        external_sort<uint32_t>(path("cover-pixels"),path("cover-sorted"),budget_,std::less<uint32_t>(),check);
        DiskArray<uint32_t> coverage(path("cover-spill"),budget_ / 4);
        {
            BufferedInputFile input(path("cover-sorted"),std::ios::binary, disk_io_buffer_bytes(budget_)); uint32_t pixel;
            while (read_disk_record(input,pixel)) coverage.push_back(pixel);
        }
        auto covered = [&](uint32_t pixel) {
            uint64_t lo = 0, hi = coverage.size();
            while (lo < hi) { const auto mid = lo + (hi - lo) / 2; if (coverage[mid] < pixel) lo = mid + 1; else hi = mid; }
            return lo < coverage.size() && coverage[lo] == pixel;
        };
        external_sort<Item>(path("input"),path("weights"),budget_,[](const Item& a,const Item& b) {
            return std::tie(a.weight,a.pixel) > std::tie(b.weight,b.pixel);
        },check);
        {
            BufferedInputFile input(path("weights"),std::ios::binary, disk_io_buffer_bytes(budget_));
            BufferedOutputFile output(path("chosen"),std::ios::binary | std::ios::trunc, disk_io_buffer_bytes(budget_));
            for (auto pixel : coverage) write_disk_record(output,pixel);
            Item item; uint64_t count = coverage.size();
            while (count < count_ && read_disk_record(input,item)) {
                if (check) check();
                if (!covered(item.pixel)) { write_disk_record(output,item.pixel); ++count; }
            }
            flush_disk_output(output);
        }
        external_sort<uint32_t>(path("chosen"),path("selected"),budget_,std::less<uint32_t>(),check);
        return path("selected");
    }

private:
    struct Item { uint64_t cell; double weight; float confidence; uint32_t pixel; };
    std::filesystem::path path(const char* suffix) const { return prefix_.string() + "-" + suffix + ".bin"; }
    std::filesystem::path prefix_;
    int width_, height_, columns_, rows_;
    uint64_t count_, seed_, budget_;
    BufferedOutputFile output_;
};

}  // namespace spirula::dense
